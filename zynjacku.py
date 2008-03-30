#!/usr/bin/env python
#
# This file is part of zynjacku
#
# Copyright (C) 2006,2007,2008 Nedko Arnaudov <nedko@arnaudov.name>
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
import zynjacku_c as zynjacku
sys.path = old_path

hint_uris = { "hidden": "http://home.gna.org/zynjacku/hints#hidden",
              "togglefloat": "http://home.gna.org/zynjacku/hints#togglefloat",
              "onesubgroup": "http://home.gna.org/zynjacku/hints#onesubgroup",
              }

class midi_led(gtk.EventBox):
    def __init__(self):
        gtk.EventBox.__init__(self)
        self.label = gtk.Label()
        #attrs = pango.AttrList()
        #font_attr =  pango.AttrFamily("monospace")
        #attrs.insert(font_attr)
        #self.label.set_attributes(attrs)
        self.add(self.label)

    def set(self, active):
        if active:
            self.modify_bg(gtk.STATE_NORMAL, gtk.gdk.Color(0, int(65535 * 0.5), 0))
        else:
            self.modify_bg(gtk.STATE_NORMAL, self.label.style.bg[gtk.STATE_NORMAL])

        self.label.set_text(" MIDI ")

# Synth window abstraction
class SynthWindow(gobject.GObject):

    __gsignals__ = {
        'destroy':                      # signal name
        (
            gobject.SIGNAL_RUN_LAST,    # signal flags, when class closure is invoked
            gobject.TYPE_NONE,          # return type
            ()                          # parameter types
        )}

    def __init__(self, synth):
        gobject.GObject.__init__(self)
        self.synth = synth

    def show(self):
        '''Show synth window'''

    def hide(self):
        '''Hide synth window'''

class SynthWindowCustom(SynthWindow):
    def __init__(self, synth):
        SynthWindow.__init__(self, synth)

        self.synth.connect("custom-gui-off", self.on_window_destroy)

    def on_window_destroy(self, synth):
        #print "Custom GUI window destroy detected"
        self.emit('destroy')

    def show(self):
        '''Show synth window'''
        return self.synth.ui_on()

    def hide(self):
        '''Hide synth window'''

        self.synth.ui_off()

class SynthWindowUniversalGroup(gobject.GObject):
    def __init__(self, window, parent_group, name, hints, context):
        gobject.GObject.__init__(self)
        self.window = window
        self.parent_group = parent_group
        self.group_name = name
        self.context = context
        self.hints = hints

    def child_param_add(self, obj):
        return

    def child_param_remove(self, obj):
        return

    def get_top_widget(self):
        return None

    def child_group_add(self, obj):
        return

    def child_group_remove(self, obj):
        return

    def on_child_group_appeared(self, group_name, hints, context):
        if hints.has_key(hint_uris['togglefloat']):
            group = SynthWindowUniversalGroupToggleFloat(self.window, self, group_name, hints, context)
            self.window.defered.append(group)
            return group
        elif hints.has_key(hint_uris['onesubgroup']):
            group = SynthWindowUniversalGroupOneSubGroup(self.window, self, group_name, hints, context)
            self.window.defered.append(group)
            return group
        else:
            return SynthWindowUniversalGroupGeneric(self.window, self, group_name, hints, context)

class SynthWindowUniversalParameter(gobject.GObject):
    def __init__(self, window, parent_group, name, context):
        gobject.GObject.__init__(self)
        self.window = window
        self.parent_group = parent_group
        self.parameter_name = name
        self.context = context

    def get_top_widget(self):
        return None

    def remove(self):
        self.parent_group.child_param_remove(self)

