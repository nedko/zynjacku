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

#include "config.h"

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <lv2.h>
#include <jack/jack.h>
#include <glib-object.h>
#if HAVE_DYNPARAMS
#include <lv2dynparam/lv2dynparam.h>
#include <lv2dynparam/lv2_rtmempool.h>
#include <lv2dynparam/host.h>
#endif

#include "lv2_contexts.h"
#include "lv2_event.h"
#include "lv2_uri_map.h"
#include "lv2_string_port.h"
#include "lv2_progress.h"

#include "list.h"
#define LOG_LEVEL LOG_LEVEL_ERROR
#include "log.h"

#include "plugin.h"
#include "rack.h"
#include "lv2.h"
#include "gtk2gui.h"

#include "zynjacku.h"
#include "plugin_internal.h"

#include "jack_compat.c"

#if HAVE_DYNPARAMS
#include "rtmempool.h"
#endif
#include "lv2_event_helpers.h"

#if HAVE_DYNPARAMS
#define ZYNJACKU_RACK_ENGINE_FEATURES 6
#else
#define ZYNJACKU_RACK_ENGINE_FEATURES 4
#endif

struct lv2rack_engine
{
  gboolean dispose_has_run;

  jack_client_t * jack_client;  /* the jack client */

  struct list_head plugins_all; /* accessed only from ui thread */
  struct list_head plugins_active; /* accessed only from rt thread */

  pthread_mutex_t active_plugins_lock;
  struct list_head plugins_pending_activation; /* protected using active_plugins_lock */

  jack_port_t * audio_in_left;
  jack_port_t * audio_in_right;

#if HAVE_DYNPARAMS
  struct lv2_rtsafe_memory_pool_provider mempool_allocator;
#endif
  LV2_URI_Map_Feature uri_map;
  LV2_Event_Feature event;
  struct lv2_progress progress;
  char * progress_plugin_name;
  char * progress_last_message;

#if HAVE_DYNPARAMS
  LV2_Feature host_feature_rtmempool;
  LV2_Feature host_feature_dynparams;
#endif
  LV2_Feature host_feature_contexts;
  LV2_Feature host_feature_msgcontext;
  LV2_Feature host_feature_stringport;
  LV2_Feature host_feature_progress;
  const LV2_Feature * host_features[ZYNJACKU_RACK_ENGINE_FEATURES + 1];
};

#define ZYNJACKU_RACK_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), ZYNJACKU_RACK_TYPE, struct lv2rack_engine))

#define ZYNJACKU_RACK_SIGNAL_PROGRESS  0 /* plugin instantiation progress */
#define ZYNJACKU_RACK_SIGNALS_COUNT    1

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

  g_zynjacku_rack_signals[ZYNJACKU_RACK_SIGNAL_PROGRESS] =
    g_signal_new(
      "progress",               /* signal_name */
      ZYNJACKU_RACK_TYPE,       /* itype */
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
void
zynjacku_progress(
  void * context,
  float progress,
  const char * message)
{
  struct lv2rack_engine * rack_ptr;
  char * old_message;

  LOG_DEBUG("zynjacku_progress(%p, %f, '%s') called.", context, progress, message);

  rack_ptr = ZYNJACKU_RACK_GET_PRIVATE(context);

  old_message = rack_ptr->progress_last_message;
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

  rack_ptr->progress_last_message = (char *)message;

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
    g_zynjacku_rack_signals[ZYNJACKU_RACK_SIGNAL_PROGRESS],
    0,
    rack_ptr->progress_plugin_name,
    (gfloat)progress,
    message);
}

