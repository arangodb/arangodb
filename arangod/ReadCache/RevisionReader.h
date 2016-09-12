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

#ifndef ARANGOD_READ_CACHE_REVISION_READER_H
#define ARANGOD_READ_CACHE_REVISION_READER_H 1

#include "Basics/Common.h"
#include "ReadCache/RevisionTypes.h"

#include <velocypack/Slice.h>

namespace arangodb {

class GlobalRevisionCacheChunk;

// guard that ensures a revision can be read from the global revision cache
// the dtor will automatically free any acquired resources
class RevisionReader {
 public:
  RevisionReader();
  RevisionReader(GlobalRevisionCacheChunk* chunk, RevisionOffset offset, RevisionVersion version);
  ~RevisionReader();
  RevisionReader(RevisionReader const&) = delete;
  RevisionReader& operator=(RevisionReader const&) = delete;
  RevisionReader(RevisionReader&&) noexcept;
  RevisionReader& operator=(RevisionReader&&) noexcept;

  arangodb::velocypack::Slice revision() const;
  uint64_t collectionId() const;

  GlobalRevisionCacheChunk* chunk() const { return _chunk; }
  RevisionOffset offset() const { return _offset; }
  RevisionVersion version() const { return _version; }

  void stealReference() { _ownsReference = false; }

 private:
  GlobalRevisionCacheChunk* _chunk;
  RevisionOffset      _offset;
  RevisionVersion     _version;
  bool                _ownsReference;
  bool                _ownsReader;
};

}

#endif
