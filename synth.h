/* -*- Mode: C ; c-basic-offset: 2 -*- */
/*****************************************************************************
 *
 *   This file is part of zynjacku
 *
 *   Copyright (C) 2006 Nedko Arnaudov <nedko@arnaudov.name>
 *   Copyright (C) 2006 Dave Robillard <drobilla@connect.carleton.ca>
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

#ifndef SYNTH_H__0C38A6AD_527B_4795_8711_3606AC3A16BD__INCLUDED
#define SYNTH_H__0C38A6AD_527B_4795_8711_3606AC3A16BD__INCLUDED

G_BEGIN_DECLS

#define ZYNJACKU_TYPE_SYNTH (zynjacku_synth_get_type())
#define ZYNJACKU_SYNTH(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), ZYNJACKU_TYPE_SYNTH, ZynjackuSynth))
#define ZYNJACKU_SYNTH_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), ZYNJACKU_TYPE_SYNTH, ZynjackuSynthClass))
#define ZYNJACKU_IS_SYNTH(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), ZYNJACKU_TYPE_SYNTH))
#define ZYNJACKU_IS_SYNTH_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), ZYNJACKU_TYPE_SYNTH))
#define ZYNJACKU_SYNTH_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), ZYNJACKU_TYPE_SYNTH, ZynjackuSynthClass))

typedef struct _ZynjackuSynth ZynjackuSynth;
typedef struct _ZynjackuSynthClass ZynjackuSynthClass;

struct _ZynjackuSynth {
  GObject parent;
  /* instance members */
};

struct _ZynjackuSynthClass {
  GObjectClass parent;
  /* class members */
};

/* used by ZYNJACKU_TYPE_SYNTH */
GType zynjacku_synth_get_type();

gboolean
zynjacku_synth_construct(
  ZynjackuSynth * synth_obj_ptr,
  GObject * engine_obj_ptr);

void
zynjacku_synth_destruct(
  ZynjackuSynth * synth_obj_ptr);

const char *
zynjacku_synth_get_name(
  ZynjackuSynth * obj_ptr);

const char *
zynjacku_synth_get_class_name(
  ZynjackuSynth * obj_ptr);

const char *
zynjacku_synth_get_class_uri(
  ZynjackuSynth * obj_ptr);

void
zynjacku_synth_ui_on(
  ZynjackuSynth * obj_ptr);

void
zynjacku_synth_ui_off(
  ZynjackuSynth * obj_ptr);

G_END_DECLS

#endif /* #ifndef SYNTH_H__0C38A6AD_527B_4795_8711_3606AC3A16BD__INCLUDED */
