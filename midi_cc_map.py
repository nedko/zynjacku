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

from math import pi
import gtk
import gobject
try:
    import calfwidgets
    calfwidgets.Knob()
except:
    calfwidgets = None

class curve_widget(gtk.DrawingArea):
    def __init__(self, min_value, max_value, value):
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

        self.min_value = min_value
        self.max_value = max_value

        self.moving_point_cc = -1
        self.moving_point_value = value

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
                adj = point[1]
                #print "removed point %u -> %f (index %u)" % (cc, value, i)
                del(self.points[i])
                return adj
            i += 1

        print "point with cc value %u not found" % cc
        return None

    def change_point_cc(self, cc_value_old, cc_value_new):
        adj = self.remove_point(cc_value_old)
        self.add_point(cc_value_new, adj)
        self.invalidate_all()

    def set_moving_point_cc(self, cc_value):
        self.moving_point_cc = cc_value
        self.invalidate_all()

    def get_moving_point(self, min_value, max_value):
        if self.moving_point_cc >= 0:
            x = self.get_x(self.moving_point_cc)

            prev_point = self.points[0]
            for point in self.points:
                if point[0] == self.moving_point_cc:
                    y3 = point[1].value
                    break
                elif point[0] > self.moving_point_cc:
                    x1 = float(prev_point[0])
                    y1 = prev_point[1].value
                    x2 = float(point[0])
                    y2 = point[1].value
                    x3 = float(self.moving_point_cc)
                    y3 = y1 + ((x3 - x1) * (y2 - y1) / (x2 - x1))
                    break
                prev_point = point

            y = self.get_y(min_value, max_value, y3)

            return x, y

        y = self.get_y(min_value, max_value, self.moving_point_value)

        #print "searching for value %f" % self.moving_point_value
        prev_point = self.points[0]
        for point in self.points:
            #print "%f ? %f ? %f" % (prev_point[1].value, self.moving_point_value, point[1].value)
            if point[1].value == self.moving_point_value:
                x = self.get_x(float(point[0]))
                return x, y
            elif ((prev_point[1].value < self.moving_point_value and self.moving_point_value < point[1].value) or
                  (prev_point[1].value > self.moving_point_value and self.moving_point_value > point[1].value)):
                x1 = float(prev_point[0])
                y1 = prev_point[1].value
                x2 = float(point[0])
                y2 = point[1].value
                y3 = self.moving_point_value
                x3 = x1 + ((y3 - y1) * (x2 - x1) / (y2 - y1))
                #print "x = %f" % x3
                x = self.get_x(x3)
                return x, y
            prev_point = point
        return None

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

    def get_x(self, cc_value):
        x = self.margin + cc_value / 127.0 * (self.width - 2 * self.margin)
        #print "x(%f) -> %u" % (cc_value, x)
        return x

    def get_y(self, min_value, max_value, value):
        if max_value == min_value:
            y = self.height / 2.0
        else:
            v = 1.0 - (value - min_value) / (max_value - min_value)
            y = self.margin + v * (self.height - 2 * self.margin)

        #print "y(%f, [%f, %f]) -> %u" % (value, min_value, max_value, y)
        return y

    def draw(self, cairo_ctx):
        cairo_ctx.set_source_color(self.color_bg)
        cairo_ctx.rectangle(0, 0, self.width, self.height)
        cairo_ctx.fill()

        #cairo_ctx.set_source_color(self.color_mark)
        #cairo_ctx.rectangle(self.margin, self.margin, self.width - 2 * self.margin, self.height - 2 * self.margin)
        #cairo_ctx.stroke()

        if not self.points:
            return

        if self.full_range:
            min_value = self.min_value
            max_value = self.max_value
        else:
            max_value = min_value = self.points[0][1].value
            for point in self.points[1:]:
                if point[1].value > max_value:
                    max_value = point[1].value
                elif point[1].value < min_value:
                    min_value = point[1].value

        if min_value <= 0 and max_value >= 0:
            cairo_ctx.set_source_color(self.color_mark)
            cairo_ctx.set_line_width(1);

            y = self.get_y(min_value, max_value, 0)
            x = self.get_x(0)
            cairo_ctx.move_to(x, y)
            x = self.get_x(127)
            cairo_ctx.line_to(x, y)

            cairo_ctx.stroke()

        cairo_ctx.set_source_color(self.color_value)
        cairo_ctx.set_line_width(1);

        prev_point = False
        for point in self.points:
            x = self.get_x(point[0])
            y = self.get_y(min_value, max_value, point[1].value)
            #print x, y
            if not prev_point:
                cairo_ctx.move_to(x, y)
            else:
                cairo_ctx.line_to(x, y)
            prev_point = True

        cairo_ctx.stroke()

        pt = self.get_moving_point(min_value, max_value)
        if pt:
            cairo_ctx.arc(pt[0], pt[1], 3, 0, 2 * pi)
            cairo_ctx.fill()

