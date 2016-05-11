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
#include "Basics/tri-strings.h"
#include "V8/v8-json.h"
#include "V8/v8-globals.h"

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
#define STRING_CONSTANT_ASCII 13

struct jsonData {
  char const* _message;
  TRI_memory_zone_t* _memoryZone;
};

#define YY_FATAL_ERROR(a)          \
  do {                             \
    if (false) {                   \
      yy_fatal_error(a, nullptr);  \
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

\"[ !\x23-\x5b\x5d-\x7f]*\" {
  // performance optimisation for all-ASCII strings without escape characters
  // this matches the ASCII chars with ordinal numbers 35 (x23) to 127 (x7f), 
  // plus space (32) and ! (33) but no quotation marks (34, x22) and backslashes (92, x5c)
  return STRING_CONSTANT_ASCII;
}

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

static v8::Handle<v8::Value> ParseObject (v8::Isolate* isolate, yyscan_t scanner);
static v8::Handle<v8::Value> ParseValue (v8::Isolate* isolate, yyscan_t scanner, int c);

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief parses an array
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> ParseArray (v8::Isolate* isolate,
                                         yyscan_t scanner) {
  struct yyguts_t* yyg = (struct yyguts_t*) scanner;

  v8::Handle<v8::Array> array = v8::Array::New(isolate);
  uint32_t pos = 0;

  int c = yylex(scanner);

  while (c != END_OF_FILE) {
    if (c == CLOSE_BRACKET) {
      return array;
    }

    if (pos > 0) {
      if (c != COMMA) {
        yyextra._message = "expecting comma";
        return v8::Undefined(isolate);
      }

      c = yylex(scanner);
    }

    v8::Handle<v8::Value> sub = ParseValue(isolate, scanner, c);

    if (sub->IsUndefined()) {
      yyextra._message = "cannot create value";
      return v8::Undefined(isolate);
    }

    array->Set(pos++, sub);

    c = yylex(scanner);
  }

  yyextra._message = "expecting an array element, got end-of-file";

  return v8::Undefined(isolate);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parses an object
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> ParseObject (v8::Isolate* isolate,
                                         yyscan_t scanner) {
  struct yyguts_t* yyg = (struct yyguts_t*) scanner;

  v8::Handle<v8::Object> object = v8::Object::New(isolate);
  bool comma = false;

  int c = yylex(scanner);

  while (c != END_OF_FILE) {
    if (c == CLOSE_BRACE) {
      return object;
    }

    if (comma) {
      if (c != COMMA) {
        yyextra._message = "expecting comma";
        return v8::Undefined(isolate);
      }

      c = yylex(scanner);
    }
    else {
      comma = true;
    }

    // attribute name
    v8::Handle<v8::String> attributeName;

    if (c == STRING_CONSTANT) {
      // utf-8 attribute name
      size_t outLength;
      char* name = TRI_UnescapeUtf8String(yyextra._memoryZone, yytext + 1, yyleng - 2, &outLength, true);
    
      if (name == nullptr) {
        yyextra._message = "out-of-memory";
        return v8::Undefined(isolate);
      }

      attributeName = TRI_V8_PAIR_STRING(name, outLength);
      TRI_FreeString(yyextra._memoryZone, name);
    }
    else if (c == STRING_CONSTANT_ASCII) {
      // ASCII-only attribute name
      attributeName = TRI_V8_ASCII_PAIR_STRING(yytext + 1, yyleng - 2);
    }
    else {
      yyextra._message = "expecting attribute name";
      return v8::Undefined(isolate);
    }

    // followed by a colon
    c = yylex(scanner);

    if (c != COLON) {
      yyextra._message = "expecting colon";
      return v8::Undefined(isolate);
    }

    // followed by an object
    c = yylex(scanner);
    v8::Handle<v8::Value> sub = ParseValue(isolate, scanner, c);

    if (sub->IsUndefined()) {
      yyextra._message = "cannot create value";
      return v8::Undefined(isolate);
    }

    object->ForceSet(attributeName, sub);

    c = yylex(scanner);
  }

  yyextra._message = "expecting an object attribute name or element, got end-of-file";
  return v8::Undefined(isolate);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parses an object
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> ParseValue (v8::Isolate* isolate,
                                         yyscan_t scanner, 
                                         int c) {
  struct yyguts_t* yyg = (struct yyguts_t*) scanner;

  switch (c) {
    case END_OF_FILE: {
      yyextra._message = "expecting atom, got end-of-file";
      return v8::Undefined(isolate);
    }

    case FALSE_CONSTANT: {
      return v8::False(isolate);
    }

    case TRUE_CONSTANT: {
      return v8::True(isolate);
    }

    case NULL_CONSTANT: {
      return v8::Null(isolate);
    }

    case NUMBER_CONSTANT: {
      char* ep;

      if ((size_t) yyleng >= 512) {
        yyextra._message = "number too big";
        return v8::Undefined(isolate);
      }

      // need to reset errno because return value of 0 is not distinguishable from an error on Linux
      errno = 0;

      // yytext is null-terminated. can use it directly without copying it into a temporary buffer
      double d = strtod(yytext, &ep);

      if (d == HUGE_VAL && errno == ERANGE) {
        yyextra._message = "number too big";
        return v8::Undefined(isolate);
      }

      if (d == 0.0 && errno == ERANGE) {
        yyextra._message = "number too small";
        return v8::Undefined(isolate);
      }

      if (ep != yytext + yyleng) {
        yyextra._message = "cannot parse number";
        return v8::Undefined(isolate);
      }

      return v8::Number::New(isolate, d);
    }

    case STRING_CONSTANT: {
      if (yyleng <= 2) {
        // string is empty
        return v8::String::Empty(isolate);
      }

      // string is not empty
      size_t outLength;
      char* ptr = TRI_UnescapeUtf8String(yyextra._memoryZone, yytext + 1, yyleng - 2, &outLength, true);

      if (ptr == nullptr || outLength == 0) {
        yyextra._message = "out-of-memory";
        return v8::Undefined(isolate);
      }

      v8::Handle<v8::String> str = TRI_V8_PAIR_STRING(ptr, outLength);
      TRI_FreeString(yyextra._memoryZone, ptr);

      return str;
    }
    
    case STRING_CONSTANT_ASCII: {
      if (yyleng <= 2) {
        // string is empty
        return v8::String::Empty(isolate);
      }

      // string is not empty
      return TRI_V8_ASCII_PAIR_STRING(yytext + 1, yyleng - 2);
    }

    case OPEN_BRACE: {
      return ParseObject(isolate, scanner);
    }

    case CLOSE_BRACE: {
      yyextra._message = "expected value, got '}'";
      return v8::Undefined(isolate);
    }

    case OPEN_BRACKET: {
      return ParseArray(isolate, scanner);
    }

    case CLOSE_BRACKET: {
      yyextra._message = "expected value, got ']'";
      return v8::Undefined(isolate);
    }

    case COMMA: {
      yyextra._message = "expected value, got ','";
      return v8::Undefined(isolate);
    }

    case COLON: {
      yyextra._message = "expected value, got ':'";
      return v8::Undefined(isolate);
    }

    case UNQUOTED_STRING: {
      yyextra._message = "expected value, got unquoted string";
      return v8::Undefined(isolate);
    }
  }

  yyextra._message = "unknown atom";
  return v8::Undefined(isolate);
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
  yyscan_t scanner;
  yylex_init(&scanner);
  struct yyguts_t* yyg = (struct yyguts_t*) scanner;

  yyextra._memoryZone = TRI_UNKNOWN_MEM_ZONE;
  YY_BUFFER_STATE buf = yy_scan_string(text, scanner);

  int c = yylex(scanner);
  v8::Handle<v8::Value> value = ParseValue(isolate, scanner, c);

  if (!value->IsUndefined()) {
    c = yylex(scanner);

    if (c != END_OF_FILE) {
      value = v8::Undefined(isolate);
    }
  }

  if (error != nullptr) {
    if (yyextra._message != nullptr) {
      *error = TRI_DuplicateString(TRI_UNKNOWN_MEM_ZONE, yyextra._message);
    }
    else {
      *error = nullptr;
    }
  }

  yy_delete_buffer(buf, scanner);
  yylex_destroy(scanner);

  return value;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: C
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\)"
// End:
