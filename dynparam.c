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

#include <stdlib.h>

#include "lv2.h"
#include "lv2dynparam.h"
#include "dynparam.h"
#include "audiolock.h"
#include "list.h"
#include "dynparam_internal.h"

#define LOG_LEVEL LOG_LEVEL_DEBUG
#include "log.h"

static struct lv2dynparam_host_callbacks g_lv2dynparam_host_callbacks =
{
  .group_appear = lv2dynparam_host_group_appear,
  .group_disappear = lv2dynparam_host_group_disappear,

  .parameter_appear = lv2dynparam_host_parameter_appear,
  .parameter_disappear = lv2dynparam_host_parameter_disappear,
  .parameter_change = lv2dynparam_host_parameter_change,

  .command_appear = lv2dynparam_host_command_appear,
  .command_disappear = lv2dynparam_host_command_disappear
};

int
lv2dynparam_host_add_synth(
  const LV2_Descriptor * lv2descriptor,
  LV2_Handle lv2instance,
  void * instance_ui_context,
  lv2dynparam_host_instance * instance_handle_ptr)
{
  struct lv2dynparam_host_instance * instance_ptr;

  lv2dynparam_preallocate();

  instance_ptr = malloc(sizeof(struct lv2dynparam_host_instance));
  if (instance_ptr == NULL)
  {
    /* out of memory */
    goto fail;
  }

  instance_ptr->instance_ui_context = instance_ui_context;

  instance_ptr->lock = audiolock_create_slow();
  if (instance_ptr->lock == AUDIOLOCK_HANDLE_INVALID_VALUE)
  {
    goto fail_free;
  }

  instance_ptr->callbacks_ptr = lv2descriptor->extension_data(LV2DYNPARAM_URI);
  if (instance_ptr->callbacks_ptr == NULL)
  {
    /* Oh my! plugin does not support dynparam extension! This should be pre-checked by caller! */
    goto fail_destroy_lock;
  }

  instance_ptr->lv2instance = lv2instance;

  if (!instance_ptr->callbacks_ptr->host_attach(
        lv2instance,
        &g_lv2dynparam_host_callbacks,
        instance_ptr))
  {
    LOG_ERROR("lv2dynparam host_attach() failed.");
    goto fail_destroy_lock;
  }

  *instance_handle_ptr = (lv2dynparam_host_instance)instance_ptr;

  return 1;

fail_destroy_lock:
  audiolock_destroy(instance_ptr->lock);

fail_free:
  free(instance_ptr);

fail:
  return 0;
}

#define instance_ptr ((struct lv2dynparam_host_instance *)instance)

void
lv2dynparam_host_realtime_run(
  lv2dynparam_host_instance instance)
{
  if (!audiolock_enter_audio(instance_ptr->lock))
  {
    /* we are not lucky enough ui thread, is accessing the protected data */
    return;
  }

  audiolock_leave_audio(instance_ptr->lock);
}

void
lv2dynparam_host_ui_run(
  lv2dynparam_host_instance instance)
{
  audiolock_enter_ui(instance_ptr->lock);
  audiolock_leave_ui(instance_ptr->lock);
}

#undef instance_ptr
#define instance_ptr ((struct lv2dynparam_host_instance *)instance_host_context)

BOOL
lv2dynparam_host_group_appear(
  void * instance_host_context,
  void * parent_group_host_context,
  lv2dynparam_group_handle group,
  void ** group_host_context)
{
  struct lv2dynparam_host_group * group_ptr;

  LOG_DEBUG("Group appeared.");

  group_ptr = lv2dynparam_get_unused_group();
  if (group_ptr == NULL)
  {
    goto fail;
  }

  group_ptr->group_handle = group;

  if (!instance_ptr->callbacks_ptr->group_get_name(
        group_ptr->group_handle,
        group_ptr->name,
        LV2DYNPARAM_MAX_STRING_SIZE))
  {
    LOG_ERROR("lv2dynparam get_group_name() failed.");
    goto fail_put;
  }

  LOG_DEBUG("Group name is \"%s\"", group_ptr->name);

//  dynparam_generic_group_appeared(instance_ui_context, instance_ptr->root_group.name, &instance_ptr->root_group.ui_context);

  return TRUE;

fail_put:
  lv2dynparam_put_unused_group(group_ptr);

fail:
  return FALSE;
}

void
lv2dynparam_host_group_disappear(
  void * instance_host_context,
  void * group_host_context)
{
}

BOOL
lv2dynparam_host_parameter_appear(
  void * instance_host_context,
  void * group_host_context,
  lv2dynparam_parameter_handle parameter,
  void ** parameter_host_context)
{
  struct lv2dynparam_host_parameter * param_ptr;

  LOG_DEBUG("Parameter appeared.");

  param_ptr = lv2dynparam_get_unused_parameter();
  if (param_ptr == NULL)
  {
    goto fail;
  }

  param_ptr->param_handle = parameter;

  if (!instance_ptr->callbacks_ptr->parameter_get_name(
        param_ptr->param_handle,
        param_ptr->name,
        LV2DYNPARAM_MAX_STRING_SIZE))
  {
    LOG_ERROR("lv2dynparam get_param_name() failed.");
    goto fail_put;
  }

  LOG_DEBUG("Parameter name is \"%s\"", param_ptr->name);

//  dynparam_generic_parameter_appeared(instance_ui_context, instance_ptr->root_param.name, &instance_ptr->root_param.ui_context);

  return TRUE;

fail_put:
  lv2dynparam_put_unused_parameter(param_ptr);

fail:
  return FALSE;
}

void
lv2dynparam_host_parameter_disappear(
  void * instance_host_context,
  void * parameter_host_context)
{
}

void
lv2dynparam_host_parameter_change(
  void * instance_host_context,
  void * parameter_host_context)
{
}

BOOL
lv2dynparam_host_command_appear(
  void * instance_host_context,
  void * group_host_context,
  lv2dynparam_command_handle command,
  void ** command_context)
{
  LOG_DEBUG("Command apperead.");
  return FALSE;                 /* not implemented */
}

void
lv2dynparam_host_command_disappear(
  void * instance_host_context,
  void * command_host_context)
{
}
