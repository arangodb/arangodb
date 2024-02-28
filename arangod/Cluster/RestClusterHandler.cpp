////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

RestClusterHandler::RestClusterHandler(ArangodServer& server,
                                       GeneralRequest* request,
                                       GeneralResponse* response)
    : RestBaseHandler(server, request, response) {}

RestStatus RestClusterHandler::execute() {
  if (_request->requestType() != RequestType::GET) {
    generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                  TRI_ERROR_HTTP_METHOD_NOT_ALLOWED,
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
      Result(TRI_ERROR_HTTP_NOT_FOUND,
             "expecting /_api/cluster/[endpoints,agency-dump,agency-cache]"));

  return RestStatus::DONE;
}

void RestClusterHandler::handleAgencyDump() {
  if (!ServerState::instance()->isCoordinator()) {
    generateError(rest::ResponseCode::NOT_IMPLEMENTED,
                  TRI_ERROR_NOT_IMPLEMENTED,
                  "only to be executed on coordinators");
    return;
  }

  AuthenticationFeature* af = AuthenticationFeature::instance();
  if (af->isActive() && !_request->user().empty()) {
    auth::Level lvl;
    if (af->userManager() != nullptr) {
      lvl = af->userManager()->databaseAuthLevel(_request->user(), "_system",
                                                 true);
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
    generateError(rest::ResponseCode::SERVICE_UNAVAILABLE, res.errorNumber(),
                  res.errorMessage());
  }
}

void RestClusterHandler::handleAgencyCache() {
  AuthenticationFeature* af = AuthenticationFeature::instance();
  if (af->isActive() && !_request->user().empty()) {
    auth::Level lvl;
    if (af->userManager() != nullptr) {
      lvl = af->userManager()->databaseAuthLevel(_request->user(), "_system",
                                                 true);
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
      lvl = af->userManager()->databaseAuthLevel(_request->user(), "_system",
                                                 true);
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

  generateResult(rest::ResponseCode::OK, dump.slice());
}

/// @brief returns information about all coordinator endpoints
void RestClusterHandler::handleCommandEndpoints() {
  ClusterInfo& ci = server().getFeature<ClusterFeature>().clusterInfo();
  std::vector<ServerID> endpoints;

  if (ServerState::instance()->isCoordinator()) {
    endpoints = ci.getCurrentCoordinators();
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
