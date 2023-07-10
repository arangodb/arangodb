////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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

#pragma once

#include "Basics/Common.h"
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
template<typename Allocator>
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
      ResourceUsageAllocatorBase<A> const& other) noexcept
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
  bool operator==(ResourceUsageAllocatorBase<A> const& other) const noexcept {
    return rawAllocator() == other.rawAllocator() &&
           _resourceMonitor == other.resourceMonitor();
  }

 private:
  ResourceMonitor* _resourceMonitor;
};

namespace detail {

template<typename T>
struct uses_allocator_construction_args_t {
  template<typename Alloc, typename... Args>
  auto operator()(Alloc const& alloc, Args&&... args) const {
    if constexpr (!std::uses_allocator<T, Alloc>::value) {
      return std::forward_as_tuple(std::forward<Args>(args)...);
    } else if constexpr (std::is_constructible_v<T, std::allocator_arg_t, Alloc,
                                                 Args...>) {
      return std::tuple<std::allocator_arg_t, Alloc const&, Args&&...>(
          std::allocator_arg, alloc, std::forward<Args>(args)...);
    } else {
      return std::tuple<Args&&..., Alloc const&>(std::forward<Args>(args)...,
                                                 alloc);
    }
  }
};

template<typename U, typename V>
struct uses_allocator_construction_args_t<std::pair<U, V>> {
  using T = std::pair<U, V>;
  template<typename Alloc, typename Tuple1, typename Tuple2>
  auto operator()(Alloc const& alloc, std::piecewise_construct_t, Tuple1&& t1,
                  Tuple2&& t2) const {
    return std::make_tuple(
        std::piecewise_construct,
        std::apply(
            [&alloc](auto&&... args1) {
              return uses_allocator_construction_args<U>(
                  alloc, std::forward<decltype(args1)>(args1)...);
            },
            std::forward<Tuple1>(t1)),
        std::apply(
            [&alloc](auto&&... args2) {
              return uses_allocator_construction_args<V>(
                  alloc, std::forward<decltype(args2)>(args2)...);
            },
            std::forward<Tuple2>(t2)));
  }
  template<typename Alloc>
  auto operator()(Alloc const& alloc) const {
    return uses_allocator_construction_args<T>(alloc, std::piecewise_construct,
                                               std::tuple<>{}, std::tuple<>{});
  }

  template<typename Alloc, typename A, typename B>
  auto operator()(Alloc const& alloc, A&& a, B&& b) const {
    return uses_allocator_construction_args<T>(
        alloc, std::piecewise_construct,
        std::forward_as_tuple(std::forward<A>(a)),
        std::forward_as_tuple(std::forward<B>(b)));
  }

  template<typename Alloc, typename A, typename B>
  auto operator()(Alloc const& alloc, std::pair<A, B>& pr) const {
    return uses_allocator_construction_args<T>(
        alloc, std::piecewise_construct, std::forward_as_tuple(pr.first),
        std::forward_as_tuple(pr.second));
  }

  template<typename Alloc, typename A, typename B>
  auto operator()(Alloc const& alloc, std::pair<A, B> const& pr) const {
    return uses_allocator_construction_args<T>(
        alloc, std::piecewise_construct, std::forward_as_tuple(pr.first),
        std::forward_as_tuple(pr.second));
  }

  template<typename Alloc, typename A, typename B>
  auto operator()(Alloc const& alloc, std::pair<A, B>&& pr) const {
    return uses_allocator_construction_args<T>(
        alloc, std::piecewise_construct,
        std::forward_as_tuple(std::move(pr.first)),
        std::forward_as_tuple(std::move(pr.second)));
  }
};

template<typename T>
inline constexpr auto uses_allocator_construction_args =
    uses_allocator_construction_args_t<T>{};

}  // namespace detail

template<typename T>
struct ResourceUsageAllocator : ResourceUsageAllocatorBase<std::allocator<T>> {
  using ResourceUsageAllocatorBase<
      std::allocator<T>>::ResourceUsageAllocatorBase;

  template<typename U>
  struct rebind {
    using other = ResourceUsageAllocator<U>;
  };

  template<typename X, typename... Args>
  void construct(X* ptr, Args&&... args) {
    // Sadly libc++ on mac does not support this function. Thus we do it by hand
    // std::uninitialized_construct_using_allocator(ptr, *this,
    //                                              std::forward<Args>(args)...)
    std::apply(
        [&](auto&&... xs) {
          return std::construct_at(ptr, std::forward<decltype(xs)>(xs)...);
        },
        detail::uses_allocator_construction_args<X>(
            *this, std::forward<Args>(args)...));
  }
};

}  // namespace arangodb
