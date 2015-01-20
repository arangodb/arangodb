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

#include "Basics/Common.h"

#include "Basics/json.h"
#include "Basics/tri-strings.h"
#include "Basics/logging.h"

#ifdef _WIN32
#define YY_NO_UNISTD_H 1
#else
#ifndef __FreeBSD__
int fileno(FILE *stream);
#endif
#endif

#define YY_NO_INPUT
}

%option noyywrap nounput batch
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
#define STRING_CONSTANT_ASCII 13

static char const* EmptyString = "";

struct jsonData {
  TRI_memory_zone_t* _memoryZone;
  char const* _message;
};

#define YY_FATAL_ERROR(a)              \
  do {                                 \
    LOG_DEBUG("json-parser: %s", (a)); \
    if (false) {                       \
      yy_fatal_error(a, nullptr);         \
    }                                  \
  }                                    \
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

\"[ !\x23-\x5b\x5d-x7f]*\" {
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

static bool ParseObject (yyscan_t, TRI_json_t*);
static bool ParseValue (yyscan_t, TRI_json_t*, int);

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief do not use, only here to silence compiler
////////////////////////////////////////////////////////////////////////////////

void TRI_JsonError (const char* msg) {
  YY_FATAL_ERROR(msg);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parses an array
////////////////////////////////////////////////////////////////////////////////

static bool ParseArray (yyscan_t scanner, TRI_json_t* result) {
  struct yyguts_t * yyg = (struct yyguts_t*) scanner;

  TRI_InitArrayJson(yyextra._memoryZone, result);

  int c = yylex(scanner);
  bool comma = false;

  while (c != END_OF_FILE) {
    if (c == CLOSE_BRACKET) {
      return true;
    }

    if (comma) {
      if (c != COMMA) {
        yyextra._message = "expecting comma";
        return false;
      }

      c = yylex(scanner);
    }
    else {
      comma = true;
    }

    {
      // optimization: get the address of the next element in the array
      // so we can create the upcoming element in place
      TRI_json_t* next = static_cast<TRI_json_t*>(TRI_NextVector(&result->_value._objects));

      if (next == nullptr) {
        yyextra._message = "out-of-memory";
        return false;
      }

      if (! ParseValue(scanner, next, c)) {
        // be paranoid 
        TRI_InitNullJson(next);

        return false;
      }
    }

    c = yylex(scanner);
  }

  yyextra._message = "expecting a list element, got end-of-file";

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parse an object
////////////////////////////////////////////////////////////////////////////////

static bool ParseObject (yyscan_t scanner, TRI_json_t* result) {
  struct yyguts_t * yyg = (struct yyguts_t*) scanner;

  bool comma = false;
  TRI_InitObjectJson(yyextra._memoryZone, result);

  int c = yylex(scanner);

  while (c != END_OF_FILE) {
    if (c == CLOSE_BRACE) {
      return true;
    }

    if (comma) {
      if (c != COMMA) {
        yyextra._message = "expecting comma";
        return false;
      }

      c = yylex(scanner);
    }
    else {
      comma = true;
    }

    char* name;
    size_t nameLen;

    // attribute name
    if (c == STRING_CONSTANT) {
      // "complex" attribute name
      size_t outLength;
      nameLen = yyleng - 2;

      // do proper unescaping
      name = TRI_UnescapeUtf8StringZ(yyextra._memoryZone, yytext + 1, nameLen, &outLength);
      nameLen = outLength;
    }
    else if (c == STRING_CONSTANT_ASCII) {
      // ASCII-only attribute name
      nameLen = yyleng - 2;

      // no unescaping necessary. just copy it
      name = TRI_DuplicateString2Z(yyextra._memoryZone, yytext + 1, nameLen);
    }
    else {
      // some other token found => invalid
      yyextra._message = "expecting attribute name";
      return false;
    }
      
    if (name == nullptr) {
      yyextra._message = "out-of-memory";
      return false;
    }

    // followed by a colon
    c = yylex(scanner);

    if (c != COLON) {
      TRI_FreeString(yyextra._memoryZone, name);
      yyextra._message = "expecting colon";
      return false;
    }

    // followed by an object
    c = yylex(scanner);

    {
      // optimization: we allocate room for two elements at once
      int res = TRI_ReserveVector(&result->_value._objects, 2);

      if (res != TRI_ERROR_NO_ERROR) {
        yyextra._message = "out-of-memory";
        return false;
      } 
 
      // get the address of the next element so we can create the attribute name in place
      TRI_json_t* next = static_cast<TRI_json_t*>(TRI_NextVector(&result->_value._objects));
      // we made sure with the reserve call that we haven't run out of memory
      TRI_ASSERT_EXPENSIVE(next != nullptr);

      // store attribute name
      TRI_InitStringJson(next, name, nameLen);

      // now process the value
      next = static_cast<TRI_json_t*>(TRI_NextVector(&result->_value._objects));
      // we made sure with the reserve call that we haven't run out of memory
      TRI_ASSERT_EXPENSIVE(next != nullptr);

      if (! ParseValue(scanner, next, c)) {
        // be paranoid
        TRI_InitNullJson(next);
        return false;
      }
    }

    c = yylex(scanner);
  }

  yyextra._message = "expecting a object attribute name or element, got end-of-file";

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parse an object
////////////////////////////////////////////////////////////////////////////////

static bool ParseValue (yyscan_t scanner, TRI_json_t* result, int c) {
  struct yyguts_t * yyg = (struct yyguts_t*) scanner;

  switch (c) {
    case FALSE_CONSTANT:
      TRI_InitBooleanJson(result, false);

      return true;

    case TRUE_CONSTANT:
      TRI_InitBooleanJson(result, true);

      return true;

    case NULL_CONSTANT:
      TRI_InitNullJson(result);

      return true;

    case NUMBER_CONSTANT: {
      char* ep;
      double d;

      if ((size_t) yyleng >= 512) {
        yyextra._message = "number too big";
        return false;
      }

      // need to reset errno because return value of 0 is not distinguishable from an error on Linux
      errno = 0;

      // yytext is null-terminated. can use it directly without copying it into a temporary buffer
      d = strtod(yytext, &ep);

      if (d == HUGE_VAL && errno == ERANGE) {
        yyextra._message = "number too big";
        return false;
      }

      if (d == 0 && errno == ERANGE) {
        yyextra._message = "number too small";
        return false;
      }

      if (ep != yytext + yyleng) {
        yyextra._message = "cannot parse number";
        return false;
      }

      TRI_InitNumberJson(result, d);

      return true;
    }

    case STRING_CONSTANT: {
      if (yyleng <= 2) {
        // string is empty
        char const* ptr = EmptyString; // we'll create a reference to this compiled-in string
        TRI_InitStringReferenceJson(result, ptr, 0);
      }
      else {
        // string is not empty, process it
        size_t outLength;
        char* ptr = TRI_UnescapeUtf8StringZ(yyextra._memoryZone, yytext + 1, yyleng - 2, &outLength);
        if (ptr == nullptr) {
          yyextra._message = "out-of-memory";
          return false;
        }
      
       TRI_InitStringJson(result, ptr, outLength);
      }
      return true;
    }
    
    case STRING_CONSTANT_ASCII: {
      if (yyleng <= 2) {
        // string is empty
        char const* ptr = EmptyString; // we'll create a reference to this compiled-in string
        TRI_InitStringReferenceJson(result, ptr, 0);
       }
      else {
        char* ptr = TRI_DuplicateString2Z(yyextra._memoryZone, yytext + 1, yyleng - 2);
 
        if (ptr == nullptr) {
          yyextra._message = "out-of-memory";
          return false;
        }
 
        TRI_InitStringJson(result, ptr, yyleng - 2);
      }
      return true;
    }

    case OPEN_BRACE:
      return ParseObject(scanner, result);

    case OPEN_BRACKET:
      return ParseArray(scanner, result);
    
    case CLOSE_BRACE:
      yyextra._message = "expected object, got '}'";
      return false;

    case CLOSE_BRACKET:
      yyextra._message = "expected object, got ']'";
      return false;

    case COMMA:
      yyextra._message = "expected object, got ','";
      return false;

    case COLON:
      yyextra._message = "expected object, got ':'";
      return false;

    case UNQUOTED_STRING:
      yyextra._message = "expected object, got unquoted string";
      return false;
    
    case END_OF_FILE:
      yyextra._message = "expecting atom, got end-of-file";
      return false;
  }

  yyextra._message = "unknown atom";
  return false;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief parses a json string
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* TRI_Json2String (TRI_memory_zone_t* zone, char const* text, char** error) {
  TRI_json_t* object;
  YY_BUFFER_STATE buf;
  int c;
  struct yyguts_t * yyg;
  yyscan_t scanner;

  object = static_cast<TRI_json_t*>
                      (TRI_Allocate(zone, sizeof(TRI_json_t), false));

  if (object == nullptr) {
    // out of memory
    return nullptr;
  }
  
  // init as a JSON null object so the memory in object is initialised
  TRI_InitNullJson(object);

  yylex_init(&scanner);
  yyg = (struct yyguts_t*) scanner;

  yyextra._memoryZone = zone;

  buf = yy_scan_string((char yyconst*) text, scanner);

  c = yylex(scanner);
  if (! ParseValue(scanner, object, c)) {
    TRI_FreeJson(zone, object);
    object = nullptr;
    LOG_DEBUG("failed to parse json object: '%s'", yyextra._message);
  }
  else {
    c = yylex(scanner);

    if (c != END_OF_FILE) {
      TRI_FreeJson(zone, object);
      object = nullptr;
      yyextra._message = "failed to parse json object: expecting EOF";

      LOG_DEBUG("failed to parse json object: expecting EOF");
    }
  }

  if (error != nullptr) {
    if (yyextra._message != nullptr) {
      *error = TRI_DuplicateString(yyextra._message);
    }
    else {
      *error = nullptr;
    }
  }

  yy_delete_buffer(buf, scanner);
  yylex_destroy(scanner);

  return object;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parses a json string
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* TRI_JsonString (TRI_memory_zone_t* zone, char const* text) {
  return TRI_Json2String(zone, text, nullptr);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parses a json file
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* TRI_JsonFile (TRI_memory_zone_t* zone, char const* path, char** error) {
  FILE* in;
  TRI_json_t* value;
  int c;
  struct yyguts_t * yyg;
  yyscan_t scanner;

  value = static_cast<TRI_json_t*>(TRI_Allocate(zone, sizeof(TRI_json_t), false));
  
  if (value == nullptr) {
    // out of memory
    return nullptr;
  }

  in = fopen(path, "rb");

  if (in == nullptr) {
    LOG_ERROR("cannot open file '%s': '%s'", path, TRI_LAST_ERROR_STR);
    TRI_Free(zone, value);

    return nullptr;
  }

  // init as a JSON null object so the memory in value is initialised
  TRI_InitNullJson(value);

  yylex_init(&scanner);
  yyg = (struct yyguts_t*) scanner;

  yyextra._memoryZone = zone;
  yyin = in;

  c = yylex(scanner);
  if (! ParseValue(scanner, value, c)) {
    TRI_FreeJson(zone, value);
    value = nullptr;
    LOG_DEBUG("failed to parse json value: '%s'", yyextra._message);
  }
  else {
    c = yylex(scanner);

    if (c != END_OF_FILE) {
      TRI_FreeJson(zone, value);
      value = nullptr;
      LOG_DEBUG("failed to parse json value: expecting EOF");
    }
  }

  if (error != nullptr) {
    if (yyextra._message != nullptr) {
      *error = TRI_DuplicateString(yyextra._message);
    }
    else {
      *error = nullptr;
    }
  }

  yylex_destroy(scanner);

  fclose(in);

  return value;
}

// Local Variables:
// mode: C
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
