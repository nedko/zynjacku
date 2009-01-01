/* -*- Mode: C ; c-basic-offset: 2 -*- */
/*****************************************************************************
 *
 *   This file is part of zynjacku
 *
 *   Copyright (C) 2008,2009 Nedko Arnaudov <nedko@arnaudov.name>
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

#ifndef MIDI_CC_MAP_INTERNAL_H__1BF0EA4F_D9DB_4A93_9A6D_9A77E5FFD9EA__INCLUDED
#define MIDI_CC_MAP_INTERNAL_H__1BF0EA4F_D9DB_4A93_9A6D_9A77E5FFD9EA__INCLUDED

G_BEGIN_DECLS

void
zynjacku_midiccmap_midi_cc_rt(
  ZynjackuMidiCcMap * map_obj_ptr,
  guint cc_no,
  guint cc_value);

void
zynjacku_midiccmap_ui_run(
  ZynjackuMidiCcMap * map_obj_ptr);

void *
zynjacku_midiccmap_get_internal_ptr(
  ZynjackuMidiCcMap * map_obj_ptr);

gfloat
zynjacku_midiccmap_map_cc_rt(
  void * internal_ptr,
  guint cc_value);

#define MIDICC_COUNT 128

G_END_DECLS

#endif /* #ifndef MIDI_CC_MAP_INTERNAL_H__1BF0EA4F_D9DB_4A93_9A6D_9A77E5FFD9EA__INCLUDED */
