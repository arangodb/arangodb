
%option reentrant
%option 8bit
%option prefix="Aql"
%option bison-locations
%option bison-bridge
%option yylineno
%option noyywrap nounput batch
%x BACKTICK
%x SINGLE_QUOTE
%x DOUBLE_QUOTE
%x COMMENT_SINGLE
%x COMMENT_MULTI

%{
#include "Basics/Common.h"

// introduce the namespace here, otherwise following references to 
// the namespace in auto-generated headers might fail

namespace triagens {
  namespace aql {
    class Query;
    class Parser;
  }
}


#include "Aql/AstNode.h"
#include "Aql/grammar.h"
#include "Aql/Parser.h"

#define YY_EXTRA_TYPE triagens::aql::Parser*

#define YY_USER_ACTION yylloc->first_line = yylineno; yylloc->first_column = yycolumn; yylloc->last_column = (int) (yycolumn + yyleng - 1); yycolumn += (int) yyleng; yyextra->increaseOffset(yyleng);

#define YY_NO_INPUT 1

#define YY_INPUT(resultBuffer, resultState, maxBytesToRead) {            \
  size_t length = yyextra->remainingLength();                            \
  if (length > static_cast<size_t>(maxBytesToRead)) {                    \
    length = static_cast<size_t>(maxBytesToRead);                        \
  }                                                                      \
  if (length > 0) {                                                      \
    yyextra->fillBuffer(resultBuffer, length);                           \
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

(?i:NOT) {
  return T_NOT;
}

(?i:AND) {
  return T_AND;
}

(?i:OR) {
  return T_OR;
}

(?i:IN) {
  return T_IN;
}

(?i:INTO) {
  return T_INTO;
}

(?i:WITH) {
  return T_WITH;
}

(?i:REMOVE) {
  return T_REMOVE;
}

(?i:INSERT) {
  return T_INSERT;
}

(?i:UPDATE) {
  return T_UPDATE;
}

(?i:REPLACE) {
  return T_REPLACE;
}

 /* ---------------------------------------------------------------------------
  * predefined type literals
  * --------------------------------------------------------------------------- */

(?i:NULL) {
  return T_NULL;
}

(?i:TRUE) {
  return T_TRUE;
}

(?i:FALSE) {
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

"::" {
  return T_SCOPE;
}

":" {
  return T_COLON;
}

"[*]" {
  return T_EXPAND;
}

".." {
  return T_RANGE; 
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
  return T_OBJECT_OPEN;
}

"}" {
  return T_OBJECT_CLOSE;
}

"[" {
  return T_ARRAY_OPEN;
}

"]" {
  return T_ARRAY_CLOSE;
}
 
 /* ---------------------------------------------------------------------------
  * literals
  * --------------------------------------------------------------------------- */

([a-zA-Z][_a-zA-Z0-9]*|_+[a-zA-Z]+[_a-zA-Z0-9]*) { 
  /* unquoted string */
  yylval->strval = yyextra->query()->registerString(yytext, yyleng, false);
  return T_STRING; 
}

<INITIAL>` {
  /* string enclosed in backticks */
  yyextra->marker(yyextra->queryString() + yyextra->offset());
  BEGIN(BACKTICK);
}

<BACKTICK>` {
  /* end of backtick-enclosed string */
  BEGIN(INITIAL);
  yylval->strval = yyextra->query()->registerString(yyextra->marker(), yyextra->offset() - (yyextra->marker() - yyextra->queryString()) - 1, true);
  return T_STRING;
}

<BACKTICK>\\. {
  /* character escaped by backslash */
  BEGIN(BACKTICK);
}

<BACKTICK>\n {
  /* newline character inside backtick */
  BEGIN(BACKTICK);
}

<BACKTICK>. {
  /* any character (except newline) inside backtick */
  BEGIN(BACKTICK);
}

<INITIAL>\" {
  yyextra->marker(yyextra->queryString() + yyextra->offset());
  BEGIN(DOUBLE_QUOTE);
}

<DOUBLE_QUOTE>\" {
  /* end of quote-enclosed string */
  BEGIN(INITIAL);
  yylval->strval = yyextra->query()->registerString(yyextra->marker(), yyextra->offset() - (yyextra->marker() - yyextra->queryString()) - 1, true);
  return T_QUOTED_STRING;
}

<DOUBLE_QUOTE>\\. {
  /* character escaped by backslash */
  BEGIN(DOUBLE_QUOTE);
}

<DOUBLE_QUOTE>\n {
  /* newline character inside quote */
  BEGIN(DOUBLE_QUOTE);
}

<DOUBLE_QUOTE>. {
  /* any character (except newline) inside quote */
  BEGIN(DOUBLE_QUOTE);
}

<INITIAL>' {
  yyextra->marker(yyextra->queryString() + yyextra->offset());
  BEGIN(SINGLE_QUOTE);
}

<SINGLE_QUOTE>' {
  /* end of quote-enclosed string */
  BEGIN(INITIAL);
  yylval->strval = yyextra->query()->registerString(yyextra->marker(), yyextra->offset() - (yyextra->marker() - yyextra->queryString()) - 1, true);
  return T_QUOTED_STRING;
}

<SINGLE_QUOTE>\\. {
  /* character escaped by backslash */
  BEGIN(SINGLE_QUOTE);
}

<SINGLE_QUOTE>\n {
  /* newline character inside quote */
  BEGIN(SINGLE_QUOTE);
}

<SINGLE_QUOTE>. {
  /* any character (except newline) inside quote */
  BEGIN(SINGLE_QUOTE);
}

(0|[1-9][0-9]*) {  
  /* a numeric integer value */
  yylval->strval = yyextra->query()->registerString(yytext, yyleng, false); 
  return T_INTEGER;
}

(0|[1-9][0-9]*)((\.[0-9]+)?([eE][\-\+]?[0-9]+)?) { 
  /* a numeric double value */
  yylval->strval = yyextra->query()->registerString(yytext, yyleng, false); 
  return T_DOUBLE;
}

 /* ---------------------------------------------------------------------------
  * bind parameters
  * --------------------------------------------------------------------------- */

@@?[a-zA-Z0-9][a-zA-Z0-9_]* {
  /* bind parameters must start with a @
     if followed by another @, this is a collection name parameter */
  yylval->strval = yyextra->query()->registerString(yytext + 1, yyleng - 1, false); 
  return T_PARAMETER;
}

 /* ---------------------------------------------------------------------------
  * whitespace etc.
  * --------------------------------------------------------------------------- */

[ \t\r]+ {
  /* whitespace is ignored */ 
}

[\n] {
  yycolumn = 0;
}

<INITIAL>"//" {
  BEGIN(COMMENT_SINGLE);
}

<COMMENT_SINGLE>\n {
  yylineno++;
  BEGIN(INITIAL);
}

<COMMENT_SINGLE>[^*\n]+ { 
  // eat comment in chunks
}

<INITIAL>"/*" {
  BEGIN(COMMENT_MULTI);
}

<COMMENT_MULTI>"*/" {
  BEGIN(INITIAL);
}

<COMMENT_MULTI>[^*\n]+ { 
  // eat comment in chunks
}

<COMMENT_MULTI>"*" {
  // eat the lone star
}

<COMMENT_MULTI>\n {
  yylineno++;
}

. {
  /* anything else is returned as it is */
  return (int) yytext[0];
}

%%

