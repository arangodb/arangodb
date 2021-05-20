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

#include "ReplicatedLogMetricsMock.h"

using namespace arangodb;

template <typename T>
auto buildMetric() {
  return std::dynamic_pointer_cast<typename T::metric_t>(T{}.build());
}

ReplicatedLogMetricsMock::ReplicatedLogMetricsMock(ReplicatedLogMetricsMockContainer&& metricsContainer)
    : ReplicatedLogMetrics(*metricsContainer.replicatedLogNumber,
                           *metricsContainer.replicatedLogAppendEntriesRttUs,
                           *metricsContainer.replicatedLogFollowerAppendEntriesRtUs),
      metricsContainer(std::move(metricsContainer)) {}

ReplicatedLogMetricsMockContainer::ReplicatedLogMetricsMockContainer()
    : replicatedLogNumber(buildMetric<arangodb_replication2_replicated_log_number>()),
      replicatedLogAppendEntriesRttUs(
          buildMetric<arangodb_replication2_replicated_log_append_entries_rtt_us>()),
      replicatedLogFollowerAppendEntriesRtUs(
          buildMetric<arangodb_replication2_replicated_log_follower_append_entries_rt_us>()) {}
