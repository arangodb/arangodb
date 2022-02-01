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

// RISC-V V vectors (length not known at compile time).
// External include guard in highway.h - see comment there.

#include <riscv_vector.h>
#include <stddef.h>
#include <stdint.h>

#include "hwy/base.h"
#include "hwy/ops/shared-inl.h"

HWY_BEFORE_NAMESPACE();
namespace hwy {
namespace HWY_NAMESPACE {

template <class V>
struct DFromV_t {};  // specialized in macros
template <class V>
using DFromV = typename DFromV_t<RemoveConst<V>>::type;

template <class V>
using TFromV = TFromD<DFromV<V>>;

template <typename T, size_t N>
HWY_INLINE constexpr size_t MLenFromD(Simd<T, N> /* tag */) {
  // Returns divisor = type bits / LMUL
  return sizeof(T) * 8 / (N / HWY_LANES(T));
}

// kShift = log2 of multiplier: 0 for m1, 1 for m2, -2 for mf4
template <typename T, int kShift = 0>
using Full = Simd<T, (kShift < 0) ? (HWY_LANES(T) >> (-kShift))
                                  : (HWY_LANES(T) << kShift)>;

// ================================================== MACROS

// Generate specializations and function definitions using X macros. Although
// harder to read and debug, writing everything manually is too bulky.

namespace detail {  // for code folding

// For all mask sizes: (1/Nth of a register, one bit per lane)
#define HWY_RVV_FOREACH_B(X_MACRO, NAME, OP) \
  X_MACRO(64, NAME, OP)                      \
  X_MACRO(32, NAME, OP)                      \
  X_MACRO(16, NAME, OP)                      \
  X_MACRO(8, NAME, OP)                       \
  X_MACRO(4, NAME, OP)                       \
  X_MACRO(2, NAME, OP)                       \
  X_MACRO(1, NAME, OP)

// For given SEW, iterate over all LMUL. Precompute SEW/LMUL => MLEN because the
// preprocessor cannot easily do it.
// TODO(janwas): GCC does not yet support fractional LMUL
#define HWY_RVV_FOREACH_08(X_MACRO, BASE, CHAR, NAME, OP)        \
  X_MACRO(BASE, CHAR, 8, m1, /*kShift=*/0, /*MLEN=*/8, NAME, OP) \
  X_MACRO(BASE, CHAR, 8, m2, /*kShift=*/1, /*MLEN=*/4, NAME, OP) \
  X_MACRO(BASE, CHAR, 8, m4, /*kShift=*/2, /*MLEN=*/2, NAME, OP) \
  X_MACRO(BASE, CHAR, 8, m8, /*kShift=*/3, /*MLEN=*/1, NAME, OP)

#define HWY_RVV_FOREACH_16(X_MACRO, BASE, CHAR, NAME, OP)          \
  X_MACRO(BASE, CHAR, 16, m1, /*kShift=*/0, /*MLEN=*/16, NAME, OP) \
  X_MACRO(BASE, CHAR, 16, m2, /*kShift=*/1, /*MLEN=*/8, NAME, OP)  \
  X_MACRO(BASE, CHAR, 16, m4, /*kShift=*/2, /*MLEN=*/4, NAME, OP)  \
  X_MACRO(BASE, CHAR, 16, m8, /*kShift=*/3, /*MLEN=*/2, NAME, OP)

#define HWY_RVV_FOREACH_32(X_MACRO, BASE, CHAR, NAME, OP)          \
  X_MACRO(BASE, CHAR, 32, m1, /*kShift=*/0, /*MLEN=*/32, NAME, OP) \
  X_MACRO(BASE, CHAR, 32, m2, /*kShift=*/1, /*MLEN=*/16, NAME, OP) \
  X_MACRO(BASE, CHAR, 32, m4, /*kShift=*/2, /*MLEN=*/8, NAME, OP)  \
  X_MACRO(BASE, CHAR, 32, m8, /*kShift=*/3, /*MLEN=*/4, NAME, OP)

#define HWY_RVV_FOREACH_64(X_MACRO, BASE, CHAR, NAME, OP)          \
  X_MACRO(BASE, CHAR, 64, m1, /*kShift=*/0, /*MLEN=*/64, NAME, OP) \
  X_MACRO(BASE, CHAR, 64, m2, /*kShift=*/1, /*MLEN=*/32, NAME, OP) \
  X_MACRO(BASE, CHAR, 64, m4, /*kShift=*/2, /*MLEN=*/16, NAME, OP) \
  X_MACRO(BASE, CHAR, 64, m8, /*kShift=*/3, /*MLEN=*/8, NAME, OP)

// SEW for unsigned:
#define HWY_RVV_FOREACH_U08(X_MACRO, NAME, OP) \
  HWY_RVV_FOREACH_08(X_MACRO, uint, u, NAME, OP)
#define HWY_RVV_FOREACH_U16(X_MACRO, NAME, OP) \
  HWY_RVV_FOREACH_16(X_MACRO, uint, u, NAME, OP)
#define HWY_RVV_FOREACH_U32(X_MACRO, NAME, OP) \
  HWY_RVV_FOREACH_32(X_MACRO, uint, u, NAME, OP)
#define HWY_RVV_FOREACH_U64(X_MACRO, NAME, OP) \
  HWY_RVV_FOREACH_64(X_MACRO, uint, u, NAME, OP)

// SEW for signed:
#define HWY_RVV_FOREACH_I08(X_MACRO, NAME, OP) \
  HWY_RVV_FOREACH_08(X_MACRO, int, i, NAME, OP)
#define HWY_RVV_FOREACH_I16(X_MACRO, NAME, OP) \
  HWY_RVV_FOREACH_16(X_MACRO, int, i, NAME, OP)
#define HWY_RVV_FOREACH_I32(X_MACRO, NAME, OP) \
  HWY_RVV_FOREACH_32(X_MACRO, int, i, NAME, OP)
#define HWY_RVV_FOREACH_I64(X_MACRO, NAME, OP) \
  HWY_RVV_FOREACH_64(X_MACRO, int, i, NAME, OP)

// SEW for float:
#define HWY_RVV_FOREACH_F16(X_MACRO, NAME, OP) \
  HWY_RVV_FOREACH_16(X_MACRO, float, f, NAME, OP)
#define HWY_RVV_FOREACH_F32(X_MACRO, NAME, OP) \
  HWY_RVV_FOREACH_32(X_MACRO, float, f, NAME, OP)
#define HWY_RVV_FOREACH_F64(X_MACRO, NAME, OP) \
  HWY_RVV_FOREACH_64(X_MACRO, float, f, NAME, OP)

// For all combinations of SEW:
#define HWY_RVV_FOREACH_U(X_MACRO, NAME, OP) \
  HWY_RVV_FOREACH_U08(X_MACRO, NAME, OP)     \
  HWY_RVV_FOREACH_U16(X_MACRO, NAME, OP)     \
  HWY_RVV_FOREACH_U32(X_MACRO, NAME, OP)     \
  HWY_RVV_FOREACH_U64(X_MACRO, NAME, OP)

#define HWY_RVV_FOREACH_I(X_MACRO, NAME, OP) \
  HWY_RVV_FOREACH_I08(X_MACRO, NAME, OP)     \
  HWY_RVV_FOREACH_I16(X_MACRO, NAME, OP)     \
  HWY_RVV_FOREACH_I32(X_MACRO, NAME, OP)     \
  HWY_RVV_FOREACH_I64(X_MACRO, NAME, OP)

#define HWY_RVV_FOREACH_F(X_MACRO, NAME, OP) \
  HWY_RVV_FOREACH_F16(X_MACRO, NAME, OP)     \
  HWY_RVV_FOREACH_F32(X_MACRO, NAME, OP)     \
  HWY_RVV_FOREACH_F64(X_MACRO, NAME, OP)

// Commonly used type categories for a given SEW:
#define HWY_RVV_FOREACH_UI16(X_MACRO, NAME, OP) \
  HWY_RVV_FOREACH_U16(X_MACRO, NAME, OP)        \
  HWY_RVV_FOREACH_I16(X_MACRO, NAME, OP)

#define HWY_RVV_FOREACH_UI32(X_MACRO, NAME, OP) \
  HWY_RVV_FOREACH_U32(X_MACRO, NAME, OP)        \
  HWY_RVV_FOREACH_I32(X_MACRO, NAME, OP)

#define HWY_RVV_FOREACH_UI64(X_MACRO, NAME, OP) \
  HWY_RVV_FOREACH_U64(X_MACRO, NAME, OP)        \
  HWY_RVV_FOREACH_I64(X_MACRO, NAME, OP)

// Commonly used type categories:
#define HWY_RVV_FOREACH_UI(X_MACRO, NAME, OP) \
  HWY_RVV_FOREACH_U(X_MACRO, NAME, OP)        \
  HWY_RVV_FOREACH_I(X_MACRO, NAME, OP)

#define HWY_RVV_FOREACH(X_MACRO, NAME, OP) \
  HWY_RVV_FOREACH_U(X_MACRO, NAME, OP)     \
  HWY_RVV_FOREACH_I(X_MACRO, NAME, OP)     \
  HWY_RVV_FOREACH_F(X_MACRO, NAME, OP)

// Assemble types for use in x-macros
#define HWY_RVV_T(BASE, SEW) BASE##SEW##_t
#define HWY_RVV_D(CHAR, SEW, LMUL) D##CHAR##SEW##LMUL
#define HWY_RVV_V(BASE, SEW, LMUL) v##BASE##SEW##LMUL##_t
#define HWY_RVV_M(MLEN) vbool##MLEN##_t

}  // namespace detail

// TODO(janwas): remove typedefs and only use HWY_RVV_V etc. directly

// Until we have full intrinsic support for fractional LMUL, mixed-precision
// code can use LMUL 1..8 (adequate unless they need many registers).
#define HWY_SPECIALIZE(BASE, CHAR, SEW, LMUL, SHIFT, MLEN, NAME, OP)    \
  using HWY_RVV_D(CHAR, SEW, LMUL) = Full<HWY_RVV_T(BASE, SEW), SHIFT>; \
  using V##CHAR##SEW##LMUL = HWY_RVV_V(BASE, SEW, LMUL);                \
  template <>                                                           \
  struct DFromV_t<HWY_RVV_V(BASE, SEW, LMUL)> {                         \
    using Lane = HWY_RVV_T(BASE, SEW);                                  \
    using type = Full<Lane, SHIFT>;                                     \
  };
using Vf16m1 = vfloat16m1_t;
using Vf16m2 = vfloat16m2_t;
using Vf16m4 = vfloat16m4_t;
using Vf16m8 = vfloat16m8_t;
using Df16m1 = Full<float16_t, 0>;
using Df16m2 = Full<float16_t, 1>;
using Df16m4 = Full<float16_t, 2>;
using Df16m8 = Full<float16_t, 3>;

HWY_RVV_FOREACH(HWY_SPECIALIZE, _, _)
#undef HWY_SPECIALIZE

// vector = f(d), e.g. Zero
#define HWY_RVV_RETV_ARGD(BASE, CHAR, SEW, LMUL, SHIFT, MLEN, NAME, OP)   \
  HWY_API HWY_RVV_V(BASE, SEW, LMUL) NAME(HWY_RVV_D(CHAR, SEW, LMUL) d) { \
    (void)Lanes(d);                                                       \
    return v##OP##_##CHAR##SEW##LMUL();                                   \
  }

// vector = f(vector), e.g. Not
#define HWY_RVV_RETV_ARGV(BASE, CHAR, SEW, LMUL, SHIFT, MLEN, NAME, OP)   \
  HWY_API HWY_RVV_V(BASE, SEW, LMUL) NAME(HWY_RVV_V(BASE, SEW, LMUL) v) { \
    return v##OP##_v_##CHAR##SEW##LMUL(v);                                \
  }

// vector = f(vector, scalar), e.g. detail::AddS
#define HWY_RVV_RETV_ARGVS(BASE, CHAR, SEW, LMUL, SHIFT, MLEN, NAME, OP) \
  HWY_API HWY_RVV_V(BASE, SEW, LMUL)                                     \
      NAME(HWY_RVV_V(BASE, SEW, LMUL) a, HWY_RVV_T(BASE, SEW) b) {       \
    return v##OP##_##CHAR##SEW##LMUL(a, b);                              \
  }

// vector = f(vector, vector), e.g. Add
#define HWY_RVV_RETV_ARGVV(BASE, CHAR, SEW, LMUL, SHIFT, MLEN, NAME, OP) \
  HWY_API HWY_RVV_V(BASE, SEW, LMUL)                                     \
      NAME(HWY_RVV_V(BASE, SEW, LMUL) a, HWY_RVV_V(BASE, SEW, LMUL) b) { \
    return v##OP##_vv_##CHAR##SEW##LMUL(a, b);                           \
  }

// ================================================== INIT

// ------------------------------ Lanes

// WARNING: we want to query VLMAX/sizeof(T), but this actually changes VL!
// vlenb is not exposed through intrinsics and vreadvl is not VLMAX.
#define HWY_RVV_LANES(BASE, CHAR, SEW, LMUL, SHIFT, MLEN, NAME, OP) \
  HWY_API size_t NAME(HWY_RVV_D(CHAR, SEW, LMUL) /* d */) {         \
    return v##OP##SEW##LMUL();                                      \
  }

HWY_RVV_FOREACH(HWY_RVV_LANES, Lanes, setvlmax_e)
#undef HWY_RVV_LANES

// ------------------------------ Zero

HWY_RVV_FOREACH(HWY_RVV_RETV_ARGD, Zero, zero)

template <class D>
using VFromD = decltype(Zero(D()));

// Partial
template <typename T, size_t N, HWY_IF_LE128(T, N)>
HWY_API VFromD<Full<T>> Zero(Simd<T, N> /*tag*/) {
  return Zero(Full<T>());
}

// ------------------------------ Set
// vector = f(d, scalar), e.g. Set
#define HWY_RVV_SET(BASE, CHAR, SEW, LMUL, SHIFT, MLEN, NAME, OP)    \
  HWY_API HWY_RVV_V(BASE, SEW, LMUL)                                 \
      NAME(HWY_RVV_D(CHAR, SEW, LMUL) d, HWY_RVV_T(BASE, SEW) arg) { \
    (void)Lanes(d);                                                  \
    return v##OP##_##CHAR##SEW##LMUL(arg);                           \
  }

HWY_RVV_FOREACH_UI(HWY_RVV_SET, Set, mv_v_x)
HWY_RVV_FOREACH_F(HWY_RVV_SET, Set, fmv_v_f)
#undef HWY_RVV_SET

// Partial vectors
template <typename T, size_t N, HWY_IF_LE128(T, N)>
HWY_API VFromD<Simd<T, N>> Set(Simd<T, N> /*tag*/, T arg) {
  return Set(Full<T>(), arg);
}

// ------------------------------ Undefined

// RVV vundefined is 'poisoned' such that even XORing a _variable_ initialized
// by it gives unpredictable results. It should only be used for maskoff, so
// keep it internal. For the Highway op, just use Zero (single instruction).
namespace detail {
HWY_RVV_FOREACH(HWY_RVV_RETV_ARGD, Undefined, undefined)
}  // namespace detail

template <class D>
HWY_API VFromD<D> Undefined(D d) {
  return Zero(d);
}

// ------------------------------ BitCast

namespace detail {

// u8: no change
#define HWY_RVV_CAST_NOP(BASE, CHAR, SEW, LMUL, SHIFT, MLEN, NAME, OP)    \
  HWY_API HWY_RVV_V(BASE, SEW, LMUL)                                      \
      BitCastToByte(HWY_RVV_V(BASE, SEW, LMUL) v) {                       \
    return v;                                                             \
  }                                                                       \
  HWY_API HWY_RVV_V(BASE, SEW, LMUL) BitCastFromByte(                     \
      HWY_RVV_D(CHAR, SEW, LMUL) /* d */, HWY_RVV_V(BASE, SEW, LMUL) v) { \
    return v;                                                             \
  }

// Other integers
#define HWY_RVV_CAST_UI(BASE, CHAR, SEW, LMUL, SHIFT, MLEN, NAME, OP)    \
  HWY_API vuint8##LMUL##_t BitCastToByte(HWY_RVV_V(BASE, SEW, LMUL) v) { \
    return v##OP##_v_##CHAR##SEW##LMUL##_u8##LMUL(v);                    \
  }                                                                      \
  HWY_API HWY_RVV_V(BASE, SEW, LMUL) BitCastFromByte(                    \
      HWY_RVV_D(CHAR, SEW, LMUL) /* d */, vuint8##LMUL##_t v) {          \
    return v##OP##_v_u8##LMUL##_##CHAR##SEW##LMUL(v);                    \
  }

// Float: first cast to/from unsigned
#define HWY_RVV_CAST_F(BASE, CHAR, SEW, LMUL, SHIFT, MLEN, NAME, OP)     \
  HWY_API vuint8##LMUL##_t BitCastToByte(HWY_RVV_V(BASE, SEW, LMUL) v) { \
    return v##OP##_v_u##SEW##LMUL##_u8##LMUL(                            \
        v##OP##_v_f##SEW##LMUL##_u##SEW##LMUL(v));                       \
  }                                                                      \
  HWY_API HWY_RVV_V(BASE, SEW, LMUL) BitCastFromByte(                    \
      HWY_RVV_D(CHAR, SEW, LMUL) /* d */, vuint8##LMUL##_t v) {          \
    return v##OP##_v_u##SEW##LMUL##_f##SEW##LMUL(                        \
        v##OP##_v_u8##LMUL##_u##SEW##LMUL(v));                           \
  }

HWY_RVV_FOREACH_U08(HWY_RVV_CAST_NOP, _, _)
HWY_RVV_FOREACH_I08(HWY_RVV_CAST_UI, _, reinterpret)
HWY_RVV_FOREACH_UI16(HWY_RVV_CAST_UI, _, reinterpret)
HWY_RVV_FOREACH_UI32(HWY_RVV_CAST_UI, _, reinterpret)
HWY_RVV_FOREACH_UI64(HWY_RVV_CAST_UI, _, reinterpret)
HWY_RVV_FOREACH_F(HWY_RVV_CAST_F, _, reinterpret)

#undef HWY_RVV_CAST_NOP
#undef HWY_RVV_CAST_UI
#undef HWY_RVV_CAST_F

}  // namespace detail

