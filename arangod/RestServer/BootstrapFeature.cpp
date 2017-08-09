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
#include "GeneralServer/RestHandlerFactory.h"
#include "Logger/Logger.h"
#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"
#include "Rest/GeneralResponse.h"
#include "Rest/Version.h"
#include "RestServer/DatabaseFeature.h"
#include "V8Server/V8DealerFeature.h"

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::options;

BootstrapFeature::BootstrapFeature(
    application_features::ApplicationServer* server)
    : ApplicationFeature(server, "Bootstrap"), _isReady(false), _bark(false) {
  startsAfter("Endpoint");
  startsAfter("Scheduler");
  startsAfter("Server");
  startsAfter("Database");
  startsAfter("Upgrade");
  startsAfter("CheckVersion");
  startsAfter("FoxxQueues");
  startsAfter("GeneralServer");
}

void BootstrapFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addHiddenOption("hund", "make ArangoDB bark on startup",
                           new BooleanParameter(&_bark));
}

static void raceForClusterBootstrap() {
  AgencyComm agency;
  auto ci = ClusterInfo::instance();
  
  while (true) {
    AgencyCommResult result = agency.getValues("Bootstrap");
    if (!result.successful()) {
      // Error in communication, note that value not found is not an error
      LOG_TOPIC(TRACE, Logger::STARTUP)
          << "raceForClusterBootstrap: no agency communication";
      sleep(1);
      continue;
    }

    VPackSlice value = result.slice()[0].get(
        std::vector<std::string>({AgencyCommManager::path(), "Bootstrap"}));

    if (value.isString()) {
      // key was found and is a string
      if (value.copyString().find("done") != std::string::npos) {
        // all done, let's get out of here:
        LOG_TOPIC(TRACE, Logger::STARTUP)
            << "raceForClusterBootstrap: bootstrap already done";
        return;
      }
      LOG_TOPIC(DEBUG, Logger::STARTUP)
          << "raceForClusterBootstrap: somebody else does the bootstrap";
      sleep(1);
      continue;
    }

    // No value set, we try to do the bootstrap ourselves:
    VPackBuilder b;
    b.add(VPackValue(arangodb::ServerState::instance()->getId()));
    result = agency.casValue("Bootstrap", b.slice(), false, 300, 15);
    if (!result.successful()) {
      LOG_TOPIC(DEBUG, Logger::STARTUP)
          << "raceForClusterBootstrap: lost race, somebody else will bootstrap";
      // Cannot get foot into the door, try again later:
      sleep(1);
      continue;
    }

    // OK, we handle things now, let's see whether a DBserver is there:
    auto dbservers = ci->getCurrentDBServers();
    if (dbservers.size() == 0) {
      LOG_TOPIC(TRACE, Logger::STARTUP)
          << "raceForClusterBootstrap: no DBservers, waiting";
      agency.removeValues("Bootstrap", false);
      sleep(1);
      continue;
    }

    LOG_TOPIC(DEBUG, Logger::STARTUP)
        << "raceForClusterBootstrap: race won, we do the bootstrap";
    auto vocbase = DatabaseFeature::DATABASE->systemDatabase();
    VPackBuilder builder;
    V8DealerFeature::DEALER->loadJavaScriptFileInDefaultContext(
        vocbase, "server/bootstrap/cluster-bootstrap.js", &builder);

    VPackSlice jsresult = builder.slice();
    if (!jsresult.isTrue()) {
      LOG_TOPIC(ERR, Logger::STARTUP) << "Problems with cluster bootstrap, "
        << "marking as not successful.";
      if (!jsresult.isNone()) {
        LOG_TOPIC(ERR, Logger::STARTUP) << "Returned value: "
          << jsresult.toJson();
      } else {
        LOG_TOPIC(ERR, Logger::STARTUP) << "Empty returned value.";
      }
      agency.removeValues("Bootstrap", false);
      sleep(1);
      continue;
    }

    LOG_TOPIC(DEBUG, Logger::STARTUP)
        << "raceForClusterBootstrap: bootstrap done";

    b.clear();
    b.add(VPackValue(arangodb::ServerState::instance()->getId() + ": done"));
    result = agency.setValue("Bootstrap", b.slice(), 0);
    if (result.successful()) {
      return;
    }

    LOG_TOPIC(TRACE, Logger::STARTUP)
        << "raceForClusterBootstrap: could not indicate success";

    sleep(1);
  }
}

void BootstrapFeature::start() {
  auto vocbase = DatabaseFeature::DATABASE->systemDatabase();

  auto ss = ServerState::instance();

  if (!ss->isRunningInCluster()) {
    LOG_TOPIC(DEBUG, Logger::STARTUP) << "Running server/server.js";
    V8DealerFeature::DEALER->loadJavaScriptFileInAllContexts(vocbase, "server/server.js", nullptr);
  } else if (ss->isCoordinator()) {
    LOG_TOPIC(DEBUG, Logger::STARTUP) << "Racing for cluster bootstrap...";
    raceForClusterBootstrap();
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
          for (auto const& val: VPackArrayIterator(slice)) {
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
        sleep(1);
      }
    }
  } else if (ss->isDBServer()) {
    LOG_TOPIC(DEBUG, Logger::STARTUP)
        << "Running server/bootstrap/db-server.js";
    V8DealerFeature::DEALER->loadJavaScriptFileInAllContexts(vocbase,
        "server/bootstrap/db-server.js", nullptr);
  }

  // Start service properly:
  rest::RestHandlerFactory::setMaintenance(false);

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

  if (ServerState::instance()->isCoordinator()) {
    for (auto& id : databaseFeature->getDatabaseIdsCoordinator(true)) {
      TRI_vocbase_t* vocbase = databaseFeature->useDatabase(id);

      if (vocbase != nullptr) {
        vocbase->queryList()->killAll(true);
        vocbase->release();
      }
    }
  } else {
    for (auto& name : databaseFeature->getDatabaseNames()) {
      TRI_vocbase_t* vocbase = databaseFeature->useDatabase(name);

      if (vocbase != nullptr) {
        vocbase->queryList()->killAll(true);
        vocbase->release();
      }
    }
  }
}
