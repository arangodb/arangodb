// Copyright 2021 Google LLC
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

// ARM SVE[2] vectors (length not known at compile time).
// External include guard in highway.h - see comment there.

#include <stddef.h>
#include <stdint.h>

#if defined(HWY_EMULATE_SVE)
#include "third_party/farm_sve/farm_sve.h"
#else
#include <arm_sve.h>
#endif

#include "hwy/base.h"
#include "hwy/ops/shared-inl.h"

HWY_BEFORE_NAMESPACE();
namespace hwy {
namespace HWY_NAMESPACE {

// SVE only supports fractions, not LMUL > 1.
template <typename T, int kShift = 0>
using Full = Simd<T, (kShift <= 0) ? (HWY_LANES(T) >> (-kShift)) : 0>;

template <class V>
struct DFromV_t {};  // specialized in macros
template <class V>
using DFromV = typename DFromV_t<RemoveConst<V>>::type;

template <class V>
using TFromV = TFromD<DFromV<V>>;

#define HWY_IF_UNSIGNED_V(V) HWY_IF_UNSIGNED(TFromV<V>)
#define HWY_IF_SIGNED_V(V) HWY_IF_SIGNED(TFromV<V>)
#define HWY_IF_FLOAT_V(V) HWY_IF_FLOAT(TFromV<V>)
#define HWY_IF_LANE_SIZE_V(V, bytes) HWY_IF_LANE_SIZE(TFromV<V>, bytes)

// ================================================== MACROS

// Generate specializations and function definitions using X macros. Although
// harder to read and debug, writing everything manually is too bulky.

namespace detail {  // for code folding

// Unsigned:
#define HWY_SVE_FOREACH_U08(X_MACRO, NAME, OP) X_MACRO(uint, u, 8, NAME, OP)
#define HWY_SVE_FOREACH_U16(X_MACRO, NAME, OP) X_MACRO(uint, u, 16, NAME, OP)
#define HWY_SVE_FOREACH_U32(X_MACRO, NAME, OP) X_MACRO(uint, u, 32, NAME, OP)
#define HWY_SVE_FOREACH_U64(X_MACRO, NAME, OP) X_MACRO(uint, u, 64, NAME, OP)

// Signed:
#define HWY_SVE_FOREACH_I08(X_MACRO, NAME, OP) X_MACRO(int, s, 8, NAME, OP)
#define HWY_SVE_FOREACH_I16(X_MACRO, NAME, OP) X_MACRO(int, s, 16, NAME, OP)
#define HWY_SVE_FOREACH_I32(X_MACRO, NAME, OP) X_MACRO(int, s, 32, NAME, OP)
#define HWY_SVE_FOREACH_I64(X_MACRO, NAME, OP) X_MACRO(int, s, 64, NAME, OP)

// Float:
#define HWY_SVE_FOREACH_F16(X_MACRO, NAME, OP) X_MACRO(float, f, 16, NAME, OP)
#define HWY_SVE_FOREACH_F32(X_MACRO, NAME, OP) X_MACRO(float, f, 32, NAME, OP)
#define HWY_SVE_FOREACH_F64(X_MACRO, NAME, OP) X_MACRO(float, f, 64, NAME, OP)

// For all element sizes:
#define HWY_SVE_FOREACH_U(X_MACRO, NAME, OP) \
  HWY_SVE_FOREACH_U08(X_MACRO, NAME, OP)     \
  HWY_SVE_FOREACH_U16(X_MACRO, NAME, OP)     \
  HWY_SVE_FOREACH_U32(X_MACRO, NAME, OP)     \
  HWY_SVE_FOREACH_U64(X_MACRO, NAME, OP)

#define HWY_SVE_FOREACH_I(X_MACRO, NAME, OP) \
  HWY_SVE_FOREACH_I08(X_MACRO, NAME, OP)     \
  HWY_SVE_FOREACH_I16(X_MACRO, NAME, OP)     \
  HWY_SVE_FOREACH_I32(X_MACRO, NAME, OP)     \
  HWY_SVE_FOREACH_I64(X_MACRO, NAME, OP)

#define HWY_SVE_FOREACH_F(X_MACRO, NAME, OP) \
  HWY_SVE_FOREACH_F16(X_MACRO, NAME, OP)     \
  HWY_SVE_FOREACH_F32(X_MACRO, NAME, OP)     \
  HWY_SVE_FOREACH_F64(X_MACRO, NAME, OP)

// Commonly used type categories for a given element size:
#define HWY_SVE_FOREACH_UI08(X_MACRO, NAME, OP) \
  HWY_SVE_FOREACH_U08(X_MACRO, NAME, OP)        \
  HWY_SVE_FOREACH_I08(X_MACRO, NAME, OP)

#define HWY_SVE_FOREACH_UI16(X_MACRO, NAME, OP) \
  HWY_SVE_FOREACH_U16(X_MACRO, NAME, OP)        \
  HWY_SVE_FOREACH_I16(X_MACRO, NAME, OP)

#define HWY_SVE_FOREACH_UI32(X_MACRO, NAME, OP) \
  HWY_SVE_FOREACH_U32(X_MACRO, NAME, OP)        \
  HWY_SVE_FOREACH_I32(X_MACRO, NAME, OP)

#define HWY_SVE_FOREACH_UI64(X_MACRO, NAME, OP) \
  HWY_SVE_FOREACH_U64(X_MACRO, NAME, OP)        \
  HWY_SVE_FOREACH_I64(X_MACRO, NAME, OP)

#define HWY_SVE_FOREACH_UIF3264(X_MACRO, NAME, OP) \
  HWY_SVE_FOREACH_UI32(X_MACRO, NAME, OP)          \
  HWY_SVE_FOREACH_UI64(X_MACRO, NAME, OP)          \
  HWY_SVE_FOREACH_F32(X_MACRO, NAME, OP)           \
  HWY_SVE_FOREACH_F64(X_MACRO, NAME, OP)

// Commonly used type categories:
#define HWY_SVE_FOREACH_UI(X_MACRO, NAME, OP) \
  HWY_SVE_FOREACH_U(X_MACRO, NAME, OP)        \
  HWY_SVE_FOREACH_I(X_MACRO, NAME, OP)

#define HWY_SVE_FOREACH_IF(X_MACRO, NAME, OP) \
  HWY_SVE_FOREACH_I(X_MACRO, NAME, OP)        \
  HWY_SVE_FOREACH_F(X_MACRO, NAME, OP)

#define HWY_SVE_FOREACH(X_MACRO, NAME, OP) \
  HWY_SVE_FOREACH_U(X_MACRO, NAME, OP)     \
  HWY_SVE_FOREACH_I(X_MACRO, NAME, OP)     \
  HWY_SVE_FOREACH_F(X_MACRO, NAME, OP)

// Assemble types for use in x-macros
#define HWY_SVE_T(BASE, BITS) BASE##BITS##_t
#define HWY_SVE_D(BASE, BITS, N) Simd<HWY_SVE_T(BASE, BITS), N>
#define HWY_SVE_V(BASE, BITS) sv##BASE##BITS##_t

}  // namespace detail

#define HWY_SPECIALIZE(BASE, CHAR, BITS, NAME, OP)                        \
  template <>                                                             \
  struct DFromV_t<HWY_SVE_V(BASE, BITS)> {                                \
    using type = HWY_SVE_D(BASE, BITS, HWY_LANES(HWY_SVE_T(BASE, BITS))); \
  };

HWY_SVE_FOREACH(HWY_SPECIALIZE, _, _)
#undef HWY_SPECIALIZE

// vector = f(d), e.g. Undefined
#define HWY_SVE_RETV_ARGD(BASE, CHAR, BITS, NAME, OP)              \
  template <size_t N>                                              \
  HWY_API HWY_SVE_V(BASE, BITS) NAME(HWY_SVE_D(BASE, BITS, N) d) { \
    return sv##OP##_##CHAR##BITS();                                \
  }

// Note: _x (don't-care value for inactive lanes) avoids additional MOVPRFX
// instructions, and we anyway only use it when the predicate is ptrue.

// vector = f(vector), e.g. Not
#define HWY_SVE_RETV_ARGPV(BASE, CHAR, BITS, NAME, OP)          \
  HWY_API HWY_SVE_V(BASE, BITS) NAME(HWY_SVE_V(BASE, BITS) v) { \
    return sv##OP##_##CHAR##BITS##_x(HWY_SVE_PTRUE(BITS), v);   \
  }
#define HWY_SVE_RETV_ARGV(BASE, CHAR, BITS, NAME, OP)           \
  HWY_API HWY_SVE_V(BASE, BITS) NAME(HWY_SVE_V(BASE, BITS) v) { \
    return sv##OP##_##CHAR##BITS(v);                            \
  }

// vector = f(vector, scalar), e.g. detail::AddK
#define HWY_SVE_RETV_ARGPVN(BASE, CHAR, BITS, NAME, OP)          \
  HWY_API HWY_SVE_V(BASE, BITS)                                  \
      NAME(HWY_SVE_V(BASE, BITS) a, HWY_SVE_T(BASE, BITS) b) {   \
    return sv##OP##_##CHAR##BITS##_x(HWY_SVE_PTRUE(BITS), a, b); \
  }
#define HWY_SVE_RETV_ARGVN(BASE, CHAR, BITS, NAME, OP)         \
  HWY_API HWY_SVE_V(BASE, BITS)                                \
      NAME(HWY_SVE_V(BASE, BITS) a, HWY_SVE_T(BASE, BITS) b) { \
    return sv##OP##_##CHAR##BITS(a, b);                        \
  }

// vector = f(vector, vector), e.g. Add
#define HWY_SVE_RETV_ARGPVV(BASE, CHAR, BITS, NAME, OP)          \
  HWY_API HWY_SVE_V(BASE, BITS)                                  \
      NAME(HWY_SVE_V(BASE, BITS) a, HWY_SVE_V(BASE, BITS) b) {   \
    return sv##OP##_##CHAR##BITS##_x(HWY_SVE_PTRUE(BITS), a, b); \
  }
#define HWY_SVE_RETV_ARGVV(BASE, CHAR, BITS, NAME, OP)         \
  HWY_API HWY_SVE_V(BASE, BITS)                                \
      NAME(HWY_SVE_V(BASE, BITS) a, HWY_SVE_V(BASE, BITS) b) { \
    return sv##OP##_##CHAR##BITS(a, b);                        \
  }

// ------------------------------ Lanes

namespace detail {

// Returns actual lanes of a hardware vector, rounded down to a power of two.
HWY_INLINE size_t HardwareLanes(hwy::SizeTag<1> /* tag */) {
  return svcntb_pat(SV_POW2);
}
HWY_INLINE size_t HardwareLanes(hwy::SizeTag<2> /* tag */) {
  return svcnth_pat(SV_POW2);
}
HWY_INLINE size_t HardwareLanes(hwy::SizeTag<8> /* tag */) {
  return svcntd_pat(SV_POW2);
}
HWY_INLINE size_t HardwareLanes(hwy::SizeTag<4> /* tag */) {
  return svcntw_pat(SV_POW2);
}

}  // namespace detail

// Capped to <= 128-bit: SVE is at least that large, so no need to query actual.
template <typename T, size_t N, HWY_IF_LE128(T, N)>
HWY_API constexpr size_t Lanes(Simd<T, N> /* tag */) {
  return N;
}

// Returns actual number of lanes after dividing by div={1,2,4,8}.
// May return 0 if div > 16/sizeof(T): there is no "1/8th" of a u32x4, but it
// would be valid for u32x8 (i.e. hardware vectors >= 256 bits).
template <typename T, size_t N, HWY_IF_GT128(T, N)>
HWY_API size_t Lanes(Simd<T, N> /* tag */) {
  static_assert(N <= HWY_LANES(T), "N cannot exceed a full vector");

  const size_t actual = detail::HardwareLanes(hwy::SizeTag<sizeof(T)>());
  const size_t div = HWY_LANES(T) / N;
  static_assert(div <= 8, "Invalid N - must be <=128 bit, or >=1/8th");
  return actual / div;
}

// ================================================== MASK INIT

// One mask bit per byte; only the one belonging to the lowest byte is valid.

// ------------------------------ FirstN
#define HWY_SVE_FIRSTN(BASE, CHAR, BITS, NAME, OP)                       \
  template <size_t KN>                                                   \
  HWY_API svbool_t NAME(HWY_SVE_D(BASE, BITS, KN) /* d */, uint32_t N) { \
    return sv##OP##_b##BITS##_u32(uint32_t(0), N);                       \
  }
HWY_SVE_FOREACH(HWY_SVE_FIRSTN, FirstN, whilelt)
#undef HWY_SVE_FIRSTN

namespace detail {

// All-true mask from a macro
#define HWY_SVE_PTRUE(BITS) svptrue_pat_b##BITS(SV_POW2)

#define HWY_SVE_WRAP_PTRUE(BASE, CHAR, BITS, NAME, OP) \
  template <size_t N>                                  \
  HWY_API svbool_t NAME(HWY_SVE_D(BASE, BITS, N) d) {  \
    return HWY_SVE_PTRUE(BITS);                        \
  }

HWY_SVE_FOREACH(HWY_SVE_WRAP_PTRUE, PTrue, ptrue)  // return all-true
#undef HWY_SVE_WRAP_PTRUE

HWY_API svbool_t PFalse() { return svpfalse_b(); }

// Returns all-true if d is HWY_FULL or FirstN(N) after capping N.
//
// This is used in functions that load/store memory; other functions (e.g.
// arithmetic on partial vectors) can ignore d and use PTrue instead.
template <typename T, size_t N>
svbool_t Mask(Simd<T, N> d) {
  return N == HWY_LANES(T) ? PTrue(d) : FirstN(d, Lanes(d));
}

}  // namespace detail

// ================================================== INIT

// ------------------------------ Set
// vector = f(d, scalar), e.g. Set
#define HWY_SVE_SET(BASE, CHAR, BITS, NAME, OP)                     \
  template <size_t N>                                               \
  HWY_API HWY_SVE_V(BASE, BITS)                                     \
      NAME(HWY_SVE_D(BASE, BITS, N) d, HWY_SVE_T(BASE, BITS) arg) { \
    return sv##OP##_##CHAR##BITS(arg);                              \
  }

HWY_SVE_FOREACH(HWY_SVE_SET, Set, dup_n)
#undef HWY_SVE_SET

template <class D>
using VFromD = decltype(Set(D(), 0));

// ------------------------------ Zero

template <class D>
VFromD<D> Zero(D d) {
  return Set(d, 0);
}

// ------------------------------ Undefined

#if defined(HWY_EMULATE_SVE)
template <class D>
VFromD<D> Undefined(D d) {
  return Zero(d);
}
#else
HWY_SVE_FOREACH(HWY_SVE_RETV_ARGD, Undefined, undef)
#endif

// ------------------------------ BitCast

namespace detail {

// u8: no change
#define HWY_SVE_CAST_NOP(BASE, CHAR, BITS, NAME, OP)                     \
  HWY_API HWY_SVE_V(BASE, BITS) BitCastToByte(HWY_SVE_V(BASE, BITS) v) { \
    return v;                                                            \
  }                                                                      \
  template <size_t N>                                                    \
  HWY_API HWY_SVE_V(BASE, BITS) BitCastFromByte(                         \
      HWY_SVE_D(BASE, BITS, N) /* d */, HWY_SVE_V(BASE, BITS) v) {       \
    return v;                                                            \
  }

// All other types
#define HWY_SVE_CAST(BASE, CHAR, BITS, NAME, OP)                       \
  HWY_INLINE svuint8_t BitCastToByte(HWY_SVE_V(BASE, BITS) v) {        \
    return sv##OP##_u8_##CHAR##BITS(v);                                \
  }                                                                    \
  template <size_t N>                                                  \
  HWY_INLINE HWY_SVE_V(BASE, BITS)                                     \
      BitCastFromByte(HWY_SVE_D(BASE, BITS, N) /* d */, svuint8_t v) { \
    return sv##OP##_##CHAR##BITS##_u8(v);                              \
  }

HWY_SVE_FOREACH_U08(HWY_SVE_CAST_NOP, _, _)
HWY_SVE_FOREACH_I08(HWY_SVE_CAST, _, reinterpret)
HWY_SVE_FOREACH_UI16(HWY_SVE_CAST, _, reinterpret)
HWY_SVE_FOREACH_UI32(HWY_SVE_CAST, _, reinterpret)
HWY_SVE_FOREACH_UI64(HWY_SVE_CAST, _, reinterpret)
HWY_SVE_FOREACH_F(HWY_SVE_CAST, _, reinterpret)

#undef HWY_SVE_CAST_NOP
#undef HWY_SVE_CAST

}  // namespace detail

template <class D, class FromV>
HWY_API VFromD<D> BitCast(D d, FromV v) {
  return detail::BitCastFromByte(d, detail::BitCastToByte(v));
}

// ================================================== LOGICAL

// detail::*N() functions accept a scalar argument to avoid extra Set().

// ------------------------------ Not

HWY_SVE_FOREACH_UI(HWY_SVE_RETV_ARGPV, Not, not )

// ------------------------------ And

namespace detail {
HWY_SVE_FOREACH_UI(HWY_SVE_RETV_ARGPVN, AndN, and_n)
}  // namespace detail

HWY_SVE_FOREACH_UI(HWY_SVE_RETV_ARGPVV, And, and)

template <class V, HWY_IF_FLOAT_V(V)>
HWY_API V And(const V a, const V b) {
  const DFromV<V> df;
  const RebindToUnsigned<decltype(df)> du;
  return BitCast(df, And(BitCast(du, a), BitCast(du, b)));
}

// ------------------------------ Or

HWY_SVE_FOREACH_UI(HWY_SVE_RETV_ARGPVV, Or, orr)

template <class V, HWY_IF_FLOAT_V(V)>
HWY_API V Or(const V a, const V b) {
  const DFromV<V> df;
  const RebindToUnsigned<decltype(df)> du;
  return BitCast(df, Or(BitCast(du, a), BitCast(du, b)));
}

// ------------------------------ Xor

namespace detail {
HWY_SVE_FOREACH_UI(HWY_SVE_RETV_ARGPVN, XorN, eor_n)
}  // namespace detail

HWY_SVE_FOREACH_UI(HWY_SVE_RETV_ARGPVV, Xor, eor)

template <class V, HWY_IF_FLOAT_V(V)>
HWY_API V Xor(const V a, const V b) {
  const DFromV<V> df;
  const RebindToUnsigned<decltype(df)> du;
  return BitCast(df, Xor(BitCast(du, a), BitCast(du, b)));
}

// ------------------------------ AndNot

namespace detail {
#define HWY_SVE_RETV_ARGPVN_SWAP(BASE, CHAR, BITS, NAME, OP)     \
  HWY_API HWY_SVE_V(BASE, BITS)                                  \
      NAME(HWY_SVE_T(BASE, BITS) a, HWY_SVE_V(BASE, BITS) b) {   \
    return sv##OP##_##CHAR##BITS##_x(HWY_SVE_PTRUE(BITS), b, a); \
  }

HWY_SVE_FOREACH_UI(HWY_SVE_RETV_ARGPVN_SWAP, AndNotN, bic_n)
#undef HWY_SVE_RETV_ARGPVN_SWAP
}  // namespace detail

#define HWY_SVE_RETV_ARGPVV_SWAP(BASE, CHAR, BITS, NAME, OP)     \
  HWY_API HWY_SVE_V(BASE, BITS)                                  \
      NAME(HWY_SVE_V(BASE, BITS) a, HWY_SVE_V(BASE, BITS) b) {   \
    return sv##OP##_##CHAR##BITS##_x(HWY_SVE_PTRUE(BITS), b, a); \
  }
HWY_SVE_FOREACH_UI(HWY_SVE_RETV_ARGPVV_SWAP, AndNot, bic)
#undef HWY_SVE_RETV_ARGPVV_SWAP

template <class V, HWY_IF_FLOAT_V(V)>
HWY_API V AndNot(const V a, const V b) {
  const DFromV<V> df;
  const RebindToUnsigned<decltype(df)> du;
  return BitCast(df, AndNot(BitCast(du, a), BitCast(du, b)));
}

// ------------------------------ PopulationCount

#ifdef HWY_NATIVE_POPCNT
#undef HWY_NATIVE_POPCNT
#else
#define HWY_NATIVE_POPCNT
#endif

// Need to return original type instead of unsigned.
#define HWY_SVE_POPCNT(BASE, CHAR, BITS, NAME, OP)                     \
  HWY_API HWY_SVE_V(BASE, BITS) NAME(HWY_SVE_V(BASE, BITS) v) {        \
    return BitCast(DFromV<decltype(v)>(),                              \
                   sv##OP##_##CHAR##BITS##_x(HWY_SVE_PTRUE(BITS), v)); \
  }
HWY_SVE_FOREACH_UI(HWY_SVE_POPCNT, PopulationCount, cnt)
#undef HWY_SVE_POPCNT

// ================================================== SIGN

// ------------------------------ Neg
HWY_SVE_FOREACH_IF(HWY_SVE_RETV_ARGPV, Neg, neg)

// ------------------------------ Abs
HWY_SVE_FOREACH_IF(HWY_SVE_RETV_ARGPV, Abs, abs)

// ------------------------------ CopySign[ToAbs]

template <class V>
HWY_API V CopySign(const V magn, const V sign) {
  const auto msb = SignBit(DFromV<V>());
  return Or(AndNot(msb, magn), And(msb, sign));
}

template <class V>
HWY_API V CopySignToAbs(const V abs, const V sign) {
  const auto msb = SignBit(DFromV<V>());
  return Or(abs, And(msb, sign));
}

// ================================================== ARITHMETIC

// ------------------------------ Add

namespace detail {
HWY_SVE_FOREACH(HWY_SVE_RETV_ARGPVN, AddN, add_n)
}  // namespace detail

HWY_SVE_FOREACH(HWY_SVE_RETV_ARGPVV, Add, add)

// ------------------------------ Sub

namespace detail {
// Can't use HWY_SVE_RETV_ARGPVN because caller wants to specify pg.
#define HWY_SVE_RETV_ARGPVN_MASK(BASE, CHAR, BITS, NAME, OP)                \
  HWY_API HWY_SVE_V(BASE, BITS)                                             \
      NAME(svbool_t pg, HWY_SVE_V(BASE, BITS) a, HWY_SVE_T(BASE, BITS) b) { \
    return sv##OP##_##CHAR##BITS##_z(pg, a, b);                             \
  }

HWY_SVE_FOREACH(HWY_SVE_RETV_ARGPVN_MASK, SubN, sub_n)
#undef HWY_SVE_RETV_ARGPVN_MASK
}  // namespace detail

HWY_SVE_FOREACH(HWY_SVE_RETV_ARGPVV, Sub, sub)

// ------------------------------ SaturatedAdd

HWY_SVE_FOREACH_UI08(HWY_SVE_RETV_ARGVV, SaturatedAdd, qadd)
HWY_SVE_FOREACH_UI16(HWY_SVE_RETV_ARGVV, SaturatedAdd, qadd)

// ------------------------------ SaturatedSub

HWY_SVE_FOREACH_UI08(HWY_SVE_RETV_ARGVV, SaturatedSub, qsub)
HWY_SVE_FOREACH_UI16(HWY_SVE_RETV_ARGVV, SaturatedSub, qsub)

// ------------------------------ AbsDiff
HWY_SVE_FOREACH_IF(HWY_SVE_RETV_ARGPVV, AbsDiff, abd)

// ------------------------------ ShiftLeft[Same]

#define HWY_SVE_SHIFT_N(BASE, CHAR, BITS, NAME, OP)                     \
  template <int kBits>                                                  \
  HWY_API HWY_SVE_V(BASE, BITS) NAME(HWY_SVE_V(BASE, BITS) v) {         \
    return sv##OP##_##CHAR##BITS##_x(HWY_SVE_PTRUE(BITS), v, kBits);    \
  }                                                                     \
  HWY_API HWY_SVE_V(BASE, BITS)                                         \
      NAME##Same(HWY_SVE_V(BASE, BITS) v, HWY_SVE_T(uint, BITS) bits) { \
    return sv##OP##_##CHAR##BITS##_x(HWY_SVE_PTRUE(BITS), v, bits);     \
  }

HWY_SVE_FOREACH_UI(HWY_SVE_SHIFT_N, ShiftLeft, lsl_n)

// ------------------------------ ShiftRight[Same]

HWY_SVE_FOREACH_U(HWY_SVE_SHIFT_N, ShiftRight, lsr_n)
HWY_SVE_FOREACH_I(HWY_SVE_SHIFT_N, ShiftRight, asr_n)

#undef HWY_SVE_SHIFT_N

// ------------------------------ Shl/r

#define HWY_SVE_SHIFT(BASE, CHAR, BITS, NAME, OP)                          \
  HWY_API HWY_SVE_V(BASE, BITS)                                            \
      NAME(HWY_SVE_V(BASE, BITS) v, HWY_SVE_V(BASE, BITS) bits) {          \
    using TU = HWY_SVE_T(uint, BITS);                                      \
    return sv##OP##_##CHAR##BITS##_x(                                      \
        HWY_SVE_PTRUE(BITS), v, BitCast(Simd<TU, HWY_LANES(TU)>(), bits)); \
  }

HWY_SVE_FOREACH_UI(HWY_SVE_SHIFT, Shl, lsl)

HWY_SVE_FOREACH_U(HWY_SVE_SHIFT, Shr, lsr)
HWY_SVE_FOREACH_I(HWY_SVE_SHIFT, Shr, asr)

#undef HWY_SVE_SHIFT

// ------------------------------ Min/Max

HWY_SVE_FOREACH_UI(HWY_SVE_RETV_ARGPVV, Min, min)
HWY_SVE_FOREACH_UI(HWY_SVE_RETV_ARGPVV, Max, max)
HWY_SVE_FOREACH_F(HWY_SVE_RETV_ARGPVV, Min, minnm)
HWY_SVE_FOREACH_F(HWY_SVE_RETV_ARGPVV, Max, maxnm)

namespace detail {
HWY_SVE_FOREACH_UI(HWY_SVE_RETV_ARGPVN, MinN, min_n)
HWY_SVE_FOREACH_UI(HWY_SVE_RETV_ARGPVN, MaxN, max_n)
}  // namespace detail

// ------------------------------ Mul
HWY_SVE_FOREACH_UI16(HWY_SVE_RETV_ARGPVV, Mul, mul)
HWY_SVE_FOREACH_UIF3264(HWY_SVE_RETV_ARGPVV, Mul, mul)

// ------------------------------ MulHigh
HWY_SVE_FOREACH_UI16(HWY_SVE_RETV_ARGPVV, MulHigh, mulh)
namespace detail {
HWY_SVE_FOREACH_UI32(HWY_SVE_RETV_ARGPVV, MulHigh, mulh)
HWY_SVE_FOREACH_U64(HWY_SVE_RETV_ARGPVV, MulHigh, mulh)
}  // namespace detail

// ------------------------------ Div
HWY_SVE_FOREACH_F(HWY_SVE_RETV_ARGPVV, Div, div)

// ------------------------------ ApproximateReciprocal
HWY_SVE_FOREACH_F32(HWY_SVE_RETV_ARGV, ApproximateReciprocal, recpe)

// ------------------------------ Sqrt
HWY_SVE_FOREACH_F(HWY_SVE_RETV_ARGPV, Sqrt, sqrt)

// ------------------------------ ApproximateReciprocalSqrt
HWY_SVE_FOREACH_F32(HWY_SVE_RETV_ARGV, ApproximateReciprocalSqrt, rsqrte)

// ------------------------------ MulAdd
#define HWY_SVE_FMA(BASE, CHAR, BITS, NAME, OP)                         \
  HWY_API HWY_SVE_V(BASE, BITS)                                         \
      NAME(HWY_SVE_V(BASE, BITS) mul, HWY_SVE_V(BASE, BITS) x,          \
           HWY_SVE_V(BASE, BITS) add) {                                 \
    return sv##OP##_##CHAR##BITS##_x(HWY_SVE_PTRUE(BITS), x, mul, add); \
  }

HWY_SVE_FOREACH_F(HWY_SVE_FMA, MulAdd, mad)

// ------------------------------ NegMulAdd
HWY_SVE_FOREACH_F(HWY_SVE_FMA, NegMulAdd, msb)

// ------------------------------ MulSub
HWY_SVE_FOREACH_F(HWY_SVE_FMA, MulSub, nmsb)

// ------------------------------ NegMulSub
HWY_SVE_FOREACH_F(HWY_SVE_FMA, NegMulSub, nmad)

#undef HWY_SVE_FMA

// ------------------------------ Round etc.

HWY_SVE_FOREACH_F(HWY_SVE_RETV_ARGPV, Round, rintn)
HWY_SVE_FOREACH_F(HWY_SVE_RETV_ARGPV, Floor, rintm)
HWY_SVE_FOREACH_F(HWY_SVE_RETV_ARGPV, Ceil, rintp)
HWY_SVE_FOREACH_F(HWY_SVE_RETV_ARGPV, Trunc, rintz)

// ================================================== MASK

// ------------------------------ RebindMask
template <class D, typename MFrom>
HWY_API svbool_t RebindMask(const D /*d*/, const MFrom mask) {
  return mask;
}

// ------------------------------ Mask logical

HWY_API svbool_t Not(svbool_t m) {
  // We don't know the lane type, so assume 8-bit. For larger types, this will
  // de-canonicalize the predicate, i.e. set bits to 1 even though they do not
  // correspond to the lowest byte in the lane. Per ARM, such bits are ignored.
  return svnot_b_z(HWY_SVE_PTRUE(8), m);
}
HWY_API svbool_t And(svbool_t a, svbool_t b) {
  return svand_b_z(b, b, a);  // same order as AndNot for consistency
}
HWY_API svbool_t AndNot(svbool_t a, svbool_t b) {
  return svbic_b_z(b, b, a);  // reversed order like NEON
}
HWY_API svbool_t Or(svbool_t a, svbool_t b) {
  return svsel_b(a, a, b);  // a ? true : b
}
HWY_API svbool_t Xor(svbool_t a, svbool_t b) {
  return svsel_b(a, svnand_b_z(a, a, b), b);  // a ? !(a & b) : b.
}

// ------------------------------ CountTrue

#define HWY_SVE_COUNT_TRUE(BASE, CHAR, BITS, NAME, OP)          \
  template <size_t N>                                           \
  HWY_API size_t NAME(HWY_SVE_D(BASE, BITS, N) d, svbool_t m) { \
    return sv##OP##_b##BITS(detail::Mask(d), m);                \
  }

HWY_SVE_FOREACH(HWY_SVE_COUNT_TRUE, CountTrue, cntp)
#undef HWY_SVE_COUNT_TRUE

// For 16-bit Compress: full vector, not limited to SV_POW2.
namespace detail {

#define HWY_SVE_COUNT_TRUE_FULL(BASE, CHAR, BITS, NAME, OP)     \
  template <size_t N>                                           \
  HWY_API size_t NAME(HWY_SVE_D(BASE, BITS, N) d, svbool_t m) { \
    return sv##OP##_b##BITS(svptrue_b##BITS(), m);              \
  }

HWY_SVE_FOREACH(HWY_SVE_COUNT_TRUE_FULL, CountTrueFull, cntp)
#undef HWY_SVE_COUNT_TRUE_FULL

}  // namespace detail

// ------------------------------ AllFalse
template <typename T, size_t N>
HWY_API bool AllFalse(Simd<T, N> d, svbool_t m) {
  return !svptest_any(detail::Mask(d), m);
}

// ------------------------------ AllTrue
template <typename T, size_t N>
HWY_API bool AllTrue(Simd<T, N> d, svbool_t m) {
  return CountTrue(d, m) == Lanes(d);
}

// ------------------------------ FindFirstTrue
template <typename T, size_t N>
HWY_API intptr_t FindFirstTrue(Simd<T, N> d, svbool_t m) {
  return AllFalse(d, m) ? -1 : CountTrue(d, svbrkb_b_z(detail::Mask(d), m));
}

// ------------------------------ IfThenElse
#define HWY_SVE_IF_THEN_ELSE(BASE, CHAR, BITS, NAME, OP)                      \
  HWY_API HWY_SVE_V(BASE, BITS)                                               \
      NAME(svbool_t m, HWY_SVE_V(BASE, BITS) yes, HWY_SVE_V(BASE, BITS) no) { \
    return sv##OP##_##CHAR##BITS(m, yes, no);                                 \
  }

HWY_SVE_FOREACH(HWY_SVE_IF_THEN_ELSE, IfThenElse, sel)
#undef HWY_SVE_IF_THEN_ELSE

// ------------------------------ IfThenElseZero
template <class M, class V>
HWY_API V IfThenElseZero(const M mask, const V yes) {
  return IfThenElse(mask, yes, Zero(DFromV<V>()));
}

// ------------------------------ IfThenZeroElse
template <class M, class V>
HWY_API V IfThenZeroElse(const M mask, const V no) {
  return IfThenElse(mask, Zero(DFromV<V>()), no);
}

// ================================================== COMPARE

// mask = f(vector, vector)
#define HWY_SVE_COMPARE(BASE, CHAR, BITS, NAME, OP)                         \
  HWY_API svbool_t NAME(HWY_SVE_V(BASE, BITS) a, HWY_SVE_V(BASE, BITS) b) { \
    return sv##OP##_##CHAR##BITS(HWY_SVE_PTRUE(BITS), a, b);                \
  }
#define HWY_SVE_COMPARE_N(BASE, CHAR, BITS, NAME, OP)                       \
  HWY_API svbool_t NAME(HWY_SVE_V(BASE, BITS) a, HWY_SVE_T(BASE, BITS) b) { \
    return sv##OP##_##CHAR##BITS(HWY_SVE_PTRUE(BITS), a, b);                \
  }

// ------------------------------ Eq
HWY_SVE_FOREACH(HWY_SVE_COMPARE, Eq, cmpeq)

// ------------------------------ Ne
HWY_SVE_FOREACH(HWY_SVE_COMPARE, Ne, cmpne)

// ------------------------------ Lt
HWY_SVE_FOREACH_IF(HWY_SVE_COMPARE, Lt, cmplt)
namespace detail {
HWY_SVE_FOREACH_IF(HWY_SVE_COMPARE_N, LtN, cmplt_n)
}  // namespace detail

// ------------------------------ Le
HWY_SVE_FOREACH_F(HWY_SVE_COMPARE, Le, cmple)

#undef HWY_SVE_COMPARE
#undef HWY_SVE_COMPARE_N

// ------------------------------ Gt/Ge (swapped order)

template <class V>
HWY_API svbool_t Gt(const V a, const V b) {
  return Lt(b, a);
}
template <class V>
HWY_API svbool_t Ge(const V a, const V b) {
  return Le(b, a);
}

// ------------------------------ TestBit
template <class V>
HWY_API svbool_t TestBit(const V a, const V bit) {
  return Ne(And(a, bit), Zero(DFromV<V>()));
}

// ------------------------------ MaskFromVec (Ne)
template <class V>
HWY_API svbool_t MaskFromVec(const V v) {
  return Ne(v, Zero(DFromV<V>()));
}

// ------------------------------ VecFromMask

template <class D, HWY_IF_NOT_FLOAT_D(D)>
HWY_API VFromD<D> VecFromMask(const D d, svbool_t mask) {
  const auto v0 = Zero(RebindToSigned<decltype(d)>());
  return BitCast(d, detail::SubN(mask, v0, 1));
}

template <class D, HWY_IF_FLOAT_D(D)>
HWY_API VFromD<D> VecFromMask(const D d, svbool_t mask) {
  return BitCast(d, VecFromMask(RebindToUnsigned<D>(), mask));
}

// ================================================== MEMORY

// ------------------------------ Load/MaskedLoad/LoadDup128/Store/Stream

#define HWY_SVE_LOAD(BASE, CHAR, BITS, NAME, OP)           \
  template <size_t N>                                      \
  HWY_API HWY_SVE_V(BASE, BITS)                            \
      NAME(HWY_SVE_D(BASE, BITS, N) d,                     \
           const HWY_SVE_T(BASE, BITS) * HWY_RESTRICT p) { \
    return sv##OP##_##CHAR##BITS(detail::Mask(d), p);      \
  }

#define HWY_SVE_MASKED_LOAD(BASE, CHAR, BITS, NAME, OP)    \
  template <size_t N>                                      \
  HWY_API HWY_SVE_V(BASE, BITS)                            \
      NAME(svbool_t m, HWY_SVE_D(BASE, BITS, N) d,         \
           const HWY_SVE_T(BASE, BITS) * HWY_RESTRICT p) { \
    return sv##OP##_##CHAR##BITS(m, p);                    \
  }

#define HWY_SVE_LOAD_DUP128(BASE, CHAR, BITS, NAME, OP)    \
  template <size_t N>                                      \
  HWY_API HWY_SVE_V(BASE, BITS)                            \
      NAME(HWY_SVE_D(BASE, BITS, N) d,                     \
           const HWY_SVE_T(BASE, BITS) * HWY_RESTRICT p) { \
    /* All-true predicate to load all 128 bits. */         \
    return sv##OP##_##CHAR##BITS(HWY_SVE_PTRUE(8), p);     \
  }

#define HWY_SVE_STORE(BASE, CHAR, BITS, NAME, OP)                        \
  template <size_t N>                                                    \
  HWY_API void NAME(HWY_SVE_V(BASE, BITS) v, HWY_SVE_D(BASE, BITS, N) d, \
                    HWY_SVE_T(BASE, BITS) * HWY_RESTRICT p) {            \
    sv##OP##_##CHAR##BITS(detail::Mask(d), p, v);                        \
  }

HWY_SVE_FOREACH(HWY_SVE_LOAD, Load, ld1)
HWY_SVE_FOREACH(HWY_SVE_MASKED_LOAD, MaskedLoad, ld1)
HWY_SVE_FOREACH(HWY_SVE_LOAD_DUP128, LoadDup128, ld1rq)
HWY_SVE_FOREACH(HWY_SVE_STORE, Store, st1)
HWY_SVE_FOREACH(HWY_SVE_STORE, Stream, stnt1)

#undef HWY_SVE_LOAD
#undef HWY_SVE_MASKED_LOAD
#undef HWY_SVE_LOAD_DUP128
#undef HWY_SVE_STORE

// ------------------------------ Load/StoreU

// SVE only requires lane alignment, not natural alignment of the entire
// vector.
template <class D>
HWY_API VFromD<D> LoadU(D d, const TFromD<D>* HWY_RESTRICT p) {
  return Load(d, p);
}

template <class V, class D>
HWY_API void StoreU(const V v, D d, TFromD<D>* HWY_RESTRICT p) {
  Store(v, d, p);
}

// ------------------------------ ScatterOffset/Index

#define HWY_SVE_SCATTER_OFFSET(BASE, CHAR, BITS, NAME, OP)                   \
  template <size_t N>                                                        \
  HWY_API void NAME(HWY_SVE_V(BASE, BITS) v, HWY_SVE_D(BASE, BITS, N) d,     \
                    HWY_SVE_T(BASE, BITS) * HWY_RESTRICT base,               \
                    HWY_SVE_V(int, BITS) offset) {                           \
    sv##OP##_s##BITS##offset_##CHAR##BITS(detail::Mask(d), base, offset, v); \
  }

#define HWY_SVE_SCATTER_INDEX(BASE, CHAR, BITS, NAME, OP)                  \
  template <size_t N>                                                      \
  HWY_API void NAME(HWY_SVE_V(BASE, BITS) v, HWY_SVE_D(BASE, BITS, N) d,   \
                    HWY_SVE_T(BASE, BITS) * HWY_RESTRICT base,             \
                    HWY_SVE_V(int, BITS) index) {                          \
    sv##OP##_s##BITS##index_##CHAR##BITS(detail::Mask(d), base, index, v); \
  }

HWY_SVE_FOREACH_UIF3264(HWY_SVE_SCATTER_OFFSET, ScatterOffset, st1_scatter)
HWY_SVE_FOREACH_UIF3264(HWY_SVE_SCATTER_INDEX, ScatterIndex, st1_scatter)
#undef HWY_SVE_SCATTER_OFFSET
#undef HWY_SVE_SCATTER_INDEX

// ------------------------------ GatherOffset/Index

#define HWY_SVE_GATHER_OFFSET(BASE, CHAR, BITS, NAME, OP)               \
  template <size_t N>                                                   \
  HWY_API HWY_SVE_V(BASE, BITS)                                         \
      NAME(HWY_SVE_D(BASE, BITS, N) d,                                  \
           const HWY_SVE_T(BASE, BITS) * HWY_RESTRICT base,             \
           HWY_SVE_V(int, BITS) offset) {                               \
    return sv##OP##_s##BITS##offset_##CHAR##BITS(detail::Mask(d), base, \
                                                 offset);               \
  }
