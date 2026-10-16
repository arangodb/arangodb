////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 by EMC Corporation, All Rights Reserved
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
/// Copyright holder is EMC Corporation
///
/// @author Andrey Abramov
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstring>
#include <memory>

#include "shared.hpp"
#include "utils/bit_utils.hpp"
#include "utils/math_utils.hpp"
#include "utils/memory.hpp"

namespace irs {

template<typename Alloc>
class dynamic_bitset {
 public:
  using index_t = size_t;
  using word_t = size_t;
  using allocator_type =
    typename std::allocator_traits<Alloc>::template rebind_alloc<word_t>;
  using word_ptr_deleter_t =
    memory::allocator_array_deallocator<allocator_type>;
  using word_ptr_t = std::unique_ptr<word_t[], word_ptr_deleter_t>;

  constexpr IRS_FORCE_INLINE static size_t bits_to_words(size_t bits) noexcept {
    return (bits + bits_required<word_t>() - 1) / bits_required<word_t>();
  }

  // returns corresponding bit index within a word for the
  // specified offset in bits
  constexpr IRS_FORCE_INLINE static size_t bit(size_t i) noexcept {
    return i % bits_required<word_t>();
  }

  // returns corresponding word index specified offset in bits
  constexpr IRS_FORCE_INLINE static size_t word(size_t i) noexcept {
    return i / bits_required<word_t>();
  }

  // returns corresponding offset in bits for the specified word index
  constexpr IRS_FORCE_INLINE static size_t bit_offset(size_t i) noexcept {
    return i * bits_required<word_t>();
  }

  dynamic_bitset(const Alloc& alloc = Alloc{})
    : data_{nullptr, word_ptr_deleter_t{alloc, 0}} {}

  explicit dynamic_bitset(size_t bits, const Alloc& alloc = Alloc{})
    : dynamic_bitset{alloc} {
    reset(bits);
  }

  dynamic_bitset(dynamic_bitset&& rhs) noexcept
    : data_{std::move(rhs.data_)},
      words_{std::exchange(rhs.words_, 0)},
      bits_{std::exchange(rhs.bits_, 0)} {}

  dynamic_bitset& operator=(dynamic_bitset&& rhs) noexcept {
    if (this != &rhs) {
      data_ = std::move(rhs.data_);
      words_ = std::exchange(rhs.words_, 0);
      bits_ = std::exchange(rhs.bits_, 0);
    }

    return *this;
  }

  void reserve(size_t bits) { reserve_words(bits_to_words(bits)); }

  template<bool Reserve = true>
  void resize(size_t bits) noexcept(!Reserve) {
    const auto new_words = bits_to_words(bits);
    if constexpr (Reserve) {
      reserve_words(new_words);
    } else {
      IRS_ASSERT(bits <= capacity());
      IRS_ASSERT(new_words <= capacity_words());
    }
    const auto old_words = words_;
    words_ = new_words;
    clear_offset(old_words);
    bits_ = bits;
    sanitize();
  }

  void reset(size_t bits) {
    const auto new_words = bits_to_words(bits);

    if (new_words > capacity_words()) {
      data_ = memory::allocate_unique<word_t[]>(
        data_.get_deleter().alloc(), new_words, memory::allocate_only);
    }

    words_ = new_words;
    bits_ = bits;
    clear();
  }

  bool operator==(const dynamic_bitset& rhs) const noexcept {
    if (bits_ != rhs.bits_) {
      return false;
    }
    IRS_ASSERT(words_ == rhs.words_);
    return 0 == std::memcmp(data(), rhs.data(), words_ * sizeof(word_t));
  }

  // number of bits in bitset
  size_t size() const noexcept { return bits_; }

  // capacity in bits
  size_t capacity() const noexcept {
    return bits_required<word_t>() * capacity_words();
  }

  size_t words() const noexcept { return words_; }

  const word_t* data() const noexcept { return data_.get(); }

  const word_t* begin() const noexcept { return data(); }
  const word_t* end() const noexcept { return data() + words_; }