template <class D, class FromV>
HWY_API VFromD<D> BitCast(D d, FromV v) {
  return detail::BitCastFromByte(d, detail::BitCastToByte(v));
}

// Partial
template <typename T, size_t N, class FromV, HWY_IF_LE128(T, N)>
HWY_API VFromD<Simd<T, N>> BitCast(Simd<T, N> /*tag*/, FromV v) {
  return BitCast(Full<T>(), v);
}

namespace detail {

template <class V, class DU = RebindToUnsigned<DFromV<V>>>
HWY_INLINE VFromD<DU> BitCastToUnsigned(V v) {
  return BitCast(DU(), v);
}

}  // namespace detail

// ------------------------------ Iota

namespace detail {

HWY_RVV_FOREACH_U(HWY_RVV_RETV_ARGD, Iota0, id_v)

template <class D, class DU = RebindToUnsigned<D>>
HWY_INLINE VFromD<DU> Iota0(const D /*d*/) {
  Lanes(DU());
  return BitCastToUnsigned(Iota0(DU()));
}

// Partial
template <typename T, size_t N, HWY_IF_LE128(T, N)>
HWY_INLINE VFromD<Simd<T, N>> Iota0(Simd<T, N> /*tag*/) {
  return Iota0(Full<T>());
}

}  // namespace detail

// ================================================== LOGICAL

// ------------------------------ Not

HWY_RVV_FOREACH_UI(HWY_RVV_RETV_ARGV, Not, not )

template <class V, HWY_IF_FLOAT_V(V)>
HWY_API V Not(const V v) {
  using DF = DFromV<V>;
  using DU = RebindToUnsigned<DF>;
  return BitCast(DF(), Not(BitCast(DU(), v)));
}

// ------------------------------ And

// Non-vector version (ideally immediate) for use with Iota0
namespace detail {
HWY_RVV_FOREACH_UI(HWY_RVV_RETV_ARGVS, AndS, and_vx)
}  // namespace detail

HWY_RVV_FOREACH_UI(HWY_RVV_RETV_ARGVV, And, and)

template <class V, HWY_IF_FLOAT_V(V)>
HWY_API V And(const V a, const V b) {
  using DF = DFromV<V>;
  using DU = RebindToUnsigned<DF>;
  return BitCast(DF(), And(BitCast(DU(), a), BitCast(DU(), b)));
}

// ------------------------------ Or

#undef HWY_RVV_OR_MASK

HWY_RVV_FOREACH_UI(HWY_RVV_RETV_ARGVV, Or, or)

template <class V, HWY_IF_FLOAT_V(V)>
HWY_API V Or(const V a, const V b) {
  using DF = DFromV<V>;
  using DU = RebindToUnsigned<DF>;
  return BitCast(DF(), Or(BitCast(DU(), a), BitCast(DU(), b)));
}

// ------------------------------ Xor

// Non-vector version (ideally immediate) for use with Iota0
namespace detail {
HWY_RVV_FOREACH_UI(HWY_RVV_RETV_ARGVS, XorS, xor_vx)
}  // namespace detail

HWY_RVV_FOREACH_UI(HWY_RVV_RETV_ARGVV, Xor, xor)

template <class V, HWY_IF_FLOAT_V(V)>
HWY_API V Xor(const V a, const V b) {
  using DF = DFromV<V>;
  using DU = RebindToUnsigned<DF>;
  return BitCast(DF(), Xor(BitCast(DU(), a), BitCast(DU(), b)));
}

// ------------------------------ AndNot

template <class V>
HWY_API V AndNot(const V not_a, const V b) {
  return And(Not(not_a), b);
}

// ------------------------------ CopySign

HWY_RVV_FOREACH_F(HWY_RVV_RETV_ARGVV, CopySign, fsgnj)

template <class V>
HWY_API V CopySignToAbs(const V abs, const V sign) {
  // RVV can also handle abs < 0, so no extra action needed.
  return CopySign(abs, sign);
}

// ================================================== ARITHMETIC

// ------------------------------ Add

namespace detail {
HWY_RVV_FOREACH_UI(HWY_RVV_RETV_ARGVS, AddS, add_vx)
HWY_RVV_FOREACH_F(HWY_RVV_RETV_ARGVS, AddS, fadd_vf)
}  // namespace detail

HWY_RVV_FOREACH_UI(HWY_RVV_RETV_ARGVV, Add, add)
HWY_RVV_FOREACH_F(HWY_RVV_RETV_ARGVV, Add, fadd)

// ------------------------------ Sub
HWY_RVV_FOREACH_UI(HWY_RVV_RETV_ARGVV, Sub, sub)
HWY_RVV_FOREACH_F(HWY_RVV_RETV_ARGVV, Sub, fsub)

// ------------------------------ SaturatedAdd

HWY_RVV_FOREACH_U08(HWY_RVV_RETV_ARGVV, SaturatedAdd, saddu)
HWY_RVV_FOREACH_U16(HWY_RVV_RETV_ARGVV, SaturatedAdd, saddu)

