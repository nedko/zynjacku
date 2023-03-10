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

#ifndef LV2_H__F5AF3921_19C9_47C0_95B6_AF05FCD9C767__INCLUDED
#define LV2_H__F5AF3921_19C9_47C0_95B6_AF05FCD9C767__INCLUDED

typedef struct { int _unused; } * zynjacku_lv2_handle;
typedef struct { int _unused; } * zynjacku_lv2_dman_handle;

struct zynjacku_port;

zynjacku_lv2_dman_handle
zynjacku_lv2_dman_open(
  const char * dlpath);

char *
zynjacku_lv2_dman_get_subjects(
  zynjacku_lv2_dman_handle dman);

char *
zynjacku_lv2_dman_get_data(
  zynjacku_lv2_dman_handle dman,
  const char *uri);

void
zynjacku_lv2_dman_close(
  zynjacku_lv2_dman_handle dman);

zynjacku_lv2_handle
zynjacku_lv2_load(
  const char * uri,
  const char * dlpath,
  const char * bundle_path,
  double sample_rate,
  const LV2_Feature * const * host_features);

void
zynjacku_lv2_unload(
  zynjacku_lv2_handle lv2handle);

void
zynjacku_lv2_connect_port(
  zynjacku_lv2_handle lv2handle,
  struct zynjacku_port *port,
  void *data_location);

void
zynjacku_lv2_run(
  zynjacku_lv2_handle lv2handle,
  uint32_t sample_count);

void
zynjacku_lv2_activate(
  zynjacku_lv2_handle lv2handle);

void
zynjacku_lv2_deactivate(
  zynjacku_lv2_handle lv2handle);

const LV2_Descriptor *
zynjacku_lv2_get_descriptor(
  zynjacku_lv2_handle lv2handle);

LV2_Handle
zynjacku_lv2_get_handle(
  zynjacku_lv2_handle lv2handle);

void
zynjacku_lv2_message(
  zynjacku_lv2_handle lv2handle,
  const void *input_data,
  void *output_data);

#endif /* #ifndef LV2_H__F5AF3921_19C9_47C0_95B6_AF05FCD9C767__INCLUDED */
