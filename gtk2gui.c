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
#include <lv2dynparam/lv2_rtmempool.h>
#include <lv2dynparam/host.h>
#include <gtk/gtk.h>
#include <jack/jack.h>

#include "lv2-miditype.h"

#include "list.h"
#include "gtk2gui.h"
#include "zynjacku.h"

#define LOG_LEVEL LOG_LEVEL_ERROR
#include "log.h"

#define LV2GTK2GUI_URI "http://ll-plugins.nongnu.org/lv2/ext/gui/dev/1#GtkGUI"
//#define LV2GTK2GUI_BINARY_URI "http://ll-plugins.nongnu.org/lv2/ext/gtk2gui#binary"
//#define LV2GTK2GUI_OPTIONAL_FEATURE_URI "http://ll-plugins.nongnu.org/lv2/ext/gtk2gui#optionalFeature"
//#define LV2GTK2GUI_REQUIRED_FEATURE_URI "http://ll-plugins.nongnu.org/lv2/ext/gtk2gui#requiredFeature"
//#define LV2GTK2GUI_NOUSERRESIZE_URI "http://ll-plugins.nongnu.org/lv2/ext/gtk2gui#noUserResize"
//#define LV2GTK2GUI_FIXEDSIZE_URI "http://ll-plugins.nongnu.org/lv2/ext/gtk2gui#fixedSize"

struct zynjacku_gtk2gui;

struct zynjacku_gtk2gui_ui
{
  SLV2UI factory;
	SLV2UIInstance instance;
  LV2UI_Handle instance_handle;
  struct zynjacku_gtk2gui * gtk2gui_ptr;
  const LV2UI_Descriptor * descr_ptr;
  GtkWidget * widget_ptr;
  GtkWidget * window_ptr;
  bool resizable;
};

struct zynjacku_gtk2gui
{
  SLV2Plugin plugin;
  unsigned int count;
  SLV2UIs uis;
  struct zynjacku_gtk2gui_ui * ui_array;
  unsigned int ports_count;
  struct zynjacku_synth_port ** ports;
  void * context_ptr;
  const char * synth_id;
};

