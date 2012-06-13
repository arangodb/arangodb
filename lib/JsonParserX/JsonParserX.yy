%skeleton "lalr1.cc"                          /*  -*- C++ -*- */
%defines
%define "parser_class_name" "JsonParserX"
%error-verbose
%pure-parser

////////////////////////////////////////////////////////////////////////////////
/// @brief bison parser
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2011 triagens GmbH, Cologne, Germany
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
/// @author Dr. Oreste Costa-Panaia
/// @author Copyright 2010-2011, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

// .............................................................................
// change the namespace here -- see define NAME_SPACE below
// .............................................................................

%define namespace "triagens::json_parser"

// .............................................................................
// preamble
// .............................................................................

%code requires {
#define NAME_SPACE triagens::json_parser
#define YY_DECL                                                     \
  NAME_SPACE::JsonParserX::token_type                               \
  yylex (NAME_SPACE::JsonParserX::semantic_type* yylval_param,      \
         NAME_SPACE::JsonParserX::location_type* yylloc_param,      \
         void* yyscanner)
}

%{

#include <Basics/Common.h>
#include <Basics/StringUtils.h>
#include <Variant/VariantArray.h>
#include <Variant/VariantBoolean.h>
#include <Variant/VariantDouble.h>
#include <Variant/VariantInt32.h>
#include <Variant/VariantInt64.h>
#include <Variant/VariantNull.h>
#include <Variant/VariantObject.h>
#include <Variant/VariantString.h>
#include <Variant/VariantUInt32.h>
#include <Variant/VariantUInt64.h>
#include <Variant/VariantVector.h>

#include "JsonParserX/JsonParserXDriver.h"

using namespace std;
using namespace triagens::basics;

#define YYSCANNER driver.scanner
#define YYENABLE_NLS 0 // warning should be suppressed in newer releases of bison
%}

// The parsing context.
%parse-param { triagens::rest::JsonParserXDriver& driver }
%lex-param   { void* YYSCANNER }

%locations

%initial-action
{
  // reset the location
  @$.step();
};

%debug
%error-verbose

%union
{
  std::string*                       str;
  int32_t                            int32_type;
  int64_t                            int64_type;
  uint32_t                           uint32_type;
  uint64_t                           uint64_type;
  double                             double_type;
  triagens::basics::VariantArray*    variantArray;
  triagens::basics::VariantArray*    keyValueList;
  triagens::basics::VariantVector*   variantVector;
  triagens::basics::VariantVector*   valueList;
  triagens::basics::VariantObject*   variantObject;
  
};

%token                   END 0            "end of file"

%token <double_type>     DECIMAL_CONSTANT                      "decimal constant"
%token <str>             DECIMAL_CONSTANT_STRING               "decimal constant string"
%token <str>             IDENTIFIER                            "identifier"
%token <int32_type>      SIGNED_INTEGER_CONSTANT               "signed integer constant"
%token <str>             SIGNED_INTEGER_CONSTANT_STRING        "signed integer constant string"
%token <int64_type>      SIGNED_LONG_INTEGER_CONSTANT          "signed long integer constant"
%token <str>             SIGNED_LONG_INTEGER_CONSTANT_STRING   "signed long integer constant string"
%token <str>             STRING_CONSTANT                       "string constant"
%token <uint32_type>     UNSIGNED_INTEGER_CONSTANT             "unsigned integer constant"
%token <str>             UNSIGNED_INTEGER_CONSTANT_STRING      "unsigned integer constant string"
%token <uint64_type>     UNSIGNED_LONG_INTEGER_CONSTANT        "unsigned long integer constant"
%token <str>             UNSIGNED_LONG_INTEGER_CONSTANT_STRING "unsigned long integer constant string"


%token                   AND              "&&"
%token                   ASSIGN           ":="
%token                   CLOSE_BRACE      "}"
%token                   CLOSE_BRACKET    "]"
%token                   CLOSE_PAREN      ")"
%token                   COLON            ":"
%token                   COMMA            ","
%token                   DOT              "."
%token                   EQ               "=="
%token                   FALSE_CONSTANT   "false"
%token                   GE               ">="
%token                   GT               ">"
%token                   LE               "<="
%token                   LT               "<"
%token                   MINUS            "-"
%token                   NE               "<>"
%token                   NULL_CONSTANT    "null"
%token                   OPEN_BRACE       "{"
%token                   OPEN_BRACKET     "["
%token                   OPEN_PAREN       "("
%token                   OR               "||"
%token                   PLUS             "+"
%token                   QUOTIENT         "/"
%token                   SEMICOLON        ";"
%token                   TIMES            "*"
%token                   TRUE_CONSTANT    "true"
%token                   STRING_CONSTANT_NULL "string_constant_null"
%token                   UNQUOTED_STRING  "unquoted string"

%type <keyValueList>     keyValueList
%type <valueList>        valueList

