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

#ifndef DYNPARAM_H__5090F477_0BE7_439F_BF1D_F2EB78822760__INCLUDED
#define DYNPARAM_H__5090F477_0BE7_439F_BF1D_F2EB78822760__INCLUDED

#if defined(GLIB_CHECK_VERSION)
#define BOOL gboolean
#elif !defined(BOOL)
#define BOOL int
#define TRUE 1
#define FALSE 0
#endif

#define LV2DYNPARAM_URI "http://nedko.arnaudov.name/soft/zyn/lv2dynparam.h"

typedef void * lv2dynparam_host_instance;
typedef void * lv2dynparam_host_parameter;
typedef void * lv2dynparam_host_group;
typedef void * lv2dynparam_host_command;

/* called from ui thread */
BOOL
lv2dynparam_host_add_synth(
  const LV2_Descriptor * lv2descriptor,
  LV2_Handle lv2instance,
  void * instance_ui_context,
  lv2dynparam_host_instance * instance_ptr);

/* called from audio/midi realtime thread */
void
lv2dynparam_host_realtime_run(
  lv2dynparam_host_instance instance);

/* called from ui thread */
void
lv2dynparam_host_ui_run(
  lv2dynparam_host_instance instance);

/* called from audio/midi realtime thread to change parameter as response to midi cc
 * If controller is not associated with any parameter, cc will be ignored */
void
lv2dynparam_host_cc(
  unsigned int controler,
  unsigned int value);

/* called from ui thread to associate midi cc with parameter */
void
lv2dynparam_host_cc_configure(
  lv2dynparam_host_instance instance,
  lv2dynparam_host_parameter parameter,
  unsigned int controler);

/* called from ui thread to deassociate parameter from midi cc */
void
lv2dynparam_host_cc_unconfigure(
  lv2dynparam_host_instance instance,
  lv2dynparam_host_parameter parameter);

/* called from ui thread */
void
lv2dynparam_host_ui_on(
  lv2dynparam_host_instance instance);

/* called from ui thread */
void
lv2dynparam_host_ui_off(
  lv2dynparam_host_instance instance);

/* callback called from ui thread */
void
dynparam_group_appeared(
  lv2dynparam_host_group group_handle,
  void * instance_ui_context,
  void * parent_group_ui_context,
  const char * group_name,
  const char * group_type_uri,
  void ** group_ui_context);

/* callback called from ui thread */
void
dynparam_group_disappeared(
  void * instance_ui_context,
  void * parent_group_ui_context,
  void * group_ui_context);

/* callback called from ui thread */
void
dynparam_command_appeared(
  lv2dynparam_host_command command_handle,
  void * instance_ui_context,
  void * group_ui_context,
  const char * command_name,
  void ** command_ui_context);

/* callback called from ui thread */
void
dynparam_parameter_boolean_appeared(
  lv2dynparam_host_parameter parameter_handle,
  void * instance_ui_context,
  void * group_ui_context,
  const char * parameter_name,
  BOOL value,
  void ** parameter_ui_context);

/* callback called from ui thread */
void
dynparam_parameter_boolean_disappeared(
  void * instance_ui_context,
  void * parent_group_ui_context,
  void * parameter_ui_context);

/* callback called from ui thread */
void
dynparam_parameter_float_appeared(
  lv2dynparam_host_parameter parameter_handle,
  void * instance_ui_context,
  void * group_ui_context,
  const char * parameter_name,
  float value,
  float min,
  float max,
  void ** parameter_ui_context);

/* callback called from ui thread */
void
dynparam_parameter_float_disappeared(
  void * instance_ui_context,
  void * parent_group_ui_context,
  void * parameter_ui_context);

/* callback called from ui thread */
void
dynparam_parameter_boolean_changed(
  void * instance_ui_context,
  void * parameter_ui_context,
  BOOL value);

/* called from ui thread, to change boolean parameter value */
void
dynparam_parameter_boolean_change(
  lv2dynparam_host_instance instance,
  lv2dynparam_host_parameter parameter_handle,
  BOOL value);

/* called from ui thread, to change boolean parameter value */
void
dynparam_parameter_float_change(
  lv2dynparam_host_instance instance,
  lv2dynparam_host_parameter parameter_handle,
  float value);

#endif /* #ifndef DYNPARAM_H__5090F477_0BE7_439F_BF1D_F2EB78822760__INCLUDED */
