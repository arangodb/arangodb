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

#include "RestRobotsHandler.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/FileUtils.h"
#include "GeneralServer/GeneralServerFeature.h"
#include "RestServer/arangod.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

RestRobotsHandler::RestRobotsHandler(ArangodServer& server,
                                     GeneralRequest* request,
                                     GeneralResponse* response)
    : RestBaseHandler(server, request, response) {}

RestStatus RestRobotsHandler::execute() {
  if (_request->requestType() == rest::RequestType::GET) {
    GeneralServerFeature& gs = server().getFeature<GeneralServerFeature>();
    std::string const& filename = gs.robotsFile();

    TRI_ASSERT(!filename.empty());
    std::string result;
    try {
      FileUtils::slurp(filename, result);
      _response->setResponseCode(rest::ResponseCode::OK);
      _response->setContentType(rest::ContentType::TEXT);
      _response->addRawPayload(result);
    } catch (...) {
      generateError(ResponseCode::SERVER_ERROR, TRI_ERROR_CANNOT_READ_FILE);
    }
  } else {
    generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                  TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
  }
  return RestStatus::DONE;
}
