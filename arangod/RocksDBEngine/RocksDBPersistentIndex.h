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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_ROCKSDB_ROCKSDB_PERSISTENT_INDEX_H
#define ARANGOD_ROCKSDB_ROCKSDB_PERSISTENT_INDEX_H 1

#include "RocksDBEngine/RocksDBVPackIndex.h"
namespace arangodb {

class RocksDBPersistentIndex final : public RocksDBVPackIndex {
 public:
  RocksDBPersistentIndex() = delete;

  RocksDBPersistentIndex(TRI_idx_iid_t iid, LogicalCollection& coll,
                         arangodb::velocypack::Slice const& info)
      : RocksDBVPackIndex(iid, coll, info) {}

  IndexType type() const override {
    return Index::TRI_IDX_TYPE_PERSISTENT_INDEX;
  }

  char const* typeName() const override { return "rocksdb-persistent"; }

  bool isSorted() const override { return true; }
};

}  // namespace arangodb

#endif