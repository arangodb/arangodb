////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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
/// @author Kaveh Vahedipour
////////////////////////////////////////////////////////////////////////////////

#include "RestMetricsHandler.h"

#include "Agency/AgencyComm.h"
#include "Agency/AgencyFeature.h"
#include "Agency/Agent.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Cluster/ServerState.h"
#include "GeneralServer/ServerSecurityFeature.h"
#include "Rest/Version.h"
#include "RestServer/ServerFeature.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

////////////////////////////////////////////////////////////////////////////////
/// @brief ArangoDB server
////////////////////////////////////////////////////////////////////////////////

RestMetricsHandler::RestMetricsHandler(
  application_features::ApplicationServer& server,
  GeneralRequest* request, GeneralResponse* response)
  : RestBaseHandler(server, request, response) {}

RestStatus RestMetricsHandler::execute() {
  ServerSecurityFeature& security = server().getFeature<ServerSecurityFeature>();

  if (!security.canAccessHardenedApi()) {
    // dont leak information about server internals here
    generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_FORBIDDEN); 
    return RestStatus::DONE;
  }

  if (_request->requestType() != RequestType::GET) {
    generateError(ResponseCode::BAD, TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
    return RestStatus::DONE;
  }

  MetricsFeature& metrics = server().getFeature<MetricsFeature>();
  if (!metrics.exportAPI()) {
    // dont export metrics, if so desired
    generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_HTTP_NOT_FOUND); 
    return RestStatus::DONE;
  }

  std::string result;
  metrics.toPrometheus(result);
  _response->setResponseCode(rest::ResponseCode::OK);
  _response->setContentType(rest::ContentType::TEXT);
  _response->addRawPayload(VPackStringRef(result));
  
  return RestStatus::DONE;
}
