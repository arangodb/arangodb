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

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

#include "RestServer/DatabaseFeature.h"
#include "Agency/AgencyComm.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ServerState.h"
#include "VocBase/modes.h"
#include "VocBase/vocbase.h"
#include "V8Server/V8Context.h"
#include "V8Server/V8DealerFeature.h"
#include "V8/v8-conv.h"
#include "V8/v8-utils.h"
#include "V8/v8-vpack.h"

#include <v8.h>


using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

RestDatabaseHandler::RestDatabaseHandler(GeneralRequest* request, GeneralResponse* response)
  : RestVocbaseBaseHandler(request, response) {}

RestStatus RestDatabaseHandler::execute() {
  // extract the request type
  rest::RequestType const type = _request->requestType();
  if (type == rest::RequestType::GET) {
    return getDatabases();
  } else if (type == rest::RequestType::POST) {
    if (!_vocbase->isSystem()) {
      generateError(rest::ResponseCode::BAD, TRI_ERROR_ARANGO_USE_SYSTEM_DATABASE);
      return RestStatus::DONE;
    }
    return createDatabase();
  } else if (type == rest::RequestType::DELETE_REQ) {
    if (!_vocbase->isSystem()) {
      generateError(rest::ResponseCode::BAD, TRI_ERROR_ARANGO_USE_SYSTEM_DATABASE);
      return RestStatus::DONE;
    }
    return deleteDatabase();
  } else {
    generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                  TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
    return RestStatus::DONE;
  }

  
  /*bool parseSuccess = true;
  std::shared_ptr<VPackBuilder> parsedBody =
      parseVelocyPackBody(parseSuccess);

  if (!parseSuccess) {
    generateResult(rest::ResponseCode::OK, VPackSlice());
    return RestStatus::DONE;
  }
  
  VPackBuilder result;*/
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

  DatabaseFeature *databaseFeature =
    application_features::ApplicationServer::getFeature<DatabaseFeature>("Database");
  if (databaseFeature == nullptr) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_SERVER_ERROR);
    return RestStatus::DONE;
  }
  
  VPackBuilder result;
  
  std::vector<std::string> names;
  if (suffixes.empty()) {
    // list of all databases
    names = databaseFeature->getDatabaseNames();
  } else if (suffixes[0] == "user") {
    // fetch all databases for the current user
    // note: req.user may be null if authentication is turned off
    if (_request->user().empty()) {
      names = databaseFeature->getDatabaseNames();
    } else {
      names = databaseFeature->getDatabaseNamesForUser(_request->user());
    }
  } else if (suffixes[0] == "current") {
    if (ServerState::instance()->isCoordinator()) {
      
      AgencyComm agency;
      AgencyCommResult commRes = agency.getValues("Plan/Databases/" + _request->databaseName());
      if (!commRes.successful()) {
        // Error in communication, note that value not found is not an error
        LOG_TOPIC(TRACE, Logger::REQUESTS)
        << "rest database handler: no agency communication";
        generateError(rest::ResponseCode::BAD, commRes.errorCode());
        return RestStatus::DONE;
      }

      VPackSlice value = commRes.slice()[0].get(
          {AgencyCommManager::path(), "Plan", "Databases", _request->databaseName()});
      if (value.isObject() && value.hasKey("name")) {
        VPackValueLength l = 0;
        const char* name = value.getString(l);
        TRI_ASSERT(l > 0);
       
        result.openObject();
        result.add("name", value.get("name"));
        result.add("id", value.get("id"));
        result.add("path", value.get("none"));
        result.add("isSystem", VPackValue(name[0] == '_'));
        result.close();
      }
    } else {
      // information about the current database
      TRI_vocbase_t* vocbase = nullptr;
      if (ServerState::instance()->isCoordinator()) {
        vocbase = databaseFeature->useDatabaseCoordinator(_request->databaseName());
      } else {
        // check if the other database exists, and increase its refcount
        vocbase = databaseFeature->useDatabase(_request->databaseName());
      }
      
      if (vocbase == nullptr) {
        generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER);
        return RestStatus::DONE;
      }
      TRI_ASSERT(!vocbase->isDangling());
      
      result.openObject();
      result.add("name", VPackValue(vocbase->name()));
      result.add("id", VPackValue(vocbase->id()));
      result.add("path", VPackValue(vocbase->path()));
      result.add("isSystem", VPackValue(vocbase->isSystem()));
      result.close();
    }
  }
  
  // has to happen for the first two cases
  if (!names.empty() && result.isEmpty()) {
    result.openArray();
    for (std::string const& name : names) {
      result.add(VPackValue(name));
    }
    result.close();
  }
  
  if (result.isEmpty()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER);
  } else {
    VPackBuilder pack;
    pack.openObject();
    pack.add("result", result.slice());
    pack.add("error", VPackValue(false));
    pack.close();
    generateResult(rest::ResponseCode::OK, pack.slice());
  }
  return RestStatus::DONE;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_get_api_database_create
