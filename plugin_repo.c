/* -*- Mode: C ; c-basic-offset: 2 -*- */
/*****************************************************************************
 *
 *   This file is part of zynjacku
 *
 *   Copyright (C) 2006,2007,2008,2009 Nedko Arnaudov <nedko@arnaudov.name>
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <slv2/slv2.h>
#include <glib-object.h>
#if HAVE_DYNPARAMS
#include <lv2dynparam/lv2dynparam.h>
#include <lv2dynparam/lv2_rtmempool.h>
#include <lv2dynparam/host.h>
#endif
#include <jack/jack.h>

#include "lv2-miditype.h"
#include "lv2_event.h"
#include "lv2_string_port.h"
#include "lv2_uri_map.h"

#include "list.h"
#include "lv2.h"
#include "gtk2gui.h"
#include "zynjacku.h"
#include "plugin.h"
#include "plugin_internal.h"
#include "plugin_repo.h"
//#define LOG_LEVEL LOG_LEVEL_DEBUG
#include "log.h"

#define LV2_RDF_LICENSE_URI "http://usefulinc.com/ns/doap#license"
#define LV2_MIDI_PORT_URI "http://ll-plugins.nongnu.org/lv2/ext/MidiPort"
#define LV2_EVENT_PORT_URI LV2_EVENT_URI "#EventPort"
#define LV2_CONTEXT_URI "http://lv2plug.in/ns/dev/contexts"
#define LV2_PORT_CONTEXT_URI LV2_CONTEXT_URI "#context"
#define LV2_MESSAGE_CONTEXT_URI LV2_CONTEXT_URI "#MessageContext"
#define LV2_STRING_PORT_ROOT_URI "http://lv2plug.in/ns/dev/string-port#"
#define LV2_STRING_PORT_TYPE_URI LV2_STRING_PORT_ROOT_URI "StringPort"
#define LV2_STRING_PORT_DEFAULT_URI LV2_STRING_PORT_ROOT_URI "default"

struct zynjacku_plugin_info
{
  struct list_head siblings;
  SLV2Plugin slv2info;
  char * name;
  char * license;
  char * author;
  char * uri;
};

struct zynjacku_iterate_context
{
  float progress;
  float progress_step;
  const LV2_Feature * const * supported_features;
  void * context;
  zynjacku_plugin_repo_check_plugin check_plugin;
  zynjacku_plugin_repo_tick tick;
  zynjacku_plugin_repo_tack tack;
};

/* I would be useful if slv2_world_get_plugins_by_filter() had callback user context... */
/* this should really be parameter of slv2 filter plugins callback */
struct zynjacku_iterate_context g_iterate_context;

static struct list_head g_available_plugins; /* "struct zynjacku_plugin_info's linked by siblings */
static SLV2World g_world;
static bool g_loaded;
static bool g_fullscanned;
static SLV2Value g_slv2uri_port_input;
static SLV2Value g_slv2uri_port_output;
static SLV2Value g_slv2uri_port_control;
static SLV2Value g_slv2uri_port_audio;
static SLV2Value g_slv2uri_port_midi;
static SLV2Value g_slv2uri_port_event;
static SLV2Value g_slv2uri_port_context;
static SLV2Value g_slv2uri_message_context;
static SLV2Value g_slv2uri_license;
static SLV2Value g_slv2uri_event_midi;
static SLV2Value g_slv2uri_port_string;
static SLV2Value g_slv2uri_string_port_default;

/* as slv2_value_as_string() but returns NULL if value is NULL or value type is not string
   such conditions are assumed to be error, thus this function should be
   used only when caller expects value to be string */
const char *
slv2_value_as_string_smart(SLV2Value value)
{
  if (value == NULL)
  {
    LOG_ERROR("SLV2Value is NULL");
    return NULL;
  }

  if (!slv2_value_is_string(value))
  {
    LOG_ERROR("SLV2Value is not string");
    return NULL;
  }

  return slv2_value_as_string(value);
}

const char *
slv2_value_as_uri_smart(SLV2Value value)
{
  if (value == NULL)
  {
    LOG_ERROR("SLV2Value is NULL");
    return NULL;
  }

  if (!slv2_value_is_uri(value))
  {
    LOG_ERROR("SLV2Value is not string");
    return NULL;
  }

  return slv2_value_as_uri(value);
}

const char *
slv2_plugin_get_uri_smart(SLV2Plugin plugin)
{
  return slv2_value_as_uri(slv2_plugin_get_uri(plugin));
}

bool
slv2_port_is_control(
  SLV2Plugin plugin,
  SLV2Port port)
{
  return slv2_port_is_a(plugin, port, g_slv2uri_port_control);
}

