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
/// @author Julia Volmer
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

#include "RestActorHandler.h"
#include "Pregel/PregelFeature.h"
#include "Pregel/Actor/Runtime.h"

#include "fmt/core.h"


namespace arangodb {
RestActorHandler::RestActorHandler(ArangodServer& server,
                                   GeneralRequest* request,
                                   GeneralResponse* response)
    : RestVocbaseBaseHandler(server, request, response),
      _pregel(server.getFeature<pregel::PregelFeature>()) {}

auto RestActorHandler::execute() -> RestStatus {
  auto const type = _request->requestType();

  switch (type) {
    case rest::RequestType::POST: {
    } break;
    case rest::RequestType::GET: {
      handleGetRequest();
    } break;
    default: {
      generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                    TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
    } break;
  }

  return RestStatus::DONE;
}

auto RestActorHandler::handleGetRequest() -> void {
//  std::vector<std::string> const& suffixes = _request->decodedSuffixes();

  velocypack::Builder responseBody;

  {
    VPackArrayBuilder array(&responseBody);

    for(auto id : _pregel.actorRuntime()->getActorIDs()) {
      auto r = _pregel.actorRuntime()->getSerializedActorByID(id);
      if(r.has_value()) {
        responseBody.add(r->slice());
      }
    }
  }
  responseBody.add(VPackValue(fmt::format("runtime: {}", *_pregel.actorRuntime())));

  generateResult(rest::ResponseCode::OK, responseBody.slice());
}

}  // namespace arangodb