// //////////////////////////////////////////////////////////////////////////////
RestStatus RestDatabaseHandler::createDatabase() {
  if (TRI_GetOperationModeServer() == TRI_VOCBASE_MODE_NO_CREATE) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_ARANGO_READ_ONLY);
    return RestStatus::DONE;
  }
  
  std::vector<std::string> const& suffixes = _request->suffixes();
  bool parseSuccess = true;
  std::shared_ptr<VPackBuilder> parsedBody =
  parseVelocyPackBody(parseSuccess);
  if (!suffixes.empty() || !parseSuccess) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER);
    return RestStatus::DONE;
  }
  VPackSlice nameVal = parsedBody->slice().get("name");
  if (!nameVal.isString()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_ARANGO_DATABASE_NAME_INVALID);
    return RestStatus::DONE;
  }
  std::string dbName = nameVal.copyString();
  
  VPackSlice options = parsedBody->slice().get("options");
  if (options.isNone() || options.isNull()) {
    options = VPackSlice::emptyObjectSlice();
  } else if (!options.isObject()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER);
    return RestStatus::DONE;
  }
  
  VPackSlice users = parsedBody->slice().get("users");
  if (users.isNone() || users.isNull()) {
    users = VPackSlice::emptyArraySlice();
  } else if (!users.isArray()) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER);
    return RestStatus::DONE;
  }
  
  VPackBuilder sanitizedUsers;
  sanitizedUsers.openArray();
  for (VPackSlice const& user : VPackArrayIterator(users)) {
    sanitizedUsers.openObject();
    if (!user.isObject()) {
      generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER);
      return RestStatus::DONE;
    }
    
    VPackSlice name;
    if (user.hasKey("username")) {
      name = user.get("username");
    } else if (user.hasKey("user")) {
      name = user.get("user");
    }
    if (!name.isString()) {
      generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER);
      return RestStatus::DONE;
    }
    sanitizedUsers.add("username", name);
    
    if (user.hasKey("passwd")) {
      VPackSlice passwd = user.get("passwd");
      if (!passwd.isString()) {
        generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER);
        return RestStatus::DONE;
      }
      sanitizedUsers.add("passwd", passwd);
    } else {
      sanitizedUsers.add("passwd", VPackValue(""));
    }
    
    VPackSlice active = user.get("active");
    if (!active.isBool()) {
      sanitizedUsers.add("active", VPackValue(true));
    } else {
      sanitizedUsers.add("active", active);
    }
    
    VPackSlice extra = user.get("extra");
    if (extra.isObject()) {
      sanitizedUsers.add("extra", extra);
    }
    sanitizedUsers.close();
  }
  sanitizedUsers.close();
  
  
  DatabaseFeature *databaseFeature =
  application_features::ApplicationServer::getFeature<DatabaseFeature>("Database");
  if (databaseFeature == nullptr) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_SERVER_ERROR);
    return RestStatus::DONE;
  }
  
  if (ServerState::instance()->isCoordinator()) {
    if (!TRI_vocbase_t::IsAllowedName(false, dbName)) {
      generateError(rest::ResponseCode::BAD, TRI_ERROR_ARANGO_DATABASE_NAME_INVALID);
      return RestStatus::DONE;
    }
    
    uint64_t const id = ClusterInfo::instance()->uniqid();
    VPackBuilder builder;
    try {
      VPackObjectBuilder b(&builder);
      std::string const idString(StringUtils::itoa(id));
      builder.add("id", VPackValue(idString));      
      builder.add("name", nameVal);
      builder.add("options", options);
      builder.add("coordinator", VPackValue(ServerState::instance()->getId()));
    } catch (VPackException const& e) {
      generateError(rest::ResponseCode::SERVER_ERROR, e.errorCode());
      return RestStatus::DONE;
    }
    
    ClusterInfo* ci = ClusterInfo::instance();
    std::string errorMsg;
    
    int res =
    ci->createDatabaseCoordinator(dbName, builder.slice(), errorMsg, 120.0);
    if (res != TRI_ERROR_NO_ERROR) {
      generateError(res == TRI_ERROR_ARANGO_DUPLICATE_NAME ?
                    rest::ResponseCode::CONFLICT : rest::ResponseCode::BAD, res);

      return RestStatus::DONE;
    }
    // database was created successfully in agency

    
    // now wait for heartbeat thread to create the database object
    TRI_vocbase_t* vocbase = nullptr;
    int tries = 0;
    while (++tries <= 6000) {
      vocbase = databaseFeature->useDatabaseCoordinator(id);
      if (vocbase != nullptr) {
        break;
      }
      // sleep
      usleep(10000);
    }
    
    if (vocbase == nullptr) {
      generateError(rest::ResponseCode::SERVER_ERROR, TRI_ERROR_INTERNAL, "unable to find database");
      return RestStatus::DONE;
    }
    
    V8Context* ctx = V8DealerFeature::DEALER->enterContext(vocbase, true);
    if (ctx == nullptr) {
      generateError(rest::ResponseCode::SERVER_ERROR, TRI_ERROR_INTERNAL,
                    "Could not get v8 context");
      return RestStatus::DONE;
    }
    TRI_DEFER(V8DealerFeature::DEALER->exitContext(ctx));
    v8::Isolate* isolate = ctx->_isolate;
    v8::HandleScope scope(isolate);
    
    // now run upgrade and copy users into context
    TRI_ASSERT(sanitizedUsers.slice().isArray());
    v8::Handle<v8::Object> userVar = v8::Object::New(ctx->_isolate);
    userVar->Set(TRI_V8_ASCII_STRING("users"),
                 TRI_VPackToV8(isolate, sanitizedUsers.slice()));
    isolate->GetCurrentContext()->Global()->Set(TRI_V8_ASCII_STRING("UPGRADE_ARGS"),
                                                userVar);
    
    TRI_GET_GLOBALS();
    // switch databases
    TRI_vocbase_t* orig = v8g->_vocbase;
    TRI_ASSERT(orig != nullptr);
    v8g->_vocbase = vocbase;
    
    // initalize database
    bool allowUseDatabase = v8g->_allowUseDatabase;
    v8g->_allowUseDatabase = true;
    
    V8DealerFeature::DEALER->startupLoader()->executeGlobalScript(
                                                                  isolate, isolate->GetCurrentContext(),
                                                                  "server/bootstrap/coordinator-database.js");
    
    v8g->_allowUseDatabase = allowUseDatabase;
    
    // and switch back
    v8g->_vocbase = orig;
    
    vocbase->release();
  } else {
    // options for database (currently only allows setting "id" for testing
    // purposes)
    TRI_voc_tick_t id = 0;
    if (options.hasKey("id")) {
      id = options.get("id").getUInt();
    }
    
    TRI_vocbase_t* vocbase = nullptr;
    int res = databaseFeature->createDatabase(id, dbName, vocbase);
    
    if (res != TRI_ERROR_NO_ERROR) {
      generateError(res == TRI_ERROR_ARANGO_DUPLICATE_NAME ?
                    rest::ResponseCode::CONFLICT : rest::ResponseCode::BAD, res);
      return RestStatus::DONE;
    }
    
    TRI_ASSERT(vocbase != nullptr);
    TRI_ASSERT(!vocbase->isDangling());
    
    
    V8Context* ctx = V8DealerFeature::DEALER->enterContext(vocbase, true);
    if (ctx == nullptr) {
      generateError(rest::ResponseCode::SERVER_ERROR, TRI_ERROR_INTERNAL,
                    "Could not get v8 context");
      return RestStatus::DONE;
    }
    TRI_DEFER(V8DealerFeature::DEALER->exitContext(ctx));
    v8::Isolate* isolate = ctx->_isolate;
    v8::HandleScope scope(isolate);
    
    // copy users into context
    TRI_ASSERT(sanitizedUsers.slice().isArray());
    v8::Handle<v8::Object> userVar = v8::Object::New(ctx->_isolate);
    userVar->Set(TRI_V8_ASCII_STRING("users"),
                 TRI_VPackToV8(isolate, sanitizedUsers.slice()));
    isolate->GetCurrentContext()->Global()->Set(TRI_V8_ASCII_STRING("UPGRADE_ARGS"),
                                                userVar);
    
    // switch databases
    {
      TRI_GET_GLOBALS();
      TRI_vocbase_t* orig = v8g->_vocbase;
      TRI_ASSERT(orig != nullptr);
      
      v8g->_vocbase = vocbase;
      
      // initalize database
      try {
        V8DealerFeature::DEALER->startupLoader()->executeGlobalScript(
                                                                      isolate, isolate->GetCurrentContext(),
                                                                      "server/bootstrap/local-database.js");
        if (v8g->_vocbase == vocbase) {
          // decrease the reference-counter only if we are coming back with the same database
          vocbase->release();
        }
        
        // and switch back
        v8g->_vocbase = orig;
      } catch (...) {
        if (v8g->_vocbase == vocbase) {
          // decrease the reference-counter only if we are coming back with the same database
          vocbase->release();
        }
        
        // and switch back
        v8g->_vocbase = orig;
        
        generateError(rest::ResponseCode::SERVER_ERROR, TRI_ERROR_INTERNAL,
                      "Could not get v8 context");
        return RestStatus::DONE;
      }
    }
  }
  
  VPackBuilder b;
  b.openObject();
  b.add("result", VPackValue(true));
  b.add("error", VPackValue(false));
  b.close();
  generateResult(rest::ResponseCode::CREATED, b.slice());
  return RestStatus::DONE;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief was docuBlock JSF_get_api_database_delete