bool
slv2_port_is_string(
  SLV2Plugin plugin,
  SLV2Port port)
{
  return slv2_port_is_a(plugin, port, g_slv2uri_port_string);
}

bool
slv2_port_is_audio(
  SLV2Plugin plugin,
  SLV2Port port)
{
  return slv2_port_is_a(plugin, port, g_slv2uri_port_audio);
}

bool
slv2_port_is_midi(
  SLV2Plugin plugin,
  SLV2Port port)
{
  return slv2_port_is_a(plugin, port, g_slv2uri_port_midi);
}

bool
slv2_port_is_event(
  SLV2Plugin plugin,
  SLV2Port port)
{
  return slv2_port_is_a(plugin, port, g_slv2uri_port_event);
}

bool
slv2_port_is_input(
  SLV2Plugin plugin,
  SLV2Port port)
{
  return slv2_port_is_a(plugin, port, g_slv2uri_port_input);
}

bool
slv2_port_is_output(
  SLV2Plugin plugin,
  SLV2Port port)
{
  return slv2_port_is_a(plugin, port, g_slv2uri_port_output);
}

bool
slv2_port_is_midi_event(
  SLV2Plugin plugin,
  SLV2Port port)
{
  return slv2_port_supports_event(plugin, port, g_slv2uri_event_midi);
}

struct uri_registration
{
  const char *name;
  SLV2Value *value;
};

static struct uri_registration uri_regs[] = {
  {SLV2_PORT_CLASS_INPUT, &g_slv2uri_port_input},
  {SLV2_PORT_CLASS_OUTPUT, &g_slv2uri_port_output},
  {SLV2_PORT_CLASS_CONTROL, &g_slv2uri_port_control},
  {SLV2_PORT_CLASS_AUDIO, &g_slv2uri_port_audio},
  {LV2_MIDI_PORT_URI, &g_slv2uri_port_midi},
  {LV2_RDF_LICENSE_URI, &g_slv2uri_license},
  {LV2_PORT_CONTEXT_URI, &g_slv2uri_port_context},
  {LV2_MESSAGE_CONTEXT_URI, &g_slv2uri_message_context},
  {LV2_EVENT_PORT_URI, &g_slv2uri_port_event},
  {LV2_EVENT_URI_TYPE_MIDI, &g_slv2uri_event_midi},
  {LV2_STRING_PORT_TYPE_URI, &g_slv2uri_port_string},
  {LV2_STRING_PORT_DEFAULT_URI, &g_slv2uri_string_port_default},
};

bool
zynjacku_plugin_repo_init()
{
  int i;
  g_world = slv2_world_new();
  if (g_world == NULL)
  {
    LOG_ERROR("slv2_world_new() failed.");
    goto fail;
  }

  INIT_LIST_HEAD(&g_available_plugins);
  g_fullscanned = false;
  g_loaded = false;

  for (i = 0; i < sizeof(uri_regs) / sizeof(struct uri_registration); i++)
  {
    *uri_regs[i].value = slv2_value_new_uri(g_world, uri_regs[i].name);
    if (!*uri_regs[i].value)
    {
      LOG_ERROR("slv2_value_new_uri() failed.");
      for (i--; i >= 0; i--)
        slv2_value_free(*uri_regs[i].value);
      goto fail_free_world;
    }
  }
  
  return true;

fail_free_world:
  slv2_world_free(g_world);

fail:
  return false;
}

void
zynjacku_plugin_repo_uninit()
{
  struct list_head * node_ptr;
  struct zynjacku_plugin_info * plugin_info_ptr;

  while(!list_empty(&g_available_plugins))
  {
    node_ptr = g_available_plugins.next;

    list_del(node_ptr);

    plugin_info_ptr = list_entry(node_ptr, struct zynjacku_plugin_info, siblings);

    //LOG_DEBUG("Removing %s", plugin_info_ptr->name);
    free(plugin_info_ptr->author);
    free(plugin_info_ptr->license);
    free(plugin_info_ptr->name);
    free(plugin_info_ptr);
  }

  slv2_value_free(g_slv2uri_event_midi);
  slv2_value_free(g_slv2uri_port_event);
  slv2_value_free(g_slv2uri_license);
  slv2_value_free(g_slv2uri_port_midi);
  slv2_value_free(g_slv2uri_port_audio);
  slv2_value_free(g_slv2uri_port_control);
  slv2_value_free(g_slv2uri_port_output);
  slv2_value_free(g_slv2uri_port_input);
  slv2_world_free(g_world);
}

