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
#include "RocksDBEngine/RocksDBTransactionState.h"
#include "Statistics/ServerStatistics.h"
#include "Transaction/OperationOrigin.h"

#include "Logger/LogMacros.h"

namespace arangodb {

RocksDBMethodsMemoryTracker::RocksDBMethodsMemoryTracker(
    RocksDBTransactionState* state)
    : _memoryUsage(0),
      _memoryUsageAtBeginQuery(0),
      _lastPublishedValueMetric(0),
      _lastPublishedValueResourceMonitor(0),
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
      _lastPublishedValue(0),
      _state(state),
#endif
      _metric(nullptr),
      _resourceMonitor(nullptr) {
  TRI_ASSERT(state != nullptr);

  switch (state->operationOrigin().type) {
    case transaction::OperationOrigin::Type::kAQL:
      // LOG_DEVEL << "AQL";
      _metric = nullptr;
      break;
    case transaction::OperationOrigin::Type::kREST:
      // LOG_DEVEL << "REST";
      _metric = &state->statistics()._restTransactionsMemoryUsage;
      break;
    case transaction::OperationOrigin::Type::kInternal:
      _metric = &state->statistics()._internalTransactionsMemoryUsage;
      break;
  }
}

RocksDBMethodsMemoryTracker::~RocksDBMethodsMemoryTracker() {
  reset();

  TRI_ASSERT(_memoryUsage == 0);
  TRI_ASSERT(_memoryUsageAtBeginQuery == 0);
}

void RocksDBMethodsMemoryTracker::reset() noexcept {
  // LOG_DEVEL << "RESET";
  _memoryUsage = 0;
  _memoryUsageAtBeginQuery = 0;
  _savePoints.clear();

  try {
    // this should effectively not throw, because in the destructor
    // we should only _decrease_ the memory usage, which will call the
    // noexcept decreaseMemoryUsage() function on the ResourceMonitor.
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    TRI_ASSERT(_memoryUsage <= _lastPublishedValue);
#endif
    publish(true);
  } catch (...) {
    // we should never get here.
    TRI_ASSERT(false);
  }
}

void RocksDBMethodsMemoryTracker::increaseMemoryUsage(std::uint64_t value) {
  if (value != 0) {
    // LOG_DEVEL << "INCR " << value;
    _memoryUsage += value;
    try {
      // note: publishing may throw when increasing the memory usage
      publish(false);
    } catch (...) {
      // if we caught an exception, roll back the increase to _memoryUsage
      TRI_ASSERT(_memoryUsage >= value);
      _memoryUsage -= value;
      throw;
    }
  }
}

void RocksDBMethodsMemoryTracker::decreaseMemoryUsage(
    std::uint64_t value) noexcept {
  if (value != 0) {
    // LOG_DEVEL << "DECR " << value;
    TRI_ASSERT(_memoryUsage >= value);
    _memoryUsage -= value;
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

void RocksDBMethodsMemoryTracker::setSavePoint() {
  _savePoints.push_back(_memoryUsage);
}

void RocksDBMethodsMemoryTracker::rollbackToSavePoint() {
  // note: this is effectively noexcept
  TRI_ASSERT(!_savePoints.empty());
  _memoryUsage = _savePoints.back();
  // LOG_DEVEL << "ROLLING BACK TO SAVEPOINT " << _memoryUsage;
  _savePoints.pop_back();
  publish(true);
}

void RocksDBMethodsMemoryTracker::popSavePoint() noexcept {
  TRI_ASSERT(!_savePoints.empty());
  _savePoints.pop_back();
}

void RocksDBMethodsMemoryTracker::beginQuery(ResourceMonitor* resourceMonitor) {
  // note: resourceMonitor can be a nullptr if we are called from truncate
  // LOG_DEVEL << "BEGINQUERY. RESOURCEM: " << resourceMonitor
  //          << ", MEMORYUSAGE AT BEGIN: " << _memoryUsage;
  TRI_ASSERT(_resourceMonitor == nullptr);
  TRI_ASSERT(_memoryUsageAtBeginQuery == 0);

  _resourceMonitor = resourceMonitor;
  _memoryUsageAtBeginQuery = _memoryUsage;
}

void RocksDBMethodsMemoryTracker::endQuery() noexcept {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  TRI_ASSERT(_memoryUsage >= _lastPublishedValue);
#endif
  // LOG_DEVEL << "ENDQUERY. RESOURCEM: " << _resourceMonitor
  //          << ", MEMORYUSAGE: " << _memoryUsage << ", RESETTING TO "
  //          << _memoryUsageAtBeginQuery;

  _memoryUsage = _memoryUsageAtBeginQuery;
  try {
    // this should effectively not throw, because in the endQuery()
    // call we should only _decrease_ the memory usage, which will call
    // the noexcept decreaseMemoryUsage() function on the ResourceMonitor.
    publish(true);
  } catch (...) {
    // we should never get here.
    TRI_ASSERT(false);
  }
  _memoryUsageAtBeginQuery = 0;
  _resourceMonitor = nullptr;
}

std::uint64_t RocksDBMethodsMemoryTracker::memoryUsage() const noexcept {
  return _memoryUsage;
}

void RocksDBMethodsMemoryTracker::publish(bool force) {
  force = true;
  auto mustForce = [](std::uint64_t lastPublished,
                      std::uint64_t memoryUsage) noexcept {
    if (lastPublished < memoryUsage) {
      // current memory usage is higher
      return (memoryUsage - lastPublished) >= kMemoryReportGranularity;
    } else if (lastPublished > memoryUsage) {
      // current memory usage is lower
      return (lastPublished - memoryUsage) >= kMemoryReportGranularity;
    }
    return false;
  };

  if (_resourceMonitor != nullptr) {
    TRI_ASSERT(_memoryUsage >= _memoryUsageAtBeginQuery);
    auto memoryUsage = _memoryUsage - _memoryUsageAtBeginQuery;
    if (force || mustForce(_lastPublishedValueResourceMonitor, memoryUsage)) {
      if (_lastPublishedValueResourceMonitor < memoryUsage) {
        // current memory usage is higher, so we increase.
        // note: this can throw!
        _resourceMonitor->increaseMemoryUsage(
            memoryUsage - _lastPublishedValueResourceMonitor);
      } else if (_lastPublishedValueResourceMonitor > memoryUsage) {
        // current memory usage is lower. note: this will not throw!
        _resourceMonitor->decreaseMemoryUsage(
            _lastPublishedValueResourceMonitor - memoryUsage);
      }
      // LOG_DEVEL << "PUBLISHED VALUE: " << memoryUsage;
      _lastPublishedValueResourceMonitor = memoryUsage;
    }
  }

  if (_metric != nullptr &&
      (force || mustForce(_lastPublishedValueMetric, _memoryUsage))) {
    if (_lastPublishedValueMetric < _memoryUsage) {
      _metric->fetch_add(_memoryUsage - _lastPublishedValueMetric);
    } else if (_lastPublishedValueMetric > _memoryUsage) {
      _metric->fetch_sub(_lastPublishedValueMetric - _memoryUsage);
    }
    _lastPublishedValueMetric = _memoryUsage;
  }

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  TRI_ASSERT(_state != nullptr);
  // publish to state for internal test purposes. this won't throw.
  _state->adjustMemoryUsage(static_cast<std::int64_t>(_memoryUsage) -
                            static_cast<std::int64_t>(_lastPublishedValue));
  _lastPublishedValue = _memoryUsage;
#endif
}

}  // namespace arangodb
