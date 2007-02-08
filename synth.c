/* -*- Mode: C ; c-basic-offset: 2 -*- */
/*****************************************************************************
 *
 *   This file is part of zynjacku
 *
 *   Copyright (C) 2006 Nedko Arnaudov <nedko@arnaudov.name>
 *   Copyright (C) 2006 Dave Robillard <drobilla@connect.carleton.ca>
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

#include <stdio.h>
#include <string.h>
#include <slv2/slv2.h>
#include <slv2/query.h>
#include <jack/jack.h>
#include <jack/midiport.h>
#include <glib-object.h>

#include "lv2-miditype.h"
#include "list.h"
#define LOG_LEVEL LOG_LEVEL_ERROR
#include "log.h"
#include "lv2dynparam/lv2dynparam.h"
#include "lv2dynparam/host.h"

#include "synth.h"
#include "engine.h"

#include "zynjacku.h"

/* signals */
#define ZYNJACKU_SYNTH_SIGNAL_TEST                0
#define ZYNJACKU_SYNTH_SIGNAL_GROUP_APPEARED      1
#define ZYNJACKU_SYNTH_SIGNAL_BOOL_APPEARED       2
#define ZYNJACKU_SYNTH_SIGNAL_FLOAT_APPEARED      3
#define ZYNJACKU_SYNTH_SIGNAL_GROUP_DISAPPEARED   4
#define ZYNJACKU_SYNTH_SIGNAL_BOOL_DISAPPEARED    5
#define ZYNJACKU_SYNTH_SIGNAL_FLOAT_DISAPPEARED   6
#define ZYNJACKU_SYNTH_SIGNALS_COUNT              7

/* properties */
#define ZYNJACKU_SYNTH_PROP_URI                1

static guint g_zynjacku_synth_signals[ZYNJACKU_SYNTH_SIGNALS_COUNT];

/* UGLY: We convert dynparam context poitners to string to pass them
   as opaque types through Python. Silly, but codegen fails to create
   marshaling code for gpointer arguments. If possible at all, it is a
   hidden black magic. Other workaround ideas: GObject wrapper, GBoxed
   and GValue. */

gchar *
zynjacku_synth_context_to_string(
  void * void_context)
{
  /* we reuse this array because we call this function only from the UI thread,
     so there is no need to be thread safe */
  static gchar string_context[100];

  sprintf(string_context, "%p", void_context);

  LOG_DEBUG("Context %p converted to \"%s\"", void_context, string_context);

  return string_context;
}

void *
zynjacku_synth_context_from_string(
  gchar * string_context)
{
  void * void_context;

  if (sscanf(string_context, "%p", &void_context) != 1)
  {
    LOG_ERROR("Cannot convert string context \"%s\" to void pointer context", string_context);
    return NULL;
  }

  LOG_DEBUG("String context \"%s\" converted to %p", string_context, void_context);

  return void_context;
}

static void
zynjacku_synth_dispose(GObject * obj)
{
  struct zynjacku_synth * synth_ptr;

  synth_ptr = ZYNJACKU_SYNTH_GET_PRIVATE(obj);

  LOG_DEBUG("zynjacku_synth_dispose() called.");

  if (synth_ptr->dispose_has_run)
  {
    /* If dispose did already run, return. */
    LOG_DEBUG("zynjacku_synth_dispose() already run!");
    return;
  }

  /* Make sure dispose does not run twice. */
  synth_ptr->dispose_has_run = TRUE;

  /* 
   * In dispose, you are supposed to free all types referenced from this
   * object which might themselves hold a reference to self. Generally,
   * the most simple solution is to unref all members on which you own a 
   * reference.
   */
  if (synth_ptr->instance)
  {
    zynjacku_synth_destruct(ZYNJACKU_SYNTH(obj));
  }

  if (synth_ptr->uri != NULL)
  {
    g_free(synth_ptr->uri);
    synth_ptr->uri = NULL;
  }

  /* Chain up to the parent class */
  G_OBJECT_CLASS(g_type_class_peek_parent(G_OBJECT_GET_CLASS(obj)))->dispose(obj);
}

