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
#include "Logger/LogMacros.h"
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

void MemoryTrackerBase::rollbackToSavePoint() {
  // note: this is effectively noexcept
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
    : MemoryTrackerBase(), _resourceMonitor(nullptr), _lastPublishedValue(0) {}

MemoryTrackerAqlQuery::~MemoryTrackerAqlQuery() { reset(); }

void MemoryTrackerAqlQuery::reset() noexcept {
  // reset everything
  MemoryTrackerBase::reset();
  try {
    // this should effectively not throw, because in the destructor
    // we should only _decrease_ the memory usage, which will call the
    // noexcept decreaseMemoryUsage() function on the ResourceMonitor.
    TRI_ASSERT(_memoryUsage <= _lastPublishedValue);
    publish(true);
  } catch (...) {
    // we should never get here.
    TRI_ASSERT(false);
  }
}

void MemoryTrackerAqlQuery::increaseMemoryUsage(std::uint64_t value) {
  if (value != 0) {
    MemoryTrackerBase::increaseMemoryUsage(value);
    try {
      // note: publishing may throw when increasing the memory usage
      publish(false);
    } catch (...) {
      // if we caught an exception, roll back the increase to _memoryUsage
      MemoryTrackerBase::decreaseMemoryUsage(value);
      throw;
    }
  }
}

void MemoryTrackerAqlQuery::decreaseMemoryUsage(std::uint64_t value) noexcept {
  if (value != 0) {
    MemoryTrackerBase::decreaseMemoryUsage(value);
    // note: publish does not throw for a decrease
    try {
      // this should effectively not throw, because we should only _decrease_
      // the memory usage, which will call the noexcept decreaseMemoryUsage()
      // function on the ResourceMonitor.
      publish(false);
    } catch (...) {
      // we should never get here.
      TRI_ASSERT(false);
    }
  }
}

void MemoryTrackerAqlQuery::rollbackToSavePoint() {
  // this will reset _memoryUsage
  MemoryTrackerBase::rollbackToSavePoint();
  publish(true);
}

void MemoryTrackerAqlQuery::beginQuery(ResourceMonitor* resourceMonitor) {
  // note: resourceMonitor cannot be a nullptr when we are called
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
  TRI_ASSERT(_memoryUsage >= _lastPublishedValue);
  _memoryUsage = _memoryUsageAtBeginQuery;
  _memoryUsageAtBeginQuery = 0;
  try {
    // this should effectively not throw, because in the endQuery()
    // call we should only _decrease_ the memory usage, which will call
    // the noexcept decreaseMemoryUsage() function on the ResourceMonitor.
    publish(true);
  } catch (...) {
    // we should never get here.
    TRI_ASSERT(false);
  }
  _resourceMonitor = nullptr;
}

void MemoryTrackerAqlQuery::publish(bool force) {
  if (_resourceMonitor == nullptr) {
    return;
  }
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
      // current memory usage is higher, so we increase.
      // note: this can throw!
      _resourceMonitor->increaseMemoryUsage(_memoryUsage - _lastPublishedValue);
    } else if (_lastPublishedValue > _memoryUsage) {
      // current memory usage is lower. note: this will not throw!
      try {
        _resourceMonitor->decreaseMemoryUsage(_lastPublishedValue -
                                              _memoryUsage);
      } catch (...) {
        TRI_ASSERT(false);
      }
    }
    _lastPublishedValue = _memoryUsage;
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

void MemoryTrackerMetric::rollbackToSavePoint() {
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