# Generic/Universal window UI, as opposed to custom UI privided by synth itself
class SynthWindowUniversal(SynthWindow):

    # enum for layout types
    layout_type_vertical = 0
    layout_type_horizontal = 1

    def __init__(self, synth, group_shadow_type, layout_type):
        SynthWindow.__init__(self, synth)

        self.group_shadow_type = group_shadow_type
        self.layout_type = layout_type

        self.window = gtk.Window(gtk.WINDOW_TOPLEVEL)
        self.window.set_size_request(600,300)
        self.window.set_title(synth.get_instance_name())
        self.window.set_role("zynjacku_synth_ui")

        self.window.connect("destroy", self.on_window_destroy)

        self.ui_enabled = False
        self.defered = []

    def on_window_destroy(self, window):
        if self.ui_enabled:
            self.hide()
            self.synth.ui_off()
            self.synth.disconnect(self.enum_disappeared_connect_id)
            self.synth.disconnect(self.enum_appeared_connect_id)
            self.synth.disconnect(self.float_disappeared_connect_id)
            self.synth.disconnect(self.float_appeared_connect_id)
            self.synth.disconnect(self.bool_disappeared_connect_id)
            self.synth.disconnect(self.bool_appeared_connect_id)
            self.synth.disconnect(self.group_disappeared_connect_id)
            self.synth.disconnect(self.group_appeared_connect_id)
            self.synth.disconnect(self.int_disappeared_connect_id)
            self.synth.disconnect(self.int_appeared_connect_id)
        else:
            self.synth.ui_off()

        self.emit('destroy')

    def show(self):
        '''Show synth window'''

        if not self.ui_enabled:
            self.group_appeared_connect_id = self.synth.connect("group-appeared", self.on_group_appeared)
            self.group_disappeared_connect_id = self.synth.connect("group-disappeared", self.on_group_disappeared)
            self.bool_appeared_connect_id = self.synth.connect("bool-appeared", self.on_bool_appeared)
            self.bool_disappeared_connect_id = self.synth.connect("bool-disappeared", self.on_bool_disappeared)
            self.float_appeared_connect_id = self.synth.connect("float-appeared", self.on_float_appeared)
            self.float_disappeared_connect_id = self.synth.connect("float-disappeared", self.on_float_disappeared)
            self.enum_appeared_connect_id = self.synth.connect("enum-appeared", self.on_enum_appeared)
            self.enum_disappeared_connect_id = self.synth.connect("enum-disappeared", self.on_enum_disappeared)
            self.int_appeared_connect_id = self.synth.connect("int-appeared", self.on_int_appeared)
            self.int_disappeared_connect_id = self.synth.connect("int-disappeared", self.on_int_disappeared)

            self.synth.ui_on()

            self.ui_enabled = True

            for child in self.defered:
                child.parent_group.child_param_add(child)

        self.window.show_all()

    def hide(self):
        '''Hide synth window'''
        if not self.ui_enabled:
            return

        self.window.hide_all()

        self.ui_enabled = False

    def convert_hints(self, hints):
        hints_hash = {}
        for i in range(hints.get_count()):
            hints_hash[hints.get_name_at_index(i)] = hints.get_value_at_index(i)
        return hints_hash

    def on_group_appeared(self, synth, parent, group_name, hints, context):
        #print "-------------- Group \"%s\" appeared" % group_name
        #print "synth: %s" % repr(synth)
        #print "parent: %s" % repr(parent)
        #print "group_name: %s" % group_name
        #print "hints: %s" % repr(hints)
        #print "hints count: %s" % hints.get_count()
        #for i in range(hints.get_count()):
        #    print hints.get_name_at_index(i)
        #    print hints.get_value_at_index(i)
        hints_hash = self.convert_hints(hints)
        #print repr(hints_hash)
        #print "context: %s" % repr(context)

        if not parent:
            return SynthWindowUniversalGroupGeneric(self, parent, group_name, hints_hash, context)

        return parent.on_child_group_appeared(group_name, hints_hash, context)

    def on_group_disappeared(self, synth, obj):
        #print "-------------- Group \"%s\" disappeared" % obj.group_name
        return

    def on_bool_appeared(self, synth, parent, name, hints, value, context):
        #print "-------------- Bool \"%s\" appeared" % name
        #print "synth: %s" % repr(synth)
        #print "parent: %s" % repr(parent)
        #print "name: %s" % name
        hints_hash = self.convert_hints(hints)
        #print repr(hints_hash)
        #print "value: %s" % repr(value)
        #print "context: %s" % repr(context)

        return parent.on_bool_appeared(self.window, name, hints_hash, value, context)

    def on_bool_disappeared(self, synth, obj):
        #print "-------------- Bool disappeared"
        obj.remove()

    def on_float_appeared(self, synth, parent, name, hints, value, min, max, context):
        #print "-------------- Float \"%s\" appeared" % name
        #print "synth: %s" % repr(synth)
        #print "parent: %s" % repr(parent)
        #print "name: %s" % name
        hints_hash = self.convert_hints(hints)
        #print repr(hints_hash)
        #print "value: %s" % repr(value)
        #print "min: %s" % repr(min)
        #print "max: %s" % repr(max)
        #print "context: %s" % repr(context)

        return parent.on_float_appeared(self.window, name, hints_hash, value, min, max, context)

    def on_float_disappeared(self, synth, obj):
        #print "-------------- Float \"%s\" disappeared" % obj.parameter_name
        #print repr(self.parent_group)
        #print repr(obj)
        obj.remove()

    def on_enum_appeared(self, synth, parent, name, hints, selected_value_index, valid_values, context):
        #print "-------------- Enum \"%s\" appeared" % name
        #print "synth: %s" % repr(synth)
        #print "parent: %s" % repr(parent)
        #print "name: %s" % name
        hints_hash = self.convert_hints(hints)
        #print repr(hints_hash)
        #print "selected value index: %s" % repr(selected_value_index)
        #print "valid values: %s" % repr(valid_values)
        #print "valid values count: %s" % valid_values.get_count()
        #for i in range(valid_values.get_count()):
        #    print valid_values.get_at_index(i)
        #print "context: %s" % repr(context)
        return parent.on_enum_appeared(self.window, name, hints_hash, selected_value_index, valid_values, context)

    def on_enum_disappeared(self, synth, obj):
        #print "-------------- Enum \"%s\" disappeared" % obj.parameter_name
        obj.remove()

    def on_int_appeared(self, synth, parent, name, hints, value, min, max, context):
        #print "-------------- Integer \"%s\" appeared" % name
        #print "synth: %s" % repr(synth)
        #print "parent: %s" % repr(parent)
        #print "name: %s" % name
        hints_hash = self.convert_hints(hints)
        #print repr(hints_hash)
        #print "value: %s" % repr(value)
        #print "min: %s" % repr(min)
        #print "max: %s" % repr(max)
        #print "context: %s" % repr(context)

        return parent.on_int_appeared(self.window, name, hints_hash, value, min, max, context)

    def on_int_disappeared(self, synth, obj):
        #print "-------------- Integer \"%s\" disappeared" % obj.parameter_name
        #print repr(self.parent_group)
        #print repr(obj)
        obj.remove()

