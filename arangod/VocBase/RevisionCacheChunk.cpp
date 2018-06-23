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

#include "RevisionCacheChunk.h"
#include "Basics/MutexLocker.h"
#include "Utils/Transaction.h"
#include "VocBase/CollectionRevisionsCache.h"

using namespace arangodb;

/// @brief create an empty usage object, pointing to nothing
ChunkProtector::ChunkProtector() 
    : _chunk(nullptr), _offset(UINT32_MAX), _version(UINT32_MAX), _isResponsible(false) {}

/// @brief create a valid usage object, pointing to vpack in the read cache
ChunkProtector::ChunkProtector(RevisionCacheChunk* chunk, uint32_t offset, uint32_t expectedVersion) 
        : _chunk(chunk), _offset(offset), _version(expectedVersion), _isResponsible(true) {
  TRI_ASSERT(_chunk != nullptr);
 
  if (offset == UINT32_MAX) {
    // chunk was full
    _chunk = nullptr;
    _isResponsible = false;
  } else if (!_chunk->use(expectedVersion)) {
    // invalid
    _offset = UINT32_MAX;
    _chunk = nullptr;
    _isResponsible = false;
  }
}

/// @brief create a valid usage object, pointing to vpack in the read cache
ChunkProtector::ChunkProtector(RevisionCacheChunk* chunk, uint32_t offset, uint32_t expectedVersion, bool) 
        : _chunk(chunk), _offset(offset), _version(expectedVersion), _isResponsible(false) {
  TRI_ASSERT(_chunk != nullptr);
  TRI_ASSERT(_offset != UINT32_MAX);
}

ChunkProtector::~ChunkProtector() {
  if (_isResponsible) {
    _chunk->release();
  }
}

ChunkProtector::ChunkProtector(ChunkProtector&& other) : _chunk(other._chunk), _offset(other._offset), _version(other._version), _isResponsible(other._isResponsible) {
  other._chunk = nullptr;
  other._isResponsible = false;
}
  
ChunkProtector& ChunkProtector::operator=(ChunkProtector&& other) {
  if (_chunk != nullptr && _chunk != other._chunk && _isResponsible) {
    _chunk->release(); 
  }

  _chunk = other._chunk;
  _offset = other._offset;
  _version = other._version;
  _isResponsible = other._isResponsible;
  other._chunk = nullptr;
  other._isResponsible = false;
  return *this;
}

void ChunkProtector::steal() {
  _isResponsible = false;
}
  
uint8_t* ChunkProtector::vpack() {
  if (_chunk == nullptr) {
    return nullptr;
  }
  return _chunk->data() + _offset;
}

uint8_t const* ChunkProtector::vpack() const {
  if (_chunk == nullptr) {
    return nullptr;
  }
  return _chunk->data() + _offset;
}

// note: version must be 1 or higher as version 0 means "WAL"
RevisionCacheChunk::RevisionCacheChunk(CollectionRevisionsCache* collectionCache, uint32_t size)
        : _collectionCache(collectionCache), _data(nullptr), _nextWriteOffset(0), 
          _size(size), _numWritersQueued(0), _versionAndRefCount(buildVersion(1)) {
  TRI_ASSERT(versionPart(_versionAndRefCount) == 1);
  _data = new uint8_t[size];
}

RevisionCacheChunk::~RevisionCacheChunk() {
  delete[] _data;
}

void RevisionCacheChunk::unqueueWriter() {
  MUTEX_LOCKER(locker, _writeMutex);
  TRI_ASSERT(_numWritersQueued > 0);
  --_numWritersQueued;
}

uint32_t RevisionCacheChunk::advanceWritePosition(uint32_t size) {
  uint32_t offset;
  {
    MUTEX_LOCKER(locker, _writeMutex);

    if (_nextWriteOffset + size > _size) {
      // chunk would be full
      return UINT32_MAX; // means: chunk is full
    }
    offset = _nextWriteOffset;
    _nextWriteOffset += size;

    ++_numWritersQueued;
  }

  return offset;
}

bool RevisionCacheChunk::invalidate(std::vector<TRI_voc_rid_t>& revisions) {
  if (!_collectionCache->allowInvalidation()) {
    return false;
  }

  // wait until all writers have finished
  while (true) {
    {
      MUTEX_LOCKER(locker, _writeMutex);
      if (_numWritersQueued == 0) {
        break;
      }
    }
    usleep(10000);
  }

  revisions.clear();
  revisions.reserve(8192);

  if (!findRevisions(revisions)) {
    return false;
  }
  if (!invalidateIfUnused()) {
    return false;
  }
  if (!revisions.empty()) {
    _collectionCache->removeRevisions(revisions);
  }
  // increase version number once again
  invalidate();
  return true;
}

bool RevisionCacheChunk::findRevisions(std::vector<TRI_voc_rid_t>& revisions) {
  // no need for write mutex here as the chunk is read-only once fully
  // written to
  uint8_t const* data = _data;
  uint8_t const* end = _data + _nextWriteOffset;
  
  while (data < end) {
    // peek into document at the current position
    VPackSlice slice(data);
    TRI_ASSERT(slice.isObject());
    data += slice.byteSize();
    
    try {
      TRI_voc_rid_t rid = Transaction::extractRevFromDocument(slice);
      revisions.emplace_back(rid);
    } catch (...) {
      // LOG(ERR) << "SLICE: " << slice.toJson();
      return false;
    }
  }
  return true;
}

void RevisionCacheChunk::invalidate() { 
  // increasing the chunk's version number
  // this will make all future reads ignore data in the chunk
  while (true) {
    uint64_t old = _versionAndRefCount.load();
    uint64_t desired = increaseVersion(old);
    // LOG(ERR) << "OLD: " << old << ", VERSIONPART(OLD): " << versionPart(old) << ", REFPART(OLD): " << refCountPart(old) << ", DESIRED: " << desired;
    if (_versionAndRefCount.compare_exchange_strong(old, desired)) {
      break;
    }
  } 
}

bool RevisionCacheChunk::invalidateIfUnused() { 
  // increasing the chunk's version number
  // this will make all future reads ignore data in the chunk
  while (true) {
    uint64_t old = _versionAndRefCount.load();
    if (refCountPart(old) > 0) {
      return false;
    }
    uint64_t desired = increaseVersion(old);
    // LOG(ERR) << "OLD: " << old << ", VERSIONPART(OLD): " << versionPart(old) << ", REFPART(OLD): " << refCountPart(old) << ", DESIRED: " << desired;
    if (_versionAndRefCount.compare_exchange_strong(old, desired)) {
      break;
    }
  } 
  return true;
}
  
bool RevisionCacheChunk::use(uint32_t expectedVersion) noexcept {
  uint64_t old = _versionAndRefCount.fetch_add(1);
  if (versionPart(old) != expectedVersion) {
    // version mismatch. now decrease refcount again
    --_versionAndRefCount;
    return false;
  }
  // we have increased the reference counter here
  return true;
}

bool RevisionCacheChunk::use() noexcept {
  ++_versionAndRefCount;
  return true;
}

void RevisionCacheChunk::release() noexcept {
  uint64_t old = _versionAndRefCount.fetch_sub(1);
  TRI_ASSERT(refCountPart(old) > 0);
  // TODO
}

bool RevisionCacheChunk::isUsed() noexcept {
  uint64_t old = _versionAndRefCount.load();
  return refCountPart(old) > 0;
}
  
