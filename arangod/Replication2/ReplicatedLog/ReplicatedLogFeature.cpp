////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
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
///
////////////////////////////////////////////////////////////////////////////////

#include "ReplicatedLogFeature.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "ApplicationFeatures/CommunicationFeaturePhase.h"
#include "Basics/application-exit.h"
#include "FeaturePhases/DatabaseFeaturePhase.h"
#include "Replication2/ReplicatedLog/LogCommon.h"
#include "Replication2/ReplicatedLog/ReplicatedLogMetrics.h"
#include "Metrics/MetricsFeature.h"
#include "Logger/LogMacros.h"
#include "Cluster/ServerState.h"
#include "Basics/FeatureFlags.h"

#include <memory>

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_log;

ReplicatedLogFeature::ReplicatedLogFeature(
    application_features::ApplicationServer& server)
    : ReplicatedLogFeature(server, ReplicatedLogGlobalSettings{}) {}

ReplicatedLogFeature::ReplicatedLogFeature(
    application_features::ApplicationServer& server,
    ReplicatedLogGlobalSettings options)
    : application_features::ApplicationFeature{server, *this},
      _options(
          std::make_shared<ReplicatedLogGlobalSettings>(std::move(options))) {
  setOptional(true);
  startsAfter<CommunicationFeaturePhase>();
  startsAfter<DatabaseFeaturePhase>();
}

auto ReplicatedLogFeature::metrics() const noexcept -> std::shared_ptr<
    replication2::replicated_log::ReplicatedLogMetrics> const& {
  return _replicatedLogMetrics;
}

void ReplicatedLogFeature::start() {
  _replicatedLogMetrics = std::make_shared<ReplicatedLogMetrics>(
      this->server().getFeature<metrics::MetricsFeature>());
}

auto ReplicatedLogFeature::options() const noexcept
    -> std::shared_ptr<replication2::ReplicatedLogGlobalSettings const> {
  return _options;
}

void ReplicatedLogFeature::prepare() {
  if (!::arangodb::replication2::EnableReplication2) {
    setEnabled(false);
    return;
  }
  if (ServerState::instance()->isCoordinator() ||
      ServerState::instance()->isAgent()) {
    setEnabled(false);
    return;
  }
}

ReplicatedLogFeature::~ReplicatedLogFeature() = default;