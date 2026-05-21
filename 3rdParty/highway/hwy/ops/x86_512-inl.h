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

// 512-bit AVX512 vectors and operations.
// External include guard in highway.h - see comment there.

// WARNING: most operations do not cross 128-bit block boundaries. In
// particular, "Broadcast", pack and zip behavior may be surprising.

// Must come before HWY_DIAGNOSTICS and HWY_COMPILER_CLANGCL
#include "hwy/base.h"

// Avoid uninitialized warnings in GCC's avx512fintrin.h - see
// https://github.com/google/highway/issues/710)
HWY_DIAGNOSTICS(push)
#if HWY_COMPILER_GCC_ACTUAL
HWY_DIAGNOSTICS_OFF(disable : 4700, ignored "-Wuninitialized")
HWY_DIAGNOSTICS_OFF(disable : 4701 4703 6001 26494,
                    ignored "-Wmaybe-uninitialized")
#endif

#include <immintrin.h>  // AVX2+

#if HWY_COMPILER_CLANGCL
// Including <immintrin.h> should be enough, but Clang's headers helpfully skip
// including these headers when _MSC_VER is defined, like when using clang-cl.
// Include these directly here.
// clang-format off
#include <smmintrin.h>

#include <avxintrin.h>
// avxintrin defines __m256i and must come before avx2intrin.
#include <avx2intrin.h>
#include <f16cintrin.h>
#include <fmaintrin.h>

#include <avx512fintrin.h>
#include <avx512vlintrin.h>
#include <avx512bwintrin.h>
#include <avx512vlbwintrin.h>
#include <avx512dqintrin.h>
#include <avx512vldqintrin.h>
#include <avx512cdintrin.h>
#include <avx512vlcdintrin.h>

#if HWY_TARGET <= HWY_AVX3_DL
#include <avx512bitalgintrin.h>
#include <avx512vlbitalgintrin.h>
#include <avx512vbmiintrin.h>
#include <avx512vbmivlintrin.h>
#include <avx512vbmi2intrin.h>
#include <avx512vlvbmi2intrin.h>
#include <avx512vpopcntdqintrin.h>
#include <avx512vpopcntdqvlintrin.h>
#include <avx512vnniintrin.h>
#include <avx512vlvnniintrin.h>
// Must come after avx512fintrin, else will not define 512-bit intrinsics.
#include <vaesintrin.h>
#include <vpclmulqdqintrin.h>
#include <gfniintrin.h>
#endif  // HWY_TARGET <= HWY_AVX3_DL
// clang-format on
#endif  // HWY_COMPILER_CLANGCL

// For half-width vectors. Already includes base.h and shared-inl.h.
#include "hwy/ops/x86_256-inl.h"

HWY_BEFORE_NAMESPACE();
namespace hwy {
namespace HWY_NAMESPACE {

namespace detail {

template <typename T>
struct Raw512 {
  using type = __m512i;
};
template <>
struct Raw512<float> {
  using type = __m512;
};
template <>
struct Raw512<double> {
  using type = __m512d;
};

// Template arg: sizeof(lane type)
template <size_t size>
struct RawMask512 {};
template <>
struct RawMask512<1> {
  using type = __mmask64;
};
template <>
struct RawMask512<2> {
  using type = __mmask32;
};
template <>
struct RawMask512<4> {
  using type = __mmask16;
};
template <>
struct RawMask512<8> {
  using type = __mmask8;
};

}  // namespace detail

template <typename T>
class Vec512 {
  using Raw = typename detail::Raw512<T>::type;

 public:
  using PrivateT = T;                                  // only for DFromV
  static constexpr size_t kPrivateN = 64 / sizeof(T);  // only for DFromV

  // Compound assignment. Only usable if there is a corresponding non-member
  // binary operator overload. For example, only f32 and f64 support division.
  HWY_INLINE Vec512& operator*=(const Vec512 other) {
    return *this = (*this * other);
  }
  HWY_INLINE Vec512& operator/=(const Vec512 other) {
    return *this = (*this / other);
  }
  HWY_INLINE Vec512& operator+=(const Vec512 other) {
    return *this = (*this + other);
  }
  HWY_INLINE Vec512& operator-=(const Vec512 other) {
    return *this = (*this - other);
  }
  HWY_INLINE Vec512& operator&=(const Vec512 other) {
    return *this = (*this & other);
  }
  HWY_INLINE Vec512& operator|=(const Vec512 other) {
    return *this = (*this | other);
  }
  HWY_INLINE Vec512& operator^=(const Vec512 other) {
    return *this = (*this ^ other);
  }