char *
zynjacku_plugin_repo_get_plugin_license(
  SLV2Plugin plugin)
{
  SLV2Values slv2_values;
  SLV2Value slv2_value;
  char * license;
  const char * license_uri;

  slv2_values = slv2_plugin_get_value(
    plugin,
    g_slv2uri_license);

  if (slv2_values_size(slv2_values) == 0)
  {
    LOG_WARNING("Plugin license query returned empty set");
    return strdup("none");      /* acutally, slv2 should reject those early */
  }

  slv2_value = slv2_values_get_at(slv2_values, 0);
  if (!slv2_value_is_uri(slv2_value))
  {
    LOG_WARNING("Plugin license is not uri");
    return strdup("none");      /* acutally, slv2 should reject those early */
  }

  license_uri = slv2_value_as_string(slv2_value);

  if (strcmp(license_uri, "http://usefulinc.com/doap/licenses/gpl") == 0)
  {
    license = strdup("GNU General Public License");
  }
  else if (strcmp(license_uri, "http://usefulinc.com/doap/licenses/lgpl") == 0)
  {
    license = strdup("GNU Lesser General Public License");
  }
  else
  {
    license = strdup(license_uri);
  }

  slv2_values_free(slv2_values);

  return license;
}

char *
zynjacku_plugin_repo_get_plugin_author(
  SLV2Plugin plugin)
{
  SLV2Value slv2_value;
  char * author;
  const char * author_const;

  slv2_value = slv2_plugin_get_author_name(plugin);
  if (slv2_value != NULL)
  {
    author_const = slv2_value_as_string_smart(slv2_value);
  }
  else
  {
    author_const = NULL;
  }

  if (author_const != NULL)
  {
    author = strdup(author_const);
  }
  else
  {
    author = strdup("unknown");
  }

  return author;
}

/* check whether plugin is a synth, if it is, save plugin info */
static
bool
zynjacku_plugin_repo_check_and_maybe_init_plugin(
  SLV2Plugin plugin)
{
  gboolean ret;
  uint32_t audio_in_ports_count;
  uint32_t audio_out_ports_count;
  uint32_t midi_in_ports_count;
  uint32_t control_ports_count;
  uint32_t string_ports_count;
  uint32_t event_ports_count;
  uint32_t midi_event_in_ports_count;
  uint32_t ports_count;
  uint32_t port_index;
  const char *plugin_uri;
  const char *feature_uri;
  const char *name;
  SLV2Value slv2name;
  SLV2Values slv2features;
  SLV2Value slv2feature;
  unsigned int features_count;
  unsigned int feature_index;
  struct zynjacku_plugin_info * plugin_info_ptr;
  SLV2Port port;
  const LV2_Feature * const * feature_ptr_ptr;

  plugin_uri = slv2_plugin_get_uri_smart(plugin);

  if (g_iterate_context.tick != NULL)
  {
    g_iterate_context.tick(g_iterate_context.context, g_iterate_context.progress, plugin_uri);
  }

  ret = FALSE;

  slv2name = slv2_plugin_get_name(plugin);
  if (slv2name == NULL)
  {
    LOG_ERROR("slv2_plugin_get_name() returned NULL.");
    goto exit;
  }

  name = slv2_value_as_string_smart(slv2name);
  if (name == NULL)
  {
    LOG_ERROR("slv2_value_as_string_smart() failed for plugin name value.");
    goto free_name;
  }

  /* check required features */

  slv2features = slv2_plugin_get_required_features(plugin);
  features_count = slv2_values_size(slv2features);
  //LOG_DEBUG("Plugin \"%s\" has %u required features", name, features_count);

  for (feature_index = 0 ; feature_index < features_count ; feature_index++)
  {
    slv2feature = slv2_values_get_at(slv2features, feature_index);

    feature_uri = slv2_value_as_uri_smart(slv2feature);
    if (feature_uri == NULL)
    {
      LOG_ERROR("slv2_value_as_uri_smart() failed for plugin name value.");
      goto free_features;
    }

    LOG_DEBUG("%s", feature_uri);

    feature_ptr_ptr = g_iterate_context.supported_features;
    while (*feature_ptr_ptr != NULL)
    {
      if (strcmp((*feature_ptr_ptr)->URI, feature_uri) == 0)
      {
        break;
      }

      feature_ptr_ptr++;
    }

    if (*feature_ptr_ptr == NULL)
    {
      LOG_DEBUG("Plugin \"%s\" requires unsupported feature \"%s\"", name, feature_uri);
      goto free_features;
    }
  }

  /* check port configuration */

  ports_count = slv2_plugin_get_num_ports(plugin);
  audio_in_ports_count = slv2_plugin_get_num_ports_of_class(plugin, g_slv2uri_port_audio, g_slv2uri_port_input, NULL);
  audio_out_ports_count = slv2_plugin_get_num_ports_of_class(plugin, g_slv2uri_port_audio, g_slv2uri_port_output, NULL);
  midi_in_ports_count = slv2_plugin_get_num_ports_of_class(plugin, g_slv2uri_port_midi, g_slv2uri_port_input, NULL);
  control_ports_count = slv2_plugin_get_num_ports_of_class(plugin, g_slv2uri_port_control, NULL);
  string_ports_count = slv2_plugin_get_num_ports_of_class(plugin, g_slv2uri_port_string, NULL);
  event_ports_count = slv2_plugin_get_num_ports_of_class(plugin, g_slv2uri_port_event, NULL);

  midi_event_in_ports_count = 0;

  if (event_ports_count != 0)
  {
    for (port_index = 0 ; port_index < ports_count ; port_index++)
    {
      port = slv2_plugin_get_port_by_index(plugin, port_index);
      if (slv2_port_is_midi_event(plugin, port) && slv2_port_is_input(plugin, port))
      {
        midi_event_in_ports_count++;
      }
    }
  }

  if (!g_iterate_context.check_plugin(
        g_iterate_context.context,
        plugin_uri,
        name,
        audio_in_ports_count,
        audio_out_ports_count,
        midi_in_ports_count,
        control_ports_count,
        string_ports_count,
        event_ports_count,
        midi_event_in_ports_count,
        ports_count))
  {
    goto free_features;
  }

  plugin_info_ptr = malloc(sizeof(struct zynjacku_plugin_info));
  if (plugin_info_ptr == NULL)
  {
    LOG_ERROR("Cannot allocate memory for zynjacku_plugin_info structure");
    goto free_features;
  }

  plugin_info_ptr->name = strdup(name);
  if (plugin_info_ptr->name == NULL)
  {
    goto free_info;
  }

  plugin_info_ptr->uri = strdup(plugin_uri);
  if (plugin_info_ptr->uri == NULL)
  {
    goto free_info_name;
  }

  plugin_info_ptr->license = zynjacku_plugin_repo_get_plugin_license(plugin);
  if (plugin_info_ptr->license == NULL)
  {
    goto free_info_uri;
  }

  plugin_info_ptr->author = zynjacku_plugin_repo_get_plugin_author(plugin);
  if (plugin_info_ptr->author == NULL)
  {
    goto free_info_license;
  }

  plugin_info_ptr->slv2info = plugin;

  list_add_tail(&plugin_info_ptr->siblings, &g_available_plugins);

  if (g_iterate_context.tack != NULL)
  {
    g_iterate_context.tack(g_iterate_context.context, plugin_uri);
  }

  ret = TRUE;

  goto free_features;

free_info_license:
  free(plugin_info_ptr->license);

free_info_uri:
  free(plugin_info_ptr->uri);

free_info_name:
  free(plugin_info_ptr->name);

free_info:
  free(plugin_info_ptr);

free_features:
  slv2_values_free(slv2features);

free_name:
  slv2_value_free(slv2name);

exit:
  g_iterate_context.progress += g_iterate_context.progress_step;
  return ret;
}