class SynthWindowUniversalGroupGeneric(SynthWindowUniversalGroup):
    def __init__(self, window, parent, group_name, hints, context):
        SynthWindowUniversalGroup.__init__(self, window, parent, group_name, hints, context)

        if self.window.layout_type == SynthWindowUniversal.layout_type_horizontal:
            self.box_params = gtk.VBox()
            self.box_top = gtk.HBox()
        else:
            self.box_params = gtk.HBox()
            self.box_top = gtk.VBox()

        self.box_top.pack_start(self.box_params, False, False)

        if parent:
            if hints.has_key(hint_uris['hidden']):
                frame = self.box_top
            else:
                frame = gtk.Frame(group_name)
                frame.set_shadow_type(self.window.group_shadow_type)
                self.frame = frame

                frame.add(self.box_top)

                if self.window.layout_type == SynthWindowUniversal.layout_type_horizontal:
                    frame.set_label_align(0.5, 0.5)

            align = gtk.Alignment(0.5, 0.5, 1.0, 1.0)
            align.set_padding(0, 10, 10, 10)
            align.add(frame)
            self.top = align
            parent.child_group_add(self)
        else:
            self.scrolled_window = gtk.ScrolledWindow()
            self.scrolled_window.set_policy(gtk.POLICY_AUTOMATIC, gtk.POLICY_AUTOMATIC)

            self.scrolled_window.add_with_viewport(self.box_top)
            self.window.window.add(self.scrolled_window)

        if window.ui_enabled:
            window.window.show_all()

        #print "Generic group \"%s\" created: %s" % (group_name, repr(self))

    def child_param_add(self, obj):
        #print "child_param_add %s for group \"%s\"" % (repr(obj), self.group_name)
        self.box_params.pack_start(obj.get_top_widget(), False, False)

        if self.window.ui_enabled:
            self.window.window.show_all()

    def child_param_remove(self, obj):
        #print "child_param_remove %s for group \"%s\"" % (repr(obj), self.group_name)
        self.box_params.remove(obj.get_top_widget())

    def child_group_add(self, obj):
        #print "child_group_add %s for group \"%s\"" % (repr(obj), self.group_name)
        self.box_top.pack_start(obj.get_top_widget(), False, True)

    def child_group_remove(self, obj):
        #print "child_group_remove %s for group \"%s\"" % (repr(obj), self.group_name)
        pass

    def on_bool_appeared(self, window, name, hints, value, context):
        parameter = SynthWindowUniversalParameterBool(self.window, self, name, value, context)
        self.child_param_add(parameter)
        return parameter

    def on_float_appeared(self, window, name, hints, value, min, max, context):
        parameter = SynthWindowUniversalParameterFloat(self.window, self, name, value, min, max, context)
        self.child_param_add(parameter)
        return parameter

    def on_int_appeared(self, window, name, hints, value, min, max, context):
        parameter = SynthWindowUniversalParameterInt(self.window, self, name, value, min, max, context)
        self.child_param_add(parameter)
        return parameter

    def on_enum_appeared(self, window, name, hints, selected_value_index, valid_values, context):
        parameter = SynthWindowUniversalParameterEnum(self.window, self, name, selected_value_index, valid_values, context)
        self.child_param_add(parameter)
        return parameter

    def get_top_widget(self):
        return self.top

    def change_display_name(self, name):
        self.frame.set_label(name)

