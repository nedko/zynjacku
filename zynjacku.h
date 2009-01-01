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

#ifndef ZYNJACKU_H__8BEF69EC_22B2_42AB_AE27_163F1ED2F7F0__INCLUDED
#define ZYNJACKU_H__8BEF69EC_22B2_42AB_AE27_163F1ED2F7F0__INCLUDED

#define LV2MIDI_BUFFER_SIZE 8192 /* bytes */

#define LV2_EVENT_URI_TYPE_MIDI "http://lv2plug.in/ns/ext/midi#MidiEvent"

#define ZYNJACKU_STRING_XFER_ID 2

#define PORT_TYPE_INVALID          0
#define PORT_TYPE_AUDIO            1 /* LV2 audio out port */
#define PORT_TYPE_MIDI             2 /* LV2 midi in port */
#define PORT_TYPE_EVENT_MIDI       3 /* LV2 midi in event port */
#define PORT_TYPE_LV2_FLOAT_PARAM  4 /* LV2 control rate input port used for synth/effect parameters */
#define PORT_TYPE_MEASURE          5 /* LV2 control rate output port used for plugin output (leds, meters, etc.) */
#define PORT_TYPE_DYNPARAM         6

#define PORT_FLAGS_MSGCONTEXT      1 /* uses LV2 message context */
#define PORT_FLAGS_IS_STRING       2 /* is a string port*/

struct zynjacku_port
{
  struct list_head plugin_siblings;
  struct list_head port_type_siblings;
  unsigned int type;            /* one of PORT_TYPE_XXX */
  unsigned int flags;
  uint32_t index;               /* LV2 port index within owning plugin */
  char * symbol;                /* valid only when type is PORT_TYPE_LV2_FLOAT_PARAM */
  char * name;                  /* valid only when type is PORT_TYPE_LV2_FLOAT_PARAM */
  union
  {
    struct
    {
      float value;
      float min;
      float max;
    } parameter;                /* for PORT_TYPE_LV2_FLOAT_PARAM */
    struct _LV2_String_Data * string;
    jack_port_t * audio;        /* for PORT_TYPE_AUDIO */
    struct
    {
      unsigned int type;
      lv2dynparam_host_parameter handle;
    } dynparam; /* for PORT_TYPE_DYNPARAM */
  } data;

  GObject * ui_context;

  struct zynjacku_plugin * plugin_ptr;
  GObject * midi_cc_map_obj_ptr;
};

#endif /* #ifndef ZYNJACKU_H__8BEF69EC_22B2_42AB_AE27_163F1ED2F7F0__INCLUDED */
