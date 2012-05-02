
%option reentrant
%option 8bit
%option prefix="Ahuacatl"
%option bison-locations
%option bison-bridge
%option yylineno
%option noyywrap nounput batch debug

%{
#include <BasicsC/common.h>
#include <BasicsC/strings.h>

#include "Ahuacatl/ahuacatl-ast-node.h"
#include "Ahuacatl/ahuacatl-context.h"
#include "Ahuacatl/ahuacatl-grammar.h"
#include "Ahuacatl/ahuacatl-parser.h"

#define YY_EXTRA_TYPE TRI_aql_context_t*

#define YY_USER_ACTION yylloc->first_line = yylineno; yylloc->first_column = yycolumn; yylloc->last_column = yycolumn + yyleng - 1; yycolumn += yyleng;

#define YY_NO_INPUT 1

#define YY_INPUT(resultBuffer, resultState, maxBytesToRead) {            \
  TRI_aql_parser_t* parser = (TRI_aql_parser_t*) (yyextra)->_parser;     \
  int length = parser->_length;                                          \
  if (length > maxBytesToRead) {                                         \
    length = maxBytesToRead;                                             \
  }                                                                      \
  if (length > 0) {                                                      \
    memcpy(resultBuffer, parser->_buffer, length);                       \
    parser->_buffer += length;                                           \
    parser->_length -= length;                                           \
    resultState = length;                                                \
  }                                                                      \
  else {                                                                 \
    resultState = YY_NULL;                                               \
  }                                                                      \
}
%}

%%

 /* ---------------------------------------------------------------------------
  * language keywords
  * --------------------------------------------------------------------------- */

(?i:FOR) {
  return T_FOR;
}

(?i:LET) {
  return T_LET;
}

(?i:FILTER) {
  return T_FILTER;
}

(?i:RETURN) {
  return T_RETURN;
}

(?i:COLLECT) {
  return T_COLLECT;
}

(?i:SORT) {
  return T_SORT;
}

(?i:LIMIT) { 
  return T_LIMIT;
}

(?i:ASC) {
  return T_ASC;
}

(?i:DESC) {
  return T_DESC;
}

(?i:IN) {
  return T_IN;
}

(?i:INTO) {
  return T_INTO;
}

 /* ---------------------------------------------------------------------------
  * predefined type literals
  * --------------------------------------------------------------------------- */

(?i:null) {
  return T_NULL;
}

(?i:true) {
  return T_TRUE;
}

(?i:false) {
  return T_FALSE;
}
 
 /* ---------------------------------------------------------------------------
  * operators
  * --------------------------------------------------------------------------- */

"==" {
  return T_EQ;
}

"!=" {
  return T_NE;
}

">=" {
  return T_GE;
}

">" {
  return T_GT;
}

"<=" {
  return T_LE;
}

"<" {
  return T_LT;
}

"=" {
  return T_ASSIGN;
}

"!" {
  return T_NOT;
}

"&&" {
  return T_AND;
}

"||" {
  return T_OR;
}

"+" {
  return T_PLUS;
}

"-" {
  return T_MINUS;
}

"*" {
  return T_TIMES;
}

"/" {
  return T_DIV;
}

"%" {
  return T_MOD;
}

"?" {
  return T_QUESTION;
}

":" {
  return T_COLON;
}
 
 /* ---------------------------------------------------------------------------
  * punctuation
  * --------------------------------------------------------------------------- */

"," {
  return T_COMMA;
}

"(" {
  return T_OPEN;
}

")" {
  return T_CLOSE;
}

"{" {
  return T_DOC_OPEN;
}

"}" {
  return T_DOC_CLOSE;
}

"[" {
  return T_LIST_OPEN;
}

"]" {
  return T_LIST_CLOSE;
}

 /* ---------------------------------------------------------------------------
  * literals
  * --------------------------------------------------------------------------- */

([a-zA-Z][_a-zA-Z0-9]*|_+[a-zA-Z]+[_a-zA-Z0-9]*) { 
  yylval->strval = TRI_RegisterStringAql(yyextra, yytext, strlen(yytext), false); 
  return T_STRING; 
}

`(\\.|[^\\`])*` {
  yylval->strval = TRI_RegisterStringAql(yyextra, yytext + 1, strlen(yytext) - 2, true); 
  return T_STRING;
}

\"(\\.|[^\\\"])*\" {
  yylval->strval = TRI_RegisterStringAql(yyextra, yytext + 1, strlen(yytext) - 2, true); 
  return T_QUOTED_STRING;
}

'(\\.|[^\\'])*' {
  yylval->strval = TRI_RegisterStringAql(yyextra, yytext + 1, strlen(yytext) - 2, true); 
  return T_QUOTED_STRING;
}

(0|[1-9][0-9]*)(\.[0-9]+([eE]([\-\+])?[0-9]+)?)? {  
  yylval->strval = TRI_RegisterStringAql(yyextra, yytext, strlen(yytext), false); 
  return T_NUMBER;
}

 /* ---------------------------------------------------------------------------
  * bind parameters
  * --------------------------------------------------------------------------- */

@[a-zA-Z][a-zA-Z0-9_]* {
  yylval->strval = TRI_RegisterStringAql(yyextra, yytext + 1, strlen(yytext) - 1, false); 
  return T_PARAMETER;
}

 /* ---------------------------------------------------------------------------
  * whitespace etc.
  * --------------------------------------------------------------------------- */

[ \t\r\n]+ {
  /* whitespace */;
}

. {
  return (int) yytext[0];
}

%%