#define HWY_SVE_GATHER_INDEX(BASE, CHAR, BITS, NAME, OP)                       \
  template <size_t N>                                                          \
  HWY_API HWY_SVE_V(BASE, BITS)                                                \
      NAME(HWY_SVE_D(BASE, BITS, N) d,                                         \
           const HWY_SVE_T(BASE, BITS) * HWY_RESTRICT base,                    \
           HWY_SVE_V(int, BITS) index) {                                       \
    return sv##OP##_s##BITS##index_##CHAR##BITS(detail::Mask(d), base, index); \
  }

HWY_SVE_FOREACH_UIF3264(HWY_SVE_GATHER_OFFSET, GatherOffset, ld1_gather)
HWY_SVE_FOREACH_UIF3264(HWY_SVE_GATHER_INDEX, GatherIndex, ld1_gather)
#undef HWY_SVE_GATHER_OFFSET
#undef HWY_SVE_GATHER_INDEX

// ------------------------------ StoreInterleaved3

#define HWY_SVE_STORE3(BASE, CHAR, BITS, NAME, OP)                            \
  template <size_t N>                                                         \
  HWY_API void NAME(HWY_SVE_V(BASE, BITS) v0, HWY_SVE_V(BASE, BITS) v1,       \
                    HWY_SVE_V(BASE, BITS) v2, HWY_SVE_D(BASE, BITS, N) d,     \
                    HWY_SVE_T(BASE, BITS) * HWY_RESTRICT unaligned) {         \
    const sv##BASE##BITS##x3_t triple = svcreate3##_##CHAR##BITS(v0, v1, v2); \
    sv##OP##_##CHAR##BITS(detail::Mask(d), unaligned, triple);                \
  }
