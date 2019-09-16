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

#include "Databases.h"
#include "Basics/Common.h"

#include "Agency/AgencyComm.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ServerState.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/SystemDatabaseFeature.h"
#include "Utils/Events.h"
#include "Utils/ExecContext.h"
#include "V8/JavaScriptSecurityContext.h"
#include "V8/v8-utils.h"
#include "V8/v8-vpack.h"
#include "V8Server/V8Context.h"
#include "V8Server/V8DealerFeature.h"
#include "V8Server/v8-dispatcher.h"
#include "V8Server/v8-user-structures.h"
#include "VocBase/Methods/Upgrade.h"
#include "VocBase/vocbase.h"

#include <chrono>
#include <thread>

#include <v8.h>
#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::methods;
using namespace arangodb::velocypack;

Result CreateDatabaseInfo::load(std::string const& name, VPackSlice const& options,
                                VPackSlice const& users) {
  Result res;
  _id = 0;
  _name = name;

  if (!TRI_vocbase_t::IsAllowedName(false, arangodb::velocypack::StringRef(name))) {
    return Result(TRI_ERROR_ARANGO_DATABASE_NAME_INVALID);
  }

  res = sanitizeUsers(users, _users);
  if (!res.ok()) {
    return res;
  }
  _userSlice = _users.slice();

  res = sanitizeOptions(options, _options);
  if (!res.ok()) {
    return res;
  }

  // Obtain a unique id for the database to be created. Since this is different
  // on Coordinator vs Other, we have to have an if here to keep the other code
  // unified.
  if (ServerState::instance()->isCoordinator()) {
    _id = ClusterInfo::instance()->uniqid();
  } else {
    if (_options.slice().hasKey("id")) {
      _id = basics::VelocyPackHelper::stringUInt64(options, "id");
    } else {
      _id = 0;
    }
  }

  return Result();
};

Result CreateDatabaseInfo::buildSlice(VPackBuilder& builder) const {
  try {
    builder.openObject();
    std::string const idString(basics::StringUtils::itoa(_id));
    builder.add(StaticStrings::DatabaseId, VPackValue(idString));
    builder.add(StaticStrings::DatabaseName, VPackValue(_name));
    builder.add(StaticStrings::DatabaseOptions, _options.slice());
    // we intentionally do not close the object, because other functions,
    // for example in the cluster code, might want to add stuff.
  } catch (VPackException const& e) {
    return Result(e.errorCode());
  }
  return Result();
}

