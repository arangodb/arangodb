////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#include "VectorIndex/VectorIndexFeature.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Futures/Utilities.h"
#include "Cluster/MaintenanceFeature.h"
#include "Cluster/ServerState.h"
#include "FeaturePhases/BasicFeaturePhaseServer.h"
#include "Metrics/MetricsFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Parameters.h"
#include "Scheduler/SchedulerFeature.h"

namespace arangodb {

VectorIndexFeature::VectorIndexFeature(
    application_features::ApplicationServer& server)
    : ApplicationFeature{server, *this} {
  setOptional(false);
  startsAfter<application_features::BasicFeaturePhaseServer>();
}

void VectorIndexFeature::collectOptions(
    std::shared_ptr<options::ProgramOptions> options) {
  options->addObsoleteOption(
      "--vector-index",
      "Enable the vector index feature. "
      "Once in use, this option cannot be turned off again.",
      true);

  options->addOldOption("--experimental-vector-index", "--vector-index");
}

bool VectorIndexFeature::shouldRunBuildManager() const {
  return isVectorIndexEnabled() && (ServerState::instance()->isDBServer() ||
                                    ServerState::instance()->isSingleServer());
}

void VectorIndexFeature::start() {
  if (!shouldRunBuildManager()) {
    return;
  }
  TRI_ASSERT(SchedulerFeature::SCHEDULER != nullptr);
  _buildManager.emplace(server().getFeature<DatabaseFeature>(),
                        server().getFeature<MaintenanceFeature>(),
                        server().getFeature<metrics::MetricsFeature>(),
                        *SchedulerFeature::SCHEDULER);
  _buildManager->start();
}

void VectorIndexFeature::beginShutdown() {
  if (!_buildManager.has_value()) {
    return;
  }
  _buildManager->beginShutdown();
}

void VectorIndexFeature::stop() {
  if (!_buildManager.has_value()) {
    return;
  }
  _buildManager->stop();
}

bool VectorIndexFeature::isVectorIndexEnabled() const {
  return _options.useVectorIndex;
}

futures::Future<Result> VectorIndexFeature::waitForIndexReady(IndexId indexId) {
  if (!_buildManager.has_value()) {
    // Build manager is not initialized (e.g. on Coordinator).
    return futures::makeFuture(Result{});
  }
  return _buildManager->waitForIndexReady(indexId);
}

}  // namespace arangodb
