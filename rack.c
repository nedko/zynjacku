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

#include "lv2-miditype.h"
#include "lv2_event.h"
#include "lv2_uri_map.h"

#include "list.h"
#define LOG_LEVEL LOG_LEVEL_ERROR
#include "log.h"

#include "plugin.h"
#include "rack.h"
#include "gtk2gui.h"
#include "lv2.h"

#include "zynjacku.h"

#include "jack_compat.c"

#include "rtmempool.h"
#include "plugin_repo.h"
#include "lv2_event_helpers.h"

struct lv2rack_engine
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
  const LV2_Feature * host_features[4];
};

#define ZYNJACKU_RACK_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), ZYNJACKU_RACK_TYPE, struct lv2rack_engine))

#define ZYNJACKU_RACK_SIGNAL_TICK    0 /* plugin iterated */
#define ZYNJACKU_RACK_SIGNAL_TACK    1 /* "good" plugin found */
#define ZYNJACKU_RACK_SIGNALS_COUNT  2

/* URI map value for event MIDI type */
#define ZYNJACKU_MIDI_EVENT_ID 1

static guint g_zynjacku_rack_signals[ZYNJACKU_RACK_SIGNALS_COUNT];

static
int
jack_process_cb(
  jack_nframes_t nframes,
  void* data);

static void
zynjacku_rack_dispose(GObject * obj)
{
  struct lv2rack_engine * rack_ptr;

  rack_ptr = ZYNJACKU_RACK_GET_PRIVATE(obj);

  LOG_DEBUG("zynjacku_rack_dispose() called.");

  if (rack_ptr->dispose_has_run)
  {
    /* If dispose did already run, return. */
    LOG_DEBUG("zynjacku_rack_dispose() already run!");
    return;
  }

  /* Make sure dispose does not run twice. */
  rack_ptr->dispose_has_run = TRUE;

  /* 
   * In dispose, you are supposed to free all types referenced from this
   * object which might themselves hold a reference to self. Generally,
   * the most simple solution is to unref all members on which you own a 
   * reference.
   */
  if (rack_ptr->jack_client)
  {
    zynjacku_rack_stop_jack(ZYNJACKU_RACK(obj));
    zynjacku_plugin_repo_uninit();
  }

  /* Chain up to the parent class */
  G_OBJECT_CLASS(g_type_class_peek_parent(G_OBJECT_GET_CLASS(obj)))->dispose(obj);
}

static void
zynjacku_rack_finalize(GObject * obj)
{
//  struct lv2rack_engine * self = ZYNJACKU_RACK_GET_PRIVATE(obj);

  LOG_DEBUG("zynjacku_rack_finalize() called.");

  /*
   * Here, complete object destruction.
   * You might not need to do much...
   */
  //g_free(self->private);

  /* Chain up to the parent class */
  G_OBJECT_CLASS(g_type_class_peek_parent(G_OBJECT_GET_CLASS(obj)))->finalize(obj);
}

