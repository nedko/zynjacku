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
from distutils import sysconfig

old_path = sys.path

inplace_libs = os.path.join(os.path.dirname(sys.argv[0]), ".libs")
if os.access(inplace_libs, os.R_OK):
    sys.path.append(inplace_libs)
else:
    inplace_libs = None

try:
    if inplace_libs:
        import zynjacku_c
    else:
        from zynworld import zynjacku_c
except Exception, e:
    print "Failed to import zynjacku internal python modules"
    print repr(e)
    print "These directories were searched"
    for path in sys.path:
        print "    " + path
    sys.exit(1)

sys.path = old_path

import zynjacku as zynjacku

try:
    import lash
except:
    print "Cannot load LASH python bindings, you want LASH unless you enjoy manual jack plumbing each time you use this app"
    lash = None

class lv2rack(zynjacku.host):
    def __init__(self, client_name, preset_extension=None, preset_name=None, lash_client=None):
        #print "lv2rack constructor called."

        zynjacku.host.__init__(self, zynjacku_c.Rack(), client_name, preset_extension, preset_name, lash_client)

class lv2rack_multi(lv2rack):
    def __init__(self, program_data, client_name, uris, lash_client):
        #print "lv2rack_multi constructor called."
        lv2rack.__init__(self, client_name, "lv2rack", "effect stack", lash_client)

        self.program_data = program_data
        self.glade_xml = program_data['glade_xml']

        self.main_window = self.glade_xml.get_widget("lv2rack")
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
            w = self.glade_xml.get_widget(k)
            if not w:
                print "failed to get glade widget '%s'" % k
                continue
            self.signal_ids.append([w, w.connect("activate", v)])

        self.effects_widget = self.glade_xml.get_widget("lv2rack_treeview_effects")

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

        lv2rack.__del__(self)

    def new_plugin(self, uri, parameters=[], maps={}):
        self.progress_window.show(uri)
        plugin = lv2rack.new_plugin(self, uri, parameters, maps)
        return plugin

    def on_plugin_progress(self, engine, name, progress, message):
        self.progress_window.progress(name, progress, message)

    def check_plugin(self, plugin):
        audio_in_ports_count = 0
        audio_out_ports_count = 0

        for port in plugin.ports:
            if port.__dict__["isAudio"]:
                if port.__dict__["isInput"]:
                    audio_in_ports_count += 1
                    continue
                if port.__dict__["isOutput"]:
                    audio_out_ports_count += 1
                    continue
                continue

        if audio_in_ports_count == 0 or audio_out_ports_count == 0:
#             print "Skipping %s (%s), [effect] plugin with unsupported port configuration" % (plugin.name, plugin.uri)
#             #print "  midi input ports: %d" % midi_in_ports_count
#             #print "  control ports: %d" % control_ports_count
#             #print "  string ports: %d" % string_ports_count
#             #print "  event ports: %d" % event_ports_count
#             #print "  event midi input ports: %d" % midi_event_in_ports_count
#             print "  audio input ports: %d" % audio_in_ports_count
#             print "  audio output ports: %d" % audio_out_ports_count
#             #print "  total ports %d" % ports_count
            return False;

#         print "Found effect plugin '%s' %s", (plugin.name, plugin.uri)
#         #print "  midi input ports: %d" % midi_in_ports_count
#         #print "  control ports: %d" % control_ports_count
#         #print "  string ports: %d" % string_ports_count
#         #print "  event ports: %d" % event_ports_count
#         #print "  event midi input ports: %d" % midi_event_in_ports_count
#         print "  audio input ports: %d" % audio_in_ports_count
#         print "  audio output ports: %d" % audio_out_ports_count
#         #print "  total ports %d" % ports_count
        return True;

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

        lv2rack.run(self)

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
        zynjacku.run_about_dialog(self.main_window, self.program_data)

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
        progressbar.set_text("Checking %s" % uri)
        while gtk.events_pending():
            gtk.main_iteration()

    def on_effect_load(self, widget):
        self.plugins_load("LV2 effect plugins")

    def on_effect_clear(self, widget):
        self.store.clear()
        self.clear_plugins()

class lv2rack_single(lv2rack):
    def __init__(self, program_data, client_name, uri):
        #print "ZynjackuHostOne constructor called."
        lv2rack.__init__(self, client_name, "lv2rack")

        self.glade_xml = program_data['glade_xml']

        self.plugin = self.new_plugin(uri)
        if not self.plugin:
            print"Failed to construct %s" % uri
            return

        if not lv2rack.create_plugin_ui(self, self.plugin):
            print"Failed to create synth window"
            return

    def new_plugin(self, uri, parameters=[], maps={}):
        self.progress_window.show(uri)
        plugin = lv2rack.new_plugin(self, uri, parameters, maps)
        self.progress_window.hide()
        return plugin

    def on_plugin_progress(self, engine, name, progress, message):
        self.progress_window.progress(name, progress, message)

    def on_plugin_ui_window_destroyed(self, window, synth, row):
        gtk.main_quit()

    def run(self):
        if not self.plugin or not self.plugin.ui_win.show():
            self.run_done()
            return

        lv2rack.run(self)

    def __del__(self):
        #print "lv2rack_single destructor called."

        lv2rack.__del__(self)

def main():
    program_data = zynjacku.get_program_data('lv2rack')

    zynjacku.register_types()

    client_name = "lv2rack"

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

    if len(sys.argv) == 2 and sys.argv[1][-9:] != ".lv2rack":
        host = lv2rack_single(program_data, client_name, sys.argv[1])
    else:
        host = lv2rack_multi(program_data, client_name, sys.argv[1:], lash_client)

    host.run()

    sys.stdout.flush()
    sys.stderr.flush()

if __name__ == '__main__':
    main()
