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
   Reference counting wrapper for a constant resource

   Destroys itself when the reference count decrements to zero.
 */
template<typename T>
struct Shared {
  auto increment() -> void { _count.fetch_add(1, std::memory_order_acq_rel); }
  auto decrement() -> void {
    auto old = _count.fetch_sub(1, std::memory_order_acq_rel);
    if (old == 1) {
      delete this;
    }
  }
  template<class... Input>
  Shared(Input... args) : _data{args...} {}
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
    other._resource = nullptr;
  }
  auto operator=(SharedPtr&& other) -> SharedPtr& {
    _resource = other._resource;
    other._resource = nullptr;
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
  SharedPtr(Input... args) : _resource{new Shared<T>(args...)} {}
  auto operator*() -> std::optional<std::reference_wrapper<T>> {
    return get_ref();
  }
  operator bool() const { return _resource != nullptr; }
  auto get_ref() -> std::optional<std::reference_wrapper<T>> {
    if (_resource) {
      return {_resource->get_ref()};
    } else {
      return std::nullopt;
    }
  }
  auto ref_count() const -> size_t {
    if (_resource) {
      return _resource->ref_count();
    } else {
      return 0;
    }
  }

  Shared<T>* _resource = nullptr;
};

/**
   Lock-free atomic either type for either a shared or a raw pointer

   Works if both types have an alignment larger than 1,
   then the last bit of a pointer to one of these types is unused and can be
   used as a flag for the type of the pointer.
*/
template<typename Left, typename Right>
struct AtomicSharedOrRawPtr {
  static constexpr auto num_flag_bits = 1;
  static_assert(std::alignment_of_v<Shared<Left>> >= (1 << num_flag_bits) &&
                std::alignment_of_v<Right> >= (1 << num_flag_bits));

  template<SameAs<Right> U>
  AtomicSharedOrRawPtr(U* right)
      : _resource{reinterpret_cast<std::uintptr_t>(right) | 0} {}

  template<SameAs<Left> U>
  AtomicSharedOrRawPtr(SharedPtr<U> const& left) {
    auto resource = left._resource;
    resource->increment();
    _resource.store(reinterpret_cast<std::uintptr_t>(resource) | 1);
  }

  ~AtomicSharedOrRawPtr() {
    // if resource includes a shared ptr, decrement it
    auto data = _resource.load();
    if (data != 0) {
      constexpr auto flag_mask = (1 << num_flag_bits) - 1;
      constexpr auto data_mask = ~flag_mask;
      if (data & flag_mask) {
        reinterpret_cast<Shared<Left>*>(data & data_mask)->decrement();
      }
    }
  }

  auto get_ref() const
      -> std::optional<std::variant<std::reference_wrapper<Left>,
                                    std::reference_wrapper<Right>>> {
    auto data = _resource.load();
    if (data == 0) {
      return std::nullopt;
    }
    constexpr auto flag_mask = (1 << num_flag_bits) - 1;
    constexpr auto data_mask = ~flag_mask;
    if (data & flag_mask) {
      return {std::reference_wrapper<Left>{
          reinterpret_cast<Shared<Left>*>(data & data_mask)->get_ref()}};
    } else {
      return {std::reference_wrapper<Right>{
          *reinterpret_cast<Right*>(data & data_mask)}};
    }
  }

 private:
  std::atomic<std::uintptr_t> _resource;
};

}  // namespace arangodb::containers

namespace arangodb::inspection {
template<typename T>
struct Access<containers::SharedPtr<T>>
    : OptionalAccess<containers::SharedPtr<T>> {};
}  // namespace arangodb::inspection