HWY_SVE_FOREACH_U08(HWY_SVE_STORE3, StoreInterleaved3, st3)

#undef HWY_SVE_STORE3

// ------------------------------ StoreInterleaved4

#define HWY_SVE_STORE4(BASE, CHAR, BITS, NAME, OP)                      \
  template <size_t N>                                                   \
  HWY_API void NAME(HWY_SVE_V(BASE, BITS) v0, HWY_SVE_V(BASE, BITS) v1, \
                    HWY_SVE_V(BASE, BITS) v2, HWY_SVE_V(BASE, BITS) v3, \
                    HWY_SVE_D(BASE, BITS, N) d,                         \
                    HWY_SVE_T(BASE, BITS) * HWY_RESTRICT unaligned) {   \
    const sv##BASE##BITS##x4_t quad =                                   \
        svcreate4##_##CHAR##BITS(v0, v1, v2, v3);                       \
    sv##OP##_##CHAR##BITS(detail::Mask(d), unaligned, quad);            \
  }
HWY_SVE_FOREACH_U08(HWY_SVE_STORE4, StoreInterleaved4, st4)

#undef HWY_SVE_STORE4

// ================================================== CONVERT

// ------------------------------ PromoteTo

// Same sign
#define HWY_SVE_PROMOTE_TO(BASE, CHAR, BITS, NAME, OP)        \
  template <size_t N>                                         \
  HWY_API HWY_SVE_V(BASE, BITS)                               \
      NAME(HWY_SVE_D(BASE, BITS, N) /* tag */,                \
           VFromD<Simd<MakeNarrow<HWY_SVE_T(BASE, BITS)>,     \
                       HWY_LANES(HWY_SVE_T(BASE, BITS)) * 2>> \
               v) {                                           \
    return sv##OP##_##CHAR##BITS(v);                          \
  }

