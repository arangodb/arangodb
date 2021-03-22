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

#ifndef IRESEARCH_BIT_UTILS_H
#define IRESEARCH_BIT_UTILS_H

#include "shared.hpp"
#include <numeric>

namespace iresearch {

template<typename T>
inline constexpr uint32_t bits_required() noexcept {
  return sizeof(T)*8U;
}

template<typename T>
inline constexpr size_t bits_required(size_t n) noexcept {
  return bits_required<T>()*n;
}

template<typename T>
inline constexpr void set_bit(T& value, size_t bit) noexcept {
  value |= (T(1) << bit);
}

template<unsigned Bit, typename T>
inline constexpr void set_bit(T& value) noexcept {
  value |= (T(1) << (Bit));
}

template<typename T>
inline constexpr void unset_bit(T& value, size_t bit) noexcept {
  value &= ~(T(1) << bit);
}

template<unsigned Bit, typename T>
inline constexpr void unset_bit(T& value) noexcept {
  value &= ~(T(1) << (Bit));
}

template<unsigned Bit, typename T>
inline constexpr void set_bit(bool set, T& value) noexcept {
  set ? set_bit< Bit >(value) : unset_bit< Bit >(value);
}

template<typename T>
inline constexpr void set_bit(T& value, size_t bit, bool set) noexcept{
  set ? set_bit(value, bit) : unset_bit(value, bit);
}

template<unsigned Bit, typename T>
inline constexpr void unset_bit(bool unset, T& value) noexcept {
  if (unset) {
    unset_bit<Bit>(value);
  }
}

template<typename T>
inline constexpr void unset_bit(T& value, size_t bit, bool unset) noexcept {
  if (unset) {
    unset_bit(value, bit);
  }
}

template<unsigned Bit, typename T>
inline constexpr bool check_bit(T value) noexcept{
  return (value & (T(1) << (Bit))) != 0;
}

template< typename T>
inline constexpr bool check_bit(T value, size_t bit) noexcept {
  return (value & (T(1) << bit)) != 0;
}

template<unsigned Offset, typename T>
inline constexpr T rol(T value) noexcept{
  static_assert(Offset >= 0 && Offset <= sizeof(T) * 8,
                 "Offset out of range");

  return (value << Offset) | (value >> (sizeof(T) * 8 - Offset));
}

template<unsigned Offset, typename T>
inline constexpr T ror(T value) noexcept{
  static_assert(Offset >= 0 && Offset <= sizeof(T) * 8,
                 "Offset out of range");

  return (value >> Offset) | (value << (sizeof(T) * 8 - Offset));
}

#if defined(_MSC_VER)
  #pragma warning( push )
  #pragma warning(disable : 4146)
#endif

inline constexpr uint32_t zig_zag_encode32(int32_t v) noexcept {
  return (v >> 31) ^ (uint32_t(v) << 1);
}

inline constexpr int32_t zig_zag_decode32(uint32_t v) noexcept {
  return (v >> 1) ^ -(v & 1);
}

inline constexpr uint64_t zig_zag_encode64(int64_t v) noexcept {
  return (v >> 63) ^ (uint64_t(v) << 1);
}

inline constexpr int64_t zig_zag_decode64(uint64_t v) noexcept {
  return (v >> 1) ^ -(v & 1);
}

#if defined(_MSC_VER)
  #pragma warning( pop )
#endif

template<typename T>
struct enum_bitwise_traits {
  typedef typename std::enable_if<
    std::is_enum<T>::value,
    typename std::underlying_type<T>::type
  >::type underlying_type_t;

  static constexpr T Or(T lhs, T rhs) noexcept {
    return static_cast<T>(static_cast<underlying_type_t>(lhs) | static_cast<underlying_type_t>(rhs));
  }

  static constexpr T Xor(T lhs, T rhs) noexcept {
    return static_cast<T>(static_cast<underlying_type_t>(lhs) ^ static_cast<underlying_type_t>(rhs));
  }

  static constexpr T And(T lhs, T rhs) noexcept {
    return static_cast<T>(static_cast<underlying_type_t>(lhs) & static_cast<underlying_type_t>(rhs));
  }

  static constexpr T Not(T v) noexcept {
    return static_cast<T>(~static_cast<underlying_type_t>(v));
  }
}; // enum_bitwise_traits

template<typename T>
inline constexpr T enum_bitwise_or(T lhs, T rhs) noexcept {
  return enum_bitwise_traits<T>::Or(lhs, rhs);
}

template<typename T>
inline constexpr T enum_bitwise_xor(T lhs, T rhs) noexcept {
  return enum_bitwise_traits<T>::Xor(lhs, rhs);
}

template<typename T>
inline constexpr T enum_bitwise_and(T lhs, T rhs) noexcept {
  return enum_bitwise_traits<T>::And(lhs, rhs);
}

template<typename T>
inline constexpr T enum_bitwise_not(T v) noexcept {
  return enum_bitwise_traits<T>::Not(v);
}

}

#define ENABLE_BITMASK_ENUM(x) \
[[maybe_unused]] inline constexpr x operator&(x lhs, x rhs) noexcept { return irs::enum_bitwise_and(lhs, rhs); } \
[[maybe_unused]] inline x& operator&=(x& lhs, x rhs)        noexcept { return lhs = irs::enum_bitwise_and(lhs, rhs); }   \
[[maybe_unused]] inline constexpr x operator|(x lhs, x rhs) noexcept { return irs::enum_bitwise_or(lhs, rhs); }  \
[[maybe_unused]] inline x& operator|=(x& lhs, x rhs)        noexcept { return lhs = irs::enum_bitwise_or(lhs, rhs); } \
[[maybe_unused]] inline constexpr x operator^(x lhs, x rhs) noexcept { return irs::enum_bitwise_xor(lhs, rhs); } \
[[maybe_unused]] inline x& operator^=(x& lhs, x rhs)        noexcept { return lhs = irs::enum_bitwise_xor(lhs, rhs); }   \
[[maybe_unused]] inline constexpr x operator~(x v)          noexcept { return irs::enum_bitwise_not(v); }

#endif
