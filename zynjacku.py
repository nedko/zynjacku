#!/usr/bin/env python
#
# This file is part of zynjacku
#
# Copyright (C) 2006 Nedko Arnaudov <nedko@arnaudov.name>
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
import zynjacku
import gtk
import gtk.glade
import gobject

the_host = None

def on_group_added(synth, parent, group_name, context):
    print "on_group_added() called !!!!!!!!!!!!!!!!!!"
    print "synth: %s" % repr(synth)
    print "parent: %s" % repr(parent)
    print "group_name: %s" % group_name
    print "context: %s" % repr(context)
    return the_host

def on_test(obj1, obj2):
    print "on_test() called !!!!!!!!!!!!!!!!!!"
    print repr(obj1)
    print repr(obj2)

class ZynjackuHost(gobject.GObject):
    def __init__(self, client_name):
        print "ZynjackuHost constructor called."
        self.engine = zynjacku.Engine()

        if not self.engine.start_jack(client_name):
            print "Failed to initialize zynjacku engine"
            sys.exit(1)

    def __del__(self):
        print "ZynjackuHost destructor called."

        self.engine.stop_jack()

    def ui_run(self):
        self.engine.ui_run()
        return True

    def run(self):
        ui_run_callback_id = gobject.timeout_add(100, self.ui_run)
        gtk.main()
        gobject.source_remove(ui_run_callback_id)

    def create_synth_window(self, synth):
        ui_win = gtk.Window(gtk.WINDOW_TOPLEVEL)
        ui_win.set_title(synth.get_name())
        ui_win.set_role("zynjacku_synth_ui")
        return ui_win

class ZynjackuHostMulti(ZynjackuHost):
    def __init__(self, glade_xml, client_name, uris):
        print "ZynjackuHostMulti constructor called."
        ZynjackuHost.__init__(self, client_name)
        
        self.synths = []

        self.main_window = glade_xml.get_widget("zynjacku_main")
        self.main_window.set_title(client_name)

        for uri in uris:
            print "Loading %s" % uri
            synth = zynjacku.Synth(uri=uri)
            synth.connect("group-added", on_group_added)
            if not synth.construct(self.engine):
                print"Failed to construct %s" % uri
            else:
                self.synths.append(synth)
                synth.ui_win = None

        self.synths_widget = glade_xml.get_widget("treeview_synths")

        self.store = gtk.ListStore(gobject.TYPE_BOOLEAN, gobject.TYPE_STRING, gobject.TYPE_STRING, gobject.TYPE_STRING, gobject.TYPE_PYOBJECT)
        text_renderer = gtk.CellRendererText()
        self.toggle_renderer = gtk.CellRendererToggle()
        self.toggle_renderer.set_property('activatable', True)

        column_ui_visible = gtk.TreeViewColumn("UI", self.toggle_renderer)
        column_ui_visible.add_attribute(self.toggle_renderer, "active", 0)
        column_name = gtk.TreeViewColumn("Name", text_renderer, text=1)
        column_class = gtk.TreeViewColumn("Class", text_renderer, text=2)
        column_uri = gtk.TreeViewColumn("URI", text_renderer, text=3)

        self.synths_widget.append_column(column_ui_visible)
        self.synths_widget.append_column(column_name)
        self.synths_widget.append_column(column_class)
        self.synths_widget.append_column(column_uri)

        for synth in self.synths:
            row = False, synth.get_name(), synth.get_class_name(), synth.get_class_uri(), synth
            self.store.append(row)

        self.synths_widget.set_model(self.store)

        self.main_window.show_all()
        self.main_window.connect("destroy", gtk.main_quit)

    def __del__(self):
        print "ZynjackuHostMulti destructor called."

        self.store.clear()

        for synth in self.synths:
            synth.destruct()

        ZynjackuHost.__del__(self)

    def run(self):
        toggled_connect_id = self.toggle_renderer.connect('toggled', self.on_ui_visible_toggled, self.store)
        ZynjackuHost.run(self)
        for synth in self.synths:
            if synth.ui_win:
                synth.ui_win.disconnect(synth.ui_win.destroy_connect_id) # signal connection holds reference to synth object...
        self.toggle_renderer.disconnect(toggled_connect_id)

    def on_synth_ui_window_destroyed(self, window, synth, row):
        synth.ui_win.disconnect(synth.ui_win.destroy_connect_id) # signal connection holds reference to synth object...
        synth.ui_win = None
        row[0] = False

    def create_synth_window(self, synth, row):
        synth.ui_win = ZynjackuHost.create_synth_window(self, synth)
        synth.ui_win.destroy_connect_id = synth.ui_win.connect("destroy", self.on_synth_ui_window_destroyed, synth, row)

    def on_ui_visible_toggled(self, cell, path, model):
        if model[path][0]:
            model[path][4].ui_win.hide_all()
            model[path][4].ui_off()
            model[path][0] = False
        else:
            if not model[path][4].ui_win:
                self.create_synth_window(model[path][4], model[path])
            model[path][4].ui_on()
            model[path][4].ui_win.show_all()
            model[path][0] = True

class ZynjackuHostOne(ZynjackuHost):
    def __init__(self, glade_xml, client_name, uri):
        print "ZynjackuHostOne constructor called."
        ZynjackuHost.__init__(self, client_name)

        self.synth = zynjacku.Synth(uri=uri)
        self.synth.connect("group-added", on_group_added)
        self.synth.connect("test", on_test)
        if not self.synth.construct(self.engine):
            print"Failed to construct %s" % uri
            del(self.synth)
            self.synth = None
        else:
            self.ui_win = ZynjackuHost.create_synth_window(self, self.synth)

    def run(self):
        if (self.synth):
            self.synth.ui_on()
            self.ui_win.show_all()
            self.ui_win.connect("destroy", gtk.main_quit)

    def __del__(self):
        print "ZynjackuHostOne destructor called."

        if (self.synth):
            self.synth.destruct()

        ZynjackuHost.__del__(self)

def main():
    glade_dir = os.path.dirname(sys.argv[0])

    # since ppl tend to run "python zynjacku.py", lets assume that it is in current directory
    # "python ./zynjacku.py" and "./zynjacku.py" will work anyway.
    if not glade_dir:
        glade_dir = "."

    glade_file = glade_dir + os.sep + "zynjacku.glade"

    if not os.path.isfile(glade_file):
        glade_file = glade_dir + os.sep + ".." + os.sep + "share"+ os.sep + "zynjacku" + os.sep + "zynjacku.glade"

    #print 'Loading glade from "%s"' % glade_file

    glade_xml = gtk.glade.XML(glade_file)

    if len(sys.argv) == 2:
        host = ZynjackuHostOne(glade_xml, "zynjacku", sys.argv[1])
    else:
        host = ZynjackuHostMulti(glade_xml, "zynjacku", sys.argv[1:])
    the_host = host
    host.run()

main()