static void
zynjacku_synth_finalize(GObject * obj)
{
//  struct zynjacku_synth * self = ZYNJACKU_SYNTH_GET_PRIVATE(obj);

  LOG_DEBUG("zynjacku_synth_finalize() called.");

  /*
   * Here, complete object destruction.
   * You might not need to do much...
   */
  //g_free(self->private);

  /* Chain up to the parent class */
  G_OBJECT_CLASS(g_type_class_peek_parent(G_OBJECT_GET_CLASS(obj)))->finalize(obj);
}

static void
zynjacku_synth_set_property(
  GObject * object_ptr,
  guint property_id,
  const GValue * value_ptr,
  GParamSpec * param_spec_ptr)
{
  struct zynjacku_synth * synth_ptr;

  synth_ptr = ZYNJACKU_SYNTH_GET_PRIVATE(object_ptr);

  switch (property_id)
  {
  case ZYNJACKU_SYNTH_PROP_URI:
    //LOG_DEBUG("setting synth uri to: \"%s\"", g_value_get_string(value_ptr));
    //break;
    if (synth_ptr->uri != NULL)
    {
      g_free(synth_ptr->uri);
    }
    synth_ptr->uri = g_value_dup_string(value_ptr);
    LOG_DEBUG("synth uri set to: \"%s\"", synth_ptr->uri);
    break;
  default:
    /* We don't have any other property... */
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object_ptr, property_id, param_spec_ptr);
    break;
  }
}

static void
zynjacku_synth_get_property(
  GObject * object_ptr,
  guint property_id,
  GValue * value_ptr,
  GParamSpec * param_spec_ptr)
{
  struct zynjacku_synth * synth_ptr;

  synth_ptr = ZYNJACKU_SYNTH_GET_PRIVATE(object_ptr);

  switch (property_id)
  {
  case ZYNJACKU_SYNTH_PROP_URI:
    if (synth_ptr->uri != NULL)
    {
      g_value_set_string(value_ptr, synth_ptr->uri);
    }
    else
    {
      g_value_set_string(value_ptr, "");
    }
    break;
  default:
    /* We don't have any other property... */
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object_ptr, property_id, param_spec_ptr);
    break;
  }
}

