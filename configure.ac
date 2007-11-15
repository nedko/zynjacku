# -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.
#
# This file is part of zynjacku
#
# Copyright (C) 2007 Nedko Arnaudov <nedko@arnaudov.name>
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

AC_PREREQ(2.61)
AC_INIT(zynjacku, dev)
AC_CONFIG_AUX_DIR(config)
AM_INIT_AUTOMAKE
AC_CONFIG_HEADER([config.h])

#AS_AC_EXPAND(DATADIR, $datadir)
AC_SUBST(DATADIR)

# Checks for programs.
AC_PROG_CC
AC_DISABLE_STATIC
AC_PROG_LIBTOOL

# Checks for libraries.

PKG_CHECK_MODULES(GTK, gtk+-2.0)
PKG_CHECK_MODULES(PYGTK, pygtk-2.0)
PKG_CHECK_MODULES(JACK, jack)
PKG_CHECK_MODULES(LV2DYNPARAMHOST, lv2dynparamhost-1)
PKG_CHECK_MODULES(SLV2, libslv2 >= 0.0.1)

# JACK MIDI
have_jackmidi="unknown"
AC_ARG_ENABLE(jackmidi, [AS_HELP_STRING(--disable-jackmidi, [Force disable JACK MIDI support [default=no]])], [ have_jackmidi="no" ])
if test "$have_jackmidi" = "unknown"
then
  PKG_CHECK_MODULES(JACK_MIDI, jack >= 0.102.0, AC_DEFINE([HAVE_JACK_MIDI], [], [Defined if we have JACK MIDI support.]) have_jackmidi="yes")
  if test "$have_jackmidi" = "yes"
  then
    PKG_CHECK_MODULES(OLD_JACK_MIDI, jack < 0.102.27, have_jackmidi="yes (old one)" AC_DEFINE(HAVE_OLD_JACK_MIDI, 1, [whether or not we building against old JACK MIDI (0.102.20)]), echo no)
  fi
fi

AM_CONDITIONAL(HAVE_JACK_MIDI, test "$have_jackmidi" = "yes" -o "$have_jackmidi" = "yes (old one)")

if test "$have_jackmidi" != "yes" -a "$have_jackmidi" != "yes (old one)"
then
  AC_MSG_ERROR([You need JACK version with MIDI support.])
else
  AC_JACK_MIDI_NFRAMES_CHECK()
  if test "$jackmidi_nframes" = "yes"
  then
    AC_DEFINE([JACK_MIDI_NEEDS_NFRAMES], 1, [Defined if we JACK MIDI functions need nframes parameter.])
  fi
fi

# Python checking
AM_PATH_PYTHON(2.4)
AM_CHECK_PYTHON_HEADERS(,[AC_MSG_ERROR(Could not find Python headers)])
#AS_AC_EXPAND(PYTHONDIR, $pythondir)
#AC_SUBST(PYTHONDIR)

AC_ARG_ENABLE(pymod-checks, [AS_HELP_STRING(--disable-pymod-checks, [Force disable checks for Python modules required at run time])], [ disable_pymod_checks="yes" ], [ disable_pymod_checks="no" ])

if test "$disable_pymod_checks" != "yes"
then
  AM_CHECK_PYMOD(phat,,,[AC_MSG_ERROR(Could not find PyPHAT)])
  # We cannot check for pyphat version, check phat version instead
  PKG_CHECK_MODULES(PHAT, phat >= 0.4.1)
else
  AC_MSG_WARN([Checks for python modules required runtime have been force disabled])
fi

# LASH
have_lash="unknown"
AC_ARG_ENABLE(lash, [AS_HELP_STRING(--disable-lash, [Force disable LASH support [default=no]])], [ have_lash="no" ])
if test "$have_lash" = "unknown"
then
  PKG_CHECK_MODULES(LASH, lash-1.0, AC_DEFINE([HAVE_LASH], [], [Defined if we have LASH support.]) have_lash="yes")
fi

have_lash="no (force disabled, not ready yet)"

AC_CONFIG_FILES([Makefile])
AC_OUTPUT

AC_MSG_RESULT([])
AC_MSG_RESULT([**********************************************************************])
AC_MSG_RESULT([])
AC_MSG_RESULT([Prefix:            $prefix])
AC_MSG_RESULT([])
AC_MSG_RESULT([Python dir:      $PYTHON_INCLUDES])
AC_MSG_RESULT([])
AC_MSG_RESULT([LASH support:      $have_lash])
AC_MSG_RESULT([])
AC_MSG_RESULT([**********************************************************************])
AC_MSG_RESULT([])