class SynthWindowUniversalGroupToggleFloat(SynthWindowUniversalGroup):
    def __init__(self, window, parent, group_name, hints, context):
        SynthWindowUniversalGroup.__init__(self, window, parent, group_name, hints, context)

        if self.window.layout_type == SynthWindowUniversal.layout_type_horizontal:
            self.box = gtk.VBox()
        else:
            self.box = gtk.HBox()

        self.float = None
        self.bool = None

        self.align = gtk.Alignment(0.5, 0.5, 1.0, 1.0)
        self.align.set_padding(0, 10, 10, 10)
        self.align.add(self.box)

        self.top = self.align

        m = re.match(r"([^:]+):(.*)", group_name)

        self.bool = SynthWindowUniversalParameterBool(self.window, self, m.group(1), False, None)
        self.child_param_add(self.bool)

        self.float = SynthWindowUniversalParameterFloat(self.window, self, m.group(2), 0, 0, 1, None)
        self.float.set_sensitive(False)
        self.child_param_add(self.float)

        #print "Toggle float group \"%s\" created: %s" % (group_name, repr(self))

    def child_param_add(self, obj):
        #print "child_param_add %s for group \"%s\"" % (repr(obj), self.group_name)
        self.box.pack_start(obj.get_top_widget(), False, False)

        if self.window.ui_enabled:
            self.window.window.show_all()

    def child_param_remove(self, obj):
        #print "child_param_remove %s for group \"%s\"" % (repr(obj), self.group_name)
        if obj == self.float:
            self.float.set_sensitive(False)

    def get_top_widget(self):
        return self.top

    def on_bool_appeared(self, window, name, hints, value, context):
        self.bool.context = context
        return self.bool

    def on_float_appeared(self, window, name, hints, value, min, max, context):
        self.float.context = context
        self.float.set_sensitive(True)
        self.float.set(name, value, min, max)
        return self.float

class SynthWindowUniversalGroupOneSubGroup(SynthWindowUniversalGroup):
    def __init__(self, window, parent, group_name, hints, context):
        SynthWindowUniversalGroup.__init__(self, window, parent, group_name, hints, context)

        self.box = gtk.VBox()

        self.align = gtk.Alignment(0.5, 0.5, 1.0, 1.0)
        self.align.set_padding(10, 10, 10, 10)
        self.align.add(self.box)
        self.groups = []

        self.top = self.align

        self.top.connect("realize", self.on_realize)

        #print "Notebook group \"%s\" created: %s" % (group_name, repr(self))

    def on_realize(self, obj):
        #print "realize"
        self.box.pack_start(self.enum.get_top_widget())
        self.enum.connect("zynjacku-parameter-changed", self.on_changed)
        selected = self.enum.get_selection()
        for group in self.groups:
            if group.group_name == selected:
                self.box.pack_start(group.get_top_widget())
                self.selected_group = group

    def on_enum_appeared(self, window, name, hints, selected_value_index, valid_values, context):
        #print "enum appeared"
        parameter = SynthWindowUniversalParameterEnum(self.window, self, name, selected_value_index, valid_values, context)
        self.enum = parameter
        return parameter

    def child_group_add(self, obj):
        #print "child_group_add %s for group \"%s\"" % (repr(obj), self.group_name)
        #self.box.pack_start(obj.get_top_widget())
        self.groups.append(obj)
        obj.change_display_name(self.group_name)

    def child_group_remove(self, obj):
        #print "child_group_remove %s for group \"%s\"" % (repr(obj), self.group_name)
        return

#    def child_param_add(self, obj):
#        print "child_param_add %s for group \"%s\"" % (repr(obj), self.group_name)

#    def child_param_remove(self, obj):
#        print "child_param_remove %s for group \"%s\"" % (repr(obj), self.group_name)

    def get_top_widget(self):
        return self.top

    def on_changed(self, adjustment):
        #print "Enum changed."
        self.box.remove(self.selected_group.get_top_widget())
        selected = self.enum.get_selection()
        for group in self.groups:
            if group.group_name == selected:
                self.box.pack_start(group.get_top_widget())
                group.get_top_widget().show_all()
                self.selected_group = group