  Raw raw;
};

// Mask register: one bit per lane.
template <typename T>
struct Mask512 {
  using Raw = typename detail::RawMask512<sizeof(T)>::type;
  Raw raw;
};

template <typename T>
using Full512 = Simd<T, 64 / sizeof(T), 0>;

// ------------------------------ BitCast

namespace detail {

HWY_INLINE __m512i BitCastToInteger(__m512i v) { return v; }
HWY_INLINE __m512i BitCastToInteger(__m512 v) { return _mm512_castps_si512(v); }
HWY_INLINE __m512i BitCastToInteger(__m512d v) {
  return _mm512_castpd_si512(v);
}

template <typename T>
HWY_INLINE Vec512<uint8_t> BitCastToByte(Vec512<T> v) {
  return Vec512<uint8_t>{BitCastToInteger(v.raw)};
}

// Cannot rely on function overloading because return types differ.
template <typename T>
struct BitCastFromInteger512 {
  HWY_INLINE __m512i operator()(__m512i v) { return v; }
};
template <>
struct BitCastFromInteger512<float> {
  HWY_INLINE __m512 operator()(__m512i v) { return _mm512_castsi512_ps(v); }
};
template <>
struct BitCastFromInteger512<double> {
  HWY_INLINE __m512d operator()(__m512i v) { return _mm512_castsi512_pd(v); }
};

template <class D, typename T = TFromD<D>>
HWY_INLINE Vec512<T> BitCastFromByte(D /* tag */, Vec512<uint8_t> v) {
  return Vec512<T>{BitCastFromInteger512<T>()(v.raw)};
}

}  // namespace detail

template <class D, typename T = TFromD<D>, typename FromT>
HWY_API Vec512<T> BitCast(D d, Vec512<FromT> v) {
  return detail::BitCastFromByte(d, detail::BitCastToByte(v));
}

// ------------------------------ Set

template <class D, HWY_IF_V_SIZE_D(D, 64), HWY_IF_T_SIZE_D(D, 1)>
HWY_API VFromD<D> Set(D /* tag */, TFromD<D> t) {
  return VFromD<D>{_mm512_set1_epi8(static_cast<char>(t))};  // NOLINT
}
template <class D, HWY_IF_V_SIZE_D(D, 64), HWY_IF_T_SIZE_D(D, 2)>
HWY_API VFromD<D> Set(D /* tag */, TFromD<D> t) {
  return VFromD<D>{_mm512_set1_epi16(static_cast<short>(t))};  // NOLINT
}
template <class D, HWY_IF_V_SIZE_D(D, 64), HWY_IF_UI32_D(D)>
HWY_API VFromD<D> Set(D /* tag */, TFromD<D> t) {
  return VFromD<D>{_mm512_set1_epi32(static_cast<int>(t))};
}
template <class D, HWY_IF_V_SIZE_D(D, 64), HWY_IF_UI64_D(D)>
HWY_API VFromD<D> Set(D /* tag */, TFromD<D> t) {
  return VFromD<D>{_mm512_set1_epi64(static_cast<long long>(t))};  // NOLINT
}
template <class D, HWY_IF_V_SIZE_D(D, 64), HWY_IF_F32_D(D)>
HWY_API Vec512<float> Set(D /* tag */, float t) {
  return Vec512<float>{_mm512_set1_ps(t)};
}
template <class D, HWY_IF_V_SIZE_D(D, 64), HWY_IF_F64_D(D)>
HWY_API Vec512<double> Set(D /* tag */, double t) {
  return Vec512<double>{_mm512_set1_pd(t)};
}

// ------------------------------ Zero (Set)

// GCC pre-9.1 lacked setzero, so use Set instead.
#if HWY_COMPILER_GCC_ACTUAL && HWY_COMPILER_GCC_ACTUAL < 900

template <class D, HWY_IF_V_SIZE_D(D, 64)>
HWY_API Vec512<TFromD<D>> Zero(D d) {
  return Set(d, TFromD<D>{0});
}

#else

template <class D, HWY_IF_V_SIZE_D(D, 64), HWY_IF_NOT_FLOAT_D(D)>
HWY_API Vec512<TFromD<D>> Zero(D /* tag */) {
  return Vec512<TFromD<D>>{_mm512_setzero_si512()};
}
template <class D, HWY_IF_V_SIZE_D(D, 64), HWY_IF_F32_D(D)>
HWY_API Vec512<float> Zero(D /* tag */) {
  return Vec512<float>{_mm512_setzero_ps()};
}
template <class D, HWY_IF_V_SIZE_D(D, 64), HWY_IF_F64_D(D)>
HWY_API Vec512<double> Zero(D /* tag */) {
  return Vec512<double>{_mm512_setzero_pd()};
}

#endif  // HWY_COMPILER_GCC_ACTUAL && HWY_COMPILER_GCC_ACTUAL < 900

// ------------------------------ Undefined

HWY_DIAGNOSTICS(push)
HWY_DIAGNOSTICS_OFF(disable : 4700, ignored "-Wuninitialized")

// Returns a vector with uninitialized elements.
template <class D, HWY_IF_V_SIZE_D(D, 64), HWY_IF_NOT_FLOAT_D(D)>
HWY_API Vec512<TFromD<D>> Undefined(D /* tag */) {
  // Available on Clang 6.0, GCC 6.2, ICC 16.03, MSVC 19.14. All but ICC
  // generate an XOR instruction.
  return Vec512<TFromD<D>>{_mm512_undefined_epi32()};
}
template <class D, HWY_IF_V_SIZE_D(D, 64), HWY_IF_F32_D(D)>
HWY_API Vec512<float> Undefined(D /* tag */) {
  return Vec512<float>{_mm512_undefined_ps()};
}
template <class D, HWY_IF_V_SIZE_D(D, 64), HWY_IF_F64_D(D)>
HWY_API Vec512<double> Undefined(D /* tag */) {
  return Vec512<double>{_mm512_undefined_pd()};
}

HWY_DIAGNOSTICS(pop)

// ------------------------------ ResizeBitCast

// 64-byte vector to 16-byte vector
template <class D, class FromV, HWY_IF_V_SIZE_V(FromV, 64),
          HWY_IF_V_SIZE_D(D, 16)>
HWY_API VFromD<D> ResizeBitCast(D d, FromV v) {
  return BitCast(d, Vec128<uint8_t>{_mm512_castsi512_si128(
                        BitCast(Full512<uint8_t>(), v).raw)});
}

// <= 16-byte vector to 64-byte vector
template <class D, class FromV, HWY_IF_V_SIZE_LE_V(FromV, 16),
          HWY_IF_V_SIZE_D(D, 64)>
HWY_API VFromD<D> ResizeBitCast(D d, FromV v) {
  return BitCast(d, Vec512<uint8_t>{_mm512_castsi128_si512(
                        ResizeBitCast(Full128<uint8_t>(), v).raw)});
}

// 32-byte vector to 64-byte vector
template <class D, class FromV, HWY_IF_V_SIZE_V(FromV, 32),
          HWY_IF_V_SIZE_D(D, 64)>
HWY_API VFromD<D> ResizeBitCast(D d, FromV v) {
  return BitCast(d, Vec512<uint8_t>{_mm512_castsi256_si512(
                        BitCast(Full256<uint8_t>(), v).raw)});
}

// ----------------------------- Iota

namespace detail {

template <class D, HWY_IF_V_SIZE_D(D, 64), HWY_IF_T_SIZE_D(D, 1)>
HWY_INLINE VFromD<D> Iota0(D d) {
#if HWY_COMPILER_GCC_ACTUAL && HWY_COMPILER_GCC_ACTUAL < 900
  // Missing set_epi8/16.
  alignas(64) static constexpr TFromD<D> kIota[64] = {
      0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15,
      16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
      32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
      48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63};
  return Load(d, kIota);
#else
  (void)d;
  return VFromD<D>{_mm512_set_epi8(
      static_cast<char>(63), static_cast<char>(62), static_cast<char>(61),
      static_cast<char>(60), static_cast<char>(59), static_cast<char>(58),
      static_cast<char>(57), static_cast<char>(56), static_cast<char>(55),
      static_cast<char>(54), static_cast<char>(53), static_cast<char>(52),
      static_cast<char>(51), static_cast<char>(50), static_cast<char>(49),
      static_cast<char>(48), static_cast<char>(47), static_cast<char>(46),
      static_cast<char>(45), static_cast<char>(44), static_cast<char>(43),
      static_cast<char>(42), static_cast<char>(41), static_cast<char>(40),
      static_cast<char>(39), static_cast<char>(38), static_cast<char>(37),
      static_cast<char>(36), static_cast<char>(35), static_cast<char>(34),
      static_cast<char>(33), static_cast<char>(32), static_cast<char>(31),
      static_cast<char>(30), static_cast<char>(29), static_cast<char>(28),
      static_cast<char>(27), static_cast<char>(26), static_cast<char>(25),
      static_cast<char>(24), static_cast<char>(23), static_cast<char>(22),
      static_cast<char>(21), static_cast<char>(20), static_cast<char>(19),
      static_cast<char>(18), static_cast<char>(17), static_cast<char>(16),
      static_cast<char>(15), static_cast<char>(14), static_cast<char>(13),
      static_cast<char>(12), static_cast<char>(11), static_cast<char>(10),
      static_cast<char>(9), static_cast<char>(8), static_cast<char>(7),
      static_cast<char>(6), static_cast<char>(5), static_cast<char>(4),
      static_cast<char>(3), static_cast<char>(2), static_cast<char>(1),
      static_cast<char>(0))};
#endif
}

template <class D, HWY_IF_V_SIZE_D(D, 64), HWY_IF_T_SIZE_D(D, 2),
          HWY_IF_NOT_SPECIAL_FLOAT_D(D)>
HWY_INLINE VFromD<D> Iota0(D /*d*/) {
#if HWY_COMPILER_GCC_ACTUAL && HWY_COMPILER_GCC_ACTUAL < 900
  // Missing set_epi8/16.
  alignas(64) static constexpr TFromD<D> kIota[32] = {
      0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15,
      16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31};
  return Load(d, kIota);
#else
  return VFromD<D>{_mm512_set_epi16(
      int16_t{31}, int16_t{30}, int16_t{29}, int16_t{28}, int16_t{27},
      int16_t{26}, int16_t{25}, int16_t{24}, int16_t{23}, int16_t{22},
      int16_t{21}, int16_t{20}, int16_t{19}, int16_t{18}, int16_t{17},
      int16_t{16}, int16_t{15}, int16_t{14}, int16_t{13}, int16_t{12},
      int16_t{11}, int16_t{10}, int16_t{9}, int16_t{8}, int16_t{7}, int16_t{6},
      int16_t{5}, int16_t{4}, int16_t{3}, int16_t{2}, int16_t{1}, int16_t{0})};
#endif
}

template <class D, HWY_IF_V_SIZE_D(D, 64), HWY_IF_UI32_D(D)>
HWY_INLINE VFromD<D> Iota0(D /*d*/) {
  return VFromD<D>{_mm512_set_epi32(
      int32_t{15}, int32_t{14}, int32_t{13}, int32_t{12}, int32_t{11},
      int32_t{10}, int32_t{9}, int32_t{8}, int32_t{7}, int32_t{6}, int32_t{5},
      int32_t{4}, int32_t{3}, int32_t{2}, int32_t{1}, int32_t{0})};
}

template <class D, HWY_IF_V_SIZE_D(D, 64), HWY_IF_UI64_D(D)>
HWY_INLINE VFromD<D> Iota0(D /*d*/) {
  return VFromD<D>{_mm512_set_epi64(int64_t{7}, int64_t{6}, int64_t{5},
                                    int64_t{4}, int64_t{3}, int64_t{2},
                                    int64_t{1}, int64_t{0})};
}

template <class D, HWY_IF_V_SIZE_D(D, 64), HWY_IF_F32_D(D)>
HWY_INLINE VFromD<D> Iota0(D /*d*/) {
  return VFromD<D>{_mm512_set_ps(15.0f, 14.0f, 13.0f, 12.0f, 11.0f, 10.0f, 9.0f,
                                 8.0f, 7.0f, 6.0f, 5.0f, 4.0f, 3.0f, 2.0f, 1.0f,
                                 0.0f)};
}

template <class D, HWY_IF_V_SIZE_D(D, 64), HWY_IF_F64_D(D)>
HWY_INLINE VFromD<D> Iota0(D /*d*/) {
  return VFromD<D>{_mm512_set_pd(7.0, 6.0, 5.0, 4.0, 3.0, 2.0, 1.0, 0.0)};
}

}  // namespace detail

template <class D, typename T2, HWY_IF_V_SIZE_D(D, 64)>
HWY_API VFromD<D> Iota(D d, const T2 first) {
  return detail::Iota0(d) + Set(d, static_cast<TFromD<D>>(first));
}

// ================================================== LOGICAL

// ------------------------------ Not

template <typename T>
HWY_API Vec512<T> Not(const Vec512<T> v) {
  const DFromV<decltype(v)> d;
  const RebindToUnsigned<decltype(d)> du;
  using VU = VFromD<decltype(du)>;
  const __m512i vu = BitCast(du, v).raw;
  return BitCast(d, VU{_mm512_ternarylogic_epi32(vu, vu, vu, 0x55)});
}

// ------------------------------ And

template <typename T>
HWY_API Vec512<T> And(const Vec512<T> a, const Vec512<T> b) {
  return Vec512<T>{_mm512_and_si512(a.raw, b.raw)};
}

HWY_API Vec512<float> And(const Vec512<float> a, const Vec512<float> b) {
  return Vec512<float>{_mm512_and_ps(a.raw, b.raw)};
}
HWY_API Vec512<double> And(const Vec512<double> a, const Vec512<double> b) {
  return Vec512<double>{_mm512_and_pd(a.raw, b.raw)};
}

// ------------------------------ AndNot

// Returns ~not_mask & mask.
template <typename T>
HWY_API Vec512<T> AndNot(const Vec512<T> not_mask, const Vec512<T> mask) {
  return Vec512<T>{_mm512_andnot_si512(not_mask.raw, mask.raw)};
}
HWY_API Vec512<float> AndNot(const Vec512<float> not_mask,
                             const Vec512<float> mask) {
  return Vec512<float>{_mm512_andnot_ps(not_mask.raw, mask.raw)};
}
HWY_API Vec512<double> AndNot(const Vec512<double> not_mask,
                              const Vec512<double> mask) {
  return Vec512<double>{_mm512_andnot_pd(not_mask.raw, mask.raw)};
}

// ------------------------------ Or

template <typename T>
HWY_API Vec512<T> Or(const Vec512<T> a, const Vec512<T> b) {
  return Vec512<T>{_mm512_or_si512(a.raw, b.raw)};
}

HWY_API Vec512<float> Or(const Vec512<float> a, const Vec512<float> b) {
  return Vec512<float>{_mm512_or_ps(a.raw, b.raw)};
}
HWY_API Vec512<double> Or(const Vec512<double> a, const Vec512<double> b) {
  return Vec512<double>{_mm512_or_pd(a.raw, b.raw)};
}

// ------------------------------ Xor

template <typename T>
HWY_API Vec512<T> Xor(const Vec512<T> a, const Vec512<T> b) {
  return Vec512<T>{_mm512_xor_si512(a.raw, b.raw)};
}

HWY_API Vec512<float> Xor(const Vec512<float> a, const Vec512<float> b) {
  return Vec512<float>{_mm512_xor_ps(a.raw, b.raw)};
}
HWY_API Vec512<double> Xor(const Vec512<double> a, const Vec512<double> b) {
  return Vec512<double>{_mm512_xor_pd(a.raw, b.raw)};
}

// ------------------------------ Xor3
template <typename T>
HWY_API Vec512<T> Xor3(Vec512<T> x1, Vec512<T> x2, Vec512<T> x3) {
  const DFromV<decltype(x1)> d;
  const RebindToUnsigned<decltype(d)> du;
  using VU = VFromD<decltype(du)>;
  const __m512i ret = _mm512_ternarylogic_epi64(
      BitCast(du, x1).raw, BitCast(du, x2).raw, BitCast(du, x3).raw, 0x96);
  return BitCast(d, VU{ret});
}

// ------------------------------ Or3
template <typename T>
HWY_API Vec512<T> Or3(Vec512<T> o1, Vec512<T> o2, Vec512<T> o3) {
  const DFromV<decltype(o1)> d;
  const RebindToUnsigned<decltype(d)> du;
  using VU = VFromD<decltype(du)>;
  const __m512i ret = _mm512_ternarylogic_epi64(
      BitCast(du, o1).raw, BitCast(du, o2).raw, BitCast(du, o3).raw, 0xFE);
  return BitCast(d, VU{ret});
}

// ------------------------------ OrAnd
template <typename T>
HWY_API Vec512<T> OrAnd(Vec512<T> o, Vec512<T> a1, Vec512<T> a2) {
  const DFromV<decltype(o)> d;
  const RebindToUnsigned<decltype(d)> du;
  using VU = VFromD<decltype(du)>;
  const __m512i ret = _mm512_ternarylogic_epi64(
      BitCast(du, o).raw, BitCast(du, a1).raw, BitCast(du, a2).raw, 0xF8);
  return BitCast(d, VU{ret});
}

// ------------------------------ IfVecThenElse
template <typename T>
HWY_API Vec512<T> IfVecThenElse(Vec512<T> mask, Vec512<T> yes, Vec512<T> no) {
  const DFromV<decltype(yes)> d;
  const RebindToUnsigned<decltype(d)> du;
  using VU = VFromD<decltype(du)>;
  return BitCast(d, VU{_mm512_ternarylogic_epi64(BitCast(du, mask).raw,
                                                 BitCast(du, yes).raw,
                                                 BitCast(du, no).raw, 0xCA)});
}

// ------------------------------ Operator overloads (internal-only if float)

template <typename T>
HWY_API Vec512<T> operator&(const Vec512<T> a, const Vec512<T> b) {
  return And(a, b);
}

template <typename T>
HWY_API Vec512<T> operator|(const Vec512<T> a, const Vec512<T> b) {
  return Or(a, b);
}

template <typename T>
HWY_API Vec512<T> operator^(const Vec512<T> a, const Vec512<T> b) {
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

template <typename T>
HWY_INLINE Vec512<T> PopulationCount(hwy::SizeTag<1> /* tag */, Vec512<T> v) {
  return Vec512<T>{_mm512_popcnt_epi8(v.raw)};
}
template <typename T>
HWY_INLINE Vec512<T> PopulationCount(hwy::SizeTag<2> /* tag */, Vec512<T> v) {
  return Vec512<T>{_mm512_popcnt_epi16(v.raw)};
}
template <typename T>
HWY_INLINE Vec512<T> PopulationCount(hwy::SizeTag<4> /* tag */, Vec512<T> v) {
  return Vec512<T>{_mm512_popcnt_epi32(v.raw)};
}
template <typename T>
HWY_INLINE Vec512<T> PopulationCount(hwy::SizeTag<8> /* tag */, Vec512<T> v) {
  return Vec512<T>{_mm512_popcnt_epi64(v.raw)};
}

}  // namespace detail

template <typename T>
HWY_API Vec512<T> PopulationCount(Vec512<T> v) {
  return detail::PopulationCount(hwy::SizeTag<sizeof(T)>(), v);
}

#endif  // HWY_TARGET <= HWY_AVX3_DL

// ================================================== SIGN

// ------------------------------ CopySign

template <typename T>
HWY_API Vec512<T> CopySign(const Vec512<T> magn, const Vec512<T> sign) {
  static_assert(IsFloat<T>(), "Only makes sense for floating-point");

  const DFromV<decltype(magn)> d;
  const auto msb = SignBit(d);

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
  const __m512i out = _mm512_ternarylogic_epi32(
      BitCast(du, msb).raw, BitCast(du, magn).raw, BitCast(du, sign).raw, 0xAC);
  return BitCast(d, decltype(Zero(du)){out});
}

template <typename T>
HWY_API Vec512<T> CopySignToAbs(const Vec512<T> abs, const Vec512<T> sign) {
  // AVX3 can also handle abs < 0, so no extra action needed.
  return CopySign(abs, sign);
}

// ================================================== MASK

// ------------------------------ FirstN

// Possibilities for constructing a bitmask of N ones:
// - kshift* only consider the lowest byte of the shift count, so they would
//   not correctly handle large n.
// - Scalar shifts >= 64 are UB.
// - BZHI has the desired semantics; we assume AVX-512 implies BMI2. However,
//   we need 64-bit masks for sizeof(T) == 1, so special-case 32-bit builds.

#if HWY_ARCH_X86_32
namespace detail {

// 32 bit mask is sufficient for lane size >= 2.
template <typename T, HWY_IF_NOT_T_SIZE(T, 1)>
HWY_INLINE Mask512<T> FirstN(size_t n) {
  Mask512<T> m;
  const uint32_t all = ~uint32_t{0};
  // BZHI only looks at the lower 8 bits of n!
  m.raw = static_cast<decltype(m.raw)>((n > 255) ? all : _bzhi_u32(all, n));
  return m;
}

#if HWY_COMPILER_MSVC >= 1920 || HWY_COMPILER_GCC_ACTUAL >= 900 || \
    HWY_COMPILER_CLANG || HWY_COMPILER_ICC
template <typename T, HWY_IF_T_SIZE(T, 1)>
HWY_INLINE Mask512<T> FirstN(size_t n) {
  uint32_t lo_mask;
  uint32_t hi_mask;
  uint32_t hi_mask_len;
#if HWY_COMPILER_GCC
  if (__builtin_constant_p(n >= 32) && n >= 32) {
    if (__builtin_constant_p(n >= 64) && n >= 64) {
      hi_mask_len = 32u;
    } else {
      hi_mask_len = ((n <= 287) ? static_cast<uint32_t>(n) : 287u) - 32u;
    }
    lo_mask = hi_mask = 0xFFFFFFFFu;
  } else  // NOLINT(readability/braces)
#endif
  {
    const uint32_t lo_mask_len = (n <= 255) ? static_cast<uint32_t>(n) : 255u;
    lo_mask = _bzhi_u32(0xFFFFFFFFu, lo_mask_len);

#if HWY_COMPILER_GCC
    if (__builtin_constant_p(lo_mask_len <= 32) && lo_mask_len <= 32) {
      return Mask512<T>{static_cast<__mmask64>(lo_mask)};
    }
#endif

    _addcarry_u32(_subborrow_u32(0, lo_mask_len, 32u, &hi_mask_len),
                  0xFFFFFFFFu, 0u, &hi_mask);
  }
  hi_mask = _bzhi_u32(hi_mask, hi_mask_len);
#if HWY_COMPILER_GCC && !HWY_COMPILER_ICC
  if (__builtin_constant_p((static_cast<uint64_t>(hi_mask) << 32) | lo_mask))
#endif
    return Mask512<T>{static_cast<__mmask64>(
        (static_cast<uint64_t>(hi_mask) << 32) | lo_mask)};
#if HWY_COMPILER_GCC && !HWY_COMPILER_ICC
  else
    return Mask512<T>{_mm512_kunpackd(static_cast<__mmask64>(hi_mask),
                                      static_cast<__mmask64>(lo_mask))};
#endif
}
#else
template <typename T, HWY_IF_T_SIZE(T, 1)>
HWY_INLINE Mask512<T> FirstN(size_t n) {
  const uint64_t bits = n < 64 ? ((1ULL << n) - 1) : ~uint64_t{0};
  return Mask512<T>{static_cast<__mmask64>(bits)};
}
#endif
}  // namespace detail
#endif  // HWY_ARCH_X86_32

template <class D, HWY_IF_V_SIZE_D(D, 64), typename T = TFromD<D>>
HWY_API Mask512<T> FirstN(D /* tag */, size_t n) {
#if HWY_ARCH_X86_64
  Mask512<T> m;
  const uint64_t all = ~uint64_t{0};
  // BZHI only looks at the lower 8 bits of n!
  m.raw = static_cast<decltype(m.raw)>((n > 255) ? all : _bzhi_u64(all, n));
  return m;
#else
  return detail::FirstN<T>(n);
#endif  // HWY_ARCH_X86_64
}

// ------------------------------ IfThenElse

// Returns mask ? b : a.

namespace detail {

// Templates for signed/unsigned integer of a particular size.
template <typename T>
HWY_INLINE Vec512<T> IfThenElse(hwy::SizeTag<1> /* tag */,
                                const Mask512<T> mask, const Vec512<T> yes,
                                const Vec512<T> no) {
  return Vec512<T>{_mm512_mask_mov_epi8(no.raw, mask.raw, yes.raw)};
}
template <typename T>
HWY_INLINE Vec512<T> IfThenElse(hwy::SizeTag<2> /* tag */,
                                const Mask512<T> mask, const Vec512<T> yes,
                                const Vec512<T> no) {
  return Vec512<T>{_mm512_mask_mov_epi16(no.raw, mask.raw, yes.raw)};
}
template <typename T>
HWY_INLINE Vec512<T> IfThenElse(hwy::SizeTag<4> /* tag */,
                                const Mask512<T> mask, const Vec512<T> yes,
                                const Vec512<T> no) {
  return Vec512<T>{_mm512_mask_mov_epi32(no.raw, mask.raw, yes.raw)};
}
template <typename T>
HWY_INLINE Vec512<T> IfThenElse(hwy::SizeTag<8> /* tag */,
                                const Mask512<T> mask, const Vec512<T> yes,
                                const Vec512<T> no) {
  return Vec512<T>{_mm512_mask_mov_epi64(no.raw, mask.raw, yes.raw)};
}

}  // namespace detail

template <typename T>
HWY_API Vec512<T> IfThenElse(const Mask512<T> mask, const Vec512<T> yes,
                             const Vec512<T> no) {
  return detail::IfThenElse(hwy::SizeTag<sizeof(T)>(), mask, yes, no);
}
HWY_API Vec512<float> IfThenElse(const Mask512<float> mask,
                                 const Vec512<float> yes,
                                 const Vec512<float> no) {
  return Vec512<float>{_mm512_mask_mov_ps(no.raw, mask.raw, yes.raw)};
}
HWY_API Vec512<double> IfThenElse(const Mask512<double> mask,
                                  const Vec512<double> yes,
                                  const Vec512<double> no) {
  return Vec512<double>{_mm512_mask_mov_pd(no.raw, mask.raw, yes.raw)};
}

namespace detail {

template <typename T>
HWY_INLINE Vec512<T> IfThenElseZero(hwy::SizeTag<1> /* tag */,
                                    const Mask512<T> mask,
                                    const Vec512<T> yes) {
  return Vec512<T>{_mm512_maskz_mov_epi8(mask.raw, yes.raw)};
}
template <typename T>
HWY_INLINE Vec512<T> IfThenElseZero(hwy::SizeTag<2> /* tag */,
                                    const Mask512<T> mask,
                                    const Vec512<T> yes) {
  return Vec512<T>{_mm512_maskz_mov_epi16(mask.raw, yes.raw)};
}
template <typename T>
HWY_INLINE Vec512<T> IfThenElseZero(hwy::SizeTag<4> /* tag */,
                                    const Mask512<T> mask,
                                    const Vec512<T> yes) {
  return Vec512<T>{_mm512_maskz_mov_epi32(mask.raw, yes.raw)};
}
template <typename T>
HWY_INLINE Vec512<T> IfThenElseZero(hwy::SizeTag<8> /* tag */,
                                    const Mask512<T> mask,
                                    const Vec512<T> yes) {
  return Vec512<T>{_mm512_maskz_mov_epi64(mask.raw, yes.raw)};
}

}  // namespace detail

template <typename T>
HWY_API Vec512<T> IfThenElseZero(const Mask512<T> mask, const Vec512<T> yes) {
  return detail::IfThenElseZero(hwy::SizeTag<sizeof(T)>(), mask, yes);
}
HWY_API Vec512<float> IfThenElseZero(const Mask512<float> mask,
                                     const Vec512<float> yes) {
  return Vec512<float>{_mm512_maskz_mov_ps(mask.raw, yes.raw)};
}
HWY_API Vec512<double> IfThenElseZero(const Mask512<double> mask,
                                      const Vec512<double> yes) {
  return Vec512<double>{_mm512_maskz_mov_pd(mask.raw, yes.raw)};
}

namespace detail {

template <typename T>
HWY_INLINE Vec512<T> IfThenZeroElse(hwy::SizeTag<1> /* tag */,
                                    const Mask512<T> mask, const Vec512<T> no) {
  // xor_epi8/16 are missing, but we have sub, which is just as fast for u8/16.
  return Vec512<T>{_mm512_mask_sub_epi8(no.raw, mask.raw, no.raw, no.raw)};
}
template <typename T>
HWY_INLINE Vec512<T> IfThenZeroElse(hwy::SizeTag<2> /* tag */,
                                    const Mask512<T> mask, const Vec512<T> no) {
  return Vec512<T>{_mm512_mask_sub_epi16(no.raw, mask.raw, no.raw, no.raw)};
}
template <typename T>
HWY_INLINE Vec512<T> IfThenZeroElse(hwy::SizeTag<4> /* tag */,
                                    const Mask512<T> mask, const Vec512<T> no) {
  return Vec512<T>{_mm512_mask_xor_epi32(no.raw, mask.raw, no.raw, no.raw)};
}
template <typename T>
HWY_INLINE Vec512<T> IfThenZeroElse(hwy::SizeTag<8> /* tag */,
                                    const Mask512<T> mask, const Vec512<T> no) {
  return Vec512<T>{_mm512_mask_xor_epi64(no.raw, mask.raw, no.raw, no.raw)};
}

}  // namespace detail

template <typename T>
HWY_API Vec512<T> IfThenZeroElse(const Mask512<T> mask, const Vec512<T> no) {
  return detail::IfThenZeroElse(hwy::SizeTag<sizeof(T)>(), mask, no);
}
HWY_API Vec512<float> IfThenZeroElse(const Mask512<float> mask,
                                     const Vec512<float> no) {
  return Vec512<float>{_mm512_mask_xor_ps(no.raw, mask.raw, no.raw, no.raw)};
}
HWY_API Vec512<double> IfThenZeroElse(const Mask512<double> mask,
                                      const Vec512<double> no) {
  return Vec512<double>{_mm512_mask_xor_pd(no.raw, mask.raw, no.raw, no.raw)};
}

template <typename T>
HWY_API Vec512<T> IfNegativeThenElse(Vec512<T> v, Vec512<T> yes, Vec512<T> no) {
  static_assert(IsSigned<T>(), "Only works for signed/float");
  // AVX3 MaskFromVec only looks at the MSB
  return IfThenElse(MaskFromVec(v), yes, no);
}

template <typename T, HWY_IF_FLOAT(T)>
HWY_API Vec512<T> ZeroIfNegative(const Vec512<T> v) {
  // AVX3 MaskFromVec only looks at the MSB
  return IfThenZeroElse(MaskFromVec(v), v);
}

// ================================================== ARITHMETIC

// ------------------------------ Addition

// Unsigned
HWY_API Vec512<uint8_t> operator+(const Vec512<uint8_t> a,
                                  const Vec512<uint8_t> b) {
  return Vec512<uint8_t>{_mm512_add_epi8(a.raw, b.raw)};
}
HWY_API Vec512<uint16_t> operator+(const Vec512<uint16_t> a,
                                   const Vec512<uint16_t> b) {
  return Vec512<uint16_t>{_mm512_add_epi16(a.raw, b.raw)};
}
HWY_API Vec512<uint32_t> operator+(const Vec512<uint32_t> a,
                                   const Vec512<uint32_t> b) {
  return Vec512<uint32_t>{_mm512_add_epi32(a.raw, b.raw)};
}
HWY_API Vec512<uint64_t> operator+(const Vec512<uint64_t> a,
                                   const Vec512<uint64_t> b) {
  return Vec512<uint64_t>{_mm512_add_epi64(a.raw, b.raw)};
}

// Signed
HWY_API Vec512<int8_t> operator+(const Vec512<int8_t> a,
                                 const Vec512<int8_t> b) {
  return Vec512<int8_t>{_mm512_add_epi8(a.raw, b.raw)};
}
HWY_API Vec512<int16_t> operator+(const Vec512<int16_t> a,
                                  const Vec512<int16_t> b) {
  return Vec512<int16_t>{_mm512_add_epi16(a.raw, b.raw)};
}
HWY_API Vec512<int32_t> operator+(const Vec512<int32_t> a,
                                  const Vec512<int32_t> b) {
  return Vec512<int32_t>{_mm512_add_epi32(a.raw, b.raw)};
}
HWY_API Vec512<int64_t> operator+(const Vec512<int64_t> a,
                                  const Vec512<int64_t> b) {
  return Vec512<int64_t>{_mm512_add_epi64(a.raw, b.raw)};
}

// Float
HWY_API Vec512<float> operator+(const Vec512<float> a, const Vec512<float> b) {
  return Vec512<float>{_mm512_add_ps(a.raw, b.raw)};
}
HWY_API Vec512<double> operator+(const Vec512<double> a,
                                 const Vec512<double> b) {
  return Vec512<double>{_mm512_add_pd(a.raw, b.raw)};
}

// ------------------------------ Subtraction

// Unsigned
HWY_API Vec512<uint8_t> operator-(const Vec512<uint8_t> a,
                                  const Vec512<uint8_t> b) {
  return Vec512<uint8_t>{_mm512_sub_epi8(a.raw, b.raw)};
}
HWY_API Vec512<uint16_t> operator-(const Vec512<uint16_t> a,
                                   const Vec512<uint16_t> b) {
  return Vec512<uint16_t>{_mm512_sub_epi16(a.raw, b.raw)};
}
HWY_API Vec512<uint32_t> operator-(const Vec512<uint32_t> a,
                                   const Vec512<uint32_t> b) {
  return Vec512<uint32_t>{_mm512_sub_epi32(a.raw, b.raw)};
}
HWY_API Vec512<uint64_t> operator-(const Vec512<uint64_t> a,
                                   const Vec512<uint64_t> b) {
  return Vec512<uint64_t>{_mm512_sub_epi64(a.raw, b.raw)};
}

// Signed
HWY_API Vec512<int8_t> operator-(const Vec512<int8_t> a,
                                 const Vec512<int8_t> b) {
  return Vec512<int8_t>{_mm512_sub_epi8(a.raw, b.raw)};
}
HWY_API Vec512<int16_t> operator-(const Vec512<int16_t> a,
                                  const Vec512<int16_t> b) {
  return Vec512<int16_t>{_mm512_sub_epi16(a.raw, b.raw)};
}
HWY_API Vec512<int32_t> operator-(const Vec512<int32_t> a,
                                  const Vec512<int32_t> b) {
  return Vec512<int32_t>{_mm512_sub_epi32(a.raw, b.raw)};
}
HWY_API Vec512<int64_t> operator-(const Vec512<int64_t> a,
                                  const Vec512<int64_t> b) {
  return Vec512<int64_t>{_mm512_sub_epi64(a.raw, b.raw)};
}

// Float
HWY_API Vec512<float> operator-(const Vec512<float> a, const Vec512<float> b) {
  return Vec512<float>{_mm512_sub_ps(a.raw, b.raw)};
}
HWY_API Vec512<double> operator-(const Vec512<double> a,
                                 const Vec512<double> b) {
  return Vec512<double>{_mm512_sub_pd(a.raw, b.raw)};
}

// ------------------------------ SumsOf8
HWY_API Vec512<uint64_t> SumsOf8(const Vec512<uint8_t> v) {
  const Full512<uint8_t> d;
  return Vec512<uint64_t>{_mm512_sad_epu8(v.raw, Zero(d).raw)};
}

HWY_API Vec512<uint64_t> SumsOf8AbsDiff(const Vec512<uint8_t> a,
                                        const Vec512<uint8_t> b) {
  return Vec512<uint64_t>{_mm512_sad_epu8(a.raw, b.raw)};
}

// ------------------------------ SaturatedAdd

// Returns a + b clamped to the destination range.

// Unsigned
HWY_API Vec512<uint8_t> SaturatedAdd(const Vec512<uint8_t> a,
                                     const Vec512<uint8_t> b) {
  return Vec512<uint8_t>{_mm512_adds_epu8(a.raw, b.raw)};
}
HWY_API Vec512<uint16_t> SaturatedAdd(const Vec512<uint16_t> a,
                                      const Vec512<uint16_t> b) {
  return Vec512<uint16_t>{_mm512_adds_epu16(a.raw, b.raw)};
}

// Signed
HWY_API Vec512<int8_t> SaturatedAdd(const Vec512<int8_t> a,
                                    const Vec512<int8_t> b) {
  return Vec512<int8_t>{_mm512_adds_epi8(a.raw, b.raw)};
}
HWY_API Vec512<int16_t> SaturatedAdd(const Vec512<int16_t> a,
                                     const Vec512<int16_t> b) {
  return Vec512<int16_t>{_mm512_adds_epi16(a.raw, b.raw)};
}

// ------------------------------ SaturatedSub

// Returns a - b clamped to the destination range.

// Unsigned
HWY_API Vec512<uint8_t> SaturatedSub(const Vec512<uint8_t> a,
                                     const Vec512<uint8_t> b) {
  return Vec512<uint8_t>{_mm512_subs_epu8(a.raw, b.raw)};
}
HWY_API Vec512<uint16_t> SaturatedSub(const Vec512<uint16_t> a,
                                      const Vec512<uint16_t> b) {
  return Vec512<uint16_t>{_mm512_subs_epu16(a.raw, b.raw)};
}

// Signed
HWY_API Vec512<int8_t> SaturatedSub(const Vec512<int8_t> a,
                                    const Vec512<int8_t> b) {
  return Vec512<int8_t>{_mm512_subs_epi8(a.raw, b.raw)};
}
HWY_API Vec512<int16_t> SaturatedSub(const Vec512<int16_t> a,
                                     const Vec512<int16_t> b) {
  return Vec512<int16_t>{_mm512_subs_epi16(a.raw, b.raw)};
}

// ------------------------------ Average

// Returns (a + b + 1) / 2

// Unsigned
HWY_API Vec512<uint8_t> AverageRound(const Vec512<uint8_t> a,
                                     const Vec512<uint8_t> b) {
  return Vec512<uint8_t>{_mm512_avg_epu8(a.raw, b.raw)};
}
HWY_API Vec512<uint16_t> AverageRound(const Vec512<uint16_t> a,
                                      const Vec512<uint16_t> b) {
  return Vec512<uint16_t>{_mm512_avg_epu16(a.raw, b.raw)};
}

// ------------------------------ Abs (Sub)

// Returns absolute value, except that LimitsMin() maps to LimitsMax() + 1.
HWY_API Vec512<int8_t> Abs(const Vec512<int8_t> v) {
#if HWY_COMPILER_MSVC
  // Workaround for incorrect codegen? (untested due to internal compiler error)
  const DFromV<decltype(v)> d;
  const auto zero = Zero(d);
  return Vec512<int8_t>{_mm512_max_epi8(v.raw, (zero - v).raw)};
#else
  return Vec512<int8_t>{_mm512_abs_epi8(v.raw)};
#endif
}
HWY_API Vec512<int16_t> Abs(const Vec512<int16_t> v) {
  return Vec512<int16_t>{_mm512_abs_epi16(v.raw)};
}
HWY_API Vec512<int32_t> Abs(const Vec512<int32_t> v) {
  return Vec512<int32_t>{_mm512_abs_epi32(v.raw)};
}
HWY_API Vec512<int64_t> Abs(const Vec512<int64_t> v) {
  return Vec512<int64_t>{_mm512_abs_epi64(v.raw)};
}

// These aren't native instructions, they also involve AND with constant.
HWY_API Vec512<float> Abs(const Vec512<float> v) {
  return Vec512<float>{_mm512_abs_ps(v.raw)};
}
HWY_API Vec512<double> Abs(const Vec512<double> v) {
// Workaround: _mm512_abs_pd expects __m512, so implement it ourselves.
#if HWY_COMPILER_GCC_ACTUAL && HWY_COMPILER_GCC_ACTUAL < 803
  const DFromV<decltype(v)> d;
  const RebindToUnsigned<decltype(d)> du;
  return And(v, BitCast(d, Set(du, 0x7FFFFFFFFFFFFFFFULL)));
#else
  return Vec512<double>{_mm512_abs_pd(v.raw)};
#endif
}
// ------------------------------ ShiftLeft

template <int kBits>
HWY_API Vec512<uint16_t> ShiftLeft(const Vec512<uint16_t> v) {
  return Vec512<uint16_t>{_mm512_slli_epi16(v.raw, kBits)};
}

template <int kBits>
HWY_API Vec512<uint32_t> ShiftLeft(const Vec512<uint32_t> v) {
  return Vec512<uint32_t>{_mm512_slli_epi32(v.raw, kBits)};
}

template <int kBits>
HWY_API Vec512<uint64_t> ShiftLeft(const Vec512<uint64_t> v) {
  return Vec512<uint64_t>{_mm512_slli_epi64(v.raw, kBits)};
}

template <int kBits>
HWY_API Vec512<int16_t> ShiftLeft(const Vec512<int16_t> v) {
  return Vec512<int16_t>{_mm512_slli_epi16(v.raw, kBits)};
}

template <int kBits>
HWY_API Vec512<int32_t> ShiftLeft(const Vec512<int32_t> v) {
  return Vec512<int32_t>{_mm512_slli_epi32(v.raw, kBits)};
}

template <int kBits>
HWY_API Vec512<int64_t> ShiftLeft(const Vec512<int64_t> v) {
  return Vec512<int64_t>{_mm512_slli_epi64(v.raw, kBits)};
}

template <int kBits, typename T, HWY_IF_T_SIZE(T, 1)>
HWY_API Vec512<T> ShiftLeft(const Vec512<T> v) {
  const DFromV<decltype(v)> d8;
  const RepartitionToWide<decltype(d8)> d16;
  const auto shifted = BitCast(d8, ShiftLeft<kBits>(BitCast(d16, v)));
  return kBits == 1
             ? (v + v)
             : (shifted & Set(d8, static_cast<T>((0xFF << kBits) & 0xFF)));
}

// ------------------------------ ShiftRight

template <int kBits>
HWY_API Vec512<uint16_t> ShiftRight(const Vec512<uint16_t> v) {
  return Vec512<uint16_t>{_mm512_srli_epi16(v.raw, kBits)};
}

template <int kBits>
HWY_API Vec512<uint32_t> ShiftRight(const Vec512<uint32_t> v) {
  return Vec512<uint32_t>{_mm512_srli_epi32(v.raw, kBits)};
}

template <int kBits>
HWY_API Vec512<uint64_t> ShiftRight(const Vec512<uint64_t> v) {
  return Vec512<uint64_t>{_mm512_srli_epi64(v.raw, kBits)};
}

template <int kBits>
HWY_API Vec512<uint8_t> ShiftRight(const Vec512<uint8_t> v) {
  const DFromV<decltype(v)> d8;
  // Use raw instead of BitCast to support N=1.
  const Vec512<uint8_t> shifted{ShiftRight<kBits>(Vec512<uint16_t>{v.raw}).raw};
  return shifted & Set(d8, 0xFF >> kBits);
}

template <int kBits>
HWY_API Vec512<int16_t> ShiftRight(const Vec512<int16_t> v) {
  return Vec512<int16_t>{_mm512_srai_epi16(v.raw, kBits)};
}

template <int kBits>
HWY_API Vec512<int32_t> ShiftRight(const Vec512<int32_t> v) {
  return Vec512<int32_t>{_mm512_srai_epi32(v.raw, kBits)};
}

template <int kBits>
HWY_API Vec512<int64_t> ShiftRight(const Vec512<int64_t> v) {
  return Vec512<int64_t>{_mm512_srai_epi64(v.raw, kBits)};
}

template <int kBits>
HWY_API Vec512<int8_t> ShiftRight(const Vec512<int8_t> v) {
  const DFromV<decltype(v)> di;
  const RebindToUnsigned<decltype(di)> du;
  const auto shifted = BitCast(di, ShiftRight<kBits>(BitCast(du, v)));
  const auto shifted_sign = BitCast(di, Set(du, 0x80 >> kBits));
  return (shifted ^ shifted_sign) - shifted_sign;
}

// ------------------------------ RotateRight

template <int kBits, typename T, HWY_IF_T_SIZE_ONE_OF(T, (1 << 1) | (1 << 2))>
HWY_API Vec512<T> RotateRight(const Vec512<T> v) {
  constexpr size_t kSizeInBits = sizeof(T) * 8;
  static_assert(0 <= kBits && kBits < kSizeInBits, "Invalid shift count");
  if (kBits == 0) return v;
  // AVX3 does not support 8/16-bit.
  return Or(ShiftRight<kBits>(v),
            ShiftLeft<HWY_MIN(kSizeInBits - 1, kSizeInBits - kBits)>(v));
}

template <int kBits>
HWY_API Vec512<uint32_t> RotateRight(const Vec512<uint32_t> v) {
  static_assert(0 <= kBits && kBits < 32, "Invalid shift count");
  if (kBits == 0) return v;
  return Vec512<uint32_t>{_mm512_ror_epi32(v.raw, kBits)};
}

template <int kBits>
HWY_API Vec512<uint64_t> RotateRight(const Vec512<uint64_t> v) {
  static_assert(0 <= kBits && kBits < 64, "Invalid shift count");
  if (kBits == 0) return v;
  return Vec512<uint64_t>{_mm512_ror_epi64(v.raw, kBits)};
}

// ------------------------------ ShiftLeftSame

// GCC and older Clang do not follow the Intel documentation for AVX-512
// shift-with-immediate: the counts should all be unsigned int.
#if HWY_COMPILER_CLANG && HWY_COMPILER_CLANG < 1100
using Shift16Count = int;
using Shift3264Count = int;
#elif HWY_COMPILER_GCC_ACTUAL
// GCC 11.0 requires these, prior versions used a macro+cast and don't care.
using Shift16Count = int;
using Shift3264Count = unsigned int;
#else
// Assume documented behavior. Clang 11 and MSVC 14.28.29910 match this.
using Shift16Count = unsigned int;
using Shift3264Count = unsigned int;
#endif

HWY_API Vec512<uint16_t> ShiftLeftSame(const Vec512<uint16_t> v,
                                       const int bits) {
#if HWY_COMPILER_GCC
  if (__builtin_constant_p(bits)) {
    return Vec512<uint16_t>{
        _mm512_slli_epi16(v.raw, static_cast<Shift16Count>(bits))};
  }
#endif
  return Vec512<uint16_t>{_mm512_sll_epi16(v.raw, _mm_cvtsi32_si128(bits))};
}
HWY_API Vec512<uint32_t> ShiftLeftSame(const Vec512<uint32_t> v,
                                       const int bits) {
#if HWY_COMPILER_GCC
  if (__builtin_constant_p(bits)) {
    return Vec512<uint32_t>{
        _mm512_slli_epi32(v.raw, static_cast<Shift3264Count>(bits))};
  }
#endif
  return Vec512<uint32_t>{_mm512_sll_epi32(v.raw, _mm_cvtsi32_si128(bits))};
}
HWY_API Vec512<uint64_t> ShiftLeftSame(const Vec512<uint64_t> v,
                                       const int bits) {
#if HWY_COMPILER_GCC
  if (__builtin_constant_p(bits)) {
    return Vec512<uint64_t>{
        _mm512_slli_epi64(v.raw, static_cast<Shift3264Count>(bits))};
  }
#endif
  return Vec512<uint64_t>{_mm512_sll_epi64(v.raw, _mm_cvtsi32_si128(bits))};
}

HWY_API Vec512<int16_t> ShiftLeftSame(const Vec512<int16_t> v, const int bits) {
#if HWY_COMPILER_GCC
  if (__builtin_constant_p(bits)) {
    return Vec512<int16_t>{
        _mm512_slli_epi16(v.raw, static_cast<Shift16Count>(bits))};
  }
#endif
  return Vec512<int16_t>{_mm512_sll_epi16(v.raw, _mm_cvtsi32_si128(bits))};
}

HWY_API Vec512<int32_t> ShiftLeftSame(const Vec512<int32_t> v, const int bits) {
#if HWY_COMPILER_GCC
  if (__builtin_constant_p(bits)) {
    return Vec512<int32_t>{
        _mm512_slli_epi32(v.raw, static_cast<Shift3264Count>(bits))};
  }
#endif
  return Vec512<int32_t>{_mm512_sll_epi32(v.raw, _mm_cvtsi32_si128(bits))};
}

HWY_API Vec512<int64_t> ShiftLeftSame(const Vec512<int64_t> v, const int bits) {
#if HWY_COMPILER_GCC
  if (__builtin_constant_p(bits)) {
    return Vec512<int64_t>{
        _mm512_slli_epi64(v.raw, static_cast<Shift3264Count>(bits))};
  }
#endif
  return Vec512<int64_t>{_mm512_sll_epi64(v.raw, _mm_cvtsi32_si128(bits))};
}

template <typename T, HWY_IF_T_SIZE(T, 1)>
HWY_API Vec512<T> ShiftLeftSame(const Vec512<T> v, const int bits) {
  const DFromV<decltype(v)> d8;
  const RepartitionToWide<decltype(d8)> d16;
  const auto shifted = BitCast(d8, ShiftLeftSame(BitCast(d16, v), bits));
  return shifted & Set(d8, static_cast<T>((0xFF << bits) & 0xFF));
}

// ------------------------------ ShiftRightSame

HWY_API Vec512<uint16_t> ShiftRightSame(const Vec512<uint16_t> v,
                                        const int bits) {
#if HWY_COMPILER_GCC
  if (__builtin_constant_p(bits)) {
    return Vec512<uint16_t>{
        _mm512_srli_epi16(v.raw, static_cast<Shift16Count>(bits))};
  }
#endif
  return Vec512<uint16_t>{_mm512_srl_epi16(v.raw, _mm_cvtsi32_si128(bits))};
}
HWY_API Vec512<uint32_t> ShiftRightSame(const Vec512<uint32_t> v,
                                        const int bits) {
#if HWY_COMPILER_GCC
  if (__builtin_constant_p(bits)) {
    return Vec512<uint32_t>{
        _mm512_srli_epi32(v.raw, static_cast<Shift3264Count>(bits))};
  }
#endif
  return Vec512<uint32_t>{_mm512_srl_epi32(v.raw, _mm_cvtsi32_si128(bits))};
}
HWY_API Vec512<uint64_t> ShiftRightSame(const Vec512<uint64_t> v,
                                        const int bits) {
#if HWY_COMPILER_GCC
  if (__builtin_constant_p(bits)) {
    return Vec512<uint64_t>{
        _mm512_srli_epi64(v.raw, static_cast<Shift3264Count>(bits))};
  }
#endif
  return Vec512<uint64_t>{_mm512_srl_epi64(v.raw, _mm_cvtsi32_si128(bits))};
}

HWY_API Vec512<uint8_t> ShiftRightSame(Vec512<uint8_t> v, const int bits) {
  const DFromV<decltype(v)> d8;
  const RepartitionToWide<decltype(d8)> d16;
  const auto shifted = BitCast(d8, ShiftRightSame(BitCast(d16, v), bits));
  return shifted & Set(d8, static_cast<uint8_t>(0xFF >> bits));
}

HWY_API Vec512<int16_t> ShiftRightSame(const Vec512<int16_t> v,
                                       const int bits) {
#if HWY_COMPILER_GCC
  if (__builtin_constant_p(bits)) {
    return Vec512<int16_t>{
        _mm512_srai_epi16(v.raw, static_cast<Shift16Count>(bits))};
  }
#endif
  return Vec512<int16_t>{_mm512_sra_epi16(v.raw, _mm_cvtsi32_si128(bits))};
}

HWY_API Vec512<int32_t> ShiftRightSame(const Vec512<int32_t> v,
                                       const int bits) {
#if HWY_COMPILER_GCC
  if (__builtin_constant_p(bits)) {
    return Vec512<int32_t>{
        _mm512_srai_epi32(v.raw, static_cast<Shift3264Count>(bits))};
  }
#endif
  return Vec512<int32_t>{_mm512_sra_epi32(v.raw, _mm_cvtsi32_si128(bits))};
}
HWY_API Vec512<int64_t> ShiftRightSame(const Vec512<int64_t> v,
                                       const int bits) {
#if HWY_COMPILER_GCC
  if (__builtin_constant_p(bits)) {
    return Vec512<int64_t>{
        _mm512_srai_epi64(v.raw, static_cast<Shift3264Count>(bits))};
  }
#endif
  return Vec512<int64_t>{_mm512_sra_epi64(v.raw, _mm_cvtsi32_si128(bits))};
}

HWY_API Vec512<int8_t> ShiftRightSame(Vec512<int8_t> v, const int bits) {
  const DFromV<decltype(v)> di;
  const RebindToUnsigned<decltype(di)> du;
  const auto shifted = BitCast(di, ShiftRightSame(BitCast(du, v), bits));
  const auto shifted_sign =
      BitCast(di, Set(du, static_cast<uint8_t>(0x80 >> bits)));
  return (shifted ^ shifted_sign) - shifted_sign;
}

// ------------------------------ Minimum

// Unsigned
HWY_API Vec512<uint8_t> Min(const Vec512<uint8_t> a, const Vec512<uint8_t> b) {
  return Vec512<uint8_t>{_mm512_min_epu8(a.raw, b.raw)};
}
HWY_API Vec512<uint16_t> Min(const Vec512<uint16_t> a,
                             const Vec512<uint16_t> b) {
  return Vec512<uint16_t>{_mm512_min_epu16(a.raw, b.raw)};
}
HWY_API Vec512<uint32_t> Min(const Vec512<uint32_t> a,
                             const Vec512<uint32_t> b) {
  return Vec512<uint32_t>{_mm512_min_epu32(a.raw, b.raw)};
}
HWY_API Vec512<uint64_t> Min(const Vec512<uint64_t> a,
                             const Vec512<uint64_t> b) {
  return Vec512<uint64_t>{_mm512_min_epu64(a.raw, b.raw)};
}

// Signed
HWY_API Vec512<int8_t> Min(const Vec512<int8_t> a, const Vec512<int8_t> b) {
  return Vec512<int8_t>{_mm512_min_epi8(a.raw, b.raw)};
}
HWY_API Vec512<int16_t> Min(const Vec512<int16_t> a, const Vec512<int16_t> b) {
  return Vec512<int16_t>{_mm512_min_epi16(a.raw, b.raw)};
}
HWY_API Vec512<int32_t> Min(const Vec512<int32_t> a, const Vec512<int32_t> b) {
  return Vec512<int32_t>{_mm512_min_epi32(a.raw, b.raw)};
}
HWY_API Vec512<int64_t> Min(const Vec512<int64_t> a, const Vec512<int64_t> b) {
  return Vec512<int64_t>{_mm512_min_epi64(a.raw, b.raw)};
}

// Float
HWY_API Vec512<float> Min(const Vec512<float> a, const Vec512<float> b) {
  return Vec512<float>{_mm512_min_ps(a.raw, b.raw)};
}
HWY_API Vec512<double> Min(const Vec512<double> a, const Vec512<double> b) {
  return Vec512<double>{_mm512_min_pd(a.raw, b.raw)};
}

// ------------------------------ Maximum

// Unsigned
HWY_API Vec512<uint8_t> Max(const Vec512<uint8_t> a, const Vec512<uint8_t> b) {
  return Vec512<uint8_t>{_mm512_max_epu8(a.raw, b.raw)};
}
HWY_API Vec512<uint16_t> Max(const Vec512<uint16_t> a,
                             const Vec512<uint16_t> b) {
  return Vec512<uint16_t>{_mm512_max_epu16(a.raw, b.raw)};
}
HWY_API Vec512<uint32_t> Max(const Vec512<uint32_t> a,
                             const Vec512<uint32_t> b) {
  return Vec512<uint32_t>{_mm512_max_epu32(a.raw, b.raw)};
}
HWY_API Vec512<uint64_t> Max(const Vec512<uint64_t> a,
                             const Vec512<uint64_t> b) {
  return Vec512<uint64_t>{_mm512_max_epu64(a.raw, b.raw)};
}

// Signed
HWY_API Vec512<int8_t> Max(const Vec512<int8_t> a, const Vec512<int8_t> b) {
  return Vec512<int8_t>{_mm512_max_epi8(a.raw, b.raw)};
}
HWY_API Vec512<int16_t> Max(const Vec512<int16_t> a, const Vec512<int16_t> b) {
  return Vec512<int16_t>{_mm512_max_epi16(a.raw, b.raw)};
}
HWY_API Vec512<int32_t> Max(const Vec512<int32_t> a, const Vec512<int32_t> b) {
  return Vec512<int32_t>{_mm512_max_epi32(a.raw, b.raw)};
}
HWY_API Vec512<int64_t> Max(const Vec512<int64_t> a, const Vec512<int64_t> b) {
  return Vec512<int64_t>{_mm512_max_epi64(a.raw, b.raw)};
}

// Float
HWY_API Vec512<float> Max(const Vec512<float> a, const Vec512<float> b) {
  return Vec512<float>{_mm512_max_ps(a.raw, b.raw)};
}
HWY_API Vec512<double> Max(const Vec512<double> a, const Vec512<double> b) {
  return Vec512<double>{_mm512_max_pd(a.raw, b.raw)};
}

// ------------------------------ Integer multiplication

// Per-target flag to prevent generic_ops-inl.h from defining 64-bit operator*.
#ifdef HWY_NATIVE_MUL_64
#undef HWY_NATIVE_MUL_64
#else
#define HWY_NATIVE_MUL_64
#endif

// Unsigned
HWY_API Vec512<uint16_t> operator*(Vec512<uint16_t> a, Vec512<uint16_t> b) {
  return Vec512<uint16_t>{_mm512_mullo_epi16(a.raw, b.raw)};
}
HWY_API Vec512<uint32_t> operator*(Vec512<uint32_t> a, Vec512<uint32_t> b) {
  return Vec512<uint32_t>{_mm512_mullo_epi32(a.raw, b.raw)};
}
HWY_API Vec512<uint64_t> operator*(Vec512<uint64_t> a, Vec512<uint64_t> b) {
  return Vec512<uint64_t>{_mm512_mullo_epi64(a.raw, b.raw)};
}
HWY_API Vec256<uint64_t> operator*(Vec256<uint64_t> a, Vec256<uint64_t> b) {
  return Vec256<uint64_t>{_mm256_mullo_epi64(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<uint64_t, N> operator*(Vec128<uint64_t, N> a,
                                      Vec128<uint64_t, N> b) {
  return Vec128<uint64_t, N>{_mm_mullo_epi64(a.raw, b.raw)};
}

// Signed
HWY_API Vec512<int16_t> operator*(Vec512<int16_t> a, Vec512<int16_t> b) {
  return Vec512<int16_t>{_mm512_mullo_epi16(a.raw, b.raw)};
}
HWY_API Vec512<int32_t> operator*(Vec512<int32_t> a, Vec512<int32_t> b) {
  return Vec512<int32_t>{_mm512_mullo_epi32(a.raw, b.raw)};
}
HWY_API Vec512<int64_t> operator*(Vec512<int64_t> a, Vec512<int64_t> b) {
  return Vec512<int64_t>{_mm512_mullo_epi64(a.raw, b.raw)};
}
HWY_API Vec256<int64_t> operator*(Vec256<int64_t> a, Vec256<int64_t> b) {
  return Vec256<int64_t>{_mm256_mullo_epi64(a.raw, b.raw)};
}
template <size_t N>
HWY_API Vec128<int64_t, N> operator*(Vec128<int64_t, N> a,
                                     Vec128<int64_t, N> b) {
  return Vec128<int64_t, N>{_mm_mullo_epi64(a.raw, b.raw)};
}
// Returns the upper 16 bits of a * b in each lane.
HWY_API Vec512<uint16_t> MulHigh(Vec512<uint16_t> a, Vec512<uint16_t> b) {
  return Vec512<uint16_t>{_mm512_mulhi_epu16(a.raw, b.raw)};
}
HWY_API Vec512<int16_t> MulHigh(Vec512<int16_t> a, Vec512<int16_t> b) {
  return Vec512<int16_t>{_mm512_mulhi_epi16(a.raw, b.raw)};
}

HWY_API Vec512<int16_t> MulFixedPoint15(Vec512<int16_t> a, Vec512<int16_t> b) {
  return Vec512<int16_t>{_mm512_mulhrs_epi16(a.raw, b.raw)};
}

// Multiplies even lanes (0, 2 ..) and places the double-wide result into
// even and the upper half into its odd neighbor lane.
HWY_API Vec512<int64_t> MulEven(Vec512<int32_t> a, Vec512<int32_t> b) {
  return Vec512<int64_t>{_mm512_mul_epi32(a.raw, b.raw)};
}
HWY_API Vec512<uint64_t> MulEven(Vec512<uint32_t> a, Vec512<uint32_t> b) {
  return Vec512<uint64_t>{_mm512_mul_epu32(a.raw, b.raw)};
}

// ------------------------------ Neg (Sub)

template <typename T, HWY_IF_FLOAT(T)>
HWY_API Vec512<T> Neg(const Vec512<T> v) {
  const DFromV<decltype(v)> d;
  return Xor(v, SignBit(d));
}

template <typename T, HWY_IF_NOT_FLOAT(T)>
HWY_API Vec512<T> Neg(const Vec512<T> v) {
  const DFromV<decltype(v)> d;
  return Zero(d) - v;
}

// ------------------------------ Floating-point mul / div

HWY_API Vec512<float> operator*(const Vec512<float> a, const Vec512<float> b) {
  return Vec512<float>{_mm512_mul_ps(a.raw, b.raw)};
}
HWY_API Vec512<double> operator*(const Vec512<double> a,
                                 const Vec512<double> b) {
  return Vec512<double>{_mm512_mul_pd(a.raw, b.raw)};
}

HWY_API Vec512<float> operator/(const Vec512<float> a, const Vec512<float> b) {
  return Vec512<float>{_mm512_div_ps(a.raw, b.raw)};
}
HWY_API Vec512<double> operator/(const Vec512<double> a,
                                 const Vec512<double> b) {
  return Vec512<double>{_mm512_div_pd(a.raw, b.raw)};
}

// Approximate reciprocal
HWY_API Vec512<float> ApproximateReciprocal(const Vec512<float> v) {
  return Vec512<float>{_mm512_rcp14_ps(v.raw)};
}

// Absolute value of difference.
HWY_API Vec512<float> AbsDiff(const Vec512<float> a, const Vec512<float> b) {
  return Abs(a - b);
}

// ------------------------------ Floating-point multiply-add variants

// Returns mul * x + add
HWY_API Vec512<float> MulAdd(const Vec512<float> mul, const Vec512<float> x,
                             const Vec512<float> add) {
  return Vec512<float>{_mm512_fmadd_ps(mul.raw, x.raw, add.raw)};
}
HWY_API Vec512<double> MulAdd(const Vec512<double> mul, const Vec512<double> x,
                              const Vec512<double> add) {
  return Vec512<double>{_mm512_fmadd_pd(mul.raw, x.raw, add.raw)};
}

// Returns add - mul * x
HWY_API Vec512<float> NegMulAdd(const Vec512<float> mul, const Vec512<float> x,
                                const Vec512<float> add) {
  return Vec512<float>{_mm512_fnmadd_ps(mul.raw, x.raw, add.raw)};
}
HWY_API Vec512<double> NegMulAdd(const Vec512<double> mul,
                                 const Vec512<double> x,
                                 const Vec512<double> add) {
  return Vec512<double>{_mm512_fnmadd_pd(mul.raw, x.raw, add.raw)};
}

// Returns mul * x - sub
HWY_API Vec512<float> MulSub(const Vec512<float> mul, const Vec512<float> x,
                             const Vec512<float> sub) {
  return Vec512<float>{_mm512_fmsub_ps(mul.raw, x.raw, sub.raw)};
}
HWY_API Vec512<double> MulSub(const Vec512<double> mul, const Vec512<double> x,
                              const Vec512<double> sub) {
  return Vec512<double>{_mm512_fmsub_pd(mul.raw, x.raw, sub.raw)};
}

// Returns -mul * x - sub
HWY_API Vec512<float> NegMulSub(const Vec512<float> mul, const Vec512<float> x,
                                const Vec512<float> sub) {
  return Vec512<float>{_mm512_fnmsub_ps(mul.raw, x.raw, sub.raw)};
}
HWY_API Vec512<double> NegMulSub(const Vec512<double> mul,
                                 const Vec512<double> x,
                                 const Vec512<double> sub) {
  return Vec512<double>{_mm512_fnmsub_pd(mul.raw, x.raw, sub.raw)};
}

// ------------------------------ Floating-point square root

// Full precision square root
HWY_API Vec512<float> Sqrt(const Vec512<float> v) {
  return Vec512<float>{_mm512_sqrt_ps(v.raw)};
}
HWY_API Vec512<double> Sqrt(const Vec512<double> v) {
  return Vec512<double>{_mm512_sqrt_pd(v.raw)};
}

// Approximate reciprocal square root
HWY_API Vec512<float> ApproximateReciprocalSqrt(const Vec512<float> v) {
  return Vec512<float>{_mm512_rsqrt14_ps(v.raw)};
}

// ------------------------------ Floating-point rounding

// Work around warnings in the intrinsic definitions (passing -1 as a mask).
HWY_DIAGNOSTICS(push)
HWY_DIAGNOSTICS_OFF(disable : 4245 4365, ignored "-Wsign-conversion")

// Toward nearest integer, tie to even
HWY_API Vec512<float> Round(const Vec512<float> v) {
  return Vec512<float>{_mm512_roundscale_ps(
      v.raw, _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC)};
}
HWY_API Vec512<double> Round(const Vec512<double> v) {
  return Vec512<double>{_mm512_roundscale_pd(
      v.raw, _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC)};
}

// Toward zero, aka truncate
HWY_API Vec512<float> Trunc(const Vec512<float> v) {
  return Vec512<float>{
      _mm512_roundscale_ps(v.raw, _MM_FROUND_TO_ZERO | _MM_FROUND_NO_EXC)};
}
HWY_API Vec512<double> Trunc(const Vec512<double> v) {
  return Vec512<double>{
      _mm512_roundscale_pd(v.raw, _MM_FROUND_TO_ZERO | _MM_FROUND_NO_EXC)};
}

// Toward +infinity, aka ceiling
HWY_API Vec512<float> Ceil(const Vec512<float> v) {
  return Vec512<float>{
      _mm512_roundscale_ps(v.raw, _MM_FROUND_TO_POS_INF | _MM_FROUND_NO_EXC)};
}
HWY_API Vec512<double> Ceil(const Vec512<double> v) {
  return Vec512<double>{
      _mm512_roundscale_pd(v.raw, _MM_FROUND_TO_POS_INF | _MM_FROUND_NO_EXC)};
}

// Toward -infinity, aka floor
HWY_API Vec512<float> Floor(const Vec512<float> v) {
  return Vec512<float>{
      _mm512_roundscale_ps(v.raw, _MM_FROUND_TO_NEG_INF | _MM_FROUND_NO_EXC)};
}
HWY_API Vec512<double> Floor(const Vec512<double> v) {
  return Vec512<double>{
      _mm512_roundscale_pd(v.raw, _MM_FROUND_TO_NEG_INF | _MM_FROUND_NO_EXC)};
}

HWY_DIAGNOSTICS(pop)

// ================================================== COMPARE

// Comparisons set a mask bit to 1 if the condition is true, else 0.

template <typename TFrom, class DTo, typename TTo = TFromD<DTo>>
HWY_API Mask512<TTo> RebindMask(DTo /*tag*/, Mask512<TFrom> m) {
  static_assert(sizeof(TFrom) == sizeof(TTo), "Must have same size");
  return Mask512<TTo>{m.raw};
}

namespace detail {

template <typename T>
HWY_INLINE Mask512<T> TestBit(hwy::SizeTag<1> /*tag*/, const Vec512<T> v,
                              const Vec512<T> bit) {
  return Mask512<T>{_mm512_test_epi8_mask(v.raw, bit.raw)};
}
template <typename T>
HWY_INLINE Mask512<T> TestBit(hwy::SizeTag<2> /*tag*/, const Vec512<T> v,
                              const Vec512<T> bit) {
  return Mask512<T>{_mm512_test_epi16_mask(v.raw, bit.raw)};
}
template <typename T>
HWY_INLINE Mask512<T> TestBit(hwy::SizeTag<4> /*tag*/, const Vec512<T> v,
                              const Vec512<T> bit) {
  return Mask512<T>{_mm512_test_epi32_mask(v.raw, bit.raw)};
}
template <typename T>
HWY_INLINE Mask512<T> TestBit(hwy::SizeTag<8> /*tag*/, const Vec512<T> v,
                              const Vec512<T> bit) {
  return Mask512<T>{_mm512_test_epi64_mask(v.raw, bit.raw)};
}

}  // namespace detail

template <typename T>
HWY_API Mask512<T> TestBit(const Vec512<T> v, const Vec512<T> bit) {
  static_assert(!hwy::IsFloat<T>(), "Only integer vectors supported");
  return detail::TestBit(hwy::SizeTag<sizeof(T)>(), v, bit);
}

// ------------------------------ Equality

template <typename T, HWY_IF_T_SIZE(T, 1)>
HWY_API Mask512<T> operator==(Vec512<T> a, Vec512<T> b) {
  return Mask512<T>{_mm512_cmpeq_epi8_mask(a.raw, b.raw)};
}
template <typename T, HWY_IF_T_SIZE(T, 2)>
HWY_API Mask512<T> operator==(Vec512<T> a, Vec512<T> b) {
  return Mask512<T>{_mm512_cmpeq_epi16_mask(a.raw, b.raw)};
}
template <typename T, HWY_IF_UI32(T)>
HWY_API Mask512<T> operator==(Vec512<T> a, Vec512<T> b) {
  return Mask512<T>{_mm512_cmpeq_epi32_mask(a.raw, b.raw)};
}
template <typename T, HWY_IF_UI64(T)>
HWY_API Mask512<T> operator==(Vec512<T> a, Vec512<T> b) {
  return Mask512<T>{_mm512_cmpeq_epi64_mask(a.raw, b.raw)};
}

HWY_API Mask512<float> operator==(Vec512<float> a, Vec512<float> b) {
  return Mask512<float>{_mm512_cmp_ps_mask(a.raw, b.raw, _CMP_EQ_OQ)};
}

HWY_API Mask512<double> operator==(Vec512<double> a, Vec512<double> b) {
  return Mask512<double>{_mm512_cmp_pd_mask(a.raw, b.raw, _CMP_EQ_OQ)};
}

// ------------------------------ Inequality

template <typename T, HWY_IF_T_SIZE(T, 1)>
HWY_API Mask512<T> operator!=(Vec512<T> a, Vec512<T> b) {
  return Mask512<T>{_mm512_cmpneq_epi8_mask(a.raw, b.raw)};
}
template <typename T, HWY_IF_T_SIZE(T, 2)>
HWY_API Mask512<T> operator!=(Vec512<T> a, Vec512<T> b) {
  return Mask512<T>{_mm512_cmpneq_epi16_mask(a.raw, b.raw)};
}
template <typename T, HWY_IF_UI32(T)>
HWY_API Mask512<T> operator!=(Vec512<T> a, Vec512<T> b) {
  return Mask512<T>{_mm512_cmpneq_epi32_mask(a.raw, b.raw)};
}
template <typename T, HWY_IF_UI64(T)>
HWY_API Mask512<T> operator!=(Vec512<T> a, Vec512<T> b) {
  return Mask512<T>{_mm512_cmpneq_epi64_mask(a.raw, b.raw)};
}

HWY_API Mask512<float> operator!=(Vec512<float> a, Vec512<float> b) {
  return Mask512<float>{_mm512_cmp_ps_mask(a.raw, b.raw, _CMP_NEQ_OQ)};
}

HWY_API Mask512<double> operator!=(Vec512<double> a, Vec512<double> b) {
  return Mask512<double>{_mm512_cmp_pd_mask(a.raw, b.raw, _CMP_NEQ_OQ)};
}

// ------------------------------ Strict inequality

HWY_API Mask512<uint8_t> operator>(Vec512<uint8_t> a, Vec512<uint8_t> b) {
  return Mask512<uint8_t>{_mm512_cmpgt_epu8_mask(a.raw, b.raw)};
}
HWY_API Mask512<uint16_t> operator>(Vec512<uint16_t> a, Vec512<uint16_t> b) {
  return Mask512<uint16_t>{_mm512_cmpgt_epu16_mask(a.raw, b.raw)};
}
HWY_API Mask512<uint32_t> operator>(Vec512<uint32_t> a, Vec512<uint32_t> b) {
  return Mask512<uint32_t>{_mm512_cmpgt_epu32_mask(a.raw, b.raw)};
}
HWY_API Mask512<uint64_t> operator>(Vec512<uint64_t> a, Vec512<uint64_t> b) {
  return Mask512<uint64_t>{_mm512_cmpgt_epu64_mask(a.raw, b.raw)};
}

HWY_API Mask512<int8_t> operator>(Vec512<int8_t> a, Vec512<int8_t> b) {
  return Mask512<int8_t>{_mm512_cmpgt_epi8_mask(a.raw, b.raw)};
}
HWY_API Mask512<int16_t> operator>(Vec512<int16_t> a, Vec512<int16_t> b) {
  return Mask512<int16_t>{_mm512_cmpgt_epi16_mask(a.raw, b.raw)};
}
HWY_API Mask512<int32_t> operator>(Vec512<int32_t> a, Vec512<int32_t> b) {
  return Mask512<int32_t>{_mm512_cmpgt_epi32_mask(a.raw, b.raw)};
}
HWY_API Mask512<int64_t> operator>(Vec512<int64_t> a, Vec512<int64_t> b) {
  return Mask512<int64_t>{_mm512_cmpgt_epi64_mask(a.raw, b.raw)};
}

HWY_API Mask512<float> operator>(Vec512<float> a, Vec512<float> b) {
  return Mask512<float>{_mm512_cmp_ps_mask(a.raw, b.raw, _CMP_GT_OQ)};
}
HWY_API Mask512<double> operator>(Vec512<double> a, Vec512<double> b) {
  return Mask512<double>{_mm512_cmp_pd_mask(a.raw, b.raw, _CMP_GT_OQ)};
}

// ------------------------------ Weak inequality

HWY_API Mask512<float> operator>=(Vec512<float> a, Vec512<float> b) {
  return Mask512<float>{_mm512_cmp_ps_mask(a.raw, b.raw, _CMP_GE_OQ)};
}
HWY_API Mask512<double> operator>=(Vec512<double> a, Vec512<double> b) {
  return Mask512<double>{_mm512_cmp_pd_mask(a.raw, b.raw, _CMP_GE_OQ)};
}

HWY_API Mask512<uint8_t> operator>=(Vec512<uint8_t> a, Vec512<uint8_t> b) {
  return Mask512<uint8_t>{_mm512_cmpge_epu8_mask(a.raw, b.raw)};
}
HWY_API Mask512<uint16_t> operator>=(Vec512<uint16_t> a, Vec512<uint16_t> b) {
  return Mask512<uint16_t>{_mm512_cmpge_epu16_mask(a.raw, b.raw)};
}
HWY_API Mask512<uint32_t> operator>=(Vec512<uint32_t> a, Vec512<uint32_t> b) {
  return Mask512<uint32_t>{_mm512_cmpge_epu32_mask(a.raw, b.raw)};
}
HWY_API Mask512<uint64_t> operator>=(Vec512<uint64_t> a, Vec512<uint64_t> b) {
  return Mask512<uint64_t>{_mm512_cmpge_epu64_mask(a.raw, b.raw)};
}

HWY_API Mask512<int8_t> operator>=(Vec512<int8_t> a, Vec512<int8_t> b) {
  return Mask512<int8_t>{_mm512_cmpge_epi8_mask(a.raw, b.raw)};
}
HWY_API Mask512<int16_t> operator>=(Vec512<int16_t> a, Vec512<int16_t> b) {
  return Mask512<int16_t>{_mm512_cmpge_epi16_mask(a.raw, b.raw)};
}
HWY_API Mask512<int32_t> operator>=(Vec512<int32_t> a, Vec512<int32_t> b) {
  return Mask512<int32_t>{_mm512_cmpge_epi32_mask(a.raw, b.raw)};
}
HWY_API Mask512<int64_t> operator>=(Vec512<int64_t> a, Vec512<int64_t> b) {
  return Mask512<int64_t>{_mm512_cmpge_epi64_mask(a.raw, b.raw)};
}

// ------------------------------ Reversed comparisons

template <typename T>
HWY_API Mask512<T> operator<(Vec512<T> a, Vec512<T> b) {
  return b > a;
}

template <typename T>
HWY_API Mask512<T> operator<=(Vec512<T> a, Vec512<T> b) {
  return b >= a;
}

// ------------------------------ Mask

namespace detail {

template <typename T>
HWY_INLINE Mask512<T> MaskFromVec(hwy::SizeTag<1> /*tag*/, const Vec512<T> v) {
  return Mask512<T>{_mm512_movepi8_mask(v.raw)};
}
template <typename T>
HWY_INLINE Mask512<T> MaskFromVec(hwy::SizeTag<2> /*tag*/, const Vec512<T> v) {
  return Mask512<T>{_mm512_movepi16_mask(v.raw)};
}
template <typename T>
HWY_INLINE Mask512<T> MaskFromVec(hwy::SizeTag<4> /*tag*/, const Vec512<T> v) {
  return Mask512<T>{_mm512_movepi32_mask(v.raw)};
}
template <typename T>
HWY_INLINE Mask512<T> MaskFromVec(hwy::SizeTag<8> /*tag*/, const Vec512<T> v) {
  return Mask512<T>{_mm512_movepi64_mask(v.raw)};
}

}  // namespace detail

template <typename T>
HWY_API Mask512<T> MaskFromVec(const Vec512<T> v) {
  return detail::MaskFromVec(hwy::SizeTag<sizeof(T)>(), v);
}
// There do not seem to be native floating-point versions of these instructions.
HWY_API Mask512<float> MaskFromVec(const Vec512<float> v) {
  const RebindToSigned<DFromV<decltype(v)>> di;
  return Mask512<float>{MaskFromVec(BitCast(di, v)).raw};
}
HWY_API Mask512<double> MaskFromVec(const Vec512<double> v) {
  const RebindToSigned<DFromV<decltype(v)>> di;
  return Mask512<double>{MaskFromVec(BitCast(di, v)).raw};
}

HWY_API Vec512<uint8_t> VecFromMask(const Mask512<uint8_t> v) {
  return Vec512<uint8_t>{_mm512_movm_epi8(v.raw)};
}
HWY_API Vec512<int8_t> VecFromMask(const Mask512<int8_t> v) {
  return Vec512<int8_t>{_mm512_movm_epi8(v.raw)};
}

HWY_API Vec512<uint16_t> VecFromMask(const Mask512<uint16_t> v) {
  return Vec512<uint16_t>{_mm512_movm_epi16(v.raw)};
}
HWY_API Vec512<int16_t> VecFromMask(const Mask512<int16_t> v) {
  return Vec512<int16_t>{_mm512_movm_epi16(v.raw)};
}

HWY_API Vec512<uint32_t> VecFromMask(const Mask512<uint32_t> v) {
  return Vec512<uint32_t>{_mm512_movm_epi32(v.raw)};
}
HWY_API Vec512<int32_t> VecFromMask(const Mask512<int32_t> v) {
  return Vec512<int32_t>{_mm512_movm_epi32(v.raw)};
}
HWY_API Vec512<float> VecFromMask(const Mask512<float> v) {
  return Vec512<float>{_mm512_castsi512_ps(_mm512_movm_epi32(v.raw))};
}

HWY_API Vec512<uint64_t> VecFromMask(const Mask512<uint64_t> v) {
  return Vec512<uint64_t>{_mm512_movm_epi64(v.raw)};
}
HWY_API Vec512<int64_t> VecFromMask(const Mask512<int64_t> v) {
  return Vec512<int64_t>{_mm512_movm_epi64(v.raw)};
}
HWY_API Vec512<double> VecFromMask(const Mask512<double> v) {
  return Vec512<double>{_mm512_castsi512_pd(_mm512_movm_epi64(v.raw))};
}

template <class D, typename T = TFromD<D>>
HWY_API Vec512<T> VecFromMask(D /* tag */, const Mask512<T> v) {
  return VecFromMask(v);
}

// ------------------------------ Mask logical

namespace detail {

template <typename T>
HWY_INLINE Mask512<T> Not(hwy::SizeTag<1> /*tag*/, const Mask512<T> m) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return Mask512<T>{_knot_mask64(m.raw)};
#else
  return Mask512<T>{~m.raw};
#endif
}
template <typename T>
HWY_INLINE Mask512<T> Not(hwy::SizeTag<2> /*tag*/, const Mask512<T> m) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return Mask512<T>{_knot_mask32(m.raw)};
#else
  return Mask512<T>{~m.raw};
#endif
}
template <typename T>
HWY_INLINE Mask512<T> Not(hwy::SizeTag<4> /*tag*/, const Mask512<T> m) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return Mask512<T>{_knot_mask16(m.raw)};
#else
  return Mask512<T>{static_cast<uint16_t>(~m.raw & 0xFFFF)};
#endif
}
template <typename T>
HWY_INLINE Mask512<T> Not(hwy::SizeTag<8> /*tag*/, const Mask512<T> m) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return Mask512<T>{_knot_mask8(m.raw)};
#else
  return Mask512<T>{static_cast<uint8_t>(~m.raw & 0xFF)};
#endif
}

template <typename T>
HWY_INLINE Mask512<T> And(hwy::SizeTag<1> /*tag*/, const Mask512<T> a,
                          const Mask512<T> b) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return Mask512<T>{_kand_mask64(a.raw, b.raw)};
#else
  return Mask512<T>{a.raw & b.raw};
#endif
}
template <typename T>
HWY_INLINE Mask512<T> And(hwy::SizeTag<2> /*tag*/, const Mask512<T> a,
                          const Mask512<T> b) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return Mask512<T>{_kand_mask32(a.raw, b.raw)};
#else
  return Mask512<T>{a.raw & b.raw};
#endif
}
template <typename T>
HWY_INLINE Mask512<T> And(hwy::SizeTag<4> /*tag*/, const Mask512<T> a,
                          const Mask512<T> b) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return Mask512<T>{_kand_mask16(a.raw, b.raw)};
