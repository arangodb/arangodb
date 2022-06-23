////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////

#include "ReplicatedStateMetrics.h"

#include "Replication2/ReplicatedLog/ReplicatedLogMetricsDeclarations.h"
#include "Metrics/MetricsFeature.h"
#include "Metrics/Gauge.h"

DECLARE_GAUGE(arangodb_replication2_replicated_state_number, std::uint64_t,
              "Number of times a replicated states on this server");
DECLARE_GAUGE(
    arangodb_replication2_replicated_state_leader_number, std::uint64_t,
    "Number of times a replicated states on this server started as a leader");
DECLARE_GAUGE(
    arangodb_replication2_replicated_state_follower_number, std::uint64_t,
    "Number of times a replicated states on this server started as a follower");

using namespace arangodb::replication2::replicated_state;

ReplicatedStateMetrics::ReplicatedStateMetrics(
    metrics::MetricsFeature& metricsFeature, std::string_view impl)
    : ReplicatedStateMetrics(&metricsFeature, impl) {}

template<typename Builder, bool mock>
auto ReplicatedStateMetrics::createMetric(
    metrics::MetricsFeature* metricsFeature, std::string_view impl)
    -> std::shared_ptr<typename Builder::MetricT> {
  TRI_ASSERT((metricsFeature == nullptr) == mock);
  if constexpr (!mock) {
    return metricsFeature->addShared(Builder{}.withLabel("state_impl", impl));
  } else {
    return std::dynamic_pointer_cast<typename Builder::MetricT>(
        Builder{}.build());
  }
}

template<
    typename MFP,
    std::enable_if_t<std::is_same_v<arangodb::metrics::MetricsFeature*, MFP> ||
                         std::is_null_pointer_v<MFP>,
                     int>,
    bool mock>
ReplicatedStateMetrics::ReplicatedStateMetrics(MFP metricsFeature,
                                               std::string_view impl)
    : replicatedStateNumber(
          createMetric<arangodb_replication2_replicated_state_number, mock>(
              metricsFeature, impl)),
      replicatedStateNumberLeaders(
          createMetric<arangodb_replication2_replicated_state_leader_number,
                       mock>(metricsFeature, impl)),
      replicatedStateNumberFollowers(
          createMetric<arangodb_replication2_replicated_state_follower_number,
                       mock>(metricsFeature, impl)) {}

template arangodb::replication2::replicated_state::ReplicatedStateMetrics::
    ReplicatedStateMetrics(arangodb::metrics::MetricsFeature*,
                           std::string_view);

template arangodb::replication2::replicated_state::ReplicatedStateMetrics::
    ReplicatedStateMetrics(std::nullptr_t, std::string_view);
