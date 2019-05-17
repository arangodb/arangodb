////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for csv.c
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2012 triagens GmbH, Cologne, Germany
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
/// @author Jan Steemann
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Basics/Common.h"

#include "catch.hpp"

#include <sstream>
#include <cstring>

#include "Basics/csv.h"
#include "Basics/tri-strings.h"

////////////////////////////////////////////////////////////////////////////////
/// @brief create a parser instance
////////////////////////////////////////////////////////////////////////////////

#define INIT_PARSER                                                   \
  TRI_csv_parser_t parser;                                            \
                                                                      \
  TRI_InitCsvParser(&parser,                                          \
                    &CCsvSetup::ProcessCsvBegin,                      \
                    &CCsvSetup::ProcessCsvAdd,                        \
                    &CCsvSetup::ProcessCsvEnd,                        \
                    nullptr);                                         \
  parser._dataAdd = &setup;                             

////////////////////////////////////////////////////////////////////////////////
/// @brief tab character
////////////////////////////////////////////////////////////////////////////////

#define TAB "\x9"

////////////////////////////////////////////////////////////////////////////////
/// @brief cr character
////////////////////////////////////////////////////////////////////////////////

#define CR "\xd"

////////////////////////////////////////////////////////////////////////////////
/// @brief lf character
////////////////////////////////////////////////////////////////////////////////

#define LF "\xa"

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

struct CCsvSetup {
  CCsvSetup () {
    column = 0;
  }

  ~CCsvSetup () {
  }

  static void ProcessCsvBegin (TRI_csv_parser_t* parser, size_t row) {
    CCsvSetup* me = reinterpret_cast<CCsvSetup*> (parser->_dataAdd);

    me->out << row << ":";
    me->column = 0;
  }
  
  static void ProcessCsvAdd (TRI_csv_parser_t* parser, char const* field, size_t, size_t row, size_t column, bool escaped) {
    CCsvSetup* me = reinterpret_cast<CCsvSetup*> (parser->_dataAdd);

    if (me->column++ > 0) {
      me->out << ",";
    }

    me->out << (escaped ? "ESC" : "") << field << (escaped ? "ESC" : "");
  }
  
  static void ProcessCsvEnd (TRI_csv_parser_t* parser, char const* field, size_t, size_t row, size_t column, bool escaped) {
    CCsvSetup* me = reinterpret_cast<CCsvSetup*> (parser->_dataAdd);
    
    if (me->column++ > 0) {
      me->out << ",";
    }
   
    me->out << (escaped ? "ESC" : "") << field << (escaped ? "ESC" : "") << "\n";
  }

