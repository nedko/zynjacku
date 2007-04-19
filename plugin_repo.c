/* -*- Mode: C ; c-basic-offset: 2 -*- */
/*****************************************************************************
 *
 *   This file is part of zynjacku
 *
 *   Copyright (C) 2006,2007 Nedko Arnaudov <nedko@arnaudov.name>
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
//#define LOG_LEVEL LOG_LEVEL_DEBUG
#include "log.h"

#define LV2_RDF_LICENSE_URI "<http://usefulinc.com/ns/doap#license>"

#define ZYNJACKU_PLUGIN_REPO_SIGNAL_TICK    0 /* plugin iterated */
#define ZYNJACKU_PLUGIN_REPO_SIGNAL_TACK    1 /* "good" plugin found */
#define ZYNJACKU_SYNTH_SIGNALS_COUNT        2

static guint g_zynjacku_plugin_repo_signals[ZYNJACKU_SYNTH_SIGNALS_COUNT];

struct zynjacku_simple_plugin_info
{
  struct list_head siblings;
  SLV2Plugin plugin;
  char * name;
  char * license;
};

struct zynjacku_plugin_repo
{
  gboolean dispose_has_run;

  gboolean scanned;
  struct list_head available_plugins; /* "struct zynjacku_simple_plugin_info"s linked by siblings */
  SLV2Model slv2_model;
  SLV2Plugins slv2_plugins;
};

#define ZYNJACKU_PLUGIN_REPO_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), ZYNJACKU_PLUGIN_REPO_TYPE, struct zynjacku_plugin_repo))

static ZynjackuPluginRepo * g_the_repo;

char *
zynjacku_rdf_uri_quote(const char * uri);

void
zynjacku_plugin_repo_clear(
  struct zynjacku_plugin_repo * repo_ptr)
{
  struct list_head * node_ptr;
  struct zynjacku_simple_plugin_info * plugin_info_ptr;

  while(!list_empty(&repo_ptr->available_plugins))
  {
    node_ptr = repo_ptr->available_plugins.next;

    list_del(node_ptr);

    plugin_info_ptr = list_entry(node_ptr, struct zynjacku_simple_plugin_info, siblings);

    //LOG_DEBUG("Removing %s", plugin_info_ptr->name);
    slv2_plugin_free(plugin_info_ptr->plugin);
    free(plugin_info_ptr->license);
    free(plugin_info_ptr->name);
    free(plugin_info_ptr);
  }

