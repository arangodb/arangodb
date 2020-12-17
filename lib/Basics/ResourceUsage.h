////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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

#include <algorithm>

namespace arangodb {

struct ResourceUsage {
  constexpr ResourceUsage() 
      : memoryUsage(0), 
        peakMemoryUsage(0) {}

  ResourceUsage(ResourceUsage const& other) = default;

  void clear() { 
    memoryUsage = 0; 
    peakMemoryUsage = 0;
  }

  size_t memoryUsage;
  size_t peakMemoryUsage;
};

struct ResourceMonitor {
  ResourceMonitor() = default;

  explicit ResourceMonitor(ResourceUsage const& maxResources)
      : currentResources(), maxResources(maxResources) {}

  ~ResourceMonitor();

  void setMemoryLimit(size_t value) { maxResources.memoryUsage = value; }

  inline void increaseMemoryUsage(size_t value) {
    currentResources.memoryUsage += value;

    if (maxResources.memoryUsage > 0 &&
        ADB_UNLIKELY(currentResources.memoryUsage > maxResources.memoryUsage)) {
      currentResources.memoryUsage -= value;
      THROW_ARANGO_EXCEPTION(TRI_ERROR_RESOURCE_LIMIT);
    }

    currentResources.peakMemoryUsage = std::max(currentResources.memoryUsage, currentResources.peakMemoryUsage);
  }

  inline void decreaseMemoryUsage(size_t value) noexcept {
    TRI_ASSERT(currentResources.memoryUsage >= value);
    currentResources.memoryUsage -= value;
  }

  void clear() { currentResources.clear(); }

  ResourceUsage currentResources;
  ResourceUsage maxResources;
};

/// @brief RAII object for temporary resource tracking
/// will track the resource usage on creation, and untrack it
/// on destruction
class ResourceUsageScope {
 public:
  ResourceUsageScope(ResourceUsageScope const&) = delete;
  ResourceUsageScope& operator=(ResourceUsageScope const&) = delete;

  explicit ResourceUsageScope(ResourceMonitor* resourceMonitor, size_t value)
      : _resourceMonitor(resourceMonitor), _value(value) {
    // may throw
    increase(_value);
  }

  ~ResourceUsageScope() {
    decrease(_value);
  }

  inline void increase(size_t value) {
    if (value > 0) {
      // may throw
      _resourceMonitor->increaseMemoryUsage(value);
    }
  }
  
  inline void decrease(size_t value) noexcept {
    if (value > 0) {
      _resourceMonitor->decreaseMemoryUsage(value);
    }
  }

  void steal() noexcept {
    _value = 0;
  }

 private:
  ResourceMonitor* _resourceMonitor;
  size_t _value;
};

}  // namespace arangodb

#endif
