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
/// @author Simon Gräter
////////////////////////////////////////////////////////////////////////////////

#include "Databases.h"
#include "Basics/Common.h"

#include "Agency/AgencyComm.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ServerState.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "Rest/HttpRequest.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/SystemDatabaseFeature.h"
#include "Utils/ExecContext.h"
#include "V8/v8-utils.h"
#include "V8/v8-vpack.h"
#include "V8Server/V8Context.h"
#include "V8Server/V8DealerFeature.h"
#include "V8Server/v8-dispatcher.h"
#include "V8Server/v8-user-structures.h"
#include "VocBase/Methods/Upgrade.h"
#include "VocBase/vocbase.h"

#include <v8.h>
#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::methods;

TRI_vocbase_t* Databases::lookup(std::string const& dbname) {
  if (DatabaseFeature::DATABASE != nullptr) {
    return DatabaseFeature::DATABASE->lookupDatabase(dbname);
  }
  return nullptr;
}

std::vector<std::string> Databases::list(std::string const& user) {
  DatabaseFeature* databaseFeature =
      application_features::ApplicationServer::getFeature<DatabaseFeature>(
          "Database");
  if (databaseFeature == nullptr) {
    return std::vector<std::string>();
  }

  if (user.empty()) {
    if (ServerState::instance()->isCoordinator()) {
      ClusterInfo* ci = ClusterInfo::instance();
      return ci->databases(true);
    } else {
      // list of all databases
      return databaseFeature->getDatabaseNames();
    }
  } else {
    // slow path for user case
    return databaseFeature->getDatabaseNamesForUser(user);
  }
}

arangodb::Result Databases::info(TRI_vocbase_t* vocbase, VPackBuilder& result) {
  if (ServerState::instance()->isCoordinator()) {
    AgencyComm agency;
    AgencyCommResult commRes =
        agency.getValues("Plan/Databases/" + vocbase->name());
    if (!commRes.successful()) {
      // Error in communication, note that value not found is not an error
      LOG_TOPIC(TRACE, Logger::COMMUNICATION)
          << "rest database handler: no agency communication";
      return Result(commRes.errorCode(), commRes.errorMessage());
    }

    VPackSlice value = commRes.slice()[0].get<std::string>(
        {AgencyCommManager::path(), "Plan", "Databases", vocbase->name()});
    if (value.isObject() && value.hasKey("name")) {
      VPackValueLength l = 0;
      const char* name = value.get("name").getString(l);
      TRI_ASSERT(l > 0);

      VPackObjectBuilder b(&result);
      result.add("name", value.get("name"));
      if (value.get("id").isString()) {
        result.add("id", value.get("id"));
      } else if (value.get("id").isNumber()) {
        result.add("id", VPackValue(std::to_string(value.get("id").getUInt())));
      } else {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                       "unexpected type for 'id' attribute");
      }
      result.add("path", VPackValue("none"));
      result.add("isSystem", VPackValue(name[0] == '_'));
    }
  } else {
    VPackObjectBuilder b(&result);
    result.add("name", VPackValue(vocbase->name()));
    result.add("id", VPackValue(std::to_string(vocbase->id())));
    result.add("path", VPackValue(vocbase->path()));
    result.add("isSystem", VPackValue(vocbase->isSystem()));
  }
  return Result();
}

