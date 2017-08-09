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

#include "mmfiles-fulltext-common.h"

#include "VocBase/voc-types.h"

struct TRI_fulltext_query_s;
struct TRI_fulltext_result_s;
struct TRI_fulltext_wordlist_s;

/// @brief maximum length of an indexed word in characters
/// a character may consist of up to 4 bytes
#define TRI_FULLTEXT_MAX_WORD_LENGTH 40

/// @brief default minimum word length for a fulltext index
#define TRI_FULLTEXT_MIN_WORD_LENGTH_DEFAULT 2

/// @brief type for index statistics
typedef struct TRI_fulltext_stats_s {
  size_t _memoryTotal;
#if TRI_FULLTEXT_DEBUG
  size_t _memoryOwn;
  size_t _memoryBase;
  size_t _memoryNodes;
  size_t _memoryFollowers;
  size_t _memoryDocuments;
  uint32_t _numNodes;
#endif
  size_t _memoryHandles;
  uint32_t _numDocuments;
  uint32_t _numDeleted;
  double _handleDeletionGrade;
  bool _shouldCompact;
} TRI_fulltext_stats_t;

/// @brief create a fulltext index
TRI_fts_index_t* TRI_CreateFtsIndex(uint32_t, uint32_t, uint32_t);

/// @brief free a fulltext index
void TRI_FreeFtsIndex(TRI_fts_index_t*);

void TRI_TruncateMMFilesFulltextIndex(TRI_fts_index_t*);

/// @brief delete a document from the index
void TRI_DeleteDocumentMMFilesFulltextIndex(TRI_fts_index_t* const,
                                            TRI_voc_rid_t);

/// @brief insert a list of words to the index
bool TRI_InsertWordsMMFilesFulltextIndex(TRI_fts_index_t* const,
                                         TRI_voc_rid_t,
                                         std::set<std::string> const&);

/// @brief find all documents that contain a word (exact match)
#if 0
struct TRI_fulltext_result_s* TRI_FindExactMMFilesFulltextIndex (TRI_fts_index_t* const,
                                                          char const* const,
                                                          size_t const);
#endif

/// @brief find all documents that contain a word (prefix match)
#if 0
struct TRI_fulltext_result_s* TRI_FindPrefixMMFilesFulltextIndex (TRI_fts_index_t* const,
                                                           char const*,
                                                           size_t const);
#endif

/// @brief execute a query on the fulltext index
/// note: this will free the query
struct TRI_fulltext_result_s* TRI_QueryMMFilesFulltextIndex(
    TRI_fts_index_t* const, struct TRI_fulltext_query_s*);

/// @brief dump index tree
#if TRI_FULLTEXT_DEBUG
void TRI_DumpTreeFtsIndex(TRI_fts_index_t*);
#endif

/// @brief dump index statistics
#if TRI_FULLTEXT_DEBUG
void TRI_DumpStatsFtsIndex(TRI_fts_index_t*);
#endif

/// @brief return stats about the index
TRI_fulltext_stats_t TRI_StatsMMFilesFulltextIndex(TRI_fts_index_t*);

/// @brief return the total memory used by the index
size_t TRI_MemoryMMFilesFulltextIndex(TRI_fts_index_t*);

/// @brief compact the fulltext index
bool TRI_CompactMMFilesFulltextIndex(TRI_fts_index_t*);

#endif
