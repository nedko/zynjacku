/* -*- Mode: C ; c-basic-offset: 2 -*- */
/*****************************************************************************
 *
 *   This file is part of zynjacku
 *
 *   Copyright (C) 2006,2007 Nedko Arnaudov <nedko@arnaudov.name>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; version 2 of the License
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 *****************************************************************************/

#include <pygobject.h>
 
void zynjacku_register_classes(PyObject * obj);
extern PyMethodDef zynjacku_functions[];
 
DL_EXPORT(void)

initzynjacku(void)
{
  PyObject * m;
  PyObject * d;

  init_pygobject();
 
  m = Py_InitModule("zynjacku", zynjacku_functions);

  d = PyModule_GetDict(m);
 
  zynjacku_register_classes(d);
 
  if (PyErr_Occurred())
  {
    Py_FatalError("can't initialise module zynjacku");
  }
}
