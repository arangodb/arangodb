////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "RestEngineHandler.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "GeneralServer/ServerSecurityFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

RestEngineHandler::RestEngineHandler(application_features::ApplicationServer& server,
                                     GeneralRequest* request, GeneralResponse* response)
    : RestBaseHandler(server, request, response) {}

RestStatus RestEngineHandler::execute() {
  // extract the sub-request type
  auto const type = _request->requestType();

  if (type != rest::RequestType::GET) {
    generateError(rest::ResponseCode::METHOD_NOT_ALLOWED, TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
    return RestStatus::DONE;
  }

  handleGet();
  return RestStatus::DONE;
}

void RestEngineHandler::handleGet() {
  std::vector<std::string> const& suffixes = _request->suffixes();

  if (suffixes.size() > 1 || (suffixes.size() == 1 && suffixes[0] != "stats")) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_BAD_PARAMETER,
                  "expecting GET /_api/engine[/stats]");
    return;
  }

  if (suffixes.empty()) {
    getCapabilities();
    return;
  }

  ServerSecurityFeature& security = server().getFeature<ServerSecurityFeature>();

  if (!security.canAccessHardenedApi()) {
    // dont leak information about server internals here
    generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_FORBIDDEN);
    return;
  }

  // access to engine stats is disallowed in hardened mode
  getStats();
}

void RestEngineHandler::getCapabilities() {
  VPackBuilder result;
  StorageEngine& engine = server().getFeature<EngineSelectorFeature>().engine();
  engine.getCapabilities(result);

  generateResult(rest::ResponseCode::OK, result.slice());
}

void RestEngineHandler::getStats() {
  VPackBuilder result;
  StorageEngine& engine = server().getFeature<EngineSelectorFeature>().engine();
  engine.getStatistics(result, true);

  generateResult(rest::ResponseCode::OK, result.slice());
}
