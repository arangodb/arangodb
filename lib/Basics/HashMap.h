/*
Copyright (c) 2017 Erik Rigtorp <erik@rigtorp.se>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
 */

/*
HashMap

A high performance hash map. Uses open addressing with linear
probing.

Advantages:
  - Predictable performance. Doesn't use the allocator unless load factor
    grows beyond 50%. Linear probing ensures cash efficency.
  - Deletes items by rearranging items and marking slots as empty instead of
    marking items as deleted. This is keeps performance high when there
    is a high rate of churn (many paired inserts and deletes) since otherwise
    most slots would be marked deleted and probing would end up scanning
    most of the table.

Disadvantages:
  - Significant performance degradation at high load factors.
  - Maximum load factor hard coded to 50%, memory inefficient.
  - Memory is not reclaimed on erase.
 */

#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

#include "Basics/SmallVector.h"

namespace rigtorp {

template <typename Key, typename T, typename Hash = std::hash<Key>,
          typename KeyEqual = std::equal_to<void>>
class HashMap {
public:
  using key_type = Key;
  using mapped_type = T;
  using value_type = std::pair<Key, T>;
  using size_type = std::size_t;
  using hasher = Hash;
  using key_equal = KeyEqual;
  using reference = value_type &;
  using const_reference = const value_type &;
  using buckets = arangodb::SmallVector<value_type>;

  template <typename ContT, typename IterVal> struct hm_iterator {
    using difference_type = std::ptrdiff_t;
    using value_type = IterVal;
    using pointer = value_type *;
    using reference = value_type &;
    using iterator_category = std::forward_iterator_tag;

    bool operator==(const hm_iterator &other) const {
      return other.hm_ == hm_ && other.idx_ == idx_;
    }
    bool operator!=(const hm_iterator &other) const {
      return !(other == *this);
    }

    hm_iterator &operator++() {
      ++idx_;
      advance_past_empty();
      return *this;
    }

    reference operator*() const { return hm_->buckets_[idx_]; }
    pointer operator->() const { return &hm_->buckets_[idx_]; }

  private:
    explicit hm_iterator(ContT *hm) : hm_(hm) { advance_past_empty(); }
    explicit hm_iterator(ContT *hm, size_type idx) : hm_(hm), idx_(idx) {}
    template <typename OtherContT, typename OtherIterVal>
    hm_iterator(const hm_iterator<OtherContT, OtherIterVal> &other)
        : hm_(other.hm_), idx_(other.idx_) {}

    void advance_past_empty() {
      while (idx_ < hm_->buckets_.size() &&
             key_equal()(hm_->buckets_[idx_].first, hm_->empty_key_)) {
        ++idx_;
      }
    }

    ContT *hm_ = nullptr;
    typename ContT::size_type idx_ = 0;
    friend ContT;
  };

  using iterator = hm_iterator<HashMap, value_type>;
  using const_iterator = hm_iterator<const HashMap, const value_type>;

public:
  HashMap(size_type bucket_count, key_type empty_key) : empty_key_(empty_key), buckets_(arena_) {
    size_t pow2 = 1;
    while (pow2 < bucket_count) {
      pow2 <<= 1;
    }
    buckets_.resize(pow2, std::make_pair(empty_key_, T()));
  }

  HashMap(const HashMap &other, size_type bucket_count)
      : HashMap(bucket_count, other.empty_key_) {
    for (auto it = other.begin(); it != other.end(); ++it) {
      insert(*it);
    }
  }

  // Iterators
  iterator begin() { return iterator(this); }

  const_iterator begin() const { return const_iterator(this); }

  const_iterator cbegin() const { return const_iterator(this); }

  iterator end() { return iterator(this, buckets_.size()); }

  const_iterator end() const { return const_iterator(this, buckets_.size()); }

  const_iterator cend() const { return const_iterator(this, buckets_.size()); }

  // Capacity
  bool empty() const { return size() == 0; }

  size_type size() const { return size_; }

  size_type max_size() const { return std::numeric_limits<size_type>::max(); }

  // Modifiers
  void clear() {
    HashMap other(bucket_count(), empty_key_);
    swap(other);
  }

  std::pair<iterator, bool> insert(const value_type &value) {
    return emplace_impl(value.first, value.second);
  }

  std::pair<iterator, bool> insert(value_type &&value) {
    return emplace_impl(value.first, std::move(value.second));
  }

  template <typename... Args>
  std::pair<iterator, bool> emplace(Args &&... args) {
    return emplace_impl(std::forward<Args>(args)...);
  }

  void erase(iterator it) { erase_impl(it); }

  size_type erase(const key_type &key) { return erase_impl(key); }

  template <typename K> size_type erase(const K &x) { return erase_impl(x); }

  void swap(HashMap &other) {
    std::swap(buckets_, other.buckets_);
    std::swap(size_, other.size_);
    std::swap(empty_key_, other.empty_key_);
  }