void
zynjacku_plugin_repo_iterate(
  bool force_scan,
  const LV2_Feature * const * supported_features,
  void * context,
  zynjacku_plugin_repo_check_plugin check_plugin,
  zynjacku_plugin_repo_tick tick,
  zynjacku_plugin_repo_tack tack)
{
  struct list_head * node_ptr;
  SLV2Plugins slv2plugins;
  struct zynjacku_plugin_info * plugin_info_ptr;

  LOG_DEBUG("zynjacku_plugin_repo_iterate() called.");

  if (!force_scan && g_fullscanned)
  {
    if (tack != NULL)
    {
      LOG_DEBUG("Iterate existing plugins!");

      list_for_each(node_ptr, &g_available_plugins)
      {
        plugin_info_ptr = list_entry(node_ptr, struct zynjacku_plugin_info, siblings);
        tack(context, plugin_info_ptr->uri);
      }
    }

    if (tick != NULL)
    {
      tick(context, 1.0, "");
    }

    return;
  }

  /* scanned in past, clear world to scan again */
  zynjacku_plugin_repo_uninit();
  zynjacku_plugin_repo_init();

  LOG_DEBUG("Scanning plugins...");

  if (tick != NULL)
  {
    tick(context, 0.0, "Loading plugins (world) ...");
  }

  assert(!g_loaded);

  slv2_world_load_all(g_world);
  g_loaded = true;

  /* get plugins count */
  slv2plugins = slv2_world_get_all_plugins(g_world);
  g_iterate_context.progress_step = 1.0 / slv2_plugins_size(slv2plugins);
  slv2_plugins_free(g_world, slv2plugins);

  g_iterate_context.progress = 0.0;
  g_iterate_context.supported_features = supported_features;
  g_iterate_context.context = context;
  g_iterate_context.check_plugin = check_plugin;
  g_iterate_context.tick = tick;
  g_iterate_context.tack = tack;

  slv2plugins = slv2_world_get_plugins_by_filter(g_world, zynjacku_plugin_repo_check_and_maybe_init_plugin);
  slv2_plugins_free(g_world, slv2plugins);

  if (tick != NULL)
  {
    tick(context, 1.0, "");
  }

  g_fullscanned = true;
}

