////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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

#ifndef IRESEARCH_BITVECTOR_H
#define IRESEARCH_BITVECTOR_H

#include "bitset.hpp"

namespace iresearch {

////////////////////////////////////////////////////////////////////////////////
/// @brief a growable implementation of a bitset
////////////////////////////////////////////////////////////////////////////////
class bitvector final {
 public:
  typedef bitset::word_t word_t;

  bitvector() = default;
  explicit bitvector(size_t bits): size_(bits) { resize(bits); }
  bitvector(const bitvector& other) { *this = other; }
  bitvector(bitvector&& other) noexcept { *this = std::move(other); }

  bool operator==(const bitvector& rhs) const noexcept {
    if (this->size() != rhs.size()) {
      return false;
    }

    return 0 == std::memcmp(this->begin(), rhs.begin(), this->size());
  }

  bool operator!=(const bitvector& rhs) const noexcept {
    return !(*this == rhs);
  }

  operator const bitset&() const { return set_; }

  bitvector& operator=(const bitvector& other) {
    if (this != &other) {
      if (set_.words() < other.set_.words()) {
        bitset set(other.set_.words() * bits_required<word_t>());

        set.memset(other.set_.begin(), other.set_.words() * sizeof(word_t));
        set_ = std::move(set);
      } else { // optimization, reuse existing container
        set_.clear();
        set_.memset(other.set_.begin(), other.set_.words() * sizeof(word_t));
      }

      size_ = other.size_;
    }

    return *this;
  }

  bitvector& operator=(bitvector&& other) noexcept {
    if (this != &other) {
      set_ = std::move(other.set_);
      size_ = std::move(other.size_);
      other.size_ = 0;
    }

    return *this;
  }

  bitvector& operator&=(const bitvector& other) {
    if (this == &other || !other.size()) {
      return *this; // nothing to do
    }

    reserve(other.size_);
    size_ = std::max(size_, other.size_);

    auto* data = const_cast<word_t*>(begin());
    auto last_word = bitset::word(other.size() - 1); // -1 for bit offset

    for (size_t i = 0; i < last_word; ++i) {
      assert(i < set_.words() && i < other.set_.words());
      *(data + i) &= *(other.data() + i);
    }

    // for the last word consider only those bits included in 'other.size()'
    auto last_word_bits = other.size() % bits_required<word_t>();
    const auto mask = (word_t(1) << last_word_bits) - 1; // set all bits that are not part of 'other.size()'

    assert(last_word < set_.words() && last_word < other.set_.words());
    *(data + last_word) &= (*(other.data() + last_word) & mask);
    std::memset(data + last_word + 1, 0, (set_.words() - last_word - 1) * sizeof(word_t)); // unset tail words

    return *this;
  }

  bitvector& operator|=(const bitvector& other) {
    if (this == &other || !other.size()) {
      return *this; // nothing to do
    }

    reserve(other.size_);
    size_ = std::max(size_, other.size_);

    auto* data = const_cast<word_t*>(begin());
    auto last_word = bitset::word(other.size() - 1); // -1 for bit offset

    for (size_t i = 0; i <= last_word; ++i) {
      assert(i < set_.words() && i < other.set_.words());
      *(data + i) |= *(other.data() + i);
    }

    return *this;
  }

  bitvector& operator^=(const bitvector& other) {
    if (!other.size()) {
      return *this; // nothing to do
    }

    reserve(other.size_);
    size_ = std::max(size_, other.size_);

    auto* data = const_cast<word_t*>(begin());
    auto last_word = bitset::word(other.size() - 1); // -1 for bit offset

    for (size_t i = 0; i < last_word; ++i) {
      assert(i < set_.words() && i < other.set_.words());
      *(data + i) ^= *(other.data() + i);
    }

    // for the last word consider only those bits included in 'other.size()'
    auto last_word_bits = other.size() % bits_required<word_t>();
    auto mask = ~word_t(0);

    // clear trailing bits
    if (last_word_bits) {
      mask = ~(mask << last_word_bits); // unset all bits that are not part of 'other.size()'
    }

    assert(last_word < set_.words() && last_word < other.set_.words());
    *(data + last_word) ^= (*(other.data() + last_word) & mask);

    return *this;
  }

