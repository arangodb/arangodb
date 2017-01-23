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

#ifndef ARANGOD_INDEXES_ROCKS_DB_KEY_COMPARATOR_H
#define ARANGOD_INDEXES_ROCKS_DB_KEY_COMPARATOR_H 1

#include "Basics/Common.h"
#include "Indexes/RocksDBIndex.h"

#include <rocksdb/comparator.h>
#include <rocksdb/slice.h>

#include <velocypack/Slice.h>

namespace arangodb {

class RocksDBKeyComparator : public rocksdb::Comparator {
 public:
  RocksDBKeyComparator() = default;
  ~RocksDBKeyComparator() = default;

  static inline arangodb::velocypack::Slice extractKeySlice(rocksdb::Slice const& slice) {
    return arangodb::velocypack::Slice(slice.data() + RocksDBIndex::keyPrefixSize());
  }
  
  int Compare(rocksdb::Slice const& lhs, rocksdb::Slice const& rhs) const;

  char const* Name() const { return "ArangoComparator"; }
  
  void FindShortestSeparator(std::string*, 
                             rocksdb::Slice const&) const {}
  void FindShortSuccessor(std::string*) const {}
};

}

#endif
