/* -*- Mode: C ; c-basic-offset: 2 -*- */
/*****************************************************************************
 *
 *   This file is part of zynjacku
 *
 *   Copyright (C) 2007,2008,2009 Nedko Arnaudov <nedko@arnaudov.name>
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

#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <assert.h>
#include <lv2.h>
#if HAVE_DYNPARAMS
#include <lv2dynparam/lv2dynparam.h>
#include <lv2dynparam/lv2_rtmempool.h>
#include <lv2dynparam/host.h>
#endif
#include <gtk/gtk.h>
#include <jack/jack.h>

#include "lv2-miditype.h"
#include "lv2_event.h"
#include "lv2_uri_map.h"
#include "lv2_data_access.h"
#include "lv2_string_port.h"
#include "lv2_ui.h"
#include "lv2_external_ui.h"

#include "list.h"
#include "lv2.h"
#include "gtk2gui.h"
#include "zynjacku.h"
#include "plugin.h"
#include "plugin_internal.h"

#define LOG_LEVEL LOG_LEVEL_ERROR
#include "log.h"

#define LV2_UI_URI "http://lv2plug.in/ns/extensions/ui"
#define LV2_UI_GTK_URI LV2_UI_URI "#GtkUI"

#define UI_TYPE_GTK       1
#define UI_TYPE_EXTERNAL  2

struct zynjacku_gtk2gui
{
  const LV2_Feature ** host_features;
  char * plugin_uri;
  char * bundle_path;
  unsigned int ports_count;
  struct zynjacku_port ** ports;
  struct zynjacku_plugin * plugin_ptr;
  void * context_ptr;
  const char * instance_name;
  bool resizable;
  void * dlhandle;
  const LV2UI_Descriptor * lv2ui;
  LV2UI_Handle ui_handle;
  GtkWidget * widget_ptr;
  GtkWidget * window_ptr;
  zynjacku_lv2_handle lv2plugin;
  LV2_Extension_Data_Feature data_access;
  struct lv2_external_ui_host external_ui;
  LV2_Feature gui_feature_instance_access;
  LV2_Feature gui_feature_data_access;
  LV2_Feature gui_feature_external_ui;
  unsigned int type;
  struct lv2_external_ui * external_ui_control;
};

#define ui_ptr ((struct zynjacku_gtk2gui *)controller)

void
zynjacku_plugin_ui_closed(
  LV2UI_Controller controller)
{
  zynjacku_gtk2gui_on_ui_destroyed(ui_ptr->context_ptr);
  ui_ptr->lv2ui->cleanup(ui_ptr->ui_handle);
  ui_ptr->ui_handle = NULL;
}

#undef ui_ptr

zynjacku_gtk2gui_handle
zynjacku_gtk2gui_create(
  const LV2_Feature * const * host_features,
  unsigned int host_feature_count,
  zynjacku_lv2_handle plugin_handle,
  struct zynjacku_plugin * plugin_ptr,
  void * context_ptr,
  const char * ui_type_uri,
  const char * plugin_uri,
  const char * ui_uri,
  const char * ui_binary_path,
  const char * ui_bundle_path,
  const char * plugin_instance_name,
  const struct list_head * parameter_ports_ptr)
{
  struct zynjacku_gtk2gui * ui_ptr;
  unsigned int ports_count;
  struct list_head * node_ptr;
  struct zynjacku_port * port_ptr;
  LV2UI_DescriptorFunction lookup;
  uint32_t index;
  unsigned int type;

  if (strcmp(ui_type_uri, LV2_UI_GTK_URI) == 0)
  {
    LOG_NOTICE("GtkUI for '%s'", plugin_uri);
    type = UI_TYPE_GTK;
  }
  else if (strcmp(ui_type_uri, LV2_EXTERNAL_UI_URI) == 0)
  {
    LOG_NOTICE("External UI for '%s'", plugin_uri);
    type = UI_TYPE_EXTERNAL;
  }
  else
  {
    LOG_DEBUG("Ignoring UI '%s' of plugin '%s', unknown type '%s'", ui_uri, plugin_uri, ui_type_uri);
    goto fail;
  }

  ui_ptr = malloc(sizeof(struct zynjacku_gtk2gui));
  if (ui_ptr == NULL)
  {
    LOG_ERROR("malloc() failed.");
    goto fail;
  }

  ui_ptr->type = type;

  ui_ptr->plugin_uri = strdup(plugin_uri);
  if (ui_ptr->plugin_uri == NULL)
  {
    LOG_ERROR("strdup(\"%s\") failed", plugin_uri);
    goto fail_free;
  }

  ui_ptr->plugin_ptr = plugin_ptr;
  ui_ptr->context_ptr = context_ptr;
  ui_ptr->instance_name = plugin_instance_name;
  ui_ptr->resizable = true;
  ui_ptr->lv2plugin = plugin_handle;
  ui_ptr->data_access.data_access = zynjacku_lv2_get_descriptor(plugin_handle)->extension_data;
  ui_ptr->external_ui.ui_closed = zynjacku_plugin_ui_closed;
  ui_ptr->external_ui.plugin_human_id = plugin_instance_name;

  ui_ptr->gui_feature_instance_access.URI = "http://lv2plug.in/ns/ext/instance-access";
  ui_ptr->gui_feature_instance_access.data = zynjacku_lv2_get_handle(ui_ptr->lv2plugin);
  ui_ptr->gui_feature_data_access.URI = LV2_DATA_ACCESS_URI;
  ui_ptr->gui_feature_data_access.data = &ui_ptr->data_access;
  ui_ptr->gui_feature_external_ui.URI = LV2_EXTERNAL_UI_URI;
  ui_ptr->gui_feature_external_ui.data = &ui_ptr->external_ui;

  ports_count = 0;

  list_for_each(node_ptr, parameter_ports_ptr)
  {
    port_ptr = list_entry(node_ptr, struct zynjacku_port, plugin_siblings);
    if (port_ptr->index >= ports_count)
    {
      ports_count = port_ptr->index + 1;
    }
  }

  ui_ptr->ports = malloc(ports_count * sizeof(struct zynjacku_port *));
  if (ui_ptr->ports == NULL)
  {
    LOG_ERROR("malloc() failed.");
    goto fail_free_plugin_uri;
  }

  memset(ui_ptr->ports, 0, ports_count * sizeof(struct zynjacku_port *));
  
  list_for_each(node_ptr, parameter_ports_ptr)
  {
    port_ptr = list_entry(node_ptr, struct zynjacku_port, plugin_siblings);
    ui_ptr->ports[port_ptr->index] = port_ptr;
  }

  ui_ptr->ports_count = ports_count;

  assert(host_features[host_feature_count] == NULL);
  
  ui_ptr->host_features = malloc((host_feature_count + 4) * sizeof(struct LV2_Feature *));
  if (ui_ptr->host_features == NULL)
  {
    LOG_WARNING("malloc() failed");
    goto fail_free_ports;
  }
  memcpy(ui_ptr->host_features, host_features, host_feature_count * sizeof(struct LV2_Feature *));
  ui_ptr->host_features[host_feature_count++] = &ui_ptr->gui_feature_data_access;
  ui_ptr->host_features[host_feature_count++] = &ui_ptr->gui_feature_instance_access;
  ui_ptr->host_features[host_feature_count++] = &ui_ptr->gui_feature_external_ui;
  ui_ptr->host_features[host_feature_count++] = NULL;

  ui_ptr->bundle_path = strdup(ui_bundle_path);
  if (ui_ptr->bundle_path == NULL)
  {
    LOG_ERROR("strdup(\"%s\") failed", ui_bundle_path);
    goto fail_free_features;
  }

  ui_ptr->dlhandle = dlopen(ui_binary_path, RTLD_NOW);
  if (ui_ptr->dlhandle == NULL)
  {
    LOG_ERROR("Cannot load \"%s\": %s", ui_binary_path, dlerror());
    goto fail_free_bundle_path;
  }

  lookup = (LV2UI_DescriptorFunction)dlsym(ui_ptr->dlhandle, "lv2ui_descriptor");
  if (lookup == NULL)
  {
    LOG_ERROR("Cannot find symbol lv2ui_descriptor");
    goto fail_dlclose;
  }

  index = 0;

  do
  {
    ui_ptr->lv2ui = lookup(index);
    if (ui_ptr->lv2ui == NULL)
    {
      LOG_ERROR("Did not find UI %s in %s", ui_uri, ui_binary_path);
      goto fail_dlclose;
    }

    index++;
  }
  while (strcmp(ui_ptr->lv2ui->URI, ui_uri) != 0);

  ui_ptr->ui_handle = NULL;
  ui_ptr->widget_ptr = NULL;
  ui_ptr->window_ptr = NULL;
  ui_ptr->external_ui_control = NULL;

  return ui_ptr;

fail_dlclose:
  dlclose(ui_ptr->dlhandle);

fail_free_bundle_path:
  free(ui_ptr->bundle_path);

fail_free_features:
  free(ui_ptr->host_features);

fail_free_ports:
  free(ui_ptr->ports);

fail_free_plugin_uri:
  free(ui_ptr->plugin_uri);

fail_free:
  free(ui_ptr);

fail:
  return NULL;
}

void
zynjacku_gtk2gui_port_event(
  struct zynjacku_gtk2gui * ui_ptr,
  struct zynjacku_port * port_ptr)
{
  int size;
  int format;
  const void * data;

  switch (port_ptr->type)
  {
  case PORT_TYPE_LV2_FLOAT:
    LOG_DEBUG(
      "parameter #%u with value %f and range %f - %f",
      (unsigned int)port_ptr->index,
      port_ptr->data.lv2float.value,
      port_ptr->data.lv2float.min,
      port_ptr->data.lv2float.max);

    size = sizeof(float);
    format = 0;
    data = (const void *)&port_ptr->data.lv2float.value;
    break;
  case PORT_TYPE_LV2_STRING:
    size = sizeof(LV2_String_Data);
    format = ZYNJACKU_STRING_XFER_ID;
    data = &port_ptr->data.lv2string;
    break;
  default:
    return;
  }
        
  ui_ptr->lv2ui->port_event(
    ui_ptr->ui_handle,
    port_ptr->index,
    size,
    format,
    data);
}

#define ui_ptr ((struct zynjacku_gtk2gui *)ui_handle)

static
void
zynjacku_on_gtk2gui_window_destroy_internal(
  GtkWidget * widget,
  gpointer ui_handle)
{
  LOG_DEBUG("zynjacku_on_gtk2gui_window_destroy_internal() called");
  //gtk_container_remove(GTK_CONTAINER(ui_ptr->window_ptr), ui_ptr->widget_ptr);
  zynjacku_gtk2gui_on_ui_destroyed(ui_ptr->context_ptr);
  ui_ptr->window_ptr = NULL;
  ui_ptr->lv2ui->cleanup(ui_ptr->ui_handle);
  ui_ptr->ui_handle = NULL;
}

void
zynjacku_gtk2gui_destroy(
  zynjacku_gtk2gui_handle ui_handle)
{
  LOG_DEBUG("zynjacku_on_gtk2gui_destroy() called");

  if (ui_ptr->ui_handle != NULL &&
      ui_ptr->type == UI_TYPE_EXTERNAL)
  {
    LV2_EXTERNAL_UI_HIDE(ui_ptr->external_ui_control);
  }

  dlclose(ui_ptr->dlhandle);
  free(ui_ptr->ports);
  free(ui_ptr->bundle_path);
  free(ui_ptr->plugin_uri);
  free(ui_ptr);
}

/* LV2UI_Write_Function */
void
zynjacku_gtk2gui_callback_write(
  LV2UI_Controller ui_handle,
  uint32_t port_index,
  uint32_t buffer_size,
  uint32_t format,
  const void * buffer)
{
  if (port_index >= ui_ptr->ports_count || ui_ptr->ports[port_index] == NULL)
  {
    LOG_WARNING(
      "Ignoring value change notification from UI for unknown port #%u",
      (unsigned int)port_index);
    return;
  }

  zynjacku_plugin_ui_set_port_value(ui_ptr->ports[port_index]->plugin_ptr, ui_ptr->ports[port_index], buffer, buffer_size);
  zynjacku_gtk2gui_port_event(ui_ptr, ui_ptr->ports[port_index]);
}

