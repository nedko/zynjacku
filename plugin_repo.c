/* -*- Mode: C ; c-basic-offset: 2 -*- */
/*****************************************************************************
 *
 *   This file is part of zynjacku
 *
 *   Copyright (C) 2006,2007,2008 Nedko Arnaudov <nedko@arnaudov.name>
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
#include <stdlib.h>
#include <string.h>
#include <slv2/slv2.h>
#include <glib-object.h>
#include <lv2dynparam/lv2dynparam.h>
#include <lv2dynparam/lv2_rtmempool.h>
#include <lv2dynparam/host.h>
#include <jack/jack.h>

#include "lv2-miditype.h"
#include "list.h"
#include "gtk2gui.h"
#include "lv2.h"
#include "zynjacku.h"
#include "plugin_repo.h"
//#define LOG_LEVEL LOG_LEVEL_DEBUG
#include "log.h"

#define LV2_RDF_LICENSE_URI "http://usefulinc.com/ns/doap#license"
#define LV2_MIDI_PORT_URI "http://ll-plugins.nongnu.org/lv2/ext/MidiPort"

struct zynjacku_plugin_info
{
  struct list_head siblings;
  SLV2Plugin slv2info;
  char * name;
  char * license;
  char * uri;
};

struct zynjacku_iterate_context
{
  float progress;
  float progress_step;
  void *context;
  zynjacku_plugin_repo_tick tick;
  zynjacku_plugin_repo_tack tack;
};

/* this should really be parameter of slv2 filter plugins callback */
struct zynjacku_iterate_context g_iterate_context;

static struct list_head g_available_plugins; /* "struct zynjacku_plugin_info's linked by siblings */
static SLV2World g_world;
static SLV2Plugins g_plugins;
static SLV2Value g_slv2uri_port_input;
static SLV2Value g_slv2uri_port_output;
static SLV2Value g_slv2uri_port_control;
static SLV2Value g_slv2uri_port_audio;
static SLV2Value g_slv2uri_port_midi;
static SLV2Value g_slv2uri_license;

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
zynjacku_plugin_repo_init()
{
  g_world = slv2_world_new();
  if (g_world == NULL)
  {
    LOG_ERROR("slv2_world_new() failed.");
    goto fail;
  }

  INIT_LIST_HEAD(&g_available_plugins);
  g_plugins = NULL;

  g_slv2uri_port_input = slv2_value_new_uri(g_world, SLV2_PORT_CLASS_INPUT);
  if (g_slv2uri_port_input == NULL)
  {
    LOG_ERROR("slv2_value_new_uri() failed.");
    goto fail_free_world;
  }

  g_slv2uri_port_output = slv2_value_new_uri(g_world, SLV2_PORT_CLASS_OUTPUT);
  if (g_slv2uri_port_output == NULL)
  {
    LOG_ERROR("slv2_value_new_uri() failed.");
    goto fail_free_port_input;
  }

  g_slv2uri_port_control = slv2_value_new_uri(g_world, SLV2_PORT_CLASS_CONTROL);
  if (g_slv2uri_port_control == NULL)
  {
    LOG_ERROR("slv2_value_new_uri() failed.");
    goto fail_free_port_output;
  }

  g_slv2uri_port_audio = slv2_value_new_uri(g_world, SLV2_PORT_CLASS_AUDIO);
  if (g_slv2uri_port_audio == NULL)
  {
    LOG_ERROR("slv2_value_new_uri() failed.");
    goto fail_free_port_control;
  }

  g_slv2uri_port_midi = slv2_value_new_uri(g_world, LV2_MIDI_PORT_URI);
  if (g_slv2uri_port_midi == NULL)
  {
    LOG_ERROR("slv2_value_new_uri() failed.");
    goto fail_free_port_audio;
  }

  g_slv2uri_license = slv2_value_new_uri(g_world, LV2_RDF_LICENSE_URI);
  if (g_slv2uri_license == NULL)
  {
    LOG_ERROR("slv2_value_new_uri() failed.");
    goto fail_free_port_midi;
  }

  return true;

fail_free_port_midi:
  slv2_value_free(g_slv2uri_port_midi);

fail_free_port_audio:
  slv2_value_free(g_slv2uri_port_audio);

fail_free_port_control:
  slv2_value_free(g_slv2uri_port_control);

fail_free_port_output:
  slv2_value_free(g_slv2uri_port_output);

fail_free_port_input:
  slv2_value_free(g_slv2uri_port_input);

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
    free(plugin_info_ptr->license);
    free(plugin_info_ptr->name);
    free(plugin_info_ptr);
  }

  if (g_plugins != NULL)
  {
    slv2_plugins_free(g_world, g_plugins);
  }

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

  license = strdup(slv2_value_as_string(slv2_value));

  slv2_values_free(slv2_values);

  return license;
}