class SynthWindowUniversalParameterFloat(SynthWindowUniversalParameter):
    def __init__(self, window, parent_group, name, value, min, max, context):
        SynthWindowUniversalParameter.__init__(self, window, parent_group, name, context)

        self.box = gtk.VBox()

        self.label = gtk.Label(name)
        align = gtk.Alignment(0, 0)
        align.add(self.label)
        self.box.pack_start(align, False, False)

        adjustment = gtk.Adjustment(value, min, max, 1, 19)

        hbox = gtk.HBox()
        self.knob = phat.HFanSlider()
        self.knob.set_adjustment(adjustment)
        #self.knob.set_size_request(200,-1)
        #align = gtk.Alignment(0.5, 0.5)
        #align.add(self.knob)
        hbox.pack_start(self.knob, True, True)
        self.spin = gtk.SpinButton(adjustment, 0.0, 2)
        #align = gtk.Alignment(0.5, 0.5)
        #align.add(self.spin)
        hbox.pack_start(self.spin, False, False)

        #align = gtk.Alignment(0.5, 0.5)
        #align.add(hbox)
        self.box.pack_start(hbox, True, True)

        self.align = gtk.Alignment(0.5, 0.5, 1.0, 1.0)
        self.align.set_padding(10, 10, 10, 10)
        self.align.add(self.box)

        self.top = self.align

        self.adjustment = adjustment

        self.cid = adjustment.connect("value-changed", self.on_value_changed)

        #print "Float \"%s\" created: %s" % (name, repr(self))

    def get_top_widget(self):
        return self.top

    def on_value_changed(self, adjustment):
        #print "Float changed. \"%s\" set to %f" % (self.parameter_name, adjustment.get_value())
        self.window.synth.float_set(self.context, adjustment.get_value())

    def set_sensitive(self, sensitive):
        self.knob.set_sensitive(sensitive)
        self.spin.set_sensitive(sensitive)

    def set(self, name, value, min, max):
        self.adjustment.disconnect(self.cid)
        self.label.set_text(name)
        self.adjustment = gtk.Adjustment(value, min, max, 1, 19)
        self.spin.set_adjustment(self.adjustment)
        self.knob.set_adjustment(self.adjustment)
        self.cid = self.adjustment.connect("value-changed", self.on_value_changed)

class SynthWindowUniversalParameterInt(SynthWindowUniversalParameter):
    def __init__(self, window, parent_group, name, value, min, max, context):
        SynthWindowUniversalParameter.__init__(self, window, parent_group, name, context)

        self.box = gtk.HBox()

        self.label = gtk.Label(name)
        align = gtk.Alignment(0.5, 0.5)
        align.add(self.label)
        self.box.pack_start(align, True, True)

        adjustment = gtk.Adjustment(value, min, max, 1, 19)

        self.spin = gtk.SpinButton(adjustment, 0.0, 0)
        #align = gtk.Alignment(0.5, 0.5)
        #align.add(self.spin)
        self.box.pack_start(self.spin, True, False)

        self.align = gtk.Alignment(0.5, 0.5, 1.0, 1.0)
        self.align.set_padding(10, 10, 10, 10)
        self.align.add(self.box)

        self.top = self.align

        self.adjustment = adjustment

        self.cid = adjustment.connect("value-changed", self.on_value_changed)

        #print "Int \"%s\" created: %s" % (name, repr(self))

    def get_top_widget(self):
        return self.top

    def on_value_changed(self, adjustment):
        #print "Int changed. \"%s\" set to %d" % (self.parameter_name, int(adjustment.get_value()))
        self.window.synth.int_set(self.context, int(adjustment.get_value()))

    def set_sensitive(self, sensitive):
        self.knob.set_sensitive(sensitive)
        self.spin.set_sensitive(sensitive)

    def set(self, name, value, min, max):
        self.adjustment.disconnect(self.cid)
        self.label.set_text(name)
        self.adjustment = gtk.Adjustment(value, min, max, 1, 19)
        self.spin.set_adjustment(self.adjustment)
        self.knob.set_adjustment(self.adjustment)
        self.cid = self.adjustment.connect("value-changed", self.on_value_changed)

