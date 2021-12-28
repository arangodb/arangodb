////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021-2021 ArangoDB GmbH, Cologne, Germany
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

#include "ReplicatedLogMetrics.h"

#include "Replication2/ReplicatedLog/ReplicatedLogMetricsDeclarations.h"
#include "Metrics/MetricsFeature.h"

using namespace arangodb::replication2::replicated_log;

ReplicatedLogMetrics::ReplicatedLogMetrics(
    metrics::MetricsFeature& metricsFeature)
    : ReplicatedLogMetrics(&metricsFeature) {}

template<typename Builder, bool mock>
auto ReplicatedLogMetrics::createMetric(metrics::MetricsFeature* metricsFeature)
    -> std::shared_ptr<typename Builder::MetricT> {
  TRI_ASSERT((metricsFeature == nullptr) == mock);
  if constexpr (!mock) {
    return metricsFeature->addShared(Builder{});
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
ReplicatedLogMetrics::ReplicatedLogMetrics(MFP metricsFeature)
    : replicatedLogNumber(
          createMetric<arangodb_replication2_replicated_log_number, mock>(
              metricsFeature)),
      replicatedLogAppendEntriesRttUs(
          createMetric<arangodb_replication2_replicated_log_append_entries_rtt,
                       mock>(metricsFeature)),
      replicatedLogFollowerAppendEntriesRtUs(
          createMetric<
              arangodb_replication2_replicated_log_follower_append_entries_rt,
              mock>(metricsFeature)),
      replicatedLogCreationNumber(
          createMetric<arangodb_replication2_replicated_log_creation_total,
                       mock>(metricsFeature)),
      replicatedLogDeletionNumber(
          createMetric<arangodb_replication2_replicated_log_deletion_total,
                       mock>(metricsFeature)),
      replicatedLogLeaderNumber(
          createMetric<arangodb_replication2_replicated_log_leader_number,
                       mock>(metricsFeature)),
      replicatedLogFollowerNumber(
          createMetric<arangodb_replication2_replicated_log_follower_number,
                       mock>(metricsFeature)),
      replicatedLogInactiveNumber(
          createMetric<arangodb_replication2_replicated_log_inactive_number,
                       mock>(metricsFeature)),
      replicatedLogLeaderTookOverNumber(
          createMetric<
              arangodb_replication2_replicated_log_leader_took_over_total,
              mock>(metricsFeature)),
      replicatedLogStartedFollowingNumber(
          createMetric<
              arangodb_replication2_replicated_log_started_following_total,
              mock>(metricsFeature)),
      replicatedLogInsertsBytes(
          createMetric<arangodb_replication2_replicated_log_inserts_bytes,
                       mock>(metricsFeature)),
      replicatedLogInsertsRtt(
          createMetric<arangodb_replication2_replicated_log_inserts_rtt, mock>(
              metricsFeature)) {
#ifndef ARANGODB_USE_GOOGLE_TESTS
  static_assert(!mock);
  static_assert(!std::is_null_pointer_v<MFP>);
#endif
}

MeasureTimeGuard::MeasureTimeGuard(
    std::shared_ptr<metrics::Histogram<metrics::LogScale<std::uint64_t>>>
        histogram) noexcept
    : _start(std::chrono::steady_clock::now()),
      _histogram(std::move(histogram)) {}

void MeasureTimeGuard::fire() {
  if (_histogram) {
    auto const endTime = std::chrono::steady_clock::now();
    auto const duration =
        std::chrono::duration_cast<std::chrono::microseconds>(endTime - _start);
    _histogram->count(duration.count());
    _histogram.reset();
  }
}

MeasureTimeGuard::~MeasureTimeGuard() { fire(); }

template arangodb::replication2::replicated_log::ReplicatedLogMetrics::
    ReplicatedLogMetrics(arangodb::metrics::MetricsFeature*);
#ifdef ARANGODB_USE_GOOGLE_TESTS
template arangodb::replication2::replicated_log::ReplicatedLogMetrics::
    ReplicatedLogMetrics(std::nullptr_t);
#endif
