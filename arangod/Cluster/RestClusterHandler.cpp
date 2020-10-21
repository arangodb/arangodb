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
/// @author Kaveh Vahedipour
////////////////////////////////////////////////////////////////////////////////

#include "RestClusterHandler.h"

#include "Agency/AgencyComm.h"
#include "Agency/AsyncAgencyComm.h"
#include "Agency/Supervision.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Cluster/AgencyCache.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ServerState.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "Replication/ReplicationFeature.h"
#include "Rest/Version.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

RestClusterHandler::RestClusterHandler(application_features::ApplicationServer& server,
                                       GeneralRequest* request, GeneralResponse* response)
    : RestBaseHandler(server, request, response) {}

RestStatus RestClusterHandler::execute() {
  if (_request->requestType() != RequestType::GET) {
    generateError(rest::ResponseCode::METHOD_NOT_ALLOWED, TRI_ERROR_HTTP_METHOD_NOT_ALLOWED,
                  "only the GET method is allowed");
    return RestStatus::DONE;
  }

  std::vector<std::string> const& suffixes = _request->suffixes();
  if (!suffixes.empty()) {
    if (suffixes[0] == "endpoints") {
      handleCommandEndpoints();
      return RestStatus::DONE;
    } else if (suffixes[0] == "agency-dump") {
      handleAgencyDump();
      return RestStatus::DONE;
    } else if (suffixes[0] == "agency-cache") {
      handleAgencyCache();
      return RestStatus::DONE;
    } else if (suffixes[0] == "cluster-info") {
      handleClusterInfo();
      return RestStatus::DONE;
    }
  }

  generateError(
    Result(TRI_ERROR_HTTP_NOT_FOUND, "expecting /_api/cluster/[endpoints,agency-dump,agency-cache]"));

  return RestStatus::DONE;
}

void RestClusterHandler::handleAgencyDump() {
  if (!ServerState::instance()->isCoordinator()) {
    generateError(rest::ResponseCode::NOT_IMPLEMENTED, TRI_ERROR_NOT_IMPLEMENTED,
                  "only to be executed on coordinators");
    return;
  }

  AuthenticationFeature* af = AuthenticationFeature::instance();
  if (af->isActive() && !_request->user().empty()) {
    auth::Level lvl;
    if (af->userManager() != nullptr) {
      lvl = af->userManager()->databaseAuthLevel(_request->user(), "_system", true);
    } else {
      lvl = auth::Level::RW;
    }
    if (lvl < auth::Level::RW) {
      generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_HTTP_FORBIDDEN,
                    "you need admin rights to produce an agency dump");
      return;
    }
  }

  std::shared_ptr<VPackBuilder> body = std::make_shared<VPackBuilder>();
  auto& ci = server().getFeature<ClusterFeature>().clusterInfo();
  Result res = ci.agencyDump(body);
  if (res.ok()) {
    generateResult(rest::ResponseCode::OK, body->slice());
  } else {
    generateError(rest::ResponseCode::SERVICE_UNAVAILABLE, res.errorNumber(), res.errorMessage());
  }
}

void RestClusterHandler::handleAgencyCache() {

  AuthenticationFeature* af = AuthenticationFeature::instance();
  if (af->isActive() && !_request->user().empty()) {
    auth::Level lvl;
    if (af->userManager() != nullptr) {
      lvl = af->userManager()->databaseAuthLevel(_request->user(), "_system", true);
    } else {
      lvl = auth::Level::RW;
    }
    if (lvl < auth::Level::RW) {
      generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_HTTP_FORBIDDEN,
                    "you need admin rights to produce an agency cache dump");
      return;
    }
  }

  auto& ac = server().getFeature<ClusterFeature>().agencyCache();
  auto acb = ac.dump();

  generateResult(rest::ResponseCode::OK, acb->slice());

}

void RestClusterHandler::handleClusterInfo() {

  AuthenticationFeature* af = AuthenticationFeature::instance();
  if (af->isActive() && !_request->user().empty()) {
    auth::Level lvl;
    if (af->userManager() != nullptr) {
      lvl = af->userManager()->databaseAuthLevel(_request->user(), "_system", true);
    } else {
      lvl = auth::Level::RW;
    }
    if (lvl < auth::Level::RW) {
      generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_HTTP_FORBIDDEN,
                    "you need admin rights to produce a cluster info dump");
      return;
    }
  }

  auto& ci = server().getFeature<ClusterFeature>().clusterInfo();
  auto dump = ci.toVelocyPack();

  LOG_DEVEL << dump.toJson();
  generateResult(rest::ResponseCode::OK, dump.slice());

}

/// @brief returns information about all coordinator endpoints
void RestClusterHandler::handleCommandEndpoints() {
  ClusterInfo& ci = server().getFeature<ClusterFeature>().clusterInfo();
  std::vector<ServerID> endpoints;

  if (ServerState::instance()->isCoordinator()) {
    endpoints = ci.getCurrentCoordinators();
  } else if (ServerState::instance()->isSingleServer()) {
    ReplicationFeature* replication = ReplicationFeature::INSTANCE;
    if (!replication->isActiveFailoverEnabled() || !AsyncAgencyCommManager::isEnabled()) {
      generateError(Result(TRI_ERROR_NOT_IMPLEMENTED,
                           "automatic failover is not enabled"));
      return;
    }

    TRI_ASSERT(AsyncAgencyCommManager::isEnabled());

    std::string const leaderPath = "Plan/AsyncReplication/Leader";
    std::string const healthPath = "Supervision/Health";

    auto& cache = server().getFeature<ClusterFeature>().agencyCache();
    auto [acb, idx] = cache.read(std::vector<std::string>{
        AgencyCommHelper::path(healthPath), AgencyCommHelper::path(leaderPath)});
    auto result = acb->slice();

    if (!result.isArray()) {
      generateError(ResponseCode::SERVER_ERROR, TRI_ERROR_HTTP_SERVER_ERROR,
                    "Failed to acquire endpoints from agency cache");
      return;
    }

    std::vector<std::string> path = AgencyCommHelper::slicePath(leaderPath);
    VPackSlice slice = result[0].get(path);
    ServerID leaderId = slice.isString() ? slice.copyString() : "";
    path = AgencyCommHelper::slicePath(healthPath);
    VPackSlice healthMap = result[0].get(path);

    if (leaderId.empty()) {
      generateError(Result(TRI_ERROR_CLUSTER_LEADERSHIP_CHALLENGE_ONGOING,
                           "Leadership challenge is ongoing"));
      // intentionally use an empty endpoint here. clients can check for the
      // returned endpoint value, and can tell the following two cases apart:
      // - endpoint value is not empty: there is a leader, and it is known
      // - endpoint value is empty: leadership challenge is ongoing, current
      // leader is unknown
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
    generateError(Result(TRI_ERROR_NOT_IMPLEMENTED,
                         "cannot serve this request for this deployment type"));
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
      std::string advertised = ci.getServerAdvertisedEndpoint(sid);
      std::string internal = ci.getServerEndpoint(sid);
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