// //////////////////////////////////////////////////////////////////////////////
RestStatus RestDatabaseHandler::deleteDatabase() {
  std::vector<std::string> const& suffixes = _request->suffixes();
  if (suffixes.size() != 1) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER);
    return RestStatus::DONE;
  }
  
  std::string const& dbName = suffixes[0];
  
  V8Context* ctx = V8DealerFeature::DEALER->enterContext(_vocbase, true);
  if (ctx == nullptr) {
    generateError(rest::ResponseCode::SERVER_ERROR, TRI_ERROR_INTERNAL,
                  "Could not get v8 context");
    return RestStatus::DONE;
  }
  TRI_DEFER(V8DealerFeature::DEALER->exitContext(ctx));
  v8::Isolate* isolate = ctx->_isolate;
  v8::HandleScope scope(isolate);
  
  // clear collections in cache object
  TRI_ClearObjectCacheV8(isolate);
  
  // If we are a coordinator in a cluster, we have to behave differently:
  if (ServerState::instance()->isCoordinator()) {
    // Arguments are already checked, there is exactly one argument
    auto databaseFeature =
    application_features::ApplicationServer::getFeature<DatabaseFeature>(
                                                                         "Database");
    TRI_vocbase_t* vocbase = databaseFeature->useDatabaseCoordinator(dbName);
    
    if (vocbase == nullptr) {
      // no such database
      generateError(rest::ResponseCode::BAD, TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
      return RestStatus::DONE;
    }
    
    TRI_voc_tick_t const id = vocbase->id();
    vocbase->release();
    
    ClusterInfo* ci = ClusterInfo::instance();
    std::string errorMsg;
    
    int res = ci->dropDatabaseCoordinator(dbName, errorMsg, 120.0);
    
    if (res != TRI_ERROR_NO_ERROR) {
      generateError(res == TRI_ERROR_ARANGO_DATABASE_NOT_FOUND ?
                    rest::ResponseCode::NOT_FOUND : rest::ResponseCode::SERVER_ERROR, res);
      return RestStatus::DONE;
    }
    
    // now wait for heartbeat thread to drop the database object
    int tries = 0;
    
    while (++tries <= 6000) {
      TRI_vocbase_t* vocbase = databaseFeature->useDatabaseCoordinator(id);
      
      if (vocbase == nullptr) {
        // object has vanished
        break;
      }
      
      vocbase->release();
      // sleep
      usleep(10000);
    }
    
  } else {
    DatabaseFeature* databaseFeature =
    application_features::ApplicationServer::getFeature<DatabaseFeature>("Database");
    int res = databaseFeature->dropDatabase(dbName, false, true);
    
    if (res != TRI_ERROR_NO_ERROR) {
      generateError(res == TRI_ERROR_ARANGO_DATABASE_NOT_FOUND ?
                    rest::ResponseCode::NOT_FOUND : rest::ResponseCode::SERVER_ERROR, res);
      return RestStatus::DONE;
    }
    
    // run the garbage collection in case the database held some objects which can
    // now be freed
    TRI_RunGarbageCollectionV8(isolate, 0.25);
    
    TRI_ExecuteJavaScriptString(
                                isolate, isolate->GetCurrentContext(),
                                TRI_V8_ASCII_STRING(
                                                    "require('internal').executeGlobalContextFunction('reloadRouting')"),
                                TRI_V8_ASCII_STRING("reload routing"), false);
  }
  
  VPackBuilder b;
  b.openObject();
  b.add("result", VPackValue(true));
  b.add("error", VPackValue(false));
  b.close();
  generateResult(rest::ResponseCode::OK, b.slice());
  return RestStatus::DONE;
}
