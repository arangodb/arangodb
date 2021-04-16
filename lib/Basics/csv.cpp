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

#include "csv.h"

#include <cstring>

#include "Basics/debugging.h"
#include "Basics/memory.h"
#include "Basics/voc-errors.h"

////////////////////////////////////////////////////////////////////////////////
/// @brief inits a CSV parser
////////////////////////////////////////////////////////////////////////////////

void TRI_InitCsvParser(
    TRI_csv_parser_t* parser, void (*begin)(TRI_csv_parser_t*, size_t),
    void (*add)(TRI_csv_parser_t*, char const*, size_t, size_t, size_t, bool),
    void (*end)(TRI_csv_parser_t*, char const*, size_t, size_t, size_t, bool), void* vData) {
  size_t length;

  parser->_state = TRI_CSV_PARSER_BOL;
  parser->_data = vData;
  TRI_SetQuoteCsvParser(parser, '"', true);
  TRI_SetSeparatorCsvParser(parser, ';');
  TRI_UseBackslashCsvParser(parser, false);

  length = 1024;

  parser->_row = 0;
  parser->_column = 0;

  parser->_begin = static_cast<char*>(TRI_Allocate(length));

  parser->_start = parser->_begin;
  parser->_written = parser->_begin;
  parser->_current = parser->_begin;
  parser->_stop = parser->_begin;

  if (parser->_begin == nullptr) {
    length = 0;
    parser->_end = nullptr;
  } else {
    parser->_end = parser->_begin + length;
  }

  parser->_dataBegin = nullptr;
  parser->_dataAdd = nullptr;
  parser->_dataEnd = nullptr;

  parser->begin = begin;
  parser->add = add;
  parser->end = end;

  parser->_nResize = 0;
  parser->_nMemmove = 0;
  parser->_nMemcpy = 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a CSV parser
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyCsvParser(TRI_csv_parser_t* parser) {
  if (parser->_begin != nullptr) {
    TRI_Free(parser->_begin);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief set the separator
///
/// note that the separator string must be valid until the parser is destroyed
////////////////////////////////////////////////////////////////////////////////

void TRI_SetSeparatorCsvParser(TRI_csv_parser_t* parser, char separator) {
  parser->_separator = separator;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief set the quote character
////////////////////////////////////////////////////////////////////////////////

void TRI_SetQuoteCsvParser(TRI_csv_parser_t* parser, char quote, bool useQuote) {
  parser->_quote = quote;
  parser->_useQuote = useQuote;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not a backslash is used to escape quotes
////////////////////////////////////////////////////////////////////////////////

void TRI_UseBackslashCsvParser(TRI_csv_parser_t* parser, bool value) {
  parser->_useBackslash = value;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parses a CSV line
////////////////////////////////////////////////////////////////////////////////

ErrorCode TRI_ParseCsvString(TRI_csv_parser_t* parser, char const* line, size_t length) {
  char* ptr;
  char* qtr;

  // append line to buffer
  if (line != nullptr) {
    TRI_ASSERT(parser->_begin <= parser->_start);
    TRI_ASSERT(parser->_start <= parser->_written);
    TRI_ASSERT(parser->_written <= parser->_current);
    TRI_ASSERT(parser->_current <= parser->_stop);
    TRI_ASSERT(parser->_stop <= parser->_end);

    // there is enough room between STOP and END
    if (parser->_stop + length <= parser->_end) {
      memcpy(parser->_stop, line, length);

      parser->_stop += length;
      parser->_nMemcpy++;
    } else {
      size_t l1 = parser->_start - parser->_begin;
      size_t l2 = parser->_end - parser->_stop;
      size_t l3;

      // not enough room, but enough room between BEGIN and START plus STOP and
      // END
      if (length <= l1 + l2) {
        l3 = parser->_stop - parser->_start;

        if (0 < l3) {
          memmove(parser->_begin, parser->_start, l3);
        }

        memcpy(parser->_begin + l3, line, length);

        parser->_start = parser->_begin;
        parser->_written = parser->_written - l1;
        parser->_current = parser->_current - l1;
        parser->_stop = parser->_begin + l3 + length;
        parser->_nMemmove++;
      }

      // really not enough room
      else {
        size_t l4, l5;

        l2 = parser->_stop - parser->_start;
        l3 = parser->_end - parser->_begin + length;
        l4 = parser->_written - parser->_start;
        l5 = parser->_current - parser->_start;

        ptr = static_cast<char*>(TRI_Allocate(l3));

        if (ptr == nullptr) {
          return TRI_ERROR_OUT_OF_MEMORY;
        }

        memcpy(ptr, parser->_start, l2);
        memcpy(ptr + l2, line, length);
        TRI_Free(parser->_begin);

        parser->_begin = ptr;
        parser->_start = ptr;
        parser->_written = ptr + l4;
        parser->_current = ptr + l5;
        parser->_stop = ptr + l2 + length;
        parser->_end = ptr + l3;
        parser->_nResize++;
      }
    }

    // start parsing or continue
    ptr = parser->_current;
    qtr = parser->_written;

    while (true) {
      switch (parser->_state) {
        case TRI_CSV_PARSER_BOL:
          if (ptr == parser->_stop) {
            parser->_written = ptr;
            parser->_current = ptr;
            return TRI_ERROR_NO_ERROR;
          }

          parser->begin(parser, parser->_row);

          parser->_column = 0;
          parser->_state = TRI_CSV_PARSER_BOF;

          break;

        case TRI_CSV_PARSER_BOL2:
          if (ptr == parser->_stop) {
            parser->_written = ptr;
            parser->_current = ptr;
            return TRI_ERROR_NO_ERROR;
          }

          if (*ptr == '\n') {
            ptr++;
          }
          parser->_state = TRI_CSV_PARSER_BOL;

          break;

        case TRI_CSV_PARSER_BOF:
          if (ptr == parser->_stop) {
            parser->_written = ptr;
            parser->_current = ptr;
            return TRI_ERROR_CORRUPTED_CSV;
          }

          else if (parser->_useQuote && *ptr == parser->_quote) {
            if (ptr + 1 == parser->_stop) {
              parser->_written = qtr;
              parser->_current = ptr;
              return TRI_ERROR_CORRUPTED_CSV;
            }

            parser->_state = TRI_CSV_PARSER_WITHIN_QUOTED_FIELD;
            parser->_start = ++ptr;

            qtr = parser->_written = ptr;
          } else {
            parser->_state = TRI_CSV_PARSER_WITHIN_FIELD;
            parser->_start = ptr;

            qtr = parser->_written = ptr;
          }

          break;

        case TRI_CSV_PARSER_CORRUPTED:
          while (ptr < parser->_stop && *ptr != parser->_separator && *ptr != '\n') {
            ptr++;
          }

          // found separator or eol
          if (ptr < parser->_stop) {
            // found separator
            if (*ptr == parser->_separator) {
              ptr++;

              parser->_state = TRI_CSV_PARSER_BOF;
            }

            // found eol
            else {
              ptr++;

              parser->_row++;
              parser->_state = TRI_CSV_PARSER_BOL;
            }
          }

          // need more input
          else {
            parser->_written = qtr;
            parser->_current = ptr;
            return TRI_ERROR_NO_ERROR;
          }

          break;

        case TRI_CSV_PARSER_WITHIN_FIELD:
          while (ptr < parser->_stop && *ptr != parser->_separator &&
                 *ptr != '\r' && *ptr != '\n') {
            *qtr++ = *ptr++;
          }

          // found separator or eol
          if (ptr < parser->_stop) {
            // found separator
            if (*ptr == parser->_separator) {
              *qtr = '\0';

              parser->add(parser, parser->_start, qtr - parser->_start,
                          parser->_row, parser->_column, false);

              ptr++;
              parser->_column++;
              parser->_state = TRI_CSV_PARSER_BOF;
            }

            // found eol
            else {
              char c = *ptr;
              *qtr = '\0';

              parser->end(parser, parser->_start, qtr - parser->_start,
                          parser->_row, parser->_column, false);
              parser->_row++;
              if (c == '\r') {
                parser->_state = TRI_CSV_PARSER_BOL2;
              } else {
                parser->_state = TRI_CSV_PARSER_BOL;
              }

              ptr++;
            }
          }

          // need more input
          else {
            parser->_written = qtr;
            parser->_current = ptr;
            return TRI_ERROR_NO_ERROR;
          }

          break;

        case TRI_CSV_PARSER_WITHIN_QUOTED_FIELD:
          TRI_ASSERT(parser->_useQuote);

          while (ptr < parser->_stop && *ptr != parser->_quote &&
                 (!parser->_useBackslash || *ptr != '\\')) {
            *qtr++ = *ptr++;
          }

          // found quote or a backslash, need at least another quote, a
          // separator, or an eol
          if (ptr + 1 < parser->_stop) {
            bool foundBackslash = (parser->_useBackslash && *ptr == '\\');

            ++ptr;

            if (foundBackslash) {
              if (*ptr == parser->_quote || *ptr == '\\') {
                // backslash-escaped quote or literal backslash
                *qtr++ = *ptr;
                ptr++;
                break;
              }
            } else if (*ptr == parser->_quote) {
              // a real quote
              *qtr++ = parser->_quote;
              ptr++;
              break;
            }

            // ignore spaces
            while ((*ptr == ' ' || *ptr == '\t') && (ptr + 1) < parser->_stop) {
              ++ptr;
            }

            // found separator
            if (*ptr == parser->_separator) {
              *qtr = '\0';

              parser->add(parser, parser->_start, qtr - parser->_start,
                          parser->_row, parser->_column, true);

              ptr++;
              parser->_column++;
              parser->_state = TRI_CSV_PARSER_BOF;
            }

            else if (*ptr == '\r' || *ptr == '\n') {
              char c = *ptr;
              *qtr = '\0';

              parser->end(parser, parser->_start, qtr - parser->_start,
                          parser->_row, parser->_column, true);
              parser->_row++;

              if (c == '\r') {
                parser->_state = TRI_CSV_PARSER_BOL2;
              } else {
                parser->_state = TRI_CSV_PARSER_BOL;
              }

              ptr++;
            }

            // ups
            else {
              parser->_state = TRI_CSV_PARSER_CORRUPTED;
            }
          }

          // need more input
          else {
            parser->_written = qtr;
            parser->_current = ptr;
            return TRI_ERROR_NO_ERROR;
          }

          break;
      }
    }
  }

  return TRI_ERROR_CORRUPTED_CSV;
}
