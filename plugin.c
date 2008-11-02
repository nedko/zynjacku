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

#include <stdio.h>
#include <string.h>
#include <slv2/slv2.h>
//#include <slv2/query.h>
#include <jack/jack.h>
#include <jack/midiport.h>
#include <glib-object.h>

#include "lv2-miditype.h"
#include "lv2_event.h"
#include "lv2_uri_map.h"

#include "list.h"
//#define LOG_LEVEL LOG_LEVEL_DEBUG
#include "log.h"
#include <lv2dynparam/lv2dynparam.h>
#include <lv2dynparam/lv2_rtmempool.h>
#include <lv2dynparam/host.h>

#include "plugin.h"
#include "engine.h"
#include "enum.h"
#include "gtk2gui.h"
#include "hints.h"
#include "lv2.h"

#include "zynjacku.h"
#include "plugin_repo.h"

/* signals */
#define ZYNJACKU_PLUGIN_SIGNAL_TEST                0
#define ZYNJACKU_PLUGIN_SIGNAL_GROUP_APPEARED      1
#define ZYNJACKU_PLUGIN_SIGNAL_GROUP_DISAPPEARED   2
#define ZYNJACKU_PLUGIN_SIGNAL_BOOL_APPEARED       3
#define ZYNJACKU_PLUGIN_SIGNAL_BOOL_DISAPPEARED    4
#define ZYNJACKU_PLUGIN_SIGNAL_FLOAT_APPEARED      5
#define ZYNJACKU_PLUGIN_SIGNAL_FLOAT_DISAPPEARED   6
#define ZYNJACKU_PLUGIN_SIGNAL_ENUM_APPEARED       7
#define ZYNJACKU_PLUGIN_SIGNAL_ENUM_DISAPPEARED    8
#define ZYNJACKU_PLUGIN_SIGNAL_INT_APPEARED        9
#define ZYNJACKU_PLUGIN_SIGNAL_INT_DISAPPEARED    10
#define ZYNJACKU_PLUGIN_SIGNAL_CUSTOM_GUI_OF      11
#define ZYNJACKU_PLUGIN_SIGNALS_COUNT             12

/* properties */
#define ZYNJACKU_PLUGIN_PROP_URI                1

static guint g_zynjacku_plugin_signals[ZYNJACKU_PLUGIN_SIGNALS_COUNT];

/* UGLY: We convert dynparam context poitners to string to pass them
   as opaque types through Python. Silly, but codegen fails to create
   marshaling code for gpointer arguments. If possible at all, it is a
   hidden black magic. Other workaround ideas: GObject wrapper, GBoxed
   and GValue. */

