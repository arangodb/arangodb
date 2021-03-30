////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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

#include "Basics/GlobalResourceMonitor.h"
#include "Basics/debugging.h"
#include "Basics/system-compiler.h"
#include "Logger/LogMacros.h"

namespace {
// a global shared instance for tracking all resources
arangodb::GlobalResourceMonitor instance;
}

namespace arangodb {

/// @brief set the global memory limit
void GlobalResourceMonitor::memoryLimit(std::int64_t value) noexcept {
  TRI_ASSERT(value >= 0);
  _limit = value;
}
  
/// @brief return the global memory limit
std::int64_t GlobalResourceMonitor::memoryLimit() const noexcept {
  return _limit;
}
  
/// @brief return the current global memory usage
std::int64_t GlobalResourceMonitor::current() const noexcept {
  return _current.load(std::memory_order_relaxed);
}

/// @brief number of times the global and any local limits were reached
GlobalResourceMonitor::Stats GlobalResourceMonitor::stats() const noexcept {
  Stats stats;
  stats.globalLimitReached = _globalLimitReachedCounter.load(std::memory_order_relaxed);
  stats.localLimitReached = _localLimitReachedCounter.load(std::memory_order_relaxed);
  return stats;
}
  
/// @brief increase the counter for global memory limit violations
void GlobalResourceMonitor::trackGlobalViolation() noexcept {
  _globalLimitReachedCounter.fetch_add(1, std::memory_order_relaxed);
}

  /// @brief increase the counter for local memory limit violations
void GlobalResourceMonitor::trackLocalViolation() noexcept {
  _localLimitReachedCounter.fetch_add(1, std::memory_order_relaxed);
}
  
/// @brief increase global memory usage by <value> bytes. if increasing exceeds the
/// memory limit, does not perform the increase and returns false. if increasing
/// succeeds, the global value is modified and true is returned
bool GlobalResourceMonitor::increaseMemoryUsage(std::int64_t value) noexcept {
  TRI_ASSERT(value >= 0);
  if (_limit == 0) {
    // since we have no limit, we can simply use fetch-add for the increment
    _current.fetch_add(value, std::memory_order_relaxed);
  } else {
    // we only want to perform the update if we don't exceed the limit!
    std::int64_t cur = _current.load(std::memory_order_relaxed);
    std::int64_t next;
    do {
      next = cur + value;
      if (ADB_UNLIKELY(next > _limit)) {
        return false;
      }
    } while (!_current.compare_exchange_weak(cur, next, std::memory_order_relaxed));
  }
  
  return true;
}
  
/// @brief decrease current global memory usage by <value> bytes. will not throw
void GlobalResourceMonitor::decreaseMemoryUsage(std::int64_t value) noexcept {
  TRI_ASSERT(value >= 0);
  _current.fetch_sub(value, std::memory_order_relaxed);
}
 
void GlobalResourceMonitor::forceUpdateMemoryUsage(std::int64_t value) noexcept {
  _current.fetch_add(value, std::memory_order_relaxed);
}

/// @brief returns a reference to a global shared instance
/*static*/ GlobalResourceMonitor& GlobalResourceMonitor::instance() noexcept {
  return ::instance;
}

}
