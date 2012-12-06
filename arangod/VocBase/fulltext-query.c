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

#include "BasicsC/common.h"
#include "BasicsC/logging.h"
#include "BasicsC/utf8-helper.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief normalise a word for a fulltext search query
////////////////////////////////////////////////////////////////////////////////

char* TRI_NormaliseWordFulltextIndex (const char* word, const size_t wordLength) {
  char* copy;
  char* copy2;
  size_t outLength;
  int32_t outLength2;

  // normalise string
  copy = TR_normalize_utf8_to_NFC(TRI_UNKNOWN_MEM_ZONE, word, wordLength, &outLength);
  if (copy == NULL) {
    return NULL;
  }

  // lower case string
  copy2 = TR_tolower_utf8(TRI_UNKNOWN_MEM_ZONE, copy, (int32_t) outLength, &outLength2);
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, copy);

  return copy2; 
}

////////////////////////////////////////////////////////////////////////////////
/// @brief free fulltext search query options
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeQueryFulltextIndex (FTS_query_t* query) {
  if (query == 0) {
    return;
  }

  if (query->_localOptions != 0) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, query->_localOptions);
  }
  if (query->_texts != 0) {
    size_t i;

    for (i = 0; i < query->_len; ++i) {
      TRI_Free(TRI_UNKNOWN_MEM_ZONE, query->_texts[i]);
    }

    TRI_Free(TRI_UNKNOWN_MEM_ZONE, query->_texts);
  }

  TRI_Free(TRI_UNKNOWN_MEM_ZONE, query);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief query the fulltext index
////////////////////////////////////////////////////////////////////////////////

FTS_document_ids_t* TRI_FindDocumentsFulltextIndex (FTS_index_t* fulltextIndex,
                                                    FTS_query_t* query) {
  return FTS_FindDocuments(fulltextIndex, query);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief free the results of a fulltext query
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeResultsFulltextIndex (FTS_document_ids_t* result) {
  assert(result);
  FTS_Free_Documents(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
