////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2022-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "PlanCollectionEntry.h"
#include "Inspection/VPack.h"
#include "VocBase/Properties/PlanCollection.h"
#include "Cluster/Utils/IShardDistributionFactory.h"
#include "PlanShardToServerMappping.h"

#include <velocypack/Builder.h>
#include <velocypack/Collection.h>

using namespace arangodb;
PlanCollectionEntry::PlanCollectionEntry(PlanCollection col,
                                         ShardDistribution shardDistribution)
    : _constantProperties{std::move(col.constantProperties)},
      _mutableProperties{std::move(col.mutableProperties)},
      _internalProperties{std::move(col.internalProperties)},
      _buildingFlags{},
      _indexProperties(
          CollectionIndexesProperties::defaultIndexesForCollectionType(
              _constantProperties.getType())),
      _shardDistribution(std::move(shardDistribution)) {}

std::string const& PlanCollectionEntry::getCID() const {
  return {_internalProperties.id};
}

VPackBuilder PlanCollectionEntry::toVPackDeprecated() const {
  // NOTE this is just a deprecated Helper that will be replaced by inspect as
  // soon as inspector feature is merged
  VPackBuilder constants;
  velocypack::serialize(constants, _constantProperties);
  VPackBuilder mutables;
  velocypack::serialize(mutables, _mutableProperties);
  auto constMut =
      VPackCollection::merge(constants.slice(), mutables.slice(), false, false);
  VPackBuilder internals;
  velocypack::serialize(internals, _internalProperties);
  VPackBuilder flags;
  if (_buildingFlags.has_value()) {
    velocypack::serialize(flags, _buildingFlags.value());
  } else {
    VPackObjectBuilder flagGuard(&flags);
  }
  auto intFlags =
      VPackCollection::merge(internals.slice(), flags.slice(), false, false);

  auto manyFlags =
      VPackCollection::merge(constMut.slice(), intFlags.slice(), false, false);
  VPackBuilder indexes;
  velocypack::serialize(indexes, _indexProperties);
  auto shardMapping = _shardDistribution.getDistributionForShards();
  VPackBuilder shards;
  velocypack::serialize(shards, shardMapping);
  auto shardsAndIndexes =
      VPackCollection::merge(shards.slice(), indexes.slice(), false, false);
  return VPackCollection::merge(manyFlags.slice(), shardsAndIndexes.slice(),
                                false, false);
}

// Remove the isBuilding flags, call it if we are completed
void PlanCollectionEntry::removeBuildingFlags() {
  _buildingFlags = std::nullopt;
}
