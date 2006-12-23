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

# Install prefix
INSTALL_PREFIX=/usr

# Where to install GConf schema, this must match the way GConf is configured
INSTALL_GCONF_SCHEMAS_DIR=/etc/gconf/schemas

CFLAGS := -g -I/usr/include/python2.4 -Wall -Werror -D_GNU_SOURCE -fPIC
CFLAGS += $(strip $(shell pkg-config --cflags gtk+-2.0 pygtk-2.0 libslv2 jack))
LDFLAGS := $(strip $(shell pkg-config --libs gtk+-2.0 pygtk-2.0 libslv2 jack))

CC = gcc -c 

GENDEP_SED_EXPR = "s/^\\(.*\\)\\.o *: /$(subst /,\/,$(@:.d=.o)) $(subst /,\/,$@) : /g"
GENDEP_C = set -e; gcc -MM $(CFLAGS) $< | sed $(GENDEP_SED_EXPR) > $@; [ -s $@ ] || rm -f $@

.PHONY: run test install uninstall

SOURCES = dynparam_preallocate.c dynparam.c audiolock.c engine.c synth.c plugin_repo.c log.c zynjacku_wrap.c zynjackumodule.c dynparam_host_callbacks.c
OBJECTS = $(SOURCES:%.c=%.o)

# The path to the GTK+ python types
DEFS=`pkg-config --variable=defsdir pygtk-2.0`

default: zynjacku.so

zynjacku.defs: engine.h synth.h plugin_repo.h
	python /usr/share/pygtk/2.0/codegen/h2def.py $^ > $@

zynjackumodule.c: init_py_constants.c

init_py_constants.c: lv2dynparam.h
	grep 'LV2[^ ]*URI' lv2dynparam.h | sed 's/#define  *LV2\([^ ]*URI\)  *\"\(.*\)\"/PyModule_AddObject(m, "\1", PyString_FromString("\2"));/' > init_py_constants.c

# Generate the C wrapper from the defs and our override file
zynjacku_wrap.c: zynjacku.defs zynjacku.override
	pygtk-codegen-2.0 --prefix zynjacku \
	--register $(DEFS)/gdk-types.defs \
	--register $(DEFS)/gtk-types.defs \
	--override zynjacku.override \
	zynjacku.defs > $@

zynjacku.so: $(OBJECTS)
	ld -shared $(OBJECTS) -o $@ $(LDFLAGS)

%.o:%.c
	$(CC) $(CFLAGS) $< -o $@

%.d:%.c
	$(GENDEP_C)

clean:
	-@rm $(OBJECTS) zynjacku_wrap.c zynjacku.so zynjacku.defs *.pyc

run: zynjacku.so
	./zynjacku.py

# All object and dependency files depend on this file
$(OBJECTS) $(DEP_FILES): GNUmakefile

-include $(DEP_FILES)
