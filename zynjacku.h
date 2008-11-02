/* -*- Mode: C ; c-basic-offset: 2 -*- */
/*****************************************************************************
 *
 *   This file is part of zynjacku
 *
 *   Copyright (C) 2006,2007,2008 Nedko Arnaudov <nedko@arnaudov.name>
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

#ifndef ZYNJACKU_H__8BEF69EC_22B2_42AB_AE27_163F1ED2F7F0__INCLUDED
#define ZYNJACKU_H__8BEF69EC_22B2_42AB_AE27_163F1ED2F7F0__INCLUDED

#define LV2MIDI_BUFFER_SIZE 8192 /* bytes */

#define LV2_EVENT_URI_TYPE_MIDI "http://lv2plug.in/ns/ext/midi#MidiEvent"

#define PORT_TYPE_INVALID          0
#define PORT_TYPE_AUDIO            1 /* LV2 audio out port */
#define PORT_TYPE_MIDI             2 /* LV2 midi in port */
#define PORT_TYPE_EVENT_MIDI       3 /* LV2 midi in event port */
#define PORT_TYPE_PARAMETER        4 /* LV2 control rate input port used for synth/effect parameters */
#define PORT_TYPE_MEASURE          5 /* LV2 control rate output port used for plugin output (leds, meters, etc.) */

struct zynjacku_port
{
  struct list_head plugin_siblings;
  struct list_head port_type_siblings;
  unsigned int type;            /* one of PORT_TYPE_XXX */
  uint32_t index;               /* LV2 port index within owning plugin */
  char * symbol;                /* valid only when type is PORT_TYPE_PARAMETER */
  char * name;                  /* valid only when type is PORT_TYPE_PARAMETER */
  union
  {
    struct
    {
      float value;
      float min;
      float max;
    } parameter;                /* for PORT_TYPE_PARAMETER */
    jack_port_t * audio;        /* for PORT_TYPE_AUDIO */
  } data;

  GObject * ui_context;
};

#define PLUGIN_TYPE_UNKNOWN  0
#define PLUGIN_TYPE_SYNTH    1
#define PLUGIN_TYPE_EFFECT   2

struct zynjacku_plugin
{
  gboolean dispose_has_run;

  GObject * root_group_ui_context;
  GObject * engine_object_ptr;
  gchar * uri;

  struct list_head siblings_all;
  struct list_head siblings_active;
  zynjacku_lv2_handle lv2plugin;
  bool dynparams_supported;
  struct list_head parameter_ports;
  struct list_head measure_ports;
  lv2dynparam_host_instance dynparams;
  zynjacku_gtk2gui_handle gtk2gui;
  char * id;
  char * name;

  bool recycle;

  unsigned int type;

  union
  {
    struct
    {
      struct zynjacku_port midi_in_port;
      struct zynjacku_port audio_out_left_port;
      struct zynjacku_port audio_out_right_port;
    } synth;
    struct
    {
      struct zynjacku_port audio_in_left_port;
      struct zynjacku_port audio_in_right_port;
      struct zynjacku_port audio_out_left_port;
      struct zynjacku_port audio_out_right_port;
    } effect;
  } subtype;

  void (* deactivate)(GObject * synth_obj_ptr);
  void (* free_ports)(GObject * synth_obj_ptr);
};

#define ZYNJACKU_PLUGIN_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), ZYNJACKU_PLUGIN_TYPE, struct zynjacku_plugin))

void
zynjacku_free_plugin_control_ports(
  struct zynjacku_plugin * plugin_ptr);

void
zynjacku_plugin_ui_run(
  struct zynjacku_plugin * plugin_ptr);

#endif /* #ifndef ZYNJACKU_H__8BEF69EC_22B2_42AB_AE27_163F1ED2F7F0__INCLUDED */
