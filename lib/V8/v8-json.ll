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

#include "Basics/Common.h"

#include "V8/v8-globals.h"

#include "Basics/tri-strings.h"
#include "Basics/logging.h"

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

#define YY_FATAL_ERROR(a)          \
  do {                             \
    LOG_DEBUG("v8-json: %s", (a)); \
    if (false) {                   \
      yy_fatal_error(a, NULL);     \
    }                              \
  }                                \
  while (0)
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

({MINUS}|{PLUS})?({ZERO}|({DIGIT1}{DIGIT}*))((\.{DIGIT}+)?([eE]({MINUS}|{PLUS})?{DIGIT}+)?)? {
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

static v8::Handle<v8::Value> ParseArray (v8::Isolate* isolate, yyscan_t scanner);
static v8::Handle<v8::Value> ParseObject (v8::Isolate* isolate, yyscan_t scanner, int c);

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief parses a list
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> ParseList (v8::Isolate* isolate,
                                        yyscan_t scanner) {
  v8::EscapableHandleScope scope(isolate);

  struct yyguts_t * yyg = (struct yyguts_t*) scanner;

  v8::Handle<v8::Array> list = v8::Array::New(isolate);
  bool comma = false;
  uint32_t pos = 0;

  int c = yylex(scanner);

  while (c != END_OF_FILE) {
    if (c == CLOSE_BRACKET) {
      return scope.Escape<v8::Value>(list);
    }

    if (comma) {
      if (c != COMMA) {
        yyextra._message = "expecting comma";
        return scope.Escape<v8::Value>(v8::Undefined(isolate));
      }

      c = yylex(scanner);
    }
    else {
      comma = true;
    }

    v8::Handle<v8::Value> sub = ParseObject(isolate, scanner, c);

    if (sub->IsUndefined()) {
      return scope.Escape<v8::Value>(v8::Undefined(isolate));
    }

    list->Set(v8::Number::New(isolate, pos++), sub);

    c = yylex(scanner);
  }

  yyextra._message = "expecting a list element, got end-of-file";

  return scope.Escape<v8::Value>(v8::Undefined(isolate));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parses an array
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> ParseArray (v8::Isolate* isolate,
                                         yyscan_t scanner) {
  v8::EscapableHandleScope scope(isolate);

  struct yyguts_t * yyg = (struct yyguts_t*) scanner;

  v8::Handle<v8::Object> array = v8::Object::New(isolate);
  bool comma = false;
  char* name;
  char const* ptr;
  size_t len;
  size_t outLength;

  int c = yylex(scanner);

  while (c != END_OF_FILE) {
    if (c == CLOSE_BRACE) {
      return scope.Escape<v8::Value>(array);
    }

    if (comma) {
      if (c != COMMA) {
        yyextra._message = "expecting comma";
        return scope.Escape<v8::Value>(v8::Undefined(isolate));
      }

      c = yylex(scanner);
    }
    else {
      comma = true;
    }

    // attribute name
    if (c != STRING_CONSTANT) {
      yyextra._message = "expecting attribute name";
      return scope.Escape<v8::Value>(v8::Undefined(isolate));
    }

    ptr = yytext;
    len = yyleng;
    name = TRI_UnescapeUtf8StringZ(yyextra._memoryZone, ptr + 1, len - 2, &outLength);

    if (name == NULL) {
      yyextra._message = "out-of-memory";
      return scope.Escape<v8::Value>(v8::Undefined(isolate));
    }

    // followed by a colon
    c = yylex(scanner);

    if (c != COLON) {
      TRI_FreeString(yyextra._memoryZone, name);
      yyextra._message = "expecting colon";
      return scope.Escape<v8::Value>(v8::Undefined(isolate));
    }

    // fallowed by an object
    c = yylex(scanner);
    v8::Handle<v8::Value> sub = ParseObject(isolate, scanner, c);

    if (sub->IsUndefined()) {
      TRI_FreeString(yyextra._memoryZone, name);
      return scope.Escape<v8::Value>(v8::Undefined(isolate));
    }

    array->Set(v8::String::NewFromUtf8(isolate, name, v8::String::kNormalString, (int) outLength), sub);

    TRI_FreeString(yyextra._memoryZone, name);

    c = yylex(scanner);
  }

  yyextra._message = "expecting a object attribute name or element, got end-of-file";
      return scope.Escape<v8::Value>(v8::Undefined(isolate));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parses an object
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> ParseObject (v8::Isolate* isolate,
                                          yyscan_t scanner, int c) {
  v8::EscapableHandleScope scope(isolate);
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
      return scope.Escape<v8::Value>(v8::Undefined(isolate));
    case FALSE_CONSTANT:
      return scope.Escape<v8::Value>(v8::False(isolate));

    case TRUE_CONSTANT:
      return scope.Escape<v8::Value>(v8::True(isolate));

    case NULL_CONSTANT:
      return scope.Escape<v8::Value>(v8::Null(isolate));

    case NUMBER_CONSTANT:
      if ((size_t) yyleng >= sizeof(buffer)) {
        yyextra._message = "number too big";
        return scope.Escape<v8::Value>(v8::Undefined(isolate));
      }

      memcpy(buffer, yytext, yyleng);
      buffer[yyleng] = '\0';

      d = strtod(buffer, &ep);

      if (d == HUGE_VAL && errno == ERANGE) {
        yyextra._message = "number too big";
        return scope.Escape<v8::Value>(v8::Undefined(isolate));
      }

      if (d == 0 && errno == ERANGE) {
        yyextra._message = "number too small";
        return scope.Escape<v8::Value>(v8::Undefined(isolate));
      }

      if (ep != buffer + yyleng) {
        yyextra._message = "cannot parse number";
        return scope.Escape<v8::Value>(v8::Undefined(isolate));
      }

      return scope.Escape<v8::Value>(v8::Number::New(isolate, d));

    case STRING_CONSTANT:
      if (yyleng <= 2) {
        // string is empty
        ptr = yytext + 1;
        outLength = 0;
      }
      else {
        // string is not empty
        ptr = TRI_UnescapeUtf8StringZ(yyextra._memoryZone, yytext + 1, yyleng - 2, &outLength);

        if (ptr == NULL || outLength == 0) {
          yyextra._message = "out-of-memory";
          return scope.Escape<v8::Value>(v8::Undefined(isolate));
        }
      }

      str = v8::String::NewFromUtf8(isolate, ptr, v8::String::kNormalString, (int) outLength);

      if (0 < outLength) {
        TRI_FreeString(yyextra._memoryZone, ptr);
      }

      return scope.Escape<v8::Value>(str);

    case OPEN_BRACE:
      return scope.Escape<v8::Value>(ParseArray(isolate, scanner));

    case CLOSE_BRACE:
      yyextra._message = "expected object, got '}'";
      return scope.Escape<v8::Value>(v8::Undefined(isolate));

    case OPEN_BRACKET:
      return scope.Escape<v8::Value>(ParseList(isolate, scanner));

    case CLOSE_BRACKET:
      yyextra._message = "expected object, got ']'";
      return scope.Escape<v8::Value>(v8::Undefined(isolate));

    case COMMA:
      yyextra._message = "expected object, got ','";
      return scope.Escape<v8::Value>(v8::Undefined(isolate));

    case COLON:
      yyextra._message = "expected object, got ':'";
      return scope.Escape<v8::Value>(v8::Undefined(isolate));

    case UNQUOTED_STRING:
      yyextra._message = "expected object, got unquoted string";
      return scope.Escape<v8::Value>(v8::Undefined(isolate));
  }

  yyextra._message = "unknown atom";
  return scope.Escape<v8::Value>(v8::Undefined(isolate));
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief parses a json string
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Value> TRI_FromJsonString (v8::Isolate* isolate,
                                          char const* text,
                                          char** error) {
  v8::EscapableHandleScope scope(isolate);

  v8::Handle<v8::Value> object = v8::Undefined(isolate);
  YY_BUFFER_STATE buf;
  int c;
  struct yyguts_t * yyg;
  yyscan_t scanner;

  yylex_init(&scanner);
  yyg = (struct yyguts_t*) scanner;

  yyextra._memoryZone = TRI_CORE_MEM_ZONE;
  buf = yy_scan_string(text, scanner);

  c = yylex(scanner);
  object = ParseObject(isolate, scanner, c);

  if (object->IsUndefined()) {
    LOG_DEBUG("failed to parse json object: '%s'", yyextra._message);
  }
  else {
    c = yylex(scanner);

    if (c != END_OF_FILE) {
      object = v8::Undefined(isolate);
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

  return scope.Escape<v8::Value>(object);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: C
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\)"
// End:
