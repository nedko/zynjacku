/* -*- Mode: C ; c-basic-offset: 2 -*- */
/*****************************************************************************
 *
 *   This file is part of zynjacku
 *
 *   Copyright (C) 2006,2007,2008 Nedko Arnaudov <nedko@arnaudov.name>
 *   Copyright (C) 2006 Dave Robillard <dave@drobilla.net>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; version 2 of the License
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 *****************************************************************************/

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <lv2.h>
#include <jack/jack.h>
#include <jack/midiport.h>
#include <glib-object.h>
#include <lv2dynparam/lv2dynparam.h>
#include <lv2dynparam/lv2_rtmempool.h>
#include <lv2dynparam/host.h>

#include "config.h"

#include "lv2_contexts.h"
#include "lv2-miditype.h"
#include "lv2_event.h"
#include "lv2_uri_map.h"
#include "lv2_string_port.h"

#include "list.h"
#define LOG_LEVEL LOG_LEVEL_ERROR
#include "log.h"

#include "plugin.h"
#include "engine.h"
#include "lv2.h"
#include "gtk2gui.h"

#include "zynjacku.h"

#include "jack_compat.c"

#include "rtmempool.h"
#include "plugin_repo.h"
#include "lv2_event_helpers.h"
#include "midi_cc_map.h"

#define ZYNJACKU_ENGINE_SIGNAL_TICK    0 /* plugin iterated */
#define ZYNJACKU_ENGINE_SIGNAL_TACK    1 /* "good" plugin found */
#define ZYNJACKU_ENGINE_SIGNALS_COUNT  2

/* URI map value for event MIDI type */
#define ZYNJACKU_MIDI_EVENT_ID 1

#define ZYNJACKU_ENGINE_FEATURES 7

struct zynjacku_engine
{
  gboolean dispose_has_run;

  jack_client_t * jack_client;  /* the jack client */

  struct list_head plugins_all; /* accessed only from ui thread */
  struct list_head plugins_active; /* accessed only from rt thread */

  pthread_mutex_t active_plugins_lock;
  struct list_head plugins_pending_activation; /* protected using active_plugins_lock */

  struct list_head midi_ports;  /* PORT_TYPE_MIDI "struct zynjacku_port"s linked by port_type_siblings */
  struct list_head audio_ports; /* PORT_TYPE_AUDIO "struct zynjacku_port"s linked by port_type_siblings */
  jack_port_t * jack_midi_in;
  LV2_MIDI lv2_midi_buffer;
  LV2_Event_Buffer lv2_midi_event_buffer;
  gboolean midi_activity;

  struct lv2_rtsafe_memory_pool_provider mempool_allocator;
  LV2_URI_Map_Feature uri_map;
  LV2_Event_Feature event;

  LV2_Feature host_feature_rtmempool;
  LV2_Feature host_feature_uri_map;
  LV2_Feature host_feature_event_ref;
  LV2_Feature host_feature_dynparams;
  LV2_Feature host_feature_contexts;
  LV2_Feature host_feature_msgcontext;
  LV2_Feature host_feature_stringport;
  const LV2_Feature * host_features[ZYNJACKU_ENGINE_FEATURES + 1];

  pthread_mutex_t cc_lock;
  struct
  {
    signed char value_rt;      /* accessed by rt-thread only */
    signed char value;         /* protected by cc_lock */
    signed char value_changed; /* accessed by ui-thread only */
    signed char value_ui;      /* accessed by ui-thread only */
  } cc[127];
};

#define ZYNJACKU_ENGINE_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), ZYNJACKU_ENGINE_TYPE, struct zynjacku_engine))

static guint g_zynjacku_engine_signals[ZYNJACKU_ENGINE_SIGNALS_COUNT];

static
int
jack_process_cb(
  jack_nframes_t nframes,
  void* data);

static void
zynjacku_engine_dispose(GObject * obj)
{
  struct zynjacku_engine * engine_ptr;

  engine_ptr = ZYNJACKU_ENGINE_GET_PRIVATE(obj);

  LOG_DEBUG("zynjacku_engine_dispose() called.");

  if (engine_ptr->dispose_has_run)
  {
    /* If dispose did already run, return. */
    LOG_DEBUG("zynjacku_engine_dispose() already run!");
    return;
  }

  /* Make sure dispose does not run twice. */
  engine_ptr->dispose_has_run = TRUE;

  /* 
   * In dispose, you are supposed to free all types referenced from this
   * object which might themselves hold a reference to self. Generally,
   * the most simple solution is to unref all members on which you own a 
   * reference.
   */
  if (engine_ptr->jack_client)
  {
    zynjacku_engine_stop_jack(ZYNJACKU_ENGINE(obj));
    zynjacku_plugin_repo_uninit();
  }

  pthread_mutex_destroy(&engine_ptr->active_plugins_lock);
  pthread_mutex_destroy(&engine_ptr->cc_lock);

  /* Chain up to the parent class */
  G_OBJECT_CLASS(g_type_class_peek_parent(G_OBJECT_GET_CLASS(obj)))->dispose(obj);
}

static void
zynjacku_engine_finalize(GObject * obj)
{
//  struct zynjacku_engine * self = ZYNJACKU_ENGINE_GET_PRIVATE(obj);

  LOG_DEBUG("zynjacku_engine_finalize() called.");

  /*
   * Here, complete object destruction.
   * You might not need to do much...
   */
  //g_free(self->private);

  /* Chain up to the parent class */
  G_OBJECT_CLASS(g_type_class_peek_parent(G_OBJECT_GET_CLASS(obj)))->finalize(obj);
}

