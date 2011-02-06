#include <Python.h>
#include "flex_ttl.h"
#include "ttl_lexer.h"

//////////////////////////////////////////////////// calfpytools

static PyObject * scan_file(PyObject *self, PyObject *args)
{
    char *ttl_name = NULL;
    PyObject *tmp = NULL;
    yyscan_t scanner;
    if (!PyArg_ParseTuple(args, "s:scan_file", &ttl_name))
        return NULL;
    
    tmp = ttl_list = PyList_New(0);
    yylex_init(&scanner);
    yyset_in(fopen(ttl_name, "r"), scanner);
    yylex(scanner);
    yylex_destroy(scanner);
    ttl_list = NULL;
    return tmp;
}

static PyObject * scan_string(PyObject *self, PyObject *args)
{
    char *ttl_text = NULL;
    PyObject *tmp = NULL;
    yyscan_t scanner;
    YY_BUFFER_STATE buffer;

    if (!PyArg_ParseTuple(args, "s:scan_string", &ttl_text))
        return NULL;

    tmp = ttl_list = PyList_New(0);
    yylex_init(&scanner);
    buffer = yy_scan_string(ttl_text, scanner);
    yylex(scanner);
    yy_delete_buffer(buffer, scanner);
    yylex_destroy(scanner);
    ttl_list = NULL;
    return tmp;
}

static PyMethodDef module_methods[] = {
    {"scan_file", scan_file, METH_VARARGS, "Scan a TTL file, return a list of token tuples"},
    {"scan_string", scan_string, METH_VARARGS, "Scan a string, return a list of token tuples"},
    {NULL, NULL, 0, NULL}
};

PyMODINIT_FUNC initzynjacku_ttl()
{
  Py_InitModule3("zynjacku_ttl", module_methods, "Flex Turtle parser");
}