HWY_SVE_FOREACH_UI16(HWY_SVE_PROMOTE_TO, PromoteTo, unpklo)
HWY_SVE_FOREACH_UI32(HWY_SVE_PROMOTE_TO, PromoteTo, unpklo)
HWY_SVE_FOREACH_UI64(HWY_SVE_PROMOTE_TO, PromoteTo, unpklo)

// 2x
template <size_t N>
HWY_API svuint32_t PromoteTo(Simd<uint32_t, N> dto, svuint8_t vfrom) {
  const RepartitionToWide<DFromV<decltype(vfrom)>> d2;
  return PromoteTo(dto, PromoteTo(d2, vfrom));
}
template <size_t N>
HWY_API svint32_t PromoteTo(Simd<int32_t, N> dto, svint8_t vfrom) {
  const RepartitionToWide<DFromV<decltype(vfrom)>> d2;
  return PromoteTo(dto, PromoteTo(d2, vfrom));
}
template <size_t N>
HWY_API svuint32_t U32FromU8(svuint8_t v) {
  return PromoteTo(Simd<uint32_t, N>(), v);
}

// Sign change
template <size_t N>
HWY_API svint16_t PromoteTo(Simd<int16_t, N> dto, svuint8_t vfrom) {
  const RebindToUnsigned<decltype(dto)> du;
  return BitCast(dto, PromoteTo(du, vfrom));
}
template <size_t N>
HWY_API svint32_t PromoteTo(Simd<int32_t, N> dto, svuint16_t vfrom) {
  const RebindToUnsigned<decltype(dto)> du;
  return BitCast(dto, PromoteTo(du, vfrom));
}
template <size_t N>
HWY_API svint32_t PromoteTo(Simd<int32_t, N> dto, svuint8_t vfrom) {
  const Repartition<uint16_t, DFromV<decltype(vfrom)>> du16;
  const Repartition<int16_t, decltype(du16)> di16;
  return PromoteTo(dto, BitCast(di16, PromoteTo(du16, vfrom)));
}

