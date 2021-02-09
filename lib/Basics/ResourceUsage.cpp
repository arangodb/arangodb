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
#include "Basics/Exceptions.h"
#include "Basics/GlobalResourceMonitor.h"
#include "Basics/debugging.h"
#include "Basics/voc-errors.h"

using namespace arangodb;

ResourceMonitor::ResourceMonitor(GlobalResourceMonitor& global) noexcept
    : _current(0),
      _peak(0),
      _limit(0),
      _global(global) {}

ResourceMonitor::~ResourceMonitor() {
  // this assertion is here to ensure that our memory usage tracking works
  // correctly, and everything that we accounted for is actually properly torn
  // down. the assertion will have no effect in production.
  TRI_ASSERT(_current.load(std::memory_order_relaxed) == 0);
}
  
/// @brief sets a memory limit
void ResourceMonitor::memoryLimit(std::size_t value) noexcept { 
  _limit = value; 
}

/// @brief returns the current memory limit
std::size_t ResourceMonitor::memoryLimit() const noexcept { 
  return _limit; 
}
  
/// @brief increase memory usage by <value> bytes. may throw!
/// this function may modify up to 3 atomic variables:
/// - the current local memory usage
/// - the peak local memory usage
/// - the global memory usage
/// the order in which we update these atomic variables is not important,
/// as long as everything is eventually consistent. 
/// in case this function triggers a "resource limit exceeded" error, 
/// the only thing that can have happened is the update of the current local
/// memory usage, which this function will roll back again.
/// as this function only adds and subtracts values from the memory usage
/// counters, it only does not lead to any lost updates due to data races
/// with other threads.
/// the peak memory usage value is updated with a CAS operation, so again
/// there will no be lost updates.
void ResourceMonitor::increaseMemoryUsage(std::size_t value) {
  std::size_t const previous = _current.fetch_add(value, std::memory_order_relaxed);
  std::size_t const current = previous + value;
  TRI_ASSERT(current >= value);
  
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
  std::size_t const previousBuckets = numBuckets(previous);
  std::size_t const currentBuckets = numBuckets(current);
  TRI_ASSERT(currentBuckets >= previousBuckets);
  std::size_t diff = currentBuckets - previousBuckets;
  
  if (diff != 0) {
    // number of buckets has changed, so this is either a substantial allocation or
    // we have piled up changes by lots of small allocations so far.
    // time for some memory expensive checks now...

    if (_limit > 0 && ADB_UNLIKELY(current > _limit)) {
      // we would use more memory than dictated by the instance's own limit.
      // because we will throw an exception directly afterwards, we now need to
      // revert the change that we already made to the instance's own counter.
      std::size_t adjustedPrevious = _current.fetch_sub(value, std::memory_order_relaxed);
      std::size_t adjustedCurrent = adjustedPrevious - value;
  
      std::size_t adjustedDiff = numBuckets(adjustedPrevious) - numBuckets(adjustedCurrent);
      if (adjustedDiff != diff) {
        if (adjustedDiff > diff) {
          _global.forceIncreaseMemoryUsage((adjustedDiff - diff) * bucketSize);
        } else {
          _global.decreaseMemoryUsage((diff - adjustedDiff) * bucketSize);
        }
      }

      // now we can safely signal an exception
      THROW_ARANGO_EXCEPTION(TRI_ERROR_RESOURCE_LIMIT);
    }

    // instance's own memory usage counter has been updated successfully once we got here.

    // now modify the global counter value, too.
    if (!_global.increaseMemoryUsage(diff * bucketSize)) {
      // the allocation would exceed the global maximum value, so we need to roll back.
      _current.fetch_sub(value, std::memory_order_relaxed);

      // now we can safely signal an exception
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_RESOURCE_LIMIT, "global memory limit exceeded");
    }
    // increasing the global counter has succeeded!
 
    // update the peak memory usage counter for the local instance. we do this
    // only when there was a change in the number of buckets.
    std::size_t peak = _peak.load(std::memory_order_relaxed);
    std::size_t const newPeak = currentBuckets * bucketSize;
    // do a CAS here, as concurrent threads may work on the peak memory usage at the
    // very same time.
    while (peak < newPeak) {
      if (_peak.compare_exchange_weak(peak, newPeak, std::memory_order_release, std::memory_order_relaxed)) {
        break;
      }
    }
  }
}
  
void ResourceMonitor::decreaseMemoryUsage(std::size_t value) noexcept {
  // this function will always decrease the current local memory usage value,
  // and may in addition lead to a decrease of the global memory usage value.
  // as it only subtracts from these counters, it will not lead to any lost
  // updates even if there are concurrent threads working on the same counters.
  // note that peak memory usage is not changed here, as this is only relevant
  // when we are _increasing_ memory usage.
  std::size_t const previous = _current.fetch_sub(value, std::memory_order_relaxed);
  TRI_ASSERT(previous >= value);
  std::size_t const current = previous - value;
  
  std::size_t diff = numBuckets(previous) - numBuckets(current);

  if (diff != 0) {
    // number of buckets has changed. now we will update the global counter!
    _global.decreaseMemoryUsage(diff * bucketSize);
    // no need to update the peak memory usage counter here
  }
}
  
size_t ResourceMonitor::current() const noexcept {
  return _current.load(std::memory_order_relaxed);
}

std::size_t ResourceMonitor::peak() const noexcept {
  return _peak.load(std::memory_order_relaxed);
}

void ResourceMonitor::clear() noexcept {
  _current = 0;
  _peak = 0;
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
