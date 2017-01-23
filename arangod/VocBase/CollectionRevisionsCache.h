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

#ifndef ARANGOD_VOCBASE_COLLECTION_REVISIONS_CACHE_H
#define ARANGOD_VOCBASE_COLLECTION_REVISIONS_CACHE_H 1

#include "Basics/Common.h"
#include "Basics/AssocUnique.h"
#include "Basics/ReadWriteLock.h"
#include "VocBase/ManagedDocumentResult.h"
#include "VocBase/ReadCache.h"
#include "VocBase/voc-types.h"

namespace arangodb {
class LogicalCollection;
class MMFilesWalLogfile;
class RevisionCacheChunkAllocator;
class Transaction;

class CollectionRevisionsCache {
 public:
  CollectionRevisionsCache(LogicalCollection*, RevisionCacheChunkAllocator*);
  ~CollectionRevisionsCache();
  
 public:
  uint32_t chunkSize() const;
  std::string name() const;

  void closeWriteChunk();
  void clear();

  void sizeHint(int64_t hint);
  size_t size();
  size_t memoryUsage();
  size_t chunksMemoryUsage();

  bool allowInvalidation() const {
    return _allowInvalidation.load();
  }

  void allowInvalidation(bool value) {
    _allowInvalidation.store(value);
  }

  // look up a revision
  bool lookupRevision(arangodb::Transaction* trx, ManagedDocumentResult& result, TRI_voc_rid_t revisionId, bool shouldLock);
  
  // conditionally look up a revision
  bool lookupRevisionConditional(arangodb::Transaction* trx, ManagedDocumentResult& result, TRI_voc_rid_t revisionId, TRI_voc_tick_t maxTick, bool excludeWal, bool shouldLock);
  
  // insert from chunk
  void insertRevision(TRI_voc_rid_t revisionId, RevisionCacheChunk* chunk, uint32_t offset, uint32_t version, bool shouldLock);
  
  // remove a revision
  void removeRevision(TRI_voc_rid_t revisionId);

  // remove multiple revisions
  void removeRevisions(std::vector<TRI_voc_rid_t> const& revisions);

 private:
  uint8_t const* readFromEngine(TRI_voc_rid_t revisionId);
  
  uint8_t const* readFromEngineConditional(TRI_voc_rid_t revisionId, TRI_voc_tick_t maxTick, bool excludeWal);

 private:
  arangodb::basics::ReadWriteLock _lock; 

  arangodb::basics::AssocUnique<TRI_voc_rid_t, RevisionCacheEntry> _revisions;
  
  LogicalCollection* _collection;

  ReadCache _readCache;

  std::atomic<bool> _allowInvalidation;
};

} // namespace arangodb

#endif