#else
  return Mask512<T>{static_cast<uint16_t>(a.raw & b.raw)};
#endif
}
template <typename T>
HWY_INLINE Mask512<T> And(hwy::SizeTag<8> /*tag*/, const Mask512<T> a,
                          const Mask512<T> b) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return Mask512<T>{_kand_mask8(a.raw, b.raw)};
#else
  return Mask512<T>{static_cast<uint8_t>(a.raw & b.raw)};
#endif
}

template <typename T>
HWY_INLINE Mask512<T> AndNot(hwy::SizeTag<1> /*tag*/, const Mask512<T> a,
                             const Mask512<T> b) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return Mask512<T>{_kandn_mask64(a.raw, b.raw)};
#else
  return Mask512<T>{~a.raw & b.raw};
#endif
}
template <typename T>
HWY_INLINE Mask512<T> AndNot(hwy::SizeTag<2> /*tag*/, const Mask512<T> a,
                             const Mask512<T> b) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return Mask512<T>{_kandn_mask32(a.raw, b.raw)};
#else
  return Mask512<T>{~a.raw & b.raw};
#endif
}
template <typename T>
HWY_INLINE Mask512<T> AndNot(hwy::SizeTag<4> /*tag*/, const Mask512<T> a,
                             const Mask512<T> b) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return Mask512<T>{_kandn_mask16(a.raw, b.raw)};
#else
  return Mask512<T>{static_cast<uint16_t>(~a.raw & b.raw)};
#endif
}
template <typename T>
HWY_INLINE Mask512<T> AndNot(hwy::SizeTag<8> /*tag*/, const Mask512<T> a,
                             const Mask512<T> b) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return Mask512<T>{_kandn_mask8(a.raw, b.raw)};
#else
  return Mask512<T>{static_cast<uint8_t>(~a.raw & b.raw)};
#endif
}

template <typename T>
HWY_INLINE Mask512<T> Or(hwy::SizeTag<1> /*tag*/, const Mask512<T> a,
                         const Mask512<T> b) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return Mask512<T>{_kor_mask64(a.raw, b.raw)};
#else
  return Mask512<T>{a.raw | b.raw};
#endif
}
template <typename T>
HWY_INLINE Mask512<T> Or(hwy::SizeTag<2> /*tag*/, const Mask512<T> a,
                         const Mask512<T> b) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return Mask512<T>{_kor_mask32(a.raw, b.raw)};
#else
  return Mask512<T>{a.raw | b.raw};
#endif
}
template <typename T>
HWY_INLINE Mask512<T> Or(hwy::SizeTag<4> /*tag*/, const Mask512<T> a,
                         const Mask512<T> b) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return Mask512<T>{_kor_mask16(a.raw, b.raw)};
#else
  return Mask512<T>{static_cast<uint16_t>(a.raw | b.raw)};
#endif
}
template <typename T>
HWY_INLINE Mask512<T> Or(hwy::SizeTag<8> /*tag*/, const Mask512<T> a,
                         const Mask512<T> b) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return Mask512<T>{_kor_mask8(a.raw, b.raw)};
#else
  return Mask512<T>{static_cast<uint8_t>(a.raw | b.raw)};
#endif
}

template <typename T>
HWY_INLINE Mask512<T> Xor(hwy::SizeTag<1> /*tag*/, const Mask512<T> a,
                          const Mask512<T> b) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return Mask512<T>{_kxor_mask64(a.raw, b.raw)};
#else
  return Mask512<T>{a.raw ^ b.raw};
#endif
}
template <typename T>
HWY_INLINE Mask512<T> Xor(hwy::SizeTag<2> /*tag*/, const Mask512<T> a,
                          const Mask512<T> b) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return Mask512<T>{_kxor_mask32(a.raw, b.raw)};
#else
  return Mask512<T>{a.raw ^ b.raw};
#endif
}
template <typename T>
HWY_INLINE Mask512<T> Xor(hwy::SizeTag<4> /*tag*/, const Mask512<T> a,
                          const Mask512<T> b) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return Mask512<T>{_kxor_mask16(a.raw, b.raw)};
#else
  return Mask512<T>{static_cast<uint16_t>(a.raw ^ b.raw)};
#endif
}
template <typename T>
HWY_INLINE Mask512<T> Xor(hwy::SizeTag<8> /*tag*/, const Mask512<T> a,
                          const Mask512<T> b) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return Mask512<T>{_kxor_mask8(a.raw, b.raw)};
#else
  return Mask512<T>{static_cast<uint8_t>(a.raw ^ b.raw)};
#endif
}

template <typename T>
HWY_INLINE Mask512<T> ExclusiveNeither(hwy::SizeTag<1> /*tag*/,
                                       const Mask512<T> a, const Mask512<T> b) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return Mask512<T>{_kxnor_mask64(a.raw, b.raw)};
#else
  return Mask512<T>{~(a.raw ^ b.raw)};
#endif
}
template <typename T>
HWY_INLINE Mask512<T> ExclusiveNeither(hwy::SizeTag<2> /*tag*/,
                                       const Mask512<T> a, const Mask512<T> b) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return Mask512<T>{_kxnor_mask32(a.raw, b.raw)};
#else
  return Mask512<T>{static_cast<__mmask32>(~(a.raw ^ b.raw) & 0xFFFFFFFF)};
#endif
}
template <typename T>
HWY_INLINE Mask512<T> ExclusiveNeither(hwy::SizeTag<4> /*tag*/,
                                       const Mask512<T> a, const Mask512<T> b) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return Mask512<T>{_kxnor_mask16(a.raw, b.raw)};
#else
  return Mask512<T>{static_cast<__mmask16>(~(a.raw ^ b.raw) & 0xFFFF)};
#endif
}
template <typename T>
HWY_INLINE Mask512<T> ExclusiveNeither(hwy::SizeTag<8> /*tag*/,
                                       const Mask512<T> a, const Mask512<T> b) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return Mask512<T>{_kxnor_mask8(a.raw, b.raw)};
#else
  return Mask512<T>{static_cast<__mmask8>(~(a.raw ^ b.raw) & 0xFF)};
#endif
}

}  // namespace detail

template <typename T>
HWY_API Mask512<T> Not(const Mask512<T> m) {
  return detail::Not(hwy::SizeTag<sizeof(T)>(), m);
}

template <typename T>
HWY_API Mask512<T> And(const Mask512<T> a, Mask512<T> b) {
  return detail::And(hwy::SizeTag<sizeof(T)>(), a, b);
}

template <typename T>
HWY_API Mask512<T> AndNot(const Mask512<T> a, Mask512<T> b) {
  return detail::AndNot(hwy::SizeTag<sizeof(T)>(), a, b);
}

template <typename T>
HWY_API Mask512<T> Or(const Mask512<T> a, Mask512<T> b) {
  return detail::Or(hwy::SizeTag<sizeof(T)>(), a, b);
}

template <typename T>
HWY_API Mask512<T> Xor(const Mask512<T> a, Mask512<T> b) {
  return detail::Xor(hwy::SizeTag<sizeof(T)>(), a, b);
}

template <typename T>
HWY_API Mask512<T> ExclusiveNeither(const Mask512<T> a, Mask512<T> b) {
  return detail::ExclusiveNeither(hwy::SizeTag<sizeof(T)>(), a, b);
}

// ------------------------------ BroadcastSignBit (ShiftRight, compare, mask)

HWY_API Vec512<int8_t> BroadcastSignBit(const Vec512<int8_t> v) {
  const DFromV<decltype(v)> d;
  return VecFromMask(v < Zero(d));
}

HWY_API Vec512<int16_t> BroadcastSignBit(const Vec512<int16_t> v) {
  return ShiftRight<15>(v);
}

HWY_API Vec512<int32_t> BroadcastSignBit(const Vec512<int32_t> v) {
  return ShiftRight<31>(v);
}

HWY_API Vec512<int64_t> BroadcastSignBit(const Vec512<int64_t> v) {
  return Vec512<int64_t>{_mm512_srai_epi64(v.raw, 63)};
}

// ------------------------------ Floating-point classification (Not)

HWY_API Mask512<float> IsNaN(const Vec512<float> v) {
  return Mask512<float>{_mm512_fpclass_ps_mask(v.raw, 0x81)};
}
HWY_API Mask512<double> IsNaN(const Vec512<double> v) {
  return Mask512<double>{_mm512_fpclass_pd_mask(v.raw, 0x81)};
}

HWY_API Mask512<float> IsInf(const Vec512<float> v) {
  return Mask512<float>{_mm512_fpclass_ps_mask(v.raw, 0x18)};
}
HWY_API Mask512<double> IsInf(const Vec512<double> v) {
  return Mask512<double>{_mm512_fpclass_pd_mask(v.raw, 0x18)};
}

// Returns whether normal/subnormal/zero. fpclass doesn't have a flag for
// positive, so we have to check for inf/NaN and negate.
HWY_API Mask512<float> IsFinite(const Vec512<float> v) {
  return Not(Mask512<float>{_mm512_fpclass_ps_mask(v.raw, 0x99)});
}
HWY_API Mask512<double> IsFinite(const Vec512<double> v) {
  return Not(Mask512<double>{_mm512_fpclass_pd_mask(v.raw, 0x99)});
}

// ================================================== MEMORY

// ------------------------------ Load

template <class D, HWY_IF_V_SIZE_D(D, 64), typename T = TFromD<D>>
HWY_API Vec512<T> Load(D /* tag */, const T* HWY_RESTRICT aligned) {
  return Vec512<T>{_mm512_load_si512(aligned)};
}
template <class D, HWY_IF_V_SIZE_D(D, 64)>
HWY_API Vec512<float> Load(D /* tag */, const float* HWY_RESTRICT aligned) {
  return Vec512<float>{_mm512_load_ps(aligned)};
}
template <class D, HWY_IF_V_SIZE_D(D, 64)>
HWY_API Vec512<double> Load(D /* tag */, const double* HWY_RESTRICT aligned) {
  return Vec512<double>{_mm512_load_pd(aligned)};
}

template <class D, HWY_IF_V_SIZE_D(D, 64), typename T = TFromD<D>>
HWY_API Vec512<T> LoadU(D /* tag */, const T* HWY_RESTRICT p) {
  return Vec512<T>{_mm512_loadu_si512(p)};
}
template <class D, HWY_IF_V_SIZE_D(D, 64)>
HWY_API Vec512<float> LoadU(D /* tag */, const float* HWY_RESTRICT p) {
  return Vec512<float>{_mm512_loadu_ps(p)};
}
template <class D, HWY_IF_V_SIZE_D(D, 64)>
HWY_API Vec512<double> LoadU(D /* tag */, const double* HWY_RESTRICT p) {
  return Vec512<double>{_mm512_loadu_pd(p)};
}

// ------------------------------ MaskedLoad

template <class D, HWY_IF_V_SIZE_D(D, 64), typename T = TFromD<D>,
          HWY_IF_T_SIZE(T, 1)>
HWY_API Vec512<T> MaskedLoad(Mask512<T> m, D /* tag */,
                             const T* HWY_RESTRICT p) {
  return Vec512<T>{_mm512_maskz_loadu_epi8(m.raw, p)};
}

template <class D, HWY_IF_V_SIZE_D(D, 64), typename T = TFromD<D>,
          HWY_IF_T_SIZE(T, 2)>
HWY_API Vec512<T> MaskedLoad(Mask512<T> m, D /* tag */,
                             const T* HWY_RESTRICT p) {
  return Vec512<T>{_mm512_maskz_loadu_epi16(m.raw, p)};
}

template <class D, HWY_IF_V_SIZE_D(D, 64), typename T = TFromD<D>,
          HWY_IF_UI32(T)>
HWY_API Vec512<T> MaskedLoad(Mask512<T> m, D /* tag */,
                             const T* HWY_RESTRICT p) {
  return Vec512<T>{_mm512_maskz_loadu_epi32(m.raw, p)};
}

template <class D, HWY_IF_V_SIZE_D(D, 64), typename T = TFromD<D>,
          HWY_IF_UI64(T)>
HWY_API Vec512<T> MaskedLoad(Mask512<T> m, D /* tag */,
                             const T* HWY_RESTRICT p) {
  return Vec512<T>{_mm512_maskz_loadu_epi64(m.raw, p)};
}

template <class D, HWY_IF_V_SIZE_D(D, 64)>
HWY_API Vec512<float> MaskedLoad(Mask512<float> m, D /* tag */,
                                 const float* HWY_RESTRICT p) {
  return Vec512<float>{_mm512_maskz_loadu_ps(m.raw, p)};
}

template <class D, HWY_IF_V_SIZE_D(D, 64)>
HWY_API Vec512<double> MaskedLoad(Mask512<double> m, D /* tag */,
                                  const double* HWY_RESTRICT p) {
  return Vec512<double>{_mm512_maskz_loadu_pd(m.raw, p)};
}

// ------------------------------ MaskedLoadOr

template <class D, HWY_IF_V_SIZE_D(D, 64), typename T = TFromD<D>,
          HWY_IF_T_SIZE(T, 1)>
HWY_API Vec512<T> MaskedLoadOr(VFromD<D> v, Mask512<T> m, D /* tag */,
                               const T* HWY_RESTRICT p) {
  return Vec512<T>{_mm512_mask_loadu_epi8(v.raw, m.raw, p)};
}

template <class D, HWY_IF_V_SIZE_D(D, 64), typename T = TFromD<D>,
          HWY_IF_T_SIZE(T, 2)>
HWY_API Vec512<T> MaskedLoadOr(VFromD<D> v, Mask512<T> m, D /* tag */,
                               const T* HWY_RESTRICT p) {
  return Vec512<T>{_mm512_mask_loadu_epi16(v.raw, m.raw, p)};
}

template <class D, HWY_IF_V_SIZE_D(D, 64), typename T = TFromD<D>,
          HWY_IF_UI32(T)>
HWY_API Vec512<T> MaskedLoadOr(VFromD<D> v, Mask512<T> m, D /* tag */,
                               const T* HWY_RESTRICT p) {
  return Vec512<T>{_mm512_mask_loadu_epi32(v.raw, m.raw, p)};
}

template <class D, HWY_IF_V_SIZE_D(D, 64), typename T = TFromD<D>,
          HWY_IF_UI64(T)>
HWY_API Vec512<T> MaskedLoadOr(VFromD<D> v, Mask512<T> m, D /* tag */,
                               const T* HWY_RESTRICT p) {
  return Vec512<T>{_mm512_mask_loadu_epi64(v.raw, m.raw, p)};
}

template <class D, HWY_IF_V_SIZE_D(D, 64)>
HWY_API Vec512<float> MaskedLoadOr(VFromD<D> v, Mask512<float> m, D /* tag */,
                                   const float* HWY_RESTRICT p) {
  return Vec512<float>{_mm512_mask_loadu_ps(v.raw, m.raw, p)};
}

template <class D, HWY_IF_V_SIZE_D(D, 64)>
HWY_API Vec512<double> MaskedLoadOr(VFromD<D> v, Mask512<double> m, D /* tag */,
                                    const double* HWY_RESTRICT p) {
  return Vec512<double>{_mm512_mask_loadu_pd(v.raw, m.raw, p)};
}

// ------------------------------ LoadDup128

// Loads 128 bit and duplicates into both 128-bit halves. This avoids the
// 3-cycle cost of moving data between 128-bit halves and avoids port 5.
template <class D, HWY_IF_V_SIZE_D(D, 64), typename T = TFromD<D>>
HWY_API Vec512<T> LoadDup128(D /* tag */, const T* const HWY_RESTRICT p) {
  const Full128<T> d128;
  return Vec512<T>{_mm512_broadcast_i32x4(LoadU(d128, p).raw)};
}
template <class D, HWY_IF_V_SIZE_D(D, 64)>
HWY_API Vec512<float> LoadDup128(D /* tag */, const float* HWY_RESTRICT p) {
  const __m128 x4 = _mm_loadu_ps(p);
  return Vec512<float>{_mm512_broadcast_f32x4(x4)};
}

template <class D, HWY_IF_V_SIZE_D(D, 64)>
HWY_API Vec512<double> LoadDup128(D /* tag */, const double* HWY_RESTRICT p) {
  const __m128d x2 = _mm_loadu_pd(p);
  return Vec512<double>{_mm512_broadcast_f64x2(x2)};
}

// ------------------------------ Store

template <class D, HWY_IF_V_SIZE_D(D, 64), typename T = TFromD<D>>
HWY_API void Store(Vec512<T> v, D /* tag */, T* HWY_RESTRICT aligned) {
  _mm512_store_si512(reinterpret_cast<__m512i*>(aligned), v.raw);
}
template <class D, HWY_IF_V_SIZE_D(D, 64)>
HWY_API void Store(Vec512<float> v, D /* tag */, float* HWY_RESTRICT aligned) {
  _mm512_store_ps(aligned, v.raw);
}
template <class D, HWY_IF_V_SIZE_D(D, 64)>
HWY_API void Store(Vec512<double> v, D /* tag */,
                   double* HWY_RESTRICT aligned) {
  _mm512_store_pd(aligned, v.raw);
}

template <class D, HWY_IF_V_SIZE_D(D, 64), typename T = TFromD<D>>
HWY_API void StoreU(Vec512<T> v, D /* tag */, T* HWY_RESTRICT p) {
  _mm512_storeu_si512(reinterpret_cast<__m512i*>(p), v.raw);
}
template <class D, HWY_IF_V_SIZE_D(D, 64)>
HWY_API void StoreU(Vec512<float> v, D /* tag */, float* HWY_RESTRICT p) {
  _mm512_storeu_ps(p, v.raw);
}
template <class D, HWY_IF_V_SIZE_D(D, 64)>
HWY_API void StoreU(Vec512<double> v, D /* tag */, double* HWY_RESTRICT p) {
  _mm512_storeu_pd(p, v.raw);
}

// ------------------------------ BlendedStore

template <class D, HWY_IF_V_SIZE_D(D, 64), typename T = TFromD<D>,
          HWY_IF_T_SIZE(T, 1)>
HWY_API void BlendedStore(Vec512<T> v, Mask512<T> m, D /* tag */,
                          T* HWY_RESTRICT p) {
  _mm512_mask_storeu_epi8(p, m.raw, v.raw);
}

template <class D, HWY_IF_V_SIZE_D(D, 64), typename T = TFromD<D>,
          HWY_IF_T_SIZE(T, 2)>
HWY_API void BlendedStore(Vec512<T> v, Mask512<T> m, D /* tag */,
                          T* HWY_RESTRICT p) {
  _mm512_mask_storeu_epi16(p, m.raw, v.raw);
}

template <class D, HWY_IF_V_SIZE_D(D, 64), typename T = TFromD<D>,
          HWY_IF_UI32(T)>
HWY_API void BlendedStore(Vec512<T> v, Mask512<T> m, D /* tag */,
                          T* HWY_RESTRICT p) {
  _mm512_mask_storeu_epi32(p, m.raw, v.raw);
}

template <class D, HWY_IF_V_SIZE_D(D, 64), typename T = TFromD<D>,
          HWY_IF_UI64(T)>
