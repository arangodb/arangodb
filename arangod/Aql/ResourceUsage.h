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

#ifndef ARANGOD_AQL_RESOURCE_USAGE_H
#define ARANGOD_AQL_RESOURCE_USAGE_H 1

#include "Basics/Common.h"
#include "Basics/Exceptions.h"

#include <algorithm>

namespace arangodb {
namespace aql {

struct ResourceUsage {
  constexpr ResourceUsage() 
      : memoryUsage(0), 
        peakMemoryUsage(0) {}
  ResourceUsage(ResourceUsage const& other) noexcept
      : memoryUsage(other.memoryUsage),
        peakMemoryUsage(other.peakMemoryUsage) {}

  void clear() { 
    memoryUsage = 0; 
    peakMemoryUsage = 0;
  }

  size_t memoryUsage;
  size_t peakMemoryUsage;
};

struct ResourceMonitor {
  ResourceMonitor() : currentResources(), maxResources() {}
  explicit ResourceMonitor(ResourceUsage const& maxResources)
      : currentResources(), maxResources(maxResources) {}

  void setMemoryLimit(size_t value) { maxResources.memoryUsage = value; }

  inline void increaseMemoryUsage(size_t value) {
    currentResources.memoryUsage += value;

    if (maxResources.memoryUsage > 0 &&
        ADB_UNLIKELY(currentResources.memoryUsage > maxResources.memoryUsage)) {
      currentResources.memoryUsage -= value;
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_RESOURCE_LIMIT, "query would use more memory than allowed");
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

}  // namespace aql
}  // namespace arangodb

#endif