/*
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

bool
zynjacku_gtk2gui_process_feature(
  struct zynjacku_gtk2gui_ui * ui_ptr,
  const char * feature)
{
  if (strcmp(feature, LV2GTK2GUI_FIXEDSIZE_URI) == 0 ||
      strcmp(feature, LV2GTK2GUI_NOUSERRESIZE_URI) == 0)
  {
    ui_ptr->resizable = false;
    return true;
  }

  LOG_DEBUG("Unknown feature %s", feature);
  return false;
}

void *
zynjacku_gtk2gui_load(
  struct zynjacku_gtk2gui_ui * ui_ptr,
  SLV2Plugin plugin)
{
  SLV2Values values;
  SLV2Value value;
  void * module;
  const char * gtk2gui_binary;
  unsigned int index;
  unsigned int count;
  const char * uri;

  values = slv2_plugin_get_value_for_subject(
    plugin,
    ui_ptr->uri,
    SLV2_URI,
    LV2GTK2GUI_OPTIONAL_FEATURE_URI);

  count = slv2_values_size(values);
  for (index = 0 ; index < count ; index++)
  {
    value = slv2_values_get_at(values, index);

    if (!slv2_value_is_uri(value))
    {
      LOG_WARNING("Ignoring optional feature that is not URI");
      continue;
    }

    uri = slv2_value_as_uri(value);

    if (!zynjacku_gtk2gui_process_feature(ui_ptr, uri))
    {
      LOG_WARNING("Ignoring unknown optional feature %s", uri);
    }
  }

  slv2_values_free(values);

  values = slv2_plugin_get_value_for_subject(
    plugin,
    ui_ptr->uri,
    SLV2_URI,
    LV2GTK2GUI_REQUIRED_FEATURE_URI);

  count = slv2_values_size(values);
  for (index = 0 ; index < count ; index++)
  {
    value = slv2_values_get_at(values, index);

    if (!slv2_value_is_uri(value))
    {
      LOG_WARNING("Ignoring custom GUI because of required feature that is not URI");
      module = NULL;
      goto exit;
    }

    uri = slv2_value_as_uri(value);

    if (!zynjacku_gtk2gui_process_feature(ui_ptr, uri))
    {
      LOG_WARNING("Ignoring custom GUI because of unknown required feature %s", uri);
      module = NULL;
      goto exit;
    }
  }

  slv2_values_free(values);

  values = slv2_plugin_get_value_for_subject(
    plugin,
    ui_ptr->uri,
    SLV2_URI,
    LV2GTK2GUI_BINARY_URI);

  if (slv2_values_size(values) != 1)
  {
    LOG_WARNING("Ignoring custom GUI %s", ui_ptr->uri);
    module = NULL;
    goto exit;
  }

  value = slv2_values_get_at(values, 0);

  if (!slv2_value_is_uri(value))
  {
    LOG_WARNING("Ignoring custom GUI %s with binary specifier is not URI");
    module = NULL;
    goto exit;
  }

  gtk2gui_binary = slv2_value_as_uri(value);

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
  slv2_values_free(values);
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

bool
zynjacku_gtk2gui_ui_init(
  struct zynjacku_gtk2gui_ui * ui_ptr,
  SLV2Plugin plugin,
  SLV2Value uri)
{
  LV2UI_DescriptorFunction descr_func;
  const char * uri_string;

  ui_ptr->resizable = true;

  ui_ptr->uri = uri;

  ui_ptr->module = zynjacku_gtk2gui_load(ui_ptr, plugin);

  descr_func = (LV2UI_DescriptorFunction)dlsym(ui_ptr->module, "lv2ui_descriptor");
  if (!descr_func)
  {
    LOG_WARNING("Could not find symbol lv2ui_descriptor");
    goto fail_dlclose;
  }

  uri_string = slv2_value_as_uri(ui_ptr->uri);

  LOG_DEBUG("%s", uri_string);

  ui_ptr->descr_ptr = zynjacku_gtk2gui_get_descriptor(descr_func, uri_string);
  if (ui_ptr->descr_ptr == NULL)
  {
    LOG_WARNING("LV2 gtk2gui descriptor not found.");
    goto fail_dlclose;
  }

  ui_ptr->ui = NULL;
  ui_ptr->widget_ptr = NULL;
  ui_ptr->window_ptr = NULL;

  LOG_DEBUG("LV2 gtk2gui descriptor found.");

  return true;

fail_dlclose:
  dlclose(ui_ptr->module);

  return false;
}
*/

