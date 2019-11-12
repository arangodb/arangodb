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

#include "ApplicationFeatures/ApplicationServer.h"
#include "Cluster/ServerState.h"
#include "FeaturePhases/FinalFeaturePhase.h"
#include "Logger/LogMacros.h"
#include "ProgramOptions/ProgramOptions.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/UpgradeFeature.h"

using namespace arangodb;
using namespace arangodb::options;

ClusterUpgradeFeature::ClusterUpgradeFeature(application_features::ApplicationServer& server)
    : ApplicationFeature(server, "ClusterUpgrade") {
  startsAfter<application_features::FinalFeaturePhase>();
}

void ClusterUpgradeFeature::start() {
  if (!ServerState::instance()->isCoordinator()) {
    return;
  }

  auto& databaseFeature = server().getFeature<arangodb::DatabaseFeature>();
  if (!databaseFeature.upgrade()) {
    return;
  }

  auto& upgradeFeature = server().getFeature<arangodb::UpgradeFeature>();
  upgradeFeature.tryClusterUpgrade();

  LOG_TOPIC("d6047", INFO, arangodb::Logger::STARTUP) << "server will now shut down due to upgrade.";
  server().beginShutdown();
}
