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

// 256-bit vectors and AVX2 instructions, plus some AVX512-VL operations when
// compiling for that target.
// External include guard in highway.h - see comment there.

// WARNING: most operations do not cross 128-bit block boundaries. In
// particular, "Broadcast", pack and zip behavior may be surprising.

#include <immintrin.h>  // AVX2+
#if defined(_MSC_VER) && defined(__clang__)
// Including <immintrin.h> should be enough, but Clang's headers helpfully skip
// including these headers when _MSC_VER is defined, like when using clang-cl.
// Include these directly here.
#include <avxintrin.h>
// avxintrin defines __m256i and must come before avx2intrin.
#include <avx2intrin.h>
#include <bmi2intrin.h>  // _pext_u64
#include <f16cintrin.h>
#include <fmaintrin.h>
#include <smmintrin.h>
#endif

#include <stddef.h>
#include <stdint.h>

// For half-width vectors. Already includes base.h and shared-inl.h.
#include "hwy/ops/x86_128-inl.h"

HWY_BEFORE_NAMESPACE();
namespace hwy {
namespace HWY_NAMESPACE {

template <typename T>
using Full256 = Simd<T, 32 / sizeof(T)>;

namespace detail {

template <typename T>
struct Raw256 {
  using type = __m256i;
};
template <>
struct Raw256<float> {
  using type = __m256;
};
template <>
struct Raw256<double> {
  using type = __m256d;
};

}  // namespace detail

template <typename T>
class Vec256 {
  using Raw = typename detail::Raw256<T>::type;

 public:
  // Compound assignment. Only usable if there is a corresponding non-member
  // binary operator overload. For example, only f32 and f64 support division.
  HWY_INLINE Vec256& operator*=(const Vec256 other) {
    return *this = (*this * other);
  }
  HWY_INLINE Vec256& operator/=(const Vec256 other) {
    return *this = (*this / other);
  }
  HWY_INLINE Vec256& operator+=(const Vec256 other) {
    return *this = (*this + other);
  }
  HWY_INLINE Vec256& operator-=(const Vec256 other) {
    return *this = (*this - other);
  }
  HWY_INLINE Vec256& operator&=(const Vec256 other) {
    return *this = (*this & other);
  }
  HWY_INLINE Vec256& operator|=(const Vec256 other) {
    return *this = (*this | other);
  }
  HWY_INLINE Vec256& operator^=(const Vec256 other) {
    return *this = (*this ^ other);
  }

  Raw raw;
};

#if HWY_TARGET <= HWY_AVX3

namespace detail {

// Template arg: sizeof(lane type)
template <size_t size>
struct RawMask256 {};
template <>
struct RawMask256<1> {
  using type = __mmask32;
};
template <>
struct RawMask256<2> {
  using type = __mmask16;
};
template <>
struct RawMask256<4> {
  using type = __mmask8;
};
template <>
struct RawMask256<8> {
  using type = __mmask8;
};

}  // namespace detail

template <typename T>
struct Mask256 {
  using Raw = typename detail::RawMask256<sizeof(T)>::type;

  static Mask256<T> FromBits(uint64_t mask_bits) {
    return Mask256<T>{static_cast<Raw>(mask_bits)};
  }

  Raw raw;
};

#else  // AVX2

// FF..FF or 0.
template <typename T>
struct Mask256 {
  typename detail::Raw256<T>::type raw;
};

#endif  // HWY_TARGET <= HWY_AVX3

// ------------------------------ BitCast

namespace detail {

HWY_INLINE __m256i BitCastToInteger(__m256i v) { return v; }
HWY_INLINE __m256i BitCastToInteger(__m256 v) { return _mm256_castps_si256(v); }
HWY_INLINE __m256i BitCastToInteger(__m256d v) {
  return _mm256_castpd_si256(v);
}

template <typename T>
HWY_INLINE Vec256<uint8_t> BitCastToByte(Vec256<T> v) {
  return Vec256<uint8_t>{BitCastToInteger(v.raw)};
}

// Cannot rely on function overloading because return types differ.
template <typename T>
struct BitCastFromInteger256 {
  HWY_INLINE __m256i operator()(__m256i v) { return v; }
};
template <>
struct BitCastFromInteger256<float> {
  HWY_INLINE __m256 operator()(__m256i v) { return _mm256_castsi256_ps(v); }
};
template <>
struct BitCastFromInteger256<double> {
  HWY_INLINE __m256d operator()(__m256i v) { return _mm256_castsi256_pd(v); }
};

template <typename T>
HWY_INLINE Vec256<T> BitCastFromByte(Full256<T> /* tag */, Vec256<uint8_t> v) {
  return Vec256<T>{BitCastFromInteger256<T>()(v.raw)};
}

}  // namespace detail

template <typename T, typename FromT>
HWY_API Vec256<T> BitCast(Full256<T> d, Vec256<FromT> v) {
  return detail::BitCastFromByte(d, detail::BitCastToByte(v));
}

// ------------------------------ Set

// Returns an all-zero vector.
template <typename T>
HWY_API Vec256<T> Zero(Full256<T> /* tag */) {
  return Vec256<T>{_mm256_setzero_si256()};
}
HWY_API Vec256<float> Zero(Full256<float> /* tag */) {
  return Vec256<float>{_mm256_setzero_ps()};
}
HWY_API Vec256<double> Zero(Full256<double> /* tag */) {
  return Vec256<double>{_mm256_setzero_pd()};
}

// Returns a vector with all lanes set to "t".
HWY_API Vec256<uint8_t> Set(Full256<uint8_t> /* tag */, const uint8_t t) {
  return Vec256<uint8_t>{_mm256_set1_epi8(static_cast<char>(t))};  // NOLINT
}
HWY_API Vec256<uint16_t> Set(Full256<uint16_t> /* tag */, const uint16_t t) {
  return Vec256<uint16_t>{_mm256_set1_epi16(static_cast<short>(t))};  // NOLINT
}
HWY_API Vec256<uint32_t> Set(Full256<uint32_t> /* tag */, const uint32_t t) {
  return Vec256<uint32_t>{_mm256_set1_epi32(static_cast<int>(t))};
}
HWY_API Vec256<uint64_t> Set(Full256<uint64_t> /* tag */, const uint64_t t) {
  return Vec256<uint64_t>{
      _mm256_set1_epi64x(static_cast<long long>(t))};  // NOLINT
}
HWY_API Vec256<int8_t> Set(Full256<int8_t> /* tag */, const int8_t t) {
  return Vec256<int8_t>{_mm256_set1_epi8(static_cast<char>(t))};  // NOLINT
}
HWY_API Vec256<int16_t> Set(Full256<int16_t> /* tag */, const int16_t t) {
  return Vec256<int16_t>{_mm256_set1_epi16(static_cast<short>(t))};  // NOLINT
}
HWY_API Vec256<int32_t> Set(Full256<int32_t> /* tag */, const int32_t t) {
  return Vec256<int32_t>{_mm256_set1_epi32(t)};
}
HWY_API Vec256<int64_t> Set(Full256<int64_t> /* tag */, const int64_t t) {
  return Vec256<int64_t>{
      _mm256_set1_epi64x(static_cast<long long>(t))};  // NOLINT
}
HWY_API Vec256<float> Set(Full256<float> /* tag */, const float t) {
  return Vec256<float>{_mm256_set1_ps(t)};
}
HWY_API Vec256<double> Set(Full256<double> /* tag */, const double t) {
  return Vec256<double>{_mm256_set1_pd(t)};
}

HWY_DIAGNOSTICS(push)
HWY_DIAGNOSTICS_OFF(disable : 4700, ignored "-Wuninitialized")

// Returns a vector with uninitialized elements.
template <typename T>
HWY_API Vec256<T> Undefined(Full256<T> /* tag */) {
  // Available on Clang 6.0, GCC 6.2, ICC 16.03, MSVC 19.14. All but ICC
  // generate an XOR instruction.
  return Vec256<T>{_mm256_undefined_si256()};
}
HWY_API Vec256<float> Undefined(Full256<float> /* tag */) {
  return Vec256<float>{_mm256_undefined_ps()};
}
HWY_API Vec256<double> Undefined(Full256<double> /* tag */) {
  return Vec256<double>{_mm256_undefined_pd()};
}

HWY_DIAGNOSTICS(pop)

// ================================================== LOGICAL

// ------------------------------ And

template <typename T>
HWY_API Vec256<T> And(Vec256<T> a, Vec256<T> b) {
  return Vec256<T>{_mm256_and_si256(a.raw, b.raw)};
}

HWY_API Vec256<float> And(const Vec256<float> a, const Vec256<float> b) {
  return Vec256<float>{_mm256_and_ps(a.raw, b.raw)};
}
HWY_API Vec256<double> And(const Vec256<double> a, const Vec256<double> b) {
  return Vec256<double>{_mm256_and_pd(a.raw, b.raw)};
}

// ------------------------------ AndNot

// Returns ~not_mask & mask.
template <typename T>
HWY_API Vec256<T> AndNot(Vec256<T> not_mask, Vec256<T> mask) {
  return Vec256<T>{_mm256_andnot_si256(not_mask.raw, mask.raw)};
}
HWY_API Vec256<float> AndNot(const Vec256<float> not_mask,
                             const Vec256<float> mask) {
  return Vec256<float>{_mm256_andnot_ps(not_mask.raw, mask.raw)};
}
HWY_API Vec256<double> AndNot(const Vec256<double> not_mask,
                              const Vec256<double> mask) {
  return Vec256<double>{_mm256_andnot_pd(not_mask.raw, mask.raw)};
}

// ------------------------------ Or

template <typename T>
HWY_API Vec256<T> Or(Vec256<T> a, Vec256<T> b) {
  return Vec256<T>{_mm256_or_si256(a.raw, b.raw)};
}

HWY_API Vec256<float> Or(const Vec256<float> a, const Vec256<float> b) {
  return Vec256<float>{_mm256_or_ps(a.raw, b.raw)};
}
HWY_API Vec256<double> Or(const Vec256<double> a, const Vec256<double> b) {
  return Vec256<double>{_mm256_or_pd(a.raw, b.raw)};
}

// ------------------------------ Xor

template <typename T>
HWY_API Vec256<T> Xor(Vec256<T> a, Vec256<T> b) {
  return Vec256<T>{_mm256_xor_si256(a.raw, b.raw)};
}

HWY_API Vec256<float> Xor(const Vec256<float> a, const Vec256<float> b) {
  return Vec256<float>{_mm256_xor_ps(a.raw, b.raw)};
}
HWY_API Vec256<double> Xor(const Vec256<double> a, const Vec256<double> b) {
  return Vec256<double>{_mm256_xor_pd(a.raw, b.raw)};
}

// ------------------------------ Not

template <typename T>
HWY_API Vec256<T> Not(const Vec256<T> v) {
  using TU = MakeUnsigned<T>;
#if HWY_TARGET <= HWY_AVX3
  const __m256i vu = BitCast(Full256<TU>(), v).raw;
  return BitCast(Full256<T>(),
                 Vec256<TU>{_mm256_ternarylogic_epi32(vu, vu, vu, 0x55)});
#else
  return Xor(v, BitCast(Full256<T>(), Vec256<TU>{_mm256_set1_epi32(-1)}));
#endif
}

// ------------------------------ Operator overloads (internal-only if float)

template <typename T>
HWY_API Vec256<T> operator&(const Vec256<T> a, const Vec256<T> b) {
  return And(a, b);
}

template <typename T>
HWY_API Vec256<T> operator|(const Vec256<T> a, const Vec256<T> b) {
  return Or(a, b);
}

template <typename T>
HWY_API Vec256<T> operator^(const Vec256<T> a, const Vec256<T> b) {
  return Xor(a, b);
}

// ------------------------------ PopulationCount

// 8/16 require BITALG, 32/64 require VPOPCNTDQ.
#if HWY_TARGET == HWY_AVX3_DL

#ifdef HWY_NATIVE_POPCNT
#undef HWY_NATIVE_POPCNT
#else
#define HWY_NATIVE_POPCNT
#endif

namespace detail {

template <typename T>
HWY_INLINE Vec256<T> PopulationCount(hwy::SizeTag<1> /* tag */, Vec256<T> v) {
  return Vec256<T>{_mm256_popcnt_epi8(v.raw)};
}
template <typename T>
HWY_INLINE Vec256<T> PopulationCount(hwy::SizeTag<2> /* tag */, Vec256<T> v) {
  return Vec256<T>{_mm256_popcnt_epi16(v.raw)};
}
template <typename T>
HWY_INLINE Vec256<T> PopulationCount(hwy::SizeTag<4> /* tag */, Vec256<T> v) {
  return Vec256<T>{_mm256_popcnt_epi32(v.raw)};
}
template <typename T>
HWY_INLINE Vec256<T> PopulationCount(hwy::SizeTag<8> /* tag */, Vec256<T> v) {
  return Vec256<T>{_mm256_popcnt_epi64(v.raw)};
}

}  // namespace detail

template <typename T>
HWY_API Vec256<T> PopulationCount(Vec256<T> v) {
  return detail::PopulationCount(hwy::SizeTag<sizeof(T)>(), v);
}

#endif  // HWY_TARGET == HWY_AVX3_DL

// ================================================== SIGN

// ------------------------------ CopySign

template <typename T>
HWY_API Vec256<T> CopySign(const Vec256<T> magn, const Vec256<T> sign) {
  static_assert(IsFloat<T>(), "Only makes sense for floating-point");

  const Full256<T> d;
  const auto msb = SignBit(d);

#if HWY_TARGET <= HWY_AVX3
  const Rebind<MakeUnsigned<T>, decltype(d)> du;
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
  const __m256i out = _mm256_ternarylogic_epi32(
      BitCast(du, msb).raw, BitCast(du, magn).raw, BitCast(du, sign).raw, 0xAC);
  return BitCast(d, decltype(Zero(du)){out});
#else
  return Or(AndNot(msb, magn), And(msb, sign));
#endif
}

template <typename T>
HWY_API Vec256<T> CopySignToAbs(const Vec256<T> abs, const Vec256<T> sign) {
#if HWY_TARGET <= HWY_AVX3
  // AVX3 can also handle abs < 0, so no extra action needed.
  return CopySign(abs, sign);
#else
  return Or(abs, And(SignBit(Full256<T>()), sign));
#endif
}

// ================================================== MASK

#if HWY_TARGET <= HWY_AVX3

// ------------------------------ IfThenElse

// Returns mask ? b : a.

namespace detail {

// Templates for signed/unsigned integer of a particular size.
template <typename T>
HWY_INLINE Vec256<T> IfThenElse(hwy::SizeTag<1> /* tag */, Mask256<T> mask,
                                Vec256<T> yes, Vec256<T> no) {
  return Vec256<T>{_mm256_mask_mov_epi8(no.raw, mask.raw, yes.raw)};
}
template <typename T>
HWY_INLINE Vec256<T> IfThenElse(hwy::SizeTag<2> /* tag */, Mask256<T> mask,
                                Vec256<T> yes, Vec256<T> no) {
  return Vec256<T>{_mm256_mask_mov_epi16(no.raw, mask.raw, yes.raw)};
}
template <typename T>
HWY_INLINE Vec256<T> IfThenElse(hwy::SizeTag<4> /* tag */, Mask256<T> mask,
                                Vec256<T> yes, Vec256<T> no) {
  return Vec256<T>{_mm256_mask_mov_epi32(no.raw, mask.raw, yes.raw)};
}
template <typename T>
HWY_INLINE Vec256<T> IfThenElse(hwy::SizeTag<8> /* tag */, Mask256<T> mask,
                                Vec256<T> yes, Vec256<T> no) {
  return Vec256<T>{_mm256_mask_mov_epi64(no.raw, mask.raw, yes.raw)};
}

}  // namespace detail

template <typename T>
HWY_API Vec256<T> IfThenElse(Mask256<T> mask, Vec256<T> yes, Vec256<T> no) {
  return detail::IfThenElse(hwy::SizeTag<sizeof(T)>(), mask, yes, no);
}
HWY_API Vec256<float> IfThenElse(Mask256<float> mask, Vec256<float> yes,
                                 Vec256<float> no) {
  return Vec256<float>{_mm256_mask_mov_ps(no.raw, mask.raw, yes.raw)};
}
HWY_API Vec256<double> IfThenElse(Mask256<double> mask, Vec256<double> yes,
                                  Vec256<double> no) {
  return Vec256<double>{_mm256_mask_mov_pd(no.raw, mask.raw, yes.raw)};
}

namespace detail {

template <typename T>
HWY_INLINE Vec256<T> IfThenElseZero(hwy::SizeTag<1> /* tag */, Mask256<T> mask,
                                    Vec256<T> yes) {
  return Vec256<T>{_mm256_maskz_mov_epi8(mask.raw, yes.raw)};
}
template <typename T>
HWY_INLINE Vec256<T> IfThenElseZero(hwy::SizeTag<2> /* tag */, Mask256<T> mask,
                                    Vec256<T> yes) {
  return Vec256<T>{_mm256_maskz_mov_epi16(mask.raw, yes.raw)};
}
template <typename T>
HWY_INLINE Vec256<T> IfThenElseZero(hwy::SizeTag<4> /* tag */, Mask256<T> mask,
                                    Vec256<T> yes) {
  return Vec256<T>{_mm256_maskz_mov_epi32(mask.raw, yes.raw)};
}
template <typename T>
HWY_INLINE Vec256<T> IfThenElseZero(hwy::SizeTag<8> /* tag */, Mask256<T> mask,
                                    Vec256<T> yes) {
  return Vec256<T>{_mm256_maskz_mov_epi64(mask.raw, yes.raw)};
}

}  // namespace detail

template <typename T>
HWY_API Vec256<T> IfThenElseZero(Mask256<T> mask, Vec256<T> yes) {
  return detail::IfThenElseZero(hwy::SizeTag<sizeof(T)>(), mask, yes);
}
HWY_API Vec256<float> IfThenElseZero(Mask256<float> mask, Vec256<float> yes) {
  return Vec256<float>{_mm256_maskz_mov_ps(mask.raw, yes.raw)};
}
HWY_API Vec256<double> IfThenElseZero(Mask256<double> mask,
                                      Vec256<double> yes) {
  return Vec256<double>{_mm256_maskz_mov_pd(mask.raw, yes.raw)};
}

namespace detail {

template <typename T>
HWY_INLINE Vec256<T> IfThenZeroElse(hwy::SizeTag<1> /* tag */, Mask256<T> mask,
                                    Vec256<T> no) {
  // xor_epi8/16 are missing, but we have sub, which is just as fast for u8/16.
  return Vec256<T>{_mm256_mask_sub_epi8(no.raw, mask.raw, no.raw, no.raw)};
}
template <typename T>
HWY_INLINE Vec256<T> IfThenZeroElse(hwy::SizeTag<2> /* tag */, Mask256<T> mask,
                                    Vec256<T> no) {
  return Vec256<T>{_mm256_mask_sub_epi16(no.raw, mask.raw, no.raw, no.raw)};
}
template <typename T>
HWY_INLINE Vec256<T> IfThenZeroElse(hwy::SizeTag<4> /* tag */, Mask256<T> mask,
                                    Vec256<T> no) {
  return Vec256<T>{_mm256_mask_xor_epi32(no.raw, mask.raw, no.raw, no.raw)};
}
template <typename T>
HWY_INLINE Vec256<T> IfThenZeroElse(hwy::SizeTag<8> /* tag */, Mask256<T> mask,
                                    Vec256<T> no) {
  return Vec256<T>{_mm256_mask_xor_epi64(no.raw, mask.raw, no.raw, no.raw)};
}

}  // namespace detail

template <typename T>
HWY_API Vec256<T> IfThenZeroElse(Mask256<T> mask, Vec256<T> no) {
  return detail::IfThenZeroElse(hwy::SizeTag<sizeof(T)>(), mask, no);
}
HWY_API Vec256<float> IfThenZeroElse(Mask256<float> mask, Vec256<float> no) {
  return Vec256<float>{_mm256_mask_xor_ps(no.raw, mask.raw, no.raw, no.raw)};
}
HWY_API Vec256<double> IfThenZeroElse(Mask256<double> mask, Vec256<double> no) {
  return Vec256<double>{_mm256_mask_xor_pd(no.raw, mask.raw, no.raw, no.raw)};
}

template <typename T, HWY_IF_FLOAT(T)>
HWY_API Vec256<T> ZeroIfNegative(const Vec256<T> v) {
  // AVX3 MaskFromVec only looks at the MSB
  return IfThenZeroElse(MaskFromVec(v), v);
}

// ------------------------------ Mask logical

namespace detail {

template <typename T>
HWY_INLINE Mask256<T> And(hwy::SizeTag<1> /*tag*/, const Mask256<T> a,
                          const Mask256<T> b) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return Mask256<T>{_kand_mask32(a.raw, b.raw)};
#else
  return Mask256<T>{a.raw & b.raw};
#endif
}
template <typename T>
HWY_INLINE Mask256<T> And(hwy::SizeTag<2> /*tag*/, const Mask256<T> a,
                          const Mask256<T> b) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return Mask256<T>{_kand_mask16(a.raw, b.raw)};
#else
  return Mask256<T>{a.raw & b.raw};
#endif
}
template <typename T>
HWY_INLINE Mask256<T> And(hwy::SizeTag<4> /*tag*/, const Mask256<T> a,
                          const Mask256<T> b) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return Mask256<T>{_kand_mask8(a.raw, b.raw)};
#else
  return Mask256<T>{static_cast<uint16_t>(a.raw & b.raw)};
#endif
}
template <typename T>
HWY_INLINE Mask256<T> And(hwy::SizeTag<8> /*tag*/, const Mask256<T> a,
                          const Mask256<T> b) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return Mask256<T>{_kand_mask8(a.raw, b.raw)};
#else
  return Mask256<T>{static_cast<uint8_t>(a.raw & b.raw)};
#endif
}

template <typename T>
HWY_INLINE Mask256<T> AndNot(hwy::SizeTag<1> /*tag*/, const Mask256<T> a,
                             const Mask256<T> b) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return Mask256<T>{_kandn_mask32(a.raw, b.raw)};
#else
  return Mask256<T>{~a.raw & b.raw};
#endif
}
template <typename T>
HWY_INLINE Mask256<T> AndNot(hwy::SizeTag<2> /*tag*/, const Mask256<T> a,
                             const Mask256<T> b) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return Mask256<T>{_kandn_mask16(a.raw, b.raw)};
#else
  return Mask256<T>{~a.raw & b.raw};
#endif
}
template <typename T>
HWY_INLINE Mask256<T> AndNot(hwy::SizeTag<4> /*tag*/, const Mask256<T> a,
                             const Mask256<T> b) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return Mask256<T>{_kandn_mask8(a.raw, b.raw)};
#else
  return Mask256<T>{static_cast<uint16_t>(~a.raw & b.raw)};
#endif
}
template <typename T>
HWY_INLINE Mask256<T> AndNot(hwy::SizeTag<8> /*tag*/, const Mask256<T> a,
                             const Mask256<T> b) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return Mask256<T>{_kandn_mask8(a.raw, b.raw)};
#else
  return Mask256<T>{static_cast<uint8_t>(~a.raw & b.raw)};
