/* -*- Mode: C ; c-basic-offset: 2 -*- */
/*****************************************************************************
 *
 *   This file is part of zynjacku
 *
 *   Copyright (C) 2008,2009 Nedko Arnaudov <nedko@arnaudov.name>
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
#include <string.h>
#include <assert.h>
#include <glib-object.h>

#include "midi_cc_map.h"

#include "list.h"
#include "midi_cc_map_internal.h"
#include "plugin_internal.h"

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

struct point_and_func
{
  guint next_cc_value;

  /* slope and y-intercept of linear function that matches this point and next one */
  /* these are not valid if next_cc_value is G_MAXUINT */
  gfloat slope;
  gfloat y_intercept;
};

struct zynjacku_midi_cc_map
{
  gboolean dispose_has_run;

  gint cc_no;
  gint cc_value;

  gboolean pending_assign;
  gboolean pending_value_change;

  GObject * plugin_obj_ptr;

  struct list_head points;

  gboolean points_need_ui_update;
  gboolean points_need_rt_update;
  struct point_and_func points_rt[MIDICC_VALUE_COUNT];
  struct point_and_func points_ui[MIDICC_VALUE_COUNT];
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
  map_ptr->pending_assign = FALSE;
  map_ptr->pending_value_change = FALSE;
  map_ptr->points_rt[0].next_cc_value = G_MAXUINT;
  map_ptr->points_need_ui_update = FALSE;
  map_ptr->points_need_rt_update = FALSE;
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

  LOG_DEBUG("zynjacku_midiccmap_point_create(%u, %f) called.", cc_value, parameter_value);

  map_ptr = ZYNJACKU_MIDI_CC_MAP_GET_PRIVATE(map_obj_ptr);

  point_ptr = malloc(sizeof(struct point));

  point_ptr->cc_value = cc_value;
  point_ptr->parameter_value = parameter_value;

  list_add_tail(&point_ptr->siblings, &map_ptr->points);

  map_ptr->points_need_ui_update = TRUE;

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

  map_ptr->points_need_ui_update = TRUE;

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

  map_ptr->points_need_ui_update = TRUE;

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

  map_ptr->points_need_ui_update = TRUE;

  zynjacku_midiccmap_point_value_changed(map_obj_ptr, cc_value, parameter_value);
}

void
zynjacku_midiccmap_midi_cc_rt(
  ZynjackuMidiCcMap * map_obj_ptr,
  guint cc_no,
  guint cc_value)
{
  struct zynjacku_midi_cc_map * map_ptr;

  LOG_DEBUG("zynjacku_midiccmap_midi_cc_rt_cc(%u, %u) called.", cc_no, cc_value);

  map_ptr = ZYNJACKU_MIDI_CC_MAP_GET_PRIVATE(map_obj_ptr);

  assert(map_ptr != NULL);

  if (map_ptr->cc_no == G_MAXUINT)
  {
    map_ptr->pending_assign = TRUE;
  }

  map_ptr->cc_no = cc_no;
  map_ptr->cc_value = cc_value;
  map_ptr->pending_value_change = TRUE;

  if (map_ptr->points_need_rt_update)
  {
    LOG_DEBUG("updating points_rt array...");
    memcpy(map_ptr->points_rt, map_ptr->points_ui, sizeof(map_ptr->points_rt));
    map_ptr->points_need_rt_update = FALSE;
  }
}

