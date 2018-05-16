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

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;

/*
ClusterIndex::ClusterIndex(
    TRI_idx_iid_t id, LogicalCollection* collection,
    Index::IndexType type,
    std::vector<std::vector<arangodb::basics::AttributeName>> const& attributes,
    bool unique, bool sparse)
    : Index(id, collection, attributes, unique, sparse), _type(type) {
}*/

ClusterIndex::ClusterIndex(TRI_idx_iid_t id, LogicalCollection* collection,
                           Index::IndexType type, VPackSlice const& info)
    : Index(id, collection, info), _type(type), _indexDefinition(info) {
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
    if (!pair.key.isEqualString("id") &&
        !pair.key.isEqualString("type") &&
        !pair.key.isEqualString("fields") &&
        !pair.key.isEqualString("selectivityEstimate") &&
        !pair.key.isEqualString("figures") &&
        !pair.key.isEqualString("unique") &&
        !pair.key.isEqualString("sparse")) {
      builder.add(pair.key);
      builder.add(pair.value);
    }
  }
  builder.close();
}

void ClusterIndex::updateProperties(velocypack::Slice const&) {
//#error engine specific properties
}