// ------------------------------ PromoteTo F

template <size_t N>
HWY_API svfloat32_t PromoteTo(Simd<float32_t, N> /* d */, const svfloat16_t v) {
  return svcvt_f32_f16_x(detail::PTrue(Simd<float16_t, N>()), v);
}

template <size_t N>
HWY_API svfloat64_t PromoteTo(Simd<float64_t, N> /* d */, const svfloat32_t v) {
  return svcvt_f64_f32_x(detail::PTrue(Simd<float32_t, N>()), v);
}

template <size_t N>
HWY_API svfloat64_t PromoteTo(Simd<float64_t, N> /* d */, const svint32_t v) {
  return svcvt_f64_s32_x(detail::PTrue(Simd<int32_t, N>()), v);
}

// For 16-bit Compress
namespace detail {
HWY_SVE_FOREACH_UI32(HWY_SVE_PROMOTE_TO, PromoteUpperTo, unpkhi)
#undef HWY_SVE_PROMOTE_TO

template <size_t N>
HWY_API svfloat32_t PromoteUpperTo(Simd<float, N> df, const svfloat16_t v) {
  const RebindToUnsigned<decltype(df)> du;
  const RepartitionToNarrow<decltype(du)> dn;
  return BitCast(df, PromoteUpperTo(du, BitCast(dn, v)));
}

}  // namespace detail

// ------------------------------ DemoteTo U

namespace detail {

// Saturates unsigned vectors to half/quarter-width TN.
template <typename TN, class VU>
VU SaturateU(VU v) {
  return detail::MinN(v, static_cast<TFromV<VU>>(LimitsMax<TN>()));
}

// Saturates unsigned vectors to half/quarter-width TN.
template <typename TN, class VI>
VI SaturateI(VI v) {
  const DFromV<VI> di;
  return detail::MinN(detail::MaxN(v, LimitsMin<TN>()), LimitsMax<TN>());
}

}  // namespace detail

template <size_t N>
HWY_API svuint8_t DemoteTo(Simd<uint8_t, N> dn, const svint16_t v) {
  const DFromV<decltype(v)> di;
  const RebindToUnsigned<decltype(di)> du;
  using TN = TFromD<decltype(dn)>;
  // First clamp negative numbers to zero and cast to unsigned.
  const svuint16_t clamped = BitCast(du, Max(Zero(di), v));
  // Saturate to unsigned-max and halve the width.
  const svuint8_t vn = BitCast(dn, detail::SaturateU<TN>(clamped));
  return svuzp1_u8(vn, vn);
}

template <size_t N>
HWY_API svuint16_t DemoteTo(Simd<uint16_t, N> dn, const svint32_t v) {
  const DFromV<decltype(v)> di;
  const RebindToUnsigned<decltype(di)> du;
  using TN = TFromD<decltype(dn)>;
  // First clamp negative numbers to zero and cast to unsigned.
  const svuint32_t clamped = BitCast(du, Max(Zero(di), v));
  // Saturate to unsigned-max and halve the width.
  const svuint16_t vn = BitCast(dn, detail::SaturateU<TN>(clamped));
  return svuzp1_u16(vn, vn);
}

template <size_t N>
HWY_API svuint8_t DemoteTo(Simd<uint8_t, N> dn, const svint32_t v) {
  const DFromV<decltype(v)> di;
  const RebindToUnsigned<decltype(di)> du;
  const RepartitionToNarrow<decltype(du)> d2;
  using TN = TFromD<decltype(dn)>;
  // First clamp negative numbers to zero and cast to unsigned.
  const svuint32_t clamped = BitCast(du, Max(Zero(di), v));
  // Saturate to unsigned-max and quarter the width.
  const svuint16_t cast16 = BitCast(d2, detail::SaturateU<TN>(clamped));
  const svuint8_t x2 = BitCast(dn, svuzp1_u16(cast16, cast16));
  return svuzp1_u8(x2, x2);
}

HWY_API svuint8_t U8FromU32(const svuint32_t v) {
  const DFromV<svuint32_t> du32;
  const RepartitionToNarrow<decltype(du32)> du16;
  const RepartitionToNarrow<decltype(du16)> du8;

  const svuint16_t cast16 = BitCast(du16, v);
  const svuint16_t x2 = svuzp1_u16(cast16, cast16);
  const svuint8_t cast8 = BitCast(du8, x2);
  return svuzp1_u8(cast8, cast8);
}

// ------------------------------ DemoteTo I

template <size_t N>
HWY_API svint8_t DemoteTo(Simd<int8_t, N> dn, const svint16_t v) {
  const DFromV<decltype(v)> di;
  using TN = TFromD<decltype(dn)>;
#if HWY_TARGET == HWY_SVE2
  const svint8_t vn = BitCast(dn, svqxtnb_s16(v));
#else
  const svint8_t vn = BitCast(dn, detail::SaturateI<TN>(v));
#endif
  return svuzp1_s8(vn, vn);
}

template <size_t N>
HWY_API svint16_t DemoteTo(Simd<int16_t, N> dn, const svint32_t v) {
  const DFromV<decltype(v)> di;
  using TN = TFromD<decltype(dn)>;
#if HWY_TARGET == HWY_SVE2
  const svint16_t vn = BitCast(dn, svqxtnb_s32(v));
#else
  const svint16_t vn = BitCast(dn, detail::SaturateI<TN>(v));
#endif
  return svuzp1_s16(vn, vn);
}

template <size_t N>
HWY_API svint8_t DemoteTo(Simd<int8_t, N> dn, const svint32_t v) {
  const DFromV<decltype(v)> di;
  using TN = TFromD<decltype(dn)>;
  const RepartitionToWide<decltype(dn)> d2;
#if HWY_TARGET == HWY_SVE2
  const svint16_t cast16 = BitCast(d2, svqxtnb_s16(svqxtnb_s32(v)));
#else
  const svint16_t cast16 = BitCast(d2, detail::SaturateI<TN>(v));
#endif
  const svint8_t v2 = BitCast(dn, svuzp1_s16(cast16, cast16));
  return BitCast(dn, svuzp1_s8(v2, v2));
}

// ------------------------------ DemoteTo F

template <size_t N>
HWY_API svfloat16_t DemoteTo(Simd<float16_t, N> d, const svfloat32_t v) {
  return svcvt_f16_f32_x(detail::PTrue(d), v);
}

template <size_t N>
HWY_API svfloat32_t DemoteTo(Simd<float32_t, N> d, const svfloat64_t v) {
  return svcvt_f32_f64_x(detail::PTrue(d), v);
}

template <size_t N>
HWY_API svint32_t DemoteTo(Simd<int32_t, N> d, const svfloat64_t v) {
  return svcvt_s32_f64_x(detail::PTrue(d), v);
}

// ------------------------------ ConvertTo F

#define HWY_SVE_CONVERT(BASE, CHAR, BITS, NAME, OP)                     \
  template <size_t N>                                                   \
  HWY_API HWY_SVE_V(BASE, BITS)                                         \
      NAME(HWY_SVE_D(BASE, BITS, N) /* d */, HWY_SVE_V(int, BITS) v) {  \
    return sv##OP##_##CHAR##BITS##_s##BITS##_x(HWY_SVE_PTRUE(BITS), v); \
  }                                                                     \
  /* Truncates (rounds toward zero). */                                 \
  template <size_t N>                                                   \
  HWY_API HWY_SVE_V(int, BITS)                                          \
      NAME(HWY_SVE_D(int, BITS, N) /* d */, HWY_SVE_V(BASE, BITS) v) {  \
    return sv##OP##_s##BITS##_##CHAR##BITS##_x(HWY_SVE_PTRUE(BITS), v); \
  }

// API only requires f32 but we provide f64 for use by Iota.
HWY_SVE_FOREACH_F(HWY_SVE_CONVERT, ConvertTo, cvt)
#undef HWY_SVE_CONVERT

// ------------------------------ NearestInt (Round, ConvertTo)

template <class VF, class DI = RebindToSigned<DFromV<VF>>>
HWY_API VFromD<DI> NearestInt(VF v) {
  // No single instruction, round then truncate.
  return ConvertTo(DI(), Round(v));
}

// ------------------------------ Iota (Add, ConvertTo)

#define HWY_SVE_IOTA(BASE, CHAR, BITS, NAME, OP)                      \
  template <size_t N>                                                 \
  HWY_API HWY_SVE_V(BASE, BITS)                                       \
      NAME(HWY_SVE_D(BASE, BITS, N) d, HWY_SVE_T(BASE, BITS) first) { \
    return sv##OP##_##CHAR##BITS(first, 1);                           \
  }

HWY_SVE_FOREACH_UI(HWY_SVE_IOTA, Iota, index)
#undef HWY_SVE_IOTA

template <class D, HWY_IF_FLOAT_D(D)>
HWY_API VFromD<D> Iota(const D d, TFromD<D> first) {
  const RebindToSigned<D> di;
  return detail::AddN(ConvertTo(d, Iota(di, 0)), first);
}

// ================================================== COMBINE

namespace detail {

template <typename T, size_t N>
svbool_t MaskLowerHalf(Simd<T, N> d) {
  return FirstN(d, Lanes(d) / 2);
}
template <typename T, size_t N>
svbool_t MaskUpperHalf(Simd<T, N> d) {
  // For Splice to work as intended, make sure bits above Lanes(d) are zero.
  return AndNot(MaskLowerHalf(d), detail::Mask(d));
}

// Right-shift vector pair by constexpr; can be used to slide down (=N) or up
// (=Lanes()-N).
#define HWY_SVE_EXT(BASE, CHAR, BITS, NAME, OP)                  \
  template <size_t kIndex>                                       \
  HWY_API HWY_SVE_V(BASE, BITS)                                  \
      NAME(HWY_SVE_V(BASE, BITS) hi, HWY_SVE_V(BASE, BITS) lo) { \
    return sv##OP##_##CHAR##BITS(lo, hi, kIndex);                \
  }
HWY_SVE_FOREACH(HWY_SVE_EXT, Ext, ext)
#undef HWY_SVE_EXT

// Used to slide up / shift whole register left; mask indicates which range
// to take from lo, and the rest is filled from hi starting at its lowest.
#define HWY_SVE_SPLICE(BASE, CHAR, BITS, NAME, OP)                         \
  HWY_API HWY_SVE_V(BASE, BITS) NAME(                                      \
      HWY_SVE_V(BASE, BITS) hi, HWY_SVE_V(BASE, BITS) lo, svbool_t mask) { \
    return sv##OP##_##CHAR##BITS(mask, lo, hi);                            \
  }
HWY_SVE_FOREACH(HWY_SVE_SPLICE, Splice, splice)
#undef HWY_SVE_SPLICE

}  // namespace detail