  std::ostringstream out;
  size_t column;
};

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief setup
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("CCsvTest", "[CCsvTest]") {

  CCsvSetup setup;

////////////////////////////////////////////////////////////////////////////////
/// @brief test csv simple
////////////////////////////////////////////////////////////////////////////////

SECTION("tst_csv_simple") {
  INIT_PARSER
  TRI_SetSeparatorCsvParser(&parser, ',');
  TRI_SetQuoteCsvParser(&parser, '"', true);

  const char* csv = 
    "a,b,c,d,e," LF
    "f,g,h" LF
    ",,i,j,," LF;

  TRI_ParseCsvString(&parser, csv, strlen(csv));
  CHECK("0:a,b,c,d,e,\n1:f,g,h\n2:,,i,j,,\n" == setup.out.str());

  TRI_DestroyCsvParser(&parser);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test csv crlf
////////////////////////////////////////////////////////////////////////////////

SECTION("tst_csv_crlf") {
  INIT_PARSER
  TRI_SetSeparatorCsvParser(&parser, ',');
  TRI_SetQuoteCsvParser(&parser, '"', true);

  const char* csv = 
    "a,b,c,d,e" CR LF
    "f,g,h" CR LF
    "i,j" CR LF;

  TRI_ParseCsvString(&parser, csv, strlen(csv));
  CHECK("0:a,b,c,d,e\n1:f,g,h\n2:i,j\n" == setup.out.str());

  TRI_DestroyCsvParser(&parser);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test csv whitespace
////////////////////////////////////////////////////////////////////////////////

SECTION("tst_csv_whitespace") {
  INIT_PARSER
  TRI_SetSeparatorCsvParser(&parser, ',');
  TRI_SetQuoteCsvParser(&parser, '"', true);

  const char* csv = 
    " a , \"b \" , c , d , e " LF
    LF
    LF
    "   x   x  " LF;

  TRI_ParseCsvString(&parser, csv, strlen(csv));
  CHECK("0: a , \"b \" , c , d , e \n1:\n2:\n3:   x   x  \n" == setup.out.str());

  TRI_DestroyCsvParser(&parser);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test csv with quotes
////////////////////////////////////////////////////////////////////////////////

SECTION("tst_csv_quotes1") {
  INIT_PARSER
  TRI_SetSeparatorCsvParser(&parser, ',');
  TRI_SetQuoteCsvParser(&parser, '"', true);

  const char* csv = 
    "\"a\",\"b\"" LF
    "a,b" LF
    "\"a,b\",\"c,d\"" LF;

  TRI_ParseCsvString(&parser, csv, strlen(csv));
  CHECK("0:ESCaESC,ESCbESC\n1:a,b\n2:ESCa,bESC,ESCc,dESC\n" == setup.out.str());

  TRI_DestroyCsvParser(&parser);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test csv with quotes in quotes
////////////////////////////////////////////////////////////////////////////////

SECTION("tst_csv_quotes2") {
  INIT_PARSER
  TRI_SetSeparatorCsvParser(&parser, ',');
  TRI_SetQuoteCsvParser(&parser, '"', true);

  const char* csv = 
    "\"x\"\"y\",\"a\"\"\"" LF
    "\"\",\"\"\"ab\",\"\"\"\"\"ab\"" LF;

  TRI_ParseCsvString(&parser, csv, strlen(csv));
  CHECK("0:ESCx\"yESC,ESCa\"ESC\n1:ESCESC,ESC\"abESC,ESC\"\"abESC\n" == setup.out.str());

  TRI_DestroyCsvParser(&parser);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test csv quotes & whitespace
////////////////////////////////////////////////////////////////////////////////

SECTION("tst_csv_quotes_whitespace") {
  INIT_PARSER
  TRI_SetSeparatorCsvParser(&parser, ',');
  TRI_SetQuoteCsvParser(&parser, '"', true);

  const char* csv = 
    "\"a \" ,\" \"\" b \",\"\"\"\" ,\" \" " LF
    " \"\" ix" LF
    " \"\" " LF;

  TRI_ParseCsvString(&parser, csv, strlen(csv));
  CHECK("0:ESCa ESC,ESC \" b ESC,ESC\"ESC,ESC ESC\n1: \"\" ix\n2: \"\" \n" == setup.out.str());

  TRI_DestroyCsvParser(&parser);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test tsv
////////////////////////////////////////////////////////////////////////////////

SECTION("tst_tsv_simple") {
  INIT_PARSER
  TRI_SetSeparatorCsvParser(&parser, '\t');
  TRI_SetQuoteCsvParser(&parser, '\0', false);

  const char* tsv = 
    "a" TAB "b" TAB "c" LF 
    "the quick" TAB "brown fox jumped" TAB "over the" TAB "lazy" TAB "dog" LF; 

  TRI_ParseCsvString(&parser, tsv, strlen(tsv));
  CHECK("0:a,b,c\n1:the quick,brown fox jumped,over the,lazy,dog\n" == setup.out.str());

  TRI_DestroyCsvParser(&parser);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test tsv with whitespace
////////////////////////////////////////////////////////////////////////////////

SECTION("tst_tsv_whitespace") {
  INIT_PARSER
  TRI_SetSeparatorCsvParser(&parser, '\t');
  TRI_SetQuoteCsvParser(&parser, '\0', false);

  const char* tsv = 
    "a " TAB " b" TAB " c " LF 
    "  " LF
    "" LF
    "something else" LF;

  TRI_ParseCsvString(&parser, tsv, strlen(tsv));
  CHECK("0:a , b, c \n1:  \n2:\n3:something else\n" == setup.out.str());

  TRI_DestroyCsvParser(&parser);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test tsv with quotes (not supported)
////////////////////////////////////////////////////////////////////////////////

SECTION("tst_tsv_quotes") {
  INIT_PARSER
  TRI_SetSeparatorCsvParser(&parser, '\t');
  TRI_SetQuoteCsvParser(&parser, '\0', false);

  const char* tsv = 
    "\"a\"" TAB "\"b\"" TAB "\"c" LF 
    " \"" LF
    "\" fox " LF;

  TRI_ParseCsvString(&parser, tsv, strlen(tsv));
  CHECK("0:\"a\",\"b\",\"c\n1: \"\n2:\" fox \n" == setup.out.str());

  TRI_DestroyCsvParser(&parser);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test tsv with separator (not supported)
////////////////////////////////////////////////////////////////////////////////

SECTION("tst_tsv_separator") {
  INIT_PARSER
  TRI_SetSeparatorCsvParser(&parser, '\t');
  TRI_SetQuoteCsvParser(&parser, ',', false);

  const char* tsv = 
    "\"a,,\"" TAB "\",,b\"" TAB "\",c," LF 
    " , ,\", " LF
    ",\", fox,, " LF;

  TRI_ParseCsvString(&parser, tsv, strlen(tsv));
  CHECK("0:\"a,,\",\",,b\",\",c,\n1: , ,\", \n2:,\", fox,, \n" == setup.out.str());

  TRI_DestroyCsvParser(&parser);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test tsv crlf
////////////////////////////////////////////////////////////////////////////////

SECTION("tst_tsv_crlf") {
  INIT_PARSER
  TRI_SetSeparatorCsvParser(&parser, '\t');
  TRI_SetQuoteCsvParser(&parser, '\0', false);

  const char* tsv = 
    "a" TAB "b" TAB "c" CR LF 
    "the quick" TAB "brown fox jumped" TAB "over the" TAB "lazy" TAB "dog" CR LF; 

  TRI_ParseCsvString(&parser, tsv, strlen(tsv));
  CHECK("0:a,b,c\n1:the quick,brown fox jumped,over the,lazy,dog\n" == setup.out.str());

  TRI_DestroyCsvParser(&parser);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test semicolon separator
////////////////////////////////////////////////////////////////////////////////

SECTION("tst_csv_semicolon") {
  INIT_PARSER
  TRI_SetSeparatorCsvParser(&parser, ';');
  TRI_SetQuoteCsvParser(&parser, '"', true);

  const char* csv = 
    "a;b,c;d;e;" LF
    "f;g;;\"h,;\"" LF
    ";" LF
    ";;i; ;j; ;" LF;

  TRI_ParseCsvString(&parser, csv, strlen(csv));
  CHECK("0:a,b,c,d,e,\n1:f,g,,ESCh,;ESC\n2:,\n3:,,i, ,j, ,\n" == setup.out.str());

  TRI_DestroyCsvParser(&parser);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test semicolon separator, no quotes
////////////////////////////////////////////////////////////////////////////////

SECTION("tst_csv_semicolon_noquote") {
  INIT_PARSER
  TRI_SetSeparatorCsvParser(&parser, ';');
  TRI_SetQuoteCsvParser(&parser, '\0', false);

  const char* csv = 
    "a; b; c; d  ;" CR LF
    CR LF
    " ;" CR LF
    " " CR LF
    "\" abc \" " CR LF;

  TRI_ParseCsvString(&parser, csv, strlen(csv));
  CHECK("0:a, b, c, d  ,\n1:\n2: ,\n3: \n4:\" abc \" \n" == setup.out.str());

  TRI_DestroyCsvParser(&parser);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate tests
////////////////////////////////////////////////////////////////////////////////

}

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
