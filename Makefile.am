#
# This file is part of zynjacku
#
# Copyright (C) 2007,2008,2009 Nedko Arnaudov <nedko@arnaudov.name>
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
#

DEFAULT_INCLUDES =

# we need this in order to include config.h with build dir != src dir
if NOT_TOP_BUILD
CPPFLAGS += -I$(top_builddir)
endif

zynjacku_c_CFLAGS = -DDATA_DIR='"$(pkgdatadir)"' -Wall -g

if DEV_VERSION
zynjacku_c_CFLAGS += -Werror
endif

zynjacku_c_CFLAGS += @GTK_CFLAGS@
zynjacku_c_CFLAGS += @PYGTK_CFLAGS@
zynjacku_c_CFLAGS += @JACK_CFLAGS@
zynjacku_c_CFLAGS += @LV2DYNPARAMHOST1_CFLAGS@

INCLUDES = $(PYTHON_INCLUDES) $(zynjacku_c_CFLAGS)

pkgpyexecdir = $(pythondir)/zynworld/

pkgpyexec_LTLIBRARIES = zynjacku_c.la zynjacku_ttl.la
pkgpyexec_SCRIPTS = zynworld/__init__.py zynworld/lv2.py zynworld/host.py

ZYNJACKU_LDFLAGS = @GTK_LIBS@
ZYNJACKU_LDFLAGS += @PYGTK_LIBS@
ZYNJACKU_LDFLAGS += @JACK_LIBS@
ZYNJACKU_LDFLAGS += @LV2DYNPARAMHOST1_LIBS@

zynjacku_c_la_LDFLAGS = -module -avoid-version $(ZYNJACKU_LDFLAGS)
zynjacku_c_la_LIBADD = $(ZYNJACKU_LIBS)

zynjacku_c_la_SOURCES = zynjacku.h
zynjacku_c_la_SOURCES += engine.c
zynjacku_c_la_SOURCES += engine.h
zynjacku_c_la_SOURCES += rack.c
zynjacku_c_la_SOURCES += rack.h
zynjacku_c_la_SOURCES += jack_compat.c
zynjacku_c_la_SOURCES += plugin.c
zynjacku_c_la_SOURCES += plugin.h
zynjacku_c_la_SOURCES += plugin_internal.h
zynjacku_c_la_SOURCES += lv2.c
zynjacku_c_la_SOURCES += lv2.h
zynjacku_c_la_SOURCES += log.c
zynjacku_c_la_SOURCES += log.h
zynjacku_c_la_SOURCES += zynjacku_wrap.c
zynjacku_c_la_SOURCES += zynjackumodule.c
zynjacku_c_la_SOURCES += enum.c
zynjacku_c_la_SOURCES += enum.h
zynjacku_c_la_SOURCES += gtk2gui.c
zynjacku_c_la_SOURCES += gtk2gui.h
zynjacku_c_la_SOURCES += hints.c
zynjacku_c_la_SOURCES += hints.h
zynjacku_c_la_SOURCES += list.h
zynjacku_c_la_SOURCES += lv2_ui.h
zynjacku_c_la_SOURCES += lv2-miditype.h
zynjacku_c_la_SOURCES += lv2_contexts.h
zynjacku_c_la_SOURCES += lv2_data_access.h
zynjacku_c_la_SOURCES += lv2_event.h
zynjacku_c_la_SOURCES += lv2_event_helpers.h
zynjacku_c_la_SOURCES += lv2_uri_map.h
zynjacku_c_la_SOURCES += lv2_string_port.h
zynjacku_c_la_SOURCES += lv2_progress.h
zynjacku_c_la_SOURCES += lv2_external_ui.h
if HAVE_DYNPARAMS
zynjacku_c_la_SOURCES += rtmempool.c
zynjacku_c_la_SOURCES += rtmempool.h
endif
zynjacku_c_la_SOURCES += midi_cc_map.c
zynjacku_c_la_SOURCES += midi_cc_map.h
zynjacku_c_la_SOURCES += midi_cc_map_internal.h

#nodist_zynjacku_la_SOURCES = 

# The path to the GTK+ python types (FIXME: use configure result)
PYGTK_DEFS=`pkg-config --variable=defsdir pygtk-2.0`
PYGTK_CODEGEN=`pkg-config --variable=codegendir pygtk-2.0`

zynjacku.defs: engine.h rack.h plugin.h enum.h hints.h midi_cc_map.h lv2.h
	python $(PYGTK_CODEGEN)/h2def.py $^ | \
	sed -e '/define-method set_parameter/,/^$$/ { s/\("midi_cc_map_obj_ptr"\)/\1 (null-ok) (default "NULL")/g; }' \
	    -e '/define-method set_midi_cc_map/,/^$$/ { s/\("midi_cc_map_obj_ptr"\)/\1 (null-ok)/g; }' \
	    -e '/define-method ui_on/,/^$$/ { s/\("ui_uri"\)/\1 (null-ok) (default "NULL")/g; }' \
	    -e '/define-method ui_on/,/^$$/ { s/\("ui_type_uri"\)/\1 (null-ok) (default "NULL")/g; }' \
	    -e '/define-method ui_on/,/^$$/ { s/\("ui_binary_path"\)/\1 (null-ok) (default "NULL")/g; }' \
	    -e '/define-method ui_on/,/^$$/ { s/\("ui_bundle_path"\)/\1 (null-ok) (default "NULL")/g; }' \
	> $@

# Generate the C wrapper from the defs and our override file
zynjacku_wrap.c: zynjacku.defs zynjacku.override
	pygtk-codegen-2.0 --prefix zynjacku_c \
	--register $(PYGTK_DEFS)/gdk-types.defs \
	--register $(PYGTK_DEFS)/gtk-types.defs \
	--override zynjacku.override \
	zynjacku.defs > $@

zynjacku_ttl_la_LDFLAGS = -module -avoid-version

zynjacku_ttl_la_SOURCES  = zynjacku_ttl.c
zynjacku_ttl_la_SOURCES += ttl_lexer.h
zynjacku_ttl_la_SOURCES += flex_ttl.c
zynjacku_ttl_la_SOURCES += flex_ttl.h

CLEANFILES = zynjacku_wrap.c *.pyc zynjacku.defs
EXTRA_DIST = zynjacku.override zynjacku.defs zynjacku lv2rack zynspect zynworld/__init__.py  zynworld/lv2.py zynworld/host.py

bin_SCRIPTS = zynjacku lv2rack zynspect

zynjackudir = $(pkgdatadir)
dist_zynjacku_DATA = gpl.txt zynjacku.glade art/logo/logo.png

ttl_flex_regenerate:
	flex --batch --nodefault -o flex_ttl.c --header-file=flex_ttl.h ttl.l
