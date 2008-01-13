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

#ifndef SLV2_H__27C1E0DC_DD5E_4A79_838B_DC6B90402229__INCLUDED
#define SLV2_H__27C1E0DC_DD5E_4A79_838B_DC6B90402229__INCLUDED

G_BEGIN_DECLS

#define ZYNJACKU_PLUGIN_REPO_TYPE (zynjacku_plugin_repo_get_type())
#define ZYNJACKU_PLUGIN_REPO(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), ZYNJACKU_PLUGIN_REPO_TYPE, ZynjackuPluginRepo))
#define ZYNJACKU_PLUGIN_REPO_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), ZYNJACKU_PLUGIN_REPO_TYPE, ZynjackuPluginRepoClass))
#define ZYNJACKU_IS_PLUGIN_REPO(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), ZYNJACKU_PLUGIN_REPO_TYPE))
#define ZYNJACKU_IS_PLUGIN_REPO_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), ZYNJACKU_PLUGIN_REPO_TYPE))
#define ZYNJACKU_PLUGIN_REPO_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), ZYNJACKU_PLUGIN_REPO_TYPE, ZynjackuPluginRepoClass))

#define ZYNJACKU_TYPE_PLUGIN_REPO ZYNJACKU_PLUGIN_REPO_TYPE

typedef struct _ZynjackuPluginRepo ZynjackuPluginRepo;
typedef struct _ZynjackuPluginRepoClass ZynjackuPluginRepoClass;

struct _ZynjackuPluginRepo {
  GObject parent;
  /* instance members */
};

struct _ZynjackuPluginRepoClass {
  GObjectClass parent;
  /* class members */
};

/* used by ZYNJACKU_PLUGIN_REPO_TYPE */
GType zynjacku_plugin_repo_get_type();

ZynjackuPluginRepo *
zynjacku_plugin_repo_get();

void
zynjacku_plugin_repo_iterate(
  ZynjackuPluginRepo * repo_obj_ptr,
  gboolean force_scan);

G_END_DECLS

#endif /* #ifndef SLV2_H__27C1E0DC_DD5E_4A79_838B_DC6B90402229__INCLUDED */
