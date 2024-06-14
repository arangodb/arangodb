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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include "UpgradeFeature.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Auth/UserManager.h"
#include "Basics/ScopeGuard.h"
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
#include "ProgramOptions/Section.h"
#include "Replication/ReplicationFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/InitDatabaseFeature.h"
#include "RestServer/RestartAction.h"
#include "VocBase/Methods/Upgrade.h"
#include "VocBase/vocbase.h"

using namespace arangodb::application_features;
using namespace arangodb::basics;
using namespace arangodb::options;

namespace arangodb {

UpgradeFeature::UpgradeFeature(Server& server, int* result,
                               std::span<const size_t> nonServerFeatures)
    : ArangodFeature{server, *this},
      _upgrade(false),
      _upgradeCheck(true),
      _result(result),
      _nonServerFeatures(nonServerFeatures) {
  setOptional(false);
  startsAfter<AqlFeaturePhase>();
}

void UpgradeFeature::addTask(methods::Upgrade::Task&& task) {
  _tasks.push_back(std::move(task));
}

void UpgradeFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addOldOption("upgrade", "database.auto-upgrade");

  options
      ->addOption("--database.auto-upgrade",
                  "Perform a database upgrade if necessary.",
                  new BooleanParameter(&_upgrade))
      .setLongDescription(R"(If you specify this option, then the server
performs a database upgrade instead of starting normally.

A database upgrade first compares the version number stored in the `VERSION`
file in the database directory with the current server version.

If the version number found in the database directory is higher than that of the
server, the server considers this is an unintentional downgrade and warns about
this. Using the server in these conditions is neither recommended nor supported.

If the version number found in the database directory is lower than that of the
server, the server checks whether there are any upgrade tasks to perform.
It then executes all required upgrade tasks and prints the status. If one of the
upgrade tasks fails, the server exits with an error. Re-starting the server with
the upgrade option again triggers the upgrade check and execution until the
problem is fixed.

Whether or not you specify this option, the server always perform a version
check on startup. If you running the server with a non-matching version number
in the `VERSION` file, the server refuses to start.)");

  options->addOption(
      "--database.upgrade-check", "Skip the database upgrade if set to false.",
      new BooleanParameter(&_upgradeCheck),
      arangodb::options::makeDefaultFlags(arangodb::options::Flags::Uncommon));
}

static int upgradeRestart() {
  unsetenv(StaticStrings::UpgradeEnvName.c_str());
  return 0;
}

void UpgradeFeature::validateOptions(std::shared_ptr<ProgramOptions> options) {
  // The following environment variable is another way to run a database
  // upgrade. If the environment variable is set, the system does a database
  // upgrade and then restarts itself without the environment variable.
  // This is used in hotbackup if a restore to a backup happens which is from
  // an older database version. The restore process sets the environment
  // variable at runtime and then does a restore. After the restart (with
  // the old data) the database upgrade is run and another restart is
  // happening afterwards with the environment variable being cleared.
  char* upgrade = getenv(StaticStrings::UpgradeEnvName.c_str());
  if (upgrade != nullptr) {
    _upgrade = true;
    restartAction = new std::function<int()>();
    *restartAction = upgradeRestart;
    LOG_TOPIC("fdeae", INFO, Logger::STARTUP)
        << "Detected environment variable " << StaticStrings::UpgradeEnvName
        << " with value " << upgrade
        << " will perform database auto-upgrade and immediately restart.";
  }
  if (_upgrade && !_upgradeCheck) {
    LOG_TOPIC("47698", FATAL, arangodb::Logger::FIXME)
        << "cannot specify both '--database.auto-upgrade true' and "
           "'--database.upgrade-check false'";
    FATAL_ERROR_EXIT_CODE(TRI_EXIT_INVALID_OPTION_VALUE);
  }

  if (!_upgrade) {
    LOG_TOPIC("ed226", TRACE, arangodb::Logger::FIXME)
        << "executing upgrade check: not disabling server features";
    return;
  }

  LOG_TOPIC("23525", INFO, arangodb::Logger::FIXME)
      << "executing upgrade procedure: disabling server features";

  // if we run the upgrade, we need to disable a few features that may get
  // in the way...
  if (ServerState::instance()->isCoordinator()) {
    auto disableDaemonAndSupervisor = [&]() {
      if constexpr (Server::contains<DaemonFeature>()) {
        server().forceDisableFeatures(std::array{Server::id<DaemonFeature>()});
      }
      if constexpr (Server::contains<SupervisorFeature>()) {
        server().forceDisableFeatures(
            std::array{Server::id<SupervisorFeature>()});
      }
    };

    server().forceDisableFeatures(std::array{Server::id<GreetingsFeature>()});
    disableDaemonAndSupervisor();
  } else {
    server().forceDisableFeatures(_nonServerFeatures);
    server().forceDisableFeatures(std::array{
        Server::id<BootstrapFeature>(), Server::id<HttpEndpointProvider>()});
  }

  ReplicationFeature& replicationFeature =
      server().getFeature<ReplicationFeature>();
  replicationFeature.disableReplicationApplier();

  DatabaseFeature& database = server().getFeature<DatabaseFeature>();
  database.enableUpgrade();

#ifdef USE_ENTERPRISE
  HotBackupFeature& hotBackupFeature = server().getFeature<HotBackupFeature>();
  hotBackupFeature.forceDisable();
#endif
}

void UpgradeFeature::prepare() {
  // need to register tasks before creating any database
  methods::Upgrade::registerTasks(*this);
}

void UpgradeFeature::start() {
  auto& init = server().getFeature<InitDatabaseFeature>();

  // upgrade the database
  if (_upgradeCheck) {
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
          Result res = um->updateUser("root", [&](auth::User& user) {
            user.updatePassword(init.defaultPassword());
            return TRI_ERROR_NO_ERROR;
          });
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

  // and force shutdown
  if (_upgrade || init.isInitDatabase() || init.restoreAdmin()) {
    if (init.isInitDatabase()) {
      *_result = EXIT_SUCCESS;
    }

    if (!ServerState::instance()->isCoordinator() || !_upgrade) {
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

    auto res =
        methods::Upgrade::startup(*vocbase, _upgrade, ignoreDatafileErrors);

    if (res.fail()) {
      std::string_view typeName = "initialization";
      int exitCode = TRI_EXIT_FAILED;

      if (res.type == methods::VersionResult::UPGRADE_NEEDED) {
        typeName = "upgrade";  // an upgrade failed or is required

        if (!_upgrade) {
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

  if (_upgrade) {
    *_result = EXIT_SUCCESS;
    LOG_TOPIC("0de5e", INFO, arangodb::Logger::FIXME)
        << "database upgrade passed";
  }

  // and return from the context
  LOG_TOPIC("01a03", TRACE, arangodb::Logger::FIXME)
      << "finished database init/upgrade";
}

}  // namespace arangodb
