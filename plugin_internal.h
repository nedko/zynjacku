/* -*- Mode: C ; c-basic-offset: 2 -*- */
/*****************************************************************************
 *
 *   This file is part of zynjacku
 *
 *   Copyright (C) 2006,2007,2008 Nedko Arnaudov <nedko@arnaudov.name>
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

#ifndef PLUGIN_INTERNAL_H__7D5A9DB4_4DBC_4BD1_BA4B_B2EE0BD931A1__INCLUDED
#define PLUGIN_INTERNAL_H__7D5A9DB4_4DBC_4BD1_BA4B_B2EE0BD931A1__INCLUDED

#ifdef LV2_H_INCLUDED

#define PLUGIN_TYPE_UNKNOWN  0
#define PLUGIN_TYPE_SYNTH    1
#define PLUGIN_TYPE_EFFECT   2

struct zynjacku_rt_plugin_command
{
  struct zynjacku_port * port; /* port to set data for */
  void *data;     /* new data */
};

struct zynjacku_plugin
{
  gboolean dispose_has_run;

  GObject * root_group_ui_context;
  GObject * engine_object_ptr;
  gchar * uri;

  struct list_head siblings_all;
  struct list_head siblings_active;
  zynjacku_lv2_handle lv2plugin;
  bool dynparams_supported;
  struct list_head parameter_ports;
  struct list_head measure_ports;
  struct list_head dynparam_ports;
  lv2dynparam_host_instance dynparams;
  zynjacku_gtk2gui_handle gtk2gui;
  char * id;
  char * name;

  bool recycle;

  unsigned int type;

  union
  {
    struct
    {
      struct zynjacku_port midi_in_port;
      struct zynjacku_port audio_out_left_port;
      struct zynjacku_port audio_out_right_port;
    } synth;
    struct
    {
      struct zynjacku_port audio_in_left_port;
      struct zynjacku_port audio_in_right_port;
      struct zynjacku_port audio_out_left_port;
      struct zynjacku_port audio_out_right_port;
    } effect;
  } subtype;
  
  struct zynjacku_rt_plugin_command * volatile command; /* command to execute */
  struct zynjacku_rt_plugin_command * volatile command_result; /* command that has been executed */

  void (* deactivate)(GObject * synth_obj_ptr);
  void (* free_ports)(GObject * synth_obj_ptr);
  bool (* set_midi_cc_map)(GObject * engine_obj_ptr, struct zynjacku_port * port_ptr, GObject * midi_cc_map_obj_ptr);
  bool (* midi_cc_map_cc_no_assign)(GObject * engine_obj_ptr, GObject * midi_cc_map_obj_ptr, guint cc_no);
};

#define ZYNJACKU_PLUGIN_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), ZYNJACKU_PLUGIN_TYPE, struct zynjacku_plugin))

void
zynjacku_free_plugin_ports(
  struct zynjacku_plugin * plugin_ptr);

void
zynjacku_plugin_ui_run(
  struct zynjacku_plugin * plugin_ptr);

void
zynjacku_plugin_dynparam_parameter_created(
  void * instance_context,
  lv2dynparam_host_parameter parameter_handle,
  void ** parameter_context_ptr);

void
zynjacku_plugin_dynparam_parameter_destroying(
  void * instance_context,
  void * parameter_context);

void
zynjacku_plugin_dynparam_parameter_value_change_context(
  void * instance_context,
  void * parameter_context,
  void * value_change_context);

#endif /* LV2_H_INCLUDED defined */

gboolean
zynjacku_plugin_midi_cc_map_cc_no_assign(
  GObject * plugin_obj_ptr,
  GObject * midi_cc_map_obj_ptr,
  guint cc_no);

#endif /* #ifndef PLUGIN_INTERNAL_H__7D5A9DB4_4DBC_4BD1_BA4B_B2EE0BD931A1__INCLUDED */