static void
zynjacku_rack_class_init(
  gpointer class_ptr,
  gpointer class_data_ptr)
{
  LOG_DEBUG("zynjacku_rack_class() called.");

  G_OBJECT_CLASS(class_ptr)->dispose = zynjacku_rack_dispose;
  G_OBJECT_CLASS(class_ptr)->finalize = zynjacku_rack_finalize;

  g_type_class_add_private(G_OBJECT_CLASS(class_ptr), sizeof(struct lv2rack_engine));

  g_zynjacku_rack_signals[ZYNJACKU_RACK_SIGNAL_TICK] =
    g_signal_new(
      "tick",                   /* signal_name */
      ZYNJACKU_RACK_TYPE,     /* itype */
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

  g_zynjacku_rack_signals[ZYNJACKU_RACK_SIGNAL_TACK] =
    g_signal_new(
      "tack",                   /* signal_name */
      ZYNJACKU_RACK_TYPE,     /* itype */
      G_SIGNAL_RUN_LAST |
      G_SIGNAL_ACTION,          /* signal_flags */
      0,                        /* class_offset */
      NULL,                     /* accumulator */
      NULL,                     /* accu_data */
      NULL,                     /* c_marshaller */
      G_TYPE_NONE,              /* return type */
      3,                        /* n_params */
      G_TYPE_STRING,            /* plugin name */
      G_TYPE_STRING,            /* plugin uri */
      G_TYPE_STRING);           /* plugin license */

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
zynjacku_rack_init(
  GTypeInstance * instance,
  gpointer g_class)
{
  struct lv2rack_engine * rack_ptr;

  LOG_DEBUG("zynjacku_rack_init() called.");

  rack_ptr = ZYNJACKU_RACK_GET_PRIVATE(instance);

  rack_ptr->dispose_has_run = FALSE;

  rack_ptr->jack_client = NULL;

  /* initialize rtsafe mempool host feature */
  rtmempool_allocator_init(&rack_ptr->mempool_allocator);

  rack_ptr->host_feature_rtmempool.URI = LV2_RTSAFE_MEMORY_POOL_URI;
  rack_ptr->host_feature_rtmempool.data = &rack_ptr->mempool_allocator;

  /* initialize uri map host feature */
  rack_ptr->uri_map.callback_data = rack_ptr;
  rack_ptr->uri_map.uri_to_id = zynjacku_uri_to_id;

  rack_ptr->host_feature_uri_map.URI = LV2_URI_MAP_URI;
  rack_ptr->host_feature_uri_map.data = &rack_ptr->uri_map;

  /* initialize event host feature */
  /* We don't support type 0 events, so the ref and unref functions just point to the same empty function. */
  rack_ptr->event.callback_data = rack_ptr;
  rack_ptr->event.lv2_event_ref = zynjacku_event_ref_func;
  rack_ptr->event.lv2_event_unref = zynjacku_event_ref_func;

  rack_ptr->host_feature_event_ref.URI = LV2_EVENT_URI;
  rack_ptr->host_feature_event_ref.data = &rack_ptr->event;

  /* initialize host features array */
  rack_ptr->host_features[0] = &rack_ptr->host_feature_rtmempool;
  rack_ptr->host_features[1] = &rack_ptr->host_feature_uri_map;
  rack_ptr->host_features[2] = &rack_ptr->host_feature_event_ref;
  rack_ptr->host_features[3] = NULL;

  zynjacku_plugin_repo_init();
}

GType zynjacku_rack_get_type()
{
  static GType type = 0;
  if (type == 0)
  {
    type = g_type_register_static_simple(
      G_TYPE_OBJECT,
      "zynjacku_rack_type",
      sizeof(ZynjackuRackClass),
      zynjacku_rack_class_init,
      sizeof(ZynjackuRack),
      zynjacku_rack_init,
      0);
  }

  return type;
}

gboolean
zynjacku_rack_start_jack(
  ZynjackuRack * obj_ptr,
  const char * client_name)
{
  gboolean ret;
  int iret;
  struct lv2rack_engine * rack_ptr;

  LOG_DEBUG("zynjacku_rack_start_jack() called.");

  rack_ptr = ZYNJACKU_RACK_GET_PRIVATE(obj_ptr);

  if (rack_ptr->jack_client != NULL)
  {
    LOG_ERROR("Cannot start already started JACK client");
    goto fail;
  }

  INIT_LIST_HEAD(&rack_ptr->plugins_all);
  INIT_LIST_HEAD(&rack_ptr->plugins_active);
  INIT_LIST_HEAD(&rack_ptr->plugins_pending_activation);
  INIT_LIST_HEAD(&rack_ptr->midi_ports);
  INIT_LIST_HEAD(&rack_ptr->audio_ports);

  /* Connect to JACK (with plugin name as client name) */
  rack_ptr->jack_client = jack_client_open(client_name, JackNullOption, NULL);
  if (rack_ptr->jack_client == NULL)
  {
    LOG_ERROR("Failed to connect to JACK.");
    goto fail;
  }

  iret = jack_set_process_callback(rack_ptr->jack_client, &jack_process_cb, rack_ptr);
  if (iret != 0)
  {
    LOG_ERROR("jack_set_process_callback() failed.");
    ret = FALSE;
    goto fail_close_jack_client;
  }

  rack_ptr->lv2_midi_buffer.capacity = LV2MIDI_BUFFER_SIZE;
  rack_ptr->lv2_midi_buffer.data = malloc(LV2MIDI_BUFFER_SIZE);
  if (rack_ptr->lv2_midi_buffer.data == NULL)
  {
    LOG_ERROR("Failed to allocate memory for LV2 midi data buffer.");
    ret = FALSE;
    goto fail_close_jack_client;
  }

  rack_ptr->lv2_midi_event_buffer.capacity = LV2MIDI_BUFFER_SIZE;
  rack_ptr->lv2_midi_event_buffer.header_size = sizeof(LV2_Event_Buffer);
  rack_ptr->lv2_midi_event_buffer.stamp_type = LV2_EVENT_AUDIO_STAMP;
  rack_ptr->lv2_midi_event_buffer.event_count = 0;
  rack_ptr->lv2_midi_event_buffer.size = 0;
  rack_ptr->lv2_midi_event_buffer.data = malloc(LV2MIDI_BUFFER_SIZE);
  if (rack_ptr->lv2_midi_event_buffer.data == NULL)
  {
    LOG_ERROR("Failed to allocate memory for LV2 midi event data buffer.");
    ret = FALSE;
    goto fail_free_lv2_midi_buffer;
  }

  /* register JACK MIDI input port */
  rack_ptr->jack_midi_in = jack_port_register(rack_ptr->jack_client, "midi in", JACK_DEFAULT_MIDI_TYPE, JackPortIsInput, 0);
  if (rack_ptr->jack_midi_in == NULL)
  {
    LOG_ERROR("Failed to registe JACK MIDI input port.");
    ret = FALSE;
    goto fail_free_lv2_midi_event_buffer;
  }

  jack_activate(rack_ptr->jack_client);

  LOG_NOTICE("JACK client activated.");

  return TRUE;

fail_free_lv2_midi_event_buffer:
  free(rack_ptr->lv2_midi_event_buffer.data);

fail_free_lv2_midi_buffer:
  free(rack_ptr->lv2_midi_buffer.data);

fail_close_jack_client:
  jack_client_close(rack_ptr->jack_client);
  rack_ptr->jack_client = NULL;

fail:
  assert(list_empty(&rack_ptr->audio_ports));
  assert(list_empty(&rack_ptr->midi_ports));

  return FALSE;
}

void
zynjacku_rack_stop_jack(
  ZynjackuRack * obj_ptr)
{
  struct lv2rack_engine * rack_ptr;

  LOG_DEBUG("zynjacku_rack_stop_jack() called.");

  rack_ptr = ZYNJACKU_RACK_GET_PRIVATE(obj_ptr);

  if (rack_ptr->jack_client == NULL)
  {
    LOG_ERROR("Cannot stop not started JACK client");
    return;
  }

  if (!list_empty(&rack_ptr->plugins_active))
  {
    LOG_ERROR("Cannot stop JACK client when there are active synths");
    return;
  }

  LOG_NOTICE("Deactivating JACK client...");

  /* Deactivate JACK */
  jack_deactivate(rack_ptr->jack_client);

  jack_port_unregister(rack_ptr->jack_client, rack_ptr->jack_midi_in);

  free(rack_ptr->lv2_midi_event_buffer.data);
  free(rack_ptr->lv2_midi_buffer.data);

  jack_client_close(rack_ptr->jack_client);

  rack_ptr->jack_client = NULL;

  assert(list_empty(&rack_ptr->audio_ports));
  assert(list_empty(&rack_ptr->midi_ports));
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

#define rack_ptr ((struct lv2rack_engine *)context_ptr)

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
        rack_ptr->jack_midi_in,
        &rack_ptr->lv2_midi_buffer,
        &rack_ptr->lv2_midi_event_buffer,
        nframes))
  {
    rack_ptr->midi_activity = TRUE;
  }

  if (pthread_mutex_trylock(&rack_ptr->active_plugins_lock) == 0)
  {
    /* Iterate over plugins pending activation */
    while (!list_empty(&rack_ptr->plugins_pending_activation))
    {
      synth_node_ptr = rack_ptr->plugins_pending_activation.next;
      list_del(synth_node_ptr); /* remove from rack_ptr->plugins_pending_activation */
      list_add_tail(synth_node_ptr, &rack_ptr->plugins_active);
    }

    pthread_mutex_unlock(&rack_ptr->active_plugins_lock);
  }

  /* Iterate over plugins */
  list_for_each_safe(synth_node_ptr, temp_node_ptr, &rack_ptr->plugins_active)
  {
    synth_ptr = list_entry(synth_node_ptr, struct zynjacku_plugin, siblings_active);

    if (synth_ptr->recycle)
    {
      list_del(synth_node_ptr);
      synth_ptr->recycle = false;
      continue;
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
        synth_ptr->subtype.synth.audio_out_left_port.index,
        jack_port_get_buffer(synth_ptr->subtype.synth.audio_out_left_port.data.audio, nframes));
    }

    /* Connect plugin LV2 output audio ports directly to JACK buffers */
    if (synth_ptr->subtype.synth.audio_out_right_port.type == PORT_TYPE_AUDIO)
    {
      zynjacku_lv2_connect_port(
        synth_ptr->lv2plugin,
        synth_ptr->subtype.synth.audio_out_right_port.index,
        jack_port_get_buffer(synth_ptr->subtype.synth.audio_out_right_port.data.audio, nframes));
    }

    /* Run plugin for this cycle */
    zynjacku_lv2_run(synth_ptr->lv2plugin, nframes);
  }

  return 0;
}

