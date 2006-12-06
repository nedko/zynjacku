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
#import gc

def on_group_added(obj, group_name, context):
    print "on_group_added() called !!!!!!!!!!!!!!!!!!"
    #print repr(obj)
    #print repr(group_name)
    #print repr(context)
    #print repr(engine)
    return obj

def on_test(obj1, obj2):
    print "on_test() called !!!!!!!!!!!!!!!!!!"
    print repr(obj2)

def on_synth_ui_window_destroyed(window, synth, row):
    synth.ui_win.disconnect(synth.ui_win.destroy_connect_id) # signal connection holds reference to synth object...
    synth.ui_win = None
    row[0] = False

def create_synth_window(synth, row):
    synth.ui_win = gtk.Window(gtk.WINDOW_TOPLEVEL)
    synth.ui_win.destroy_connect_id = synth.ui_win.connect("destroy", on_synth_ui_window_destroyed, synth, row)
    synth.ui_win.set_title(synth.get_name())
    synth.ui_win.set_role("zynjacku_synth_ui")

def on_ui_visible_toggled(cell, path, model):
    if model[path][0]:
        model[path][4].ui_win.hide_all()
        model[path][4].ui_off()
        model[path][0] = False
    else:
        if not model[path][4].ui_win:
            create_synth_window(model[path][4], model[path])
        model[path][4].ui_on()
        model[path][4].ui_win.show_all()
        model[path][0] = True

class ZynjackuHost:
    def __init__(self, client_name):
        print "ZynjackuHost constructor called."
        self.engine = zynjacku.Engine()

        if not self.engine.start_jack(client_name):
            print "Failed to initialize zynjacku engine"
            sys.exit(1)

    def __del__(self):
        print "ZynjackuHost destructor called."

    def ui_run(self):
        self.engine.ui_run()
        return True

    def run(self):
        ui_run_callback_id = gobject.timeout_add(100, self.ui_run)
        gtk.main()
        gobject.source_remove(ui_run_callback_id)

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
            #print "signal registration..."
            synth.connect("group-added", on_group_added)
            #synth.connect("test", on_test)
            if not synth.construct(self.engine):
                print"Failed to construct %s" % uri
            else:
                self.synths.append(synth)
                synth.ui_win = None
            #del(synth)

        self.synths_widget = glade_xml.get_widget("treeview_synths")

        self.store = gtk.ListStore(gobject.TYPE_BOOLEAN, gobject.TYPE_STRING, gobject.TYPE_STRING, gobject.TYPE_STRING, gobject.TYPE_PYOBJECT)
        text_renderer = gtk.CellRendererText()
        toggle_renderer = gtk.CellRendererToggle()
        toggle_renderer.set_property('activatable', True)
        toggle_renderer.connect('toggled', on_ui_visible_toggled, self.store)

        column_ui_visible = gtk.TreeViewColumn("UI", toggle_renderer)
        column_ui_visible.add_attribute(toggle_renderer, "active", 0)
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
            #del(row)
            #del(synth)

        self.synths_widget.set_model(self.store)

        self.main_window.show_all()
        self.main_window.connect("destroy", gtk.main_quit)

    def __del__(self):
        print "ZynjackuHostMulti destructor called."

        ZynjackuHost.__del__(self)

        self.store.clear()

        for synth in self.synths:
            if synth.ui_win:
                synth.ui_win.disconnect(synth.ui_win.destroy_connect_id) # signal connection holds reference to synth object...
            synth.destruct()
            #del(synth)

        #del(self.synths)

        self.engine.stop_jack()

        #del(self.engine)

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

    host = ZynjackuHostMulti(glade_xml, "zynjacku", sys.argv[1:])
    host.run()
    #del(host)

# class test:
#     def __init__(self):
#         print "test constructor called."

#     def __del__(self):
#         print "test destructor called."

# test = test()

main()
#print gc.get_count()
#gc.collect()
