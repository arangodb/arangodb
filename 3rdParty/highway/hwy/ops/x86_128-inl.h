// Copyright 2019 Google LLC
// SPDX-License-Identifier: Apache-2.0
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

// 128-bit vectors and SSE4 instructions, plus some AVX2 and AVX512-VL
// operations when compiling for those targets.
// External include guard in highway.h - see comment there.

// Must come before HWY_DIAGNOSTICS and HWY_COMPILER_GCC_ACTUAL
#include "hwy/base.h"

// Avoid uninitialized warnings in GCC's emmintrin.h - see
// https://github.com/google/highway/issues/710 and pull/902
HWY_DIAGNOSTICS(push)
#if HWY_COMPILER_GCC_ACTUAL
HWY_DIAGNOSTICS_OFF(disable : 4700, ignored "-Wuninitialized")
HWY_DIAGNOSTICS_OFF(disable : 4701 4703 6001 26494,
                    ignored "-Wmaybe-uninitialized")
#endif

#include <emmintrin.h>
#include <stdio.h>
#if HWY_TARGET == HWY_SSSE3
#include <tmmintrin.h>  // SSSE3
#elif HWY_TARGET <= HWY_SSE4
#include <smmintrin.h>  // SSE4
#ifndef HWY_DISABLE_PCLMUL_AES
#include <wmmintrin.h>  // CLMUL
#endif
#endif
#include <string.h>  // memcpy

#include "hwy/ops/shared-inl.h"

HWY_BEFORE_NAMESPACE();
namespace hwy {
namespace HWY_NAMESPACE {
namespace detail {

template <typename T>
struct Raw128 {
  using type = __m128i;
};
template <>
struct Raw128<float> {
  using type = __m128;
};
template <>
struct Raw128<double> {
  using type = __m128d;
};

}  // namespace detail

template <typename T, size_t N = 16 / sizeof(T)>
class Vec128 {
  using Raw = typename detail::Raw128<T>::type;

 public:
  using PrivateT = T;                     // only for DFromV
  static constexpr size_t kPrivateN = N;  // only for DFromV

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

template <typename T>
using Vec64 = Vec128<T, 8 / sizeof(T)>;

template <typename T>
using Vec32 = Vec128<T, 4 / sizeof(T)>;

template <typename T>
using Vec16 = Vec128<T, 2 / sizeof(T)>;

#if HWY_TARGET <= HWY_AVX3

namespace detail {

// Template arg: sizeof(lane type)
template <size_t size>
struct RawMask128 {};
template <>
struct RawMask128<1> {
  using type = __mmask16;
};
template <>
struct RawMask128<2> {
  using type = __mmask8;
};
template <>
struct RawMask128<4> {
  using type = __mmask8;
};
template <>
struct RawMask128<8> {
  using type = __mmask8;
};

}  // namespace detail

template <typename T, size_t N = 16 / sizeof(T)>
struct Mask128 {
  using Raw = typename detail::RawMask128<sizeof(T)>::type;

  static Mask128<T, N> FromBits(uint64_t mask_bits) {
    return Mask128<T, N>{static_cast<Raw>(mask_bits)};
  }

  Raw raw;
};

#else  // AVX2 or below

// FF..FF or 0.
template <typename T, size_t N = 16 / sizeof(T)>
struct Mask128 {
  typename detail::Raw128<T>::type raw;
};

#endif  // AVX2 or below

namespace detail {

// Returns the lowest N of the _mm_movemask* bits.
template <typename T, size_t N>
constexpr uint64_t OnlyActive(uint64_t mask_bits) {
  return ((N * sizeof(T)) == 16) ? mask_bits : mask_bits & ((1ull << N) - 1);
}

}  // namespace detail

#if HWY_TARGET <= HWY_AVX3
namespace detail {

// Used by Expand() emulation, which is required for both AVX3 and AVX2.
template <typename T, size_t N>
HWY_INLINE uint64_t BitsFromMask(const Mask128<T, N> mask) {
  return OnlyActive<T, N>(mask.raw);
}

}  // namespace detail
#endif  // HWY_TARGET <= HWY_AVX3

template <class V>
using DFromV = Simd<typename V::PrivateT, V::kPrivateN, 0>;

template <class V>
using TFromV = typename V::PrivateT;

// ------------------------------ Zero

// Use HWY_MAX_LANES_D here because VFromD is defined in terms of Zero.
template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_NOT_FLOAT_D(D)>
HWY_API Vec128<TFromD<D>, HWY_MAX_LANES_D(D)> Zero(D /* tag */) {
  return Vec128<TFromD<D>, HWY_MAX_LANES_D(D)>{_mm_setzero_si128()};
}
template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_F32_D(D)>
HWY_API Vec128<float, HWY_MAX_LANES_D(D)> Zero(D /* tag */) {
  return Vec128<float, HWY_MAX_LANES_D(D)>{_mm_setzero_ps()};
}
template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_F64_D(D)>
HWY_API Vec128<double, HWY_MAX_LANES_D(D)> Zero(D /* tag */) {
  return Vec128<double, HWY_MAX_LANES_D(D)>{_mm_setzero_pd()};
}

// Using the existing Zero function instead of a dedicated function for
// deduction avoids having to forward-declare Vec256 here.
template <class D>
using VFromD = decltype(Zero(D()));

// ------------------------------ Tuple (VFromD)
#include "hwy/ops/tuple-inl.h"

// ------------------------------ BitCast

namespace detail {

HWY_INLINE __m128i BitCastToInteger(__m128i v) { return v; }
HWY_INLINE __m128i BitCastToInteger(__m128 v) { return _mm_castps_si128(v); }
HWY_INLINE __m128i BitCastToInteger(__m128d v) { return _mm_castpd_si128(v); }

template <typename T, size_t N>
HWY_INLINE Vec128<uint8_t, N * sizeof(T)> BitCastToByte(Vec128<T, N> v) {
  return Vec128<uint8_t, N * sizeof(T)>{BitCastToInteger(v.raw)};
}

// Cannot rely on function overloading because return types differ.
template <typename T>
struct BitCastFromInteger128 {
  HWY_INLINE __m128i operator()(__m128i v) { return v; }
};
template <>
struct BitCastFromInteger128<float> {
  HWY_INLINE __m128 operator()(__m128i v) { return _mm_castsi128_ps(v); }
};
template <>
struct BitCastFromInteger128<double> {
  HWY_INLINE __m128d operator()(__m128i v) { return _mm_castsi128_pd(v); }
};

template <class D>
HWY_INLINE VFromD<D> BitCastFromByte(D /* tag */,
                                     Vec128<uint8_t, D().MaxBytes()> v) {
  return VFromD<D>{BitCastFromInteger128<TFromD<D>>()(v.raw)};
}

}  // namespace detail

template <class D, typename FromT>
HWY_API VFromD<D> BitCast(D d,
                          Vec128<FromT, Repartition<FromT, D>().MaxLanes()> v) {
  return detail::BitCastFromByte(d, detail::BitCastToByte(v));
}

// ------------------------------ Set

template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_T_SIZE_D(D, 1)>
HWY_API VFromD<D> Set(D /* tag */, TFromD<D> t) {
  return VFromD<D>{_mm_set1_epi8(static_cast<char>(t))};  // NOLINT
}
template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_T_SIZE_D(D, 2)>
HWY_API VFromD<D> Set(D /* tag */, TFromD<D> t) {
  return VFromD<D>{_mm_set1_epi16(static_cast<short>(t))};  // NOLINT
}
template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_UI32_D(D)>
HWY_API VFromD<D> Set(D /* tag */, TFromD<D> t) {
  return VFromD<D>{_mm_set1_epi32(static_cast<int>(t))};
}
template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_UI64_D(D)>
HWY_API VFromD<D> Set(D /* tag */, TFromD<D> t) {
  return VFromD<D>{_mm_set1_epi64x(static_cast<long long>(t))};  // NOLINT
}
template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_F32_D(D)>
HWY_API VFromD<D> Set(D /* tag */, float t) {
  return VFromD<D>{_mm_set1_ps(t)};
}
template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_F64_D(D)>
HWY_API VFromD<D> Set(D /* tag */, double t) {
  return VFromD<D>{_mm_set1_pd(t)};
}

// ------------------------------ Undefined

HWY_DIAGNOSTICS(push)
HWY_DIAGNOSTICS_OFF(disable : 4700, ignored "-Wuninitialized")

// Returns a vector with uninitialized elements.
template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_NOT_FLOAT_D(D)>
HWY_API VFromD<D> Undefined(D /* tag */) {
  // Available on Clang 6.0, GCC 6.2, ICC 16.03, MSVC 19.14. All but ICC
  // generate an XOR instruction.
  return VFromD<D>{_mm_undefined_si128()};
}
template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_F32_D(D)>
HWY_API VFromD<D> Undefined(D /* tag */) {
  return VFromD<D>{_mm_undefined_ps()};
}
template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_F64_D(D)>
HWY_API VFromD<D> Undefined(D /* tag */) {
  return VFromD<D>{_mm_undefined_pd()};
}

HWY_DIAGNOSTICS(pop)

// ------------------------------ GetLane

template <typename T, size_t N, HWY_IF_T_SIZE(T, 1)>
HWY_API T GetLane(const Vec128<T, N> v) {
  return static_cast<T>(_mm_cvtsi128_si32(v.raw) & 0xFF);
}
template <typename T, size_t N, HWY_IF_T_SIZE(T, 2)>
HWY_API T GetLane(const Vec128<T, N> v) {
  return static_cast<T>(_mm_cvtsi128_si32(v.raw) & 0xFFFF);
}
template <typename T, size_t N, HWY_IF_T_SIZE(T, 4)>
HWY_API T GetLane(const Vec128<T, N> v) {
  return static_cast<T>(_mm_cvtsi128_si32(v.raw));
}
template <size_t N>
HWY_API float GetLane(const Vec128<float, N> v) {
  return _mm_cvtss_f32(v.raw);
}
template <typename T, size_t N, HWY_IF_T_SIZE(T, 8)>
HWY_API T GetLane(const Vec128<T, N> v) {
#if HWY_ARCH_X86_32
  const DFromV<decltype(v)> d;
  alignas(16) T lanes[2];
  Store(v, d, lanes);
  return lanes[0];
#else
  return static_cast<T>(_mm_cvtsi128_si64(v.raw));
#endif
}
template <size_t N>
HWY_API double GetLane(const Vec128<double, N> v) {
  return _mm_cvtsd_f64(v.raw);
}

// ------------------------------ ResizeBitCast

template <class D, class FromV, HWY_IF_V_SIZE_LE_V(FromV, 16),
          HWY_IF_V_SIZE_LE_D(D, 16)>
HWY_API VFromD<D> ResizeBitCast(D d, FromV v) {
  const Repartition<uint8_t, decltype(d)> du8;
  return BitCast(d, VFromD<decltype(du8)>{detail::BitCastToInteger(v.raw)});
}

// ================================================== LOGICAL

// ------------------------------ And

template <typename T, size_t N>
HWY_API Vec128<T, N> And(Vec128<T, N> a, Vec128<T, N> b) {
  return Vec128<T, N>{_mm_and_si128(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<float, N> And(const Vec128<float, N> a,
                             const Vec128<float, N> b) {
  return Vec128<float, N>{_mm_and_ps(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<double, N> And(const Vec128<double, N> a,
                              const Vec128<double, N> b) {
  return Vec128<double, N>{_mm_and_pd(a.raw, b.raw)};
}

// ------------------------------ AndNot

// Returns ~not_mask & mask.
template <typename T, size_t N>
HWY_API Vec128<T, N> AndNot(Vec128<T, N> not_mask, Vec128<T, N> mask) {
  return Vec128<T, N>{_mm_andnot_si128(not_mask.raw, mask.raw)};
}
template <size_t N>
HWY_API Vec128<float, N> AndNot(const Vec128<float, N> not_mask,
                                const Vec128<float, N> mask) {
  return Vec128<float, N>{_mm_andnot_ps(not_mask.raw, mask.raw)};
}
template <size_t N>
HWY_API Vec128<double, N> AndNot(const Vec128<double, N> not_mask,
                                 const Vec128<double, N> mask) {
  return Vec128<double, N>{_mm_andnot_pd(not_mask.raw, mask.raw)};
}

// ------------------------------ Or

template <typename T, size_t N>
HWY_API Vec128<T, N> Or(Vec128<T, N> a, Vec128<T, N> b) {
  return Vec128<T, N>{_mm_or_si128(a.raw, b.raw)};
}

template <size_t N>
HWY_API Vec128<float, N> Or(const Vec128<float, N> a,
                            const Vec128<float, N> b) {
  return Vec128<float, N>{_mm_or_ps(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<double, N> Or(const Vec128<double, N> a,
                             const Vec128<double, N> b) {
  return Vec128<double, N>{_mm_or_pd(a.raw, b.raw)};
}

// ------------------------------ Xor

template <typename T, size_t N>
HWY_API Vec128<T, N> Xor(Vec128<T, N> a, Vec128<T, N> b) {
  return Vec128<T, N>{_mm_xor_si128(a.raw, b.raw)};
}

template <size_t N>
HWY_API Vec128<float, N> Xor(const Vec128<float, N> a,
                             const Vec128<float, N> b) {
  return Vec128<float, N>{_mm_xor_ps(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<double, N> Xor(const Vec128<double, N> a,
                              const Vec128<double, N> b) {
  return Vec128<double, N>{_mm_xor_pd(a.raw, b.raw)};
}

// ------------------------------ Not
template <typename T, size_t N>
HWY_API Vec128<T, N> Not(const Vec128<T, N> v) {
  const DFromV<decltype(v)> d;
  const RebindToUnsigned<decltype(d)> du;
  using VU = VFromD<decltype(du)>;
#if HWY_TARGET <= HWY_AVX3
  const __m128i vu = BitCast(du, v).raw;
  return BitCast(d, VU{_mm_ternarylogic_epi32(vu, vu, vu, 0x55)});
#else
  return Xor(v, BitCast(d, VU{_mm_set1_epi32(-1)}));
#endif
}

// ------------------------------ Xor3
template <typename T, size_t N>
HWY_API Vec128<T, N> Xor3(Vec128<T, N> x1, Vec128<T, N> x2, Vec128<T, N> x3) {
#if HWY_TARGET <= HWY_AVX3
  const DFromV<decltype(x1)> d;
  const RebindToUnsigned<decltype(d)> du;
  using VU = VFromD<decltype(du)>;
  const __m128i ret = _mm_ternarylogic_epi64(
      BitCast(du, x1).raw, BitCast(du, x2).raw, BitCast(du, x3).raw, 0x96);
  return BitCast(d, VU{ret});
#else
  return Xor(x1, Xor(x2, x3));
#endif
}

// ------------------------------ Or3
template <typename T, size_t N>
HWY_API Vec128<T, N> Or3(Vec128<T, N> o1, Vec128<T, N> o2, Vec128<T, N> o3) {
#if HWY_TARGET <= HWY_AVX3
  const DFromV<decltype(o1)> d;
  const RebindToUnsigned<decltype(d)> du;
  using VU = VFromD<decltype(du)>;
  const __m128i ret = _mm_ternarylogic_epi64(
      BitCast(du, o1).raw, BitCast(du, o2).raw, BitCast(du, o3).raw, 0xFE);
  return BitCast(d, VU{ret});
#else
  return Or(o1, Or(o2, o3));
#endif
}

// ------------------------------ OrAnd
template <typename T, size_t N>
HWY_API Vec128<T, N> OrAnd(Vec128<T, N> o, Vec128<T, N> a1, Vec128<T, N> a2) {
#if HWY_TARGET <= HWY_AVX3
  const DFromV<decltype(o)> d;
  const RebindToUnsigned<decltype(d)> du;
  using VU = VFromD<decltype(du)>;
  const __m128i ret = _mm_ternarylogic_epi64(
      BitCast(du, o).raw, BitCast(du, a1).raw, BitCast(du, a2).raw, 0xF8);
  return BitCast(d, VU{ret});
#else
  return Or(o, And(a1, a2));
#endif
}

// ------------------------------ IfVecThenElse
template <typename T, size_t N>
HWY_API Vec128<T, N> IfVecThenElse(Vec128<T, N> mask, Vec128<T, N> yes,
                                   Vec128<T, N> no) {
#if HWY_TARGET <= HWY_AVX3
  const DFromV<decltype(no)> d;
  const RebindToUnsigned<decltype(d)> du;
  using VU = VFromD<decltype(du)>;
  return BitCast(
      d, VU{_mm_ternarylogic_epi64(BitCast(du, mask).raw, BitCast(du, yes).raw,
                                   BitCast(du, no).raw, 0xCA)});
#else
  return IfThenElse(MaskFromVec(mask), yes, no);
#endif
}

// ------------------------------ BitwiseIfThenElse
#if HWY_TARGET <= HWY_AVX3

#ifdef HWY_NATIVE_BITWISE_IF_THEN_ELSE
#undef HWY_NATIVE_BITWISE_IF_THEN_ELSE
#else
#define HWY_NATIVE_BITWISE_IF_THEN_ELSE
#endif

template <class V>
HWY_API V BitwiseIfThenElse(V mask, V yes, V no) {
  return IfVecThenElse(mask, yes, no);
}

#endif

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

// 8/16 require BITALG, 32/64 require VPOPCNTDQ.
#if HWY_TARGET <= HWY_AVX3_DL

#ifdef HWY_NATIVE_POPCNT
#undef HWY_NATIVE_POPCNT
#else
#define HWY_NATIVE_POPCNT
#endif

namespace detail {

template <typename T, size_t N>
HWY_INLINE Vec128<T, N> PopulationCount(hwy::SizeTag<1> /* tag */,
                                        Vec128<T, N> v) {
  return Vec128<T, N>{_mm_popcnt_epi8(v.raw)};
}
template <typename T, size_t N>
HWY_INLINE Vec128<T, N> PopulationCount(hwy::SizeTag<2> /* tag */,
                                        Vec128<T, N> v) {
  return Vec128<T, N>{_mm_popcnt_epi16(v.raw)};
}
template <typename T, size_t N>
HWY_INLINE Vec128<T, N> PopulationCount(hwy::SizeTag<4> /* tag */,
                                        Vec128<T, N> v) {
  return Vec128<T, N>{_mm_popcnt_epi32(v.raw)};
}
template <typename T, size_t N>
HWY_INLINE Vec128<T, N> PopulationCount(hwy::SizeTag<8> /* tag */,
                                        Vec128<T, N> v) {
  return Vec128<T, N>{_mm_popcnt_epi64(v.raw)};
}

}  // namespace detail

template <typename T, size_t N>
HWY_API Vec128<T, N> PopulationCount(Vec128<T, N> v) {
  return detail::PopulationCount(hwy::SizeTag<sizeof(T)>(), v);
}

#endif  // HWY_TARGET <= HWY_AVX3_DL

// ================================================== SIGN

// ------------------------------ Neg

// Tag dispatch instead of SFINAE for MSVC 2017 compatibility
namespace detail {

template <typename T, size_t N>
HWY_INLINE Vec128<T, N> Neg(hwy::FloatTag /*tag*/, const Vec128<T, N> v) {
  return Xor(v, SignBit(DFromV<decltype(v)>()));
}

template <typename T, size_t N>
HWY_INLINE Vec128<T, N> Neg(hwy::NonFloatTag /*tag*/, const Vec128<T, N> v) {
  return Zero(DFromV<decltype(v)>()) - v;
}

}  // namespace detail

template <typename T, size_t N>
HWY_INLINE Vec128<T, N> Neg(const Vec128<T, N> v) {
  return detail::Neg(hwy::IsFloatTag<T>(), v);
}

// ------------------------------ Floating-point Abs

// Returns absolute value
template <size_t N>
HWY_API Vec128<float, N> Abs(const Vec128<float, N> v) {
  const Vec128<int32_t, N> mask{_mm_set1_epi32(0x7FFFFFFF)};
  return v & BitCast(DFromV<decltype(v)>(), mask);
}
template <size_t N>
HWY_API Vec128<double, N> Abs(const Vec128<double, N> v) {
  const Vec128<int64_t, N> mask{_mm_set1_epi64x(0x7FFFFFFFFFFFFFFFLL)};
  return v & BitCast(DFromV<decltype(v)>(), mask);
}

// ------------------------------ CopySign

template <typename T, size_t N>
HWY_API Vec128<T, N> CopySign(const Vec128<T, N> magn,
                              const Vec128<T, N> sign) {
  static_assert(IsFloat<T>(), "Only makes sense for floating-point");

  const DFromV<decltype(magn)> d;
  const auto msb = SignBit(d);

#if HWY_TARGET <= HWY_AVX3
  const RebindToUnsigned<decltype(d)> du;
  // Truth table for msb, magn, sign | bitwise msb ? sign : mag
  //                  0    0     0   |  0
  //                  0    0     1   |  0
  //                  0    1     0   |  1
  //                  0    1     1   |  1
  //                  1    0     0   |  0
  //                  1    0     1   |  1
  //                  1    1     0   |  0
  //                  1    1     1   |  1
  // The lane size does not matter because we are not using predication.
  const __m128i out = _mm_ternarylogic_epi32(
      BitCast(du, msb).raw, BitCast(du, magn).raw, BitCast(du, sign).raw, 0xAC);
  return BitCast(d, VFromD<decltype(du)>{out});
#else
  return Or(AndNot(msb, magn), And(msb, sign));
#endif
}

template <typename T, size_t N>
HWY_API Vec128<T, N> CopySignToAbs(const Vec128<T, N> abs,
                                   const Vec128<T, N> sign) {
#if HWY_TARGET <= HWY_AVX3
  // AVX3 can also handle abs < 0, so no extra action needed.
  return CopySign(abs, sign);
#else
  return Or(abs, And(SignBit(DFromV<decltype(abs)>()), sign));
#endif
}

// ================================================== MASK

#if HWY_TARGET <= HWY_AVX3

// ------------------------------ IfThenElse

// Returns mask ? b : a.

namespace detail {

// Templates for signed/unsigned integer of a particular size.
template <typename T, size_t N>
HWY_INLINE Vec128<T, N> IfThenElse(hwy::SizeTag<1> /* tag */,
                                   Mask128<T, N> mask, Vec128<T, N> yes,
                                   Vec128<T, N> no) {
  return Vec128<T, N>{_mm_mask_mov_epi8(no.raw, mask.raw, yes.raw)};
}
template <typename T, size_t N>
HWY_INLINE Vec128<T, N> IfThenElse(hwy::SizeTag<2> /* tag */,
                                   Mask128<T, N> mask, Vec128<T, N> yes,
                                   Vec128<T, N> no) {
  return Vec128<T, N>{_mm_mask_mov_epi16(no.raw, mask.raw, yes.raw)};
}
template <typename T, size_t N>
HWY_INLINE Vec128<T, N> IfThenElse(hwy::SizeTag<4> /* tag */,
                                   Mask128<T, N> mask, Vec128<T, N> yes,
                                   Vec128<T, N> no) {
  return Vec128<T, N>{_mm_mask_mov_epi32(no.raw, mask.raw, yes.raw)};
}
template <typename T, size_t N>
HWY_INLINE Vec128<T, N> IfThenElse(hwy::SizeTag<8> /* tag */,
                                   Mask128<T, N> mask, Vec128<T, N> yes,
                                   Vec128<T, N> no) {
  return Vec128<T, N>{_mm_mask_mov_epi64(no.raw, mask.raw, yes.raw)};
}

}  // namespace detail

template <typename T, size_t N>
HWY_API Vec128<T, N> IfThenElse(Mask128<T, N> mask, Vec128<T, N> yes,
                                Vec128<T, N> no) {
  return detail::IfThenElse(hwy::SizeTag<sizeof(T)>(), mask, yes, no);
}

template <size_t N>
HWY_API Vec128<float, N> IfThenElse(Mask128<float, N> mask,
                                    Vec128<float, N> yes, Vec128<float, N> no) {
  return Vec128<float, N>{_mm_mask_mov_ps(no.raw, mask.raw, yes.raw)};
}

template <size_t N>
HWY_API Vec128<double, N> IfThenElse(Mask128<double, N> mask,
                                     Vec128<double, N> yes,
                                     Vec128<double, N> no) {
  return Vec128<double, N>{_mm_mask_mov_pd(no.raw, mask.raw, yes.raw)};
}

namespace detail {

template <typename T, size_t N>
HWY_INLINE Vec128<T, N> IfThenElseZero(hwy::SizeTag<1> /* tag */,
                                       Mask128<T, N> mask, Vec128<T, N> yes) {
  return Vec128<T, N>{_mm_maskz_mov_epi8(mask.raw, yes.raw)};
}
template <typename T, size_t N>
HWY_INLINE Vec128<T, N> IfThenElseZero(hwy::SizeTag<2> /* tag */,
                                       Mask128<T, N> mask, Vec128<T, N> yes) {
  return Vec128<T, N>{_mm_maskz_mov_epi16(mask.raw, yes.raw)};
}
template <typename T, size_t N>
HWY_INLINE Vec128<T, N> IfThenElseZero(hwy::SizeTag<4> /* tag */,
                                       Mask128<T, N> mask, Vec128<T, N> yes) {
  return Vec128<T, N>{_mm_maskz_mov_epi32(mask.raw, yes.raw)};
}
template <typename T, size_t N>
HWY_INLINE Vec128<T, N> IfThenElseZero(hwy::SizeTag<8> /* tag */,
                                       Mask128<T, N> mask, Vec128<T, N> yes) {
  return Vec128<T, N>{_mm_maskz_mov_epi64(mask.raw, yes.raw)};
}

}  // namespace detail

template <typename T, size_t N>
HWY_API Vec128<T, N> IfThenElseZero(Mask128<T, N> mask, Vec128<T, N> yes) {
  return detail::IfThenElseZero(hwy::SizeTag<sizeof(T)>(), mask, yes);
}

template <size_t N>
HWY_API Vec128<float, N> IfThenElseZero(Mask128<float, N> mask,
                                        Vec128<float, N> yes) {
  return Vec128<float, N>{_mm_maskz_mov_ps(mask.raw, yes.raw)};
}

template <size_t N>
HWY_API Vec128<double, N> IfThenElseZero(Mask128<double, N> mask,
                                         Vec128<double, N> yes) {
  return Vec128<double, N>{_mm_maskz_mov_pd(mask.raw, yes.raw)};
}

namespace detail {

template <typename T, size_t N>
HWY_INLINE Vec128<T, N> IfThenZeroElse(hwy::SizeTag<1> /* tag */,
                                       Mask128<T, N> mask, Vec128<T, N> no) {
  // xor_epi8/16 are missing, but we have sub, which is just as fast for u8/16.
  return Vec128<T, N>{_mm_mask_sub_epi8(no.raw, mask.raw, no.raw, no.raw)};
}
template <typename T, size_t N>
HWY_INLINE Vec128<T, N> IfThenZeroElse(hwy::SizeTag<2> /* tag */,
                                       Mask128<T, N> mask, Vec128<T, N> no) {
  return Vec128<T, N>{_mm_mask_sub_epi16(no.raw, mask.raw, no.raw, no.raw)};
}
template <typename T, size_t N>
HWY_INLINE Vec128<T, N> IfThenZeroElse(hwy::SizeTag<4> /* tag */,
                                       Mask128<T, N> mask, Vec128<T, N> no) {
  return Vec128<T, N>{_mm_mask_xor_epi32(no.raw, mask.raw, no.raw, no.raw)};
}
template <typename T, size_t N>
HWY_INLINE Vec128<T, N> IfThenZeroElse(hwy::SizeTag<8> /* tag */,
                                       Mask128<T, N> mask, Vec128<T, N> no) {
  return Vec128<T, N>{_mm_mask_xor_epi64(no.raw, mask.raw, no.raw, no.raw)};
}

}  // namespace detail

template <typename T, size_t N>
HWY_API Vec128<T, N> IfThenZeroElse(Mask128<T, N> mask, Vec128<T, N> no) {
  return detail::IfThenZeroElse(hwy::SizeTag<sizeof(T)>(), mask, no);
}

template <size_t N>
HWY_API Vec128<float, N> IfThenZeroElse(Mask128<float, N> mask,
                                        Vec128<float, N> no) {
  return Vec128<float, N>{_mm_mask_xor_ps(no.raw, mask.raw, no.raw, no.raw)};
}

template <size_t N>
HWY_API Vec128<double, N> IfThenZeroElse(Mask128<double, N> mask,
                                         Vec128<double, N> no) {
  return Vec128<double, N>{_mm_mask_xor_pd(no.raw, mask.raw, no.raw, no.raw)};
}

// ------------------------------ Mask logical

// For Clang and GCC, mask intrinsics (KORTEST) weren't added until recently.
#if !defined(HWY_COMPILER_HAS_MASK_INTRINSICS)
#if HWY_COMPILER_MSVC != 0 || HWY_COMPILER_GCC_ACTUAL >= 700 || \
    HWY_COMPILER_CLANG >= 800
#define HWY_COMPILER_HAS_MASK_INTRINSICS 1
#else
#define HWY_COMPILER_HAS_MASK_INTRINSICS 0
#endif
#endif  // HWY_COMPILER_HAS_MASK_INTRINSICS

namespace detail {

template <typename T, size_t N>
HWY_INLINE Mask128<T, N> And(hwy::SizeTag<1> /*tag*/, const Mask128<T, N> a,
                             const Mask128<T, N> b) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return Mask128<T, N>{_kand_mask16(a.raw, b.raw)};
#else
  return Mask128<T, N>{static_cast<__mmask16>(a.raw & b.raw)};
#endif
}
template <typename T, size_t N>
HWY_INLINE Mask128<T, N> And(hwy::SizeTag<2> /*tag*/, const Mask128<T, N> a,
                             const Mask128<T, N> b) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return Mask128<T, N>{_kand_mask8(a.raw, b.raw)};
#else
  return Mask128<T, N>{static_cast<__mmask8>(a.raw & b.raw)};
#endif
}
template <typename T, size_t N>
HWY_INLINE Mask128<T, N> And(hwy::SizeTag<4> /*tag*/, const Mask128<T, N> a,
                             const Mask128<T, N> b) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return Mask128<T, N>{_kand_mask8(a.raw, b.raw)};
#else
  return Mask128<T, N>{static_cast<__mmask8>(a.raw & b.raw)};
#endif
}
template <typename T, size_t N>
HWY_INLINE Mask128<T, N> And(hwy::SizeTag<8> /*tag*/, const Mask128<T, N> a,
                             const Mask128<T, N> b) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return Mask128<T, N>{_kand_mask8(a.raw, b.raw)};
#else
  return Mask128<T, N>{static_cast<__mmask8>(a.raw & b.raw)};
#endif
}

template <typename T, size_t N>
HWY_INLINE Mask128<T, N> AndNot(hwy::SizeTag<1> /*tag*/, const Mask128<T, N> a,
                                const Mask128<T, N> b) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return Mask128<T, N>{_kandn_mask16(a.raw, b.raw)};
#else
  return Mask128<T, N>{static_cast<__mmask16>(~a.raw & b.raw)};
#endif
}
template <typename T, size_t N>
HWY_INLINE Mask128<T, N> AndNot(hwy::SizeTag<2> /*tag*/, const Mask128<T, N> a,
                                const Mask128<T, N> b) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return Mask128<T, N>{_kandn_mask8(a.raw, b.raw)};
#else
  return Mask128<T, N>{static_cast<__mmask8>(~a.raw & b.raw)};
#endif
}
template <typename T, size_t N>
HWY_INLINE Mask128<T, N> AndNot(hwy::SizeTag<4> /*tag*/, const Mask128<T, N> a,
                                const Mask128<T, N> b) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return Mask128<T, N>{_kandn_mask8(a.raw, b.raw)};
#else
  return Mask128<T, N>{static_cast<__mmask8>(~a.raw & b.raw)};
#endif
}
template <typename T, size_t N>
HWY_INLINE Mask128<T, N> AndNot(hwy::SizeTag<8> /*tag*/, const Mask128<T, N> a,
                                const Mask128<T, N> b) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return Mask128<T, N>{_kandn_mask8(a.raw, b.raw)};
#else
  return Mask128<T, N>{static_cast<__mmask8>(~a.raw & b.raw)};
#endif
}

template <typename T, size_t N>
HWY_INLINE Mask128<T, N> Or(hwy::SizeTag<1> /*tag*/, const Mask128<T, N> a,
                            const Mask128<T, N> b) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return Mask128<T, N>{_kor_mask16(a.raw, b.raw)};
#else
  return Mask128<T, N>{static_cast<__mmask16>(a.raw | b.raw)};
#endif
}
template <typename T, size_t N>
HWY_INLINE Mask128<T, N> Or(hwy::SizeTag<2> /*tag*/, const Mask128<T, N> a,
                            const Mask128<T, N> b) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return Mask128<T, N>{_kor_mask8(a.raw, b.raw)};
#else
  return Mask128<T, N>{static_cast<__mmask8>(a.raw | b.raw)};
#endif
}
template <typename T, size_t N>
HWY_INLINE Mask128<T, N> Or(hwy::SizeTag<4> /*tag*/, const Mask128<T, N> a,
                            const Mask128<T, N> b) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return Mask128<T, N>{_kor_mask8(a.raw, b.raw)};
#else
  return Mask128<T, N>{static_cast<__mmask8>(a.raw | b.raw)};
#endif
}
template <typename T, size_t N>
HWY_INLINE Mask128<T, N> Or(hwy::SizeTag<8> /*tag*/, const Mask128<T, N> a,
                            const Mask128<T, N> b) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return Mask128<T, N>{_kor_mask8(a.raw, b.raw)};
#else
  return Mask128<T, N>{static_cast<__mmask8>(a.raw | b.raw)};
#endif
}

template <typename T, size_t N>
HWY_INLINE Mask128<T, N> Xor(hwy::SizeTag<1> /*tag*/, const Mask128<T, N> a,
                             const Mask128<T, N> b) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return Mask128<T, N>{_kxor_mask16(a.raw, b.raw)};
#else
  return Mask128<T, N>{static_cast<__mmask16>(a.raw ^ b.raw)};
#endif
}
template <typename T, size_t N>
HWY_INLINE Mask128<T, N> Xor(hwy::SizeTag<2> /*tag*/, const Mask128<T, N> a,
                             const Mask128<T, N> b) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return Mask128<T, N>{_kxor_mask8(a.raw, b.raw)};
#else
  return Mask128<T, N>{static_cast<__mmask8>(a.raw ^ b.raw)};
#endif
}
template <typename T, size_t N>
HWY_INLINE Mask128<T, N> Xor(hwy::SizeTag<4> /*tag*/, const Mask128<T, N> a,
                             const Mask128<T, N> b) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return Mask128<T, N>{_kxor_mask8(a.raw, b.raw)};
#else
  return Mask128<T, N>{static_cast<__mmask8>(a.raw ^ b.raw)};
#endif
}
template <typename T, size_t N>
HWY_INLINE Mask128<T, N> Xor(hwy::SizeTag<8> /*tag*/, const Mask128<T, N> a,
                             const Mask128<T, N> b) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return Mask128<T, N>{_kxor_mask8(a.raw, b.raw)};
#else
  return Mask128<T, N>{static_cast<__mmask8>(a.raw ^ b.raw)};
#endif
}

template <typename T, size_t N>
HWY_INLINE Mask128<T, N> ExclusiveNeither(hwy::SizeTag<1> /*tag*/,
                                          const Mask128<T, N> a,
                                          const Mask128<T, N> b) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return Mask128<T, N>{_kxnor_mask16(a.raw, b.raw)};
#else
  return Mask128<T, N>{static_cast<__mmask16>(~(a.raw ^ b.raw) & 0xFFFF)};
#endif
}
template <typename T, size_t N>
HWY_INLINE Mask128<T, N> ExclusiveNeither(hwy::SizeTag<2> /*tag*/,
                                          const Mask128<T, N> a,
                                          const Mask128<T, N> b) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return Mask128<T, N>{_kxnor_mask8(a.raw, b.raw)};
#else
  return Mask128<T, N>{static_cast<__mmask8>(~(a.raw ^ b.raw) & 0xFF)};
#endif
}
template <typename T, size_t N>
HWY_INLINE Mask128<T, N> ExclusiveNeither(hwy::SizeTag<4> /*tag*/,
                                          const Mask128<T, N> a,
                                          const Mask128<T, N> b) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return Mask128<T, N>{static_cast<__mmask8>(_kxnor_mask8(a.raw, b.raw) & 0xF)};
#else
  return Mask128<T, N>{static_cast<__mmask8>(~(a.raw ^ b.raw) & 0xF)};
#endif
}
template <typename T, size_t N>
HWY_INLINE Mask128<T, N> ExclusiveNeither(hwy::SizeTag<8> /*tag*/,
                                          const Mask128<T, N> a,
                                          const Mask128<T, N> b) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return Mask128<T, N>{static_cast<__mmask8>(_kxnor_mask8(a.raw, b.raw) & 0x3)};
#else
  return Mask128<T, N>{static_cast<__mmask8>(~(a.raw ^ b.raw) & 0x3)};
#endif
}

}  // namespace detail

template <typename T, size_t N>
HWY_API Mask128<T, N> And(const Mask128<T, N> a, Mask128<T, N> b) {
  return detail::And(hwy::SizeTag<sizeof(T)>(), a, b);
}

template <typename T, size_t N>
HWY_API Mask128<T, N> AndNot(const Mask128<T, N> a, Mask128<T, N> b) {
  return detail::AndNot(hwy::SizeTag<sizeof(T)>(), a, b);
}

template <typename T, size_t N>
HWY_API Mask128<T, N> Or(const Mask128<T, N> a, Mask128<T, N> b) {
  return detail::Or(hwy::SizeTag<sizeof(T)>(), a, b);
}

template <typename T, size_t N>
HWY_API Mask128<T, N> Xor(const Mask128<T, N> a, Mask128<T, N> b) {
  return detail::Xor(hwy::SizeTag<sizeof(T)>(), a, b);
}

template <typename T, size_t N>
HWY_API Mask128<T, N> Not(const Mask128<T, N> m) {
  // Flip only the valid bits.
  // TODO(janwas): use _knot intrinsics if N >= 8.
  return Xor(m, Mask128<T, N>::FromBits((1ull << N) - 1));
}

template <typename T, size_t N>
HWY_API Mask128<T, N> ExclusiveNeither(const Mask128<T, N> a, Mask128<T, N> b) {
  return detail::ExclusiveNeither(hwy::SizeTag<sizeof(T)>(), a, b);
}

#else  // AVX2 or below

// ------------------------------ Mask

// Mask and Vec are the same (true = FF..FF).
template <typename T, size_t N>
HWY_API Mask128<T, N> MaskFromVec(const Vec128<T, N> v) {
  return Mask128<T, N>{v.raw};
}

template <class D>
using MFromD = decltype(MaskFromVec(VFromD<D>()));

template <typename T, size_t N>
HWY_API Vec128<T, N> VecFromMask(const Mask128<T, N> v) {
  return Vec128<T, N>{v.raw};
}

template <class D>
HWY_API VFromD<D> VecFromMask(D /* tag */, MFromD<D> v) {
  return VFromD<D>{v.raw};
}

#if HWY_TARGET >= HWY_SSSE3

// mask ? yes : no
template <typename T, size_t N>
HWY_API Vec128<T, N> IfThenElse(Mask128<T, N> mask, Vec128<T, N> yes,
                                Vec128<T, N> no) {
  const auto vmask = VecFromMask(DFromV<decltype(no)>(), mask);
  return Or(And(vmask, yes), AndNot(vmask, no));
}

#else  // HWY_TARGET < HWY_SSSE3

// mask ? yes : no
template <typename T, size_t N>
HWY_API Vec128<T, N> IfThenElse(Mask128<T, N> mask, Vec128<T, N> yes,
                                Vec128<T, N> no) {
  return Vec128<T, N>{_mm_blendv_epi8(no.raw, yes.raw, mask.raw)};
}
template <size_t N>
HWY_API Vec128<float, N> IfThenElse(const Mask128<float, N> mask,
                                    const Vec128<float, N> yes,
                                    const Vec128<float, N> no) {
  return Vec128<float, N>{_mm_blendv_ps(no.raw, yes.raw, mask.raw)};
}
template <size_t N>
HWY_API Vec128<double, N> IfThenElse(const Mask128<double, N> mask,
                                     const Vec128<double, N> yes,
                                     const Vec128<double, N> no) {
  return Vec128<double, N>{_mm_blendv_pd(no.raw, yes.raw, mask.raw)};
}

#endif  // HWY_TARGET >= HWY_SSSE3

// mask ? yes : 0
template <typename T, size_t N>
HWY_API Vec128<T, N> IfThenElseZero(Mask128<T, N> mask, Vec128<T, N> yes) {
  return yes & VecFromMask(DFromV<decltype(yes)>(), mask);
}

// mask ? 0 : no
template <typename T, size_t N>
HWY_API Vec128<T, N> IfThenZeroElse(Mask128<T, N> mask, Vec128<T, N> no) {
  return AndNot(VecFromMask(DFromV<decltype(no)>(), mask), no);
}

// ------------------------------ Mask logical

template <typename T, size_t N>
HWY_API Mask128<T, N> Not(const Mask128<T, N> m) {
  const Simd<T, N, 0> d;
  return MaskFromVec(Not(VecFromMask(d, m)));
}

template <typename T, size_t N>
HWY_API Mask128<T, N> And(const Mask128<T, N> a, Mask128<T, N> b) {
  const Simd<T, N, 0> d;
  return MaskFromVec(And(VecFromMask(d, a), VecFromMask(d, b)));
}

template <typename T, size_t N>
HWY_API Mask128<T, N> AndNot(const Mask128<T, N> a, Mask128<T, N> b) {
  const Simd<T, N, 0> d;
  return MaskFromVec(AndNot(VecFromMask(d, a), VecFromMask(d, b)));
}

template <typename T, size_t N>
HWY_API Mask128<T, N> Or(const Mask128<T, N> a, Mask128<T, N> b) {
  const Simd<T, N, 0> d;
  return MaskFromVec(Or(VecFromMask(d, a), VecFromMask(d, b)));
}

template <typename T, size_t N>
HWY_API Mask128<T, N> Xor(const Mask128<T, N> a, Mask128<T, N> b) {
  const Simd<T, N, 0> d;
  return MaskFromVec(Xor(VecFromMask(d, a), VecFromMask(d, b)));
}

template <typename T, size_t N>
HWY_API Mask128<T, N> ExclusiveNeither(const Mask128<T, N> a, Mask128<T, N> b) {
  const Simd<T, N, 0> d;
  return MaskFromVec(AndNot(VecFromMask(d, a), Not(VecFromMask(d, b))));
}

#endif  // HWY_TARGET <= HWY_AVX3

// ------------------------------ ShiftLeft

template <int kBits, size_t N>
HWY_API Vec128<uint16_t, N> ShiftLeft(const Vec128<uint16_t, N> v) {
  return Vec128<uint16_t, N>{_mm_slli_epi16(v.raw, kBits)};
}

template <int kBits, size_t N>
HWY_API Vec128<uint32_t, N> ShiftLeft(const Vec128<uint32_t, N> v) {
  return Vec128<uint32_t, N>{_mm_slli_epi32(v.raw, kBits)};
}

template <int kBits, size_t N>
HWY_API Vec128<uint64_t, N> ShiftLeft(const Vec128<uint64_t, N> v) {
  return Vec128<uint64_t, N>{_mm_slli_epi64(v.raw, kBits)};
}

template <int kBits, size_t N>
HWY_API Vec128<int16_t, N> ShiftLeft(const Vec128<int16_t, N> v) {
  return Vec128<int16_t, N>{_mm_slli_epi16(v.raw, kBits)};
}
template <int kBits, size_t N>
HWY_API Vec128<int32_t, N> ShiftLeft(const Vec128<int32_t, N> v) {
  return Vec128<int32_t, N>{_mm_slli_epi32(v.raw, kBits)};
}
template <int kBits, size_t N>
HWY_API Vec128<int64_t, N> ShiftLeft(const Vec128<int64_t, N> v) {
  return Vec128<int64_t, N>{_mm_slli_epi64(v.raw, kBits)};
}

template <int kBits, typename T, size_t N, HWY_IF_T_SIZE(T, 1)>
HWY_API Vec128<T, N> ShiftLeft(const Vec128<T, N> v) {
  const DFromV<decltype(v)> d8;
  // Use raw instead of BitCast to support N=1.
  const Vec128<T, N> shifted{ShiftLeft<kBits>(Vec128<MakeWide<T>>{v.raw}).raw};
  return kBits == 1
             ? (v + v)
             : (shifted & Set(d8, static_cast<T>((0xFF << kBits) & 0xFF)));
}

// ------------------------------ ShiftRight

template <int kBits, size_t N>
HWY_API Vec128<uint16_t, N> ShiftRight(const Vec128<uint16_t, N> v) {
  return Vec128<uint16_t, N>{_mm_srli_epi16(v.raw, kBits)};
}
template <int kBits, size_t N>
HWY_API Vec128<uint32_t, N> ShiftRight(const Vec128<uint32_t, N> v) {
  return Vec128<uint32_t, N>{_mm_srli_epi32(v.raw, kBits)};
}
template <int kBits, size_t N>
HWY_API Vec128<uint64_t, N> ShiftRight(const Vec128<uint64_t, N> v) {
  return Vec128<uint64_t, N>{_mm_srli_epi64(v.raw, kBits)};
}

template <int kBits, size_t N>
HWY_API Vec128<uint8_t, N> ShiftRight(const Vec128<uint8_t, N> v) {
  const DFromV<decltype(v)> d8;
  // Use raw instead of BitCast to support N=1.
  const Vec128<uint8_t, N> shifted{
      ShiftRight<kBits>(Vec128<uint16_t>{v.raw}).raw};
  return shifted & Set(d8, 0xFF >> kBits);
}

template <int kBits, size_t N>
HWY_API Vec128<int16_t, N> ShiftRight(const Vec128<int16_t, N> v) {
  return Vec128<int16_t, N>{_mm_srai_epi16(v.raw, kBits)};
}
template <int kBits, size_t N>
HWY_API Vec128<int32_t, N> ShiftRight(const Vec128<int32_t, N> v) {
  return Vec128<int32_t, N>{_mm_srai_epi32(v.raw, kBits)};
}

template <int kBits, size_t N>
HWY_API Vec128<int8_t, N> ShiftRight(const Vec128<int8_t, N> v) {
  const DFromV<decltype(v)> di;
  const RebindToUnsigned<decltype(di)> du;
  const auto shifted = BitCast(di, ShiftRight<kBits>(BitCast(du, v)));
  const auto shifted_sign = BitCast(di, Set(du, 0x80 >> kBits));
  return (shifted ^ shifted_sign) - shifted_sign;
}

// i64 is implemented after BroadcastSignBit.

// ================================================== MEMORY (1)

// Clang static analysis claims the memory immediately after a partial vector
// store is uninitialized, and also flags the input to partial loads (at least
// for loadl_pd) as "garbage". This is a false alarm because msan does not
// raise errors. We work around this by using CopyBytes instead of intrinsics,
// but only for the analyzer to avoid potentially bad code generation.
// Unfortunately __clang_analyzer__ was not defined for clang-tidy prior to v7.
#ifndef HWY_SAFE_PARTIAL_LOAD_STORE
#if defined(__clang_analyzer__) || \
    (HWY_COMPILER_CLANG != 0 && HWY_COMPILER_CLANG < 700)
#define HWY_SAFE_PARTIAL_LOAD_STORE 1
#else
#define HWY_SAFE_PARTIAL_LOAD_STORE 0
#endif
#endif  // HWY_SAFE_PARTIAL_LOAD_STORE

// ------------------------------ Load

template <class D, HWY_IF_V_SIZE_D(D, 16), typename T = TFromD<D>>
HWY_API Vec128<T> Load(D /* tag */, const T* HWY_RESTRICT aligned) {
  return Vec128<T>{_mm_load_si128(reinterpret_cast<const __m128i*>(aligned))};
}
template <class D, HWY_IF_V_SIZE_D(D, 16), HWY_IF_F32_D(D)>
HWY_API Vec128<float> Load(D /* tag */, const float* HWY_RESTRICT aligned) {
  return Vec128<float>{_mm_load_ps(aligned)};
}
template <class D, HWY_IF_V_SIZE_D(D, 16), HWY_IF_F64_D(D)>
HWY_API Vec128<double> Load(D /* tag */, const double* HWY_RESTRICT aligned) {
  return Vec128<double>{_mm_load_pd(aligned)};
}

template <class D, HWY_IF_V_SIZE_D(D, 16), typename T = TFromD<D>>
HWY_API Vec128<T> LoadU(D /* tag */, const T* HWY_RESTRICT p) {
  return Vec128<T>{_mm_loadu_si128(reinterpret_cast<const __m128i*>(p))};
}
template <class D, HWY_IF_V_SIZE_D(D, 16), HWY_IF_F32_D(D)>
HWY_API Vec128<float> LoadU(D /* tag */, const float* HWY_RESTRICT p) {
  return Vec128<float>{_mm_loadu_ps(p)};
}
template <class D, HWY_IF_V_SIZE_D(D, 16), HWY_IF_F64_D(D)>
HWY_API Vec128<double> LoadU(D /* tag */, const double* HWY_RESTRICT p) {
  return Vec128<double>{_mm_loadu_pd(p)};
}

template <class D, HWY_IF_V_SIZE_D(D, 8), typename T = TFromD<D>>
HWY_API Vec64<T> Load(D /* tag */, const T* HWY_RESTRICT p) {
#if HWY_SAFE_PARTIAL_LOAD_STORE
  __m128i v = _mm_setzero_si128();
  CopyBytes<8>(p, &v);  // not same size
  return Vec64<T>{v};
#else
  return Vec64<T>{_mm_loadl_epi64(reinterpret_cast<const __m128i*>(p))};
#endif
}

template <class D, HWY_IF_V_SIZE_D(D, 8), HWY_IF_F32_D(D)>
HWY_API Vec64<float> Load(D /* tag */, const float* HWY_RESTRICT p) {
#if HWY_SAFE_PARTIAL_LOAD_STORE
  __m128 v = _mm_setzero_ps();
  CopyBytes<8>(p, &v);  // not same size
  return Vec64<float>{v};
#else
  const __m128 hi = _mm_setzero_ps();
  return Vec64<float>{_mm_loadl_pi(hi, reinterpret_cast<const __m64*>(p))};
#endif
}

template <class D, HWY_IF_V_SIZE_D(D, 8), HWY_IF_F64_D(D)>
HWY_API Vec64<double> Load(D /* tag */, const double* HWY_RESTRICT p) {
#if HWY_SAFE_PARTIAL_LOAD_STORE
  __m128d v = _mm_setzero_pd();
  CopyBytes<8>(p, &v);  // not same size
  return Vec64<double>{v};
#else
  return Vec64<double>{_mm_load_sd(p)};
#endif
}

template <class D, HWY_IF_V_SIZE_D(D, 4), HWY_IF_F32_D(D)>
HWY_API Vec32<float> Load(D /* tag */, const float* HWY_RESTRICT p) {
#if HWY_SAFE_PARTIAL_LOAD_STORE
  __m128 v = _mm_setzero_ps();
  CopyBytes<4>(p, &v);  // not same size
  return Vec32<float>{v};
#else
  return Vec32<float>{_mm_load_ss(p)};
#endif
}

// Any <= 32 bit except <float, 1>
template <class D, HWY_IF_V_SIZE_LE_D(D, 4), typename T = TFromD<D>>
HWY_API VFromD<D> Load(D d, const T* HWY_RESTRICT p) {
  // Clang ArgumentPromotionPass seems to break this code. We can unpoison
  // before SetTableIndices -> LoadU -> Load and the memory is poisoned again.
  detail::MaybeUnpoison(p, Lanes(d));

#if HWY_SAFE_PARTIAL_LOAD_STORE
  Vec128<T> v = Zero(Full128<T>());
  CopyBytes<d.MaxBytes()>(p, &v.raw);  // not same size as VFromD
  return VFromD<D>{v.raw};
#else
  int32_t bits = 0;
  CopyBytes<d.MaxBytes()>(p, &bits);  // not same size as VFromD
  return VFromD<D>{_mm_cvtsi32_si128(bits)};
#endif
}

// For < 128 bit, LoadU == Load.
template <class D, HWY_IF_V_SIZE_LE_D(D, 8), typename T = TFromD<D>>
HWY_API VFromD<D> LoadU(D d, const T* HWY_RESTRICT p) {
  return Load(d, p);
}

// 128-bit SIMD => nothing to duplicate, same as an unaligned load.
template <class D, HWY_IF_V_SIZE_LE_D(D, 16), typename T = TFromD<D>>
HWY_API VFromD<D> LoadDup128(D d, const T* HWY_RESTRICT p) {
  return LoadU(d, p);
}

// ------------------------------ Store

template <class D, HWY_IF_V_SIZE_D(D, 16), typename T = TFromD<D>>
HWY_API void Store(Vec128<T> v, D /* tag */, T* HWY_RESTRICT aligned) {
  _mm_store_si128(reinterpret_cast<__m128i*>(aligned), v.raw);
}
template <class D, HWY_IF_V_SIZE_D(D, 16), HWY_IF_F32_D(D)>
HWY_API void Store(Vec128<float> v, D /* tag */, float* HWY_RESTRICT aligned) {
  _mm_store_ps(aligned, v.raw);
}
template <class D, HWY_IF_V_SIZE_D(D, 16), HWY_IF_F64_D(D)>
HWY_API void Store(Vec128<double> v, D /* tag */,
                   double* HWY_RESTRICT aligned) {
  _mm_store_pd(aligned, v.raw);
}

template <class D, HWY_IF_V_SIZE_D(D, 16), typename T = TFromD<D>>
HWY_API void StoreU(Vec128<T> v, D /* tag */, T* HWY_RESTRICT p) {
  _mm_storeu_si128(reinterpret_cast<__m128i*>(p), v.raw);
}
template <class D, HWY_IF_V_SIZE_D(D, 16), HWY_IF_F32_D(D)>
HWY_API void StoreU(Vec128<float> v, D /* tag */, float* HWY_RESTRICT p) {
  _mm_storeu_ps(p, v.raw);
}
template <class D, HWY_IF_V_SIZE_D(D, 16), HWY_IF_F64_D(D)>
HWY_API void StoreU(Vec128<double> v, D /* tag */, double* HWY_RESTRICT p) {
  _mm_storeu_pd(p, v.raw);
}

template <class D, HWY_IF_V_SIZE_D(D, 8), typename T = TFromD<D>>
HWY_API void Store(Vec64<T> v, D /* tag */, T* HWY_RESTRICT p) {
#if HWY_SAFE_PARTIAL_LOAD_STORE
  CopyBytes<8>(&v, p);  // not same size
#else
  _mm_storel_epi64(reinterpret_cast<__m128i*>(p), v.raw);
#endif
}
template <class D, HWY_IF_V_SIZE_D(D, 8), HWY_IF_F32_D(D)>
HWY_API void Store(Vec64<float> v, D /* tag */, float* HWY_RESTRICT p) {
#if HWY_SAFE_PARTIAL_LOAD_STORE
  CopyBytes<8>(&v, p);  // not same size
#else
  _mm_storel_pi(reinterpret_cast<__m64*>(p), v.raw);
#endif
}
template <class D, HWY_IF_V_SIZE_D(D, 8), HWY_IF_F64_D(D)>
HWY_API void Store(Vec64<double> v, D /* tag */, double* HWY_RESTRICT p) {
#if HWY_SAFE_PARTIAL_LOAD_STORE
  CopyBytes<8>(&v, p);  // not same size
#else
  _mm_storel_pd(p, v.raw);
#endif
}

// Any <= 32 bit except <float, 1>
template <class D, HWY_IF_V_SIZE_LE_D(D, 4), typename T = TFromD<D>>
HWY_API void Store(VFromD<D> v, D d, T* HWY_RESTRICT p) {
  CopyBytes<d.MaxBytes()>(&v, p);  // not same size
}
template <class D, HWY_IF_V_SIZE_D(D, 4), HWY_IF_F32_D(D)>
HWY_API void Store(Vec32<float> v, D /* tag */, float* HWY_RESTRICT p) {
#if HWY_SAFE_PARTIAL_LOAD_STORE
  CopyBytes<4>(&v, p);  // not same size
#else
  _mm_store_ss(p, v.raw);
#endif
}

// For < 128 bit, StoreU == Store.
template <class D, HWY_IF_V_SIZE_LE_D(D, 8), typename T = TFromD<D>>
HWY_API void StoreU(VFromD<D> v, D d, T* HWY_RESTRICT p) {
  Store(v, d, p);
}

// ================================================== SWIZZLE (1)

// ------------------------------ TableLookupBytes
template <typename T, size_t N, typename TI, size_t NI>
HWY_API Vec128<TI, NI> TableLookupBytes(const Vec128<T, N> bytes,
                                        const Vec128<TI, NI> from) {
#if HWY_TARGET == HWY_SSE2
#if HWY_COMPILER_GCC_ACTUAL && HWY_HAS_BUILTIN(__builtin_shuffle)
  typedef uint8_t GccU8RawVectType __attribute__((__vector_size__(16)));
  return Vec128<TI, NI>{reinterpret_cast<typename detail::Raw128<TI>::type>(
      __builtin_shuffle(reinterpret_cast<GccU8RawVectType>(bytes.raw),
                        reinterpret_cast<GccU8RawVectType>(from.raw)))};
#else
  const DFromV<decltype(from)> d;
  const Repartition<uint8_t, decltype(d)> du8;
  const Full128<uint8_t> du8_full;

  const DFromV<decltype(bytes)> d_bytes;
  const Repartition<uint8_t, decltype(d_bytes)> du8_bytes;

  alignas(16) uint8_t result_bytes[16];
  alignas(16) uint8_t u8_bytes[16];
  alignas(16) uint8_t from_bytes[16];

  Store(Vec128<uint8_t>{BitCast(du8_bytes, bytes).raw}, du8_full, u8_bytes);
  Store(Vec128<uint8_t>{BitCast(du8, from).raw}, du8_full, from_bytes);

  for (int i = 0; i < 16; i++) {
    result_bytes[i] = u8_bytes[from_bytes[i] & 15];
  }

  return BitCast(d, VFromD<decltype(du8)>{Load(du8_full, result_bytes).raw});
#endif
#else  // SSSE3 or newer
  return Vec128<TI, NI>{_mm_shuffle_epi8(bytes.raw, from.raw)};
#endif
}

// ------------------------------ TableLookupBytesOr0
// For all vector widths; x86 anyway zeroes if >= 0x80 on SSSE3/SSE4/AVX2/AVX3
template <class V, class VI>
HWY_API VI TableLookupBytesOr0(const V bytes, const VI from) {
#if HWY_TARGET == HWY_SSE2
  const DFromV<decltype(from)> d;
  const Repartition<int8_t, decltype(d)> di8;

  const auto di8_from = BitCast(di8, from);
  return BitCast(d, IfThenZeroElse(di8_from < Zero(di8),
                                   TableLookupBytes(bytes, di8_from)));
#else
  return TableLookupBytes(bytes, from);
#endif
}

// ------------------------------ Shuffles (ShiftRight, TableLookupBytes)

// Notation: let Vec128<int32_t> have lanes 3,2,1,0 (0 is least-significant).
// Shuffle0321 rotates one lane to the right (the previous least-significant
// lane is now most-significant). These could also be implemented via
// CombineShiftRightBytes but the shuffle_abcd notation is more convenient.

// Swap 32-bit halves in 64-bit halves.
template <typename T, size_t N>
HWY_API Vec128<T, N> Shuffle2301(const Vec128<T, N> v) {
  static_assert(sizeof(T) == 4, "Only for 32-bit lanes");
  static_assert(N == 2 || N == 4, "Does not make sense for N=1");
  return Vec128<T, N>{_mm_shuffle_epi32(v.raw, 0xB1)};
}
template <size_t N>
HWY_API Vec128<float, N> Shuffle2301(const Vec128<float, N> v) {
  static_assert(N == 2 || N == 4, "Does not make sense for N=1");
  return Vec128<float, N>{_mm_shuffle_ps(v.raw, v.raw, 0xB1)};
}

// These are used by generic_ops-inl to implement LoadInterleaved3. As with
// Intel's shuffle* intrinsics and InterleaveLower, the lower half of the output
// comes from the first argument.
namespace detail {

template <typename T, HWY_IF_T_SIZE(T, 1)>
HWY_API Vec32<T> ShuffleTwo2301(const Vec32<T> a, const Vec32<T> b) {
  const DFromV<decltype(a)> d;
  const Twice<decltype(d)> d2;
  const auto ba = Combine(d2, b, a);
#if HWY_TARGET == HWY_SSE2
  Vec32<uint16_t> ba_shuffled{
      _mm_shufflelo_epi16(ba.raw, _MM_SHUFFLE(3, 0, 3, 0))};
  return BitCast(d, Or(ShiftLeft<8>(ba_shuffled), ShiftRight<8>(ba_shuffled)));
#else
  alignas(16) const T kShuffle[8] = {1, 0, 7, 6};
  return Vec32<T>{TableLookupBytes(ba, Load(d2, kShuffle)).raw};
#endif
}
template <typename T, HWY_IF_T_SIZE(T, 2)>
HWY_API Vec64<T> ShuffleTwo2301(const Vec64<T> a, const Vec64<T> b) {
  const DFromV<decltype(a)> d;
  const Twice<decltype(d)> d2;
  const auto ba = Combine(d2, b, a);
#if HWY_TARGET == HWY_SSE2
  Vec64<uint32_t> ba_shuffled{
      _mm_shuffle_epi32(ba.raw, _MM_SHUFFLE(3, 0, 3, 0))};
  return Vec64<T>{
      _mm_shufflelo_epi16(ba_shuffled.raw, _MM_SHUFFLE(2, 3, 0, 1))};
#else
  alignas(16) const T kShuffle[8] = {0x0302, 0x0100, 0x0f0e, 0x0d0c};
  return Vec64<T>{TableLookupBytes(ba, Load(d2, kShuffle)).raw};
#endif
}
template <typename T, HWY_IF_T_SIZE(T, 4)>
HWY_API Vec128<T> ShuffleTwo2301(const Vec128<T> a, const Vec128<T> b) {
  const DFromV<decltype(a)> d;
  const RebindToFloat<decltype(d)> df;
  constexpr int m = _MM_SHUFFLE(2, 3, 0, 1);
  return BitCast(d, Vec128<float>{_mm_shuffle_ps(BitCast(df, a).raw,
                                                 BitCast(df, b).raw, m)});
}

template <typename T, HWY_IF_T_SIZE(T, 1)>
HWY_API Vec32<T> ShuffleTwo1230(const Vec32<T> a, const Vec32<T> b) {
  const DFromV<decltype(a)> d;
#if HWY_TARGET == HWY_SSE2
  const auto zero = Zero(d);
  const Rebind<int16_t, decltype(d)> di16;
  const Vec32<int16_t> a_shuffled{_mm_shufflelo_epi16(
      _mm_unpacklo_epi8(a.raw, zero.raw), _MM_SHUFFLE(3, 0, 3, 0))};
  const Vec32<int16_t> b_shuffled{_mm_shufflelo_epi16(
      _mm_unpacklo_epi8(b.raw, zero.raw), _MM_SHUFFLE(1, 2, 1, 2))};
  const auto ba_shuffled = Combine(di16, b_shuffled, a_shuffled);
  return Vec32<T>{_mm_packus_epi16(ba_shuffled.raw, ba_shuffled.raw)};
#else
  const Twice<decltype(d)> d2;
  const auto ba = Combine(d2, b, a);
  alignas(16) const T kShuffle[8] = {0, 3, 6, 5};
  return Vec32<T>{TableLookupBytes(ba, Load(d2, kShuffle)).raw};
#endif
}
template <typename T, HWY_IF_T_SIZE(T, 2)>
HWY_API Vec64<T> ShuffleTwo1230(const Vec64<T> a, const Vec64<T> b) {
  const DFromV<decltype(a)> d;
#if HWY_TARGET == HWY_SSE2
  const Vec32<T> a_shuffled{
      _mm_shufflelo_epi16(a.raw, _MM_SHUFFLE(3, 0, 3, 0))};
  const Vec32<T> b_shuffled{
      _mm_shufflelo_epi16(b.raw, _MM_SHUFFLE(1, 2, 1, 2))};
  return Combine(d, b_shuffled, a_shuffled);
#else
  const Twice<decltype(d)> d2;
  const auto ba = Combine(d2, b, a);
  alignas(16) const T kShuffle[8] = {0x0100, 0x0706, 0x0d0c, 0x0b0a};
  return Vec64<T>{TableLookupBytes(ba, Load(d2, kShuffle)).raw};
#endif
}
template <typename T, HWY_IF_T_SIZE(T, 4)>
HWY_API Vec128<T> ShuffleTwo1230(const Vec128<T> a, const Vec128<T> b) {
  const DFromV<decltype(a)> d;
  const RebindToFloat<decltype(d)> df;
  constexpr int m = _MM_SHUFFLE(1, 2, 3, 0);
  return BitCast(d, Vec128<float>{_mm_shuffle_ps(BitCast(df, a).raw,
                                                 BitCast(df, b).raw, m)});
}

template <typename T, HWY_IF_T_SIZE(T, 1)>
HWY_API Vec32<T> ShuffleTwo3012(const Vec32<T> a, const Vec32<T> b) {
  const DFromV<decltype(a)> d;
#if HWY_TARGET == HWY_SSE2
  const auto zero = Zero(d);
  const Rebind<int16_t, decltype(d)> di16;
  const Vec32<int16_t> a_shuffled{_mm_shufflelo_epi16(
      _mm_unpacklo_epi8(a.raw, zero.raw), _MM_SHUFFLE(1, 2, 1, 2))};
  const Vec32<int16_t> b_shuffled{_mm_shufflelo_epi16(
      _mm_unpacklo_epi8(b.raw, zero.raw), _MM_SHUFFLE(3, 0, 3, 0))};
  const auto ba_shuffled = Combine(di16, b_shuffled, a_shuffled);
  return Vec32<T>{_mm_packus_epi16(ba_shuffled.raw, ba_shuffled.raw)};
#else
  const Twice<decltype(d)> d2;
  const auto ba = Combine(d2, b, a);
  alignas(16) const T kShuffle[8] = {2, 1, 4, 7};
  return Vec32<T>{TableLookupBytes(ba, Load(d2, kShuffle)).raw};
#endif
}
template <typename T, HWY_IF_T_SIZE(T, 2)>
HWY_API Vec64<T> ShuffleTwo3012(const Vec64<T> a, const Vec64<T> b) {
  const DFromV<decltype(a)> d;
#if HWY_TARGET == HWY_SSE2
  const Vec32<T> a_shuffled{
      _mm_shufflelo_epi16(a.raw, _MM_SHUFFLE(1, 2, 1, 2))};
  const Vec32<T> b_shuffled{
      _mm_shufflelo_epi16(b.raw, _MM_SHUFFLE(3, 0, 3, 0))};
  return Combine(d, b_shuffled, a_shuffled);
#else
  const Twice<decltype(d)> d2;
  const auto ba = Combine(d2, b, a);
  alignas(16) const T kShuffle[8] = {0x0504, 0x0302, 0x0908, 0x0f0e};
  return Vec64<T>{TableLookupBytes(ba, Load(d2, kShuffle)).raw};
#endif
}
template <typename T, HWY_IF_T_SIZE(T, 4)>
HWY_API Vec128<T> ShuffleTwo3012(const Vec128<T> a, const Vec128<T> b) {
  const DFromV<decltype(a)> d;
  const RebindToFloat<decltype(d)> df;
  constexpr int m = _MM_SHUFFLE(3, 0, 1, 2);
  return BitCast(d, Vec128<float>{_mm_shuffle_ps(BitCast(df, a).raw,
                                                 BitCast(df, b).raw, m)});
}

}  // namespace detail

// Swap 64-bit halves
HWY_API Vec128<uint32_t> Shuffle1032(const Vec128<uint32_t> v) {
  return Vec128<uint32_t>{_mm_shuffle_epi32(v.raw, 0x4E)};
}
HWY_API Vec128<int32_t> Shuffle1032(const Vec128<int32_t> v) {
  return Vec128<int32_t>{_mm_shuffle_epi32(v.raw, 0x4E)};
}
HWY_API Vec128<float> Shuffle1032(const Vec128<float> v) {
  return Vec128<float>{_mm_shuffle_ps(v.raw, v.raw, 0x4E)};
}
HWY_API Vec128<uint64_t> Shuffle01(const Vec128<uint64_t> v) {
  return Vec128<uint64_t>{_mm_shuffle_epi32(v.raw, 0x4E)};
}
HWY_API Vec128<int64_t> Shuffle01(const Vec128<int64_t> v) {
  return Vec128<int64_t>{_mm_shuffle_epi32(v.raw, 0x4E)};
}
HWY_API Vec128<double> Shuffle01(const Vec128<double> v) {
  return Vec128<double>{_mm_shuffle_pd(v.raw, v.raw, 1)};
}

// Rotate right 32 bits
HWY_API Vec128<uint32_t> Shuffle0321(const Vec128<uint32_t> v) {
  return Vec128<uint32_t>{_mm_shuffle_epi32(v.raw, 0x39)};
}
HWY_API Vec128<int32_t> Shuffle0321(const Vec128<int32_t> v) {
  return Vec128<int32_t>{_mm_shuffle_epi32(v.raw, 0x39)};
}
HWY_API Vec128<float> Shuffle0321(const Vec128<float> v) {
  return Vec128<float>{_mm_shuffle_ps(v.raw, v.raw, 0x39)};
}
// Rotate left 32 bits
HWY_API Vec128<uint32_t> Shuffle2103(const Vec128<uint32_t> v) {
  return Vec128<uint32_t>{_mm_shuffle_epi32(v.raw, 0x93)};
}
HWY_API Vec128<int32_t> Shuffle2103(const Vec128<int32_t> v) {
  return Vec128<int32_t>{_mm_shuffle_epi32(v.raw, 0x93)};
}
HWY_API Vec128<float> Shuffle2103(const Vec128<float> v) {
  return Vec128<float>{_mm_shuffle_ps(v.raw, v.raw, 0x93)};
}

// Reverse
HWY_API Vec128<uint32_t> Shuffle0123(const Vec128<uint32_t> v) {
  return Vec128<uint32_t>{_mm_shuffle_epi32(v.raw, 0x1B)};
}
HWY_API Vec128<int32_t> Shuffle0123(const Vec128<int32_t> v) {
  return Vec128<int32_t>{_mm_shuffle_epi32(v.raw, 0x1B)};
}
HWY_API Vec128<float> Shuffle0123(const Vec128<float> v) {
  return Vec128<float>{_mm_shuffle_ps(v.raw, v.raw, 0x1B)};
}

// ================================================== COMPARE

#if HWY_TARGET <= HWY_AVX3

// Comparisons set a mask bit to 1 if the condition is true, else 0.

// ------------------------------ MaskFromVec

namespace detail {

template <typename T, size_t N>
HWY_INLINE Mask128<T, N> MaskFromVec(hwy::SizeTag<1> /*tag*/,
                                     const Vec128<T, N> v) {
  return Mask128<T, N>{_mm_movepi8_mask(v.raw)};
}
template <typename T, size_t N>
HWY_INLINE Mask128<T, N> MaskFromVec(hwy::SizeTag<2> /*tag*/,
                                     const Vec128<T, N> v) {
  return Mask128<T, N>{_mm_movepi16_mask(v.raw)};
}
template <typename T, size_t N>
HWY_INLINE Mask128<T, N> MaskFromVec(hwy::SizeTag<4> /*tag*/,
                                     const Vec128<T, N> v) {
  return Mask128<T, N>{_mm_movepi32_mask(v.raw)};
}
template <typename T, size_t N>
HWY_INLINE Mask128<T, N> MaskFromVec(hwy::SizeTag<8> /*tag*/,
                                     const Vec128<T, N> v) {
  return Mask128<T, N>{_mm_movepi64_mask(v.raw)};
}

}  // namespace detail

template <typename T, size_t N>
HWY_API Mask128<T, N> MaskFromVec(const Vec128<T, N> v) {
  return detail::MaskFromVec(hwy::SizeTag<sizeof(T)>(), v);
}
// There do not seem to be native floating-point versions of these instructions.
template <size_t N>
HWY_API Mask128<float, N> MaskFromVec(const Vec128<float, N> v) {
  const RebindToSigned<DFromV<decltype(v)>> di;
  return Mask128<float, N>{MaskFromVec(BitCast(di, v)).raw};
}
template <size_t N>
HWY_API Mask128<double, N> MaskFromVec(const Vec128<double, N> v) {
  const RebindToSigned<DFromV<decltype(v)>> di;
  return Mask128<double, N>{MaskFromVec(BitCast(di, v)).raw};
}

template <class D>
using MFromD = decltype(MaskFromVec(VFromD<D>()));

// ------------------------------ VecFromMask

template <typename T, size_t N, HWY_IF_T_SIZE(T, 1)>
HWY_API Vec128<T, N> VecFromMask(const Mask128<T, N> v) {
  return Vec128<T, N>{_mm_movm_epi8(v.raw)};
}

template <typename T, size_t N, HWY_IF_T_SIZE(T, 2)>
HWY_API Vec128<T, N> VecFromMask(const Mask128<T, N> v) {
  return Vec128<T, N>{_mm_movm_epi16(v.raw)};
}

template <typename T, size_t N, HWY_IF_T_SIZE(T, 4)>
HWY_API Vec128<T, N> VecFromMask(const Mask128<T, N> v) {
  return Vec128<T, N>{_mm_movm_epi32(v.raw)};
}

template <typename T, size_t N, HWY_IF_T_SIZE(T, 8)>
HWY_API Vec128<T, N> VecFromMask(const Mask128<T, N> v) {
  return Vec128<T, N>{_mm_movm_epi64(v.raw)};
}

template <size_t N>
HWY_API Vec128<float, N> VecFromMask(const Mask128<float, N> v) {
  return Vec128<float, N>{_mm_castsi128_ps(_mm_movm_epi32(v.raw))};
}

template <size_t N>
HWY_API Vec128<double, N> VecFromMask(const Mask128<double, N> v) {
  return Vec128<double, N>{_mm_castsi128_pd(_mm_movm_epi64(v.raw))};
}

template <class D>
HWY_API VFromD<D> VecFromMask(D /* tag */, MFromD<D> v) {
  return VecFromMask(v);
}

// ------------------------------ RebindMask (MaskFromVec)

template <typename TFrom, size_t NFrom, class DTo>
HWY_API MFromD<DTo> RebindMask(DTo /* tag */, Mask128<TFrom, NFrom> m) {
  static_assert(sizeof(TFrom) == sizeof(TFromD<DTo>), "Must have same size");
  return MFromD<DTo>{m.raw};
}

// ------------------------------ TestBit

namespace detail {

template <typename T, size_t N>
HWY_INLINE Mask128<T, N> TestBit(hwy::SizeTag<1> /*tag*/, const Vec128<T, N> v,
                                 const Vec128<T, N> bit) {
  return Mask128<T, N>{_mm_test_epi8_mask(v.raw, bit.raw)};
}
template <typename T, size_t N>
HWY_INLINE Mask128<T, N> TestBit(hwy::SizeTag<2> /*tag*/, const Vec128<T, N> v,
                                 const Vec128<T, N> bit) {
  return Mask128<T, N>{_mm_test_epi16_mask(v.raw, bit.raw)};
}
template <typename T, size_t N>
HWY_INLINE Mask128<T, N> TestBit(hwy::SizeTag<4> /*tag*/, const Vec128<T, N> v,
                                 const Vec128<T, N> bit) {
  return Mask128<T, N>{_mm_test_epi32_mask(v.raw, bit.raw)};
}
template <typename T, size_t N>
HWY_INLINE Mask128<T, N> TestBit(hwy::SizeTag<8> /*tag*/, const Vec128<T, N> v,
                                 const Vec128<T, N> bit) {
  return Mask128<T, N>{_mm_test_epi64_mask(v.raw, bit.raw)};
}

}  // namespace detail

template <typename T, size_t N>
HWY_API Mask128<T, N> TestBit(const Vec128<T, N> v, const Vec128<T, N> bit) {
  static_assert(!hwy::IsFloat<T>(), "Only integer vectors supported");
  return detail::TestBit(hwy::SizeTag<sizeof(T)>(), v, bit);
}

// ------------------------------ Equality

template <typename T, size_t N, HWY_IF_T_SIZE(T, 1)>
HWY_API Mask128<T, N> operator==(const Vec128<T, N> a, const Vec128<T, N> b) {
  return Mask128<T, N>{_mm_cmpeq_epi8_mask(a.raw, b.raw)};
}

template <typename T, size_t N, HWY_IF_T_SIZE(T, 2)>
HWY_API Mask128<T, N> operator==(const Vec128<T, N> a, const Vec128<T, N> b) {
  return Mask128<T, N>{_mm_cmpeq_epi16_mask(a.raw, b.raw)};
}

template <typename T, size_t N, HWY_IF_T_SIZE(T, 4)>
HWY_API Mask128<T, N> operator==(const Vec128<T, N> a, const Vec128<T, N> b) {
  return Mask128<T, N>{_mm_cmpeq_epi32_mask(a.raw, b.raw)};
}

template <typename T, size_t N, HWY_IF_T_SIZE(T, 8)>
HWY_API Mask128<T, N> operator==(const Vec128<T, N> a, const Vec128<T, N> b) {
  return Mask128<T, N>{_mm_cmpeq_epi64_mask(a.raw, b.raw)};
}

template <size_t N>
HWY_API Mask128<float, N> operator==(Vec128<float, N> a, Vec128<float, N> b) {
  return Mask128<float, N>{_mm_cmp_ps_mask(a.raw, b.raw, _CMP_EQ_OQ)};
}

template <size_t N>
HWY_API Mask128<double, N> operator==(Vec128<double, N> a,
                                      Vec128<double, N> b) {
  return Mask128<double, N>{_mm_cmp_pd_mask(a.raw, b.raw, _CMP_EQ_OQ)};
}

// ------------------------------ Inequality

template <typename T, size_t N, HWY_IF_T_SIZE(T, 1)>
HWY_API Mask128<T, N> operator!=(const Vec128<T, N> a, const Vec128<T, N> b) {
  return Mask128<T, N>{_mm_cmpneq_epi8_mask(a.raw, b.raw)};
}

template <typename T, size_t N, HWY_IF_T_SIZE(T, 2)>
HWY_API Mask128<T, N> operator!=(const Vec128<T, N> a, const Vec128<T, N> b) {
  return Mask128<T, N>{_mm_cmpneq_epi16_mask(a.raw, b.raw)};
}

template <typename T, size_t N, HWY_IF_T_SIZE(T, 4)>
HWY_API Mask128<T, N> operator!=(const Vec128<T, N> a, const Vec128<T, N> b) {
  return Mask128<T, N>{_mm_cmpneq_epi32_mask(a.raw, b.raw)};
}

template <typename T, size_t N, HWY_IF_T_SIZE(T, 8)>
HWY_API Mask128<T, N> operator!=(const Vec128<T, N> a, const Vec128<T, N> b) {
  return Mask128<T, N>{_mm_cmpneq_epi64_mask(a.raw, b.raw)};
}

template <size_t N>
HWY_API Mask128<float, N> operator!=(Vec128<float, N> a, Vec128<float, N> b) {
  return Mask128<float, N>{_mm_cmp_ps_mask(a.raw, b.raw, _CMP_NEQ_OQ)};
}

template <size_t N>
HWY_API Mask128<double, N> operator!=(Vec128<double, N> a,
                                      Vec128<double, N> b) {
  return Mask128<double, N>{_mm_cmp_pd_mask(a.raw, b.raw, _CMP_NEQ_OQ)};
}

// ------------------------------ Strict inequality

// Signed/float <
template <size_t N>
HWY_API Mask128<int8_t, N> operator>(Vec128<int8_t, N> a, Vec128<int8_t, N> b) {
  return Mask128<int8_t, N>{_mm_cmpgt_epi8_mask(a.raw, b.raw)};
}
template <size_t N>
HWY_API Mask128<int16_t, N> operator>(Vec128<int16_t, N> a,
                                      Vec128<int16_t, N> b) {
  return Mask128<int16_t, N>{_mm_cmpgt_epi16_mask(a.raw, b.raw)};
}
template <size_t N>
HWY_API Mask128<int32_t, N> operator>(Vec128<int32_t, N> a,
                                      Vec128<int32_t, N> b) {
  return Mask128<int32_t, N>{_mm_cmpgt_epi32_mask(a.raw, b.raw)};
}
template <size_t N>
HWY_API Mask128<int64_t, N> operator>(Vec128<int64_t, N> a,
                                      Vec128<int64_t, N> b) {
  return Mask128<int64_t, N>{_mm_cmpgt_epi64_mask(a.raw, b.raw)};
}

template <size_t N>
HWY_API Mask128<uint8_t, N> operator>(Vec128<uint8_t, N> a,
                                      Vec128<uint8_t, N> b) {
  return Mask128<uint8_t, N>{_mm_cmpgt_epu8_mask(a.raw, b.raw)};
}
template <size_t N>
HWY_API Mask128<uint16_t, N> operator>(Vec128<uint16_t, N> a,
                                       Vec128<uint16_t, N> b) {
  return Mask128<uint16_t, N>{_mm_cmpgt_epu16_mask(a.raw, b.raw)};
}
template <size_t N>
HWY_API Mask128<uint32_t, N> operator>(Vec128<uint32_t, N> a,
                                       Vec128<uint32_t, N> b) {
  return Mask128<uint32_t, N>{_mm_cmpgt_epu32_mask(a.raw, b.raw)};
}
template <size_t N>
HWY_API Mask128<uint64_t, N> operator>(Vec128<uint64_t, N> a,
                                       Vec128<uint64_t, N> b) {
  return Mask128<uint64_t, N>{_mm_cmpgt_epu64_mask(a.raw, b.raw)};
}

template <size_t N>
HWY_API Mask128<float, N> operator>(Vec128<float, N> a, Vec128<float, N> b) {
  return Mask128<float, N>{_mm_cmp_ps_mask(a.raw, b.raw, _CMP_GT_OQ)};
}
template <size_t N>
HWY_API Mask128<double, N> operator>(Vec128<double, N> a, Vec128<double, N> b) {
  return Mask128<double, N>{_mm_cmp_pd_mask(a.raw, b.raw, _CMP_GT_OQ)};
}

// ------------------------------ Weak inequality

template <size_t N>
HWY_API Mask128<float, N> operator>=(Vec128<float, N> a, Vec128<float, N> b) {
  return Mask128<float, N>{_mm_cmp_ps_mask(a.raw, b.raw, _CMP_GE_OQ)};
}
template <size_t N>
HWY_API Mask128<double, N> operator>=(Vec128<double, N> a,
                                      Vec128<double, N> b) {
  return Mask128<double, N>{_mm_cmp_pd_mask(a.raw, b.raw, _CMP_GE_OQ)};
}

template <size_t N>
HWY_API Mask128<int8_t, N> operator>=(Vec128<int8_t, N> a,
                                      Vec128<int8_t, N> b) {
  return Mask128<int8_t, N>{_mm_cmpge_epi8_mask(a.raw, b.raw)};
}
template <size_t N>
HWY_API Mask128<int16_t, N> operator>=(Vec128<int16_t, N> a,
                                       Vec128<int16_t, N> b) {
  return Mask128<int16_t, N>{_mm_cmpge_epi16_mask(a.raw, b.raw)};
}
template <size_t N>
HWY_API Mask128<int32_t, N> operator>=(Vec128<int32_t, N> a,
                                       Vec128<int32_t, N> b) {
  return Mask128<int32_t, N>{_mm_cmpge_epi32_mask(a.raw, b.raw)};
}
template <size_t N>
HWY_API Mask128<int64_t, N> operator>=(Vec128<int64_t, N> a,
                                       Vec128<int64_t, N> b) {
  return Mask128<int64_t, N>{_mm_cmpge_epi64_mask(a.raw, b.raw)};
}

template <size_t N>
HWY_API Mask128<uint8_t, N> operator>=(Vec128<uint8_t, N> a,
                                       Vec128<uint8_t, N> b) {
  return Mask128<uint8_t, N>{_mm_cmpge_epu8_mask(a.raw, b.raw)};
}
template <size_t N>
HWY_API Mask128<uint16_t, N> operator>=(Vec128<uint16_t, N> a,
                                        Vec128<uint16_t, N> b) {
  return Mask128<uint16_t, N>{_mm_cmpge_epu16_mask(a.raw, b.raw)};
}
template <size_t N>
HWY_API Mask128<uint32_t, N> operator>=(Vec128<uint32_t, N> a,
                                        Vec128<uint32_t, N> b) {
  return Mask128<uint32_t, N>{_mm_cmpge_epu32_mask(a.raw, b.raw)};
}
template <size_t N>
HWY_API Mask128<uint64_t, N> operator>=(Vec128<uint64_t, N> a,
                                        Vec128<uint64_t, N> b) {
  return Mask128<uint64_t, N>{_mm_cmpge_epu64_mask(a.raw, b.raw)};
}

#else  // AVX2 or below

// Comparisons fill a lane with 1-bits if the condition is true, else 0.

template <class DTo, typename TFrom, size_t NFrom>
HWY_API MFromD<DTo> RebindMask(DTo dto, Mask128<TFrom, NFrom> m) {
  static_assert(sizeof(TFrom) == sizeof(TFromD<DTo>), "Must have same size");
  const Simd<TFrom, NFrom, 0> d;
  return MaskFromVec(BitCast(dto, VecFromMask(d, m)));
}

template <typename T, size_t N>
HWY_API Mask128<T, N> TestBit(Vec128<T, N> v, Vec128<T, N> bit) {
  static_assert(!hwy::IsFloat<T>(), "Only integer vectors supported");
  return (v & bit) == bit;
}

// ------------------------------ Equality

// Unsigned
template <size_t N>
HWY_API Mask128<uint8_t, N> operator==(const Vec128<uint8_t, N> a,
                                       const Vec128<uint8_t, N> b) {
  return Mask128<uint8_t, N>{_mm_cmpeq_epi8(a.raw, b.raw)};
}
template <size_t N>
HWY_API Mask128<uint16_t, N> operator==(const Vec128<uint16_t, N> a,
                                        const Vec128<uint16_t, N> b) {
  return Mask128<uint16_t, N>{_mm_cmpeq_epi16(a.raw, b.raw)};
}
template <size_t N>
HWY_API Mask128<uint32_t, N> operator==(const Vec128<uint32_t, N> a,
                                        const Vec128<uint32_t, N> b) {
  return Mask128<uint32_t, N>{_mm_cmpeq_epi32(a.raw, b.raw)};
}
template <size_t N>
HWY_API Mask128<uint64_t, N> operator==(const Vec128<uint64_t, N> a,
                                        const Vec128<uint64_t, N> b) {
#if HWY_TARGET >= HWY_SSSE3
  const DFromV<decltype(a)> d64;
  const RepartitionToNarrow<decltype(d64)> d32;
  const auto cmp32 = VecFromMask(d32, Eq(BitCast(d32, a), BitCast(d32, b)));
  const auto cmp64 = cmp32 & Shuffle2301(cmp32);
  return MaskFromVec(BitCast(d64, cmp64));
#else
  return Mask128<uint64_t, N>{_mm_cmpeq_epi64(a.raw, b.raw)};
#endif
}

// Signed
template <size_t N>
HWY_API Mask128<int8_t, N> operator==(const Vec128<int8_t, N> a,
                                      const Vec128<int8_t, N> b) {
  return Mask128<int8_t, N>{_mm_cmpeq_epi8(a.raw, b.raw)};
}
template <size_t N>
HWY_API Mask128<int16_t, N> operator==(Vec128<int16_t, N> a,
                                       Vec128<int16_t, N> b) {
  return Mask128<int16_t, N>{_mm_cmpeq_epi16(a.raw, b.raw)};
}
template <size_t N>
HWY_API Mask128<int32_t, N> operator==(const Vec128<int32_t, N> a,
                                       const Vec128<int32_t, N> b) {
  return Mask128<int32_t, N>{_mm_cmpeq_epi32(a.raw, b.raw)};
}
template <size_t N>
HWY_API Mask128<int64_t, N> operator==(const Vec128<int64_t, N> a,
                                       const Vec128<int64_t, N> b) {
  // Same as signed ==; avoid duplicating the SSSE3 version.
  const DFromV<decltype(a)> d;
  RebindToUnsigned<decltype(d)> du;
  return RebindMask(d, BitCast(du, a) == BitCast(du, b));
}

// Float
template <size_t N>
HWY_API Mask128<float, N> operator==(const Vec128<float, N> a,
                                     const Vec128<float, N> b) {
  return Mask128<float, N>{_mm_cmpeq_ps(a.raw, b.raw)};
}
template <size_t N>
HWY_API Mask128<double, N> operator==(const Vec128<double, N> a,
                                      const Vec128<double, N> b) {
  return Mask128<double, N>{_mm_cmpeq_pd(a.raw, b.raw)};
}

// ------------------------------ Inequality

// This cannot have T as a template argument, otherwise it is not more
// specialized than rewritten operator== in C++20, leading to compile
// errors: https://gcc.godbolt.org/z/xsrPhPvPT.
template <size_t N>
HWY_API Mask128<uint8_t, N> operator!=(Vec128<uint8_t, N> a,
                                       Vec128<uint8_t, N> b) {
  return Not(a == b);
}
template <size_t N>
HWY_API Mask128<uint16_t, N> operator!=(Vec128<uint16_t, N> a,
                                        Vec128<uint16_t, N> b) {
  return Not(a == b);
}
template <size_t N>
HWY_API Mask128<uint32_t, N> operator!=(Vec128<uint32_t, N> a,
                                        Vec128<uint32_t, N> b) {
  return Not(a == b);
}
template <size_t N>
HWY_API Mask128<uint64_t, N> operator!=(Vec128<uint64_t, N> a,
                                        Vec128<uint64_t, N> b) {
  return Not(a == b);
}
template <size_t N>
HWY_API Mask128<int8_t, N> operator!=(Vec128<int8_t, N> a,
                                      Vec128<int8_t, N> b) {
  return Not(a == b);
}
template <size_t N>
HWY_API Mask128<int16_t, N> operator!=(Vec128<int16_t, N> a,
                                       Vec128<int16_t, N> b) {
  return Not(a == b);
}
template <size_t N>
HWY_API Mask128<int32_t, N> operator!=(Vec128<int32_t, N> a,
                                       Vec128<int32_t, N> b) {
  return Not(a == b);
}
template <size_t N>
HWY_API Mask128<int64_t, N> operator!=(Vec128<int64_t, N> a,
                                       Vec128<int64_t, N> b) {
  return Not(a == b);
}

template <size_t N>
HWY_API Mask128<float, N> operator!=(const Vec128<float, N> a,
                                     const Vec128<float, N> b) {
  return Mask128<float, N>{_mm_cmpneq_ps(a.raw, b.raw)};
}
template <size_t N>
HWY_API Mask128<double, N> operator!=(const Vec128<double, N> a,
                                      const Vec128<double, N> b) {
  return Mask128<double, N>{_mm_cmpneq_pd(a.raw, b.raw)};
}

// ------------------------------ Strict inequality

namespace detail {

template <size_t N>
HWY_INLINE Mask128<int8_t, N> Gt(hwy::SignedTag /*tag*/, Vec128<int8_t, N> a,
                                 Vec128<int8_t, N> b) {
  return Mask128<int8_t, N>{_mm_cmpgt_epi8(a.raw, b.raw)};
}
template <size_t N>
HWY_INLINE Mask128<int16_t, N> Gt(hwy::SignedTag /*tag*/, Vec128<int16_t, N> a,
                                  Vec128<int16_t, N> b) {
  return Mask128<int16_t, N>{_mm_cmpgt_epi16(a.raw, b.raw)};
}
template <size_t N>
HWY_INLINE Mask128<int32_t, N> Gt(hwy::SignedTag /*tag*/, Vec128<int32_t, N> a,
                                  Vec128<int32_t, N> b) {
  return Mask128<int32_t, N>{_mm_cmpgt_epi32(a.raw, b.raw)};
}

template <size_t N>
HWY_INLINE Mask128<int64_t, N> Gt(hwy::SignedTag /*tag*/,
                                  const Vec128<int64_t, N> a,
                                  const Vec128<int64_t, N> b) {
#if HWY_TARGET >= HWY_SSSE3
  // See https://stackoverflow.com/questions/65166174/:
  const DFromV<decltype(a)> d;
  const RepartitionToNarrow<decltype(d)> d32;
  const Vec128<int64_t, N> m_eq32{Eq(BitCast(d32, a), BitCast(d32, b)).raw};
  const Vec128<int64_t, N> m_gt32{Gt(BitCast(d32, a), BitCast(d32, b)).raw};
  // If a.upper is greater, upper := true. Otherwise, if a.upper == b.upper:
  // upper := b-a (unsigned comparison result of lower). Otherwise: upper := 0.
  const __m128i upper = OrAnd(m_gt32, m_eq32, Sub(b, a)).raw;
  // Duplicate upper to lower half.
  return Mask128<int64_t, N>{_mm_shuffle_epi32(upper, _MM_SHUFFLE(3, 3, 1, 1))};
#else
  return Mask128<int64_t, N>{_mm_cmpgt_epi64(a.raw, b.raw)};  // SSE4.2
#endif
}

template <typename T, size_t N>
HWY_INLINE Mask128<T, N> Gt(hwy::UnsignedTag /*tag*/, Vec128<T, N> a,
                            Vec128<T, N> b) {
  const DFromV<decltype(a)> du;
  const RebindToSigned<decltype(du)> di;
  const Vec128<T, N> msb = Set(du, (LimitsMax<T>() >> 1) + 1);
  const auto sa = BitCast(di, Xor(a, msb));
  const auto sb = BitCast(di, Xor(b, msb));
  return RebindMask(du, Gt(hwy::SignedTag(), sa, sb));
}

template <size_t N>
HWY_INLINE Mask128<float, N> Gt(hwy::FloatTag /*tag*/, Vec128<float, N> a,
                                Vec128<float, N> b) {
  return Mask128<float, N>{_mm_cmpgt_ps(a.raw, b.raw)};
}
template <size_t N>
HWY_INLINE Mask128<double, N> Gt(hwy::FloatTag /*tag*/, Vec128<double, N> a,
                                 Vec128<double, N> b) {
  return Mask128<double, N>{_mm_cmpgt_pd(a.raw, b.raw)};
}

}  // namespace detail

template <typename T, size_t N>
HWY_INLINE Mask128<T, N> operator>(Vec128<T, N> a, Vec128<T, N> b) {
  return detail::Gt(hwy::TypeTag<T>(), a, b);
}

// ------------------------------ Weak inequality

namespace detail {
template <typename T, size_t N>
HWY_INLINE Mask128<T, N> Ge(hwy::SignedTag tag, Vec128<T, N> a,
                            Vec128<T, N> b) {
  return Not(Gt(tag, b, a));
}

template <typename T, size_t N>
HWY_INLINE Mask128<T, N> Ge(hwy::UnsignedTag tag, Vec128<T, N> a,
                            Vec128<T, N> b) {
  return Not(Gt(tag, b, a));
}

template <size_t N>
HWY_INLINE Mask128<float, N> Ge(hwy::FloatTag /*tag*/, Vec128<float, N> a,
                                Vec128<float, N> b) {
  return Mask128<float, N>{_mm_cmpge_ps(a.raw, b.raw)};
}
template <size_t N>
HWY_INLINE Mask128<double, N> Ge(hwy::FloatTag /*tag*/, Vec128<double, N> a,
                                 Vec128<double, N> b) {
  return Mask128<double, N>{_mm_cmpge_pd(a.raw, b.raw)};
}

}  // namespace detail

template <typename T, size_t N>
HWY_API Mask128<T, N> operator>=(Vec128<T, N> a, Vec128<T, N> b) {
  return detail::Ge(hwy::TypeTag<T>(), a, b);
}

#endif  // HWY_TARGET <= HWY_AVX3

// ------------------------------ Reversed comparisons

template <typename T, size_t N>
HWY_API Mask128<T, N> operator<(Vec128<T, N> a, Vec128<T, N> b) {
  return b > a;
}

template <typename T, size_t N>
HWY_API Mask128<T, N> operator<=(Vec128<T, N> a, Vec128<T, N> b) {
  return b >= a;
}

// ------------------------------ Iota (Load)

namespace detail {

template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_T_SIZE_D(D, 1)>
HWY_INLINE VFromD<D> Iota0(D /*d*/) {
  return VFromD<D>{_mm_set_epi8(
      static_cast<char>(15), static_cast<char>(14), static_cast<char>(13),
      static_cast<char>(12), static_cast<char>(11), static_cast<char>(10),
      static_cast<char>(9), static_cast<char>(8), static_cast<char>(7),
      static_cast<char>(6), static_cast<char>(5), static_cast<char>(4),
      static_cast<char>(3), static_cast<char>(2), static_cast<char>(1),
      static_cast<char>(0))};
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_T_SIZE_D(D, 2),
          HWY_IF_NOT_SPECIAL_FLOAT_D(D)>
HWY_INLINE VFromD<D> Iota0(D /*d*/) {
  return VFromD<D>{_mm_set_epi16(int16_t{7}, int16_t{6}, int16_t{5}, int16_t{4},
                                 int16_t{3}, int16_t{2}, int16_t{1},
                                 int16_t{0})};
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_UI32_D(D)>
HWY_INLINE VFromD<D> Iota0(D /*d*/) {
  return VFromD<D>{
      _mm_set_epi32(int32_t{3}, int32_t{2}, int32_t{1}, int32_t{0})};
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_UI64_D(D)>
HWY_INLINE VFromD<D> Iota0(D /*d*/) {
  return VFromD<D>{_mm_set_epi64x(int64_t{1}, int64_t{0})};
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_F32_D(D)>
HWY_INLINE VFromD<D> Iota0(D /*d*/) {
  return VFromD<D>{_mm_set_ps(3.0f, 2.0f, 1.0f, 0.0f)};
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_F64_D(D)>
HWY_INLINE VFromD<D> Iota0(D /*d*/) {
  return VFromD<D>{_mm_set_pd(1.0, 0.0)};
}

#if HWY_COMPILER_MSVC
template <class V, HWY_IF_V_SIZE_V(V, 1)>
static HWY_INLINE V MaskOutVec128Iota(V v) {
  const V mask_out_mask{_mm_set_epi32(0, 0, 0, 0xFF)};
  return v & mask_out_mask;
}
template <class V, HWY_IF_V_SIZE_V(V, 2)>
static HWY_INLINE V MaskOutVec128Iota(V v) {
#if HWY_TARGET <= HWY_SSE4
  return V{_mm_blend_epi16(v.raw, _mm_setzero_si128(), 0xFE)};
#else
  const V mask_out_mask{_mm_set_epi32(0, 0, 0, 0xFFFF)};
  return v & mask_out_mask;
#endif
}
template <class V, HWY_IF_V_SIZE_V(V, 4)>
static HWY_INLINE V MaskOutVec128Iota(V v) {
  const DFromV<decltype(v)> d;
  const Repartition<float, decltype(d)> df;
  using VF = VFromD<decltype(df)>;
  return BitCast(d, VF{_mm_move_ss(_mm_setzero_ps(), BitCast(df, v).raw)});
}
template <class V, HWY_IF_V_SIZE_V(V, 8)>
static HWY_INLINE V MaskOutVec128Iota(V v) {
  const DFromV<decltype(v)> d;
  const RebindToUnsigned<decltype(d)> du;
  using VU = VFromD<decltype(du)>;
  return BitCast(d, VU{_mm_move_epi64(BitCast(du, v).raw)});
}
template <class V, HWY_IF_V_SIZE_GT_V(V, 8)>
static HWY_INLINE V MaskOutVec128Iota(V v) {
  return v;
}
#endif

}  // namespace detail

template <class D, typename T2, HWY_IF_V_SIZE_LE_D(D, 16)>
HWY_API VFromD<D> Iota(D d, const T2 first) {
  const auto result_iota = detail::Iota0(d) + Set(d, static_cast<TFromD<D>>(first));
#if HWY_COMPILER_MSVC
  return detail::MaskOutVec128Iota(result_iota);
#else
  return result_iota;
#endif
}

// ------------------------------ FirstN (Iota, Lt)

template <class D, class M = MFromD<D>, HWY_IF_V_SIZE_LE_D(D, 16)>
HWY_API M FirstN(D d, size_t num) {
#if HWY_TARGET <= HWY_AVX3
  constexpr size_t kN = MaxLanes(d);
#if HWY_ARCH_X86_64
  const uint64_t all = (1ull << kN) - 1;
  // BZHI only looks at the lower 8 bits of n!
  return M::FromBits((num > 255) ? all : _bzhi_u64(all, num));
#else
  const uint32_t all = static_cast<uint32_t>((1ull << kN) - 1);
  // BZHI only looks at the lower 8 bits of n!
  return M::FromBits((num > 255) ? all
                                 : _bzhi_u32(all, static_cast<uint32_t>(num)));
#endif  // HWY_ARCH_X86_64
#else   // HWY_TARGET > HWY_AVX3
  const RebindToSigned<decltype(d)> di;  // Signed comparisons are cheaper.
  using TI = TFromD<decltype(di)>;
  return RebindMask(d, detail::Iota0(di) < Set(di, static_cast<TI>(num)));
#endif  // HWY_TARGET <= HWY_AVX3
}

// ================================================== MEMORY (2)

// ------------------------------ MaskedLoad

#if HWY_TARGET <= HWY_AVX3

template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_T_SIZE_D(D, 1)>
HWY_API VFromD<D> MaskedLoad(MFromD<D> m, D /* tag */,
                             const TFromD<D>* HWY_RESTRICT p) {
  return VFromD<D>{_mm_maskz_loadu_epi8(m.raw, p)};
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_T_SIZE_D(D, 2)>
HWY_API VFromD<D> MaskedLoad(MFromD<D> m, D /* tag */,
                             const TFromD<D>* HWY_RESTRICT p) {
  return VFromD<D>{_mm_maskz_loadu_epi16(m.raw, p)};
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_UI32_D(D)>
HWY_API VFromD<D> MaskedLoad(MFromD<D> m, D /* tag */,
                             const TFromD<D>* HWY_RESTRICT p) {
  return VFromD<D>{_mm_maskz_loadu_epi32(m.raw, p)};
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_UI64_D(D)>
HWY_API VFromD<D> MaskedLoad(MFromD<D> m, D /* tag */,
                             const TFromD<D>* HWY_RESTRICT p) {
  return VFromD<D>{_mm_maskz_loadu_epi64(m.raw, p)};
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 16)>
HWY_API VFromD<D> MaskedLoad(MFromD<D> m, D /* tag */,
                             const float* HWY_RESTRICT p) {
  return VFromD<D>{_mm_maskz_loadu_ps(m.raw, p)};
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 16)>
HWY_API VFromD<D> MaskedLoad(MFromD<D> m, D /* tag */,
                             const double* HWY_RESTRICT p) {
  return VFromD<D>{_mm_maskz_loadu_pd(m.raw, p)};
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_T_SIZE_D(D, 1)>
HWY_API VFromD<D> MaskedLoadOr(VFromD<D> v, MFromD<D> m, D /* tag */,
                               const TFromD<D>* HWY_RESTRICT p) {
  return VFromD<D>{_mm_mask_loadu_epi8(v.raw, m.raw, p)};
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_T_SIZE_D(D, 2)>
HWY_API VFromD<D> MaskedLoadOr(VFromD<D> v, MFromD<D> m, D /* tag */,
                               const TFromD<D>* HWY_RESTRICT p) {
  return VFromD<D>{_mm_mask_loadu_epi16(v.raw, m.raw, p)};
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_UI32_D(D)>
HWY_API VFromD<D> MaskedLoadOr(VFromD<D> v, MFromD<D> m, D /* tag */,
                               const TFromD<D>* HWY_RESTRICT p) {
  return VFromD<D>{_mm_mask_loadu_epi32(v.raw, m.raw, p)};
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_UI64_D(D)>
HWY_API VFromD<D> MaskedLoadOr(VFromD<D> v, MFromD<D> m, D /* tag */,
                               const TFromD<D>* HWY_RESTRICT p) {
  return VFromD<D>{_mm_mask_loadu_epi64(v.raw, m.raw, p)};
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 16)>
HWY_API VFromD<D> MaskedLoadOr(VFromD<D> v, MFromD<D> m, D /* tag */,
                               const float* HWY_RESTRICT p) {
  return VFromD<D>{_mm_mask_loadu_ps(v.raw, m.raw, p)};
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 16)>
HWY_API VFromD<D> MaskedLoadOr(VFromD<D> v, MFromD<D> m, D /* tag */,
                               const double* HWY_RESTRICT p) {
  return VFromD<D>{_mm_mask_loadu_pd(v.raw, m.raw, p)};
}

#elif HWY_TARGET == HWY_AVX2

template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_UI32_D(D)>
HWY_API VFromD<D> MaskedLoad(MFromD<D> m, D /* tag */,
                             const TFromD<D>* HWY_RESTRICT p) {
  auto p_p = reinterpret_cast<const int*>(p);  // NOLINT
  return VFromD<D>{_mm_maskload_epi32(p_p, m.raw)};
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_UI64_D(D)>
HWY_API VFromD<D> MaskedLoad(MFromD<D> m, D /* tag */,
                             const TFromD<D>* HWY_RESTRICT p) {
  auto p_p = reinterpret_cast<const long long*>(p);  // NOLINT
  return VFromD<D>{_mm_maskload_epi64(p_p, m.raw)};
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 16)>
HWY_API VFromD<D> MaskedLoad(MFromD<D> m, D d, const float* HWY_RESTRICT p) {
  const RebindToSigned<decltype(d)> di;
  return VFromD<D>{_mm_maskload_ps(p, BitCast(di, VecFromMask(d, m)).raw)};
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 16)>
HWY_API VFromD<D> MaskedLoad(MFromD<D> m, D d, const double* HWY_RESTRICT p) {
  const RebindToSigned<decltype(d)> di;
  return VFromD<D>{_mm_maskload_pd(p, BitCast(di, VecFromMask(d, m)).raw)};
}

// There is no maskload_epi8/16, so blend instead.
template <class D, HWY_IF_V_SIZE_LE_D(D, 16),
          HWY_IF_T_SIZE_ONE_OF_D(D, (1 << 1) | (1 << 2))>
HWY_API VFromD<D> MaskedLoad(MFromD<D> m, D d,
                             const TFromD<D>* HWY_RESTRICT p) {
  return IfThenElseZero(m, LoadU(d, p));
}

#else  // <= SSE4

// Avoid maskmov* - its nontemporal 'hint' causes it to bypass caches (slow).
template <class D, HWY_IF_V_SIZE_LE_D(D, 16), typename T = TFromD<D>>
HWY_API VFromD<D> MaskedLoad(MFromD<D> m, D d, const T* HWY_RESTRICT p) {
  return IfThenElseZero(m, LoadU(d, p));
}

#endif

// ------------------------------ MaskedLoadOr

#if HWY_TARGET > HWY_AVX3  // else: native

// For all vector widths
template <class D, typename T = TFromD<D>>
HWY_API VFromD<D> MaskedLoadOr(VFromD<D> v, MFromD<D> m, D d,
                               const T* HWY_RESTRICT p) {
  return IfThenElse(m, LoadU(d, p), v);
}

#endif  // HWY_TARGET > HWY_AVX3

// ------------------------------ BlendedStore

namespace detail {

// There is no maskload_epi8/16 with which we could safely implement
// BlendedStore. Manual blending is also unsafe because loading a full vector
// that crosses the array end causes asan faults. Resort to scalar code; the
// caller should instead use memcpy, assuming m is FirstN(d, n).
template <class D>
HWY_API void ScalarMaskedStore(VFromD<D> v, MFromD<D> m, D d,
                               TFromD<D>* HWY_RESTRICT p) {
  const RebindToSigned<decltype(d)> di;  // for testing mask if T=bfloat16_t.
  using TI = TFromD<decltype(di)>;
  alignas(16) TI buf[MaxLanes(d)];
  alignas(16) TI mask[MaxLanes(d)];
  Store(BitCast(di, v), di, buf);
  Store(BitCast(di, VecFromMask(d, m)), di, mask);
  for (size_t i = 0; i < MaxLanes(d); ++i) {
    if (mask[i]) {
      CopySameSize(buf + i, p + i);
    }
  }
}
}  // namespace detail

#if HWY_TARGET <= HWY_AVX3

template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_T_SIZE_D(D, 1)>
HWY_API void BlendedStore(VFromD<D> v, MFromD<D> m, D /* tag */,
                          TFromD<D>* HWY_RESTRICT p) {
  _mm_mask_storeu_epi8(p, m.raw, v.raw);
}
template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_T_SIZE_D(D, 2)>
HWY_API void BlendedStore(VFromD<D> v, MFromD<D> m, D /* tag */,
                          TFromD<D>* HWY_RESTRICT p) {
  _mm_mask_storeu_epi16(p, m.raw, v.raw);
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_UI32_D(D)>
HWY_API void BlendedStore(VFromD<D> v, MFromD<D> m, D /* tag */,
                          TFromD<D>* HWY_RESTRICT p) {
  auto pi = reinterpret_cast<int*>(p);  // NOLINT
  _mm_mask_storeu_epi32(pi, m.raw, v.raw);
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_UI64_D(D)>
HWY_API void BlendedStore(VFromD<D> v, MFromD<D> m, D /* tag */,
                          TFromD<D>* HWY_RESTRICT p) {
  auto pi = reinterpret_cast<long long*>(p);  // NOLINT
  _mm_mask_storeu_epi64(pi, m.raw, v.raw);
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_F32_D(D)>
HWY_API void BlendedStore(VFromD<D> v, MFromD<D> m, D, float* HWY_RESTRICT p) {
  _mm_mask_storeu_ps(p, m.raw, v.raw);
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_F64_D(D)>
HWY_API void BlendedStore(VFromD<D> v, MFromD<D> m, D, double* HWY_RESTRICT p) {
  _mm_mask_storeu_pd(p, m.raw, v.raw);
}

#elif HWY_TARGET == HWY_AVX2

template <class D, HWY_IF_V_SIZE_LE_D(D, 16),
          HWY_IF_T_SIZE_ONE_OF_D(D, (1 << 1) | (1 << 2))>
HWY_API void BlendedStore(VFromD<D> v, MFromD<D> m, D d,
                          TFromD<D>* HWY_RESTRICT p) {
  detail::ScalarMaskedStore(v, m, d, p);
}

namespace detail {

template <class D, class V, class M, HWY_IF_UI32_D(D)>
HWY_INLINE void NativeBlendedStore(V v, M m, TFromD<D>* HWY_RESTRICT p) {
  auto pi = reinterpret_cast<int*>(p);  // NOLINT
  _mm_maskstore_epi32(pi, m.raw, v.raw);
}

template <class D, class V, class M, HWY_IF_UI64_D(D)>
HWY_INLINE void NativeBlendedStore(V v, M m, TFromD<D>* HWY_RESTRICT p) {
  auto pi = reinterpret_cast<long long*>(p);  // NOLINT
  _mm_maskstore_epi64(pi, m.raw, v.raw);
}

template <class D, class V, class M, HWY_IF_F32_D(D)>
HWY_INLINE void NativeBlendedStore(V v, M m, float* HWY_RESTRICT p) {
  _mm_maskstore_ps(p, m.raw, v.raw);
}

template <class D, class V, class M, HWY_IF_F64_D(D)>
HWY_INLINE void NativeBlendedStore(V v, M m, double* HWY_RESTRICT p) {
  _mm_maskstore_pd(p, m.raw, v.raw);
}

}  // namespace detail

template <class D, HWY_IF_V_SIZE_LE_D(D, 16),
          HWY_IF_T_SIZE_ONE_OF_D(D, (1 << 4) | (1 << 8))>
HWY_API void BlendedStore(VFromD<D> v, MFromD<D> m, D d,
                          TFromD<D>* HWY_RESTRICT p) {
  const RebindToSigned<decltype(d)> di;
  // For partial vectors, avoid writing other lanes by zeroing their mask.
  if (d.MaxBytes() < 16) {
    const Full128<TFromD<D>> dfull;
    const Mask128<TFromD<D>> mfull{m.raw};
    m = MFromD<D>{And(mfull, FirstN(dfull, MaxLanes(d))).raw};
  }

  // Float/double require, and unsigned ints tolerate, signed int masks.
  detail::NativeBlendedStore<D>(v, RebindMask(di, m), p);
}

#else  // <= SSE4

template <class D, HWY_IF_V_SIZE_LE_D(D, 16)>
HWY_API void BlendedStore(VFromD<D> v, MFromD<D> m, D d,
                          TFromD<D>* HWY_RESTRICT p) {
  // Avoid maskmov* - its nontemporal 'hint' causes it to bypass caches (slow).
  detail::ScalarMaskedStore(v, m, d, p);
}

#endif  // SSE4

// ================================================== ARITHMETIC

// ------------------------------ Addition

// Unsigned
template <size_t N>
HWY_API Vec128<uint8_t, N> operator+(const Vec128<uint8_t, N> a,
                                     const Vec128<uint8_t, N> b) {
  return Vec128<uint8_t, N>{_mm_add_epi8(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<uint16_t, N> operator+(const Vec128<uint16_t, N> a,
                                      const Vec128<uint16_t, N> b) {
  return Vec128<uint16_t, N>{_mm_add_epi16(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<uint32_t, N> operator+(const Vec128<uint32_t, N> a,
                                      const Vec128<uint32_t, N> b) {
  return Vec128<uint32_t, N>{_mm_add_epi32(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<uint64_t, N> operator+(const Vec128<uint64_t, N> a,
                                      const Vec128<uint64_t, N> b) {
  return Vec128<uint64_t, N>{_mm_add_epi64(a.raw, b.raw)};
}

// Signed
template <size_t N>
HWY_API Vec128<int8_t, N> operator+(const Vec128<int8_t, N> a,
                                    const Vec128<int8_t, N> b) {
  return Vec128<int8_t, N>{_mm_add_epi8(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<int16_t, N> operator+(const Vec128<int16_t, N> a,
                                     const Vec128<int16_t, N> b) {
  return Vec128<int16_t, N>{_mm_add_epi16(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<int32_t, N> operator+(const Vec128<int32_t, N> a,
                                     const Vec128<int32_t, N> b) {
  return Vec128<int32_t, N>{_mm_add_epi32(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<int64_t, N> operator+(const Vec128<int64_t, N> a,
                                     const Vec128<int64_t, N> b) {
  return Vec128<int64_t, N>{_mm_add_epi64(a.raw, b.raw)};
}

// Float
template <size_t N>
HWY_API Vec128<float, N> operator+(const Vec128<float, N> a,
                                   const Vec128<float, N> b) {
  return Vec128<float, N>{_mm_add_ps(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<double, N> operator+(const Vec128<double, N> a,
                                    const Vec128<double, N> b) {
  return Vec128<double, N>{_mm_add_pd(a.raw, b.raw)};
}

// ------------------------------ Subtraction

// Unsigned
template <size_t N>
HWY_API Vec128<uint8_t, N> operator-(const Vec128<uint8_t, N> a,
                                     const Vec128<uint8_t, N> b) {
  return Vec128<uint8_t, N>{_mm_sub_epi8(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<uint16_t, N> operator-(Vec128<uint16_t, N> a,
                                      Vec128<uint16_t, N> b) {
  return Vec128<uint16_t, N>{_mm_sub_epi16(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<uint32_t, N> operator-(const Vec128<uint32_t, N> a,
                                      const Vec128<uint32_t, N> b) {
  return Vec128<uint32_t, N>{_mm_sub_epi32(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<uint64_t, N> operator-(const Vec128<uint64_t, N> a,
                                      const Vec128<uint64_t, N> b) {
  return Vec128<uint64_t, N>{_mm_sub_epi64(a.raw, b.raw)};
}

// Signed
template <size_t N>
HWY_API Vec128<int8_t, N> operator-(const Vec128<int8_t, N> a,
                                    const Vec128<int8_t, N> b) {
  return Vec128<int8_t, N>{_mm_sub_epi8(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<int16_t, N> operator-(const Vec128<int16_t, N> a,
                                     const Vec128<int16_t, N> b) {
  return Vec128<int16_t, N>{_mm_sub_epi16(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<int32_t, N> operator-(const Vec128<int32_t, N> a,
                                     const Vec128<int32_t, N> b) {
  return Vec128<int32_t, N>{_mm_sub_epi32(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<int64_t, N> operator-(const Vec128<int64_t, N> a,
                                     const Vec128<int64_t, N> b) {
  return Vec128<int64_t, N>{_mm_sub_epi64(a.raw, b.raw)};
}

// Float
template <size_t N>
HWY_API Vec128<float, N> operator-(const Vec128<float, N> a,
                                   const Vec128<float, N> b) {
  return Vec128<float, N>{_mm_sub_ps(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<double, N> operator-(const Vec128<double, N> a,
                                    const Vec128<double, N> b) {
  return Vec128<double, N>{_mm_sub_pd(a.raw, b.raw)};
}

// ------------------------------ SumsOf8
template <size_t N>
HWY_API Vec128<uint64_t, N / 8> SumsOf8(const Vec128<uint8_t, N> v) {
  return Vec128<uint64_t, N / 8>{_mm_sad_epu8(v.raw, _mm_setzero_si128())};
}

#ifdef HWY_NATIVE_SUMS_OF_8_ABS_DIFF
#undef HWY_NATIVE_SUMS_OF_8_ABS_DIFF
#else
#define HWY_NATIVE_SUMS_OF_8_ABS_DIFF
#endif

template <size_t N>
HWY_API Vec128<uint64_t, N / 8> SumsOf8AbsDiff(const Vec128<uint8_t, N> a,
                                               const Vec128<uint8_t, N> b) {
  return Vec128<uint64_t, N / 8>{_mm_sad_epu8(a.raw, b.raw)};
}

// ------------------------------ SaturatedAdd

// Returns a + b clamped to the destination range.

// Unsigned
template <size_t N>
HWY_API Vec128<uint8_t, N> SaturatedAdd(const Vec128<uint8_t, N> a,
                                        const Vec128<uint8_t, N> b) {
  return Vec128<uint8_t, N>{_mm_adds_epu8(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<uint16_t, N> SaturatedAdd(const Vec128<uint16_t, N> a,
                                         const Vec128<uint16_t, N> b) {
  return Vec128<uint16_t, N>{_mm_adds_epu16(a.raw, b.raw)};
}

// Signed
template <size_t N>
HWY_API Vec128<int8_t, N> SaturatedAdd(const Vec128<int8_t, N> a,
                                       const Vec128<int8_t, N> b) {
  return Vec128<int8_t, N>{_mm_adds_epi8(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<int16_t, N> SaturatedAdd(const Vec128<int16_t, N> a,
                                        const Vec128<int16_t, N> b) {
  return Vec128<int16_t, N>{_mm_adds_epi16(a.raw, b.raw)};
}

#if HWY_TARGET <= HWY_AVX3
#ifdef HWY_NATIVE_I32_SATURATED_ADDSUB
#undef HWY_NATIVE_I32_SATURATED_ADDSUB
#else
#define HWY_NATIVE_I32_SATURATED_ADDSUB
#endif

#ifdef HWY_NATIVE_I64_SATURATED_ADDSUB
#undef HWY_NATIVE_I64_SATURATED_ADDSUB
#else
#define HWY_NATIVE_I64_SATURATED_ADDSUB
#endif

template <size_t N>
HWY_API Vec128<int32_t, N> SaturatedAdd(Vec128<int32_t, N> a,
                                        Vec128<int32_t, N> b) {
  const DFromV<decltype(a)> d;
  const auto sum = a + b;
  const auto overflow_mask = MaskFromVec(
      Vec128<int32_t, N>{_mm_ternarylogic_epi32(a.raw, b.raw, sum.raw, 0x42)});
  const auto i32_max = Set(d, LimitsMax<int32_t>());
  const Vec128<int32_t, N> overflow_result{_mm_mask_ternarylogic_epi32(
      i32_max.raw, MaskFromVec(a).raw, i32_max.raw, i32_max.raw, 0x55)};
  return IfThenElse(overflow_mask, overflow_result, sum);
}

template <size_t N>
HWY_API Vec128<int64_t, N> SaturatedAdd(Vec128<int64_t, N> a,
                                        Vec128<int64_t, N> b) {
  const DFromV<decltype(a)> d;
  const auto sum = a + b;
  const auto overflow_mask = MaskFromVec(
      Vec128<int64_t, N>{_mm_ternarylogic_epi64(a.raw, b.raw, sum.raw, 0x42)});
  const auto i64_max = Set(d, LimitsMax<int64_t>());
  const Vec128<int64_t, N> overflow_result{_mm_mask_ternarylogic_epi64(
      i64_max.raw, MaskFromVec(a).raw, i64_max.raw, i64_max.raw, 0x55)};
  return IfThenElse(overflow_mask, overflow_result, sum);
}
#endif  // HWY_TARGET <= HWY_AVX3

// ------------------------------ SaturatedSub

// Returns a - b clamped to the destination range.

// Unsigned
template <size_t N>
HWY_API Vec128<uint8_t, N> SaturatedSub(const Vec128<uint8_t, N> a,
                                        const Vec128<uint8_t, N> b) {
  return Vec128<uint8_t, N>{_mm_subs_epu8(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<uint16_t, N> SaturatedSub(const Vec128<uint16_t, N> a,
                                         const Vec128<uint16_t, N> b) {
  return Vec128<uint16_t, N>{_mm_subs_epu16(a.raw, b.raw)};
}

// Signed
template <size_t N>
HWY_API Vec128<int8_t, N> SaturatedSub(const Vec128<int8_t, N> a,
                                       const Vec128<int8_t, N> b) {
  return Vec128<int8_t, N>{_mm_subs_epi8(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<int16_t, N> SaturatedSub(const Vec128<int16_t, N> a,
                                        const Vec128<int16_t, N> b) {
  return Vec128<int16_t, N>{_mm_subs_epi16(a.raw, b.raw)};
}

#if HWY_TARGET <= HWY_AVX3
template <size_t N>
HWY_API Vec128<int32_t, N> SaturatedSub(Vec128<int32_t, N> a,
                                        Vec128<int32_t, N> b) {
  const DFromV<decltype(a)> d;
  const auto diff = a - b;
  const auto overflow_mask = MaskFromVec(
      Vec128<int32_t, N>{_mm_ternarylogic_epi32(a.raw, b.raw, diff.raw, 0x18)});
  const auto i32_max = Set(d, LimitsMax<int32_t>());
  const Vec128<int32_t, N> overflow_result{_mm_mask_ternarylogic_epi32(
      i32_max.raw, MaskFromVec(a).raw, i32_max.raw, i32_max.raw, 0x55)};
  return IfThenElse(overflow_mask, overflow_result, diff);
}

template <size_t N>
HWY_API Vec128<int64_t, N> SaturatedSub(Vec128<int64_t, N> a,
                                        Vec128<int64_t, N> b) {
  const DFromV<decltype(a)> d;
  const auto diff = a - b;
  const auto overflow_mask = MaskFromVec(
      Vec128<int64_t, N>{_mm_ternarylogic_epi64(a.raw, b.raw, diff.raw, 0x18)});
  const auto i64_max = Set(d, LimitsMax<int64_t>());
  const Vec128<int64_t, N> overflow_result{_mm_mask_ternarylogic_epi64(
      i64_max.raw, MaskFromVec(a).raw, i64_max.raw, i64_max.raw, 0x55)};
  return IfThenElse(overflow_mask, overflow_result, diff);
}
#endif  // HWY_TARGET <= HWY_AVX3

// ------------------------------ AverageRound

// Returns (a + b + 1) / 2

// Unsigned
template <size_t N>
HWY_API Vec128<uint8_t, N> AverageRound(const Vec128<uint8_t, N> a,
                                        const Vec128<uint8_t, N> b) {
  return Vec128<uint8_t, N>{_mm_avg_epu8(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<uint16_t, N> AverageRound(const Vec128<uint16_t, N> a,
                                         const Vec128<uint16_t, N> b) {
  return Vec128<uint16_t, N>{_mm_avg_epu16(a.raw, b.raw)};
}

// ------------------------------ Integer multiplication

template <size_t N>
HWY_API Vec128<uint16_t, N> operator*(const Vec128<uint16_t, N> a,
                                      const Vec128<uint16_t, N> b) {
  return Vec128<uint16_t, N>{_mm_mullo_epi16(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<int16_t, N> operator*(const Vec128<int16_t, N> a,
                                     const Vec128<int16_t, N> b) {
  return Vec128<int16_t, N>{_mm_mullo_epi16(a.raw, b.raw)};
}

// Returns the upper 16 bits of a * b in each lane.
template <size_t N>
HWY_API Vec128<uint16_t, N> MulHigh(const Vec128<uint16_t, N> a,
                                    const Vec128<uint16_t, N> b) {
  return Vec128<uint16_t, N>{_mm_mulhi_epu16(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<int16_t, N> MulHigh(const Vec128<int16_t, N> a,
                                   const Vec128<int16_t, N> b) {
  return Vec128<int16_t, N>{_mm_mulhi_epi16(a.raw, b.raw)};
}

// Multiplies even lanes (0, 2 ..) and places the double-wide result into
// even and the upper half into its odd neighbor lane.
template <size_t N>
HWY_API Vec128<uint64_t, (N + 1) / 2> MulEven(const Vec128<uint32_t, N> a,
                                              const Vec128<uint32_t, N> b) {
  return Vec128<uint64_t, (N + 1) / 2>{_mm_mul_epu32(a.raw, b.raw)};
}

#if HWY_TARGET >= HWY_SSSE3

template <size_t N, HWY_IF_V_SIZE_LE(int32_t, N, 8)>  // N=1 or 2
HWY_API Vec128<int64_t, (N + 1) / 2> MulEven(const Vec128<int32_t, N> a,
                                             const Vec128<int32_t, N> b) {
  const DFromV<decltype(a)> d;
  const RepartitionToWide<decltype(d)> dw;
  return Set(dw, static_cast<int64_t>(GetLane(a)) * GetLane(b));
}
HWY_API Vec128<int64_t> MulEven(Vec128<int32_t> a, Vec128<int32_t> b) {
  alignas(16) int32_t a_lanes[4];
  alignas(16) int32_t b_lanes[4];
  const DFromV<decltype(a)> di32;
  const RepartitionToWide<decltype(di32)> di64;
  Store(a, di32, a_lanes);
  Store(b, di32, b_lanes);
  alignas(16) int64_t mul[2];
  mul[0] = static_cast<int64_t>(a_lanes[0]) * b_lanes[0];
  mul[1] = static_cast<int64_t>(a_lanes[2]) * b_lanes[2];
  return Load(di64, mul);
}

#else  // HWY_TARGET < HWY_SSSE3

template <size_t N>
HWY_API Vec128<int64_t, (N + 1) / 2> MulEven(const Vec128<int32_t, N> a,
                                             const Vec128<int32_t, N> b) {
  return Vec128<int64_t, (N + 1) / 2>{_mm_mul_epi32(a.raw, b.raw)};
}

#endif  // HWY_TARGET >= HWY_SSSE3

template <size_t N>
HWY_API Vec128<uint32_t, N> operator*(const Vec128<uint32_t, N> a,
                                      const Vec128<uint32_t, N> b) {
#if HWY_TARGET >= HWY_SSSE3
  // Not as inefficient as it looks: _mm_mullo_epi32 has 10 cycle latency.
  // 64-bit right shift would also work but also needs port 5, so no benefit.
  // Notation: x=don't care, z=0.
  const __m128i a_x3x1 = _mm_shuffle_epi32(a.raw, _MM_SHUFFLE(3, 3, 1, 1));
  const auto mullo_x2x0 = MulEven(a, b);
  const __m128i b_x3x1 = _mm_shuffle_epi32(b.raw, _MM_SHUFFLE(3, 3, 1, 1));
  const auto mullo_x3x1 =
      MulEven(Vec128<uint32_t, N>{a_x3x1}, Vec128<uint32_t, N>{b_x3x1});
  // We could _mm_slli_epi64 by 32 to get 3z1z and OR with z2z0, but generating
  // the latter requires one more instruction or a constant.
  const __m128i mul_20 =
      _mm_shuffle_epi32(mullo_x2x0.raw, _MM_SHUFFLE(2, 0, 2, 0));
  const __m128i mul_31 =
      _mm_shuffle_epi32(mullo_x3x1.raw, _MM_SHUFFLE(2, 0, 2, 0));
  return Vec128<uint32_t, N>{_mm_unpacklo_epi32(mul_20, mul_31)};
#else
  return Vec128<uint32_t, N>{_mm_mullo_epi32(a.raw, b.raw)};
#endif
}

template <size_t N>
HWY_API Vec128<int32_t, N> operator*(const Vec128<int32_t, N> a,
                                     const Vec128<int32_t, N> b) {
  // Same as unsigned; avoid duplicating the SSSE3 code.
  const DFromV<decltype(a)> d;
  const RebindToUnsigned<decltype(d)> du;
  return BitCast(d, BitCast(du, a) * BitCast(du, b));
}

// ------------------------------ RotateRight (ShiftRight, Or)

template <int kBits, typename T, size_t N,
          HWY_IF_T_SIZE_ONE_OF(T, (1 << 1) | (1 << 2))>
HWY_API Vec128<T, N> RotateRight(const Vec128<T, N> v) {
  constexpr size_t kSizeInBits = sizeof(T) * 8;
  static_assert(0 <= kBits && kBits < kSizeInBits, "Invalid shift count");
  if (kBits == 0) return v;
  // AVX3 does not support 8/16-bit.
  return Or(ShiftRight<kBits>(v),
            ShiftLeft<HWY_MIN(kSizeInBits - 1, kSizeInBits - kBits)>(v));
}

template <int kBits, size_t N>
HWY_API Vec128<uint32_t, N> RotateRight(const Vec128<uint32_t, N> v) {
  static_assert(0 <= kBits && kBits < 32, "Invalid shift count");
#if HWY_TARGET <= HWY_AVX3
  return Vec128<uint32_t, N>{_mm_ror_epi32(v.raw, kBits)};
#else
  if (kBits == 0) return v;
  return Or(ShiftRight<kBits>(v), ShiftLeft<HWY_MIN(31, 32 - kBits)>(v));
#endif
}

template <int kBits, size_t N>
HWY_API Vec128<uint64_t, N> RotateRight(const Vec128<uint64_t, N> v) {
  static_assert(0 <= kBits && kBits < 64, "Invalid shift count");
#if HWY_TARGET <= HWY_AVX3
  return Vec128<uint64_t, N>{_mm_ror_epi64(v.raw, kBits)};
#else
  if (kBits == 0) return v;
  return Or(ShiftRight<kBits>(v), ShiftLeft<HWY_MIN(63, 64 - kBits)>(v));
#endif
}

// ------------------------------ BroadcastSignBit (ShiftRight, compare, mask)

template <size_t N>
HWY_API Vec128<int8_t, N> BroadcastSignBit(const Vec128<int8_t, N> v) {
  const DFromV<decltype(v)> d;
  return VecFromMask(v < Zero(d));
}

template <size_t N>
HWY_API Vec128<int16_t, N> BroadcastSignBit(const Vec128<int16_t, N> v) {
  return ShiftRight<15>(v);
}

template <size_t N>
HWY_API Vec128<int32_t, N> BroadcastSignBit(const Vec128<int32_t, N> v) {
  return ShiftRight<31>(v);
}

template <size_t N>
HWY_API Vec128<int64_t, N> BroadcastSignBit(const Vec128<int64_t, N> v) {
  const DFromV<decltype(v)> d;
#if HWY_TARGET <= HWY_AVX3
  (void)d;
  return Vec128<int64_t, N>{_mm_srai_epi64(v.raw, 63)};
#elif HWY_TARGET == HWY_AVX2 || HWY_TARGET == HWY_SSE4
  return VecFromMask(v < Zero(d));
#else
  // Efficient Lt() requires SSE4.2 and BLENDVPD requires SSE4.1. 32-bit shift
  // avoids generating a zero.
  const RepartitionToNarrow<decltype(d)> d32;
  const auto sign = ShiftRight<31>(BitCast(d32, v));
  return Vec128<int64_t, N>{
      _mm_shuffle_epi32(sign.raw, _MM_SHUFFLE(3, 3, 1, 1))};
#endif
}

// ------------------------------ Integer Abs

// Returns absolute value, except that LimitsMin() maps to LimitsMax() + 1.
template <size_t N>
HWY_API Vec128<int8_t, N> Abs(const Vec128<int8_t, N> v) {
#if HWY_COMPILER_MSVC || HWY_TARGET == HWY_SSE2
  const DFromV<decltype(v)> d;
  const RebindToUnsigned<decltype(d)> du;
  const auto zero = Zero(du);
  const auto v_as_u8 = BitCast(du, v);
  return BitCast(d, Min(v_as_u8, zero - v_as_u8));
#else
  return Vec128<int8_t, N>{_mm_abs_epi8(v.raw)};
#endif
}

template <size_t N>
HWY_API Vec128<int16_t, N> Abs(const Vec128<int16_t, N> v) {
#if HWY_TARGET == HWY_SSE2
  const auto zero = Zero(DFromV<decltype(v)>());
  return Max(v, zero - v);
#else
  return Vec128<int16_t, N>{_mm_abs_epi16(v.raw)};
#endif
}

template <size_t N>
HWY_API Vec128<int32_t, N> Abs(const Vec128<int32_t, N> v) {
#if HWY_TARGET <= HWY_SSSE3
  return Vec128<int32_t, N>{_mm_abs_epi32(v.raw)};
#else
  const auto zero = Zero(DFromV<decltype(v)>());
  return IfThenElse(MaskFromVec(BroadcastSignBit(v)), zero - v, v);
#endif
}

template <size_t N>
HWY_API Vec128<int64_t, N> Abs(const Vec128<int64_t, N> v) {
#if HWY_TARGET <= HWY_AVX3
  return Vec128<int64_t, N>{_mm_abs_epi64(v.raw)};
#else
  const auto zero = Zero(DFromV<decltype(v)>());
  return IfThenElse(MaskFromVec(BroadcastSignBit(v)), zero - v, v);
#endif
}

// GCC and older Clang do not follow the Intel documentation for AVX-512VL
// srli_epi64: the count should be unsigned int. Note that this is not the same
// as the Shift3264Count in x86_512-inl.h (GCC also requires int).
#if (HWY_COMPILER_CLANG && HWY_COMPILER_CLANG < 1100) || HWY_COMPILER_GCC_ACTUAL
using Shift64Count = int;
#else
// Assume documented behavior. Clang 12 and MSVC 14.28.29910 match this.
using Shift64Count = unsigned int;
#endif

template <int kBits, size_t N>
HWY_API Vec128<int64_t, N> ShiftRight(const Vec128<int64_t, N> v) {
#if HWY_TARGET <= HWY_AVX3
  return Vec128<int64_t, N>{
      _mm_srai_epi64(v.raw, static_cast<Shift64Count>(kBits))};
#else
  const DFromV<decltype(v)> di;
  const RebindToUnsigned<decltype(di)> du;
  const auto right = BitCast(di, ShiftRight<kBits>(BitCast(du, v)));
  const auto sign = ShiftLeft<64 - kBits>(BroadcastSignBit(v));
  return right | sign;
#endif
}

// ------------------------------ ZeroIfNegative (BroadcastSignBit)
template <typename T, size_t N>
HWY_API Vec128<T, N> ZeroIfNegative(Vec128<T, N> v) {
  static_assert(IsFloat<T>(), "Only works for float");
  const DFromV<decltype(v)> d;
#if HWY_TARGET >= HWY_SSSE3
  const RebindToSigned<decltype(d)> di;
  const auto mask = MaskFromVec(BitCast(d, BroadcastSignBit(BitCast(di, v))));
#else
  const auto mask = MaskFromVec(v);  // MSB is sufficient for BLENDVPS
#endif
  return IfThenElse(mask, Zero(d), v);
}

// ------------------------------ IfNegativeThenElse
template <size_t N>
HWY_API Vec128<int8_t, N> IfNegativeThenElse(const Vec128<int8_t, N> v,
                                             const Vec128<int8_t, N> yes,
                                             const Vec128<int8_t, N> no) {
  // int8: IfThenElse only looks at the MSB.
  return IfThenElse(MaskFromVec(v), yes, no);
}

template <typename T, size_t N, HWY_IF_T_SIZE(T, 2)>
HWY_API Vec128<T, N> IfNegativeThenElse(Vec128<T, N> v, Vec128<T, N> yes,
                                        Vec128<T, N> no) {
  static_assert(IsSigned<T>(), "Only works for signed/float");
  const DFromV<decltype(v)> d;
  const RebindToSigned<decltype(d)> di;

  // 16-bit: no native blendv, so copy sign to lower byte's MSB.
  v = BitCast(d, BroadcastSignBit(BitCast(di, v)));
  return IfThenElse(MaskFromVec(v), yes, no);
}

template <typename T, size_t N, HWY_IF_NOT_T_SIZE(T, 2)>
HWY_API Vec128<T, N> IfNegativeThenElse(Vec128<T, N> v, Vec128<T, N> yes,
                                        Vec128<T, N> no) {
  static_assert(IsSigned<T>(), "Only works for signed/float");
  const DFromV<decltype(v)> d;
  const RebindToFloat<decltype(d)> df;

  // 32/64-bit: use float IfThenElse, which only looks at the MSB.
  return BitCast(d, IfThenElse(MaskFromVec(BitCast(df, v)), BitCast(df, yes),
                               BitCast(df, no)));
}

// ------------------------------ ShiftLeftSame

template <size_t N>
HWY_API Vec128<uint16_t, N> ShiftLeftSame(const Vec128<uint16_t, N> v,
                                          const int bits) {
#if HWY_COMPILER_GCC
  if (__builtin_constant_p(bits)) {
    return Vec128<uint16_t, N>{_mm_slli_epi16(v.raw, bits)};
  }
#endif
  return Vec128<uint16_t, N>{_mm_sll_epi16(v.raw, _mm_cvtsi32_si128(bits))};
}
template <size_t N>
HWY_API Vec128<uint32_t, N> ShiftLeftSame(const Vec128<uint32_t, N> v,
                                          const int bits) {
#if HWY_COMPILER_GCC
  if (__builtin_constant_p(bits)) {
    return Vec128<uint32_t, N>{_mm_slli_epi32(v.raw, bits)};
  }
#endif
  return Vec128<uint32_t, N>{_mm_sll_epi32(v.raw, _mm_cvtsi32_si128(bits))};
}
template <size_t N>
HWY_API Vec128<uint64_t, N> ShiftLeftSame(const Vec128<uint64_t, N> v,
                                          const int bits) {
#if HWY_COMPILER_GCC
  if (__builtin_constant_p(bits)) {
    return Vec128<uint64_t, N>{_mm_slli_epi64(v.raw, bits)};
  }
#endif
  return Vec128<uint64_t, N>{_mm_sll_epi64(v.raw, _mm_cvtsi32_si128(bits))};
}

template <size_t N>
HWY_API Vec128<int16_t, N> ShiftLeftSame(const Vec128<int16_t, N> v,
                                         const int bits) {
#if HWY_COMPILER_GCC
  if (__builtin_constant_p(bits)) {
    return Vec128<int16_t, N>{_mm_slli_epi16(v.raw, bits)};
  }
#endif
  return Vec128<int16_t, N>{_mm_sll_epi16(v.raw, _mm_cvtsi32_si128(bits))};
}

template <size_t N>
HWY_API Vec128<int32_t, N> ShiftLeftSame(const Vec128<int32_t, N> v,
                                         const int bits) {
#if HWY_COMPILER_GCC
  if (__builtin_constant_p(bits)) {
    return Vec128<int32_t, N>{_mm_slli_epi32(v.raw, bits)};
  }
#endif
  return Vec128<int32_t, N>{_mm_sll_epi32(v.raw, _mm_cvtsi32_si128(bits))};
}

template <size_t N>
HWY_API Vec128<int64_t, N> ShiftLeftSame(const Vec128<int64_t, N> v,
                                         const int bits) {
#if HWY_COMPILER_GCC
  if (__builtin_constant_p(bits)) {
    return Vec128<int64_t, N>{_mm_slli_epi64(v.raw, bits)};
  }
#endif
  return Vec128<int64_t, N>{_mm_sll_epi64(v.raw, _mm_cvtsi32_si128(bits))};
}

template <typename T, size_t N, HWY_IF_T_SIZE(T, 1)>
HWY_API Vec128<T, N> ShiftLeftSame(const Vec128<T, N> v, const int bits) {
  const DFromV<decltype(v)> d8;
  // Use raw instead of BitCast to support N=1.
  const Vec128<T, N> shifted{
      ShiftLeftSame(Vec128<MakeWide<T>>{v.raw}, bits).raw};
  return shifted & Set(d8, static_cast<T>((0xFF << bits) & 0xFF));
}

// ------------------------------ ShiftRightSame (BroadcastSignBit)

template <size_t N>
HWY_API Vec128<uint16_t, N> ShiftRightSame(const Vec128<uint16_t, N> v,
                                           const int bits) {
#if HWY_COMPILER_GCC
  if (__builtin_constant_p(bits)) {
    return Vec128<uint16_t, N>{_mm_srli_epi16(v.raw, bits)};
  }
#endif
  return Vec128<uint16_t, N>{_mm_srl_epi16(v.raw, _mm_cvtsi32_si128(bits))};
}
template <size_t N>
HWY_API Vec128<uint32_t, N> ShiftRightSame(const Vec128<uint32_t, N> v,
                                           const int bits) {
#if HWY_COMPILER_GCC
  if (__builtin_constant_p(bits)) {
    return Vec128<uint32_t, N>{_mm_srli_epi32(v.raw, bits)};
  }
#endif
  return Vec128<uint32_t, N>{_mm_srl_epi32(v.raw, _mm_cvtsi32_si128(bits))};
}
template <size_t N>
HWY_API Vec128<uint64_t, N> ShiftRightSame(const Vec128<uint64_t, N> v,
                                           const int bits) {
#if HWY_COMPILER_GCC
  if (__builtin_constant_p(bits)) {
    return Vec128<uint64_t, N>{_mm_srli_epi64(v.raw, bits)};
  }
#endif
  return Vec128<uint64_t, N>{_mm_srl_epi64(v.raw, _mm_cvtsi32_si128(bits))};
}

template <size_t N>
HWY_API Vec128<uint8_t, N> ShiftRightSame(Vec128<uint8_t, N> v,
                                          const int bits) {
  const DFromV<decltype(v)> d8;
  // Use raw instead of BitCast to support N=1.
  const Vec128<uint8_t, N> shifted{
      ShiftRightSame(Vec128<uint16_t>{v.raw}, bits).raw};
  return shifted & Set(d8, static_cast<uint8_t>(0xFF >> bits));
}

template <size_t N>
HWY_API Vec128<int16_t, N> ShiftRightSame(const Vec128<int16_t, N> v,
                                          const int bits) {
#if HWY_COMPILER_GCC
  if (__builtin_constant_p(bits)) {
    return Vec128<int16_t, N>{_mm_srai_epi16(v.raw, bits)};
  }
#endif
  return Vec128<int16_t, N>{_mm_sra_epi16(v.raw, _mm_cvtsi32_si128(bits))};
}

template <size_t N>
HWY_API Vec128<int32_t, N> ShiftRightSame(const Vec128<int32_t, N> v,
                                          const int bits) {
#if HWY_COMPILER_GCC
  if (__builtin_constant_p(bits)) {
    return Vec128<int32_t, N>{_mm_srai_epi32(v.raw, bits)};
  }
#endif
  return Vec128<int32_t, N>{_mm_sra_epi32(v.raw, _mm_cvtsi32_si128(bits))};
}
template <size_t N>
HWY_API Vec128<int64_t, N> ShiftRightSame(const Vec128<int64_t, N> v,
                                          const int bits) {
#if HWY_TARGET <= HWY_AVX3
#if HWY_COMPILER_GCC
  if (__builtin_constant_p(bits)) {
    return Vec128<int64_t, N>{
        _mm_srai_epi64(v.raw, static_cast<Shift64Count>(bits))};
  }
#endif
  return Vec128<int64_t, N>{_mm_sra_epi64(v.raw, _mm_cvtsi32_si128(bits))};
#else
  const DFromV<decltype(v)> di;
  const RebindToUnsigned<decltype(di)> du;
  const auto right = BitCast(di, ShiftRightSame(BitCast(du, v), bits));
  const auto sign = ShiftLeftSame(BroadcastSignBit(v), 64 - bits);
  return right | sign;
#endif
}

template <size_t N>
HWY_API Vec128<int8_t, N> ShiftRightSame(Vec128<int8_t, N> v, const int bits) {
  const DFromV<decltype(v)> di;
  const RebindToUnsigned<decltype(di)> du;
  const auto shifted = BitCast(di, ShiftRightSame(BitCast(du, v), bits));
  const auto shifted_sign =
      BitCast(di, Set(du, static_cast<uint8_t>(0x80 >> bits)));
  return (shifted ^ shifted_sign) - shifted_sign;
}

// ------------------------------ Floating-point mul / div

template <size_t N>
HWY_API Vec128<float, N> operator*(Vec128<float, N> a, Vec128<float, N> b) {
  return Vec128<float, N>{_mm_mul_ps(a.raw, b.raw)};
}
HWY_API Vec128<float, 1> operator*(const Vec128<float, 1> a,
                                   const Vec128<float, 1> b) {
  return Vec128<float, 1>{_mm_mul_ss(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<double, N> operator*(const Vec128<double, N> a,
                                    const Vec128<double, N> b) {
  return Vec128<double, N>{_mm_mul_pd(a.raw, b.raw)};
}
HWY_API Vec64<double> operator*(const Vec64<double> a, const Vec64<double> b) {
  return Vec64<double>{_mm_mul_sd(a.raw, b.raw)};
}

template <size_t N>
HWY_API Vec128<float, N> operator/(const Vec128<float, N> a,
                                   const Vec128<float, N> b) {
  return Vec128<float, N>{_mm_div_ps(a.raw, b.raw)};
}
HWY_API Vec128<float, 1> operator/(const Vec128<float, 1> a,
                                   const Vec128<float, 1> b) {
  return Vec128<float, 1>{_mm_div_ss(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<double, N> operator/(const Vec128<double, N> a,
                                    const Vec128<double, N> b) {
  return Vec128<double, N>{_mm_div_pd(a.raw, b.raw)};
}
HWY_API Vec64<double> operator/(const Vec64<double> a, const Vec64<double> b) {
  return Vec64<double>{_mm_div_sd(a.raw, b.raw)};
}

// Approximate reciprocal
template <size_t N>
HWY_API Vec128<float, N> ApproximateReciprocal(const Vec128<float, N> v) {
  return Vec128<float, N>{_mm_rcp_ps(v.raw)};
}
HWY_API Vec128<float, 1> ApproximateReciprocal(const Vec128<float, 1> v) {
  return Vec128<float, 1>{_mm_rcp_ss(v.raw)};
}

// Absolute value of difference.
template <size_t N>
HWY_API Vec128<float, N> AbsDiff(Vec128<float, N> a, Vec128<float, N> b) {
  return Abs(a - b);
}

// ------------------------------ Floating-point multiply-add variants

// Returns mul * x + add
template <size_t N>
HWY_API Vec128<float, N> MulAdd(Vec128<float, N> mul, Vec128<float, N> x,
                                Vec128<float, N> add) {
#if HWY_TARGET >= HWY_SSE4
  return mul * x + add;
#else
  return Vec128<float, N>{_mm_fmadd_ps(mul.raw, x.raw, add.raw)};
#endif
}
template <size_t N>
HWY_API Vec128<double, N> MulAdd(Vec128<double, N> mul, Vec128<double, N> x,
                                 Vec128<double, N> add) {
#if HWY_TARGET >= HWY_SSE4
  return mul * x + add;
#else
  return Vec128<double, N>{_mm_fmadd_pd(mul.raw, x.raw, add.raw)};
#endif
}

// Returns add - mul * x
template <size_t N>
HWY_API Vec128<float, N> NegMulAdd(Vec128<float, N> mul, Vec128<float, N> x,
                                   Vec128<float, N> add) {
#if HWY_TARGET >= HWY_SSE4
  return add - mul * x;
#else
  return Vec128<float, N>{_mm_fnmadd_ps(mul.raw, x.raw, add.raw)};
#endif
}
template <size_t N>
HWY_API Vec128<double, N> NegMulAdd(Vec128<double, N> mul, Vec128<double, N> x,
                                    Vec128<double, N> add) {
#if HWY_TARGET >= HWY_SSE4
  return add - mul * x;
#else
  return Vec128<double, N>{_mm_fnmadd_pd(mul.raw, x.raw, add.raw)};
#endif
}

// Returns mul * x - sub
template <size_t N>
HWY_API Vec128<float, N> MulSub(Vec128<float, N> mul, Vec128<float, N> x,
                                Vec128<float, N> sub) {
#if HWY_TARGET >= HWY_SSE4
  return mul * x - sub;
#else
  return Vec128<float, N>{_mm_fmsub_ps(mul.raw, x.raw, sub.raw)};
#endif
}
template <size_t N>
HWY_API Vec128<double, N> MulSub(Vec128<double, N> mul, Vec128<double, N> x,
                                 Vec128<double, N> sub) {
#if HWY_TARGET >= HWY_SSE4
  return mul * x - sub;
#else
  return Vec128<double, N>{_mm_fmsub_pd(mul.raw, x.raw, sub.raw)};
#endif
}

// Returns -mul * x - sub
template <size_t N>
HWY_API Vec128<float, N> NegMulSub(Vec128<float, N> mul, Vec128<float, N> x,
                                   Vec128<float, N> sub) {
#if HWY_TARGET >= HWY_SSE4
  return Neg(mul) * x - sub;
#else
  return Vec128<float, N>{_mm_fnmsub_ps(mul.raw, x.raw, sub.raw)};
#endif
}
template <size_t N>
HWY_API Vec128<double, N> NegMulSub(Vec128<double, N> mul, Vec128<double, N> x,
                                    Vec128<double, N> sub) {
#if HWY_TARGET >= HWY_SSE4
  return Neg(mul) * x - sub;
#else
  return Vec128<double, N>{_mm_fnmsub_pd(mul.raw, x.raw, sub.raw)};
#endif
}

// ------------------------------ Floating-point square root

// Full precision square root
template <size_t N>
HWY_API Vec128<float, N> Sqrt(Vec128<float, N> v) {
  return Vec128<float, N>{_mm_sqrt_ps(v.raw)};
}
HWY_API Vec128<float, 1> Sqrt(Vec128<float, 1> v) {
  return Vec128<float, 1>{_mm_sqrt_ss(v.raw)};
}
template <size_t N>
HWY_API Vec128<double, N> Sqrt(Vec128<double, N> v) {
  return Vec128<double, N>{_mm_sqrt_pd(v.raw)};
}
HWY_API Vec64<double> Sqrt(Vec64<double> v) {
  return Vec64<double>{_mm_sqrt_sd(_mm_setzero_pd(), v.raw)};
}

// Approximate reciprocal square root
template <size_t N>
HWY_API Vec128<float, N> ApproximateReciprocalSqrt(Vec128<float, N> v) {
  return Vec128<float, N>{_mm_rsqrt_ps(v.raw)};
}
HWY_API Vec128<float, 1> ApproximateReciprocalSqrt(Vec128<float, 1> v) {
  return Vec128<float, 1>{_mm_rsqrt_ss(v.raw)};
}

// ------------------------------ Min (Gt, IfThenElse)

namespace detail {

template <typename T, size_t N>
HWY_INLINE HWY_MAYBE_UNUSED Vec128<T, N> MinU(const Vec128<T, N> a,
                                              const Vec128<T, N> b) {
  const DFromV<decltype(a)> d;
  const RebindToUnsigned<decltype(d)> du;
  const RebindToSigned<decltype(d)> di;
  const auto msb = Set(du, static_cast<T>(T(1) << (sizeof(T) * 8 - 1)));
  const auto gt = RebindMask(du, BitCast(di, a ^ msb) > BitCast(di, b ^ msb));
  return IfThenElse(gt, b, a);
}

}  // namespace detail

// Unsigned
template <size_t N>
HWY_API Vec128<uint8_t, N> Min(Vec128<uint8_t, N> a, Vec128<uint8_t, N> b) {
  return Vec128<uint8_t, N>{_mm_min_epu8(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<uint16_t, N> Min(Vec128<uint16_t, N> a, Vec128<uint16_t, N> b) {
#if HWY_TARGET >= HWY_SSSE3
  return detail::MinU(a, b);
#else
  return Vec128<uint16_t, N>{_mm_min_epu16(a.raw, b.raw)};
#endif
}
template <size_t N>
HWY_API Vec128<uint32_t, N> Min(Vec128<uint32_t, N> a, Vec128<uint32_t, N> b) {
#if HWY_TARGET >= HWY_SSSE3
  return detail::MinU(a, b);
#else
  return Vec128<uint32_t, N>{_mm_min_epu32(a.raw, b.raw)};
#endif
}
template <size_t N>
HWY_API Vec128<uint64_t, N> Min(Vec128<uint64_t, N> a, Vec128<uint64_t, N> b) {
#if HWY_TARGET <= HWY_AVX3
  return Vec128<uint64_t, N>{_mm_min_epu64(a.raw, b.raw)};
#else
  return detail::MinU(a, b);
#endif
}

// Signed
template <size_t N>
HWY_API Vec128<int8_t, N> Min(Vec128<int8_t, N> a, Vec128<int8_t, N> b) {
#if HWY_TARGET >= HWY_SSSE3
  return IfThenElse(a < b, a, b);
#else
  return Vec128<int8_t, N>{_mm_min_epi8(a.raw, b.raw)};
#endif
}
template <size_t N>
HWY_API Vec128<int16_t, N> Min(Vec128<int16_t, N> a, Vec128<int16_t, N> b) {
  return Vec128<int16_t, N>{_mm_min_epi16(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<int32_t, N> Min(Vec128<int32_t, N> a, Vec128<int32_t, N> b) {
#if HWY_TARGET >= HWY_SSSE3
  return IfThenElse(a < b, a, b);
#else
  return Vec128<int32_t, N>{_mm_min_epi32(a.raw, b.raw)};
#endif
}
template <size_t N>
HWY_API Vec128<int64_t, N> Min(Vec128<int64_t, N> a, Vec128<int64_t, N> b) {
#if HWY_TARGET <= HWY_AVX3
  return Vec128<int64_t, N>{_mm_min_epi64(a.raw, b.raw)};
#else
  return IfThenElse(a < b, a, b);
#endif
}

// Float
template <size_t N>
HWY_API Vec128<float, N> Min(Vec128<float, N> a, Vec128<float, N> b) {
  return Vec128<float, N>{_mm_min_ps(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<double, N> Min(Vec128<double, N> a, Vec128<double, N> b) {
  return Vec128<double, N>{_mm_min_pd(a.raw, b.raw)};
}

// ------------------------------ Max (Gt, IfThenElse)

namespace detail {
template <typename T, size_t N>
HWY_INLINE HWY_MAYBE_UNUSED Vec128<T, N> MaxU(const Vec128<T, N> a,
                                              const Vec128<T, N> b) {
  const DFromV<decltype(a)> d;
  const RebindToUnsigned<decltype(d)> du;
  const RebindToSigned<decltype(d)> di;
  const auto msb = Set(du, static_cast<T>(T(1) << (sizeof(T) * 8 - 1)));
  const auto gt = RebindMask(du, BitCast(di, a ^ msb) > BitCast(di, b ^ msb));
  return IfThenElse(gt, a, b);
}

}  // namespace detail

// Unsigned
template <size_t N>
HWY_API Vec128<uint8_t, N> Max(Vec128<uint8_t, N> a, Vec128<uint8_t, N> b) {
  return Vec128<uint8_t, N>{_mm_max_epu8(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<uint16_t, N> Max(Vec128<uint16_t, N> a, Vec128<uint16_t, N> b) {
#if HWY_TARGET >= HWY_SSSE3
  return detail::MaxU(a, b);
#else
  return Vec128<uint16_t, N>{_mm_max_epu16(a.raw, b.raw)};
#endif
}
template <size_t N>
HWY_API Vec128<uint32_t, N> Max(Vec128<uint32_t, N> a, Vec128<uint32_t, N> b) {
#if HWY_TARGET >= HWY_SSSE3
  return detail::MaxU(a, b);
#else
  return Vec128<uint32_t, N>{_mm_max_epu32(a.raw, b.raw)};
#endif
}
template <size_t N>
HWY_API Vec128<uint64_t, N> Max(Vec128<uint64_t, N> a, Vec128<uint64_t, N> b) {
#if HWY_TARGET <= HWY_AVX3
  return Vec128<uint64_t, N>{_mm_max_epu64(a.raw, b.raw)};
#else
  return detail::MaxU(a, b);
#endif
}

// Signed
template <size_t N>
HWY_API Vec128<int8_t, N> Max(Vec128<int8_t, N> a, Vec128<int8_t, N> b) {
#if HWY_TARGET >= HWY_SSSE3
  return IfThenElse(a < b, b, a);
#else
  return Vec128<int8_t, N>{_mm_max_epi8(a.raw, b.raw)};
#endif
}
template <size_t N>
HWY_API Vec128<int16_t, N> Max(Vec128<int16_t, N> a, Vec128<int16_t, N> b) {
  return Vec128<int16_t, N>{_mm_max_epi16(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<int32_t, N> Max(Vec128<int32_t, N> a, Vec128<int32_t, N> b) {
#if HWY_TARGET >= HWY_SSSE3
  return IfThenElse(a < b, b, a);
#else
  return Vec128<int32_t, N>{_mm_max_epi32(a.raw, b.raw)};
#endif
}
template <size_t N>
HWY_API Vec128<int64_t, N> Max(Vec128<int64_t, N> a, Vec128<int64_t, N> b) {
#if HWY_TARGET <= HWY_AVX3
  return Vec128<int64_t, N>{_mm_max_epi64(a.raw, b.raw)};
#else
  return IfThenElse(a < b, b, a);
#endif
}

// Float
template <size_t N>
HWY_API Vec128<float, N> Max(Vec128<float, N> a, Vec128<float, N> b) {
  return Vec128<float, N>{_mm_max_ps(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<double, N> Max(Vec128<double, N> a, Vec128<double, N> b) {
  return Vec128<double, N>{_mm_max_pd(a.raw, b.raw)};
}

// ================================================== MEMORY (3)

// ------------------------------ Non-temporal stores

// On clang6, we see incorrect code generated for _mm_stream_pi, so
// round even partial vectors up to 16 bytes.
template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_NOT_FLOAT_D(D)>
HWY_API void Stream(VFromD<D> v, D /* tag */, TFromD<D>* HWY_RESTRICT aligned) {
  _mm_stream_si128(reinterpret_cast<__m128i*>(aligned), v.raw);
}
template <class D, HWY_IF_V_SIZE_LE_D(D, 16)>
HWY_API void Stream(VFromD<D> v, D /* tag */, float* HWY_RESTRICT aligned) {
  _mm_stream_ps(aligned, v.raw);
}
template <class D, HWY_IF_V_SIZE_LE_D(D, 16)>
HWY_API void Stream(VFromD<D> v, D /* tag */, double* HWY_RESTRICT aligned) {
  _mm_stream_pd(aligned, v.raw);
}

// ------------------------------ Scatter

// Work around warnings in the intrinsic definitions (passing -1 as a mask).
HWY_DIAGNOSTICS(push)
HWY_DIAGNOSTICS_OFF(disable : 4245 4365, ignored "-Wsign-conversion")

// Unfortunately the GCC/Clang intrinsics do not accept int64_t*.
using GatherIndex64 = long long int;  // NOLINT(runtime/int)
static_assert(sizeof(GatherIndex64) == 8, "Must be 64-bit type");

#if HWY_TARGET <= HWY_AVX3
namespace detail {

template <int kScale, class D, class VI, HWY_IF_UI32_D(D)>
HWY_INLINE void NativeScatter128(VFromD<D> v, D d, TFromD<D>* HWY_RESTRICT base,
                                 VI index) {
  if (d.MaxBytes() == 16) {
    _mm_i32scatter_epi32(base, index.raw, v.raw, kScale);
  } else {
    const __mmask8 mask = (1u << MaxLanes(d)) - 1;
    _mm_mask_i32scatter_epi32(base, mask, index.raw, v.raw, kScale);
  }
}

template <int kScale, class D, class VI, HWY_IF_UI64_D(D)>
HWY_INLINE void NativeScatter128(VFromD<D> v, D d, TFromD<D>* HWY_RESTRICT base,
                                 VI index) {
  if (d.MaxBytes() == 16) {
    _mm_i64scatter_epi64(base, index.raw, v.raw, kScale);
  } else {
    const __mmask8 mask = (1u << MaxLanes(d)) - 1;
    _mm_mask_i64scatter_epi64(base, mask, index.raw, v.raw, kScale);
  }
}

template <int kScale, class D, class VI, HWY_IF_F32_D(D)>
HWY_INLINE void NativeScatter128(VFromD<D> v, D d, float* HWY_RESTRICT base,
                                 VI index) {
  if (d.MaxBytes() == 16) {
    _mm_i32scatter_ps(base, index.raw, v.raw, kScale);
  } else {
    const __mmask8 mask = (1u << MaxLanes(d)) - 1;
    _mm_mask_i32scatter_ps(base, mask, index.raw, v.raw, kScale);
  }
}

template <int kScale, class D, class VI, HWY_IF_F64_D(D)>
HWY_INLINE void NativeScatter128(VFromD<D> v, D d, double* HWY_RESTRICT base,
                                 VI index) {
  if (d.MaxBytes() == 16) {
    _mm_i64scatter_pd(base, index.raw, v.raw, kScale);
  } else {
    const __mmask8 mask = (1u << MaxLanes(d)) - 1;
    _mm_mask_i64scatter_pd(base, mask, index.raw, v.raw, kScale);
  }
}

}  // namespace detail

template <class D, HWY_IF_V_SIZE_LE_D(D, 16), typename T = TFromD<D>, class VI>
HWY_API void ScatterOffset(VFromD<D> v, D d, T* HWY_RESTRICT base, VI offset) {
  static_assert(sizeof(T) == sizeof(TFromV<VI>), "Index/lane size must match");
  return detail::NativeScatter128<1>(v, d, base, offset);
}
template <class D, HWY_IF_V_SIZE_LE_D(D, 16), typename T = TFromD<D>, class VI>
HWY_API void ScatterIndex(VFromD<D> v, D d, T* HWY_RESTRICT base, VI index) {
  static_assert(sizeof(T) == sizeof(TFromV<VI>), "Index/lane size must match");
  return detail::NativeScatter128<sizeof(T)>(v, d, base, index);
}

#else  // HWY_TARGET <= HWY_AVX3

template <class D, HWY_IF_V_SIZE_LE_D(D, 16), typename T = TFromD<D>, class VI>
HWY_API void ScatterOffset(VFromD<D> v, D d, T* HWY_RESTRICT base, VI offset) {
  using TI = TFromV<VI>;
  static_assert(sizeof(T) == sizeof(TI), "Index/lane size must match");

  alignas(16) T lanes[MaxLanes(d)];
  Store(v, d, lanes);

  alignas(16) TI offset_lanes[MaxLanes(d)];
  Store(offset, Rebind<TI, decltype(d)>(), offset_lanes);

  uint8_t* base_bytes = reinterpret_cast<uint8_t*>(base);
  for (size_t i = 0; i < MaxLanes(d); ++i) {
    CopyBytes<sizeof(T)>(&lanes[i], base_bytes + offset_lanes[i]);
  }
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 16), typename T = TFromD<D>, class VI>
HWY_API void ScatterIndex(VFromD<D> v, D d, T* HWY_RESTRICT base, VI index) {
  using TI = TFromV<VI>;
  static_assert(sizeof(T) == sizeof(TI), "Index/lane size must match");

  alignas(16) T lanes[MaxLanes(d)];
  Store(v, d, lanes);

  alignas(16) TI index_lanes[MaxLanes(d)];
  Store(index, Rebind<TI, decltype(d)>(), index_lanes);

  for (size_t i = 0; i < MaxLanes(d); ++i) {
    base[index_lanes[i]] = lanes[i];
  }
}

#endif

// ------------------------------ Gather (Load/Store)

#if HWY_TARGET >= HWY_SSE4

template <class D, HWY_IF_V_SIZE_LE_D(D, 16), typename T = TFromD<D>, class VI>
HWY_API VFromD<D> GatherOffset(D d, const T* HWY_RESTRICT base, VI offset) {
  using TI = TFromV<VI>;
  static_assert(sizeof(T) == sizeof(TI), "Index/lane size must match");

  alignas(16) TI offset_lanes[MaxLanes(d)];
  Store(offset, Rebind<TI, decltype(d)>(), offset_lanes);

  alignas(16) T lanes[MaxLanes(d)];
  const uint8_t* base_bytes = reinterpret_cast<const uint8_t*>(base);
  for (size_t i = 0; i < MaxLanes(d); ++i) {
    CopyBytes<sizeof(T)>(base_bytes + offset_lanes[i], &lanes[i]);
  }
  return Load(d, lanes);
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 16), typename T = TFromD<D>, class VI>
HWY_API VFromD<D> GatherIndex(D d, const T* HWY_RESTRICT base, VI index) {
  using TI = TFromV<VI>;
  static_assert(sizeof(T) == sizeof(TI), "Index/lane size must match");

  alignas(16) TI index_lanes[MaxLanes(d)];
  Store(index, Rebind<TI, decltype(d)>(), index_lanes);

  alignas(16) T lanes[MaxLanes(d)];
  for (size_t i = 0; i < MaxLanes(d); ++i) {
    lanes[i] = base[index_lanes[i]];
  }
  return Load(d, lanes);
}

#else

namespace detail {

template <int kScale, class D, class VI, HWY_IF_UI32_D(D)>
HWY_INLINE VFromD<D> NativeGather128(D /* tag */,
                                     const TFromD<D>* HWY_RESTRICT base,
                                     VI index) {
  return VFromD<D>{_mm_i32gather_epi32(reinterpret_cast<const int32_t*>(base),
                                       index.raw, kScale)};
}

template <int kScale, class D, class VI, HWY_IF_UI64_D(D)>
HWY_INLINE VFromD<D> NativeGather128(D /* tag */,
                                     const TFromD<D>* HWY_RESTRICT base,
                                     VI index) {
  return VFromD<D>{_mm_i64gather_epi64(
      reinterpret_cast<const GatherIndex64*>(base), index.raw, kScale)};
}

template <int kScale, class D, class VI, HWY_IF_F32_D(D)>
HWY_INLINE VFromD<D> NativeGather128(D /* tag */,
                                     const float* HWY_RESTRICT base, VI index) {
  return VFromD<D>{_mm_i32gather_ps(base, index.raw, kScale)};
}

template <int kScale, class D, class VI, HWY_IF_F64_D(D)>
HWY_INLINE VFromD<D> NativeGather128(D /* tag */,
                                     const double* HWY_RESTRICT base,
                                     VI index) {
  return VFromD<D>{_mm_i64gather_pd(base, index.raw, kScale)};
}

}  // namespace detail

template <class D, HWY_IF_V_SIZE_LE_D(D, 16), typename T = TFromD<D>, class VI>
HWY_API VFromD<D> GatherOffset(D d, const T* HWY_RESTRICT base, VI offset) {
  static_assert(sizeof(T) == sizeof(TFromV<VI>), "Index/lane size must match");
  return detail::NativeGather128<1>(d, base, offset);
}
template <class D, HWY_IF_V_SIZE_LE_D(D, 16), typename T = TFromD<D>, class VI>
HWY_API VFromD<D> GatherIndex(D d, const T* HWY_RESTRICT base, VI index) {
  static_assert(sizeof(T) == sizeof(TFromV<VI>), "Index/lane size must match");
  return detail::NativeGather128<sizeof(T)>(d, base, index);
}

#endif  // HWY_TARGET >= HWY_SSE4

HWY_DIAGNOSTICS(pop)

// ================================================== SWIZZLE (2)

// ------------------------------ LowerHalf

template <class D, HWY_IF_V_SIZE_LE_D(D, 8)>
HWY_API VFromD<D> LowerHalf(D /* tag */, VFromD<Twice<D>> v) {
  return VFromD<D>{v.raw};
}
template <typename T, size_t N>
HWY_API Vec128<T, N / 2> LowerHalf(Vec128<T, N> v) {
  return Vec128<T, N / 2>{v.raw};
}

// ------------------------------ ShiftLeftBytes

template <int kBytes, class D, HWY_IF_V_SIZE_LE_D(D, 16)>
HWY_API VFromD<D> ShiftLeftBytes(D d, VFromD<D> v) {
  static_assert(0 <= kBytes && kBytes <= 16, "Invalid kBytes");
  const RebindToUnsigned<decltype(d)> du;
  return BitCast(
      d, VFromD<decltype(du)>{_mm_slli_si128(BitCast(du, v).raw, kBytes)});
}

template <int kBytes, typename T, size_t N>
HWY_API Vec128<T, N> ShiftLeftBytes(const Vec128<T, N> v) {
  return ShiftLeftBytes<kBytes>(DFromV<decltype(v)>(), v);
}

// ------------------------------ ShiftLeftLanes

template <int kLanes, class D, typename T = TFromD<D>,
          HWY_IF_V_SIZE_LE_D(D, 16)>
HWY_API VFromD<D> ShiftLeftLanes(D d, const VFromD<D> v) {
  const Repartition<uint8_t, decltype(d)> d8;
  return BitCast(d, ShiftLeftBytes<kLanes * sizeof(T)>(BitCast(d8, v)));
}

template <int kLanes, typename T, size_t N>
HWY_API Vec128<T, N> ShiftLeftLanes(const Vec128<T, N> v) {
  return ShiftLeftLanes<kLanes>(DFromV<decltype(v)>(), v);
}

// ------------------------------ ShiftRightBytes
template <int kBytes, class D, HWY_IF_V_SIZE_LE_D(D, 16)>
HWY_API VFromD<D> ShiftRightBytes(D d, VFromD<D> v) {
  static_assert(0 <= kBytes && kBytes <= 16, "Invalid kBytes");
  const RebindToUnsigned<decltype(d)> du;
  // For partial vectors, clear upper lanes so we shift in zeros.
  if (d.MaxBytes() != 16) {
    const Full128<TFromD<D>> dfull;
    const VFromD<decltype(dfull)> vfull{v.raw};
    v = VFromD<D>{IfThenElseZero(FirstN(dfull, MaxLanes(d)), vfull).raw};
  }
  return BitCast(
      d, VFromD<decltype(du)>{_mm_srli_si128(BitCast(du, v).raw, kBytes)});
}

// ------------------------------ ShiftRightLanes
template <int kLanes, class D, HWY_IF_V_SIZE_LE_D(D, 16)>
HWY_API VFromD<D> ShiftRightLanes(D d, const VFromD<D> v) {
  const Repartition<uint8_t, decltype(d)> d8;
  constexpr size_t kBytes = kLanes * sizeof(TFromD<D>);
  return BitCast(d, ShiftRightBytes<kBytes>(d8, BitCast(d8, v)));
}

// ------------------------------ UpperHalf (ShiftRightBytes)

// Full input: copy hi into lo (smaller instruction encoding than shifts).
template <class D, typename T = TFromD<D>>
HWY_API Vec64<T> UpperHalf(D /* tag */, Vec128<T> v) {
  return Vec64<T>{_mm_unpackhi_epi64(v.raw, v.raw)};
}
template <class D>
HWY_API Vec64<float> UpperHalf(D /* tag */, Vec128<float> v) {
  return Vec64<float>{_mm_movehl_ps(v.raw, v.raw)};
}
template <class D>
HWY_API Vec64<double> UpperHalf(D /* tag */, Vec128<double> v) {
  return Vec64<double>{_mm_unpackhi_pd(v.raw, v.raw)};
}

// Partial
template <class D, HWY_IF_V_SIZE_LE_D(D, 4)>
HWY_API VFromD<D> UpperHalf(D d, VFromD<Twice<D>> v) {
  return LowerHalf(d, ShiftRightBytes<d.MaxBytes()>(Twice<D>(), v));
}

// ------------------------------ ExtractLane (UpperHalf)

namespace detail {

template <size_t kLane, typename T, size_t N, HWY_IF_T_SIZE(T, 1)>
HWY_INLINE T ExtractLane(const Vec128<T, N> v) {
  static_assert(kLane < N, "Lane index out of bounds");
#if HWY_TARGET >= HWY_SSSE3
  const int pair = _mm_extract_epi16(v.raw, kLane / 2);
  constexpr int kShift = kLane & 1 ? 8 : 0;
  return static_cast<T>((pair >> kShift) & 0xFF);
#else
  return static_cast<T>(_mm_extract_epi8(v.raw, kLane) & 0xFF);
#endif
}

template <size_t kLane, typename T, size_t N, HWY_IF_T_SIZE(T, 2)>
HWY_INLINE T ExtractLane(const Vec128<T, N> v) {
  static_assert(kLane < N, "Lane index out of bounds");
  return static_cast<T>(_mm_extract_epi16(v.raw, kLane) & 0xFFFF);
}

template <size_t kLane, typename T, size_t N, HWY_IF_T_SIZE(T, 4)>
HWY_INLINE T ExtractLane(const Vec128<T, N> v) {
  static_assert(kLane < N, "Lane index out of bounds");
#if HWY_TARGET >= HWY_SSSE3
  alignas(16) T lanes[4];
  Store(v, DFromV<decltype(v)>(), lanes);
  return lanes[kLane];
#else
  return static_cast<T>(_mm_extract_epi32(v.raw, kLane));
#endif
}

template <size_t kLane, typename T, size_t N, HWY_IF_T_SIZE(T, 8)>
HWY_INLINE T ExtractLane(const Vec128<T, N> v) {
  static_assert(kLane < N, "Lane index out of bounds");
#if HWY_ARCH_X86_32
  alignas(16) T lanes[2];
  Store(v, DFromV<decltype(v)>(), lanes);
  return lanes[kLane];
#elif HWY_TARGET >= HWY_SSSE3
  return static_cast<T>(
      _mm_cvtsi128_si64((kLane == 0) ? v.raw : _mm_shuffle_epi32(v.raw, 0xEE)));
#else
  return static_cast<T>(_mm_extract_epi64(v.raw, kLane));
#endif
}

template <size_t kLane, size_t N>
HWY_INLINE float ExtractLane(const Vec128<float, N> v) {
  static_assert(kLane < N, "Lane index out of bounds");
#if HWY_TARGET >= HWY_SSSE3
  alignas(16) float lanes[4];
  Store(v, DFromV<decltype(v)>(), lanes);
  return lanes[kLane];
#else
  // Bug in the intrinsic, returns int but should be float.
  const int32_t bits = _mm_extract_ps(v.raw, kLane);
  float ret;
  CopySameSize(&bits, &ret);
  return ret;
#endif
}

// There is no extract_pd; two overloads because there is no UpperHalf for N=1.
template <size_t kLane>
HWY_INLINE double ExtractLane(const Vec128<double, 1> v) {
  static_assert(kLane == 0, "Lane index out of bounds");
  return GetLane(v);
}

template <size_t kLane>
HWY_INLINE double ExtractLane(const Vec128<double> v) {
  static_assert(kLane < 2, "Lane index out of bounds");
  const Half<DFromV<decltype(v)>> dh;
  return kLane == 0 ? GetLane(v) : GetLane(UpperHalf(dh, v));
}

}  // namespace detail

// Requires one overload per vector length because ExtractLane<3> may be a
// compile error if it calls _mm_extract_epi64.
template <typename T>
HWY_API T ExtractLane(const Vec128<T, 1> v, size_t i) {
  HWY_DASSERT(i == 0);
  (void)i;
  return GetLane(v);
}

template <typename T>
HWY_API T ExtractLane(const Vec128<T, 2> v, size_t i) {
#if !HWY_IS_DEBUG_BUILD && HWY_COMPILER_GCC  // includes clang
  if (__builtin_constant_p(i)) {
    switch (i) {
      case 0:
        return detail::ExtractLane<0>(v);
      case 1:
        return detail::ExtractLane<1>(v);
    }
  }
#endif
  alignas(16) T lanes[2];
  Store(v, DFromV<decltype(v)>(), lanes);
  return lanes[i];
}

template <typename T>
HWY_API T ExtractLane(const Vec128<T, 4> v, size_t i) {
#if !HWY_IS_DEBUG_BUILD && HWY_COMPILER_GCC  // includes clang
  if (__builtin_constant_p(i)) {
    switch (i) {
      case 0:
        return detail::ExtractLane<0>(v);
      case 1:
        return detail::ExtractLane<1>(v);
      case 2:
        return detail::ExtractLane<2>(v);
      case 3:
        return detail::ExtractLane<3>(v);
    }
  }
#endif
  alignas(16) T lanes[4];
  Store(v, DFromV<decltype(v)>(), lanes);
  return lanes[i];
}

template <typename T>
HWY_API T ExtractLane(const Vec128<T, 8> v, size_t i) {
#if !HWY_IS_DEBUG_BUILD && HWY_COMPILER_GCC  // includes clang
  if (__builtin_constant_p(i)) {
    switch (i) {
      case 0:
        return detail::ExtractLane<0>(v);
      case 1:
        return detail::ExtractLane<1>(v);
      case 2:
        return detail::ExtractLane<2>(v);
      case 3:
        return detail::ExtractLane<3>(v);
      case 4:
        return detail::ExtractLane<4>(v);
      case 5:
        return detail::ExtractLane<5>(v);
      case 6:
        return detail::ExtractLane<6>(v);
      case 7:
        return detail::ExtractLane<7>(v);
    }
  }
#endif
  alignas(16) T lanes[8];
  Store(v, DFromV<decltype(v)>(), lanes);
  return lanes[i];
}

template <typename T>
HWY_API T ExtractLane(const Vec128<T, 16> v, size_t i) {
#if !HWY_IS_DEBUG_BUILD && HWY_COMPILER_GCC  // includes clang
  if (__builtin_constant_p(i)) {
    switch (i) {
      case 0:
        return detail::ExtractLane<0>(v);
      case 1:
        return detail::ExtractLane<1>(v);
      case 2:
        return detail::ExtractLane<2>(v);
      case 3:
        return detail::ExtractLane<3>(v);
      case 4:
        return detail::ExtractLane<4>(v);
      case 5:
        return detail::ExtractLane<5>(v);
      case 6:
        return detail::ExtractLane<6>(v);
      case 7:
        return detail::ExtractLane<7>(v);
      case 8:
        return detail::ExtractLane<8>(v);
      case 9:
        return detail::ExtractLane<9>(v);
      case 10:
        return detail::ExtractLane<10>(v);
      case 11:
        return detail::ExtractLane<11>(v);
      case 12:
        return detail::ExtractLane<12>(v);
      case 13:
        return detail::ExtractLane<13>(v);
      case 14:
        return detail::ExtractLane<14>(v);
      case 15:
        return detail::ExtractLane<15>(v);
    }
  }
#endif
  alignas(16) T lanes[16];
  Store(v, DFromV<decltype(v)>(), lanes);
  return lanes[i];
}

// ------------------------------ InsertLane (UpperHalf)

namespace detail {

template <class V>
HWY_INLINE V InsertLaneUsingBroadcastAndBlend(V v, size_t i, TFromV<V> t) {
  const DFromV<decltype(v)> d;

#if HWY_TARGET <= HWY_AVX3
  using RawMask = decltype(MaskFromVec(VFromD<decltype(d)>()).raw);
  const auto mask = MFromD<decltype(d)>{static_cast<RawMask>(uint64_t{1} << i)};
#else
  const RebindToUnsigned<decltype(d)> du;
  using TU = TFromD<decltype(du)>;
  const auto mask = RebindMask(d, Iota(du, 0) == Set(du, static_cast<TU>(i)));
#endif

  return IfThenElse(mask, Set(d, t), v);
}

template <size_t kLane, typename T, size_t N, HWY_IF_T_SIZE(T, 1)>
HWY_INLINE Vec128<T, N> InsertLane(const Vec128<T, N> v, T t) {
  static_assert(kLane < N, "Lane index out of bounds");
#if HWY_TARGET >= HWY_SSSE3
  return InsertLaneUsingBroadcastAndBlend(v, kLane, t);
#else
  return Vec128<T, N>{_mm_insert_epi8(v.raw, t, kLane)};
#endif
}

template <size_t kLane, typename T, size_t N, HWY_IF_T_SIZE(T, 2)>
HWY_INLINE Vec128<T, N> InsertLane(const Vec128<T, N> v, T t) {
  static_assert(kLane < N, "Lane index out of bounds");
  return Vec128<T, N>{_mm_insert_epi16(v.raw, t, kLane)};
}

template <size_t kLane, typename T, size_t N, HWY_IF_T_SIZE(T, 4)>
HWY_INLINE Vec128<T, N> InsertLane(const Vec128<T, N> v, T t) {
  static_assert(kLane < N, "Lane index out of bounds");
#if HWY_TARGET >= HWY_SSSE3
  return InsertLaneUsingBroadcastAndBlend(v, kLane, t);
#else
  MakeSigned<T> ti;
  CopySameSize(&t, &ti);  // don't just cast because T might be float.
  return Vec128<T, N>{_mm_insert_epi32(v.raw, ti, kLane)};
#endif
}

template <size_t kLane, typename T, size_t N, HWY_IF_T_SIZE(T, 8)>
HWY_INLINE Vec128<T, N> InsertLane(const Vec128<T, N> v, T t) {
  static_assert(kLane < N, "Lane index out of bounds");
#if HWY_TARGET >= HWY_SSSE3 || HWY_ARCH_X86_32
  const DFromV<decltype(v)> d;
  const RebindToFloat<decltype(d)> df;
  const auto vt = BitCast(df, Set(d, t));
  if (kLane == 0) {
    return BitCast(
        d, Vec128<double, N>{_mm_shuffle_pd(vt.raw, BitCast(df, v).raw, 2)});
  }
  return BitCast(
      d, Vec128<double, N>{_mm_shuffle_pd(BitCast(df, v).raw, vt.raw, 0)});
#else
  MakeSigned<T> ti;
  CopySameSize(&t, &ti);  // don't just cast because T might be float.
  return Vec128<T, N>{_mm_insert_epi64(v.raw, ti, kLane)};
#endif
}

template <size_t kLane, size_t N>
HWY_INLINE Vec128<float, N> InsertLane(const Vec128<float, N> v, float t) {
  static_assert(kLane < N, "Lane index out of bounds");
#if HWY_TARGET >= HWY_SSSE3
  return InsertLaneUsingBroadcastAndBlend(v, kLane, t);
#else
  return Vec128<float, N>{_mm_insert_ps(v.raw, _mm_set_ss(t), kLane << 4)};
#endif
}

// There is no insert_pd; two overloads because there is no UpperHalf for N=1.
template <size_t kLane>
HWY_INLINE Vec128<double, 1> InsertLane(const Vec128<double, 1> v, double t) {
  static_assert(kLane == 0, "Lane index out of bounds");
  return Set(DFromV<decltype(v)>(), t);
}

template <size_t kLane>
HWY_INLINE Vec128<double> InsertLane(const Vec128<double> v, double t) {
  static_assert(kLane < 2, "Lane index out of bounds");
  const DFromV<decltype(v)> d;
  const Vec128<double> vt = Set(d, t);
  if (kLane == 0) {
    return Vec128<double>{_mm_shuffle_pd(vt.raw, v.raw, 2)};
  }
  return Vec128<double>{_mm_shuffle_pd(v.raw, vt.raw, 0)};
}

}  // namespace detail

// Requires one overload per vector length because InsertLane<3> may be a
// compile error if it calls _mm_insert_epi64.

template <typename T>
HWY_API Vec128<T, 1> InsertLane(const Vec128<T, 1> v, size_t i, T t) {
  HWY_DASSERT(i == 0);
  (void)i;
  return Set(DFromV<decltype(v)>(), t);
}

template <typename T>
HWY_API Vec128<T, 2> InsertLane(const Vec128<T, 2> v, size_t i, T t) {
#if !HWY_IS_DEBUG_BUILD && HWY_COMPILER_GCC  // includes clang
  if (__builtin_constant_p(i)) {
    switch (i) {
      case 0:
        return detail::InsertLane<0>(v, t);
      case 1:
        return detail::InsertLane<1>(v, t);
    }
  }
#endif
  return detail::InsertLaneUsingBroadcastAndBlend(v, i, t);
}

template <typename T>
HWY_API Vec128<T, 4> InsertLane(const Vec128<T, 4> v, size_t i, T t) {
#if !HWY_IS_DEBUG_BUILD && HWY_COMPILER_GCC  // includes clang
  if (__builtin_constant_p(i)) {
    switch (i) {
      case 0:
        return detail::InsertLane<0>(v, t);
      case 1:
        return detail::InsertLane<1>(v, t);
      case 2:
        return detail::InsertLane<2>(v, t);
      case 3:
        return detail::InsertLane<3>(v, t);
    }
  }
#endif
  return detail::InsertLaneUsingBroadcastAndBlend(v, i, t);
}

template <typename T>
HWY_API Vec128<T, 8> InsertLane(const Vec128<T, 8> v, size_t i, T t) {
#if !HWY_IS_DEBUG_BUILD && HWY_COMPILER_GCC  // includes clang
  if (__builtin_constant_p(i)) {
    switch (i) {
      case 0:
        return detail::InsertLane<0>(v, t);
      case 1:
        return detail::InsertLane<1>(v, t);
      case 2:
        return detail::InsertLane<2>(v, t);
      case 3:
        return detail::InsertLane<3>(v, t);
      case 4:
        return detail::InsertLane<4>(v, t);
      case 5:
        return detail::InsertLane<5>(v, t);
      case 6:
        return detail::InsertLane<6>(v, t);
      case 7:
        return detail::InsertLane<7>(v, t);
    }
  }
#endif
  return detail::InsertLaneUsingBroadcastAndBlend(v, i, t);
}

template <typename T>
HWY_API Vec128<T, 16> InsertLane(const Vec128<T, 16> v, size_t i, T t) {
#if !HWY_IS_DEBUG_BUILD && HWY_COMPILER_GCC  // includes clang
  if (__builtin_constant_p(i)) {
    switch (i) {
      case 0:
        return detail::InsertLane<0>(v, t);
      case 1:
        return detail::InsertLane<1>(v, t);
      case 2:
        return detail::InsertLane<2>(v, t);
      case 3:
        return detail::InsertLane<3>(v, t);
      case 4:
        return detail::InsertLane<4>(v, t);
      case 5:
        return detail::InsertLane<5>(v, t);
      case 6:
        return detail::InsertLane<6>(v, t);
      case 7:
        return detail::InsertLane<7>(v, t);
      case 8:
        return detail::InsertLane<8>(v, t);
      case 9:
        return detail::InsertLane<9>(v, t);
      case 10:
        return detail::InsertLane<10>(v, t);
      case 11:
        return detail::InsertLane<11>(v, t);
      case 12:
        return detail::InsertLane<12>(v, t);
      case 13:
        return detail::InsertLane<13>(v, t);
      case 14:
        return detail::InsertLane<14>(v, t);
      case 15:
        return detail::InsertLane<15>(v, t);
    }
  }
#endif
  return detail::InsertLaneUsingBroadcastAndBlend(v, i, t);
}

// ------------------------------ CombineShiftRightBytes

#if HWY_TARGET == HWY_SSE2
template <int kBytes, class D, typename T = TFromD<D>>
HWY_API Vec128<T> CombineShiftRightBytes(D d, Vec128<T> hi, Vec128<T> lo) {
  static_assert(0 < kBytes && kBytes < 16, "kBytes invalid");
  return Or(ShiftRightBytes<kBytes>(d, lo), ShiftLeftBytes<16 - kBytes>(d, hi));
}
template <int kBytes, class D, HWY_IF_V_SIZE_LE_D(D, 8)>
HWY_API VFromD<D> CombineShiftRightBytes(D d, VFromD<D> hi, VFromD<D> lo) {
  constexpr size_t kSize = d.MaxBytes();
  static_assert(0 < kBytes && kBytes < kSize, "kBytes invalid");

  const Twice<decltype(d)> dt;
  return VFromD<D>{ShiftRightBytes<kBytes>(dt, Combine(dt, hi, lo)).raw};
}
#else
template <int kBytes, class D, typename T = TFromD<D>>
HWY_API Vec128<T> CombineShiftRightBytes(D d, Vec128<T> hi, Vec128<T> lo) {
  const Repartition<uint8_t, decltype(d)> d8;
  return BitCast(d, Vec128<uint8_t>{_mm_alignr_epi8(
                        BitCast(d8, hi).raw, BitCast(d8, lo).raw, kBytes)});
}

template <int kBytes, class D, HWY_IF_V_SIZE_LE_D(D, 8)>
HWY_API VFromD<D> CombineShiftRightBytes(D d, VFromD<D> hi, VFromD<D> lo) {
  constexpr size_t kSize = d.MaxBytes();
  static_assert(0 < kBytes && kBytes < kSize, "kBytes invalid");
  const Repartition<uint8_t, decltype(d)> d8;
  using V8 = Vec128<uint8_t>;
  const DFromV<V8> dfull8;
  const Repartition<TFromD<D>, decltype(dfull8)> dfull;
  const V8 hi8{BitCast(d8, hi).raw};
  // Move into most-significant bytes
  const V8 lo8 = ShiftLeftBytes<16 - kSize>(V8{BitCast(d8, lo).raw});
  const V8 r = CombineShiftRightBytes<16 - kSize + kBytes>(dfull8, hi8, lo8);
  return VFromD<D>{BitCast(dfull, r).raw};
}
#endif

// ------------------------------ Broadcast/splat any lane

// Unsigned
template <int kLane, size_t N>
HWY_API Vec128<uint16_t, N> Broadcast(const Vec128<uint16_t, N> v) {
  static_assert(0 <= kLane && kLane < N, "Invalid lane");
  if (kLane < 4) {
    const __m128i lo = _mm_shufflelo_epi16(v.raw, (0x55 * kLane) & 0xFF);
    return Vec128<uint16_t, N>{_mm_unpacklo_epi64(lo, lo)};
  } else {
    const __m128i hi = _mm_shufflehi_epi16(v.raw, (0x55 * (kLane - 4)) & 0xFF);
    return Vec128<uint16_t, N>{_mm_unpackhi_epi64(hi, hi)};
  }
}
template <int kLane, size_t N>
HWY_API Vec128<uint32_t, N> Broadcast(const Vec128<uint32_t, N> v) {
  static_assert(0 <= kLane && kLane < N, "Invalid lane");
  return Vec128<uint32_t, N>{_mm_shuffle_epi32(v.raw, 0x55 * kLane)};
}
template <int kLane, size_t N>
HWY_API Vec128<uint64_t, N> Broadcast(const Vec128<uint64_t, N> v) {
  static_assert(0 <= kLane && kLane < N, "Invalid lane");
  return Vec128<uint64_t, N>{_mm_shuffle_epi32(v.raw, kLane ? 0xEE : 0x44)};
}

// Signed
template <int kLane, size_t N>
HWY_API Vec128<int16_t, N> Broadcast(const Vec128<int16_t, N> v) {
  static_assert(0 <= kLane && kLane < N, "Invalid lane");
  if (kLane < 4) {
    const __m128i lo = _mm_shufflelo_epi16(v.raw, (0x55 * kLane) & 0xFF);
    return Vec128<int16_t, N>{_mm_unpacklo_epi64(lo, lo)};
  } else {
    const __m128i hi = _mm_shufflehi_epi16(v.raw, (0x55 * (kLane - 4)) & 0xFF);
    return Vec128<int16_t, N>{_mm_unpackhi_epi64(hi, hi)};
  }
}
template <int kLane, size_t N>
HWY_API Vec128<int32_t, N> Broadcast(const Vec128<int32_t, N> v) {
  static_assert(0 <= kLane && kLane < N, "Invalid lane");
  return Vec128<int32_t, N>{_mm_shuffle_epi32(v.raw, 0x55 * kLane)};
}
template <int kLane, size_t N>
HWY_API Vec128<int64_t, N> Broadcast(const Vec128<int64_t, N> v) {
  static_assert(0 <= kLane && kLane < N, "Invalid lane");
  return Vec128<int64_t, N>{_mm_shuffle_epi32(v.raw, kLane ? 0xEE : 0x44)};
}

// Float
template <int kLane, size_t N>
HWY_API Vec128<float, N> Broadcast(const Vec128<float, N> v) {
  static_assert(0 <= kLane && kLane < N, "Invalid lane");
  return Vec128<float, N>{_mm_shuffle_ps(v.raw, v.raw, 0x55 * kLane)};
}
template <int kLane, size_t N>
HWY_API Vec128<double, N> Broadcast(const Vec128<double, N> v) {
  static_assert(0 <= kLane && kLane < N, "Invalid lane");
  return Vec128<double, N>{_mm_shuffle_pd(v.raw, v.raw, 3 * kLane)};
}

// ------------------------------ TableLookupLanes (Shuffle01)

// Returned by SetTableIndices/IndicesFromVec for use by TableLookupLanes.
template <typename T, size_t N = 16 / sizeof(T)>
struct Indices128 {
  __m128i raw;
};

template <class D, typename T = TFromD<D>, typename TI, size_t kN,
          HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_T_SIZE(T, 1)>
HWY_API Indices128<T, kN> IndicesFromVec(D d, Vec128<TI, kN> vec) {
  static_assert(sizeof(T) == sizeof(TI), "Index size must match lane");
#if HWY_IS_DEBUG_BUILD
  const Rebind<TI, decltype(d)> di;
  HWY_DASSERT(AllFalse(di, Lt(vec, Zero(di))) &&
              AllTrue(di, Lt(vec, Set(di, kN * 2))));
#endif

  // No change as byte indices are always used for 8-bit lane types
  (void)d;
  return Indices128<T, kN>{vec.raw};
}

template <class D, typename T = TFromD<D>, typename TI, size_t kN,
          HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_T_SIZE(T, 2)>
HWY_API Indices128<T, kN> IndicesFromVec(D d, Vec128<TI, kN> vec) {
  static_assert(sizeof(T) == sizeof(TI), "Index size must match lane");
#if HWY_IS_DEBUG_BUILD
  const Rebind<TI, decltype(d)> di;
  HWY_DASSERT(AllFalse(di, Lt(vec, Zero(di))) &&
              AllTrue(di, Lt(vec, Set(di, kN * 2))));
#endif

#if HWY_TARGET <= HWY_AVX3 || HWY_TARGET == HWY_SSE2
  (void)d;
  return Indices128<T, kN>{vec.raw};
#else   // SSSE3, SSE4, or AVX2
  const Repartition<uint8_t, decltype(d)> d8;
  using V8 = VFromD<decltype(d8)>;
  alignas(16) static constexpr uint8_t kByteOffsets[16] = {
      0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1};

  // Broadcast each lane index to all 4 bytes of T
  alignas(16) static constexpr uint8_t kBroadcastLaneBytes[16] = {
      0, 0, 2, 2, 4, 4, 6, 6, 8, 8, 10, 10, 12, 12, 14, 14};
  const V8 lane_indices = TableLookupBytes(vec, Load(d8, kBroadcastLaneBytes));

  // Shift to bytes
  const Repartition<uint16_t, decltype(d)> d16;
  const V8 byte_indices = BitCast(d8, ShiftLeft<1>(BitCast(d16, lane_indices)));

  return Indices128<T, kN>{Add(byte_indices, Load(d8, kByteOffsets)).raw};
#endif  // HWY_TARGET <= HWY_AVX3 || HWY_TARGET == HWY_SSE2
}

template <class D, typename T = TFromD<D>, typename TI, size_t kN,
          HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_T_SIZE(T, 4)>
HWY_API Indices128<T, kN> IndicesFromVec(D d, Vec128<TI, kN> vec) {
  static_assert(sizeof(T) == sizeof(TI), "Index size must match lane");
#if HWY_IS_DEBUG_BUILD
  const Rebind<TI, decltype(d)> di;
  HWY_DASSERT(AllFalse(di, Lt(vec, Zero(di))) &&
              AllTrue(di, Lt(vec, Set(di, kN * 2))));
#endif

#if HWY_TARGET <= HWY_AVX2 || HWY_TARGET == HWY_SSE2
  (void)d;
  return Indices128<T, kN>{vec.raw};
#else
  const Repartition<uint8_t, decltype(d)> d8;
  using V8 = VFromD<decltype(d8)>;
  alignas(16) static constexpr uint8_t kByteOffsets[16] = {
      0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3};

  // Broadcast each lane index to all 4 bytes of T
  alignas(16) static constexpr uint8_t kBroadcastLaneBytes[16] = {
      0, 0, 0, 0, 4, 4, 4, 4, 8, 8, 8, 8, 12, 12, 12, 12};
  const V8 lane_indices = TableLookupBytes(vec, Load(d8, kBroadcastLaneBytes));

  // Shift to bytes
  const Repartition<uint16_t, decltype(d)> d16;
  const V8 byte_indices = BitCast(d8, ShiftLeft<2>(BitCast(d16, lane_indices)));

  return Indices128<T, kN>{Add(byte_indices, Load(d8, kByteOffsets)).raw};
#endif
}

template <class D, typename T = TFromD<D>, typename TI, size_t kN,
          HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_T_SIZE(T, 8)>
HWY_API Indices128<T, kN> IndicesFromVec(D d, Vec128<TI, kN> vec) {
  static_assert(sizeof(T) == sizeof(TI), "Index size must match lane");
#if HWY_IS_DEBUG_BUILD
  const Rebind<TI, decltype(d)> di;
  HWY_DASSERT(AllFalse(di, Lt(vec, Zero(di))) &&
              AllTrue(di, Lt(vec, Set(di, static_cast<TI>(kN * 2)))));
#else
  (void)d;
#endif

  // No change - even without AVX3, we can shuffle+blend.
  return Indices128<T, kN>{vec.raw};
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 16), typename TI>
HWY_API Indices128<TFromD<D>, HWY_MAX_LANES_D(D)> SetTableIndices(
    D d, const TI* idx) {
  static_assert(sizeof(TFromD<D>) == sizeof(TI), "Index size must match lane");
  const Rebind<TI, decltype(d)> di;
  return IndicesFromVec(d, LoadU(di, idx));
}

template <typename T, size_t N, HWY_IF_T_SIZE(T, 1)>
HWY_API Vec128<T, N> TableLookupLanes(Vec128<T, N> v, Indices128<T, N> idx) {
  return TableLookupBytes(v, Vec128<T, N>{idx.raw});
}

template <typename T, size_t N, HWY_IF_T_SIZE(T, 2)>
HWY_API Vec128<T, N> TableLookupLanes(Vec128<T, N> v, Indices128<T, N> idx) {
#if HWY_TARGET <= HWY_AVX3
  return {_mm_permutexvar_epi16(idx.raw, v.raw)};
#elif HWY_TARGET == HWY_SSE2
#if HWY_COMPILER_GCC_ACTUAL && HWY_HAS_BUILTIN(__builtin_shuffle)
  typedef uint16_t GccU16RawVectType __attribute__((__vector_size__(16)));
  return Vec128<T, N>{reinterpret_cast<typename detail::Raw128<T>::type>(
      __builtin_shuffle(reinterpret_cast<GccU16RawVectType>(v.raw),
                        reinterpret_cast<GccU16RawVectType>(idx.raw)))};
#else
  const Full128<T> d_full;
  alignas(16) T src_lanes[8];
  alignas(16) uint16_t indices[8];
  alignas(16) T result_lanes[8];

  Store(Vec128<T>{v.raw}, d_full, src_lanes);
  _mm_store_si128(reinterpret_cast<__m128i*>(indices), idx.raw);

  for (int i = 0; i < 8; i++) {
    result_lanes[i] = src_lanes[indices[i] & 7u];
  }

  return Vec128<T, N>{Load(d_full, result_lanes).raw};
#endif  // HWY_COMPILER_GCC_ACTUAL && HWY_HAS_BUILTIN(__builtin_shuffle)
#else
  return TableLookupBytes(v, Vec128<T, N>{idx.raw});
#endif
}

template <typename T, size_t N, HWY_IF_T_SIZE(T, 4)>
HWY_API Vec128<T, N> TableLookupLanes(Vec128<T, N> v, Indices128<T, N> idx) {
#if HWY_TARGET <= HWY_AVX2
  const DFromV<decltype(v)> d;
  const RebindToFloat<decltype(d)> df;
  const Vec128<float, N> perm{_mm_permutevar_ps(BitCast(df, v).raw, idx.raw)};
  return BitCast(d, perm);
#elif HWY_TARGET == HWY_SSE2
#if HWY_COMPILER_GCC_ACTUAL && HWY_HAS_BUILTIN(__builtin_shuffle)
  typedef uint32_t GccU32RawVectType __attribute__((__vector_size__(16)));
  return Vec128<T, N>{reinterpret_cast<typename detail::Raw128<T>::type>(
      __builtin_shuffle(reinterpret_cast<GccU32RawVectType>(v.raw),
                        reinterpret_cast<GccU32RawVectType>(idx.raw)))};
#else
  const Full128<T> d_full;
  alignas(16) T src_lanes[4];
  alignas(16) uint32_t indices[4];
  alignas(16) T result_lanes[4];

  Store(Vec128<T>{v.raw}, d_full, src_lanes);
  _mm_store_si128(reinterpret_cast<__m128i*>(indices), idx.raw);

  for (int i = 0; i < 4; i++) {
    result_lanes[i] = src_lanes[indices[i] & 3u];
  }

  return Vec128<T, N>{Load(d_full, result_lanes).raw};
#endif  // HWY_COMPILER_GCC_ACTUAL && HWY_HAS_BUILTIN(__builtin_shuffle)
#else   // SSSE3 or SSE4
  return TableLookupBytes(v, Vec128<T, N>{idx.raw});
#endif
}

#if HWY_TARGET <= HWY_SSSE3
template <size_t N, HWY_IF_V_SIZE_GT(float, N, 4)>
HWY_API Vec128<float, N> TableLookupLanes(Vec128<float, N> v,
                                          Indices128<float, N> idx) {
#if HWY_TARGET <= HWY_AVX2
  return Vec128<float, N>{_mm_permutevar_ps(v.raw, idx.raw)};
#else   // SSSE3 or SSE4
  const DFromV<decltype(v)> df;
  const RebindToSigned<decltype(df)> di;
  return BitCast(df,
                 TableLookupBytes(BitCast(di, v), Vec128<int32_t, N>{idx.raw}));
#endif  // HWY_TARGET <= HWY_AVX2
}
#endif  // HWY_TARGET <= HWY_SSSE3

// Single lane: no change
template <typename T>
HWY_API Vec128<T, 1> TableLookupLanes(Vec128<T, 1> v,
                                      Indices128<T, 1> /* idx */) {
  return v;
}

template <typename T, HWY_IF_T_SIZE(T, 8)>
HWY_API Vec128<T> TableLookupLanes(Vec128<T> v, Indices128<T> idx) {
  const DFromV<decltype(v)> d;
  Vec128<int64_t> vidx{idx.raw};
#if HWY_TARGET <= HWY_AVX2
  // There is no _mm_permute[x]var_epi64.
  vidx += vidx;  // bit1 is the decider (unusual)
  const RebindToFloat<decltype(d)> df;
  return BitCast(
      d, Vec128<double>{_mm_permutevar_pd(BitCast(df, v).raw, vidx.raw)});
#else
  // Only 2 lanes: can swap+blend. Choose v if vidx == iota. To avoid a 64-bit
  // comparison (expensive on SSSE3), just invert the upper lane and subtract 1
  // to obtain an all-zero or all-one mask.
  const RebindToSigned<decltype(d)> di;
  const Vec128<int64_t> same = (vidx ^ Iota(di, 0)) - Set(di, 1);
  const Mask128<T> mask_same = RebindMask(d, MaskFromVec(same));
  return IfThenElse(mask_same, v, Shuffle01(v));
#endif
}

HWY_API Vec128<double> TableLookupLanes(Vec128<double> v,
                                        Indices128<double> idx) {
  Vec128<int64_t> vidx{idx.raw};
#if HWY_TARGET <= HWY_AVX2
  vidx += vidx;  // bit1 is the decider (unusual)
  return Vec128<double>{_mm_permutevar_pd(v.raw, vidx.raw)};
#else
  // Only 2 lanes: can swap+blend. Choose v if vidx == iota. To avoid a 64-bit
  // comparison (expensive on SSSE3), just invert the upper lane and subtract 1
  // to obtain an all-zero or all-one mask.
  const DFromV<decltype(v)> d;
  const RebindToSigned<decltype(d)> di;
  const Vec128<int64_t> same = (vidx ^ Iota(di, 0)) - Set(di, 1);
  const Mask128<double> mask_same = RebindMask(d, MaskFromVec(same));
  return IfThenElse(mask_same, v, Shuffle01(v));
#endif
}

// ------------------------------ ReverseBlocks

// Single block: no change
template <class D, HWY_IF_V_SIZE_LE_D(D, 16)>
HWY_API VFromD<D> ReverseBlocks(D /* tag */, VFromD<D> v) {
  return v;
}

// ------------------------------ Reverse (Shuffle0123, Shuffle2301)

// Single lane: no change
template <class D, typename T = TFromD<D>, HWY_IF_LANES_D(D, 1)>
HWY_API Vec128<T, 1> Reverse(D /* tag */, Vec128<T, 1> v) {
  return v;
}

// 32-bit x2: shuffle
template <class D, typename T = TFromD<D>, HWY_IF_T_SIZE(T, 4)>
HWY_API Vec64<T> Reverse(D /* tag */, const Vec64<T> v) {
  return Vec64<T>{Shuffle2301(Vec128<T>{v.raw}).raw};
}

// 64-bit x2: shuffle
template <class D, typename T = TFromD<D>, HWY_IF_T_SIZE(T, 8)>
HWY_API Vec128<T> Reverse(D /* tag */, const Vec128<T> v) {
  return Shuffle01(v);
}

// 32-bit x4: shuffle
template <class D, typename T = TFromD<D>, HWY_IF_T_SIZE(T, 4)>
HWY_API Vec128<T> Reverse(D /* tag */, const Vec128<T> v) {
  return Shuffle0123(v);
}

// 16-bit
template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_T_SIZE_D(D, 2)>
HWY_API VFromD<D> Reverse(D d, const VFromD<D> v) {
  constexpr size_t kN = MaxLanes(d);
  if (kN == 1) return v;
  if (kN == 2) {
    return VFromD<D>{_mm_shufflelo_epi16(v.raw, _MM_SHUFFLE(0, 1, 0, 1))};
  }
  if (kN == 4) {
    return VFromD<D>{_mm_shufflelo_epi16(v.raw, _MM_SHUFFLE(0, 1, 2, 3))};
  }

#if HWY_TARGET == HWY_SSE2
  const VFromD<D> rev4{
      _mm_shufflehi_epi16(_mm_shufflelo_epi16(v.raw, _MM_SHUFFLE(0, 1, 2, 3)),
                          _MM_SHUFFLE(0, 1, 2, 3))};
  return VFromD<D>{_mm_shuffle_epi32(rev4.raw, _MM_SHUFFLE(1, 0, 3, 2))};
#else
  const RebindToSigned<decltype(d)> di;
  alignas(16) static constexpr int16_t kShuffle[8] = {
      0x0F0E, 0x0D0C, 0x0B0A, 0x0908, 0x0706, 0x0504, 0x0302, 0x0100};
  return BitCast(d, TableLookupBytes(v, LoadDup128(di, kShuffle)));
#endif
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_T_SIZE_D(D, 1)>
HWY_API VFromD<D> Reverse(D d, const VFromD<D> v) {
  constexpr size_t kN = MaxLanes(d);
  if (kN == 1) return v;
#if HWY_TARGET <= HWY_SSE3
  // NOTE: Lanes with negative shuffle control mask values are set to zero.
  alignas(16) constexpr int8_t kReverse[16] = {
      kN - 1, kN - 2,  kN - 3,  kN - 4,  kN - 5,  kN - 6,  kN - 7,  kN - 8,
      kN - 9, kN - 10, kN - 11, kN - 12, kN - 13, kN - 14, kN - 15, kN - 16};
  const RebindToSigned<decltype(d)> di;
  const VFromD<decltype(di)> idx = Load(di, kReverse);
  return VFromD<D>{_mm_shuffle_epi8(BitCast(di, v).raw, idx.raw)};
#else
  const RepartitionToWide<decltype(d)> d16;
  return BitCast(d, Reverse(d16, RotateRight<8>(BitCast(d16, v))));
#endif
}

// ------------------------------ Reverse2

// Single lane: no change
template <class D, typename T = TFromD<D>, HWY_IF_LANES_D(D, 1)>
HWY_API Vec128<T, 1> Reverse2(D /* tag */, Vec128<T, 1> v) {
  return v;
}

template <class D, typename T = TFromD<D>, HWY_IF_T_SIZE(T, 2)>
HWY_API VFromD<D> Reverse2(D d, VFromD<D> v) {
#if HWY_TARGET <= HWY_AVX3
  const Repartition<uint32_t, decltype(d)> du32;
  return BitCast(d, RotateRight<16>(BitCast(du32, v)));
#elif HWY_TARGET == HWY_SSE2
  constexpr size_t kN = MaxLanes(d);
  __m128i shuf_result = _mm_shufflelo_epi16(v.raw, _MM_SHUFFLE(2, 3, 0, 1));
  if (kN > 4) {
    shuf_result = _mm_shufflehi_epi16(shuf_result, _MM_SHUFFLE(2, 3, 0, 1));
  }
  return VFromD<D>{shuf_result};
#else
  const RebindToSigned<decltype(d)> di;
  alignas(16) static constexpr int16_t kShuffle[8] = {
      0x0302, 0x0100, 0x0706, 0x0504, 0x0B0A, 0x0908, 0x0F0E, 0x0D0C};
  return BitCast(d, TableLookupBytes(v, LoadDup128(di, kShuffle)));
#endif
}

template <class D, typename T = TFromD<D>, HWY_IF_T_SIZE(T, 4)>
HWY_API VFromD<D> Reverse2(D /* tag */, VFromD<D> v) {
  return Shuffle2301(v);
}

template <class D, typename T = TFromD<D>, HWY_IF_T_SIZE(T, 8)>
HWY_API VFromD<D> Reverse2(D /* tag */, VFromD<D> v) {
  return Shuffle01(v);
}

// ------------------------------ Reverse4

template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_T_SIZE_D(D, 2)>
HWY_API VFromD<D> Reverse4(D d, VFromD<D> v) {
  // 4x 16-bit: a single shufflelo suffices.
  constexpr size_t kN = MaxLanes(d);
  if (kN <= 4) {
    return VFromD<D>{_mm_shufflelo_epi16(v.raw, _MM_SHUFFLE(0, 1, 2, 3))};
  }

#if HWY_TARGET == HWY_SSE2
  return VFromD<D>{
      _mm_shufflehi_epi16(_mm_shufflelo_epi16(v.raw, _MM_SHUFFLE(0, 1, 2, 3)),
                          _MM_SHUFFLE(0, 1, 2, 3))};
#else
  const RebindToSigned<decltype(d)> di;
  alignas(16) static constexpr int16_t kShuffle[8] = {
      0x0706, 0x0504, 0x0302, 0x0100, 0x0F0E, 0x0D0C, 0x0B0A, 0x0908};
  return BitCast(d, TableLookupBytes(v, LoadDup128(di, kShuffle)));
#endif
}

// 32-bit, any vector size: use Shuffle0123
template <class D, HWY_IF_T_SIZE_D(D, 4)>
HWY_API VFromD<D> Reverse4(D /* tag */, const VFromD<D> v) {
  return Shuffle0123(v);
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_T_SIZE_D(D, 8)>
HWY_API VFromD<D> Reverse4(D /* tag */, VFromD<D> /* v */) {
  HWY_ASSERT(0);  // don't have 4 u64 lanes
}

// ------------------------------ Reverse8

template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_T_SIZE_D(D, 2)>
HWY_API VFromD<D> Reverse8(D d, const VFromD<D> v) {
#if HWY_TARGET == HWY_SSE2
  const RepartitionToWide<decltype(d)> dw;
  return Reverse2(d, BitCast(d, Shuffle0123(BitCast(dw, v))));
#else
  const RebindToSigned<decltype(d)> di;
  alignas(16) static constexpr int16_t kShuffle[8] = {
      0x0F0E, 0x0D0C, 0x0B0A, 0x0908, 0x0706, 0x0504, 0x0302, 0x0100};
  return BitCast(d, TableLookupBytes(v, LoadDup128(di, kShuffle)));
#endif
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 16),
          HWY_IF_T_SIZE_ONE_OF_D(D, (1 << 4) | (1 << 8))>
HWY_API VFromD<D> Reverse8(D /* tag */, VFromD<D> /* v */) {
  HWY_ASSERT(0);  // don't have 8 lanes if larger than 16-bit
}

// ------------------------------ ReverseBits

#if HWY_TARGET <= HWY_AVX3_DL

#ifdef HWY_NATIVE_REVERSE_BITS_UI8
#undef HWY_NATIVE_REVERSE_BITS_UI8
#else
#define HWY_NATIVE_REVERSE_BITS_UI8
#endif

template <class V, HWY_IF_T_SIZE_V(V, 1), HWY_IF_V_SIZE_LE_D(DFromV<V>, 16)>
HWY_API V ReverseBits(V v) {
  const Full128<uint64_t> du64_full;
  const auto affine_matrix = Set(du64_full, 0x8040201008040201u);
  return V{_mm_gf2p8affine_epi64_epi8(v.raw, affine_matrix.raw, 0)};
}
#endif  // HWY_TARGET <= HWY_AVX3_DL

// ------------------------------ InterleaveLower

// Interleaves lanes from halves of the 128-bit blocks of "a" (which provides
// the least-significant lane) and "b". To concatenate two half-width integers
// into one, use ZipLower/Upper instead (also works with scalar).

template <size_t N>
HWY_API Vec128<uint8_t, N> InterleaveLower(Vec128<uint8_t, N> a,
                                           Vec128<uint8_t, N> b) {
  return Vec128<uint8_t, N>{_mm_unpacklo_epi8(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<uint16_t, N> InterleaveLower(Vec128<uint16_t, N> a,
                                            Vec128<uint16_t, N> b) {
  return Vec128<uint16_t, N>{_mm_unpacklo_epi16(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<uint32_t, N> InterleaveLower(Vec128<uint32_t, N> a,
                                            Vec128<uint32_t, N> b) {
  return Vec128<uint32_t, N>{_mm_unpacklo_epi32(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<uint64_t, N> InterleaveLower(Vec128<uint64_t, N> a,
                                            Vec128<uint64_t, N> b) {
  return Vec128<uint64_t, N>{_mm_unpacklo_epi64(a.raw, b.raw)};
}

template <size_t N>
HWY_API Vec128<int8_t, N> InterleaveLower(Vec128<int8_t, N> a,
                                          Vec128<int8_t, N> b) {
  return Vec128<int8_t, N>{_mm_unpacklo_epi8(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<int16_t, N> InterleaveLower(Vec128<int16_t, N> a,
                                           Vec128<int16_t, N> b) {
  return Vec128<int16_t, N>{_mm_unpacklo_epi16(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<int32_t, N> InterleaveLower(Vec128<int32_t, N> a,
                                           Vec128<int32_t, N> b) {
  return Vec128<int32_t, N>{_mm_unpacklo_epi32(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<int64_t, N> InterleaveLower(Vec128<int64_t, N> a,
                                           Vec128<int64_t, N> b) {
  return Vec128<int64_t, N>{_mm_unpacklo_epi64(a.raw, b.raw)};
}

template <size_t N>
HWY_API Vec128<float, N> InterleaveLower(Vec128<float, N> a,
                                         Vec128<float, N> b) {
  return Vec128<float, N>{_mm_unpacklo_ps(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<double, N> InterleaveLower(Vec128<double, N> a,
                                          Vec128<double, N> b) {
  return Vec128<double, N>{_mm_unpacklo_pd(a.raw, b.raw)};
}

// Additional overload for the optional tag (also for 256/512).
template <class D>
HWY_API VFromD<D> InterleaveLower(D /* tag */, VFromD<D> a, VFromD<D> b) {
  return InterleaveLower(a, b);
}

// ------------------------------ InterleaveUpper (UpperHalf)

// All functions inside detail lack the required D parameter.
namespace detail {

HWY_API Vec128<uint8_t> InterleaveUpper(Vec128<uint8_t> a, Vec128<uint8_t> b) {
  return Vec128<uint8_t>{_mm_unpackhi_epi8(a.raw, b.raw)};
}
HWY_API Vec128<uint16_t> InterleaveUpper(Vec128<uint16_t> a,
                                         Vec128<uint16_t> b) {
  return Vec128<uint16_t>{_mm_unpackhi_epi16(a.raw, b.raw)};
}
HWY_API Vec128<uint32_t> InterleaveUpper(Vec128<uint32_t> a,
                                         Vec128<uint32_t> b) {
  return Vec128<uint32_t>{_mm_unpackhi_epi32(a.raw, b.raw)};
}
HWY_API Vec128<uint64_t> InterleaveUpper(Vec128<uint64_t> a,
                                         Vec128<uint64_t> b) {
  return Vec128<uint64_t>{_mm_unpackhi_epi64(a.raw, b.raw)};
}

HWY_API Vec128<int8_t> InterleaveUpper(Vec128<int8_t> a, Vec128<int8_t> b) {
  return Vec128<int8_t>{_mm_unpackhi_epi8(a.raw, b.raw)};
}
HWY_API Vec128<int16_t> InterleaveUpper(Vec128<int16_t> a, Vec128<int16_t> b) {
  return Vec128<int16_t>{_mm_unpackhi_epi16(a.raw, b.raw)};
}
HWY_API Vec128<int32_t> InterleaveUpper(Vec128<int32_t> a, Vec128<int32_t> b) {
  return Vec128<int32_t>{_mm_unpackhi_epi32(a.raw, b.raw)};
}
HWY_API Vec128<int64_t> InterleaveUpper(Vec128<int64_t> a, Vec128<int64_t> b) {
  return Vec128<int64_t>{_mm_unpackhi_epi64(a.raw, b.raw)};
}

HWY_API Vec128<float> InterleaveUpper(Vec128<float> a, Vec128<float> b) {
  return Vec128<float>{_mm_unpackhi_ps(a.raw, b.raw)};
}
HWY_API Vec128<double> InterleaveUpper(Vec128<double> a, Vec128<double> b) {
  return Vec128<double>{_mm_unpackhi_pd(a.raw, b.raw)};
}

}  // namespace detail

// Full
template <class D, typename T = TFromD<D>>
HWY_API Vec128<T> InterleaveUpper(D /* tag */, Vec128<T> a, Vec128<T> b) {
  return detail::InterleaveUpper(a, b);
}

// Partial
template <class D, HWY_IF_V_SIZE_LE_D(D, 8)>
HWY_API VFromD<D> InterleaveUpper(D d, VFromD<D> a, VFromD<D> b) {
  const Half<decltype(d)> d2;
  return InterleaveLower(d, VFromD<D>{UpperHalf(d2, a).raw},
                         VFromD<D>{UpperHalf(d2, b).raw});
}

// ------------------------------ ZipLower/ZipUpper (InterleaveLower)

// Same as Interleave*, except that the return lanes are double-width integers;
// this is necessary because the single-lane scalar cannot return two values.
template <class V, class DW = RepartitionToWide<DFromV<V>>>
HWY_API VFromD<DW> ZipLower(V a, V b) {
  return BitCast(DW(), InterleaveLower(a, b));
}
template <class V, class D = DFromV<V>, class DW = RepartitionToWide<D>>
HWY_API VFromD<DW> ZipLower(DW dw, V a, V b) {
  return BitCast(dw, InterleaveLower(D(), a, b));
}

template <class V, class D = DFromV<V>, class DW = RepartitionToWide<D>>
HWY_API VFromD<DW> ZipUpper(DW dw, V a, V b) {
  return BitCast(dw, InterleaveUpper(D(), a, b));
}

// ================================================== COMBINE

// ------------------------------ Combine (InterleaveLower)

// N = N/2 + N/2 (upper half undefined)
template <class D, HWY_IF_V_SIZE_LE_D(D, 16), class VH = VFromD<Half<D>>>
HWY_API VFromD<D> Combine(D d, VH hi_half, VH lo_half) {
  const Half<decltype(d)> dh;
  const RebindToUnsigned<decltype(dh)> duh;
  // Treat half-width input as one lane, and expand to two lanes.
  using VU = Vec128<UnsignedFromSize<dh.MaxBytes()>, 2>;
  const VU lo{BitCast(duh, lo_half).raw};
  const VU hi{BitCast(duh, hi_half).raw};
  return BitCast(d, InterleaveLower(lo, hi));
}

// ------------------------------ ZeroExtendVector (Combine, IfThenElseZero)

// Tag dispatch instead of SFINAE for MSVC 2017 compatibility
namespace detail {

template <class D, typename T = TFromD<D>>
HWY_INLINE Vec128<T> ZeroExtendVector(hwy::NonFloatTag /*tag*/, D /* d */,
                                      Vec64<T> lo) {
  return Vec128<T>{_mm_move_epi64(lo.raw)};
}

template <class D, typename T = TFromD<D>>
HWY_INLINE Vec128<T> ZeroExtendVector(hwy::FloatTag /*tag*/, D d, Vec64<T> lo) {
  const RebindToUnsigned<decltype(d)> du;
  return BitCast(d, ZeroExtendVector(du, BitCast(Half<decltype(du)>(), lo)));
}

}  // namespace detail

template <class D, HWY_IF_V_SIZE_D(D, 16), typename T = TFromD<D>>
HWY_API Vec128<T> ZeroExtendVector(D d, Vec64<T> lo) {
  return detail::ZeroExtendVector(hwy::IsFloatTag<T>(), d, lo);
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 8)>
HWY_API VFromD<D> ZeroExtendVector(D d, VFromD<Half<D>> lo) {
  const Half<D> dh;
  return IfThenElseZero(FirstN(d, MaxLanes(dh)), VFromD<D>{lo.raw});
}

// ------------------------------ Concat full (InterleaveLower)

// hiH,hiL loH,loL |-> hiL,loL (= lower halves)
template <class D, typename T = TFromD<D>>
HWY_API Vec128<T> ConcatLowerLower(D d, Vec128<T> hi, Vec128<T> lo) {
  const Repartition<uint64_t, decltype(d)> d64;
  return BitCast(d, InterleaveLower(BitCast(d64, lo), BitCast(d64, hi)));
}

// hiH,hiL loH,loL |-> hiH,loH (= upper halves)
template <class D, typename T = TFromD<D>>
HWY_API Vec128<T> ConcatUpperUpper(D d, Vec128<T> hi, Vec128<T> lo) {
  const Repartition<uint64_t, decltype(d)> d64;
  return BitCast(d, InterleaveUpper(d64, BitCast(d64, lo), BitCast(d64, hi)));
}

// hiH,hiL loH,loL |-> hiL,loH (= inner halves)
template <class D, typename T = TFromD<D>>
HWY_API Vec128<T> ConcatLowerUpper(D d, Vec128<T> hi, Vec128<T> lo) {
  return CombineShiftRightBytes<8>(d, hi, lo);
}

// hiH,hiL loH,loL |-> hiH,loL (= outer halves)
template <class D, typename T = TFromD<D>>
HWY_API Vec128<T> ConcatUpperLower(D d, Vec128<T> hi, Vec128<T> lo) {
  const Repartition<double, decltype(d)> dd;
#if HWY_TARGET >= HWY_SSSE3
  return BitCast(
      d, Vec128<double>{_mm_shuffle_pd(BitCast(dd, lo).raw, BitCast(dd, hi).raw,
                                       _MM_SHUFFLE2(1, 0))});
#else
  // _mm_blend_epi16 has throughput 1/cycle on SKX, whereas _pd can do 3/cycle.
  return BitCast(d, Vec128<double>{_mm_blend_pd(BitCast(dd, hi).raw,
                                                BitCast(dd, lo).raw, 1)});
#endif
}
template <class D>
HWY_API Vec128<float> ConcatUpperLower(D d, Vec128<float> hi,
                                       Vec128<float> lo) {
#if HWY_TARGET >= HWY_SSSE3
  (void)d;
  return Vec128<float>{_mm_shuffle_ps(lo.raw, hi.raw, _MM_SHUFFLE(3, 2, 1, 0))};
#else
  // _mm_shuffle_ps has throughput 1/cycle on SKX, whereas blend can do 3/cycle.
  const RepartitionToWide<decltype(d)> dd;
  return BitCast(d, Vec128<double>{_mm_blend_pd(BitCast(dd, hi).raw,
                                                BitCast(dd, lo).raw, 1)});
#endif
}
template <class D>
HWY_API Vec128<double> ConcatUpperLower(D /* tag */, Vec128<double> hi,
                                        Vec128<double> lo) {
#if HWY_TARGET >= HWY_SSSE3
  return Vec128<double>{_mm_shuffle_pd(lo.raw, hi.raw, _MM_SHUFFLE2(1, 0))};
#else
  // _mm_shuffle_pd has throughput 1/cycle on SKX, whereas blend can do 3/cycle.
  return Vec128<double>{_mm_blend_pd(hi.raw, lo.raw, 1)};
#endif
}

// ------------------------------ Concat partial (Combine, LowerHalf)

template <class D, HWY_IF_V_SIZE_LE_D(D, 8)>
HWY_API VFromD<D> ConcatLowerLower(D d, VFromD<D> hi, VFromD<D> lo) {
  const Half<decltype(d)> d2;
  return Combine(d, LowerHalf(d2, hi), LowerHalf(d2, lo));
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 8)>
HWY_API VFromD<D> ConcatUpperUpper(D d, VFromD<D> hi, VFromD<D> lo) {
  const Half<decltype(d)> d2;
  return Combine(d, UpperHalf(d2, hi), UpperHalf(d2, lo));
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 8)>
HWY_API VFromD<D> ConcatLowerUpper(D d, const VFromD<D> hi,
                                   const VFromD<D> lo) {
  const Half<decltype(d)> d2;
  return Combine(d, LowerHalf(d2, hi), UpperHalf(d2, lo));
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 8)>
HWY_API VFromD<D> ConcatUpperLower(D d, VFromD<D> hi, VFromD<D> lo) {
  const Half<decltype(d)> d2;
  return Combine(d, UpperHalf(d2, hi), LowerHalf(d2, lo));
}

// ------------------------------ ConcatOdd

// 8-bit full
template <class D, typename T = TFromD<D>, HWY_IF_T_SIZE(T, 1)>
HWY_API Vec128<T> ConcatOdd(D d, Vec128<T> hi, Vec128<T> lo) {
  const Repartition<uint16_t, decltype(d)> dw;
  // Right-shift 8 bits per u16 so we can pack.
  const Vec128<uint16_t> uH = ShiftRight<8>(BitCast(dw, hi));
  const Vec128<uint16_t> uL = ShiftRight<8>(BitCast(dw, lo));
  return Vec128<T>{_mm_packus_epi16(uL.raw, uH.raw)};
}

// 8-bit x8
template <class D, typename T = TFromD<D>, HWY_IF_T_SIZE(T, 1)>
HWY_API Vec64<T> ConcatOdd(D d, Vec64<T> hi, Vec64<T> lo) {
#if HWY_TARGET == HWY_SSE2
  const Repartition<uint16_t, decltype(d)> dw;
  // Right-shift 8 bits per u16 so we can pack.
  const Vec64<uint16_t> uH = ShiftRight<8>(BitCast(dw, hi));
  const Vec64<uint16_t> uL = ShiftRight<8>(BitCast(dw, lo));
  return Vec64<T>{_mm_shuffle_epi32(_mm_packus_epi16(uL.raw, uH.raw),
                                    _MM_SHUFFLE(2, 0, 2, 0))};
#else
  const Repartition<uint32_t, decltype(d)> du32;
  // Don't care about upper half, no need to zero.
  alignas(16) const uint8_t kCompactOddU8[8] = {1, 3, 5, 7};
  const Vec64<T> shuf = BitCast(d, Load(Full64<uint8_t>(), kCompactOddU8));
  const Vec64<T> L = TableLookupBytes(lo, shuf);
  const Vec64<T> H = TableLookupBytes(hi, shuf);
  return BitCast(d, InterleaveLower(du32, BitCast(du32, L), BitCast(du32, H)));
#endif
}

// 8-bit x4
template <class D, typename T = TFromD<D>, HWY_IF_T_SIZE(T, 1)>
HWY_API Vec32<T> ConcatOdd(D d, Vec32<T> hi, Vec32<T> lo) {
#if HWY_TARGET == HWY_SSE2
  const Repartition<uint16_t, decltype(d)> dw;
  const Twice<decltype(dw)> dw_2;
  // Right-shift 8 bits per u16 so we can pack.
  const Vec32<uint16_t> uH = ShiftRight<8>(BitCast(dw, hi));
  const Vec32<uint16_t> uL = ShiftRight<8>(BitCast(dw, lo));
  const Vec64<uint16_t> uHL = Combine(dw_2, uH, uL);
  return Vec32<T>{_mm_packus_epi16(uHL.raw, uHL.raw)};
#else
  const Repartition<uint16_t, decltype(d)> du16;
  // Don't care about upper half, no need to zero.
  alignas(16) const uint8_t kCompactOddU8[4] = {1, 3};
  const Vec32<T> shuf = BitCast(d, Load(Full32<uint8_t>(), kCompactOddU8));
  const Vec32<T> L = TableLookupBytes(lo, shuf);
  const Vec32<T> H = TableLookupBytes(hi, shuf);
  return BitCast(d, InterleaveLower(du16, BitCast(du16, L), BitCast(du16, H)));
#endif
}

// 16-bit full
template <class D, typename T = TFromD<D>, HWY_IF_T_SIZE(T, 2)>
HWY_API Vec128<T> ConcatOdd(D d, Vec128<T> hi, Vec128<T> lo) {
  // Right-shift 16 bits per i32 - a *signed* shift of 0x8000xxxx returns
  // 0xFFFF8000, which correctly saturates to 0x8000.
  const Repartition<int32_t, decltype(d)> dw;
  const Vec128<int32_t> uH = ShiftRight<16>(BitCast(dw, hi));
  const Vec128<int32_t> uL = ShiftRight<16>(BitCast(dw, lo));
  return Vec128<T>{_mm_packs_epi32(uL.raw, uH.raw)};
}

// 16-bit x4
template <class D, typename T = TFromD<D>, HWY_IF_T_SIZE(T, 2)>
HWY_API Vec64<T> ConcatOdd(D d, Vec64<T> hi, Vec64<T> lo) {
#if HWY_TARGET == HWY_SSE2
  // Right-shift 16 bits per i32 - a *signed* shift of 0x8000xxxx returns
  // 0xFFFF8000, which correctly saturates to 0x8000.
  const Repartition<int32_t, decltype(d)> dw;
  const Vec64<int32_t> uH = ShiftRight<16>(BitCast(dw, hi));
  const Vec64<int32_t> uL = ShiftRight<16>(BitCast(dw, lo));
  return Vec64<T>{_mm_shuffle_epi32(_mm_packs_epi32(uL.raw, uH.raw),
                                    _MM_SHUFFLE(2, 0, 2, 0))};
#else
  const Repartition<uint32_t, decltype(d)> du32;
  // Don't care about upper half, no need to zero.
  alignas(16) const uint8_t kCompactOddU16[8] = {2, 3, 6, 7};
  const Vec64<T> shuf = BitCast(d, Load(Full64<uint8_t>(), kCompactOddU16));
  const Vec64<T> L = TableLookupBytes(lo, shuf);
  const Vec64<T> H = TableLookupBytes(hi, shuf);
  return BitCast(d, InterleaveLower(du32, BitCast(du32, L), BitCast(du32, H)));
#endif
}

// 32-bit full
template <class D, typename T = TFromD<D>, HWY_IF_T_SIZE(T, 4)>
HWY_API Vec128<T> ConcatOdd(D d, Vec128<T> hi, Vec128<T> lo) {
  const RebindToFloat<decltype(d)> df;
  return BitCast(
      d, Vec128<float>{_mm_shuffle_ps(BitCast(df, lo).raw, BitCast(df, hi).raw,
                                      _MM_SHUFFLE(3, 1, 3, 1))});
}
template <class D>
HWY_API Vec128<float> ConcatOdd(D /* d */, Vec128<float> hi, Vec128<float> lo) {
  return Vec128<float>{_mm_shuffle_ps(lo.raw, hi.raw, _MM_SHUFFLE(3, 1, 3, 1))};
}

// Any type x2
template <class D, typename T = TFromD<D>, HWY_IF_LANES_D(D, 2)>
HWY_API Vec128<T, 2> ConcatOdd(D d, Vec128<T, 2> hi, Vec128<T, 2> lo) {
  return InterleaveUpper(d, lo, hi);
}

// ------------------------------ ConcatEven (InterleaveLower)

// 8-bit full
template <class D, typename T = TFromD<D>, HWY_IF_T_SIZE(T, 1)>
HWY_API Vec128<T> ConcatEven(D d, Vec128<T> hi, Vec128<T> lo) {
  const Repartition<uint16_t, decltype(d)> dw;
  // Isolate lower 8 bits per u16 so we can pack.
  const Vec128<uint16_t> mask = Set(dw, 0x00FF);
  const Vec128<uint16_t> uH = And(BitCast(dw, hi), mask);
  const Vec128<uint16_t> uL = And(BitCast(dw, lo), mask);
  return Vec128<T>{_mm_packus_epi16(uL.raw, uH.raw)};
}

// 8-bit x8
template <class D, typename T = TFromD<D>, HWY_IF_T_SIZE(T, 1)>
HWY_API Vec64<T> ConcatEven(D d, Vec64<T> hi, Vec64<T> lo) {
#if HWY_TARGET == HWY_SSE2
  const Repartition<uint16_t, decltype(d)> dw;
  // Isolate lower 8 bits per u16 so we can pack.
  const Vec64<uint16_t> mask = Set(dw, 0x00FF);
  const Vec64<uint16_t> uH = And(BitCast(dw, hi), mask);
  const Vec64<uint16_t> uL = And(BitCast(dw, lo), mask);
  return Vec64<T>{_mm_shuffle_epi32(_mm_packus_epi16(uL.raw, uH.raw),
                                    _MM_SHUFFLE(2, 0, 2, 0))};
#else
  const Repartition<uint32_t, decltype(d)> du32;
  // Don't care about upper half, no need to zero.
  alignas(16) const uint8_t kCompactEvenU8[8] = {0, 2, 4, 6};
  const Vec64<T> shuf = BitCast(d, Load(Full64<uint8_t>(), kCompactEvenU8));
  const Vec64<T> L = TableLookupBytes(lo, shuf);
  const Vec64<T> H = TableLookupBytes(hi, shuf);
  return BitCast(d, InterleaveLower(du32, BitCast(du32, L), BitCast(du32, H)));
#endif
}

// 8-bit x4
template <class D, typename T = TFromD<D>, HWY_IF_T_SIZE(T, 1)>
HWY_API Vec32<T> ConcatEven(D d, Vec32<T> hi, Vec32<T> lo) {
#if HWY_TARGET == HWY_SSE2
  const Repartition<uint16_t, decltype(d)> dw;
  const Twice<decltype(dw)> dw_2;
  // Isolate lower 8 bits per u16 so we can pack.
  const Vec32<uint16_t> mask = Set(dw, 0x00FF);
  const Vec32<uint16_t> uH = And(BitCast(dw, hi), mask);
  const Vec32<uint16_t> uL = And(BitCast(dw, lo), mask);
  const Vec64<uint16_t> uHL = Combine(dw_2, uH, uL);
  return Vec32<T>{_mm_packus_epi16(uHL.raw, uHL.raw)};
#else
  const Repartition<uint16_t, decltype(d)> du16;
  // Don't care about upper half, no need to zero.
  alignas(16) const uint8_t kCompactEvenU8[4] = {0, 2};
  const Vec32<T> shuf = BitCast(d, Load(Full32<uint8_t>(), kCompactEvenU8));
  const Vec32<T> L = TableLookupBytes(lo, shuf);
  const Vec32<T> H = TableLookupBytes(hi, shuf);
  return BitCast(d, InterleaveLower(du16, BitCast(du16, L), BitCast(du16, H)));
#endif
}

// 16-bit full
template <class D, typename T = TFromD<D>, HWY_IF_T_SIZE(T, 2)>
HWY_API Vec128<T> ConcatEven(D d, Vec128<T> hi, Vec128<T> lo) {
#if HWY_TARGET <= HWY_SSE4
  // Isolate lower 16 bits per u32 so we can pack.
  const Repartition<uint32_t, decltype(d)> dw;
  const Vec128<uint32_t> mask = Set(dw, 0x0000FFFF);
  const Vec128<uint32_t> uH = And(BitCast(dw, hi), mask);
  const Vec128<uint32_t> uL = And(BitCast(dw, lo), mask);
  return Vec128<T>{_mm_packus_epi32(uL.raw, uH.raw)};
#elif HWY_TARGET == HWY_SSE2
  const Repartition<uint32_t, decltype(d)> dw;
  return ConcatOdd(d, BitCast(d, ShiftLeft<16>(BitCast(dw, hi))),
                   BitCast(d, ShiftLeft<16>(BitCast(dw, lo))));
#else
  // packs_epi32 saturates 0x8000 to 0x7FFF. Instead ConcatEven within the two
  // inputs, then concatenate them.
  alignas(16) const T kCompactEvenU16[8] = {0x0100, 0x0504, 0x0908, 0x0D0C};
  const Vec128<T> shuf = BitCast(d, Load(d, kCompactEvenU16));
  const Vec128<T> L = TableLookupBytes(lo, shuf);
  const Vec128<T> H = TableLookupBytes(hi, shuf);
  return ConcatLowerLower(d, H, L);
#endif
}

// 16-bit x4
template <class D, typename T = TFromD<D>, HWY_IF_T_SIZE(T, 2)>
HWY_API Vec64<T> ConcatEven(D d, Vec64<T> hi, Vec64<T> lo) {
#if HWY_TARGET == HWY_SSE2
  const Repartition<uint32_t, decltype(d)> dw;
  return ConcatOdd(d, BitCast(d, ShiftLeft<16>(BitCast(dw, hi))),
                   BitCast(d, ShiftLeft<16>(BitCast(dw, lo))));
#else
  const Repartition<uint32_t, decltype(d)> du32;
  // Don't care about upper half, no need to zero.
  alignas(16) const uint8_t kCompactEvenU16[8] = {0, 1, 4, 5};
  const Vec64<T> shuf = BitCast(d, Load(Full64<uint8_t>(), kCompactEvenU16));
  const Vec64<T> L = TableLookupBytes(lo, shuf);
  const Vec64<T> H = TableLookupBytes(hi, shuf);
  return BitCast(d, InterleaveLower(du32, BitCast(du32, L), BitCast(du32, H)));
#endif
}

// 32-bit full
template <class D, typename T = TFromD<D>, HWY_IF_T_SIZE(T, 4)>
HWY_API Vec128<T> ConcatEven(D d, Vec128<T> hi, Vec128<T> lo) {
  const RebindToFloat<decltype(d)> df;
  return BitCast(
      d, Vec128<float>{_mm_shuffle_ps(BitCast(df, lo).raw, BitCast(df, hi).raw,
                                      _MM_SHUFFLE(2, 0, 2, 0))});
}
template <class D>
HWY_API Vec128<float> ConcatEven(D /* d */, Vec128<float> hi,
                                 Vec128<float> lo) {
  return Vec128<float>{_mm_shuffle_ps(lo.raw, hi.raw, _MM_SHUFFLE(2, 0, 2, 0))};
}

// Any T x2
template <typename D, typename T = TFromD<D>, HWY_IF_LANES_D(D, 2)>
HWY_API Vec128<T, 2> ConcatEven(D d, Vec128<T, 2> hi, Vec128<T, 2> lo) {
  return InterleaveLower(d, lo, hi);
}

// ------------------------------ DupEven (InterleaveLower)

template <typename T, size_t N, HWY_IF_T_SIZE(T, 4)>
HWY_API Vec128<T, N> DupEven(Vec128<T, N> v) {
  return Vec128<T, N>{_mm_shuffle_epi32(v.raw, _MM_SHUFFLE(2, 2, 0, 0))};
}
template <size_t N>
HWY_API Vec128<float, N> DupEven(Vec128<float, N> v) {
  return Vec128<float, N>{
      _mm_shuffle_ps(v.raw, v.raw, _MM_SHUFFLE(2, 2, 0, 0))};
}

template <typename T, size_t N, HWY_IF_T_SIZE(T, 8)>
HWY_API Vec128<T, N> DupEven(const Vec128<T, N> v) {
  return InterleaveLower(DFromV<decltype(v)>(), v, v);
}

// ------------------------------ DupOdd (InterleaveUpper)

template <typename T, size_t N, HWY_IF_T_SIZE(T, 4)>
HWY_API Vec128<T, N> DupOdd(Vec128<T, N> v) {
  return Vec128<T, N>{_mm_shuffle_epi32(v.raw, _MM_SHUFFLE(3, 3, 1, 1))};
}
template <size_t N>
HWY_API Vec128<float, N> DupOdd(Vec128<float, N> v) {
  return Vec128<float, N>{
      _mm_shuffle_ps(v.raw, v.raw, _MM_SHUFFLE(3, 3, 1, 1))};
}

template <typename T, size_t N, HWY_IF_T_SIZE(T, 8)>
HWY_API Vec128<T, N> DupOdd(const Vec128<T, N> v) {
  return InterleaveUpper(DFromV<decltype(v)>(), v, v);
}

// ------------------------------ TwoTablesLookupLanes (DupEven)

template <typename T, size_t N, HWY_IF_V_SIZE_LE(T, N, 8)>
HWY_API Vec128<T, N> TwoTablesLookupLanes(Vec128<T, N> a, Vec128<T, N> b,
                                          Indices128<T, N> idx) {
  const DFromV<decltype(a)> d;
  const Twice<decltype(d)> dt;
// TableLookupLanes currently requires table and index vectors to be the same
// size, though a half-length index vector would be sufficient here.
#if HWY_IS_MSAN
  const Vec128<T, N> idx_vec{idx.raw};
  const Indices128<T, N * 2> idx2{Combine(dt, idx_vec, idx_vec).raw};
#else
  // We only keep LowerHalf of the result, which is valid in idx.
  const Indices128<T, N * 2> idx2{idx.raw};
#endif
  return LowerHalf(d, TableLookupLanes(Combine(dt, b, a), idx2));
}

template <typename T, HWY_IF_T_SIZE(T, 1)>
HWY_API Vec128<T> TwoTablesLookupLanes(Vec128<T> a, Vec128<T> b,
                                       Indices128<T> idx) {
#if HWY_TARGET <= HWY_AVX3_DL
  return Vec128<T>{_mm_permutex2var_epi8(a.raw, idx.raw, b.raw)};
#else  // AVX3 or below
  const DFromV<decltype(a)> d;
  const Vec128<T> idx_vec{idx.raw};

#if HWY_TARGET <= HWY_SSE4
  const Repartition<uint16_t, decltype(d)> du16;
  const auto sel_hi_mask =
      MaskFromVec(BitCast(d, ShiftLeft<3>(BitCast(du16, idx_vec))));
#else
  const RebindToSigned<decltype(d)> di;
  const auto sel_hi_mask =
      RebindMask(d, BitCast(di, idx_vec) > Set(di, int8_t{15}));
#endif

  const auto lo_lookup_result = TableLookupBytes(a, idx_vec);
#if HWY_TARGET <= HWY_AVX3
  const Vec128<T> lookup_result{_mm_mask_shuffle_epi8(
      lo_lookup_result.raw, sel_hi_mask.raw, b.raw, idx_vec.raw)};
  return lookup_result;
#else
  const auto hi_lookup_result = TableLookupBytes(b, idx_vec);
  return IfThenElse(sel_hi_mask, hi_lookup_result, lo_lookup_result);
#endif  // HWY_TARGET <= HWY_AVX3
#endif  // HWY_TARGET <= HWY_AVX3_DL
}

template <typename T, HWY_IF_T_SIZE(T, 2)>
HWY_API Vec128<T> TwoTablesLookupLanes(Vec128<T> a, Vec128<T> b,
                                       Indices128<T> idx) {
#if HWY_TARGET <= HWY_AVX3
  return Vec128<T>{_mm_permutex2var_epi16(a.raw, idx.raw, b.raw)};
#elif HWY_TARGET == HWY_SSE2
  const DFromV<decltype(a)> d;
  const RebindToSigned<decltype(d)> di;
  const Vec128<T> idx_vec{idx.raw};
  const auto sel_hi_mask =
      RebindMask(d, BitCast(di, idx_vec) > Set(di, int16_t{7}));
  const auto lo_lookup_result = TableLookupLanes(a, idx);
  const auto hi_lookup_result = TableLookupLanes(b, idx);
  return IfThenElse(sel_hi_mask, hi_lookup_result, lo_lookup_result);
#else
  const DFromV<decltype(a)> d;
  const Repartition<uint8_t, decltype(d)> du8;
  return BitCast(d, TwoTablesLookupLanes(BitCast(du8, a), BitCast(du8, b),
                                         Indices128<uint8_t>{idx.raw}));
#endif
}

template <typename T, HWY_IF_UI32(T)>
HWY_API Vec128<T> TwoTablesLookupLanes(Vec128<T> a, Vec128<T> b,
                                       Indices128<T> idx) {
#if HWY_TARGET <= HWY_AVX3
  return Vec128<T>{_mm_permutex2var_epi32(a.raw, idx.raw, b.raw)};
#else  // AVX2 or below
  const DFromV<decltype(a)> d;

#if HWY_TARGET <= HWY_AVX2 || HWY_TARGET == HWY_SSE2
  const Vec128<T> idx_vec{idx.raw};

#if HWY_TARGET <= HWY_AVX2
  const RebindToFloat<decltype(d)> d_sel;
  const auto sel_hi_mask = MaskFromVec(BitCast(d_sel, ShiftLeft<29>(idx_vec)));
#else
  const RebindToSigned<decltype(d)> d_sel;
  const auto sel_hi_mask = BitCast(d_sel, idx_vec) > Set(d_sel, int32_t{3});
#endif

  const auto lo_lookup_result = BitCast(d_sel, TableLookupLanes(a, idx));
  const auto hi_lookup_result = BitCast(d_sel, TableLookupLanes(b, idx));
  return BitCast(d,
                 IfThenElse(sel_hi_mask, hi_lookup_result, lo_lookup_result));
#else   // SSSE3 or SSE4
  const Repartition<uint8_t, decltype(d)> du8;
  return BitCast(d, TwoTablesLookupLanes(BitCast(du8, a), BitCast(du8, b),
                                         Indices128<uint8_t>{idx.raw}));
#endif  // HWY_TARGET <= HWY_AVX2 || HWY_TARGET == HWY_SSE2
#endif  // HWY_TARGET <= HWY_AVX3
}

HWY_API Vec128<float> TwoTablesLookupLanes(Vec128<float> a, Vec128<float> b,
                                           Indices128<float> idx) {
#if HWY_TARGET <= HWY_AVX3
  return Vec128<float>{_mm_permutex2var_ps(a.raw, idx.raw, b.raw)};
#elif HWY_TARGET <= HWY_AVX2 || HWY_TARGET == HWY_SSE2
  const DFromV<decltype(a)> d;

#if HWY_TARGET <= HWY_AVX2
  const auto sel_hi_mask =
      MaskFromVec(BitCast(d, ShiftLeft<29>(Vec128<int32_t>{idx.raw})));
#else
  const RebindToSigned<decltype(d)> di;
  const auto sel_hi_mask =
      RebindMask(d, Vec128<int32_t>{idx.raw} > Set(di, int32_t{3}));
#endif

  const auto lo_lookup_result = TableLookupLanes(a, idx);
  const auto hi_lookup_result = TableLookupLanes(b, idx);
  return IfThenElse(sel_hi_mask, hi_lookup_result, lo_lookup_result);
#else  // SSSE3 or SSE4
  const DFromV<decltype(a)> d;
  const Repartition<uint8_t, decltype(d)> du8;
  return BitCast(d, TwoTablesLookupLanes(BitCast(du8, a), BitCast(du8, b),
                                         Indices128<uint8_t>{idx.raw}));
#endif
}

template <typename T, HWY_IF_UI64(T)>
HWY_API Vec128<T> TwoTablesLookupLanes(Vec128<T> a, Vec128<T> b,
                                       Indices128<T> idx) {
#if HWY_TARGET <= HWY_AVX3
  return Vec128<T>{_mm_permutex2var_epi64(a.raw, idx.raw, b.raw)};
#else
  const DFromV<decltype(a)> d;
  const Vec128<T> idx_vec{idx.raw};
  const Indices128<T> idx_mod{And(idx_vec, Set(d, T{1})).raw};

#if HWY_TARGET <= HWY_SSE4
  const RebindToFloat<decltype(d)> d_sel;
  const auto sel_hi_mask = MaskFromVec(BitCast(d_sel, ShiftLeft<62>(idx_vec)));
#else   // SSE2 or SSSE3
  const Repartition<int32_t, decltype(d)> di32;
  const RebindToSigned<decltype(d)> d_sel;
  const auto sel_hi_mask = MaskFromVec(
      BitCast(d_sel, VecFromMask(di32, DupEven(BitCast(di32, idx_vec)) >
                                           Set(di32, int32_t{1}))));
#endif  // HWY_TARGET <= HWY_SSE4

  const auto lo_lookup_result = BitCast(d_sel, TableLookupLanes(a, idx_mod));
  const auto hi_lookup_result = BitCast(d_sel, TableLookupLanes(b, idx_mod));
  return BitCast(d,
                 IfThenElse(sel_hi_mask, hi_lookup_result, lo_lookup_result));
#endif  // HWY_TARGET <= HWY_AVX3
}

HWY_API Vec128<double> TwoTablesLookupLanes(Vec128<double> a, Vec128<double> b,
                                            Indices128<double> idx) {
#if HWY_TARGET <= HWY_AVX3
  return Vec128<double>{_mm_permutex2var_pd(a.raw, idx.raw, b.raw)};
#else
  const DFromV<decltype(a)> d;
  const RebindToSigned<decltype(d)> di;
  const Vec128<int64_t> idx_vec{idx.raw};
  const Indices128<double> idx_mod{And(idx_vec, Set(di, int64_t{1})).raw};

#if HWY_TARGET <= HWY_SSE4
  const auto sel_hi_mask = MaskFromVec(BitCast(d, ShiftLeft<62>(idx_vec)));
#else   // SSE2 or SSSE3
  const Repartition<int32_t, decltype(d)> di32;
  const auto sel_hi_mask =
      MaskFromVec(BitCast(d, VecFromMask(di32, DupEven(BitCast(di32, idx_vec)) >
                                                   Set(di32, int32_t{1}))));
#endif  // HWY_TARGET <= HWY_SSE4

  const auto lo_lookup_result = TableLookupLanes(a, idx_mod);
  const auto hi_lookup_result = TableLookupLanes(b, idx_mod);
  return IfThenElse(sel_hi_mask, hi_lookup_result, lo_lookup_result);
#endif  // HWY_TARGET <= HWY_AVX3
}

// ------------------------------ OddEven (IfThenElse)

template <typename T, size_t N, HWY_IF_T_SIZE(T, 1)>
HWY_INLINE Vec128<T, N> OddEven(const Vec128<T, N> a, const Vec128<T, N> b) {
  const DFromV<decltype(a)> d;
  const Repartition<uint8_t, decltype(d)> d8;
  alignas(16) static constexpr uint8_t mask[16] = {
      0xFF, 0, 0xFF, 0, 0xFF, 0, 0xFF, 0, 0xFF, 0, 0xFF, 0, 0xFF, 0, 0xFF, 0};
  return IfThenElse(MaskFromVec(BitCast(d, Load(d8, mask))), b, a);
}

template <typename T, size_t N, HWY_IF_T_SIZE(T, 2)>
HWY_INLINE Vec128<T, N> OddEven(const Vec128<T, N> a, const Vec128<T, N> b) {
#if HWY_TARGET >= HWY_SSSE3
  const DFromV<decltype(a)> d;
  const Repartition<uint8_t, decltype(d)> d8;
  alignas(16) static constexpr uint8_t mask[16] = {
      0xFF, 0xFF, 0, 0, 0xFF, 0xFF, 0, 0, 0xFF, 0xFF, 0, 0, 0xFF, 0xFF, 0, 0};
  return IfThenElse(MaskFromVec(BitCast(d, Load(d8, mask))), b, a);
#else
  return Vec128<T, N>{_mm_blend_epi16(a.raw, b.raw, 0x55)};
#endif
}

template <typename T, size_t N, HWY_IF_T_SIZE(T, 4)>
HWY_INLINE Vec128<T, N> OddEven(const Vec128<T, N> a, const Vec128<T, N> b) {
#if HWY_TARGET >= HWY_SSSE3
  const __m128i odd = _mm_shuffle_epi32(a.raw, _MM_SHUFFLE(3, 1, 3, 1));
  const __m128i even = _mm_shuffle_epi32(b.raw, _MM_SHUFFLE(2, 0, 2, 0));
  return Vec128<T, N>{_mm_unpacklo_epi32(even, odd)};
#else
  // _mm_blend_epi16 has throughput 1/cycle on SKX, whereas _ps can do 3/cycle.
  const DFromV<decltype(a)> d;
  const RebindToFloat<decltype(d)> df;
  return BitCast(d, Vec128<float, N>{_mm_blend_ps(BitCast(df, a).raw,
                                                  BitCast(df, b).raw, 5)});
#endif
}

template <typename T, size_t N, HWY_IF_T_SIZE(T, 8)>
HWY_INLINE Vec128<T, N> OddEven(const Vec128<T, N> a, const Vec128<T, N> b) {
  // Same as ConcatUpperLower for full vectors; do not call that because this
  // is more efficient for 64x1 vectors.
  const DFromV<decltype(a)> d;
  const RebindToFloat<decltype(d)> dd;
#if HWY_TARGET >= HWY_SSSE3
  return BitCast(
      d, Vec128<double, N>{_mm_shuffle_pd(
             BitCast(dd, b).raw, BitCast(dd, a).raw, _MM_SHUFFLE2(1, 0))});
#else
  // _mm_shuffle_pd has throughput 1/cycle on SKX, whereas blend can do 3/cycle.
  return BitCast(d, Vec128<double, N>{_mm_blend_pd(BitCast(dd, a).raw,
                                                   BitCast(dd, b).raw, 1)});
#endif
}

template <size_t N>
HWY_API Vec128<float, N> OddEven(Vec128<float, N> a, Vec128<float, N> b) {
#if HWY_TARGET >= HWY_SSSE3
  // SHUFPS must fill the lower half of the output from one input, so we
  // need another shuffle. Unpack avoids another immediate byte.
  const __m128 odd = _mm_shuffle_ps(a.raw, a.raw, _MM_SHUFFLE(3, 1, 3, 1));
  const __m128 even = _mm_shuffle_ps(b.raw, b.raw, _MM_SHUFFLE(2, 0, 2, 0));
  return Vec128<float, N>{_mm_unpacklo_ps(even, odd)};
#else
  return Vec128<float, N>{_mm_blend_ps(a.raw, b.raw, 5)};
#endif
}

// ------------------------------ OddEvenBlocks
template <typename T, size_t N>
HWY_API Vec128<T, N> OddEvenBlocks(Vec128<T, N> /* odd */, Vec128<T, N> even) {
  return even;
}

// ------------------------------ SwapAdjacentBlocks

template <typename T, size_t N>
HWY_API Vec128<T, N> SwapAdjacentBlocks(Vec128<T, N> v) {
  return v;
}

// ------------------------------ Shl (ZipLower, Mul)

// Use AVX2/3 variable shifts where available, otherwise multiply by powers of
// two from loading float exponents, which is considerably faster (according
// to LLVM-MCA) than scalar or testing bits: https://gcc.godbolt.org/z/9G7Y9v.

namespace detail {
#if HWY_TARGET > HWY_AVX3  // Unused for AVX3 - we use sllv directly

// Returns 2^v for use as per-lane multipliers to emulate 16-bit shifts.
template <typename T, size_t N, HWY_IF_T_SIZE(T, 2)>
HWY_INLINE Vec128<MakeUnsigned<T>, N> Pow2(const Vec128<T, N> v) {
  const DFromV<decltype(v)> d;
  const RebindToUnsigned<decltype(d)> du;
  const RepartitionToWide<decltype(d)> dw;
  const Rebind<float, decltype(dw)> df;
  const auto zero = Zero(d);
  // Move into exponent (this u16 will become the upper half of an f32)
  const auto exp = ShiftLeft<23 - 16>(v);
  const auto upper = exp + Set(d, 0x3F80);  // upper half of 1.0f
  // Insert 0 into lower halves for reinterpreting as binary32.
  const auto f0 = ZipLower(dw, zero, upper);
  const auto f1 = ZipUpper(dw, zero, upper);
  // See cvtps comment below.
  const VFromD<decltype(dw)> bits0{_mm_cvtps_epi32(BitCast(df, f0).raw)};
  const VFromD<decltype(dw)> bits1{_mm_cvtps_epi32(BitCast(df, f1).raw)};
#if HWY_TARGET <= HWY_SSE4
  return VFromD<decltype(du)>{_mm_packus_epi32(bits0.raw, bits1.raw)};
#else
  return ConcatEven(du, BitCast(du, bits1), BitCast(du, bits0));
#endif
}

// Same, for 32-bit shifts.
template <typename T, size_t N, HWY_IF_T_SIZE(T, 4)>
HWY_INLINE Vec128<MakeUnsigned<T>, N> Pow2(const Vec128<T, N> v) {
  const DFromV<decltype(v)> d;
  const auto exp = ShiftLeft<23>(v);
  const auto f = exp + Set(d, 0x3F800000);  // 1.0f
  // Do not use ConvertTo because we rely on the native 0x80..00 overflow
  // behavior.
  return Vec128<MakeUnsigned<T>, N>{_mm_cvtps_epi32(_mm_castsi128_ps(f.raw))};
}

#endif  // HWY_TARGET > HWY_AVX3

template <size_t N>
HWY_API Vec128<uint16_t, N> Shl(hwy::UnsignedTag /*tag*/, Vec128<uint16_t, N> v,
                                Vec128<uint16_t, N> bits) {
#if HWY_TARGET <= HWY_AVX3
  return Vec128<uint16_t, N>{_mm_sllv_epi16(v.raw, bits.raw)};
#else
  return v * Pow2(bits);
#endif
}
HWY_API Vec128<uint16_t, 1> Shl(hwy::UnsignedTag /*tag*/, Vec128<uint16_t, 1> v,
                                Vec128<uint16_t, 1> bits) {
  return Vec128<uint16_t, 1>{_mm_sll_epi16(v.raw, bits.raw)};
}

// 8-bit: may use the Shl overload for uint16_t.
template <size_t N>
HWY_API Vec128<uint8_t, N> Shl(hwy::UnsignedTag tag, Vec128<uint8_t, N> v,
                               Vec128<uint8_t, N> bits) {
  const DFromV<decltype(v)> d;
#if HWY_TARGET <= HWY_AVX3_DL
  (void)tag;
  // kMask[i] = 0xFF >> i
  alignas(16) static constexpr uint8_t kMasks[16] = {
      0xFF, 0x7F, 0x3F, 0x1F, 0x0F, 0x07, 0x03, 0x01, 0x00};
  // kShl[i] = 1 << i
  alignas(16) static constexpr uint8_t kShl[16] = {1,    2,    4,    8,   0x10,
                                                   0x20, 0x40, 0x80, 0x00};
  v = And(v, TableLookupBytes(Load(d, kMasks), bits));
  const VFromD<decltype(d)> mul = TableLookupBytes(Load(d, kShl), bits);
  return VFromD<decltype(d)>{_mm_gf2p8mul_epi8(v.raw, mul.raw)};
#else
  const Repartition<uint16_t, decltype(d)> dw;
  using VW = VFromD<decltype(dw)>;
  const VW mask = Set(dw, 0x00FF);
  const VW vw = BitCast(dw, v);
  const VW bits16 = BitCast(dw, bits);
  const VW evens = Shl(tag, And(vw, mask), And(bits16, mask));
  // Shift odd lanes in-place
  const VW odds = Shl(tag, vw, ShiftRight<8>(bits16));
  return BitCast(d, IfVecThenElse(Set(dw, 0xFF00), odds, evens));
#endif
}
HWY_API Vec128<uint8_t, 1> Shl(hwy::UnsignedTag /*tag*/, Vec128<uint8_t, 1> v,
                               Vec128<uint8_t, 1> bits) {
  const Full16<uint16_t> d16;
  const Vec16<uint16_t> bits16{bits.raw};
  const Vec16<uint16_t> bits8 = And(bits16, Set(d16, 0xFF));
  return Vec128<uint8_t, 1>{_mm_sll_epi16(v.raw, bits8.raw)};
}

template <size_t N>
HWY_API Vec128<uint32_t, N> Shl(hwy::UnsignedTag /*tag*/, Vec128<uint32_t, N> v,
                                Vec128<uint32_t, N> bits) {
#if HWY_TARGET >= HWY_SSE4
  return v * Pow2(bits);
#else
  return Vec128<uint32_t, N>{_mm_sllv_epi32(v.raw, bits.raw)};
#endif
}
HWY_API Vec128<uint32_t, 1> Shl(hwy::UnsignedTag /*tag*/, Vec128<uint32_t, 1> v,
                                const Vec128<uint32_t, 1> bits) {
  return Vec128<uint32_t, 1>{_mm_sll_epi32(v.raw, bits.raw)};
}

HWY_API Vec128<uint64_t> Shl(hwy::UnsignedTag /*tag*/, Vec128<uint64_t> v,
                             Vec128<uint64_t> bits) {
#if HWY_TARGET >= HWY_SSE4
  const DFromV<decltype(v)> d;
  // Individual shifts and combine
  const Vec128<uint64_t> out0{_mm_sll_epi64(v.raw, bits.raw)};
  const __m128i bits1 = _mm_unpackhi_epi64(bits.raw, bits.raw);
  const Vec128<uint64_t> out1{_mm_sll_epi64(v.raw, bits1)};
  return ConcatUpperLower(d, out1, out0);
#else
  return Vec128<uint64_t>{_mm_sllv_epi64(v.raw, bits.raw)};
#endif
}
HWY_API Vec64<uint64_t> Shl(hwy::UnsignedTag /*tag*/, Vec64<uint64_t> v,
                            Vec64<uint64_t> bits) {
  return Vec64<uint64_t>{_mm_sll_epi64(v.raw, bits.raw)};
}

// Signed left shift is the same as unsigned.
template <typename T, size_t N>
HWY_API Vec128<T, N> Shl(hwy::SignedTag /*tag*/, Vec128<T, N> v,
                         Vec128<T, N> bits) {
  const DFromV<decltype(v)> di;
  const RebindToUnsigned<decltype(di)> du;
  return BitCast(di,
                 Shl(hwy::UnsignedTag(), BitCast(du, v), BitCast(du, bits)));
}

}  // namespace detail

template <typename T, size_t N>
HWY_API Vec128<T, N> operator<<(Vec128<T, N> v, Vec128<T, N> bits) {
  return detail::Shl(hwy::TypeTag<T>(), v, bits);
}

// ------------------------------ Shr (mul, mask, BroadcastSignBit)

// Use AVX2+ variable shifts except for SSSE3/SSE4 or 16-bit. There, we use
// widening multiplication by powers of two obtained by loading float exponents,
// followed by a constant right-shift. This is still faster than a scalar or
// bit-test approach: https://gcc.godbolt.org/z/9G7Y9v.

template <size_t N>
HWY_API Vec128<uint16_t, N> operator>>(Vec128<uint16_t, N> in,
                                       const Vec128<uint16_t, N> bits) {
#if HWY_TARGET <= HWY_AVX3
  return Vec128<uint16_t, N>{_mm_srlv_epi16(in.raw, bits.raw)};
#else
  const DFromV<decltype(in)> d;
  // For bits=0, we cannot mul by 2^16, so fix the result later.
  const auto out = MulHigh(in, detail::Pow2(Set(d, 16) - bits));
  // Replace output with input where bits == 0.
  return IfThenElse(bits == Zero(d), in, out);
#endif
}
HWY_API Vec128<uint16_t, 1> operator>>(const Vec128<uint16_t, 1> in,
                                       const Vec128<uint16_t, 1> bits) {
  return Vec128<uint16_t, 1>{_mm_srl_epi16(in.raw, bits.raw)};
}

// 8-bit uses 16-bit shifts.
template <size_t N>
HWY_API Vec128<uint8_t, N> operator>>(Vec128<uint8_t, N> in,
                                      const Vec128<uint8_t, N> bits) {
  const DFromV<decltype(in)> d;
  const Repartition<uint16_t, decltype(d)> dw;
  using VW = VFromD<decltype(dw)>;
  const VW mask = Set(dw, 0x00FF);
  const VW vw = BitCast(dw, in);
  const VW bits16 = BitCast(dw, bits);
  const VW evens = And(vw, mask) >> And(bits16, mask);
  // Shift odd lanes in-place
  const VW odds = vw >> ShiftRight<8>(bits16);
  return OddEven(BitCast(d, odds), BitCast(d, evens));
}
HWY_API Vec128<uint8_t, 1> operator>>(const Vec128<uint8_t, 1> in,
                                      const Vec128<uint8_t, 1> bits) {
  const Full16<uint16_t> d16;
  const Vec16<uint16_t> bits16{bits.raw};
  const Vec16<uint16_t> bits8 = And(bits16, Set(d16, 0xFF));
  return Vec128<uint8_t, 1>{_mm_srl_epi16(in.raw, bits8.raw)};
}

template <size_t N>
HWY_API Vec128<uint32_t, N> operator>>(const Vec128<uint32_t, N> in,
                                       const Vec128<uint32_t, N> bits) {
#if HWY_TARGET >= HWY_SSE4
  // 32x32 -> 64 bit mul, then shift right by 32.
  const DFromV<decltype(in)> d32;
  // Move odd lanes into position for the second mul. Shuffle more gracefully
  // handles N=1 than repartitioning to u64 and shifting 32 bits right.
  const Vec128<uint32_t, N> in31{_mm_shuffle_epi32(in.raw, 0x31)};
  // For bits=0, we cannot mul by 2^32, so fix the result later.
  const auto mul = detail::Pow2(Set(d32, 32) - bits);
  const auto out20 = ShiftRight<32>(MulEven(in, mul));  // z 2 z 0
  const Vec128<uint32_t, N> mul31{_mm_shuffle_epi32(mul.raw, 0x31)};
  // No need to shift right, already in the correct position.
  const auto out31 = BitCast(d32, MulEven(in31, mul31));  // 3 ? 1 ?
  const Vec128<uint32_t, N> out = OddEven(out31, BitCast(d32, out20));
  // Replace output with input where bits == 0.
  return IfThenElse(bits == Zero(d32), in, out);
#else
  return Vec128<uint32_t, N>{_mm_srlv_epi32(in.raw, bits.raw)};
#endif
}
HWY_API Vec128<uint32_t, 1> operator>>(const Vec128<uint32_t, 1> in,
                                       const Vec128<uint32_t, 1> bits) {
  return Vec128<uint32_t, 1>{_mm_srl_epi32(in.raw, bits.raw)};
}

HWY_API Vec128<uint64_t> operator>>(const Vec128<uint64_t> v,
                                    const Vec128<uint64_t> bits) {
#if HWY_TARGET >= HWY_SSE4
  const DFromV<decltype(v)> d;
  // Individual shifts and combine
  const Vec128<uint64_t> out0{_mm_srl_epi64(v.raw, bits.raw)};
  const __m128i bits1 = _mm_unpackhi_epi64(bits.raw, bits.raw);
  const Vec128<uint64_t> out1{_mm_srl_epi64(v.raw, bits1)};
  return ConcatUpperLower(d, out1, out0);
#else
  return Vec128<uint64_t>{_mm_srlv_epi64(v.raw, bits.raw)};
#endif
}
HWY_API Vec64<uint64_t> operator>>(const Vec64<uint64_t> v,
                                   const Vec64<uint64_t> bits) {
  return Vec64<uint64_t>{_mm_srl_epi64(v.raw, bits.raw)};
}

#if HWY_TARGET > HWY_AVX3  // AVX2 or older
namespace detail {

// Also used in x86_256-inl.h.
template <class DI, class V>
HWY_INLINE V SignedShr(const DI di, const V v, const V count_i) {
  const RebindToUnsigned<DI> du;
  const auto count = BitCast(du, count_i);  // same type as value to shift
  // Clear sign and restore afterwards. This is preferable to shifting the MSB
  // downwards because Shr is somewhat more expensive than Shl.
  const auto sign = BroadcastSignBit(v);
  const auto abs = BitCast(du, v ^ sign);  // off by one, but fixed below
  return BitCast(di, abs >> count) ^ sign;
}

}  // namespace detail
#endif  // HWY_TARGET > HWY_AVX3

template <size_t N>
HWY_API Vec128<int16_t, N> operator>>(Vec128<int16_t, N> v,
                                      Vec128<int16_t, N> bits) {
#if HWY_TARGET <= HWY_AVX3
  return Vec128<int16_t, N>{_mm_srav_epi16(v.raw, bits.raw)};
#else
  const DFromV<decltype(v)> d;
  return detail::SignedShr(d, v, bits);
#endif
}
HWY_API Vec128<int16_t, 1> operator>>(Vec128<int16_t, 1> v,
                                      Vec128<int16_t, 1> bits) {
  return Vec128<int16_t, 1>{_mm_sra_epi16(v.raw, bits.raw)};
}

template <size_t N>
HWY_API Vec128<int32_t, N> operator>>(Vec128<int32_t, N> v,
                                      Vec128<int32_t, N> bits) {
#if HWY_TARGET <= HWY_AVX2
  return Vec128<int32_t, N>{_mm_srav_epi32(v.raw, bits.raw)};
#else
  const DFromV<decltype(v)> d;
  return detail::SignedShr(d, v, bits);
#endif
}
HWY_API Vec128<int32_t, 1> operator>>(Vec128<int32_t, 1> v,
                                      Vec128<int32_t, 1> bits) {
  return Vec128<int32_t, 1>{_mm_sra_epi32(v.raw, bits.raw)};
}

template <size_t N>
HWY_API Vec128<int64_t, N> operator>>(Vec128<int64_t, N> v,
                                      Vec128<int64_t, N> bits) {
#if HWY_TARGET <= HWY_AVX3
  return Vec128<int64_t, N>{_mm_srav_epi64(v.raw, bits.raw)};
#else
  const DFromV<decltype(v)> d;
  return detail::SignedShr(d, v, bits);
#endif
}

// ------------------------------ MulEven/Odd 64x64 (UpperHalf)

HWY_INLINE Vec128<uint64_t> MulEven(Vec128<uint64_t> a, Vec128<uint64_t> b) {
  const DFromV<decltype(a)> d;
  alignas(16) uint64_t mul[2];
  mul[0] = Mul128(GetLane(a), GetLane(b), &mul[1]);
  return Load(d, mul);
}

HWY_INLINE Vec128<uint64_t> MulOdd(Vec128<uint64_t> a, Vec128<uint64_t> b) {
  const DFromV<decltype(a)> d;
  const Half<decltype(d)> d2;
  alignas(16) uint64_t mul[2];
  const uint64_t a1 = GetLane(UpperHalf(d2, a));
  const uint64_t b1 = GetLane(UpperHalf(d2, b));
  mul[0] = Mul128(a1, b1, &mul[1]);
  return Load(d, mul);
}

// ------------------------------ ReorderWidenMulAccumulate (MulAdd, ShiftLeft)

// Generic for all vector lengths.
template <class D32, HWY_IF_F32_D(D32),
          class V16 = VFromD<Repartition<bfloat16_t, D32>>>
HWY_API VFromD<D32> ReorderWidenMulAccumulate(D32 df32, V16 a, V16 b,
                                              const VFromD<D32> sum0,
                                              VFromD<D32>& sum1) {
  // TODO(janwas): _mm_dpbf16_ps when available
  const RebindToUnsigned<decltype(df32)> du32;
  // Lane order within sum0/1 is undefined, hence we can avoid the
  // longer-latency lane-crossing PromoteTo. Using shift/and instead of Zip
  // leads to the odd/even order that RearrangeToOddPlusEven prefers.
  using VU32 = VFromD<decltype(du32)>;
  const VU32 odd = Set(du32, 0xFFFF0000u);
  const VU32 ae = ShiftLeft<16>(BitCast(du32, a));
  const VU32 ao = And(BitCast(du32, a), odd);
  const VU32 be = ShiftLeft<16>(BitCast(du32, b));
  const VU32 bo = And(BitCast(du32, b), odd);
  sum1 = MulAdd(BitCast(df32, ao), BitCast(df32, bo), sum1);
  return MulAdd(BitCast(df32, ae), BitCast(df32, be), sum0);
}

// Even if N=1, the input is always at least 2 lanes, hence madd_epi16 is safe.
template <class D32, HWY_IF_I32_D(D32), HWY_IF_V_SIZE_LE_D(D32, 16),
          class V16 = VFromD<RepartitionToNarrow<D32>>>
HWY_API VFromD<D32> ReorderWidenMulAccumulate(D32 /* tag */, V16 a, V16 b,
                                              const VFromD<D32> sum0,
                                              VFromD<D32>& /*sum1*/) {
#if HWY_TARGET <= HWY_AVX3_DL
  return VFromD<D32>{_mm_dpwssd_epi32(sum0.raw, a.raw, b.raw)};
#else
  return sum0 + VFromD<D32>{_mm_madd_epi16(a.raw, b.raw)};
#endif
}

// ------------------------------ RearrangeToOddPlusEven
template <size_t N>
HWY_API Vec128<int32_t, N> RearrangeToOddPlusEven(const Vec128<int32_t, N> sum0,
                                                  Vec128<int32_t, N> /*sum1*/) {
  return sum0;  // invariant already holds
}

template <class VW>
HWY_API VW RearrangeToOddPlusEven(const VW sum0, const VW sum1) {
  return Add(sum0, sum1);
}

// ================================================== CONVERT

// ------------------------------ Promotions (part w/ narrow lanes -> full)

// Unsigned: zero-extend.
template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_U16_D(D)>
HWY_API VFromD<D> PromoteTo(D /* tag */, VFromD<Rebind<uint8_t, D>> v) {
#if HWY_TARGET >= HWY_SSSE3
  const __m128i zero = _mm_setzero_si128();
  return VFromD<D>{_mm_unpacklo_epi8(v.raw, zero)};
#else
  return VFromD<D>{_mm_cvtepu8_epi16(v.raw)};
#endif
}
template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_U32_D(D)>
HWY_API VFromD<D> PromoteTo(D /* tag */, VFromD<Rebind<uint16_t, D>> v) {
#if HWY_TARGET >= HWY_SSSE3
  return VFromD<D>{_mm_unpacklo_epi16(v.raw, _mm_setzero_si128())};
#else
  return VFromD<D>{_mm_cvtepu16_epi32(v.raw)};
#endif
}
template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_U64_D(D)>
HWY_API VFromD<D> PromoteTo(D /* tag */, VFromD<Rebind<uint32_t, D>> v) {
#if HWY_TARGET >= HWY_SSSE3
  return VFromD<D>{_mm_unpacklo_epi32(v.raw, _mm_setzero_si128())};
#else
  return VFromD<D>{_mm_cvtepu32_epi64(v.raw)};
#endif
}
template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_U32_D(D)>
HWY_API VFromD<D> PromoteTo(D /* tag */, VFromD<Rebind<uint8_t, D>> v) {
#if HWY_TARGET >= HWY_SSSE3
  const __m128i zero = _mm_setzero_si128();
  const __m128i u16 = _mm_unpacklo_epi8(v.raw, zero);
  return VFromD<D>{_mm_unpacklo_epi16(u16, zero)};
#else
  return VFromD<D>{_mm_cvtepu8_epi32(v.raw)};
#endif
}
template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_U64_D(D)>
HWY_API VFromD<D> PromoteTo(D d, VFromD<Rebind<uint8_t, D>> v) {
#if HWY_TARGET > HWY_SSSE3
  const Rebind<uint32_t, decltype(d)> du32;
  return PromoteTo(d, PromoteTo(du32, v));
#elif HWY_TARGET == HWY_SSSE3
  alignas(16) static constexpr int8_t kShuffle[16] = {
      0, -1, -1, -1, -1, -1, -1, -1, 1, -1, -1, -1, -1, -1, -1, -1};
  const Repartition<int8_t, decltype(d)> di8;
  return TableLookupBytesOr0(v, BitCast(d, Load(di8, kShuffle)));
#else
  (void)d;
  return VFromD<D>{_mm_cvtepu8_epi64(v.raw)};
#endif
}
template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_U64_D(D)>
HWY_API VFromD<D> PromoteTo(D d, VFromD<Rebind<uint16_t, D>> v) {
#if HWY_TARGET > HWY_SSSE3
  const Rebind<uint32_t, decltype(d)> du32;
  return PromoteTo(d, PromoteTo(du32, v));
#elif HWY_TARGET == HWY_SSSE3
  alignas(16) static constexpr int8_t kShuffle[16] = {
      0, 1, -1, -1, -1, -1, -1, -1, 2, 3, -1, -1, -1, -1, -1, -1};
  const Repartition<int8_t, decltype(d)> di8;
  return TableLookupBytesOr0(v, BitCast(d, Load(di8, kShuffle)));
#else
  (void)d;
  return VFromD<D>{_mm_cvtepu16_epi64(v.raw)};
#endif
}

// Unsigned to signed: same plus cast.
template <class D, class V, HWY_IF_SIGNED_D(D), HWY_IF_UNSIGNED_V(V),
          HWY_IF_LANES_GT(sizeof(TFromD<D>), sizeof(TFromV<V>)),
          HWY_IF_LANES_D(D, HWY_MAX_LANES_V(V))>
HWY_API VFromD<D> PromoteTo(D di, V v) {
  const RebindToUnsigned<decltype(di)> du;
  return BitCast(di, PromoteTo(du, v));
}

// Signed: replicate sign bit.
template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_I16_D(D)>
HWY_API VFromD<D> PromoteTo(D /* tag */, VFromD<Rebind<int8_t, D>> v) {
#if HWY_TARGET >= HWY_SSSE3
  return ShiftRight<8>(VFromD<D>{_mm_unpacklo_epi8(v.raw, v.raw)});
#else
  return VFromD<D>{_mm_cvtepi8_epi16(v.raw)};
#endif
}
template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_I32_D(D)>
HWY_API VFromD<D> PromoteTo(D /* tag */, VFromD<Rebind<int16_t, D>> v) {
#if HWY_TARGET >= HWY_SSSE3
  return ShiftRight<16>(VFromD<D>{_mm_unpacklo_epi16(v.raw, v.raw)});
#else
  return VFromD<D>{_mm_cvtepi16_epi32(v.raw)};
#endif
}
template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_I64_D(D)>
HWY_API VFromD<D> PromoteTo(D /* tag */, VFromD<Rebind<int32_t, D>> v) {
#if HWY_TARGET >= HWY_SSSE3
  return ShiftRight<32>(VFromD<D>{_mm_unpacklo_epi32(v.raw, v.raw)});
#else
  return VFromD<D>{_mm_cvtepi32_epi64(v.raw)};
#endif
}
template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_I32_D(D)>
HWY_API VFromD<D> PromoteTo(D /* tag */, VFromD<Rebind<int8_t, D>> v) {
#if HWY_TARGET >= HWY_SSSE3
  const __m128i x2 = _mm_unpacklo_epi8(v.raw, v.raw);
  const __m128i x4 = _mm_unpacklo_epi16(x2, x2);
  return ShiftRight<24>(VFromD<D>{x4});
#else
  return VFromD<D>{_mm_cvtepi8_epi32(v.raw)};
#endif
}
template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_I64_D(D)>
HWY_API VFromD<D> PromoteTo(D d, VFromD<Rebind<int8_t, D>> v) {
#if HWY_TARGET >= HWY_SSSE3
  const Repartition<int32_t, decltype(d)> di32;
  const Half<decltype(di32)> dh_i32;
  const VFromD<decltype(di32)> x4{PromoteTo(dh_i32, v).raw};
  const VFromD<decltype(di32)> s4{
      _mm_shufflelo_epi16(x4.raw, _MM_SHUFFLE(3, 3, 1, 1))};
  return ZipLower(d, x4, s4);
#else
  (void)d;
  return VFromD<D>{_mm_cvtepi8_epi64(v.raw)};
#endif
}
template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_I64_D(D)>
HWY_API VFromD<D> PromoteTo(D d, VFromD<Rebind<int16_t, D>> v) {
#if HWY_TARGET >= HWY_SSSE3
  const Repartition<int32_t, decltype(d)> di32;
  const Half<decltype(di32)> dh_i32;
  const VFromD<decltype(di32)> x2{PromoteTo(dh_i32, v).raw};
  const VFromD<decltype(di32)> s2{
      _mm_shufflelo_epi16(x2.raw, _MM_SHUFFLE(3, 3, 1, 1))};
  return ZipLower(d, x2, s2);
#else
  (void)d;
  return VFromD<D>{_mm_cvtepi16_epi64(v.raw)};
#endif
}

// Workaround for origin tracking bug in Clang msan prior to 11.0
// (spurious "uninitialized memory" for TestF16 with "ORIGIN: invalid")
#if HWY_IS_MSAN && (HWY_COMPILER_CLANG != 0 && HWY_COMPILER_CLANG < 1100)
#define HWY_INLINE_F16 HWY_NOINLINE
#else
#define HWY_INLINE_F16 HWY_INLINE
#endif
template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_F32_D(D)>
HWY_INLINE_F16 VFromD<D> PromoteTo(D df32, VFromD<Rebind<float16_t, D>> v) {
#if HWY_TARGET >= HWY_SSE4 || defined(HWY_DISABLE_F16C)
  const RebindToSigned<decltype(df32)> di32;
  const RebindToUnsigned<decltype(df32)> du32;
  // Expand to u32 so we can shift.
  const auto bits16 = PromoteTo(du32, VFromD<Rebind<uint16_t, D>>{v.raw});
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
#else
  (void)df32;
  return VFromD<D>{_mm_cvtph_ps(v.raw)};
#endif
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_F32_D(D)>
HWY_API VFromD<D> PromoteTo(D df32, VFromD<Rebind<bfloat16_t, D>> v) {
  const Rebind<uint16_t, decltype(df32)> du16;
  const RebindToSigned<decltype(df32)> di32;
  return BitCast(df32, ShiftLeft<16>(PromoteTo(di32, BitCast(du16, v))));
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_F64_D(D)>
HWY_API VFromD<D> PromoteTo(D /* tag */, VFromD<Rebind<float, D>> v) {
  return VFromD<D>{_mm_cvtps_pd(v.raw)};
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_F64_D(D)>
HWY_API VFromD<D> PromoteTo(D /* tag */, VFromD<Rebind<int32_t, D>> v) {
  return VFromD<D>{_mm_cvtepi32_pd(v.raw)};
}

// ------------------------------ Demotions (full -> part w/ narrow lanes)

template <class D, HWY_IF_V_SIZE_LE_D(D, 8), HWY_IF_I16_D(D)>
HWY_API VFromD<D> DemoteTo(D /* tag */, VFromD<Rebind<int32_t, D>> v) {
  return VFromD<D>{_mm_packs_epi32(v.raw, v.raw)};
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 8), HWY_IF_U16_D(D)>
HWY_API VFromD<D> DemoteTo(D /* tag */, VFromD<Rebind<int32_t, D>> v) {
#if HWY_TARGET >= HWY_SSSE3
  const Rebind<int32_t, D> di32;
  const auto zero_if_neg = AndNot(ShiftRight<31>(v), v);
  const auto too_big = VecFromMask(di32, Gt(v, Set(di32, 0xFFFF)));
  const auto clamped = Or(zero_if_neg, too_big);
#if HWY_TARGET == HWY_SSE2
  const Rebind<uint16_t, decltype(di32)> du16;
  const RebindToSigned<decltype(du16)> di16;
  return BitCast(du16, DemoteTo(di16, ShiftRight<16>(ShiftLeft<16>(clamped))));
#else
  const Repartition<uint16_t, decltype(di32)> du16;
  // Lower 2 bytes from each 32-bit lane; same as return type for fewer casts.
  alignas(16) static constexpr uint16_t kLower2Bytes[16] = {
      0x0100, 0x0504, 0x0908, 0x0D0C, 0x8080, 0x8080, 0x8080, 0x8080};
  const auto lo2 = Load(du16, kLower2Bytes);
  return VFromD<D>{TableLookupBytes(BitCast(du16, clamped), lo2).raw};
#endif
#else
  return VFromD<D>{_mm_packus_epi32(v.raw, v.raw)};
#endif
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 8), HWY_IF_U16_D(D)>
HWY_API VFromD<D> DemoteTo(D du16, VFromD<Rebind<uint32_t, D>> v) {
  const DFromV<decltype(v)> du32;
  const RebindToSigned<decltype(du32)> di32;
#if HWY_TARGET >= HWY_SSSE3
  const auto too_big =
      VecFromMask(di32, Gt(BitCast(di32, ShiftRight<16>(v)), Zero(di32)));
  const auto clamped = Or(BitCast(di32, v), too_big);
#if HWY_TARGET == HWY_SSE2
  const RebindToSigned<decltype(du16)> di16;
  return BitCast(du16, DemoteTo(di16, ShiftRight<16>(ShiftLeft<16>(clamped))));
#else
  (void)du16;
  const Repartition<uint16_t, decltype(di32)> du16_full;
  // Lower 2 bytes from each 32-bit lane; same as return type for fewer casts.
  alignas(16) static constexpr uint16_t kLower2Bytes[16] = {
      0x0100, 0x0504, 0x0908, 0x0D0C, 0x8080, 0x8080, 0x8080, 0x8080};
  const auto lo2 = Load(du16_full, kLower2Bytes);
  return VFromD<D>{TableLookupBytes(BitCast(du16_full, clamped), lo2).raw};
#endif
#else
  return DemoteTo(du16, BitCast(di32, Min(v, Set(du32, 0x7FFFFFFF))));
#endif
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 4), HWY_IF_U8_D(D)>
HWY_API VFromD<D> DemoteTo(D /* tag */, VFromD<Rebind<int32_t, D>> v) {
  const __m128i i16 = _mm_packs_epi32(v.raw, v.raw);
  return VFromD<D>{_mm_packus_epi16(i16, i16)};
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 8), HWY_IF_U8_D(D)>
HWY_API VFromD<D> DemoteTo(D /* tag */, VFromD<Rebind<int16_t, D>> v) {
  return VFromD<D>{_mm_packus_epi16(v.raw, v.raw)};
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 4), HWY_IF_I8_D(D)>
HWY_API VFromD<D> DemoteTo(D /* tag */, VFromD<Rebind<int32_t, D>> v) {
  const __m128i i16 = _mm_packs_epi32(v.raw, v.raw);
  return VFromD<D>{_mm_packs_epi16(i16, i16)};
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 8), HWY_IF_I8_D(D)>
HWY_API VFromD<D> DemoteTo(D /* tag */, VFromD<Rebind<int16_t, D>> v) {
  return VFromD<D>{_mm_packs_epi16(v.raw, v.raw)};
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 4), HWY_IF_U8_D(D)>
HWY_API VFromD<D> DemoteTo(D du8, VFromD<Rebind<uint32_t, D>> v) {
#if HWY_TARGET <= HWY_AVX3
  // NOTE: _mm_cvtusepi32_epi8 is a saturated conversion of 32-bit unsigned
  // integers to 8-bit unsigned integers
  (void)du8;
  return VFromD<D>{_mm_cvtusepi32_epi8(v.raw)};
#else
  const DFromV<decltype(v)> du32;
  const RebindToSigned<decltype(du32)> di32;
  const auto max_i32 = Set(du32, 0x7FFFFFFFu);

#if HWY_TARGET >= HWY_SSSE3
  // On SSE2/SSSE3, clamp u32 values to an i32 using the u8 Min operation
  // as SSE2/SSSE3 can do an u8 Min operation in a single instruction.

  // The u8 Min operation below leaves the lower 24 bits of each 32-bit
  // lane unchanged.

  // The u8 Min operation below will leave any values that are less than or
  // equal to 0x7FFFFFFF unchanged.

  // For values that are greater than or equal to 0x80000000, the u8 Min
  // operation below will force the upper 8 bits to 0x7F and leave the lower
  // 24 bits unchanged.

  // An u8 Min operation is okay here as any clamped value that is greater than
  // or equal to 0x80000000 will be clamped to a value between 0x7F000000 and
  // 0x7FFFFFFF through the u8 Min operation below, which will then be converted
  // to 0xFF through the i32->u8 demotion.
  const Repartition<uint8_t, decltype(du32)> du32_as_du8;
  const auto clamped = BitCast(
      di32, Min(BitCast(du32_as_du8, v), BitCast(du32_as_du8, max_i32)));
#else
  const auto clamped = BitCast(di32, Min(v, max_i32));
#endif

  return DemoteTo(du8, clamped);
#endif
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 8), HWY_IF_U8_D(D)>
HWY_API VFromD<D> DemoteTo(D du8, VFromD<Rebind<uint16_t, D>> v) {
  const DFromV<decltype(v)> du16;
  const RebindToSigned<decltype(du16)> di16;
  const auto max_i16 = Set(du16, 0x7FFF);

#if HWY_TARGET >= HWY_SSSE3
  // On SSE2/SSSE3, clamp u16 values to an i16 using the u8 Min operation
  // as SSE2/SSSE3 can do an u8 Min operation in a single instruction.

  // The u8 Min operation below leaves the lower 8 bits of each 16-bit
  // lane unchanged.

  // The u8 Min operation below will leave any values that are less than or
  // equal to 0x7FFF unchanged.

  // For values that are greater than or equal to 0x8000, the u8 Min
  // operation below will force the upper 8 bits to 0x7F and leave the lower
  // 8 bits unchanged.

  // An u8 Min operation is okay here as any clamped value that is greater than
  // or equal to 0x8000 will be clamped to a value between 0x7F00 and
  // 0x7FFF through the u8 Min operation below, which will then be converted
  // to 0xFF through the i16->u8 demotion.
  const Repartition<uint8_t, decltype(du16)> du16_as_du8;
  const auto clamped = BitCast(
      di16, Min(BitCast(du16_as_du8, v), BitCast(du16_as_du8, max_i16)));
#else
  const auto clamped = BitCast(di16, Min(v, max_i16));
#endif

  return DemoteTo(du8, clamped);
}

// Work around MSVC warning for _mm_cvtps_ph (8 is actually a valid immediate).
// clang-cl requires a non-empty string, so we 'ignore' the irrelevant -Wmain.
HWY_DIAGNOSTICS(push)
HWY_DIAGNOSTICS_OFF(disable : 4556, ignored "-Wmain")

template <class D, HWY_IF_V_SIZE_LE_D(D, 8), HWY_IF_F16_D(D)>
HWY_API VFromD<D> DemoteTo(D df16, VFromD<Rebind<float, D>> v) {
#if HWY_TARGET >= HWY_SSE4 || defined(HWY_DISABLE_F16C)
  const RebindToUnsigned<decltype(df16)> du16;
  const Rebind<uint32_t, decltype(df16)> du;
  const RebindToSigned<decltype(du)> di;
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
  return BitCast(df16, DemoteTo(du16, bits16));
#else
  (void)df16;
  return VFromD<D>{_mm_cvtps_ph(v.raw, _MM_FROUND_NO_EXC)};
#endif
}

HWY_DIAGNOSTICS(pop)

template <class D, HWY_IF_V_SIZE_LE_D(D, 8), HWY_IF_BF16_D(D)>
HWY_API VFromD<D> DemoteTo(D dbf16, VFromD<Rebind<float, D>> v) {
  // TODO(janwas): _mm_cvtneps_pbh once we have avx512bf16.
  const Rebind<int32_t, decltype(dbf16)> di32;
  const Rebind<uint32_t, decltype(dbf16)> du32;  // for logical shift right
  const Rebind<uint16_t, decltype(dbf16)> du16;
  const auto bits_in_32 = BitCast(di32, ShiftRight<16>(BitCast(du32, v)));
  return BitCast(dbf16, DemoteTo(du16, bits_in_32));
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_BF16_D(D),
          class V32 = VFromD<Repartition<float, D>>>
HWY_API VFromD<D> ReorderDemote2To(D dbf16, V32 a, V32 b) {
  // TODO(janwas): _mm_cvtne2ps_pbh once we have avx512bf16.
  const RebindToUnsigned<decltype(dbf16)> du16;
  const Repartition<uint32_t, decltype(dbf16)> du32;
  const VFromD<decltype(du32)> b_in_even = ShiftRight<16>(BitCast(du32, b));
  return BitCast(dbf16, OddEven(BitCast(du16, a), BitCast(du16, b_in_even)));
}

// Specializations for partial vectors because packs_epi32 sets lanes above 2*N.
template <class D, HWY_IF_I16_D(D)>
HWY_API Vec32<int16_t> ReorderDemote2To(D dn, Vec32<int32_t> a,
                                        Vec32<int32_t> b) {
  const DFromV<decltype(a)> d;
  const Twice<decltype(d)> dt;
  return DemoteTo(dn, Combine(dt, b, a));
}
template <class D, HWY_IF_I16_D(D)>
HWY_API Vec64<int16_t> ReorderDemote2To(D /* tag */, Vec64<int32_t> a,
                                        Vec64<int32_t> b) {
  return Vec64<int16_t>{_mm_shuffle_epi32(_mm_packs_epi32(a.raw, b.raw),
                                          _MM_SHUFFLE(2, 0, 2, 0))};
}
template <class D, HWY_IF_I16_D(D)>
HWY_API Vec128<int16_t> ReorderDemote2To(D /* tag */, Vec128<int32_t> a,
                                         Vec128<int32_t> b) {
  return Vec128<int16_t>{_mm_packs_epi32(a.raw, b.raw)};
}

template <class D, HWY_IF_U16_D(D)>
HWY_API Vec32<uint16_t> ReorderDemote2To(D dn, Vec32<int32_t> a,
                                         Vec32<int32_t> b) {
  const DFromV<decltype(a)> d;
  const Twice<decltype(d)> dt;
  return DemoteTo(dn, Combine(dt, b, a));
}
template <class D, HWY_IF_U16_D(D)>
HWY_API Vec64<uint16_t> ReorderDemote2To(D dn, Vec64<int32_t> a,
                                         Vec64<int32_t> b) {
#if HWY_TARGET >= HWY_SSSE3
  const DFromV<decltype(a)> d;
  const Twice<decltype(d)> dt;
  return DemoteTo(dn, Combine(dt, b, a));
#else
  (void)dn;
  return Vec64<uint16_t>{_mm_shuffle_epi32(_mm_packus_epi32(a.raw, b.raw),
                                           _MM_SHUFFLE(2, 0, 2, 0))};
#endif
}
template <class D, HWY_IF_U16_D(D)>
HWY_API Vec128<uint16_t> ReorderDemote2To(D dn, Vec128<int32_t> a,
                                          Vec128<int32_t> b) {
#if HWY_TARGET >= HWY_SSSE3
  const Half<decltype(dn)> dnh;
  const auto u16_a = DemoteTo(dnh, a);
  const auto u16_b = DemoteTo(dnh, b);
  return Combine(dn, u16_b, u16_a);
#else
  (void)dn;
  return Vec128<uint16_t>{_mm_packus_epi32(a.raw, b.raw)};
#endif
}

template <class D, HWY_IF_U16_D(D)>
HWY_API VFromD<D> ReorderDemote2To(D dn, Vec128<uint32_t> a,
                                   Vec128<uint32_t> b) {
  const DFromV<decltype(a)> du32;
  const RebindToSigned<decltype(du32)> di32;
  const auto max_i32 = Set(du32, 0x7FFFFFFFu);

#if HWY_TARGET >= HWY_SSSE3
  const Repartition<uint8_t, decltype(du32)> du32_as_du8;
  // On SSE2/SSSE3, clamp a and b using u8 Min operation
  const auto clamped_a = BitCast(
      di32, Min(BitCast(du32_as_du8, a), BitCast(du32_as_du8, max_i32)));
  const auto clamped_b = BitCast(
      di32, Min(BitCast(du32_as_du8, b), BitCast(du32_as_du8, max_i32)));
#else
  const auto clamped_a = BitCast(di32, Min(a, max_i32));
  const auto clamped_b = BitCast(di32, Min(b, max_i32));
#endif

  return ReorderDemote2To(dn, clamped_a, clamped_b);
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 8), HWY_IF_U16_D(D)>
HWY_API VFromD<D> ReorderDemote2To(D dn, VFromD<Repartition<uint32_t, D>> a,
                                   VFromD<Repartition<uint32_t, D>> b) {
  const DFromV<decltype(a)> d;
  const Twice<decltype(d)> dt;
  return DemoteTo(dn, Combine(dt, b, a));
}

// Specializations for partial vectors because packs_epi32 sets lanes above 2*N.
template <class D, HWY_IF_V_SIZE_LE_D(D, 4), HWY_IF_I8_D(D)>
HWY_API VFromD<D> ReorderDemote2To(D dn, VFromD<Repartition<int16_t, D>> a,
                                   VFromD<Repartition<int16_t, D>> b) {
  const DFromV<decltype(a)> d;
  const Twice<decltype(d)> dt;
  return DemoteTo(dn, Combine(dt, b, a));
}
template <class D, HWY_IF_I8_D(D)>
HWY_API Vec64<int8_t> ReorderDemote2To(D /* tag */, Vec64<int16_t> a,
                                       Vec64<int16_t> b) {
  return Vec64<int8_t>{_mm_shuffle_epi32(_mm_packs_epi16(a.raw, b.raw),
                                         _MM_SHUFFLE(2, 0, 2, 0))};
}
template <class D, HWY_IF_I8_D(D)>
HWY_API Vec128<int8_t> ReorderDemote2To(D /* tag */, Vec128<int16_t> a,
                                        Vec128<int16_t> b) {
  return Vec128<int8_t>{_mm_packs_epi16(a.raw, b.raw)};
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 4), HWY_IF_U8_D(D)>
HWY_API VFromD<D> ReorderDemote2To(D dn, VFromD<Repartition<int16_t, D>> a,
                                   VFromD<Repartition<int16_t, D>> b) {
  const DFromV<decltype(a)> d;
  const Twice<decltype(d)> dt;
  return DemoteTo(dn, Combine(dt, b, a));
}
template <class D, HWY_IF_U8_D(D)>
HWY_API Vec64<uint8_t> ReorderDemote2To(D /* tag */, Vec64<int16_t> a,
                                        Vec64<int16_t> b) {
  return Vec64<uint8_t>{_mm_shuffle_epi32(_mm_packus_epi16(a.raw, b.raw),
                                          _MM_SHUFFLE(2, 0, 2, 0))};
}
template <class D, HWY_IF_U8_D(D)>
HWY_API Vec128<uint8_t> ReorderDemote2To(D /* tag */, Vec128<int16_t> a,
                                         Vec128<int16_t> b) {
  return Vec128<uint8_t>{_mm_packus_epi16(a.raw, b.raw)};
}

template <class D, HWY_IF_U8_D(D)>
HWY_API VFromD<D> ReorderDemote2To(D dn, Vec128<uint16_t> a,
                                   Vec128<uint16_t> b) {
  const DFromV<decltype(a)> du16;
  const RebindToSigned<decltype(du16)> di16;
  const auto max_i16 = Set(du16, 0x7FFFu);

#if HWY_TARGET >= HWY_SSSE3
  const Repartition<uint8_t, decltype(du16)> du16_as_du8;
  // On SSE2/SSSE3, clamp a and b using u8 Min operation
  const auto clamped_a = BitCast(
      di16, Min(BitCast(du16_as_du8, a), BitCast(du16_as_du8, max_i16)));
  const auto clamped_b = BitCast(
      di16, Min(BitCast(du16_as_du8, b), BitCast(du16_as_du8, max_i16)));
#else
  const auto clamped_a = BitCast(di16, Min(a, max_i16));
  const auto clamped_b = BitCast(di16, Min(b, max_i16));
#endif

  return ReorderDemote2To(dn, clamped_a, clamped_b);
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 8), HWY_IF_U8_D(D)>
HWY_API VFromD<D> ReorderDemote2To(D dn, VFromD<Repartition<uint16_t, D>> a,
                                   VFromD<Repartition<uint16_t, D>> b) {
  const DFromV<decltype(a)> d;
  const Twice<decltype(d)> dt;
  return DemoteTo(dn, Combine(dt, b, a));
}

template <class D, HWY_IF_NOT_FLOAT_NOR_SPECIAL(TFromD<D>),
          HWY_IF_V_SIZE_LE_D(D, 16), class V, HWY_IF_NOT_FLOAT_NOR_SPECIAL_V(V),
          HWY_IF_T_SIZE_V(V, sizeof(TFromD<D>) * 2),
          HWY_IF_LANES_D(D, HWY_MAX_LANES_D(DFromV<V>) * 2)>
HWY_API VFromD<D> OrderedDemote2To(D d, V a, V b) {
  return ReorderDemote2To(d, a, b);
}

template <class D, HWY_IF_BF16_D(D), class V32 = VFromD<Repartition<float, D>>>
HWY_API VFromD<D> OrderedDemote2To(D dbf16, V32 a, V32 b) {
  const RebindToUnsigned<decltype(dbf16)> du16;
  return BitCast(dbf16, ConcatOdd(du16, BitCast(du16, b), BitCast(du16, a)));
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 8), HWY_IF_F32_D(D)>
HWY_API VFromD<D> DemoteTo(D /* tag */, VFromD<Rebind<double, D>> v) {
  return VFromD<D>{_mm_cvtpd_ps(v.raw)};
}

namespace detail {

// For well-defined float->int demotion in all x86_*-inl.h.
template <class D>
HWY_INLINE VFromD<D> ClampF64ToI32Max(D d, VFromD<D> v) {
  // The max can be exactly represented in binary64, so clamping beforehand
  // prevents x86 conversion from raising an exception and returning 80..00.
  return Min(v, Set(d, 2147483647.0));
}

// For ConvertTo float->int of same size, clamping before conversion would
// change the result because the max integer value is not exactly representable.
// Instead detect the overflow result after conversion and fix it.
template <class DI, class DF = RebindToFloat<DI>>
HWY_INLINE VFromD<DI> FixConversionOverflow(
    DI di, VFromD<DF> original, decltype(Zero(DI()).raw) converted_raw) {
  // Combinations of original and output sign:
  //   --: normal <0 or -huge_val to 80..00: OK
  //   -+: -0 to 0                         : OK
  //   +-: +huge_val to 80..00             : xor with FF..FF to get 7F..FF
  //   ++: normal >0                       : OK
  const VFromD<DI> converted{converted_raw};
  const VFromD<DI> sign_wrong = AndNot(BitCast(di, original), converted);
#if HWY_COMPILER_GCC_ACTUAL
  // Critical GCC 11 compiler bug (possibly also GCC 10): omits the Xor; also
  // Add() if using that instead. Work around with one more instruction.
  const RebindToUnsigned<DI> du;
  const VFromD<DI> mask = BroadcastSignBit(sign_wrong);
  const VFromD<DI> max = BitCast(di, ShiftRight<1>(BitCast(du, mask)));
  return IfVecThenElse(mask, max, converted);
#else
  return Xor(converted, BroadcastSignBit(sign_wrong));
#endif
}

}  // namespace detail

template <class D, HWY_IF_V_SIZE_LE_D(D, 8), HWY_IF_I32_D(D),
          class DF = Rebind<double, D>>
HWY_API VFromD<D> DemoteTo(D /* tag */, VFromD<DF> v) {
  const VFromD<DF> clamped = detail::ClampF64ToI32Max(DF(), v);
  return VFromD<D>{_mm_cvttpd_epi32(clamped.raw)};
}

// For already range-limited input [0, 255].
template <size_t N>
HWY_API Vec128<uint8_t, N> U8FromU32(const Vec128<uint32_t, N> v) {
#if HWY_TARGET == HWY_SSE2
  const RebindToSigned<DFromV<decltype(v)>> di32;
  const Rebind<uint8_t, decltype(di32)> du8;
  return DemoteTo(du8, BitCast(di32, v));
#else
  const DFromV<decltype(v)> d32;
  const Repartition<uint8_t, decltype(d32)> d8;
  alignas(16) static constexpr uint32_t k8From32[4] = {
      0x0C080400u, 0x0C080400u, 0x0C080400u, 0x0C080400u};
  // Also replicate bytes into all 32 bit lanes for safety.
  const auto quad = TableLookupBytes(v, Load(d32, k8From32));
  return LowerHalf(LowerHalf(BitCast(d8, quad)));
#endif
}

// ------------------------------ MulFixedPoint15

#if HWY_TARGET == HWY_SSE2
HWY_API Vec128<int16_t> MulFixedPoint15(const Vec128<int16_t> a,
                                        const Vec128<int16_t> b) {
  const DFromV<decltype(a)> d;
  const Repartition<int32_t, decltype(d)> di32;

  auto lo_product = a * b;
  auto hi_product = MulHigh(a, b);

  const VFromD<decltype(di32)> i32_product_lo{
      _mm_unpacklo_epi16(lo_product.raw, hi_product.raw)};
  const VFromD<decltype(di32)> i32_product_hi{
      _mm_unpackhi_epi16(lo_product.raw, hi_product.raw)};

  const auto round_up_incr = Set(di32, 0x4000);
  return ReorderDemote2To(d, ShiftRight<15>(i32_product_lo + round_up_incr),
                          ShiftRight<15>(i32_product_hi + round_up_incr));
}

template <size_t N, HWY_IF_V_SIZE_LE(int16_t, N, 8)>
HWY_API Vec128<int16_t, N> MulFixedPoint15(const Vec128<int16_t, N> a,
                                           const Vec128<int16_t, N> b) {
  const DFromV<decltype(a)> d;
  const Rebind<int32_t, decltype(d)> di32;

  const auto lo_product = a * b;
  const auto hi_product = MulHigh(a, b);
  const VFromD<decltype(di32)> i32_product{
      _mm_unpacklo_epi16(lo_product.raw, hi_product.raw)};

  return DemoteTo(d, ShiftRight<15>(i32_product + Set(di32, 0x4000)));
}
#else
template <size_t N>
HWY_API Vec128<int16_t, N> MulFixedPoint15(const Vec128<int16_t, N> a,
                                           const Vec128<int16_t, N> b) {
  return Vec128<int16_t, N>{_mm_mulhrs_epi16(a.raw, b.raw)};
}
#endif

// ------------------------------ Truncations

template <typename From, class DTo, HWY_IF_LANES_D(DTo, 1)>
HWY_API VFromD<DTo> TruncateTo(DTo /* tag */, Vec128<From, 1> v) {
  // BitCast requires the same size; DTo might be u8x1 and v u16x1.
  const Repartition<TFromD<DTo>, DFromV<decltype(v)>> dto;
  return VFromD<DTo>{BitCast(dto, v).raw};
}

template <class D, HWY_IF_U8_D(D)>
HWY_API Vec16<uint8_t> TruncateTo(D d, Vec128<uint64_t> v) {
#if HWY_TARGET == HWY_SSE2
  const Vec128<uint8_t, 1> lo{v.raw};
  const Vec128<uint8_t, 1> hi{_mm_unpackhi_epi64(v.raw, v.raw)};
  return Combine(d, hi, lo);
#else
  const Repartition<uint8_t, DFromV<decltype(v)>> d8;
  (void)d;
  alignas(16) static constexpr uint8_t kIdx[16] = {0, 8, 0, 8, 0, 8, 0, 8,
                                                   0, 8, 0, 8, 0, 8, 0, 8};
  const Vec128<uint8_t> v8 = TableLookupBytes(v, Load(d8, kIdx));
  return LowerHalf(LowerHalf(LowerHalf(v8)));
#endif
}

template <class D, HWY_IF_U16_D(D)>
HWY_API Vec32<uint16_t> TruncateTo(D d, Vec128<uint64_t> v) {
#if HWY_TARGET == HWY_SSE2
  const Vec128<uint16_t, 1> lo{v.raw};
  const Vec128<uint16_t, 1> hi{_mm_unpackhi_epi64(v.raw, v.raw)};
  return Combine(d, hi, lo);
#else
  (void)d;
  const Repartition<uint16_t, DFromV<decltype(v)>> d16;
  alignas(16) static constexpr uint16_t kIdx[8] = {
      0x100u, 0x908u, 0x100u, 0x908u, 0x100u, 0x908u, 0x100u, 0x908u};
  const Vec128<uint16_t> v16 = TableLookupBytes(v, Load(d16, kIdx));
  return LowerHalf(LowerHalf(v16));
#endif
}

template <class D, HWY_IF_U32_D(D)>
HWY_API Vec64<uint32_t> TruncateTo(D /* tag */, Vec128<uint64_t> v) {
  return Vec64<uint32_t>{_mm_shuffle_epi32(v.raw, 0x88)};
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 4), HWY_IF_U8_D(D)>
HWY_API VFromD<D> TruncateTo(D /* tag */, VFromD<Rebind<uint32_t, D>> v) {
  const DFromV<decltype(v)> du32;
#if HWY_TARGET == HWY_SSE2
  const RebindToSigned<decltype(du32)> di32;
  const Rebind<uint8_t, decltype(di32)> du8;
  return DemoteTo(du8, BitCast(di32, ShiftRight<24>(ShiftLeft<24>(v))));
#else
  const Repartition<uint8_t, decltype(du32)> d;
  alignas(16) static constexpr uint8_t kIdx[16] = {
      0x0u, 0x4u, 0x8u, 0xCu, 0x0u, 0x4u, 0x8u, 0xCu,
      0x0u, 0x4u, 0x8u, 0xCu, 0x0u, 0x4u, 0x8u, 0xCu};
  return LowerHalf(LowerHalf(TableLookupBytes(v, Load(d, kIdx))));
#endif
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 8), HWY_IF_U16_D(D)>
HWY_API VFromD<D> TruncateTo(D /* tag */, VFromD<Rebind<uint32_t, D>> v) {
  const DFromV<decltype(v)> du32;
#if HWY_TARGET == HWY_SSE2
  const RebindToSigned<decltype(du32)> di32;
  const Rebind<uint16_t, decltype(di32)> du16;
  const RebindToSigned<decltype(du16)> di16;
  return BitCast(
      du16, DemoteTo(di16, ShiftRight<16>(BitCast(di32, ShiftLeft<16>(v)))));
#else
  const Repartition<uint16_t, decltype(du32)> d;
  return LowerHalf(ConcatEven(d, BitCast(d, v), BitCast(d, v)));
#endif
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 8), HWY_IF_U8_D(D)>
HWY_API VFromD<D> TruncateTo(D /* tag */, VFromD<Rebind<uint16_t, D>> v) {
  const DFromV<decltype(v)> du16;
#if HWY_TARGET == HWY_SSE2
  const RebindToSigned<decltype(du16)> di16;
  const Rebind<uint8_t, decltype(di16)> du8;
  const RebindToSigned<decltype(du8)> di8;
  return BitCast(du8,
                 DemoteTo(di8, ShiftRight<8>(BitCast(di16, ShiftLeft<8>(v)))));
#else
  const Repartition<uint8_t, decltype(du16)> d;
  return LowerHalf(ConcatEven(d, BitCast(d, v), BitCast(d, v)));
#endif
}

// ------------------------------ Demotions to/from i64

#if HWY_TARGET <= HWY_AVX3
template <class D, HWY_IF_V_SIZE_LE_D(D, 8), HWY_IF_I32_D(D)>
HWY_API VFromD<D> DemoteTo(D /* tag */, VFromD<Rebind<int64_t, D>> v) {
  return VFromD<D>{_mm_cvtsepi64_epi32(v.raw)};
}
template <class D, HWY_IF_V_SIZE_LE_D(D, 4), HWY_IF_I16_D(D)>
HWY_API VFromD<D> DemoteTo(D /* tag */, VFromD<Rebind<int64_t, D>> v) {
  return VFromD<D>{_mm_cvtsepi64_epi16(v.raw)};
}
template <class D, HWY_IF_V_SIZE_LE_D(D, 2), HWY_IF_I8_D(D)>
HWY_API VFromD<D> DemoteTo(D /* tag */, VFromD<Rebind<int64_t, D>> v) {
  return VFromD<D>{_mm_cvtsepi64_epi8(v.raw)};
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 8), HWY_IF_U32_D(D)>
HWY_API VFromD<D> DemoteTo(D /* tag */, VFromD<Rebind<int64_t, D>> v) {
  const auto neg_mask = MaskFromVec(v);
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  const __mmask8 non_neg_mask = _knot_mask8(neg_mask.raw);
#else
  const __mmask8 non_neg_mask = static_cast<__mmask8>(~neg_mask.raw);
#endif
  return VFromD<D>{_mm_maskz_cvtusepi64_epi32(non_neg_mask, v.raw)};
}
template <class D, HWY_IF_V_SIZE_LE_D(D, 4), HWY_IF_U16_D(D)>
HWY_API VFromD<D> DemoteTo(D /* tag */, VFromD<Rebind<int64_t, D>> v) {
  const auto neg_mask = MaskFromVec(v);
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  const __mmask8 non_neg_mask = _knot_mask8(neg_mask.raw);
#else
  const __mmask8 non_neg_mask = static_cast<__mmask8>(~neg_mask.raw);
#endif
  return VFromD<D>{_mm_maskz_cvtusepi64_epi16(non_neg_mask, v.raw)};
}
template <class D, HWY_IF_V_SIZE_LE_D(D, 2), HWY_IF_U8_D(D)>
HWY_API VFromD<D> DemoteTo(D /* tag */, VFromD<Rebind<int64_t, D>> v) {
  const auto neg_mask = MaskFromVec(v);
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  const __mmask8 non_neg_mask = _knot_mask8(neg_mask.raw);
#else
  const __mmask8 non_neg_mask = static_cast<__mmask8>(~neg_mask.raw);
#endif
  return VFromD<D>{_mm_maskz_cvtusepi64_epi8(non_neg_mask, v.raw)};
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 8), HWY_IF_U32_D(D)>
HWY_API VFromD<D> DemoteTo(D /* tag */, VFromD<Rebind<uint64_t, D>> v) {
  return VFromD<D>{_mm_cvtusepi64_epi32(v.raw)};
}
template <class D, HWY_IF_V_SIZE_LE_D(D, 4), HWY_IF_U16_D(D)>
HWY_API VFromD<D> DemoteTo(D /* tag */, VFromD<Rebind<uint64_t, D>> v) {
  return VFromD<D>{_mm_cvtusepi64_epi16(v.raw)};
}
template <class D, HWY_IF_V_SIZE_LE_D(D, 2), HWY_IF_U8_D(D)>
HWY_API VFromD<D> DemoteTo(D /* tag */, VFromD<Rebind<uint64_t, D>> v) {
  return VFromD<D>{_mm_cvtusepi64_epi8(v.raw)};
}
#else   // AVX2 or below
namespace detail {
template <class D, HWY_IF_UNSIGNED_D(D)>
HWY_INLINE VFromD<Rebind<uint64_t, D>> DemoteFromU64MaskOutResult(
    D /*dn*/, VFromD<Rebind<uint64_t, D>> v) {
  return v;
}

template <class D, HWY_IF_SIGNED_D(D)>
HWY_INLINE VFromD<Rebind<uint64_t, D>> DemoteFromU64MaskOutResult(
    D /*dn*/, VFromD<Rebind<uint64_t, D>> v) {
  const DFromV<decltype(v)> du64;
  return And(v,
             Set(du64, static_cast<uint64_t>(hwy::HighestValue<TFromD<D>>())));
}

template <class D>
HWY_INLINE VFromD<Rebind<uint64_t, D>> DemoteFromU64Saturate(
    D dn, VFromD<Rebind<uint64_t, D>> v) {
  const Rebind<uint64_t, D> du64;
  const RebindToSigned<decltype(du64)> di64;
  constexpr int kShiftAmt = static_cast<int>(sizeof(TFromD<D>) * 8) -
                            static_cast<int>(hwy::IsSigned<TFromD<D>>());

  const auto too_big = BitCast(
      du64, VecFromMask(
                di64, Gt(BitCast(di64, ShiftRight<kShiftAmt>(v)), Zero(di64))));
  return DemoteFromU64MaskOutResult(dn, Or(v, too_big));
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 16), class V>
HWY_INLINE VFromD<D> ReorderDemote2From64To32Combine(D dn, V a, V b) {
  return ConcatEven(dn, BitCast(dn, b), BitCast(dn, a));
}

}  // namespace detail

template <class D, HWY_IF_T_SIZE_ONE_OF_D(D, (1 << 1) | (1 << 2) | (1 << 4)),
          HWY_IF_SIGNED_D(D)>
HWY_API VFromD<D> DemoteTo(D dn, VFromD<Rebind<int64_t, D>> v) {
  const DFromV<decltype(v)> di64;
  const RebindToUnsigned<decltype(di64)> du64;
  const RebindToUnsigned<decltype(dn)> dn_u;

  // Negative values are saturated by first saturating their bitwise inverse
  // and then inverting the saturation result
  const auto invert_mask = BitCast(du64, BroadcastSignBit(v));
  const auto saturated_vals = Xor(
      invert_mask,
      detail::DemoteFromU64Saturate(dn, Xor(invert_mask, BitCast(du64, v))));
  return BitCast(dn, TruncateTo(dn_u, saturated_vals));
}

template <class D, HWY_IF_T_SIZE_ONE_OF_D(D, (1 << 1) | (1 << 2) | (1 << 4)),
          HWY_IF_UNSIGNED_D(D)>
HWY_API VFromD<D> DemoteTo(D dn, VFromD<Rebind<int64_t, D>> v) {
  const DFromV<decltype(v)> di64;
  const RebindToUnsigned<decltype(di64)> du64;

  const auto non_neg_vals = BitCast(du64, AndNot(BroadcastSignBit(v), v));
  return TruncateTo(dn, detail::DemoteFromU64Saturate(dn, non_neg_vals));
}

template <class D, HWY_IF_T_SIZE_ONE_OF_D(D, (1 << 1) | (1 << 2) | (1 << 4)),
          HWY_IF_UNSIGNED_D(D)>
HWY_API VFromD<D> DemoteTo(D dn, VFromD<Rebind<uint64_t, D>> v) {
  return TruncateTo(dn, detail::DemoteFromU64Saturate(dn, v));
}
#endif  // HWY_TARGET <= HWY_AVX3

template <class D, HWY_IF_V_SIZE_LE_D(D, HWY_MAX_BYTES / 2),
          HWY_IF_T_SIZE_D(D, 4), HWY_IF_NOT_FLOAT_NOR_SPECIAL(TFromD<D>)>
HWY_API VFromD<D> ReorderDemote2To(D dn, VFromD<Repartition<int64_t, D>> a,
                                   VFromD<Repartition<int64_t, D>> b) {
  const DFromV<decltype(a)> d;
  const Twice<decltype(d)> dt;
  return DemoteTo(dn, Combine(dt, b, a));
}

template <class D, HWY_IF_V_SIZE_LE_D(D, HWY_MAX_BYTES / 2), HWY_IF_U32_D(D)>
HWY_API VFromD<D> ReorderDemote2To(D dn, VFromD<Repartition<uint64_t, D>> a,
                                   VFromD<Repartition<uint64_t, D>> b) {
  const DFromV<decltype(a)> d;
  const Twice<decltype(d)> dt;
  return DemoteTo(dn, Combine(dt, b, a));
}

#if HWY_TARGET > HWY_AVX2
template <class D, HWY_IF_V_SIZE_D(D, 16), HWY_IF_I32_D(D)>
HWY_API Vec128<int32_t> ReorderDemote2To(D dn, Vec128<int64_t> a,
                                         Vec128<int64_t> b) {
  const DFromV<decltype(a)> di64;
  const RebindToUnsigned<decltype(di64)> du64;
  const Half<decltype(dn)> dnh;

  // Negative values are saturated by first saturating their bitwise inverse
  // and then inverting the saturation result
  const auto invert_mask_a = BitCast(du64, BroadcastSignBit(a));
  const auto invert_mask_b = BitCast(du64, BroadcastSignBit(b));
  const auto saturated_a = Xor(
      invert_mask_a,
      detail::DemoteFromU64Saturate(dnh, Xor(invert_mask_a, BitCast(du64, a))));
  const auto saturated_b = Xor(
      invert_mask_b,
      detail::DemoteFromU64Saturate(dnh, Xor(invert_mask_b, BitCast(du64, b))));

  return ConcatEven(dn, BitCast(dn, saturated_b), BitCast(dn, saturated_a));
}

template <class D, HWY_IF_V_SIZE_D(D, 16), HWY_IF_U32_D(D)>
HWY_API Vec128<uint32_t> ReorderDemote2To(D dn, Vec128<int64_t> a,
                                          Vec128<int64_t> b) {
  const DFromV<decltype(a)> di64;
  const RebindToUnsigned<decltype(di64)> du64;
  const Half<decltype(dn)> dnh;

  const auto saturated_a = detail::DemoteFromU64Saturate(
      dnh, BitCast(du64, AndNot(BroadcastSignBit(a), a)));
  const auto saturated_b = detail::DemoteFromU64Saturate(
      dnh, BitCast(du64, AndNot(BroadcastSignBit(b), b)));

  return ConcatEven(dn, BitCast(dn, saturated_b), BitCast(dn, saturated_a));
}

template <class D, HWY_IF_V_SIZE_D(D, 16), HWY_IF_U32_D(D)>
HWY_API Vec128<uint32_t> ReorderDemote2To(D dn, Vec128<uint64_t> a,
                                          Vec128<uint64_t> b) {
  const Half<decltype(dn)> dnh;

  const auto saturated_a = detail::DemoteFromU64Saturate(dnh, a);
  const auto saturated_b = detail::DemoteFromU64Saturate(dnh, b);

  return ConcatEven(dn, BitCast(dn, saturated_b), BitCast(dn, saturated_a));
}
#endif  // HWY_TARGET > HWY_AVX2

// ------------------------------ Integer <=> fp (ShiftRight, OddEven)

template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_F32_D(D)>
HWY_API VFromD<D> ConvertTo(D /* tag */, VFromD<Rebind<int32_t, D>> v) {
  return VFromD<D>{_mm_cvtepi32_ps(v.raw)};
}

#if HWY_TARGET <= HWY_AVX3
template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_F32_D(D)>
HWY_API VFromD<D> ConvertTo(D /*df*/, VFromD<Rebind<uint32_t, D>> v) {
  return VFromD<D>{_mm_cvtepu32_ps(v.raw)};
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_F64_D(D)>
HWY_API VFromD<D> ConvertTo(D /*dd*/, VFromD<Rebind<int64_t, D>> v) {
  return VFromD<D>{_mm_cvtepi64_pd(v.raw)};
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_F64_D(D)>
HWY_API VFromD<D> ConvertTo(D /*dd*/, VFromD<Rebind<uint64_t, D>> v) {
  return VFromD<D>{_mm_cvtepu64_pd(v.raw)};
}
#else   // AVX2 or below
template <class D, HWY_IF_F32_D(D)>
HWY_API VFromD<D> ConvertTo(D df, VFromD<Rebind<uint32_t, D>> v) {
  // Based on wim's approach (https://stackoverflow.com/questions/34066228/)
  const RebindToUnsigned<decltype(df)> du32;
  const RebindToSigned<decltype(df)> d32;

  const auto msk_lo = Set(du32, 0xFFFF);
  const auto cnst2_16_flt = Set(df, 65536.0f);  // 2^16

  // Extract the 16 lowest/highest significant bits of v and cast to signed int
  const auto v_lo = BitCast(d32, And(v, msk_lo));
  const auto v_hi = BitCast(d32, ShiftRight<16>(v));
  return MulAdd(cnst2_16_flt, ConvertTo(df, v_hi), ConvertTo(df, v_lo));
}

template <class D, HWY_IF_F64_D(D)>
HWY_API VFromD<D> ConvertTo(D dd, VFromD<Rebind<int64_t, D>> v) {
  // Based on wim's approach (https://stackoverflow.com/questions/41144668/)
  const Repartition<uint32_t, decltype(dd)> d32;
  const Repartition<uint64_t, decltype(dd)> d64;

  // Toggle MSB of lower 32-bits and insert exponent for 2^84 + 2^63
  const auto k84_63 = Set(d64, 0x4530000080000000ULL);
  const auto v_upper = BitCast(dd, ShiftRight<32>(BitCast(d64, v)) ^ k84_63);

  // Exponent is 2^52, lower 32 bits from v (=> 32-bit OddEven)
  const auto k52 = Set(d32, 0x43300000);
  const auto v_lower = BitCast(dd, OddEven(k52, BitCast(d32, v)));

  const auto k84_63_52 = BitCast(dd, Set(d64, 0x4530000080100000ULL));
  return (v_upper - k84_63_52) + v_lower;  // order matters!
}

namespace detail {
template <class VW>
HWY_INLINE VFromD<Rebind<double, DFromV<VW>>> U64ToF64VecFast(VW w) {
  const DFromV<decltype(w)> d64;
  const RebindToFloat<decltype(d64)> dd;
  const auto cnst2_52_dbl = Set(dd, 0x0010000000000000);  // 2^52
  return BitCast(dd, Or(w, BitCast(d64, cnst2_52_dbl))) - cnst2_52_dbl;
}
}  // namespace detail

template <class D, HWY_IF_F64_D(D)>
HWY_API VFromD<D> ConvertTo(D dd, VFromD<Rebind<uint64_t, D>> v) {
  // Based on wim's approach (https://stackoverflow.com/questions/41144668/)
  const RebindToUnsigned<decltype(dd)> d64;
  using VU = VFromD<decltype(d64)>;

  const VU msk_lo = Set(d64, 0xFFFFFFFF);
  const auto cnst2_32_dbl = Set(dd, 4294967296.0);  // 2^32

  // Extract the 32 lowest/highest significant bits of v
  const VU v_lo = And(v, msk_lo);
  const VU v_hi = ShiftRight<32>(v);

  const auto v_lo_dbl = detail::U64ToF64VecFast(v_lo);
  return MulAdd(cnst2_32_dbl, detail::U64ToF64VecFast(v_hi), v_lo_dbl);
}
#endif  // HWY_TARGET <= HWY_AVX3

// Truncates (rounds toward zero).
template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_I32_D(D)>
HWY_API VFromD<D> ConvertTo(D di, VFromD<Rebind<float, D>> v) {
  return detail::FixConversionOverflow(di, v, _mm_cvttps_epi32(v.raw));
}

#if HWY_TARGET <= HWY_AVX3
template <class DI, HWY_IF_V_SIZE_LE_D(DI, 16), HWY_IF_I64_D(DI)>
HWY_API VFromD<DI> ConvertTo(DI di, VFromD<Rebind<double, DI>> v) {
  return detail::FixConversionOverflow(di, v, _mm_cvttpd_epi64(v.raw));
}

#else  // AVX2 or below

#if HWY_ARCH_X86_64
template <class DI, HWY_IF_V_SIZE_D(DI, 8), HWY_IF_I64_D(DI)>
HWY_API VFromD<DI> ConvertTo(DI di, Vec64<double> v) {
  const Vec64<int64_t> i0{_mm_cvtsi64_si128(_mm_cvttsd_si64(v.raw))};
  return detail::FixConversionOverflow(di, v, i0.raw);
}
template <class DI, HWY_IF_V_SIZE_D(DI, 16), HWY_IF_I64_D(DI)>
HWY_API VFromD<DI> ConvertTo(DI di, Vec128<double> v) {
  const __m128i i0 = _mm_cvtsi64_si128(_mm_cvttsd_si64(v.raw));
  const Full64<double> dd2;
  const __m128i i1 = _mm_cvtsi64_si128(_mm_cvttsd_si64(UpperHalf(dd2, v).raw));
  return detail::FixConversionOverflow(di, v, _mm_unpacklo_epi64(i0, i1));
}
#endif  // HWY_ARCH_X86_64

#if !HWY_ARCH_X86_64 || HWY_TARGET <= HWY_AVX2
template <class DI, HWY_IF_V_SIZE_GT_D(DI, (HWY_ARCH_X86_64 ? 16 : 0)),
          HWY_IF_I64_D(DI)>
HWY_API VFromD<DI> ConvertTo(DI di, VFromD<Rebind<double, DI>> v) {
  using VI = VFromD<decltype(di)>;
  const RebindToUnsigned<decltype(di)> du;
  using VU = VFromD<decltype(du)>;
  const Repartition<uint16_t, decltype(di)> du16;
  const VI k1075 = Set(di, 1075); /* biased exponent of 2^52 */

  // Exponent indicates whether the number can be represented as int64_t.
  const VU biased_exp = ShiftRight<52>(BitCast(du, v)) & Set(du, 0x7FF);
#if HWY_TARGET <= HWY_SSE4
  const auto in_range = BitCast(di, biased_exp) < Set(di, 1086);
#else
  const Repartition<int32_t, decltype(di)> di32;
  const auto in_range = MaskFromVec(BitCast(
      di,
      VecFromMask(di32, DupEven(BitCast(di32, biased_exp)) < Set(di32, 1086))));
#endif

  // If we were to cap the exponent at 51 and add 2^52, the number would be in
  // [2^52, 2^53) and mantissa bits could be read out directly. We need to
  // round-to-0 (truncate), but changing rounding mode in MXCSR hits a
  // compiler reordering bug: https://gcc.godbolt.org/z/4hKj6c6qc . We instead
  // manually shift the mantissa into place (we already have many of the
  // inputs anyway).

  // Use 16-bit saturated unsigned subtraction to compute shift_mnt and
  // shift_int since biased_exp[i] is a non-negative integer that is less than
  // or equal to 2047.

  // 16-bit saturated unsigned subtraction is also more efficient than a
  // 64-bit subtraction followed by a 64-bit signed Max operation on
  // SSE2/SSSE3/SSE4/AVX2.

  // The upper 48 bits of both shift_mnt and shift_int are guaranteed to be
  // zero as the upper 48 bits of both k1075 and biased_exp are zero.

  const VU shift_mnt = BitCast(
      du, SaturatedSub(BitCast(du16, k1075), BitCast(du16, biased_exp)));
  const VU shift_int = BitCast(
      du, SaturatedSub(BitCast(du16, biased_exp), BitCast(du16, k1075)));
  const VU mantissa = BitCast(du, v) & Set(du, (1ULL << 52) - 1);
  // Include implicit 1-bit
  const VU int53 = (mantissa | Set(du, 1ULL << 52)) >> shift_mnt;

  // For inputs larger than 2^53 - 1, insert zeros at the bottom.

  // For inputs less than 2^63, the implicit 1-bit is guaranteed not to be
  // shifted out of the left shift result below as shift_int[i] <= 10 is true
  // for any inputs that are less than 2^63.

  const VU shifted = int53 << shift_int;

  // Saturate to LimitsMin (unchanged when negating below) or LimitsMax.
  const VI sign_mask = BroadcastSignBit(BitCast(di, v));
  const VI limit = Set(di, LimitsMax<int64_t>()) - sign_mask;
  const VI magnitude = IfThenElse(in_range, BitCast(di, shifted), limit);

  // If the input was negative, negate the integer (two's complement).
  return (magnitude ^ sign_mask) - sign_mask;
}
#endif  // !HWY_ARCH_X86_64 || HWY_TARGET <= HWY_AVX2
#endif  // HWY_TARGET <= HWY_AVX3

template <size_t N>
HWY_API Vec128<int32_t, N> NearestInt(const Vec128<float, N> v) {
  const RebindToSigned<DFromV<decltype(v)>> di;
  return detail::FixConversionOverflow(di, v, _mm_cvtps_epi32(v.raw));
}

// ------------------------------ Floating-point rounding (ConvertTo)

#if HWY_TARGET >= HWY_SSSE3

// Toward nearest integer, ties to even
template <typename T, size_t N>
HWY_API Vec128<T, N> Round(const Vec128<T, N> v) {
  static_assert(IsFloat<T>(), "Only for float");
  // Rely on rounding after addition with a large value such that no mantissa
  // bits remain (assuming the current mode is nearest-even). We may need a
  // compiler flag for precise floating-point to prevent "optimizing" this out.
  const DFromV<decltype(v)> df;
  const auto max = Set(df, MantissaEnd<T>());
  const auto large = CopySignToAbs(max, v);
  const auto added = large + v;
  const auto rounded = added - large;
  // Keep original if NaN or the magnitude is large (already an int).
  return IfThenElse(Abs(v) < max, rounded, v);
}

namespace detail {

// Truncating to integer and converting back to float is correct except when the
// input magnitude is large, in which case the input was already an integer
// (because mantissa >> exponent is zero).
template <typename T, size_t N>
HWY_INLINE Mask128<T, N> UseInt(const Vec128<T, N> v) {
  static_assert(IsFloat<T>(), "Only for float");
  const DFromV<decltype(v)> d;
  return Abs(v) < Set(d, MantissaEnd<T>());
}

}  // namespace detail

// Toward zero, aka truncate
template <typename T, size_t N>
HWY_API Vec128<T, N> Trunc(const Vec128<T, N> v) {
  static_assert(IsFloat<T>(), "Only for float");
  const DFromV<decltype(v)> df;
  const RebindToSigned<decltype(df)> di;

  const auto integer = ConvertTo(di, v);  // round toward 0
  const auto int_f = ConvertTo(df, integer);

  return IfThenElse(detail::UseInt(v), CopySign(int_f, v), v);
}

// Toward +infinity, aka ceiling
template <typename T, size_t N>
HWY_API Vec128<T, N> Ceil(const Vec128<T, N> v) {
  static_assert(IsFloat<T>(), "Only for float");
  const DFromV<decltype(v)> df;
  const RebindToSigned<decltype(df)> di;

  const auto integer = ConvertTo(di, v);  // round toward 0
  const auto int_f = ConvertTo(df, integer);

  // Truncating a positive non-integer ends up smaller; if so, add 1.
  const auto neg1 = ConvertTo(df, VecFromMask(di, RebindMask(di, int_f < v)));

  return IfThenElse(detail::UseInt(v), int_f - neg1, v);
}

// Toward -infinity, aka floor
template <typename T, size_t N>
HWY_API Vec128<T, N> Floor(const Vec128<T, N> v) {
  static_assert(IsFloat<T>(), "Only for float");
  const DFromV<decltype(v)> df;
  const RebindToSigned<decltype(df)> di;

  const auto integer = ConvertTo(di, v);  // round toward 0
  const auto int_f = ConvertTo(df, integer);

  // Truncating a negative non-integer ends up larger; if so, subtract 1.
  const auto neg1 = ConvertTo(df, VecFromMask(di, RebindMask(di, int_f > v)));

  return IfThenElse(detail::UseInt(v), int_f + neg1, v);
}

#else

// Toward nearest integer, ties to even
template <size_t N>
HWY_API Vec128<float, N> Round(const Vec128<float, N> v) {
  return Vec128<float, N>{
      _mm_round_ps(v.raw, _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC)};
}
template <size_t N>
HWY_API Vec128<double, N> Round(const Vec128<double, N> v) {
  return Vec128<double, N>{
      _mm_round_pd(v.raw, _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC)};
}

// Toward zero, aka truncate
template <size_t N>
HWY_API Vec128<float, N> Trunc(const Vec128<float, N> v) {
  return Vec128<float, N>{
      _mm_round_ps(v.raw, _MM_FROUND_TO_ZERO | _MM_FROUND_NO_EXC)};
}
template <size_t N>
HWY_API Vec128<double, N> Trunc(const Vec128<double, N> v) {
  return Vec128<double, N>{
      _mm_round_pd(v.raw, _MM_FROUND_TO_ZERO | _MM_FROUND_NO_EXC)};
}

// Toward +infinity, aka ceiling
template <size_t N>
HWY_API Vec128<float, N> Ceil(const Vec128<float, N> v) {
  return Vec128<float, N>{
      _mm_round_ps(v.raw, _MM_FROUND_TO_POS_INF | _MM_FROUND_NO_EXC)};
}
template <size_t N>
HWY_API Vec128<double, N> Ceil(const Vec128<double, N> v) {
  return Vec128<double, N>{
      _mm_round_pd(v.raw, _MM_FROUND_TO_POS_INF | _MM_FROUND_NO_EXC)};
}

// Toward -infinity, aka floor
template <size_t N>
HWY_API Vec128<float, N> Floor(const Vec128<float, N> v) {
  return Vec128<float, N>{
      _mm_round_ps(v.raw, _MM_FROUND_TO_NEG_INF | _MM_FROUND_NO_EXC)};
}
template <size_t N>
HWY_API Vec128<double, N> Floor(const Vec128<double, N> v) {
  return Vec128<double, N>{
      _mm_round_pd(v.raw, _MM_FROUND_TO_NEG_INF | _MM_FROUND_NO_EXC)};
}

#endif  // !HWY_SSSE3

// ------------------------------ Floating-point classification

template <size_t N>
HWY_API Mask128<float, N> IsNaN(const Vec128<float, N> v) {
#if HWY_TARGET <= HWY_AVX3
  return Mask128<float, N>{_mm_fpclass_ps_mask(v.raw, 0x81)};
#else
  return Mask128<float, N>{_mm_cmpunord_ps(v.raw, v.raw)};
#endif
}
template <size_t N>
HWY_API Mask128<double, N> IsNaN(const Vec128<double, N> v) {
#if HWY_TARGET <= HWY_AVX3
  return Mask128<double, N>{_mm_fpclass_pd_mask(v.raw, 0x81)};
#else
  return Mask128<double, N>{_mm_cmpunord_pd(v.raw, v.raw)};
#endif
}

#if HWY_TARGET <= HWY_AVX3

template <size_t N>
HWY_API Mask128<float, N> IsInf(const Vec128<float, N> v) {
  return Mask128<float, N>{_mm_fpclass_ps_mask(v.raw, 0x18)};
}
template <size_t N>
HWY_API Mask128<double, N> IsInf(const Vec128<double, N> v) {
  return Mask128<double, N>{_mm_fpclass_pd_mask(v.raw, 0x18)};
}

// Returns whether normal/subnormal/zero.
template <size_t N>
HWY_API Mask128<float, N> IsFinite(const Vec128<float, N> v) {
  // fpclass doesn't have a flag for positive, so we have to check for inf/NaN
  // and negate the mask.
  return Not(Mask128<float, N>{_mm_fpclass_ps_mask(v.raw, 0x99)});
}
template <size_t N>
HWY_API Mask128<double, N> IsFinite(const Vec128<double, N> v) {
  return Not(Mask128<double, N>{_mm_fpclass_pd_mask(v.raw, 0x99)});
}

#else

template <typename T, size_t N>
HWY_API Mask128<T, N> IsInf(const Vec128<T, N> v) {
  static_assert(IsFloat<T>(), "Only for float");
  const DFromV<decltype(v)> d;
  const RebindToSigned<decltype(d)> di;
  const VFromD<decltype(di)> vi = BitCast(di, v);
  // 'Shift left' to clear the sign bit, check for exponent=max and mantissa=0.
  return RebindMask(d, Eq(Add(vi, vi), Set(di, hwy::MaxExponentTimes2<T>())));
}

// Returns whether normal/subnormal/zero.
template <typename T, size_t N>
HWY_API Mask128<T, N> IsFinite(const Vec128<T, N> v) {
  static_assert(IsFloat<T>(), "Only for float");
  const DFromV<decltype(v)> d;
  const RebindToUnsigned<decltype(d)> du;
  const RebindToSigned<decltype(d)> di;  // cheaper than unsigned comparison
  const VFromD<decltype(du)> vu = BitCast(du, v);
  // Shift left to clear the sign bit, then right so we can compare with the
  // max exponent (cannot compare with MaxExponentTimes2 directly because it is
  // negative and non-negative floats would be greater). MSVC seems to generate
  // incorrect code if we instead add vu + vu.
  const VFromD<decltype(di)> exp =
      BitCast(di, ShiftRight<hwy::MantissaBits<T>() + 1>(ShiftLeft<1>(vu)));
  return RebindMask(d, Lt(exp, Set(di, hwy::MaxExponentField<T>())));
}

#endif  // HWY_TARGET <= HWY_AVX3

// ================================================== CRYPTO

#if !defined(HWY_DISABLE_PCLMUL_AES) && HWY_TARGET <= HWY_SSE4

// Per-target flag to prevent generic_ops-inl.h from defining AESRound.
#ifdef HWY_NATIVE_AES
#undef HWY_NATIVE_AES
#else
#define HWY_NATIVE_AES
#endif

HWY_API Vec128<uint8_t> AESRound(Vec128<uint8_t> state,
                                 Vec128<uint8_t> round_key) {
  return Vec128<uint8_t>{_mm_aesenc_si128(state.raw, round_key.raw)};
}

HWY_API Vec128<uint8_t> AESLastRound(Vec128<uint8_t> state,
                                     Vec128<uint8_t> round_key) {
  return Vec128<uint8_t>{_mm_aesenclast_si128(state.raw, round_key.raw)};
}

HWY_API Vec128<uint8_t> AESInvMixColumns(Vec128<uint8_t> state) {
  return Vec128<uint8_t>{_mm_aesimc_si128(state.raw)};
}

HWY_API Vec128<uint8_t> AESRoundInv(Vec128<uint8_t> state,
                                    Vec128<uint8_t> round_key) {
  return Vec128<uint8_t>{_mm_aesdec_si128(state.raw, round_key.raw)};
}

HWY_API Vec128<uint8_t> AESLastRoundInv(Vec128<uint8_t> state,
                                        Vec128<uint8_t> round_key) {
  return Vec128<uint8_t>{_mm_aesdeclast_si128(state.raw, round_key.raw)};
}

template <uint8_t kRcon>
HWY_API Vec128<uint8_t> AESKeyGenAssist(Vec128<uint8_t> v) {
  return Vec128<uint8_t>{_mm_aeskeygenassist_si128(v.raw, kRcon)};
}

template <size_t N>
HWY_API Vec128<uint64_t, N> CLMulLower(Vec128<uint64_t, N> a,
                                       Vec128<uint64_t, N> b) {
  return Vec128<uint64_t, N>{_mm_clmulepi64_si128(a.raw, b.raw, 0x00)};
}

template <size_t N>
HWY_API Vec128<uint64_t, N> CLMulUpper(Vec128<uint64_t, N> a,
                                       Vec128<uint64_t, N> b) {
  return Vec128<uint64_t, N>{_mm_clmulepi64_si128(a.raw, b.raw, 0x11)};
}

#endif  // !defined(HWY_DISABLE_PCLMUL_AES) && HWY_TARGET <= HWY_SSE4

// ================================================== MISC

// ------------------------------ LoadMaskBits (TestBit)

#if HWY_TARGET > HWY_AVX3
namespace detail {

template <class D, HWY_IF_T_SIZE_D(D, 1)>
HWY_INLINE MFromD<D> LoadMaskBits128(D d, uint64_t mask_bits) {
  const RebindToUnsigned<decltype(d)> du;
  // Easier than Set(), which would require an >8-bit type, which would not
  // compile for T=uint8_t, kN=1.
  const VFromD<D> vbits{_mm_cvtsi32_si128(static_cast<int>(mask_bits))};

#if HWY_TARGET == HWY_SSE2
  // {b0, b1, ...} ===> {b0, b0, b1, b1, ...}
  __m128i unpacked_vbits = _mm_unpacklo_epi8(vbits.raw, vbits.raw);
  // {b0, b0, b1, b1, ...} ==> {b0, b0, b0, b0, b1, b1, b1, b1, ...}
  unpacked_vbits = _mm_unpacklo_epi16(unpacked_vbits, unpacked_vbits);
  // {b0, b0, b0, b0, b1, b1, b1, b1, ...} ==>
  // {b0, b0, b0, b0, b0, b0, b0, b0, b1, b1, b1, b1, b1, b1, b1, b1}
  const VFromD<decltype(du)> rep8{
      _mm_unpacklo_epi32(unpacked_vbits, unpacked_vbits)};
#else
  // Replicate bytes 8x such that each byte contains the bit that governs it.
  alignas(16) static constexpr uint8_t kRep8[16] = {0, 0, 0, 0, 0, 0, 0, 0,
                                                    1, 1, 1, 1, 1, 1, 1, 1};
  const auto rep8 = TableLookupBytes(vbits, Load(du, kRep8));
#endif

  alignas(16) static constexpr uint8_t kBit[16] = {1, 2, 4, 8, 16, 32, 64, 128,
                                                   1, 2, 4, 8, 16, 32, 64, 128};
  return RebindMask(d, TestBit(rep8, LoadDup128(du, kBit)));
}

template <class D, HWY_IF_T_SIZE_D(D, 2)>
HWY_INLINE MFromD<D> LoadMaskBits128(D d, uint64_t mask_bits) {
  const RebindToUnsigned<decltype(d)> du;
  alignas(16) static constexpr uint16_t kBit[8] = {1, 2, 4, 8, 16, 32, 64, 128};
  const auto vmask_bits = Set(du, static_cast<uint16_t>(mask_bits));
  return RebindMask(d, TestBit(vmask_bits, Load(du, kBit)));
}

template <class D, HWY_IF_T_SIZE_D(D, 4)>
HWY_INLINE MFromD<D> LoadMaskBits128(D d, uint64_t mask_bits) {
  const RebindToUnsigned<decltype(d)> du;
  alignas(16) static constexpr uint32_t kBit[8] = {1, 2, 4, 8};
  const auto vmask_bits = Set(du, static_cast<uint32_t>(mask_bits));
  return RebindMask(d, TestBit(vmask_bits, Load(du, kBit)));
}

template <class D, HWY_IF_T_SIZE_D(D, 8)>
HWY_INLINE MFromD<D> LoadMaskBits128(D d, uint64_t mask_bits) {
  const RebindToUnsigned<decltype(d)> du;
  alignas(16) static constexpr uint64_t kBit[8] = {1, 2};
  return RebindMask(d, TestBit(Set(du, mask_bits), Load(du, kBit)));
}

}  // namespace detail
#endif  // HWY_TARGET > HWY_AVX3

// `p` points to at least 8 readable bytes, not all of which need be valid.
template <class D, HWY_IF_V_SIZE_LE_D(D, 16)>
HWY_API MFromD<D> LoadMaskBits(D d, const uint8_t* HWY_RESTRICT bits) {
  constexpr size_t kN = MaxLanes(d);
#if HWY_TARGET <= HWY_AVX3
  (void)d;
  uint64_t mask_bits = 0;
  constexpr size_t kNumBytes = (kN + 7) / 8;
  CopyBytes<kNumBytes>(bits, &mask_bits);
  if (kN < 8) {
    mask_bits &= (1ull << kN) - 1;
  }

  return MFromD<D>::FromBits(mask_bits);
#else
  uint64_t mask_bits = 0;
  constexpr size_t kNumBytes = (kN + 7) / 8;
  CopyBytes<kNumBytes>(bits, &mask_bits);
  if (kN < 8) {
    mask_bits &= (1ull << kN) - 1;
  }

  return detail::LoadMaskBits128(d, mask_bits);
#endif
}

template <typename T>
struct CompressIsPartition {
#if HWY_TARGET <= HWY_AVX3
  // AVX3 supports native compress, but a table-based approach allows
  // 'partitioning' (also moving mask=false lanes to the top), which helps
  // vqsort. This is only feasible for eight or less lanes, i.e. sizeof(T) == 8
  // on AVX3. For simplicity, we only use tables for 64-bit lanes (not AVX3
  // u32x8 etc.).
  enum { value = (sizeof(T) == 8) };
#else
  // generic_ops-inl does not guarantee IsPartition for 8-bit.
  enum { value = (sizeof(T) != 1) };
#endif
};

#if HWY_TARGET <= HWY_AVX3

// ------------------------------ StoreMaskBits

// `p` points to at least 8 writable bytes.
template <class D, HWY_IF_V_SIZE_LE_D(D, 16)>
HWY_API size_t StoreMaskBits(D d, MFromD<D> mask, uint8_t* bits) {
  constexpr size_t kN = MaxLanes(d);
  constexpr size_t kNumBytes = (kN + 7) / 8;
  CopyBytes<kNumBytes>(&mask.raw, bits);

  // Non-full byte, need to clear the undefined upper bits.
  if (kN < 8) {
    const int mask_bits = (1 << kN) - 1;
    bits[0] = static_cast<uint8_t>(bits[0] & mask_bits);
  }

  return kNumBytes;
}

// ------------------------------ Mask testing

// Beware: the suffix indicates the number of mask bits, not lane size!

template <class D, HWY_IF_V_SIZE_LE_D(D, 16)>
HWY_API size_t CountTrue(D d, MFromD<D> mask) {
  constexpr size_t kN = MaxLanes(d);
  const uint64_t mask_bits = uint64_t{mask.raw} & ((1ull << kN) - 1);
  return PopCount(mask_bits);
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 16)>
HWY_API size_t FindKnownFirstTrue(D d, MFromD<D> mask) {
  constexpr size_t kN = MaxLanes(d);
  const uint32_t mask_bits = uint32_t{mask.raw} & ((1u << kN) - 1);
  return Num0BitsBelowLS1Bit_Nonzero32(mask_bits);
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 16)>
HWY_API intptr_t FindFirstTrue(D d, MFromD<D> mask) {
  constexpr size_t kN = MaxLanes(d);
  const uint32_t mask_bits = uint32_t{mask.raw} & ((1u << kN) - 1);
  return mask_bits ? intptr_t(Num0BitsBelowLS1Bit_Nonzero32(mask_bits)) : -1;
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 16)>
HWY_API size_t FindKnownLastTrue(D d, MFromD<D> mask) {
  constexpr size_t kN = MaxLanes(d);
  const uint32_t mask_bits = uint32_t{mask.raw} & ((1u << kN) - 1);
  return 31 - Num0BitsAboveMS1Bit_Nonzero32(mask_bits);
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 16)>
HWY_API intptr_t FindLastTrue(D d, MFromD<D> mask) {
  constexpr size_t kN = MaxLanes(d);
  const uint32_t mask_bits = uint32_t{mask.raw} & ((1u << kN) - 1);
  return mask_bits ? intptr_t(31 - Num0BitsAboveMS1Bit_Nonzero32(mask_bits))
                   : -1;
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 16)>
HWY_API bool AllFalse(D d, MFromD<D> mask) {
  constexpr size_t kN = MaxLanes(d);
  const uint64_t mask_bits = uint64_t{mask.raw} & ((1ull << kN) - 1);
  return mask_bits == 0;
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 16)>
HWY_API bool AllTrue(D d, MFromD<D> mask) {
  constexpr size_t kN = MaxLanes(d);
  const uint64_t mask_bits = uint64_t{mask.raw} & ((1ull << kN) - 1);
  // Cannot use _kortestc because we may have less than 8 mask bits.
  return mask_bits == (1ull << kN) - 1;
}

// ------------------------------ Compress

// 8-16 bit Compress, CompressStore defined in x86_512 because they use Vec512.

// Single lane: no-op
template <typename T>
HWY_API Vec128<T, 1> Compress(Vec128<T, 1> v, Mask128<T, 1> /*m*/) {
  return v;
}

template <size_t N, HWY_IF_V_SIZE_GT(float, N, 4)>
HWY_API Vec128<float, N> Compress(Vec128<float, N> v, Mask128<float, N> mask) {
  return Vec128<float, N>{_mm_maskz_compress_ps(mask.raw, v.raw)};
}

template <typename T, HWY_IF_T_SIZE(T, 8)>
HWY_API Vec128<T> Compress(Vec128<T> v, Mask128<T> mask) {
  HWY_DASSERT(mask.raw < 4);

  // There are only 2 lanes, so we can afford to load the index vector directly.
  alignas(16) static constexpr uint8_t u8_indices[64] = {
      0, 1, 2,  3,  4,  5,  6,  7,  8, 9, 10, 11, 12, 13, 14, 15,
      0, 1, 2,  3,  4,  5,  6,  7,  8, 9, 10, 11, 12, 13, 14, 15,
      8, 9, 10, 11, 12, 13, 14, 15, 0, 1, 2,  3,  4,  5,  6,  7,
      0, 1, 2,  3,  4,  5,  6,  7,  8, 9, 10, 11, 12, 13, 14, 15};

  const DFromV<decltype(v)> d;
  const Repartition<uint8_t, decltype(d)> d8;
  const auto index = Load(d8, u8_indices + 16 * mask.raw);
  return BitCast(d, TableLookupBytes(BitCast(d8, v), index));
}

// ------------------------------ CompressNot (Compress)

// Single lane: no-op
template <typename T>
HWY_API Vec128<T, 1> CompressNot(Vec128<T, 1> v, Mask128<T, 1> /*m*/) {
  return v;
}

template <typename T, HWY_IF_T_SIZE(T, 8)>
HWY_API Vec128<T> CompressNot(Vec128<T> v, Mask128<T> mask) {
  // See CompressIsPartition, PrintCompressNot64x2NibbleTables
  alignas(16) static constexpr uint64_t packed_array[16] = {
      0x00000010, 0x00000001, 0x00000010, 0x00000010};

  // For lane i, shift the i-th 4-bit index down to bits [0, 2) -
  // _mm_permutexvar_epi64 will ignore the upper bits.
  const DFromV<decltype(v)> d;
  const RebindToUnsigned<decltype(d)> du64;
  const auto packed = Set(du64, packed_array[mask.raw]);
  alignas(16) static constexpr uint64_t shifts[2] = {0, 4};
  const auto indices = Indices128<T>{(packed >> Load(du64, shifts)).raw};
  return TableLookupLanes(v, indices);
}

// ------------------------------ CompressBlocksNot
HWY_API Vec128<uint64_t> CompressBlocksNot(Vec128<uint64_t> v,
                                           Mask128<uint64_t> /* m */) {
  return v;
}

// ------------------------------ CompressStore (defined in x86_512)

// ------------------------------ CompressBlendedStore (CompressStore)
template <class D, typename T = TFromD<D>, HWY_IF_V_SIZE_LE_D(D, 8)>
HWY_API size_t CompressBlendedStore(VFromD<D> v, MFromD<D> m, D d,
                                    T* HWY_RESTRICT unaligned) {
  // AVX-512 already does the blending at no extra cost (latency 11,
  // rthroughput 2 - same as compress plus store).
  if (HWY_TARGET == HWY_AVX3_DL ||
      (HWY_TARGET != HWY_AVX3_ZEN4 && sizeof(T) > 2)) {
    // We're relying on the mask to blend. Clear the undefined upper bits.
    constexpr size_t kN = MaxLanes(d);
    if (kN != 16 / sizeof(T)) {
      m = And(m, FirstN(d, kN));
    }
    return CompressStore(v, m, d, unaligned);
  } else {
    const size_t count = CountTrue(d, m);
    const VFromD<D> compressed = Compress(v, m);
#if HWY_MEM_OPS_MIGHT_FAULT
    // BlendedStore tests mask for each lane, but we know that the mask is
    // FirstN, so we can just copy.
    alignas(16) T buf[MaxLanes(d)];
    Store(compressed, d, buf);
    memcpy(unaligned, buf, count * sizeof(T));
#else
    BlendedStore(compressed, FirstN(d, count), d, unaligned);
#endif
    detail::MaybeUnpoison(unaligned, count);
    return count;
  }
}

// ------------------------------ CompressBitsStore (defined in x86_512)

#else  // AVX2 or below

// ------------------------------ StoreMaskBits

namespace detail {

constexpr HWY_INLINE uint64_t U64FromInt(int mask_bits) {
  return static_cast<uint64_t>(static_cast<unsigned>(mask_bits));
}

template <typename T, size_t N>
HWY_INLINE uint64_t BitsFromMask(hwy::SizeTag<1> /*tag*/,
                                 const Mask128<T, N> mask) {
  const Simd<T, N, 0> d;
  const auto sign_bits = BitCast(d, VecFromMask(d, mask)).raw;
  return U64FromInt(_mm_movemask_epi8(sign_bits));
}

template <typename T, size_t N>
HWY_INLINE uint64_t BitsFromMask(hwy::SizeTag<2> /*tag*/,
                                 const Mask128<T, N> mask) {
  // Remove useless lower half of each u16 while preserving the sign bit.
  const auto sign_bits = _mm_packs_epi16(mask.raw, _mm_setzero_si128());
  return U64FromInt(_mm_movemask_epi8(sign_bits));
}

template <typename T, size_t N>
HWY_INLINE uint64_t BitsFromMask(hwy::SizeTag<4> /*tag*/, Mask128<T, N> mask) {
  const Simd<T, N, 0> d;
  const Simd<float, N, 0> df;
  const auto sign_bits = BitCast(df, VecFromMask(d, mask));
  return U64FromInt(_mm_movemask_ps(sign_bits.raw));
}

template <typename T, size_t N>
HWY_INLINE uint64_t BitsFromMask(hwy::SizeTag<8> /*tag*/, Mask128<T, N> mask) {
  const Simd<T, N, 0> d;
  const Simd<double, N, 0> df;
  const auto sign_bits = BitCast(df, VecFromMask(d, mask));
  return U64FromInt(_mm_movemask_pd(sign_bits.raw));
}

template <typename T, size_t N>
HWY_INLINE uint64_t BitsFromMask(const Mask128<T, N> mask) {
  return OnlyActive<T, N>(BitsFromMask(hwy::SizeTag<sizeof(T)>(), mask));
}

}  // namespace detail

// `p` points to at least 8 writable bytes.
template <class D, HWY_IF_V_SIZE_LE_D(D, 16)>
HWY_API size_t StoreMaskBits(D d, MFromD<D> mask, uint8_t* bits) {
  constexpr size_t kNumBytes = (MaxLanes(d) + 7) / 8;
  const uint64_t mask_bits = detail::BitsFromMask(mask);
  CopyBytes<kNumBytes>(&mask_bits, bits);
  return kNumBytes;
}

// ------------------------------ Mask testing

template <class D, HWY_IF_V_SIZE_LE_D(D, 16)>
HWY_API bool AllFalse(D /* tag */, MFromD<D> mask) {
  // Cheaper than PTEST, which is 2 uop / 3L.
  return detail::BitsFromMask(mask) == 0;
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 16)>
HWY_API bool AllTrue(D d, MFromD<D> mask) {
  constexpr uint64_t kAllBits = (1ull << MaxLanes(d)) - 1;
  return detail::BitsFromMask(mask) == kAllBits;
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 16)>
HWY_API size_t CountTrue(D /* tag */, MFromD<D> mask) {
  return PopCount(detail::BitsFromMask(mask));
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 16)>
HWY_API size_t FindKnownFirstTrue(D /* tag */, MFromD<D> mask) {
  return Num0BitsBelowLS1Bit_Nonzero32(
      static_cast<uint32_t>(detail::BitsFromMask(mask)));
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 16)>
HWY_API intptr_t FindFirstTrue(D /* tag */, MFromD<D> mask) {
  const uint32_t mask_bits = static_cast<uint32_t>(detail::BitsFromMask(mask));
  return mask_bits ? intptr_t(Num0BitsBelowLS1Bit_Nonzero32(mask_bits)) : -1;
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 16)>
HWY_API size_t FindKnownLastTrue(D /* tag */, MFromD<D> mask) {
  return 31 - Num0BitsAboveMS1Bit_Nonzero32(
                  static_cast<uint32_t>(detail::BitsFromMask(mask)));
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 16)>
HWY_API intptr_t FindLastTrue(D /* tag */, MFromD<D> mask) {
  const uint32_t mask_bits = static_cast<uint32_t>(detail::BitsFromMask(mask));
  return mask_bits ? intptr_t(31 - Num0BitsAboveMS1Bit_Nonzero32(mask_bits))
                   : -1;
}

// ------------------------------ Compress, CompressBits

namespace detail {

// Also works for N < 8 because the first 16 4-tuples only reference bytes 0-6.
template <class D, HWY_IF_T_SIZE_D(D, 2)>
HWY_INLINE VFromD<D> IndicesFromBits128(D d, uint64_t mask_bits) {
  HWY_DASSERT(mask_bits < 256);
  const Rebind<uint8_t, decltype(d)> d8;
  const Twice<decltype(d8)> d8t;
  const RebindToUnsigned<decltype(d)> du;

  // compress_epi16 requires VBMI2 and there is no permutevar_epi16, so we need
  // byte indices for PSHUFB (one vector's worth for each of 256 combinations of
  // 8 mask bits). Loading them directly would require 4 KiB. We can instead
  // store lane indices and convert to byte indices (2*lane + 0..1), with the
  // doubling baked into the table. AVX2 Compress32 stores eight 4-bit lane
  // indices (total 1 KiB), broadcasts them into each 32-bit lane and shifts.
  // Here, 16-bit lanes are too narrow to hold all bits, and unpacking nibbles
  // is likely more costly than the higher cache footprint from storing bytes.
  alignas(16) static constexpr uint8_t table[2048] = {
      // PrintCompress16x8Tables
      0,  2,  4,  6,  8,  10, 12, 14, /**/ 0, 2,  4,  6,  8,  10, 12, 14,  //
      2,  0,  4,  6,  8,  10, 12, 14, /**/ 0, 2,  4,  6,  8,  10, 12, 14,  //
      4,  0,  2,  6,  8,  10, 12, 14, /**/ 0, 4,  2,  6,  8,  10, 12, 14,  //
      2,  4,  0,  6,  8,  10, 12, 14, /**/ 0, 2,  4,  6,  8,  10, 12, 14,  //
      6,  0,  2,  4,  8,  10, 12, 14, /**/ 0, 6,  2,  4,  8,  10, 12, 14,  //
      2,  6,  0,  4,  8,  10, 12, 14, /**/ 0, 2,  6,  4,  8,  10, 12, 14,  //
      4,  6,  0,  2,  8,  10, 12, 14, /**/ 0, 4,  6,  2,  8,  10, 12, 14,  //
      2,  4,  6,  0,  8,  10, 12, 14, /**/ 0, 2,  4,  6,  8,  10, 12, 14,  //
      8,  0,  2,  4,  6,  10, 12, 14, /**/ 0, 8,  2,  4,  6,  10, 12, 14,  //
      2,  8,  0,  4,  6,  10, 12, 14, /**/ 0, 2,  8,  4,  6,  10, 12, 14,  //
      4,  8,  0,  2,  6,  10, 12, 14, /**/ 0, 4,  8,  2,  6,  10, 12, 14,  //
      2,  4,  8,  0,  6,  10, 12, 14, /**/ 0, 2,  4,  8,  6,  10, 12, 14,  //
      6,  8,  0,  2,  4,  10, 12, 14, /**/ 0, 6,  8,  2,  4,  10, 12, 14,  //
      2,  6,  8,  0,  4,  10, 12, 14, /**/ 0, 2,  6,  8,  4,  10, 12, 14,  //
      4,  6,  8,  0,  2,  10, 12, 14, /**/ 0, 4,  6,  8,  2,  10, 12, 14,  //
      2,  4,  6,  8,  0,  10, 12, 14, /**/ 0, 2,  4,  6,  8,  10, 12, 14,  //
      10, 0,  2,  4,  6,  8,  12, 14, /**/ 0, 10, 2,  4,  6,  8,  12, 14,  //
      2,  10, 0,  4,  6,  8,  12, 14, /**/ 0, 2,  10, 4,  6,  8,  12, 14,  //
      4,  10, 0,  2,  6,  8,  12, 14, /**/ 0, 4,  10, 2,  6,  8,  12, 14,  //
      2,  4,  10, 0,  6,  8,  12, 14, /**/ 0, 2,  4,  10, 6,  8,  12, 14,  //
      6,  10, 0,  2,  4,  8,  12, 14, /**/ 0, 6,  10, 2,  4,  8,  12, 14,  //
      2,  6,  10, 0,  4,  8,  12, 14, /**/ 0, 2,  6,  10, 4,  8,  12, 14,  //
      4,  6,  10, 0,  2,  8,  12, 14, /**/ 0, 4,  6,  10, 2,  8,  12, 14,  //
      2,  4,  6,  10, 0,  8,  12, 14, /**/ 0, 2,  4,  6,  10, 8,  12, 14,  //
      8,  10, 0,  2,  4,  6,  12, 14, /**/ 0, 8,  10, 2,  4,  6,  12, 14,  //
      2,  8,  10, 0,  4,  6,  12, 14, /**/ 0, 2,  8,  10, 4,  6,  12, 14,  //
      4,  8,  10, 0,  2,  6,  12, 14, /**/ 0, 4,  8,  10, 2,  6,  12, 14,  //
      2,  4,  8,  10, 0,  6,  12, 14, /**/ 0, 2,  4,  8,  10, 6,  12, 14,  //
      6,  8,  10, 0,  2,  4,  12, 14, /**/ 0, 6,  8,  10, 2,  4,  12, 14,  //
      2,  6,  8,  10, 0,  4,  12, 14, /**/ 0, 2,  6,  8,  10, 4,  12, 14,  //
      4,  6,  8,  10, 0,  2,  12, 14, /**/ 0, 4,  6,  8,  10, 2,  12, 14,  //
      2,  4,  6,  8,  10, 0,  12, 14, /**/ 0, 2,  4,  6,  8,  10, 12, 14,  //
      12, 0,  2,  4,  6,  8,  10, 14, /**/ 0, 12, 2,  4,  6,  8,  10, 14,  //
      2,  12, 0,  4,  6,  8,  10, 14, /**/ 0, 2,  12, 4,  6,  8,  10, 14,  //
      4,  12, 0,  2,  6,  8,  10, 14, /**/ 0, 4,  12, 2,  6,  8,  10, 14,  //
      2,  4,  12, 0,  6,  8,  10, 14, /**/ 0, 2,  4,  12, 6,  8,  10, 14,  //
      6,  12, 0,  2,  4,  8,  10, 14, /**/ 0, 6,  12, 2,  4,  8,  10, 14,  //
      2,  6,  12, 0,  4,  8,  10, 14, /**/ 0, 2,  6,  12, 4,  8,  10, 14,  //
      4,  6,  12, 0,  2,  8,  10, 14, /**/ 0, 4,  6,  12, 2,  8,  10, 14,  //
      2,  4,  6,  12, 0,  8,  10, 14, /**/ 0, 2,  4,  6,  12, 8,  10, 14,  //
      8,  12, 0,  2,  4,  6,  10, 14, /**/ 0, 8,  12, 2,  4,  6,  10, 14,  //
      2,  8,  12, 0,  4,  6,  10, 14, /**/ 0, 2,  8,  12, 4,  6,  10, 14,  //
      4,  8,  12, 0,  2,  6,  10, 14, /**/ 0, 4,  8,  12, 2,  6,  10, 14,  //
      2,  4,  8,  12, 0,  6,  10, 14, /**/ 0, 2,  4,  8,  12, 6,  10, 14,  //
      6,  8,  12, 0,  2,  4,  10, 14, /**/ 0, 6,  8,  12, 2,  4,  10, 14,  //
      2,  6,  8,  12, 0,  4,  10, 14, /**/ 0, 2,  6,  8,  12, 4,  10, 14,  //
      4,  6,  8,  12, 0,  2,  10, 14, /**/ 0, 4,  6,  8,  12, 2,  10, 14,  //
      2,  4,  6,  8,  12, 0,  10, 14, /**/ 0, 2,  4,  6,  8,  12, 10, 14,  //
      10, 12, 0,  2,  4,  6,  8,  14, /**/ 0, 10, 12, 2,  4,  6,  8,  14,  //
      2,  10, 12, 0,  4,  6,  8,  14, /**/ 0, 2,  10, 12, 4,  6,  8,  14,  //
      4,  10, 12, 0,  2,  6,  8,  14, /**/ 0, 4,  10, 12, 2,  6,  8,  14,  //
      2,  4,  10, 12, 0,  6,  8,  14, /**/ 0, 2,  4,  10, 12, 6,  8,  14,  //
      6,  10, 12, 0,  2,  4,  8,  14, /**/ 0, 6,  10, 12, 2,  4,  8,  14,  //
      2,  6,  10, 12, 0,  4,  8,  14, /**/ 0, 2,  6,  10, 12, 4,  8,  14,  //
      4,  6,  10, 12, 0,  2,  8,  14, /**/ 0, 4,  6,  10, 12, 2,  8,  14,  //
      2,  4,  6,  10, 12, 0,  8,  14, /**/ 0, 2,  4,  6,  10, 12, 8,  14,  //
      8,  10, 12, 0,  2,  4,  6,  14, /**/ 0, 8,  10, 12, 2,  4,  6,  14,  //
      2,  8,  10, 12, 0,  4,  6,  14, /**/ 0, 2,  8,  10, 12, 4,  6,  14,  //
      4,  8,  10, 12, 0,  2,  6,  14, /**/ 0, 4,  8,  10, 12, 2,  6,  14,  //
      2,  4,  8,  10, 12, 0,  6,  14, /**/ 0, 2,  4,  8,  10, 12, 6,  14,  //
      6,  8,  10, 12, 0,  2,  4,  14, /**/ 0, 6,  8,  10, 12, 2,  4,  14,  //
      2,  6,  8,  10, 12, 0,  4,  14, /**/ 0, 2,  6,  8,  10, 12, 4,  14,  //
      4,  6,  8,  10, 12, 0,  2,  14, /**/ 0, 4,  6,  8,  10, 12, 2,  14,  //
      2,  4,  6,  8,  10, 12, 0,  14, /**/ 0, 2,  4,  6,  8,  10, 12, 14,  //
      14, 0,  2,  4,  6,  8,  10, 12, /**/ 0, 14, 2,  4,  6,  8,  10, 12,  //
      2,  14, 0,  4,  6,  8,  10, 12, /**/ 0, 2,  14, 4,  6,  8,  10, 12,  //
      4,  14, 0,  2,  6,  8,  10, 12, /**/ 0, 4,  14, 2,  6,  8,  10, 12,  //
      2,  4,  14, 0,  6,  8,  10, 12, /**/ 0, 2,  4,  14, 6,  8,  10, 12,  //
      6,  14, 0,  2,  4,  8,  10, 12, /**/ 0, 6,  14, 2,  4,  8,  10, 12,  //
      2,  6,  14, 0,  4,  8,  10, 12, /**/ 0, 2,  6,  14, 4,  8,  10, 12,  //
      4,  6,  14, 0,  2,  8,  10, 12, /**/ 0, 4,  6,  14, 2,  8,  10, 12,  //
      2,  4,  6,  14, 0,  8,  10, 12, /**/ 0, 2,  4,  6,  14, 8,  10, 12,  //
      8,  14, 0,  2,  4,  6,  10, 12, /**/ 0, 8,  14, 2,  4,  6,  10, 12,  //
      2,  8,  14, 0,  4,  6,  10, 12, /**/ 0, 2,  8,  14, 4,  6,  10, 12,  //
      4,  8,  14, 0,  2,  6,  10, 12, /**/ 0, 4,  8,  14, 2,  6,  10, 12,  //
      2,  4,  8,  14, 0,  6,  10, 12, /**/ 0, 2,  4,  8,  14, 6,  10, 12,  //
      6,  8,  14, 0,  2,  4,  10, 12, /**/ 0, 6,  8,  14, 2,  4,  10, 12,  //
      2,  6,  8,  14, 0,  4,  10, 12, /**/ 0, 2,  6,  8,  14, 4,  10, 12,  //
      4,  6,  8,  14, 0,  2,  10, 12, /**/ 0, 4,  6,  8,  14, 2,  10, 12,  //
      2,  4,  6,  8,  14, 0,  10, 12, /**/ 0, 2,  4,  6,  8,  14, 10, 12,  //
      10, 14, 0,  2,  4,  6,  8,  12, /**/ 0, 10, 14, 2,  4,  6,  8,  12,  //
      2,  10, 14, 0,  4,  6,  8,  12, /**/ 0, 2,  10, 14, 4,  6,  8,  12,  //
      4,  10, 14, 0,  2,  6,  8,  12, /**/ 0, 4,  10, 14, 2,  6,  8,  12,  //
      2,  4,  10, 14, 0,  6,  8,  12, /**/ 0, 2,  4,  10, 14, 6,  8,  12,  //
      6,  10, 14, 0,  2,  4,  8,  12, /**/ 0, 6,  10, 14, 2,  4,  8,  12,  //
      2,  6,  10, 14, 0,  4,  8,  12, /**/ 0, 2,  6,  10, 14, 4,  8,  12,  //
      4,  6,  10, 14, 0,  2,  8,  12, /**/ 0, 4,  6,  10, 14, 2,  8,  12,  //
      2,  4,  6,  10, 14, 0,  8,  12, /**/ 0, 2,  4,  6,  10, 14, 8,  12,  //
      8,  10, 14, 0,  2,  4,  6,  12, /**/ 0, 8,  10, 14, 2,  4,  6,  12,  //
      2,  8,  10, 14, 0,  4,  6,  12, /**/ 0, 2,  8,  10, 14, 4,  6,  12,  //
      4,  8,  10, 14, 0,  2,  6,  12, /**/ 0, 4,  8,  10, 14, 2,  6,  12,  //
      2,  4,  8,  10, 14, 0,  6,  12, /**/ 0, 2,  4,  8,  10, 14, 6,  12,  //
      6,  8,  10, 14, 0,  2,  4,  12, /**/ 0, 6,  8,  10, 14, 2,  4,  12,  //
      2,  6,  8,  10, 14, 0,  4,  12, /**/ 0, 2,  6,  8,  10, 14, 4,  12,  //
      4,  6,  8,  10, 14, 0,  2,  12, /**/ 0, 4,  6,  8,  10, 14, 2,  12,  //
      2,  4,  6,  8,  10, 14, 0,  12, /**/ 0, 2,  4,  6,  8,  10, 14, 12,  //
      12, 14, 0,  2,  4,  6,  8,  10, /**/ 0, 12, 14, 2,  4,  6,  8,  10,  //
      2,  12, 14, 0,  4,  6,  8,  10, /**/ 0, 2,  12, 14, 4,  6,  8,  10,  //
      4,  12, 14, 0,  2,  6,  8,  10, /**/ 0, 4,  12, 14, 2,  6,  8,  10,  //
      2,  4,  12, 14, 0,  6,  8,  10, /**/ 0, 2,  4,  12, 14, 6,  8,  10,  //
      6,  12, 14, 0,  2,  4,  8,  10, /**/ 0, 6,  12, 14, 2,  4,  8,  10,  //
      2,  6,  12, 14, 0,  4,  8,  10, /**/ 0, 2,  6,  12, 14, 4,  8,  10,  //
      4,  6,  12, 14, 0,  2,  8,  10, /**/ 0, 4,  6,  12, 14, 2,  8,  10,  //
      2,  4,  6,  12, 14, 0,  8,  10, /**/ 0, 2,  4,  6,  12, 14, 8,  10,  //
      8,  12, 14, 0,  2,  4,  6,  10, /**/ 0, 8,  12, 14, 2,  4,  6,  10,  //
      2,  8,  12, 14, 0,  4,  6,  10, /**/ 0, 2,  8,  12, 14, 4,  6,  10,  //
      4,  8,  12, 14, 0,  2,  6,  10, /**/ 0, 4,  8,  12, 14, 2,  6,  10,  //
      2,  4,  8,  12, 14, 0,  6,  10, /**/ 0, 2,  4,  8,  12, 14, 6,  10,  //
      6,  8,  12, 14, 0,  2,  4,  10, /**/ 0, 6,  8,  12, 14, 2,  4,  10,  //
      2,  6,  8,  12, 14, 0,  4,  10, /**/ 0, 2,  6,  8,  12, 14, 4,  10,  //
      4,  6,  8,  12, 14, 0,  2,  10, /**/ 0, 4,  6,  8,  12, 14, 2,  10,  //
      2,  4,  6,  8,  12, 14, 0,  10, /**/ 0, 2,  4,  6,  8,  12, 14, 10,  //
      10, 12, 14, 0,  2,  4,  6,  8,  /**/ 0, 10, 12, 14, 2,  4,  6,  8,   //
      2,  10, 12, 14, 0,  4,  6,  8,  /**/ 0, 2,  10, 12, 14, 4,  6,  8,   //
      4,  10, 12, 14, 0,  2,  6,  8,  /**/ 0, 4,  10, 12, 14, 2,  6,  8,   //
      2,  4,  10, 12, 14, 0,  6,  8,  /**/ 0, 2,  4,  10, 12, 14, 6,  8,   //
      6,  10, 12, 14, 0,  2,  4,  8,  /**/ 0, 6,  10, 12, 14, 2,  4,  8,   //
      2,  6,  10, 12, 14, 0,  4,  8,  /**/ 0, 2,  6,  10, 12, 14, 4,  8,   //
      4,  6,  10, 12, 14, 0,  2,  8,  /**/ 0, 4,  6,  10, 12, 14, 2,  8,   //
      2,  4,  6,  10, 12, 14, 0,  8,  /**/ 0, 2,  4,  6,  10, 12, 14, 8,   //
      8,  10, 12, 14, 0,  2,  4,  6,  /**/ 0, 8,  10, 12, 14, 2,  4,  6,   //
      2,  8,  10, 12, 14, 0,  4,  6,  /**/ 0, 2,  8,  10, 12, 14, 4,  6,   //
      4,  8,  10, 12, 14, 0,  2,  6,  /**/ 0, 4,  8,  10, 12, 14, 2,  6,   //
      2,  4,  8,  10, 12, 14, 0,  6,  /**/ 0, 2,  4,  8,  10, 12, 14, 6,   //
      6,  8,  10, 12, 14, 0,  2,  4,  /**/ 0, 6,  8,  10, 12, 14, 2,  4,   //
      2,  6,  8,  10, 12, 14, 0,  4,  /**/ 0, 2,  6,  8,  10, 12, 14, 4,   //
      4,  6,  8,  10, 12, 14, 0,  2,  /**/ 0, 4,  6,  8,  10, 12, 14, 2,   //
      2,  4,  6,  8,  10, 12, 14, 0,  /**/ 0, 2,  4,  6,  8,  10, 12, 14};

  const VFromD<decltype(d8t)> byte_idx{Load(d8, table + mask_bits * 8).raw};
  const VFromD<decltype(du)> pairs = ZipLower(byte_idx, byte_idx);
  return BitCast(d, pairs + Set(du, 0x0100));
}

template <class D, HWY_IF_T_SIZE_D(D, 2)>
HWY_INLINE VFromD<D> IndicesFromNotBits128(D d, uint64_t mask_bits) {
  HWY_DASSERT(mask_bits < 256);
  const Rebind<uint8_t, decltype(d)> d8;
  const Twice<decltype(d8)> d8t;
  const RebindToUnsigned<decltype(d)> du;

  // compress_epi16 requires VBMI2 and there is no permutevar_epi16, so we need
  // byte indices for PSHUFB (one vector's worth for each of 256 combinations of
  // 8 mask bits). Loading them directly would require 4 KiB. We can instead
  // store lane indices and convert to byte indices (2*lane + 0..1), with the
  // doubling baked into the table. AVX2 Compress32 stores eight 4-bit lane
  // indices (total 1 KiB), broadcasts them into each 32-bit lane and shifts.
  // Here, 16-bit lanes are too narrow to hold all bits, and unpacking nibbles
  // is likely more costly than the higher cache footprint from storing bytes.
  alignas(16) static constexpr uint8_t table[2048] = {
      // PrintCompressNot16x8Tables
      0, 2,  4,  6,  8,  10, 12, 14, /**/ 2,  4,  6,  8,  10, 12, 14, 0,   //
      0, 4,  6,  8,  10, 12, 14, 2,  /**/ 4,  6,  8,  10, 12, 14, 0,  2,   //
      0, 2,  6,  8,  10, 12, 14, 4,  /**/ 2,  6,  8,  10, 12, 14, 0,  4,   //
      0, 6,  8,  10, 12, 14, 2,  4,  /**/ 6,  8,  10, 12, 14, 0,  2,  4,   //
      0, 2,  4,  8,  10, 12, 14, 6,  /**/ 2,  4,  8,  10, 12, 14, 0,  6,   //
      0, 4,  8,  10, 12, 14, 2,  6,  /**/ 4,  8,  10, 12, 14, 0,  2,  6,   //
      0, 2,  8,  10, 12, 14, 4,  6,  /**/ 2,  8,  10, 12, 14, 0,  4,  6,   //
      0, 8,  10, 12, 14, 2,  4,  6,  /**/ 8,  10, 12, 14, 0,  2,  4,  6,   //
      0, 2,  4,  6,  10, 12, 14, 8,  /**/ 2,  4,  6,  10, 12, 14, 0,  8,   //
      0, 4,  6,  10, 12, 14, 2,  8,  /**/ 4,  6,  10, 12, 14, 0,  2,  8,   //
      0, 2,  6,  10, 12, 14, 4,  8,  /**/ 2,  6,  10, 12, 14, 0,  4,  8,   //
      0, 6,  10, 12, 14, 2,  4,  8,  /**/ 6,  10, 12, 14, 0,  2,  4,  8,   //
      0, 2,  4,  10, 12, 14, 6,  8,  /**/ 2,  4,  10, 12, 14, 0,  6,  8,   //
      0, 4,  10, 12, 14, 2,  6,  8,  /**/ 4,  10, 12, 14, 0,  2,  6,  8,   //
      0, 2,  10, 12, 14, 4,  6,  8,  /**/ 2,  10, 12, 14, 0,  4,  6,  8,   //
      0, 10, 12, 14, 2,  4,  6,  8,  /**/ 10, 12, 14, 0,  2,  4,  6,  8,   //
      0, 2,  4,  6,  8,  12, 14, 10, /**/ 2,  4,  6,  8,  12, 14, 0,  10,  //
      0, 4,  6,  8,  12, 14, 2,  10, /**/ 4,  6,  8,  12, 14, 0,  2,  10,  //
      0, 2,  6,  8,  12, 14, 4,  10, /**/ 2,  6,  8,  12, 14, 0,  4,  10,  //
      0, 6,  8,  12, 14, 2,  4,  10, /**/ 6,  8,  12, 14, 0,  2,  4,  10,  //
      0, 2,  4,  8,  12, 14, 6,  10, /**/ 2,  4,  8,  12, 14, 0,  6,  10,  //
      0, 4,  8,  12, 14, 2,  6,  10, /**/ 4,  8,  12, 14, 0,  2,  6,  10,  //
      0, 2,  8,  12, 14, 4,  6,  10, /**/ 2,  8,  12, 14, 0,  4,  6,  10,  //
      0, 8,  12, 14, 2,  4,  6,  10, /**/ 8,  12, 14, 0,  2,  4,  6,  10,  //
      0, 2,  4,  6,  12, 14, 8,  10, /**/ 2,  4,  6,  12, 14, 0,  8,  10,  //
      0, 4,  6,  12, 14, 2,  8,  10, /**/ 4,  6,  12, 14, 0,  2,  8,  10,  //
      0, 2,  6,  12, 14, 4,  8,  10, /**/ 2,  6,  12, 14, 0,  4,  8,  10,  //
      0, 6,  12, 14, 2,  4,  8,  10, /**/ 6,  12, 14, 0,  2,  4,  8,  10,  //
      0, 2,  4,  12, 14, 6,  8,  10, /**/ 2,  4,  12, 14, 0,  6,  8,  10,  //
      0, 4,  12, 14, 2,  6,  8,  10, /**/ 4,  12, 14, 0,  2,  6,  8,  10,  //
      0, 2,  12, 14, 4,  6,  8,  10, /**/ 2,  12, 14, 0,  4,  6,  8,  10,  //
      0, 12, 14, 2,  4,  6,  8,  10, /**/ 12, 14, 0,  2,  4,  6,  8,  10,  //
      0, 2,  4,  6,  8,  10, 14, 12, /**/ 2,  4,  6,  8,  10, 14, 0,  12,  //
      0, 4,  6,  8,  10, 14, 2,  12, /**/ 4,  6,  8,  10, 14, 0,  2,  12,  //
      0, 2,  6,  8,  10, 14, 4,  12, /**/ 2,  6,  8,  10, 14, 0,  4,  12,  //
      0, 6,  8,  10, 14, 2,  4,  12, /**/ 6,  8,  10, 14, 0,  2,  4,  12,  //
      0, 2,  4,  8,  10, 14, 6,  12, /**/ 2,  4,  8,  10, 14, 0,  6,  12,  //
      0, 4,  8,  10, 14, 2,  6,  12, /**/ 4,  8,  10, 14, 0,  2,  6,  12,  //
      0, 2,  8,  10, 14, 4,  6,  12, /**/ 2,  8,  10, 14, 0,  4,  6,  12,  //
      0, 8,  10, 14, 2,  4,  6,  12, /**/ 8,  10, 14, 0,  2,  4,  6,  12,  //
      0, 2,  4,  6,  10, 14, 8,  12, /**/ 2,  4,  6,  10, 14, 0,  8,  12,  //
      0, 4,  6,  10, 14, 2,  8,  12, /**/ 4,  6,  10, 14, 0,  2,  8,  12,  //
      0, 2,  6,  10, 14, 4,  8,  12, /**/ 2,  6,  10, 14, 0,  4,  8,  12,  //
      0, 6,  10, 14, 2,  4,  8,  12, /**/ 6,  10, 14, 0,  2,  4,  8,  12,  //
      0, 2,  4,  10, 14, 6,  8,  12, /**/ 2,  4,  10, 14, 0,  6,  8,  12,  //
      0, 4,  10, 14, 2,  6,  8,  12, /**/ 4,  10, 14, 0,  2,  6,  8,  12,  //
      0, 2,  10, 14, 4,  6,  8,  12, /**/ 2,  10, 14, 0,  4,  6,  8,  12,  //
      0, 10, 14, 2,  4,  6,  8,  12, /**/ 10, 14, 0,  2,  4,  6,  8,  12,  //
      0, 2,  4,  6,  8,  14, 10, 12, /**/ 2,  4,  6,  8,  14, 0,  10, 12,  //
      0, 4,  6,  8,  14, 2,  10, 12, /**/ 4,  6,  8,  14, 0,  2,  10, 12,  //
      0, 2,  6,  8,  14, 4,  10, 12, /**/ 2,  6,  8,  14, 0,  4,  10, 12,  //
      0, 6,  8,  14, 2,  4,  10, 12, /**/ 6,  8,  14, 0,  2,  4,  10, 12,  //
      0, 2,  4,  8,  14, 6,  10, 12, /**/ 2,  4,  8,  14, 0,  6,  10, 12,  //
      0, 4,  8,  14, 2,  6,  10, 12, /**/ 4,  8,  14, 0,  2,  6,  10, 12,  //
      0, 2,  8,  14, 4,  6,  10, 12, /**/ 2,  8,  14, 0,  4,  6,  10, 12,  //
      0, 8,  14, 2,  4,  6,  10, 12, /**/ 8,  14, 0,  2,  4,  6,  10, 12,  //
      0, 2,  4,  6,  14, 8,  10, 12, /**/ 2,  4,  6,  14, 0,  8,  10, 12,  //
      0, 4,  6,  14, 2,  8,  10, 12, /**/ 4,  6,  14, 0,  2,  8,  10, 12,  //
      0, 2,  6,  14, 4,  8,  10, 12, /**/ 2,  6,  14, 0,  4,  8,  10, 12,  //
      0, 6,  14, 2,  4,  8,  10, 12, /**/ 6,  14, 0,  2,  4,  8,  10, 12,  //
      0, 2,  4,  14, 6,  8,  10, 12, /**/ 2,  4,  14, 0,  6,  8,  10, 12,  //
      0, 4,  14, 2,  6,  8,  10, 12, /**/ 4,  14, 0,  2,  6,  8,  10, 12,  //
      0, 2,  14, 4,  6,  8,  10, 12, /**/ 2,  14, 0,  4,  6,  8,  10, 12,  //
      0, 14, 2,  4,  6,  8,  10, 12, /**/ 14, 0,  2,  4,  6,  8,  10, 12,  //
      0, 2,  4,  6,  8,  10, 12, 14, /**/ 2,  4,  6,  8,  10, 12, 0,  14,  //
      0, 4,  6,  8,  10, 12, 2,  14, /**/ 4,  6,  8,  10, 12, 0,  2,  14,  //
      0, 2,  6,  8,  10, 12, 4,  14, /**/ 2,  6,  8,  10, 12, 0,  4,  14,  //
      0, 6,  8,  10, 12, 2,  4,  14, /**/ 6,  8,  10, 12, 0,  2,  4,  14,  //
      0, 2,  4,  8,  10, 12, 6,  14, /**/ 2,  4,  8,  10, 12, 0,  6,  14,  //
      0, 4,  8,  10, 12, 2,  6,  14, /**/ 4,  8,  10, 12, 0,  2,  6,  14,  //
      0, 2,  8,  10, 12, 4,  6,  14, /**/ 2,  8,  10, 12, 0,  4,  6,  14,  //
      0, 8,  10, 12, 2,  4,  6,  14, /**/ 8,  10, 12, 0,  2,  4,  6,  14,  //
      0, 2,  4,  6,  10, 12, 8,  14, /**/ 2,  4,  6,  10, 12, 0,  8,  14,  //
      0, 4,  6,  10, 12, 2,  8,  14, /**/ 4,  6,  10, 12, 0,  2,  8,  14,  //
      0, 2,  6,  10, 12, 4,  8,  14, /**/ 2,  6,  10, 12, 0,  4,  8,  14,  //
      0, 6,  10, 12, 2,  4,  8,  14, /**/ 6,  10, 12, 0,  2,  4,  8,  14,  //
      0, 2,  4,  10, 12, 6,  8,  14, /**/ 2,  4,  10, 12, 0,  6,  8,  14,  //
      0, 4,  10, 12, 2,  6,  8,  14, /**/ 4,  10, 12, 0,  2,  6,  8,  14,  //
      0, 2,  10, 12, 4,  6,  8,  14, /**/ 2,  10, 12, 0,  4,  6,  8,  14,  //
      0, 10, 12, 2,  4,  6,  8,  14, /**/ 10, 12, 0,  2,  4,  6,  8,  14,  //
      0, 2,  4,  6,  8,  12, 10, 14, /**/ 2,  4,  6,  8,  12, 0,  10, 14,  //
      0, 4,  6,  8,  12, 2,  10, 14, /**/ 4,  6,  8,  12, 0,  2,  10, 14,  //
      0, 2,  6,  8,  12, 4,  10, 14, /**/ 2,  6,  8,  12, 0,  4,  10, 14,  //
      0, 6,  8,  12, 2,  4,  10, 14, /**/ 6,  8,  12, 0,  2,  4,  10, 14,  //
      0, 2,  4,  8,  12, 6,  10, 14, /**/ 2,  4,  8,  12, 0,  6,  10, 14,  //
      0, 4,  8,  12, 2,  6,  10, 14, /**/ 4,  8,  12, 0,  2,  6,  10, 14,  //
      0, 2,  8,  12, 4,  6,  10, 14, /**/ 2,  8,  12, 0,  4,  6,  10, 14,  //
      0, 8,  12, 2,  4,  6,  10, 14, /**/ 8,  12, 0,  2,  4,  6,  10, 14,  //
      0, 2,  4,  6,  12, 8,  10, 14, /**/ 2,  4,  6,  12, 0,  8,  10, 14,  //
      0, 4,  6,  12, 2,  8,  10, 14, /**/ 4,  6,  12, 0,  2,  8,  10, 14,  //
      0, 2,  6,  12, 4,  8,  10, 14, /**/ 2,  6,  12, 0,  4,  8,  10, 14,  //
      0, 6,  12, 2,  4,  8,  10, 14, /**/ 6,  12, 0,  2,  4,  8,  10, 14,  //
      0, 2,  4,  12, 6,  8,  10, 14, /**/ 2,  4,  12, 0,  6,  8,  10, 14,  //
      0, 4,  12, 2,  6,  8,  10, 14, /**/ 4,  12, 0,  2,  6,  8,  10, 14,  //
      0, 2,  12, 4,  6,  8,  10, 14, /**/ 2,  12, 0,  4,  6,  8,  10, 14,  //
      0, 12, 2,  4,  6,  8,  10, 14, /**/ 12, 0,  2,  4,  6,  8,  10, 14,  //
      0, 2,  4,  6,  8,  10, 12, 14, /**/ 2,  4,  6,  8,  10, 0,  12, 14,  //
      0, 4,  6,  8,  10, 2,  12, 14, /**/ 4,  6,  8,  10, 0,  2,  12, 14,  //
      0, 2,  6,  8,  10, 4,  12, 14, /**/ 2,  6,  8,  10, 0,  4,  12, 14,  //
      0, 6,  8,  10, 2,  4,  12, 14, /**/ 6,  8,  10, 0,  2,  4,  12, 14,  //
      0, 2,  4,  8,  10, 6,  12, 14, /**/ 2,  4,  8,  10, 0,  6,  12, 14,  //
      0, 4,  8,  10, 2,  6,  12, 14, /**/ 4,  8,  10, 0,  2,  6,  12, 14,  //
      0, 2,  8,  10, 4,  6,  12, 14, /**/ 2,  8,  10, 0,  4,  6,  12, 14,  //
      0, 8,  10, 2,  4,  6,  12, 14, /**/ 8,  10, 0,  2,  4,  6,  12, 14,  //
      0, 2,  4,  6,  10, 8,  12, 14, /**/ 2,  4,  6,  10, 0,  8,  12, 14,  //
      0, 4,  6,  10, 2,  8,  12, 14, /**/ 4,  6,  10, 0,  2,  8,  12, 14,  //
      0, 2,  6,  10, 4,  8,  12, 14, /**/ 2,  6,  10, 0,  4,  8,  12, 14,  //
      0, 6,  10, 2,  4,  8,  12, 14, /**/ 6,  10, 0,  2,  4,  8,  12, 14,  //
      0, 2,  4,  10, 6,  8,  12, 14, /**/ 2,  4,  10, 0,  6,  8,  12, 14,  //
      0, 4,  10, 2,  6,  8,  12, 14, /**/ 4,  10, 0,  2,  6,  8,  12, 14,  //
      0, 2,  10, 4,  6,  8,  12, 14, /**/ 2,  10, 0,  4,  6,  8,  12, 14,  //
      0, 10, 2,  4,  6,  8,  12, 14, /**/ 10, 0,  2,  4,  6,  8,  12, 14,  //
      0, 2,  4,  6,  8,  10, 12, 14, /**/ 2,  4,  6,  8,  0,  10, 12, 14,  //
      0, 4,  6,  8,  2,  10, 12, 14, /**/ 4,  6,  8,  0,  2,  10, 12, 14,  //
      0, 2,  6,  8,  4,  10, 12, 14, /**/ 2,  6,  8,  0,  4,  10, 12, 14,  //
      0, 6,  8,  2,  4,  10, 12, 14, /**/ 6,  8,  0,  2,  4,  10, 12, 14,  //
      0, 2,  4,  8,  6,  10, 12, 14, /**/ 2,  4,  8,  0,  6,  10, 12, 14,  //
      0, 4,  8,  2,  6,  10, 12, 14, /**/ 4,  8,  0,  2,  6,  10, 12, 14,  //
      0, 2,  8,  4,  6,  10, 12, 14, /**/ 2,  8,  0,  4,  6,  10, 12, 14,  //
      0, 8,  2,  4,  6,  10, 12, 14, /**/ 8,  0,  2,  4,  6,  10, 12, 14,  //
      0, 2,  4,  6,  8,  10, 12, 14, /**/ 2,  4,  6,  0,  8,  10, 12, 14,  //
      0, 4,  6,  2,  8,  10, 12, 14, /**/ 4,  6,  0,  2,  8,  10, 12, 14,  //
      0, 2,  6,  4,  8,  10, 12, 14, /**/ 2,  6,  0,  4,  8,  10, 12, 14,  //
      0, 6,  2,  4,  8,  10, 12, 14, /**/ 6,  0,  2,  4,  8,  10, 12, 14,  //
      0, 2,  4,  6,  8,  10, 12, 14, /**/ 2,  4,  0,  6,  8,  10, 12, 14,  //
      0, 4,  2,  6,  8,  10, 12, 14, /**/ 4,  0,  2,  6,  8,  10, 12, 14,  //
      0, 2,  4,  6,  8,  10, 12, 14, /**/ 2,  0,  4,  6,  8,  10, 12, 14,  //
      0, 2,  4,  6,  8,  10, 12, 14, /**/ 0,  2,  4,  6,  8,  10, 12, 14};

  const VFromD<decltype(d8t)> byte_idx{Load(d8, table + mask_bits * 8).raw};
  const VFromD<decltype(du)> pairs = ZipLower(byte_idx, byte_idx);
  return BitCast(d, pairs + Set(du, 0x0100));
}

template <class D, HWY_IF_T_SIZE_D(D, 4)>
HWY_INLINE VFromD<D> IndicesFromBits128(D d, uint64_t mask_bits) {
  HWY_DASSERT(mask_bits < 16);

  // There are only 4 lanes, so we can afford to load the index vector directly.
  alignas(16) static constexpr uint8_t u8_indices[256] = {
      // PrintCompress32x4Tables
      0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15,  //
      0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15,  //
      4,  5,  6,  7,  0,  1,  2,  3,  8,  9,  10, 11, 12, 13, 14, 15,  //
      0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15,  //
      8,  9,  10, 11, 0,  1,  2,  3,  4,  5,  6,  7,  12, 13, 14, 15,  //
      0,  1,  2,  3,  8,  9,  10, 11, 4,  5,  6,  7,  12, 13, 14, 15,  //
      4,  5,  6,  7,  8,  9,  10, 11, 0,  1,  2,  3,  12, 13, 14, 15,  //
      0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15,  //
      12, 13, 14, 15, 0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11,  //
      0,  1,  2,  3,  12, 13, 14, 15, 4,  5,  6,  7,  8,  9,  10, 11,  //
      4,  5,  6,  7,  12, 13, 14, 15, 0,  1,  2,  3,  8,  9,  10, 11,  //
      0,  1,  2,  3,  4,  5,  6,  7,  12, 13, 14, 15, 8,  9,  10, 11,  //
      8,  9,  10, 11, 12, 13, 14, 15, 0,  1,  2,  3,  4,  5,  6,  7,   //
      0,  1,  2,  3,  8,  9,  10, 11, 12, 13, 14, 15, 4,  5,  6,  7,   //
      4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 0,  1,  2,  3,   //
      0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15};

  const Repartition<uint8_t, decltype(d)> d8;
  return BitCast(d, Load(d8, u8_indices + 16 * mask_bits));
}

template <class D, HWY_IF_T_SIZE_D(D, 4)>
HWY_INLINE VFromD<D> IndicesFromNotBits128(D d, uint64_t mask_bits) {
  HWY_DASSERT(mask_bits < 16);

  // There are only 4 lanes, so we can afford to load the index vector directly.
  alignas(16) static constexpr uint8_t u8_indices[256] = {
      // PrintCompressNot32x4Tables
      0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 4,  5,
      6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 0,  1,  2,  3,  0,  1,  2,  3,
      8,  9,  10, 11, 12, 13, 14, 15, 4,  5,  6,  7,  8,  9,  10, 11, 12, 13,
      14, 15, 0,  1,  2,  3,  4,  5,  6,  7,  0,  1,  2,  3,  4,  5,  6,  7,
      12, 13, 14, 15, 8,  9,  10, 11, 4,  5,  6,  7,  12, 13, 14, 15, 0,  1,
      2,  3,  8,  9,  10, 11, 0,  1,  2,  3,  12, 13, 14, 15, 4,  5,  6,  7,
      8,  9,  10, 11, 12, 13, 14, 15, 0,  1,  2,  3,  4,  5,  6,  7,  8,  9,
      10, 11, 0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15,
      4,  5,  6,  7,  8,  9,  10, 11, 0,  1,  2,  3,  12, 13, 14, 15, 0,  1,
      2,  3,  8,  9,  10, 11, 4,  5,  6,  7,  12, 13, 14, 15, 8,  9,  10, 11,
      0,  1,  2,  3,  4,  5,  6,  7,  12, 13, 14, 15, 0,  1,  2,  3,  4,  5,
      6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 4,  5,  6,  7,  0,  1,  2,  3,
      8,  9,  10, 11, 12, 13, 14, 15, 0,  1,  2,  3,  4,  5,  6,  7,  8,  9,
      10, 11, 12, 13, 14, 15, 0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11,
      12, 13, 14, 15};

  const Repartition<uint8_t, decltype(d)> d8;
  return BitCast(d, Load(d8, u8_indices + 16 * mask_bits));
}

template <class D, HWY_IF_T_SIZE_D(D, 8)>
HWY_INLINE VFromD<D> IndicesFromBits128(D d, uint64_t mask_bits) {
  HWY_DASSERT(mask_bits < 4);

  // There are only 2 lanes, so we can afford to load the index vector directly.
  alignas(16) static constexpr uint8_t u8_indices[64] = {
      // PrintCompress64x2Tables
      0, 1, 2,  3,  4,  5,  6,  7,  8, 9, 10, 11, 12, 13, 14, 15,
      0, 1, 2,  3,  4,  5,  6,  7,  8, 9, 10, 11, 12, 13, 14, 15,
      8, 9, 10, 11, 12, 13, 14, 15, 0, 1, 2,  3,  4,  5,  6,  7,
      0, 1, 2,  3,  4,  5,  6,  7,  8, 9, 10, 11, 12, 13, 14, 15};

  const Repartition<uint8_t, decltype(d)> d8;
  return BitCast(d, Load(d8, u8_indices + 16 * mask_bits));
}

template <class D, HWY_IF_T_SIZE_D(D, 8)>
HWY_INLINE VFromD<D> IndicesFromNotBits128(D d, uint64_t mask_bits) {
  HWY_DASSERT(mask_bits < 4);

  // There are only 2 lanes, so we can afford to load the index vector directly.
  alignas(16) static constexpr uint8_t u8_indices[64] = {
      // PrintCompressNot64x2Tables
      0, 1, 2,  3,  4,  5,  6,  7,  8, 9, 10, 11, 12, 13, 14, 15,
      8, 9, 10, 11, 12, 13, 14, 15, 0, 1, 2,  3,  4,  5,  6,  7,
      0, 1, 2,  3,  4,  5,  6,  7,  8, 9, 10, 11, 12, 13, 14, 15,
      0, 1, 2,  3,  4,  5,  6,  7,  8, 9, 10, 11, 12, 13, 14, 15};

  const Repartition<uint8_t, decltype(d)> d8;
  return BitCast(d, Load(d8, u8_indices + 16 * mask_bits));
}

template <typename T, size_t N, HWY_IF_NOT_T_SIZE(T, 1)>
HWY_API Vec128<T, N> CompressBits(Vec128<T, N> v, uint64_t mask_bits) {
  const DFromV<decltype(v)> d;
  const RebindToUnsigned<decltype(d)> du;

  HWY_DASSERT(mask_bits < (1ull << N));
  const auto indices = BitCast(du, detail::IndicesFromBits128(d, mask_bits));
  return BitCast(d, TableLookupBytes(BitCast(du, v), indices));
}

template <typename T, size_t N, HWY_IF_NOT_T_SIZE(T, 1)>
HWY_API Vec128<T, N> CompressNotBits(Vec128<T, N> v, uint64_t mask_bits) {
  const DFromV<decltype(v)> d;
  const RebindToUnsigned<decltype(d)> du;

  HWY_DASSERT(mask_bits < (1ull << N));
  const auto indices = BitCast(du, detail::IndicesFromNotBits128(d, mask_bits));
  return BitCast(d, TableLookupBytes(BitCast(du, v), indices));
}

}  // namespace detail

// Single lane: no-op
template <typename T>
HWY_API Vec128<T, 1> Compress(Vec128<T, 1> v, Mask128<T, 1> /*m*/) {
  return v;
}

// Two lanes: conditional swap
template <typename T, HWY_IF_T_SIZE(T, 8)>
HWY_API Vec128<T> Compress(Vec128<T> v, Mask128<T> mask) {
  // If mask[1] = 1 and mask[0] = 0, then swap both halves, else keep.
  const DFromV<decltype(v)> d;
  const Vec128<T> m = VecFromMask(d, mask);
  const Vec128<T> maskL = DupEven(m);
  const Vec128<T> maskH = DupOdd(m);
  const Vec128<T> swap = AndNot(maskL, maskH);
  return IfVecThenElse(swap, Shuffle01(v), v);
}

// General case, 2 or 4 bytes
template <typename T, size_t N, HWY_IF_T_SIZE_ONE_OF(T, (1 << 2) | (1 << 4))>
HWY_API Vec128<T, N> Compress(Vec128<T, N> v, Mask128<T, N> mask) {
  return detail::CompressBits(v, detail::BitsFromMask(mask));
}

// ------------------------------ CompressNot

// Single lane: no-op
template <typename T>
HWY_API Vec128<T, 1> CompressNot(Vec128<T, 1> v, Mask128<T, 1> /*m*/) {
  return v;
}

// Two lanes: conditional swap
template <typename T, HWY_IF_T_SIZE(T, 8)>
HWY_API Vec128<T> CompressNot(Vec128<T> v, Mask128<T> mask) {
  // If mask[1] = 0 and mask[0] = 1, then swap both halves, else keep.
  const DFromV<decltype(v)> d;
  const Vec128<T> m = VecFromMask(d, mask);
  const Vec128<T> maskL = DupEven(m);
  const Vec128<T> maskH = DupOdd(m);
  const Vec128<T> swap = AndNot(maskH, maskL);
  return IfVecThenElse(swap, Shuffle01(v), v);
}

template <typename T, size_t N, HWY_IF_T_SIZE_ONE_OF(T, (1 << 2) | (1 << 4))>
HWY_API Vec128<T, N> CompressNot(Vec128<T, N> v, Mask128<T, N> mask) {
  // For partial vectors, we cannot pull the Not() into the table because
  // BitsFromMask clears the upper bits.
  if (N < 16 / sizeof(T)) {
    return detail::CompressBits(v, detail::BitsFromMask(Not(mask)));
  }
  return detail::CompressNotBits(v, detail::BitsFromMask(mask));
}

// ------------------------------ CompressBlocksNot
HWY_API Vec128<uint64_t> CompressBlocksNot(Vec128<uint64_t> v,
                                           Mask128<uint64_t> /* m */) {
  return v;
}

template <typename T, size_t N, HWY_IF_NOT_T_SIZE(T, 1)>
HWY_API Vec128<T, N> CompressBits(Vec128<T, N> v,
                                  const uint8_t* HWY_RESTRICT bits) {
  uint64_t mask_bits = 0;
  constexpr size_t kNumBytes = (N + 7) / 8;
  CopyBytes<kNumBytes>(bits, &mask_bits);
  if (N < 8) {
    mask_bits &= (1ull << N) - 1;
  }

  return detail::CompressBits(v, mask_bits);
}

// ------------------------------ CompressStore, CompressBitsStore

template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_NOT_T_SIZE_D(D, 1)>
HWY_API size_t CompressStore(VFromD<D> v, MFromD<D> m, D d,
                             TFromD<D>* HWY_RESTRICT unaligned) {
  const RebindToUnsigned<decltype(d)> du;

  const uint64_t mask_bits = detail::BitsFromMask(m);
  HWY_DASSERT(mask_bits < (1ull << MaxLanes(d)));
  const size_t count = PopCount(mask_bits);

  // Avoid _mm_maskmoveu_si128 (>500 cycle latency because it bypasses caches).
  const auto indices = BitCast(du, detail::IndicesFromBits128(d, mask_bits));
  const auto compressed = BitCast(d, TableLookupBytes(BitCast(du, v), indices));
  StoreU(compressed, d, unaligned);
  detail::MaybeUnpoison(unaligned, count);
  return count;
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_NOT_T_SIZE_D(D, 1)>
HWY_API size_t CompressBlendedStore(VFromD<D> v, MFromD<D> m, D d,
                                    TFromD<D>* HWY_RESTRICT unaligned) {
  const RebindToUnsigned<decltype(d)> du;

  const uint64_t mask_bits = detail::BitsFromMask(m);
  HWY_DASSERT(mask_bits < (1ull << MaxLanes(d)));
  const size_t count = PopCount(mask_bits);

  // Avoid _mm_maskmoveu_si128 (>500 cycle latency because it bypasses caches).
  const auto indices = BitCast(du, detail::IndicesFromBits128(d, mask_bits));
  const auto compressed = BitCast(d, TableLookupBytes(BitCast(du, v), indices));
  BlendedStore(compressed, FirstN(d, count), d, unaligned);
  detail::MaybeUnpoison(unaligned, count);
  return count;
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_NOT_T_SIZE_D(D, 1)>
HWY_API size_t CompressBitsStore(VFromD<D> v, const uint8_t* HWY_RESTRICT bits,
                                 D d, TFromD<D>* HWY_RESTRICT unaligned) {
  const RebindToUnsigned<decltype(d)> du;

  uint64_t mask_bits = 0;
  constexpr size_t kN = MaxLanes(d);
  constexpr size_t kNumBytes = (kN + 7) / 8;
  CopyBytes<kNumBytes>(bits, &mask_bits);
  if (kN < 8) {
    mask_bits &= (1ull << kN) - 1;
  }
  const size_t count = PopCount(mask_bits);

  // Avoid _mm_maskmoveu_si128 (>500 cycle latency because it bypasses caches).
  const auto indices = BitCast(du, detail::IndicesFromBits128(d, mask_bits));
  const auto compressed = BitCast(d, TableLookupBytes(BitCast(du, v), indices));
  StoreU(compressed, d, unaligned);

  detail::MaybeUnpoison(unaligned, count);
  return count;
}

#endif  // HWY_TARGET <= HWY_AVX3

// ------------------------------ Expand

// Otherwise, use the generic_ops-inl.h fallback.
#if HWY_TARGET <= HWY_AVX3 || HWY_IDE

// The native instructions for 8/16-bit actually require VBMI2 (HWY_AVX3_DL),
// but we still want to override generic_ops-inl's table-based implementation
// whenever we have the 32-bit expand provided by AVX3.
#ifdef HWY_NATIVE_EXPAND
#undef HWY_NATIVE_EXPAND
#else
#define HWY_NATIVE_EXPAND
#endif

namespace detail {

#if HWY_TARGET <= HWY_AVX3_DL || HWY_IDE  // VBMI2

template <size_t N>
HWY_INLINE Vec128<uint8_t, N> NativeExpand(Vec128<uint8_t, N> v,
                                           Mask128<uint8_t, N> mask) {
  return Vec128<uint8_t, N>{_mm_maskz_expand_epi8(mask.raw, v.raw)};
}

template <size_t N>
HWY_INLINE Vec128<uint16_t, N> NativeExpand(Vec128<uint16_t, N> v,
                                            Mask128<uint16_t, N> mask) {
  return Vec128<uint16_t, N>{_mm_maskz_expand_epi16(mask.raw, v.raw)};
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_U8_D(D)>
HWY_INLINE VFromD<D> NativeLoadExpand(MFromD<D> mask, D /* d */,
                                      const uint8_t* HWY_RESTRICT unaligned) {
  return VFromD<D>{_mm_maskz_expandloadu_epi8(mask.raw, unaligned)};
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_U16_D(D)>
HWY_INLINE VFromD<D> NativeLoadExpand(MFromD<D> mask, D /* d */,
                                      const uint16_t* HWY_RESTRICT unaligned) {
  return VFromD<D>{_mm_maskz_expandloadu_epi16(mask.raw, unaligned)};
}

#endif  // HWY_TARGET <= HWY_AVX3_DL

template <size_t N>
HWY_INLINE Vec128<uint32_t, N> NativeExpand(Vec128<uint32_t, N> v,
                                            Mask128<uint32_t, N> mask) {
  return Vec128<uint32_t, N>{_mm_maskz_expand_epi32(mask.raw, v.raw)};
}

template <size_t N>
HWY_INLINE Vec128<uint64_t, N> NativeExpand(Vec128<uint64_t, N> v,
                                            Mask128<uint64_t, N> mask) {
  return Vec128<uint64_t, N>{_mm_maskz_expand_epi64(mask.raw, v.raw)};
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_U32_D(D)>
HWY_INLINE VFromD<D> NativeLoadExpand(MFromD<D> mask, D /* d */,
                                      const uint32_t* HWY_RESTRICT unaligned) {
  return VFromD<D>{_mm_maskz_expandloadu_epi32(mask.raw, unaligned)};
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 16), HWY_IF_U64_D(D)>
HWY_INLINE VFromD<D> NativeLoadExpand(MFromD<D> mask, D /* d */,
                                      const uint64_t* HWY_RESTRICT unaligned) {
  return VFromD<D>{_mm_maskz_expandloadu_epi64(mask.raw, unaligned)};
}

}  // namespace detail

// Otherwise, 8/16-bit are implemented in x86_512 using PromoteTo.
#if HWY_TARGET <= HWY_AVX3_DL || HWY_IDE  // VBMI2

template <typename T, size_t N, HWY_IF_T_SIZE_ONE_OF(T, (1 << 1) | (1 << 2))>
HWY_API Vec128<T, N> Expand(Vec128<T, N> v, Mask128<T, N> mask) {
  const DFromV<decltype(v)> d;
  const RebindToUnsigned<decltype(d)> du;
  const MFromD<decltype(du)> mu = RebindMask(du, mask);
  return BitCast(d, detail::NativeExpand(BitCast(du, v), mu));
}

#endif  // HWY_TARGET <= HWY_AVX3_DL

template <typename T, size_t N, HWY_IF_T_SIZE_ONE_OF(T, (1 << 4) | (1 << 8))>
HWY_API Vec128<T, N> Expand(Vec128<T, N> v, Mask128<T, N> mask) {
  const DFromV<decltype(v)> d;
  const RebindToUnsigned<decltype(d)> du;
  const MFromD<decltype(du)> mu = RebindMask(du, mask);
  return BitCast(d, detail::NativeExpand(BitCast(du, v), mu));
}

// ------------------------------ LoadExpand

template <class D, HWY_IF_V_SIZE_LE_D(D, 16),
          HWY_IF_T_SIZE_ONE_OF_D(D, (1 << 1) | (1 << 2))>
HWY_API VFromD<D> LoadExpand(MFromD<D> mask, D d,
                             const TFromD<D>* HWY_RESTRICT unaligned) {
#if HWY_TARGET <= HWY_AVX3_DL  // VBMI2
  const RebindToUnsigned<decltype(d)> du;
  using TU = TFromD<decltype(du)>;
  const TU* HWY_RESTRICT pu = reinterpret_cast<const TU*>(unaligned);
  const MFromD<decltype(du)> mu = RebindMask(du, mask);
  return BitCast(d, detail::NativeLoadExpand(mu, du, pu));
#else
  return Expand(LoadU(d, unaligned), mask);
#endif
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 16),
          HWY_IF_T_SIZE_ONE_OF_D(D, (1 << 4) | (1 << 8))>
HWY_API VFromD<D> LoadExpand(MFromD<D> mask, D d,
                             const TFromD<D>* HWY_RESTRICT unaligned) {
#if HWY_TARGET <= HWY_AVX3
  const RebindToUnsigned<decltype(d)> du;
  using TU = TFromD<decltype(du)>;
  const TU* HWY_RESTRICT pu = reinterpret_cast<const TU*>(unaligned);
  const MFromD<decltype(du)> mu = RebindMask(du, mask);
  return BitCast(d, detail::NativeLoadExpand(mu, du, pu));
#else
  return Expand(LoadU(d, unaligned), mask);
#endif
}

#endif  // HWY_TARGET <= HWY_AVX3

// ------------------------------ StoreInterleaved2/3/4

// HWY_NATIVE_LOAD_STORE_INTERLEAVED not set, hence defined in
// generic_ops-inl.h.

// ------------------------------ Reductions

namespace detail {

// N=1 for any T: no-op
template <typename T>
HWY_INLINE Vec128<T, 1> SumOfLanes(Vec128<T, 1> v) {
  return v;
}
template <typename T>
HWY_INLINE T ReduceSum(Vec128<T, 1> v) {
  return GetLane(v);
}
template <typename T>
HWY_INLINE Vec128<T, 1> MinOfLanes(Vec128<T, 1> v) {
  return v;
}
template <typename T>
HWY_INLINE Vec128<T, 1> MaxOfLanes(Vec128<T, 1> v) {
  return v;
}

// u32/i32/f32:

// N=2
template <typename T, HWY_IF_T_SIZE(T, 4)>
HWY_INLINE Vec128<T, 2> SumOfLanes(Vec128<T, 2> v10) {
  return v10 + Shuffle2301(v10);
}
template <typename T, HWY_IF_T_SIZE(T, 4)>
HWY_INLINE T ReduceSum(Vec128<T, 2> v10) {
  return GetLane(SumOfLanes(v10));
}
template <typename T, HWY_IF_T_SIZE(T, 4)>
HWY_INLINE Vec128<T, 2> MinOfLanes(Vec128<T, 2> v10) {
  return Min(v10, Shuffle2301(v10));
}
template <typename T, HWY_IF_T_SIZE(T, 4)>
HWY_INLINE Vec128<T, 2> MaxOfLanes(Vec128<T, 2> v10) {
  return Max(v10, Shuffle2301(v10));
}

// N=4 (full)
template <typename T, HWY_IF_T_SIZE(T, 4)>
HWY_INLINE Vec128<T> SumOfLanes(Vec128<T> v3210) {
  const Vec128<T> v1032 = Shuffle1032(v3210);
  const Vec128<T> v31_20_31_20 = v3210 + v1032;
  const Vec128<T> v20_31_20_31 = Shuffle0321(v31_20_31_20);
  return v20_31_20_31 + v31_20_31_20;
}
template <typename T, HWY_IF_T_SIZE(T, 4)>
HWY_INLINE T ReduceSum(Vec128<T> v3210) {
  return GetLane(SumOfLanes(v3210));
}
template <typename T, HWY_IF_T_SIZE(T, 4)>
HWY_INLINE Vec128<T> MinOfLanes(Vec128<T> v3210) {
  const Vec128<T> v1032 = Shuffle1032(v3210);
  const Vec128<T> v31_20_31_20 = Min(v3210, v1032);
  const Vec128<T> v20_31_20_31 = Shuffle0321(v31_20_31_20);
  return Min(v20_31_20_31, v31_20_31_20);
}
template <typename T, HWY_IF_T_SIZE(T, 4)>
HWY_INLINE Vec128<T> MaxOfLanes(Vec128<T> v3210) {
  const Vec128<T> v1032 = Shuffle1032(v3210);
  const Vec128<T> v31_20_31_20 = Max(v3210, v1032);
  const Vec128<T> v20_31_20_31 = Shuffle0321(v31_20_31_20);
  return Max(v20_31_20_31, v31_20_31_20);
}

// u64/i64/f64:

// N=2 (full)
template <typename T, HWY_IF_T_SIZE(T, 8)>
HWY_INLINE Vec128<T> SumOfLanes(Vec128<T> v10) {
  const Vec128<T> v01 = Shuffle01(v10);
  return v10 + v01;
}
template <typename T, HWY_IF_T_SIZE(T, 8)>
HWY_INLINE T ReduceSum(Vec128<T> v10) {
  return GetLane(SumOfLanes(v10));
}
template <typename T, HWY_IF_T_SIZE(T, 8)>
HWY_INLINE Vec128<T> MinOfLanes(Vec128<T> v10) {
  const Vec128<T> v01 = Shuffle01(v10);
  return Min(v10, v01);
}
template <typename T, HWY_IF_T_SIZE(T, 8)>
HWY_INLINE Vec128<T> MaxOfLanes(Vec128<T> v10) {
  const Vec128<T> v01 = Shuffle01(v10);
  return Max(v10, v01);
}

template <size_t N, HWY_IF_V_SIZE_GT(uint16_t, N, 2)>
HWY_INLINE uint16_t ReduceSum(Vec128<uint16_t, N> v) {
  const DFromV<decltype(v)> d;
  const RepartitionToWide<decltype(d)> d32;
  const auto even = And(BitCast(d32, v), Set(d32, 0xFFFF));
  const auto odd = ShiftRight<16>(BitCast(d32, v));
  const auto sum = ReduceSum(even + odd);
  return static_cast<uint16_t>(sum);
}
template <size_t N, HWY_IF_V_SIZE_GT(uint16_t, N, 2)>
HWY_INLINE Vec128<uint16_t, N> SumOfLanes(Vec128<uint16_t, N> v) {
  const DFromV<decltype(v)> d;
  return Set(d, ReduceSum(v));
}
template <size_t N, HWY_IF_V_SIZE_GT(uint16_t, N, 2)>
HWY_INLINE int16_t ReduceSum(Vec128<int16_t, N> v) {
  const DFromV<decltype(v)> d;
  const RepartitionToWide<decltype(d)> d32;
  // Sign-extend
  const auto even = ShiftRight<16>(ShiftLeft<16>(BitCast(d32, v)));
  const auto odd = ShiftRight<16>(BitCast(d32, v));
  const auto sum = ReduceSum(even + odd);
  return static_cast<int16_t>(sum);
}
template <size_t N, HWY_IF_V_SIZE_GT(uint16_t, N, 2)>
HWY_INLINE Vec128<int16_t, N> SumOfLanes(Vec128<int16_t, N> v) {
  const DFromV<decltype(v)> d;
  return Set(d, ReduceSum(v));
}
// u8, N=8, N=16:
HWY_INLINE uint8_t ReduceSum(Vec64<uint8_t> v) {
  return static_cast<uint8_t>(GetLane(SumsOf8(v)) & 0xFF);
}
HWY_INLINE Vec64<uint8_t> SumOfLanes(Vec64<uint8_t> v) {
  const Full64<uint8_t> d;
  return Set(d, ReduceSum(v));
}
HWY_INLINE uint8_t ReduceSum(Vec128<uint8_t> v) {
  uint64_t sums = ReduceSum(SumsOf8(v));
  return static_cast<uint8_t>(sums & 0xFF);
}
HWY_INLINE Vec128<uint8_t> SumOfLanes(Vec128<uint8_t> v) {
  const DFromV<decltype(v)> d;
  return Set(d, ReduceSum(v));
}
template <size_t N, HWY_IF_V_SIZE_GT(int8_t, N, 4)>
HWY_INLINE int8_t ReduceSum(const Vec128<int8_t, N> v) {
  const DFromV<decltype(v)> d;
  const RebindToUnsigned<decltype(d)> du;
  const auto is_neg = v < Zero(d);

  // Sum positive and negative lanes separately, then combine to get the result.
  const auto positive = SumsOf8(BitCast(du, IfThenZeroElse(is_neg, v)));
  const auto negative = SumsOf8(BitCast(du, IfThenElseZero(is_neg, Abs(v))));
  return static_cast<int8_t>(ReduceSum(positive - negative) & 0xFF);
}
template <size_t N, HWY_IF_V_SIZE_GT(int8_t, N, 4)>
HWY_INLINE Vec128<int8_t, N> SumOfLanes(const Vec128<int8_t, N> v) {
  const DFromV<decltype(v)> d;
  return Set(d, ReduceSum(v));
}

#if HWY_TARGET <= HWY_SSE4
HWY_INLINE Vec128<uint16_t> MinOfLanes(Vec128<uint16_t> v) {
  using V = decltype(v);
  return Broadcast<0>(V{_mm_minpos_epu16(v.raw)});
}
HWY_INLINE Vec64<uint8_t> MinOfLanes(Vec64<uint8_t> v) {
  const DFromV<decltype(v)> d;
  const Rebind<uint16_t, decltype(d)> d16;
  return TruncateTo(d, MinOfLanes(PromoteTo(d16, v)));
}
HWY_INLINE Vec128<uint8_t> MinOfLanes(Vec128<uint8_t> v) {
  const Half<DFromV<decltype(v)>> d;
  Vec64<uint8_t> result =
      Min(MinOfLanes(UpperHalf(d, v)), MinOfLanes(LowerHalf(d, v)));
  return Combine(DFromV<decltype(v)>(), result, result);
}

HWY_INLINE Vec128<uint16_t> MaxOfLanes(Vec128<uint16_t> v) {
  const Vec128<uint16_t> m(Set(DFromV<decltype(v)>(), LimitsMax<uint16_t>()));
  return m - MinOfLanes(m - v);
}
HWY_INLINE Vec64<uint8_t> MaxOfLanes(Vec64<uint8_t> v) {
  const Vec64<uint8_t> m(Set(DFromV<decltype(v)>(), LimitsMax<uint8_t>()));
  return m - MinOfLanes(m - v);
}
HWY_INLINE Vec128<uint8_t> MaxOfLanes(Vec128<uint8_t> v) {
  const Vec128<uint8_t> m(Set(DFromV<decltype(v)>(), LimitsMax<uint8_t>()));
  return m - MinOfLanes(m - v);
}
#elif HWY_TARGET >= HWY_SSSE3
template <size_t N, HWY_IF_V_SIZE_GT(uint8_t, N, 4)>
HWY_API Vec128<uint8_t, N> MaxOfLanes(Vec128<uint8_t, N> v) {
  const DFromV<decltype(v)> d;
  const RepartitionToWide<decltype(d)> d16;
  const RepartitionToWide<decltype(d16)> d32;
  Vec128<uint8_t, N> vm = Max(v, Reverse2(d, v));
  vm = Max(vm, BitCast(d, Reverse2(d16, BitCast(d16, vm))));
  vm = Max(vm, BitCast(d, Reverse2(d32, BitCast(d32, vm))));
  if (N > 8) {
    const RepartitionToWide<decltype(d32)> d64;
    vm = Max(vm, BitCast(d, Reverse2(d64, BitCast(d64, vm))));
  }
  return vm;
}

template <size_t N, HWY_IF_V_SIZE_GT(uint8_t, N, 4)>
HWY_API Vec128<uint8_t, N> MinOfLanes(Vec128<uint8_t, N> v) {
  const DFromV<decltype(v)> d;
  const RepartitionToWide<decltype(d)> d16;
  const RepartitionToWide<decltype(d16)> d32;
  Vec128<uint8_t, N> vm = Min(v, Reverse2(d, v));
  vm = Min(vm, BitCast(d, Reverse2(d16, BitCast(d16, vm))));
  vm = Min(vm, BitCast(d, Reverse2(d32, BitCast(d32, vm))));
  if (N > 8) {
    const RepartitionToWide<decltype(d32)> d64;
    vm = Min(vm, BitCast(d, Reverse2(d64, BitCast(d64, vm))));
  }
  return vm;
}
#endif

// Implement min/max of i8 in terms of u8 by toggling the sign bit.
template <size_t N, HWY_IF_V_SIZE_GT(int8_t, N, 4)>
HWY_INLINE Vec128<int8_t, N> MinOfLanes(Vec128<int8_t, N> v) {
  const DFromV<decltype(v)> d;
  const RebindToUnsigned<decltype(d)> du;
  const auto mask = SignBit(du);
  const auto vu = Xor(BitCast(du, v), mask);
  return BitCast(d, Xor(MinOfLanes(vu), mask));
}
template <size_t N, HWY_IF_V_SIZE_GT(int8_t, N, 4)>
HWY_INLINE Vec128<int8_t, N> MaxOfLanes(Vec128<int8_t, N> v) {
  const DFromV<decltype(v)> d;
  const RebindToUnsigned<decltype(d)> du;
  const auto mask = SignBit(du);
  const auto vu = Xor(BitCast(du, v), mask);
  return BitCast(d, Xor(MaxOfLanes(vu), mask));
}

template <size_t N, HWY_IF_V_SIZE_GT(uint16_t, N, 2)>
HWY_INLINE Vec128<uint16_t, N> MinOfLanes(Vec128<uint16_t, N> v) {
  const DFromV<decltype(v)> d;
  const RepartitionToWide<decltype(d)> d32;
  const auto even = And(BitCast(d32, v), Set(d32, 0xFFFF));
  const auto odd = ShiftRight<16>(BitCast(d32, v));
  const auto min = MinOfLanes(Min(even, odd));
  // Also broadcast into odd lanes.
  return OddEven(BitCast(d, ShiftLeft<16>(min)), BitCast(d, min));
}
template <size_t N, HWY_IF_V_SIZE_GT(int16_t, N, 2)>
HWY_INLINE Vec128<int16_t, N> MinOfLanes(Vec128<int16_t, N> v) {
  const DFromV<decltype(v)> d;
  const RepartitionToWide<decltype(d)> d32;
  // Sign-extend
  const auto even = ShiftRight<16>(ShiftLeft<16>(BitCast(d32, v)));
  const auto odd = ShiftRight<16>(BitCast(d32, v));
  const auto min = MinOfLanes(Min(even, odd));
  // Also broadcast into odd lanes.
  return OddEven(BitCast(d, ShiftLeft<16>(min)), BitCast(d, min));
}

template <size_t N, HWY_IF_V_SIZE_GT(uint16_t, N, 2)>
HWY_INLINE Vec128<uint16_t, N> MaxOfLanes(Vec128<uint16_t, N> v) {
  const DFromV<decltype(v)> d;
  const RepartitionToWide<decltype(d)> d32;
  const auto even = And(BitCast(d32, v), Set(d32, 0xFFFF));
  const auto odd = ShiftRight<16>(BitCast(d32, v));
  const auto min = MaxOfLanes(Max(even, odd));
  // Also broadcast into odd lanes.
  return OddEven(BitCast(d, ShiftLeft<16>(min)), BitCast(d, min));
}
template <size_t N, HWY_IF_V_SIZE_GT(int16_t, N, 2)>
HWY_INLINE Vec128<int16_t, N> MaxOfLanes(Vec128<int16_t, N> v) {
  const DFromV<decltype(v)> d;
  const RepartitionToWide<decltype(d)> d32;
  // Sign-extend
  const auto even = ShiftRight<16>(ShiftLeft<16>(BitCast(d32, v)));
  const auto odd = ShiftRight<16>(BitCast(d32, v));
  const auto min = MaxOfLanes(Max(even, odd));
  // Also broadcast into odd lanes.
  return OddEven(BitCast(d, ShiftLeft<16>(min)), BitCast(d, min));
}

}  // namespace detail

// Supported for u/i/f 32/64. Returns the same value in each lane.
template <class D, HWY_IF_V_SIZE_LE_D(D, 16)>
HWY_API VFromD<D> SumOfLanes(D /* tag */, VFromD<D> v) {
  return detail::SumOfLanes(v);
}
template <class D, HWY_IF_V_SIZE_LE_D(D, 16)>
HWY_API TFromD<D> ReduceSum(D /* tag */, VFromD<D> v) {
  return detail::ReduceSum(v);
}
template <class D, HWY_IF_V_SIZE_LE_D(D, 16)>
HWY_API VFromD<D> MinOfLanes(D /* tag */, VFromD<D> v) {
  return detail::MinOfLanes(v);
}
template <class D, HWY_IF_V_SIZE_LE_D(D, 16)>
HWY_API VFromD<D> MaxOfLanes(D /* tag */, VFromD<D> v) {
  return detail::MaxOfLanes(v);
}

// ------------------------------ Lt128

namespace detail {

// Returns vector-mask for Lt128. Also used by x86_256/x86_512.
template <class D, class V = VFromD<D>>
HWY_INLINE V Lt128Vec(const D d, const V a, const V b) {
  static_assert(IsSame<TFromD<D>, uint64_t>(), "D must be u64");
  // Truth table of Eq and Lt for Hi and Lo u64.
  // (removed lines with (=H && cH) or (=L && cL) - cannot both be true)
  // =H =L cH cL  | out = cH | (=H & cL)
  //  0  0  0  0  |  0
  //  0  0  0  1  |  0
  //  0  0  1  0  |  1
  //  0  0  1  1  |  1
  //  0  1  0  0  |  0
  //  0  1  0  1  |  0
  //  0  1  1  0  |  1
  //  1  0  0  0  |  0
  //  1  0  0  1  |  1
  //  1  1  0  0  |  0
  const auto eqHL = Eq(a, b);
  const V ltHL = VecFromMask(d, Lt(a, b));
  const V ltLX = ShiftLeftLanes<1>(ltHL);
  const V vecHx = IfThenElse(eqHL, ltLX, ltHL);
  return InterleaveUpper(d, vecHx, vecHx);
}

// Returns vector-mask for Eq128. Also used by x86_256/x86_512.
template <class D, class V = VFromD<D>>
HWY_INLINE V Eq128Vec(const D d, const V a, const V b) {
  static_assert(IsSame<TFromD<D>, uint64_t>(), "D must be u64");
  const auto eqHL = VecFromMask(d, Eq(a, b));
  const auto eqLH = Reverse2(d, eqHL);
  return And(eqHL, eqLH);
}

template <class D, class V = VFromD<D>>
HWY_INLINE V Ne128Vec(const D d, const V a, const V b) {
  static_assert(IsSame<TFromD<D>, uint64_t>(), "D must be u64");
  const auto neHL = VecFromMask(d, Ne(a, b));
  const auto neLH = Reverse2(d, neHL);
  return Or(neHL, neLH);
}

template <class D, class V = VFromD<D>>
HWY_INLINE V Lt128UpperVec(const D d, const V a, const V b) {
  // No specialization required for AVX-512: Mask <-> Vec is fast, and
  // copying mask bits to their neighbor seems infeasible.
  const V ltHL = VecFromMask(d, Lt(a, b));
  return InterleaveUpper(d, ltHL, ltHL);
}

template <class D, class V = VFromD<D>>
HWY_INLINE V Eq128UpperVec(const D d, const V a, const V b) {
  // No specialization required for AVX-512: Mask <-> Vec is fast, and
  // copying mask bits to their neighbor seems infeasible.
  const V eqHL = VecFromMask(d, Eq(a, b));
  return InterleaveUpper(d, eqHL, eqHL);
}

template <class D, class V = VFromD<D>>
HWY_INLINE V Ne128UpperVec(const D d, const V a, const V b) {
  // No specialization required for AVX-512: Mask <-> Vec is fast, and
  // copying mask bits to their neighbor seems infeasible.
  const V neHL = VecFromMask(d, Ne(a, b));
  return InterleaveUpper(d, neHL, neHL);
}

}  // namespace detail

template <class D, class V = VFromD<D>>
HWY_API MFromD<D> Lt128(D d, const V a, const V b) {
  return MaskFromVec(detail::Lt128Vec(d, a, b));
}

template <class D, class V = VFromD<D>>
HWY_API MFromD<D> Eq128(D d, const V a, const V b) {
  return MaskFromVec(detail::Eq128Vec(d, a, b));
}

template <class D, class V = VFromD<D>>
HWY_API MFromD<D> Ne128(D d, const V a, const V b) {
  return MaskFromVec(detail::Ne128Vec(d, a, b));
}

template <class D, class V = VFromD<D>>
HWY_API MFromD<D> Lt128Upper(D d, const V a, const V b) {
  return MaskFromVec(detail::Lt128UpperVec(d, a, b));
}

template <class D, class V = VFromD<D>>
HWY_API MFromD<D> Eq128Upper(D d, const V a, const V b) {
  return MaskFromVec(detail::Eq128UpperVec(d, a, b));
}

template <class D, class V = VFromD<D>>
HWY_API MFromD<D> Ne128Upper(D d, const V a, const V b) {
  return MaskFromVec(detail::Ne128UpperVec(d, a, b));
}

// ------------------------------ Min128, Max128 (Lt128)

// Avoids the extra MaskFromVec in Lt128.
template <class D, class V = VFromD<D>>
HWY_API V Min128(D d, const V a, const V b) {
  return IfVecThenElse(detail::Lt128Vec(d, a, b), a, b);
}

template <class D, class V = VFromD<D>>
HWY_API V Max128(D d, const V a, const V b) {
  return IfVecThenElse(detail::Lt128Vec(d, b, a), a, b);
}

template <class D, class V = VFromD<D>>
HWY_API V Min128Upper(D d, const V a, const V b) {
  return IfVecThenElse(detail::Lt128UpperVec(d, a, b), a, b);
}

template <class D, class V = VFromD<D>>
HWY_API V Max128Upper(D d, const V a, const V b) {
  return IfVecThenElse(detail::Lt128UpperVec(d, b, a), a, b);
}

// -------------------- LeadingZeroCount, TrailingZeroCount, HighestSetBitIndex

#if HWY_TARGET <= HWY_AVX3

#ifdef HWY_NATIVE_LEADING_ZERO_COUNT
#undef HWY_NATIVE_LEADING_ZERO_COUNT
#else
#define HWY_NATIVE_LEADING_ZERO_COUNT
#endif

template <class V, HWY_IF_UI32(TFromV<V>), HWY_IF_V_SIZE_LE_D(DFromV<V>, 16)>
HWY_API V LeadingZeroCount(V v) {
  return V{_mm_lzcnt_epi32(v.raw)};
}

template <class V, HWY_IF_UI64(TFromV<V>), HWY_IF_V_SIZE_LE_D(DFromV<V>, 16)>
HWY_API V LeadingZeroCount(V v) {
  return V{_mm_lzcnt_epi64(v.raw)};
}

// HighestSetBitIndex and TrailingZeroCount is implemented in x86_512-inl.h
// for AVX3 targets

#endif  // HWY_TARGET <= HWY_AVX3

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace hwy
HWY_AFTER_NAMESPACE();

// Note that the GCC warnings are not suppressed if we only wrap the *intrin.h -
// the warning seems to be issued at the call site of intrinsics, i.e. our code.
HWY_DIAGNOSTICS(pop)
