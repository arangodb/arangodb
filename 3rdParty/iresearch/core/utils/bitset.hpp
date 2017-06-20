//
// IResearch search engine 
// 
// Copyright (c) 2016 by EMC Corporation, All Rights Reserved
// 
// This software contains the intellectual property of EMC Corporation or is licensed to
// EMC Corporation from third parties. Use of this software and the intellectual property
// contained therein is expressly limited to the terms and conditions of the License
// Agreement under which it is provided by or on behalf of EMC.
// 

#ifndef IRESEARCH_BITSET_H
#define IRESEARCH_BITSET_H

#include <memory>

#include "shared.hpp"
#include "bit_utils.hpp"
#include "math_utils.hpp"
#include "noncopyable.hpp"
#include "memory.hpp"

NS_ROOT

class bitset : util::noncopyable {
 public:
  typedef size_t word_t;
  typedef size_t index_t;

  explicit bitset(size_t bits = 0) {
    reset(bits);
  }

  bitset(bitset&& rhs) NOEXCEPT
    : bits_(rhs.bits_),
      words_(rhs.words_),
      capacity_(rhs.capacity_),
      data_(std::move(rhs.data_)) {
    rhs.bits_ = 0;
    rhs.words_ = 0;
    rhs.capacity_ = 0;
  }

  bitset& operator=(bitset&& rhs) NOEXCEPT {
    if (this != &rhs) {
      bits_ = rhs.bits_;
      words_ = rhs.words_;
      capacity_ = rhs.capacity_;
      data_ = std::move(rhs.data_);
      rhs.bits_ = 0;
      rhs.words_ = 0;
      rhs.capacity_ = 0;
    }

    return *this;
  }

  void reset(size_t bits) {
    const auto words = bit_to_words(bits);
    if (words > capacity_) {
      data_ = memory::make_unique<word_t[]>(words);
      capacity_ = words;
    }
    words_ = words;
    bits_ = bits;
    clear();
  }

  // returns number of bits in bitset
  size_t size() const { return bits_; }
  size_t capacity() const { return capacity_; }
  size_t words() const { return words_; }

  const word_t* data() const { return data_.get(); }
  word_t* data() { return data_.get(); }

  void set(size_t i) NOEXCEPT {
    assert(i < bits_);
    auto* data = data_.get();
    const auto wi = word(i);
    set_bit(data[wi], bit(i, wi));
  }

  void unset(size_t i) NOEXCEPT {
    assert(i < bits_);
    auto* data = data_.get();
    const auto wi = word(i);
    unset_bit(data[wi], bit(i, wi));
  }

  void reset(size_t i, bool set) NOEXCEPT {
    assert(i < bits_);
    auto* data = data_.get();
    const auto wi = word(i);
    set_bit(data[wi], bit(i, wi), set);
  }

  bool test(size_t i) const NOEXCEPT {
    assert(i < bits_);
    auto* data = data_.get();
    const auto wi = word(i);
    return check_bit(data[wi], bit(i, wi));
  }

  bool any() const {
    auto* data = data_.get();
    return std::any_of(
      data, data + words_,
      [] (word_t w) { return w != 0; }
    );
  }

  bool none() const {
    return !any();
  }

  bool all() const {
    return (count() == size());
  }

  void clear() {
    std::memset(data_.get(), 0, sizeof(word_t)*words_);
  }

  // counts bits set
  word_t count() const NOEXCEPT {
    auto* data = data_.get();
    return std::accumulate(
      data, data + words_, word_t(0),
      [] (word_t v, word_t w) {
        return v + math::math_traits<word_t>::pop(w);
    });
  }

 private:
  typedef std::unique_ptr<word_t[]> word_ptr_t;

  FORCE_INLINE static size_t word(size_t i) NOEXCEPT {
    return i / bits_required<word_t>();
  }

  FORCE_INLINE static size_t bit(size_t i, size_t wi) NOEXCEPT {
    return i - bits_required<word_t>()*wi;
  }

  FORCE_INLINE static size_t bit_to_words(size_t bits) NOEXCEPT {
    return bits ? ((bits - 1) / bits_required<word_t>()) + 1 : 0;
  }

  size_t bits_{};    // number of bits in a bitset
  size_t words_{};   // number of words used for storing data
  size_t capacity_{}; // capacity in words
  word_ptr_t data_; // words array
}; // bitset

NS_END

#endif
