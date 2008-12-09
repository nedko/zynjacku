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

import gtk
import gobject
import calfwidgets

class midiccmap:
    def __init__(self, min_value=0.1, max_value=1.0):
        w = gtk.Window(gtk.WINDOW_TOPLEVEL)
        #w.set_size_request(300,300)
        w.set_title("Parameter MIDI CC map")

        vbox = gtk.VBox()
        w.add(vbox)

        hbox_top = gtk.HBox()
        vbox.pack_start(hbox_top)

        vbox_top_left = gtk.VBox()
        hbox_top.pack_start(vbox_top_left)

        curve = gtk.Frame("Here will be the curve widget..................................")
        hbox_top.pack_start(curve)

        start_value_text = "Start value"
        end_value_text = "End value"

        value_min = calfwidgets.Knob()
        adj_min = gtk.Adjustment(min_value, 0, 1, 0.01, 0.2)
        value_min.set_adjustment(adj_min)
        min_box = gtk.HBox()
        min_box.pack_start(gtk.Label(start_value_text))
        min_box.pack_start(value_min)
        vbox_top_left.pack_start(min_box)

        self.ls = gtk.ListStore(gobject.TYPE_STRING, gobject.TYPE_STRING, gtk.Adjustment, str, bool)

        r = gtk.CellRendererText()
        c1 = gtk.TreeViewColumn("MIDI", r, text=0)
        c2 = gtk.TreeViewColumn("Parameter", r, text=1)

        self.tv = gtk.TreeView(self.ls)
        self.tv.append_column(c1)
        self.tv.append_column(c2)
        vbox_top_left.pack_start(self.tv)

        adj_max = gtk.Adjustment(max_value, 0, 1, 0.01, 0.2)
        value_max = calfwidgets.Knob()
        value_max.set_adjustment(adj_max)
        max_box = gtk.HBox()
        max_box.pack_start(gtk.Label(end_value_text))
        max_box.pack_start(value_max)
        vbox_top_left.pack_start(max_box)

        hbox_bottom = gtk.HBox()
        vbox.pack_start(hbox_bottom)

        adj_cc = gtk.Adjustment(70, 0, 127, 1, 19)
        cc = gtk.SpinButton(adj_cc, 0.0, 0)
        cc_box = gtk.HBox()
        cc_box.pack_start(gtk.Label("MIDI CC"))
        cc_box.pack_start(cc)
        cc_frame = gtk.Frame()
        cc_frame.add(cc_box)
        hbox_bottom.pack_start(cc_frame)

        self.value = calfwidgets.Knob()
        value_box = gtk.HBox()
        self.value_label = gtk.Label()
        value_box.pack_start(self.value_label)
        value_box.pack_start(self.value)
        value_frame = gtk.Frame()
        value_frame.add(value_box)
        hbox_bottom.pack_start(value_frame)

        adj_cc_value = gtk.Adjustment(17, 0, 127, 1, 19)
        self.cc_value = gtk.SpinButton(adj_cc_value, 0.0, 0)
        cc_value_box = gtk.HBox()
        cc_value_box.pack_start(gtk.Label("MIDI CC value"))
        cc_value_box.pack_start(self.cc_value)
        cc_value_change_button = gtk.Button("Change")
        cc_value_box.pack_start(cc_value_change_button)
        cc_value_new_button = gtk.Button("New")
        cc_value_box.pack_start(cc_value_new_button)
        cc_value_delete_button = gtk.Button("Remove")
        cc_value_box.pack_start(cc_value_delete_button)
        cc_value_frame = gtk.Frame()
        cc_value_frame.add(cc_value_box)
        hbox_bottom.pack_start(cc_value_frame)

        iter = self.ls.append(["0", "", adj_min, start_value_text, True])
        adj_min.connect("value-changed", self.on_value_changed, iter)
        self.on_value_changed(None, iter)

        iter = self.ls.append(["127", "", adj_max, end_value_text, True])
        adj_max.connect("value-changed", self.on_value_changed, iter)
        self.on_value_changed(None, iter)

        self.tv.connect("cursor-changed", self.on_cursor_changed)

        self.tv.set_cursor((0,))

        w.connect("destroy", gtk.main_quit)
        w.show_all()

    def on_value_changed(self, adj, iter):
        self.ls[iter][1] = str(self.ls[iter][2].value)

    def on_cursor_changed(self, tree):
        cur = self.tv.get_cursor()
        row = self.ls[cur[0]]
        self.value_label.set_text(row[3])
        self.value.set_adjustment(row[2])
        self.cc_value.set_value(int(row[0]))


m = midiccmap(0.23, 0.78)
gtk.main()