// ------------------------------ ConcatUpperLower
template <class D, class V>
HWY_API V ConcatUpperLower(const D d, const V hi, const V lo) {
  return IfThenElse(detail::MaskLowerHalf(d), lo, hi);
}

// ------------------------------ ConcatLowerLower
template <class D, class V>
HWY_API V ConcatLowerLower(const D d, const V hi, const V lo) {
  return detail::Splice(hi, lo, detail::MaskLowerHalf(d));
}

// ------------------------------ ConcatLowerUpper
template <class D, class V>
HWY_API V ConcatLowerUpper(const D d, const V hi, const V lo) {
  return detail::Splice(hi, lo, detail::MaskUpperHalf(d));
}

// ------------------------------ ConcatUpperUpper
template <class D, class V>
HWY_API V ConcatUpperUpper(const D d, const V hi, const V lo) {
  const svbool_t mask_upper = detail::MaskUpperHalf(d);
  const V lo_upper = detail::Splice(lo, lo, mask_upper);
  return IfThenElse(mask_upper, hi, lo_upper);
}

// ------------------------------ Combine
template <class D, class V2>
HWY_API VFromD<D> Combine(const D d, const V2 hi, const V2 lo) {
  return ConcatLowerLower(d, hi, lo);
}

// ------------------------------ ZeroExtendVector

template <class D, class V>
HWY_API V ZeroExtendVector(const D d, const V lo) {
  return Combine(d, Zero(Half<D>()), lo);
}

// ------------------------------ Lower/UpperHalf

template <class D2, class V>
HWY_API V LowerHalf(D2 /* tag */, const V v) {
  return v;
}

template <class V>
HWY_API V LowerHalf(const V v) {
  return v;
}

template <class D2, class V>
HWY_API V UpperHalf(const D2 d2, const V v) {
  return detail::Splice(v, v, detail::MaskUpperHalf(Twice<D2>()));
}

// ================================================== SWIZZLE

// ------------------------------ GetLane

#define HWY_SVE_GET_LANE(BASE, CHAR, BITS, NAME, OP)            \
  HWY_API HWY_SVE_T(BASE, BITS) NAME(HWY_SVE_V(BASE, BITS) v) { \
    return sv##OP##_##CHAR##BITS(detail::PFalse(), v);          \
  }

HWY_SVE_FOREACH(HWY_SVE_GET_LANE, GetLane, lasta)
#undef HWY_SVE_GET_LANE

// ------------------------------ OddEven

namespace detail {
HWY_SVE_FOREACH(HWY_SVE_RETV_ARGVN, Insert, insr_n)
HWY_SVE_FOREACH(HWY_SVE_RETV_ARGVV, InterleaveEven, trn1)
HWY_SVE_FOREACH(HWY_SVE_RETV_ARGVV, InterleaveOdd, trn2)
}  // namespace detail

template <class V>
HWY_API V OddEven(const V odd, const V even) {
  const auto even_in_odd = detail::Insert(even, 0);
  return detail::InterleaveOdd(even_in_odd, odd);
}

// ------------------------------ TableLookupLanes

template <class D, class DI = RebindToSigned<D>>
HWY_API VFromD<DI> SetTableIndices(D d, const TFromD<DI>* idx) {
#if HWY_IS_DEBUG_BUILD
  const size_t N = Lanes(d);
  for (size_t i = 0; i < N; ++i) {
    HWY_DASSERT(0 <= idx[i] && idx[i] < static_cast<TFromD<DI>>(N));
  }
#else
  (void)d;
#endif
  return Load(DI(), idx);
}

// <32bit are not part of Highway API, but used in Broadcast.
#define HWY_SVE_TABLE(BASE, CHAR, BITS, NAME, OP)                             \
  HWY_API HWY_SVE_V(BASE, BITS)                                               \
      NAME(HWY_SVE_V(BASE, BITS) v, HWY_SVE_V(int, BITS) idx) {               \
    const auto idx_u = BitCast(RebindToUnsigned<DFromV<decltype(v)>>(), idx); \
    return sv##OP##_##CHAR##BITS(v, idx_u);                                   \
  }

HWY_SVE_FOREACH(HWY_SVE_TABLE, TableLookupLanes, tbl)
#undef HWY_SVE_TABLE

// ------------------------------ Compress (PromoteTo)

namespace detail {

#define HWY_SVE_CONCAT_EVERY_SECOND(BASE, CHAR, BITS, NAME, OP)  \
  HWY_API HWY_SVE_V(BASE, BITS)                                  \
      NAME(HWY_SVE_V(BASE, BITS) hi, HWY_SVE_V(BASE, BITS) lo) { \
    return sv##OP##_##CHAR##BITS(lo, hi);                        \
  }
HWY_SVE_FOREACH(HWY_SVE_CONCAT_EVERY_SECOND, ConcatEven, uzp1)
HWY_SVE_FOREACH(HWY_SVE_CONCAT_EVERY_SECOND, ConcatOdd, uzp2)
#undef HWY_SVE_CONCAT_EVERY_SECOND

}  // namespace detail

#define HWY_SVE_COMPRESS(BASE, CHAR, BITS, NAME, OP)                           \
  HWY_API HWY_SVE_V(BASE, BITS) NAME(HWY_SVE_V(BASE, BITS) v, svbool_t mask) { \
    return sv##OP##_##CHAR##BITS(mask, v);                                     \
  }

HWY_SVE_FOREACH_UIF3264(HWY_SVE_COMPRESS, Compress, compact)
#undef HWY_SVE_COMPRESS

template <class V, HWY_IF_LANE_SIZE_V(V, 2)>
HWY_API V Compress(V v, svbool_t mask16) {
  static_assert(!IsSame<V, svfloat16_t>(), "Must use overload");
  const DFromV<V> d16;

  // Promote vector and mask to 32-bit
  const RepartitionToWide<decltype(d16)> dw;
  const auto v32L = PromoteTo(dw, v);
  const auto v32H = detail::PromoteUpperTo(dw, v);
  const svbool_t mask32L = svunpklo_b(mask16);
  const svbool_t mask32H = svunpkhi_b(mask16);

  const auto compressedL = Compress(v32L, mask32L);
  const auto compressedH = Compress(v32H, mask32H);

  // Demote to 16-bit (already in range) - separately so we can splice
  const V evenL = BitCast(d16, compressedL);
  const V evenH = BitCast(d16, compressedH);
  const V v16L = detail::ConcatEven(evenL, evenL);
  const V v16H = detail::ConcatEven(evenH, evenH);

  // We need to combine two vectors of non-constexpr length, so the only option
  // is Splice, which requires us to synthesize a mask. NOTE: this function uses
  // full vectors (SV_ALL instead of SV_POW2), hence we need unmasked svcnt.
  const size_t countL = detail::CountTrueFull(dw, mask32L);
  const auto compressed_maskL = FirstN(d16, countL);
  return detail::Splice(v16H, v16L, compressed_maskL);
}

// Must treat float16_t as integers so we can ConcatEven.
HWY_API svfloat16_t Compress(svfloat16_t v, svbool_t mask16) {
  const DFromV<decltype(v)> df;
  const RebindToSigned<decltype(df)> di;
  return BitCast(df, Compress(BitCast(di, v), mask16));
}

// ------------------------------ CompressStore

template <class V, class M, class D>
HWY_API size_t CompressStore(const V v, const M mask, const D d,
                             TFromD<D>* HWY_RESTRICT unaligned) {
  StoreU(Compress(v, mask), d, unaligned);
  return CountTrue(d, mask);
}

// ================================================== BLOCKWISE

// ------------------------------ CombineShiftRightBytes

namespace detail {

// For x86-compatible behaviour mandated by Highway API: TableLookupBytes
// offsets are implicitly relative to the start of their 128-bit block.
template <typename T, size_t N>
constexpr size_t LanesPerBlock(Simd<T, N> /* tag */) {
  // We might have a capped vector smaller than a block, so honor that.
  return HWY_MIN(16 / sizeof(T), N);
}

template <class D, class V>
HWY_INLINE V OffsetsOf128BitBlocks(const D d, const V iota0) {
  using T = MakeUnsigned<TFromD<D>>;
  return detail::AndNotN(static_cast<T>(LanesPerBlock(d) - 1), iota0);
}

template <size_t kLanes, class D>
svbool_t FirstNPerBlock(D d) {
  const RebindToSigned<D> di;
  constexpr size_t kLanesPerBlock = detail::LanesPerBlock(di);
  const auto idx_mod = detail::AndN(Iota(di, 0), kLanesPerBlock - 1);
  return detail::LtN(BitCast(di, idx_mod), kLanes);
}

}  // namespace detail

template <size_t kBytes, class D, class V = VFromD<D>>
HWY_API V CombineShiftRightBytes(const D d, const V hi, const V lo) {
  const Repartition<uint8_t, decltype(d)> d8;
  const auto hi8 = BitCast(d8, hi);
  const auto lo8 = BitCast(d8, lo);
  const auto hi_up = detail::Splice(hi8, hi8, FirstN(d8, 16 - kBytes));
  const auto lo_down = detail::Ext<kBytes>(lo8, lo8);
  const svbool_t is_lo = detail::FirstNPerBlock<16 - kBytes>(d8);
  return BitCast(d, IfThenElse(is_lo, lo_down, hi_up));
}

// ------------------------------ Shuffle2301

