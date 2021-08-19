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
using DFromV = typename DFromV_t<V>::type;

template <class V>
using TFromV = TFromD<DFromV<V>>;

#define HWY_IF_UNSIGNED_V(V) hwy::EnableIf<!IsSigned<TFromV<V>>()>* = nullptr
#define HWY_IF_SIGNED_V(V) \
  hwy::EnableIf<IsSigned<TFromV<V>>() && !IsFloat<TFromV<V>>()>* = nullptr
#define HWY_IF_FLOAT_V(V) hwy::EnableIf<IsFloat<TFromV<V>>()>* = nullptr

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
#define HWY_RVV_FOREACH_08(X_MACRO, BASE, CHAR, NAME, OP) \
  X_MACRO(BASE, CHAR, 8, 1, 8, NAME, OP)                  \
  X_MACRO(BASE, CHAR, 8, 2, 4, NAME, OP)                  \
  X_MACRO(BASE, CHAR, 8, 4, 2, NAME, OP)                  \
  X_MACRO(BASE, CHAR, 8, 8, 1, NAME, OP)

#define HWY_RVV_FOREACH_16(X_MACRO, BASE, CHAR, NAME, OP) \
  X_MACRO(BASE, CHAR, 16, 1, 16, NAME, OP)                \
  X_MACRO(BASE, CHAR, 16, 2, 8, NAME, OP)                 \
  X_MACRO(BASE, CHAR, 16, 4, 4, NAME, OP)                 \
  X_MACRO(BASE, CHAR, 16, 8, 2, NAME, OP)

#define HWY_RVV_FOREACH_32(X_MACRO, BASE, CHAR, NAME, OP) \
  X_MACRO(BASE, CHAR, 32, 1, 32, NAME, OP)                \
  X_MACRO(BASE, CHAR, 32, 2, 16, NAME, OP)                \
  X_MACRO(BASE, CHAR, 32, 4, 8, NAME, OP)                 \
  X_MACRO(BASE, CHAR, 32, 8, 4, NAME, OP)

#define HWY_RVV_FOREACH_64(X_MACRO, BASE, CHAR, NAME, OP) \
  X_MACRO(BASE, CHAR, 64, 1, 64, NAME, OP)                \
  X_MACRO(BASE, CHAR, 64, 2, 32, NAME, OP)                \
  X_MACRO(BASE, CHAR, 64, 4, 16, NAME, OP)                \
  X_MACRO(BASE, CHAR, 64, 8, 8, NAME, OP)

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
#define HWY_RVV_D(CHAR, SEW, LMUL) D##CHAR##SEW##m##LMUL
#define HWY_RVV_V(BASE, SEW, LMUL) v##BASE##SEW##m##LMUL##_t
#define HWY_RVV_M(MLEN) vbool##MLEN##_t

}  // namespace detail

// TODO(janwas): remove typedefs and only use HWY_RVV_V etc. directly

// TODO(janwas): do we want fractional LMUL? (can encode as negative)
// Mixed-precision code can use LMUL 1..8 and that should be enough unless they
// need many registers.
#define HWY_SPECIALIZE(BASE, CHAR, SEW, LMUL, MLEN, NAME, OP)             \
  using HWY_RVV_D(CHAR, SEW, LMUL) =                                      \
      Simd<HWY_RVV_T(BASE, SEW), HWY_LANES(HWY_RVV_T(BASE, SEW)) * LMUL>; \
  using V##CHAR##SEW##m##LMUL = HWY_RVV_V(BASE, SEW, LMUL);               \
  template <>                                                             \
  struct DFromV_t<HWY_RVV_V(BASE, SEW, LMUL)> {                           \
    using Lane = HWY_RVV_T(BASE, SEW);                                    \
    using type = Simd<Lane, HWY_LANES(Lane) * LMUL>;                      \
  };
using Vf16m1 = vfloat16m1_t;
using Vf16m2 = vfloat16m2_t;
using Vf16m4 = vfloat16m4_t;
using Vf16m8 = vfloat16m8_t;
using Df16m1 = Simd<float16_t, HWY_LANES(uint16_t) * 1>;
using Df16m2 = Simd<float16_t, HWY_LANES(uint16_t) * 2>;
using Df16m4 = Simd<float16_t, HWY_LANES(uint16_t) * 4>;
using Df16m8 = Simd<float16_t, HWY_LANES(uint16_t) * 8>;

HWY_RVV_FOREACH(HWY_SPECIALIZE, _, _)
#undef HWY_SPECIALIZE

// vector = f(d), e.g. Zero
#define HWY_RVV_RETV_ARGD(BASE, CHAR, SEW, LMUL, MLEN, NAME, OP)          \
  HWY_API HWY_RVV_V(BASE, SEW, LMUL) NAME(HWY_RVV_D(CHAR, SEW, LMUL) d) { \
    (void)Lanes(d);                                                       \
    return v##OP##_##CHAR##SEW##m##LMUL();                                \
  }

// vector = f(vector), e.g. Not
#define HWY_RVV_RETV_ARGV(BASE, CHAR, SEW, LMUL, MLEN, NAME, OP)          \
  HWY_API HWY_RVV_V(BASE, SEW, LMUL) NAME(HWY_RVV_V(BASE, SEW, LMUL) v) { \
    return v##OP##_v_##CHAR##SEW##m##LMUL(v);                             \
  }

// vector = f(vector, scalar), e.g. detail::Add
#define HWY_RVV_RETV_ARGVS(BASE, CHAR, SEW, LMUL, MLEN, NAME, OP)  \
  HWY_API HWY_RVV_V(BASE, SEW, LMUL)                               \
      NAME(HWY_RVV_V(BASE, SEW, LMUL) a, HWY_RVV_T(BASE, SEW) b) { \
    return v##OP##_##CHAR##SEW##m##LMUL(a, b);                     \
  }

// vector = f(vector, vector), e.g. Add
#define HWY_RVV_RETV_ARGVV(BASE, CHAR, SEW, LMUL, MLEN, NAME, OP)        \
  HWY_API HWY_RVV_V(BASE, SEW, LMUL)                                     \
      NAME(HWY_RVV_V(BASE, SEW, LMUL) a, HWY_RVV_V(BASE, SEW, LMUL) b) { \
    return v##OP##_vv_##CHAR##SEW##m##LMUL(a, b);                        \
  }