static
struct zynjacku_plugin_info *
zynjacku_plugin_repo_lookup_by_uri(
  const char * uri)
{
  struct list_head * node_ptr;
  struct zynjacku_plugin_info * plugin_info_ptr;

  list_for_each(node_ptr, &g_available_plugins)
  {
    plugin_info_ptr = list_entry(node_ptr, struct zynjacku_plugin_info, siblings);
    if (strcmp(plugin_info_ptr->uri, uri) == 0)
    {
      return plugin_info_ptr;
    }
  }

  LOG_ERROR("Unknown plugin '%s'", uri);
  return NULL;
}

const char *
zynjacku_plugin_repo_get_name(
  const char *uri)
{
  struct zynjacku_plugin_info * plugin_info_ptr;

  plugin_info_ptr = zynjacku_plugin_repo_lookup_by_uri(uri);
  if (plugin_info_ptr == NULL)
  {
    return NULL;
  }

  return plugin_info_ptr->name;
}

const char *
zynjacku_plugin_repo_get_license(
  const char *uri)
{
  struct zynjacku_plugin_info * plugin_info_ptr;

  plugin_info_ptr = zynjacku_plugin_repo_lookup_by_uri(uri);
  if (plugin_info_ptr == NULL)
  {
    return NULL;
  }

  return plugin_info_ptr->license;
}

const char *
zynjacku_plugin_repo_get_author(
  const char * uri)
{
  struct zynjacku_plugin_info * plugin_info_ptr;

  plugin_info_ptr = zynjacku_plugin_repo_lookup_by_uri(uri);
  if (plugin_info_ptr == NULL)
  {
    return NULL;
  }

  return plugin_info_ptr->author;
}

const char *
zynjacku_plugin_repo_get_dlpath(
  const char *uri)
{
  struct zynjacku_plugin_info * info_ptr;

  info_ptr = zynjacku_plugin_repo_lookup_by_uri(uri);
  if (info_ptr == NULL)
  {
    return NULL;
  }

  return slv2_uri_to_path(slv2_value_as_uri(slv2_plugin_get_library_uri(info_ptr->slv2info)));
}

const char *
zynjacku_plugin_repo_get_bundle_path(
  const char *uri)
{
  struct zynjacku_plugin_info * info_ptr;

  info_ptr = zynjacku_plugin_repo_lookup_by_uri(uri);
  if (info_ptr == NULL)
  {
    return NULL;
  }

  return slv2_uri_to_path(slv2_value_as_uri(slv2_plugin_get_bundle_uri(info_ptr->slv2info)));
}

static
struct zynjacku_port *
new_lv2parameter_port(
  struct zynjacku_plugin_info * info_ptr,
  SLV2Port port,
  uint32_t index,
  const char * symbol_str,
  struct zynjacku_plugin * plugin_ptr)
{
  struct zynjacku_port * port_ptr;
  SLV2Value name;
  const char * name_str;
  SLV2Values contexts;
  int i;

  port_ptr = malloc(sizeof(struct zynjacku_port));
  if (port_ptr == NULL)
  {
    LOG_ERROR("malloc() failed to allocate memory for struct zynjacku_port.");
    goto fail;
  }

  port_ptr->index = index;
  port_ptr->flags = 0;
  port_ptr->ui_context = NULL;
  port_ptr->plugin_ptr = plugin_ptr;
  port_ptr->midi_cc_map_obj_ptr = NULL;

  port_ptr->symbol = strdup(symbol_str);
  if (port_ptr->symbol == NULL)
  {
    LOG_ERROR("strdup() failed.");
    goto fail_free_port;
  }

  /* port name */
  name = slv2_port_get_name(info_ptr->slv2info, port);
  if (name == NULL)
  {
    LOG_ERROR("slv2_port_get_name() failed.");
    goto fail_free_symbol;
  }

  name_str = slv2_value_as_string_smart(name);
  if (name_str == NULL)
  {
    LOG_ERROR("port symbol is not string.");
    goto fail_free_symbol;
  }

  port_ptr->name = strdup(name_str);

  slv2_value_free(name);

  if (port_ptr->name == NULL)
  {
    LOG_ERROR("strdup() failed.");
    goto fail_free_symbol;
  }

