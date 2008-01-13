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

#include <stdio.h>
#include <string.h>
#include <slv2/slv2.h>
#include <jack/jack.h>
#include <jack/midiport.h>
#include <glib-object.h>
#include <lv2dynparam/lv2dynparam.h>
#include <lv2dynparam/lv2_rtmempool.h>
#include <lv2dynparam/host.h>

#include "config.h"

#include "lv2-miditype.h"
#include "list.h"
#define LOG_LEVEL LOG_LEVEL_ERROR
#include "log.h"

#include "synth.h"
#include "engine.h"
#include "gtk2gui.h"

#include "zynjacku.h"

#include "jack_compat.c"

#include "rtmempool.h"

int
jack_process_cb(
  jack_nframes_t nframes,
  void* data);

static void
zynjacku_engine_dispose(GObject * obj)
{
  struct zynjacku_engine * engine_ptr;

  engine_ptr = ZYNJACKU_ENGINE_GET_PRIVATE(obj);

  LOG_DEBUG("zynjacku_engine_dispose() called.");

  if (engine_ptr->dispose_has_run)
  {
    /* If dispose did already run, return. */
    LOG_DEBUG("zynjacku_engine_dispose() already run!");
    return;
  }

  /* Make sure dispose does not run twice. */
  engine_ptr->dispose_has_run = TRUE;

  /* 
   * In dispose, you are supposed to free all types referenced from this
   * object which might themselves hold a reference to self. Generally,
   * the most simple solution is to unref all members on which you own a 
   * reference.
   */
  if (engine_ptr->jack_client)
  {
    zynjacku_engine_stop_jack(ZYNJACKU_ENGINE(obj));
  }

  /* Chain up to the parent class */
  G_OBJECT_CLASS(g_type_class_peek_parent(G_OBJECT_GET_CLASS(obj)))->dispose(obj);
}

static void
zynjacku_engine_finalize(GObject * obj)
{
//  struct zynjacku_engine * self = ZYNJACKU_ENGINE_GET_PRIVATE(obj);

  LOG_DEBUG("zynjacku_engine_finalize() called.");

  /*
   * Here, complete object destruction.
   * You might not need to do much...
   */
  //g_free(self->private);

  /* Chain up to the parent class */
  G_OBJECT_CLASS(g_type_class_peek_parent(G_OBJECT_GET_CLASS(obj)))->finalize(obj);
}

static void
zynjacku_engine_class_init(
  gpointer class_ptr,
  gpointer class_data_ptr)
{
  LOG_DEBUG("zynjacku_engine_class() called.");

  G_OBJECT_CLASS(class_ptr)->dispose = zynjacku_engine_dispose;
  G_OBJECT_CLASS(class_ptr)->finalize = zynjacku_engine_finalize;

  g_type_class_add_private(G_OBJECT_CLASS(class_ptr), sizeof(struct zynjacku_engine));
}

static void
zynjacku_engine_init(
  GTypeInstance * instance,
  gpointer g_class)
{
  struct zynjacku_engine * engine_ptr;

  LOG_DEBUG("zynjacku_engine_init() called.");

  engine_ptr = ZYNJACKU_ENGINE_GET_PRIVATE(instance);

  engine_ptr->dispose_has_run = FALSE;

  engine_ptr->jack_client = NULL;

  rtmempool_allocator_init(&engine_ptr->mempool_allocator);

  engine_ptr->host_feature_rtmempool.URI = LV2_RTSAFE_MEMORY_POOL_URI;
  engine_ptr->host_feature_rtmempool.data = &engine_ptr->mempool_allocator;

  engine_ptr->host_features[0] = &engine_ptr->host_feature_rtmempool;
  engine_ptr->host_features[1] = NULL;
}

GType zynjacku_engine_get_type()
{
  static GType type = 0;
  if (type == 0)
  {
    type = g_type_register_static_simple(
      G_TYPE_OBJECT,
      "zynjacku_engine_type",
      sizeof(ZynjackuEngineClass),
      zynjacku_engine_class_init,
      sizeof(ZynjackuEngine),
      zynjacku_engine_init,
      0);
  }

  return type;
}

