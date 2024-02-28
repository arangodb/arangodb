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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "RestOptionsHandler.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "ProgramOptions/ProgramOptions.h"
#include "RestServer/arangod.h"

#include <velocypack/Builder.h>

using namespace arangodb;
using namespace arangodb::rest;

RestOptionsHandler::RestOptionsHandler(ArangodServer& server,
                                       GeneralRequest* request,
                                       GeneralResponse* response)
    : RestOptionsBaseHandler(server, request, response) {}

RestStatus RestOptionsHandler::execute() {
  if (_request->requestType() != rest::RequestType::GET) {
    // only HTTP GET allowed
    generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                  TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
    return RestStatus::DONE;
  }

  if (!checkAuthentication()) {
    // checkAuthentication() will have created the response message already
    return RestStatus::DONE;
  }

  VPackBuilder builder =
      server().options(options::ProgramOptions::defaultOptionsFilter);

  generateResult(rest::ResponseCode::OK, builder.slice());
  return RestStatus::DONE;
}
