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

#include "RestQueryHandler.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Aql/Query.h"
#include "Aql/QueryList.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ClusterMethods.h"
#include "Cluster/ServerState.h"
#include "Transaction/Helpers.h"
#include "Transaction/StandaloneContext.h"
#include "VocBase/Methods/Queries.h"
#include "VocBase/vocbase.h"

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::basics;
using namespace arangodb::rest;

RestQueryHandler::RestQueryHandler(application_features::ApplicationServer& server,
                                   GeneralRequest* request, GeneralResponse* response)
    : RestVocbaseBaseHandler(server, request, response) {}

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
      generateNotImplemented("ILLEGAL /_api/query");
      break;
  }

  // this handler is done
  return RestStatus::DONE;
}

void RestQueryHandler::readQueryProperties() {
  auto queryList = _vocbase.queryList();
  VPackBuilder result;

  result.add(VPackValue(VPackValueType::Object));
  result.add(StaticStrings::Error, VPackValue(false));
  result.add(StaticStrings::Code, VPackValue((int)rest::ResponseCode::OK));
  result.add("enabled", VPackValue(queryList->enabled()));
  result.add("trackSlowQueries", VPackValue(queryList->trackSlowQueries()));
  result.add("trackBindVars", VPackValue(queryList->trackBindVars()));
  result.add("maxSlowQueries", VPackValue(queryList->maxSlowQueries()));
  result.add("slowQueryThreshold", VPackValue(queryList->slowQueryThreshold()));
  result.add("slowStreamingQueryThreshold",
             VPackValue(queryList->slowStreamingQueryThreshold()));
  result.add("maxQueryStringLength", VPackValue(queryList->maxQueryStringLength()));
  result.close();

  generateResult(rest::ResponseCode::OK, result.slice());
}

void RestQueryHandler::readQuery(bool slow) {
  Result res;
  VPackBuilder result;
    
  bool const allDatabases = _request->parsedValue("all", false);
  bool const fanout = ServerState::instance()->isCoordinator() && !_request->parsedValue("local", false);
  if (slow) {
    res = methods::Queries::listSlow(_vocbase, result, allDatabases, fanout);
  } else {
    res = methods::Queries::listCurrent(_vocbase, result, allDatabases, fanout);
  }

  if (res.ok()) {
    generateResult(rest::ResponseCode::OK, result.slice());
  } else {
    generateError(res);
  }
}

/// @brief returns AQL query tracking
void RestQueryHandler::readQuery() {
  auto const& suffixes = _request->suffixes();

  if (suffixes.size() != 1) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expecting GET /_api/query/<type>");
    return;
  }

  auto const& name = suffixes[0];

  if (name == "slow") {
    readQuery(true);
  } else if (name == "current") {
    readQuery(false);
  } else if (name == "properties") {
    readQueryProperties();
  } else {
    generateError(rest::ResponseCode::NOT_FOUND, TRI_ERROR_HTTP_NOT_FOUND,
                  "unknown type '" + name +
                      "', expecting 'slow', 'current', or 'properties'");
  }
}

void RestQueryHandler::deleteQuerySlow() {
  bool const allDatabases = _request->parsedValue("all", false);
  bool const fanout = ServerState::instance()->isCoordinator() && !_request->parsedValue("local", false);
  
  Result res = methods::Queries::clearSlow(_vocbase, allDatabases, fanout);

  if (res.ok()) {
    generateOk(rest::ResponseCode::OK, velocypack::Slice::noneSlice());
  } else {
    generateError(res);
  }
}

void RestQueryHandler::killQuery(std::string const& id) {
  bool const allDatabases = _request->parsedValue("all", false);
  Result res = methods::Queries::kill(_vocbase, StringUtils::uint64(id), allDatabases);

  if (res.ok()) {
    generateOk(rest::ResponseCode::OK, velocypack::Slice::noneSlice());
  } else {
    generateError(res);
  }
}

/// @brief interrupts a query
void RestQueryHandler::deleteQuery() {
  auto const& suffixes = _request->suffixes();

  if (suffixes.size() != 1) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expecting DELETE /_api/query/<id> or /_api/query/slow");
    return;
  }

  auto const& id = suffixes[0];

  if (id == "slow") {
    deleteQuerySlow();
  } else {
    killQuery(id);
  }
}