static void
zynjacku_engine_class_init(
  gpointer class_ptr,
  gpointer class_data_ptr)
{
  LOG_DEBUG("zynjacku_engine_class() called.");

  G_OBJECT_CLASS(class_ptr)->dispose = zynjacku_engine_dispose;
  G_OBJECT_CLASS(class_ptr)->finalize = zynjacku_engine_finalize;

  g_type_class_add_private(G_OBJECT_CLASS(class_ptr), sizeof(struct zynjacku_engine));

  g_zynjacku_engine_signals[ZYNJACKU_ENGINE_SIGNAL_TICK] =
    g_signal_new(
      "tick",                   /* signal_name */
      ZYNJACKU_ENGINE_TYPE,     /* itype */
      G_SIGNAL_RUN_LAST |
      G_SIGNAL_ACTION,          /* signal_flags */
      0,                        /* class_offset */
      NULL,                     /* accumulator */
      NULL,                     /* accu_data */
      NULL,                     /* c_marshaller */
      G_TYPE_NONE,              /* return type */
      2,                        /* n_params */
      G_TYPE_FLOAT,             /* progress 0 .. 1 */
      G_TYPE_STRING);           /* uri of plugin being scanned */

  g_zynjacku_engine_signals[ZYNJACKU_ENGINE_SIGNAL_TACK] =
    g_signal_new(
      "tack",                   /* signal_name */
      ZYNJACKU_ENGINE_TYPE,     /* itype */
      G_SIGNAL_RUN_LAST |
      G_SIGNAL_ACTION,          /* signal_flags */
      0,                        /* class_offset */
      NULL,                     /* accumulator */
      NULL,                     /* accu_data */
      NULL,                     /* c_marshaller */
      G_TYPE_NONE,              /* return type */
      4,                        /* n_params */
      G_TYPE_STRING,            /* plugin name */
      G_TYPE_STRING,            /* plugin uri */
      G_TYPE_STRING,            /* plugin license */
      G_TYPE_STRING);           /* plugin author */
}

static
uint32_t
zynjacku_uri_to_id(
  LV2_URI_Map_Callback_Data callback_data,
  const char * map,
  const char * uri)
{
  if (strcmp(map, LV2_EVENT_URI) == 0 &&
      strcmp(uri, LV2_EVENT_URI_TYPE_MIDI) == 0)
  {
    return ZYNJACKU_MIDI_EVENT_ID;
  }
  else if (strcmp(map, "http://lv2plug.in/ns/extensions/ui") == 0 &&
      strcmp(uri, LV2_STRING_PORT_URI) == 0)
  {
    return ZYNJACKU_STRING_XFER_ID;
  }

  return 0;                     /* "unsupported" */
}

static
uint32_t
zynjacku_event_ref_func(
  LV2_Event_Callback_Data callback_data,
  LV2_Event * event)
{
	return 0;
}

static void
zynjacku_engine_init(
  GTypeInstance * instance,
  gpointer g_class)
{
  struct zynjacku_engine * engine_ptr;
  int count;
  int i;

  LOG_DEBUG("zynjacku_engine_init() called.");

  engine_ptr = ZYNJACKU_ENGINE_GET_PRIVATE(instance);

  engine_ptr->dispose_has_run = FALSE;

  engine_ptr->jack_client = NULL;

  pthread_mutex_init(&engine_ptr->active_plugins_lock, NULL);
  pthread_mutex_init(&engine_ptr->cc_lock, NULL);

  for (i = 0 ; i < sizeof(engine_ptr->cc) / sizeof(engine_ptr->cc[0]) ; i++)
  {
    engine_ptr->cc[i].value_rt = 255;
    engine_ptr->cc[i].value = 255;
    engine_ptr->cc[i].value_changed = false;
    engine_ptr->cc[i].value_ui = 255;
  }

  /* initialize rtsafe mempool host feature */
  rtmempool_allocator_init(&engine_ptr->mempool_allocator);

  engine_ptr->host_feature_rtmempool.URI = LV2_RTSAFE_MEMORY_POOL_URI;
  engine_ptr->host_feature_rtmempool.data = &engine_ptr->mempool_allocator;

  /* initialize uri map host feature */
  engine_ptr->uri_map.callback_data = engine_ptr;
  engine_ptr->uri_map.uri_to_id = zynjacku_uri_to_id;

  engine_ptr->host_feature_uri_map.URI = LV2_URI_MAP_URI;
  engine_ptr->host_feature_uri_map.data = &engine_ptr->uri_map;

  /* initialize event host feature */
  /* We don't support type 0 events, so the ref and unref functions just point to the same empty function. */
  engine_ptr->event.callback_data = engine_ptr;
  engine_ptr->event.lv2_event_ref = zynjacku_event_ref_func;
  engine_ptr->event.lv2_event_unref = zynjacku_event_ref_func;

  engine_ptr->host_feature_event_ref.URI = LV2_EVENT_URI;
  engine_ptr->host_feature_event_ref.data = &engine_ptr->event;

  engine_ptr->host_feature_dynparams.URI = LV2DYNPARAM_URI;
  engine_ptr->host_feature_dynparams.data = NULL;

  engine_ptr->host_feature_contexts.URI = LV2_CONTEXTS_URI;
  engine_ptr->host_feature_contexts.data = NULL;

  engine_ptr->host_feature_msgcontext.URI = LV2_CONTEXT_MESSAGE;
  engine_ptr->host_feature_msgcontext.data = NULL;

  engine_ptr->host_feature_stringport.URI = LV2_STRING_PORT_URI;
  engine_ptr->host_feature_stringport.data = NULL;

  /* initialize host features array */
  count = 0;
  engine_ptr->host_features[count++] = &engine_ptr->host_feature_rtmempool;
  engine_ptr->host_features[count++] = &engine_ptr->host_feature_uri_map;
  engine_ptr->host_features[count++] = &engine_ptr->host_feature_event_ref;
  engine_ptr->host_features[count++] = &engine_ptr->host_feature_dynparams;
  engine_ptr->host_features[count++] = &engine_ptr->host_feature_contexts;
  engine_ptr->host_features[count++] = &engine_ptr->host_feature_msgcontext;
  engine_ptr->host_features[count++] = &engine_ptr->host_feature_stringport;
  assert(ZYNJACKU_ENGINE_FEATURES == count);
  engine_ptr->host_features[count] = NULL;
  /* keep in mind to update the constant when adding things here */

  zynjacku_plugin_repo_init();
}

