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
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Metrics/Fwd.h"

#include <chrono>
#include <cstdint>
#include <memory>

namespace arangodb::pregel {

struct PregelMetrics {
  explicit PregelMetrics(metrics::MetricsFeature& metricsFeature);

 private:
  template<typename Builder, bool mock = false>
  static auto createMetric(metrics::MetricsFeature* metricsFeature)
      -> std::shared_ptr<typename Builder::MetricT>;

 protected:
  template<typename MFP,
           std::enable_if_t<std::is_same_v<metrics::MetricsFeature*, MFP> ||
                                std::is_null_pointer_v<MFP>,
                            int> = 0,
           bool mock = std::is_null_pointer_v<MFP>>
  explicit PregelMetrics(MFP metricsFeature);

 public:
  std::shared_ptr<metrics::Gauge<uint64_t>> const pregelConductorsNumber;
  std::shared_ptr<metrics::Gauge<uint64_t>> const pregelConductorsLoadingNumber;
  std::shared_ptr<metrics::Gauge<uint64_t>> const pregelConductorsRunningNumber;
  std::shared_ptr<metrics::Gauge<uint64_t>> const pregelConductorsStoringNumber;

  std::shared_ptr<metrics::Gauge<uint64_t>> const pregelWorkersNumber;
  std::shared_ptr<metrics::Gauge<uint64_t>> const pregelWorkersLoadingNumber;
  std::shared_ptr<metrics::Gauge<uint64_t>> const pregelWorkersRunningNumber;
  std::shared_ptr<metrics::Gauge<uint64_t>> const pregelWorkersStoringNumber;

  std::shared_ptr<metrics::Counter> const pregelMessagesSent;
  std::shared_ptr<metrics::Counter> const pregelMessagesReceived;

  std::shared_ptr<metrics::Gauge<uint64_t>> const pregelNumberOfThreads;

  std::shared_ptr<metrics::Gauge<uint64_t>> const pregelMemoryUsedForGraph;
};

}  // namespace arangodb::pregel
