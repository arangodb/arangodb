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
/// @author Daniel H. Larkin
////////////////////////////////////////////////////////////////////////////////

#include "RestAdminRoutingHandler.h"

#include "V8Server/V8Context.h"
#include "V8Server/V8DealerFeature.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

RestAdminRoutingHandler::RestAdminRoutingHandler(application_features::ApplicationServer& server,
                                                 GeneralRequest* request,
                                                 GeneralResponse* response)
    : RestVocbaseBaseHandler(server, request, response) {}

RestStatus RestAdminRoutingHandler::execute() {
  if (!V8DealerFeature::DEALER || !V8DealerFeature::DEALER->isEnabled()) {
    generateError(rest::ResponseCode::NOT_IMPLEMENTED, TRI_ERROR_NOT_IMPLEMENTED, "JavaScript operations are disabled");
    return RestStatus::DONE;
  }

  std::vector<std::string> const& suffixes = _request->suffixes();
  if (suffixes.size() == 1 && suffixes[0] == "reload") {
    reloadRouting();
    return RestStatus::DONE;
  }

  generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_HTTP_NOT_FOUND);

  // this handler is done
  return RestStatus::DONE;
}

void RestAdminRoutingHandler::reloadRouting() {
  if (!V8DealerFeature::DEALER->addGlobalContextMethod("reloadRouting")) {
    generateError(rest::ResponseCode::SERVER_ERROR, TRI_ERROR_INTERNAL,
                  "invalid action definition");
    return;
  }

  resetResponse(rest::ResponseCode::NO_CONTENT);
}
