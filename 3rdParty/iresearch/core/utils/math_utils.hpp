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

#ifndef IRESEARCH_MATH_UTILS_H
#define IRESEARCH_MATH_UTILS_H

#ifdef _MSC_VER
  #include <intrin.h>

  #pragma intrinsic(_BitScanReverse)
  #pragma intrinsic(_BitScanForward)
#endif

#include "shared.hpp"

#include "cpuinfo.hpp"

#include <numeric>
#include <cassert>
#include <cmath>

namespace iresearch {
namespace math {

/// @brief sum two unsigned integral values with overflow check
/// @returns false if sum is overflowed, true - otherwise
template<
  typename T,
  typename = typename std::enable_if<
    std::is_integral<T>::value && std::is_unsigned<T>::value
  >::type
> inline bool sum_check_overflow(T lhs, T rhs, T& sum) noexcept {
  sum = lhs + rhs;
  return sum >= lhs && sum >= rhs;
}

inline constexpr size_t roundup_power2(size_t v) noexcept {
  v--;
  v |= v >> 1;
  v |= v >> 2;
  v |= v >> 4;
  v |= v >> 8;
  v |= v >> 16;
  v++;
  return v;
}

#if defined(_MSC_VER) && (_MSC_VER < 1900)
  #define is_power2(v) (std::is_integral<decltype(v)>::value && !(v & (v-1)))
#else
  // undefined for 0
  template<typename T>
  constexpr inline bool is_power2(T v) noexcept {
    static_assert(
      std::is_integral<T>::value,
      "T must be an integral type"
    );

    return !(v & (v-1));
  }
#endif

inline bool approx_equals(double_t lhs, double_t rhs) noexcept {
  return std::fabs(rhs - lhs) < std::numeric_limits<double_t>::epsilon();
}

/// @brief rounds the result of division (num/den) to
///        the next greater integer value
constexpr inline uint64_t div_ceil64(uint64_t num, uint64_t den) noexcept {
  // ensure no overflow
  return IRS_ASSERT(den != 0 && (num + den) >= num && (num + den >= den)),
         (num + den - 1)/den;
}

/// @brief rounds the result of division (num/den) to
///        the next greater integer value
constexpr inline uint32_t div_ceil32(uint32_t num, uint32_t den) noexcept {
  // ensure no overflow
  return IRS_ASSERT(den != 0 && (num + den) >= num && (num + den >= den)),
         (num + den - 1)/den;
}

/// @brief rounds the specified 'value' to the next greater
/// value that is multiple of the specified 'step'
constexpr inline uint64_t ceil64(uint64_t value, uint64_t step) noexcept {
  return div_ceil64(value,step)*step;
}

/// @brief rounds the specified 'value' to the next greater
/// value that is multiple of the specified 'step'
constexpr inline uint32_t ceil32(uint32_t value, uint32_t step) noexcept {
  return div_ceil32(value, step)*step;
}

IRESEARCH_API uint32_t log2_64(uint64_t value);

IRESEARCH_API uint32_t log2_32(uint32_t value);

IRESEARCH_API uint32_t log(uint64_t x, uint64_t base);

/* returns number of set bits in a set of words */
template<typename T>
FORCE_INLINE size_t popcnt( const T* value, size_t count ) noexcept {
  const char *const bitsperbyte =
  "\0\1\1\2\1\2\2\3\1\2\2\3\2\3\3\4"
  "\1\2\2\3\2\3\3\4\2\3\3\4\3\4\4\5"
  "\1\2\2\3\2\3\3\4\2\3\3\4\3\4\4\5"
  "\2\3\3\4\3\4\4\5\3\4\4\5\4\5\5\6"
  "\1\2\2\3\2\3\3\4\2\3\3\4\3\4\4\5"
  "\2\3\3\4\3\4\4\5\3\4\4\5\4\5\5\6"
  "\2\3\3\4\3\4\4\5\3\4\4\5\4\5\5\6"
  "\3\4\4\5\4\5\5\6\4\5\5\6\5\6\6\7"
  "\1\2\2\3\2\3\3\4\2\3\3\4\3\4\4\5"
  "\2\3\3\4\3\4\4\5\3\4\4\5\4\5\5\6"
  "\2\3\3\4\3\4\4\5\3\4\4\5\4\5\5\6"
  "\3\4\4\5\4\5\5\6\4\5\5\6\5\6\6\7"
  "\2\3\3\4\3\4\4\5\3\4\4\5\4\5\5\6"
  "\3\4\4\5\4\5\5\6\4\5\5\6\5\6\6\7"
  "\3\4\4\5\4\5\5\6\4\5\5\6\5\6\6\7"
  "\4\5\5\6\5\6\6\7\5\6\6\7\6\7\7\x8";
  const unsigned char* begin = reinterpret_cast< const unsigned char* >( value );
  const unsigned char* end = begin + count;

  return std::accumulate(
    begin, end, size_t( 0 ),
    [bitsperbyte] ( size_t v, unsigned char c ) {
      return v + bitsperbyte[c];
  } );
}

/* Hamming weight for 64bit values */
FORCE_INLINE uint64_t popcnt64( uint64_t v ) noexcept{  
  v = v - ( ( v >> 1 ) & ( uint64_t ) ~( uint64_t ) 0 / 3 );
  v = ( v & ( uint64_t ) ~( uint64_t ) 0 / 15 * 3 ) + ( ( v >> 2 ) & ( uint64_t ) ~( uint64_t ) 0 / 15 * 3 );
  v = ( v + ( v >> 4 ) ) & ( uint64_t ) ~( uint64_t ) 0 / 255 * 15;                      
  return ( ( uint64_t ) ( v * ( ( uint64_t ) ~( uint64_t ) 0 / 255 ) ) >> ( sizeof( uint64_t ) - 1 ) * CHAR_BIT); 
}

/* Hamming weight for 32bit values */
FORCE_INLINE uint32_t popcnt32( uint32_t v ) noexcept{
  v = v - ( ( v >> 1 ) & 0x55555555 );                    
  v = ( v & 0x33333333 ) + ( ( v >> 2 ) & 0x33333333 );  
  v = (v + (v >> 4)) & 0xF0F0F0F;
  return ((v * 0x1010101) >> 24);
}

FORCE_INLINE uint32_t pop32( uint32_t v ) noexcept {
#if __GNUC__ >= 4 
  return __builtin_popcount(v);
#elif defined(_MSC_VER) 
  //TODO: compile time
  return cpuinfo::support_popcnt() ?__popcnt(v) : popcnt32(v);
#endif
}

FORCE_INLINE uint64_t pop64(uint64_t v) noexcept{
#if  __GNUC__ >= 4
  return __builtin_popcountll(v);
#elif defined(_MSC_VER) && defined(_M_X64)
  //TODO: compile time
  return cpuinfo::support_popcnt() ? __popcnt64(v) : popcnt64(v);
#endif
}

FORCE_INLINE uint32_t ctz32(uint32_t v) noexcept {
  assert(v); // 0 is not supported
#if  __GNUC__ >= 4
  return __builtin_ffs(v) - 1; // __builit_ffs returns `index`+1
#elif defined(_MSC_VER)
  unsigned long idx;
  _BitScanForward(&idx, v);
  return idx;
#else
  static_assert(false, "Not supported");
#endif
}

FORCE_INLINE uint64_t ctz64(uint64_t v) noexcept {
  assert(v); // 0 is not supported
#if  __GNUC__ >= 4
  return __builtin_ffsll(v) - 1; // __builit_ffsll returns `index`+1
#elif defined(_MSC_VER)
  unsigned long idx;
  _BitScanForward64(&idx, v);
  return idx;
#else
  static_assert(false, "Not supported");
#endif
}

FORCE_INLINE uint32_t clz32(uint32_t v) noexcept {
  assert(v); // 0 is not supported
#if  __GNUC__ >= 4
  return __builtin_clz(v);
#elif defined(_MSC_VER)
  unsigned long idx;
  _BitScanReverse(&idx, v);
  return 31 - idx;
#else
  static_assert(false, "Not supported");
#endif
}

FORCE_INLINE uint64_t clz64(uint64_t v) noexcept {
  assert(v); // 0 is not supported
#if  __GNUC__ >= 4
  return __builtin_clzll(v);
#elif defined(_MSC_VER)
  unsigned long idx;
  _BitScanReverse64(&idx, v);
  return 63 - idx;
#else
  static_assert(false, "Not supported");
#endif
}

FORCE_INLINE uint32_t log2_floor_32(uint32_t v) {
#if __GNUC__ >= 4
  return 31 ^ __builtin_clz(v);
#elif defined(_MSC_VER)
  unsigned long idx;
  _BitScanReverse(&idx, v);
  return idx;
#else
  return log2_32(v);
#endif
}

FORCE_INLINE uint32_t log2_ceil_32(uint32_t v) {
  static const uint32_t CEIL_EXTRA[] = { 1, 0 };
  return log2_floor_32(v) + CEIL_EXTRA[is_power2(v)];
}

FORCE_INLINE uint64_t log2_floor_64(uint64_t v) {
#if __GNUC__ >= 4
  return 63 ^ __builtin_clzll(v);
#elif defined(_MSC_VER)
  unsigned long idx;
  _BitScanReverse64(&idx, v);
  return idx;
#else
  return log2_64(v);
#endif
}

FORCE_INLINE uint64_t log2_ceil_64(uint64_t v) {
  static const uint64_t CEIL_EXTRA[] = { UINT64_C(1), UINT64_C(0) };
  return log2_floor_64(v) + CEIL_EXTRA[is_power2(v)];
}

template<typename T, size_t N = sizeof(T)>
struct math_traits {
  static size_t clz(T value);
  static size_t ctz(T value);
  static size_t pop(T value);
  static size_t ceil(T value, T step);
}; // math_traits 

template<typename T>
struct math_traits<T, sizeof(uint32_t)> {
  typedef T type;

