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
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////

#include "RestServer/BootstrapFeature.h"

#include "Agency/AgencyComm.h"
#include "Aql/QueryList.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ServerState.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "GeneralServer/RestHandlerFactory.h"
#include "Logger/Logger.h"
#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"
#include "Rest/GeneralResponse.h"
#include "Rest/Version.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/SystemDatabaseFeature.h"
#include "VocBase/Methods/Upgrade.h"
#include "V8Server/V8DealerFeature.h"

namespace {
static std::string const FEATURE_NAME("Bootstrap");
}

using namespace arangodb;
using namespace arangodb::options;

static std::string const boostrapKey = "Bootstrap";

BootstrapFeature::BootstrapFeature(
    application_features::ApplicationServer& server
)
    : ApplicationFeature(server, ::FEATURE_NAME), _isReady(false), _bark(false) {
  startsAfter("ServerPhase");
  startsAfter(SystemDatabaseFeature::name());

  // TODO: It is only in FoxxPhase because of:
  startsAfter("FoxxQueues");

  // If this is Sorted out we can go down to ServerPhase
  // And activate the following dependencies:
  /*
  startsAfter("Endpoint");
  startsAfter("GeneralServer");
  startsAfter("Server");
  startsAfter("Upgrade");
  */
}

/*static*/ std::string const& BootstrapFeature::name() noexcept {
  return FEATURE_NAME;
}

void BootstrapFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addOption("hund", "make ArangoDB bark on startup",
                     new BooleanParameter(&_bark),
                     arangodb::options::makeFlags(arangodb::options::Flags::Hidden));
}

