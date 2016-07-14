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

#ifndef ARANGOD_READ_CACHE_REVISION_CACHE_H
#define ARANGOD_READ_CACHE_REVISION_CACHE_H 1

#include "Basics/Common.h"
#include "Basics/ReadWriteLock.h"
#include "ReadCache/RevisionReader.h"

#include <velocypack/Slice.h>
#include <list>

namespace arangodb {

class RevisionCacheChunk;

// the global revision cache for documents
class RevisionCache {
 public:
  RevisionCache() = delete;
  RevisionCache(RevisionCache const&) = delete;
  RevisionCache& operator=(RevisionCache const&) = delete;

  // create the cache instance
  RevisionCache(size_t defaultChunkSize, size_t totalTargetSize, GarbageCollectionCallback const& callback);

  // destroy the cache instance
  ~RevisionCache();

  // total number of bytes allocated by the cache
  size_t totalAllocated();

  // stores a revision in the read-cache, acquiring a lease
  // the collection id is prepended to the actual data in order to quickly access
  // the shard-local hash for the revision when cleaning up the chunk 
  RevisionReader storeAndLease(uint64_t collectionId, uint8_t const* data, size_t length);
  
  // stores a revision in the read-cache, acquiring a lease
  RevisionReader storeAndLease(uint64_t collectionId, arangodb::velocypack::Slice const& data) {
    // simply forward to the pointer-based function
    return storeAndLease(collectionId, data.begin(), data.byteSize());
  } 
  
  // stores a revision in the read-cache, without acquiring a lease
  void store(uint64_t collectionId, uint8_t const* data, size_t length);
  
  // stores a revision in the read-cache, without acquiring a lease
  void store(uint64_t collectionId, arangodb::velocypack::Slice const& data) {
    // simply forward to the pointer-based function
    return store(collectionId, data.begin(), data.byteSize());
  } 
  
  // run the garbage collection with the intent to free unused chunks
  bool garbageCollect();

 private:
  // garbage collects a single chunk
  bool garbageCollect(std::unique_ptr<RevisionCacheChunk>& chunk);

  // calculate the size for a new chunk
  size_t newChunkSize(size_t dataLength) const noexcept;

  // adds a new chunk, capable of storing at least dataLength
  // additionally this will move fullChunk into the used list if it is still
  // contained in the free list
  void addChunk(size_t dataLength, RevisionCacheChunk* fullChunk);

  // creates a chunk, or uses an existing one from the cache
  RevisionCacheChunk* buildChunk(size_t targetSize);

 private:
  // lock for the lists of chunks
  arangodb::basics::ReadWriteLock         _chunksLock;

  // filled chunks in a doubly-linked list, with most recently accessed
  // chunks at the head of the list. the chunks at the tail of the list
  // are subject to garbage collection!
  std::unordered_set<RevisionCacheChunk*> _usedList;
  
  // completely (or partially unused) chunks that can still be written to
  std::vector<RevisionCacheChunk*>        _freeList;

  // default size for new memory chunks
  size_t                                  _defaultChunkSize;

  // total target size for all chunks
  size_t                                  _totalTargetSize;

  // total number of bytes allocated by chunks
  size_t                                  _totalAllocated;
 
  // callback function for garbage collection 
  GarbageCollectionCallback               _callback;
};

}

#endif
