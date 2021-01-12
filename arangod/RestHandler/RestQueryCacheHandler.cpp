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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "RestQueryCacheHandler.h"
#include "Aql/QueryCache.h"

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::basics;
using namespace arangodb::rest;

RestQueryCacheHandler::RestQueryCacheHandler(application_features::ApplicationServer& server,
                                             GeneralRequest* request,
                                             GeneralResponse* response)
    : RestVocbaseBaseHandler(server, request, response) {}

RestStatus RestQueryCacheHandler::execute() {
  // extract the sub-request type
  auto const type = _request->requestType();

  switch (type) {
    case rest::RequestType::GET:
      executeRead();
      break;
    case rest::RequestType::DELETE_REQ:
      clearCache();
      break;
    case rest::RequestType::PUT:
      replaceProperties();
      break;
    default:
      generateNotImplemented("ILLEGAL " + DOCUMENT_PATH);
      break;
  }

  // this handler is done
  return RestStatus::DONE;
}

void RestQueryCacheHandler::clearCache() {
  auto queryCache = arangodb::aql::QueryCache::instance();
  queryCache->invalidate(&_vocbase);

  VPackBuilder result;
  result.add(VPackValue(VPackValueType::Object));
  result.add(StaticStrings::Error, VPackValue(false));
  result.add(StaticStrings::Code, VPackValue((int)rest::ResponseCode::OK));
  result.close();
  generateResult(rest::ResponseCode::OK, result.slice());
}

void RestQueryCacheHandler::executeRead() {
  auto const& suffixes = _request->suffixes();

  if (suffixes.size() != 1 ||
      (suffixes[0] != "properties" && suffixes[0] != "entries")) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expecting GET /_api/query-cache/properties or "
                  "/_api/query-cache/entries");
    return;
  }

  if (suffixes[0] == "properties") {
    readProperties();
  } else {
    readQueries();
  }
}

void RestQueryCacheHandler::readQueries() {
  auto queryCache = arangodb::aql::QueryCache::instance();

  VPackBuilder result;
  queryCache->queriesToVelocyPack(&_vocbase, result);
  generateResult(rest::ResponseCode::OK, result.slice());
}

void RestQueryCacheHandler::readProperties() {
  auto queryCache = arangodb::aql::QueryCache::instance();

  VPackBuilder result;
  queryCache->toVelocyPack(result);
  generateResult(rest::ResponseCode::OK, result.slice());
}

void RestQueryCacheHandler::replaceProperties() {
  auto const& suffixes = _request->suffixes();

  if (suffixes.size() != 1 || suffixes[0] != "properties") {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expecting PUT /_api/query-cache/properties");
    return;
  }

  bool validBody = false;
  VPackSlice body = this->parseVPackBody(validBody);
  if (!validBody) {
    // error message generated in parseJsonBody
    return;
  }

  if (!body.isObject()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "expecting a JSON-Object body");
    return;
  }

  arangodb::aql::QueryCache::instance()->properties(body);
  readProperties();
}
