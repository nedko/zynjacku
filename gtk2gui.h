/* -*- Mode: C ; c-basic-offset: 2 -*- */
/*****************************************************************************
 *
 *   This file is part of zynjacku
 *
 *   Copyright (C) 2007 Nedko Arnaudov <nedko@arnaudov.name>
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
zynjacku_gtk2gui_init(
  void * context_ptr,
  SLV2Plugin plugin,
  const char * synth_id,
  const struct list_head * parameter_ports_ptr);

void
zynjacku_gtk2gui_uninit(
  zynjacku_gtk2gui_handle gtk2gui_handle);

unsigned int
zynjacku_gtk2gui_get_count(
  zynjacku_gtk2gui_handle gtk2gui_handle);

const char *
zynjacku_gtk2gui_get_name(
  zynjacku_gtk2gui_handle gtk2gui_handle,
  unsigned int index);

void
zynjacku_gtk2gui_ui_on(
  zynjacku_gtk2gui_handle gtk2gui_handle,
  unsigned int index);

void
zynjacku_gtk2gui_ui_off(
  zynjacku_gtk2gui_handle gtk2gui_handle,
  unsigned int index);

/* callback */
void
zynjacku_gtk2gui_on_ui_destroyed(
  void * context_ptr);

#endif /* #ifndef GTK2GUI_H__6BE36C30_2948_428C_94F3_5443FBFF32F6__INCLUDED */
