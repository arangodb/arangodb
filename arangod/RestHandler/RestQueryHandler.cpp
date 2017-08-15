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

#include "RestQueryHandler.h"

#include "Aql/Query.h"
#include "Aql/QueryList.h"
#include "Basics/conversions.h"
#include "Basics/StringBuffer.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Logger/Logger.h"
#include "Cluster/ClusterComm.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ClusterMethods.h"
#include "Cluster/ServerState.h"
#include "Rest/HttpRequest.h"
#include "VocBase/vocbase.h"

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::basics;
using namespace arangodb::rest;

RestQueryHandler::RestQueryHandler(GeneralRequest* request,
                                   GeneralResponse* response)
    : RestVocbaseBaseHandler(request, response) {}

bool RestQueryHandler::isDirect() const {
  return _request->requestType() != rest::RequestType::POST;
}

RestStatus RestQueryHandler::execute() {
  // extract the sub-request type
  auto const type = _request->requestType();

  // execute one of the CRUD methods
  switch (type) {
    case rest::RequestType::DELETE_REQ:
      deleteQuery();
      break;
    case rest::RequestType::GET:
      readQuery();
      break;
    case rest::RequestType::PUT:
      replaceProperties();
      break;
    case rest::RequestType::POST:
      parseQuery();
      break;
    default:
      generateNotImplemented("ILLEGAL " + DOCUMENT_PATH);
      break;
  }

  // this handler is done
  return RestStatus::DONE;
}

bool RestQueryHandler::readQueryProperties() {
  auto queryList = _vocbase->queryList();

  VPackBuilder result;
  result.add(VPackValue(VPackValueType::Object));
  result.add("error", VPackValue(false));
  result.add("code", VPackValue((int)rest::ResponseCode::OK));
  result.add("enabled", VPackValue(queryList->enabled()));
  result.add("trackSlowQueries", VPackValue(queryList->trackSlowQueries()));
  result.add("trackBindVars", VPackValue(queryList->trackBindVars()));
  result.add("maxSlowQueries", VPackValue(queryList->maxSlowQueries()));
  result.add("slowQueryThreshold",
              VPackValue(queryList->slowQueryThreshold()));
  result.add("maxQueryStringLength",
              VPackValue(queryList->maxQueryStringLength()));
  result.close();

  generateResult(rest::ResponseCode::OK, result.slice());

  return true;
}

bool RestQueryHandler::readQuery(bool slow) {
  auto queryList = _vocbase->queryList();
  auto queries = slow ? queryList->listSlow() : queryList->listCurrent();

  VPackBuilder result;
  result.add(VPackValue(VPackValueType::Array));

  for (auto const& q : queries) {
    auto timeString = TRI_StringTimeStamp(q.started, Logger::getUseLocalTime());

    result.add(VPackValue(VPackValueType::Object));
    result.add("id", VPackValue(StringUtils::itoa(q.id)));
    result.add("query", VPackValue(q.queryString));
    if (q.bindParameters != nullptr) {
      result.add("bindVars", q.bindParameters->slice());
    } else {
      result.add("bindVars", arangodb::basics::VelocyPackHelper::EmptyObjectValue());
    }
    result.add("started", VPackValue(timeString));
    result.add("runTime", VPackValue(q.runTime));
    result.add("state", VPackValue(QueryExecutionState::toString(q.state)));
    result.close();
  }
  result.close();

  generateResult(rest::ResponseCode::OK, result.slice());

  return true;
}

