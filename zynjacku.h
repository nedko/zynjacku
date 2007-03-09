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

#ifndef ZYNJACKU_H__8BEF69EC_22B2_42AB_AE27_163F1ED2F7F0__INCLUDED
#define ZYNJACKU_H__8BEF69EC_22B2_42AB_AE27_163F1ED2F7F0__INCLUDED

#define LV2MIDI_BUFFER_SIZE 8192

#define PORT_TYPE_INVALID          0
#define PORT_TYPE_AUDIO            1 /* LV2 audio out port */
#define PORT_TYPE_MIDI             2 /* LV2 midi in port */
#define PORT_TYPE_PARAMETER        3 /* LV2 control rate port used for synth parameters */

struct zynjacku_synth_port
{
  struct list_head plugin_siblings;
  struct list_head port_type_siblings;
  unsigned int type;            /* one of PORT_TYPE_XXX */
  uint32_t index;               /* LV2 port index within owning plugin */
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

struct zynjacku_synth
{
  gboolean dispose_has_run;

  GObject * root_group_ui_context;
  GObject * engine_object_ptr;
  gchar * uri;

  struct list_head siblings;
  SLV2Plugin plugin;            /* plugin "class" (actually just a few strings) */
  SLV2Instance instance;        /* plugin "instance" (loaded shared lib) */
  struct zynjacku_synth_port midi_in_port;
  struct zynjacku_synth_port audio_out_left_port;
  struct zynjacku_synth_port audio_out_right_port;
  struct list_head parameter_ports;
  lv2dynparam_host_instance dynparams;
  zynjacku_gtk2gui_handle gtk2gui;
  char * id;
};

struct zynjacku_engine
{
  gboolean dispose_has_run;

  jack_client_t * jack_client;  /* the jack client */
  struct list_head plugins;
  struct list_head midi_ports;  /* PORT_TYPE_MIDI "struct zynjacku_synth_port"s linked by port_type_siblings */
  struct list_head audio_ports; /* PORT_TYPE_AUDIO "struct zynjacku_synth_port"s linked by port_type_siblings */
  jack_port_t * jack_midi_in;
  LV2_MIDI lv2_midi_buffer;
  gboolean midi_activity;
};

SLV2Plugin
zynjacku_plugin_repo_lookup_by_uri(const char * uri);

#define ZYNJACKU_ENGINE_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), ZYNJACKU_ENGINE_TYPE, struct zynjacku_engine))

#define ZYNJACKU_SYNTH_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), ZYNJACKU_SYNTH_TYPE, struct zynjacku_synth))

#endif /* #ifndef ZYNJACKU_H__8BEF69EC_22B2_42AB_AE27_163F1ED2F7F0__INCLUDED */
