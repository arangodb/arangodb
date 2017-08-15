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

#include "mmfiles-fulltext-query.h"
#include "Basics/tri-strings.h"
#include "Basics/Utf8Helper.h"
#include "MMFiles/mmfiles-fulltext-index.h"

/// @brief normalize a word for a fulltext search query
static TRI_fulltext_query_operation_e ParseOperation(char c) {
  if (c == '|') {
    return TRI_FULLTEXT_OR;
  } else if (c == '-') {
    return TRI_FULLTEXT_EXCLUDE;
  }

  // this is the default
  return TRI_FULLTEXT_AND;
}

/// @brief normalize a word for a fulltext search query
/// this will create a copy of the word
static char* NormalizeWord(char const* word, size_t wordLength) {
  // normalize string
  size_t outLength;
  char* copy = TRI_normalize_utf8_to_NFC(TRI_UNKNOWN_MEM_ZONE, word, wordLength,
                                         &outLength);

  if (copy == nullptr) {
    return nullptr;
  }

  // lower case string
  int32_t outLength2;
  char* copy2 = TRI_tolower_utf8(TRI_UNKNOWN_MEM_ZONE, copy, (int32_t)outLength,
                                 &outLength2);
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, copy);

  if (copy2 == nullptr) {
    return nullptr;
  }

  char* prefixEnd = TRI_PrefixUtf8String(copy2, TRI_FULLTEXT_MAX_WORD_LENGTH);
  ptrdiff_t prefixLength = prefixEnd - copy2;

  char* copy3 = static_cast<char*>(TRI_Allocate(
      TRI_UNKNOWN_MEM_ZONE, sizeof(char) * ((size_t)prefixLength + 1)));

  if (copy3 == nullptr) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, copy2);
    return nullptr;
  }

  memcpy(copy3, copy2, ((size_t)prefixLength) * sizeof(char));
  copy3[prefixLength] = '\0';
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, copy2);

  return copy3;
}

/// @brief create a fulltext query
TRI_fulltext_query_t* TRI_CreateQueryMMFilesFulltextIndex(size_t numWords,
                                                   size_t maxResults) {
  TRI_fulltext_query_t* query = static_cast<TRI_fulltext_query_t*>(
      TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_fulltext_query_t)));

  if (query == nullptr) {
    return nullptr;
  }

  // fill word vector with NULLs
  query->_words = static_cast<char**>(
      TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(char*) * numWords));

  if (query->_words == nullptr) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, query);
    return nullptr;
  }

  memset(query->_words, 0, sizeof(char*) * numWords);

  query->_matches = static_cast<TRI_fulltext_query_match_e*>(
      TRI_Allocate(TRI_UNKNOWN_MEM_ZONE,
                   sizeof(TRI_fulltext_query_match_e) * numWords));

  if (query->_matches == nullptr) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, query->_words);
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, query);
    return nullptr;
  }

  query->_operations = static_cast<TRI_fulltext_query_operation_e*>(
      TRI_Allocate(TRI_UNKNOWN_MEM_ZONE,
                   sizeof(TRI_fulltext_query_operation_e) * numWords));

  if (query->_operations == nullptr) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, query->_matches);
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, query->_words);
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, query);
    return nullptr;
  }

  query->_numWords = numWords;
  query->_maxResults = maxResults;

  return query;
}

/// @brief free a fulltext query
void TRI_FreeQueryMMFilesFulltextIndex(TRI_fulltext_query_t* query) {
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, query->_operations);
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, query->_matches);

  for (size_t i = 0; i < query->_numWords; ++i) {
    char* word = query->_words[i];

    if (word != nullptr) {
      TRI_Free(TRI_UNKNOWN_MEM_ZONE, word);
    }
  }

  TRI_Free(TRI_UNKNOWN_MEM_ZONE, query->_words);
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, query);
}

/// @brief create a fulltext query from a query string
int TRI_ParseQueryMMFilesFulltextIndex(TRI_fulltext_query_t* query,
                                char const* queryString,
                                bool* isSubstringQuery) {
  char* ptr;
  size_t i;

  *isSubstringQuery = false;

  ptr = (char*)queryString;
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
    if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' ||
        c == '\b' || c == ',') {
      ++ptr;
      continue;
    }

    // defaults
    operation = TRI_FULLTEXT_AND;
    match = TRI_FULLTEXT_COMPLETE;

    // word begin
    // get operation
    if (c == '+' || c == '-' || c == '|') {
      operation = ParseOperation(c);
      ++ptr;
    }

    split = nullptr;
    start = ptr;
    while (*ptr) {
      c = *ptr;
      if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' ||
          c == '\b' || c == ',') {
        // end of word
        break;
      } else if (split == nullptr && c == ':') {
        split = ptr + 1;
      }
      ++ptr;
    }
    end = ptr;

    if ((end - start == 0) || (split != nullptr && split - start == 0) ||
        (split != nullptr && end - split == 0)) {
      // invalid string
      return TRI_ERROR_BAD_PARAMETER;
    }

    // get command
    if (split != nullptr) {
      if (TRI_CaseEqualString(start, "prefix:", strlen("prefix:"))) {
        match = TRI_FULLTEXT_PREFIX;
      } else if (TRI_CaseEqualString(start, "complete:", strlen("complete:"))) {
        match = TRI_FULLTEXT_COMPLETE;
      }

      start = split;
    }

    TRI_ASSERT(end >= start);

    if (!TRI_SetQueryMMFilesFulltextIndex(query, (size_t)i, start,
                                   (size_t)(end - start), match, operation)) {
      // normalization failed
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

/// @brief set a search word & option for a query
/// the query will take ownership of the search word
bool TRI_SetQueryMMFilesFulltextIndex(TRI_fulltext_query_t* query, size_t position,
                               char const* word, size_t wordLength,
                               TRI_fulltext_query_match_e match,
                               TRI_fulltext_query_operation_e operation) {
  char* normalized;

  if (position >= query->_numWords) {
    return false;
  }

  normalized = NormalizeWord(word, wordLength);

  if (normalized == nullptr) {
    query->_words[position] = nullptr;
    return false;
  }

  query->_words[position] = normalized;
  query->_matches[position] = match;
  query->_operations[position] = operation;

  return true;
}
