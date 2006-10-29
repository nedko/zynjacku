/* -*- Mode: C ; c-basic-offset: 2 -*- */
/*****************************************************************************
 *
 *   This file is part of jack_mixer
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <slv2/slv2.h>
#include <jack/jack.h>
#include <jack/midiport.h>

#include "lv2-miditype.h"
#include "list.h"

#define LV2MIDI_BUFFER_SIZE 8192

#define PORT_TYPE_INVALID          0
#define PORT_TYPE_AUDIO            1 /* LV2 audio out port */
#define PORT_TYPE_MIDI             2 /* LV2 midi in port */
#define PORT_TYPE_PARAMETER        3 /* LV2 control rate port used for synth parameters */

jack_client_t * g_jack_client;  /* the jack client */
struct list_head g_plugins;
struct list_head g_midi_ports;  /* PORT_TYPE_MIDI "struct zynjacku_plugin_port"s linked by port_type_siblings */
struct list_head g_audio_ports; /* PORT_TYPE_AUDIO "struct zynjacku_plugin_port"s linked by port_type_siblings */
jack_port_t * g_jack_midi_in;
LV2_MIDI g_lv2_midi_buffer;

struct zynjacku_plugin_port
{
  struct list_head plugin_siblings;
  struct list_head port_type_siblings;
  unsigned int type;            /* one of PORT_TYPE_XXX */
  uint32_t index;               /* LV2 port index within owning plugin */
  union
  {
    float parameter;            /* for PORT_TYPE_PARAMETER */
    jack_port_t * audio;        /* for PORT_TYPE_AUDIO */
  } data;
};

struct zynjacku_plugin
{
  struct list_head siblings;
  SLV2Plugin * plugin;          /* plugin "class" (actually just a few strings) */
  SLV2Instance * instance;      /* plugin "instance" (loaded shared lib) */
  struct zynjacku_plugin_port midi_in_port;
  struct zynjacku_plugin_port audio_out_left_port;
  struct zynjacku_plugin_port audio_out_right_port;
  struct list_head control_ports;
};

void die(const char* msg);
void create_port(struct zynjacku_plugin * plugin_ptr, uint32_t port_index);
int jack_process_cb(jack_nframes_t nframes, void* data);
void list_plugins(SLV2List list);

int
main(int argc, char** argv)
{
  uint32_t i;
  SLV2List plugins;
  struct zynjacku_plugin plugin;
  const char * plugin_uri;
  char * name;
  uint32_t ports_count;
  struct list_head * node_ptr;
  struct zynjacku_plugin_port * port_ptr;

  INIT_LIST_HEAD(&g_plugins);
  INIT_LIST_HEAD(&g_midi_ports);
  INIT_LIST_HEAD(&g_audio_ports);

  INIT_LIST_HEAD(&plugin.control_ports);
  list_add_tail(&plugin.siblings, &g_plugins);
  plugin.midi_in_port.type = PORT_TYPE_INVALID;
  plugin.audio_out_left_port.type = PORT_TYPE_INVALID;
  plugin.audio_out_right_port.type = PORT_TYPE_INVALID;

  /* Find all installed plugins */
  plugins = slv2_list_new();
  slv2_list_load_all(plugins);
  //slv2_list_load_bundle(plugins, "http://www.scs.carleton.ca/~drobilla/files/Amp-swh.lv2");

  /* Find the plugin to run */
  plugin_uri = (argc == 2) ? argv[1] : NULL;

  if (!plugin_uri)
  {
    fprintf(stderr, "\nYou must specify a plugin URI to load.\n");
    fprintf(stderr, "\nKnown plugins:\n\n");
    list_plugins(plugins);
    return EXIT_FAILURE;
  }

  printf("URI:\t%s\n", plugin_uri);
  plugin.plugin = slv2_list_get_plugin_by_uri(plugins, plugin_uri);
  if (!plugin.plugin)
  {
    fprintf(stderr, "Failed to find plugin %s.\n", plugin_uri);
    slv2_list_free(plugins);
    return EXIT_FAILURE;
  }

  /* Get the plugin's name */
  name = slv2_plugin_get_name(plugin.plugin);
  printf("Name:\t%s\n", name);

  /* Connect to JACK (with plugin name as client name) */
  g_jack_client = jack_client_open(name, JackNullOption, NULL);
  free(name);
  if (!g_jack_client)
  {
    die("Failed to connect to JACK.");
  }
  else
  {
    printf("Connected to JACK.\n");
  }

  /* Instantiate the plugin */
  plugin.instance = slv2_plugin_instantiate(plugin.plugin, jack_get_sample_rate(g_jack_client), NULL);
  if (!plugin.instance)
  {
    die("Failed to instantiate plugin.\n");
  }
  else
  {
    printf("Succesfully instantiated plugin.\n");
  }

  jack_set_process_callback(g_jack_client, &jack_process_cb, &plugin);

  g_lv2_midi_buffer.capacity = LV2MIDI_BUFFER_SIZE;
  g_lv2_midi_buffer.data = malloc(LV2MIDI_BUFFER_SIZE);

  /* Create ports */
  ports_count  = slv2_plugin_get_num_ports(plugin.plugin);

  for (i = 0 ; i < ports_count ; i++)
  {
    create_port(&plugin, i);
  }

  /* register JACK MIDI input port */
  g_jack_midi_in = jack_port_register(g_jack_client, "midi in", JACK_DEFAULT_MIDI_TYPE, JackPortIsInput, 0);

  /* Activate plugin and JACK */
  slv2_instance_activate(plugin.instance);
  jack_activate(g_jack_client);

  /* Run */
  printf("Press enter to quit: ");
  getc(stdin);
  printf("\n");

  jack_deactivate(g_jack_client);

  /* Deactivate plugin and JACK */
  slv2_instance_free(plugin.instance);
  slv2_list_free(plugins);

  printf("Shutting down JACK.\n");

  jack_port_unregister(g_jack_client, g_jack_midi_in);

  list_for_each(node_ptr, &g_audio_ports)
  {
    port_ptr = list_entry(node_ptr, struct zynjacku_plugin_port, port_type_siblings);
    assert(port_ptr->type == PORT_TYPE_AUDIO);
    jack_port_unregister(g_jack_client, port_ptr->data.audio);
  }

  jack_client_close(g_jack_client);

  while (!list_empty(&plugin.control_ports))
  {
    node_ptr = plugin.control_ports.next;
    port_ptr = list_entry(node_ptr, struct zynjacku_plugin_port, plugin_siblings);
    list_del(node_ptr);
    free(port_ptr);
  }

  return 0;
}

