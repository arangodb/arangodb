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

#include "Basics/Exceptions.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "Pregel/Utils.h"
#include "Pregel/Conductor.h"
#include "Pregel/WorkerState.h"
#include "ApplicationFeatures/ApplicationServer.h"

using namespace arangodb;
using namespace arangodb::pregel;

RecoveryManager::RecoveryManager(Conductor *c) : _conductor(c) {
  ClusterFeature* cluster =
  application_features::ApplicationServer::getFeature<ClusterFeature>(
                                                                      "Cluster");
  TRI_ASSERT(cluster != nullptr);
  _agencyCallbackRegistry = cluster->agencyCallbackRegistry();
}

RecoveryManager::~RecoveryManager() {
  for (auto call : _agencyCallbacks) {
    _agencyCallbackRegistry->unregisterCallback(call);
  }
}

void RecoveryManager::monitorDBServers(std::vector<ServerID> const& dbServers) {
  
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
    LOG(INFO) << result.toString();
    return true;
  };
  //std::string const& dbName = _conductor->_vocbaseGuard.vocbase()->name();
  
  for (auto server : dbServers) {
    _statusMap[server] = "";
    std::string path = "Supervision/Health/" + server + "/Status";
    auto call = std::make_shared<AgencyCallback>(_agency, path,
                                                 dbServerChanged, true, false);
    _agencyCallbacks.push_back(call);
    _agencyCallbackRegistry->registerCallback(call);
  }
  
}

bool RecoveryManager::allServersAvailable(std::vector<ServerID> const& dbServers) {
  
  
  return false;
}

