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

#ifndef ARANGOD_BASICS_GLOBAL_RESOURCE_MONITOR_H
#define ARANGOD_BASICS_GLOBAL_RESOURCE_MONITOR_H 1

#include "Basics/Common.h"

#include <atomic>
#include <cstddef>

namespace arangodb {

class alignas(64) GlobalResourceMonitor final {
  GlobalResourceMonitor(GlobalResourceMonitor const&) = delete;
  GlobalResourceMonitor& operator=(GlobalResourceMonitor const&) = delete;

 public:
  constexpr GlobalResourceMonitor() 
      : _current(0),
        _limit(0) {}

  /// @brief set the global memory limit
  void memoryLimit(std::size_t value) noexcept;

  /// @brief return the global memory limit
  std::size_t memoryLimit() const noexcept;

  /// @brief return the current global memory usage
  std::size_t current() const noexcept;
  
  /// @brief the current memory usage to zero
  void clear() noexcept;
  
  /// @brief increase global memory usage by <value> bytes. if increasing exceeds the
  /// memory limit, does not perform the increase and returns false. if increasing
  /// succeds, the global value is modified and true is returned
  [[nodiscard]] bool increaseMemoryUsage(std::size_t value) noexcept;
  
  /// @brief decrease current global memory usage by <value> bytes. will not throw
  void decreaseMemoryUsage(std::size_t value) noexcept;

  /// @brief returns a reference to a global shared instance
  static GlobalResourceMonitor& instance() noexcept;

 private:
  /// @brief the current combined memory usage of all tracked operations.
  /// should only very brief go above "_globalMemoryLimit" (if it goes
  /// above it, it will instantly throw an exception so that memory usage
  /// will go down again quickly).
  /// this counter is updated by local instances only if there is a substantial
  /// allocation/deallocation. it is intentionally _not_ updated on every
  /// small allocation/deallocation. the granularity for the values in this
  /// counter is bucketSize.
  std::atomic<std::size_t> _current;

  /// @brief maximum allowed global memory limit for all tracked operations combined.
  /// a value of 0 means that there will be no global limit enforced.
  std::size_t _limit;
};

}  // namespace arangodb

#endif
