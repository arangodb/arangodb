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

#ifndef ARANGOD_FULLTEXT_INDEX_FULLTEXT_WORDLIST_H
#define ARANGOD_FULLTEXT_INDEX_FULLTEXT_WORDLIST_H 1

#include "Basics/Common.h"

////////////////////////////////////////////////////////////////////////////////
/// @brief typedef for a fulltext word list
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_fulltext_wordlist_s {
  uint32_t _numWords;
  char** _words;
} TRI_fulltext_wordlist_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief create a wordlist
/// the words passed to the wordlist will be owned by the wordlist and will be
/// freed when the wordlist is freed
////////////////////////////////////////////////////////////////////////////////

TRI_fulltext_wordlist_t* TRI_CreateWordlistFulltextIndex(char**, size_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy a wordlist
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyWordlistFulltextIndex(TRI_fulltext_wordlist_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief free a wordlist
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeWordlistFulltextIndex(TRI_fulltext_wordlist_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief sort a wordlist in place
////////////////////////////////////////////////////////////////////////////////

void TRI_SortWordlistFulltextIndex(TRI_fulltext_wordlist_t*);

#endif