/* Abort and exit on error */
void
die(const char* msg)
{
  fprintf(stderr, "%s\n", msg);
  exit(EXIT_FAILURE);
}

void
create_port(struct zynjacku_plugin * plugin_ptr, uint32_t port_index)
{
  enum SLV2PortClass class;
  char * type;
  char * symbol;
  struct zynjacku_plugin_port * port_ptr;

  /* Get the 'class' of the port (control input, audio output, etc) */
  class = slv2_port_get_class(plugin_ptr->plugin, port_index);

  type = slv2_port_get_data_type(plugin_ptr->plugin, port_index);

  /* Get the port symbol (label) for console printing */
  symbol = slv2_port_get_symbol(plugin_ptr->plugin, port_index);

  if (strcmp(type, SLV2_DATA_TYPE_FLOAT) == 0)
  {
    if (class == SLV2_CONTROL_RATE_INPUT)
    {
      port_ptr = (struct zynjacku_plugin_port *)malloc(sizeof(struct zynjacku_plugin_port));
      port_ptr->type = PORT_TYPE_PARAMETER;
      port_ptr->index = port_index;
      port_ptr->data.parameter = slv2_port_get_default_value(plugin_ptr->plugin, port_index);
      slv2_instance_connect_port(plugin_ptr->instance, port_index, &port_ptr->data.parameter);
      printf("Set %s to %f\n", symbol, port_ptr->data.parameter);
    }
    else if (class == SLV2_AUDIO_RATE_OUTPUT)
    {
      if (plugin_ptr->audio_out_left_port.type == PORT_TYPE_INVALID)
      {
        port_ptr = &plugin_ptr->audio_out_left_port;
      }
      else if (plugin_ptr->audio_out_right_port.type == PORT_TYPE_INVALID)
      {
        port_ptr = &plugin_ptr->audio_out_right_port;
      }
      else
      {
        /* Notify user that maximum two audio output ports are supported. Maybe just add message to log */
        return;
      }

      port_ptr->type = PORT_TYPE_AUDIO;
      port_ptr->index = port_index;
      port_ptr->data.audio = jack_port_register(g_jack_client, symbol, JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
      list_add_tail(&port_ptr->port_type_siblings, &g_audio_ports);
    }
    else if (class == SLV2_AUDIO_RATE_INPUT)
    {
      /* Notify user that audio input ports are not supported. Maybe just add message to log */
      return;
    }
    else if (class == SLV2_CONTROL_RATE_OUTPUT)
    {
      /* Notify user that control rate float output ports are not supported. Maybe just add message to log */
      return;
    }
    else
    {
      /* Notify user about unrecognized port. Maybe just add message to log */
      return;
    }
  }
  else if (strcmp(type, SLV2_DATA_TYPE_MIDI) == 0)
  {
    if (class == SLV2_CONTROL_RATE_INPUT)
    {
      if (plugin_ptr->midi_in_port.type == PORT_TYPE_INVALID)
      {
        port_ptr = &plugin_ptr->midi_in_port;
      }
      else
      {
        /* Notify user that maximum one midi input port is supported. Maybe just add message to log */
        return;
      }

      port_ptr->type = PORT_TYPE_MIDI;
      port_ptr->index = port_index;
      slv2_instance_connect_port(plugin_ptr->instance, port_index, &g_lv2_midi_buffer);
      list_add_tail(&port_ptr->port_type_siblings, &g_midi_ports);
    }
    else if (class == SLV2_CONTROL_RATE_OUTPUT)
    {
      /* Notify user that midi output ports are not supported. Maybe just add message to log */
      return;
    }
    else
    {
      /* Notify user about unrecognized port. Maybe just add message to log */
      return;
    }
  }
  else
  {
    die("Unrecognized data type, aborting.");
  }

  free(type);
  free(symbol);
}

/** Translate from a JACK MIDI buffer to an LV2 MIDI buffer. */
void jackmidi2lv2midi(jack_port_t * jack_port, LV2_MIDI * output_buf, jack_nframes_t nframes)
{
  static unsigned bank = 0;

  void * input_buf;
  jack_midi_event_t input_event;
  jack_nframes_t input_event_index;
  jack_nframes_t input_event_count;
  jack_nframes_t timestamp;
  jack_nframes_t i;
  unsigned char * data;

  input_event_index = 0;
  output_buf->event_count = 0;
  input_buf = jack_port_get_buffer(jack_port, nframes);
  input_event_count = jack_midi_port_get_info(input_buf, nframes)->event_count;

  /* iterate over all incoming JACK MIDI events */
  data = output_buf->data;
  for (i = 0; i < input_event_count; i++)
  {
    /* retrieve JACK MIDI event */
    jack_midi_event_get(&input_event, input_buf, i, nframes);
    if ((data - output_buf->data) + sizeof(double) + sizeof(size_t) + input_event.size >= output_buf->capacity)
    {
      break;
    }

    /* write LV2 MIDI event */
    *((double*)data) = input_event.time;
    data += sizeof(double);
    *((size_t*)data) = input_event.size;
    data += sizeof(size_t);
    memcpy(data, input_event.buffer, input_event.size);

    /* normalise note events if needed */
    if ((input_event.size == 3) && ((data[0] & 0xF0) == 0x90) &&
        (data[2] == 0))
    {
      data[0] = 0x80 | (data[0] & 0x0F);
    }

    data += input_event.size;
    output_buf->event_count++;
  }

  output_buf->size = data - output_buf->data;
}

/** Jack process callback. */
int
jack_process_cb(jack_nframes_t nframes, void* data)
{
  struct list_head * plugin_node_ptr;
  struct zynjacku_plugin * plugin_ptr;

  /* Copy MIDI input data to all LV2 midi in ports */
  jackmidi2lv2midi(g_jack_midi_in, &g_lv2_midi_buffer, nframes);

  /* Iterate over plugins */
  list_for_each(plugin_node_ptr, &g_plugins)
  {
    plugin_ptr = list_entry(plugin_node_ptr, struct zynjacku_plugin, siblings);

    /* Connect plugin LV2 output audio ports directly to JACK buffers */
    if (plugin_ptr->audio_out_left_port.type == PORT_TYPE_AUDIO)
    {
      slv2_instance_connect_port(
        plugin_ptr->instance,
        plugin_ptr->audio_out_left_port.index,
        jack_port_get_buffer(plugin_ptr->audio_out_left_port.data.audio, nframes));
    }

    /* Connect plugin LV2 output audio ports directly to JACK buffers */
    if (plugin_ptr->audio_out_right_port.type == PORT_TYPE_AUDIO)
    {
      slv2_instance_connect_port(
        plugin_ptr->instance,
        plugin_ptr->audio_out_right_port.index,
        jack_port_get_buffer(plugin_ptr->audio_out_right_port.data.audio, nframes));
    }

    /* Run plugin for this cycle */
    slv2_instance_run(plugin_ptr->instance, nframes);
  }

  return 0;
}


void
list_plugins(SLV2List list)
{
  size_t i;
  for (i=0; i < slv2_list_get_length(list); ++i)
  {
    const SLV2Plugin* const p = slv2_list_get_plugin_by_index(list, i);
    printf("%s\n", slv2_plugin_get_uri(p));
  }
}
