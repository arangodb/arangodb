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
/// @brief parses a list
////////////////////////////////////////////////////////////////////////////////

static TRI_json_t* ParseList (yyscan_t scanner) {
  struct yyguts_t * yyg = (struct yyguts_t*) scanner;

  TRI_json_t* list;
  bool comma;
  int c;

  list = TRI_CreateListJson(yyextra._memoryZone);

  if (list == NULL) {
    yyextra._message = "out-of-memory";
    return NULL;
  }

  c = yylex(scanner);
  comma = false;

  while (c != END_OF_FILE) {
    TRI_json_t* sub;

    if (c == CLOSE_BRACKET) {
      return list;
    }

    if (comma) {
      if (c != COMMA) {
        TRI_FreeJson(yyextra._memoryZone, list);
        yyextra._message = "expecting comma";
        return NULL;
      }

      c = yylex(scanner);
    }
    else {
      comma = true;
    }

    sub = ParseObject(scanner, c);

    if (sub == NULL) {
      TRI_FreeJson(yyextra._memoryZone, list);
      return NULL;
    }

    TRI_PushBack3ListJson(yyextra._memoryZone, list, sub);

    c = yylex(scanner);
  }

  TRI_FreeJson(yyextra._memoryZone, list);
  yyextra._message = "expecting a list element, got end-of-file";

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
  size_t nameLen;
  int c;

  comma = false;
  array = TRI_CreateArrayJson(yyextra._memoryZone);

  if (array == NULL) {
    yyextra._message = "out-of-memory";
  }

  c = yylex(scanner);

  while (c != END_OF_FILE) {
    if (c == CLOSE_BRACE) {
      return array;
    }

    if (comma) {
      if (c != COMMA) {
        TRI_FreeJson(yyextra._memoryZone, array);
        yyextra._message = "expecting comma";
        return NULL;
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
      TRI_FreeJson(yyextra._memoryZone, array);
      yyextra._message = "expecting attribute name";
      return NULL;
    }
      
    if (name == NULL) {
      TRI_FreeJson(yyextra._memoryZone, array);
      yyextra._message = "out-of-memory";
      return NULL;
    }

    // followed by a colon
    c = yylex(scanner);

    if (c != COLON) {
      TRI_FreeString(yyextra._memoryZone, name);
      TRI_FreeJson(yyextra._memoryZone, array);
      yyextra._message = "expecting colon";
      return NULL;
    }

    // followed by an object
    c = yylex(scanner);
    sub = ParseObject(scanner, c);

    if (sub == NULL) {
      TRI_FreeString(yyextra._memoryZone, name);
      TRI_FreeJson(yyextra._memoryZone, array);
      return NULL;
    }

    TRI_Insert4ArrayJson(yyextra._memoryZone, array, name, nameLen, sub);

    c = yylex(scanner);
  }

  TRI_FreeJson(yyextra._memoryZone, array);
  yyextra._message = "expecting a object attribute name or element, got end-of-file";

  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parse an object
////////////////////////////////////////////////////////////////////////////////

static TRI_json_t* ParseObject (yyscan_t scanner, int c) {
  struct yyguts_t * yyg = (struct yyguts_t*) scanner;
  TRI_json_t* result;

  switch (c) {
    case FALSE_CONSTANT:
      result = TRI_CreateBooleanJson(yyextra._memoryZone, false);

      if (result == NULL) {
        yyextra._message = "out-of-memory";
      }

      return result;

    case TRUE_CONSTANT:
      result = TRI_CreateBooleanJson(yyextra._memoryZone, true);

      if (result == NULL) {
        yyextra._message = "out-of-memory";
      }

      return result;

    case NULL_CONSTANT:
      result = TRI_CreateNullJson(yyextra._memoryZone);

      if (result == NULL) {
        yyextra._message = "out-of-memory";
      }

      return result;

    case NUMBER_CONSTANT: {
      char* ep;
      double d;

      if ((size_t) yyleng >= 512) {
        yyextra._message = "number too big";
        return NULL;
      }

      // yytext is null-terminated. can use it directly without copying it into a temporary buffer
      d = strtod(yytext, &ep);

      if (d == HUGE_VAL && errno == ERANGE) {
        yyextra._message = "number too big";
        return NULL;
      }

      if (d == 0 && errno == ERANGE) {
        yyextra._message = "number too small";
        return NULL;
      }

      if (ep != yytext + yyleng) {
        yyextra._message = "cannot parse number";
        return NULL;
      }

      result = TRI_CreateNumberJson(yyextra._memoryZone, d);

      if (result == NULL) {
        yyextra._message = "out-of-memory";
      }

      return result;
    }

    case STRING_CONSTANT: {
      char* ptr;
      size_t outLength;
        
      if (yyleng <= 2) {
        // string is empty, use original pointer
        ptr = yytext + 1;
        outLength = 0;
      }
      else {
        // string is not empty, process it
        ptr = TRI_UnescapeUtf8StringZ(yyextra._memoryZone, yytext + 1, yyleng - 2, &outLength);

        if (ptr == NULL) {
          yyextra._message = "out-of-memory";
          return NULL;
        }
      }

      result = TRI_CreateString2Json(yyextra._memoryZone, ptr, outLength);

      if (result == NULL) {
        yyextra._message = "out-of-memory";
      }

      return result;
    }
    
    case STRING_CONSTANT_ASCII: {
      result = TRI_CreateString2CopyJson(yyextra._memoryZone, yytext + 1, yyleng - 2);

      if (result == NULL) {
        yyextra._message = "out-of-memory";
      }

      return result;
    }

    case OPEN_BRACE:
      return ParseArray(scanner);

    case OPEN_BRACKET:
      return ParseList(scanner);
    
    case CLOSE_BRACE:
      yyextra._message = "expected object, got '}'";
      return NULL;

    case CLOSE_BRACKET:
      yyextra._message = "expected object, got ']'";
      return NULL;

    case COMMA:
      yyextra._message = "expected object, got ','";
      return NULL;

    case COLON:
      yyextra._message = "expected object, got ':'";
      return NULL;

    case UNQUOTED_STRING:
      yyextra._message = "expected object, got unquoted string";
      return NULL;
    
    case END_OF_FILE:
      yyextra._message = "expecting atom, got end-of-file";
      return NULL;
  }

  yyextra._message = "unknown atom";
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

TRI_json_t* TRI_Json2String (TRI_memory_zone_t* zone, char const* text, char** error) {
  TRI_json_t* object;
  YY_BUFFER_STATE buf;
  int c;
  struct yyguts_t * yyg;
  yyscan_t scanner;

  object = 0;

  yylex_init(&scanner);
  yyg = (struct yyguts_t*) scanner;

  yyextra._memoryZone = zone;

  buf = yy_scan_string((char yyconst*) text, scanner);

  c = yylex(scanner);
  object = ParseObject(scanner, c);

  if (object == NULL) {
    LOG_DEBUG("failed to parse json object: '%s'", yyextra._message);
  }
  else {
    c = yylex(scanner);

    if (c != END_OF_FILE) {
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

  object = 0;
  in = fopen(path, "rb");

  if (in == 0) {
    LOG_ERROR("cannot open file '%s': '%s'", path, TRI_LAST_ERROR_STR);
    return 0;
  }

  yylex_init(&scanner);
  yyg = (struct yyguts_t*) scanner;

  yyextra._memoryZone = zone;
  yyin = in;

  c = yylex(scanner);
  object = ParseObject(scanner, c);

  if (object == NULL) {
    LOG_DEBUG("failed to parse json object: '%s'", yyextra._message);
  }
  else {
    c = yylex(scanner);

    if (c != END_OF_FILE) {
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
