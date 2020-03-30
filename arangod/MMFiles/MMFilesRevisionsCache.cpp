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
#include "Logger/Logger.h"
#include "MMFiles/MMFilesDatafileHelper.h"

using namespace arangodb;

MMFilesRevisionsCache::MMFilesRevisionsCache()
    : _positions(MMFilesRevisionsCacheHelper(), 8,
                 []() -> std::string { return "mmfiles revisions"; }) {}

MMFilesRevisionsCache::~MMFilesRevisionsCache() = default;

MMFilesDocumentPosition MMFilesRevisionsCache::lookup(LocalDocumentId const& documentId) const {
  TRI_ASSERT(documentId.isSet());
  READ_LOCKER(locker, _lock);

  return _positions.findByKey(nullptr, documentId.data());
}

void MMFilesRevisionsCache::batchLookup(
    std::vector<std::pair<LocalDocumentId, uint8_t const*>>& documentIds) const {
  READ_LOCKER(locker, _lock);

  for (auto& it : documentIds) {
    MMFilesDocumentPosition const old = _positions.findByKey(nullptr, it.first.data());
    if (old) {
      uint8_t const* vpack = static_cast<uint8_t const*>(old.dataptr());
      TRI_ASSERT(VPackSlice(vpack).isObject());
      it.second = vpack;
    } else {
      it.second = nullptr;
    }
  }
}

void MMFilesRevisionsCache::sizeHint(int64_t hint) {
  WRITE_LOCKER(locker, _lock);
  if (hint > 256) {
    _positions.resize(nullptr, static_cast<size_t>(hint));
  }
}

size_t MMFilesRevisionsCache::size() {
  READ_LOCKER(locker, _lock);
  return _positions.size();
}

size_t MMFilesRevisionsCache::capacity() {
  READ_LOCKER(locker, _lock);
  return _positions.capacity();
}

size_t MMFilesRevisionsCache::memoryUsage() {
  READ_LOCKER(locker, _lock);
  return _positions.memoryUsage();
}

void MMFilesRevisionsCache::clear() {
  WRITE_LOCKER(locker, _lock);
  _positions.truncate([](MMFilesDocumentPosition&) { return true; });
}

MMFilesDocumentPosition MMFilesRevisionsCache::insert(LocalDocumentId const& documentId,
                                                      uint8_t const* dataptr, FileId fid,
                                                      bool isInWal, bool shouldLock) {
  TRI_ASSERT(documentId.isSet());
  TRI_ASSERT(dataptr != nullptr);

  CONDITIONAL_WRITE_LOCKER(locker, _lock, shouldLock);
  int res = _positions.insert(nullptr, MMFilesDocumentPosition(documentId, dataptr,
                                                               fid, isInWal));

  if (res != TRI_ERROR_NO_ERROR) {
    MMFilesDocumentPosition old = _positions.removeByKey(nullptr, documentId.data());
    _positions.insert(nullptr, MMFilesDocumentPosition(documentId, dataptr, fid, isInWal));
    return old;
  }

  return MMFilesDocumentPosition();
}

void MMFilesRevisionsCache::insert(MMFilesDocumentPosition const& position, bool shouldLock) {
  CONDITIONAL_WRITE_LOCKER(locker, _lock, shouldLock);
  _positions.insert(nullptr, position);
}

void MMFilesRevisionsCache::update(LocalDocumentId const& documentId,
                                   uint8_t const* dataptr, FileId fid, bool isInWal) {
  TRI_ASSERT(documentId.isSet());
  TRI_ASSERT(dataptr != nullptr);

  WRITE_LOCKER(locker, _lock);

  MMFilesDocumentPosition* old = _positions.findByKeyRef(nullptr, documentId.data());
  TRI_ASSERT(old != nullptr);

  if (!(*old)) {
    return;
  }

  // update the element in place
  old->dataptr(dataptr);
  old->fid(fid, isInWal);
}

bool MMFilesRevisionsCache::updateConditional(LocalDocumentId const& documentId,
                                              MMFilesMarker const* oldPosition,
                                              MMFilesMarker const* newPosition,
                                              FileId newFid, bool isInWal) {
  WRITE_LOCKER(locker, _lock);

  MMFilesDocumentPosition* old = _positions.findByKeyRef(nullptr, documentId.data());
  TRI_ASSERT(old != nullptr);

  if (!(*old)) {
    return false;
  }

  uint8_t const* vpack = static_cast<uint8_t const*>(old->dataptr());
  TRI_ASSERT(vpack != nullptr);

  MMFilesMarker const* markerPtr = reinterpret_cast<MMFilesMarker const*>(
      vpack - MMFilesDatafileHelper::VPackOffset(TRI_DF_MARKER_VPACK_DOCUMENT));

  if (markerPtr != oldPosition) {
    // element already outdated
    return false;
  }

  old->dataptr(reinterpret_cast<char const*>(newPosition) +
               MMFilesDatafileHelper::VPackOffset(TRI_DF_MARKER_VPACK_DOCUMENT));
  old->fid(newFid, isInWal);

  return true;
}

void MMFilesRevisionsCache::remove(LocalDocumentId const& documentId) {
  TRI_ASSERT(documentId.isSet());

  WRITE_LOCKER(locker, _lock);
  _positions.removeByKey(nullptr, documentId.data());
}

MMFilesDocumentPosition MMFilesRevisionsCache::fetchAndRemove(LocalDocumentId const& documentId) {
  TRI_ASSERT(documentId.isSet());

  WRITE_LOCKER(locker, _lock);
  return _positions.removeByKey(nullptr, documentId.data());
}
