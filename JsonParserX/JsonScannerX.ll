%{ /* -*- C++ -*- */
////////////////////////////////////////////////////////////////////////////////
/// @brief flex scanner
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

#include "Basics/StringUtils.h"

#include <cstdlib>
#include <errno.h>
#include <limits.h>

#include <string>

#include "JsonParserX/JsonParserXDriver.h"
#include "JsonParserX/JsonParserX.h"

using namespace triagens::basics;
using namespace triagens::rest;

#if defined(_MSC_VER)
#pragma warning( disable : 4018 )
#endif

/* Work around an incompatibility in flex (at least versions
   2.5.31 through 2.5.33): it generates code that does
   not conform to C89.  See Debian bug 333231
   <http://bugs.debian.org/cgi-bin/bugreport.cgi?bug=333231>.  */

/* By default yylex returns int, we use token_type.
   Unfortunately yyterminate by default returns 0, which is
   not of token_type.  */
   
#define yyterminate() return token::END
#define YYSTYPE NAME_SPACE::JsonParserX::semantic_type
#define YYLTYPE NAME_SPACE::location

/* NOT USABLE HERE too MANY THREADS std::string buffer; */
%}

%option reentrant
%option bison-bridge
%option bison-locations

%option noyywrap nounput batch debug
%x STRINGS 
%x UNQUOTED_STRINGS

blank         [ \t]
ZERO          [0]
DIGIT         [0-9]
DIGIT1        [1-9]
HEXDIGIT      [0123456789abcdefABCDEF]{1}
MINUS         [-]
PLUS          [+]

%{
#define YY_EXTRA_TYPE triagens::rest::JsonParserXDriver*
#define YY_USER_ACTION  yylloc->columns(yyleng);

%}

%%

%{
%}

{blank}+ {
   yylloc->step();
}
   
[\n]+ {
  yylloc->lines(yyleng); 
  yylloc->step();
}  

%{
  typedef NAME_SPACE::JsonParserX::token token;
%}



 /****************************** normal strings *******************************/

 
\" {
  BEGIN STRINGS; 
  yyextra->buffer = "";
}

<STRINGS>\\\" { yyextra->buffer.push_back('\"'); }
<STRINGS>\\\\ { yyextra->buffer.push_back('\\'); }
<STRINGS>\\\/ { yyextra->buffer.push_back('/');  }
<STRINGS>\\b  { yyextra->buffer.push_back('\b'); }
<STRINGS>\\f  { yyextra->buffer.push_back('\f'); }
<STRINGS>\\n  { yyextra->buffer.push_back('\n'); }
<STRINGS>\\r  { yyextra->buffer.push_back('\r'); }
<STRINGS>\\t  { yyextra->buffer.push_back('\t'); }

<STRINGS>\\(?i:u){HEXDIGIT}{HEXDIGIT}{HEXDIGIT}{HEXDIGIT}  {
  StringUtils::unicodeToUTF8(&yytext[2],4, yyextra->buffer);
  //should be terminate if incorrect unicode?
  //yyterminate(); -- call this to prematurely terminate the scanning
}

<STRINGS>\\(?i:u)([Dd][89ABab]{HEXDIGIT}{HEXDIGIT})\\(?i:u)([Dd][CDEFcdef]{HEXDIGIT}{HEXDIGIT})  {
  StringUtils::convertUTF16ToUTF8(&yytext[2], &yytext[8], yyextra->buffer);
}


<STRINGS>\" {
  BEGIN 0;
  yylval->str = new std::string(yyextra->buffer);
  return token::STRING_CONSTANT;
}

