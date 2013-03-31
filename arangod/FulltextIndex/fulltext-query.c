////////////////////////////////////////////////////////////////////////////////
/// @brief fulltext query functionality
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

#include "fulltext-query.h"

#include "BasicsC/logging.h"
#include "BasicsC/tri-strings.h"
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
////////////////////////////////////////////////////////////////////////////////

static TRI_fulltext_query_operation_e ParseOperation (const char c) {
  if (c == '|') {
    return TRI_FULLTEXT_OR;
  }
  else if (c == '-') {
    return TRI_FULLTEXT_EXCLUDE;
  }

  // this is the default
  return TRI_FULLTEXT_AND;
}


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

  copy3 = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(char) * ((size_t) prefixLength + 1), false);
  if (copy3 == NULL) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, copy2);
    return NULL;
  }

  memcpy(copy3, copy2, ((size_t) prefixLength) * sizeof(char));
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

  // fill word vector with NULLs
  query->_words = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(char*) * numWords, true);
  if (query->_words == NULL) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, query);
    return NULL;
  }

  query->_matches = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_fulltext_query_match_e) * numWords, false);
  if (query->_matches == NULL) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, query->_words);
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, query);
    return NULL;
  }

  query->_operations = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_fulltext_query_operation_e) * numWords, false);
  if (query->_operations == NULL) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, query->_matches);
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

  TRI_Free(TRI_UNKNOWN_MEM_ZONE, query->_operations);
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, query->_matches);

  for (i = 0; i < query->_numWords; ++i) {
    char* word = query->_words[i];

    if (word != NULL) {
      TRI_Free(TRI_UNKNOWN_MEM_ZONE, word);
    }
  }

  TRI_Free(TRI_UNKNOWN_MEM_ZONE, query->_words);
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
/// @brief create a fulltext query from a query string
////////////////////////////////////////////////////////////////////////////////

int TRI_ParseQueryFulltextIndex (TRI_fulltext_query_t* const query,
                                 const char* const queryString,
                                 bool* isSubstringQuery) {
  char* ptr;
  size_t i;

  *isSubstringQuery = false;

  ptr = (char*) queryString;
  if (*ptr == '\0') {
    return TRI_ERROR_BAD_PARAMETER;
  }

  i = 0;

  while (*ptr) {
    char* start;
    char* end;
    char* split;
    TRI_fulltext_query_operation_e operation;
    TRI_fulltext_query_match_e match;
    char c;

    c = *ptr;

    // ignore whitespace
    if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' || c == '\b' || c == ',') {
      ++ptr;
      continue;
    }

    // defaults
    operation = TRI_FULLTEXT_AND;
    match     = TRI_FULLTEXT_COMPLETE;

    // word begin
    // get operation
    if (c == '+' || c == '-' || c == '|') {
      operation = ParseOperation(c);
      ++ptr;
    }

    split = NULL;
    start = ptr;
    while (*ptr) {
      c = *ptr;
      if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' || c == '\b' || c == ',') {
        // end of word
        break;
      }
      else if (split == NULL && c == ':') {
        split = ptr + 1;
      }
      ++ptr;
    }
    end = ptr;

    if ((end - start == 0) ||
        (split != NULL && split - start == 0) ||
        (split != NULL && end - split == 0)) {
      // invalid string
      return TRI_ERROR_BAD_PARAMETER;
    }

    // get command
    if (split != NULL) {
      if (TRI_CaseEqualString2(start, "prefix:", strlen("prefix:"))) {
        match = TRI_FULLTEXT_PREFIX;
      }
      // 2013-01-17: deactivated substring matching as there is no implementation for it
      // else if (TRI_CaseEqualString2(start, "substring:", strlen("substring:"))) {
      //   match = TRI_FULLTEXT_SUBSTRING;
      // }
      else if (TRI_CaseEqualString2(start, "complete:", strlen("complete:"))) {
        match = TRI_FULLTEXT_COMPLETE;
      }

      start = split;
    }

    assert(end >= start);

    if (! TRI_SetQueryFulltextIndex(query, (size_t) i, start, (size_t) (end - start), match, operation)) {
      // normalisation failed
      return TRI_ERROR_OUT_OF_MEMORY;
    }

    ++i;

    if (i >= TRI_FULLTEXT_SEARCH_MAX_WORDS) {
      break;
    }
  }

  if (i == 0) {
    // no words to search...
    return TRI_ERROR_BAD_PARAMETER;
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief set a search word & option for a query
/// the query will take ownership of the search word
////////////////////////////////////////////////////////////////////////////////

bool TRI_SetQueryFulltextIndex (TRI_fulltext_query_t* const query,
                                const size_t position,
                                const char* const word,
                                const size_t wordLength,
                                TRI_fulltext_query_match_e match,
                                TRI_fulltext_query_operation_e operation) {
  char* normalised;

  if (position >= query->_numWords) {
    return false;
  }

  normalised = NormaliseWord(word, wordLength);
  if (normalised == NULL) {
    query->_words[position] = NULL;
    return false;
  }

  query->_words[position]      = normalised;
  query->_matches[position]    = match;
  query->_operations[position] = operation;

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
