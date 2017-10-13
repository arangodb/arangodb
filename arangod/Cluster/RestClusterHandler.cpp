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
#include "Cluster/ClusterInfo.h"
#include "Cluster/ServerState.h"
#include "Replication/ReplicationFeature.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

#include "Rest/HttpRequest.h"
#include "Rest/Version.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

RestClusterHandler::RestClusterHandler(GeneralRequest* request,
                                 GeneralResponse* response)
    : RestBaseHandler(request, response) {}

RestStatus RestClusterHandler::execute() {

  if (_request->requestType() != RequestType::GET) {
    generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_FORBIDDEN,
                  "only the GET method is allowed");
    return RestStatus::DONE;
  }
  
  std::vector<std::string> const& suffixes = _request->suffixes();
  if (!suffixes.empty() && suffixes[0] == "endpoints") {
    handleCommandEndpoints();
  } else if (!suffixes.empty() && suffixes[0] == "serverInfo") {
    handleCommandServerInfo();
  } else {
    generateError(Result(TRI_ERROR_FORBIDDEN,
                         "expecting _api/cluster/endpoints"));
  }

  return RestStatus::DONE;
}

// //////////////////////////////////////////////////////////////////////////////
// / @start Docu Block JSF_getClusterEndpoints
// / @brief returns information about all coordinator endpoints
// /
// / @ RESTHEADER{GET /_api/cluster/endpoints, Get information
// / about all coordinator endpoints
// /
// / @ RESTDESCRIPTION Returns an array of objects, which each have
// / the attribute `endpoint`, whose value is a string with the endpoint
// / description. There is an entry for each coordinator in the cluster.
// /
// / @ RESTRETURNCODES
// /
// / @ RESTRETURNCODE{200} is returned when everything went well.
// /
// / @ RESTRETURNCODE{403} server is not a coordinator or method was not GET.
// /
// / @end Docu Block
// //////////////////////////////////////////////////////////////////////////////
void RestClusterHandler::handleCommandEndpoints() {
  TRI_ASSERT(AgencyCommManager::isEnabled());
  if (!ServerState::instance()->isCoordinator()) {
    generateError(Result(TRI_ERROR_FORBIDDEN,
                         "only coordinators can serve this request"));
    return;
  }
  
  std::vector<std::string> endpoints = AgencyCommManager::MANAGER->endpoints();
  // make the list of endpoints unique
  std::sort(endpoints.begin(), endpoints.end());
  endpoints.assign(endpoints.begin(),
                   std::unique(endpoints.begin(), endpoints.end()));
  
  VPackBuilder builder;
  builder.openArray();
  for (size_t i = 0; i < endpoints.size(); ++i) {
    builder.add(VPackValue(endpoints[i]));
  }
  builder.close();
  generateResult(rest::ResponseCode::OK, builder.slice());
}

void RestClusterHandler::handleCommandServerInfo() {
  LOG_TOPIC(INFO, arangodb::Logger::FIXME) << "demo handler working very hard";
  if (!ServerState::instance()->isSingleServer()) {
    generateError(Result(TRI_ERROR_FORBIDDEN,
                         "only single server can serve this request"));
    return;
  }
  
  auto feature = application_features::ApplicationServer::getFeature<ReplicationFeature>("Replication");
  if (!feature->isAutomaticFailoverEnabled() ||
      !AgencyCommManager::isEnabled()) {
    generateError(Result(TRI_ERROR_FORBIDDEN,
                         "automatic failover is not enabled"));
    return;
  }
  
  ClusterInfo* ci = ClusterInfo::instance();
  TRI_ASSERT(ci != nullptr);
  
  ServerID leaderId;
  AgencyComm agency;
  std::string const leaderPath = "Plan/AsyncReplication/Leader";
  AgencyCommResult result = agency.getValues(leaderPath);
  if (result.successful()) {
    std::vector<std::string> components = AgencyCommManager::slicePath(leaderPath);
    VPackSlice slice = result.slice()[0].get(components);
    leaderId = slice.copyString();
  }
  
  std::unordered_map<ServerID, std::string> serverMap = ci->getServers();
  VPackBuilder builder;
  auto const& it = serverMap.find(leaderId);
  if (it != serverMap.end()) {
    VPackObjectBuilder obj(&builder, true);
    obj->add("leader", VPackValue(it->second));
    
    VPackArrayBuilder followers(&builder, "followers", true);
    for (auto const& pair : serverMap) {
      if (pair.first != leaderId) {
        followers->add(VPackValue(pair.second));
      }
    }
  } else {
    LOG_TOPIC(INFO, Logger::CLUSTER) << "Could not resolve server id";
  }
  generateResult(ResponseCode::OK, builder.slice());
}
