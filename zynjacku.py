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
import phat
import re

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

class SynthWindowUniversalGroup(gobject.GObject):
    def __init__(self, window, parent_group, name, context):
        gobject.GObject.__init__(self)
        self.window = window
        self.parent_group = parent_group
        self.group_name = name
        self.context = context

    def child_add(self, obj):
        return

    def child_remove(self, obj):
        return

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
        self.parent_group.child_remove(self)

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
        self.window.set_title(synth.get_name())
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

            self.synth.ui_on()

            self.ui_enabled = True

            for child in self.defered:
                child.parent_group.child_add(child)

        self.window.show_all()

    def hide(self):
        '''Hide synth window'''
        if not self.ui_enabled:
            return

        self.window.hide_all()

        self.ui_enabled = False

    def on_group_appeared(self, synth, parent, group_name, group_type, context):
        #print "-------------- Group \"%s\" appeared" % group_name
        #print "synth: %s" % repr(synth)
        #print "parent: %s" % repr(parent)
        #print "group_name: %s" % group_name
        #print "group_type: %s" % group_type
        #print "context: %s" % repr(context)

        if not parent:
            return SynthWindowUniversalGroupGeneric(self, parent, group_name, context)

        return parent.on_child_group_appeared(group_name, group_type, context)

    def on_group_disappeared(self, synth, obj):
        #print "-------------- Group \"%s\" disappeared" % obj.group_name
        return

    def on_bool_appeared(self, synth, parent, name, value, context):
        #print "-------------- Bool \"%s\" appeared" % name
        #print "synth: %s" % repr(synth)
        #print "parent: %s" % repr(parent)
        #print "name: %s" % name
        #print "value: %s" % repr(value)
        #print "context: %s" % repr(context)

        return parent.on_bool_appeared(self.window, name, value, context)

    def on_bool_disappeared(self, synth, obj):
        #print "-------------- Bool disappeared"
        obj.remove()

    def on_float_appeared(self, synth, parent, name, value, min, max, context):
        #print "-------------- Float \"%s\" appeared" % name
        #print "synth: %s" % repr(synth)
        #print "parent: %s" % repr(parent)
        #print "name: %s" % name
        #print "value: %s" % repr(value)
        #print "min: %s" % repr(min)
        #print "max: %s" % repr(max)
        #print "context: %s" % repr(context)

        return parent.on_float_appeared(self.window, name, value, min, max, context)

    def on_float_disappeared(self, synth, obj):
        #print "-------------- Float \"%s\" disappeared" % obj.parameter_name
        #print repr(self.parent_group)
        #print repr(obj)
        obj.remove()

    def on_enum_appeared(self, synth, parent, name, selected_value_index, valid_values, context):
        print "-------------- Enum \"%s\" appeared" % name
        print "synth: %s" % repr(synth)
        print "parent: %s" % repr(parent)
        print "name: %s" % name
        print "selected value index: %s" % repr(selected_value_index)
        print "valid values: %s" % repr(valid_values)
        print "valid values count: %s" % valid_values.get_count()
        for i in range(valid_values.get_count()):
            print valid_values.get_at_index(i)
        print "context: %s" % repr(context)
        return parent.on_bool_appeared(self.window, "enum", True, context)

    def on_enum_disappeared(self, synth, obj):
        print "-------------- Enum \"%s\" disappeared" % obj.parameter_name
        obj.remove()

class SynthWindowUniversalGroupGeneric(SynthWindowUniversalGroup):
    def __init__(self, window, parent, group_name, context):
        SynthWindowUniversalGroup.__init__(self, window, parent, group_name, context)

        if self.window.layout_type == SynthWindowUniversal.layout_type_horizontal:
            self.box_params = gtk.VBox()
            self.box_top = gtk.HBox()
        else:
            self.box_params = gtk.HBox()
            self.box_top = gtk.VBox()

        self.box_top.pack_start(self.box_params, False, False)

        if parent:
            frame = gtk.Frame(group_name)
            frame.set_shadow_type(self.window.group_shadow_type)

            frame.add(self.box_top)

            if self.window.layout_type == SynthWindowUniversal.layout_type_horizontal:
                frame.set_label_align(0.5, 0.5)

            align = gtk.Alignment(0.5, 0.5, 1.0, 1.0)
            align.set_padding(0, 10, 10, 10)
            align.add(frame)
            parent.box_top.pack_start(align, False, True)
        else:
            self.scrolled_window = gtk.ScrolledWindow()
            self.scrolled_window.set_policy(gtk.POLICY_AUTOMATIC, gtk.POLICY_AUTOMATIC)

            self.scrolled_window.add_with_viewport(self.box_top)
            self.window.window.add(self.scrolled_window)

        if window.ui_enabled:
            window.window.show_all()

        #print "Generic group \"%s\" created: %s" % (group_name, repr(self))

    def child_add(self, obj):
        #print "child_add %s for group \"%s\"" % (repr(obj), self.group_name)
        self.box_params.pack_start(obj.get_top_widget(), False, False)

        if self.window.ui_enabled:
            self.window.window.show_all()

    def child_remove(self, obj):
        #print "child_remove %s for group \"%s\"" % (repr(obj), self.group_name)
        self.box_params.remove(obj.get_top_widget())

    def on_child_group_appeared(self, group_name, group_type, context):
        if group_type == zynjacku.LV2DYNPARAM_GROUP_TYPE_TOGGLE_FLOAT_URI:
            group = SynthWindowUniversalGroupToggleFloat(self.window, self, group_name, context)
            self.window.defered.append(group)
            return group
        else:
            return SynthWindowUniversalGroupGeneric(self.window, self, group_name, context)

    def on_bool_appeared(self, window, name, value, context):
        parameter = SynthWindowUniversalParameterBool(self.window, self, name, value, context)
        self.child_add(parameter)
        return parameter

    def on_float_appeared(self, window, name, value, min, max, context):
        parameter = SynthWindowUniversalParameterFloat(self.window, self, name, value, min, max, context)
        self.child_add(parameter)
        return parameter

