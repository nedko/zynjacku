/* -*- Mode: C ; c-basic-offset: 2 -*- */
/*****************************************************************************
 *
 *   This file is part of zynjacku
 *
 *   Copyright (C) 2007,2008 Nedko Arnaudov <nedko@arnaudov.name>
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
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <assert.h>
#include <lv2.h>
#include <lv2dynparam/lv2dynparam.h>
#include <lv2dynparam/lv2_rtmempool.h>
#include <lv2dynparam/host.h>
#include <slv2/lv2_ui.h>
#include <gtk/gtk.h>
#include <jack/jack.h>

#include "lv2-miditype.h"
#include "lv2_event.h"
#include "lv2_uri_map.h"

#include "list.h"
#include "gtk2gui.h"
#include "lv2.h"
#include "zynjacku.h"
#include "plugin_repo.h"

#define LOG_LEVEL LOG_LEVEL_ERROR
#include "log.h"

#define LV2_UI_URI "http://lv2plug.in/ns/extensions/ui"
#define LV2_UI_GTK_URI LV2_UI_URI "#GtkUI"

struct zynjacku_gtk2gui
{
  const LV2_Feature * const * host_features;
  const char *plugin_uri;
  char *bundle_path;
  unsigned int ports_count;
  struct zynjacku_port ** ports;
  void * context_ptr;
  const char * synth_id;
  bool resizable;
  void *dlhandle;
  const LV2UI_Descriptor * lv2ui;
  LV2UI_Handle ui_handle;
  GtkWidget * widget_ptr;
  GtkWidget * window_ptr;
};

zynjacku_gtk2gui_handle
zynjacku_gtk2gui_create(
  const LV2_Feature * const * host_features,
  void *context_ptr,
  const char *uri,
  const char *synth_id,
  const struct list_head *parameter_ports_ptr)
{
  struct zynjacku_gtk2gui * ui_ptr;
  unsigned int ports_count;
  struct list_head *node_ptr;
  struct zynjacku_port * port_ptr;
  LV2UI_DescriptorFunction lookup;
  uint32_t index;
  char * ui_uri;
  char * ui_binary_path;
  char * ui_bundle_path;

  if (!zynjacku_plugin_repo_get_ui_info(uri, LV2_UI_GTK_URI, &ui_uri, &ui_binary_path, &ui_bundle_path))
  {
    LOG_ERROR("zynjacku_plugin_repo_get_ui_info() failed for '%s' and GtkGUI", uri);
    goto fail;
  }

  ui_ptr = malloc(sizeof(struct zynjacku_gtk2gui));
  if (ui_ptr == NULL)
  {
    LOG_ERROR("malloc() failed.");
    goto fail_free_ui_strings;
  }

  ui_ptr->host_features = host_features;
  ui_ptr->plugin_uri = uri;
  ui_ptr->context_ptr = context_ptr;
  ui_ptr->synth_id = synth_id;
  ui_ptr->resizable = true;

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
    goto fail_free;
  }

  memset(ui_ptr->ports, 0, ports_count * sizeof(struct zynjacku_port *));

  list_for_each(node_ptr, parameter_ports_ptr)
  {
    port_ptr = list_entry(node_ptr, struct zynjacku_port, plugin_siblings);
    ui_ptr->ports[port_ptr->index] = port_ptr;
  }

  ui_ptr->ports_count = ports_count;

  ui_ptr->bundle_path = ui_bundle_path;

  ui_ptr->dlhandle = dlopen(ui_binary_path, RTLD_NOW);
  if (ui_ptr->dlhandle == NULL)
  {
    LOG_WARNING("Cannot load \"%s\": %s", ui_binary_path, dlerror());
    goto fail_free_ports;
  }

  lookup = (LV2UI_DescriptorFunction)dlsym(ui_ptr->dlhandle, "lv2ui_descriptor");
  if (lookup == NULL)
  {
    LOG_WARNING("Cannot find symbol lv2ui_descriptor");
    goto fail_dlclose;
  }

  index = 0;

  do
  {
    ui_ptr->lv2ui = lookup(index);
    if (ui_ptr->lv2ui == NULL)
    {
      LOG_ERROR("Did not find UI %s in %s", uri, ui_binary_path);
      goto fail_dlclose;
    }

    index++;
  }
  while (strcmp(ui_ptr->lv2ui->URI, ui_uri) != 0);

  ui_ptr->ui_handle = NULL;
  ui_ptr->widget_ptr = NULL;
  ui_ptr->window_ptr = NULL;

  free(ui_uri);
  free(ui_bundle_path);

  return ui_ptr;

fail_dlclose:
  dlclose(ui_ptr->dlhandle);

fail_free_ports:
  free(ui_ptr->ports);

fail_free:
  free(ui_ptr);

fail_free_ui_strings:
  free(ui_uri);
  free(ui_bundle_path);
  free(ui_binary_path);

fail:
  return NULL;
}

#define ui_ptr ((struct zynjacku_gtk2gui *)ui_handle)

static
void
zynjacku_on_gtk2gui_window_destroy_internal(
  GtkWidget * widget,
  gpointer ui_handle)
{
  gtk_container_remove(GTK_CONTAINER(ui_ptr->window_ptr), ui_ptr->widget_ptr);
  zynjacku_gtk2gui_on_ui_destroyed(ui_ptr->context_ptr);
  ui_ptr->window_ptr = NULL;
}

void
zynjacku_gtk2gui_destroy(
  zynjacku_gtk2gui_handle ui_handle)
{
  dlclose(ui_ptr->dlhandle);
  free(ui_ptr->ports);
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

  /* se support only lv2:ControlPort ATM */
  assert(buffer_size == sizeof(float));

  LOG_DEBUG("setting port %u to %f", (unsigned int)port_index, *(float *)buffer);

  ui_ptr->ports[port_index]->data.parameter.value = *(float *)buffer;
}

bool
zynjacku_gtk2gui_ui_on(
  zynjacku_gtk2gui_handle ui_handle)
{
  LV2_Feature * features[1];
  struct zynjacku_port * port_ptr;
  unsigned int port_index;
  LV2UI_Widget widget;

  LOG_DEBUG("zynjacku_gtk2gui_ui_on(%u) called.", index);

  if (ui_ptr->ui_handle == NULL)
  {
    LOG_DEBUG("Instantiating UI...");

    features[0] = NULL;

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

    ui_ptr->widget_ptr = widget;

    LOG_DEBUG("widget: %p", ui_ptr->widget_ptr);

    assert(GTK_IS_WIDGET(ui_ptr->widget_ptr));

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

        LOG_DEBUG(
          "parameter #%u with value %f and range %f - %f",
          (unsigned int)port_ptr->index,
          port_ptr->data.parameter.value,
          port_ptr->data.parameter.min,
          port_ptr->data.parameter.max);

        ui_ptr->lv2ui->port_event(
          ui_ptr->ui_handle,
          port_ptr->index,
          sizeof(float),
          0,
          (const void *)&port_ptr->data.parameter.value);
      }
    }
  }

  if (ui_ptr->window_ptr == NULL)
  {
    ui_ptr->window_ptr = gtk_window_new(GTK_WINDOW_TOPLEVEL);

    gtk_window_set_title(GTK_WINDOW(ui_ptr->window_ptr), ui_ptr->synth_id);

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

  return true;
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

  /* Hide the widgets */
  gtk_widget_hide_all(ui_ptr->window_ptr);
}
