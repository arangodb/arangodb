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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////

#include "ReplicatedStateMetrics.h"

#include "Replication2/ReplicatedLog/ReplicatedLogMetricsDeclarations.h"
#include "Metrics/IRegistry.h"
#include "Metrics/Gauge.h"
#include "Metrics/Scale.h"

namespace arangodb {

DECLARE_GAUGE(arangodb_replication2_replicated_state_number, std::uint64_t,
              "Number of times a replicated states on this server");
DECLARE_GAUGE(
    arangodb_replication2_replicated_state_leader_number, std::uint64_t,
    "Number of times a replicated states on this server started as a leader");
DECLARE_GAUGE(
    arangodb_replication2_replicated_state_follower_number, std::uint64_t,
    "Number of times a replicated states on this server started as a follower");

struct ApplyEntriesRttScale {
  using scale_t = metrics::LogScale<std::uint64_t>;
  static scale_t scale() {
    // values in us, smallest bucket is up to 1ms, scales up to 2^16ms =~ 65s.
    return {scale_t::kSupplySmallestBucket, 2, 0, 1'000, 16};
  }
};

DECLARE_HISTOGRAM(
    arangodb_replication2_replicated_state_follower_apply_entries_rt,
    ApplyEntriesRttScale, "RT for ApplyEntries call [us]");
DECLARE_HISTOGRAM(
    arangodb_replication2_replicated_state_leader_recover_entries_rt,
    ApplyEntriesRttScale, "RT for RecoverEntries call [us]");
DECLARE_HISTOGRAM(
    arangodb_replication2_replicated_state_follower_acquire_snapshot_rt,
    ApplyEntriesRttScale, "RT for AcquireSnapshot call [us]");

DECLARE_GAUGE(
    arangodb_replication2_replicated_state_follower_waiting_for_snapshot_number,
    std::uint64_t,
    "Number of followers waiting for a snapshot transfer to complete");
DECLARE_GAUGE(
    arangodb_replication2_replicated_state_follower_waiting_for_leader_number,
    std::uint64_t,
    "Number of followers waiting for the leader to acknowledge the current "
    "term");
DECLARE_GAUGE(
    arangodb_replication2_replicated_state_leader_waiting_for_recovery_number,
    std::uint64_t, "Number of leaders waiting for recovery to be complete");

DECLARE_COUNTER(arangodb_replication2_replicated_state_applied_entries_total,
                "Number of log entries applied to the internal state");
DECLARE_COUNTER(arangodb_replication2_replicated_state_processed_entries_total,
                "Number of log entries processed by the follower");

DECLARE_COUNTER(
    arangodb_replication2_replicated_state_acquire_snapshot_errors_total,
    "Number of errors during an acquire snapshot operation");

DECLARE_COUNTER(
    arangodb_replication2_replicated_state_apply_entries_errors_total,
    "Number of errors during an apply entries operation");

DECLARE_GAUGE(arangodb_replication2_replicated_state_follower_apply_debt,
              std::uint64_t, "Number of log entries that need to be applied");

}  // namespace arangodb

namespace arangodb::replication2::replicated_state {

ReplicatedStateMetrics::ReplicatedStateMetrics(
    metrics::IRegistry& metricsRegistry, std::string_view impl)
    : replicatedStateNumber(metricsRegistry.addShared(
          arangodb_replication2_replicated_state_number{}.withLabel(
              "state_impl", impl))),
      replicatedStateNumberLeaders(metricsRegistry.addShared(
          arangodb_replication2_replicated_state_leader_number{}.withLabel(
              "state_impl", impl))),
      replicatedStateNumberFollowers(metricsRegistry.addShared(
          arangodb_replication2_replicated_state_follower_number{}.withLabel(
              "state_impl", impl))),
      replicatedStateApplyEntriesRtt(metricsRegistry.addShared(
          arangodb_replication2_replicated_state_follower_apply_entries_rt{}
              .withLabel("state_impl", impl))),

      replicatedStateRecoverEntriesRtt(metricsRegistry.addShared(
          arangodb_replication2_replicated_state_leader_recover_entries_rt{}
              .withLabel("state_impl", impl))),
      replicatedStateAcquireSnapshotRtt(metricsRegistry.addShared(
          arangodb_replication2_replicated_state_follower_acquire_snapshot_rt{}
              .withLabel("state_impl", impl))),

      replicatedStateNumberWaitingForSnapshot(metricsRegistry.addShared(
          arangodb_replication2_replicated_state_follower_waiting_for_snapshot_number{}
              .withLabel("state_impl", impl))),
      replicatedStateNumberWaitingForLeader(metricsRegistry.addShared(
          arangodb_replication2_replicated_state_follower_waiting_for_leader_number{}
              .withLabel("state_impl", impl))),
      replicatedStateNumberWaitingForRecovery(metricsRegistry.addShared(
          arangodb_replication2_replicated_state_leader_waiting_for_recovery_number{}
              .withLabel("state_impl", impl))),

      replicatedStateNumberAppliedEntries(metricsRegistry.addShared(
          arangodb_replication2_replicated_state_applied_entries_total{}
              .withLabel("state_impl", impl))),
      replicatedStateNumberProcessedEntries(metricsRegistry.addShared(
          arangodb_replication2_replicated_state_processed_entries_total{}
              .withLabel("state_impl", impl))),

      replicatedStateNumberAcquireSnapshotErrors(metricsRegistry.addShared(
          arangodb_replication2_replicated_state_acquire_snapshot_errors_total{}
              .withLabel("state_impl", impl))),
      replicatedStateNumberApplyEntriesErrors(metricsRegistry.addShared(
          arangodb_replication2_replicated_state_apply_entries_errors_total{}
              .withLabel("state_impl", impl))),
      replicatedStateApplyDebt(metricsRegistry.addShared(
          arangodb_replication2_replicated_state_follower_apply_debt{}
              .withLabel("state_impl", impl))) {}

}  // namespace arangodb::replication2::replicated_state