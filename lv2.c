/* -*- Mode: C ; c-basic-offset: 2 -*- */
/*****************************************************************************
 *
 *   This file is part of zynjacku
 *
 *   Copyright (C) 2008 Nedko Arnaudov <nedko@arnaudov.name>
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

#include <stdbool.h>
#include <stdint.h>
#include <assert.h>
#include <dlfcn.h>
#include <string.h>
#include <stdlib.h>
#include <jack/jack.h>
#include <glib-object.h>
#include <lv2.h>
#include <lv2dynparam/lv2dynparam.h>
#include <lv2dynparam/lv2_rtmempool.h>
#include <lv2dynparam/host.h>

#include "lv2-miditype.h"
#include "lv2_event.h"
#include "lv2_uri_map.h"

#include "list.h"
#include "lv2.h"
#include "gtk2gui.h"
#include "zynjacku.h"
//#define LOG_LEVEL LOG_LEVEL_DEBUG
#include "log.h"
#include "plugin_repo.h"

struct zynjacku_lv2_plugin
{
  void *dlhandle;
  const LV2_Descriptor *lv2;
  LV2_Handle lv2handle;
};

zynjacku_lv2_handle
zynjacku_lv2_load(
  const char * uri,
  double sample_rate,
  const LV2_Feature * const * features)
{
  const char *dlpath;
  const char *bundle_path;

  struct zynjacku_lv2_plugin *plugin_ptr;
  LV2_Descriptor_Function lv2lookup;
  char *error;
  uint32_t plugin_index;

  dlpath = zynjacku_plugin_repo_get_dlpath(uri);
  if (dlpath == NULL)
  {
    LOG_ERROR("Failed to get path of library implementeding plugin %s", uri);
    goto fail;
  }

  bundle_path = zynjacku_plugin_repo_get_bundle_path(uri);
  if (bundle_path == NULL)
  {
    LOG_ERROR("Failed to get bundle path of plugin %s", uri);
    goto fail;
  }

  plugin_ptr = malloc(sizeof(struct zynjacku_lv2_plugin));
  if (plugin_ptr == NULL)
  {
    LOG_ERROR("Failed to allocate memory for zynjacku_lv2_plugin structure");
    goto fail;
  }

  plugin_ptr->dlhandle = dlopen(dlpath, RTLD_NOW);
  if (plugin_ptr->dlhandle == NULL)
  {
    LOG_ERROR("Unable to open library %s (%s)", dlpath, dlerror());
    goto fail_free;
  }

  dlerror();                    /* clear error */
  lv2lookup = dlsym(plugin_ptr->dlhandle, "lv2_descriptor");
  error = dlerror();
  if (error != NULL)
  {
    LOG_ERROR("Cannot retrieve descriptor function of LV2 plugin %s (%s)", dlpath, dlerror());
    goto fail_dlclose;
  }

  if (lv2lookup == NULL)
  {
    LOG_ERROR("Descriptor function of LV2 plugin %s is NULL", dlpath);
    goto fail_dlclose;
  }

  plugin_index = 0;

  do
  {
    plugin_ptr->lv2 = lv2lookup(plugin_index);
    if (plugin_ptr->lv2 == NULL)
    {
      LOG_ERROR("Did not find plugin %s in %s", uri, dlpath);
      goto fail_dlclose;
    }

    plugin_index++;
  }
  while (strcmp(plugin_ptr->lv2->URI, uri) != 0);

  plugin_ptr->lv2handle = plugin_ptr->lv2->instantiate(plugin_ptr->lv2, sample_rate, bundle_path, features);
  if (plugin_ptr->lv2handle == NULL)
  {
    LOG_ERROR("Instantiation of %s failed.", uri);
    goto fail_dlclose;
  }

  return (zynjacku_lv2_handle)plugin_ptr;

fail_dlclose:
  dlclose(plugin_ptr->dlhandle);

fail_free:
  free(plugin_ptr);

fail:
  return NULL;
}

#define plugin_ptr ((struct zynjacku_lv2_plugin *)lv2handle)

void
zynjacku_lv2_unload(
  zynjacku_lv2_handle lv2handle)
{
  plugin_ptr->lv2->cleanup(plugin_ptr->lv2handle);
  dlclose(plugin_ptr->dlhandle);
  free(plugin_ptr);
}

void
zynjacku_lv2_connect_port(
  zynjacku_lv2_handle lv2handle,
  uint32_t port,
  void *data_location)
{
  LOG_DEBUG("Connecting port %d", port);
  plugin_ptr->lv2->connect_port(plugin_ptr->lv2handle, port, data_location);
}

void
zynjacku_lv2_run(
  zynjacku_lv2_handle lv2handle,
  uint32_t sample_count)
{
  plugin_ptr->lv2->run(plugin_ptr->lv2handle, sample_count);
}

void
zynjacku_lv2_activate(
  zynjacku_lv2_handle lv2handle)
{
  if (plugin_ptr->lv2->activate == NULL)
  {
    return;
  }

  plugin_ptr->lv2->activate(plugin_ptr->lv2handle);
}

void
zynjacku_lv2_deactivate(
  zynjacku_lv2_handle lv2handle)
{
  if (plugin_ptr->lv2->deactivate == NULL)
  {
    return;
  }

  plugin_ptr->lv2->deactivate(plugin_ptr->lv2handle);
}

const LV2_Descriptor *
zynjacku_lv2_get_descriptor(
  zynjacku_lv2_handle lv2handle)
{
  return plugin_ptr->lv2;
}

LV2_Handle
zynjacku_lv2_get_handle(
  zynjacku_lv2_handle lv2handle)
{
  return plugin_ptr->lv2handle;
}
