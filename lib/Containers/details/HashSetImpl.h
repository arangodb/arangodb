// By Emil Ernerfeldt 2014-2016
// LICENSE:
//   This software is dual-licensed to the public domain and under the following
//   license: you are granted a perpetual, irrevocable license to copy, modify,
//   publish, and distribute this file as you see fit.

#ifndef ARANGODB_CONTAINERS_HASH_SET_IMPL_H
#define ARANGODB_CONTAINERS_HASH_SET_IMPL_H 1

#include <algorithm>
#include <cstring>
#include <initializer_list>
#include <iterator>
#include <type_traits>
#include <utility>

namespace emilib {

/// like std::equal_to but no need to `#include <functional>`
template <typename T>
struct HashSetEqualTo {
  constexpr bool operator()(const T& lhs, const T& rhs) const {
    return lhs == rhs;
  }
};

/// A cache-friendly hash set with open addressing, linear probing and power-of-two capacity
template <typename KeyT, typename HashT = std::hash<KeyT>, typename EqT = HashSetEqualTo<KeyT>>
class HashSet {
 private:
  using MyType = HashSet<KeyT, HashT, EqT>;

 public:
  using size_type = size_t;
  using value_type = KeyT;
  using reference = KeyT&;
  using const_reference = const KeyT&;

  class iterator {
   public:
    using iterator_category = std::forward_iterator_tag;
    using difference_type = size_t;
    using distance_type = size_t;
    using value_type = KeyT;
    using pointer = value_type*;
    using reference = value_type&;

    iterator() {}

    iterator(MyType* hash_set, size_t bucket)
        : _set(hash_set), _bucket(bucket) {}

    iterator& operator++() {
      this->goto_next_element();
      return *this;
    }

    iterator operator++(int) {
      size_t old_index = _bucket;
      this->goto_next_element();
      return iterator(_set, old_index);
    }

    reference operator*() const { return _set->_keys[_bucket]; }

    pointer operator->() const { return _set->_keys + _bucket; }

    bool operator==(const iterator& rhs) const {
      return this->_bucket == rhs._bucket;
    }

    bool operator!=(const iterator& rhs) const {
      return this->_bucket != rhs._bucket;
    }

   private:
    void goto_next_element() {
      do {
        _bucket++;
      } while (_bucket < _set->_num_buckets && _set->_states[_bucket] != State::FILLED);
    }

   public:
    MyType* _set;
    size_t _bucket;
  };

  class const_iterator {
   public:
    using iterator_category = std::forward_iterator_tag;
    using difference_type = size_t;
    using distance_type = size_t;
    using value_type = const KeyT;
    using pointer = value_type*;
    using reference = value_type&;

    const_iterator() {}

    // cppcheck-suppress noExplicitConstructor
    /* implicit */ const_iterator(iterator proto)
        : _set(proto._set), _bucket(proto._bucket) {}

    const_iterator(const MyType* hash_set, size_t bucket)
        : _set(hash_set), _bucket(bucket) {}

    const_iterator& operator++() {
      this->goto_next_element();
      return *this;
    }

    const_iterator operator++(int) {
      size_t old_index = _bucket;
      this->goto_next_element();
      return const_iterator(_set, old_index);
    }

    reference operator*() const { return _set->_keys[_bucket]; }

    pointer operator->() const { return _set->_keys + _bucket; }

    bool operator==(const const_iterator& rhs) const {
      return this->_bucket == rhs._bucket;
    }

    bool operator!=(const const_iterator& rhs) const {
      return this->_bucket != rhs._bucket;
    }

   private:
    void goto_next_element() {
      do {
        _bucket++;
      } while (_bucket < _set->_num_buckets && _set->_states[_bucket] != State::FILLED);
    }

   public:
    const MyType* _set;
    size_t _bucket;
  };

  HashSet() = default;

  HashSet(const HashSet& other) {
    reserve(other.size());
    insert(other.cbegin(), other.cend());
  }

  HashSet(HashSet&& other) noexcept { doMove(std::move(other)); }