class midiccmap:
    def __init__(self, map, parameter_name, cc_no, min_value=0.0, max_value=1.0, value=0.5):
        self.parameter_name = parameter_name
        self.min_value = min_value
        self.max_value = max_value

        self.map = map
        self.map.connect("point-created", self.on_point_created)
        self.map.connect("point-removed", self.on_point_removed)
        self.map.connect("point-cc-changed", self.on_point_cc_changed)
        self.map.connect("point-value-changed", self.on_point_value_changed)
        self.map.connect("cc-no-assigned", self.on_cc_no_assigned)
        self.map.connect("cc-value-changed", self.on_cc_map_value_changed)

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

        self.curve = curve_widget(min_value, max_value, value)
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

        if calfwidgets:
            self.value_knob = calfwidgets.Knob()
            hbox_bottom.pack_start(self.value_knob, False)
        else:
            self.value_knob = None

        self.value_spin = gtk.SpinButton(digits=2)
        hbox_bottom.pack_start(self.value_spin, False)

        self.initial_points = True
        self.points = []
        self.map.get_points()
        self.initial_points = False

        self.tv.get_selection().connect("changed", self.on_selection_changed)

        self.tv.get_selection().select_path((0,))

        self.set_title()

    def on_point_created(self, map, cc_value, parameter_value):
        #print "on_point_created(%u, %f)" % (cc_value, parameter_value)

        prev_iter = None

        for row in self.ls:
            if int(row[0]) > cc_value:
                break
            prev_iter = row.iter
        
        adj = gtk.Adjustment(parameter_value, self.min_value, self.max_value, 0.01, 0.2)
        self.curve.add_point(cc_value, adj)
        iter = self.ls.insert_after(prev_iter, [str(cc_value), "", adj, cc_value == 0 or cc_value == 127, "->"])
        adj.connect("value-changed", self.on_value_change_request)
        self.on_value_changed(iter, parameter_value)

        if self.initial_points:
            self.points.append([cc_value, parameter_value])

        self.tv.get_selection().select_iter(iter)
        self.curve.invalidate_all()

    def on_point_removed(self, map, cc_value):
        #print "on_point_removed(%u)" % cc_value

        selection = self.tv.get_selection()

        prev_iter = None
        row = None
        for row in self.ls:
            if int(row[0]) == cc_value:
                break
            prev_iter = row.iter

        if not row or row.iter == prev_iter:
            print "cannot find point to remove. cc value is %u" % cc_value
            return

        if row.iter != selection.get_selected()[1]:
            path = self.ls.get_path(self.current_row)
        else:
            path = None

        self.ls.remove(row.iter)

        if path:
            selection.select_path(path)

        self.curve.remove_point(cc_value)
        self.curve.invalidate_all()

    def on_point_cc_changed(self, map, cc_value_old, cc_value_new):
        #print "on_point_cc_changed(%u, %u)" % (cc_value_old, cc_value_new)
        self.curve.change_point_cc(cc_value_old, cc_value_new)
        for row in self.ls:
            if int(row[0]) == cc_value_old:
                row[0] = cc_value_new

    def on_point_value_changed(self, map, cc_value, parameter_value):
        #print "on_point_value_changed(%u, %f)" % (cc_value, parameter_value)
        for row in self.ls:
            if int(row[0]) == cc_value:
                self.on_value_changed(row.iter, parameter_value)

    def on_cc_no_assigned(self, map, cc_no):
        #print "on_cc_no_assigned(%u)" % cc_no
        self.adj_cc_no.value = cc_no

    def on_cc_map_value_changed(self, map, cc_value):
        #print "on_cc_map_value_changed(%u)" % cc_value
        self.adj_cc_value.value = cc_value
        self.curve.set_moving_point_cc(cc_value)

    def revert_points(self):
        path = self.ls.get_path(self.current_row)
        for row in self.ls:
            self.map.point_remove(int(row[0]))
        
        for point in self.points:
            self.map.point_create(point[0], point[1])
        if path[0] >= len(self.ls):
            path = (len(self.ls) - 1,)
        self.tv.get_selection().select_path(path)

    def run(self):
        self.window.show_all()
        while True:
            ret = self.window.run()
            if ret == gtk.RESPONSE_NONE: # revert/undo button pressed?
                self.revert_points()
                continue
            elif ret == gtk.RESPONSE_REJECT: # cancel button pressed?
                self.revert_points()
                ret = False
            else:
                ret = True

            self.window.hide_all()
            return ret

    def set_title(self):
        self.window.set_title('Parameter "%s" MIDI CC #%u map' % (self.parameter_name, int(self.adj_cc_no.value)))

    def on_set_full_range(self, button, full_range):
        self.curve.set_full_range(full_range)
        self.curve.invalidate_all()

    def on_cc_no_changed(self, adj):
        self.set_title()

    def on_value_change_request(self, adj):
        #print "on_value_change_request() called. value = %f" % adj.value

        for row in self.ls:
            if row[2] == adj:
                #print "found"
                self.map.point_parameter_value_change(int(row[0]), adj.value)
                return

        #print "not found"

    def on_value_changed(self, iter, value):
        self.ls[iter][1] = "%.2f" % value
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

        if self.value_knob:
            self.value_knob.set_adjustment(row[2])
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

    def on_button_clicked(self, button):
        if button == self.cc_value_change_button:
            #print "change cc value"
            self.map.point_cc_value_change(int(self.ls[self.current_row][0]), int(self.adj_cc_value.value))
        elif button == self.cc_value_new_button:
            #print "new cc value"
            self.map.point_create(int(self.adj_cc_value.value), self.ls[self.current_row][2].value)
        elif button == self.cc_value_delete_button:
            #print "delete cc value"
            self.map.point_remove(int(self.ls[self.current_row][0]))

if __name__ == '__main__':
    import zynjacku_c

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

    map = zynjacku_c.MidiCcMap()

    for point in values_flat:
        map.point_create(point[0], point[1])

    midiccmap(map, "Modulation", 23, 0, 1, 0.5).run()