HWY_API void BlendedStore(Vec512<T> v, Mask512<T> m, D /* tag */,
                          T* HWY_RESTRICT p) {
  _mm512_mask_storeu_epi64(p, m.raw, v.raw);
}

template <class D, HWY_IF_V_SIZE_D(D, 64)>
HWY_API void BlendedStore(Vec512<float> v, Mask512<float> m, D /* tag */,
                          float* HWY_RESTRICT p) {
  _mm512_mask_storeu_ps(p, m.raw, v.raw);
}

template <class D, HWY_IF_V_SIZE_D(D, 64)>
HWY_API void BlendedStore(Vec512<double> v, Mask512<double> m, D /* tag */,
                          double* HWY_RESTRICT p) {
  _mm512_mask_storeu_pd(p, m.raw, v.raw);
}

// ------------------------------ Non-temporal stores

template <class D, HWY_IF_V_SIZE_D(D, 64), typename T = TFromD<D>,
          HWY_IF_NOT_FLOAT(T)>
HWY_API void Stream(Vec512<T> v, D /* tag */, T* HWY_RESTRICT aligned) {
  _mm512_stream_si512(reinterpret_cast<__m512i*>(aligned), v.raw);
}
template <class D, HWY_IF_V_SIZE_D(D, 64)>
HWY_API void Stream(Vec512<float> v, D /* tag */, float* HWY_RESTRICT aligned) {
  _mm512_stream_ps(aligned, v.raw);
}
template <class D, HWY_IF_V_SIZE_D(D, 64)>
HWY_API void Stream(Vec512<double> v, D /* tag */,
                    double* HWY_RESTRICT aligned) {
  _mm512_stream_pd(aligned, v.raw);
}

// ------------------------------ Scatter

// Work around warnings in the intrinsic definitions (passing -1 as a mask).
HWY_DIAGNOSTICS(push)
HWY_DIAGNOSTICS_OFF(disable : 4245 4365, ignored "-Wsign-conversion")

namespace detail {

template <typename T>
HWY_INLINE void ScatterOffset(hwy::SizeTag<4> /* tag */, Vec512<T> v,
                              T* HWY_RESTRICT base, Vec512<int32_t> offset) {
  _mm512_i32scatter_epi32(base, offset.raw, v.raw, 1);
}
template <typename T>
HWY_INLINE void ScatterIndex(hwy::SizeTag<4> /* tag */, Vec512<T> v,
                             T* HWY_RESTRICT base, Vec512<int32_t> index) {
  _mm512_i32scatter_epi32(base, index.raw, v.raw, 4);
}

template <typename T>
HWY_INLINE void ScatterOffset(hwy::SizeTag<8> /* tag */, Vec512<T> v,
                              T* HWY_RESTRICT base, Vec512<int64_t> offset) {
  _mm512_i64scatter_epi64(base, offset.raw, v.raw, 1);
}
template <typename T>
HWY_INLINE void ScatterIndex(hwy::SizeTag<8> /* tag */, Vec512<T> v,
                             T* HWY_RESTRICT base, Vec512<int64_t> index) {
  _mm512_i64scatter_epi64(base, index.raw, v.raw, 8);
}

}  // namespace detail

template <class D, HWY_IF_V_SIZE_D(D, 64), typename TI, typename T = TFromD<D>>
HWY_API void ScatterOffset(Vec512<T> v, D /* tag */, T* HWY_RESTRICT base,
                           Vec512<TI> offset) {
  static_assert(sizeof(T) == sizeof(TI), "Must match for portability");
  return detail::ScatterOffset(hwy::SizeTag<sizeof(T)>(), v, base, offset);
}
template <class D, HWY_IF_V_SIZE_D(D, 64), typename TI, typename T = TFromD<D>>
HWY_API void ScatterIndex(Vec512<T> v, D /* tag */, T* HWY_RESTRICT base,
                          Vec512<TI> index) {
  static_assert(sizeof(T) == sizeof(TI), "Must match for portability");
  return detail::ScatterIndex(hwy::SizeTag<sizeof(T)>(), v, base, index);
}

template <class D, HWY_IF_V_SIZE_D(D, 64)>
HWY_API void ScatterOffset(Vec512<float> v, D /* tag */,
                           float* HWY_RESTRICT base, Vec512<int32_t> offset) {
  _mm512_i32scatter_ps(base, offset.raw, v.raw, 1);
}
template <class D, HWY_IF_V_SIZE_D(D, 64)>
HWY_API void ScatterIndex(Vec512<float> v, D /* tag */,
                          float* HWY_RESTRICT base, Vec512<int32_t> index) {
  _mm512_i32scatter_ps(base, index.raw, v.raw, 4);
}
template <class D, HWY_IF_V_SIZE_D(D, 64)>
HWY_API void ScatterOffset(Vec512<double> v, D /* tag */,
                           double* HWY_RESTRICT base, Vec512<int64_t> offset) {
  _mm512_i64scatter_pd(base, offset.raw, v.raw, 1);
}
template <class D, HWY_IF_V_SIZE_D(D, 64)>
HWY_API void ScatterIndex(Vec512<double> v, D /* tag */,
                          double* HWY_RESTRICT base, Vec512<int64_t> index) {
  _mm512_i64scatter_pd(base, index.raw, v.raw, 8);
}

// ------------------------------ Gather

namespace detail {

template <int kScale, typename T, HWY_IF_UI32(T), HWY_IF_NOT_FLOAT(T)>
HWY_INLINE Vec512<T> NativeGather(const T* HWY_RESTRICT base,
                                  Vec512<int32_t> index) {
  return Vec512<T>{_mm512_i32gather_epi32(index.raw, base, kScale)};
}

template <int kScale, typename T, HWY_IF_UI64(T), HWY_IF_NOT_FLOAT(T)>
HWY_INLINE Vec512<T> NativeGather(const T* HWY_RESTRICT base,
                                  Vec512<int64_t> index) {
  return Vec512<T>{_mm512_i64gather_epi64(index.raw, base, kScale)};
}

template <int kScale>
HWY_INLINE Vec512<float> NativeGather(const float* HWY_RESTRICT base,
                                      Vec512<int32_t> index) {
  return Vec512<float>{_mm512_i32gather_ps(index.raw, base, kScale)};
}

template <int kScale>
HWY_INLINE Vec512<double> NativeGather(const double* HWY_RESTRICT base,
                                       Vec512<int64_t> index) {
  return Vec512<double>{_mm512_i64gather_pd(index.raw, base, kScale)};
}

}  // namespace detail

template <class D, HWY_IF_V_SIZE_D(D, 64), typename T = TFromD<D>, typename TI>
HWY_API Vec512<T> GatherOffset(D /* tag */, const T* HWY_RESTRICT base,
                               Vec512<TI> offset) {
  static_assert(sizeof(T) == sizeof(TI), "Must match for portability");
  return detail::NativeGather<1>(base, offset);
}
template <class D, HWY_IF_V_SIZE_D(D, 64), typename T = TFromD<D>, typename TI>
HWY_API Vec512<T> GatherIndex(D /* tag */, const T* HWY_RESTRICT base,
                              Vec512<TI> index) {
  static_assert(sizeof(T) == sizeof(TI), "Must match for portability");
  return detail::NativeGather<sizeof(T)>(base, index);
}

HWY_DIAGNOSTICS(pop)

// ================================================== SWIZZLE

// ------------------------------ LowerHalf

template <class D, typename T = TFromD<D>, HWY_IF_NOT_FLOAT(T)>
HWY_API Vec256<T> LowerHalf(D /* tag */, Vec512<T> v) {
  return Vec256<T>{_mm512_castsi512_si256(v.raw)};
}
template <class D>
HWY_API Vec256<float> LowerHalf(D /* tag */, Vec512<float> v) {
  return Vec256<float>{_mm512_castps512_ps256(v.raw)};
}
template <class D>
HWY_API Vec256<double> LowerHalf(D /* tag */, Vec512<double> v) {
  return Vec256<double>{_mm512_castpd512_pd256(v.raw)};
}

template <typename T>
HWY_API Vec256<T> LowerHalf(Vec512<T> v) {
  const Half<DFromV<decltype(v)>> dh;
  return LowerHalf(dh, v);
}

// ------------------------------ UpperHalf

template <class D, typename T = TFromD<D>>
HWY_API Vec256<T> UpperHalf(D /* tag */, Vec512<T> v) {
  return Vec256<T>{_mm512_extracti32x8_epi32(v.raw, 1)};
}
template <class D>
HWY_API Vec256<float> UpperHalf(D /* tag */, Vec512<float> v) {
  return Vec256<float>{_mm512_extractf32x8_ps(v.raw, 1)};
}
template <class D>
HWY_API Vec256<double> UpperHalf(D /* tag */, Vec512<double> v) {
  return Vec256<double>{_mm512_extractf64x4_pd(v.raw, 1)};
}

// ------------------------------ ExtractLane (Store)
template <typename T>
HWY_API T ExtractLane(const Vec512<T> v, size_t i) {
  const DFromV<decltype(v)> d;
  HWY_DASSERT(i < Lanes(d));
  alignas(64) T lanes[64 / sizeof(T)];
  Store(v, d, lanes);
  return lanes[i];
}

// ------------------------------ InsertLane (Store)
template <typename T>
HWY_API Vec512<T> InsertLane(const Vec512<T> v, size_t i, T t) {
  return detail::InsertLaneUsingBroadcastAndBlend(v, i, t);
}

// ------------------------------ GetLane (LowerHalf)
template <typename T>
HWY_API T GetLane(const Vec512<T> v) {
  return GetLane(LowerHalf(v));
}

// ------------------------------ ZeroExtendVector

template <class D, HWY_IF_V_SIZE_D(D, 64), typename T = TFromD<D>>
HWY_API Vec512<T> ZeroExtendVector(D d, Vec256<T> lo) {
#if HWY_HAVE_ZEXT  // See definition/comment in x86_256-inl.h.
  (void)d;
  return Vec512<T>{_mm512_zextsi256_si512(lo.raw)};
#else
  return Vec512<T>{_mm512_inserti32x8(Zero(d).raw, lo.raw, 0)};
#endif
}
template <class D, HWY_IF_V_SIZE_D(D, 64)>
HWY_API Vec512<float> ZeroExtendVector(D d, Vec256<float> lo) {
#if HWY_HAVE_ZEXT
  (void)d;
  return Vec512<float>{_mm512_zextps256_ps512(lo.raw)};
#else
  return Vec512<float>{_mm512_insertf32x8(Zero(d).raw, lo.raw, 0)};
#endif
}
template <class D, HWY_IF_V_SIZE_D(D, 64)>
HWY_API Vec512<double> ZeroExtendVector(D d, Vec256<double> lo) {
#if HWY_HAVE_ZEXT
  (void)d;
  return Vec512<double>{_mm512_zextpd256_pd512(lo.raw)};
#else
  return Vec512<double>{_mm512_insertf64x4(Zero(d).raw, lo.raw, 0)};
#endif
}

// ------------------------------ ZeroExtendResizeBitCast

namespace detail {

template <class DTo, class DFrom, HWY_IF_NOT_FLOAT_D(DTo)>
HWY_INLINE VFromD<DTo> ZeroExtendResizeBitCast(
    hwy::SizeTag<16> /* from_size_tag */, hwy::SizeTag<64> /* to_size_tag */,
    DTo d_to, DFrom d_from, VFromD<DFrom> v) {
  const Repartition<uint8_t, decltype(d_from)> du8_from;
  const auto vu8 = BitCast(du8_from, v);
#if HWY_HAVE_ZEXT
  (void)d_to;
  return VFromD<DTo>{_mm512_zextsi128_si512(vu8.raw)};
#else
  return VFromD<DTo>{_mm512_inserti32x4(Zero(d_to).raw, vu8.raw, 0)};
#endif
}

template <class DTo, class DFrom, HWY_IF_F32_D(DTo)>
HWY_INLINE VFromD<DTo> ZeroExtendResizeBitCast(
    hwy::SizeTag<16> /* from_size_tag */, hwy::SizeTag<64> /* to_size_tag */,
    DTo d_to, DFrom d_from, VFromD<DFrom> v) {
  const Repartition<float, decltype(d_from)> df32_from;
  const auto vf32 = BitCast(df32_from, v);
#if HWY_HAVE_ZEXT
  (void)d_to;
  return Vec512<float>{_mm512_zextps128_ps512(vf32.raw)};
#else
  return Vec512<float>{_mm512_insertf32x4(Zero(d_to).raw, vf32.raw, 0)};
#endif
}

template <class DTo, class DFrom, HWY_IF_F64_D(DTo)>
HWY_INLINE Vec512<double> ZeroExtendResizeBitCast(
    hwy::SizeTag<16> /* from_size_tag */, hwy::SizeTag<64> /* to_size_tag */,
    DTo d_to, DFrom d_from, VFromD<DFrom> v) {
  const Repartition<double, decltype(d_from)> df64_from;
  const auto vf64 = BitCast(df64_from, v);
#if HWY_HAVE_ZEXT
  (void)d_to;
  return Vec512<double>{_mm512_zextpd128_pd512(vf64.raw)};
#else
  return Vec512<double>{_mm512_insertf64x2(Zero(d_to).raw, vf64.raw, 0)};
#endif
}

template <class DTo, class DFrom>
HWY_INLINE VFromD<DTo> ZeroExtendResizeBitCast(
    hwy::SizeTag<8> /* from_size_tag */, hwy::SizeTag<64> /* to_size_tag */,
    DTo d_to, DFrom d_from, VFromD<DFrom> v) {
  const Twice<decltype(d_from)> dt_from;
  return ZeroExtendResizeBitCast(hwy::SizeTag<16>(), hwy::SizeTag<64>(), d_to,
                                 dt_from, ZeroExtendVector(dt_from, v));
}

}  // namespace detail

// ------------------------------ Combine

template <class D, typename T = TFromD<D>>
HWY_API Vec512<T> Combine(D d, Vec256<T> hi, Vec256<T> lo) {
  const auto lo512 = ZeroExtendVector(d, lo);
  return Vec512<T>{_mm512_inserti32x8(lo512.raw, hi.raw, 1)};
}
template <class D>
HWY_API Vec512<float> Combine(D d, Vec256<float> hi, Vec256<float> lo) {
  const auto lo512 = ZeroExtendVector(d, lo);
  return Vec512<float>{_mm512_insertf32x8(lo512.raw, hi.raw, 1)};
}
template <class D>
HWY_API Vec512<double> Combine(D d, Vec256<double> hi, Vec256<double> lo) {
  const auto lo512 = ZeroExtendVector(d, lo);
  return Vec512<double>{_mm512_insertf64x4(lo512.raw, hi.raw, 1)};
}

// ------------------------------ ShiftLeftBytes

template <int kBytes, class D, typename T = TFromD<D>>
HWY_API Vec512<T> ShiftLeftBytes(D /* tag */, const Vec512<T> v) {
  static_assert(0 <= kBytes && kBytes <= 16, "Invalid kBytes");
  return Vec512<T>{_mm512_bslli_epi128(v.raw, kBytes)};
}

template <int kBytes, typename T>
HWY_API Vec512<T> ShiftLeftBytes(const Vec512<T> v) {
  const DFromV<decltype(v)> d;
  return ShiftLeftBytes<kBytes>(d, v);
}

// ------------------------------ ShiftLeftLanes

template <int kLanes, class D, typename T = TFromD<D>>
HWY_API Vec512<T> ShiftLeftLanes(D d, const Vec512<T> v) {
  const Repartition<uint8_t, decltype(d)> d8;
  return BitCast(d, ShiftLeftBytes<kLanes * sizeof(T)>(BitCast(d8, v)));
}

template <int kLanes, typename T>
HWY_API Vec512<T> ShiftLeftLanes(const Vec512<T> v) {
  const DFromV<decltype(v)> d;
  return ShiftLeftLanes<kLanes>(d, v);
}

// ------------------------------ ShiftRightBytes
template <int kBytes, class D, typename T = TFromD<D>>
HWY_API Vec512<T> ShiftRightBytes(D /* tag */, const Vec512<T> v) {
  static_assert(0 <= kBytes && kBytes <= 16, "Invalid kBytes");
  return Vec512<T>{_mm512_bsrli_epi128(v.raw, kBytes)};
}

// ------------------------------ ShiftRightLanes
template <int kLanes, class D, typename T = TFromD<D>>
HWY_API Vec512<T> ShiftRightLanes(D d, const Vec512<T> v) {
  const Repartition<uint8_t, decltype(d)> d8;
  return BitCast(d, ShiftRightBytes<kLanes * sizeof(T)>(d8, BitCast(d8, v)));
}

// ------------------------------ CombineShiftRightBytes

template <int kBytes, class D, typename T = TFromD<D>>
HWY_API Vec512<T> CombineShiftRightBytes(D d, Vec512<T> hi, Vec512<T> lo) {
  const Repartition<uint8_t, decltype(d)> d8;
  return BitCast(d, Vec512<uint8_t>{_mm512_alignr_epi8(
                        BitCast(d8, hi).raw, BitCast(d8, lo).raw, kBytes)});
}

// ------------------------------ Broadcast/splat any lane

// Unsigned
template <int kLane>
HWY_API Vec512<uint16_t> Broadcast(const Vec512<uint16_t> v) {
  static_assert(0 <= kLane && kLane < 8, "Invalid lane");
  if (kLane < 4) {
    const __m512i lo = _mm512_shufflelo_epi16(v.raw, (0x55 * kLane) & 0xFF);
    return Vec512<uint16_t>{_mm512_unpacklo_epi64(lo, lo)};
  } else {
    const __m512i hi =
        _mm512_shufflehi_epi16(v.raw, (0x55 * (kLane - 4)) & 0xFF);
    return Vec512<uint16_t>{_mm512_unpackhi_epi64(hi, hi)};
  }
}
template <int kLane>
HWY_API Vec512<uint32_t> Broadcast(const Vec512<uint32_t> v) {
  static_assert(0 <= kLane && kLane < 4, "Invalid lane");
  constexpr _MM_PERM_ENUM perm = static_cast<_MM_PERM_ENUM>(0x55 * kLane);
  return Vec512<uint32_t>{_mm512_shuffle_epi32(v.raw, perm)};
}
template <int kLane>
HWY_API Vec512<uint64_t> Broadcast(const Vec512<uint64_t> v) {
  static_assert(0 <= kLane && kLane < 2, "Invalid lane");
  constexpr _MM_PERM_ENUM perm = kLane ? _MM_PERM_DCDC : _MM_PERM_BABA;
  return Vec512<uint64_t>{_mm512_shuffle_epi32(v.raw, perm)};
}

// Signed
template <int kLane>
HWY_API Vec512<int16_t> Broadcast(const Vec512<int16_t> v) {
  static_assert(0 <= kLane && kLane < 8, "Invalid lane");
  if (kLane < 4) {
    const __m512i lo = _mm512_shufflelo_epi16(v.raw, (0x55 * kLane) & 0xFF);
    return Vec512<int16_t>{_mm512_unpacklo_epi64(lo, lo)};
  } else {
    const __m512i hi =
        _mm512_shufflehi_epi16(v.raw, (0x55 * (kLane - 4)) & 0xFF);
    return Vec512<int16_t>{_mm512_unpackhi_epi64(hi, hi)};
  }
}
template <int kLane>
HWY_API Vec512<int32_t> Broadcast(const Vec512<int32_t> v) {
  static_assert(0 <= kLane && kLane < 4, "Invalid lane");
  constexpr _MM_PERM_ENUM perm = static_cast<_MM_PERM_ENUM>(0x55 * kLane);
  return Vec512<int32_t>{_mm512_shuffle_epi32(v.raw, perm)};
}
template <int kLane>
HWY_API Vec512<int64_t> Broadcast(const Vec512<int64_t> v) {
  static_assert(0 <= kLane && kLane < 2, "Invalid lane");
  constexpr _MM_PERM_ENUM perm = kLane ? _MM_PERM_DCDC : _MM_PERM_BABA;
  return Vec512<int64_t>{_mm512_shuffle_epi32(v.raw, perm)};
}

// Float
template <int kLane>
HWY_API Vec512<float> Broadcast(const Vec512<float> v) {
  static_assert(0 <= kLane && kLane < 4, "Invalid lane");
  constexpr _MM_PERM_ENUM perm = static_cast<_MM_PERM_ENUM>(0x55 * kLane);
  return Vec512<float>{_mm512_shuffle_ps(v.raw, v.raw, perm)};
}
template <int kLane>
HWY_API Vec512<double> Broadcast(const Vec512<double> v) {
  static_assert(0 <= kLane && kLane < 2, "Invalid lane");
  constexpr _MM_PERM_ENUM perm = static_cast<_MM_PERM_ENUM>(0xFF * kLane);
  return Vec512<double>{_mm512_shuffle_pd(v.raw, v.raw, perm)};
}

// ------------------------------ Hard-coded shuffles

// Notation: let Vec512<int32_t> have lanes 7,6,5,4,3,2,1,0 (0 is
// least-significant). Shuffle0321 rotates four-lane blocks one lane to the
// right (the previous least-significant lane is now most-significant =>
// 47650321). These could also be implemented via CombineShiftRightBytes but
// the shuffle_abcd notation is more convenient.

// Swap 32-bit halves in 64-bit halves.
template <typename T, HWY_IF_UI32(T)>
HWY_API Vec512<T> Shuffle2301(const Vec512<T> v) {
  return Vec512<T>{_mm512_shuffle_epi32(v.raw, _MM_PERM_CDAB)};
}
HWY_API Vec512<float> Shuffle2301(const Vec512<float> v) {
  return Vec512<float>{_mm512_shuffle_ps(v.raw, v.raw, _MM_PERM_CDAB)};
}

namespace detail {

template <typename T, HWY_IF_T_SIZE(T, 4)>
HWY_API Vec512<T> ShuffleTwo2301(const Vec512<T> a, const Vec512<T> b) {
  const DFromV<decltype(a)> d;
  const RebindToFloat<decltype(d)> df;
  return BitCast(
      d, Vec512<float>{_mm512_shuffle_ps(BitCast(df, a).raw, BitCast(df, b).raw,
                                         _MM_PERM_CDAB)});
}
template <typename T, HWY_IF_T_SIZE(T, 4)>
HWY_API Vec512<T> ShuffleTwo1230(const Vec512<T> a, const Vec512<T> b) {
  const DFromV<decltype(a)> d;
  const RebindToFloat<decltype(d)> df;
  return BitCast(
      d, Vec512<float>{_mm512_shuffle_ps(BitCast(df, a).raw, BitCast(df, b).raw,
                                         _MM_PERM_BCDA)});
}
template <typename T, HWY_IF_T_SIZE(T, 4)>
HWY_API Vec512<T> ShuffleTwo3012(const Vec512<T> a, const Vec512<T> b) {
  const DFromV<decltype(a)> d;
  const RebindToFloat<decltype(d)> df;
  return BitCast(
      d, Vec512<float>{_mm512_shuffle_ps(BitCast(df, a).raw, BitCast(df, b).raw,
                                         _MM_PERM_DABC)});
}

}  // namespace detail

// Swap 64-bit halves
HWY_API Vec512<uint32_t> Shuffle1032(const Vec512<uint32_t> v) {
  return Vec512<uint32_t>{_mm512_shuffle_epi32(v.raw, _MM_PERM_BADC)};
}
HWY_API Vec512<int32_t> Shuffle1032(const Vec512<int32_t> v) {
  return Vec512<int32_t>{_mm512_shuffle_epi32(v.raw, _MM_PERM_BADC)};
}
HWY_API Vec512<float> Shuffle1032(const Vec512<float> v) {
  // Shorter encoding than _mm512_permute_ps.
  return Vec512<float>{_mm512_shuffle_ps(v.raw, v.raw, _MM_PERM_BADC)};
}
HWY_API Vec512<uint64_t> Shuffle01(const Vec512<uint64_t> v) {
  return Vec512<uint64_t>{_mm512_shuffle_epi32(v.raw, _MM_PERM_BADC)};
}
HWY_API Vec512<int64_t> Shuffle01(const Vec512<int64_t> v) {
  return Vec512<int64_t>{_mm512_shuffle_epi32(v.raw, _MM_PERM_BADC)};
}
HWY_API Vec512<double> Shuffle01(const Vec512<double> v) {
  // Shorter encoding than _mm512_permute_pd.
  return Vec512<double>{_mm512_shuffle_pd(v.raw, v.raw, _MM_PERM_BBBB)};
}

// Rotate right 32 bits
HWY_API Vec512<uint32_t> Shuffle0321(const Vec512<uint32_t> v) {
  return Vec512<uint32_t>{_mm512_shuffle_epi32(v.raw, _MM_PERM_ADCB)};
}
HWY_API Vec512<int32_t> Shuffle0321(const Vec512<int32_t> v) {
  return Vec512<int32_t>{_mm512_shuffle_epi32(v.raw, _MM_PERM_ADCB)};
}
HWY_API Vec512<float> Shuffle0321(const Vec512<float> v) {
  return Vec512<float>{_mm512_shuffle_ps(v.raw, v.raw, _MM_PERM_ADCB)};
}
// Rotate left 32 bits
HWY_API Vec512<uint32_t> Shuffle2103(const Vec512<uint32_t> v) {
  return Vec512<uint32_t>{_mm512_shuffle_epi32(v.raw, _MM_PERM_CBAD)};
}
HWY_API Vec512<int32_t> Shuffle2103(const Vec512<int32_t> v) {
  return Vec512<int32_t>{_mm512_shuffle_epi32(v.raw, _MM_PERM_CBAD)};
}
HWY_API Vec512<float> Shuffle2103(const Vec512<float> v) {
  return Vec512<float>{_mm512_shuffle_ps(v.raw, v.raw, _MM_PERM_CBAD)};
}

// Reverse
HWY_API Vec512<uint32_t> Shuffle0123(const Vec512<uint32_t> v) {
  return Vec512<uint32_t>{_mm512_shuffle_epi32(v.raw, _MM_PERM_ABCD)};
}
HWY_API Vec512<int32_t> Shuffle0123(const Vec512<int32_t> v) {
  return Vec512<int32_t>{_mm512_shuffle_epi32(v.raw, _MM_PERM_ABCD)};
}
HWY_API Vec512<float> Shuffle0123(const Vec512<float> v) {
  return Vec512<float>{_mm512_shuffle_ps(v.raw, v.raw, _MM_PERM_ABCD)};
}

// ------------------------------ TableLookupLanes

// Returned by SetTableIndices/IndicesFromVec for use by TableLookupLanes.
template <typename T>
struct Indices512 {
  __m512i raw;
};

template <class D, typename T = TFromD<D>, typename TI>
HWY_API Indices512<T> IndicesFromVec(D /* tag */, Vec512<TI> vec) {
  static_assert(sizeof(T) == sizeof(TI), "Index size must match lane");
#if HWY_IS_DEBUG_BUILD
  const DFromV<decltype(vec)> di;
  const RebindToUnsigned<decltype(di)> du;
  using TU = MakeUnsigned<T>;
  const auto vec_u = BitCast(du, vec);
  HWY_DASSERT(
      AllTrue(du, Lt(vec_u, Set(du, static_cast<TU>(128 / sizeof(T))))));
#endif
  return Indices512<T>{vec.raw};
}

template <class D, HWY_IF_V_SIZE_D(D, 64), typename T = TFromD<D>, typename TI>
HWY_API Indices512<T> SetTableIndices(D d, const TI* idx) {
  const Rebind<TI, decltype(d)> di;
  return IndicesFromVec(d, LoadU(di, idx));
}

template <typename T, HWY_IF_T_SIZE(T, 1)>
HWY_API Vec512<T> TableLookupLanes(Vec512<T> v, Indices512<T> idx) {
#if HWY_TARGET <= HWY_AVX3_DL
  return Vec512<T>{_mm512_permutexvar_epi8(idx.raw, v.raw)};
#else
  const DFromV<decltype(v)> d;
  const Repartition<uint16_t, decltype(d)> du16;
  const Vec512<T> idx_vec{idx.raw};

  const auto bd_sel_mask =
      MaskFromVec(BitCast(d, ShiftLeft<3>(BitCast(du16, idx_vec))));
  const auto cd_sel_mask =
      MaskFromVec(BitCast(d, ShiftLeft<2>(BitCast(du16, idx_vec))));

  const Vec512<T> v_a{_mm512_shuffle_i32x4(v.raw, v.raw, 0x00)};
  const Vec512<T> v_b{_mm512_shuffle_i32x4(v.raw, v.raw, 0x55)};
  const Vec512<T> v_c{_mm512_shuffle_i32x4(v.raw, v.raw, 0xAA)};
  const Vec512<T> v_d{_mm512_shuffle_i32x4(v.raw, v.raw, 0xFF)};

  const auto shuf_a = TableLookupBytes(v_a, idx_vec);
  const auto shuf_c = TableLookupBytes(v_c, idx_vec);
  const Vec512<T> shuf_ab{_mm512_mask_shuffle_epi8(shuf_a.raw, bd_sel_mask.raw,
                                                   v_b.raw, idx_vec.raw)};
  const Vec512<T> shuf_cd{_mm512_mask_shuffle_epi8(shuf_c.raw, bd_sel_mask.raw,
                                                   v_d.raw, idx_vec.raw)};
  return IfThenElse(cd_sel_mask, shuf_cd, shuf_ab);
#endif
}

template <typename T, HWY_IF_T_SIZE(T, 2)>
HWY_API Vec512<T> TableLookupLanes(Vec512<T> v, Indices512<T> idx) {
  return Vec512<T>{_mm512_permutexvar_epi16(idx.raw, v.raw)};
}

template <typename T, HWY_IF_T_SIZE(T, 4)>
HWY_API Vec512<T> TableLookupLanes(Vec512<T> v, Indices512<T> idx) {
  return Vec512<T>{_mm512_permutexvar_epi32(idx.raw, v.raw)};
}

template <typename T, HWY_IF_T_SIZE(T, 8)>
HWY_API Vec512<T> TableLookupLanes(Vec512<T> v, Indices512<T> idx) {
  return Vec512<T>{_mm512_permutexvar_epi64(idx.raw, v.raw)};
}

HWY_API Vec512<float> TableLookupLanes(Vec512<float> v, Indices512<float> idx) {
  return Vec512<float>{_mm512_permutexvar_ps(idx.raw, v.raw)};
}

HWY_API Vec512<double> TableLookupLanes(Vec512<double> v,
                                        Indices512<double> idx) {
  return Vec512<double>{_mm512_permutexvar_pd(idx.raw, v.raw)};
}

template <typename T, HWY_IF_T_SIZE(T, 1)>
HWY_API Vec512<T> TwoTablesLookupLanes(Vec512<T> a, Vec512<T> b,
                                       Indices512<T> idx) {
#if HWY_TARGET <= HWY_AVX3_DL
  return Vec512<T>{_mm512_permutex2var_epi8(a.raw, idx.raw, b.raw)};
#else
  const DFromV<decltype(a)> d;
  const auto b_sel_mask =
      MaskFromVec(BitCast(d, ShiftLeft<1>(Vec512<uint16_t>{idx.raw})));
  return IfThenElse(b_sel_mask, TableLookupLanes(b, idx),
                    TableLookupLanes(a, idx));
#endif
}

template <typename T, HWY_IF_T_SIZE(T, 2)>
HWY_API Vec512<T> TwoTablesLookupLanes(Vec512<T> a, Vec512<T> b,
                                       Indices512<T> idx) {
  return Vec512<T>{_mm512_permutex2var_epi16(a.raw, idx.raw, b.raw)};
}

template <typename T, HWY_IF_UI32(T)>
HWY_API Vec512<T> TwoTablesLookupLanes(Vec512<T> a, Vec512<T> b,
                                       Indices512<T> idx) {
  return Vec512<T>{_mm512_permutex2var_epi32(a.raw, idx.raw, b.raw)};
}

HWY_API Vec512<float> TwoTablesLookupLanes(Vec512<float> a, Vec512<float> b,
                                           Indices512<float> idx) {
  return Vec512<float>{_mm512_permutex2var_ps(a.raw, idx.raw, b.raw)};
}

template <typename T, HWY_IF_UI64(T)>
HWY_API Vec512<T> TwoTablesLookupLanes(Vec512<T> a, Vec512<T> b,
                                       Indices512<T> idx) {
  return Vec512<T>{_mm512_permutex2var_epi64(a.raw, idx.raw, b.raw)};
}

HWY_API Vec512<double> TwoTablesLookupLanes(Vec512<double> a, Vec512<double> b,
                                            Indices512<double> idx) {
  return Vec512<double>{_mm512_permutex2var_pd(a.raw, idx.raw, b.raw)};
}

// ------------------------------ Reverse

template <class D, typename T = TFromD<D>, HWY_IF_T_SIZE(T, 1)>
HWY_API Vec512<T> Reverse(D d, const Vec512<T> v) {
#if HWY_TARGET <= HWY_AVX3_DL
  const RebindToSigned<decltype(d)> di;
  alignas(64) static constexpr int8_t kReverse[64] = {
      63, 62, 61, 60, 59, 58, 57, 56, 55, 54, 53, 52, 51, 50, 49, 48,
      47, 46, 45, 44, 43, 42, 41, 40, 39, 38, 37, 36, 35, 34, 33, 32,
      31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16,
      15, 14, 13, 12, 11, 10, 9,  8,  7,  6,  5,  4,  3,  2,  1,  0};
  const Vec512<int8_t> idx = Load(di, kReverse);
  return BitCast(
      d, Vec512<int8_t>{_mm512_permutexvar_epi8(idx.raw, BitCast(di, v).raw)});
#else
  const RepartitionToWide<decltype(d)> d16;
  return BitCast(d, Reverse(d16, RotateRight<8>(BitCast(d16, v))));
#endif
}

template <class D, typename T = TFromD<D>, HWY_IF_T_SIZE(T, 2)>
HWY_API Vec512<T> Reverse(D d, const Vec512<T> v) {
  const RebindToSigned<decltype(d)> di;
  alignas(64) static constexpr int16_t kReverse[32] = {
      31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16,
      15, 14, 13, 12, 11, 10, 9,  8,  7,  6,  5,  4,  3,  2,  1,  0};
  const Vec512<int16_t> idx = Load(di, kReverse);
  return BitCast(d, Vec512<int16_t>{
                        _mm512_permutexvar_epi16(idx.raw, BitCast(di, v).raw)});
}

template <class D, typename T = TFromD<D>, HWY_IF_T_SIZE(T, 4)>
HWY_API Vec512<T> Reverse(D d, const Vec512<T> v) {
  alignas(64) static constexpr int32_t kReverse[16] = {
      15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0};
  return TableLookupLanes(v, SetTableIndices(d, kReverse));
}

template <class D, typename T = TFromD<D>, HWY_IF_T_SIZE(T, 8)>
HWY_API Vec512<T> Reverse(D d, const Vec512<T> v) {
  alignas(64) static constexpr int64_t kReverse[8] = {7, 6, 5, 4, 3, 2, 1, 0};
  return TableLookupLanes(v, SetTableIndices(d, kReverse));
}

// ------------------------------ Reverse2 (in x86_128)

// ------------------------------ Reverse4

