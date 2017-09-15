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

NS_ROOT
NS_BEGIN(math)

inline size_t roundup_power2(size_t v) NOEXCEPT {
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
  CONSTEXPR inline bool is_power2(T v) NOEXCEPT {
    static_assert(
      std::is_integral<T>::value,
      "T must be an integral type"
    );

    return !(v & (v-1));
  }
#endif

// rounds the specified 'value' to the next greater
// value that is multiple to the specified 'step'
inline uint64_t ceil64(uint64_t value, uint64_t step) {
  return uint64_t(std::ceil(double_t(value)/step))*step;
}

// rounds the specified 'value' to the next greater
// value that is multiple to the specified 'step'
inline uint32_t ceil32(uint32_t value, uint32_t step) {
  return uint32_t(std::ceil(float_t(value)/step))*step;
}

IRESEARCH_API uint32_t log2_64(uint64_t value);

IRESEARCH_API uint32_t log2_32(uint32_t value);

IRESEARCH_API uint32_t log(uint64_t x, uint64_t base);

/* returns number of set bits in a set of words */
template<typename T>
FORCE_INLINE size_t popcnt( const T* value, size_t count ) NOEXCEPT {
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
FORCE_INLINE uint64_t popcnt64( uint64_t v ) NOEXCEPT{  
  v = v - ( ( v >> 1 ) & ( uint64_t ) ~( uint64_t ) 0 / 3 );
  v = ( v & ( uint64_t ) ~( uint64_t ) 0 / 15 * 3 ) + ( ( v >> 2 ) & ( uint64_t ) ~( uint64_t ) 0 / 15 * 3 );
  v = ( v + ( v >> 4 ) ) & ( uint64_t ) ~( uint64_t ) 0 / 255 * 15;                      
  return ( ( uint64_t ) ( v * ( ( uint64_t ) ~( uint64_t ) 0 / 255 ) ) >> ( sizeof( uint64_t ) - 1 ) * CHAR_BIT); 
}

/* Hamming weight for 32bit values */
FORCE_INLINE uint32_t popcnt32( uint32_t v ) NOEXCEPT{
  v = v - ( ( v >> 1 ) & 0x55555555 );                    
  v = ( v & 0x33333333 ) + ( ( v >> 2 ) & 0x33333333 );  
  v = (v + (v >> 4)) & 0xF0F0F0F;
  return ((v * 0x1010101) >> 24);
}

FORCE_INLINE uint32_t pop32( uint32_t v ) NOEXCEPT {
#if __GNUC__ >= 4 
  return __builtin_popcount(v);
#elif defined(_MSC_VER) 
  //TODO: compile time
  return cpuinfo::support_popcnt() ?__popcnt(v) : popcnt32(v);
#endif
}

FORCE_INLINE uint64_t pop64(uint64_t v) NOEXCEPT{
#if  __GNUC__ >= 4
  return __builtin_popcountll(v);
#elif defined(_MSC_VER) && defined(_M_X64)
  //TODO: compile time
  return cpuinfo::support_popcnt() ? __popcnt64(v) : popcnt64(v);
#endif
}

FORCE_INLINE uint32_t ctz32(uint32_t v) NOEXCEPT {
  assert(v); // 0 does not supported
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

FORCE_INLINE uint64_t ctz64(uint64_t v) NOEXCEPT {
  assert(v); // 0 does not supported
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

FORCE_INLINE uint32_t clz32(uint32_t v) NOEXCEPT {
  assert(v); // 0 does not supported
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

FORCE_INLINE uint64_t clz64(uint64_t v) NOEXCEPT {
  assert(v); // 0 does not supported
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

template<typename T>
struct math_traits {
  static size_t clz(T value);
  static size_t ctz(T value);
  static size_t pop(T value);
}; // math_traits 

template<>
struct math_traits<uint32_t> {
  typedef uint32_t type;

  static size_t clz(type value) { return clz32(value); }
  static size_t ctz(type value) { return ctz32(value); }
  static size_t pop(type value) { return pop32(value); }
}; // math_traits

template<>
struct math_traits<uint64_t> {
  typedef uint64_t type;

  static size_t clz(type value) { return clz64(value); }
  static size_t ctz(type value) { return ctz64(value); }
  static size_t pop(type value) { return pop64(value); }
}; // math_traits

NS_END // math
NS_END // root

#endif
