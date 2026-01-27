////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrei Lobov
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "misc.hpp"

namespace irs {
template<typename Allocator, typename Manager>
class ManagedAllocator : private Allocator {
 public:
  using difference_type =
    typename std::allocator_traits<Allocator>::difference_type;
  using propagate_on_container_move_assignment = typename std::allocator_traits<
    Allocator>::propagate_on_container_move_assignment;
  using propagate_on_container_copy_assignment = typename std::allocator_traits<
    Allocator>::propagate_on_container_copy_assignment;
  using propagate_on_container_swap = std::true_type;
  // Note: If swap would be needed this code should do the trick.
  // But beware of UB in case of non equal allocators.
  // std::conditional_t<std::is_empty_v<Allocator>, std::true_type,
  // typename std::allocator_traits<Allocator>::propagate_on_container_swap>;
  using size_type = typename std::allocator_traits<Allocator>::size_type;
  using value_type = typename std::allocator_traits<Allocator>::value_type;

  template<typename... Args>
  ManagedAllocator(Manager& rm, Args&&... args) noexcept(
    std::is_nothrow_constructible_v<Allocator, Args&&...>)
    : Allocator(std::forward<Args>(args)...), rm_(&rm) {}

  ManagedAllocator(ManagedAllocator&& other) noexcept(
    std::is_nothrow_move_constructible_v<Allocator>)
    : Allocator(std::move(other)), rm_(&other.ResourceManager()) {}

  ManagedAllocator(const ManagedAllocator& other) noexcept(
    std::is_nothrow_copy_constructible_v<Allocator>)
    : Allocator(other), rm_(&other.ResourceManager()) {}

  ManagedAllocator& operator=(ManagedAllocator&& other) noexcept(
    std::is_nothrow_move_assignable_v<Allocator>) {
    static_cast<Allocator&>(*this) = std::move(other);
    rm_ = &other.ResourceManager();
    return *this;
  }

  ManagedAllocator& operator=(const ManagedAllocator& other) noexcept(
    std::is_nothrow_copy_assignable_v<Allocator>) {
    static_cast<Allocator&>(*this) = other;
    rm_ = &other.ResourceManager();
    return *this;
  }

  template<typename A>
  ManagedAllocator(const ManagedAllocator<A, Manager>& other) noexcept(
    std::is_nothrow_copy_constructible_v<Allocator>)
    : Allocator(other.RawAllocator()), rm_(&other.ResourceManager()) {}

  value_type* allocate(size_type n) {
    rm_->Increase(sizeof(value_type) * n);
    Finally cleanup = [&]() noexcept {
      rm_->DecreaseChecked(sizeof(value_type) * n);
    };
    auto* res = Allocator::allocate(n);
    n = 0;
    return res;
  }

  void deallocate(value_type* p, size_type n) noexcept {
    Allocator::deallocate(p, n);
    rm_->Decrease(sizeof(value_type) * n);
  }

  const Allocator& RawAllocator() const noexcept {
    return static_cast<const Allocator&>(*this);
  }

  template<typename A>
  bool operator==(const ManagedAllocator<A, Manager>& other) const noexcept {
    return rm_ == &other.ResourceManager() &&
           RawAllocator() == other.RawAllocator();
  }

  Manager& ResourceManager() const noexcept { return *rm_; }

 private:
  Manager* rm_;
};
}  // namespace irs