GType zynjacku_engine_get_type()
{
  static GType type = 0;
  if (type == 0)
  {
    type = g_type_register_static_simple(
      G_TYPE_OBJECT,
      "zynjacku_engine_type",
      sizeof(ZynjackuEngineClass),
      zynjacku_engine_class_init,
      sizeof(ZynjackuEngine),
      zynjacku_engine_init,
      0);
  }

  return type;
}

gboolean
zynjacku_engine_start_jack(
  ZynjackuEngine * obj_ptr,
  const char * client_name)
{
  gboolean ret;
  int iret;
  struct zynjacku_engine * engine_ptr;

  LOG_DEBUG("zynjacku_engine_start_jack() called.");

  engine_ptr = ZYNJACKU_ENGINE_GET_PRIVATE(obj_ptr);

  if (engine_ptr->jack_client != NULL)
  {
    LOG_ERROR("Cannot start already started JACK client");
    goto fail;
  }

  INIT_LIST_HEAD(&engine_ptr->plugins_all);
  INIT_LIST_HEAD(&engine_ptr->plugins_active);
  INIT_LIST_HEAD(&engine_ptr->plugins_pending_activation);
  INIT_LIST_HEAD(&engine_ptr->midi_ports);
  INIT_LIST_HEAD(&engine_ptr->audio_ports);

  /* Connect to JACK (with plugin name as client name) */
  engine_ptr->jack_client = jack_client_open(client_name, JackNullOption, NULL);
  if (engine_ptr->jack_client == NULL)
  {
    LOG_ERROR("Failed to connect to JACK.");
    goto fail;
  }

  iret = jack_set_process_callback(engine_ptr->jack_client, &jack_process_cb, engine_ptr);
  if (iret != 0)
  {
    LOG_ERROR("jack_set_process_callback() failed.");
    ret = FALSE;
    goto fail_close_jack_client;
  }

  engine_ptr->lv2_midi_buffer.capacity = LV2MIDI_BUFFER_SIZE;
  engine_ptr->lv2_midi_buffer.data = malloc(LV2MIDI_BUFFER_SIZE);
  if (engine_ptr->lv2_midi_buffer.data == NULL)
  {
    LOG_ERROR("Failed to allocate memory for LV2 midi data buffer.");
    ret = FALSE;
    goto fail_close_jack_client;
  }

  engine_ptr->lv2_midi_event_buffer.capacity = LV2MIDI_BUFFER_SIZE;
  engine_ptr->lv2_midi_event_buffer.header_size = sizeof(LV2_Event_Buffer);
  engine_ptr->lv2_midi_event_buffer.stamp_type = LV2_EVENT_AUDIO_STAMP;
  engine_ptr->lv2_midi_event_buffer.event_count = 0;
  engine_ptr->lv2_midi_event_buffer.size = 0;
  engine_ptr->lv2_midi_event_buffer.data = malloc(LV2MIDI_BUFFER_SIZE);
  if (engine_ptr->lv2_midi_event_buffer.data == NULL)
  {
    LOG_ERROR("Failed to allocate memory for LV2 midi event data buffer.");
    ret = FALSE;
    goto fail_free_lv2_midi_buffer;
  }

  /* register JACK MIDI input port */
  engine_ptr->jack_midi_in = jack_port_register(engine_ptr->jack_client, "midi in", JACK_DEFAULT_MIDI_TYPE, JackPortIsInput, 0);
  if (engine_ptr->jack_midi_in == NULL)
  {
    LOG_ERROR("Failed to registe JACK MIDI input port.");
    ret = FALSE;
    goto fail_free_lv2_midi_event_buffer;
  }

  jack_activate(engine_ptr->jack_client);

  LOG_NOTICE("JACK client activated.");

  return TRUE;

fail_free_lv2_midi_event_buffer:
  free(engine_ptr->lv2_midi_event_buffer.data);

fail_free_lv2_midi_buffer:
  free(engine_ptr->lv2_midi_buffer.data);

fail_close_jack_client:
  jack_client_close(engine_ptr->jack_client);
  engine_ptr->jack_client = NULL;

fail:
  assert(list_empty(&engine_ptr->audio_ports));
  assert(list_empty(&engine_ptr->midi_ports));

  return FALSE;
}

