// Copyright 2023 Google LLC
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

// 128-bit vectors for VSX
// External include guard in highway.h - see comment there.

#pragma push_macro("vector")
#pragma push_macro("pixel")
#pragma push_macro("bool")

#undef vector
#undef pixel
#undef bool

#include <altivec.h>

#pragma pop_macro("vector")
#pragma pop_macro("pixel")
#pragma pop_macro("bool")

#include <string.h>  // memcpy

#include "hwy/ops/shared-inl.h"

// clang's altivec.h gates some intrinsics behind #ifdef __POWER10_VECTOR__.
// This means we can only use POWER10-specific intrinsics in static dispatch
// mode (where the -mpower10-vector compiler flag is passed). Same for PPC9.
// On other compilers, the usual target check is sufficient.
#if HWY_TARGET <= HWY_PPC9 && \
    (!HWY_COMPILER_CLANG || defined(__POWER9_VECTOR__))
#define HWY_PPC_HAVE_9 1
#else
#define HWY_PPC_HAVE_9 0
#endif

#if HWY_TARGET <= HWY_PPC10 && \
    (!HWY_COMPILER_CLANG || defined(__POWER10_VECTOR__))
#define HWY_PPC_HAVE_10 1
#else
#define HWY_PPC_HAVE_10 0
#endif

HWY_BEFORE_NAMESPACE();
namespace hwy {
namespace HWY_NAMESPACE {
namespace detail {

template <typename T>
struct Raw128;

// Each Raw128 specialization defines the following typedefs:
// - type:
//   the backing Altivec/VSX raw vector type of the Vec128<T, N> type
// - RawBoolVec:
//   the backing Altivec/VSX raw __bool vector type of the Mask128<T, N> type
// - RawT:
//   the lane type for intrinsics, in particular vec_splat
// - AlignedRawVec:
//   the 128-bit GCC/Clang vector type for aligned loads/stores
// - UnalignedRawVec:
//   the 128-bit GCC/Clang vector type for unaligned loads/stores
#define HWY_VSX_RAW128(LANE_TYPE, RAW_VECT_LANE_TYPE, RAW_BOOL_VECT_LANE_TYPE) \
  template <>                                                                  \
  struct Raw128<LANE_TYPE> {                                                   \
    using type = __vector RAW_VECT_LANE_TYPE;                                  \
    using RawBoolVec = __vector __bool RAW_BOOL_VECT_LANE_TYPE;                \
    using RawT = RAW_VECT_LANE_TYPE;                                           \
    typedef LANE_TYPE AlignedRawVec                                            \
        __attribute__((__vector_size__(16), __aligned__(16), __may_alias__));  \
    typedef LANE_TYPE UnalignedRawVec __attribute__((                          \
        __vector_size__(16), __aligned__(alignof(LANE_TYPE)), __may_alias__)); \
  };

HWY_VSX_RAW128(int8_t, signed char, char)
HWY_VSX_RAW128(uint8_t, unsigned char, char)
HWY_VSX_RAW128(int16_t, signed short, short)     // NOLINT(runtime/int)
HWY_VSX_RAW128(uint16_t, unsigned short, short)  // NOLINT(runtime/int)
HWY_VSX_RAW128(int32_t, signed int, int)
HWY_VSX_RAW128(uint32_t, unsigned int, int)
HWY_VSX_RAW128(int64_t, signed long long, long long)     // NOLINT(runtime/int)
HWY_VSX_RAW128(uint64_t, unsigned long long, long long)  // NOLINT(runtime/int)
HWY_VSX_RAW128(float, float, int)
HWY_VSX_RAW128(double, double, long long)  // NOLINT(runtime/int)

template <>
struct Raw128<bfloat16_t> : public Raw128<uint16_t> {};

template <>
struct Raw128<float16_t> : public Raw128<uint16_t> {};

#undef HWY_VSX_RAW128

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

// FF..FF or 0.
template <typename T, size_t N = 16 / sizeof(T)>
struct Mask128 {
  typename detail::Raw128<T>::RawBoolVec raw;

