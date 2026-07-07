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

ReplicatedLogMetrics::ReplicatedLogMetrics(
    metrics::IRegistry& metricsRegistry) {
  replicatedLogNumber =
      &metricsRegistry.add(arangodb_replication2_replicated_log_number{});
  replicatedLogAppendEntriesRttUs = &metricsRegistry.add(
      arangodb_replication2_replicated_log_append_entries_rtt{});
  replicatedLogFollowerAppendEntriesRtUs = &metricsRegistry.add(
      arangodb_replication2_replicated_log_follower_append_entries_rt{});
  replicatedLogCreationNumber = &metricsRegistry.add(
      arangodb_replication2_replicated_log_creation_total{});
  replicatedLogDeletionNumber = &metricsRegistry.add(
      arangodb_replication2_replicated_log_deletion_total{});
  replicatedLogLeaderNumber = &metricsRegistry.add(
      arangodb_replication2_replicated_log_leader_number{});
  replicatedLogFollowerNumber = &metricsRegistry.add(
      arangodb_replication2_replicated_log_follower_number{});
  replicatedLogInactiveNumber = &metricsRegistry.add(
      arangodb_replication2_replicated_log_inactive_number{});
  replicatedLogLeaderTookOverNumber = &metricsRegistry.add(
      arangodb_replication2_replicated_log_leader_took_over_total{});
  replicatedLogStartedFollowingNumber = &metricsRegistry.add(
      arangodb_replication2_replicated_log_started_following_total{});
  replicatedLogInsertsBytes = &metricsRegistry.add(
      arangodb_replication2_replicated_log_inserts_bytes{});
  replicatedLogInsertsRtt =
      &metricsRegistry.add(arangodb_replication2_replicated_log_inserts_rtt{});
  replicatedLogNumberAcceptedEntries = &metricsRegistry.add(
      arangodb_replication2_replicated_log_number_accepted_entries_total{});
  replicatedLogNumberCommittedEntries = &metricsRegistry.add(
      arangodb_replication2_replicated_log_number_committed_entries_total{});
  replicatedLogNumberMetaEntries = &metricsRegistry.add(
      arangodb_replication2_replicated_log_number_meta_entries_total{});
  replicatedLogNumberCompactedEntries = &metricsRegistry.add(
      arangodb_replication2_replicated_log_number_compacted_entries_total{});
  leaderNumInMemoryEntries =
      &metricsRegistry.add(arangodb_replication2_leader_in_memory_entries{});
  leaderNumInMemoryBytes =
      &metricsRegistry.add(arangodb_replication2_leader_in_memory_bytes{});
  replicatedLogAppendEntriesNumEntries = &metricsRegistry.add(
      arangodb_replication2_replicated_log_append_entries_num_entries{});
  replicatedLogAppendEntriesSize = &metricsRegistry.add(
      arangodb_replication2_replicated_log_append_entries_size{});
  replicatedLogFollowerEntryDropCount = &metricsRegistry.add(
      arangodb_replication2_replicated_log_follower_entry_drop_total{});
  replicatedLogLeaderAppendEntriesErrorCount = &metricsRegistry.add(
      arangodb_replication2_replicated_log_leader_append_entries_error_total{});
}

}  // namespace arangodb::replication2::replicated_log
