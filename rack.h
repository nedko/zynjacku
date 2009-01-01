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

#ifndef RACK_H__512BF192_2626_4759_839B_47B7780A971B__INCLUDED
#define RACK_H__512BF192_2626_4759_839B_47B7780A971B__INCLUDED

G_BEGIN_DECLS

#define ZYNJACKU_RACK_TYPE (zynjacku_rack_get_type())
#define ZYNJACKU_RACK(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), ZYNJACKU_RACK_TYPE, ZynjackuRack))
#define ZYNJACKU_RACK_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), ZYNJACKU_RACK_TYPE, ZynjackuRackClass))
#define ZYNJACKU_IS_RACK(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), ZYNJACKU_RACK_TYPE))
#define ZYNJACKU_IS_RACK_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), ZYNJACKU_RACK_TYPE))
#define ZYNJACKU_RACK_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), ZYNJACKU_RACK_TYPE, ZynjackuRackClass))

#define ZYNJACKU_TYPE_RACK ZYNJACKU_RACK_TYPE

typedef struct _ZynjackuRack ZynjackuRack;
typedef struct _ZynjackuRackClass ZynjackuRackClass;

struct _ZynjackuRack {
  GObject parent;
  /* instance members */
};

struct _ZynjackuRackClass {
  GObjectClass parent;
  /* class members */
};

/* used by ZYNJACKU_RACK_TYPE */
GType zynjacku_rack_get_type();

gboolean
zynjacku_rack_start_jack(
  ZynjackuRack * obj_ptr,
  const char * client_name);

void
zynjacku_rack_stop_jack(
  ZynjackuRack * obj_ptr);

guint
zynjacku_rack_get_sample_rate(
  ZynjackuRack * rack_obj_ptr);

void
zynjacku_rack_ui_run(
  ZynjackuRack * rack_obj_ptr);

const gchar *
zynjacku_rack_get_version();

void
zynjacku_rack_iterate_plugins(
  ZynjackuRack * rack_obj_ptr,
  gboolean force);

G_END_DECLS

#endif /* #ifndef RACK_H__512BF192_2626_4759_839B_47B7780A971B__INCLUDED */