zynjacku_gtk2gui_handle
zynjacku_gtk2gui_init(
  void * context_ptr,
  SLV2Plugin plugin,
  const char * synth_id,
  const struct list_head * parameter_ports_ptr)
{
  unsigned int index;
  unsigned int count;
  struct zynjacku_gtk2gui * gtk2gui_ptr;
  unsigned int ports_count;
  struct list_head * node_ptr;
  struct zynjacku_synth_port * port_ptr;
	SLV2UI ui;

  gtk2gui_ptr = malloc(sizeof(struct zynjacku_gtk2gui));
  if (gtk2gui_ptr == NULL)
  {
    LOG_ERROR("malloc() failed.");
    goto fail;
  }

  gtk2gui_ptr->uis = slv2_plugin_get_uis(plugin);

  count = slv2_values_size(gtk2gui_ptr->uis);

  if (count == 0)
  {
    LOG_DEBUG("No gtk2gui custom GUIs available for %s", slv2_plugin_get_uri(plugin));
    goto fail_free_uis;
  }

  gtk2gui_ptr->ui_array = malloc(count * sizeof(struct zynjacku_gtk2gui_ui));
  if (gtk2gui_ptr->ui_array == NULL)
  {
    LOG_ERROR("malloc() failed.");
    goto fail_free_uis;
  }

  LOG_NOTICE("Plugin has %u custom GUI(s)", count);

  gtk2gui_ptr->count = 0;

  for (index = 0 ; index < count; index++)
  {
    ui = slv2_uis_get_at(gtk2gui_ptr->uis, index);

    if (!slv2_ui_is_type(ui, LV2GTK2GUI_URI))
    {
      continue;
    }

    gtk2gui_ptr->ui_array[gtk2gui_ptr->count].factory = ui;
    gtk2gui_ptr->ui_array[gtk2gui_ptr->count].instance = NULL;
    gtk2gui_ptr->ui_array[gtk2gui_ptr->count].resizable = true;
    gtk2gui_ptr->ui_array[gtk2gui_ptr->count].gtk2gui_ptr = gtk2gui_ptr;
    gtk2gui_ptr->ui_array[gtk2gui_ptr->count].window_ptr = NULL;

    gtk2gui_ptr->count++;
  }

  gtk2gui_ptr->plugin = plugin;

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
  gtk2gui_ptr->context_ptr = context_ptr;
  gtk2gui_ptr->synth_id = synth_id;

  return (zynjacku_gtk2gui_handle)gtk2gui_ptr;

fail_free_array:
  free(gtk2gui_ptr->ui_array);

fail_free_uis:
  slv2_uis_free(gtk2gui_ptr->uis);

//fail_free:
  free(gtk2gui_ptr);

fail:
  return ZYNJACKU_GTK2GUI_HANDLE_INVALID_VALUE;
}

#define ui_ptr ((struct zynjacku_gtk2gui_ui *)data_ptr)

static void
zynjacku_on_gtk2gui_window_destroy_internal(
  GtkWidget * widget,
  gpointer data_ptr)
{
  gtk_container_remove(GTK_CONTAINER(ui_ptr->window_ptr), ui_ptr->widget_ptr);
  zynjacku_gtk2gui_on_ui_destroyed(ui_ptr->gtk2gui_ptr->context_ptr);
  ui_ptr->window_ptr = NULL;
}

#undef ui_ptr

#define gtk2gui_ptr ((struct zynjacku_gtk2gui *)gtk2gui_handle)

