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
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Fulltext
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief compare words, using fruitsort
////////////////////////////////////////////////////////////////////////////////

#define FSRT_INSTANCE SortFulltext
#define FSRT_NAME SortWordlistFulltext
#define FSRT_NAM2 SortWordlistFulltextTmp
#define FSRT_TYPE const char*

// define copy function to swap two char* values. no need to memcpy them
#define FSRT_COPY(x, y, s)  (*(char**)(x) = *(char**)(y))

#define FSRT_COMP(l, r, s) (strcmp(*l, *r))

uint32_t SortFulltextFSRT_Rand = 0;

static uint32_t SortFulltextRandomGenerator (void) {
  return (SortFulltextFSRT_Rand = SortFulltextFSRT_Rand * 31415 + 27818);
}

#define FSRT__RAND ((fs_b) + FSRT__UNIT * (SortFulltextRandomGenerator() % FSRT__DIST(fs_e,fs_b,FSRT__SIZE)))

#include "BasicsC/fsrt.inc"

#undef FSRT__RAND

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Fulltext
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief create a wordlist
///
/// the words passed to the wordlist will be owned by the wordlist and will be
/// freed when the wordlist is freed
////////////////////////////////////////////////////////////////////////////////

TRI_fulltext_wordlist_t* TRI_CreateWordlistFulltextIndex (char** words,
                                                          const size_t numWords) {
  TRI_fulltext_wordlist_t* wordlist;

  wordlist = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_fulltext_wordlist_t), false);
  if (wordlist == NULL) {
    return NULL;
  }

  wordlist->_words    = words;
  wordlist->_numWords = numWords;

  return wordlist;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy a wordlist
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyWordlistFulltextIndex (TRI_fulltext_wordlist_t* wordlist) {
  uint32_t i;

  for (i = 0; i < wordlist->_numWords; ++i) {
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

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Fulltext
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief sort a wordlist in place
////////////////////////////////////////////////////////////////////////////////

void TRI_SortWordlistFulltextIndex (TRI_fulltext_wordlist_t* const wordlist) {
  if (wordlist->_numWords <= 1) {
    // do not sort in this case
    return;
  }

  SortWordlistFulltext((const char**) wordlist->_words, (const char**) (wordlist->_words + wordlist->_numWords));
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:

