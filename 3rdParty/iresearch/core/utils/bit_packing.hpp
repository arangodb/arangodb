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
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#ifndef IRESEARCH_BIT_PACKING_H
#define IRESEARCH_BIT_PACKING_H

#include "shared.hpp"
#include "math_utils.hpp"

#include <cmath>
#include <limits>
#include <iterator>

NS_ROOT
NS_BEGIN( packed )

const uint32_t BLOCK_SIZE_32 = sizeof(uint32_t) * 8; // block size is tied to number of bits in value
const uint32_t BLOCK_SIZE_64 = sizeof(uint64_t) * 8; // block size is tied to number of bits in value

const uint32_t VERSION = 1U;

inline uint32_t bits_required_64(uint64_t val) {
  return 1 + iresearch::math::log2_64(val);
}

inline uint32_t bits_required_32(uint32_t val) {
  return 1 + iresearch::math::log2_32(val);
}

inline uint32_t bytes_required_32(uint32_t count, uint32_t bits) {
  return static_cast<uint32_t>(
    ceilf(static_cast<float_t>(count) * bits / 8)
  );
}

inline uint64_t bytes_required_64(uint64_t count, uint64_t bits) {
  return static_cast<uint64_t>(
    std::ceil(static_cast<double_t>(count) * bits / 8)
  );
}

inline uint32_t blocks_required_32(uint32_t count, uint32_t bits) {
  return static_cast<uint32_t>(
    ceilf(static_cast<float_t>(count) * bits / (8 * sizeof(uint32_t)))
  );
}

inline uint64_t blocks_required_64(uint64_t count, uint64_t bits) {
  return static_cast<uint64_t>(
    std::ceil(static_cast<double_t>(count) * bits / (8 * sizeof(uint64_t)))
  );
}

//////////////////////////////////////////////////////////////////////////////
/// @brief returns number of elements required to store unpacked data
//////////////////////////////////////////////////////////////////////////////
inline uint64_t items_required( uint32_t count ) {
  return BLOCK_SIZE_32 * static_cast<uint32_t>(std::ceil(double(count) / BLOCK_SIZE_32));
}

inline uint64_t iterations_required( uint32_t count ) {
  return items_required(count) / BLOCK_SIZE_32;
}

template< typename T >
inline T max_value(uint32_t bits) {
  assert( bits >= 0U && bits <= sizeof( T ) * 8U );

  return bits == sizeof( T ) * 8U
    ? (std::numeric_limits<T>::max)() 
    : ~(~T(0) << bits);
}

IRESEARCH_API uint32_t at(
  const uint32_t* encoded, 
  const size_t i, 
  const uint32_t bit) NOEXCEPT;

IRESEARCH_API uint64_t at(
  const uint64_t* encoded, 
  const size_t i, 
  const uint32_t bit) NOEXCEPT;

IRESEARCH_API void pack(
  const uint32_t* first, 
  const uint32_t* last, 
  uint32_t* out, 
  const uint32_t bit) NOEXCEPT;

IRESEARCH_API void pack(
  const uint64_t* first, 
  const uint64_t* last, 
  uint64_t* out, 
  const uint32_t bit) NOEXCEPT;

IRESEARCH_API void unpack(
  uint32_t* first, 
  uint32_t* last, 
  const uint32_t* in, 
  const uint32_t bit) NOEXCEPT;

IRESEARCH_API void unpack(
  uint64_t* first, 
  uint64_t* last, 
  const uint64_t* in, 
  const uint32_t bit) NOEXCEPT;

template<typename T>
class iterator : public std::iterator<std::random_access_iterator_tag, T> {
 public:
  typedef typename std::iterator_traits<iterator>::value_type value_type;
  typedef typename std::iterator_traits<iterator>::difference_type difference_type;
  typedef const value_type* const_pointer;

  iterator(const_pointer packed, uint32_t bits, size_t i = 0)
    : packed_(packed), i_(i), bits_(bits) {
    assert(packed_);
    assert(bits_ > 0 && bits <= sizeof(value_type)*8);
  }

  iterator(const iterator&) = default;
  iterator& operator=(const iterator&) = default;

  iterator& operator++() {
    ++i_;
    return *this; 
  }

  iterator operator++(int) { 
    const auto tmp = *this;
    ++*this;
    return tmp;
  }

  iterator& operator+=(difference_type v) {
    i_ += v;
    return *this;
  }

  iterator operator+(difference_type v) const {
    return iterator(packed_, bits_, i_ + v);
  }

  iterator& operator--() {
    --i_;
    return *this; 
  }
  
  iterator operator--(int) {
    const auto tmp = *this;
    --*this;
    return tmp;
  }
  
  iterator& operator-=(difference_type v) {
    i_ -= v;
    return *this;
  }

  iterator operator-(difference_type v) const {
    return iterator(packed_, bits_, i_ - v);
  }
  
  difference_type operator-(const iterator& rhs) const {
    assert(packed_ == rhs.packed_); // compatibility
    return i_ - rhs.i_;
  }

  value_type operator*() const {
    return at(packed_, i_, bits_);
  }

  bool operator==(const iterator& rhs) const {
    assert(packed_ == rhs.packed_); // compatibility
    return i_ == rhs.i_;
  }

  bool operator!=(const iterator& rhs) const {
    return !(*this == rhs);
  }

  bool operator<(const iterator& rhs) const {
    assert(packed_ == rhs.packed_); // compatibility
    return i_ < rhs.i_;
  }

  bool operator>=(const iterator& rhs) const {
    return !(*this < rhs);
  }
  
  bool operator>(const iterator& rhs) const {
    assert(packed_ == rhs.packed_); // compatibility
    return i_ > rhs.i_;
  }
  
  bool operator<=(const iterator& rhs) const {
    return !(*this > rhs);
  }

 private:
  const_pointer packed_;
  size_t i_;
  uint32_t bits_;
}; // iterator

typedef iterator<uint32_t> iterator32;
typedef iterator<uint64_t> iterator64;

NS_END // packing
NS_END // root

#endif
