#
# This file is part of zynjacku
#
# Copyright (C) 2007,2008 Nedko Arnaudov <nedko@arnaudov.name>
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

zynjacku_c_CFLAGS = -DDATA_DIR='"$(pkgdatadir)"' -Wall -g

if DEV_VERSION
zynjacku_c_CFLAGS += -Werror
endif

zynjacku_c_CFLAGS += @GTK_CFLAGS@
zynjacku_c_CFLAGS += @PYGTK_CFLAGS@
zynjacku_c_CFLAGS += @SLV2_CFLAGS@
zynjacku_c_CFLAGS += @JACK_CFLAGS@
zynjacku_c_CFLAGS += @LV2DYNPARAMHOST_CFLAGS@

INCLUDES = $(PYTHON_INCLUDES) $(zynjacku_c_CFLAGS)

pkgpyexecdir = $(pythondir)

pkgpyexec_LTLIBRARIES = zynjacku_c.la

ZYNJACKU_LDFLAGS = @GTK_LIBS@
ZYNJACKU_LDFLAGS += @PYGTK_LIBS@
ZYNJACKU_LDFLAGS += @SLV2_LIBS@
ZYNJACKU_LDFLAGS += @JACK_LIBS@
ZYNJACKU_LDFLAGS += @LV2DYNPARAMHOST_LIBS@

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
zynjacku_c_la_SOURCES += synth.h
zynjacku_c_la_SOURCES += effect.h
zynjacku_c_la_SOURCES += lv2.c
zynjacku_c_la_SOURCES += lv2.h
zynjacku_c_la_SOURCES += plugin_repo.c
zynjacku_c_la_SOURCES += plugin_repo.h
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
zynjacku_c_la_SOURCES += lv2-miditype.h
zynjacku_c_la_SOURCES += rtmempool.c
zynjacku_c_la_SOURCES += rtmempool.h

#nodist_zynjacku_la_SOURCES = 

# The path to the GTK+ python types (FIXME: use configure result)
PYGTK_DEFS=`pkg-config --variable=defsdir pygtk-2.0`
PYGTK_CODEGEN=`pkg-config --variable=codegendir pygtk-2.0`

zynjacku.defs: engine.h rack.h plugin.h enum.h hints.h
	python $(PYGTK_CODEGEN)/h2def.py $^ > $@

# Generate the C wrapper from the defs and our override file
zynjacku_wrap.c: zynjacku.defs zynjacku.override
	pygtk-codegen-2.0 --prefix zynjacku_c \
	--register $(PYGTK_DEFS)/gdk-types.defs \
	--register $(PYGTK_DEFS)/gtk-types.defs \
	--override zynjacku.override \
	zynjacku.defs > $@

CLEANFILES = zynjacku_wrap.c zynjacku.pyc zynjacku.defs
EXTRA_DIST = zynjacku.override zynjacku.defs zynjacku.py

bin_SCRIPTS = zynjacku.py

zynjackudir = $(pkgdatadir)
dist_zynjacku_DATA = gpl.txt zynjacku.glade logo.png

install-exec-hook:
	ln -vfs zynjacku.py $(DESTDIR)$(bindir)/zynjacku

uninstall-hook:
	rm -vf $(DESTDIR)$(bindir)/zynjacku