  word_t operator[](size_t i) const noexcept {
    IRS_ASSERT(i < words_);
    return data_[i];
  }

  template<typename T>
  void memset(const T& value) noexcept {
    memset(&value, sizeof(value));
  }

  void memset(const void* src, size_t byte_size) noexcept {
    byte_size = std::min(byte_size, words_ * sizeof(word_t));
    if (byte_size != 0) {
      // passing nullptr to `std::memcpy` is undefined behavior
      IRS_ASSERT(data_ != nullptr);
      IRS_ASSERT(src != nullptr);
      std::memcpy(data_.get(), src, byte_size);
      sanitize();
    }
  }

  void set(size_t i) noexcept {
    IRS_ASSERT(i < bits_);
    set_bit(data_[word(i)], bit(i));
  }

  void unset(size_t i) noexcept {
    IRS_ASSERT(i < bits_);
    unset_bit(data_[word(i)], bit(i));
  }

  void reset(size_t i, bool set) noexcept {
    IRS_ASSERT(i < bits_);
    set_bit(data_[word(i)], bit(i), set);
  }

  bool test(size_t i) const noexcept {
    IRS_ASSERT(i < bits_);
    return check_bit(data_[word(i)], bit(i));
  }

  bool try_set(size_t i) noexcept {
    IRS_ASSERT(i < bits_);
    auto& data = data_[word(i)];
    const auto mask = word_t{1} << bit(i);
    if ((data & mask) != 0) {
      return false;
    }
    data |= mask;
    return true;
  }

  bool any() const noexcept {
    return std::any_of(begin(), end(), [](word_t w) { return w != 0; });
  }

  bool none() const noexcept { return !any(); }

  bool all() const noexcept {
    if (words_ == 0) {
      return true;
    }
    auto* begin = data_.get();
    for (auto* end = begin + words_ - 1; begin != end; ++begin) {
      static_assert(std::is_unsigned_v<word_t>);
      if (*begin != std::numeric_limits<word_t>::max()) {
        return false;
      }
    }
    return std::popcount(*begin) ==
           bits_ - (words_ - 1) * bits_required<word_t>();
  }

  void clear() noexcept { clear_offset(0); }

  // counts bits set
  size_t count() const noexcept { return math::popcount(begin(), end()); }

  Alloc& get_allocator() { return data_.get_deleter().alloc(); }

 private:
  size_t capacity_words() const noexcept { return data_.get_deleter().size(); }

  void reserve_words(size_t words) {
    if (words > capacity_words()) {
      auto new_data = memory::allocate_unique<word_t[]>(
        data_.get_deleter().alloc(), words, memory::allocate_only);
      IRS_ASSERT(new_data != nullptr);
      if (words_ != 0) {
        IRS_ASSERT(data() != nullptr);
        std::memcpy(new_data.get(), data(), words_ * sizeof(word_t));
      }
      data_ = std::move(new_data);
    }
  }

  void clear_offset(size_t offset_words) noexcept {
    if (offset_words < words_) {
      IRS_ASSERT(data_ != nullptr);
      std::memset(data_.get() + offset_words, 0,
                  (words_ - offset_words) * sizeof(word_t));
    }
  }

  void sanitize() noexcept {
    IRS_ASSERT(bits_ <= capacity());
    IRS_ASSERT(words_ <= capacity_words());
    const auto last_word_bits = bit(bits_);

    if (last_word_bits == 0) {
      return;  // no words or last word has all bits set
    }

    const auto mask = ~(~word_t{0} << last_word_bits);

    data_[words_ - 1] &= mask;
  }

  word_ptr_t data_;  // words array: pointer, allocator, capacity in words
  size_t words_{0};  // size of bitset in words
  size_t bits_{0};   // size of bitset in bits
};

// TODO: move to tests?
using bitset = dynamic_bitset<std::allocator<size_t>>;
using ManagedBitset = dynamic_bitset<ManagedTypedAllocator<size_t>>;

}  // namespace irs
