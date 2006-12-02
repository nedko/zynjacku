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

client_name = "zynjacku"

engine = zynjacku.Engine()

if not engine.start_jack(client_name):
    print "Failed to initialize zynjacku engine"
    sys.exit(1)

synths = []

def on_group_added(obj):
    print "on_group_added() called !!!!!!!!!!!!!!!!!!"
    return

for arg in sys.argv[1:]:
    print "Loading %s" % arg
    synth = zynjacku.Synth()
    if not synth.construct(engine):
        print"Failed to construct %s" % arg
    else:
        synths.append(synth)
        synth.connect("group-added", on_group_added)
    del(synth)

main_window = glade_xml.get_widget("zynjacku_main")
main_window.set_title(client_name)

synths_widget = glade_xml.get_widget("treeview_synths")

store = gtk.ListStore(gobject.TYPE_STRING, gobject.TYPE_STRING, gobject.TYPE_STRING, gobject.TYPE_PYOBJECT)
renderer = gtk.CellRendererText()

column_name = gtk.TreeViewColumn("Name", renderer, text=0)
column_class = gtk.TreeViewColumn("Class", renderer, text=1)
column_uri = gtk.TreeViewColumn("URI", renderer, text=2)

synths_widget.append_column(column_name)
synths_widget.append_column(column_class)
synths_widget.append_column(column_uri)

for synth in synths:
    row = synth.get_name(), synth.get_class_name(), synth.get_class_uri(), synth
    store.append(row)
    del(row)
    del(synth)

synths_widget.set_model(store)

main_window.show_all()
main_window.connect("destroy", gtk.main_quit)

gtk.main()

store.clear()

for synth in synths:
    synth.destruct()
    del(synth)

del(synths)

engine.stop_jack()

del(engine)
