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

#ifndef ARANGOD_MMFILES_MMFILES_FULLTEXT_INDEX_H
#define ARANGOD_MMFILES_MMFILES_FULLTEXT_INDEX_H 1

#include <set>

#include "VocBase/Identifiers/LocalDocumentId.h"
#include "VocBase/voc-types.h"
#include "mmfiles-fulltext-common.h"

struct TRI_fulltext_query_s;

/// @brief create a fulltext index
TRI_fts_index_t* TRI_CreateFtsIndex(uint32_t, uint32_t, uint32_t);

/// @brief free a fulltext index
void TRI_FreeFtsIndex(TRI_fts_index_t*);

void TRI_TruncateMMFilesFulltextIndex(TRI_fts_index_t*);

/// @brief insert a list of words to the index
int TRI_InsertWordsMMFilesFulltextIndex(TRI_fts_index_t*,
                                        arangodb::LocalDocumentId const& documentId,
                                        std::set<std::string> const&);

/// @brief insert a list of words to the index
int TRI_RemoveWordsMMFilesFulltextIndex(TRI_fts_index_t*,
                                        arangodb::LocalDocumentId const& documentId,
                                        std::set<std::string> const&);

/// @brief execute a query on the fulltext index
/// note: this will free the query
std::set<TRI_voc_rid_t> TRI_QueryMMFilesFulltextIndex(TRI_fts_index_t* const,
                                                      struct TRI_fulltext_query_s*);

/// @brief return the total memory used by the index
size_t TRI_MemoryMMFilesFulltextIndex(TRI_fts_index_t*);

#endif
