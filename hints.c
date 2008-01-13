/* -*- Mode: C ; c-basic-offset: 2 -*- */
/*****************************************************************************
 *
 *   This file is part of zynjacku
 *
 *   Copyright (C) 2006,2007,2008 Nedko Arnaudov <nedko@arnaudov.name>
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
#include <lv2.h>
#include <lv2dynparam/lv2dynparam.h>

#include "hints.h"

//#define LOG_LEVEL LOG_LEVEL_DEBUG
#include "log.h"

struct zynjacku_hints
{
  gboolean dispose_has_run;

  guint count;
  GArray * names;
  GArray * values;
};

#define ZYNJACKU_HINTS_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), ZYNJACKU_HINTS_TYPE, struct zynjacku_hints))

static void
zynjacku_hints_dispose(GObject * obj)
{
  struct zynjacku_hints * hints_ptr;

  hints_ptr = ZYNJACKU_HINTS_GET_PRIVATE(obj);

  LOG_DEBUG("zynjacku_hints_dispose() called.");

  if (hints_ptr->dispose_has_run)
  {
    /* If dispose did already run, return. */
    LOG_DEBUG("zynjacku_hints_dispose() already run!");
    return;
  }

  /* Make sure dispose does not run twice. */
  hints_ptr->dispose_has_run = TRUE;

  /* 
   * In dispose, you are supposed to free all types referenced from this
   * object which might themselves hold a reference to self. Generally,
   * the most simple solution is to unref all members on which you own a 
   * reference.
   */
  g_array_free(hints_ptr->names, TRUE);
  g_array_free(hints_ptr->values, TRUE);

  /* Chain up to the parent class */
  G_OBJECT_CLASS(g_type_class_peek_parent(G_OBJECT_GET_CLASS(obj)))->dispose(obj);
}

static void
zynjacku_hints_finalize(GObject * obj)
{
//  struct zynjacku_hints * self = ZYNJACKU_HINTS_GET_PRIVATE(obj);

  LOG_DEBUG("zynjacku_hints_finalize() called.");

  /*
   * Here, complete object destruction.
   * You might not need to do much...
   */
  //g_free(self->private);

  /* Chain up to the parent class */
  G_OBJECT_CLASS(g_type_class_peek_parent(G_OBJECT_GET_CLASS(obj)))->finalize(obj);
}

static void
zynjacku_hints_class_init(
  gpointer class_ptr,
  gpointer class_data_ptr)
{
  LOG_DEBUG("zynjacku_hints_class() called.");

  G_OBJECT_CLASS(class_ptr)->dispose = zynjacku_hints_dispose;
  G_OBJECT_CLASS(class_ptr)->finalize = zynjacku_hints_finalize;

  g_type_class_add_private(G_OBJECT_CLASS(class_ptr), sizeof(struct zynjacku_hints));
}

static void
zynjacku_hints_init(
  GTypeInstance * instance,
  gpointer g_class)
{
  struct zynjacku_hints * hints_ptr;

  LOG_DEBUG("zynjacku_hints_init() called.");

  hints_ptr = ZYNJACKU_HINTS_GET_PRIVATE(instance);

  hints_ptr->names = g_array_new(TRUE, TRUE, sizeof(gchar *));
  hints_ptr->values = g_array_new(TRUE, TRUE, sizeof(gchar *));
}

GType zynjacku_hints_get_type()
{
  static GType type = 0;
  if (type == 0)
  {
    type = g_type_register_static_simple(
      G_TYPE_OBJECT,
      "zynjacku_hints_type",
      sizeof(ZynjackuHintsClass),
      zynjacku_hints_class_init,
      sizeof(ZynjackuHints),
      zynjacku_hints_init,
      0);
  }

  return type;
}

guint
zynjacku_hints_get_count(
  ZynjackuHints * hints_obj_ptr)
{
  struct zynjacku_hints * hints_ptr;

  hints_ptr = ZYNJACKU_HINTS_GET_PRIVATE(hints_obj_ptr);

  return hints_ptr->count;
}

const gchar *
zynjacku_hints_get_name_at_index(
  ZynjackuHints * hints_obj_ptr,
  guint index)
{
  struct zynjacku_hints * hints_ptr;

  hints_ptr = ZYNJACKU_HINTS_GET_PRIVATE(hints_obj_ptr);

  return g_array_index(hints_ptr->names, gchar *, index);
}

const gchar *
zynjacku_hints_get_value_at_index(
  ZynjackuHints * hints_obj_ptr,
  guint index)
{
  struct zynjacku_hints * hints_ptr;

  hints_ptr = ZYNJACKU_HINTS_GET_PRIVATE(hints_obj_ptr);

  return g_array_index(hints_ptr->values, gchar *, index);
}

void
zynjacku_hints_set(
  ZynjackuHints * hints_obj_ptr,
  guint count,
  const gchar * const * names,
  const gchar * const * values)
{
  guint i;
  struct zynjacku_hints * hints_ptr;
  gchar * name;
  gchar * value;

  hints_ptr = ZYNJACKU_HINTS_GET_PRIVATE(hints_obj_ptr);

  for (i = 0 ; i < count ; i++)
  {
    name = g_strdup(names[i]);
    g_array_append_val(hints_ptr->names, name);

    if (values[i] == NULL)
    {
      value = NULL;
    }
    else
    {
      value = g_strdup(values[i]);
    }

    g_array_append_val(hints_ptr->values, value);
  }

  hints_ptr->count = count;
}
