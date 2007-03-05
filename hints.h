/* -*- Mode: C ; c-basic-offset: 2 -*- */
/*****************************************************************************
 *
 *   This file is part of zynjacku
 *
 *   Copyright (C) 2006,2007 Nedko Arnaudov <nedko@arnaudov.name>
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

#ifndef HINTS_H__11E2EF6D_9CB6_4D61_BC8E_39B243E6B973__INCLUDED
#define HINTS_H__11E2EF6D_9CB6_4D61_BC8E_39B243E6B973__INCLUDED

G_BEGIN_DECLS

#define ZYNJACKU_HINTS_TYPE (zynjacku_hints_get_type())
#define ZYNJACKU_HINTS(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), ZYNJACKU_TYPE_HINTS, ZynjackuHints))
#define ZYNJACKU_HINTS_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), ZYNJACKU_TYPE_HINTS, ZynjackuHintsClass))
#define ZYNJACKU_IS_HINTS(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), ZYNJACKU_TYPE_HINTS))
#define ZYNJACKU_IS_HINTS_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), ZYNJACKU_TYPE_HINTS))
#define ZYNJACKU_HINTS_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), ZYNJACKU_TYPE_HINTS, ZynjackuHintsClass))

#define ZYNJACKU_TYPE_HINTS ZYNJACKU_HINTS_TYPE

typedef struct _ZynjackuHints ZynjackuHints;
typedef struct _ZynjackuHintsClass ZynjackuHintsClass;

struct _ZynjackuHints {
  GObject parent;
  /* instance members */
};

struct _ZynjackuHintsClass {
  GObjectClass parent;
  /* class members */
};

/* used by ZYNJACKU_TYPE_HINTS */
GType zynjacku_hints_get_type();

guint
zynjacku_hints_get_count(
  ZynjackuHints * hints_obj_ptr);

const gchar *
zynjacku_hints_get_name_at_index(
  ZynjackuHints * hints_obj_ptr,
  guint index);

const gchar *
zynjacku_hints_get_value_at_index(
  ZynjackuHints * hints_obj_ptr,
  guint index);

void
zynjacku_hints_set(
  ZynjackuHints * hints_obj_ptr,
  guint count,
  const gchar * const * names,
  const gchar * const * values);

G_END_DECLS

#endif /* #ifndef HINTS_H__11E2EF6D_9CB6_4D61_BC8E_39B243E6B973__INCLUDED */
