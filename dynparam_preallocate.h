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

#ifndef DYNPARAM_PREALLOCATE_H__354DD885_C35E_4475_AD0A_BF1D11B6BD25__INCLUDED
#define DYNPARAM_PREALLOCATE_H__354DD885_C35E_4475_AD0A_BF1D11B6BD25__INCLUDED

void
lv2dynparam_preallocate();

struct lv2dynparam_host_group *
lv2dynparam_get_unused_group();

void
lv2dynparam_put_unused_group(
  struct lv2dynparam_host_group * group_ptr);

struct lv2dynparam_host_parameter *
lv2dynparam_get_unused_parameter();

void
lv2dynparam_put_unused_parameter(
  struct lv2dynparam_host_parameter * parameter_ptr);

/* return preallocated message struct */
/* realtime safe */
struct lv2dynparam_host_message *
lv2dynparam_get_unused_message();

/* return message struct, always. */
/* may block, not realtime safe */
struct lv2dynparam_host_message *
lv2dynparam_get_unused_message_may_block();

void
lv2dynparam_put_unused_message(
  struct lv2dynparam_host_message * message_ptr);

#endif /* #ifndef DYNPARAM_PREALLOCATE_H__354DD885_C35E_4475_AD0A_BF1D11B6BD25__INCLUDED */
