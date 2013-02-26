////////////////////////////////////////////////////////////////////////////////
/// @brief csv functions
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

#ifndef TRIAGENS_BASICS_C_CSV_H
#define TRIAGENS_BASICS_C_CSV_H 1

#include "BasicsC/common.h"

#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup CSV
/// @{
////////////////////////////////////////////////////////////////////////////////

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
}
TRI_csv_parser_states_e;

////////////////////////////////////////////////////////////////////////////////
/// @brief template for CSV parser
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_csv_parser_s {
  TRI_csv_parser_states_e _state;

  TRI_memory_zone_t* _memoryZone;

  char _quote;
  char _separator;
  bool _useQuote;

  char* _begin;       // beginning of the input buffer
  char* _start;       // start of the unproccessed part
  char* _written;     // pointer to currently written character
  char* _current;     // pointer to currently processed character
  char* _stop;        // end of unproccessed part
  char* _end;         // end of the input buffer

  size_t _row;
  size_t _column;

  void* _dataBegin;
  void* _dataAdd;
  void* _dataEnd;

  void (*begin) (struct TRI_csv_parser_s*, size_t row);
  void (*add) (struct TRI_csv_parser_s*, char const*, size_t row, size_t column, bool escaped);
  void (*end) (struct TRI_csv_parser_s*, char const*, size_t row, size_t column, bool escaped);

  size_t _nResize;
  size_t _nMemmove;
  size_t _nMemcpy;
}
TRI_csv_parser_t;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup CSV
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief inits a CSV parser
////////////////////////////////////////////////////////////////////////////////

void TRI_InitCsvParser (TRI_csv_parser_t*,
                        TRI_memory_zone_t*,
                        void (*) (TRI_csv_parser_t*, size_t),
                        void (*) (TRI_csv_parser_t*, char const*, size_t, size_t, bool),
                        void (*) (TRI_csv_parser_t*, char const*, size_t, size_t, bool));

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a CSV parser
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyCsvParser (TRI_csv_parser_t* parser);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup CSV
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief set the separator
///
/// note that the separator string must be valid until the parser is destroyed
////////////////////////////////////////////////////////////////////////////////

void TRI_SetSeparatorCsvParser (TRI_csv_parser_t* parser, 
                                char separator);

////////////////////////////////////////////////////////////////////////////////
/// @brief set the quote character
////////////////////////////////////////////////////////////////////////////////

void TRI_SetQuoteCsvParser (TRI_csv_parser_t* parser, 
                            char quote,
                            bool useQuote); 

////////////////////////////////////////////////////////////////////////////////
/// @brief parses a CSV line
////////////////////////////////////////////////////////////////////////////////

int TRI_ParseCsvString (TRI_csv_parser_t*, char const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief parses a CSV line
////////////////////////////////////////////////////////////////////////////////

int TRI_ParseCsvString2 (TRI_csv_parser_t*, char const*, size_t);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