/* check whether plugin is a synth, if it is, save plugin info */
bool
zynjacku_plugin_repo_check_plugin(
  SLV2Plugin plugin)
{
  gboolean ret;
  uint32_t audio_out_ports_count;
  uint32_t midi_in_ports_count;
  uint32_t control_ports_count;
  uint32_t ports_count;
  const char *plugin_uri;
  const char *feature_uri;
  const char *name;
  SLV2Value slv2name;
  SLV2Values slv2features;
  SLV2Value slv2feature;
  unsigned int features_count;
  unsigned int feature_index;
  struct zynjacku_plugin_info * plugin_info_ptr;

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

    if (strcmp(LV2DYNPARAM_URI, feature_uri) == 0)
    {
      continue;
    }

    if (strcmp(LV2_RTSAFE_MEMORY_POOL_URI, feature_uri) == 0)
    {
      continue;
    }

    LOG_DEBUG("Plugin \"%s\" requires unsupported feature \"%s\"", name, feature_uri);
    goto free_features;
  }

  /* check port configuration */

  ports_count = slv2_plugin_get_num_ports(plugin);
  audio_out_ports_count = slv2_plugin_get_num_ports_of_class(plugin, g_slv2uri_port_audio, g_slv2uri_port_output, NULL);
  midi_in_ports_count = slv2_plugin_get_num_ports_of_class(plugin, g_slv2uri_port_midi, g_slv2uri_port_input, NULL);
  control_ports_count = slv2_plugin_get_num_ports_of_class(plugin, g_slv2uri_port_control, NULL);

  if (midi_in_ports_count + control_ports_count + audio_out_ports_count != ports_count ||
      midi_in_ports_count != 1 ||
      audio_out_ports_count == 0)
  {
    LOG_DEBUG("Skipping \"%s\" %s, plugin with unsupported port configuration", name, uri);
    LOG_DEBUG("  midi input ports: %d", (unsigned int)midi_in_ports_count);
    LOG_DEBUG("  control ports: %d", (unsigned int)control_ports_count);
    LOG_DEBUG("  audio output ports: %d", (unsigned int)audio_out_ports_count);
    LOG_DEBUG("  total ports %d", (unsigned int)ports_count);
    goto free_features;
  }

  LOG_DEBUG("Found \"%s\" %s", name, uri);
  LOG_DEBUG("  midi input ports: %d", (unsigned int)midi_in_ports_count);
  LOG_DEBUG("  control ports: %d", (unsigned int)control_ports_count);
  LOG_DEBUG("  audio output ports: %d", (unsigned int)audio_out_ports_count);
  LOG_DEBUG("  total ports %d", (unsigned int)ports_count);

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

  plugin_info_ptr->slv2info = plugin;

  list_add_tail(&plugin_info_ptr->siblings, &g_available_plugins);

  if (g_iterate_context.tack != NULL)
  {
    g_iterate_context.tack(g_iterate_context.context, plugin_uri);
  }

  ret = TRUE;

  goto free_features;

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
  void *context,
  zynjacku_plugin_repo_tick tick,
  zynjacku_plugin_repo_tack tack)
{
  struct list_head * node_ptr;
  SLV2Plugins slv2plugins;
  struct zynjacku_plugin_info * plugin_info_ptr;

  LOG_DEBUG("zynjacku_plugin_repo_iterate() called.");

  if (force_scan)
  {
    /* scanned in past, clear world to scan again */
    zynjacku_plugin_repo_uninit();
    zynjacku_plugin_repo_init();
  }
  else if (g_plugins != NULL)
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

    return;
  }

  LOG_DEBUG("Scanning plugins...");

  if (tick != NULL)
  {
    tick(context, 0.0, "Loading plugins (world) ...");
  }

  slv2_world_load_all(g_world);

  /* get plugins count */
  slv2plugins = slv2_world_get_all_plugins(g_world);
  g_iterate_context.progress_step = 1.0 / slv2_plugins_size(slv2plugins);
  slv2_plugins_free(g_world, slv2plugins);

  g_iterate_context.progress = 0.0;
  g_iterate_context.context = context;
  g_iterate_context.tick = tick;
  g_iterate_context.tack = tack;

  slv2plugins = slv2_world_get_plugins_by_filter(g_world, zynjacku_plugin_repo_check_plugin);
  slv2_plugins_free(g_world, slv2plugins);

  if (tick != NULL)
  {
    tick(context, 1.0, "");
  }
}