gboolean
zynjacku_engine_start_jack(
  ZynjackuEngine * obj_ptr,
  const char * client_name)
{
  gboolean ret;
  int iret;
  struct zynjacku_engine * engine_ptr;

  LOG_DEBUG("zynjacku_engine_start_jack() called.");

  engine_ptr = ZYNJACKU_ENGINE_GET_PRIVATE(obj_ptr);

  if (engine_ptr->jack_client != NULL)
  {
    LOG_ERROR("Cannot start already started JACK client");
    goto fail;
  }

  INIT_LIST_HEAD(&engine_ptr->plugins);
  INIT_LIST_HEAD(&engine_ptr->midi_ports);
  INIT_LIST_HEAD(&engine_ptr->audio_ports);

  /* Connect to JACK (with plugin name as client name) */
  engine_ptr->jack_client = jack_client_open(client_name, JackNullOption, NULL);
  if (engine_ptr->jack_client == NULL)
  {
    LOG_ERROR("Failed to connect to JACK.");
    goto fail;
  }

  iret = jack_set_process_callback(engine_ptr->jack_client, &jack_process_cb, engine_ptr);
  if (iret != 0)
  {
    LOG_ERROR("jack_set_process_callback() failed.");
    ret = FALSE;
    goto fail_close_jack_client;
  }

  engine_ptr->lv2_midi_buffer.capacity = LV2MIDI_BUFFER_SIZE;
  engine_ptr->lv2_midi_buffer.data = malloc(LV2MIDI_BUFFER_SIZE);
  if (engine_ptr->lv2_midi_buffer.data == NULL)
  {
    LOG_ERROR("Failed to allocate memory for LV2 midi data buffer.");
    ret = FALSE;
    goto fail_close_jack_client;
  }

  /* register JACK MIDI input port */
  engine_ptr->jack_midi_in = jack_port_register(engine_ptr->jack_client, "midi in", JACK_DEFAULT_MIDI_TYPE, JackPortIsInput, 0);
  if (engine_ptr->jack_midi_in == NULL)
  {
    LOG_ERROR("Failed to registe JACK MIDI input port.");
    ret = FALSE;
    goto fail_free_lv2_midi_buffer;
  }

  jack_activate(engine_ptr->jack_client);

  LOG_NOTICE("JACK client activated.");

  return TRUE;

fail_free_lv2_midi_buffer:
  free(engine_ptr->lv2_midi_buffer.data);

fail_close_jack_client:
  jack_client_close(engine_ptr->jack_client);
  engine_ptr->jack_client = NULL;

fail:
  assert(list_empty(&engine_ptr->audio_ports));
  assert(list_empty(&engine_ptr->midi_ports));

  return FALSE;
}

void
zynjacku_engine_stop_jack(
  ZynjackuEngine * obj_ptr)
{
  struct zynjacku_engine * engine_ptr;

  LOG_DEBUG("zynjacku_engine_stop_jack() called.");

  engine_ptr = ZYNJACKU_ENGINE_GET_PRIVATE(obj_ptr);

  if (engine_ptr->jack_client == NULL)
  {
    LOG_ERROR("Cannot stop not started JACK client");
    return;
  }

  if (!list_empty(&engine_ptr->plugins))
  {
    LOG_ERROR("Cannot stop JACK client when there are active synths");
    return;
  }

  LOG_NOTICE("Deactivating JACK client...");

  /* Deactivate JACK */
  jack_deactivate(engine_ptr->jack_client);

  jack_port_unregister(engine_ptr->jack_client, engine_ptr->jack_midi_in);

  free(engine_ptr->lv2_midi_buffer.data);

  jack_client_close(engine_ptr->jack_client);

  engine_ptr->jack_client = NULL;

  assert(list_empty(&engine_ptr->audio_ports));
  assert(list_empty(&engine_ptr->midi_ports));
}

/* Translate from a JACK MIDI buffer to an LV2 MIDI buffer. */
gboolean jackmidi2lv2midi(jack_port_t * jack_port, LV2_MIDI * output_buf, jack_nframes_t nframes)
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
  input_event_count = jack_midi_get_event_count(input_buf);

  /* iterate over all incoming JACK MIDI events */
  data = output_buf->data;
  for (i = 0; i < input_event_count; i++)
  {
    /* retrieve JACK MIDI event */
    jack_midi_event_get(&input_event, input_buf, i);
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

  return input_event_count != 0;
}

#define engine_ptr ((struct zynjacku_engine *)context_ptr)

