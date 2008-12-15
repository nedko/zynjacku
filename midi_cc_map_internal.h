/* -*- Mode: C ; c-basic-offset: 2 -*- */
/*****************************************************************************
 *
 *   This file is part of zynjacku
 *
 *   Copyright (C) 2008 Nedko Arnaudov <nedko@arnaudov.name>
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
zynjacku_midiccmap_point_midi_cc(
  ZynjackuMidiCcMap * map_obj_ptr,
  guint cc_no,
  guint cc_value);

G_END_DECLS

#endif /* #ifndef MIDI_CC_MAP_INTERNAL_H__1BF0EA4F_D9DB_4A93_9A6D_9A77E5FFD9EA__INCLUDED */