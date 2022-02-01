// Copyright 2019 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// 128-bit ARM64 NEON vectors and operations.
// External include guard in highway.h - see comment there.

#include <arm_neon.h>
#include <stddef.h>
#include <stdint.h>

#include "hwy/base.h"
#include "hwy/ops/shared-inl.h"

HWY_BEFORE_NAMESPACE();
namespace hwy {
namespace HWY_NAMESPACE {

template <typename T>
using Full128 = Simd<T, 16 / sizeof(T)>;

namespace detail {  // for code folding and Raw128

// Macros used to define single and double function calls for multiple types
// for full and half vectors. These macros are undefined at the end of the file.

// HWY_NEON_BUILD_TPL_* is the template<...> prefix to the function.
#define HWY_NEON_BUILD_TPL_1
#define HWY_NEON_BUILD_TPL_2
#define HWY_NEON_BUILD_TPL_3

// HWY_NEON_BUILD_RET_* is return type.
#define HWY_NEON_BUILD_RET_1(type, size) Vec128<type, size>
#define HWY_NEON_BUILD_RET_2(type, size) Vec128<type, size>
#define HWY_NEON_BUILD_RET_3(type, size) Vec128<type, size>

// HWY_NEON_BUILD_PARAM_* is the list of parameters the function receives.
#define HWY_NEON_BUILD_PARAM_1(type, size) const Vec128<type, size> a
#define HWY_NEON_BUILD_PARAM_2(type, size) \
  const Vec128<type, size> a, const Vec128<type, size> b
#define HWY_NEON_BUILD_PARAM_3(type, size)                \
  const Vec128<type, size> a, const Vec128<type, size> b, \
      const Vec128<type, size> c

// HWY_NEON_BUILD_ARG_* is the list of arguments passed to the underlying
// function.
#define HWY_NEON_BUILD_ARG_1 a.raw
#define HWY_NEON_BUILD_ARG_2 a.raw, b.raw
#define HWY_NEON_BUILD_ARG_3 a.raw, b.raw, c.raw

// We use HWY_NEON_EVAL(func, ...) to delay the evaluation of func until after
// the __VA_ARGS__ have been expanded. This allows "func" to be a macro on
// itself like with some of the library "functions" such as vshlq_u8. For
// example, HWY_NEON_EVAL(vshlq_u8, MY_PARAMS) where MY_PARAMS is defined as
// "a, b" (without the quotes) will end up expanding "vshlq_u8(a, b)" if needed.
// Directly writing vshlq_u8(MY_PARAMS) would fail since vshlq_u8() macro
// expects two arguments.
#define HWY_NEON_EVAL(func, ...) func(__VA_ARGS__)

// Main macro definition that defines a single function for the given type and
// size of vector, using the underlying (prefix##infix##suffix) function and
// the template, return type, parameters and arguments defined by the "args"
// parameters passed here (see HWY_NEON_BUILD_* macros defined before).
#define HWY_NEON_DEF_FUNCTION(type, size, name, prefix, infix, suffix, args) \
  HWY_CONCAT(HWY_NEON_BUILD_TPL_, args)                                      \
  HWY_API HWY_CONCAT(HWY_NEON_BUILD_RET_, args)(type, size)                  \
      name(HWY_CONCAT(HWY_NEON_BUILD_PARAM_, args)(type, size)) {            \
    return HWY_CONCAT(HWY_NEON_BUILD_RET_, args)(type, size)(                \
        HWY_NEON_EVAL(prefix##infix##suffix, HWY_NEON_BUILD_ARG_##args));    \
  }

// The HWY_NEON_DEF_FUNCTION_* macros define all the variants of a function
// called "name" using the set of neon functions starting with the given
// "prefix" for all the variants of certain types, as specified next to each
// macro. For example, the prefix "vsub" can be used to define the operator-
// using args=2.

// uint8_t
#define HWY_NEON_DEF_FUNCTION_UINT_8(name, prefix, infix, args)        \
  HWY_NEON_DEF_FUNCTION(uint8_t, 16, name, prefix##q, infix, u8, args) \
  HWY_NEON_DEF_FUNCTION(uint8_t, 8, name, prefix, infix, u8, args)     \
  HWY_NEON_DEF_FUNCTION(uint8_t, 4, name, prefix, infix, u8, args)     \
  HWY_NEON_DEF_FUNCTION(uint8_t, 2, name, prefix, infix, u8, args)     \
  HWY_NEON_DEF_FUNCTION(uint8_t, 1, name, prefix, infix, u8, args)

// int8_t
#define HWY_NEON_DEF_FUNCTION_INT_8(name, prefix, infix, args)        \
  HWY_NEON_DEF_FUNCTION(int8_t, 16, name, prefix##q, infix, s8, args) \
  HWY_NEON_DEF_FUNCTION(int8_t, 8, name, prefix, infix, s8, args)     \
  HWY_NEON_DEF_FUNCTION(int8_t, 4, name, prefix, infix, s8, args)     \
  HWY_NEON_DEF_FUNCTION(int8_t, 2, name, prefix, infix, s8, args)     \
  HWY_NEON_DEF_FUNCTION(int8_t, 1, name, prefix, infix, s8, args)

// uint16_t
#define HWY_NEON_DEF_FUNCTION_UINT_16(name, prefix, infix, args)        \
  HWY_NEON_DEF_FUNCTION(uint16_t, 8, name, prefix##q, infix, u16, args) \
  HWY_NEON_DEF_FUNCTION(uint16_t, 4, name, prefix, infix, u16, args)    \
  HWY_NEON_DEF_FUNCTION(uint16_t, 2, name, prefix, infix, u16, args)    \
  HWY_NEON_DEF_FUNCTION(uint16_t, 1, name, prefix, infix, u16, args)

// int16_t
#define HWY_NEON_DEF_FUNCTION_INT_16(name, prefix, infix, args)        \
  HWY_NEON_DEF_FUNCTION(int16_t, 8, name, prefix##q, infix, s16, args) \
  HWY_NEON_DEF_FUNCTION(int16_t, 4, name, prefix, infix, s16, args)    \
  HWY_NEON_DEF_FUNCTION(int16_t, 2, name, prefix, infix, s16, args)    \
  HWY_NEON_DEF_FUNCTION(int16_t, 1, name, prefix, infix, s16, args)

// uint32_t
#define HWY_NEON_DEF_FUNCTION_UINT_32(name, prefix, infix, args)        \
  HWY_NEON_DEF_FUNCTION(uint32_t, 4, name, prefix##q, infix, u32, args) \
  HWY_NEON_DEF_FUNCTION(uint32_t, 2, name, prefix, infix, u32, args)    \
  HWY_NEON_DEF_FUNCTION(uint32_t, 1, name, prefix, infix, u32, args)

// int32_t
#define HWY_NEON_DEF_FUNCTION_INT_32(name, prefix, infix, args)        \
  HWY_NEON_DEF_FUNCTION(int32_t, 4, name, prefix##q, infix, s32, args) \
  HWY_NEON_DEF_FUNCTION(int32_t, 2, name, prefix, infix, s32, args)    \
  HWY_NEON_DEF_FUNCTION(int32_t, 1, name, prefix, infix, s32, args)

// uint64_t
#define HWY_NEON_DEF_FUNCTION_UINT_64(name, prefix, infix, args)        \
  HWY_NEON_DEF_FUNCTION(uint64_t, 2, name, prefix##q, infix, u64, args) \
  HWY_NEON_DEF_FUNCTION(uint64_t, 1, name, prefix, infix, u64, args)

// int64_t
#define HWY_NEON_DEF_FUNCTION_INT_64(name, prefix, infix, args)        \
  HWY_NEON_DEF_FUNCTION(int64_t, 2, name, prefix##q, infix, s64, args) \
  HWY_NEON_DEF_FUNCTION(int64_t, 1, name, prefix, infix, s64, args)

// float and double
#if HWY_ARCH_ARM_A64
#define HWY_NEON_DEF_FUNCTION_ALL_FLOATS(name, prefix, infix, args)   \
  HWY_NEON_DEF_FUNCTION(float, 4, name, prefix##q, infix, f32, args)  \
  HWY_NEON_DEF_FUNCTION(float, 2, name, prefix, infix, f32, args)     \
  HWY_NEON_DEF_FUNCTION(float, 1, name, prefix, infix, f32, args)     \
  HWY_NEON_DEF_FUNCTION(double, 2, name, prefix##q, infix, f64, args) \
  HWY_NEON_DEF_FUNCTION(double, 1, name, prefix, infix, f64, args)
#else
#define HWY_NEON_DEF_FUNCTION_ALL_FLOATS(name, prefix, infix, args)  \
  HWY_NEON_DEF_FUNCTION(float, 4, name, prefix##q, infix, f32, args) \
  HWY_NEON_DEF_FUNCTION(float, 2, name, prefix, infix, f32, args)    \
  HWY_NEON_DEF_FUNCTION(float, 1, name, prefix, infix, f32, args)
#endif

// Helper macros to define for more than one type.
// uint8_t, uint16_t and uint32_t
#define HWY_NEON_DEF_FUNCTION_UINT_8_16_32(name, prefix, infix, args) \
  HWY_NEON_DEF_FUNCTION_UINT_8(name, prefix, infix, args)             \
  HWY_NEON_DEF_FUNCTION_UINT_16(name, prefix, infix, args)            \
  HWY_NEON_DEF_FUNCTION_UINT_32(name, prefix, infix, args)

// int8_t, int16_t and int32_t
#define HWY_NEON_DEF_FUNCTION_INT_8_16_32(name, prefix, infix, args) \
  HWY_NEON_DEF_FUNCTION_INT_8(name, prefix, infix, args)             \
  HWY_NEON_DEF_FUNCTION_INT_16(name, prefix, infix, args)            \
  HWY_NEON_DEF_FUNCTION_INT_32(name, prefix, infix, args)

// uint8_t, uint16_t, uint32_t and uint64_t
#define HWY_NEON_DEF_FUNCTION_UINTS(name, prefix, infix, args)  \
  HWY_NEON_DEF_FUNCTION_UINT_8_16_32(name, prefix, infix, args) \
  HWY_NEON_DEF_FUNCTION_UINT_64(name, prefix, infix, args)

// int8_t, int16_t, int32_t and int64_t
#define HWY_NEON_DEF_FUNCTION_INTS(name, prefix, infix, args)  \
  HWY_NEON_DEF_FUNCTION_INT_8_16_32(name, prefix, infix, args) \
  HWY_NEON_DEF_FUNCTION_INT_64(name, prefix, infix, args)

// All int*_t and uint*_t up to 64
#define HWY_NEON_DEF_FUNCTION_INTS_UINTS(name, prefix, infix, args) \
  HWY_NEON_DEF_FUNCTION_INTS(name, prefix, infix, args)             \
  HWY_NEON_DEF_FUNCTION_UINTS(name, prefix, infix, args)

// All previous types.
#define HWY_NEON_DEF_FUNCTION_ALL_TYPES(name, prefix, infix, args) \
  HWY_NEON_DEF_FUNCTION_INTS_UINTS(name, prefix, infix, args)      \
  HWY_NEON_DEF_FUNCTION_ALL_FLOATS(name, prefix, infix, args)

// Emulation of some intrinsics on armv7.
#if HWY_ARCH_ARM_V7
#define vuzp1_s8(x, y) vuzp_s8(x, y).val[0]
#define vuzp1_u8(x, y) vuzp_u8(x, y).val[0]
#define vuzp1_s16(x, y) vuzp_s16(x, y).val[0]
#define vuzp1_u16(x, y) vuzp_u16(x, y).val[0]
#define vuzp1_s32(x, y) vuzp_s32(x, y).val[0]
#define vuzp1_u32(x, y) vuzp_u32(x, y).val[0]
#define vuzp1_f32(x, y) vuzp_f32(x, y).val[0]
#define vuzp1q_s8(x, y) vuzpq_s8(x, y).val[0]
#define vuzp1q_u8(x, y) vuzpq_u8(x, y).val[0]
#define vuzp1q_s16(x, y) vuzpq_s16(x, y).val[0]
#define vuzp1q_u16(x, y) vuzpq_u16(x, y).val[0]
#define vuzp1q_s32(x, y) vuzpq_s32(x, y).val[0]
#define vuzp1q_u32(x, y) vuzpq_u32(x, y).val[0]
#define vuzp1q_f32(x, y) vuzpq_f32(x, y).val[0]
#define vuzp2_s8(x, y) vuzp_s8(x, y).val[1]
#define vuzp2_u8(x, y) vuzp_u8(x, y).val[1]
#define vuzp2_s16(x, y) vuzp_s16(x, y).val[1]
#define vuzp2_u16(x, y) vuzp_u16(x, y).val[1]
#define vuzp2_s32(x, y) vuzp_s32(x, y).val[1]
#define vuzp2_u32(x, y) vuzp_u32(x, y).val[1]
#define vuzp2_f32(x, y) vuzp_f32(x, y).val[1]
#define vuzp2q_s8(x, y) vuzpq_s8(x, y).val[1]
#define vuzp2q_u8(x, y) vuzpq_u8(x, y).val[1]
#define vuzp2q_s16(x, y) vuzpq_s16(x, y).val[1]
#define vuzp2q_u16(x, y) vuzpq_u16(x, y).val[1]
#define vuzp2q_s32(x, y) vuzpq_s32(x, y).val[1]
#define vuzp2q_u32(x, y) vuzpq_u32(x, y).val[1]
#define vuzp2q_f32(x, y) vuzpq_f32(x, y).val[1]
#define vzip1_s8(x, y) vzip_s8(x, y).val[0]
#define vzip1_u8(x, y) vzip_u8(x, y).val[0]
#define vzip1_s16(x, y) vzip_s16(x, y).val[0]
#define vzip1_u16(x, y) vzip_u16(x, y).val[0]
#define vzip1_f32(x, y) vzip_f32(x, y).val[0]
#define vzip1_u32(x, y) vzip_u32(x, y).val[0]
#define vzip1_s32(x, y) vzip_s32(x, y).val[0]
#define vzip1q_s8(x, y) vzipq_s8(x, y).val[0]
#define vzip1q_u8(x, y) vzipq_u8(x, y).val[0]
#define vzip1q_s16(x, y) vzipq_s16(x, y).val[0]
#define vzip1q_u16(x, y) vzipq_u16(x, y).val[0]
#define vzip1q_s32(x, y) vzipq_s32(x, y).val[0]
#define vzip1q_u32(x, y) vzipq_u32(x, y).val[0]
#define vzip1q_f32(x, y) vzipq_f32(x, y).val[0]
#define vzip2_s8(x, y) vzip_s8(x, y).val[1]
#define vzip2_u8(x, y) vzip_u8(x, y).val[1]
#define vzip2_s16(x, y) vzip_s16(x, y).val[1]
#define vzip2_u16(x, y) vzip_u16(x, y).val[1]
#define vzip2_s32(x, y) vzip_s32(x, y).val[1]
#define vzip2_u32(x, y) vzip_u32(x, y).val[1]
#define vzip2_f32(x, y) vzip_f32(x, y).val[1]
#define vzip2q_s8(x, y) vzipq_s8(x, y).val[1]
#define vzip2q_u8(x, y) vzipq_u8(x, y).val[1]
#define vzip2q_s16(x, y) vzipq_s16(x, y).val[1]
#define vzip2q_u16(x, y) vzipq_u16(x, y).val[1]
#define vzip2q_s32(x, y) vzipq_s32(x, y).val[1]
#define vzip2q_u32(x, y) vzipq_u32(x, y).val[1]
#define vzip2q_f32(x, y) vzipq_f32(x, y).val[1]
#endif

template <typename T, size_t N>
struct Raw128;

// 128
template <>
struct Raw128<uint8_t, 16> {
  using type = uint8x16_t;
};

template <>
struct Raw128<uint16_t, 8> {
  using type = uint16x8_t;
};

template <>
struct Raw128<uint32_t, 4> {
  using type = uint32x4_t;
};

template <>
struct Raw128<uint64_t, 2> {
  using type = uint64x2_t;
};

template <>
struct Raw128<int8_t, 16> {
  using type = int8x16_t;
};

template <>
struct Raw128<int16_t, 8> {
  using type = int16x8_t;
};

template <>
struct Raw128<int32_t, 4> {
  using type = int32x4_t;
};

template <>
struct Raw128<int64_t, 2> {
  using type = int64x2_t;
};

template <>
struct Raw128<float16_t, 8> {
  using type = uint16x8_t;
};

template <>
struct Raw128<float, 4> {
  using type = float32x4_t;
};

#if HWY_ARCH_ARM_A64
template <>
struct Raw128<double, 2> {
  using type = float64x2_t;
};
#endif

// 64
template <>
struct Raw128<uint8_t, 8> {
  using type = uint8x8_t;
};

template <>
struct Raw128<uint16_t, 4> {
  using type = uint16x4_t;
};

template <>
struct Raw128<uint32_t, 2> {
  using type = uint32x2_t;
};

template <>
struct Raw128<uint64_t, 1> {
  using type = uint64x1_t;
};

template <>
struct Raw128<int8_t, 8> {
  using type = int8x8_t;
};

template <>
struct Raw128<int16_t, 4> {
  using type = int16x4_t;
};

template <>
struct Raw128<int32_t, 2> {
  using type = int32x2_t;
};

template <>
struct Raw128<int64_t, 1> {
  using type = int64x1_t;
};

template <>
struct Raw128<float16_t, 4> {
  using type = uint16x4_t;
};

template <>
struct Raw128<float, 2> {
  using type = float32x2_t;
};

#if HWY_ARCH_ARM_A64
template <>
struct Raw128<double, 1> {
  using type = float64x1_t;
};
#endif

// 32 (same as 64)
template <>
struct Raw128<uint8_t, 4> {
  using type = uint8x8_t;
};

template <>
struct Raw128<uint16_t, 2> {
  using type = uint16x4_t;
};

template <>
struct Raw128<uint32_t, 1> {
  using type = uint32x2_t;
};

template <>
struct Raw128<int8_t, 4> {
  using type = int8x8_t;
};

template <>
struct Raw128<int16_t, 2> {
  using type = int16x4_t;
};

template <>
struct Raw128<int32_t, 1> {
  using type = int32x2_t;
};

template <>
struct Raw128<float16_t, 2> {
  using type = uint16x4_t;
};

template <>
struct Raw128<float, 1> {
  using type = float32x2_t;
};

// 16 (same as 64)
template <>
struct Raw128<uint8_t, 2> {
  using type = uint8x8_t;
};

template <>
struct Raw128<uint16_t, 1> {
  using type = uint16x4_t;
};

template <>
struct Raw128<int8_t, 2> {
  using type = int8x8_t;
};

template <>
struct Raw128<int16_t, 1> {
  using type = int16x4_t;
};

template <>
struct Raw128<float16_t, 1> {
  using type = uint16x4_t;
};

// 8 (same as 64)
template <>
struct Raw128<uint8_t, 1> {
  using type = uint8x8_t;
};

template <>
struct Raw128<int8_t, 1> {
  using type = int8x8_t;
};

}  // namespace detail

template <typename T, size_t N = 16 / sizeof(T)>
class Vec128 {
  using Raw = typename detail::Raw128<T, N>::type;

 public:
  HWY_INLINE Vec128() {}
  Vec128(const Vec128&) = default;
  Vec128& operator=(const Vec128&) = default;
  HWY_INLINE explicit Vec128(const Raw raw) : raw(raw) {}

  // Compound assignment. Only usable if there is a corresponding non-member
  // binary operator overload. For example, only f32 and f64 support division.
  HWY_INLINE Vec128& operator*=(const Vec128 other) {
    return *this = (*this * other);
  }
  HWY_INLINE Vec128& operator/=(const Vec128 other) {
    return *this = (*this / other);
  }
  HWY_INLINE Vec128& operator+=(const Vec128 other) {
    return *this = (*this + other);
  }
  HWY_INLINE Vec128& operator-=(const Vec128 other) {
    return *this = (*this - other);
  }
  HWY_INLINE Vec128& operator&=(const Vec128 other) {
    return *this = (*this & other);
  }
  HWY_INLINE Vec128& operator|=(const Vec128 other) {
    return *this = (*this | other);
  }
  HWY_INLINE Vec128& operator^=(const Vec128 other) {
    return *this = (*this ^ other);
  }

  Raw raw;
};

// FF..FF or 0.
template <typename T, size_t N = 16 / sizeof(T)>
class Mask128 {
  // ARM C Language Extensions return and expect unsigned type.
  using Raw = typename detail::Raw128<MakeUnsigned<T>, N>::type;

 public:
  HWY_INLINE Mask128() {}
  Mask128(const Mask128&) = default;
  Mask128& operator=(const Mask128&) = default;
  HWY_INLINE explicit Mask128(const Raw raw) : raw(raw) {}

  Raw raw;
};

namespace detail {

// Deduce Simd<T, N> from Vec128<T, N>
struct DeduceD {
  template <typename T, size_t N>
  Simd<T, N> operator()(Vec128<T, N>) const {
    return Simd<T, N>();
  }
};

}  // namespace detail

template <class V>
using DFromV = decltype(detail::DeduceD()(V()));

// ------------------------------ BitCast

namespace detail {

// Converts from Vec128<T, N> to Vec128<uint8_t, N * sizeof(T)> using the
// vreinterpret*_u8_*() set of functions.
#define HWY_NEON_BUILD_TPL_HWY_CAST_TO_U8
#define HWY_NEON_BUILD_RET_HWY_CAST_TO_U8(type, size) \
  Vec128<uint8_t, size * sizeof(type)>
#define HWY_NEON_BUILD_PARAM_HWY_CAST_TO_U8(type, size) Vec128<type, size> v
#define HWY_NEON_BUILD_ARG_HWY_CAST_TO_U8 v.raw

// Special case of u8 to u8 since vreinterpret*_u8_u8 is obviously not defined.
template <size_t N>
HWY_INLINE Vec128<uint8_t, N> BitCastToByte(Vec128<uint8_t, N> v) {
  return v;
}

HWY_NEON_DEF_FUNCTION_ALL_FLOATS(BitCastToByte, vreinterpret, _u8_,
                                 HWY_CAST_TO_U8)
HWY_NEON_DEF_FUNCTION_INTS(BitCastToByte, vreinterpret, _u8_, HWY_CAST_TO_U8)
HWY_NEON_DEF_FUNCTION_UINT_16(BitCastToByte, vreinterpret, _u8_, HWY_CAST_TO_U8)
HWY_NEON_DEF_FUNCTION_UINT_32(BitCastToByte, vreinterpret, _u8_, HWY_CAST_TO_U8)
HWY_NEON_DEF_FUNCTION_UINT_64(BitCastToByte, vreinterpret, _u8_, HWY_CAST_TO_U8)

// Special case for float16_t, which has the same Raw as uint16_t.
template <size_t N>
HWY_INLINE Vec128<uint8_t, N * 2> BitCastToByte(Vec128<float16_t, N> v) {
  return BitCastToByte(Vec128<uint16_t, N>(v.raw));
}

#undef HWY_NEON_BUILD_TPL_HWY_CAST_TO_U8
#undef HWY_NEON_BUILD_RET_HWY_CAST_TO_U8
#undef HWY_NEON_BUILD_PARAM_HWY_CAST_TO_U8
#undef HWY_NEON_BUILD_ARG_HWY_CAST_TO_U8

template <size_t N>
HWY_INLINE Vec128<uint8_t, N> BitCastFromByte(Simd<uint8_t, N> /* tag */,
                                              Vec128<uint8_t, N> v) {
  return v;
}

// 64-bit or less:

template <size_t N, HWY_IF_LE64(int8_t, N)>
HWY_INLINE Vec128<int8_t, N> BitCastFromByte(Simd<int8_t, N> /* tag */,
                                             Vec128<uint8_t, N> v) {
  return Vec128<int8_t, N>(vreinterpret_s8_u8(v.raw));
}
template <size_t N, HWY_IF_LE64(uint16_t, N)>
HWY_INLINE Vec128<uint16_t, N> BitCastFromByte(Simd<uint16_t, N> /* tag */,
                                               Vec128<uint8_t, N * 2> v) {
  return Vec128<uint16_t, N>(vreinterpret_u16_u8(v.raw));
}
template <size_t N, HWY_IF_LE64(int16_t, N)>
HWY_INLINE Vec128<int16_t, N> BitCastFromByte(Simd<int16_t, N> /* tag */,
                                              Vec128<uint8_t, N * 2> v) {
  return Vec128<int16_t, N>(vreinterpret_s16_u8(v.raw));
}
template <size_t N, HWY_IF_LE64(uint32_t, N)>
HWY_INLINE Vec128<uint32_t, N> BitCastFromByte(Simd<uint32_t, N> /* tag */,
                                               Vec128<uint8_t, N * 4> v) {
  return Vec128<uint32_t, N>(vreinterpret_u32_u8(v.raw));
}
template <size_t N, HWY_IF_LE64(int32_t, N)>
HWY_INLINE Vec128<int32_t, N> BitCastFromByte(Simd<int32_t, N> /* tag */,
                                              Vec128<uint8_t, N * 4> v) {
  return Vec128<int32_t, N>(vreinterpret_s32_u8(v.raw));
}
template <size_t N, HWY_IF_LE64(float, N)>
HWY_INLINE Vec128<float, N> BitCastFromByte(Simd<float, N> /* tag */,
                                            Vec128<uint8_t, N * 4> v) {
  return Vec128<float, N>(vreinterpret_f32_u8(v.raw));
}
HWY_INLINE Vec128<uint64_t, 1> BitCastFromByte(Simd<uint64_t, 1> /* tag */,
                                               Vec128<uint8_t, 1 * 8> v) {
  return Vec128<uint64_t, 1>(vreinterpret_u64_u8(v.raw));
}
HWY_INLINE Vec128<int64_t, 1> BitCastFromByte(Simd<int64_t, 1> /* tag */,
                                              Vec128<uint8_t, 1 * 8> v) {
  return Vec128<int64_t, 1>(vreinterpret_s64_u8(v.raw));
}
#if HWY_ARCH_ARM_A64
HWY_INLINE Vec128<double, 1> BitCastFromByte(Simd<double, 1> /* tag */,
                                             Vec128<uint8_t, 1 * 8> v) {
  return Vec128<double, 1>(vreinterpret_f64_u8(v.raw));
}
#endif

// 128-bit full:

HWY_INLINE Vec128<int8_t> BitCastFromByte(Full128<int8_t> /* tag */,
                                          Vec128<uint8_t> v) {
  return Vec128<int8_t>(vreinterpretq_s8_u8(v.raw));
}
HWY_INLINE Vec128<uint16_t> BitCastFromByte(Full128<uint16_t> /* tag */,
                                            Vec128<uint8_t> v) {
  return Vec128<uint16_t>(vreinterpretq_u16_u8(v.raw));
}
HWY_INLINE Vec128<int16_t> BitCastFromByte(Full128<int16_t> /* tag */,
                                           Vec128<uint8_t> v) {
  return Vec128<int16_t>(vreinterpretq_s16_u8(v.raw));
}
HWY_INLINE Vec128<uint32_t> BitCastFromByte(Full128<uint32_t> /* tag */,
                                            Vec128<uint8_t> v) {
  return Vec128<uint32_t>(vreinterpretq_u32_u8(v.raw));
}
HWY_INLINE Vec128<int32_t> BitCastFromByte(Full128<int32_t> /* tag */,
                                           Vec128<uint8_t> v) {
  return Vec128<int32_t>(vreinterpretq_s32_u8(v.raw));
}
HWY_INLINE Vec128<float> BitCastFromByte(Full128<float> /* tag */,
                                         Vec128<uint8_t> v) {
  return Vec128<float>(vreinterpretq_f32_u8(v.raw));
}
HWY_INLINE Vec128<uint64_t> BitCastFromByte(Full128<uint64_t> /* tag */,
                                            Vec128<uint8_t> v) {
  return Vec128<uint64_t>(vreinterpretq_u64_u8(v.raw));
}
HWY_INLINE Vec128<int64_t> BitCastFromByte(Full128<int64_t> /* tag */,
                                           Vec128<uint8_t> v) {
  return Vec128<int64_t>(vreinterpretq_s64_u8(v.raw));
}

#if HWY_ARCH_ARM_A64
HWY_INLINE Vec128<double> BitCastFromByte(Full128<double> /* tag */,
                                          Vec128<uint8_t> v) {
  return Vec128<double>(vreinterpretq_f64_u8(v.raw));
}
#endif

// Special case for float16_t, which has the same Raw as uint16_t.
template <size_t N>
HWY_INLINE Vec128<float16_t, N> BitCastFromByte(Simd<float16_t, N> /* tag */,
                                                Vec128<uint8_t, N * 2> v) {
  return Vec128<float16_t, N>(BitCastFromByte(Simd<uint16_t, N>(), v).raw);
}

}  // namespace detail

template <typename T, size_t N, typename FromT>
HWY_API Vec128<T, N> BitCast(Simd<T, N> d,
                             Vec128<FromT, N * sizeof(T) / sizeof(FromT)> v) {
  return detail::BitCastFromByte(d, detail::BitCastToByte(v));
}

// ------------------------------ Set

// Returns a vector with all lanes set to "t".
#define HWY_NEON_BUILD_TPL_HWY_SET1
#define HWY_NEON_BUILD_RET_HWY_SET1(type, size) Vec128<type, size>
#define HWY_NEON_BUILD_PARAM_HWY_SET1(type, size) \
  Simd<type, size> /* tag */, const type t
#define HWY_NEON_BUILD_ARG_HWY_SET1 t

HWY_NEON_DEF_FUNCTION_ALL_TYPES(Set, vdup, _n_, HWY_SET1)

#undef HWY_NEON_BUILD_TPL_HWY_SET1
#undef HWY_NEON_BUILD_RET_HWY_SET1
#undef HWY_NEON_BUILD_PARAM_HWY_SET1
#undef HWY_NEON_BUILD_ARG_HWY_SET1

// Returns an all-zero vector.
template <typename T, size_t N>
HWY_API Vec128<T, N> Zero(Simd<T, N> d) {
  return Set(d, 0);
}

template <class D>
using VFromD = decltype(Zero(D()));

// Returns a vector with uninitialized elements.
template <typename T, size_t N>
HWY_API Vec128<T, N> Undefined(Simd<T, N> /*d*/) {
  HWY_DIAGNOSTICS(push)
  HWY_DIAGNOSTICS_OFF(disable : 4701, ignored "-Wuninitialized")
  typename detail::Raw128<T, N>::type a;
  return Vec128<T, N>(a);
  HWY_DIAGNOSTICS(pop)
}

// Returns a vector with lane i=[0, N) set to "first" + i.
template <typename T, size_t N, typename T2>
Vec128<T, N> Iota(const Simd<T, N> d, const T2 first) {
  HWY_ALIGN T lanes[16 / sizeof(T)];
  for (size_t i = 0; i < 16 / sizeof(T); ++i) {
    lanes[i] = static_cast<T>(first + static_cast<T2>(i));
  }
  return Load(d, lanes);
}

// ------------------------------ GetLane

HWY_API uint8_t GetLane(const Vec128<uint8_t, 16> v) {
  return vgetq_lane_u8(v.raw, 0);
}
template <size_t N>
HWY_API uint8_t GetLane(const Vec128<uint8_t, N> v) {
  return vget_lane_u8(v.raw, 0);
}

HWY_API int8_t GetLane(const Vec128<int8_t, 16> v) {
  return vgetq_lane_s8(v.raw, 0);
}
template <size_t N>
HWY_API int8_t GetLane(const Vec128<int8_t, N> v) {
  return vget_lane_s8(v.raw, 0);
}

HWY_API uint16_t GetLane(const Vec128<uint16_t, 8> v) {
  return vgetq_lane_u16(v.raw, 0);
}
template <size_t N>
HWY_API uint16_t GetLane(const Vec128<uint16_t, N> v) {
  return vget_lane_u16(v.raw, 0);
}

HWY_API int16_t GetLane(const Vec128<int16_t, 8> v) {
  return vgetq_lane_s16(v.raw, 0);
}
template <size_t N>
HWY_API int16_t GetLane(const Vec128<int16_t, N> v) {
  return vget_lane_s16(v.raw, 0);
}

HWY_API uint32_t GetLane(const Vec128<uint32_t, 4> v) {
  return vgetq_lane_u32(v.raw, 0);
}
template <size_t N>
HWY_API uint32_t GetLane(const Vec128<uint32_t, N> v) {
  return vget_lane_u32(v.raw, 0);
}

HWY_API int32_t GetLane(const Vec128<int32_t, 4> v) {
  return vgetq_lane_s32(v.raw, 0);
}
template <size_t N>
HWY_API int32_t GetLane(const Vec128<int32_t, N> v) {
  return vget_lane_s32(v.raw, 0);
}

HWY_API uint64_t GetLane(const Vec128<uint64_t, 2> v) {
  return vgetq_lane_u64(v.raw, 0);
}
HWY_API uint64_t GetLane(const Vec128<uint64_t, 1> v) {
  return vget_lane_u64(v.raw, 0);
}
HWY_API int64_t GetLane(const Vec128<int64_t, 2> v) {
  return vgetq_lane_s64(v.raw, 0);
}
HWY_API int64_t GetLane(const Vec128<int64_t, 1> v) {
  return vget_lane_s64(v.raw, 0);
}

HWY_API float GetLane(const Vec128<float, 4> v) {
  return vgetq_lane_f32(v.raw, 0);
}
HWY_API float GetLane(const Vec128<float, 2> v) {
  return vget_lane_f32(v.raw, 0);
}
HWY_API float GetLane(const Vec128<float, 1> v) {
  return vget_lane_f32(v.raw, 0);
}
#if HWY_ARCH_ARM_A64
HWY_API double GetLane(const Vec128<double, 2> v) {
  return vgetq_lane_f64(v.raw, 0);
}
HWY_API double GetLane(const Vec128<double, 1> v) {
  return vget_lane_f64(v.raw, 0);
}
#endif

// ================================================== ARITHMETIC

// ------------------------------ Addition
HWY_NEON_DEF_FUNCTION_ALL_TYPES(operator+, vadd, _, 2)

// ------------------------------ Subtraction
HWY_NEON_DEF_FUNCTION_ALL_TYPES(operator-, vsub, _, 2)

// ------------------------------ Saturating addition and subtraction
// Only defined for uint8_t, uint16_t and their signed versions, as in other
// architectures.

// Returns a + b clamped to the destination range.
HWY_NEON_DEF_FUNCTION_INT_8(SaturatedAdd, vqadd, _, 2)
HWY_NEON_DEF_FUNCTION_INT_16(SaturatedAdd, vqadd, _, 2)
HWY_NEON_DEF_FUNCTION_UINT_8(SaturatedAdd, vqadd, _, 2)
HWY_NEON_DEF_FUNCTION_UINT_16(SaturatedAdd, vqadd, _, 2)

// Returns a - b clamped to the destination range.
HWY_NEON_DEF_FUNCTION_INT_8(SaturatedSub, vqsub, _, 2)
HWY_NEON_DEF_FUNCTION_INT_16(SaturatedSub, vqsub, _, 2)
HWY_NEON_DEF_FUNCTION_UINT_8(SaturatedSub, vqsub, _, 2)
HWY_NEON_DEF_FUNCTION_UINT_16(SaturatedSub, vqsub, _, 2)

// Not part of API, used in implementation.
namespace detail {
HWY_NEON_DEF_FUNCTION_UINT_32(SaturatedSub, vqsub, _, 2)
HWY_NEON_DEF_FUNCTION_UINT_64(SaturatedSub, vqsub, _, 2)
HWY_NEON_DEF_FUNCTION_INT_32(SaturatedSub, vqsub, _, 2)
HWY_NEON_DEF_FUNCTION_INT_64(SaturatedSub, vqsub, _, 2)
}  // namespace detail

// ------------------------------ Average

// Returns (a + b + 1) / 2
HWY_NEON_DEF_FUNCTION_UINT_8(AverageRound, vrhadd, _, 2)
HWY_NEON_DEF_FUNCTION_UINT_16(AverageRound, vrhadd, _, 2)

// ------------------------------ Neg

HWY_NEON_DEF_FUNCTION_ALL_FLOATS(Neg, vneg, _, 1)
HWY_NEON_DEF_FUNCTION_INT_8_16_32(Neg, vneg, _, 1)  // i64 implemented below

HWY_API Vec128<int64_t, 1> Neg(const Vec128<int64_t, 1> v) {
#if HWY_ARCH_ARM_A64
  return Vec128<int64_t, 1>(vneg_s64(v.raw));
#else
  return Zero(Simd<int64_t, 1>()) - v;
#endif
}

HWY_API Vec128<int64_t> Neg(const Vec128<int64_t> v) {
#if HWY_ARCH_ARM_A64
  return Vec128<int64_t>(vnegq_s64(v.raw));
#else
  return Zero(Full128<int64_t>()) - v;
#endif
}

// ------------------------------ ShiftLeft

// Customize HWY_NEON_DEF_FUNCTION to special-case count=0 (not supported).
#pragma push_macro("HWY_NEON_DEF_FUNCTION")
#undef HWY_NEON_DEF_FUNCTION
#define HWY_NEON_DEF_FUNCTION(type, size, name, prefix, infix, suffix, args)   \
  template <int kBits>                                                         \
  HWY_API Vec128<type, size> name(const Vec128<type, size> v) {                \
    return kBits == 0 ? v                                                      \
                      : Vec128<type, size>(HWY_NEON_EVAL(                      \
                            prefix##infix##suffix, v.raw, HWY_MAX(1, kBits))); \
  }

HWY_NEON_DEF_FUNCTION_INTS_UINTS(ShiftLeft, vshl, _n_, HWY_SHIFT)

HWY_NEON_DEF_FUNCTION_UINTS(ShiftRight, vshr, _n_, HWY_SHIFT)
HWY_NEON_DEF_FUNCTION_INTS(ShiftRight, vshr, _n_, HWY_SHIFT)

#pragma pop_macro("HWY_NEON_DEF_FUNCTION")

// ------------------------------ Shl

HWY_API Vec128<uint8_t> operator<<(const Vec128<uint8_t> v,
                                   const Vec128<uint8_t> bits) {
  return Vec128<uint8_t>(vshlq_u8(v.raw, vreinterpretq_s8_u8(bits.raw)));
}
template <size_t N, HWY_IF_LE64(uint8_t, N)>
HWY_API Vec128<uint8_t, N> operator<<(const Vec128<uint8_t, N> v,
                                      const Vec128<uint8_t, N> bits) {
  return Vec128<uint8_t, N>(vshl_u8(v.raw, vreinterpret_s8_u8(bits.raw)));
}

HWY_API Vec128<uint16_t> operator<<(const Vec128<uint16_t> v,
                                    const Vec128<uint16_t> bits) {
  return Vec128<uint16_t>(vshlq_u16(v.raw, vreinterpretq_s16_u16(bits.raw)));
}
template <size_t N, HWY_IF_LE64(uint16_t, N)>
HWY_API Vec128<uint16_t, N> operator<<(const Vec128<uint16_t, N> v,
                                       const Vec128<uint16_t, N> bits) {
  return Vec128<uint16_t, N>(vshl_u16(v.raw, vreinterpret_s16_u16(bits.raw)));
}

HWY_API Vec128<uint32_t> operator<<(const Vec128<uint32_t> v,
                                    const Vec128<uint32_t> bits) {
  return Vec128<uint32_t>(vshlq_u32(v.raw, vreinterpretq_s32_u32(bits.raw)));
}
template <size_t N, HWY_IF_LE64(uint32_t, N)>
HWY_API Vec128<uint32_t, N> operator<<(const Vec128<uint32_t, N> v,
                                       const Vec128<uint32_t, N> bits) {
  return Vec128<uint32_t, N>(vshl_u32(v.raw, vreinterpret_s32_u32(bits.raw)));
}

HWY_API Vec128<uint64_t> operator<<(const Vec128<uint64_t> v,
                                    const Vec128<uint64_t> bits) {
  return Vec128<uint64_t>(vshlq_u64(v.raw, vreinterpretq_s64_u64(bits.raw)));
}
HWY_API Vec128<uint64_t, 1> operator<<(const Vec128<uint64_t, 1> v,
                                       const Vec128<uint64_t, 1> bits) {
  return Vec128<uint64_t, 1>(vshl_u64(v.raw, vreinterpret_s64_u64(bits.raw)));
}

HWY_API Vec128<int8_t> operator<<(const Vec128<int8_t> v,
                                  const Vec128<int8_t> bits) {
  return Vec128<int8_t>(vshlq_s8(v.raw, bits.raw));
}
template <size_t N, HWY_IF_LE64(int8_t, N)>
HWY_API Vec128<int8_t, N> operator<<(const Vec128<int8_t, N> v,
                                     const Vec128<int8_t, N> bits) {
  return Vec128<int8_t, N>(vshl_s8(v.raw, bits.raw));
}

HWY_API Vec128<int16_t> operator<<(const Vec128<int16_t> v,
                                   const Vec128<int16_t> bits) {
  return Vec128<int16_t>(vshlq_s16(v.raw, bits.raw));
}
template <size_t N, HWY_IF_LE64(int16_t, N)>
HWY_API Vec128<int16_t, N> operator<<(const Vec128<int16_t, N> v,
                                      const Vec128<int16_t, N> bits) {
  return Vec128<int16_t, N>(vshl_s16(v.raw, bits.raw));
}

HWY_API Vec128<int32_t> operator<<(const Vec128<int32_t> v,
                                   const Vec128<int32_t> bits) {
  return Vec128<int32_t>(vshlq_s32(v.raw, bits.raw));
}
template <size_t N, HWY_IF_LE64(int32_t, N)>
HWY_API Vec128<int32_t, N> operator<<(const Vec128<int32_t, N> v,
                                      const Vec128<int32_t, N> bits) {
  return Vec128<int32_t, N>(vshl_s32(v.raw, bits.raw));
}

HWY_API Vec128<int64_t> operator<<(const Vec128<int64_t> v,
                                   const Vec128<int64_t> bits) {
  return Vec128<int64_t>(vshlq_s64(v.raw, bits.raw));
}
HWY_API Vec128<int64_t, 1> operator<<(const Vec128<int64_t, 1> v,
                                      const Vec128<int64_t, 1> bits) {
  return Vec128<int64_t, 1>(vshl_s64(v.raw, bits.raw));
}

// ------------------------------ Shr (Neg)

HWY_API Vec128<uint8_t> operator>>(const Vec128<uint8_t> v,
                                   const Vec128<uint8_t> bits) {
  const int8x16_t neg_bits = Neg(BitCast(Full128<int8_t>(), bits)).raw;
  return Vec128<uint8_t>(vshlq_u8(v.raw, neg_bits));
}
template <size_t N, HWY_IF_LE64(uint8_t, N)>
HWY_API Vec128<uint8_t, N> operator>>(const Vec128<uint8_t, N> v,
                                      const Vec128<uint8_t, N> bits) {
  const int8x8_t neg_bits = Neg(BitCast(Simd<int8_t, N>(), bits)).raw;
  return Vec128<uint8_t, N>(vshl_u8(v.raw, neg_bits));
}

HWY_API Vec128<uint16_t> operator>>(const Vec128<uint16_t> v,
                                    const Vec128<uint16_t> bits) {
  const int16x8_t neg_bits = Neg(BitCast(Full128<int16_t>(), bits)).raw;
  return Vec128<uint16_t>(vshlq_u16(v.raw, neg_bits));
}
template <size_t N, HWY_IF_LE64(uint16_t, N)>
HWY_API Vec128<uint16_t, N> operator>>(const Vec128<uint16_t, N> v,
                                       const Vec128<uint16_t, N> bits) {
  const int16x4_t neg_bits = Neg(BitCast(Simd<int16_t, N>(), bits)).raw;
  return Vec128<uint16_t, N>(vshl_u16(v.raw, neg_bits));
}

HWY_API Vec128<uint32_t> operator>>(const Vec128<uint32_t> v,
                                    const Vec128<uint32_t> bits) {
  const int32x4_t neg_bits = Neg(BitCast(Full128<int32_t>(), bits)).raw;
  return Vec128<uint32_t>(vshlq_u32(v.raw, neg_bits));
}
template <size_t N, HWY_IF_LE64(uint32_t, N)>
HWY_API Vec128<uint32_t, N> operator>>(const Vec128<uint32_t, N> v,
                                       const Vec128<uint32_t, N> bits) {
  const int32x2_t neg_bits = Neg(BitCast(Simd<int32_t, N>(), bits)).raw;
  return Vec128<uint32_t, N>(vshl_u32(v.raw, neg_bits));
}

HWY_API Vec128<uint64_t> operator>>(const Vec128<uint64_t> v,
                                    const Vec128<uint64_t> bits) {
  const int64x2_t neg_bits = Neg(BitCast(Full128<int64_t>(), bits)).raw;
  return Vec128<uint64_t>(vshlq_u64(v.raw, neg_bits));
}
HWY_API Vec128<uint64_t, 1> operator>>(const Vec128<uint64_t, 1> v,
                                       const Vec128<uint64_t, 1> bits) {
  const int64x1_t neg_bits = Neg(BitCast(Simd<int64_t, 1>(), bits)).raw;
  return Vec128<uint64_t, 1>(vshl_u64(v.raw, neg_bits));
}

HWY_API Vec128<int8_t> operator>>(const Vec128<int8_t> v,
                                  const Vec128<int8_t> bits) {
  return Vec128<int8_t>(vshlq_s8(v.raw, Neg(bits).raw));
}
template <size_t N, HWY_IF_LE64(int8_t, N)>
HWY_API Vec128<int8_t, N> operator>>(const Vec128<int8_t, N> v,
                                     const Vec128<int8_t, N> bits) {
  return Vec128<int8_t, N>(vshl_s8(v.raw, Neg(bits).raw));
}

HWY_API Vec128<int16_t> operator>>(const Vec128<int16_t> v,
                                   const Vec128<int16_t> bits) {
  return Vec128<int16_t>(vshlq_s16(v.raw, Neg(bits).raw));
}
template <size_t N, HWY_IF_LE64(int16_t, N)>
HWY_API Vec128<int16_t, N> operator>>(const Vec128<int16_t, N> v,
                                      const Vec128<int16_t, N> bits) {
  return Vec128<int16_t, N>(vshl_s16(v.raw, Neg(bits).raw));
}

HWY_API Vec128<int32_t> operator>>(const Vec128<int32_t> v,
                                   const Vec128<int32_t> bits) {
  return Vec128<int32_t>(vshlq_s32(v.raw, Neg(bits).raw));
}
template <size_t N, HWY_IF_LE64(int32_t, N)>
HWY_API Vec128<int32_t, N> operator>>(const Vec128<int32_t, N> v,
                                      const Vec128<int32_t, N> bits) {
  return Vec128<int32_t, N>(vshl_s32(v.raw, Neg(bits).raw));
}

HWY_API Vec128<int64_t> operator>>(const Vec128<int64_t> v,
                                   const Vec128<int64_t> bits) {
  return Vec128<int64_t>(vshlq_s64(v.raw, Neg(bits).raw));
}
HWY_API Vec128<int64_t, 1> operator>>(const Vec128<int64_t, 1> v,
                                      const Vec128<int64_t, 1> bits) {
  return Vec128<int64_t, 1>(vshl_s64(v.raw, Neg(bits).raw));
}

// ------------------------------ ShiftLeftSame (Shl)

template <typename T, size_t N>
HWY_API Vec128<T, N> ShiftLeftSame(const Vec128<T, N> v, int bits) {
  return v << Set(Simd<T, N>(), static_cast<T>(bits));
}
template <typename T, size_t N>
HWY_API Vec128<T, N> ShiftRightSame(const Vec128<T, N> v, int bits) {
  return v >> Set(Simd<T, N>(), static_cast<T>(bits));
}

// ------------------------------ Integer multiplication

// Unsigned
HWY_API Vec128<uint16_t> operator*(const Vec128<uint16_t> a,
                                   const Vec128<uint16_t> b) {
  return Vec128<uint16_t>(vmulq_u16(a.raw, b.raw));
}
HWY_API Vec128<uint32_t> operator*(const Vec128<uint32_t> a,
                                   const Vec128<uint32_t> b) {
  return Vec128<uint32_t>(vmulq_u32(a.raw, b.raw));
}

template <size_t N, HWY_IF_LE64(uint16_t, N)>
HWY_API Vec128<uint16_t, N> operator*(const Vec128<uint16_t, N> a,
                                      const Vec128<uint16_t, N> b) {
  return Vec128<uint16_t, N>(vmul_u16(a.raw, b.raw));
}
template <size_t N, HWY_IF_LE64(uint32_t, N)>
HWY_API Vec128<uint32_t, N> operator*(const Vec128<uint32_t, N> a,
                                      const Vec128<uint32_t, N> b) {
  return Vec128<uint32_t, N>(vmul_u32(a.raw, b.raw));
}

// Signed
HWY_API Vec128<int16_t> operator*(const Vec128<int16_t> a,
                                  const Vec128<int16_t> b) {
  return Vec128<int16_t>(vmulq_s16(a.raw, b.raw));
}
HWY_API Vec128<int32_t> operator*(const Vec128<int32_t> a,
                                  const Vec128<int32_t> b) {
  return Vec128<int32_t>(vmulq_s32(a.raw, b.raw));
}

template <size_t N, HWY_IF_LE64(uint16_t, N)>
HWY_API Vec128<int16_t, N> operator*(const Vec128<int16_t, N> a,
                                     const Vec128<int16_t, N> b) {
  return Vec128<int16_t, N>(vmul_s16(a.raw, b.raw));
}
template <size_t N, HWY_IF_LE64(int32_t, N)>
HWY_API Vec128<int32_t, N> operator*(const Vec128<int32_t, N> a,
                                     const Vec128<int32_t, N> b) {
  return Vec128<int32_t, N>(vmul_s32(a.raw, b.raw));
}

// Returns the upper 16 bits of a * b in each lane.
HWY_API Vec128<int16_t> MulHigh(const Vec128<int16_t> a,
                                const Vec128<int16_t> b) {
  int32x4_t rlo = vmull_s16(vget_low_s16(a.raw), vget_low_s16(b.raw));
#if HWY_ARCH_ARM_A64
  int32x4_t rhi = vmull_high_s16(a.raw, b.raw);
#else
  int32x4_t rhi = vmull_s16(vget_high_s16(a.raw), vget_high_s16(b.raw));
#endif
  return Vec128<int16_t>(
      vuzp2q_s16(vreinterpretq_s16_s32(rlo), vreinterpretq_s16_s32(rhi)));
}
HWY_API Vec128<uint16_t> MulHigh(const Vec128<uint16_t> a,
                                 const Vec128<uint16_t> b) {
  uint32x4_t rlo = vmull_u16(vget_low_u16(a.raw), vget_low_u16(b.raw));
#if HWY_ARCH_ARM_A64
  uint32x4_t rhi = vmull_high_u16(a.raw, b.raw);
#else
  uint32x4_t rhi = vmull_u16(vget_high_u16(a.raw), vget_high_u16(b.raw));
#endif
  return Vec128<uint16_t>(
      vuzp2q_u16(vreinterpretq_u16_u32(rlo), vreinterpretq_u16_u32(rhi)));
}

template <size_t N, HWY_IF_LE64(int16_t, N)>
HWY_API Vec128<int16_t, N> MulHigh(const Vec128<int16_t, N> a,
                                   const Vec128<int16_t, N> b) {
  int16x8_t hi_lo = vreinterpretq_s16_s32(vmull_s16(a.raw, b.raw));
  return Vec128<int16_t, N>(vget_low_s16(vuzp2q_s16(hi_lo, hi_lo)));
}
template <size_t N, HWY_IF_LE64(uint16_t, N)>
HWY_API Vec128<uint16_t, N> MulHigh(const Vec128<uint16_t, N> a,
                                    const Vec128<uint16_t, N> b) {
  uint16x8_t hi_lo = vreinterpretq_u16_u32(vmull_u16(a.raw, b.raw));
  return Vec128<uint16_t, N>(vget_low_u16(vuzp2q_u16(hi_lo, hi_lo)));
}

// Multiplies even lanes (0, 2 ..) and places the double-wide result into
// even and the upper half into its odd neighbor lane.
HWY_API Vec128<int64_t> MulEven(const Vec128<int32_t> a,
                                const Vec128<int32_t> b) {
  int32x4_t a_packed = vuzp1q_s32(a.raw, a.raw);
  int32x4_t b_packed = vuzp1q_s32(b.raw, b.raw);
  return Vec128<int64_t>(
      vmull_s32(vget_low_s32(a_packed), vget_low_s32(b_packed)));
}
HWY_API Vec128<uint64_t> MulEven(const Vec128<uint32_t> a,
                                 const Vec128<uint32_t> b) {
  uint32x4_t a_packed = vuzp1q_u32(a.raw, a.raw);
  uint32x4_t b_packed = vuzp1q_u32(b.raw, b.raw);
  return Vec128<uint64_t>(
      vmull_u32(vget_low_u32(a_packed), vget_low_u32(b_packed)));
}

template <size_t N>
HWY_API Vec128<int64_t, (N + 1) / 2> MulEven(const Vec128<int32_t, N> a,
                                             const Vec128<int32_t, N> b) {
  int32x2_t a_packed = vuzp1_s32(a.raw, a.raw);
  int32x2_t b_packed = vuzp1_s32(b.raw, b.raw);
  return Vec128<int64_t, (N + 1) / 2>(
      vget_low_s64(vmull_s32(a_packed, b_packed)));
}
template <size_t N>
HWY_API Vec128<uint64_t, (N + 1) / 2> MulEven(const Vec128<uint32_t, N> a,
                                              const Vec128<uint32_t, N> b) {
  uint32x2_t a_packed = vuzp1_u32(a.raw, a.raw);
  uint32x2_t b_packed = vuzp1_u32(b.raw, b.raw);
  return Vec128<uint64_t, (N + 1) / 2>(
      vget_low_u64(vmull_u32(a_packed, b_packed)));
}

HWY_INLINE Vec128<uint64_t> MulEven(const Vec128<uint64_t> a,
                                    const Vec128<uint64_t> b) {
  uint64_t hi;
  uint64_t lo = Mul128(vgetq_lane_u64(a.raw, 0), vgetq_lane_u64(b.raw, 0), &hi);
  return Vec128<uint64_t>(vsetq_lane_u64(hi, vdupq_n_u64(lo), 1));
}

HWY_INLINE Vec128<uint64_t> MulOdd(const Vec128<uint64_t> a,
                                   const Vec128<uint64_t> b) {
  uint64_t hi;
  uint64_t lo = Mul128(vgetq_lane_u64(a.raw, 1), vgetq_lane_u64(b.raw, 1), &hi);
  return Vec128<uint64_t>(vsetq_lane_u64(hi, vdupq_n_u64(lo), 1));
}

// ------------------------------ Floating-point mul / div

HWY_NEON_DEF_FUNCTION_ALL_FLOATS(operator*, vmul, _, 2)

// Approximate reciprocal
HWY_API Vec128<float> ApproximateReciprocal(const Vec128<float> v) {
  return Vec128<float>(vrecpeq_f32(v.raw));
}
template <size_t N>
HWY_API Vec128<float, N> ApproximateReciprocal(const Vec128<float, N> v) {
  return Vec128<float, N>(vrecpe_f32(v.raw));
}

#if HWY_ARCH_ARM_A64
HWY_NEON_DEF_FUNCTION_ALL_FLOATS(operator/, vdiv, _, 2)
#else
// Not defined on armv7: approximate
namespace detail {

HWY_INLINE Vec128<float> ReciprocalNewtonRaphsonStep(
    const Vec128<float> recip, const Vec128<float> divisor) {
  return Vec128<float>(vrecpsq_f32(recip.raw, divisor.raw));
}
template <size_t N>
HWY_INLINE Vec128<float, N> ReciprocalNewtonRaphsonStep(
    const Vec128<float, N> recip, Vec128<float, N> divisor) {
  return Vec128<float, N>(vrecps_f32(recip.raw, divisor.raw));
}

}  // namespace detail

template <size_t N>
HWY_API Vec128<float, N> operator/(const Vec128<float, N> a,
                                   const Vec128<float, N> b) {
  auto x = ApproximateReciprocal(b);
  x *= detail::ReciprocalNewtonRaphsonStep(x, b);
  x *= detail::ReciprocalNewtonRaphsonStep(x, b);
  x *= detail::ReciprocalNewtonRaphsonStep(x, b);
  return a * x;
}
#endif

// ------------------------------ Absolute value of difference.

HWY_API Vec128<float> AbsDiff(const Vec128<float> a, const Vec128<float> b) {
  return Vec128<float>(vabdq_f32(a.raw, b.raw));
}
template <size_t N, HWY_IF_LE64(float, N)>
HWY_API Vec128<float, N> AbsDiff(const Vec128<float, N> a,
                                 const Vec128<float, N> b) {
  return Vec128<float, N>(vabd_f32(a.raw, b.raw));
}

// ------------------------------ Floating-point multiply-add variants

// Returns add + mul * x
#if defined(__ARM_VFPV4__) || HWY_ARCH_ARM_A64
template <size_t N, HWY_IF_LE64(float, N)>
HWY_API Vec128<float, N> MulAdd(const Vec128<float, N> mul,
                                const Vec128<float, N> x,
                                const Vec128<float, N> add) {
  return Vec128<float, N>(vfma_f32(add.raw, mul.raw, x.raw));
}
HWY_API Vec128<float> MulAdd(const Vec128<float> mul, const Vec128<float> x,
                             const Vec128<float> add) {
  return Vec128<float>(vfmaq_f32(add.raw, mul.raw, x.raw));
}
#else
// Emulate FMA for floats.
template <size_t N>
HWY_API Vec128<float, N> MulAdd(const Vec128<float, N> mul,
                                const Vec128<float, N> x,
                                const Vec128<float, N> add) {
  return mul * x + add;
}
#endif

#if HWY_ARCH_ARM_A64
HWY_API Vec128<double, 1> MulAdd(const Vec128<double, 1> mul,
                                 const Vec128<double, 1> x,
                                 const Vec128<double, 1> add) {
  return Vec128<double, 1>(vfma_f64(add.raw, mul.raw, x.raw));
}
HWY_API Vec128<double> MulAdd(const Vec128<double> mul, const Vec128<double> x,
                              const Vec128<double> add) {
  return Vec128<double>(vfmaq_f64(add.raw, mul.raw, x.raw));
}
#endif

// Returns add - mul * x
#if defined(__ARM_VFPV4__) || HWY_ARCH_ARM_A64
template <size_t N, HWY_IF_LE64(float, N)>
HWY_API Vec128<float, N> NegMulAdd(const Vec128<float, N> mul,
                                   const Vec128<float, N> x,
                                   const Vec128<float, N> add) {
  return Vec128<float, N>(vfms_f32(add.raw, mul.raw, x.raw));
}
HWY_API Vec128<float> NegMulAdd(const Vec128<float> mul, const Vec128<float> x,
                                const Vec128<float> add) {
  return Vec128<float>(vfmsq_f32(add.raw, mul.raw, x.raw));
}
#else
// Emulate FMA for floats.
template <size_t N>
HWY_API Vec128<float, N> NegMulAdd(const Vec128<float, N> mul,
                                   const Vec128<float, N> x,
                                   const Vec128<float, N> add) {
  return add - mul * x;
}
#endif

#if HWY_ARCH_ARM_A64
HWY_API Vec128<double, 1> NegMulAdd(const Vec128<double, 1> mul,
                                    const Vec128<double, 1> x,
                                    const Vec128<double, 1> add) {
  return Vec128<double, 1>(vfms_f64(add.raw, mul.raw, x.raw));
}
HWY_API Vec128<double> NegMulAdd(const Vec128<double> mul,
                                 const Vec128<double> x,
                                 const Vec128<double> add) {
  return Vec128<double>(vfmsq_f64(add.raw, mul.raw, x.raw));
}
#endif

// Returns mul * x - sub
template <size_t N>
HWY_API Vec128<float, N> MulSub(const Vec128<float, N> mul,
                                const Vec128<float, N> x,
                                const Vec128<float, N> sub) {
  return MulAdd(mul, x, Neg(sub));
}

// Returns -mul * x - sub
template <size_t N>
HWY_API Vec128<float, N> NegMulSub(const Vec128<float, N> mul,
                                   const Vec128<float, N> x,
                                   const Vec128<float, N> sub) {
  return Neg(MulAdd(mul, x, sub));
}

#if HWY_ARCH_ARM_A64
template <size_t N>
HWY_API Vec128<double, N> MulSub(const Vec128<double, N> mul,
                                 const Vec128<double, N> x,
                                 const Vec128<double, N> sub) {
  return MulAdd(mul, x, Neg(sub));
}
template <size_t N>
HWY_API Vec128<double, N> NegMulSub(const Vec128<double, N> mul,
                                    const Vec128<double, N> x,
                                    const Vec128<double, N> sub) {
  return Neg(MulAdd(mul, x, sub));
}
#endif

// ------------------------------ Floating-point square root (IfThenZeroElse)

// Approximate reciprocal square root
HWY_API Vec128<float> ApproximateReciprocalSqrt(const Vec128<float> v) {
  return Vec128<float>(vrsqrteq_f32(v.raw));
}
template <size_t N>
HWY_API Vec128<float, N> ApproximateReciprocalSqrt(const Vec128<float, N> v) {
  return Vec128<float, N>(vrsqrte_f32(v.raw));
}

// Full precision square root
#if HWY_ARCH_ARM_A64
HWY_NEON_DEF_FUNCTION_ALL_FLOATS(Sqrt, vsqrt, _, 1)
#else
namespace detail {

HWY_INLINE Vec128<float> ReciprocalSqrtStep(const Vec128<float> root,
                                            const Vec128<float> recip) {
  return Vec128<float>(vrsqrtsq_f32(root.raw, recip.raw));
}
template <size_t N>
HWY_INLINE Vec128<float, N> ReciprocalSqrtStep(const Vec128<float, N> root,
                                               Vec128<float, N> recip) {
  return Vec128<float, N>(vrsqrts_f32(root.raw, recip.raw));
}

}  // namespace detail

// Not defined on armv7: approximate
template <size_t N>
HWY_API Vec128<float, N> Sqrt(const Vec128<float, N> v) {
  auto recip = ApproximateReciprocalSqrt(v);

  recip *= detail::ReciprocalSqrtStep(v * recip, recip);
  recip *= detail::ReciprocalSqrtStep(v * recip, recip);
  recip *= detail::ReciprocalSqrtStep(v * recip, recip);

  const auto root = v * recip;
  return IfThenZeroElse(v == Zero(Simd<float, N>()), root);
}
#endif

// ================================================== LOGICAL

// ------------------------------ Not

// There is no 64-bit vmvn, so cast instead of using HWY_NEON_DEF_FUNCTION.
template <typename T>
HWY_API Vec128<T> Not(const Vec128<T> v) {
  const Full128<T> d;
  const Repartition<uint8_t, decltype(d)> d8;
  return BitCast(d, Vec128<uint8_t>(vmvnq_u8(BitCast(d8, v).raw)));
}
template <typename T, size_t N, HWY_IF_LE64(T, N)>
HWY_API Vec128<T, N> Not(const Vec128<T, N> v) {
  const Simd<T, N> d;
  const Repartition<uint8_t, decltype(d)> d8;
  using V8 = decltype(Zero(d8));
  return BitCast(d, V8(vmvn_u8(BitCast(d8, v).raw)));
}

// ------------------------------ And
HWY_NEON_DEF_FUNCTION_INTS_UINTS(And, vand, _, 2)

// Uses the u32/64 defined above.
template <typename T, size_t N, HWY_IF_FLOAT(T)>
HWY_API Vec128<T, N> And(const Vec128<T, N> a, const Vec128<T, N> b) {
  const Simd<MakeUnsigned<T>, N> d;
  return BitCast(Simd<T, N>(), BitCast(d, a) & BitCast(d, b));
}

// ------------------------------ AndNot

namespace internal {
// reversed_andnot returns a & ~b.
HWY_NEON_DEF_FUNCTION_INTS_UINTS(reversed_andnot, vbic, _, 2)
}  // namespace internal

// Returns ~not_mask & mask.
template <typename T, size_t N, HWY_IF_NOT_FLOAT(T)>
HWY_API Vec128<T, N> AndNot(const Vec128<T, N> not_mask,
                            const Vec128<T, N> mask) {
  return internal::reversed_andnot(mask, not_mask);
}

// Uses the u32/64 defined above.
template <typename T, size_t N, HWY_IF_FLOAT(T)>
HWY_API Vec128<T, N> AndNot(const Vec128<T, N> not_mask,
                            const Vec128<T, N> mask) {
  const Simd<MakeUnsigned<T>, N> du;
  Vec128<MakeUnsigned<T>, N> ret =
      internal::reversed_andnot(BitCast(du, mask), BitCast(du, not_mask));
  return BitCast(Simd<T, N>(), ret);
}

// ------------------------------ Or

HWY_NEON_DEF_FUNCTION_INTS_UINTS(Or, vorr, _, 2)

// Uses the u32/64 defined above.
template <typename T, size_t N, HWY_IF_FLOAT(T)>
HWY_API Vec128<T, N> Or(const Vec128<T, N> a, const Vec128<T, N> b) {
  const Simd<MakeUnsigned<T>, N> d;
  return BitCast(Simd<T, N>(), BitCast(d, a) | BitCast(d, b));
}

// ------------------------------ Xor

HWY_NEON_DEF_FUNCTION_INTS_UINTS(Xor, veor, _, 2)

// Uses the u32/64 defined above.
template <typename T, size_t N, HWY_IF_FLOAT(T)>
HWY_API Vec128<T, N> Xor(const Vec128<T, N> a, const Vec128<T, N> b) {
  const Simd<MakeUnsigned<T>, N> d;
  return BitCast(Simd<T, N>(), BitCast(d, a) ^ BitCast(d, b));
}

// ------------------------------ Operator overloads (internal-only if float)

template <typename T, size_t N>
HWY_API Vec128<T, N> operator&(const Vec128<T, N> a, const Vec128<T, N> b) {
  return And(a, b);
}

template <typename T, size_t N>
HWY_API Vec128<T, N> operator|(const Vec128<T, N> a, const Vec128<T, N> b) {
  return Or(a, b);
}

template <typename T, size_t N>
HWY_API Vec128<T, N> operator^(const Vec128<T, N> a, const Vec128<T, N> b) {
  return Xor(a, b);
}

// ------------------------------ PopulationCount

#ifdef HWY_NATIVE_POPCNT
#undef HWY_NATIVE_POPCNT
#else
#define HWY_NATIVE_POPCNT
#endif

namespace detail {

template <typename T>
HWY_INLINE Vec128<T> PopulationCount(hwy::SizeTag<1> /* tag */, Vec128<T> v) {
  const Full128<uint8_t> d8;
  return Vec128<T>(vcntq_u8(BitCast(d8, v).raw));
}
template <typename T, size_t N, HWY_IF_LE64(T, N)>
HWY_INLINE Vec128<T, N> PopulationCount(hwy::SizeTag<1> /* tag */,
                                        Vec128<T, N> v) {
  const Simd<uint8_t, N> d8;
  return Vec128<T, N>(vcnt_u8(BitCast(d8, v).raw));
}

// ARM lacks popcount for lane sizes > 1, so take pairwise sums of the bytes.
template <typename T>
HWY_INLINE Vec128<T> PopulationCount(hwy::SizeTag<2> /* tag */, Vec128<T> v) {
  const Full128<uint8_t> d8;
  const uint8x16_t bytes = vcntq_u8(BitCast(d8, v).raw);
  return Vec128<T>(vpaddlq_u8(bytes));
}
template <typename T, size_t N, HWY_IF_LE64(T, N)>
HWY_INLINE Vec128<T, N> PopulationCount(hwy::SizeTag<2> /* tag */,
                                        Vec128<T, N> v) {
  const Repartition<uint8_t, Simd<T, N>> d8;
  const uint8x8_t bytes = vcnt_u8(BitCast(d8, v).raw);
  return Vec128<T, N>(vpaddl_u8(bytes));
}

template <typename T>
HWY_INLINE Vec128<T> PopulationCount(hwy::SizeTag<4> /* tag */, Vec128<T> v) {
  const Full128<uint8_t> d8;
  const uint8x16_t bytes = vcntq_u8(BitCast(d8, v).raw);
  return Vec128<T>(vpaddlq_u16(vpaddlq_u8(bytes)));
}
template <typename T, size_t N, HWY_IF_LE64(T, N)>
HWY_INLINE Vec128<T, N> PopulationCount(hwy::SizeTag<4> /* tag */,
                                        Vec128<T, N> v) {
  const Repartition<uint8_t, Simd<T, N>> d8;
  const uint8x8_t bytes = vcnt_u8(BitCast(d8, v).raw);
  return Vec128<T, N>(vpaddl_u16(vpaddl_u8(bytes)));
}

template <typename T>
HWY_INLINE Vec128<T> PopulationCount(hwy::SizeTag<8> /* tag */, Vec128<T> v) {
  const Full128<uint8_t> d8;
  const uint8x16_t bytes = vcntq_u8(BitCast(d8, v).raw);
  return Vec128<T>(vpaddlq_u32(vpaddlq_u16(vpaddlq_u8(bytes))));
}
template <typename T, size_t N, HWY_IF_LE64(T, N)>
HWY_INLINE Vec128<T, N> PopulationCount(hwy::SizeTag<8> /* tag */,
                                        Vec128<T, N> v) {
  const Repartition<uint8_t, Simd<T, N>> d8;
  const uint8x8_t bytes = vcnt_u8(BitCast(d8, v).raw);
  return Vec128<T, N>(vpaddl_u32(vpaddl_u16(vpaddl_u8(bytes))));
}

}  // namespace detail

template <typename T, size_t N, HWY_IF_NOT_FLOAT(T)>
HWY_API Vec128<T, N> PopulationCount(Vec128<T, N> v) {
  return detail::PopulationCount(hwy::SizeTag<sizeof(T)>(), v);
}

// ================================================== SIGN

// ------------------------------ Abs

// Returns absolute value, except that LimitsMin() maps to LimitsMax() + 1.
HWY_API Vec128<int8_t> Abs(const Vec128<int8_t> v) {
  return Vec128<int8_t>(vabsq_s8(v.raw));
}
HWY_API Vec128<int16_t> Abs(const Vec128<int16_t> v) {
  return Vec128<int16_t>(vabsq_s16(v.raw));
}
HWY_API Vec128<int32_t> Abs(const Vec128<int32_t> v) {
  return Vec128<int32_t>(vabsq_s32(v.raw));
}
// i64 is implemented after BroadcastSignBit.
HWY_API Vec128<float> Abs(const Vec128<float> v) {
  return Vec128<float>(vabsq_f32(v.raw));
}

template <size_t N, HWY_IF_LE64(int8_t, N)>
HWY_API Vec128<int8_t, N> Abs(const Vec128<int8_t, N> v) {
  return Vec128<int8_t, N>(vabs_s8(v.raw));
}
template <size_t N, HWY_IF_LE64(int16_t, N)>
HWY_API Vec128<int16_t, N> Abs(const Vec128<int16_t, N> v) {
  return Vec128<int16_t, N>(vabs_s16(v.raw));
}
template <size_t N, HWY_IF_LE64(int32_t, N)>
HWY_API Vec128<int32_t, N> Abs(const Vec128<int32_t, N> v) {
  return Vec128<int32_t, N>(vabs_s32(v.raw));
}
template <size_t N, HWY_IF_LE64(float, N)>
HWY_API Vec128<float, N> Abs(const Vec128<float, N> v) {
  return Vec128<float, N>(vabs_f32(v.raw));
}

#if HWY_ARCH_ARM_A64
HWY_API Vec128<double> Abs(const Vec128<double> v) {
  return Vec128<double>(vabsq_f64(v.raw));
}

HWY_API Vec128<double, 1> Abs(const Vec128<double, 1> v) {
  return Vec128<double, 1>(vabs_f64(v.raw));
}
#endif

// ------------------------------ CopySign

template <typename T, size_t N>
HWY_API Vec128<T, N> CopySign(const Vec128<T, N> magn,
                              const Vec128<T, N> sign) {
  static_assert(IsFloat<T>(), "Only makes sense for floating-point");
  const auto msb = SignBit(Simd<T, N>());
  return Or(AndNot(msb, magn), And(msb, sign));
}

template <typename T, size_t N>
HWY_API Vec128<T, N> CopySignToAbs(const Vec128<T, N> abs,
                                   const Vec128<T, N> sign) {
  static_assert(IsFloat<T>(), "Only makes sense for floating-point");
  return Or(abs, And(SignBit(Simd<T, N>()), sign));
}

// ------------------------------ BroadcastSignBit

template <typename T, size_t N>
HWY_API Vec128<T, N> BroadcastSignBit(const Vec128<T, N> v) {
  return ShiftRight<sizeof(T) * 8 - 1>(v);
}

// ================================================== MASK

// ------------------------------ To/from vector

// Mask and Vec have the same representation (true = FF..FF).
template <typename T, size_t N>
HWY_API Mask128<T, N> MaskFromVec(const Vec128<T, N> v) {
  const Simd<MakeUnsigned<T>, N> du;
  return Mask128<T, N>(BitCast(du, v).raw);
}

// DEPRECATED
template <typename T, size_t N>
HWY_API Vec128<T, N> VecFromMask(const Mask128<T, N> v) {
  return BitCast(Simd<T, N>(), Vec128<MakeUnsigned<T>, N>(v.raw));
}

template <typename T, size_t N>
HWY_API Vec128<T, N> VecFromMask(Simd<T, N> d, const Mask128<T, N> v) {
  return BitCast(d, Vec128<MakeUnsigned<T>, N>(v.raw));
}

// ------------------------------ RebindMask

template <typename TFrom, typename TTo, size_t N>
HWY_API Mask128<TTo, N> RebindMask(Simd<TTo, N> dto, Mask128<TFrom, N> m) {
  static_assert(sizeof(TFrom) == sizeof(TTo), "Must have same size");
  return MaskFromVec(BitCast(dto, VecFromMask(Simd<TFrom, N>(), m)));
}

// ------------------------------ IfThenElse(mask, yes, no) = mask ? b : a.

#define HWY_NEON_BUILD_TPL_HWY_IF
#define HWY_NEON_BUILD_RET_HWY_IF(type, size) Vec128<type, size>
#define HWY_NEON_BUILD_PARAM_HWY_IF(type, size)                 \
  const Mask128<type, size> mask, const Vec128<type, size> yes, \
      const Vec128<type, size> no
#define HWY_NEON_BUILD_ARG_HWY_IF mask.raw, yes.raw, no.raw

HWY_NEON_DEF_FUNCTION_ALL_TYPES(IfThenElse, vbsl, _, HWY_IF)

#undef HWY_NEON_BUILD_TPL_HWY_IF
#undef HWY_NEON_BUILD_RET_HWY_IF
#undef HWY_NEON_BUILD_PARAM_HWY_IF
#undef HWY_NEON_BUILD_ARG_HWY_IF

// mask ? yes : 0
template <typename T, size_t N>
HWY_API Vec128<T, N> IfThenElseZero(const Mask128<T, N> mask,
                                    const Vec128<T, N> yes) {
  return yes & VecFromMask(Simd<T, N>(), mask);
}

// mask ? 0 : no
template <typename T, size_t N>
HWY_API Vec128<T, N> IfThenZeroElse(const Mask128<T, N> mask,
                                    const Vec128<T, N> no) {
  return AndNot(VecFromMask(Simd<T, N>(), mask), no);
}

template <typename T, size_t N>
HWY_API Vec128<T, N> ZeroIfNegative(Vec128<T, N> v) {
  const auto zero = Zero(Simd<T, N>());
  return Max(zero, v);
}

// ------------------------------ Mask logical

template <typename T, size_t N>
HWY_API Mask128<T, N> Not(const Mask128<T, N> m) {
  return MaskFromVec(Not(VecFromMask(Simd<T, N>(), m)));
}

template <typename T, size_t N>
HWY_API Mask128<T, N> And(const Mask128<T, N> a, Mask128<T, N> b) {
  const Simd<T, N> d;
  return MaskFromVec(And(VecFromMask(d, a), VecFromMask(d, b)));
}

template <typename T, size_t N>
HWY_API Mask128<T, N> AndNot(const Mask128<T, N> a, Mask128<T, N> b) {
  const Simd<T, N> d;
  return MaskFromVec(AndNot(VecFromMask(d, a), VecFromMask(d, b)));
}

template <typename T, size_t N>
HWY_API Mask128<T, N> Or(const Mask128<T, N> a, Mask128<T, N> b) {
  const Simd<T, N> d;
  return MaskFromVec(Or(VecFromMask(d, a), VecFromMask(d, b)));
}

template <typename T, size_t N>
HWY_API Mask128<T, N> Xor(const Mask128<T, N> a, Mask128<T, N> b) {
  const Simd<T, N> d;
  return MaskFromVec(Xor(VecFromMask(d, a), VecFromMask(d, b)));
}

// ================================================== COMPARE

// Comparisons fill a lane with 1-bits if the condition is true, else 0.

// ------------------------------ Shuffle2301 (for i64 compares)

// Swap 32-bit halves in 64-bits
HWY_API Vec128<uint32_t, 2> Shuffle2301(const Vec128<uint32_t, 2> v) {
  return Vec128<uint32_t, 2>(vrev64_u32(v.raw));
}
HWY_API Vec128<int32_t, 2> Shuffle2301(const Vec128<int32_t, 2> v) {
  return Vec128<int32_t, 2>(vrev64_s32(v.raw));
}
HWY_API Vec128<float, 2> Shuffle2301(const Vec128<float, 2> v) {
  return Vec128<float, 2>(vrev64_f32(v.raw));
}
HWY_API Vec128<uint32_t> Shuffle2301(const Vec128<uint32_t> v) {
  return Vec128<uint32_t>(vrev64q_u32(v.raw));
}
HWY_API Vec128<int32_t> Shuffle2301(const Vec128<int32_t> v) {
  return Vec128<int32_t>(vrev64q_s32(v.raw));
}
HWY_API Vec128<float> Shuffle2301(const Vec128<float> v) {
  return Vec128<float>(vrev64q_f32(v.raw));
}

#define HWY_NEON_BUILD_TPL_HWY_COMPARE
#define HWY_NEON_BUILD_RET_HWY_COMPARE(type, size) Mask128<type, size>
#define HWY_NEON_BUILD_PARAM_HWY_COMPARE(type, size) \
  const Vec128<type, size> a, const Vec128<type, size> b
#define HWY_NEON_BUILD_ARG_HWY_COMPARE a.raw, b.raw

// ------------------------------ Equality
HWY_NEON_DEF_FUNCTION_ALL_FLOATS(operator==, vceq, _, HWY_COMPARE)
#if HWY_ARCH_ARM_A64
HWY_NEON_DEF_FUNCTION_INTS_UINTS(operator==, vceq, _, HWY_COMPARE)
#else
// No 64-bit comparisons on armv7: emulate them below, after Shuffle2301.
HWY_NEON_DEF_FUNCTION_INT_8_16_32(operator==, vceq, _, HWY_COMPARE)
HWY_NEON_DEF_FUNCTION_UINT_8_16_32(operator==, vceq, _, HWY_COMPARE)
#endif

// ------------------------------ Inequality
template <typename T, size_t N>
HWY_API Mask128<T, N> operator!=(const Vec128<T, N> a, const Vec128<T, N> b) {
  return Not(a == b);
}

// ------------------------------ Strict inequality (signed, float)
#if HWY_ARCH_ARM_A64
HWY_NEON_DEF_FUNCTION_INTS(operator<, vclt, _, HWY_COMPARE)
#else
HWY_NEON_DEF_FUNCTION_INT_8_16_32(operator<, vclt, _, HWY_COMPARE)
#endif
HWY_NEON_DEF_FUNCTION_ALL_FLOATS(operator<, vclt, _, HWY_COMPARE)

// ------------------------------ Weak inequality (float)
HWY_NEON_DEF_FUNCTION_ALL_FLOATS(operator<=, vcle, _, HWY_COMPARE)

#undef HWY_NEON_BUILD_TPL_HWY_COMPARE
#undef HWY_NEON_BUILD_RET_HWY_COMPARE
#undef HWY_NEON_BUILD_PARAM_HWY_COMPARE
#undef HWY_NEON_BUILD_ARG_HWY_COMPARE

// ------------------------------ ARMv7 i64 compare (Shuffle2301, Eq)

#if HWY_ARCH_ARM_V7

template <size_t N>
HWY_API Mask128<int64_t, N> operator==(const Vec128<int64_t, N> a,
                                       const Vec128<int64_t, N> b) {
  const Simd<int32_t, N * 2> d32;
  const Simd<int64_t, N> d64;
  const auto cmp32 = VecFromMask(d32, Eq(BitCast(d32, a), BitCast(d32, b)));
  const auto cmp64 = cmp32 & Shuffle2301(cmp32);
  return MaskFromVec(BitCast(d64, cmp64));
}

template <size_t N>
HWY_API Mask128<uint64_t, N> operator==(const Vec128<uint64_t, N> a,
                                        const Vec128<uint64_t, N> b) {
  const Simd<uint32_t, N * 2> d32;
  const Simd<uint64_t, N> d64;
  const auto cmp32 = VecFromMask(d32, Eq(BitCast(d32, a), BitCast(d32, b)));
  const auto cmp64 = cmp32 & Shuffle2301(cmp32);
  return MaskFromVec(BitCast(d64, cmp64));
}

HWY_API Mask128<int64_t> operator<(const Vec128<int64_t> a,
                                   const Vec128<int64_t> b) {
  const int64x2_t sub = vqsubq_s64(a.raw, b.raw);
  return MaskFromVec(BroadcastSignBit(Vec128<int64_t>(sub)));
}
HWY_API Mask128<int64_t, 1> operator<(const Vec128<int64_t, 1> a,
                                      const Vec128<int64_t, 1> b) {
  const int64x1_t sub = vqsub_s64(a.raw, b.raw);
  return MaskFromVec(BroadcastSignBit(Vec128<int64_t, 1>(sub)));
}

#endif

// ------------------------------ Reversed comparisons

template <typename T, size_t N>
HWY_API Mask128<T, N> operator>(Vec128<T, N> a, Vec128<T, N> b) {
  return operator<(b, a);
}
template <typename T, size_t N>
HWY_API Mask128<T, N> operator>=(Vec128<T, N> a, Vec128<T, N> b) {
  return operator<=(b, a);
}

// ------------------------------ FirstN (Iota, Lt)

template <typename T, size_t N>
HWY_API Mask128<T, N> FirstN(const Simd<T, N> d, size_t num) {
  const RebindToSigned<decltype(d)> di;  // Signed comparisons are cheaper.
  return RebindMask(d, Iota(di, 0) < Set(di, static_cast<MakeSigned<T>>(num)));
}

// ------------------------------ TestBit (Eq)

#define HWY_NEON_BUILD_TPL_HWY_TESTBIT
#define HWY_NEON_BUILD_RET_HWY_TESTBIT(type, size) Mask128<type, size>
#define HWY_NEON_BUILD_PARAM_HWY_TESTBIT(type, size) \
  Vec128<type, size> v, Vec128<type, size> bit
#define HWY_NEON_BUILD_ARG_HWY_TESTBIT v.raw, bit.raw

#if HWY_ARCH_ARM_A64
HWY_NEON_DEF_FUNCTION_INTS_UINTS(TestBit, vtst, _, HWY_TESTBIT)
#else
// No 64-bit versions on armv7
HWY_NEON_DEF_FUNCTION_UINT_8_16_32(TestBit, vtst, _, HWY_TESTBIT)
HWY_NEON_DEF_FUNCTION_INT_8_16_32(TestBit, vtst, _, HWY_TESTBIT)

template <size_t N>
HWY_API Mask128<uint64_t, N> TestBit(Vec128<uint64_t, N> v,
                                     Vec128<uint64_t, N> bit) {
  return (v & bit) == bit;
}
template <size_t N>
HWY_API Mask128<int64_t, N> TestBit(Vec128<int64_t, N> v,
                                    Vec128<int64_t, N> bit) {
  return (v & bit) == bit;
}

#endif
#undef HWY_NEON_BUILD_TPL_HWY_TESTBIT
#undef HWY_NEON_BUILD_RET_HWY_TESTBIT
#undef HWY_NEON_BUILD_PARAM_HWY_TESTBIT
#undef HWY_NEON_BUILD_ARG_HWY_TESTBIT

// ------------------------------ Abs i64 (IfThenElse, BroadcastSignBit)
HWY_API Vec128<int64_t> Abs(const Vec128<int64_t> v) {
#if HWY_ARCH_ARM_A64
  return Vec128<int64_t>(vabsq_s64(v.raw));
#else
  const auto zero = Zero(Full128<int64_t>());
  return IfThenElse(MaskFromVec(BroadcastSignBit(v)), zero - v, v);
#endif
}
HWY_API Vec128<int64_t, 1> Abs(const Vec128<int64_t, 1> v) {
#if HWY_ARCH_ARM_A64
  return Vec128<int64_t, 1>(vabs_s64(v.raw));
#else
  const auto zero = Zero(Simd<int64_t, 1>());
  return IfThenElse(MaskFromVec(BroadcastSignBit(v)), zero - v, v);
#endif
}

// ------------------------------ Min (IfThenElse, BroadcastSignBit)

#if HWY_ARCH_ARM_A64

HWY_API Mask128<uint64_t> operator<(Vec128<uint64_t> a, Vec128<uint64_t> b) {
  return Mask128<uint64_t>(vcltq_u64(a.raw, b.raw));
}
HWY_API Mask128<uint64_t, 1> operator<(Vec128<uint64_t, 1> a,
                                       Vec128<uint64_t, 1> b) {
  return Mask128<uint64_t, 1>(vclt_u64(a.raw, b.raw));
}

#endif

// Unsigned
HWY_NEON_DEF_FUNCTION_UINT_8_16_32(Min, vmin, _, 2)

template <size_t N>
HWY_API Vec128<uint64_t, N> Min(const Vec128<uint64_t, N> a,
                                const Vec128<uint64_t, N> b) {
#if HWY_ARCH_ARM_A64
  return IfThenElse(b < a, b, a);
#else
  const Simd<uint64_t, N> du;
  const Simd<int64_t, N> di;
  return BitCast(du, BitCast(di, a) - BitCast(di, detail::SaturatedSub(a, b)));
#endif
}

// Signed
HWY_NEON_DEF_FUNCTION_INT_8_16_32(Min, vmin, _, 2)

template <size_t N>
HWY_API Vec128<int64_t, N> Min(const Vec128<int64_t, N> a,
                               const Vec128<int64_t, N> b) {
#if HWY_ARCH_ARM_A64
  return IfThenElse(b < a, b, a);
#else
  const Vec128<int64_t, N> sign = detail::SaturatedSub(a, b);
  return IfThenElse(MaskFromVec(BroadcastSignBit(sign)), a, b);
#endif
}

// Float: IEEE minimumNumber on v8, otherwise NaN if any is NaN.
#if HWY_ARCH_ARM_A64
HWY_NEON_DEF_FUNCTION_ALL_FLOATS(Min, vminnm, _, 2)
#else
HWY_NEON_DEF_FUNCTION_ALL_FLOATS(Min, vmin, _, 2)
#endif

// ------------------------------ Max (IfThenElse, BroadcastSignBit)

// Unsigned (no u64)
HWY_NEON_DEF_FUNCTION_UINT_8_16_32(Max, vmax, _, 2)

template <size_t N>
HWY_API Vec128<uint64_t, N> Max(const Vec128<uint64_t, N> a,
                                const Vec128<uint64_t, N> b) {
#if HWY_ARCH_ARM_A64
  return IfThenElse(b < a, a, b);
#else
  const Simd<uint64_t, N> du;
  const Simd<int64_t, N> di;
  return BitCast(du, BitCast(di, b) + BitCast(di, detail::SaturatedSub(a, b)));
#endif
}

// Signed (no i64)
HWY_NEON_DEF_FUNCTION_INT_8_16_32(Max, vmax, _, 2)

template <size_t N>
HWY_API Vec128<int64_t, N> Max(const Vec128<int64_t, N> a,
                               const Vec128<int64_t, N> b) {
#if HWY_ARCH_ARM_A64
  return IfThenElse(b < a, a, b);
#else
  const Vec128<int64_t, N> sign = detail::SaturatedSub(a, b);
  return IfThenElse(MaskFromVec(BroadcastSignBit(sign)), b, a);
#endif
}

// Float: IEEE maximumNumber on v8, otherwise NaN if any is NaN.
#if HWY_ARCH_ARM_A64
HWY_NEON_DEF_FUNCTION_ALL_FLOATS(Max, vmaxnm, _, 2)
#else
HWY_NEON_DEF_FUNCTION_ALL_FLOATS(Max, vmax, _, 2)
#endif

// ================================================== MEMORY

// ------------------------------ Load 128

HWY_API Vec128<uint8_t> LoadU(Full128<uint8_t> /* tag */,
                              const uint8_t* HWY_RESTRICT unaligned) {
  return Vec128<uint8_t>(vld1q_u8(unaligned));
}
HWY_API Vec128<uint16_t> LoadU(Full128<uint16_t> /* tag */,
                               const uint16_t* HWY_RESTRICT unaligned) {
  return Vec128<uint16_t>(vld1q_u16(unaligned));
}
HWY_API Vec128<uint32_t> LoadU(Full128<uint32_t> /* tag */,
                               const uint32_t* HWY_RESTRICT unaligned) {
  return Vec128<uint32_t>(vld1q_u32(unaligned));
}
HWY_API Vec128<uint64_t> LoadU(Full128<uint64_t> /* tag */,
                               const uint64_t* HWY_RESTRICT unaligned) {
  return Vec128<uint64_t>(vld1q_u64(unaligned));
}
HWY_API Vec128<int8_t> LoadU(Full128<int8_t> /* tag */,
                             const int8_t* HWY_RESTRICT unaligned) {
  return Vec128<int8_t>(vld1q_s8(unaligned));
}
HWY_API Vec128<int16_t> LoadU(Full128<int16_t> /* tag */,
                              const int16_t* HWY_RESTRICT unaligned) {
  return Vec128<int16_t>(vld1q_s16(unaligned));
}
HWY_API Vec128<int32_t> LoadU(Full128<int32_t> /* tag */,
                              const int32_t* HWY_RESTRICT unaligned) {
  return Vec128<int32_t>(vld1q_s32(unaligned));
}
HWY_API Vec128<int64_t> LoadU(Full128<int64_t> /* tag */,
                              const int64_t* HWY_RESTRICT unaligned) {
  return Vec128<int64_t>(vld1q_s64(unaligned));
}
HWY_API Vec128<float> LoadU(Full128<float> /* tag */,
                            const float* HWY_RESTRICT unaligned) {
  return Vec128<float>(vld1q_f32(unaligned));
}
#if HWY_ARCH_ARM_A64
HWY_API Vec128<double> LoadU(Full128<double> /* tag */,
                             const double* HWY_RESTRICT unaligned) {
  return Vec128<double>(vld1q_f64(unaligned));
}
#endif

// ------------------------------ Load 64

HWY_API Vec128<uint8_t, 8> LoadU(Simd<uint8_t, 8> /* tag */,
                                 const uint8_t* HWY_RESTRICT p) {
  return Vec128<uint8_t, 8>(vld1_u8(p));
}
HWY_API Vec128<uint16_t, 4> LoadU(Simd<uint16_t, 4> /* tag */,
                                  const uint16_t* HWY_RESTRICT p) {
  return Vec128<uint16_t, 4>(vld1_u16(p));
}
HWY_API Vec128<uint32_t, 2> LoadU(Simd<uint32_t, 2> /* tag */,
                                  const uint32_t* HWY_RESTRICT p) {
  return Vec128<uint32_t, 2>(vld1_u32(p));
}
HWY_API Vec128<uint64_t, 1> LoadU(Simd<uint64_t, 1> /* tag */,
                                  const uint64_t* HWY_RESTRICT p) {
  return Vec128<uint64_t, 1>(vld1_u64(p));
}
HWY_API Vec128<int8_t, 8> LoadU(Simd<int8_t, 8> /* tag */,
                                const int8_t* HWY_RESTRICT p) {
  return Vec128<int8_t, 8>(vld1_s8(p));
}
HWY_API Vec128<int16_t, 4> LoadU(Simd<int16_t, 4> /* tag */,
                                 const int16_t* HWY_RESTRICT p) {
  return Vec128<int16_t, 4>(vld1_s16(p));
}
HWY_API Vec128<int32_t, 2> LoadU(Simd<int32_t, 2> /* tag */,
                                 const int32_t* HWY_RESTRICT p) {
  return Vec128<int32_t, 2>(vld1_s32(p));
}
HWY_API Vec128<int64_t, 1> LoadU(Simd<int64_t, 1> /* tag */,
                                 const int64_t* HWY_RESTRICT p) {
  return Vec128<int64_t, 1>(vld1_s64(p));
}
HWY_API Vec128<float, 2> LoadU(Simd<float, 2> /* tag */,
                               const float* HWY_RESTRICT p) {
  return Vec128<float, 2>(vld1_f32(p));
}
#if HWY_ARCH_ARM_A64
HWY_API Vec128<double, 1> LoadU(Simd<double, 1> /* tag */,
                                const double* HWY_RESTRICT p) {
  return Vec128<double, 1>(vld1_f64(p));
}
#endif

// ------------------------------ Load 32

// In the following load functions, |a| is purposely undefined.
// It is a required parameter to the intrinsic, however
// we don't actually care what is in it, and we don't want
// to introduce extra overhead by initializing it to something.

HWY_API Vec128<uint8_t, 4> LoadU(Simd<uint8_t, 4> /*tag*/,
                                 const uint8_t* HWY_RESTRICT p) {
  uint32x2_t a = Undefined(Simd<uint32_t, 2>()).raw;
  uint32x2_t b = vld1_lane_u32(reinterpret_cast<const uint32_t*>(p), a, 0);
  return Vec128<uint8_t, 4>(vreinterpret_u8_u32(b));
}
HWY_API Vec128<uint16_t, 2> LoadU(Simd<uint16_t, 2> /*tag*/,
                                  const uint16_t* HWY_RESTRICT p) {
  uint32x2_t a = Undefined(Simd<uint32_t, 2>()).raw;
  uint32x2_t b = vld1_lane_u32(reinterpret_cast<const uint32_t*>(p), a, 0);
  return Vec128<uint16_t, 2>(vreinterpret_u16_u32(b));
}
HWY_API Vec128<uint32_t, 1> LoadU(Simd<uint32_t, 1> /*tag*/,
                                  const uint32_t* HWY_RESTRICT p) {
  uint32x2_t a = Undefined(Simd<uint32_t, 2>()).raw;
  uint32x2_t b = vld1_lane_u32(p, a, 0);
  return Vec128<uint32_t, 1>(b);
}
HWY_API Vec128<int8_t, 4> LoadU(Simd<int8_t, 4> /*tag*/,
                                const int8_t* HWY_RESTRICT p) {
  int32x2_t a = Undefined(Simd<int32_t, 2>()).raw;
  int32x2_t b = vld1_lane_s32(reinterpret_cast<const int32_t*>(p), a, 0);
  return Vec128<int8_t, 4>(vreinterpret_s8_s32(b));
}
HWY_API Vec128<int16_t, 2> LoadU(Simd<int16_t, 2> /*tag*/,
                                 const int16_t* HWY_RESTRICT p) {
  int32x2_t a = Undefined(Simd<int32_t, 2>()).raw;
  int32x2_t b = vld1_lane_s32(reinterpret_cast<const int32_t*>(p), a, 0);
  return Vec128<int16_t, 2>(vreinterpret_s16_s32(b));
}
HWY_API Vec128<int32_t, 1> LoadU(Simd<int32_t, 1> /*tag*/,
                                 const int32_t* HWY_RESTRICT p) {
  int32x2_t a = Undefined(Simd<int32_t, 2>()).raw;
  int32x2_t b = vld1_lane_s32(p, a, 0);
  return Vec128<int32_t, 1>(b);
}
HWY_API Vec128<float, 1> LoadU(Simd<float, 1> /*tag*/,
                               const float* HWY_RESTRICT p) {
  float32x2_t a = Undefined(Simd<float, 2>()).raw;
  float32x2_t b = vld1_lane_f32(p, a, 0);
  return Vec128<float, 1>(b);
}

// ------------------------------ Load 16

HWY_API Vec128<uint8_t, 2> LoadU(Simd<uint8_t, 2> /*tag*/,
                                 const uint8_t* HWY_RESTRICT p) {
  uint16x4_t a = Undefined(Simd<uint16_t, 4>()).raw;
  uint16x4_t b = vld1_lane_u16(reinterpret_cast<const uint16_t*>(p), a, 0);
  return Vec128<uint8_t, 2>(vreinterpret_u8_u16(b));
}
HWY_API Vec128<uint16_t, 1> LoadU(Simd<uint16_t, 1> /*tag*/,
                                  const uint16_t* HWY_RESTRICT p) {
  uint16x4_t a = Undefined(Simd<uint16_t, 4>()).raw;
  uint16x4_t b = vld1_lane_u16(p, a, 0);
  return Vec128<uint16_t, 1>(b);
}
HWY_API Vec128<int8_t, 2> LoadU(Simd<int8_t, 2> /*tag*/,
                                const int8_t* HWY_RESTRICT p) {
  int16x4_t a = Undefined(Simd<int16_t, 4>()).raw;
  int16x4_t b = vld1_lane_s16(reinterpret_cast<const int16_t*>(p), a, 0);
  return Vec128<int8_t, 2>(vreinterpret_s8_s16(b));
}
HWY_API Vec128<int16_t, 1> LoadU(Simd<int16_t, 1> /*tag*/,
                                 const int16_t* HWY_RESTRICT p) {
  int16x4_t a = Undefined(Simd<int16_t, 4>()).raw;
  int16x4_t b = vld1_lane_s16(p, a, 0);
  return Vec128<int16_t, 1>(b);
}

// ------------------------------ Load 8

HWY_API Vec128<uint8_t, 1> LoadU(Simd<uint8_t, 1> d,
                                 const uint8_t* HWY_RESTRICT p) {
  uint8x8_t a = Undefined(d).raw;
  uint8x8_t b = vld1_lane_u8(p, a, 0);
  return Vec128<uint8_t, 1>(b);
}

HWY_API Vec128<int8_t, 1> LoadU(Simd<int8_t, 1> d,
                                const int8_t* HWY_RESTRICT p) {
  int8x8_t a = Undefined(d).raw;
  int8x8_t b = vld1_lane_s8(p, a, 0);
  return Vec128<int8_t, 1>(b);
}

// float16_t uses the same Raw as uint16_t, so forward to that.
template <size_t N>
HWY_API Vec128<float16_t, N> LoadU(Simd<float16_t, N> /*d*/,
                                   const float16_t* HWY_RESTRICT p) {
  const Simd<uint16_t, N> du16;
  const auto pu16 = reinterpret_cast<const uint16_t*>(p);
  return Vec128<float16_t, N>(LoadU(du16, pu16).raw);
}

// On ARM, Load is the same as LoadU.
template <typename T, size_t N>
HWY_API Vec128<T, N> Load(Simd<T, N> d, const T* HWY_RESTRICT p) {
  return LoadU(d, p);
}

template <typename T, size_t N>
HWY_API Vec128<T, N> MaskedLoad(Mask128<T, N> m, Simd<T, N> d,
                                const T* HWY_RESTRICT aligned) {
  return IfThenElseZero(m, Load(d, aligned));
}

// 128-bit SIMD => nothing to duplicate, same as an unaligned load.
template <typename T, size_t N, HWY_IF_LE128(T, N)>
HWY_API Vec128<T, N> LoadDup128(Simd<T, N> d, const T* const HWY_RESTRICT p) {
  return LoadU(d, p);
}

// ------------------------------ Store 128

HWY_API void StoreU(const Vec128<uint8_t> v, Full128<uint8_t> /* tag */,
                    uint8_t* HWY_RESTRICT unaligned) {
  vst1q_u8(unaligned, v.raw);
}
HWY_API void StoreU(const Vec128<uint16_t> v, Full128<uint16_t> /* tag */,
                    uint16_t* HWY_RESTRICT unaligned) {
  vst1q_u16(unaligned, v.raw);
}
HWY_API void StoreU(const Vec128<uint32_t> v, Full128<uint32_t> /* tag */,
                    uint32_t* HWY_RESTRICT unaligned) {
  vst1q_u32(unaligned, v.raw);
}
HWY_API void StoreU(const Vec128<uint64_t> v, Full128<uint64_t> /* tag */,
                    uint64_t* HWY_RESTRICT unaligned) {
  vst1q_u64(unaligned, v.raw);
}
HWY_API void StoreU(const Vec128<int8_t> v, Full128<int8_t> /* tag */,
                    int8_t* HWY_RESTRICT unaligned) {
  vst1q_s8(unaligned, v.raw);
}
HWY_API void StoreU(const Vec128<int16_t> v, Full128<int16_t> /* tag */,
                    int16_t* HWY_RESTRICT unaligned) {
  vst1q_s16(unaligned, v.raw);
}
HWY_API void StoreU(const Vec128<int32_t> v, Full128<int32_t> /* tag */,
                    int32_t* HWY_RESTRICT unaligned) {
  vst1q_s32(unaligned, v.raw);
}
HWY_API void StoreU(const Vec128<int64_t> v, Full128<int64_t> /* tag */,
                    int64_t* HWY_RESTRICT unaligned) {
  vst1q_s64(unaligned, v.raw);
}
HWY_API void StoreU(const Vec128<float> v, Full128<float> /* tag */,
                    float* HWY_RESTRICT unaligned) {
  vst1q_f32(unaligned, v.raw);
}
#if HWY_ARCH_ARM_A64
HWY_API void StoreU(const Vec128<double> v, Full128<double> /* tag */,
                    double* HWY_RESTRICT unaligned) {
  vst1q_f64(unaligned, v.raw);
}
#endif

// ------------------------------ Store 64

HWY_API void StoreU(const Vec128<uint8_t, 8> v, Simd<uint8_t, 8> /* tag */,
                    uint8_t* HWY_RESTRICT p) {
  vst1_u8(p, v.raw);
}
HWY_API void StoreU(const Vec128<uint16_t, 4> v, Simd<uint16_t, 4> /* tag */,
                    uint16_t* HWY_RESTRICT p) {
  vst1_u16(p, v.raw);
}
HWY_API void StoreU(const Vec128<uint32_t, 2> v, Simd<uint32_t, 2> /* tag */,
                    uint32_t* HWY_RESTRICT p) {
  vst1_u32(p, v.raw);
}
HWY_API void StoreU(const Vec128<uint64_t, 1> v, Simd<uint64_t, 1> /* tag */,
                    uint64_t* HWY_RESTRICT p) {
  vst1_u64(p, v.raw);
}
HWY_API void StoreU(const Vec128<int8_t, 8> v, Simd<int8_t, 8> /* tag */,
                    int8_t* HWY_RESTRICT p) {
  vst1_s8(p, v.raw);
}
HWY_API void StoreU(const Vec128<int16_t, 4> v, Simd<int16_t, 4> /* tag */,
                    int16_t* HWY_RESTRICT p) {
  vst1_s16(p, v.raw);
}
HWY_API void StoreU(const Vec128<int32_t, 2> v, Simd<int32_t, 2> /* tag */,
                    int32_t* HWY_RESTRICT p) {
  vst1_s32(p, v.raw);
}
HWY_API void StoreU(const Vec128<int64_t, 1> v, Simd<int64_t, 1> /* tag */,
                    int64_t* HWY_RESTRICT p) {
  vst1_s64(p, v.raw);
}
HWY_API void StoreU(const Vec128<float, 2> v, Simd<float, 2> /* tag */,
                    float* HWY_RESTRICT p) {
  vst1_f32(p, v.raw);
}
#if HWY_ARCH_ARM_A64
HWY_API void StoreU(const Vec128<double, 1> v, Simd<double, 1> /* tag */,
                    double* HWY_RESTRICT p) {
  vst1_f64(p, v.raw);
}
#endif

// ------------------------------ Store 32

HWY_API void StoreU(const Vec128<uint8_t, 4> v, Simd<uint8_t, 4>,
                    uint8_t* HWY_RESTRICT p) {
  uint32x2_t a = vreinterpret_u32_u8(v.raw);
  vst1_lane_u32(reinterpret_cast<uint32_t*>(p), a, 0);
}
HWY_API void StoreU(const Vec128<uint16_t, 2> v, Simd<uint16_t, 2>,
                    uint16_t* HWY_RESTRICT p) {
  uint32x2_t a = vreinterpret_u32_u16(v.raw);
  vst1_lane_u32(reinterpret_cast<uint32_t*>(p), a, 0);
}
HWY_API void StoreU(const Vec128<uint32_t, 1> v, Simd<uint32_t, 1>,
                    uint32_t* HWY_RESTRICT p) {
  vst1_lane_u32(p, v.raw, 0);
}
HWY_API void StoreU(const Vec128<int8_t, 4> v, Simd<int8_t, 4>,
                    int8_t* HWY_RESTRICT p) {
  int32x2_t a = vreinterpret_s32_s8(v.raw);
  vst1_lane_s32(reinterpret_cast<int32_t*>(p), a, 0);
}
HWY_API void StoreU(const Vec128<int16_t, 2> v, Simd<int16_t, 2>,
                    int16_t* HWY_RESTRICT p) {
  int32x2_t a = vreinterpret_s32_s16(v.raw);
  vst1_lane_s32(reinterpret_cast<int32_t*>(p), a, 0);
}
HWY_API void StoreU(const Vec128<int32_t, 1> v, Simd<int32_t, 1>,
                    int32_t* HWY_RESTRICT p) {
  vst1_lane_s32(p, v.raw, 0);
}
HWY_API void StoreU(const Vec128<float, 1> v, Simd<float, 1>,
                    float* HWY_RESTRICT p) {
  vst1_lane_f32(p, v.raw, 0);
}

// ------------------------------ Store 16

HWY_API void StoreU(const Vec128<uint8_t, 2> v, Simd<uint8_t, 2>,
                    uint8_t* HWY_RESTRICT p) {
  uint16x4_t a = vreinterpret_u16_u8(v.raw);
  vst1_lane_u16(reinterpret_cast<uint16_t*>(p), a, 0);
}
HWY_API void StoreU(const Vec128<uint16_t, 1> v, Simd<uint16_t, 1>,
                    uint16_t* HWY_RESTRICT p) {
  vst1_lane_u16(p, v.raw, 0);
}
HWY_API void StoreU(const Vec128<int8_t, 2> v, Simd<int8_t, 2>,
                    int8_t* HWY_RESTRICT p) {
  int16x4_t a = vreinterpret_s16_s8(v.raw);
  vst1_lane_s16(reinterpret_cast<int16_t*>(p), a, 0);
}
HWY_API void StoreU(const Vec128<int16_t, 1> v, Simd<int16_t, 1>,
                    int16_t* HWY_RESTRICT p) {
  vst1_lane_s16(p, v.raw, 0);
}

// ------------------------------ Store 8

HWY_API void StoreU(const Vec128<uint8_t, 1> v, Simd<uint8_t, 1>,
                    uint8_t* HWY_RESTRICT p) {
  vst1_lane_u8(p, v.raw, 0);
}
HWY_API void StoreU(const Vec128<int8_t, 1> v, Simd<int8_t, 1>,
                    int8_t* HWY_RESTRICT p) {
  vst1_lane_s8(p, v.raw, 0);
}

// float16_t uses the same Raw as uint16_t, so forward to that.
template <size_t N>
HWY_API void StoreU(Vec128<float16_t, N> v, Simd<float16_t, N> /* tag */,
                    float16_t* HWY_RESTRICT p) {
  const Simd<uint16_t, N> du16;
  const auto pu16 = reinterpret_cast<uint16_t*>(p);
  return StoreU(Vec128<uint16_t, N>(v.raw), du16, pu16);
}

// On ARM, Store is the same as StoreU.
template <typename T, size_t N>
HWY_API void Store(Vec128<T, N> v, Simd<T, N> d, T* HWY_RESTRICT aligned) {
  StoreU(v, d, aligned);
}

// ------------------------------ Non-temporal stores

// Same as aligned stores on non-x86.

template <typename T, size_t N>
HWY_API void Stream(const Vec128<T, N> v, Simd<T, N> d,
                    T* HWY_RESTRICT aligned) {
  Store(v, d, aligned);
}

// ================================================== CONVERT

// ------------------------------ Promotions (part w/ narrow lanes -> full)

// Unsigned: zero-extend to full vector.
HWY_API Vec128<uint16_t> PromoteTo(Full128<uint16_t> /* tag */,
                                   const Vec128<uint8_t, 8> v) {
  return Vec128<uint16_t>(vmovl_u8(v.raw));
}
HWY_API Vec128<uint32_t> PromoteTo(Full128<uint32_t> /* tag */,
                                   const Vec128<uint8_t, 4> v) {
  uint16x8_t a = vmovl_u8(v.raw);
  return Vec128<uint32_t>(vmovl_u16(vget_low_u16(a)));
}
HWY_API Vec128<uint32_t> PromoteTo(Full128<uint32_t> /* tag */,
                                   const Vec128<uint16_t, 4> v) {
  return Vec128<uint32_t>(vmovl_u16(v.raw));
}
HWY_API Vec128<uint64_t> PromoteTo(Full128<uint64_t> /* tag */,
                                   const Vec128<uint32_t, 2> v) {
  return Vec128<uint64_t>(vmovl_u32(v.raw));
}
HWY_API Vec128<int16_t> PromoteTo(Full128<int16_t> d,
                                  const Vec128<uint8_t, 8> v) {
  return BitCast(d, Vec128<uint16_t>(vmovl_u8(v.raw)));
}
HWY_API Vec128<int32_t> PromoteTo(Full128<int32_t> d,
                                  const Vec128<uint8_t, 4> v) {
  uint16x8_t a = vmovl_u8(v.raw);
  return BitCast(d, Vec128<uint32_t>(vmovl_u16(vget_low_u16(a))));
}
HWY_API Vec128<int32_t> PromoteTo(Full128<int32_t> d,
                                  const Vec128<uint16_t, 4> v) {
  return BitCast(d, Vec128<uint32_t>(vmovl_u16(v.raw)));
}

// Unsigned: zero-extend to half vector.
template <size_t N, HWY_IF_LE64(uint16_t, N)>
HWY_API Vec128<uint16_t, N> PromoteTo(Simd<uint16_t, N> /* tag */,
                                      const Vec128<uint8_t, N> v) {
  return Vec128<uint16_t, N>(vget_low_u16(vmovl_u8(v.raw)));
}
template <size_t N, HWY_IF_LE64(uint32_t, N)>
HWY_API Vec128<uint32_t, N> PromoteTo(Simd<uint32_t, N> /* tag */,
                                      const Vec128<uint8_t, N> v) {
  uint16x8_t a = vmovl_u8(v.raw);
  return Vec128<uint32_t, N>(vget_low_u32(vmovl_u16(vget_low_u16(a))));
}
template <size_t N>
HWY_API Vec128<uint32_t, N> PromoteTo(Simd<uint32_t, N> /* tag */,
                                      const Vec128<uint16_t, N> v) {
  return Vec128<uint32_t, N>(vget_low_u32(vmovl_u16(v.raw)));
}
template <size_t N, HWY_IF_LE64(uint64_t, N)>
HWY_API Vec128<uint64_t, N> PromoteTo(Simd<uint64_t, N> /* tag */,
                                      const Vec128<uint32_t, N> v) {
  return Vec128<uint64_t, N>(vget_low_u64(vmovl_u32(v.raw)));
}
template <size_t N, HWY_IF_LE64(int16_t, N)>
HWY_API Vec128<int16_t, N> PromoteTo(Simd<int16_t, N> d,
                                     const Vec128<uint8_t, N> v) {
  return BitCast(d, Vec128<uint16_t, N>(vget_low_u16(vmovl_u8(v.raw))));
}
template <size_t N, HWY_IF_LE64(int32_t, N)>
HWY_API Vec128<int32_t, N> PromoteTo(Simd<int32_t, N> /* tag */,
                                     const Vec128<uint8_t, N> v) {
  uint16x8_t a = vmovl_u8(v.raw);
  uint32x4_t b = vmovl_u16(vget_low_u16(a));
  return Vec128<int32_t, N>(vget_low_s32(vreinterpretq_s32_u32(b)));
}
template <size_t N, HWY_IF_LE64(int32_t, N)>
HWY_API Vec128<int32_t, N> PromoteTo(Simd<int32_t, N> /* tag */,
                                     const Vec128<uint16_t, N> v) {
  uint32x4_t a = vmovl_u16(v.raw);
  return Vec128<int32_t, N>(vget_low_s32(vreinterpretq_s32_u32(a)));
}

// Signed: replicate sign bit to full vector.
HWY_API Vec128<int16_t> PromoteTo(Full128<int16_t> /* tag */,
                                  const Vec128<int8_t, 8> v) {
  return Vec128<int16_t>(vmovl_s8(v.raw));
}
HWY_API Vec128<int32_t> PromoteTo(Full128<int32_t> /* tag */,
                                  const Vec128<int8_t, 4> v) {
  int16x8_t a = vmovl_s8(v.raw);
  return Vec128<int32_t>(vmovl_s16(vget_low_s16(a)));
}
HWY_API Vec128<int32_t> PromoteTo(Full128<int32_t> /* tag */,
                                  const Vec128<int16_t, 4> v) {
  return Vec128<int32_t>(vmovl_s16(v.raw));
}
HWY_API Vec128<int64_t> PromoteTo(Full128<int64_t> /* tag */,
                                  const Vec128<int32_t, 2> v) {
  return Vec128<int64_t>(vmovl_s32(v.raw));
}

// Signed: replicate sign bit to half vector.
template <size_t N>
HWY_API Vec128<int16_t, N> PromoteTo(Simd<int16_t, N> /* tag */,
                                     const Vec128<int8_t, N> v) {
  return Vec128<int16_t, N>(vget_low_s16(vmovl_s8(v.raw)));
}
template <size_t N>
HWY_API Vec128<int32_t, N> PromoteTo(Simd<int32_t, N> /* tag */,
                                     const Vec128<int8_t, N> v) {
  int16x8_t a = vmovl_s8(v.raw);
  int32x4_t b = vmovl_s16(vget_low_s16(a));
  return Vec128<int32_t, N>(vget_low_s32(b));
}
template <size_t N>
HWY_API Vec128<int32_t, N> PromoteTo(Simd<int32_t, N> /* tag */,
                                     const Vec128<int16_t, N> v) {
  return Vec128<int32_t, N>(vget_low_s32(vmovl_s16(v.raw)));
}
template <size_t N>
HWY_API Vec128<int64_t, N> PromoteTo(Simd<int64_t, N> /* tag */,
                                     const Vec128<int32_t, N> v) {
  return Vec128<int64_t, N>(vget_low_s64(vmovl_s32(v.raw)));
}

#if __ARM_FP & 2

HWY_API Vec128<float> PromoteTo(Full128<float> /* tag */,
                                const Vec128<float16_t, 4> v) {
  const float32x4_t f32 = vcvt_f32_f16(vreinterpret_f16_u16(v.raw));
  return Vec128<float>(f32);
}
template <size_t N>
HWY_API Vec128<float, N> PromoteTo(Simd<float, N> /* tag */,
                                   const Vec128<float16_t, N> v) {
  const float32x4_t f32 = vcvt_f32_f16(vreinterpret_f16_u16(v.raw));
  return Vec128<float, N>(vget_low_f32(f32));
}

#else

template <size_t N>
HWY_API Vec128<float, N> PromoteTo(Simd<float, N> /* tag */,
                                   const Vec128<float16_t, N> v) {
  const Simd<int32_t, N> di32;
  const Simd<uint32_t, N> du32;
  const Simd<float, N> df32;
  // Expand to u32 so we can shift.
  const auto bits16 = PromoteTo(du32, Vec128<uint16_t, N>{v.raw});
  const auto sign = ShiftRight<15>(bits16);
  const auto biased_exp = ShiftRight<10>(bits16) & Set(du32, 0x1F);
  const auto mantissa = bits16 & Set(du32, 0x3FF);
  const auto subnormal =
      BitCast(du32, ConvertTo(df32, BitCast(di32, mantissa)) *
                        Set(df32, 1.0f / 16384 / 1024));

  const auto biased_exp32 = biased_exp + Set(du32, 127 - 15);
  const auto mantissa32 = ShiftLeft<23 - 10>(mantissa);
  const auto normal = ShiftLeft<23>(biased_exp32) | mantissa32;
  const auto bits32 = IfThenElse(biased_exp == Zero(du32), subnormal, normal);
  return BitCast(df32, ShiftLeft<31>(sign) | bits32);
}

#endif

#if HWY_ARCH_ARM_A64

HWY_API Vec128<double> PromoteTo(Full128<double> /* tag */,
                                 const Vec128<float, 2> v) {
  return Vec128<double>(vcvt_f64_f32(v.raw));
}

HWY_API Vec128<double, 1> PromoteTo(Simd<double, 1> /* tag */,
                                    const Vec128<float, 1> v) {
  return Vec128<double, 1>(vget_low_f64(vcvt_f64_f32(v.raw)));
}

HWY_API Vec128<double> PromoteTo(Full128<double> /* tag */,
                                 const Vec128<int32_t, 2> v) {
  const int64x2_t i64 = vmovl_s32(v.raw);
  return Vec128<double>(vcvtq_f64_s64(i64));
}

HWY_API Vec128<double, 1> PromoteTo(Simd<double, 1> /* tag */,
                                    const Vec128<int32_t, 1> v) {
  const int64x1_t i64 = vget_low_s64(vmovl_s32(v.raw));
  return Vec128<double, 1>(vcvt_f64_s64(i64));
}

#endif

// ------------------------------ Demotions (full -> part w/ narrow lanes)

// From full vector to half or quarter
HWY_API Vec128<uint16_t, 4> DemoteTo(Simd<uint16_t, 4> /* tag */,
                                     const Vec128<int32_t> v) {
  return Vec128<uint16_t, 4>(vqmovun_s32(v.raw));
}
HWY_API Vec128<int16_t, 4> DemoteTo(Simd<int16_t, 4> /* tag */,
                                    const Vec128<int32_t> v) {
  return Vec128<int16_t, 4>(vqmovn_s32(v.raw));
}
HWY_API Vec128<uint8_t, 4> DemoteTo(Simd<uint8_t, 4> /* tag */,
                                    const Vec128<int32_t> v) {
  const uint16x4_t a = vqmovun_s32(v.raw);
  return Vec128<uint8_t, 4>(vqmovn_u16(vcombine_u16(a, a)));
}
HWY_API Vec128<uint8_t, 8> DemoteTo(Simd<uint8_t, 8> /* tag */,
                                    const Vec128<int16_t> v) {
  return Vec128<uint8_t, 8>(vqmovun_s16(v.raw));
}
HWY_API Vec128<int8_t, 4> DemoteTo(Simd<int8_t, 4> /* tag */,
                                   const Vec128<int32_t> v) {
  const int16x4_t a = vqmovn_s32(v.raw);
  return Vec128<int8_t, 4>(vqmovn_s16(vcombine_s16(a, a)));
}
HWY_API Vec128<int8_t, 8> DemoteTo(Simd<int8_t, 8> /* tag */,
                                   const Vec128<int16_t> v) {
  return Vec128<int8_t, 8>(vqmovn_s16(v.raw));
}

// From half vector to partial half
template <size_t N, HWY_IF_LE64(int32_t, N)>
HWY_API Vec128<uint16_t, N> DemoteTo(Simd<uint16_t, N> /* tag */,
                                     const Vec128<int32_t, N> v) {
  return Vec128<uint16_t, N>(vqmovun_s32(vcombine_s32(v.raw, v.raw)));
}
template <size_t N, HWY_IF_LE64(int32_t, N)>
HWY_API Vec128<int16_t, N> DemoteTo(Simd<int16_t, N> /* tag */,
                                    const Vec128<int32_t, N> v) {
  return Vec128<int16_t, N>(vqmovn_s32(vcombine_s32(v.raw, v.raw)));
}
template <size_t N, HWY_IF_LE64(int32_t, N)>
HWY_API Vec128<uint8_t, N> DemoteTo(Simd<uint8_t, N> /* tag */,
                                    const Vec128<int32_t, N> v) {
  const uint16x4_t a = vqmovun_s32(vcombine_s32(v.raw, v.raw));
  return Vec128<uint8_t, N>(vqmovn_u16(vcombine_u16(a, a)));
}
template <size_t N, HWY_IF_LE64(int16_t, N)>
HWY_API Vec128<uint8_t, N> DemoteTo(Simd<uint8_t, N> /* tag */,
                                    const Vec128<int16_t, N> v) {
  return Vec128<uint8_t, N>(vqmovun_s16(vcombine_s16(v.raw, v.raw)));
}
template <size_t N, HWY_IF_LE64(int32_t, N)>
HWY_API Vec128<int8_t, N> DemoteTo(Simd<int8_t, N> /* tag */,
                                   const Vec128<int32_t, N> v) {
  const int16x4_t a = vqmovn_s32(vcombine_s32(v.raw, v.raw));
  return Vec128<int8_t, N>(vqmovn_s16(vcombine_s16(a, a)));
}
template <size_t N, HWY_IF_LE64(int16_t, N)>
HWY_API Vec128<int8_t, N> DemoteTo(Simd<int8_t, N> /* tag */,
                                   const Vec128<int16_t, N> v) {
  return Vec128<int8_t, N>(vqmovn_s16(vcombine_s16(v.raw, v.raw)));
}

#if __ARM_FP & 2

HWY_API Vec128<float16_t, 4> DemoteTo(Simd<float16_t, 4> /* tag */,
                                      const Vec128<float> v) {
  return Vec128<float16_t, 4>{vreinterpret_u16_f16(vcvt_f16_f32(v.raw))};
}
template <size_t N>
HWY_API Vec128<float16_t, N> DemoteTo(Simd<float16_t, N> /* tag */,
                                      const Vec128<float, N> v) {
  const float16x4_t f16 = vcvt_f16_f32(vcombine_f32(v.raw, v.raw));
  return Vec128<float16_t, N>(vreinterpret_u16_f16(f16));
}

#else

template <size_t N>
HWY_API Vec128<float16_t, N> DemoteTo(Simd<float16_t, N> /* tag */,
                                      const Vec128<float, N> v) {
  const Simd<int32_t, N> di;
  const Simd<uint32_t, N> du;
  const Simd<uint16_t, N> du16;
  const auto bits32 = BitCast(du, v);
  const auto sign = ShiftRight<31>(bits32);
  const auto biased_exp32 = ShiftRight<23>(bits32) & Set(du, 0xFF);
  const auto mantissa32 = bits32 & Set(du, 0x7FFFFF);

  const auto k15 = Set(di, 15);
  const auto exp = Min(BitCast(di, biased_exp32) - Set(di, 127), k15);
  const auto is_tiny = exp < Set(di, -24);

  const auto is_subnormal = exp < Set(di, -14);
  const auto biased_exp16 =
      BitCast(du, IfThenZeroElse(is_subnormal, exp + k15));
  const auto sub_exp = BitCast(du, Set(di, -14) - exp);  // [1, 11)
  const auto sub_m = (Set(du, 1) << (Set(du, 10) - sub_exp)) +
                     (mantissa32 >> (Set(du, 13) + sub_exp));
  const auto mantissa16 = IfThenElse(RebindMask(du, is_subnormal), sub_m,
                                     ShiftRight<13>(mantissa32));  // <1024

  const auto sign16 = ShiftLeft<15>(sign);
  const auto normal16 = sign16 | ShiftLeft<10>(biased_exp16) | mantissa16;
  const auto bits16 = IfThenZeroElse(is_tiny, BitCast(di, normal16));
  return Vec128<float16_t, N>(DemoteTo(du16, bits16).raw);
}

#endif
#if HWY_ARCH_ARM_A64

HWY_API Vec128<float, 2> DemoteTo(Simd<float, 2> /* tag */,
                                  const Vec128<double> v) {
  return Vec128<float, 2>(vcvt_f32_f64(v.raw));
}
HWY_API Vec128<float, 1> DemoteTo(Simd<float, 1> /* tag */,
                                  const Vec128<double, 1> v) {
  return Vec128<float, 1>(vcvt_f32_f64(vcombine_f64(v.raw, v.raw)));
}

HWY_API Vec128<int32_t, 2> DemoteTo(Simd<int32_t, 2> /* tag */,
                                    const Vec128<double> v) {
  const int64x2_t i64 = vcvtq_s64_f64(v.raw);
  return Vec128<int32_t, 2>(vqmovn_s64(i64));
}
HWY_API Vec128<int32_t, 1> DemoteTo(Simd<int32_t, 1> /* tag */,
                                    const Vec128<double, 1> v) {
  const int64x1_t i64 = vcvt_s64_f64(v.raw);
  // There is no i64x1 -> i32x1 narrow, so expand to int64x2_t first.
  const int64x2_t i64x2 = vcombine_s64(i64, i64);
  return Vec128<int32_t, 1>(vqmovn_s64(i64x2));
}

#endif

HWY_API Vec128<uint8_t, 4> U8FromU32(const Vec128<uint32_t> v) {
  const uint8x16_t org_v = detail::BitCastToByte(v).raw;
  const uint8x16_t w = vuzp1q_u8(org_v, org_v);
  return Vec128<uint8_t, 4>(vget_low_u8(vuzp1q_u8(w, w)));
}
template <size_t N, HWY_IF_LE64(uint32_t, N)>
HWY_API Vec128<uint8_t, N> U8FromU32(const Vec128<uint32_t, N> v) {
  const uint8x8_t org_v = detail::BitCastToByte(v).raw;
  const uint8x8_t w = vuzp1_u8(org_v, org_v);
  return Vec128<uint8_t, N>(vuzp1_u8(w, w));
}

// In the following DemoteTo functions, |b| is purposely undefined.
// The value a needs to be extended to 128 bits so that vqmovn can be
// used and |b| is undefined so that no extra overhead is introduced.
HWY_DIAGNOSTICS(push)
HWY_DIAGNOSTICS_OFF(disable : 4701, ignored "-Wuninitialized")

template <size_t N>
HWY_API Vec128<uint8_t, N> DemoteTo(Simd<uint8_t, N> /* tag */,
                                    const Vec128<int32_t> v) {
  Vec128<uint16_t, N> a = DemoteTo(Simd<uint16_t, N>(), v);
  Vec128<uint16_t, N> b;
  uint16x8_t c = vcombine_u16(a.raw, b.raw);
  return Vec128<uint8_t, N>(vqmovn_u16(c));
}

template <size_t N>
HWY_API Vec128<int8_t, N> DemoteTo(Simd<int8_t, N> /* tag */,
                                   const Vec128<int32_t> v) {
  Vec128<int16_t, N> a = DemoteTo(Simd<int16_t, N>(), v);
  Vec128<int16_t, N> b;
  int16x8_t c = vcombine_s16(a.raw, b.raw);
  return Vec128<int8_t, N>(vqmovn_s16(c));
}

HWY_DIAGNOSTICS(pop)

// ------------------------------ Convert integer <=> floating-point

HWY_API Vec128<float> ConvertTo(Full128<float> /* tag */,
                                const Vec128<int32_t> v) {
  return Vec128<float>(vcvtq_f32_s32(v.raw));
}
template <size_t N, HWY_IF_LE64(int32_t, N)>
HWY_API Vec128<float, N> ConvertTo(Simd<float, N> /* tag */,
                                   const Vec128<int32_t, N> v) {
  return Vec128<float, N>(vcvt_f32_s32(v.raw));
}

// Truncates (rounds toward zero).
HWY_API Vec128<int32_t> ConvertTo(Full128<int32_t> /* tag */,
                                  const Vec128<float> v) {
  return Vec128<int32_t>(vcvtq_s32_f32(v.raw));
}
template <size_t N, HWY_IF_LE64(float, N)>
HWY_API Vec128<int32_t, N> ConvertTo(Simd<int32_t, N> /* tag */,
                                     const Vec128<float, N> v) {
  return Vec128<int32_t, N>(vcvt_s32_f32(v.raw));
}

#if HWY_ARCH_ARM_A64

HWY_API Vec128<double> ConvertTo(Full128<double> /* tag */,
                                 const Vec128<int64_t> v) {
  return Vec128<double>(vcvtq_f64_s64(v.raw));
}
HWY_API Vec128<double, 1> ConvertTo(Simd<double, 1> /* tag */,
                                    const Vec128<int64_t, 1> v) {
  return Vec128<double, 1>(vcvt_f64_s64(v.raw));
}

// Truncates (rounds toward zero).
HWY_API Vec128<int64_t> ConvertTo(Full128<int64_t> /* tag */,
                                  const Vec128<double> v) {
  return Vec128<int64_t>(vcvtq_s64_f64(v.raw));
}
HWY_API Vec128<int64_t, 1> ConvertTo(Simd<int64_t, 1> /* tag */,
                                     const Vec128<double, 1> v) {
  return Vec128<int64_t, 1>(vcvt_s64_f64(v.raw));
}

#endif

// ------------------------------ Round (IfThenElse, mask, logical)

#if HWY_ARCH_ARM_A64
// Toward nearest integer
HWY_NEON_DEF_FUNCTION_ALL_FLOATS(Round, vrndn, _, 1)

// Toward zero, aka truncate
HWY_NEON_DEF_FUNCTION_ALL_FLOATS(Trunc, vrnd, _, 1)

// Toward +infinity, aka ceiling
HWY_NEON_DEF_FUNCTION_ALL_FLOATS(Ceil, vrndp, _, 1)

// Toward -infinity, aka floor
HWY_NEON_DEF_FUNCTION_ALL_FLOATS(Floor, vrndm, _, 1)
#else

// ------------------------------ Trunc

// ARMv7 only supports truncation to integer. We can either convert back to
// float (3 floating-point and 2 logic operations) or manipulate the binary32
// representation, clearing the lowest 23-exp mantissa bits. This requires 9
// integer operations and 3 constants, which is likely more expensive.

namespace detail {

// The original value is already the desired result if NaN or the magnitude is
// large (i.e. the value is already an integer).
template <size_t N>
HWY_INLINE Mask128<float, N> UseInt(const Vec128<float, N> v) {
  return Abs(v) < Set(Simd<float, N>(), MantissaEnd<float>());
}

}  // namespace detail

template <size_t N>
HWY_API Vec128<float, N> Trunc(const Vec128<float, N> v) {
  const Simd<float, N> df;
  const RebindToSigned<decltype(df)> di;

  const auto integer = ConvertTo(di, v);  // round toward 0
  const auto int_f = ConvertTo(df, integer);

  return IfThenElse(detail::UseInt(v), int_f, v);
}

template <size_t N>
HWY_API Vec128<float, N> Round(const Vec128<float, N> v) {
  const Simd<float, N> df;

  // ARMv7 also lacks a native NearestInt, but we can instead rely on rounding
  // (we assume the current mode is nearest-even) after addition with a large
  // value such that no mantissa bits remain. We may need a compiler flag for
  // precise floating-point to prevent this from being "optimized" out.
  const auto max = Set(df, MantissaEnd<float>());
  const auto large = CopySignToAbs(max, v);
  const auto added = large + v;
  const auto rounded = added - large;

  // Keep original if NaN or the magnitude is large (already an int).
  return IfThenElse(Abs(v) < max, rounded, v);
}

template <size_t N>
HWY_API Vec128<float, N> Ceil(const Vec128<float, N> v) {
  const Simd<float, N> df;
  const RebindToSigned<decltype(df)> di;

  const auto integer = ConvertTo(di, v);  // round toward 0
  const auto int_f = ConvertTo(df, integer);

  // Truncating a positive non-integer ends up smaller; if so, add 1.
  const auto neg1 = ConvertTo(df, VecFromMask(di, RebindMask(di, int_f < v)));

  return IfThenElse(detail::UseInt(v), int_f - neg1, v);
}

template <size_t N>
HWY_API Vec128<float, N> Floor(const Vec128<float, N> v) {
  const Simd<float, N> df;
  const Simd<int32_t, N> di;

  const auto integer = ConvertTo(di, v);  // round toward 0
  const auto int_f = ConvertTo(df, integer);

  // Truncating a negative non-integer ends up larger; if so, subtract 1.
  const auto neg1 = ConvertTo(df, VecFromMask(di, RebindMask(di, int_f > v)));

  return IfThenElse(detail::UseInt(v), int_f + neg1, v);
}

#endif

// ------------------------------ NearestInt (Round)

#if HWY_ARCH_ARM_A64

HWY_API Vec128<int32_t> NearestInt(const Vec128<float> v) {
  return Vec128<int32_t>(vcvtnq_s32_f32(v.raw));
}
template <size_t N, HWY_IF_LE64(float, N)>
HWY_API Vec128<int32_t, N> NearestInt(const Vec128<float, N> v) {
  return Vec128<int32_t, N>(vcvtn_s32_f32(v.raw));
}

#else

template <size_t N>
HWY_API Vec128<int32_t, N> NearestInt(const Vec128<float, N> v) {
  const Simd<int32_t, N> di;
  return ConvertTo(di, Round(v));
}

#endif

// ================================================== SWIZZLE

// ------------------------------ LowerHalf

// <= 64 bit: just return different type
template <typename T, size_t N, HWY_IF_LE64(uint8_t, N)>
HWY_API Vec128<T, N / 2> LowerHalf(const Vec128<T, N> v) {
  return Vec128<T, N / 2>(v.raw);
}

HWY_API Vec128<uint8_t, 8> LowerHalf(const Vec128<uint8_t> v) {
  return Vec128<uint8_t, 8>(vget_low_u8(v.raw));
}
HWY_API Vec128<uint16_t, 4> LowerHalf(const Vec128<uint16_t> v) {
  return Vec128<uint16_t, 4>(vget_low_u16(v.raw));
}
HWY_API Vec128<uint32_t, 2> LowerHalf(const Vec128<uint32_t> v) {
  return Vec128<uint32_t, 2>(vget_low_u32(v.raw));
}
HWY_API Vec128<uint64_t, 1> LowerHalf(const Vec128<uint64_t> v) {
  return Vec128<uint64_t, 1>(vget_low_u64(v.raw));
}
HWY_API Vec128<int8_t, 8> LowerHalf(const Vec128<int8_t> v) {
  return Vec128<int8_t, 8>(vget_low_s8(v.raw));
}
HWY_API Vec128<int16_t, 4> LowerHalf(const Vec128<int16_t> v) {
  return Vec128<int16_t, 4>(vget_low_s16(v.raw));
}
HWY_API Vec128<int32_t, 2> LowerHalf(const Vec128<int32_t> v) {
  return Vec128<int32_t, 2>(vget_low_s32(v.raw));
}
HWY_API Vec128<int64_t, 1> LowerHalf(const Vec128<int64_t> v) {
  return Vec128<int64_t, 1>(vget_low_s64(v.raw));
}
HWY_API Vec128<float, 2> LowerHalf(const Vec128<float> v) {
  return Vec128<float, 2>(vget_low_f32(v.raw));
}
#if HWY_ARCH_ARM_A64
HWY_API Vec128<double, 1> LowerHalf(const Vec128<double> v) {
  return Vec128<double, 1>(vget_low_f64(v.raw));
}
#endif

template <typename T, size_t N>
HWY_API Vec128<T, N / 2> LowerHalf(Simd<T, N / 2> /* tag */, Vec128<T, N> v) {
  return LowerHalf(v);
}

// ------------------------------ CombineShiftRightBytes

// 128-bit
template <int kBytes, typename T, class V128 = Vec128<T>>
HWY_API V128 CombineShiftRightBytes(Full128<T> d, V128 hi, V128 lo) {
  static_assert(0 < kBytes && kBytes < 16, "kBytes must be in [1, 15]");
  const Repartition<uint8_t, decltype(d)> d8;
  uint8x16_t v8 = vextq_u8(BitCast(d8, lo).raw, BitCast(d8, hi).raw, kBytes);
  return BitCast(d, Vec128<uint8_t>(v8));
}

// 64-bit
template <int kBytes, typename T, class V64 = Vec128<T, 8 / sizeof(T)>>
HWY_API V64 CombineShiftRightBytes(Simd<T, 8 / sizeof(T)> d, V64 hi, V64 lo) {
  static_assert(0 < kBytes && kBytes < 8, "kBytes must be in [1, 7]");
  const Repartition<uint8_t, decltype(d)> d8;
  uint8x8_t v8 = vext_u8(BitCast(d8, lo).raw, BitCast(d8, hi).raw, kBytes);
  return BitCast(d, VFromD<decltype(d8)>(v8));
}

// <= 32-bit defined after ShiftLeftBytes.

// ------------------------------ Shift vector by constant #bytes

namespace detail {

// Partially specialize because kBytes = 0 and >= size are compile errors;
// callers replace the latter with 0xFF for easier specialization.
template <int kBytes>
struct ShiftLeftBytesT {
  // Full
  template <class T>
  HWY_INLINE Vec128<T> operator()(const Vec128<T> v) {
    const Full128<T> d;
    return CombineShiftRightBytes<16 - kBytes>(d, v, Zero(d));
  }

  // Partial
  template <class T, size_t N, HWY_IF_LE64(T, N)>
  HWY_INLINE Vec128<T, N> operator()(const Vec128<T, N> v) {
    // Expand to 64-bit so we only use the native EXT instruction.
    const Simd<T, 8 / sizeof(T)> d64;
    const auto zero64 = Zero(d64);
    const decltype(zero64) v64(v.raw);
    return Vec128<T, N>(
        CombineShiftRightBytes<8 - kBytes>(d64, v64, zero64).raw);
  }
};
template <>
struct ShiftLeftBytesT<0> {
  template <class T, size_t N>
  HWY_INLINE Vec128<T, N> operator()(const Vec128<T, N> v) {
    return v;
  }
};
template <>
struct ShiftLeftBytesT<0xFF> {
  template <class T, size_t N>
  HWY_INLINE Vec128<T, N> operator()(const Vec128<T, N> /* v */) {
    return Zero(Simd<T, N>());
  }
};

template <int kBytes>
struct ShiftRightBytesT {
  template <class T, size_t N>
  HWY_INLINE Vec128<T, N> operator()(Vec128<T, N> v) {
    const Simd<T, N> d;
    // For < 64-bit vectors, zero undefined lanes so we shift in zeros.
    if (N * sizeof(T) < 8) {
      constexpr size_t kReg = N * sizeof(T) == 16 ? 16 : 8;
      const Simd<T, kReg / sizeof(T)> dreg;
      v = Vec128<T, N>(
          IfThenElseZero(FirstN(dreg, N), VFromD<decltype(dreg)>(v.raw)).raw);
    }
    return CombineShiftRightBytes<kBytes>(d, Zero(d), v);
  }
};
template <>
struct ShiftRightBytesT<0> {
  template <class T, size_t N>
  HWY_INLINE Vec128<T, N> operator()(const Vec128<T, N> v) {
    return v;
  }
};
template <>
struct ShiftRightBytesT<0xFF> {
  template <class T, size_t N>
  HWY_INLINE Vec128<T, N> operator()(const Vec128<T, N> /* v */) {
    return Zero(Simd<T, N>());
  }
};

}  // namespace detail

template <int kBytes, typename T, size_t N>
HWY_API Vec128<T, N> ShiftLeftBytes(Simd<T, N> /* tag */, Vec128<T, N> v) {
  return detail::ShiftLeftBytesT < kBytes >= N * sizeof(T) ? 0xFF
                                                           : kBytes > ()(v);
}

template <int kBytes, typename T, size_t N>
HWY_API Vec128<T, N> ShiftLeftBytes(const Vec128<T, N> v) {
  return ShiftLeftBytes<kBytes>(Simd<T, N>(), v);
}

template <int kLanes, typename T, size_t N>
HWY_API Vec128<T, N> ShiftLeftLanes(Simd<T, N> d, const Vec128<T, N> v) {
  const Repartition<uint8_t, decltype(d)> d8;
  return BitCast(d, ShiftLeftBytes<kLanes * sizeof(T)>(BitCast(d8, v)));
}

template <int kLanes, typename T, size_t N>
HWY_API Vec128<T, N> ShiftLeftLanes(const Vec128<T, N> v) {
  return ShiftLeftLanes<kLanes>(Simd<T, N>(), v);
}

// 0x01..0F, kBytes = 1 => 0x0001..0E
template <int kBytes, typename T, size_t N>
HWY_API Vec128<T, N> ShiftRightBytes(Simd<T, N> /* tag */, Vec128<T, N> v) {
  return detail::ShiftRightBytesT < kBytes >= N * sizeof(T) ? 0xFF
                                                            : kBytes > ()(v);
}

template <int kLanes, typename T, size_t N>
HWY_API Vec128<T, N> ShiftRightLanes(Simd<T, N> d, const Vec128<T, N> v) {
  const Repartition<uint8_t, decltype(d)> d8;
  return BitCast(d, ShiftRightBytes<kLanes * sizeof(T)>(BitCast(d8, v)));
}

// Calls ShiftLeftBytes
template <int kBytes, typename T, size_t N, HWY_IF_LE32(T, N)>
HWY_API Vec128<T, N> CombineShiftRightBytes(Simd<T, N> d, Vec128<T, N> hi,
                                            Vec128<T, N> lo) {
  constexpr size_t kSize = N * sizeof(T);
  static_assert(0 < kBytes && kBytes < kSize, "kBytes invalid");
  const Repartition<uint8_t, decltype(d)> d8;
  const Simd<uint8_t, 8> d_full8;
  const Repartition<T, decltype(d_full8)> d_full;
  using V64 = VFromD<decltype(d_full8)>;
  const V64 hi64(BitCast(d8, hi).raw);
  // Move into most-significant bytes
  const V64 lo64 = ShiftLeftBytes<8 - kSize>(V64(BitCast(d8, lo).raw));
  const V64 r = CombineShiftRightBytes<8 - kSize + kBytes>(d_full8, hi64, lo64);
  // After casting to full 64-bit vector of correct type, shrink to 32-bit
  return Vec128<T, N>(BitCast(d_full, r).raw);
}

// ------------------------------ UpperHalf (ShiftRightBytes)

// Full input
HWY_API Vec128<uint8_t, 8> UpperHalf(Simd<uint8_t, 8> /* tag */,
                                     const Vec128<uint8_t> v) {
  return Vec128<uint8_t, 8>(vget_high_u8(v.raw));
}
HWY_API Vec128<uint16_t, 4> UpperHalf(Simd<uint16_t, 4> /* tag */,
                                      const Vec128<uint16_t> v) {
  return Vec128<uint16_t, 4>(vget_high_u16(v.raw));
}
HWY_API Vec128<uint32_t, 2> UpperHalf(Simd<uint32_t, 2> /* tag */,
                                      const Vec128<uint32_t> v) {
  return Vec128<uint32_t, 2>(vget_high_u32(v.raw));
}
HWY_API Vec128<uint64_t, 1> UpperHalf(Simd<uint64_t, 1> /* tag */,
                                      const Vec128<uint64_t> v) {
  return Vec128<uint64_t, 1>(vget_high_u64(v.raw));
}
HWY_API Vec128<int8_t, 8> UpperHalf(Simd<int8_t, 8> /* tag */,
                                    const Vec128<int8_t> v) {
  return Vec128<int8_t, 8>(vget_high_s8(v.raw));
}
HWY_API Vec128<int16_t, 4> UpperHalf(Simd<int16_t, 4> /* tag */,
                                     const Vec128<int16_t> v) {
  return Vec128<int16_t, 4>(vget_high_s16(v.raw));
}
HWY_API Vec128<int32_t, 2> UpperHalf(Simd<int32_t, 2> /* tag */,
                                     const Vec128<int32_t> v) {
  return Vec128<int32_t, 2>(vget_high_s32(v.raw));
}
HWY_API Vec128<int64_t, 1> UpperHalf(Simd<int64_t, 1> /* tag */,
                                     const Vec128<int64_t> v) {
  return Vec128<int64_t, 1>(vget_high_s64(v.raw));
}
HWY_API Vec128<float, 2> UpperHalf(Simd<float, 2> /* tag */,
                                   const Vec128<float> v) {
  return Vec128<float, 2>(vget_high_f32(v.raw));
}
#if HWY_ARCH_ARM_A64
HWY_API Vec128<double, 1> UpperHalf(Simd<double, 1> /* tag */,
                                    const Vec128<double> v) {
  return Vec128<double, 1>(vget_high_f64(v.raw));
}
#endif

// Partial
template <typename T, size_t N, HWY_IF_LE64(T, N)>
HWY_API Vec128<T, (N + 1) / 2> UpperHalf(Half<Simd<T, N>> /* tag */,
                                         Vec128<T, N> v) {
  const Simd<T, N> d;
  const auto vu = BitCast(RebindToUnsigned<decltype(d)>(), v);
  const auto upper = BitCast(d, ShiftRightBytes<N * sizeof(T) / 2>(vu));
  return Vec128<T, (N + 1) / 2>(upper.raw);
}

// ------------------------------ Broadcast/splat any lane

#if HWY_ARCH_ARM_A64
// Unsigned
template <int kLane>
HWY_API Vec128<uint16_t> Broadcast(const Vec128<uint16_t> v) {
  static_assert(0 <= kLane && kLane < 8, "Invalid lane");
  return Vec128<uint16_t>(vdupq_laneq_u16(v.raw, kLane));
}
template <int kLane, size_t N, HWY_IF_LE64(uint16_t, N)>
HWY_API Vec128<uint16_t, N> Broadcast(const Vec128<uint16_t, N> v) {
  static_assert(0 <= kLane && kLane < N, "Invalid lane");
  return Vec128<uint16_t, N>(vdup_lane_u16(v.raw, kLane));
}
template <int kLane>
HWY_API Vec128<uint32_t> Broadcast(const Vec128<uint32_t> v) {
  static_assert(0 <= kLane && kLane < 4, "Invalid lane");
  return Vec128<uint32_t>(vdupq_laneq_u32(v.raw, kLane));
}
template <int kLane, size_t N, HWY_IF_LE64(uint32_t, N)>
HWY_API Vec128<uint32_t, N> Broadcast(const Vec128<uint32_t, N> v) {
  static_assert(0 <= kLane && kLane < N, "Invalid lane");
  return Vec128<uint32_t, N>(vdup_lane_u32(v.raw, kLane));
}
template <int kLane>
HWY_API Vec128<uint64_t> Broadcast(const Vec128<uint64_t> v) {
  static_assert(0 <= kLane && kLane < 2, "Invalid lane");
  return Vec128<uint64_t>(vdupq_laneq_u64(v.raw, kLane));
}
// Vec128<uint64_t, 1> is defined below.

// Signed
template <int kLane>
HWY_API Vec128<int16_t> Broadcast(const Vec128<int16_t> v) {
  static_assert(0 <= kLane && kLane < 8, "Invalid lane");
  return Vec128<int16_t>(vdupq_laneq_s16(v.raw, kLane));
}
template <int kLane, size_t N, HWY_IF_LE64(int16_t, N)>
HWY_API Vec128<int16_t, N> Broadcast(const Vec128<int16_t, N> v) {
  static_assert(0 <= kLane && kLane < N, "Invalid lane");
  return Vec128<int16_t, N>(vdup_lane_s16(v.raw, kLane));
}
template <int kLane>
HWY_API Vec128<int32_t> Broadcast(const Vec128<int32_t> v) {
  static_assert(0 <= kLane && kLane < 4, "Invalid lane");
  return Vec128<int32_t>(vdupq_laneq_s32(v.raw, kLane));
}
template <int kLane, size_t N, HWY_IF_LE64(int32_t, N)>
HWY_API Vec128<int32_t, N> Broadcast(const Vec128<int32_t, N> v) {
  static_assert(0 <= kLane && kLane < N, "Invalid lane");
  return Vec128<int32_t, N>(vdup_lane_s32(v.raw, kLane));
}
template <int kLane>
HWY_API Vec128<int64_t> Broadcast(const Vec128<int64_t> v) {
  static_assert(0 <= kLane && kLane < 2, "Invalid lane");
  return Vec128<int64_t>(vdupq_laneq_s64(v.raw, kLane));
}
// Vec128<int64_t, 1> is defined below.

// Float
template <int kLane>
HWY_API Vec128<float> Broadcast(const Vec128<float> v) {
  static_assert(0 <= kLane && kLane < 4, "Invalid lane");
  return Vec128<float>(vdupq_laneq_f32(v.raw, kLane));
}
template <int kLane, size_t N, HWY_IF_LE64(float, N)>
HWY_API Vec128<float, N> Broadcast(const Vec128<float, N> v) {
  static_assert(0 <= kLane && kLane < N, "Invalid lane");
  return Vec128<float, N>(vdup_lane_f32(v.raw, kLane));
}
template <int kLane>
HWY_API Vec128<double> Broadcast(const Vec128<double> v) {
  static_assert(0 <= kLane && kLane < 2, "Invalid lane");
  return Vec128<double>(vdupq_laneq_f64(v.raw, kLane));
}
template <int kLane>
HWY_API Vec128<double, 1> Broadcast(const Vec128<double, 1> v) {
  static_assert(0 <= kLane && kLane < 1, "Invalid lane");
  return v;
}

#else
// No vdupq_laneq_* on armv7: use vgetq_lane_* + vdupq_n_*.

// Unsigned
template <int kLane>
HWY_API Vec128<uint16_t> Broadcast(const Vec128<uint16_t> v) {
  static_assert(0 <= kLane && kLane < 8, "Invalid lane");
  return Vec128<uint16_t>(vdupq_n_u16(vgetq_lane_u16(v.raw, kLane)));
}
template <int kLane, size_t N, HWY_IF_LE64(uint16_t, N)>
HWY_API Vec128<uint16_t, N> Broadcast(const Vec128<uint16_t, N> v) {
  static_assert(0 <= kLane && kLane < N, "Invalid lane");
  return Vec128<uint16_t, N>(vdup_lane_u16(v.raw, kLane));
}
template <int kLane>
HWY_API Vec128<uint32_t> Broadcast(const Vec128<uint32_t> v) {
  static_assert(0 <= kLane && kLane < 4, "Invalid lane");
  return Vec128<uint32_t>(vdupq_n_u32(vgetq_lane_u32(v.raw, kLane)));
}
template <int kLane, size_t N, HWY_IF_LE64(uint32_t, N)>
HWY_API Vec128<uint32_t, N> Broadcast(const Vec128<uint32_t, N> v) {
  static_assert(0 <= kLane && kLane < N, "Invalid lane");
  return Vec128<uint32_t, N>(vdup_lane_u32(v.raw, kLane));
}
template <int kLane>
HWY_API Vec128<uint64_t> Broadcast(const Vec128<uint64_t> v) {
  static_assert(0 <= kLane && kLane < 2, "Invalid lane");
  return Vec128<uint64_t>(vdupq_n_u64(vgetq_lane_u64(v.raw, kLane)));
}
// Vec128<uint64_t, 1> is defined below.

// Signed
template <int kLane>
HWY_API Vec128<int16_t> Broadcast(const Vec128<int16_t> v) {
  static_assert(0 <= kLane && kLane < 8, "Invalid lane");
  return Vec128<int16_t>(vdupq_n_s16(vgetq_lane_s16(v.raw, kLane)));
}
template <int kLane, size_t N, HWY_IF_LE64(int16_t, N)>
HWY_API Vec128<int16_t, N> Broadcast(const Vec128<int16_t, N> v) {
  static_assert(0 <= kLane && kLane < N, "Invalid lane");
  return Vec128<int16_t, N>(vdup_lane_s16(v.raw, kLane));
}
template <int kLane>
HWY_API Vec128<int32_t> Broadcast(const Vec128<int32_t> v) {
  static_assert(0 <= kLane && kLane < 4, "Invalid lane");
  return Vec128<int32_t>(vdupq_n_s32(vgetq_lane_s32(v.raw, kLane)));
}
template <int kLane, size_t N, HWY_IF_LE64(int32_t, N)>
HWY_API Vec128<int32_t, N> Broadcast(const Vec128<int32_t, N> v) {
  static_assert(0 <= kLane && kLane < N, "Invalid lane");
  return Vec128<int32_t, N>(vdup_lane_s32(v.raw, kLane));
}
template <int kLane>
HWY_API Vec128<int64_t> Broadcast(const Vec128<int64_t> v) {
  static_assert(0 <= kLane && kLane < 2, "Invalid lane");
  return Vec128<int64_t>(vdupq_n_s64(vgetq_lane_s64(v.raw, kLane)));
}
// Vec128<int64_t, 1> is defined below.

// Float
template <int kLane>
HWY_API Vec128<float> Broadcast(const Vec128<float> v) {
  static_assert(0 <= kLane && kLane < 4, "Invalid lane");
  return Vec128<float>(vdupq_n_f32(vgetq_lane_f32(v.raw, kLane)));
}
template <int kLane, size_t N, HWY_IF_LE64(float, N)>
HWY_API Vec128<float, N> Broadcast(const Vec128<float, N> v) {
  static_assert(0 <= kLane && kLane < N, "Invalid lane");
  return Vec128<float, N>(vdup_lane_f32(v.raw, kLane));
}

#endif

template <int kLane>
HWY_API Vec128<uint64_t, 1> Broadcast(const Vec128<uint64_t, 1> v) {
  static_assert(0 <= kLane && kLane < 1, "Invalid lane");
  return v;
}
template <int kLane>
HWY_API Vec128<int64_t, 1> Broadcast(const Vec128<int64_t, 1> v) {
  static_assert(0 <= kLane && kLane < 1, "Invalid lane");
  return v;
}

// ------------------------------ TableLookupLanes

// Returned by SetTableIndices for use by TableLookupLanes.
template <typename T, size_t N>
struct Indices128 {
  typename detail::Raw128<T, N>::type raw;
};

template <typename T, size_t N, HWY_IF_LE128(T, N)>
HWY_API Indices128<T, N> SetTableIndices(Simd<T, N> d, const int32_t* idx) {
#if HWY_IS_DEBUG_BUILD
  for (size_t i = 0; i < N; ++i) {
    HWY_DASSERT(0 <= idx[i] && idx[i] < static_cast<int32_t>(N));
  }
#endif

  const Repartition<uint8_t, decltype(d)> d8;
  alignas(16) uint8_t control[16] = {0};
  for (size_t idx_lane = 0; idx_lane < N; ++idx_lane) {
    for (size_t idx_byte = 0; idx_byte < sizeof(T); ++idx_byte) {
      control[idx_lane * sizeof(T) + idx_byte] = static_cast<uint8_t>(
          static_cast<size_t>(idx[idx_lane]) * sizeof(T) + idx_byte);
    }
  }
  return Indices128<T, N>{BitCast(d, Load(d8, control)).raw};
}

template <size_t N>
HWY_API Vec128<uint32_t, N> TableLookupLanes(
    const Vec128<uint32_t, N> v, const Indices128<uint32_t, N> idx) {
  return TableLookupBytes(v, Vec128<uint32_t, N>{idx.raw});
}
template <size_t N>
HWY_API Vec128<int32_t, N> TableLookupLanes(const Vec128<int32_t, N> v,
                                            const Indices128<int32_t, N> idx) {
  return TableLookupBytes(v, Vec128<int32_t, N>{idx.raw});
}
template <size_t N>
HWY_API Vec128<float, N> TableLookupLanes(const Vec128<float, N> v,
                                          const Indices128<float, N> idx) {
  const Simd<int32_t, N> di;
  const auto idx_i = BitCast(di, Vec128<float, N>{idx.raw});
  return BitCast(Simd<float, N>(), TableLookupBytes(BitCast(di, v), idx_i));
}

// ------------------------------ Other shuffles (TableLookupBytes)

// Notation: let Vec128<int32_t> have lanes 3,2,1,0 (0 is least-significant).
// Shuffle0321 rotates one lane to the right (the previous least-significant
// lane is now most-significant). These could also be implemented via
// CombineShiftRightBytes but the shuffle_abcd notation is more convenient.

// Swap 64-bit halves
template <typename T>
HWY_API Vec128<T> Shuffle1032(const Vec128<T> v) {
  return CombineShiftRightBytes<8>(Full128<T>(), v, v);
}
template <typename T>
HWY_API Vec128<T> Shuffle01(const Vec128<T> v) {
  return CombineShiftRightBytes<8>(Full128<T>(), v, v);
}

// Rotate right 32 bits
template <typename T>
HWY_API Vec128<T> Shuffle0321(const Vec128<T> v) {
  return CombineShiftRightBytes<4>(Full128<T>(), v, v);
}

// Rotate left 32 bits
template <typename T>
HWY_API Vec128<T> Shuffle2103(const Vec128<T> v) {
  return CombineShiftRightBytes<12>(Full128<T>(), v, v);
}

// Reverse
template <typename T>
HWY_API Vec128<T> Shuffle0123(const Vec128<T> v) {
  return Shuffle2301(Shuffle1032(v));
}

// ------------------------------ InterleaveLower

// Interleaves lanes from halves of the 128-bit blocks of "a" (which provides
// the least-significant lane) and "b". To concatenate two half-width integers
// into one, use ZipLower/Upper instead (also works with scalar).
HWY_NEON_DEF_FUNCTION_INT_8_16_32(InterleaveLower, vzip1, _, 2)
HWY_NEON_DEF_FUNCTION_UINT_8_16_32(InterleaveLower, vzip1, _, 2)

#if HWY_ARCH_ARM_A64
// N=1 makes no sense (in that case, there would be no upper/lower).
HWY_API Vec128<uint64_t> InterleaveLower(const Vec128<uint64_t> a,
                                         const Vec128<uint64_t> b) {
  return Vec128<uint64_t>(vzip1q_u64(a.raw, b.raw));
}
HWY_API Vec128<int64_t> InterleaveLower(const Vec128<int64_t> a,
                                        const Vec128<int64_t> b) {
  return Vec128<int64_t>(vzip1q_s64(a.raw, b.raw));
}
HWY_API Vec128<double> InterleaveLower(const Vec128<double> a,
                                       const Vec128<double> b) {
  return Vec128<double>(vzip1q_f64(a.raw, b.raw));
}
#else
// ARMv7 emulation.
HWY_API Vec128<uint64_t> InterleaveLower(const Vec128<uint64_t> a,
                                         const Vec128<uint64_t> b) {
  return CombineShiftRightBytes<8>(Full128<uint64_t>(), b, Shuffle01(a));
}
HWY_API Vec128<int64_t> InterleaveLower(const Vec128<int64_t> a,
                                        const Vec128<int64_t> b) {
  return CombineShiftRightBytes<8>(Full128<int64_t>(), b, Shuffle01(a));
}
#endif

// Floats
HWY_API Vec128<float> InterleaveLower(const Vec128<float> a,
                                      const Vec128<float> b) {
  return Vec128<float>(vzip1q_f32(a.raw, b.raw));
}
template <size_t N, HWY_IF_LE64(float, N)>
HWY_API Vec128<float, N> InterleaveLower(const Vec128<float, N> a,
                                         const Vec128<float, N> b) {
  return Vec128<float, N>(vzip1_f32(a.raw, b.raw));
}

// < 64 bit parts
template <typename T, size_t N, HWY_IF_LE32(T, N)>
HWY_API Vec128<T, N> InterleaveLower(Vec128<T, N> a, Vec128<T, N> b) {
  using V64 = Vec128<T, 8 / sizeof(T)>;
  return Vec128<T, N>(InterleaveLower(V64(a.raw), V64(b.raw)).raw);
}

// Additional overload for the optional Simd<> tag.
template <typename T, size_t N, class V = Vec128<T, N>>
HWY_API V InterleaveLower(Simd<T, N> /* tag */, V a, V b) {
  return InterleaveLower(a, b);
}

// ------------------------------ InterleaveUpper (UpperHalf)

// All functions inside detail lack the required D parameter.
namespace detail {
HWY_NEON_DEF_FUNCTION_INT_8_16_32(InterleaveUpper, vzip2, _, 2)
HWY_NEON_DEF_FUNCTION_UINT_8_16_32(InterleaveUpper, vzip2, _, 2)

#if HWY_ARCH_ARM_A64
// N=1 makes no sense (in that case, there would be no upper/lower).
HWY_API Vec128<uint64_t> InterleaveUpper(const Vec128<uint64_t> a,
                                         const Vec128<uint64_t> b) {
  return Vec128<uint64_t>(vzip2q_u64(a.raw, b.raw));
}
HWY_API Vec128<int64_t> InterleaveUpper(Vec128<int64_t> a, Vec128<int64_t> b) {
  return Vec128<int64_t>(vzip2q_s64(a.raw, b.raw));
}
HWY_API Vec128<double> InterleaveUpper(Vec128<double> a, Vec128<double> b) {
  return Vec128<double>(vzip2q_f64(a.raw, b.raw));
}
#else
// ARMv7 emulation.
HWY_API Vec128<uint64_t> InterleaveUpper(const Vec128<uint64_t> a,
                                         const Vec128<uint64_t> b) {
  return CombineShiftRightBytes<8>(Full128<uint64_t>(), Shuffle01(b), a);
}
HWY_API Vec128<int64_t> InterleaveUpper(Vec128<int64_t> a, Vec128<int64_t> b) {
  return CombineShiftRightBytes<8>(Full128<int64_t>(), Shuffle01(b), a);
}
#endif

HWY_API Vec128<float> InterleaveUpper(Vec128<float> a, Vec128<float> b) {
  return Vec128<float>(vzip2q_f32(a.raw, b.raw));
}
HWY_API Vec128<float, 2> InterleaveUpper(const Vec128<float, 2> a,
                                         const Vec128<float, 2> b) {
  return Vec128<float, 2>(vzip2_f32(a.raw, b.raw));
}

}  // namespace detail

// Full register
template <typename T, size_t N, HWY_IF_GE64(T, N), class V = Vec128<T, N>>
HWY_API V InterleaveUpper(Simd<T, N> /* tag */, V a, V b) {
  return detail::InterleaveUpper(a, b);
}

// Partial
template <typename T, size_t N, HWY_IF_LE32(T, N), class V = Vec128<T, N>>
HWY_API V InterleaveUpper(Simd<T, N> d, V a, V b) {
  const Half<decltype(d)> d2;
  return InterleaveLower(d, V(UpperHalf(d2, a).raw), V(UpperHalf(d2, b).raw));
}

// ------------------------------ ZipLower/ZipUpper (InterleaveLower)

// Same as Interleave*, except that the return lanes are double-width integers;
// this is necessary because the single-lane scalar cannot return two values.
template <typename T, size_t N, class DW = RepartitionToWide<Simd<T, N>>>
HWY_API VFromD<DW> ZipLower(Vec128<T, N> a, Vec128<T, N> b) {
  return BitCast(DW(), InterleaveLower(a, b));
}
template <typename T, size_t N, class D = Simd<T, N>,
          class DW = RepartitionToWide<D>>
HWY_API VFromD<DW> ZipLower(DW dw, Vec128<T, N> a, Vec128<T, N> b) {
  return BitCast(dw, InterleaveLower(D(), a, b));
}

template <typename T, size_t N, class D = Simd<T, N>,
          class DW = RepartitionToWide<D>>
HWY_API VFromD<DW> ZipUpper(DW dw, Vec128<T, N> a, Vec128<T, N> b) {
  return BitCast(dw, InterleaveUpper(D(), a, b));
}

// ================================================== COMBINE

// ------------------------------ Combine (InterleaveLower)

// Full result
HWY_API Vec128<uint8_t> Combine(Full128<uint8_t> /* tag */,
                                Vec128<uint8_t, 8> hi, Vec128<uint8_t, 8> lo) {
  return Vec128<uint8_t>(vcombine_u8(lo.raw, hi.raw));
}
HWY_API Vec128<uint16_t> Combine(Full128<uint16_t> /* tag */,
                                 Vec128<uint16_t, 4> hi,
                                 Vec128<uint16_t, 4> lo) {
  return Vec128<uint16_t>(vcombine_u16(lo.raw, hi.raw));
}
HWY_API Vec128<uint32_t> Combine(Full128<uint32_t> /* tag */,
                                 Vec128<uint32_t, 2> hi,
                                 Vec128<uint32_t, 2> lo) {
  return Vec128<uint32_t>(vcombine_u32(lo.raw, hi.raw));
}
HWY_API Vec128<uint64_t> Combine(Full128<uint64_t> /* tag */,
                                 Vec128<uint64_t, 1> hi,
                                 Vec128<uint64_t, 1> lo) {
  return Vec128<uint64_t>(vcombine_u64(lo.raw, hi.raw));
}

HWY_API Vec128<int8_t> Combine(Full128<int8_t> /* tag */, Vec128<int8_t, 8> hi,
                               Vec128<int8_t, 8> lo) {
  return Vec128<int8_t>(vcombine_s8(lo.raw, hi.raw));
}
HWY_API Vec128<int16_t> Combine(Full128<int16_t> /* tag */,
                                Vec128<int16_t, 4> hi, Vec128<int16_t, 4> lo) {
  return Vec128<int16_t>(vcombine_s16(lo.raw, hi.raw));
}
HWY_API Vec128<int32_t> Combine(Full128<int32_t> /* tag */,
                                Vec128<int32_t, 2> hi, Vec128<int32_t, 2> lo) {
  return Vec128<int32_t>(vcombine_s32(lo.raw, hi.raw));
}
HWY_API Vec128<int64_t> Combine(Full128<int64_t> /* tag */,
                                Vec128<int64_t, 1> hi, Vec128<int64_t, 1> lo) {
  return Vec128<int64_t>(vcombine_s64(lo.raw, hi.raw));
}

HWY_API Vec128<float> Combine(Full128<float> /* tag */, Vec128<float, 2> hi,
                              Vec128<float, 2> lo) {
  return Vec128<float>(vcombine_f32(lo.raw, hi.raw));
}
#if HWY_ARCH_ARM_A64
HWY_API Vec128<double> Combine(Full128<double> /* tag */, Vec128<double, 1> hi,
                               Vec128<double, 1> lo) {
  return Vec128<double>(vcombine_f64(lo.raw, hi.raw));
}
#endif

// < 64bit input, <= 64 bit result
template <typename T, size_t N, HWY_IF_LE64(T, N)>
HWY_API Vec128<T, N> Combine(Simd<T, N> d, Vec128<T, N / 2> hi,
                             Vec128<T, N / 2> lo) {
  // First double N (only lower halves will be used).
  const Vec128<T, N> hi2(hi.raw);
  const Vec128<T, N> lo2(lo.raw);
  // Repartition to two unsigned lanes (each the size of the valid input).
  const Simd<UnsignedFromSize<N * sizeof(T) / 2>, 2> du;
  return BitCast(d, InterleaveLower(BitCast(du, lo2), BitCast(du, hi2)));
}

// ------------------------------ ZeroExtendVector (Combine)

template <typename T, size_t N>
HWY_API Vec128<T, N> ZeroExtendVector(Simd<T, N> d, Vec128<T, N / 2> lo) {
  return Combine(d, Zero(Half<decltype(d)>()), lo);
}

// ------------------------------ ConcatLowerLower

// 64 or 128-bit input: just interleave
template <typename T, size_t N, HWY_IF_GE64(T, N)>
HWY_API Vec128<T, N> ConcatLowerLower(const Simd<T, N> d, Vec128<T, N> hi,
                                      Vec128<T, N> lo) {
  // Treat half-width input as a single lane and interleave them.
  const Repartition<UnsignedFromSize<N * sizeof(T) / 2>, decltype(d)> du;
  return BitCast(d, InterleaveLower(BitCast(du, lo), BitCast(du, hi)));
}

#if HWY_ARCH_ARM_A64
namespace detail {

HWY_INLINE Vec128<uint8_t, 2> ConcatEven(Vec128<uint8_t, 2> hi,
                                         Vec128<uint8_t, 2> lo) {
  return Vec128<uint8_t, 2>(vtrn1_u8(lo.raw, hi.raw));
}
HWY_INLINE Vec128<uint16_t, 2> ConcatEven(Vec128<uint16_t, 2> hi,
                                          Vec128<uint16_t, 2> lo) {
  return Vec128<uint16_t, 2>(vtrn1_u16(lo.raw, hi.raw));
}

}  // namespace detail

// <= 32-bit input/output
template <typename T, size_t N, HWY_IF_LE32(T, N)>
HWY_API Vec128<T, N> ConcatLowerLower(const Simd<T, N> d, Vec128<T, N> hi,
                                      Vec128<T, N> lo) {
  // Treat half-width input as two lanes and take every second one.
  const Repartition<UnsignedFromSize<N * sizeof(T) / 2>, decltype(d)> du;
  return BitCast(d, detail::ConcatEven(BitCast(du, hi), BitCast(du, lo)));
}

#else

template <typename T, size_t N, HWY_IF_LE32(T, N)>
HWY_API Vec128<T, N> ConcatLowerLower(const Simd<T, N> d, Vec128<T, N> hi,
                                      Vec128<T, N> lo) {
  const Half<decltype(d)> d2;
  return Combine(LowerHalf(d2, hi), LowerHalf(d2, lo));
}
#endif  // HWY_ARCH_ARM_A64

// ------------------------------ ConcatUpperUpper

// 64 or 128-bit input: just interleave
template <typename T, size_t N, HWY_IF_GE64(T, N)>
HWY_API Vec128<T, N> ConcatUpperUpper(const Simd<T, N> d, Vec128<T, N> hi,
                                      Vec128<T, N> lo) {
  // Treat half-width input as a single lane and interleave them.
  const Repartition<UnsignedFromSize<N * sizeof(T) / 2>, decltype(d)> du;
  return BitCast(d, InterleaveUpper(du, BitCast(du, lo), BitCast(du, hi)));
}

#if HWY_ARCH_ARM_A64
namespace detail {

HWY_INLINE Vec128<uint8_t, 2> ConcatOdd(Vec128<uint8_t, 2> hi,
                                        Vec128<uint8_t, 2> lo) {
  return Vec128<uint8_t, 2>(vtrn2_u8(lo.raw, hi.raw));
}
HWY_INLINE Vec128<uint16_t, 2> ConcatOdd(Vec128<uint16_t, 2> hi,
                                         Vec128<uint16_t, 2> lo) {
  return Vec128<uint16_t, 2>(vtrn2_u16(lo.raw, hi.raw));
}

}  // namespace detail

// <= 32-bit input/output
template <typename T, size_t N, HWY_IF_LE32(T, N)>
HWY_API Vec128<T, N> ConcatUpperUpper(const Simd<T, N> d, Vec128<T, N> hi,
                                      Vec128<T, N> lo) {
  // Treat half-width input as two lanes and take every second one.
  const Repartition<UnsignedFromSize<N * sizeof(T) / 2>, decltype(d)> du;
  return BitCast(d, detail::ConcatOdd(BitCast(du, hi), BitCast(du, lo)));
}

#else

template <typename T, size_t N, HWY_IF_LE32(T, N)>
HWY_API Vec128<T, N> ConcatUpperUpper(const Simd<T, N> d, Vec128<T, N> hi,
                                      Vec128<T, N> lo) {
  const Half<decltype(d)> d2;
  return Combine(UpperHalf(d2, hi), UpperHalf(d2, lo));
}

#endif  // HWY_ARCH_ARM_A64

// ------------------------------ ConcatLowerUpper (ShiftLeftBytes)

// 64 or 128-bit input: extract from concatenated
template <typename T, size_t N, HWY_IF_GE64(T, N)>
HWY_API Vec128<T, N> ConcatLowerUpper(const Simd<T, N> d, Vec128<T, N> hi,
                                      Vec128<T, N> lo) {
  return CombineShiftRightBytes<N * sizeof(T) / 2>(d, hi, lo);
}

// <= 32-bit input/output
template <typename T, size_t N, HWY_IF_LE32(T, N)>
HWY_API Vec128<T, N> ConcatLowerUpper(const Simd<T, N> d, Vec128<T, N> hi,
                                      Vec128<T, N> lo) {
  constexpr size_t kSize = N * sizeof(T);
  const Repartition<uint8_t, decltype(d)> d8;
  const Simd<uint8_t, 8> d8x8;
  const Simd<T, 8 / sizeof(T)> d64;
  using V8x8 = VFromD<decltype(d8x8)>;
  const V8x8 hi8x8(BitCast(d8, hi).raw);
  // Move into most-significant bytes
  const V8x8 lo8x8 = ShiftLeftBytes<8 - kSize>(V8x8(BitCast(d8, lo).raw));
  const V8x8 r = CombineShiftRightBytes<8 - kSize / 2>(d8x8, hi8x8, lo8x8);
  // Back to original lane type, then shrink N.
  return Vec128<T, N>(BitCast(d64, r).raw);
}

// ------------------------------ ConcatUpperLower

// Works for all N.
template <typename T, size_t N>
HWY_API Vec128<T, N> ConcatUpperLower(Simd<T, N> d, Vec128<T, N> hi,
                                      Vec128<T, N> lo) {
  return IfThenElse(FirstN(d, Lanes(d) / 2), lo, hi);
}

// ------------------------------ OddEven (IfThenElse)

template <typename T, size_t N>
HWY_API Vec128<T, N> OddEven(const Vec128<T, N> a, const Vec128<T, N> b) {
  const Simd<T, N> d;
  const Repartition<uint8_t, decltype(d)> d8;
  alignas(16) constexpr uint8_t kBytes[16] = {
      ((0 / sizeof(T)) & 1) ? 0 : 0xFF,  ((1 / sizeof(T)) & 1) ? 0 : 0xFF,
      ((2 / sizeof(T)) & 1) ? 0 : 0xFF,  ((3 / sizeof(T)) & 1) ? 0 : 0xFF,
      ((4 / sizeof(T)) & 1) ? 0 : 0xFF,  ((5 / sizeof(T)) & 1) ? 0 : 0xFF,
      ((6 / sizeof(T)) & 1) ? 0 : 0xFF,  ((7 / sizeof(T)) & 1) ? 0 : 0xFF,
      ((8 / sizeof(T)) & 1) ? 0 : 0xFF,  ((9 / sizeof(T)) & 1) ? 0 : 0xFF,
      ((10 / sizeof(T)) & 1) ? 0 : 0xFF, ((11 / sizeof(T)) & 1) ? 0 : 0xFF,
      ((12 / sizeof(T)) & 1) ? 0 : 0xFF, ((13 / sizeof(T)) & 1) ? 0 : 0xFF,
      ((14 / sizeof(T)) & 1) ? 0 : 0xFF, ((15 / sizeof(T)) & 1) ? 0 : 0xFF,
  };
  const auto vec = BitCast(d, Load(d8, kBytes));
  return IfThenElse(MaskFromVec(vec), b, a);
}

// ================================================== CRYPTO

#if defined(__ARM_FEATURE_AES)

// Per-target flag to prevent generic_ops-inl.h from defining AESRound.
#ifdef HWY_NATIVE_AES
#undef HWY_NATIVE_AES
#else
#define HWY_NATIVE_AES
#endif

HWY_API Vec128<uint8_t> AESRound(Vec128<uint8_t> state,
                                 Vec128<uint8_t> round_key) {
  // NOTE: it is important that AESE and AESMC be consecutive instructions so
  // they can be fused. AESE includes AddRoundKey, which is a different ordering
  // than the AES-NI semantics we adopted, so XOR by 0 and later with the actual
  // round key (the compiler will hopefully optimize this for multiple rounds).
  return Vec128<uint8_t>(vaesmcq_u8(vaeseq_u8(state.raw, vdupq_n_u8(0)))) ^
         round_key;
}

HWY_API Vec128<uint64_t> CLMulLower(Vec128<uint64_t> a, Vec128<uint64_t> b) {
  return Vec128<uint64_t>((uint64x2_t)vmull_p64(GetLane(a), GetLane(b)));
}

HWY_API Vec128<uint64_t> CLMulUpper(Vec128<uint64_t> a, Vec128<uint64_t> b) {
  return Vec128<uint64_t>(
      (uint64x2_t)vmull_high_p64((poly64x2_t)a.raw, (poly64x2_t)b.raw));
}

#endif  // __ARM_FEATURE_AES

// ================================================== MISC

// ------------------------------ TableLookupBytes (Combine, LowerHalf)

// Both full
template <typename T, typename TI>
HWY_API Vec128<TI> TableLookupBytes(const Vec128<T> bytes,
                                    const Vec128<TI> from) {
  const Full128<TI> d;
  const Repartition<uint8_t, decltype(d)> d8;
#if HWY_ARCH_ARM_A64
  return BitCast(d, Vec128<uint8_t>(vqtbl1q_u8(BitCast(d8, bytes).raw,
                                               BitCast(d8, from).raw)));
#else
  uint8x16_t table0 = BitCast(d8, bytes).raw;
  uint8x8x2_t table;
  table.val[0] = vget_low_u8(table0);
  table.val[1] = vget_high_u8(table0);
  uint8x16_t idx = BitCast(d8, from).raw;
  uint8x8_t low = vtbl2_u8(table, vget_low_u8(idx));
  uint8x8_t hi = vtbl2_u8(table, vget_high_u8(idx));
  return BitCast(d, Vec128<uint8_t>(vcombine_u8(low, hi)));
#endif
}

// Partial index vector
template <typename T, typename TI, size_t NI, HWY_IF_LE64(TI, NI)>
HWY_API Vec128<TI, NI> TableLookupBytes(const Vec128<T> bytes,
                                        const Vec128<TI, NI> from) {
  const Full128<TI> d_full;
  const Vec128<TI, 8 / sizeof(T)> from64(from.raw);
  const auto idx_full = Combine(d_full, from64, from64);
  const auto out_full = TableLookupBytes(bytes, idx_full);
  return Vec128<TI, NI>(LowerHalf(Half<decltype(d_full)>(), out_full).raw);
}

// Partial table vector
template <typename T, size_t N, typename TI, HWY_IF_LE64(T, N)>
HWY_API Vec128<TI> TableLookupBytes(const Vec128<T, N> bytes,
                                    const Vec128<TI> from) {
  const Full128<T> d_full;
  return TableLookupBytes(Combine(d_full, bytes, bytes), from);
}

// Partial both
template <typename T, size_t N, typename TI, size_t NI, HWY_IF_LE64(T, N),
          HWY_IF_LE64(TI, NI)>
HWY_API VFromD<Repartition<T, Simd<TI, NI>>> TableLookupBytes(
    Vec128<T, N> bytes, Vec128<TI, NI> from) {
  const Simd<T, N> d;
  const Simd<TI, NI> d_idx;
  const Repartition<uint8_t, decltype(d_idx)> d_idx8;
  // uint8x8
  const auto bytes8 = BitCast(Repartition<uint8_t, decltype(d)>(), bytes);
  const auto from8 = BitCast(d_idx8, from);
  const VFromD<decltype(d_idx8)> v8(vtbl1_u8(bytes8.raw, from8.raw));
  return BitCast(d_idx, v8);
}

// For all vector widths; ARM anyway zeroes if >= 0x10.
template <class V, class VI>
HWY_API VI TableLookupBytesOr0(const V bytes, const VI from) {
  return TableLookupBytes(bytes, from);
}

// ------------------------------ Scatter (Store)

template <typename T, size_t N, typename Offset, HWY_IF_LE128(T, N)>
HWY_API void ScatterOffset(Vec128<T, N> v, Simd<T, N> d, T* HWY_RESTRICT base,
                           const Vec128<Offset, N> offset) {
  static_assert(sizeof(T) == sizeof(Offset), "Must match for portability");

  alignas(16) T lanes[N];
  Store(v, d, lanes);

  alignas(16) Offset offset_lanes[N];
  Store(offset, Simd<Offset, N>(), offset_lanes);

  uint8_t* base_bytes = reinterpret_cast<uint8_t*>(base);
  for (size_t i = 0; i < N; ++i) {
    CopyBytes<sizeof(T)>(&lanes[i], base_bytes + offset_lanes[i]);
  }
}

template <typename T, size_t N, typename Index, HWY_IF_LE128(T, N)>
HWY_API void ScatterIndex(Vec128<T, N> v, Simd<T, N> d, T* HWY_RESTRICT base,
                          const Vec128<Index, N> index) {
  static_assert(sizeof(T) == sizeof(Index), "Must match for portability");

  alignas(16) T lanes[N];
  Store(v, d, lanes);

  alignas(16) Index index_lanes[N];
  Store(index, Simd<Index, N>(), index_lanes);

  for (size_t i = 0; i < N; ++i) {
    base[index_lanes[i]] = lanes[i];
  }
}

// ------------------------------ Gather (Load/Store)

template <typename T, size_t N, typename Offset>
HWY_API Vec128<T, N> GatherOffset(const Simd<T, N> d,
                                  const T* HWY_RESTRICT base,
                                  const Vec128<Offset, N> offset) {
  static_assert(sizeof(T) == sizeof(Offset), "Must match for portability");

  alignas(16) Offset offset_lanes[N];
  Store(offset, Simd<Offset, N>(), offset_lanes);

  alignas(16) T lanes[N];
  const uint8_t* base_bytes = reinterpret_cast<const uint8_t*>(base);
  for (size_t i = 0; i < N; ++i) {
    CopyBytes<sizeof(T)>(base_bytes + offset_lanes[i], &lanes[i]);
  }
  return Load(d, lanes);
}

template <typename T, size_t N, typename Index>
HWY_API Vec128<T, N> GatherIndex(const Simd<T, N> d, const T* HWY_RESTRICT base,
                                 const Vec128<Index, N> index) {
  static_assert(sizeof(T) == sizeof(Index), "Must match for portability");

  alignas(16) Index index_lanes[N];
  Store(index, Simd<Index, N>(), index_lanes);

  alignas(16) T lanes[N];
  for (size_t i = 0; i < N; ++i) {
    lanes[i] = base[index_lanes[i]];
  }
  return Load(d, lanes);
}

// ------------------------------ Reductions

namespace detail {

// N=1 for any T: no-op
template <typename T>
HWY_INLINE Vec128<T, 1> SumOfLanes(const Vec128<T, 1> v) {
  return v;
}
template <typename T>
HWY_INLINE Vec128<T, 1> MinOfLanes(hwy::SizeTag<sizeof(T)> /* tag */,
                                   const Vec128<T, 1> v) {
  return v;
}
template <typename T>
HWY_INLINE Vec128<T, 1> MaxOfLanes(hwy::SizeTag<sizeof(T)> /* tag */,
                                   const Vec128<T, 1> v) {
  return v;
}

// u32/i32/f32: N=2
template <typename T, HWY_IF_LANE_SIZE(T, 4)>
HWY_INLINE Vec128<T, 2> SumOfLanes(const Vec128<T, 2> v10) {
  return v10 + Shuffle2301(v10);
}
template <typename T>
HWY_INLINE Vec128<T, 2> MinOfLanes(hwy::SizeTag<4> /* tag */,
                                   const Vec128<T, 2> v10) {
  return Min(v10, Shuffle2301(v10));
}
template <typename T>
HWY_INLINE Vec128<T, 2> MaxOfLanes(hwy::SizeTag<4> /* tag */,
                                   const Vec128<T, 2> v10) {
  return Max(v10, Shuffle2301(v10));
}

// full vectors
#if HWY_ARCH_ARM_A64
HWY_INLINE Vec128<uint32_t> SumOfLanes(const Vec128<uint32_t> v) {
  return Vec128<uint32_t>(vdupq_n_u32(vaddvq_u32(v.raw)));
}
HWY_INLINE Vec128<int32_t> SumOfLanes(const Vec128<int32_t> v) {
  return Vec128<int32_t>(vdupq_n_s32(vaddvq_s32(v.raw)));
}
HWY_INLINE Vec128<float> SumOfLanes(const Vec128<float> v) {
  return Vec128<float>(vdupq_n_f32(vaddvq_f32(v.raw)));
}
HWY_INLINE Vec128<uint64_t> SumOfLanes(const Vec128<uint64_t> v) {
  return Vec128<uint64_t>(vdupq_n_u64(vaddvq_u64(v.raw)));
}
HWY_INLINE Vec128<int64_t> SumOfLanes(const Vec128<int64_t> v) {
  return Vec128<int64_t>(vdupq_n_s64(vaddvq_s64(v.raw)));
}
HWY_INLINE Vec128<double> SumOfLanes(const Vec128<double> v) {
  return Vec128<double>(vdupq_n_f64(vaddvq_f64(v.raw)));
}
#else
// ARMv7 version for everything except doubles.
HWY_INLINE Vec128<uint32_t> SumOfLanes(const Vec128<uint32_t> v) {
  uint32x4x2_t v0 = vuzpq_u32(v.raw, v.raw);
  uint32x4_t c0 = vaddq_u32(v0.val[0], v0.val[1]);
  uint32x4x2_t v1 = vuzpq_u32(c0, c0);
  return Vec128<uint32_t>(vaddq_u32(v1.val[0], v1.val[1]));
}
HWY_INLINE Vec128<int32_t> SumOfLanes(const Vec128<int32_t> v) {
  int32x4x2_t v0 = vuzpq_s32(v.raw, v.raw);
  int32x4_t c0 = vaddq_s32(v0.val[0], v0.val[1]);
  int32x4x2_t v1 = vuzpq_s32(c0, c0);
  return Vec128<int32_t>(vaddq_s32(v1.val[0], v1.val[1]));
}
HWY_INLINE Vec128<float> SumOfLanes(const Vec128<float> v) {
  float32x4x2_t v0 = vuzpq_f32(v.raw, v.raw);
  float32x4_t c0 = vaddq_f32(v0.val[0], v0.val[1]);
  float32x4x2_t v1 = vuzpq_f32(c0, c0);
  return Vec128<float>(vaddq_f32(v1.val[0], v1.val[1]));
}
HWY_INLINE Vec128<uint64_t> SumOfLanes(const Vec128<uint64_t> v) {
  return v + Shuffle01(v);
}
HWY_INLINE Vec128<int64_t> SumOfLanes(const Vec128<int64_t> v) {
  return v + Shuffle01(v);
}
#endif

template <typename T>
HWY_INLINE Vec128<T> MinOfLanes(hwy::SizeTag<4> /* tag */,
                                const Vec128<T> v3210) {
  const Vec128<T> v1032 = Shuffle1032(v3210);
  const Vec128<T> v31_20_31_20 = Min(v3210, v1032);
  const Vec128<T> v20_31_20_31 = Shuffle0321(v31_20_31_20);
  return Min(v20_31_20_31, v31_20_31_20);
}
template <typename T>
HWY_INLINE Vec128<T> MaxOfLanes(hwy::SizeTag<4> /* tag */,
                                const Vec128<T> v3210) {
  const Vec128<T> v1032 = Shuffle1032(v3210);
  const Vec128<T> v31_20_31_20 = Max(v3210, v1032);
  const Vec128<T> v20_31_20_31 = Shuffle0321(v31_20_31_20);
  return Max(v20_31_20_31, v31_20_31_20);
}

// For u64/i64[/f64].
template <typename T>
HWY_INLINE Vec128<T> MinOfLanes(hwy::SizeTag<8> /* tag */,
                                const Vec128<T> v10) {
  const Vec128<T> v01 = Shuffle01(v10);
  return Min(v10, v01);
}
template <typename T>
HWY_INLINE Vec128<T> MaxOfLanes(hwy::SizeTag<8> /* tag */,
                                const Vec128<T> v10) {
  const Vec128<T> v01 = Shuffle01(v10);
  return Max(v10, v01);
}

}  // namespace detail

template <typename T, size_t N>
HWY_API Vec128<T, N> SumOfLanes(Simd<T, N> /* tag */, const Vec128<T, N> v) {
  return detail::SumOfLanes(v);
}
template <typename T, size_t N>
HWY_API Vec128<T, N> MinOfLanes(Simd<T, N> /* tag */, const Vec128<T, N> v) {
  return detail::MinOfLanes(hwy::SizeTag<sizeof(T)>(), v);
}
template <typename T, size_t N>
HWY_API Vec128<T, N> MaxOfLanes(Simd<T, N> /* tag */, const Vec128<T, N> v) {
  return detail::MaxOfLanes(hwy::SizeTag<sizeof(T)>(), v);
}

// ------------------------------ LoadMaskBits (TestBit)

namespace detail {

// Helper function to set 64 bits and potentially return a smaller vector. The
// overload is required to call the q vs non-q intrinsics. Note that 8-bit
// LoadMaskBits only requires 16 bits, but 64 avoids casting.
template <typename T, size_t N, HWY_IF_LE64(T, N)>
HWY_INLINE Vec128<T, N> Set64(Simd<T, N> /* tag */, uint64_t mask_bits) {
  const auto v64 = Vec128<uint64_t, 1>(vdup_n_u64(mask_bits));
  return Vec128<T, N>(BitCast(Simd<T, 8 / sizeof(T)>(), v64).raw);
}
template <typename T>
HWY_INLINE Vec128<T> Set64(Full128<T> d, uint64_t mask_bits) {
  return BitCast(d, Vec128<uint64_t>(vdupq_n_u64(mask_bits)));
}

template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 1)>
HWY_INLINE Mask128<T, N> LoadMaskBits(Simd<T, N> d, uint64_t mask_bits) {
  const RebindToUnsigned<decltype(d)> du;
  // Easier than Set(), which would require an >8-bit type, which would not
  // compile for T=uint8_t, N=1.
  const auto vmask_bits = Set64(du, mask_bits);

  // Replicate bytes 8x such that each byte contains the bit that governs it.
  alignas(16) constexpr uint8_t kRep8[16] = {0, 0, 0, 0, 0, 0, 0, 0,
                                             1, 1, 1, 1, 1, 1, 1, 1};
  const auto rep8 = TableLookupBytes(vmask_bits, Load(du, kRep8));

  alignas(16) constexpr uint8_t kBit[16] = {1, 2, 4, 8, 16, 32, 64, 128,
                                            1, 2, 4, 8, 16, 32, 64, 128};
  return RebindMask(d, TestBit(rep8, LoadDup128(du, kBit)));
}

template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 2)>
HWY_INLINE Mask128<T, N> LoadMaskBits(Simd<T, N> d, uint64_t mask_bits) {
  const RebindToUnsigned<decltype(d)> du;
  alignas(16) constexpr uint16_t kBit[8] = {1, 2, 4, 8, 16, 32, 64, 128};
  const auto vmask_bits = Set(du, static_cast<uint16_t>(mask_bits));
  return RebindMask(d, TestBit(vmask_bits, Load(du, kBit)));
}

template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 4)>
HWY_INLINE Mask128<T, N> LoadMaskBits(Simd<T, N> d, uint64_t mask_bits) {
  const RebindToUnsigned<decltype(d)> du;
  alignas(16) constexpr uint32_t kBit[8] = {1, 2, 4, 8};
  const auto vmask_bits = Set(du, static_cast<uint32_t>(mask_bits));
  return RebindMask(d, TestBit(vmask_bits, Load(du, kBit)));
}

template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 8)>
HWY_INLINE Mask128<T, N> LoadMaskBits(Simd<T, N> d, uint64_t mask_bits) {
  const RebindToUnsigned<decltype(d)> du;
  alignas(16) constexpr uint64_t kBit[8] = {1, 2};
  return RebindMask(d, TestBit(Set(du, mask_bits), Load(du, kBit)));
}

}  // namespace detail

// `p` points to at least 8 readable bytes, not all of which need be valid.
template <typename T, size_t N, HWY_IF_LE128(T, N)>
HWY_API Mask128<T, N> LoadMaskBits(Simd<T, N> d,
                                   const uint8_t* HWY_RESTRICT bits) {
  uint64_t mask_bits = 0;
  CopyBytes<(N + 7) / 8>(bits, &mask_bits);
  return detail::LoadMaskBits(d, mask_bits);
}

// ------------------------------ Mask

namespace detail {

template <typename T>
HWY_INLINE uint64_t BitsFromMask(hwy::SizeTag<1> /*tag*/,
                                 const Mask128<T> mask) {
  alignas(16) constexpr uint8_t kSliceLanes[16] = {
      1, 2, 4, 8, 0x10, 0x20, 0x40, 0x80, 1, 2, 4, 8, 0x10, 0x20, 0x40, 0x80,
  };
  const Full128<uint8_t> du;
  const Vec128<uint8_t> values =
      BitCast(du, VecFromMask(Full128<T>(), mask)) & Load(du, kSliceLanes);

#if HWY_ARCH_ARM_A64
  // Can't vaddv - we need two separate bytes (16 bits).
  const uint8x8_t x2 = vget_low_u8(vpaddq_u8(values.raw, values.raw));
  const uint8x8_t x4 = vpadd_u8(x2, x2);
  const uint8x8_t x8 = vpadd_u8(x4, x4);
  return vget_lane_u64(vreinterpret_u64_u8(x8), 0);
#else
  // Don't have vpaddq, so keep doubling lane size.
  const uint16x8_t x2 = vpaddlq_u8(values.raw);
  const uint32x4_t x4 = vpaddlq_u16(x2);
  const uint64x2_t x8 = vpaddlq_u32(x4);
  return (vgetq_lane_u64(x8, 1) << 8) | vgetq_lane_u64(x8, 0);
#endif
}

template <typename T, size_t N, HWY_IF_LE64(T, N)>
HWY_INLINE uint64_t BitsFromMask(hwy::SizeTag<1> /*tag*/,
                                 const Mask128<T, N> mask) {
  // Upper lanes of partial loads are undefined. OnlyActive will fix this if
  // we load all kSliceLanes so the upper lanes do not pollute the valid bits.
  alignas(8) constexpr uint8_t kSliceLanes[8] = {1,    2,    4,    8,
                                                 0x10, 0x20, 0x40, 0x80};
  const Simd<T, N> d;
  const Simd<uint8_t, N> du;
  const Vec128<uint8_t, N> slice(Load(Simd<uint8_t, 8>(), kSliceLanes).raw);
  const Vec128<uint8_t, N> values = BitCast(du, VecFromMask(d, mask)) & slice;

#if HWY_ARCH_ARM_A64
  return vaddv_u8(values.raw);
#else
  const uint16x4_t x2 = vpaddl_u8(values.raw);
  const uint32x2_t x4 = vpaddl_u16(x2);
  const uint64x1_t x8 = vpaddl_u32(x4);
  return vget_lane_u64(x8, 0);
#endif
}

template <typename T>
HWY_INLINE uint64_t BitsFromMask(hwy::SizeTag<2> /*tag*/,
                                 const Mask128<T> mask) {
  alignas(16) constexpr uint16_t kSliceLanes[8] = {1,    2,    4,    8,
                                                   0x10, 0x20, 0x40, 0x80};
  const Full128<T> d;
  const Full128<uint16_t> du;
  const Vec128<uint16_t> values =
      BitCast(du, VecFromMask(d, mask)) & Load(du, kSliceLanes);
#if HWY_ARCH_ARM_A64
  return vaddvq_u16(values.raw);
#else
  const uint32x4_t x2 = vpaddlq_u16(values.raw);
  const uint64x2_t x4 = vpaddlq_u32(x2);
  return vgetq_lane_u64(x4, 0) + vgetq_lane_u64(x4, 1);
#endif
}

template <typename T, size_t N, HWY_IF_LE64(T, N)>
HWY_INLINE uint64_t BitsFromMask(hwy::SizeTag<2> /*tag*/,
                                 const Mask128<T, N> mask) {
  // Upper lanes of partial loads are undefined. OnlyActive will fix this if
  // we load all kSliceLanes so the upper lanes do not pollute the valid bits.
  alignas(8) constexpr uint16_t kSliceLanes[4] = {1, 2, 4, 8};
  const Simd<T, N> d;
  const Simd<uint16_t, N> du;
  const Vec128<uint16_t, N> slice(Load(Simd<uint16_t, 4>(), kSliceLanes).raw);
  const Vec128<uint16_t, N> values = BitCast(du, VecFromMask(d, mask)) & slice;
#if HWY_ARCH_ARM_A64
  return vaddv_u16(values.raw);
#else
  const uint32x2_t x2 = vpaddl_u16(values.raw);
  const uint64x1_t x4 = vpaddl_u32(x2);
  return vget_lane_u64(x4, 0);
#endif
}

template <typename T>
HWY_INLINE uint64_t BitsFromMask(hwy::SizeTag<4> /*tag*/,
                                 const Mask128<T> mask) {
  alignas(16) constexpr uint32_t kSliceLanes[4] = {1, 2, 4, 8};
  const Full128<T> d;
  const Full128<uint32_t> du;
  const Vec128<uint32_t> values =
      BitCast(du, VecFromMask(d, mask)) & Load(du, kSliceLanes);
#if HWY_ARCH_ARM_A64
  return vaddvq_u32(values.raw);
#else
  const uint64x2_t x2 = vpaddlq_u32(values.raw);
  return vgetq_lane_u64(x2, 0) + vgetq_lane_u64(x2, 1);
#endif
}

template <typename T, size_t N, HWY_IF_LE64(T, N)>
HWY_INLINE uint64_t BitsFromMask(hwy::SizeTag<4> /*tag*/,
                                 const Mask128<T, N> mask) {
  // Upper lanes of partial loads are undefined. OnlyActive will fix this if
  // we load all kSliceLanes so the upper lanes do not pollute the valid bits.
  alignas(8) constexpr uint32_t kSliceLanes[2] = {1, 2};
  const Simd<T, N> d;
  const Simd<uint32_t, N> du;
  const Vec128<uint32_t, N> slice(Load(Simd<uint32_t, 2>(), kSliceLanes).raw);
  const Vec128<uint32_t, N> values = BitCast(du, VecFromMask(d, mask)) & slice;
#if HWY_ARCH_ARM_A64
  return vaddv_u32(values.raw);
#else
  const uint64x1_t x2 = vpaddl_u32(values.raw);
  return vget_lane_u64(x2, 0);
#endif
}

template <typename T>
HWY_INLINE uint64_t BitsFromMask(hwy::SizeTag<8> /*tag*/, const Mask128<T> m) {
  alignas(16) constexpr uint64_t kSliceLanes[2] = {1, 2};
  const Full128<T> d;
  const Full128<uint64_t> du;
  const Vec128<uint64_t> values =
      BitCast(du, VecFromMask(d, m)) & Load(du, kSliceLanes);
#if HWY_ARCH_ARM_A64
  return vaddvq_u64(values.raw);
#else
  return vgetq_lane_u64(values.raw, 0) + vgetq_lane_u64(values.raw, 1);
#endif
}

template <typename T>
HWY_INLINE uint64_t BitsFromMask(hwy::SizeTag<8> /*tag*/,
                                 const Mask128<T, 1> m) {
  const Simd<T, 1> d;
  const Simd<uint64_t, 1> du;
  const Vec128<uint64_t, 1> values =
      BitCast(du, VecFromMask(d, m)) & Set(du, 1);
  return vget_lane_u64(values.raw, 0);
}

// Returns the lowest N for the BitsFromMask result.
template <typename T, size_t N>
constexpr uint64_t OnlyActive(uint64_t bits) {
  return ((N * sizeof(T)) >= 8) ? bits : (bits & ((1ull << N) - 1));
}

template <typename T, size_t N>
HWY_INLINE uint64_t BitsFromMask(const Mask128<T, N> mask) {
  return OnlyActive<T, N>(BitsFromMask(hwy::SizeTag<sizeof(T)>(), mask));
}

// Returns number of lanes whose mask is set.
//
// Masks are either FF..FF or 0. Unfortunately there is no reduce-sub op
// ("vsubv"). ANDing with 1 would work but requires a constant. Negating also
// changes each lane to 1 (if mask set) or 0.

template <typename T>
HWY_INLINE size_t CountTrue(hwy::SizeTag<1> /*tag*/, const Mask128<T> mask) {
  const Full128<int8_t> di;
  const int8x16_t ones =
      vnegq_s8(BitCast(di, VecFromMask(Full128<T>(), mask)).raw);

#if HWY_ARCH_ARM_A64
  return static_cast<size_t>(vaddvq_s8(ones));
#else
  const int16x8_t x2 = vpaddlq_s8(ones);
  const int32x4_t x4 = vpaddlq_s16(x2);
  const int64x2_t x8 = vpaddlq_s32(x4);
  return static_cast<size_t>(vgetq_lane_s64(x8, 0) + vgetq_lane_s64(x8, 1));
#endif
}
template <typename T>
HWY_INLINE size_t CountTrue(hwy::SizeTag<2> /*tag*/, const Mask128<T> mask) {
  const Full128<int16_t> di;
  const int16x8_t ones =
      vnegq_s16(BitCast(di, VecFromMask(Full128<T>(), mask)).raw);

#if HWY_ARCH_ARM_A64
  return static_cast<size_t>(vaddvq_s16(ones));
#else
  const int32x4_t x2 = vpaddlq_s16(ones);
  const int64x2_t x4 = vpaddlq_s32(x2);
  return static_cast<size_t>(vgetq_lane_s64(x4, 0) + vgetq_lane_s64(x4, 1));
#endif
}

template <typename T>
HWY_INLINE size_t CountTrue(hwy::SizeTag<4> /*tag*/, const Mask128<T> mask) {
  const Full128<int32_t> di;
  const int32x4_t ones =
      vnegq_s32(BitCast(di, VecFromMask(Full128<T>(), mask)).raw);

#if HWY_ARCH_ARM_A64
  return static_cast<size_t>(vaddvq_s32(ones));
#else
  const int64x2_t x2 = vpaddlq_s32(ones);
  return static_cast<size_t>(vgetq_lane_s64(x2, 0) + vgetq_lane_s64(x2, 1));
#endif
}

template <typename T>
HWY_INLINE size_t CountTrue(hwy::SizeTag<8> /*tag*/, const Mask128<T> mask) {
#if HWY_ARCH_ARM_A64
  const Full128<int64_t> di;
  const int64x2_t ones =
      vnegq_s64(BitCast(di, VecFromMask(Full128<T>(), mask)).raw);
  return static_cast<size_t>(vaddvq_s64(ones));
#else
  const Full128<uint64_t> du;
  const auto mask_u = VecFromMask(du, RebindMask(du, mask));
  const uint64x2_t ones = vshrq_n_u64(mask_u.raw, 63);
  return static_cast<size_t>(vgetq_lane_u64(ones, 0) + vgetq_lane_u64(ones, 1));
#endif
}

}  // namespace detail

// Full
template <typename T>
HWY_API size_t CountTrue(Full128<T> /* tag */, const Mask128<T> mask) {
  return detail::CountTrue(hwy::SizeTag<sizeof(T)>(), mask);
}

// Partial
template <typename T, size_t N, HWY_IF_LE64(T, N)>
HWY_API size_t CountTrue(Simd<T, N> /* tag */, const Mask128<T, N> mask) {
  return PopCount(detail::BitsFromMask(mask));
}

template <typename T, size_t N>
HWY_API intptr_t FindFirstTrue(const Simd<T, N> /* tag */,
                              const Mask128<T, N> mask) {
  const uint64_t bits = detail::BitsFromMask(mask);
  return bits ? static_cast<intptr_t>(Num0BitsBelowLS1Bit_Nonzero64(bits)) : -1;
}

// `p` points to at least 8 writable bytes.
template <typename T, size_t N>
HWY_API size_t StoreMaskBits(Simd<T, N> /* tag */, const Mask128<T, N> mask,
                             uint8_t* bits) {
  const uint64_t mask_bits = detail::BitsFromMask(mask);
  const size_t kNumBytes = (N + 7) / 8;
  CopyBytes<kNumBytes>(&mask_bits, bits);
  return kNumBytes;
}

// Full
template <typename T>
HWY_API bool AllFalse(const Full128<T> d, const Mask128<T> m) {
#if HWY_ARCH_ARM_A64
  const Full128<uint32_t> d32;
  const auto m32 = MaskFromVec(BitCast(d32, VecFromMask(d, m)));
  return (vmaxvq_u32(m32.raw) == 0);
#else
  const auto v64 = BitCast(Full128<uint64_t>(), VecFromMask(d, m));
  uint32x2_t a = vqmovn_u64(v64.raw);
  return vget_lane_u64(vreinterpret_u64_u32(a), 0) == 0;
#endif
}

// Partial
template <typename T, size_t N, HWY_IF_LE64(T, N)>
HWY_API bool AllFalse(const Simd<T, N> /* tag */, const Mask128<T, N> m) {
  return detail::BitsFromMask(m) == 0;
}

template <typename T, size_t N>
HWY_API bool AllTrue(const Simd<T, N> d, const Mask128<T, N> m) {
  return AllFalse(VecFromMask(d, m) == Zero(d));
}

// ------------------------------ Compress

namespace detail {

// Load 8 bytes, replicate into upper half so ZipLower can use the lower half.
HWY_INLINE Vec128<uint8_t> Load8Bytes(Full128<uint8_t> /*d*/,
                                      const uint8_t* bytes) {
  return Vec128<uint8_t>(vreinterpretq_u8_u64(
      vld1q_dup_u64(reinterpret_cast<const uint64_t*>(bytes))));
}

// Load 8 bytes and return half-reg with N <= 8 bytes.
template <size_t N, HWY_IF_LE64(uint8_t, N)>
HWY_INLINE Vec128<uint8_t, N> Load8Bytes(Simd<uint8_t, N> d,
                                         const uint8_t* bytes) {
  return Load(d, bytes);
}

template <typename T, size_t N>
HWY_INLINE Vec128<T, N> IdxFromBits(hwy::SizeTag<2> /*tag*/,
                                    const uint64_t mask_bits) {
  HWY_DASSERT(mask_bits < 256);
  const Simd<T, N> d;
  const Repartition<uint8_t, decltype(d)> d8;
  const Simd<uint16_t, N> du;

  // ARM does not provide an equivalent of AVX2 permutevar, so we need byte
  // indices for VTBL (one vector's worth for each of 256 combinations of
  // 8 mask bits). Loading them directly would require 4 KiB. We can instead
  // store lane indices and convert to byte indices (2*lane + 0..1), with the
  // doubling baked into the table. AVX2 Compress32 stores eight 4-bit lane
  // indices (total 1 KiB), broadcasts them into each 32-bit lane and shifts.
  // Here, 16-bit lanes are too narrow to hold all bits, and unpacking nibbles
  // is likely more costly than the higher cache footprint from storing bytes.
  alignas(16) constexpr uint8_t table[256 * 8] = {
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  2,  0,
      0,  0,  0,  0,  0,  0,  0,  2,  0,  0,  0,  0,  0,  0,  4,  0,  0,  0,
      0,  0,  0,  0,  0,  4,  0,  0,  0,  0,  0,  0,  2,  4,  0,  0,  0,  0,
      0,  0,  0,  2,  4,  0,  0,  0,  0,  0,  6,  0,  0,  0,  0,  0,  0,  0,
      0,  6,  0,  0,  0,  0,  0,  0,  2,  6,  0,  0,  0,  0,  0,  0,  0,  2,
      6,  0,  0,  0,  0,  0,  4,  6,  0,  0,  0,  0,  0,  0,  0,  4,  6,  0,
      0,  0,  0,  0,  2,  4,  6,  0,  0,  0,  0,  0,  0,  2,  4,  6,  0,  0,
      0,  0,  8,  0,  0,  0,  0,  0,  0,  0,  0,  8,  0,  0,  0,  0,  0,  0,
      2,  8,  0,  0,  0,  0,  0,  0,  0,  2,  8,  0,  0,  0,  0,  0,  4,  8,
      0,  0,  0,  0,  0,  0,  0,  4,  8,  0,  0,  0,  0,  0,  2,  4,  8,  0,
      0,  0,  0,  0,  0,  2,  4,  8,  0,  0,  0,  0,  6,  8,  0,  0,  0,  0,
      0,  0,  0,  6,  8,  0,  0,  0,  0,  0,  2,  6,  8,  0,  0,  0,  0,  0,
      0,  2,  6,  8,  0,  0,  0,  0,  4,  6,  8,  0,  0,  0,  0,  0,  0,  4,
      6,  8,  0,  0,  0,  0,  2,  4,  6,  8,  0,  0,  0,  0,  0,  2,  4,  6,
      8,  0,  0,  0,  10, 0,  0,  0,  0,  0,  0,  0,  0,  10, 0,  0,  0,  0,
      0,  0,  2,  10, 0,  0,  0,  0,  0,  0,  0,  2,  10, 0,  0,  0,  0,  0,
      4,  10, 0,  0,  0,  0,  0,  0,  0,  4,  10, 0,  0,  0,  0,  0,  2,  4,
      10, 0,  0,  0,  0,  0,  0,  2,  4,  10, 0,  0,  0,  0,  6,  10, 0,  0,
      0,  0,  0,  0,  0,  6,  10, 0,  0,  0,  0,  0,  2,  6,  10, 0,  0,  0,
      0,  0,  0,  2,  6,  10, 0,  0,  0,  0,  4,  6,  10, 0,  0,  0,  0,  0,
      0,  4,  6,  10, 0,  0,  0,  0,  2,  4,  6,  10, 0,  0,  0,  0,  0,  2,
      4,  6,  10, 0,  0,  0,  8,  10, 0,  0,  0,  0,  0,  0,  0,  8,  10, 0,
      0,  0,  0,  0,  2,  8,  10, 0,  0,  0,  0,  0,  0,  2,  8,  10, 0,  0,
      0,  0,  4,  8,  10, 0,  0,  0,  0,  0,  0,  4,  8,  10, 0,  0,  0,  0,
      2,  4,  8,  10, 0,  0,  0,  0,  0,  2,  4,  8,  10, 0,  0,  0,  6,  8,
      10, 0,  0,  0,  0,  0,  0,  6,  8,  10, 0,  0,  0,  0,  2,  6,  8,  10,
      0,  0,  0,  0,  0,  2,  6,  8,  10, 0,  0,  0,  4,  6,  8,  10, 0,  0,
      0,  0,  0,  4,  6,  8,  10, 0,  0,  0,  2,  4,  6,  8,  10, 0,  0,  0,
      0,  2,  4,  6,  8,  10, 0,  0,  12, 0,  0,  0,  0,  0,  0,  0,  0,  12,
      0,  0,  0,  0,  0,  0,  2,  12, 0,  0,  0,  0,  0,  0,  0,  2,  12, 0,
      0,  0,  0,  0,  4,  12, 0,  0,  0,  0,  0,  0,  0,  4,  12, 0,  0,  0,
      0,  0,  2,  4,  12, 0,  0,  0,  0,  0,  0,  2,  4,  12, 0,  0,  0,  0,
      6,  12, 0,  0,  0,  0,  0,  0,  0,  6,  12, 0,  0,  0,  0,  0,  2,  6,
      12, 0,  0,  0,  0,  0,  0,  2,  6,  12, 0,  0,  0,  0,  4,  6,  12, 0,
      0,  0,  0,  0,  0,  4,  6,  12, 0,  0,  0,  0,  2,  4,  6,  12, 0,  0,
      0,  0,  0,  2,  4,  6,  12, 0,  0,  0,  8,  12, 0,  0,  0,  0,  0,  0,
      0,  8,  12, 0,  0,  0,  0,  0,  2,  8,  12, 0,  0,  0,  0,  0,  0,  2,
      8,  12, 0,  0,  0,  0,  4,  8,  12, 0,  0,  0,  0,  0,  0,  4,  8,  12,
      0,  0,  0,  0,  2,  4,  8,  12, 0,  0,  0,  0,  0,  2,  4,  8,  12, 0,
      0,  0,  6,  8,  12, 0,  0,  0,  0,  0,  0,  6,  8,  12, 0,  0,  0,  0,
      2,  6,  8,  12, 0,  0,  0,  0,  0,  2,  6,  8,  12, 0,  0,  0,  4,  6,
      8,  12, 0,  0,  0,  0,  0,  4,  6,  8,  12, 0,  0,  0,  2,  4,  6,  8,
      12, 0,  0,  0,  0,  2,  4,  6,  8,  12, 0,  0,  10, 12, 0,  0,  0,  0,
      0,  0,  0,  10, 12, 0,  0,  0,  0,  0,  2,  10, 12, 0,  0,  0,  0,  0,
      0,  2,  10, 12, 0,  0,  0,  0,  4,  10, 12, 0,  0,  0,  0,  0,  0,  4,
      10, 12, 0,  0,  0,  0,  2,  4,  10, 12, 0,  0,  0,  0,  0,  2,  4,  10,
      12, 0,  0,  0,  6,  10, 12, 0,  0,  0,  0,  0,  0,  6,  10, 12, 0,  0,
      0,  0,  2,  6,  10, 12, 0,  0,  0,  0,  0,  2,  6,  10, 12, 0,  0,  0,
      4,  6,  10, 12, 0,  0,  0,  0,  0,  4,  6,  10, 12, 0,  0,  0,  2,  4,
      6,  10, 12, 0,  0,  0,  0,  2,  4,  6,  10, 12, 0,  0,  8,  10, 12, 0,
      0,  0,  0,  0,  0,  8,  10, 12, 0,  0,  0,  0,  2,  8,  10, 12, 0,  0,
      0,  0,  0,  2,  8,  10, 12, 0,  0,  0,  4,  8,  10, 12, 0,  0,  0,  0,
      0,  4,  8,  10, 12, 0,  0,  0,  2,  4,  8,  10, 12, 0,  0,  0,  0,  2,
      4,  8,  10, 12, 0,  0,  6,  8,  10, 12, 0,  0,  0,  0,  0,  6,  8,  10,
      12, 0,  0,  0,  2,  6,  8,  10, 12, 0,  0,  0,  0,  2,  6,  8,  10, 12,
      0,  0,  4,  6,  8,  10, 12, 0,  0,  0,  0,  4,  6,  8,  10, 12, 0,  0,
      2,  4,  6,  8,  10, 12, 0,  0,  0,  2,  4,  6,  8,  10, 12, 0,  14, 0,
      0,  0,  0,  0,  0,  0,  0,  14, 0,  0,  0,  0,  0,  0,  2,  14, 0,  0,
      0,  0,  0,  0,  0,  2,  14, 0,  0,  0,  0,  0,  4,  14, 0,  0,  0,  0,
      0,  0,  0,  4,  14, 0,  0,  0,  0,  0,  2,  4,  14, 0,  0,  0,  0,  0,
      0,  2,  4,  14, 0,  0,  0,  0,  6,  14, 0,  0,  0,  0,  0,  0,  0,  6,
      14, 0,  0,  0,  0,  0,  2,  6,  14, 0,  0,  0,  0,  0,  0,  2,  6,  14,
      0,  0,  0,  0,  4,  6,  14, 0,  0,  0,  0,  0,  0,  4,  6,  14, 0,  0,
      0,  0,  2,  4,  6,  14, 0,  0,  0,  0,  0,  2,  4,  6,  14, 0,  0,  0,
      8,  14, 0,  0,  0,  0,  0,  0,  0,  8,  14, 0,  0,  0,  0,  0,  2,  8,
      14, 0,  0,  0,  0,  0,  0,  2,  8,  14, 0,  0,  0,  0,  4,  8,  14, 0,
      0,  0,  0,  0,  0,  4,  8,  14, 0,  0,  0,  0,  2,  4,  8,  14, 0,  0,
      0,  0,  0,  2,  4,  8,  14, 0,  0,  0,  6,  8,  14, 0,  0,  0,  0,  0,
      0,  6,  8,  14, 0,  0,  0,  0,  2,  6,  8,  14, 0,  0,  0,  0,  0,  2,
      6,  8,  14, 0,  0,  0,  4,  6,  8,  14, 0,  0,  0,  0,  0,  4,  6,  8,
      14, 0,  0,  0,  2,  4,  6,  8,  14, 0,  0,  0,  0,  2,  4,  6,  8,  14,
      0,  0,  10, 14, 0,  0,  0,  0,  0,  0,  0,  10, 14, 0,  0,  0,  0,  0,
      2,  10, 14, 0,  0,  0,  0,  0,  0,  2,  10, 14, 0,  0,  0,  0,  4,  10,
      14, 0,  0,  0,  0,  0,  0,  4,  10, 14, 0,  0,  0,  0,  2,  4,  10, 14,
      0,  0,  0,  0,  0,  2,  4,  10, 14, 0,  0,  0,  6,  10, 14, 0,  0,  0,
      0,  0,  0,  6,  10, 14, 0,  0,  0,  0,  2,  6,  10, 14, 0,  0,  0,  0,
      0,  2,  6,  10, 14, 0,  0,  0,  4,  6,  10, 14, 0,  0,  0,  0,  0,  4,
      6,  10, 14, 0,  0,  0,  2,  4,  6,  10, 14, 0,  0,  0,  0,  2,  4,  6,
      10, 14, 0,  0,  8,  10, 14, 0,  0,  0,  0,  0,  0,  8,  10, 14, 0,  0,
      0,  0,  2,  8,  10, 14, 0,  0,  0,  0,  0,  2,  8,  10, 14, 0,  0,  0,
      4,  8,  10, 14, 0,  0,  0,  0,  0,  4,  8,  10, 14, 0,  0,  0,  2,  4,
      8,  10, 14, 0,  0,  0,  0,  2,  4,  8,  10, 14, 0,  0,  6,  8,  10, 14,
      0,  0,  0,  0,  0,  6,  8,  10, 14, 0,  0,  0,  2,  6,  8,  10, 14, 0,
      0,  0,  0,  2,  6,  8,  10, 14, 0,  0,  4,  6,  8,  10, 14, 0,  0,  0,
      0,  4,  6,  8,  10, 14, 0,  0,  2,  4,  6,  8,  10, 14, 0,  0,  0,  2,
      4,  6,  8,  10, 14, 0,  12, 14, 0,  0,  0,  0,  0,  0,  0,  12, 14, 0,
      0,  0,  0,  0,  2,  12, 14, 0,  0,  0,  0,  0,  0,  2,  12, 14, 0,  0,
      0,  0,  4,  12, 14, 0,  0,  0,  0,  0,  0,  4,  12, 14, 0,  0,  0,  0,
      2,  4,  12, 14, 0,  0,  0,  0,  0,  2,  4,  12, 14, 0,  0,  0,  6,  12,
      14, 0,  0,  0,  0,  0,  0,  6,  12, 14, 0,  0,  0,  0,  2,  6,  12, 14,
      0,  0,  0,  0,  0,  2,  6,  12, 14, 0,  0,  0,  4,  6,  12, 14, 0,  0,
      0,  0,  0,  4,  6,  12, 14, 0,  0,  0,  2,  4,  6,  12, 14, 0,  0,  0,
      0,  2,  4,  6,  12, 14, 0,  0,  8,  12, 14, 0,  0,  0,  0,  0,  0,  8,
      12, 14, 0,  0,  0,  0,  2,  8,  12, 14, 0,  0,  0,  0,  0,  2,  8,  12,
      14, 0,  0,  0,  4,  8,  12, 14, 0,  0,  0,  0,  0,  4,  8,  12, 14, 0,
      0,  0,  2,  4,  8,  12, 14, 0,  0,  0,  0,  2,  4,  8,  12, 14, 0,  0,
      6,  8,  12, 14, 0,  0,  0,  0,  0,  6,  8,  12, 14, 0,  0,  0,  2,  6,
      8,  12, 14, 0,  0,  0,  0,  2,  6,  8,  12, 14, 0,  0,  4,  6,  8,  12,
      14, 0,  0,  0,  0,  4,  6,  8,  12, 14, 0,  0,  2,  4,  6,  8,  12, 14,
      0,  0,  0,  2,  4,  6,  8,  12, 14, 0,  10, 12, 14, 0,  0,  0,  0,  0,
      0,  10, 12, 14, 0,  0,  0,  0,  2,  10, 12, 14, 0,  0,  0,  0,  0,  2,
      10, 12, 14, 0,  0,  0,  4,  10, 12, 14, 0,  0,  0,  0,  0,  4,  10, 12,
      14, 0,  0,  0,  2,  4,  10, 12, 14, 0,  0,  0,  0,  2,  4,  10, 12, 14,
      0,  0,  6,  10, 12, 14, 0,  0,  0,  0,  0,  6,  10, 12, 14, 0,  0,  0,
      2,  6,  10, 12, 14, 0,  0,  0,  0,  2,  6,  10, 12, 14, 0,  0,  4,  6,
      10, 12, 14, 0,  0,  0,  0,  4,  6,  10, 12, 14, 0,  0,  2,  4,  6,  10,
      12, 14, 0,  0,  0,  2,  4,  6,  10, 12, 14, 0,  8,  10, 12, 14, 0,  0,
      0,  0,  0,  8,  10, 12, 14, 0,  0,  0,  2,  8,  10, 12, 14, 0,  0,  0,
      0,  2,  8,  10, 12, 14, 0,  0,  4,  8,  10, 12, 14, 0,  0,  0,  0,  4,
      8,  10, 12, 14, 0,  0,  2,  4,  8,  10, 12, 14, 0,  0,  0,  2,  4,  8,
      10, 12, 14, 0,  6,  8,  10, 12, 14, 0,  0,  0,  0,  6,  8,  10, 12, 14,
      0,  0,  2,  6,  8,  10, 12, 14, 0,  0,  0,  2,  6,  8,  10, 12, 14, 0,
      4,  6,  8,  10, 12, 14, 0,  0,  0,  4,  6,  8,  10, 12, 14, 0,  2,  4,
      6,  8,  10, 12, 14, 0,  0,  2,  4,  6,  8,  10, 12, 14};

  const Vec128<uint8_t, 2 * N> byte_idx = Load8Bytes(d8, table + mask_bits * 8);
  const Vec128<uint16_t, N> pairs = ZipLower(byte_idx, byte_idx);
  return BitCast(d, pairs + Set(du, 0x0100));
}

template <typename T, size_t N>
HWY_INLINE Vec128<T, N> IdxFromBits(hwy::SizeTag<4> /*tag*/,
                                    const uint64_t mask_bits) {
  HWY_DASSERT(mask_bits < 16);

  // There are only 4 lanes, so we can afford to load the index vector directly.
  alignas(16) constexpr uint8_t packed_array[16 * 16] = {
      0,  1,  2,  3,  0,  1,  2,  3,  0,  1,  2,  3,  0,  1,  2,  3,  //
      0,  1,  2,  3,  0,  1,  2,  3,  0,  1,  2,  3,  0,  1,  2,  3,  //
      4,  5,  6,  7,  0,  1,  2,  3,  0,  1,  2,  3,  0,  1,  2,  3,  //
      0,  1,  2,  3,  4,  5,  6,  7,  0,  1,  2,  3,  0,  1,  2,  3,  //
      8,  9,  10, 11, 0,  1,  2,  3,  0,  1,  2,  3,  0,  1,  2,  3,  //
      0,  1,  2,  3,  8,  9,  10, 11, 0,  1,  2,  3,  0,  1,  2,  3,  //
      4,  5,  6,  7,  8,  9,  10, 11, 0,  1,  2,  3,  0,  1,  2,  3,  //
      0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 0,  1,  2,  3,  //
      12, 13, 14, 15, 0,  1,  2,  3,  0,  1,  2,  3,  0,  1,  2,  3,  //
      0,  1,  2,  3,  12, 13, 14, 15, 0,  1,  2,  3,  0,  1,  2,  3,  //
      4,  5,  6,  7,  12, 13, 14, 15, 0,  1,  2,  3,  0,  1,  2,  3,  //
      0,  1,  2,  3,  4,  5,  6,  7,  12, 13, 14, 15, 0,  1,  2,  3,  //
      8,  9,  10, 11, 12, 13, 14, 15, 0,  1,  2,  3,  0,  1,  2,  3,  //
      0,  1,  2,  3,  8,  9,  10, 11, 12, 13, 14, 15, 0,  1,  2,  3,  //
      4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 0,  1,  2,  3,  //
      0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15};

  const Simd<T, N> d;
  const Repartition<uint8_t, decltype(d)> d8;
  return BitCast(d, Load(d8, packed_array + 16 * mask_bits));
}

#if HWY_CAP_INTEGER64 || HWY_CAP_FLOAT64

template <typename T, size_t N>
HWY_INLINE Vec128<T, N> IdxFromBits(hwy::SizeTag<8> /*tag*/,
                                    const uint64_t mask_bits) {
  HWY_DASSERT(mask_bits < 4);

  // There are only 2 lanes, so we can afford to load the index vector directly.
  alignas(16) constexpr uint8_t packed_array[4 * 16] = {
      0, 1, 2,  3,  4,  5,  6,  7,  0, 1, 2,  3,  4,  5,  6,  7,  //
      0, 1, 2,  3,  4,  5,  6,  7,  0, 1, 2,  3,  4,  5,  6,  7,  //
      8, 9, 10, 11, 12, 13, 14, 15, 0, 1, 2,  3,  4,  5,  6,  7,  //
      0, 1, 2,  3,  4,  5,  6,  7,  8, 9, 10, 11, 12, 13, 14, 15};

  const Simd<T, N> d;
  const Repartition<uint8_t, decltype(d)> d8;
  return BitCast(d, Load(d8, packed_array + 16 * mask_bits));
}

#endif

// Helper function called by both Compress and CompressStore - avoids a
// redundant BitsFromMask in the latter.
template <typename T, size_t N>
HWY_INLINE Vec128<T, N> Compress(Vec128<T, N> v, const uint64_t mask_bits) {
  const auto idx =
      detail::IdxFromBits<T, N>(hwy::SizeTag<sizeof(T)>(), mask_bits);
  using D = Simd<T, N>;
  const RebindToSigned<D> di;
  return BitCast(D(), TableLookupBytes(BitCast(di, v), BitCast(di, idx)));
}

}  // namespace detail

template <typename T, size_t N>
HWY_API Vec128<T, N> Compress(Vec128<T, N> v, const Mask128<T, N> mask) {
  return detail::Compress(v, detail::BitsFromMask(mask));
}

// ------------------------------ CompressBits

template <typename T, size_t N>
HWY_INLINE Vec128<T, N> CompressBits(Vec128<T, N> v,
                                     const uint8_t* HWY_RESTRICT bits) {
  uint64_t mask_bits = 0;
  constexpr size_t kNumBytes = (N + 7) / 8;
  CopyBytes<kNumBytes>(bits, &mask_bits);
  if (N < 8) {
    mask_bits &= (1ull << N) - 1;
  }

  return detail::Compress(v, mask_bits);
}

// ------------------------------ CompressStore

template <typename T, size_t N>
HWY_API size_t CompressStore(Vec128<T, N> v, const Mask128<T, N> mask,
                             Simd<T, N> d, T* HWY_RESTRICT unaligned) {
  const uint64_t mask_bits = detail::BitsFromMask(mask);
  StoreU(detail::Compress(v, mask_bits), d, unaligned);
  return PopCount(mask_bits);
}

// ------------------------------ CompressBitsStore

template <typename T, size_t N>
HWY_API size_t CompressBitsStore(Vec128<T, N> v,
                                 const uint8_t* HWY_RESTRICT bits, Simd<T, N> d,
                                 T* HWY_RESTRICT unaligned) {
  uint64_t mask_bits = 0;
  constexpr size_t kNumBytes = (N + 7) / 8;
  CopyBytes<kNumBytes>(bits, &mask_bits);
  if (N < 8) {
    mask_bits &= (1ull << N) - 1;
  }

  StoreU(detail::Compress(v, mask_bits), d, unaligned);
  return PopCount(mask_bits);
}

// ------------------------------ StoreInterleaved3

// 128 bits
HWY_API void StoreInterleaved3(const Vec128<uint8_t> v0,
                               const Vec128<uint8_t> v1,
                               const Vec128<uint8_t> v2,
                               Full128<uint8_t> /*tag*/,
                               uint8_t* HWY_RESTRICT unaligned) {
  const uint8x16x3_t triple = {v0.raw, v1.raw, v2.raw};
  vst3q_u8(unaligned, triple);
}

// 64 bits
HWY_API void StoreInterleaved3(const Vec128<uint8_t, 8> v0,
                               const Vec128<uint8_t, 8> v1,
                               const Vec128<uint8_t, 8> v2,
                               Simd<uint8_t, 8> /*tag*/,
                               uint8_t* HWY_RESTRICT unaligned) {
  const uint8x8x3_t triple = {v0.raw, v1.raw, v2.raw};
  vst3_u8(unaligned, triple);
}

// <= 32 bits: avoid writing more than N bytes by copying to buffer
template <size_t N, HWY_IF_LE32(uint8_t, N)>
HWY_API void StoreInterleaved3(const Vec128<uint8_t, N> v0,
                               const Vec128<uint8_t, N> v1,
                               const Vec128<uint8_t, N> v2,
                               Simd<uint8_t, N> /*tag*/,
                               uint8_t* HWY_RESTRICT unaligned) {
  alignas(16) uint8_t buf[24];
  const uint8x8x3_t triple = {v0.raw, v1.raw, v2.raw};
  vst3_u8(buf, triple);
  CopyBytes<N * 3>(buf, unaligned);
}

// ------------------------------ StoreInterleaved4

// 128 bits
HWY_API void StoreInterleaved4(const Vec128<uint8_t> v0,
                               const Vec128<uint8_t> v1,
                               const Vec128<uint8_t> v2,
                               const Vec128<uint8_t> v3,
                               Full128<uint8_t> /*tag*/,
                               uint8_t* HWY_RESTRICT unaligned) {
  const uint8x16x4_t quad = {v0.raw, v1.raw, v2.raw, v3.raw};
  vst4q_u8(unaligned, quad);
}

// 64 bits
HWY_API void StoreInterleaved4(const Vec128<uint8_t, 8> v0,
                               const Vec128<uint8_t, 8> v1,
                               const Vec128<uint8_t, 8> v2,
                               const Vec128<uint8_t, 8> v3,
                               Simd<uint8_t, 8> /*tag*/,
                               uint8_t* HWY_RESTRICT unaligned) {
  const uint8x8x4_t quad = {v0.raw, v1.raw, v2.raw, v3.raw};
  vst4_u8(unaligned, quad);
}

// <= 32 bits: avoid writing more than N bytes by copying to buffer
template <size_t N, HWY_IF_LE32(uint8_t, N)>
HWY_API void StoreInterleaved4(const Vec128<uint8_t, N> v0,
                               const Vec128<uint8_t, N> v1,
                               const Vec128<uint8_t, N> v2,
                               const Vec128<uint8_t, N> v3,
                               Simd<uint8_t, N> /*tag*/,
                               uint8_t* HWY_RESTRICT unaligned) {
  alignas(16) uint8_t buf[32];
  const uint8x8x4_t quad = {v0.raw, v1.raw, v2.raw, v3.raw};
  vst4_u8(buf, quad);
  CopyBytes<N * 4>(buf, unaligned);
}

// ================================================== DEPRECATED

template <typename T, size_t N>
HWY_API size_t StoreMaskBits(const Mask128<T, N> mask, uint8_t* bits) {
  return StoreMaskBits(Simd<T, N>(), mask, bits);
}

template <typename T, size_t N>
HWY_API bool AllTrue(const Mask128<T, N> mask) {
  return AllTrue(Simd<T, N>(), mask);
}

template <typename T, size_t N>
HWY_API bool AllFalse(const Mask128<T, N> mask) {
  return AllFalse(Simd<T, N>(), mask);
}

template <typename T, size_t N>
HWY_API size_t CountTrue(const Mask128<T, N> mask) {
  return CountTrue(Simd<T, N>(), mask);
}

template <typename T, size_t N>
HWY_API Vec128<T, N> SumOfLanes(const Vec128<T, N> v) {
  return SumOfLanes(Simd<T, N>(), v);
}
template <typename T, size_t N>
HWY_API Vec128<T, N> MinOfLanes(const Vec128<T, N> v) {
  return MinOfLanes(Simd<T, N>(), v);
}
template <typename T, size_t N>
HWY_API Vec128<T, N> MaxOfLanes(const Vec128<T, N> v) {
  return MaxOfLanes(Simd<T, N>(), v);
}

template <typename T, size_t N>
HWY_API Vec128<T, (N + 1) / 2> UpperHalf(Vec128<T, N> v) {
  return UpperHalf(Half<Simd<T, N>>(), v);
}

template <int kBytes, typename T, size_t N>
HWY_API Vec128<T, N> ShiftRightBytes(const Vec128<T, N> v) {
  return ShiftRightBytes<kBytes>(Simd<T, N>(), v);
}

template <int kLanes, typename T, size_t N>
HWY_API Vec128<T, N> ShiftRightLanes(const Vec128<T, N> v) {
  return ShiftRightLanes<kLanes>(Simd<T, N>(), v);
}

template <size_t kBytes, typename T, size_t N>
HWY_API Vec128<T, N> CombineShiftRightBytes(Vec128<T, N> hi, Vec128<T, N> lo) {
  return CombineShiftRightBytes<kBytes>(Simd<T, N>(), hi, lo);
}

template <typename T, size_t N>
HWY_API Vec128<T, N> InterleaveUpper(Vec128<T, N> a, Vec128<T, N> b) {
  return InterleaveUpper(Simd<T, N>(), a, b);
}

template <typename T, size_t N, class D = Simd<T, N>>
HWY_API VFromD<RepartitionToWide<D>> ZipUpper(Vec128<T, N> a, Vec128<T, N> b) {
  return InterleaveUpper(RepartitionToWide<D>(), a, b);
}

template <typename T, size_t N2>
HWY_API Vec128<T, N2 * 2> Combine(Vec128<T, N2> hi2, Vec128<T, N2> lo2) {
  return Combine(Simd<T, N2 * 2>(), hi2, lo2);
}

template <typename T, size_t N2, HWY_IF_LE64(T, N2)>
HWY_API Vec128<T, N2 * 2> ZeroExtendVector(Vec128<T, N2> lo) {
  return ZeroExtendVector(Simd<T, N2 * 2>(), lo);
}

template <typename T, size_t N>
HWY_API Vec128<T, N> ConcatLowerLower(Vec128<T, N> hi, Vec128<T, N> lo) {
  return ConcatLowerLower(Simd<T, N>(), hi, lo);
}

template <typename T, size_t N>
HWY_API Vec128<T, N> ConcatUpperUpper(Vec128<T, N> hi, Vec128<T, N> lo) {
  return ConcatUpperUpper(Simd<T, N>(), hi, lo);
}

template <typename T, size_t N>
HWY_API Vec128<T, N> ConcatLowerUpper(const Vec128<T, N> hi,
                                      const Vec128<T, N> lo) {
  return ConcatLowerUpper(Simd<T, N>(), hi, lo);
}

template <typename T, size_t N>
HWY_API Vec128<T, N> ConcatUpperLower(Vec128<T, N> hi, Vec128<T, N> lo) {
  return ConcatUpperLower(Simd<T, N>(), hi, lo);
}

// ================================================== Operator wrapper

// These apply to all x86_*-inl.h because there are no restrictions on V.

template <class V>
HWY_API V Add(V a, V b) {
  return a + b;
}
template <class V>
HWY_API V Sub(V a, V b) {
  return a - b;
}

template <class V>
HWY_API V Mul(V a, V b) {
  return a * b;
}
template <class V>
HWY_API V Div(V a, V b) {
  return a / b;
}

template <class V>
V Shl(V a, V b) {
  return a << b;
}
template <class V>
V Shr(V a, V b) {
  return a >> b;
}

template <class V>
HWY_API auto Eq(V a, V b) -> decltype(a == b) {
  return a == b;
}
template <class V>
HWY_API auto Ne(V a, V b) -> decltype(a == b) {
  return a != b;
}
template <class V>
HWY_API auto Lt(V a, V b) -> decltype(a == b) {
  return a < b;
}

template <class V>
HWY_API auto Gt(V a, V b) -> decltype(a == b) {
  return a > b;
}
template <class V>
HWY_API auto Ge(V a, V b) -> decltype(a == b) {
  return a >= b;
}

template <class V>
HWY_API auto Le(V a, V b) -> decltype(a == b) {
  return a <= b;
}

namespace detail {  // for code folding
#if HWY_ARCH_ARM_V7
#undef vuzp1_s8
#undef vuzp1_u8
#undef vuzp1_s16
#undef vuzp1_u16
#undef vuzp1_s32
#undef vuzp1_u32
#undef vuzp1_f32
#undef vuzp1q_s8
#undef vuzp1q_u8
#undef vuzp1q_s16
#undef vuzp1q_u16
#undef vuzp1q_s32
#undef vuzp1q_u32
#undef vuzp1q_f32
#undef vuzp2_s8
#undef vuzp2_u8
#undef vuzp2_s16
#undef vuzp2_u16
#undef vuzp2_s32
#undef vuzp2_u32
#undef vuzp2_f32
#undef vuzp2q_s8
#undef vuzp2q_u8
#undef vuzp2q_s16
#undef vuzp2q_u16
#undef vuzp2q_s32
#undef vuzp2q_u32
#undef vuzp2q_f32
#undef vzip1_s8
#undef vzip1_u8
#undef vzip1_s16
#undef vzip1_u16
#undef vzip1_s32
#undef vzip1_u32
#undef vzip1_f32
#undef vzip1q_s8
#undef vzip1q_u8
#undef vzip1q_s16
#undef vzip1q_u16
#undef vzip1q_s32
#undef vzip1q_u32
#undef vzip1q_f32
#undef vzip2_s8
#undef vzip2_u8
#undef vzip2_s16
#undef vzip2_u16
#undef vzip2_s32
#undef vzip2_u32
#undef vzip2_f32
#undef vzip2q_s8
#undef vzip2q_u8
#undef vzip2q_s16
#undef vzip2q_u16
#undef vzip2q_s32
#undef vzip2q_u32
#undef vzip2q_f32
#endif

#undef HWY_NEON_BUILD_ARG_1
#undef HWY_NEON_BUILD_ARG_2
#undef HWY_NEON_BUILD_ARG_3
#undef HWY_NEON_BUILD_PARAM_1
#undef HWY_NEON_BUILD_PARAM_2
#undef HWY_NEON_BUILD_PARAM_3
#undef HWY_NEON_BUILD_RET_1
#undef HWY_NEON_BUILD_RET_2
#undef HWY_NEON_BUILD_RET_3
#undef HWY_NEON_BUILD_TPL_1
#undef HWY_NEON_BUILD_TPL_2
#undef HWY_NEON_BUILD_TPL_3
#undef HWY_NEON_DEF_FUNCTION
#undef HWY_NEON_DEF_FUNCTION_ALL_FLOATS
#undef HWY_NEON_DEF_FUNCTION_ALL_TYPES
#undef HWY_NEON_DEF_FUNCTION_INT_8
#undef HWY_NEON_DEF_FUNCTION_INT_16
#undef HWY_NEON_DEF_FUNCTION_INT_32
#undef HWY_NEON_DEF_FUNCTION_INT_8_16_32
#undef HWY_NEON_DEF_FUNCTION_INTS
#undef HWY_NEON_DEF_FUNCTION_INTS_UINTS
#undef HWY_NEON_DEF_FUNCTION_TPL
#undef HWY_NEON_DEF_FUNCTION_UINT_8
#undef HWY_NEON_DEF_FUNCTION_UINT_16
#undef HWY_NEON_DEF_FUNCTION_UINT_32
#undef HWY_NEON_DEF_FUNCTION_UINT_8_16_32
#undef HWY_NEON_DEF_FUNCTION_UINTS
#undef HWY_NEON_EVAL
}  // namespace detail

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace hwy
HWY_AFTER_NAMESPACE();
