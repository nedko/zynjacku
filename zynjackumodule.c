/* -*- Mode: C ; c-basic-offset: 2 -*- */

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
