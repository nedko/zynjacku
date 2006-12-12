/* -*- Mode: C ; c-basic-offset: 2 -*- */
/*****************************************************************************
 *
 *   Implementations of lv2dynparam host callbacks, the ones called by plugin
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
#include "dynparam_host_callbacks.h"
#include "dynparam_preallocate.h"

#define LOG_LEVEL LOG_LEVEL_DEBUG
#include "log.h"

#define instance_ptr ((struct lv2dynparam_host_instance *)instance_host_context)

unsigned char
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
    LOG_DEBUG("The top level group.");
    instance_ptr->root_group_ptr = group_ptr;
  }
  else
  {
    LOG_DEBUG("Parent is \"%s\".", parent_group_ptr->name);
    list_add_tail(&group_ptr->siblings, &parent_group_ptr->child_groups);
  }

  message_ptr->message_type = LV2DYNPARAM_HOST_MESSAGE_TYPE_GROUP_APPEAR;
  message_ptr->context.group = group_ptr;
  list_add_tail(&message_ptr->siblings, &instance_ptr->realtime_to_ui_queue);

  *group_host_context = group_ptr;

  return TRUE;

fail_put_group:
  lv2dynparam_put_unused_group(group_ptr);

fail:
  return FALSE;
}

unsigned char
lv2dynparam_host_group_disappear(
  void * instance_host_context,
  void * group_host_context)
{
  return TRUE;
}

unsigned char
lv2dynparam_host_parameter_appear(
  void * instance_host_context,
  void * group_host_context,
  lv2dynparam_parameter_handle parameter,
  void ** parameter_host_context)
{
  struct lv2dynparam_host_parameter * param_ptr;
  struct lv2dynparam_host_group * group_ptr;

  LOG_DEBUG("Parameter appeared.");

  group_ptr = (struct lv2dynparam_host_group *)group_host_context;

  LOG_DEBUG("Parent is \"%s\".", group_ptr->name);

  param_ptr = lv2dynparam_get_unused_parameter();
  if (param_ptr == NULL)
  {
    goto fail;
  }

  param_ptr->param_handle = parameter;
  param_ptr->gui_referenced = FALSE;

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

  lv2dynparam_host_map_type_uri(param_ptr);

  instance_ptr->callbacks_ptr->parameter_get_value(
    parameter,
    &param_ptr->value_ptr);

  if (param_ptr->type == LV2DYNPARAM_PARAMETER_TYPE_FLOAT ||
      param_ptr->type == LV2DYNPARAM_PARAMETER_TYPE_INT ||
      param_ptr->type == LV2DYNPARAM_PARAMETER_TYPE_NOTE)
  {
    /* ranged type */
    instance_ptr->callbacks_ptr->parameter_get_range(
      parameter,
      &param_ptr->min_ptr,
      &param_ptr->max_ptr);
  }
  else
  {
    /* unranged type, range predefined or not applicable */
    param_ptr->min_ptr = NULL;
    param_ptr->max_ptr = NULL;
  }

  /* read current value */
  switch (param_ptr->type)
  {
  case LV2DYNPARAM_PARAMETER_TYPE_BOOLEAN:
    param_ptr->value.boolean = *(unsigned char *)(param_ptr->value_ptr);
    LOG_DEBUG("Boolean parameter with value %s", param_ptr->value.boolean ? "TRUE" : "FALSE");
    break;
  case LV2DYNPARAM_PARAMETER_TYPE_FLOAT:
    param_ptr->value.fpoint = *(float *)(param_ptr->value_ptr);
    LOG_DEBUG("Float parameter with value %f", param_ptr->value.fpoint);
    break;
  }

  /* read current range */
  switch (param_ptr->type)
  {
  case LV2DYNPARAM_PARAMETER_TYPE_FLOAT:
    param_ptr->min.fpoint = *(float *)(param_ptr->min_ptr);
    param_ptr->max.fpoint = *(float *)(param_ptr->max_ptr);
    LOG_DEBUG("Float parameter with range %f - %f", param_ptr->min.fpoint, param_ptr->max.fpoint);
    break;
  }

  /* Add parameter as child of its group */
  param_ptr->group_ptr = group_ptr;
  list_add_tail(&param_ptr->siblings, &group_ptr->child_params);

  //list_add_tail(&message_ptr->siblings, &instance_ptr->realtime_to_ui_queue);

  return TRUE;

fail_put:
  lv2dynparam_put_unused_parameter(param_ptr);

fail:
  return FALSE;
}

unsigned char
lv2dynparam_host_parameter_disappear(
  void * instance_host_context,
  void * parameter_host_context)
{
  return TRUE;
}

unsigned char
lv2dynparam_host_parameter_change(
  void * instance_host_context,
  void * parameter_host_context)
{
  return TRUE;
}

unsigned char
lv2dynparam_host_command_appear(
  void * instance_host_context,
  void * group_host_context,
  lv2dynparam_command_handle command,
  void ** command_context)
{
  LOG_DEBUG("Command apperead.");
  return FALSE;                 /* not implemented */
}

unsigned char
lv2dynparam_host_command_disappear(
  void * instance_host_context,
  void * command_host_context)
{
  return TRUE;
}
