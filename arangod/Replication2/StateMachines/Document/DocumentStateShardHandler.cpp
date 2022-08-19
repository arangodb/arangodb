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
/// @author Alexandru Petenchea
////////////////////////////////////////////////////////////////////////////////

#include "Replication2/StateMachines/Document/DocumentStateShardHandler.h"

#include "Cluster/ActionDescription.h"
#include "Cluster/CreateCollection.h"
#include "Cluster/Maintenance.h"
#include "Cluster/ServerState.h"

namespace arangodb::replication2::replicated_state::document {

DocumentStateShardHandler::DocumentStateShardHandler(
    GlobalLogIdentifier gid, MaintenanceFeature& maintenanceFeature)
    : _gid(std::move(gid)), _maintenanceFeature(maintenanceFeature) {}

auto DocumentStateShardHandler::stateIdToShardId(LogId logId) -> std::string {
  return fmt::format("s{}", logId);
}

auto DocumentStateShardHandler::createLocalShard(
    std::string const& collectionId,
    std::shared_ptr<velocypack::Builder> const& properties)
    -> ResultT<std::string> {
  auto shardId = stateIdToShardId(_gid.id);
  auto serverId = ServerState::instance()->getId();

  maintenance::ActionDescription actionDescription(
      std::map<std::string, std::string>{
          {maintenance::NAME, maintenance::CREATE_COLLECTION},
          {maintenance::COLLECTION, collectionId},
          {maintenance::SHARD, shardId},
          {maintenance::DATABASE, _gid.database},
          {maintenance::SERVER_ID, std::move(serverId)},
          {maintenance::THE_LEADER, "replication2"},
      },
      maintenance::HIGHER_PRIORITY, false, properties);

  maintenance::CreateCollection collectionCreator(_maintenanceFeature,
                                                  actionDescription);
  bool work = collectionCreator.first();
  if (work) {
    return ResultT<std::string>::error(
        TRI_ERROR_INTERNAL, fmt::format("Cannot create shard ID {}", shardId));
  }

  return ResultT<std::string>::success(std::move(shardId));
}

}  // namespace arangodb::replication2::replicated_state::document