arangodb::Result Databases::create(std::string const& dbName,
                                   VPackSlice const& inUsers,
                                   VPackSlice const& inOptions) {
  auth::UserManager* um = AuthenticationFeature::instance()->userManager();
  ExecContext const* exec = ExecContext::CURRENT;
  if (exec != nullptr) {
    if (!exec->isAdminUser()) {
      return TRI_ERROR_FORBIDDEN;
    }
  }

  VPackSlice options = inOptions;
  if (options.isNone() || options.isNull()) {
    options = VPackSlice::emptyObjectSlice();
  } else if (!options.isObject()) {
    return Result(TRI_ERROR_HTTP_BAD_PARAMETER, "invalid options slice");
  }
  VPackSlice users = inUsers;
  if (users.isNone() || users.isNull()) {
    users = VPackSlice::emptyArraySlice();
  } else if (!users.isArray()) {
    return Result(TRI_ERROR_HTTP_BAD_PARAMETER, "invalid users slice");
  }

  VPackBuilder sanitizedUsers;
  sanitizedUsers.openArray();
  for (VPackSlice const& user : VPackArrayIterator(users)) {
    sanitizedUsers.openObject();
    if (!user.isObject()) {
      return Result(TRI_ERROR_HTTP_BAD_PARAMETER);
    }

    VPackSlice name;
    if (user.hasKey("username")) {
      name = user.get("username");
    } else if (user.hasKey("user")) {
      name = user.get("user");
    }
    if (!name.isString()) {  // empty names are silently ignored later
      return Result(TRI_ERROR_HTTP_BAD_PARAMETER);
    }
    sanitizedUsers.add("username", name);

    if (user.hasKey("passwd")) {
      VPackSlice passwd = user.get("passwd");
      if (!passwd.isString()) {
        return Result(TRI_ERROR_HTTP_BAD_PARAMETER);
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

  DatabaseFeature* databaseFeature = DatabaseFeature::DATABASE;
  if (databaseFeature == nullptr) {
    return Result(TRI_ERROR_INTERNAL);
  }

  UpgradeResult upgradeRes;
  if (ServerState::instance()->isCoordinator()) {
    if (!TRI_vocbase_t::IsAllowedName(false, arangodb::velocypack::StringRef(dbName))) {
      return Result(TRI_ERROR_ARANGO_DATABASE_NAME_INVALID);
    }

    uint64_t const id = ClusterInfo::instance()->uniqid();
    VPackBuilder builder;
    try {
      VPackObjectBuilder b(&builder);
      std::string const idString(basics::StringUtils::itoa(id));
      builder.add("id", VPackValue(idString));
      builder.add("name", VPackValue(dbName));
      builder.add("options", options);
      builder.add("coordinator", VPackValue(ServerState::instance()->getId()));
    } catch (VPackException const& e) {
      return Result(e.errorCode());
    }

    ClusterInfo* ci = ClusterInfo::instance();
    std::string errorMsg;

    int res =
        ci->createDatabaseCoordinator(dbName, builder.slice(), errorMsg, 120.0);
    if (res != TRI_ERROR_NO_ERROR) {
      return Result(res);
    }
    // database was created successfully in agency

    // now wait for heartbeat thread to create the database object
    TRI_vocbase_t* vocbase = nullptr;
    int tries = 0;
    while (++tries <= 6000) {
      vocbase = databaseFeature->useDatabase(id);
      if (vocbase != nullptr) {
        break;
      }
      // sleep
      std::this_thread::sleep_for(std::chrono::microseconds(10000));
    }

    if (vocbase == nullptr) {
      return Result(TRI_ERROR_INTERNAL, "unable to find database");
    }
    TRI_DEFER(vocbase->release());
    TRI_ASSERT(vocbase->id() == id);
    TRI_ASSERT(vocbase->name() == dbName);

    // we need to add the permissions before running the upgrade script
    if (ExecContext::CURRENT != nullptr && um != nullptr) {
      // ignore errors here Result r =
      um->updateUser(
          ExecContext::CURRENT->user(), [&](auth::User& entry) {
            entry.grantDatabase(dbName, auth::Level::RW);
            entry.grantCollection(dbName, "*", auth::Level::RW);
            return TRI_ERROR_NO_ERROR;
          });
    }

    TRI_ASSERT(sanitizedUsers.slice().isArray());
    upgradeRes = methods::Upgrade::createDB(*vocbase, sanitizedUsers.slice());
  } else { // Single, DBServer, Agency
    // options for database (currently only allows setting "id"
    // for testing purposes)
    TRI_voc_tick_t id = 0;
    if (options.hasKey("id")) {
      id = basics::VelocyPackHelper::stringUInt64(options, "id");
    }

    TRI_vocbase_t* vocbase = nullptr;
    int res = databaseFeature->createDatabase(id, dbName, vocbase);
    if (res != TRI_ERROR_NO_ERROR) {
      return Result(res);
    }
  
    TRI_ASSERT(vocbase != nullptr);
    TRI_ASSERT(!vocbase->isDangling());

    TRI_DEFER(vocbase->release());

    // we need to add the permissions before running the upgrade script
    if (ExecContext::CURRENT != nullptr && um != nullptr) {
      // ignore errors here Result r =
      um->updateUser(
          ExecContext::CURRENT->user(), [&](auth::User& entry) {
             entry.grantDatabase(dbName, auth::Level::RW);
             entry.grantCollection(dbName, "*", auth::Level::RW);
             return TRI_ERROR_NO_ERROR;
           });
    }

    upgradeRes = methods::Upgrade::createDB(*vocbase, sanitizedUsers.slice());
  }

  if (upgradeRes.fail()) {
    LOG_TOPIC(ERR, Logger::FIXME) << "Could not create database: "
    << upgradeRes.errorMessage();
    return upgradeRes;
  }

  // Entirely Foxx related:
  if (ServerState::instance()->isSingleServerOrCoordinator()) {
    try {
      auto* sysDbFeature = arangodb::application_features::ApplicationServer::getFeature<
        arangodb::SystemDatabaseFeature
      >();
      auto database = sysDbFeature->use();

      TRI_ExpireFoxxQueueDatabaseCache(database.get());
    } catch(...) {
      // it is of no real importance if cache invalidation fails, because
      // the cache entry has a ttl
    }
  }

  return Result();
}

namespace  {
  int dropDBCoordinator(std::string const& dbName) {
    // Arguments are already checked, there is exactly one argument
    DatabaseFeature* databaseFeature = DatabaseFeature::DATABASE;
    TRI_vocbase_t* vocbase = databaseFeature->useDatabase(dbName);
    if (vocbase == nullptr) {
      return TRI_ERROR_ARANGO_DATABASE_NOT_FOUND;
    }
    
    TRI_voc_tick_t const id = vocbase->id();
    vocbase->release();
    
    ClusterInfo* ci = ClusterInfo::instance();
    std::string errorMsg;
    
    int res = ci->dropDatabaseCoordinator(dbName, errorMsg, 120.0);
    
    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }
    
    // now wait for heartbeat thread to drop the database object
    int tries = 0;
    while (++tries <= 6000) {
      TRI_vocbase_t* vocbase = databaseFeature->useDatabase(id);
      
      if (vocbase == nullptr) {
        // object has vanished
        break;
      }
      
      vocbase->release();
      // sleep
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return TRI_ERROR_NO_ERROR;
  }
}

arangodb::Result Databases::drop(TRI_vocbase_t* systemVocbase,
                                 std::string const& dbName) {
  TRI_ASSERT(systemVocbase->isSystem());
  ExecContext const* exec = ExecContext::CURRENT;
  if (exec != nullptr) {
    if (exec->systemAuthLevel() != auth::Level::RW) {
      return TRI_ERROR_FORBIDDEN;
    }
  }

  int res;
  V8DealerFeature* dealer = V8DealerFeature::DEALER;
  if (dealer != nullptr && dealer->isEnabled()) {
    V8Context* v8ctx = V8DealerFeature::DEALER->enterContext(systemVocbase, true);
    if (v8ctx == nullptr) {
      return Result(TRI_ERROR_INTERNAL, "Could not get v8 context");
    }
    TRI_DEFER(V8DealerFeature::DEALER->exitContext(v8ctx));
    v8::Isolate* isolate = v8ctx->_isolate;
    v8::HandleScope scope(isolate);
    
    // clear collections in cache object
    TRI_ClearObjectCacheV8(isolate);
    
    if (ServerState::instance()->isCoordinator()) {
      // If we are a coordinator in a cluster, we have to behave differently:
      res = ::dropDBCoordinator(dbName);
    } else {
      res = DatabaseFeature::DATABASE->dropDatabase(dbName, false, true);
      
      if (res != TRI_ERROR_NO_ERROR) {
        return Result(res);
      }
      
      TRI_RemoveDatabaseTasksV8Dispatcher(dbName);
      // run the garbage collection in case the database held some objects which
      // can now be freed
      TRI_RunGarbageCollectionV8(isolate, 0.25);
      TRI_ExecuteJavaScriptString(isolate, isolate->GetCurrentContext(),
                                  TRI_V8_ASCII_STRING(isolate,
                                                      "require('internal').executeGlobalContextFunction('"
                                                      "reloadRouting')"),
                                  TRI_V8_ASCII_STRING(isolate, "reload routing"), false);
    }
  } else {
    if (ServerState::instance()->isCoordinator()) {
      // If we are a coordinator in a cluster, we have to behave differently:
      res = ::dropDBCoordinator(dbName);
    } else {
      res = DatabaseFeature::DATABASE->dropDatabase(dbName, false, true);;
    }
  }
  
  auth::UserManager* um = AuthenticationFeature::instance()->userManager();
  if (res == TRI_ERROR_NO_ERROR && um != nullptr) {
    return um->enumerateUsers([&](auth::User& entry) -> bool {
      return entry.removeDatabase(dbName);
    });
  }

  return res;
}
