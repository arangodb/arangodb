%top{
////////////////////////////////////////////////////////////////////////////////
/// @brief json parser
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2012 triagens GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
/// @author Copyright 2011-2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "BasicsC/common.h"

#include "BasicsC/json.h"
#include "BasicsC/strings.h"
#include "BasicsC/logging.h"

#ifdef _WIN32
#define YY_NO_UNISTD_H 1
#else
int fileno(FILE *stream);
#endif

#define YY_NO_INPUT
}

%option noyywrap nounput batch debug
%option 8bit
%option reentrant
%option extra-type="struct jsonData"
%option prefix="tri_jsp_"

ZERO          [0]
DIGIT         [0-9]
DIGIT1        [1-9]
MINUS         [-]
PLUS          [+]

%{
#define END_OF_FILE 0
#define FALSE_CONSTANT 1
#define TRUE_CONSTANT 2
#define NULL_CONSTANT 3
#define NUMBER_CONSTANT 4
#define STRING_CONSTANT 5
#define OPEN_BRACE 6
#define CLOSE_BRACE 7
#define OPEN_BRACKET 8
#define CLOSE_BRACKET 9
#define COMMA 10
#define COLON 11
#define UNQUOTED_STRING 12

struct jsonData {
  char const* message;
};

#define YY_FATAL_ERROR(a) \
  LOG_DEBUG("json-paser: %s", (a))
%}

%%

 /* -----------------------------------------------------------------------------
  * keywords
  * ----------------------------------------------------------------------------- */

(?i:false) {
  return FALSE_CONSTANT;
}

(?i:null) {
  return NULL_CONSTANT;
}

(?i:true) {
  return TRUE_CONSTANT;
}

 /* -----------------------------------------------------------------------------
  * strings
  * ----------------------------------------------------------------------------- */

\"[^\"]*\" {
  return STRING_CONSTANT;
}

 /* -----------------------------------------------------------------------------
  * numbers
  * ----------------------------------------------------------------------------- */

({MINUS}|{PLUS})?({ZERO}|({DIGIT1}{DIGIT}*))(\.{DIGIT}+([eE]({MINUS}|{PLUS})?{DIGIT}+)?)? {
  return NUMBER_CONSTANT;
}

 /* -----------------------------------------------------------------------------
  * special characters
  * ----------------------------------------------------------------------------- */

"{" {
  return OPEN_BRACE;
}

"}" {
  return CLOSE_BRACE;
}

"[" {
  return OPEN_BRACKET;
}

"]" {
  return CLOSE_BRACKET;
}

"," {
 return COMMA;
}

":" {
  return COLON;
}

 /* -----------------------------------------------------------------------------
  * Skip whitespaces. Whatever is left, should be an unquoted string appearing
  * somewhere. This will be reported as an error.
  * ----------------------------------------------------------------------------- */

[ \t\r\n]* {
}

. {
  return UNQUOTED_STRING;
}

%%

// -----------------------------------------------------------------------------
// --SECTION--                                              forward declarations
// -----------------------------------------------------------------------------

static TRI_json_t* ParseArray (yyscan_t scanner);
static TRI_json_t* ParseObject (yyscan_t scanner, int c);

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Json
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief parse a list
////////////////////////////////////////////////////////////////////////////////

