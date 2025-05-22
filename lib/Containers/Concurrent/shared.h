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
  auto ref_count() -> size_t { return _count.load(std::memory_order_relaxed); }

 private:
  std::atomic<std::size_t> _count = 1;
  T _data;
};

template<typename T>
struct SharedReference {
  SharedReference(SharedReference&& other) : _resource{other._resource} {
    other._resource = nullptr;
  }
  auto operator=(SharedReference&& other) -> SharedReference& {
    _resource = other._resource;
    other._resource = nullptr;
    return *this;
  }
  SharedReference(SharedReference const& other) : _resource{other._resource} {
    _resource->increment();
  }
  auto operator=(SharedReference const& other) -> SharedReference& {
    _resource = other._resource;
    _resource->increment();
    return *this;
  }
  ~SharedReference() {
    if (_resource) {
      _resource->decrement();
    }
  }
  template<class... Input>
  SharedReference(Input... args) : _resource{new Shared<T>(args...)} {}
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
  auto ref_count() -> size_t {
    if (_resource) {
      return _resource->ref_count();
    } else {
      return 0;
    }
  }

 private:
  Shared<T>* _resource = nullptr;
};
template<typename T, typename K>
struct VariantPtr {
  static constexpr auto num_flag_bits = 1;
  static_assert(std::alignment_of_v<Shared<T>> >= (1 << num_flag_bits) &&
                std::alignment_of_v<K> >= (1 << num_flag_bits));

  template<class... Input>
  static auto first(Input... args) -> VariantPtr {
    auto ptr = new Shared<T>(args...);
    return VariantPtr{reinterpret_cast<std::uintptr_t>(ptr) | 1};
  }
  template<class... Input>
  static auto second(Input... args) -> VariantPtr {
    // TODO cleanup
    auto ptr = new K(args...);
    return VariantPtr{reinterpret_cast<std::uintptr_t>(ptr) | 0};
  }
  auto get_ref() -> std::optional<
      std::variant<std::reference_wrapper<T>, std::reference_wrapper<K>>> {
    auto data = _resource.load();
    if (data == 0) {
      return std::nullopt;
    }
    constexpr auto flag_mask = (1 << num_flag_bits) - 1;
    constexpr auto data_mask = ~flag_mask;
    if (data & flag_mask) {
      return {std::reference_wrapper<T>{
          reinterpret_cast<Shared<T>*>(data & data_mask)->get_ref()}};
    } else {
      return {
          std::reference_wrapper<K>{*reinterpret_cast<K*>(data & data_mask)}};
    }
  }
  std::atomic<std::uintptr_t> _resource;
};

}  // namespace arangodb::containers

namespace arangodb::inspection {
template<typename T>
struct Access<containers::SharedReference<T>>
    : OptionalAccess<containers::SharedReference<T>> {};
}  // namespace arangodb::inspection
