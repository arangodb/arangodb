////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Kaveh Vahedipour
/// @author Matthew Von-Maszewski
////////////////////////////////////////////////////////////////////////////////

#include "MaintenanceFeature.h"

using namespace arangodb;
using namespace arangodb::options;


MaintenanceFeature::MaintenanceFeature(application_features::ApplicationServer* server)
    : ApplicationFeature(server, "Maintenance") {
  setOptional(true);
  requiresElevatedPrivileges(false);
  startsAfter("EngineSelector");
  startsBefore("StorageEngine");

  _maintenanceThreadsMax = static_cast<int32_t>(TRI_numberProcessors()/4 +1);
  _secondsActionsBlock = 30;
  _secondsActionsLinger = 300;
}

void MaintenanceFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addSection("server", "Server features");

  options->addHiddenOption("--server.maintenance-threads",
                           "maximum number of threads available for maintenance actions",
                     new Int32Parameter(&_maintenanceThreadsMax));

  options->addHiddenOption("--server.maintenance-actions-block",
                           "minimum number of seconds finished Actions block duplicates",
                     new Int32Parameter(&_secondsActionsBlock));

  options->addHiddenOption("--server.maintenance-actions-linger",
                           "minimum number of seconds finished Actions remain in deque",
                     new Int32Parameter(&_secondsActionsLinger));

}

void MaintenanceFeature::prepare() {
}
