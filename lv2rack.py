#!/usr/bin/env python
#
# This file is part of zynjacku
#
# Copyright (C) 2008,2009 Nedko Arnaudov <nedko@arnaudov.name>
#  
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 2 of the License
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.

import os
import sys
import gtk
import gtk.glade
import gobject
import re
import time

old_path = sys.path
sys.path.insert(0, "%s/.libs" % os.path.dirname(sys.argv[0]))
import zynjacku_c
sys.path = old_path

import zynjacku as zynjacku

try:
    import lash
except:
    print "Cannot load LASH python bindings, you want LASH unless you enjoy manual jack plumbing each time you use this app"
    lash = None

class lv2rack(zynjacku.host):
    def __init__(self, data_dir, glade_xml, client_name, the_license, uris, lash_client):
        #print "lv2rack constructor called."
        zynjacku.host.__init__(self, zynjacku_c.Rack(), client_name, "lv2rack", "effect stack", lash_client)
        
        self.data_dir = data_dir
        self.glade_xml = glade_xml

        self.main_window = glade_xml.get_widget("lv2rack")
        self.main_window.set_title(client_name)

        self.statusbar = self.glade_xml.get_widget("lv2rack_statusbar")

	# Create our dictionary and connect it
        dic = {"lv2rack_quit_menuitem" : self.on_quit,
               "lv2rack_help_about_menuitem" : self.on_about,
               "lv2rack_preset_load_menuitem" : self.on_preset_load,
               "lv2rack_preset_save_as_menuitem" : self.on_preset_save_as,
               "lv2rack_effect_load_menuitem" : self.on_effect_load,
               "lv2rack_effect_clear_menuitem" : self.on_effect_clear,
               }

        self.signal_ids = []
        for k, v in dic.items():
            w = glade_xml.get_widget(k)
            if not w:
                print "failed to get glade widget '%s'" % k
                continue
            self.signal_ids.append([w, w.connect("activate", v)])

        self.the_license = the_license

        self.effects_widget = glade_xml.get_widget("lv2rack_treeview_effects")

        self.store = gtk.ListStore(gobject.TYPE_BOOLEAN, gobject.TYPE_STRING, gobject.TYPE_STRING, gobject.TYPE_STRING, gobject.TYPE_PYOBJECT)
        text_renderer = gtk.CellRendererText()
        self.toggle_renderer = gtk.CellRendererToggle()
        self.toggle_renderer.set_property('activatable', True)

        column_ui_visible = gtk.TreeViewColumn("UI", self.toggle_renderer)
        column_ui_visible.add_attribute(self.toggle_renderer, "active", 0)
        column_instance = gtk.TreeViewColumn("Instance", text_renderer, text=1)
        column_name = gtk.TreeViewColumn("Name", text_renderer, text=2)
        #column_uri = gtk.TreeViewColumn("URI", text_renderer, text=3)

        self.effects_widget.append_column(column_ui_visible)
        self.effects_widget.append_column(column_instance)
        self.effects_widget.append_column(column_name)
        #self.effects_widget.append_column(column_uri)

        self.effects_widget.set_model(self.store)

        self.main_window.show_all()
        self.signal_ids.append([self.main_window, self.main_window.connect("destroy", self.on_quit)])

        if len(uris) == 1 and uris[0][-8:] == ".lv2rack":
            self.preset_load(uris[0])
        else:
            for uri in uris:
                self.load_plugin(uri)
            self.progress_window.hide()

    def on_quit(self, window=None):
        #print "ZynjackuHostMulti::on_quit() called"
        for cid in self.signal_ids:
            #print "disconnecting %u" % cid[1]
            cid[0].disconnect(cid[1])
            #print "host object refcount is %u" % sys.getrefcount(self)
        gtk.main_quit()

    def __del__(self):
        #print "lv2rack destructor called."

        self.store.clear()

        zynjacku.host.__del__(self)

    def new_plugin(self, uri, parameters=[], maps={}):
        self.progress_window.show(uri)
        plugin = zynjacku.host.new_plugin(self, uri, parameters, maps)
        return plugin

    def on_plugin_progress(self, engine, name, progress, message):
        self.progress_window.progress(name, progress, message)

    def load_plugin(self, uri, parameters=[], maps={}):
        statusbar_context_id = self.statusbar.get_context_id("loading plugin")
        statusbar_id = self.statusbar.push(statusbar_context_id, "Loading %s" % uri)
        while gtk.events_pending():
            gtk.main_iteration()
        self.statusbar.pop(statusbar_id)
        effect = self.new_plugin(uri, parameters, maps)
        if not effect:
            self.statusbar.push(statusbar_context_id, "Failed to construct %s" % uri)
        else:
            row = False, effect.get_instance_name(), effect.get_name(), effect.get_uri(), effect
            self.store.append(row)
            self.statusbar.remove(statusbar_context_id, statusbar_id)

    def run(self):
        toggled_connect_id = self.toggle_renderer.connect('toggled', self.on_ui_visible_toggled, self.store)

        zynjacku.host.run(self)

        self.toggle_renderer.disconnect(toggled_connect_id)

    def on_plugin_ui_window_destroyed(self, window, effect, row):
        effect.ui_win.disconnect(effect.ui_win.destroy_connect_id) # signal connection holds reference to effect object...
        effect.ui_win = None
        row[0] = False

    def on_ui_visible_toggled(self, cell, path, model):
        #print "on_ui_visible_toggled() called."
        if model[path][0]:
            model[path][4].ui_win.hide()
            model[path][0] = False
        else:
            if not model[path][4].ui_win:
                if not self.create_plugin_ui(model[path][4], model[path]):
                    return

            statusbar_context_id = self.statusbar.get_context_id("loading plugin UI")
            if model[path][4].ui_win.show():
                model[path][0] = True
            else:
                self.statusbar.push(statusbar_context_id, "Failed to construct show effect UI")

    def on_about(self, widget):
        about = gtk.AboutDialog()
        about.set_transient_for(self.main_window)
        about.set_name("lv2rack")
        if zynjacku_c.zynjacku_get_version() == "dev":
            about.set_comments("(development snapshot)")
        else:
            about.set_version(zynjacku_c.zynjacku_get_version())
        about.set_license(self.the_license)
        about.set_website("http://home.gna.org/zynjacku/")
        about.set_authors(["Nedko Arnaudov"])
        #about.set_artists(["Thorsten Wilms"])
        #about.set_logo(gtk.gdk.pixbuf_new_from_file("%s/logo.png" % self.data_dir))
        about.show()
        about.run()
        about.hide()

    def on_preset_load(self, widget):
        self.preset_load_ask()

    def preset_get_pre_plugins_xml(self):
        xml = "<lv2rack>\n"
        xml += "  <plugins>\n"
        return xml

    def preset_get_post_plugins_xml(self):
        xml = "  </plugins>\n"
        xml += "</lv2rack>\n"
        return xml

    def on_preset_save_as(self, widget):
        self.preset_save_ask()

    def on_plugin_repo_tick(self, repo, progress, uri, progressbar):
        if progress == 1.0:
            progressbar.hide()
            return

        progressbar.show()
        progressbar.set_fraction(progress)
        progressbar.set_text("Checking %s" % uri);
        while gtk.events_pending():
            gtk.main_iteration()

    def on_effect_load(self, widget):
        self.plugins_load("LV2 effect plugins")

    def on_effect_clear(self, widget):
        self.store.clear();
        self.clear_plugins()

def main():
    data_dir, glade_xml, the_license = zynjacku.file_setup()

    zynjacku.register_types()

    if lash:                        # If LASH python bindings are available
        # sys.argv is modified by this call
        lash_client = lash.init(sys.argv, "lv2rack", lash.LASH_Config_File)
    else:
        lash_client = None

    # TODO: generic argument processing goes here

    # Yeah , this sounds stupid, we connected earlier, but we dont want to show this if we got --help option
    # This issue should be fixed in pylash, there is a reason for having two functions for initialization after all
    if lash_client:
        print "Successfully connected to LASH server at " +  lash.lash_get_server_name(lash_client)

    lv2rack(data_dir, glade_xml, "lv2rack", the_license, sys.argv[1:], lash_client).run()

    sys.stdout.flush()
    sys.stderr.flush()

if __name__ == '__main__':
    main()
