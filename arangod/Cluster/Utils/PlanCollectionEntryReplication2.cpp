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

#include "PlanCollectionEntryReplication2.h"
#include "Inspection/VPack.h"
#include "Replication2/ReplicatedLog/AgencyLogSpecification.h"
#include "Replication2/StateMachines/Document/DocumentStateMachine.h"
#include "VocBase/Properties/CreateCollectionBody.h"
#include "VocBase/LogicalCollection.h"

#include <velocypack/Builder.h>
#include <velocypack/Collection.h>

#include "Logger/LogMacros.h"

using namespace arangodb;
PlanCollectionEntryReplication2::PlanCollectionEntryReplication2(
    UserInputCollectionProperties col, ShardDistribution shardDistribution,
    AgencyIsBuildingFlags isBuildingFlags)
    : _properties{std::move(col)},
      _buildingFlags{std::move(isBuildingFlags)},
      _indexProperties(
          CollectionIndexesProperties::defaultIndexesForCollectionType(
              col.getType())),
      _shardDistribution(std::move(shardDistribution)) {}

std::string PlanCollectionEntryReplication2::getCID() const {
  TRI_ASSERT(!_properties.id.empty());
  return std::to_string(_properties.id.id());
}

std::string const& PlanCollectionEntryReplication2::getName() const {
  TRI_ASSERT(!_properties.name.empty());
  return {_properties.name};
}

bool PlanCollectionEntryReplication2::requiresCurrentWatcher() const {
  return _properties.numberOfShards != 0ull;
}

PlanShardToServerMapping PlanCollectionEntryReplication2::getShardMapping()
    const {
  TRI_ASSERT(_shardDistribution.getDistributionForShards().shards.size() ==
             _properties.numberOfShards);
  return _shardDistribution.getDistributionForShards();
}

[[nodiscard]] replication2::agency::LogTarget
PlanCollectionEntryReplication2::getReplicatedLogForTarget(
    replication2::LogId const& logId, ResponsibleServerList const& serverIds,
    std::string_view databaseName, ShardID const& shardId) const {
  using namespace replication2::replicated_state;

  replication2::agency::LogTarget spec;

  spec.id = logId;

  spec.properties.implementation.type = document::DocumentState::NAME;
  auto parameters = document::DocumentCoreParameters{
      getCID(), std::string{databaseName}, shardId};
  spec.properties.implementation.parameters = parameters.toSharedSlice();

  TRI_ASSERT(!serverIds.servers.empty());
  spec.leader = serverIds.getLeader();

  for (auto const& serverId : serverIds.servers) {
    spec.participants.emplace(serverId, replication2::ParticipantFlags{});
  }

  TRI_ASSERT(_properties.writeConcern.has_value());
  spec.config.writeConcern = _properties.writeConcern.value();
  TRI_ASSERT(_properties.replicationFactor.has_value());
  spec.config.softWriteConcern = _properties.replicationFactor.value();
  spec.config.waitForSync = false;
  spec.version = 1;

  return spec;
}

[[nodiscard]] std::size_t PlanCollectionEntryReplication2::indexOfShardId(
    ShardID const& shard) const {
  TRI_ASSERT(_properties.shardsR2.has_value());
  auto const& it = std::find(_properties.shardsR2->begin(),
                             _properties.shardsR2->end(), shard);
  TRI_ASSERT(it != _properties.shardsR2->end());
  return it - _properties.shardsR2->begin();
}

// Remove the isBuilding flags, call it if we are completed
void PlanCollectionEntryReplication2::removeBuildingFlags() {
  _buildingFlags = std::nullopt;
}

VPackBuilder PlanCollectionEntryReplication2::toVPackDeprecated() const {
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
  velocypack::serialize(indexes, _indexProperties);
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