// ================================================== INIT

// ------------------------------ Lanes

// WARNING: we want to query VLMAX/sizeof(T), but this actually changes VL!
// vlenb is not exposed through intrinsics and vreadvl is not VLMAX.
#define HWY_RVV_LANES(BASE, CHAR, SEW, LMUL, MLEN, NAME, OP) \
  HWY_API size_t NAME(HWY_RVV_D(CHAR, SEW, LMUL) /* d */) {  \
    return v##OP##SEW##m##LMUL();                            \
  }

HWY_RVV_FOREACH(HWY_RVV_LANES, Lanes, setvlmax_e)
#undef HWY_RVV_LANES

// ------------------------------ Zero

HWY_RVV_FOREACH(HWY_RVV_RETV_ARGD, Zero, zero)

template <class D>
using VFromD = decltype(Zero(D()));

// ------------------------------ Set
// vector = f(d, scalar), e.g. Set
#define HWY_RVV_SET(BASE, CHAR, SEW, LMUL, MLEN, NAME, OP)           \
  HWY_API HWY_RVV_V(BASE, SEW, LMUL)                                 \
      NAME(HWY_RVV_D(CHAR, SEW, LMUL) d, HWY_RVV_T(BASE, SEW) arg) { \
    (void)Lanes(d);                                                  \
    return v##OP##_##CHAR##SEW##m##LMUL(arg);                        \
  }

HWY_RVV_FOREACH_UI(HWY_RVV_SET, Set, mv_v_x)
HWY_RVV_FOREACH_F(HWY_RVV_SET, Set, fmv_v_f)
#undef HWY_RVV_SET

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
#define HWY_RVV_CAST_NOP(BASE, CHAR, SEW, LMUL, MLEN, NAME, OP)           \
  HWY_API HWY_RVV_V(BASE, SEW, LMUL)                                      \
      BitCastToByte(HWY_RVV_V(BASE, SEW, LMUL) v) {                       \
    return v;                                                             \
  }                                                                       \
  HWY_API HWY_RVV_V(BASE, SEW, LMUL) BitCastFromByte(                     \
      HWY_RVV_D(CHAR, SEW, LMUL) /* d */, HWY_RVV_V(BASE, SEW, LMUL) v) { \
    return v;                                                             \
  }

// Other integers
#define HWY_RVV_CAST_UI(BASE, CHAR, SEW, LMUL, MLEN, NAME, OP)            \
  HWY_API vuint8m##LMUL##_t BitCastToByte(HWY_RVV_V(BASE, SEW, LMUL) v) { \
    return v##OP##_v_##CHAR##SEW##m##LMUL##_u8m##LMUL(v);                 \
  }                                                                       \
  HWY_API HWY_RVV_V(BASE, SEW, LMUL) BitCastFromByte(                     \
      HWY_RVV_D(CHAR, SEW, LMUL) /* d */, vuint8m##LMUL##_t v) {          \
    return v##OP##_v_u8m##LMUL##_##CHAR##SEW##m##LMUL(v);                 \
  }

