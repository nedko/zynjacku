/* -*- Mode: C ; c-basic-offset: 2 -*- */
/*****************************************************************************
 *
 *   This file is part of zynjacku
 *
 *   Copyright (C) 2006,2007,2008 Nedko Arnaudov <nedko@arnaudov.name>
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
  void *context,
  float progress,               /* 0..1 */
  const char *message);

typedef
void
( *zynjacku_plugin_repo_tack)(
  void *context,
  const char *uri);

bool
zynjacku_plugin_repo_init();

void
zynjacku_plugin_repo_iterate(
  bool force_scan,
  void *context,
  zynjacku_plugin_repo_tick tick,
  zynjacku_plugin_repo_tack tack);

const char *
zynjacku_plugin_repo_get_name(
  const char *uri);

const char *
zynjacku_plugin_repo_get_license(
  const char *uri);

const char *
zynjacku_plugin_repo_get_dlpath(
  const char *uri);

const char *
zynjacku_plugin_repo_get_bundle_path(
  const char *uri);

bool
zynjacku_plugin_repo_load_synth(
  struct zynjacku_synth * synth_ptr);

void
zynjacku_plugin_repo_uninit();

#endif /* #ifndef PLUGIN_REPO_H__27C1E0DC_DD5E_4A79_838B_DC6B90402229__INCLUDED */