  static size_t clz(type value) { return clz32(value); }
  static size_t ctz(type value) { return ctz32(value); }
  static size_t pop(type value) { return pop32(value); }
  static size_t div_ceil(type num, type den) { return div_ceil32(num, den); }
  static size_t ceil(type value, type step) { return ceil32(value, step); }
}; // math_traits

template<typename T>
struct math_traits<T, sizeof(uint64_t)> {
  typedef T type;

  static size_t clz(type value) { return clz64(value); }
  static size_t ctz(type value) { return ctz64(value); }
  static size_t pop(type value) { return pop64(value); }
  static size_t div_ceil(type num, type den) { return div_ceil64(num, den); }
  static size_t ceil(type value, type step) { return ceil64(value, step); }
}; // math_traits

//// MacOS size_t is a different type from any of the above
//#if defined(__APPLE__)
//  template<>
//  struct math_traits<size_t> {
//    typedef size_t type;
//
//    static size_t clz(type value) { return clz64(value); }
//    static size_t ctz(type value) { return ctz64(value); }
//    static size_t pop(type value) { return pop64(value); }
//  };
//#endif

template<
  typename Input, 
  typename Output, 
  Input Size,
  typename = typename std::enable_if<std::is_integral<Input>::value>::type
> class sqrt {
 public:
  typedef Input input_type;
  typedef Output output_type;

  sqrt() noexcept {
    for (input_type i = 0, size = Size; i < size; ++i) {
      table_[i] = std::sqrt(static_cast<output_type>(i));
    }
  }

  FORCE_INLINE output_type operator()(input_type value) const noexcept {
    static_assert(
      std::is_same<decltype(std::sqrt(static_cast<output_type>(value))), output_type>::value,
      "invalid overload"
    );

    return value < Size
      ? table_[value]
      : std::sqrt(static_cast<output_type>(value));
  }

 private:
  output_type table_[Size];
}; // sqrt

} // math
} // root

#endif
