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

#ifndef ARANGOD_READ_CACHE_REVISION_TYPES_H
#define ARANGOD_READ_CACHE_REVISION_TYPES_H 1

#include "Basics/Common.h"
#include "VocBase/voc-types.h"

#include <velocypack/Slice.h>
#include <functional>

namespace arangodb {
class GlobalRevisionCacheChunk;

// type-safe wrapper for an offset inside a chunk
struct RevisionOffset {
  RevisionOffset(uint32_t value) : value(value) {}
  uint32_t value;
};

// type-safe wrapper for a version number
struct RevisionVersion {
  RevisionVersion(uint32_t value) : value(value) {}
  uint32_t value;
};

/* TODO: check if this is necessary
// offset and version number used for a revision in the cache
struct RevisionOffsetVersion {
  RevisionOffsetVersion(uint32_t offset, uint32_t version) : offset(offset), version(version) {}
  RevisionOffsetVersion(RevisionOffset offset, RevisionVersion version) : offset(offset.value), version(version.value) {}
  uint32_t  offset;
  uint32_t  version;
};

static_assert(sizeof(RevisionOffsetVersion) == 8, "invalid size for RevisionOffsetVersion");
*/

class RevisionLocation {
 private:
  struct WalLocation {
    WalLocation(TRI_voc_fid_t datafileId, RevisionOffset offset)
        : datafileId(datafileId), offset(offset.value) {}
    
    TRI_voc_fid_t        datafileId;
    uint32_t             offset;
  };

  struct RevisionCacheLocation {
    RevisionCacheLocation(GlobalRevisionCacheChunk* chunk, RevisionVersion version, RevisionOffset offset)
        : chunk(chunk), version(version.value), offset(offset.value) {}

    GlobalRevisionCacheChunk*  chunk;
    uint32_t             version;
    uint32_t             offset;
  };

  union Location {
    WalLocation           walLocation;
    RevisionCacheLocation revisionCacheLocation;
    uint8_t               raw[16];

    Location() = delete;

    Location(TRI_voc_fid_t datafileId, RevisionOffset offset) 
        : walLocation(datafileId, offset) {}
    
    Location(GlobalRevisionCacheChunk* chunk, RevisionOffset offset, RevisionVersion version)
        : revisionCacheLocation(chunk, version, offset) {}
  };

 public:
  RevisionLocation() = delete;
  RevisionLocation(TRI_voc_fid_t datafileId, RevisionOffset offset)
      : _location(datafileId, offset) {}
  
  RevisionLocation(GlobalRevisionCacheChunk* chunk, RevisionOffset offset, RevisionVersion version)
      : _location(chunk, offset, version) {} 

  inline bool isInWal() const {
    // TODO: find something more sensible
    return _location.raw[sizeof(_location.raw) - 1] == 1;
  }

  inline TRI_voc_fid_t datafileId() const {
    TRI_ASSERT(isInWal());
    return _location.walLocation.datafileId;
  }
  
  inline bool isInRevisionCache() const {
    // TODO: find something more sensible
    return _location.raw[sizeof(_location.raw) - 1] == 0;
  }

  inline GlobalRevisionCacheChunk* chunk() const {
    TRI_ASSERT(isInRevisionCache());
    return _location.revisionCacheLocation.chunk;
  }

 private:
  Location _location;
};

// garbage collection callback function
typedef std::function<void(uint64_t, arangodb::velocypack::Slice const&)> GarbageCollectionCallback;

}

#endif
