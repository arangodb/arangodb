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

#include "Logger/Logger.h"
#include "Cluster/AgencyComm.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ServerState.h"
#include "Rest/GeneralResponse.h"
#include "Rest/Version.h"
#include "RestServer/DatabaseFeature.h"
#include "V8Server/V8DealerFeature.h"

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::options;

BootstrapFeature::BootstrapFeature(application_features::ApplicationServer* server)
    : ApplicationFeature(server, "Bootstrap") {
  startsAfter("Dispatcher");
  startsAfter("Endpoint");
  startsAfter("Scheduler");
  startsAfter("Server");
  startsAfter("Agency");
  startsAfter("LogfileManager");
  startsAfter("Database");
  startsAfter("Upgrade");
  startsAfter("CheckVersion");
  startsAfter("FoxxQueues");
  startsAfter("RestServer");
}

static void raceForClusterBootstrap() {
  AgencyComm agency;
  auto ci = ClusterInfo::instance();

  while (true) {
    AgencyCommResult result = agency.getValues("Bootstrap", false);
    if (!result.successful() && 
        result.httpCode() != (int) arangodb::GeneralResponse::ResponseCode::NOT_FOUND) {
      // Error in communication, we could live with NOT_FOUND or OK
      LOG_TOPIC(TRACE, Logger::STARTUP) 
          << "raceForClusterBootstrap: no agency communication";
      sleep(1);
      continue;
    }
    result.parse("", false);
    auto it = result._values.begin();
    if (it != result._values.end()) {
      VPackSlice value = it->second._vpack->slice();
      if (value.isString() && value.isEqualString("done")) {
        // all done, let's get out of here:
        LOG_TOPIC(TRACE, Logger::STARTUP) 
            << "raceForClusterBootstrap: bootstrap already done";
        return;
      }
      LOG_TOPIC(TRACE, Logger::STARTUP) 
          << "raceForClusterBootstrap: somebody else does the bootstrap";
      sleep(1);
      continue;
    }

    // No value set, we try to do the bootstrap ourselves:
    VPackBuilder b;
    b.add(VPackValue(arangodb::ServerState::instance()->getId()));
    result = agency.casValue("Bootstrap", b.slice(), false, 300, 15);
    if (!result.successful()) {
      LOG_TOPIC(INFO, Logger::STARTUP) << "raceForClusterBootstrap: lost race";
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

    LOG_TOPIC(INFO, Logger::STARTUP) 
        << "raceForClusterBootstrap: race won, we do the bootstrap";
    auto vocbase = DatabaseFeature::DATABASE->vocbase();
    V8DealerFeature::DEALER->loadJavascriptFiles(vocbase, "server/bootstrap/cluster-bootstrap.js", 0);

    LOG_TOPIC(INFO, Logger::STARTUP) 
        << "raceForClusterBootstrap: bootstrap done";

    b.clear();
    b.add(VPackValue("done"));
    result = agency.setValue("Bootstrap", b.slice(), 15);
    if (result.successful()) {
      return;
    }

    LOG_TOPIC(TRACE, Logger::STARTUP) 
        << "raceForClusterBootstrap: could not indicate success";

    sleep(1);
  }
}

void BootstrapFeature::start() {
  auto vocbase = DatabaseFeature::DATABASE->vocbase();

  auto ss = ServerState::instance();
  if (ss->isCoordinator()) {
    LOG_TOPIC(INFO, Logger::STARTUP) << "Racing for cluster bootstrap...";
    raceForClusterBootstrap();
    LOG_TOPIC(INFO, Logger::STARTUP) << "Running server/bootstrap/coordinator.js";
    V8DealerFeature::DEALER->loadJavascript(vocbase, "server/bootstrap/coordinator.js");
  } else if (ss->isDBServer()) {
    LOG_TOPIC(INFO, Logger::STARTUP) << "Running server/bootstrap/db-server.js";
    V8DealerFeature::DEALER->loadJavascript(vocbase, "server/bootstrap/db-server.js");
  } else {
    LOG_TOPIC(INFO, Logger::STARTUP) << "Running server/server.js";
    V8DealerFeature::DEALER->loadJavascript(vocbase, "server/server.js");
  }

  LOG(INFO) << "ArangoDB (version " << ARANGODB_VERSION_FULL
            << ") is ready for business. Have fun!";
}

