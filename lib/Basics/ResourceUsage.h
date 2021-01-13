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

#ifndef ARANGOD_BASICS_RESOURCE_USAGE_H
#define ARANGOD_BASICS_RESOURCE_USAGE_H 1

#include "Basics/Common.h"
#include "Basics/Exceptions.h"
#include "Basics/debugging.h"
#include "Basics/system-compiler.h"

#include <atomic>

namespace arangodb {

struct ResourceUsage final {
  constexpr ResourceUsage() 
      : memoryUsage(0), 
        peakMemoryUsage(0) {}

  void clear() { 
    memoryUsage = 0; 
    peakMemoryUsage = 0;
  }

  std::atomic<size_t> memoryUsage;
  std::atomic<size_t> peakMemoryUsage;
};

struct ResourceMonitor final {
  ResourceMonitor(ResourceMonitor const&) = delete;
  ResourceMonitor& operator=(ResourceMonitor const&) = delete;

  ResourceMonitor() : 
      currentResources(), 
      maxMemoryUsage(0) {}

  ~ResourceMonitor();

  void setMemoryLimit(size_t value) { maxMemoryUsage = value; }

  size_t memoryLimit() const { return maxMemoryUsage; }

  /// @brief increase memory usage by <value> bytes. may throw!
  void increaseMemoryUsage(size_t value);

  void decreaseMemoryUsage(size_t value) noexcept;

  void clear() { currentResources.clear(); }
  
  size_t peakMemoryUsage() const {
    return currentResources.peakMemoryUsage.load(std::memory_order_relaxed);
  }

  size_t currentMemoryUsage() const {
    return currentResources.memoryUsage.load(std::memory_order_relaxed);
  }

  
 private:

  ResourceUsage currentResources;
  std::size_t maxMemoryUsage;
};

/// @brief RAII object for temporary resource tracking
/// will track the resource usage on creation, and untrack it
/// on destruction, unless the responsibility is stolen
/// from it.
class ResourceUsageScope {
 public:
  ResourceUsageScope(ResourceUsageScope const&) = delete;
  ResourceUsageScope& operator=(ResourceUsageScope const&) = delete;

  /// @brief track <value> bytes of memory, may throw!
  explicit ResourceUsageScope(ResourceMonitor& resourceMonitor, size_t value);

  ~ResourceUsageScope();
  
  /// @brief steal responsibility for decreasing the memory 
  /// usage on destruction
  void steal() noexcept;

 private:
  /// @brief track <value> bytes of memory, may throw!
  void increase(size_t value);
  
  void decrease(size_t value) noexcept;

 private:
  ResourceMonitor& _resourceMonitor;
  size_t _value;
};

}  // namespace arangodb

#endif