bool
zynjacku_gtk2gui_ui_on(
  zynjacku_gtk2gui_handle ui_handle)
{
  struct zynjacku_port * port_ptr;
  unsigned int port_index;
  LV2UI_Widget widget;

  LOG_DEBUG("zynjacku_gtk2gui_ui_on(%u) called.", index);

  if (ui_ptr->ui_handle == NULL)
  {
    LOG_DEBUG("Instantiating UI...");

    ui_ptr->ui_handle = ui_ptr->lv2ui->instantiate(
      ui_ptr->lv2ui,
      ui_ptr->plugin_uri,
      ui_ptr->bundle_path,
      zynjacku_gtk2gui_callback_write,
      ui_ptr,
      &widget,
      ui_ptr->host_features);

    LOG_DEBUG("Instantiation done.");

    if (ui_ptr->ui_handle == NULL)
    {
      LOG_ERROR("plugin custom UI instantiation failed");
      return false;
    }

    LOG_DEBUG("widget: %p", widget);

    switch (ui_ptr->type)
    {
    case UI_TYPE_GTK:
      ui_ptr->widget_ptr = widget;
      assert(GTK_IS_WIDGET(ui_ptr->widget_ptr));
      break;
    case UI_TYPE_EXTERNAL:
      ui_ptr->external_ui_control = widget;
      break;
    default:
      assert(false);
    }

    /* Set parameter values */
    if (ui_ptr->lv2ui->port_event != NULL)
    {
      for (port_index = 0 ; port_index < ui_ptr->ports_count ; port_index++)
      {
        port_ptr = ui_ptr->ports[port_index];
        if (port_ptr == NULL)     /* handle gaps */
        {
          continue;
        }

        zynjacku_gtk2gui_port_event(ui_ptr, port_ptr);
      }
    }
  }

  switch (ui_ptr->type)
  {
  case UI_TYPE_GTK:
    if (ui_ptr->window_ptr == NULL)
    {
      ui_ptr->window_ptr = gtk_window_new(GTK_WINDOW_TOPLEVEL);

      gtk_window_set_title(GTK_WINDOW(ui_ptr->window_ptr), ui_ptr->instance_name);

      gtk_window_set_role(GTK_WINDOW(ui_ptr->window_ptr), "plugin_ui");

      gtk_window_set_resizable(GTK_WINDOW(ui_ptr->window_ptr), ui_ptr->resizable);

      gtk_container_add(GTK_CONTAINER(ui_ptr->window_ptr), ui_ptr->widget_ptr);

      g_signal_connect(
        G_OBJECT(ui_ptr->window_ptr),
        "destroy",
        G_CALLBACK(zynjacku_on_gtk2gui_window_destroy_internal),
        ui_ptr);
    }

    /* Show the widgets */
    gtk_widget_show_all(ui_ptr->window_ptr);

    break;
  case UI_TYPE_EXTERNAL:
    LV2_EXTERNAL_UI_SHOW(ui_ptr->external_ui_control);
    break;
  }

  return true;
}