void
zynjacku_engine_stop_jack(
  ZynjackuEngine * obj_ptr)
{
  struct zynjacku_engine * engine_ptr;

  LOG_DEBUG("zynjacku_engine_stop_jack() called.");

  engine_ptr = ZYNJACKU_ENGINE_GET_PRIVATE(obj_ptr);

  if (engine_ptr->jack_client == NULL)
  {
    LOG_ERROR("Cannot stop not started JACK client");
    return;
  }

  if (!list_empty(&engine_ptr->plugins_active))
  {
    LOG_ERROR("Cannot stop JACK client when there are active synths");
    return;
  }

  LOG_NOTICE("Deactivating JACK client...");

  /* Deactivate JACK */
  jack_deactivate(engine_ptr->jack_client);

  jack_port_unregister(engine_ptr->jack_client, engine_ptr->jack_midi_in);

  free(engine_ptr->lv2_midi_event_buffer.data);
  free(engine_ptr->lv2_midi_buffer.data);

  jack_client_close(engine_ptr->jack_client);

  engine_ptr->jack_client = NULL;

  assert(list_empty(&engine_ptr->audio_ports));
  assert(list_empty(&engine_ptr->midi_ports));
}

/* Translate from a JACK MIDI buffer to an LV2 MIDI buffers (both old midi port and new midi event port). */
static
bool
zynjacku_jackmidi_to_lv2midi(
  jack_port_t * jack_port,
  LV2_MIDI * midi_buf,
  LV2_Event_Buffer * event_buf,
  jack_nframes_t nframes)
{
  void * input_buf;
  jack_midi_event_t input_event;
  jack_nframes_t input_event_index;
  jack_nframes_t input_event_count;
  jack_nframes_t i;
  unsigned char * midi_data;
  LV2_Event * event_ptr;
  uint16_t size16;

  input_event_index = 0;
  midi_buf->event_count = 0;
  input_buf = jack_port_get_buffer(jack_port, nframes);
  input_event_count = jack_midi_get_event_count(input_buf);

  /* iterate over all incoming JACK MIDI events */
  midi_data = midi_buf->data;
  event_buf->event_count = 0;
  event_buf->size = 0;
  event_ptr = (LV2_Event *)event_buf->data;
  for (i = 0; i < input_event_count; i++)
  {
    /* retrieve JACK MIDI event */
    jack_midi_event_get(&input_event, input_buf, i);

    /* store event in midi port buffer */
    if ((midi_data - midi_buf->data) + sizeof(double) + sizeof(size_t) + input_event.size < midi_buf->capacity)
    {
      /* write LV2 MIDI event */
      *((double*)midi_data) = input_event.time;
      midi_data += sizeof(double);
      *((size_t*)midi_data) = input_event.size;
      midi_data += sizeof(size_t);
      memcpy(midi_data, input_event.buffer, input_event.size);

      /* normalise note events if needed */
      if ((input_event.size == 3) && ((midi_data[0] & 0xF0) == 0x90) &&
          (midi_data[2] == 0))
      {
        midi_data[0] = 0x80 | (midi_data[0] & 0x0F);
      }

      midi_data += input_event.size;
      midi_buf->event_count++;
    }
    else
    {
      /* no space left in destination buffer */
      /* TODO: notify user that midi event(s) got lost */
    }

    /* store event in event port buffer */
    if (event_buf->capacity - event_buf->size >= sizeof(LV2_Event) + input_event.size)
    {
      event_ptr->frames = input_event.time;
      event_ptr->subframes = 0;
      event_ptr->type = ZYNJACKU_MIDI_EVENT_ID;
      event_ptr->size = input_event.size;
      memcpy(event_ptr + 1, input_event.buffer, input_event.size);
        
      size16 = lv2_event_pad_size(sizeof(LV2_Event) + input_event.size);
      event_buf->size += size16;
      event_ptr = (LV2_Event *)((char *)event_ptr + size16);
      event_buf->event_count++;
    }
    else
    {
      /* no space left in destination buffer */
      /* TODO: notify user that midi event(s) got lost */
    }
  }

  midi_buf->size = midi_data - midi_buf->data;

  return input_event_count != 0;
}

static
void
zynjacku_jackmidi_cc(
  struct zynjacku_engine * engine_ptr,
  jack_port_t * jack_port,
  jack_nframes_t nframes)
{
  void * input_buf;
  jack_midi_event_t input_event;
  jack_nframes_t input_event_count;
  jack_nframes_t i;
  int cc_no;
  bool changes;

  input_buf = jack_port_get_buffer(jack_port, nframes);
  input_event_count = jack_midi_get_event_count(input_buf);

  changes = false;

  /* iterate over all incoming JACK MIDI events */
  for (i = 0; i < input_event_count; i++)
  {
    /* retrieve JACK MIDI event */
    jack_midi_event_get(&input_event, input_buf, i);

    if ((input_event.size == 3) && ((input_event.buffer[0] & 0xF0) == 0xB0))
    {
      //LOG_DEBUG("CC %u, value %u, channel %u", input_event.buffer[1], input_event.buffer[2], (input_event.buffer[0] & 0x0F));
      cc_no = input_event.buffer[1] & 0x7F;
      engine_ptr->cc[cc_no].value_rt = input_event.buffer[2] & 0x7F;
      changes = true;
    }
  }

  if (changes)
  {
    if (pthread_mutex_trylock(&engine_ptr->active_plugins_lock) == 0)
    {
      for (i = 0; i < sizeof(engine_ptr->cc) / sizeof(engine_ptr->cc[0]); i++)
      {
        engine_ptr->cc[i].value = engine_ptr->cc[i].value_rt;
      }

      pthread_mutex_unlock(&engine_ptr->active_plugins_lock);
    }
  }
}

