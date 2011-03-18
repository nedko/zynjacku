%{
#include <Python.h>
#include "ttl_lexer.h"


PyObject *ttl_list = NULL;
static PyObject *tmp_string = NULL;

void unrecognized_char(unsigned char ch, int line)
{
    char buffer[256];
    snprintf(buffer, sizeof(buffer), "unrecognised char 0x%02X", ch);
    PyErr_SetString(PyExc_SyntaxError, buffer);
    PyErr_SyntaxLocation("", line);
}

void add_to(const char *str)
{
    PyString_ConcatAndDel(&tmp_string, PyString_FromString(str));
}

void add_token_str(const char *name, const char *value)
{
    //printf("add_token_str: name='%s', value='%s'\n", name, value);
    PyList_Append(ttl_list, Py_BuildValue("(ss)", name, value));
}

void add_token(const char *name, PyObject *value)
{
    PyList_Append(ttl_list, Py_BuildValue("(sO)", name, value));
}        

%}

%option nounput reentrant yylineno
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
[+-]?[0-9]+[eE][-+]?[0-9]+ { add_token_str("float", yytext); }
[+-]?[0-9]+ { add_token("number", PyInt_FromLong(atol(yytext))); }
{SYMBOL}?:{SYMBOL}? { add_token_str("prnot", yytext); } 
{SYMBOL} { add_token_str("symbol", yytext); } 
[.,;\[\]\(\)] { add_token_str(yytext, yytext); } 
[ \t\n\r] ;
\^{2} { add_token_str("datatype_URI", yytext); } 
@ { add_token_str("language", yytext); } 

. { unrecognized_char(*yytext, yylineno); }

<C_LONGSTRING>{TRIPLEQUOTE} { add_token("string", tmp_string); BEGIN(INITIAL); }
<C_LONGSTRING>[^"]+ add_to(yytext);
<C_LONGSTRING>\" add_to("\"");

<C_STRING>\" { add_token("string", tmp_string); BEGIN(INITIAL); }
<C_STRING>\\\" add_to("\""); 
<C_STRING>\\u([0-9A-F]){4} add_to(yytext);
<C_STRING>\\U([0-9A-F]){8} add_to(yytext);
<C_STRING>\\[tnr] add_to(yytext);
<C_STRING>[^\\\"\n]+ add_to(yytext);
<C_STRING>. { unrecognized_char(*yytext, yylineno); }

<C_COMMENT>\n { BEGIN(INITIAL); }
<C_COMMENT>. ;

%%

int yywrap(yyscan_t scanner)
{
  return 1;
}

