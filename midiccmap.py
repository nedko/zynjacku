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

def on_value_changed(adj, iter):
    ls[iter][1] = str(adj.value)

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
adj_min = gtk.Adjustment(0, 0, 1, 0.01, 0.2)
value_min.set_adjustment(adj_min)
min_box = gtk.HBox()
min_box.pack_start(gtk.Label(start_value_text))
min_box.pack_start(value_min)
vbox_top_left.pack_start(min_box)

ls = gtk.ListStore(gobject.TYPE_STRING, gobject.TYPE_STRING, gtk.Adjustment, str, bool)

r = gtk.CellRendererText()
c1 = gtk.TreeViewColumn("MIDI", r, text=0)
c2 = gtk.TreeViewColumn("Parameter", r, text=1)

tv = gtk.TreeView(ls)
tv.append_column(c1)
tv.append_column(c2)
vbox_top_left.pack_start(tv)

adj_max = gtk.Adjustment(1, 0, 1, 0.01, 0.2)
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

value = calfwidgets.Knob()
value_box = gtk.HBox()
value_label = gtk.Label("Value")
value_box.pack_start(value_label)
value_box.pack_start(value)
value_frame = gtk.Frame()
value_frame.add(value_box)
hbox_bottom.pack_start(value_frame)

adj_cc_value = gtk.Adjustment(17, 0, 127, 1, 19)
cc_value = gtk.SpinButton(adj_cc_value, 0.0, 0)
cc_value_box = gtk.HBox()
cc_value_box.pack_start(gtk.Label("MIDI CC value"))
cc_value_box.pack_start(cc_value)
cc_value_change_button = gtk.Button("Change")
cc_value_box.pack_start(cc_value_change_button)
cc_value_new_button = gtk.Button("New")
cc_value_box.pack_start(cc_value_new_button)
cc_value_delete_button = gtk.Button("Remove")
cc_value_box.pack_start(cc_value_delete_button)
cc_value_frame = gtk.Frame()
cc_value_frame.add(cc_value_box)
hbox_bottom.pack_start(cc_value_frame)

value.set_adjustment(adj_min)
value_label.set_text(start_value_text)

iter = ls.append(["0", "0.0", adj_min, start_value_text, True])
adj_min.connect("value-changed", on_value_changed, iter)
iter = ls.append(["127", "1.0", adj_max, end_value_text, True])
adj_max.connect("value-changed", on_value_changed, iter)

def on_cursor_changed(tree):
    cur = tv.get_cursor()
    row = ls[cur[0]]
    value_label.set_text(row[3])
    value.set_adjustment(row[2])
    cc_value.set_value(int(row[0]))

tv.connect("cursor-changed", on_cursor_changed)

w.connect("destroy", gtk.main_quit)
w.show_all()
gtk.main()
