/* -*- Mode: C ; c-basic-offset: 2 -*- */
/*****************************************************************************
 *
 *   Declarations of lv2dynparam host callbacks, the ones called by plugin
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

#ifndef DYNPARAM_HOST_CALLBACKS_H__17AEA0D2_0207_478E_AF6F_9719018DEFB6__INCLUDED
#define DYNPARAM_HOST_CALLBACKS_H__17AEA0D2_0207_478E_AF6F_9719018DEFB6__INCLUDED

unsigned char
lv2dynparam_host_group_appear(
  void * instance_host_context,
  void * parent_group_host_context,
  lv2dynparam_group_handle group,
  void ** host_context);

unsigned char
lv2dynparam_host_group_disappear(
  void * instance_host_context,
  void * group_host_context);

unsigned char
lv2dynparam_host_parameter_appear(
  void * instance_host_context,
  void * group_host_context,
  lv2dynparam_parameter_handle parameter,
  void ** parameter_host_context);

unsigned char
lv2dynparam_host_parameter_disappear(
  void * instance_host_context,
  void * parameter_host_context);

unsigned char
lv2dynparam_host_parameter_change(
  void * instance_host_context,
  void * parameter_host_context);

unsigned char
lv2dynparam_host_command_appear(
  void * instance_host_context,
  void * group_host_context,
  lv2dynparam_command_handle command,
  void ** command_context);

unsigned char
lv2dynparam_host_command_disappear(
  void * instance_host_context,
  void * command_host_context);

#endif /* #ifndef DYNPARAM_HOST_CALLBACKS_H__17AEA0D2_0207_478E_AF6F_9719018DEFB6__INCLUDED */
