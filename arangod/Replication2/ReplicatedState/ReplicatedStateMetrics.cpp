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

using namespace arangodb::replication2::replicated_state;

ReplicatedStateMetrics::ReplicatedStateMetrics(
    metrics::IRegistry& metricsRegistry, std::string_view impl)
    : ReplicatedStateMetrics(&metricsRegistry, impl) {}

template<typename Builder, bool mock>
auto ReplicatedStateMetrics::createMetric(metrics::IRegistry* metricsRegistry,
                                          std::string_view impl) ->
    typename Builder::MetricT* {
  TRI_ASSERT((metricsRegistry == nullptr) == mock);
  if constexpr (!mock) {
    return &metricsRegistry->add(Builder{}.withLabel("state_impl", impl));
  } else {
    static std::vector<std::shared_ptr<typename Builder::MetricT>> metrics;
    auto ptr =
        std::dynamic_pointer_cast<typename Builder::MetricT>(Builder{}.build());
    return metrics.emplace_back(ptr).get();
  }
}

template<typename MFP,
         std::enable_if_t<std::is_same_v<arangodb::metrics::IRegistry*, MFP> ||
                              std::is_null_pointer_v<MFP>,
                          int>,
         bool mock>
ReplicatedStateMetrics::ReplicatedStateMetrics(MFP metricsRegistry,
                                               std::string_view impl)
    : replicatedStateNumber(
          createMetric<arangodb_replication2_replicated_state_number, mock>(
              metricsRegistry, impl)),
      replicatedStateNumberLeaders(
          createMetric<arangodb_replication2_replicated_state_leader_number,
                       mock>(metricsRegistry, impl)),
      replicatedStateNumberFollowers(
          createMetric<arangodb_replication2_replicated_state_follower_number,
                       mock>(metricsRegistry, impl)),
      replicatedStateApplyEntriesRtt(
          createMetric<
              arangodb_replication2_replicated_state_follower_apply_entries_rt,
              mock>(metricsRegistry, impl)),

      replicatedStateRecoverEntriesRtt(
          createMetric<
              arangodb_replication2_replicated_state_leader_recover_entries_rt,
              mock>(metricsRegistry, impl)),
      replicatedStateAcquireSnapshotRtt(
          createMetric<
              arangodb_replication2_replicated_state_follower_acquire_snapshot_rt,
              mock>(metricsRegistry, impl)),

      replicatedStateNumberWaitingForSnapshot(
          createMetric<
              arangodb_replication2_replicated_state_follower_waiting_for_snapshot_number,
              mock>(metricsRegistry, impl)),
      replicatedStateNumberWaitingForLeader(
          createMetric<
              arangodb_replication2_replicated_state_follower_waiting_for_leader_number,
              mock>(metricsRegistry, impl)),
      replicatedStateNumberWaitingForRecovery(
          createMetric<
              arangodb_replication2_replicated_state_leader_waiting_for_recovery_number,
              mock>(metricsRegistry, impl)),

      replicatedStateNumberAppliedEntries(
          createMetric<
              arangodb_replication2_replicated_state_applied_entries_total,
              mock>(metricsRegistry, impl)),
      replicatedStateNumberProcessedEntries(
          createMetric<
              arangodb_replication2_replicated_state_processed_entries_total,
              mock>(metricsRegistry, impl)),

      replicatedStateNumberAcquireSnapshotErrors(
          createMetric<
              arangodb_replication2_replicated_state_acquire_snapshot_errors_total,
              mock>(metricsRegistry, impl)),
      replicatedStateNumberApplyEntriesErrors(
          createMetric<
              arangodb_replication2_replicated_state_apply_entries_errors_total,
              mock>(metricsRegistry, impl)),
      replicatedStateApplyDebt(
          createMetric<
              arangodb_replication2_replicated_state_follower_apply_debt, mock>(
              metricsRegistry, impl)) {}

template arangodb::replication2::replicated_state::ReplicatedStateMetrics::
    ReplicatedStateMetrics(arangodb::metrics::IRegistry*, std::string_view);

template arangodb::replication2::replicated_state::ReplicatedStateMetrics::
    ReplicatedStateMetrics(std::nullptr_t, std::string_view);
