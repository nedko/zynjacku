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
#include <glib-object.h>
#include <lv2dynparam/lv2dynparam.h>
#include <lv2dynparam/lv2_rtmempool.h>
#include <lv2dynparam/host.h>

#include "config.h"

#include "lv2_contexts.h"
#include "lv2_event.h"
#include "lv2_uri_map.h"
#include "lv2_string_port.h"

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

#include "rtmempool.h"
#include "plugin_repo.h"
#include "lv2_event_helpers.h"

#define ZYNJACKU_RACK_ENGINE_FEATURES 5

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

  struct lv2_rtsafe_memory_pool_provider mempool_allocator;
  LV2_URI_Map_Feature uri_map;
  LV2_Event_Feature event;

  LV2_Feature host_feature_rtmempool;
  LV2_Feature host_feature_dynparams;
  LV2_Feature host_feature_contexts;
  LV2_Feature host_feature_msgcontext;
  LV2_Feature host_feature_stringport;
  const LV2_Feature * host_features[ZYNJACKU_RACK_ENGINE_FEATURES + 1];
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
      4,                        /* n_params */
      G_TYPE_STRING,            /* plugin name */
      G_TYPE_STRING,            /* plugin uri */
      G_TYPE_STRING,            /* plugin license */
      G_TYPE_STRING);           /* plugin author */
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

  /* initialize rtsafe mempool host feature */
  rtmempool_allocator_init(&rack_ptr->mempool_allocator);

  rack_ptr->host_feature_rtmempool.URI = LV2_RTSAFE_MEMORY_POOL_URI;
  rack_ptr->host_feature_rtmempool.data = &rack_ptr->mempool_allocator;

  rack_ptr->host_feature_dynparams.URI = LV2DYNPARAM_URI;
  rack_ptr->host_feature_dynparams.data = NULL;

  rack_ptr->host_feature_contexts.URI = LV2_CONTEXTS_URI;
  rack_ptr->host_feature_contexts.data = NULL;

  rack_ptr->host_feature_msgcontext.URI = LV2_CONTEXT_MESSAGE;
  rack_ptr->host_feature_msgcontext.data = NULL;

  rack_ptr->host_feature_stringport.URI = LV2_STRING_PORT_URI;
  rack_ptr->host_feature_stringport.data = NULL;

  /* initialize host features array */
  count = 0;
  rack_ptr->host_features[count++] = &rack_ptr->host_feature_rtmempool;
  rack_ptr->host_features[count++] = &rack_ptr->host_feature_dynparams;
  rack_ptr->host_features[count++] = &rack_ptr->host_feature_contexts;
  rack_ptr->host_features[count++] = &rack_ptr->host_feature_msgcontext;
  rack_ptr->host_features[count++] = &rack_ptr->host_feature_stringport;
  assert(ZYNJACKU_RACK_ENGINE_FEATURES == count);
  rack_ptr->host_features[count] = NULL;

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
    struct zynjacku_rt_plugin_command * cmd;

    effect_ptr = list_entry(effect_node_ptr, struct zynjacku_plugin, siblings_active);

    if (effect_ptr->recycle)
    {
      list_del(effect_node_ptr);
      effect_ptr->recycle = false;
      continue;
    }

    cmd = effect_ptr->command;

    /* Execute the command */
    if (cmd)
    {
      assert(!effect_ptr->command_result);
      assert(!(cmd->port->flags & PORT_FLAGS_MSGCONTEXT));
      zynjacku_lv2_connect_port(effect_ptr->lv2plugin, cmd->port, cmd->data);
    }

    if (effect_ptr->dynparams)
    {
      lv2dynparam_host_realtime_run(effect_ptr->dynparams);
    }

    /* Connect plugin LV2 input audio ports */
    zynjacku_lv2_connect_port(
      effect_ptr->lv2plugin,
      &effect_ptr->subtype.effect.audio_in_left_port,
      left);

    if (effect_ptr->subtype.effect.audio_in_right_port.type == PORT_TYPE_AUDIO)
    {
      zynjacku_lv2_connect_port(
        effect_ptr->lv2plugin,
        &effect_ptr->subtype.effect.audio_in_right_port,
        mono ? left : right);
    }

    /* Connect plugin LV2 output audio ports directly to JACK buffers */
    left = jack_port_get_buffer(effect_ptr->subtype.effect.audio_out_left_port.data.audio, nframes);
    zynjacku_lv2_connect_port(
      effect_ptr->lv2plugin,
      &effect_ptr->subtype.effect.audio_out_left_port,
      left);

    mono = effect_ptr->subtype.effect.audio_out_right_port.type != PORT_TYPE_AUDIO;
    if (!mono)
    {
      right = jack_port_get_buffer(effect_ptr->subtype.effect.audio_out_right_port.data.audio, nframes);
      zynjacku_lv2_connect_port(
        effect_ptr->lv2plugin,
        &effect_ptr->subtype.effect.audio_out_right_port,
        right);
    }

    /* Run plugin for this cycle */
    zynjacku_lv2_run(effect_ptr->lv2plugin, nframes);
    
    /* Acknowledge the command */
    if (cmd)
    {
      if (cmd->port->flags & PORT_FLAGS_IS_STRING)
        ((LV2_String_Data *)(cmd->data))->flags &= ~LV2_STRING_DATA_CHANGED_FLAG;
      effect_ptr->command = NULL;
      effect_ptr->command_result = cmd;
    }
  }

  return 0;
}

