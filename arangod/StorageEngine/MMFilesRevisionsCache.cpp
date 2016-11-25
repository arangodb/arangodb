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

#include "MMFilesRevisionsCache.h"
#include "Basics/ReadLocker.h"
#include "Basics/WriteLocker.h"
#include "Basics/xxhash.h"
#include "Logger/Logger.h"
#include "VocBase/DatafileHelper.h"

using namespace arangodb;

namespace {
static inline uint64_t HashKey(void*, TRI_voc_rid_t const* key) {
  return std::hash<TRI_voc_rid_t>()(*key);
//  return XXH64(key, sizeof(TRI_voc_rid_t), 0x12345678);
}

static inline uint64_t HashElement(void*, MMFilesDocumentPosition const& element) {
  return std::hash<TRI_voc_rid_t>()(element.revisionId());
//  TRI_voc_rid_t revisionId = element.revisionId();
//  return HashKey(nullptr, &revisionId);
}

static bool IsEqualKeyElement(void*, TRI_voc_rid_t const* key,
                              uint64_t hash, MMFilesDocumentPosition const& element) {
  return *key == element.revisionId();
}

static bool IsEqualElementElement(void*, MMFilesDocumentPosition const& left,
                                  MMFilesDocumentPosition const& right) {
  return left.revisionId() == right.revisionId();
}

} // namespace

MMFilesRevisionsCache::MMFilesRevisionsCache() 
    : _positions(HashKey, HashElement, IsEqualKeyElement, IsEqualElementElement, IsEqualElementElement, 8, []() -> std::string { return "mmfiles revisions"; }) {}

MMFilesRevisionsCache::~MMFilesRevisionsCache() {}

MMFilesDocumentPosition MMFilesRevisionsCache::lookup(TRI_voc_rid_t revisionId) const {
  TRI_ASSERT(revisionId != 0);
  READ_LOCKER(locker, _lock);

  return _positions.findByKey(nullptr, &revisionId);
}

void MMFilesRevisionsCache::sizeHint(int64_t hint) {
  WRITE_LOCKER(locker, _lock);
  if (hint > 256) {
    _positions.resize(nullptr, static_cast<size_t>(hint * 1.1));
  }
}

void MMFilesRevisionsCache::clear() {
  WRITE_LOCKER(locker, _lock);
  _positions.truncate([](MMFilesDocumentPosition&) { return true; });
}

void MMFilesRevisionsCache::insert(TRI_voc_rid_t revisionId, uint8_t const* dataptr, TRI_voc_fid_t fid, bool isInWal, bool shouldLock) {
  TRI_ASSERT(revisionId != 0);
  TRI_ASSERT(dataptr != nullptr);

  CONDITIONAL_WRITE_LOCKER(locker, _lock, shouldLock);
  int res = _positions.insert(nullptr, MMFilesDocumentPosition(revisionId, dataptr, fid, isInWal));

  if (res != TRI_ERROR_NO_ERROR) {
    _positions.removeByKey(nullptr, &revisionId);
    _positions.insert(nullptr, MMFilesDocumentPosition(revisionId, dataptr, fid, isInWal));
  }
}

void MMFilesRevisionsCache::update(TRI_voc_rid_t revisionId, uint8_t const* dataptr, TRI_voc_fid_t fid, bool isInWal) {
  TRI_ASSERT(revisionId != 0);
  TRI_ASSERT(dataptr != nullptr);

  WRITE_LOCKER(locker, _lock);
  
  MMFilesDocumentPosition old = _positions.removeByKey(nullptr, &revisionId);
  if (!old) {
    return;
  }
     
  old.dataptr(dataptr);
  old.fid(fid, isInWal); 
  
  _positions.insert(nullptr, old);
}
  
bool MMFilesRevisionsCache::updateConditional(TRI_voc_rid_t revisionId, TRI_df_marker_t const* oldPosition, TRI_df_marker_t const* newPosition, TRI_voc_fid_t newFid, bool isInWal) {
  WRITE_LOCKER(locker, _lock);

  MMFilesDocumentPosition old = _positions.findByKey(nullptr, &revisionId);
  if (!old) {
    return false;
  }
     
  uint8_t const* vpack = static_cast<uint8_t const*>(old.dataptr());
  TRI_ASSERT(vpack != nullptr);

  TRI_df_marker_t const* markerPtr = reinterpret_cast<TRI_df_marker_t const*>(vpack - arangodb::DatafileHelper::VPackOffset(TRI_DF_MARKER_VPACK_DOCUMENT));

  if (markerPtr != oldPosition) {
    // element already outdated
    return false;
  }
  
  _positions.removeByKey(nullptr, &revisionId);

  old.dataptr(reinterpret_cast<char const*>(newPosition) + arangodb::DatafileHelper::VPackOffset(TRI_DF_MARKER_VPACK_DOCUMENT));
  old.fid(newFid, isInWal); 

  _positions.insert(nullptr, old);
  
  return true;
}
   
void MMFilesRevisionsCache::remove(TRI_voc_rid_t revisionId) {
  TRI_ASSERT(revisionId != 0);

  WRITE_LOCKER(locker, _lock);
  _positions.removeByKey(nullptr, &revisionId);
}

MMFilesDocumentPosition MMFilesRevisionsCache::fetchAndRemove(TRI_voc_rid_t revisionId) {
  TRI_ASSERT(revisionId != 0);

  WRITE_LOCKER(locker, _lock);
  return _positions.removeByKey(nullptr, &revisionId);
}

