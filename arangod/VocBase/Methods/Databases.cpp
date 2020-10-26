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
#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/AgencyCache.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ServerState.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/SystemDatabaseFeature.h"
#include "Sharding/ShardingInfo.h"
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

TRI_vocbase_t* Databases::lookup(std::string const& dbname) {
  if (DatabaseFeature::DATABASE != nullptr) {
    return DatabaseFeature::DATABASE->lookupDatabase(dbname);
  }
  return nullptr;
}

std::vector<std::string> Databases::list(application_features::ApplicationServer& server,
                                         std::string const& user) {
  if (!server.hasFeature<DatabaseFeature>()) {
    return std::vector<std::string>();
  }
  DatabaseFeature& databaseFeature = server.getFeature<DatabaseFeature>();

  if (user.empty()) {
    if (ServerState::instance()->isCoordinator()) {
      ClusterInfo& ci = server.getFeature<ClusterFeature>().clusterInfo();
      return ci.databases(false);
    } else {
      // list of all databases
      return databaseFeature.getDatabaseNames();
    }
  } else {
    // slow path for user case
    return databaseFeature.getDatabaseNamesForUser(user);
  }
}

arangodb::Result Databases::info(TRI_vocbase_t* vocbase, VPackBuilder& result) {
  if (ServerState::instance()->isCoordinator()) {

    auto& cache = vocbase->server().getFeature<ClusterFeature>().agencyCache();
    auto [acb,idx] = cache.read(std::vector<std::string>{
        AgencyCommHelper::path("Plan/Databases/" + vocbase->name())});
    auto res = acb->slice();

    if (!res.isArray()) {
      // Error in communication, note that value not found is not an error
      LOG_TOPIC("87642", TRACE, Logger::COMMUNICATION)
        << "rest database handler: no agency communication";
      return Result(TRI_ERROR_HTTP_SERVICE_UNAVAILABLE, "agency cache empty");
    }

    VPackSlice value = res[0].get<std::string>(
        {AgencyCommHelper::path(), "Plan", "Databases", vocbase->name()});
    if (value.isObject() && value.hasKey(StaticStrings::DataSourceName)) {
      std::string name = value.get(StaticStrings::DataSourceName).copyString();

      VPackObjectBuilder b(&result);
      result.add(StaticStrings::DataSourceName, VPackValue(name));
      VPackSlice s = value.get(StaticStrings::DataSourceId);
      if (s.isString()) {
        result.add(StaticStrings::DataSourceId, s);
      } else if (s.isNumber()) {
        result.add(StaticStrings::DataSourceId, VPackValue(std::to_string(s.getUInt())));
      } else {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                       "unexpected type for 'id' attribute");
      }
      result.add(StaticStrings::DataSourceSystem, VPackValue(TRI_vocbase_t::IsSystemName(name)));
      result.add("path", VPackValue("none"));
    }
  } else {
    VPackObjectBuilder b(&result);
    result.add(StaticStrings::DataSourceName, VPackValue(vocbase->name()));
    result.add(StaticStrings::DataSourceId, VPackValue(std::to_string(vocbase->id())));
    result.add(StaticStrings::DataSourceSystem, VPackValue(vocbase->isSystem()));
    result.add("path", VPackValue(vocbase->path()));
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
    if (!exec.isAdminUser()) {
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

        if (info.server().isStopping()) {
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

  if (!TRI_vocbase_t::IsAllowedName(/*_isSystemDB*/ false, arangodb::velocypack::StringRef(info.getName()))) {
    return Result(TRI_ERROR_ARANGO_DATABASE_NAME_INVALID);
  }

  LOG_TOPIC("56372", DEBUG, Logger::CLUSTER) << "createDatabase on coordinator: Starting, name: " << info.getName();

  // This operation enters the database as isBuilding into the agency
  // while the database is still building it is not visible.
  ClusterInfo& ci = info.server().getFeature<ClusterFeature>().clusterInfo();
  Result res = ci.createIsBuildingDatabaseCoordinator(info);

  LOG_TOPIC("54322", DEBUG, Logger::CLUSTER) << "createDatabase on coordinator: have created isBuilding database, name: " << info.getName();

  // Even entering the database as building failed; This can happen
  // because a database with this name already exists, or because we could
  // not write to Plan/ in the agency
  if (!res.ok()) {
    events::CreateDatabase(info.getName(), res.errorNumber());
    return res;
  }

  auto failureGuard = scopeGuard([&ci, info]() {
    LOG_TOPIC("8cc61", ERR, Logger::FIXME)
      << "Failed to create database '" << info.getName() << "', rolling back.";
    Result res = ci.cancelCreateDatabaseCoordinator(info);
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

  LOG_TOPIC("54323", DEBUG, Logger::CLUSTER) << "createDatabase on coordinator: have granted current user for database: " << info.getName();

  // This vocbase is needed for the call to methods::Upgrade::createDB, but
  // is just a placeholder
  CreateDatabaseInfo tempInfo = info;
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, std::move(tempInfo));

  // Now create *all* system collections for the database,
  // if any of these fail, database creation is considered unsuccessful

  VPackBuilder userBuilder;
  info.UsersToVelocyPack(userBuilder);
  UpgradeResult upgradeRes = methods::Upgrade::createDB(vocbase, userBuilder.slice());
  failureGuard.cancel();

  LOG_TOPIC("54324", DEBUG, Logger::CLUSTER) << "createDatabase on coordinator: have run Upgrade::createDB for database: " << info.getName();

  // If the creation of system collections was successful,
  // make the database visible, otherwise clean up what we can.
  if (upgradeRes.ok()) {
    LOG_TOPIC("54325", DEBUG, Logger::CLUSTER) << "createDatabase on coordinator: finished, database: " << info.getName();
    return ci.createFinalizeDatabaseCoordinator(info);
  }

  LOG_TOPIC("24653", DEBUG, Logger::CLUSTER) << "createDatabase on coordinator: cancelling, database: " << info.getName();

  // We leave this handling here to be able to capture
  // error messages and return
  // Cleanup entries in agency.
  res = ci.cancelCreateDatabaseCoordinator(info);
  LOG_TOPIC("54327", DEBUG, Logger::CLUSTER)
      << "createDatabase on coordinator: cancelled, database: " << info.getName()
      << " result: " << res.errorNumber();
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
  if (!info.server().hasFeature<DatabaseFeature>()) {
    events::CreateDatabase(info.getName(), TRI_ERROR_INTERNAL);
    return {TRI_ERROR_INTERNAL};
  }
  DatabaseFeature& databaseFeature = info.server().getFeature<DatabaseFeature>();

  TRI_vocbase_t* vocbase = nullptr;
  auto tempInfo = info;
  Result createResult = databaseFeature.createDatabase(std::move(tempInfo), vocbase);
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

  VPackBuilder userBuilder;
  info.UsersToVelocyPack(userBuilder);
  UpgradeResult upgradeRes = methods::Upgrade::createDB(*vocbase, userBuilder.slice());

  return std::move(upgradeRes.result());
}

arangodb::Result Databases::create(application_features::ApplicationServer& server,
                                   std::string const& dbName, VPackSlice const& users,
                                   VPackSlice const& options) {
  // Only admin users are permitted to create databases
  ExecContext const& exec = ExecContext::current();

  if (!exec.isAdminUser() || (ServerState::readOnly() && !exec.isSuperuser())) {
    events::CreateDatabase(dbName, TRI_ERROR_FORBIDDEN);
    return Result(TRI_ERROR_FORBIDDEN);
  }

  CreateDatabaseInfo createInfo(server);
  arangodb::Result res = createInfo.load(dbName, options, users);

  if (!res.ok()) {
    events::CreateDatabase(dbName, res.errorNumber());
    return res;
  }

  if (ServerState::instance()->isCoordinator() /* REVIEW! && !localDatabase*/) {
    if (!createInfo.validId()) {
      auto& clusterInfo = server.getFeature<ClusterFeature>().clusterInfo();
      createInfo.setId(clusterInfo.uniqid());
    }

    res = ShardingInfo::validateShardsAndReplicationFactor(options, server, true);
    if (res.ok()) {
      res = createCoordinator(createInfo);
    }

  } else {  // Single, DBServer, Agency
    if (!createInfo.validId()) {
      createInfo.setId(TRI_NewTickServer());
    }
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
      auto& sysDbFeature = server.getFeature<arangodb::SystemDatabaseFeature>();
      auto database = sysDbFeature.use();

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

  ClusterInfo& ci = vocbase->server().getFeature<ClusterFeature>().clusterInfo();
  auto res = ci.dropDatabaseCoordinator(dbName, 120.0);

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

const std::string dropError = "Error when dropping database";
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
      JavaScriptSecurityContext securityContext = JavaScriptSecurityContext::createInternalContext();
      
      v8::Isolate* isolate = v8::Isolate::GetCurrent();
      V8ConditionalContextGuard guard(res, isolate, systemVocbase, securityContext);

      if (res.fail()) {
        return res;
      }

      v8::HandleScope scope(isolate);

      // clear collections in cache object
      TRI_ClearObjectCacheV8(isolate);

      if (ServerState::instance()->isCoordinator()) {
        // If we are a coordinator in a cluster, we have to behave differently:
        res = ::dropDBCoordinator(dbName);
      } else {
        res = DatabaseFeature::DATABASE->dropDatabase(dbName, true);

        if (res.fail()) {
          events::DropDatabase(dbName, res.errorNumber());
          return Result(res);
        }

        TRI_RemoveDatabaseTasksV8Dispatcher(dbName);
        // run the garbage collection in case the database held some objects
        // which can now be freed
        TRI_RunGarbageCollectionV8(isolate, 0.25);
        dealer->addGlobalContextMethod("reloadRouting");
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
      res = DatabaseFeature::DATABASE->dropDatabase(dbName, true);
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
