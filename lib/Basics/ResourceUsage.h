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
#include "Basics/NumberUtils.h"

#include <atomic>
#include <cstdint>

namespace arangodb {
class GlobalResourceMonitor;

struct alignas(64) ResourceMonitor final {
  /// @brief: granularity of allocations that we track. this should be a 
  /// power of 2, so dividing by it is efficient!
  /// note: whatever this value is, it will also dictate the minimum granularity
  /// of the global memory usage counter, plus the granularity for each query's
  /// peak memory usage value.
  /// note: if you adjust this value, keep in mind that making the chunk size
  /// smaller will lead to better granularity, but also will increase the number
  /// of atomic updates we need to make inside increaseMemoryUsage() and
  /// decreaseMemoryUsage().
  static constexpr std::uint64_t chunkSize = 32768;
  static_assert(NumberUtils::isPowerOfTwo(chunkSize));

  ResourceMonitor(ResourceMonitor const&) = delete;
  ResourceMonitor& operator=(ResourceMonitor const&) = delete;

  explicit ResourceMonitor(GlobalResourceMonitor& global) noexcept;
  ~ResourceMonitor();

  /// @brief sets a memory limit
  void memoryLimit(std::uint64_t value) noexcept;
  
  /// @brief returns the current memory limit
  std::uint64_t memoryLimit() const noexcept;
  
  /// @brief increase memory usage by <value> bytes. may throw!
  void increaseMemoryUsage(std::uint64_t value);

  /// @brief decrease memory usage by <value> bytes. will not throw
  void decreaseMemoryUsage(std::uint64_t value) noexcept;

  /// @brief return the current memory usage of the instance
  std::uint64_t current() const noexcept;
  
  /// @brief return the peak memory usage of the instance
  std::uint64_t peak() const noexcept;

  /// @brief reset counters for the local instance  
  void clear() noexcept;

  /// @brief calculate the "number of chunks" used by an allocation size.
  /// for this, we simply divide the size by a constant value, which is large
  /// enough so that many subsequent small allocations mostly fall into the
  /// same chunk.
  static constexpr std::int64_t numChunks(std::uint64_t value) noexcept {
    // this is intentionally an integer division, which truncates any remainders.
    // we want this to be fast, so chunkSize should be a power of 2 and the div
    // operation can be substituted by a bit shift operation.
    static_assert(chunkSize != 0);
    static_assert(NumberUtils::isPowerOfTwo(chunkSize));
    return static_cast<std::int64_t>(value / chunkSize);
  }

 private:
  std::atomic<std::uint64_t> _current;
  std::atomic<std::uint64_t> _peak;
  std::uint64_t _limit;
  GlobalResourceMonitor& _global;
};

/// @brief RAII object for temporary resource tracking
/// will track the resource usage on creation, and untrack it
/// on destruction, unless the responsibility is stolen
/// from it.
class ResourceUsageScope {
 public:
  ResourceUsageScope(ResourceUsageScope const&) = delete;
  ResourceUsageScope& operator=(ResourceUsageScope const&) = delete;

  explicit ResourceUsageScope(ResourceMonitor& resourceMonitor) noexcept;

  /// @brief track <value> bytes of memory, may throw!
  explicit ResourceUsageScope(ResourceMonitor& resourceMonitor, std::uint64_t value);

  ~ResourceUsageScope();
  
  /// @brief steal responsibility for decreasing the memory 
  /// usage on destruction
  void steal() noexcept;

  /// @brief revert all memory usage tracking operations in this scope
  void revert() noexcept;
  
  /// @brief track <value> bytes of memory, may throw!
  void increase(std::uint64_t value);
  
  void decrease(std::uint64_t value) noexcept;

 private:
  ResourceMonitor& _resourceMonitor;
  std::uint64_t _value;
};

}  // namespace arangodb

#endif