  using PrivateT = T;                     // only for DFromM
  static constexpr size_t kPrivateN = N;  // only for DFromM
};

template <class V>
using DFromV = Simd<typename V::PrivateT, V::kPrivateN, 0>;

template <class M>
using DFromM = Simd<typename M::PrivateT, M::kPrivateN, 0>;

template <class V>
using TFromV = typename V::PrivateT;

// ------------------------------ Zero

// Returns an all-zero vector/part.
template <class D, typename T = TFromD<D>>
HWY_API Vec128<T, HWY_MAX_LANES_D(D)> Zero(D /* tag */) {
  // There is no vec_splats for 64-bit, so we cannot rely on casting the 0
  // argument in order to select the correct overload. We instead cast the
  // return vector type; see also the comment in BitCast.
  return Vec128<T, HWY_MAX_LANES_D(D)>{
      reinterpret_cast<typename detail::Raw128<T>::type>(vec_splats(0))};
}

template <class D>
using VFromD = decltype(Zero(D()));

// ------------------------------ Tuple (VFromD)
#include "hwy/ops/tuple-inl.h"

// ------------------------------ BitCast

template <class D, typename FromT>
HWY_API VFromD<D> BitCast(D /*d*/,
                          Vec128<FromT, Repartition<FromT, D>().MaxLanes()> v) {
  // C-style casts are not sufficient when compiling with
  // -fno-lax-vector-conversions, which will be the future default in Clang,
  // but reinterpret_cast is.
  return VFromD<D>{
      reinterpret_cast<typename detail::Raw128<TFromD<D>>::type>(v.raw)};
}

// ------------------------------ ResizeBitCast

template <class D, typename FromV>
HWY_API VFromD<D> ResizeBitCast(D /*d*/, FromV v) {
  // C-style casts are not sufficient when compiling with
  // -fno-lax-vector-conversions, which will be the future default in Clang,
  // but reinterpret_cast is.
  return VFromD<D>{
      reinterpret_cast<typename detail::Raw128<TFromD<D>>::type>(v.raw)};
}

// ------------------------------ Set

// Returns a vector/part with all lanes set to "t".
template <class D, HWY_IF_NOT_SPECIAL_FLOAT(TFromD<D>)>
HWY_API VFromD<D> Set(D /* tag */, TFromD<D> t) {
  using RawLane = typename detail::Raw128<TFromD<D>>::RawT;
  return VFromD<D>{vec_splats(static_cast<RawLane>(t))};
}

// Returns a vector with uninitialized elements.
template <class D>
HWY_API VFromD<D> Undefined(D d) {
#if HWY_COMPILER_GCC_ACTUAL
  // Suppressing maybe-uninitialized both here and at the caller does not work,
  // so initialize.
  return Zero(d);
#else
  HWY_DIAGNOSTICS(push)
  HWY_DIAGNOSTICS_OFF(disable : 4700, ignored "-Wuninitialized")
  typename detail::Raw128<TFromD<D>>::type raw;
  return VFromD<decltype(d)>{raw};
  HWY_DIAGNOSTICS(pop)
#endif
}

// ------------------------------ GetLane

// Gets the single value stored in a vector/part.

template <typename T, size_t N>
HWY_API T GetLane(Vec128<T, N> v) {
  return static_cast<T>(v.raw[0]);
}

// ================================================== LOGICAL

// ------------------------------ And

template <typename T, size_t N>
HWY_API Vec128<T, N> And(Vec128<T, N> a, Vec128<T, N> b) {
  const DFromV<decltype(a)> d;
  const RebindToUnsigned<decltype(d)> du;
  using VU = VFromD<decltype(du)>;
  return BitCast(d, VU{vec_and(BitCast(du, a).raw, BitCast(du, b).raw)});
}

// ------------------------------ AndNot

// Returns ~not_mask & mask.
template <typename T, size_t N>
HWY_API Vec128<T, N> AndNot(Vec128<T, N> not_mask, Vec128<T, N> mask) {
  const DFromV<decltype(mask)> d;
  const RebindToUnsigned<decltype(d)> du;
  using VU = VFromD<decltype(du)>;
  return BitCast(
      d, VU{vec_andc(BitCast(du, mask).raw, BitCast(du, not_mask).raw)});
}

// ------------------------------ Or

template <typename T, size_t N>
HWY_API Vec128<T, N> Or(Vec128<T, N> a, Vec128<T, N> b) {
  const DFromV<decltype(a)> d;
  const RebindToUnsigned<decltype(d)> du;
  using VU = VFromD<decltype(du)>;
  return BitCast(d, VU{vec_or(BitCast(du, a).raw, BitCast(du, b).raw)});
}

// ------------------------------ Xor

template <typename T, size_t N>
HWY_API Vec128<T, N> Xor(Vec128<T, N> a, Vec128<T, N> b) {
  const DFromV<decltype(a)> d;
  const RebindToUnsigned<decltype(d)> du;
  using VU = VFromD<decltype(du)>;
  return BitCast(d, VU{vec_xor(BitCast(du, a).raw, BitCast(du, b).raw)});
}

// ------------------------------ Not
template <typename T, size_t N>
HWY_API Vec128<T, N> Not(Vec128<T, N> v) {
  const DFromV<decltype(v)> d;
  const RebindToUnsigned<decltype(d)> du;
  using VU = VFromD<decltype(du)>;
  return BitCast(d, VU{vec_nor(BitCast(du, v).raw, BitCast(du, v).raw)});
}

// ------------------------------ IsConstantRawAltivecVect
namespace detail {

template <class RawV>
static HWY_INLINE bool IsConstantRawAltivecVect(
    hwy::SizeTag<1> /* lane_size_tag */, RawV v) {
  return __builtin_constant_p(v[0]) && __builtin_constant_p(v[1]) &&
         __builtin_constant_p(v[2]) && __builtin_constant_p(v[3]) &&
         __builtin_constant_p(v[4]) && __builtin_constant_p(v[5]) &&
         __builtin_constant_p(v[6]) && __builtin_constant_p(v[7]) &&
         __builtin_constant_p(v[8]) && __builtin_constant_p(v[9]) &&
         __builtin_constant_p(v[10]) && __builtin_constant_p(v[11]) &&
         __builtin_constant_p(v[12]) && __builtin_constant_p(v[13]) &&
         __builtin_constant_p(v[14]) && __builtin_constant_p(v[15]);
}

template <class RawV>
static HWY_INLINE bool IsConstantRawAltivecVect(
    hwy::SizeTag<2> /* lane_size_tag */, RawV v) {
  return __builtin_constant_p(v[0]) && __builtin_constant_p(v[1]) &&
         __builtin_constant_p(v[2]) && __builtin_constant_p(v[3]) &&
         __builtin_constant_p(v[4]) && __builtin_constant_p(v[5]) &&
         __builtin_constant_p(v[6]) && __builtin_constant_p(v[7]);
}

template <class RawV>
static HWY_INLINE bool IsConstantRawAltivecVect(
    hwy::SizeTag<4> /* lane_size_tag */, RawV v) {
  return __builtin_constant_p(v[0]) && __builtin_constant_p(v[1]) &&
         __builtin_constant_p(v[2]) && __builtin_constant_p(v[3]);
}

template <class RawV>
static HWY_INLINE bool IsConstantRawAltivecVect(
    hwy::SizeTag<8> /* lane_size_tag */, RawV v) {
  return __builtin_constant_p(v[0]) && __builtin_constant_p(v[1]);
}

template <class RawV>
static HWY_INLINE bool IsConstantRawAltivecVect(RawV v) {
  return IsConstantRawAltivecVect(hwy::SizeTag<sizeof(decltype(v[0]))>(), v);
}

}  // namespace detail

// ------------------------------ TernaryLogic
#if HWY_PPC_HAVE_10
namespace detail {

// NOTE: the kTernLogOp bits of the PPC10 TernaryLogic operation are in reverse
// order of the kTernLogOp bits of AVX3
// _mm_ternarylogic_epi64(a, b, c, kTernLogOp)
template <uint8_t kTernLogOp, class V>
HWY_INLINE V TernaryLogic(V a, V b, V c) {
  const DFromV<decltype(a)> d;
  const RebindToUnsigned<decltype(d)> du;
  using VU = VFromD<decltype(du)>;
  const auto a_raw = BitCast(du, a).raw;
  const auto b_raw = BitCast(du, b).raw;
  const auto c_raw = BitCast(du, c).raw;

#if HWY_COMPILER_GCC_ACTUAL
  // Use inline assembly on GCC to work around GCC compiler bug
  typename detail::Raw128<TFromV<VU>>::type raw_ternlog_result;
  __asm__("xxeval %x0,%x1,%x2,%x3,%4"
          : "=wa"(raw_ternlog_result)
          : "wa"(a_raw), "wa"(b_raw), "wa"(c_raw), "n"(kTernLogOp)
          :);
#else
  const auto raw_ternlog_result =
      vec_ternarylogic(a_raw, b_raw, c_raw, kTernLogOp);
#endif

  return BitCast(d, VU{raw_ternlog_result});
}

}  // namespace detail
#endif  // HWY_PPC_HAVE_10

// ------------------------------ Xor3
template <typename T, size_t N>
HWY_API Vec128<T, N> Xor3(Vec128<T, N> x1, Vec128<T, N> x2, Vec128<T, N> x3) {
#if HWY_PPC_HAVE_10
#if defined(__OPTIMIZE__)
  if (static_cast<int>(detail::IsConstantRawAltivecVect(x1.raw)) +
          static_cast<int>(detail::IsConstantRawAltivecVect(x2.raw)) +
          static_cast<int>(detail::IsConstantRawAltivecVect(x3.raw)) >=
      2) {
    return Xor(x1, Xor(x2, x3));
  } else  // NOLINT
#endif
  {
    return detail::TernaryLogic<0x69>(x1, x2, x3);
  }
#else
  return Xor(x1, Xor(x2, x3));
#endif
}

// ------------------------------ Or3
template <typename T, size_t N>
HWY_API Vec128<T, N> Or3(Vec128<T, N> o1, Vec128<T, N> o2, Vec128<T, N> o3) {
#if HWY_PPC_HAVE_10
#if defined(__OPTIMIZE__)
  if (static_cast<int>(detail::IsConstantRawAltivecVect(o1.raw)) +
          static_cast<int>(detail::IsConstantRawAltivecVect(o2.raw)) +
          static_cast<int>(detail::IsConstantRawAltivecVect(o3.raw)) >=
      2) {
    return Or(o1, Or(o2, o3));
  } else  // NOLINT
#endif
  {
    return detail::TernaryLogic<0x7F>(o1, o2, o3);
  }
#else
  return Or(o1, Or(o2, o3));
#endif
}

// ------------------------------ OrAnd
template <typename T, size_t N>
HWY_API Vec128<T, N> OrAnd(Vec128<T, N> o, Vec128<T, N> a1, Vec128<T, N> a2) {
#if HWY_PPC_HAVE_10
#if defined(__OPTIMIZE__)
  if (detail::IsConstantRawAltivecVect(a1.raw) &&
      detail::IsConstantRawAltivecVect(a2.raw)) {
    return Or(o, And(a1, a2));
  } else  // NOLINT
#endif
  {
    return detail::TernaryLogic<0x1F>(o, a1, a2);
  }
#else
  return Or(o, And(a1, a2));
#endif
}

// ------------------------------ IfVecThenElse
template <typename T, size_t N>
HWY_API Vec128<T, N> IfVecThenElse(Vec128<T, N> mask, Vec128<T, N> yes,
                                   Vec128<T, N> no) {
  const DFromV<decltype(yes)> d;
  const RebindToUnsigned<decltype(d)> du;
  return BitCast(
      d, VFromD<decltype(du)>{vec_sel(BitCast(du, no).raw, BitCast(du, yes).raw,
                                      BitCast(du, mask).raw)});
}

// ------------------------------ BitwiseIfThenElse

#ifdef HWY_NATIVE_BITWISE_IF_THEN_ELSE
#undef HWY_NATIVE_BITWISE_IF_THEN_ELSE
#else
#define HWY_NATIVE_BITWISE_IF_THEN_ELSE
#endif

template <class V>
HWY_API V BitwiseIfThenElse(V mask, V yes, V no) {
  return IfVecThenElse(mask, yes, no);
}

// ------------------------------ Operator overloads (internal-only if float)

template <typename T, size_t N>
HWY_API Vec128<T, N> operator&(Vec128<T, N> a, Vec128<T, N> b) {
  return And(a, b);
}

template <typename T, size_t N>
HWY_API Vec128<T, N> operator|(Vec128<T, N> a, Vec128<T, N> b) {
  return Or(a, b);
}

template <typename T, size_t N>
HWY_API Vec128<T, N> operator^(Vec128<T, N> a, Vec128<T, N> b) {
  return Xor(a, b);
}

// ================================================== SIGN

// ------------------------------ Neg

template <typename T, size_t N, HWY_IF_NOT_SPECIAL_FLOAT(T)>
HWY_INLINE Vec128<T, N> Neg(Vec128<T, N> v) {
  return Vec128<T, N>{vec_neg(v.raw)};
}

// ------------------------------ Abs

// Returns absolute value, except that LimitsMin() maps to LimitsMax() + 1.
template <class T, size_t N, HWY_IF_NOT_SPECIAL_FLOAT(T)>
HWY_API Vec128<T, N> Abs(Vec128<T, N> v) {
  return Vec128<T, N>{vec_abs(v.raw)};
}

// ------------------------------ CopySign

template <size_t N>
HWY_API Vec128<float, N> CopySign(Vec128<float, N> magn,
                                  Vec128<float, N> sign) {
  // Work around compiler bugs that are there with vec_cpsgn on older versions
  // of GCC/Clang
#if HWY_COMPILER_GCC_ACTUAL && HWY_COMPILER_GCC_ACTUAL < 1200
  return Vec128<float, N>{__builtin_vec_copysign(magn.raw, sign.raw)};
#elif HWY_COMPILER_CLANG && HWY_COMPILER_CLANG < 1200 && \
    HWY_HAS_BUILTIN(__builtin_vsx_xvcpsgnsp)
  return Vec128<float, N>{__builtin_vsx_xvcpsgnsp(magn.raw, sign.raw)};
#else
  return Vec128<float, N>{vec_cpsgn(sign.raw, magn.raw)};
#endif
}

template <size_t N>
HWY_API Vec128<double, N> CopySign(Vec128<double, N> magn,
                                   Vec128<double, N> sign) {
  // Work around compiler bugs that are there with vec_cpsgn on older versions
  // of GCC/Clang
#if HWY_COMPILER_GCC_ACTUAL && HWY_COMPILER_GCC_ACTUAL < 1200
  return Vec128<double, N>{__builtin_vec_copysign(magn.raw, sign.raw)};
#elif HWY_COMPILER_CLANG && HWY_COMPILER_CLANG < 1200 && \
    HWY_HAS_BUILTIN(__builtin_vsx_xvcpsgndp)
  return Vec128<double, N>{__builtin_vsx_xvcpsgndp(magn.raw, sign.raw)};
#else
  return Vec128<double, N>{vec_cpsgn(sign.raw, magn.raw)};
#endif
}

template <typename T, size_t N>
HWY_API Vec128<T, N> CopySignToAbs(Vec128<T, N> abs, Vec128<T, N> sign) {
  // PPC8 can also handle abs < 0, so no extra action needed.
  static_assert(IsFloat<T>(), "Only makes sense for floating-point");
  return CopySign(abs, sign);
}

// ================================================== MEMORY (1)

// Note: type punning is safe because the types are tagged with may_alias.
// (https://godbolt.org/z/fqrWjfjsP)

// ------------------------------ Load

template <class D, HWY_IF_V_SIZE_D(D, 16), typename T = TFromD<D>>
HWY_API Vec128<T> Load(D /* tag */, const T* HWY_RESTRICT aligned) {
  using LoadRaw = typename detail::Raw128<T>::AlignedRawVec;
  const LoadRaw* HWY_RESTRICT p = reinterpret_cast<const LoadRaw*>(aligned);
  using ResultRaw = typename detail::Raw128<T>::type;
  return Vec128<T>{reinterpret_cast<ResultRaw>(*p)};
}

// Any <= 64 bit
template <class D, HWY_IF_V_SIZE_LE_D(D, 8), typename T = TFromD<D>>
HWY_API VFromD<D> Load(D d, const T* HWY_RESTRICT p) {
  using BitsT = UnsignedFromSize<d.MaxBytes()>;

  BitsT bits;
  const Repartition<BitsT, decltype(d)> d_bits;
  CopyBytes<d.MaxBytes()>(p, &bits);
  return BitCast(d, Set(d_bits, bits));
}

// ================================================== MASK

// ------------------------------ Mask

// Mask and Vec are both backed by vector types (true = FF..FF).
template <typename T, size_t N>
HWY_API Mask128<T, N> MaskFromVec(Vec128<T, N> v) {
  using Raw = typename detail::Raw128<T>::RawBoolVec;
  return Mask128<T, N>{reinterpret_cast<Raw>(v.raw)};
}

template <class D>
using MFromD = decltype(MaskFromVec(VFromD<D>()));

template <typename T, size_t N>
HWY_API Vec128<T, N> VecFromMask(Mask128<T, N> v) {
  return Vec128<T, N>{
      reinterpret_cast<typename detail::Raw128<T>::type>(v.raw)};
}

template <class D>
HWY_API VFromD<D> VecFromMask(D /* tag */, MFromD<D> v) {
  return VFromD<D>{
      reinterpret_cast<typename detail::Raw128<TFromD<D>>::type>(v.raw)};
}

// mask ? yes : no
template <typename T, size_t N>
HWY_API Vec128<T, N> IfThenElse(Mask128<T, N> mask, Vec128<T, N> yes,
                                Vec128<T, N> no) {
  const DFromV<decltype(yes)> d;
  const RebindToUnsigned<decltype(d)> du;
  return BitCast(d, VFromD<decltype(du)>{vec_sel(
                        BitCast(du, no).raw, BitCast(du, yes).raw, mask.raw)});
}

// mask ? yes : 0
template <typename T, size_t N>
HWY_API Vec128<T, N> IfThenElseZero(Mask128<T, N> mask, Vec128<T, N> yes) {
  const DFromV<decltype(yes)> d;
  const RebindToUnsigned<decltype(d)> du;
  return BitCast(d,
                 VFromD<decltype(du)>{vec_and(BitCast(du, yes).raw, mask.raw)});
}

// mask ? 0 : no
template <typename T, size_t N>
HWY_API Vec128<T, N> IfThenZeroElse(Mask128<T, N> mask, Vec128<T, N> no) {
  const DFromV<decltype(no)> d;
  const RebindToUnsigned<decltype(d)> du;
  return BitCast(d,
                 VFromD<decltype(du)>{vec_andc(BitCast(du, no).raw, mask.raw)});
}

// ------------------------------ Mask logical

template <typename T, size_t N>
HWY_API Mask128<T, N> Not(Mask128<T, N> m) {
  return Mask128<T, N>{vec_nor(m.raw, m.raw)};
}

template <typename T, size_t N>
HWY_API Mask128<T, N> And(Mask128<T, N> a, Mask128<T, N> b) {
  return Mask128<T, N>{vec_and(a.raw, b.raw)};
}

template <typename T, size_t N>
HWY_API Mask128<T, N> AndNot(Mask128<T, N> a, Mask128<T, N> b) {
  return Mask128<T, N>{vec_andc(b.raw, a.raw)};
}

template <typename T, size_t N>
HWY_API Mask128<T, N> Or(Mask128<T, N> a, Mask128<T, N> b) {
  return Mask128<T, N>{vec_or(a.raw, b.raw)};
}

template <typename T, size_t N>
HWY_API Mask128<T, N> Xor(Mask128<T, N> a, Mask128<T, N> b) {
  return Mask128<T, N>{vec_xor(a.raw, b.raw)};
}

template <typename T, size_t N>
HWY_API Mask128<T, N> ExclusiveNeither(Mask128<T, N> a, Mask128<T, N> b) {
  return Mask128<T, N>{vec_nor(a.raw, b.raw)};
}

// ------------------------------ BroadcastSignBit

template <size_t N>
HWY_API Vec128<int8_t, N> BroadcastSignBit(Vec128<int8_t, N> v) {
  return Vec128<int8_t, N>{
      vec_sra(v.raw, vec_splats(static_cast<unsigned char>(7)))};
}

template <size_t N>
HWY_API Vec128<int16_t, N> BroadcastSignBit(Vec128<int16_t, N> v) {
  return Vec128<int16_t, N>{
      vec_sra(v.raw, vec_splats(static_cast<unsigned short>(15)))};
}

template <size_t N>
HWY_API Vec128<int32_t, N> BroadcastSignBit(Vec128<int32_t, N> v) {
  return Vec128<int32_t, N>{vec_sra(v.raw, vec_splats(31u))};
}

template <size_t N>
HWY_API Vec128<int64_t, N> BroadcastSignBit(Vec128<int64_t, N> v) {
  return Vec128<int64_t, N>{vec_sra(v.raw, vec_splats(63ULL))};
}

// ------------------------------ ShiftLeftSame

template <typename T, size_t N, HWY_IF_NOT_FLOAT_NOR_SPECIAL(T)>
HWY_API Vec128<T, N> ShiftLeftSame(Vec128<T, N> v, const int bits) {
  using TU = typename detail::Raw128<MakeUnsigned<T>>::RawT;
  return Vec128<T, N>{vec_sl(v.raw, vec_splats(static_cast<TU>(bits)))};
}

// ------------------------------ ShiftRightSame

template <typename T, size_t N, HWY_IF_UNSIGNED(T)>
HWY_API Vec128<T, N> ShiftRightSame(Vec128<T, N> v, const int bits) {
  using TU = typename detail::Raw128<MakeUnsigned<T>>::RawT;
  return Vec128<T, N>{vec_sr(v.raw, vec_splats(static_cast<TU>(bits)))};
}

template <typename T, size_t N, HWY_IF_SIGNED(T)>
HWY_API Vec128<T, N> ShiftRightSame(Vec128<T, N> v, const int bits) {
  using TU = typename detail::Raw128<MakeUnsigned<T>>::RawT;
  return Vec128<T, N>{vec_sra(v.raw, vec_splats(static_cast<TU>(bits)))};
}

// ------------------------------ ShiftLeft

template <int kBits, typename T, size_t N, HWY_IF_NOT_FLOAT_NOR_SPECIAL(T)>
HWY_API Vec128<T, N> ShiftLeft(Vec128<T, N> v) {
  static_assert(0 <= kBits && kBits < sizeof(T) * 8, "Invalid shift");
  return ShiftLeftSame(v, kBits);
}

// ------------------------------ ShiftRight

template <int kBits, typename T, size_t N, HWY_IF_NOT_FLOAT_NOR_SPECIAL(T)>
HWY_API Vec128<T, N> ShiftRight(Vec128<T, N> v) {
  static_assert(0 <= kBits && kBits < sizeof(T) * 8, "Invalid shift");
  return ShiftRightSame(v, kBits);
}

// ================================================== SWIZZLE (1)

// ------------------------------ TableLookupBytes
template <typename T, size_t N, typename TI, size_t NI>
HWY_API Vec128<TI, NI> TableLookupBytes(Vec128<T, N> bytes,
                                        Vec128<TI, NI> from) {
  const Repartition<uint8_t, DFromV<decltype(from)>> du8_from;
  return Vec128<TI, NI>{reinterpret_cast<typename detail::Raw128<TI>::type>(
      vec_perm(bytes.raw, bytes.raw, BitCast(du8_from, from).raw))};
}

// ------------------------------ TableLookupBytesOr0
// For all vector widths; Altivec/VSX needs zero out
template <class V, class VI>
HWY_API VI TableLookupBytesOr0(const V bytes, const VI from) {
  const DFromV<VI> di;
  Repartition<int8_t, decltype(di)> di8;
  const VI zeroOutMask = BitCast(di, BroadcastSignBit(BitCast(di8, from)));
  return AndNot(zeroOutMask, TableLookupBytes(bytes, from));
}

// ------------------------------ Reverse
template <class D, typename T = TFromD<D>, HWY_IF_LANES_GT_D(D, 1)>
HWY_API Vec128<T> Reverse(D /* tag */, Vec128<T> v) {
  return Vec128<T>{vec_reve(v.raw)};
}

// ------------------------------ Shuffles (Reverse)

// Notation: let Vec128<int32_t> have lanes 3,2,1,0 (0 is least-significant).
// Shuffle0321 rotates one lane to the right (the previous least-significant
// lane is now most-significant). These could also be implemented via
// CombineShiftRightBytes but the shuffle_abcd notation is more convenient.

// Swap 32-bit halves in 64-bit halves.
template <typename T, size_t N>
HWY_API Vec128<T, N> Shuffle2301(Vec128<T, N> v) {
  static_assert(sizeof(T) == 4, "Only for 32-bit lanes");
  static_assert(N == 2 || N == 4, "Does not make sense for N=1");
  const __vector unsigned char kShuffle = {4,  5,  6,  7,  0, 1, 2,  3,
                                           12, 13, 14, 15, 8, 9, 10, 11};
  return Vec128<T, N>{vec_perm(v.raw, v.raw, kShuffle)};
}

// These are used by generic_ops-inl to implement LoadInterleaved3. As with
// Intel's shuffle* intrinsics and InterleaveLower, the lower half of the output
// comes from the first argument.
namespace detail {

template <typename T, HWY_IF_T_SIZE(T, 1)>
HWY_API Vec32<T> ShuffleTwo2301(Vec32<T> a, Vec32<T> b) {
  const __vector unsigned char kShuffle16 = {1, 0, 19, 18};
  return Vec32<T>{vec_perm(a.raw, b.raw, kShuffle16)};
}
template <typename T, HWY_IF_T_SIZE(T, 2)>
HWY_API Vec64<T> ShuffleTwo2301(Vec64<T> a, Vec64<T> b) {
  const __vector unsigned char kShuffle = {2, 3, 0, 1, 22, 23, 20, 21};
  return Vec64<T>{vec_perm(a.raw, b.raw, kShuffle)};
}
template <typename T, HWY_IF_T_SIZE(T, 4)>
HWY_API Vec128<T> ShuffleTwo2301(Vec128<T> a, Vec128<T> b) {
  const __vector unsigned char kShuffle = {4,  5,  6,  7,  0,  1,  2,  3,
                                           28, 29, 30, 31, 24, 25, 26, 27};
  return Vec128<T>{vec_perm(a.raw, b.raw, kShuffle)};
}

template <typename T, HWY_IF_T_SIZE(T, 1)>
HWY_API Vec32<T> ShuffleTwo1230(Vec32<T> a, Vec32<T> b) {
  const __vector unsigned char kShuffle = {0, 3, 18, 17};
  return Vec32<T>{vec_perm(a.raw, b.raw, kShuffle)};
}
template <typename T, HWY_IF_T_SIZE(T, 2)>
HWY_API Vec64<T> ShuffleTwo1230(Vec64<T> a, Vec64<T> b) {
  const __vector unsigned char kShuffle = {0, 1, 6, 7, 20, 21, 18, 19};
  return Vec64<T>{vec_perm(a.raw, b.raw, kShuffle)};
}
template <typename T, HWY_IF_T_SIZE(T, 4)>
HWY_API Vec128<T> ShuffleTwo1230(Vec128<T> a, Vec128<T> b) {
  const __vector unsigned char kShuffle = {0,  1,  2,  3,  12, 13, 14, 15,
                                           24, 25, 26, 27, 20, 21, 22, 23};
  return Vec128<T>{vec_perm(a.raw, b.raw, kShuffle)};
}

template <typename T, HWY_IF_T_SIZE(T, 1)>
HWY_API Vec32<T> ShuffleTwo3012(Vec32<T> a, Vec32<T> b) {
  const __vector unsigned char kShuffle = {2, 1, 16, 19};
  return Vec32<T>{vec_perm(a.raw, b.raw, kShuffle)};
}
template <typename T, HWY_IF_T_SIZE(T, 2)>
HWY_API Vec64<T> ShuffleTwo3012(Vec64<T> a, Vec64<T> b) {
  const __vector unsigned char kShuffle = {4, 5, 2, 3, 16, 17, 22, 23};
  return Vec64<T>{vec_perm(a.raw, b.raw, kShuffle)};
}
template <typename T, HWY_IF_T_SIZE(T, 4)>
HWY_API Vec128<T> ShuffleTwo3012(Vec128<T> a, Vec128<T> b) {
  const __vector unsigned char kShuffle = {8,  9,  10, 11, 4,  5,  6,  7,
                                           16, 17, 18, 19, 28, 29, 30, 31};
  return Vec128<T>{vec_perm(a.raw, b.raw, kShuffle)};
}

}  // namespace detail

// Swap 64-bit halves
template <class T, HWY_IF_T_SIZE(T, 4)>
HWY_API Vec128<T> Shuffle1032(Vec128<T> v) {
  const Full128<T> d;
  const Full128<uint64_t> du64;
  return BitCast(d, Reverse(du64, BitCast(du64, v)));
}
template <class T, HWY_IF_T_SIZE(T, 8)>
HWY_API Vec128<T> Shuffle01(Vec128<T> v) {
  return Reverse(Full128<T>(), v);
}

// Rotate right 32 bits
template <class T, HWY_IF_T_SIZE(T, 4)>
HWY_API Vec128<T> Shuffle0321(Vec128<T> v) {
#if HWY_IS_LITTLE_ENDIAN
  return Vec128<T>{vec_sld(v.raw, v.raw, 12)};
#else
  return Vec128<T>{vec_sld(v.raw, v.raw, 4)};
#endif
}
// Rotate left 32 bits
template <class T, HWY_IF_T_SIZE(T, 4)>
HWY_API Vec128<T> Shuffle2103(Vec128<T> v) {
#if HWY_IS_LITTLE_ENDIAN
  return Vec128<T>{vec_sld(v.raw, v.raw, 4)};
#else
  return Vec128<T>{vec_sld(v.raw, v.raw, 12)};
#endif
}

template <class T, HWY_IF_T_SIZE(T, 4)>
HWY_API Vec128<T> Shuffle0123(Vec128<T> v) {
  return Reverse(Full128<T>(), v);
}

// ================================================== COMPARE

// Comparisons fill a lane with 1-bits if the condition is true, else 0.

template <class DTo, typename TFrom, size_t NFrom>
HWY_API MFromD<DTo> RebindMask(DTo /*dto*/, Mask128<TFrom, NFrom> m) {
  static_assert(sizeof(TFrom) == sizeof(TFromD<DTo>), "Must have same size");
  return MFromD<DTo>{m.raw};
}

template <typename T, size_t N>
HWY_API Mask128<T, N> TestBit(Vec128<T, N> v, Vec128<T, N> bit) {
  static_assert(!hwy::IsFloat<T>(), "Only integer vectors supported");
  return (v & bit) == bit;
}

// ------------------------------ Equality

template <typename T, size_t N>
HWY_API Mask128<T, N> operator==(Vec128<T, N> a, Vec128<T, N> b) {
  return Mask128<T, N>{vec_cmpeq(a.raw, b.raw)};
}

// ------------------------------ Inequality

// This cannot have T as a template argument, otherwise it is not more
// specialized than rewritten operator== in C++20, leading to compile
// errors: https://gcc.godbolt.org/z/xsrPhPvPT.
template <size_t N>
HWY_API Mask128<uint8_t, N> operator!=(Vec128<uint8_t, N> a,
                                       Vec128<uint8_t, N> b) {
#if HWY_PPC_HAVE_9
  return Mask128<uint8_t, N>{vec_cmpne(a.raw, b.raw)};
#else
  return Not(a == b);
#endif
}
template <size_t N>
HWY_API Mask128<uint16_t, N> operator!=(Vec128<uint16_t, N> a,
                                        Vec128<uint16_t, N> b) {
#if HWY_PPC_HAVE_9
  return Mask128<uint16_t, N>{vec_cmpne(a.raw, b.raw)};
#else
  return Not(a == b);
#endif
}
template <size_t N>
HWY_API Mask128<uint32_t, N> operator!=(Vec128<uint32_t, N> a,
                                        Vec128<uint32_t, N> b) {
#if HWY_PPC_HAVE_9
  return Mask128<uint32_t, N>{vec_cmpne(a.raw, b.raw)};
#else
  return Not(a == b);
#endif
}
template <size_t N>
HWY_API Mask128<uint64_t, N> operator!=(Vec128<uint64_t, N> a,
                                        Vec128<uint64_t, N> b) {
  return Not(a == b);
}
template <size_t N>
HWY_API Mask128<int8_t, N> operator!=(Vec128<int8_t, N> a,
                                      Vec128<int8_t, N> b) {
#if HWY_PPC_HAVE_9
  return Mask128<int8_t, N>{vec_cmpne(a.raw, b.raw)};
#else
  return Not(a == b);
#endif
}
template <size_t N>
HWY_API Mask128<int16_t, N> operator!=(Vec128<int16_t, N> a,
                                       Vec128<int16_t, N> b) {
#if HWY_PPC_HAVE_9
  return Mask128<int16_t, N>{vec_cmpne(a.raw, b.raw)};
#else
  return Not(a == b);
#endif
}
template <size_t N>
HWY_API Mask128<int32_t, N> operator!=(Vec128<int32_t, N> a,
                                       Vec128<int32_t, N> b) {
#if HWY_PPC_HAVE_9
  return Mask128<int32_t, N>{vec_cmpne(a.raw, b.raw)};
#else
  return Not(a == b);
#endif
}
template <size_t N>
HWY_API Mask128<int64_t, N> operator!=(Vec128<int64_t, N> a,
                                       Vec128<int64_t, N> b) {
  return Not(a == b);
}

template <size_t N>
HWY_API Mask128<float, N> operator!=(Vec128<float, N> a, Vec128<float, N> b) {
  return Not(a == b);
}

template <size_t N>
HWY_API Mask128<double, N> operator!=(Vec128<double, N> a,
                                      Vec128<double, N> b) {
  return Not(a == b);
}

// ------------------------------ Strict inequality

template <typename T, size_t N, HWY_IF_NOT_SPECIAL_FLOAT(T)>
HWY_INLINE Mask128<T, N> operator>(Vec128<T, N> a, Vec128<T, N> b) {
  return Mask128<T, N>{vec_cmpgt(a.raw, b.raw)};
}

// ------------------------------ Weak inequality

template <typename T, size_t N, HWY_IF_FLOAT(T)>
HWY_API Mask128<T, N> operator>=(Vec128<T, N> a, Vec128<T, N> b) {
  return Mask128<T, N>{vec_cmpge(a.raw, b.raw)};
}

template <typename T, size_t N, HWY_IF_NOT_FLOAT_NOR_SPECIAL(T)>
HWY_API Mask128<T, N> operator>=(Vec128<T, N> a, Vec128<T, N> b) {
  return Not(b > a);
}

// ------------------------------ Reversed comparisons

template <typename T, size_t N, HWY_IF_NOT_SPECIAL_FLOAT(T)>
HWY_API Mask128<T, N> operator<(Vec128<T, N> a, Vec128<T, N> b) {
  return b > a;
}

template <typename T, size_t N, HWY_IF_NOT_SPECIAL_FLOAT(T)>
HWY_API Mask128<T, N> operator<=(Vec128<T, N> a, Vec128<T, N> b) {
  return b >= a;
}

// ================================================== MEMORY (2)

// ------------------------------ Load
template <class D, HWY_IF_V_SIZE_D(D, 16), typename T = TFromD<D>>
HWY_API Vec128<T> LoadU(D /* tag */, const T* HWY_RESTRICT p) {
  using LoadRaw = typename detail::Raw128<T>::UnalignedRawVec;
  const LoadRaw* HWY_RESTRICT praw = reinterpret_cast<const LoadRaw*>(p);
  using ResultRaw = typename detail::Raw128<T>::type;
  return Vec128<T>{reinterpret_cast<ResultRaw>(*praw)};
}

// For < 128 bit, LoadU == Load.
template <class D, HWY_IF_V_SIZE_LE_D(D, 8), typename T = TFromD<D>>
HWY_API VFromD<D> LoadU(D d, const T* HWY_RESTRICT p) {
  return Load(d, p);
}

// 128-bit SIMD => nothing to duplicate, same as an unaligned load.
template <class D, typename T = TFromD<D>>
HWY_API VFromD<D> LoadDup128(D d, const T* HWY_RESTRICT p) {
  return LoadU(d, p);
}

// Returns a vector with lane i=[0, N) set to "first" + i.
namespace detail {

template <class D, HWY_IF_T_SIZE_D(D, 1)>
HWY_INLINE VFromD<D> Iota0(D d) {
  constexpr __vector unsigned char kU8Iota0 = {0, 1, 2,  3,  4,  5,  6,  7,
                                               8, 9, 10, 11, 12, 13, 14, 15};
  return BitCast(d, VFromD<RebindToUnsigned<D>>{kU8Iota0});
}

template <class D, HWY_IF_T_SIZE_D(D, 2), HWY_IF_NOT_SPECIAL_FLOAT_D(D)>
HWY_INLINE VFromD<D> Iota0(D d) {
  constexpr __vector unsigned short kU16Iota0 = {0, 1, 2, 3, 4, 5, 6, 7};
  return BitCast(d, VFromD<RebindToUnsigned<D>>{kU16Iota0});
}

template <class D, HWY_IF_UI32_D(D)>
HWY_INLINE VFromD<D> Iota0(D d) {
  constexpr __vector unsigned int kU32Iota0 = {0, 1, 2, 3};
  return BitCast(d, VFromD<RebindToUnsigned<D>>{kU32Iota0});
}

template <class D, HWY_IF_UI64_D(D)>
HWY_INLINE VFromD<D> Iota0(D d) {
  constexpr __vector unsigned long long kU64Iota0 = {0, 1};
  return BitCast(d, VFromD<RebindToUnsigned<D>>{kU64Iota0});
}

template <class D, HWY_IF_F32_D(D)>
HWY_INLINE VFromD<D> Iota0(D /*d*/) {
  constexpr __vector float kF32Iota0 = {0.0f, 1.0f, 2.0f, 3.0f};
  return VFromD<D>{kF32Iota0};
}

template <class D, HWY_IF_F64_D(D)>
HWY_INLINE VFromD<D> Iota0(D /*d*/) {
  constexpr __vector double kF64Iota0 = {0.0, 1.0};
  return VFromD<D>{kF64Iota0};
}

}  // namespace detail

template <class D, typename T2>
HWY_API VFromD<D> Iota(D d, const T2 first) {
  return detail::Iota0(d) + Set(d, static_cast<TFromD<D>>(first));
}

// ------------------------------ FirstN (Iota, Lt)

template <class D>
HWY_API MFromD<D> FirstN(D d, size_t num) {
  const RebindToUnsigned<decltype(d)> du;
  using TU = TFromD<decltype(du)>;
  return RebindMask(d, Iota(du, 0) < Set(du, static_cast<TU>(num)));
}

// ------------------------------ MaskedLoad
template <class D, typename T = TFromD<D>>
HWY_API VFromD<D> MaskedLoad(MFromD<D> m, D d, const T* HWY_RESTRICT p) {
  return IfThenElseZero(m, LoadU(d, p));
}

// ------------------------------ MaskedLoadOr
template <class D, typename T = TFromD<D>>
HWY_API VFromD<D> MaskedLoadOr(VFromD<D> v, MFromD<D> m, D d,
                               const T* HWY_RESTRICT p) {
  return IfThenElse(m, LoadU(d, p), v);
}

// ------------------------------ Store

template <class D, HWY_IF_V_SIZE_D(D, 16), typename T = TFromD<D>>
HWY_API void Store(Vec128<T> v, D /* tag */, T* HWY_RESTRICT aligned) {
  using StoreRaw = typename detail::Raw128<T>::AlignedRawVec;
  *reinterpret_cast<StoreRaw*>(aligned) = reinterpret_cast<StoreRaw>(v.raw);
}

template <class D, HWY_IF_V_SIZE_D(D, 16), typename T = TFromD<D>>
HWY_API void StoreU(Vec128<T> v, D /* tag */, T* HWY_RESTRICT p) {
  using StoreRaw = typename detail::Raw128<T>::UnalignedRawVec;
  *reinterpret_cast<StoreRaw*>(p) = reinterpret_cast<StoreRaw>(v.raw);
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 8), typename T = TFromD<D>>
HWY_API void Store(VFromD<D> v, D d, T* HWY_RESTRICT p) {
  using BitsT = UnsignedFromSize<d.MaxBytes()>;

  const Repartition<BitsT, decltype(d)> d_bits;
  const BitsT bits = GetLane(BitCast(d_bits, v));
  CopyBytes<d.MaxBytes()>(&bits, p);
}

// For < 128 bit, StoreU == Store.
template <class D, HWY_IF_V_SIZE_LE_D(D, 8), typename T = TFromD<D>>
HWY_API void StoreU(VFromD<D> v, D d, T* HWY_RESTRICT p) {
  Store(v, d, p);
}

// ------------------------------ BlendedStore

template <class D>
HWY_API void BlendedStore(VFromD<D> v, MFromD<D> m, D d,
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

// ================================================== ARITHMETIC

// ------------------------------ Addition

template <typename T, size_t N, HWY_IF_NOT_SPECIAL_FLOAT(T)>
HWY_API Vec128<T, N> operator+(Vec128<T, N> a, Vec128<T, N> b) {
  return Vec128<T, N>{vec_add(a.raw, b.raw)};
}

// ------------------------------ Subtraction

template <typename T, size_t N, HWY_IF_NOT_SPECIAL_FLOAT(T)>
HWY_API Vec128<T, N> operator-(Vec128<T, N> a, Vec128<T, N> b) {
  return Vec128<T, N>{vec_sub(a.raw, b.raw)};
}

// ------------------------------ SumsOf8
namespace detail {

// Casts nominally uint32_t result to D.
template <class D>
HWY_INLINE VFromD<D> AltivecVsum4ubs(D d, __vector unsigned char a,
                                     __vector unsigned int b) {
  const Repartition<uint32_t, D> du32;
#ifdef __OPTIMIZE__
  if (IsConstantRawAltivecVect(a) && IsConstantRawAltivecVect(b)) {
    const uint64_t sum0 =
        static_cast<uint64_t>(a[0]) + static_cast<uint64_t>(a[1]) +
        static_cast<uint64_t>(a[2]) + static_cast<uint64_t>(a[3]) +
        static_cast<uint64_t>(b[0]);
    const uint64_t sum1 =
        static_cast<uint64_t>(a[4]) + static_cast<uint64_t>(a[5]) +
        static_cast<uint64_t>(a[6]) + static_cast<uint64_t>(a[7]) +
        static_cast<uint64_t>(b[1]);
    const uint64_t sum2 =
        static_cast<uint64_t>(a[8]) + static_cast<uint64_t>(a[9]) +
        static_cast<uint64_t>(a[10]) + static_cast<uint64_t>(a[11]) +
        static_cast<uint64_t>(b[2]);
    const uint64_t sum3 =
        static_cast<uint64_t>(a[12]) + static_cast<uint64_t>(a[13]) +
        static_cast<uint64_t>(a[14]) + static_cast<uint64_t>(a[15]) +
        static_cast<uint64_t>(b[3]);
    return BitCast(
        d,
        VFromD<decltype(du32)>{(__vector unsigned int){
            static_cast<unsigned int>(sum0 <= 0xFFFFFFFFu ? sum0 : 0xFFFFFFFFu),
            static_cast<unsigned int>(sum1 <= 0xFFFFFFFFu ? sum1 : 0xFFFFFFFFu),
            static_cast<unsigned int>(sum2 <= 0xFFFFFFFFu ? sum2 : 0xFFFFFFFFu),
            static_cast<unsigned int>(sum3 <= 0xFFFFFFFFu ? sum3
                                                          : 0xFFFFFFFFu)}});
  } else  // NOLINT
#endif
  {
    return BitCast(d, VFromD<decltype(du32)>{vec_vsum4ubs(a, b)});
  }
}

// Casts nominally int32_t result to D.
template <class D>
HWY_INLINE VFromD<D> AltivecVsum2sws(D d, __vector signed int a,
                                     __vector signed int b) {
  const Repartition<int32_t, D> di32;
#ifdef __OPTIMIZE__
  const Repartition<uint64_t, D> du64;
  constexpr int kDestLaneOffset = HWY_IS_BIG_ENDIAN;
  if (IsConstantRawAltivecVect(a) && __builtin_constant_p(b[kDestLaneOffset]) &&
      __builtin_constant_p(b[kDestLaneOffset + 2])) {
    const int64_t sum0 = static_cast<int64_t>(a[0]) +
                         static_cast<int64_t>(a[1]) +
                         static_cast<int64_t>(b[kDestLaneOffset]);
    const int64_t sum1 = static_cast<int64_t>(a[2]) +
                         static_cast<int64_t>(a[3]) +
                         static_cast<int64_t>(b[kDestLaneOffset + 2]);
    const int32_t sign0 = static_cast<int32_t>(sum0 >> 63);
    const int32_t sign1 = static_cast<int32_t>(sum1 >> 63);
    return BitCast(d, VFromD<decltype(du64)>{(__vector unsigned long long){
                          (sign0 == (sum0 >> 31))
                              ? static_cast<uint32_t>(sum0)
                              : static_cast<uint32_t>(sign0 ^ 0x7FFFFFFF),
                          (sign1 == (sum1 >> 31))
                              ? static_cast<uint32_t>(sum1)
                              : static_cast<uint32_t>(sign1 ^ 0x7FFFFFFF)}});
  } else  // NOLINT
#endif
  {
    __vector signed int sum;

    // Inline assembly is used for vsum2sws to avoid unnecessary shuffling
    // on little-endian PowerPC targets as the result of the vsum2sws
    // instruction will already be in the correct lanes on little-endian
    // PowerPC targets.
    __asm__("vsum2sws %0,%1,%2" : "=v"(sum) : "v"(a), "v"(b));

    return BitCast(d, VFromD<decltype(di32)>{sum});
  }
}

}  // namespace detail

template <size_t N>
HWY_API Vec128<uint64_t, N / 8> SumsOf8(Vec128<uint8_t, N> v) {
  const Repartition<uint64_t, DFromV<decltype(v)>> du64;
  const Repartition<int32_t, decltype(du64)> di32;
  const RebindToUnsigned<decltype(di32)> du32;

  return detail::AltivecVsum2sws(
      du64, detail::AltivecVsum4ubs(di32, v.raw, Zero(du32).raw).raw,
      Zero(di32).raw);
}

// ------------------------------ SaturatedAdd

// Returns a + b clamped to the destination range.

#ifdef HWY_NATIVE_I32_SATURATED_ADDSUB
#undef HWY_NATIVE_I32_SATURATED_ADDSUB
#else
#define HWY_NATIVE_I32_SATURATED_ADDSUB
#endif

#ifdef HWY_NATIVE_U32_SATURATED_ADDSUB
#undef HWY_NATIVE_U32_SATURATED_ADDSUB
#else
#define HWY_NATIVE_U32_SATURATED_ADDSUB
#endif

template <typename T, size_t N, HWY_IF_NOT_FLOAT_NOR_SPECIAL(T),
          HWY_IF_T_SIZE_ONE_OF(T, (1 << 1) | (1 << 2) | (1 << 4))>
HWY_API Vec128<T, N> SaturatedAdd(Vec128<T, N> a, Vec128<T, N> b) {
  return Vec128<T, N>{vec_adds(a.raw, b.raw)};
}

#if HWY_PPC_HAVE_10

#ifdef HWY_NATIVE_I64_SATURATED_ADDSUB
#undef HWY_NATIVE_I64_SATURATED_ADDSUB
#else
#define HWY_NATIVE_I64_SATURATED_ADDSUB
#endif

template <class V, HWY_IF_I64_D(DFromV<V>)>
HWY_API V SaturatedAdd(V a, V b) {
  const DFromV<decltype(a)> d;
  const auto sum = Add(a, b);
  const auto overflow_mask =
      MaskFromVec(BroadcastSignBit(detail::TernaryLogic<0x42>(a, b, sum)));
  const auto overflow_result =
      Xor(BroadcastSignBit(a), Set(d, LimitsMax<int64_t>()));
  return IfThenElse(overflow_mask, overflow_result, sum);
}

#endif  // HWY_PPC_HAVE_10

// ------------------------------ SaturatedSub

// Returns a - b clamped to the destination range.

template <typename T, size_t N, HWY_IF_NOT_FLOAT_NOR_SPECIAL(T),
          HWY_IF_T_SIZE_ONE_OF(T, (1 << 1) | (1 << 2) | (1 << 4))>
HWY_API Vec128<T, N> SaturatedSub(Vec128<T, N> a, Vec128<T, N> b) {
  return Vec128<T, N>{vec_subs(a.raw, b.raw)};
}

#if HWY_PPC_HAVE_10

template <class V, HWY_IF_I64_D(DFromV<V>)>
HWY_API V SaturatedSub(V a, V b) {
  const DFromV<decltype(a)> d;
  const auto diff = Sub(a, b);
  const auto overflow_mask =
      MaskFromVec(BroadcastSignBit(detail::TernaryLogic<0x18>(a, b, diff)));
  const auto overflow_result =
      Xor(BroadcastSignBit(a), Set(d, LimitsMax<int64_t>()));
  return IfThenElse(overflow_mask, overflow_result, diff);
}

#endif  // HWY_PPC_HAVE_10

// ------------------------------ AverageRound

// Returns (a + b + 1) / 2

template <typename T, size_t N, HWY_IF_UNSIGNED(T),
          HWY_IF_T_SIZE_ONE_OF(T, 0x6)>
HWY_API Vec128<T, N> AverageRound(Vec128<T, N> a, Vec128<T, N> b) {
  return Vec128<T, N>{vec_avg(a.raw, b.raw)};
}

// ------------------------------ Multiplication

// Per-target flags to prevent generic_ops-inl.h defining 8/64-bit operator*.
#ifdef HWY_NATIVE_MUL_8
#undef HWY_NATIVE_MUL_8
#else
#define HWY_NATIVE_MUL_8
#endif
#ifdef HWY_NATIVE_MUL_64
#undef HWY_NATIVE_MUL_64
#else
#define HWY_NATIVE_MUL_64
#endif

template <typename T, size_t N, HWY_IF_NOT_SPECIAL_FLOAT(T)>
HWY_API Vec128<T, N> operator*(Vec128<T, N> a, Vec128<T, N> b) {
  return Vec128<T, N>{a.raw * b.raw};
}

// Returns the upper 16 bits of a * b in each lane.
template <typename T, size_t N, HWY_IF_T_SIZE(T, 2), HWY_IF_NOT_FLOAT(T)>
HWY_API Vec128<T, N> MulHigh(Vec128<T, N> a, Vec128<T, N> b) {
  const DFromV<decltype(a)> d;
  const RepartitionToWide<decltype(d)> dw;
  const VFromD<decltype(dw)> p1{vec_mule(a.raw, b.raw)};
  const VFromD<decltype(dw)> p2{vec_mulo(a.raw, b.raw)};
#if HWY_IS_LITTLE_ENDIAN
  const __vector unsigned char kShuffle = {2,  3,  18, 19, 6,  7,  22, 23,
                                           10, 11, 26, 27, 14, 15, 30, 31};
#else
  const __vector unsigned char kShuffle = {0, 1, 16, 17, 4,  5,  20, 21,
                                           8, 9, 24, 25, 12, 13, 28, 29};
#endif
  return BitCast(d, VFromD<decltype(dw)>{vec_perm(p1.raw, p2.raw, kShuffle)});
}

template <size_t N>
HWY_API Vec128<int16_t, N> MulFixedPoint15(Vec128<int16_t, N> a,
                                           Vec128<int16_t, N> b) {
  const Vec128<int16_t> zero = Zero(Full128<int16_t>());
  return Vec128<int16_t, N>{vec_mradds(a.raw, b.raw, zero.raw)};
}

// Multiplies even lanes (0, 2 ..) and places the double-wide result into
// even and the upper half into its odd neighbor lane.
template <typename T, size_t N, HWY_IF_T_SIZE(T, 4), HWY_IF_NOT_FLOAT(T)>
HWY_API Vec128<MakeWide<T>, (N + 1) / 2> MulEven(Vec128<T, N> a,
                                                 Vec128<T, N> b) {
  return Vec128<MakeWide<T>, (N + 1) / 2>{vec_mule(a.raw, b.raw)};
}

// ------------------------------ RotateRight
template <int kBits, typename T, size_t N>
HWY_API Vec128<T, N> RotateRight(const Vec128<T, N> v) {
  const DFromV<decltype(v)> d;
  constexpr size_t kSizeInBits = sizeof(T) * 8;
  static_assert(0 <= kBits && kBits < kSizeInBits, "Invalid shift count");
  if (kBits == 0) return v;
  return Vec128<T, N>{vec_rl(v.raw, Set(d, kSizeInBits - kBits).raw)};
}

// ------------------------------ ZeroIfNegative (BroadcastSignBit)
template <typename T, size_t N>
HWY_API Vec128<T, N> ZeroIfNegative(Vec128<T, N> v) {
  static_assert(IsFloat<T>(), "Only works for float");
  const DFromV<decltype(v)> d;
  const RebindToSigned<decltype(d)> di;
  const auto mask = MaskFromVec(BitCast(d, BroadcastSignBit(BitCast(di, v))));
  return IfThenElse(mask, Zero(d), v);
}

// ------------------------------ IfNegativeThenElse
template <typename T, size_t N>
HWY_API Vec128<T, N> IfNegativeThenElse(Vec128<T, N> v, Vec128<T, N> yes,
                                        Vec128<T, N> no) {
  static_assert(IsSigned<T>(), "Only works for signed/float");

  const DFromV<decltype(v)> d;
  const RebindToSigned<decltype(d)> di;
  return IfThenElse(MaskFromVec(BitCast(d, BroadcastSignBit(BitCast(di, v)))),
                    yes, no);
}

// Absolute value of difference.
template <size_t N>
HWY_API Vec128<float, N> AbsDiff(Vec128<float, N> a, Vec128<float, N> b) {
  return Abs(a - b);
}

// ------------------------------ Floating-point multiply-add variants

// Returns mul * x + add
template <typename T, size_t N, HWY_IF_FLOAT(T)>
HWY_API Vec128<T, N> MulAdd(Vec128<T, N> mul, Vec128<T, N> x,
                            Vec128<T, N> add) {
  return Vec128<T, N>{vec_madd(mul.raw, x.raw, add.raw)};
}

// Returns add - mul * x
template <typename T, size_t N, HWY_IF_FLOAT(T)>
HWY_API Vec128<T, N> NegMulAdd(Vec128<T, N> mul, Vec128<T, N> x,
                               Vec128<T, N> add) {
  // NOTE: the vec_nmsub operation below computes -(mul * x - add),
  // which is equivalent to add - mul * x in the round-to-nearest
  // and round-towards-zero rounding modes
  return Vec128<T, N>{vec_nmsub(mul.raw, x.raw, add.raw)};
}

// Returns mul * x - sub
template <typename T, size_t N, HWY_IF_FLOAT(T)>
HWY_API Vec128<T, N> MulSub(Vec128<T, N> mul, Vec128<T, N> x,
                            Vec128<T, N> sub) {
  return Vec128<T, N>{vec_msub(mul.raw, x.raw, sub.raw)};
}

// Returns -mul * x - sub
template <typename T, size_t N, HWY_IF_FLOAT(T)>
HWY_API Vec128<T, N> NegMulSub(Vec128<T, N> mul, Vec128<T, N> x,
                               Vec128<T, N> sub) {
  // NOTE: The vec_nmadd operation below computes -(mul * x + sub),
  // which is equivalent to -mul * x - sub in the round-to-nearest
  // and round-towards-zero rounding modes
  return Vec128<T, N>{vec_nmadd(mul.raw, x.raw, sub.raw)};
}

// ------------------------------ Floating-point div
// Approximate reciprocal
template <size_t N>
HWY_API Vec128<float, N> ApproximateReciprocal(Vec128<float, N> v) {
  return Vec128<float, N>{vec_re(v.raw)};
}

template <typename T, size_t N, HWY_IF_FLOAT(T)>
HWY_API Vec128<T, N> operator/(Vec128<T, N> a, Vec128<T, N> b) {
  return Vec128<T, N>{vec_div(a.raw, b.raw)};
}

// ------------------------------ Floating-point square root

// Approximate reciprocal square root
template <size_t N>
HWY_API Vec128<float, N> ApproximateReciprocalSqrt(Vec128<float, N> v) {
  return Vec128<float, N>{vec_rsqrte(v.raw)};
}

// Full precision square root
template <class T, size_t N, HWY_IF_FLOAT(T)>
HWY_API Vec128<T, N> Sqrt(Vec128<T, N> v) {
  return Vec128<T, N>{vec_sqrt(v.raw)};
}

// ------------------------------ Min (Gt, IfThenElse)

template <typename T, size_t N, HWY_IF_NOT_SPECIAL_FLOAT(T)>
HWY_API Vec128<T, N> Min(Vec128<T, N> a, Vec128<T, N> b) {
  return Vec128<T, N>{vec_min(a.raw, b.raw)};
}

// ------------------------------ Max (Gt, IfThenElse)

template <typename T, size_t N, HWY_IF_NOT_SPECIAL_FLOAT(T)>
HWY_API Vec128<T, N> Max(Vec128<T, N> a, Vec128<T, N> b) {
  return Vec128<T, N>{vec_max(a.raw, b.raw)};
}

// ------------------------------- Integer AbsDiff for PPC9/PPC10

#if HWY_PPC_HAVE_9
#ifdef HWY_NATIVE_INTEGER_ABS_DIFF
#undef HWY_NATIVE_INTEGER_ABS_DIFF
#else
#define HWY_NATIVE_INTEGER_ABS_DIFF
#endif

template <class V, HWY_IF_UNSIGNED_V(V),
          HWY_IF_T_SIZE_ONE_OF_V(V, (1 << 1) | (1 << 2) | (1 << 4))>
HWY_API V AbsDiff(const V a, const V b) {
  return V{vec_absd(a.raw, b.raw)};
}

template <class V, HWY_IF_U64_D(DFromV<V>)>
HWY_API V AbsDiff(const V a, const V b) {
  return Sub(Max(a, b), Min(a, b));
}

template <class V, HWY_IF_SIGNED_V(V)>
HWY_API V AbsDiff(const V a, const V b) {
  return Sub(Max(a, b), Min(a, b));
}

#endif  // HWY_PPC_HAVE_9

// ================================================== MEMORY (3)

// ------------------------------ Non-temporal stores

template <class D>
HWY_API void Stream(VFromD<D> v, D d, TFromD<D>* HWY_RESTRICT aligned) {
  __builtin_prefetch(aligned, 1, 0);
  Store(v, d, aligned);
}

// ------------------------------ Scatter

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

// ------------------------------ Gather (Load/Store)

template <class D, typename T = TFromD<D>, class VI>
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

template <class D, typename T = TFromD<D>, class VI>
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

// ================================================== SWIZZLE (2)

// ------------------------------ LowerHalf

// Returns upper/lower half of a vector.
template <class D, HWY_IF_V_SIZE_LE_D(D, 8)>
HWY_API VFromD<D> LowerHalf(D /* tag */, VFromD<Twice<D>> v) {
  return VFromD<D>{v.raw};
}
template <typename T, size_t N>
HWY_API Vec128<T, N / 2> LowerHalf(Vec128<T, N> v) {
  return Vec128<T, N / 2>{v.raw};
}

// ------------------------------ ShiftLeftBytes

// NOTE: The ShiftLeftBytes operation moves the elements of v to the right
// by kBytes bytes and zeroes out the first kBytes bytes of v on both
// little-endian and big-endian PPC targets
// (same behavior as the HWY_EMU128 ShiftLeftBytes operation on both
// little-endian and big-endian targets)

template <int kBytes, class D>
HWY_API VFromD<D> ShiftLeftBytes(D d, VFromD<D> v) {
  static_assert(0 <= kBytes && kBytes <= 16, "Invalid kBytes");
  if (kBytes == 0) return v;
  const auto zeros = Zero(d);
#if HWY_IS_LITTLE_ENDIAN
  return VFromD<D>{vec_sld(v.raw, zeros.raw, kBytes)};
#else
  return VFromD<D>{vec_sld(zeros.raw, v.raw, (-kBytes) & 15)};
#endif
}

template <int kBytes, typename T, size_t N>
HWY_API Vec128<T, N> ShiftLeftBytes(Vec128<T, N> v) {
  return ShiftLeftBytes<kBytes>(DFromV<decltype(v)>(), v);
}

// ------------------------------ ShiftLeftLanes

// NOTE: The ShiftLeftLanes operation moves the elements of v to the right
// by kLanes lanes and zeroes out the first kLanes lanes of v on both
// little-endian and big-endian PPC targets
// (same behavior as the HWY_EMU128 ShiftLeftLanes operation on both
// little-endian and big-endian targets)

template <int kLanes, class D, typename T = TFromD<D>>
HWY_API VFromD<D> ShiftLeftLanes(D d, VFromD<D> v) {
  const Repartition<uint8_t, decltype(d)> d8;
  return BitCast(d, ShiftLeftBytes<kLanes * sizeof(T)>(BitCast(d8, v)));
}

template <int kLanes, typename T, size_t N>
HWY_API Vec128<T, N> ShiftLeftLanes(Vec128<T, N> v) {
  return ShiftLeftLanes<kLanes>(DFromV<decltype(v)>(), v);
}

// ------------------------------ ShiftRightBytes

// NOTE: The ShiftRightBytes operation moves the elements of v to the left
// by kBytes bytes and zeroes out the last kBytes bytes of v on both
// little-endian and big-endian PPC targets
// (same behavior as the HWY_EMU128 ShiftRightBytes operation on both
// little-endian and big-endian targets)

template <int kBytes, class D>
HWY_API VFromD<D> ShiftRightBytes(D d, VFromD<D> v) {
  static_assert(0 <= kBytes && kBytes <= 16, "Invalid kBytes");
  if (kBytes == 0) return v;

  // For partial vectors, clear upper lanes so we shift in zeros.
  if (d.MaxBytes() != 16) {
    const Full128<TFromD<D>> dfull;
    VFromD<decltype(dfull)> vfull{v.raw};
    v = VFromD<D>{IfThenElseZero(FirstN(dfull, MaxLanes(d)), vfull).raw};
  }

  const auto zeros = Zero(d);
#if HWY_IS_LITTLE_ENDIAN
  return VFromD<D>{vec_sld(zeros.raw, v.raw, (-kBytes) & 15)};
#else
  return VFromD<D>{vec_sld(v.raw, zeros.raw, kBytes)};
#endif
}

// ------------------------------ ShiftRightLanes

// NOTE: The ShiftRightLanes operation moves the elements of v to the left
// by kLanes lanes and zeroes out the last kLanes lanes of v on both
// little-endian and big-endian PPC targets
// (same behavior as the HWY_EMU128 ShiftRightLanes operation on both
// little-endian and big-endian targets)

template <int kLanes, class D>
HWY_API VFromD<D> ShiftRightLanes(D d, VFromD<D> v) {
  const Repartition<uint8_t, decltype(d)> d8;
  constexpr size_t kBytes = kLanes * sizeof(TFromD<D>);
  return BitCast(d, ShiftRightBytes<kBytes>(d8, BitCast(d8, v)));
}

// ------------------------------ UpperHalf (ShiftRightBytes)

template <class D, HWY_IF_V_SIZE_LE_D(D, 8)>
HWY_API VFromD<D> UpperHalf(D d, VFromD<Twice<D>> v) {
  return LowerHalf(d, ShiftRightBytes<d.MaxBytes()>(Twice<D>(), v));
}

// ------------------------------ ExtractLane (UpperHalf)

template <typename T, size_t N>
HWY_API T ExtractLane(Vec128<T, N> v, size_t i) {
  return static_cast<T>(v.raw[i]);
}

// ------------------------------ InsertLane (UpperHalf)

template <typename T, size_t N>
HWY_API Vec128<T, N> InsertLane(Vec128<T, N> v, size_t i, T t) {
  typename detail::Raw128<T>::type raw_result = v.raw;
  raw_result[i] = t;
  return Vec128<T, N>{raw_result};
}

// ------------------------------ CombineShiftRightBytes

// NOTE: The CombineShiftRightBytes operation below moves the elements of lo to
// the left by kBytes bytes and moves the elements of hi right by (d.MaxBytes()
// - kBytes) bytes on both little-endian and big-endian PPC targets.

template <int kBytes, class D, HWY_IF_V_SIZE_D(D, 16), typename T = TFromD<D>>
HWY_API Vec128<T> CombineShiftRightBytes(D /*d*/, Vec128<T> hi, Vec128<T> lo) {
  constexpr size_t kSize = 16;
  static_assert(0 < kBytes && kBytes < kSize, "kBytes invalid");
#if HWY_IS_LITTLE_ENDIAN
  return Vec128<T>{vec_sld(hi.raw, lo.raw, (-kBytes) & 15)};
#else
  return Vec128<T>{vec_sld(lo.raw, hi.raw, kBytes)};
#endif
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

// ------------------------------ Broadcast/splat any lane

template <int kLane, typename T, size_t N>
HWY_API Vec128<T, N> Broadcast(Vec128<T, N> v) {
  static_assert(0 <= kLane && kLane < N, "Invalid lane");
  return Vec128<T, N>{vec_splat(v.raw, kLane)};
}

// ------------------------------ TableLookupLanes (Shuffle01)

// Returned by SetTableIndices/IndicesFromVec for use by TableLookupLanes.
template <typename T, size_t N = 16 / sizeof(T)>
struct Indices128 {
  __vector unsigned char raw;
};

namespace detail {

template <class D, HWY_IF_T_SIZE_D(D, 1)>
HWY_INLINE VFromD<Repartition<uint8_t, D>> IndicesFromVecBroadcastLaneBytes(
    D d) {
  const Repartition<uint8_t, decltype(d)> d8;
  return Iota(d8, 0);
}

template <class D, HWY_IF_T_SIZE_D(D, 2)>
HWY_INLINE VFromD<Repartition<uint8_t, D>> IndicesFromVecBroadcastLaneBytes(
    D d) {
  const Repartition<uint8_t, decltype(d)> d8;
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
  constexpr __vector unsigned char kBroadcastLaneBytes = {
      0, 0, 2, 2, 4, 4, 6, 6, 8, 8, 10, 10, 12, 12, 14, 14};
#else
  constexpr __vector unsigned char kBroadcastLaneBytes = {
      1, 1, 3, 3, 5, 5, 7, 7, 9, 9, 11, 11, 13, 13, 15, 15};
#endif
  return VFromD<decltype(d8)>{kBroadcastLaneBytes};
}

template <class D, HWY_IF_T_SIZE_D(D, 4)>
HWY_INLINE VFromD<Repartition<uint8_t, D>> IndicesFromVecBroadcastLaneBytes(
    D d) {
  const Repartition<uint8_t, decltype(d)> d8;
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
  constexpr __vector unsigned char kBroadcastLaneBytes = {
      0, 0, 0, 0, 4, 4, 4, 4, 8, 8, 8, 8, 12, 12, 12, 12};
#else
  constexpr __vector unsigned char kBroadcastLaneBytes = {
      3, 3, 3, 3, 7, 7, 7, 7, 11, 11, 11, 11, 15, 15, 15, 15};
#endif
  return VFromD<decltype(d8)>{kBroadcastLaneBytes};
}

template <class D, HWY_IF_T_SIZE_D(D, 8)>
HWY_INLINE VFromD<Repartition<uint8_t, D>> IndicesFromVecBroadcastLaneBytes(
    D d) {
  const Repartition<uint8_t, decltype(d)> d8;
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
  constexpr __vector unsigned char kBroadcastLaneBytes = {
      0, 0, 0, 0, 0, 0, 0, 0, 8, 8, 8, 8, 8, 8, 8, 8};
#else
  constexpr __vector unsigned char kBroadcastLaneBytes = {
      7, 7, 7, 7, 7, 7, 7, 7, 15, 15, 15, 15, 15, 15, 15, 15};
#endif
  return VFromD<decltype(d8)>{kBroadcastLaneBytes};
}

template <class D, HWY_IF_T_SIZE_D(D, 1)>
HWY_INLINE VFromD<Repartition<uint8_t, D>> IndicesFromVecByteOffsets(D d) {
  const Repartition<uint8_t, decltype(d)> d8;
  return Zero(d8);
}

template <class D, HWY_IF_T_SIZE_D(D, 2)>
HWY_INLINE VFromD<Repartition<uint8_t, D>> IndicesFromVecByteOffsets(D d) {
  const Repartition<uint8_t, decltype(d)> d8;
  constexpr __vector unsigned char kByteOffsets = {0, 1, 0, 1, 0, 1, 0, 1,
                                                   0, 1, 0, 1, 0, 1, 0, 1};
  return VFromD<decltype(d8)>{kByteOffsets};
}

template <class D, HWY_IF_T_SIZE_D(D, 4)>
HWY_INLINE VFromD<Repartition<uint8_t, D>> IndicesFromVecByteOffsets(D d) {
  const Repartition<uint8_t, decltype(d)> d8;
  constexpr __vector unsigned char kByteOffsets = {0, 1, 2, 3, 0, 1, 2, 3,
                                                   0, 1, 2, 3, 0, 1, 2, 3};
  return VFromD<decltype(d8)>{kByteOffsets};
}

template <class D, HWY_IF_T_SIZE_D(D, 8)>
HWY_INLINE VFromD<Repartition<uint8_t, D>> IndicesFromVecByteOffsets(D d) {
  const Repartition<uint8_t, decltype(d)> d8;
  constexpr __vector unsigned char kByteOffsets = {0, 1, 2, 3, 4, 5, 6, 7,
                                                   0, 1, 2, 3, 4, 5, 6, 7};
  return VFromD<decltype(d8)>{kByteOffsets};
}

}  // namespace detail

template <class D, typename TI, HWY_IF_T_SIZE_D(D, 1)>
HWY_API Indices128<TFromD<D>, MaxLanes(D())> IndicesFromVec(
    D d, Vec128<TI, MaxLanes(D())> vec) {
  using T = TFromD<D>;
  static_assert(sizeof(T) == sizeof(TI), "Index size must match lane");
#if HWY_IS_DEBUG_BUILD
  const RebindToUnsigned<decltype(d)> du;
  using TU = TFromD<decltype(du)>;
  HWY_DASSERT(AllTrue(
      du, Lt(BitCast(du, vec), Set(du, static_cast<TU>(MaxLanes(d) * 2)))));
#endif

  const Repartition<uint8_t, decltype(d)> d8;
  return Indices128<TFromD<D>, MaxLanes(D())>{BitCast(d8, vec).raw};
}

template <class D, typename TI,
          HWY_IF_T_SIZE_ONE_OF_D(D, (1 << 2) | (1 << 4) | (1 << 8))>
HWY_API Indices128<TFromD<D>, MaxLanes(D())> IndicesFromVec(
    D d, Vec128<TI, MaxLanes(D())> vec) {
  using T = TFromD<D>;
  static_assert(sizeof(T) == sizeof(TI), "Index size must match lane");
#if HWY_IS_DEBUG_BUILD
  const RebindToUnsigned<decltype(d)> du;
  using TU = TFromD<decltype(du)>;
  HWY_DASSERT(AllTrue(
      du, Lt(BitCast(du, vec), Set(du, static_cast<TU>(MaxLanes(d) * 2)))));
#endif

  const Repartition<uint8_t, decltype(d)> d8;
  using V8 = VFromD<decltype(d8)>;

  // Broadcast each lane index to all bytes of T and shift to bytes
  const V8 lane_indices = TableLookupBytes(
      BitCast(d8, vec), detail::IndicesFromVecBroadcastLaneBytes(d));
  constexpr int kIndexShiftAmt = static_cast<int>(FloorLog2(sizeof(T)));
  const V8 byte_indices = ShiftLeft<kIndexShiftAmt>(lane_indices);
  const V8 sum = Add(byte_indices, detail::IndicesFromVecByteOffsets(d));
  return Indices128<TFromD<D>, MaxLanes(D())>{sum.raw};
}

template <class D, typename TI>
HWY_API Indices128<TFromD<D>, HWY_MAX_LANES_D(D)> SetTableIndices(
    D d, const TI* idx) {
  const Rebind<TI, decltype(d)> di;
  return IndicesFromVec(d, LoadU(di, idx));
}

template <typename T, size_t N>
HWY_API Vec128<T, N> TableLookupLanes(Vec128<T, N> v, Indices128<T, N> idx) {
  const DFromV<decltype(v)> d;
  const Repartition<uint8_t, decltype(d)> d8;
  return BitCast(d, TableLookupBytes(v, VFromD<decltype(d8)>{idx.raw}));
}

// Single lane: no change
template <typename T>
HWY_API Vec128<T, 1> TableLookupLanes(Vec128<T, 1> v,
                                      Indices128<T, 1> /* idx */) {
  return v;
}

template <typename T, size_t N, HWY_IF_V_SIZE_LE(T, N, 8)>
HWY_API Vec128<T, N> TwoTablesLookupLanes(Vec128<T, N> a, Vec128<T, N> b,
                                          Indices128<T, N> idx) {
  const DFromV<decltype(a)> d;
  const Twice<decltype(d)> dt;
  const Repartition<uint8_t, decltype(dt)> dt_u8;
// TableLookupLanes currently requires table and index vectors to be the same
// size, though a half-length index vector would be sufficient here.
#if HWY_IS_MSAN
  const Vec128<T, N> idx_vec{idx.raw};
  const Indices128<T, N * 2> idx2{Combine(dt, idx_vec, idx_vec).raw};
#else
  // We only keep LowerHalf of the result, which is valid in idx.
  const Indices128<T, N * 2> idx2{idx.raw};
#endif
  return LowerHalf(
      d, TableLookupBytes(Combine(dt, b, a),
                          BitCast(dt, VFromD<decltype(dt_u8)>{idx2.raw})));
}

template <typename T>
HWY_API Vec128<T> TwoTablesLookupLanes(Vec128<T> a, Vec128<T> b,
                                       Indices128<T> idx) {
  return Vec128<T>{vec_perm(a.raw, b.raw, idx.raw)};
}

// ------------------------------ ReverseBlocks

// Single block: no change
template <class D>
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
HWY_API Vec64<T> Reverse(D /* tag */, Vec64<T> v) {
  return Vec64<T>{Shuffle2301(Vec128<T>{v.raw}).raw};
}

// 16-bit x4: shuffle
template <class D, typename T = TFromD<D>, HWY_IF_T_SIZE(T, 2)>
HWY_API Vec64<T> Reverse(D /* tag */, Vec64<T> v) {
  const __vector unsigned char kShuffle = {6,  7,  4,  5,  2,  3,  0, 1,
                                           14, 15, 12, 13, 10, 11, 8, 9};
  return Vec64<T>{vec_perm(v.raw, v.raw, kShuffle)};
}

// 16-bit x2: rotate bytes
template <class D, typename T = TFromD<D>, HWY_IF_T_SIZE(T, 2)>
HWY_API Vec32<T> Reverse(D d, Vec32<T> v) {
  const RepartitionToWide<RebindToUnsigned<decltype(d)>> du32;
  return BitCast(d, RotateRight<16>(Reverse(du32, BitCast(du32, v))));
}

// ------------------------------- ReverseLaneBytes

#if HWY_PPC_HAVE_9 && \
    (HWY_COMPILER_GCC_ACTUAL >= 710 || HWY_COMPILER_CLANG >= 400)

// Per-target flag to prevent generic_ops-inl.h defining 8-bit ReverseLaneBytes.
#ifdef HWY_NATIVE_REVERSE_LANE_BYTES
#undef HWY_NATIVE_REVERSE_LANE_BYTES
#else
#define HWY_NATIVE_REVERSE_LANE_BYTES
#endif

template <class V, HWY_IF_NOT_FLOAT_NOR_SPECIAL_V(V),
          HWY_IF_T_SIZE_ONE_OF_V(V, (1 << 2) | (1 << 4) | (1 << 8))>
HWY_API V ReverseLaneBytes(V v) {
  return V{vec_revb(v.raw)};
}

// Per-target flag to prevent generic_ops-inl.h defining 8-bit Reverse2/4/8.
#ifdef HWY_NATIVE_REVERSE2_8
#undef HWY_NATIVE_REVERSE2_8
#else
#define HWY_NATIVE_REVERSE2_8
#endif

template <class D, typename T = TFromD<D>, HWY_IF_T_SIZE(T, 1)>
HWY_API VFromD<D> Reverse2(D d, VFromD<D> v) {
  const Repartition<uint16_t, decltype(d)> du16;
  return BitCast(d, ReverseLaneBytes(BitCast(du16, v)));
}

template <class D, typename T = TFromD<D>, HWY_IF_T_SIZE(T, 1)>
HWY_API VFromD<D> Reverse4(D d, VFromD<D> v) {
  const Repartition<uint32_t, decltype(d)> du32;
  return BitCast(d, ReverseLaneBytes(BitCast(du32, v)));
}

template <class D, typename T = TFromD<D>, HWY_IF_T_SIZE(T, 1)>
HWY_API VFromD<D> Reverse8(D d, VFromD<D> v) {
  const Repartition<uint64_t, decltype(d)> du64;
  return BitCast(d, ReverseLaneBytes(BitCast(du64, v)));
}

#endif  // HWY_PPC_HAVE_9

template <class D, typename T = TFromD<D>, HWY_IF_T_SIZE(T, 1)>
HWY_API Vec16<T> Reverse(D d, Vec16<T> v) {
  return Reverse2(d, v);
}

template <class D, typename T = TFromD<D>, HWY_IF_T_SIZE(T, 1)>
HWY_API Vec32<T> Reverse(D d, Vec32<T> v) {
  return Reverse4(d, v);
}

template <class D, typename T = TFromD<D>, HWY_IF_T_SIZE(T, 1)>
HWY_API Vec64<T> Reverse(D d, Vec64<T> v) {
  return Reverse8(d, v);
}

// ------------------------------ Reverse2

// Single lane: no change
template <class D, typename T = TFromD<D>, HWY_IF_LANES_D(D, 1)>
HWY_API Vec128<T, 1> Reverse2(D /* tag */, Vec128<T, 1> v) {
  return v;
}

template <class D, typename T = TFromD<D>, HWY_IF_T_SIZE(T, 2)>
HWY_API VFromD<D> Reverse2(D d, VFromD<D> v) {
  const Repartition<uint32_t, decltype(d)> du32;
  return BitCast(d, RotateRight<16>(BitCast(du32, v)));
}

template <class D, typename T = TFromD<D>, HWY_IF_T_SIZE(T, 4)>
HWY_API VFromD<D> Reverse2(D d, VFromD<D> v) {
  const Repartition<uint64_t, decltype(d)> du64;
  return BitCast(d, RotateRight<32>(BitCast(du64, v)));
}

template <class D, typename T = TFromD<D>, HWY_IF_T_SIZE(T, 8)>
HWY_API VFromD<D> Reverse2(D /* tag */, VFromD<D> v) {
  return Shuffle01(v);
}

// ------------------------------ Reverse4

template <class D, HWY_IF_T_SIZE_D(D, 2)>
HWY_API VFromD<D> Reverse4(D /*d*/, VFromD<D> v) {
  const __vector unsigned char kShuffle = {6,  7,  4,  5,  2,  3,  0, 1,
                                           14, 15, 12, 13, 10, 11, 8, 9};
  return VFromD<D>{vec_perm(v.raw, v.raw, kShuffle)};
}

template <class D, HWY_IF_T_SIZE_D(D, 4)>
HWY_API VFromD<D> Reverse4(D d, VFromD<D> v) {
  return Reverse(d, v);
}

template <class D, HWY_IF_T_SIZE_D(D, 8)>
HWY_API VFromD<D> Reverse4(D /* tag */, VFromD<D> /* v */) {
  HWY_ASSERT(0);  // don't have 4 u64 lanes
}

// ------------------------------ Reverse8

template <class D, HWY_IF_T_SIZE_D(D, 2)>
HWY_API VFromD<D> Reverse8(D d, VFromD<D> v) {
  return Reverse(d, v);
}

template <class D, HWY_IF_T_SIZE_ONE_OF_D(D, (1 << 4) | (1 << 8))>
HWY_API VFromD<D> Reverse8(D /* tag */, VFromD<D> /* v */) {
  HWY_ASSERT(0);  // don't have 8 lanes if larger than 16-bit
}

// ------------------------------ InterleaveLower

// Interleaves lanes from halves of the 128-bit blocks of "a" (which provides
// the least-significant lane) and "b". To concatenate two half-width integers
// into one, use ZipLower/Upper instead (also works with scalar).

template <typename T, size_t N>
HWY_API Vec128<T, N> InterleaveLower(Vec128<T, N> a, Vec128<T, N> b) {
  return Vec128<T, N>{vec_mergeh(a.raw, b.raw)};
}

// Additional overload for the optional tag
template <class D>
HWY_API VFromD<D> InterleaveLower(D /* tag */, VFromD<D> a, VFromD<D> b) {
  return InterleaveLower(a, b);
}

// ------------------------------ InterleaveUpper (UpperHalf)

// Full
template <class D, typename T = TFromD<D>>
HWY_API Vec128<T> InterleaveUpper(D /* tag */, Vec128<T> a, Vec128<T> b) {
  return Vec128<T>{vec_mergel(a.raw, b.raw)};
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
  // Treat half-width input as one lane, and expand to two lanes.
  using VU = Vec128<UnsignedFromSize<dh.MaxBytes()>, 2>;
  using Raw = typename detail::Raw128<TFromV<VU>>::type;
  const VU lo{reinterpret_cast<Raw>(lo_half.raw)};
  const VU hi{reinterpret_cast<Raw>(hi_half.raw)};
  return BitCast(d, InterleaveLower(lo, hi));
}

// ------------------------------ ZeroExtendVector (Combine, IfThenElseZero)

template <class D>
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
HWY_API Vec128<T> ConcatUpperLower(D /*d*/, Vec128<T> hi, Vec128<T> lo) {
  const __vector unsigned char kShuffle = {0,  1,  2,  3,  4,  5,  6,  7,
                                           24, 25, 26, 27, 28, 29, 30, 31};
  return Vec128<T>{vec_perm(lo.raw, hi.raw, kShuffle)};
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
HWY_API VFromD<D> ConcatLowerUpper(D d, VFromD<D> hi, VFromD<D> lo) {
  const Half<decltype(d)> d2;
  return Combine(d, LowerHalf(d2, hi), UpperHalf(d2, lo));
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 8)>
HWY_API VFromD<D> ConcatUpperLower(D d, VFromD<D> hi, VFromD<D> lo) {
  const Half<decltype(d)> d2;
  return Combine(d, UpperHalf(d2, hi), LowerHalf(d2, lo));
}

// ------------------------------ TruncateTo

template <class D, typename FromT, HWY_IF_UNSIGNED_D(D), HWY_IF_UNSIGNED(FromT),
          hwy::EnableIf<(sizeof(FromT) >= sizeof(TFromD<D>) * 2)>* = nullptr,
          HWY_IF_LANES_D(D, 1)>
HWY_API VFromD<D> TruncateTo(D /* tag */, Vec128<FromT, 1> v) {
  using Raw = typename detail::Raw128<TFromD<D>>::type;
#if HWY_IS_LITTLE_ENDIAN
  return VFromD<D>{reinterpret_cast<Raw>(v.raw)};
#else
  return VFromD<D>{reinterpret_cast<Raw>(
      vec_sld(v.raw, v.raw, sizeof(FromT) - sizeof(TFromD<D>)))};
#endif
}

namespace detail {

template <class D, typename FromT, HWY_IF_UNSIGNED_D(D), HWY_IF_UNSIGNED(FromT),
          HWY_IF_T_SIZE(FromT, sizeof(TFromD<D>) * 2), HWY_IF_LANES_GT_D(D, 1)>
HWY_API VFromD<D> Truncate2To(
    D /* tag */, Vec128<FromT, Repartition<FromT, D>().MaxLanes()> lo,
    Vec128<FromT, Repartition<FromT, D>().MaxLanes()> hi) {
  return VFromD<D>{vec_pack(lo.raw, hi.raw)};
}

}  // namespace detail

template <class D, typename FromT, HWY_IF_UNSIGNED_D(D), HWY_IF_UNSIGNED(FromT),
          HWY_IF_T_SIZE(FromT, sizeof(TFromD<D>) * 2), HWY_IF_LANES_GT_D(D, 1)>
HWY_API VFromD<D> TruncateTo(D /* d */,
                             Vec128<FromT, Rebind<FromT, D>().MaxLanes()> v) {
  return VFromD<D>{vec_pack(v.raw, v.raw)};
}

template <class D, typename FromT, HWY_IF_UNSIGNED_D(D), HWY_IF_UNSIGNED(FromT),
          hwy::EnableIf<(sizeof(FromT) >= sizeof(TFromD<D>) * 4)>* = nullptr,
          HWY_IF_LANES_GT_D(D, 1)>
HWY_API VFromD<D> TruncateTo(D d,
                             Vec128<FromT, Rebind<FromT, D>().MaxLanes()> v) {
  const Rebind<MakeNarrow<FromT>, decltype(d)> d2;
  return TruncateTo(d, TruncateTo(d2, v));
}

// ------------------------------ ConcatOdd (TruncateTo)

// 8-bit full
template <class D, typename T = TFromD<D>, HWY_IF_T_SIZE(T, 1)>
HWY_API Vec128<T> ConcatOdd(D d, Vec128<T> hi, Vec128<T> lo) {
  const Repartition<uint16_t, decltype(d)> dw;
  const RebindToUnsigned<decltype(d)> du;
#if HWY_IS_LITTLE_ENDIAN
  // Right-shift 8 bits per u16 so we can pack.
  const Vec128<uint16_t> uH = ShiftRight<8>(BitCast(dw, hi));
  const Vec128<uint16_t> uL = ShiftRight<8>(BitCast(dw, lo));
#else
  const Vec128<uint16_t> uH = BitCast(dw, hi);
  const Vec128<uint16_t> uL = BitCast(dw, lo);
#endif
  return BitCast(d, detail::Truncate2To(du, uL, uH));
}

// 8-bit x8
template <class D, typename T = TFromD<D>, HWY_IF_T_SIZE(T, 1)>
HWY_API Vec64<T> ConcatOdd(D /*d*/, Vec64<T> hi, Vec64<T> lo) {
  // Don't care about upper half, no need to zero.
  const __vector unsigned char kCompactOddU8 = {1, 3, 5, 7, 17, 19, 21, 23};
  return Vec64<T>{vec_perm(lo.raw, hi.raw, kCompactOddU8)};
}

// 8-bit x4
template <class D, typename T = TFromD<D>, HWY_IF_T_SIZE(T, 1)>
HWY_API Vec32<T> ConcatOdd(D /*d*/, Vec32<T> hi, Vec32<T> lo) {
  // Don't care about upper half, no need to zero.
  const __vector unsigned char kCompactOddU8 = {1, 3, 17, 19};
  return Vec32<T>{vec_perm(lo.raw, hi.raw, kCompactOddU8)};
}

// 16-bit full
template <class D, typename T = TFromD<D>, HWY_IF_T_SIZE(T, 2)>
HWY_API Vec128<T> ConcatOdd(D d, Vec128<T> hi, Vec128<T> lo) {
  const Repartition<uint32_t, decltype(d)> dw;
  const RebindToUnsigned<decltype(d)> du;
#if HWY_IS_LITTLE_ENDIAN
  const Vec128<uint32_t> uH = ShiftRight<16>(BitCast(dw, hi));
  const Vec128<uint32_t> uL = ShiftRight<16>(BitCast(dw, lo));
#else
  const Vec128<uint32_t> uH = BitCast(dw, hi);
  const Vec128<uint32_t> uL = BitCast(dw, lo);
#endif
  return BitCast(d, detail::Truncate2To(du, uL, uH));
}

// 16-bit x4
template <class D, typename T = TFromD<D>, HWY_IF_T_SIZE(T, 2)>
HWY_API Vec64<T> ConcatOdd(D /*d*/, Vec64<T> hi, Vec64<T> lo) {
  // Don't care about upper half, no need to zero.
  const __vector unsigned char kCompactOddU16 = {2, 3, 6, 7, 18, 19, 22, 23};
  return Vec64<T>{vec_perm(lo.raw, hi.raw, kCompactOddU16)};
}

// 32-bit full
template <class D, typename T = TFromD<D>, HWY_IF_T_SIZE(T, 4)>
HWY_API Vec128<T> ConcatOdd(D d, Vec128<T> hi, Vec128<T> lo) {
#if HWY_IS_LITTLE_ENDIAN
  (void)d;
  const __vector unsigned char kShuffle = {4,  5,  6,  7,  12, 13, 14, 15,
                                           20, 21, 22, 23, 28, 29, 30, 31};
  return Vec128<T>{vec_perm(lo.raw, hi.raw, kShuffle)};
#else
  const RebindToUnsigned<decltype(d)> du;
  const Repartition<uint64_t, decltype(d)> dw;
  return BitCast(d, detail::Truncate2To(du, BitCast(dw, lo), BitCast(dw, hi)));
#endif
}

// Any type x2
template <class D, typename T = TFromD<D>, HWY_IF_LANES_D(D, 2)>
HWY_API Vec128<T, 2> ConcatOdd(D d, Vec128<T, 2> hi, Vec128<T, 2> lo) {
  return InterleaveUpper(d, lo, hi);
}

// ------------------------------ ConcatEven (TruncateTo)

// 8-bit full
template <class D, typename T = TFromD<D>, HWY_IF_T_SIZE(T, 1)>
HWY_API Vec128<T> ConcatEven(D d, Vec128<T> hi, Vec128<T> lo) {
  const Repartition<uint16_t, decltype(d)> dw;
  const RebindToUnsigned<decltype(d)> du;
#if HWY_IS_LITTLE_ENDIAN
  const Vec128<uint16_t> uH = BitCast(dw, hi);
  const Vec128<uint16_t> uL = BitCast(dw, lo);
#else
  // Right-shift 8 bits per u16 so we can pack.
  const Vec128<uint16_t> uH = ShiftRight<8>(BitCast(dw, hi));
  const Vec128<uint16_t> uL = ShiftRight<8>(BitCast(dw, lo));
#endif
  return BitCast(d, detail::Truncate2To(du, uL, uH));
}

// 8-bit x8
template <class D, typename T = TFromD<D>, HWY_IF_T_SIZE(T, 1)>
HWY_API Vec64<T> ConcatEven(D /*d*/, Vec64<T> hi, Vec64<T> lo) {
  // Don't care about upper half, no need to zero.
  const __vector unsigned char kCompactEvenU8 = {0, 2, 4, 6, 16, 18, 20, 22};
  return Vec64<T>{vec_perm(lo.raw, hi.raw, kCompactEvenU8)};
}

// 8-bit x4
template <class D, typename T = TFromD<D>, HWY_IF_T_SIZE(T, 1)>
HWY_API Vec32<T> ConcatEven(D /*d*/, Vec32<T> hi, Vec32<T> lo) {
  // Don't care about upper half, no need to zero.
  const __vector unsigned char kCompactEvenU8 = {0, 2, 16, 18};
  return Vec32<T>{vec_perm(lo.raw, hi.raw, kCompactEvenU8)};
}

// 16-bit full
template <class D, typename T = TFromD<D>, HWY_IF_T_SIZE(T, 2)>
HWY_API Vec128<T> ConcatEven(D d, Vec128<T> hi, Vec128<T> lo) {
  // Isolate lower 16 bits per u32 so we can pack.
  const Repartition<uint32_t, decltype(d)> dw;
  const RebindToUnsigned<decltype(d)> du;
#if HWY_IS_LITTLE_ENDIAN
  const Vec128<uint32_t> uH = BitCast(dw, hi);
  const Vec128<uint32_t> uL = BitCast(dw, lo);
#else
  const Vec128<uint32_t> uH = ShiftRight<16>(BitCast(dw, hi));
  const Vec128<uint32_t> uL = ShiftRight<16>(BitCast(dw, lo));
#endif
  return BitCast(d, detail::Truncate2To(du, uL, uH));
}

// 16-bit x4
template <class D, typename T = TFromD<D>, HWY_IF_T_SIZE(T, 2)>
HWY_API Vec64<T> ConcatEven(D /*d*/, Vec64<T> hi, Vec64<T> lo) {
  // Don't care about upper half, no need to zero.
  const __vector unsigned char kCompactEvenU16 = {0, 1, 4, 5, 16, 17, 20, 21};
  return Vec64<T>{vec_perm(lo.raw, hi.raw, kCompactEvenU16)};
}

// 32-bit full
template <class D, typename T = TFromD<D>, HWY_IF_T_SIZE(T, 4)>
HWY_API Vec128<T> ConcatEven(D d, Vec128<T> hi, Vec128<T> lo) {
#if HWY_IS_LITTLE_ENDIAN
  const Repartition<uint64_t, decltype(d)> dw;
  const RebindToUnsigned<decltype(d)> du;
  return BitCast(d, detail::Truncate2To(du, BitCast(dw, lo), BitCast(dw, hi)));
#else
  (void)d;
  constexpr __vector unsigned char kShuffle = {0,  1,  2,  3,  8,  9,  10, 11,
                                               16, 17, 18, 19, 24, 25, 26, 27};
  return Vec128<T>{vec_perm(lo.raw, hi.raw, kShuffle)};
#endif
}

// Any T x2
template <typename D, typename T = TFromD<D>, HWY_IF_LANES_D(D, 2)>
HWY_API Vec128<T, 2> ConcatEven(D d, Vec128<T, 2> hi, Vec128<T, 2> lo) {
  return InterleaveLower(d, lo, hi);
}

// ------------------------------ OrderedTruncate2To (ConcatEven, ConcatOdd)
#ifdef HWY_NATIVE_ORDERED_TRUNCATE_2_TO
#undef HWY_NATIVE_ORDERED_TRUNCATE_2_TO
#else
#define HWY_NATIVE_ORDERED_TRUNCATE_2_TO
#endif

template <class D, HWY_IF_UNSIGNED_D(D), class V, HWY_IF_UNSIGNED_V(V),
          HWY_IF_T_SIZE_V(V, sizeof(TFromD<D>) * 2),
          HWY_IF_LANES_D(D, HWY_MAX_LANES_D(DFromV<V>) * 2)>
HWY_API VFromD<D> OrderedTruncate2To(D d, V a, V b) {
#if HWY_IS_LITTLE_ENDIAN
  return ConcatEven(d, BitCast(d, b), BitCast(d, a));
#else
  return ConcatOdd(d, BitCast(d, b), BitCast(d, a));
#endif
}

// ------------------------------ DupEven (InterleaveLower)

template <typename T, size_t N, HWY_IF_T_SIZE(T, 4)>
HWY_API Vec128<T, N> DupEven(Vec128<T, N> v) {
  return Vec128<T, N>{vec_mergee(v.raw, v.raw)};
}

template <typename T, size_t N, HWY_IF_T_SIZE(T, 8)>
HWY_API Vec128<T, N> DupEven(Vec128<T, N> v) {
  return InterleaveLower(DFromV<decltype(v)>(), v, v);
}

// ------------------------------ DupOdd (InterleaveUpper)

template <typename T, size_t N, HWY_IF_T_SIZE(T, 4)>
HWY_API Vec128<T, N> DupOdd(Vec128<T, N> v) {
  return Vec128<T, N>{vec_mergeo(v.raw, v.raw)};
}

template <typename T, size_t N, HWY_IF_T_SIZE(T, 8)>
HWY_API Vec128<T, N> DupOdd(Vec128<T, N> v) {
  return InterleaveUpper(DFromV<decltype(v)>(), v, v);
}

// ------------------------------ OddEven (IfThenElse)

template <typename T, size_t N, HWY_IF_T_SIZE(T, 1)>
HWY_INLINE Vec128<T, N> OddEven(Vec128<T, N> a, Vec128<T, N> b) {
  const DFromV<decltype(a)> d;
  const __vector unsigned char mask = {0xFF, 0, 0xFF, 0, 0xFF, 0, 0xFF, 0,
                                       0xFF, 0, 0xFF, 0, 0xFF, 0, 0xFF, 0};
  return IfVecThenElse(BitCast(d, Vec128<uint8_t, N>{mask}), b, a);
}

template <typename T, size_t N, HWY_IF_T_SIZE(T, 2)>
HWY_INLINE Vec128<T, N> OddEven(Vec128<T, N> a, Vec128<T, N> b) {
  const DFromV<decltype(a)> d;
  const __vector unsigned char mask = {0xFF, 0xFF, 0, 0, 0xFF, 0xFF, 0, 0,
                                       0xFF, 0xFF, 0, 0, 0xFF, 0xFF, 0, 0};
  return IfVecThenElse(BitCast(d, Vec128<uint8_t, N * 2>{mask}), b, a);
}

template <typename T, size_t N, HWY_IF_T_SIZE(T, 4)>
HWY_INLINE Vec128<T, N> OddEven(Vec128<T, N> a, Vec128<T, N> b) {
  const DFromV<decltype(a)> d;
  const __vector unsigned char mask = {0xFF, 0xFF, 0xFF, 0xFF, 0, 0, 0, 0,
                                       0xFF, 0xFF, 0xFF, 0xFF, 0, 0, 0, 0};
  return IfVecThenElse(BitCast(d, Vec128<uint8_t, N * 4>{mask}), b, a);
}

template <typename T, size_t N, HWY_IF_T_SIZE(T, 8)>
HWY_INLINE Vec128<T, N> OddEven(Vec128<T, N> a, Vec128<T, N> b) {
  // Same as ConcatUpperLower for full vectors; do not call that because this
  // is more efficient for 64x1 vectors.
  const DFromV<decltype(a)> d;
  const __vector unsigned char mask = {
      0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0, 0, 0, 0, 0, 0, 0, 0};
  return IfVecThenElse(BitCast(d, Vec128<uint8_t, N * 8>{mask}), b, a);
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

// ------------------------------ Shl

namespace detail {
template <typename T, size_t N>
HWY_API Vec128<T, N> Shl(hwy::UnsignedTag /*tag*/, Vec128<T, N> v,
                         Vec128<T, N> bits) {
  return Vec128<T, N>{vec_sl(v.raw, bits.raw)};
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

template <typename T, size_t N, HWY_IF_NOT_FLOAT(T)>
HWY_API Vec128<T, N> operator<<(Vec128<T, N> v, Vec128<T, N> bits) {
  return detail::Shl(hwy::TypeTag<T>(), v, bits);
}

// ------------------------------ Shr

namespace detail {
template <typename T, size_t N>
HWY_API Vec128<T, N> Shr(hwy::UnsignedTag /*tag*/, Vec128<T, N> v,
                         Vec128<T, N> bits) {
  return Vec128<T, N>{vec_sr(v.raw, bits.raw)};
}

template <typename T, size_t N>
HWY_API Vec128<T, N> Shr(hwy::SignedTag /*tag*/, Vec128<T, N> v,
                         Vec128<T, N> bits) {
  const DFromV<decltype(v)> di;
  const RebindToUnsigned<decltype(di)> du;
  return Vec128<T, N>{vec_sra(v.raw, BitCast(du, bits).raw)};
}

}  // namespace detail

template <typename T, size_t N>
HWY_API Vec128<T, N> operator>>(Vec128<T, N> v, Vec128<T, N> bits) {
  return detail::Shr(hwy::TypeTag<T>(), v, bits);
}

// ------------------------------ MulEven/Odd 64x64 (UpperHalf)

HWY_INLINE Vec128<uint64_t> MulEven(Vec128<uint64_t> a, Vec128<uint64_t> b) {
#if HWY_PPC_HAVE_10 && defined(__SIZEOF_INT128__)
  using VU64 = __vector unsigned long long;
  const VU64 mul128_result = reinterpret_cast<VU64>(vec_mule(a.raw, b.raw));
#if HWY_IS_LITTLE_ENDIAN
  return Vec128<uint64_t>{mul128_result};
#else
  // Need to swap the two halves of mul128_result on big-endian targets as
  // the upper 64 bits of the product are in lane 0 of mul128_result and
  // the lower 64 bits of the product are in lane 1 of mul128_result
  return Vec128<uint64_t>{vec_sld(mul128_result, mul128_result, 8)};
#endif
#else
  alignas(16) uint64_t mul[2];
  mul[0] = Mul128(GetLane(a), GetLane(b), &mul[1]);
  return Load(Full128<uint64_t>(), mul);
#endif
}

HWY_INLINE Vec128<uint64_t> MulOdd(Vec128<uint64_t> a, Vec128<uint64_t> b) {
#if HWY_PPC_HAVE_10 && defined(__SIZEOF_INT128__)
  using VU64 = __vector unsigned long long;
  const VU64 mul128_result = reinterpret_cast<VU64>(vec_mulo(a.raw, b.raw));
#if HWY_IS_LITTLE_ENDIAN
  return Vec128<uint64_t>{mul128_result};
#else
  // Need to swap the two halves of mul128_result on big-endian targets as
  // the upper 64 bits of the product are in lane 0 of mul128_result and
  // the lower 64 bits of the product are in lane 1 of mul128_result
  return Vec128<uint64_t>{vec_sld(mul128_result, mul128_result, 8)};
#endif
#else
  alignas(16) uint64_t mul[2];
  const Full64<uint64_t> d2;
  mul[0] =
      Mul128(GetLane(UpperHalf(d2, a)), GetLane(UpperHalf(d2, b)), &mul[1]);
  return Load(Full128<uint64_t>(), mul);
#endif
}

// ------------------------------ ReorderWidenMulAccumulate (MulAdd, ZipLower)

template <class D32, HWY_IF_F32_D(D32),
          class V16 = VFromD<Repartition<bfloat16_t, D32>>>
HWY_API VFromD<D32> ReorderWidenMulAccumulate(D32 df32, V16 a, V16 b,
                                              VFromD<D32> sum0,
                                              VFromD<D32>& sum1) {
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

// Even if N=1, the input is always at least 2 lanes, hence vec_msum is safe.
template <class D32, HWY_IF_I32_D(D32),
          class V16 = VFromD<RepartitionToNarrow<D32>>>
HWY_API VFromD<D32> ReorderWidenMulAccumulate(D32 /* tag */, V16 a, V16 b,
                                              VFromD<D32> sum0,
                                              VFromD<D32>& /*sum1*/) {
  return VFromD<D32>{vec_msum(a.raw, b.raw, sum0.raw)};
}

// ------------------------------ RearrangeToOddPlusEven
template <size_t N>
HWY_API Vec128<int32_t, N> RearrangeToOddPlusEven(Vec128<int32_t, N> sum0,
                                                  Vec128<int32_t, N> /*sum1*/) {
  return sum0;  // invariant already holds
}

template <class VW>
HWY_API VW RearrangeToOddPlusEven(const VW sum0, const VW sum1) {
  return Add(sum0, sum1);
}

// ================================================== CONVERT

// ------------------------------ Promotions (part w/ narrow lanes -> full)

// Unsigned to signed/unsigned: zero-extend.
template <class D, typename FromT, HWY_IF_T_SIZE_D(D, 2 * sizeof(FromT)),
          HWY_IF_NOT_FLOAT_D(D), HWY_IF_UNSIGNED(FromT)>
HWY_API VFromD<D> PromoteTo(D /* d */,
                            Vec128<FromT, Rebind<FromT, D>().MaxLanes()> v) {
  // First pretend the input has twice the lanes - the upper half will be
  // ignored by ZipLower.
  const Rebind<FromT, Twice<D>> d2;
  const VFromD<decltype(d2)> twice{v.raw};
  // Then cast to narrow as expected by ZipLower, in case the sign of FromT
  // differs from that of D.
  const RepartitionToNarrow<D> dn;

#if HWY_IS_LITTLE_ENDIAN
  return ZipLower(BitCast(dn, twice), Zero(dn));
#else
  return ZipLower(Zero(dn), BitCast(dn, twice));
#endif
}

// Signed: replicate sign bit.
template <class D, typename FromT, HWY_IF_T_SIZE_D(D, 2 * sizeof(FromT)),
          HWY_IF_NOT_FLOAT_D(D), HWY_IF_SIGNED(FromT)>
HWY_API VFromD<D> PromoteTo(D /* d */,
                            Vec128<FromT, Rebind<FromT, D>().MaxLanes()> v) {
  using Raw = typename detail::Raw128<TFromD<D>>::type;
  return VFromD<D>{reinterpret_cast<Raw>(vec_unpackh(v.raw))};
}

// 8-bit to 32-bit: First, promote to 16-bit, and then convert to 32-bit.
template <class D, typename FromT, HWY_IF_T_SIZE_D(D, 4), HWY_IF_NOT_FLOAT_D(D),
          HWY_IF_T_SIZE(FromT, 1)>
HWY_API VFromD<D> PromoteTo(D d32,
                            Vec128<FromT, Rebind<FromT, D>().MaxLanes()> v) {
  const DFromV<decltype(v)> d8;
  const Rebind<MakeWide<FromT>, decltype(d8)> d16;
  return PromoteTo(d32, PromoteTo(d16, v));
}

// 8-bit or 16-bit to 64-bit: First, promote to MakeWide<FromT>, and then
// convert to 64-bit.
template <class D, typename FromT, HWY_IF_T_SIZE_D(D, 8), HWY_IF_NOT_FLOAT_D(D),
          HWY_IF_NOT_FLOAT_NOR_SPECIAL(FromT),
          HWY_IF_T_SIZE_ONE_OF(FromT, (1 << 1) | (1 << 2))>
HWY_API VFromD<D> PromoteTo(D d64,
                            Vec128<FromT, Rebind<FromT, D>().MaxLanes()> v) {
  const Rebind<MakeWide<FromT>, decltype(d64)> dw;
  return PromoteTo(d64, PromoteTo(dw, v));
}

// Workaround for origin tracking bug in Clang msan prior to 11.0
// (spurious "uninitialized memory" for TestF16 with "ORIGIN: invalid")
#if HWY_IS_MSAN && (HWY_COMPILER_CLANG != 0 && HWY_COMPILER_CLANG < 1100)
#define HWY_INLINE_F16 HWY_NOINLINE
#else
#define HWY_INLINE_F16 HWY_INLINE
#endif
template <class D, HWY_IF_F32_D(D)>
HWY_INLINE_F16 VFromD<D> PromoteTo(D df32, VFromD<Rebind<float16_t, D>> v) {
#if HWY_PPC_HAVE_9
  (void)df32;
  return VFromD<D>{vec_extract_fp32_from_shorth(v.raw)};
#else
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
#endif
}

template <class D, HWY_IF_F32_D(D)>
HWY_API VFromD<D> PromoteTo(D df32, VFromD<Rebind<bfloat16_t, D>> v) {
  const Rebind<uint16_t, decltype(df32)> du16;
  const RebindToSigned<decltype(df32)> di32;
  return BitCast(df32, ShiftLeft<16>(PromoteTo(di32, BitCast(du16, v))));
}

template <class D, HWY_IF_F64_D(D)>
HWY_API VFromD<D> PromoteTo(D /* tag */, VFromD<Rebind<float, D>> v) {
  const __vector float raw_v = InterleaveLower(v, v).raw;
#if HWY_IS_LITTLE_ENDIAN
  return VFromD<D>{vec_doubleo(raw_v)};
#else
  return VFromD<D>{vec_doublee(raw_v)};
#endif
}

template <class D, HWY_IF_F64_D(D)>
HWY_API VFromD<D> PromoteTo(D /* tag */, VFromD<Rebind<int32_t, D>> v) {
  const __vector signed int raw_v = InterleaveLower(v, v).raw;
#if HWY_IS_LITTLE_ENDIAN
  return VFromD<D>{vec_doubleo(raw_v)};
#else
  return VFromD<D>{vec_doublee(raw_v)};
#endif
}

// ------------------------------ Demotions (full -> part w/ narrow lanes)

template <class D, typename FromT, HWY_IF_UNSIGNED_D(D),
          HWY_IF_T_SIZE_ONE_OF_D(D, (1 << 1) | (1 << 2) | (1 << 4)),
          HWY_IF_SIGNED(FromT), HWY_IF_T_SIZE(FromT, sizeof(TFromD<D>) * 2)>
HWY_API VFromD<D> DemoteTo(D /* tag */,
                           Vec128<FromT, Rebind<FromT, D>().MaxLanes()> v) {
  return VFromD<D>{vec_packsu(v.raw, v.raw)};
}

template <class D, typename FromT, HWY_IF_SIGNED_D(D), HWY_IF_SIGNED(FromT),
          HWY_IF_T_SIZE_ONE_OF_D(D, (1 << 1) | (1 << 2) | (1 << 4)),
          HWY_IF_T_SIZE(FromT, sizeof(TFromD<D>) * 2)>
HWY_API VFromD<D> DemoteTo(D /* tag */,
                           Vec128<FromT, Rebind<FromT, D>().MaxLanes()> v) {
  return VFromD<D>{vec_packs(v.raw, v.raw)};
}

template <class D, typename FromT, HWY_IF_UNSIGNED_D(D), HWY_IF_UNSIGNED(FromT),
          HWY_IF_T_SIZE_ONE_OF_D(D, (1 << 1) | (1 << 2) | (1 << 4)),
          HWY_IF_T_SIZE(FromT, sizeof(TFromD<D>) * 2)>
HWY_API VFromD<D> DemoteTo(D /* tag */,
                           Vec128<FromT, Rebind<FromT, D>().MaxLanes()> v) {
  return VFromD<D>{vec_packs(v.raw, v.raw)};
}

template <class D, class FromT, HWY_IF_SIGNED_D(D), HWY_IF_SIGNED(FromT),
          HWY_IF_T_SIZE_ONE_OF_D(D, (1 << 1) | (1 << 2)),
          hwy::EnableIf<(sizeof(FromT) >= sizeof(TFromD<D>) * 4)>* = nullptr>
HWY_API VFromD<D> DemoteTo(D d,
                           Vec128<FromT, Rebind<FromT, D>().MaxLanes()> v) {
  const Rebind<MakeNarrow<FromT>, D> d2;
  return DemoteTo(d, DemoteTo(d2, v));
}

template <class D, class FromT, HWY_IF_UNSIGNED_D(D), HWY_IF_UNSIGNED(FromT),
          HWY_IF_T_SIZE_ONE_OF_D(D, (1 << 1) | (1 << 2)),
          hwy::EnableIf<(sizeof(FromT) >= sizeof(TFromD<D>) * 4)>* = nullptr>
HWY_API VFromD<D> DemoteTo(D d,
                           Vec128<FromT, Rebind<FromT, D>().MaxLanes()> v) {
  const Rebind<MakeNarrow<FromT>, D> d2;
  return DemoteTo(d, DemoteTo(d2, v));
}

template <class D, class FromT, HWY_IF_UNSIGNED_D(D), HWY_IF_SIGNED(FromT),
          HWY_IF_T_SIZE_ONE_OF_D(D, (1 << 1) | (1 << 2)),
          hwy::EnableIf<(sizeof(FromT) >= sizeof(TFromD<D>) * 4)>* = nullptr>
HWY_API VFromD<D> DemoteTo(D d,
                           Vec128<FromT, Rebind<FromT, D>().MaxLanes()> v) {
  const Rebind<MakeUnsigned<MakeNarrow<FromT>>, D> d2;
  return DemoteTo(d, DemoteTo(d2, v));
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 8), HWY_IF_F16_D(D)>
HWY_API VFromD<D> DemoteTo(D df16, VFromD<Rebind<float, D>> v) {
#if HWY_PPC_HAVE_9 && HWY_COMPILER_GCC_ACTUAL
  // Do not use vec_pack_to_short_fp32 on clang as there is a bug in the clang
  // version of vec_pack_to_short_fp32
  (void)df16;
  return VFromD<D>{vec_pack_to_short_fp32(v.raw, v.raw)};
#else
  const Rebind<uint32_t, decltype(df16)> du;
  const RebindToUnsigned<decltype(df16)> du16;
#if HWY_PPC_HAVE_9 && HWY_HAS_BUILTIN(__builtin_vsx_xvcvsphp)
  // Work around bug in the clang implementation of vec_pack_to_short_fp32
  // by using the __builtin_vsx_xvcvsphp builtin on PPC9/PPC10 targets
  // if the __builtin_vsx_xvcvsphp intrinsic is available
  const VFromD<decltype(du)> bits16{
      reinterpret_cast<__vector unsigned int>(__builtin_vsx_xvcvsphp(v.raw))};
#else
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
  const auto bits16 = IfThenZeroElse(RebindMask(du, is_tiny), normal16);
#endif  // HWY_PPC_HAVE_9 && HWY_HAS_BUILTIN(__builtin_vsx_xvcvsphp)
  return BitCast(df16, TruncateTo(du16, bits16));
#endif  // HWY_PPC_HAVE_9 && HWY_COMPILER_GCC_ACTUAL
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 8), HWY_IF_BF16_D(D)>
HWY_API VFromD<D> DemoteTo(D dbf16, VFromD<Rebind<float, D>> v) {
  const Rebind<uint32_t, decltype(dbf16)> du32;  // for logical shift right
  const Rebind<uint16_t, decltype(dbf16)> du16;
  const auto bits_in_32 = ShiftRight<16>(BitCast(du32, v));
  return BitCast(dbf16, TruncateTo(du16, bits_in_32));
}

template <class D, HWY_IF_BF16_D(D), class V32 = VFromD<Repartition<float, D>>>
HWY_API VFromD<D> ReorderDemote2To(D dbf16, V32 a, V32 b) {
  const RebindToUnsigned<decltype(dbf16)> du16;
  const Repartition<uint32_t, decltype(dbf16)> du32;
#if HWY_IS_LITTLE_ENDIAN
  const auto a_in_odd = a;
  const auto b_in_even = ShiftRight<16>(BitCast(du32, b));
#else
  const auto a_in_odd = ShiftRight<16>(BitCast(du32, a));
  const auto b_in_even = b;
#endif
  return BitCast(dbf16,
                 OddEven(BitCast(du16, a_in_odd), BitCast(du16, b_in_even)));
}

// Specializations for partial vectors because vec_packs sets lanes above 2*N.
template <class DN, typename V, HWY_IF_V_SIZE_LE_D(DN, 4), HWY_IF_SIGNED_D(DN),
          HWY_IF_SIGNED_V(V),
          HWY_IF_T_SIZE_ONE_OF_D(DN, (1 << 1) | (1 << 2) | (1 << 4)),
          HWY_IF_T_SIZE_V(V, sizeof(TFromD<DN>) * 2)>
HWY_API VFromD<DN> ReorderDemote2To(DN dn, V a, V b) {
  const DFromV<decltype(a)> d;
  const Twice<decltype(d)> dt;
  return DemoteTo(dn, Combine(dt, b, a));
}
template <class DN, typename V, HWY_IF_V_SIZE_D(DN, 8), HWY_IF_SIGNED_D(DN),
          HWY_IF_SIGNED_V(V),
          HWY_IF_T_SIZE_ONE_OF_D(DN, (1 << 1) | (1 << 2) | (1 << 4)),
          HWY_IF_T_SIZE_V(V, sizeof(TFromD<DN>) * 2)>
HWY_API VFromD<DN> ReorderDemote2To(DN dn, V a, V b) {
  const Twice<decltype(dn)> dn_full;
  const Repartition<uint32_t, decltype(dn_full)> du32_full;

  const VFromD<decltype(dn_full)> v_full{vec_packs(a.raw, b.raw)};
  const auto vu32_full = BitCast(du32_full, v_full);
  return LowerHalf(
      BitCast(dn_full, ConcatEven(du32_full, vu32_full, vu32_full)));
}
template <class DN, typename V, HWY_IF_V_SIZE_D(DN, 16), HWY_IF_SIGNED_D(DN),
          HWY_IF_SIGNED_V(V),
          HWY_IF_T_SIZE_ONE_OF_D(DN, (1 << 1) | (1 << 2) | (1 << 4)),
          HWY_IF_T_SIZE_V(V, sizeof(TFromD<DN>) * 2)>
HWY_API VFromD<DN> ReorderDemote2To(DN /*dn*/, V a, V b) {
  return VFromD<DN>{vec_packs(a.raw, b.raw)};
}

template <class DN, typename V, HWY_IF_V_SIZE_LE_D(DN, 4),
          HWY_IF_UNSIGNED_D(DN), HWY_IF_SIGNED_V(V),
          HWY_IF_T_SIZE_ONE_OF_D(DN, (1 << 1) | (1 << 2) | (1 << 4)),
          HWY_IF_T_SIZE_V(V, sizeof(TFromD<DN>) * 2)>
HWY_API VFromD<DN> ReorderDemote2To(DN dn, V a, V b) {
  const DFromV<decltype(a)> d;
  const Twice<decltype(d)> dt;
  return DemoteTo(dn, Combine(dt, b, a));
}
template <class DN, typename V, HWY_IF_V_SIZE_D(DN, 8), HWY_IF_UNSIGNED_D(DN),
          HWY_IF_SIGNED_V(V),
          HWY_IF_T_SIZE_ONE_OF_D(DN, (1 << 1) | (1 << 2) | (1 << 4)),
          HWY_IF_T_SIZE_V(V, sizeof(TFromD<DN>) * 2)>
HWY_API VFromD<DN> ReorderDemote2To(DN dn, V a, V b) {
  const Twice<decltype(dn)> dn_full;
  const Repartition<uint32_t, decltype(dn_full)> du32_full;

  const VFromD<decltype(dn_full)> v_full{vec_packsu(a.raw, b.raw)};
  const auto vu32_full = BitCast(du32_full, v_full);
  return LowerHalf(
      BitCast(dn_full, ConcatEven(du32_full, vu32_full, vu32_full)));
}
template <class DN, typename V, HWY_IF_V_SIZE_D(DN, 16), HWY_IF_UNSIGNED_D(DN),
          HWY_IF_SIGNED_V(V),
          HWY_IF_T_SIZE_ONE_OF_D(DN, (1 << 1) | (1 << 2) | (1 << 4)),
          HWY_IF_T_SIZE_V(V, sizeof(TFromD<DN>) * 2)>
HWY_API VFromD<DN> ReorderDemote2To(DN /*dn*/, V a, V b) {
  return VFromD<DN>{vec_packsu(a.raw, b.raw)};
}

template <class DN, typename V, HWY_IF_V_SIZE_LE_D(DN, 4),
          HWY_IF_UNSIGNED_D(DN), HWY_IF_UNSIGNED_V(V),
          HWY_IF_T_SIZE_ONE_OF_D(DN, (1 << 1) | (1 << 2) | (1 << 4)),
          HWY_IF_T_SIZE_V(V, sizeof(TFromD<DN>) * 2)>
HWY_API VFromD<DN> ReorderDemote2To(DN dn, V a, V b) {
  const DFromV<decltype(a)> d;
  const Twice<decltype(d)> dt;
  return DemoteTo(dn, Combine(dt, b, a));
}
template <class DN, typename V, HWY_IF_V_SIZE_D(DN, 8), HWY_IF_UNSIGNED_D(DN),
          HWY_IF_UNSIGNED_V(V),
          HWY_IF_T_SIZE_ONE_OF_D(DN, (1 << 1) | (1 << 2) | (1 << 4)),
          HWY_IF_T_SIZE_V(V, sizeof(TFromD<DN>) * 2)>
HWY_API VFromD<DN> ReorderDemote2To(DN dn, V a, V b) {
  const Twice<decltype(dn)> dn_full;
  const Repartition<uint32_t, decltype(dn_full)> du32_full;

  const VFromD<decltype(dn_full)> v_full{vec_packs(a.raw, b.raw)};
  const auto vu32_full = BitCast(du32_full, v_full);
  return LowerHalf(
      BitCast(dn_full, ConcatEven(du32_full, vu32_full, vu32_full)));
}
template <class DN, typename V, HWY_IF_V_SIZE_D(DN, 16), HWY_IF_UNSIGNED_D(DN),
          HWY_IF_UNSIGNED_V(V),
          HWY_IF_T_SIZE_ONE_OF_D(DN, (1 << 1) | (1 << 2) | (1 << 4)),
          HWY_IF_T_SIZE_V(V, sizeof(TFromD<DN>) * 2)>
HWY_API VFromD<DN> ReorderDemote2To(DN /*dn*/, V a, V b) {
  return VFromD<DN>{vec_packs(a.raw, b.raw)};
}

template <class D, HWY_IF_NOT_FLOAT_NOR_SPECIAL(TFromD<D>), class V,
          HWY_IF_NOT_FLOAT_NOR_SPECIAL_V(V),
          HWY_IF_T_SIZE_V(V, sizeof(TFromD<D>) * 2),
          HWY_IF_LANES_D(D, HWY_MAX_LANES_D(DFromV<V>) * 2)>
HWY_API VFromD<D> OrderedDemote2To(D d, V a, V b) {
  return ReorderDemote2To(d, a, b);
}

template <class D, HWY_IF_BF16_D(D), class V32 = VFromD<Repartition<float, D>>>
HWY_API VFromD<D> OrderedDemote2To(D dbf16, V32 a, V32 b) {
  const RebindToUnsigned<decltype(dbf16)> du16;
#if HWY_IS_LITTLE_ENDIAN
  return BitCast(dbf16, ConcatOdd(du16, BitCast(du16, b), BitCast(du16, a)));
#else
  return BitCast(dbf16, ConcatEven(du16, BitCast(du16, b), BitCast(du16, a)));
#endif
}

template <class D, HWY_IF_V_SIZE_D(D, 4), HWY_IF_F32_D(D)>
HWY_API Vec32<float> DemoteTo(D /* tag */, Vec64<double> v) {
  return Vec32<float>{vec_floate(v.raw)};
}

template <class D, HWY_IF_V_SIZE_D(D, 8), HWY_IF_F32_D(D)>
HWY_API Vec64<float> DemoteTo(D d, Vec128<double> v) {
#if HWY_IS_LITTLE_ENDIAN
  const Vec128<float> f64_to_f32{vec_floate(v.raw)};
#else
  const Vec128<float> f64_to_f32{vec_floato(v.raw)};
#endif

  const RebindToUnsigned<D> du;
  const Rebind<uint64_t, D> du64;
  return Vec64<float>{
      BitCast(d, TruncateTo(du, BitCast(du64, f64_to_f32))).raw};
}

template <class D, HWY_IF_V_SIZE_D(D, 4), HWY_IF_I32_D(D)>
HWY_API Vec32<int32_t> DemoteTo(D /* tag */, Vec64<double> v) {
  return Vec32<int32_t>{vec_signede(v.raw)};
}

template <class D, HWY_IF_V_SIZE_D(D, 8), HWY_IF_I32_D(D)>
HWY_API Vec64<int32_t> DemoteTo(D /* tag */, Vec128<double> v) {
#if HWY_IS_LITTLE_ENDIAN
  const Vec128<int32_t> f64_to_i32{vec_signede(v.raw)};
#else
  const Vec128<int32_t> f64_to_i32{vec_signedo(v.raw)};
#endif

  const Rebind<int64_t, D> di64;
  const Vec128<int64_t> vi64 = BitCast(di64, f64_to_i32);
  return Vec64<int32_t>{vec_pack(vi64.raw, vi64.raw)};
}

// For already range-limited input [0, 255].
template <size_t N>
HWY_API Vec128<uint8_t, N> U8FromU32(Vec128<uint32_t, N> v) {
  const Rebind<uint16_t, DFromV<decltype(v)>> du16;
  const Rebind<uint8_t, decltype(du16)> du8;
  return TruncateTo(du8, TruncateTo(du16, v));
}
// ------------------------------ Integer <=> fp (ShiftRight, OddEven)

// Note: altivec.h vec_ct* currently contain C casts which triggers
// -Wdeprecate-lax-vec-conv-all warnings, so disable them.

template <class D, typename FromT, HWY_IF_F32_D(D), HWY_IF_NOT_FLOAT(FromT),
          HWY_IF_T_SIZE_D(D, sizeof(FromT))>
HWY_API VFromD<D> ConvertTo(D /* tag */,
                            Vec128<FromT, Rebind<FromT, D>().MaxLanes()> v) {
  HWY_DIAGNOSTICS(push)
#if HWY_COMPILER_CLANG
  HWY_DIAGNOSTICS_OFF(disable : 5219, ignored "-Wdeprecate-lax-vec-conv-all")
#endif
  return VFromD<D>{vec_ctf(v.raw, 0)};
  HWY_DIAGNOSTICS(pop)
}

template <class D, typename FromT, HWY_IF_F64_D(D), HWY_IF_NOT_FLOAT(FromT),
          HWY_IF_T_SIZE_D(D, sizeof(FromT))>
HWY_API VFromD<D> ConvertTo(D /* tag */,
                            Vec128<FromT, Rebind<FromT, D>().MaxLanes()> v) {
  return VFromD<D>{vec_double(v.raw)};
}

// Truncates (rounds toward zero).
template <class D, typename FromT, HWY_IF_SIGNED_D(D), HWY_IF_FLOAT(FromT),
          HWY_IF_T_SIZE_D(D, sizeof(FromT))>
HWY_API VFromD<D> ConvertTo(D /* tag */,
                            Vec128<FromT, Rebind<FromT, D>().MaxLanes()> v) {
  HWY_DIAGNOSTICS(push)
#if HWY_COMPILER_CLANG
  HWY_DIAGNOSTICS_OFF(disable : 5219, ignored "-Wdeprecate-lax-vec-conv-all")
#endif
  return VFromD<D>{vec_cts(v.raw, 0)};
  HWY_DIAGNOSTICS(pop)
}

template <class D, typename FromT, HWY_IF_UNSIGNED_D(D), HWY_IF_FLOAT(FromT),
          HWY_IF_T_SIZE_D(D, sizeof(FromT))>
HWY_API VFromD<D> ConvertTo(D /* tag */,
                            Vec128<FromT, Rebind<FromT, D>().MaxLanes()> v) {
  HWY_DIAGNOSTICS(push)
#if HWY_COMPILER_CLANG
  HWY_DIAGNOSTICS_OFF(disable : 5219, ignored "-Wdeprecate-lax-vec-conv-all")
#endif
  return VFromD<D>{vec_ctu(v.raw, 0)};
  HWY_DIAGNOSTICS(pop)
}

template <size_t N>
HWY_API Vec128<int32_t, N> NearestInt(Vec128<float, N> v) {
  HWY_DIAGNOSTICS(push)
#if HWY_COMPILER_CLANG
  HWY_DIAGNOSTICS_OFF(disable : 5219, ignored "-Wdeprecate-lax-vec-conv-all")
#endif
  return Vec128<int32_t, N>{vec_cts(vec_round(v.raw), 0)};
  HWY_DIAGNOSTICS(pop)
}

// ------------------------------ Floating-point rounding (ConvertTo)

// Toward nearest integer, ties to even
template <size_t N>
HWY_API Vec128<float, N> Round(Vec128<float, N> v) {
  return Vec128<float, N>{vec_round(v.raw)};
}

template <size_t N>
HWY_API Vec128<double, N> Round(Vec128<double, N> v) {
  return Vec128<double, N>{vec_rint(v.raw)};
}

// Toward zero, aka truncate
template <typename T, size_t N, HWY_IF_FLOAT(T)>
HWY_API Vec128<T, N> Trunc(Vec128<T, N> v) {
  return Vec128<T, N>{vec_trunc(v.raw)};
}

// Toward +infinity, aka ceiling
template <typename T, size_t N, HWY_IF_FLOAT(T)>
HWY_API Vec128<T, N> Ceil(Vec128<T, N> v) {
  return Vec128<T, N>{vec_ceil(v.raw)};
}

// Toward -infinity, aka floor
template <typename T, size_t N, HWY_IF_FLOAT(T)>
HWY_API Vec128<T, N> Floor(Vec128<T, N> v) {
  return Vec128<T, N>{vec_floor(v.raw)};
}

// ------------------------------ Floating-point classification

template <typename T, size_t N>
HWY_API Mask128<T, N> IsNaN(Vec128<T, N> v) {
  static_assert(IsFloat<T>(), "Only for float");
  return v != v;
}

template <typename T, size_t N>
HWY_API Mask128<T, N> IsInf(Vec128<T, N> v) {
  static_assert(IsFloat<T>(), "Only for float");
  using TU = MakeUnsigned<T>;
  const DFromV<decltype(v)> d;
  const RebindToUnsigned<decltype(d)> du;
  const VFromD<decltype(du)> vu = BitCast(du, v);
  // 'Shift left' to clear the sign bit, check for exponent=max and mantissa=0.
  return RebindMask(
      d,
      Eq(Add(vu, vu), Set(du, static_cast<TU>(hwy::MaxExponentTimes2<T>()))));
}

// Returns whether normal/subnormal/zero.
template <typename T, size_t N>
HWY_API Mask128<T, N> IsFinite(Vec128<T, N> v) {
  static_assert(IsFloat<T>(), "Only for float");
  using TU = MakeUnsigned<T>;
  const DFromV<decltype(v)> d;
  const RebindToUnsigned<decltype(d)> du;
  const VFromD<decltype(du)> vu = BitCast(du, v);
  // 'Shift left' to clear the sign bit, check for exponent<max.
  return RebindMask(
      d,
      Lt(Add(vu, vu), Set(du, static_cast<TU>(hwy::MaxExponentTimes2<T>()))));
}

// ================================================== CRYPTO

#if !defined(HWY_DISABLE_PPC8_CRYPTO)

// Per-target flag to prevent generic_ops-inl.h from defining AESRound.
#ifdef HWY_NATIVE_AES
#undef HWY_NATIVE_AES
#else
#define HWY_NATIVE_AES
#endif

namespace detail {
#if HWY_COMPILER_CLANG && HWY_COMPILER_CLANG < 1600
using CipherTag = Full128<uint64_t>;
#else
using CipherTag = Full128<uint8_t>;
#endif  // !HWY_COMPILER_CLANG
using CipherVec = VFromD<CipherTag>;
}  // namespace detail

HWY_API Vec128<uint8_t> AESRound(Vec128<uint8_t> state,
                                 Vec128<uint8_t> round_key) {
  const detail::CipherTag dc;
  const Full128<uint8_t> du8;
#if HWY_IS_LITTLE_ENDIAN
  return Reverse(du8,
                 BitCast(du8, detail::CipherVec{vec_cipher_be(
                                  BitCast(dc, Reverse(du8, state)).raw,
                                  BitCast(dc, Reverse(du8, round_key)).raw)}));
#else
  return BitCast(du8, detail::CipherVec{vec_cipher_be(
                          BitCast(dc, state).raw, BitCast(dc, round_key).raw)});
#endif
}

HWY_API Vec128<uint8_t> AESLastRound(Vec128<uint8_t> state,
                                     Vec128<uint8_t> round_key) {
  const detail::CipherTag dc;
  const Full128<uint8_t> du8;
#if HWY_IS_LITTLE_ENDIAN
  return Reverse(du8,
                 BitCast(du8, detail::CipherVec{vec_cipherlast_be(
                                  BitCast(dc, Reverse(du8, state)).raw,
                                  BitCast(dc, Reverse(du8, round_key)).raw)}));
#else
  return BitCast(du8, detail::CipherVec{vec_cipherlast_be(
                          BitCast(dc, state).raw, BitCast(dc, round_key).raw)});
#endif
}

HWY_API Vec128<uint8_t> AESRoundInv(Vec128<uint8_t> state,
                                    Vec128<uint8_t> round_key) {
  const detail::CipherTag dc;
  const Full128<uint8_t> du8;
#if HWY_IS_LITTLE_ENDIAN
  return Xor(Reverse(du8, BitCast(du8, detail::CipherVec{vec_ncipher_be(
                                           BitCast(dc, Reverse(du8, state)).raw,
                                           Zero(dc).raw)})),
             round_key);
#else
  return Xor(BitCast(du8, detail::CipherVec{vec_ncipher_be(
                              BitCast(dc, state).raw, Zero(dc).raw)}),
             round_key);
#endif
}

HWY_API Vec128<uint8_t> AESLastRoundInv(Vec128<uint8_t> state,
                                        Vec128<uint8_t> round_key) {
  const detail::CipherTag dc;
  const Full128<uint8_t> du8;
#if HWY_IS_LITTLE_ENDIAN
  return Reverse(du8,
                 BitCast(du8, detail::CipherVec{vec_ncipherlast_be(
                                  BitCast(dc, Reverse(du8, state)).raw,
                                  BitCast(dc, Reverse(du8, round_key)).raw)}));
#else
  return BitCast(du8, detail::CipherVec{vec_ncipherlast_be(
                          BitCast(dc, state).raw, BitCast(dc, round_key).raw)});
#endif
}

HWY_API Vec128<uint8_t> AESInvMixColumns(Vec128<uint8_t> state) {
  const Full128<uint8_t> du8;
  const auto zero = Zero(du8);

  // PPC8/PPC9/PPC10 does not have a single instruction for the AES
  // InvMixColumns operation like ARM Crypto, SVE2 Crypto, or AES-NI do.

  // The AESInvMixColumns operation can be carried out on PPC8/PPC9/PPC10
  // by doing an AESLastRound operation with a zero round_key followed by an
  // AESRoundInv operation with a zero round_key.
  return AESRoundInv(AESLastRound(state, zero), zero);
}

template <uint8_t kRcon>
HWY_API Vec128<uint8_t> AESKeyGenAssist(Vec128<uint8_t> v) {
  constexpr __vector unsigned char kRconXorMask = {0, 0, 0, 0, kRcon, 0, 0, 0,
                                                   0, 0, 0, 0, kRcon, 0, 0, 0};
  constexpr __vector unsigned char kRotWordShuffle = {
      4, 5, 6, 7, 5, 6, 7, 4, 12, 13, 14, 15, 13, 14, 15, 12};
  const detail::CipherTag dc;
  const Full128<uint8_t> du8;
  const auto sub_word_result =
      BitCast(du8, detail::CipherVec{vec_sbox_be(BitCast(dc, v).raw)});
  const auto rot_word_result =
      TableLookupBytes(sub_word_result, Vec128<uint8_t>{kRotWordShuffle});
  return Xor(rot_word_result, Vec128<uint8_t>{kRconXorMask});
}

template <size_t N>
HWY_API Vec128<uint64_t, N> CLMulLower(Vec128<uint64_t, N> a,
                                       Vec128<uint64_t, N> b) {
  // NOTE: Lane 1 of both a and b need to be zeroed out for the
  // vec_pmsum_be operation below as the vec_pmsum_be operation
  // does a carryless multiplication of each 64-bit half and then
  // adds the two halves using an bitwise XOR operation.

  const DFromV<decltype(a)> d;
  const auto zero = Zero(d);

  using VU64 = __vector unsigned long long;
  const VU64 pmsum_result = reinterpret_cast<VU64>(
      vec_pmsum_be(InterleaveLower(a, zero).raw, InterleaveLower(b, zero).raw));

#if HWY_IS_LITTLE_ENDIAN
  return Vec128<uint64_t, N>{pmsum_result};
#else
  // Need to swap the two halves of pmsum_result on big-endian targets as
  // the upper 64 bits of the carryless multiplication result are in lane 0 of
  // pmsum_result and the lower 64 bits of the carryless multiplication result
  // are in lane 1 of mul128_result
  return Vec128<uint64_t, N>{vec_sld(pmsum_result, pmsum_result, 8)};
#endif
}

template <size_t N>
HWY_API Vec128<uint64_t, N> CLMulUpper(Vec128<uint64_t, N> a,
                                       Vec128<uint64_t, N> b) {
  // NOTE: Lane 0 of both a and b need to be zeroed out for the
  // vec_pmsum_be operation below as the vec_pmsum_be operation
  // does a carryless multiplication of each 64-bit half and then
  // adds the two halves using an bitwise XOR operation.

  const DFromV<decltype(a)> d;
  const auto zero = Zero(d);

  using VU64 = __vector unsigned long long;
  const VU64 pmsum_result = reinterpret_cast<VU64>(
      vec_pmsum_be(vec_mergel(zero.raw, a.raw), vec_mergel(zero.raw, b.raw)));

#if HWY_IS_LITTLE_ENDIAN
  return Vec128<uint64_t, N>{pmsum_result};
#else
  // Need to swap the two halves of pmsum_result on big-endian targets as
  // the upper 64 bits of the carryless multiplication result are in lane 0 of
  // pmsum_result and the lower 64 bits of the carryless multiplication result
  // are in lane 1 of mul128_result
  return Vec128<uint64_t, N>{vec_sld(pmsum_result, pmsum_result, 8)};
#endif
}

#endif  // !defined(HWY_DISABLE_PPC8_CRYPTO)

// ================================================== MISC

// ------------------------------ LoadMaskBits (TestBit)

namespace detail {

template <class D, HWY_IF_T_SIZE_D(D, 1)>
HWY_INLINE MFromD<D> LoadMaskBits128(D /*d*/, uint64_t mask_bits) {
#if HWY_PPC_HAVE_10
  const Vec128<uint8_t> mask_vec{vec_genbm(mask_bits)};

#if HWY_IS_LITTLE_ENDIAN
  return MFromD<D>{MaskFromVec(mask_vec).raw};
#else
  return MFromD<D>{MaskFromVec(Reverse(Full128<uint8_t>(), mask_vec)).raw};
#endif  // HWY_IS_LITTLE_ENDIAN

#else  // PPC9 or earlier
  const Full128<uint8_t> du8;
  const Full128<uint16_t> du16;
  const Vec128<uint8_t> vbits =
      BitCast(du8, Set(du16, static_cast<uint16_t>(mask_bits)));

  // Replicate bytes 8x such that each byte contains the bit that governs it.
#if HWY_IS_LITTLE_ENDIAN
  const __vector unsigned char kRep8 = {0, 0, 0, 0, 0, 0, 0, 0,
                                        1, 1, 1, 1, 1, 1, 1, 1};
#else
  const __vector unsigned char kRep8 = {1, 1, 1, 1, 1, 1, 1, 1,
                                        0, 0, 0, 0, 0, 0, 0, 0};
#endif  // HWY_IS_LITTLE_ENDIAN

  const Vec128<uint8_t> rep8{vec_perm(vbits.raw, vbits.raw, kRep8)};
  const __vector unsigned char kBit = {1, 2, 4, 8, 16, 32, 64, 128,
                                       1, 2, 4, 8, 16, 32, 64, 128};
  return MFromD<D>{TestBit(rep8, Vec128<uint8_t>{kBit}).raw};
#endif  // HWY_PPC_HAVE_10
}

template <class D, HWY_IF_T_SIZE_D(D, 2)>
HWY_INLINE MFromD<D> LoadMaskBits128(D /*d*/, uint64_t mask_bits) {
#if HWY_PPC_HAVE_10
  const Vec128<uint16_t> mask_vec{vec_genhm(mask_bits)};

#if HWY_IS_LITTLE_ENDIAN
  return MFromD<D>{MaskFromVec(mask_vec).raw};
#else
  return MFromD<D>{MaskFromVec(Reverse(Full128<uint16_t>(), mask_vec)).raw};
#endif  // HWY_IS_LITTLE_ENDIAN

#else   // PPC9 or earlier
  const __vector unsigned short kBit = {1, 2, 4, 8, 16, 32, 64, 128};
  const auto vmask_bits =
      Set(Full128<uint16_t>(), static_cast<uint16_t>(mask_bits));
  return MFromD<D>{TestBit(vmask_bits, Vec128<uint16_t>{kBit}).raw};
#endif  // HWY_PPC_HAVE_10
}

template <class D, HWY_IF_T_SIZE_D(D, 4)>
HWY_INLINE MFromD<D> LoadMaskBits128(D /*d*/, uint64_t mask_bits) {
#if HWY_PPC_HAVE_10
  const Vec128<uint32_t> mask_vec{vec_genwm(mask_bits)};

#if HWY_IS_LITTLE_ENDIAN
  return MFromD<D>{MaskFromVec(mask_vec).raw};
#else
  return MFromD<D>{MaskFromVec(Reverse(Full128<uint32_t>(), mask_vec)).raw};
#endif  // HWY_IS_LITTLE_ENDIAN

#else   // PPC9 or earlier
  const __vector unsigned int kBit = {1, 2, 4, 8};
  const auto vmask_bits =
      Set(Full128<uint32_t>(), static_cast<uint32_t>(mask_bits));
  return MFromD<D>{TestBit(vmask_bits, Vec128<uint32_t>{kBit}).raw};
#endif  // HWY_PPC_HAVE_10
}

template <class D, HWY_IF_T_SIZE_D(D, 8)>
HWY_INLINE MFromD<D> LoadMaskBits128(D /*d*/, uint64_t mask_bits) {
#if HWY_PPC_HAVE_10
  const Vec128<uint64_t> mask_vec{vec_gendm(mask_bits)};

#if HWY_IS_LITTLE_ENDIAN
  return MFromD<D>{MaskFromVec(mask_vec).raw};
#else
  return MFromD<D>{MaskFromVec(Reverse(Full128<uint64_t>(), mask_vec)).raw};
#endif  // HWY_IS_LITTLE_ENDIAN

#else   // PPC9 or earlier
  const __vector unsigned long long kBit = {1, 2};
  const auto vmask_bits =
      Set(Full128<uint64_t>(), static_cast<uint64_t>(mask_bits));
  return MFromD<D>{TestBit(vmask_bits, Vec128<uint64_t>{kBit}).raw};
#endif  // HWY_PPC_HAVE_10
}

}  // namespace detail

// `p` points to at least 8 readable bytes, not all of which need be valid.
template <class D, HWY_IF_LANES_LE_D(D, 8)>
HWY_API MFromD<D> LoadMaskBits(D d, const uint8_t* HWY_RESTRICT bits) {
  // If there are 8 or fewer lanes, simply convert bits[0] to a uint64_t
  uint64_t mask_bits = bits[0];

  constexpr size_t kN = MaxLanes(d);
  if (kN < 8) mask_bits &= (1u << kN) - 1;

  return detail::LoadMaskBits128(d, mask_bits);
}

template <class D, HWY_IF_LANES_D(D, 16)>
HWY_API MFromD<D> LoadMaskBits(D d, const uint8_t* HWY_RESTRICT bits) {
  // First, copy the mask bits to a uint16_t as there as there are at most
  // 16 lanes in a vector.

  // Copying the mask bits to a uint16_t first will also ensure that the
  // mask bits are loaded into the lower 16 bits on big-endian PPC targets.
  uint16_t u16_mask_bits;
  CopyBytes<sizeof(uint16_t)>(bits, &u16_mask_bits);

#if HWY_IS_LITTLE_ENDIAN
  return detail::LoadMaskBits128(d, u16_mask_bits);
#else
  // On big-endian targets, u16_mask_bits need to be byte swapped as bits
  // contains the mask bits in little-endian byte order

  // GCC/Clang will optimize the load of u16_mask_bits and byte swap to a
  // single lhbrx instruction on big-endian PPC targets when optimizations
  // are enabled.
#if HWY_HAS_BUILTIN(__builtin_bswap16)
  return detail::LoadMaskBits128(d, __builtin_bswap16(u16_mask_bits));
#else
  return detail::LoadMaskBits128(
      d, static_cast<uint16_t>((u16_mask_bits << 8) | (u16_mask_bits >> 8)));
#endif
#endif
}

template <typename T>
struct CompressIsPartition {
  // generic_ops-inl does not guarantee IsPartition for 8-bit.
  enum { value = (sizeof(T) != 1) };
};

// ------------------------------ StoreMaskBits

namespace detail {

#if !HWY_PPC_HAVE_10 || HWY_IS_BIG_ENDIAN
// fallback for missing vec_extractm
template <size_t N>
HWY_INLINE uint64_t ExtractSignBits(Vec128<uint8_t, N> sign_bits,
                                    __vector unsigned char bit_shuffle) {
  // clang POWER8 and 9 targets appear to differ in their return type of
  // vec_vbpermq: unsigned or signed, so cast to avoid a warning.
  using VU64 = detail::Raw128<uint64_t>::type;
  const Vec128<uint64_t> extracted{
      reinterpret_cast<VU64>(vec_vbpermq(sign_bits.raw, bit_shuffle))};
  return extracted.raw[HWY_IS_LITTLE_ENDIAN];
}

#endif  // !HWY_PPC_HAVE_10

template <typename T, size_t N>
HWY_INLINE uint64_t BitsFromMask(hwy::SizeTag<1> /*tag*/, Mask128<T, N> mask) {
  const DFromM<decltype(mask)> d;
  const Repartition<uint8_t, decltype(d)> du8;
  const VFromD<decltype(du8)> sign_bits = BitCast(du8, VecFromMask(d, mask));
#if HWY_PPC_HAVE_10 && HWY_IS_LITTLE_ENDIAN
  return static_cast<uint64_t>(vec_extractm(sign_bits.raw));
#else
  const __vector unsigned char kBitShuffle = {120, 112, 104, 96, 88, 80, 72, 64,
                                              56,  48,  40,  32, 24, 16, 8,  0};
  return ExtractSignBits(sign_bits, kBitShuffle);
#endif  // HWY_PPC_HAVE_10
}

template <typename T, size_t N>
HWY_INLINE uint64_t BitsFromMask(hwy::SizeTag<2> /*tag*/, Mask128<T, N> mask) {
  const DFromM<decltype(mask)> d;
  const Repartition<uint8_t, decltype(d)> du8;
  const VFromD<decltype(du8)> sign_bits = BitCast(du8, VecFromMask(d, mask));

#if HWY_PPC_HAVE_10 && HWY_IS_LITTLE_ENDIAN
  const RebindToUnsigned<decltype(d)> du;
  return static_cast<uint64_t>(vec_extractm(BitCast(du, sign_bits).raw));
#else
#if HWY_IS_LITTLE_ENDIAN
  const __vector unsigned char kBitShuffle = {
      112, 96, 80, 64, 48, 32, 16, 0, 128, 128, 128, 128, 128, 128, 128, 128};
#else
  const __vector unsigned char kBitShuffle = {
      128, 128, 128, 128, 128, 128, 128, 128, 112, 96, 80, 64, 48, 32, 16, 0};
#endif
  return ExtractSignBits(sign_bits, kBitShuffle);
#endif  // HWY_PPC_HAVE_10
}

template <typename T, size_t N>
HWY_INLINE uint64_t BitsFromMask(hwy::SizeTag<4> /*tag*/, Mask128<T, N> mask) {
  const DFromM<decltype(mask)> d;
  const Repartition<uint8_t, decltype(d)> du8;
  const VFromD<decltype(du8)> sign_bits = BitCast(du8, VecFromMask(d, mask));
#if HWY_PPC_HAVE_10 && HWY_IS_LITTLE_ENDIAN
  const RebindToUnsigned<decltype(d)> du;
  return static_cast<uint64_t>(vec_extractm(BitCast(du, sign_bits).raw));
#else
#if HWY_IS_LITTLE_ENDIAN
  const __vector unsigned char kBitShuffle = {96,  64,  32,  0,   128, 128,
                                              128, 128, 128, 128, 128, 128,
                                              128, 128, 128, 128};
#else
  const __vector unsigned char kBitShuffle = {128, 128, 128, 128, 128, 128,
                                              128, 128, 128, 128, 128, 128,
                                              96,  64,  32,  0};
#endif
  return ExtractSignBits(sign_bits, kBitShuffle);
#endif  // HWY_PPC_HAVE_10
}

template <typename T, size_t N>
HWY_INLINE uint64_t BitsFromMask(hwy::SizeTag<8> /*tag*/, Mask128<T, N> mask) {
  const DFromM<decltype(mask)> d;
  const Repartition<uint8_t, decltype(d)> du8;
  const VFromD<decltype(du8)> sign_bits = BitCast(du8, VecFromMask(d, mask));
#if HWY_PPC_HAVE_10 && HWY_IS_LITTLE_ENDIAN
  const RebindToUnsigned<decltype(d)> du;
  return static_cast<uint64_t>(vec_extractm(BitCast(du, sign_bits).raw));
#else
#if HWY_IS_LITTLE_ENDIAN
  const __vector unsigned char kBitShuffle = {64,  0,   128, 128, 128, 128,
                                              128, 128, 128, 128, 128, 128,
                                              128, 128, 128, 128};
#else
  const __vector unsigned char kBitShuffle = {128, 128, 128, 128, 128, 128,
                                              128, 128, 128, 128, 128, 128,
                                              128, 128, 64,  0};
#endif
  return ExtractSignBits(sign_bits, kBitShuffle);
#endif  // HWY_PPC_HAVE_10
}

// Returns the lowest N of the mask bits.
template <typename T, size_t N>
constexpr uint64_t OnlyActive(uint64_t mask_bits) {
  return ((N * sizeof(T)) == 16) ? mask_bits : mask_bits & ((1ull << N) - 1);
}

template <typename T, size_t N>
HWY_INLINE uint64_t BitsFromMask(Mask128<T, N> mask) {
  return OnlyActive<T, N>(BitsFromMask(hwy::SizeTag<sizeof(T)>(), mask));
}

}  // namespace detail

// `p` points to at least 8 writable bytes.
template <class D, HWY_IF_LANES_LE_D(D, 8)>
HWY_API size_t StoreMaskBits(D /*d*/, MFromD<D> mask, uint8_t* bits) {
  // For vectors with 8 or fewer lanes, simply cast the result of BitsFromMask
  // to an uint8_t and store the result in bits[0].
  bits[0] = static_cast<uint8_t>(detail::BitsFromMask(mask));
  return sizeof(uint8_t);
}

template <class D, HWY_IF_LANES_D(D, 16)>
HWY_API size_t StoreMaskBits(D /*d*/, MFromD<D> mask, uint8_t* bits) {
  const auto mask_bits = detail::BitsFromMask(mask);

  // First convert mask_bits to a uint16_t as we only want to store
  // the lower 16 bits of mask_bits as there are 16 lanes in mask.

  // Converting mask_bits to a uint16_t first will also ensure that
  // the lower 16 bits of mask_bits are stored instead of the upper 16 bits
  // of mask_bits on big-endian PPC targets.
#if HWY_IS_LITTLE_ENDIAN
  const uint16_t u16_mask_bits = static_cast<uint16_t>(mask_bits);
#else
  // On big-endian targets, the bytes of mask_bits need to be swapped
  // as StoreMaskBits expects the mask bits to be stored in little-endian
  // byte order.

  // GCC will also optimize the byte swap and CopyBytes operations below
  // to a single sthbrx instruction when optimizations are enabled on
  // big-endian PPC targets
#if HWY_HAS_BUILTIN(__builtin_bswap16)
  const uint16_t u16_mask_bits =
      __builtin_bswap16(static_cast<uint16_t>(mask_bits));
#else
  const uint16_t u16_mask_bits = static_cast<uint16_t>(
      (mask_bits << 8) | (static_cast<uint16_t>(mask_bits) >> 8));
#endif
#endif

  CopyBytes<sizeof(uint16_t)>(&u16_mask_bits, bits);
  return sizeof(uint16_t);
}

// ------------------------------ Mask testing

template <class D, HWY_IF_V_SIZE_D(D, 16)>
HWY_API bool AllFalse(D d, MFromD<D> mask) {
  const RebindToUnsigned<decltype(d)> du;
  return static_cast<bool>(vec_all_eq(RebindMask(du, mask).raw, Zero(du).raw));
}

template <class D, HWY_IF_V_SIZE_D(D, 16)>
HWY_API bool AllTrue(D d, MFromD<D> mask) {
  const RebindToUnsigned<decltype(d)> du;
  using TU = TFromD<decltype(du)>;
  return static_cast<bool>(
      vec_all_eq(RebindMask(du, mask).raw, Set(du, hwy::LimitsMax<TU>()).raw));
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 8)>
HWY_API bool AllFalse(D d, MFromD<D> mask) {
  const Full128<TFromD<D>> d_full;
  constexpr size_t kN = MaxLanes(d);
  return AllFalse(d_full, MFromD<decltype(d_full)>{
                              vec_and(mask.raw, FirstN(d_full, kN).raw)});
}

template <class D, HWY_IF_V_SIZE_LE_D(D, 8)>
HWY_API bool AllTrue(D d, MFromD<D> mask) {
  const Full128<TFromD<D>> d_full;
  constexpr size_t kN = MaxLanes(d);
  return AllTrue(d_full, MFromD<decltype(d_full)>{
                             vec_or(mask.raw, Not(FirstN(d_full, kN)).raw)});
}

template <class D>
HWY_API size_t CountTrue(D /* tag */, MFromD<D> mask) {
  return PopCount(detail::BitsFromMask(mask));
}

template <class D>
HWY_API size_t FindKnownFirstTrue(D /* tag */, MFromD<D> mask) {
  return Num0BitsBelowLS1Bit_Nonzero64(detail::BitsFromMask(mask));
}

template <class D>
HWY_API intptr_t FindFirstTrue(D /* tag */, MFromD<D> mask) {
  const uint64_t mask_bits = detail::BitsFromMask(mask);
  return mask_bits ? intptr_t(Num0BitsBelowLS1Bit_Nonzero64(mask_bits)) : -1;
}

template <class D>
HWY_API size_t FindKnownLastTrue(D /* tag */, MFromD<D> mask) {
  return 63 - Num0BitsAboveMS1Bit_Nonzero64(detail::BitsFromMask(mask));
}

template <class D>
HWY_API intptr_t FindLastTrue(D /* tag */, MFromD<D> mask) {
  const uint64_t mask_bits = detail::BitsFromMask(mask);
  return mask_bits ? intptr_t(63 - Num0BitsAboveMS1Bit_Nonzero64(mask_bits))
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

  // To reduce cache footprint, store lane indices and convert to byte indices
  // (2*lane + 0..1), with the doubling baked into the table. It's not clear
  // that the additional cost of unpacking nibbles is worthwhile.
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
  constexpr uint16_t kPairIndexIncrement =
      HWY_IS_LITTLE_ENDIAN ? 0x0100 : 0x0001;

  return BitCast(d, pairs + Set(du, kPairIndexIncrement));
}

template <class D, HWY_IF_T_SIZE_D(D, 2)>
HWY_INLINE VFromD<D> IndicesFromNotBits128(D d, uint64_t mask_bits) {
  HWY_DASSERT(mask_bits < 256);
  const Rebind<uint8_t, decltype(d)> d8;
  const Twice<decltype(d8)> d8t;
  const RebindToUnsigned<decltype(d)> du;

  // To reduce cache footprint, store lane indices and convert to byte indices
  // (2*lane + 0..1), with the doubling baked into the table. It's not clear
  // that the additional cost of unpacking nibbles is worthwhile.
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
  constexpr uint16_t kPairIndexIncrement =
      HWY_IS_LITTLE_ENDIAN ? 0x0100 : 0x0001;

  return BitCast(d, pairs + Set(du, kPairIndexIncrement));
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
  const Full128<T> d;
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
  const Full128<T> d;
  const Vec128<T> m = VecFromMask(d, mask);
  const Vec128<T> maskL = DupEven(m);
  const Vec128<T> maskH = DupOdd(m);
  const Vec128<T> swap = AndNot(maskH, maskL);
  return IfVecThenElse(swap, Shuffle01(v), v);
}

// General case, 2 or 4 bytes
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
  // As there are at most 8 lanes in v if sizeof(TFromD<D>) > 1, simply
  // convert bits[0] to a uint64_t
  uint64_t mask_bits = bits[0];
  if (N < 8) {
    mask_bits &= (1ull << N) - 1;
  }

  return detail::CompressBits(v, mask_bits);
}

// ------------------------------ CompressStore, CompressBitsStore

template <class D, HWY_IF_NOT_T_SIZE_D(D, 1)>
HWY_API size_t CompressStore(VFromD<D> v, MFromD<D> m, D d,
                             TFromD<D>* HWY_RESTRICT unaligned) {
  const RebindToUnsigned<decltype(d)> du;

  const uint64_t mask_bits = detail::BitsFromMask(m);
  HWY_DASSERT(mask_bits < (1ull << MaxLanes(d)));
  const size_t count = PopCount(mask_bits);

  const auto indices = BitCast(du, detail::IndicesFromBits128(d, mask_bits));
  const auto compressed = BitCast(d, TableLookupBytes(BitCast(du, v), indices));
  StoreU(compressed, d, unaligned);
  return count;
}

template <class D, HWY_IF_NOT_T_SIZE_D(D, 1)>
HWY_API size_t CompressBlendedStore(VFromD<D> v, MFromD<D> m, D d,
                                    TFromD<D>* HWY_RESTRICT unaligned) {
  const RebindToUnsigned<decltype(d)> du;

  const uint64_t mask_bits = detail::BitsFromMask(m);
  HWY_DASSERT(mask_bits < (1ull << MaxLanes(d)));
  const size_t count = PopCount(mask_bits);

  const auto indices = BitCast(du, detail::IndicesFromBits128(d, mask_bits));
  const auto compressed = BitCast(d, TableLookupBytes(BitCast(du, v), indices));
  BlendedStore(compressed, FirstN(d, count), d, unaligned);
  return count;
}

template <class D, HWY_IF_NOT_T_SIZE_D(D, 1)>
HWY_API size_t CompressBitsStore(VFromD<D> v, const uint8_t* HWY_RESTRICT bits,
                                 D d, TFromD<D>* HWY_RESTRICT unaligned) {
  const RebindToUnsigned<decltype(d)> du;

  // As there are at most 8 lanes in v if sizeof(TFromD<D>) > 1, simply
  // convert bits[0] to a uint64_t
  uint64_t mask_bits = bits[0];
  constexpr size_t kN = MaxLanes(d);
  if (kN < 8) {
    mask_bits &= (1ull << kN) - 1;
  }
  const size_t count = PopCount(mask_bits);

  const auto indices = BitCast(du, detail::IndicesFromBits128(d, mask_bits));
  const auto compressed = BitCast(d, TableLookupBytes(BitCast(du, v), indices));
  StoreU(compressed, d, unaligned);

  return count;
}

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
  // NOTE: AltivecVsum2sws cannot be used here as AltivecVsum2sws
  // computes the signed saturated sum of the lanes.
  return v10 + Shuffle2301(v10);
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
  // NOTE: AltivecVsumsws cannot be used here as AltivecVsumsws
  // computes the signed saturated sum of the lanes.
  const Vec128<T> v1032 = Shuffle1032(v3210);
  const Vec128<T> v31_20_31_20 = v3210 + v1032;
  const Vec128<T> v20_31_20_31 = Shuffle0321(v31_20_31_20);
  return v20_31_20_31 + v31_20_31_20;
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
HWY_INLINE Vec128<T> MinOfLanes(Vec128<T> v10) {
  const Vec128<T> v01 = Shuffle01(v10);
  return Min(v10, v01);
}
template <typename T, HWY_IF_T_SIZE(T, 8)>
HWY_INLINE Vec128<T> MaxOfLanes(Vec128<T> v10) {
  const Vec128<T> v01 = Shuffle01(v10);
  return Max(v10, v01);
}

// Casts nominally int32_t result to D.
template <class D>
HWY_INLINE VFromD<D> AltivecVsum4shs(D d, __vector signed short a,
                                     __vector signed int b) {
  const Repartition<int32_t, D> di32;
#ifdef __OPTIMIZE__
  if (IsConstantRawAltivecVect(a) && IsConstantRawAltivecVect(b)) {
    const int64_t sum0 = static_cast<int64_t>(a[0]) +
                         static_cast<int64_t>(a[1]) +
                         static_cast<int64_t>(b[0]);
    const int64_t sum1 = static_cast<int64_t>(a[2]) +
                         static_cast<int64_t>(a[3]) +
                         static_cast<int64_t>(b[1]);
    const int64_t sum2 = static_cast<int64_t>(a[4]) +
                         static_cast<int64_t>(a[5]) +
                         static_cast<int64_t>(b[2]);
    const int64_t sum3 = static_cast<int64_t>(a[6]) +
                         static_cast<int64_t>(a[7]) +
                         static_cast<int64_t>(b[3]);
    const int32_t sign0 = static_cast<int32_t>(sum0 >> 63);
    const int32_t sign1 = static_cast<int32_t>(sum1 >> 63);
    const int32_t sign2 = static_cast<int32_t>(sum2 >> 63);
    const int32_t sign3 = static_cast<int32_t>(sum3 >> 63);
    using Raw = typename detail::Raw128<int32_t>::type;
    return BitCast(
        d,
        VFromD<decltype(di32)>{Raw{
            (sign0 == (sum0 >> 31)) ? static_cast<int32_t>(sum0)
                                    : static_cast<int32_t>(sign0 ^ 0x7FFFFFFF),
            (sign1 == (sum1 >> 31)) ? static_cast<int32_t>(sum1)
                                    : static_cast<int32_t>(sign1 ^ 0x7FFFFFFF),
            (sign2 == (sum2 >> 31)) ? static_cast<int32_t>(sum2)
                                    : static_cast<int32_t>(sign2 ^ 0x7FFFFFFF),
            (sign3 == (sum3 >> 31))
                ? static_cast<int32_t>(sum3)
                : static_cast<int32_t>(sign3 ^ 0x7FFFFFFF)}});
  } else  // NOLINT
#endif
  {
    return BitCast(d, VFromD<decltype(di32)>{vec_vsum4shs(a, b)});
  }
}

// Casts nominally int32_t result to D.
template <class D>
HWY_INLINE VFromD<D> AltivecVsum4sbs(D d, __vector signed char a,
                                     __vector signed int b) {
  const Repartition<int32_t, D> di32;
#ifdef __OPTIMIZE__
  if (IsConstantRawAltivecVect(a) && IsConstantRawAltivecVect(b)) {
    const int64_t sum0 =
        static_cast<int64_t>(a[0]) + static_cast<int64_t>(a[1]) +
        static_cast<int64_t>(a[2]) + static_cast<int64_t>(a[3]) +
        static_cast<int64_t>(b[0]);
    const int64_t sum1 =
        static_cast<int64_t>(a[4]) + static_cast<int64_t>(a[5]) +
        static_cast<int64_t>(a[6]) + static_cast<int64_t>(a[7]) +
        static_cast<int64_t>(b[1]);
    const int64_t sum2 =
        static_cast<int64_t>(a[8]) + static_cast<int64_t>(a[9]) +
        static_cast<int64_t>(a[10]) + static_cast<int64_t>(a[11]) +
        static_cast<int64_t>(b[2]);
    const int64_t sum3 =
        static_cast<int64_t>(a[12]) + static_cast<int64_t>(a[13]) +
        static_cast<int64_t>(a[14]) + static_cast<int64_t>(a[15]) +
        static_cast<int64_t>(b[3]);
    const int32_t sign0 = static_cast<int32_t>(sum0 >> 63);
    const int32_t sign1 = static_cast<int32_t>(sum1 >> 63);
    const int32_t sign2 = static_cast<int32_t>(sum2 >> 63);
    const int32_t sign3 = static_cast<int32_t>(sum3 >> 63);
    using Raw = typename detail::Raw128<int32_t>::type;
    return BitCast(
        d,
        VFromD<decltype(di32)>{Raw{
            (sign0 == (sum0 >> 31)) ? static_cast<int32_t>(sum0)
                                    : static_cast<int32_t>(sign0 ^ 0x7FFFFFFF),
            (sign1 == (sum1 >> 31)) ? static_cast<int32_t>(sum1)
                                    : static_cast<int32_t>(sign1 ^ 0x7FFFFFFF),
            (sign2 == (sum2 >> 31)) ? static_cast<int32_t>(sum2)
                                    : static_cast<int32_t>(sign2 ^ 0x7FFFFFFF),
            (sign3 == (sum3 >> 31))
                ? static_cast<int32_t>(sum3)
                : static_cast<int32_t>(sign3 ^ 0x7FFFFFFF)}});
  } else  // NOLINT
#endif
  {
    return BitCast(d, VFromD<decltype(di32)>{vec_vsum4sbs(a, b)});
  }
}

// Casts nominally int32_t result to D.
template <class D>
HWY_INLINE VFromD<D> AltivecVsumsws(D d, __vector signed int a,
                                    __vector signed int b) {
  const Repartition<int32_t, D> di32;
#ifdef __OPTIMIZE__
  constexpr int kDestLaneOffset = HWY_IS_LITTLE_ENDIAN ? 0 : 3;
  if (IsConstantRawAltivecVect(a) && __builtin_constant_p(b[kDestLaneOffset])) {
    const int64_t sum =
        static_cast<int64_t>(a[0]) + static_cast<int64_t>(a[1]) +
        static_cast<int64_t>(a[2]) + static_cast<int64_t>(a[3]) +
        static_cast<int64_t>(b[kDestLaneOffset]);
    const int32_t sign = static_cast<int32_t>(sum >> 63);
#if HWY_IS_LITTLE_ENDIAN
    return BitCast(
        d, VFromD<decltype(di32)>{(__vector signed int){
               (sign == (sum >> 31)) ? static_cast<int32_t>(sum)
                                     : static_cast<int32_t>(sign ^ 0x7FFFFFFF),
               0, 0, 0}});
#else
    return BitCast(d, VFromD<decltype(di32)>{(__vector signed int){
                          0, 0, 0,
                          (sign == (sum >> 31))
                              ? static_cast<int32_t>(sum)
                              : static_cast<int32_t>(sign ^ 0x7FFFFFFF)}});
#endif
  } else  // NOLINT
#endif
  {
    __vector signed int sum;

    // Inline assembly is used for vsumsws to avoid unnecessary shuffling
    // on little-endian PowerPC targets as the result of the vsumsws
    // instruction will already be in the correct lanes on little-endian
    // PowerPC targets.
    __asm__("vsumsws %0,%1,%2" : "=v"(sum) : "v"(a), "v"(b));

    return BitCast(d, VFromD<decltype(di32)>{sum});
  }
}

template <size_t N>
HWY_INLINE Vec128<int32_t, N / 2> AltivecU16SumsOf2(Vec128<uint16_t, N> v) {
  const RebindToSigned<DFromV<decltype(v)>> di16;
  const RepartitionToWide<decltype(di16)> di32;
  return AltivecVsum4shs(di32, Xor(BitCast(di16, v), Set(di16, -32768)).raw,
                         Set(di32, 65536).raw);
}

HWY_API Vec32<uint16_t> SumOfLanes(Vec32<uint16_t> v) {
  constexpr int kSumLaneIdx = HWY_IS_BIG_ENDIAN;
  DFromV<decltype(v)> du16;
  return Broadcast<kSumLaneIdx>(BitCast(du16, AltivecU16SumsOf2(v)));
}

HWY_API Vec64<uint16_t> SumOfLanes(Vec64<uint16_t> v) {
  constexpr int kSumLaneIdx = HWY_IS_LITTLE_ENDIAN ? 0 : 3;
  const Full64<uint16_t> du16;
  const auto zero = Zero(Full128<int32_t>());
  return Broadcast<kSumLaneIdx>(
      AltivecVsum2sws(du16, AltivecU16SumsOf2(v).raw, zero.raw));
}

HWY_API Vec128<uint16_t> SumOfLanes(Vec128<uint16_t> v) {
  constexpr int kSumLaneIdx = HWY_IS_LITTLE_ENDIAN ? 0 : 7;
  const Full128<uint16_t> du16;
  const auto zero = Zero(Full128<int32_t>());
  return Broadcast<kSumLaneIdx>(
      AltivecVsumsws(du16, AltivecU16SumsOf2(v).raw, zero.raw));
}

HWY_API Vec32<int16_t> SumOfLanes(Vec32<int16_t> v) {
  constexpr int kSumLaneIdx = HWY_IS_BIG_ENDIAN;
  const Full32<int16_t> di16;
  const auto zero = Zero(Full128<int32_t>());
  return Broadcast<kSumLaneIdx>(AltivecVsum4shs(di16, v.raw, zero.raw));
}

HWY_API Vec64<int16_t> SumOfLanes(Vec64<int16_t> v) {
  constexpr int kSumLaneIdx = HWY_IS_LITTLE_ENDIAN ? 0 : 3;
  const Full128<int32_t> di32;
  const Full64<int16_t> di16;
  const auto zero = Zero(di32);
  return Broadcast<kSumLaneIdx>(AltivecVsum2sws(
      di16, AltivecVsum4shs(di32, v.raw, zero.raw).raw, zero.raw));
}

HWY_API Vec128<int16_t> SumOfLanes(Vec128<int16_t> v) {
  constexpr int kSumLaneIdx = HWY_IS_LITTLE_ENDIAN ? 0 : 7;
  const Full128<int16_t> di16;
  const Full128<int32_t> di32;
  const auto zero = Zero(di32);
  return Broadcast<kSumLaneIdx>(AltivecVsumsws(
      di16, AltivecVsum4shs(di32, v.raw, zero.raw).raw, zero.raw));
}

// u8, N=2, N=4, N=8, N=16:
HWY_API Vec16<uint8_t> SumOfLanes(Vec16<uint8_t> v) {
  constexpr int kSumLaneIdx = HWY_IS_LITTLE_ENDIAN ? 0 : 3;
  const Full16<uint8_t> du8;
  const Full16<uint16_t> du16;
  const Twice<decltype(du8)> dt_u8;
  const Twice<decltype(du16)> dt_u16;
  const Full128<uint32_t> du32;
  return LowerHalf(Broadcast<kSumLaneIdx>(AltivecVsum4ubs(
      dt_u8, BitCast(dt_u8, Combine(dt_u16, Zero(du16), BitCast(du16, v))).raw,
      Zero(du32).raw)));
}

HWY_API Vec32<uint8_t> SumOfLanes(Vec32<uint8_t> v) {
  constexpr int kSumLaneIdx = HWY_IS_LITTLE_ENDIAN ? 0 : 3;
  const Full128<uint32_t> du32;
  const Full32<uint8_t> du8;
  return Broadcast<kSumLaneIdx>(AltivecVsum4ubs(du8, v.raw, Zero(du32).raw));
}

HWY_API Vec64<uint8_t> SumOfLanes(Vec64<uint8_t> v) {
  constexpr int kSumLaneIdx = HWY_IS_LITTLE_ENDIAN ? 0 : 7;
  const Full64<uint8_t> du8;
  return Broadcast<kSumLaneIdx>(BitCast(du8, SumsOf8(v)));
}

HWY_API Vec128<uint8_t> SumOfLanes(Vec128<uint8_t> v) {
  constexpr int kSumLaneIdx = HWY_IS_LITTLE_ENDIAN ? 0 : 15;

  const Full128<uint32_t> du32;
  const RebindToSigned<decltype(du32)> di32;
  const Full128<uint8_t> du8;
  const Vec128<uint32_t> zero = Zero(du32);
  return Broadcast<kSumLaneIdx>(
      AltivecVsumsws(du8, AltivecVsum4ubs(di32, v.raw, zero.raw).raw,
                     BitCast(di32, zero).raw));
}

HWY_API Vec16<int8_t> SumOfLanes(Vec16<int8_t> v) {
  constexpr int kSumLaneIdx = HWY_IS_LITTLE_ENDIAN ? 0 : 3;

  const Full128<uint16_t> du16;
  const Repartition<int32_t, decltype(du16)> di32;
  const Repartition<int8_t, decltype(du16)> di8;
  const Vec128<int8_t> zzvv = BitCast(
      di8, InterleaveLower(BitCast(du16, Vec128<int8_t>{v.raw}), Zero(du16)));
  return Vec16<int8_t>{
      Broadcast<kSumLaneIdx>(AltivecVsum4sbs(di8, zzvv.raw, Zero(di32).raw))
          .raw};
}

HWY_API Vec32<int8_t> SumOfLanes(Vec32<int8_t> v) {
  constexpr int kSumLaneIdx = HWY_IS_LITTLE_ENDIAN ? 0 : 3;
  const Full32<int8_t> di8;
  const Vec128<int32_t> zero = Zero(Full128<int32_t>());
  return Broadcast<kSumLaneIdx>(AltivecVsum4sbs(di8, v.raw, zero.raw));
}

HWY_API Vec64<int8_t> SumOfLanes(Vec64<int8_t> v) {
  constexpr int kSumLaneIdx = HWY_IS_LITTLE_ENDIAN ? 0 : 7;
  const Full128<int32_t> di32;
  const Vec128<int32_t> zero = Zero(di32);
  const Full64<int8_t> di8;
  return Broadcast<kSumLaneIdx>(AltivecVsum2sws(
      di8, AltivecVsum4sbs(di32, v.raw, zero.raw).raw, zero.raw));
}

HWY_API Vec128<int8_t> SumOfLanes(Vec128<int8_t> v) {
  constexpr int kSumLaneIdx = HWY_IS_LITTLE_ENDIAN ? 0 : 15;
  const Full128<int8_t> di8;
  const Full128<int32_t> di32;
  const Vec128<int32_t> zero = Zero(di32);
  return Broadcast<kSumLaneIdx>(AltivecVsumsws(
      di8, AltivecVsum4sbs(di32, v.raw, zero.raw).raw, zero.raw));
}

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

template <size_t N, HWY_IF_V_SIZE_GT(int8_t, N, 4)>
HWY_API Vec128<int8_t, N> MaxOfLanes(Vec128<int8_t, N> v) {
  const DFromV<decltype(v)> d;
  const RepartitionToWide<decltype(d)> d16;
  const RepartitionToWide<decltype(d16)> d32;
  Vec128<int8_t, N> vm = Max(v, Reverse2(d, v));
  vm = Max(vm, BitCast(d, Reverse2(d16, BitCast(d16, vm))));
  vm = Max(vm, BitCast(d, Reverse2(d32, BitCast(d32, vm))));
  if (N > 8) {
    const RepartitionToWide<decltype(d32)> d64;
    vm = Max(vm, BitCast(d, Reverse2(d64, BitCast(d64, vm))));
  }
  return vm;
}

template <size_t N, HWY_IF_V_SIZE_GT(int8_t, N, 4)>
HWY_API Vec128<int8_t, N> MinOfLanes(Vec128<int8_t, N> v) {
  const DFromV<decltype(v)> d;
  const RepartitionToWide<decltype(d)> d16;
  const RepartitionToWide<decltype(d16)> d32;
  Vec128<int8_t, N> vm = Min(v, Reverse2(d, v));
  vm = Min(vm, BitCast(d, Reverse2(d16, BitCast(d16, vm))));
  vm = Min(vm, BitCast(d, Reverse2(d32, BitCast(d32, vm))));
  if (N > 8) {
    const RepartitionToWide<decltype(d32)> d64;
    vm = Min(vm, BitCast(d, Reverse2(d64, BitCast(d64, vm))));
  }
  return vm;
}

template <size_t N, HWY_IF_V_SIZE_GT(uint16_t, N, 2)>
HWY_API Vec128<uint16_t, N> MinOfLanes(Vec128<uint16_t, N> v) {
  const Simd<uint16_t, N, 0> d;
  const RepartitionToWide<decltype(d)> d32;
#if HWY_IS_LITTLE_ENDIAN
  const auto even = And(BitCast(d32, v), Set(d32, 0xFFFF));
  const auto odd = ShiftRight<16>(BitCast(d32, v));
#else
  const auto even = ShiftRight<16>(BitCast(d32, v));
  const auto odd = And(BitCast(d32, v), Set(d32, 0xFFFF));
#endif
  const auto min = MinOfLanes(Min(even, odd));
  // Also broadcast into odd lanes on little-endian and into even lanes
  // on big-endian
  return Vec128<uint16_t, N>{vec_pack(min.raw, min.raw)};
}
template <size_t N, HWY_IF_V_SIZE_GT(int16_t, N, 2)>
HWY_API Vec128<int16_t, N> MinOfLanes(Vec128<int16_t, N> v) {
  const Simd<int16_t, N, 0> d;
  const RepartitionToWide<decltype(d)> d32;
  // Sign-extend
#if HWY_IS_LITTLE_ENDIAN
  const auto even = ShiftRight<16>(ShiftLeft<16>(BitCast(d32, v)));
  const auto odd = ShiftRight<16>(BitCast(d32, v));
#else
  const auto even = ShiftRight<16>(BitCast(d32, v));
  const auto odd = ShiftRight<16>(ShiftLeft<16>(BitCast(d32, v)));
#endif
  const auto min = MinOfLanes(Min(even, odd));
  // Also broadcast into odd lanes on little-endian and into even lanes
  // on big-endian
  return Vec128<int16_t, N>{vec_pack(min.raw, min.raw)};
}

template <size_t N, HWY_IF_V_SIZE_GT(uint16_t, N, 2)>
HWY_API Vec128<uint16_t, N> MaxOfLanes(Vec128<uint16_t, N> v) {
  const Simd<uint16_t, N, 0> d;
  const RepartitionToWide<decltype(d)> d32;
#if HWY_IS_LITTLE_ENDIAN
  const auto even = And(BitCast(d32, v), Set(d32, 0xFFFF));
  const auto odd = ShiftRight<16>(BitCast(d32, v));
#else
  const auto even = ShiftRight<16>(BitCast(d32, v));
  const auto odd = And(BitCast(d32, v), Set(d32, 0xFFFF));
#endif
  const auto max = MaxOfLanes(Max(even, odd));
  // Also broadcast into odd lanes.
  return Vec128<uint16_t, N>{vec_pack(max.raw, max.raw)};
}
template <size_t N, HWY_IF_V_SIZE_GT(int16_t, N, 2)>
HWY_API Vec128<int16_t, N> MaxOfLanes(Vec128<int16_t, N> v) {
  const Simd<int16_t, N, 0> d;
  const RepartitionToWide<decltype(d)> d32;
  // Sign-extend
#if HWY_IS_LITTLE_ENDIAN
  const auto even = ShiftRight<16>(ShiftLeft<16>(BitCast(d32, v)));
  const auto odd = ShiftRight<16>(BitCast(d32, v));
#else
  const auto even = ShiftRight<16>(BitCast(d32, v));
  const auto odd = ShiftRight<16>(ShiftLeft<16>(BitCast(d32, v)));
#endif
  const auto max = MaxOfLanes(Max(even, odd));
  // Also broadcast into odd lanes on little-endian and into even lanes
  // on big-endian
  return Vec128<int16_t, N>{vec_pack(max.raw, max.raw)};
}

}  // namespace detail

// Supported for u/i/f 32/64. Returns the same value in each lane.
template <class D>
HWY_API VFromD<D> SumOfLanes(D /* tag */, VFromD<D> v) {
  return detail::SumOfLanes(v);
}
template <class D>
HWY_API TFromD<D> ReduceSum(D /* tag */, VFromD<D> v) {
  return GetLane(detail::SumOfLanes(v));
}
template <class D>
HWY_API VFromD<D> MinOfLanes(D /* tag */, VFromD<D> v) {
  return detail::MinOfLanes(v);
}
template <class D>
HWY_API VFromD<D> MaxOfLanes(D /* tag */, VFromD<D> v) {
  return detail::MaxOfLanes(v);
}

// ------------------------------ Lt128

namespace detail {

// Returns vector-mask for Lt128.
template <class D, class V = VFromD<D>>
HWY_INLINE V Lt128Vec(D d, V a, V b) {
  static_assert(IsSame<TFromD<D>, uint64_t>(), "D must be u64");
#if HWY_PPC_HAVE_10 && defined(__SIZEOF_INT128__)
  (void)d;
  using VU64 = __vector unsigned long long;
  using VU128 = __vector unsigned __int128;
#if HWY_IS_LITTLE_ENDIAN
  const VU128 a_u128 = reinterpret_cast<VU128>(a.raw);
  const VU128 b_u128 = reinterpret_cast<VU128>(b.raw);
#else
  // NOTE: Need to swap the halves of both a and b on big-endian targets
  // as the upper 64 bits of a and b are in lane 1 and the lower 64 bits
  // of a and b are in lane 0 whereas the vec_cmplt operation below expects
  // the upper 64 bits in lane 0 and the lower 64 bits in lane 1 on
  // big-endian PPC targets.
  const VU128 a_u128 = reinterpret_cast<VU128>(vec_sld(a.raw, a.raw, 8));
  const VU128 b_u128 = reinterpret_cast<VU128>(vec_sld(b.raw, b.raw, 8));
#endif
  return V{reinterpret_cast<VU64>(vec_cmplt(a_u128, b_u128))};
#else  // !HWY_PPC_HAVE_10
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
#endif
}

// Returns vector-mask for Eq128.
template <class D, class V = VFromD<D>>
HWY_INLINE V Eq128Vec(D d, V a, V b) {
  static_assert(IsSame<TFromD<D>, uint64_t>(), "D must be u64");
#if HWY_PPC_HAVE_10 && defined(__SIZEOF_INT128__)
  (void)d;
  using VU64 = __vector unsigned long long;
  using VU128 = __vector unsigned __int128;
  return V{reinterpret_cast<VU64>(vec_cmpeq(reinterpret_cast<VU128>(a.raw),
                                            reinterpret_cast<VU128>(b.raw)))};
#else
  const auto eqHL = VecFromMask(d, Eq(a, b));
  const auto eqLH = Reverse2(d, eqHL);
  return And(eqHL, eqLH);
#endif
}

template <class D, class V = VFromD<D>>
HWY_INLINE V Ne128Vec(D d, V a, V b) {
  static_assert(IsSame<TFromD<D>, uint64_t>(), "D must be u64");
#if HWY_PPC_HAVE_10 && defined(__SIZEOF_INT128__)
  (void)d;
  using VU64 = __vector unsigned long long;
  using VU128 = __vector unsigned __int128;
  return V{reinterpret_cast<VU64>(vec_cmpne(reinterpret_cast<VU128>(a.raw),
                                            reinterpret_cast<VU128>(b.raw)))};
#else
  const auto neHL = VecFromMask(d, Ne(a, b));
  const auto neLH = Reverse2(d, neHL);
  return Or(neHL, neLH);
#endif
}

template <class D, class V = VFromD<D>>
HWY_INLINE V Lt128UpperVec(D d, V a, V b) {
  const V ltHL = VecFromMask(d, Lt(a, b));
  return InterleaveUpper(d, ltHL, ltHL);
}

template <class D, class V = VFromD<D>>
HWY_INLINE V Eq128UpperVec(D d, V a, V b) {
  const V eqHL = VecFromMask(d, Eq(a, b));
  return InterleaveUpper(d, eqHL, eqHL);
}

template <class D, class V = VFromD<D>>
HWY_INLINE V Ne128UpperVec(D d, V a, V b) {
  const V neHL = VecFromMask(d, Ne(a, b));
  return InterleaveUpper(d, neHL, neHL);
}

}  // namespace detail

template <class D, class V = VFromD<D>>
HWY_API MFromD<D> Lt128(D d, V a, V b) {
  return MaskFromVec(detail::Lt128Vec(d, a, b));
}

template <class D, class V = VFromD<D>>
HWY_API MFromD<D> Eq128(D d, V a, V b) {
  return MaskFromVec(detail::Eq128Vec(d, a, b));
}

template <class D, class V = VFromD<D>>
HWY_API MFromD<D> Ne128(D d, V a, V b) {
  return MaskFromVec(detail::Ne128Vec(d, a, b));
}

template <class D, class V = VFromD<D>>
HWY_API MFromD<D> Lt128Upper(D d, V a, V b) {
  return MaskFromVec(detail::Lt128UpperVec(d, a, b));
}

template <class D, class V = VFromD<D>>
HWY_API MFromD<D> Eq128Upper(D d, V a, V b) {
  return MaskFromVec(detail::Eq128UpperVec(d, a, b));
}

template <class D, class V = VFromD<D>>
HWY_API MFromD<D> Ne128Upper(D d, V a, V b) {
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

#ifdef HWY_NATIVE_LEADING_ZERO_COUNT
#undef HWY_NATIVE_LEADING_ZERO_COUNT
#else
#define HWY_NATIVE_LEADING_ZERO_COUNT
#endif

template <class V, HWY_IF_NOT_FLOAT_NOR_SPECIAL_V(V)>
HWY_API V LeadingZeroCount(V v) {
  return V{vec_cntlz(v.raw)};
}

template <class V, HWY_IF_NOT_FLOAT_NOR_SPECIAL_V(V)>
HWY_API V HighestSetBitIndex(V v) {
  const DFromV<decltype(v)> d;
  using T = TFromD<decltype(d)>;
  return BitCast(d, Set(d, T{sizeof(T) * 8 - 1}) - LeadingZeroCount(v));
}

#if HWY_PPC_HAVE_9
template <class V, HWY_IF_NOT_FLOAT_NOR_SPECIAL_V(V)>
HWY_API V TrailingZeroCount(V v) {
#if HWY_COMPILER_GCC_ACTUAL && HWY_COMPILER_GCC_ACTUAL < 700
  return V{vec_vctz(v.raw)};
#else
  return V{vec_cnttz(v.raw)};
#endif
}
#else
template <class V, HWY_IF_NOT_FLOAT_NOR_SPECIAL_V(V)>
HWY_API V TrailingZeroCount(V v) {
  const DFromV<decltype(v)> d;
  const RebindToSigned<decltype(d)> di;
  using TI = TFromD<decltype(di)>;

  const auto vi = BitCast(di, v);
  const auto lowest_bit = And(vi, Neg(vi));
  constexpr TI kNumOfBitsInT{sizeof(TI) * 8};
  const auto bit_idx = HighestSetBitIndex(lowest_bit);
  return BitCast(d, IfThenElse(MaskFromVec(BroadcastSignBit(bit_idx)),
                               Set(di, kNumOfBitsInT), bit_idx));
}
#endif

#undef HWY_PPC_HAVE_9
#undef HWY_PPC_HAVE_10

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace hwy
HWY_AFTER_NAMESPACE();
