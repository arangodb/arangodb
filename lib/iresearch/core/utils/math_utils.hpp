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

#ifdef _MSC_VER
#include <intrin.h>

#pragma intrinsic(_BitScanReverse)
#pragma intrinsic(_BitScanForward)
#endif

#include <climits>
#include <cmath>
#include <numeric>

#include "shared.hpp"
#include "utils/assert.hpp"

namespace irs::math {

// Sum two unsigned integral values with overflow check
// Returns false if sum is overflowed, true - otherwise
template<typename T>
std::enable_if_t<std::is_integral_v<T> && std::is_unsigned_v<T>, bool>
sum_check_overflow(T lhs, T rhs, T& sum) noexcept {
  sum = lhs + rhs;
  return sum >= lhs && sum >= rhs;
}

constexpr size_t roundup_power2(size_t v) noexcept {
  v--;
  v |= v >> 1U;
  v |= v >> 2U;
  v |= v >> 4U;
  v |= v >> 8U;
  v |= v >> 16U;
  v++;
  return v;
}

template<typename T>
constexpr bool is_power2(T v) noexcept {  // undefined for 0
  static_assert(std::is_integral_v<T>, "T must be an integral type");
  return !(v & (v - 1));
}

inline bool approx_equals(double_t lhs, double_t rhs) noexcept {
  return std::fabs(rhs - lhs) < std::numeric_limits<double_t>::epsilon();
}

constexpr uint64_t ceil64(double_t v) noexcept {
  return (static_cast<double_t>(static_cast<size_t>(v)) == v)
           ? static_cast<size_t>(v)
           : static_cast<size_t>(v) + ((v > 0) ? 1 : 0);
}

constexpr uint32_t ceil32(float_t v) noexcept {
  return (static_cast<float_t>(static_cast<uint32_t>(v)) == v)
           ? static_cast<uint32_t>(v)
           : static_cast<uint32_t>(v) + ((v > 0) ? 1 : 0);
}

// Rounds the result of division (num/den) to the next greater integer value.
constexpr uint64_t div_ceil64(uint64_t num, uint64_t den) noexcept {
  IRS_ASSERT(den != 0);
  IRS_ASSERT((num + den) >= num);
  return (num + den - 1) / den;
}

// Rounds the result of division (num/den) to the next greater integer value.
constexpr uint32_t div_ceil32(uint32_t num, uint32_t den) noexcept {
  IRS_ASSERT(den != 0);
  IRS_ASSERT((num + den) >= num);
  return (num + den - 1) / den;
}

// Rounds the specified 'value' to the next greater
// value that is multiple of the specified 'step'.
constexpr uint64_t ceil64(uint64_t value, uint64_t step) noexcept {
  return div_ceil64(value, step) * step;
}

// Rounds the specified 'value' to the next greater
// value that is multiple of the specified 'step'.
constexpr uint32_t ceil32(uint32_t value, uint32_t step) noexcept {
  return div_ceil32(value, step) * step;
}

constexpr uint32_t log(uint64_t x, uint64_t base) noexcept {
  uint32_t res = 0;
  while (x >= base) {
    x /= base;
    ++res;
  }
  return res;
}

constexpr uint32_t log2_64(uint64_t value) noexcept { return log(value, 2); }

constexpr uint32_t log2_32(uint32_t value) noexcept { return log(value, 2); }

IRS_FORCE_INLINE uint32_t log2_floor_32(uint32_t v) {
#if __GNUC__ >= 4
  return UINT32_C(31) ^ __builtin_clz(v);
#elif defined(_MSC_VER)
  unsigned long idx;
  _BitScanReverse(&idx, v);
  return idx;
#else
  return log2_32(v);
#endif
}

IRS_FORCE_INLINE uint32_t log2_ceil_32(uint32_t v) {
  return log2_floor_32(v) + static_cast<uint32_t>(!is_power2(v));
}

IRS_FORCE_INLINE uint64_t log2_floor_64(uint64_t v) {
#if __GNUC__ >= 4
  return UINT64_C(63) ^ __builtin_clzll(v);
#elif defined(_MSC_VER)
  unsigned long idx;
  _BitScanReverse64(&idx, v);
  return idx;
#else
  return log2_64(v);
#endif
}

IRS_FORCE_INLINE uint64_t log2_ceil_64(uint64_t v) {
  return log2_floor_64(v) + static_cast<uint64_t>(!is_power2(v));
}

template<typename T, size_t N = sizeof(T)>
struct math_traits {
  static size_t ceil(T value, T step);
  static uint32_t bits_required(T val) noexcept;
};

template<typename Iterator>
constexpr size_t popcount(Iterator begin, Iterator end) noexcept {
  return std::accumulate(begin, end, size_t{0}, [](size_t acc, auto word) {
    return acc + std::popcount(word);
  });
}

template<typename T>
struct math_traits<T, sizeof(uint32_t)> {
  using type = T;

  static size_t div_ceil(type num, type den) noexcept {
    return div_ceil32(num, den);
  }
  static size_t ceil(type value, type step) noexcept {
    return ceil32(value, step);
  }
  static uint32_t bits_required(type val) noexcept {
    return 0 == val ? 0 : 32 - static_cast<uint32_t>(std::countl_zero(val));
  }
};

template<typename T>
struct math_traits<T, sizeof(uint64_t)> {
  using type = T;

  static size_t div_ceil(type num, type den) noexcept {
    return div_ceil64(num, den);
  }
  static size_t ceil(type value, type step) noexcept {
    return ceil64(value, step);
  }
  static uint32_t bits_required(type val) noexcept {
    return 0 == val ? 0 : 64 - static_cast<uint32_t>(std::countl_zero(val));
  }
};

}  // namespace irs::math
