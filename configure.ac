# -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.
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

AC_PREREQ(2.61)
AC_INIT(zynjacku, dev)
AC_CONFIG_AUX_DIR(config)
AM_INIT_AUTOMAKE
AC_CONFIG_HEADER([config.h])

AM_CONDITIONAL(NOT_TOP_BUILD, test "$srcdir" != ".")

if test -d $srcdir/.git ; then dev_version="yes" ; else dev_version="no"; fi
AM_CONDITIONAL(DEV_VERSION, test "$dev_version" = "yes")

#AS_AC_EXPAND(DATADIR, $datadir)
AC_SUBST(DATADIR)

# Checks for programs.
AC_PROG_CC
AC_DISABLE_STATIC
AC_PROG_LIBTOOL

# Checks for libraries.

PKG_CHECK_MODULES(LV2, lv2core >= 1)
PKG_CHECK_MODULES(GTK, gtk+-2.0)
PKG_CHECK_MODULES(PYGTK, pygtk-2.0)
PKG_CHECK_MODULES(JACK, jack)
PKG_CHECK_MODULES(LV2DYNPARAMHOST1, lv2dynparamhost1 = 2, HAVE_DYNPARAMS="yes", HAVE_DYNPARAMS="no"; AC_MSG_WARN([$LV2DYNPARAMHOST1_PKG_ERRORS]))
if test "$HAVE_DYNPARAMS" = "yes"
then
  AC_DEFINE([HAVE_DYNPARAMS], 1, [Defined if dynparam1 is present.])
fi
AM_CONDITIONAL(HAVE_DYNPARAMS, test "$HAVE_DYNPARAMS" = "yes")

# JACK MIDI
have_jackmidi="unknown"
AC_ARG_ENABLE(jackmidi, [AS_HELP_STRING(--disable-jackmidi, [Force disable JACK MIDI support [default=no]])], [ have_jackmidi="no" ])
if test "$have_jackmidi" = "unknown"
then
  PKG_CHECK_MODULES(JACK_MIDI, jack >= 0.102.0, AC_DEFINE([HAVE_JACK_MIDI], [], [Defined if we have JACK MIDI support.]) have_jackmidi="yes")
  if test "$have_jackmidi" = "yes"
  then
    PKG_CHECK_MODULES(OLD_JACK_MIDI, jack < 0.102.27, have_jackmidi="yes (old one)" AC_DEFINE(HAVE_OLD_JACK_MIDI, 1, [whether or not we building against old JACK MIDI (0.102.20)]), echo -n)
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

# Allow the user to override AM_PATH_PYTHON choice of PYTHON_PREFIX
# and PYTHON_EXEC_PREFIX by setting PYLIBDIR
AC_ARG_VAR(PYLIBDIR, [Override default Python site-packages dir])
if test "$PYLIBDIR" ; then
   pylibdir=$PYLIBDIR
   AC_MSG_NOTICE([User set PYLIBDIR... $pylibdir])
   AC_SUBST([pythondir],$pylibdir)
   AC_SUBST([pyexecdir],$pylibdir)
fi

# Warn if python does not search in the installed places
#AC_MSG_NOTICE([Checking to see whether Python will find installed files...])

# Figure out the script directory
if test "$prefix" == 'NONE'; then
  #AC_MSG_NOTICE([prefix not set, falling back to ac_default_prefix...])
  pyprefix=$ac_default_prefix
else
  pyprefix=$prefix
fi
abs_pythondir=`echo $pythondir | $SED s='${prefix}'=$pyprefix=`
#AC_MSG_NOTICE([Python script dir will be: $abs_pythondir])
match_pythondir=no
for p in `$PYTHON -c "import sys
for p in sys.path:
  print p"`
do
  #AC_MSG_NOTICE([Checking Python sys.path entry $p ...])
  if test $p == $abs_pythondir ; then
    match_pythondir=
  fi
done
AC_CONFIG_FILES([Makefile])
AC_OUTPUT

AC_MSG_RESULT([])
AC_MSG_RESULT([========================])
AC_MSG_RESULT([Prefix                    : $prefix])
AC_MSG_RESULT([Dynparam1 plugins support : $HAVE_DYNPARAMS])
AC_MSG_RESULT([Python include dir        : $PYTHON_INCLUDES])
AC_MSG_RESULT([Python script dir         : $abs_pythondir])
AC_MSG_RESULT([Dev version               : $dev_version])
AC_MSG_RESULT([])

if test "$match_pythondir"; then
   AC_MSG_WARN([==================================================])
   AC_MSG_WARN([==                                              ==])
   AC_MSG_WARN([== pythondir is NOT in your Python's sys.path   ==])
   AC_MSG_WARN([== which means you should add it to PYTHONPATH  ==])
   AC_MSG_WARN([== at runtime, or add a .pth file to one of     ==])
   AC_MSG_WARN([== your Python's existing sys.path directories. ==])
   AC_MSG_WARN([==                                              ==])
   AC_MSG_WARN([== Alternatively, re-run ./configure with       ==])
   AC_MSG_WARN([== PYLIBDIR set to override the default         ==])
   AC_MSG_WARN([== pythondir, or adjust prefix to match the     ==])
   AC_MSG_WARN([== prefix used to build your Python.            ==])
   AC_MSG_WARN([==                                              ==])
   AC_MSG_WARN([==================================================])
fi
