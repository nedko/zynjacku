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
#include "slv2.h"
#define LOG_LEVEL LOG_LEVEL_DEBUG
#include "log.h"
#include "dynparam.h"

#include "synth.h"
#include "engine.h"

#include "zynjacku.h"

/* signals */
#define ZYNJACKU_SYNTH_SIGNAL_GROUP_ADDED      0
#define ZYNJACKU_SYNTH_SIGNALS_COUNT           1

/* properties */
#define ZYNJACKU_SYNTH_PROP_URI                1

static guint g_zynjacku_synth_signals[ZYNJACKU_SYNTH_SIGNALS_COUNT];

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

  g_zynjacku_synth_signals[ZYNJACKU_SYNTH_SIGNAL_GROUP_ADDED] =
    g_signal_new(
      "group-added",            /* signal_name */
      ZYNJACKU_SYNTH_TYPE,      /* itype */
      G_SIGNAL_RUN_FIRST |
      G_SIGNAL_ACTION,          /* signal_flags */
      0,                        /* class_offset */
      NULL,                     /* accumulator */
      NULL,                     /* accu_data */
      NULL,                     /* c_marshaller */
      G_TYPE_NONE,              /* return type */
      1,                        /* n_params */
      G_TYPE_STRING);

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

void
zynjacku_synth_ui_on(
  ZynjackuSynth * obj_ptr)
{
  struct zynjacku_synth * synth_ptr;

  LOG_DEBUG("zynjacku_synth_ui_on() called.");

  synth_ptr = ZYNJACKU_SYNTH_GET_PRIVATE(obj_ptr);

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
  class = slv2_port_get_class(plugin_ptr->plugin, port_index);

  type = slv2_port_get_data_type(plugin_ptr->plugin, port_index);

  /* Get the port symbol (label) for console printing */
  symbol = slv2_port_get_symbol(plugin_ptr->plugin, port_index);

  if (strcmp(type, SLV2_DATA_TYPE_FLOAT) == 0)
  {
    if (class == SLV2_CONTROL_RATE_INPUT)
    {
      port_ptr = malloc(sizeof(struct zynjacku_synth_port));
      port_ptr->type = PORT_TYPE_PARAMETER;
      port_ptr->index = port_index;
      port_ptr->data.parameter = slv2_port_get_default_value(plugin_ptr->plugin, port_index);
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
    else
    {
      LOG_ERROR("unrecognized port.");
      goto fail;
    }
  }
  else if (strcmp(type, SLV2_DATA_TYPE_MIDI) == 0)
  {
    if (class == SLV2_CONTROL_RATE_INPUT)
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
    else if (class == SLV2_CONTROL_RATE_OUTPUT)
    {
      LOG_ERROR("midi output ports are not supported.");
      goto fail;
    }
    else
    {
      LOG_ERROR("unrecognized port.");
      goto fail;
    }
  }
  else
  {
    LOG_ERROR("unrecognized data type.");
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
  GObject * engine_obj_ptr)
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
  SLV2Property slv2_property;
  size_t value_index;
  gboolean dynparams_supported;

  synth_ptr = ZYNJACKU_SYNTH_GET_PRIVATE(synth_obj_ptr);
  engine_ptr = ZYNJACKU_ENGINE_GET_PRIVATE(engine_obj_ptr);

  if (synth_ptr->uri == NULL)
  {
    LOG_ERROR("\"uri\" property needs to be set before constructing plugin");
    goto fail;
  }

  synth_ptr->plugin = zynjacku_plugin_lookup_by_uri(synth_ptr->uri);
  if (synth_ptr->plugin == NULL)
  {
    LOG_ERROR("Failed to find plugin <%s>", synth_ptr->uri);
    goto fail;
  }

  dynparams_supported = FALSE;

  slv2_property = slv2_plugin_get_required_features(synth_ptr->plugin);

  LOG_DEBUG("Plugin has %u required features", (unsigned int)slv2_property->num_values);
  for (value_index = 0 ; value_index < slv2_property->num_values ; value_index++)
  {
    LOG_DEBUG("\"%s\"", slv2_property->values[value_index]);
    if (strcmp(LV2DYNPARAM_URI, slv2_property->values[value_index]) == 0)
    {
      dynparams_supported = TRUE;
    }
    else
    {
      LOG_DEBUG("Plugin requires unsupported feature \"%s\"", slv2_property->values[value_index]);
      slv2_property_free(slv2_property);
      goto fail;
    }
  }

  slv2_property_free(slv2_property);

  slv2_property = slv2_plugin_get_optional_features(synth_ptr->plugin);

  LOG_NOTICE("Plugin has %u optional features", (unsigned int)slv2_property->num_values);
  for (value_index = 0 ; value_index < slv2_property->num_values ; value_index++)
  {
    LOG_NOTICE("\"%s\"", slv2_property->values[value_index]);
    if (strcmp(LV2DYNPARAM_URI, slv2_property->values[value_index]) == 0)
    {
      dynparams_supported = TRUE;
    }
  }

  slv2_property_free(slv2_property);

  sample_rate = zynjacku_engine_get_sample_rate(ZYNJACKU_ENGINE(engine_obj_ptr));

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

  zynjacku_engine_activate_synth(ZYNJACKU_ENGINE(engine_obj_ptr), G_OBJECT(synth_obj_ptr));

  synth_ptr->engine_obj_ptr = engine_obj_ptr;
  g_object_ref(synth_ptr->engine_obj_ptr);

  LOG_DEBUG("Constructed plugin <%s>", slv2_plugin_get_uri(synth_ptr->plugin));

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
  engine_ptr = ZYNJACKU_ENGINE_GET_PRIVATE(synth_ptr->engine_obj_ptr);

  LOG_DEBUG("Destructing plugin <%s>", slv2_plugin_get_uri(synth_ptr->plugin));

  zynjacku_engine_deactivate_synth(ZYNJACKU_ENGINE(synth_ptr->engine_obj_ptr), G_OBJECT(synth_obj_ptr));

  slv2_instance_free(synth_ptr->instance);

  zynjacku_synth_free_ports(engine_ptr, synth_ptr);

  g_object_unref(synth_ptr->engine_obj_ptr);

  synth_ptr->instance = NULL;
}

void
dynparam_generic_group_appeared(
  lv2dynparam_host_group group_handle,
  void * instance_ui_context,
  void * parent_group_ui_context,
  const char * group_name,
  void ** group_ui_context)
{
/*   char * name; */
  struct zynjacku_synth * synth_ptr;

  synth_ptr = ZYNJACKU_SYNTH_GET_PRIVATE((ZynjackuSynth *)instance_ui_context);

/*
  name = slv2_plugin_get_name(synth_ptr->plugin);
  if (name == NULL)
  {
    LOG_ERROR("Failed to get plugin name");
    goto exit;
  }
*/

  LOG_NOTICE("Generic group \"%s\" appeared", group_name);

  g_signal_emit(
    (ZynjackuSynth *)instance_ui_context,
    g_zynjacku_synth_signals[ZYNJACKU_SYNTH_SIGNAL_GROUP_ADDED],
    0,
    group_name);

/* exit: */
  *group_ui_context = NULL;
}
