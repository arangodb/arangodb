////////////////////////////////////////////////////////////////////////////////
/// @brief full text search, result list handling
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

#include "fulltext-result.h"

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Fulltext
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief create a result
////////////////////////////////////////////////////////////////////////////////

TRI_fulltext_result_t* TRI_CreateResultFulltextIndex (const uint32_t size) {
  TRI_fulltext_result_t* result;

  result = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_fulltext_result_t), false);
  if (result == NULL) {
    return NULL;
  }

  result->_documents    = NULL;
  result->_numDocuments = 0;

  if (size > 0) {
    result->_documents = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_fulltext_doc_t) * size, false);
    if (result->_documents == NULL) {
      TRI_Free(TRI_UNKNOWN_MEM_ZONE, result);
      return NULL;
    }
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy a result
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyResultFulltextIndex (TRI_fulltext_result_t* result) {
  if (result->_documents != NULL) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, result->_documents);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief free a result
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeResultFulltextIndex (TRI_fulltext_result_t* result) {
  TRI_DestroyResultFulltextIndex(result);
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, result);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
