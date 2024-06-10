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
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include <cstdint>
#include <array>

#include "Metrics/Fwd.h"

namespace arangodb {

struct SchedulerMetrics {
  metrics::Gauge<uint64_t>& _metricsQueueLength;
  metrics::Counter& _metricsJobsDoneTotal;
  metrics::Counter& _metricsJobsSubmittedTotal;
  metrics::Counter& _metricsJobsDequeuedTotal;
  metrics::Gauge<uint64_t>& _metricsNumAwakeThreads;
  metrics::Gauge<uint64_t>& _metricsNumWorkingThreads;
  metrics::Gauge<uint64_t>& _metricsNumWorkerThreads;
  metrics::Gauge<uint64_t>& _metricsNumDetachedThreads;
  metrics::Gauge<uint64_t>& _metricsStackMemoryWorkerThreads;
  metrics::Gauge<int64_t>& _schedulerQueueMemory;

  metrics::Counter& _metricsHandlerTasksCreated;
  metrics::Counter& _metricsThreadsStarted;
  metrics::Counter& _metricsThreadsStopped;
  metrics::Counter& _metricsQueueFull;
  metrics::Counter& _metricsQueueTimeViolations;
  metrics::Gauge<uint64_t>& _ongoingLowPriorityGauge;

  /// @brief amount of time it took for the last low prio item to be dequeued
  /// (time between queuing and dequeing) [ms].
  /// this metric is only updated probabilistically
  metrics::Gauge<uint64_t>& _metricsLastLowPriorityDequeueTime;

  std::array<metrics::Histogram<metrics::LogScale<double>>*, 4>
      _metricsDequeueTimes;
  std::array<metrics::Gauge<uint64_t>*, 4> _metricsQueueLengths;

  SchedulerMetrics(metrics::MetricsFeature& metrics);
};

}  // namespace arangodb
