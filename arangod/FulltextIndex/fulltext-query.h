////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_FULLTEXT_INDEX_FULLTEXT_QUERY_H
#define ARANGOD_FULLTEXT_INDEX_FULLTEXT_QUERY_H 1

#include "Basics/Common.h"

////////////////////////////////////////////////////////////////////////////////
/// @brief maximum number of search words in a query
////////////////////////////////////////////////////////////////////////////////

#define TRI_FULLTEXT_SEARCH_MAX_WORDS 32

////////////////////////////////////////////////////////////////////////////////
/// @brief fulltext query match options
////////////////////////////////////////////////////////////////////////////////

typedef enum {
  TRI_FULLTEXT_COMPLETE,
  TRI_FULLTEXT_PREFIX,
  TRI_FULLTEXT_SUBSTRING  // currently not implemented, maybe later
} TRI_fulltext_query_match_e;

////////////////////////////////////////////////////////////////////////////////
/// @brief fulltext query logical operators
////////////////////////////////////////////////////////////////////////////////

typedef enum {
  TRI_FULLTEXT_AND,
  TRI_FULLTEXT_OR,
  TRI_FULLTEXT_EXCLUDE
} TRI_fulltext_query_operation_e;

////////////////////////////////////////////////////////////////////////////////
/// @brief fulltext query
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_fulltext_query_s {
  size_t _numWords;
  char** _words;
  TRI_fulltext_query_match_e* _matches;
  TRI_fulltext_query_operation_e* _operations;
  size_t _maxResults;
} TRI_fulltext_query_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief create a fulltext query
////////////////////////////////////////////////////////////////////////////////

TRI_fulltext_query_t* TRI_CreateQueryFulltextIndex(size_t, size_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief free a fulltext query
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeQueryFulltextIndex(TRI_fulltext_query_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief create a fulltext query from a query string
////////////////////////////////////////////////////////////////////////////////

int TRI_ParseQueryFulltextIndex(TRI_fulltext_query_t*, char const*, bool*);

////////////////////////////////////////////////////////////////////////////////
/// @brief set a search word & option for a query
/// the query will take ownership of the search word
/// the caller must not free the word itself
////////////////////////////////////////////////////////////////////////////////

bool TRI_SetQueryFulltextIndex(TRI_fulltext_query_t*, size_t, char const*,
                               size_t, TRI_fulltext_query_match_e,
                               TRI_fulltext_query_operation_e);

#endif
