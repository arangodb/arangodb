////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_AQL_RESOURCE_USAGE_H
#define ARANGOD_AQL_RESOURCE_USAGE_H 1

#include "Basics/Common.h"
#include "Basics/Exceptions.h"
#include "Basics/debugging.h"
#include "Basics/system-compiler.h"

#include <algorithm>
#include <atomic>

namespace arangodb {
namespace aql {

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
  ResourceMonitor() : currentResources(), maxMemoryUsage(0) {}

  void setMemoryLimit(size_t value) { maxMemoryUsage = value; }

  size_t memoryLimit() const { return maxMemoryUsage; }

  inline void increaseMemoryUsage(size_t value) {
    size_t current = currentResources.memoryUsage.fetch_add(value, std::memory_order_relaxed);
    current += value;

    if (maxMemoryUsage > 0 && ADB_UNLIKELY(current > maxMemoryUsage)) {
      currentResources.memoryUsage.fetch_sub(value, std::memory_order_relaxed);
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_RESOURCE_LIMIT, "query would use more memory than allowed");
    }
    
    size_t peak = currentResources.peakMemoryUsage.load(std::memory_order_relaxed);
    while (peak < current) {
      if (currentResources.peakMemoryUsage.compare_exchange_weak(peak, current,
                                                                 std::memory_order_release,
                                                                 std::memory_order_relaxed)) {
        break;
      }
    }
  }

  inline void decreaseMemoryUsage(size_t value) noexcept {
    TRI_ASSERT(currentResources.memoryUsage >= value);
    currentResources.memoryUsage -= value;
  }

  void clear() { currentResources.clear(); }
  
  size_t peakMemoryUsage() const {
    return currentResources.peakMemoryUsage.load(std::memory_order_relaxed);
  }
  
private:

  ResourceUsage currentResources;
  std::size_t maxMemoryUsage;
};

}  // namespace aql
}  // namespace arangodb

#endif
