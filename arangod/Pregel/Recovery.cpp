////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "Recovery.h"

#include <algorithm>

#include "Agency/Supervision.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/MutexLocker.h"
#include "Cluster/ServerState.h"
#include "Pregel/Conductor.h"
#include "Pregel/PregelFeature.h"
#include "Pregel/Utils.h"
#include "Pregel/WorkerConfig.h"
#include "Scheduler/Scheduler.h"
#include "Scheduler/SchedulerFeature.h"
#include "VocBase/LogicalCollection.h"

using namespace arangodb;
using namespace arangodb::pregel;

RecoveryManager::RecoveryManager(ClusterInfo& ci)
    : _ci(ci), _agency(ci.server()) {}

RecoveryManager::~RecoveryManager() {
  _listeners.clear();
}

void RecoveryManager::stopMonitoring(Conductor* listener) {
  MUTEX_LOCKER(guard, _lock);

  for (auto& pair : _listeners) {
    if (pair.second.find(listener) != pair.second.end()) {
      pair.second.erase(listener);
    }
  }
}

void RecoveryManager::monitorCollections(DatabaseID const& database,
                                         std::vector<CollectionID> const& collections,
                                         Conductor* listener) {
  MUTEX_LOCKER(guard, _lock);
  if (ServerState::instance()->isCoordinator() == false) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_CLUSTER_ONLY_ON_COORDINATOR);
  }

  for (CollectionID const& collname : collections) {
    std::shared_ptr<LogicalCollection> coll = _ci.getCollection(database, collname);
    CollectionID cid = std::to_string(coll->id().id());
    std::shared_ptr<std::vector<ShardID>> shards = _ci.getShardList(cid);

    if (!shards) {
      continue;
    }

    for (ShardID const& shard : *(shards.get())) {
      std::set<Conductor*>& conductors = _listeners[shard];
      if (conductors.find(listener) != conductors.end()) {
        continue;
      }
      conductors.insert(listener);
      //_monitorShard(coll->dbName(), cid, shard);

      std::shared_ptr<std::vector<ServerID>> servers = _ci.getResponsibleServer(shard);
      if (servers->size() > 0) {
        // _lock is already held
        _primaryServers[shard] = servers->at(0);
      }
    }
  }
}

int RecoveryManager::filterGoodServers(std::vector<ServerID> const& servers,
                                       std::vector<ServerID>& goodServers) {
  // TODO I could also use ClusterInfo::failedServers
  AgencyCommResult result = _agency.getValues("Supervision/Health");
  if (result.successful()) {
    VPackSlice serversRegistered = result.slice()[0].get(std::vector<std::string>(
        {AgencyCommHelper::path(), "Supervision", "Health"}));

    LOG_TOPIC("68f55", INFO, Logger::PREGEL) << "Server Status: " << serversRegistered.toJson();

    if (serversRegistered.isObject()) {
      for (auto const& res : VPackObjectIterator(serversRegistered)) {
        VPackSlice serverId = res.key;
        VPackSlice slice = res.value;
        if (slice.isObject() && slice.hasKey("Status")) {
          VPackSlice status = slice.get("Status");
          if (status.compareString(consensus::Supervision::HEALTH_STATUS_GOOD) == 0) {
            ServerID name = serverId.copyString();
            if (std::find(servers.begin(), servers.end(), name) != servers.end()) {
              goodServers.push_back(name);
            }
          }
        }
      }
    }
  } else {
    return result.errorCode();
  }
  return TRI_ERROR_NO_ERROR;
}

void RecoveryManager::updatedFailedServers(std::vector<ServerID> const& failed) {
  MUTEX_LOCKER(guard, _lock);  // we are accessing _primaryServers

  for (auto const& pair : _primaryServers) {
    auto const& it = std::find(failed.begin(), failed.end(), pair.second);
    if (it != failed.end()) {
      // found a failed server
      ShardID const& shard = pair.first;

      TRI_ASSERT(SchedulerFeature::SCHEDULER != nullptr);
      Scheduler* scheduler = SchedulerFeature::SCHEDULER;
      bool queued = scheduler->queue(RequestLane::INTERNAL_LOW, [this, shard] {
        _renewPrimaryServer(shard);
      });
      if (!queued) {
        LOG_TOPIC("038de", ERR, Logger::PREGEL)
            << "No thread available to queue pregel recovery manager request";
      }
    }
  }
}

// should try to figure out if the primary server for a shards has changed
// it doesn't really matter if this is called multiple times, this should not
// affect the outcome.
// don't call while holding _lock
void RecoveryManager::_renewPrimaryServer(ShardID const& shard) {
  MUTEX_LOCKER(guard, _lock);  // editing

  auto const& conductors = _listeners.find(shard);
  auto const& currentPrimary = _primaryServers.find(shard);
  if (conductors == _listeners.end() || currentPrimary == _primaryServers.end()) {
    LOG_TOPIC("30077", ERR, Logger::PREGEL) << "Shard is not properly registered";
    return;
  }

  int tries = 0;
  do {
    std::shared_ptr<std::vector<ServerID>> servers = _ci.getResponsibleServer(shard);
    if (servers && !servers->empty()) {
      ServerID const& nextPrimary = servers->front();
      if (currentPrimary->second != nextPrimary) {
        _primaryServers[shard] = nextPrimary;
        for (Conductor* cc : conductors->second) {
          cc->startRecovery();
        }
        LOG_TOPIC("e9429", INFO, Logger::PREGEL) << "Recovery action was initiated";
        break;
      }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    tries++;
  } while (tries < 3);
}
