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

#ifndef PLUGIN_H__0C38A6AD_527B_4795_8711_3606AC3A16BD__INCLUDED
#define PLUGIN_H__0C38A6AD_527B_4795_8711_3606AC3A16BD__INCLUDED

G_BEGIN_DECLS

#define ZYNJACKU_PLUGIN_TYPE (zynjacku_plugin_get_type())
#define ZYNJACKU_PLUGIN(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), ZYNJACKU_TYPE_PLUGIN, ZynjackuPlugin))
#define ZYNJACKU_PLUGIN_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), ZYNJACKU_TYPE_PLUGIN, ZynjackuPluginClass))
#define ZYNJACKU_IS_PLUGIN(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), ZYNJACKU_TYPE_PLUGIN))
#define ZYNJACKU_IS_PLUGIN_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), ZYNJACKU_TYPE_PLUGIN))
#define ZYNJACKU_PLUGIN_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), ZYNJACKU_TYPE_PLUGIN, ZynjackuPluginClass))

#define ZYNJACKU_TYPE_PLUGIN ZYNJACKU_PLUGIN_TYPE

typedef struct _ZynjackuPlugin ZynjackuPlugin;
typedef struct _ZynjackuPluginClass ZynjackuPluginClass;

struct _ZynjackuPlugin {
  GObject parent;
  /* instance members */
};

struct _ZynjackuPluginClass {
  GObjectClass parent;
  /* class members */
};

/* used by ZYNJACKU_TYPE_PLUGIN */
GType zynjacku_plugin_get_type();

gboolean
zynjacku_plugin_construct(
  ZynjackuPlugin * plugin_obj_ptr,
  GObject * engine_obj_ptr);

void
zynjacku_plugin_destruct(
  ZynjackuPlugin * plugin_obj_ptr);

gboolean
zynjacku_plugin_supports_generic_ui(
  ZynjackuPlugin * plugin_obj_ptr);

gboolean
zynjacku_plugin_supports_custom_ui(
  ZynjackuPlugin * plugin_obj_ptr);

const char *
zynjacku_plugin_get_instance_name(
  ZynjackuPlugin * obj_ptr);

const char *
zynjacku_plugin_get_name(
  ZynjackuPlugin * obj_ptr);

const char *
zynjacku_plugin_get_uri(
  ZynjackuPlugin * obj_ptr);

gboolean
zynjacku_plugin_ui_on(
  ZynjackuPlugin * plugin_obj_ptr);

void
zynjacku_plugin_ui_off(
  ZynjackuPlugin * obj_ptr);

void
zynjacku_plugin_bool_set(
  ZynjackuPlugin * obj_ptr,
  gchar * context,
  gboolean value);

void
zynjacku_plugin_float_set(
  ZynjackuPlugin * obj_ptr,
  gchar * context,
  gfloat value);

void
zynjacku_plugin_int_set(
  ZynjackuPlugin * obj_ptr,
  gchar * context,
  gint value);

void
zynjacku_plugin_enum_set(
  ZynjackuPlugin * obj_ptr,
  gchar * context,
  guint value);

void
zynjacku_plugin_get_parameters(
  ZynjackuPlugin * obj_ptr);

gboolean
zynjacku_plugin_set_parameter(
  ZynjackuPlugin * obj_ptr,
  gchar * parameter,
  gchar * value,
  GObject * midi_cc_map_obj_ptr);

GObject *
zynjacku_plugin_get_midi_cc_map(
  ZynjackuPlugin * obj_ptr,
  gchar * parameter_context);

gboolean
zynjacku_plugin_set_midi_cc_map(
  ZynjackuPlugin * plugin_obj_ptr,
  gchar * string_context,
  GObject * midi_cc_map_obj_ptr);

G_END_DECLS

#endif /* #ifndef PLUGIN_H__0C38A6AD_527B_4795_8711_3606AC3A16BD__INCLUDED */