#undef rack_ptr

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

#define rack_obj_ptr ((ZynjackuRack *)context)

bool
zynjacku_rack_check_plugin(
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
  if (audio_in_ports_count == 0 || audio_out_ports_count == 0)
  {
    LOG_DEBUG("Skipping 's' %s, [effect] plugin with unsupported port configuration", name, plugin_uri);
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

  LOG_DEBUG("Found effect plugin '%s' %s", name, plugin_uri);
  LOG_DEBUG("  midi input ports: %d", (unsigned int)midi_in_ports_count);
  LOG_DEBUG("  control ports: %d", (unsigned int)control_ports_count);
  LOG_DEBUG("  string ports: %d", (unsigned int)string_ports_count);
  LOG_DEBUG("  event ports: %d", (unsigned int)event_ports_count);
  LOG_DEBUG("  event midi input ports: %d", (unsigned int)midi_event_in_ports_count);
  LOG_DEBUG("  audio input ports: %d", (unsigned int)audio_in_ports_count);
  LOG_DEBUG("  audio output ports: %d", (unsigned int)audio_out_ports_count);
  LOG_DEBUG("  total ports %d", (unsigned int)ports_count);
  return true;
}

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
  const char * author;

  name = zynjacku_plugin_repo_get_name(uri);
  license = zynjacku_plugin_repo_get_license(uri);
  author = zynjacku_plugin_repo_get_author(uri);

  g_signal_emit(
    rack_obj_ptr,
    g_zynjacku_rack_signals[ZYNJACKU_RACK_SIGNAL_TACK],
    0,
    name,
    uri,
    license,
    author);
}

#undef rack_obj_ptr

void
zynjacku_rack_iterate_plugins(
  ZynjackuRack * rack_obj_ptr,
  gboolean force)
{
  struct lv2rack_engine * rack_ptr;

  rack_ptr = ZYNJACKU_RACK_GET_PRIVATE(rack_obj_ptr);

  zynjacku_plugin_repo_iterate(
    force,
    rack_ptr->host_features,
    rack_obj_ptr,
    zynjacku_rack_check_plugin,
    zynjacku_rack_tick,
    zynjacku_rack_tack);
}

void
zynjacku_free_effect_ports(
  GObject * plugin_object_ptr)
{
  struct lv2rack_engine * rack_ptr;
  struct zynjacku_plugin * plugin_ptr;

  plugin_ptr = ZYNJACKU_PLUGIN_GET_PRIVATE(plugin_object_ptr);
  rack_ptr = ZYNJACKU_RACK_GET_PRIVATE(plugin_ptr->engine_object_ptr);

  LOG_DEBUG("zynjacku_free_effect_ports() called");

  zynjacku_free_plugin_ports(plugin_ptr);

  if (plugin_ptr->type == PLUGIN_TYPE_EFFECT)
  {
    if (plugin_ptr->subtype.effect.audio_out_left_port.type == PORT_TYPE_AUDIO)
    {
      jack_port_unregister(rack_ptr->jack_client, plugin_ptr->subtype.effect.audio_out_left_port.data.audio);
    }

    if (plugin_ptr->subtype.effect.audio_out_right_port.type == PORT_TYPE_AUDIO) /* stereo? */
    {
      assert(plugin_ptr->subtype.effect.audio_out_left_port.type == PORT_TYPE_AUDIO);
      jack_port_unregister(rack_ptr->jack_client, plugin_ptr->subtype.effect.audio_out_right_port.data.audio);
    }
  }
}

#define effect_ptr (&((struct zynjacku_plugin *)context)->subtype.effect)