#endif
}

template <typename T>
HWY_INLINE Mask256<T> Or(hwy::SizeTag<1> /*tag*/, const Mask256<T> a,
                         const Mask256<T> b) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return Mask256<T>{_kor_mask32(a.raw, b.raw)};
#else
  return Mask256<T>{a.raw | b.raw};
#endif
}
template <typename T>
HWY_INLINE Mask256<T> Or(hwy::SizeTag<2> /*tag*/, const Mask256<T> a,
                         const Mask256<T> b) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return Mask256<T>{_kor_mask16(a.raw, b.raw)};
#else
  return Mask256<T>{a.raw | b.raw};
#endif
}
template <typename T>
HWY_INLINE Mask256<T> Or(hwy::SizeTag<4> /*tag*/, const Mask256<T> a,
                         const Mask256<T> b) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return Mask256<T>{_kor_mask8(a.raw, b.raw)};
#else
  return Mask256<T>{static_cast<uint16_t>(a.raw | b.raw)};
#endif
}
template <typename T>
HWY_INLINE Mask256<T> Or(hwy::SizeTag<8> /*tag*/, const Mask256<T> a,
                         const Mask256<T> b) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return Mask256<T>{_kor_mask8(a.raw, b.raw)};
#else
  return Mask256<T>{static_cast<uint8_t>(a.raw | b.raw)};
#endif
}

template <typename T>
HWY_INLINE Mask256<T> Xor(hwy::SizeTag<1> /*tag*/, const Mask256<T> a,
                          const Mask256<T> b) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return Mask256<T>{_kxor_mask32(a.raw, b.raw)};
#else
  return Mask256<T>{a.raw ^ b.raw};
#endif
}
template <typename T>
HWY_INLINE Mask256<T> Xor(hwy::SizeTag<2> /*tag*/, const Mask256<T> a,
                          const Mask256<T> b) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return Mask256<T>{_kxor_mask16(a.raw, b.raw)};
#else
  return Mask256<T>{a.raw ^ b.raw};
#endif
}
template <typename T>
HWY_INLINE Mask256<T> Xor(hwy::SizeTag<4> /*tag*/, const Mask256<T> a,
                          const Mask256<T> b) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return Mask256<T>{_kxor_mask8(a.raw, b.raw)};
#else
  return Mask256<T>{static_cast<uint16_t>(a.raw ^ b.raw)};
#endif
}
template <typename T>
HWY_INLINE Mask256<T> Xor(hwy::SizeTag<8> /*tag*/, const Mask256<T> a,
                          const Mask256<T> b) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return Mask256<T>{_kxor_mask8(a.raw, b.raw)};
#else
  return Mask256<T>{static_cast<uint8_t>(a.raw ^ b.raw)};
#endif
}

}  // namespace detail

template <typename T>
HWY_API Mask256<T> And(const Mask256<T> a, Mask256<T> b) {
  return detail::And(hwy::SizeTag<sizeof(T)>(), a, b);
}

template <typename T>
HWY_API Mask256<T> AndNot(const Mask256<T> a, Mask256<T> b) {
  return detail::AndNot(hwy::SizeTag<sizeof(T)>(), a, b);
}

template <typename T>
HWY_API Mask256<T> Or(const Mask256<T> a, Mask256<T> b) {
  return detail::Or(hwy::SizeTag<sizeof(T)>(), a, b);
}

template <typename T>
HWY_API Mask256<T> Xor(const Mask256<T> a, Mask256<T> b) {
  return detail::Xor(hwy::SizeTag<sizeof(T)>(), a, b);
}

template <typename T>
HWY_API Mask256<T> Not(const Mask256<T> m) {
  // Flip only the valid bits.
  constexpr size_t N = 32 / sizeof(T);
  return Xor(m, Mask256<T>::FromBits((1ull << N) - 1));
}

#else  // AVX2

// ------------------------------ Mask

// Mask and Vec are the same (true = FF..FF).
template <typename T>
HWY_API Mask256<T> MaskFromVec(const Vec256<T> v) {
  return Mask256<T>{v.raw};
}

template <typename T>
HWY_API Vec256<T> VecFromMask(const Mask256<T> v) {
  return Vec256<T>{v.raw};
}

template <typename T>
HWY_API Vec256<T> VecFromMask(Full256<T> /* tag */, const Mask256<T> v) {
  return Vec256<T>{v.raw};
}

// ------------------------------ IfThenElse

// mask ? yes : no
template <typename T>
HWY_API Vec256<T> IfThenElse(const Mask256<T> mask, const Vec256<T> yes,
                             const Vec256<T> no) {
  return Vec256<T>{_mm256_blendv_epi8(no.raw, yes.raw, mask.raw)};
}
HWY_API Vec256<float> IfThenElse(const Mask256<float> mask,
                                 const Vec256<float> yes,
                                 const Vec256<float> no) {
  return Vec256<float>{_mm256_blendv_ps(no.raw, yes.raw, mask.raw)};
}
HWY_API Vec256<double> IfThenElse(const Mask256<double> mask,
                                  const Vec256<double> yes,
                                  const Vec256<double> no) {
  return Vec256<double>{_mm256_blendv_pd(no.raw, yes.raw, mask.raw)};
}

// mask ? yes : 0
template <typename T>
HWY_API Vec256<T> IfThenElseZero(Mask256<T> mask, Vec256<T> yes) {
  return yes & VecFromMask(Full256<T>(), mask);
}

// mask ? 0 : no
template <typename T>
HWY_API Vec256<T> IfThenZeroElse(Mask256<T> mask, Vec256<T> no) {
  return AndNot(VecFromMask(Full256<T>(), mask), no);
}

template <typename T, HWY_IF_FLOAT(T)>
HWY_API Vec256<T> ZeroIfNegative(Vec256<T> v) {
  const auto zero = Zero(Full256<T>());
  return IfThenElse(MaskFromVec(v), zero, v);
}

// ------------------------------ Mask logical

template <typename T>
HWY_API Mask256<T> Not(const Mask256<T> m) {
  return MaskFromVec(Not(VecFromMask(Full256<T>(), m)));
}

template <typename T>
HWY_API Mask256<T> And(const Mask256<T> a, Mask256<T> b) {
  const Full256<T> d;
  return MaskFromVec(And(VecFromMask(d, a), VecFromMask(d, b)));
}

template <typename T>
HWY_API Mask256<T> AndNot(const Mask256<T> a, Mask256<T> b) {
  const Full256<T> d;
  return MaskFromVec(AndNot(VecFromMask(d, a), VecFromMask(d, b)));
}

template <typename T>
HWY_API Mask256<T> Or(const Mask256<T> a, Mask256<T> b) {
  const Full256<T> d;
  return MaskFromVec(Or(VecFromMask(d, a), VecFromMask(d, b)));
}

template <typename T>
HWY_API Mask256<T> Xor(const Mask256<T> a, Mask256<T> b) {
  const Full256<T> d;
  return MaskFromVec(Xor(VecFromMask(d, a), VecFromMask(d, b)));
}

#endif  // HWY_TARGET <= HWY_AVX3

// ================================================== COMPARE

#if HWY_TARGET <= HWY_AVX3

// Comparisons set a mask bit to 1 if the condition is true, else 0.

template <typename TFrom, typename TTo>
HWY_API Mask256<TTo> RebindMask(Full256<TTo> /*tag*/, Mask256<TFrom> m) {
  static_assert(sizeof(TFrom) == sizeof(TTo), "Must have same size");
  return Mask256<TTo>{m.raw};
}

namespace detail {

template <typename T>
HWY_INLINE Mask256<T> TestBit(hwy::SizeTag<1> /*tag*/, const Vec256<T> v,
                              const Vec256<T> bit) {
  return Mask256<T>{_mm256_test_epi8_mask(v.raw, bit.raw)};
}
template <typename T>
HWY_INLINE Mask256<T> TestBit(hwy::SizeTag<2> /*tag*/, const Vec256<T> v,
                              const Vec256<T> bit) {
  return Mask256<T>{_mm256_test_epi16_mask(v.raw, bit.raw)};
}
template <typename T>
HWY_INLINE Mask256<T> TestBit(hwy::SizeTag<4> /*tag*/, const Vec256<T> v,
                              const Vec256<T> bit) {
  return Mask256<T>{_mm256_test_epi32_mask(v.raw, bit.raw)};
}
template <typename T>
HWY_INLINE Mask256<T> TestBit(hwy::SizeTag<8> /*tag*/, const Vec256<T> v,
                              const Vec256<T> bit) {
  return Mask256<T>{_mm256_test_epi64_mask(v.raw, bit.raw)};
}

}  // namespace detail

template <typename T>
HWY_API Mask256<T> TestBit(const Vec256<T> v, const Vec256<T> bit) {
  static_assert(!hwy::IsFloat<T>(), "Only integer vectors supported");
  return detail::TestBit(hwy::SizeTag<sizeof(T)>(), v, bit);
}

// ------------------------------ Equality

template <typename T, HWY_IF_LANE_SIZE(T, 1)>
HWY_API Mask256<T> operator==(const Vec256<T> a, const Vec256<T> b) {
  return Mask256<T>{_mm256_cmpeq_epi8_mask(a.raw, b.raw)};
}
template <typename T, HWY_IF_LANE_SIZE(T, 2)>
HWY_API Mask256<T> operator==(const Vec256<T> a, const Vec256<T> b) {
  return Mask256<T>{_mm256_cmpeq_epi16_mask(a.raw, b.raw)};
}
template <typename T, HWY_IF_LANE_SIZE(T, 4)>
HWY_API Mask256<T> operator==(const Vec256<T> a, const Vec256<T> b) {
  return Mask256<T>{_mm256_cmpeq_epi32_mask(a.raw, b.raw)};
}
template <typename T, HWY_IF_LANE_SIZE(T, 8)>
HWY_API Mask256<T> operator==(const Vec256<T> a, const Vec256<T> b) {
  return Mask256<T>{_mm256_cmpeq_epi64_mask(a.raw, b.raw)};
}

HWY_API Mask256<float> operator==(Vec256<float> a, Vec256<float> b) {
  return Mask256<float>{_mm256_cmp_ps_mask(a.raw, b.raw, _CMP_EQ_OQ)};
}

HWY_API Mask256<double> operator==(Vec256<double> a, Vec256<double> b) {
  return Mask256<double>{_mm256_cmp_pd_mask(a.raw, b.raw, _CMP_EQ_OQ)};
}

// ------------------------------ Inequality

template <typename T, HWY_IF_LANE_SIZE(T, 1)>
HWY_API Mask256<T> operator!=(const Vec256<T> a, const Vec256<T> b) {
  return Mask256<T>{_mm256_cmpneq_epi8_mask(a.raw, b.raw)};
}
template <typename T, HWY_IF_LANE_SIZE(T, 2)>
HWY_API Mask256<T> operator!=(const Vec256<T> a, const Vec256<T> b) {
  return Mask256<T>{_mm256_cmpneq_epi16_mask(a.raw, b.raw)};
}
template <typename T, HWY_IF_LANE_SIZE(T, 4)>
HWY_API Mask256<T> operator!=(const Vec256<T> a, const Vec256<T> b) {
  return Mask256<T>{_mm256_cmpneq_epi32_mask(a.raw, b.raw)};
}
template <typename T, HWY_IF_LANE_SIZE(T, 8)>
HWY_API Mask256<T> operator!=(const Vec256<T> a, const Vec256<T> b) {
  return Mask256<T>{_mm256_cmpneq_epi64_mask(a.raw, b.raw)};
}

HWY_API Mask256<float> operator!=(Vec256<float> a, Vec256<float> b) {
  return Mask256<float>{_mm256_cmp_ps_mask(a.raw, b.raw, _CMP_NEQ_OQ)};
}

HWY_API Mask256<double> operator!=(Vec256<double> a, Vec256<double> b) {
  return Mask256<double>{_mm256_cmp_pd_mask(a.raw, b.raw, _CMP_NEQ_OQ)};
}

// ------------------------------ Strict inequality

// Signed/float <
HWY_API Mask256<int8_t> operator>(Vec256<int8_t> a, Vec256<int8_t> b) {
  return Mask256<int8_t>{_mm256_cmpgt_epi8_mask(a.raw, b.raw)};
}
HWY_API Mask256<int16_t> operator>(Vec256<int16_t> a, Vec256<int16_t> b) {
  return Mask256<int16_t>{_mm256_cmpgt_epi16_mask(a.raw, b.raw)};
}
HWY_API Mask256<int32_t> operator>(Vec256<int32_t> a, Vec256<int32_t> b) {
  return Mask256<int32_t>{_mm256_cmpgt_epi32_mask(a.raw, b.raw)};
}
HWY_API Mask256<int64_t> operator>(Vec256<int64_t> a, Vec256<int64_t> b) {
  return Mask256<int64_t>{_mm256_cmpgt_epi64_mask(a.raw, b.raw)};
}
HWY_API Mask256<float> operator>(Vec256<float> a, Vec256<float> b) {
  return Mask256<float>{_mm256_cmp_ps_mask(a.raw, b.raw, _CMP_GT_OQ)};
}
HWY_API Mask256<double> operator>(Vec256<double> a, Vec256<double> b) {
  return Mask256<double>{_mm256_cmp_pd_mask(a.raw, b.raw, _CMP_GT_OQ)};
}

// ------------------------------ Weak inequality

HWY_API Mask256<float> operator>=(Vec256<float> a, Vec256<float> b) {
  return Mask256<float>{_mm256_cmp_ps_mask(a.raw, b.raw, _CMP_GE_OQ)};
}
HWY_API Mask256<double> operator>=(Vec256<double> a, Vec256<double> b) {
  return Mask256<double>{_mm256_cmp_pd_mask(a.raw, b.raw, _CMP_GE_OQ)};
}

// ------------------------------ Mask

namespace detail {

template <typename T>
HWY_INLINE Mask256<T> MaskFromVec(hwy::SizeTag<1> /*tag*/, const Vec256<T> v) {
  return Mask256<T>{_mm256_movepi8_mask(v.raw)};
}
template <typename T>
HWY_INLINE Mask256<T> MaskFromVec(hwy::SizeTag<2> /*tag*/, const Vec256<T> v) {
  return Mask256<T>{_mm256_movepi16_mask(v.raw)};
}
template <typename T>
HWY_INLINE Mask256<T> MaskFromVec(hwy::SizeTag<4> /*tag*/, const Vec256<T> v) {
  return Mask256<T>{_mm256_movepi32_mask(v.raw)};
}
template <typename T>
HWY_INLINE Mask256<T> MaskFromVec(hwy::SizeTag<8> /*tag*/, const Vec256<T> v) {
  return Mask256<T>{_mm256_movepi64_mask(v.raw)};
}

}  // namespace detail

template <typename T>
HWY_API Mask256<T> MaskFromVec(const Vec256<T> v) {
  return detail::MaskFromVec(hwy::SizeTag<sizeof(T)>(), v);
}
// There do not seem to be native floating-point versions of these instructions.
HWY_API Mask256<float> MaskFromVec(const Vec256<float> v) {
  return Mask256<float>{MaskFromVec(BitCast(Full256<int32_t>(), v)).raw};
}
HWY_API Mask256<double> MaskFromVec(const Vec256<double> v) {
  return Mask256<double>{MaskFromVec(BitCast(Full256<int64_t>(), v)).raw};
}

template <typename T, HWY_IF_LANE_SIZE(T, 1)>
HWY_API Vec256<T> VecFromMask(const Mask256<T> v) {
  return Vec256<T>{_mm256_movm_epi8(v.raw)};
}

template <typename T, HWY_IF_LANE_SIZE(T, 2)>
HWY_API Vec256<T> VecFromMask(const Mask256<T> v) {
  return Vec256<T>{_mm256_movm_epi16(v.raw)};
}

template <typename T, HWY_IF_LANE_SIZE(T, 4)>
HWY_API Vec256<T> VecFromMask(const Mask256<T> v) {
  return Vec256<T>{_mm256_movm_epi32(v.raw)};
}

template <typename T, HWY_IF_LANE_SIZE(T, 8)>
HWY_API Vec256<T> VecFromMask(const Mask256<T> v) {
  return Vec256<T>{_mm256_movm_epi64(v.raw)};
}

HWY_API Vec256<float> VecFromMask(const Mask256<float> v) {
  return Vec256<float>{_mm256_castsi256_ps(_mm256_movm_epi32(v.raw))};
}

HWY_API Vec256<double> VecFromMask(const Mask256<double> v) {
  return Vec256<double>{_mm256_castsi256_pd(_mm256_movm_epi64(v.raw))};
}

template <typename T>
HWY_API Vec256<T> VecFromMask(Full256<T> /* tag */, const Mask256<T> v) {
  return VecFromMask(v);
}

#else  // AVX2

// Comparisons fill a lane with 1-bits if the condition is true, else 0.

template <typename TFrom, typename TTo>
HWY_API Mask256<TTo> RebindMask(Full256<TTo> d_to, Mask256<TFrom> m) {
  static_assert(sizeof(TFrom) == sizeof(TTo), "Must have same size");
  return MaskFromVec(BitCast(d_to, VecFromMask(Full256<TFrom>(), m)));
}

template <typename T>
HWY_API Mask256<T> TestBit(const Vec256<T> v, const Vec256<T> bit) {
  static_assert(!hwy::IsFloat<T>(), "Only integer vectors supported");
  return (v & bit) == bit;
}

// ------------------------------ Equality

template <typename T, HWY_IF_LANE_SIZE(T, 1)>
HWY_API Mask256<T> operator==(const Vec256<T> a, const Vec256<T> b) {
  return Mask256<T>{_mm256_cmpeq_epi8(a.raw, b.raw)};
}

template <typename T, HWY_IF_LANE_SIZE(T, 2)>
HWY_API Mask256<T> operator==(const Vec256<T> a, const Vec256<T> b) {
  return Mask256<T>{_mm256_cmpeq_epi16(a.raw, b.raw)};
}

template <typename T, HWY_IF_LANE_SIZE(T, 4)>
HWY_API Mask256<T> operator==(const Vec256<T> a, const Vec256<T> b) {
  return Mask256<T>{_mm256_cmpeq_epi32(a.raw, b.raw)};
}

template <typename T, HWY_IF_LANE_SIZE(T, 8)>
HWY_API Mask256<T> operator==(const Vec256<T> a, const Vec256<T> b) {
  return Mask256<T>{_mm256_cmpeq_epi64(a.raw, b.raw)};
}

HWY_API Mask256<float> operator==(const Vec256<float> a,
                                  const Vec256<float> b) {
  return Mask256<float>{_mm256_cmp_ps(a.raw, b.raw, _CMP_EQ_OQ)};
}

HWY_API Mask256<double> operator==(const Vec256<double> a,
                                   const Vec256<double> b) {
  return Mask256<double>{_mm256_cmp_pd(a.raw, b.raw, _CMP_EQ_OQ)};
}

// ------------------------------ Inequality

template <typename T, HWY_IF_NOT_FLOAT(T)>
HWY_API Mask256<T> operator!=(const Vec256<T> a, const Vec256<T> b) {
  return Not(a == b);
}

HWY_API Mask256<float> operator!=(const Vec256<float> a,
                                  const Vec256<float> b) {
  return Mask256<float>{_mm256_cmp_ps(a.raw, b.raw, _CMP_NEQ_OQ)};
}
HWY_API Mask256<double> operator!=(const Vec256<double> a,
                                   const Vec256<double> b) {
  return Mask256<double>{_mm256_cmp_pd(a.raw, b.raw, _CMP_NEQ_OQ)};
}

// ------------------------------ Strict inequality

// Pre-9.3 GCC immintrin.h uses char, which may be unsigned, causing cmpgt_epi8
// to perform an unsigned comparison instead of the intended signed. Workaround
// is to cast to an explicitly signed type. See https://godbolt.org/z/PL7Ujy
#if HWY_COMPILER_GCC != 0 && HWY_COMPILER_GCC < 930
#define HWY_AVX2_GCC_CMPGT8_WORKAROUND 1
#else
#define HWY_AVX2_GCC_CMPGT8_WORKAROUND 0
#endif

// Signed/float <
HWY_API Mask256<int8_t> operator>(Vec256<int8_t> a, Vec256<int8_t> b) {
#if HWY_AVX2_GCC_CMPGT8_WORKAROUND
  using i8x32 = signed char __attribute__((__vector_size__(32)));
  return Mask256<int8_t>{static_cast<__m256i>(reinterpret_cast<i8x32>(a.raw) >
                                              reinterpret_cast<i8x32>(b.raw))};
#else
  return Mask256<int8_t>{_mm256_cmpgt_epi8(a.raw, b.raw)};
#endif
}
HWY_API Mask256<int16_t> operator>(const Vec256<int16_t> a,
                                   const Vec256<int16_t> b) {
  return Mask256<int16_t>{_mm256_cmpgt_epi16(a.raw, b.raw)};
}
HWY_API Mask256<int32_t> operator>(const Vec256<int32_t> a,
                                   const Vec256<int32_t> b) {
  return Mask256<int32_t>{_mm256_cmpgt_epi32(a.raw, b.raw)};
}
HWY_API Mask256<int64_t> operator>(const Vec256<int64_t> a,
                                   const Vec256<int64_t> b) {
  return Mask256<int64_t>{_mm256_cmpgt_epi64(a.raw, b.raw)};
}
HWY_API Mask256<float> operator>(const Vec256<float> a, const Vec256<float> b) {
  return Mask256<float>{_mm256_cmp_ps(a.raw, b.raw, _CMP_GT_OQ)};
}
HWY_API Mask256<double> operator>(Vec256<double> a, Vec256<double> b) {
  return Mask256<double>{_mm256_cmp_pd(a.raw, b.raw, _CMP_GT_OQ)};
}

// ------------------------------ Weak inequality

HWY_API Mask256<float> operator>=(const Vec256<float> a,
                                  const Vec256<float> b) {
  return Mask256<float>{_mm256_cmp_ps(a.raw, b.raw, _CMP_GE_OQ)};
}
HWY_API Mask256<double> operator>=(const Vec256<double> a,
                                   const Vec256<double> b) {
  return Mask256<double>{_mm256_cmp_pd(a.raw, b.raw, _CMP_GE_OQ)};
}

#endif  // HWY_TARGET <= HWY_AVX3

// ------------------------------ Reversed comparisons

template <typename T>
HWY_API Mask256<T> operator<(const Vec256<T> a, const Vec256<T> b) {
  return b > a;
}

template <typename T>
HWY_API Mask256<T> operator<=(const Vec256<T> a, const Vec256<T> b) {
  return b >= a;
}

// ------------------------------ Min (Gt, IfThenElse)