%type <variantArray>     variantArray
%type <variantObject>    variantObject
%type <variantVector>    variantVector

%printer { debug_stream() << *$$; } STRING_CONSTANT

%{
YY_DECL;
%}

// .............................................................................
// grammar
// .............................................................................

%%

%start jsonDefinition;

// .............................................................................
// precedence 
// .............................................................................

%left NEGATION;
%left GE LE EQ NE GT LT;
%left PLUS MINUS;
%left TIMES QUOTIENT;

// .............................................................................
// DEFINITION FILE
// .............................................................................

jsonDefinition:
    variantArray {
      driver.addVariantArray($1);
    }
  |
    DECIMAL_CONSTANT {
      driver.addVariantDouble($1);
    }
  |  
    FALSE_CONSTANT {
      driver.addVariantBoolean(false);
    }
  |  
    NULL_CONSTANT  {
      driver.addVariantNull();
    }
  |  
    SIGNED_INTEGER_CONSTANT  {
      driver.addVariantInt32($1);
    }
  |  
    SIGNED_LONG_INTEGER_CONSTANT {
      driver.addVariantInt64($1);
    }
  |  
    STRING_CONSTANT {
      driver.addVariantString(*$1);
      delete $1;
    }
  |  
    TRUE_CONSTANT {
      driver.addVariantBoolean(true);
    }
  |  
    UNSIGNED_INTEGER_CONSTANT  {
      driver.addVariantUInt32($1);
    }
  |  
    UNSIGNED_LONG_INTEGER_CONSTANT {
      driver.addVariantUInt64($1);
    }
  |  
    variantVector {
      driver.addVariantVector($1);
    }    
  ;


variantArray:
    OPEN_BRACE keyValueList CLOSE_BRACE {
      $$ = $2;
    }
  |
    OPEN_BRACE CLOSE_BRACE {
      $$ = new VariantArray();
    }
  ;


variantVector:
    OPEN_BRACKET valueList CLOSE_BRACKET {
      $$ = $2;
    }   
  |    
    OPEN_BRACKET  CLOSE_BRACKET {
      $$ = new VariantVector();
    }   
  ;

  
keyValueList:
    STRING_CONSTANT COLON variantObject {
      $$ = new VariantArray();
      $$->add(*$1,$3);    
      delete $1;      
    }    
  |
    keyValueList COMMA {
      // nothing to add 
    }    
  |
    keyValueList COMMA STRING_CONSTANT COLON variantObject {
      $$->add(*$3,$5);    
      delete $3;      
    }    
  ;
  

valueList:
    variantObject {
      $$ = new VariantVector();
      $$->add($1);
    }    
  |
    valueList COMMA {
      // nothing to add
    }    
  |
    valueList COMMA variantObject {
      $1->add($3);
    }    
  ;
  
    
variantObject:
    DECIMAL_CONSTANT {
      $$ = new VariantDouble($1);
    }
  |    
    DECIMAL_CONSTANT_STRING {
      $$ = new VariantDouble(StringUtils::doubleDecimal(*$1));
    }
  |    
    SIGNED_INTEGER_CONSTANT {
      $$ = new VariantInt32($1);
    }    
  |    
    SIGNED_INTEGER_CONSTANT_STRING {
      $$ = new VariantInt32(StringUtils::int32(*$1));
    }    
  |    
    SIGNED_LONG_INTEGER_CONSTANT {
      $$ = new VariantInt64($1);
    }    
  |    
    SIGNED_LONG_INTEGER_CONSTANT_STRING {
      $$ = new VariantInt64(StringUtils::int64(*$1));
    }    
  |    
    STRING_CONSTANT {
      $$ = new VariantString(*$1);
      delete $1;
    }    
  |
    FALSE_CONSTANT {
      $$ = new VariantBoolean(false);
    }
  |  
    TRUE_CONSTANT {
      $$ = new VariantBoolean(true);
    }
  |  
    NULL_CONSTANT {
      $$ = new VariantNull();
    }
  |    
    UNSIGNED_INTEGER_CONSTANT {
      $$ = new VariantUInt32($1);
    }    
  |    
    UNSIGNED_INTEGER_CONSTANT_STRING {
      $$ = new VariantUInt32(StringUtils::uint32(*$1));
    }    
  |    
    UNSIGNED_LONG_INTEGER_CONSTANT {
      $$ = new VariantUInt64($1);
    }    
  |    
    UNSIGNED_LONG_INTEGER_CONSTANT_STRING {
      $$ = new VariantUInt64(StringUtils::uint64(*$1));
    }    
  |  
    variantArray {
      $$ = $1;
    }    
  |
    variantVector {
      $$ = $1;
    }    
  ;  

%%

// .............................................................................
// postamble
// .............................................................................

void triagens::json_parser::JsonParserX::error (const triagens::json_parser::JsonParserX::location_type& l, const string& m) {
  triagens::json_parser::position last = l.end - 1;
  driver.setError(last.line, last.column, m);
}