  explicit HashSet(std::initializer_list<KeyT> const& values) : HashSet() {
    for (auto const& v : values) {
      insert(v);
    }
  }

  HashSet& operator=(const HashSet& other) {
    if (this != &other) {
      clear();
      reserve(other.size());
      insert(other.cbegin(), other.cend());
    }
    return *this;
  }

  HashSet& operator=(HashSet&& other) noexcept {
    if (this != &other) {
      for (size_t bucket = 0; bucket < _num_buckets; ++bucket) {
        if (_states[bucket] == State::FILLED) {
          _keys[bucket].~KeyT();
        }
      }
      if (_buffer != &_local_buffer[0]) {
        delete[] _buffer;
      }
      doMove(std::move(other));
    }
    return *this;
  }

  ~HashSet() {
    for (size_t bucket = 0; bucket < _num_buckets; ++bucket) {
      if (_states[bucket] == State::FILLED) {
        _keys[bucket].~KeyT();
      }
    }
    if (_buffer != &_local_buffer[0]) {
      delete[] _buffer;
    }
  }

  void swap(HashSet& other) {
    std::swap(_hasher, other._hasher);
    std::swap(_eq, other._eq);
    std::swap(_buffer, other._buffer);
    std::swap(_states, other._states);
    std::swap(_keys, other._keys);
    std::swap(_num_buckets, other._num_buckets);
    std::swap(_num_filled, other._num_filled);
    std::swap(_max_probe_length, other._max_probe_length);
    std::swap(_mask, other._mask);
  }

  // -------------------------------------------------------------

  iterator begin() {
    size_t bucket = 0;
    while (bucket < _num_buckets && _states[bucket] != State::FILLED) {
      ++bucket;
    }
    return iterator(this, bucket);
  }

  const_iterator cbegin() const {
    size_t bucket = 0;
    while (bucket < _num_buckets && _states[bucket] != State::FILLED) {
      ++bucket;
    }
    return const_iterator(this, bucket);
  }

  const_iterator begin() const { return cbegin(); }

  iterator end() { return iterator(this, _num_buckets); }

  const_iterator cend() const { return const_iterator(this, _num_buckets); }

  const_iterator end() const { return cend(); }

  size_t size() const { return _num_filled; }

  bool empty() const { return _num_filled == 0; }

  // Returns the number of buckets.
  size_t bucket_count() const { return _num_buckets; }

  /// Returns average number of elements per bucket.
  float load_factor() const {
    return static_cast<float>(_num_filled) / static_cast<float>(_num_buckets);
  }

  // ------------------------------------------------------------

  iterator find(const KeyT& key) {
    auto bucket = this->find_filled_bucket(key);
    if (bucket == (size_t)-1) {
      return this->end();
    }
    return iterator(this, bucket);
  }

  const_iterator find(const KeyT& key) const {
    auto bucket = this->find_filled_bucket(key);
    if (bucket == (size_t)-1) {
      return this->end();
    }
    return const_iterator(this, bucket);
  }

  bool contains(const KeyT& k) const {
    return find_filled_bucket(k) != (size_t)-1;
  }

  size_t count(const KeyT& k) const {
    return find_filled_bucket(k) != (size_t)-1 ? 1 : 0;
  }

  // -----------------------------------------------------

  /// Insert an element, unless it already exists.
  /// Returns a pair consisting of an iterator to the inserted element
  /// (or to the element that prevented the insertion)
  /// and a bool denoting whether the insertion took place.
  std::pair<iterator, bool> insert(const KeyT& key) {
    check_expand_need();

    auto bucket = find_or_allocate(key);

    if (_states[bucket] == State::FILLED) {
      return {iterator(this, bucket), false};
    } else {
      new (_keys + bucket) KeyT(key);
      _states[bucket] = State::FILLED;
      _num_filled++;
      return {iterator(this, bucket), true};
    }
  }