  bitvector& operator-=(const bitvector& other) {
    if (!other.size()) {
      return *this; // nothing to do
    }

    reserve(other.size_);
    size_ = std::max(size_, other.size_);
    auto* data = const_cast<word_t*>(begin());
    auto last_word = bitset::word(other.size() - 1); // -1 for bit offset

    for (size_t i = 0; i < last_word; ++i) {
      assert(i < set_.words() && i < other.set_.words());
      *(data + i) &= ~(*(other.data() + i));
    }

    // for the last word consider only those bits included in 'other.size()'
    auto last_word_bits = other.size() % bits_required<word_t>();
    auto mask = ~word_t(0);

    // clear trailing bits
    if (last_word_bits) {
      mask = ~(mask << last_word_bits); // unset all bits that are not part of 'other.size()'
    }

    assert(last_word < set_.words() && last_word < other.set_.words());
    *(data + last_word) &= ~(*(other.data() + last_word) & mask);

    return *this;
  }

  bool all() const noexcept { return set_.count() == size(); }
  bool any() const noexcept { return set_.any(); }
  const word_t* begin() const noexcept { return set_.data(); }
  size_t capacity() const noexcept { return set_.capacity(); }
  void clear() noexcept {
    set_.clear();
    size_ = 0;
  }
  word_t count() const noexcept { return set_.count(); }
  const word_t* data() const noexcept { return set_.data(); }
  const word_t* end() const noexcept { return set_.end(); }

  template<typename T>
  void memset(const T& value) noexcept { memset(&value, sizeof(value)); }

  void memset(const void* src, size_t size) noexcept {
    auto bits = bits_required<uint8_t>() * size; // size is in bytes

    reserve(bits);
    std::memcpy(const_cast<word_t*>(begin()), src, std::min(size, set_.words() * sizeof(word_t)));
    size_ = std::max(size_, bits);
  }

  bool none() const noexcept { return set_.none(); }

  // reserve at least this many bits
  void reserve(size_t bits) {
    const auto words = bitset::word(bits - 1) + 1;

    if (!bits || words <= set_.words()) {
      return; // nothing to do
    }

    auto set = bitset(words * bits_required<word_t>());

    if (data()) {
      set.memset(begin(), set_.words() * sizeof(word_t)); // copy original
    }

    set_ = std::move(set);
  }

  void resize(size_t bits, bool preserve_capacity = false) {
    const auto words = bitset::word(bits) + 1; // +1 for count instead of offset

    size_ = bits;

    if (words > set_.words()) {
      reserve(bits);

      return;
    }

    if (preserve_capacity) {
      std::memset(const_cast<word_t*>(begin()) + words, 0, (set_.words() - words) * sizeof(word_t)); // clear trailing words
    } else if (words < set_.words()) {
      auto set = bitset(words * bits_required<word_t>());

      set.memset(begin(), set.words() * sizeof(word_t)); // copy original
      set_ = std::move(set);
    }

    auto last_word_bits = bits % bits_required<word_t>();

    // clear trailing bits
    if (last_word_bits) {
      auto* last_word = begin() + words - 1;
      const auto mask = ~(~word_t(0) << last_word_bits); // set all bits up to and including max bit

      const_cast<word_t&>(*last_word) &= mask; // keep only bits up to max bit
    }
  }

  void reset(size_t i, bool set) {
    if (!set && bitset::word(i) >= set_.words()) {
      size_ = std::max(size(), i + 1);

      return; // nothing to do
    }

    resize(std::max(size(), i + 1), true); // ensure capacity
    set_.reset(i, set);
  }

  void shrink_to_fit() {
    auto words = set_.words();

    if (!words) {
      return; // nothing to do, no buffer
    }

    while (words && 0 == *(begin() + words - 1)) {
      --words;
    }

    if (!words) {
      auto set = bitset(); // create empty set

      set_ = std::move(set);
      size_ = 0;

      return;
    }

    if (words != set_.words()) {
      auto set = bitset(words * bits_required<word_t>());

      set.memset(begin(), set.words() * sizeof(word_t)); // copy original
      set_ = std::move(set);
    }

    size_ = (set_.words() * bits_required<word_t>())
          - math::math_traits<word_t>::clz(*(begin() + set_.words() - 1))
          ;
  }

  void set(size_t i) { reset(i, true); }
  size_t size() const noexcept { return size_; }
  bool empty() const noexcept { return 0 == size_; }

  bool test(size_t i) const noexcept {
    return bitset::word(i) < set_.words() && set_.test(i);
  }

  void unset(size_t i) { reset(i, false); }

  size_t words() const noexcept { return set_.words(); }

  template<typename Visitor>
  bool visit(Visitor visitor) const {
    for (size_t i = 0; i < size_; ++i) {
      if (set_.test(i) && !visitor(i)) {
        return false;
      }
    }
    return true;
  }

 private:
  bitset set_;
  size_t size_{}; // number of bits requested in a bitset
};

static_assert(std::is_nothrow_move_constructible<bitvector>::value,
              "default move constructor expected");

}

#endif
