////////////////////////////////////////////////////////////////////////////////
/// @brief fulltext query functionality
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
/// @author Jan Steemann
/// @author Copyright 2012, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "fulltext-query.h"

#include "BasicsC/logging.h"
#include "BasicsC/strings.h"
#include "BasicsC/utf8-helper.h"

#include "fulltext-index.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Fulltext
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief normalise a word for a fulltext search query
/// this will create a copy of the word
////////////////////////////////////////////////////////////////////////////////

static char* NormaliseWord (const char* const word, const size_t wordLength) { 
  char* copy;
  char* copy2;
  char* copy3;
  char* prefixEnd;
  size_t outLength;
  int32_t outLength2;
  ptrdiff_t prefixLength; 

  // normalise string
  copy = TRI_normalize_utf8_to_NFC(TRI_UNKNOWN_MEM_ZONE, word, wordLength, &outLength);
  if (copy == NULL) {
    return NULL;
  }

  // lower case string
  copy2 = TRI_tolower_utf8(TRI_UNKNOWN_MEM_ZONE, copy, (int32_t) outLength, &outLength2);
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, copy);

  if (copy2 == NULL) {
    return NULL;
  }

  prefixEnd = TRI_PrefixUtf8String(copy2, TRI_FULLTEXT_MAX_WORD_LENGTH);
  prefixLength = prefixEnd - copy2;

  copy3 = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(char) * (prefixLength + 1), false);
  if (copy3 == NULL) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, copy2);
    return NULL;
  }
  
  memcpy(copy3, copy2, prefixLength * sizeof(char));
  copy3[prefixLength] = '\0';
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, copy2);

  return copy3; 
}

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
/// @brief create a fulltext query
////////////////////////////////////////////////////////////////////////////////

TRI_fulltext_query_t* TRI_CreateQueryFulltextIndex (size_t numWords) {
  TRI_fulltext_query_t* query;

  query = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_fulltext_query_t), false);
  if (query == NULL) {
    return NULL;
  }

  query->_words = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(char*) * numWords, true);
  if (query->_words == NULL) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, query);
    return NULL;
  }
  
  query->_options = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_fulltext_query_option_e) * numWords, true);
  if (query->_options == NULL) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, query->_words);
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, query);
    return NULL;
  }

  query->_numWords = numWords;

  return query;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief free a fulltext query
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeQueryFulltextIndex (TRI_fulltext_query_t* query) {
  size_t i;

  for (i = 0; i < query->_numWords; ++i) {
    char* word = query->_words[i];

    if (word != NULL) {
      TRI_Free(TRI_UNKNOWN_MEM_ZONE, word);
    }
  }

  TRI_Free(TRI_UNKNOWN_MEM_ZONE, query->_words);
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, query->_options);
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, query);
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
/// @brief set a search word & option for a query
/// the query will take ownership of the search word
////////////////////////////////////////////////////////////////////////////////

bool TRI_SetQueryFulltextIndex (TRI_fulltext_query_t* const query,
                                const size_t position, 
                                const char* const word, 
                                const size_t wordLength,
                                TRI_fulltext_query_option_e option) {
  char* normalised;

  if (position >= query->_numWords) {
    return false;
  }
  
  normalised = NormaliseWord(word, wordLength);
  if (normalised == NULL) {
    query->_words[position] = NULL;
    return false;
  }

  query->_words[position]   = normalised;
  query->_options[position] = option;

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
