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
#include "list.h"
#include "audiolock.h"
#include "dynparam.h"
#include "dynparam_internal.h"
#include "dynparam_preallocate.h"

#define LV2DYNPARAM_GROUPS_PREALLOCATE      1024
#define LV2DYNPARAM_PARAMETERS_PREALLOCATE  1024
#define LV2DYNPARAM_MESSAGES_PREALLOCATE    1024

/* these are protected by all audiolocks */
/* !!!!!!!!!!!!! not really, global mutex or per-plugin handle is required */
LIST_HEAD(g_unused_groups);
unsigned int g_unused_groups_count;
LIST_HEAD(g_unused_parameters);
unsigned int g_unused_parameters_count;
LIST_HEAD(g_unused_messages);
unsigned int g_unused_messages_count;

void
lv2dynparam_preallocate()
{
  struct lv2dynparam_host_group * group_ptr;
  struct lv2dynparam_host_parameter * parameter_ptr;
  struct lv2dynparam_host_message * message_ptr;

  while (g_unused_groups_count < LV2DYNPARAM_GROUPS_PREALLOCATE)
  {
    group_ptr = malloc(sizeof(struct lv2dynparam_host_group));
    if (group_ptr == NULL)
    {
      break;
    }

    list_add_tail(&group_ptr->siblings, &g_unused_groups);
    g_unused_groups_count++;
  }

  while (g_unused_parameters_count < LV2DYNPARAM_PARAMETERS_PREALLOCATE)
  {
    parameter_ptr = malloc(sizeof(struct lv2dynparam_host_parameter));
    if (parameter_ptr == NULL)
    {
      break;
    }

    list_add_tail(&parameter_ptr->siblings, &g_unused_parameters);
    g_unused_parameters_count++;
  }

  while (g_unused_messages_count < LV2DYNPARAM_MESSAGES_PREALLOCATE)
  {
    message_ptr = malloc(sizeof(struct lv2dynparam_host_message));
    if (message_ptr == NULL)
    {
      break;
    }

    list_add_tail(&message_ptr->siblings, &g_unused_messages);
    g_unused_messages_count++;
  }
}

struct lv2dynparam_host_group *
lv2dynparam_get_unused_group()
{
  struct lv2dynparam_host_group * group_ptr;

  if (list_empty(&g_unused_groups))
  {
    return NULL;
  }

  group_ptr = list_entry(g_unused_groups.next, struct lv2dynparam_host_group, siblings);
  list_del(g_unused_groups.next);
  g_unused_groups_count--;

  return group_ptr;
}

void
lv2dynparam_put_unused_group(
  struct lv2dynparam_host_group * group_ptr)
{
  list_add_tail(&group_ptr->siblings, &g_unused_groups);
  g_unused_groups_count++;
}

struct lv2dynparam_host_parameter *
lv2dynparam_get_unused_parameter()
{
  struct lv2dynparam_host_parameter * parameter_ptr;

  if (list_empty(&g_unused_parameters))
  {
    return NULL;
  }

  parameter_ptr = list_entry(g_unused_parameters.next, struct lv2dynparam_host_parameter, siblings);
  list_del(g_unused_parameters.next);
  g_unused_parameters_count--;

  return parameter_ptr;
}

void
lv2dynparam_put_unused_parameter(
  struct lv2dynparam_host_parameter * parameter_ptr)
{
  list_add_tail(&parameter_ptr->siblings, &g_unused_parameters);
  g_unused_parameters_count++;
}

struct lv2dynparam_host_message *
lv2dynparam_get_unused_message()
{
  struct lv2dynparam_host_message * message_ptr;

  if (list_empty(&g_unused_messages))
  {
    return NULL;
  }

  message_ptr = list_entry(g_unused_messages.next, struct lv2dynparam_host_message, siblings);
  list_del(g_unused_messages.next);
  g_unused_messages_count--;

  return message_ptr;
}

struct lv2dynparam_host_message *
lv2dynparam_get_unused_message_may_block()
{
  struct lv2dynparam_host_message * message_ptr;

  do
  {
    lv2dynparam_preallocate();
    message_ptr = lv2dynparam_get_unused_message();
  }
  while (message_ptr == NULL);

  return message_ptr;
}

void
lv2dynparam_put_unused_message(
  struct lv2dynparam_host_message * message_ptr)
{
  list_add_tail(&message_ptr->siblings, &g_unused_messages);
  g_unused_messages_count++;
}