// Unsigned
HWY_API Vec256<uint8_t> Min(const Vec256<uint8_t> a, const Vec256<uint8_t> b) {
  return Vec256<uint8_t>{_mm256_min_epu8(a.raw, b.raw)};
}
HWY_API Vec256<uint16_t> Min(const Vec256<uint16_t> a,
                             const Vec256<uint16_t> b) {
  return Vec256<uint16_t>{_mm256_min_epu16(a.raw, b.raw)};
}
HWY_API Vec256<uint32_t> Min(const Vec256<uint32_t> a,
                             const Vec256<uint32_t> b) {
  return Vec256<uint32_t>{_mm256_min_epu32(a.raw, b.raw)};
}
HWY_API Vec256<uint64_t> Min(const Vec256<uint64_t> a,
                             const Vec256<uint64_t> b) {
#if HWY_TARGET <= HWY_AVX3
  return Vec256<uint64_t>{_mm256_min_epu64(a.raw, b.raw)};
#else
  const Full256<uint64_t> du;
  const Full256<int64_t> di;
  const auto msb = Set(du, 1ull << 63);
  const auto gt = RebindMask(du, BitCast(di, a ^ msb) > BitCast(di, b ^ msb));
  return IfThenElse(gt, b, a);
#endif
}

// Signed
HWY_API Vec256<int8_t> Min(const Vec256<int8_t> a, const Vec256<int8_t> b) {
  return Vec256<int8_t>{_mm256_min_epi8(a.raw, b.raw)};
}
HWY_API Vec256<int16_t> Min(const Vec256<int16_t> a, const Vec256<int16_t> b) {
  return Vec256<int16_t>{_mm256_min_epi16(a.raw, b.raw)};
}
HWY_API Vec256<int32_t> Min(const Vec256<int32_t> a, const Vec256<int32_t> b) {
  return Vec256<int32_t>{_mm256_min_epi32(a.raw, b.raw)};
}
HWY_API Vec256<int64_t> Min(const Vec256<int64_t> a, const Vec256<int64_t> b) {
#if HWY_TARGET <= HWY_AVX3
  return Vec256<int64_t>{_mm256_min_epi64(a.raw, b.raw)};
#else
  return IfThenElse(a < b, a, b);
#endif
}

// Float
HWY_API Vec256<float> Min(const Vec256<float> a, const Vec256<float> b) {
  return Vec256<float>{_mm256_min_ps(a.raw, b.raw)};
}
HWY_API Vec256<double> Min(const Vec256<double> a, const Vec256<double> b) {
  return Vec256<double>{_mm256_min_pd(a.raw, b.raw)};
}

// ------------------------------ Max (Gt, IfThenElse)

// Unsigned
HWY_API Vec256<uint8_t> Max(const Vec256<uint8_t> a, const Vec256<uint8_t> b) {
  return Vec256<uint8_t>{_mm256_max_epu8(a.raw, b.raw)};
}
HWY_API Vec256<uint16_t> Max(const Vec256<uint16_t> a,
                             const Vec256<uint16_t> b) {
  return Vec256<uint16_t>{_mm256_max_epu16(a.raw, b.raw)};
}
HWY_API Vec256<uint32_t> Max(const Vec256<uint32_t> a,
                             const Vec256<uint32_t> b) {
  return Vec256<uint32_t>{_mm256_max_epu32(a.raw, b.raw)};
}
HWY_API Vec256<uint64_t> Max(const Vec256<uint64_t> a,
                             const Vec256<uint64_t> b) {
#if HWY_TARGET <= HWY_AVX3
  return Vec256<uint64_t>{_mm256_max_epu64(a.raw, b.raw)};
#else
  const Full256<uint64_t> du;
  const Full256<int64_t> di;
  const auto msb = Set(du, 1ull << 63);
  const auto gt = RebindMask(du, BitCast(di, a ^ msb) > BitCast(di, b ^ msb));
  return IfThenElse(gt, a, b);
#endif
}

// Signed
HWY_API Vec256<int8_t> Max(const Vec256<int8_t> a, const Vec256<int8_t> b) {
  return Vec256<int8_t>{_mm256_max_epi8(a.raw, b.raw)};
}
HWY_API Vec256<int16_t> Max(const Vec256<int16_t> a, const Vec256<int16_t> b) {
  return Vec256<int16_t>{_mm256_max_epi16(a.raw, b.raw)};
}
HWY_API Vec256<int32_t> Max(const Vec256<int32_t> a, const Vec256<int32_t> b) {
  return Vec256<int32_t>{_mm256_max_epi32(a.raw, b.raw)};
}
HWY_API Vec256<int64_t> Max(const Vec256<int64_t> a, const Vec256<int64_t> b) {
#if HWY_TARGET <= HWY_AVX3
  return Vec256<int64_t>{_mm256_max_epi64(a.raw, b.raw)};
#else
  return IfThenElse(a < b, b, a);
#endif
}

// Float
HWY_API Vec256<float> Max(const Vec256<float> a, const Vec256<float> b) {
  return Vec256<float>{_mm256_max_ps(a.raw, b.raw)};
}
HWY_API Vec256<double> Max(const Vec256<double> a, const Vec256<double> b) {
  return Vec256<double>{_mm256_max_pd(a.raw, b.raw)};
}

// ------------------------------ FirstN (Iota, Lt)

template <typename T>
HWY_API Mask256<T> FirstN(const Full256<T> d, size_t n) {
#if HWY_TARGET <= HWY_AVX3
  (void)d;
#if HWY_ARCH_X86_64
  return Mask256<T>::FromBits(_bzhi_u64(~0ull, n));
#else
  return Mask256<T>::FromBits(_bzhi_u32(~0u, static_cast<uint32_t>(n)));
#endif  // HWY_ARCH_X86_64
#else
  const RebindToSigned<decltype(d)> di;  // Signed comparisons are cheaper.
  return RebindMask(d, Iota(di, 0) < Set(di, static_cast<MakeSigned<T>>(n)));
#endif
}

// ================================================== ARITHMETIC

// ------------------------------ Addition

// Unsigned
HWY_API Vec256<uint8_t> operator+(const Vec256<uint8_t> a,
                                  const Vec256<uint8_t> b) {
  return Vec256<uint8_t>{_mm256_add_epi8(a.raw, b.raw)};
}
HWY_API Vec256<uint16_t> operator+(const Vec256<uint16_t> a,
                                   const Vec256<uint16_t> b) {
  return Vec256<uint16_t>{_mm256_add_epi16(a.raw, b.raw)};
}
HWY_API Vec256<uint32_t> operator+(const Vec256<uint32_t> a,
                                   const Vec256<uint32_t> b) {
  return Vec256<uint32_t>{_mm256_add_epi32(a.raw, b.raw)};
}
HWY_API Vec256<uint64_t> operator+(const Vec256<uint64_t> a,
                                   const Vec256<uint64_t> b) {
  return Vec256<uint64_t>{_mm256_add_epi64(a.raw, b.raw)};
}

// Signed
HWY_API Vec256<int8_t> operator+(const Vec256<int8_t> a,
                                 const Vec256<int8_t> b) {
  return Vec256<int8_t>{_mm256_add_epi8(a.raw, b.raw)};
}
HWY_API Vec256<int16_t> operator+(const Vec256<int16_t> a,
                                  const Vec256<int16_t> b) {
  return Vec256<int16_t>{_mm256_add_epi16(a.raw, b.raw)};
}
HWY_API Vec256<int32_t> operator+(const Vec256<int32_t> a,
                                  const Vec256<int32_t> b) {
  return Vec256<int32_t>{_mm256_add_epi32(a.raw, b.raw)};
}
HWY_API Vec256<int64_t> operator+(const Vec256<int64_t> a,
                                  const Vec256<int64_t> b) {
  return Vec256<int64_t>{_mm256_add_epi64(a.raw, b.raw)};
}

// Float
HWY_API Vec256<float> operator+(const Vec256<float> a, const Vec256<float> b) {
  return Vec256<float>{_mm256_add_ps(a.raw, b.raw)};
}
HWY_API Vec256<double> operator+(const Vec256<double> a,
                                 const Vec256<double> b) {
  return Vec256<double>{_mm256_add_pd(a.raw, b.raw)};
}

// ------------------------------ Subtraction

// Unsigned
HWY_API Vec256<uint8_t> operator-(const Vec256<uint8_t> a,
                                  const Vec256<uint8_t> b) {
  return Vec256<uint8_t>{_mm256_sub_epi8(a.raw, b.raw)};
}
HWY_API Vec256<uint16_t> operator-(const Vec256<uint16_t> a,
                                   const Vec256<uint16_t> b) {
  return Vec256<uint16_t>{_mm256_sub_epi16(a.raw, b.raw)};
}
HWY_API Vec256<uint32_t> operator-(const Vec256<uint32_t> a,
                                   const Vec256<uint32_t> b) {
  return Vec256<uint32_t>{_mm256_sub_epi32(a.raw, b.raw)};
}
HWY_API Vec256<uint64_t> operator-(const Vec256<uint64_t> a,
                                   const Vec256<uint64_t> b) {
  return Vec256<uint64_t>{_mm256_sub_epi64(a.raw, b.raw)};
}

// Signed
HWY_API Vec256<int8_t> operator-(const Vec256<int8_t> a,
                                 const Vec256<int8_t> b) {
  return Vec256<int8_t>{_mm256_sub_epi8(a.raw, b.raw)};
}
HWY_API Vec256<int16_t> operator-(const Vec256<int16_t> a,
                                  const Vec256<int16_t> b) {
  return Vec256<int16_t>{_mm256_sub_epi16(a.raw, b.raw)};
}
HWY_API Vec256<int32_t> operator-(const Vec256<int32_t> a,
                                  const Vec256<int32_t> b) {
  return Vec256<int32_t>{_mm256_sub_epi32(a.raw, b.raw)};
}
HWY_API Vec256<int64_t> operator-(const Vec256<int64_t> a,
                                  const Vec256<int64_t> b) {
  return Vec256<int64_t>{_mm256_sub_epi64(a.raw, b.raw)};
}

// Float
HWY_API Vec256<float> operator-(const Vec256<float> a, const Vec256<float> b) {
  return Vec256<float>{_mm256_sub_ps(a.raw, b.raw)};
}
HWY_API Vec256<double> operator-(const Vec256<double> a,
                                 const Vec256<double> b) {
  return Vec256<double>{_mm256_sub_pd(a.raw, b.raw)};
}

// ------------------------------ Saturating addition

// Returns a + b clamped to the destination range.

// Unsigned
HWY_API Vec256<uint8_t> SaturatedAdd(const Vec256<uint8_t> a,
                                     const Vec256<uint8_t> b) {
  return Vec256<uint8_t>{_mm256_adds_epu8(a.raw, b.raw)};
}
HWY_API Vec256<uint16_t> SaturatedAdd(const Vec256<uint16_t> a,
                                      const Vec256<uint16_t> b) {
  return Vec256<uint16_t>{_mm256_adds_epu16(a.raw, b.raw)};
}

// Signed
HWY_API Vec256<int8_t> SaturatedAdd(const Vec256<int8_t> a,
                                    const Vec256<int8_t> b) {
  return Vec256<int8_t>{_mm256_adds_epi8(a.raw, b.raw)};
}
HWY_API Vec256<int16_t> SaturatedAdd(const Vec256<int16_t> a,
                                     const Vec256<int16_t> b) {
  return Vec256<int16_t>{_mm256_adds_epi16(a.raw, b.raw)};
}

// ------------------------------ Saturating subtraction

// Returns a - b clamped to the destination range.

// Unsigned
HWY_API Vec256<uint8_t> SaturatedSub(const Vec256<uint8_t> a,
                                     const Vec256<uint8_t> b) {
  return Vec256<uint8_t>{_mm256_subs_epu8(a.raw, b.raw)};
}
HWY_API Vec256<uint16_t> SaturatedSub(const Vec256<uint16_t> a,
                                      const Vec256<uint16_t> b) {
  return Vec256<uint16_t>{_mm256_subs_epu16(a.raw, b.raw)};
}

// Signed
HWY_API Vec256<int8_t> SaturatedSub(const Vec256<int8_t> a,
                                    const Vec256<int8_t> b) {
  return Vec256<int8_t>{_mm256_subs_epi8(a.raw, b.raw)};
}
HWY_API Vec256<int16_t> SaturatedSub(const Vec256<int16_t> a,
                                     const Vec256<int16_t> b) {
  return Vec256<int16_t>{_mm256_subs_epi16(a.raw, b.raw)};
}

// ------------------------------ Average

// Returns (a + b + 1) / 2

// Unsigned
HWY_API Vec256<uint8_t> AverageRound(const Vec256<uint8_t> a,
                                     const Vec256<uint8_t> b) {
  return Vec256<uint8_t>{_mm256_avg_epu8(a.raw, b.raw)};
}
HWY_API Vec256<uint16_t> AverageRound(const Vec256<uint16_t> a,
                                      const Vec256<uint16_t> b) {
  return Vec256<uint16_t>{_mm256_avg_epu16(a.raw, b.raw)};
}

// ------------------------------ Abs (Sub)

// Returns absolute value, except that LimitsMin() maps to LimitsMax() + 1.
HWY_API Vec256<int8_t> Abs(const Vec256<int8_t> v) {
#if HWY_COMPILER_MSVC
  // Workaround for incorrect codegen? (wrong result)
  const auto zero = Zero(Full256<int8_t>());
  return Vec256<int8_t>{_mm256_max_epi8(v.raw, (zero - v).raw)};
#else
  return Vec256<int8_t>{_mm256_abs_epi8(v.raw)};
#endif
}
HWY_API Vec256<int16_t> Abs(const Vec256<int16_t> v) {
  return Vec256<int16_t>{_mm256_abs_epi16(v.raw)};
}
HWY_API Vec256<int32_t> Abs(const Vec256<int32_t> v) {
  return Vec256<int32_t>{_mm256_abs_epi32(v.raw)};
}
// i64 is implemented after BroadcastSignBit.

HWY_API Vec256<float> Abs(const Vec256<float> v) {
  const Vec256<int32_t> mask{_mm256_set1_epi32(0x7FFFFFFF)};
  return v & BitCast(Full256<float>(), mask);
}
HWY_API Vec256<double> Abs(const Vec256<double> v) {
  const Vec256<int64_t> mask{_mm256_set1_epi64x(0x7FFFFFFFFFFFFFFFLL)};
  return v & BitCast(Full256<double>(), mask);
}

// ------------------------------ Integer multiplication

// Unsigned
HWY_API Vec256<uint16_t> operator*(const Vec256<uint16_t> a,
                                   const Vec256<uint16_t> b) {
  return Vec256<uint16_t>{_mm256_mullo_epi16(a.raw, b.raw)};
}
HWY_API Vec256<uint32_t> operator*(const Vec256<uint32_t> a,
                                   const Vec256<uint32_t> b) {
  return Vec256<uint32_t>{_mm256_mullo_epi32(a.raw, b.raw)};
}

// Signed
HWY_API Vec256<int16_t> operator*(const Vec256<int16_t> a,
                                  const Vec256<int16_t> b) {
  return Vec256<int16_t>{_mm256_mullo_epi16(a.raw, b.raw)};
}
HWY_API Vec256<int32_t> operator*(const Vec256<int32_t> a,
                                  const Vec256<int32_t> b) {
  return Vec256<int32_t>{_mm256_mullo_epi32(a.raw, b.raw)};
}

// Returns the upper 16 bits of a * b in each lane.
HWY_API Vec256<uint16_t> MulHigh(const Vec256<uint16_t> a,
                                 const Vec256<uint16_t> b) {
  return Vec256<uint16_t>{_mm256_mulhi_epu16(a.raw, b.raw)};
}
HWY_API Vec256<int16_t> MulHigh(const Vec256<int16_t> a,
                                const Vec256<int16_t> b) {
  return Vec256<int16_t>{_mm256_mulhi_epi16(a.raw, b.raw)};
}

// Multiplies even lanes (0, 2 ..) and places the double-wide result into
// even and the upper half into its odd neighbor lane.
HWY_API Vec256<int64_t> MulEven(const Vec256<int32_t> a,
                                const Vec256<int32_t> b) {
  return Vec256<int64_t>{_mm256_mul_epi32(a.raw, b.raw)};
}
HWY_API Vec256<uint64_t> MulEven(const Vec256<uint32_t> a,
                                 const Vec256<uint32_t> b) {
  return Vec256<uint64_t>{_mm256_mul_epu32(a.raw, b.raw)};
}

// ------------------------------ ShiftLeft

template <int kBits>
HWY_API Vec256<uint16_t> ShiftLeft(const Vec256<uint16_t> v) {
  return Vec256<uint16_t>{_mm256_slli_epi16(v.raw, kBits)};
}

template <int kBits>
HWY_API Vec256<uint32_t> ShiftLeft(const Vec256<uint32_t> v) {
  return Vec256<uint32_t>{_mm256_slli_epi32(v.raw, kBits)};
}

template <int kBits>
HWY_API Vec256<uint64_t> ShiftLeft(const Vec256<uint64_t> v) {
  return Vec256<uint64_t>{_mm256_slli_epi64(v.raw, kBits)};
}

template <int kBits>
HWY_API Vec256<int16_t> ShiftLeft(const Vec256<int16_t> v) {
  return Vec256<int16_t>{_mm256_slli_epi16(v.raw, kBits)};
}

template <int kBits>
HWY_API Vec256<int32_t> ShiftLeft(const Vec256<int32_t> v) {
  return Vec256<int32_t>{_mm256_slli_epi32(v.raw, kBits)};
}

template <int kBits>
HWY_API Vec256<int64_t> ShiftLeft(const Vec256<int64_t> v) {
  return Vec256<int64_t>{_mm256_slli_epi64(v.raw, kBits)};
}

template <int kBits, typename T, HWY_IF_LANE_SIZE(T, 1)>
HWY_API Vec256<T> ShiftLeft(const Vec256<T> v) {
  const Full256<T> d8;
  const RepartitionToWide<decltype(d8)> d16;
  const auto shifted = BitCast(d8, ShiftLeft<kBits>(BitCast(d16, v)));
  return kBits == 1
             ? (v + v)
             : (shifted & Set(d8, static_cast<T>((0xFF << kBits) & 0xFF)));
}

// ------------------------------ ShiftRight

template <int kBits>
HWY_API Vec256<uint16_t> ShiftRight(const Vec256<uint16_t> v) {
  return Vec256<uint16_t>{_mm256_srli_epi16(v.raw, kBits)};
}

template <int kBits>
HWY_API Vec256<uint32_t> ShiftRight(const Vec256<uint32_t> v) {
  return Vec256<uint32_t>{_mm256_srli_epi32(v.raw, kBits)};
}

template <int kBits>
HWY_API Vec256<uint64_t> ShiftRight(const Vec256<uint64_t> v) {
  return Vec256<uint64_t>{_mm256_srli_epi64(v.raw, kBits)};
}

template <int kBits>
HWY_API Vec256<uint8_t> ShiftRight(const Vec256<uint8_t> v) {
  const Full256<uint8_t> d8;
  // Use raw instead of BitCast to support N=1.
  const Vec256<uint8_t> shifted{ShiftRight<kBits>(Vec256<uint16_t>{v.raw}).raw};
  return shifted & Set(d8, 0xFF >> kBits);
}

template <int kBits>
HWY_API Vec256<int16_t> ShiftRight(const Vec256<int16_t> v) {
  return Vec256<int16_t>{_mm256_srai_epi16(v.raw, kBits)};
}

template <int kBits>
HWY_API Vec256<int32_t> ShiftRight(const Vec256<int32_t> v) {
  return Vec256<int32_t>{_mm256_srai_epi32(v.raw, kBits)};
}

template <int kBits>
HWY_API Vec256<int8_t> ShiftRight(const Vec256<int8_t> v) {
  const Full256<int8_t> di;
  const Full256<uint8_t> du;
  const auto shifted = BitCast(di, ShiftRight<kBits>(BitCast(du, v)));
  const auto shifted_sign = BitCast(di, Set(du, 0x80 >> kBits));
  return (shifted ^ shifted_sign) - shifted_sign;
}

// i64 is implemented after BroadcastSignBit.

// ------------------------------ BroadcastSignBit (ShiftRight, compare, mask)

HWY_API Vec256<int8_t> BroadcastSignBit(const Vec256<int8_t> v) {
  return VecFromMask(v < Zero(Full256<int8_t>()));
}

HWY_API Vec256<int16_t> BroadcastSignBit(const Vec256<int16_t> v) {
  return ShiftRight<15>(v);
}

HWY_API Vec256<int32_t> BroadcastSignBit(const Vec256<int32_t> v) {
  return ShiftRight<31>(v);
}

HWY_API Vec256<int64_t> BroadcastSignBit(const Vec256<int64_t> v) {
#if HWY_TARGET == HWY_AVX2
  return VecFromMask(v < Zero(Full256<int64_t>()));
#else
  return Vec256<int64_t>{_mm256_srai_epi64(v.raw, 63)};
#endif
}

template <int kBits>
HWY_API Vec256<int64_t> ShiftRight(const Vec256<int64_t> v) {
#if HWY_TARGET <= HWY_AVX3
  return Vec256<int64_t>{_mm256_srai_epi64(v.raw, kBits)};
#else
  const Full256<int64_t> di;
  const Full256<uint64_t> du;
  const auto right = BitCast(di, ShiftRight<kBits>(BitCast(du, v)));
  const auto sign = ShiftLeft<64 - kBits>(BroadcastSignBit(v));
  return right | sign;
#endif
}

HWY_API Vec256<int64_t> Abs(const Vec256<int64_t> v) {
#if HWY_TARGET <= HWY_AVX3
  return Vec256<int64_t>{_mm256_abs_epi64(v.raw)};
#else
  const auto zero = Zero(Full256<int64_t>());
  return IfThenElse(MaskFromVec(BroadcastSignBit(v)), zero - v, v);
#endif
}

// ------------------------------ ShiftLeftSame

HWY_API Vec256<uint16_t> ShiftLeftSame(const Vec256<uint16_t> v,
                                       const int bits) {
  return Vec256<uint16_t>{_mm256_sll_epi16(v.raw, _mm_cvtsi32_si128(bits))};
}
HWY_API Vec256<uint32_t> ShiftLeftSame(const Vec256<uint32_t> v,
                                       const int bits) {
  return Vec256<uint32_t>{_mm256_sll_epi32(v.raw, _mm_cvtsi32_si128(bits))};
}
HWY_API Vec256<uint64_t> ShiftLeftSame(const Vec256<uint64_t> v,
                                       const int bits) {
  return Vec256<uint64_t>{_mm256_sll_epi64(v.raw, _mm_cvtsi32_si128(bits))};
}

HWY_API Vec256<int16_t> ShiftLeftSame(const Vec256<int16_t> v, const int bits) {
  return Vec256<int16_t>{_mm256_sll_epi16(v.raw, _mm_cvtsi32_si128(bits))};
}

HWY_API Vec256<int32_t> ShiftLeftSame(const Vec256<int32_t> v, const int bits) {
  return Vec256<int32_t>{_mm256_sll_epi32(v.raw, _mm_cvtsi32_si128(bits))};
}

HWY_API Vec256<int64_t> ShiftLeftSame(const Vec256<int64_t> v, const int bits) {
  return Vec256<int64_t>{_mm256_sll_epi64(v.raw, _mm_cvtsi32_si128(bits))};
}

template <typename T, HWY_IF_LANE_SIZE(T, 1)>
HWY_API Vec256<T> ShiftLeftSame(const Vec256<T> v, const int bits) {
  const Full256<T> d8;
  const RepartitionToWide<decltype(d8)> d16;
  const auto shifted = BitCast(d8, ShiftLeftSame(BitCast(d16, v), bits));
  return shifted & Set(d8, static_cast<T>((0xFF << bits) & 0xFF));
}

