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
#include <string.h>

#include "lv2.h"
#include "lv2dynparam.h"
#include "dynparam.h"
#include "audiolock.h"
#include "list.h"
#include "dynparam_internal.h"
#include "dynparam_host_callbacks.h"
#include "dynparam_preallocate.h"

#define LOG_LEVEL LOG_LEVEL_DEBUG
#include "log.h"

void
lv2dynparam_host_map_type_uri(
  struct lv2dynparam_host_parameter * parameter_ptr)
{
  if (strcmp(parameter_ptr->type_uri, LV2DYNPARAM_PARAMETER_TYPE_BOOLEAN_URI) == 0)
  {
    parameter_ptr->type = LV2DYNPARAM_PARAMETER_TYPE_BOOLEAN;
    return;
  }
}

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
  instance_ptr->ui = FALSE;

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

void
lv2dynparam_host_notify_group_appeared(
  struct lv2dynparam_host_instance * instance_ptr,
  struct lv2dynparam_host_group * group_ptr)
{
  void * parent_group_ui_context;

  LOG_DEBUG("lv2dynparam_host_notify_group_appeared() called.");

  if (group_ptr->parent_group_ptr)
  {
    parent_group_ui_context = group_ptr->parent_group_ptr->ui_context;
  }
  else
  {
    /* Master Yoda says: The root group it is */
    parent_group_ui_context = NULL;
  }

  /* C block because this is the last of not implemented yet if statements for vairous group types */
  {
    dynparam_generic_group_appeared(
      group_ptr,
      instance_ptr->instance_ui_context,
      parent_group_ui_context,
      group_ptr->name,
      &group_ptr->ui_context);
  }

  group_ptr->gui_referenced = TRUE;
}

void
lv2dynparam_host_notify_parameter_appeared(
  struct lv2dynparam_host_instance * instance_ptr,
  struct lv2dynparam_host_parameter * parameter_ptr)
{
  if (parameter_ptr->type == LV2DYNPARAM_PARAMETER_TYPE_BOOLEAN)
  {
    dynparam_parameter_boolean_appeared(
      parameter_ptr,
      instance_ptr->instance_ui_context,
      parameter_ptr->group_ptr->ui_context,
      parameter_ptr->name,
      parameter_ptr->value.boolean,
      &parameter_ptr->ui_context);
  }

  parameter_ptr->gui_referenced = TRUE;
}

void
lv2dynparam_host_notify(
  struct lv2dynparam_host_instance * instance_ptr,
  struct lv2dynparam_host_group * group_ptr)
{
  struct list_head * node_ptr;
  struct lv2dynparam_host_group * child_group_ptr;
  struct lv2dynparam_host_parameter * parameter_ptr;

  LOG_DEBUG("Iterating \"%s\" groups begin", group_ptr->name);

  list_for_each(node_ptr, &group_ptr->child_groups)
  {
    child_group_ptr = list_entry(node_ptr, struct lv2dynparam_host_group, siblings);
    LOG_DEBUG("host notify - group \"%s\"", child_group_ptr->name);

    if (!child_group_ptr->gui_referenced)
    {
      /* UI knows nothing about this group - notify it */
      lv2dynparam_host_notify_group_appeared(
        instance_ptr,
        child_group_ptr);
    }

    lv2dynparam_host_notify(
      instance_ptr,
      child_group_ptr);
  }

  LOG_DEBUG("Iterating \"%s\" groups end", group_ptr->name);
  LOG_DEBUG("Iterating \"%s\" params begin", group_ptr->name);

  list_for_each(node_ptr, &group_ptr->child_params)
  {
    parameter_ptr = list_entry(node_ptr, struct lv2dynparam_host_parameter, siblings);
    LOG_DEBUG("host notify - parameter");

    if (!parameter_ptr->gui_referenced)
    {
      lv2dynparam_host_notify_parameter_appeared(
        instance_ptr,
        parameter_ptr);
    }
  }

  LOG_DEBUG("Iterating \"%s\" params end", group_ptr->name);
}