class SynthWindowUniversalParameterBool(SynthWindowUniversalParameter):
    def __init__(self, window, parent_group, name, value, context):
        SynthWindowUniversalParameter.__init__(self, window, parent_group, name, context)

        widget = gtk.CheckButton(name)

        widget.set_active(value)
        widget.connect("toggled", self.on_toggled)

        self.align = gtk.Alignment(0.5, 0.5, 1.0, 1.0)
        self.align.set_padding(10, 10, 10, 10)
        self.align.add(widget)

    def get_top_widget(self):
        return self.align

    def on_toggled(self, widget):
        #print "Boolean toggled. \"%s\" set to \"%s\"" % (widget.get_label(), widget.get_active())
        self.window.synth.bool_set(self.context, widget.get_active())

class SynthWindowUniversalParameterEnum(SynthWindowUniversalParameter):
    def __init__(self, window, parent_group, name, selected_value_index, valid_values, context):
        SynthWindowUniversalParameter.__init__(self, window, parent_group, name, context)

        label = gtk.Label(name)

        self.box = gtk.VBox()

        self.label = gtk.Label(name)
        align = gtk.Alignment(0, 0)
        align.set_padding(0, 0, 10, 10)
        align.add(self.label)
        self.box.pack_start(align, True, True)

        self.liststore = gtk.ListStore(gobject.TYPE_STRING)
        self.combobox = gtk.ComboBox(self.liststore)
        self.cell = gtk.CellRendererText()
        self.combobox.pack_start(self.cell, True)
        self.combobox.add_attribute(self.cell, 'text', 0)

        for i in range(valid_values.get_count()):
            row = valid_values.get_at_index(i),
            self.liststore.append(row)

        self.combobox.set_active(selected_value_index)
        self.selected_value_index = selected_value_index;

        self.combobox.connect("changed", self.on_changed)

        align = gtk.Alignment(0.5, 0.5, 1.0, 1.0)
        align.set_padding(0, 10, 10, 10)
        align.add(self.combobox)

        self.box.pack_start(align, False, False)

    def get_top_widget(self):
        return self.box

    def on_changed(self, adjustment):
        #print "Enum changed. \"%s\" set to %u" % (self.parameter_name, adjustment.get_active())
        self.selected_value_index = adjustment.get_active()
        self.window.synth.enum_set(self.context, self.selected_value_index)
        self.emit("zynjacku-parameter-changed")

    def get_selection(self):
        return self.liststore.get(self.liststore.iter_nth_child(None, self.selected_value_index), 0)[0]

class SynthWindowFactory:
    def __init__(self):
        self.group_shadow_type = gtk.SHADOW_ETCHED_OUT
        self.layout_type = SynthWindowUniversal.layout_type_horizontal

    def create_synth_window(self, synth):
        if not self.synth_window_available(synth):
            return False

        if synth.supports_custom_ui():
            synth.ui_win = SynthWindowCustom(synth)
        else:
            synth.ui_win = SynthWindowUniversal(synth, self.group_shadow_type, self.layout_type)

        return True

    def synth_window_available(self, synth):
        return synth.supports_custom_ui() or synth.supports_generic_ui()

class ZynjackuHost(SynthWindowFactory):
    def __init__(self, client_name):
        #print "ZynjackuHost constructor called."

        SynthWindowFactory.__init__(self)

        self.engine = zynjacku.Engine()

        if not self.engine.start_jack(client_name):
            print "Failed to initialize zynjacku engine"
            sys.exit(1)

    def __del__(self):
        #print "ZynjackuHost destructor called."

        self.engine.stop_jack()

    def ui_run(self):
        self.engine.ui_run()
        return True

    def run(self):
        ui_run_callback_id = gobject.timeout_add(100, self.ui_run)
        gtk.main()
        gobject.source_remove(ui_run_callback_id)

    def on_test(self, obj1, obj2):
        print "on_test() called !!!!!!!!!!!!!!!!!!"
        print repr(obj1)
        print repr(obj2)