#define engine_ptr ((struct zynjacku_engine *)context_ptr)

/* Jack process callback. */
static
int
jack_process_cb(
  jack_nframes_t nframes,
  void * context_ptr)
{
  struct list_head * synth_node_ptr;
  struct list_head * temp_node_ptr;
  struct zynjacku_plugin * synth_ptr;

  /* Copy MIDI input data to all LV2 midi in ports */
  if (zynjacku_jackmidi_to_lv2midi(
        engine_ptr->jack_midi_in,
        &engine_ptr->lv2_midi_buffer,
        &engine_ptr->lv2_midi_event_buffer,
        nframes))
  {
    engine_ptr->midi_activity = TRUE;
  }

  zynjacku_jackmidi_cc(engine_ptr, engine_ptr->jack_midi_in, nframes);

  if (pthread_mutex_trylock(&engine_ptr->active_plugins_lock) == 0)
  {
    /* Iterate over plugins pending activation */
    while (!list_empty(&engine_ptr->plugins_pending_activation))
    {
      synth_node_ptr = engine_ptr->plugins_pending_activation.next;
      list_del(synth_node_ptr); /* remove from engine_ptr->plugins_pending_activation */
      list_add_tail(synth_node_ptr, &engine_ptr->plugins_active);
    }

    pthread_mutex_unlock(&engine_ptr->active_plugins_lock);
  }

  /* Iterate over plugins */
  list_for_each_safe(synth_node_ptr, temp_node_ptr, &engine_ptr->plugins_active)
  {
    struct zynjacku_rt_command * cmd;
    synth_ptr = list_entry(synth_node_ptr, struct zynjacku_plugin, siblings_active);
    
    if (synth_ptr->recycle)
    {
      list_del(synth_node_ptr);
      synth_ptr->recycle = false;
      continue;
    }
    
    cmd = synth_ptr->command;

    /* Execute the command */
    if (cmd)
    {
      assert(!synth_ptr->command_result);
      assert(!(cmd->port->flags & PORT_FLAGS_MSGCONTEXT));
      zynjacku_lv2_connect_port(synth_ptr->lv2plugin, cmd->port, cmd->data);
    }

    if (synth_ptr->dynparams)
    {
      lv2dynparam_host_realtime_run(synth_ptr->dynparams);
    }

    /* Connect plugin LV2 output audio ports directly to JACK buffers */
    if (synth_ptr->subtype.synth.audio_out_left_port.type == PORT_TYPE_AUDIO)
    {
      zynjacku_lv2_connect_port(
        synth_ptr->lv2plugin,
        &synth_ptr->subtype.synth.audio_out_left_port,
        jack_port_get_buffer(synth_ptr->subtype.synth.audio_out_left_port.data.audio, nframes));
    }

    /* Connect plugin LV2 output audio ports directly to JACK buffers */
    if (synth_ptr->subtype.synth.audio_out_right_port.type == PORT_TYPE_AUDIO)
    {
      zynjacku_lv2_connect_port(
        synth_ptr->lv2plugin,
        &synth_ptr->subtype.synth.audio_out_right_port,
        jack_port_get_buffer(synth_ptr->subtype.synth.audio_out_right_port.data.audio, nframes));
    }

    /* Run plugin for this cycle */
    zynjacku_lv2_run(synth_ptr->lv2plugin, nframes);
    
    /* Acknowledge the command */
    if (cmd)
    {
      if (cmd->port->flags & PORT_FLAGS_IS_STRING)
        ((LV2_String_Data *)(cmd->data))->flags &= ~LV2_STRING_DATA_CHANGED_FLAG;
      synth_ptr->command = NULL;
      synth_ptr->command_result = cmd;
    }
  }

  return 0;
}

#undef engine_ptr

void
zynjacku_engine_deactivate_synth(
  GObject * synth_obj_ptr)
{
  struct zynjacku_plugin * synth_ptr;

  synth_ptr = ZYNJACKU_PLUGIN_GET_PRIVATE(synth_obj_ptr);

  synth_ptr->recycle = true;

  /* unfortunately condvars dont always work with realtime threads */
  while (synth_ptr->recycle)
  {
    usleep(10000);
  }

  list_del(&synth_ptr->siblings_all); /* remove from engine_ptr->plugins_all */

  zynjacku_lv2_deactivate(synth_ptr->lv2plugin);
}

