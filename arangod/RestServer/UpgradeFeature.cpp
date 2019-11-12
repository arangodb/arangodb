////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
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

#include "Agency/AgencyComm.h"
#include "Agency/AgencyFeature.h"
#include "ApplicationFeatures/DaemonFeature.h"
#include "ApplicationFeatures/HttpEndpointProvider.h"
#include "ApplicationFeatures/GreetingsFeature.h"
#include "ApplicationFeatures/SupervisorFeature.h"
#include "Basics/application-exit.h"
#include "Basics/ScopeGuard.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ServerState.h"
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

namespace {
static std::string const upgradeVersionKey = "ClusterUpgradeVersion";
static std::string const upgradeExecutedByKey = "ClusterUpgradeExecutedBy";
}

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
                     arangodb::options::makeFlags(arangodb::options::Flags::Hidden));
}

void UpgradeFeature::validateOptions(std::shared_ptr<ProgramOptions> options) {
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

      server().beginShutdown();
    }
  }
}

std::string const& UpgradeFeature::clusterVersionAgencyKey() const {
  return ::upgradeVersionKey;
}

void UpgradeFeature::tryClusterUpgrade() {
  TRI_ASSERT(ServerState::instance()->isCoordinator());
  
  if (!_upgrade) {
    return;
  }
  
  AgencyComm agency;
  AgencyCommResult result = agency.getValues(::upgradeVersionKey);

  if (!result.successful()) {
    LOG_TOPIC("26104", ERR, arangodb::Logger::CLUSTER) << "unable to fetch cluster upgrade version from agency: " << result.errorMessage();
    return;
  }

  uint64_t latestUpgradeVersion = 0;
  VPackSlice value = result.slice()[0].get(
      std::vector<std::string>({AgencyCommManager::path(), ::upgradeVersionKey}));
  if (value.isNumber()) {
    latestUpgradeVersion = value.getNumber<uint64_t>();
    LOG_TOPIC("54f69", DEBUG, arangodb::Logger::CLUSTER) << "found previous cluster upgrade version in agency: " << latestUpgradeVersion;
  } else {
    // key not there yet.
    LOG_TOPIC("5b00d", DEBUG, arangodb::Logger::CLUSTER) << "did not find previous cluster upgrade version in agency"; 
  }

  if (arangodb::methods::Version::current() <= latestUpgradeVersion) {
    // nothing to do
    return;
  }

  std::vector<AgencyPrecondition> precs;
  if (latestUpgradeVersion == 0) {
    precs.emplace_back(::upgradeVersionKey, AgencyPrecondition::Type::EMPTY, true);
  } else {
    precs.emplace_back(::upgradeVersionKey, AgencyPrecondition::Type::VALUE, latestUpgradeVersion);
  }
  precs.emplace_back(::upgradeExecutedByKey, AgencyPrecondition::Type::EMPTY, true);

  // try to register ourselves as responsible for the upgrade
  AgencyOperation operation(::upgradeExecutedByKey, AgencyValueOperationType::SET, ServerState::instance()->getId());
  // make the key expire automatically in case we crash
  // operation._ttl = TRI_microtime() + 1800.0;  
  AgencyWriteTransaction transaction(operation, precs);

  result = agency.sendTransactionWithFailover(transaction);
  if (result.successful()) {
    // we are responsible for the upgrade!
    LOG_TOPIC("15ac4", INFO, arangodb::Logger::CLUSTER) << "running cluster upgrade...";

    bool success = false;
    try {
      success = upgradeCoordinator();
    } catch (std::exception const& ex) {
      LOG_TOPIC("f2a84", ERR, Logger::CLUSTER) << "caught exception during cluster upgrade: " << ex.what();
      TRI_ASSERT(!success);
    }

    // now finally remove the upgrading key and store the new version number
    std::vector<AgencyPrecondition> precs;
    precs.emplace_back(::upgradeExecutedByKey, AgencyPrecondition::Type::VALUE, ServerState::instance()->getId());

    std::vector<AgencyOperation> operations;
    if (success) {
      // upgrade successful - store our current version number
      operations.emplace_back(::upgradeVersionKey, AgencyValueOperationType::SET, arangodb::methods::Version::current());
    }
    operations.emplace_back(::upgradeExecutedByKey, AgencySimpleOperationType::DELETE_OP);
    AgencyWriteTransaction transaction(operations, precs);

    result = agency.sendTransactionWithFailover(transaction);
    if (result.successful()) {
      LOG_TOPIC("853de", DEBUG, arangodb::Logger::CLUSTER) << "successfully stored upgrade information in agency";
    } else {
      LOG_TOPIC("a0b4f", ERR, arangodb::Logger::CLUSTER) << "unable to store cluster upgrade information in agency: " << result.errorMessage();
    }
  } else if (result.httpCode() != (int)arangodb::rest::ResponseCode::PRECONDITION_FAILED) {
    // someone else has performed the upgrade
    LOG_TOPIC("482a3", WARN, arangodb::Logger::CLUSTER) << "unable to fetch upgrade information: " << result.errorMessage();
  } else {
    LOG_TOPIC("ab6eb", DEBUG, arangodb::Logger::CLUSTER) << "someone else is running the cluster upgrade right now";
  }
}

bool UpgradeFeature::upgradeCoordinator() {
  LOG_TOPIC("a2d65", TRACE, arangodb::Logger::FIXME) << "starting coordinator upgrade";
 
  bool success = true;
  DatabaseFeature& databaseFeature = server().getFeature<DatabaseFeature>();
  
  for (auto& name : databaseFeature.getDatabaseNames()) {
    TRI_vocbase_t* vocbase = databaseFeature.useDatabase(name);

    if (vocbase == nullptr) {
      // probably deleted in the meantime... so we can ignore it here
      continue;
    }

    auto guard = scopeGuard([&vocbase]() { vocbase->release(); });

    auto res = methods::Upgrade::startupCoordinator(*vocbase);
    if (res.fail()) {
      LOG_TOPIC("f51b1", ERR, arangodb::Logger::FIXME)
          << "Database '" << vocbase->name() << "' upgrade failed ("
          << res.errorMessage() << "). "
          << "Please inspect the logs from the upgrade procedure"
          << " and try starting the server again.";
      success = false;
    }
  }
  
  LOG_TOPIC("efd49", TRACE, arangodb::Logger::FIXME) << "finished coordinator upgrade";
  return success;
}

void UpgradeFeature::upgradeLocalDatabase() {
  LOG_TOPIC("05dff", TRACE, arangodb::Logger::FIXME) << "starting database init/upgrade";

  DatabaseFeature& databaseFeature = server().getFeature<DatabaseFeature>();

  bool ignoreDatafileErrors = false;
  {
    VPackBuilder options = server().options([](std::string const& name) {
      return (name.find("database.ignore-datafile-errors") != std::string::npos);
    });
    VPackSlice s = options.slice();
    if (s.get("database.ignore-datafile-errors").isBoolean()) {
      ignoreDatafileErrors = s.get("database.ignore-datafile-errors").getBool();
    }
  }

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
