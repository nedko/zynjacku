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

#define LV2DYNPARAM_MAX_STRING_SIZE 1024

struct lv2dynparam_host_group
{
  struct list_head siblings;
  struct lv2dynparam_host_instance * instance_ptr;
  struct lv2dynparam_host_group * parent_group_ptr;
  lv2dynparam_group_handle group_handle;
  char name[LV2DYNPARAM_MAX_STRING_SIZE];

  void * ui_context;
};

struct lv2dynparam_host_parameter
{
  struct list_head siblings;
  struct lv2dynparam_host_instance * instance_ptr;
  struct lv2dynparam_host_group * group_ptr;
  lv2dynparam_parameter_handle param_handle;
  char name[LV2DYNPARAM_MAX_STRING_SIZE];

  void * ui_context;
};

struct lv2dynparam_host_instance
{
  void * instance_ui_context;
  audiolock_handle lock;
  struct lv2dynparam_plugin_callbacks * callbacks_ptr;
  LV2_Handle lv2instance;
};

BOOL
lv2dynparam_host_group_appear(
  void * instance_host_context,
  void * parent_group_host_context,
  lv2dynparam_group_handle group,
  void ** host_context);

void
lv2dynparam_host_group_disappear(
  void * instance_host_context,
  void * group_host_context);

BOOL
lv2dynparam_host_parameter_appear(
  void * instance_host_context,
  void * group_host_context,
  lv2dynparam_parameter_handle parameter,
  void ** parameter_host_context);

void
lv2dynparam_host_parameter_disappear(
  void * instance_host_context,
  void * parameter_host_context);

void
lv2dynparam_host_parameter_change(
  void * instance_host_context,
  void * parameter_host_context);

BOOL
lv2dynparam_host_command_appear(
  void * instance_host_context,
  void * group_host_context,
  lv2dynparam_command_handle command,
  void ** command_context);

void
lv2dynparam_host_command_disappear(
  void * instance_host_context,
  void * command_host_context);

void
lv2dynparam_preallocate();

struct lv2dynparam_host_group *
lv2dynparam_get_unused_group();

void
lv2dynparam_put_unused_group(
  struct lv2dynparam_host_group * group_ptr);

struct lv2dynparam_host_parameter *
lv2dynparam_get_unused_parameter();

void
lv2dynparam_put_unused_parameter(
  struct lv2dynparam_host_parameter * parameter_ptr);

#endif /* #ifndef DYNPARAM_INTERNAL_H__86778596_B1A9_4BD7_A14A_BECBD5589468__INCLUDED */
