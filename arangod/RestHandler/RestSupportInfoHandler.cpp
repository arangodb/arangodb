////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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

#include "RestSupportInfoHandler.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/StaticStrings.h"
#include "GeneralServer/GeneralServerFeature.h"
#include "RestServer/arangod.h"
#include "Utils/ExecContext.h"
#include "Utils/SupportInfoBuilder.h"

#include <velocypack/Builder.h>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

RestSupportInfoHandler::RestSupportInfoHandler(ArangodServer& server,
                                               GeneralRequest* request,
                                               GeneralResponse* response)
    : RestBaseHandler(server, request, response) {}

RestStatus RestSupportInfoHandler::execute() {
  GeneralServerFeature& gs = server().getFeature<GeneralServerFeature>();
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

  if (_request->databaseName() != StaticStrings::SystemDatabase) {
    generateError(
        GeneralResponse::responseCode(TRI_ERROR_ARANGO_USE_SYSTEM_DATABASE),
        TRI_ERROR_ARANGO_USE_SYSTEM_DATABASE);
    return RestStatus::DONE;
  }

  VPackBuilder result;
  SupportInfoBuilder::buildInfoMessage(result, _request->databaseName(),
                                       _server,
                                       _request->parsedValue("local", false));

  generateResult(rest::ResponseCode::OK, result.slice());
  return RestStatus::DONE;
}