Result CreateDatabaseInfo::sanitizeUsers(VPackSlice const& users, VPackBuilder& sanitizedUsers) {
  if (users.isNone() || users.isNull()) {
    sanitizedUsers.openArray();
    sanitizedUsers.close();
    return Result();
  } else if (!users.isArray()) {
    events::CreateDatabase(_name, TRI_ERROR_HTTP_BAD_PARAMETER);
    return Result(TRI_ERROR_HTTP_BAD_PARAMETER, "invalid users slice");
  }

  sanitizedUsers.openArray();
  for (VPackSlice const& user : VPackArrayIterator(users)) {
    sanitizedUsers.openObject();
    if (!user.isObject()) {
      events::CreateDatabase(_name, TRI_ERROR_HTTP_BAD_PARAMETER);
      return Result(TRI_ERROR_HTTP_BAD_PARAMETER);
    }

    VPackSlice name;
    if (user.hasKey("username")) {
      name = user.get("username");
    } else if (user.hasKey("user")) {
      name = user.get("user");
    }
    if (!name.isString()) {  // empty names are silently ignored later
      events::CreateDatabase(_name, TRI_ERROR_HTTP_BAD_PARAMETER);
      return Result(TRI_ERROR_HTTP_BAD_PARAMETER);
    }
    sanitizedUsers.add("username", name);

    if (user.hasKey("passwd")) {
      VPackSlice passwd = user.get("passwd");
      if (!passwd.isString()) {
        events::CreateDatabase(_name, TRI_ERROR_HTTP_BAD_PARAMETER);
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
  TRI_ASSERT(sanitizedUsers.slice().isArray());
  return Result();
}

Result CreateDatabaseInfo::sanitizeOptions(VPackSlice const& options,
                                           VPackBuilder& sanitizedOptions) {
  if (options.isNone() || options.isNull()) {
    sanitizedOptions.openObject();
    sanitizedOptions.close();
    return Result();
  } else if (!options.isObject()) {
    events::CreateDatabase(_name, TRI_ERROR_HTTP_BAD_PARAMETER);
    return Result(TRI_ERROR_HTTP_BAD_PARAMETER, "invalid options slice");
  }
  sanitizedOptions.add(options);
  return Result();
}

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
    AgencyCommResult commRes = agency.getValues("Plan/Databases/" + vocbase->name());
    if (!commRes.successful()) {
      // Error in communication, note that value not found is not an error
      LOG_TOPIC("87642", TRACE, Logger::COMMUNICATION)
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

// Grant permissions on newly created database to current user
// to be able to run the upgrade script
arangodb::Result Databases::grantCurrentUser(CreateDatabaseInfo const& info, int64_t timeout) {
  auth::UserManager* um = AuthenticationFeature::instance()->userManager();

  Result res;

  if (um != nullptr) {
    ExecContext const& exec = ExecContext::current();
    // If the current user is empty (which happens if a Maintenance job
    // called us, or when authentication is off), granting rights
    // will fail. We hence ignore it here, but issue a warning below
    if (!exec.isSuperuser()) {
      auto const endTime = std::chrono::steady_clock::now() + std::chrono::seconds(timeout);
      while (true) {
        res = um->updateUser(exec.user(), [&](auth::User& entry) {
          entry.grantDatabase(info.getName(), auth::Level::RW);
          entry.grantCollection(info.getName(), "*", auth::Level::RW);
          return TRI_ERROR_NO_ERROR;
        });
        if (res.ok() || 
            !res.is(TRI_ERROR_ARANGO_CONFLICT) ||
            std::chrono::steady_clock::now() > endTime) {
          break;
        }

        if (application_features::ApplicationServer::isStopping()) {
          res.reset(TRI_ERROR_SHUTTING_DOWN);
          break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
      }
    } 

    LOG_TOPIC("2a4dd", DEBUG, Logger::FIXME)
      << "current ExecContext's user() is empty. "
      << "Database will be created without any user having permissions";
  }

  return res;
}

// Create database on cluster;
Result Databases::createCoordinator(CreateDatabaseInfo const& info) {
  TRI_ASSERT(ServerState::instance()->isCoordinator());

  // This operation enters the database as isBuilding into the agency
  // while the database is still building it is not visible.
  ClusterInfo* ci = ClusterInfo::instance();
  Result res = ci->createIsBuildingDatabaseCoordinator(info);

  // Even entering the database as building failed; This can happen
  // because a database with this name already exists, or because we could
  // not write to Plan/ in the agency
  if (!res.ok()) {
    events::CreateDatabase(info.getName(), res.errorNumber());
    return res;
  }

  auto failureGuard = scopeGuard([ci, info]() {
    LOG_TOPIC("8cc61", ERR, Logger::FIXME)
      << "Failed to create database '" << info.getName() << "', rolling back.";
    Result res = ci->cancelCreateDatabaseCoordinator(info);
    if (!res.ok()) {
      // this cannot happen since cancelCreateDatabaseCoordinator keeps retrying
      // indefinitely until the cancellation is either successful or the cluster
      // is shut down.
      LOG_TOPIC("92157", ERR, Logger::FIXME)
        << "Failed to rollback creation of database '" << info.getName() <<
        "'. Cleanup will happen through a supervision job.";
    }
  });

  res = grantCurrentUser(info, 5); 
  if (!res.ok()) {
    return res;
  }

  // This vocbase is needed for the call to methods::Upgrade::createDB, but
  // is just a placeholder
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 0, info.getName());

  // Now create *all* system collections for the database,
  // if any of these fail, database creation is considered unsuccessful

  UpgradeResult upgradeRes = methods::Upgrade::createDB(vocbase, info.getUsers());
  failureGuard.cancel();

  // If the creation of system collections was successful,
  // make the database visible, otherwise clean up what we can.
  if (upgradeRes.ok()) {
    return ci->createFinalizeDatabaseCoordinator(info);
  } 
    
  // We leave this handling here to be able to capture
  // error messages and return
  // Cleanup entries in agency.
  res = ci->cancelCreateDatabaseCoordinator(info);
  if (!res.ok()) {
    // this should never happen as cancelCreateDatabaseCoordinator keeps retrying
    // until either cancellation is successful or the cluster is shut down.
    return res;
  } 
  
  return std::move(upgradeRes.result());
}

// Create a database on SingleServer, DBServer,
Result Databases::createOther(CreateDatabaseInfo const& info) {
  // Without the database feature, we can't create a database
  DatabaseFeature* databaseFeature = DatabaseFeature::DATABASE;
  if (databaseFeature == nullptr) {
    events::CreateDatabase(info.getName(), TRI_ERROR_INTERNAL);
    return {TRI_ERROR_INTERNAL};
  }

  TRI_vocbase_t* vocbase = nullptr;
  Result createResult = databaseFeature->createDatabase(info.getId(), info.getName(), vocbase);
  if (createResult.fail()) {
    return createResult;
  }

  TRI_ASSERT(vocbase != nullptr);
  TRI_ASSERT(!vocbase->isDangling());

  TRI_DEFER(vocbase->release());

  Result res = grantCurrentUser(info, 10);
  if (!res.ok()) {
    return res;
  }

  UpgradeResult upgradeRes = methods::Upgrade::createDB(*vocbase, info.getUsers());

  return std::move(upgradeRes.result());
}

arangodb::Result Databases::create(std::string const& dbName, VPackSlice const& users,
                                   VPackSlice const& options) {
  arangodb::Result res;

  // Only admin users are permitted to create databases
  ExecContext const& exec = ExecContext::current();
  if (!exec.isAdminUser()) {
    events::CreateDatabase(dbName, TRI_ERROR_FORBIDDEN);
    return Result(TRI_ERROR_FORBIDDEN);
  }

  // Encapsulate and sanitize the input
  // TODO: maybe this should just be a function that produces
  //       a struct to avoid the try/catch
  //       or the object could have a .valid() method?
  CreateDatabaseInfo createInfo;
  res = createInfo.load(dbName, options, users);

  if (!res.ok()) {
    LOG_TOPIC("15580", ERR, Logger::FIXME)
      << "Could not create database: " << res.errorMessage();
    events::CreateDatabase(dbName, res.errorNumber());
    return res;
  }

  if (ServerState::instance()->isCoordinator()) {
    res = createCoordinator(createInfo);
  } else {  // Single, DBServer, Agency
    res = createOther(createInfo);
  }

  if (res.fail()) {
    LOG_TOPIC("1964a", ERR, Logger::FIXME)
        << "Could not create database: " << res.errorMessage();
    return res;
  }

  // Invalidate Foxx Queue database cache. We do not care if this fails,
  // because the cache entry has a TTL
  if (ServerState::instance()->isSingleServerOrCoordinator()) {
    try {
      auto* sysDbFeature =
          arangodb::application_features::ApplicationServer::getFeature<arangodb::SystemDatabaseFeature>();
      auto database = sysDbFeature->use();

      TRI_ExpireFoxxQueueDatabaseCache(database.get());
    } catch (...) {
    }
  }

  return Result();
}

namespace {
int dropDBCoordinator(std::string const& dbName) {
  // Arguments are already checked, there is exactly one argument
  DatabaseFeature* databaseFeature = DatabaseFeature::DATABASE;
  TRI_vocbase_t* vocbase = databaseFeature->useDatabase(dbName);

  if (vocbase == nullptr) {
    events::DropDatabase(dbName, TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
    return TRI_ERROR_ARANGO_DATABASE_NOT_FOUND;
  }

  TRI_voc_tick_t const id = vocbase->id();

  vocbase->release();

  ClusterInfo* ci = ClusterInfo::instance();
  auto res = ci->dropDatabaseCoordinator(dbName, 120.0);

  if (!res.ok()) {
    events::DropDatabase(dbName, res.errorNumber());
    return res.errorNumber();
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

const std::string dropError = "Error when dropping Datbase";
}  // namespace

arangodb::Result Databases::drop(TRI_vocbase_t* systemVocbase, std::string const& dbName) {
  TRI_ASSERT(systemVocbase->isSystem());
  ExecContext const& exec = ExecContext::current();
  if (exec.systemAuthLevel() != auth::Level::RW) {
    events::DropDatabase(dbName, TRI_ERROR_FORBIDDEN);
    return TRI_ERROR_FORBIDDEN;
  }

  Result res;
  V8DealerFeature* dealer = V8DealerFeature::DEALER;
  if (dealer != nullptr && dealer->isEnabled()) {
    try {
      JavaScriptSecurityContext securityContext =
          JavaScriptSecurityContext::createInternalContext();

      V8ContextGuard guard(systemVocbase, securityContext);
      v8::Isolate* isolate = guard.isolate();

      v8::HandleScope scope(isolate);

      // clear collections in cache object
      TRI_ClearObjectCacheV8(isolate);

      if (ServerState::instance()->isCoordinator()) {
        // If we are a coordinator in a cluster, we have to behave differently:
        res = ::dropDBCoordinator(dbName);
      } else {
        res = DatabaseFeature::DATABASE->dropDatabase(dbName, false, true);

        if (res.fail()) {
          events::DropDatabase(dbName, res.errorNumber());
          return Result(res);
        }

        TRI_RemoveDatabaseTasksV8Dispatcher(dbName);
        // run the garbage collection in case the database held some objects
        // which can now be freed
        TRI_RunGarbageCollectionV8(isolate, 0.25);
        V8DealerFeature::DEALER->addGlobalContextMethod("reloadRouting");
      }
    } catch (arangodb::basics::Exception const& ex) {
      events::DropDatabase(dbName, TRI_ERROR_INTERNAL);
      return Result(ex.code(), dropError + ex.message());
    } catch (std::exception const& ex) {
      events::DropDatabase(dbName, TRI_ERROR_INTERNAL);
      return Result(TRI_ERROR_INTERNAL, dropError + ex.what());
    } catch (...) {
      events::DropDatabase(dbName, TRI_ERROR_INTERNAL);
      return Result(TRI_ERROR_INTERNAL, dropError);
    }
  } else {
    if (ServerState::instance()->isCoordinator()) {
      // If we are a coordinator in a cluster, we have to behave differently:
      res = ::dropDBCoordinator(dbName);
    } else {
      res = DatabaseFeature::DATABASE->dropDatabase(dbName, false, true);
    }
  }

  auth::UserManager* um = AuthenticationFeature::instance()->userManager();
  if (res.ok() && um != nullptr) {
    auto cb = [&](auth::User& entry) -> bool {
      return entry.removeDatabase(dbName);
    };
    res = um->enumerateUsers(cb, /*retryOnConflict*/ true);
  }

  return res;
}