HWY_RVV_FOREACH_I08(HWY_RVV_RETV_ARGVV, SaturatedAdd, sadd)
HWY_RVV_FOREACH_I16(HWY_RVV_RETV_ARGVV, SaturatedAdd, sadd)

// ------------------------------ SaturatedSub

HWY_RVV_FOREACH_U08(HWY_RVV_RETV_ARGVV, SaturatedSub, ssubu)
HWY_RVV_FOREACH_U16(HWY_RVV_RETV_ARGVV, SaturatedSub, ssubu)

HWY_RVV_FOREACH_I08(HWY_RVV_RETV_ARGVV, SaturatedSub, ssub)
HWY_RVV_FOREACH_I16(HWY_RVV_RETV_ARGVV, SaturatedSub, ssub)

// ------------------------------ AverageRound

// TODO(janwas): check vxrm rounding mode
HWY_RVV_FOREACH_U08(HWY_RVV_RETV_ARGVV, AverageRound, aaddu)
HWY_RVV_FOREACH_U16(HWY_RVV_RETV_ARGVV, AverageRound, aaddu)

// ------------------------------ ShiftLeft[Same]

// Intrinsics do not define .vi forms, so use .vx instead.
#define HWY_RVV_SHIFT(BASE, CHAR, SEW, LMUL, SHIFT, MLEN, NAME, OP)       \
  template <int kBits>                                                    \
  HWY_API HWY_RVV_V(BASE, SEW, LMUL) NAME(HWY_RVV_V(BASE, SEW, LMUL) v) { \
    return v##OP##_vx_##CHAR##SEW##LMUL(v, kBits);                        \
  }                                                                       \
  HWY_API HWY_RVV_V(BASE, SEW, LMUL)                                      \
      NAME##Same(HWY_RVV_V(BASE, SEW, LMUL) v, int bits) {                \
    return v##OP##_vx_##CHAR##SEW##LMUL(v, static_cast<uint8_t>(bits));   \
  }

HWY_RVV_FOREACH_UI(HWY_RVV_SHIFT, ShiftLeft, sll)

// ------------------------------ ShiftRight[Same]

HWY_RVV_FOREACH_U(HWY_RVV_SHIFT, ShiftRight, srl)
HWY_RVV_FOREACH_I(HWY_RVV_SHIFT, ShiftRight, sra)

#undef HWY_RVV_SHIFT

// ------------------------------ Shl
#define HWY_RVV_SHIFT_VV(BASE, CHAR, SEW, LMUL, SHIFT, MLEN, NAME, OP)      \
  HWY_API HWY_RVV_V(BASE, SEW, LMUL)                                        \
      NAME(HWY_RVV_V(BASE, SEW, LMUL) v, HWY_RVV_V(BASE, SEW, LMUL) bits) { \
    return v##OP##_vv_##CHAR##SEW##LMUL(v, bits);                           \
  }

HWY_RVV_FOREACH_U(HWY_RVV_SHIFT_VV, Shl, sll)

#define HWY_RVV_SHIFT_II(BASE, CHAR, SEW, LMUL, SHIFT, MLEN, NAME, OP)       \
  HWY_API HWY_RVV_V(BASE, SEW, LMUL)                                         \
      NAME(HWY_RVV_V(BASE, SEW, LMUL) v, HWY_RVV_V(BASE, SEW, LMUL) bits) {  \
    return v##OP##_vv_##CHAR##SEW##LMUL(v, detail::BitCastToUnsigned(bits)); \
  }

HWY_RVV_FOREACH_I(HWY_RVV_SHIFT_II, Shl, sll)

// ------------------------------ Shr

HWY_RVV_FOREACH_U(HWY_RVV_SHIFT_VV, Shr, srl)
HWY_RVV_FOREACH_I(HWY_RVV_SHIFT_II, Shr, sra)

#undef HWY_RVV_SHIFT_II
#undef HWY_RVV_SHIFT_VV

// ------------------------------ Min

HWY_RVV_FOREACH_U(HWY_RVV_RETV_ARGVV, Min, minu)
HWY_RVV_FOREACH_I(HWY_RVV_RETV_ARGVV, Min, min)
HWY_RVV_FOREACH_F(HWY_RVV_RETV_ARGVV, Min, fmin)

// ------------------------------ Max

namespace detail {

HWY_RVV_FOREACH_U(HWY_RVV_RETV_ARGVS, Max, maxu_vx)
HWY_RVV_FOREACH_I(HWY_RVV_RETV_ARGVS, Max, max_vx)
HWY_RVV_FOREACH_F(HWY_RVV_RETV_ARGVS, Max, fmax_vf)

}  // namespace detail

HWY_RVV_FOREACH_U(HWY_RVV_RETV_ARGVV, Max, maxu)
HWY_RVV_FOREACH_I(HWY_RVV_RETV_ARGVV, Max, max)
HWY_RVV_FOREACH_F(HWY_RVV_RETV_ARGVV, Max, fmax)

// ------------------------------ Mul

// Only for internal use (Highway only promises Mul for 16/32-bit inputs).
// Used by MulLower.
namespace detail {
HWY_RVV_FOREACH_U64(HWY_RVV_RETV_ARGVV, Mul, mul)
}  // namespace detail

HWY_RVV_FOREACH_UI16(HWY_RVV_RETV_ARGVV, Mul, mul)
HWY_RVV_FOREACH_UI32(HWY_RVV_RETV_ARGVV, Mul, mul)
HWY_RVV_FOREACH_F(HWY_RVV_RETV_ARGVV, Mul, fmul)

// ------------------------------ MulHigh

// Only for internal use (Highway only promises MulHigh for 16-bit inputs).
// Used by MulEven; vwmul does not work for m8.
namespace detail {
HWY_RVV_FOREACH_I32(HWY_RVV_RETV_ARGVV, MulHigh, mulh)
HWY_RVV_FOREACH_U32(HWY_RVV_RETV_ARGVV, MulHigh, mulhu)
HWY_RVV_FOREACH_U64(HWY_RVV_RETV_ARGVV, MulHigh, mulhu)
}  // namespace detail

HWY_RVV_FOREACH_U16(HWY_RVV_RETV_ARGVV, MulHigh, mulhu)
HWY_RVV_FOREACH_I16(HWY_RVV_RETV_ARGVV, MulHigh, mulh)

// ------------------------------ Div
HWY_RVV_FOREACH_F(HWY_RVV_RETV_ARGVV, Div, fdiv)

// ------------------------------ ApproximateReciprocal

// TODO(janwas): not yet supported in intrinsics
template <class V>
HWY_API V ApproximateReciprocal(const V v) {
  return Set(DFromV<V>(), 1) / v;
}
// HWY_RVV_FOREACH_F32(HWY_RVV_RETV_ARGV, ApproximateReciprocal, frece7)

// ------------------------------ Sqrt
HWY_RVV_FOREACH_F(HWY_RVV_RETV_ARGV, Sqrt, fsqrt)

// ------------------------------ ApproximateReciprocalSqrt

// TODO(janwas): not yet supported in intrinsics
template <class V>
HWY_API V ApproximateReciprocalSqrt(const V v) {
  return ApproximateReciprocal(Sqrt(v));
}
// HWY_RVV_FOREACH_F32(HWY_RVV_RETV_ARGV, ApproximateReciprocalSqrt, frsqrte7)

// ------------------------------ MulAdd
// Note: op is still named vv, not vvv.
#define HWY_RVV_FMA(BASE, CHAR, SEW, LMUL, SHIFT, MLEN, NAME, OP)        \
  HWY_API HWY_RVV_V(BASE, SEW, LMUL)                                     \
      NAME(HWY_RVV_V(BASE, SEW, LMUL) mul, HWY_RVV_V(BASE, SEW, LMUL) x, \
           HWY_RVV_V(BASE, SEW, LMUL) add) {                             \
    return v##OP##_vv_##CHAR##SEW##LMUL(add, mul, x);                    \
  }

HWY_RVV_FOREACH_F(HWY_RVV_FMA, MulAdd, fmacc)

// ------------------------------ NegMulAdd
HWY_RVV_FOREACH_F(HWY_RVV_FMA, NegMulAdd, fnmsac)

// ------------------------------ MulSub
HWY_RVV_FOREACH_F(HWY_RVV_FMA, MulSub, fmsac)

// ------------------------------ NegMulSub
HWY_RVV_FOREACH_F(HWY_RVV_FMA, NegMulSub, fnmacc)

#undef HWY_RVV_FMA

// ================================================== COMPARE

// Comparisons set a mask bit to 1 if the condition is true, else 0. The XX in
// vboolXX_t is a power of two divisor for vector bits. SLEN 8 / LMUL 1 = 1/8th
// of all bits; SLEN 8 / LMUL 4 = half of all bits.

// mask = f(vector, vector)
#define HWY_RVV_RETM_ARGVV(BASE, CHAR, SEW, LMUL, SHIFT, MLEN, NAME, OP) \
  HWY_API HWY_RVV_M(MLEN)                                                \
      NAME(HWY_RVV_V(BASE, SEW, LMUL) a, HWY_RVV_V(BASE, SEW, LMUL) b) { \
    (void)Lanes(DFromV<decltype(a)>());                                  \
    return v##OP##_vv_##CHAR##SEW##LMUL##_b##MLEN(a, b);                 \
  }

// mask = f(vector, scalar)
#define HWY_RVV_RETM_ARGVS(BASE, CHAR, SEW, LMUL, SHIFT, MLEN, NAME, OP) \
  HWY_API HWY_RVV_M(MLEN)                                                \
      NAME(HWY_RVV_V(BASE, SEW, LMUL) a, HWY_RVV_T(BASE, SEW) b) {       \
    (void)Lanes(DFromV<decltype(a)>());                                  \
    return v##OP##_vx_##CHAR##SEW##LMUL##_b##MLEN(a, b);                 \
  }

// ------------------------------ Eq
HWY_RVV_FOREACH_UI(HWY_RVV_RETM_ARGVV, Eq, mseq)
HWY_RVV_FOREACH_F(HWY_RVV_RETM_ARGVV, Eq, mfeq)

// ------------------------------ Ne
HWY_RVV_FOREACH_UI(HWY_RVV_RETM_ARGVV, Ne, msne)
HWY_RVV_FOREACH_F(HWY_RVV_RETM_ARGVV, Ne, mfne)

// ------------------------------ Lt
HWY_RVV_FOREACH_I(HWY_RVV_RETM_ARGVV, Lt, mslt)
HWY_RVV_FOREACH_F(HWY_RVV_RETM_ARGVV, Lt, mflt)

namespace detail {
HWY_RVV_FOREACH_I(HWY_RVV_RETM_ARGVS, LtS, mslt)
}  // namespace detail

// ------------------------------ Le
HWY_RVV_FOREACH_F(HWY_RVV_RETM_ARGVV, Le, mfle)

#undef HWY_RVV_RETM_ARGVV
#undef HWY_RVV_RETM_ARGVS

// ------------------------------ Gt/Ge

template <class V>
HWY_API auto Ge(const V a, const V b) -> decltype(Le(a, b)) {
  return Le(b, a);
}

template <class V>
HWY_API auto Gt(const V a, const V b) -> decltype(Lt(a, b)) {
  return Lt(b, a);
}

// ------------------------------ TestBit
template <class V>
HWY_API auto TestBit(const V a, const V bit) -> decltype(Eq(a, bit)) {
  return Ne(And(a, bit), Zero(DFromV<V>()));
}

// ------------------------------ Not

// mask = f(mask)
#define HWY_RVV_RETM_ARGM(MLEN, NAME, OP)                 \
  HWY_API HWY_RVV_M(MLEN) NAME(HWY_RVV_M(MLEN) m) {       \
    return vm##OP##_m_b##MLEN(m);                         \
  }

