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

#include <stdlib.h>
#include <glib-object.h>

#include "midi_cc_map.h"

#include "list.h"
#include "midi_cc_map_internal.h"

//#define LOG_LEVEL LOG_LEVEL_DEBUG
#include "log.h"

#define ZYNJACKU_MIDI_CC_MAP_SIGNAL_POINT_CREATED         0
#define ZYNJACKU_MIDI_CC_MAP_SIGNAL_POINT_REMOVED         1
#define ZYNJACKU_MIDI_CC_MAP_SIGNAL_POINT_CC_CHANGED      2
#define ZYNJACKU_MIDI_CC_MAP_SIGNAL_POINT_VALUE_CHANGED   3
#define ZYNJACKU_MIDI_CC_MAP_SIGNAL_CC_NO_ASSIGNED        4
#define ZYNJACKU_MIDI_CC_MAP_SIGNAL_CC_VALUE_CHANGED      5
#define ZYNJACKU_MIDI_CC_MAP_SIGNALS_COUNT                6

struct point
{
  struct list_head siblings;

  guint cc_value;
  gfloat parameter_value;
};

struct zynjacku_midi_cc_map
{
  gboolean dispose_has_run;

  gint cc_no;
  struct list_head points;
};

#define ZYNJACKU_MIDI_CC_MAP_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), ZYNJACKU_MIDI_CC_MAP_TYPE, struct zynjacku_midi_cc_map))

static guint g_zynjacku_midi_cc_map_signals[ZYNJACKU_MIDI_CC_MAP_SIGNALS_COUNT];

static void
zynjacku_midi_cc_map_dispose(GObject * obj)
{
  struct zynjacku_midi_cc_map * map_ptr;

  map_ptr = ZYNJACKU_MIDI_CC_MAP_GET_PRIVATE(obj);

  LOG_DEBUG("zynjacku_midi_cc_map_dispose() called.");

  if (map_ptr->dispose_has_run)
  {
    /* If dispose did already run, return. */
    LOG_DEBUG("zynjacku_midi_cc_map_dispose() already run!");
    return;
  }

  /* Make sure dispose does not run twice. */
  map_ptr->dispose_has_run = TRUE;

  /* 
   * In dispose, you are supposed to free all types referenced from this
   * object which might themselves hold a reference to self. Generally,
   * the most simple solution is to unref all members on which you own a 
   * reference.
   */

  /* Chain up to the parent class */
  G_OBJECT_CLASS(g_type_class_peek_parent(G_OBJECT_GET_CLASS(obj)))->dispose(obj);
}

static void
zynjacku_midi_cc_map_finalize(GObject * obj)
{
//  struct zynjacku_midi_cc_map * self = ZYNJACKU_MIDI_CC_MAP_GET_PRIVATE(obj);

  LOG_DEBUG("zynjacku_midi_cc_map_finalize() called.");

  /*
   * Here, complete object destruction.
   * You might not need to do much...
   */
  //g_free(self->private);

  /* Chain up to the parent class */
  G_OBJECT_CLASS(g_type_class_peek_parent(G_OBJECT_GET_CLASS(obj)))->finalize(obj);
}