void
zynjacku_gtk2gui_push_measure_ports(
  zynjacku_gtk2gui_handle ui_handle,
  const struct list_head * measure_ports_ptr)
{
  struct list_head * node_ptr;
  struct zynjacku_port * port_ptr;

  if (ui_ptr->ui_handle == NULL)
  {
    return;
  }

  if (ui_ptr->type == UI_TYPE_EXTERNAL)
  {
    LV2_EXTERNAL_UI_RUN(ui_ptr->external_ui_control);
  }

  if (ui_ptr->lv2ui->port_event == NULL)
  {
    return;
  }

  list_for_each(node_ptr, measure_ports_ptr)
  {
    port_ptr = list_entry(node_ptr, struct zynjacku_port, plugin_siblings);
    zynjacku_gtk2gui_port_event(ui_ptr, port_ptr);
  }
}

void
zynjacku_gtk2gui_ui_off(
  zynjacku_gtk2gui_handle ui_handle)
{
  LOG_DEBUG("zynjacku_gtk2gui_ui_off() called.");

  if (ui_ptr->ui_handle == NULL)
  {
    return;
  }

  switch (ui_ptr->type)
  {
  case UI_TYPE_GTK:
    /* Hide the widgets */
    gtk_widget_hide_all(ui_ptr->window_ptr);
    break;
  case UI_TYPE_EXTERNAL:
    LV2_EXTERNAL_UI_HIDE(ui_ptr->external_ui_control);
    break;
  }
}
