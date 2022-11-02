////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <unordered_map>

#include "Basics/Common.h"
#include "Basics/debugging.h"
#include "Basics/ReadWriteLock.h"
#include "Basics/WriteLocker.h"

#include "utils/hash_utils.hpp"
#include "utils/map_utils.hpp"
#include "utils/memory.hpp"
#include "utils/string.hpp"
#include "utils/thread_utils.hpp"

namespace {

template<typename...>
struct typelist;

}

namespace arangodb {
namespace iresearch {

////////////////////////////////////////////////////////////////////////////////
/// @brief A wrapper to control the lifetime of an object
///        that is used by multiple threads.
///
/// An analogue of shared_ptr and weak_ptr,
/// in fact AsyncValue is both shared_ptr and weak_ptr, before they called
/// AsyncValue::reset, after it only weak_ptr to already destroyed shared_ptr.
////////////////////////////////////////////////////////////////////////////////
template<typename T>
class AsyncValue {
 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief Same semantics as shared_ptr, expect copy ctor and assign
  ///        (easy to add but we don't want it).
  //////////////////////////////////////////////////////////////////////////////
  class Value {
   public:
    constexpr Value() noexcept : _self{nullptr} {}
    constexpr Value(Value&& other) noexcept
        : _self{std::exchange(other._self, nullptr)} {}
    constexpr Value& operator=(Value&& other) noexcept {
      std::swap(_self, other._self);
      return *this;
    }

    T* get() noexcept { return _self ? _self->_resource : nullptr; }
    T const* get() const noexcept { return _self ? _self->_resource : nullptr; }
    T* operator->() noexcept { return get(); }
    T const* operator->() const noexcept { return get(); }

    explicit operator bool() const noexcept { return get() != nullptr; }

    ~Value() {
      if (_self) {
        _self->destroy();
      }
    }

   private:
    friend class AsyncValue;
    constexpr Value(AsyncValue& self) noexcept : _self{&self} {}

    AsyncValue* _self;
  };

  explicit AsyncValue(T* resource) noexcept
      : _resource{resource}, _count{[&] {
          if (!resource) {
            return kReset | kDestroy;
          } else {
            return kRef;
          }
        }()} {}

