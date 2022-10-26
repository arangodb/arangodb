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
/// @author Aditya Mukhopadhyay, Heiko Kernbach
////////////////////////////////////////////////////////////////////////////////

#include "RestPlanHandler.h"

#include "Aql/Query.h"
#include "Aql/Ast.h"
#include "Aql/ExecutionEngine.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/ServerState.h"
#include "Logger/LogMacros.h"

#include "Aql/Optimizer2/Plan/PlanRPCHandler.h"

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::basics;

RestPlanHandler::RestPlanHandler(ArangodServer& server, GeneralRequest* request,
                                 GeneralResponse* response,
                                 aql::QueryRegistry* queryRegistry)
    : RestCursorHandler(server, request, response, queryRegistry) {}

RestStatus RestPlanHandler::execute() {
  // extract the sub-request type
  rest::RequestType const type = _request->requestType();

  if (type == rest::RequestType::POST) {
    std::vector<std::string> const& suffixes = _request->suffixes();

    if (!suffixes.empty()) {
      generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                    "expecting POST /_api/plan");
      return RestStatus::FAIL;
    }

    bool parseSuccess = false;
    VPackSlice body = this->parseVPackBody(parseSuccess);

    if (!parseSuccess || !body.isObject() || body.isEmptyObject()) {
      generateError(rest::ResponseCode::BAD, TRI_ERROR_MALFORMED_JSON);
      return RestStatus::FAIL;
    }

    /* RPC handler takes over now */
    using PlanHandler = aql::optimizer2::plan::PlanRPCHandler;
    PlanHandler handler;

    VPackSlice plan = body.get("plan");
    if (!plan.isObject() || plan.isEmptyObject()) {
      generateError(rest::ResponseCode::BAD, TRI_ERROR_QUERY_BAD_JSON_PLAN);
      return RestStatus::FAIL;
    }

    buildOptions(body);

    const AccessMode::Type mode = AccessMode::Type::WRITE;
    auto query = Query::createFromPlan(_vocbase, createTransactionContext(mode),
                                       plan, _options->slice());

    return registerQueryOrCursor(query);
  }

  generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
  return RestStatus::DONE;
}