static void
zynjacku_synth_class_init(
  gpointer class_ptr,
  gpointer class_data_ptr)
{
  GParamSpec * uri_param_spec;

  LOG_DEBUG("zynjacku_synth_class_init() called.");

  G_OBJECT_CLASS(class_ptr)->dispose = zynjacku_synth_dispose;
  G_OBJECT_CLASS(class_ptr)->finalize = zynjacku_synth_finalize;

  g_type_class_add_private(G_OBJECT_CLASS(class_ptr), sizeof(struct zynjacku_synth));

  g_zynjacku_synth_signals[ZYNJACKU_SYNTH_SIGNAL_TEST] =
    g_signal_new(
      "test",                   /* signal_name */
      ZYNJACKU_SYNTH_TYPE,      /* itype */
      G_SIGNAL_RUN_LAST |
      G_SIGNAL_ACTION,          /* signal_flags */
      0,                        /* class_offset */
      NULL,                     /* accumulator */
      NULL,                     /* accu_data */
      NULL,                     /* c_marshaller */
      G_TYPE_NONE,              /* return type */
      1,                        /* n_params */
      G_TYPE_OBJECT);

  g_zynjacku_synth_signals[ZYNJACKU_SYNTH_SIGNAL_GROUP_APPEARED] =
    g_signal_new(
      "group-appeared",         /* signal_name */
      ZYNJACKU_SYNTH_TYPE,      /* itype */
      G_SIGNAL_RUN_LAST |
      G_SIGNAL_ACTION,          /* signal_flags */
      0,                        /* class_offset */
      NULL,                     /* accumulator */
      NULL,                     /* accu_data */
      NULL,                     /* c_marshaller */
      G_TYPE_OBJECT,            /* return type */
      4,                        /* n_params */
      G_TYPE_OBJECT,            /* parent */
      G_TYPE_STRING,            /* group name */
      G_TYPE_STRING,            /* group type URI */
      G_TYPE_STRING);           /* context */

  g_zynjacku_synth_signals[ZYNJACKU_SYNTH_SIGNAL_BOOL_APPEARED] =
    g_signal_new(
      "bool-appeared",          /* signal_name */
      ZYNJACKU_SYNTH_TYPE,      /* itype */
      G_SIGNAL_RUN_LAST |
      G_SIGNAL_ACTION,          /* signal_flags */
      0,                        /* class_offset */
      NULL,                     /* accumulator */
      NULL,                     /* accu_data */
      NULL,                     /* c_marshaller */
      G_TYPE_OBJECT,            /* return type */
      4,                        /* n_params */
      G_TYPE_OBJECT,            /* parent */
      G_TYPE_STRING,            /* parameter name */
      G_TYPE_BOOLEAN,           /* value */
      G_TYPE_STRING);           /* context */

  g_zynjacku_synth_signals[ZYNJACKU_SYNTH_SIGNAL_FLOAT_APPEARED] =
    g_signal_new(
      "float-appeared",         /* signal_name */
      ZYNJACKU_SYNTH_TYPE,      /* itype */
      G_SIGNAL_RUN_LAST |
      G_SIGNAL_ACTION,          /* signal_flags */
      0,                        /* class_offset */
      NULL,                     /* accumulator */
      NULL,                     /* accu_data */
      NULL,                     /* c_marshaller */
      G_TYPE_OBJECT,            /* return type */
      6,                        /* n_params */
      G_TYPE_OBJECT,            /* parent */
      G_TYPE_STRING,            /* parameter name */
      G_TYPE_FLOAT,             /* value */
      G_TYPE_FLOAT,             /* min */
      G_TYPE_FLOAT,             /* max */
      G_TYPE_STRING);           /* context */

  g_zynjacku_synth_signals[ZYNJACKU_SYNTH_SIGNAL_GROUP_DISAPPEARED] =
    g_signal_new(
      "group-disappeared",      /* signal_name */
      ZYNJACKU_SYNTH_TYPE,      /* itype */
      G_SIGNAL_RUN_LAST |
      G_SIGNAL_ACTION,          /* signal_flags */
      0,                        /* class_offset */
      NULL,                     /* accumulator */
      NULL,                     /* accu_data */
      NULL,                     /* c_marshaller */
      G_TYPE_NONE,              /* return type */
      1,                        /* n_params */
      G_TYPE_OBJECT);           /* object */

  g_zynjacku_synth_signals[ZYNJACKU_SYNTH_SIGNAL_BOOL_DISAPPEARED] =
    g_signal_new(
      "bool-disappeared",       /* signal_name */
      ZYNJACKU_SYNTH_TYPE,      /* itype */
      G_SIGNAL_RUN_LAST |
      G_SIGNAL_ACTION,          /* signal_flags */
      0,                        /* class_offset */
      NULL,                     /* accumulator */
      NULL,                     /* accu_data */
      NULL,                     /* c_marshaller */
      G_TYPE_NONE,              /* return type */
      1,                        /* n_params */
      G_TYPE_OBJECT);           /* object */

  g_zynjacku_synth_signals[ZYNJACKU_SYNTH_SIGNAL_FLOAT_DISAPPEARED] =
    g_signal_new(
      "float-disappeared",      /* signal_name */
      ZYNJACKU_SYNTH_TYPE,      /* itype */
      G_SIGNAL_RUN_LAST |
      G_SIGNAL_ACTION,          /* signal_flags */
      0,                        /* class_offset */
      NULL,                     /* accumulator */
      NULL,                     /* accu_data */
      NULL,                     /* c_marshaller */
      G_TYPE_NONE,              /* return type */
      1,                        /* n_params */
      G_TYPE_OBJECT);           /* object */

  G_OBJECT_CLASS(class_ptr)->get_property = zynjacku_synth_get_property;
  G_OBJECT_CLASS(class_ptr)->set_property = zynjacku_synth_set_property;

  uri_param_spec = g_param_spec_string(
    "uri",
    "Synth LV2 URI construct property",
    "Synth LV2 URI construct property",
    "" /* default value */,
    G_PARAM_CONSTRUCT_ONLY |G_PARAM_READWRITE);

  g_object_class_install_property(
    G_OBJECT_CLASS(class_ptr),
    ZYNJACKU_SYNTH_PROP_URI,
    uri_param_spec);
}