#undef rack_ptr

void
zynjacku_rack_activate_synth(
  ZynjackuRack * rack_obj_ptr,
  GObject * synth_obj_ptr)
{
  struct lv2rack_engine * rack_ptr;
  struct zynjacku_plugin * synth_ptr;

  LOG_DEBUG("zynjacku_rack_add_synth() called.");

  rack_ptr = ZYNJACKU_RACK_GET_PRIVATE(rack_obj_ptr);
  synth_ptr = ZYNJACKU_PLUGIN_GET_PRIVATE(synth_obj_ptr);

  /* Activate plugin */
  zynjacku_lv2_activate(synth_ptr->lv2plugin);

  synth_ptr->recycle = false;

  list_add_tail(&synth_ptr->siblings_all, &rack_ptr->plugins_all);

  pthread_mutex_lock(&rack_ptr->active_plugins_lock);
  list_add_tail(&synth_ptr->siblings_active, &rack_ptr->plugins_pending_activation);
  pthread_mutex_unlock(&rack_ptr->active_plugins_lock);
}

void
zynjacku_rack_deactivate_synth(
  ZynjackuRack * rack_obj_ptr,
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

  list_del(&synth_ptr->siblings_all); /* remove from rack_ptr->plugins_all */

  zynjacku_lv2_deactivate(synth_ptr->lv2plugin);
}

