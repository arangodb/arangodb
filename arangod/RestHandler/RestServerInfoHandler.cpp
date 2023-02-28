////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Julia Puget
////////////////////////////////////////////////////////////////////////////////

#include "RestServerInfoHandler.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "GeneralServer/GeneralServerFeature.h"
#include "RestServer/arangod.h"
#include "Utils/ExecContext.h"
#include "Utils/SupportInfoBuilder.h"

#include "Logger/LogMacros.h"
#include <velocypack/Builder.h>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

RestServerInfoHandler::RestServerInfoHandler(ArangodServer& server,
                                             GeneralRequest* request,
                                             GeneralResponse* response)
    : RestBaseHandler(server, request, response) {}

RestStatus RestServerInfoHandler::execute() {
  GeneralServerFeature& gs = server().getFeature<GeneralServerFeature>();
  if (!gs.isTelemetricsEnabled()) {
    generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_HTTP_FORBIDDEN,
                  "telemetrics is disabled. Must enable with startup parameter "
                  "`--server.send-telemetrics`.");
    return RestStatus::DONE;
  }

  auto const& apiPolicy = gs.supportInfoApiPolicy();
  TRI_ASSERT(apiPolicy != "disabled");

  if (apiPolicy == "jwt") {
    if (!ExecContext::current().isSuperuser()) {
      generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_HTTP_FORBIDDEN,
                    "insufficient permissions");
      return RestStatus::DONE;
    }
  }

  if (apiPolicy == "admin" && !ExecContext::current().isAdminUser()) {
    generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_HTTP_FORBIDDEN,
                  "insufficient permissions");
    return RestStatus::DONE;
  }

  VPackBuilder result;
  bool isLocal = _request->parsedValue("local", false);

  SupportInfoBuilder::buildInfoMessage(result, _request->databaseName(),
                                       _server, isLocal);

  generateResult(rest::ResponseCode::OK, result.slice());
  return RestStatus::DONE;
}
