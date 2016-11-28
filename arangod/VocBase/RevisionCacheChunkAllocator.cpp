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

#include "RevisionCacheChunkAllocator.h"
#include "Basics/ConditionLocker.h"
#include "Basics/MutexLocker.h"
#include "Basics/ReadLocker.h"
#include "Basics/WriteLocker.h"
#include "Logger/Logger.h"
#include "VocBase/CollectionRevisionsCache.h"

using namespace arangodb;

RevisionCacheChunkAllocator::RevisionCacheChunkAllocator(uint32_t defaultChunkSize, uint64_t totalTargetSize)
    : _defaultChunkSize(defaultChunkSize), 
      _totalTargetSize(totalTargetSize), 
      _totalAllocated(0) {

  TRI_ASSERT(defaultChunkSize >= 1024);

  _freeList.reserve(4);
}

RevisionCacheChunkAllocator::~RevisionCacheChunkAllocator() {
  try {
    while (garbageCollect()) {
      // garbage collect until nothing more to do
    }
  } catch (...) {
    // must not throw here
  }

  // free all chunks
  WRITE_LOCKER(locker, _chunksLock);

  for (auto& chunk : _freeList) {
    TRI_ASSERT(_totalAllocated >= chunk->size());
    _totalAllocated -= chunk->size();
    deleteChunk(chunk);
  }

  for (auto& it : _fullChunks) {
    for (auto& chunkInfo : it.second) {
      RevisionCacheChunk* chunk = chunkInfo.first;
      TRI_ASSERT(_totalAllocated >= chunk->size());
      _totalAllocated -= chunk->size();
      deleteChunk(chunk);
    }
  }

  TRI_ASSERT(_totalAllocated == 0);
}

void RevisionCacheChunkAllocator::startGcThread() {
  _gcThread.reset(new RevisionCacheGCThread(this));
  
  if (!_gcThread->start()) {
    LOG(FATAL) << "could not start garbage collection thread";
    FATAL_ERROR_EXIT();
  }
}
 
void RevisionCacheChunkAllocator::stopGcThread() { 
  _gcThread->beginShutdown();
  while (_gcThread->isRunning()) {
    usleep(10000);
  }
}

void RevisionCacheChunkAllocator::beginShutdown() {
  _gcThread->beginShutdown();
}
  
// total number of bytes allocated by the cache
uint64_t RevisionCacheChunkAllocator::totalAllocated() {
  READ_LOCKER(locker, _chunksLock);
  return _totalAllocated;
}

RevisionCacheChunk* RevisionCacheChunkAllocator::orderChunk(CollectionRevisionsCache* collectionCache, 
                                                            uint32_t valueSize, uint32_t chunkSize) {
  if (chunkSize == 0) {
    chunkSize = _defaultChunkSize;
  }

  uint32_t const targetSize = RevisionCacheChunk::alignSize((std::max)(valueSize, chunkSize), blockSize());
  {
    // first check if there's a chunk ready on the freelist
    WRITE_LOCKER(locker, _chunksLock);
    if (!_freeList.empty()) {
      RevisionCacheChunk* chunk = _freeList.back();

      if (chunk->size() >= targetSize) {
        // chunk must be big enough...
        _freeList.pop_back();
        chunk->reset(collectionCache);
        return chunk;
      }
    }
  }

  // could not find a big enough chunk on the freelist
  // create a new chunk now
  auto c = std::make_unique<RevisionCacheChunk>(collectionCache, targetSize);

  bool hasMemoryPressure;
  {
    WRITE_LOCKER(locker, _chunksLock);
    // check freelist again
    if (!_freeList.empty()) {
      // a free chunk arrived on the freelist
      RevisionCacheChunk* chunk = _freeList.back();
      if (chunk->size() >= targetSize) {
        // it is big enough
        _freeList.pop_back();
        return chunk;
      }
    }

    // no chunk arrived on the freelist, now return what we created
    _totalAllocated += targetSize;
    hasMemoryPressure = (_totalAllocated > _totalTargetSize);
  }
  
  // LOG(ERR) << "ORDERED NEW CHUNK: " << c.get() << ", SIZE: " << targetSize << ", COLLECTION: " << collectionCache->name();
      
  if (hasMemoryPressure) {
    _gcThread->signal();
  }

  return c.release();
}

void RevisionCacheChunkAllocator::returnUsed(ReadCache* cache, RevisionCacheChunk* chunk) {
  TRI_ASSERT(chunk != nullptr);

  // LOG(ERR) << "RETURNING USED CHUNK: " << chunk;
  bool hasMemoryPressure;
  {
    WRITE_LOCKER(locker, _chunksLock);
    hasMemoryPressure = (_totalAllocated > _totalTargetSize);
  }

  {
    MUTEX_LOCKER(locker, _gcLock);

    auto it = _fullChunks.find(cache);

    if (it == _fullChunks.end()) {
      it = _fullChunks.emplace(cache, std::unordered_map<RevisionCacheChunk*, bool>()).first;
    } 
    (*it).second.emplace(chunk, false);
  }

  if (hasMemoryPressure) {
    _gcThread->signal();
  }
}

