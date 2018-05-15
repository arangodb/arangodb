////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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

#include "ClusterIndex.h"
#include "Basics/VelocyPackHelper.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ticks.h"

#include <rocksdb/comparator.h>

using namespace arangodb;
using namespace arangodb::rocksutils;

// This is the number of distinct elements the index estimator can reliably
// store
// This correlates directly with the memory of the estimator:
// memory == ESTIMATOR_SIZE * 6 bytes

uint64_t const arangodb::RocksDBIndex::ESTIMATOR_SIZE = 4096;

ClusterIndex::ClusterIndex(
    TRI_idx_iid_t id, LogicalCollection* collection,
    std::vector<std::vector<arangodb::basics::AttributeName>> const& attributes,
    bool unique, bool sparse, rocksdb::ColumnFamilyHandle* cf,
    uint64_t objectId, bool useCache)
    : Index(id, collection, attributes, unique, sparse) {
}

ClusterIndex::ClusterIndex(TRI_idx_iid_t id, LogicalCollection* collection,
                           VPackSlice const& info,
                           rocksdb::ColumnFamilyHandle* cf, bool useCache)
    : Index(id, collection, info), _indexDefinition(info) {
      _indexDefinition.slice().isObject();
}

ClusterIndex::~ClusterIndex() {

}


void ClusterIndex::toVelocyPackFigures(VPackBuilder& builder) const {
  TRI_ASSERT(builder.isOpenObject());
  Index::toVelocyPackFigures(builder);
  builder.add(VPackObjectIterator(_indexDefinition.slice()));
  // TODO fix
}

/// @brief return a VelocyPack representation of the index
void ClusterIndex::toVelocyPack(VPackBuilder& builder, bool withFigures,
                                bool forPersistence) const {
  builder.openObject();
  Index::toVelocyPack(builder, withFigures, forPersistence);
  builder.add("unique", VPackValue(_unique));
  builder.add("sparse", VPackValue(_sparse));
  
  //static std::vector forbidden = {};
  for (auto pair : VPackObjectIterator(_indexDefinition.slice())) {
    if (!pair.key.equalString("id") &&
        !pair.key.equalString("type") &&
        !pair.key.equalString("fields") &&
        !pair.key.equalString("selectivityEstimate") &&
        !pair.key.equalString("figures") &&
        !pair.key.equalString("unique") &&
        !pair.key.equalString("sparse")) {
      builder.add(pair.key, pair.value);
    }
  }
  builder.close();
}

void ClusterIndex::updateProperties(velocypack::Slice const&) {
#error engine specific properties
}