// ------------------------------ ShiftRightSame (BroadcastSignBit)

HWY_API Vec256<uint16_t> ShiftRightSame(const Vec256<uint16_t> v,
                                        const int bits) {
  return Vec256<uint16_t>{_mm256_srl_epi16(v.raw, _mm_cvtsi32_si128(bits))};
}
HWY_API Vec256<uint32_t> ShiftRightSame(const Vec256<uint32_t> v,
                                        const int bits) {
  return Vec256<uint32_t>{_mm256_srl_epi32(v.raw, _mm_cvtsi32_si128(bits))};
}
HWY_API Vec256<uint64_t> ShiftRightSame(const Vec256<uint64_t> v,
                                        const int bits) {
  return Vec256<uint64_t>{_mm256_srl_epi64(v.raw, _mm_cvtsi32_si128(bits))};
}

HWY_API Vec256<uint8_t> ShiftRightSame(Vec256<uint8_t> v, const int bits) {
  const Full256<uint8_t> d8;
  const RepartitionToWide<decltype(d8)> d16;
  const auto shifted = BitCast(d8, ShiftRightSame(BitCast(d16, v), bits));
  return shifted & Set(d8, static_cast<uint8_t>(0xFF >> bits));
}

HWY_API Vec256<int16_t> ShiftRightSame(const Vec256<int16_t> v,
                                       const int bits) {
  return Vec256<int16_t>{_mm256_sra_epi16(v.raw, _mm_cvtsi32_si128(bits))};
}

HWY_API Vec256<int32_t> ShiftRightSame(const Vec256<int32_t> v,
                                       const int bits) {
  return Vec256<int32_t>{_mm256_sra_epi32(v.raw, _mm_cvtsi32_si128(bits))};
}
HWY_API Vec256<int64_t> ShiftRightSame(const Vec256<int64_t> v,
                                       const int bits) {
#if HWY_TARGET <= HWY_AVX3
  return Vec256<int64_t>{_mm256_sra_epi64(v.raw, _mm_cvtsi32_si128(bits))};
#else
  const Full256<int64_t> di;
  const Full256<uint64_t> du;
  const auto right = BitCast(di, ShiftRightSame(BitCast(du, v), bits));
  const auto sign = ShiftLeftSame(BroadcastSignBit(v), 64 - bits);
  return right | sign;
#endif
}

HWY_API Vec256<int8_t> ShiftRightSame(Vec256<int8_t> v, const int bits) {
  const Full256<int8_t> di;
  const Full256<uint8_t> du;
  const auto shifted = BitCast(di, ShiftRightSame(BitCast(du, v), bits));
  const auto shifted_sign =
      BitCast(di, Set(du, static_cast<uint8_t>(0x80 >> bits)));
  return (shifted ^ shifted_sign) - shifted_sign;
}

// ------------------------------ Neg (Xor, Sub)

template <typename T, HWY_IF_FLOAT(T)>
HWY_API Vec256<T> Neg(const Vec256<T> v) {
  return Xor(v, SignBit(Full256<T>()));
}

template <typename T, HWY_IF_NOT_FLOAT(T)>
HWY_API Vec256<T> Neg(const Vec256<T> v) {
  return Zero(Full256<T>()) - v;
}

// ------------------------------ Floating-point mul / div

HWY_API Vec256<float> operator*(const Vec256<float> a, const Vec256<float> b) {
  return Vec256<float>{_mm256_mul_ps(a.raw, b.raw)};
}
HWY_API Vec256<double> operator*(const Vec256<double> a,
                                 const Vec256<double> b) {
  return Vec256<double>{_mm256_mul_pd(a.raw, b.raw)};
}

HWY_API Vec256<float> operator/(const Vec256<float> a, const Vec256<float> b) {
  return Vec256<float>{_mm256_div_ps(a.raw, b.raw)};
}
HWY_API Vec256<double> operator/(const Vec256<double> a,
                                 const Vec256<double> b) {
  return Vec256<double>{_mm256_div_pd(a.raw, b.raw)};
}

// Approximate reciprocal
HWY_API Vec256<float> ApproximateReciprocal(const Vec256<float> v) {
  return Vec256<float>{_mm256_rcp_ps(v.raw)};
}

// Absolute value of difference.
HWY_API Vec256<float> AbsDiff(const Vec256<float> a, const Vec256<float> b) {
  return Abs(a - b);
}

// ------------------------------ Floating-point multiply-add variants

// Returns mul * x + add
HWY_API Vec256<float> MulAdd(const Vec256<float> mul, const Vec256<float> x,
                             const Vec256<float> add) {
#ifdef HWY_DISABLE_BMI2_FMA
  return mul * x + add;
#else
  return Vec256<float>{_mm256_fmadd_ps(mul.raw, x.raw, add.raw)};
#endif
}
HWY_API Vec256<double> MulAdd(const Vec256<double> mul, const Vec256<double> x,
                              const Vec256<double> add) {
#ifdef HWY_DISABLE_BMI2_FMA
  return mul * x + add;
#else
  return Vec256<double>{_mm256_fmadd_pd(mul.raw, x.raw, add.raw)};
#endif
}

// Returns add - mul * x
HWY_API Vec256<float> NegMulAdd(const Vec256<float> mul, const Vec256<float> x,
                                const Vec256<float> add) {
#ifdef HWY_DISABLE_BMI2_FMA
  return add - mul * x;
#else
  return Vec256<float>{_mm256_fnmadd_ps(mul.raw, x.raw, add.raw)};
#endif
}
HWY_API Vec256<double> NegMulAdd(const Vec256<double> mul,
                                 const Vec256<double> x,
                                 const Vec256<double> add) {
#ifdef HWY_DISABLE_BMI2_FMA
  return add - mul * x;
#else
  return Vec256<double>{_mm256_fnmadd_pd(mul.raw, x.raw, add.raw)};
#endif
}

// Returns mul * x - sub
HWY_API Vec256<float> MulSub(const Vec256<float> mul, const Vec256<float> x,
                             const Vec256<float> sub) {
#ifdef HWY_DISABLE_BMI2_FMA
  return mul * x - sub;
#else
  return Vec256<float>{_mm256_fmsub_ps(mul.raw, x.raw, sub.raw)};
#endif
}
HWY_API Vec256<double> MulSub(const Vec256<double> mul, const Vec256<double> x,
                              const Vec256<double> sub) {
#ifdef HWY_DISABLE_BMI2_FMA
  return mul * x - sub;
#else
  return Vec256<double>{_mm256_fmsub_pd(mul.raw, x.raw, sub.raw)};
#endif
}

// Returns -mul * x - sub
HWY_API Vec256<float> NegMulSub(const Vec256<float> mul, const Vec256<float> x,
                                const Vec256<float> sub) {
#ifdef HWY_DISABLE_BMI2_FMA
  return Neg(mul * x) - sub;
#else
  return Vec256<float>{_mm256_fnmsub_ps(mul.raw, x.raw, sub.raw)};
#endif
}
HWY_API Vec256<double> NegMulSub(const Vec256<double> mul,
                                 const Vec256<double> x,
                                 const Vec256<double> sub) {
#ifdef HWY_DISABLE_BMI2_FMA
  return Neg(mul * x) - sub;
#else
  return Vec256<double>{_mm256_fnmsub_pd(mul.raw, x.raw, sub.raw)};
#endif
}

// ------------------------------ Floating-point square root

// Full precision square root
HWY_API Vec256<float> Sqrt(const Vec256<float> v) {
  return Vec256<float>{_mm256_sqrt_ps(v.raw)};
}
HWY_API Vec256<double> Sqrt(const Vec256<double> v) {
  return Vec256<double>{_mm256_sqrt_pd(v.raw)};
}

// Approximate reciprocal square root
HWY_API Vec256<float> ApproximateReciprocalSqrt(const Vec256<float> v) {
  return Vec256<float>{_mm256_rsqrt_ps(v.raw)};
}

// ------------------------------ Floating-point rounding

// Toward nearest integer, tie to even
HWY_API Vec256<float> Round(const Vec256<float> v) {
  return Vec256<float>{
      _mm256_round_ps(v.raw, _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC)};
}
HWY_API Vec256<double> Round(const Vec256<double> v) {
  return Vec256<double>{
      _mm256_round_pd(v.raw, _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC)};
}

// Toward zero, aka truncate
HWY_API Vec256<float> Trunc(const Vec256<float> v) {
  return Vec256<float>{
      _mm256_round_ps(v.raw, _MM_FROUND_TO_ZERO | _MM_FROUND_NO_EXC)};
}
HWY_API Vec256<double> Trunc(const Vec256<double> v) {
  return Vec256<double>{
      _mm256_round_pd(v.raw, _MM_FROUND_TO_ZERO | _MM_FROUND_NO_EXC)};
}

// Toward +infinity, aka ceiling
HWY_API Vec256<float> Ceil(const Vec256<float> v) {
  return Vec256<float>{
      _mm256_round_ps(v.raw, _MM_FROUND_TO_POS_INF | _MM_FROUND_NO_EXC)};
}
HWY_API Vec256<double> Ceil(const Vec256<double> v) {
  return Vec256<double>{
      _mm256_round_pd(v.raw, _MM_FROUND_TO_POS_INF | _MM_FROUND_NO_EXC)};
}

// Toward -infinity, aka floor
HWY_API Vec256<float> Floor(const Vec256<float> v) {
  return Vec256<float>{
      _mm256_round_ps(v.raw, _MM_FROUND_TO_NEG_INF | _MM_FROUND_NO_EXC)};
}
HWY_API Vec256<double> Floor(const Vec256<double> v) {
  return Vec256<double>{
      _mm256_round_pd(v.raw, _MM_FROUND_TO_NEG_INF | _MM_FROUND_NO_EXC)};
}

// ================================================== MEMORY

// ------------------------------ Load

template <typename T>
HWY_API Vec256<T> Load(Full256<T> /* tag */, const T* HWY_RESTRICT aligned) {
  return Vec256<T>{
      _mm256_load_si256(reinterpret_cast<const __m256i*>(aligned))};
}
HWY_API Vec256<float> Load(Full256<float> /* tag */,
                           const float* HWY_RESTRICT aligned) {
  return Vec256<float>{_mm256_load_ps(aligned)};
}
HWY_API Vec256<double> Load(Full256<double> /* tag */,
                            const double* HWY_RESTRICT aligned) {
  return Vec256<double>{_mm256_load_pd(aligned)};
}

template <typename T>
HWY_API Vec256<T> LoadU(Full256<T> /* tag */, const T* HWY_RESTRICT p) {
  return Vec256<T>{_mm256_loadu_si256(reinterpret_cast<const __m256i*>(p))};
}
HWY_API Vec256<float> LoadU(Full256<float> /* tag */,
                            const float* HWY_RESTRICT p) {
  return Vec256<float>{_mm256_loadu_ps(p)};
}
HWY_API Vec256<double> LoadU(Full256<double> /* tag */,
                             const double* HWY_RESTRICT p) {
  return Vec256<double>{_mm256_loadu_pd(p)};
}

// ------------------------------ MaskedLoad

#if HWY_TARGET <= HWY_AVX3

template <typename T, HWY_IF_LANE_SIZE(T, 4)>
HWY_API Vec256<T> MaskedLoad(Mask256<T> m, Full256<T> /* tag */,
                             const T* HWY_RESTRICT aligned) {
  return Vec256<T>{_mm256_maskz_load_epi32(m.raw, aligned)};
}

template <typename T, HWY_IF_LANE_SIZE(T, 8)>
HWY_API Vec256<T> MaskedLoad(Mask256<T> m, Full256<T> /* tag */,
                             const T* HWY_RESTRICT aligned) {
  return Vec256<T>{_mm256_maskz_load_epi64(m.raw, aligned)};
}

HWY_API Vec256<float> MaskedLoad(Mask256<float> m, Full256<float> /* tag */,
                                 const float* HWY_RESTRICT aligned) {
  return Vec256<float>{_mm256_maskz_load_ps(m.raw, aligned)};
}

HWY_API Vec256<double> MaskedLoad(Mask256<double> m, Full256<double> /* tag */,
                                  const double* HWY_RESTRICT aligned) {
  return Vec256<double>{_mm256_maskz_load_pd(m.raw, aligned)};
}

// There is no load_epi8/16, so use loadu instead.
template <typename T, HWY_IF_LANE_SIZE(T, 1)>
HWY_API Vec256<T> MaskedLoad(Mask256<T> m, Full256<T> /* tag */,
                             const T* HWY_RESTRICT aligned) {
  return Vec256<T>{_mm256_maskz_loadu_epi8(m.raw, aligned)};
}

template <typename T, HWY_IF_LANE_SIZE(T, 2)>
HWY_API Vec256<T> MaskedLoad(Mask256<T> m, Full256<T> /* tag */,
                             const T* HWY_RESTRICT aligned) {
  return Vec256<T>{_mm256_maskz_loadu_epi16(m.raw, aligned)};
}

#endif // else: fallback defined in x86_128-inl.h

// ------------------------------ LoadDup128

// Loads 128 bit and duplicates into both 128-bit halves. This avoids the
// 3-cycle cost of moving data between 128-bit halves and avoids port 5.
template <typename T>
HWY_API Vec256<T> LoadDup128(Full256<T> /* tag */, const T* HWY_RESTRICT p) {
#if HWY_LOADDUP_ASM
  __m256i out;
  asm("vbroadcasti128 %1, %[reg]" : [ reg ] "=x"(out) : "m"(p[0]));
  return Vec256<T>{out};
#elif HWY_COMPILER_MSVC && !HWY_COMPILER_CLANG
  // Workaround for incorrect results with _mm256_broadcastsi128_si256. Note
  // that MSVC also lacks _mm256_zextsi128_si256, but cast (which leaves the
  // upper half undefined) is fine because we're overwriting that anyway.
  const __m128i v128 = LoadU(Full128<T>(), p).raw;
  return Vec256<T>{
      _mm256_inserti128_si256(_mm256_castsi128_si256(v128), v128, 1)};
#else
  return Vec256<T>{_mm256_broadcastsi128_si256(LoadU(Full128<T>(), p).raw)};
#endif
}
HWY_API Vec256<float> LoadDup128(Full256<float> /* tag */,
                                 const float* const HWY_RESTRICT p) {
#if HWY_LOADDUP_ASM
  __m256 out;
  asm("vbroadcastf128 %1, %[reg]" : [ reg ] "=x"(out) : "m"(p[0]));
  return Vec256<float>{out};
#elif HWY_COMPILER_MSVC && !HWY_COMPILER_CLANG
  const __m128 v128 = LoadU(Full128<float>(), p).raw;
  return Vec256<float>{
      _mm256_insertf128_ps(_mm256_castps128_ps256(v128), v128, 1)};
#else
  return Vec256<float>{_mm256_broadcast_ps(reinterpret_cast<const __m128*>(p))};
#endif
}
HWY_API Vec256<double> LoadDup128(Full256<double> /* tag */,
                                  const double* const HWY_RESTRICT p) {
#if HWY_LOADDUP_ASM
  __m256d out;
  asm("vbroadcastf128 %1, %[reg]" : [ reg ] "=x"(out) : "m"(p[0]));
  return Vec256<double>{out};
#elif HWY_COMPILER_MSVC && !HWY_COMPILER_CLANG
  const __m128d v128 = LoadU(Full128<double>(), p).raw;
  return Vec256<double>{
      _mm256_insertf128_pd(_mm256_castpd128_pd256(v128), v128, 1)};
#else
  return Vec256<double>{
      _mm256_broadcast_pd(reinterpret_cast<const __m128d*>(p))};
#endif
}

// ------------------------------ Store

template <typename T>
HWY_API void Store(Vec256<T> v, Full256<T> /* tag */, T* HWY_RESTRICT aligned) {
  _mm256_store_si256(reinterpret_cast<__m256i*>(aligned), v.raw);
}
HWY_API void Store(const Vec256<float> v, Full256<float> /* tag */,
                   float* HWY_RESTRICT aligned) {
  _mm256_store_ps(aligned, v.raw);
}
HWY_API void Store(const Vec256<double> v, Full256<double> /* tag */,
                   double* HWY_RESTRICT aligned) {
  _mm256_store_pd(aligned, v.raw);
}

template <typename T>
HWY_API void StoreU(Vec256<T> v, Full256<T> /* tag */, T* HWY_RESTRICT p) {
  _mm256_storeu_si256(reinterpret_cast<__m256i*>(p), v.raw);
}
HWY_API void StoreU(const Vec256<float> v, Full256<float> /* tag */,
                    float* HWY_RESTRICT p) {
  _mm256_storeu_ps(p, v.raw);
}
HWY_API void StoreU(const Vec256<double> v, Full256<double> /* tag */,
                    double* HWY_RESTRICT p) {
  _mm256_storeu_pd(p, v.raw);
}

// ------------------------------ Non-temporal stores

template <typename T>
HWY_API void Stream(Vec256<T> v, Full256<T> /* tag */,
                    T* HWY_RESTRICT aligned) {
  _mm256_stream_si256(reinterpret_cast<__m256i*>(aligned), v.raw);
}
HWY_API void Stream(const Vec256<float> v, Full256<float> /* tag */,
                    float* HWY_RESTRICT aligned) {
  _mm256_stream_ps(aligned, v.raw);
}
HWY_API void Stream(const Vec256<double> v, Full256<double> /* tag */,
                    double* HWY_RESTRICT aligned) {
  _mm256_stream_pd(aligned, v.raw);
}

// ------------------------------ Scatter

// Work around warnings in the intrinsic definitions (passing -1 as a mask).
HWY_DIAGNOSTICS(push)
HWY_DIAGNOSTICS_OFF(disable : 4245 4365, ignored "-Wsign-conversion")

#if HWY_TARGET <= HWY_AVX3
namespace detail {

template <typename T>
HWY_INLINE void ScatterOffset(hwy::SizeTag<4> /* tag */, Vec256<T> v,
                              Full256<T> /* tag */, T* HWY_RESTRICT base,
                              const Vec256<int32_t> offset) {
  _mm256_i32scatter_epi32(base, offset.raw, v.raw, 1);
}
template <typename T>
HWY_INLINE void ScatterIndex(hwy::SizeTag<4> /* tag */, Vec256<T> v,
                             Full256<T> /* tag */, T* HWY_RESTRICT base,
                             const Vec256<int32_t> index) {
  _mm256_i32scatter_epi32(base, index.raw, v.raw, 4);
}

template <typename T>
HWY_INLINE void ScatterOffset(hwy::SizeTag<8> /* tag */, Vec256<T> v,
                              Full256<T> /* tag */, T* HWY_RESTRICT base,
                              const Vec256<int64_t> offset) {
  _mm256_i64scatter_epi64(base, offset.raw, v.raw, 1);
}
template <typename T>
HWY_INLINE void ScatterIndex(hwy::SizeTag<8> /* tag */, Vec256<T> v,
                             Full256<T> /* tag */, T* HWY_RESTRICT base,
                             const Vec256<int64_t> index) {
  _mm256_i64scatter_epi64(base, index.raw, v.raw, 8);
}

}  // namespace detail

template <typename T, typename Offset>
HWY_API void ScatterOffset(Vec256<T> v, Full256<T> d, T* HWY_RESTRICT base,
                           const Vec256<Offset> offset) {
  static_assert(sizeof(T) == sizeof(Offset), "Must match for portability");
  return detail::ScatterOffset(hwy::SizeTag<sizeof(T)>(), v, d, base, offset);
}
template <typename T, typename Index>
HWY_API void ScatterIndex(Vec256<T> v, Full256<T> d, T* HWY_RESTRICT base,
                          const Vec256<Index> index) {
  static_assert(sizeof(T) == sizeof(Index), "Must match for portability");
  return detail::ScatterIndex(hwy::SizeTag<sizeof(T)>(), v, d, base, index);
}

HWY_API void ScatterOffset(Vec256<float> v, Full256<float> /* tag */,
                           float* HWY_RESTRICT base,
                           const Vec256<int32_t> offset) {
  _mm256_i32scatter_ps(base, offset.raw, v.raw, 1);
}
HWY_API void ScatterIndex(Vec256<float> v, Full256<float> /* tag */,
                          float* HWY_RESTRICT base,
                          const Vec256<int32_t> index) {
  _mm256_i32scatter_ps(base, index.raw, v.raw, 4);
}

HWY_API void ScatterOffset(Vec256<double> v, Full256<double> /* tag */,
                           double* HWY_RESTRICT base,
                           const Vec256<int64_t> offset) {
  _mm256_i64scatter_pd(base, offset.raw, v.raw, 1);
}
HWY_API void ScatterIndex(Vec256<double> v, Full256<double> /* tag */,
                          double* HWY_RESTRICT base,
                          const Vec256<int64_t> index) {
  _mm256_i64scatter_pd(base, index.raw, v.raw, 8);
}

#else

template <typename T, typename Offset>
HWY_API void ScatterOffset(Vec256<T> v, Full256<T> d, T* HWY_RESTRICT base,
                           const Vec256<Offset> offset) {
  static_assert(sizeof(T) == sizeof(Offset), "Must match for portability");

  constexpr size_t N = 32 / sizeof(T);
  alignas(32) T lanes[N];
  Store(v, d, lanes);

  alignas(32) Offset offset_lanes[N];
  Store(offset, Simd<Offset, N>(), offset_lanes);

  uint8_t* base_bytes = reinterpret_cast<uint8_t*>(base);
  for (size_t i = 0; i < N; ++i) {
    CopyBytes<sizeof(T)>(&lanes[i], base_bytes + offset_lanes[i]);
  }
}

template <typename T, typename Index>
HWY_API void ScatterIndex(Vec256<T> v, Full256<T> d, T* HWY_RESTRICT base,
                          const Vec256<Index> index) {
  static_assert(sizeof(T) == sizeof(Index), "Must match for portability");

  constexpr size_t N = 32 / sizeof(T);
  alignas(32) T lanes[N];
  Store(v, d, lanes);

  alignas(32) Index index_lanes[N];
  Store(index, Simd<Index, N>(), index_lanes);

  for (size_t i = 0; i < N; ++i) {
    base[index_lanes[i]] = lanes[i];
  }
}

#endif

// ------------------------------ Gather

namespace detail {

template <typename T>
HWY_INLINE Vec256<T> GatherOffset(hwy::SizeTag<4> /* tag */,
                                  Full256<T> /* tag */,
                                  const T* HWY_RESTRICT base,
                                  const Vec256<int32_t> offset) {
  return Vec256<T>{_mm256_i32gather_epi32(
      reinterpret_cast<const int32_t*>(base), offset.raw, 1)};
}
template <typename T>
HWY_INLINE Vec256<T> GatherIndex(hwy::SizeTag<4> /* tag */,
                                 Full256<T> /* tag */,
                                 const T* HWY_RESTRICT base,
                                 const Vec256<int32_t> index) {
  return Vec256<T>{_mm256_i32gather_epi32(
      reinterpret_cast<const int32_t*>(base), index.raw, 4)};
}

template <typename T>
HWY_INLINE Vec256<T> GatherOffset(hwy::SizeTag<8> /* tag */,
                                  Full256<T> /* tag */,
                                  const T* HWY_RESTRICT base,
                                  const Vec256<int64_t> offset) {
  return Vec256<T>{_mm256_i64gather_epi64(
      reinterpret_cast<const GatherIndex64*>(base), offset.raw, 1)};
}
template <typename T>
HWY_INLINE Vec256<T> GatherIndex(hwy::SizeTag<8> /* tag */,
                                 Full256<T> /* tag */,
                                 const T* HWY_RESTRICT base,
                                 const Vec256<int64_t> index) {
  return Vec256<T>{_mm256_i64gather_epi64(
      reinterpret_cast<const GatherIndex64*>(base), index.raw, 8)};
}

}  // namespace detail

