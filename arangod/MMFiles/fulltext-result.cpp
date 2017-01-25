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

#include "fulltext-result.h"

#include "StorageEngine/DocumentIdentifierToken.h"

/// @brief create a result
TRI_fulltext_result_t* TRI_CreateResultMMFilesFulltextIndex(const uint32_t size) {
  TRI_fulltext_result_t* result = static_cast<TRI_fulltext_result_t*>(
      TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_fulltext_result_t), false));

  if (result == nullptr) {
    return nullptr;
  }

  result->_documents = nullptr;
  result->_numDocuments = 0;

  if (size > 0) {
    result->_documents = static_cast<arangodb::DocumentIdentifierToken*>(TRI_Allocate(
        TRI_UNKNOWN_MEM_ZONE, sizeof(arangodb::DocumentIdentifierToken) * size, false));

    if (result->_documents == nullptr) {
      TRI_Free(TRI_UNKNOWN_MEM_ZONE, result);
      return nullptr;
    }
  }

  return result;
}

/// @brief destroy a result
void TRI_DestroyResultMMFilesFulltextIndex(TRI_fulltext_result_t* result) {
  if (result->_documents != nullptr) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, result->_documents);
  }
}

/// @brief free a result
void TRI_FreeResultMMFilesFulltextIndex(TRI_fulltext_result_t* result) {
  TRI_DestroyResultMMFilesFulltextIndex(result);
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, result);
}
