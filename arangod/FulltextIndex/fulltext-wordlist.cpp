////////////////////////////////////////////////////////////////////////////////
/// @brief full text search, wordlists
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2013 triAGENS GmbH, Cologne, Germany
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
/// @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "fulltext-wordlist.h"

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create a wordlist
///
/// the words passed to the wordlist will be owned by the wordlist and will be
/// freed when the wordlist is freed
////////////////////////////////////////////////////////////////////////////////

TRI_fulltext_wordlist_t* TRI_CreateWordlistFulltextIndex (char** words,
                                                          size_t numWords) {
  TRI_fulltext_wordlist_t* wordlist = static_cast<TRI_fulltext_wordlist_t*>(TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_fulltext_wordlist_t), false));

  if (wordlist == nullptr) {
    return nullptr;
  }

  wordlist->_words    = words;
  wordlist->_numWords = (uint32_t) numWords;

  return wordlist;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy a wordlist
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyWordlistFulltextIndex (TRI_fulltext_wordlist_t* wordlist) {
  for (uint32_t i = 0; i < wordlist->_numWords; ++i) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, wordlist->_words[i]);
  }

  TRI_Free(TRI_UNKNOWN_MEM_ZONE, wordlist->_words);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief free a wordlist
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeWordlistFulltextIndex (TRI_fulltext_wordlist_t* wordlist) {
  TRI_DestroyWordlistFulltextIndex(wordlist);
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, wordlist);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief sort a wordlist in place
////////////////////////////////////////////////////////////////////////////////

void TRI_SortWordlistFulltextIndex (TRI_fulltext_wordlist_t* wordlist) {
  if (wordlist->_numWords <= 1) {
    // do not sort in this case
    return;
  }

  auto compareSort = [] (char const* l, char const* r) {
    return (strcmp(l, r) < 0);
  };
  std::sort(wordlist->_words, wordlist->_words + wordlist->_numWords, compareSort);
}

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:

