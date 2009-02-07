/* -*- Mode: C ; c-basic-offset: 2 -*- */
/*****************************************************************************
 *
 *   This file is part of zynjacku
 *
 *   Copyright (C) 2006,2007,2008,2009 Nedko Arnaudov <nedko@arnaudov.name>
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
  gchar * dlpath;
  gchar * bundle_path;

  struct list_head siblings_all;
  struct list_head siblings_active;
  zynjacku_lv2_handle lv2plugin;
#if HAVE_DYNPARAMS
  bool dynparams_supported;
#endif
  struct list_head midi_ports;
  struct list_head audio_ports;
  struct list_head parameter_ports;
  struct list_head measure_ports;
#if HAVE_DYNPARAMS
  struct list_head dynparam_ports;
  lv2dynparam_host_instance dynparams;
#endif
  zynjacku_gtk2gui_handle gtk2gui;
  char * id;
  char * name;

  bool recycle;

  union
  {
    struct
    {
      struct zynjacku_port * midi_in_port_ptr;
      struct zynjacku_port * audio_out_left_port_ptr;
      struct zynjacku_port * audio_out_right_port_ptr;
    } synth;
    struct
    {
      struct zynjacku_port * audio_in_left_port_ptr;
      struct zynjacku_port * audio_in_right_port_ptr;
      struct zynjacku_port * audio_out_left_port_ptr;
      struct zynjacku_port * audio_out_right_port_ptr;
    } effect;
  } subtype;
  
  struct zynjacku_rt_plugin_command * volatile command; /* command to execute */
  struct zynjacku_rt_plugin_command * volatile command_result; /* command that has been executed */

  void (* deactivate)(GObject * synth_obj_ptr);
  void (* get_required_features)(GObject * engine_obj_ptr, const LV2_Feature * const ** host_features, unsigned int * host_feature_count);
  void (* unregister_port)(GObject * engine_obj_ptr, struct zynjacku_port * port_ptr);
  bool (* set_midi_cc_map)(GObject * engine_obj_ptr, struct zynjacku_port * port_ptr, GObject * midi_cc_map_obj_ptr);
  bool (* midi_cc_map_cc_no_assign)(GObject * engine_obj_ptr, GObject * midi_cc_map_obj_ptr, guint cc_no);
};

#define ZYNJACKU_PLUGIN_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), ZYNJACKU_PLUGIN_TYPE, struct zynjacku_plugin))

bool
zynjacku_connect_plugin_ports(
  struct zynjacku_plugin * plugin_ptr,
  ZynjackuPlugin * plugin_obj_ptr,
  GObject * engine_object_ptr
#if HAVE_DYNPARAMS
  ,struct lv2_rtsafe_memory_pool_provider * mempool_allocator_ptr
#endif
  );

void *
zynjacku_plugin_prerun_rt(
  struct zynjacku_plugin * plugin_ptr);

void
zynjacku_plugin_postrun_rt(
  struct zynjacku_plugin * plugin_ptr,
  void * old_data);

void
zynjacku_plugin_ui_run(
  struct zynjacku_plugin * plugin_ptr);

void
zynjacku_plugin_ui_set_port_value(
  struct zynjacku_plugin * plugin_ptr,
  struct zynjacku_port * port_ptr,
  const void * value_ptr,
  size_t value_size);

#endif /* LV2_H_INCLUDED defined */

gboolean
zynjacku_plugin_midi_cc_map_cc_no_assign(
  GObject * plugin_obj_ptr,
  GObject * midi_cc_map_obj_ptr,
  guint cc_no);

#endif /* #ifndef PLUGIN_INTERNAL_H__7D5A9DB4_4DBC_4BD1_BA4B_B2EE0BD931A1__INCLUDED */
