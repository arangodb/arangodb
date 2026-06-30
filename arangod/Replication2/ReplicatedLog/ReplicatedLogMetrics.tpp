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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#include "ReplicatedLogMetrics.h"

#include "Replication2/ReplicatedLog/ReplicatedLogMetricsDeclarations.h"
#include "Metrics/IRegistry.h"

namespace arangodb::replication2::replicated_log {

template<bool mock>
template<typename Builder>
auto ReplicatedLogMetricsIndirect<mock>::createMetric(
    metrics::IRegistry* metricsRegistry) -> typename Builder::MetricT* {
  if constexpr (not mock) {
    TRI_ASSERT((metricsRegistry == nullptr) == mock);
    return &metricsRegistry->add(Builder{});
  } else {
    static std::vector<std::shared_ptr<typename Builder::MetricT>> metrics;
    auto ptr =
        std::dynamic_pointer_cast<typename Builder::MetricT>(Builder{}.build());
    return metrics.emplace_back(ptr).get();
  }
}

template<bool mock>
ReplicatedLogMetricsIndirect<mock>::ReplicatedLogMetricsIndirect(
    metrics::IRegistry* metricsRegistry) {
  replicatedLogNumber =
      createMetric<arangodb_replication2_replicated_log_number>(
          metricsRegistry);
  replicatedLogAppendEntriesRttUs =
      createMetric<arangodb_replication2_replicated_log_append_entries_rtt>(
          metricsRegistry);
  replicatedLogFollowerAppendEntriesRtUs = createMetric<
      arangodb_replication2_replicated_log_follower_append_entries_rt>(
      metricsRegistry);
  replicatedLogCreationNumber =
      createMetric<arangodb_replication2_replicated_log_creation_total>(
          metricsRegistry);
  replicatedLogDeletionNumber =
      createMetric<arangodb_replication2_replicated_log_deletion_total>(
          metricsRegistry);
  replicatedLogLeaderNumber =
      createMetric<arangodb_replication2_replicated_log_leader_number>(
          metricsRegistry);
  replicatedLogFollowerNumber =
      createMetric<arangodb_replication2_replicated_log_follower_number>(
          metricsRegistry);
  replicatedLogInactiveNumber =
      createMetric<arangodb_replication2_replicated_log_inactive_number>(
          metricsRegistry);
  replicatedLogLeaderTookOverNumber =
      createMetric<arangodb_replication2_replicated_log_leader_took_over_total>(
          metricsRegistry);
  replicatedLogStartedFollowingNumber = createMetric<
      arangodb_replication2_replicated_log_started_following_total>(
      metricsRegistry);
  replicatedLogInsertsBytes =
      createMetric<arangodb_replication2_replicated_log_inserts_bytes>(
          metricsRegistry);
  replicatedLogInsertsRtt =
      createMetric<arangodb_replication2_replicated_log_inserts_rtt>(
          metricsRegistry);
  replicatedLogNumberAcceptedEntries = createMetric<
      arangodb_replication2_replicated_log_number_accepted_entries_total>(
      metricsRegistry);
  replicatedLogNumberCommittedEntries = createMetric<
      arangodb_replication2_replicated_log_number_committed_entries_total>(
      metricsRegistry);
  replicatedLogNumberMetaEntries = createMetric<
      arangodb_replication2_replicated_log_number_meta_entries_total>(
      metricsRegistry);
  replicatedLogNumberCompactedEntries = createMetric<
      arangodb_replication2_replicated_log_number_compacted_entries_total>(
      metricsRegistry);

  leaderNumInMemoryEntries =
      createMetric<arangodb_replication2_leader_in_memory_entries>(
          metricsRegistry);
  leaderNumInMemoryBytes =
      createMetric<arangodb_replication2_leader_in_memory_bytes>(
          metricsRegistry);

  replicatedLogAppendEntriesNumEntries = createMetric<
      arangodb_replication2_replicated_log_append_entries_num_entries>(
      metricsRegistry);
  replicatedLogAppendEntriesSize =
      createMetric<arangodb_replication2_replicated_log_append_entries_size>(
          metricsRegistry);
  replicatedLogFollowerEntryDropCount = createMetric<
      arangodb_replication2_replicated_log_follower_entry_drop_total>(
      metricsRegistry);
  replicatedLogLeaderAppendEntriesErrorCount = createMetric<
      arangodb_replication2_replicated_log_leader_append_entries_error_total>(
      metricsRegistry);
}

}  // namespace arangodb::replication2::replicated_log
