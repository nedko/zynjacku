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

#ifndef PLUGIN_REPO_H__27C1E0DC_DD5E_4A79_838B_DC6B90402229__INCLUDED
#define PLUGIN_REPO_H__27C1E0DC_DD5E_4A79_838B_DC6B90402229__INCLUDED

typedef
void
(* zynjacku_plugin_repo_tick)(
  void * context,
  float progress,               /* 0..1 */
  const char * message);

typedef
void
(* zynjacku_plugin_repo_tack)(
  void * context,
  const char * uri);

typedef
bool
(* zynjacku_plugin_repo_check_plugin)(
  void * context,
  const char * plugin_uri,
  const char * plugin_name,
  uint32_t audio_in_ports_count,
  uint32_t audio_out_ports_count,
  uint32_t midi_in_ports_count,
  uint32_t control_ports_count,
  uint32_t string_ports_count,
  uint32_t event_ports_count,
  uint32_t midi_event_in_ports_count,
  uint32_t ports_count);

typedef
bool
(* zynjacku_plugin_repo_create_port)(
  void * context,
  unsigned int port_type,
  bool output,
  uint32_t port_index);

bool
zynjacku_plugin_repo_init();

void
zynjacku_plugin_repo_iterate(
  bool force_scan,
  const LV2_Feature * const * supported_features,
  void * context,
  zynjacku_plugin_repo_check_plugin check_plugin,
  zynjacku_plugin_repo_tick tick,
  zynjacku_plugin_repo_tack tack);

const char *
zynjacku_plugin_repo_get_name(
  const char * uri);

const char *
zynjacku_plugin_repo_get_license(
  const char * uri);

const char *
zynjacku_plugin_repo_get_author(
  const char * uri);

const char *
zynjacku_plugin_repo_get_dlpath(
  const char * uri);

const char *
zynjacku_plugin_repo_get_bundle_path(
  const char * uri);

bool
zynjacku_plugin_repo_get_ui_info(
  const char * plugin_uri,
  const char * ui_type_uri,
  char ** ui_uri_ptr,
  char ** ui_binary_path_ptr,
  char ** ui_bundle_path_ptr);

bool
zynjacku_plugin_repo_load_plugin(
  struct zynjacku_plugin * synth_ptr,
  void * context,
  zynjacku_plugin_repo_create_port create_port,
  zynjacku_plugin_repo_check_plugin check_plugin,
  const LV2_Feature * const * supported_features);

void
zynjacku_plugin_repo_uninit();

#endif /* #ifndef PLUGIN_REPO_H__27C1E0DC_DD5E_4A79_838B_DC6B90402229__INCLUDED */
