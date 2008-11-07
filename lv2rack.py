#!/usr/bin/env python
#
# This file is part of zynjacku
#
# Copyright (C) 2008 Nedko Arnaudov <nedko@arnaudov.name>
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
import phat
import re
import time

old_path = sys.path
sys.path.insert(0, "%s/.libs" % os.path.dirname(sys.argv[0]))
import zynjacku_c
sys.path = old_path

import zynjacku as zynjacku

class lv2rack(zynjacku.host):
    def __init__(self, data_dir, glade_xml, client_name, the_license, uris):
        #print "lv2rack constructor called."
        zynjacku.host.__init__(self, zynjacku_c.Rack(), client_name)
        
        self.data_dir = data_dir
        self.glade_xml = glade_xml

        self.main_window = glade_xml.get_widget("lv2rack")
        self.main_window.set_title(client_name)

        self.statusbar = self.glade_xml.get_widget("lv2rack_statusbar")

	#Create our dictionay and connect it
        dic = {"lv2rack_on_quit_activate" : gtk.main_quit,
               "lv2rack_on_about_activate" : self.on_about,
               "lv2rack_on_preset_load_activate" : self.on_preset_load,
               "lv2rack_on_preset_save_as_activate" : self.on_preset_save_as,
               "lv2rack_on_effect_load_activate" : self.on_effect_load,
               "lv2rack_on_effect_clear_activate" : self.on_effect_clear,
               }
        glade_xml.signal_autoconnect(dic)

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

        for uri in uris:
            self.add_effect(uri)

        self.effects_widget.set_model(self.store)

        self.main_window.show_all()
        self.main_window.connect("destroy", gtk.main_quit)

    def __del__(self):
        #print "lv2rack destructor called."

        self.store.clear()

        zynjacku.host.__del__(self)

    def add_effect(self, uri):
        statusbar_context_id = self.statusbar.get_context_id("loading plugin")
        statusbar_id = self.statusbar.push(statusbar_context_id, "Loading %s" % uri)
        while gtk.events_pending():
            gtk.main_iteration()
        self.statusbar.pop(statusbar_id)
        effect = self.new_plugin(uri)
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
        print "Preset load not implemented yet!"

    def on_preset_save_as(self, widget):
        print "Preset saving not implemented yet!"

    def on_plugin_repo_tick(self, repo, progress, uri, progressbar):
        if progress == 1.0:
            progressbar.hide()
            return

        progressbar.show()
        progressbar.set_fraction(progress)
        progressbar.set_text("Checking %s" % uri);
        while gtk.events_pending():
            gtk.main_iteration()

    def on_plugin_repo_tack(self, repo, name, uri, plugin_license, store):
        #print "tack: %s %s %s" % (name, uri, plugin_license)
        store.append([name, uri, plugin_license])

    def rescan_plugins(self, store, progressbar, force):
        store.clear()
        tick = self.engine.connect("tick", self.on_plugin_repo_tick, progressbar)
        tack = self.engine.connect("tack", self.on_plugin_repo_tack, store)
        self.engine.iterate_plugins(force)
        self.engine.disconnect(tack)
        self.engine.disconnect(tick)

    def on_effect_load(self, widget):
        dialog = self.glade_xml.get_widget("zynjacku_plugin_repo")
        plugin_repo_widget = self.glade_xml.get_widget("treeview_available_plugins")
        progressbar = self.glade_xml.get_widget("progressbar")

        dialog.set_title("LV2 effect plugins")

        plugin_repo_widget.get_selection().set_mode(gtk.SELECTION_MULTIPLE)

        store = gtk.ListStore(gobject.TYPE_STRING, gobject.TYPE_STRING, gobject.TYPE_STRING)
        text_renderer = gtk.CellRendererText()

        column_name = gtk.TreeViewColumn("Name", text_renderer, text=0)
        #column_uri = gtk.TreeViewColumn("URI", text_renderer, text=1)
        #column_license = gtk.TreeViewColumn("License", text_renderer, text=2)

        column_name.set_sort_column_id(0)
        #column_uri.set_sort_column_id(1)
        #column_license.set_sort_column_id(2)

        plugin_repo_widget.append_column(column_name)
        #plugin_repo_widget.append_column(column_uri)
        #plugin_repo_widget.append_column(column_license)

        plugin_repo_widget.set_model(store)

        dialog.show()
        self.rescan_plugins(store, progressbar, False)
        while True:
            ret = dialog.run()
            if ret == 0:
                dialog.hide()
                for path in plugin_repo_widget.get_selection().get_selected_rows()[1]:
                    self.add_effect(store.get(store.get_iter(path), 1)[0])
                return
            elif ret == 1:
                self.rescan_plugins(store, progressbar, True)
            else:
                dialog.hide()
                return

    def on_effect_clear(self, widget):
        self.store.clear();
        self.clear_plugins()

def main():
    data_dir, glade_xml, the_license = zynjacku.file_setup()

    zynjacku.register_types()

    lv2rack(data_dir, glade_xml, "lv2rack", the_license, sys.argv[1:]).run()

if __name__ == '__main__':
    main()
