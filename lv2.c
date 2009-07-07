/* -*- Mode: C ; c-basic-offset: 2 -*- */
/*****************************************************************************
 *
 *   This file is part of zynjacku
 *
 *   Copyright (C) 2008,2009 Nedko Arnaudov <nedko@arnaudov.name>
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
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>
#include <dlfcn.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <jack/jack.h>
#include <glib-object.h>
#include <lv2.h>
#if HAVE_DYNPARAMS
#include <lv2dynparam/lv2dynparam.h>
#include <lv2dynparam/lv2_rtmempool.h>
#include <lv2dynparam/host.h>
#endif
#include <lv2_dyn_manifest.h>

#include "lv2-miditype.h"
#include "lv2_event.h"
#include "lv2_uri_map.h"
#include "lv2_contexts.h"
#include "lv2_string_port.h"

#include "list.h"
#include "lv2.h"
#include "gtk2gui.h"
#include "zynjacku.h"
//#define LOG_LEVEL LOG_LEVEL_DEBUG
#include "log.h"
#include "plugin.h"

struct zynjacku_lv2_plugin
{
  void *dlhandle;
  const LV2_Descriptor *lv2;
  const LV2MessageContext *lv2msg;
  LV2_Handle lv2handle;
};

struct zynjacku_lv2_dman
{
  void *dlhandle;
  LV2_Dyn_Manifest_Handle handle;
  int (*open)(LV2_Dyn_Manifest_Handle *handle,
              const LV2_Feature *const * features);
  int (*get_subjects)(LV2_Dyn_Manifest_Handle handle, FILE *fp);
  int (*get_data)(LV2_Dyn_Manifest_Handle handle, FILE *fp, const char *uri);
  void (*close)(LV2_Dyn_Manifest_Handle handle);
};

#define dman_ptr ((struct zynjacku_lv2_dman *)dman)

zynjacku_lv2_dman_handle
zynjacku_lv2_dman_open(
  const char * dlpath)
{
  struct zynjacku_lv2_dman tmp, *ret;
  const LV2_Feature * const * features = {NULL};
  int err;

  tmp.dlhandle = dlopen(dlpath, RTLD_NOW);
  if (tmp.dlhandle == NULL)
  {
    LOG_ERROR("Unable to open library %s (%s)", dlpath, dlerror());
    return NULL;
  }

  dlerror();
  tmp.open = dlsym(tmp.dlhandle, "lv2_dyn_manifest_open");
  if (tmp.open == NULL)
  {
    LOG_ERROR("Cannot retrieve dynamic manifest open function of LV2 plugin %s (%s)", dlpath, dlerror());
    dlclose(tmp.dlhandle);
    return NULL;
  }

  dlerror();
  tmp.get_subjects = dlsym(tmp.dlhandle, "lv2_dyn_manifest_get_subjects");
  if (tmp.get_subjects == NULL)
  {
    LOG_ERROR("Cannot retrieve dynamic manifest get subjects function of LV2 plugin %s (%s)", dlpath, dlerror());
    dlclose(tmp.dlhandle);
    return NULL;
  }

  dlerror();
  tmp.get_data = dlsym(tmp.dlhandle, "lv2_dyn_manifest_get_data");
  if (tmp.open == NULL)
  {
    LOG_ERROR("Cannot retrieve dynamic manifest get data function of LV2 plugin %s (%s)", dlpath, dlerror());
    dlclose(tmp.dlhandle);
    return NULL;
  }

  dlerror();
  tmp.close = dlsym(tmp.dlhandle, "lv2_dyn_manifest_close");
  if (tmp.close == NULL)
  {
    LOG_ERROR("Cannot retrieve dynamic manifest close function of LV2 plugin %s (%s)", dlpath, dlerror());
    dlclose(tmp.dlhandle);
    return NULL;
  }

  err = tmp.open(&tmp.handle, features);
  if (err != 0)
  {
    LOG_ERROR("Error while opening dynamic manifest of LV2 plugin %s (%d)", dlpath, err);
    dlclose(tmp.dlhandle);
    return NULL;
  }

  ret = malloc(sizeof(struct zynjacku_lv2_dman));
  if (ret == NULL)
  {
    LOG_ERROR("Failed to allocate memory for dynamic manifest of LV2 plugin %s", dlpath);
    tmp.close(tmp.handle);
    dlclose(tmp.dlhandle);
    return NULL;
  }

  *ret = tmp;

  return (zynjacku_lv2_dman_handle)ret;
}

char *
zynjacku_lv2_dman_get_subjects(
  zynjacku_lv2_dman_handle dman)
{
  FILE *fp;
  char *ret;
  size_t size;
  int err;

  fp = tmpfile();
  if (fp == NULL)
  {
    LOG_ERROR("Failed to generate temporary file for dynamic manifest (%s)", strerror(errno));
    return NULL;
  }

  err = dman_ptr->get_subjects(dman_ptr->handle, fp);
  if (err != 0)
  {
    LOG_ERROR("Failed to fetch subject URIs from dynamic manifest (%d)", err);
    fclose(fp);
    return NULL;
  }

  if (fseek(fp, 0, SEEK_END) < 0)
  {
    LOG_ERROR("Cannot determine the size of dynamic manifest file (%s)", strerror(errno));
    fclose(fp);
    return NULL;
  }
  size = ftell(fp);
  if (size < 0)
  {
    LOG_ERROR("Cannot determine the size of dynamic manifest file (%s)", strerror(errno));
    fclose(fp);
    return NULL;
  }
  rewind(fp);

  ret = malloc(size + 1);
  if (ret == NULL)
  {
    LOG_ERROR("Failed to allocate memory to store the dynamically generated manifest file");
    fclose(fp);
    return NULL;
  }

  size = fread(ret, 1, size, fp);
  ret[size] = '\0';
  fclose(fp);

  return ret;
}

char *
zynjacku_lv2_dman_get_data(
  zynjacku_lv2_dman_handle dman,
  const char *uri)
{
  FILE *fp;
  char *ret;
  size_t size;
  int err;

  fp = tmpfile();
  if (fp == NULL)
  {
    LOG_ERROR("Failed to generate temporary file for dynamic manifest (%s)", strerror(errno));
    return NULL;
  }

  err = dman_ptr->get_data(dman_ptr->handle, fp, uri);
  if (err != 0)
  {
    LOG_ERROR("Failed to fetch data from dynamic manifest (%d)", err);
    fclose(fp);
    return NULL;
  }

  if (fseek(fp, 0, SEEK_END) < 0)
  {
    LOG_ERROR("Cannot determine the size of dynamic manifest file (%s)", strerror(errno));
    fclose(fp);
    return NULL;
  }
  size = ftell(fp);
  if (size < 0)
  {
    LOG_ERROR("Cannot determine the size of dynamic manifest file (%s)", strerror(errno));
    fclose(fp);
    return NULL;
  }
  rewind(fp);

  ret = malloc(size + 1);
  if (ret == NULL)
  {
    LOG_ERROR("Failed to allocate memory to store the dynamically generated manifest file");
    fclose(fp);
    return NULL;
  }

  size = fread(ret, 1, size, fp);
  ret[size] = '\0';
  fclose(fp);

  return ret;
}

void
zynjacku_lv2_dman_close(
  zynjacku_lv2_dman_handle dman)
{
  dman_ptr->close(dman_ptr->handle);
  /* TODO: Cannot dlclose() here, otherwise descriptors are not guaranteed to be
   *       consistent. This needs to be managed somehow. */
  free(dman_ptr);
}

