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

namespace {
// a global shared instance for tracking all resources
arangodb::GlobalResourceMonitor instance;
}

namespace arangodb {

/// @brief set the global memory limit
void GlobalResourceMonitor::memoryLimit(std::size_t value) noexcept {
  _limit = value;
}
  
/// @brief return the global memory limit
std::size_t GlobalResourceMonitor::memoryLimit() const noexcept {
  return _limit;
}
  
/// @brief return the current global memory usage
std::size_t GlobalResourceMonitor::current() const noexcept {
  return _current.load(std::memory_order_relaxed);
}

/// @brief the current memory usage to zero
void GlobalResourceMonitor::clear() noexcept {
  _current = 0;
}
  
/// @brief increase global memory usage by <value> bytes. if increasing exceeds the
/// memory limit, does not perform the increase and returns false. if increasing
/// succeds, the global value is modified and true is returned
bool GlobalResourceMonitor::increaseMemoryUsage(std::size_t value) noexcept {
  std::size_t previous = _current.fetch_add(value, std::memory_order_relaxed);
    
  if (_limit > 0 && ADB_UNLIKELY(previous + value > _limit)) {
    // the allocation would exceed the global maximum value, so we need to roll back.
    // revert the change to the global counter
    _current.fetch_sub(value, std::memory_order_relaxed);
    return false;
  }

  return true;
}
  
/// @brief decrease current global memory usage by <value> bytes. will not throw
void GlobalResourceMonitor::decreaseMemoryUsage(std::size_t value) noexcept {
  [[maybe_unused]] std::size_t previous = _current.fetch_sub(value, std::memory_order_relaxed);
  TRI_ASSERT(previous >= value);
}
  
/// @brief returns a reference to a global shared instance
/*static*/ GlobalResourceMonitor& GlobalResourceMonitor::instance() noexcept {
  return ::instance;
}

}