  contexts = slv2_port_get_value(info_ptr->slv2info, port, g_slv2uri_port_context);
  for (i = 0; i < slv2_values_size(contexts); i++)
  {
    if (slv2_value_equals(slv2_values_get_at(contexts, i), g_slv2uri_message_context))
    {
      port_ptr->flags |= PORT_FLAGS_MSGCONTEXT;
      LOG_DEBUG("Port %d has message context", index);
      break;
    }
  }

  return port_ptr;
      
fail_free_symbol:
  free(port_ptr->symbol);

fail_free_port:
  free(port_ptr);

fail:
  return NULL;
}

static
bool
zynjacku_plugin_repo_create_port_internal(
  struct zynjacku_plugin_info * info_ptr,
  uint32_t port_index,
  struct zynjacku_plugin * plugin_ptr,
  void * context,
  zynjacku_plugin_repo_create_port create_port)
{
  struct zynjacku_port * port_ptr;
  SLV2Port port;
  SLV2Value symbol;
  const char * symbol_str;
  SLV2Value default_value;
  SLV2Value min_value;
  SLV2Value max_value;
  unsigned int port_type;
  bool output_port;
  SLV2Values defs;
  SLV2Value defval;
  const char * defval_str;
  size_t defval_len;

  port = slv2_plugin_get_port_by_index(info_ptr->slv2info, port_index);

  output_port = slv2_port_is_output(info_ptr->slv2info, port);

  /* port symbol */
  symbol = slv2_port_get_symbol(info_ptr->slv2info, port);
  if (symbol == NULL)
  {
    LOG_ERROR("slv2_port_get_symbol() failed.");
    return false;
  }

  symbol_str = slv2_value_as_string_smart(symbol);
  if (symbol_str == NULL)
  {
    LOG_ERROR("port symbol is not string.");
    return false;
  }

  if (slv2_port_is_control(info_ptr->slv2info, port))
  {
    port_ptr = new_lv2parameter_port(info_ptr, port, port_index, symbol_str, plugin_ptr);

    port_ptr->type = PORT_TYPE_LV2_FLOAT;

    if (output_port)
    {
      port_ptr->flags |= PORT_FLAGS_OUTPUT;
      list_add_tail(&port_ptr->plugin_siblings, &plugin_ptr->measure_ports);
      return true;
    }

    /* port range */
    slv2_port_get_range(
      info_ptr->slv2info,
      port,
      &default_value,
      &min_value,
      &max_value);

    if (default_value != NULL)
    {
      port_ptr->data.lv2float.value = slv2_value_as_float(default_value);
      slv2_value_free(default_value);
    }

    if (min_value != NULL)
    {
      port_ptr->data.lv2float.min = slv2_value_as_float(min_value);
      slv2_value_free(min_value);
    }

    if (max_value != NULL)
    {
      port_ptr->data.lv2float.max = slv2_value_as_float(max_value);
      slv2_value_free(max_value);
    }

    list_add_tail(&port_ptr->plugin_siblings, &plugin_ptr->parameter_ports);

    return true;
  }

  if (slv2_port_is_string(info_ptr->slv2info, port))
  {
    if (output_port)
    {
      /* TODO measure string ports are ignored for now */
      return true;
    }

    port_ptr = new_lv2parameter_port(info_ptr, port, port_index, symbol_str, plugin_ptr);

    port_ptr->type = PORT_TYPE_LV2_STRING;

    /* TODO: get from slv2 (requiredSpace) */
    port_ptr->data.lv2string.storage = 256;

    defval_str = "\0";

    /* get default */
    defs = slv2_port_get_value(info_ptr->slv2info, port, g_slv2uri_string_port_default);
    if (defs && slv2_values_size(defs) == 1)
    {
      defval = slv2_values_get_at(defs, 0);
      if (slv2_value_is_string(defval))
      {
        defval_str = slv2_value_as_string(defval);
      }
    }

    defval_len = strlen(defval_str) + 1;

    if (defval_len > port_ptr->data.lv2string.storage)
    {
      port_ptr->data.lv2string.storage = defval_len;
    }

    port_ptr->data.lv2string.data = malloc(port_ptr->data.lv2string.storage);
    memcpy(port_ptr->data.lv2string.data, defval_str, defval_len);

    port_ptr->data.lv2string.len = defval_len - 1;
    port_ptr->data.lv2string.flags = LV2_STRING_DATA_CHANGED_FLAG;
    port_ptr->data.lv2string.pad = 0;

    list_add_tail(&port_ptr->plugin_siblings, &plugin_ptr->parameter_ports);

    return true;
  }

  if (slv2_port_is_audio(info_ptr->slv2info, port))
  {
    port_type = PORT_TYPE_AUDIO;
  }
  else if (slv2_port_is_midi(info_ptr->slv2info, port))
  {
    port_type = PORT_TYPE_MIDI;
  }
  else if (slv2_port_is_event(info_ptr->slv2info, port) &&
           slv2_port_is_midi_event(info_ptr->slv2info, port))
  {
    port_type = PORT_TYPE_EVENT_MIDI;
  }
  else
  {
    LOG_ERROR("Unrecognized port '%s' type (index is %u)", slv2_value_as_string_smart(symbol), (unsigned int)port_index);
    return false;
  }

  if (create_port(
        context,
        port_type,
        output_port,
        port_index))
  {
    return true;
  }

  LOG_ERROR("Unmatched port '%s'. type is %u, index is %u", slv2_value_as_string_smart(symbol), (unsigned int)port_type, (unsigned int)port_index);
  return false;
}