HWY_RVV_FOREACH_B(HWY_RVV_RETM_ARGM, Not, not )

#undef HWY_RVV_RETM_ARGM

// ------------------------------ And

// mask = f(mask_a, mask_b) (note arg2,arg1 order!)
#define HWY_RVV_RETM_ARGMM(MLEN, NAME, OP)                             \
  HWY_API HWY_RVV_M(MLEN) NAME(HWY_RVV_M(MLEN) a, HWY_RVV_M(MLEN) b) { \
    return vm##OP##_mm_b##MLEN(b, a);                                  \
  }

HWY_RVV_FOREACH_B(HWY_RVV_RETM_ARGMM, And, and)

// ------------------------------ AndNot
HWY_RVV_FOREACH_B(HWY_RVV_RETM_ARGMM, AndNot, andnot)

// ------------------------------ Or
HWY_RVV_FOREACH_B(HWY_RVV_RETM_ARGMM, Or, or)

// ------------------------------ Xor
HWY_RVV_FOREACH_B(HWY_RVV_RETM_ARGMM, Xor, xor)

#undef HWY_RVV_RETM_ARGMM

// ------------------------------ IfThenElse
#define HWY_RVV_IF_THEN_ELSE(BASE, CHAR, SEW, LMUL, SHIFT, MLEN, NAME, OP) \
  HWY_API HWY_RVV_V(BASE, SEW, LMUL)                                       \
      NAME(HWY_RVV_M(MLEN) m, HWY_RVV_V(BASE, SEW, LMUL) yes,              \
           HWY_RVV_V(BASE, SEW, LMUL) no) {                                \
    return v##OP##_vvm_##CHAR##SEW##LMUL(m, no, yes);                      \
  }

HWY_RVV_FOREACH(HWY_RVV_IF_THEN_ELSE, IfThenElse, merge)

#undef HWY_RVV_IF_THEN_ELSE

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

// ------------------------------ MaskFromVec

template <class V>
HWY_API auto MaskFromVec(const V v) -> decltype(Eq(v, v)) {
  return Ne(v, Zero(DFromV<V>()));
}

template <class D>
using MFromD = decltype(MaskFromVec(Zero(D())));

template <class D, typename MFrom>
HWY_API MFromD<D> RebindMask(const D /*d*/, const MFrom mask) {
  // No need to check lane size/LMUL are the same: if not, casting MFrom to
  // MFromD<D> would fail.
  return mask;
}

// ------------------------------ VecFromMask

namespace detail {
#define HWY_RVV_VEC_FROM_MASK(BASE, CHAR, SEW, LMUL, SHIFT, MLEN, NAME, OP) \
  HWY_API HWY_RVV_V(BASE, SEW, LMUL)                                        \
      NAME(HWY_RVV_V(BASE, SEW, LMUL) v0, HWY_RVV_M(MLEN) m) {              \
    return v##OP##_##CHAR##SEW##LMUL##_m(m, v0, v0, 1);                     \
  }

HWY_RVV_FOREACH_UI(HWY_RVV_VEC_FROM_MASK, SubS, sub_vx)
#undef HWY_RVV_VEC_FROM_MASK
}  // namespace detail

template <class D, HWY_IF_NOT_FLOAT_D(D)>
HWY_API VFromD<D> VecFromMask(const D d, MFromD<D> mask) {
  return detail::SubS(Zero(d), mask);
}

template <class D, HWY_IF_FLOAT_D(D)>
HWY_API VFromD<D> VecFromMask(const D d, MFromD<D> mask) {
  return BitCast(d, VecFromMask(RebindToUnsigned<D>(), mask));
}

// ------------------------------ ZeroIfNegative
template <class V>
HWY_API V ZeroIfNegative(const V v) {
  const auto v0 = Zero(DFromV<V>());
  // We already have a zero constant, so avoid IfThenZeroElse.
  return IfThenElse(Lt(v, v0), v0, v);
}

// ------------------------------ BroadcastSignBit
template <class V>
HWY_API V BroadcastSignBit(const V v) {
  return ShiftRight<sizeof(TFromV<V>) * 8 - 1>(v);
}

// ------------------------------ AllFalse

#define HWY_RVV_ALL_FALSE(MLEN, NAME, OP)                     \
  template <class D>                                          \
  HWY_API bool AllFalse(const D d, const HWY_RVV_M(MLEN) m) { \
    static_assert(MLenFromD(d) == MLEN, "Type mismatch");     \
    return vfirst_m_b##MLEN(m) < 0;                           \
  }                                                           \
  /* DEPRECATED */                                            \
  HWY_API bool AllFalse(const HWY_RVV_M(MLEN) m) {            \
    return vfirst_m_b##MLEN(m) < 0;                           \
  }
HWY_RVV_FOREACH_B(HWY_RVV_ALL_FALSE, _, _)
#undef HWY_RVV_ALL_FALSE

// ------------------------------ AllTrue