static
void
zynjacku_midi_cc_map_class_init(
  gpointer class_ptr,
  gpointer class_data_ptr)
{
  LOG_DEBUG("zynjacku_midi_cc_map_class() called.");

  G_OBJECT_CLASS(class_ptr)->dispose = zynjacku_midi_cc_map_dispose;
  G_OBJECT_CLASS(class_ptr)->finalize = zynjacku_midi_cc_map_finalize;

  g_type_class_add_private(G_OBJECT_CLASS(class_ptr), sizeof(struct zynjacku_midi_cc_map));

  g_zynjacku_midi_cc_map_signals[ZYNJACKU_MIDI_CC_MAP_SIGNAL_POINT_CREATED] =
    g_signal_new(
      "point-created",          /* signal_name */
      ZYNJACKU_MIDI_CC_MAP_TYPE, /* itype */
      G_SIGNAL_RUN_LAST |
      G_SIGNAL_ACTION,          /* signal_flags */
      0,                        /* class_offset */
      NULL,                     /* accumulator */
      NULL,                     /* accu_data */
      NULL,                     /* c_marshaller */
      G_TYPE_NONE,              /* return type */
      2,                        /* n_params */
      G_TYPE_UINT,              /* MIDI CC value 0..127 */
      G_TYPE_FLOAT);            /* uri of plugin being scanned */

  g_zynjacku_midi_cc_map_signals[ZYNJACKU_MIDI_CC_MAP_SIGNAL_POINT_REMOVED] =
    g_signal_new(
      "point-removed",          /* signal_name */
      ZYNJACKU_MIDI_CC_MAP_TYPE, /* itype */
      G_SIGNAL_RUN_LAST |
      G_SIGNAL_ACTION,          /* signal_flags */
      0,                        /* class_offset */
      NULL,                     /* accumulator */
      NULL,                     /* accu_data */
      NULL,                     /* c_marshaller */
      G_TYPE_NONE,              /* return type */
      1,                        /* n_params */
      G_TYPE_UINT);             /* MIDI CC value 0..127 */

  g_zynjacku_midi_cc_map_signals[ZYNJACKU_MIDI_CC_MAP_SIGNAL_POINT_CC_CHANGED] =
    g_signal_new(
      "point-cc-changed",       /* signal_name */
      ZYNJACKU_MIDI_CC_MAP_TYPE, /* itype */
      G_SIGNAL_RUN_LAST |
      G_SIGNAL_ACTION,          /* signal_flags */
      0,                        /* class_offset */
      NULL,                     /* accumulator */
      NULL,                     /* accu_data */
      NULL,                     /* c_marshaller */
      G_TYPE_NONE,              /* return type */
      2,                        /* n_params */
      G_TYPE_UINT,              /* old MIDI CC value 0..127 */
      G_TYPE_UINT);             /* new MIDI CC value 0..127 */

  g_zynjacku_midi_cc_map_signals[ZYNJACKU_MIDI_CC_MAP_SIGNAL_POINT_VALUE_CHANGED] =
    g_signal_new(
      "point-value-changed",    /* signal_name */
      ZYNJACKU_MIDI_CC_MAP_TYPE, /* itype */
      G_SIGNAL_RUN_LAST |
      G_SIGNAL_ACTION,          /* signal_flags */
      0,                        /* class_offset */
      NULL,                     /* accumulator */
      NULL,                     /* accu_data */
      NULL,                     /* c_marshaller */
      G_TYPE_NONE,              /* return type */
      2,                        /* n_params */
      G_TYPE_UINT,              /* MIDI CC value 0..127 */
      G_TYPE_FLOAT);            /* uri of plugin being scanned */

  g_zynjacku_midi_cc_map_signals[ZYNJACKU_MIDI_CC_MAP_SIGNAL_CC_NO_ASSIGNED] =
    g_signal_new(
      "cc-no-assigned",         /* signal_name */
      ZYNJACKU_MIDI_CC_MAP_TYPE, /* itype */
      G_SIGNAL_RUN_LAST |
      G_SIGNAL_ACTION,          /* signal_flags */
      0,                        /* class_offset */
      NULL,                     /* accumulator */
      NULL,                     /* accu_data */
      NULL,                     /* c_marshaller */
      G_TYPE_NONE,              /* return type */
      1,                        /* n_params */
      G_TYPE_UINT);             /* MIDI CC No, 0..127 */

  g_zynjacku_midi_cc_map_signals[ZYNJACKU_MIDI_CC_MAP_SIGNAL_CC_VALUE_CHANGED] =
    g_signal_new(
      "cc-value-changed",       /* signal_name */
      ZYNJACKU_MIDI_CC_MAP_TYPE, /* itype */
      G_SIGNAL_RUN_LAST |
      G_SIGNAL_ACTION,          /* signal_flags */
      0,                        /* class_offset */
      NULL,                     /* accumulator */
      NULL,                     /* accu_data */
      NULL,                     /* c_marshaller */
      G_TYPE_NONE,              /* return type */
      1,                        /* n_params */
      G_TYPE_UINT);             /* MIDI CC value 0..127 */
}

static
void
zynjacku_midi_cc_map_init(
  GTypeInstance * instance,
  gpointer g_class)
{
  struct zynjacku_midi_cc_map * map_ptr;

  LOG_DEBUG("zynjacku_midi_cc_map_init() called.");

  map_ptr = ZYNJACKU_MIDI_CC_MAP_GET_PRIVATE(instance);

  INIT_LIST_HEAD(&map_ptr->points);
  map_ptr->cc_no = G_MAXUINT;
}

GType zynjacku_midiccmap_get_type()
{
  static GType type = 0;
  if (type == 0)
  {
    type = g_type_register_static_simple(
      G_TYPE_OBJECT,
      "zynjacku_midi_cc_map_type",
      sizeof(ZynjackuMidiCcMapClass),
      zynjacku_midi_cc_map_class_init,
      sizeof(ZynjackuMidiCcMap),
      zynjacku_midi_cc_map_init,
      0);
  }

  return type;
}

