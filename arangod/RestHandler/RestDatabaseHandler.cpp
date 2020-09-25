////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "RestDatabaseHandler.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Utils/Events.h"
#include "VocBase/Methods/Databases.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

RestDatabaseHandler::RestDatabaseHandler(application_features::ApplicationServer& server,
                                         GeneralRequest* request, GeneralResponse* response)
    : RestVocbaseBaseHandler(server, request, response) {}

RestStatus RestDatabaseHandler::execute() {
  // extract the request type
  rest::RequestType const type = _request->requestType();
  if (type == rest::RequestType::GET) {
    return getDatabases();
  } else if (type == rest::RequestType::POST) {
    return createDatabase();
  } else if (type == rest::RequestType::DELETE_REQ) {
    return deleteDatabase();
  } else {
    generateError(rest::ResponseCode::METHOD_NOT_ALLOWED, TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);

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

  Result res;
  VPackBuilder builder;
  if (suffixes.empty() || suffixes[0] == "user") {
    std::vector<std::string> names;
    if (suffixes.empty()) {
      if (!_vocbase.isSystem()) {
        res.reset(TRI_ERROR_ARANGO_USE_SYSTEM_DATABASE);
      } else {
        names = methods::Databases::list(server(), std::string());
      }
    } else if (suffixes[0] == "user") {
      if (!_request->authenticated() && ExecContext::isAuthEnabled()) {
        res.reset(TRI_ERROR_FORBIDDEN);
      } else {
        names = methods::Databases::list(server(), _request->user());
      }
    }

    builder.openArray();
    for (std::string const& name : names) {
      builder.add(VPackValue(name));
    }
    builder.close();
  } else if (suffixes[0] == "current") {
    _vocbase.toVelocyPack(builder);
  }

  if (res.fail()) {
    generateError(res);
  } else if (builder.isEmpty()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_BAD_PARAMETER);
  } else {
    generateOk(rest::ResponseCode::OK, builder.slice());
  }
  return RestStatus::DONE;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_get_api_database_create
// //////////////////////////////////////////////////////////////////////////////
RestStatus RestDatabaseHandler::createDatabase() {
  if (!_vocbase.isSystem()) {
    generateError(GeneralResponse::responseCode(TRI_ERROR_ARANGO_USE_SYSTEM_DATABASE),
                  TRI_ERROR_ARANGO_USE_SYSTEM_DATABASE);
    events::CreateDatabase("", Result(TRI_ERROR_ARANGO_USE_SYSTEM_DATABASE), _context);
    return RestStatus::DONE;
  }

  std::vector<std::string> const& suffixes = _request->suffixes();
  bool parseSuccess = false;
  VPackSlice body = this->parseVPackBody(parseSuccess);
  if (!suffixes.empty() || !parseSuccess) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER);
    events::CreateDatabase("", Result(TRI_ERROR_BAD_PARAMETER), _context);
    return RestStatus::DONE;
  }
  VPackSlice nameVal = body.get("name");
  if (!nameVal.isString()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_ARANGO_DATABASE_NAME_INVALID);
    events::CreateDatabase("", Result(TRI_ERROR_ARANGO_DATABASE_NAME_INVALID), _context);
    return RestStatus::DONE;
  }
  std::string dbName = nameVal.copyString();

  VPackSlice options = body.get("options");
  VPackSlice users = body.get("users");

  Result res = methods::Databases::create(server(), _context, dbName, users, options);
  if (res.ok()) {
    generateOk(rest::ResponseCode::CREATED, VPackSlice::trueSlice());
  } else {
    if (res.errorNumber() == TRI_ERROR_FORBIDDEN ||
        res.errorNumber() == TRI_ERROR_ARANGO_DUPLICATE_NAME) {
      generateError(res);
    } else {  // http_server compatibility
      generateError(rest::ResponseCode::BAD, res.errorNumber(), res.errorMessage());
    }
  }
  return RestStatus::DONE;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_get_api_database_delete
// //////////////////////////////////////////////////////////////////////////////
RestStatus RestDatabaseHandler::deleteDatabase() {
  if (!_vocbase.isSystem()) {
    generateError(GeneralResponse::responseCode(TRI_ERROR_ARANGO_USE_SYSTEM_DATABASE),
                  TRI_ERROR_ARANGO_USE_SYSTEM_DATABASE);
    events::DropDatabase("", Result(TRI_ERROR_ARANGO_USE_SYSTEM_DATABASE), _context);
    return RestStatus::DONE;
  }
  std::vector<std::string> const& suffixes = _request->suffixes();
  if (suffixes.size() != 1) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER);
    events::DropDatabase("", Result(TRI_ERROR_HTTP_BAD_PARAMETER), _context);
    return RestStatus::DONE;
  }

  std::string const& dbName = suffixes[0];
  Result res = methods::Databases::drop(_context, &_vocbase, dbName);

  if (res.ok()) {
    generateOk(rest::ResponseCode::OK, VPackSlice::trueSlice());
  } else {
    generateError(res);
  }

  return RestStatus::DONE;
}
