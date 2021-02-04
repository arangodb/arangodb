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
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_IRESEARCH__IRESEARCH_CONTAINERS_H
#define ARANGODB_IRESEARCH__IRESEARCH_CONTAINERS_H 1

#include <memory>
#include <shared_mutex>
#include <unordered_map>

#include "Basics/Common.h"
#include "Basics/debugging.h"
#include "Basics/ReadWriteLock.h"
#include "Basics/WriteLocker.h"

#include "utils/async_utils.hpp"
#include "utils/hash_utils.hpp"
#include "utils/map_utils.hpp"
#include "utils/memory.hpp"
#include "utils/string.hpp"

namespace {

template <typename...>
struct typelist;

}

namespace arangodb {
namespace iresearch {

////////////////////////////////////////////////////////////////////////////////
/// @brief std::unique_lock<...> compliant wrapper for arangodb::ReadWriteLock
////////////////////////////////////////////////////////////////////////////////
class ReadMutex {
 public:
  ReadMutex(basics::ReadWriteLock& mtx) noexcept
    : _mtx(&mtx) {
  }
  ReadMutex(ReadMutex&& rhs) noexcept
    : _mtx(rhs._mtx) {
    rhs._mtx = nullptr;
  }

  void lock() { _mtx->lockRead(); }
  bool try_lock() noexcept { return _mtx->tryLockRead(); }
  void unlock() { _mtx->unlockRead(); }

 private:
  ReadMutex(ReadMutex const&) = delete;
  ReadMutex operator=(ReadMutex const&) = delete;
  ReadMutex operator=(ReadMutex&&) = delete;

  basics::ReadWriteLock* _mtx;
}; // ReadMutex

////////////////////////////////////////////////////////////////////////////////
/// @brief a read-mutex for a resource
////////////////////////////////////////////////////////////////////////////////
template<typename T>
class ResourceMutexT {
 public:
  explicit ResourceMutexT(T* resource) noexcept
      : _readMutex(_mutex),
        _resource(resource) {
  }
  ~ResourceMutexT() { reset(); }

  std::unique_lock<ReadMutex> lock() const {
    return std::unique_lock<ReadMutex>{ _readMutex };
  }

  std::unique_lock<ReadMutex> try_lock() const noexcept {
    return { _readMutex, std::try_to_lock };
  }

  // will block until a write lock can be acquired on the _mutex
  void reset() {
    WRITE_LOCKER(lock, _mutex);
    _resource = nullptr;
  }

  T* get() const noexcept {
    TRI_ASSERT(_mutex.isLocked());
    return _resource;
  }

  bool empty() const {
    auto lock = irs::make_unique_lock(_readMutex);
    return nullptr == _resource;
  }

 private:
  basics::ReadWriteLock _mutex;
  mutable ReadMutex _readMutex; // read-lock to prevent '_resource' reset()
  T* _resource;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief a read-mutex for a resource
////////////////////////////////////////////////////////////////////////////////
class ResourceMutex {
 public:
  typedef irs::async_utils::read_write_mutex::read_mutex ReadMutex;
  explicit ResourceMutex(void* resource) noexcept
      : _readMutex(_mutex), _resource(resource) {}
  ~ResourceMutex() { reset(); }
  operator bool() { return get() != nullptr; }
  ReadMutex& mutex() const noexcept {
    return _readMutex;
  }              // prevent '_resource' reset()
  void reset();  // will block until a write lock can be acquired on the _mutex

 protected:
  void* get() const noexcept { return _resource.load(); }

 private:
  irs::async_utils::read_write_mutex _mutex;  // read-lock to prevent '_resource' reset()
  mutable ReadMutex _readMutex;  // object that can be referenced by std::unique_lock
  std::atomic<void*> _resource;  // atomic because of 'operator bool()'
};

////////////////////////////////////////////////////////////////////////////////
/// @brief a read-mutex for a resource of a specific type
////////////////////////////////////////////////////////////////////////////////
template <typename T>
class TypedResourceMutex : public ResourceMutex {
 public:
  explicit TypedResourceMutex(T* value) : ResourceMutex(value) {}
  T* get() const { return static_cast<T*>(ResourceMutex::get()); }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief a wrapper around a type, placing the value on the heap to allow
///        declaration of map member variables whos' values are of the type
///        being declared
////////////////////////////////////////////////////////////////////////////////
template <typename T>
class UniqueHeapInstance {
 public:
  template <typename... Args,
            typename = typename std::enable_if<!std::is_same<typelist<UniqueHeapInstance>,
                                                             typelist<typename std::decay<Args>::type...>>::value>::type  // prevent matching of copy/move constructor
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
template <typename CharType, typename V>
struct UnorderedRefKeyMapBase {
 public:
  typedef std::unordered_map<irs::hashed_basic_string_ref<CharType>, std::pair<std::basic_string<CharType>, V>> MapType;

  typedef typename MapType::key_type KeyType;
  typedef V value_type;
  typedef std::hash<typename MapType::key_type::base_t> KeyHasher;

  struct KeyGenerator {
    KeyType operator()(KeyType const& key, typename MapType::mapped_type const& value) const {
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
template <typename CharType, typename V>
class UnorderedRefKeyMap : public UnorderedRefKeyMapBase<CharType, V>,
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
    return irs::map_utils::try_emplace_update_key(_map, keyGenerator(),
                                                  key,  // use same key for MapType::key_type and
                                                        // MapType::value_type.first
                                                  std::piecewise_construct,
                                                  std::forward_as_tuple(key),
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

  template <typename... Args>
  std::pair<Iterator, bool> emplace(KeyType const& key, Args&&... args) {
    auto res = irs::map_utils::try_emplace_update_key(
        _map, keyGenerator(),
        key,  // use same key for MapType::key_type and
              // MapType::value_type.first
        std::piecewise_construct, std::forward_as_tuple(key),
        std::forward_as_tuple(std::forward<Args>(args)...)  // MapType::value_type
    );

    return std::make_pair(Iterator(res.first), res.second);
  }

  template <typename... Args>
  std::pair<Iterator, bool> emplace(typename KeyType::base_t const& key, Args&&... args) {
    return emplace(irs::make_hashed_ref(key, keyHasher()), std::forward<Args>(args)...);
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

#endif
