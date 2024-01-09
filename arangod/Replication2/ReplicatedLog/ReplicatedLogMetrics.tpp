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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#include "ReplicatedLogMetrics.h"

#include "Replication2/ReplicatedLog/ReplicatedLogMetricsDeclarations.h"
#include "Metrics/MetricsFeature.h"

namespace arangodb::replication2::replicated_log {

template<bool mock>
template<typename Builder>
auto ReplicatedLogMetricsIndirect<mock>::createMetric(
    metrics::MetricsFeature* metricsFeature) -> typename Builder::MetricT* {
  if constexpr (not mock) {
    TRI_ASSERT((metricsFeature == nullptr) == mock);
    return &metricsFeature->add(Builder{});
  } else {
    static std::vector<std::shared_ptr<typename Builder::MetricT>> metrics;
    auto ptr =
        std::dynamic_pointer_cast<typename Builder::MetricT>(Builder{}.build());
    return metrics.emplace_back(ptr).get();
  }
}

template<bool mock>
ReplicatedLogMetricsIndirect<mock>::ReplicatedLogMetricsIndirect(
    metrics::MetricsFeature* metricsFeature) {
  replicatedLogNumber =
      createMetric<arangodb_replication2_replicated_log_number>(metricsFeature);
  replicatedLogAppendEntriesRttUs =
      createMetric<arangodb_replication2_replicated_log_append_entries_rtt>(
          metricsFeature);
  replicatedLogFollowerAppendEntriesRtUs = createMetric<
      arangodb_replication2_replicated_log_follower_append_entries_rt>(
      metricsFeature);
  replicatedLogCreationNumber =
      createMetric<arangodb_replication2_replicated_log_creation_total>(
          metricsFeature);
  replicatedLogDeletionNumber =
      createMetric<arangodb_replication2_replicated_log_deletion_total>(
          metricsFeature);
  replicatedLogLeaderNumber =
      createMetric<arangodb_replication2_replicated_log_leader_number>(
          metricsFeature);
  replicatedLogFollowerNumber =
      createMetric<arangodb_replication2_replicated_log_follower_number>(
          metricsFeature);
  replicatedLogInactiveNumber =
      createMetric<arangodb_replication2_replicated_log_inactive_number>(
          metricsFeature);
  replicatedLogLeaderTookOverNumber =
      createMetric<arangodb_replication2_replicated_log_leader_took_over_total>(
          metricsFeature);
  replicatedLogStartedFollowingNumber = createMetric<
      arangodb_replication2_replicated_log_started_following_total>(
      metricsFeature);
  replicatedLogInsertsBytes =
      createMetric<arangodb_replication2_replicated_log_inserts_bytes>(
          metricsFeature);
  replicatedLogInsertsRtt =
      createMetric<arangodb_replication2_replicated_log_inserts_rtt>(
          metricsFeature);
  replicatedLogNumberAcceptedEntries = createMetric<
      arangodb_replication2_replicated_log_number_accepted_entries_total>(
      metricsFeature);
  replicatedLogNumberCommittedEntries = createMetric<
      arangodb_replication2_replicated_log_number_committed_entries_total>(
      metricsFeature);
  replicatedLogNumberMetaEntries = createMetric<
      arangodb_replication2_replicated_log_number_meta_entries_total>(
      metricsFeature);
  replicatedLogNumberCompactedEntries = createMetric<
      arangodb_replication2_replicated_log_number_compacted_entries_total>(
      metricsFeature);

  leaderNumInMemoryEntries =
      createMetric<arangodb_replication2_leader_in_memory_entries>(
          metricsFeature);
  leaderNumInMemoryBytes =
      createMetric<arangodb_replication2_leader_in_memory_bytes>(
          metricsFeature);

  replicatedLogAppendEntriesNumEntries = createMetric<
      arangodb_replication2_replicated_log_append_entries_num_entries>(
      metricsFeature);
  replicatedLogAppendEntriesSize =
      createMetric<arangodb_replication2_replicated_log_append_entries_size>(
          metricsFeature);
  replicatedLogFollowerEntryDropCount = createMetric<
      arangodb_replication2_replicated_log_follower_entry_drop_total>(
      metricsFeature);
  replicatedLogLeaderAppendEntriesErrorCount = createMetric<
      arangodb_replication2_replicated_log_leader_append_entries_error_total>(
      metricsFeature);
}

}  // namespace arangodb::replication2::replicated_log
