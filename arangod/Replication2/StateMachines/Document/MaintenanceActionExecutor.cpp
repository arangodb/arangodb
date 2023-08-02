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
#include "Cluster/Maintenance.h"
#include "Cluster/DropCollection.h"

namespace arangodb::replication2::replicated_state::document {
MaintenanceActionExecutor::MaintenanceActionExecutor(
    GlobalLogIdentifier gid, ServerID server,
    MaintenanceFeature& maintenanceFeature)
    : _gid(std::move(gid)),
      _maintenanceFeature(maintenanceFeature),
      _server(std::move(server)) {}

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
  bool work = createCollectionAction.first();
  if (work) {
    return {TRI_ERROR_INTERNAL};
  }
  return {};
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
  bool work = dropCollectionAction.first();
  if (work) {
    return {TRI_ERROR_INTERNAL};
  }
  return {};
}

void MaintenanceActionExecutor::addDirty() {
  _maintenanceFeature.addDirty(_gid.database);
}
}  // namespace arangodb::replication2::replicated_state::document
