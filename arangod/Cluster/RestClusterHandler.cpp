////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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

#include "RestClusterHandler.h"
#include "Agency/AgencyComm.h"
#include "Agency/Supervision.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ServerState.h"
#include "Replication/ReplicationFeature.h"
#include "Rest/HttpRequest.h"
#include "Rest/Version.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

RestClusterHandler::RestClusterHandler(GeneralRequest* request,
                                       GeneralResponse* response)
    : RestBaseHandler(request, response) {}

RestStatus RestClusterHandler::execute() {
  if (_request->requestType() != RequestType::GET) {
    generateError(rest::ResponseCode::METHOD_NOT_ALLOWED, TRI_ERROR_HTTP_METHOD_NOT_ALLOWED,
                  "only the GET method is allowed");
    return RestStatus::DONE;
  }
  
  std::vector<std::string> const& suffixes = _request->suffixes();
  if (!suffixes.empty() && suffixes[0] == "endpoints") {
    handleCommandEndpoints();
  } else {
    generateError(Result(TRI_ERROR_FORBIDDEN,
                         "expecting _api/cluster/endpoints"));
  }

  return RestStatus::DONE;
}

/// @brief returns information about all coordinator endpoints
void RestClusterHandler::handleCommandEndpoints() {
  ClusterInfo* ci = ClusterInfo::instance();
  TRI_ASSERT(ci != nullptr);
  std::vector<ServerID> endpoints;
  
  if (ServerState::instance()->isCoordinator()) {
    endpoints = ci->getCurrentCoordinators();
  } else if (ServerState::instance()->isSingleServer()) {
    
    ReplicationFeature* replication = ReplicationFeature::INSTANCE;
    if (!replication->isActiveFailoverEnabled() ||
        !AgencyCommManager::isEnabled()) {
      generateError(Result(TRI_ERROR_FORBIDDEN,
                           "automatic failover is not enabled"));
      return;
    }
  
    TRI_ASSERT(AgencyCommManager::isEnabled());
    
    std::string const leaderPath = "Plan/AsyncReplication/Leader";
    std::string const healthPath = "Supervision/Health";
    AgencyComm agency;
    
    AgencyReadTransaction trx(std::vector<std::string>({
      AgencyCommManager::path(healthPath),
      AgencyCommManager::path(leaderPath)}));
    AgencyCommResult result = agency.sendTransactionWithFailover(trx, 5.0);
    
    if (!result.successful()) {
      generateError(ResponseCode::SERVER_ERROR, result.errorCode(),
                    result.errorMessage());
      return;
    }
    
    std::vector<std::string> path = AgencyCommManager::slicePath(leaderPath);
    VPackSlice slice = result.slice()[0].get(path);
    ServerID leaderId = slice.isString() ? slice.copyString() : "";
    path = AgencyCommManager::slicePath(healthPath);
    VPackSlice healthMap = result.slice()[0].get(path);
    
    if (leaderId.empty()) {
      generateError(Result(TRI_ERROR_CLUSTER_LEADERSHIP_CHALLENGE_ONGOING, "Leadership challenge is ongoing"));
      // intentionally use an empty endpoint here. clients can check for the returned
      // endpoint value, and can tell the following two cases apart:
      // - endpoint value is not empty: there is a leader, and it is known
      // - endpoint value is empty: leadership challenge is ongoing, current leader is unknown
      _response->setHeaderNC(StaticStrings::LeaderEndpoint, "");
      return;
    }
      
    // {"serverId" : {"Status" : "GOOD", ...}}
    for (VPackObjectIterator::ObjectPair const& pair : VPackObjectIterator(healthMap)) {
      TRI_ASSERT(pair.key.isString() && pair.value.isObject());
      if (pair.key.compareString(leaderId) != 0) {
        VPackSlice status = pair.value.get("Status");
        TRI_ASSERT(status.isString());
        
        if (status.compareString(consensus::Supervision::HEALTH_STATUS_GOOD) == 0) {
          endpoints.insert(endpoints.begin(), pair.key.copyString());
        } else if (status.compareString(consensus::Supervision::HEALTH_STATUS_BAD) == 0) {
          endpoints.push_back(pair.key.copyString());
        }
      }
    }
    
    // master always in front
    endpoints.insert(endpoints.begin(), leaderId);

  } else {
    generateError(Result(TRI_ERROR_FORBIDDEN, "cannot serve this request"));
    return;
  }
  
  VPackBuilder builder;
  builder.openObject();
  builder.add(StaticStrings::Error, VPackValue(false));
  builder.add(StaticStrings::Code, VPackValue(200));
  {
    VPackArrayBuilder array(&builder, "endpoints", true);

    for (ServerID const& sid : endpoints) {
      VPackObjectBuilder obj(&builder);
      std::string advertised = ci->getServerAdvertisedEndpoint(sid);
      std::string internal = ci->getServerEndpoint(sid);
      if (!advertised.empty()) {
        builder.add("endpoint", VPackValue(advertised));
        builder.add("internal", VPackValue(internal));
      } else {
        builder.add("endpoint", VPackValue(internal));
      }
    }
  }
  builder.close();
  generateResult(rest::ResponseCode::OK, builder.slice());
}