void
zynjacku_midiccmap_ui_run(
  ZynjackuMidiCcMap * map_obj_ptr)
{
  struct zynjacku_midi_cc_map * map_ptr;
  struct list_head * node_ptr;
  struct point * point_ptr;
  struct point * points_map[MIDICC_VALUE_COUNT];
  int index;
  int prev;
  gfloat x1, x2, y1, y2;

  //LOG_DEBUG("zynjacku_midiccmap_ui_run() called.");

  map_ptr = ZYNJACKU_MIDI_CC_MAP_GET_PRIVATE(map_obj_ptr);

  if (map_ptr->pending_assign)
  {
    g_signal_emit(
      map_obj_ptr,
      g_zynjacku_midi_cc_map_signals[ZYNJACKU_MIDI_CC_MAP_SIGNAL_CC_NO_ASSIGNED],
      0,
      map_ptr->cc_no);

    map_ptr->pending_assign = FALSE;
  }

  if (map_ptr->pending_value_change)
  {
    g_signal_emit(
      map_obj_ptr,
      g_zynjacku_midi_cc_map_signals[ZYNJACKU_MIDI_CC_MAP_SIGNAL_CC_VALUE_CHANGED],
      0,
      map_ptr->cc_value);

    map_ptr->pending_value_change = FALSE;
  }

  if (!map_ptr->points_need_ui_update)
  {
    return;
  }

  /* regenerate points_ui array from points list */

  LOG_DEBUG("regenerating points_ui array of map %p", map_ptr);

  map_ptr->points_need_ui_update = FALSE;

  memset(points_map, 0, sizeof(points_map));

  list_for_each(node_ptr, &map_ptr->points)
  {
    point_ptr = list_entry(node_ptr, struct point, siblings);
    assert(point_ptr->cc_value < MIDICC_VALUE_COUNT);
    points_map[point_ptr->cc_value] = point_ptr;
  }

  if (points_map[0] == NULL || points_map[MIDICC_VALUE_COUNT - 1] == NULL)
  {
    /* if we dont have the extreme points then map is not complete
       and thus it cannot be used for mapping */
    LOG_DEBUG("not complete map");
    return;
  }

  prev = -1;
  for (index = 0; index < MIDICC_VALUE_COUNT; index++)
  {
    map_ptr->points_ui[index].next_cc_value = G_MAXUINT;

    if (points_map[index] == NULL)
    {
      continue;
    }

    if (prev == -1)
    {
      prev = index;
      continue;
    }

    map_ptr->points_ui[prev].next_cc_value = index;

    x1 = (gfloat)prev;
    x2 = (gfloat)index;
    y1 = points_map[prev]->parameter_value;
    y2 = points_map[index]->parameter_value;

    map_ptr->points_ui[prev].slope = (y2 - y1) / (x2 - x1);
    map_ptr->points_ui[prev].y_intercept = (y1 * x2 - x1 * y2) / (x2 - x1);

    LOG_DEBUG("%u -> %u has slope of %f and y-intercept of %f", prev, index, map_ptr->points_ui[prev].slope, map_ptr->points_ui[prev].y_intercept);

    prev = index;
  }

  /* schedule update of points_rt array on next zynjacku_midiccmap_midi_cc_rt() call */
  /* this function and zynjacku_midiccmap_midi_cc_rt() are called with same lock obtained */
  /* however zynjacku_midiccmap_map_cc_rt() is called without lock obtained */
  map_ptr->points_need_rt_update = TRUE;
}

gboolean
zynjacku_midiccmap_cc_no_assign(
  ZynjackuMidiCcMap * map_obj_ptr,
  guint cc_no)
{
  struct zynjacku_midi_cc_map * map_ptr;

  LOG_DEBUG("zynjacku_midiccmap_cc_no_assign(%u) called.", cc_no);

  map_ptr = ZYNJACKU_MIDI_CC_MAP_GET_PRIVATE(map_obj_ptr);

  if (map_ptr->plugin_obj_ptr == NULL)
  {
    //LOG_DEBUG("no plugin associated");

    if (map_ptr->cc_no != cc_no)
    {
      map_ptr->cc_no = cc_no;

      g_signal_emit(
        map_obj_ptr,
        g_zynjacku_midi_cc_map_signals[ZYNJACKU_MIDI_CC_MAP_SIGNAL_CC_NO_ASSIGNED],
        0,
        cc_no);
    }

    return TRUE;
  }
  else
  {
    //LOG_DEBUG("plugin associated");
  }

  return zynjacku_plugin_midi_cc_map_cc_no_assign(map_ptr->plugin_obj_ptr, G_OBJECT(map_obj_ptr), cc_no);
}

gint
zynjacku_midiccmap_get_cc_no(
  ZynjackuMidiCcMap * map_obj_ptr)
{
  struct zynjacku_midi_cc_map * map_ptr;

  map_ptr = ZYNJACKU_MIDI_CC_MAP_GET_PRIVATE(map_obj_ptr);

  return map_ptr->cc_no;
}

/* we meed this function because engine has to call zynjacku_midiccmap_map_cc_rt
   and ZYNJACKU_MIDI_CC_MAP_GET_PRIVATE is not good to be used in realtime thread */
void *
zynjacku_midiccmap_get_internal_ptr(
  ZynjackuMidiCcMap * map_obj_ptr)
{
  return ZYNJACKU_MIDI_CC_MAP_GET_PRIVATE(map_obj_ptr);
}

#define map_ptr ((struct zynjacku_midi_cc_map *)internal_ptr)

gfloat
zynjacku_midiccmap_map_cc_rt(
  void * internal_ptr,
  guint cc_value)
{
  int index;

  LOG_DEBUG("zynjacku_midiccmap_map_cc_rt(%p, %u)", map_ptr, cc_value);

  if (map_ptr->points_rt[0].next_cc_value == G_MAXUINT)
  {
    /* no valid map */
    LOG_DEBUG("no valid map");
    return 0.0;
  }

  assert(cc_value < MIDICC_VALUE_COUNT);
  index = cc_value;

  while (map_ptr->points_rt[index].next_cc_value == G_MAXUINT)
  {
    index--;
    assert(index >= 0);
  }

  LOG_DEBUG("%u -> %u has slope of %f and y-intercept of %f", index, map_ptr->points_rt[index].next_cc_value, map_ptr->points_rt[index].slope, map_ptr->points_rt[index].y_intercept);

  return map_ptr->points_rt[index].slope * (gfloat)cc_value + map_ptr->points_rt[index].y_intercept;
}

#undef map_ptr
