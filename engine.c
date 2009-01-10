/* -*- Mode: C ; c-basic-offset: 2 -*- */
/*****************************************************************************
 *
 *   This file is part of zynjacku
 *
 *   Copyright (C) 2006,2007,2008,2009 Nedko Arnaudov <nedko@arnaudov.name>
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
#include <slv2/lv2_ui.h>

#include "config.h"

#include "lv2_contexts.h"
#include "lv2-miditype.h"
#include "lv2_event.h"
#include "lv2_uri_map.h"
#include "lv2_string_port.h"
#include "lv2_progress.h"

#include "list.h"
#define LOG_LEVEL LOG_LEVEL_ERROR
#include "log.h"

#include "plugin.h"
#include "engine.h"
#include "lv2.h"
#include "gtk2gui.h"

#include "zynjacku.h"
#include "plugin_internal.h"

#include "jack_compat.c"

#include "rtmempool.h"
#include "plugin_repo.h"
#include "lv2_event_helpers.h"
#include "midi_cc_map.h"
#include "midi_cc_map_internal.h"

#define ZYNJACKU_ENGINE_SIGNAL_TICK      0 /* plugin iterated */
#define ZYNJACKU_ENGINE_SIGNAL_TACK      1 /* "good" plugin found */
#define ZYNJACKU_ENGINE_SIGNAL_PROGRESS  2 /* plugin instantiation progress */
#define ZYNJACKU_ENGINE_SIGNALS_COUNT    3

/* URI map value for event MIDI type */
#define ZYNJACKU_MIDI_EVENT_ID 1

#define ZYNJACKU_ENGINE_FEATURES 8

struct zynjacku_midicc
{
  struct list_head siblings;    /* link in engine's midicc_pending_activation, unassigned_midicc_rt or a midicc_rt list */
  struct list_head siblings_ui; /* link in engine's midicc_ui list */
  struct list_head siblings_pending_cc_value_change; /* link in engine's midicc_pending_cc_value_change list */
  struct list_head siblings_pending_cc_no_change; /* link in engine's midicc_pending_cc_no_change list */
  struct list_head siblings_pending_deactivation; /* link in engine's midicc_pending_deactivation list */

  guint cc_no;
  guint cc_value;

  guint pending_cc_no;          /* protected using engine's rt_lock */

  ZynjackuMidiCcMap * map_obj_ptr;
  void * map_internal_ptr;      /* ZYNJACKU_MIDI_CC_MAP_GET_PRIVATE is not good to be used in realtime thread */
  struct zynjacku_port * port_ptr;
};

struct zynjacku_engine
{
  gboolean dispose_has_run;

  jack_client_t * jack_client;  /* the jack client */

  struct list_head plugins_all; /* accessed only from ui thread */
  struct list_head plugins_active; /* accessed only from rt thread */

  pthread_mutex_t rt_lock;
  struct list_head plugins_pending_activation; /* protected using rt_lock */

  struct list_head midi_ports;  /* PORT_TYPE_MIDI "struct zynjacku_port"s linked by port_type_siblings */
  struct list_head audio_ports; /* PORT_TYPE_AUDIO "struct zynjacku_port"s linked by port_type_siblings */
  jack_port_t * jack_midi_in;
  LV2_MIDI lv2_midi_buffer;
  LV2_Event_Buffer lv2_midi_event_buffer;
  gboolean midi_activity;

  struct lv2_rtsafe_memory_pool_provider mempool_allocator;
  LV2_URI_Map_Feature uri_map;
  LV2_Event_Feature event;
  struct lv2_progress progress;
  char * progress_plugin_name;
  char * progress_last_message;

  LV2_Feature host_feature_rtmempool;
  LV2_Feature host_feature_uri_map;
  LV2_Feature host_feature_event_ref;
  LV2_Feature host_feature_dynparams;
  LV2_Feature host_feature_contexts;
  LV2_Feature host_feature_msgcontext;
  LV2_Feature host_feature_stringport;
  LV2_Feature host_feature_progress;
  const LV2_Feature * host_features[ZYNJACKU_ENGINE_FEATURES + 1];