template <typename T, typename Offset>
HWY_API Vec256<T> GatherOffset(Full256<T> d, const T* HWY_RESTRICT base,
                               const Vec256<Offset> offset) {
  static_assert(sizeof(T) == sizeof(Offset), "Must match for portability");
  return detail::GatherOffset(hwy::SizeTag<sizeof(T)>(), d, base, offset);
}
template <typename T, typename Index>
HWY_API Vec256<T> GatherIndex(Full256<T> d, const T* HWY_RESTRICT base,
                              const Vec256<Index> index) {
  static_assert(sizeof(T) == sizeof(Index), "Must match for portability");
  return detail::GatherIndex(hwy::SizeTag<sizeof(T)>(), d, base, index);
}

HWY_API Vec256<float> GatherOffset(Full256<float> /* tag */,
                                   const float* HWY_RESTRICT base,
                                   const Vec256<int32_t> offset) {
  return Vec256<float>{_mm256_i32gather_ps(base, offset.raw, 1)};
}
HWY_API Vec256<float> GatherIndex(Full256<float> /* tag */,
                                  const float* HWY_RESTRICT base,
                                  const Vec256<int32_t> index) {
  return Vec256<float>{_mm256_i32gather_ps(base, index.raw, 4)};
}

HWY_API Vec256<double> GatherOffset(Full256<double> /* tag */,
                                    const double* HWY_RESTRICT base,
                                    const Vec256<int64_t> offset) {
  return Vec256<double>{_mm256_i64gather_pd(base, offset.raw, 1)};
}
HWY_API Vec256<double> GatherIndex(Full256<double> /* tag */,
                                   const double* HWY_RESTRICT base,
                                   const Vec256<int64_t> index) {
  return Vec256<double>{_mm256_i64gather_pd(base, index.raw, 8)};
}

HWY_DIAGNOSTICS(pop)

// ================================================== SWIZZLE

// ------------------------------ LowerHalf

template <typename T>
HWY_API Vec128<T> LowerHalf(Full128<T> /* tag */, Vec256<T> v) {
  return Vec128<T>{_mm256_castsi256_si128(v.raw)};
}
HWY_API Vec128<float> LowerHalf(Full128<float> /* tag */, Vec256<float> v) {
  return Vec128<float>{_mm256_castps256_ps128(v.raw)};
}
HWY_API Vec128<double> LowerHalf(Full128<double> /* tag */, Vec256<double> v) {
  return Vec128<double>{_mm256_castpd256_pd128(v.raw)};
}

template <typename T>
HWY_API Vec128<T> LowerHalf(Vec256<T> v) {
  return LowerHalf(Full128<T>(), v);
}

// ------------------------------ UpperHalf

template <typename T>
HWY_API Vec128<T> UpperHalf(Full128<T> /* tag */, Vec256<T> v) {
  return Vec128<T>{_mm256_extracti128_si256(v.raw, 1)};
}
HWY_API Vec128<float> UpperHalf(Full128<float> /* tag */, Vec256<float> v) {
  return Vec128<float>{_mm256_extractf128_ps(v.raw, 1)};
}
HWY_API Vec128<double> UpperHalf(Full128<double> /* tag */, Vec256<double> v) {
  return Vec128<double>{_mm256_extractf128_pd(v.raw, 1)};
}

// ------------------------------ GetLane (LowerHalf)
template <typename T>
HWY_API T GetLane(const Vec256<T> v) {
  return GetLane(LowerHalf(v));
}

// ------------------------------ ZeroExtendVector

// Unfortunately the initial _mm256_castsi128_si256 intrinsic leaves the upper
// bits undefined. Although it makes sense for them to be zero (VEX encoded
// 128-bit instructions zero the upper lanes to avoid large penalties), a
// compiler could decide to optimize out code that relies on this.
//
// The newer _mm256_zextsi128_si256 intrinsic fixes this by specifying the
// zeroing, but it is not available on MSVC nor GCC until 10.1. For older GCC,
// we can still obtain the desired code thanks to pattern recognition; note that
// the expensive insert instruction is not actually generated, see
// https://gcc.godbolt.org/z/1MKGaP.

template <typename T>
HWY_API Vec256<T> ZeroExtendVector(Full256<T> /* tag */, Vec128<T> lo) {
#if !HWY_COMPILER_CLANG && HWY_COMPILER_GCC && (HWY_COMPILER_GCC < 1000)
  return Vec256<T>{_mm256_inserti128_si256(_mm256_setzero_si256(), lo.raw, 0)};
#else
  return Vec256<T>{_mm256_zextsi128_si256(lo.raw)};
#endif
}
HWY_API Vec256<float> ZeroExtendVector(Full256<float> /* tag */,
                                       Vec128<float> lo) {
#if !HWY_COMPILER_CLANG && HWY_COMPILER_GCC && (HWY_COMPILER_GCC < 1000)
  return Vec256<float>{_mm256_insertf128_ps(_mm256_setzero_ps(), lo.raw, 0)};
#else
  return Vec256<float>{_mm256_zextps128_ps256(lo.raw)};
#endif
}
HWY_API Vec256<double> ZeroExtendVector(Full256<double> /* tag */,
                                        Vec128<double> lo) {
#if !HWY_COMPILER_CLANG && HWY_COMPILER_GCC && (HWY_COMPILER_GCC < 1000)
  return Vec256<double>{_mm256_insertf128_pd(_mm256_setzero_pd(), lo.raw, 0)};
#else
  return Vec256<double>{_mm256_zextpd128_pd256(lo.raw)};
#endif
}

// ------------------------------ Combine

template <typename T>
HWY_API Vec256<T> Combine(Full256<T> d, Vec128<T> hi, Vec128<T> lo) {
  const auto lo256 = ZeroExtendVector(d, lo);
  return Vec256<T>{_mm256_inserti128_si256(lo256.raw, hi.raw, 1)};
}
HWY_API Vec256<float> Combine(Full256<float> d, Vec128<float> hi,
                              Vec128<float> lo) {
  const auto lo256 = ZeroExtendVector(d, lo);
  return Vec256<float>{_mm256_insertf128_ps(lo256.raw, hi.raw, 1)};
}
HWY_API Vec256<double> Combine(Full256<double> d, Vec128<double> hi,
                               Vec128<double> lo) {
  const auto lo256 = ZeroExtendVector(d, lo);
  return Vec256<double>{_mm256_insertf128_pd(lo256.raw, hi.raw, 1)};
}

// ------------------------------ ShiftLeftBytes

template <int kBytes, typename T>
HWY_API Vec256<T> ShiftLeftBytes(Full256<T> /* tag */, const Vec256<T> v) {
  static_assert(0 <= kBytes && kBytes <= 16, "Invalid kBytes");
  // This is the same operation as _mm256_bslli_epi128.
  return Vec256<T>{_mm256_slli_si256(v.raw, kBytes)};
}

template <int kBytes, typename T>
HWY_API Vec256<T> ShiftLeftBytes(const Vec256<T> v) {
  return ShiftLeftBytes<kBytes>(Full256<T>(), v);
}

// ------------------------------ ShiftLeftLanes

template <int kLanes, typename T>
HWY_API Vec256<T> ShiftLeftLanes(Full256<T> d, const Vec256<T> v) {
  const Repartition<uint8_t, decltype(d)> d8;
  return BitCast(d, ShiftLeftBytes<kLanes * sizeof(T)>(BitCast(d8, v)));
}

template <int kLanes, typename T>
HWY_API Vec256<T> ShiftLeftLanes(const Vec256<T> v) {
  return ShiftLeftLanes<kLanes>(Full256<T>(), v);
}

// ------------------------------ ShiftRightBytes

template <int kBytes, typename T>
HWY_API Vec256<T> ShiftRightBytes(Full256<T> /* tag */, const Vec256<T> v) {
  static_assert(0 <= kBytes && kBytes <= 16, "Invalid kBytes");
  // This is the same operation as _mm256_bsrli_epi128.
  return Vec256<T>{_mm256_srli_si256(v.raw, kBytes)};
}

// ------------------------------ ShiftRightLanes
template <int kLanes, typename T>
HWY_API Vec256<T> ShiftRightLanes(Full256<T> d, const Vec256<T> v) {
  const Repartition<uint8_t, decltype(d)> d8;
  return BitCast(d, ShiftRightBytes<kLanes * sizeof(T)>(BitCast(d8, v)));
}

// ------------------------------ CombineShiftRightBytes

// Extracts 128 bits from <hi, lo> by skipping the least-significant kBytes.
template <int kBytes, typename T, class V = Vec256<T>>
HWY_API V CombineShiftRightBytes(Full256<T> d, V hi, V lo) {
  const Repartition<uint8_t, decltype(d)> d8;
  return BitCast(d, Vec256<uint8_t>{_mm256_alignr_epi8(
                        BitCast(d8, hi).raw, BitCast(d8, lo).raw, kBytes)});
}

// ------------------------------ Broadcast/splat any lane

// Unsigned
template <int kLane>
HWY_API Vec256<uint16_t> Broadcast(const Vec256<uint16_t> v) {
  static_assert(0 <= kLane && kLane < 8, "Invalid lane");
  if (kLane < 4) {
    const __m256i lo = _mm256_shufflelo_epi16(v.raw, (0x55 * kLane) & 0xFF);
    return Vec256<uint16_t>{_mm256_unpacklo_epi64(lo, lo)};
  } else {
    const __m256i hi =
        _mm256_shufflehi_epi16(v.raw, (0x55 * (kLane - 4)) & 0xFF);
    return Vec256<uint16_t>{_mm256_unpackhi_epi64(hi, hi)};
  }
}
template <int kLane>
HWY_API Vec256<uint32_t> Broadcast(const Vec256<uint32_t> v) {
  static_assert(0 <= kLane && kLane < 4, "Invalid lane");
  return Vec256<uint32_t>{_mm256_shuffle_epi32(v.raw, 0x55 * kLane)};
}
template <int kLane>
HWY_API Vec256<uint64_t> Broadcast(const Vec256<uint64_t> v) {
  static_assert(0 <= kLane && kLane < 2, "Invalid lane");
  return Vec256<uint64_t>{_mm256_shuffle_epi32(v.raw, kLane ? 0xEE : 0x44)};
}

// Signed
template <int kLane>
HWY_API Vec256<int16_t> Broadcast(const Vec256<int16_t> v) {
  static_assert(0 <= kLane && kLane < 8, "Invalid lane");
  if (kLane < 4) {
    const __m256i lo = _mm256_shufflelo_epi16(v.raw, (0x55 * kLane) & 0xFF);
    return Vec256<int16_t>{_mm256_unpacklo_epi64(lo, lo)};
  } else {
    const __m256i hi =
        _mm256_shufflehi_epi16(v.raw, (0x55 * (kLane - 4)) & 0xFF);
    return Vec256<int16_t>{_mm256_unpackhi_epi64(hi, hi)};
  }
}
template <int kLane>
HWY_API Vec256<int32_t> Broadcast(const Vec256<int32_t> v) {
  static_assert(0 <= kLane && kLane < 4, "Invalid lane");
  return Vec256<int32_t>{_mm256_shuffle_epi32(v.raw, 0x55 * kLane)};
}
template <int kLane>
HWY_API Vec256<int64_t> Broadcast(const Vec256<int64_t> v) {
  static_assert(0 <= kLane && kLane < 2, "Invalid lane");
  return Vec256<int64_t>{_mm256_shuffle_epi32(v.raw, kLane ? 0xEE : 0x44)};
}

// Float
template <int kLane>
HWY_API Vec256<float> Broadcast(Vec256<float> v) {
  static_assert(0 <= kLane && kLane < 4, "Invalid lane");
  return Vec256<float>{_mm256_shuffle_ps(v.raw, v.raw, 0x55 * kLane)};
}
template <int kLane>
HWY_API Vec256<double> Broadcast(const Vec256<double> v) {
  static_assert(0 <= kLane && kLane < 2, "Invalid lane");
  return Vec256<double>{_mm256_shuffle_pd(v.raw, v.raw, 15 * kLane)};
}

// ------------------------------ Hard-coded shuffles

// Notation: let Vec256<int32_t> have lanes 7,6,5,4,3,2,1,0 (0 is
// least-significant). Shuffle0321 rotates four-lane blocks one lane to the
// right (the previous least-significant lane is now most-significant =>
// 47650321). These could also be implemented via CombineShiftRightBytes but
// the shuffle_abcd notation is more convenient.

// Swap 32-bit halves in 64-bit halves.
HWY_API Vec256<uint32_t> Shuffle2301(const Vec256<uint32_t> v) {
  return Vec256<uint32_t>{_mm256_shuffle_epi32(v.raw, 0xB1)};
}
HWY_API Vec256<int32_t> Shuffle2301(const Vec256<int32_t> v) {
  return Vec256<int32_t>{_mm256_shuffle_epi32(v.raw, 0xB1)};
}
HWY_API Vec256<float> Shuffle2301(const Vec256<float> v) {
  return Vec256<float>{_mm256_shuffle_ps(v.raw, v.raw, 0xB1)};
}

// Swap 64-bit halves
HWY_API Vec256<uint32_t> Shuffle1032(const Vec256<uint32_t> v) {
  return Vec256<uint32_t>{_mm256_shuffle_epi32(v.raw, 0x4E)};
}
HWY_API Vec256<int32_t> Shuffle1032(const Vec256<int32_t> v) {
  return Vec256<int32_t>{_mm256_shuffle_epi32(v.raw, 0x4E)};
}
HWY_API Vec256<float> Shuffle1032(const Vec256<float> v) {
  // Shorter encoding than _mm256_permute_ps.
  return Vec256<float>{_mm256_shuffle_ps(v.raw, v.raw, 0x4E)};
}
HWY_API Vec256<uint64_t> Shuffle01(const Vec256<uint64_t> v) {
  return Vec256<uint64_t>{_mm256_shuffle_epi32(v.raw, 0x4E)};
}
HWY_API Vec256<int64_t> Shuffle01(const Vec256<int64_t> v) {
  return Vec256<int64_t>{_mm256_shuffle_epi32(v.raw, 0x4E)};
}
HWY_API Vec256<double> Shuffle01(const Vec256<double> v) {
  // Shorter encoding than _mm256_permute_pd.
  return Vec256<double>{_mm256_shuffle_pd(v.raw, v.raw, 5)};
}

// Rotate right 32 bits
HWY_API Vec256<uint32_t> Shuffle0321(const Vec256<uint32_t> v) {
  return Vec256<uint32_t>{_mm256_shuffle_epi32(v.raw, 0x39)};
}
HWY_API Vec256<int32_t> Shuffle0321(const Vec256<int32_t> v) {
  return Vec256<int32_t>{_mm256_shuffle_epi32(v.raw, 0x39)};
}
HWY_API Vec256<float> Shuffle0321(const Vec256<float> v) {
  return Vec256<float>{_mm256_shuffle_ps(v.raw, v.raw, 0x39)};
}
// Rotate left 32 bits
HWY_API Vec256<uint32_t> Shuffle2103(const Vec256<uint32_t> v) {
  return Vec256<uint32_t>{_mm256_shuffle_epi32(v.raw, 0x93)};
}
HWY_API Vec256<int32_t> Shuffle2103(const Vec256<int32_t> v) {
  return Vec256<int32_t>{_mm256_shuffle_epi32(v.raw, 0x93)};
}
HWY_API Vec256<float> Shuffle2103(const Vec256<float> v) {
  return Vec256<float>{_mm256_shuffle_ps(v.raw, v.raw, 0x93)};
}

// Reverse
HWY_API Vec256<uint32_t> Shuffle0123(const Vec256<uint32_t> v) {
  return Vec256<uint32_t>{_mm256_shuffle_epi32(v.raw, 0x1B)};
}
HWY_API Vec256<int32_t> Shuffle0123(const Vec256<int32_t> v) {
  return Vec256<int32_t>{_mm256_shuffle_epi32(v.raw, 0x1B)};
}
HWY_API Vec256<float> Shuffle0123(const Vec256<float> v) {
  return Vec256<float>{_mm256_shuffle_ps(v.raw, v.raw, 0x1B)};
}

// ------------------------------ TableLookupLanes

// Returned by SetTableIndices for use by TableLookupLanes.
template <typename T>
struct Indices256 {
  __m256i raw;
};

template <typename T>
HWY_API Indices256<T> SetTableIndices(const Full256<T>, const int32_t* idx) {
#if HWY_IS_DEBUG_BUILD
  const size_t N = 32 / sizeof(T);
  for (size_t i = 0; i < N; ++i) {
    HWY_DASSERT(0 <= idx[i] && idx[i] < static_cast<int32_t>(N));
  }
#endif
  return Indices256<T>{LoadU(Full256<int32_t>(), idx).raw};
}

HWY_API Vec256<uint32_t> TableLookupLanes(const Vec256<uint32_t> v,
                                          const Indices256<uint32_t> idx) {
  return Vec256<uint32_t>{_mm256_permutevar8x32_epi32(v.raw, idx.raw)};
}
HWY_API Vec256<int32_t> TableLookupLanes(const Vec256<int32_t> v,
                                         const Indices256<int32_t> idx) {
  return Vec256<int32_t>{_mm256_permutevar8x32_epi32(v.raw, idx.raw)};
}
HWY_API Vec256<float> TableLookupLanes(const Vec256<float> v,
                                       const Indices256<float> idx) {
  return Vec256<float>{_mm256_permutevar8x32_ps(v.raw, idx.raw)};
}

// ------------------------------ InterleaveLower

// Interleaves lanes from halves of the 128-bit blocks of "a" (which provides
// the least-significant lane) and "b". To concatenate two half-width integers
// into one, use ZipLower/Upper instead (also works with scalar).

HWY_API Vec256<uint8_t> InterleaveLower(const Vec256<uint8_t> a,
                                        const Vec256<uint8_t> b) {
  return Vec256<uint8_t>{_mm256_unpacklo_epi8(a.raw, b.raw)};
}
HWY_API Vec256<uint16_t> InterleaveLower(const Vec256<uint16_t> a,
                                         const Vec256<uint16_t> b) {
  return Vec256<uint16_t>{_mm256_unpacklo_epi16(a.raw, b.raw)};
}
HWY_API Vec256<uint32_t> InterleaveLower(const Vec256<uint32_t> a,
                                         const Vec256<uint32_t> b) {
  return Vec256<uint32_t>{_mm256_unpacklo_epi32(a.raw, b.raw)};
}
HWY_API Vec256<uint64_t> InterleaveLower(const Vec256<uint64_t> a,
                                         const Vec256<uint64_t> b) {
  return Vec256<uint64_t>{_mm256_unpacklo_epi64(a.raw, b.raw)};
}

HWY_API Vec256<int8_t> InterleaveLower(const Vec256<int8_t> a,
                                       const Vec256<int8_t> b) {
  return Vec256<int8_t>{_mm256_unpacklo_epi8(a.raw, b.raw)};
}
HWY_API Vec256<int16_t> InterleaveLower(const Vec256<int16_t> a,
                                        const Vec256<int16_t> b) {
  return Vec256<int16_t>{_mm256_unpacklo_epi16(a.raw, b.raw)};
}
HWY_API Vec256<int32_t> InterleaveLower(const Vec256<int32_t> a,
                                        const Vec256<int32_t> b) {
  return Vec256<int32_t>{_mm256_unpacklo_epi32(a.raw, b.raw)};
}
HWY_API Vec256<int64_t> InterleaveLower(const Vec256<int64_t> a,
                                        const Vec256<int64_t> b) {
  return Vec256<int64_t>{_mm256_unpacklo_epi64(a.raw, b.raw)};
}

HWY_API Vec256<float> InterleaveLower(const Vec256<float> a,
                                      const Vec256<float> b) {
  return Vec256<float>{_mm256_unpacklo_ps(a.raw, b.raw)};
}
HWY_API Vec256<double> InterleaveLower(const Vec256<double> a,
                                       const Vec256<double> b) {
  return Vec256<double>{_mm256_unpacklo_pd(a.raw, b.raw)};
}

// Additional overload for the optional Simd<> tag.
template <typename T, class V = Vec256<T>>
HWY_API V InterleaveLower(Full256<T> /* tag */, V a, V b) {
  return InterleaveLower(a, b);
}

// ------------------------------ InterleaveUpper

// All functions inside detail lack the required D parameter.
namespace detail {

HWY_API Vec256<uint8_t> InterleaveUpper(const Vec256<uint8_t> a,
                                        const Vec256<uint8_t> b) {
  return Vec256<uint8_t>{_mm256_unpackhi_epi8(a.raw, b.raw)};
}
HWY_API Vec256<uint16_t> InterleaveUpper(const Vec256<uint16_t> a,
                                         const Vec256<uint16_t> b) {
  return Vec256<uint16_t>{_mm256_unpackhi_epi16(a.raw, b.raw)};
}
HWY_API Vec256<uint32_t> InterleaveUpper(const Vec256<uint32_t> a,
                                         const Vec256<uint32_t> b) {
  return Vec256<uint32_t>{_mm256_unpackhi_epi32(a.raw, b.raw)};
}
HWY_API Vec256<uint64_t> InterleaveUpper(const Vec256<uint64_t> a,
                                         const Vec256<uint64_t> b) {
  return Vec256<uint64_t>{_mm256_unpackhi_epi64(a.raw, b.raw)};
}

HWY_API Vec256<int8_t> InterleaveUpper(const Vec256<int8_t> a,
                                       const Vec256<int8_t> b) {
  return Vec256<int8_t>{_mm256_unpackhi_epi8(a.raw, b.raw)};
}
HWY_API Vec256<int16_t> InterleaveUpper(const Vec256<int16_t> a,
                                        const Vec256<int16_t> b) {
  return Vec256<int16_t>{_mm256_unpackhi_epi16(a.raw, b.raw)};
}
HWY_API Vec256<int32_t> InterleaveUpper(const Vec256<int32_t> a,
                                        const Vec256<int32_t> b) {
  return Vec256<int32_t>{_mm256_unpackhi_epi32(a.raw, b.raw)};
}
HWY_API Vec256<int64_t> InterleaveUpper(const Vec256<int64_t> a,
                                        const Vec256<int64_t> b) {
  return Vec256<int64_t>{_mm256_unpackhi_epi64(a.raw, b.raw)};
}

HWY_API Vec256<float> InterleaveUpper(const Vec256<float> a,
                                      const Vec256<float> b) {
  return Vec256<float>{_mm256_unpackhi_ps(a.raw, b.raw)};
}
HWY_API Vec256<double> InterleaveUpper(const Vec256<double> a,
                                       const Vec256<double> b) {
  return Vec256<double>{_mm256_unpackhi_pd(a.raw, b.raw)};
}

}  // namespace detail