  /// Insert an element, unless it already exists.
  /// Returns a pair consisting of an iterator to the inserted element
  /// (or to the element that prevented the insertion)
  /// and a bool denoting whether the insertion took place.
  std::pair<iterator, bool> insert(KeyT&& key) {
    check_expand_need();

    auto bucket = find_or_allocate(key);

    if (_states[bucket] == State::FILLED) {
      return {iterator(this, bucket), false};
    } else {
      new (_keys + bucket) KeyT(std::move(key));
      _states[bucket] = State::FILLED;
      _num_filled++;
      return {iterator(this, bucket), true};
    }
  }

  template <class... Args>
  std::pair<iterator, bool> emplace(Args&&... args) {
    return insert(KeyT(std::forward<Args>(args)...));
  }

  void insert(const_iterator begin, const_iterator end) {
    // TODO: reserve space exactly once.
    for (; begin != end; ++begin) {
      insert(*begin);
    }
  }

  // -------------------------------------------------------

  /// Erase an element from the hash set.
  /// return false if element was not found.
  bool erase(const KeyT& key) {
    auto bucket = find_filled_bucket(key);
    if (bucket != (size_t)-1) {
      _states[bucket] = State::ACTIVE;
      _keys[bucket].~KeyT();
      _num_filled -= 1;
      return true;
    } else {
      return false;
    }
  }

  /// Erase an element using an iterator.
  /// Returns an iterator to the next element (or end()).
  iterator erase(iterator it) {
    _states[it._bucket] = State::ACTIVE;
    _keys[it._bucket].~KeyT();
    _num_filled -= 1;
    return ++it;
  }

  /// Remove all elements, keeping full capacity.
  void clear() {
    for (size_t bucket = 0; bucket < _num_buckets; ++bucket) {
      if (_states[bucket] == State::FILLED) {
        _states[bucket] = State::INACTIVE;
        _keys[bucket].~KeyT();
      }
    }
    _num_filled = 0;
    _max_probe_length = -1;
  }

  /// Make room for this many elements
  void reserve(size_t num_elems) {
    size_t required_buckets = num_elems + num_elems / 2 + 1;
    if (required_buckets <= _num_buckets) {
      return;
    }
    size_t num_buckets = 8;
    while (num_buckets < required_buckets) {
      num_buckets *= 2;
    }

    size_t states_size = ((num_buckets * sizeof(State) + 8 - 1) / 8) * 8;
    size_t keys_size = num_buckets * sizeof(KeyT);
    // intentionally uninitialized here, initialization will be done below
    char* new_buffer;
    if (_buffer == nullptr && states_size + keys_size <= sizeof(_local_buffer)) {
      new_buffer = &_local_buffer[0];
    } else {
      new_buffer = new char[states_size + keys_size];
    }
    auto new_states = reinterpret_cast<State*>(new_buffer);
    auto new_keys = reinterpret_cast<KeyT*>(new_buffer + states_size);

    // auto old_num_filled  = _num_filled;
    auto old_num_buckets = _num_buckets;
    auto old_buffer = _buffer;
    auto old_states = _states;
    auto old_keys = _keys;

    _num_filled = 0;
    _num_buckets = num_buckets;
    _mask = _num_buckets - 1;
    _buffer = new_buffer;
    _states = new_states;
    _keys = new_keys;

    std::fill_n(_states, num_buckets, State::INACTIVE);

    _max_probe_length = -1;

    for (size_t src_bucket = 0; src_bucket < old_num_buckets; src_bucket++) {
      if (old_states[src_bucket] == State::FILLED) {
        auto& src = old_keys[src_bucket];

        auto dst_bucket = find_empty_bucket(src);
        _states[dst_bucket] = State::FILLED;
        new (_keys + dst_bucket) KeyT(std::move(src));
        _num_filled += 1;

        src.~KeyT();
      }
    }

    if (old_buffer != &_local_buffer[0]) {
      delete[] old_buffer;
    }
  }