void RestQueryHandler::replaceProperties() {
  auto const& suffixes = _request->suffixes();

  if (suffixes.size() != 1 || suffixes[0] != "properties") {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expecting PUT /_api/query/properties");
    return;
  }

  bool parseSuccess = false;
  VPackSlice body = this->parseVPackBody(parseSuccess);
  if (!parseSuccess) {
    // error message generated in parseVPackBody
    return;
  }

  if (!body.isObject()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expecting a JSON object as body");
    return;
  }

  auto queryList = _vocbase.queryList();
  bool enabled = queryList->enabled();
  bool trackSlowQueries = queryList->trackSlowQueries();
  bool trackBindVars = queryList->trackBindVars();
  size_t maxSlowQueries = queryList->maxSlowQueries();
  double slowQueryThreshold = queryList->slowQueryThreshold();
  double slowStreamingQueryThreshold = queryList->slowStreamingQueryThreshold();
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

  attribute = body.get("slowStreamingQueryThreshold");
  if (attribute.isNumber()) {
    slowStreamingQueryThreshold = attribute.getNumber<double>();
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
  queryList->slowStreamingQueryThreshold(slowStreamingQueryThreshold);
  queryList->maxQueryStringLength(maxQueryStringLength);

  readQueryProperties();
}

void RestQueryHandler::parseQuery() {
  auto const& suffixes = _request->suffixes();

  if (!suffixes.empty()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expecting POST /_api/query");
    return;
  }

  bool parseSuccess = false;
  VPackSlice body = this->parseVPackBody(parseSuccess);
  if (!parseSuccess) {
    // error message generated in parseVPackBody
    return;
  }

  if (!body.isObject()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expecting a JSON object as body");
    return;
  }

  std::string const queryString =
      VelocyPackHelper::checkAndGetStringValue(body, "query");

  Query query(transaction::StandaloneContext::Create(_vocbase),
              QueryString(queryString), nullptr);
  auto parseResult = query.parse();

  if (parseResult.result.fail()) {
    generateError(parseResult.result);
    return;
  }

  VPackBuilder result;
  {
    VPackObjectBuilder b(&result);
    result.add(StaticStrings::Error, VPackValue(false));
    result.add(StaticStrings::Code, VPackValue((int)rest::ResponseCode::OK));
    result.add("parsed", VPackValue(true));

    result.add("collections", VPackValue(VPackValueType::Array));
    for (const auto& it : parseResult.collectionNames) {
      result.add(VPackValue(it));
    }
    result.close();  // collections

    result.add("bindVars", VPackValue(VPackValueType::Array));
    for (const auto& it : parseResult.bindParameters) {
      result.add(VPackValue(it));
    }
    result.close();  // bindVars

    result.add("ast", parseResult.data->slice());

    if (parseResult.extra && parseResult.extra->slice().hasKey("warnings")) {
      result.add("warnings", parseResult.extra->slice().get("warnings"));
    }
  }

  generateResult(rest::ResponseCode::OK, result.slice());
}

/// @brief returns the short id of the server which should handle this request
ResultT<std::pair<std::string, bool>> RestQueryHandler::forwardingTarget() {
  TRI_ASSERT(ServerState::instance()->isCoordinator());

  auto base = RestVocbaseBaseHandler::forwardingTarget();
  if (base.ok() && !std::get<0>(base.get()).empty()) {
    return base;
  }

  if (_request->requestType() == RequestType::DELETE_REQ) {
    // kill operation
    auto const& suffixes = _request->suffixes();
    TRI_ASSERT(suffixes.size() >= 1);
    auto const& id = suffixes[0];
    if (id != "slow") {
      uint64_t tick = basics::StringUtils::uint64(id);
      uint32_t sourceServer = TRI_ExtractServerIdFromTick(tick);
      if (sourceServer != ServerState::instance()->getShortId()) {
        auto& ci = server().getFeature<ClusterFeature>().clusterInfo();
        return {std::make_pair(ci.getCoordinatorByShortID(sourceServer), false)};
      }
    }
  }

  return {std::make_pair(StaticStrings::Empty, false)};
}