static TRI_json_t* ParseList (yyscan_t scanner) {
  struct yyguts_t * yyg = (struct yyguts_t*) scanner;

  TRI_json_t* list;
  bool comma;
  int c;

  list = TRI_CreateListJson();
  c = yylex(scanner);
  comma = false;

  while (c != END_OF_FILE) {
    TRI_json_t* sub;

    if (c == CLOSE_BRACKET) {
      return list;
    }

    if (comma) {
      if (c != COMMA) {
        TRI_FreeJson(list);
        yyextra.message = "expecting comma";
        return NULL;
      }

      c = yylex(scanner);
    }
    else {
      comma = true;
    }

    sub = ParseObject(scanner, c);

    if (sub == NULL) {
      TRI_FreeJson(list);
      return NULL;
    }

    TRI_PushBackListJson(list, sub);
    TRI_FreeJson(sub); // TODO remove unnecessary copying

    c = yylex(scanner);
  }

  TRI_FreeJson(list);
  yyextra.message = "expecting a list element, got end-of-file";

  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parse an array
////////////////////////////////////////////////////////////////////////////////

static TRI_json_t* ParseArray (yyscan_t scanner) {
  struct yyguts_t * yyg = (struct yyguts_t*) scanner;

  TRI_json_t* array;
  TRI_json_t* sub;
  bool comma;
  char* name;
  char const* ptr;
  int c;
  size_t len;
  size_t outLength;

  comma = false;
  array = TRI_CreateArrayJson();

  c = yylex(scanner);

  while (c != END_OF_FILE) {
    if (c == CLOSE_BRACE) {
      return array;
    }

    if (comma) {
      if (c != COMMA) {
        TRI_FreeJson(array);
        yyextra.message = "expecting comma";
        return NULL;
      }

      c = yylex(scanner);
    }
    else {
      comma = true;
    }

    // attribute name
    if (c != STRING_CONSTANT) {
      TRI_FreeJson(array);
      yyextra.message = "expecting attribute name";
      return NULL;
    }

    ptr = yytext;
    len = yyleng;
    name = TRI_UnescapeUtf8String(ptr + 1, len - 2, &outLength);

    // followed by a colon
    c = yylex(scanner);

    if (c != COLON) {
      TRI_FreeString(name);
      TRI_FreeJson(array);
      yyextra.message = "expecting colon";
      return NULL;
    }

    // fallowed by an object
    c = yylex(scanner);
    sub = ParseObject(scanner, c);

    if (sub == NULL) {
      TRI_FreeString(name);
      TRI_FreeJson(array);
      return NULL;
    }

    TRI_InsertArrayJson(array, name, sub);
    TRI_FreeString(name);
    TRI_FreeJson(sub); // TODO remove unnecessary copying

    c = yylex(scanner);
  }

  TRI_FreeJson(array);
  yyextra.message = "expecting a object attribute name or element, got end-of-file";

  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parse an object
////////////////////////////////////////////////////////////////////////////////

static TRI_json_t* ParseObject (yyscan_t scanner, int c) {
  struct yyguts_t * yyg = (struct yyguts_t*) scanner;

  char buffer[1024];

  char* ep;
  char* ptr;
  double d;
  size_t outLength;

  switch (c) {
    case END_OF_FILE:
      yyextra.message = "expecting atom, got end-of-file";
      return NULL;

    case FALSE_CONSTANT:
      return TRI_CreateBooleanJson(false);

    case TRUE_CONSTANT:
      return TRI_CreateBooleanJson(true);

    case NULL_CONSTANT:
      return TRI_CreateNullJson();

    case NUMBER_CONSTANT:
      if ((size_t) yyleng >= sizeof(buffer)) {
        yyextra.message = "number too big";
        return NULL;
      }

      memcpy(buffer, yytext, yyleng);
      buffer[yyleng] = '\0';

      d = strtod(buffer, &ep);

      if (d == HUGE_VAL && errno == ERANGE) {
        yyextra.message = "number too big";
        return NULL;
      }

      if (d == 0 && errno == ERANGE) {
        yyextra.message = "number too small";
        return NULL;
      }

      if (ep != buffer + yyleng) {
        yyextra.message = "cannot parse number";
        return NULL;
      }

      return TRI_CreateNumberJson(d);

    case STRING_CONSTANT:
      ptr = TRI_UnescapeUtf8String(yytext + 1, yyleng - 2, &outLength);
      return TRI_CreateString2Json(ptr, outLength);

    case OPEN_BRACE:
      return ParseArray(scanner);

    case CLOSE_BRACE:
      yyextra.message = "expected object, got '}'";
      return NULL;

    case OPEN_BRACKET:
      return ParseList(scanner);

    case CLOSE_BRACKET:
      yyextra.message = "expected object, got ']'";
      return NULL;

    case COMMA:
      yyextra.message = "expected object, got ','";
      return NULL;

    case COLON:
      yyextra.message = "expected object, got ':'";
      return NULL;

    case UNQUOTED_STRING:
      yyextra.message = "expected object, got unquoted string";
      return NULL;
  }

  yyextra.message = "unknown atom";
  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Json
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief parses a json string
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* TRI_Json2String (char const* text, char** error) {
  TRI_json_t* object;
  YY_BUFFER_STATE buf;
  int c;
  struct yyguts_t * yyg;
  yyscan_t scanner;

  object = 0;

  yylex_init(&scanner);
  yyg = (struct yyguts_t*) scanner;

  buf = yy_scan_string((char yyconst*) text, scanner);

  c = yylex(scanner);
  object = ParseObject(scanner, c);

  if (object == NULL) {
    LOG_DEBUG("failed to parse json object: '%s'", yyextra.message);
  }
  else {
    c = yylex(scanner);

    if (c != END_OF_FILE) {
      object = NULL;
      LOG_DEBUG("failed to parse json object: expecting EOF");
    }
  }

  if (error != NULL) {
    if (yyextra.message != 0) {
      *error = TRI_DuplicateString(yyextra.message);
    }
    else {
      *error = NULL;
    }
  }

  yy_delete_buffer(buf, scanner);
  yylex_destroy(scanner);

  return object;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parses a json string
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* TRI_JsonString (char const* text) {
  return TRI_Json2String(text, 0);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parses a json file
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* TRI_JsonFile (char const* path, char** error) {
  FILE* in;
  TRI_json_t* object;
  int c;
  struct yyguts_t * yyg;
  yyscan_t scanner;

  object = 0;
  in = fopen(path, "rb");

  if (in == 0) {
    LOG_ERROR("cannot open file '%s': '%s'", path, TRI_LAST_ERROR_STR);
    return 0;
  }

  yylex_init(&scanner);
  yyg = (struct yyguts_t*) scanner;

  yyin = in;

  c = yylex(scanner);
  object = ParseObject(scanner, c);

  if (object == NULL) {
    LOG_DEBUG("failed to parse json object: '%s'", yyextra.message);
  }
  else {
    c = yylex(scanner);

    if (c != END_OF_FILE) {
      object = NULL;
      LOG_DEBUG("failed to parse json object: expecting EOF");
    }
  }

  if (error != NULL) {
    if (yyextra.message != NULL) {
      *error = TRI_DuplicateString(yyextra.message);
    }
    else {
      *error = NULL;
    }
  }

  yylex_destroy(scanner);

  fclose(in);

  return object;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: C
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