template <class D, typename T = TFromD<D>, HWY_IF_T_SIZE(T, 2)>
HWY_API Vec512<T> Reverse4(D d, const Vec512<T> v) {
  const RebindToSigned<decltype(d)> di;
  alignas(64) static constexpr int16_t kReverse4[32] = {
      3,  2,  1,  0,  7,  6,  5,  4,  11, 10, 9,  8,  15, 14, 13, 12,
      19, 18, 17, 16, 23, 22, 21, 20, 27, 26, 25, 24, 31, 30, 29, 28};
  const Vec512<int16_t> idx = Load(di, kReverse4);
  return BitCast(d, Vec512<int16_t>{
                        _mm512_permutexvar_epi16(idx.raw, BitCast(di, v).raw)});
}

// 32 bit Reverse4 defined in x86_128.

template <class D, typename T = TFromD<D>, HWY_IF_UI64(T)>
HWY_API Vec512<T> Reverse4(D /* tag */, const Vec512<T> v) {
  return Vec512<T>{_mm512_permutex_epi64(v.raw, _MM_SHUFFLE(0, 1, 2, 3))};
}
template <class D>
HWY_API Vec512<double> Reverse4(D /* tag */, Vec512<double> v) {
  return Vec512<double>{_mm512_permutex_pd(v.raw, _MM_SHUFFLE(0, 1, 2, 3))};
}

// ------------------------------ Reverse8

template <class D, typename T = TFromD<D>, HWY_IF_T_SIZE(T, 2)>
HWY_API Vec512<T> Reverse8(D d, const Vec512<T> v) {
  const RebindToSigned<decltype(d)> di;
  alignas(64) static constexpr int16_t kReverse8[32] = {
      7,  6,  5,  4,  3,  2,  1,  0,  15, 14, 13, 12, 11, 10, 9,  8,
      23, 22, 21, 20, 19, 18, 17, 16, 31, 30, 29, 28, 27, 26, 25, 24};
  const Vec512<int16_t> idx = Load(di, kReverse8);
  return BitCast(d, Vec512<int16_t>{
                        _mm512_permutexvar_epi16(idx.raw, BitCast(di, v).raw)});
}

template <class D, typename T = TFromD<D>, HWY_IF_T_SIZE(T, 4)>
HWY_API Vec512<T> Reverse8(D d, const Vec512<T> v) {
  const RebindToSigned<decltype(d)> di;
  alignas(64) static constexpr int32_t kReverse8[16] = {
      7, 6, 5, 4, 3, 2, 1, 0, 15, 14, 13, 12, 11, 10, 9, 8};
  const Vec512<int32_t> idx = Load(di, kReverse8);
  return BitCast(d, Vec512<int32_t>{
                        _mm512_permutexvar_epi32(idx.raw, BitCast(di, v).raw)});
}

template <class D, typename T = TFromD<D>, HWY_IF_T_SIZE(T, 8)>
HWY_API Vec512<T> Reverse8(D d, const Vec512<T> v) {
  return Reverse(d, v);
}

// ------------------------------ ReverseBits

#if HWY_TARGET <= HWY_AVX3_DL
template <class V, HWY_IF_T_SIZE_V(V, 1), HWY_IF_V_SIZE_D(DFromV<V>, 64)>
HWY_API V ReverseBits(V v) {
  const Full512<uint64_t> du64;
  const auto affine_matrix = Set(du64, 0x8040201008040201u);
  return V{_mm512_gf2p8affine_epi64_epi8(v.raw, affine_matrix.raw, 0)};
}
#endif  // HWY_TARGET <= HWY_AVX3_DL

// ------------------------------ InterleaveLower

// Interleaves lanes from halves of the 128-bit blocks of "a" (which provides
// the least-significant lane) and "b". To concatenate two half-width integers
// into one, use ZipLower/Upper instead (also works with scalar).

HWY_API Vec512<uint8_t> InterleaveLower(Vec512<uint8_t> a, Vec512<uint8_t> b) {
  return Vec512<uint8_t>{_mm512_unpacklo_epi8(a.raw, b.raw)};
}
HWY_API Vec512<uint16_t> InterleaveLower(Vec512<uint16_t> a,
                                         Vec512<uint16_t> b) {
  return Vec512<uint16_t>{_mm512_unpacklo_epi16(a.raw, b.raw)};
}
HWY_API Vec512<uint32_t> InterleaveLower(Vec512<uint32_t> a,
                                         Vec512<uint32_t> b) {
  return Vec512<uint32_t>{_mm512_unpacklo_epi32(a.raw, b.raw)};
}
HWY_API Vec512<uint64_t> InterleaveLower(Vec512<uint64_t> a,
                                         Vec512<uint64_t> b) {
  return Vec512<uint64_t>{_mm512_unpacklo_epi64(a.raw, b.raw)};
}

HWY_API Vec512<int8_t> InterleaveLower(Vec512<int8_t> a, Vec512<int8_t> b) {
  return Vec512<int8_t>{_mm512_unpacklo_epi8(a.raw, b.raw)};
}
HWY_API Vec512<int16_t> InterleaveLower(Vec512<int16_t> a, Vec512<int16_t> b) {
  return Vec512<int16_t>{_mm512_unpacklo_epi16(a.raw, b.raw)};
}
HWY_API Vec512<int32_t> InterleaveLower(Vec512<int32_t> a, Vec512<int32_t> b) {
  return Vec512<int32_t>{_mm512_unpacklo_epi32(a.raw, b.raw)};
}
HWY_API Vec512<int64_t> InterleaveLower(Vec512<int64_t> a, Vec512<int64_t> b) {
  return Vec512<int64_t>{_mm512_unpacklo_epi64(a.raw, b.raw)};
}

HWY_API Vec512<float> InterleaveLower(Vec512<float> a, Vec512<float> b) {
  return Vec512<float>{_mm512_unpacklo_ps(a.raw, b.raw)};
}
HWY_API Vec512<double> InterleaveLower(Vec512<double> a, Vec512<double> b) {
  return Vec512<double>{_mm512_unpacklo_pd(a.raw, b.raw)};
}

// ------------------------------ InterleaveUpper

// All functions inside detail lack the required D parameter.
namespace detail {

HWY_API Vec512<uint8_t> InterleaveUpper(Vec512<uint8_t> a, Vec512<uint8_t> b) {
  return Vec512<uint8_t>{_mm512_unpackhi_epi8(a.raw, b.raw)};
}
HWY_API Vec512<uint16_t> InterleaveUpper(Vec512<uint16_t> a,
                                         Vec512<uint16_t> b) {
  return Vec512<uint16_t>{_mm512_unpackhi_epi16(a.raw, b.raw)};
}
HWY_API Vec512<uint32_t> InterleaveUpper(Vec512<uint32_t> a,
                                         Vec512<uint32_t> b) {
  return Vec512<uint32_t>{_mm512_unpackhi_epi32(a.raw, b.raw)};
}
HWY_API Vec512<uint64_t> InterleaveUpper(Vec512<uint64_t> a,
                                         Vec512<uint64_t> b) {
  return Vec512<uint64_t>{_mm512_unpackhi_epi64(a.raw, b.raw)};
}

HWY_API Vec512<int8_t> InterleaveUpper(Vec512<int8_t> a, Vec512<int8_t> b) {
  return Vec512<int8_t>{_mm512_unpackhi_epi8(a.raw, b.raw)};
}
HWY_API Vec512<int16_t> InterleaveUpper(Vec512<int16_t> a, Vec512<int16_t> b) {
  return Vec512<int16_t>{_mm512_unpackhi_epi16(a.raw, b.raw)};
}
HWY_API Vec512<int32_t> InterleaveUpper(Vec512<int32_t> a, Vec512<int32_t> b) {
  return Vec512<int32_t>{_mm512_unpackhi_epi32(a.raw, b.raw)};
}
HWY_API Vec512<int64_t> InterleaveUpper(Vec512<int64_t> a, Vec512<int64_t> b) {
  return Vec512<int64_t>{_mm512_unpackhi_epi64(a.raw, b.raw)};
}

HWY_API Vec512<float> InterleaveUpper(Vec512<float> a, Vec512<float> b) {
  return Vec512<float>{_mm512_unpackhi_ps(a.raw, b.raw)};
}
HWY_API Vec512<double> InterleaveUpper(Vec512<double> a, Vec512<double> b) {
  return Vec512<double>{_mm512_unpackhi_pd(a.raw, b.raw)};
}

}  // namespace detail

template <class D, typename T = TFromD<D>>
HWY_API Vec512<T> InterleaveUpper(D /* tag */, Vec512<T> a, Vec512<T> b) {
  return detail::InterleaveUpper(a, b);
}

// ------------------------------ ZipLower/ZipUpper (InterleaveLower)

// Same as Interleave*, except that the return lanes are double-width integers;
// this is necessary because the single-lane scalar cannot return two values.
template <typename T, typename TW = MakeWide<T>>
HWY_API Vec512<TW> ZipLower(Vec512<T> a, Vec512<T> b) {
  const RepartitionToWide<DFromV<decltype(a)>> dw;
  return BitCast(dw, InterleaveLower(a, b));
}
template <class DW, typename T>
HWY_API VFromD<DW> ZipLower(DW dw, Vec512<T> a, Vec512<T> b) {
  return BitCast(dw, InterleaveLower(a, b));
}

template <class DW, typename T>
HWY_API VFromD<DW> ZipUpper(DW dw, Vec512<T> a, Vec512<T> b) {
  const DFromV<decltype(a)> d;
  return BitCast(dw, InterleaveUpper(d, a, b));
}

// ------------------------------ Concat* halves

// hiH,hiL loH,loL |-> hiL,loL (= lower halves)
template <class D, typename T = TFromD<D>>
HWY_API Vec512<T> ConcatLowerLower(D /* tag */, Vec512<T> hi, Vec512<T> lo) {
  return Vec512<T>{_mm512_shuffle_i32x4(lo.raw, hi.raw, _MM_PERM_BABA)};
}
template <class D>
HWY_API Vec512<float> ConcatLowerLower(D /* tag */, Vec512<float> hi,
                                       Vec512<float> lo) {
  return Vec512<float>{_mm512_shuffle_f32x4(lo.raw, hi.raw, _MM_PERM_BABA)};
}
template <class D>
HWY_API Vec512<double> ConcatLowerLower(D /* tag */, Vec512<double> hi,
                                        Vec512<double> lo) {
  return Vec512<double>{_mm512_shuffle_f64x2(lo.raw, hi.raw, _MM_PERM_BABA)};
}

// hiH,hiL loH,loL |-> hiH,loH (= upper halves)
template <class D, typename T = TFromD<D>>
HWY_API Vec512<T> ConcatUpperUpper(D /* tag */, Vec512<T> hi, Vec512<T> lo) {
  return Vec512<T>{_mm512_shuffle_i32x4(lo.raw, hi.raw, _MM_PERM_DCDC)};
}
template <class D>
HWY_API Vec512<float> ConcatUpperUpper(D /* tag */, Vec512<float> hi,
                                       Vec512<float> lo) {
  return Vec512<float>{_mm512_shuffle_f32x4(lo.raw, hi.raw, _MM_PERM_DCDC)};
}
template <class D>
HWY_API Vec512<double> ConcatUpperUpper(D /* tag */, Vec512<double> hi,
                                        Vec512<double> lo) {
  return Vec512<double>{_mm512_shuffle_f64x2(lo.raw, hi.raw, _MM_PERM_DCDC)};
}

// hiH,hiL loH,loL |-> hiL,loH (= inner halves / swap blocks)
template <class D, typename T = TFromD<D>>
HWY_API Vec512<T> ConcatLowerUpper(D /* tag */, Vec512<T> hi, Vec512<T> lo) {
  return Vec512<T>{_mm512_shuffle_i32x4(lo.raw, hi.raw, _MM_PERM_BADC)};
}
template <class D>
HWY_API Vec512<float> ConcatLowerUpper(D /* tag */, Vec512<float> hi,
                                       Vec512<float> lo) {
  return Vec512<float>{_mm512_shuffle_f32x4(lo.raw, hi.raw, _MM_PERM_BADC)};
}
template <class D>
HWY_API Vec512<double> ConcatLowerUpper(D /* tag */, Vec512<double> hi,
                                        Vec512<double> lo) {
  return Vec512<double>{_mm512_shuffle_f64x2(lo.raw, hi.raw, _MM_PERM_BADC)};
}

// hiH,hiL loH,loL |-> hiH,loL (= outer halves)
template <class D, typename T = TFromD<D>>
HWY_API Vec512<T> ConcatUpperLower(D /* tag */, Vec512<T> hi, Vec512<T> lo) {
  // There are no imm8 blend in AVX512. Use blend16 because 32-bit masks
  // are efficiently loaded from 32-bit regs.
  const __mmask32 mask = /*_cvtu32_mask32 */ (0x0000FFFF);
  return Vec512<T>{_mm512_mask_blend_epi16(mask, hi.raw, lo.raw)};
}
template <class D>
HWY_API Vec512<float> ConcatUpperLower(D /* tag */, Vec512<float> hi,
                                       Vec512<float> lo) {
  const __mmask16 mask = /*_cvtu32_mask16 */ (0x00FF);
  return Vec512<float>{_mm512_mask_blend_ps(mask, hi.raw, lo.raw)};
}
template <class D>
HWY_API Vec512<double> ConcatUpperLower(D /* tag */, Vec512<double> hi,
                                        Vec512<double> lo) {
  const __mmask8 mask = /*_cvtu32_mask8 */ (0x0F);
  return Vec512<double>{_mm512_mask_blend_pd(mask, hi.raw, lo.raw)};
}

// ------------------------------ ConcatOdd

template <class D, typename T = TFromD<D>, HWY_IF_T_SIZE(T, 1)>
HWY_API Vec512<T> ConcatOdd(D d, Vec512<T> hi, Vec512<T> lo) {
  const RebindToUnsigned<decltype(d)> du;
#if HWY_TARGET <= HWY_AVX3_DL
  alignas(64) static constexpr uint8_t kIdx[64] = {
      1,   3,   5,   7,   9,   11,  13,  15,  17,  19,  21,  23,  25,
      27,  29,  31,  33,  35,  37,  39,  41,  43,  45,  47,  49,  51,
      53,  55,  57,  59,  61,  63,  65,  67,  69,  71,  73,  75,  77,
      79,  81,  83,  85,  87,  89,  91,  93,  95,  97,  99,  101, 103,
      105, 107, 109, 111, 113, 115, 117, 119, 121, 123, 125, 127};
  return BitCast(
      d, Vec512<uint8_t>{_mm512_permutex2var_epi8(
             BitCast(du, lo).raw, Load(du, kIdx).raw, BitCast(du, hi).raw)});
#else
  const RepartitionToWide<decltype(du)> dw;
  // Right-shift 8 bits per u16 so we can pack.
  const Vec512<uint16_t> uH = ShiftRight<8>(BitCast(dw, hi));
  const Vec512<uint16_t> uL = ShiftRight<8>(BitCast(dw, lo));
  const Vec512<uint64_t> u8{_mm512_packus_epi16(uL.raw, uH.raw)};
  // Undo block interleave: lower half = even u64 lanes, upper = odd u64 lanes.
  const Full512<uint64_t> du64;
  alignas(64) static constexpr uint64_t kIdx[8] = {0, 2, 4, 6, 1, 3, 5, 7};
  return BitCast(d, TableLookupLanes(u8, SetTableIndices(du64, kIdx)));
#endif
}

template <class D, typename T = TFromD<D>, HWY_IF_T_SIZE(T, 2)>
HWY_API Vec512<T> ConcatOdd(D d, Vec512<T> hi, Vec512<T> lo) {
  const RebindToUnsigned<decltype(d)> du;
  alignas(64) static constexpr uint16_t kIdx[32] = {
      1,  3,  5,  7,  9,  11, 13, 15, 17, 19, 21, 23, 25, 27, 29, 31,
      33, 35, 37, 39, 41, 43, 45, 47, 49, 51, 53, 55, 57, 59, 61, 63};
  return BitCast(
      d, Vec512<uint16_t>{_mm512_permutex2var_epi16(
             BitCast(du, lo).raw, Load(du, kIdx).raw, BitCast(du, hi).raw)});
}

template <class D, typename T = TFromD<D>, HWY_IF_T_SIZE(T, 4)>
HWY_API Vec512<T> ConcatOdd(D d, Vec512<T> hi, Vec512<T> lo) {
  const RebindToUnsigned<decltype(d)> du;
  alignas(64) static constexpr uint32_t kIdx[16] = {
      1, 3, 5, 7, 9, 11, 13, 15, 17, 19, 21, 23, 25, 27, 29, 31};
  return BitCast(
      d, Vec512<uint32_t>{_mm512_permutex2var_epi32(
             BitCast(du, lo).raw, Load(du, kIdx).raw, BitCast(du, hi).raw)});
}

template <class D>
HWY_API Vec512<float> ConcatOdd(D d, Vec512<float> hi, Vec512<float> lo) {
  const RebindToUnsigned<decltype(d)> du;
  alignas(64) static constexpr uint32_t kIdx[16] = {
      1, 3, 5, 7, 9, 11, 13, 15, 17, 19, 21, 23, 25, 27, 29, 31};
  return Vec512<float>{
      _mm512_permutex2var_ps(lo.raw, Load(du, kIdx).raw, hi.raw)};
}

template <class D, typename T = TFromD<D>, HWY_IF_T_SIZE(T, 8)>
HWY_API Vec512<T> ConcatOdd(D d, Vec512<T> hi, Vec512<T> lo) {
  const RebindToUnsigned<decltype(d)> du;
  alignas(64) static constexpr uint64_t kIdx[8] = {1, 3, 5, 7, 9, 11, 13, 15};
  return BitCast(
      d, Vec512<uint64_t>{_mm512_permutex2var_epi64(
             BitCast(du, lo).raw, Load(du, kIdx).raw, BitCast(du, hi).raw)});
}

template <class D>
HWY_API Vec512<double> ConcatOdd(D d, Vec512<double> hi, Vec512<double> lo) {
  const RebindToUnsigned<decltype(d)> du;
  alignas(64) static constexpr uint64_t kIdx[8] = {1, 3, 5, 7, 9, 11, 13, 15};
  return Vec512<double>{
      _mm512_permutex2var_pd(lo.raw, Load(du, kIdx).raw, hi.raw)};
}

// ------------------------------ ConcatEven

template <class D, typename T = TFromD<D>, HWY_IF_T_SIZE(T, 1)>
HWY_API Vec512<T> ConcatEven(D d, Vec512<T> hi, Vec512<T> lo) {
  const RebindToUnsigned<decltype(d)> du;
#if HWY_TARGET <= HWY_AVX3_DL
  alignas(64) static constexpr uint8_t kIdx[64] = {
      0,   2,   4,   6,   8,   10,  12,  14,  16,  18,  20,  22,  24,
      26,  28,  30,  32,  34,  36,  38,  40,  42,  44,  46,  48,  50,
      52,  54,  56,  58,  60,  62,  64,  66,  68,  70,  72,  74,  76,
      78,  80,  82,  84,  86,  88,  90,  92,  94,  96,  98,  100, 102,
      104, 106, 108, 110, 112, 114, 116, 118, 120, 122, 124, 126};
  return BitCast(
      d, Vec512<uint32_t>{_mm512_permutex2var_epi8(
             BitCast(du, lo).raw, Load(du, kIdx).raw, BitCast(du, hi).raw)});
#else
  const RepartitionToWide<decltype(du)> dw;
  // Isolate lower 8 bits per u16 so we can pack.
  const Vec512<uint16_t> mask = Set(dw, 0x00FF);
  const Vec512<uint16_t> uH = And(BitCast(dw, hi), mask);
  const Vec512<uint16_t> uL = And(BitCast(dw, lo), mask);
  const Vec512<uint64_t> u8{_mm512_packus_epi16(uL.raw, uH.raw)};
  // Undo block interleave: lower half = even u64 lanes, upper = odd u64 lanes.
  const Full512<uint64_t> du64;
  alignas(64) static constexpr uint64_t kIdx[8] = {0, 2, 4, 6, 1, 3, 5, 7};
  return BitCast(d, TableLookupLanes(u8, SetTableIndices(du64, kIdx)));
#endif
}

template <class D, typename T = TFromD<D>, HWY_IF_T_SIZE(T, 2)>
HWY_API Vec512<T> ConcatEven(D d, Vec512<T> hi, Vec512<T> lo) {
  const RebindToUnsigned<decltype(d)> du;
  alignas(64) static constexpr uint16_t kIdx[32] = {
      0,  2,  4,  6,  8,  10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30,
      32, 34, 36, 38, 40, 42, 44, 46, 48, 50, 52, 54, 56, 58, 60, 62};
  return BitCast(
      d, Vec512<uint32_t>{_mm512_permutex2var_epi16(
             BitCast(du, lo).raw, Load(du, kIdx).raw, BitCast(du, hi).raw)});
}

template <class D, typename T = TFromD<D>, HWY_IF_T_SIZE(T, 4)>
HWY_API Vec512<T> ConcatEven(D d, Vec512<T> hi, Vec512<T> lo) {
  const RebindToUnsigned<decltype(d)> du;
  alignas(64) static constexpr uint32_t kIdx[16] = {
      0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30};
  return BitCast(
      d, Vec512<uint32_t>{_mm512_permutex2var_epi32(
             BitCast(du, lo).raw, Load(du, kIdx).raw, BitCast(du, hi).raw)});
}

template <class D>
HWY_API Vec512<float> ConcatEven(D d, Vec512<float> hi, Vec512<float> lo) {
  const RebindToUnsigned<decltype(d)> du;
  alignas(64) static constexpr uint32_t kIdx[16] = {
      0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30};
  return Vec512<float>{
      _mm512_permutex2var_ps(lo.raw, Load(du, kIdx).raw, hi.raw)};
}

template <class D, typename T = TFromD<D>, HWY_IF_T_SIZE(T, 8)>
HWY_API Vec512<T> ConcatEven(D d, Vec512<T> hi, Vec512<T> lo) {
  const RebindToUnsigned<decltype(d)> du;
  alignas(64) static constexpr uint64_t kIdx[8] = {0, 2, 4, 6, 8, 10, 12, 14};
  return BitCast(
      d, Vec512<uint64_t>{_mm512_permutex2var_epi64(
             BitCast(du, lo).raw, Load(du, kIdx).raw, BitCast(du, hi).raw)});
}

template <class D>
HWY_API Vec512<double> ConcatEven(D d, Vec512<double> hi, Vec512<double> lo) {
  const RebindToUnsigned<decltype(d)> du;
  alignas(64) static constexpr uint64_t kIdx[8] = {0, 2, 4, 6, 8, 10, 12, 14};
  return Vec512<double>{
      _mm512_permutex2var_pd(lo.raw, Load(du, kIdx).raw, hi.raw)};
}

// ------------------------------ DupEven (InterleaveLower)

template <typename T, HWY_IF_T_SIZE(T, 4)>
HWY_API Vec512<T> DupEven(Vec512<T> v) {
  return Vec512<T>{_mm512_shuffle_epi32(v.raw, _MM_PERM_CCAA)};
}
HWY_API Vec512<float> DupEven(Vec512<float> v) {
  return Vec512<float>{_mm512_shuffle_ps(v.raw, v.raw, _MM_PERM_CCAA)};
}

template <typename T, HWY_IF_T_SIZE(T, 8)>
HWY_API Vec512<T> DupEven(const Vec512<T> v) {
  const DFromV<decltype(v)> d;
  return InterleaveLower(d, v, v);
}

// ------------------------------ DupOdd (InterleaveUpper)

template <typename T, HWY_IF_T_SIZE(T, 4)>
HWY_API Vec512<T> DupOdd(Vec512<T> v) {
  return Vec512<T>{_mm512_shuffle_epi32(v.raw, _MM_PERM_DDBB)};
}
HWY_API Vec512<float> DupOdd(Vec512<float> v) {
  return Vec512<float>{_mm512_shuffle_ps(v.raw, v.raw, _MM_PERM_DDBB)};
}

template <typename T, HWY_IF_T_SIZE(T, 8)>
HWY_API Vec512<T> DupOdd(const Vec512<T> v) {
  const DFromV<decltype(v)> d;
  return InterleaveUpper(d, v, v);
}

// ------------------------------ OddEven (IfThenElse)

template <typename T>
HWY_API Vec512<T> OddEven(const Vec512<T> a, const Vec512<T> b) {
  constexpr size_t s = sizeof(T);
  constexpr int shift = s == 1 ? 0 : s == 2 ? 32 : s == 4 ? 48 : 56;
  return IfThenElse(Mask512<T>{0x5555555555555555ull >> shift}, b, a);
}

// ------------------------------ OddEvenBlocks

template <typename T>
HWY_API Vec512<T> OddEvenBlocks(Vec512<T> odd, Vec512<T> even) {
  return Vec512<T>{_mm512_mask_blend_epi64(__mmask8{0x33u}, odd.raw, even.raw)};
}

HWY_API Vec512<float> OddEvenBlocks(Vec512<float> odd, Vec512<float> even) {
  return Vec512<float>{
      _mm512_mask_blend_ps(__mmask16{0x0F0Fu}, odd.raw, even.raw)};
}

HWY_API Vec512<double> OddEvenBlocks(Vec512<double> odd, Vec512<double> even) {
  return Vec512<double>{
      _mm512_mask_blend_pd(__mmask8{0x33u}, odd.raw, even.raw)};
}

// ------------------------------ SwapAdjacentBlocks

template <typename T>
HWY_API Vec512<T> SwapAdjacentBlocks(Vec512<T> v) {
  return Vec512<T>{_mm512_shuffle_i32x4(v.raw, v.raw, _MM_PERM_CDAB)};
}

HWY_API Vec512<float> SwapAdjacentBlocks(Vec512<float> v) {
  return Vec512<float>{_mm512_shuffle_f32x4(v.raw, v.raw, _MM_PERM_CDAB)};
}

HWY_API Vec512<double> SwapAdjacentBlocks(Vec512<double> v) {
  return Vec512<double>{_mm512_shuffle_f64x2(v.raw, v.raw, _MM_PERM_CDAB)};
}

// ------------------------------ ReverseBlocks

template <class D, typename T = TFromD<D>>
HWY_API Vec512<T> ReverseBlocks(D /* tag */, Vec512<T> v) {
  return Vec512<T>{_mm512_shuffle_i32x4(v.raw, v.raw, _MM_PERM_ABCD)};
}
template <class D>
HWY_API Vec512<float> ReverseBlocks(D /* tag */, Vec512<float> v) {
  return Vec512<float>{_mm512_shuffle_f32x4(v.raw, v.raw, _MM_PERM_ABCD)};
}
template <class D>
HWY_API Vec512<double> ReverseBlocks(D /* tag */, Vec512<double> v) {
  return Vec512<double>{_mm512_shuffle_f64x2(v.raw, v.raw, _MM_PERM_ABCD)};
}

// ------------------------------ TableLookupBytes (ZeroExtendVector)

// Both full
template <typename T, typename TI>
HWY_API Vec512<TI> TableLookupBytes(Vec512<T> bytes, Vec512<TI> indices) {
  return Vec512<TI>{_mm512_shuffle_epi8(bytes.raw, indices.raw)};
}

// Partial index vector
template <typename T, typename TI, size_t NI>
HWY_API Vec128<TI, NI> TableLookupBytes(Vec512<T> bytes, Vec128<TI, NI> from) {
  const Full512<TI> d512;
  const Half<decltype(d512)> d256;
  const Half<decltype(d256)> d128;
  // First expand to full 128, then 256, then 512.
  const Vec128<TI> from_full{from.raw};
  const auto from_512 =
      ZeroExtendVector(d512, ZeroExtendVector(d256, from_full));
  const auto tbl_full = TableLookupBytes(bytes, from_512);
  // Shrink to 256, then 128, then partial.
  return Vec128<TI, NI>{LowerHalf(d128, LowerHalf(d256, tbl_full)).raw};
}
template <typename T, typename TI>
HWY_API Vec256<TI> TableLookupBytes(Vec512<T> bytes, Vec256<TI> from) {
  const DFromV<decltype(from)> dih;
  const Twice<decltype(dih)> di;
  const auto from_512 = ZeroExtendVector(di, from);
  return LowerHalf(dih, TableLookupBytes(bytes, from_512));
}

// Partial table vector
template <typename T, size_t N, typename TI>
HWY_API Vec512<TI> TableLookupBytes(Vec128<T, N> bytes, Vec512<TI> from) {
  const DFromV<decltype(from)> d512;
  const Half<decltype(d512)> d256;
  const Half<decltype(d256)> d128;
  // First expand to full 128, then 256, then 512.
  const Vec128<T> bytes_full{bytes.raw};
  const auto bytes_512 =
      ZeroExtendVector(d512, ZeroExtendVector(d256, bytes_full));
  return TableLookupBytes(bytes_512, from);
}
template <typename T, typename TI>
HWY_API Vec512<TI> TableLookupBytes(Vec256<T> bytes, Vec512<TI> from) {
  const Full512<T> d;
  return TableLookupBytes(ZeroExtendVector(d, bytes), from);
}

// Partial both are handled by x86_128/256.

// ================================================== CONVERT

// ------------------------------ Promotions (part w/ narrow lanes -> full)

// Unsigned: zero-extend.
// Note: these have 3 cycle latency; if inputs are already split across the
// 128 bit blocks (in their upper/lower halves), then Zip* would be faster.
template <class D, HWY_IF_U16_D(D)>
HWY_API Vec512<uint16_t> PromoteTo(D /* tag */, Vec256<uint8_t> v) {
  return Vec512<uint16_t>{_mm512_cvtepu8_epi16(v.raw)};
}
template <class D, HWY_IF_U32_D(D)>
HWY_API Vec512<uint32_t> PromoteTo(D /* tag */, Vec128<uint8_t> v) {
  return Vec512<uint32_t>{_mm512_cvtepu8_epi32(v.raw)};
}
template <class D, HWY_IF_U32_D(D)>
HWY_API Vec512<uint32_t> PromoteTo(D /* tag */, Vec256<uint16_t> v) {
  return Vec512<uint32_t>{_mm512_cvtepu16_epi32(v.raw)};
}
template <class D, HWY_IF_U64_D(D)>
HWY_API Vec512<uint64_t> PromoteTo(D /* tag */, Vec256<uint32_t> v) {
  return Vec512<uint64_t>{_mm512_cvtepu32_epi64(v.raw)};
}
template <class D, HWY_IF_U64_D(D)>
HWY_API Vec512<uint64_t> PromoteTo(D /* tag */, Vec128<uint16_t> v) {
  return Vec512<uint64_t>{_mm512_cvtepu16_epi64(v.raw)};
}
template <class D, HWY_IF_U64_D(D)>
HWY_API Vec512<uint64_t> PromoteTo(D /* tag */, Vec64<uint8_t> v) {
  return Vec512<uint64_t>{_mm512_cvtepu8_epi64(v.raw)};
}

// Signed: replicate sign bit.
// Note: these have 3 cycle latency; if inputs are already split across the
// 128 bit blocks (in their upper/lower halves), then ZipUpper/lo followed by
// signed shift would be faster.
template <class D, HWY_IF_I16_D(D)>
HWY_API Vec512<int16_t> PromoteTo(D /* tag */, Vec256<int8_t> v) {
  return Vec512<int16_t>{_mm512_cvtepi8_epi16(v.raw)};
}
template <class D, HWY_IF_I32_D(D)>
HWY_API Vec512<int32_t> PromoteTo(D /* tag */, Vec128<int8_t> v) {
  return Vec512<int32_t>{_mm512_cvtepi8_epi32(v.raw)};
}
template <class D, HWY_IF_I32_D(D)>
HWY_API Vec512<int32_t> PromoteTo(D /* tag */, Vec256<int16_t> v) {
  return Vec512<int32_t>{_mm512_cvtepi16_epi32(v.raw)};
}
template <class D, HWY_IF_I64_D(D)>
HWY_API Vec512<int64_t> PromoteTo(D /* tag */, Vec256<int32_t> v) {
  return Vec512<int64_t>{_mm512_cvtepi32_epi64(v.raw)};
}
template <class D, HWY_IF_I64_D(D)>
HWY_API Vec512<int64_t> PromoteTo(D /* tag */, Vec128<int16_t> v) {
  return Vec512<int64_t>{_mm512_cvtepi16_epi64(v.raw)};
}
template <class D, HWY_IF_I64_D(D)>
HWY_API Vec512<int64_t> PromoteTo(D /* tag */, Vec64<int8_t> v) {
  return Vec512<int64_t>{_mm512_cvtepi8_epi64(v.raw)};
}

// Float
template <class D, HWY_IF_F32_D(D)>
HWY_API Vec512<float> PromoteTo(D /* tag */, Vec256<float16_t> v) {
  return Vec512<float>{_mm512_cvtph_ps(v.raw)};
}

template <class D, HWY_IF_F32_D(D)>
HWY_API Vec512<float> PromoteTo(D df32, Vec256<bfloat16_t> v) {
  const Rebind<uint16_t, decltype(df32)> du16;
  const RebindToSigned<decltype(df32)> di32;
  return BitCast(df32, ShiftLeft<16>(PromoteTo(di32, BitCast(du16, v))));
}

template <class D, HWY_IF_F64_D(D)>
HWY_API Vec512<double> PromoteTo(D /* tag */, Vec256<float> v) {
  return Vec512<double>{_mm512_cvtps_pd(v.raw)};
}

template <class D, HWY_IF_F64_D(D)>
HWY_API Vec512<double> PromoteTo(D /* tag */, Vec256<int32_t> v) {
  return Vec512<double>{_mm512_cvtepi32_pd(v.raw)};
}

// ------------------------------ Demotions (full -> part w/ narrow lanes)

template <class D, HWY_IF_U16_D(D)>
HWY_API Vec256<uint16_t> DemoteTo(D /* tag */, Vec512<int32_t> v) {
  const Full512<uint64_t> du64;
  const Vec512<uint16_t> u16{_mm512_packus_epi32(v.raw, v.raw)};

  // Compress even u64 lanes into 256 bit.
  alignas(64) static constexpr uint64_t kLanes[8] = {0, 2, 4, 6, 0, 2, 4, 6};
  const auto idx64 = Load(du64, kLanes);
  const Vec512<uint16_t> even{_mm512_permutexvar_epi64(idx64.raw, u16.raw)};
  return LowerHalf(even);
}

template <class D, HWY_IF_U16_D(D)>
HWY_API Vec256<uint16_t> DemoteTo(D dn, Vec512<uint32_t> v) {
  const DFromV<decltype(v)> d;
  const RebindToSigned<decltype(d)> di;
  return DemoteTo(dn, BitCast(di, Min(v, Set(d, 0x7FFFFFFFu))));
}

template <class D, HWY_IF_I16_D(D)>
HWY_API Vec256<int16_t> DemoteTo(D /* tag */, Vec512<int32_t> v) {
  const Full512<uint64_t> du64;
  const Vec512<int16_t> i16{_mm512_packs_epi32(v.raw, v.raw)};

  // Compress even u64 lanes into 256 bit.
  alignas(64) static constexpr uint64_t kLanes[8] = {0, 2, 4, 6, 0, 2, 4, 6};
  const auto idx64 = Load(du64, kLanes);
  const Vec512<int16_t> even{_mm512_permutexvar_epi64(idx64.raw, i16.raw)};
  return LowerHalf(even);
}

