////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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

#include "RestDatabaseHandler.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Rest/HttpRequest.h"
#include "VocBase/Methods/Databases.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

RestDatabaseHandler::RestDatabaseHandler(GeneralRequest* request,
                                         GeneralResponse* response)
    : RestVocbaseBaseHandler(request, response) {}

RestStatus RestDatabaseHandler::execute() {
  // extract the request type
  rest::RequestType const type = _request->requestType();
  if (type == rest::RequestType::GET) {
    return getDatabases();
  } else if (type == rest::RequestType::POST) {
    if (!_vocbase->isSystem()) {
      generateError(GeneralResponse::responseCode(TRI_ERROR_ARANGO_USE_SYSTEM_DATABASE),
                    TRI_ERROR_ARANGO_USE_SYSTEM_DATABASE);
      return RestStatus::DONE;
    }
    return createDatabase();
  } else if (type == rest::RequestType::DELETE_REQ) {
    return deleteDatabase();
  } else {
    generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                  TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
    return RestStatus::DONE;
  }
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief get database infos
// //////////////////////////////////////////////////////////////////////////////
RestStatus RestDatabaseHandler::getDatabases() {
  std::vector<std::string> const& suffixes = _request->suffixes();
  if (suffixes.size() > 1) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER);
    return RestStatus::DONE;
  }

  VPackBuilder result;

  if (suffixes.empty() || suffixes[0] == "user") {
    std::vector<std::string> names;
    if (suffixes.empty()) {
      names = methods::Databases::list(std::string());
    } else if (suffixes[0] == "user") {
      names = methods::Databases::list(_request->user());
    }

    result.openArray();
    for (std::string const& name : names) {
      result.add(VPackValue(name));
    }
    result.close();
  } else if (suffixes[0] == "current") {
    Result res = methods::Databases::info(_vocbase, result);
    if (!res.ok()) {
      generateError(rest::ResponseCode::BAD, res.errorNumber());
      return RestStatus::DONE;
    }
  }

  if (result.isEmpty()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_BAD_PARAMETER);
  } else {
    generateSuccess(rest::ResponseCode::OK, result.slice());
  }
  return RestStatus::DONE;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_get_api_database_create
// //////////////////////////////////////////////////////////////////////////////
RestStatus RestDatabaseHandler::createDatabase() {
  std::vector<std::string> const& suffixes = _request->suffixes();
  bool parseSuccess = true;
  std::shared_ptr<VPackBuilder> parsedBody = parseVelocyPackBody(parseSuccess);
  if (!suffixes.empty() || !parseSuccess) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER);
    return RestStatus::DONE;
  }
  VPackSlice nameVal = parsedBody->slice().get("name");
  if (!nameVal.isString()) {
    generateError(rest::ResponseCode::BAD,
                  TRI_ERROR_ARANGO_DATABASE_NAME_INVALID);
    return RestStatus::DONE;
  }
  std::string dbName = nameVal.copyString();

  VPackSlice options = parsedBody->slice().get("options");
  VPackSlice users = parsedBody->slice().get("users");

  Result res = methods::Databases::create(dbName, users, options);
  if (res.ok()) {
    generateSuccess(rest::ResponseCode::CREATED, VPackSlice::trueSlice());
  } else {
    if (res.errorNumber() == TRI_ERROR_FORBIDDEN ||
        res.errorNumber() == TRI_ERROR_ARANGO_DUPLICATE_NAME) {
      generateError(res);
    } else {// http_server compatibility
      generateError(rest::ResponseCode::BAD, res.errorNumber(), res.errorMessage());
    }
  }
  return RestStatus::DONE;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_get_api_database_delete
// //////////////////////////////////////////////////////////////////////////////
RestStatus RestDatabaseHandler::deleteDatabase() {
  if (!_vocbase->isSystem()) {
    generateError(GeneralResponse::responseCode(TRI_ERROR_ARANGO_USE_SYSTEM_DATABASE),
                  TRI_ERROR_ARANGO_USE_SYSTEM_DATABASE);
    return RestStatus::DONE;
  }
  std::vector<std::string> const& suffixes = _request->suffixes();
  if (suffixes.size() != 1) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER);
    return RestStatus::DONE;
  }

  std::string const& dbName = suffixes[0];

  Result res = methods::Databases::drop(_vocbase, dbName);
  if (res.ok()) {
    generateSuccess(rest::ResponseCode::OK, VPackSlice::trueSlice());
  } else {
    generateError(res);
  }

  return RestStatus::DONE;
}
