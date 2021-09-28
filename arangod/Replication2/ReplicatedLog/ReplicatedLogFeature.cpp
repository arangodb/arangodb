////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#include "ReplicatedLogFeature.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "ApplicationFeatures/CommunicationFeaturePhase.h"
#include "FeaturePhases/DatabaseFeaturePhase.h"
#include "Replication2/ReplicatedLog/ReplicatedLogMetrics.h"
#include "RestServer/MetricsFeature.h"

#include <memory>

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_log;

ReplicatedLogFeature::ReplicatedLogFeature(ApplicationServer& server)
    : ApplicationFeature(server, "ReplicatedLog"),
      _replicatedLogMetrics(std::make_shared<ReplicatedLogMetrics>(
          server.getFeature<MetricsFeature>())) {
  setOptional(true);
  startsAfter<CommunicationFeaturePhase>();
  startsAfter<DatabaseFeaturePhase>();
}

auto ReplicatedLogFeature::metrics() const noexcept
    -> std::shared_ptr<replication2::replicated_log::ReplicatedLogMetrics> const& {
  return _replicatedLogMetrics;
}

void ReplicatedLogFeature::prepare() {
  if (ServerState::instance()->isCoordinator() || ServerState::instance()->isAgent()) {
    setEnabled(false);
    return;
  }
}

ReplicatedLogFeature::~ReplicatedLogFeature() = default;