class ZynjackuHostMulti(ZynjackuHost):
    def __init__(self, data_dir, glade_xml, client_name, the_license, uris):
        #print "ZynjackuHostMulti constructor called."
        ZynjackuHost.__init__(self, client_name)
        
        self.synths = []

        self.data_dir = data_dir
        self.glade_xml = glade_xml

        self.main_window = glade_xml.get_widget("zynjacku_main")
        self.main_window.set_title(client_name)

        self.statusbar = self.glade_xml.get_widget("statusbar")

        self.hbox_menubar = glade_xml.get_widget("hbox_menubar")
        self.midi_led = midi_led()
        self.midi_led_frame = gtk.Frame()
        self.midi_led_frame.set_shadow_type(gtk.SHADOW_OUT)
        self.midi_led_frame.add(self.midi_led);
        self.hbox_menubar.pack_start(self.midi_led_frame, False, False)

	#Create our dictionay and connect it
        dic = {"on_quit_activate" : gtk.main_quit,
               "on_about_activate" : self.on_about,
               "on_preset_load_activate" : self.on_preset_load,
               "on_preset_save_as_activate" : self.on_preset_save_as,
               "on_synth_load_activate" : self.on_synth_load,
               "on_synth_clear_activate" : self.on_synth_clear,
               }
        glade_xml.signal_autoconnect(dic)

        self.the_license = the_license

        self.synths_widget = glade_xml.get_widget("treeview_synths")

        self.store = gtk.ListStore(gobject.TYPE_BOOLEAN, gobject.TYPE_STRING, gobject.TYPE_STRING, gobject.TYPE_STRING, gobject.TYPE_PYOBJECT)
        text_renderer = gtk.CellRendererText()
        self.toggle_renderer = gtk.CellRendererToggle()
        self.toggle_renderer.set_property('activatable', True)

        column_ui_visible = gtk.TreeViewColumn("UI", self.toggle_renderer)
        column_ui_visible.add_attribute(self.toggle_renderer, "active", 0)
        column_instance = gtk.TreeViewColumn("Instance", text_renderer, text=1)
        column_name = gtk.TreeViewColumn("Name", text_renderer, text=2)
        #column_uri = gtk.TreeViewColumn("URI", text_renderer, text=3)

        self.synths_widget.append_column(column_ui_visible)
        self.synths_widget.append_column(column_instance)
        self.synths_widget.append_column(column_name)
        #self.synths_widget.append_column(column_uri)

        for uri in uris:
            self.add_synth(uri)

        self.synths_widget.set_model(self.store)

        self.main_window.show_all()
        self.main_window.connect("destroy", gtk.main_quit)

    def __del__(self):
        #print "ZynjackuHostMulti destructor called."

        self.store.clear()

        for synth in self.synths:
            synth.destruct()

        ZynjackuHost.__del__(self)

    def update_midi_led(self):
        self.midi_led.set(self.engine.get_midi_activity())
        return True

    def add_synth(self, uri):
        statusbar_context_id = self.statusbar.get_context_id("loading plugin")
        statusbar_id = self.statusbar.push(statusbar_context_id, "Loading %s" % uri)
        while gtk.events_pending():
            gtk.main_iteration()
        synth = zynjacku.Synth(uri=uri)
        self.statusbar.pop(statusbar_id)
        if not synth.construct(self.engine):
            self.statusbar.push(statusbar_context_id, "Failed to construct %s" % uri)
        else:
            self.synths.append(synth)
            synth.ui_win = None
            row = False, synth.get_instance_name(), synth.get_name(), synth.get_uri(), synth
            self.store.append(row)
            self.statusbar.remove(statusbar_context_id, statusbar_id)

    def run(self):
        toggled_connect_id = self.toggle_renderer.connect('toggled', self.on_ui_visible_toggled, self.store)

        update_midi_led_callback_id = gobject.timeout_add(100, self.update_midi_led)

        ZynjackuHost.run(self)

        gobject.source_remove(update_midi_led_callback_id)

        for synth in self.synths:
            if synth.ui_win:
                synth.ui_win.disconnect(synth.ui_win.destroy_connect_id) # signal connection holds reference to synth object...
        self.toggle_renderer.disconnect(toggled_connect_id)

    def on_synth_ui_window_destroyed(self, window, synth, row):
        synth.ui_win.disconnect(synth.ui_win.destroy_connect_id) # signal connection holds reference to synth object...
        synth.ui_win = None
        row[0] = False

    def create_synth_window(self, synth, row):
        if not ZynjackuHost.create_synth_window(self, synth):
            return False

        synth.ui_win.destroy_connect_id = synth.ui_win.connect("destroy", self.on_synth_ui_window_destroyed, synth, row)
        return True

    def on_ui_visible_toggled(self, cell, path, model):
        #print "on_ui_visible_toggled() called."
        if model[path][0]:
            model[path][4].ui_win.hide()
            model[path][0] = False
        else:
            if not model[path][4].ui_win:
                if not self.create_synth_window(model[path][4], model[path]):
                    return

            statusbar_context_id = self.statusbar.get_context_id("loading plugin UI")
            if model[path][4].ui_win.show():
                model[path][0] = True
            else:
                self.statusbar.push(statusbar_context_id, "Failed to construct show synth UI")

    def on_about(self, widget):
        about = gtk.AboutDialog()
        about.set_transient_for(self.main_window)
        about.set_name("zynjacku")
        if zynjacku.zynjacku_get_version() == "dev":
            about.set_comments("(development snapshot)")
        else:
            about.set_version(zynjacku.zynjacku_get_version())
        about.set_license(self.the_license)
        about.set_website("http://home.gna.org/zynjacku/")
        about.set_authors(["Nedko Arnaudov"])
        about.set_artists(["Thorsten Wilms"])
        about.set_logo(gtk.gdk.pixbuf_new_from_file("%s/logo.png" % self.data_dir))
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

    def on_synth_load(self, widget):
        dialog = self.glade_xml.get_widget("zynjacku_plugin_repo")
        plugin_repo_widget = self.glade_xml.get_widget("treeview_available_synths")
        progressbar = self.glade_xml.get_widget("progressbar")

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
                    self.add_synth(store.get(store.get_iter(path), 1)[0])
                return
            elif ret == 1:
                self.rescan_plugins(store, progressbar, True)
            else:
                dialog.hide()
                return

    def on_synth_clear(self, widget):
        self.store.clear();
        for synth in self.synths:
            synth.destruct()
        self.synths = []