bool
zynjacku_plugin_repo_load_plugin(
  struct zynjacku_plugin * synth_ptr,
  void * context,
  zynjacku_plugin_repo_create_port create_port,
  zynjacku_plugin_repo_check_plugin check_plugin,
  const LV2_Feature * const * supported_features)
{
  struct zynjacku_plugin_info * info_ptr;
  SLV2Values slv2features;
  SLV2Value slv2feature;
  unsigned int features_count;
  unsigned int feature_index;
  bool ret;
  const char *uri;
  uint32_t ports_count;
  uint32_t i;
  SLV2Plugins slv2plugins;
  SLV2Plugin slv2plugin;
  SLV2Value uri_value;

  ret = false;

  LOG_DEBUG("zynjacku_plugin_repo_load_plugin() called.");

#if HAVE_DYNPARAMS
  synth_ptr->dynparams_supported = FALSE;
#endif

  if (!g_fullscanned)
  {
    if (!g_loaded)
    {
      slv2_world_load_all(g_world);
      g_loaded = true;
    }

    g_iterate_context.supported_features = supported_features;
    g_iterate_context.context = context;
    g_iterate_context.check_plugin = check_plugin;
    g_iterate_context.progress_step = 0.0;
    g_iterate_context.progress = 0.0;
    g_iterate_context.tick = NULL;
    g_iterate_context.tack = NULL;

    slv2plugins = slv2_world_get_all_plugins(g_world);

    uri_value = slv2_value_new_uri(g_world, synth_ptr->uri);

    slv2plugin = slv2_plugins_get_by_uri(slv2plugins, uri_value);
    if (slv2plugin == NULL)
    {
      slv2_value_free(uri_value);
      slv2_plugins_free(g_world, slv2plugins);
      LOG_ERROR("Plugin '%s' not found", synth_ptr->uri);
      goto exit;
    }

    ret = zynjacku_plugin_repo_check_and_maybe_init_plugin(slv2plugin);

    slv2_value_free(uri_value);

    slv2_plugins_free(g_world, slv2plugins);

    if (!ret)
    {
      LOG_ERROR("plugin '%s' failed to match synth constraints", synth_ptr->uri);
      goto exit;
    }

    /* MAYBE: return info_ptr from zynjacku_plugin_repo_check_and_maybe_init_plugin()
       so we dont lookup it in next line, by calling zynjacku_plugin_repo_lookup_by_uri() */
  }

  info_ptr = zynjacku_plugin_repo_lookup_by_uri(synth_ptr->uri);
  if (info_ptr == NULL)
  {
    LOG_ERROR("Failed to find plugin %s", synth_ptr->uri);
    goto exit;
  }

  synth_ptr->name = strdup(info_ptr->name);
  if (synth_ptr->name == NULL)
  {
    LOG_ERROR("Failed to strdup('%s')", info_ptr->name);
    goto exit;
  }

  slv2features = slv2_plugin_get_optional_features(info_ptr->slv2info);

  features_count = slv2_values_size(slv2features);
  LOG_DEBUG("Plugin has %u optional features", features_count);
  for (feature_index = 0 ; feature_index < features_count ; feature_index++)
  {
    slv2feature = slv2_values_get_at(slv2features, feature_index);

    uri = slv2_value_as_uri_smart(slv2feature);
    if (uri == NULL)
    {
      LOG_ERROR("slv2_value_as_uri_smart() failed for plugin name value.");
      goto free_features;
    }

    LOG_DEBUG("%s", uri);

#if HAVE_DYNPARAMS
    if (strcmp(LV2DYNPARAM_URI, uri) == 0)
    {
      synth_ptr->dynparams_supported = TRUE;
    }
#endif
  }

  ports_count  = slv2_plugin_get_num_ports(info_ptr->slv2info);

  for (i = 0 ; i < ports_count ; i++)
  {
    if (!zynjacku_plugin_repo_create_port_internal(info_ptr, i, synth_ptr, context, create_port))
    {
      LOG_ERROR("Failed to create plugin port");
      goto free_features;
    }
  }

  ret = true;

free_features:
  slv2_values_free(slv2features);

exit:
  return ret;
}

