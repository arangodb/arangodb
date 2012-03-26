
%option reentrant
%option 8bit
%option prefix="QL"
%option bison-locations
%option bison-bridge
%option yylineno
%option noyywrap nounput batch debug

%{
#include <BasicsC/common.h>
#include <BasicsC/strings.h>

#include "VocBase/query-node.h"
#include "VocBase/query-base.h"
#include "VocBase/query-parse.h"
#include "QL/parser.h"

#define YY_EXTRA_TYPE TRI_query_template_t*

// currently we do not use the positioning information
// #define YY_USER_ACTION yylloc->first_line = yylineno; yylloc->first_column = yycolumn; yylloc->last_column = yycolumn + yyleng - 1; yycolumn += yyleng;

#define YY_NO_INPUT 1

#define YY_INPUT(resultBuffer, resultState, maxBytesToRead) {            \
  TRI_query_parser_t* parser = (yyextra)->_parser;                       \
  int length = parser->_length;                                          \
                                                                         \
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
  * SQL-like keywords
  * --------------------------------------------------------------------------- */

(?i:select) {
  return SELECT;
}

(?i:from) {
  return FROM;
}

(?i:where) {
  return WHERE;
}

(?i:list) {
  return LIST;
}

(?i:join) {
  return JOIN;
}

(?i:inner) {
  return INNER;
}

(?i:left) {
  return LEFT;
}

(?i:right) { 
  return RIGHT;
}

(?i:outer) {
  return OUTER;
}

(?i:on)	{
  return ON;
}

(?i:in)	{
  return IN;
}

(?i:order) {
  return ORDER;
}

(?i:by) {
  return BY;
}

(?i:asc) {
  return ASC;
}

(?i:desc) {
  return DESC;
}

(?i:limit) {
  return LIMIT;
}


 /* ---------------------------------------------------------------------------
  * type keywords
  * --------------------------------------------------------------------------- */

(?i:null) {
  return NULLX;
}

(?i:undefined) { 
  return UNDEFINED;
}

(?i:true) {
  return TRUE;
}

(?i:false) {
  return FALSE;
}

 
 /* ---------------------------------------------------------------------------
  * other keywords
  * --------------------------------------------------------------------------- */

(?i:within) {
  return WITHIN;
}

(?i:near) {
  return NEAR;
}


 /* ---------------------------------------------------------------------------
  * logical operators 
  * --------------------------------------------------------------------------- */

"&&" {
  return AND;
}

"||" {
  return OR;
}

"!" {
  return NOT;
}


 /* ---------------------------------------------------------------------------
  * ternary operator
  * --------------------------------------------------------------------------- */

"?" {
  return TERNARY;
}

":" {
  return COLON;
}


 /* ---------------------------------------------------------------------------
  * comparison operators 
  * --------------------------------------------------------------------------- */

"!=" {
  return UNEQUAL;
}

"==" { 
  return EQUAL;
}

"<>" { 
  return UNEQUAL;
}

"<=" { 
  return LESS_EQUAL;
}

">=" { 
  return GREATER_EQUAL;
}

"<" {
  return LESS;
}

">" {
  return GREATER;
}


 /* ---------------------------------------------------------------------------
  * assignment
  * --------------------------------------------------------------------------- */

"=" {
  return ASSIGNMENT;
}


 /* ---------------------------------------------------------------------------
  * literals
  * --------------------------------------------------------------------------- */

([a-zA-Z][_a-zA-Z0-9]*|_+[a-zA-Z]+[_a-zA-Z0-9]*) { 
  yylval->strval = TRI_ParseQueryAllocString(yyextra, yytext); 
  return IDENTIFIER; 
}

`(\\.|[^\\`])*` {
  yylval->strval = TRI_ParseQueryAllocString(yyextra, yytext); 
  return QUOTED_IDENTIFIER;
}

(0|[1-9][0-9]*)(\.[0-9]+([eE]([\-\+])?[0-9]+)?)? {  
  yylval->strval = TRI_ParseQueryAllocString(yyextra, yytext); 
  return REAL; 
}

\"(\\.|[^\\\"])*\" {
  yylval->strval = TRI_ParseQueryAllocString(yyextra, yytext); 
  return STRING;
}

'(\\.|[^\\'])*' {
  yylval->strval = TRI_ParseQueryAllocString(yyextra, yytext); 
  return STRING;
}


 /* ---------------------------------------------------------------------------
  * parameters
  * --------------------------------------------------------------------------- */

@(0|[1-9][0-9]*) {
  yylval->strval = TRI_ParseQueryAllocString(yyextra, yytext + 1); 
  return PARAMETER;
}

@[a-zA-Z][a-zA-Z0-9_]*@ {
  yylval->strval = TRI_ParseQueryAllocString2(yyextra, yytext + 1 , strlen(yytext) - 2);
  return PARAMETER_NAMED;
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