 private:
  void doMove(HashSet&& other) noexcept {
    if (other._buffer == nullptr) {
      _buffer = nullptr;
      _states = nullptr;
      _keys = nullptr;
    } else if (other._buffer != &other._local_buffer[0]) {
      // we can just steal the other's buffer
      _buffer = other._buffer;
      _states = other._states;
      _keys = other._keys;
    } else {
      // we can copy the other's local buffer
      _buffer = &_local_buffer[0];
      _states = reinterpret_cast<State*>(_buffer);
      size_t states_size = ((other._num_buckets * sizeof(State) + 8 - 1) / 8) * 8;
      memcpy(&_local_buffer[0], &other._local_buffer[0], states_size);
      _keys = (KeyT*)(_buffer + states_size);

      for (size_t src_bucket = 0; src_bucket < other._num_buckets; src_bucket++) {
        if (other._states[src_bucket] == State::FILLED) {
          auto& src = other._keys[src_bucket];
          new (_keys + src_bucket) KeyT(std::move(src));
        }
      }
    }

    _num_buckets = other._num_buckets;
    _num_filled = other._num_filled;
    _max_probe_length = other._max_probe_length;
    _mask = other._mask;

    other._buffer = nullptr;
    other._states = nullptr;
    other._keys = nullptr;
    other._num_buckets = 0;
    other._num_filled = 0;
    other._max_probe_length = -1;
    other._mask = 0;
  }

  // Can we fit another element?
  void check_expand_need() { reserve(_num_filled + 1); }

  // Find the bucket with this key, or return (size_t)-1
  size_t find_filled_bucket(const KeyT& key) const {
    if (empty()) {
      return (size_t)-1;
    }  // Optimization

    auto hash_value = _hasher(key);
    for (int offset = 0; offset <= _max_probe_length; ++offset) {
      auto bucket = (hash_value + offset) & _mask;
      if (_states[bucket] == State::FILLED) {
        if (_eq(_keys[bucket], key)) {
          return bucket;
        }
      } else if (_states[bucket] == State::INACTIVE) {
        return (size_t)-1;  // End of the chain!
      }
    }
    return (size_t)-1;
  }

  // Find the bucket with this key, or return a good empty bucket to place the
  // key in. In the latter case, the bucket is expected to be filled.
  size_t find_or_allocate(const KeyT& key) {
    auto hash_value = _hasher(key);
    size_t hole = (size_t)-1;
    int offset = 0;
    for (; offset <= _max_probe_length; ++offset) {
      auto bucket = (hash_value + offset) & _mask;

      if (_states[bucket] == State::FILLED) {
        if (_eq(_keys[bucket], key)) {
          return bucket;
        }
      } else if (_states[bucket] == State::INACTIVE) {
        return bucket;
      } else {
        // ACTIVE: keep searching
        if (hole == (size_t)-1) {
          hole = bucket;
        }
      }
    }

    // No key found - but maybe a hole for it
    if (hole != (size_t)-1) {
      return hole;
    }

    // No hole found within _max_probe_length
    for (;; ++offset) {
      auto bucket = (hash_value + offset) & _mask;

      if (_states[bucket] != State::FILLED) {
        _max_probe_length = offset;
        return bucket;
      }
    }
  }

  // key is not in this map. Find a place to put it.
  size_t find_empty_bucket(const KeyT& key) {
    auto hash_value = _hasher(key);
    for (int offset = 0;; ++offset) {
      auto bucket = (hash_value + offset) & _mask;
      if (_states[bucket] != State::FILLED) {
        if (offset > _max_probe_length) {
          _max_probe_length = offset;
        }
        return bucket;
      }
    }
  }

 private:
  enum class State : uint8_t {
    INACTIVE,  // Never been touched
    ACTIVE,    // Is inside a search-chain, but is empty
    FILLED     // Is set with key/value
  };

  HashT _hasher;
  EqT _eq;
  char* _buffer = nullptr;
  State* _states = nullptr;
  KeyT* _keys = nullptr;
  size_t _num_buckets = 0;
  size_t _num_filled = 0;
  int _max_probe_length = -1;  // Our longest bucket-brigade is this long. ONLY when we have zero elements is this ever negative (-1).
  size_t _mask = 0;            // _num_buckets minus one
  char _local_buffer[80];      // first few bytes are always allocated here
};

}  // namespace emilib

#endif