/// @brief returns AQL query tracking
bool RestQueryHandler::readQuery() {
  auto const& suffixes = _request->suffixes();

  if (suffixes.size() != 1) {
    generateError(rest::ResponseCode::BAD,
                  TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expecting GET /_api/query/<type>");
    return true;
  }

  auto const& name = suffixes[0];

  if (name == "slow") {
    return readQuery(true);
  } else if (name == "current") {
    return readQuery(false);
  } else if (name == "properties") {
    return readQueryProperties();
  }

  generateError(rest::ResponseCode::NOT_FOUND,
                TRI_ERROR_HTTP_NOT_FOUND,
                "unknown type '" + name +
                    "', expecting 'slow', 'current', or 'properties'");
  return true;
}

bool RestQueryHandler::deleteQuerySlow() {
  auto queryList = _vocbase->queryList();
  queryList->clearSlow();

  VPackBuilder result;
  result.add(VPackValue(VPackValueType::Object));
  result.add("error", VPackValue(false));
  result.add("code", VPackValue((int)rest::ResponseCode::OK));
  result.close();

  generateResult(rest::ResponseCode::OK, result.slice());

  return true;
}

bool RestQueryHandler::deleteQuery(std::string const& name) {
  auto id = StringUtils::uint64(name);
  auto queryList = _vocbase->queryList();
  TRI_ASSERT(queryList != nullptr);

  auto res = queryList->kill(id);

  if (res == TRI_ERROR_NO_ERROR) {
    VPackBuilder result;
    result.add(VPackValue(VPackValueType::Object));
    result.add("error", VPackValue(false));
    result.add("code", VPackValue((int)rest::ResponseCode::OK));
    result.close();

    generateResult(rest::ResponseCode::OK, result.slice());
  } else {
    generateError(rest::ResponseCode::BAD, res,
                  "cannot kill query '" + name + "'");
  }

  return true;
}

/// @brief interrupts a query
bool RestQueryHandler::deleteQuery() {
  auto const& suffixes = _request->suffixes();

  if (suffixes.size() != 1) {
    generateError(rest::ResponseCode::BAD,
                  TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expecting DELETE /_api/query/<id> or /_api/query/slow");
    return true;
  }

  auto const& name = suffixes[0];

  if (name == "slow") {
    return deleteQuerySlow();
  }
  return deleteQuery(name);
}

bool RestQueryHandler::replaceProperties() {
  auto const& suffixes = _request->suffixes();

  if (suffixes.size() != 1 || suffixes[0] != "properties") {
    generateError(rest::ResponseCode::BAD,
                  TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expecting PUT /_api/query/properties");
    return true;
  }

  bool parseSuccess = true;
  std::shared_ptr<VPackBuilder> parsedBody =
      parseVelocyPackBody(parseSuccess);
  if (!parseSuccess) {
    // error message generated in parseVelocyPackBody
    return true;
  }

  VPackSlice body = parsedBody.get()->slice();
  if (!body.isObject()) {
    generateError(rest::ResponseCode::BAD,
                  TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expecting a JSON object as body");
  };

  auto queryList = _vocbase->queryList();

  bool enabled = queryList->enabled();
  bool trackSlowQueries = queryList->trackSlowQueries();
  bool trackBindVars = queryList->trackBindVars();
  size_t maxSlowQueries = queryList->maxSlowQueries();
  double slowQueryThreshold = queryList->slowQueryThreshold();
  size_t maxQueryStringLength = queryList->maxQueryStringLength();

  VPackSlice attribute;
  attribute = body.get("enabled");
  if (attribute.isBool()) {
    enabled = attribute.getBool();
  }

  attribute = body.get("trackSlowQueries");
  if (attribute.isBool()) {
    trackSlowQueries = attribute.getBool();
  }

  attribute = body.get("trackBindVars");
  if (attribute.isBool()) {
    trackBindVars = attribute.getBool();
  }

  attribute = body.get("maxSlowQueries");
  if (attribute.isInteger()) {
    maxSlowQueries = static_cast<size_t>(attribute.getUInt());
  }

  attribute = body.get("slowQueryThreshold");
  if (attribute.isNumber()) {
    slowQueryThreshold = attribute.getNumber<double>();
  }

  attribute = body.get("maxQueryStringLength");
  if (attribute.isInteger()) {
    maxQueryStringLength = static_cast<size_t>(attribute.getUInt());
  }

  queryList->enabled(enabled);
  queryList->trackSlowQueries(trackSlowQueries);
  queryList->trackBindVars(trackBindVars);
  queryList->maxSlowQueries(maxSlowQueries);
  queryList->slowQueryThreshold(slowQueryThreshold);
  queryList->maxQueryStringLength(maxQueryStringLength);

  return readQueryProperties();
}

bool RestQueryHandler::parseQuery() {
  auto const& suffixes = _request->suffixes();

  if (!suffixes.empty()) {
    generateError(rest::ResponseCode::BAD,
                  TRI_ERROR_HTTP_BAD_PARAMETER, "expecting POST /_api/query");
    return true;
  }

  bool parseSuccess = true;
  std::shared_ptr<VPackBuilder> parsedBody =
      parseVelocyPackBody(parseSuccess);
  if (!parseSuccess) {
    // error message generated in parseVelocyPackBody
    return true;
  }

  VPackSlice body = parsedBody.get()->slice();

  if (!body.isObject()) {
    generateError(rest::ResponseCode::BAD,
                  TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expecting a JSON object as body");
  };

  std::string const queryString =
      VelocyPackHelper::checkAndGetStringValue(body, "query");

  Query query(false, _vocbase, QueryString(queryString),
              nullptr, nullptr, PART_MAIN);

  auto parseResult = query.parse();

  if (parseResult.code != TRI_ERROR_NO_ERROR) {
    generateError(rest::ResponseCode::BAD, parseResult.code,
                  parseResult.details);
    return true;
  }

  VPackBuilder result;
  {
    VPackObjectBuilder b(&result);
    result.add("error", VPackValue(false));
    result.add("code", VPackValue((int)rest::ResponseCode::OK));
    result.add("parsed", VPackValue(true));

    result.add("collections", VPackValue(VPackValueType::Array));
    for (const auto& it : parseResult.collectionNames) {
      result.add(VPackValue(it));
    }
    result.close();  // Collections

    result.add("bindVars", VPackValue(VPackValueType::Array));
    for (const auto& it : parseResult.bindParameters) {
      result.add(VPackValue(it));
    }
    result.close();  // bindVars

    result.add("ast", parseResult.result->slice());

    if (parseResult.warnings != nullptr) {
      result.add("warnings", parseResult.warnings->slice());
    }
  }

  generateResult(rest::ResponseCode::OK, result.slice());
  return true;
}
