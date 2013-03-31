%top{
////////////////////////////////////////////////////////////////////////////////
/// @brief json parser
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2011 triagens GmbH, Cologne, Germany
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
/// @author Copyright 2011, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "BasicsC/common.h"

#include "V8/v8-globals.h"

#include "BasicsC/tri-strings.h"
#include "BasicsC/logging.h"

#define YY_NO_INPUT
}

%option noyywrap nounput batch
%option 8bit
%option reentrant
%option extra-type="struct jsonData"
%option prefix="tri_v8_"

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
  char const* _message;
  TRI_memory_zone_t* _memoryZone;
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

\"(\\.|[^\\\"])*\" {
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

static v8::Handle<v8::Value> ParseArray (yyscan_t scanner);
static v8::Handle<v8::Value> ParseObject (yyscan_t scanner, int c);

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Json
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief parses a list
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> ParseList (yyscan_t scanner) {
  v8::HandleScope scope;

  struct yyguts_t * yyg = (struct yyguts_t*) scanner;

  v8::Handle<v8::Array> list = v8::Array::New();
  bool comma = false;
  size_t pos = 0;

  int c = yylex(scanner);

  while (c != END_OF_FILE) {
    if (c == CLOSE_BRACKET) {
      return scope.Close(list);
    }

    if (comma) {
      if (c != COMMA) {
        yyextra._message = "expecting comma";
        return scope.Close(v8::Undefined());
      }

      c = yylex(scanner);
    }
    else {
      comma = true;
    }

    v8::Handle<v8::Value> sub = ParseObject(scanner, c);

    if (sub->IsUndefined()) {
      return scope.Close(v8::Undefined());
    }

    list->Set(pos++, sub);

    c = yylex(scanner);
  }

  yyextra._message = "expecting a list element, got end-of-file";

  return scope.Close(v8::Undefined());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parses an array
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> ParseArray (yyscan_t scanner) {
  v8::HandleScope scope;

  struct yyguts_t * yyg = (struct yyguts_t*) scanner;

  v8::Handle<v8::Object> array = v8::Object::New();
  bool comma = false;
  char* name;
  char const* ptr;
  size_t len;
  size_t outLength;

  int c = yylex(scanner);

  while (c != END_OF_FILE) {
    if (c == CLOSE_BRACE) {
      return scope.Close(array);
    }

    if (comma) {
      if (c != COMMA) {
        yyextra._message = "expecting comma";
        return scope.Close(v8::Undefined());
      }

      c = yylex(scanner);
    }
    else {
      comma = true;
    }

    // attribute name
    if (c != STRING_CONSTANT) {
      yyextra._message = "expecting attribute name";
      return scope.Close(v8::Undefined());
    }

    ptr = yytext;
    len = yyleng;
    name = TRI_UnescapeUtf8StringZ(yyextra._memoryZone, ptr + 1, len - 2, &outLength);

    if (name == NULL) {
      yyextra._message = "out-of-memory";
      return scope.Close(v8::Undefined());
    }

    // followed by a colon
    c = yylex(scanner);

    if (c != COLON) {
      TRI_FreeString(yyextra._memoryZone, name);
      yyextra._message = "expecting colon";
      return scope.Close(v8::Undefined());
    }

    // fallowed by an object
    c = yylex(scanner);
    v8::Handle<v8::Value> sub = ParseObject(scanner, c);

    if (sub->IsUndefined()) {
      TRI_FreeString(yyextra._memoryZone, name);
      return scope.Close(v8::Undefined());
    }

    array->Set(v8::String::New(name), sub);

    TRI_FreeString(yyextra._memoryZone, name);

    c = yylex(scanner);
  }

  yyextra._message = "expecting a object attribute name or element, got end-of-file";
  return scope.Close(v8::Undefined());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parses an object
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> ParseObject (yyscan_t scanner, int c) {
  v8::HandleScope scope;

  struct yyguts_t * yyg = (struct yyguts_t*) scanner;

  char buffer[1024];

  char* ep;
  char* ptr;
  double d;
  size_t outLength;

  v8::Handle<v8::String> str;

  switch (c) {
    case END_OF_FILE:
      yyextra._message = "expecting atom, got end-of-file";
      return scope.Close(v8::Undefined());

    case FALSE_CONSTANT:
      return scope.Close(v8::False());

    case TRUE_CONSTANT:
      return scope.Close(v8::True());

    case NULL_CONSTANT:
      return scope.Close(v8::Null());

    case NUMBER_CONSTANT:
      if ((size_t) yyleng >= sizeof(buffer)) {
        yyextra._message = "number too big";
        return scope.Close(v8::Undefined());
      }

      memcpy(buffer, yytext, yyleng);
      buffer[yyleng] = '\0';

      d = strtod(buffer, &ep);

      if (d == HUGE_VAL && errno == ERANGE) {
        yyextra._message = "number too big";
        return scope.Close(v8::Undefined());
      }

      if (d == 0 && errno == ERANGE) {
        yyextra._message = "number too small";
        return scope.Close(v8::Undefined());
      }

      if (ep != buffer + yyleng) {
        yyextra._message = "cannot parse number";
        return scope.Close(v8::Undefined());
      }

      return scope.Close(v8::Number::New(d));

    case STRING_CONSTANT:
      ptr = TRI_UnescapeUtf8StringZ(yyextra._memoryZone, yytext + 1, yyleng - 2, &outLength);

      if (ptr == NULL) {
        yyextra._message = "out-of-memory";
        return scope.Close(v8::Undefined());
      }

      str = v8::String::New(ptr, outLength);
      TRI_FreeString(yyextra._memoryZone, ptr);

      return scope.Close(str);

    case OPEN_BRACE:
      return scope.Close(ParseArray(scanner));

    case CLOSE_BRACE:
      yyextra._message = "expected object, got '}'";
      return scope.Close(v8::Undefined());

    case OPEN_BRACKET:
      return scope.Close(ParseList(scanner));

    case CLOSE_BRACKET:
      yyextra._message = "expected object, got ']'";
      return scope.Close(v8::Undefined());

    case COMMA:
      yyextra._message = "expected object, got ','";
      return scope.Close(v8::Undefined());

    case COLON:
      yyextra._message = "expected object, got ':'";
      return scope.Close(v8::Undefined());

    case UNQUOTED_STRING:
      yyextra._message = "expected object, got unquoted string";
      return scope.Close(v8::Undefined());
  }

  yyextra._message = "unknown atom";
  return scope.Close(v8::Undefined());
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

v8::Handle<v8::Value> TRI_FromJsonString (char const* text, char** error) {
  v8::HandleScope scope;

  v8::Handle<v8::Value> object = v8::Undefined();
  YY_BUFFER_STATE buf;
  int c;
  struct yyguts_t * yyg;
  yyscan_t scanner;

  yylex_init(&scanner);
  yyg = (struct yyguts_t*) scanner;

  buf = yy_scan_string(text, scanner);

  c = yylex(scanner);
  object = ParseObject(scanner, c);

  if (object->IsUndefined()) {
    LOG_DEBUG("failed to parse json object: '%s'", yyextra._message);
  }
  else {
    c = yylex(scanner);

    if (c != END_OF_FILE) {
      object = v8::Undefined();
      LOG_DEBUG("failed to parse json object: expecting EOF");
    }
  }

  if (error != NULL) {
    if (yyextra._message != 0) {
      *error = TRI_DuplicateString(yyextra._message);
    }
    else {
      *error = NULL;
    }
  }

  yy_delete_buffer(buf, scanner);
  yylex_destroy(scanner);

  return scope.Close(object);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: C
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\)"
// End:
