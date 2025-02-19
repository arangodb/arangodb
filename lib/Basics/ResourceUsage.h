////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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

#pragma once

#include "Basics/NumberUtils.h"

#include <atomic>
#include <cstdint>
#include <memory>

namespace arangodb {
class GlobalResourceMonitor;

/// @brief a ResourceMonitor to track and limit memory usage for allocations
/// in a certain area.
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
    // this is intentionally an integer division, which truncates any
    // remainders. we want this to be fast, so chunkSize should be a power of 2
    // and the div operation can be substituted by a bit shift operation.
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
  explicit ResourceUsageScope(ResourceMonitor& resourceMonitor,
                              std::uint64_t value);

  ~ResourceUsageScope();

  /// @brief steal responsibility for decreasing the memory
  /// usage on destruction
  void steal() noexcept;

  /// @brief revert all memory usage tracking operations in this scope
  void revert() noexcept;

  /// @brief track <value> bytes of memory, may throw!
  void increase(std::uint64_t value);

  void decrease(std::uint64_t value) noexcept;

  // memory tracked by this particlular scope instance
  std::uint64_t tracked() const noexcept { return _value; }

  std::uint64_t trackedAndSteal() noexcept;

 private:
  ResourceMonitor& _resourceMonitor;
  std::uint64_t _value;
};

/// @brief an std::allocator-like specialization that uses a ResourceMonitor
/// underneath.
template<typename Allocator, typename ResourceMonitor>
class ResourceUsageAllocatorBase : public Allocator {
 public:
  using difference_type =
      typename std::allocator_traits<Allocator>::difference_type;
  using propagate_on_container_move_assignment = typename std::allocator_traits<
      Allocator>::propagate_on_container_move_assignment;
  using size_type = typename std::allocator_traits<Allocator>::size_type;
  using value_type = typename std::allocator_traits<Allocator>::value_type;

  ResourceUsageAllocatorBase() = delete;

  template<typename... Args>
  ResourceUsageAllocatorBase(ResourceMonitor& resourceMonitor, Args&&... args)
      : Allocator(std::forward<Args>(args)...),
        _resourceMonitor(&resourceMonitor) {}

  ResourceUsageAllocatorBase(ResourceUsageAllocatorBase&& other) noexcept
      : Allocator(other.rawAllocator()),
        _resourceMonitor(other._resourceMonitor) {}

  ResourceUsageAllocatorBase(ResourceUsageAllocatorBase const& other) noexcept
      : Allocator(other.rawAllocator()),
        _resourceMonitor(other._resourceMonitor) {}

  ResourceUsageAllocatorBase& operator=(
      ResourceUsageAllocatorBase&& other) noexcept {
    static_cast<Allocator&>(*this) = std::move(static_cast<Allocator&>(other));
    _resourceMonitor = other._resourceMonitor;
    return *this;
  }

  template<typename A>
  ResourceUsageAllocatorBase(
      ResourceUsageAllocatorBase<A, ResourceMonitor> const& other) noexcept
      : Allocator(other.rawAllocator()),
        _resourceMonitor(other.resourceMonitor()) {}

  value_type* allocate(std::size_t n) {
    _resourceMonitor->increaseMemoryUsage(sizeof(value_type) * n);
    try {
      return Allocator::allocate(n);
    } catch (...) {
      _resourceMonitor->decreaseMemoryUsage(sizeof(value_type) * n);
      throw;
    }
  }

  void deallocate(value_type* p, std::size_t n) {
    Allocator::deallocate(p, n);
    _resourceMonitor->decreaseMemoryUsage(sizeof(value_type) * n);
  }

  Allocator const& rawAllocator() const noexcept {
    return static_cast<Allocator const&>(*this);
  }

  ResourceMonitor* resourceMonitor() const noexcept { return _resourceMonitor; }

  template<typename A>
  bool operator==(ResourceUsageAllocatorBase<A, ResourceMonitor> const& other)
      const noexcept {
    return rawAllocator() == other.rawAllocator() &&
           _resourceMonitor == other.resourceMonitor();
  }

 private:
  ResourceMonitor* _resourceMonitor;
};

template<typename T, typename ResourceMonitor>
struct ResourceUsageAllocator
    : ResourceUsageAllocatorBase<std::allocator<T>, ResourceMonitor> {
  using ResourceUsageAllocatorBase<std::allocator<T>,
                                   ResourceMonitor>::ResourceUsageAllocatorBase;

  template<typename U>
  struct rebind {
    using other = ResourceUsageAllocator<U, ResourceMonitor>;
  };

  template<typename X, typename... Args>
  void construct(X* ptr, Args&&... args) {
    std::uninitialized_construct_using_allocator(ptr, *this,
                                                 std::forward<Args>(args)...);
  }
};

}  // namespace arangodb

template<typename T, typename R>
struct std::allocator_traits<arangodb::ResourceUsageAllocator<T, R>> {
  using allocator_type = arangodb::ResourceUsageAllocator<T, R>;
  using value_type = T;
  using pointer = T*;
  using const_pointer = const T*;
  using void_pointer = void*;
  using const_void_pointer = const void*;
  using difference_type = std::ptrdiff_t;
  using size_type = std::size_t;

  using propagate_on_container_copy_assignment = false_type;
  using propagate_on_container_move_assignment = false_type;
  using propagate_on_container_swap = false_type;

  static allocator_type select_on_container_copy_construction(
      const allocator_type& other) noexcept {
    return other;
  }

  using is_always_equal = false_type;

  template<typename U>
  using rebind_alloc = arangodb::ResourceUsageAllocator<U, R>;

  template<typename U>
  using rebind_traits =
      allocator_traits<arangodb::ResourceUsageAllocator<U, R>>;

  [[nodiscard]] static pointer allocate(allocator_type& a, size_type n) {
    return a.allocate(n);
  }

  [[nodiscard]] static pointer allocate(allocator_type& a, size_type n,
                                        const_void_pointer) {
    return a.allocate(n);
  }

  static void deallocate(allocator_type& a, pointer p, size_type n) {
    a.deallocate(p, n);
  }

  template<typename U, typename... Args>
  static void construct(allocator_type& a, U* p, Args&&... args) {
    a.construct(p, std::forward<Args>(args)...);
  }

  template<typename U>
  static constexpr void destroy(allocator_type&, U* p) noexcept(
      is_nothrow_destructible<U>::value) {
    p->~U();
  }

  static constexpr size_type max_size(const allocator_type&) noexcept {
    return std::numeric_limits<size_type>::max() / sizeof(value_type);
  }
};