  ~AsyncValue() { reset(); }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief If return true then reset was call
  //////////////////////////////////////////////////////////////////////////////
  [[nodiscard]] bool empty() const noexcept {
    return _count.load(std::memory_order_acquire) & kReset;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Has the same semantics as weak_ptr::lock
  //////////////////////////////////////////////////////////////////////////////
  [[nodiscard]] Value lock() noexcept {
    if (empty()) {
      return {};
    }
    if (_count.fetch_add(kRef, std::memory_order_acquire) & kDestroy) {
      return {};
    }
    return {*this};
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Denies access to resource granted by a lock,
  ///        and waits until all locks on resource have been released.
  //////////////////////////////////////////////////////////////////////////////
  void reset() noexcept {
    auto count = _count.fetch_or(kReset, std::memory_order_release);
    if ((count & kDestroy) == kDestroy) {
      return;
    }
    if ((count & kReset) != kReset) {  // handle first reset
      if (destroy()) {                 // handle successfully destroy
        return;
      }
      count -= kRef - kReset;
    }
    do {  // wait destroy
      _count.wait(count, std::memory_order_relaxed);
      count = _count.load(std::memory_order_acquire);
    } while ((count & kDestroy) != kDestroy);
  }

 private:
  static constexpr uint32_t kReset = 1;
  static constexpr uint32_t kDestroy = 2;
  static constexpr uint32_t kRef = 4;

  bool destroy() noexcept {
    auto count = _count.fetch_sub(kRef, std::memory_order_release) - kRef;
    if (count == kReset &&
        _count.compare_exchange_strong(count, kReset | kDestroy,
                                       std::memory_order_acq_rel,
                                       std::memory_order_relaxed)) {
      _count.notify_all();
      return true;
    }
    return false;
  }

  T* _resource;
  mutable std::atomic_uint32_t _count;  // linux futex handle only 32 bit
};

////////////////////////////////////////////////////////////////////////////////
/// @brief a wrapper around a type, placing the value on the heap to allow
///        declaration of map member variables whos' values are of the type
///        being declared
////////////////////////////////////////////////////////////////////////////////
template<typename T>
class UniqueHeapInstance {
 public:
  template<typename... Args,
           typename = typename std::enable_if<!std::is_same<
               typelist<UniqueHeapInstance>,
               typelist<typename std::decay<Args>::type...>>::value>::
               type  // prevent matching of copy/move constructor
           >
  explicit UniqueHeapInstance(Args&&... args)
      : _instance(irs::memory::make_unique<T>(std::forward<Args>(args)...)) {}

  UniqueHeapInstance(UniqueHeapInstance const& other)
      : _instance(irs::memory::make_unique<T>(*(other._instance))) {}

  UniqueHeapInstance(UniqueHeapInstance&& other) noexcept
      : _instance(std::move(other._instance)) {}

  UniqueHeapInstance& operator=(UniqueHeapInstance const& other) {
    if (this != &other) {
      _instance = irs::memory::make_unique<T>(*(other._instance));
    }

    return *this;
  }

  UniqueHeapInstance& operator=(UniqueHeapInstance&& other) noexcept {
    if (this != &other) {
      _instance = std::move(other._instance);
    }

    return *this;
  }

  T& operator=(T const& other) {
    *_instance = other;

    return *_instance;
  }

  T& operator=(T&& other) {
    *_instance = std::move(other);

    return *_instance;
  }

  T& operator*() const noexcept { return *_instance; }
  T* operator->() const noexcept { return _instance.get(); }

  bool operator==(UniqueHeapInstance const& other) const {
    return _instance ? (other._instance && *_instance == *(other._instance))
                     : !other._instance;
  }

  bool operator!=(UniqueHeapInstance const& other) const {
    return !(*this == other);
  }

  T* get() noexcept { return _instance.get(); }
  T const* get() const noexcept { return _instance.get(); }

 private:
  std::unique_ptr<T> _instance;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief a base class for UnorderedRefKeyMap providing implementation for
///        KeyHasher and KeyGenerator
////////////////////////////////////////////////////////////////////////////////
template<typename CharType, typename V>
struct UnorderedRefKeyMapBase {
 public:
  typedef std::unordered_map<irs::hashed_basic_string_ref<CharType>,
                             std::pair<std::basic_string<CharType>, V>>
      MapType;

  typedef typename MapType::key_type KeyType;
  typedef V value_type;
  typedef std::hash<typename MapType::key_type::base_t> KeyHasher;

  struct KeyGenerator {
    KeyType operator()(KeyType const& key,
                       typename MapType::mapped_type const& value) const {
      return KeyType(key.hash(), value.first);
    }
  };
};

////////////////////////////////////////////////////////////////////////////////
/// @brief a map whose key is an irs::hashed_basic_string_ref and the actual
///        key memory is in an std::pair beside the value
///        allowing the use of the map with an irs::basic_string_ref without
///        the need to allocaate memmory during find(...)
////////////////////////////////////////////////////////////////////////////////
template<typename CharType, typename V>
class UnorderedRefKeyMap
    : public UnorderedRefKeyMapBase<CharType, V>,
      private UnorderedRefKeyMapBase<CharType, V>::KeyGenerator,
      private UnorderedRefKeyMapBase<CharType, V>::KeyHasher {
 public:
  typedef UnorderedRefKeyMapBase<CharType, V> MyBase;
  typedef typename MyBase::MapType MapType;
  typedef typename MyBase::KeyType KeyType;
  typedef typename MyBase::KeyGenerator KeyGenerator;
  typedef typename MyBase::KeyHasher KeyHasher;

  class ConstIterator {
   public:
    bool operator==(ConstIterator const& other) const noexcept {
      return _itr == other._itr;
    }

    bool operator!=(ConstIterator const& other) const noexcept {
      return !(*this == other);
    }

    ConstIterator& operator*() noexcept { return *this; }

    ConstIterator& operator++() {
      ++_itr;

      return *this;
    }

    const KeyType& key() const noexcept { return _itr->first; }
    const V& value() const noexcept { return _itr->second.second; }

   private:
    friend UnorderedRefKeyMap;
    typename MapType::const_iterator _itr;

    explicit ConstIterator(typename MapType::const_iterator const& itr)
        : _itr(itr) {}
  };

  class Iterator {
   public:
    bool operator==(Iterator const& other) const noexcept {
      return _itr == other._itr;
    }

    bool operator!=(Iterator const& other) const noexcept {
      return !(*this == other);
    }

    Iterator& operator*() noexcept { return *this; }

    Iterator& operator++() {
      ++_itr;

      return *this;
    }

    const KeyType& key() const noexcept { return _itr->first; }
    V& value() const noexcept { return _itr->second.second; }

   private:
    friend UnorderedRefKeyMap;
    typename MapType::iterator _itr;

    explicit Iterator(typename MapType::iterator const& itr) : _itr(itr) {}
  };

  UnorderedRefKeyMap() = default;
  ~UnorderedRefKeyMap() {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    // ensure every key points to valid data
    for (auto& entry : _map) {
      TRI_ASSERT(entry.first.c_str() == entry.second.first.c_str());
    }
#endif
  }
  UnorderedRefKeyMap(UnorderedRefKeyMap const& other) { *this = other; }
  UnorderedRefKeyMap(UnorderedRefKeyMap&& other) noexcept
      : _map(std::move(other._map)) {}

  UnorderedRefKeyMap& operator=(UnorderedRefKeyMap const& other) {
    if (this != &other) {
      _map.clear();
      _map.reserve(other._map.size());

      for (auto& entry : other._map) {
        emplace(entry.first,
                entry.second.second);  // ensure that the key is regenerated
      }
    }

    return *this;
  }

  UnorderedRefKeyMap& operator=(UnorderedRefKeyMap&& other) {
    if (this != &other) {
      _map = std::move(other._map);
    }

    return *this;
  }

  V& operator[](KeyType const& key) {
    return irs::map_utils::try_emplace_update_key(
               _map, keyGenerator(),
               key,  // use same key for MapType::key_type and
                     // MapType::value_type.first
               std::piecewise_construct, std::forward_as_tuple(key),
               std::forward_as_tuple()  // MapType::value_type
               )
        .first->second.second;
  }

  V& operator[](typename KeyType::base_t const& key) {
    return (*this)[irs::make_hashed_ref(key, keyHasher())];
  }

  Iterator begin() noexcept { return Iterator(_map.begin()); }
  ConstIterator begin() const noexcept { return ConstIterator(_map.begin()); }

  void clear() noexcept { _map.clear(); }

  template<typename... Args>
  std::pair<Iterator, bool> emplace(KeyType const& key, Args&&... args) {
    auto res = irs::map_utils::try_emplace_update_key(
        _map, keyGenerator(),
        key,  // use same key for MapType::key_type and
              // MapType::value_type.first
        std::piecewise_construct, std::forward_as_tuple(key),
        std::forward_as_tuple(
            std::forward<Args>(args)...)  // MapType::value_type
    );

    return std::make_pair(Iterator(res.first), res.second);
  }

  template<typename... Args>
  std::pair<Iterator, bool> emplace(typename KeyType::base_t const& key,
                                    Args&&... args) {
    return emplace(irs::make_hashed_ref(key, keyHasher()),
                   std::forward<Args>(args)...);
  }

  bool empty() const noexcept { return _map.empty(); }

  Iterator end() noexcept { return Iterator(_map.end()); }
  ConstIterator end() const noexcept { return ConstIterator(_map.end()); }

  Iterator find(KeyType const& key) noexcept {
    return Iterator(_map.find(key));
  }

  Iterator find(typename KeyType::base_t const& key) noexcept {
    return find(irs::make_hashed_ref(key, keyHasher()));
  }

  ConstIterator find(KeyType const& key) const noexcept {
    return ConstIterator(_map.find(key));
  }

  ConstIterator find(typename KeyType::base_t const& key) const noexcept {
    return find(irs::make_hashed_ref(key, keyHasher()));
  }

  V* findPtr(KeyType const& key) noexcept {
    auto itr = _map.find(key);

    return itr == _map.end() ? nullptr : &(itr->second.second);
  }

  V* findPtr(typename KeyType::base_t const& key) noexcept {
    return findPtr(irs::make_hashed_ref(key, keyHasher()));
  }

  V const* findPtr(KeyType const& key) const noexcept {
    auto itr = _map.find(key);

    return itr == _map.end() ? nullptr : &(itr->second.second);
  }

  V const* findPtr(typename KeyType::base_t const& key) const noexcept {
    return findPtr(irs::make_hashed_ref(key, keyHasher()));
  }

  size_t size() const noexcept { return _map.size(); }

 private:
  MapType _map;

  constexpr KeyGenerator const& keyGenerator() const noexcept {
    return static_cast<KeyGenerator const&>(*this);
  }

  constexpr KeyHasher const& keyHasher() const noexcept {
    return static_cast<KeyHasher const&>(*this);
  }
};

}  // namespace iresearch
}  // namespace arangodb
