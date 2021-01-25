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

#include "Basics/ResourceUsage.h"

using namespace arangodb;

ResourceMonitor::~ResourceMonitor() {
  // this assertion is here to ensure that our memory usage tracking works
  // correctly, and everything that we accounted for is actually properly torn
  // down. the assertion will have no effect in production.
  TRI_ASSERT(currentResources.memoryUsage.load(std::memory_order_relaxed) == 0);
}
  
void ResourceMonitor::increaseMemoryUsage(std::size_t value) {
  std::size_t current = value + currentResources.memoryUsage.fetch_add(value, std::memory_order_relaxed);

  if (maxMemoryUsage > 0 && ADB_UNLIKELY(current > maxMemoryUsage)) {
    currentResources.memoryUsage.fetch_sub(value, std::memory_order_relaxed);
    THROW_ARANGO_EXCEPTION(TRI_ERROR_RESOURCE_LIMIT);
  }
  
  std::size_t peak = currentResources.peakMemoryUsage.load(std::memory_order_relaxed);
  while (peak < current) {
    if (currentResources.peakMemoryUsage.compare_exchange_weak(peak, current,
                                                               std::memory_order_release,
                                                               std::memory_order_relaxed)) {
      break;
    }
  }
}
  
void ResourceMonitor::decreaseMemoryUsage(std::size_t value) noexcept {
  [[maybe_unused]] std::size_t previous = currentResources.memoryUsage.fetch_sub(value, std::memory_order_relaxed);
  TRI_ASSERT(previous >= value);
}
  
ResourceUsageScope::ResourceUsageScope(ResourceMonitor& resourceMonitor, std::size_t value)
    : _resourceMonitor(resourceMonitor), _value(value) {
  // may throw
  increase(_value);
}
  
ResourceUsageScope::~ResourceUsageScope() {
  decrease(_value);
}

void ResourceUsageScope::steal() noexcept {
  _value = 0;
}
  
void ResourceUsageScope::increase(std::size_t value) {
  if (value > 0) {
    // may throw
    _resourceMonitor.increaseMemoryUsage(value);
  }
}
  
void ResourceUsageScope::decrease(std::size_t value) noexcept {
  if (value > 0) {
    _resourceMonitor.decreaseMemoryUsage(value);
  }
}