void
zynjacku_rack_ui_run(
  ZynjackuRack * rack_obj_ptr)
{
  struct list_head * synth_node_ptr;
  struct zynjacku_plugin * synth_ptr;
  struct lv2rack_engine * rack_ptr;

//  LOG_DEBUG("zynjacku_rack_ui_run() called.");

  rack_ptr = ZYNJACKU_RACK_GET_PRIVATE(rack_obj_ptr);

  /* Iterate over plugins */
  list_for_each(synth_node_ptr, &rack_ptr->plugins_all)
  {
    synth_ptr = list_entry(synth_node_ptr, struct zynjacku_plugin, siblings_all);

    if (synth_ptr->dynparams)
    {
      lv2dynparam_host_ui_run(synth_ptr->dynparams);
    }
  }
}

guint
zynjacku_rack_get_sample_rate(
  ZynjackuRack * rack_obj_ptr)
{
  struct lv2rack_engine * rack_ptr;
  rack_ptr = ZYNJACKU_RACK_GET_PRIVATE(rack_obj_ptr);

  if (rack_ptr->jack_client == NULL)
  {
    g_assert_not_reached();
    return 0xDEADBEAF;
  }

  return jack_get_sample_rate(rack_ptr->jack_client);
}

gboolean
zynjacku_rack_get_midi_activity(
  ZynjackuRack * rack_obj_ptr)
{
  gboolean ret;
  struct lv2rack_engine * rack_ptr;

  rack_ptr = ZYNJACKU_RACK_GET_PRIVATE(rack_obj_ptr);

  ret = rack_ptr->midi_activity;

  rack_ptr->midi_activity = FALSE;

  return ret;
}

const gchar *
zynjacku_rack_get_version()
{
  return VERSION;
}

#define rack_obj_ptr ((ZynjackuRack *)context)

void
zynjacku_rack_tick(
  void *context,
  float progress,               /* 0..1 */
  const char *message)
{
  g_signal_emit(
    rack_obj_ptr,
    g_zynjacku_rack_signals[ZYNJACKU_RACK_SIGNAL_TICK],
    0,
    progress,
    message);
}

void
zynjacku_rack_tack(
  void *context,
  const char *uri)
{
  const char * name;
  const char * license;

  name = zynjacku_plugin_repo_get_name(uri);
  license = zynjacku_plugin_repo_get_license(uri);

  g_signal_emit(
    rack_obj_ptr,
    g_zynjacku_rack_signals[ZYNJACKU_RACK_SIGNAL_TACK],
    0,
    name,
    uri,
    license);
}

#undef rack_obj_ptr

void
zynjacku_rack_iterate_plugins(
  ZynjackuRack * rack_obj_ptr,
  gboolean force)
{
  zynjacku_plugin_repo_iterate(force, rack_obj_ptr, zynjacku_rack_tick, zynjacku_rack_tack);
}
