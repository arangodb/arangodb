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

#include <cstdlib>
#include <functional>
#include <atomic>

/**
   Reference counting wrapper for a resource

   Destroys itself and calls a custom cleanup function on the resource when the
   reference count decrements to zero.
 */
template<typename T>
struct Shared {
  static auto create(T* resource, std::function<void(T*)> cleanup) -> Shared* {
    if (resource == nullptr) {
      std::abort();
    }
    return new Shared{resource, cleanup};
  }
  auto get_ref() const -> T& { return *_resource; }
  auto get() const -> T* { return _resource; }
  auto increment() -> void { _count.fetch_add(1, std::memory_order_acq_rel); }
  auto decrement() -> void {
    auto old = _count.fetch_sub(1, std::memory_order_acq_rel);
    if (old == 1) {
      _cleanup(_resource);
      delete this;
    }
  }
  auto ref_count() -> size_t { return _count.load(std::memory_order_release); }

 private:
  T* _resource;
  std::function<void(T*)> _cleanup;
  std::atomic<size_t> _count = 0;
  Shared(T* node, std::function<void(T*)> cleanup)
      : _resource{node}, _cleanup{cleanup} {}
};

/**
   Shared reference to a resource

   Increases reference counter on construction and decreases it on destruction.
 */
template<typename T>
struct SharedReference {
  SharedReference(SharedReference const& other)
      : _shared_node{other._shared_node} {
    _shared_node->increment();
  }
  auto operator=(SharedReference const& other) -> SharedReference {
    _shared_node = other._shared_node;
    _shared_node->increment();
    return *this;
  }
  SharedReference(SharedReference&& other) : _shared_node{other._shared_node} {
    other._shared_node = nullptr;
  }
  auto operator=(SharedReference&& other) -> SharedReference& {
    _shared_node = other._shared_node;
    other._shared_node = nullptr;
    return *this;
  }
  ~SharedReference() {
    if (_shared_node) {
      _shared_node->decrement();
    }
  }
  static auto create(Shared<T>* node) -> SharedReference {
    if (node == nullptr) {
      std::abort();
    }
    return SharedReference{node};
  }
  static auto create(T* resource, std::function<void(T*)> cleanup)
      -> SharedReference {
    return SharedReference{Shared<T>::create(resource, cleanup)};
  }
  auto operator*() const -> T& { return _shared_node->get_ref(); }
  auto operator->() const -> T* { return _shared_node->get(); }
  auto get() const -> T* { return _shared_node->get(); }
  auto ref_count() -> size_t { return _shared_node->ref_count(); }

 private:
  Shared<T>* _shared_node;
  SharedReference(Shared<T>* node) : _shared_node{node} {
    _shared_node->increment();
  }
};
