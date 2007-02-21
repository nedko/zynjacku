/* -*- Mode: C ; c-basic-offset: 2 -*- */
/*****************************************************************************
 *
 *   This file is part of zynjacku
 *
 *   Copyright (C) 2007 Nedko Arnaudov <nedko@arnaudov.name>
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

#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <slv2/slv2.h>
#include <dlfcn.h>
#include <lv2dynparam/lv2.h>
#include <lv2dynparam/lv2dynparam.h>
#include <lv2dynparam/host.h>
#include <gtk/gtk.h>
#include <jack/jack.h>

#include "lv2-miditype.h"
#include "lv2-gtk2gui.h"

#include "list.h"
#include "gtk2gui.h"
#include "zynjacku.h"

#define LOG_LEVEL LOG_LEVEL_DEBUG
#include "log.h"

#define LV2GTK2GUI_URI "<http://ll-plugins.nongnu.org/lv2/ext/gtk2gui#gui>"
#define LV2GTK2GUI_BINARY_URI "<http://ll-plugins.nongnu.org/lv2/ext/gtk2gui#binary>"

struct zynjacku_gtk2gui_ui
{
  char * uri;
  char * quoted_uri;
  char * bundle_path;
  void * module;
  const LV2UI_Descriptor * descr_ptr;
  LV2UI_Handle ui;
  GtkWidget * widget_ptr;
  GtkWidget * window_ptr;
};

struct zynjacku_gtk2gui
{
  SLV2Plugin plugin;
  unsigned int count;
  struct zynjacku_gtk2gui_ui * ui_array;
  unsigned int ports_count;
  struct zynjacku_synth_port ** ports;
};

char *
zynjacku_rdf_uri_quote(const char * uri)
{
  size_t size_temp;
  char * quoted_uri;

  size_temp = strlen(uri);

  quoted_uri = malloc(size_temp + 2 + 1);
  if (quoted_uri == NULL)
  {
    LOG_ERROR("malloc() failed.");
    return NULL;
  }

  quoted_uri[0] = '<';
  memcpy(quoted_uri + 1, uri, size_temp);
  quoted_uri[1 + size_temp] = '>';
  quoted_uri[1 + size_temp + 1] = 0;

  return quoted_uri;
}

char *
zynjacku_get_bundle_path(
  const char * binary_path)
{
  const char * pch;
  size_t size;
  char * bundle_path;

  pch = strrchr(binary_path, '/');
  if (pch == NULL)
  {
    LOG_ERROR("Cannot extract bundle path from binary because cannot find last slash in \"%s\"", binary_path);
    return NULL;
  }

  size = (pch - binary_path) + 1;

  bundle_path = malloc(size + 1);
  if (bundle_path == NULL)
  {
    LOG_ERROR("malloc() failed.");
    return NULL;
  }

  memcpy(bundle_path, binary_path, size);
  bundle_path[size] = 0;

  return bundle_path;
}

void *
zynjacku_gtk2gui_load(
  struct zynjacku_gtk2gui_ui * ui_ptr,
  SLV2Plugin plugin)
{
  SLV2Strings slv2_strings;
  void * module;
  const char * gtk2gui_binary;

  slv2_strings = slv2_plugin_get_value_for_subject(
    plugin,
    ui_ptr->quoted_uri,
    LV2GTK2GUI_BINARY_URI);

  if (slv2_strings_size(slv2_strings) != 1)
  {
    LOG_WARNING("Ignoring custom GUI %s", ui_ptr->uri);
    module = NULL;
    goto exit;
  }

  gtk2gui_binary = slv2_strings_get_at(slv2_strings, 0);

  if (strlen(gtk2gui_binary) <= 8 || memcmp(gtk2gui_binary, "file:///", 8) != 0)
  {
    LOG_WARNING("Ignoring custom GUI %s with binary \"%s\" - don't know how to load it", ui_ptr->uri, gtk2gui_binary);
    module = NULL;
    goto exit;
  }

  gtk2gui_binary += 7;
  LOG_NOTICE("Custom GUI %s with binary \"%s\"", ui_ptr->uri, gtk2gui_binary);

  ui_ptr->bundle_path = zynjacku_get_bundle_path(gtk2gui_binary);
  if (ui_ptr->bundle_path == NULL)
  {
    LOG_WARNING("Ignoring custom GUI because cannot extract bundle path.");
    module = NULL;
    goto exit;
  }

  LOG_NOTICE("Custom GUI %s with bundle path \"%s\"", ui_ptr->uri, ui_ptr->bundle_path);

  LOG_DEBUG("Loading \"%s\"", gtk2gui_binary);
  module = dlopen(gtk2gui_binary, RTLD_LAZY);
  if (module == NULL)
  {
    LOG_WARNING("Could not load \"%s\": %s", gtk2gui_binary, dlerror());
    free(ui_ptr->bundle_path);
  }

exit:
  slv2_strings_free(slv2_strings);
  return module;
}

const LV2UI_Descriptor *
zynjacku_gtk2gui_get_descriptor(
  LV2UI_DescriptorFunction descr_func,
  const char * uri)
{
  const LV2UI_Descriptor * descr_ptr;
  uint32_t descr_index;

  descr_index = 0;

loop:
  descr_ptr = descr_func(descr_index);
  if (descr_ptr == NULL)
  {
    return NULL;
  }

  //LOG_DEBUG("Comparing \"%s\" with \"%s\"", descr_ptr->URI, uri);
  if (strcmp(descr_ptr->URI, uri) == 0)
  {
    return descr_ptr;
  }

  descr_index++;
  goto loop;
}

gboolean
zynjacku_gtk2gui_ui_init(
  struct zynjacku_gtk2gui_ui * ui_ptr,
  SLV2Plugin plugin,
  const char * uri)
{
  LV2UI_DescriptorFunction descr_func;

  ui_ptr->uri = strdup(uri);
  if (ui_ptr->uri == NULL)
  {
    goto fail;
  }

  ui_ptr->quoted_uri = zynjacku_rdf_uri_quote(uri);
  if (ui_ptr->quoted_uri == NULL)
  {
    goto fail_free_uri;
  }

  ui_ptr->module = zynjacku_gtk2gui_load(ui_ptr, plugin);

  descr_func = (LV2UI_DescriptorFunction)dlsym(ui_ptr->module, "lv2ui_descriptor");
  if (!descr_func)
  {
    LOG_WARNING("Could not find symbol lv2ui_descriptor");
    goto fail_dlclose;
  }

  ui_ptr->descr_ptr = zynjacku_gtk2gui_get_descriptor(descr_func, ui_ptr->uri);
  if (ui_ptr->descr_ptr == NULL)
  {
    LOG_WARNING("LV2 gtk2gui descriptor not found.");
    goto fail_dlclose;
  }

  ui_ptr->ui = NULL;
  ui_ptr->widget_ptr = NULL;
  ui_ptr->window_ptr = NULL;

  LOG_DEBUG("LV2 gtk2gui descriptor found.");

  return TRUE;

fail_dlclose:
  dlclose(ui_ptr->module);

fail_free_uri:
  free(ui_ptr->uri);

fail:
  return FALSE;
}

void
zynjacku_gtk2gui_ui_uninit(
  struct zynjacku_gtk2gui_ui * ui_ptr)
{
  dlclose(ui_ptr->module);
  free(ui_ptr->bundle_path);
  free(ui_ptr->quoted_uri);
  free(ui_ptr->uri);
}

zynjacku_gtk2gui_handle
zynjacku_gtk2gui_init(
  SLV2Plugin plugin,
  const struct list_head * parameter_ports_ptr)
{
  SLV2Strings uris;
  unsigned int index;
  unsigned int count;
  const char * uri;
  struct zynjacku_gtk2gui * gtk2gui_ptr;
  unsigned int ports_count;
  struct list_head * node_ptr;
  struct zynjacku_synth_port * port_ptr;

  uris = slv2_plugin_get_value(plugin, LV2GTK2GUI_URI);

  count = slv2_strings_size(uris);

  if (count == 0)
  {
    LOG_DEBUG("No gtk2gui custom GUIs available for %s", slv2_plugin_get_uri(plugin));
    goto fail;
  }

  gtk2gui_ptr = malloc(sizeof(struct zynjacku_gtk2gui));
  if (gtk2gui_ptr == NULL)
  {
    LOG_ERROR("malloc() failed.");
    goto fail;
  }

  gtk2gui_ptr->ui_array = malloc(count * sizeof(struct zynjacku_gtk2gui_ui));
  if (gtk2gui_ptr->ui_array == NULL)
  {
    LOG_ERROR("malloc() failed.");
    goto fail_free;
  }

  LOG_NOTICE("Plugin has %u custom GUI(s)", count);
  for (index = 0 ; index < count ; index++)
  {
    uri = slv2_strings_get_at(uris, index);

    LOG_DEBUG("%s", uri);

    if (!zynjacku_gtk2gui_ui_init(gtk2gui_ptr->ui_array + index, plugin, uri))
    {
      goto fail_free_array;
    }
  }

  slv2_strings_free(uris);

  gtk2gui_ptr->count = count;
  gtk2gui_ptr->plugin = slv2_plugin_duplicate(plugin);

  ports_count = 0;

  list_for_each(node_ptr, parameter_ports_ptr)
  {
    port_ptr = list_entry(node_ptr, struct zynjacku_synth_port, plugin_siblings);
    if (port_ptr->index >= ports_count)
    {
      ports_count = port_ptr->index + 1;
    }
  }

  gtk2gui_ptr->ports = malloc(ports_count * sizeof(struct zynjacku_synth_port *));
  if (gtk2gui_ptr->ports == NULL)
  {
    LOG_ERROR("malloc() failed.");
    goto fail_free_array;
  }

  memset(gtk2gui_ptr->ports, 0, ports_count * sizeof(struct zynjacku_synth_port *));

  list_for_each(node_ptr, parameter_ports_ptr)
  {
    port_ptr = list_entry(node_ptr, struct zynjacku_synth_port, plugin_siblings);
    gtk2gui_ptr->ports[port_ptr->index] = port_ptr;
  }

  gtk2gui_ptr->ports_count = ports_count;

  return (zynjacku_gtk2gui_handle)gtk2gui_ptr;

fail_free_array:
  while (index > 0)
  {
    index--;
    zynjacku_gtk2gui_ui_uninit(gtk2gui_ptr->ui_array + index);
  }

  free(gtk2gui_ptr->ui_array);

fail_free:
  free(gtk2gui_ptr);

fail:
  slv2_strings_free(uris);
  return ZYNJACKU_GTK2GUI_HANDLE_INVALID_VALUE;
}

#define gtk2gui_ptr ((struct zynjacku_gtk2gui *)gtk2gui_handle)

void
zynjacku_gtk2gui_uninit(
  zynjacku_gtk2gui_handle gtk2gui_handle)
{
  unsigned int index;

  free(gtk2gui_ptr->ports);

  slv2_plugin_free(gtk2gui_ptr->plugin);

  for (index = 0 ; index < gtk2gui_ptr->count ; index++)
  {
    free(gtk2gui_ptr->ui_array[index].quoted_uri);
    free(gtk2gui_ptr->ui_array[index].uri);
  }

  free(gtk2gui_ptr->ui_array);
  free(gtk2gui_ptr);
}

unsigned int
zynjacku_gtk2gui_get_count(
  zynjacku_gtk2gui_handle gtk2gui_handle)
{
  return gtk2gui_ptr->count;
}

const char *
zynjacku_gtk2gui_get_name(
  zynjacku_gtk2gui_handle gtk2gui_handle,
  unsigned int index)
{
  return "Custom GUI";
}

void
zynjacku_gtk2gui_control(
  LV2UI_Controller gtk2gui_handle,
  uint32_t port,
  float value)
{
  LOG_DEBUG("setting port %u to %f", (unsigned int)port, value);

  if (port >= gtk2gui_ptr->ports_count || gtk2gui_ptr->ports[port] == NULL)
  {
    LOG_WARNING(
      "Ignoring value change notification from UI for unknown port #%u",
      (unsigned int)port);
    return;
  }

  gtk2gui_ptr->ports[port]->data.parameter.value = value;
}

void
zynjacku_gtk2gui_ui_on(
  zynjacku_gtk2gui_handle gtk2gui_handle,
  unsigned int index)
{
  LV2_Host_Feature * features[1];
  struct zynjacku_synth_port * port_ptr;
  unsigned int port_index;

  LOG_DEBUG("zynjacku_gtk2gui_ui_on() called.");

  if (gtk2gui_ptr->ui_array[index].ui != NULL)
  {
    return;
  }

  features[0] = NULL;

  gtk2gui_ptr->ui_array[index].ui = gtk2gui_ptr->ui_array[index].descr_ptr->instantiate(
    gtk2gui_ptr->ui_array[index].descr_ptr,
    gtk2gui_ptr->ui_array[index].uri,
    gtk2gui_ptr->ui_array[index].bundle_path,
    zynjacku_gtk2gui_control, 
    gtk2gui_ptr,
    &gtk2gui_ptr->ui_array[index].widget_ptr, 
    (const LV2_Host_Feature **)features);

  LOG_DEBUG("ui: %p", gtk2gui_ptr->ui_array[index].ui);
  LOG_DEBUG("widget: %p", gtk2gui_ptr->ui_array[index].widget_ptr);

  gtk2gui_ptr->ui_array[index].window_ptr = gtk_window_new(GTK_WINDOW_TOPLEVEL);

  gtk_container_add(GTK_CONTAINER(gtk2gui_ptr->ui_array[index].window_ptr), gtk2gui_ptr->ui_array[index].widget_ptr);

  /* Show the widgets */
  gtk_widget_show_all(gtk2gui_ptr->ui_array[index].window_ptr);

  /* Set parameter values */
  if (gtk2gui_ptr->ui_array[index].descr_ptr->set_control != NULL)
  {
    for (port_index = 0 ; port_index < gtk2gui_ptr->ports_count ; port_index++)
    {
      port_ptr = gtk2gui_ptr->ports[port_index];

      if (port_ptr == NULL)     /* handle gaps */
      {
        continue;
      }

      LOG_DEBUG(
        "parameter #%u with value %f and range %f - %f",
        (unsigned int)port_ptr->index,
        port_ptr->data.parameter.value,
        port_ptr->data.parameter.min,
        port_ptr->data.parameter.max);

      gtk2gui_ptr->ui_array[index].descr_ptr->set_control(
        gtk2gui_ptr->ui_array[index].ui,
        port_ptr->index,
        port_ptr->data.parameter.value);
    }
  }
}

void
zynjacku_gtk2gui_ui_off(
  zynjacku_gtk2gui_handle gtk2gui_handle,
  unsigned int index)
{
  LOG_DEBUG("zynjacku_gtk2gui_ui_off() called.");
}
