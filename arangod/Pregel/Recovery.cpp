////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
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
#include "Basics/MutexLocker.h"
#include "Basics/ThreadPool.h"
#include "Cluster/ClusterInfo.h"
#include "Pregel/Conductor.h"
#include "Pregel/PregelFeature.h"
#include "Pregel/Utils.h"
#include "Pregel/WorkerConfig.h"
#include "VocBase/LogicalCollection.h"

using namespace arangodb;
using namespace arangodb::pregel;

RecoveryManager::RecoveryManager(AgencyCallbackRegistry* registry)
    : _agencyCallbackRegistry(registry) {}

RecoveryManager::~RecoveryManager() {
  for (auto const& call : _agencyCallbacks) {
    _agencyCallbackRegistry->unregisterCallback(call.second);
  }
  _listeners.clear();
  _agencyCallbacks.clear();
}

void RecoveryManager::stopMonitoring(Conductor* listener) {
  MUTEX_LOCKER(guard, _lock);

  for (auto& pair : _listeners) {
    if (pair.second.find(listener) != pair.second.end()) {
      pair.second.erase(listener);
    }
    if (pair.second.size() == 0) {
      std::shared_ptr<AgencyCallback> callback = _agencyCallbacks[pair.first];
      _agencyCallbackRegistry->unregisterCallback(callback);
      _agencyCallbacks.erase(pair.first);
    }
  }
}

void RecoveryManager::monitorCollections(
    std::vector<std::shared_ptr<LogicalCollection>> const& collections,
    Conductor* listener) {
  MUTEX_LOCKER(guard, _lock);

  for (auto const& coll : collections) {
    CollectionID cid = coll->cid_as_string();
    std::shared_ptr<std::vector<ShardID>> shards =
        ClusterInfo::instance()->getShardList(cid);
    if (!shards) {
      continue;
    }

    for (ShardID const& shard : *(shards.get())) {
      std::set<Conductor*>& conductors = _listeners[shard];
      if (conductors.find(listener) != conductors.end()) {
        continue;
      }
      conductors.insert(listener);
      _monitorShard(coll->dbName(), cid, shard);
    }
  }
}

/// Only call while holding _lock
void RecoveryManager::_monitorShard(DatabaseID const& databaseName,
                                    CollectionID const& cid,
                                    ShardID const& shard) {
  std::function<bool(VPackSlice const& result)> listener =
      [this, shard](VPackSlice const& result) {
        MUTEX_LOCKER(guard, _lock);  // we are editing _primaryServers

        auto const& conductors = _listeners.find(shard);
        if (conductors == _listeners.end()) {
          return false;
        }

        if (result.isArray()) {
          if (result.length() > 0) {
            ServerID nextPrimary = result.at(0).copyString();
            auto const& currentPrimary = _primaryServers.find(shard);
            if (currentPrimary != _primaryServers.end() &&
                currentPrimary->second != nextPrimary) {
              _primaryServers[shard] = nextPrimary;
              for (Conductor* cc : conductors->second) {
                cc->startRecovery();
              }
            }
          } else {  // what a terrible failure
            for (Conductor* cc : conductors->second) {
              cc->cancel();
            }
          }
        }

        LOG(INFO) << result.toString();
        return true;
      };

  std::string path = "Plan/Collections/" + databaseName + "/" + cid
                     + "/shards/" + shard;

  // first let's resolve the primary so we know if it has changed later
  // AgencyCommResult result = _agency.getValues(path);
  std::shared_ptr<std::vector<ServerID>> servers =
      ClusterInfo::instance()->getResponsibleServer(shard);
  if (servers->size() > 0) {
    MUTEX_LOCKER(guard, _lock);
    _primaryServers[shard] = servers->at(0);

    auto call =
        std::make_shared<AgencyCallback>(_agency, path, listener, true, false);
    _agencyCallbacks.emplace(shard, call);
    _agencyCallbackRegistry->registerCallback(call);
  }
}

int RecoveryManager::filterGoodServers(std::vector<ServerID> const& servers,
                                       std::vector<ServerID>& goodServers) {
  AgencyCommResult result = _agency.getValues("Supervision/Health");
  if (result.successful()) {
    VPackSlice serversRegistered =
        result.slice()[0].get(std::vector<std::string>(
            {AgencyCommManager::path(), "Supervision", "Health"}));

    if (serversRegistered.isObject()) {
      for (auto const& res : VPackObjectIterator(serversRegistered)) {
        VPackSlice serverId = res.key;
        VPackSlice slice = res.value;
        if (slice.isObject() && slice.hasKey("Status")) {
          VPackSlice status = slice.get("Status");
          if (status.compareString(
                  consensus::Supervision::HEALTH_STATUS_GOOD)) {
            ServerID name = serverId.copyString();
            if (std::find(servers.begin(), servers.end(), name) !=
                servers.end()) {
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

void RecoveryManager::updatedFailedServers() {
  MUTEX_LOCKER(guard, _lock);  // we are accessing _primaryServers

  std::vector<std::string> const failed =
      ClusterInfo::instance()->getFailedServers();
  for (auto const& pair : _primaryServers) {
    auto const& it = std::find(failed.begin(), failed.end(), pair.second);
    if (it != failed.end()) {
      // found a failed server
      ShardID const& shard = pair.first;
      basics::ThreadPool* pool = PregelFeature::instance()->threadPool();
      pool->enqueue([this, shard] { _renewPrimaryServer(shard); });
    }
  }
}

// don't call while holding _lock
void RecoveryManager::_renewPrimaryServer(ShardID const& shard) {
  MUTEX_LOCKER(guard, _lock);

  ClusterInfo* ci = ClusterInfo::instance();
  auto const& conductors = _listeners.find(shard);
  auto const& currentPrimary = _primaryServers.find(shard);
  if (conductors == _listeners.end() ||
      currentPrimary == _primaryServers.end()) {
    LOG(ERR) << "Shard is not properly registered";
    return;
  }

  int tries = 0;
  do {
    std::shared_ptr<std::vector<ServerID>> servers =
        ci->getResponsibleServer(shard);
    if (servers) {
      ServerID const& nextPrimary = servers->front();
      if (currentPrimary->second != nextPrimary) {
        _primaryServers[shard] = nextPrimary;
        for (Conductor* cc : conductors->second) {
          cc->startRecovery();
        }
        LOG(INFO) << "Recovery action was initiated";
        break;
      }
    }
    usleep(250000);  // 250ms
    tries++;
  } while (tries < 2);
}