void RevisionCacheChunkAllocator::removeCollection(ReadCache* cache) {
  {
    MUTEX_LOCKER(locker, _gcLock);
    
    auto it = _fullChunks.find(cache);

    if (it == _fullChunks.end()) {
      return;
    }

    for (auto& chunkInfo : (*it).second) {
      chunkInfo.second = true; // set to already invalidated
    }
  }
  
  bool hasMemoryPressure;
  {
    WRITE_LOCKER(locker, _chunksLock);
    hasMemoryPressure = (_totalAllocated > _totalTargetSize);
  }

  if (hasMemoryPressure) {
    _gcThread->signal();
  }
}

bool RevisionCacheChunkAllocator::garbageCollect() {
  RevisionCacheChunk* chunk = nullptr;
  bool hasMemoryPressure;

  {
    WRITE_LOCKER(locker, _chunksLock);
    // LOG(ERR) << "gc: total allocated: " << _totalAllocated << ", target: " << _totalTargetSize;
    hasMemoryPressure = (_totalAllocated >= _totalTargetSize);
    
    if (hasMemoryPressure && !_freeList.empty()) {
      // try a chunk from the freelist first
      chunk = _freeList.back();
      _freeList.pop_back();

      TRI_ASSERT(chunk != nullptr);
      // fix statistics here already, because we already have the lock
      TRI_ASSERT(_totalAllocated >= chunk->size());
      _totalAllocated -= chunk->size();
    } 
  }

  if (chunk != nullptr) {
    // delete found chunk outside of lock section
    // LOG(ERR) << "deleting chunk from freelist";
    deleteChunk(chunk);
    return true;
  }

  // nothing found. now check chunks in use
  struct ChunkInfo {
    ReadCache* cache;
    RevisionCacheChunk* chunk;
    bool alreadyInvalidated;
  };
  
  std::vector<ChunkInfo> toCheck;
  toCheck.reserve(128);
  
  {
    // copy list of chunks to check under the mutex into local variable
    MUTEX_LOCKER(locker, _gcLock);
  
    // LOG(ERR) << "CHECKING THE CHUNKS OF " << _fullChunks.size() << " COLLECTIONS";

    for (auto const& it : _fullChunks) {
      for (auto const& chunkInfo : it.second) {
        toCheck.emplace_back(ChunkInfo{it.first, chunkInfo.first, chunkInfo.second});
      }
    }
  }

  // iterate through collections while only briefly acquiring the mutex
  bool worked = false;
  std::vector<TRI_voc_rid_t> revisions;
 
  for (auto& chunkInfo : toCheck) {
    RevisionCacheChunk* chunk = chunkInfo.chunk;
    bool alreadyInvalidated = chunkInfo.alreadyInvalidated;
    // LOG(ERR) << "gc: checking chunk " << chunk << "; invalidated: " << alreadyInvalidated;

    if (alreadyInvalidated) {
      // already invalidated
      if (!chunk->isUsed()) {
        chunk->wipeout();
        
        // remove chunk from full list
        {
          MUTEX_LOCKER(locker, _gcLock);
          
          auto it = _fullChunks.find(chunkInfo.cache);
          
          if (it != _fullChunks.end()) {
            (*it).second.erase(chunk);
            if ((*it).second.empty()) {
              _fullChunks.erase(chunkInfo.cache);
            } 
          }
        }

        // now free the chunk (don't move it into freelist so we
        // can release the chunk's allocated memory here already)
        {
          WRITE_LOCKER(locker, _chunksLock);

          TRI_ASSERT(_totalAllocated >= chunk->size());
          _totalAllocated -= chunk->size();
          deleteChunk(chunk);
        }
        return true;
      }
    } else if (hasMemoryPressure) {
      // LOG(ERR) << "gc: invalidating chunk " << chunk;
      if (chunk->invalidate(revisions)) {
        // LOG(ERR) << "gc: invalidating chunk " << chunk << " done";
        MUTEX_LOCKER(locker, _gcLock);
            
        auto it = _fullChunks.find(chunkInfo.cache);

        if (it != _fullChunks.end()) {
          auto it2 = (*it).second.find(chunk);
          if (it2 != (*it).second.end()) {
            // set to invalidated
            (*it2).second = true;
            worked = true;
          }
        }
      }
    }
  }
 
  // LOG(ERR) << "gc: over. worked: " << worked;
  return worked;
}

void RevisionCacheChunkAllocator::deleteChunk(RevisionCacheChunk* chunk) const {
  // LOG(ERR) << "PHYSICALLY DELETING CHUNK: " << chunk;
  delete chunk;
}

RevisionCacheGCThread::RevisionCacheGCThread(RevisionCacheChunkAllocator* allocator)
        : Thread("ReadCacheCleaner"), _allocator(allocator) {}

void RevisionCacheGCThread::signal() {
  // LOG(ERR) << "signalling gc thread";
  CONDITION_LOCKER(guard, _condition);
  _condition.signal();
}

void RevisionCacheGCThread::beginShutdown() {
  Thread::beginShutdown();

  CONDITION_LOCKER(guard, _condition);
  _condition.signal();
}

void RevisionCacheGCThread::run() {
  while (!isStopping()) {
    try {
      if (!_allocator->garbageCollect()) {
        CONDITION_LOCKER(guard, _condition);
        guard.wait(1000000);
      } // otherwise directly continue the garbage collection
    } catch (std::exception const& ex) {
      LOG(WARN) << "caught exception in ReadCacheCleaner: " << ex.what();
    } catch (...) {
      LOG(WARN) << "caught unknown exception in ReadCacheCleaner";
    }
  }
}

