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

#ifndef MIDI_CC_MAP_H__CD0E5008_4B6A_4003_BF06_3F6FC6475FDF__INCLUDED
#define MIDI_CC_MAP_H__CD0E5008_4B6A_4003_BF06_3F6FC6475FDF__INCLUDED

G_BEGIN_DECLS

#define ZYNJACKU_MIDI_CC_MAP_TYPE (zynjacku_midiccmap_get_type())
#define ZYNJACKU_MIDI_CC_MAP(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), ZYNJACKU_TYPE_MIDI_CC_MAP, ZynjackuMidiCcMap))
#define ZYNJACKU_MIDI_CC_MAP_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), ZYNJACKU_TYPE_MIDI_CC_MAP, ZynjackuMidiCcMapClass))
#define ZYNJACKU_IS_MIDI_CC_MAP(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), ZYNJACKU_TYPE_MIDI_CC_MAP))
#define ZYNJACKU_IS_MIDI_CC_MAP_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), ZYNJACKU_TYPE_MIDI_CC_MAP))
#define ZYNJACKU_MIDI_CC_MAP_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), ZYNJACKU_TYPE_MIDI_CC_MAP, ZynjackuMidiCc_MapClass))

#define ZYNJACKU_TYPE_MIDI_CC_MAP ZYNJACKU_MIDI_CC_MAP_TYPE

typedef struct _ZynjackuMidiCcMap ZynjackuMidiCcMap;
typedef struct _ZynjackuMidiCcMapClass ZynjackuMidiCcMapClass;

struct _ZynjackuMidiCcMap {
  GObject parent;
  /* instance members */
};

struct _ZynjackuMidiCcMapClass {
  GObjectClass parent;
  /* class members */
};

/* used by ZYNJACKU_TYPE_MIDI_CC_MAP */
GType zynjacku_midiccmap_get_type();

void
zynjacku_midiccmap_get_points(
  ZynjackuMidiCcMap * map_obj_ptr);

void
zynjacku_midiccmap_point_create(
  ZynjackuMidiCcMap * map_obj_ptr,
  guint cc_value,
  gfloat parameter_value);

void
zynjacku_midiccmap_point_remove(
  ZynjackuMidiCcMap * map_obj_ptr,
  guint cc_value);

void
zynjacku_midiccmap_point_cc_value_change(
  ZynjackuMidiCcMap * map_obj_ptr,
  guint cc_value_old,
  guint cc_value_new);

void
zynjacku_midiccmap_point_parameter_value_change(
  ZynjackuMidiCcMap * map_obj_ptr,
  guint cc_value,
  gfloat parameter_value);

void
zynjacku_midiccmap_cc_no_assign(
  ZynjackuMidiCcMap * map_obj_ptr,
  guint cc_no);

G_END_DECLS

#endif /* #ifndef MIDI_CC_MAP_H__CD0E5008_4B6A_4003_BF06_3F6FC6475FDF__INCLUDED */
