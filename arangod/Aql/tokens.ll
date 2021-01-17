%option reentrant
%option 8bit
%option prefix="Aql"
%option bison-locations
%option bison-bridge
%option yylineno
%option noyywrap nounput batch
%x BACKTICK
%x FORWARDTICK
%x SINGLE_QUOTE
%x DOUBLE_QUOTE
%x COMMENT_SINGLE
%x COMMENT_MULTI

%top{
#include <stdint.h>
#if (_MSC_VER >= 1)
// fix ret_val = EOB_ACT_LAST_MATCH later on, its generated, we can't control this.
#pragma warning( disable : 4267)
#endif
}

%{
#include "Basics/Common.h"
#include "Basics/NumberUtils.h"
#include "Basics/conversions.h"
#include "Basics/operating-system.h"

#if _WIN32
#include "Basics/win-utils.h"
#endif

// introduce the namespace here, otherwise following references to
// the namespace in auto-generated headers might fail

namespace arangodb {
namespace aql {
class QueryContext;
class Parser;
}
}

#include "Aql/AstNode.h"
#include "Aql/grammar.h"
#include "Aql/Functions.h"
#include "Aql/Parser.h"
#include "Aql/QueryContext.h"

#include <algorithm>

#define YY_EXTRA_TYPE arangodb::aql::Parser*

#define YY_USER_ACTION                                                   \
  yylloc->first_line = static_cast<int>(yylineno);                       \
  yylloc->first_column = static_cast<int>(yycolumn);                     \
  yylloc->last_column = static_cast<int>(yycolumn + yyleng - 1);         \
  yycolumn += static_cast<int>(yyleng);                                  \
  yyextra->increaseOffset(yyleng);

#define YY_NO_INPUT 1

#define YY_INPUT(resultBuffer, resultState, maxBytesToRead) {            \
  size_t length = std::min(yyextra->remainingLength(), static_cast<size_t>(maxBytesToRead));  \
  if (length > 0) {                                                      \
    yyextra->fillBuffer(resultBuffer, length);                           \
    resultState = length;                                                \
  } else {                                                               \
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

(?i:WINDOW) {
  return T_WINDOW;
}

(?i:DISTINCT) {
  return T_DISTINCT;
}

(?i:AGGREGATE) {
  return T_AGGREGATE;
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

(?i:UPSERT) {
  return T_UPSERT;
}

(?i:GRAPH) {
  return T_GRAPH;
}

(?i:SHORTEST_PATH) {
  return T_SHORTEST_PATH;
}

(?i:K_SHORTEST_PATHS) {
  return T_K_SHORTEST_PATHS;
}

(?i:K_PATHS) {
  return T_K_PATHS;
}

(?i:OUTBOUND) {
  return T_OUTBOUND;
}

(?i:INBOUND) {
  return T_INBOUND;
}

(?i:ANY) {
  return T_ANY;
}

(?i:ALL) {
  return T_ALL;
}

(?i:NONE) {
  return T_NONE;
}

(?i:LIKE) {
  return T_LIKE;
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

"=~" {
  return T_REGEX_MATCH;
}

"!~" {
  return T_REGEX_NON_MATCH;
}

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
  * identifiers
  * --------------------------------------------------------------------------- */

($?[a-zA-Z][_a-zA-Z0-9]*|_+[a-zA-Z]+[_a-zA-Z0-9]*) {
  /* unquoted string */
  yylval->strval.value = yyextra->ast()->resources().registerString(yytext, yyleng);
  yylval->strval.length = yyleng;
  return T_STRING;
}

<INITIAL>` {
  /* string enclosed in backticks */
  yyextra->marker(yyextra->queryStringStart() + yyextra->offset());
  BEGIN(BACKTICK);
}

<BACKTICK>` {
  /* end of backtick-enclosed string */
  BEGIN(INITIAL);
  size_t outLength;
  yylval->strval.value = yyextra->ast()->resources().registerEscapedString(yyextra->marker(), yyextra->offset() - (yyextra->marker() - yyextra->queryStringStart()) - 1, outLength);
  yylval->strval.length = outLength;
  return T_STRING;
}

<BACKTICK>\\. {
  /* character escaped by backslash */
}

<BACKTICK>\n {
  /* newline character inside backtick */
}

<BACKTICK><<EOF>> {
  auto parser = yyextra;
  parser->registerParseError(TRI_ERROR_QUERY_PARSE, "unexpected unterminated identifier", yylloc->first_line, yylloc->first_column);
}

<BACKTICK>. {
  /* any character (except newline) inside backtick */
}


<INITIAL>´ {
  /* string enclosed in forwardticks */
  yyextra->marker(yyextra->queryStringStart() + yyextra->offset());
  BEGIN(FORWARDTICK);
}

<FORWARDTICK>´ {
  /* end of forwardtick-enclosed string */
  BEGIN(INITIAL);
  size_t outLength;
  yylval->strval.value = yyextra->ast()->resources().registerEscapedString(yyextra->marker(), yyextra->offset() - (yyextra->marker() - yyextra->queryStringStart()) - 2, outLength);
  yylval->strval.length = outLength;
  return T_STRING;
}

<FORWARDTICK>\\. {
  /* character escaped by backslash */
}

<FORWARDTICK>\n {
  /* newline character inside forwardtick */
}

<FORWARDTICK><<EOF>> {
  auto parser = yyextra;
  parser->registerParseError(TRI_ERROR_QUERY_PARSE, "unexpected unterminated identifier", yylloc->first_line, yylloc->first_column);
}

<FORWARDTICK>. {
  /* any character (except newline) inside forwardtick */
}

 /* ---------------------------------------------------------------------------
  * strings
  * --------------------------------------------------------------------------- */

<INITIAL>\" {
  yyextra->marker(yyextra->queryStringStart() + yyextra->offset());
  BEGIN(DOUBLE_QUOTE);
}

<DOUBLE_QUOTE>\" {
  /* end of quote-enclosed string */
  BEGIN(INITIAL);
  size_t outLength;
  yylval->strval.value = yyextra->ast()->resources().registerEscapedString(yyextra->marker(), yyextra->offset() - (yyextra->marker() - yyextra->queryStringStart()) - 1, outLength);
  yylval->strval.length = outLength;
  return T_QUOTED_STRING;
}

<DOUBLE_QUOTE>\\. {
  /* character escaped by backslash */
}

<DOUBLE_QUOTE>\n {
  /* newline character inside quote */
}

<DOUBLE_QUOTE><<EOF>> {
  auto parser = yyextra;
  parser->registerParseError(TRI_ERROR_QUERY_PARSE, "unexpected unterminated string literal", yylloc->first_line, yylloc->first_column);
}

<DOUBLE_QUOTE>. {
  /* any character (except newline) inside quote */
}

<INITIAL>' {
  yyextra->marker(yyextra->queryStringStart() + yyextra->offset());
  BEGIN(SINGLE_QUOTE);
}

<SINGLE_QUOTE>' {
  /* end of quote-enclosed string */
  BEGIN(INITIAL);
  size_t outLength;
  yylval->strval.value = yyextra->ast()->resources().registerEscapedString(yyextra->marker(), yyextra->offset() - (yyextra->marker() - yyextra->queryStringStart()) - 1, outLength);
  yylval->strval.length = outLength;
  return T_QUOTED_STRING;
}

