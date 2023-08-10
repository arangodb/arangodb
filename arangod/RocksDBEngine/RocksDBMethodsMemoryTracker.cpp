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

#include "RocksDBMethodsMemoryTracker.h"

#include "Basics/ResourceUsage.h"
#include "Metrics/Gauge.h"

namespace arangodb {

MemoryTrackerBase::MemoryTrackerBase()
    : _memoryUsage(0), _memoryUsageAtBeginQuery(0) {}

MemoryTrackerBase::~MemoryTrackerBase() {
  TRI_ASSERT(_memoryUsage == 0);
  TRI_ASSERT(_memoryUsageAtBeginQuery == 0);
}

void MemoryTrackerBase::reset() noexcept {
  _memoryUsage = 0;
  _memoryUsageAtBeginQuery = 0;
  _savePoints.clear();
}

void MemoryTrackerBase::increaseMemoryUsage(std::uint64_t value) {
  _memoryUsage += value;
}

void MemoryTrackerBase::decreaseMemoryUsage(std::uint64_t value) noexcept {
  TRI_ASSERT(_memoryUsage >= value);
  _memoryUsage -= value;
}

void MemoryTrackerBase::setSavePoint() { _savePoints.push_back(_memoryUsage); }

void MemoryTrackerBase::rollbackToSavePoint() noexcept {
  TRI_ASSERT(!_savePoints.empty());
  _memoryUsage = _savePoints.back();
  _savePoints.pop_back();
}

void MemoryTrackerBase::popSavePoint() noexcept {
  TRI_ASSERT(!_savePoints.empty());
  _savePoints.pop_back();
}

std::uint64_t MemoryTrackerBase::memoryUsage() const noexcept {
  return _memoryUsage;
}

// memory usage tracker for AQL transactions that tracks memory
// allocations during an AQL query. reports to the AQL query's
// ResourceMonitor.
MemoryTrackerAqlQuery::MemoryTrackerAqlQuery()
    : MemoryTrackerBase(), _resourceMonitor(nullptr) {}

MemoryTrackerAqlQuery::~MemoryTrackerAqlQuery() { reset(); }

void MemoryTrackerAqlQuery::reset() noexcept {
  // inform resource monitor first
  if (_resourceMonitor != nullptr && _memoryUsage != 0) {
    _resourceMonitor->decreaseMemoryUsage(_memoryUsage);
  }
  // reset everything
  MemoryTrackerBase::reset();
}

void MemoryTrackerAqlQuery::increaseMemoryUsage(std::uint64_t value) {
  if (value != 0) {
    // inform resource monitor first
    if (_resourceMonitor != nullptr) {
      // can throw. if it does, no harm is done
      _resourceMonitor->increaseMemoryUsage(value);
    }
    MemoryTrackerBase::increaseMemoryUsage(value);
  }
}

void MemoryTrackerAqlQuery::decreaseMemoryUsage(std::uint64_t value) noexcept {
  if (value != 0) {
    // inform resource monitor
    if (_resourceMonitor != nullptr) {
      _resourceMonitor->decreaseMemoryUsage(value);
    }
    MemoryTrackerBase::decreaseMemoryUsage(value);
  }
}

void MemoryTrackerAqlQuery::rollbackToSavePoint() noexcept {
  auto value = _memoryUsage;
  // this will reset _memoryUsage
  MemoryTrackerBase::rollbackToSavePoint();

  // inform resource monitor
  if (_resourceMonitor != nullptr) {
    if (value > _memoryUsage) {
      // this should be the usual case, that after popping a
      // SavePoint we will use _less_ memory
      _resourceMonitor->decreaseMemoryUsage(value - _memoryUsage);
    } else {
      // somewhat unexpected case
      _resourceMonitor->increaseMemoryUsage(_memoryUsage - value);
    }
  }
}

void MemoryTrackerAqlQuery::beginQuery(ResourceMonitor* resourceMonitor) {
  TRI_ASSERT(resourceMonitor != nullptr);
  TRI_ASSERT(_resourceMonitor == nullptr);
  TRI_ASSERT(_memoryUsageAtBeginQuery == 0);
  _resourceMonitor = resourceMonitor;
  if (_resourceMonitor != nullptr) {
    _memoryUsageAtBeginQuery = _memoryUsage;
  }
}

void MemoryTrackerAqlQuery::endQuery() noexcept {
  TRI_ASSERT(_resourceMonitor != nullptr);

  if (_resourceMonitor != nullptr) {
    if (_memoryUsage >= _memoryUsageAtBeginQuery) {
      _resourceMonitor->decreaseMemoryUsage(_memoryUsage -
                                            _memoryUsageAtBeginQuery);
    } else {
      _resourceMonitor->increaseMemoryUsage(_memoryUsageAtBeginQuery -
                                            _memoryUsage);
    }
    _memoryUsage = _memoryUsageAtBeginQuery;
    _memoryUsageAtBeginQuery = 0;
    _resourceMonitor = nullptr;
  }
}

MemoryTrackerMetric::MemoryTrackerMetric(metrics::Gauge<std::uint64_t>* metric)
    : MemoryTrackerBase(), _metric(metric), _lastPublishedValue(0) {
  TRI_ASSERT(_metric != nullptr);
}

MemoryTrackerMetric::~MemoryTrackerMetric() { reset(); }

void MemoryTrackerMetric::reset() noexcept {
  // reset everything
  MemoryTrackerBase::reset();
  publish(true);
}

void MemoryTrackerMetric::increaseMemoryUsage(std::uint64_t value) {
  if (value != 0) {
    // both of these will not throw, so we can execute them
    // in any order
    MemoryTrackerBase::increaseMemoryUsage(value);
    publish(false);
  }
}

void MemoryTrackerMetric::decreaseMemoryUsage(std::uint64_t value) noexcept {
  if (value != 0) {
    MemoryTrackerBase::decreaseMemoryUsage(value);
    publish(false);
  }
}

void MemoryTrackerMetric::rollbackToSavePoint() noexcept {
  // this will reset _memoryUsage
  MemoryTrackerBase::rollbackToSavePoint();
  publish(true);
}

void MemoryTrackerMetric::beginQuery(ResourceMonitor* /*resourceMonitor*/) {
  // note: resourceMonitor can be a nullptr when we are called
  // from RocksDBCollection::truncateWithRemovals()
  TRI_ASSERT(_memoryUsageAtBeginQuery == 0);
  _memoryUsageAtBeginQuery = _memoryUsage;
}

void MemoryTrackerMetric::endQuery() noexcept {
  _memoryUsage = _memoryUsageAtBeginQuery;
  _memoryUsageAtBeginQuery = 0;
  publish(true);
}

void MemoryTrackerMetric::publish(bool force) noexcept {
  if (!force) {
    if (_lastPublishedValue < _memoryUsage) {
      // current memory usage is higher
      force |= (_memoryUsage - _lastPublishedValue) >= kMemoryReportGranularity;
    } else if (_lastPublishedValue > _memoryUsage) {
      // current memory usage is lower
      force |= (_lastPublishedValue - _memoryUsage) >= kMemoryReportGranularity;
    }
  }
  if (force) {
    if (_lastPublishedValue < _memoryUsage) {
      // current memory usage is higher
      _metric->fetch_add(_memoryUsage - _lastPublishedValue);
    } else if (_lastPublishedValue > _memoryUsage) {
      // current memory usage is lower
      _metric->fetch_sub(_lastPublishedValue - _memoryUsage);
    }
    _lastPublishedValue = _memoryUsage;
  }
}

}  // namespace arangodb
