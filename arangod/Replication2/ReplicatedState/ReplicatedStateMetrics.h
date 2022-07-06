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
  explicit ReplicatedStateMetrics(metrics::MetricsFeature& metricsFeature,
                                  std::string_view impl);

 private:
  template<typename Builder, bool mock = false>
  static auto createMetric(metrics::MetricsFeature* metricsFeature,
                           std::string_view impl)
      -> std::shared_ptr<typename Builder::MetricT>;

 protected:
  template<typename MFP,
           std::enable_if_t<std::is_same_v<metrics::MetricsFeature*, MFP> ||
                                std::is_null_pointer_v<MFP>,
                            int> = 0,
           bool mock = std::is_null_pointer_v<MFP>>
  explicit ReplicatedStateMetrics(MFP metricsFeature, std::string_view impl);

 public:
  std::shared_ptr<metrics::Gauge<uint64_t>> const replicatedStateNumber;
  std::shared_ptr<metrics::Gauge<uint64_t>> const replicatedStateNumberLeaders;
  std::shared_ptr<metrics::Gauge<uint64_t>> const
      replicatedStateNumberFollowers;
};

}  // namespace arangodb::replication2::replicated_state
