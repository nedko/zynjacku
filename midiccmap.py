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
    def __init__(self, parameter_name, cc_no, min_value=0.0, max_value=1.0, points=[]):
        self.parameter_name = parameter_name
        self.min_value = min_value
        self.max_value = max_value
        self.window = gtk.Window(gtk.WINDOW_TOPLEVEL)

        # the grand vbox
        vbox = gtk.VBox()
        vbox.set_spacing(5)
        self.window.add(vbox)

        # top hbox
        hbox_top = gtk.HBox()
        hbox_top.set_spacing(10)
        align = gtk.Alignment(0.5, 0.5, 1.0, 1.0)
        align.set_padding(10, 0, 10, 10)
        align.add(hbox_top)
        vbox.pack_start(align, False)

        self.adj_cc_no = gtk.Adjustment(cc_no, 0, 127, 1, 19)
        self.adj_cc_no.connect("value-changed", self.on_cc_no_changed)
        cc_no = gtk.SpinButton(self.adj_cc_no, 0.0, 0)
        cc_no_box = gtk.HBox()
        cc_no_box.pack_start(gtk.Label("MIDI CC #"))
        cc_no_box.pack_start(cc_no)
        hbox_top.pack_start(cc_no_box, False)

        hbox_top.pack_start(gtk.Label("Show parameter range:"), False)
        button = gtk.RadioButton(None, "mapped")
        hbox_top.pack_start(button, False)
        button = gtk.RadioButton(button, "full")
        hbox_top.pack_start(button, False)

        # middle hbox
        hbox_middle = gtk.HBox()
        hbox_middle.set_spacing(10)
        align = gtk.Alignment(0.5, 0.5, 1.0, 1.0)
        align.set_padding(0, 0, 10, 10)
        align.add(hbox_middle)
        vbox.pack_start(align)

        vbox_left = gtk.VBox()
        vbox_left.set_spacing(5)
        hbox_middle.pack_start(vbox_left, False)

        self.ls = gtk.ListStore(gobject.TYPE_STRING, gobject.TYPE_STRING, gtk.Adjustment, bool, str)

        r = gtk.CellRendererText()
        c1 = gtk.TreeViewColumn("From", r, text=0)
        c2 = gtk.TreeViewColumn("->", r, text=4)
        c3 = gtk.TreeViewColumn("To", r, text=1)

        self.tv = gtk.TreeView(self.ls)
        self.tv.set_headers_visible(False)
        self.tv.append_column(c1)
        self.tv.append_column(c2)
        self.tv.append_column(c3)
        tv_box = gtk.VBox()

        align = gtk.Alignment(0.5, 0.5, 1.0, 1.0)
        align.set_padding(3, 3, 10, 10)
        align.add(gtk.Label("Control points"))
        tv_box.pack_start(align, False)

        sw = gtk.ScrolledWindow()
        sw.set_policy(gtk.POLICY_AUTOMATIC, gtk.POLICY_AUTOMATIC)
        sw.add(self.tv)
        tv_box.pack_start(sw)

        vbox_left.pack_start(tv_box, True, True)

        self.cc_value_delete_button = gtk.Button("Remove")
        self.cc_value_delete_button.connect("clicked", self.on_button_clicked)
        vbox_left.pack_start(self.cc_value_delete_button, False)

        self.cc_value_new_button = gtk.Button("New")
        self.cc_value_new_button.connect("clicked", self.on_button_clicked)
        vbox_left.pack_start(self.cc_value_new_button, False)

        self.cc_value_change_button = gtk.Button("Change CC value")
        self.cc_value_change_button.connect("clicked", self.on_button_clicked)
        vbox_left.pack_start(self.cc_value_change_button, False)

        curve = gtk.Frame("Here will be the curve widget..................................")
        curve.set_size_request(250,250)
        hbox_middle.pack_start(curve, True, True)

        # bottom hbox
        hbox_bottom = gtk.HBox()
        hbox_bottom.set_spacing(10)
        align = gtk.Alignment(0.5, 0.5, 1.0, 1.0)
        align.set_padding(0, 2, 10, 10)
        align.add(hbox_bottom)
        vbox.pack_start(align, False)

        self.adj_cc_value = gtk.Adjustment(17, 0, 127, 1, 10)
        self.adj_cc_value.connect("value-changed", self.on_cc_value_changed)

        self.cc_value = gtk.SpinButton(None, 0.0, 0)
        self.cc_value.set_adjustment(self.adj_cc_value)
        label = gtk.Label("CC value")
        hbox_bottom.pack_start(label, False)
        hbox_bottom.pack_start(self.cc_value, False)

        hbox_bottom.pack_start(gtk.Label("-> %s" % self.parameter_name), False)

        self.value = calfwidgets.Knob()
        hbox_bottom.pack_start(self.value, False)

        self.value_spin = gtk.SpinButton(digits=2)
        hbox_bottom.pack_start(self.value_spin, False)

        # add points
        self.new_point(points[0][0], points[0][1], True)
        for point in points[1:-1]:
            self.new_point(point[0], point[1])
        self.new_point(points[-1][0], points[-1][1], True)

        self.tv.get_selection().connect("changed", self.on_selection_changed)

        self.tv.get_selection().select_path((0,))

        self.set_title()

        self.window.connect("destroy", gtk.main_quit)
        self.window.show_all()

    def set_title(self):
        self.window.set_title('Parameter "%s" MIDI CC #%u map' % (self.parameter_name, int(self.adj_cc_no.value)))

    def on_cc_no_changed(self, adj):
        self.set_title()

    def on_value_changed(self, adj, iter):
        self.ls[iter][1] = "%.2f" % self.ls[iter][2].value

    def on_selection_changed(self, obj):
        iter = self.tv.get_selection().get_selected()[1]
        self.current_row = iter
        if not iter:
            #print "selection gone"
            return
        row = self.ls[iter]
        #print "selected %s" % row[0]

        # is immutable?
        immutable = row[3]
        self.cc_value_delete_button.set_sensitive(not immutable)
        self.current_immutable = immutable

        self.value.set_adjustment(row[2])
        self.value_spin.set_adjustment(row[2])
        self.value_spin.set_value(row[2].value)
        self.cc_value.set_value(int(row[0]))

        self.on_cc_value_changed(row[2])

    def on_cc_value_changed(self, adj):
        for row in self.ls:
            #print "%s ?= %s" % (row[0], int(adj.value))
            if int(row[0]) == int(adj.value):
                self.cc_value_new_button.set_sensitive(False)
                self.cc_value_change_button.set_sensitive(False)
                return
        self.cc_value_new_button.set_sensitive(True)
        self.cc_value_change_button.set_sensitive(not self.current_immutable)

    def new_point(self, cc_value, value, immutable=False):
        #print "new point %u" % cc_value
        prev_iter = None

        for row in self.ls:
            if int(row[0]) > cc_value:
                break
            prev_iter = row.iter
        
        adj = gtk.Adjustment(value, self.min_value, self.max_value, 0.01, 0.2)
        iter = self.ls.insert_after(prev_iter, [str(cc_value), "", adj, immutable, "->"])
        adj.connect("value-changed", self.on_value_changed, iter)
        self.on_value_changed(None, iter)
        return iter

    def on_button_clicked(self, button):
        if button == self.cc_value_change_button:
            #print "change cc value"
            adj = self.ls[self.current_row][2]
            self.ls.remove(self.tv.get_selection().get_selected()[1])
            iter = self.new_point(int(self.adj_cc_value.value), adj.value)
            self.tv.get_selection().select_iter(iter)
        elif button == self.cc_value_new_button:
            #print "new cc value"
            iter = self.new_point(int(self.adj_cc_value.value), self.ls[self.current_row][2].value)
            self.tv.get_selection().select_iter(iter)
        elif button == self.cc_value_delete_button:
            #print "delete cc value"
            selection = self.tv.get_selection()
            path = self.ls.get_path(self.current_row)
            self.ls.remove(selection.get_selected()[1])
            selection.select_path(path)

values = [
    [0, 0.1],
    [1, 0.12],
    [2, 0.13],
    [3, 0.14],
    [4, 0.18],
    [5, 0.2],
    [56, 0.71],
    [89, 0.80],
    [127, 0.95]
    ]
m = midiccmap("Modulation", 23, 0, 1, values)
gtk.main()
