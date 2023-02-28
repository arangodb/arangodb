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
    : _gid(std::move(gid)),
      _maintenanceFeature(maintenanceFeature),
      _server(ServerState::instance()->getId()) {}

auto DocumentStateShardHandler::ensureShard(
    ShardID shard, CollectionID collection,
    std::shared_ptr<VPackBuilder> properties) -> ResultT<bool> {
  std::unique_lock lock(_shardMap.mutex);
  if (_shardMap.shards.contains(shard)) {
    return false;
  }

  if (auto res = executeCreateCollectionAction(shard, collection, properties);
      res.fail()) {
    return res;
  }

  _shardMap.shards.emplace(
      std::move(shard),
      ShardProperties{std::move(collection), std::move(properties)});
  lock.unlock();

  _maintenanceFeature.addDirty(_gid.database);
  return true;
}

auto DocumentStateShardHandler::dropShard(ShardID const& shard)
    -> ResultT<bool> {
  std::unique_lock lock(_shardMap.mutex);
  auto it = _shardMap.shards.find(shard);
  if (it == std::end(_shardMap.shards)) {
    return false;
  }

  if (auto res = executeDropCollectionAction(shard, it->second.collection);
      res.fail()) {
    return res;
  }
  _shardMap.shards.erase(shard);
  lock.unlock();

  _maintenanceFeature.addDirty(_gid.database);
  return true;
}

auto DocumentStateShardHandler::dropAllShards() -> Result {
  std::unique_lock lock(_shardMap.mutex);
  for (auto const& [shard, properties] : _shardMap.shards) {
    if (auto res = executeDropCollectionAction(shard, properties.collection);
        res.fail()) {
      return Result{res.errorNumber(),
                    fmt::format("Failed to drop shard {}: {}", shard,
                                res.errorMessage())};
    }
  }
  return {};
}

auto DocumentStateShardHandler::isShardAvailable(const ShardID& shard) -> bool {
  std::shared_lock lock(_shardMap.mutex);
  return _shardMap.shards.contains(shard);
}

auto DocumentStateShardHandler::executeCreateCollectionAction(
    ShardID shard, CollectionID collection,
    std::shared_ptr<VPackBuilder> properties) -> Result {
  maintenance::ActionDescription actionDescription(
      std::map<std::string, std::string>{
          {maintenance::NAME, maintenance::CREATE_COLLECTION},
          {maintenance::COLLECTION, std::move(collection)},
          {maintenance::SHARD, std::move(shard)},
          {maintenance::DATABASE, _gid.database},
          {maintenance::SERVER_ID, _server},
          {maintenance::THE_LEADER, "replication2"},
          {maintenance::REPLICATED_LOG_ID, to_string(_gid.id)}},
      maintenance::HIGHER_PRIORITY, false, std::move(properties));
  maintenance::CreateCollection createCollectionAction(_maintenanceFeature,
                                                       actionDescription);
  bool work = createCollectionAction.first();
  if (work) {
    return {TRI_ERROR_INTERNAL};
  }
  return {};
}

auto DocumentStateShardHandler::executeDropCollectionAction(
    ShardID shard, CollectionID collection) -> Result {
  maintenance::ActionDescription actionDescription(
      std::map<std::string, std::string>{
          {maintenance::NAME, maintenance::DROP_COLLECTION},
          {maintenance::COLLECTION, std::move(collection)},
          {maintenance::SHARD, std::move(shard)},
          {maintenance::DATABASE, _gid.database},
          {maintenance::SERVER_ID, _server},
          {maintenance::THE_LEADER, "replication2"},
      },
      maintenance::HIGHER_PRIORITY, false);
  maintenance::DropCollection dropCollectionAction(_maintenanceFeature,
                                                   actionDescription);
  bool work = dropCollectionAction.first();
  if (work) {
    return {TRI_ERROR_INTERNAL};
  }
  return {};
}

auto DocumentStateShardHandler::getShardMap() -> ShardMap {
  std::shared_lock lock(_shardMap.mutex);
  return _shardMap.shards;
}

}  // namespace arangodb::replication2::replicated_state::document
