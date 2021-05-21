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

#include "Replication2/ReplicatedLogMetricsDeclarations.h"
#include "RestServer/Metrics.h"
#include "RestServer/MetricsFeature.h"

using namespace arangodb::replication2::replicated_log;

ReplicatedLogMetrics::ReplicatedLogMetrics(arangodb::MetricsFeature& metricsFeature)
    : ReplicatedLogMetrics(&metricsFeature) {}

template <typename Builder, bool mock>
auto ReplicatedLogMetrics::createMetric(arangodb::MetricsFeature* metricsFeature)
    -> std::shared_ptr<typename Builder::metric_t> {
  TRI_ASSERT((metricsFeature == nullptr) == mock);
  if constexpr (!mock) {
    return metricsFeature->addShared(Builder{});
  } else {
    return std::dynamic_pointer_cast<typename Builder::metric_t>(Builder{}.build());
  }
}

template <typename MFP, std::enable_if_t<std::is_same_v<arangodb::MetricsFeature*, MFP> || std::is_null_pointer_v<MFP>, int>, bool mock>
ReplicatedLogMetrics::ReplicatedLogMetrics(MFP metricsFeature)
    : replicatedLogNumber(
          createMetric<arangodb_replication2_replicated_log_number, mock>(metricsFeature)),
      replicatedLogAppendEntriesRttUs(
          createMetric<arangodb_replication2_replicated_log_append_entries_rtt_us, mock>(
              metricsFeature)),
      replicatedLogFollowerAppendEntriesRtUs(
          createMetric<arangodb_replication2_replicated_log_follower_append_entries_rt_us, mock>(
              metricsFeature)) {
#ifndef ARANGODB_USE_GOOGLE_TESTS
  static_assert(!mock);
  static_assert(!isNullptrT);
#endif
}

template arangodb::replication2::replicated_log::ReplicatedLogMetrics::ReplicatedLogMetrics(
    arangodb::MetricsFeature*);
template arangodb::replication2::replicated_log::ReplicatedLogMetrics::ReplicatedLogMetrics(nullptr_t);
