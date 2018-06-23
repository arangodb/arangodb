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

#include "CollectionRevisionsCache.h"
#include "Basics/ReadLocker.h"
#include "Basics/WriteLocker.h"
#include "Basics/xxhash.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/PhysicalCollection.h"
#include "VocBase/RevisionCacheChunk.h"
#include "Wal/LogfileManager.h"

using namespace arangodb;

namespace {
static inline uint64_t HashKey(void*, TRI_voc_rid_t const* key) {
  return std::hash<TRI_voc_rid_t>()(*key);
//  return XXH64(key, sizeof(TRI_voc_rid_t), 0x12345678);
}

static inline uint64_t HashElement(void*, RevisionCacheEntry const& element) {
  return std::hash<TRI_voc_rid_t>()(element.revisionId);
//  return HashKey(nullptr, &element.revisionId);
}

static bool IsEqualKeyElement(void*, TRI_voc_rid_t const* key,
                              uint64_t hash, RevisionCacheEntry const& element) {
  return *key == element.revisionId;
}

static bool IsEqualElementElement(void*, RevisionCacheEntry const& left,
                                  RevisionCacheEntry const& right) {
  return left == right;
}

} // namespace

CollectionRevisionsCache::CollectionRevisionsCache(LogicalCollection* collection, RevisionCacheChunkAllocator* allocator) 
    : _revisions(HashKey, HashElement, IsEqualKeyElement, IsEqualElementElement, IsEqualElementElement, 8, [this]() -> std::string { return std::string("revisions for ") + this->_collection->name(); }), 
       _collection(collection), 
       _readCache(allocator, this),
       _allowInvalidation(true) {}

CollectionRevisionsCache::~CollectionRevisionsCache() {
  try {
    clear();
  } catch (...) {
    // ignore errors here because of destructor
  }
}

std::string CollectionRevisionsCache::name() const {
  return _collection->name();
}
  
uint32_t CollectionRevisionsCache::chunkSize() const {
  if (_collection->isSystem()) {
    return 512 * 1024; // use small chunks for system collections
  }
  return 0; // means: use system default 
}

void CollectionRevisionsCache::closeWriteChunk() {
  _readCache.closeWriteChunk();
}

void CollectionRevisionsCache::clear() {
  {
    WRITE_LOCKER(locker, _lock);
    _revisions.truncate([](RevisionCacheEntry& entry) { return true; });
  }
  _readCache.clear();
}

size_t CollectionRevisionsCache::size() {
  READ_LOCKER(locker, _lock);
  return _revisions.size();
}

size_t CollectionRevisionsCache::memoryUsage() {
  READ_LOCKER(locker, _lock);
  return _revisions.memoryUsage();
}

size_t CollectionRevisionsCache::chunksMemoryUsage() {
  READ_LOCKER(locker, _lock);
  return _readCache.chunksMemoryUsage();
}

void CollectionRevisionsCache::sizeHint(int64_t hint) {
  if (hint > 256) {
    _revisions.resize(nullptr, static_cast<size_t>(hint));
  }
}
  
