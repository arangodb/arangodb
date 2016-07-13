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

#include "RevisionCache.h"
#include "Basics/Exceptions.h"
#include "Basics/ReadLocker.h"
#include "Basics/WriteLocker.h"
#include "ReadCache/RevisionCacheChunk.h"
    
#include "Logger/Logger.h"

using namespace arangodb;

// the global read-cache for documents
RevisionCache::RevisionCache(size_t defaultChunkSize, size_t totalTargetSize)
    : _defaultChunkSize(defaultChunkSize), _totalTargetSize(totalTargetSize), _totalAllocated(0) {
}

RevisionCache::~RevisionCache() {
  // free all chunks
  WRITE_LOCKER(locker, _chunksLock);
  for (auto& chunk : _usedList) {
    delete chunk;
  }
  for (auto& chunk : _freeList) {
    delete chunk;
  }
}

size_t RevisionCache::totalAllocated() {
  READ_LOCKER(locker, _chunksLock);
  return _totalAllocated;
}

// stores a revision in the read-cache, acquiring a lease
// the collection id is prepended to the actual data in order to quickly access
// the shard-local hash for the revision when cleaning up the chunk 
RevisionReader RevisionCache::storeAndLease(uint64_t collectionId, uint8_t const* data, size_t length) {
  while (true) {
    RevisionCacheChunk* chunk = nullptr;
    {
      READ_LOCKER(locker, _chunksLock);

      // try to find a suitable chunk in the freelist
      if (!_freeList.empty()) {
        chunk = _freeList.back();
        //LOG(ERR) << "FREELIST IS NOT EMPTY. FOUND CHUNK " << chunk << " ON FREELIST";
        try {
          //LOG(ERR) << "TRYING STORE AND LEASE";
          return chunk->storeAndLease(collectionId, data, length);
          // all is well
        } catch (arangodb::basics::Exception const& ex) {
          //LOG(ERR) << "STORE AND LEASE FAILED";
          if (ex.code() == TRI_ERROR_LOCKED) {
          //LOG(ERR) << "CHUNK IS LOCKED";
            // chunk is being garbage collected at the moment
            // we cannot use this chunk but need to create a new one
            chunk = nullptr;
          } 
        }
      }
    }

    if (chunk != nullptr) {
      //LOG(ERR) << "MOVING CHUNK " << chunk << " TO USED LIST";
      moveChunkToUsedList(chunk);
    }
    
    // no suitable chunk found...
    // add a new chunk capable of holding at least the target length
    addChunk(length);

    // and try insertion again in next iteration
    //LOG(ERR) << "REPEATING";
  }
}

// stores a revision in the read-cache, without acquiring a lease
// the collection id is prepended to the actual data in order to quickly access
// the shard-local hash for the revision when cleaning up the chunk 
void RevisionCache::store(uint64_t collectionId, uint8_t const* data, size_t length) {
  while (true) {
    RevisionCacheChunk* chunk = nullptr;
    {
      READ_LOCKER(locker, _chunksLock);

      // try to find a suitable chunk in the freelist
      if (!_freeList.empty()) {
        chunk = _freeList.back();
        try {
          chunk->store(collectionId, data, length);
          // all is well
          return;
        } catch (arangodb::basics::Exception const& ex) {
          if (ex.code() == TRI_ERROR_LOCKED) {
            // chunk is being garbage collected at the moment
            // we cannot use this chunk but need to create a new one
            chunk = nullptr;
          } 
        }
      }
    }
    
    if (chunk != nullptr) {
      moveChunkToUsedList(chunk);
    }

    // no suitable chunk found...
    // add a new chunk capable of holding at least the target length
    addChunk(length);

    // and try insertion again in next iteration
  }
}
  
// run the garbage collection with the intent to free unused chunks
// note: this needs some way to access the shard-local caches
void RevisionCache::garbageCollect(GarbageCollectionCallback const& callback) {
  RevisionCacheChunk* chunk = nullptr;

  {
    WRITE_LOCKER(locker, _chunksLock);

    auto it = _usedList.begin();

    while (it != _usedList.end()) {
      if ((*it)->hasReaders()) {
        // there are active readers for this chunk. cannot collect it now!
        ++it;
      } else if ((*it)->hasReferences()) {
        // there are still external references for this chunk. must not clean it up
        ++it;
      } else {
        chunk = (*it);
        _usedList.erase(it);
        // adjust statistics
        _totalAllocated -= chunk->size();
        break;
      }
    }
  }

  if (chunk != nullptr) {
    // simply delete the chunk for now
    // TODO: implement resetting and reusage for chunks
    chunk->garbageCollect(callback);
    delete chunk;
  }
}

// calculate the size for a new chunk
size_t RevisionCache::newChunkSize(size_t dataLength) const noexcept {
  return (std::max)(_defaultChunkSize, RevisionCacheChunk::physicalSize(dataLength));
}

// adds a new chunk, capable of storing at least dataLength
void RevisionCache::addChunk(size_t dataLength) {
  // create a new chunk with the required size
  size_t const targetSize = newChunkSize(dataLength);
  auto chunk = std::make_unique<RevisionCacheChunk>(static_cast<uint32_t>(targetSize));
  {
    // add chunk to the list of free chunks
    WRITE_LOCKER(locker, _chunksLock);
    _freeList.push_back(chunk.get());
    chunk.release();
    _totalAllocated += targetSize;
  
  //LOG(ERR) << "CREATED NEW CHUNK OF SIZE " << targetSize << ", TOTAL ALLOC: " << _totalAllocated;
  }
}

// moves a chunk from the free list to the used list
// (but only if it's still contained in the free list)
void RevisionCache::moveChunkToUsedList(RevisionCacheChunk* chunk) {
  TRI_ASSERT(chunk != nullptr);
      
  // we have found a chunk but cannot use it because it is full
  // now move it from the free list to the used list if it still
  // is in the free list
  WRITE_LOCKER(locker, _chunksLock);

  for (size_t i = 0; i < _freeList.size(); ++i) {
    if (_freeList[i] == chunk) {
      // found it. move it to the used list
      _usedList.emplace(chunk);
      // and erase it from the free list
      _freeList.erase(_freeList.begin() + i);
      return;
    }
  }
}
    
