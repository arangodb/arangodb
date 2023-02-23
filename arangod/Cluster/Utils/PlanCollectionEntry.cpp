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
#include "VocBase/Properties/CreateCollectionBody.h"

#include <velocypack/Builder.h>
#include <velocypack/Collection.h>

using namespace arangodb;
PlanCollectionEntry::PlanCollectionEntry(UserInputCollectionProperties col,
                                         ShardDistribution shardDistribution,
                                         AgencyIsBuildingFlags isBuildingFlags)
    : _properties{std::move(col)},
      _buildingFlags{std::move(isBuildingFlags)},
      _indexProperties(
          CollectionIndexesProperties::defaultIndexesForCollectionType(
              col.getType())),
      _shardDistribution(std::move(shardDistribution)) {}

std::string PlanCollectionEntry::getCID() const {
  TRI_ASSERT(!_properties.id.empty());
  return std::to_string(_properties.id.id());
}

std::string const& PlanCollectionEntry::getName() const {
  TRI_ASSERT(!_properties.name.empty());
  return {_properties.name};
}

bool PlanCollectionEntry::requiresCurrentWatcher() const {
  return _properties.numberOfShards != 0ull;
}

PlanShardToServerMapping PlanCollectionEntry::getShardMapping() const {
  TRI_ASSERT(_shardDistribution.getDistributionForShards().shards.size() ==
             _properties.numberOfShards);
  return _shardDistribution.getDistributionForShards();
}

// Remove the isBuilding flags, call it if we are completed
void PlanCollectionEntry::removeBuildingFlags() {
  _buildingFlags = std::nullopt;
}

VPackBuilder PlanCollectionEntry::toVPackDeprecated() const {
  // NOTE this is just a deprecated Helper that will be replaced by inspect as
  // soon as inspector feature is merged
  VPackBuilder props;
  velocypack::serializeWithContext(props, _properties, InspectAgencyContext{});
  VPackBuilder flags;
  if (_buildingFlags.has_value()) {
    velocypack::serialize(flags, _buildingFlags.value());
  } else {
    VPackObjectBuilder flagGuard(&flags);
  }
  VPackBuilder indexes;
  {
    VPackObjectBuilder indexBuilder(&indexes);
    indexes.add(VPackValue("indexes"));
    velocypack::serialize(indexes, _indexProperties);
  }
  auto shardMapping = getShardMapping();
  VPackBuilder shards;
  velocypack::serialize(shards, shardMapping);
  auto shardsAndIndexes =
      VPackCollection::merge(shards.slice(), indexes.slice(), false, false);
  auto propsAndBuilding =
      VPackCollection::merge(props.slice(), flags.slice(), false, false);
  return VPackCollection::merge(propsAndBuilding.slice(),
                                shardsAndIndexes.slice(), false, false);
}