  struct list_head midicc_ui;   /* accessed only from ui thread */
  struct list_head midicc_pending_activation; /* protected using rt_lock */
  struct list_head midicc_pending_deactivation; /* protected using rt_lock */

  struct list_head midicc_rt[MIDICC_NO_COUNT]; /* accessed only from rt thread */
  struct list_head midicc_pending_cc_value_change; /* accessed only from rt thread */

  struct list_head midicc_pending_cc_no_change; /* protected using rt_lock */

  struct list_head unassigned_midicc_rt; /* accessed only from ui thread */
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

  pthread_mutex_destroy(&engine_ptr->rt_lock);

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

  g_zynjacku_engine_signals[ZYNJACKU_ENGINE_SIGNAL_PROGRESS] =
    g_signal_new(
      "progress",               /* signal_name */
      ZYNJACKU_ENGINE_TYPE,     /* itype */
      G_SIGNAL_RUN_LAST |
      G_SIGNAL_ACTION,          /* signal_flags */
      0,                        /* class_offset */
      NULL,                     /* accumulator */
      NULL,                     /* accu_data */
      NULL,                     /* c_marshaller */
      G_TYPE_NONE,              /* return type */
      3,                        /* n_params */
      G_TYPE_STRING,            /* plugin name */
      G_TYPE_FLOAT,             /* progress 0 .. 100 */
      G_TYPE_STRING);           /* progress message */
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
  else if (strcmp(map, LV2_UI_URI) == 0 &&
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

static
void
zynjacku_progress(
  void * context,
  float progress,
  const char * message)
{
  struct zynjacku_engine * engine_ptr;
  char * old_message;

  LOG_DEBUG("zynjacku_progress(%p, %f, '%s') called.", context, progress, message);

  engine_ptr = ZYNJACKU_ENGINE_GET_PRIVATE(context);

  old_message = engine_ptr->progress_last_message;
  if (message != NULL)
  {
    message = strdup(message);
  }

  if (old_message != NULL)
  {
    if (message == NULL)
    {
      message = old_message;
    }
  }
  else
  {
    free(old_message);
  }

  engine_ptr->progress_last_message = (char *)message;

  if (message == NULL)
  {
    LOG_DEBUG("%5.1f%% complete.", progress);
  }
  else
  {
    LOG_DEBUG("%5.1f%% complete. %s", progress, message);
  }

  /* make NULL message pointer signal friendly */
  if (message == NULL)
  {
    message = "";
  }

  g_signal_emit(
    context,
    g_zynjacku_engine_signals[ZYNJACKU_ENGINE_SIGNAL_PROGRESS],
    0,
    engine_ptr->progress_plugin_name,
    (gfloat)progress,
    message);
}

static void
zynjacku_engine_init(
  GTypeInstance * instance,
  gpointer g_class)
{
  struct zynjacku_engine * engine_ptr;
  int count;

  LOG_DEBUG("zynjacku_engine_init() called.");

  engine_ptr = ZYNJACKU_ENGINE_GET_PRIVATE(instance);

  engine_ptr->dispose_has_run = FALSE;

  engine_ptr->jack_client = NULL;

  pthread_mutex_init(&engine_ptr->rt_lock, NULL);

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

  /* initialize progress host feature */
  engine_ptr->progress.progress = zynjacku_progress;
  engine_ptr->progress.context = NULL;
  engine_ptr->progress_plugin_name = NULL;
  engine_ptr->progress_last_message = NULL;

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

  engine_ptr->host_feature_progress.URI = LV2_PROGRESS_URI;
  engine_ptr->host_feature_progress.data = &engine_ptr->progress;

  /* initialize host features array */
  count = 0;
  engine_ptr->host_features[count++] = &engine_ptr->host_feature_rtmempool;
  engine_ptr->host_features[count++] = &engine_ptr->host_feature_uri_map;
  engine_ptr->host_features[count++] = &engine_ptr->host_feature_event_ref;
  engine_ptr->host_features[count++] = &engine_ptr->host_feature_dynparams;
  engine_ptr->host_features[count++] = &engine_ptr->host_feature_contexts;
  engine_ptr->host_features[count++] = &engine_ptr->host_feature_msgcontext;
  engine_ptr->host_features[count++] = &engine_ptr->host_feature_stringport;
  engine_ptr->host_features[count++] = &engine_ptr->host_feature_progress;
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
  unsigned int i;

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

  INIT_LIST_HEAD(&engine_ptr->midicc_ui);
  INIT_LIST_HEAD(&engine_ptr->midicc_pending_activation);
  INIT_LIST_HEAD(&engine_ptr->midicc_pending_deactivation);

  for (i = 0; i < MIDICC_NO_COUNT; i++)
  {
    INIT_LIST_HEAD(&engine_ptr->midicc_rt[i]);
  }

  INIT_LIST_HEAD(&engine_ptr->midicc_pending_cc_value_change);
  INIT_LIST_HEAD(&engine_ptr->midicc_pending_cc_no_change);
  INIT_LIST_HEAD(&engine_ptr->unassigned_midicc_rt);

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

//static
void
zynjacku_jackmidi_cc(
  struct zynjacku_engine * engine_ptr,
  jack_port_t * jack_port,
  jack_nframes_t nframes)
{
  struct list_head * node_ptr;
  struct zynjacku_midicc * midicc_ptr;
  void * input_buf;
  jack_midi_event_t input_event;
  jack_nframes_t input_event_count;
  jack_nframes_t i;
  guint cc_no;
  guint cc_value;
  float cc_fvalue;
  gint pitch;
  gfloat mapvalue;
  union lv2dynparam_host_parameter_value dynvalue;
  uint8_t status;

  if (pthread_mutex_trylock(&engine_ptr->rt_lock) == 0)
  {
    /* Iterate over midicc pending activation */
    while (!list_empty(&engine_ptr->midicc_pending_activation))
    {
      node_ptr = engine_ptr->midicc_pending_activation.next;
      midicc_ptr = list_entry(node_ptr, struct zynjacku_midicc, siblings);

      assert(ZYNJACKU_IS_MIDI_CC_MAP(midicc_ptr->map_obj_ptr));

      list_del(node_ptr); /* remove from engine_ptr->midicc_pending_activation */

      if (midicc_ptr->cc_no == G_MAXUINT)
      {
        list_add_tail(node_ptr, &engine_ptr->unassigned_midicc_rt);
      }
      else
      {
        list_add_tail(node_ptr, engine_ptr->midicc_rt + midicc_ptr->cc_no);
      }
    }

    /* Iterate over midicc pending deactivation  */
    while (!list_empty(&engine_ptr->midicc_pending_deactivation))
    {
      node_ptr = engine_ptr->midicc_pending_deactivation.next;
      midicc_ptr = list_entry(node_ptr, struct zynjacku_midicc, siblings_pending_deactivation);

      assert(ZYNJACKU_IS_MIDI_CC_MAP(midicc_ptr->map_obj_ptr));

      /* remove from engine_ptr->midicc_pending_deactivation */
      list_del_init(node_ptr);

      /* remove from engine's unassigned_midicc_rt or one of midicc_rt list */
      list_del(&midicc_ptr->siblings);

      if (!list_empty(&midicc_ptr->siblings_pending_cc_no_change))
      {
        list_del(&midicc_ptr->siblings_pending_cc_no_change);
      }

      if (!list_empty(&midicc_ptr->siblings_pending_cc_value_change))
      {
        list_del(&midicc_ptr->siblings_pending_cc_value_change);
      }
    }

    /* Iterate over midicc with pending cc no change */
    while (!list_empty(&engine_ptr->midicc_pending_cc_no_change))
    {
      node_ptr = engine_ptr->midicc_pending_cc_no_change.next;
      midicc_ptr = list_entry(node_ptr, struct zynjacku_midicc, siblings_pending_cc_no_change);

      assert(ZYNJACKU_IS_MIDI_CC_MAP(midicc_ptr->map_obj_ptr));

      list_del_init(node_ptr); /* remove from engine_ptr->midicc_pending_cc_no_change */

      list_del(&midicc_ptr->siblings); /* remove from current midicc_rt list */

      midicc_ptr->cc_no = midicc_ptr->pending_cc_no;
      midicc_ptr->pending_cc_no = G_MAXUINT;
      list_add_tail(node_ptr, engine_ptr->midicc_rt + midicc_ptr->cc_no);
    }

    /* Iterate over midicc with pending value change */
    while (!list_empty(&engine_ptr->midicc_pending_cc_value_change))
    {
      node_ptr = engine_ptr->midicc_pending_cc_value_change.next;
      midicc_ptr = list_entry(node_ptr, struct zynjacku_midicc, siblings_pending_cc_value_change);

      assert(ZYNJACKU_IS_MIDI_CC_MAP(midicc_ptr->map_obj_ptr));

      list_del_init(node_ptr); /* remove from engine_ptr->midicc_pending_cc_value_change */

      zynjacku_midiccmap_midi_cc_rt(midicc_ptr->map_obj_ptr, midicc_ptr->cc_no, midicc_ptr->cc_value);
    }

    pthread_mutex_unlock(&engine_ptr->rt_lock);
  }

  input_buf = jack_port_get_buffer(jack_port, nframes);
  input_event_count = jack_midi_get_event_count(input_buf);

  /* iterate over all incoming JACK MIDI events */
  for (i = 0; i < input_event_count; i++)
  {
    /* retrieve JACK MIDI event */
    jack_midi_event_get(&input_event, input_buf, i);

    /* status byte minus channel */
    status = input_event.buffer[0] & 0xF0;

    if (input_event.size == 3 && (status == 0xB0 || status == 0xE0))
    {
      if (status == 0xB0)
      {
        cc_no = input_event.buffer[1] & 0x7F;
        cc_value = input_event.buffer[2] & 0x7F;
        cc_fvalue = (float)cc_value / 127.0;
        LOG_DEBUG("CC %u, value %u (%f), channel %u", cc_no, cc_value, cc_fvalue, (input_event.buffer[0] & 0x0F));
      }
      else                      /* pitch wheel */
      {
        cc_no = 144;            /* fake */
        pitch = input_event.buffer[1] & 0x7F;
        pitch |= (input_event.buffer[2] & 0x7F) << 7;
        cc_value = pitch >> 7;
        pitch -= 0x2000;

        if (pitch < 0)
        {
          cc_fvalue = (float)pitch / 0x2000;
        }
        else
        {
          cc_fvalue = (float)pitch / (0x2000 - 1);
        }

        /* -1..1 -> 0..1 */
        cc_fvalue += 1.0;
        cc_fvalue /= 2;

        LOG_DEBUG("Pitch %d, value %f, channel %u", pitch, cc_fvalue, (input_event.buffer[0] & 0x0F));
      }

      /* assign all unassigned midicc maps */
      while (!list_empty(&engine_ptr->unassigned_midicc_rt))
      {
        node_ptr = engine_ptr->unassigned_midicc_rt.next;
        midicc_ptr = list_entry(node_ptr, struct zynjacku_midicc, siblings);

        assert(ZYNJACKU_IS_MIDI_CC_MAP(midicc_ptr->map_obj_ptr));

        //LOG_DEBUG("assigning  cc no %u to map %p", cc_no, midicc_ptr);

        midicc_ptr->cc_no = cc_no;

        list_del(node_ptr); /* remove from engine_ptr->unassigned_midicc_rt */
        list_add_tail(node_ptr, engine_ptr->midicc_rt + cc_no);
      }

      list_for_each(node_ptr, engine_ptr->midicc_rt + cc_no)
      {
        midicc_ptr = list_entry(node_ptr, struct zynjacku_midicc, siblings);

        assert(ZYNJACKU_IS_MIDI_CC_MAP(midicc_ptr->map_obj_ptr));
        assert(PORT_IS_INPUT(midicc_ptr->port_ptr));

        if (pthread_mutex_trylock(&engine_ptr->rt_lock) == 0)
        {
          zynjacku_midiccmap_midi_cc_rt(midicc_ptr->map_obj_ptr, cc_no, cc_value);
          pthread_mutex_unlock(&engine_ptr->rt_lock);
        }
        else
        {
          /* we are not lucky enough, ui thread is holding the lock */
          /* postpone value change for next cycle */
          midicc_ptr->cc_value = cc_value;
          list_add_tail(&midicc_ptr->siblings_pending_cc_value_change, &engine_ptr->midicc_pending_cc_value_change);
        }

        mapvalue = zynjacku_midiccmap_map_cc_rt(midicc_ptr->map_internal_ptr, cc_fvalue);
        LOG_DEBUG("%u (%f) mapped to %f", cc_value, cc_fvalue, (float)mapvalue);

        switch (midicc_ptr->port_ptr->type)
        {
        case PORT_TYPE_LV2_FLOAT:
          midicc_ptr->port_ptr->data.lv2float.value = mapvalue;
          break;
        case PORT_TYPE_DYNPARAM:
          switch (midicc_ptr->port_ptr->data.dynparam.type)
          {
          case LV2DYNPARAM_PARAMETER_TYPE_FLOAT:
            dynvalue.fpoint = mapvalue;
            lv2dynparam_parameter_change_rt(midicc_ptr->port_ptr->plugin_ptr->dynparams, midicc_ptr->port_ptr->data.dynparam.handle, dynvalue);
            break;
          }

          break;
        }
      }
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
  void * old_data;

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

  if (pthread_mutex_trylock(&engine_ptr->rt_lock) == 0)
  {
    /* Iterate over plugins pending activation */
    while (!list_empty(&engine_ptr->plugins_pending_activation))
    {
      synth_node_ptr = engine_ptr->plugins_pending_activation.next;
      list_del(synth_node_ptr); /* remove from engine_ptr->plugins_pending_activation */
      list_add_tail(synth_node_ptr, &engine_ptr->plugins_active);
    }

    pthread_mutex_unlock(&engine_ptr->rt_lock);
  }

  /* Iterate over plugins */
  list_for_each_safe(synth_node_ptr, temp_node_ptr, &engine_ptr->plugins_active)
  {
    synth_ptr = list_entry(synth_node_ptr, struct zynjacku_plugin, siblings_active);
    
    if (synth_ptr->recycle)
    {
      list_del(synth_node_ptr);
      synth_ptr->recycle = false;
      continue;
    }

    old_data = zynjacku_plugin_prerun_rt(synth_ptr);

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
    
    zynjacku_plugin_postrun_rt(synth_ptr, old_data);
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
  struct zynjacku_engine * engine_ptr;
  struct list_head * node_ptr;
  struct zynjacku_midicc * midicc_ptr;
  struct zynjacku_plugin * plugin_ptr;

//  LOG_DEBUG("zynjacku_engine_ui_run() called.");

  engine_ptr = ZYNJACKU_ENGINE_GET_PRIVATE(engine_obj_ptr);

  pthread_mutex_lock(&engine_ptr->rt_lock);

  /* Iterate over midi cc maps */
  list_for_each(node_ptr, &engine_ptr->midicc_ui)
  {
    midicc_ptr = list_entry(node_ptr, struct zynjacku_midicc, siblings_ui);
    zynjacku_midiccmap_ui_run(midicc_ptr->map_obj_ptr);
  }

  pthread_mutex_unlock(&engine_ptr->rt_lock);

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

static
bool
zynjacku_set_midi_cc_map(
  GObject * engine_obj_ptr,
  struct zynjacku_port * port_ptr,
  GObject * midi_cc_map_obj_ptr)
{
  struct zynjacku_engine * engine_ptr;
  struct zynjacku_midicc * midicc_ptr;
  struct list_head * node_ptr;

  engine_ptr = ZYNJACKU_ENGINE_GET_PRIVATE(engine_obj_ptr);

  LOG_DEBUG("zynjacku_set_midi_cc_map(port=%p, map=%p) called.", port_ptr, midi_cc_map_obj_ptr);

  if (midi_cc_map_obj_ptr != NULL)
  {
    LOG_DEBUG("new midicc");

    midicc_ptr = malloc(sizeof(struct zynjacku_midicc));
    //LOG_DEBUG("midicc struct at %p", midicc_ptr);
    if (midicc_ptr == NULL)
    {
      LOG_ERROR("Failed to allocate memory for struct zynjacku_midicc");
      return false;
    }

    assert(midi_cc_map_obj_ptr != NULL);

    midicc_ptr->port_ptr = port_ptr;

    g_object_ref(midi_cc_map_obj_ptr);
    midicc_ptr->map_obj_ptr = ZYNJACKU_MIDI_CC_MAP(midi_cc_map_obj_ptr);
    //LOG_DEBUG("midi cc map is %p", midicc_ptr->map_obj_ptr);
    assert(midicc_ptr->map_obj_ptr != NULL);

    midicc_ptr->map_internal_ptr = zynjacku_midiccmap_get_internal_ptr(midicc_ptr->map_obj_ptr);
    midicc_ptr->cc_no = zynjacku_midiccmap_get_cc_no(midicc_ptr->map_obj_ptr);
    midicc_ptr->pending_cc_no = G_MAXUINT;

    INIT_LIST_HEAD(&midicc_ptr->siblings_pending_cc_no_change);
    INIT_LIST_HEAD(&midicc_ptr->siblings_pending_cc_value_change);

    pthread_mutex_lock(&engine_ptr->rt_lock);
    list_add_tail(&midicc_ptr->siblings, &engine_ptr->midicc_pending_activation);
    pthread_mutex_unlock(&engine_ptr->rt_lock);

    list_add_tail(&midicc_ptr->siblings_ui, &engine_ptr->midicc_ui);

    return true;
  }

  LOG_DEBUG("remove midicc");

  /* Iterate over midi cc maps */
  list_for_each(node_ptr, &engine_ptr->midicc_ui)
  {
    midicc_ptr = list_entry(node_ptr, struct zynjacku_midicc, siblings_ui);

    if (midicc_ptr->port_ptr == port_ptr)
    {
      /* add to pending deactivation list */
      pthread_mutex_lock(&engine_ptr->rt_lock);
      list_add_tail(&midicc_ptr->siblings_pending_deactivation, &engine_ptr->midicc_pending_deactivation);
      pthread_mutex_unlock(&engine_ptr->rt_lock);

      /* unfortunately condvars dont always work with realtime threads */
      pthread_mutex_lock(&engine_ptr->rt_lock);
      while (!list_empty(&midicc_ptr->siblings_pending_deactivation))
      {
        pthread_mutex_unlock(&engine_ptr->rt_lock);
        usleep(10000);
        pthread_mutex_lock(&engine_ptr->rt_lock);
      }
      pthread_mutex_unlock(&engine_ptr->rt_lock);

      list_del(&midicc_ptr->siblings_ui); /* remove from engine_ptr->midicc_ui */

      g_object_ref(midicc_ptr->map_obj_ptr);
      free(midicc_ptr);

      return true;
    }
  }

  LOG_ERROR("Cannot remove MIDI CC map because cannot find the port %p", port_ptr);
  return false;
}

static
bool
zynjacku_midi_cc_map_cc_no_assign(
  GObject * engine_obj_ptr,
  GObject * midi_cc_map_obj_ptr,
  guint cc_no)
{
  struct zynjacku_engine * engine_ptr;
  ZynjackuMidiCcMap * map_obj_ptr;
  struct list_head * node_ptr;
  struct zynjacku_midicc * midicc_ptr;

  engine_ptr = ZYNJACKU_ENGINE_GET_PRIVATE(engine_obj_ptr);
  map_obj_ptr = ZYNJACKU_MIDI_CC_MAP(midi_cc_map_obj_ptr);

  LOG_DEBUG("zynjacku_midi_cc_map_cc_no_assign() called.");

  if (cc_no == G_MAXUINT)
  {
    assert(0);
    return false;
  }

  /* Iterate over midi cc maps */
  list_for_each(node_ptr, &engine_ptr->midicc_ui)
  {
    midicc_ptr = list_entry(node_ptr, struct zynjacku_midicc, siblings_ui);

    if (midicc_ptr->map_obj_ptr == map_obj_ptr)
    {
      pthread_mutex_lock(&engine_ptr->rt_lock);

      if (midicc_ptr->cc_no != cc_no)
      {
        midicc_ptr->pending_cc_no = cc_no;
        list_add_tail(&midicc_ptr->siblings_pending_cc_no_change, &engine_ptr->midicc_pending_cc_no_change);
      }

      pthread_mutex_unlock(&engine_ptr->rt_lock);
    }
  }

  LOG_ERROR("Cannot assign MIDI CC No because cannot find the map %p", midi_cc_map_obj_ptr);
  return false;                 /* not found */
}

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

  engine_ptr->progress.context = engine_object_ptr;
  engine_ptr->progress_last_message = NULL;
  engine_ptr->progress_plugin_name = plugin_ptr->name;

  plugin_ptr->lv2plugin = zynjacku_lv2_load(
    plugin_ptr->uri,
    zynjacku_engine_get_sample_rate(ZYNJACKU_ENGINE(engine_object_ptr)),
    engine_ptr->host_features);

  engine_ptr->progress.context = NULL;
  if (engine_ptr->progress_last_message != NULL)
  {
    free(engine_ptr->progress_last_message);
    engine_ptr->progress_last_message = NULL;
  }
  engine_ptr->progress_plugin_name = NULL;

  if (plugin_ptr->lv2plugin == NULL)
  {
    LOG_ERROR("Failed to load LV2 plugin %s", plugin_ptr->uri);
    goto fail;
  }

  /* connect parameter/measure ports */
  if (!zynjacku_connect_plugin_ports(plugin_ptr, plugin_obj_ptr, engine_object_ptr, &engine_ptr->mempool_allocator))
  {
    goto fail_unload;
  }

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

  /* setup audio ports (they are connected in jack process callback */

  size_name = strlen(plugin_ptr->name);
  port_name = malloc(size_name + 1024);
  if (port_name == NULL)
  {
    LOG_ERROR("Failed to allocate memory for port name");
    goto fail_free_ports;
  }

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

  pthread_mutex_lock(&engine_ptr->rt_lock);
  list_add_tail(&plugin_ptr->siblings_active, &engine_ptr->plugins_pending_activation);
  pthread_mutex_unlock(&engine_ptr->rt_lock);

  g_object_ref(plugin_ptr->engine_object_ptr);

  /* no plugins to test gtk2gui */
  plugin_ptr->gtk2gui = zynjacku_gtk2gui_create(engine_ptr->host_features, ZYNJACKU_ENGINE_FEATURES, plugin_ptr->lv2plugin, 
    plugin_ptr, plugin_obj_ptr, plugin_ptr->uri, plugin_ptr->id, &plugin_ptr->parameter_ports);

  plugin_ptr->deactivate = zynjacku_engine_deactivate_synth;
  plugin_ptr->free_ports = zynjacku_free_synth_ports;
  plugin_ptr->set_midi_cc_map = zynjacku_set_midi_cc_map;
  plugin_ptr->midi_cc_map_cc_no_assign = zynjacku_midi_cc_map_cc_no_assign;

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
