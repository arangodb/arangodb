////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_BASICS_CSV_H
#define ARANGODB_BASICS_CSV_H 1

#include <cstdlib>

#include "Basics/Common.h"
#include "ErrorCode.h"

////////////////////////////////////////////////////////////////////////////////
/// @brief parser states
////////////////////////////////////////////////////////////////////////////////

typedef enum {
  TRI_CSV_PARSER_BOL,
  TRI_CSV_PARSER_BOL2,
  TRI_CSV_PARSER_BOF,
  TRI_CSV_PARSER_WITHIN_FIELD,
  TRI_CSV_PARSER_WITHIN_QUOTED_FIELD,
  TRI_CSV_PARSER_CORRUPTED
} TRI_csv_parser_states_e;

////////////////////////////////////////////////////////////////////////////////
/// @brief template for CSV parser
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_csv_parser_s {
  TRI_csv_parser_states_e _state;

  char _quote;
  char _separator;
  bool _useQuote;
  bool _useBackslash;

  char* _begin;    // beginning of the input buffer
  char* _start;    // start of the unproccessed part
  char* _written;  // pointer to currently written character
  char* _current;  // pointer to currently processed character
  char* _stop;     // end of unproccessed part
  char* _end;      // end of the input buffer

  size_t _row;
  size_t _column;

  void* _dataBegin;
  void* _dataAdd;
  void* _dataEnd;

  void (*begin)(struct TRI_csv_parser_s*, size_t row);
  void (*add)(struct TRI_csv_parser_s*, char const*, size_t, size_t row,
              size_t column, bool escaped);
  void (*end)(struct TRI_csv_parser_s*, char const*, size_t, size_t row,
              size_t column, bool escaped);

  size_t _nResize;
  size_t _nMemmove;
  size_t _nMemcpy;
  void* _data;
} TRI_csv_parser_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief inits a CSV parser
////////////////////////////////////////////////////////////////////////////////

void TRI_InitCsvParser(TRI_csv_parser_t*, void (*)(TRI_csv_parser_t*, size_t),
                       void (*)(TRI_csv_parser_t*, char const*, size_t, size_t, size_t, bool),
                       void (*)(TRI_csv_parser_t*, char const*, size_t, size_t, size_t, bool),
                       void* vData);

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a CSV parser
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyCsvParser(TRI_csv_parser_t* parser);

////////////////////////////////////////////////////////////////////////////////
/// @brief set the separator
///
/// note that the separator string must be valid until the parser is destroyed
////////////////////////////////////////////////////////////////////////////////

void TRI_SetSeparatorCsvParser(TRI_csv_parser_t* parser, char separator);

////////////////////////////////////////////////////////////////////////////////
/// @brief set the quote character
////////////////////////////////////////////////////////////////////////////////

void TRI_SetQuoteCsvParser(TRI_csv_parser_t* parser, char quote, bool useQuote);

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not a backslash is used to escape quotes
////////////////////////////////////////////////////////////////////////////////

void TRI_UseBackslashCsvParser(TRI_csv_parser_t* parser, bool value);

////////////////////////////////////////////////////////////////////////////////
/// @brief parses a CSV line
////////////////////////////////////////////////////////////////////////////////

ErrorCode TRI_ParseCsvString(TRI_csv_parser_t* parser, char const* line, size_t length);

#endif