class ZynjackuHostOne(ZynjackuHost):
    def __init__(self, glade_xml, client_name, uri):
        #print "ZynjackuHostOne constructor called."
        ZynjackuHost.__init__(self, client_name)

        self.synth = zynjacku.Synth(uri=uri)
        if not self.synth.construct(self.engine):
            print"Failed to construct %s" % uri
            del(self.synth)
            self.synth = None
        else:
            if not ZynjackuHost.synth_window_available(self, self.synth):
                print"Synth window not available"
                self.synth.destruct()
                del(self.synth)
                self.synth = None
            else:
                if not ZynjackuHost.create_synth_window(self, self.synth):
                    print"Failed to create synth window"
                    self.synth.destruct()
                    del(self.synth)
                    self.synth = None

    def run(self):
        if not self.synth:
            return

        if False:                       # test code
            self.synth.ui_win.show()

            self.synth.ui_win.hide()
            self.synth.ui_off()
            del(self.synth.ui_win)

            if not ZynjackuHost.create_synth_window(self, self.synth):
                print"Failed to create synth window"
                return

            self.synth.ui_win.show()

            return
        
        test_connect_id = self.synth.connect("test", self.on_test)
        if not self.synth.ui_win.show():
            return
        self.synth.ui_win.connect("destroy", gtk.main_quit)

        ZynjackuHost.run(self)

        self.synth.disconnect(test_connect_id)

    def __del__(self):
        #print "ZynjackuHostOne destructor called."

        if self.synth:
            self.synth.destruct()

        ZynjackuHost.__del__(self)

def main():
    data_dir = os.path.dirname(sys.argv[0])

    # since ppl tend to run "python zynjacku.py", lets assume that it is in current directory
    # "python ./zynjacku.py" and "./zynjacku.py" will work anyway.
    if not data_dir:
        data_dir = "."

    glade_file = data_dir + os.sep + "zynjacku.glade"

    if not os.path.isfile(glade_file):
        data_dir = data_dir + os.sep + ".." + os.sep + "share"+ os.sep + "zynjacku"
        glade_file = data_dir + os.sep + "zynjacku.glade"

    #print 'data dir is "%s"' % data_dir
    #print 'Loading glade from "%s"' % glade_file

    the_license = file(data_dir + os.sep + "gpl.txt").read()

    glade_xml = gtk.glade.XML(glade_file)

    gobject.type_register(SynthWindow)
    gobject.type_register(SynthWindowUniversal)
    gobject.type_register(SynthWindowUniversalGroupGeneric)
    gobject.type_register(SynthWindowUniversalGroupToggleFloat)
    gobject.type_register(SynthWindowUniversalParameterFloat)
    gobject.type_register(SynthWindowUniversalParameterBool)

    client_name = "zynjacku"

    if len(sys.argv) == 2:
        host = ZynjackuHostOne(glade_xml, client_name, sys.argv[1])
    else:
        host = ZynjackuHostMulti(data_dir, glade_xml, client_name, the_license, sys.argv[1:])

    host.run()

gobject.signal_new("zynjacku-parameter-changed", SynthWindowUniversalParameter, gobject.SIGNAL_RUN_FIRST | gobject.SIGNAL_ACTION, gobject.TYPE_NONE, [])

main()
