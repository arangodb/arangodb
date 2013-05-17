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
#include "BasicsC/tri-strings.h"
#include "BasicsC/logging.h"

#ifdef _WIN32
#define YY_NO_UNISTD_H 1
#else
int fileno(FILE *stream);
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

struct jsonData {
  TRI_memory_zone_t* _memoryZone;
  char const* _message;
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

static bool ParseArray (yyscan_t, TRI_json_t*);
static bool ParseObject (yyscan_t, TRI_json_t*, int);

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

static bool ParseList (yyscan_t scanner, TRI_json_t* result) {
  struct yyguts_t * yyg = (struct yyguts_t*) scanner;

  bool comma;
  int c;

  TRI_InitListJson(yyextra._memoryZone, result);

  c = yylex(scanner);
  comma = false;

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
      TRI_json_t sub;

      if (! ParseObject(scanner, &sub, c)) {
        return false;
      }

      TRI_PushBack2ListJson(result, &sub);
    }

    c = yylex(scanner);
  }

  yyextra._message = "expecting a list element, got end-of-file";

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parse an array
////////////////////////////////////////////////////////////////////////////////

static bool ParseArray (yyscan_t scanner, TRI_json_t* result) {
  struct yyguts_t * yyg = (struct yyguts_t*) scanner;

  bool comma;
  char* name;
  size_t nameLen;
  int c;

  comma = false;
  TRI_InitArrayJson(yyextra._memoryZone, result);

  c = yylex(scanner);

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
      
    if (name == NULL) {
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
      TRI_json_t sub;

      if (! ParseObject(scanner, &sub, c)) {
        TRI_FreeString(yyextra._memoryZone, name);
        return false;
      }

      TRI_Insert4ArrayJson(yyextra._memoryZone, result, name, nameLen, &sub);
    }

    c = yylex(scanner);
  }

  yyextra._message = "expecting a object attribute name or element, got end-of-file";

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parse an object
////////////////////////////////////////////////////////////////////////////////

static bool ParseObject (yyscan_t scanner, TRI_json_t* result, int c) {
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
      char* ptr;
      size_t outLength;

      if (yyleng <= 2) {
        // string is empty
        ptr = TRI_DuplicateString2Z(yyextra._memoryZone, yytext + 1, 0);
        outLength = 0;
      }
      else {
        // string is not empty, process it
        ptr = TRI_UnescapeUtf8StringZ(yyextra._memoryZone, yytext + 1, yyleng - 2, &outLength);
      }

      if (ptr == NULL) {
        yyextra._message = "out-of-memory";
        return false;
      }

      TRI_InitString2Json(result, ptr, outLength);

      return true;
    }
    
    case STRING_CONSTANT_ASCII: {
      char* ptr;

      ptr = TRI_DuplicateString2Z(yyextra._memoryZone, yytext + 1, yyleng - 2);

      if (ptr == NULL) {
        yyextra._message = "out-of-memory";
        return false;
      }

      TRI_InitString2Json(result, ptr, yyleng - 2);

      return true;
    }

    case OPEN_BRACE:
      return ParseArray(scanner, result);

    case OPEN_BRACKET:
      return ParseList(scanner, result);
    
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

TRI_json_t* TRI_Json2String (TRI_memory_zone_t* zone, char const* text, char** error) {
  TRI_json_t* object;
  YY_BUFFER_STATE buf;
  int c;
  struct yyguts_t * yyg;
  yyscan_t scanner;

  object = TRI_Allocate(zone, sizeof(TRI_json_t), false);

  if (object == NULL) {
    // out of memory
    return NULL;
  }
  
  // init as a JSON null object so the memory in object is initialised
  TRI_InitNullJson(object);

  yylex_init(&scanner);
  yyg = (struct yyguts_t*) scanner;

  yyextra._memoryZone = zone;

  buf = yy_scan_string((char yyconst*) text, scanner);

  c = yylex(scanner);
  if (! ParseObject(scanner, object, c)) {
    TRI_FreeJson(zone, object);
    object = NULL;
    LOG_DEBUG("failed to parse json object: '%s'", yyextra._message);
  }
  else {
    c = yylex(scanner);

    if (c != END_OF_FILE) {
      TRI_FreeJson(zone, object);
      object = NULL;
      yyextra._message = "failed to parse json object: expecting EOF";

      LOG_DEBUG("failed to parse json object: expecting EOF");
    }
  }

  if (error != NULL) {
    if (yyextra._message != NULL) {
      *error = TRI_DuplicateString(yyextra._message);
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

TRI_json_t* TRI_JsonString (TRI_memory_zone_t* zone, char const* text) {
  return TRI_Json2String(zone, text, 0);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parses a json file
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* TRI_JsonFile (TRI_memory_zone_t* zone, char const* path, char** error) {
  FILE* in;
  TRI_json_t* object;
  int c;
  struct yyguts_t * yyg;
  yyscan_t scanner;

  object = TRI_Allocate(zone, sizeof(TRI_json_t), false);
  
  if (object == NULL) {
    // out of memory
    return NULL;
  }

  in = fopen(path, "rb");

  if (in == 0) {
    LOG_ERROR("cannot open file '%s': '%s'", path, TRI_LAST_ERROR_STR);
    TRI_Free(zone, object);

    return NULL;
  }

  // init as a JSON null object so the memory in object is initialised
  TRI_InitNullJson(object);

  yylex_init(&scanner);
  yyg = (struct yyguts_t*) scanner;

  yyextra._memoryZone = zone;
  yyin = in;

  c = yylex(scanner);
  if (! ParseObject(scanner, object, c)) {
    TRI_FreeJson(zone, object);
    object = NULL;
    LOG_DEBUG("failed to parse json object: '%s'", yyextra._message);
  }
  else {
    c = yylex(scanner);

    if (c != END_OF_FILE) {
      TRI_FreeJson(zone, object);
      object = NULL;
      LOG_DEBUG("failed to parse json object: expecting EOF");
    }
  }

  if (error != NULL) {
    if (yyextra._message != NULL) {
      *error = TRI_DuplicateString(yyextra._message);
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
