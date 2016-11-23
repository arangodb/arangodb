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

void RecoveryManager::monitorCollections(std::vector<std::shared_ptr<LogicalCollection>> const& collections) {

  /*
  std::function<bool(VPackSlice const& result)> dbServerChanged =
  [=](VPackSlice const& result) {
    if (result.isObject() && result.length() == (size_t)numberOfShards) {
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
    }
    return true;
  };
  
  // ATTENTION: The following callback calls the above closure in a
  // different thread. Nevertheless, the closure accesses some of our
  // local variables. Therefore we have to protect all accesses to them
  // by a mutex. We use the mutex of the condition variable in the
  // AgencyCallback for this.
  auto agencyCallback = std::make_shared<AgencyCallback>(
                                                         ac, "Current/Collections/" + databaseName + "/" + collectionID,
                                                         dbServerChanged, true, false);
  _agencyCallbackRegistry->registerCallback(agencyCallback);
  TRI_DEFER(_agencyCallbackRegistry->unregisterCallback(agencyCallback));*/
  
  
}

bool RecoveryManager::allServersAvailable(std::vector<ServerID> const& dbServers) {
  
  
  return false;
}