static void
zynjacku_synth_init(
  GTypeInstance * instance,
  gpointer g_class)
{
  struct zynjacku_synth * synth_ptr;

  LOG_DEBUG("zynjacku_synth_init() called.");

  synth_ptr = ZYNJACKU_SYNTH_GET_PRIVATE(instance);

  INIT_LIST_HEAD(&synth_ptr->parameter_ports);
  synth_ptr->midi_in_port.type = PORT_TYPE_INVALID;
  synth_ptr->audio_out_left_port.type = PORT_TYPE_INVALID;
  synth_ptr->audio_out_right_port.type = PORT_TYPE_INVALID;

  synth_ptr->instance = NULL;
  synth_ptr->uri = NULL;
}

GType zynjacku_synth_get_type()
{
  static GType type = 0;
  if (type == 0)
  {
    type = g_type_register_static_simple(
      G_TYPE_OBJECT,
      "zynjacku_synth_type",
      sizeof(ZynjackuSynthClass),
      zynjacku_synth_class_init,
      sizeof(ZynjackuSynth),
      zynjacku_synth_init,
      0);
  }

  return type;
}

const char *
zynjacku_synth_get_name(
  ZynjackuSynth * obj_ptr)
{
  struct zynjacku_synth * synth_ptr;

  synth_ptr = ZYNJACKU_SYNTH_GET_PRIVATE(obj_ptr);

  return synth_ptr->id;
}

const char *
zynjacku_synth_get_class_name(
  ZynjackuSynth * obj_ptr)
{
  struct zynjacku_synth * synth_ptr;

  synth_ptr = ZYNJACKU_SYNTH_GET_PRIVATE(obj_ptr);

  return slv2_plugin_get_name(synth_ptr->plugin);
}

const char *
zynjacku_synth_get_class_uri(
  ZynjackuSynth * obj_ptr)
{
  struct zynjacku_synth * synth_ptr;

  synth_ptr = ZYNJACKU_SYNTH_GET_PRIVATE(obj_ptr);

  return slv2_plugin_get_uri(synth_ptr->plugin);
}

gboolean
zynjacku_synth_supports_generic_ui(
  ZynjackuSynth * synth_obj_ptr)
{
  struct zynjacku_synth * synth_ptr;

  LOG_DEBUG("zynjacku_synth_supports_generic_ui() called.");

  synth_ptr = ZYNJACKU_SYNTH_GET_PRIVATE(synth_obj_ptr);

  /* generic ui can be supported without dynparams but we don't do it atm */

/*   LOG_DEBUG("%p", synth_ptr->dynparams); */
  return (synth_ptr->dynparams != NULL) ? TRUE : FALSE;
}

void
zynjacku_synth_ui_on(
  ZynjackuSynth * synth_obj_ptr)
{
  struct zynjacku_synth * synth_ptr;

  LOG_DEBUG("zynjacku_synth_ui_on() called.");

  synth_ptr = ZYNJACKU_SYNTH_GET_PRIVATE(synth_obj_ptr);

  if (synth_ptr->dynparams)
  {
    lv2dynparam_host_ui_on(synth_ptr->dynparams);
  }
}

void
zynjacku_synth_ui_off(
  ZynjackuSynth * obj_ptr)
{
  struct zynjacku_synth * synth_ptr;

  LOG_DEBUG("zynjacku_synth_ui_off() called.");

  synth_ptr = ZYNJACKU_SYNTH_GET_PRIVATE(obj_ptr);

  if (synth_ptr->dynparams)
  {
    lv2dynparam_host_ui_off(synth_ptr->dynparams);
  }
}