static
const char *
uri_to_fs_path(
  const char * uri)
{
  if (uri == NULL)
  {
    return NULL;
  }

  if (strlen(uri) <= 8 || memcmp(uri, "file:///", 8) != 0)
  {
    return NULL;
  }

  return uri + 7;
}

bool
zynjacku_plugin_repo_get_ui_info(
  const char * plugin_uri,
  const char * ui_type_uri,
  char ** ui_uri_ptr,
  char ** ui_binary_path_ptr,
  char ** ui_bundle_path_ptr)
{
  struct zynjacku_plugin_info * plugin_info_ptr;
  SLV2UIs slv2uis;
  SLV2UI slv2ui;
  SLV2Value ui_type;
  SLV2Value ui_uri;
  SLV2Value ui_binary_uri;
  SLV2Value ui_bundle_uri;
  const char * ui_uri_str;
  const char * ui_binary_path;
  const char * ui_bundle_path;
  bool ret;

  LOG_DEBUG("zynjacku_plugin_repo_get_ui_info() called.");

  ret = false;

  ui_type = slv2_value_new_uri(g_world, ui_type_uri);
  if (ui_type == NULL)
  {
    LOG_ERROR("slv2_value_new_uri() failed.");
    goto fail;
  }

  plugin_info_ptr = zynjacku_plugin_repo_lookup_by_uri(plugin_uri);
  if (plugin_info_ptr == NULL)
  {
    LOG_ERROR("Unknown plugin '%s'", plugin_uri);
    goto fail_free_ui_type;
  }

  slv2uis = slv2_plugin_get_uis(plugin_info_ptr->slv2info);

  if (slv2_uis_size(slv2uis) == 0)
  {
    LOG_DEBUG("Plugin '%s' has no UIs", plugin_uri);
    goto fail_free_uis;
  }

  slv2ui = slv2_uis_get_at(slv2uis, 0);
  if (slv2ui == NULL)
  {
    LOG_ERROR("slv2_uis_get_at() failed with plugin '%s'", plugin_uri);
    goto fail_free_uis;
  }

  if (!slv2_ui_is_a(slv2ui, ui_type))
  {
    LOG_DEBUG("First UI of '%s' is not '%s'", plugin_uri, ui_type_uri);
    goto fail_free_uis;
  }

  ui_uri = slv2_ui_get_uri(slv2ui);
  ui_binary_uri = slv2_ui_get_binary_uri(slv2ui);
  ui_bundle_uri = slv2_ui_get_bundle_uri(slv2ui);

  ui_uri_str = slv2_value_as_uri_smart(ui_uri);
  ui_binary_path = uri_to_fs_path(slv2_value_as_uri_smart(ui_binary_uri));
  ui_bundle_path = uri_to_fs_path(slv2_value_as_uri_smart(ui_bundle_uri));

  if (ui_uri_str == NULL)
  {
    LOG_ERROR("Failed to retrieve UI URI of '%s'", plugin_uri);
    goto fail_free_uis;
  }

  if (ui_binary_uri == NULL)
  {
    LOG_ERROR("Failed to retrieve UI binary path of '%s'", plugin_uri);
    goto fail_free_uis;
  }

  if (ui_bundle_uri == NULL)
  {
    LOG_ERROR("Failed to retrieve UI bundle path of '%s'", plugin_uri);
    goto fail_free_uis;
  }

  LOG_DEBUG("UI URI is '%s'", ui_uri_str);
  LOG_DEBUG("UI binary URI is '%s'", ui_binary_path);
  LOG_DEBUG("UI bundle URI is '%s'", ui_bundle_path);

  *ui_uri_ptr = strdup(ui_uri_str);
  if (*ui_uri_ptr == NULL)
  {
    LOG_ERROR("strdup() failed");
    goto fail_free_uis;
  }

  *ui_binary_path_ptr = strdup(ui_binary_path);
  if (*ui_binary_path_ptr == NULL)
  {
    LOG_ERROR("strdup() failed");
    free(*ui_uri_ptr);
    goto fail_free_uis;
  }

  *ui_bundle_path_ptr = strdup(ui_bundle_path);
  if (*ui_bundle_path_ptr == NULL)
  {
    LOG_ERROR("strdup() failed");
    free(*ui_binary_path_ptr);
    goto fail_free_uis;
  }

  ret = true;

fail_free_uis:
  slv2_uis_free(slv2uis);

fail_free_ui_type:
  slv2_value_free(ui_type);

fail:
  return ret;
}
