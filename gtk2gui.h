/* -*- Mode: C ; c-basic-offset: 2 -*- */
/*****************************************************************************
 *
 *   This file is part of zynjacku
 *
 *   Copyright (C) 2007,2008 Nedko Arnaudov <nedko@arnaudov.name>
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

#ifndef GTK2GUI_H__6BE36C30_2948_428C_94F3_5443FBFF32F6__INCLUDED
#define GTK2GUI_H__6BE36C30_2948_428C_94F3_5443FBFF32F6__INCLUDED

typedef void * zynjacku_gtk2gui_handle;
#define ZYNJACKU_GTK2GUI_HANDLE_INVALID_VALUE NULL

zynjacku_gtk2gui_handle
zynjacku_gtk2gui_create(
  const LV2_Feature * const * host_features,
  unsigned int host_feature_count,
  zynjacku_lv2_handle plugin_handle,
  void * context_ptr,
  const char * uri,
  const char * synth_id,
  const struct list_head * parameter_ports_ptr);

void
zynjacku_gtk2gui_destroy(
  zynjacku_gtk2gui_handle gtk2gui_handle);

bool
zynjacku_gtk2gui_ui_on(
  zynjacku_gtk2gui_handle gtk2gui_handle);

void
zynjacku_gtk2gui_ui_off(
  zynjacku_gtk2gui_handle gtk2gui_handle);

void
zynjacku_gtk2gui_push_measure_ports(
  zynjacku_gtk2gui_handle ui_handle,
  const struct list_head * measure_ports_ptr);

/* callback */
void
zynjacku_gtk2gui_on_ui_destroyed(
  void * context_ptr);

#endif /* #ifndef GTK2GUI_H__6BE36C30_2948_428C_94F3_5443FBFF32F6__INCLUDED */
