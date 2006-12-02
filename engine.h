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

#ifndef ENGINE_H__512BF192_2626_4759_839B_47B7780A971B__INCLUDED
#define ENGINE_H__512BF192_2626_4759_839B_47B7780A971B__INCLUDED

G_BEGIN_DECLS

#define ZYNJACKU_ENGINE_TYPE (zynjacku_engine_get_type())
#define ZYNJACKU_ENGINE(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), ZYNJACKU_ENGINE_TYPE, ZynjackuEngine))
#define ZYNJACKU_ENGINE_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), ZYNJACKU_ENGINE_TYPE, ZynjackuEngineClass))
#define ZYNJACKU_IS_ENGINE(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), ZYNJACKU_ENGINE_TYPE))
#define ZYNJACKU_IS_ENGINE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), ZYNJACKU_ENGINE_TYPE))
#define ZYNJACKU_ENGINE_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), ZYNJACKU_ENGINE_TYPE, ZynjackuEngineClass))

#define ZYNJACKU_TYPE_ENGINE ZYNJACKU_ENGINE_TYPE

typedef struct _ZynjackuEngine ZynjackuEngine;
typedef struct _ZynjackuEngineClass ZynjackuEngineClass;

struct _ZynjackuEngine {
  GObject parent;
  /* instance members */
};

struct _ZynjackuEngineClass {
  GObjectClass parent;
  /* class members */
};

/* used by ZYNJACKU_ENGINE_TYPE */
GType zynjacku_engine_get_type();

gboolean
zynjacku_engine_start_jack(
  ZynjackuEngine * obj_ptr,
  const char * client_name);

void
zynjacku_engine_stop_jack(
  ZynjackuEngine * obj_ptr);

void
zynjacku_engine_activate_synth(
  ZynjackuEngine * engine_obj_ptr,
  GObject * synth_obj_ptr);

void
zynjacku_engine_deactivate_synth(
  ZynjackuEngine * engine_obj_ptr,
  GObject * synth_obj_ptr);

guint
zynjacku_engine_get_sample_rate(
  ZynjackuEngine * engine_obj_ptr);

G_END_DECLS

/*

void
zynjacku_ui_run();

*/

#endif /* #ifndef ENGINE_H__512BF192_2626_4759_839B_47B7780A971B__INCLUDED */
