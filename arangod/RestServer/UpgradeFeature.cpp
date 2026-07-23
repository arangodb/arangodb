////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
////////////////////////////////////////////////////////////////////////////////

#include "UpgradeFeature.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "ApplicationFeatures/GreetingsFeature.h"
#include "ApplicationFeatures/HttpEndpointProvider.h"
#include "Auth/UserManager.h"
#include "FeaturePhases/AqlFeaturePhase.h"
#include "RestServer/DaemonFeature.h"
#include "RestServer/SupervisorFeature.h"
#include "Basics/StaticStrings.h"
#include "Basics/application-exit.h"
#include "Basics/exitcodes.h"
#include "Cluster/ServerState.h"
#ifdef USE_ENTERPRISE
#include "Enterprise/StorageEngine/HotBackupFeature.h"
#endif
#include "GeneralServer/AuthenticationFeature.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "ProgramOptions/ProgramOptions.h"
#include "RestServer/UpgradeOptionsProvider.h"
#include "Replication/ReplicationFeature.h"
#include "RestServer/BootstrapFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/InitDatabaseFeature.h"
#include "RestServer/RestartAction.h"
#include "StorageEngine/StorageEngine.h"
#include "VocBase/Methods/Upgrade.h"
#include "VocBase/vocbase.h"

using namespace arangodb::application_features;
using namespace arangodb::basics;
using namespace arangodb::options;

