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

#ifndef ARANGOD_MMFILES_MMFILES_REVISIONS_CACHE_H
#define ARANGOD_MMFILES_MMFILES_REVISIONS_CACHE_H 1

#include "Basics/AssocUnique.h"
#include "Basics/Common.h"
#include "Basics/ReadWriteLock.h"
#include "MMFiles/MMFilesDocumentPosition.h"
#include "VocBase/voc-types.h"

struct MMFilesMarker;

namespace arangodb {

class MMFilesRevisionsCache {
 public:
  MMFilesRevisionsCache();
  ~MMFilesRevisionsCache();

 public:
  void sizeHint(int64_t hint);
  size_t size();
  size_t capacity();
  size_t memoryUsage();
  void clear();
  MMFilesDocumentPosition lookup(TRI_voc_rid_t revisionId) const;
  MMFilesDocumentPosition insert(TRI_voc_rid_t revisionId, uint8_t const* dataptr,
                                 TRI_voc_fid_t fid, bool isInWal, bool shouldLock);
  void insert(MMFilesDocumentPosition const& position, bool shouldLock);
  void update(TRI_voc_rid_t revisionId, uint8_t const* dataptr,
              TRI_voc_fid_t fid, bool isInWal);
  bool updateConditional(TRI_voc_rid_t revisionId, MMFilesMarker const* oldPosition,
                         MMFilesMarker const* newPosition, TRI_voc_fid_t newFid,
                         bool isInWal);
  void remove(TRI_voc_rid_t revisionId);
  MMFilesDocumentPosition fetchAndRemove(TRI_voc_rid_t revisionId);

 private:
  mutable arangodb::basics::ReadWriteLock _lock;

  arangodb::basics::AssocUnique<TRI_voc_rid_t, MMFilesDocumentPosition> _positions;
};

}  // namespace arangodb

#endif