template <typename T, class V = Vec256<T>>
HWY_API V InterleaveUpper(Full256<T> /* tag */, V a, V b) {
  return detail::InterleaveUpper(a, b);
}

// ------------------------------ ZipLower/ZipUpper (InterleaveLower)

// Same as Interleave*, except that the return lanes are double-width integers;
// this is necessary because the single-lane scalar cannot return two values.
template <typename T, typename TW = MakeWide<T>>
HWY_API Vec256<TW> ZipLower(Vec256<T> a, Vec256<T> b) {
  return BitCast(Full256<TW>(), InterleaveLower(Full256<T>(), a, b));
}
template <typename T, typename TW = MakeWide<T>>
HWY_API Vec256<TW> ZipLower(Full256<TW> dw, Vec256<T> a, Vec256<T> b) {
  return BitCast(dw, InterleaveLower(Full256<T>(), a, b));
}

template <typename T, typename TW = MakeWide<T>>
HWY_API Vec256<TW> ZipUpper(Full256<TW> dw, Vec256<T> a, Vec256<T> b) {
  return BitCast(dw, InterleaveUpper(Full256<T>(), a, b));
}

// ------------------------------ Blocks (LowerHalf, ZeroExtendVector)

// _mm256_broadcastsi128_si256 has 7 cycle latency. _mm256_permute2x128_si256 is
// slow on Zen1 (8 uops); we can avoid it for LowerLower and UpperLower, and on
// UpperUpper at the cost of one extra cycle/instruction.

// hiH,hiL loH,loL |-> hiL,loL (= lower halves)
template <typename T>
HWY_API Vec256<T> ConcatLowerLower(Full256<T> d, const Vec256<T> hi,
                                   const Vec256<T> lo) {
  const Half<decltype(d)> d2;
  return Vec256<T>{_mm256_inserti128_si256(lo.raw, LowerHalf(d2, hi).raw, 1)};
}
HWY_API Vec256<float> ConcatLowerLower(Full256<float> d, const Vec256<float> hi,
                                       const Vec256<float> lo) {
  const Half<decltype(d)> d2;
  return Vec256<float>{_mm256_insertf128_ps(lo.raw, LowerHalf(d2, hi).raw, 1)};
}
HWY_API Vec256<double> ConcatLowerLower(Full256<double> d,
                                        const Vec256<double> hi,
                                        const Vec256<double> lo) {
  const Half<decltype(d)> d2;
  return Vec256<double>{_mm256_insertf128_pd(lo.raw, LowerHalf(d2, hi).raw, 1)};
}

// hiH,hiL loH,loL |-> hiL,loH (= inner halves / swap blocks)
template <typename T>
HWY_API Vec256<T> ConcatLowerUpper(Full256<T> /* tag */, const Vec256<T> hi,
                                   const Vec256<T> lo) {
  return Vec256<T>{_mm256_permute2x128_si256(lo.raw, hi.raw, 0x21)};
}
HWY_API Vec256<float> ConcatLowerUpper(Full256<float> /* tag */,
                                       const Vec256<float> hi,
                                       const Vec256<float> lo) {
  return Vec256<float>{_mm256_permute2f128_ps(lo.raw, hi.raw, 0x21)};
}
HWY_API Vec256<double> ConcatLowerUpper(Full256<double> /* tag */,
                                        const Vec256<double> hi,
                                        const Vec256<double> lo) {
  return Vec256<double>{_mm256_permute2f128_pd(lo.raw, hi.raw, 0x21)};
}

// hiH,hiL loH,loL |-> hiH,loL (= outer halves)
template <typename T>
HWY_API Vec256<T> ConcatUpperLower(Full256<T> /* tag */, const Vec256<T> hi,
                                   const Vec256<T> lo) {
  return Vec256<T>{_mm256_blend_epi32(hi.raw, lo.raw, 0x0F)};
}
HWY_API Vec256<float> ConcatUpperLower(Full256<float> /* tag */,
                                       const Vec256<float> hi,
                                       const Vec256<float> lo) {
  return Vec256<float>{_mm256_blend_ps(hi.raw, lo.raw, 0x0F)};
}
HWY_API Vec256<double> ConcatUpperLower(Full256<double> /* tag */,
                                        const Vec256<double> hi,
                                        const Vec256<double> lo) {
  return Vec256<double>{_mm256_blend_pd(hi.raw, lo.raw, 3)};
}

// hiH,hiL loH,loL |-> hiH,loH (= upper halves)
template <typename T>
HWY_API Vec256<T> ConcatUpperUpper(Full256<T> d, const Vec256<T> hi,
                                   const Vec256<T> lo) {
  const Half<decltype(d)> d2;
  return ConcatUpperLower(d, hi, ZeroExtendVector(d, UpperHalf(d2, lo)));
}

// ------------------------------ Odd/even lanes

namespace detail {

template <typename T>
HWY_INLINE Vec256<T> OddEven(hwy::SizeTag<1> /* tag */, const Vec256<T> a,
                             const Vec256<T> b) {
  const Full256<T> d;
  const Full256<uint8_t> d8;
  alignas(32) constexpr uint8_t mask[16] = {0xFF, 0, 0xFF, 0, 0xFF, 0, 0xFF, 0,
                                            0xFF, 0, 0xFF, 0, 0xFF, 0, 0xFF, 0};
  return IfThenElse(MaskFromVec(BitCast(d, LoadDup128(d8, mask))), b, a);
}
template <typename T>
HWY_INLINE Vec256<T> OddEven(hwy::SizeTag<2> /* tag */, const Vec256<T> a,
                             const Vec256<T> b) {
  return Vec256<T>{_mm256_blend_epi16(a.raw, b.raw, 0x55)};
}
template <typename T>
HWY_INLINE Vec256<T> OddEven(hwy::SizeTag<4> /* tag */, const Vec256<T> a,
                             const Vec256<T> b) {
  return Vec256<T>{_mm256_blend_epi32(a.raw, b.raw, 0x55)};
}
template <typename T>
HWY_INLINE Vec256<T> OddEven(hwy::SizeTag<8> /* tag */, const Vec256<T> a,
                             const Vec256<T> b) {
  return Vec256<T>{_mm256_blend_epi32(a.raw, b.raw, 0x33)};
}

}  // namespace detail

template <typename T>
HWY_API Vec256<T> OddEven(const Vec256<T> a, const Vec256<T> b) {
  return detail::OddEven(hwy::SizeTag<sizeof(T)>(), a, b);
}
HWY_API Vec256<float> OddEven(const Vec256<float> a, const Vec256<float> b) {
  return Vec256<float>{_mm256_blend_ps(a.raw, b.raw, 0x55)};
}

HWY_API Vec256<double> OddEven(const Vec256<double> a, const Vec256<double> b) {
  return Vec256<double>{_mm256_blend_pd(a.raw, b.raw, 5)};
}

// ------------------------------ TableLookupBytes (ZeroExtendVector)

// Both full
template <typename T, typename TI>
HWY_API Vec256<TI> TableLookupBytes(const Vec256<T> bytes,
                                    const Vec256<TI> from) {
  return Vec256<TI>{_mm256_shuffle_epi8(bytes.raw, from.raw)};
}

// Partial index vector
template <typename T, typename TI, size_t NI>
HWY_API Vec128<TI, NI> TableLookupBytes(const Vec256<T> bytes,
                                        const Vec128<TI, NI> from) {
  // First expand to full 128, then 256.
  const auto from_256 = ZeroExtendVector(Full256<TI>(), Vec128<TI>{from.raw});
  const auto tbl_full = TableLookupBytes(bytes, from_256);
  // Shrink to 128, then partial.
  return Vec128<TI, NI>{LowerHalf(Full128<TI>(), tbl_full).raw};
}

// Partial table vector
template <typename T, size_t N, typename TI>
HWY_API Vec256<TI> TableLookupBytes(const Vec128<T, N> bytes,
                                    const Vec256<TI> from) {
  // First expand to full 128, then 256.
  const auto bytes_256 = ZeroExtendVector(Full256<T>(), Vec128<T>{bytes.raw});
  return TableLookupBytes(bytes_256, from);
}

// Partial both are handled by x86_128.

// ------------------------------ Shl (Mul, ZipLower)

#if HWY_TARGET > HWY_AVX3  // AVX2 or older
namespace detail {

// Returns 2^v for use as per-lane multipliers to emulate 16-bit shifts.
template <typename T, HWY_IF_LANE_SIZE(T, 2)>
HWY_INLINE Vec256<MakeUnsigned<T>> Pow2(const Vec256<T> v) {
  const Full256<T> d;
  const RepartitionToWide<decltype(d)> dw;
  const Rebind<float, decltype(dw)> df;
  const auto zero = Zero(d);
  // Move into exponent (this u16 will become the upper half of an f32)
  const auto exp = ShiftLeft<23 - 16>(v);
  const auto upper = exp + Set(d, 0x3F80);  // upper half of 1.0f
  // Insert 0 into lower halves for reinterpreting as binary32.
  const auto f0 = ZipLower(dw, zero, upper);
  const auto f1 = ZipUpper(dw, zero, upper);
  // Do not use ConvertTo because it checks for overflow, which is redundant
  // because we only care about v in [0, 16).
  const Vec256<int32_t> bits0{_mm256_cvttps_epi32(BitCast(df, f0).raw)};
  const Vec256<int32_t> bits1{_mm256_cvttps_epi32(BitCast(df, f1).raw)};
  return Vec256<MakeUnsigned<T>>{_mm256_packus_epi32(bits0.raw, bits1.raw)};
}

}  // namespace detail
#endif  // HWY_TARGET > HWY_AVX3

HWY_API Vec256<uint16_t> operator<<(const Vec256<uint16_t> v,
                                    const Vec256<uint16_t> bits) {
#if HWY_TARGET <= HWY_AVX3
  return Vec256<uint16_t>{_mm256_sllv_epi16(v.raw, bits.raw)};
#else
  return v * detail::Pow2(bits);
#endif
}

HWY_API Vec256<uint32_t> operator<<(const Vec256<uint32_t> v,
                                    const Vec256<uint32_t> bits) {
  return Vec256<uint32_t>{_mm256_sllv_epi32(v.raw, bits.raw)};
}

HWY_API Vec256<uint64_t> operator<<(const Vec256<uint64_t> v,
                                    const Vec256<uint64_t> bits) {
  return Vec256<uint64_t>{_mm256_sllv_epi64(v.raw, bits.raw)};
}

// Signed left shift is the same as unsigned.
template <typename T, HWY_IF_SIGNED(T)>
HWY_API Vec256<T> operator<<(const Vec256<T> v, const Vec256<T> bits) {
  const Full256<T> di;
  const Full256<MakeUnsigned<T>> du;
  return BitCast(di, BitCast(du, v) << BitCast(du, bits));
}

// ------------------------------ Shr (MulHigh, IfThenElse, Not)

HWY_API Vec256<uint16_t> operator>>(const Vec256<uint16_t> v,
                                    const Vec256<uint16_t> bits) {
#if HWY_TARGET <= HWY_AVX3
  return Vec256<uint16_t>{_mm256_srlv_epi16(v.raw, bits.raw)};
#else
  const Full256<uint16_t> d;
  // For bits=0, we cannot mul by 2^16, so fix the result later.
  const auto out = MulHigh(v, detail::Pow2(Set(d, 16) - bits));
  // Replace output with input where bits == 0.
  return IfThenElse(bits == Zero(d), v, out);
#endif
}

HWY_API Vec256<uint32_t> operator>>(const Vec256<uint32_t> v,
                                    const Vec256<uint32_t> bits) {
  return Vec256<uint32_t>{_mm256_srlv_epi32(v.raw, bits.raw)};
}

HWY_API Vec256<uint64_t> operator>>(const Vec256<uint64_t> v,
                                    const Vec256<uint64_t> bits) {
  return Vec256<uint64_t>{_mm256_srlv_epi64(v.raw, bits.raw)};
}

HWY_API Vec256<int16_t> operator>>(const Vec256<int16_t> v,
                                   const Vec256<int16_t> bits) {
#if HWY_TARGET <= HWY_AVX3
  return Vec256<int16_t>{_mm256_srav_epi16(v.raw, bits.raw)};
#else
  return detail::SignedShr(Full256<int16_t>(), v, bits);
#endif
}

HWY_API Vec256<int32_t> operator>>(const Vec256<int32_t> v,
                                   const Vec256<int32_t> bits) {
  return Vec256<int32_t>{_mm256_srav_epi32(v.raw, bits.raw)};
}

HWY_API Vec256<int64_t> operator>>(const Vec256<int64_t> v,
                                   const Vec256<int64_t> bits) {
#if HWY_TARGET <= HWY_AVX3
  return Vec256<int64_t>{_mm256_srav_epi64(v.raw, bits.raw)};
#else
  return detail::SignedShr(Full256<int64_t>(), v, bits);
#endif
}

HWY_INLINE Vec256<uint64_t> MulEven(const Vec256<uint64_t> a,
                                    const Vec256<uint64_t> b) {
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

HWY_INLINE Vec256<uint64_t> MulOdd(const Vec256<uint64_t> a,
                                   const Vec256<uint64_t> b) {
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

// ================================================== CONVERT

// ------------------------------ Promotions (part w/ narrow lanes -> full)

HWY_API Vec256<double> PromoteTo(Full256<double> /* tag */,
                                 const Vec128<float, 4> v) {
  return Vec256<double>{_mm256_cvtps_pd(v.raw)};
}

HWY_API Vec256<double> PromoteTo(Full256<double> /* tag */,
                                 const Vec128<int32_t, 4> v) {
  return Vec256<double>{_mm256_cvtepi32_pd(v.raw)};
}

// Unsigned: zero-extend.
// Note: these have 3 cycle latency; if inputs are already split across the
// 128 bit blocks (in their upper/lower halves), then Zip* would be faster.
HWY_API Vec256<uint16_t> PromoteTo(Full256<uint16_t> /* tag */,
                                   Vec128<uint8_t> v) {
  return Vec256<uint16_t>{_mm256_cvtepu8_epi16(v.raw)};
}
HWY_API Vec256<uint32_t> PromoteTo(Full256<uint32_t> /* tag */,
                                   Vec128<uint8_t, 8> v) {
  return Vec256<uint32_t>{_mm256_cvtepu8_epi32(v.raw)};
}
HWY_API Vec256<int16_t> PromoteTo(Full256<int16_t> /* tag */,
                                  Vec128<uint8_t> v) {
  return Vec256<int16_t>{_mm256_cvtepu8_epi16(v.raw)};
}
HWY_API Vec256<int32_t> PromoteTo(Full256<int32_t> /* tag */,
                                  Vec128<uint8_t, 8> v) {
  return Vec256<int32_t>{_mm256_cvtepu8_epi32(v.raw)};
}
HWY_API Vec256<uint32_t> PromoteTo(Full256<uint32_t> /* tag */,
                                   Vec128<uint16_t> v) {
  return Vec256<uint32_t>{_mm256_cvtepu16_epi32(v.raw)};
}
HWY_API Vec256<int32_t> PromoteTo(Full256<int32_t> /* tag */,
                                  Vec128<uint16_t> v) {
  return Vec256<int32_t>{_mm256_cvtepu16_epi32(v.raw)};
}
HWY_API Vec256<uint64_t> PromoteTo(Full256<uint64_t> /* tag */,
                                   Vec128<uint32_t> v) {
  return Vec256<uint64_t>{_mm256_cvtepu32_epi64(v.raw)};
}

// Signed: replicate sign bit.
// Note: these have 3 cycle latency; if inputs are already split across the
// 128 bit blocks (in their upper/lower halves), then ZipUpper/lo followed by
// signed shift would be faster.
HWY_API Vec256<int16_t> PromoteTo(Full256<int16_t> /* tag */,
                                  Vec128<int8_t> v) {
  return Vec256<int16_t>{_mm256_cvtepi8_epi16(v.raw)};
}
HWY_API Vec256<int32_t> PromoteTo(Full256<int32_t> /* tag */,
                                  Vec128<int8_t, 8> v) {
  return Vec256<int32_t>{_mm256_cvtepi8_epi32(v.raw)};
}
HWY_API Vec256<int32_t> PromoteTo(Full256<int32_t> /* tag */,
                                  Vec128<int16_t> v) {
  return Vec256<int32_t>{_mm256_cvtepi16_epi32(v.raw)};
}
HWY_API Vec256<int64_t> PromoteTo(Full256<int64_t> /* tag */,
                                  Vec128<int32_t> v) {
  return Vec256<int64_t>{_mm256_cvtepi32_epi64(v.raw)};
}

// ------------------------------ Demotions (full -> part w/ narrow lanes)

HWY_API Vec128<uint16_t> DemoteTo(Full128<uint16_t> /* tag */,
                                  const Vec256<int32_t> v) {
  const __m256i u16 = _mm256_packus_epi32(v.raw, v.raw);
  // Concatenating lower halves of both 128-bit blocks afterward is more
  // efficient than an extra input with low block = high block of v.
  return Vec128<uint16_t>{
      _mm256_castsi256_si128(_mm256_permute4x64_epi64(u16, 0x88))};
}

HWY_API Vec128<int16_t> DemoteTo(Full128<int16_t> /* tag */,
                                 const Vec256<int32_t> v) {
  const __m256i i16 = _mm256_packs_epi32(v.raw, v.raw);
  return Vec128<int16_t>{
      _mm256_castsi256_si128(_mm256_permute4x64_epi64(i16, 0x88))};
}

HWY_API Vec128<uint8_t, 8> DemoteTo(Simd<uint8_t, 8> /* tag */,
                                    const Vec256<int32_t> v) {
  const __m256i u16_blocks = _mm256_packus_epi32(v.raw, v.raw);
  // Concatenate lower 64 bits of each 128-bit block
  const __m256i u16_concat = _mm256_permute4x64_epi64(u16_blocks, 0x88);
  const __m128i u16 = _mm256_castsi256_si128(u16_concat);
  // packus treats the input as signed; we want unsigned. Clear the MSB to get
  // unsigned saturation to u8.
  const __m128i i16 = _mm_and_si128(u16, _mm_set1_epi16(0x7FFF));
  return Vec128<uint8_t, 8>{_mm_packus_epi16(i16, i16)};
}

HWY_API Vec128<uint8_t> DemoteTo(Full128<uint8_t> /* tag */,
                                 const Vec256<int16_t> v) {
  const __m256i u8 = _mm256_packus_epi16(v.raw, v.raw);
  return Vec128<uint8_t>{
      _mm256_castsi256_si128(_mm256_permute4x64_epi64(u8, 0x88))};
}

HWY_API Vec128<int8_t, 8> DemoteTo(Simd<int8_t, 8> /* tag */,
                                   const Vec256<int32_t> v) {
  const __m256i i16_blocks = _mm256_packs_epi32(v.raw, v.raw);
  // Concatenate lower 64 bits of each 128-bit block
  const __m256i i16_concat = _mm256_permute4x64_epi64(i16_blocks, 0x88);
  const __m128i i16 = _mm256_castsi256_si128(i16_concat);
  return Vec128<int8_t, 8>{_mm_packs_epi16(i16, i16)};
}

HWY_API Vec128<int8_t> DemoteTo(Full128<int8_t> /* tag */,
                                const Vec256<int16_t> v) {
  const __m256i i8 = _mm256_packs_epi16(v.raw, v.raw);
  return Vec128<int8_t>{
      _mm256_castsi256_si128(_mm256_permute4x64_epi64(i8, 0x88))};
}

  // Avoid "value of intrinsic immediate argument '8' is out of range '0 - 7'".
  // 8 is the correct value of _MM_FROUND_NO_EXC, which is allowed here.
HWY_DIAGNOSTICS(push)
HWY_DIAGNOSTICS_OFF(disable : 4556, ignored "-Wsign-conversion")

HWY_API Vec128<float16_t> DemoteTo(Full128<float16_t> df16,
                                   const Vec256<float> v) {
#ifdef HWY_DISABLE_F16C
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
  return Vec128<float16_t>{_mm256_cvtps_ph(v.raw, _MM_FROUND_NO_EXC)};
#endif
}

HWY_DIAGNOSTICS(pop)

HWY_API Vec128<float> DemoteTo(Full128<float> /* tag */,
                               const Vec256<double> v) {
  return Vec128<float>{_mm256_cvtpd_ps(v.raw)};
}

HWY_API Vec128<int32_t> DemoteTo(Full128<int32_t> /* tag */,
                                 const Vec256<double> v) {
  const auto clamped = detail::ClampF64ToI32Max(Full256<double>(), v);
  return Vec128<int32_t>{_mm256_cvttpd_epi32(clamped.raw)};
}

// For already range-limited input [0, 255].
HWY_API Vec128<uint8_t, 8> U8FromU32(const Vec256<uint32_t> v) {
  const Full256<uint32_t> d32;
  alignas(32) static constexpr uint32_t k8From32[8] = {
      0x0C080400u, ~0u, ~0u, ~0u, ~0u, 0x0C080400u, ~0u, ~0u};
  // Place first four bytes in lo[0], remaining 4 in hi[1].
  const auto quad = TableLookupBytes(v, Load(d32, k8From32));
  // Interleave both quadruplets - OR instead of unpack reduces port5 pressure.
  const auto lo = LowerHalf(quad);
  const auto hi = UpperHalf(Full128<uint32_t>(), quad);
  const auto pair = LowerHalf(lo | hi);
  return BitCast(Simd<uint8_t, 8>(), pair);
}

// ------------------------------ Integer <=> fp (ShiftRight, OddEven)

HWY_API Vec256<float> ConvertTo(Full256<float> /* tag */,
                                const Vec256<int32_t> v) {
  return Vec256<float>{_mm256_cvtepi32_ps(v.raw)};
}

HWY_API Vec256<double> ConvertTo(Full256<double> dd, const Vec256<int64_t> v) {
#if HWY_TARGET <= HWY_AVX3
  (void)dd;
  return Vec256<double>{_mm256_cvtepi64_pd(v.raw)};
#else
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
#endif
}

// Truncates (rounds toward zero).
HWY_API Vec256<int32_t> ConvertTo(Full256<int32_t> d, const Vec256<float> v) {
  return detail::FixConversionOverflow(d, v, _mm256_cvttps_epi32(v.raw));
}

HWY_API Vec256<int64_t> ConvertTo(Full256<int64_t> di, const Vec256<double> v) {
#if HWY_TARGET <= HWY_AVX3
  return detail::FixConversionOverflow(di, v, _mm256_cvttpd_epi64(v.raw));
#else
  using VI = decltype(Zero(di));
  const VI k0 = Zero(di);
  const VI k1 = Set(di, 1);
  const VI k51 = Set(di, 51);

  // Exponent indicates whether the number can be represented as int64_t.
  const VI biased_exp = ShiftRight<52>(BitCast(di, v)) & Set(di, 0x7FF);
  const VI exp = biased_exp - Set(di, 0x3FF);
  const auto in_range = exp < Set(di, 63);

  // If we were to cap the exponent at 51 and add 2^52, the number would be in
  // [2^52, 2^53) and mantissa bits could be read out directly. We need to
  // round-to-0 (truncate), but changing rounding mode in MXCSR hits a
  // compiler reordering bug: https://gcc.godbolt.org/z/4hKj6c6qc . We instead
  // manually shift the mantissa into place (we already have many of the
  // inputs anyway).
  const VI shift_mnt = Max(k51 - exp, k0);
  const VI shift_int = Max(exp - k51, k0);
  const VI mantissa = BitCast(di, v) & Set(di, (1ULL << 52) - 1);
  // Include implicit 1-bit; shift by one more to ensure it's in the mantissa.
  const VI int52 = (mantissa | Set(di, 1ULL << 52)) >> (shift_mnt + k1);
  // For inputs larger than 2^52, insert zeros at the bottom.
  const VI shifted = int52 << shift_int;
  // Restore the one bit lost when shifting in the implicit 1-bit.
  const VI restored = shifted | ((mantissa & k1) << (shift_int - k1));

  // Saturate to LimitsMin (unchanged when negating below) or LimitsMax.
  const VI sign_mask = BroadcastSignBit(BitCast(di, v));
  const VI limit = Set(di, LimitsMax<int64_t>()) - sign_mask;
  const VI magnitude = IfThenElse(in_range, restored, limit);

  // If the input was negative, negate the integer (two's complement).
  return (magnitude ^ sign_mask) - sign_mask;
#endif
}

HWY_API Vec256<int32_t> NearestInt(const Vec256<float> v) {
  const Full256<int32_t> di;
  return detail::FixConversionOverflow(di, v, _mm256_cvtps_epi32(v.raw));
}


HWY_API Vec256<float> PromoteTo(Full256<float> df32,
                                const Vec128<float16_t> v) {
#ifdef HWY_DISABLE_F16C
  const RebindToSigned<decltype(df32)> di32;
  const RebindToUnsigned<decltype(df32)> du32;
  // Expand to u32 so we can shift.
  const auto bits16 = PromoteTo(du32, Vec128<uint16_t>{v.raw});
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
  return Vec256<float>{_mm256_cvtph_ps(v.raw)};
#endif
}

// ================================================== CRYPTO

#if !defined(HWY_DISABLE_PCLMUL_AES)

// Per-target flag to prevent generic_ops-inl.h from defining AESRound.
#ifdef HWY_NATIVE_AES
#undef HWY_NATIVE_AES
#else
#define HWY_NATIVE_AES
#endif

HWY_API Vec256<uint8_t> AESRound(Vec256<uint8_t> state,
                                 Vec256<uint8_t> round_key) {
#if HWY_TARGET == HWY_AVX3_DL
  return Vec256<uint8_t>{_mm256_aesenc_epi128(state.raw, round_key.raw)};
#else
  const Full256<uint8_t> d;
  const Half<decltype(d)> d2;
  return Combine(d, AESRound(UpperHalf(d2, state), UpperHalf(d2, round_key)),
                 AESRound(LowerHalf(state), LowerHalf(round_key)));
#endif
}

HWY_API Vec256<uint64_t> CLMulLower(Vec256<uint64_t> a, Vec256<uint64_t> b) {
#if HWY_TARGET == HWY_AVX3_DL
  return Vec256<uint64_t>{_mm256_clmulepi64_epi128(a.raw, b.raw, 0x00)};
#else
  const Full256<uint64_t> d;
  const Half<decltype(d)> d2;
  return Combine(d, CLMulLower(UpperHalf(d2, a), UpperHalf(d2, b)),
                 CLMulLower(LowerHalf(a), LowerHalf(b)));
#endif
}

HWY_API Vec256<uint64_t> CLMulUpper(Vec256<uint64_t> a, Vec256<uint64_t> b) {
#if HWY_TARGET == HWY_AVX3_DL
  return Vec256<uint64_t>{_mm256_clmulepi64_epi128(a.raw, b.raw, 0x11)};
#else
  const Full256<uint64_t> d;
  const Half<decltype(d)> d2;
  return Combine(d, CLMulUpper(UpperHalf(d2, a), UpperHalf(d2, b)),
                 CLMulUpper(LowerHalf(a), LowerHalf(b)));
#endif
}

#endif  // HWY_DISABLE_PCLMUL_AES

// ================================================== MISC

// Returns a vector with lane i=[0, N) set to "first" + i.
template <typename T, typename T2>
HWY_API Vec256<T> Iota(const Full256<T> d, const T2 first) {
  HWY_ALIGN T lanes[32 / sizeof(T)];
  for (size_t i = 0; i < 32 / sizeof(T); ++i) {
    lanes[i] = static_cast<T>(first + static_cast<T2>(i));
  }
  return Load(d, lanes);
}

#if HWY_TARGET <= HWY_AVX3

// ------------------------------ LoadMaskBits

// `p` points to at least 8 readable bytes, not all of which need be valid.
template <typename T>
HWY_API Mask256<T> LoadMaskBits(const Full256<T> /* tag */,
                                const uint8_t* HWY_RESTRICT bits) {
  constexpr size_t N = 32 / sizeof(T);
  constexpr size_t kNumBytes = (N + 7) / 8;

  uint64_t mask_bits = 0;
  CopyBytes<kNumBytes>(bits, &mask_bits);

  if (N < 8) {
    mask_bits &= (1ull << N) - 1;
  }

  return Mask256<T>::FromBits(mask_bits);
}

// ------------------------------ StoreMaskBits

// `p` points to at least 8 writable bytes.
template <typename T>
HWY_API size_t StoreMaskBits(const Full256<T> /* tag */, const Mask256<T> mask,
                             uint8_t* bits) {
  constexpr size_t N = 32 / sizeof(T);
  constexpr size_t kNumBytes = (N + 7) / 8;

  CopyBytes<kNumBytes>(&mask.raw, bits);

  // Non-full byte, need to clear the undefined upper bits.
  if (N < 8) {
    const int mask = (1ull << N) - 1;
    bits[0] = static_cast<uint8_t>(bits[0] & mask);
  }
  return kNumBytes;
}

// ------------------------------ Mask testing

template <typename T>
HWY_API size_t CountTrue(const Full256<T> /* tag */, const Mask256<T> mask) {
  return PopCount(static_cast<uint64_t>(mask.raw));
}

template <typename T>
HWY_API intptr_t FindFirstTrue(const Full256<T> /* tag */,
                               const Mask256<T> mask) {
  return mask.raw ? intptr_t(Num0BitsBelowLS1Bit_Nonzero32(mask.raw)) : -1;
}

// Beware: the suffix indicates the number of mask bits, not lane size!

namespace detail {

template <typename T>
HWY_INLINE bool AllFalse(hwy::SizeTag<1> /*tag*/, const Mask256<T> mask) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return _kortestz_mask32_u8(mask.raw, mask.raw);
#else
  return mask.raw == 0;
#endif
}
template <typename T>
HWY_INLINE bool AllFalse(hwy::SizeTag<2> /*tag*/, const Mask256<T> mask) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return _kortestz_mask16_u8(mask.raw, mask.raw);
#else
  return mask.raw == 0;
#endif
}
template <typename T>
HWY_INLINE bool AllFalse(hwy::SizeTag<4> /*tag*/, const Mask256<T> mask) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return _kortestz_mask8_u8(mask.raw, mask.raw);
#else
  return mask.raw == 0;
#endif
}
template <typename T>
HWY_INLINE bool AllFalse(hwy::SizeTag<8> /*tag*/, const Mask256<T> mask) {
  return (uint64_t{mask.raw} & 0xF) == 0;
}

}  // namespace detail

