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

#ifndef ARANGOD_ROCKSDB_ENGINE_ROCKSDB_INDEX_H
#define ARANGOD_ROCKSDB_ENGINE_ROCKSDB_INDEX_H 1

#include "Basics/AttributeNameParser.h"
#include "Basics/Common.h"
#include "Indexes/Index.h"
#include "RocksDBEngine/RocksDBKeyBounds.h"

namespace arangodb {
class LogicalCollection;
class RocksDBComparator;

class RocksDBIndex : public Index {
 protected:
  RocksDBIndex(TRI_idx_iid_t, LogicalCollection*,
               std::vector<std::vector<arangodb::basics::AttributeName>> const&
                   attributes,
               bool unique, bool sparse, uint64_t objectId = 0);

  RocksDBIndex(TRI_idx_iid_t, LogicalCollection*,
               arangodb::velocypack::Slice const&);

 public:
  uint64_t objectId() const { return _objectId; }

  bool isPersistent() const override final { return true; }
  
protected:
  uint64_t _objectId;
  RocksDBComparator* _cmp;
};
}

#endif
