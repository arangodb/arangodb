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

#ifndef ARANGOD_MMFILES_MMFILES_INDEX_H
#define ARANGOD_MMFILES_MMFILES_INDEX_H 1

#include "Basics/Common.h"
#include "Basics/AttributeNameParser.h"
#include "Indexes/Index.h"

#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

namespace arangodb {
class LogicalCollection;

class MMFilesIndex : public Index {
 public:
  MMFilesIndex(
      TRI_idx_iid_t id,
      LogicalCollection& collection,
      std::vector<std::vector<arangodb::basics::AttributeName>> const& attributes,
      bool unique,
      bool sparse
  )
    : Index(id, collection, attributes, unique, sparse) {}

  MMFilesIndex(
      TRI_idx_iid_t id,
      LogicalCollection& collection,
      arangodb::velocypack::Slice const& info
  )
    : Index(id, collection, info) {}


  void afterTruncate(TRI_voc_tick_t) override {
    // for mmfiles, truncating the index just unloads it
    unload();
  }
};
}

#endif
