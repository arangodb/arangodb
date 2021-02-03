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
  
/// @brief maximum allowed global memory limit for all tracked operations combined.
/// a value of 0 means that there will be no global limit enforced.
/*static*/ std::size_t ResourceMonitor::_globalMemoryLimit = 0;

/// @brief the current combined memory usage of all tracked operations.
/// should only very brief go above "_globalMemoryLimit" (if it goes
/// above it, it will instantly throw an exception so that memory usage
/// will go down again quickly).
/// this counter is updated by local instances only if there is a substantial
/// allocation/deallocation. it is intentionally _not_ updated on every
/// small allocation/deallocation. the granularity for the values in this
/// counter is bucketSize.
/*static*/ std::atomic<std::size_t> ResourceMonitor::_globalMemoryUsage{0};

ResourceMonitor::ResourceMonitor() noexcept
    : _currentResources(), 
      _memoryLimit(0) {}

ResourceMonitor::~ResourceMonitor() {
  // this assertion is here to ensure that our memory usage tracking works
  // correctly, and everything that we accounted for is actually properly torn
  // down. the assertion will have no effect in production.
  TRI_ASSERT(_currentResources.memoryUsage.load(std::memory_order_relaxed) == 0);
}
  
/// @brief sets a memory limit
void ResourceMonitor::memoryLimit(std::size_t value) noexcept { 
  _memoryLimit = value; 
}

/// @brief returns the current memory limit
std::size_t ResourceMonitor::memoryLimit() const noexcept { 
  return _memoryLimit; 
}
  
/// @brief sets the global memory limit
/*static*/ void ResourceMonitor::globalMemoryLimit(std::size_t value) noexcept { 
  _globalMemoryLimit = value; 
}

/// @brief returns the global memory limit
/*static*/ std::size_t ResourceMonitor::globalMemoryLimit() noexcept { 
  return _globalMemoryLimit; 
}

/// @brief returns the current global memory usage
/*static*/ std::size_t ResourceMonitor::globalMemoryUsage() noexcept { 
  return _globalMemoryUsage.load(std::memory_order_relaxed);
}
  
/// @brief increase memory usage by <value> bytes. may throw!
void ResourceMonitor::increaseMemoryUsage(std::size_t value) {
  std::size_t const previous = _currentResources.memoryUsage.fetch_add(value, std::memory_order_relaxed);
  std::size_t const current = previous + value;
  
  // now calculate if the number of buckets used by instance's allocations stays the 
  // same after the extra allocation. if yes, it was likely a very small allocation, and 
  // we don't bother with updating the global counter for it. not updating the global
  // counter on each (small) allocation is an optimization that saves updating a
  // (potentially highly contended) shared atomic variable on each allocation.
  // only doing this when there are substantial allocations/deallocations simply
  // makes many cases of small allocations and deallocations more efficient.
  // as a consequence, the granularity of allocations tracked in the global counter
  // is `bucketSize`.
  //
  // this idea is based on suggestions from @dothebart and @mpoeter for reducing the 
  // number of updates to the shared global memory usage counter, which very likely
  // would be a source of contention in case multiple queries execute in parallel.
  std::size_t const currentBuckets = numBuckets(current);
  std::size_t diff = currentBuckets - numBuckets(previous);
  
  if (diff != 0) {
    // number of buckets has changed, so this is either a substantial allocation or
    // we have piled up changes by lots of small allocations so far.
    // time for some memory expensive checks now...

    if (_memoryLimit > 0 && ADB_UNLIKELY(current > _memoryLimit)) {
      // we would use more memory than dictated by the instance's own limit.
      // because we will throw an exception directly afterwards, we now need to
      // revert the change that we already made to the instance's own counter.
      _currentResources.memoryUsage.fetch_sub(value, std::memory_order_relaxed);

      // now we can safely signal an exception
      THROW_ARANGO_EXCEPTION(TRI_ERROR_RESOURCE_LIMIT);
    }

    // instance's own memory usage counter has been updated successfully once we got here.

    // now modify the global counter value, too.
    diff *= bucketSize;

    TRI_ASSERT((diff % bucketSize) == 0);

    std::size_t previousGlobal = _globalMemoryUsage.fetch_add(diff, std::memory_order_relaxed);
    std::size_t currentGlobal = previousGlobal + diff;
    
    if (_globalMemoryLimit > 0 && ADB_UNLIKELY(currentGlobal > _globalMemoryLimit)) {
      // the allocation would exceed the global maximum value, so we need to roll back.

      // revert the change to the global counter
      _globalMemoryUsage.fetch_sub(diff, std::memory_order_relaxed);
    
      // revert the change to the local counter
      _currentResources.memoryUsage.fetch_sub(value, std::memory_order_relaxed);

      // now we can safely signal an exception
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_RESOURCE_LIMIT, "global memory limit exceeded");
    }
 
    // update the peak memory usage counter for the local instance. we do this
    // only when there was a change in the number of buckets.
    std::size_t peak = _currentResources.peakMemoryUsage.load(std::memory_order_relaxed);
    std::size_t const newPeak = currentBuckets * bucketSize;
    while (peak < newPeak) {
      if (_currentResources.peakMemoryUsage.compare_exchange_weak(peak, newPeak,
                                                                  std::memory_order_release,
                                                                  std::memory_order_relaxed)) {
        break;
      }
    }
  }
}
  
void ResourceMonitor::decreaseMemoryUsage(std::size_t value) noexcept {
  std::size_t const previous = _currentResources.memoryUsage.fetch_sub(value, std::memory_order_relaxed);
  TRI_ASSERT(previous >= value);
  std::size_t const current = previous - value;
  
  std::size_t diff = numBuckets(previous) - numBuckets(current);

  if (diff != 0) {
    // number of buckets has changed. now we will update the global counter!
    diff *= bucketSize;

    TRI_ASSERT((diff % bucketSize) == 0);
  
    [[maybe_unused]] std::size_t previousGlobal = _globalMemoryUsage.fetch_sub(diff, std::memory_order_relaxed);
    TRI_ASSERT(previousGlobal >= diff);
    
    // no need to update the peak memory usage counter here
  }
}
  
size_t ResourceMonitor::currentMemoryUsage() const noexcept {
  return _currentResources.memoryUsage.load(std::memory_order_relaxed);
}

std::size_t ResourceMonitor::peakMemoryUsage() const noexcept {
  return _currentResources.peakMemoryUsage.load(std::memory_order_relaxed);
}

void ResourceMonitor::clear() noexcept {
  _currentResources.clear();
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