void
zynjacku_engine_ui_run(
  ZynjackuEngine * engine_obj_ptr)
{
  struct list_head * node_ptr;
  struct zynjacku_plugin * plugin_ptr;
  struct zynjacku_engine * engine_ptr;
  unsigned int i;

//  LOG_DEBUG("zynjacku_engine_ui_run() called.");

  engine_ptr = ZYNJACKU_ENGINE_GET_PRIVATE(engine_obj_ptr);

  pthread_mutex_lock(&engine_ptr->cc_lock);
  for (i = 0 ; i < sizeof(engine_ptr->cc) / sizeof(engine_ptr->cc[0]) ; i++)
  {
    if (engine_ptr->cc[i].value != engine_ptr->cc[i].value_ui)
    {
      engine_ptr->cc[i].value_changed = true;
      engine_ptr->cc[i].value_ui = engine_ptr->cc[i].value;
    }
  }
  pthread_mutex_unlock(&engine_ptr->cc_lock);

  for (i = 0 ; i < sizeof(engine_ptr->cc) / sizeof(engine_ptr->cc[0]) ; i++)
  {
    if (engine_ptr->cc[i].value_changed)
    {
      /* Iterate over plugins */
      list_for_each(node_ptr, &engine_ptr->plugins_all)
      {
        plugin_ptr = list_entry(node_ptr, struct zynjacku_plugin, siblings_all);

        zynjacku_plugin_midi_cc(plugin_ptr, i, (unsigned int)engine_ptr->cc[i].value_ui);
      }

      engine_ptr->cc[i].value_changed = false;
    }
  }

  /* Iterate over plugins */
  list_for_each(node_ptr, &engine_ptr->plugins_all)
  {
    plugin_ptr = list_entry(node_ptr, struct zynjacku_plugin, siblings_all);

    zynjacku_plugin_ui_run(plugin_ptr);
  }
}

guint
zynjacku_engine_get_sample_rate(
  ZynjackuEngine * engine_obj_ptr)
{
  struct zynjacku_engine * engine_ptr;
  engine_ptr = ZYNJACKU_ENGINE_GET_PRIVATE(engine_obj_ptr);

  if (engine_ptr->jack_client == NULL)
  {
    g_assert_not_reached();
    return 0xDEADBEAF;
  }

  return jack_get_sample_rate(engine_ptr->jack_client);
}

gboolean
zynjacku_engine_get_midi_activity(
  ZynjackuEngine * engine_obj_ptr)
{
  gboolean ret;
  struct zynjacku_engine * engine_ptr;

  engine_ptr = ZYNJACKU_ENGINE_GET_PRIVATE(engine_obj_ptr);

  ret = engine_ptr->midi_activity;

  engine_ptr->midi_activity = FALSE;

  return ret;
}

const gchar *
zynjacku_get_version()
{
  return VERSION;
}

#define engine_obj_ptr ((ZynjackuEngine *)context)

bool
zynjacku_check_plugin(
  void * context,
  const char * plugin_uri,
  const char * plugin_name,
  uint32_t audio_in_ports_count,
  uint32_t audio_out_ports_count,
  uint32_t midi_in_ports_count,
  uint32_t control_ports_count,
  uint32_t string_ports_count,
  uint32_t event_ports_count,
  uint32_t midi_event_in_ports_count,
  uint32_t ports_count)
{
  if (midi_in_ports_count + control_ports_count + string_ports_count + event_ports_count + audio_out_ports_count != ports_count ||
      midi_in_ports_count + midi_event_in_ports_count != 1 ||
      audio_out_ports_count == 0)
  {
    LOG_DEBUG("Skipping 's' %s, [synth] plugin with unsupported port configuration", plugin_name, plugin_uri);
    LOG_DEBUG("  midi input ports: %d", (unsigned int)midi_in_ports_count);
    LOG_DEBUG("  control ports: %d", (unsigned int)control_ports_count);
    LOG_DEBUG("  string ports: %d", (unsigned int)string_ports_count);
    LOG_DEBUG("  event ports: %d", (unsigned int)event_ports_count);
    LOG_DEBUG("  event midi input ports: %d", (unsigned int)midi_event_in_ports_count);
    LOG_DEBUG("  audio input ports: %d", (unsigned int)audio_in_ports_count);
    LOG_DEBUG("  audio output ports: %d", (unsigned int)audio_out_ports_count);
    LOG_DEBUG("  total ports %d", (unsigned int)ports_count);
    return false;
  }

  LOG_DEBUG("Found \"simple\" synth plugin '%s' %s", plugin_name, plugin_uri);
  LOG_DEBUG("  midi input ports: %d", (unsigned int)midi_in_ports_count);
  LOG_DEBUG("  control ports: %d", (unsigned int)control_ports_count);
  LOG_DEBUG("  event ports: %d", (unsigned int)event_ports_count);
  LOG_DEBUG("  event midi input ports: %d", (unsigned int)midi_event_in_ports_count);
  LOG_DEBUG("  audio input ports: %d", (unsigned int)audio_in_ports_count);
  LOG_DEBUG("  audio output ports: %d", (unsigned int)audio_out_ports_count);
  LOG_DEBUG("  total ports %d", (unsigned int)ports_count);
  return true;
}

void
zynjacku_engine_tick(
  void *context,
  float progress,               /* 0..1 */
  const char *message)
{
  g_signal_emit(
    engine_obj_ptr,
    g_zynjacku_engine_signals[ZYNJACKU_ENGINE_SIGNAL_TICK],
    0,
    progress,
    message);
}

void
zynjacku_engine_tack(
  void *context,
  const char *uri)
{
  const char * name;
  const char * license;
  const char * author;

  name = zynjacku_plugin_repo_get_name(uri);
  license = zynjacku_plugin_repo_get_license(uri);
  author = zynjacku_plugin_repo_get_author(uri);

  g_signal_emit(
    engine_obj_ptr,
    g_zynjacku_engine_signals[ZYNJACKU_ENGINE_SIGNAL_TACK],
    0,
    name,
    uri,
    license,
    author);
}

#undef engine_obj_ptr

void
zynjacku_engine_iterate_plugins(
  ZynjackuEngine * engine_obj_ptr,
  gboolean force)
{
  struct zynjacku_engine * engine_ptr;

  engine_ptr = ZYNJACKU_ENGINE_GET_PRIVATE(engine_obj_ptr);

  zynjacku_plugin_repo_iterate(
    force,
    engine_ptr->host_features,
    engine_obj_ptr,
    zynjacku_check_plugin,
    zynjacku_engine_tick,
    zynjacku_engine_tack);
}

