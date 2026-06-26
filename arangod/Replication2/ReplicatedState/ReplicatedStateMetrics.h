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

#pragma once

#include "Metrics/Fwd.h"

#include <chrono>
#include <cstdint>
#include <memory>
#include <string_view>

namespace arangodb::replication2::replicated_state {

struct ReplicatedStateMetrics {
  explicit ReplicatedStateMetrics(metrics::IRegistry& metricsRegistry,
                                  std::string_view impl);

 private:
  template<typename Builder, bool mock = false>
  static auto createMetric(metrics::IRegistry* metricsRegistry,
                           std::string_view impl) -> typename Builder::MetricT*;

 protected:
  template<typename MFP,
           std::enable_if_t<std::is_same_v<metrics::IRegistry*, MFP> ||
                                std::is_null_pointer_v<MFP>,
                            int> = 0,
           bool mock = std::is_null_pointer_v<MFP>>
  explicit ReplicatedStateMetrics(MFP metricsRegistry, std::string_view impl);

 public:
  metrics::Gauge<uint64_t>* const replicatedStateNumber;
  metrics::Gauge<uint64_t>* const replicatedStateNumberLeaders;
  metrics::Gauge<uint64_t>* const replicatedStateNumberFollowers;

  metrics::Histogram<metrics::LogScale<std::uint64_t>>* const
      replicatedStateApplyEntriesRtt;
  metrics::Histogram<metrics::LogScale<std::uint64_t>>* const
      replicatedStateRecoverEntriesRtt;
  metrics::Histogram<metrics::LogScale<std::uint64_t>>* const
      replicatedStateAcquireSnapshotRtt;

  metrics::Gauge<uint64_t>* const replicatedStateNumberWaitingForSnapshot;
  metrics::Gauge<uint64_t>* const replicatedStateNumberWaitingForLeader;
  metrics::Gauge<uint64_t>* const replicatedStateNumberWaitingForRecovery;

  metrics::Counter* const replicatedStateNumberAppliedEntries;
  metrics::Counter* const replicatedStateNumberProcessedEntries;

  metrics::Counter* const replicatedStateNumberAcquireSnapshotErrors;
  metrics::Counter* const replicatedStateNumberApplyEntriesErrors;

  metrics::Gauge<std::uint64_t>* const replicatedStateApplyDebt;
};

}  // namespace arangodb::replication2::replicated_state
