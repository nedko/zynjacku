#!/usr/bin/env python
#
# This file is part of zynjacku
#
# Copyright (C) 2006,2007 Nedko Arnaudov <nedko@arnaudov.name>
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
import pango
import gobject

class widget(gtk.EventBox):
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
