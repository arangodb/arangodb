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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Containers/SmallVector.h"
#include "Metrics/Fwd.h"

#include <cstdint>

namespace arangodb {
struct ResourceMonitor;
class RocksDBTransactionState;

// memory usage tracker for RocksDB methods.
// accumulates memory usage changes locally, and only publishes them to a
// ResourceMonitor and/or metric in case the diff since the last publishing
// is bigger than a certain threshold.
// this is done to save a lot of updates on the ResourceMonitor's/metric's
// underlying atomic variables.
class RocksDBMethodsMemoryTracker {
 public:
  RocksDBMethodsMemoryTracker(RocksDBMethodsMemoryTracker const&) = delete;
  RocksDBMethodsMemoryTracker& operator=(RocksDBMethodsMemoryTracker const&) =
      delete;

  explicit RocksDBMethodsMemoryTracker(RocksDBTransactionState* state,
                                       metrics::Gauge<std::uint64_t>* _metric,
                                       std::uint64_t reportGranularity);

  TEST_VIRTUAL ~RocksDBMethodsMemoryTracker();

  void reset() noexcept;

  void increaseMemoryUsage(std::uint64_t value);

  void decreaseMemoryUsage(std::uint64_t value) noexcept;

  void setSavePoint();

  void rollbackToSavePoint();

  void popSavePoint() noexcept;

  void beginQuery(std::shared_ptr<ResourceMonitor> resourceMonitor);

  void endQuery() noexcept;

  std::uint64_t memoryUsage() const noexcept;

  static constexpr std::uint64_t kDefaultGranularity = 8192;

 protected:
  void publish(bool force);

  std::uint64_t _memoryUsage;
  std::uint64_t _memoryUsageAtBeginQuery;

  // last value we published to the metric ourselves. we keep track
  // of this so we only need to update the metric if our current
  // memory usage differs by more than the threshold from
  // what we already posted to the metric. we do this to save lots
  // of metrics updates with very small increments/decrements, which
  // would provide little value and would only lead to contention on
  // the metric's underlying atomic value.
  std::uint64_t _lastPublishedValueMetric;
  std::uint64_t _lastPublishedValueResourceMonitor;

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  // we need these two members only to track the memory usage
  // accurately, without any delays caused by publishing. we need
  // this for testing only.
  std::uint64_t _lastPublishedValue;
  RocksDBTransactionState* _state;
#endif

  // publish only memory usage differences if memory usage changed
  // by this many bytes since our last update to the metric.
  // this is to avoid too frequent metrics updates and potential
  // contention on ths metric's underlying atomic value.
  std::uint64_t _reportGranularity;

  // the underlying metric to publish to. can be a nullptr!
  metrics::Gauge<std::uint64_t>* _metric;

  // the underlying ResourceMonitor to publish to. can be a nullptr!
  std::shared_ptr<ResourceMonitor> _resourceMonitor;

  containers::SmallVector<std::uint64_t, 4> _savePoints;
};

}  // namespace arangodb
