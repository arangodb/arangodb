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

using namespace arangodb;
using namespace arangodb::pregel;

RecoveryManager::RecoveryManager(AgencyCallbackRegistry *registry) : _agencyCallbackRegistry(registry) {
}

RecoveryManager::~RecoveryManager() {
  stopMonitoring();
}

void RecoveryManager::stopMonitoring() {
  for (auto call : _agencyCallbacks) {
    _agencyCallbackRegistry->unregisterCallback(call);
  }
  _agencyCallbacks.clear();
}

void RecoveryManager::monitorDBServers(std::vector<ServerID> const& dbServers) {
  MUTEX_LOCKER(guard, _lock);
  
  std::function<bool(VPackSlice const& result)> dbServerChanged =
  [](VPackSlice const& result) {
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
    
    PregelFeature::instance()->notifyConductors();
    
    LOG(INFO) << result.toString();
    return true;
  };
  //std::string const& dbName = _conductor->_vocbaseGuard.vocbase()->name();
  
  std::string path = "Plan/Collections/_system/6500032";
  auto call = std::make_shared<AgencyCallback>(_agency, path,
                                               dbServerChanged, true, false);
  _agencyCallbacks.push_back(call);
  _agencyCallbackRegistry->registerCallback(call);
  
  /*for (auto server : dbServers) {
    auto const& it = _statusMap.find(server);
    if (it == _statusMap.end()) {
      std::string path = "Supervision/Health/" + server + "/Status/";
      auto call = std::make_shared<AgencyCallback>(_agency, path,
                                                   dbServerChanged, true, false);
      _agencyCallbacks.push_back(call);
      _agencyCallbackRegistry->registerCallback(call);
    }
  }*/
  
}

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
}