namespace arangodb {

UpgradeFeature::UpgradeFeature(
    ApplicationServer& server, int* result,
    std::span<const std::type_index> nonServerFeatures)
    : UpgradeFeature(server, result, nonServerFeatures,
                     UpgradeFeatureOptions{}) {}

UpgradeFeature::UpgradeFeature(
    ApplicationServer& server, int* result,
    std::span<const std::type_index> nonServerFeatures,
    UpgradeFeatureOptions options)
    : ApplicationFeature{server, *this},
      _options(std::move(options)),
      _result(result),
      _nonServerFeatures(nonServerFeatures) {
  setOptional(false);
  startsAfter<AqlFeaturePhase>();

  if (!_options.upgrade) {
    LOG_TOPIC("ed226", TRACE, arangodb::Logger::FIXME)
        << "executing upgrade check: not disabling server features";
    return;
  }
  LOG_TOPIC("23525", INFO, arangodb::Logger::FIXME)
      << "executing upgrade procedure: disabling server features";

  // if we run the upgrade, we need to disable a few features that may get
  // in the way...
  if (ServerState::instance()->isCoordinator()) {
#ifdef ARANGODB_HAVE_FORK
    server.forceDisableFeatures<DaemonFeature>();
    server.forceDisableFeatures<SupervisorFeature>();
#endif
    std::array greetingsFeature{std::type_index(typeid(GreetingsFeature))};
    server.forceDisableFeatures(greetingsFeature);
  } else {
    server.forceDisableFeatures(_nonServerFeatures);
    std::array bootstrapFeatures{std::type_index(typeid(BootstrapFeature)),
                                 std::type_index(typeid(HttpEndpointProvider))};
    server.forceDisableFeatures(bootstrapFeatures);
  }
  server.getFeature<ReplicationFeature>().disableReplicationApplier();
  server.getFeature<DatabaseFeature>().enableUpgrade();
#ifdef USE_ENTERPRISE
  server.getFeature<HotBackupFeature>().forceDisable();
#endif
}

void UpgradeFeature::addTask(methods::Upgrade::Task&& task) {
  _tasks.push_back(std::move(task));
}

void UpgradeFeature::prepare() {
  // need to register tasks before creating any database
  methods::Upgrade::registerTasks(*this);
}

void UpgradeFeature::start() {
  auto& init = server().getFeature<InitDatabaseFeature>();

  // upgrade the database
  if (_options.upgradeCheck) {
    if (!ServerState::instance()->isCoordinator()) {
      // no need to run local upgrades in the coordinator
      upgradeLocalDatabase();
    }

    auth::UserManager* um =
        server().getFeature<AuthenticationFeature>().userManager();

    if (um != nullptr) {
      if (!ServerState::instance()->isCoordinator() && !init.restoreAdmin() &&
          !init.defaultPassword().empty()) {
        // this method sets the root password in case on non-coordinators.
        // on coordinators, we cannot execute it here, because the _users
        // collection is not yet present.
        // for coordinators, the default password will be installed by the
        // BootstrapFeature later.
        Result res = catchToResult([&]() {
          if (ServerState::instance()->isSingleServer()) {
            um->loadUserCacheAndStartUpdateThread();
          }
          Result res = um->updateUser(
              "root",
              [&](auth::User& user) {
                user.updatePassword(init.defaultPassword());
                return TRI_ERROR_NO_ERROR;
              },
              auth::UserManager::RetryOnConflict::Yes);
          if (res.is(TRI_ERROR_USER_NOT_FOUND)) {
            VPackSlice extras = VPackSlice::noneSlice();
            res = um->storeUser(false, "root", init.defaultPassword(), true,
                                extras);
          }
          return res;
        });
        if (res.fail()) {
          LOG_TOPIC("ce6bf", ERR, arangodb::Logger::FIXME)
              << "failed to set default password: " << res.errorMessage();
          *_result = EXIT_FAILURE;
        }
      }
    }

    // change admin user
    if (init.restoreAdmin() &&
        ServerState::instance()->isSingleServerOrCoordinator()) {
      um->loadUserCacheAndStartUpdateThread();
      Result res = um->removeAllUsers();
      if (res.fail()) {
        LOG_TOPIC("70922", ERR, arangodb::Logger::FIXME)
            << "failed to clear users: " << res.errorMessage();
        *_result = EXIT_FAILURE;
        return;
      }

      VPackSlice extras = VPackSlice::noneSlice();
      res = um->storeUser(true, "root", init.defaultPassword(), true, extras);
      if (res.is(TRI_ERROR_USER_NOT_FOUND)) {
        res =
            um->storeUser(false, "root", init.defaultPassword(), true, extras);
      }

      if (res.fail()) {
        LOG_TOPIC("e9637", ERR, arangodb::Logger::FIXME)
            << "failed to create root user: " << res.errorMessage();
        *_result = EXIT_FAILURE;
        return;
      }
      auto oldLevel = arangodb::Logger::FIXME.level();
      arangodb::Logger::FIXME.setLogLevel(arangodb::LogLevel::INFO);
      LOG_TOPIC("95cab", INFO, arangodb::Logger::FIXME) << "Password changed.";
      arangodb::Logger::FIXME.setLogLevel(oldLevel);
      *_result = EXIT_SUCCESS;
    }
  }

  // perform full compaction if requested
  if (_options.upgrade && _options.upgradeFullCompaction &&
      !ServerState::instance()->isCoordinator()) {
    Result res = catchToResult([&]() { return performFullCompaction(); });
    if (res.fail()) {
      LOG_TOPIC("e8f46", FATAL, arangodb::Logger::ENGINES)
          << "full RocksDB compaction after upgrade failed: "
          << "errorNumber: " << res.errorNumber()
          << ", message: " << res.errorMessage();
      *_result = TRI_EXIT_FULL_COMPACTION_FAILED;
      server().beginShutdown();
      return;
    }
  }

  // and force shutdown
  if (_options.upgrade || init.isInitDatabase() || init.restoreAdmin()) {
    if (init.isInitDatabase()) {
      *_result = EXIT_SUCCESS;
    }

    if (!ServerState::instance()->isCoordinator() || !_options.upgrade) {
      LOG_TOPIC("7da27", INFO, arangodb::Logger::STARTUP)
          << "server will now shut down due to upgrade, database "
             "initialization "
             "or admin restoration.";

      // in the non-coordinator case, we are already done now and will shut
      // down. in the coordinator case, the actual upgrade is performed by the
      // ClusterUpgradeFeature, which is way later in the startup sequence.
      server().beginShutdown();
    }
  }
}

void UpgradeFeature::upgradeLocalDatabase() {
  LOG_TOPIC("05dff", TRACE, arangodb::Logger::FIXME)
      << "starting database init/upgrade";

  DatabaseFeature& databaseFeature = server().getFeature<DatabaseFeature>();

  bool ignoreDatafileErrors = databaseFeature.ignoreDatafileErrors();

  for (auto& name : databaseFeature.getDatabaseNames()) {
    auto vocbase = databaseFeature.useDatabase(name);

    // in this phase, all databases returned by getDatabaseNames() should
    // still be present and shouldn't be deleted concurrently
    TRI_ASSERT(vocbase != nullptr);

    if (vocbase == nullptr) {
      continue;
    }

    auto res = methods::Upgrade::startup(*vocbase, _options.upgrade,
                                         ignoreDatafileErrors);

    if (res.fail()) {
      std::string_view typeName = "initialization";
      int exitCode = TRI_EXIT_FAILED;

      if (res.type == methods::VersionResult::UPGRADE_NEEDED) {
        typeName = "upgrade";  // an upgrade failed or is required

        if (!_options.upgrade) {
          exitCode = TRI_EXIT_UPGRADE_REQUIRED;
          LOG_TOPIC("1c156", ERR, arangodb::Logger::FIXME)
              << "Database '" << vocbase->name() << "' needs upgrade. "
              << "Please start the server with --database.auto-upgrade";
        } else {
          exitCode = TRI_EXIT_UPGRADE_FAILED;
        }
      } else if (res.type == methods::VersionResult::DOWNGRADE_NEEDED) {
        exitCode = TRI_EXIT_DOWNGRADE_REQUIRED;
      } else if (res.type ==
                     methods::VersionResult::CANNOT_PARSE_VERSION_FILE ||
                 res.type == methods::VersionResult::CANNOT_READ_VERSION_FILE) {
        exitCode = TRI_EXIT_VERSION_CHECK_FAILED;
      }

      LOG_TOPIC("2eb08", FATAL, arangodb::Logger::FIXME)
          << "Database '" << vocbase->name() << "' " << typeName << " failed ("
          << res.errorMessage() << "). "
          << "Please inspect the logs from the " << typeName << " procedure"
          << " and try starting the server again.";

      FATAL_ERROR_EXIT_CODE(exitCode);
    }
  }

  if (_options.upgrade) {
    *_result = EXIT_SUCCESS;
    LOG_TOPIC("0de5e", INFO, arangodb::Logger::FIXME)
        << "database upgrade passed";
  }

  // and return from the context
  LOG_TOPIC("01a03", TRACE, arangodb::Logger::FIXME)
      << "finished database init/upgrade";
}

Result UpgradeFeature::performFullCompaction() {
  LOG_TOPIC("e8f45", INFO, arangodb::Logger::ENGINES)
      << "starting full RocksDB compaction after upgrade";

  StorageEngine& engine = server().getFeature<DatabaseFeature>().engine();

  // Perform full compaction with both changeLevel and compactBottomMostLevel
  // enabled This matches the behavior of the /_admin/compact API with
  // bottomMost=true and changeLevels=true
  Result res = engine.compactAll(true, true);
  if (res.fail()) {
    return res;
  }

  LOG_TOPIC("e8f47", INFO, arangodb::Logger::ENGINES)
      << "full RocksDB compaction after upgrade completed successfully";
  return {};
}

}  // namespace arangodb