template <typename T>
HWY_API bool AllFalse(const Full256<T> /* tag */, const Mask256<T> mask) {
  return detail::AllFalse(hwy::SizeTag<sizeof(T)>(), mask);
}

namespace detail {

template <typename T>
HWY_INLINE bool AllTrue(hwy::SizeTag<1> /*tag*/, const Mask256<T> mask) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return _kortestc_mask32_u8(mask.raw, mask.raw);
#else
  return mask.raw == 0xFFFFFFFFu;
#endif
}
template <typename T>
HWY_INLINE bool AllTrue(hwy::SizeTag<2> /*tag*/, const Mask256<T> mask) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return _kortestc_mask16_u8(mask.raw, mask.raw);
#else
  return mask.raw == 0xFFFFu;
#endif
}
template <typename T>
HWY_INLINE bool AllTrue(hwy::SizeTag<4> /*tag*/, const Mask256<T> mask) {
#if HWY_COMPILER_HAS_MASK_INTRINSICS
  return _kortestc_mask8_u8(mask.raw, mask.raw);
#else
  return mask.raw == 0xFFu;
#endif
}
template <typename T>
HWY_INLINE bool AllTrue(hwy::SizeTag<8> /*tag*/, const Mask256<T> mask) {
  // Cannot use _kortestc because we have less than 8 mask bits.
  return mask.raw == 0xFu;
}

}  // namespace detail

template <typename T>
HWY_API bool AllTrue(const Full256<T> /* tag */, const Mask256<T> mask) {
  return detail::AllTrue(hwy::SizeTag<sizeof(T)>(), mask);
}

// ------------------------------ Compress

// 16-bit is defined in x86_512 so we can use 512-bit vectors.

template <typename T, HWY_IF_LANE_SIZE(T, 4)>
HWY_API Vec256<T> Compress(Vec256<T> v, Mask256<T> mask) {
  return Vec256<T>{_mm256_maskz_compress_epi32(mask.raw, v.raw)};
}

template <typename T, HWY_IF_LANE_SIZE(T, 8)>
HWY_API Vec256<T> Compress(Vec256<T> v, Mask256<T> mask) {
  return Vec256<T>{_mm256_maskz_compress_epi64(mask.raw, v.raw)};
}

HWY_API Vec256<float> Compress(Vec256<float> v, Mask256<float> mask) {
  return Vec256<float>{_mm256_maskz_compress_ps(mask.raw, v.raw)};
}

HWY_API Vec256<double> Compress(Vec256<double> v, Mask256<double> mask) {
  return Vec256<double>{_mm256_maskz_compress_pd(mask.raw, v.raw)};
}

// ------------------------------ CompressBits (LoadMaskBits)

template <typename T>
HWY_API Vec256<T> CompressBits(Vec256<T> v, const uint8_t* HWY_RESTRICT bits) {
  return Compress(v, LoadMaskBits(Full256<T>(), bits));
}

// ------------------------------ CompressStore

template <typename T, HWY_IF_LANE_SIZE(T, 2)>
HWY_API size_t CompressStore(Vec256<T> v, Mask256<T> mask, Full256<T> d,
                             T* HWY_RESTRICT unaligned) {
  const Rebind<uint16_t, decltype(d)> du;
  const auto vu = BitCast(du, v);  // (required for float16_t inputs)

  const uint64_t mask_bits{mask.raw};

#if HWY_TARGET == HWY_AVX3_DL  // VBMI2
  _mm256_mask_compressstoreu_epi16(unaligned, mask.raw, v.raw);
#else
  // Split into halves to keep the table size manageable.
  const Half<decltype(du)> duh;
  const auto vL = LowerHalf(duh, vu);
  const auto vH = UpperHalf(duh, vu);

  const uint64_t mask_bitsL = mask_bits & 0xFF;
  const uint64_t mask_bitsH = mask_bits >> 8;

  const auto idxL = detail::IndicesForCompress16(mask_bitsL);
  const auto idxH = detail::IndicesForCompress16(mask_bitsH);

  // Compress and 128-bit halves.
  const Vec128<uint16_t> cL{_mm_permutexvar_epi16(idxL.raw, vL.raw)};
  const Vec128<uint16_t> cH{_mm_permutexvar_epi16(idxH.raw, vH.raw)};
  const Half<decltype(d)> dh;
  StoreU(BitCast(dh, cL), dh, unaligned);
  StoreU(BitCast(dh, cH), dh, unaligned + PopCount(mask_bitsL));
#endif  // HWY_TARGET == HWY_AVX3_DL

  return PopCount(mask_bits);
}

template <typename T, HWY_IF_LANE_SIZE(T, 4)>
HWY_API size_t CompressStore(Vec256<T> v, Mask256<T> mask, Full256<T> /* tag */,
                             T* HWY_RESTRICT unaligned) {
  _mm256_mask_compressstoreu_epi32(unaligned, mask.raw, v.raw);
  return PopCount(uint64_t{mask.raw});
}

template <typename T, HWY_IF_LANE_SIZE(T, 8)>
HWY_API size_t CompressStore(Vec256<T> v, Mask256<T> mask, Full256<T> /* tag */,
                             T* HWY_RESTRICT unaligned) {
  _mm256_mask_compressstoreu_epi64(unaligned, mask.raw, v.raw);
  return PopCount(uint64_t{mask.raw} & 0xFull);
}

HWY_API size_t CompressStore(Vec256<float> v, Mask256<float> mask,
                             Full256<float> /* tag */,
                             float* HWY_RESTRICT unaligned) {
  _mm256_mask_compressstoreu_ps(unaligned, mask.raw, v.raw);
  return PopCount(uint64_t{mask.raw});
}

HWY_API size_t CompressStore(Vec256<double> v, Mask256<double> mask,
                             Full256<double> /* tag */,
                             double* HWY_RESTRICT unaligned) {
  _mm256_mask_compressstoreu_pd(unaligned, mask.raw, v.raw);
  return PopCount(uint64_t{mask.raw} & 0xFull);
}

// ------------------------------ CompressBitsStore (LoadMaskBits)

template <typename T>
HWY_API size_t CompressBitsStore(Vec256<T> v, const uint8_t* HWY_RESTRICT bits,
                                 Full256<T> d, T* HWY_RESTRICT unaligned) {
  return CompressStore(v, LoadMaskBits(d, bits), d, unaligned);
}

#else  // AVX2

// ------------------------------ LoadMaskBits (TestBit)

namespace detail {

// 256 suffix avoids ambiguity with x86_128 without needing HWY_IF_LE128 there.
template <typename T, HWY_IF_LANE_SIZE(T, 1)>
HWY_INLINE Mask256<T> LoadMaskBits256(Full256<T> d, uint64_t mask_bits) {
  const RebindToUnsigned<decltype(d)> du;
  const Repartition<uint32_t, decltype(d)> du32;
  const auto vbits = BitCast(du, Set(du32, static_cast<uint32_t>(mask_bits)));

  // Replicate bytes 8x such that each byte contains the bit that governs it.
  const Repartition<uint64_t, decltype(d)> du64;
  alignas(32) constexpr uint64_t kRep8[4] = {
      0x0000000000000000ull, 0x0101010101010101ull, 0x0202020202020202ull,
      0x0303030303030303ull};
  const auto rep8 = TableLookupBytes(vbits, BitCast(du, Load(du64, kRep8)));

  alignas(32) constexpr uint8_t kBit[16] = {1, 2, 4, 8, 16, 32, 64, 128,
                                            1, 2, 4, 8, 16, 32, 64, 128};
  return RebindMask(d, TestBit(rep8, LoadDup128(du, kBit)));
}

template <typename T, HWY_IF_LANE_SIZE(T, 2)>
HWY_INLINE Mask256<T> LoadMaskBits256(Full256<T> d, uint64_t mask_bits) {
  const RebindToUnsigned<decltype(d)> du;
  alignas(32) constexpr uint16_t kBit[16] = {
      1,     2,     4,     8,     16,     32,     64,     128,
      0x100, 0x200, 0x400, 0x800, 0x1000, 0x2000, 0x4000, 0x8000};
  const auto vmask_bits = Set(du, static_cast<uint16_t>(mask_bits));
  return RebindMask(d, TestBit(vmask_bits, Load(du, kBit)));
}

template <typename T, HWY_IF_LANE_SIZE(T, 4)>
HWY_INLINE Mask256<T> LoadMaskBits256(Full256<T> d, uint64_t mask_bits) {
  const RebindToUnsigned<decltype(d)> du;
  constexpr uint32_t kBit[8] = {1, 2, 4, 8, 16, 32, 64, 128};
  const auto vmask_bits = Set(du, static_cast<uint32_t>(mask_bits));
  return RebindMask(d, TestBit(vmask_bits, Load(du, kBit)));
}

template <typename T, HWY_IF_LANE_SIZE(T, 8)>
HWY_INLINE Mask256<T> LoadMaskBits256(Full256<T> d, uint64_t mask_bits) {
  const RebindToUnsigned<decltype(d)> du;
  constexpr uint64_t kBit[8] = {1, 2, 4, 8};
  return RebindMask(d, TestBit(Set(du, mask_bits), Load(du, kBit)));
}

}  // namespace detail

// `p` points to at least 8 readable bytes, not all of which need be valid.
template <typename T>
HWY_API Mask256<T> LoadMaskBits(Full256<T> d,
                                const uint8_t* HWY_RESTRICT bits) {
  constexpr size_t N = 32 / sizeof(T);
  constexpr size_t kNumBytes = (N + 7) / 8;

  uint64_t mask_bits = 0;
  CopyBytes<kNumBytes>(bits, &mask_bits);

  if (N < 8) {
    mask_bits &= (1ull << N) - 1;
  }

  return detail::LoadMaskBits256(d, mask_bits);
}

// ------------------------------ StoreMaskBits

namespace detail {

template <typename T>
HWY_INLINE uint64_t BitsFromMask(hwy::SizeTag<1> /*tag*/,
                                 const Mask256<T> mask) {
  const Full256<T> d;
  const Full256<uint8_t> d8;
  const auto sign_bits = BitCast(d8, VecFromMask(d, mask)).raw;
  // Prevent sign-extension of 32-bit masks because the intrinsic returns int.
  return static_cast<uint32_t>(_mm256_movemask_epi8(sign_bits));
}

template <typename T>
HWY_INLINE uint64_t BitsFromMask(hwy::SizeTag<2> /*tag*/,
                                 const Mask256<T> mask) {
#if HWY_ARCH_X86_64
  const uint64_t sign_bits8 = BitsFromMask(hwy::SizeTag<1>(), mask);
  // Skip the bits from the lower byte of each u16 (better not to use the
  // same packs_epi16 as SSE4, because that requires an extra swizzle here).
  return _pext_u64(sign_bits8, 0xAAAAAAAAull);
#else
  // Slow workaround for 32-bit builds, which lack _pext_u64.
  // Remove useless lower half of each u16 while preserving the sign bit.
  // Bytes [0, 8) and [16, 24) have the same sign bits as the input lanes.
  const auto sign_bits = _mm256_packs_epi16(mask.raw, _mm256_setzero_si256());
  // Move odd qwords (value zero) to top so they don't affect the mask value.
  const auto compressed =
      _mm256_permute4x64_epi64(sign_bits, _MM_SHUFFLE(3, 1, 2, 0));
  return static_cast<unsigned>(_mm256_movemask_epi8(compressed));
#endif  // HWY_ARCH_X86_64
}

template <typename T>
HWY_INLINE uint64_t BitsFromMask(hwy::SizeTag<4> /*tag*/,
                                 const Mask256<T> mask) {
  const Full256<T> d;
  const Full256<float> df;
  const auto sign_bits = BitCast(df, VecFromMask(d, mask)).raw;
  return static_cast<unsigned>(_mm256_movemask_ps(sign_bits));
}

template <typename T>
HWY_INLINE uint64_t BitsFromMask(hwy::SizeTag<8> /*tag*/,
                                 const Mask256<T> mask) {
  const Full256<T> d;
  const Full256<double> df;
  const auto sign_bits = BitCast(df, VecFromMask(d, mask)).raw;
  return static_cast<unsigned>(_mm256_movemask_pd(sign_bits));
}

template <typename T>
HWY_INLINE uint64_t BitsFromMask(const Mask256<T> mask) {
  return BitsFromMask(hwy::SizeTag<sizeof(T)>(), mask);
}

}  // namespace detail

// `p` points to at least 8 writable bytes.
template <typename T>
HWY_API size_t StoreMaskBits(const Full256<T> /* tag */, const Mask256<T> mask,
                             uint8_t* bits) {
  constexpr size_t N = 32 / sizeof(T);
  constexpr size_t kNumBytes = (N + 7) / 8;

  const uint64_t mask_bits = detail::BitsFromMask(mask);
  CopyBytes<kNumBytes>(&mask_bits, bits);
  return kNumBytes;
}

// ------------------------------ Mask testing

template <typename T>
HWY_API bool AllFalse(const Full256<T> /* tag */, const Mask256<T> mask) {
  // Cheaper than PTEST, which is 2 uop / 3L.
  return detail::BitsFromMask(mask) == 0;
}

template <typename T>
HWY_API bool AllTrue(const Full256<T> /* tag */, const Mask256<T> mask) {
  constexpr uint64_t kAllBits = (1ull << (32 / sizeof(T))) - 1;
  return detail::BitsFromMask(mask) == kAllBits;
}

template <typename T>
HWY_API size_t CountTrue(const Full256<T> /* tag */, const Mask256<T> mask) {
  return PopCount(detail::BitsFromMask(mask));
}

template <typename T>
HWY_API intptr_t FindFirstTrue(const Full256<T> /* tag */,
                               const Mask256<T> mask) {
  const uint64_t mask_bits = detail::BitsFromMask(mask);
  return mask_bits ? intptr_t(Num0BitsBelowLS1Bit_Nonzero64(mask_bits)) : -1;
}

// ------------------------------ Compress, CompressBits