  if (repo_ptr->scanned)
  {
    slv2_plugins_free(repo_ptr->slv2_plugins);
    slv2_model_free(repo_ptr->slv2_model);
    repo_ptr->scanned = FALSE;
  }
}

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
  zynjacku_plugin_repo_clear(plugin_repo_ptr);

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

  g_zynjacku_plugin_repo_signals[ZYNJACKU_PLUGIN_REPO_SIGNAL_TICK] =
    g_signal_new(
      "tick",                   /* signal_name */
      ZYNJACKU_PLUGIN_REPO_TYPE, /* itype */
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

  g_zynjacku_plugin_repo_signals[ZYNJACKU_PLUGIN_REPO_SIGNAL_TACK] =
    g_signal_new(
      "tack",                   /* signal_name */
      ZYNJACKU_PLUGIN_REPO_TYPE, /* itype */
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

static void
zynjacku_plugin_repo_init(
  GTypeInstance * instance,
  gpointer g_class)
{
  struct zynjacku_plugin_repo * plugin_repo_ptr;

  LOG_DEBUG("zynjacku_plugin_repo_init() called.");

  plugin_repo_ptr = ZYNJACKU_PLUGIN_REPO_GET_PRIVATE(instance);

  plugin_repo_ptr->dispose_has_run = FALSE;
  INIT_LIST_HEAD(&plugin_repo_ptr->available_plugins);
  plugin_repo_ptr->scanned = FALSE;
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

/* check whether plugin is "simple synth" */
gboolean
zynjacku_plugin_repo_check_plugin(
  SLV2Plugin plugin)
{
  gboolean ret;
  uint32_t audio_out_ports_count;
  uint32_t midi_in_ports_count;
  SLV2PortClass class;
  char * name;
  uint32_t ports_count;
  uint32_t port_index;

  ret = FALSE;

  name = slv2_plugin_get_name(plugin);

  ports_count = slv2_plugin_get_num_ports(plugin);
  audio_out_ports_count = 0;
  midi_in_ports_count = 0;

  for (port_index = 0 ; port_index < ports_count ; port_index++)
  {
    class = slv2_port_get_class(plugin, slv2_plugin_get_port_by_index(plugin, port_index));

    if (class == SLV2_CONTROL_INPUT)
    {
    }
    else if (class == SLV2_AUDIO_OUTPUT)
    {
      if (audio_out_ports_count == 2)
      {
        LOG_DEBUG("Skipping \"%s\" %s, plugin with control output port", name, slv2_plugin_get_uri(plugin));
        goto free;
      }

      audio_out_ports_count++;
    }
    else if (class == SLV2_AUDIO_INPUT)
    {
      LOG_DEBUG("Skipping \"%s\" %s, plugin with audio input port", name, slv2_plugin_get_uri(plugin));
      goto free;
    }
    else if (class == SLV2_MIDI_INPUT)
    {
      if (midi_in_ports_count == 1)
      {
        LOG_DEBUG("Skipping \"%s\" %s, plugin with more than one MIDI input port", name, slv2_plugin_get_uri(plugin));
        goto free;
      }

      midi_in_ports_count++;
    }
    else if (class == SLV2_MIDI_OUTPUT)
    {
      LOG_DEBUG("Skipping \"%s\" %s, plugin with MIDI output port", name, slv2_plugin_get_uri(plugin));
      goto free;
    }
    else if (class == SLV2_CONTROL_OUTPUT)
    {
      LOG_DEBUG("Skipping \"%s\" %s, plugin with control output port", name, slv2_plugin_get_uri(plugin));
      goto free;
    }
    else
    {
      LOG_DEBUG("Skipping \"%s\" %s, plugin with port of unknown class", name, slv2_plugin_get_uri(plugin));
      goto free;
    }
  }

  if (audio_out_ports_count == 0)
  {
    LOG_DEBUG("Skipping \"%s\" %s, plugin without audio output ports", name, slv2_plugin_get_uri(plugin));
    goto free;
  }

  LOG_DEBUG("Found \"%s\" %s", name, slv2_plugin_get_uri(plugin));

  ret = TRUE;

free:
  free(name);

  return ret;
}

char *
zynjacku_plugin_repo_get_plugin_license(
  SLV2Plugin plugin)
{
  SLV2Strings slv2_strings;
  char * quoted_uri;
  char * license;

  quoted_uri = zynjacku_rdf_uri_quote(slv2_plugin_get_uri(plugin));

  slv2_strings = slv2_plugin_get_value_for_subject(
    plugin,
    quoted_uri,
    LV2_RDF_LICENSE_URI);

  if (slv2_strings_size(slv2_strings) == 0)
  {
    return strdup("none");      /* slv2 acutallu should reject those early */
  }

  license = strdup(slv2_strings_get_at(slv2_strings, 0));

  slv2_strings_free(slv2_strings);
  free(quoted_uri);

  return license;
}

void
zynjacku_plugin_repo_iterate(
  ZynjackuPluginRepo * repo_obj_ptr,
  gboolean force_scan)
{
  struct zynjacku_plugin_repo * plugin_repo_ptr;
  unsigned int plugins_count;
  unsigned int index;
  SLV2Plugin plugin;
  struct zynjacku_simple_plugin_info * plugin_info_ptr;
  float progress;
  float progress_step;
  struct list_head * node_ptr;

  LOG_DEBUG("zynjacku_plugin_repo_iterate() called.");

  plugin_repo_ptr = ZYNJACKU_PLUGIN_REPO_GET_PRIVATE(repo_obj_ptr);

  /* disable force scan because if we free model/plugins using non-duplicated plugin will crash */
  force_scan = FALSE;
  if (force_scan || !plugin_repo_ptr->scanned)
  {
    LOG_DEBUG("Scanning plugins...");
    /* disable force scan because if we free model/plugins using non-duplicated plugin will crash */
    //zynjacku_plugin_repo_clear(plugin_repo_ptr);

    plugin_repo_ptr->slv2_model = slv2_model_new();
    slv2_model_load_all(plugin_repo_ptr->slv2_model);
    plugin_repo_ptr->slv2_plugins = slv2_model_get_all_plugins(plugin_repo_ptr->slv2_model);

    plugins_count = slv2_plugins_size(plugin_repo_ptr->slv2_plugins);
    progress_step = 1.0 / plugins_count;
    progress = 0;

    LOG_DEBUG("%u plugins.", plugins_count);

    for (index = 0 ; index < plugins_count; index++)
    {
      plugin = slv2_plugins_get_at(plugin_repo_ptr->slv2_plugins, index);

      g_signal_emit(
        repo_obj_ptr,
        g_zynjacku_plugin_repo_signals[ZYNJACKU_PLUGIN_REPO_SIGNAL_TICK],
        0,
        progress,
        slv2_plugin_get_uri(plugin));

      if (zynjacku_plugin_repo_check_plugin(plugin))
      {
        plugin_info_ptr = malloc(sizeof(struct zynjacku_simple_plugin_info));
        plugin_info_ptr->plugin = plugin;

        list_add_tail(&plugin_info_ptr->siblings, &plugin_repo_ptr->available_plugins);

        plugin_info_ptr->name = slv2_plugin_get_name(plugin);
        plugin_info_ptr->license = zynjacku_plugin_repo_get_plugin_license(plugin);

        LOG_DEBUG("tack emit");
        g_signal_emit(
          repo_obj_ptr,
          g_zynjacku_plugin_repo_signals[ZYNJACKU_PLUGIN_REPO_SIGNAL_TACK],
          0,
          plugin_info_ptr->name,
          slv2_plugin_get_uri(plugin),
          plugin_info_ptr->license);
      }

      progress += progress_step;
    }

    g_signal_emit(
      repo_obj_ptr,
      g_zynjacku_plugin_repo_signals[ZYNJACKU_PLUGIN_REPO_SIGNAL_TICK],
      0,
      1.0,
      "");

    plugin_repo_ptr->scanned = TRUE;
  }
  else
  {
    LOG_DEBUG("Iterate existing plugins!");
    list_for_each(node_ptr, &plugin_repo_ptr->available_plugins)
    {
      plugin_info_ptr = list_entry(node_ptr, struct zynjacku_simple_plugin_info, siblings);
      g_signal_emit(
        repo_obj_ptr,
        g_zynjacku_plugin_repo_signals[ZYNJACKU_PLUGIN_REPO_SIGNAL_TACK],
        0,
        plugin_info_ptr->name,
        slv2_plugin_get_uri(plugin_info_ptr->plugin),
        plugin_info_ptr->license);
    }
  }
}

SLV2Plugin
zynjacku_plugin_repo_lookup_by_uri(const char * uri)
{
  struct list_head * node_ptr;
  const char * current_uri;
  struct zynjacku_simple_plugin_info * plugin_info_ptr;
  struct zynjacku_plugin_repo * plugin_repo_ptr;

  zynjacku_plugin_repo_iterate(zynjacku_plugin_repo_get(), FALSE);

  plugin_repo_ptr = ZYNJACKU_PLUGIN_REPO_GET_PRIVATE(zynjacku_plugin_repo_get());

  list_for_each(node_ptr, &plugin_repo_ptr->available_plugins)
  {
    plugin_info_ptr = list_entry(node_ptr, struct zynjacku_simple_plugin_info, siblings);
    current_uri = slv2_plugin_get_uri(plugin_info_ptr->plugin);
    if (strcmp(current_uri, uri) == 0)
    {
      return plugin_info_ptr->plugin;
    }
  }

  return NULL;
}

ZynjackuPluginRepo *
zynjacku_plugin_repo_get()
{
  if (g_the_repo == NULL)
  {
    g_the_repo = g_object_new(ZYNJACKU_PLUGIN_REPO_TYPE, NULL);
  }

  return g_the_repo;
}
