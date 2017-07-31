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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include "RestExplainHandler.h"

#include "Aql/Query.h"
#include "Aql/QueryString.h"
#include "Logger/Logger.h"

#include <velocypack/Builder.h>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

RestExplainHandler::RestExplainHandler(GeneralRequest* request,
                                       GeneralResponse* response)
    : RestVocbaseBaseHandler(request, response) {}

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

  if (_vocbase == nullptr) {
    generateError(rest::ResponseCode::NOT_FOUND,
                  TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
    return;
  }

  bool parseSuccess = true;
  std::shared_ptr<VPackBuilder> parsedBody = parseVelocyPackBody(parseSuccess);

  if (!parseSuccess) {
    return;
  }
  VPackSlice body = parsedBody.get()->slice();

  auto badParamError = [&](std::string const& msg) -> void {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_BAD_PARAMETER, msg);
  };

  if (!body.isObject() || body.length() < 1 || body.length() > 3) {
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

  auto bindBuilder = std::make_shared<VPackBuilder>();
  bindBuilder->add(bindSlice);

  auto optionsBuilder = std::make_shared<VPackBuilder>();
  optionsBuilder->add(optionsSlice);

  arangodb::aql::Query query(false, _vocbase, aql::QueryString(queryString),
                             bindBuilder, optionsBuilder,
                             arangodb::aql::PART_MAIN);

  auto queryResult = query.explain();

  if (queryResult.code != TRI_ERROR_NO_ERROR) {
    auto code = rest::ResponseCode::BAD;

    switch (queryResult.code) {
      case TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND:
        code = rest::ResponseCode::NOT_FOUND;
        break;
    }

    generateError(code, queryResult.code, queryResult.details);
    return;
  }

  VPackBuilder result;
  result.openObject();

  if (query.queryOptions().allPlans) {
    result.add("plans", queryResult.result->slice());
  } else {
    result.add("plan", queryResult.result->slice());
    result.add("cacheable", VPackValue(queryResult.cached));
  }

  if (queryResult.warnings == nullptr) {
    result.add("warnings", VPackSlice::emptyArraySlice());
  } else {
    result.add("warnings", queryResult.warnings->slice());
  }
  if (queryResult.stats != nullptr) {
    VPackSlice stats = queryResult.stats->slice();
    if (stats.isNone()) {
      result.add("stats", VPackSlice::noneSlice());
    } else {
      result.add("stats", stats);
    }
  } else {
    result.add("stats", VPackSlice::noneSlice());
  }

  result.add("error", VPackValue(false));
  result.add("code", VPackValue(static_cast<int>(rest::ResponseCode::OK)));

  result.close();

  generateResult(rest::ResponseCode::OK, result.slice());
}
