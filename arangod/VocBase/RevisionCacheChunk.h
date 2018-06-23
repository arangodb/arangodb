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

#ifndef ARANGOD_VOCBASE_REVISION_CACHE_CHUNK_H
#define ARANGOD_VOCBASE_REVISION_CACHE_CHUNK_H 1

#include "Basics/Common.h"
#include "Basics/Mutex.h"
#include "Logger/Logger.h"
#include "VocBase/voc-types.h"

namespace arangodb {
class CollectionRevisionsCache;
class RevisionCacheChunk;

class ChunkProtector {
 public:
  ChunkProtector();
  ChunkProtector(RevisionCacheChunk* chunk, uint32_t offset, uint32_t expectedVersion);
  ChunkProtector(RevisionCacheChunk* chunk, uint32_t offset, uint32_t expectedVersion, bool);
  ~ChunkProtector();
  
  ChunkProtector(ChunkProtector const& other) = delete;
  ChunkProtector(ChunkProtector&& other);
  ChunkProtector& operator=(ChunkProtector const& other) = delete;
  ChunkProtector& operator=(ChunkProtector&& other);

  inline operator bool() const { return _chunk != nullptr && _offset != UINT32_MAX; }
  
  void steal();

  uint8_t* vpack();
  uint8_t const* vpack() const;
  RevisionCacheChunk* chunk() const { return _chunk; }
  uint32_t offset() const { return _offset; }
  uint32_t version() const { return _version; }

 private:
  RevisionCacheChunk* _chunk;
  uint32_t _offset;
  uint32_t _version;
  bool _isResponsible;
};


class RevisionCacheChunk {
 public:
  RevisionCacheChunk(RevisionCacheChunk const&) = delete;
  RevisionCacheChunk& operator=(RevisionCacheChunk const&) = delete;

  RevisionCacheChunk(CollectionRevisionsCache* collectionCache, uint32_t size);
  ~RevisionCacheChunk();

 public:
  inline uint32_t size() const noexcept { return _size; }
  
  void reset(CollectionRevisionsCache* collectionCache) { 
    _collectionCache = collectionCache; 
    _nextWriteOffset = 0;
    // initialize chunk with a new version number
    uint32_t newVersion = versionPart(_versionAndRefCount) + 1;
    if (newVersion == UINT32_MAX) {
      newVersion = 1;
    }
    _versionAndRefCount.store(buildVersion(newVersion));
  }
  
  inline uint8_t const* data() const noexcept { return _data; }
  inline uint8_t* data() noexcept { return _data; }
  
  inline uint32_t version() { return versionPart(_versionAndRefCount); }
  
  void unqueueWriter();
  uint32_t advanceWritePosition(uint32_t size);

  /// @brief increases the refcount value if the chunk's version matches
  /// the specified version. returns true then, false otherwise
  bool use(uint32_t expectedVersion) noexcept;
  bool use() noexcept;

  /// @brief decrease the refcount value originally increased by increase()
  void release() noexcept;
  
  bool isUsed() noexcept;

  bool invalidate(std::vector<TRI_voc_rid_t>& revisions);
        
  void wipeout() {
#ifdef TRI_ENABLE_MAINTAINER_MODE
    // overwrite data with garbage
    memset(_data, 0xff, _size);
#endif
  }
  
  // align the length value to a multiple of blockSize
  static constexpr inline uint32_t alignSize(uint32_t value, uint32_t blockSize) {
    return (value + (blockSize - 1)) - ((value + (blockSize - 1)) & (blockSize - 1));
  }
  
 private:
  
  bool findRevisions(std::vector<TRI_voc_rid_t>& revisions);
  void invalidate();
  bool invalidateIfUnused();

  static inline uint32_t versionPart(uint64_t value) {
    return static_cast<uint32_t>((value & 0xffffffff00000000ULL) >> 32);
  }
  
  static inline uint32_t refCountPart(uint64_t value) {
    return static_cast<uint32_t>(value & 0x00000000ffffffffULL);
  }
  
  static inline uint64_t buildVersion(uint32_t value) {
    return static_cast<uint64_t>(value) << 32;
  }
      
  static inline uint64_t increaseVersion(uint64_t value) {
    uint32_t version = versionPart(value);

    do {
      // we must never reach a value of UINT32_MAX because this will mean "not found"
      // additionally we must not reach the value 0 because this will cause confusion
      // with any WAL entries in the collection's hash table
      ++version;
    } while (version == UINT32_MAX || version == 0);

    return (static_cast<uint64_t>(version) << 32) + refCountPart(value);
  }

 private:
  // lock protecting _offset
  mutable arangodb::Mutex _writeMutex;

  CollectionRevisionsCache* _collectionCache;

  // pointer to the chunk's raw memory
  uint8_t* _data;

  // pointer to the current write position
  uint32_t _nextWriteOffset;

  // size of the chunk's memory
  uint32_t const _size;

  // number of writers that wait to copy their data into this chunk
  uint64_t _numWritersQueued;

  // version and reference counter
  // the higher 32 bits contain the version number, the lower 32 bits contain the refcount
  std::atomic<uint64_t> _versionAndRefCount;
};

} // namespace

#endif
