////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////

#include "RestShutdownHandler.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

#include "Agency/AgencyComm.h"
#include "Cluster/ClusterFeature.h"
#include "Scheduler/Scheduler.h"
#include "Scheduler/SchedulerFeature.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "Rest/HttpRequest.h"
#include "Utils/ExecContext.h"

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::rest;

RestShutdownHandler::RestShutdownHandler(GeneralRequest* request,
                                         GeneralResponse* response)
    : RestBaseHandler(request, response) {}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_get_api_initiate
////////////////////////////////////////////////////////////////////////////////

RestStatus RestShutdownHandler::execute() {
  if (_request->requestType() != rest::RequestType::DELETE_REQ) {
    generateError(rest::ResponseCode::METHOD_NOT_ALLOWED, 405);
    return RestStatus::DONE;
  }
  
  AuthenticationFeature* af = AuthenticationFeature::instance();
  if (af->isActive() && !_request->user().empty()) {
    auth::Level lvl = auth::Level::NONE;
    if (af->userManager() != nullptr) {
      lvl = af->userManager()->databaseAuthLevel(_request->user(), "_system", /*configured*/true);
    } else {
      lvl = auth::Level::RW;
    }
    if (lvl < auth::Level::RW) {
      generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_HTTP_FORBIDDEN,
                    "you need admin rights to trigger shutdown");
      return RestStatus::DONE;
    }
  }
  
  bool removeFromCluster;
  std::string const& remove =
      _request->value("remove_from_cluster", removeFromCluster);
  removeFromCluster = removeFromCluster && remove == "1";

  bool shutdownClusterFound;
  std::string const& shutdownCluster =
      _request->value("shutdown_cluster", shutdownClusterFound);
  if (shutdownClusterFound && shutdownCluster == "1" &&
    AgencyCommManager::isEnabled()) {
    AgencyComm agency;
    VPackBuilder builder;
    builder.add(VPackValue(true));
    AgencyCommResult result = agency.setValue("Shutdown", builder.slice(), 0.0);
    if (!result.successful()) {
      generateError(rest::ResponseCode::SERVER_ERROR, 500);
      return RestStatus::DONE;
    }
    removeFromCluster = true;
  }
  if (removeFromCluster) {
    ClusterFeature* clusterFeature =
        ApplicationServer::getFeature<ClusterFeature>("Cluster");
    clusterFeature->setUnregisterOnShutdown(true);
  }
 
  try {
    VPackBuilder result;
    result.add(VPackValue("OK"));
    generateResult(rest::ResponseCode::OK, result.slice());
  } catch (...) {
    // Ignore the error
  }
  rest::Scheduler* scheduler = SchedulerFeature::SCHEDULER;
  // don't block the response for workers waiting on this callback
  // this should allow workers to go into the IDLE state
  scheduler->queue(RequestPriority::HIGH, [this] {
    // Give the server 2 seconds to send the reply:
    std::this_thread::sleep_for(std::chrono::seconds(2));
    // Go down:
    ApplicationServer::server->beginShutdown();
    });
    
  return RestStatus::DONE;
}
