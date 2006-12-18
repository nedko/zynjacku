/* -*- Mode: C ; c-basic-offset: 2 -*- */
/*****************************************************************************
 *
 *   This file is part of zynjacku
 *
 *   Copyright (C) 2006 Nedko Arnaudov <nedko@arnaudov.name>
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

#include "list.h"
#include "plugin_repo.h"
#include "log.h"

struct zynjacku_simple_plugin_info
{
  struct list_head siblings;
  SLV2Plugin * plugin_ptr;
};

struct zynjacku_plugin_repo
{
  gboolean dispose_has_run;

  struct list_head available_plugins; /* "struct zynjacku_simple_plugin_info"s linked by siblings */
};

#define ZYNJACKU_PLUGIN_REPO_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), ZYNJACKU_PLUGIN_REPO_TYPE, struct zynjacku_plugin_repo))

static void
zynjacku_plugin_repo_dispose(GObject * obj)
{
  struct zynjacku_plugin_repo * plugin_repo_ptr;

  plugin_repo_ptr = ZYNJACKU_PLUGIN_REPO_GET_PRIVATE(obj);

  LOG_DEBUG("zynjacku_plugin_repo_dispose() called.");

  if (plugin_repo_ptr->dispose_has_run)
  {
    /* If dispose did already run, return. */
    LOG_DEBUG("zynjacku_plugin_repo_dispose() already run!");
    return;
  }

  /* Make sure dispose does not run twice. */
  plugin_repo_ptr->dispose_has_run = TRUE;

  /* 
   * In dispose, you are supposed to free all types referenced from this
   * object which might themselves hold a reference to self. Generally,
   * the most simple solution is to unref all members on which you own a 
   * reference.
   */

  /* Chain up to the parent class */
  G_OBJECT_CLASS(g_type_class_peek_parent(G_OBJECT_GET_CLASS(obj)))->dispose(obj);
}

static void
zynjacku_plugin_repo_finalize(GObject * obj)
{
//  struct zynjacku_plugin_repo * self = ZYNJACKU_PLUGIN_REPO_GET_PRIVATE(obj);

  LOG_DEBUG("zynjacku_plugin_repo_finalize() called.");

  /*
   * Here, complete object destruction.
   * You might not need to do much...
   */
  //g_free(self->private);

  /* Chain up to the parent class */
  G_OBJECT_CLASS(g_type_class_peek_parent(G_OBJECT_GET_CLASS(obj)))->finalize(obj);
}

static void
zynjacku_plugin_repo_class_init(
  gpointer class_ptr,
  gpointer class_data_ptr)
{
  LOG_DEBUG("zynjacku_plugin_repo_class() called.");

  G_OBJECT_CLASS(class_ptr)->dispose = zynjacku_plugin_repo_dispose;
  G_OBJECT_CLASS(class_ptr)->finalize = zynjacku_plugin_repo_finalize;

  g_type_class_add_private(G_OBJECT_CLASS(class_ptr), sizeof(struct zynjacku_plugin_repo));
}

static void
zynjacku_plugin_repo_init(
  GTypeInstance * instance,
  gpointer g_class)
{
  struct zynjacku_plugin_repo * plugin_repo_ptr;

  LOG_DEBUG("zynjacku_plugin_repo_init() called.");

  plugin_repo_ptr = ZYNJACKU_PLUGIN_REPO_GET_PRIVATE(instance);
}

GType zynjacku_plugin_repo_get_type()
{
  static GType type = 0;
  if (type == 0)
  {
    type = g_type_register_static_simple(
      G_TYPE_OBJECT,
      "zynjacku_plugin_repo_type",
      sizeof(ZynjackuPluginRepoClass),
      zynjacku_plugin_repo_class_init,
      sizeof(ZynjackuPluginRepo),
      zynjacku_plugin_repo_init,
      0);
  }

  return type;
}

ZynjackuPluginRepo *
zynjacku_plugin_repo_get()
{
  return NULL;
}

