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

#ifndef ENUM_H__82946166_6B3C_47CD_897A_ABF4F85D9136__INCLUDED
#define ENUM_H__82946166_6B3C_47CD_897A_ABF4F85D9136__INCLUDED

G_BEGIN_DECLS

#define ZYNJACKU_ENUM_TYPE (zynjacku_enum_get_type())
#define ZYNJACKU_ENUM(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), ZYNJACKU_TYPE_ENUM, ZynjackuEnum))
#define ZYNJACKU_ENUM_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), ZYNJACKU_TYPE_ENUM, ZynjackuEnumClass))
#define ZYNJACKU_IS_ENUM(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), ZYNJACKU_TYPE_ENUM))
#define ZYNJACKU_IS_ENUM_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), ZYNJACKU_TYPE_ENUM))
#define ZYNJACKU_ENUM_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), ZYNJACKU_TYPE_ENUM, ZynjackuEnumClass))

#define ZYNJACKU_TYPE_ENUM ZYNJACKU_ENUM_TYPE

typedef struct _ZynjackuEnum ZynjackuEnum;
typedef struct _ZynjackuEnumClass ZynjackuEnumClass;

struct _ZynjackuEnum {
  GObject parent;
  /* instance members */
};

struct _ZynjackuEnumClass {
  GObjectClass parent;
  /* class members */
};

/* used by ZYNJACKU_TYPE_ENUM */
GType zynjacku_enum_get_type();

guint
zynjacku_enum_get_count(
  ZynjackuEnum * enum_obj_ptr);

const gchar *
zynjacku_enum_get_at_index(
  ZynjackuEnum * enum_obj_ptr,
  guint index);

void
zynjacku_enum_set(
  ZynjackuEnum * enum_obj_ptr,
  const gchar * const * values,
  guint values_count);

G_END_DECLS

#endif /* #ifndef ENUM_H__82946166_6B3C_47CD_897A_ABF4F85D9136__INCLUDED */