<SINGLE_QUOTE>\\. {
  /* character escaped by backslash */
}

<SINGLE_QUOTE>\n {
  /* newline character inside quote */
}

<SINGLE_QUOTE><<EOF>> {
  auto parser = yyextra;
  parser->registerParseError(TRI_ERROR_QUERY_PARSE, "unexpected unterminated string literal", yylloc->first_line, yylloc->first_column);
}

<SINGLE_QUOTE>. {
  /* any character (except newline) inside quote */
}

 /* ---------------------------------------------------------------------------
  * number literals
  * --------------------------------------------------------------------------- */

(0|[1-9][0-9]*) {
  /* a numeric integer value, base 10 (decimal) */
  arangodb::aql::AstNode* node = nullptr;
  auto parser = yyextra;

  bool valid;
  int64_t value1 = arangodb::NumberUtils::atoi<int64_t>(yytext, yytext + yyleng, valid);

  if (valid) {
    node = parser->ast()->createNodeValueInt(value1);
  } else {
    // TODO: use std::from_chars
    double value2 = TRI_DoubleString(yytext);

    if (TRI_errno() != TRI_ERROR_NO_ERROR) {
      parser->registerWarning(TRI_ERROR_QUERY_NUMBER_OUT_OF_RANGE, TRI_errno_string(TRI_ERROR_QUERY_NUMBER_OUT_OF_RANGE), yylloc->first_line, yylloc->first_column);
      node = parser->ast()->createNodeValueNull();
    } else {
      node = parser->ast()->createNodeValueDouble(value2);
    }
  }

  yylval->node = node;

  return T_INTEGER;
}

(0[bB][01]+) {
  /* a numeric integer value, base 2 (binary) */
  /* note that we support an arbitrary run of leading zeroes for the actual number */

  /* cut off first 2 characters */
  char const* p = yytext + 2;
  char const* e = yytext + yyleng;

  auto parser = yyextra;
  if (static_cast<uint64_t>(e - p) > arangodb::aql::Functions::bitFunctionsMaxSupportedBits) {
    /* we only support up to 32 bits for now */
    parser->registerParseError(TRI_ERROR_QUERY_PARSE, "binary number literal value exceeds the supported range", yylloc->first_line, yylloc->first_column);
  }
  
  uint64_t result = 0;

  while (p != e) {
    char c = *p;
    if (c == '1') {
      /* only the 1s are interesting for us */
      result += (static_cast<uint64_t>(1) << (e - p - 1));
    }
    ++p;
  }
  
  TRI_ASSERT(result <= UINT32_MAX);
  
  arangodb::aql::AstNode* node = parser->ast()->createNodeValueInt(static_cast<int64_t>(result));
  yylval->node = node;

  return T_INTEGER;
}

