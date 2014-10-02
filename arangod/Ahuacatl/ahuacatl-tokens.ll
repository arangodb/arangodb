
%option reentrant
%option 8bit
%option prefix="Ahuacatl"
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

#include "Basics/tri-strings.h"

#include "Ahuacatl/ahuacatl-ast-node.h"
#include "Ahuacatl/ahuacatl-context.h"
#include "Ahuacatl/ahuacatl-grammar.h"
#include "Ahuacatl/ahuacatl-parser.h"

#define YY_EXTRA_TYPE TRI_aql_context_t*

#define YY_USER_ACTION yylloc->first_line = yylineno; yylloc->first_column = yycolumn; yylloc->last_column = (int) (yycolumn + yyleng - 1); yycolumn += (int) yyleng; yyextra->_parser->_offset += yyleng;

#define YY_NO_INPUT 1

#define YY_INPUT(resultBuffer, resultState, maxBytesToRead) {            \
  TRI_aql_parser_t* parser = (TRI_aql_parser_t*) (yyextra)->_parser;     \
  size_t length = parser->_length;                                       \
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
  /* unquoted string */
  yylval->strval = TRI_RegisterStringAql(yyextra, yytext, yyleng, false); 
  return T_STRING; 
}

<INITIAL>` {
  /* string enclosed in backticks */
  yyextra->_parser->_marker = (char*) (yyextra->_query + yyextra->_parser->_offset);
  BEGIN(BACKTICK);
}

<BACKTICK>` {
  /* end of backtick-enclosed string */
  BEGIN(INITIAL);
  yylval->strval = TRI_RegisterStringAql(yyextra, yyextra->_parser->_marker, yyextra->_parser->_offset - (yyextra->_parser->_marker - yyextra->_query) - 1, true);
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
  yyextra->_parser->_marker = (char*) (yyextra->_query + yyextra->_parser->_offset);
  BEGIN(DOUBLE_QUOTE);
}

<DOUBLE_QUOTE>\" {
  /* end of quote-enclosed string */
  BEGIN(INITIAL);
  yylval->strval = TRI_RegisterStringAql(yyextra, yyextra->_parser->_marker, yyextra->_parser->_offset - (yyextra->_parser->_marker - yyextra->_query) - 1, true);
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
  yyextra->_parser->_marker = (char*) (yyextra->_query + yyextra->_parser->_offset);
  BEGIN(SINGLE_QUOTE);
}

<SINGLE_QUOTE>' {
  /* end of quote-enclosed string */
  BEGIN(INITIAL);
  yylval->strval = TRI_RegisterStringAql(yyextra, yyextra->_parser->_marker, yyextra->_parser->_offset - (yyextra->_parser->_marker - yyextra->_query) - 1, true);
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
  yylval->strval = TRI_RegisterStringAql(yyextra, yytext, yyleng, false); 
  return T_INTEGER;
}

(0|[1-9][0-9]*)(\.[0-9]+([eE]([\-\+])?[0-9]+)?) {  
  /* a numeric double value */
  yylval->strval = TRI_RegisterStringAql(yyextra, yytext, yyleng, false); 
  return T_DOUBLE;
}

 /* ---------------------------------------------------------------------------
  * bind parameters
  * --------------------------------------------------------------------------- */

@@?[a-zA-Z0-9][a-zA-Z0-9_]* {
  /* bind parameters must start with a @
     if followed by another @, this is a collection name parameter */
  yylval->strval = TRI_RegisterStringAql(yyextra, yytext + 1, yyleng - 1, false); 
  return T_PARAMETER;
}

 /* ---------------------------------------------------------------------------
  * whitespace etc.
  * --------------------------------------------------------------------------- */

[ \t\r\n]+ {
  /* whitespace is ignored */ 
}

<INITIAL>"//" {
  BEGIN(COMMENT_SINGLE);
}

<COMMENT_SINGLE>\r?\n {
  yylineno++;
  BEGIN(INITIAL);
}

<COMMENT_SINGLE>\r {
  yylineno++;
  BEGIN(INITIAL);
}

<COMMENT_SINGLE>[^*\r\n]+ { 
  // eat comment in chunks
}

<INITIAL>"/*" {
  BEGIN(COMMENT_MULTI);
}

<COMMENT_MULTI>"*/" {
  BEGIN(INITIAL);
}

<COMMENT_MULTI>[^*\r\n]+ { 
  // eat comment in chunks
}

<COMMENT_MULTI>"*" {
  // eat the lone star
}

<COMMENT_MULTI>\r?\n {
  yylineno++;
}

<COMMENT_MULTI>\r {
  yylineno++;
}

 
. {
  /* anything else is returned as it is */
  return (int) yytext[0];
}

%%