void
zynjacku_free_synth_ports(
  GObject * plugin_object_ptr)
{
  struct zynjacku_engine * engine_ptr;
  struct zynjacku_plugin * plugin_ptr;

  plugin_ptr = ZYNJACKU_PLUGIN_GET_PRIVATE(plugin_object_ptr);
  engine_ptr = ZYNJACKU_ENGINE_GET_PRIVATE(plugin_ptr->engine_object_ptr);

  zynjacku_free_plugin_ports(plugin_ptr);

  if (plugin_ptr->type == PLUGIN_TYPE_SYNTH)
  {
    if (plugin_ptr->subtype.synth.audio_out_left_port.type == PORT_TYPE_AUDIO)
    {
      jack_port_unregister(engine_ptr->jack_client, plugin_ptr->subtype.synth.audio_out_left_port.data.audio);
      list_del(&plugin_ptr->subtype.synth.audio_out_left_port.port_type_siblings);
    }

    if (plugin_ptr->subtype.synth.audio_out_right_port.type == PORT_TYPE_AUDIO) /* stereo? */
    {
      assert(plugin_ptr->subtype.synth.audio_out_left_port.type == PORT_TYPE_AUDIO);
      jack_port_unregister(engine_ptr->jack_client, plugin_ptr->subtype.synth.audio_out_right_port.data.audio);
      list_del(&plugin_ptr->subtype.synth.audio_out_right_port.port_type_siblings);
    }

    if (plugin_ptr->subtype.synth.midi_in_port.type == PORT_TYPE_MIDI ||
        plugin_ptr->subtype.synth.midi_in_port.type == PORT_TYPE_EVENT_MIDI)
    {
      list_del(&plugin_ptr->subtype.synth.midi_in_port.port_type_siblings);
    }
  }
}

#define synth_ptr (&((struct zynjacku_plugin *)context)->subtype.synth)

bool
zynjacku_synth_create_port(
  void * context,
  unsigned int port_type,
  bool output,
  uint32_t port_index)
{
  struct zynjacku_port * port_ptr;

  LOG_NOTICE("creating synth %s port of type %u, index %u", output ? "output" : "input", (unsigned int)port_type, (unsigned int)port_index);

  switch (port_type)
  {
  case PORT_TYPE_AUDIO:
    if (output)
    {
      if (synth_ptr->audio_out_left_port.type == PORT_TYPE_INVALID)
      {
        port_ptr = &synth_ptr->audio_out_left_port;
      }
      else if (synth_ptr->audio_out_right_port.type == PORT_TYPE_INVALID)
      {
        port_ptr = &synth_ptr->audio_out_right_port;
      }
      else
      {
        /* ignore, we dont support more than two audio ports yet */
        return true;
      }

      port_ptr->type = PORT_TYPE_AUDIO;
      port_ptr->flags = 0;
      port_ptr->index = port_index;

      return true;
    }
    break;
  case PORT_TYPE_MIDI:
  case PORT_TYPE_EVENT_MIDI:
    if (!output)
    {
      port_ptr = &synth_ptr->midi_in_port;
      port_ptr->type = port_type;
      port_ptr->index = port_index;
      return true;
    }
    break;
  }

  return false;
}

#undef synth_ptr
#define synth_ptr (&plugin_ptr->subtype.synth)