class SynthWindowUniversalGroupToggleFloat(SynthWindowUniversalGroup):
    def __init__(self, window, parent, group_name, context):
        SynthWindowUniversalGroup.__init__(self, window, parent, group_name, context)

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
        self.child_add(self.bool)

        self.float = SynthWindowUniversalParameterFloat(self.window, self, m.group(2), 0, 0, 1, None)
        self.float.set_sensitive(False)
        self.child_add(self.float)

        #print "Toggle float group \"%s\" created: %s" % (group_name, repr(self))

    def child_add(self, obj):
        #print "child_add %s for group \"%s\"" % (repr(obj), self.group_name)
        self.box.pack_start(obj.get_top_widget(), False, False)

        if self.window.ui_enabled:
            self.window.window.show_all()

    def child_remove(self, obj):
        #print "child_remove %s for group \"%s\"" % (repr(obj), self.group_name)
        if obj == self.float:
            self.float.set_sensitive(False)

    def get_top_widget(self):
        return self.top

    def on_bool_appeared(self, window, name, value, context):
        self.bool.context = context
        return self.bool

    def on_float_appeared(self, window, name, value, min, max, context):
        self.float.context = context
        self.float.set_sensitive(True)
        self.float.set(name, value, min, max)
        return self.float

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
        #align = gtk.Alignment(0.5, 0.5)
        #align.add(self.knob)
        hbox.pack_start(self.knob, True, True)
        self.spin = gtk.SpinButton(adjustment)
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

class SynthWindowFactory:
    def __init__(self):
        self.group_shadow_type = gtk.SHADOW_ETCHED_OUT
        self.layout_type = SynthWindowUniversal.layout_type_horizontal

    def create_synth_window(self, synth):
        if not self.synth_window_available(synth):
            return False

        synth.ui_win = SynthWindowUniversal(synth, self. group_shadow_type, self.layout_type)
        return True

    def synth_window_available(self, synth):
        return synth.supports_generic_ui()

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
    def __init__(self, glade_xml, client_name, uris):
        #print "ZynjackuHostMulti constructor called."
        ZynjackuHost.__init__(self, client_name)
        
        self.synths = []

        self.main_window = glade_xml.get_widget("zynjacku_main")
        self.main_window.set_title(client_name)

        for uri in uris:
            #print "Loading %s" % uri
            synth = zynjacku.Synth(uri=uri)
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
        #print "ZynjackuHostMulti destructor called."

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
            model[path][4].ui_win.show()
            model[path][0] = True

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
        self.synth.ui_win.show()
        self.synth.ui_win.connect("destroy", gtk.main_quit)

        ZynjackuHost.run(self)

        self.synth.disconnect(test_connect_id)

    def __del__(self):
        #print "ZynjackuHostOne destructor called."

        if self.synth:
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

    gobject.type_register(SynthWindow)
    gobject.type_register(SynthWindowUniversal)
    gobject.type_register(SynthWindowUniversalGroupGeneric)
    gobject.type_register(SynthWindowUniversalGroupToggleFloat)
    gobject.type_register(SynthWindowUniversalParameterFloat)
    gobject.type_register(SynthWindowUniversalParameterBool)

    if len(sys.argv) == 2:
        host = ZynjackuHostOne(glade_xml, "zynjacku", sys.argv[1])
    else:
        host = ZynjackuHostMulti(glade_xml, "zynjacku", sys.argv[1:])

    host.run()

main()
