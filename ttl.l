%{
#include <Python.h>
#include "ttl_lexer.h"


PyObject *ttl_list = NULL;
static PyObject *tmp_string = NULL;

void yyerror(const char *str)
{
    PyErr_SetString(PyExc_SyntaxError, str);
}

void add_to(const char *str)
{
    PyString_ConcatAndDel(&tmp_string, PyString_FromString(str));
}

void add_token_str(const char *name, const char *value)
{
    PyList_Append(ttl_list, Py_BuildValue("(ss)", name, value));
}

void add_token(const char *name, PyObject *value)
{
    PyList_Append(ttl_list, Py_BuildValue("(sO)", name, value));
}        

%}

%option nounput reentrant
%x C_COMMENT C_LONGSTRING C_STRING

SYMBOL [A-Za-z_-][a-zA-Z0-9_-]*
TRIPLEQUOTE \"\"\"

%%

@prefix { add_token_str("prefix", "@prefix"); }
# BEGIN(C_COMMENT);
{TRIPLEQUOTE} { tmp_string = PyString_FromString(""); BEGIN(C_LONGSTRING); }
\" { tmp_string = PyString_FromString("");  BEGIN(C_STRING); }
\<[^>]*\> { add_token_str("URI_", yytext); }
[+-]?([0-9]+\.[0-9]*|\.[0-9]+)([eE][-+]?[0-9]+)? { add_token_str("float", yytext); }
[+-]?[0-9]+ { add_token("number", PyInt_FromLong(atol(yytext))); }
{SYMBOL}?:{SYMBOL}? { add_token_str("prnot", yytext); } 
{SYMBOL} { add_token_str("symbol", yytext); } 
[.,;\[\]\(\)] { add_token_str(yytext, yytext); } 
[ \t\n\r] ;

. { 
    // TODO concat with yytext
    //printf("yytext = '%u''%u'\n", yytext[0], yytext[1]);
    PyErr_SetString(PyExc_ValueError, "Unexpected characters");
    yyerror("Syntax error");
}

<C_LONGSTRING>{TRIPLEQUOTE} { add_token("string", tmp_string); BEGIN(INITIAL); }
<C_LONGSTRING>[^"]+ add_to(yytext);
<C_LONGSTRING>\" add_to("\"");

<C_STRING>\" { add_token("string", tmp_string); BEGIN(INITIAL); }
<C_STRING>\\\" add_to("\""); 
<C_STRING>[^\\\"\n]+ add_to(yytext);

<C_COMMENT>\n { BEGIN(INITIAL); }
<C_COMMENT>. ;

%%

int yywrap(yyscan_t scanner)
{
  return 1;
}