void
zynjacku_midiccmap_point_created(
  ZynjackuMidiCcMap * map_obj_ptr,
  guint cc_value,
  gfloat parameter_value)
{
  struct zynjacku_midi_cc_map * map_ptr;

  LOG_DEBUG("zynjacku_midiccmap_point_created() called.");

  map_ptr = ZYNJACKU_MIDI_CC_MAP_GET_PRIVATE(map_obj_ptr);

  g_signal_emit(
    map_obj_ptr,
    g_zynjacku_midi_cc_map_signals[ZYNJACKU_MIDI_CC_MAP_SIGNAL_POINT_CREATED],
    0,
    cc_value,
    parameter_value);
}

void
zynjacku_midiccmap_point_removed(
  ZynjackuMidiCcMap * map_obj_ptr,
  guint cc_value)
{
  LOG_DEBUG("zynjacku_midiccmap_point_removed() called.");

  g_signal_emit(
    map_obj_ptr,
    g_zynjacku_midi_cc_map_signals[ZYNJACKU_MIDI_CC_MAP_SIGNAL_POINT_REMOVED],
    0,
    cc_value);
}

void
zynjacku_midiccmap_point_cc_changed(
  ZynjackuMidiCcMap * map_obj_ptr,
  guint cc_value_old,
  guint cc_value_new)
{
  LOG_DEBUG("zynjacku_midiccmap_point_cc_value_changed() called.");

  g_signal_emit(
    map_obj_ptr,
    g_zynjacku_midi_cc_map_signals[ZYNJACKU_MIDI_CC_MAP_SIGNAL_POINT_CC_CHANGED],
    0,
    cc_value_old,
    cc_value_new);
}

void
zynjacku_midiccmap_point_value_changed(
  ZynjackuMidiCcMap * map_obj_ptr,
  guint cc_value,
  gfloat parameter_value)
{
  struct zynjacku_midi_cc_map * map_ptr;

  LOG_DEBUG("zynjacku_midiccmap_point_value_changed() called.");

  map_ptr = ZYNJACKU_MIDI_CC_MAP_GET_PRIVATE(map_obj_ptr);

  g_signal_emit(
    map_obj_ptr,
    g_zynjacku_midi_cc_map_signals[ZYNJACKU_MIDI_CC_MAP_SIGNAL_POINT_VALUE_CHANGED],
    0,
    cc_value,
    parameter_value);
}

void
zynjacku_midiccmap_get_points(
  ZynjackuMidiCcMap * map_obj_ptr)
{
  struct zynjacku_midi_cc_map * map_ptr;
  struct list_head * node_ptr;
  struct point * point_ptr;

  LOG_DEBUG("zynjacku_midiccmap_get_points() called.");

  map_ptr = ZYNJACKU_MIDI_CC_MAP_GET_PRIVATE(map_obj_ptr);

  list_for_each(node_ptr, &map_ptr->points)
  {
    point_ptr = list_entry(node_ptr, struct point, siblings);

    zynjacku_midiccmap_point_created(map_obj_ptr, point_ptr->cc_value, point_ptr->parameter_value);
  }
}

void
zynjacku_midiccmap_point_create(
  ZynjackuMidiCcMap * map_obj_ptr,
  guint cc_value,
  gfloat parameter_value)
{
  struct zynjacku_midi_cc_map * map_ptr;
  struct point * point_ptr;

  LOG_DEBUG("zynjacku_midiccmap_point_create() called.");

  map_ptr = ZYNJACKU_MIDI_CC_MAP_GET_PRIVATE(map_obj_ptr);

  point_ptr = malloc(sizeof(struct point));

  point_ptr->cc_value = cc_value;
  point_ptr->parameter_value = parameter_value;

  list_add_tail(&point_ptr->siblings, &map_ptr->points);

  zynjacku_midiccmap_point_created(map_obj_ptr, cc_value, parameter_value);
}

static
struct point *
zynjacku_midiccmap_point_find(
  struct zynjacku_midi_cc_map * map_ptr,
  guint cc_value)
{
  struct list_head * node_ptr;
  struct point * point_ptr;

  list_for_each(node_ptr, &map_ptr->points)
  {
    point_ptr = list_entry(node_ptr, struct point, siblings);

    if (point_ptr->cc_value == cc_value)
    {
      return point_ptr;
    }
  }

  return NULL;
}

