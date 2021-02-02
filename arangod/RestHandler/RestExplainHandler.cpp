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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include "RestExplainHandler.h"

#include "Aql/Query.h"
#include "Aql/QueryString.h"
#include "Logger/Logger.h"
#include "Transaction/StandaloneContext.h"

#include <velocypack/Builder.h>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

RestExplainHandler::RestExplainHandler(application_features::ApplicationServer& server,
                                       GeneralRequest* request, GeneralResponse* response)
    : RestVocbaseBaseHandler(server, request, response) {}

RestStatus RestExplainHandler::execute() {
  // extract the sub-request type
  auto const type = _request->requestType();

  // execute one of the CRUD methods
  switch (type) {
    case rest::RequestType::POST:
      explainQuery();
      break;
    default: { generateNotImplemented("Unsupported method"); }
  }

  // this handler is done
  return RestStatus::DONE;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock REST_DOCUMENT_CREATE
////////////////////////////////////////////////////////////////////////////////

void RestExplainHandler::explainQuery() {
  std::vector<std::string> const& suffixes = _request->suffixes();
  if (suffixes.size() != 0) {
    generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_HTTP_NOT_FOUND);
    return;
  }

  bool parseSuccess = false;
  VPackSlice body = this->parseVPackBody(parseSuccess);
  if (!parseSuccess) {
    return;
  }

  auto badParamError = [&](std::string const& msg) -> void {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_BAD_PARAMETER, msg);
  };

  if (!body.isObject()) {
    badParamError(
        "expected usage: AQL_EXPLAIN(<queryString>, <bindVars>, "
        "<options>)");
    return;
  }

  VPackSlice const querySlice = body.get("query");
  if (!querySlice.isString()) {
    badParamError("expecting string for <queryString>");
    return;
  }
  std::string const queryString(querySlice.copyString());

  VPackSlice const bindSlice = body.get("bindVars");
  if (!bindSlice.isNone() && !bindSlice.isObject()) {
    badParamError("expecting object for <bindVars>");
    return;
  }

  VPackSlice const optionsSlice = body.get("options");
  if (!optionsSlice.isNone() && !optionsSlice.isObject()) {
    badParamError("expecting object for <options>");
    return;
  }

  auto bindBuilder = std::make_shared<VPackBuilder>(bindSlice);

  arangodb::aql::Query query(transaction::StandaloneContext::Create(_vocbase), aql::QueryString(queryString),
                             bindBuilder, optionsSlice);
  auto queryResult = query.explain();

  if (queryResult.result.fail()) {
    generateError(queryResult.result);
    return;
  }

  VPackBuilder result;
  result.openObject();

  if (query.queryOptions().allPlans) {
    result.add("plans", queryResult.data->slice());
  } else {
    result.add("plan", queryResult.data->slice());
    result.add("cacheable", VPackValue(queryResult.cached));
  }

  VPackSlice extras = queryResult.extra->slice();
  if (extras.hasKey("warnings")) {
    result.add("warnings", extras.get("warnings"));
  } else {
    result.add("warnings", VPackSlice::emptyArraySlice());
  }
  if (extras.hasKey("stats")) {
    result.add("stats", extras.get("stats"));
  }

  result.add(StaticStrings::Error, VPackValue(false));
  result.add(StaticStrings::Code, VPackValue(static_cast<int>(ResponseCode::OK)));
  result.close();

  generateResult(rest::ResponseCode::OK, result.slice());
}