gchar *
zynjacku_plugin_context_to_string(
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
zynjacku_plugin_context_from_string(
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
zynjacku_plugin_dispose(GObject * obj)
{
  struct zynjacku_plugin * plugin_ptr;

  plugin_ptr = ZYNJACKU_PLUGIN_GET_PRIVATE(obj);

  LOG_DEBUG("zynjacku_plugin_dispose() called.");

  if (plugin_ptr->dispose_has_run)
  {
    /* If dispose did already run, return. */
    LOG_DEBUG("zynjacku_plugin_dispose() already run!");
    return;
  }

  /* Make sure dispose does not run twice. */
  plugin_ptr->dispose_has_run = TRUE;

  /* 
   * In dispose, you are supposed to free all types referenced from this
   * object which might themselves hold a reference to self. Generally,
   * the most simple solution is to unref all members on which you own a 
   * reference.
   */
  if (plugin_ptr->lv2plugin != NULL)
  {
    zynjacku_plugin_destruct(ZYNJACKU_PLUGIN(obj));
  }

  if (plugin_ptr->uri != NULL)
  {
    g_free(plugin_ptr->uri);
    plugin_ptr->uri = NULL;
  }

  /* Chain up to the parent class */
  G_OBJECT_CLASS(g_type_class_peek_parent(G_OBJECT_GET_CLASS(obj)))->dispose(obj);
}

static void
zynjacku_plugin_finalize(GObject * obj)
{
//  struct zynjacku_plugin * self = ZYNJACKU_PLUGIN_GET_PRIVATE(obj);

  LOG_DEBUG("zynjacku_plugin_finalize() called.");

  /*
   * Here, complete object destruction.
   * You might not need to do much...
   */
  //g_free(self->private);

  /* Chain up to the parent class */
  G_OBJECT_CLASS(g_type_class_peek_parent(G_OBJECT_GET_CLASS(obj)))->finalize(obj);
}

static void
zynjacku_plugin_set_property(
  GObject * object_ptr,
  guint property_id,
  const GValue * value_ptr,
  GParamSpec * param_spec_ptr)
{
  struct zynjacku_plugin * plugin_ptr;

  plugin_ptr = ZYNJACKU_PLUGIN_GET_PRIVATE(object_ptr);

  switch (property_id)
  {
  case ZYNJACKU_PLUGIN_PROP_URI:
    //LOG_DEBUG("setting plugin uri to: \"%s\"", g_value_get_string(value_ptr));
    //break;
    if (plugin_ptr->uri != NULL)
    {
      g_free(plugin_ptr->uri);
    }
    plugin_ptr->uri = g_value_dup_string(value_ptr);
    LOG_DEBUG("plugin uri set to: \"%s\"", plugin_ptr->uri);
    break;
  default:
    /* We don't have any other property... */
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object_ptr, property_id, param_spec_ptr);
    break;
  }
}

static void
zynjacku_plugin_get_property(
  GObject * object_ptr,
  guint property_id,
  GValue * value_ptr,
  GParamSpec * param_spec_ptr)
{
  struct zynjacku_plugin * plugin_ptr;

  plugin_ptr = ZYNJACKU_PLUGIN_GET_PRIVATE(object_ptr);

  switch (property_id)
  {
  case ZYNJACKU_PLUGIN_PROP_URI:
    if (plugin_ptr->uri != NULL)
    {
      g_value_set_string(value_ptr, plugin_ptr->uri);
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
zynjacku_plugin_class_init(
  gpointer class_ptr,
  gpointer class_data_ptr)
{
  GParamSpec * uri_param_spec;

  LOG_DEBUG("zynjacku_plugin_class_init() called.");

  G_OBJECT_CLASS(class_ptr)->dispose = zynjacku_plugin_dispose;
  G_OBJECT_CLASS(class_ptr)->finalize = zynjacku_plugin_finalize;

  g_type_class_add_private(G_OBJECT_CLASS(class_ptr), sizeof(struct zynjacku_plugin));

  g_zynjacku_plugin_signals[ZYNJACKU_PLUGIN_SIGNAL_TEST] =
    g_signal_new(
      "test",                   /* signal_name */
      ZYNJACKU_PLUGIN_TYPE,      /* itype */
      G_SIGNAL_RUN_LAST |
      G_SIGNAL_ACTION,          /* signal_flags */
      0,                        /* class_offset */
      NULL,                     /* accumulator */
      NULL,                     /* accu_data */
      NULL,                     /* c_marshaller */
      G_TYPE_NONE,              /* return type */
      1,                        /* n_params */
      G_TYPE_OBJECT);

  g_zynjacku_plugin_signals[ZYNJACKU_PLUGIN_SIGNAL_GROUP_APPEARED] =
    g_signal_new(
      "group-appeared",         /* signal_name */
      ZYNJACKU_PLUGIN_TYPE,      /* itype */
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
      G_TYPE_OBJECT,            /* hints */
      G_TYPE_STRING);           /* context */

  g_zynjacku_plugin_signals[ZYNJACKU_PLUGIN_SIGNAL_GROUP_DISAPPEARED] =
    g_signal_new(
      "group-disappeared",      /* signal_name */
      ZYNJACKU_PLUGIN_TYPE,      /* itype */
      G_SIGNAL_RUN_LAST |
      G_SIGNAL_ACTION,          /* signal_flags */
      0,                        /* class_offset */
      NULL,                     /* accumulator */
      NULL,                     /* accu_data */
      NULL,                     /* c_marshaller */
      G_TYPE_NONE,              /* return type */
      1,                        /* n_params */
      G_TYPE_OBJECT);           /* object */

  g_zynjacku_plugin_signals[ZYNJACKU_PLUGIN_SIGNAL_BOOL_APPEARED] =
    g_signal_new(
      "bool-appeared",          /* signal_name */
      ZYNJACKU_PLUGIN_TYPE,      /* itype */
      G_SIGNAL_RUN_LAST |
      G_SIGNAL_ACTION,          /* signal_flags */
      0,                        /* class_offset */
      NULL,                     /* accumulator */
      NULL,                     /* accu_data */
      NULL,                     /* c_marshaller */
      G_TYPE_OBJECT,            /* return type */
      5,                        /* n_params */
      G_TYPE_OBJECT,            /* parent */
      G_TYPE_STRING,            /* parameter name */
      G_TYPE_OBJECT,            /* hints */
      G_TYPE_BOOLEAN,           /* value */
      G_TYPE_STRING);           /* context */

  g_zynjacku_plugin_signals[ZYNJACKU_PLUGIN_SIGNAL_BOOL_DISAPPEARED] =
    g_signal_new(
      "bool-disappeared",       /* signal_name */
      ZYNJACKU_PLUGIN_TYPE,      /* itype */
      G_SIGNAL_RUN_LAST |
      G_SIGNAL_ACTION,          /* signal_flags */
      0,                        /* class_offset */
      NULL,                     /* accumulator */
      NULL,                     /* accu_data */
      NULL,                     /* c_marshaller */
      G_TYPE_NONE,              /* return type */
      1,                        /* n_params */
      G_TYPE_OBJECT);           /* object */

  g_zynjacku_plugin_signals[ZYNJACKU_PLUGIN_SIGNAL_FLOAT_APPEARED] =
    g_signal_new(
      "float-appeared",         /* signal_name */
      ZYNJACKU_PLUGIN_TYPE,      /* itype */
      G_SIGNAL_RUN_LAST |
      G_SIGNAL_ACTION,          /* signal_flags */
      0,                        /* class_offset */
      NULL,                     /* accumulator */
      NULL,                     /* accu_data */
      NULL,                     /* c_marshaller */
      G_TYPE_OBJECT,            /* return type */
      7,                        /* n_params */
      G_TYPE_OBJECT,            /* parent */
      G_TYPE_STRING,            /* parameter name */
      G_TYPE_OBJECT,            /* hints */
      G_TYPE_FLOAT,             /* value */
      G_TYPE_FLOAT,             /* min */
      G_TYPE_FLOAT,             /* max */
      G_TYPE_STRING);           /* context */

  g_zynjacku_plugin_signals[ZYNJACKU_PLUGIN_SIGNAL_FLOAT_DISAPPEARED] =
    g_signal_new(
      "float-disappeared",      /* signal_name */
      ZYNJACKU_PLUGIN_TYPE,      /* itype */
      G_SIGNAL_RUN_LAST |
      G_SIGNAL_ACTION,          /* signal_flags */
      0,                        /* class_offset */
      NULL,                     /* accumulator */
      NULL,                     /* accu_data */
      NULL,                     /* c_marshaller */
      G_TYPE_NONE,              /* return type */
      1,                        /* n_params */
      G_TYPE_OBJECT);           /* object */

  g_zynjacku_plugin_signals[ZYNJACKU_PLUGIN_SIGNAL_ENUM_APPEARED] =
    g_signal_new(
      "enum-appeared",          /* signal_name */
      ZYNJACKU_PLUGIN_TYPE,      /* itype */
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
      G_TYPE_OBJECT,            /* hints */
      G_TYPE_UINT,              /* selected value index */
      G_TYPE_OBJECT,            /* valid values (ZynjackuEnum) */
      G_TYPE_STRING);           /* context */

  g_zynjacku_plugin_signals[ZYNJACKU_PLUGIN_SIGNAL_ENUM_DISAPPEARED] =
    g_signal_new(
      "enum-disappeared",       /* signal_name */
      ZYNJACKU_PLUGIN_TYPE,      /* itype */
      G_SIGNAL_RUN_LAST |
      G_SIGNAL_ACTION,          /* signal_flags */
      0,                        /* class_offset */
      NULL,                     /* accumulator */
      NULL,                     /* accu_data */
      NULL,                     /* c_marshaller */
      G_TYPE_NONE,              /* return type */
      1,                        /* n_params */
      G_TYPE_OBJECT);           /* object */

  g_zynjacku_plugin_signals[ZYNJACKU_PLUGIN_SIGNAL_INT_APPEARED] =
    g_signal_new(
      "int-appeared",           /* signal_name */
      ZYNJACKU_PLUGIN_TYPE,      /* itype */
      G_SIGNAL_RUN_LAST |
      G_SIGNAL_ACTION,          /* signal_flags */
      0,                        /* class_offset */
      NULL,                     /* accumulator */
      NULL,                     /* accu_data */
      NULL,                     /* c_marshaller */
      G_TYPE_OBJECT,            /* return type */
      7,                        /* n_params */
      G_TYPE_OBJECT,            /* parent */
      G_TYPE_STRING,            /* parameter name */
      G_TYPE_OBJECT,            /* hints */
      G_TYPE_INT,               /* value */
      G_TYPE_INT,               /* min */
      G_TYPE_INT,               /* max */
      G_TYPE_STRING);           /* context */

  g_zynjacku_plugin_signals[ZYNJACKU_PLUGIN_SIGNAL_INT_DISAPPEARED] =
    g_signal_new(
      "int-disappeared",        /* signal_name */
      ZYNJACKU_PLUGIN_TYPE,      /* itype */
      G_SIGNAL_RUN_LAST |
      G_SIGNAL_ACTION,          /* signal_flags */
      0,                        /* class_offset */
      NULL,                     /* accumulator */
      NULL,                     /* accu_data */
      NULL,                     /* c_marshaller */
      G_TYPE_NONE,              /* return type */
      1,                        /* n_params */
      G_TYPE_OBJECT);           /* object */

  g_zynjacku_plugin_signals[ZYNJACKU_PLUGIN_SIGNAL_CUSTOM_GUI_OF] =
    g_signal_new(
      "custom-gui-off",         /* signal_name */
      ZYNJACKU_PLUGIN_TYPE,      /* itype */
      G_SIGNAL_RUN_LAST |
      G_SIGNAL_ACTION,          /* signal_flags */
      0,                        /* class_offset */
      NULL,                     /* accumulator */
      NULL,                     /* accu_data */
      NULL,                     /* c_marshaller */
      G_TYPE_NONE,              /* return type */
      0);                       /* n_params */

  G_OBJECT_CLASS(class_ptr)->get_property = zynjacku_plugin_get_property;
  G_OBJECT_CLASS(class_ptr)->set_property = zynjacku_plugin_set_property;

  uri_param_spec = g_param_spec_string(
    "uri",
    "Plugin LV2 URI construct property",
    "Plugin LV2 URI construct property",
    "" /* default value */,
    G_PARAM_CONSTRUCT_ONLY |G_PARAM_READWRITE);

  g_object_class_install_property(
    G_OBJECT_CLASS(class_ptr),
    ZYNJACKU_PLUGIN_PROP_URI,
    uri_param_spec);
}

static void
zynjacku_plugin_init(
  GTypeInstance * instance,
  gpointer g_class)
{
  struct zynjacku_plugin * plugin_ptr;

  LOG_DEBUG("zynjacku_plugin_init() called.");

  plugin_ptr = ZYNJACKU_PLUGIN_GET_PRIVATE(instance);

  plugin_ptr->dispose_has_run = FALSE;
  INIT_LIST_HEAD(&plugin_ptr->parameter_ports);

  plugin_ptr->type = PLUGIN_TYPE_UNKNOWN;

  plugin_ptr->uri = NULL;
  plugin_ptr->lv2plugin = NULL;

  plugin_ptr->root_group_ui_context = NULL;
}

GType zynjacku_plugin_get_type()
{
  static GType type = 0;
  if (type == 0)
  {
    type = g_type_register_static_simple(
      G_TYPE_OBJECT,
      "zynjacku_plugin_type",
      sizeof(ZynjackuPluginClass),
      zynjacku_plugin_class_init,
      sizeof(ZynjackuPlugin),
      zynjacku_plugin_init,
      0);
  }

  return type;
}

const char *
zynjacku_plugin_get_instance_name(
  ZynjackuPlugin * obj_ptr)
{
  struct zynjacku_plugin * plugin_ptr;

  plugin_ptr = ZYNJACKU_PLUGIN_GET_PRIVATE(obj_ptr);

  return plugin_ptr->id;
}

const char *
zynjacku_plugin_get_name(
  ZynjackuPlugin * obj_ptr)
{
  struct zynjacku_plugin * plugin_ptr;

  plugin_ptr = ZYNJACKU_PLUGIN_GET_PRIVATE(obj_ptr);

  return plugin_ptr->name;
}

const char *
zynjacku_plugin_get_uri(
  ZynjackuPlugin * obj_ptr)
{
  struct zynjacku_plugin * plugin_ptr;

  plugin_ptr = ZYNJACKU_PLUGIN_GET_PRIVATE(obj_ptr);

  return plugin_ptr->uri;
}

void
zynjacku_plugin_generic_lv2_ui_on(
  ZynjackuPlugin * plugin_obj_ptr)
{
  struct zynjacku_plugin * plugin_ptr;
  struct list_head * node_ptr;
  struct zynjacku_port * port_ptr;
  ZynjackuHints * hints_obj_ptr;

  LOG_DEBUG("zynjacku_plugin_generic_lv2_ui_on() called.");

  plugin_ptr = ZYNJACKU_PLUGIN_GET_PRIVATE(plugin_obj_ptr);

  if (plugin_ptr->root_group_ui_context != NULL)
  {
    LOG_DEBUG("ui on ignored - already shown");
    return;                     /* already shown */
  }

  hints_obj_ptr = g_object_new(ZYNJACKU_HINTS_TYPE, NULL);

  zynjacku_hints_set(
    hints_obj_ptr,
    0,
    NULL,
    NULL);

  g_signal_emit(
    plugin_obj_ptr,
    g_zynjacku_plugin_signals[ZYNJACKU_PLUGIN_SIGNAL_GROUP_APPEARED],
    0,
    NULL,
    plugin_ptr->id,
    hints_obj_ptr,
    "",
    &plugin_ptr->root_group_ui_context);

  list_for_each(node_ptr, &plugin_ptr->parameter_ports)
  {
    port_ptr = list_entry(node_ptr, struct zynjacku_port, plugin_siblings);

    g_signal_emit(
      plugin_obj_ptr,
      g_zynjacku_plugin_signals[ZYNJACKU_PLUGIN_SIGNAL_FLOAT_APPEARED],
      0,
      plugin_ptr->root_group_ui_context,
      port_ptr->name,
      hints_obj_ptr,
      port_ptr->data.parameter.value,
      port_ptr->data.parameter.min,
      port_ptr->data.parameter.max,
      zynjacku_plugin_context_to_string(&port_ptr->data.parameter.value),
      &port_ptr->ui_context);
  }

  g_object_unref(hints_obj_ptr);
}

void
zynjacku_plugin_generic_lv2_ui_off(
  ZynjackuPlugin * plugin_obj_ptr)
{
  struct zynjacku_plugin * plugin_ptr;
  struct list_head * node_ptr;
  struct zynjacku_port * port_ptr;

  LOG_DEBUG("zynjacku_plugin_generic_lv2_ui_off() called.");

  plugin_ptr = ZYNJACKU_PLUGIN_GET_PRIVATE(plugin_obj_ptr);

  if (plugin_ptr->root_group_ui_context == NULL)
  {
    LOG_DEBUG("ui off ignored - not shown");
    return;                     /* not shown */
  }

  list_for_each(node_ptr, &plugin_ptr->parameter_ports)
  {
    port_ptr = list_entry(node_ptr, struct zynjacku_port, plugin_siblings);

    g_signal_emit(
      plugin_obj_ptr,
      g_zynjacku_plugin_signals[ZYNJACKU_PLUGIN_SIGNAL_FLOAT_DISAPPEARED],
      0,
      port_ptr->ui_context);

    g_object_unref(port_ptr->ui_context);
    port_ptr->ui_context = NULL;
  }

  g_signal_emit(
    plugin_obj_ptr,
    g_zynjacku_plugin_signals[ZYNJACKU_PLUGIN_SIGNAL_GROUP_DISAPPEARED],
    0,
    plugin_ptr->root_group_ui_context);

  g_object_unref(plugin_ptr->root_group_ui_context);
  plugin_ptr->root_group_ui_context = NULL;
}

gboolean
zynjacku_plugin_supports_generic_ui(
  ZynjackuPlugin * plugin_obj_ptr)
{
//  struct zynjacku_plugin * plugin_ptr;

  LOG_DEBUG("zynjacku_plugin_supports_generic_ui() called.");

//  plugin_ptr = ZYNJACKU_PLUGIN_GET_PRIVATE(plugin_obj_ptr);

  /* we can generate it always */
  return TRUE;
}

gboolean
zynjacku_plugin_supports_custom_ui(
  ZynjackuPlugin * plugin_obj_ptr)
{
  struct zynjacku_plugin * plugin_ptr;

  LOG_DEBUG("zynjacku_plugin_supports_custom_ui() called.");

  plugin_ptr = ZYNJACKU_PLUGIN_GET_PRIVATE(plugin_obj_ptr);

  return (plugin_ptr->gtk2gui != ZYNJACKU_GTK2GUI_HANDLE_INVALID_VALUE) ? TRUE : FALSE;
}

gboolean
zynjacku_plugin_ui_on(
  ZynjackuPlugin * plugin_obj_ptr)
{
  struct zynjacku_plugin * plugin_ptr;

  LOG_DEBUG("zynjacku_plugin_ui_on() called.");

  plugin_ptr = ZYNJACKU_PLUGIN_GET_PRIVATE(plugin_obj_ptr);

  if (plugin_ptr->gtk2gui != ZYNJACKU_GTK2GUI_HANDLE_INVALID_VALUE)
  {
    return zynjacku_gtk2gui_ui_on(plugin_ptr->gtk2gui);
  }

  if (plugin_ptr->dynparams)
  {
    lv2dynparam_host_ui_on(plugin_ptr->dynparams);
  }
  else
  {
    zynjacku_plugin_generic_lv2_ui_on(plugin_obj_ptr);
  }

  return true;
}

void
zynjacku_plugin_ui_off(
  ZynjackuPlugin * plugin_obj_ptr)
{
  struct zynjacku_plugin * plugin_ptr;

  LOG_DEBUG("zynjacku_plugin_ui_off() called.");

  plugin_ptr = ZYNJACKU_PLUGIN_GET_PRIVATE(plugin_obj_ptr);

  if (plugin_ptr->gtk2gui != ZYNJACKU_GTK2GUI_HANDLE_INVALID_VALUE)
  {
    zynjacku_gtk2gui_ui_off(plugin_ptr->gtk2gui);
  }
  else if (plugin_ptr->dynparams)
  {
    lv2dynparam_host_ui_off(plugin_ptr->dynparams);
  }
  else
  {
    zynjacku_plugin_generic_lv2_ui_off(plugin_obj_ptr);
  }
}

void
zynjacku_gtk2gui_on_ui_destroyed(
  void * context_ptr)
{
  struct zynjacku_plugin * plugin_ptr;

  plugin_ptr = ZYNJACKU_PLUGIN_GET_PRIVATE(context_ptr);

  LOG_DEBUG("%s gtk2gui window destroyed", plugin_ptr->id);

  g_signal_emit(
    (ZynjackuPlugin *)context_ptr,
    g_zynjacku_plugin_signals[ZYNJACKU_PLUGIN_SIGNAL_CUSTOM_GUI_OF],
    0,
    NULL);
}

void
zynjacku_plugin_free_ports(
  struct zynjacku_engine * engine_ptr,
  struct zynjacku_plugin * plugin_ptr)
{
  struct list_head * node_ptr;
  struct zynjacku_port * port_ptr;

  while (!list_empty(&plugin_ptr->parameter_ports))
  {
    node_ptr = plugin_ptr->parameter_ports.next;
    port_ptr = list_entry(node_ptr, struct zynjacku_port, plugin_siblings);

    assert(port_ptr->type == PORT_TYPE_PARAMETER);

    list_del(node_ptr);

    free(port_ptr->symbol);
    free(port_ptr->name);
    free(port_ptr);
  }

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

#define synth_ptr (&plugin_ptr->subtype.synth)

bool
zynjacku_plugin_construct_synth(
  struct zynjacku_plugin * plugin_ptr,
  ZynjackuPlugin * plugin_obj_ptr,
  struct zynjacku_engine * engine_ptr,
  GObject * engine_object_ptr)
{
  static unsigned int id;
  char * port_name;
  size_t size_name;
  size_t size_id;
  struct list_head * node_ptr;
  struct zynjacku_port * port_ptr;

  plugin_ptr->type = PLUGIN_TYPE_SYNTH;
  synth_ptr->midi_in_port.type = PORT_TYPE_INVALID;
  synth_ptr->audio_out_left_port.type = PORT_TYPE_INVALID;
  synth_ptr->audio_out_right_port.type = PORT_TYPE_INVALID;

  if (!zynjacku_plugin_repo_load_synth(plugin_ptr))
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

  /* connect midi port */
  switch (synth_ptr->midi_in_port.type)
  {
  case PORT_TYPE_MIDI:
    zynjacku_lv2_connect_port(plugin_ptr->lv2plugin, synth_ptr->midi_in_port.index, &engine_ptr->lv2_midi_buffer);
    break;
  case PORT_TYPE_EVENT_MIDI:
    zynjacku_lv2_connect_port(plugin_ptr->lv2plugin, synth_ptr->midi_in_port.index, &engine_ptr->lv2_midi_event_buffer);
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
    zynjacku_lv2_connect_port(plugin_ptr->lv2plugin, port_ptr->index, &port_ptr->data.parameter);
    LOG_INFO("Set %s to %f", port_ptr->symbol, port_ptr->data.parameter);
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

  zynjacku_engine_activate_synth(ZYNJACKU_ENGINE(engine_object_ptr), G_OBJECT(plugin_obj_ptr));

  plugin_ptr->engine_object_ptr = engine_object_ptr;
  g_object_ref(plugin_ptr->engine_object_ptr);

  /* no plugins to test gtk2gui */
  plugin_ptr->gtk2gui = zynjacku_gtk2gui_create(engine_ptr->host_features, plugin_obj_ptr, plugin_ptr->uri, plugin_ptr->id, &plugin_ptr->parameter_ports);

  LOG_DEBUG("Constructed plugin <%s>, gtk2gui <%p>", plugin_ptr->uri, plugin_ptr->gtk2gui);

  return true;

fail_free_ports:
  zynjacku_plugin_free_ports(engine_ptr, plugin_ptr);

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

gboolean
zynjacku_plugin_construct(
  ZynjackuPlugin * plugin_obj_ptr,
  GObject * engine_object_ptr)
{
  struct zynjacku_plugin * plugin_ptr;

  plugin_ptr = ZYNJACKU_PLUGIN_GET_PRIVATE(plugin_obj_ptr);

  if (plugin_ptr->uri == NULL)
  {
    LOG_ERROR("\"uri\" property needs to be set before constructing plugin");
    return false;
  }

  if (ZYNJACKU_IS_ENGINE(engine_object_ptr))
  {
    return zynjacku_plugin_construct_synth(
      plugin_ptr,
      plugin_obj_ptr,
      ZYNJACKU_ENGINE_GET_PRIVATE(engine_object_ptr),
      engine_object_ptr);
  }

  LOG_ERROR("Cannot cosntruct plugin for unknown engine type");

  return false;
}

#undef synth_ptr

void
zynjacku_plugin_destruct(
  ZynjackuPlugin * plugin_obj_ptr)
{
  struct zynjacku_plugin * plugin_ptr;
  struct zynjacku_engine * engine_ptr;

  plugin_ptr = ZYNJACKU_PLUGIN_GET_PRIVATE(plugin_obj_ptr);
  engine_ptr = ZYNJACKU_ENGINE_GET_PRIVATE(plugin_ptr->engine_object_ptr);

  LOG_DEBUG("Destructing plugin <%s>", plugin_ptr->uri);

  zynjacku_engine_deactivate_synth(ZYNJACKU_ENGINE(plugin_ptr->engine_object_ptr), G_OBJECT(plugin_obj_ptr));

  if (plugin_ptr->gtk2gui != ZYNJACKU_GTK2GUI_HANDLE_INVALID_VALUE)
  {
    zynjacku_gtk2gui_destroy(plugin_ptr->gtk2gui);
  }

  zynjacku_plugin_free_ports(engine_ptr, plugin_ptr);

  if (plugin_ptr->dynparams != NULL)
  {
    lv2dynparam_host_detach(plugin_ptr->dynparams);
    plugin_ptr->dynparams = NULL;
  }

  g_object_unref(plugin_ptr->engine_object_ptr);

  zynjacku_lv2_unload(plugin_ptr->lv2plugin);
  plugin_ptr->lv2plugin = NULL;

  free(plugin_ptr->id);
  plugin_ptr->id = NULL;
}

void
dynparam_group_appeared(
  lv2dynparam_host_group group_handle,
  void * instance_ui_context,
  void * parent_group_ui_context,
  const char * group_name,
  const struct lv2dynparam_hints * hints_ptr,
  void ** group_ui_context)
{
  struct zynjacku_plugin * plugin_ptr;
  GObject * ret_obj_ptr;
  ZynjackuHints * hints_obj_ptr;

  plugin_ptr = ZYNJACKU_PLUGIN_GET_PRIVATE((ZynjackuPlugin *)instance_ui_context);

  LOG_DEBUG("Group \"%s\" appeared, handle %p", group_name, group_handle);

  hints_obj_ptr = g_object_new(ZYNJACKU_HINTS_TYPE, NULL);

  zynjacku_hints_set(
    hints_obj_ptr,
    hints_ptr->count,
    (const gchar * const *)hints_ptr->names,
    (const gchar * const *)hints_ptr->values);

  g_signal_emit(
    (ZynjackuPlugin *)instance_ui_context,
    g_zynjacku_plugin_signals[ZYNJACKU_PLUGIN_SIGNAL_GROUP_APPEARED],
    0,
    parent_group_ui_context,
    group_name,
    hints_obj_ptr,
    zynjacku_plugin_context_to_string(group_handle),
    &ret_obj_ptr);

  LOG_DEBUG("group-appeared signal returned object ptr is %p", ret_obj_ptr);

  g_object_unref(hints_obj_ptr);

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
    (ZynjackuPlugin *)instance_ui_context,
    g_zynjacku_plugin_signals[ZYNJACKU_PLUGIN_SIGNAL_GROUP_DISAPPEARED],
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
    (ZynjackuPlugin *)instance_ui_context,
    g_zynjacku_plugin_signals[ZYNJACKU_PLUGIN_SIGNAL_BOOL_DISAPPEARED],
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
    (ZynjackuPlugin *)instance_ui_context,
    g_zynjacku_plugin_signals[ZYNJACKU_PLUGIN_SIGNAL_FLOAT_DISAPPEARED],
    0,
    parameter_ui_context);

  g_object_unref(parameter_ui_context);
}

void
dynparam_parameter_enum_disappeared(
  void * instance_ui_context,
  void * parent_group_ui_context,
  void * parameter_ui_context)
{
  LOG_DEBUG("dynparam_parameter_enum_disappeared() called.");

  g_signal_emit(
    (ZynjackuPlugin *)instance_ui_context,
    g_zynjacku_plugin_signals[ZYNJACKU_PLUGIN_SIGNAL_ENUM_DISAPPEARED],
    0,
    parameter_ui_context);

  g_object_unref(parameter_ui_context);
}

void
dynparam_parameter_int_disappeared(
  void * instance_ui_context,
  void * parent_group_ui_context,
  void * parameter_ui_context)
{
  LOG_DEBUG("dynparam_parameter_int_disappeared() called.");

  g_signal_emit(
    (ZynjackuPlugin *)instance_ui_context,
    g_zynjacku_plugin_signals[ZYNJACKU_PLUGIN_SIGNAL_INT_DISAPPEARED],
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
  const struct lv2dynparam_hints * hints_ptr,
  bool value,
  void ** parameter_ui_context)
{
  GObject * ret_obj_ptr;
  ZynjackuHints * hints_obj_ptr;

  LOG_DEBUG(
    "Boolean parameter \"%s\" appeared, value %s, handle %p",
    parameter_name,
    value ? "TRUE" : "FALSE",
    parameter_handle);

  hints_obj_ptr = g_object_new(ZYNJACKU_HINTS_TYPE, NULL);

  zynjacku_hints_set(
    hints_obj_ptr,
    hints_ptr->count,
    (const gchar * const *)hints_ptr->names,
    (const gchar * const *)hints_ptr->values);

  g_signal_emit(
    (ZynjackuPlugin *)instance_ui_context,
    g_zynjacku_plugin_signals[ZYNJACKU_PLUGIN_SIGNAL_BOOL_APPEARED],
    0,
    group_ui_context,
    parameter_name,
    hints_obj_ptr,
    (gboolean)value,
    zynjacku_plugin_context_to_string(parameter_handle),
    &ret_obj_ptr);

  LOG_DEBUG("bool-appeared signal returned object ptr is %p", ret_obj_ptr);

  g_object_unref(hints_obj_ptr);

  *parameter_ui_context = ret_obj_ptr;
}

void
zynjacku_plugin_bool_set(
  ZynjackuPlugin * plugin_obj_ptr,
  gchar * string_context,
  gboolean value)
{
  void * context;
  struct zynjacku_plugin * plugin_ptr;

  plugin_ptr = ZYNJACKU_PLUGIN_GET_PRIVATE(plugin_obj_ptr);

  context = zynjacku_plugin_context_from_string(string_context);

  LOG_DEBUG("zynjacku_plugin_bool_set() called, context %p", context);

  dynparam_parameter_boolean_change(
    plugin_ptr->dynparams,
    (lv2dynparam_host_parameter)context,
    value);
}

void
dynparam_parameter_float_appeared(
  lv2dynparam_host_parameter parameter_handle,
  void * instance_ui_context,
  void * group_ui_context,
  const char * parameter_name,
  const struct lv2dynparam_hints * hints_ptr,
  float value,
  float min,
  float max,
  void ** parameter_ui_context)
{
  GObject * ret_obj_ptr;
  ZynjackuHints * hints_obj_ptr;

  LOG_DEBUG(
    "Float parameter \"%s\" appeared, value %f, min %f, max %f, handle %p",
    parameter_name,
    value,
    min,
    max,
    parameter_handle);

  hints_obj_ptr = g_object_new(ZYNJACKU_HINTS_TYPE, NULL);

  zynjacku_hints_set(
    hints_obj_ptr,
    hints_ptr->count,
    (const gchar * const *)hints_ptr->names,
    (const gchar * const *)hints_ptr->values);

  g_signal_emit(
    (ZynjackuPlugin *)instance_ui_context,
    g_zynjacku_plugin_signals[ZYNJACKU_PLUGIN_SIGNAL_FLOAT_APPEARED],
    0,
    group_ui_context,
    parameter_name,
    hints_obj_ptr,
    (gfloat)value,
    (gfloat)min,
    (gfloat)max,
    zynjacku_plugin_context_to_string(parameter_handle),
    &ret_obj_ptr);

  LOG_DEBUG("float-appeared signal returned object ptr is %p", ret_obj_ptr);

  g_object_unref(hints_obj_ptr);

  *parameter_ui_context = ret_obj_ptr;
}

void
zynjacku_plugin_float_set(
  ZynjackuPlugin * plugin_obj_ptr,
  gchar * string_context,
  gfloat value)
{
  void * context;
  struct zynjacku_plugin * plugin_ptr;

  plugin_ptr = ZYNJACKU_PLUGIN_GET_PRIVATE(plugin_obj_ptr);

  context = zynjacku_plugin_context_from_string(string_context);

  LOG_DEBUG("zynjacku_plugin_float_set() called, context %p", context);

  if (plugin_ptr->dynparams != NULL)
  {
    dynparam_parameter_float_change(
      plugin_ptr->dynparams,
      (lv2dynparam_host_parameter)context,
      value);
  }
  else
  {
    *(float *)context = value;
  }
}

void
dynparam_parameter_enum_appeared(
  lv2dynparam_host_parameter parameter_handle,
  void * instance_ui_context,
  void * group_ui_context,
  const char * parameter_name,
  const struct lv2dynparam_hints * hints_ptr,
  unsigned int selected_value,
  const char * const * values,
  unsigned int values_count,
  void ** parameter_ui_context)
{
  unsigned int i;
  GObject * ret_obj_ptr;
  ZynjackuEnum * enum_ptr;
  ZynjackuHints * hints_obj_ptr;

  LOG_DEBUG(
    "Enum parameter \"%s\" appeared, %u possible values, handle %p",
    parameter_name,
    values_count,
    parameter_handle);

  for (i = 0 ; i < values_count ; i++)
  {
    LOG_DEBUG("\"%s\"%s", values[i], selected_value == i ? " [SELECTED]" : "");
  }

  hints_obj_ptr = g_object_new(ZYNJACKU_HINTS_TYPE, NULL);

  zynjacku_hints_set(
    hints_obj_ptr,
    hints_ptr->count,
    (const gchar * const *)hints_ptr->names,
    (const gchar * const *)hints_ptr->values);

  enum_ptr = g_object_new(ZYNJACKU_ENUM_TYPE, NULL);

  zynjacku_enum_set(enum_ptr, values, values_count);

  g_signal_emit(
    (ZynjackuPlugin *)instance_ui_context,
    g_zynjacku_plugin_signals[ZYNJACKU_PLUGIN_SIGNAL_ENUM_APPEARED],
    0,
    group_ui_context,
    parameter_name,
    hints_obj_ptr,
    (guint)selected_value,
    enum_ptr,
    zynjacku_plugin_context_to_string(parameter_handle),
    &ret_obj_ptr);

  LOG_DEBUG("enum-appeared signal returned object ptr is %p", ret_obj_ptr);

  g_object_unref(hints_obj_ptr);

  g_object_unref(enum_ptr);

  *parameter_ui_context = ret_obj_ptr;
}

void
zynjacku_plugin_enum_set(
  ZynjackuPlugin * plugin_obj_ptr,
  gchar * string_context,
  guint value)
{
  void * context;
  struct zynjacku_plugin * plugin_ptr;

  plugin_ptr = ZYNJACKU_PLUGIN_GET_PRIVATE(plugin_obj_ptr);

  context = zynjacku_plugin_context_from_string(string_context);

  LOG_NOTICE("zynjacku_plugin_enum_set() called, context %p, value %u", context, value);

  if (plugin_ptr->dynparams != NULL)
  {
    dynparam_parameter_enum_change(
      plugin_ptr->dynparams,
      (lv2dynparam_host_parameter)context,
      value);
  }
}

void
dynparam_parameter_int_appeared(
  lv2dynparam_host_parameter parameter_handle,
  void * instance_ui_context,
  void * group_ui_context,
  const char * parameter_name,
  const struct lv2dynparam_hints * hints_ptr,
  signed int value,
  signed int min,
  signed int max,
  void ** parameter_ui_context)
{
  GObject * ret_obj_ptr;
  ZynjackuHints * hints_obj_ptr;

  LOG_DEBUG(
    "Integer parameter \"%s\" appeared, value %d, min %d, max %d, handle %p",
    parameter_name,
    value,
    min,
    max,
    parameter_handle);

  hints_obj_ptr = g_object_new(ZYNJACKU_HINTS_TYPE, NULL);

  zynjacku_hints_set(
    hints_obj_ptr,
    hints_ptr->count,
    (const gchar * const *)hints_ptr->names,
    (const gchar * const *)hints_ptr->values);

  g_signal_emit(
    (ZynjackuPlugin *)instance_ui_context,
    g_zynjacku_plugin_signals[ZYNJACKU_PLUGIN_SIGNAL_INT_APPEARED],
    0,
    group_ui_context,
    parameter_name,
    hints_obj_ptr,
    (gint)value,
    (gint)min,
    (gint)max,
    zynjacku_plugin_context_to_string(parameter_handle),
    &ret_obj_ptr);

  LOG_DEBUG("int-appeared signal returned object ptr is %p", ret_obj_ptr);

  g_object_unref(hints_obj_ptr);

  *parameter_ui_context = ret_obj_ptr;
}

void
zynjacku_plugin_int_set(
  ZynjackuPlugin * plugin_obj_ptr,
  gchar * string_context,
  gint value)
{
  void * context;
  struct zynjacku_plugin * plugin_ptr;

  plugin_ptr = ZYNJACKU_PLUGIN_GET_PRIVATE(plugin_obj_ptr);

  context = zynjacku_plugin_context_from_string(string_context);

  LOG_DEBUG("zynjacku_plugin_int_set() called, context %p", context);

  if (plugin_ptr->dynparams != NULL)
  {
    dynparam_parameter_int_change(
      plugin_ptr->dynparams,
      (lv2dynparam_host_parameter)context,
      value);
  }
}
