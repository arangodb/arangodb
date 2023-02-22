////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Alexandru Petenchea
////////////////////////////////////////////////////////////////////////////////

#include "Replication2/StateMachines/Document/DocumentStateShardHandler.h"

#include "Cluster/ActionDescription.h"
#include "Cluster/CreateCollection.h"
#include "Cluster/Maintenance.h"
#include "Cluster/ServerState.h"
#include "Cluster/DropCollection.h"

namespace arangodb::replication2::replicated_state::document {

DocumentStateShardHandler::DocumentStateShardHandler(
    GlobalLogIdentifier gid, MaintenanceFeature& maintenanceFeature)
    : _gid(std::move(gid)), _maintenanceFeature(maintenanceFeature) {}

auto DocumentStateShardHandler::createLocalShard(
    ShardID const& shardId, std::string const& collectionId,
    std::shared_ptr<velocypack::Builder> const& properties) -> Result {
  auto serverId = ServerState::instance()->getId();

  maintenance::ActionDescription actionDescription(
      std::map<std::string, std::string>{
          {maintenance::NAME, maintenance::CREATE_COLLECTION},
          {maintenance::COLLECTION, collectionId},
          {maintenance::SHARD, shardId},
          {maintenance::DATABASE, _gid.database},
          {maintenance::SERVER_ID, std::move(serverId)},
          {maintenance::THE_LEADER, "replication2"},
          {maintenance::REPLICATED_LOG_ID, to_string(_gid.id)}},
      maintenance::HIGHER_PRIORITY, false, properties);

  maintenance::CreateCollection collectionCreator(_maintenanceFeature,
                                                  actionDescription);
  bool work = collectionCreator.first();
  if (work) {
    return {TRI_ERROR_INTERNAL,
            fmt::format("Cannot create shard ID {}", shardId)};
  }

  _maintenanceFeature.addDirty(_gid.database);

  return {};
}

Result DocumentStateShardHandler::dropLocalShard(
    ShardID const& shardId, const std::string& collectionId) {
  auto serverId = ServerState::instance()->getId();

  maintenance::ActionDescription actionDescription(
      std::map<std::string, std::string>{
          {maintenance::NAME, maintenance::DROP_COLLECTION},
          {maintenance::COLLECTION, collectionId},
          {maintenance::SHARD, shardId},
          {maintenance::DATABASE, _gid.database},
          {maintenance::SERVER_ID, std::move(serverId)},
          {maintenance::THE_LEADER, "replication2"},
      },
      maintenance::HIGHER_PRIORITY, false);

  maintenance::DropCollection collectionDropper(_maintenanceFeature,
                                                actionDescription);
  bool work = collectionDropper.first();
  if (work) {
    return {TRI_ERROR_INTERNAL,
            fmt::format("Cannot drop shard ID {}", shardId)};
  }

  _maintenanceFeature.addDirty(_gid.database);
  return {};
}

}  // namespace arangodb::replication2::replicated_state::document