gboolean
create_port(
  struct zynjacku_engine * engine_ptr,
  struct zynjacku_synth * plugin_ptr,
  uint32_t port_index)
{
  enum SLV2PortClass class;
  char * type;
  char * symbol;
  struct zynjacku_synth_port * port_ptr;
  gboolean ret;

  /* Get the 'class' of the port (control input, audio output, etc) */
  class = slv2_port_get_class(plugin_ptr->plugin, slv2_port_by_index(port_index));

  /* Get the port symbol (label) for console printing */
  symbol = slv2_port_get_symbol(plugin_ptr->plugin, slv2_port_by_index(port_index));

  if (class == SLV2_CONTROL_RATE_INPUT)
  {
    port_ptr = malloc(sizeof(struct zynjacku_synth_port));
    port_ptr->type = PORT_TYPE_PARAMETER;
    port_ptr->index = port_index;
    port_ptr->data.parameter = slv2_port_get_default_value(plugin_ptr->plugin, slv2_port_by_index(port_index));
    slv2_instance_connect_port(plugin_ptr->instance, port_index, &port_ptr->data.parameter);
    LOG_INFO("Set %s to %f", symbol, port_ptr->data.parameter);
    list_add_tail(&port_ptr->plugin_siblings, &plugin_ptr->parameter_ports);
  }
  else if (class == SLV2_AUDIO_RATE_OUTPUT)
  {
    if (plugin_ptr->audio_out_left_port.type == PORT_TYPE_INVALID)
    {
      port_ptr = &plugin_ptr->audio_out_left_port;
    }
    else if (plugin_ptr->audio_out_right_port.type == PORT_TYPE_INVALID)
    {
      port_ptr = &plugin_ptr->audio_out_right_port;
    }
    else
    {
      LOG_ERROR("Maximum two audio output ports are supported.");
      goto fail;
    }

    port_ptr->type = PORT_TYPE_AUDIO;
    port_ptr->index = port_index;
  }
  else if (class == SLV2_AUDIO_RATE_INPUT)
  {
    LOG_ERROR("audio input ports are not supported.");
    goto fail;
  }
  else if (class == SLV2_CONTROL_RATE_OUTPUT)
  {
    LOG_ERROR("control rate float output ports are not supported.");
    goto fail;
  }
  else if (class == SLV2_MIDI_INPUT)
  {
    if (plugin_ptr->midi_in_port.type == PORT_TYPE_INVALID)
    {
      port_ptr = &plugin_ptr->midi_in_port;
    }
    else
    {
      LOG_ERROR("maximum one midi input port is supported.");
      goto fail;
    }

    port_ptr->type = PORT_TYPE_MIDI;
    port_ptr->index = port_index;
    slv2_instance_connect_port(plugin_ptr->instance, port_index, &engine_ptr->lv2_midi_buffer);
    list_add_tail(&port_ptr->port_type_siblings, &engine_ptr->midi_ports);
  }
  else if (class == SLV2_MIDI_OUTPUT)
  {
    LOG_ERROR("midi output ports are not supported.");
    goto fail;
  }
  else
  {
    LOG_ERROR("unrecognized port.");
    goto fail;
  }

  ret = TRUE;
  goto exit;
fail:
  ret = FALSE;
exit:
  free(type);
  free(symbol);

  return ret;
}

void
zynjacku_synth_free_ports(
  struct zynjacku_engine * engine_ptr,
  struct zynjacku_synth * plugin_ptr)
{
  struct list_head * node_ptr;
  struct zynjacku_synth_port * port_ptr;

  while (!list_empty(&plugin_ptr->parameter_ports))
  {
    node_ptr = plugin_ptr->parameter_ports.next;
    port_ptr = list_entry(node_ptr, struct zynjacku_synth_port, plugin_siblings);

    assert(port_ptr->type == PORT_TYPE_PARAMETER);

    list_del(node_ptr);
    free(port_ptr);
  }

  if (plugin_ptr->audio_out_left_port.type == PORT_TYPE_AUDIO)
  {
    jack_port_unregister(engine_ptr->jack_client, plugin_ptr->audio_out_left_port.data.audio);
    list_del(&plugin_ptr->audio_out_left_port.port_type_siblings);
  }

  if (plugin_ptr->audio_out_right_port.type == PORT_TYPE_AUDIO) /* stereo? */
  {
    assert(plugin_ptr->audio_out_right_port.type == PORT_TYPE_AUDIO);
    jack_port_unregister(engine_ptr->jack_client, plugin_ptr->audio_out_right_port.data.audio);
    list_del(&plugin_ptr->audio_out_right_port.port_type_siblings);
  }

  if (plugin_ptr->midi_in_port.type == PORT_TYPE_MIDI)
  {
    list_del(&plugin_ptr->midi_in_port.port_type_siblings);
  }
}