zynjacku_lv2_handle
zynjacku_lv2_load(
  const char * uri,
  const char * dlpath,
  const char * bundle_path,
  double sample_rate,
  const LV2_Feature * const * features)
{
  struct zynjacku_lv2_plugin *plugin_ptr;
  LV2_Descriptor_Function lv2lookup;
  char *error;
  uint32_t plugin_index;

  plugin_ptr = malloc(sizeof(struct zynjacku_lv2_plugin));
  if (plugin_ptr == NULL)
  {
    LOG_ERROR("Failed to allocate memory for zynjacku_lv2_plugin structure");
    goto fail;
  }
  /* zero everything to initialize pointers and ensure predictability */
  memset(plugin_ptr, 0, sizeof(struct zynjacku_lv2_plugin));

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

  if (plugin_ptr->lv2->extension_data != NULL)
  {
    plugin_ptr->lv2msg = plugin_ptr->lv2->extension_data("http://lv2plug.in/ns/dev/contexts#MessageContext");
  }
  else
  {
    plugin_ptr->lv2msg = NULL;
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
  struct zynjacku_port *port,
  void *data_location)
{
  LOG_DEBUG("Connecting port %d", port);
  if (port->flags & PORT_FLAGS_MSGCONTEXT)
    (*plugin_ptr->lv2msg->message_connect_port)(plugin_ptr->lv2handle, port->index, data_location);
  else
    plugin_ptr->lv2->connect_port(plugin_ptr->lv2handle, port->index, data_location);
}

void
zynjacku_lv2_message(
  zynjacku_lv2_handle lv2handle,
  const void *input_data,
  void *output_data)
{
  plugin_ptr->lv2msg->message_run(plugin_ptr->lv2handle, input_data, output_data);
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