template <class D, HWY_IF_U8_D(D)>
HWY_API Vec128<uint8_t> DemoteTo(D /* tag */, Vec512<int32_t> v) {
  const Full512<uint32_t> du32;
  const Vec512<int16_t> i16{_mm512_packs_epi32(v.raw, v.raw)};
  const Vec512<uint8_t> u8{_mm512_packus_epi16(i16.raw, i16.raw)};

  alignas(16) static constexpr uint32_t kLanes[4] = {0, 4, 8, 12};
  const auto idx32 = LoadDup128(du32, kLanes);
  const Vec512<uint8_t> fixed{_mm512_permutexvar_epi32(idx32.raw, u8.raw)};
  return LowerHalf(LowerHalf(fixed));
}

template <class D, HWY_IF_U8_D(D)>
HWY_API Vec128<uint8_t> DemoteTo(D /* tag */, Vec512<uint32_t> v) {
  return Vec128<uint8_t>{_mm512_cvtusepi32_epi8(v.raw)};
}

template <class D, HWY_IF_U8_D(D)>
HWY_API Vec256<uint8_t> DemoteTo(D /* tag */, Vec512<int16_t> v) {
  const Full512<uint64_t> du64;
  const Vec512<uint8_t> u8{_mm512_packus_epi16(v.raw, v.raw)};

  // Compress even u64 lanes into 256 bit.
  alignas(64) static constexpr uint64_t kLanes[8] = {0, 2, 4, 6, 0, 2, 4, 6};
  const auto idx64 = Load(du64, kLanes);
  const Vec512<uint8_t> even{_mm512_permutexvar_epi64(idx64.raw, u8.raw)};
  return LowerHalf(even);
}

template <class D, HWY_IF_U8_D(D)>
HWY_API Vec256<uint8_t> DemoteTo(D dn, Vec512<uint16_t> v) {
  const DFromV<decltype(v)> d;
  const RebindToSigned<decltype(d)> di;
  return DemoteTo(dn, BitCast(di, Min(v, Set(d, 0x7FFFu))));
}

template <class D, HWY_IF_I8_D(D)>
HWY_API Vec128<int8_t> DemoteTo(D /* tag */, Vec512<int32_t> v) {
  const Full512<uint32_t> du32;
  const Vec512<int16_t> i16{_mm512_packs_epi32(v.raw, v.raw)};
  const Vec512<int8_t> i8{_mm512_packs_epi16(i16.raw, i16.raw)};

  alignas(16) static constexpr uint32_t kLanes[16] = {0, 4, 8, 12, 0, 4, 8, 12,
                                                      0, 4, 8, 12, 0, 4, 8, 12};
  const auto idx32 = LoadDup128(du32, kLanes);
  const Vec512<int8_t> fixed{_mm512_permutexvar_epi32(idx32.raw, i8.raw)};
  return LowerHalf(LowerHalf(fixed));
}

template <class D, HWY_IF_I8_D(D)>
HWY_API Vec256<int8_t> DemoteTo(D /* tag */, Vec512<int16_t> v) {
  const Full512<uint64_t> du64;
  const Vec512<int8_t> u8{_mm512_packs_epi16(v.raw, v.raw)};

  // Compress even u64 lanes into 256 bit.
  alignas(64) static constexpr uint64_t kLanes[8] = {0, 2, 4, 6, 0, 2, 4, 6};
  const auto idx64 = Load(du64, kLanes);
  const Vec512<int8_t> even{_mm512_permutexvar_epi64(idx64.raw, u8.raw)};
  return LowerHalf(even);
}

template <class D, HWY_IF_I32_D(D)>
HWY_API Vec256<int32_t> DemoteTo(D /* tag */, Vec512<int64_t> v) {
  return Vec256<int32_t>{_mm512_cvtsepi64_epi32(v.raw)};
}
template <class D, HWY_IF_I16_D(D)>
HWY_API Vec128<int16_t> DemoteTo(D /* tag */, Vec512<int64_t> v) {
  return Vec128<int16_t>{_mm512_cvtsepi64_epi16(v.raw)};
}
template <class D, HWY_IF_I8_D(D)>
HWY_API Vec64<int8_t> DemoteTo(D /* tag */, Vec512<int64_t> v) {
  return Vec64<int8_t>{_mm512_cvtsepi64_epi8(v.raw)};
}

template <class D, HWY_IF_U32_D(D)>
HWY_API Vec256<uint32_t> DemoteTo(D /* tag */, Vec512<int64_t> v) {
  const auto neg_mask = MaskFromVec(v);
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  const __mmask8 non_neg_mask = _knot_mask8(neg_mask.raw);
#else
  const __mmask8 non_neg_mask = static_cast<__mmask8>(~neg_mask.raw);
#endif
  return Vec256<uint32_t>{_mm512_maskz_cvtusepi64_epi32(non_neg_mask, v.raw)};
}
template <class D, HWY_IF_U16_D(D)>
HWY_API Vec128<uint16_t> DemoteTo(D /* tag */, Vec512<int64_t> v) {
  const auto neg_mask = MaskFromVec(v);
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  const __mmask8 non_neg_mask = _knot_mask8(neg_mask.raw);
#else
  const __mmask8 non_neg_mask = static_cast<__mmask8>(~neg_mask.raw);
#endif
  return Vec128<uint16_t>{_mm512_maskz_cvtusepi64_epi16(non_neg_mask, v.raw)};
}
template <class D, HWY_IF_U8_D(D)>
HWY_API Vec64<uint8_t> DemoteTo(D /* tag */, Vec512<int64_t> v) {
  const auto neg_mask = MaskFromVec(v);
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  const __mmask8 non_neg_mask = _knot_mask8(neg_mask.raw);
#else
  const __mmask8 non_neg_mask = static_cast<__mmask8>(~neg_mask.raw);
#endif
  return Vec64<uint8_t>{_mm512_maskz_cvtusepi64_epi8(non_neg_mask, v.raw)};
}

template <class D, HWY_IF_U32_D(D)>
HWY_API Vec256<uint32_t> DemoteTo(D /* tag */, Vec512<uint64_t> v) {
  return Vec256<uint32_t>{_mm512_cvtusepi64_epi32(v.raw)};
}
template <class D, HWY_IF_U16_D(D)>
HWY_API Vec128<uint16_t> DemoteTo(D /* tag */, Vec512<uint64_t> v) {
  return Vec128<uint16_t>{_mm512_cvtusepi64_epi16(v.raw)};
}
template <class D, HWY_IF_U8_D(D)>
HWY_API Vec64<uint8_t> DemoteTo(D /* tag */, Vec512<uint64_t> v) {
  return Vec64<uint8_t>{_mm512_cvtusepi64_epi8(v.raw)};
}

template <class D, HWY_IF_F16_D(D)>
HWY_API Vec256<float16_t> DemoteTo(D /* tag */, Vec512<float> v) {
  // Work around warnings in the intrinsic definitions (passing -1 as a mask).
  HWY_DIAGNOSTICS(push)
  HWY_DIAGNOSTICS_OFF(disable : 4245 4365, ignored "-Wsign-conversion")
  return Vec256<float16_t>{_mm512_cvtps_ph(v.raw, _MM_FROUND_NO_EXC)};
  HWY_DIAGNOSTICS(pop)
}

template <class D, HWY_IF_BF16_D(D)>
HWY_API Vec256<bfloat16_t> DemoteTo(D dbf16, Vec512<float> v) {
  // TODO(janwas): _mm512_cvtneps_pbh once we have avx512bf16.
  const Rebind<int32_t, decltype(dbf16)> di32;
  const Rebind<uint32_t, decltype(dbf16)> du32;  // for logical shift right
  const Rebind<uint16_t, decltype(dbf16)> du16;
  const auto bits_in_32 = BitCast(di32, ShiftRight<16>(BitCast(du32, v)));
  return BitCast(dbf16, DemoteTo(du16, bits_in_32));
}

template <class D, HWY_IF_BF16_D(D)>
HWY_API Vec512<bfloat16_t> ReorderDemote2To(D dbf16, Vec512<float> a,
                                            Vec512<float> b) {
  // TODO(janwas): _mm512_cvtne2ps_pbh once we have avx512bf16.
  const RebindToUnsigned<decltype(dbf16)> du16;
  const Repartition<uint32_t, decltype(dbf16)> du32;
  const Vec512<uint32_t> b_in_even = ShiftRight<16>(BitCast(du32, b));
  return BitCast(dbf16, OddEven(BitCast(du16, a), BitCast(du16, b_in_even)));
}

template <class D, HWY_IF_I16_D(D)>
HWY_API Vec512<int16_t> ReorderDemote2To(D /* tag */, Vec512<int32_t> a,
                                         Vec512<int32_t> b) {
  return Vec512<int16_t>{_mm512_packs_epi32(a.raw, b.raw)};
}

template <class D, HWY_IF_U16_D(D)>
HWY_API Vec512<uint16_t> ReorderDemote2To(D /* tag */, Vec512<int32_t> a,
                                          Vec512<int32_t> b) {
  return Vec512<uint16_t>{_mm512_packus_epi32(a.raw, b.raw)};
}

template <class D, HWY_IF_U16_D(D)>
HWY_API Vec512<uint16_t> ReorderDemote2To(D dn, Vec512<uint32_t> a,
                                          Vec512<uint32_t> b) {
  const DFromV<decltype(a)> du32;
  const RebindToSigned<decltype(du32)> di32;
  const auto max_i32 = Set(du32, 0x7FFFFFFFu);

  return ReorderDemote2To(dn, BitCast(di32, Min(a, max_i32)),
                          BitCast(di32, Min(b, max_i32)));
}

template <class D, HWY_IF_I8_D(D)>
HWY_API Vec512<int8_t> ReorderDemote2To(D /* tag */, Vec512<int16_t> a,
                                        Vec512<int16_t> b) {
  return Vec512<int8_t>{_mm512_packs_epi16(a.raw, b.raw)};
}

template <class D, HWY_IF_U8_D(D)>
HWY_API Vec512<uint8_t> ReorderDemote2To(D /* tag */, Vec512<int16_t> a,
                                         Vec512<int16_t> b) {
  return Vec512<uint8_t>{_mm512_packus_epi16(a.raw, b.raw)};
}

template <class D, HWY_IF_U8_D(D)>
HWY_API Vec512<uint8_t> ReorderDemote2To(D dn, Vec512<uint16_t> a,
                                         Vec512<uint16_t> b) {
  const DFromV<decltype(a)> du16;
  const RebindToSigned<decltype(du16)> di16;
  const auto max_i16 = Set(du16, 0x7FFFu);

  return ReorderDemote2To(dn, BitCast(di16, Min(a, max_i16)),
                          BitCast(di16, Min(b, max_i16)));
}

template <class D, HWY_IF_T_SIZE_D(D, 4),
          HWY_IF_NOT_FLOAT_NOR_SPECIAL(TFromD<D>)>
HWY_API VFromD<D> ReorderDemote2To(D dn, Vec512<int64_t> a, Vec512<int64_t> b) {
  const Half<decltype(dn)> dnh;
  return Combine(dn, DemoteTo(dnh, b), DemoteTo(dnh, a));
}

template <class D, HWY_IF_U32_D(D)>
HWY_API Vec512<uint32_t> ReorderDemote2To(D dn, Vec512<uint64_t> a,
                                          Vec512<uint64_t> b) {
  const Half<decltype(dn)> dnh;
  return Combine(dn, DemoteTo(dnh, b), DemoteTo(dnh, a));
}

template <class D, class V, HWY_IF_NOT_FLOAT_NOR_SPECIAL(TFromD<D>),
          HWY_IF_V_SIZE_D(D, 64), HWY_IF_NOT_FLOAT_NOR_SPECIAL_V(V),
          HWY_IF_T_SIZE_V(V, sizeof(TFromD<D>) * 2),
          HWY_IF_LANES_D(D, HWY_MAX_LANES_D(DFromV<V>) * 2),
          HWY_IF_T_SIZE_ONE_OF_V(V, (1 << 1) | (1 << 2) | (1 << 4))>
HWY_API VFromD<D> OrderedDemote2To(D d, V a, V b) {
  const Full512<uint64_t> du64;
  alignas(64) static constexpr uint64_t kIdx[8] = {0, 2, 4, 6, 1, 3, 5, 7};
  return BitCast(d, TableLookupLanes(BitCast(du64, ReorderDemote2To(d, a, b)),
                                     SetTableIndices(du64, kIdx)));
}

template <class D, HWY_IF_NOT_FLOAT_NOR_SPECIAL(TFromD<D>),
          HWY_IF_V_SIZE_GT_D(D, 16), class V, HWY_IF_NOT_FLOAT_NOR_SPECIAL_V(V),
          HWY_IF_T_SIZE_V(V, sizeof(TFromD<D>) * 2),
          HWY_IF_LANES_D(D, HWY_MAX_LANES_D(DFromV<V>) * 2),
          HWY_IF_T_SIZE_V(V, 8)>
HWY_API VFromD<D> OrderedDemote2To(D d, V a, V b) {
  return ReorderDemote2To(d, a, b);
}

template <class D, HWY_IF_F32_D(D)>
HWY_API Vec256<float> DemoteTo(D /* tag */, Vec512<double> v) {
  return Vec256<float>{_mm512_cvtpd_ps(v.raw)};
}

template <class D, HWY_IF_I32_D(D)>
HWY_API Vec256<int32_t> DemoteTo(D /* tag */, Vec512<double> v) {
  const DFromV<decltype(v)> d;
  const Vec512<double> clamped = detail::ClampF64ToI32Max(d, v);
  return Vec256<int32_t>{_mm512_cvttpd_epi32(clamped.raw)};
}

// For already range-limited input [0, 255].
HWY_API Vec128<uint8_t> U8FromU32(const Vec512<uint32_t> v) {
  const DFromV<decltype(v)> d32;
  // In each 128 bit block, gather the lower byte of 4 uint32_t lanes into the
  // lowest 4 bytes.
  alignas(16) static constexpr uint32_t k8From32[4] = {0x0C080400u, ~0u, ~0u,
                                                       ~0u};
  const auto quads = TableLookupBytes(v, LoadDup128(d32, k8From32));
  // Gather the lowest 4 bytes of 4 128-bit blocks.
  alignas(16) static constexpr uint32_t kIndex32[4] = {0, 4, 8, 12};
  const Vec512<uint8_t> bytes{
      _mm512_permutexvar_epi32(LoadDup128(d32, kIndex32).raw, quads.raw)};
  return LowerHalf(LowerHalf(bytes));
}

// ------------------------------ Truncations

template <class D, HWY_IF_U8_D(D)>
HWY_API Vec64<uint8_t> TruncateTo(D d, const Vec512<uint64_t> v) {
#if HWY_TARGET <= HWY_AVX3_DL
  (void)d;
  const Full512<uint8_t> d8;
  alignas(16) static constexpr uint8_t k8From64[16] = {
      0, 8, 16, 24, 32, 40, 48, 56, 0, 8, 16, 24, 32, 40, 48, 56};
  const Vec512<uint8_t> bytes{
      _mm512_permutexvar_epi8(LoadDup128(d8, k8From64).raw, v.raw)};
  return LowerHalf(LowerHalf(LowerHalf(bytes)));
#else
  const Full512<uint32_t> d32;
  alignas(64) static constexpr uint32_t kEven[16] = {0, 2, 4, 6, 8, 10, 12, 14,
                                                     0, 2, 4, 6, 8, 10, 12, 14};
  const Vec512<uint32_t> even{
      _mm512_permutexvar_epi32(Load(d32, kEven).raw, v.raw)};
  return TruncateTo(d, LowerHalf(even));
#endif
}

template <class D, HWY_IF_U16_D(D)>
HWY_API Vec128<uint16_t> TruncateTo(D /* tag */, const Vec512<uint64_t> v) {
  const Full512<uint16_t> d16;
  alignas(16) static constexpr uint16_t k16From64[8] = {0,  4,  8,  12,
                                                        16, 20, 24, 28};
  const Vec512<uint16_t> bytes{
      _mm512_permutexvar_epi16(LoadDup128(d16, k16From64).raw, v.raw)};
  return LowerHalf(LowerHalf(bytes));
}

template <class D, HWY_IF_U32_D(D)>
HWY_API Vec256<uint32_t> TruncateTo(D /* tag */, const Vec512<uint64_t> v) {
  const Full512<uint32_t> d32;
  alignas(64) static constexpr uint32_t kEven[16] = {0, 2, 4, 6, 8, 10, 12, 14,
                                                     0, 2, 4, 6, 8, 10, 12, 14};
  const Vec512<uint32_t> even{
      _mm512_permutexvar_epi32(Load(d32, kEven).raw, v.raw)};
  return LowerHalf(even);
}

template <class D, HWY_IF_U8_D(D)>
HWY_API Vec128<uint8_t> TruncateTo(D /* tag */, const Vec512<uint32_t> v) {
#if HWY_TARGET <= HWY_AVX3_DL
  const Full512<uint8_t> d8;
  alignas(16) static constexpr uint8_t k8From32[16] = {
      0, 4, 8, 12, 16, 20, 24, 28, 32, 36, 40, 44, 48, 52, 56, 60};
  const Vec512<uint8_t> bytes{
      _mm512_permutexvar_epi8(LoadDup128(d8, k8From32).raw, v.raw)};
#else
  const Full512<uint32_t> d32;
  // In each 128 bit block, gather the lower byte of 4 uint32_t lanes into the
  // lowest 4 bytes.
  alignas(16) static constexpr uint32_t k8From32[4] = {0x0C080400u, ~0u, ~0u,
                                                       ~0u};
  const auto quads = TableLookupBytes(v, LoadDup128(d32, k8From32));
  // Gather the lowest 4 bytes of 4 128-bit blocks.
  alignas(16) static constexpr uint32_t kIndex32[4] = {0, 4, 8, 12};
  const Vec512<uint8_t> bytes{
      _mm512_permutexvar_epi32(LoadDup128(d32, kIndex32).raw, quads.raw)};
#endif
  return LowerHalf(LowerHalf(bytes));
}

template <class D, HWY_IF_U16_D(D)>
HWY_API Vec256<uint16_t> TruncateTo(D /* tag */, const Vec512<uint32_t> v) {
  const Full512<uint16_t> d16;
  alignas(64) static constexpr uint16_t k16From32[32] = {
      0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30,
      0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30};
  const Vec512<uint16_t> bytes{
      _mm512_permutexvar_epi16(Load(d16, k16From32).raw, v.raw)};
  return LowerHalf(bytes);
}

template <class D, HWY_IF_U8_D(D)>
HWY_API Vec256<uint8_t> TruncateTo(D /* tag */, const Vec512<uint16_t> v) {
#if HWY_TARGET <= HWY_AVX3_DL
  const Full512<uint8_t> d8;
  alignas(64) static constexpr uint8_t k8From16[64] = {
      0,  2,  4,  6,  8,  10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30,
      32, 34, 36, 38, 40, 42, 44, 46, 48, 50, 52, 54, 56, 58, 60, 62,
      0,  2,  4,  6,  8,  10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30,
      32, 34, 36, 38, 40, 42, 44, 46, 48, 50, 52, 54, 56, 58, 60, 62};
  const Vec512<uint8_t> bytes{
      _mm512_permutexvar_epi8(Load(d8, k8From16).raw, v.raw)};
#else
  const Full512<uint32_t> d32;
  alignas(16) static constexpr uint32_t k16From32[4] = {
      0x06040200u, 0x0E0C0A08u, 0x06040200u, 0x0E0C0A08u};
  const auto quads = TableLookupBytes(v, LoadDup128(d32, k16From32));
  alignas(64) static constexpr uint32_t kIndex32[16] = {
      0, 1, 4, 5, 8, 9, 12, 13, 0, 1, 4, 5, 8, 9, 12, 13};
  const Vec512<uint8_t> bytes{
      _mm512_permutexvar_epi32(Load(d32, kIndex32).raw, quads.raw)};
#endif
  return LowerHalf(bytes);
}

// ------------------------------ Convert integer <=> floating point

template <class D, HWY_IF_F32_D(D)>
HWY_API Vec512<float> ConvertTo(D /* tag */, Vec512<int32_t> v) {
  return Vec512<float>{_mm512_cvtepi32_ps(v.raw)};
}

template <class D, HWY_IF_F64_D(D)>
HWY_API Vec512<double> ConvertTo(D /* tag */, Vec512<int64_t> v) {
  return Vec512<double>{_mm512_cvtepi64_pd(v.raw)};
}

template <class D, HWY_IF_F32_D(D)>
HWY_API Vec512<float> ConvertTo(D /* tag*/, Vec512<uint32_t> v) {
  return Vec512<float>{_mm512_cvtepu32_ps(v.raw)};
}

template <class D, HWY_IF_F64_D(D)>
HWY_API Vec512<double> ConvertTo(D /* tag*/, Vec512<uint64_t> v) {
  return Vec512<double>{_mm512_cvtepu64_pd(v.raw)};
}

// Truncates (rounds toward zero).
template <class D, HWY_IF_I32_D(D)>
HWY_API Vec512<int32_t> ConvertTo(D d, Vec512<float> v) {
  return detail::FixConversionOverflow(d, v, _mm512_cvttps_epi32(v.raw));
}
template <class D, HWY_IF_I64_D(D)>
HWY_API Vec512<int64_t> ConvertTo(D di, Vec512<double> v) {
  return detail::FixConversionOverflow(di, v, _mm512_cvttpd_epi64(v.raw));
}

HWY_API Vec512<int32_t> NearestInt(const Vec512<float> v) {
  const RebindToSigned<DFromV<decltype(v)>> di;
  return detail::FixConversionOverflow(di, v, _mm512_cvtps_epi32(v.raw));
}

// ================================================== CRYPTO

#if !defined(HWY_DISABLE_PCLMUL_AES)

HWY_API Vec512<uint8_t> AESRound(Vec512<uint8_t> state,
                                 Vec512<uint8_t> round_key) {
#if HWY_TARGET <= HWY_AVX3_DL
  return Vec512<uint8_t>{_mm512_aesenc_epi128(state.raw, round_key.raw)};
#else
  const DFromV<decltype(state)> d;
  const Half<decltype(d)> d2;
  return Combine(d, AESRound(UpperHalf(d2, state), UpperHalf(d2, round_key)),
                 AESRound(LowerHalf(state), LowerHalf(round_key)));
#endif
}

HWY_API Vec512<uint8_t> AESLastRound(Vec512<uint8_t> state,
                                     Vec512<uint8_t> round_key) {
#if HWY_TARGET <= HWY_AVX3_DL
  return Vec512<uint8_t>{_mm512_aesenclast_epi128(state.raw, round_key.raw)};
#else
  const DFromV<decltype(state)> d;
  const Half<decltype(d)> d2;
  return Combine(d,
                 AESLastRound(UpperHalf(d2, state), UpperHalf(d2, round_key)),
                 AESLastRound(LowerHalf(state), LowerHalf(round_key)));
#endif
}

HWY_API Vec512<uint8_t> AESRoundInv(Vec512<uint8_t> state,
                                    Vec512<uint8_t> round_key) {
#if HWY_TARGET <= HWY_AVX3_DL
  return Vec512<uint8_t>{_mm512_aesdec_epi128(state.raw, round_key.raw)};
#else
  const Full512<uint8_t> d;
  const Half<decltype(d)> d2;
  return Combine(d, AESRoundInv(UpperHalf(d2, state), UpperHalf(d2, round_key)),
                 AESRoundInv(LowerHalf(state), LowerHalf(round_key)));
#endif
}

HWY_API Vec512<uint8_t> AESLastRoundInv(Vec512<uint8_t> state,
                                        Vec512<uint8_t> round_key) {
#if HWY_TARGET <= HWY_AVX3_DL
  return Vec512<uint8_t>{_mm512_aesdeclast_epi128(state.raw, round_key.raw)};
#else
  const Full512<uint8_t> d;
  const Half<decltype(d)> d2;
  return Combine(
      d, AESLastRoundInv(UpperHalf(d2, state), UpperHalf(d2, round_key)),
      AESLastRoundInv(LowerHalf(state), LowerHalf(round_key)));
#endif
}

template <uint8_t kRcon>
HWY_API Vec512<uint8_t> AESKeyGenAssist(Vec512<uint8_t> v) {
  const Full512<uint8_t> d;
#if HWY_TARGET <= HWY_AVX3_DL
  alignas(16) static constexpr uint8_t kRconXorMask[16] = {
      0, kRcon, 0, 0, 0, 0, 0, 0, 0, kRcon, 0, 0, 0, 0, 0, 0};
  alignas(16) static constexpr uint8_t kRotWordShuffle[16] = {
      0, 13, 10, 7, 1, 14, 11, 4, 8, 5, 2, 15, 9, 6, 3, 12};
  const Repartition<uint32_t, decltype(d)> du32;
  const auto w13 = BitCast(d, DupOdd(BitCast(du32, v)));
  const auto sub_word_result = AESLastRound(w13, Load(d, kRconXorMask));
  return TableLookupBytes(sub_word_result, Load(d, kRotWordShuffle));
#else
  const Half<decltype(d)> d2;
  return Combine(d, AESKeyGenAssist<kRcon>(UpperHalf(d2, v)),
                 AESKeyGenAssist<kRcon>(LowerHalf(v)));
#endif
}

HWY_API Vec512<uint64_t> CLMulLower(Vec512<uint64_t> va, Vec512<uint64_t> vb) {
#if HWY_TARGET <= HWY_AVX3_DL
  return Vec512<uint64_t>{_mm512_clmulepi64_epi128(va.raw, vb.raw, 0x00)};
#else
  alignas(64) uint64_t a[8];
  alignas(64) uint64_t b[8];
  const DFromV<decltype(va)> d;
  const Half<Half<decltype(d)>> d128;
  Store(va, d, a);
  Store(vb, d, b);
  for (size_t i = 0; i < 8; i += 2) {
    const auto mul = CLMulLower(Load(d128, a + i), Load(d128, b + i));
    Store(mul, d128, a + i);
  }
  return Load(d, a);
#endif
}

HWY_API Vec512<uint64_t> CLMulUpper(Vec512<uint64_t> va, Vec512<uint64_t> vb) {
#if HWY_TARGET <= HWY_AVX3_DL
  return Vec512<uint64_t>{_mm512_clmulepi64_epi128(va.raw, vb.raw, 0x11)};
#else
  alignas(64) uint64_t a[8];
  alignas(64) uint64_t b[8];
  const DFromV<decltype(va)> d;
  const Half<Half<decltype(d)>> d128;
  Store(va, d, a);
  Store(vb, d, b);
  for (size_t i = 0; i < 8; i += 2) {
    const auto mul = CLMulUpper(Load(d128, a + i), Load(d128, b + i));
    Store(mul, d128, a + i);
  }
  return Load(d, a);
#endif
}

#endif  // HWY_DISABLE_PCLMUL_AES

// ================================================== MISC

// ------------------------------ I32/I64 SaturatedAdd (MaskFromVec)

HWY_API Vec512<int32_t> SaturatedAdd(Vec512<int32_t> a, Vec512<int32_t> b) {
  const DFromV<decltype(a)> d;
  const auto sum = a + b;
  const auto overflow_mask = MaskFromVec(
      Vec512<int32_t>{_mm512_ternarylogic_epi32(a.raw, b.raw, sum.raw, 0x42)});
  const auto i32_max = Set(d, LimitsMax<int32_t>());
  const Vec512<int32_t> overflow_result{_mm512_mask_ternarylogic_epi32(
      i32_max.raw, MaskFromVec(a).raw, i32_max.raw, i32_max.raw, 0x55)};
  return IfThenElse(overflow_mask, overflow_result, sum);
}

HWY_API Vec512<int64_t> SaturatedAdd(Vec512<int64_t> a, Vec512<int64_t> b) {
  const DFromV<decltype(a)> d;
  const auto sum = a + b;
  const auto overflow_mask = MaskFromVec(
      Vec512<int64_t>{_mm512_ternarylogic_epi64(a.raw, b.raw, sum.raw, 0x42)});
  const auto i64_max = Set(d, LimitsMax<int64_t>());
  const Vec512<int64_t> overflow_result{_mm512_mask_ternarylogic_epi64(
      i64_max.raw, MaskFromVec(a).raw, i64_max.raw, i64_max.raw, 0x55)};
  return IfThenElse(overflow_mask, overflow_result, sum);
}

// ------------------------------ I32/I64 SaturatedSub (MaskFromVec)

HWY_API Vec512<int32_t> SaturatedSub(Vec512<int32_t> a, Vec512<int32_t> b) {
  const DFromV<decltype(a)> d;
  const auto diff = a - b;
  const auto overflow_mask = MaskFromVec(
      Vec512<int32_t>{_mm512_ternarylogic_epi32(a.raw, b.raw, diff.raw, 0x18)});
  const auto i32_max = Set(d, LimitsMax<int32_t>());
  const Vec512<int32_t> overflow_result{_mm512_mask_ternarylogic_epi32(
      i32_max.raw, MaskFromVec(a).raw, i32_max.raw, i32_max.raw, 0x55)};
  return IfThenElse(overflow_mask, overflow_result, diff);
}

HWY_API Vec512<int64_t> SaturatedSub(Vec512<int64_t> a, Vec512<int64_t> b) {
  const DFromV<decltype(a)> d;
  const auto diff = a - b;
  const auto overflow_mask = MaskFromVec(
      Vec512<int64_t>{_mm512_ternarylogic_epi64(a.raw, b.raw, diff.raw, 0x18)});
  const auto i64_max = Set(d, LimitsMax<int64_t>());
  const Vec512<int64_t> overflow_result{_mm512_mask_ternarylogic_epi64(
      i64_max.raw, MaskFromVec(a).raw, i64_max.raw, i64_max.raw, 0x55)};
  return IfThenElse(overflow_mask, overflow_result, diff);
}

// ------------------------------ Mask testing

// Beware: the suffix indicates the number of mask bits, not lane size!

namespace detail {

template <typename T>
HWY_INLINE bool AllFalse(hwy::SizeTag<1> /*tag*/, const Mask512<T> mask) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return _kortestz_mask64_u8(mask.raw, mask.raw);
#else
  return mask.raw == 0;
#endif
}
template <typename T>
HWY_INLINE bool AllFalse(hwy::SizeTag<2> /*tag*/, const Mask512<T> mask) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return _kortestz_mask32_u8(mask.raw, mask.raw);
#else
  return mask.raw == 0;
#endif
}
template <typename T>
HWY_INLINE bool AllFalse(hwy::SizeTag<4> /*tag*/, const Mask512<T> mask) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return _kortestz_mask16_u8(mask.raw, mask.raw);
#else
  return mask.raw == 0;
#endif
}
template <typename T>
HWY_INLINE bool AllFalse(hwy::SizeTag<8> /*tag*/, const Mask512<T> mask) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return _kortestz_mask8_u8(mask.raw, mask.raw);
#else
  return mask.raw == 0;
#endif
}

}  // namespace detail

template <class D, typename T = TFromD<D>>
HWY_API bool AllFalse(D /* tag */, const Mask512<T> mask) {
  return detail::AllFalse(hwy::SizeTag<sizeof(T)>(), mask);
}

namespace detail {

template <typename T>
HWY_INLINE bool AllTrue(hwy::SizeTag<1> /*tag*/, const Mask512<T> mask) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return _kortestc_mask64_u8(mask.raw, mask.raw);
#else
  return mask.raw == 0xFFFFFFFFFFFFFFFFull;
#endif
}
template <typename T>
HWY_INLINE bool AllTrue(hwy::SizeTag<2> /*tag*/, const Mask512<T> mask) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return _kortestc_mask32_u8(mask.raw, mask.raw);
#else
  return mask.raw == 0xFFFFFFFFull;
#endif
}
template <typename T>
HWY_INLINE bool AllTrue(hwy::SizeTag<4> /*tag*/, const Mask512<T> mask) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return _kortestc_mask16_u8(mask.raw, mask.raw);
#else
  return mask.raw == 0xFFFFull;
#endif
}
template <typename T>
HWY_INLINE bool AllTrue(hwy::SizeTag<8> /*tag*/, const Mask512<T> mask) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return _kortestc_mask8_u8(mask.raw, mask.raw);
#else
  return mask.raw == 0xFFull;
#endif
}

}  // namespace detail

template <class D, typename T = TFromD<D>>
HWY_API bool AllTrue(D /* tag */, const Mask512<T> mask) {
  return detail::AllTrue(hwy::SizeTag<sizeof(T)>(), mask);
}

// `p` points to at least 8 readable bytes, not all of which need be valid.
template <class D, HWY_IF_V_SIZE_D(D, 64), typename T = TFromD<D>>
HWY_API Mask512<T> LoadMaskBits(D /* tag */, const uint8_t* HWY_RESTRICT bits) {
  Mask512<T> mask;
  CopyBytes<8 / sizeof(T)>(bits, &mask.raw);
  // N >= 8 (= 512 / 64), so no need to mask invalid bits.
  return mask;
}

// `p` points to at least 8 writable bytes.
template <class D, typename T = TFromD<D>>
HWY_API size_t StoreMaskBits(D /* tag */, Mask512<T> mask, uint8_t* bits) {
  const size_t kNumBytes = 8 / sizeof(T);
  CopyBytes<kNumBytes>(&mask.raw, bits);
  // N >= 8 (= 512 / 64), so no need to mask invalid bits.
  return kNumBytes;
}

template <class D, typename T = TFromD<D>>
HWY_API size_t CountTrue(D /* tag */, const Mask512<T> mask) {
  return PopCount(static_cast<uint64_t>(mask.raw));
}

template <class D, typename T = TFromD<D>, HWY_IF_NOT_T_SIZE(T, 1)>
HWY_API size_t FindKnownFirstTrue(D /* tag */, Mask512<T> mask) {
  return Num0BitsBelowLS1Bit_Nonzero32(mask.raw);
}

template <class D, typename T = TFromD<D>, HWY_IF_T_SIZE(T, 1)>
HWY_API size_t FindKnownFirstTrue(D /* tag */, Mask512<T> mask) {
  return Num0BitsBelowLS1Bit_Nonzero64(mask.raw);
}

template <class D, typename T = TFromD<D>>
HWY_API intptr_t FindFirstTrue(D d, Mask512<T> mask) {
  return mask.raw ? static_cast<intptr_t>(FindKnownFirstTrue(d, mask))
                  : intptr_t{-1};
}

template <class D, typename T = TFromD<D>, HWY_IF_NOT_T_SIZE(T, 1)>
HWY_API size_t FindKnownLastTrue(D /* tag */, Mask512<T> mask) {
  return 31 - Num0BitsAboveMS1Bit_Nonzero32(mask.raw);
}

template <class D, typename T = TFromD<D>, HWY_IF_T_SIZE(T, 1)>
HWY_API size_t FindKnownLastTrue(D /* tag */, Mask512<T> mask) {
  return 63 - Num0BitsAboveMS1Bit_Nonzero64(mask.raw);
}

template <class D, typename T = TFromD<D>>
HWY_API intptr_t FindLastTrue(D d, Mask512<T> mask) {
  return mask.raw ? static_cast<intptr_t>(FindKnownLastTrue(d, mask))
                  : intptr_t{-1};
}

// ------------------------------ Compress