// look up a revision
bool CollectionRevisionsCache::lookupRevision(Transaction* trx, ManagedDocumentResult& result, TRI_voc_rid_t revisionId, bool shouldLock) {
  TRI_ASSERT(revisionId != 0);
  
  if (result.lastRevisionId() == revisionId) {
    return true;
  }

  CONDITIONAL_READ_LOCKER(locker, _lock, shouldLock);
  
  RevisionCacheEntry found = _revisions.findByKey(nullptr, &revisionId);

  if (found) {
    TRI_ASSERT(found.revisionId != 0);
    // revision found in hash table
    if (found.isWal()) {
      locker.unlock();
      // document is still in WAL
      // TODO: handle WAL reference counters
      wal::Logfile* logfile = found.logfile();
      // now move it into read cache
      ChunkProtector protector = _readCache.insertAndLease(revisionId, reinterpret_cast<uint8_t const*>(logfile->data() + found.offset()), result);
      TRI_ASSERT(protector);
      // must have succeeded (otherwise an exception was thrown)
      // and insert result into the hash
      insertRevision(revisionId, protector.chunk(), protector.offset(), protector.version(), shouldLock);
      // TODO: handle WAL reference counters
      return true;
    } 

    // document is not in WAL but already in read cache
    ChunkProtector protector = _readCache.readAndLease(found, result);
    if (protector) {
      // found in read cache, and still valid
      return true;
    }
  }
      
  // either revision was not in hash or it was in hash but outdated
  locker.unlock();

  // fetch document from engine
  uint8_t const* vpack = readFromEngine(revisionId);
  if (vpack == nullptr) {
    // engine could not provide the revision
    return false;
  }
  // insert found revision into our hash
  ChunkProtector protector = _readCache.insertAndLease(revisionId, vpack, result);
  TRI_ASSERT(protector);
  // insert result into the hash
  insertRevision(revisionId, protector.chunk(), protector.offset(), protector.version(), shouldLock);
  return true;
}

bool CollectionRevisionsCache::lookupRevisionConditional(Transaction* trx, ManagedDocumentResult& result, TRI_voc_rid_t revisionId, TRI_voc_tick_t maxTick, bool excludeWal, bool shouldLock) {
  // fetch document from engine
  uint8_t const* vpack = readFromEngineConditional(revisionId, maxTick, excludeWal);

  if (vpack == nullptr) {
    // engine could not provide the revision
    return false;
  }
  // insert found revision into our hash
  ChunkProtector protector = _readCache.insertAndLease(revisionId, vpack, result);
  TRI_ASSERT(protector);
  // insert result into the hash
  insertRevision(revisionId, protector.chunk(), protector.offset(), protector.version(), shouldLock);
  return true;
}
  
// insert from chunk
void CollectionRevisionsCache::insertRevision(TRI_voc_rid_t revisionId, RevisionCacheChunk* chunk, uint32_t offset, uint32_t version, bool shouldLock) {
  TRI_ASSERT(revisionId != 0);
  TRI_ASSERT(chunk != nullptr);
  TRI_ASSERT(offset != UINT32_MAX);
  TRI_ASSERT(version != 0 && version != UINT32_MAX);

  CONDITIONAL_WRITE_LOCKER(locker, _lock, shouldLock);
  int res = _revisions.insert(nullptr, RevisionCacheEntry(revisionId, chunk, offset, version));

  if (res != TRI_ERROR_NO_ERROR) {
    _revisions.removeByKey(nullptr, &revisionId);
    // try again
    _revisions.insert(nullptr, RevisionCacheEntry(revisionId, chunk, offset, version));
  }
}

// remove a revision
void CollectionRevisionsCache::removeRevision(TRI_voc_rid_t revisionId) {
  WRITE_LOCKER(locker, _lock);
  _revisions.removeByKey(nullptr, &revisionId);
}

void CollectionRevisionsCache::removeRevisions(std::vector<TRI_voc_rid_t> const& revisions) {
  WRITE_LOCKER(locker, _lock);
  for (auto const& it : revisions) {
    _revisions.removeByKey(nullptr, &it);
  }
}
  
uint8_t const* CollectionRevisionsCache::readFromEngine(TRI_voc_rid_t revisionId) {
  TRI_ASSERT(revisionId != 0);
  // TODO: add proper protection for this call
  return _collection->getPhysical()->lookupRevisionVPack(revisionId);
}

uint8_t const* CollectionRevisionsCache::readFromEngineConditional(TRI_voc_rid_t revisionId, TRI_voc_tick_t maxTick, bool excludeWal) {
  TRI_ASSERT(revisionId != 0);
  // TODO: add proper protection for this call
  return _collection->getPhysical()->lookupRevisionVPackConditional(revisionId, maxTick, excludeWal);
}
