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

class curve_widget(gtk.DrawingArea):
    def __init__(self):
        gtk.DrawingArea.__init__(self)

        self.connect("expose-event", self.on_expose)
        self.connect("size-request", self.on_size_request)
        self.connect("size_allocate", self.on_size_allocate)

        self.color_bg = gtk.gdk.Color(0,0,0)
        self.color_value = gtk.gdk.Color(int(65535 * 0.8), int(65535 * 0.7), 0)
        self.color_mark = gtk.gdk.Color(int(65535 * 0.2), int(65535 * 0.2), int(65535 * 0.2))
        self.width = 0
        self.height = 0
        self.margin = 5

        self.points = []
        self.full_range = False

    def add_point(self, cc, adj):
        i = 0
        for point in self.points:
            if point[0] > cc:
                break
            i += 1
        self.points.insert(i, [cc, adj])

    def set_full_range(self, full_range = True):
        self.full_range = full_range
        self.invalidate_all()

    def remove_point(self, cc):
        #print "removing point with cc value %u" % cc
        i = 0
        for point in self.points:
            #print "%u ?= %u" % (cc, point[0])
            if point[0] == cc:
                #print "removed point %u -> %f (index %u)" % (cc, point[1].value, i)
                del(self.points[i])
                return
            i += 1

        print "point with cc value %u not found" % cc

    def on_expose(self, widget, event):
        cairo_ctx = widget.window.cairo_create()

        # set a clip region for the expose event
        cairo_ctx.rectangle(event.area.x, event.area.y, event.area.width, event.area.height)
        cairo_ctx.clip()

        self.draw(cairo_ctx)

        return False

    def on_size_allocate(self, widget, allocation):
        #print allocation.x, allocation.y, allocation.width, allocation.height
        self.width = float(allocation.width)
        self.height = float(allocation.height)
        self.font_size = 10

    def on_size_request(self, widget, requisition):
        #print "size-request, %u x %u" % (requisition.width, requisition.height)
        requisition.width = 150
        requisition.height = 150
        return

    def invalidate_all(self):
        self.queue_draw_area(0, 0, int(self.width), int(self.height))

    def draw(self, cairo_ctx):
        cairo_ctx.set_source_color(self.color_bg)
        cairo_ctx.rectangle(0, 0, self.width, self.height)
        cairo_ctx.fill()

        #cairo_ctx.set_source_color(self.color_mark)
        #cairo_ctx.rectangle(self.margin, self.margin, self.width - 2 * self.margin, self.height - 2 * self.margin)
        #cairo_ctx.stroke()

        if not self.points:
            return

        cairo_ctx.set_source_color(self.color_value)

        if self.full_range:
            min_value = 0.0
            max_value = 1.0
        else:
            max_value = min_value = self.points[0][1].value
            for point in self.points[1:]:
                if point[1].value > max_value:
                    max_value = point[1].value
                elif point[1].value < min_value:
                    min_value = point[1].value

        prev_point = False
        for point in self.points:
            x = int(self.margin + float(point[0]) / 127 * (self.width - 2 * self.margin))
            value = float(point[1].value)
            value -= min_value
            value = value / (max_value - min_value)
            y = int(self.margin + (1.0 - value) * (self.height - 2 * self.margin))
            #print x, y
            if not prev_point:
                cairo_ctx.move_to(x, y)
            else:
                cairo_ctx.line_to(x, y)
            prev_point = True

        cairo_ctx.stroke()

class midiccmap:
    def __init__(self, parameter_name, cc_no, min_value=0.0, max_value=1.0, points=[[0, 0], [127, 1]]):
        self.parameter_name = parameter_name
        self.min_value = min_value
        self.max_value = max_value
        self.points = points
        self.window = gtk.Dialog(
            flags=gtk.DIALOG_MODAL | gtk.DIALOG_DESTROY_WITH_PARENT,
            buttons=(gtk.STOCK_UNDO, gtk.RESPONSE_NONE, gtk.STOCK_CANCEL, gtk.RESPONSE_REJECT, gtk.STOCK_OK, gtk.RESPONSE_ACCEPT))

        # the grand vbox
        vbox = self.window.vbox
        #vbox.set_spacing(5)
        #self.window.add(vbox)

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
        button.connect("toggled", self.on_set_full_range, False)
        hbox_top.pack_start(button, False)
        button = gtk.RadioButton(button, "full")
        button.connect("toggled", self.on_set_full_range, True)
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

        self.curve = curve_widget()
        self.curve.set_size_request(250,250)
        frame = gtk.Frame()
        frame.set_shadow_type(gtk.SHADOW_ETCHED_OUT)
        frame.add(self.curve)
        hbox_middle.pack_start(frame, True, True)

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

        self.add_points()

        self.tv.get_selection().connect("changed", self.on_selection_changed)

        self.tv.get_selection().select_path((0,))

        self.set_title()

    def add_points(self):
        self.new_point(self.points[0][0], self.points[0][1], True)
        for point in self.points[1:-1]:
            self.new_point(point[0], point[1])
        self.new_point(self.points[-1][0], self.points[-1][1], True)

    def run(self):
        self.window.show_all()
        while True:
            ret = self.window.run()
            if ret == gtk.RESPONSE_NONE: # revert/undo button pressed?
                path = self.ls.get_path(self.current_row)
                self.ls.clear()
                self.add_points()
                if path[0] >= len(self.ls):
                    path = (len(self.ls) - 1,)
                self.tv.get_selection().select_path(path)
                continue

            self.window.hide_all()
            return ret

    def set_title(self):
        self.window.set_title('Parameter "%s" MIDI CC #%u map' % (self.parameter_name, int(self.adj_cc_no.value)))

    def on_set_full_range(self, button, full_range):
        self.curve.set_full_range(full_range)
        self.curve.invalidate_all()

    def on_cc_no_changed(self, adj):
        self.set_title()

    def on_value_changed(self, adj, iter):
        self.ls[iter][1] = "%.2f" % self.ls[iter][2].value
        self.curve.invalidate_all()

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
        self.curve.add_point(cc_value, adj)
        iter = self.ls.insert_after(prev_iter, [str(cc_value), "", adj, immutable, "->"])
        adj.connect("value-changed", self.on_value_changed, iter)
        self.on_value_changed(None, iter)
        return iter

    def on_button_clicked(self, button):
        if button == self.cc_value_change_button:
            #print "change cc value"
            adj = self.ls[self.current_row][2]
            cc = int(self.ls[self.current_row][0])
            self.ls.remove(self.tv.get_selection().get_selected()[1])
            iter = self.new_point(int(self.adj_cc_value.value), adj.value)
            self.tv.get_selection().select_iter(iter)
            self.curve.remove_point(cc)
            self.curve.invalidate_all()
        elif button == self.cc_value_new_button:
            #print "new cc value"
            iter = self.new_point(int(self.adj_cc_value.value), self.ls[self.current_row][2].value)
            self.tv.get_selection().select_iter(iter)
            self.curve.invalidate_all()
        elif button == self.cc_value_delete_button:
            #print "delete cc value"
            selection = self.tv.get_selection()
            cc = int(self.ls[self.current_row][0])
            path = self.ls.get_path(self.current_row)
            self.ls.remove(selection.get_selected()[1])
            selection.select_path(path)
            self.curve.remove_point(cc)
            self.curve.invalidate_all()

values_big = [
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

values_flat = [
    [0, 0.40],
    [5, 0.42],
    [56, 0.37],
    [89, 0.60],
    [127, 0.65]
    ]

midiccmap("Modulation", 23, 0, 1, values_flat).run()
