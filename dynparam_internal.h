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

#ifndef DYNPARAM_INTERNAL_H__86778596_B1A9_4BD7_A14A_BECBD5589468__INCLUDED
#define DYNPARAM_INTERNAL_H__86778596_B1A9_4BD7_A14A_BECBD5589468__INCLUDED

#define LV2DYNPARAM_GROUP_TYPE_GENERIC   0
#define LV2DYNPARAM_GROUP_TYPE_ADSR      1

#define LV2DYNPARAM_PARAMETER_TYPE_COMMAND   0
#define LV2DYNPARAM_PARAMETER_TYPE_FLOAT     1
#define LV2DYNPARAM_PARAMETER_TYPE_INT       2
#define LV2DYNPARAM_PARAMETER_TYPE_NOTE      3
#define LV2DYNPARAM_PARAMETER_TYPE_STRING    4
#define LV2DYNPARAM_PARAMETER_TYPE_FILENAME  5
#define LV2DYNPARAM_PARAMETER_TYPE_BOOLEAN   6

#define LV2DYNPARAM_PENDING_NOTHING    0 /* nothing pending */
#define LV2DYNPARAM_PENDING_APPEAR     1 /* pending appear */
#define LV2DYNPARAM_PENDING_DISAPPEAR  2 /* pending disappear */

struct lv2dynparam_host_group
{
  struct list_head siblings;
  struct lv2dynparam_host_group * parent_group_ptr;
  lv2dynparam_group_handle group_handle;

  struct list_head child_groups;
  struct list_head child_params;
  struct list_head child_commands;

  char name[LV2DYNPARAM_MAX_STRING_SIZE];
  char type_uri[LV2DYNPARAM_MAX_STRING_SIZE];

  unsigned int pending_state;
  unsigned int pending_childern_count;

  void * ui_context;
};

struct lv2dynparam_host_parameter
{
  struct list_head siblings;
  struct lv2dynparam_host_group * group_ptr;
  lv2dynparam_parameter_handle param_handle;
  char name[LV2DYNPARAM_MAX_STRING_SIZE];
  char type_uri[LV2DYNPARAM_MAX_STRING_SIZE];
  unsigned int type;

  void * value_ptr;
  void * min_ptr;
  void * max_ptr;

  union
  {
    BOOL boolean;
    float fpoint;
  } value;

  union
  {
    float fpoint;
  } min;

  union
  {
    float fpoint;
  } max;

  unsigned int pending_state;

  void * ui_context;
};

struct lv2dynparam_host_command
{
  struct list_head siblings;
  struct lv2dynparam_host_group * group_ptr;
  lv2dynparam_command_handle command_handle;
  char name[LV2DYNPARAM_MAX_STRING_SIZE];

  BOOL gui_referenced;
  void * ui_context;
};

#define LV2DYNPARAM_HOST_MESSAGE_TYPE_PARAMETER_CHANGE          0
#define LV2DYNPARAM_HOST_MESSAGE_TYPE_COMMAND_EXECUTE           1

struct lv2dynparam_host_message
{
  struct list_head siblings;

  unsigned int message_type;

  union
  {
    struct lv2dynparam_host_group * group;
    struct lv2dynparam_host_parameter * parameter;
    struct lv2dynparam_host_command * command;
  } context;
};

struct lv2dynparam_host_instance
{
  void * instance_ui_context;
  audiolock_handle lock;
  struct lv2dynparam_plugin_callbacks * callbacks_ptr;
  LV2_Handle lv2instance;

  struct lv2dynparam_host_group * root_group_ptr;

  BOOL ui;

  struct list_head realtime_to_ui_queue; /* protected by the audiolock */
  struct list_head ui_to_realtime_queue; /* protected by the audiolock */
};

void
lv2dynparam_host_map_type_uri(
  struct lv2dynparam_host_parameter * parameter_ptr);

void
lv2dynparam_host_group_pending_children_count_increment(
  struct lv2dynparam_host_group * group_ptr);

void
lv2dynparam_host_group_pending_children_count_decrement(
  struct lv2dynparam_host_group * group_ptr);

#endif /* #ifndef DYNPARAM_INTERNAL_H__86778596_B1A9_4BD7_A14A_BECBD5589468__INCLUDED */
