////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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

#pragma once

#include "Metrics/Fwd.h"

#include <chrono>
#include <cstdint>
#include <memory>

namespace arangodb::replication2::replicated_log {

struct ReplicatedLogMetrics {
  metrics::Gauge<uint64_t>* replicatedLogNumber{nullptr};
  metrics::Histogram<metrics::LogScale<std::uint64_t>>*
      replicatedLogAppendEntriesRttUs{nullptr};
  metrics::Histogram<metrics::LogScale<std::uint64_t>>*
      replicatedLogFollowerAppendEntriesRtUs{nullptr};
  metrics::Counter* replicatedLogCreationNumber{nullptr};
  metrics::Counter* replicatedLogDeletionNumber{nullptr};
  metrics::Gauge<std::uint64_t>* replicatedLogLeaderNumber{nullptr};
  metrics::Gauge<std::uint64_t>* replicatedLogFollowerNumber{nullptr};
  // TODO This metric currently isn't populated
  metrics::Gauge<std::uint64_t>* replicatedLogInactiveNumber{nullptr};
  metrics::Gauge<std::uint64_t>* leaderNumInMemoryEntries{nullptr};
  metrics::Gauge<std::size_t>* leaderNumInMemoryBytes{nullptr};
  metrics::Counter* replicatedLogLeaderTookOverNumber{nullptr};
  metrics::Counter* replicatedLogStartedFollowingNumber{nullptr};
  metrics::Histogram<metrics::LogScale<std::uint64_t>>*
      replicatedLogInsertsBytes{nullptr};
  metrics::Histogram<metrics::LogScale<std::uint64_t>>* replicatedLogInsertsRtt{
      nullptr};
  metrics::Histogram<metrics::LogScale<std::uint64_t>>*
      replicatedLogAppendEntriesNumEntries{nullptr};
  metrics::Histogram<metrics::LogScale<std::uint64_t>>*
      replicatedLogAppendEntriesSize{nullptr};
  metrics::Counter* replicatedLogFollowerEntryDropCount{nullptr};
  metrics::Counter* replicatedLogLeaderAppendEntriesErrorCount{nullptr};

  metrics::Counter* replicatedLogNumberAcceptedEntries{nullptr};
  metrics::Counter* replicatedLogNumberCommittedEntries{nullptr};
  metrics::Counter* replicatedLogNumberMetaEntries{nullptr};
  // TODO This metric currently isn't populated
  metrics::Counter* replicatedLogNumberCompactedEntries{nullptr};
};

template<bool Mock>
struct ReplicatedLogMetricsIndirect : ReplicatedLogMetrics {
  explicit ReplicatedLogMetricsIndirect(
      metrics::MetricsFeature* metricsFeature);

 private:
  template<typename Builder>
  static auto createMetric(metrics::MetricsFeature* metricsFeature) ->
      typename Builder::MetricT*;
};

}  // namespace arangodb::replication2::replicated_log