(0[xX][0-9a-fA-F]+) {
  /* a numeric integer value, base 16 (hexadecimal) */
  /* note that we support an arbitrary run of leading zeroes for the actual number */

  /* cut off first 2 characters */
  char const* p = yytext + 2;
  char const* e = yytext + yyleng;

  auto parser = yyextra;
  /* each digit 0-9a-f carries 4 bits of information */
  if (static_cast<uint64_t>(e - p) > arangodb::aql::Functions::bitFunctionsMaxSupportedBits / 4) {
    /* we only support up to 32 bits for now */
    parser->registerParseError(TRI_ERROR_QUERY_PARSE, "hex number literal value exceeds the supported range", yylloc->first_line, yylloc->first_column);
  }
  
  uint64_t result = 0;
  
  while (p != e) {
    uint8_t v;
    char c = *p;
    if (c >= 'A' && c <= 'F') {
      v = c - 'A' + 10;
    } else if (c >= 'a' && c <= 'f') {
      v = c - 'a' + 10;
    } else {
      v = c - '0';
    }
    result += (static_cast<uint64_t>(v) << (4 * (e - p - 1)));
    ++p;
  }

  TRI_ASSERT(result <= UINT32_MAX);

  arangodb::aql::AstNode* node = parser->ast()->createNodeValueInt(static_cast<int64_t>(result));
  yylval->node = node;

  return T_INTEGER;
}

((0|[1-9][0-9]*)(\.[0-9]+)?|\.[0-9]+)([eE][\-\+]?[0-9]+)? {
  /* a numeric double value, base 10 (decimal) */

  arangodb::aql::AstNode* node = nullptr;
  auto parser = yyextra;
  // TODO: use std::from_chars
  double value = TRI_DoubleString(yytext);

  if (TRI_errno() != TRI_ERROR_NO_ERROR) {
    parser->registerWarning(TRI_ERROR_QUERY_NUMBER_OUT_OF_RANGE, TRI_errno_string(TRI_ERROR_QUERY_NUMBER_OUT_OF_RANGE), yylloc->first_line, yylloc->first_column);
    node = parser->ast()->createNodeValueNull();
  } else {
    node = parser->ast()->createNodeValueDouble(value);
  }

  yylval->node = node;

  return T_DOUBLE;
}

 /* ---------------------------------------------------------------------------
  * bind parameters
  * --------------------------------------------------------------------------- */

@(_+[a-zA-Z0-9]+[a-zA-Z0-9_]*|[a-zA-Z0-9][a-zA-Z0-9_]*) {
  /* bind parameters must start with a @
     if followed by another @, this is a collection name or a view name parameter */
  yylval->strval.value = yyextra->ast()->resources().registerString(yytext + 1, yyleng - 1);
  yylval->strval.length = yyleng - 1;
  return T_PARAMETER;
}

 /* ---------------------------------------------------------------------------
  * bind data source parameters
  * --------------------------------------------------------------------------- */

@@(_+[a-zA-Z0-9]+[a-zA-Z0-9_]*|[a-zA-Z0-9][a-zA-Z0-9_]*) {
  /* bind parameters must start with a @
     if followed by another @, this is a collection name or a view name parameter */
  yylval->strval.value = yyextra->ast()->resources().registerString(yytext + 1, yyleng - 1);
  yylval->strval.length = yyleng - 1;
  return T_DATA_SOURCE_PARAMETER;
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

 /* ---------------------------------------------------------------------------
  * comments
  * --------------------------------------------------------------------------- */

<INITIAL>"//" {
  BEGIN(COMMENT_SINGLE);
}

<COMMENT_SINGLE>\n {
  /* line numbers are counted elsewhere already */
  yycolumn = 0;
  BEGIN(INITIAL);
}

<COMMENT_SINGLE>[^\n]+ {
  /* everything else */
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

<COMMENT_MULTI><<EOF>> {
  auto parser = yyextra;
  parser->registerParseError(TRI_ERROR_QUERY_PARSE, "unexpected unterminated multi-line comment", yylloc->first_line, yylloc->first_column);
}

<COMMENT_MULTI>\n {
  /* line numbers are counted elsewhere already */
  yycolumn = 0;
}

. {
  /* anything else is returned as it is */
  return (int) yytext[0];
}


%%