void
zynjacku_gtk2gui_uninit(
  zynjacku_gtk2gui_handle gtk2gui_handle)
{
  unsigned int index;

  free(gtk2gui_ptr->ports);

  for (index = 0 ; index < gtk2gui_ptr->count ; index++)
  {
    if (gtk2gui_ptr->ui_array[index].instance != NULL)
    {
      slv2_ui_instance_free(gtk2gui_ptr->ui_array[index].instance);
    }
  }

  free(gtk2gui_ptr->ui_array);
  slv2_uis_free(gtk2gui_ptr->uis);
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

/* LV2UI_Write_Function */
void
zynjacku_gtk2gui_callback_write(
  LV2UI_Controller gtk2gui_handle,
  uint32_t port_index,
  uint32_t buffer_size,
  const void * buffer)
{
  if (port_index >= gtk2gui_ptr->ports_count || gtk2gui_ptr->ports[port_index] == NULL)
  {
    LOG_WARNING(
      "Ignoring value change notification from UI for unknown port #%u",
      (unsigned int)port_index);
    return;
  }

  /* se support only lv2:ControlPort ATM */
  assert(buffer_size == sizeof(float));

  LOG_DEBUG("setting port %u to %f", (unsigned int)port_index, *(float *)buffer);

  gtk2gui_ptr->ports[port_index]->data.parameter.value = *(float *)buffer;
}

/* LV2UI_Command_Function */
void
zynjacku_gtk2gui_callback_command(
  LV2UI_Controller gtk2gui_handle,
  uint32_t argc,
  const char* const* argv)
{
}

/* LV2UI_Program_Change_Function */
void
zynjacku_gtk2gui_callback_program_change(
  LV2UI_Controller controller,
  unsigned char program)
{
}

/* LV2UI_Program_Save_Function */
void
zynjacku_gtk2gui_callback_program_save(
  LV2UI_Controller controller,
  unsigned char program,
  const char *name)
{
}

void
zynjacku_gtk2gui_ui_on(
  zynjacku_gtk2gui_handle gtk2gui_handle,
  unsigned int index)
{
  LV2_Feature * features[1];
  struct zynjacku_synth_port * port_ptr;
  unsigned int port_index;

  LOG_DEBUG("zynjacku_gtk2gui_ui_on() called.");

  assert(index < gtk2gui_ptr->count);

  if (gtk2gui_ptr->ui_array[index].instance == NULL)
  {
    LOG_DEBUG("Instantiating UI...");

    features[0] = NULL;

    gtk2gui_ptr->ui_array[index].instance = slv2_ui_instantiate(
      gtk2gui_ptr->plugin,
      gtk2gui_ptr->ui_array[index].factory,
      zynjacku_gtk2gui_callback_write,
      zynjacku_gtk2gui_callback_command,
      zynjacku_gtk2gui_callback_program_change,
      zynjacku_gtk2gui_callback_program_save,
      gtk2gui_ptr,
      (const LV2_Feature * const *)features);

    LOG_DEBUG("Instantiation done.");

    gtk2gui_ptr->ui_array[index].widget_ptr = slv2_ui_instance_get_widget(gtk2gui_ptr->ui_array[index].instance);
    gtk2gui_ptr->ui_array[index].descr_ptr = slv2_ui_instance_get_descriptor(gtk2gui_ptr->ui_array[index].instance);
    gtk2gui_ptr->ui_array[index].instance_handle = slv2_ui_instance_get_handle(gtk2gui_ptr->ui_array[index].instance);

    LOG_DEBUG("instance: %p", gtk2gui_ptr->ui_array[index].instance);
    LOG_DEBUG("widget: %p", gtk2gui_ptr->ui_array[index].widget_ptr);
    assert(GTK_IS_WIDGET(gtk2gui_ptr->ui_array[index].widget_ptr));

    /* Set parameter values */
    if (gtk2gui_ptr->ui_array[index].descr_ptr->port_event != NULL)
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

        gtk2gui_ptr->ui_array[index].descr_ptr->port_event(
          gtk2gui_ptr->ui_array[index].instance_handle,
          port_ptr->index,
          sizeof(float),
          (const void *)&port_ptr->data.parameter.value);
      }
    }
  }

  if (gtk2gui_ptr->ui_array[index].window_ptr == NULL)
  {
    gtk2gui_ptr->ui_array[index].window_ptr = gtk_window_new(GTK_WINDOW_TOPLEVEL);

    gtk_window_set_title(GTK_WINDOW(gtk2gui_ptr->ui_array[index].window_ptr), gtk2gui_ptr->synth_id);

    gtk_window_set_resizable(GTK_WINDOW(gtk2gui_ptr->ui_array[index].window_ptr), gtk2gui_ptr->ui_array[index].resizable);

    gtk_container_add(GTK_CONTAINER(gtk2gui_ptr->ui_array[index].window_ptr), gtk2gui_ptr->ui_array[index].widget_ptr);

    g_signal_connect(
      G_OBJECT(gtk2gui_ptr->ui_array[index].window_ptr),
      "destroy",
      G_CALLBACK(zynjacku_on_gtk2gui_window_destroy_internal),
      gtk2gui_ptr->ui_array + index);
  }

  /* Show the widgets */
  gtk_widget_show_all(gtk2gui_ptr->ui_array[index].window_ptr);
}

void
zynjacku_gtk2gui_ui_off(
  zynjacku_gtk2gui_handle gtk2gui_handle,
  unsigned int index)
{
  LOG_DEBUG("zynjacku_gtk2gui_ui_off() called.");

  if (gtk2gui_ptr->ui_array[index].instance == NULL)
  {
    return;
  }

  /* Show the widgets */
  gtk_widget_hide_all(gtk2gui_ptr->ui_array[index].window_ptr);
}