// Always implement 8-bit here even if we lack VBMI2 because we can do better
// than generic_ops (8 at a time) via the native 32-bit compress (16 at a time).
#ifdef HWY_NATIVE_COMPRESS8
#undef HWY_NATIVE_COMPRESS8
#else
#define HWY_NATIVE_COMPRESS8
#endif

namespace detail {

#if HWY_TARGET <= HWY_AVX3_DL  // VBMI2
template <size_t N>
HWY_INLINE Vec128<uint8_t, N> NativeCompress(const Vec128<uint8_t, N> v,
                                             const Mask128<uint8_t, N> mask) {
  return Vec128<uint8_t, N>{_mm_maskz_compress_epi8(mask.raw, v.raw)};
}
HWY_INLINE Vec256<uint8_t> NativeCompress(const Vec256<uint8_t> v,
                                          const Mask256<uint8_t> mask) {
  return Vec256<uint8_t>{_mm256_maskz_compress_epi8(mask.raw, v.raw)};
}
HWY_INLINE Vec512<uint8_t> NativeCompress(const Vec512<uint8_t> v,
                                          const Mask512<uint8_t> mask) {
  return Vec512<uint8_t>{_mm512_maskz_compress_epi8(mask.raw, v.raw)};
}

template <size_t N>
HWY_INLINE Vec128<uint16_t, N> NativeCompress(const Vec128<uint16_t, N> v,
                                              const Mask128<uint16_t, N> mask) {
  return Vec128<uint16_t, N>{_mm_maskz_compress_epi16(mask.raw, v.raw)};
}
HWY_INLINE Vec256<uint16_t> NativeCompress(const Vec256<uint16_t> v,
                                           const Mask256<uint16_t> mask) {
  return Vec256<uint16_t>{_mm256_maskz_compress_epi16(mask.raw, v.raw)};
}
HWY_INLINE Vec512<uint16_t> NativeCompress(const Vec512<uint16_t> v,
                                           const Mask512<uint16_t> mask) {
  return Vec512<uint16_t>{_mm512_maskz_compress_epi16(mask.raw, v.raw)};
}

// Slow on Zen4, do not even define these to prevent accidental usage.
#if HWY_TARGET != HWY_AVX3_ZEN4

template <size_t N>
HWY_INLINE void NativeCompressStore(Vec128<uint8_t, N> v,
                                    Mask128<uint8_t, N> mask,
                                    uint8_t* HWY_RESTRICT unaligned) {
  _mm_mask_compressstoreu_epi8(unaligned, mask.raw, v.raw);
}
HWY_INLINE void NativeCompressStore(Vec256<uint8_t> v, Mask256<uint8_t> mask,
                                    uint8_t* HWY_RESTRICT unaligned) {
  _mm256_mask_compressstoreu_epi8(unaligned, mask.raw, v.raw);
}
HWY_INLINE void NativeCompressStore(Vec512<uint8_t> v, Mask512<uint8_t> mask,
                                    uint8_t* HWY_RESTRICT unaligned) {
  _mm512_mask_compressstoreu_epi8(unaligned, mask.raw, v.raw);
}

template <size_t N>
HWY_INLINE void NativeCompressStore(Vec128<uint16_t, N> v,
                                    Mask128<uint16_t, N> mask,
                                    uint16_t* HWY_RESTRICT unaligned) {
  _mm_mask_compressstoreu_epi16(unaligned, mask.raw, v.raw);
}
HWY_INLINE void NativeCompressStore(Vec256<uint16_t> v, Mask256<uint16_t> mask,
                                    uint16_t* HWY_RESTRICT unaligned) {
  _mm256_mask_compressstoreu_epi16(unaligned, mask.raw, v.raw);
}
HWY_INLINE void NativeCompressStore(Vec512<uint16_t> v, Mask512<uint16_t> mask,
                                    uint16_t* HWY_RESTRICT unaligned) {
  _mm512_mask_compressstoreu_epi16(unaligned, mask.raw, v.raw);
}

#endif  // HWY_TARGET != HWY_AVX3_ZEN4

HWY_INLINE Vec512<uint8_t> NativeExpand(Vec512<uint8_t> v,
                                        Mask512<uint8_t> mask) {
  return Vec512<uint8_t>{_mm512_maskz_expand_epi8(mask.raw, v.raw)};
}

HWY_INLINE Vec512<uint16_t> NativeExpand(Vec512<uint16_t> v,
                                         Mask512<uint16_t> mask) {
  return Vec512<uint16_t>{_mm512_maskz_expand_epi16(mask.raw, v.raw)};
}

template <class D, HWY_IF_V_SIZE_D(D, 64), HWY_IF_U8_D(D)>
HWY_INLINE Vec512<uint8_t> NativeLoadExpand(
    Mask512<uint8_t> mask, D /* d */, const uint8_t* HWY_RESTRICT unaligned) {
  return Vec512<uint8_t>{_mm512_maskz_expandloadu_epi8(mask.raw, unaligned)};
}

template <class D, HWY_IF_V_SIZE_D(D, 64), HWY_IF_U16_D(D)>
HWY_INLINE Vec512<uint16_t> NativeLoadExpand(
    Mask512<uint16_t> mask, D /* d */, const uint16_t* HWY_RESTRICT unaligned) {
  return Vec512<uint16_t>{_mm512_maskz_expandloadu_epi16(mask.raw, unaligned)};
}

#endif  // HWY_TARGET <= HWY_AVX3_DL

template <size_t N>
HWY_INLINE Vec128<uint32_t, N> NativeCompress(Vec128<uint32_t, N> v,
                                              Mask128<uint32_t, N> mask) {
  return Vec128<uint32_t, N>{_mm_maskz_compress_epi32(mask.raw, v.raw)};
}
HWY_INLINE Vec256<uint32_t> NativeCompress(Vec256<uint32_t> v,
                                           Mask256<uint32_t> mask) {
  return Vec256<uint32_t>{_mm256_maskz_compress_epi32(mask.raw, v.raw)};
}
HWY_INLINE Vec512<uint32_t> NativeCompress(Vec512<uint32_t> v,
                                           Mask512<uint32_t> mask) {
  return Vec512<uint32_t>{_mm512_maskz_compress_epi32(mask.raw, v.raw)};
}
// We use table-based compress for 64-bit lanes, see CompressIsPartition.

// Slow on Zen4, do not even define these to prevent accidental usage.
#if HWY_TARGET != HWY_AVX3_ZEN4

template <size_t N>
HWY_INLINE void NativeCompressStore(Vec128<uint32_t, N> v,
                                    Mask128<uint32_t, N> mask,
                                    uint32_t* HWY_RESTRICT unaligned) {
  _mm_mask_compressstoreu_epi32(unaligned, mask.raw, v.raw);
}
HWY_INLINE void NativeCompressStore(Vec256<uint32_t> v, Mask256<uint32_t> mask,
                                    uint32_t* HWY_RESTRICT unaligned) {
  _mm256_mask_compressstoreu_epi32(unaligned, mask.raw, v.raw);
}
HWY_INLINE void NativeCompressStore(Vec512<uint32_t> v, Mask512<uint32_t> mask,
                                    uint32_t* HWY_RESTRICT unaligned) {
  _mm512_mask_compressstoreu_epi32(unaligned, mask.raw, v.raw);
}

template <size_t N>
HWY_INLINE void NativeCompressStore(Vec128<uint64_t, N> v,
                                    Mask128<uint64_t, N> mask,
                                    uint64_t* HWY_RESTRICT unaligned) {
  _mm_mask_compressstoreu_epi64(unaligned, mask.raw, v.raw);
}
HWY_INLINE void NativeCompressStore(Vec256<uint64_t> v, Mask256<uint64_t> mask,
                                    uint64_t* HWY_RESTRICT unaligned) {
  _mm256_mask_compressstoreu_epi64(unaligned, mask.raw, v.raw);
}
HWY_INLINE void NativeCompressStore(Vec512<uint64_t> v, Mask512<uint64_t> mask,
                                    uint64_t* HWY_RESTRICT unaligned) {
  _mm512_mask_compressstoreu_epi64(unaligned, mask.raw, v.raw);
}

template <size_t N>
HWY_INLINE void NativeCompressStore(Vec128<float, N> v, Mask128<float, N> mask,
                                    float* HWY_RESTRICT unaligned) {
  _mm_mask_compressstoreu_ps(unaligned, mask.raw, v.raw);
}
HWY_INLINE void NativeCompressStore(Vec256<float> v, Mask256<float> mask,
                                    float* HWY_RESTRICT unaligned) {
  _mm256_mask_compressstoreu_ps(unaligned, mask.raw, v.raw);
}
HWY_INLINE void NativeCompressStore(Vec512<float> v, Mask512<float> mask,
                                    float* HWY_RESTRICT unaligned) {
  _mm512_mask_compressstoreu_ps(unaligned, mask.raw, v.raw);
}

template <size_t N>
HWY_INLINE void NativeCompressStore(Vec128<double, N> v,
                                    Mask128<double, N> mask,
                                    double* HWY_RESTRICT unaligned) {
  _mm_mask_compressstoreu_pd(unaligned, mask.raw, v.raw);
}
HWY_INLINE void NativeCompressStore(Vec256<double> v, Mask256<double> mask,
                                    double* HWY_RESTRICT unaligned) {
  _mm256_mask_compressstoreu_pd(unaligned, mask.raw, v.raw);
}
HWY_INLINE void NativeCompressStore(Vec512<double> v, Mask512<double> mask,
                                    double* HWY_RESTRICT unaligned) {
  _mm512_mask_compressstoreu_pd(unaligned, mask.raw, v.raw);
}

#endif  // HWY_TARGET != HWY_AVX3_ZEN4

HWY_INLINE Vec512<uint32_t> NativeExpand(Vec512<uint32_t> v,
                                         Mask512<uint32_t> mask) {
  return Vec512<uint32_t>{_mm512_maskz_expand_epi32(mask.raw, v.raw)};
}

HWY_INLINE Vec512<uint64_t> NativeExpand(Vec512<uint64_t> v,
                                         Mask512<uint64_t> mask) {
  return Vec512<uint64_t>{_mm512_maskz_expand_epi64(mask.raw, v.raw)};
}

template <class D, HWY_IF_V_SIZE_D(D, 64), HWY_IF_U32_D(D)>
HWY_INLINE Vec512<uint32_t> NativeLoadExpand(
    Mask512<uint32_t> mask, D /* d */, const uint32_t* HWY_RESTRICT unaligned) {
  return Vec512<uint32_t>{_mm512_maskz_expandloadu_epi32(mask.raw, unaligned)};
}

template <class D, HWY_IF_V_SIZE_D(D, 64), HWY_IF_U64_D(D)>
HWY_INLINE Vec512<uint64_t> NativeLoadExpand(
    Mask512<uint64_t> mask, D /* d */, const uint64_t* HWY_RESTRICT unaligned) {
  return Vec512<uint64_t>{_mm512_maskz_expandloadu_epi64(mask.raw, unaligned)};
}

// For u8x16 and <= u16x16 we can avoid store+load for Compress because there is
// only a single compressed vector (u32x16). Other EmuCompress are implemented
// after the EmuCompressStore they build upon.
template <size_t N>
HWY_INLINE Vec128<uint8_t, N> EmuCompress(Vec128<uint8_t, N> v,
                                          Mask128<uint8_t, N> mask) {
  const DFromV<decltype(v)> d;
  const Rebind<uint32_t, decltype(d)> d32;
  const VFromD<decltype(d32)> v0 = PromoteTo(d32, v);

  const uint64_t mask_bits{mask.raw};
  // Mask type is __mmask16 if v is full 128, else __mmask8.
  using M32 = MFromD<decltype(d32)>;
  const M32 m0{static_cast<typename M32::Raw>(mask_bits)};
  return TruncateTo(d, Compress(v0, m0));
}

template <size_t N>
HWY_INLINE Vec128<uint16_t, N> EmuCompress(Vec128<uint16_t, N> v,
                                           Mask128<uint16_t, N> mask) {
  const DFromV<decltype(v)> d;
  const Rebind<int32_t, decltype(d)> di32;
  const RebindToUnsigned<decltype(di32)> du32;
  const MFromD<decltype(du32)> mask32{static_cast<__mmask8>(mask.raw)};
  // DemoteTo is 2 ops, but likely lower latency than TruncateTo on SKX.
  // Only i32 -> u16 is supported, whereas NativeCompress expects u32.
  const VFromD<decltype(du32)> v32 = BitCast(du32, PromoteTo(di32, v));
  return DemoteTo(d, BitCast(di32, NativeCompress(v32, mask32)));
}

HWY_INLINE Vec256<uint16_t> EmuCompress(Vec256<uint16_t> v,
                                        Mask256<uint16_t> mask) {
  const DFromV<decltype(v)> d;
  const Rebind<int32_t, decltype(d)> di32;
  const RebindToUnsigned<decltype(di32)> du32;
  const Mask512<uint32_t> mask32{static_cast<__mmask16>(mask.raw)};
  const Vec512<uint32_t> v32 = BitCast(du32, PromoteTo(di32, v));
  return DemoteTo(d, BitCast(di32, NativeCompress(v32, mask32)));
}

// See above - small-vector EmuCompressStore are implemented via EmuCompress.
template <class D, HWY_IF_V_SIZE_LE_D(D, 16)>
HWY_INLINE void EmuCompressStore(VFromD<D> v, MFromD<D> mask, D d,
                                 TFromD<D>* HWY_RESTRICT unaligned) {
  StoreU(EmuCompress(v, mask), d, unaligned);
}

template <class D, HWY_IF_U16_D(D), HWY_IF_V_SIZE_D(D, 32)>
HWY_INLINE void EmuCompressStore(VFromD<D> v, MFromD<D> mask, D d,
                                 TFromD<D>* HWY_RESTRICT unaligned) {
  StoreU(EmuCompress(v, mask), d, unaligned);
}

// Main emulation logic for wider vector, starting with EmuCompressStore because
// it is most convenient to merge pieces using memory (concatenating vectors at
// byte offsets is difficult).
template <class D>
HWY_INLINE void EmuCompressStore(Vec256<uint8_t> v, Mask256<uint8_t> mask, D d,
                                 uint8_t* HWY_RESTRICT unaligned) {
  const uint64_t mask_bits{mask.raw};
  const Half<decltype(d)> dh;
  const Rebind<uint32_t, decltype(dh)> d32;
  const Vec512<uint32_t> v0 = PromoteTo(d32, LowerHalf(v));
  const Vec512<uint32_t> v1 = PromoteTo(d32, UpperHalf(dh, v));
  const Mask512<uint32_t> m0{static_cast<__mmask16>(mask_bits & 0xFFFFu)};
  const Mask512<uint32_t> m1{static_cast<__mmask16>(mask_bits >> 16)};
  const Vec128<uint8_t> c0 = TruncateTo(dh, NativeCompress(v0, m0));
  const Vec128<uint8_t> c1 = TruncateTo(dh, NativeCompress(v1, m1));
  uint8_t* HWY_RESTRICT pos = unaligned;
  StoreU(c0, dh, pos);
  StoreU(c1, dh, pos + CountTrue(d32, m0));
}

template <class D>
HWY_INLINE void EmuCompressStore(Vec512<uint8_t> v, Mask512<uint8_t> mask, D d,
                                 uint8_t* HWY_RESTRICT unaligned) {
  const uint64_t mask_bits{mask.raw};
  const Half<Half<decltype(d)>> dq;
  const Rebind<uint32_t, decltype(dq)> d32;
  alignas(64) uint8_t lanes[64];
  Store(v, d, lanes);
  const Vec512<uint32_t> v0 = PromoteTo(d32, LowerHalf(LowerHalf(v)));
  const Vec512<uint32_t> v1 = PromoteTo(d32, Load(dq, lanes + 16));
  const Vec512<uint32_t> v2 = PromoteTo(d32, Load(dq, lanes + 32));
  const Vec512<uint32_t> v3 = PromoteTo(d32, Load(dq, lanes + 48));
  const Mask512<uint32_t> m0{static_cast<__mmask16>(mask_bits & 0xFFFFu)};
  const Mask512<uint32_t> m1{
      static_cast<uint16_t>((mask_bits >> 16) & 0xFFFFu)};
  const Mask512<uint32_t> m2{
      static_cast<uint16_t>((mask_bits >> 32) & 0xFFFFu)};
  const Mask512<uint32_t> m3{static_cast<__mmask16>(mask_bits >> 48)};
  const Vec128<uint8_t> c0 = TruncateTo(dq, NativeCompress(v0, m0));
  const Vec128<uint8_t> c1 = TruncateTo(dq, NativeCompress(v1, m1));
  const Vec128<uint8_t> c2 = TruncateTo(dq, NativeCompress(v2, m2));
  const Vec128<uint8_t> c3 = TruncateTo(dq, NativeCompress(v3, m3));
  uint8_t* HWY_RESTRICT pos = unaligned;
  StoreU(c0, dq, pos);
  pos += CountTrue(d32, m0);
  StoreU(c1, dq, pos);
  pos += CountTrue(d32, m1);
  StoreU(c2, dq, pos);
  pos += CountTrue(d32, m2);
  StoreU(c3, dq, pos);
}

template <class D>
HWY_INLINE void EmuCompressStore(Vec512<uint16_t> v, Mask512<uint16_t> mask,
                                 D d, uint16_t* HWY_RESTRICT unaligned) {
  const Repartition<int32_t, decltype(d)> di32;
  const RebindToUnsigned<decltype(di32)> du32;
  const Half<decltype(d)> dh;
  const Vec512<uint32_t> promoted0 =
      BitCast(du32, PromoteTo(di32, LowerHalf(dh, v)));
  const Vec512<uint32_t> promoted1 =
      BitCast(du32, PromoteTo(di32, UpperHalf(dh, v)));

  const uint64_t mask_bits{mask.raw};
  const uint64_t maskL = mask_bits & 0xFFFF;
  const uint64_t maskH = mask_bits >> 16;
  const Mask512<uint32_t> mask0{static_cast<__mmask16>(maskL)};
  const Mask512<uint32_t> mask1{static_cast<__mmask16>(maskH)};
  const Vec512<uint32_t> compressed0 = NativeCompress(promoted0, mask0);
  const Vec512<uint32_t> compressed1 = NativeCompress(promoted1, mask1);

  const Vec256<uint16_t> demoted0 = DemoteTo(dh, BitCast(di32, compressed0));
  const Vec256<uint16_t> demoted1 = DemoteTo(dh, BitCast(di32, compressed1));

  // Store 256-bit halves
  StoreU(demoted0, dh, unaligned);
  StoreU(demoted1, dh, unaligned + PopCount(maskL));
}

// Finally, the remaining EmuCompress for wide vectors, using EmuCompressStore.
template <typename T>  // 1 or 2 bytes
HWY_INLINE Vec512<T> EmuCompress(Vec512<T> v, Mask512<T> mask) {
  const DFromV<decltype(v)> d;
  alignas(64) T buf[2 * 64 / sizeof(T)];
  EmuCompressStore(v, mask, d, buf);
  return Load(d, buf);
}

HWY_INLINE Vec256<uint8_t> EmuCompress(Vec256<uint8_t> v,
                                       const Mask256<uint8_t> mask) {
  const DFromV<decltype(v)> d;
  alignas(32) uint8_t buf[2 * 32 / sizeof(uint8_t)];
  EmuCompressStore(v, mask, d, buf);
  return Load(d, buf);
}

}  // namespace detail

template <class V, class M, HWY_IF_T_SIZE_ONE_OF_V(V, (1 << 1) | (1 << 2))>
HWY_API V Compress(V v, const M mask) {
  const DFromV<decltype(v)> d;
  const RebindToUnsigned<decltype(d)> du;
  const auto mu = RebindMask(du, mask);
#if HWY_TARGET <= HWY_AVX3_DL  // VBMI2
  return BitCast(d, detail::NativeCompress(BitCast(du, v), mu));
#else
  return BitCast(d, detail::EmuCompress(BitCast(du, v), mu));
#endif
}

template <class V, class M, HWY_IF_T_SIZE_V(V, 4)>
HWY_API V Compress(V v, const M mask) {
  const DFromV<decltype(v)> d;
  const RebindToUnsigned<decltype(d)> du;
  const auto mu = RebindMask(du, mask);
  return BitCast(d, detail::NativeCompress(BitCast(du, v), mu));
}

template <typename T, HWY_IF_T_SIZE(T, 8)>
HWY_API Vec512<T> Compress(Vec512<T> v, Mask512<T> mask) {
  // See CompressIsPartition. u64 is faster than u32.
  alignas(16) static constexpr uint64_t packed_array[256] = {
      // From PrintCompress32x8Tables, without the FirstN extension (there is
      // no benefit to including them because 64-bit CompressStore is anyway
      // masked, but also no harm because TableLookupLanes ignores the MSB).
      0x76543210, 0x76543210, 0x76543201, 0x76543210, 0x76543102, 0x76543120,
      0x76543021, 0x76543210, 0x76542103, 0x76542130, 0x76542031, 0x76542310,
      0x76541032, 0x76541320, 0x76540321, 0x76543210, 0x76532104, 0x76532140,
      0x76532041, 0x76532410, 0x76531042, 0x76531420, 0x76530421, 0x76534210,
      0x76521043, 0x76521430, 0x76520431, 0x76524310, 0x76510432, 0x76514320,
      0x76504321, 0x76543210, 0x76432105, 0x76432150, 0x76432051, 0x76432510,
      0x76431052, 0x76431520, 0x76430521, 0x76435210, 0x76421053, 0x76421530,
      0x76420531, 0x76425310, 0x76410532, 0x76415320, 0x76405321, 0x76453210,
      0x76321054, 0x76321540, 0x76320541, 0x76325410, 0x76310542, 0x76315420,
      0x76305421, 0x76354210, 0x76210543, 0x76215430, 0x76205431, 0x76254310,
      0x76105432, 0x76154320, 0x76054321, 0x76543210, 0x75432106, 0x75432160,
      0x75432061, 0x75432610, 0x75431062, 0x75431620, 0x75430621, 0x75436210,
      0x75421063, 0x75421630, 0x75420631, 0x75426310, 0x75410632, 0x75416320,
      0x75406321, 0x75463210, 0x75321064, 0x75321640, 0x75320641, 0x75326410,
      0x75310642, 0x75316420, 0x75306421, 0x75364210, 0x75210643, 0x75216430,
      0x75206431, 0x75264310, 0x75106432, 0x75164320, 0x75064321, 0x75643210,
      0x74321065, 0x74321650, 0x74320651, 0x74326510, 0x74310652, 0x74316520,
      0x74306521, 0x74365210, 0x74210653, 0x74216530, 0x74206531, 0x74265310,
      0x74106532, 0x74165320, 0x74065321, 0x74653210, 0x73210654, 0x73216540,
      0x73206541, 0x73265410, 0x73106542, 0x73165420, 0x73065421, 0x73654210,
      0x72106543, 0x72165430, 0x72065431, 0x72654310, 0x71065432, 0x71654320,
      0x70654321, 0x76543210, 0x65432107, 0x65432170, 0x65432071, 0x65432710,
      0x65431072, 0x65431720, 0x65430721, 0x65437210, 0x65421073, 0x65421730,
      0x65420731, 0x65427310, 0x65410732, 0x65417320, 0x65407321, 0x65473210,
      0x65321074, 0x65321740, 0x65320741, 0x65327410, 0x65310742, 0x65317420,
      0x65307421, 0x65374210, 0x65210743, 0x65217430, 0x65207431, 0x65274310,
      0x65107432, 0x65174320, 0x65074321, 0x65743210, 0x64321075, 0x64321750,
      0x64320751, 0x64327510, 0x64310752, 0x64317520, 0x64307521, 0x64375210,
      0x64210753, 0x64217530, 0x64207531, 0x64275310, 0x64107532, 0x64175320,
      0x64075321, 0x64753210, 0x63210754, 0x63217540, 0x63207541, 0x63275410,
      0x63107542, 0x63175420, 0x63075421, 0x63754210, 0x62107543, 0x62175430,
      0x62075431, 0x62754310, 0x61075432, 0x61754320, 0x60754321, 0x67543210,
      0x54321076, 0x54321760, 0x54320761, 0x54327610, 0x54310762, 0x54317620,
      0x54307621, 0x54376210, 0x54210763, 0x54217630, 0x54207631, 0x54276310,
      0x54107632, 0x54176320, 0x54076321, 0x54763210, 0x53210764, 0x53217640,
      0x53207641, 0x53276410, 0x53107642, 0x53176420, 0x53076421, 0x53764210,
      0x52107643, 0x52176430, 0x52076431, 0x52764310, 0x51076432, 0x51764320,
      0x50764321, 0x57643210, 0x43210765, 0x43217650, 0x43207651, 0x43276510,
      0x43107652, 0x43176520, 0x43076521, 0x43765210, 0x42107653, 0x42176530,
      0x42076531, 0x42765310, 0x41076532, 0x41765320, 0x40765321, 0x47653210,
      0x32107654, 0x32176540, 0x32076541, 0x32765410, 0x31076542, 0x31765420,
      0x30765421, 0x37654210, 0x21076543, 0x21765430, 0x20765431, 0x27654310,
      0x10765432, 0x17654320, 0x07654321, 0x76543210};

  // For lane i, shift the i-th 4-bit index down to bits [0, 3) -
  // _mm512_permutexvar_epi64 will ignore the upper bits.
  const DFromV<decltype(v)> d;
  const RebindToUnsigned<decltype(d)> du64;
  const auto packed = Set(du64, packed_array[mask.raw]);
  alignas(64) static constexpr uint64_t shifts[8] = {0,  4,  8,  12,
                                                     16, 20, 24, 28};
  const auto indices = Indices512<T>{(packed >> Load(du64, shifts)).raw};
  return TableLookupLanes(v, indices);
}

// ------------------------------ Expand

template <typename T, HWY_IF_T_SIZE(T, 1)>
HWY_API Vec512<T> Expand(Vec512<T> v, const Mask512<T> mask) {
  const Full512<T> d;
#if HWY_TARGET <= HWY_AVX3_DL  // VBMI2
  const RebindToUnsigned<decltype(d)> du;
  const auto mu = RebindMask(du, mask);
  return BitCast(d, detail::NativeExpand(BitCast(du, v), mu));
#else
  // LUTs are infeasible for 2^64 possible masks, so splice together two
  // half-vector Expand.
  const Full256<T> dh;
  constexpr size_t N = 64 / sizeof(T);
  // We have to shift the input by a variable number of u8. Shuffling requires
  // VBMI2, in which case we would already have NativeExpand. We instead
  // load at an offset, which may incur a store to load forwarding stall.
  alignas(64) T lanes[N];
  Store(v, d, lanes);
  using Bits = typename Mask256<T>::Raw;
  const Mask256<T> maskL{
      static_cast<Bits>(mask.raw & Bits{(1ULL << (N / 2)) - 1})};
  const Mask256<T> maskH{static_cast<Bits>(mask.raw >> (N / 2))};
  const size_t countL = CountTrue(dh, maskL);
  const Vec256<T> expandL = Expand(LowerHalf(v), maskL);
  const Vec256<T> expandH = Expand(LoadU(dh, lanes + countL), maskH);
  return Combine(d, expandH, expandL);
#endif
}

template <typename T, HWY_IF_T_SIZE(T, 2)>
HWY_API Vec512<T> Expand(Vec512<T> v, const Mask512<T> mask) {
  const Full512<T> d;
  const RebindToUnsigned<decltype(d)> du;
  const Vec512<uint16_t> vu = BitCast(du, v);
#if HWY_TARGET <= HWY_AVX3_DL  // VBMI2
  return BitCast(d, detail::NativeExpand(vu, RebindMask(du, mask)));
#else   // AVX3
  // LUTs are infeasible for 2^32 possible masks, so splice together two
  // half-vector Expand.
  const Full256<T> dh;
  constexpr size_t N = 64 / sizeof(T);
  using Bits = typename Mask256<T>::Raw;
  const Mask256<T> maskL{
      static_cast<Bits>(mask.raw & Bits{(1ULL << (N / 2)) - 1})};
  const Mask256<T> maskH{static_cast<Bits>(mask.raw >> (N / 2))};
  // In AVX3 we can permutevar, which avoids a potential store to load
  // forwarding stall vs. reloading the input.
  alignas(64) uint16_t iota[64] = {0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10,
                                   11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
                                   22, 23, 24, 25, 26, 27, 28, 29, 30, 31};
  const Vec512<uint16_t> indices = LoadU(du, iota + CountTrue(dh, maskL));
  const Vec512<uint16_t> shifted{_mm512_permutexvar_epi16(indices.raw, vu.raw)};
  const Vec256<T> expandL = Expand(LowerHalf(v), maskL);
  const Vec256<T> expandH = Expand(LowerHalf(BitCast(d, shifted)), maskH);
  return Combine(d, expandH, expandL);
#endif  // AVX3
}

template <class V, class M, HWY_IF_T_SIZE_ONE_OF_V(V, (1 << 4) | (1 << 8))>
HWY_API V Expand(V v, const M mask) {
  const DFromV<decltype(v)> d;
  const RebindToUnsigned<decltype(d)> du;
  const auto mu = RebindMask(du, mask);
  return BitCast(d, detail::NativeExpand(BitCast(du, v), mu));
}

// For smaller vectors, it is likely more efficient to promote to 32-bit.
// This works for u8x16, u16x8, u16x16 (can be promoted to u32x16), but is
// unnecessary if HWY_AVX3_DL, which provides native instructions.
#if HWY_TARGET > HWY_AVX3_DL  // no VBMI2

template <class V, class M, HWY_IF_T_SIZE_ONE_OF_V(V, (1 << 1) | (1 << 2)),
          HWY_IF_LANES_LE_D(DFromV<V>, 16)>
HWY_API V Expand(V v, M mask) {
  const DFromV<V> d;
  const RebindToUnsigned<decltype(d)> du;
  const Rebind<uint32_t, decltype(d)> du32;
  const VFromD<decltype(du)> vu = BitCast(du, v);
  using M32 = MFromD<decltype(du32)>;
  const M32 m32{static_cast<typename M32::Raw>(mask.raw)};
  return BitCast(d, TruncateTo(du, Expand(PromoteTo(du32, vu), m32)));
}

#endif  // HWY_TARGET > HWY_AVX3_DL

// ------------------------------ LoadExpand

template <class D, HWY_IF_V_SIZE_D(D, 64),
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

template <class D, HWY_IF_V_SIZE_D(D, 64),
          HWY_IF_T_SIZE_ONE_OF_D(D, (1 << 4) | (1 << 8))>
HWY_API VFromD<D> LoadExpand(MFromD<D> mask, D d,
                             const TFromD<D>* HWY_RESTRICT unaligned) {
  const RebindToUnsigned<decltype(d)> du;
  using TU = TFromD<decltype(du)>;
  const TU* HWY_RESTRICT pu = reinterpret_cast<const TU*>(unaligned);
  const MFromD<decltype(du)> mu = RebindMask(du, mask);
  return BitCast(d, detail::NativeLoadExpand(mu, du, pu));
}

// ------------------------------ CompressNot

template <class V, class M, HWY_IF_NOT_T_SIZE_V(V, 8)>
HWY_API V CompressNot(V v, const M mask) {
  return Compress(v, Not(mask));
}

template <typename T, HWY_IF_T_SIZE(T, 8)>
HWY_API Vec512<T> CompressNot(Vec512<T> v, Mask512<T> mask) {
  // See CompressIsPartition. u64 is faster than u32.
  alignas(16) static constexpr uint64_t packed_array[256] = {
      // From PrintCompressNot32x8Tables, without the FirstN extension (there is
      // no benefit to including them because 64-bit CompressStore is anyway
      // masked, but also no harm because TableLookupLanes ignores the MSB).
      0x76543210, 0x07654321, 0x17654320, 0x10765432, 0x27654310, 0x20765431,
      0x21765430, 0x21076543, 0x37654210, 0x30765421, 0x31765420, 0x31076542,
      0x32765410, 0x32076541, 0x32176540, 0x32107654, 0x47653210, 0x40765321,
      0x41765320, 0x41076532, 0x42765310, 0x42076531, 0x42176530, 0x42107653,
      0x43765210, 0x43076521, 0x43176520, 0x43107652, 0x43276510, 0x43207651,
      0x43217650, 0x43210765, 0x57643210, 0x50764321, 0x51764320, 0x51076432,
      0x52764310, 0x52076431, 0x52176430, 0x52107643, 0x53764210, 0x53076421,
      0x53176420, 0x53107642, 0x53276410, 0x53207641, 0x53217640, 0x53210764,
      0x54763210, 0x54076321, 0x54176320, 0x54107632, 0x54276310, 0x54207631,
      0x54217630, 0x54210763, 0x54376210, 0x54307621, 0x54317620, 0x54310762,
      0x54327610, 0x54320761, 0x54321760, 0x54321076, 0x67543210, 0x60754321,
      0x61754320, 0x61075432, 0x62754310, 0x62075431, 0x62175430, 0x62107543,
      0x63754210, 0x63075421, 0x63175420, 0x63107542, 0x63275410, 0x63207541,
      0x63217540, 0x63210754, 0x64753210, 0x64075321, 0x64175320, 0x64107532,
      0x64275310, 0x64207531, 0x64217530, 0x64210753, 0x64375210, 0x64307521,
      0x64317520, 0x64310752, 0x64327510, 0x64320751, 0x64321750, 0x64321075,
      0x65743210, 0x65074321, 0x65174320, 0x65107432, 0x65274310, 0x65207431,
      0x65217430, 0x65210743, 0x65374210, 0x65307421, 0x65317420, 0x65310742,
      0x65327410, 0x65320741, 0x65321740, 0x65321074, 0x65473210, 0x65407321,
      0x65417320, 0x65410732, 0x65427310, 0x65420731, 0x65421730, 0x65421073,
      0x65437210, 0x65430721, 0x65431720, 0x65431072, 0x65432710, 0x65432071,
      0x65432170, 0x65432107, 0x76543210, 0x70654321, 0x71654320, 0x71065432,
      0x72654310, 0x72065431, 0x72165430, 0x72106543, 0x73654210, 0x73065421,
      0x73165420, 0x73106542, 0x73265410, 0x73206541, 0x73216540, 0x73210654,
      0x74653210, 0x74065321, 0x74165320, 0x74106532, 0x74265310, 0x74206531,
      0x74216530, 0x74210653, 0x74365210, 0x74306521, 0x74316520, 0x74310652,
      0x74326510, 0x74320651, 0x74321650, 0x74321065, 0x75643210, 0x75064321,
      0x75164320, 0x75106432, 0x75264310, 0x75206431, 0x75216430, 0x75210643,
      0x75364210, 0x75306421, 0x75316420, 0x75310642, 0x75326410, 0x75320641,
      0x75321640, 0x75321064, 0x75463210, 0x75406321, 0x75416320, 0x75410632,
      0x75426310, 0x75420631, 0x75421630, 0x75421063, 0x75436210, 0x75430621,
      0x75431620, 0x75431062, 0x75432610, 0x75432061, 0x75432160, 0x75432106,
      0x76543210, 0x76054321, 0x76154320, 0x76105432, 0x76254310, 0x76205431,
      0x76215430, 0x76210543, 0x76354210, 0x76305421, 0x76315420, 0x76310542,
      0x76325410, 0x76320541, 0x76321540, 0x76321054, 0x76453210, 0x76405321,
      0x76415320, 0x76410532, 0x76425310, 0x76420531, 0x76421530, 0x76421053,
      0x76435210, 0x76430521, 0x76431520, 0x76431052, 0x76432510, 0x76432051,
      0x76432150, 0x76432105, 0x76543210, 0x76504321, 0x76514320, 0x76510432,
      0x76524310, 0x76520431, 0x76521430, 0x76521043, 0x76534210, 0x76530421,
      0x76531420, 0x76531042, 0x76532410, 0x76532041, 0x76532140, 0x76532104,
      0x76543210, 0x76540321, 0x76541320, 0x76541032, 0x76542310, 0x76542031,
      0x76542130, 0x76542103, 0x76543210, 0x76543021, 0x76543120, 0x76543102,
      0x76543210, 0x76543201, 0x76543210, 0x76543210};

  // For lane i, shift the i-th 4-bit index down to bits [0, 3) -
  // _mm512_permutexvar_epi64 will ignore the upper bits.
  const DFromV<decltype(v)> d;
  const RebindToUnsigned<decltype(d)> du64;
  const auto packed = Set(du64, packed_array[mask.raw]);
  alignas(64) static constexpr uint64_t shifts[8] = {0,  4,  8,  12,
                                                     16, 20, 24, 28};
  const auto indices = Indices512<T>{(packed >> Load(du64, shifts)).raw};
  return TableLookupLanes(v, indices);
}

