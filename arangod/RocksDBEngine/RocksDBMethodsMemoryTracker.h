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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Containers/SmallVector.h"
#include "Metrics/Gauge.h"

#include <cstdint>

namespace arangodb {
struct ResourceMonitor;

struct RocksDBMethodsMemoryTracker {
  virtual ~RocksDBMethodsMemoryTracker() {}
  virtual void reset() noexcept = 0;
  virtual void increaseMemoryUsage(std::uint64_t value) = 0;
  virtual void decreaseMemoryUsage(std::uint64_t value) noexcept = 0;

  virtual void setSavePoint() = 0;
  virtual void rollbackToSavePoint() noexcept = 0;
  virtual void popSavePoint() noexcept = 0;

  virtual size_t memoryUsage() const noexcept = 0;
  virtual void beginQuery(ResourceMonitor* resourceMonitor) = 0;
  virtual void endQuery() noexcept = 0;
};

// abstract base class with common functionality for other memory usage
// trackers
class MemoryTrackerBase : public RocksDBMethodsMemoryTracker {
 public:
  MemoryTrackerBase(MemoryTrackerBase const&) = delete;
  MemoryTrackerBase& operator=(MemoryTrackerBase const&) = delete;

  MemoryTrackerBase();
  ~MemoryTrackerBase();

  void reset() noexcept override;

  void increaseMemoryUsage(std::uint64_t value) override;

  void decreaseMemoryUsage(std::uint64_t value) noexcept override;

  void setSavePoint() override;

  void rollbackToSavePoint() noexcept override;

  void popSavePoint() noexcept override;

  std::uint64_t memoryUsage() const noexcept override;

 protected:
  std::uint64_t _memoryUsage;
  std::uint64_t _memoryUsageAtBeginQuery;
  containers::SmallVector<std::uint64_t, 4> _savePoints;
};

// memory usage tracker for AQL transactions that tracks memory
// allocations during an AQL query. reports to the AQL query's
// ResourceMonitor.
class MemoryTrackerAqlQuery final : public MemoryTrackerBase {
 public:
  MemoryTrackerAqlQuery();
  ~MemoryTrackerAqlQuery();

  void reset() noexcept override;

  void increaseMemoryUsage(std::uint64_t value) override;

  void decreaseMemoryUsage(std::uint64_t value) noexcept override;

  void rollbackToSavePoint() noexcept override;

  void beginQuery(ResourceMonitor* resourceMonitor) override;

  void endQuery() noexcept override;

 private:
  ResourceMonitor* _resourceMonitor;
};

// memory usage tracker for transactions that update a particular
// memory usage metric. currently used for all transactions that
// are not top-level AQL queries, and for internal transactions
// (transactions that were not explicitly initiated by users).
class MemoryTrackerMetric final : public MemoryTrackerBase {
 public:
  MemoryTrackerMetric(metrics::Gauge<std::uint64_t>* metric);
  ~MemoryTrackerMetric();

  void reset() noexcept override;

  void increaseMemoryUsage(std::uint64_t value) override;

  void decreaseMemoryUsage(std::uint64_t value) noexcept override;

  void rollbackToSavePoint() noexcept override;

  void beginQuery(ResourceMonitor* /*resourceMonitor*/) override;

  void endQuery() noexcept override;

 private:
  void publish(bool force) noexcept;

  metrics::Gauge<std::uint64_t>* _metric;

  // last value we published to the metric ourselves. we keep track
  // of this so we only need to update the metric if our current
  // memory usage differs by more than kMemoryReportGranularity from
  // what we already posted to the metric. we do this to save lots
  // of metrics updates with very small increments/decrements, which
  // would provide little value and would only lead to contention on
  // the metric's underlying atomic value.
  std::uint64_t _lastPublishedValue;

  // publish only memory usage differences if memory usage changed
  // by this many bytes since our last update to the metric.
  // this is to avoid too frequent metrics updates and potential
  // contention on ths metric's underlying atomic value.
  static constexpr std::int64_t kMemoryReportGranularity = 4096;
};

}  // namespace arangodb
