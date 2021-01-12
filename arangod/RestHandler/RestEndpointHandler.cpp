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
/// @author Daniel H. Larkin
////////////////////////////////////////////////////////////////////////////////

#include "RestEndpointHandler.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "RestServer/EndpointFeature.h"
#include "VocBase/vocbase.h"

#include "Logger/Logger.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

RestEndpointHandler::RestEndpointHandler(application_features::ApplicationServer& server,
                                         GeneralRequest* request, GeneralResponse* response)
    : RestVocbaseBaseHandler(server, request, response) {}

RestStatus RestEndpointHandler::execute() {
  // extract the sub-request type
  auto const type = _request->requestType();

  // execute one of the CRUD methods
  switch (type) {
    case rest::RequestType::GET:
      retrieveEndpoints();
      break;
    default: {
      generateError(rest::ResponseCode::METHOD_NOT_ALLOWED, TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
    }
  }

  // this handler is done
  return RestStatus::DONE;
}

void RestEndpointHandler::retrieveEndpoints() {
  auto& server = _vocbase.server().getFeature<HttpEndpointProvider>();

  if (!_vocbase.isSystem()) {
    generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_ARANGO_USE_SYSTEM_DATABASE);
    return;
  }

  VPackBuilder result;
  result.openArray();

  for (auto const& it : server.httpEndpoints()) {
    result.openObject();
    result.add("endpoint", VPackValue(it));
    result.close();
  }

  result.close();

  generateResult(rest::ResponseCode::OK, result.slice());
}