#define HWY_SVE_SHUFFLE_2301(BASE, CHAR, BITS, NAME, OP)                      \
  HWY_API HWY_SVE_V(BASE, BITS) NAME(HWY_SVE_V(BASE, BITS) v) {               \
    const DFromV<decltype(v)> d;                                              \
    const svuint64_t vu64 = BitCast(Repartition<uint64_t, decltype(d)>(), v); \
    return BitCast(d, sv##OP##_u64_x(HWY_SVE_PTRUE(64), vu64));               \
  }

HWY_SVE_FOREACH_UI32(HWY_SVE_SHUFFLE_2301, Shuffle2301, revw)
#undef HWY_SVE_SHUFFLE_2301

template <class V, HWY_IF_FLOAT_V(V)>
HWY_API V Shuffle2301(const V v) {
  const DFromV<V> df;
  const RebindToUnsigned<decltype(df)> du;
  return BitCast(df, Shuffle2301(BitCast(du, v)));
}

// ------------------------------ Shuffle2103
template <class V>
HWY_API V Shuffle2103(const V v) {
  const DFromV<V> d;
  const Repartition<uint8_t, decltype(d)> d8;
  static_assert(sizeof(TFromD<decltype(d)>) == 4, "Defined for 32-bit types");
  const svuint8_t v8 = BitCast(d8, v);
  return BitCast(d, CombineShiftRightBytes<12>(d8, v8, v8));
}

// ------------------------------ Shuffle0321
template <class V>
HWY_API V Shuffle0321(const V v) {
  const DFromV<V> d;
  const Repartition<uint8_t, decltype(d)> d8;
  static_assert(sizeof(TFromD<decltype(d)>) == 4, "Defined for 32-bit types");
  const svuint8_t v8 = BitCast(d8, v);
  return BitCast(d, CombineShiftRightBytes<4>(d8, v8, v8));
}

// ------------------------------ Shuffle1032
template <class V>
HWY_API V Shuffle1032(const V v) {
  const DFromV<V> d;
  const Repartition<uint8_t, decltype(d)> d8;
  static_assert(sizeof(TFromD<decltype(d)>) == 4, "Defined for 32-bit types");
  const svuint8_t v8 = BitCast(d8, v);
  return BitCast(d, CombineShiftRightBytes<8>(d8, v8, v8));
}

// ------------------------------ Shuffle01
template <class V>
HWY_API V Shuffle01(const V v) {
  const DFromV<V> d;
  const Repartition<uint8_t, decltype(d)> d8;
  static_assert(sizeof(TFromD<decltype(d)>) == 8, "Defined for 64-bit types");
  const svuint8_t v8 = BitCast(d8, v);
  return BitCast(d, CombineShiftRightBytes<8>(d8, v8, v8));
}

// ------------------------------ Shuffle0123
template <class V>
HWY_API V Shuffle0123(const V v) {
  return Shuffle2301(Shuffle1032(v));
}

// ------------------------------ TableLookupBytes

template <class V, class VI>
HWY_API VI TableLookupBytes(const V v, const VI idx) {
  const DFromV<VI> d;
  const Repartition<uint8_t, decltype(d)> du8;
  const Repartition<int8_t, decltype(d)> di8;
  const auto offsets128 = detail::OffsetsOf128BitBlocks(du8, Iota(du8, 0));
  const auto idx8 = BitCast(di8, Add(BitCast(du8, idx), offsets128));
  return BitCast(d, TableLookupLanes(BitCast(du8, v), idx8));
}

template <class V, class VI>
HWY_API VI TableLookupBytesOr0(const V v, const VI idx) {
  const DFromV<VI> d;
  // Mask size must match vector type, so cast everything to this type.
  const Repartition<int8_t, decltype(d)> di8;

  auto idx8 = BitCast(di8, idx);
  const auto msb = Lt(idx8, Zero(di8));
// Prevent overflow in table lookups (unnecessary if native)
#if defined(HWY_EMULATE_SVE)
  idx8 = IfThenZeroElse(msb, idx8);
#endif

  const auto lookup = TableLookupBytes(BitCast(di8, v), idx8);
  return BitCast(d, IfThenZeroElse(msb, lookup));
}

// ------------------------------ Broadcast

template <int kLane, class V>
HWY_API V Broadcast(const V v) {
  const DFromV<V> d;
  const RebindToSigned<decltype(d)> di;
  constexpr size_t kLanesPerBlock = detail::LanesPerBlock(di);
  static_assert(0 <= kLane && kLane < kLanesPerBlock, "Invalid lane");
  auto idx = detail::OffsetsOf128BitBlocks(di, Iota(di, 0));
  if (kLane != 0) {
    idx = detail::AddN(idx, kLane);
  }
  return TableLookupLanes(v, idx);
}

// ------------------------------ ShiftLeftLanes

template <size_t kLanes, class D, class V = VFromD<D>>
HWY_API V ShiftLeftLanes(D d, const V v) {
  const RebindToSigned<decltype(d)> di;
  const auto zero = Zero(d);
  const auto shifted = detail::Splice(v, zero, FirstN(d, kLanes));
  // Match x86 semantics by zeroing lower lanes in 128-bit blocks
  return IfThenElse(detail::FirstNPerBlock<kLanes>(d), zero, shifted);
}

template <size_t kLanes, class V>
HWY_API V ShiftLeftLanes(const V v) {
  return ShiftLeftLanes<kLanes>(DFromV<V>(), v);
}

// ------------------------------ ShiftRightLanes
template <size_t kLanes, typename T, size_t N, class V = VFromD<Simd<T, N>>>
HWY_API V ShiftRightLanes(Simd<T, N> d, V v) {
  const RebindToSigned<decltype(d)> di;
  // For partial vectors, clear upper lanes so we shift in zeros.
  if (N != HWY_LANES(T)) {
    v = IfThenElseZero(detail::Mask(d), v);
  }

  const auto shifted = detail::Ext<kLanes>(v, v);
  // Match x86 semantics by zeroing upper lanes in 128-bit blocks
  constexpr size_t kLanesPerBlock = detail::LanesPerBlock(d);
  const svbool_t mask = detail::FirstNPerBlock<kLanesPerBlock - kLanes>(d);
  return IfThenElseZero(mask, shifted);
}

// ------------------------------ ShiftLeftBytes

template <int kBytes, class D, class V = VFromD<D>>
HWY_API V ShiftLeftBytes(const D d, const V v) {
  const Repartition<uint8_t, decltype(d)> d8;
  return BitCast(d, ShiftLeftLanes<kBytes>(BitCast(d8, v)));
}

template <int kBytes, class V>
HWY_API V ShiftLeftBytes(const V v) {
  return ShiftLeftBytes<kBytes>(DFromV<V>(), v);
}

// ------------------------------ ShiftRightBytes
template <int kBytes, class D, class V = VFromD<D>>
HWY_API V ShiftRightBytes(const D d, const V v) {
  const Repartition<uint8_t, decltype(d)> d8;
  return BitCast(d, ShiftRightLanes<kBytes>(d8, BitCast(d8, v)));
}

// ------------------------------ InterleaveLower

namespace detail {
HWY_SVE_FOREACH(HWY_SVE_RETV_ARGVV, ZipLower, zip1)
}  // namespace detail

template <class D, class V>
HWY_API V InterleaveLower(D d, const V a, const V b) {
  static_assert(IsSame<TFromD<D>, TFromV<V>>(), "D/V mismatch");
  // Move lower halves of blocks to lower half of vector.
  const Repartition<uint64_t, decltype(d)> d64;
  const auto a64 = BitCast(d64, a);
  const auto b64 = BitCast(d64, b);
  const auto a_blocks = detail::ConcatEven(a64, a64);
  const auto b_blocks = detail::ConcatEven(b64, b64);

  return detail::ZipLower(BitCast(d, a_blocks), BitCast(d, b_blocks));
}

template <class V>
HWY_API V InterleaveLower(const V a, const V b) {
  return InterleaveLower(DFromV<V>(), a, b);
}

// ------------------------------ InterleaveUpper

// Full vector: guaranteed to have at least one block
template <typename T, class V = VFromD<Full<T>>>
HWY_API V InterleaveUpper(Simd<T, HWY_LANES(T)> d, const V a, const V b) {
  // Move upper halves of blocks to lower half of vector.
  const Repartition<uint64_t, decltype(d)> d64;
  const auto a64 = BitCast(d64, a);
  const auto b64 = BitCast(d64, b);
  const auto a_blocks = detail::ConcatOdd(a64, a64);
  const auto b_blocks = detail::ConcatOdd(b64, b64);
  return detail::ZipLower(BitCast(d, a_blocks), BitCast(d, b_blocks));
}

// Capped: less than one block
template <typename T, size_t N, HWY_IF_LE64(T, N), class V = VFromD<Simd<T, N>>>
HWY_API V InterleaveUpper(Simd<T, N> d, const V a, const V b) {
  static_assert(IsSame<T, TFromV<V>>(), "D/V mismatch");
  const Half<decltype(d)> d2;
  return InterleaveLower(d, UpperHalf(d2, a), UpperHalf(d2, b));
}

// Partial: need runtime check
template <typename T, size_t N,
          hwy::EnableIf<(N < HWY_LANES(T) && N * sizeof(T) >= 16)>* = nullptr,
          class V = VFromD<Simd<T, N>>>
HWY_API V InterleaveUpper(Simd<T, N> d, const V a, const V b) {
  static_assert(IsSame<T, TFromV<V>>(), "D/V mismatch");
  // Less than one block: treat as capped
  if (Lanes(d) * sizeof(T) < 16) {
    const Half<decltype(d)> d2;
    return InterleaveLower(d, UpperHalf(d2, a), UpperHalf(d2, b));
  }
  return InterleaveUpper(Full<T>(), a, b);
}

// ------------------------------ ZipLower

template <class V, class DW = RepartitionToWide<DFromV<V>>>
HWY_API VFromD<DW> ZipLower(DW dw, V a, V b) {
  const RepartitionToNarrow<DW> dn;
  static_assert(IsSame<TFromD<decltype(dn)>, TFromV<V>>(), "D/V mismatch");
  return BitCast(dw, InterleaveLower(dn, a, b));
}
template <class V, class D = DFromV<V>, class DW = RepartitionToWide<D>>
HWY_API VFromD<DW> ZipLower(const V a, const V b) {
  return BitCast(DW(), InterleaveLower(D(), a, b));
}

// ------------------------------ ZipUpper
template <class V, class DW = RepartitionToWide<DFromV<V>>>
HWY_API VFromD<DW> ZipUpper(DW dw, V a, V b) {
  const RepartitionToNarrow<DW> dn;
  static_assert(IsSame<TFromD<decltype(dn)>, TFromV<V>>(), "D/V mismatch");
  return BitCast(dw, InterleaveUpper(dn, a, b));
}

// ================================================== REDUCE

#define HWY_SVE_REDUCE(BASE, CHAR, BITS, NAME, OP)                \
  template <size_t N>                                             \
  HWY_API HWY_SVE_V(BASE, BITS)                                   \
      NAME(HWY_SVE_D(BASE, BITS, N) d, HWY_SVE_V(BASE, BITS) v) { \
    return Set(d, sv##OP##_##CHAR##BITS(detail::Mask(d), v));     \
  }

HWY_SVE_FOREACH(HWY_SVE_REDUCE, SumOfLanes, addv)
HWY_SVE_FOREACH_UI(HWY_SVE_REDUCE, MinOfLanes, minv)
HWY_SVE_FOREACH_UI(HWY_SVE_REDUCE, MaxOfLanes, maxv)
// NaN if all are
HWY_SVE_FOREACH_F(HWY_SVE_REDUCE, MinOfLanes, minnmv)
HWY_SVE_FOREACH_F(HWY_SVE_REDUCE, MaxOfLanes, maxnmv)

#undef HWY_SVE_REDUCE

// ================================================== Ops with dependencies

// ------------------------------ ZeroIfNegative (Lt, IfThenElse)
template <class V>
HWY_API V ZeroIfNegative(const V v) {
  const auto v0 = Zero(DFromV<V>());
  // We already have a zero constant, so avoid IfThenZeroElse.
  return IfThenElse(Lt(v, v0), v0, v);
}

// ------------------------------ BroadcastSignBit (ShiftRight)
template <class V>
HWY_API V BroadcastSignBit(const V v) {
  return ShiftRight<sizeof(TFromV<V>) * 8 - 1>(v);
}

// ------------------------------ AverageRound (ShiftRight)

#if HWY_TARGET == HWY_SVE2
HWY_SVE_FOREACH_U08(HWY_SVE_RETV_ARGPVV, AverageRound, rhadd)
HWY_SVE_FOREACH_U16(HWY_SVE_RETV_ARGPVV, AverageRound, rhadd)
#else
template <class V>
V AverageRound(const V a, const V b) {
  return ShiftRight<1>(Add(Add(a, b), Set(DFromV<V>(), 1)));
}
#endif  // HWY_TARGET == HWY_SVE2

// ------------------------------ LoadMaskBits (TestBit)

// `p` points to at least 8 readable bytes, not all of which need be valid.
template <class D, HWY_IF_LANE_SIZE_D(D, 1)>
HWY_INLINE svbool_t LoadMaskBits(D d, const uint8_t* HWY_RESTRICT bits) {
  const RebindToUnsigned<D> du;
  const svuint8_t iota = Iota(du, 0);

  // Load correct number of bytes (bits/8) with 7 zeros after each.
  const svuint8_t bytes = BitCast(du, svld1ub_u64(detail::PTrue(d), bits));
  // Replicate bytes 8x such that each byte contains the bit that governs it.
  const svuint8_t rep8 = svtbl_u8(bytes, detail::AndNotN(7, iota));

  // 1, 2, 4, 8, 16, 32, 64, 128,  1, 2 ..
  const svuint8_t bit = Shl(Set(du, 1), detail::AndN(iota, 7));

  return TestBit(rep8, bit);
}

template <class D, HWY_IF_LANE_SIZE_D(D, 2)>
HWY_INLINE svbool_t LoadMaskBits(D /* tag */,
                                 const uint8_t* HWY_RESTRICT bits) {
  const RebindToUnsigned<D> du;
  const Repartition<uint8_t, D> du8;

  // There may be up to 128 bits; avoid reading past the end.
  const svuint8_t bytes = svld1(FirstN(du8, (Lanes(du) + 7) / 8), bits);

  // Replicate bytes 16x such that each lane contains the bit that governs it.
  const svuint8_t rep16 = svtbl_u8(bytes, ShiftRight<4>(Iota(du8, 0)));

  // 1, 2, 4, 8, 16, 32, 64, 128,  1, 2 ..
  const svuint16_t bit = Shl(Set(du, 1), detail::AndN(Iota(du, 0), 7));

  return TestBit(BitCast(du, rep16), bit);
}

template <class D, HWY_IF_LANE_SIZE_D(D, 4)>
HWY_INLINE svbool_t LoadMaskBits(D /* tag */,
                                 const uint8_t* HWY_RESTRICT bits) {
  const RebindToUnsigned<D> du;
  const Repartition<uint8_t, D> du8;

  // Upper bound = 2048 bits / 32 bit = 64 bits; at least 8 bytes are readable,
  // so we can skip computing the actual length (Lanes(du)+7)/8.
  const svuint8_t bytes = svld1(FirstN(du8, 8), bits);

  // Replicate bytes 32x such that each lane contains the bit that governs it.
  const svuint8_t rep32 = svtbl_u8(bytes, ShiftRight<5>(Iota(du8, 0)));

  // 1, 2, 4, 8, 16, 32, 64, 128,  1, 2 ..
  const svuint32_t bit = Shl(Set(du, 1), detail::AndN(Iota(du, 0), 7));

  return TestBit(BitCast(du, rep32), bit);
}

template <class D, HWY_IF_LANE_SIZE_D(D, 8)>
HWY_INLINE svbool_t LoadMaskBits(D /* tag */,
                                 const uint8_t* HWY_RESTRICT bits) {
  const RebindToUnsigned<D> du;

  // Max 2048 bits = 32 lanes = 32 input bits; replicate those into each lane.
  // The "at least 8 byte" guarantee in quick_reference ensures this is safe.
  uint32_t mask_bits;
  CopyBytes<4>(bits, &mask_bits);
  const auto vbits = Set(du, mask_bits);

  // 2 ^ {0,1, .., 31}, will not have more lanes than that.
  const svuint64_t bit = Shl(Set(du, 1), Iota(du, 0));

  return TestBit(vbits, bit);
}

// ------------------------------ StoreMaskBits

namespace detail {

// Returns mask ? 1 : 0 in BYTE lanes.
template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 1)>
HWY_API svuint8_t BoolFromMask(Simd<T, N> d, svbool_t m) {
  return svdup_n_u8_z(m, 1);
}
template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 2)>
HWY_API svuint8_t BoolFromMask(Simd<T, N> d, svbool_t m) {
  const Repartition<uint8_t, decltype(d)> d8;
  const svuint8_t b16 = BitCast(d8, svdup_n_u16_z(m, 1));
  return detail::ConcatEven(b16, b16);
}
template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 4)>
HWY_API svuint8_t BoolFromMask(Simd<T, N> d, svbool_t m) {
  return U8FromU32(svdup_n_u32_z(m, 1));
}
template <typename T, size_t N, HWY_IF_LANE_SIZE(T, 8)>
HWY_API svuint8_t BoolFromMask(Simd<T, N> d, svbool_t m) {
  const Repartition<uint32_t, decltype(d)> d32;
  const svuint32_t b64 = BitCast(d32, svdup_n_u64_z(m, 1));
  return U8FromU32(detail::ConcatEven(b64, b64));
}

}  // namespace detail

