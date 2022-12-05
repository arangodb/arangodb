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

#pragma once

#include "Metrics/Fwd.h"

#include <chrono>
#include <cstdint>
#include <memory>

namespace arangodb::replication2::replicated_log {

struct ReplicatedLogMetrics {
  metrics::Gauge<uint64_t>* replicatedLogNumber;
  metrics::Histogram<metrics::LogScale<std::uint64_t>>*
      replicatedLogAppendEntriesRttUs;
  metrics::Histogram<metrics::LogScale<std::uint64_t>>*
      replicatedLogFollowerAppendEntriesRtUs;
  metrics::Counter* replicatedLogCreationNumber;
  metrics::Counter* replicatedLogDeletionNumber;
  metrics::Gauge<std::uint64_t>* replicatedLogLeaderNumber;
  metrics::Gauge<std::uint64_t>* replicatedLogFollowerNumber;
  metrics::Gauge<std::uint64_t>* replicatedLogInactiveNumber;
  metrics::Counter* replicatedLogLeaderTookOverNumber;
  metrics::Counter* replicatedLogStartedFollowingNumber;
  metrics::Histogram<metrics::LogScale<std::uint64_t>>*
      replicatedLogInsertsBytes;
  metrics::Histogram<metrics::LogScale<std::uint64_t>>* replicatedLogInsertsRtt;

  metrics::Counter* replicatedLogNumberAcceptedEntries;
  metrics::Counter* replicatedLogNumberCommittedEntries;
  metrics::Counter* replicatedLogNumberMetaEntries;
  metrics::Counter* replicatedLogNumberCompactedEntries;
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