  // Lookup
  mapped_type &at(const key_type &key) { return at_impl(key); }

  template <typename K> mapped_type &at(const K &x) { return at_impl(x); }

  const mapped_type &at(const key_type &key) const { return at_impl(key); }

  template <typename K> const mapped_type &at(const K &x) const {
    return at_impl(x);
  }

  mapped_type &operator[](const key_type &key) {
    return emplace_impl(key).first->second;
  }

  size_type count(const key_type &key) const { return count_impl(key); }

  template <typename K> size_type count(const K &x) const {
    return count_impl(x);
  }

  iterator find(const key_type &key) { return find_impl(key); }

  template <typename K> iterator find(const K &x) { return find_impl(x); }

  const_iterator find(const key_type &key) const { return find_impl(key); }

  template <typename K> const_iterator find(const K &x) const {
    return find_impl(x);
  }

  // Bucket interface
  size_type bucket_count() const noexcept { return buckets_.size(); }

  size_type max_bucket_count() const noexcept {
    return std::numeric_limits<size_type>::max();
  }

  // Hash policy
  void rehash(size_type count) {
    count = std::max(count, size() * 2);
    HashMap other(*this, count);
    swap(other);
  }

  void reserve(size_type count) {
    if (count * 2 > buckets_.size()) {
      rehash(count * 2);
    }
  }

  // Observers
  hasher hash_function() const { return hasher(); }

  key_equal key_eq() const { return key_equal(); }

private:
  template <typename K, typename... Args>
  std::pair<iterator, bool> emplace_impl(const K &key, Args &&... args) {
    assert(!key_equal()(empty_key_, key) && "empty key shouldn't be used");
    reserve(size_ + 1);
    for (size_t idx = key_to_idx(key);; idx = probe_next(idx)) {
      if (key_equal()(buckets_[idx].first, empty_key_)) {
        buckets_[idx].second = mapped_type(std::forward<Args>(args)...);
        buckets_[idx].first = key;
        size_++;
        return {iterator(this, idx), true};
      } else if (key_equal()(buckets_[idx].first, key)) {
        return {iterator(this, idx), false};
      }
    }
  }

  void erase_impl(iterator it) {
    size_t bucket = it.idx_;
    for (size_t idx = probe_next(bucket);; idx = probe_next(idx)) {
      if (key_equal()(buckets_[idx].first, empty_key_)) {
        buckets_[bucket].first = empty_key_;
        size_--;
        return;
      }
      size_t ideal = key_to_idx(buckets_[idx].first);
      if (diff(bucket, ideal) < diff(idx, ideal)) {
        // swap, bucket is closer to ideal than idx
        buckets_[bucket] = buckets_[idx];
        bucket = idx;
      }
    }
  }

  template <typename K> size_type erase_impl(const K &key) {
    auto it = find_impl(key);
    if (it != end()) {
      erase_impl(it);
      return 1;
    }
    return 0;
  }

  template <typename K> mapped_type &at_impl(const K &key) {
    iterator it = find_impl(key);
    if (it != end()) {
      return it->second;
    }
    throw std::out_of_range("HashMap::at");
  }

  template <typename K> const mapped_type &at_impl(const K &key) const {
    return const_cast<HashMap *>(this)->at_impl(key);
  }

  template <typename K> size_t count_impl(const K &key) const {
    return find_impl(key) == end() ? 0 : 1;
  }

  template <typename K> iterator find_impl(const K &key) {
    assert(!key_equal()(empty_key_, key) && "empty key shouldn't be used");
    for (size_t idx = key_to_idx(key);; idx = probe_next(idx)) {
      if (key_equal()(buckets_[idx].first, key)) {
        return iterator(this, idx);
      }
      if (key_equal()(buckets_[idx].first, empty_key_)) {
        return end();
      }
    }
  }

  template <typename K> const_iterator find_impl(const K &key) const {
    return const_cast<HashMap *>(this)->find_impl(key);
  }

  template <typename K> size_t key_to_idx(const K &key) const {
    const size_t mask = buckets_.size() - 1;
    return hasher()(key) & mask;
  }

  size_t probe_next(size_t idx) const {
    const size_t mask = buckets_.size() - 1;
    return (idx + 1) & mask;
  }

  size_t diff(size_t a, size_t b) const {
    const size_t mask = buckets_.size() - 1;
    return (buckets_.size() + (a - b)) & mask;
  }

private:
  key_type empty_key_;
  typename buckets::allocator_type::arena_type arena_;
  buckets buckets_;
  size_t size_ = 0;
};
}

// map HashMap into arangodb namespace
namespace arangodb {

template<typename KeyT, typename ValueT, typename HashT = std::hash<KeyT>, typename EqT = std::equal_to<void>>
using HashMap = rigtorp::HashMap<KeyT, ValueT, HashT, EqT>;

}
