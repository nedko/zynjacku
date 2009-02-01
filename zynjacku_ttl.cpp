#include <Python.h>
#include "flex_ttl.h"
#include "ttl_lexer.h"
#include <list>
#include <map>
#include <iostream>
#include <fstream>

//////////////////////////////////////////////////// calfpytools

static PyObject * scan_file(PyObject *self, PyObject *args)
{
    char *ttl_name = NULL;
    if (!PyArg_ParseTuple(args, "s:scan_file", &ttl_name))
        return NULL;
    
    std::ifstream istr(ttl_name, std::ifstream::in);
    TTLLexer lexer(&istr);
    lexer.yylex();
    return lexer.grab();
}

static PyObject *scan_string(PyObject *self, PyObject *args)
{
    char *data = NULL;
    if (!PyArg_ParseTuple(args, "s:scan_string", &data))
        return NULL;
    
    std::string data_str = data;
    std::stringstream str(data_str);
    TTLLexer lexer(&str);
    lexer.yylex();
    return lexer.grab();
}

static PyMethodDef module_methods[] = {
    {"scan_file", scan_file, METH_VARARGS, "Scan a TTL file, return a list of token tuples"},
    {"scan_string", scan_string, METH_VARARGS, "Scan a TTL string, return a list of token tuples"},
    {NULL, NULL, 0, NULL}
};

PyMODINIT_FUNC initzynjacku_ttl()
{
  Py_InitModule3("zynjacku_ttl", module_methods, "Flex Turtle parser");
}