bool
zynjacku_effect_create_port(
  void * context,
  unsigned int port_type,
  bool output,
  uint32_t port_index)
{
  struct zynjacku_port * port_ptr;

  LOG_NOTICE("creating effect %s port of type %u, index %u", output ? "output" : "input", (unsigned int)port_type, (unsigned int)port_index);

  if (port_type != PORT_TYPE_AUDIO)
  {
    /* ignore unknown ports */
    return true;
  }

  if (!output)
  {
    if (effect_ptr->audio_in_left_port.type == PORT_TYPE_INVALID)
    {
      port_ptr = &effect_ptr->audio_in_left_port;
    }
    else if (effect_ptr->audio_in_right_port.type == PORT_TYPE_INVALID)
    {
      port_ptr = &effect_ptr->audio_in_right_port;
    }
    else
    {
      /* ignore, we dont support more than two audio ports yet */
      return true;
    }
  }
  else
  {
    if (effect_ptr->audio_out_left_port.type == PORT_TYPE_INVALID)
    {
      port_ptr = &effect_ptr->audio_out_left_port;
    }
    else if (effect_ptr->audio_out_right_port.type == PORT_TYPE_INVALID)
    {
      port_ptr = &effect_ptr->audio_out_right_port;
    }
    else
    {
      /* ignore, we dont support more than two audio ports yet */
      return true;
    }
  }

  port_ptr->type = PORT_TYPE_AUDIO;
  port_ptr->index = port_index;

  return true;
}

#undef effect_ptr
#define effect_ptr (&plugin_ptr->subtype.effect)

bool
zynjacku_plugin_construct_effect(
  struct zynjacku_plugin * plugin_ptr,
  ZynjackuPlugin * plugin_obj_ptr,
  GObject * rack_object_ptr)
{
  static unsigned int id;
  char * port_name;
  size_t size_name;
  size_t size_id;
  struct list_head * node_ptr;
  struct zynjacku_port * port_ptr;
  struct lv2rack_engine * rack_ptr;

  rack_ptr = ZYNJACKU_RACK_GET_PRIVATE(rack_object_ptr);

  plugin_ptr->type = PLUGIN_TYPE_EFFECT;
  effect_ptr->audio_in_left_port.type = PORT_TYPE_INVALID;
  effect_ptr->audio_in_right_port.type = PORT_TYPE_INVALID;
  effect_ptr->audio_out_left_port.type = PORT_TYPE_INVALID;
  effect_ptr->audio_out_right_port.type = PORT_TYPE_INVALID;

  if (!zynjacku_plugin_repo_load_plugin(plugin_ptr, plugin_ptr, zynjacku_effect_create_port, zynjacku_rack_check_plugin, rack_ptr->host_features))
  {
    LOG_ERROR("Failed to load LV2 info for plugin %s", plugin_ptr->uri);
    goto fail;
  }

  plugin_ptr->lv2plugin = zynjacku_lv2_load(
    plugin_ptr->uri,
    zynjacku_rack_get_sample_rate(ZYNJACKU_RACK(rack_object_ptr)),
    rack_ptr->host_features);
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
          &rack_ptr->mempool_allocator,
          plugin_obj_ptr,
          zynjacku_plugin_dynparam_parameter_created,
          zynjacku_plugin_dynparam_parameter_destroying,
          zynjacku_plugin_dynparam_parameter_value_change_context,
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

  plugin_ptr->engine_object_ptr = rack_object_ptr;

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

  if (effect_ptr->audio_out_left_port.type == PORT_TYPE_AUDIO &&
      effect_ptr->audio_out_right_port.type == PORT_TYPE_AUDIO)
  {
    strcpy(port_name + size_id + size_name, " L");
    effect_ptr->audio_out_left_port.data.audio = jack_port_register(rack_ptr->jack_client, port_name, JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);

    strcpy(port_name + size_id + size_name, " R");
    effect_ptr->audio_out_right_port.data.audio = jack_port_register(rack_ptr->jack_client, port_name, JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
  }
  else if (effect_ptr->audio_out_left_port.type == PORT_TYPE_AUDIO &&
           effect_ptr->audio_out_right_port.type == PORT_TYPE_INVALID)
  {
    port_name[size_id + size_name] = 0;
    effect_ptr->audio_out_left_port.data.audio = jack_port_register(rack_ptr->jack_client, port_name, JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
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

  /* no plugins to test gtk2gui */
  plugin_ptr->gtk2gui = zynjacku_gtk2gui_create(rack_ptr->host_features, ZYNJACKU_RACK_ENGINE_FEATURES, plugin_ptr->lv2plugin, 
    plugin_ptr, plugin_obj_ptr, plugin_ptr->uri, plugin_ptr->id, &plugin_ptr->parameter_ports);

  plugin_ptr->deactivate = zynjacku_rack_deactivate_effect;
  plugin_ptr->free_ports = zynjacku_free_effect_ports;

  /* we dont support midi cc maps for lv2rack yet */
  plugin_ptr->set_midi_cc_map = NULL;
  plugin_ptr->midi_cc_map_cc_no_assign = NULL;

  LOG_DEBUG("Constructed plugin <%s>, gtk2gui <%p>", plugin_ptr->uri, plugin_ptr->gtk2gui);

  return true;

fail_free_ports:
  zynjacku_free_effect_ports(G_OBJECT(plugin_obj_ptr));
  plugin_ptr->engine_object_ptr = NULL;

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

#undef effect_ptr
