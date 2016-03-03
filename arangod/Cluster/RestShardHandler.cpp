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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "RestShardHandler.h"
#include "Cluster/ServerState.h"
#include "Cluster/ClusterComm.h"
#include "Dispatcher/Dispatcher.h"
#include "HttpServer/GeneralServer.h"
#include "HttpServer/GeneralHandlerFactory.h"
#include "Rest/GeneralRequest.h"
#include "Rest/GeneralResponse.h"

using namespace arangodb;
using namespace arangodb::rest;

RestShardHandler::RestShardHandler(arangodb::rest::GeneralRequest* request,
                                   Dispatcher* data)
    : RestBaseHandler(request), _dispatcher(data) {
  TRI_ASSERT(_dispatcher != nullptr);
}

bool RestShardHandler::isDirect() const { return true; }

arangodb::rest::GeneralHandler::status_t RestShardHandler::execute() {
  bool found;
  char const* _coordinator = _request->header("x-arango-coordinator", found);

  if (!found) {
    generateError(arangodb::rest::GeneralResponse::BAD,
                  (int)arangodb::rest::GeneralResponse::BAD,
                  "header 'X-Arango-Coordinator' is missing");
    return status_t(HANDLER_DONE);
  }

  std::string coordinatorHeader = _coordinator;
  std::string result =
      ClusterComm::instance()->processAnswer(coordinatorHeader, stealRequest());

  if (result == "") {
    createResponse(arangodb::rest::GeneralResponse::ACCEPTED);
  } else {
    generateError(arangodb::rest::GeneralResponse::BAD,
                  (int)arangodb::rest::GeneralResponse::BAD, result.c_str());
  }

  return status_t(HANDLER_DONE);
}