void
zynjacku_midiccmap_point_remove(
  ZynjackuMidiCcMap * map_obj_ptr,
  guint cc_value)
{
  struct zynjacku_midi_cc_map * map_ptr;
  struct point * point_ptr;

  LOG_DEBUG("zynjacku_midiccmap_point_remove() called.");

  map_ptr = ZYNJACKU_MIDI_CC_MAP_GET_PRIVATE(map_obj_ptr);

  point_ptr = zynjacku_midiccmap_point_find(map_ptr, cc_value);
  if (point_ptr == NULL)
  {
    LOG_ERROR("cannot find point with cc value of %u", cc_value);
    return;
  }

  zynjacku_midiccmap_point_removed(map_obj_ptr, cc_value);
}

void
zynjacku_midiccmap_point_cc_value_change(
  ZynjackuMidiCcMap * map_obj_ptr,
  guint cc_value_old,
  guint cc_value_new)
{
  struct zynjacku_midi_cc_map * map_ptr;
  struct point * point_ptr;

  LOG_DEBUG("zynjacku_midiccmap_point_cc_value_change() called.");

  map_ptr = ZYNJACKU_MIDI_CC_MAP_GET_PRIVATE(map_obj_ptr);

  point_ptr = zynjacku_midiccmap_point_find(map_ptr, cc_value_old);
  if (point_ptr == NULL)
  {
    LOG_ERROR("cannot find point with cc value of %u", cc_value_old);
    return;
  }

  point_ptr->cc_value = cc_value_new;

  zynjacku_midiccmap_point_cc_changed(map_obj_ptr, cc_value_old, cc_value_new);
}

void
zynjacku_midiccmap_point_parameter_value_change(
  ZynjackuMidiCcMap * map_obj_ptr,
  guint cc_value,
  gfloat parameter_value)
{
  struct zynjacku_midi_cc_map * map_ptr;
  struct point * point_ptr;

  LOG_DEBUG("zynjacku_midiccmap_point_parameter_value_change() called.");

  map_ptr = ZYNJACKU_MIDI_CC_MAP_GET_PRIVATE(map_obj_ptr);

  point_ptr = zynjacku_midiccmap_point_find(map_ptr, cc_value);
  if (point_ptr == NULL)
  {
    LOG_ERROR("cannot find point with cc value of %u", cc_value);
    return;
  }

  point_ptr->parameter_value = parameter_value;

  zynjacku_midiccmap_point_value_changed(map_obj_ptr, cc_value, parameter_value);
}

void
zynjacku_midiccmap_point_midi_cc(
  ZynjackuMidiCcMap * map_obj_ptr,
  guint cc_no,
  guint cc_value)
{
  struct zynjacku_midi_cc_map * map_ptr;

  LOG_DEBUG("zynjacku_midiccmap_point_midi_cc(%u, %u) called.", cc_no, cc_value);

  map_ptr = ZYNJACKU_MIDI_CC_MAP_GET_PRIVATE(map_obj_ptr);

  if (map_ptr->cc_no == G_MAXUINT)
  {
    map_ptr->cc_no = cc_no;

    g_signal_emit(
      map_obj_ptr,
      g_zynjacku_midi_cc_map_signals[ZYNJACKU_MIDI_CC_MAP_SIGNAL_CC_NO_ASSIGNED],
      0,
      cc_no);
  }

  if (map_ptr->cc_no == cc_no)
  {
    g_signal_emit(
      map_obj_ptr,
      g_zynjacku_midi_cc_map_signals[ZYNJACKU_MIDI_CC_MAP_SIGNAL_CC_VALUE_CHANGED],
      0,
      cc_value);
  }
}

void
zynjacku_midiccmap_cc_no_assign(
  ZynjackuMidiCcMap * map_obj_ptr,
  guint cc_no)
{
  struct zynjacku_midi_cc_map * map_ptr;

  LOG_DEBUG("zynjacku_midiccmap_cc_no_assign(%u) called.", cc_no);

  map_ptr = ZYNJACKU_MIDI_CC_MAP_GET_PRIVATE(map_obj_ptr);

  if (map_ptr->cc_no != cc_no)
  {
    map_ptr->cc_no = cc_no;

    g_signal_emit(
      map_obj_ptr,
      g_zynjacku_midi_cc_map_signals[ZYNJACKU_MIDI_CC_MAP_SIGNAL_CC_NO_ASSIGNED],
      0,
      cc_no);
  }
}

gint
zynjacku_midiccmap_get_cc_no(
  ZynjackuMidiCcMap * map_obj_ptr)
{
  struct zynjacku_midi_cc_map * map_ptr;

  map_ptr = ZYNJACKU_MIDI_CC_MAP_GET_PRIVATE(map_obj_ptr);

  return map_ptr->cc_no;
}
