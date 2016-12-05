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

#include "Basics/MutexLocker.h"
#include "Cluster/ClusterInfo.h"
#include "Pregel/Utils.h"
#include "Pregel/Conductor.h"
#include "Pregel/WorkerState.h"
#include "Pregel/PregelFeature.h"
#include "Agency/Supervision.h"
#include "VocBase/LogicalCollection.h"

using namespace arangodb;
using namespace arangodb::pregel;

RecoveryManager::RecoveryManager(AgencyCallbackRegistry *registry) : _agencyCallbackRegistry(registry) {
}

RecoveryManager::~RecoveryManager() {
  for (auto const& call : _agencyCallbacks) {
    _agencyCallbackRegistry->unregisterCallback(call.second);
  }
  _listeners.clear();
  _agencyCallbacks.clear();
}

void RecoveryManager::stopMonitoring(Conductor *listener) {
  MUTEX_LOCKER(guard, _lock);
  
  for (auto &pair : _listeners) {
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

void RecoveryManager::monitorCollections(std::vector<std::shared_ptr<LogicalCollection>> const& collections,
                                         Conductor *listener) {
  MUTEX_LOCKER(guard, _lock);
  
  for (auto const& coll : collections) {
    CollectionID cid = coll->cid_as_string();
    std::shared_ptr<std::vector<ShardID>> shards = ClusterInfo::instance()->getShardList(cid);
    if (!shards) {
      continue;
    }
    
    for (ShardID const& shard : *(shards.get())) {
      std::set<Conductor*> &conductors = _listeners[shard];
      if (conductors.find(listener) != conductors.end()) {
        continue;
      }
      conductors.insert(listener);
      _monitorShard(cid, shard);
    }
  }
}

void RecoveryManager::_monitorShard(CollectionID const& cid, ShardID const& shard) {
  
  std::function<bool(VPackSlice const& result)> listener =
  [this](VPackSlice const& result) {
    /*if (result.isObject() && result.length() == (size_t)numberOfShards) {
     std::string tmpMsg = "";
     bool tmpHaveError = false;
     
     for (auto const& p : VPackObjectIterator(result)) {
     if (arangodb::basics::VelocyPackHelper::getBooleanValue(
     p.value, "error", false)) {
     tmpHaveError = true;
     tmpMsg += " shardID:" + p.key.copyString() + ":";
     tmpMsg += arangodb::basics::VelocyPackHelper::getStringValue(
     p.value, "errorMessage", "");
     if (p.value.hasKey("errorNum")) {
     VPackSlice const errorNum = p.value.get("errorNum");
     if (errorNum.isNumber()) {
     tmpMsg += " (errNum=";
     tmpMsg += basics::StringUtils::itoa(
     errorNum.getNumericValue<uint32_t>());
     tmpMsg += ")";
     }
     }
     }
     }
     if (tmpHaveError) {
     *errMsg = "Error in creation of collection:" + tmpMsg;
     *dbServerResult = TRI_ERROR_CLUSTER_COULD_NOT_CREATE_COLLECTION;
     return true;
     }
     *dbServerResult = setErrormsg(TRI_ERROR_NO_ERROR, *errMsg);
     }*/
    
    //PregelFeature::instance()->notifyConductors();
    
    LOG(INFO) << result.toString();
    return true;
  };
  
  std::string path = "Plan/Collections/" + cid + "/shards/" + shard;
  
  // first let's resolve the primary so we know if it has change later
  //AgencyCommResult result = _agency.getValues(path);
  std::shared_ptr<std::vector<ServerID>> servers
    = ClusterInfo::instance()->getResponsibleServer(shard);
  if (servers->size() > 0) {
    _primaryServer[shard] = servers->at(0);
    
    auto call = std::make_shared<AgencyCallback>(_agency, path, listener, true, false);
    _agencyCallbacks.emplace(shard, call);
    _agencyCallbackRegistry->registerCallback(call);
  }
}

int RecoveryManager::filterGoodServers(std::vector<ServerID> const& servers, std::set<ServerID> &goodServers) {
  
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
          if (status.compareString(consensus::Supervision::HEALTH_STATUS_GOOD)) {
            ServerID name = serverId.copyString();
            if (std::find(servers.begin(), servers.end(), name) != servers.end()) {
              goodServers.insert(name);
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
