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

#include <glib-object.h>

#include "enum.h"

#define LOG_LEVEL LOG_LEVEL_DEBUG
#include "log.h"

struct zynjacku_enum
{
  gboolean dispose_has_run;

  GArray * array;
};

#define ZYNJACKU_ENUM_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), ZYNJACKU_ENUM_TYPE, struct zynjacku_enum))

static void
zynjacku_enum_dispose(GObject * obj)
{
  struct zynjacku_enum * enum_ptr;

  enum_ptr = ZYNJACKU_ENUM_GET_PRIVATE(obj);

  LOG_DEBUG("zynjacku_enum_dispose() called.");

  if (enum_ptr->dispose_has_run)
  {
    /* If dispose did already run, return. */
    LOG_DEBUG("zynjacku_enum_dispose() already run!");
    return;
  }

  /* Make sure dispose does not run twice. */
  enum_ptr->dispose_has_run = TRUE;

  /* 
   * In dispose, you are supposed to free all types referenced from this
   * object which might themselves hold a reference to self. Generally,
   * the most simple solution is to unref all members on which you own a 
   * reference.
   */
  g_array_free(enum_ptr->array, TRUE);

  /* Chain up to the parent class */
  G_OBJECT_CLASS(g_type_class_peek_parent(G_OBJECT_GET_CLASS(obj)))->dispose(obj);
}

static void
zynjacku_enum_finalize(GObject * obj)
{
//  struct zynjacku_enum * self = ZYNJACKU_ENUM_GET_PRIVATE(obj);

  LOG_DEBUG("zynjacku_enum_finalize() called.");

  /*
   * Here, complete object destruction.
   * You might not need to do much...
   */
  //g_free(self->private);

  /* Chain up to the parent class */
  G_OBJECT_CLASS(g_type_class_peek_parent(G_OBJECT_GET_CLASS(obj)))->finalize(obj);
}

static void
zynjacku_enum_class_init(
  gpointer class_ptr,
  gpointer class_data_ptr)
{
  LOG_DEBUG("zynjacku_enum_class() called.");

  G_OBJECT_CLASS(class_ptr)->dispose = zynjacku_enum_dispose;
  G_OBJECT_CLASS(class_ptr)->finalize = zynjacku_enum_finalize;

  g_type_class_add_private(G_OBJECT_CLASS(class_ptr), sizeof(struct zynjacku_enum));
}

static void
zynjacku_enum_init(
  GTypeInstance * instance,
  gpointer g_class)
{
  struct zynjacku_enum * enum_ptr;

  LOG_DEBUG("zynjacku_enum_init() called.");

  enum_ptr = ZYNJACKU_ENUM_GET_PRIVATE(instance);

  enum_ptr->array = g_array_new(TRUE, TRUE, sizeof(gchar *));
}

GType zynjacku_enum_get_type()
{
  static GType type = 0;
  if (type == 0)
  {
    type = g_type_register_static_simple(
      G_TYPE_OBJECT,
      "zynjacku_enum_type",
      sizeof(ZynjackuEnumClass),
      zynjacku_enum_class_init,
      sizeof(ZynjackuEnum),
      zynjacku_enum_init,
      0);
  }

  return type;
}

guint
zynjacku_enum_get_count(
  ZynjackuEnum * enum_obj_ptr)
{
  struct zynjacku_enum * enum_ptr;

  enum_ptr = ZYNJACKU_ENUM_GET_PRIVATE(enum_obj_ptr);

  return enum_ptr->array->len;
}

const gchar *
zynjacku_enum_get_at_index(
  ZynjackuEnum * enum_obj_ptr,
  guint index)
{
  struct zynjacku_enum * enum_ptr;

  enum_ptr = ZYNJACKU_ENUM_GET_PRIVATE(enum_obj_ptr);

  return g_array_index(enum_ptr->array, gchar *, index);
}

void
zynjacku_enum_set(
  ZynjackuEnum * enum_obj_ptr,
  const gchar * const * values,
  guint values_count)
{
  struct zynjacku_enum * enum_ptr;
  unsigned int i;
  gchar * value;

  enum_ptr = ZYNJACKU_ENUM_GET_PRIVATE(enum_obj_ptr);

  for (i = 0 ; i < values_count ; i++)
  {
    value = g_strdup(values[i]);
    g_array_append_val(enum_ptr->array, value);
  }
}