/* Jack process callback. */
int
jack_process_cb(
  jack_nframes_t nframes,
  void * context_ptr)
{
  struct list_head * synth_node_ptr;
  struct zynjacku_synth * synth_ptr;

  /* Copy MIDI input data to all LV2 midi in ports */
  if (jackmidi2lv2midi(engine_ptr->jack_midi_in, &engine_ptr->lv2_midi_buffer, nframes))
  {
    engine_ptr->midi_activity = TRUE;
  }

  /* Iterate over plugins */
  list_for_each(synth_node_ptr, &engine_ptr->plugins)
  {
    synth_ptr = list_entry(synth_node_ptr, struct zynjacku_synth, siblings);

    if (synth_ptr->dynparams)
    {
      lv2dynparam_host_realtime_run(synth_ptr->dynparams);
    }

    /* Connect plugin LV2 output audio ports directly to JACK buffers */
    if (synth_ptr->audio_out_left_port.type == PORT_TYPE_AUDIO)
    {
      slv2_instance_connect_port(
        synth_ptr->instance,
        synth_ptr->audio_out_left_port.index,
        jack_port_get_buffer(synth_ptr->audio_out_left_port.data.audio, nframes));
    }

    /* Connect plugin LV2 output audio ports directly to JACK buffers */
    if (synth_ptr->audio_out_right_port.type == PORT_TYPE_AUDIO)
    {
      slv2_instance_connect_port(
        synth_ptr->instance,
        synth_ptr->audio_out_right_port.index,
        jack_port_get_buffer(synth_ptr->audio_out_right_port.data.audio, nframes));
    }

    /* Run plugin for this cycle */
    slv2_instance_run(synth_ptr->instance, nframes);
  }

  return 0;
}

#undef engine_ptr

void
zynjacku_engine_activate_synth(
  ZynjackuEngine * engine_obj_ptr,
  GObject * synth_obj_ptr)
{
  struct zynjacku_engine * engine_ptr;
  struct zynjacku_synth * synth_ptr;

  LOG_DEBUG("zynjacku_engine_add_synth() called.");

  engine_ptr = ZYNJACKU_ENGINE_GET_PRIVATE(engine_obj_ptr);
  synth_ptr = ZYNJACKU_SYNTH_GET_PRIVATE(synth_obj_ptr);

  list_add_tail(&synth_ptr->siblings, &engine_ptr->plugins);

  /* Activate plugin */
  slv2_instance_activate(synth_ptr->instance);
}

void
zynjacku_engine_deactivate_synth(
  ZynjackuEngine * engine_obj_ptr,
  GObject * synth_obj_ptr)
{
  struct zynjacku_synth * synth_ptr;

  synth_ptr = ZYNJACKU_SYNTH_GET_PRIVATE(synth_obj_ptr);

  slv2_instance_deactivate(synth_ptr->instance);

  list_del(&synth_ptr->siblings);
}

void
zynjacku_engine_ui_run(
  ZynjackuEngine * engine_obj_ptr)
{
  struct list_head * synth_node_ptr;
  struct zynjacku_synth * synth_ptr;
  struct zynjacku_engine * engine_ptr;

//  LOG_DEBUG("zynjacku_engine_ui_run() called.");

  engine_ptr = ZYNJACKU_ENGINE_GET_PRIVATE(engine_obj_ptr);

  /* Iterate over plugins */
  list_for_each(synth_node_ptr, &engine_ptr->plugins)
  {
    synth_ptr = list_entry(synth_node_ptr, struct zynjacku_synth, siblings);

    if (synth_ptr->dynparams)
    {
      lv2dynparam_host_ui_run(synth_ptr->dynparams);
    }
  }
}

guint
zynjacku_engine_get_sample_rate(
  ZynjackuEngine * engine_obj_ptr)
{
  struct zynjacku_engine * engine_ptr;
  engine_ptr = ZYNJACKU_ENGINE_GET_PRIVATE(engine_obj_ptr);

  if (engine_ptr->jack_client == NULL)
  {
    g_assert_not_reached();
    return 0xDEADBEAF;
  }

  return jack_get_sample_rate(engine_ptr->jack_client);
}

gboolean
zynjacku_engine_get_midi_activity(
  ZynjackuEngine * engine_obj_ptr)
{
  gboolean ret;
  struct zynjacku_engine * engine_ptr;

  engine_ptr = ZYNJACKU_ENGINE_GET_PRIVATE(engine_obj_ptr);

  ret = engine_ptr->midi_activity;

  engine_ptr->midi_activity = FALSE;

  return ret;
}

const gchar *
zynjacku_get_version()
{
  return VERSION;
}
