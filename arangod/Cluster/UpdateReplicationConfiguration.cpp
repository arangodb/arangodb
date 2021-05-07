////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#include "UpdateReplicationConfiguration.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Cluster/FollowerInfo.h"
#include "Cluster/MaintenanceFeature.h"
#include "Replication2/ReplicatedLog/Common.h"
#include "RestServer/DatabaseFeature.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/vocbase.h"

#include <Logger/LogMacros.h>
#include <Network/NetworkFeature.h>
#include <Replication2/ReplicatedLog/FakeLogFollower.h>

using namespace arangodb;
using namespace arangodb::maintenance;

UpdateReplicationConfiguration::UpdateReplicationConfiguration(MaintenanceFeature& maintenanceFeature,
                                                               ActionDescription actionDescription)
    : ActionBase(maintenanceFeature, std::move(actionDescription)),
      _databaseName(get(DATABASE)),
      _collectionName(get(COLLECTION)),
      _shardName(get(SHARD)),
      _leader(get(THE_LEADER)) {}

bool UpdateReplicationConfiguration::first() {
#if 0
  using namespace replication2;
  using namespace replication2::replicated_log;
  auto db = feature().server().getFeature<DatabaseFeature>().lookupDatabase(_databaseName);
  if (db == nullptr) {
    return false;
  }
  if (db->replicationVersion() != replication::Version::TWO) {
    return false;
  }

  auto col = db->lookupCollection(_shardName);
  if (col != nullptr) {
    if (auto logId = col->replicatedLogId()) {
      auto& log = db->getReplicatedLogById(*logId);
      if (log.isUnconfigured()) {
        if (_leader.empty()) {
          auto& followers = col->followers();
          if (!followers->get()->empty()) {
            auto replicationFollowers =
                std::vector<std::shared_ptr<AbstractFollower>>{};
            replicationFollowers.reserve(followers->get()->size());
            auto& networkFeature = db->server().getFeature<NetworkFeature>();
            std::transform(followers->get()->begin(), followers->get()->end(),
                           std::back_inserter(replicationFollowers),
                           [&](auto const& followerId) {
                             return std::make_shared<FakeLogFollower>(
                                 networkFeature.pool(), followerId, db->name(), *logId);
                           });

            log.becomeLeader(ServerState::instance()->getId(), LogTerm{1},
                             replicationFollowers, col->writeConcern());
          }
        } else {
          log.becomeFollower(ServerState::instance()->getId(), LogTerm{1}, _leader);
        }
      }
    }
  }
#endif

  return false;
}