namespace detail {

template <typename T, HWY_IF_LANE_SIZE(T, 4)>
HWY_INLINE Indices256<uint32_t> IndicesFromBits(Simd<T, 8> d,
                                                uint64_t mask_bits) {
  const RebindToUnsigned<decltype(d)> d32;
  // We need a masked Iota(). With 8 lanes, there are 256 combinations and a LUT
  // of SetTableIndices would require 8 KiB, a large part of L1D. The other
  // alternative is _pext_u64, but this is extremely slow on Zen2 (18 cycles)
  // and unavailable in 32-bit builds. We instead compress each index into 4
  // bits, for a total of 1 KiB.
  alignas(16) constexpr uint32_t packed_array[256] = {
      0x00000000, 0x00000000, 0x00000001, 0x00000010, 0x00000002, 0x00000020,
      0x00000021, 0x00000210, 0x00000003, 0x00000030, 0x00000031, 0x00000310,
      0x00000032, 0x00000320, 0x00000321, 0x00003210, 0x00000004, 0x00000040,
      0x00000041, 0x00000410, 0x00000042, 0x00000420, 0x00000421, 0x00004210,
      0x00000043, 0x00000430, 0x00000431, 0x00004310, 0x00000432, 0x00004320,
      0x00004321, 0x00043210, 0x00000005, 0x00000050, 0x00000051, 0x00000510,
      0x00000052, 0x00000520, 0x00000521, 0x00005210, 0x00000053, 0x00000530,
      0x00000531, 0x00005310, 0x00000532, 0x00005320, 0x00005321, 0x00053210,
      0x00000054, 0x00000540, 0x00000541, 0x00005410, 0x00000542, 0x00005420,
      0x00005421, 0x00054210, 0x00000543, 0x00005430, 0x00005431, 0x00054310,
      0x00005432, 0x00054320, 0x00054321, 0x00543210, 0x00000006, 0x00000060,
      0x00000061, 0x00000610, 0x00000062, 0x00000620, 0x00000621, 0x00006210,
      0x00000063, 0x00000630, 0x00000631, 0x00006310, 0x00000632, 0x00006320,
      0x00006321, 0x00063210, 0x00000064, 0x00000640, 0x00000641, 0x00006410,
      0x00000642, 0x00006420, 0x00006421, 0x00064210, 0x00000643, 0x00006430,
      0x00006431, 0x00064310, 0x00006432, 0x00064320, 0x00064321, 0x00643210,
      0x00000065, 0x00000650, 0x00000651, 0x00006510, 0x00000652, 0x00006520,
      0x00006521, 0x00065210, 0x00000653, 0x00006530, 0x00006531, 0x00065310,
      0x00006532, 0x00065320, 0x00065321, 0x00653210, 0x00000654, 0x00006540,
      0x00006541, 0x00065410, 0x00006542, 0x00065420, 0x00065421, 0x00654210,
      0x00006543, 0x00065430, 0x00065431, 0x00654310, 0x00065432, 0x00654320,
      0x00654321, 0x06543210, 0x00000007, 0x00000070, 0x00000071, 0x00000710,
      0x00000072, 0x00000720, 0x00000721, 0x00007210, 0x00000073, 0x00000730,
      0x00000731, 0x00007310, 0x00000732, 0x00007320, 0x00007321, 0x00073210,
      0x00000074, 0x00000740, 0x00000741, 0x00007410, 0x00000742, 0x00007420,
      0x00007421, 0x00074210, 0x00000743, 0x00007430, 0x00007431, 0x00074310,
      0x00007432, 0x00074320, 0x00074321, 0x00743210, 0x00000075, 0x00000750,
      0x00000751, 0x00007510, 0x00000752, 0x00007520, 0x00007521, 0x00075210,
      0x00000753, 0x00007530, 0x00007531, 0x00075310, 0x00007532, 0x00075320,
      0x00075321, 0x00753210, 0x00000754, 0x00007540, 0x00007541, 0x00075410,
      0x00007542, 0x00075420, 0x00075421, 0x00754210, 0x00007543, 0x00075430,
      0x00075431, 0x00754310, 0x00075432, 0x00754320, 0x00754321, 0x07543210,
      0x00000076, 0x00000760, 0x00000761, 0x00007610, 0x00000762, 0x00007620,
      0x00007621, 0x00076210, 0x00000763, 0x00007630, 0x00007631, 0x00076310,
      0x00007632, 0x00076320, 0x00076321, 0x00763210, 0x00000764, 0x00007640,
      0x00007641, 0x00076410, 0x00007642, 0x00076420, 0x00076421, 0x00764210,
      0x00007643, 0x00076430, 0x00076431, 0x00764310, 0x00076432, 0x00764320,
      0x00764321, 0x07643210, 0x00000765, 0x00007650, 0x00007651, 0x00076510,
      0x00007652, 0x00076520, 0x00076521, 0x00765210, 0x00007653, 0x00076530,
      0x00076531, 0x00765310, 0x00076532, 0x00765320, 0x00765321, 0x07653210,
      0x00007654, 0x00076540, 0x00076541, 0x00765410, 0x00076542, 0x00765420,
      0x00765421, 0x07654210, 0x00076543, 0x00765430, 0x00765431, 0x07654310,
      0x00765432, 0x07654320, 0x07654321, 0x76543210};

  // No need to mask because _mm256_permutevar8x32_epi32 ignores bits 3..31.
  // Just shift each copy of the 32 bit LUT to extract its 4-bit fields.
  // If broadcasting 32-bit from memory incurs the 3-cycle block-crossing
  // latency, it may be faster to use LoadDup128 and PSHUFB.
  const auto packed = Set(d32, packed_array[mask_bits]);
  alignas(32) constexpr uint32_t shifts[8] = {0, 4, 8, 12, 16, 20, 24, 28};
  return Indices256<uint32_t>{(packed >> Load(d32, shifts)).raw};
}

template <typename T, HWY_IF_LANE_SIZE(T, 8)>
HWY_INLINE Indices256<uint32_t> IndicesFromBits(Simd<T, 4> d,
                                                uint64_t mask_bits) {
  const Repartition<uint32_t, decltype(d)> d32;

  // For 64-bit, we still need 32-bit indices because there is no 64-bit
  // permutevar, but there are only 4 lanes, so we can afford to skip the
  // unpacking and load the entire index vector directly.
  alignas(32) constexpr uint32_t packed_array[128] = {
      0, 1, 0, 1, 0, 1, 0, 1, /**/ 0, 1, 0, 1, 0, 1, 0, 1,  //
      2, 3, 0, 1, 0, 1, 0, 1, /**/ 0, 1, 2, 3, 0, 1, 0, 1,  //
      4, 5, 0, 1, 0, 1, 0, 1, /**/ 0, 1, 4, 5, 0, 1, 0, 1,  //
      2, 3, 4, 5, 0, 1, 0, 1, /**/ 0, 1, 2, 3, 4, 5, 0, 1,  //
      6, 7, 0, 1, 0, 1, 0, 1, /**/ 0, 1, 6, 7, 0, 1, 0, 1,  //
      2, 3, 6, 7, 0, 1, 0, 1, /**/ 0, 1, 2, 3, 6, 7, 0, 1,  //
      4, 5, 6, 7, 0, 1, 0, 1, /**/ 0, 1, 4, 5, 6, 7, 0, 1,
      2, 3, 4, 5, 6, 7, 0, 1, /**/ 0, 1, 2, 3, 4, 5, 6, 7};
  return Indices256<uint32_t>{Load(d32, packed_array + 8 * mask_bits).raw};
}

template <typename T, HWY_IF_NOT_LANE_SIZE(T, 2)>
HWY_INLINE Vec256<T> Compress(Vec256<T> v, const uint64_t mask_bits) {
  const Full256<T> d;
  const Repartition<uint32_t, decltype(d)> du32;

  HWY_DASSERT(mask_bits < (1ull << (32 / sizeof(T))));
  const auto indices = IndicesFromBits(d, mask_bits);
  return BitCast(d, TableLookupLanes(BitCast(du32, v), indices));
}

// LUTs are infeasible for 2^16 possible masks. Promoting to 32-bit and using
// the native Compress is probably more efficient than 2 LUTs.
template <typename T, HWY_IF_LANE_SIZE(T, 2)>
HWY_INLINE Vec256<T> Compress(Vec256<T> v, const uint64_t mask_bits) {
  using D = Full256<T>;
  const Rebind<uint16_t, D> du;
  const Repartition<int32_t, D> dw;
  const auto vu16 = BitCast(du, v);  // (required for float16_t inputs)
  const auto promoted0 = PromoteTo(dw, LowerHalf(vu16));
  const auto promoted1 = PromoteTo(dw, UpperHalf(Half<decltype(du)>(), vu16));

  const uint64_t mask_bits0 = mask_bits & 0xFF;
  const uint64_t mask_bits1 = mask_bits >> 8;
  const auto compressed0 = Compress(promoted0, mask_bits0);
  const auto compressed1 = Compress(promoted1, mask_bits1);

  const Half<decltype(du)> dh;
  const auto demoted0 = ZeroExtendVector(DemoteTo(dh, compressed0));
  const auto demoted1 = ZeroExtendVector(DemoteTo(dh, compressed1));

  const size_t count0 = PopCount(mask_bits0);
  // Now combine by shifting demoted1 up. AVX2 lacks VPERMW, so start with
  // VPERMD for shifting at 4 byte granularity.
  alignas(32) constexpr int32_t iota4[16] = {0, 0, 0, 0, 0, 0, 0, 0,
                                             0, 1, 2, 3, 4, 5, 6, 7};
  const auto indices = SetTableIndices(dw, iota4 + 8 - count0 / 2);
  const auto shift1_multiple4 =
      BitCast(du, TableLookupLanes(BitCast(dw, demoted1), indices));

  // Whole-register unconditional shift by 2 bytes.
  // TODO(janwas): slow on AMD, use 2 shifts + permq + OR instead?
  const __m256i lo_zz = _mm256_permute2x128_si256(shift1_multiple4.raw,
                                                  shift1_multiple4.raw, 0x08);
  const auto shift1_multiple2 =
      Vec256<uint16_t>{_mm256_alignr_epi8(shift1_multiple4.raw, lo_zz, 14)};

  // Make the shift conditional on the lower bit of count0.
  const auto m_odd =
      TestBit(Set(du, static_cast<uint16_t>(count0)), Set(du, 1));
  const auto shifted1 = IfThenElse(m_odd, shift1_multiple2, shift1_multiple4);

  // Blend the lower and shifted upper parts.
  constexpr uint16_t on = 0xFFFF;
  alignas(32) constexpr uint16_t lower_lanes[32] = {HWY_REP4(on), HWY_REP4(on),
                                                    HWY_REP4(on), HWY_REP4(on)};
  const auto m_lower = MaskFromVec(LoadU(du, lower_lanes + 16 - count0));
  return BitCast(D(), IfThenElse(m_lower, demoted0, shifted1));
}

}  // namespace detail

template <typename T>
HWY_API Vec256<T> Compress(Vec256<T> v, Mask256<T> m) {
  const uint64_t mask_bits = detail::BitsFromMask(m);
  return detail::Compress(v, mask_bits);
}

template <typename T>
HWY_API Vec256<T> CompressBits(Vec256<T> v, const uint8_t* HWY_RESTRICT bits) {
  constexpr size_t N = 32 / sizeof(T);
  constexpr size_t kNumBytes = (N + 7) / 8;

  uint64_t mask_bits = 0;
  CopyBytes<kNumBytes>(bits, &mask_bits);

  if (N < 8) {
    mask_bits &= (1ull << N) - 1;
  }

  return detail::Compress(v, mask_bits);
}

// ------------------------------ CompressStore, CompressBitsStore

template <typename T>
HWY_API size_t CompressStore(Vec256<T> v, Mask256<T> m, Full256<T> d,
                             T* HWY_RESTRICT unaligned) {
  const uint64_t mask_bits = detail::BitsFromMask(m);
  StoreU(detail::Compress(v, mask_bits), d, unaligned);
  return PopCount(mask_bits);
}

template <typename T>
HWY_API size_t CompressBitsStore(Vec256<T> v, const uint8_t* HWY_RESTRICT bits,
                                 Full256<T> d, T* HWY_RESTRICT unaligned) {
  constexpr size_t N = 32 / sizeof(T);
  constexpr size_t kNumBytes = (N + 7) / 8;

  uint64_t mask_bits = 0;
  CopyBytes<kNumBytes>(bits, &mask_bits);

  if (N < 8) {
    mask_bits &= (1ull << N) - 1;
  }

  StoreU(detail::Compress(v, mask_bits), d, unaligned);
  return PopCount(mask_bits);
}

#endif  // HWY_TARGET <= HWY_AVX3

// ------------------------------ StoreInterleaved3 (CombineShiftRightBytes,
// TableLookupBytes, ConcatUpperLower)

HWY_API void StoreInterleaved3(const Vec256<uint8_t> v0,
                               const Vec256<uint8_t> v1,
                               const Vec256<uint8_t> v2, Full256<uint8_t> d,
                               uint8_t* HWY_RESTRICT unaligned) {
  const auto k5 = Set(d, 5);
  const auto k6 = Set(d, 6);

  // Shuffle (v0,v1,v2) vector bytes to (MSB on left): r5, bgr[4:0].
  // 0x80 so lanes to be filled from other vectors are 0 for blending.
  alignas(16) static constexpr uint8_t tbl_r0[16] = {
      0, 0x80, 0x80, 1, 0x80, 0x80, 2, 0x80, 0x80,  //
      3, 0x80, 0x80, 4, 0x80, 0x80, 5};
  alignas(16) static constexpr uint8_t tbl_g0[16] = {
      0x80, 0, 0x80, 0x80, 1, 0x80,  //
      0x80, 2, 0x80, 0x80, 3, 0x80, 0x80, 4, 0x80, 0x80};
  const auto shuf_r0 = LoadDup128(d, tbl_r0);
  const auto shuf_g0 = LoadDup128(d, tbl_g0);  // cannot reuse r0 due to 5
  const auto shuf_b0 = CombineShiftRightBytes<15>(d, shuf_g0, shuf_g0);
  const auto r0 = TableLookupBytes(v0, shuf_r0);  // 5..4..3..2..1..0
  const auto g0 = TableLookupBytes(v1, shuf_g0);  // ..4..3..2..1..0.
  const auto b0 = TableLookupBytes(v2, shuf_b0);  // .4..3..2..1..0..
  const auto interleaved_10_00 = r0 | g0 | b0;

  // Second vector: g10,r10, bgr[9:6], b5,g5
  const auto shuf_r1 = shuf_b0 + k6;  // .A..9..8..7..6..
  const auto shuf_g1 = shuf_r0 + k5;  // A..9..8..7..6..5
  const auto shuf_b1 = shuf_g0 + k5;  // ..9..8..7..6..5.
  const auto r1 = TableLookupBytes(v0, shuf_r1);
  const auto g1 = TableLookupBytes(v1, shuf_g1);
  const auto b1 = TableLookupBytes(v2, shuf_b1);
  const auto interleaved_15_05 = r1 | g1 | b1;

  // We want to write the lower halves of the interleaved vectors, then the
  // upper halves. We could obtain 10_05 and 15_0A via ConcatUpperLower, but
  // that would require two ununaligned stores. For the lower halves, we can
  // merge two 128-bit stores for the same swizzling cost:
  const auto out0 = ConcatLowerLower(d, interleaved_15_05, interleaved_10_00);
  StoreU(out0, d, unaligned + 0 * 32);

  // Third vector: bgr[15:11], b10
  const auto shuf_r2 = shuf_b1 + k6;  // ..F..E..D..C..B.
  const auto shuf_g2 = shuf_r1 + k5;  // .F..E..D..C..B..
  const auto shuf_b2 = shuf_g1 + k5;  // F..E..D..C..B..A
  const auto r2 = TableLookupBytes(v0, shuf_r2);
  const auto g2 = TableLookupBytes(v1, shuf_g2);
  const auto b2 = TableLookupBytes(v2, shuf_b2);
  const auto interleaved_1A_0A = r2 | g2 | b2;

  const auto out1 = ConcatUpperLower(d, interleaved_10_00, interleaved_1A_0A);
  StoreU(out1, d, unaligned + 1 * 32);

  const auto out2 = ConcatUpperUpper(d, interleaved_1A_0A, interleaved_15_05);
  StoreU(out2, d, unaligned + 2 * 32);
}

// ------------------------------ StoreInterleaved4

HWY_API void StoreInterleaved4(const Vec256<uint8_t> v0,
                               const Vec256<uint8_t> v1,
                               const Vec256<uint8_t> v2,
                               const Vec256<uint8_t> v3, Full256<uint8_t> d8,
                               uint8_t* HWY_RESTRICT unaligned) {
  const RepartitionToWide<decltype(d8)> d16;
  const RepartitionToWide<decltype(d16)> d32;
  // let a,b,c,d denote v0..3.
  const auto ba0 = ZipLower(d16, v0, v1);  // b7 a7 .. b0 a0
  const auto dc0 = ZipLower(d16, v2, v3);  // d7 c7 .. d0 c0
  const auto ba8 = ZipUpper(d16, v0, v1);
  const auto dc8 = ZipUpper(d16, v2, v3);
  const auto dcba_0 = ZipLower(d32, ba0, dc0);  // d..a13 d..a10 | d..a03 d..a00
  const auto dcba_4 = ZipUpper(d32, ba0, dc0);  // d..a17 d..a14 | d..a07 d..a04
  const auto dcba_8 = ZipLower(d32, ba8, dc8);  // d..a1B d..a18 | d..a0B d..a08
  const auto dcba_C = ZipUpper(d32, ba8, dc8);  // d..a1F d..a1C | d..a0F d..a0C
  // Write lower halves, then upper. vperm2i128 is slow on Zen1 but we can
  // efficiently combine two lower halves into 256 bits:
  const auto out0 = BitCast(d8, ConcatLowerLower(d32, dcba_4, dcba_0));
  const auto out1 = BitCast(d8, ConcatLowerLower(d32, dcba_C, dcba_8));
  StoreU(out0, d8, unaligned + 0 * 32);
  StoreU(out1, d8, unaligned + 1 * 32);
  const auto out2 = BitCast(d8, ConcatUpperUpper(d32, dcba_4, dcba_0));
  const auto out3 = BitCast(d8, ConcatUpperUpper(d32, dcba_C, dcba_8));
  StoreU(out2, d8, unaligned + 2 * 32);
  StoreU(out3, d8, unaligned + 3 * 32);
}

// ------------------------------ Reductions

namespace detail {

// Returns sum{lane[i]} in each lane. "v3210" is a replicated 128-bit block.
// Same logic as x86/128.h, but with Vec256 arguments.
template <typename T>
HWY_INLINE Vec256<T> SumOfLanes(hwy::SizeTag<4> /* tag */,
                                const Vec256<T> v3210) {
  const auto v1032 = Shuffle1032(v3210);
  const auto v31_20_31_20 = v3210 + v1032;
  const auto v20_31_20_31 = Shuffle0321(v31_20_31_20);
  return v20_31_20_31 + v31_20_31_20;
}
template <typename T>
HWY_INLINE Vec256<T> MinOfLanes(hwy::SizeTag<4> /* tag */,
                                const Vec256<T> v3210) {
  const auto v1032 = Shuffle1032(v3210);
  const auto v31_20_31_20 = Min(v3210, v1032);
  const auto v20_31_20_31 = Shuffle0321(v31_20_31_20);
  return Min(v20_31_20_31, v31_20_31_20);
}
template <typename T>
HWY_INLINE Vec256<T> MaxOfLanes(hwy::SizeTag<4> /* tag */,
                                const Vec256<T> v3210) {
  const auto v1032 = Shuffle1032(v3210);
  const auto v31_20_31_20 = Max(v3210, v1032);
  const auto v20_31_20_31 = Shuffle0321(v31_20_31_20);
  return Max(v20_31_20_31, v31_20_31_20);
}

template <typename T>
HWY_INLINE Vec256<T> SumOfLanes(hwy::SizeTag<8> /* tag */,
                                const Vec256<T> v10) {
  const auto v01 = Shuffle01(v10);
  return v10 + v01;
}
template <typename T>
HWY_INLINE Vec256<T> MinOfLanes(hwy::SizeTag<8> /* tag */,
                                const Vec256<T> v10) {
  const auto v01 = Shuffle01(v10);
  return Min(v10, v01);
}
template <typename T>
HWY_INLINE Vec256<T> MaxOfLanes(hwy::SizeTag<8> /* tag */,
                                const Vec256<T> v10) {
  const auto v01 = Shuffle01(v10);
  return Max(v10, v01);
}

}  // namespace detail

// Supported for {uif}32x8, {uif}64x4. Returns the sum in each lane.
template <typename T>
HWY_API Vec256<T> SumOfLanes(Full256<T> d, const Vec256<T> vHL) {
  const Vec256<T> vLH = ConcatLowerUpper(d, vHL, vHL);
  return detail::SumOfLanes(hwy::SizeTag<sizeof(T)>(), vLH + vHL);
}
template <typename T>
HWY_API Vec256<T> MinOfLanes(Full256<T> d, const Vec256<T> vHL) {
  const Vec256<T> vLH = ConcatLowerUpper(d, vHL, vHL);
  return detail::MinOfLanes(hwy::SizeTag<sizeof(T)>(), Min(vLH, vHL));
}
template <typename T>
HWY_API Vec256<T> MaxOfLanes(Full256<T> d, const Vec256<T> vHL) {
  const Vec256<T> vLH = ConcatLowerUpper(d, vHL, vHL);
  return detail::MaxOfLanes(hwy::SizeTag<sizeof(T)>(), Max(vLH, vHL));
}

// ================================================== DEPRECATED

template <typename T>
HWY_API size_t StoreMaskBits(const Mask256<T> mask, uint8_t* bits) {
  return StoreMaskBits(Full256<T>(), mask, bits);
}

template <typename T>
HWY_API bool AllTrue(const Mask256<T> mask) {
  return AllTrue(Full256<T>(), mask);
}

template <typename T>
HWY_API bool AllFalse(const Mask256<T> mask) {
  return AllFalse(Full256<T>(), mask);
}

template <typename T>
HWY_API size_t CountTrue(const Mask256<T> mask) {
  return CountTrue(Full256<T>(), mask);
}

template <typename T>
HWY_API Vec256<T> SumOfLanes(const Vec256<T> vHL) {
  return SumOfLanes(Full256<T>(), vHL);
}
template <typename T>
HWY_API Vec256<T> MinOfLanes(const Vec256<T> vHL) {
  return MinOfLanes(Full256<T>(), vHL);
}
template <typename T>
HWY_API Vec256<T> MaxOfLanes(const Vec256<T> vHL) {
  return MaxOfLanes(Full256<T>(), vHL);
}

template <typename T>
HWY_API Vec128<T> UpperHalf(Vec256<T> v) {
  return UpperHalf(Full128<T>(), v);
}

template <int kBytes, typename T>
HWY_API Vec256<T> ShiftRightBytes(const Vec256<T> v) {
  return ShiftRightBytes<kBytes>(Full256<T>(), v);
}

template <int kLanes, typename T>
HWY_API Vec256<T> ShiftRightLanes(const Vec256<T> v) {
  return ShiftRightLanes<kLanes>(Full256<T>(), v);
}

template <size_t kBytes, typename T>
HWY_API Vec256<T> CombineShiftRightBytes(Vec256<T> hi, Vec256<T> lo) {
  return CombineShiftRightBytes<kBytes>(Full256<T>(), hi, lo);
}

template <typename T>
HWY_API Vec256<T> InterleaveUpper(Vec256<T> a, Vec256<T> b) {
  return InterleaveUpper(Full256<T>(), a, b);
}

template <typename T>
HWY_API Vec256<MakeWide<T>> ZipUpper(Vec256<T> a, Vec256<T> b) {
  return InterleaveUpper(Full256<MakeWide<T>>(), a, b);
}

template <typename T>
HWY_API Vec256<T> Combine(Vec128<T> hi, Vec128<T> lo) {
  return Combine(Full256<T>(), hi, lo);
}

template <typename T>
HWY_API Vec256<T> ZeroExtendVector(Vec128<T> lo) {
  return ZeroExtendVector(Full256<T>(), lo);
}

template <typename T>
HWY_API Vec256<T> ConcatLowerLower(Vec256<T> hi, Vec256<T> lo) {
  return ConcatLowerLower(Full256<T>(), hi, lo);
}

template <typename T>
HWY_API Vec256<T> ConcatLowerUpper(Vec256<T> hi, Vec256<T> lo) {
  return ConcatLowerUpper(Full256<T>(), hi, lo);
}

template <typename T>
HWY_API Vec256<T> ConcatUpperLower(Vec256<T> hi, Vec256<T> lo) {
  return ConcatUpperLower(Full256<T>(), hi, lo);
}

template <typename T>
HWY_API Vec256<T> ConcatUpperUpper(Vec256<T> hi, Vec256<T> lo) {
  return ConcatUpperUpper(Full256<T>(), hi, lo);
}

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace hwy
HWY_AFTER_NAMESPACE();