// Float: first cast to/from unsigned
#define HWY_RVV_CAST_F(BASE, CHAR, SEW, LMUL, MLEN, NAME, OP)             \
  HWY_API vuint8m##LMUL##_t BitCastToByte(HWY_RVV_V(BASE, SEW, LMUL) v) { \
    return v##OP##_v_u##SEW##m##LMUL##_u8m##LMUL(                         \
        v##OP##_v_f##SEW##m##LMUL##_u##SEW##m##LMUL(v));                  \
  }                                                                       \
  HWY_API HWY_RVV_V(BASE, SEW, LMUL) BitCastFromByte(                     \
      HWY_RVV_D(CHAR, SEW, LMUL) /* d */, vuint8m##LMUL##_t v) {          \
    return v##OP##_v_u##SEW##m##LMUL##_f##SEW##m##LMUL(                   \
        v##OP##_v_u8m##LMUL##_u##SEW##m##LMUL(v));                        \
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

namespace detail {

template <class V, class DU = RebindToUnsigned<DFromV<V>>>
HWY_API VFromD<DU> BitCastToUnsigned(V v) {
  return BitCast(DU(), v);
}

}  // namespace detail

// ------------------------------ Iota

namespace detail {

HWY_RVV_FOREACH_U(HWY_RVV_RETV_ARGD, Iota0, id_v)

template <class D, class DU = RebindToUnsigned<D>>
HWY_API VFromD<DU> Iota0(const D /*d*/) {
  Lanes(DU());
  return BitCastToUnsigned(Iota0(DU()));
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
HWY_RVV_FOREACH_UI(HWY_RVV_RETV_ARGVS, And, and_vx)
}  // namespace detail

HWY_RVV_FOREACH_UI(HWY_RVV_RETV_ARGVV, And, and)

template <class V, HWY_IF_FLOAT_V(V)>
HWY_API V And(const V a, const V b) {
  using DF = DFromV<V>;
  using DU = RebindToUnsigned<DF>;
  return BitCast(DF(), And(BitCast(DU(), a), BitCast(DU(), b)));
}

// ------------------------------ Or

// Scalar argument plus mask. Used by VecFromMask.
#define HWY_RVV_OR_MASK(BASE, CHAR, SEW, LMUL, MLEN, NAME, OP)           \
  HWY_API HWY_RVV_V(BASE, SEW, LMUL)                                     \
      NAME(HWY_RVV_V(BASE, SEW, LMUL) v, HWY_RVV_T(BASE, SEW) imm,       \
           HWY_RVV_M(MLEN) mask, HWY_RVV_V(BASE, SEW, LMUL) maskedoff) { \
    return v##OP##_##CHAR##SEW##m##LMUL##_m(mask, maskedoff, v, imm);    \
  }

namespace detail {
HWY_RVV_FOREACH_UI(HWY_RVV_OR_MASK, Or, or_vx)
}  // namespace detail

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
HWY_RVV_FOREACH_UI(HWY_RVV_RETV_ARGVS, Xor, xor_vx)
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
HWY_RVV_FOREACH_UI(HWY_RVV_RETV_ARGVS, Add, add_vx)
HWY_RVV_FOREACH_F(HWY_RVV_RETV_ARGVS, Add, fadd_vf)
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
#define HWY_RVV_SHIFT(BASE, CHAR, SEW, LMUL, MLEN, NAME, OP)               \
  template <int kBits>                                                     \
  HWY_API HWY_RVV_V(BASE, SEW, LMUL) NAME(HWY_RVV_V(BASE, SEW, LMUL) v) {  \
    return v##OP##_vx_##CHAR##SEW##m##LMUL(v, kBits);                      \
  }                                                                        \
  HWY_API HWY_RVV_V(BASE, SEW, LMUL)                                       \
      NAME##Same(HWY_RVV_V(BASE, SEW, LMUL) v, int bits) {                 \
    return v##OP##_vx_##CHAR##SEW##m##LMUL(v, static_cast<uint8_t>(bits)); \
  }

HWY_RVV_FOREACH_UI(HWY_RVV_SHIFT, ShiftLeft, sll)

// ------------------------------ ShiftRight[Same]

HWY_RVV_FOREACH_U(HWY_RVV_SHIFT, ShiftRight, srl)
HWY_RVV_FOREACH_I(HWY_RVV_SHIFT, ShiftRight, sra)

#undef HWY_RVV_SHIFT

// ------------------------------ Shl
#define HWY_RVV_SHIFT_VV(BASE, CHAR, SEW, LMUL, MLEN, NAME, OP)             \
  HWY_API HWY_RVV_V(BASE, SEW, LMUL)                                        \
      NAME(HWY_RVV_V(BASE, SEW, LMUL) v, HWY_RVV_V(BASE, SEW, LMUL) bits) { \
    return v##OP##_vv_##CHAR##SEW##m##LMUL(v, bits);                        \
  }

HWY_RVV_FOREACH_U(HWY_RVV_SHIFT_VV, Shl, sll)

#define HWY_RVV_SHIFT_II(BASE, CHAR, SEW, LMUL, MLEN, NAME, OP)              \
  HWY_API HWY_RVV_V(BASE, SEW, LMUL)                                         \
      NAME(HWY_RVV_V(BASE, SEW, LMUL) v, HWY_RVV_V(BASE, SEW, LMUL) bits) {  \
    return v##OP##_vv_##CHAR##SEW##m##LMUL(v,                                \
                                           detail::BitCastToUnsigned(bits)); \
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

HWY_RVV_FOREACH_UI16(HWY_RVV_RETV_ARGVV, Mul, mul)
HWY_RVV_FOREACH_UI32(HWY_RVV_RETV_ARGVV, Mul, mul)
HWY_RVV_FOREACH_F(HWY_RVV_RETV_ARGVV, Mul, fmul)

// ------------------------------ MulHigh

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
#define HWY_RVV_FMA(BASE, CHAR, SEW, LMUL, MLEN, NAME, OP)               \
  HWY_API HWY_RVV_V(BASE, SEW, LMUL)                                     \
      NAME(HWY_RVV_V(BASE, SEW, LMUL) mul, HWY_RVV_V(BASE, SEW, LMUL) x, \
           HWY_RVV_V(BASE, SEW, LMUL) add) {                             \
    return v##OP##_vv_##CHAR##SEW##m##LMUL(add, mul, x);                 \
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
#define HWY_RVV_RETM_ARGVV(BASE, CHAR, SEW, LMUL, MLEN, NAME, OP)        \
  HWY_API HWY_RVV_M(MLEN)                                                \
      NAME(HWY_RVV_V(BASE, SEW, LMUL) a, HWY_RVV_V(BASE, SEW, LMUL) b) { \
    (void)Lanes(DFromV<decltype(a)>());                                  \
    return v##OP##_vv_##CHAR##SEW##m##LMUL##_b##MLEN(a, b);              \
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

// ------------------------------ Gt

template <class V>
HWY_API auto Gt(const V a, const V b) -> decltype(Lt(a, b)) {
  return Lt(b, a);
}

// ------------------------------ Le
HWY_RVV_FOREACH_F(HWY_RVV_RETM_ARGVV, Le, mfle)

#undef HWY_RVV_RETM_ARGVV

// ------------------------------ Ge

template <class V>
HWY_API auto Ge(const V a, const V b) -> decltype(Le(a, b)) {
  return Le(b, a);
}

// ------------------------------ TestBit

template <class V>
HWY_API auto TestBit(const V a, const V bit) -> decltype(Eq(a, bit)) {
  return Ne(And(a, bit), Zero(DFromV<V>()));
}

// ------------------------------ Not

// mask = f(mask)
#define HWY_RVV_RETM_ARGM(MLEN, NAME, OP)           \
  HWY_API HWY_RVV_M(MLEN) NAME(HWY_RVV_M(MLEN) m) { \
    return vm##OP##_m_b##MLEN(m);                   \
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
#define HWY_RVV_IF_THEN_ELSE(BASE, CHAR, SEW, LMUL, MLEN, NAME, OP) \
  HWY_API HWY_RVV_V(BASE, SEW, LMUL)                                \
      NAME(HWY_RVV_M(MLEN) m, HWY_RVV_V(BASE, SEW, LMUL) yes,       \
           HWY_RVV_V(BASE, SEW, LMUL) no) {                         \
    return v##OP##_vvm_##CHAR##SEW##m##LMUL(m, no, yes);            \
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
HWY_API MFromD<D> RebindMask(const D d, const MFrom mask) {
  // No need to check lane size/LMUL are the same: if not, casting MFrom to
  // MFromD<D> would fail.
  return mask;
}

// ------------------------------ VecFromMask

template <class D, HWY_IF_NOT_FLOAT_D(D)>
HWY_API VFromD<D> VecFromMask(const D d, MFromD<D> mask) {
  const auto v0 = Zero(d);
  return detail::Or(v0, -1, mask, v0);
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

#define HWY_RVV_ALL_FALSE(MLEN, NAME, OP)          \
  HWY_API bool AllFalse(const HWY_RVV_M(MLEN) m) { \
    return vfirst_m_b##MLEN(m) < 0;                \
  }
HWY_RVV_FOREACH_B(HWY_RVV_ALL_FALSE, _, _)
#undef HWY_RVV_ALL_FALSE

// ------------------------------ AllTrue

#define HWY_RVV_ALL_TRUE(MLEN, NAME, OP)    \
  HWY_API bool AllTrue(HWY_RVV_M(MLEN) m) { \
    return AllFalse(vmnot_m_b##MLEN(m));    \
  }
HWY_RVV_FOREACH_B(HWY_RVV_ALL_TRUE, _, _)
#undef HWY_RVV_ALL_TRUE

// ------------------------------ CountTrue

#define HWY_RVV_COUNT_TRUE(MLEN, NAME, OP) \
  HWY_API size_t CountTrue(HWY_RVV_M(MLEN) m) { return vpopc_m_b##MLEN(m); }
HWY_RVV_FOREACH_B(HWY_RVV_COUNT_TRUE, _, _)
#undef HWY_RVV_COUNT_TRUE

// ================================================== MEMORY

// ------------------------------ Load

#define HWY_RVV_LOAD(BASE, CHAR, SEW, LMUL, MLEN, NAME, OP) \
  HWY_API HWY_RVV_V(BASE, SEW, LMUL)                        \
      NAME(HWY_RVV_D(CHAR, SEW, LMUL) d,                    \
           const HWY_RVV_T(BASE, SEW) * HWY_RESTRICT p) {   \
    (void)Lanes(d);                                         \
    return v##OP##SEW##_v_##CHAR##SEW##m##LMUL(p);          \
  }
HWY_RVV_FOREACH(HWY_RVV_LOAD, Load, le)
#undef HWY_RVV_LOAD

// Partial load
template <typename T, size_t N, HWY_IF_LE128(T, N)>
HWY_API VFromD<Simd<T, N>> Load(Simd<T, N> d, const T* HWY_RESTRICT p) {
  return Load(d, p);
}

// ------------------------------ LoadU

// RVV only requires lane alignment, not natural alignment of the entire vector.
template <class D>
HWY_API VFromD<D> LoadU(D d, const TFromD<D>* HWY_RESTRICT p) {
  return Load(d, p);
}

// ------------------------------ Store

#define HWY_RVV_RET_ARGVDP(BASE, CHAR, SEW, LMUL, MLEN, NAME, OP) \
  HWY_API void NAME(HWY_RVV_V(BASE, SEW, LMUL) v,                 \
                    HWY_RVV_D(CHAR, SEW, LMUL) d,                 \
                    HWY_RVV_T(BASE, SEW) * HWY_RESTRICT p) {      \
    (void)Lanes(d);                                               \
    return v##OP##SEW##_v_##CHAR##SEW##m##LMUL(p, v);             \
  }
HWY_RVV_FOREACH(HWY_RVV_RET_ARGVDP, Store, se)
#undef HWY_RVV_RET_ARGVDP

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

// ------------------------------ GatherOffset

#define HWY_RVV_GATHER(BASE, CHAR, SEW, LMUL, MLEN, NAME, OP) \
  HWY_API HWY_RVV_V(BASE, SEW, LMUL)                          \
      NAME(HWY_RVV_D(CHAR, SEW, LMUL) /* d */,                \
           const HWY_RVV_T(BASE, SEW) * HWY_RESTRICT base,    \
           HWY_RVV_V(int, SEW, LMUL) offset) {                \
    return v##OP##ei##SEW##_v_##CHAR##SEW##m##LMUL(           \
        base, detail::BitCastToUnsigned(offset));             \
  }
HWY_RVV_FOREACH(HWY_RVV_GATHER, GatherOffset, lx)
#undef HWY_RVV_GATHER

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

// ================================================== CONVERT

// ------------------------------ PromoteTo U

HWY_API Vu16m2 PromoteTo(Du16m2 /* d */, Vu8m1 v) { return vzext_vf2_u16m2(v); }
HWY_API Vu16m4 PromoteTo(Du16m4 /* d */, Vu8m2 v) { return vzext_vf2_u16m4(v); }
HWY_API Vu16m8 PromoteTo(Du16m8 /* d */, Vu8m4 v) { return vzext_vf2_u16m8(v); }

HWY_API Vu32m4 PromoteTo(Du32m4 /* d */, Vu8m1 v) { return vzext_vf4_u32m4(v); }
HWY_API Vu32m8 PromoteTo(Du32m8 /* d */, Vu8m2 v) { return vzext_vf4_u32m8(v); }

HWY_API Vu32m2 PromoteTo(Du32m2 /* d */, const Vu16m1 v) {
  return vzext_vf2_u32m2(v);
}
HWY_API Vu32m4 PromoteTo(Du32m4 /* d */, const Vu16m2 v) {
  return vzext_vf2_u32m4(v);
}
HWY_API Vu32m8 PromoteTo(Du32m8 /* d */, const Vu16m4 v) {
  return vzext_vf2_u32m8(v);
}

HWY_API Vu64m2 PromoteTo(Du64m2 /* d */, const Vu32m1 v) {
  return vzext_vf2_u64m2(v);
}
HWY_API Vu64m4 PromoteTo(Du64m4 /* d */, const Vu32m2 v) {
  return vzext_vf2_u64m4(v);
}
HWY_API Vu64m8 PromoteTo(Du64m8 /* d */, const Vu32m4 v) {
  return vzext_vf2_u64m8(v);
}

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

// ------------------------------ PromoteTo I

HWY_API Vi16m2 PromoteTo(Di16m2 /* d */, Vi8m1 v) { return vsext_vf2_i16m2(v); }
HWY_API Vi16m4 PromoteTo(Di16m4 /* d */, Vi8m2 v) { return vsext_vf2_i16m4(v); }
HWY_API Vi16m8 PromoteTo(Di16m8 /* d */, Vi8m4 v) { return vsext_vf2_i16m8(v); }

HWY_API Vi32m4 PromoteTo(Di32m4 /* d */, Vi8m1 v) { return vsext_vf4_i32m4(v); }
HWY_API Vi32m8 PromoteTo(Di32m8 /* d */, Vi8m2 v) { return vsext_vf4_i32m8(v); }

HWY_API Vi32m2 PromoteTo(Di32m2 /* d */, const Vi16m1 v) {
  return vsext_vf2_i32m2(v);
}
HWY_API Vi32m4 PromoteTo(Di32m4 /* d */, const Vi16m2 v) {
  return vsext_vf2_i32m4(v);
}
HWY_API Vi32m8 PromoteTo(Di32m8 /* d */, const Vi16m4 v) {
  return vsext_vf2_i32m8(v);
}

HWY_API Vi64m2 PromoteTo(Di64m2 /* d */, const Vi32m1 v) {
  return vsext_vf2_i64m2(v);
}
HWY_API Vi64m4 PromoteTo(Di64m4 /* d */, const Vi32m2 v) {
  return vsext_vf2_i64m4(v);
}
HWY_API Vi64m8 PromoteTo(Di64m8 /* d */, const Vi32m4 v) {
  return vsext_vf2_i64m8(v);
}

// ------------------------------ PromoteTo F

HWY_API Vf32m2 PromoteTo(Df32m2 /* d */, const Vf16m1 v) {
  return vfwcvt_f_f_v_f32m2(v);
}
HWY_API Vf32m4 PromoteTo(Df32m4 /* d */, const Vf16m2 v) {
  return vfwcvt_f_f_v_f32m4(v);
}
HWY_API Vf32m8 PromoteTo(Df32m8 /* d */, const Vf16m4 v) {
  return vfwcvt_f_f_v_f32m8(v);
}

HWY_API Vf64m2 PromoteTo(Df64m2 /* d */, const Vf32m1 v) {
  return vfwcvt_f_f_v_f64m2(v);
}
HWY_API Vf64m4 PromoteTo(Df64m4 /* d */, const Vf32m2 v) {
  return vfwcvt_f_f_v_f64m4(v);
}
HWY_API Vf64m8 PromoteTo(Df64m8 /* d */, const Vf32m4 v) {
  return vfwcvt_f_f_v_f64m8(v);
}

HWY_API Vf64m2 PromoteTo(Df64m2 /* d */, const Vi32m1 v) {
  return vfwcvt_f_x_v_f64m2(v);
}
HWY_API Vf64m4 PromoteTo(Df64m4 /* d */, const Vi32m2 v) {
  return vfwcvt_f_x_v_f64m4(v);
}
HWY_API Vf64m8 PromoteTo(Df64m8 /* d */, const Vi32m4 v) {
  return vfwcvt_f_x_v_f64m8(v);
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

#define HWY_RVV_CONVERT(BASE, CHAR, SEW, LMUL, MLEN, NAME, OP)                 \
  HWY_API HWY_RVV_V(BASE, SEW, LMUL) ConvertTo(                                \
      HWY_RVV_D(CHAR, SEW, LMUL) /* d */, HWY_RVV_V(int, SEW, LMUL) v) {       \
    return vfcvt_f_x_v_f##SEW##m##LMUL(v);                                     \
  }                                                                            \
  /* Truncates (rounds toward zero). */                                        \
  HWY_API HWY_RVV_V(int, SEW, LMUL) ConvertTo(HWY_RVV_D(i, SEW, LMUL) /* d */, \
                                              HWY_RVV_V(BASE, SEW, LMUL) v) {  \
    return vfcvt_rtz_x_f_v_i##SEW##m##LMUL(v);                                 \
  }                                                                            \
  /* Uses default rounding mode. */                                            \
  HWY_API HWY_RVV_V(int, SEW, LMUL) NearestInt(HWY_RVV_V(BASE, SEW, LMUL) v) { \
    return vfcvt_x_f_v_i##SEW##m##LMUL(v);                                     \
  }

// API only requires f32 but we provide f64 for internal use (otherwise, it
// seems difficult to implement Iota without a _mf2 vector half).
HWY_RVV_FOREACH_F(HWY_RVV_CONVERT, _, _)
#undef HWY_RVV_CONVERT

// ================================================== SWIZZLE

// ------------------------------ Compress

#define HWY_RVV_COMPRESS(BASE, CHAR, SEW, LMUL, MLEN, NAME, OP)  \
  HWY_API HWY_RVV_V(BASE, SEW, LMUL)                             \
      NAME(HWY_RVV_V(BASE, SEW, LMUL) v, HWY_RVV_M(MLEN) mask) { \
    return v##OP##_vm_##CHAR##SEW##m##LMUL(mask, v, v);          \
  }

HWY_RVV_FOREACH_UI32(HWY_RVV_COMPRESS, Compress, compress)
HWY_RVV_FOREACH_UI64(HWY_RVV_COMPRESS, Compress, compress)
HWY_RVV_FOREACH_F(HWY_RVV_COMPRESS, Compress, compress)
#undef HWY_RVV_COMPRESS

// ------------------------------ CompressStore

template <class V, class M, class D>
HWY_API size_t CompressStore(const V v, const M mask, const D d,
                             TFromD<D>* HWY_RESTRICT aligned) {
  Store(Compress(v, mask), d, aligned);
  return CountTrue(mask);
}

// ------------------------------ TableLookupLanes

template <class D, class DU = RebindToUnsigned<D>>
HWY_API VFromD<DU> SetTableIndices(D d, const TFromD<DU>* idx) {
#if !defined(NDEBUG) || defined(ADDRESS_SANITIZER)
  const size_t N = Lanes(d);
  for (size_t i = 0; i < N; ++i) {
    HWY_DASSERT(0 <= idx[i] && idx[i] < static_cast<TFromD<DU>>(N));
  }
#endif
  return Load(DU(), idx);
}

// <32bit are not part of Highway API, but used in Broadcast. This limits VLMAX
// to 2048! We could instead use vrgatherei16.
#define HWY_RVV_TABLE(BASE, CHAR, SEW, LMUL, MLEN, NAME, OP)               \
  HWY_API HWY_RVV_V(BASE, SEW, LMUL)                                       \
      NAME(HWY_RVV_V(BASE, SEW, LMUL) v, HWY_RVV_V(uint, SEW, LMUL) idx) { \
    return v##OP##_vv_##CHAR##SEW##m##LMUL(v, idx);                        \
  }

HWY_RVV_FOREACH(HWY_RVV_TABLE, TableLookupLanes, rgather)
#undef HWY_RVV_TABLE

// ------------------------------ Shuffle01

template <class V>
HWY_API V Shuffle01(const V v) {
  using D = DFromV<V>;
  static_assert(sizeof(TFromD<D>) == 8, "Defined for 64-bit types");
  const auto idx = detail::Xor(detail::Iota0(D()), 1);
  return TableLookupLanes(v, idx);
}

// ------------------------------ Shuffle2301

template <class V>
HWY_API V Shuffle2301(const V v) {
  using D = DFromV<V>;
  static_assert(sizeof(TFromD<D>) == 4, "Defined for 32-bit types");
  const auto idx = detail::Xor(detail::Iota0(D()), 1);
  return TableLookupLanes(v, idx);
}

// ------------------------------ Shuffle1032

template <class V>
HWY_API V Shuffle1032(const V v) {
  using D = DFromV<V>;
  static_assert(sizeof(TFromD<D>) == 4, "Defined for 32-bit types");
  const auto idx = detail::Xor(detail::Iota0(D()), 2);
  return TableLookupLanes(v, idx);
}

// ------------------------------ Shuffle0123

template <class V>
HWY_API V Shuffle0123(const V v) {
  using D = DFromV<V>;
  static_assert(sizeof(TFromD<D>) == 4, "Defined for 32-bit types");
  const auto idx = detail::Xor(detail::Iota0(D()), 3);
  return TableLookupLanes(v, idx);
}

// ------------------------------ Shuffle2103

template <class V>
HWY_API V Shuffle2103(const V v) {
  using D = DFromV<V>;
  static_assert(sizeof(TFromD<D>) == 4, "Defined for 32-bit types");
  // This shuffle is a rotation. We can compute subtraction modulo 4 (number of
  // lanes per 128-bit block) via bitwise ops.
  const auto i = detail::Xor(detail::Iota0(D()), 1);
  const auto lsb = detail::And(i, 1);
  const auto borrow = Add(lsb, lsb);
  const auto idx = Xor(i, borrow);
  return TableLookupLanes(v, idx);
}

// ------------------------------ Shuffle0321

template <class V>
HWY_API V Shuffle0321(const V v) {
  using D = DFromV<V>;
  static_assert(sizeof(TFromD<D>) == 4, "Defined for 32-bit types");
  // This shuffle is a rotation. We can compute subtraction modulo 4 (number of
  // lanes per 128-bit block) via bitwise ops.
  const auto i = detail::Xor(detail::Iota0(D()), 3);
  const auto lsb = detail::And(i, 1);
  const auto borrow = Add(lsb, lsb);
  const auto idx = Xor(i, borrow);
  return TableLookupLanes(v, idx);
}

// ------------------------------ TableLookupBytes

namespace detail {

// For x86-compatible behaviour mandated by Highway API: TableLookupBytes
// offsets are implicitly relative to the start of their 128-bit block.
template <class D>
constexpr size_t LanesPerBlock(D) {
  return 16 / sizeof(TFromD<D>);
}

template <class D, class V>
HWY_API V OffsetsOf128BitBlocks(const D d, const V iota0) {
  using T = MakeUnsigned<TFromD<D>>;
  return detail::And(iota0, static_cast<T>(~(LanesPerBlock(d) - 1)));
}

}  // namespace detail

template <class V>
HWY_API V TableLookupBytes(const V v, const V idx) {
  using D = DFromV<V>;
  const Repartition<uint8_t, D> d8;
  const auto offsets128 = detail::OffsetsOf128BitBlocks(d8, detail::Iota0(d8));
  const auto idx8 = Add(BitCast(d8, idx), offsets128);
  return BitCast(D(), TableLookupLanes(BitCast(d8, v), idx8));
}

// ------------------------------ Broadcast

template <int kLane, class V>
HWY_API V Broadcast(const V v) {
  const DFromV<V> d;
  constexpr size_t kLanesPerBlock = detail::LanesPerBlock(d);
  static_assert(0 <= kLane && kLane < kLanesPerBlock, "Invalid lane");
  auto idx = detail::OffsetsOf128BitBlocks(d, detail::Iota0(d));
  if (kLane != 0) {
    idx = detail::Add(idx, kLane);
  }
  return TableLookupLanes(v, idx);
}

// ------------------------------ GetLane

#define HWY_RVV_GET_LANE(BASE, CHAR, SEW, LMUL, MLEN, NAME, OP)     \
  HWY_API HWY_RVV_T(BASE, SEW) NAME(HWY_RVV_V(BASE, SEW, LMUL) v) { \
    return v##OP##_s_##CHAR##SEW##m##LMUL##_##CHAR##SEW(v);         \
  }

HWY_RVV_FOREACH_UI(HWY_RVV_GET_LANE, GetLane, mv_x)
HWY_RVV_FOREACH_F(HWY_RVV_GET_LANE, GetLane, fmv_f)
#undef HWY_RVV_GET_LANE

// ------------------------------ ShiftLeftLanes

// vector = f(vector, size_t)
#define HWY_RVV_SLIDE(BASE, CHAR, SEW, LMUL, MLEN, NAME, OP) \
  HWY_API HWY_RVV_V(BASE, SEW, LMUL)                         \
      NAME(HWY_RVV_V(BASE, SEW, LMUL) v, size_t lanes) {     \
    return v##OP##_vx_##CHAR##SEW##m##LMUL(v, v, lanes);     \
  }

namespace detail {
HWY_RVV_FOREACH(HWY_RVV_SLIDE, SlideUp, slideup)
}  // namespace detail

template <size_t kLanes, class V>
HWY_API V ShiftLeftLanes(const V v) {
  using D = DFromV<V>;
  const RebindToSigned<D> di;
  const auto shifted = detail::SlideUp(v, kLanes);
  // Match x86 semantics by zeroing lower lanes in 128-bit blocks
  constexpr size_t kLanesPerBlock = detail::LanesPerBlock(di);
  const auto idx_mod = detail::And(detail::Iota0(di), kLanesPerBlock - 1);
  const auto clear = Lt(BitCast(di, idx_mod), Set(di, kLanes));
  return IfThenZeroElse(clear, shifted);
}

// ------------------------------ ShiftLeftBytes

template <int kBytes, class V>
HWY_API V ShiftLeftBytes(const V v) {
  using D = DFromV<V>;
  const Repartition<uint8_t, D> d8;
  Lanes(d8);
  return BitCast(D(), ShiftLeftLanes<kBytes>(BitCast(d8, v)));
}

// ------------------------------ ShiftRightLanes

namespace detail {
HWY_RVV_FOREACH(HWY_RVV_SLIDE, SlideDown, slidedown)
}  // namespace detail

#undef HWY_RVV_SLIDE

template <size_t kLanes, class V>
HWY_API V ShiftRightLanes(const V v) {
  using D = DFromV<V>;
  const RebindToSigned<D> di;
  const auto shifted = detail::SlideDown(v, kLanes);
  // Match x86 semantics by zeroing upper lanes in 128-bit blocks
  constexpr size_t kLanesPerBlock = detail::LanesPerBlock(di);
  const auto idx_mod = detail::And(detail::Iota0(di), kLanesPerBlock - 1);
  const auto keep = Lt(BitCast(di, idx_mod), Set(di, kLanesPerBlock - kLanes));
  return IfThenElseZero(keep, shifted);
}

// ------------------------------ ShiftRightBytes

template <int kBytes, class V>
HWY_API V ShiftRightBytes(const V v) {
  using D = DFromV<V>;
  const Repartition<uint8_t, D> d8;
  Lanes(d8);
  return BitCast(D(), ShiftRightLanes<kBytes>(BitCast(d8, v)));
}

// ------------------------------ OddEven

template <class V>
HWY_API V OddEven(const V a, const V b) {
  const RebindToUnsigned<DFromV<V>> du;  // Iota0 is unsigned only
  const auto is_even = Eq(detail::And(detail::Iota0(du), 1), Zero(du));
  return IfThenElse(is_even, b, a);
}

// ------------------------------ ConcatUpperLower

template <class V>
HWY_API V ConcatUpperLower(const V hi, const V lo) {
  const RebindToSigned<DFromV<V>> di;
  const auto idx_half = Set(di, Lanes(di) / 2);
  const auto is_lower_half = Lt(BitCast(di, detail::Iota0(di)), idx_half);
  return IfThenElse(is_lower_half, lo, hi);
}

// ------------------------------ ConcatLowerLower

template <class V>
HWY_API V ConcatLowerLower(const V hi, const V lo) {
  // Move lower half into upper
  const auto hi_up = detail::SlideUp(hi, Lanes(DFromV<V>()) / 2);
  return ConcatUpperLower(hi_up, lo);
}

// ------------------------------ ConcatUpperUpper

template <class V>
HWY_API V ConcatUpperUpper(const V hi, const V lo) {
  // Move upper half into lower
  const auto lo_down = detail::SlideDown(lo, Lanes(DFromV<V>()) / 2);
  return ConcatUpperLower(hi, lo_down);
}

// ------------------------------ ConcatLowerUpper

template <class V>
HWY_API V ConcatLowerUpper(const V hi, const V lo) {
  // Move half of both inputs to the other half
  const auto hi_up = detail::SlideUp(hi, Lanes(DFromV<V>()) / 2);
  const auto lo_down = detail::SlideDown(lo, Lanes(DFromV<V>()) / 2);
  return ConcatUpperLower(hi_up, lo_down);
}

// ------------------------------ InterleaveLower

template <class V>
HWY_API V InterleaveLower(const V a, const V b) {
  const DFromV<V> d;
  const RebindToUnsigned<decltype(d)> du;
  constexpr size_t kLanesPerBlock = detail::LanesPerBlock(d);
  const auto i = detail::Iota0(d);
  const auto idx_mod = ShiftRight<1>(detail::And(i, kLanesPerBlock - 1));
  const auto idx = Add(idx_mod, detail::OffsetsOf128BitBlocks(d, i));
  const auto is_even = Eq(detail::And(i, 1), Zero(du));
  return IfThenElse(is_even, TableLookupLanes(a, idx),
                    TableLookupLanes(b, idx));
}

// ------------------------------ InterleaveUpper

template <class V>
HWY_API V InterleaveUpper(const V a, const V b) {
  const DFromV<V> d;
  const RebindToUnsigned<decltype(d)> du;
  constexpr size_t kLanesPerBlock = detail::LanesPerBlock(d);
  const auto i = detail::Iota0(d);
  const auto idx_mod = ShiftRight<1>(detail::And(i, kLanesPerBlock - 1));
  const auto idx_lower = Add(idx_mod, detail::OffsetsOf128BitBlocks(d, i));
  const auto idx = detail::Add(idx_lower, kLanesPerBlock / 2);
  const auto is_even = Eq(detail::And(i, 1), Zero(du));
  return IfThenElse(is_even, TableLookupLanes(a, idx),
                    TableLookupLanes(b, idx));
}

// ------------------------------ ZipLower

template <class V>
HWY_API VFromD<RepartitionToWide<DFromV<V>>> ZipLower(const V a, const V b) {
  RepartitionToWide<DFromV<V>> dw;
  return BitCast(dw, InterleaveLower(a, b));
}

// ------------------------------ ZipUpper

template <class V>
HWY_API VFromD<RepartitionToWide<DFromV<V>>> ZipUpper(const V a, const V b) {
  RepartitionToWide<DFromV<V>> dw;
  return BitCast(dw, InterleaveUpper(a, b));
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

// ================================================== REDUCE

// vector = f(vector, zero_m1)
#define HWY_RVV_REDUCE(BASE, CHAR, SEW, LMUL, MLEN, NAME, OP)             \
  HWY_API HWY_RVV_V(BASE, SEW, LMUL)                                      \
      NAME(HWY_RVV_V(BASE, SEW, LMUL) v, HWY_RVV_V(BASE, SEW, 1) v0) {    \
    vsetvlmax_e##SEW##m##LMUL();                                          \
    return Set(HWY_RVV_D(CHAR, SEW, LMUL)(),                              \
               GetLane(v##OP##_vs_##CHAR##SEW##m##LMUL##_##CHAR##SEW##m1( \
                   v0, v, v0)));                                          \
  }

// ------------------------------ SumOfLanes

namespace detail {

HWY_RVV_FOREACH_UI(HWY_RVV_REDUCE, RedSum, redsum)
HWY_RVV_FOREACH_F(HWY_RVV_REDUCE, RedSum, fredsum)

}  // namespace detail

template <class V>
HWY_API V SumOfLanes(const V v) {
  using T = TFromV<V>;
  const auto v0 = Zero(Simd<T, HWY_LANES(T)>());  // always m1
  return detail::RedSum(v, v0);
}

// ------------------------------ MinOfLanes
namespace detail {

HWY_RVV_FOREACH_U(HWY_RVV_REDUCE, RedMin, redminu)
HWY_RVV_FOREACH_I(HWY_RVV_REDUCE, RedMin, redmin)
HWY_RVV_FOREACH_F(HWY_RVV_REDUCE, RedMin, fredmin)

}  // namespace detail

template <class V>
HWY_API V MinOfLanes(const V v) {
  using T = TFromV<V>;
  const Simd<T, HWY_LANES(T)> d1;  // always m1
  const auto neutral = Set(d1, HighestValue<T>());
  return detail::RedMin(v, neutral);
}

// ------------------------------ MaxOfLanes
namespace detail {

HWY_RVV_FOREACH_U(HWY_RVV_REDUCE, RedMax, redmaxu)
HWY_RVV_FOREACH_I(HWY_RVV_REDUCE, RedMax, redmax)
HWY_RVV_FOREACH_F(HWY_RVV_REDUCE, RedMax, fredmax)

}  // namespace detail

template <class V>
HWY_API V MaxOfLanes(const V v) {
  using T = TFromV<V>;
  const Simd<T, HWY_LANES(T)> d1;  // always m1
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
  const auto idx = detail::And(detail::Iota0(d), kLanesPerBlock - 1);
  return TableLookupLanes(loaded, idx);
}

// ------------------------------ StoreMaskBits
#define HWY_RVV_STORE_MASK_BITS(MLEN, NAME, OP)                 \
  HWY_API size_t StoreMaskBits(HWY_RVV_M(MLEN) m, uint8_t* p) { \
    /* LMUL=1 is always enough */                               \
    Simd<uint8_t, HWY_LANES(uint8_t)> d8;                       \
    const size_t num_bytes = (Lanes(d8) + MLEN - 1) / MLEN;     \
    /* TODO(janwas): how to convert vbool* to vuint?*/          \
    /*Store(m, d8, p);*/                                        \
    (void)m;                                                    \
    (void)p;                                                    \
    return num_bytes;                                           \
  }
HWY_RVV_FOREACH_B(HWY_RVV_STORE_MASK_BITS, _, _)
#undef HWY_RVV_STORE_MASK_BITS

// ------------------------------ Neg

template <class V, HWY_IF_SIGNED_V(V)>
HWY_API V Neg(const V v) {
  return Sub(Zero(DFromV<V>()), v);
}

// vector = f(vector), but argument is repeated
#define HWY_RVV_RETV_ARGV2(BASE, CHAR, SEW, LMUL, MLEN, NAME, OP)         \
  HWY_API HWY_RVV_V(BASE, SEW, LMUL) NAME(HWY_RVV_V(BASE, SEW, LMUL) v) { \
    return v##OP##_vv_##CHAR##SEW##m##LMUL(v, v);                         \
  }

HWY_RVV_FOREACH_F(HWY_RVV_RETV_ARGV2, Neg, fsgnjn)

// ------------------------------ Abs

template <class V, HWY_IF_SIGNED_V(V)>
HWY_API V Abs(const V v) {
  return Max(v, Neg(v));
}

HWY_RVV_FOREACH_F(HWY_RVV_RETV_ARGV2, Abs, fsgnjx)

#undef HWY_RVV_RETV_ARGV2

// ------------------------------ AbsDiff

template <class V>
HWY_API V AbsDiff(const V a, const V b) {
  return Abs(Sub(a, b));
}

// ------------------------------ Round

// IEEE-754 roundToIntegralTiesToEven returns floating-point, but we do not have
// a dedicated instruction for that. Rounding to integer and converting back to
// float is correct except when the input magnitude is large, in which case the
// input was already an integer (because mantissa >> exponent is zero).

namespace detail {
enum RoundingModes { kNear, kTrunc, kDown, kUp };

template <class V>
HWY_API auto UseInt(const V v) -> decltype(MaskFromVec(v)) {
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

// ------------------------------ Trunc

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

// ------------------------------ Iota

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
  return detail::Add(ConvertTo(d, BitCast(di, detail::Iota0(du))), first);
}

// ------------------------------ MulEven

// Using vwmul does not work for m8, so use mulh instead. Highway only provides
// MulHigh for 16-bit, so use a private wrapper.
namespace detail {

HWY_RVV_FOREACH_U32(HWY_RVV_RETV_ARGVV, MulHigh, mulhu)
HWY_RVV_FOREACH_I32(HWY_RVV_RETV_ARGVV, MulHigh, mulh)

}  // namespace detail

template <class V>
HWY_API VFromD<RepartitionToWide<DFromV<V>>> MulEven(const V a, const V b) {
  const DFromV<V> d;
  Lanes(d);
  const auto lo = Mul(a, b);
  const auto hi = detail::MulHigh(a, b);
  const RepartitionToWide<DFromV<V>> dw;
  return BitCast(dw, OddEven(detail::SlideUp(hi, 1), lo));
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
