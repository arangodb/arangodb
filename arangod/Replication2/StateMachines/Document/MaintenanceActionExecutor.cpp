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

#include "Replication2/StateMachines/Document/MaintenanceActionExecutor.h"
#include "Cluster/ActionDescription.h"
#include "Cluster/CreateCollection.h"
#include "Cluster/DropCollection.h"
#include "Cluster/EnsureIndex.h"
#include "Cluster/Maintenance.h"
#include "Cluster/UpdateCollection.h"
#include "Logger/LogMacros.h"
#include "Utils/DatabaseGuard.h"

namespace arangodb::replication2::replicated_state::document {
MaintenanceActionExecutor::MaintenanceActionExecutor(
    GlobalLogIdentifier gid, ServerID server,
    MaintenanceFeature& maintenanceFeature, TRI_vocbase_t& vocbase)
    : _gid(std::move(gid)),
      _maintenanceFeature(maintenanceFeature),
      _server(std::move(server)),
      _vocbase(vocbase) {}

auto MaintenanceActionExecutor::executeCreateCollectionAction(
    ShardID shard, CollectionID collection,
    std::shared_ptr<VPackBuilder> properties) -> Result {
  namespace maintenance = arangodb::maintenance;

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
  createCollectionAction.first();
  return createCollectionAction.result();
}

auto MaintenanceActionExecutor::executeDropCollectionAction(
    ShardID shard, CollectionID collection) -> Result {
  namespace maintenance = arangodb::maintenance;

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
  dropCollectionAction.first();
  return dropCollectionAction.result();
}

auto MaintenanceActionExecutor::executeModifyCollectionAction(
    ShardID shard, CollectionID collection,
    std::shared_ptr<VPackBuilder> properties, std::string followersToDrop)
    -> Result {
  namespace maintenance = arangodb::maintenance;

  maintenance::ActionDescription actionDescription(
      std::map<std::string, std::string>{
          {maintenance::NAME, maintenance::UPDATE_COLLECTION},
          {maintenance::DATABASE, _gid.database},
          {maintenance::COLLECTION, collection},
          {maintenance::SHARD, shard},
          {maintenance::SERVER_ID, _server},
          {maintenance::FOLLOWERS_TO_DROP, followersToDrop}},
      maintenance::HIGHER_PRIORITY, true, std::move(properties));

  maintenance::UpdateCollection updateCollectionAction(_maintenanceFeature,
                                                       actionDescription);
  updateCollectionAction.first();
  return updateCollectionAction.result();
}

auto MaintenanceActionExecutor::executeCreateIndex(
    ShardID shard, std::shared_ptr<VPackBuilder> const& properties,
    std::shared_ptr<methods::Indexes::ProgressTracker> progress) -> Result {
  auto col = _vocbase.lookupCollection(shard);
  if (col == nullptr) {
    return {
        TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
        fmt::format(
            "Failed to lookup shard {} in database {} while creating index {}",
            shard, _vocbase.name(), properties->toJson())};
  }

  VPackBuilder output;
  auto res = methods::Indexes::ensureIndex(*col, properties->slice(), true,
                                           output, std::move(progress));
  if (res.ok()) {
    arangodb::maintenance::EnsureIndex::indexCreationLogging(output.slice());
  }
  return res;
}

auto MaintenanceActionExecutor::executeDropIndex(ShardID shard,
                                                 velocypack::SharedSlice index)
    -> Result {
  auto col = _vocbase.lookupCollection(shard);
  if (col == nullptr) {
    return {
        TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
        fmt::format(
            "Failed to lookup shard {} in database {} while dropping index {}",
            shard, _vocbase.name(), index.toJson())};
  }

  auto res = methods::Indexes::drop(*col, index.slice());
  LOG_TOPIC("e155f", DEBUG, Logger::MAINTENANCE)
      << "Dropping local index " << index.toJson() << " of shard " << shard
      << " in database " << _vocbase.name() << " result: " << res;
  return res;
}

void MaintenanceActionExecutor::addDirty() {
  _maintenanceFeature.addDirty(_gid.database);
}
}  // namespace arangodb::replication2::replicated_state::document
