/* -*- Mode: C ; c-basic-offset: 2 -*- */
/*****************************************************************************
 *
 *   This file is part of zynjacku
 *
 *   Copyright (C) 2006 Nedko Arnaudov <nedko@arnaudov.name>
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

/* For supporting philosophy look at article at
 * http://nedko.arnaudov.name/wiki/moin.cgi/Accessing_data_from_audio_process_thread_and_UI_thread */

#include <pthread.h>
#include <assert.h>
#include <stdlib.h>
#include <errno.h>

#include "audiolock.h"

struct audiolock
{
  pthread_mutex_t mutex;
};

audiolock_handle audiolock_create_optimistic()
{
  return AUDIOLOCK_HANDLE_INVALID_VALUE; /* not implemented */
}

audiolock_handle audiolock_create_pessimistic()
{
  return AUDIOLOCK_HANDLE_INVALID_VALUE; /* not implemented */
}

audiolock_handle audiolock_create_slow()
{
  struct audiolock * audiolock_ptr;

  audiolock_ptr = malloc(sizeof(struct audiolock));
  if (audiolock_ptr == NULL)
  {
    return AUDIOLOCK_HANDLE_INVALID_VALUE;
  }

  pthread_mutex_init(&audiolock_ptr->mutex, NULL);

  return (audiolock_handle)audiolock_ptr;
}

#define audiolock_ptr ((struct audiolock *)lock)

int audiolock_enter_audio(audiolock_handle lock)
{
  int error;
  error = pthread_mutex_trylock(&audiolock_ptr->mutex);
  if (error == 0)
  {
    return 1;
  }

  assert(error == EBUSY);
  return 0;
}

void audiolock_leave_audio(audiolock_handle lock)
{
  pthread_mutex_unlock(&audiolock_ptr->mutex);
}

void audiolock_enter_ui(audiolock_handle lock)
{
  pthread_mutex_lock(&audiolock_ptr->mutex);
}

void audiolock_leave_ui(audiolock_handle lock)
{
  pthread_mutex_unlock(&audiolock_ptr->mutex);
}

void audiolock_destroy(audiolock_handle lock)
{
  int error;

  error = pthread_mutex_destroy(&audiolock_ptr->mutex);

  assert(error == 0);           /* destroy should fail only if mutex is currently locked */

  if (error != 0)
  {
    return;
  }

  free(audiolock_ptr);
}