// `p` points to at least 8 writable bytes.
template <typename T, size_t N>
HWY_API size_t StoreMaskBits(Simd<T, N> d, svbool_t m, uint8_t* bits) {
  const Repartition<uint8_t, decltype(d)> d8;
  const Repartition<uint16_t, decltype(d)> d16;
  const Repartition<uint32_t, decltype(d)> d32;
  const Repartition<uint64_t, decltype(d)> d64;
  auto x = detail::BoolFromMask(d, m);
  // Compact bytes to bits. Could use SVE2 BDEP, but it's optional.
  x = Or(x, BitCast(d8, ShiftRight<7>(BitCast(d16, x))));
  x = Or(x, BitCast(d8, ShiftRight<14>(BitCast(d32, x))));
  x = Or(x, BitCast(d8, ShiftRight<28>(BitCast(d64, x))));

  const size_t num_bits = Lanes(d);
  const size_t num_bytes = (num_bits + 8 - 1) / 8;  // Round up, see below

  // Truncate to 8 bits and store.
  svst1b_u64(FirstN(d64, num_bytes), bits, BitCast(d64, x));

  // Non-full byte, need to clear the undefined upper bits. Can happen for
  // capped/partial vectors or large T and small hardware vectors.
  if (num_bits < 8) {
    const int mask = (1 << num_bits) - 1;
    bits[0] = static_cast<uint8_t>(bits[0] & mask);
  }
  // Else: we wrote full bytes because num_bits is a power of two >= 8.

  return num_bytes;
}

// ------------------------------ CompressBits, CompressBitsStore (LoadMaskBits)

template <class V>
HWY_INLINE V CompressBits(V v, const uint8_t* HWY_RESTRICT bits) {
  return Compress(v, LoadMaskBits(DFromV<V>(), bits));
}

template <class D>
HWY_API size_t CompressBitsStore(VFromD<D> v, const uint8_t* HWY_RESTRICT bits,
                                 D d, TFromD<D>* HWY_RESTRICT unaligned) {
  return CompressStore(v, LoadMaskBits(d, bits), d, unaligned);
}

// ------------------------------ MulEven (InterleaveEven)

#if HWY_TARGET == HWY_SVE2
namespace detail {
HWY_SVE_FOREACH_UI32(HWY_SVE_RETV_ARGPVV, MulEven, mullb)
}  // namespace detail
#endif

template <class V, class DW = RepartitionToWide<DFromV<V>>>
HWY_API VFromD<DW> MulEven(const V a, const V b) {
#if HWY_TARGET == HWY_SVE2
  return BitCast(DW(), detail::MulEven(a, b));
#else
  const auto lo = Mul(a, b);
  const auto hi = detail::MulHigh(a, b);
  return BitCast(DW(), detail::InterleaveEven(lo, hi));
#endif
}

HWY_API svuint64_t MulEven(const svuint64_t a, const svuint64_t b) {
  const auto lo = Mul(a, b);
  const auto hi = detail::MulHigh(a, b);
  return detail::InterleaveEven(lo, hi);
}

HWY_API svuint64_t MulOdd(const svuint64_t a, const svuint64_t b) {
  const auto lo = Mul(a, b);
  const auto hi = detail::MulHigh(a, b);
  return detail::InterleaveOdd(lo, hi);
}

// ------------------------------ AESRound / CLMul

#if defined(__ARM_FEATURE_SVE2_AES)

// Per-target flag to prevent generic_ops-inl.h from defining AESRound.
#ifdef HWY_NATIVE_AES
#undef HWY_NATIVE_AES
#else
#define HWY_NATIVE_AES
#endif

HWY_API svuint8_t AESRound(svuint8_t state, svuint8_t round_key) {
  // NOTE: it is important that AESE and AESMC be consecutive instructions so
  // they can be fused. AESE includes AddRoundKey, which is a different ordering
  // than the AES-NI semantics we adopted, so XOR by 0 and later with the actual
  // round key (the compiler will hopefully optimize this for multiple rounds).
  const svuint8_t zero = Zero(HWY_FULL(uint8_t)());
  return Xor(vaesmcq_u8(vaeseq_u8(state, zero), round_key));
}

HWY_API svuint64_t CLMulLower(const svuint64_t a, const svuint64_t b) {
  return svpmullb_pair(a, b);
}

HWY_API svuint64_t CLMulUpper(const svuint64_t a, const svuint64_t b) {
  return svpmullt_pair(a, b);
}

#endif  // __ARM_FEATURE_SVE2_AES

// ================================================== END MACROS
namespace detail {  // for code folding
#undef HWY_IF_FLOAT_V
#undef HWY_IF_LANE_SIZE_V
#undef HWY_IF_SIGNED_V
#undef HWY_IF_UNSIGNED_V
#undef HWY_SVE_D
#undef HWY_SVE_FOREACH
#undef HWY_SVE_FOREACH_F
#undef HWY_SVE_FOREACH_F16
#undef HWY_SVE_FOREACH_F32
#undef HWY_SVE_FOREACH_F64
#undef HWY_SVE_FOREACH_I
#undef HWY_SVE_FOREACH_I08
#undef HWY_SVE_FOREACH_I16
#undef HWY_SVE_FOREACH_I32
#undef HWY_SVE_FOREACH_I64
#undef HWY_SVE_FOREACH_IF
#undef HWY_SVE_FOREACH_U
#undef HWY_SVE_FOREACH_U08
#undef HWY_SVE_FOREACH_U16
#undef HWY_SVE_FOREACH_U32
#undef HWY_SVE_FOREACH_U64
#undef HWY_SVE_FOREACH_UI
#undef HWY_SVE_FOREACH_UI08
#undef HWY_SVE_FOREACH_UI16
#undef HWY_SVE_FOREACH_UI32
#undef HWY_SVE_FOREACH_UI64
#undef HWY_SVE_FOREACH_UIF3264
#undef HWY_SVE_PTRUE
#undef HWY_SVE_RETV_ARGD
#undef HWY_SVE_RETV_ARGPV
#undef HWY_SVE_RETV_ARGPVN
#undef HWY_SVE_RETV_ARGPVV
#undef HWY_SVE_RETV_ARGV
#undef HWY_SVE_RETV_ARGVN
#undef HWY_SVE_RETV_ARGVV
#undef HWY_SVE_T
#undef HWY_SVE_V

}  // namespace detail
// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace hwy
HWY_AFTER_NAMESPACE();
