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

namespace arangodb {
namespace aql {

struct ResourceUsage {
  constexpr ResourceUsage() : memoryUsage(0) {}
  explicit ResourceUsage(size_t memoryUsage) : memoryUsage(memoryUsage) {}

  void clear() { memoryUsage = 0; }
   
  size_t memoryUsage;
};

struct ResourceMonitor {
  ResourceMonitor() : currentResources(), maxResources() {}
  explicit ResourceMonitor(ResourceUsage const& maxResources) : currentResources(), maxResources(maxResources) {}
 
  void setMemoryLimit(size_t value) {
    maxResources.memoryUsage = value;
  }
   
  void increaseMemoryUsage(size_t value) {
    if (maxResources.memoryUsage > 0 && 
        currentResources.memoryUsage + value > maxResources.memoryUsage) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_RESOURCE_LIMIT, "query would use more memory than allowed");
    }
    currentResources.memoryUsage += value;
  }
  
  void decreaseMemoryUsage(size_t value) noexcept {
    TRI_ASSERT(currentResources.memoryUsage >= value);
    currentResources.memoryUsage -= value;
  }

  void clear() {
    currentResources.clear();
  }

  ResourceUsage currentResources;
  ResourceUsage maxResources;
};

}
}

#endif