bool
zynjacku_plugin_construct_synth(
  struct zynjacku_plugin * plugin_ptr,
  ZynjackuPlugin * plugin_obj_ptr,
  GObject * engine_object_ptr)
{
  static unsigned int id;
  char * port_name;
  size_t size_name;
  size_t size_id;
  struct list_head * node_ptr;
  struct zynjacku_port * port_ptr;
  struct zynjacku_engine * engine_ptr;

  engine_ptr = ZYNJACKU_ENGINE_GET_PRIVATE(engine_object_ptr);

  plugin_ptr->type = PLUGIN_TYPE_SYNTH;
  synth_ptr->midi_in_port.type = PORT_TYPE_INVALID;
  synth_ptr->audio_out_left_port.type = PORT_TYPE_INVALID;
  synth_ptr->audio_out_right_port.type = PORT_TYPE_INVALID;

  if (!zynjacku_plugin_repo_load_plugin(plugin_ptr, plugin_ptr, zynjacku_synth_create_port, zynjacku_check_plugin, engine_ptr->host_features))
  {
    LOG_ERROR("Failed to load LV2 info for plugin %s", plugin_ptr->uri);
    goto fail;
  }

  plugin_ptr->lv2plugin = zynjacku_lv2_load(
    plugin_ptr->uri,
    zynjacku_engine_get_sample_rate(ZYNJACKU_ENGINE(engine_object_ptr)),
    engine_ptr->host_features);
  if (plugin_ptr->lv2plugin == NULL)
  {
    LOG_ERROR("Failed to load LV2 plugin %s", plugin_ptr->uri);
    goto fail;
  }

  if (plugin_ptr->dynparams_supported)
  {
    if (!lv2dynparam_host_attach(
          zynjacku_lv2_get_descriptor(plugin_ptr->lv2plugin),
          zynjacku_lv2_get_handle(plugin_ptr->lv2plugin),
          &engine_ptr->mempool_allocator,
          plugin_obj_ptr,
          &plugin_ptr->dynparams))
    {
      LOG_ERROR("Failed to instantiate dynparams extension.");
      goto fail_unload;
    }
  }
  else
  {
    plugin_ptr->dynparams = NULL;
  }

  plugin_ptr->engine_object_ptr = engine_object_ptr;

  /* connect midi port */
  switch (synth_ptr->midi_in_port.type)
  {
  case PORT_TYPE_MIDI:
    zynjacku_lv2_connect_port(plugin_ptr->lv2plugin, &synth_ptr->midi_in_port, &engine_ptr->lv2_midi_buffer);
    break;
  case PORT_TYPE_EVENT_MIDI:
    zynjacku_lv2_connect_port(plugin_ptr->lv2plugin, &synth_ptr->midi_in_port, &engine_ptr->lv2_midi_event_buffer);
    break;
  default:
    LOG_ERROR("don't know how to connect midi port of type %u", synth_ptr->midi_in_port.type);
    goto fail_detach_dynparams;
  }

  list_add_tail(&synth_ptr->midi_in_port.port_type_siblings, &engine_ptr->midi_ports);

  /* connect parameter ports */
  list_for_each(node_ptr, &plugin_ptr->parameter_ports)
  {
    port_ptr = list_entry(node_ptr, struct zynjacku_port, plugin_siblings);
    zynjacku_lv2_connect_port(plugin_ptr->lv2plugin, port_ptr, &port_ptr->data.parameter);
    LOG_INFO("Set %s to %f", port_ptr->symbol, port_ptr->data.parameter);
  }

  /* connect measurement ports */
  list_for_each(node_ptr, &plugin_ptr->measure_ports)
  {
    port_ptr = list_entry(node_ptr, struct zynjacku_port, plugin_siblings);
    zynjacku_lv2_connect_port(plugin_ptr->lv2plugin, port_ptr, &port_ptr->data.parameter);
  }

  size_name = strlen(plugin_ptr->name);
  port_name = malloc(size_name + 1024);
  if (port_name == NULL)
  {
    LOG_ERROR("Failed to allocate memory for port name");
    goto fail_free_ports;
  }

  /* setup audio ports (they are connected in jack process callback */
  size_id = sprintf(port_name, "%u:", id);
  memcpy(port_name + size_id, plugin_ptr->name, size_name);

  if (synth_ptr->audio_out_left_port.type == PORT_TYPE_AUDIO &&
      synth_ptr->audio_out_right_port.type == PORT_TYPE_AUDIO)
  {
    strcpy(port_name + size_id + size_name, " L");
    synth_ptr->audio_out_left_port.data.audio = jack_port_register(engine_ptr->jack_client, port_name, JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
    list_add_tail(&synth_ptr->audio_out_left_port.port_type_siblings, &engine_ptr->audio_ports);

    strcpy(port_name + size_id + size_name, " R");
    synth_ptr->audio_out_right_port.data.audio = jack_port_register(engine_ptr->jack_client, port_name, JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
    list_add_tail(&synth_ptr->audio_out_right_port.port_type_siblings, &engine_ptr->audio_ports);
  }
  else if (synth_ptr->audio_out_left_port.type == PORT_TYPE_AUDIO &&
           synth_ptr->audio_out_right_port.type == PORT_TYPE_INVALID)
  {
    port_name[size_id + size_name] = 0;
    synth_ptr->audio_out_left_port.data.audio = jack_port_register(engine_ptr->jack_client, port_name, JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
    list_add_tail(&synth_ptr->audio_out_left_port.port_type_siblings, &engine_ptr->audio_ports);
  }

  port_name[size_id + size_name] = 0;
  plugin_ptr->id = port_name;

  id++;

  /* Activate plugin */
  zynjacku_lv2_activate(plugin_ptr->lv2plugin);

  plugin_ptr->recycle = false;

  list_add_tail(&plugin_ptr->siblings_all, &engine_ptr->plugins_all);

  pthread_mutex_lock(&engine_ptr->active_plugins_lock);
  list_add_tail(&plugin_ptr->siblings_active, &engine_ptr->plugins_pending_activation);
  pthread_mutex_unlock(&engine_ptr->active_plugins_lock);

  g_object_ref(plugin_ptr->engine_object_ptr);

  /* no plugins to test gtk2gui */
  plugin_ptr->gtk2gui = zynjacku_gtk2gui_create(engine_ptr->host_features, ZYNJACKU_ENGINE_FEATURES, plugin_ptr->lv2plugin, 
    plugin_ptr, plugin_obj_ptr, plugin_ptr->uri, plugin_ptr->id, &plugin_ptr->parameter_ports);

  plugin_ptr->deactivate = zynjacku_engine_deactivate_synth;
  plugin_ptr->free_ports = zynjacku_free_synth_ports;

  LOG_DEBUG("Constructed plugin <%s>, gtk2gui <%p>", plugin_ptr->uri, plugin_ptr->gtk2gui);

  return true;

fail_free_ports:
  zynjacku_free_synth_ports(G_OBJECT(plugin_obj_ptr));
  plugin_ptr->engine_object_ptr = NULL;

fail_detach_dynparams:
  if (plugin_ptr->dynparams != NULL)
  {
    lv2dynparam_host_detach(plugin_ptr->dynparams);
    plugin_ptr->dynparams = NULL;
  }

fail_unload:
  zynjacku_lv2_unload(plugin_ptr->lv2plugin);

fail:
  return false;
}

#undef synth_ptr
