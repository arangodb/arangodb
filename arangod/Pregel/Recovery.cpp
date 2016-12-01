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
  _agencyCallbacks.clear();
}


void RecoveryManager::stopMonitoring(Conductor*) {
  MUTEX_LOCKER(guard, _lock);
  
  for (auto const& it : _listeners) {
    
  }
}
/*
std::map<ShardID, std::set<Conductor*>> _listeners;
std::map<ShardID, ServerID> _primaryServer;
std::map<ShardID, std::shared_ptr<AgencyCallback>> _agencyCallbacks;
*/
void RecoveryManager::monitorCollections(std::vector<std::shared_ptr<LogicalCollection>> const& collections,
                                         Conductor *listener) {
  MUTEX_LOCKER(guard, _lock);
  
  for (auto const& coll : collections) {
    CollectionID cid = coll->cid_as_string();
    std::shared_ptr<std::vector<ShardID>> shards = ClusterInfo::instance()->getShardList(cid);
    for (ShardID const& shard : *shards.get()) {
      std::set<Conductor*> &conductors = _listeners[shard];
      if (conductors.find(listener) != conductors.end()) {
        continue;
      }
      conductors.insert(listener);
      monitorShard(cid, shard);
    }
  }
}

void RecoveryManager::monitorShard(CollectionID const& cid, ShardID const& shard) {

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

/*
bool RecoveryManager::allServersAvailable(std::vector<ServerID> const& dbServers) {
  MUTEX_LOCKER(guard, _lock);
  
  if (TRI_microtime() - _lastHealthCheck >  5.0) {
    //Supervis
    AgencyCommResult result = _agency.getValues("Supervision/Health/");
    if (result.successful() && result.slice().isObject()) {
      for (auto server : VPackObjectIterator(result.slice())) {
        std::string status = server.value.get("Status").copyString();
        _statusMap[server.key.copyString()] = status;
      }
      _lastHealthCheck = TRI_microtime();// I don't like this
    }
  }
  
  std::string failed(consensus::Supervision::HEALTH_STATUS_FAILED);
  
  for (auto const& server : dbServers) {
    auto const& it = _statusMap.find(server);
    if (it != _statusMap.end()) {
      if (it->second == failed) {
        return false;
      }
    }
  }
  
  return true;
}*/

