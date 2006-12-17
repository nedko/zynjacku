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

#ifndef AUDIOLOCK_H__E7FF7044_126C_402E_81C6_B6E17A046295__INCLUDED
#define AUDIOLOCK_H__E7FF7044_126C_402E_81C6_B6E17A046295__INCLUDED

typedef struct _audiolock_handle { int unused; } * audiolock_handle;
#define AUDIOLOCK_HANDLE_INVALID_VALUE NULL

/* Creates lock implementing optimistic approach.
 *
 * Returns AUDIOLOCK_HANDLE_INVALID_VALUE in case of failure.
 */
audiolock_handle audiolock_create_optimistic();

/* Creates lock implementing pessimistic approach.
 *
 * Returns AUDIOLOCK_HANDLE_INVALID_VALUE in case of failure.
 */
audiolock_handle audiolock_create_pessimistic();

/* Creates lock implementing "Not time critical UI <-> audio thread data
 * transfer" approach.
 *
 * Returns AUDIOLOCK_HANDLE_INVALID_VALUE in case of failure.
 */
audiolock_handle audiolock_create_slow();

/* Returns zero if lock is owned by UI thread.
 * For pessimistc and optimistic audiolocks always returns non-zero
 */
int audiolock_enter_audio(audiolock_handle lock);

void audiolock_leave_audio(audiolock_handle lock);

void audiolock_enter_ui(audiolock_handle lock);

void audiolock_leave_ui(audiolock_handle lock);

void audiolock_destroy(audiolock_handle lock);

#endif /* #ifndef AUDIOLOCK_H__E7FF7044_126C_402E_81C6_B6E17A046295__INCLUDED */
