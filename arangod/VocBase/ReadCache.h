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

#ifndef ARANGOD_VOCBASE_READ_CACHE_H
#define ARANGOD_VOCBASE_READ_CACHE_H 1

#include "Basics/Common.h"
#include "Basics/Mutex.h"
#include "VocBase/ManagedDocumentResult.h"
#include "VocBase/RevisionCacheChunk.h"
#include "VocBase/RevisionCacheChunkAllocator.h"
#include "VocBase/voc-types.h"
#include "Wal/Logfile.h"

namespace arangodb {

class CollectionRevisionsCache;
class RevisionCacheChunk;

struct ReadCachePosition {
  ReadCachePosition() noexcept : chunk(nullptr), offset(0), version(UINT32_MAX) {}

  ReadCachePosition(RevisionCacheChunk* chunk, uint32_t offset, uint32_t version) noexcept 
          : chunk(chunk), offset(offset), version(version) {
    TRI_ASSERT(version != 0);
  }
  ReadCachePosition(ReadCachePosition const& other) noexcept 
          : chunk(other.chunk), offset(other.offset), version(other.version) {}
  ReadCachePosition(ReadCachePosition && other) noexcept 
          : chunk(other.chunk), offset(other.offset), version(other.version) {}
  ReadCachePosition& operator=(ReadCachePosition const& other) noexcept {
    chunk = other.chunk;
    offset = other.offset;
    version = other.version;
    return *this;
  } 
  ReadCachePosition& operator=(ReadCachePosition&& other) noexcept {
    chunk = other.chunk;
    offset = other.offset;
    version = other.version;
    return *this;
  } 
  
  uint8_t const* vpack() const noexcept;
  uint8_t* vpack() noexcept;
  
  RevisionCacheChunk* chunk;
  uint32_t offset;
  uint32_t version;
};

struct WalPosition {
  WalPosition(wal::Logfile* logfile, uint32_t offset) noexcept 
          : logfile(logfile), offset(offset), version(0) {}
  WalPosition(WalPosition const& other) noexcept 
          : logfile(other.logfile), offset(other.offset), version(0) {}
  WalPosition(WalPosition&& other) noexcept 
          : logfile(other.logfile), offset(other.offset), version(0) {}
  WalPosition& operator=(WalPosition const& other) noexcept {
    logfile = other.logfile;
    offset = other.offset;
    version = 0;
    return *this;
  } 
  WalPosition& operator=(WalPosition&& other) noexcept {
    logfile = other.logfile;
    offset = other.offset;
    version = 0;
    return *this;
  }
  
  wal::Logfile* logfile;
  uint32_t offset;
  uint32_t version; // will always be for a WAL entry (used to disambiguate WAL and Cache entries)
};

static_assert(sizeof(ReadCachePosition) == sizeof(WalPosition), "invalid sizes");

union RevisionCacheValue {
  ReadCachePosition chunk;
  WalPosition wal;
  uint8_t raw[16];

  RevisionCacheValue(RevisionCacheChunk* chunk, uint32_t offset, uint32_t version) noexcept : chunk(chunk, offset, version) {}
  RevisionCacheValue(wal::Logfile* logfile, uint32_t offset) noexcept : wal(logfile, offset) {}
  RevisionCacheValue(RevisionCacheValue const& other) noexcept {
    memcpy(&raw[0], &other.raw[0], sizeof(raw));
  }
  RevisionCacheValue(RevisionCacheValue&& other) noexcept {
    memcpy(&raw[0], &other.raw[0], sizeof(raw));
  }
  RevisionCacheValue& operator=(RevisionCacheValue const& other) = delete;
  RevisionCacheValue& operator=(RevisionCacheValue&& other) = delete;
};
  
struct RevisionCacheEntry {
  TRI_voc_rid_t revisionId;
  RevisionCacheValue data; 
 
  // default ctor used for hash arrays 
  RevisionCacheEntry() noexcept : revisionId(0), data(nullptr, 0, UINT32_MAX) {}

  RevisionCacheEntry(TRI_voc_rid_t revisionId, RevisionCacheChunk* chunk, uint32_t offset, uint32_t version) noexcept : revisionId(revisionId), data(chunk, offset, version) {}
  RevisionCacheEntry(TRI_voc_rid_t revisionId, wal::Logfile* logfile, uint32_t offset) noexcept : revisionId(revisionId), data(logfile, offset) {}
  
  RevisionCacheEntry(RevisionCacheEntry const& other) noexcept : revisionId(other.revisionId), data(other.data) {}
  
  RevisionCacheEntry(RevisionCacheEntry&& other) noexcept : revisionId(other.revisionId), data(std::move(other.data)) {}
  
  RevisionCacheEntry& operator=(RevisionCacheEntry const& other) noexcept {
    revisionId = other.revisionId;
    if (other.isWal()) {
      data.wal = other.data.wal;
    } else {
      data.chunk = other.data.chunk;
    }
    return *this;
  }
  
  RevisionCacheEntry& operator=(RevisionCacheEntry&& other) noexcept {
    revisionId = other.revisionId;
    if (other.isWal()) {
      data.wal = other.data.wal;
    } else {
      data.chunk = other.data.chunk;
    }
    return *this;
  }
  
  inline RevisionCacheChunk* chunk() const noexcept { 
    TRI_ASSERT(isChunk());
    return data.chunk.chunk; 
  }

  inline uint32_t offset() const noexcept {
    if (isWal()) {
      return data.wal.offset;
    }
    return data.chunk.offset;
  }
  
  inline uint32_t version() const noexcept {
    TRI_ASSERT(isChunk());
    return data.chunk.version;
  } 
  
  inline wal::Logfile* logfile() const noexcept {
    TRI_ASSERT(isWal());
    return data.wal.logfile;
  } 

  inline bool isChunk() const noexcept { return data.chunk.version != 0 && data.chunk.version != UINT32_MAX; }
  inline bool isWal() const noexcept { return data.chunk.version == 0; }

  inline operator bool() const noexcept { return revisionId != 0; }

  inline bool operator==(RevisionCacheEntry const& other) const noexcept { 
    return memcmp(this, &other, sizeof(other)) == 0;
  }

};

class ReadCache {
 public: 
  ReadCache(RevisionCacheChunkAllocator* allocator, CollectionRevisionsCache* collectionCache);
  ~ReadCache();

  // clear all chunks currently in use. this is a fast-path deletion without checks
  void clear();

  void closeWriteChunk();

  ChunkProtector readAndLease(RevisionCacheEntry const&, ManagedDocumentResult& result);

  ChunkProtector insertAndLease(TRI_voc_rid_t revisionId, uint8_t const* vpack, ManagedDocumentResult& result);
  
 private:
  RevisionCacheChunkAllocator* _allocator;
  CollectionRevisionsCache* _collectionCache; 

  /// @brief mutex for _writeChunk
  arangodb::Mutex _writeMutex;
  /// @brief chunk that we currently write into. may be a nullptr
  RevisionCacheChunk* _writeChunk;
};

} // namespace arangodb

#endif
