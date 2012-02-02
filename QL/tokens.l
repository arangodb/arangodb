
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

#include "ast-node.h"
#include "parser-context.h"
#include "parser.h"

#define YY_EXTRA_TYPE QL_parser_context_t*
#define YY_USER_ACTION yylloc->first_line = yylineno; yylloc->first_column = yycolumn; yylloc->last_column = yycolumn + yyleng - 1; yycolumn += yyleng;

#define YY_NO_INPUT 1

#define YY_INPUT(resultBuffer,resultState,maxBytesToRead) {          \
  int length = (yyextra)->_lexState._length;                         \
                                                                     \
  if (length > maxBytesToRead) {                                     \
    length = maxBytesToRead;                                         \
  }                                                                  \
  if (length > 0) {                                                  \
    memcpy(resultBuffer, (yyextra)->_lexState._buffer,length);       \
    (yyextra)->_lexState._buffer += length;                          \
    (yyextra)->_lexState._length -= length;                          \
    resultState = length;                                            \
  }                                                                  \
  else {                                                             \
    resultState = YY_NULL;                                           \
  }                                                                  \
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

"===" {
  return IDENTICAL;
}

"!==" {
  return UNIDENTICAL;
}

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

[_a-zA-Z][_a-zA-Z0-9]* { 
  yylval->strval = QLParseAllocString(yyextra, yytext); 
  return IDENTIFIER; 
}

(0|[1-9][0-9]*)(\.[0-9]+([eE]([\-\+])?[0-9]+)?)? {  
  yylval->strval = QLParseAllocString(yyextra, yytext); 
  return REAL; 
}

\"(\\.|[^\\\"])*\" {
  yylval->strval = QLParseAllocString(yyextra, yytext); 
  return STRING;
}

'(\\.|[^\\'])*' {
  yylval->strval = QLParseAllocString(yyextra, yytext); 
  return STRING;
}
 

 /* ---------------------------------------------------------------------------
  * parameters
  * --------------------------------------------------------------------------- */

@(0|[1-9][0-9]*) {
  yylval->strval = QLParseAllocString(yyextra, yytext + 1); 
  return PARAMETER;
}

@[a-zA-Z][a-zA-Z0-9_]+@ {
  yylval->strval = QLParseAllocString2(yyextra, yytext + 1 , strlen(yytext) - 2);
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

