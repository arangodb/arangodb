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

#ifndef IRESEARCH_BIT_UTILS_H
#define IRESEARCH_BIT_UTILS_H

#include "shared.hpp"
#include <numeric>

NS_ROOT

template< typename T >
CONSTEXPR uint32_t bits_required() NOEXCEPT {
  return sizeof(T)*8U;
}

template< typename T >
inline void set_bit( T& value, size_t bit ) NOEXCEPT { 
  value |= ( T(1) << bit ); 
}

template< unsigned Bit, typename T >
inline void set_bit( T& value ) NOEXCEPT { 
  value |= ( T(1) << (Bit) ); 
}

template< typename T >
inline void unset_bit( T& value, size_t bit ) NOEXCEPT { 
  value &= ~( T(1) << bit ); 
}

template< unsigned Bit, typename T >
inline void unset_bit( T& value ) NOEXCEPT { 
  value &= ~( T(1) << (Bit) ); 
}

template< unsigned Bit, typename T >
inline void set_bit( bool set, T& value ) NOEXCEPT { 
  set ? set_bit< Bit >( value ) : unset_bit< Bit >( value ); 
}

template< typename T >
inline void set_bit( T& value, size_t bit, bool set ) NOEXCEPT{
  set ? set_bit( value, bit ) : unset_bit( value, bit );
}

template<unsigned Bit, typename T>
inline void unset_bit(bool unset, T& value) NOEXCEPT {
  if (unset) {
    unset_bit<Bit>(value);
  }
}

template<typename T>
inline void unset_bit(T& value, size_t bit, bool unset) NOEXCEPT {
  if (unset) {
    unset_bit(value, bit);
  }
}

template< unsigned Bit, typename T >
inline CONSTEXPR bool check_bit( T value ) NOEXCEPT{
  return ( value & ( T(1) << (Bit) ) ) != 0; 
}

template< typename T >
inline bool check_bit( T value, size_t bit ) NOEXCEPT {
  return ( value & ( T(1) << bit ) ) != 0; 
}

template< unsigned Offset, typename T >
inline CONSTEXPR T rol( T value ) NOEXCEPT{
  static_assert( Offset >= 0 && Offset <= sizeof( T ) * 8,
                 "Offset out of range" );

  return ( value << Offset ) | ( value >> ( sizeof( T ) * 8 - Offset ) );
}

template< unsigned Offset, typename T >
inline CONSTEXPR T ror( T value ) NOEXCEPT{
  static_assert( Offset >= 0 && Offset <= sizeof( T ) * 8,
                 "Offset out of range" );

  return ( value >> Offset ) | ( value << ( sizeof( T ) * 8 - Offset ) );
}

#if defined(_MSC_VER)
  #pragma warning( disable : 4146 )
#endif

inline CONSTEXPR uint32_t zig_zag_encode32(int32_t v) NOEXCEPT {
  return (v >> 31) ^ (uint32_t(v) << 1);
}

inline CONSTEXPR int32_t zig_zag_decode32(uint32_t v) NOEXCEPT {
  return (v >> 1) ^ -(v & 1);
}

inline CONSTEXPR uint64_t zig_zag_encode64(int64_t v) NOEXCEPT {
  return (v >> 63) ^ (uint64_t(v) << 1);
}

inline CONSTEXPR int64_t zig_zag_decode64(uint64_t v) NOEXCEPT {
  return (v >> 1) ^ -(v & 1);
}

#if defined(_MSC_VER)
  #pragma warning( default  : 4146 )
#endif

template<typename T>
struct enum_bitwise_traits {
  typedef typename std::enable_if<
    std::is_enum<T>::value,
    typename std::underlying_type<T>::type
  >::type underlying_type_t;

  CONSTEXPR static T Or(T lhs, T rhs) NOEXCEPT {
    return static_cast<T>(static_cast<underlying_type_t>(lhs) | static_cast<underlying_type_t>(rhs));
  }

  CONSTEXPR static T Xor(T lhs, T rhs) NOEXCEPT {
    return static_cast<T>(static_cast<underlying_type_t>(lhs) ^ static_cast<underlying_type_t>(rhs));
  }

  CONSTEXPR static T And(T lhs, T rhs) NOEXCEPT {
    return static_cast<T>(static_cast<underlying_type_t>(lhs) & static_cast<underlying_type_t>(rhs));
  }

  CONSTEXPR static T Not(T v) NOEXCEPT {
    return static_cast<T>(~static_cast<underlying_type_t>(v));
  }
}; // enum_bitwise_traits

template<typename T>
inline CONSTEXPR T enum_bitwise_or(T lhs, T rhs) NOEXCEPT {
  return enum_bitwise_traits<T>::Or(lhs, rhs);
}

template<typename T>
inline CONSTEXPR T enum_bitwise_xor(T lhs, T rhs) NOEXCEPT {
  return enum_bitwise_traits<T>::Xor(lhs, rhs);
}

template<typename T>
inline CONSTEXPR T enum_bitwise_and(T lhs, T rhs) NOEXCEPT {
  return enum_bitwise_traits<T>::And(lhs, rhs);
}

template<typename T>
inline CONSTEXPR T enum_bitwise_not(T v) NOEXCEPT {
  return enum_bitwise_traits<T>::Not(v);
}

#define ENABLE_BITMASK_ENUM(x) \
inline CONSTEXPR x operator&(x lhs, x rhs) NOEXCEPT { return enum_bitwise_and(lhs, rhs); } \
inline x& operator&=(x& lhs, x rhs)        NOEXCEPT { return lhs = enum_bitwise_and(lhs, rhs); }   \
inline CONSTEXPR x operator|(x lhs, x rhs) NOEXCEPT { return enum_bitwise_or(lhs, rhs); }  \
inline x& operator|=(x& lhs, x rhs)        NOEXCEPT { return lhs = enum_bitwise_or(lhs, rhs); } \
inline CONSTEXPR x operator^(x lhs, x rhs) NOEXCEPT { return enum_bitwise_xor(lhs, rhs); } \
inline x& operator^=(x& lhs, x rhs)        NOEXCEPT { return lhs = enum_bitwise_xor(lhs, rhs); }   \
inline CONSTEXPR x operator~(x v)          NOEXCEPT { return enum_bitwise_not(v); }

NS_END

#endif
