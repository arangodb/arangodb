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
/// @author Julia Volmer
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <atomic>
#include <cstdint>
#include <optional>
#include <variant>
#include <iostream>

#include "Inspection/Access.h"

namespace arangodb::containers {

/**
   Reference counting wrapper for a resource

   Destroys itself when the reference count decrements to zero.
   This resource can be referenced - in contrast to a simle std::shared_ptr - by
   different types of pointers at the same time, e.g. by a SharedPtr (similar to
   an std::shared_ptr) and an AtomicSharedOrRawPtr.
 */
template<typename T>
struct SharedResource {
  auto increment() -> void { _count.fetch_add(1, std::memory_order_acq_rel); }
  auto decrement() -> void {
    auto old = _count.fetch_sub(1, std::memory_order_acq_rel);
    if (old == 1) {
      delete this;
    }
  }
  template<class... Input>
  SharedResource(Input... args) : _data{args...} {}
  auto get() -> T* { return &_data; }
  auto get_ref() -> T& { return _data; }
  auto ref_count() const -> size_t {
    return _count.load(std::memory_order_relaxed);
  }

 private:
  std::atomic<std::size_t> _count = 1;
  T _data;
};

template<typename T, typename ExpectedT>
concept SameAs = std::is_same_v<T, ExpectedT>;

template<typename T>
struct SharedPtr {
  SharedPtr(SharedPtr&& other) : _resource{other._resource} {
    if (&other != this) {
      other._resource = nullptr;
    }
  }
  auto operator=(SharedPtr&& other) -> SharedPtr& {
    if (&other != this) {
      _resource = other._resource;
      other._resource = nullptr;
    }
    return *this;
  }
  SharedPtr(SharedPtr const& other) {
    other._resource->increment();
    _resource = other._resource;
  }
  auto operator=(SharedPtr const& other) -> SharedPtr& {
    other._resource->increment();
    _resource = other._resource;
    return *this;
  }
  ~SharedPtr() {
    if (_resource) {
      _resource->decrement();
    }
  }
  template<class... Input>
  SharedPtr(Input... args) : _resource{new SharedResource<T>(args...)} {}
  auto operator*() -> std::optional<std::reference_wrapper<T>> {
    if (_resource) {
      return {_resource->get_ref()};
    } else {
      return std::nullopt;
    }
  }
  operator bool() const { return _resource != nullptr; }
  auto get() -> T* {
    if (not _resource) {
      return nullptr;
    }
    return {_resource->get()};
  }
  auto get_ref() const -> std::optional<std::reference_wrapper<T>> {
    if (not _resource) {
      return std::nullopt;
    }
    return {_resource->get_ref()};
  }
  auto ref_count() const -> size_t {
    if (_resource) {
      return _resource->ref_count();
    } else {
      return 0;
    }
  }

  SharedResource<T>* _resource = nullptr;
};

/**
   Lock-free atomic either type for either a shared or a raw pointer

   Works if both types have an alignment larger than 1,
   then the last bit of a pointer to one of these types is unused and can be
   used as a flag for the type of the pointer.

   Make sure that Raw define the pointer type with alignment
   larger than (1 << num_flag_bits)
*/
template<typename Shared, typename Raw>
struct AtomicSharedOrRawPtr {
  template<SameAs<Raw> U>
  AtomicSharedOrRawPtr(U* right) : _resource{raw_to_ptr(right)} {}

  template<SameAs<Shared> U>
  AtomicSharedOrRawPtr(SharedPtr<U> const& left) {
    auto resource = left._resource;
    resource->increment();
    _resource.store(shared_to_ptr(resource), std::memory_order_relaxed);
  }

  ~AtomicSharedOrRawPtr() {
    auto old =
        _resource.exchange(raw_to_ptr(nullptr), std::memory_order_relaxed);
    decrement_shared(old);
  }

  /**
     User needs to make sure that SharedResource still exists
   */
  auto load() const -> std::variant<Shared, Raw*> {
    // (1) syncs with (2), (3)
    auto data = _resource.load(std::memory_order_acquire);
    if (is_shared(data)) {
      auto shared = ptr_to_shared(data);
      return {shared->get_ref()};
    } else {
      return {ptr_to_raw(data)};
    }
  }

  template<SameAs<Shared> U>
  auto store(SharedPtr<U> left) -> void {
    auto resource = left._resource;
    resource->increment();
    // (2) syncs with (1), (2), (3)
    auto old =
        _resource.exchange(shared_to_ptr(resource), std::memory_order_acq_rel);
    decrement_shared(old);
  }
  template<SameAs<Raw> U>
  auto store(U* right) -> void {
    // (3) syncs with (1), (2), (3)
    auto old = _resource.exchange(raw_to_ptr(right), std::memory_order_acq_rel);
    decrement_shared(old);
  }

 private:
  std::atomic<std::uintptr_t> _resource;

  static constexpr auto num_flag_bits = 1;
  static constexpr auto flag_mask = (1 << num_flag_bits) - 1;
  static constexpr auto data_mask = ~flag_mask;
  static_assert(std::alignment_of_v<SharedResource<Shared>> >=
                (1 << num_flag_bits));

  auto is_shared(std::uintptr_t ptr) const -> bool { return ptr & flag_mask; }
  auto shared_to_ptr(SharedResource<Shared>* shared) const -> std::uintptr_t {
    return reinterpret_cast<std::uintptr_t>(shared) | 1;
  }
  auto raw_to_ptr(Raw* raw) const -> std::uintptr_t {
    return reinterpret_cast<std::uintptr_t>(raw) | 0;
  }
  auto ptr_to_shared(std::uintptr_t ptr) const -> SharedResource<Shared>* {
    return reinterpret_cast<SharedResource<Shared>*>(ptr & data_mask);
  }
  auto ptr_to_raw(std::uintptr_t ptr) const -> Raw* {
    return reinterpret_cast<Raw*>(ptr & data_mask);
  }

  /**
     can only be called if we are sure that no-one else updates
     given ptr in-between
   */
  auto decrement_shared(std::uintptr_t ptr) const -> void {
    if (is_shared(ptr)) {
      ptr_to_shared(ptr)->decrement();
    }
  }
};
template<typename Inspector, typename Shared, typename Raw>
auto inspect(Inspector& f, AtomicSharedOrRawPtr<Shared, Raw>& x) {
  if constexpr (not Inspector::isLoading) {  // only serialization
    auto var = x.load();
    if (std::holds_alternative<Shared>(var)) {
      return f.apply(std::get<Shared>(var));
    } else {
      auto ptr = std::get<Raw*>(var);
      if (ptr) {
        return f.apply(*ptr);
      }
      return f.apply(inspection::Null{});
    }
  }
}

}  // namespace arangodb::containers

namespace arangodb::inspection {
template<typename T>
struct Access<containers::SharedPtr<T>>
    : OptionalAccess<containers::SharedPtr<T>> {};

}  // namespace arangodb::inspection
