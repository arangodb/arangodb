////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#include "RestPregelHandler.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Rest/HttpRequest.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

#include "Pregel/PregelFeature.h"
#include "Pregel/Utils.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;
using namespace arangodb::pregel;

RestPregelHandler::RestPregelHandler(GeneralRequest* request, GeneralResponse* response)
: RestVocbaseBaseHandler(request, response) {}

RestStatus RestPregelHandler::execute() {
  try {    
    bool parseSuccess = true;
    std::shared_ptr<VPackBuilder> parsedBody =
    parseVelocyPackBody(parseSuccess);
    VPackSlice body(parsedBody->start());// never nullptr
    
    if (!parseSuccess || !body.isObject()) {
      LOG_TOPIC(ERR, Logger::PREGEL) << "Bad request body\n";
      generateError(rest::ResponseCode::BAD,
                    TRI_ERROR_NOT_IMPLEMENTED, "illegal request for /_api/pregel");
      return RestStatus::DONE;
    }
    if (_request->requestType() != rest::RequestType::POST) {
      generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                    TRI_ERROR_NOT_IMPLEMENTED, "illegal method for /_api/pregel");
      return RestStatus::DONE;

    }
    std::vector<std::string> const& suffix = _request->suffixes();
    if (suffix.size() != 2) {
      generateError(rest::ResponseCode::BAD,
                    TRI_ERROR_NOT_IMPLEMENTED, "you are missing a prefix");
    } else if (suffix[0] == Utils::conductorPrefix) {
      VPackBuilder response;
      PregelFeature::handleConductorRequest(suffix[1], body, response);
      if (response.isEmpty()) {
        resetResponse(rest::ResponseCode::OK);
      } else {
        generateResult(rest::ResponseCode::OK, response.slice());
      }
    } else if (suffix[0] == Utils::workerPrefix) {
      VPackBuilder response;
      PregelFeature::handleWorkerRequest(_vocbase, suffix[1], body, response);
      if (response.isEmpty()) {
        resetResponse(rest::ResponseCode::OK);
      } else {
        generateResult(rest::ResponseCode::OK, response.slice());
      }
    } else {
      generateError(rest::ResponseCode::BAD,
                    TRI_ERROR_NOT_IMPLEMENTED, "the prefix is incorrect");
    }
  } catch (std::exception const &e) {
    LOG_TOPIC(ERR, Logger::PREGEL) << e.what();
  } catch(...) {
    LOG_TOPIC(ERR, Logger::PREGEL) << "Exception in pregel REST handler";
  }
    
  return RestStatus::DONE;
}