void
zynjacku_find_simple_plugins()
{
  SLV2List plugins;
  size_t i;
  uint32_t ports_count;
  uint32_t port_index;
  const SLV2Plugin * plugin_ptr;
  size_t plugins_count;
  uint32_t audio_out_ports_count;
  uint32_t midi_in_ports_count;
  enum SLV2PortClass class;
  char * type;
  char * name;
  struct zynjacku_simple_plugin_info * plugin_info_ptr;

  plugins = slv2_list_new();
  slv2_list_load_all(plugins);
  plugins_count = slv2_list_get_length(plugins);

  for (i = 0 ; i < plugins_count; i++)
  {
    plugin_ptr = slv2_list_get_plugin_by_index(plugins, i);

    name = slv2_plugin_get_name(plugin_ptr);

    if (!slv2_plugin_verify(plugin_ptr))
    {
      LOG_DEBUG("Skipping slv2 verify failed \"%s\" %s", name, slv2_plugin_get_uri(plugin_ptr));
      goto next_plugin;
    }

    ports_count = slv2_plugin_get_num_ports(plugin_ptr);
    audio_out_ports_count = 0;
    midi_in_ports_count = 0;

    for (port_index = 0 ; port_index < ports_count ; port_index++)
    {
      class = slv2_port_get_class(plugin_ptr, port_index);
      type = slv2_port_get_data_type(plugin_ptr, port_index);

      if (strcmp(type, SLV2_DATA_TYPE_FLOAT) == 0)
      {
        if (class == SLV2_CONTROL_RATE_INPUT)
        {
        }
        else if (class == SLV2_AUDIO_RATE_OUTPUT)
        {
          if (audio_out_ports_count == 2)
          {
            LOG_DEBUG("Skipping \"%s\" %s, plugin with control output port", name, slv2_plugin_get_uri(plugin_ptr));
            goto next_plugin;
          }

          audio_out_ports_count++;
        }
        else if (class == SLV2_AUDIO_RATE_INPUT)
        {
          LOG_DEBUG("Skipping \"%s\" %s, plugin with audio input port", name, slv2_plugin_get_uri(plugin_ptr));
          goto next_plugin;
        }
        else
        {
          LOG_DEBUG("Skipping \"%s\" %s, plugin with control (output?) port", name, slv2_plugin_get_uri(plugin_ptr));
          goto next_plugin;
        }
      }
      else if (strcmp(type, SLV2_DATA_TYPE_MIDI) == 0)
      {
        if (class == SLV2_CONTROL_RATE_INPUT)
        {
          if (midi_in_ports_count == 1)
          {
            LOG_DEBUG("Skipping \"%s\" %s, plugin with more than one MIDI input port", name, slv2_plugin_get_uri(plugin_ptr));
            goto next_plugin;
          }

          midi_in_ports_count++;
        }
        else
        {
          LOG_DEBUG("Skipping \"%s\" %s, plugin with MIDI (output?) port", name, slv2_plugin_get_uri(plugin_ptr));
          goto next_plugin;
        }
      }
      else
      {
        LOG_DEBUG("Skipping \"%s\" %s, plugin unknown type \"%s\" port", name, slv2_plugin_get_uri(plugin_ptr), type);
        goto next_plugin;
      }
    }

    if (audio_out_ports_count == 0)
    {
      LOG_DEBUG("Skipping \"%s\" %s, plugin without audio output ports", name, slv2_plugin_get_uri(plugin_ptr));
      goto next_plugin;
    }

    LOG_DEBUG("Found \"%s\" %s", name, slv2_plugin_get_uri(plugin_ptr));
    plugin_info_ptr = malloc(sizeof(struct zynjacku_simple_plugin_info));
    plugin_info_ptr->plugin_ptr = slv2_plugin_duplicate(plugin_ptr);
    list_add_tail(&plugin_info_ptr->siblings, &g_available_plugins);

  next_plugin:
    free(name);
  }

  slv2_list_free(plugins);
}

void
zynjacku_find_all_plugins()
{
  SLV2List plugins;
  size_t i;
  const SLV2Plugin * plugin_ptr;
  size_t plugins_count;
  struct zynjacku_simple_plugin_info * plugin_info_ptr;

  plugins = slv2_list_new();
  slv2_list_load_all(plugins);
  plugins_count = slv2_list_get_length(plugins);

  for (i = 0 ; i < plugins_count; i++)
  {
    plugin_ptr = slv2_list_get_plugin_by_index(plugins, i);
    plugin_info_ptr = malloc(sizeof(struct zynjacku_simple_plugin_info));
    plugin_info_ptr->plugin_ptr = slv2_plugin_duplicate(plugin_ptr);
    list_add_tail(&plugin_info_ptr->siblings, &g_available_plugins);
  }

  slv2_list_free(plugins);
}

SLV2Plugin *
zynjacku_plugin_repo_lookup_by_uri_list(const char * uri)
{
  struct list_head * node_ptr;
  const char * current_uri;
  struct zynjacku_simple_plugin_info * plugin_info_ptr;

  list_for_each(node_ptr, &g_available_plugins)
  {
    plugin_info_ptr = list_entry(node_ptr, struct zynjacku_simple_plugin_info, siblings);
    current_uri = slv2_plugin_get_uri(plugin_info_ptr->plugin_ptr);
    if (strcmp(current_uri, uri) == 0)
    {
      return plugin_info_ptr->plugin_ptr;
    }
  }

  return NULL;
}

/* Nasty hack until we start using real plugin list */
SLV2Plugin *
zynjacku_plugin_repo_lookup_by_uri(const char * uri)
{
  SLV2Plugin * plugin_ptr;
  SLV2List plugins;

  plugins = slv2_list_new();
  slv2_list_load_all(plugins);

  plugin_ptr = slv2_list_get_plugin_by_uri(plugins, uri);
  if (plugin_ptr == NULL)
  {
    slv2_list_free(plugins);
    return NULL;
  }

  /* yup, overwrite old plugin_ptr value - we don't need it anyway after the dup */
  plugin_ptr = slv2_plugin_duplicate(plugin_ptr);

  slv2_list_free(plugins);

  return plugin_ptr;
}

void
zynjacku_dump_simple_plugins()
{
  char * name;
  struct list_head * node_ptr;
  struct zynjacku_simple_plugin_info * plugin_info_ptr;

  list_for_each(node_ptr, &g_available_plugins)
  {
    plugin_info_ptr = list_entry(node_ptr, struct zynjacku_simple_plugin_info, siblings);
    name = slv2_plugin_get_name(plugin_info_ptr->plugin_ptr);
    printf("\"%s\", %s\n", name, slv2_plugin_get_uri(plugin_info_ptr->plugin_ptr));
    free(name);
  }
}
