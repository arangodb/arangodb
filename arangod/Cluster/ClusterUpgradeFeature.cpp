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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "Cluster/ClusterUpgradeFeature.h"

#include "Agency/AgencyComm.h"
#include "Agency/AgencyFeature.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/ScopeGuard.h"
#include "Cluster/AgencyCache.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ServerState.h"
#include "FeaturePhases/FinalFeaturePhase.h"
#include "Logger/LogMacros.h"
#include "ProgramOptions/ProgramOptions.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/UpgradeFeature.h"
#include "VocBase/vocbase.h"
#include "VocBase/Methods/Upgrade.h"
#include "VocBase/Methods/Version.h"

using namespace arangodb;
using namespace arangodb::options;

namespace {
static std::string const upgradeVersionKey = "ClusterUpgradeVersion";
static std::string const upgradeExecutedByKey = "ClusterUpgradeExecutedBy";
}

ClusterUpgradeFeature::ClusterUpgradeFeature(application_features::ApplicationServer& server)
    : ApplicationFeature(server, "ClusterUpgrade"),
      _upgradeMode("auto") {
  startsAfter<application_features::FinalFeaturePhase>();
}

void ClusterUpgradeFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addOption("--cluster.upgrade",
                     "perform a cluster upgrade if necessary (auto = perform upgrade and shut down only if `--database.auto-upgrade true` is set, disable = never perform upgrade, force = always perform an upgrade and shut down, online = always perform an upgrade but don't shut down)",
                     new DiscreteValuesParameter<StringParameter>(&_upgradeMode, std::unordered_set<std::string>{"auto", "disable", "force", "online"}));
}

void ClusterUpgradeFeature::validateOptions(std::shared_ptr<ProgramOptions> options) {
  auto& databaseFeature = server().getFeature<arangodb::DatabaseFeature>();
  if (_upgradeMode == "force") {
    // always perform an upgrade, regardless of the value of `--database.auto-upgrade`.
    // after the upgrade, shut down the server
    databaseFeature.enableUpgrade();
  } else if (_upgradeMode == "disable") {
    // never perform an upgrade, regardless of the value of `--database.auto-upgrade`.
    // don't shut down the server
    databaseFeature.disableUpgrade();
  } else if (_upgradeMode == "online") {
    // perform an upgrade, but stay online and don't shut down the server.
    // disabling the upgrade functionality in the database feature is required for this.
    databaseFeature.disableUpgrade();
  }
}

void ClusterUpgradeFeature::start() {
  if (!ServerState::instance()->isCoordinator()) {
    return;
  }

  // this feature is doing something meaning only in a coordinator, and only
  // if the server was started with the option `--database.auto-upgrade true`.
  auto& databaseFeature = server().getFeature<arangodb::DatabaseFeature>();
  if (_upgradeMode == "disable" || (!databaseFeature.upgrade() && (_upgradeMode != "online" && _upgradeMode != "force"))) {
    return;
  }

  tryClusterUpgrade();

  if (_upgradeMode != "online") {
    LOG_TOPIC("d6047", INFO, arangodb::Logger::STARTUP) << "server will now shut down due to upgrade.";
    server().beginShutdown();
  }
}

void ClusterUpgradeFeature::setBootstrapVersion() {
  // it is not a fundamental problem if the setValue fails. if it fails, we can't
  // store the version number in the agency, so an upgrade we will run all the
  // (idempotent) upgrade tasks for the same version again.
  VPackBuilder builder;
  builder.add(VPackValue(arangodb::methods::Version::current()));

  AgencyComm agency(server());
  agency.setValue(::upgradeVersionKey, builder.slice(), 0);
}

void ClusterUpgradeFeature::tryClusterUpgrade() {
  TRI_ASSERT(ServerState::instance()->isCoordinator());

  auto& cache = server().getFeature<ClusterFeature>().agencyCache();
  auto [acb, idx] = cache.read(
    std::vector<std::string>{AgencyCommHelper::path(::upgradeVersionKey)});
  auto res = acb->slice();

  if (!res.isArray()) {
    LOG_TOPIC("26104", ERR, arangodb::Logger::CLUSTER)
      << "unable to fetch cluster upgrade version from agency: " << res.toJson();
    return;
  }

  uint64_t latestUpgradeVersion = 0;
  VPackSlice value = res[0].get(
      std::vector<std::string>({AgencyCommHelper::path(), ::upgradeVersionKey}));
  if (value.isNumber()) {
    latestUpgradeVersion = value.getNumber<uint64_t>();
    LOG_TOPIC("54f69", DEBUG, arangodb::Logger::CLUSTER)
      << "found previous cluster upgrade version in agency: " << latestUpgradeVersion;
  } else {
    // key not there yet.
    LOG_TOPIC("5b00d", DEBUG, arangodb::Logger::CLUSTER)
      << "did not find previous cluster upgrade version in agency";
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
  // there must be no other coordinator that performs an upgrade at the same time
  precs.emplace_back(::upgradeExecutedByKey, AgencyPrecondition::Type::EMPTY, true);

  // try to register ourselves as responsible for the upgrade
  AgencyOperation operation(::upgradeExecutedByKey, AgencyValueOperationType::SET, ServerState::instance()->getId());
  // make the key expire automatically in case we crash
  // operation._ttl = TRI_microtime() + 1800.0;
  AgencyWriteTransaction transaction(operation, precs);

  AgencyComm agency(server());
  AgencyCommResult result = agency.sendTransactionWithFailover(transaction);
  if (result.successful()) {
    auto& cache = server().getFeature<ClusterFeature>().agencyCache();
    cache.waitFor(result.slice().get("results")[0].getNumber<uint64_t>()).get();

    // we are responsible for the upgrade!
    LOG_TOPIC("15ac4", INFO, arangodb::Logger::CLUSTER)
        << "running cluster upgrade from "
        << (latestUpgradeVersion == 0 ? std::string("an unknown version") : std::string("version ") + std::to_string(latestUpgradeVersion))
        << " to version " << arangodb::methods::Version::current() << "...";

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
    // remove the key that locks out other coordinators from upgrading
    operations.emplace_back(::upgradeExecutedByKey, AgencySimpleOperationType::DELETE_OP);
    AgencyWriteTransaction transaction(operations, precs);

    result = agency.sendTransactionWithFailover(transaction);
    if (result.successful()) {
      LOG_TOPIC("853de", INFO, arangodb::Logger::CLUSTER)
          << "cluster upgrade to version " << arangodb::methods::Version::current()
          << " completed successfully";
    } else {
      LOG_TOPIC("a0b4f", ERR, arangodb::Logger::CLUSTER) << "unable to store cluster upgrade information in agency: " << result.errorMessage();
    }
  } else if (result.httpCode() != (int)arangodb::rest::ResponseCode::PRECONDITION_FAILED) {
    LOG_TOPIC("482a3", WARN, arangodb::Logger::CLUSTER) << "unable to fetch upgrade information: " << result.errorMessage();
  } else {
    // someone else is performing the upgrade
    LOG_TOPIC("ab6eb", DEBUG, arangodb::Logger::CLUSTER) << "someone else is running the cluster upgrade right now";
  }
}

bool ClusterUpgradeFeature::upgradeCoordinator() {
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
