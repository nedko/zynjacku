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

#include <stdio.h>
#include <string.h>
#include <slv2/slv2.h>
#include <jack/jack.h>
#include <jack/midiport.h>

#include "lv2-miditype.h"
#include "list.h"
#include "slv2.h"
#include "log.h"

#define BOOL int
#define TRUE 1
#define FALSE 0

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
  struct list_head parameter_ports;
};

BOOL create_port(struct zynjacku_plugin * plugin_ptr, uint32_t port_index);
int jack_process_cb(jack_nframes_t nframes, void* data);

void
zynjacku_plugin_free_ports(struct zynjacku_plugin * plugin_ptr)
{
  struct list_head * node_ptr;
  struct zynjacku_plugin_port * port_ptr;

  while (!list_empty(&plugin_ptr->parameter_ports))
  {
    node_ptr = plugin_ptr->parameter_ports.next;
    port_ptr = list_entry(node_ptr, struct zynjacku_plugin_port, plugin_siblings);

    assert(port_ptr->type == PORT_TYPE_PARAMETER);

    list_del(node_ptr);
    free(port_ptr);
  }

  if (plugin_ptr->audio_out_left_port.type == PORT_TYPE_AUDIO)
  {
    jack_port_unregister(g_jack_client, plugin_ptr->audio_out_left_port.data.audio);
    list_del(&plugin_ptr->audio_out_left_port.port_type_siblings);
  }

  if (plugin_ptr->audio_out_right_port.type == PORT_TYPE_AUDIO) /* stereo? */
  {
    assert(plugin_ptr->audio_out_right_port.type == PORT_TYPE_AUDIO);
    jack_port_unregister(g_jack_client, plugin_ptr->audio_out_right_port.data.audio);
    list_del(&plugin_ptr->audio_out_right_port.port_type_siblings);
  }

  if (plugin_ptr->midi_in_port.type == PORT_TYPE_MIDI)
  {
    list_del(&plugin_ptr->midi_in_port.port_type_siblings);
  }
}