<STRINGS>[\x20\x21\x23-\x2e\x3a-\x40\x5b\x5d`{}|~A-Za-z0-9_]* {
  yyextra->buffer.append(yytext,yyleng); 
}
  
<STRINGS>.       { 
  yyextra->buffer.push_back(yytext[0]);
}
  
  
 /****************************** identifiers and Keywords *********************/

(?i:false)      { return token::FALSE_CONSTANT; }
(?i:null)       { return token::NULL_CONSTANT; }
(?i:true)       { return token::TRUE_CONSTANT; }




 /*****************************************************************************/
 /****************************** INTEGERS *************************************/
 /*****************************************************************************/

 /*****************************************************************************/
 /* Integer with 'L' or 'l' are 64 bit                                        */
 /*****************************************************************************/
 
 /* -- long zero -- */
({MINUS}|{PLUS})?{ZERO}[lL] {
  yylval->int64_type = 0;
  return token::SIGNED_LONG_INTEGER_CONSTANT;
}

 /* -- unsigned long zero -- */
[uU]{ZERO}[lL] {
  yylval->uint64_type = 0;
  return token::UNSIGNED_LONG_INTEGER_CONSTANT;
}

 /* -- long integer -- */
({MINUS}|{PLUS})?{DIGIT1}{DIGIT}*[lL] {
  yylval->int64_type = StringUtils::int64(yytext, yyleng - 1);
  return token::SIGNED_LONG_INTEGER_CONSTANT;
}
 
 /* -- unsigned long integer -- */
[uU]{DIGIT1}{DIGIT}*[lL] {
  yylval->uint64_type = StringUtils::uint64(yytext + 1, yyleng - 2);
  return token::UNSIGNED_LONG_INTEGER_CONSTANT;
}

 /*****************************************************************************/
 /* Integer without 'L' or 'l' are also 64 bit                                */
 /*****************************************************************************/
 
 /* -- zero -- */
({MINUS}|{PLUS})?{ZERO} {
  yylval->int64_type = 0;
  return token::SIGNED_LONG_INTEGER_CONSTANT;
}

 /* -- unsigned zero -- */
[uU]{ZERO} {
  yylval->uint64_type = 0;
  return token::UNSIGNED_LONG_INTEGER_CONSTANT;
}

 /* -- integer -- */
({MINUS}|{PLUS})?{DIGIT1}{DIGIT}* {
  yylval->int64_type = StringUtils::int64(yytext, yyleng);
  return token::SIGNED_LONG_INTEGER_CONSTANT;
}
 
 /* -- unsigned integer -- */
[uU]{DIGIT1}{DIGIT}* {
  yylval->uint64_type = StringUtils::uint64(yytext + 1, yyleng - 1);
  return token::UNSIGNED_LONG_INTEGER_CONSTANT;
}

 /*******************************************************************************/
 /* floats with exponents                                                       */
 /*******************************************************************************/

 /*  -- decimal with exponent beginning with zero -- */
({MINUS}|{PLUS})?{ZERO}\.{DIGIT}+[eE]({MINUS}|{PLUS})?{DIGIT}+ {
  yylval->double_type = StringUtils::doubleDecimal(yytext, yyleng);
  return token::DECIMAL_CONSTANT;
}

 /* -- decimal with exponent not starting with ZERO -- */
({MINUS}|{PLUS})?{DIGIT1}{DIGIT}*\.{DIGIT}+[eE]({MINUS}|{PLUS})?{DIGIT}+ {
  yylval->double_type = StringUtils::doubleDecimal(yytext, yyleng);
  return token::DECIMAL_CONSTANT;
}

 /*******************************************************************************/
 /* floats without exponents                                                    */
 /*******************************************************************************/

 /*  -- decimal without exponent beginning with zero -- */
({MINUS}|{PLUS})?{ZERO}\.{DIGIT}+ {
  yylval->double_type = StringUtils::doubleDecimal(yytext, yyleng);
  return token::DECIMAL_CONSTANT;
}

 /* -- decimal without exponent not starting with ZERO -- */
({MINUS}|{PLUS})?{DIGIT1}{DIGIT}*\.{DIGIT}+ {
  yylval->double_type = StringUtils::doubleDecimal(yytext, yyleng);
  return token::DECIMAL_CONSTANT;
}

 /*******************************************************************************/
 /* special characters                                                          */
 /*******************************************************************************/

"}"             { return token::CLOSE_BRACE;   }
"]"             { return token::CLOSE_BRACKET; }
"{"             { return token::OPEN_BRACE;    }
"["             { return token::OPEN_BRACKET;  }
","             { return token::COMMA;         }
":"             { return token::COLON;         }

  /* these special characters are here only to report an error which can easily be detected */
"&&"            { return token::AND;         }
":="            { return token::ASSIGN;      }
")"             { return token::CLOSE_PAREN; }
"."             { return token::DOT;         }
"=="            { return token::EQ;          }
">="            { return token::GE;          }
">"             { return token::GT;          }
"<="            { return token::LE;          }
"<"             { return token::LT;          }
"-"             { return token::MINUS;       }
"<>"            { return token::NE;          }
"!="            { return token::NE;          }
"||"            { return token::OR;          }
"("             { return token::OPEN_PAREN;  }
"+"             { return token::PLUS;        }
";"             { return token::SEMICOLON;   }
"*"             { return token::TIMES;       }
"/"             { return token::QUOTIENT;    }

 /*******************************************************************************/
 /* unquoted                                                                    */
 /*******************************************************************************/

 /* whatever is left, should be an unquoted string appearing somewhere this */
 /* will be reported as an error                                            */  
  
. {
  return token::UNQUOTED_STRING;
} 

%%

void JsonParserXDriver::doParse () {
  NAME_SPACE::JsonParserX parser(*this);
  parser.set_debug_level(traceParsing);
  parser.parse();  
}

void JsonParserXDriver::scan_begin () {
  yylex_init_extra(this,&scanner);
  yyset_debug(traceScanning,scanner);
  yy_scan_string(scanString,scanner);
}

void JsonParserXDriver::scan_end () {
  yylex_destroy(scanner);
}
