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

#include "UpgradeFeature.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "ApplicationFeatures/DaemonFeature.h"
#include "ApplicationFeatures/GreetingsFeature.h"
#include "ApplicationFeatures/HttpEndpointProvider.h"
#include "ApplicationFeatures/SupervisorFeature.h"
#include "Basics/ScopeGuard.h"
#include "Basics/StaticStrings.h"
#include "Basics/application-exit.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ServerState.h"
#ifdef USE_ENTERPRISE
#include "Enterprise/StorageEngine/HotBackupFeature.h"
#endif
#include "FeaturePhases/AqlFeaturePhase.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "Pregel/PregelFeature.h"
#include "Replication/ReplicationFeature.h"
#include "RestServer/BootstrapFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/InitDatabaseFeature.h"
#include "VocBase/Methods/Upgrade.h"
#include "VocBase/vocbase.h"

using namespace arangodb::application_features;
using namespace arangodb::basics;
using namespace arangodb::options;

namespace arangodb {

UpgradeFeature::UpgradeFeature(application_features::ApplicationServer& server,
                               int* result, std::vector<std::type_index> const& nonServerFeatures)
    : ApplicationFeature(server, "Upgrade"),
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
  options->addSection("database", "Configure the database");

  options->addOldOption("upgrade", "database.auto-upgrade");

  options->addOption("--database.auto-upgrade",
                     "perform a database upgrade if necessary",
                     new BooleanParameter(&_upgrade));

  options->addOption("--database.upgrade-check", "skip a database upgrade",
                     new BooleanParameter(&_upgradeCheck),
                     arangodb::options::makeDefaultFlags(arangodb::options::Flags::Hidden));
}

/// @brief This external is buried in RestServer/arangod.cpp.
///        Used to perform one last action upon shutdown.
extern std::function<int()> * restartAction;

#ifndef _WIN32
static int upgradeRestart() {
  unsetenv(StaticStrings::UpgradeEnvName.c_str());
  return 0;
}
#endif

void UpgradeFeature::validateOptions(std::shared_ptr<ProgramOptions> options) {
#ifndef _WIN32
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
#endif
  if (_upgrade && !_upgradeCheck) {
    LOG_TOPIC("47698", FATAL, arangodb::Logger::FIXME)
        << "cannot specify both '--database.auto-upgrade true' and "
           "'--database.upgrade-check false'";
    FATAL_ERROR_EXIT();
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
    std::vector<std::type_index> otherFeaturesToDisable = {
        std::type_index(typeid(DaemonFeature)),
        std::type_index(typeid(GreetingsFeature)),
        std::type_index(typeid(pregel::PregelFeature)),
        std::type_index(typeid(SupervisorFeature))
    };
    server().forceDisableFeatures(otherFeaturesToDisable);
  } else {
    server().forceDisableFeatures(_nonServerFeatures);
    std::vector<std::type_index> otherFeaturesToDisable = {
        std::type_index(typeid(BootstrapFeature)),
        std::type_index(typeid(HttpEndpointProvider))
    };
    server().forceDisableFeatures(otherFeaturesToDisable);
  }

  ReplicationFeature& replicationFeature = server().getFeature<ReplicationFeature>();
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
  auth::UserManager* um = server().getFeature<AuthenticationFeature>().userManager();

  // upgrade the database
  if (_upgradeCheck) {
    if (!ServerState::instance()->isCoordinator()) {
      // no need to run local upgrades in the coordinator
      upgradeLocalDatabase();
    }

    if (!init.restoreAdmin() && !init.defaultPassword().empty() && um != nullptr) {
      um->updateUser("root", [&](auth::User& user) {
        user.updatePassword(init.defaultPassword());
        return TRI_ERROR_NO_ERROR;
      });
    }
  }

  // change admin user
  if (init.restoreAdmin() && ServerState::instance()->isSingleServerOrCoordinator()) {
    Result res = um->removeAllUsers();
    if (res.fail()) {
      LOG_TOPIC("70922", ERR, arangodb::Logger::FIXME)
          << "failed to clear users: " << res.errorMessage();
      *_result = EXIT_FAILURE;
      return;
    }

    VPackSlice extras = VPackSlice::noneSlice();
    res = um->storeUser(true, "root", init.defaultPassword(), true, extras);
    if (res.fail() && res.errorNumber() == TRI_ERROR_USER_NOT_FOUND) {
      res = um->storeUser(false, "root", init.defaultPassword(), true, extras);
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

  // and force shutdown
  if (_upgrade || init.isInitDatabase() || init.restoreAdmin()) {
    if (init.isInitDatabase()) {
      *_result = EXIT_SUCCESS;
    }

    if (!ServerState::instance()->isCoordinator() || !_upgrade) {
      LOG_TOPIC("7da27", INFO, arangodb::Logger::STARTUP)
          << "server will now shut down due to upgrade, database initialization "
             "or admin restoration.";

      // in the non-coordinator case, we are already done now and will shut down.
      // in the coordinator case, the actual upgrade is performed by the 
      // ClusterUpgradeFeature, which is way later in the startup sequence.
      server().beginShutdown();
    }
  }
}

void UpgradeFeature::upgradeLocalDatabase() {
  LOG_TOPIC("05dff", TRACE, arangodb::Logger::FIXME) << "starting database init/upgrade";

  DatabaseFeature& databaseFeature = server().getFeature<DatabaseFeature>();

  bool ignoreDatafileErrors = databaseFeature.ignoreDatafileErrors();

  for (auto& name : databaseFeature.getDatabaseNames()) {
    TRI_vocbase_t* vocbase = databaseFeature.lookupDatabase(name);
    TRI_ASSERT(vocbase != nullptr);

    auto res = methods::Upgrade::startup(*vocbase, _upgrade, ignoreDatafileErrors);

    if (res.fail()) {
      char const* typeName = "initialization";

      if (res.type == methods::VersionResult::UPGRADE_NEEDED) {
        typeName = "upgrade";  // an upgrade failed or is required

        if (!_upgrade) {
          LOG_TOPIC("1c156", ERR, arangodb::Logger::FIXME)
              << "Database '" << vocbase->name() << "' needs upgrade. "
              << "Please start the server with --database.auto-upgrade";
        }
      }

      LOG_TOPIC("2eb08", FATAL, arangodb::Logger::FIXME)
          << "Database '" << vocbase->name() << "' " << typeName << " failed ("
          << res.errorMessage() << "). "
          << "Please inspect the logs from the " << typeName << " procedure"
          << " and try starting the server again.";

      FATAL_ERROR_EXIT();
    }
  }

  if (_upgrade) {
    *_result = EXIT_SUCCESS;
    LOG_TOPIC("0de5e", INFO, arangodb::Logger::FIXME) << "database upgrade passed";
  }

  // and return from the context
  LOG_TOPIC("01a03", TRACE, arangodb::Logger::FIXME) << "finished database init/upgrade";
}

}  // namespace arangodb