#define instance_ptr ((struct lv2dynparam_host_instance *)instance)

void
lv2dynparam_host_realtime_run(
  lv2dynparam_host_instance instance)
{
  struct list_head * node_ptr;
  struct lv2dynparam_host_message * message_ptr;
  struct lv2dynparam_host_parameter * parameter_ptr;

  if (!audiolock_enter_audio(instance_ptr->lock))
  {
    /* we are not lucky enough - ui thread, is accessing the protected data */
    return;
  }

  while (!list_empty(&instance_ptr->ui_to_realtime_queue))
  {
    node_ptr = instance_ptr->ui_to_realtime_queue.next;
    list_del(node_ptr);
    message_ptr = list_entry(node_ptr, struct lv2dynparam_host_message, siblings);

    switch (message_ptr->message_type)
    {
    case LV2DYNPARAM_HOST_MESSAGE_TYPE_PARAMETER_CHANGE:
      parameter_ptr = message_ptr->context.parameter;

      switch (parameter_ptr->type)
      {
      case LV2DYNPARAM_PARAMETER_TYPE_BOOLEAN:
        *((unsigned char *)parameter_ptr->value_ptr) = parameter_ptr->value.boolean ? 1 : 0;
        break;
      default:
        LOG_ERROR("Parameter change for parameter of unknown type %u received", message_ptr->message_type);
      }

      instance_ptr->callbacks_ptr->parameter_change(parameter_ptr->param_handle);
      break;
    default:
       LOG_ERROR("Message of unknown type %u received", message_ptr->message_type);
     }

    lv2dynparam_put_unused_message(message_ptr);
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
  struct lv2dynparam_host_group * group_ptr;

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
        lv2dynparam_host_notify_group_appeared(
          instance_ptr,
          message_ptr->context.group);

        group_ptr->gui_referenced = TRUE;

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
      LOG_DEBUG("ignoring message of type %u because UI is off.", message_ptr->message_type);
    }

    lv2dynparam_put_unused_message(message_ptr);
  }
  
  audiolock_leave_ui(instance_ptr->lock);
}

void
lv2dynparam_host_ui_on(
  lv2dynparam_host_instance instance)
{
  audiolock_enter_ui(instance_ptr->lock);

  LOG_DEBUG("UI on - notifying for new things.");

  if (instance_ptr->root_group_ptr != NULL)
  {
    if (!instance_ptr->root_group_ptr->gui_referenced)
    {
      /* UI knows nothing about this group - notify it */
      lv2dynparam_host_notify_group_appeared(
        instance_ptr,
        instance_ptr->root_group_ptr);
    }

    lv2dynparam_host_notify(
      instance_ptr,
      instance_ptr->root_group_ptr);
  }

  audiolock_leave_ui(instance_ptr->lock);
}

void
lv2dynparam_host_ui_off(
  lv2dynparam_host_instance instance)
{
  audiolock_enter_ui(instance_ptr->lock);
  audiolock_leave_ui(instance_ptr->lock);
}

#define parameter_ptr ((struct lv2dynparam_host_parameter *)parameter_handle)

void
dynparam_parameter_boolean_change(
  lv2dynparam_host_instance instance,
  lv2dynparam_host_parameter parameter_handle,
  BOOL value)
{
  struct lv2dynparam_host_message * message_ptr;

  audiolock_enter_ui(instance_ptr->lock);

  LOG_DEBUG("\"%s\" changed to \"%s\"", parameter_ptr->name, value ? "TRUE" : "FALSE");

  message_ptr = lv2dynparam_get_unused_message_may_block();

  parameter_ptr->value.boolean = value;

  message_ptr->message_type = LV2DYNPARAM_HOST_MESSAGE_TYPE_PARAMETER_CHANGE;

  message_ptr->context.parameter = parameter_ptr;

  list_add_tail(&message_ptr->siblings, &instance_ptr->ui_to_realtime_queue);

  audiolock_leave_ui(instance_ptr->lock);
}