// Local Helper functions
namespace {

/// Initialize certain agency entries, like Plan, system collections
/// and various similar things. Only runs through on a SINGLE coordinator.
/// must only return if we are boostrap lead or bootstrap is done
void raceForClusterBootstrap() {
  AgencyComm agency;
  auto ci = ClusterInfo::instance();
  while (true) {
    AgencyCommResult result = agency.getValues(boostrapKey);
    if (!result.successful()) {
      // Error in communication, note that value not found is not an error
      LOG_TOPIC(TRACE, Logger::STARTUP)
      << "raceForClusterBootstrap: no agency communication";
      std::this_thread::sleep_for(std::chrono::seconds(1));
      continue;
    }

    VPackSlice value = result.slice()[0].get(
                std::vector<std::string>({AgencyCommManager::path(), boostrapKey}));
    if (value.isString()) {
      // key was found and is a string
      std::string boostrapVal = value.copyString();
      if (boostrapVal.find("done") != std::string::npos) {
        // all done, let's get out of here:
        LOG_TOPIC(TRACE, Logger::STARTUP)
          << "raceForClusterBootstrap: bootstrap already done";
        return;
      } else if (boostrapVal == ServerState::instance()->getId()) {
        agency.removeValues(boostrapKey, false);
      }
      LOG_TOPIC(DEBUG, Logger::STARTUP)
        << "raceForClusterBootstrap: somebody else does the bootstrap";
      std::this_thread::sleep_for(std::chrono::seconds(1));
      continue;
    }

    // No value set, we try to do the bootstrap ourselves:
    VPackBuilder b;
    b.add(VPackValue(arangodb::ServerState::instance()->getId()));
    result = agency.casValue(boostrapKey, b.slice(), false, 300, 15);
    if (!result.successful()) {
      LOG_TOPIC(DEBUG, Logger::STARTUP)
      << "raceForClusterBootstrap: lost race, somebody else will bootstrap";
      // Cannot get foot into the door, try again later:
      std::this_thread::sleep_for(std::chrono::seconds(1));
      continue;
    }
    // OK, we handle things now
    LOG_TOPIC(DEBUG, Logger::STARTUP)
        << "raceForClusterBootstrap: race won, we do the bootstrap";

    // let's see whether a DBserver is there:
    ci->loadCurrentDBServers();

    auto dbservers = ci->getCurrentDBServers();

    if (dbservers.size() == 0) {
      LOG_TOPIC(TRACE, Logger::STARTUP)
          << "raceForClusterBootstrap: no DBservers, waiting";
      agency.removeValues(boostrapKey, false);
      std::this_thread::sleep_for(std::chrono::seconds(1));
      continue;
    }

    auto* sysDbFeature = arangodb::application_features::ApplicationServer::lookupFeature<
      arangodb::SystemDatabaseFeature
    >();
    arangodb::SystemDatabaseFeature::ptr vocbase =
      sysDbFeature ? sysDbFeature->use() : nullptr;
    auto upgradeRes = vocbase
      ? methods::Upgrade::clusterBootstrap(*vocbase)
      : arangodb::Result(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND)
      ;

    if (upgradeRes.fail()) {
      LOG_TOPIC(ERR, Logger::STARTUP) << "Problems with cluster bootstrap, "
      << "marking as not successful.";
      agency.removeValues(boostrapKey, false);
      std::this_thread::sleep_for(std::chrono::seconds(1));
      continue;
    }

    // become Foxxmaster, ignore result
    LOG_TOPIC(DEBUG, Logger::STARTUP) << "Write Foxxmaster";
    agency.setValue("Current/Foxxmaster", b.slice(), 0);
    agency.increment("Current/Version");

    LOG_TOPIC(DEBUG, Logger::STARTUP) << "Creating the root user";
    auth::UserManager* um = AuthenticationFeature::instance()->userManager();
    if (um != nullptr) {
      um->createRootUser();
    }

    LOG_TOPIC(DEBUG, Logger::STARTUP)
        << "raceForClusterBootstrap: bootstrap done";

    b.clear();
    b.add(VPackValue(arangodb::ServerState::instance()->getId() + ": done"));
    result = agency.setValue(boostrapKey, b.slice(), 0);
    if (result.successful()) {
      return;
    }

    LOG_TOPIC(TRACE, Logger::STARTUP)
        << "raceForClusterBootstrap: could not indicate success";
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
}

/// Run the coordinator initialization script, will run on each
/// coordinator, not just one.
void runCoordinatorJS(TRI_vocbase_t* vocbase) {
  bool success = false;
  while (!success) {
    LOG_TOPIC(DEBUG, Logger::STARTUP)
    << "Running server/bootstrap/coordinator.js";

    VPackBuilder builder;
    V8DealerFeature::DEALER->loadJavaScriptFileInAllContexts(vocbase,
                                        "server/bootstrap/coordinator.js", &builder);

    auto slice = builder.slice();
    if (slice.isArray()) {
      if (slice.length() > 0) {
        bool newResult = true;
        for (VPackSlice val: VPackArrayIterator(slice)) {
          newResult = newResult && val.isTrue();
        }
        if (!newResult) {
          LOG_TOPIC(ERR, Logger::STARTUP)
          << "result of bootstrap was: " << builder.toJson() << ". retrying bootstrap in 1s.";
        }
        success = newResult;
      } else {
        LOG_TOPIC(ERR, Logger::STARTUP)
        << "bootstrap wasn't executed in a single context! retrying bootstrap in 1s.";
      }
    } else {
      LOG_TOPIC(ERR, Logger::STARTUP)
      << "result of bootstrap was not an array: " << slice.typeName() << ". retrying bootstrap in 1s.";
    }
    if (!success) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
  }
}

// Try to become leader in active-failover setup
void runActiveFailoverStart(std::string const& myId) {
  std::string const leaderPath = "Plan/AsyncReplication/Leader";
  try {
    VPackBuilder myIdBuilder;
    myIdBuilder.add(VPackValue(myId));
    AgencyComm agency;
    AgencyCommResult res = agency.getValues(leaderPath);
    if (res.successful()) {
      VPackSlice leader = res.slice()[0].get(AgencyCommManager::slicePath(leaderPath));
      if (!leader.isString() || leader.getStringLength() == 0) { // no leader in agency
        if (leader.isNone()) {
          res = agency.casValue(leaderPath, myIdBuilder.slice(), /*prevExist*/ false,
                                /*ttl*/ 0, /*timeout*/ 5.0);
        } else {
          res = agency.casValue(leaderPath, /*old*/leader, /*new*/myIdBuilder.slice(),
                                /*ttl*/ 0, /*timeout*/ 5.0);
        }
        if (res.successful()) { // sucessfull leadership takeover
          leader = myIdBuilder.slice();
        } // ignore for now, heartbeat thread will handle it
      }

      if (leader.isString() && leader.getStringLength() > 0) {
        ServerState::instance()->setFoxxmaster(leader.copyString());
        if (leader == myIdBuilder.slice()) {
          LOG_TOPIC(INFO, Logger::STARTUP) << "Became leader in active-failover setup";
        } else {
          LOG_TOPIC(INFO, Logger::STARTUP) << "Following: " << ServerState::instance()->getFoxxmaster();
        }
      }
    }
  } catch(...) {} // weglaecheln
}
}

void BootstrapFeature::start() {
  auto* sysDbFeature = arangodb::application_features::ApplicationServer::lookupFeature<
    arangodb::SystemDatabaseFeature
  >();
  arangodb::SystemDatabaseFeature::ptr vocbase =
    sysDbFeature ? sysDbFeature->use() : nullptr;
  bool v8Enabled = V8DealerFeature::DEALER && V8DealerFeature::DEALER->isEnabled();
  TRI_ASSERT(vocbase.get() != nullptr);

  auto ss = ServerState::instance();
  ServerState::RoleEnum role =  ServerState::instance()->getRole();

  if (ServerState::isRunningInCluster(role)) {
    // the coordinators will race to perform the cluster initialization.
    // The coordinatpr who does it will create system collections and
    // the root user
    if (ServerState::isCoordinator(role)) {
      LOG_TOPIC(DEBUG, Logger::STARTUP) << "Racing for cluster bootstrap...";
      raceForClusterBootstrap();

      if (v8Enabled) {
        ::runCoordinatorJS(vocbase.get());
      }
    } else if (ServerState::isDBServer(role)) {
      LOG_TOPIC(DEBUG, Logger::STARTUP) << "Running bootstrap";

      auto upgradeRes = methods::Upgrade::clusterBootstrap(*vocbase);

      if (upgradeRes.fail()) {
        LOG_TOPIC(ERR, Logger::STARTUP) << "Problem during startup";
      }
    } else {
      TRI_ASSERT(false);
    }
  } else {
    std::string const myId = ServerState::instance()->getId(); // local cluster UUID

    // become leader before running server.js to ensure the leader
    // is the foxxmaster. Everything else is handled in heartbeat
    if (ServerState::isSingleServer(role) && AgencyCommManager::isEnabled()) {
      ::runActiveFailoverStart(myId);
    } else {
      ss->setFoxxmaster(myId); // could be empty, but set anyway
    }

    if (v8Enabled) { // runs the single server boostrap JS
      // will run foxx/manager.js::_startup() and more (start queues, load routes, etc)
      LOG_TOPIC(DEBUG, Logger::STARTUP) << "Running server/server.js";
      V8DealerFeature::DEALER->loadJavaScriptFileInAllContexts(vocbase.get(), "server/server.js", nullptr);
    }
    auth::UserManager* um = AuthenticationFeature::instance()->userManager();
    if (um != nullptr) {
      // only creates root user if it does not exist, will be overwritten on slaves
      um->createRootUser();
    }
  }

  if (ServerState::isSingleServer(role) && AgencyCommManager::isEnabled()) {
    // simon: this is set to correct value in the heartbeat thread
    ServerState::setServerMode(ServerState::Mode::TRYAGAIN);
  } else {
    // Start service properly:
    ServerState::setServerMode(ServerState::Mode::DEFAULT);
  }

  LOG_TOPIC(INFO, arangodb::Logger::FIXME) << "ArangoDB (version " << ARANGODB_VERSION_FULL
            << ") is ready for business. Have fun!";
  if (_bark) {
    LOG_TOPIC(INFO, arangodb::Logger::FIXME) << "The dog says: wau wau!";
  }

  _isReady = true;
}

void BootstrapFeature::unprepare() {
  // notify all currently running queries about the shutdown
  auto databaseFeature =
      application_features::ApplicationServer::getFeature<DatabaseFeature>(
          "Database");

  for (auto& name : databaseFeature->getDatabaseNames()) {
    TRI_vocbase_t* vocbase = databaseFeature->useDatabase(name);

    if (vocbase != nullptr) {
      vocbase->queryList()->killAll(true);
      vocbase->release();
    }
  }
}