static
struct zynjacku_plugin_info *
zynjacku_plugin_repo_lookup_by_uri(const char * uri)
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

bool
zynjacku_plugin_repo_create_port(
  struct zynjacku_plugin_info *info_ptr,
  uint32_t port_index,
  struct zynjacku_synth *synth_ptr)
{
  SLV2Value symbol;
  struct zynjacku_synth_port * port_ptr;
  SLV2Port port;
  SLV2Value default_value;
  SLV2Value min_value;
  SLV2Value max_value;

  port = slv2_plugin_get_port_by_index(info_ptr->slv2info, port_index);

  /* Get the port symbol (label) for console printing */
  symbol = slv2_port_get_symbol(info_ptr->slv2info, port);
  if (symbol == NULL)
  {
    LOG_ERROR("slv2_port_get_symbol() failed.");
    return FALSE;
  }

  if (slv2_port_is_a(info_ptr->slv2info, port, g_slv2uri_port_control))
  {
    if (!slv2_port_is_a(info_ptr->slv2info, port, g_slv2uri_port_input))
    {
      /* ignore output control ports, we dont support them yet */
      return true;
    }

    port_ptr = malloc(sizeof(struct zynjacku_synth_port));
    if (port_ptr == NULL)
    {
      LOG_ERROR("malloc() failed.");
      return false;
    }

    port_ptr->type = PORT_TYPE_PARAMETER;
    port_ptr->index = port_index;

    slv2_port_get_range(
      info_ptr->slv2info,
      port,
      &default_value,
      &min_value,
      &max_value);

    if (default_value != NULL)
    {
      port_ptr->data.parameter.value = slv2_value_as_float(default_value);
      slv2_value_free(default_value);
    }

    if (min_value != NULL)
    {
      port_ptr->data.parameter.min = slv2_value_as_float(min_value);
      slv2_value_free(min_value);
    }

    if (max_value != NULL)
    {
      port_ptr->data.parameter.max = slv2_value_as_float(max_value);
      slv2_value_free(max_value);
    }

    list_add_tail(&port_ptr->plugin_siblings, &synth_ptr->parameter_ports);

    return true;
  }

  if (slv2_port_is_a(info_ptr->slv2info, port, g_slv2uri_port_audio) &&
      slv2_port_is_a(info_ptr->slv2info, port, g_slv2uri_port_output))
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
    port_ptr->index = port_index;

    return true;
  }

  if (slv2_port_is_a(info_ptr->slv2info, port, g_slv2uri_port_midi) &&
      slv2_port_is_a(info_ptr->slv2info, port, g_slv2uri_port_input))
  {
    port_ptr = &synth_ptr->midi_in_port;
    port_ptr->type = PORT_TYPE_MIDI;
    port_ptr->index = port_index;
    return true;
  }

  LOG_ERROR("Unrecognized port '%s' type (index is %u)", symbol, (unsigned int)port_index);
  return false;
}

bool
zynjacku_plugin_repo_load_synth(
  struct zynjacku_synth * synth_ptr)
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

  ret = false;

  synth_ptr->dynparams_supported = FALSE;

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

    if (strcmp(LV2DYNPARAM_URI, uri) == 0)
    {
      synth_ptr->dynparams_supported = TRUE;
    }
  }

  ports_count  = slv2_plugin_get_num_ports(info_ptr->slv2info);

  for (i = 0 ; i < ports_count ; i++)
  {
    if (!zynjacku_plugin_repo_create_port(info_ptr, i, synth_ptr))
    {
      LOG_ERROR("Failed to create plugin port");
    }
  }

free_features:
  slv2_values_free(slv2features);

exit:
  return ret;
}
