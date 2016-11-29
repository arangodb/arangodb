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

#include "VocBase/ReadCache.h"
#include "VocBase/CollectionRevisionsCache.h"

#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;

uint8_t const* ReadCachePosition::vpack() const noexcept {
  return chunk->data() + offset;
}
  
uint8_t* ReadCachePosition::vpack() noexcept {
  return chunk->data() + offset;
}

ReadCache::ReadCache(RevisionCacheChunkAllocator* allocator, CollectionRevisionsCache* collectionCache) 
        : _allocator(allocator), _collectionCache(collectionCache), 
          _writeChunk(nullptr) {}

ReadCache::~ReadCache() {
  try {
    clear(); // clear all chunks
  } catch (...) {
    // ignore any errors because of destructor
  }
}

// clear all chunks currently in use. this is a fast-path deletion without checks
void ReadCache::clear() {
  closeWriteChunk();

  // tell allocator that it can delete all chunks for the collection
  _allocator->removeCollection(this);
}

void ReadCache::closeWriteChunk() {
  MUTEX_LOCKER(locker, _writeMutex);

  if (_writeChunk != nullptr) {
    // chunk may be still in use
    _allocator->returnUsed(this, _writeChunk);
    _writeChunk = nullptr;
  }
}

ChunkProtector ReadCache::readAndLease(RevisionCacheEntry const& entry, ManagedDocumentResult& result) {
  if (result.hasSeenChunk(entry.chunk()) && entry.offset() != UINT32_MAX) {
    // TODO: don't return a Protector here but a simple value object
    ChunkProtector p(entry.chunk(), entry.offset(), entry.version(), false);
    // handle take-over of resources from ChunkProtector inside ManagedDocumentResult::addExisting()
    // remove ChunkProtector::~ChunkProtector
    result.addExisting(p, entry.revisionId);
    return p;
  }

  ChunkProtector p(entry.chunk(), entry.offset(), entry.version());
  
  if (p) {
    // LOG(ERR) << (void*) &result << ", HAVE NOT SEEN CHUNK: " << entry.chunk() << ", OFFSET: " << entry.offset();
    // handle take-over of resources from ChunkProtector inside ManagedDocumentResult::add()
    // remove ChunkProtector::~ChunkProtector
    result.add(p, entry.revisionId);
  }

  // TODO: don't return a Protector here but a simple value object
  return p;
}

ChunkProtector ReadCache::insertAndLease(TRI_voc_rid_t revisionId, uint8_t const* vpack, ManagedDocumentResult& result) {
  TRI_ASSERT(revisionId != 0);
  TRI_ASSERT(vpack != nullptr);

  uint32_t const size = static_cast<uint32_t>(VPackSlice(vpack).byteSize());

  ChunkProtector p;

  while (true) { 
    RevisionCacheChunk* returnChunk = nullptr;

    {
      MUTEX_LOCKER(locker, _writeMutex);
      
      if (_writeChunk == nullptr) {
        _writeChunk = _allocator->orderChunk(_collectionCache, size, _collectionCache->chunkSize());
        TRI_ASSERT(_writeChunk != nullptr);
      }
     
      TRI_ASSERT(_writeChunk != nullptr); 
      
      uint32_t version = _writeChunk->version();
      TRI_ASSERT(version != 0);
      p = ChunkProtector(_writeChunk, _writeChunk->advanceWritePosition(size), version);

      if (!p) {
        // chunk was full
        returnChunk = _writeChunk; // save _writeChunk value
        _writeChunk = nullptr; // reset _writeChunk and try again
      } else {
        TRI_ASSERT(p.version() != 0);
      } 
    }
      
    // check result without mutex
    
    if (returnChunk != nullptr) {
      _allocator->returnUsed(this, returnChunk);
    } else if (p) {
      // we got a free slot in the chunk. copy data and return target position
      TRI_ASSERT(p.version() != 0);
      memcpy(p.vpack(), vpack, size); 
      
      TRI_ASSERT(VPackSlice(p.vpack()).isObject());
      RevisionCacheChunk* chunk = p.chunk();
      try {
        if (result.hasSeenChunk(chunk)) {
          result.addExisting(p, revisionId);
        } else {
          result.add(p, revisionId);
        }
        chunk->unqueueWriter();
      } catch (...) {
        chunk->unqueueWriter();
        throw;
      }

      return p;
    }
    
    // try again
  }
}