static void
zynjacku_rack_init(
  GTypeInstance * instance,
  gpointer g_class)
{
  struct lv2rack_engine * rack_ptr;
  int count;

  LOG_DEBUG("zynjacku_rack_init() called.");

  rack_ptr = ZYNJACKU_RACK_GET_PRIVATE(instance);

  rack_ptr->dispose_has_run = FALSE;

  rack_ptr->jack_client = NULL;

#if HAVE_DYNPARAMS
  /* initialize rtsafe mempool host feature */
  rtmempool_allocator_init(&rack_ptr->mempool_allocator);
#endif

  /* initialize progress host feature */
  rack_ptr->progress.progress = zynjacku_progress;
  rack_ptr->progress.context = NULL;
  rack_ptr->progress_plugin_name = NULL;
  rack_ptr->progress_last_message = NULL;

#if HAVE_DYNPARAMS
  rack_ptr->host_feature_rtmempool.URI = LV2_RTSAFE_MEMORY_POOL_URI;
  rack_ptr->host_feature_rtmempool.data = &rack_ptr->mempool_allocator;

  rack_ptr->host_feature_dynparams.URI = LV2DYNPARAM_URI;
  rack_ptr->host_feature_dynparams.data = NULL;
#endif

  rack_ptr->host_feature_contexts.URI = LV2_CONTEXTS_URI;
  rack_ptr->host_feature_contexts.data = NULL;

  rack_ptr->host_feature_msgcontext.URI = LV2_CONTEXT_MESSAGE;
  rack_ptr->host_feature_msgcontext.data = NULL;

  rack_ptr->host_feature_stringport.URI = LV2_STRING_PORT_URI;
  rack_ptr->host_feature_stringport.data = NULL;

  rack_ptr->host_feature_progress.URI = LV2_PROGRESS_URI;
  rack_ptr->host_feature_progress.data = &rack_ptr->progress;

  /* initialize host features array */
  count = 0;
#if HAVE_DYNPARAMS
  rack_ptr->host_features[count++] = &rack_ptr->host_feature_rtmempool;
  rack_ptr->host_features[count++] = &rack_ptr->host_feature_dynparams;
#endif
  rack_ptr->host_features[count++] = &rack_ptr->host_feature_contexts;
  rack_ptr->host_features[count++] = &rack_ptr->host_feature_msgcontext;
  rack_ptr->host_features[count++] = &rack_ptr->host_feature_stringport;
  rack_ptr->host_features[count++] = &rack_ptr->host_feature_progress;
  assert(ZYNJACKU_RACK_ENGINE_FEATURES == count);
  rack_ptr->host_features[count] = NULL;
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

  rack_ptr->audio_in_left = NULL;
  rack_ptr->audio_in_right = NULL;

  /* register JACK input ports */
  rack_ptr->audio_in_left = jack_port_register(rack_ptr->jack_client, "left", JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
  rack_ptr->audio_in_right = jack_port_register(rack_ptr->jack_client, "right", JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
  if (rack_ptr->audio_in_left == NULL || rack_ptr->audio_in_right == NULL)
  {
    LOG_ERROR("Failed to register input port.");
    ret = FALSE;
    goto fail_close_jack_client;
  }

  jack_activate(rack_ptr->jack_client);

  LOG_NOTICE("JACK client activated.");

  return TRUE;

fail_close_jack_client:
  jack_client_close(rack_ptr->jack_client);
  rack_ptr->jack_client = NULL;

fail:
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
    LOG_ERROR("Cannot stop JACK client when there are active effects");
    return;
  }

  LOG_NOTICE("Deactivating JACK client...");

  /* Deactivate JACK */
  jack_deactivate(rack_ptr->jack_client);

  jack_client_close(rack_ptr->jack_client);

  rack_ptr->jack_client = NULL;
}

#define rack_ptr ((struct lv2rack_engine *)context_ptr)

/* Jack process callback. */
static
int
jack_process_cb(
  jack_nframes_t nframes,
  void * context_ptr)
{
  struct list_head * effect_node_ptr;
  struct list_head * temp_node_ptr;
  struct zynjacku_plugin * effect_ptr;
  void * left;
  void * right;
  bool mono;
  void * old_data;

  if (pthread_mutex_trylock(&rack_ptr->active_plugins_lock) == 0)
  {
    /* Iterate over plugins pending activation */
    while (!list_empty(&rack_ptr->plugins_pending_activation))
    {
      effect_node_ptr = rack_ptr->plugins_pending_activation.next;
      list_del(effect_node_ptr); /* remove from rack_ptr->plugins_pending_activation */
      list_add_tail(effect_node_ptr, &rack_ptr->plugins_active);
    }

    pthread_mutex_unlock(&rack_ptr->active_plugins_lock);
  }

  left = jack_port_get_buffer(rack_ptr->audio_in_left, nframes);
  right = jack_port_get_buffer(rack_ptr->audio_in_right, nframes);
  mono = false;

  /* Iterate over plugins */
  list_for_each_safe(effect_node_ptr, temp_node_ptr, &rack_ptr->plugins_active)
  {
    effect_ptr = list_entry(effect_node_ptr, struct zynjacku_plugin, siblings_active);

    if (effect_ptr->recycle)
    {
      list_del(effect_node_ptr);
      effect_ptr->recycle = false;
      continue;
    }

    old_data = zynjacku_plugin_prerun_rt(effect_ptr);

#if HAVE_DYNPARAMS
    if (effect_ptr->dynparams)
    {
      lv2dynparam_host_realtime_run(effect_ptr->dynparams);
    }
#endif

    /* Connect plugin LV2 input audio ports */
    zynjacku_lv2_connect_port(
      effect_ptr->lv2plugin,
      effect_ptr->subtype.effect.audio_in_left_port_ptr,
      left);

    if (effect_ptr->subtype.effect.audio_in_right_port_ptr != NULL)
    {
      zynjacku_lv2_connect_port(
        effect_ptr->lv2plugin,
        effect_ptr->subtype.effect.audio_in_right_port_ptr,
        mono ? left : right);
    }

    /* Connect plugin LV2 output audio ports directly to JACK buffers */
    left = jack_port_get_buffer(effect_ptr->subtype.effect.audio_out_left_port_ptr->data.audio, nframes);
    zynjacku_lv2_connect_port(
      effect_ptr->lv2plugin,
      effect_ptr->subtype.effect.audio_out_left_port_ptr,
      left);

    mono = effect_ptr->subtype.effect.audio_out_right_port_ptr == NULL;
    if (!mono)
    {
      right = jack_port_get_buffer(effect_ptr->subtype.effect.audio_out_right_port_ptr->data.audio, nframes);
      zynjacku_lv2_connect_port(
        effect_ptr->lv2plugin,
        effect_ptr->subtype.effect.audio_out_right_port_ptr,
        right);
    }

    /* Run plugin for this cycle */
    zynjacku_lv2_run(effect_ptr->lv2plugin, nframes);
    
    zynjacku_plugin_postrun_rt(effect_ptr, old_data);
  }

  return 0;
}

#undef rack_ptr

static
void
zynjacku_rack_deactivate_effect(
  GObject * effect_obj_ptr)
{
  struct zynjacku_plugin * effect_ptr;

  effect_ptr = ZYNJACKU_PLUGIN_GET_PRIVATE(effect_obj_ptr);

  effect_ptr->recycle = true;

  /* unfortunately condvars dont always work with realtime threads */
  while (effect_ptr->recycle)
  {
    usleep(10000);
  }

  list_del(&effect_ptr->siblings_all); /* remove from rack_ptr->plugins_all */

  zynjacku_lv2_deactivate(effect_ptr->lv2plugin);
}

void
zynjacku_rack_get_required_features(
  GObject * rack_obj_ptr,
  const LV2_Feature * const ** host_features,
  unsigned int * host_feature_count)
{
  struct lv2rack_engine * rack_ptr;

  rack_ptr = ZYNJACKU_RACK_GET_PRIVATE(rack_obj_ptr);

  *host_features = rack_ptr->host_features;
  *host_feature_count = ZYNJACKU_RACK_ENGINE_FEATURES;
}

static
void
zynjacku_rack_unregister_port(
  GObject * rack_obj_ptr,
  struct zynjacku_port * port_ptr)
{
  struct lv2rack_engine * rack_ptr;

  rack_ptr = ZYNJACKU_RACK_GET_PRIVATE(rack_obj_ptr);

  if (port_ptr->data.audio != NULL)
  {
    jack_port_unregister(rack_ptr->jack_client, port_ptr->data.audio);
  }
}

void
zynjacku_rack_ui_run(
  ZynjackuRack * rack_obj_ptr)
{
  struct list_head * effect_node_ptr;
  struct zynjacku_plugin * effect_ptr;
  struct lv2rack_engine * rack_ptr;

//  LOG_DEBUG("zynjacku_rack_ui_run() called.");

  rack_ptr = ZYNJACKU_RACK_GET_PRIVATE(rack_obj_ptr);

  /* Iterate over plugins */
  list_for_each(effect_node_ptr, &rack_ptr->plugins_all)
  {
    effect_ptr = list_entry(effect_node_ptr, struct zynjacku_plugin, siblings_all);

    zynjacku_plugin_ui_run(effect_ptr);
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

const gchar *
zynjacku_rack_get_version()
{
  return VERSION;
}

const gchar *
zynjacku_rack_get_supported_feature(
  ZynjackuRack * rack_obj_ptr,
  guint index)
{
  struct lv2rack_engine * rack_ptr;

  if (index >= ZYNJACKU_RACK_ENGINE_FEATURES)
  {
    return NULL;
  }

  rack_ptr = ZYNJACKU_RACK_GET_PRIVATE(rack_obj_ptr);

  return rack_ptr->host_features[index]->URI;
}

#define effect_ptr (&plugin_ptr->subtype.effect)

gboolean
zynjacku_rack_construct_plugin(
  ZynjackuRack * rack_object_ptr,
  ZynjackuPlugin * plugin_object_ptr)
{
  static unsigned int id;
  char * port_name;
  size_t size_name;
  size_t size_id;
  struct lv2rack_engine * rack_ptr;
  struct list_head * node_ptr;
  struct zynjacku_port * port_ptr;
  struct zynjacku_port * audio_in_left_port_ptr;
  struct zynjacku_port * audio_in_right_port_ptr;
  struct zynjacku_port * audio_out_left_port_ptr;
  struct zynjacku_port * audio_out_right_port_ptr;
  struct zynjacku_plugin * plugin_ptr;

  rack_ptr = ZYNJACKU_RACK_GET_PRIVATE(rack_object_ptr);
  plugin_ptr = ZYNJACKU_PLUGIN_GET_PRIVATE(plugin_object_ptr);

  if (plugin_ptr->uri == NULL)
  {
    LOG_ERROR("\"uri\" property needs to be set before constructing plugin");
    goto fail;
  }

  if (plugin_ptr->name == NULL)
  {
    LOG_ERROR("\"name\" property needs to be set before constructing plugin");
    goto fail;
  }

  if (plugin_ptr->dlpath == NULL)
  {
    LOG_ERROR("Plugin %s has no dlpath set", plugin_ptr->uri);
    goto fail;
  }

  if (plugin_ptr->bundle_path == NULL)
  {
    LOG_ERROR("Plugin %s has no bundle path set", plugin_ptr->uri);
    goto fail;
  }

  audio_in_left_port_ptr = NULL;
  audio_in_right_port_ptr = NULL;

  list_for_each(node_ptr, &plugin_ptr->audio_ports)
  {
    port_ptr = list_entry(node_ptr, struct zynjacku_port, plugin_siblings);
    assert(port_ptr->type == PORT_TYPE_AUDIO);
    if (PORT_IS_INPUT(port_ptr))
    {
      if (audio_in_left_port_ptr == NULL)
      {
        audio_in_left_port_ptr = port_ptr;
        continue;
      }

      assert(audio_in_right_port_ptr == NULL);
      audio_in_right_port_ptr = port_ptr;
      break;
    }
  }

  if (audio_in_left_port_ptr == NULL)
  {
    LOG_ERROR("Cannot construct effect plugin without audio input port(s). %s", plugin_ptr->uri);
    goto fail;
  }

  audio_out_left_port_ptr = NULL;
  audio_out_right_port_ptr = NULL;

  list_for_each(node_ptr, &plugin_ptr->audio_ports)
  {
    port_ptr = list_entry(node_ptr, struct zynjacku_port, plugin_siblings);
    assert(port_ptr->type == PORT_TYPE_AUDIO);
    if (PORT_IS_OUTPUT(port_ptr))
    {
      if (audio_out_left_port_ptr == NULL)
      {
        audio_out_left_port_ptr = port_ptr;
        continue;
      }

      assert(audio_out_right_port_ptr == NULL);
      audio_out_right_port_ptr = port_ptr;
      break;
    }
  }

  if (audio_out_left_port_ptr == NULL)
  {
    LOG_ERROR("Cannot construct effect plugin without audio output port(s). %s", plugin_ptr->uri);
    goto fail;
  }

  rack_ptr->progress.context = rack_object_ptr;
  rack_ptr->progress_last_message = NULL;
  rack_ptr->progress_plugin_name = plugin_ptr->name;

  plugin_ptr->lv2plugin = zynjacku_lv2_load(
    plugin_ptr->uri,
    plugin_ptr->dlpath,
    plugin_ptr->bundle_path,
    zynjacku_rack_get_sample_rate(ZYNJACKU_RACK(rack_object_ptr)),
    rack_ptr->host_features);

  rack_ptr->progress.context = NULL;
  if (rack_ptr->progress_last_message != NULL)
  {
    free(rack_ptr->progress_last_message);
    rack_ptr->progress_last_message = NULL;
  }
  rack_ptr->progress_plugin_name = NULL;

  if (plugin_ptr->lv2plugin == NULL)
  {
    LOG_ERROR("Failed to load LV2 plugin %s", plugin_ptr->uri);
    goto fail;
  }

  /* connect parameter/measure ports */

  if (!zynjacku_connect_plugin_ports(
        plugin_ptr,
        plugin_object_ptr,
        G_OBJECT(rack_object_ptr)
#if HAVE_DYNPARAMS
        , &rack_ptr->mempool_allocator
#endif
        ))
  {
    goto fail_unload;
  }

  /* setup audio ports (they are connected in jack process callback */

  effect_ptr->audio_in_left_port_ptr = audio_in_left_port_ptr;
  effect_ptr->audio_in_right_port_ptr = audio_in_right_port_ptr;
  effect_ptr->audio_out_left_port_ptr = audio_out_left_port_ptr;
  effect_ptr->audio_out_right_port_ptr = audio_out_right_port_ptr;

  size_name = strlen(plugin_ptr->name);
  port_name = malloc(size_name + 1024);
  if (port_name == NULL)
  {
    LOG_ERROR("Failed to allocate memory for port name");
    goto fail_unload;
  }

  size_id = sprintf(port_name, "%u:", id);
  memcpy(port_name + size_id, plugin_ptr->name, size_name);

  if (audio_out_left_port_ptr != NULL &&
      audio_out_right_port_ptr != NULL)
  {
    assert(audio_out_left_port_ptr->type == PORT_TYPE_AUDIO);
    assert(PORT_IS_OUTPUT(audio_out_left_port_ptr));

    strcpy(port_name + size_id + size_name, " L");
    audio_out_left_port_ptr->data.audio = jack_port_register(rack_ptr->jack_client, port_name, JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);

    assert(audio_out_right_port_ptr->type == PORT_TYPE_AUDIO);
    assert(PORT_IS_OUTPUT(audio_out_right_port_ptr));

    strcpy(port_name + size_id + size_name, " R");
    audio_out_right_port_ptr->data.audio = jack_port_register(rack_ptr->jack_client, port_name, JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
  }
  else if (audio_out_left_port_ptr != NULL &&
           audio_out_right_port_ptr == NULL)
  {
    assert(audio_out_left_port_ptr->type == PORT_TYPE_AUDIO);
    assert(PORT_IS_OUTPUT(audio_out_left_port_ptr));

    port_name[size_id + size_name] = 0;
    audio_out_left_port_ptr->data.audio = jack_port_register(rack_ptr->jack_client, port_name, JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
  }

  port_name[size_id + size_name] = 0;
  plugin_ptr->id = port_name;

  id++;

  /* Activate plugin */
  zynjacku_lv2_activate(plugin_ptr->lv2plugin);

  plugin_ptr->recycle = false;

  list_add_tail(&plugin_ptr->siblings_all, &rack_ptr->plugins_all);

  pthread_mutex_lock(&rack_ptr->active_plugins_lock);
  list_add_tail(&plugin_ptr->siblings_active, &rack_ptr->plugins_pending_activation);
  pthread_mutex_unlock(&rack_ptr->active_plugins_lock);

  g_object_ref(plugin_ptr->engine_object_ptr);

  plugin_ptr->deactivate = zynjacku_rack_deactivate_effect;
  plugin_ptr->unregister_port = zynjacku_rack_unregister_port;
  plugin_ptr->get_required_features = zynjacku_rack_get_required_features;

  /* we dont support midi cc maps for lv2rack yet */
  plugin_ptr->set_midi_cc_map = NULL;
  plugin_ptr->midi_cc_map_cc_no_assign = NULL;

  LOG_DEBUG("Constructed plugin <%s>, gtk2gui <%p>", plugin_ptr->uri, plugin_ptr->gtk2gui);

  return true;

fail_unload:
  zynjacku_lv2_unload(plugin_ptr->lv2plugin);

fail:
  return false;
}

#undef effect_ptr