#define HWY_RVV_ALL_TRUE(MLEN, NAME, OP)                  \
  template <class D>                                      \
  HWY_API bool AllTrue(D d, HWY_RVV_M(MLEN) m) {          \
    static_assert(MLenFromD(d) == MLEN, "Type mismatch"); \
    return AllFalse(vmnot_m_b##MLEN(m));                  \
  }                                                       \
  /* DEPRECATED */                                        \
  HWY_API bool AllTrue(HWY_RVV_M(MLEN) m) {               \
    return AllFalse(vmnot_m_b##MLEN(m));                  \
  }

HWY_RVV_FOREACH_B(HWY_RVV_ALL_TRUE, _, _)
#undef HWY_RVV_ALL_TRUE

// ------------------------------ CountTrue

#define HWY_RVV_COUNT_TRUE(MLEN, NAME, OP)                \
  template <class D>                                      \
  HWY_API size_t CountTrue(D d, HWY_RVV_M(MLEN) m) {      \
    static_assert(MLenFromD(d) == MLEN, "Type mismatch"); \
    return vpopc_m_b##MLEN(m);                            \
  }                                                       \
  /* DEPRECATED */                                        \
  HWY_API size_t CountTrue(HWY_RVV_M(MLEN) m) { return vpopc_m_b##MLEN(m); }

HWY_RVV_FOREACH_B(HWY_RVV_COUNT_TRUE, _, _)
#undef HWY_RVV_COUNT_TRUE

// ------------------------------ FindFirstTrue

#define HWY_RVV_FIND_FIRST_TRUE(MLEN, NAME, OP)           \
  template <class D>                                      \
  HWY_API intptr_t FindFirstTrue(D d, HWY_RVV_M(MLEN) m) { \
    static_assert(MLenFromD(d) == MLEN, "Type mismatch"); \
    return vfirst_m_b##MLEN(m);                           \
  }

HWY_RVV_FOREACH_B(HWY_RVV_FIND_FIRST_TRUE, _, _)
#undef HWY_RVV_FIND_FIRST_TRUE

// ================================================== MEMORY

// ------------------------------ Load

#define HWY_RVV_LOAD(BASE, CHAR, SEW, LMUL, SHIFT, MLEN, NAME, OP) \
  HWY_API HWY_RVV_V(BASE, SEW, LMUL)                               \
      NAME(HWY_RVV_D(CHAR, SEW, LMUL) d,                           \
           const HWY_RVV_T(BASE, SEW) * HWY_RESTRICT p) {          \
    (void)Lanes(d);                                                \
    return v##OP##SEW##_v_##CHAR##SEW##LMUL(p);                    \
  }
HWY_RVV_FOREACH(HWY_RVV_LOAD, Load, le)
#undef HWY_RVV_LOAD

// Partial
template <typename T, size_t N, HWY_IF_LE128(T, N)>
HWY_API VFromD<Simd<T, N>> Load(Simd<T, N> d, const T* HWY_RESTRICT p) {
  return Load(d, p);
}

// ------------------------------ MaskedLoad

#define HWY_RVV_MASKED_LOAD(BASE, CHAR, SEW, LMUL, SHIFT, MLEN, NAME, OP) \
  HWY_API HWY_RVV_V(BASE, SEW, LMUL)                                      \
      NAME(HWY_RVV_M(MLEN) m, HWY_RVV_D(CHAR, SEW, LMUL) d,               \
           const HWY_RVV_T(BASE, SEW) * HWY_RESTRICT p) {                 \
    (void)Lanes(d);                                                       \
    return v##OP##SEW##_v_##CHAR##SEW##LMUL##_m(m, Zero(d), p);           \
  }
HWY_RVV_FOREACH(HWY_RVV_MASKED_LOAD, MaskedLoad, le)
#undef HWY_RVV_MASKED_LOAD

// ------------------------------ LoadU

// RVV only requires lane alignment, not natural alignment of the entire vector.
template <class D>
HWY_API VFromD<D> LoadU(D d, const TFromD<D>* HWY_RESTRICT p) {
  return Load(d, p);
}

// ------------------------------ Store

#define HWY_RVV_RET_ARGVDP(BASE, CHAR, SEW, LMUL, SHIFT, MLEN, NAME, OP) \
  HWY_API void NAME(HWY_RVV_V(BASE, SEW, LMUL) v,                        \
                    HWY_RVV_D(CHAR, SEW, LMUL) d,                        \
                    HWY_RVV_T(BASE, SEW) * HWY_RESTRICT p) {             \
    (void)Lanes(d);                                                      \
    return v##OP##SEW##_v_##CHAR##SEW##LMUL(p, v);                       \
  }
HWY_RVV_FOREACH(HWY_RVV_RET_ARGVDP, Store, se)
#undef HWY_RVV_RET_ARGVDP

// Partial
template <typename T, size_t N, HWY_IF_LE128(T, N)>
HWY_API void Store(VFromD<Simd<T, N>> v, Simd<T, N> d, T* HWY_RESTRICT p) {
  return Store(v, Full<T>(), p);
}

// ------------------------------ StoreU

// RVV only requires lane alignment, not natural alignment of the entire vector.
template <class V, class D>
HWY_API void StoreU(const V v, D d, TFromD<D>* HWY_RESTRICT p) {
  Store(v, d, p);
}

// ------------------------------ Stream
template <class V, class D, typename T>
HWY_API void Stream(const V v, D d, T* HWY_RESTRICT aligned) {
  Store(v, d, aligned);
}

// ------------------------------ ScatterOffset

#define HWY_RVV_SCATTER(BASE, CHAR, SEW, LMUL, SHIFT, MLEN, NAME, OP) \
  HWY_API void NAME(HWY_RVV_V(BASE, SEW, LMUL) v,                     \
                    HWY_RVV_D(CHAR, SEW, LMUL) /* d */,               \
                    HWY_RVV_T(BASE, SEW) * HWY_RESTRICT base,         \
                    HWY_RVV_V(int, SEW, LMUL) offset) {               \
    return v##OP##ei##SEW##_v_##CHAR##SEW##LMUL(                      \
        base, detail::BitCastToUnsigned(offset), v);                  \
  }
HWY_RVV_FOREACH(HWY_RVV_SCATTER, ScatterOffset, sx)
#undef HWY_RVV_SCATTER

// Partial
template <typename T, size_t N, HWY_IF_LE128(T, N)>
HWY_API void ScatterOffset(VFromD<Simd<T, N>> v, Simd<T, N> d,
                           T* HWY_RESTRICT base,
                           VFromD<Simd<MakeSigned<T>, N>> offset) {
  return ScatterOffset(v, Full<T>(), base, offset);
}

// ------------------------------ ScatterIndex

template <class D, HWY_IF_LANE_SIZE_D(D, 4)>
HWY_API void ScatterIndex(VFromD<D> v, D d, TFromD<D>* HWY_RESTRICT base,
                          const VFromD<RebindToSigned<D>> index) {
  return ScatterOffset(v, d, base, ShiftLeft<2>(index));
}

template <class D, HWY_IF_LANE_SIZE_D(D, 8)>
HWY_API void ScatterIndex(VFromD<D> v, D d, TFromD<D>* HWY_RESTRICT base,
                          const VFromD<RebindToSigned<D>> index) {
  return ScatterOffset(v, d, base, ShiftLeft<3>(index));
}

// ------------------------------ GatherOffset

#define HWY_RVV_GATHER(BASE, CHAR, SEW, LMUL, SHIFT, MLEN, NAME, OP) \
  HWY_API HWY_RVV_V(BASE, SEW, LMUL)                                 \
      NAME(HWY_RVV_D(CHAR, SEW, LMUL) /* d */,                       \
           const HWY_RVV_T(BASE, SEW) * HWY_RESTRICT base,           \
           HWY_RVV_V(int, SEW, LMUL) offset) {                       \
    return v##OP##ei##SEW##_v_##CHAR##SEW##LMUL(                     \
        base, detail::BitCastToUnsigned(offset));                    \
  }
HWY_RVV_FOREACH(HWY_RVV_GATHER, GatherOffset, lx)
#undef HWY_RVV_GATHER

// Partial
template <typename T, size_t N, HWY_IF_LE128(T, N)>
HWY_API VFromD<Simd<T, N>> GatherOffset(Simd<T, N> d,
                                        const T* HWY_RESTRICT base,
                                        VFromD<Simd<MakeSigned<T>, N>> offset) {
  return GatherOffset(Full<T>(), base, offset);
}

// ------------------------------ GatherIndex

template <class D, HWY_IF_LANE_SIZE_D(D, 4)>
HWY_API VFromD<D> GatherIndex(D d, const TFromD<D>* HWY_RESTRICT base,
                              const VFromD<RebindToSigned<D>> index) {
  return GatherOffset(d, base, ShiftLeft<2>(index));
}

template <class D, HWY_IF_LANE_SIZE_D(D, 8)>
HWY_API VFromD<D> GatherIndex(D d, const TFromD<D>* HWY_RESTRICT base,
                              const VFromD<RebindToSigned<D>> index) {
  return GatherOffset(d, base, ShiftLeft<3>(index));
}

// ------------------------------ StoreInterleaved3

#define HWY_RVV_STORE3(BASE, CHAR, SEW, LMUL, SHIFT, MLEN, NAME, OP)    \
  HWY_API void NAME(                                                    \
      HWY_RVV_V(BASE, SEW, LMUL) a, HWY_RVV_V(BASE, SEW, LMUL) b,       \
      HWY_RVV_V(BASE, SEW, LMUL) c, HWY_RVV_D(CHAR, SEW, LMUL) /* d */, \
      HWY_RVV_T(BASE, SEW) * HWY_RESTRICT unaligned) {                  \
    const v##BASE##SEW##LMUL##x3_t triple =                             \
        vcreate_##CHAR##SEW##LMUL##x3(a, b, c);                         \
    return v##OP##e8_v_##CHAR##SEW##LMUL##x3(unaligned, triple);        \
  }
// Segments are limited to 8 registers, so we can only go up to LMUL=2.
HWY_RVV_STORE3(uint, u, 8, m1, /*kShift=*/0, 8, StoreInterleaved3, sseg3)
HWY_RVV_STORE3(uint, u, 8, m2, /*kShift=*/1, 4, StoreInterleaved3, sseg3)

#undef HWY_RVV_STORE3

// Partial
template <typename T, size_t N, HWY_IF_LE128(T, N)>
HWY_API void StoreInterleaved3(VFromD<Simd<T, N>> v0, VFromD<Simd<T, N>> v1,
                               VFromD<Simd<T, N>> v2, Simd<T, N> /*tag*/,
                               T* unaligned) {
  return StoreInterleaved3(v0, v1, v2, Full<T>(), unaligned);
}

// ------------------------------ StoreInterleaved4

#define HWY_RVV_STORE4(BASE, CHAR, SEW, LMUL, SHIFT, MLEN, NAME, OP) \
  HWY_API void NAME(                                                 \
      HWY_RVV_V(BASE, SEW, LMUL) v0, HWY_RVV_V(BASE, SEW, LMUL) v1,  \
      HWY_RVV_V(BASE, SEW, LMUL) v2, HWY_RVV_V(BASE, SEW, LMUL) v3,  \
      HWY_RVV_D(CHAR, SEW, LMUL) /* d */,                            \
      HWY_RVV_T(BASE, SEW) * HWY_RESTRICT aligned) {                 \
    const v##BASE##SEW##LMUL##x4_t quad =                            \
        vcreate_##CHAR##SEW##LMUL##x4(v0, v1, v2, v3);               \
    return v##OP##e8_v_##CHAR##SEW##LMUL##x4(aligned, quad);         \
  }
// Segments are limited to 8 registers, so we can only go up to LMUL=2.
HWY_RVV_STORE4(uint, u, 8, m1, /*kShift=*/0, 8, StoreInterleaved4, sseg4)
HWY_RVV_STORE4(uint, u, 8, m2, /*kShift=*/1, 4, StoreInterleaved4, sseg4)

#undef HWY_RVV_STORE4

// Partial
template <typename T, size_t N, HWY_IF_LE128(T, N)>
HWY_API void StoreInterleaved4(VFromD<Simd<T, N>> v0, VFromD<Simd<T, N>> v1,
                               VFromD<Simd<T, N>> v2, VFromD<Simd<T, N>> v3,
                               Simd<T, N> /*tag*/, T* unaligned) {
  return StoreInterleaved4(v0, v1, v2, v3, Full<T>(), unaligned);
}

// ================================================== CONVERT

#define HWY_RVV_PROMOTE(OP, BASE, CHAR, BITS, BASE_IN, BITS_IN, LMUL, LMUL_IN) \
  HWY_API HWY_RVV_V(BASE, BITS, LMUL)                                          \
      PromoteTo(HWY_RVV_D(CHAR, BITS, LMUL) /*d*/,                             \
                HWY_RVV_V(BASE_IN, BITS_IN, LMUL_IN) v) {                      \
    return OP##CHAR##BITS##LMUL(v);                                            \
  }

// TODO(janwas): GCC does not yet support fractional LMUL
#define HWY_RVV_PROMOTE_X2(OP, BASE, CHAR, BITS, BASE_IN, BITS_IN)     \
  /*HWY_RVV_PROMOTE(OP, BASE, CHAR, BITS, BASE_IN, BITS_IN, m1, mf2)*/ \
  HWY_RVV_PROMOTE(OP, BASE, CHAR, BITS, BASE_IN, BITS_IN, m2, m1)      \
  HWY_RVV_PROMOTE(OP, BASE, CHAR, BITS, BASE_IN, BITS_IN, m4, m2)      \
  HWY_RVV_PROMOTE(OP, BASE, CHAR, BITS, BASE_IN, BITS_IN, m8, m4)

#define HWY_RVV_PROMOTE_X4(OP, BASE, CHAR, BITS, BASE_IN, BITS_IN)     \
  /*HWY_RVV_PROMOTE(OP, BASE, CHAR, BITS, BASE_IN, BITS_IN, m1, mf4)*/ \
  /*HWY_RVV_PROMOTE(OP, BASE, CHAR, BITS, BASE_IN, BITS_IN, m2, mf2)*/ \
  HWY_RVV_PROMOTE(OP, BASE, CHAR, BITS, BASE_IN, BITS_IN, m4, m1)      \
  HWY_RVV_PROMOTE(OP, BASE, CHAR, BITS, BASE_IN, BITS_IN, m8, m2)

// ------------------------------ PromoteTo

HWY_RVV_PROMOTE_X2(vzext_vf2_, uint, u, 16, uint, 8)
HWY_RVV_PROMOTE_X2(vzext_vf2_, uint, u, 32, uint, 16)
HWY_RVV_PROMOTE_X2(vzext_vf2_, uint, u, 64, uint, 32)
HWY_RVV_PROMOTE_X4(vzext_vf4_, uint, u, 32, uint, 8)

HWY_RVV_PROMOTE_X2(vsext_vf2_, int, i, 16, int, 8)
HWY_RVV_PROMOTE_X2(vsext_vf2_, int, i, 32, int, 16)
HWY_RVV_PROMOTE_X2(vsext_vf2_, int, i, 64, int, 32)
HWY_RVV_PROMOTE_X4(vsext_vf4_, int, i, 32, int, 8)

HWY_RVV_PROMOTE_X2(vfwcvt_f_f_v_, float, f, 32, float, 16)
HWY_RVV_PROMOTE_X2(vfwcvt_f_f_v_, float, f, 64, float, 32)

// i32 to f64
HWY_RVV_PROMOTE_X2(vfwcvt_f_x_v_, float, f, 64, int, 32)

#undef HWY_RVV_PROMOTE_X4
#undef HWY_RVV_PROMOTE_X2
#undef HWY_RVV_PROMOTE

template <size_t N>
HWY_API VFromD<Simd<int16_t, N>> PromoteTo(Simd<int16_t, N> d,
                                           VFromD<Simd<uint8_t, N>> v) {
  return BitCast(d, PromoteTo(Simd<uint16_t, N>(), v));
}

template <size_t N>
HWY_API VFromD<Simd<int32_t, N>> PromoteTo(Simd<int32_t, N> d,
                                           VFromD<Simd<uint8_t, N>> v) {
  return BitCast(d, PromoteTo(Simd<uint32_t, N>(), v));
}

template <size_t N>
HWY_API VFromD<Simd<int32_t, N>> PromoteTo(Simd<int32_t, N> d,
                                           VFromD<Simd<uint16_t, N>> v) {
  return BitCast(d, PromoteTo(Simd<uint32_t, N>(), v));
}

// ------------------------------ DemoteTo U

// First clamp negative numbers to zero to match x86 packus.
HWY_API Vu16m1 DemoteTo(Du16m1 /* d */, const Vi32m2 v) {
  return vnclipu_wx_u16m1(detail::BitCastToUnsigned(detail::Max(v, 0)), 0);
}
HWY_API Vu16m2 DemoteTo(Du16m2 /* d */, const Vi32m4 v) {
  return vnclipu_wx_u16m2(detail::BitCastToUnsigned(detail::Max(v, 0)), 0);
}
HWY_API Vu16m4 DemoteTo(Du16m4 /* d */, const Vi32m8 v) {
  return vnclipu_wx_u16m4(detail::BitCastToUnsigned(detail::Max(v, 0)), 0);
}

HWY_API Vu8m1 DemoteTo(Du8m1 /* d */, const Vi32m4 v) {
  return vnclipu_wx_u8m1(DemoteTo(Du16m2(), v), 0);
}
HWY_API Vu8m2 DemoteTo(Du8m2 /* d */, const Vi32m8 v) {
  return vnclipu_wx_u8m2(DemoteTo(Du16m4(), v), 0);
}

HWY_API Vu8m1 DemoteTo(Du8m1 /* d */, const Vi16m2 v) {
  return vnclipu_wx_u8m1(detail::BitCastToUnsigned(detail::Max(v, 0)), 0);
}
HWY_API Vu8m2 DemoteTo(Du8m2 /* d */, const Vi16m4 v) {
  return vnclipu_wx_u8m2(detail::BitCastToUnsigned(detail::Max(v, 0)), 0);
}
HWY_API Vu8m4 DemoteTo(Du8m4 /* d */, const Vi16m8 v) {
  return vnclipu_wx_u8m4(detail::BitCastToUnsigned(detail::Max(v, 0)), 0);
}

HWY_API Vu8m1 U8FromU32(const Vu32m4 v) {
  return vnclipu_wx_u8m1(vnclipu_wx_u16m2(v, 0), 0);
}
HWY_API Vu8m2 U8FromU32(const Vu32m8 v) {
  return vnclipu_wx_u8m2(vnclipu_wx_u16m4(v, 0), 0);
}

// ------------------------------ DemoteTo I

HWY_API Vi8m1 DemoteTo(Di8m1 /* d */, const Vi16m2 v) {
  return vnclip_wx_i8m1(v, 0);
}
HWY_API Vi8m2 DemoteTo(Di8m2 /* d */, const Vi16m4 v) {
  return vnclip_wx_i8m2(v, 0);
}
HWY_API Vi8m4 DemoteTo(Di8m4 /* d */, const Vi16m8 v) {
  return vnclip_wx_i8m4(v, 0);
}

HWY_API Vi16m1 DemoteTo(Di16m1 /* d */, const Vi32m2 v) {
  return vnclip_wx_i16m1(v, 0);
}
HWY_API Vi16m2 DemoteTo(Di16m2 /* d */, const Vi32m4 v) {
  return vnclip_wx_i16m2(v, 0);
}
HWY_API Vi16m4 DemoteTo(Di16m4 /* d */, const Vi32m8 v) {
  return vnclip_wx_i16m4(v, 0);
}

HWY_API Vi8m1 DemoteTo(Di8m1 d, const Vi32m4 v) {
  return DemoteTo(d, DemoteTo(Di16m2(), v));
}
HWY_API Vi8m2 DemoteTo(Di8m2 d, const Vi32m8 v) {
  return DemoteTo(d, DemoteTo(Di16m4(), v));
}

// ------------------------------ DemoteTo F

HWY_API Vf16m1 DemoteTo(Df16m1 /* d */, const Vf32m2 v) {
  return vfncvt_rod_f_f_w_f16m1(v);
}
HWY_API Vf16m2 DemoteTo(Df16m2 /* d */, const Vf32m4 v) {
  return vfncvt_rod_f_f_w_f16m2(v);
}
HWY_API Vf16m4 DemoteTo(Df16m4 /* d */, const Vf32m8 v) {
  return vfncvt_rod_f_f_w_f16m4(v);
}

HWY_API Vf32m1 DemoteTo(Df32m1 /* d */, const Vf64m2 v) {
  return vfncvt_rod_f_f_w_f32m1(v);
}
HWY_API Vf32m2 DemoteTo(Df32m2 /* d */, const Vf64m4 v) {
  return vfncvt_rod_f_f_w_f32m2(v);
}
HWY_API Vf32m4 DemoteTo(Df32m4 /* d */, const Vf64m8 v) {
  return vfncvt_rod_f_f_w_f32m4(v);
}

HWY_API Vi32m1 DemoteTo(Di32m1 /* d */, const Vf64m2 v) {
  return vfncvt_rtz_x_f_w_i32m1(v);
}
HWY_API Vi32m2 DemoteTo(Di32m2 /* d */, const Vf64m4 v) {
  return vfncvt_rtz_x_f_w_i32m2(v);
}
HWY_API Vi32m4 DemoteTo(Di32m4 /* d */, const Vf64m8 v) {
  return vfncvt_rtz_x_f_w_i32m4(v);
}

// ------------------------------ ConvertTo F

#define HWY_RVV_CONVERT(BASE, CHAR, SEW, LMUL, SHIFT, MLEN, NAME, OP)          \
  HWY_API HWY_RVV_V(BASE, SEW, LMUL) ConvertTo(                                \
      HWY_RVV_D(CHAR, SEW, LMUL) /* d */, HWY_RVV_V(int, SEW, LMUL) v) {       \
    return vfcvt_f_x_v_f##SEW##LMUL(v);                                        \
  }                                                                            \
  /* Truncates (rounds toward zero). */                                        \
  HWY_API HWY_RVV_V(int, SEW, LMUL) ConvertTo(HWY_RVV_D(i, SEW, LMUL) /* d */, \
                                              HWY_RVV_V(BASE, SEW, LMUL) v) {  \
    return vfcvt_rtz_x_f_v_i##SEW##LMUL(v);                                    \
  }                                                                            \
  /* Uses default rounding mode. */                                            \
  HWY_API HWY_RVV_V(int, SEW, LMUL) NearestInt(HWY_RVV_V(BASE, SEW, LMUL) v) { \
    return vfcvt_x_f_v_i##SEW##LMUL(v);                                        \
  }

// API only requires f32 but we provide f64 for internal use (otherwise, it
// seems difficult to implement Iota without a _mf2 vector half).
HWY_RVV_FOREACH_F(HWY_RVV_CONVERT, _, _)
#undef HWY_RVV_CONVERT

// Partial
template <typename T, size_t N, class FromV, HWY_IF_LE128(T, N)>
HWY_API VFromD<Simd<T, N>> ConvertTo(Simd<T, N> /*tag*/, FromV v) {
  return ConvertTo(Full<T>(), v);
}

// ================================================== COMBINE

namespace detail {

// For x86-compatible behaviour mandated by Highway API: TableLookupBytes
// offsets are implicitly relative to the start of their 128-bit block.
template <typename T, size_t N>
constexpr size_t LanesPerBlock(Simd<T, N> /* tag */) {
  // Also cap to the limit imposed by D (for fixed-size <= 128-bit vectors).
  return HWY_MIN(16 / sizeof(T), N);
}

template <class D, class V>
HWY_INLINE V OffsetsOf128BitBlocks(const D d, const V iota0) {
  using T = MakeUnsigned<TFromD<D>>;
  return AndS(iota0, static_cast<T>(~(LanesPerBlock(d) - 1)));
}

template <size_t kLanes, class D>
HWY_INLINE MFromD<D> FirstNPerBlock(D /* tag */) {
  const RebindToUnsigned<D> du;
  const RebindToSigned<D> di;
  constexpr size_t kLanesPerBlock = LanesPerBlock(du);
  const auto idx_mod = AndS(Iota0(du), kLanesPerBlock - 1);
  return LtS(BitCast(di, idx_mod), static_cast<TFromD<decltype(di)>>(kLanes));
}

// vector = f(vector, vector, size_t)
#define HWY_RVV_SLIDE(BASE, CHAR, SEW, LMUL, SHIFT, MLEN, NAME, OP)        \
  HWY_API HWY_RVV_V(BASE, SEW, LMUL)                                       \
      NAME(HWY_RVV_V(BASE, SEW, LMUL) dst, HWY_RVV_V(BASE, SEW, LMUL) src, \
           size_t lanes) {                                                 \
    return v##OP##_vx_##CHAR##SEW##LMUL(dst, src, lanes);                  \
  }

HWY_RVV_FOREACH(HWY_RVV_SLIDE, SlideUp, slideup)
HWY_RVV_FOREACH(HWY_RVV_SLIDE, SlideDown, slidedown)

#undef HWY_RVV_SLIDE

}  // namespace detail

// ------------------------------ ConcatUpperLower
template <class V>
HWY_API V ConcatUpperLower(const V hi, const V lo) {
  const RebindToSigned<DFromV<V>> di;
  return IfThenElse(FirstN(di, Lanes(di) / 2), lo, hi);
}

// ------------------------------ ConcatLowerLower
template <class V>
HWY_API V ConcatLowerLower(const V hi, const V lo) {
  return detail::SlideUp(lo, hi, Lanes(DFromV<V>()) / 2);
}

// ------------------------------ ConcatUpperUpper
template <class V>
HWY_API V ConcatUpperUpper(const V hi, const V lo) {
  // Move upper half into lower
  const auto lo_down = detail::SlideDown(lo, lo, Lanes(DFromV<V>()) / 2);
  return ConcatUpperLower(hi, lo_down);
}

// ------------------------------ ConcatLowerUpper
template <class V>
HWY_API V ConcatLowerUpper(const V hi, const V lo) {
  // Move half of both inputs to the other half
  const auto hi_up = detail::SlideUp(hi, hi, Lanes(DFromV<V>()) / 2);
  const auto lo_down = detail::SlideDown(lo, lo, Lanes(DFromV<V>()) / 2);
  return ConcatUpperLower(hi_up, lo_down);
}

// ------------------------------ Combine

// TODO(janwas): implement after LMUL ext/trunc
#if 0

template <class V>
HWY_API V Combine(const V a, const V b) {
  using D = DFromV<V>;
  // double LMUL of inputs, then SlideUp with Lanes().
}

#endif

// ================================================== SWIZZLE

// ------------------------------ GetLane

#define HWY_RVV_GET_LANE(BASE, CHAR, SEW, LMUL, SHIFT, MLEN, NAME, OP) \
  HWY_API HWY_RVV_T(BASE, SEW) NAME(HWY_RVV_V(BASE, SEW, LMUL) v) {    \
    return v##OP##_s_##CHAR##SEW##LMUL##_##CHAR##SEW(v);               \
  }

HWY_RVV_FOREACH_UI(HWY_RVV_GET_LANE, GetLane, mv_x)
HWY_RVV_FOREACH_F(HWY_RVV_GET_LANE, GetLane, fmv_f)
#undef HWY_RVV_GET_LANE

// ------------------------------ OddEven
template <class V>
HWY_API V OddEven(const V a, const V b) {
  const RebindToUnsigned<DFromV<V>> du;  // Iota0 is unsigned only
  const auto is_even = Eq(detail::AndS(detail::Iota0(du), 1), Zero(du));
  return IfThenElse(is_even, b, a);
}

// ------------------------------ TableLookupLanes

template <class D, class DU = RebindToUnsigned<D>>
HWY_API VFromD<DU> SetTableIndices(D d, const TFromD<DU>* idx) {
#if HWY_IS_DEBUG_BUILD
  const size_t N = Lanes(d);
  for (size_t i = 0; i < N; ++i) {
    HWY_DASSERT(0 <= idx[i] && idx[i] < static_cast<TFromD<DU>>(N));
  }
  #else
  (void)d;
#endif
  return Load(DU(), idx);
}

// <32bit are not part of Highway API, but used in Broadcast. This limits VLMAX
// to 2048! We could instead use vrgatherei16.
#define HWY_RVV_TABLE(BASE, CHAR, SEW, LMUL, SHIFT, MLEN, NAME, OP)        \
  HWY_API HWY_RVV_V(BASE, SEW, LMUL)                                       \
      NAME(HWY_RVV_V(BASE, SEW, LMUL) v, HWY_RVV_V(uint, SEW, LMUL) idx) { \
    return v##OP##_vv_##CHAR##SEW##LMUL(v, idx);                           \
  }

HWY_RVV_FOREACH(HWY_RVV_TABLE, TableLookupLanes, rgather)
#undef HWY_RVV_TABLE

// ------------------------------ Compress

#define HWY_RVV_COMPRESS(BASE, CHAR, SEW, LMUL, SHIFT, MLEN, NAME, OP) \
  HWY_API HWY_RVV_V(BASE, SEW, LMUL)                                   \
      NAME(HWY_RVV_V(BASE, SEW, LMUL) v, HWY_RVV_M(MLEN) mask) {       \
    Lanes(HWY_RVV_D(CHAR, SEW, LMUL)());                               \
    return v##OP##_vm_##CHAR##SEW##LMUL(mask, v, v);                   \
  }

HWY_RVV_FOREACH_UI16(HWY_RVV_COMPRESS, Compress, compress)
HWY_RVV_FOREACH_UI32(HWY_RVV_COMPRESS, Compress, compress)
HWY_RVV_FOREACH_UI64(HWY_RVV_COMPRESS, Compress, compress)
HWY_RVV_FOREACH_F(HWY_RVV_COMPRESS, Compress, compress)
#undef HWY_RVV_COMPRESS

// ------------------------------ CompressStore
template <class V, class M, class D>
HWY_API size_t CompressStore(const V v, const M mask, const D d,
                             TFromD<D>* HWY_RESTRICT unaligned) {
  StoreU(Compress(v, mask), d, unaligned);
  return CountTrue(d, mask);
}

// ================================================== BLOCKWISE

// ------------------------------ CombineShiftRightBytes
template <size_t kBytes, class D, class V = VFromD<D>>
HWY_API V CombineShiftRightBytes(const D d, const V hi, V lo) {
  const Repartition<uint8_t, decltype(d)> d8;
  Lanes(d8);
  const auto hi8 = BitCast(d8, hi);
  const auto lo8 = BitCast(d8, lo);
  const auto hi_up = detail::SlideUp(hi8, hi8, 16 - kBytes);
  const auto lo_down = detail::SlideDown(lo8, lo8, kBytes);
  const auto is_lo = detail::FirstNPerBlock<16 - kBytes>(d8);
  const auto combined = BitCast(d, IfThenElse(is_lo, lo_down, hi_up));
  Lanes(d);
  return combined;
}

// ------------------------------ CombineShiftRightLanes
template <size_t kLanes, class D, class V = VFromD<D>>
HWY_API V CombineShiftRightLanes(const D d, const V hi, V lo) {
  constexpr size_t kLanesUp = 16 / sizeof(TFromV<V>) - kLanes;
  const auto hi_up = detail::SlideUp(hi, hi, kLanesUp);
  const auto lo_down = detail::SlideDown(lo, lo, kLanes);
  const auto is_lo = detail::FirstNPerBlock<kLanesUp>(d);
  return IfThenElse(is_lo, lo_down, hi_up);
}

// ------------------------------ Shuffle2301 (ShiftLeft)
template <class V>
HWY_API V Shuffle2301(const V v) {
  const DFromV<V> d;
  static_assert(sizeof(TFromD<decltype(d)>) == 4, "Defined for 32-bit types");
  const Repartition<uint64_t, decltype(d)> du64;
  const auto v64 = BitCast(du64, v);
  Lanes(du64);
  const auto rotated = BitCast(d, Or(ShiftRight<32>(v64), ShiftLeft<32>(v64)));
  Lanes(d);
  return rotated;
}

// ------------------------------ Shuffle2103
template <class V>
HWY_API V Shuffle2103(const V v) {
  const DFromV<V> d;
  static_assert(sizeof(TFromD<decltype(d)>) == 4, "Defined for 32-bit types");
  return CombineShiftRightLanes<3>(d, v, v);
}

// ------------------------------ Shuffle0321
template <class V>
HWY_API V Shuffle0321(const V v) {
  const DFromV<V> d;
  static_assert(sizeof(TFromD<decltype(d)>) == 4, "Defined for 32-bit types");
  return CombineShiftRightLanes<1>(d, v, v);
}

// ------------------------------ Shuffle1032
template <class V>
HWY_API V Shuffle1032(const V v) {
  const DFromV<V> d;
  static_assert(sizeof(TFromD<decltype(d)>) == 4, "Defined for 32-bit types");
  return CombineShiftRightLanes<2>(d, v, v);
}

// ------------------------------ Shuffle01
template <class V>
HWY_API V Shuffle01(const V v) {
  const DFromV<V> d;
  static_assert(sizeof(TFromD<decltype(d)>) == 8, "Defined for 64-bit types");
  return CombineShiftRightLanes<1>(d, v, v);
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
  const Repartition<uint8_t, decltype(d)> d8;
  Lanes(d8);
  const auto offsets128 = detail::OffsetsOf128BitBlocks(d8, detail::Iota0(d8));
  const auto idx8 = Add(BitCast(d8, idx), offsets128);
  const auto out = BitCast(d, TableLookupLanes(BitCast(d8, v), idx8));
  Lanes(d);
  return out;
}

template <class V, class VI>
HWY_API V TableLookupBytesOr0(const VI v, const V idx) {
  const DFromV<VI> d;
  // Mask size must match vector type, so cast everything to this type.
  const Repartition<int8_t, decltype(d)> di8;
  Lanes(di8);
  const auto lookup = TableLookupBytes(BitCast(di8, v), BitCast(di8, idx));
  const auto msb = Lt(BitCast(di8, idx), Zero(di8));
  const auto out = BitCast(d, IfThenZeroElse(msb, lookup));
  Lanes(d);
  return out;
}

// ------------------------------ Broadcast
template <int kLane, class V>
HWY_API V Broadcast(const V v) {
  const DFromV<V> d;
  constexpr size_t kLanesPerBlock = detail::LanesPerBlock(d);
  static_assert(0 <= kLane && kLane < kLanesPerBlock, "Invalid lane");
  auto idx = detail::OffsetsOf128BitBlocks(d, detail::Iota0(d));
  if (kLane != 0) {
    idx = detail::AddS(idx, kLane);
  }
  return TableLookupLanes(v, idx);
}

// ------------------------------ ShiftLeftLanes

template <size_t kLanes, class D, class V = VFromD<D>>
HWY_API V ShiftLeftLanes(const D d, const V v) {
  const RebindToSigned<decltype(d)> di;
  const auto shifted = detail::SlideUp(v, v, kLanes);
  // Match x86 semantics by zeroing lower lanes in 128-bit blocks
  constexpr size_t kLanesPerBlock = detail::LanesPerBlock(di);
  const auto idx_mod = detail::AndS(detail::Iota0(di), kLanesPerBlock - 1);
  const auto clear = Lt(BitCast(di, idx_mod), Set(di, kLanes));
  return IfThenZeroElse(clear, shifted);
}

template <size_t kLanes, class V>
HWY_API V ShiftLeftLanes(const V v) {
  return ShiftLeftLanes<kLanes>(DFromV<V>(), v);
}

// ------------------------------ ShiftLeftBytes

template <int kBytes, class V>
HWY_API V ShiftLeftBytes(DFromV<V> d, const V v) {
  const Repartition<uint8_t, decltype(d)> d8;
  Lanes(d8);
  const auto shifted = BitCast(d, ShiftLeftLanes<kBytes>(BitCast(d8, v)));
  Lanes(d);
  return shifted;
}

template <int kBytes, class V>
HWY_API V ShiftLeftBytes(const V v) {
  return ShiftLeftBytes<kBytes>(DFromV<V>(), v);
}

// ------------------------------ ShiftRightLanes
template <size_t kLanes, typename T, size_t N, class V = VFromD<Simd<T, N>>>
HWY_API V ShiftRightLanes(const Simd<T, N> d, V v) {
  const RebindToSigned<decltype(d)> di;
  // For partial vectors, clear upper lanes so we shift in zeros.
  if (N <= 16 / sizeof(T)) {
    v = IfThenElseZero(FirstN(d, N), v);
  }

  const auto shifted = detail::SlideDown(v, v, kLanes);
  // Match x86 semantics by zeroing upper lanes in 128-bit blocks
  constexpr size_t kLanesPerBlock = detail::LanesPerBlock(di);
  const auto idx_mod = detail::AndS(detail::Iota0(di), kLanesPerBlock - 1);
  const auto keep = Lt(BitCast(di, idx_mod), Set(di, kLanesPerBlock - kLanes));
  return IfThenElseZero(keep, shifted);
}

// ------------------------------ ShiftRightBytes
template <int kBytes, class D, class V = VFromD<D>>
HWY_API V ShiftRightBytes(const D d, const V v) {
  const Repartition<uint8_t, decltype(d)> d8;
  Lanes(d8);
  const auto shifted = BitCast(d, ShiftRightLanes<kBytes>(d8, BitCast(d8, v)));
  Lanes(d);
  return shifted;
}

// ------------------------------ InterleaveLower

// TODO(janwas): PromoteTo(LowerHalf), slide1up, add
template <class D, class V>
HWY_API V InterleaveLower(D d, const V a, const V b) {
  static_assert(IsSame<TFromD<D>, TFromV<V>>(), "D/V mismatch");
  const RebindToUnsigned<decltype(d)> du;
  constexpr size_t kLanesPerBlock = detail::LanesPerBlock(du);
  const auto i = detail::Iota0(du);
  const auto idx_mod = ShiftRight<1>(detail::AndS(i, kLanesPerBlock - 1));
  const auto idx = Add(idx_mod, detail::OffsetsOf128BitBlocks(d, i));
  const auto is_even = Eq(detail::AndS(i, 1), Zero(du));
  return IfThenElse(is_even, TableLookupLanes(a, idx),
                    TableLookupLanes(b, idx));
}

template <class V>
HWY_API V InterleaveLower(const V a, const V b) {
  return InterleaveLower(DFromV<V>(), a, b);
}

// ------------------------------ InterleaveUpper

template <class D, class V>
HWY_API V InterleaveUpper(const D d, const V a, const V b) {
  static_assert(IsSame<TFromD<D>, TFromV<V>>(), "D/V mismatch");
  const RebindToUnsigned<decltype(d)> du;
  constexpr size_t kLanesPerBlock = detail::LanesPerBlock(du);
  const auto i = detail::Iota0(du);
  const auto idx_mod = ShiftRight<1>(detail::AndS(i, kLanesPerBlock - 1));
  const auto idx_lower = Add(idx_mod, detail::OffsetsOf128BitBlocks(d, i));
  const auto idx = detail::AddS(idx_lower, kLanesPerBlock / 2);
  const auto is_even = Eq(detail::AndS(i, 1), Zero(du));
  return IfThenElse(is_even, TableLookupLanes(a, idx),
                    TableLookupLanes(b, idx));
}

// ------------------------------ ZipLower

template <class V, class DW = RepartitionToWide<DFromV<V>>>
HWY_API VFromD<DW> ZipLower(DW dw, V a, V b) {
  const RepartitionToNarrow<DW> dn;
  static_assert(IsSame<TFromD<decltype(dn)>, TFromV<V>>(), "D/V mismatch");
  const auto zipped = BitCast(dw, InterleaveLower(dn, a, b));
  Lanes(dw);
  return zipped;
}
template <class V, class DW = RepartitionToWide<DFromV<V>>>
HWY_API VFromD<DW> ZipLower(const V a, const V b) {
  return ZipLower(DW(), a, b);
}

// ------------------------------ ZipUpper
template <class DW, class V>
HWY_API VFromD<DW> ZipUpper(DW dw, V a, V b) {
  const RepartitionToNarrow<DW> dn;
  static_assert(IsSame<TFromD<decltype(dn)>, TFromV<V>>(), "D/V mismatch");
  const auto zipped = BitCast(dw, InterleaveUpper(dn, a, b));
  Lanes(dw);
  return zipped;
}

// ================================================== REDUCE

// vector = f(vector, zero_m1)
#define HWY_RVV_REDUCE(BASE, CHAR, SEW, LMUL, SHIFT, MLEN, NAME, OP)         \
  HWY_API HWY_RVV_V(BASE, SEW, LMUL)                                         \
      NAME(HWY_RVV_V(BASE, SEW, LMUL) v, HWY_RVV_V(BASE, SEW, m1) v0) {      \
    vsetvlmax_e##SEW##LMUL();                                                \
    return Set(                                                              \
        HWY_RVV_D(CHAR, SEW, LMUL)(),                                        \
        GetLane(v##OP##_vs_##CHAR##SEW##LMUL##_##CHAR##SEW##m1(v0, v, v0))); \
  }

// ------------------------------ SumOfLanes

namespace detail {
HWY_RVV_FOREACH_UI(HWY_RVV_REDUCE, RedSum, redsum)
HWY_RVV_FOREACH_F(HWY_RVV_REDUCE, RedSum, fredsum)
}  // namespace detail

template <class D>
HWY_API VFromD<D> SumOfLanes(D /* d */, const VFromD<D> v) {
  const auto v0 = Zero(Full<TFromD<D>>());  // always m1
  return detail::RedSum(v, v0);
}

// ------------------------------ MinOfLanes
namespace detail {
HWY_RVV_FOREACH_U(HWY_RVV_REDUCE, RedMin, redminu)
HWY_RVV_FOREACH_I(HWY_RVV_REDUCE, RedMin, redmin)
HWY_RVV_FOREACH_F(HWY_RVV_REDUCE, RedMin, fredmin)
}  // namespace detail

template <class D>
HWY_API VFromD<D> MinOfLanes(D /* d */, const VFromD<D> v) {
  using T = TFromD<D>;
  const Full<T> d1;  // always m1
  const auto neutral = Set(d1, HighestValue<T>());
  return detail::RedMin(v, neutral);
}

// ------------------------------ MaxOfLanes
namespace detail {
HWY_RVV_FOREACH_U(HWY_RVV_REDUCE, RedMax, redmaxu)
HWY_RVV_FOREACH_I(HWY_RVV_REDUCE, RedMax, redmax)
HWY_RVV_FOREACH_F(HWY_RVV_REDUCE, RedMax, fredmax)
}  // namespace detail

template <class D>
HWY_API VFromD<D> MaxOfLanes(D /* d */, const VFromD<D> v) {
  using T = TFromD<D>;
  const Full<T> d1;  // always m1
  const auto neutral = Set(d1, LowestValue<T>());
  return detail::RedMax(v, neutral);
}

#undef HWY_RVV_REDUCE

// ================================================== Ops with dependencies

// ------------------------------ LoadDup128

template <class D>
HWY_API VFromD<D> LoadDup128(D d, const TFromD<D>* const HWY_RESTRICT p) {
  // TODO(janwas): set VL
  const auto loaded = Load(d, p);
  constexpr size_t kLanesPerBlock = detail::LanesPerBlock(d);
  // Broadcast the first block
  const auto idx = detail::AndS(detail::Iota0(d), kLanesPerBlock - 1);
  return TableLookupLanes(loaded, idx);
}

// ------------------------------ StoreMaskBits
#define HWY_RVV_STORE_MASK_BITS(MLEN, NAME, OP)                    \
  /* DEPRECATED */                                                 \
  HWY_API size_t StoreMaskBits(HWY_RVV_M(MLEN) m, uint8_t* bits) { \
    /* LMUL=1 is always enough */                                  \
    Full<uint8_t> d8;                                              \
    const size_t num_bytes = (Lanes(d8) + MLEN - 1) / MLEN;        \
    /* TODO(janwas): how to convert vbool* to vuint?*/             \
    /*Store(m, d8, bits);*/                                        \
    (void)m;                                                       \
    (void)bits;                                                    \
    return num_bytes;                                              \
  }                                                                \
  template <class D>                                               \
  HWY_API size_t StoreMaskBits(D /* tag */, HWY_RVV_M(MLEN) m,     \
                               uint8_t* bits) {                    \
    return StoreMaskBits(m, bits);                                 \
  }
HWY_RVV_FOREACH_B(HWY_RVV_STORE_MASK_BITS, _, _)
#undef HWY_RVV_STORE_MASK_BITS

// ------------------------------ FirstN (Iota0, Lt, RebindMask, SlideUp)

// Disallow for 8-bit because Iota is likely to overflow.
template <class D, HWY_IF_NOT_LANE_SIZE_D(D, 1)>
HWY_API MFromD<D> FirstN(const D d, const size_t n) {
  const RebindToSigned<D> di;
  return RebindMask(d, Lt(BitCast(di, detail::Iota0(d)), Set(di, n)));
}

template <class D, HWY_IF_LANE_SIZE_D(D, 1)>
HWY_API MFromD<D> FirstN(const D d, const size_t n) {
  const auto zero = Zero(d);
  const auto one = Set(d, 1);
  return Eq(detail::SlideUp(one, zero, n), one);
}

// ------------------------------ Neg (Sub)

template <class V, HWY_IF_SIGNED_V(V)>
HWY_API V Neg(const V v) {
  return Sub(Zero(DFromV<V>()), v);
}

// vector = f(vector), but argument is repeated
#define HWY_RVV_RETV_ARGV2(BASE, CHAR, SEW, LMUL, SHIFT, MLEN, NAME, OP)  \
  HWY_API HWY_RVV_V(BASE, SEW, LMUL) NAME(HWY_RVV_V(BASE, SEW, LMUL) v) { \
    return v##OP##_vv_##CHAR##SEW##LMUL(v, v);                            \
  }

HWY_RVV_FOREACH_F(HWY_RVV_RETV_ARGV2, Neg, fsgnjn)

// ------------------------------ Abs (Max, Neg)

template <class V, HWY_IF_SIGNED_V(V)>
HWY_API V Abs(const V v) {
  return Max(v, Neg(v));
}

HWY_RVV_FOREACH_F(HWY_RVV_RETV_ARGV2, Abs, fsgnjx)

#undef HWY_RVV_RETV_ARGV2

// ------------------------------ AbsDiff (Abs, Sub)
template <class V>
HWY_API V AbsDiff(const V a, const V b) {
  return Abs(Sub(a, b));
}

// ------------------------------ Round  (NearestInt, ConvertTo, CopySign)

// IEEE-754 roundToIntegralTiesToEven returns floating-point, but we do not have
// a dedicated instruction for that. Rounding to integer and converting back to
// float is correct except when the input magnitude is large, in which case the
// input was already an integer (because mantissa >> exponent is zero).

namespace detail {
enum RoundingModes { kNear, kTrunc, kDown, kUp };

template <class V>
HWY_INLINE auto UseInt(const V v) -> decltype(MaskFromVec(v)) {
  return Lt(Abs(v), Set(DFromV<V>(), MantissaEnd<TFromV<V>>()));
}

}  // namespace detail

template <class V>
HWY_API V Round(const V v) {
  const DFromV<V> df;

  const auto integer = NearestInt(v);  // round using current mode
  const auto int_f = ConvertTo(df, integer);

  return IfThenElse(detail::UseInt(v), CopySign(int_f, v), v);
}

// ------------------------------ Trunc (ConvertTo)
template <class V>
HWY_API V Trunc(const V v) {
  const DFromV<V> df;
  const RebindToSigned<decltype(df)> di;

  const auto integer = ConvertTo(di, v);  // round toward 0
  const auto int_f = ConvertTo(df, integer);

  return IfThenElse(detail::UseInt(v), CopySign(int_f, v), v);
}

// ------------------------------ Ceil
template <class V>
HWY_API V Ceil(const V v) {
  asm volatile("fsrm %0" ::"r"(detail::kUp));
  const auto ret = Round(v);
  asm volatile("fsrm %0" ::"r"(detail::kNear));
  return ret;
}

// ------------------------------ Floor
template <class V>
HWY_API V Floor(const V v) {
  asm volatile("fsrm %0" ::"r"(detail::kDown));
  const auto ret = Round(v);
  asm volatile("fsrm %0" ::"r"(detail::kNear));
  return ret;
}

// ------------------------------ Iota (ConvertTo)

template <class D, HWY_IF_UNSIGNED_D(D)>
HWY_API VFromD<D> Iota(const D d, TFromD<D> first) {
  return Add(detail::Iota0(d), Set(d, first));
}

template <class D, HWY_IF_SIGNED_D(D)>
HWY_API VFromD<D> Iota(const D d, TFromD<D> first) {
  const RebindToUnsigned<D> du;
  return Add(BitCast(d, detail::Iota0(du)), Set(d, first));
}

template <class D, HWY_IF_FLOAT_D(D)>
HWY_API VFromD<D> Iota(const D d, TFromD<D> first) {
  const RebindToUnsigned<D> du;
  const RebindToSigned<D> di;
  return detail::AddS(ConvertTo(d, BitCast(di, detail::Iota0(du))), first);
}

// ------------------------------ MulEven/Odd (Mul, OddEven)

namespace detail {
// Special instruction for 1 lane is presumably faster?
#define HWY_RVV_SLIDE1(BASE, CHAR, SEW, LMUL, SHIFT, MLEN, NAME, OP)      \
  HWY_API HWY_RVV_V(BASE, SEW, LMUL) NAME(HWY_RVV_V(BASE, SEW, LMUL) v) { \
    return v##OP##_vx_##CHAR##SEW##LMUL(v, 0);                            \
  }

HWY_RVV_FOREACH_UI32(HWY_RVV_SLIDE1, Slide1Up, slide1up)
HWY_RVV_FOREACH_U64(HWY_RVV_SLIDE1, Slide1Up, slide1up)
HWY_RVV_FOREACH_UI32(HWY_RVV_SLIDE1, Slide1Down, slide1down)
HWY_RVV_FOREACH_U64(HWY_RVV_SLIDE1, Slide1Down, slide1down)
#undef HWY_RVV_SLIDE1
}  // namespace detail

template <class V, HWY_IF_LANE_SIZE_V(V, 4)>
HWY_API VFromD<RepartitionToWide<DFromV<V>>> MulEven(const V a, const V b) {
  const DFromV<V> d;
  Lanes(d);
  const auto lo = Mul(a, b);
  const auto hi = detail::MulHigh(a, b);
  const RepartitionToWide<DFromV<V>> dw;
  const auto wide = BitCast(dw, OddEven(detail::Slide1Up(hi), lo));
  Lanes(dw);
  return wide;
}

// There is no 64x64 vwmul.
template <class V, HWY_IF_LANE_SIZE_V(V, 8)>
HWY_INLINE V MulEven(const V a, const V b) {
  const auto lo = detail::Mul(a, b);
  const auto hi = detail::MulHigh(a, b);
  return OddEven(detail::Slide1Up(hi), lo);
}

template <class V, HWY_IF_LANE_SIZE_V(V, 8)>
HWY_INLINE V MulOdd(const V a, const V b) {
  const auto lo = detail::Mul(a, b);
  const auto hi = detail::MulHigh(a, b);
  return OddEven(hi, detail::Slide1Down(lo));
}

// ================================================== END MACROS
namespace detail {  // for code folding
#undef HWY_IF_FLOAT_V
#undef HWY_IF_SIGNED_V
#undef HWY_IF_UNSIGNED_V

#undef HWY_RVV_FOREACH
#undef HWY_RVV_FOREACH_08
#undef HWY_RVV_FOREACH_16
#undef HWY_RVV_FOREACH_32
#undef HWY_RVV_FOREACH_64
#undef HWY_RVV_FOREACH_B
#undef HWY_RVV_FOREACH_F
#undef HWY_RVV_FOREACH_F32
#undef HWY_RVV_FOREACH_F64
#undef HWY_RVV_FOREACH_I
#undef HWY_RVV_FOREACH_I08
#undef HWY_RVV_FOREACH_I16
#undef HWY_RVV_FOREACH_I32
#undef HWY_RVV_FOREACH_I64
#undef HWY_RVV_FOREACH_U
#undef HWY_RVV_FOREACH_U08
#undef HWY_RVV_FOREACH_U16
#undef HWY_RVV_FOREACH_U32
#undef HWY_RVV_FOREACH_U64
#undef HWY_RVV_FOREACH_UI
#undef HWY_RVV_FOREACH_UI16
#undef HWY_RVV_FOREACH_UI32
#undef HWY_RVV_FOREACH_UI64

#undef HWY_RVV_RETV_ARGD
#undef HWY_RVV_RETV_ARGV
#undef HWY_RVV_RETV_ARGVS
#undef HWY_RVV_RETV_ARGVV

#undef HWY_RVV_T
#undef HWY_RVV_D
#undef HWY_RVV_V
#undef HWY_RVV_M

}  // namespace detail
// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace hwy
HWY_AFTER_NAMESPACE();