// uint64_t lanes. Only implement for 256 and 512-bit vectors because this is a
// no-op for 128-bit.
template <class V, class M, HWY_IF_V_SIZE_GT_D(DFromV<V>, 16)>
HWY_API V CompressBlocksNot(V v, M mask) {
  return CompressNot(v, mask);
}

// ------------------------------ CompressBits
template <class V>
HWY_API V CompressBits(V v, const uint8_t* HWY_RESTRICT bits) {
  return Compress(v, LoadMaskBits(DFromV<V>(), bits));
}

// ------------------------------ CompressStore

template <class V, class D, HWY_IF_T_SIZE_ONE_OF_V(V, (1 << 1) | (1 << 2))>
HWY_API size_t CompressStore(V v, MFromD<D> mask, D d,
                             TFromD<D>* HWY_RESTRICT unaligned) {
#if HWY_TARGET == HWY_AVX3_ZEN4
  StoreU(Compress(v, mask), d, unaligned);
#else
  const RebindToUnsigned<decltype(d)> du;
  const auto mu = RebindMask(du, mask);
  auto pu = reinterpret_cast<TFromD<decltype(du)> * HWY_RESTRICT>(unaligned);

#if HWY_TARGET <= HWY_AVX3_DL  // VBMI2
  detail::NativeCompressStore(BitCast(du, v), mu, pu);
#else
  detail::EmuCompressStore(BitCast(du, v), mu, du, pu);
#endif
#endif  // HWY_TARGET != HWY_AVX3_ZEN4
  const size_t count = CountTrue(d, mask);
  detail::MaybeUnpoison(unaligned, count);
  return count;
}

template <class V, class D, HWY_IF_NOT_FLOAT_D(D),
          HWY_IF_T_SIZE_ONE_OF_V(V, (1 << 4) | (1 << 8))>
HWY_API size_t CompressStore(V v, MFromD<D> mask, D d,
                             TFromD<D>* HWY_RESTRICT unaligned) {
#if HWY_TARGET == HWY_AVX3_ZEN4
  StoreU(Compress(v, mask), d, unaligned);
#else
  const RebindToUnsigned<decltype(d)> du;
  const auto mu = RebindMask(du, mask);
  using TU = TFromD<decltype(du)>;
  TU* HWY_RESTRICT pu = reinterpret_cast<TU*>(unaligned);
  detail::NativeCompressStore(BitCast(du, v), mu, pu);
#endif  // HWY_TARGET != HWY_AVX3_ZEN4
  const size_t count = CountTrue(d, mask);
  detail::MaybeUnpoison(unaligned, count);
  return count;
}

// Additional overloads to avoid casting to uint32_t (delay?).
template <class D, HWY_IF_FLOAT_D(D)>  // for 128..512
HWY_API size_t CompressStore(VFromD<D> v, MFromD<D> mask, D d,
                             TFromD<D>* HWY_RESTRICT unaligned) {
#if HWY_TARGET == HWY_AVX3_ZEN4
  StoreU(Compress(v, mask), d, unaligned);
#else
  (void)d;
  detail::NativeCompressStore(v, mask, unaligned);
#endif  // HWY_TARGET != HWY_AVX3_ZEN4
  const size_t count = PopCount(uint64_t{mask.raw});
  detail::MaybeUnpoison(unaligned, count);
  return count;
}

// ------------------------------ CompressBlendedStore
template <class D, HWY_IF_V_SIZE_GT_D(D, 8)>  // for full 128..512
HWY_API size_t CompressBlendedStore(VFromD<D> v, MFromD<D> m, D d,
                                    TFromD<D>* HWY_RESTRICT unaligned) {
  // Native CompressStore already does the blending at no extra cost (latency
  // 11, rthroughput 2 - same as compress plus store).
  if (HWY_TARGET == HWY_AVX3_DL ||
      (HWY_TARGET != HWY_AVX3_ZEN4 && sizeof(TFromD<D>) > 2)) {
    return CompressStore(v, m, d, unaligned);
  } else {
    const size_t count = CountTrue(d, m);
    BlendedStore(Compress(v, m), FirstN(d, count), d, unaligned);
    detail::MaybeUnpoison(unaligned, count);
    return count;
  }
}

// ------------------------------ CompressBitsStore
template <class D>  // also for shorter vectors
HWY_API size_t CompressBitsStore(VFromD<D> v, const uint8_t* HWY_RESTRICT bits,
                                 D d, TFromD<D>* HWY_RESTRICT unaligned) {
  return CompressStore(v, LoadMaskBits(d, bits), d, unaligned);
}

// ------------------------------ LoadInterleaved4

// Actually implemented in generic_ops, we just overload LoadTransposedBlocks4.
namespace detail {

// Type-safe wrapper.
template <_MM_PERM_ENUM kPerm, typename T>
Vec512<T> Shuffle128(const Vec512<T> lo, const Vec512<T> hi) {
  return Vec512<T>{_mm512_shuffle_i64x2(lo.raw, hi.raw, kPerm)};
}
template <_MM_PERM_ENUM kPerm>
Vec512<float> Shuffle128(const Vec512<float> lo, const Vec512<float> hi) {
  return Vec512<float>{_mm512_shuffle_f32x4(lo.raw, hi.raw, kPerm)};
}
template <_MM_PERM_ENUM kPerm>
Vec512<double> Shuffle128(const Vec512<double> lo, const Vec512<double> hi) {
  return Vec512<double>{_mm512_shuffle_f64x2(lo.raw, hi.raw, kPerm)};
}

// Input (128-bit blocks):
// 3 2 1 0 (<- first block in unaligned)
// 7 6 5 4
// b a 9 8
// Output:
// 9 6 3 0 (LSB of A)
// a 7 4 1
// b 8 5 2
template <class D, typename T = TFromD<D>>
HWY_API void LoadTransposedBlocks3(D d, const T* HWY_RESTRICT unaligned,
                                   Vec512<T>& A, Vec512<T>& B, Vec512<T>& C) {
  constexpr size_t N = 64 / sizeof(T);
  const Vec512<T> v3210 = LoadU(d, unaligned + 0 * N);
  const Vec512<T> v7654 = LoadU(d, unaligned + 1 * N);
  const Vec512<T> vba98 = LoadU(d, unaligned + 2 * N);

  const Vec512<T> v5421 = detail::Shuffle128<_MM_PERM_BACB>(v3210, v7654);
  const Vec512<T> va976 = detail::Shuffle128<_MM_PERM_CBDC>(v7654, vba98);

  A = detail::Shuffle128<_MM_PERM_CADA>(v3210, va976);
  B = detail::Shuffle128<_MM_PERM_DBCA>(v5421, va976);
  C = detail::Shuffle128<_MM_PERM_DADB>(v5421, vba98);
}

// Input (128-bit blocks):
// 3 2 1 0 (<- first block in unaligned)
// 7 6 5 4
// b a 9 8
// f e d c
// Output:
// c 8 4 0 (LSB of A)
// d 9 5 1
// e a 6 2
// f b 7 3
template <class D, typename T = TFromD<D>>
HWY_API void LoadTransposedBlocks4(D d, const T* HWY_RESTRICT unaligned,
                                   Vec512<T>& vA, Vec512<T>& vB, Vec512<T>& vC,
                                   Vec512<T>& vD) {
  constexpr size_t N = 64 / sizeof(T);
  const Vec512<T> v3210 = LoadU(d, unaligned + 0 * N);
  const Vec512<T> v7654 = LoadU(d, unaligned + 1 * N);
  const Vec512<T> vba98 = LoadU(d, unaligned + 2 * N);
  const Vec512<T> vfedc = LoadU(d, unaligned + 3 * N);

  const Vec512<T> v5410 = detail::Shuffle128<_MM_PERM_BABA>(v3210, v7654);
  const Vec512<T> vdc98 = detail::Shuffle128<_MM_PERM_BABA>(vba98, vfedc);
  const Vec512<T> v7632 = detail::Shuffle128<_MM_PERM_DCDC>(v3210, v7654);
  const Vec512<T> vfeba = detail::Shuffle128<_MM_PERM_DCDC>(vba98, vfedc);
  vA = detail::Shuffle128<_MM_PERM_CACA>(v5410, vdc98);
  vB = detail::Shuffle128<_MM_PERM_DBDB>(v5410, vdc98);
  vC = detail::Shuffle128<_MM_PERM_CACA>(v7632, vfeba);
  vD = detail::Shuffle128<_MM_PERM_DBDB>(v7632, vfeba);
}

}  // namespace detail

// ------------------------------ StoreInterleaved2

// Implemented in generic_ops, we just overload StoreTransposedBlocks2/3/4.

namespace detail {

// Input (128-bit blocks):
// 6 4 2 0 (LSB of i)
// 7 5 3 1
// Output:
// 3 2 1 0
// 7 6 5 4
template <class D, typename T = TFromD<D>>
HWY_API void StoreTransposedBlocks2(const Vec512<T> i, const Vec512<T> j, D d,
                                    T* HWY_RESTRICT unaligned) {
  constexpr size_t N = 64 / sizeof(T);
  const auto j1_j0_i1_i0 = detail::Shuffle128<_MM_PERM_BABA>(i, j);
  const auto j3_j2_i3_i2 = detail::Shuffle128<_MM_PERM_DCDC>(i, j);
  const auto j1_i1_j0_i0 =
      detail::Shuffle128<_MM_PERM_DBCA>(j1_j0_i1_i0, j1_j0_i1_i0);
  const auto j3_i3_j2_i2 =
      detail::Shuffle128<_MM_PERM_DBCA>(j3_j2_i3_i2, j3_j2_i3_i2);
  StoreU(j1_i1_j0_i0, d, unaligned + 0 * N);
  StoreU(j3_i3_j2_i2, d, unaligned + 1 * N);
}

// Input (128-bit blocks):
// 9 6 3 0 (LSB of i)
// a 7 4 1
// b 8 5 2
// Output:
// 3 2 1 0
// 7 6 5 4
// b a 9 8
template <class D, typename T = TFromD<D>>
HWY_API void StoreTransposedBlocks3(const Vec512<T> i, const Vec512<T> j,
                                    const Vec512<T> k, D d,
                                    T* HWY_RESTRICT unaligned) {
  constexpr size_t N = 64 / sizeof(T);
  const Vec512<T> j2_j0_i2_i0 = detail::Shuffle128<_MM_PERM_CACA>(i, j);
  const Vec512<T> i3_i1_k2_k0 = detail::Shuffle128<_MM_PERM_DBCA>(k, i);
  const Vec512<T> j3_j1_k3_k1 = detail::Shuffle128<_MM_PERM_DBDB>(k, j);

  const Vec512<T> out0 =  // i1 k0 j0 i0
      detail::Shuffle128<_MM_PERM_CACA>(j2_j0_i2_i0, i3_i1_k2_k0);
  const Vec512<T> out1 =  // j2 i2 k1 j1
      detail::Shuffle128<_MM_PERM_DBAC>(j3_j1_k3_k1, j2_j0_i2_i0);
  const Vec512<T> out2 =  // k3 j3 i3 k2
      detail::Shuffle128<_MM_PERM_BDDB>(i3_i1_k2_k0, j3_j1_k3_k1);

  StoreU(out0, d, unaligned + 0 * N);
  StoreU(out1, d, unaligned + 1 * N);
  StoreU(out2, d, unaligned + 2 * N);
}

// Input (128-bit blocks):
// c 8 4 0 (LSB of i)
// d 9 5 1
// e a 6 2
// f b 7 3
// Output:
// 3 2 1 0
// 7 6 5 4
// b a 9 8
// f e d c
template <class D, typename T = TFromD<D>>
HWY_API void StoreTransposedBlocks4(const Vec512<T> i, const Vec512<T> j,
                                    const Vec512<T> k, const Vec512<T> l, D d,
                                    T* HWY_RESTRICT unaligned) {
  constexpr size_t N = 64 / sizeof(T);
  const Vec512<T> j1_j0_i1_i0 = detail::Shuffle128<_MM_PERM_BABA>(i, j);
  const Vec512<T> l1_l0_k1_k0 = detail::Shuffle128<_MM_PERM_BABA>(k, l);
  const Vec512<T> j3_j2_i3_i2 = detail::Shuffle128<_MM_PERM_DCDC>(i, j);
  const Vec512<T> l3_l2_k3_k2 = detail::Shuffle128<_MM_PERM_DCDC>(k, l);
  const Vec512<T> out0 =
      detail::Shuffle128<_MM_PERM_CACA>(j1_j0_i1_i0, l1_l0_k1_k0);
  const Vec512<T> out1 =
      detail::Shuffle128<_MM_PERM_DBDB>(j1_j0_i1_i0, l1_l0_k1_k0);
  const Vec512<T> out2 =
      detail::Shuffle128<_MM_PERM_CACA>(j3_j2_i3_i2, l3_l2_k3_k2);
  const Vec512<T> out3 =
      detail::Shuffle128<_MM_PERM_DBDB>(j3_j2_i3_i2, l3_l2_k3_k2);
  StoreU(out0, d, unaligned + 0 * N);
  StoreU(out1, d, unaligned + 1 * N);
  StoreU(out2, d, unaligned + 2 * N);
  StoreU(out3, d, unaligned + 3 * N);
}

}  // namespace detail

// ------------------------------ Shl (LoadDup128)

HWY_API Vec512<uint16_t> operator<<(Vec512<uint16_t> v, Vec512<uint16_t> bits) {
  return Vec512<uint16_t>{_mm512_sllv_epi16(v.raw, bits.raw)};
}

// 8-bit: may use the << overload for uint16_t.
HWY_API Vec512<uint8_t> operator<<(Vec512<uint8_t> v, Vec512<uint8_t> bits) {
  const DFromV<decltype(v)> d;
#if HWY_TARGET <= HWY_AVX3_DL
  // kMask[i] = 0xFF >> i
  alignas(16) static constexpr uint8_t kMasks[16] = {
      0xFF, 0x7F, 0x3F, 0x1F, 0x0F, 0x07, 0x03, 0x01, 0x00};
  // kShl[i] = 1 << i
  alignas(16) static constexpr uint8_t kShl[16] = {0x01, 0x02, 0x04, 0x08,
                                                   0x10, 0x20, 0x40, 0x80};
  v = And(v, TableLookupBytes(LoadDup128(d, kMasks), bits));
  const VFromD<decltype(d)> mul = TableLookupBytes(LoadDup128(d, kShl), bits);
  return VFromD<decltype(d)>{_mm512_gf2p8mul_epi8(v.raw, mul.raw)};
#else
  const Repartition<uint16_t, decltype(d)> dw;
  using VW = VFromD<decltype(dw)>;
  const VW mask = Set(dw, 0x00FF);
  const VW vw = BitCast(dw, v);
  const VW bits16 = BitCast(dw, bits);
  const VW evens = And(vw, mask) << And(bits16, mask);
  // Shift odd lanes in-place
  const VW odds = vw << ShiftRight<8>(bits16);
  return BitCast(d, IfVecThenElse(Set(dw, 0xFF00), odds, evens));
#endif
}

HWY_API Vec512<uint32_t> operator<<(const Vec512<uint32_t> v,
                                    const Vec512<uint32_t> bits) {
  return Vec512<uint32_t>{_mm512_sllv_epi32(v.raw, bits.raw)};
}

HWY_API Vec512<uint64_t> operator<<(const Vec512<uint64_t> v,
                                    const Vec512<uint64_t> bits) {
  return Vec512<uint64_t>{_mm512_sllv_epi64(v.raw, bits.raw)};
}

// Signed left shift is the same as unsigned.
template <typename T, HWY_IF_SIGNED(T)>
HWY_API Vec512<T> operator<<(const Vec512<T> v, const Vec512<T> bits) {
  const DFromV<decltype(v)> di;
  const RebindToUnsigned<decltype(di)> du;
  return BitCast(di, BitCast(du, v) << BitCast(du, bits));
}

// ------------------------------ Shr (IfVecThenElse)

HWY_API Vec512<uint16_t> operator>>(const Vec512<uint16_t> v,
                                    const Vec512<uint16_t> bits) {
  return Vec512<uint16_t>{_mm512_srlv_epi16(v.raw, bits.raw)};
}

// 8-bit uses 16-bit shifts.
template <size_t N>
HWY_API Vec512<uint8_t> operator>>(Vec512<uint8_t> v, Vec512<uint8_t> bits) {
  const DFromV<decltype(v)> d;
  const RepartitionToWide<decltype(d)> dw;
  using VW = VFromD<decltype(dw)>;
  const VW mask = Set(dw, 0x00FF);
  const VW vw = BitCast(dw, v);
  const VW bits16 = BitCast(dw, bits);
  const VW evens = And(vw, mask) >> And(bits16, mask);
  // Shift odd lanes in-place
  const VW odds = vw >> ShiftRight<8>(bits16);
  return BitCast(d, IfVecThenElse(Set(dw, 0xFF00), odds, evens));
}

HWY_API Vec512<uint32_t> operator>>(const Vec512<uint32_t> v,
                                    const Vec512<uint32_t> bits) {
  return Vec512<uint32_t>{_mm512_srlv_epi32(v.raw, bits.raw)};
}

HWY_API Vec512<uint64_t> operator>>(const Vec512<uint64_t> v,
                                    const Vec512<uint64_t> bits) {
  return Vec512<uint64_t>{_mm512_srlv_epi64(v.raw, bits.raw)};
}

HWY_API Vec512<int16_t> operator>>(const Vec512<int16_t> v,
                                   const Vec512<int16_t> bits) {
  return Vec512<int16_t>{_mm512_srav_epi16(v.raw, bits.raw)};
}

HWY_API Vec512<int32_t> operator>>(const Vec512<int32_t> v,
                                   const Vec512<int32_t> bits) {
  return Vec512<int32_t>{_mm512_srav_epi32(v.raw, bits.raw)};
}

HWY_API Vec512<int64_t> operator>>(const Vec512<int64_t> v,
                                   const Vec512<int64_t> bits) {
  return Vec512<int64_t>{_mm512_srav_epi64(v.raw, bits.raw)};
}

// ------------------------------ MulEven/Odd (Shuffle2301, InterleaveLower)

HWY_INLINE Vec512<uint64_t> MulEven(const Vec512<uint64_t> a,
                                    const Vec512<uint64_t> b) {
  const DFromV<decltype(a)> du64;
  const RepartitionToNarrow<decltype(du64)> du32;
  const auto maskL = Set(du64, 0xFFFFFFFFULL);
  const auto a32 = BitCast(du32, a);
  const auto b32 = BitCast(du32, b);
  // Inputs for MulEven: we only need the lower 32 bits
  const auto aH = Shuffle2301(a32);
  const auto bH = Shuffle2301(b32);

  // Knuth double-word multiplication. We use 32x32 = 64 MulEven and only need
  // the even (lower 64 bits of every 128-bit block) results. See
  // https://github.com/hcs0/Hackers-Delight/blob/master/muldwu.c.tat
  const auto aLbL = MulEven(a32, b32);
  const auto w3 = aLbL & maskL;

  const auto t2 = MulEven(aH, b32) + ShiftRight<32>(aLbL);
  const auto w2 = t2 & maskL;
  const auto w1 = ShiftRight<32>(t2);

  const auto t = MulEven(a32, bH) + w2;
  const auto k = ShiftRight<32>(t);

  const auto mulH = MulEven(aH, bH) + w1 + k;
  const auto mulL = ShiftLeft<32>(t) + w3;
  return InterleaveLower(mulL, mulH);
}

HWY_INLINE Vec512<uint64_t> MulOdd(const Vec512<uint64_t> a,
                                   const Vec512<uint64_t> b) {
  const DFromV<decltype(a)> du64;
  const RepartitionToNarrow<decltype(du64)> du32;
  const auto maskL = Set(du64, 0xFFFFFFFFULL);
  const auto a32 = BitCast(du32, a);
  const auto b32 = BitCast(du32, b);
  // Inputs for MulEven: we only need bits [95:64] (= upper half of input)
  const auto aH = Shuffle2301(a32);
  const auto bH = Shuffle2301(b32);

  // Same as above, but we're using the odd results (upper 64 bits per block).
  const auto aLbL = MulEven(a32, b32);
  const auto w3 = aLbL & maskL;

  const auto t2 = MulEven(aH, b32) + ShiftRight<32>(aLbL);
  const auto w2 = t2 & maskL;
  const auto w1 = ShiftRight<32>(t2);

  const auto t = MulEven(a32, bH) + w2;
  const auto k = ShiftRight<32>(t);

  const auto mulH = MulEven(aH, bH) + w1 + k;
  const auto mulL = ShiftLeft<32>(t) + w3;
  return InterleaveUpper(du64, mulL, mulH);
}

// ------------------------------ ReorderWidenMulAccumulate
template <class D, HWY_IF_I32_D(D)>
HWY_API Vec512<int32_t> ReorderWidenMulAccumulate(D /*d32*/, Vec512<int16_t> a,
                                                  Vec512<int16_t> b,
                                                  const Vec512<int32_t> sum0,
                                                  Vec512<int32_t>& /*sum1*/) {
#if HWY_TARGET <= HWY_AVX3_DL
  return Vec512<int32_t>{_mm512_dpwssd_epi32(sum0.raw, a.raw, b.raw)};
#else
  return sum0 + Vec512<int32_t>{_mm512_madd_epi16(a.raw, b.raw)};
#endif
}

HWY_API Vec512<int32_t> RearrangeToOddPlusEven(const Vec512<int32_t> sum0,
                                               Vec512<int32_t> /*sum1*/) {
  return sum0;  // invariant already holds
}

// ------------------------------ Reductions

template <class D>
HWY_API int32_t ReduceSum(D, Vec512<int32_t> v) {
  return _mm512_reduce_add_epi32(v.raw);
}
template <class D>
HWY_API int64_t ReduceSum(D, Vec512<int64_t> v) {
  return _mm512_reduce_add_epi64(v.raw);
}
template <class D>
HWY_API uint32_t ReduceSum(D, Vec512<uint32_t> v) {
  return static_cast<uint32_t>(_mm512_reduce_add_epi32(v.raw));
}
template <class D>
HWY_API uint64_t ReduceSum(D, Vec512<uint64_t> v) {
  return static_cast<uint64_t>(_mm512_reduce_add_epi64(v.raw));
}
template <class D>
HWY_API float ReduceSum(D, Vec512<float> v) {
  return _mm512_reduce_add_ps(v.raw);
}
template <class D>
HWY_API double ReduceSum(D, Vec512<double> v) {
  return  _mm512_reduce_add_pd(v.raw);
}
template <class D>
HWY_API uint16_t ReduceSum(D d, Vec512<uint16_t> v) {
  const RepartitionToWide<decltype(d)> d32;
  const auto even = And(BitCast(d32, v), Set(d32, 0xFFFF));
  const auto odd = ShiftRight<16>(BitCast(d32, v));
  const auto sum = ReduceSum(d32, even + odd);
  return static_cast<uint16_t>(sum);
}
template <class D>
HWY_API int16_t ReduceSum(D d, Vec512<int16_t> v) {
  const RepartitionToWide<decltype(d)> d32;
  // Sign-extend
  const auto even = ShiftRight<16>(ShiftLeft<16>(BitCast(d32, v)));
  const auto odd = ShiftRight<16>(BitCast(d32, v));
  const auto sum = ReduceSum(d32, even + odd);
  return static_cast<int16_t>(sum);
}

// Returns the sum in each lane.
template <class D, typename T>
HWY_API Vec512<T> SumOfLanes(D d, Vec512<T> v) {
  return Set(d, ReduceSum(d, v));
}

// Returns the minimum in each lane.
template <class D>
HWY_API Vec512<int32_t> MinOfLanes(D d, Vec512<int32_t> v) {
  return Set(d, _mm512_reduce_min_epi32(v.raw));
}
template <class D>
HWY_API Vec512<int64_t> MinOfLanes(D d, Vec512<int64_t> v) {
  return Set(d, _mm512_reduce_min_epi64(v.raw));
}
template <class D>
HWY_API Vec512<uint32_t> MinOfLanes(D d, Vec512<uint32_t> v) {
  return Set(d, _mm512_reduce_min_epu32(v.raw));
}
template <class D>
HWY_API Vec512<uint64_t> MinOfLanes(D d, Vec512<uint64_t> v) {
  return Set(d, _mm512_reduce_min_epu64(v.raw));
}
template <class D>
HWY_API Vec512<float> MinOfLanes(D d, Vec512<float> v) {
  return Set(d, _mm512_reduce_min_ps(v.raw));
}
template <class D>
HWY_API Vec512<double> MinOfLanes(D d, Vec512<double> v) {
  return Set(d, _mm512_reduce_min_pd(v.raw));
}
template <class D>
HWY_API Vec512<uint16_t> MinOfLanes(D d, Vec512<uint16_t> v) {
  const RepartitionToWide<decltype(d)> d32;
  const auto even = And(BitCast(d32, v), Set(d32, 0xFFFF));
  const auto odd = ShiftRight<16>(BitCast(d32, v));
  const auto min = MinOfLanes(d32, Min(even, odd));
  // Also broadcast into odd lanes.
  return OddEven(BitCast(d, ShiftLeft<16>(min)), BitCast(d, min));
}
template <class D>
HWY_API Vec512<int16_t> MinOfLanes(D d, Vec512<int16_t> v) {
  const RepartitionToWide<decltype(d)> d32;
  // Sign-extend
  const auto even = ShiftRight<16>(ShiftLeft<16>(BitCast(d32, v)));
  const auto odd = ShiftRight<16>(BitCast(d32, v));
  const auto min = MinOfLanes(d32, Min(even, odd));
  // Also broadcast into odd lanes.
  return OddEven(BitCast(d, ShiftLeft<16>(min)), BitCast(d, min));
}

// Returns the maximum in each lane.
template <class D>
HWY_API Vec512<int32_t> MaxOfLanes(D d, Vec512<int32_t> v) {
  return Set(d, _mm512_reduce_max_epi32(v.raw));
}
template <class D>
HWY_API Vec512<int64_t> MaxOfLanes(D d, Vec512<int64_t> v) {
  return Set(d, _mm512_reduce_max_epi64(v.raw));
}
template <class D>
HWY_API Vec512<uint32_t> MaxOfLanes(D d, Vec512<uint32_t> v) {
  return Set(d, _mm512_reduce_max_epu32(v.raw));
}
template <class D>
HWY_API Vec512<uint64_t> MaxOfLanes(D d, Vec512<uint64_t> v) {
  return Set(d, _mm512_reduce_max_epu64(v.raw));
}
template <class D>
HWY_API Vec512<float> MaxOfLanes(D d, Vec512<float> v) {
  return Set(d, _mm512_reduce_max_ps(v.raw));
}
template <class D>
HWY_API Vec512<double> MaxOfLanes(D d, Vec512<double> v) {
  return Set(d, _mm512_reduce_max_pd(v.raw));
}
template <class D>
HWY_API Vec512<uint16_t> MaxOfLanes(D d, Vec512<uint16_t> v) {
  const RepartitionToWide<decltype(d)> d32;
  const auto even = And(BitCast(d32, v), Set(d32, 0xFFFF));
  const auto odd = ShiftRight<16>(BitCast(d32, v));
  const auto min = MaxOfLanes(d32, Max(even, odd));
  // Also broadcast into odd lanes.
  return OddEven(BitCast(d, ShiftLeft<16>(min)), BitCast(d, min));
}
template <class D>
HWY_API Vec512<int16_t> MaxOfLanes(D d, Vec512<int16_t> v) {
  const RepartitionToWide<decltype(d)> d32;
  // Sign-extend
  const auto even = ShiftRight<16>(ShiftLeft<16>(BitCast(d32, v)));
  const auto odd = ShiftRight<16>(BitCast(d32, v));
  const auto min = MaxOfLanes(d32, Max(even, odd));
  // Also broadcast into odd lanes.
  return OddEven(BitCast(d, ShiftLeft<16>(min)), BitCast(d, min));
}

// -------------------- LeadingZeroCount, TrailingZeroCount, HighestSetBitIndex

template <class V, HWY_IF_UI32(TFromV<V>), HWY_IF_V_SIZE_D(DFromV<V>, 64)>
HWY_API V LeadingZeroCount(V v) {
  return V{_mm512_lzcnt_epi32(v.raw)};
}

template <class V, HWY_IF_UI64(TFromV<V>), HWY_IF_V_SIZE_D(DFromV<V>, 64)>
HWY_API V LeadingZeroCount(V v) {
  return V{_mm512_lzcnt_epi64(v.raw)};
}

namespace detail {

template <class V, HWY_IF_UNSIGNED_V(V),
          HWY_IF_T_SIZE_ONE_OF_V(V, (1 << 1) | (1 << 2)),
          HWY_IF_LANES_LE_D(DFromV<V>, 16)>
HWY_INLINE V Lzcnt32ForU8OrU16(V v) {
  const DFromV<decltype(v)> d;
  const Rebind<int32_t, decltype(d)> di32;
  const Rebind<uint32_t, decltype(d)> du32;

  const auto v_lz_count = LeadingZeroCount(PromoteTo(du32, v));
  return DemoteTo(d, BitCast(di32, v_lz_count));
}

template <class V, HWY_IF_UNSIGNED_V(V),
          HWY_IF_T_SIZE_ONE_OF_V(V, (1 << 1) | (1 << 2)),
          HWY_IF_LANES_D(DFromV<V>, 32)>
HWY_INLINE VFromD<Rebind<uint16_t, DFromV<V>>> Lzcnt32ForU8OrU16AsU16(V v) {
  const DFromV<decltype(v)> d;
  const Half<decltype(d)> dh;
  const Rebind<int32_t, decltype(dh)> di32;
  const Rebind<uint32_t, decltype(dh)> du32;
  const Rebind<uint16_t, decltype(d)> du16;

  const auto lo_v_lz_count =
      LeadingZeroCount(PromoteTo(du32, LowerHalf(dh, v)));
  const auto hi_v_lz_count =
      LeadingZeroCount(PromoteTo(du32, UpperHalf(dh, v)));
  return OrderedDemote2To(du16, BitCast(di32, lo_v_lz_count),
                          BitCast(di32, hi_v_lz_count));
}

HWY_INLINE Vec256<uint8_t> Lzcnt32ForU8OrU16(Vec256<uint8_t> v) {
  const DFromV<decltype(v)> d;
  const Rebind<int16_t, decltype(d)> di16;
  return DemoteTo(d, BitCast(di16, Lzcnt32ForU8OrU16AsU16(v)));
}

HWY_INLINE Vec512<uint8_t> Lzcnt32ForU8OrU16(Vec512<uint8_t> v) {
  const DFromV<decltype(v)> d;
  const Half<decltype(d)> dh;
  const Rebind<int16_t, decltype(dh)> di16;

  const auto lo_half = LowerHalf(dh, v);
  const auto hi_half = UpperHalf(dh, v);

  const auto lo_v_lz_count = BitCast(di16, Lzcnt32ForU8OrU16AsU16(lo_half));
  const auto hi_v_lz_count = BitCast(di16, Lzcnt32ForU8OrU16AsU16(hi_half));
  return OrderedDemote2To(d, lo_v_lz_count, hi_v_lz_count);
}

HWY_INLINE Vec512<uint16_t> Lzcnt32ForU8OrU16(Vec512<uint16_t> v) {
  return Lzcnt32ForU8OrU16AsU16(v);
}

}  // namespace detail

template <class V, HWY_IF_NOT_FLOAT_NOR_SPECIAL_V(V),
          HWY_IF_T_SIZE_ONE_OF_V(V, (1 << 1) | (1 << 2))>
HWY_API V LeadingZeroCount(V v) {
  const DFromV<decltype(v)> d;
  const RebindToUnsigned<decltype(d)> du;
  using TU = TFromD<decltype(du)>;

  constexpr TU kNumOfBitsInT{sizeof(TU) * 8};
  const auto v_lzcnt32 = detail::Lzcnt32ForU8OrU16(BitCast(du, v));
  return BitCast(d, Min(v_lzcnt32 - Set(du, TU{32 - kNumOfBitsInT}),
                        Set(du, TU{kNumOfBitsInT})));
}

template <class V, HWY_IF_NOT_FLOAT_NOR_SPECIAL_V(V),
          HWY_IF_T_SIZE_ONE_OF_V(V, (1 << 1) | (1 << 2))>
HWY_API V HighestSetBitIndex(V v) {
  const DFromV<decltype(v)> d;
  const RebindToUnsigned<decltype(d)> du;
  using TU = TFromD<decltype(du)>;
  return BitCast(d,
                 Set(du, TU{31}) - detail::Lzcnt32ForU8OrU16(BitCast(du, v)));
}

template <class V, HWY_IF_NOT_FLOAT_NOR_SPECIAL_V(V),
          HWY_IF_T_SIZE_ONE_OF_V(V, (1 << 4) | (1 << 8))>
HWY_API V HighestSetBitIndex(V v) {
  const DFromV<decltype(v)> d;
  using T = TFromD<decltype(d)>;
  return BitCast(d, Set(d, T{sizeof(T) * 8 - 1}) - LeadingZeroCount(v));
}

template <class V, HWY_IF_NOT_FLOAT_NOR_SPECIAL_V(V)>
HWY_API V TrailingZeroCount(V v) {
  const DFromV<decltype(v)> d;
  const RebindToSigned<decltype(d)> di;
  using T = TFromD<decltype(d)>;

  const auto vi = BitCast(di, v);
  const auto lowest_bit = BitCast(d, And(vi, Neg(vi)));
  constexpr T kNumOfBitsInT{sizeof(T) * 8};
  const auto bit_idx = HighestSetBitIndex(lowest_bit);
  return IfThenElse(MaskFromVec(bit_idx), Set(d, kNumOfBitsInT), bit_idx);
}

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace hwy
HWY_AFTER_NAMESPACE();

// Note that the GCC warnings are not suppressed if we only wrap the *intrin.h -
// the warning seems to be issued at the call site of intrinsics, i.e. our code.
HWY_DIAGNOSTICS(pop)
