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
  auto get_ref() const -> T const& { return _data; }
  auto ref_count() const -> size_t {
    return _count.load(std::memory_order_relaxed);
  }

 private:
  std::atomic<std::size_t> _count = 1;
  const T _data;
};

template<typename T, typename ExpectedT>
concept SameAs = std::is_same_v<T, ExpectedT>;

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
  SharedReference(SharedReference const& other) {
    // TODO should both store and increment not be done at same time?
    other._resource->increment();
    _resource = other._resource;
  }
  auto operator=(SharedReference const& other) -> SharedReference& {
    // TODO should both store and increment not be done at same time?
    other._resource->increment();
    _resource = other._resource;
    return *this;
  }
  ~SharedReference() {
    if (_resource) {
      _resource->decrement();
    }
  }
  template<class... Input>
  SharedReference(Input... args) : _resource{new Shared<T>(args...)} {}
  auto operator*() const -> std::optional<std::reference_wrapper<const T>> {
    return get_ref();
  }
  operator bool() const { return _resource != nullptr; }
  auto get_ref() const -> std::optional<std::reference_wrapper<const T>> {
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

  // TODO private:
  Shared<T>* _resource = nullptr;
};

// lock-free atomic variant (intrusive)
// either shared or simple pointer
template<typename T, typename K>
struct VariantPtr {
  static constexpr auto num_flag_bits = 1;
  static_assert(std::alignment_of_v<Shared<T>> >= (1 << num_flag_bits) &&
                std::alignment_of_v<K> >= (1 << num_flag_bits));

  template<SameAs<K> U>
  VariantPtr(U* second)
      : _resource{reinterpret_cast<std::uintptr_t>(second) | 0} {}

  template<SameAs<T> U>
  VariantPtr(SharedReference<U> const& first) {
    // TODO should both store and increment not be done at same time?
    auto resource = first._resource;
    resource->increment();
    _resource.store(reinterpret_cast<std::uintptr_t>(resource) | 1);
  }

  ~VariantPtr() {
    auto data = _resource.load();
    if (data != 0) {
      constexpr auto flag_mask = (1 << num_flag_bits) - 1;
      constexpr auto data_mask = ~flag_mask;
      if (data & flag_mask) {
        // this is the shared ptr
        reinterpret_cast<Shared<T>*>(data & data_mask)->decrement();
      }
    }
  }
  // explicitly delete because we don't need them
  VariantPtr(VariantPtr&&) = delete;
  auto operator=(VariantPtr&&) -> VariantPtr& = delete;
  VariantPtr(VariantPtr const&) = delete;
  auto operator=(VariantPtr const&) -> VariantPtr& = delete;

  // TODO get rid of these because we need to do cleanup for them
  template<class... Input>
  static auto first(Input... args) -> VariantPtr {
    auto ptr = new Shared<T>(args...);
    return VariantPtr{ptr};
  }
  template<class... Input>
  static auto second(Input... args) -> VariantPtr {
    auto ptr = new K(args...);
    return VariantPtr{ptr};
  }

  auto get_ref()
      -> std::optional<std::variant<std::reference_wrapper<const T>,
                                    std::reference_wrapper<const K>>> {
    auto data = _resource.load();
    if (data == 0) {
      return std::nullopt;
    }
    constexpr auto flag_mask = (1 << num_flag_bits) - 1;
    constexpr auto data_mask = ~flag_mask;
    if (data & flag_mask) {
      return {std::reference_wrapper<const T>{
          reinterpret_cast<Shared<T>*>(data & data_mask)->get_ref()}};
    } else {
      return {std::reference_wrapper<const K>{
          *reinterpret_cast<K*>(data & data_mask)}};
    }
  }

 private:
  template<SameAs<T> U>
  VariantPtr(Shared<U>* shared)
      : _resource{reinterpret_cast<std::uintptr_t>(shared) | 1} {}
  std::atomic<std::uintptr_t> _resource;
};

}  // namespace arangodb::containers

namespace arangodb::inspection {
template<typename T>
struct Access<containers::SharedReference<T>>
    : OptionalAccess<containers::SharedReference<T>> {};
}  // namespace arangodb::inspection