struct zynjacku_plugin *
zynjacku_plugin_construct(const void * uri)
{
  struct zynjacku_plugin * plugin_ptr;
  uint32_t ports_count;
  uint32_t i;
  static unsigned int id;
  char * name;
  char * port_name;
  size_t size_name;
  size_t size_id;

  plugin_ptr = (struct zynjacku_plugin *)malloc(sizeof(struct zynjacku_plugin));
  if (plugin_ptr == NULL)
  {
    LOG_ERROR("Cannot allocate memory for plugin");
    goto fail;
  }

  INIT_LIST_HEAD(&plugin_ptr->parameter_ports);
  plugin_ptr->midi_in_port.type = PORT_TYPE_INVALID;
  plugin_ptr->audio_out_left_port.type = PORT_TYPE_INVALID;
  plugin_ptr->audio_out_right_port.type = PORT_TYPE_INVALID;

  plugin_ptr->plugin = zynjacku_plugin_lookup_by_uri(uri);
  if (plugin_ptr->plugin == NULL)
  {
    LOG_ERROR("Failed to find plugin <%s>", uri);
    goto fail_free;
  }

  /* Instantiate the plugin */
  plugin_ptr->instance = slv2_plugin_instantiate(plugin_ptr->plugin, jack_get_sample_rate(g_jack_client), NULL);
  if (plugin_ptr->instance == NULL)
  {
    LOG_ERROR("Failed to instantiate plugin.");
    goto fail_free;
  }

  /* Create ports */
  ports_count  = slv2_plugin_get_num_ports(plugin_ptr->plugin);

  for (i = 0 ; i < ports_count ; i++)
  {
    if (!create_port(plugin_ptr, i))
    {
      LOG_ERROR("Failed to create plugin port");
      goto fail_free_ports;
    }
  }

  name = slv2_plugin_get_name(plugin_ptr->plugin);
  if (name == NULL)
  {
    LOG_ERROR("Failed to get plugin name");
    goto fail_free_ports;
  }

  size_name = strlen(name);
  port_name = (char *)malloc(size_name + 1024);
  if (port_name == NULL)
  {
    free(name);
    LOG_ERROR("Failed to allocate memory for port name");
    goto fail_free_ports;
  }

  size_id = sprintf(port_name, "%u:", id);
  memcpy(port_name + size_id, name, size_name);

  if (plugin_ptr->audio_out_left_port.type == PORT_TYPE_AUDIO &&
      plugin_ptr->audio_out_right_port.type == PORT_TYPE_AUDIO)
  {
    sprintf(port_name + size_id + size_name, " L");
    plugin_ptr->audio_out_left_port.data.audio = jack_port_register(g_jack_client, port_name, JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
    list_add_tail(&plugin_ptr->audio_out_left_port.port_type_siblings, &g_audio_ports);

    sprintf(port_name + size_id + size_name, " R");
    plugin_ptr->audio_out_right_port.data.audio = jack_port_register(g_jack_client, port_name, JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
    list_add_tail(&plugin_ptr->audio_out_right_port.port_type_siblings, &g_audio_ports);
  }
  else if (plugin_ptr->audio_out_left_port.type == PORT_TYPE_AUDIO &&
           plugin_ptr->audio_out_right_port.type == PORT_TYPE_INVALID)
  {
    port_name[size_id + size_name] = 0;
    plugin_ptr->audio_out_left_port.data.audio = jack_port_register(g_jack_client, port_name, JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
    list_add_tail(&plugin_ptr->audio_out_left_port.port_type_siblings, &g_audio_ports);
  }

  free(name);

  id++;

  list_add_tail(&plugin_ptr->siblings, &g_plugins);

  /* Activate plugin and JACK */
  slv2_instance_activate(plugin_ptr->instance);

  LOG_DEBUG("Constructed plugin <%s>", slv2_plugin_get_uri(plugin_ptr->plugin));

  return plugin_ptr;

fail_free_ports:
  zynjacku_plugin_free_ports(plugin_ptr);

fail_free:
  free(plugin_ptr);
fail:
  return NULL;
}

void
zynjacku_plugin_destruct(struct zynjacku_plugin * plugin_ptr)
{
  LOG_DEBUG("Destructing plugin <%s>", slv2_plugin_get_uri(plugin_ptr->plugin));

  slv2_instance_free(plugin_ptr->instance);

  zynjacku_plugin_free_ports(plugin_ptr);

  free(plugin_ptr);
}

int
main(int argc, char** argv)
{
  int ret;
  struct zynjacku_plugin * plugin_ptr;
  struct list_head * node_ptr;
  int plugins_count;

  INIT_LIST_HEAD(&g_plugins);
  INIT_LIST_HEAD(&g_midi_ports);
  INIT_LIST_HEAD(&g_audio_ports);

  /* Connect to JACK (with plugin name as client name) */
  g_jack_client = jack_client_open("zynjacku", JackNullOption, NULL);
  if (!g_jack_client)
  {
    LOG_ERROR("Failed to connect to JACK.");
    ret = EXIT_FAILURE;
    goto exit;
  }

  ret = jack_set_process_callback(g_jack_client, &jack_process_cb, NULL);
  if (ret != 0)
  {
    LOG_ERROR("jack_set_process_callback() failed.");
    ret = EXIT_FAILURE;
    goto close_jack_client;
  }

  g_lv2_midi_buffer.capacity = LV2MIDI_BUFFER_SIZE;
  g_lv2_midi_buffer.data = malloc(LV2MIDI_BUFFER_SIZE);
  if (g_lv2_midi_buffer.data == NULL)
  {
    LOG_ERROR("Failed to allocate memory for LV2 midi data buffer.");
    ret = EXIT_FAILURE;
    goto close_jack_client;
  }

  /* register JACK MIDI input port */
  g_jack_midi_in = jack_port_register(g_jack_client, "midi in", JACK_DEFAULT_MIDI_TYPE, JackPortIsInput, 0);
  if (g_jack_midi_in == NULL)
  {
    LOG_ERROR("Failed to registe JACK MIDI input port.");
    ret = EXIT_FAILURE;
    goto free_lv2_midi_buffer;
  }

  argv++;
  argc--;

  if (argc == 0)
  {
    LOG_NOTICE("You must specify at least one simple LV2 synth plugin URI to load.");
    //LOG_NOTICE("Available simple LV2 synth plugins:");
    //zynjacku_dump_simple_plugins();
    ret = EXIT_FAILURE;
    goto unregister_midi_port;
  }

  while (argc)
  {
    plugins_count++;
    plugin_ptr = zynjacku_plugin_construct(*argv);
    if (plugin_ptr == NULL)
    {
      LOG_ERROR("Failed to instantiate plugin <%s>", *argv);
    }
    argv++;
    argc--;
  }

  if (!list_empty(&g_plugins))
  {
    jack_activate(g_jack_client);

    /* Run */
    printf("Press enter to quit: ");
    getc(stdin);
    printf("\n");

    LOG_NOTICE("Shutting down JACK.");

    /* Deactivate JACK */
    jack_deactivate(g_jack_client);
  }

//destroy_plugins:
  while (!list_empty(&g_plugins))
  {
    node_ptr = g_plugins.next;
    plugin_ptr = list_entry(node_ptr, struct zynjacku_plugin, siblings);
    list_del(node_ptr);
    zynjacku_plugin_destruct(plugin_ptr);
  }

unregister_midi_port:
  jack_port_unregister(g_jack_client, g_jack_midi_in);

free_lv2_midi_buffer:
  free(g_lv2_midi_buffer.data);

close_jack_client:
  jack_client_close(g_jack_client);

exit:
  assert(list_empty(&g_audio_ports));
  assert(list_empty(&g_midi_ports));

  return ret;
}

BOOL
create_port(struct zynjacku_plugin * plugin_ptr, uint32_t port_index)
{
  enum SLV2PortClass class;
  char * type;
  char * symbol;
  struct zynjacku_plugin_port * port_ptr;
  BOOL ret;

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
      LOG_INFO("Set %s to %f", symbol, port_ptr->data.parameter);
      list_add_tail(&port_ptr->plugin_siblings, &plugin_ptr->parameter_ports);
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
        LOG_ERROR("Maximum two audio output ports are supported.");
        goto fail;
      }

      port_ptr->type = PORT_TYPE_AUDIO;
      port_ptr->index = port_index;
    }
    else if (class == SLV2_AUDIO_RATE_INPUT)
    {
      LOG_ERROR("audio input ports are not supported.");
      goto fail;
    }
    else if (class == SLV2_CONTROL_RATE_OUTPUT)
    {
      LOG_ERROR("control rate float output ports are not supported.");
      goto fail;
    }
    else
    {
      LOG_ERROR("unrecognized port.");
      goto fail;
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
        LOG_ERROR("maximum one midi input port is supported.");
        goto fail;
      }

      port_ptr->type = PORT_TYPE_MIDI;
      port_ptr->index = port_index;
      slv2_instance_connect_port(plugin_ptr->instance, port_index, &g_lv2_midi_buffer);
      list_add_tail(&port_ptr->port_type_siblings, &g_midi_ports);
    }
    else if (class == SLV2_CONTROL_RATE_OUTPUT)
    {
      LOG_ERROR("midi output ports are not supported.");
      goto fail;
    }
    else
    {
      LOG_ERROR("unrecognized port.");
      goto fail;
    }
  }
  else
  {
    LOG_ERROR("unrecognized data type.");
    goto fail;
  }

  ret = TRUE;
  goto exit;
fail:
  ret = FALSE;
exit:
  free(type);
  free(symbol);

  return ret;
}

/* Translate from a JACK MIDI buffer to an LV2 MIDI buffer. */
void jackmidi2lv2midi(jack_port_t * jack_port, LV2_MIDI * output_buf, jack_nframes_t nframes)
{
  void * input_buf;
  jack_midi_event_t input_event;
  jack_nframes_t input_event_index;
  jack_nframes_t input_event_count;
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

/* Jack process callback. */
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
