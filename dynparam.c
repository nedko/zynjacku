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

  INIT_LIST_HEAD(&instance_ptr->realtime_to_ui_queue);
  INIT_LIST_HEAD(&instance_ptr->ui_to_realtime_queue);
  instance_ptr->lv2instance = lv2instance;
  instance_ptr->root_group_ptr = NULL;
  instance_ptr->ui = TRUE;

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
  struct list_head * node_ptr;
  struct lv2dynparam_host_message * message_ptr;
/*   struct lv2dynparam_host_command * command_ptr; */
/*   struct lv2dynparam_host_parameter * parameter_ptr; */
/*   struct lv2dynparam_host_group * group_ptr; */

  //LOG_DEBUG("lv2dynparam_host_ui_run() called.");

  audiolock_enter_ui(instance_ptr->lock);

  while (!list_empty(&instance_ptr->realtime_to_ui_queue))
  {
    node_ptr = instance_ptr->realtime_to_ui_queue.next;
    list_del(node_ptr);
    message_ptr = list_entry(node_ptr, struct lv2dynparam_host_message, siblings);

    if (instance_ptr->ui)
    {
      switch (message_ptr->message_type)
      {
      case LV2DYNPARAM_HOST_MESSAGE_TYPE_GROUP_APPEAR:
        {
          dynparam_generic_group_appeared(
            message_ptr->context.group_ptr,
            instance_ptr->instance_ui_context,
            message_ptr->context.group_ptr->parent_group_ptr->ui_context,
            message_ptr->context.group_ptr->name,
            &message_ptr->context.group_ptr->ui_context);
        }

        message_ptr->context.group_ptr->gui_referenced = TRUE;

        break;
      case LV2DYNPARAM_HOST_MESSAGE_TYPE_GROUP_DISAPPEAR:
      case LV2DYNPARAM_HOST_MESSAGE_TYPE_PARAMETER_APPEAR:
      case LV2DYNPARAM_HOST_MESSAGE_TYPE_PARAMETER_DISAPPEAR:
      case LV2DYNPARAM_HOST_MESSAGE_TYPE_PARAMETER_CHANGE:
      case LV2DYNPARAM_HOST_MESSAGE_TYPE_COMMAND_APPEAR:
      case LV2DYNPARAM_HOST_MESSAGE_TYPE_COMMAND_DISAPPEAR:
      default:
        LOG_ERROR("Message of unknown type %u received", message_ptr->message_type);
      }
    }
    else
    {
      LOG_DEBUG("ignoring message because UI is off.");
    }

    lv2dynparam_put_unused_message(message_ptr);
  }
  
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
  struct lv2dynparam_host_group * parent_group_ptr;
  struct lv2dynparam_host_message * message_ptr;

  LOG_DEBUG("Group appeared.");

  parent_group_ptr = (struct lv2dynparam_host_group *)parent_group_host_context;

  group_ptr = lv2dynparam_get_unused_group();
  if (group_ptr == NULL)
  {
    goto fail;
  }

  group_ptr->parent_group_ptr = parent_group_ptr;
  group_ptr->group_handle = group;
  INIT_LIST_HEAD(&group_ptr->child_groups);
  INIT_LIST_HEAD(&group_ptr->child_params);
  INIT_LIST_HEAD(&group_ptr->child_commands);
  group_ptr->gui_referenced = FALSE;

  if (!instance_ptr->callbacks_ptr->group_get_name(
        group_ptr->group_handle,
        group_ptr->name,
        LV2DYNPARAM_MAX_STRING_SIZE))
  {
    LOG_ERROR("lv2dynparam get_group_name() failed.");
    goto fail_put_group;
  }

  LOG_DEBUG("Group name is \"%s\"", group_ptr->name);

  if (!instance_ptr->callbacks_ptr->group_get_type_uri(
        group_ptr->group_handle,
        group_ptr->type,
        LV2DYNPARAM_MAX_STRING_SIZE))
  {
    LOG_ERROR("lv2dynparam get_group_type_uri() failed.");
    goto fail_put_group;
  }

  LOG_DEBUG("Group type is \"%s\"", group_ptr->type);

  message_ptr = lv2dynparam_get_unused_message();
  if (message_ptr == NULL)
  {
    goto fail_put_group;
  }

  if (parent_group_ptr == NULL)
  {
    instance_ptr->root_group_ptr = group_ptr;
  }
  else
  {
    list_add_tail(&group_ptr->siblings, &parent_group_ptr->child_groups);
  }

  list_add_tail(&message_ptr->siblings, &instance_ptr->realtime_to_ui_queue);

  return TRUE;

fail_put_group:
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

  if (!instance_ptr->callbacks_ptr->parameter_get_type_uri(
        param_ptr->param_handle,
        param_ptr->type_uri,
        LV2DYNPARAM_MAX_STRING_SIZE))
  {
    LOG_ERROR("lv2dynparam get_parameter_type_uri() failed.");
    goto fail_put;
  }

  LOG_DEBUG("Parameter type is \"%s\"", param_ptr->type_uri);

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