gboolean
zynjacku_synth_construct(
  ZynjackuSynth * synth_obj_ptr,
  GObject * engine_object_ptr)
{
  uint32_t ports_count;
  uint32_t i;
  static unsigned int id;
  char * name;
  char * port_name;
  size_t size_name;
  size_t size_id;
  struct zynjacku_synth * synth_ptr;
  guint sample_rate;
  struct zynjacku_engine * engine_ptr;
  SLV2Value slv2_value;
  size_t value_index;
  gboolean dynparams_supported;

  synth_ptr = ZYNJACKU_SYNTH_GET_PRIVATE(synth_obj_ptr);
  engine_ptr = ZYNJACKU_ENGINE_GET_PRIVATE(engine_object_ptr);

  if (synth_ptr->uri == NULL)
  {
    LOG_ERROR("\"uri\" property needs to be set before constructing plugin");
    goto fail;
  }

  synth_ptr->plugin = zynjacku_plugin_repo_lookup_by_uri(synth_ptr->uri);
  if (synth_ptr->plugin == NULL)
  {
    LOG_ERROR("Failed to find plugin <%s>", synth_ptr->uri);
    goto fail;
  }

  dynparams_supported = FALSE;

  slv2_value = slv2_plugin_get_required_features(synth_ptr->plugin);

  LOG_DEBUG("Plugin has %u required features", (unsigned int)slv2_value->num_values);
  for (value_index = 0 ; value_index < slv2_value->num_values ; value_index++)
  {
    LOG_DEBUG("\"%s\"", slv2_value->values[value_index]);
    if (strcmp(LV2DYNPARAM_URI, slv2_value->values[value_index]) == 0)
    {
      dynparams_supported = TRUE;
    }
    else
    {
      LOG_DEBUG("Plugin requires unsupported feature \"%s\"", slv2_value->values[value_index]);
      slv2_value_free(slv2_value);
      goto fail;
    }
  }

  slv2_value_free(slv2_value);

  slv2_value = slv2_plugin_get_optional_features(synth_ptr->plugin);

  LOG_NOTICE("Plugin has %u optional features", (unsigned int)slv2_value->num_values);
  for (value_index = 0 ; value_index < slv2_value->num_values ; value_index++)
  {
    LOG_NOTICE("\"%s\"", slv2_value->values[value_index]);
    if (strcmp(LV2DYNPARAM_URI, slv2_value->values[value_index]) == 0)
    {
      dynparams_supported = TRUE;
    }
  }

  slv2_value_free(slv2_value);

  sample_rate = zynjacku_engine_get_sample_rate(ZYNJACKU_ENGINE(engine_object_ptr));

  /* Instantiate the plugin */
  synth_ptr->instance = slv2_plugin_instantiate(synth_ptr->plugin, sample_rate, NULL);
  if (synth_ptr->instance == NULL)
  {
    LOG_ERROR("Failed to instantiate plugin.");
    goto fail;
  }

  if (dynparams_supported)
  {
    if (!lv2dynparam_host_add_synth(
          slv2_instance_get_descriptor(synth_ptr->instance),
          slv2_instance_get_handle(synth_ptr->instance),
          synth_obj_ptr,
          &synth_ptr->dynparams))
    {
      LOG_ERROR("Failed to instantiate dynparams extension.");
      goto fail_instance_free;
    }
  }
  else
  {
    synth_ptr->dynparams = NULL;
  }

  /* Create ports */
  ports_count  = slv2_plugin_get_num_ports(synth_ptr->plugin);

  for (i = 0 ; i < ports_count ; i++)
  {
    if (!create_port(engine_ptr, synth_ptr, i))
    {
      LOG_ERROR("Failed to create plugin port");
      goto fail_free_ports;
    }
  }

  name = slv2_plugin_get_name(synth_ptr->plugin);
  if (name == NULL)
  {
    LOG_ERROR("Failed to get plugin name");
    goto fail_free_ports;
  }

  size_name = strlen(name);
  port_name = malloc(size_name + 1024);
  if (port_name == NULL)
  {
    free(name);
    LOG_ERROR("Failed to allocate memory for port name");
    goto fail_free_ports;
  }

  size_id = sprintf(port_name, "%u:", id);
  memcpy(port_name + size_id, name, size_name);

  if (synth_ptr->audio_out_left_port.type == PORT_TYPE_AUDIO &&
      synth_ptr->audio_out_right_port.type == PORT_TYPE_AUDIO)
  {
    sprintf(port_name + size_id + size_name, " L");
    synth_ptr->audio_out_left_port.data.audio = jack_port_register(engine_ptr->jack_client, port_name, JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
    list_add_tail(&synth_ptr->audio_out_left_port.port_type_siblings, &engine_ptr->audio_ports);

    sprintf(port_name + size_id + size_name, " R");
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
  synth_ptr->id = port_name;

  free(name);

  id++;

  zynjacku_engine_activate_synth(ZYNJACKU_ENGINE(engine_object_ptr), G_OBJECT(synth_obj_ptr));

  synth_ptr->engine_object_ptr = engine_object_ptr;
  g_object_ref(synth_ptr->engine_object_ptr);

  LOG_DEBUG("Constructed synth <%s>", slv2_plugin_get_uri(synth_ptr->plugin));

  return TRUE;

fail_free_ports:
  zynjacku_synth_free_ports(engine_ptr, synth_ptr);

fail_instance_free:
  slv2_instance_free(synth_ptr->instance);
  synth_ptr->instance = NULL;

fail:
  return FALSE;
}

void
zynjacku_synth_destruct(
  ZynjackuSynth * synth_obj_ptr)
{
  struct zynjacku_synth * synth_ptr;
  struct zynjacku_engine * engine_ptr;

  synth_ptr = ZYNJACKU_SYNTH_GET_PRIVATE(synth_obj_ptr);
  engine_ptr = ZYNJACKU_ENGINE_GET_PRIVATE(synth_ptr->engine_object_ptr);

  LOG_DEBUG("Destructing plugin <%s>", slv2_plugin_get_uri(synth_ptr->plugin));

  zynjacku_engine_deactivate_synth(ZYNJACKU_ENGINE(synth_ptr->engine_object_ptr), G_OBJECT(synth_obj_ptr));

  slv2_instance_free(synth_ptr->instance);

  zynjacku_synth_free_ports(engine_ptr, synth_ptr);

  g_object_unref(synth_ptr->engine_object_ptr);

  synth_ptr->instance = NULL;
}

void
dynparam_group_appeared(
  lv2dynparam_host_group group_handle,
  void * instance_ui_context,
  void * parent_group_ui_context,
  const char * group_name,
  const char * group_type_uri,
  void ** group_ui_context)
{
  struct zynjacku_synth * synth_ptr;
  GObject * ret_obj_ptr;

  synth_ptr = ZYNJACKU_SYNTH_GET_PRIVATE((ZynjackuSynth *)instance_ui_context);

  LOG_DEBUG("Group \"%s\" if type \"%s\" appeared, handle %p", group_name, group_type_uri, group_handle);

  g_signal_emit(
    (ZynjackuSynth *)instance_ui_context,
    g_zynjacku_synth_signals[ZYNJACKU_SYNTH_SIGNAL_GROUP_APPEARED],
    0,
    parent_group_ui_context,
    group_name,
    group_type_uri,
    zynjacku_synth_context_to_string(group_handle),
    &ret_obj_ptr);

  LOG_DEBUG("group-appeared signal returned object ptr is %p", ret_obj_ptr);

  *group_ui_context = ret_obj_ptr;
}

void
dynparam_group_disappeared(
  void * instance_ui_context,
  void * parent_group_ui_context,
  void * group_ui_context)
{
  LOG_DEBUG("dynparam_generic_group_disappeared() called.");

  g_signal_emit(
    (ZynjackuSynth *)instance_ui_context,
    g_zynjacku_synth_signals[ZYNJACKU_SYNTH_SIGNAL_GROUP_DISAPPEARED],
    0,
    group_ui_context);

  g_object_unref(group_ui_context);
}

void
dynparam_parameter_boolean_disappeared(
  void * instance_ui_context,
  void * parent_group_ui_context,
  void * parameter_ui_context)
{
  LOG_DEBUG("dynparam_parameter_boolean_disappeared() called.");

  g_signal_emit(
    (ZynjackuSynth *)instance_ui_context,
    g_zynjacku_synth_signals[ZYNJACKU_SYNTH_SIGNAL_BOOL_DISAPPEARED],
    0,
    parameter_ui_context);

  g_object_unref(parameter_ui_context);
}

void
dynparam_parameter_float_disappeared(
  void * instance_ui_context,
  void * parent_group_ui_context,
  void * parameter_ui_context)
{
  LOG_DEBUG("dynparam_parameter_float_disappeared() called.");

  g_signal_emit(
    (ZynjackuSynth *)instance_ui_context,
    g_zynjacku_synth_signals[ZYNJACKU_SYNTH_SIGNAL_FLOAT_DISAPPEARED],
    0,
    parameter_ui_context);

  g_object_unref(parameter_ui_context);
}

void
dynparam_parameter_boolean_appeared(
  lv2dynparam_host_parameter parameter_handle,
  void * instance_ui_context,
  void * group_ui_context,
  const char * parameter_name,
  BOOL value,
  void ** parameter_ui_context)
{
  GObject * ret_obj_ptr;

  LOG_DEBUG(
    "Boolean parameter \"%s\" appeared, value %s, handle %p",
    parameter_name,
    value ? "TRUE" : "FALSE",
    parameter_handle);

  g_signal_emit(
    (ZynjackuSynth *)instance_ui_context,
    g_zynjacku_synth_signals[ZYNJACKU_SYNTH_SIGNAL_BOOL_APPEARED],
    0,
    group_ui_context,
    parameter_name,
    (gboolean)value,
    zynjacku_synth_context_to_string(parameter_handle),
    &ret_obj_ptr);

  LOG_DEBUG("bool-appeared signal returned object ptr is %p", ret_obj_ptr);

  *parameter_ui_context = ret_obj_ptr;
}

void
zynjacku_synth_bool_set(
  ZynjackuSynth * synth_obj_ptr,
  gchar * string_context,
  gboolean value)
{
  void * context;
  struct zynjacku_synth * synth_ptr;

  synth_ptr = ZYNJACKU_SYNTH_GET_PRIVATE(synth_obj_ptr);

  context = zynjacku_synth_context_from_string(string_context);

  LOG_DEBUG("zynjacku_synth_bool_set() called, context %p", context);

  dynparam_parameter_boolean_change(
    synth_ptr->dynparams,
    (lv2dynparam_host_parameter)context,
    value);
}

void
dynparam_parameter_float_appeared(
  lv2dynparam_host_parameter parameter_handle,
  void * instance_ui_context,
  void * group_ui_context,
  const char * parameter_name,
  float value,
  float min,
  float max,
  void ** parameter_ui_context)
{
  GObject * ret_obj_ptr;

  LOG_DEBUG(
    "Float parameter \"%s\" appeared, value %f, min %f, max %f, handle %p",
    parameter_name,
    value,
    min,
    max,
    parameter_handle);

  g_signal_emit(
    (ZynjackuSynth *)instance_ui_context,
    g_zynjacku_synth_signals[ZYNJACKU_SYNTH_SIGNAL_FLOAT_APPEARED],
    0,
    group_ui_context,
    parameter_name,
    (gfloat)value,
    (gfloat)min,
    (gfloat)max,
    zynjacku_synth_context_to_string(parameter_handle),
    &ret_obj_ptr);

  LOG_DEBUG("float-appeared signal returned object ptr is %p", ret_obj_ptr);

  *parameter_ui_context = ret_obj_ptr;
}

void
zynjacku_synth_float_set(
  ZynjackuSynth * synth_obj_ptr,
  gchar * string_context,
  gfloat value)
{
  void * context;
  struct zynjacku_synth * synth_ptr;

  synth_ptr = ZYNJACKU_SYNTH_GET_PRIVATE(synth_obj_ptr);

  context = zynjacku_synth_context_from_string(string_context);

  LOG_DEBUG("zynjacku_synth_float_set() called, context %p", context);

  dynparam_parameter_float_change(
    synth_ptr->dynparams,
    (lv2dynparam_host_parameter)context,
    value);
}
