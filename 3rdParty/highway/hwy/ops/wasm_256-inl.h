// Copyright 2021 Google LLC
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

// 256-bit WASM vectors and operations. Experimental.
// External include guard in highway.h - see comment there.

// For half-width vectors. Already includes base.h and shared-inl.h.
#include "hwy/ops/wasm_128-inl.h"

HWY_BEFORE_NAMESPACE();
namespace hwy {
namespace HWY_NAMESPACE {

template <typename T>
class Vec256 {
 public:
  using PrivateT = T;                                  // only for DFromV
  static constexpr size_t kPrivateN = 32 / sizeof(T);  // only for DFromV

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

  Vec128<T> v0;
  Vec128<T> v1;
};

template <typename T>
struct Mask256 {
  Mask128<T> m0;
  Mask128<T> m1;
};

// ------------------------------ Zero

// Avoid VFromD here because it is defined in terms of Zero.
template <class D, HWY_IF_V_SIZE_D(D, 32)>
HWY_API Vec256<TFromD<D>> Zero(D d) {
  const Half<decltype(d)> dh;
  Vec256<TFromD<D>> ret;
  ret.v0 = ret.v1 = Zero(dh);
  return ret;
}

// ------------------------------ BitCast
template <class D, typename TFrom>
HWY_API VFromD<D> BitCast(D d, Vec256<TFrom> v) {
  const Half<decltype(d)> dh;
  VFromD<D> ret;
  ret.v0 = BitCast(dh, v.v0);
  ret.v1 = BitCast(dh, v.v1);
  return ret;
}

// ------------------------------ ResizeBitCast

// 32-byte vector to 32-byte vector: Same as BitCast
template <class D, typename FromV, HWY_IF_V_SIZE_V(FromV, 32),
          HWY_IF_V_SIZE_D(D, 32)>
HWY_API VFromD<D> ResizeBitCast(D d, FromV v) {
  return BitCast(d, v);
}

// <= 16-byte vector to 32-byte vector
template <class D, typename FromV, HWY_IF_V_SIZE_LE_V(FromV, 16),
          HWY_IF_V_SIZE_D(D, 32)>
HWY_API VFromD<D> ResizeBitCast(D d, FromV v) {
  const Half<decltype(d)> dh;
  VFromD<D> ret;
  ret.v0 = ResizeBitCast(dh, v);
  ret.v1 = Zero(dh);
  return ret;
}

// 32-byte vector to <= 16-byte vector
template <class D, typename FromV, HWY_IF_V_SIZE_V(FromV, 32),
          HWY_IF_V_SIZE_LE_D(D, 16)>
HWY_API VFromD<D> ResizeBitCast(D d, FromV v) {
  return ResizeBitCast(d, v.v0);
}

// ------------------------------ Set
template <class D, HWY_IF_V_SIZE_D(D, 32), typename T2>
HWY_API VFromD<D> Set(D d, const T2 t) {
  const Half<decltype(d)> dh;
  VFromD<D> ret;
  ret.v0 = ret.v1 = Set(dh, static_cast<TFromD<D>>(t));
  return ret;
}

// Undefined, Iota defined in wasm_128.

// ================================================== ARITHMETIC

template <typename T>
HWY_API Vec256<T> operator+(Vec256<T> a, const Vec256<T> b) {
  a.v0 += b.v0;
  a.v1 += b.v1;
  return a;
}

template <typename T>
HWY_API Vec256<T> operator-(Vec256<T> a, const Vec256<T> b) {
  a.v0 -= b.v0;
  a.v1 -= b.v1;
  return a;
}

// ------------------------------ SumsOf8
HWY_API Vec256<uint64_t> SumsOf8(const Vec256<uint8_t> v) {
  Vec256<uint64_t> ret;
  ret.v0 = SumsOf8(v.v0);
  ret.v1 = SumsOf8(v.v1);
  return ret;
}

template <typename T>
HWY_API Vec256<T> SaturatedAdd(Vec256<T> a, const Vec256<T> b) {
  a.v0 = SaturatedAdd(a.v0, b.v0);
  a.v1 = SaturatedAdd(a.v1, b.v1);
  return a;
}

template <typename T>
HWY_API Vec256<T> SaturatedSub(Vec256<T> a, const Vec256<T> b) {
  a.v0 = SaturatedSub(a.v0, b.v0);
  a.v1 = SaturatedSub(a.v1, b.v1);
  return a;
}

template <typename T>
HWY_API Vec256<T> AverageRound(Vec256<T> a, const Vec256<T> b) {
  a.v0 = AverageRound(a.v0, b.v0);
  a.v1 = AverageRound(a.v1, b.v1);
  return a;
}

template <typename T>
HWY_API Vec256<T> Abs(Vec256<T> v) {
  v.v0 = Abs(v.v0);
  v.v1 = Abs(v.v1);
  return v;
}

// ------------------------------ Shift lanes by constant #bits

template <int kBits, typename T>
HWY_API Vec256<T> ShiftLeft(Vec256<T> v) {
  v.v0 = ShiftLeft<kBits>(v.v0);
  v.v1 = ShiftLeft<kBits>(v.v1);
  return v;
}

template <int kBits, typename T>
HWY_API Vec256<T> ShiftRight(Vec256<T> v) {
  v.v0 = ShiftRight<kBits>(v.v0);
  v.v1 = ShiftRight<kBits>(v.v1);
  return v;
}

// ------------------------------ RotateRight (ShiftRight, Or)
template <int kBits, typename T>
HWY_API Vec256<T> RotateRight(const Vec256<T> v) {
  constexpr size_t kSizeInBits = sizeof(T) * 8;
  static_assert(0 <= kBits && kBits < kSizeInBits, "Invalid shift count");
  if (kBits == 0) return v;
  return Or(ShiftRight<kBits>(v), ShiftLeft<kSizeInBits - kBits>(v));
}

// ------------------------------ Shift lanes by same variable #bits

template <typename T>
HWY_API Vec256<T> ShiftLeftSame(Vec256<T> v, const int bits) {
  v.v0 = ShiftLeftSame(v.v0, bits);
  v.v1 = ShiftLeftSame(v.v1, bits);
  return v;
}

template <typename T>
HWY_API Vec256<T> ShiftRightSame(Vec256<T> v, const int bits) {
  v.v0 = ShiftRightSame(v.v0, bits);
  v.v1 = ShiftRightSame(v.v1, bits);
  return v;
}

// ------------------------------ Min, Max
template <typename T>
HWY_API Vec256<T> Min(Vec256<T> a, const Vec256<T> b) {
  a.v0 = Min(a.v0, b.v0);
  a.v1 = Min(a.v1, b.v1);
  return a;
}

template <typename T>
HWY_API Vec256<T> Max(Vec256<T> a, const Vec256<T> b) {
  a.v0 = Max(a.v0, b.v0);
  a.v1 = Max(a.v1, b.v1);
  return a;
}
// ------------------------------ Integer multiplication

template <typename T>
HWY_API Vec256<T> operator*(Vec256<T> a, const Vec256<T> b) {
  a.v0 *= b.v0;
  a.v1 *= b.v1;
  return a;
}

template <typename T>
HWY_API Vec256<T> MulHigh(Vec256<T> a, const Vec256<T> b) {
  a.v0 = MulHigh(a.v0, b.v0);
  a.v1 = MulHigh(a.v1, b.v1);
  return a;
}

template <typename T>
HWY_API Vec256<T> MulFixedPoint15(Vec256<T> a, const Vec256<T> b) {
  a.v0 = MulFixedPoint15(a.v0, b.v0);
  a.v1 = MulFixedPoint15(a.v1, b.v1);
  return a;
}

// Cannot use MakeWide because that returns uint128_t for uint64_t, but we want
// uint64_t.
HWY_API Vec256<uint64_t> MulEven(Vec256<uint32_t> a, const Vec256<uint32_t> b) {
  Vec256<uint64_t> ret;
  ret.v0 = MulEven(a.v0, b.v0);
  ret.v1 = MulEven(a.v1, b.v1);
  return ret;
}
HWY_API Vec256<int64_t> MulEven(Vec256<int32_t> a, const Vec256<int32_t> b) {
  Vec256<int64_t> ret;
  ret.v0 = MulEven(a.v0, b.v0);
  ret.v1 = MulEven(a.v1, b.v1);
  return ret;
}

HWY_API Vec256<uint64_t> MulEven(Vec256<uint64_t> a, const Vec256<uint64_t> b) {
  Vec256<uint64_t> ret;
  ret.v0 = MulEven(a.v0, b.v0);
  ret.v1 = MulEven(a.v1, b.v1);
  return ret;
}
HWY_API Vec256<uint64_t> MulOdd(Vec256<uint64_t> a, const Vec256<uint64_t> b) {
  Vec256<uint64_t> ret;
  ret.v0 = MulOdd(a.v0, b.v0);
  ret.v1 = MulOdd(a.v1, b.v1);
  return ret;
}

// ------------------------------ Negate
template <typename T>
HWY_API Vec256<T> Neg(Vec256<T> v) {
  v.v0 = Neg(v.v0);
  v.v1 = Neg(v.v1);
  return v;
}

// ------------------------------ Floating-point division
template <typename T>
HWY_API Vec256<T> operator/(Vec256<T> a, const Vec256<T> b) {
  a.v0 /= b.v0;
  a.v1 /= b.v1;
  return a;
}

// Approximate reciprocal
HWY_API Vec256<float> ApproximateReciprocal(const Vec256<float> v) {
  const Vec256<float> one = Set(Full256<float>(), 1.0f);
  return one / v;
}

// Absolute value of difference.
HWY_API Vec256<float> AbsDiff(const Vec256<float> a, const Vec256<float> b) {
  return Abs(a - b);
}

// ------------------------------ Floating-point multiply-add variants

HWY_API Vec256<float> MulAdd(Vec256<float> mul, Vec256<float> x,
                             Vec256<float> add) {
  mul.v0 = MulAdd(mul.v0, x.v0, add.v0);
  mul.v1 = MulAdd(mul.v1, x.v1, add.v1);
  return mul;
}

HWY_API Vec256<float> NegMulAdd(Vec256<float> mul, Vec256<float> x,
                                Vec256<float> add) {
  mul.v0 = NegMulAdd(mul.v0, x.v0, add.v0);
  mul.v1 = NegMulAdd(mul.v1, x.v1, add.v1);
  return mul;
}

HWY_API Vec256<float> MulSub(Vec256<float> mul, Vec256<float> x,
                             Vec256<float> sub) {
  mul.v0 = MulSub(mul.v0, x.v0, sub.v0);
  mul.v1 = MulSub(mul.v1, x.v1, sub.v1);
  return mul;
}

HWY_API Vec256<float> NegMulSub(Vec256<float> mul, Vec256<float> x,
                                Vec256<float> sub) {
  mul.v0 = NegMulSub(mul.v0, x.v0, sub.v0);
  mul.v1 = NegMulSub(mul.v1, x.v1, sub.v1);
  return mul;
}

// ------------------------------ Floating-point square root

template <typename T>
HWY_API Vec256<T> Sqrt(Vec256<T> v) {
  v.v0 = Sqrt(v.v0);
  v.v1 = Sqrt(v.v1);
  return v;
}

// Approximate reciprocal square root
HWY_API Vec256<float> ApproximateReciprocalSqrt(const Vec256<float> v) {
  // TODO(eustas): find cheaper a way to calculate this.
  const Vec256<float> one = Set(Full256<float>(), 1.0f);
  return one / Sqrt(v);
}

// ------------------------------ Floating-point rounding

// Toward nearest integer, ties to even
HWY_API Vec256<float> Round(Vec256<float> v) {
  v.v0 = Round(v.v0);
  v.v1 = Round(v.v1);
  return v;
}

// Toward zero, aka truncate
HWY_API Vec256<float> Trunc(Vec256<float> v) {
  v.v0 = Trunc(v.v0);
  v.v1 = Trunc(v.v1);
  return v;
}

// Toward +infinity, aka ceiling
HWY_API Vec256<float> Ceil(Vec256<float> v) {
  v.v0 = Ceil(v.v0);
  v.v1 = Ceil(v.v1);
  return v;
}

// Toward -infinity, aka floor
HWY_API Vec256<float> Floor(Vec256<float> v) {
  v.v0 = Floor(v.v0);
  v.v1 = Floor(v.v1);
  return v;
}

// ------------------------------ Floating-point classification

template <typename T>
HWY_API Mask256<T> IsNaN(const Vec256<T> v) {
  return v != v;
}

template <typename T, HWY_IF_FLOAT(T)>
HWY_API Mask256<T> IsInf(const Vec256<T> v) {
  const DFromV<decltype(v)> d;
  const RebindToSigned<decltype(d)> di;
  const VFromD<decltype(di)> vi = BitCast(di, v);
  // 'Shift left' to clear the sign bit, check for exponent=max and mantissa=0.
  return RebindMask(d, Eq(Add(vi, vi), Set(di, hwy::MaxExponentTimes2<T>())));
}

// Returns whether normal/subnormal/zero.
template <typename T, HWY_IF_FLOAT(T)>
HWY_API Mask256<T> IsFinite(const Vec256<T> v) {
  const DFromV<decltype(v)> d;
  const RebindToUnsigned<decltype(d)> du;
  const RebindToSigned<decltype(d)> di;  // cheaper than unsigned comparison
  const VFromD<decltype(du)> vu = BitCast(du, v);
  // 'Shift left' to clear the sign bit, then right so we can compare with the
  // max exponent (cannot compare with MaxExponentTimes2 directly because it is
  // negative and non-negative floats would be greater).
  const VFromD<decltype(di)> exp =
      BitCast(di, ShiftRight<hwy::MantissaBits<T>() + 1>(Add(vu, vu)));
  return RebindMask(d, Lt(exp, Set(di, hwy::MaxExponentField<T>())));
}

// ================================================== COMPARE

// Comparisons fill a lane with 1-bits if the condition is true, else 0.

template <class DTo, typename TFrom, typename TTo = TFromD<DTo>>
HWY_API MFromD<DTo> RebindMask(DTo /*tag*/, Mask256<TFrom> m) {
  static_assert(sizeof(TFrom) == sizeof(TTo), "Must have same size");
  return MFromD<DTo>{Mask128<TTo>{m.m0.raw}, Mask128<TTo>{m.m1.raw}};
}

template <typename T>
HWY_API Mask256<T> TestBit(Vec256<T> v, Vec256<T> bit) {
  static_assert(!hwy::IsFloat<T>(), "Only integer vectors supported");
  return (v & bit) == bit;
}

template <typename T>
HWY_API Mask256<T> operator==(Vec256<T> a, const Vec256<T> b) {
  Mask256<T> m;
  m.m0 = operator==(a.v0, b.v0);
  m.m1 = operator==(a.v1, b.v1);
  return m;
}

template <typename T>
HWY_API Mask256<T> operator!=(Vec256<T> a, const Vec256<T> b) {
  Mask256<T> m;
  m.m0 = operator!=(a.v0, b.v0);
  m.m1 = operator!=(a.v1, b.v1);
  return m;
}

template <typename T>
HWY_API Mask256<T> operator<(Vec256<T> a, const Vec256<T> b) {
  Mask256<T> m;
  m.m0 = operator<(a.v0, b.v0);
  m.m1 = operator<(a.v1, b.v1);
  return m;
}

template <typename T>
HWY_API Mask256<T> operator>(Vec256<T> a, const Vec256<T> b) {
  Mask256<T> m;
  m.m0 = operator>(a.v0, b.v0);
  m.m1 = operator>(a.v1, b.v1);
  return m;
}

template <typename T>
HWY_API Mask256<T> operator<=(Vec256<T> a, const Vec256<T> b) {
  Mask256<T> m;
  m.m0 = operator<=(a.v0, b.v0);
  m.m1 = operator<=(a.v1, b.v1);
  return m;
}

template <typename T>
HWY_API Mask256<T> operator>=(Vec256<T> a, const Vec256<T> b) {
  Mask256<T> m;
  m.m0 = operator>=(a.v0, b.v0);
  m.m1 = operator>=(a.v1, b.v1);
  return m;
}

// ------------------------------ FirstN (Iota, Lt)

template <class D, HWY_IF_V_SIZE_D(D, 32)>
HWY_API MFromD<D> FirstN(const D d, size_t num) {
  const RebindToSigned<decltype(d)> di;  // Signed comparisons may be cheaper.
  using TI = TFromD<decltype(di)>;
  return RebindMask(d, Iota(di, 0) < Set(di, static_cast<TI>(num)));
}

// ================================================== LOGICAL

template <typename T>
HWY_API Vec256<T> Not(Vec256<T> v) {
  v.v0 = Not(v.v0);
  v.v1 = Not(v.v1);
  return v;
}

template <typename T>
HWY_API Vec256<T> And(Vec256<T> a, Vec256<T> b) {
  a.v0 = And(a.v0, b.v0);
  a.v1 = And(a.v1, b.v1);
  return a;
}

template <typename T>
HWY_API Vec256<T> AndNot(Vec256<T> not_mask, Vec256<T> mask) {
  not_mask.v0 = AndNot(not_mask.v0, mask.v0);
  not_mask.v1 = AndNot(not_mask.v1, mask.v1);
  return not_mask;
}

template <typename T>
HWY_API Vec256<T> Or(Vec256<T> a, Vec256<T> b) {
  a.v0 = Or(a.v0, b.v0);
  a.v1 = Or(a.v1, b.v1);
  return a;
}

template <typename T>
HWY_API Vec256<T> Xor(Vec256<T> a, Vec256<T> b) {
  a.v0 = Xor(a.v0, b.v0);
  a.v1 = Xor(a.v1, b.v1);
  return a;
}

template <typename T>
HWY_API Vec256<T> Xor3(Vec256<T> x1, Vec256<T> x2, Vec256<T> x3) {
  return Xor(x1, Xor(x2, x3));
}

template <typename T>
HWY_API Vec256<T> Or3(Vec256<T> o1, Vec256<T> o2, Vec256<T> o3) {
  return Or(o1, Or(o2, o3));
}

template <typename T>
HWY_API Vec256<T> OrAnd(Vec256<T> o, Vec256<T> a1, Vec256<T> a2) {
  return Or(o, And(a1, a2));
}

template <typename T>
HWY_API Vec256<T> IfVecThenElse(Vec256<T> mask, Vec256<T> yes, Vec256<T> no) {
  return IfThenElse(MaskFromVec(mask), yes, no);
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

// ------------------------------ CopySign

template <typename T>
HWY_API Vec256<T> CopySign(const Vec256<T> magn, const Vec256<T> sign) {
  static_assert(IsFloat<T>(), "Only makes sense for floating-point");
  const auto msb = SignBit(DFromV<decltype(magn)>());
  return Or(AndNot(msb, magn), And(msb, sign));
}

template <typename T>
HWY_API Vec256<T> CopySignToAbs(const Vec256<T> abs, const Vec256<T> sign) {
  static_assert(IsFloat<T>(), "Only makes sense for floating-point");
  return Or(abs, And(SignBit(DFromV<decltype(sign)>()), sign));
}

// ------------------------------ Mask

// Mask and Vec are the same (true = FF..FF).
template <typename T>
HWY_API Mask256<T> MaskFromVec(const Vec256<T> v) {
  Mask256<T> m;
  m.m0 = MaskFromVec(v.v0);
  m.m1 = MaskFromVec(v.v1);
  return m;
}

template <class D, typename T = TFromD<D>>
HWY_API Vec256<T> VecFromMask(D d, Mask256<T> m) {
  const Half<decltype(d)> dh;
  Vec256<T> v;
  v.v0 = VecFromMask(dh, m.m0);
  v.v1 = VecFromMask(dh, m.m1);
  return v;
}

// mask ? yes : no
template <typename T>
HWY_API Vec256<T> IfThenElse(Mask256<T> mask, Vec256<T> yes, Vec256<T> no) {
  yes.v0 = IfThenElse(mask.m0, yes.v0, no.v0);
  yes.v1 = IfThenElse(mask.m1, yes.v1, no.v1);
  return yes;
}

// mask ? yes : 0
template <typename T>
HWY_API Vec256<T> IfThenElseZero(Mask256<T> mask, Vec256<T> yes) {
  return yes & VecFromMask(DFromV<decltype(yes)>(), mask);
}

// mask ? 0 : no
template <typename T>
HWY_API Vec256<T> IfThenZeroElse(Mask256<T> mask, Vec256<T> no) {
  return AndNot(VecFromMask(DFromV<decltype(no)>(), mask), no);
}

template <typename T>
HWY_API Vec256<T> IfNegativeThenElse(Vec256<T> v, Vec256<T> yes, Vec256<T> no) {
  v.v0 = IfNegativeThenElse(v.v0, yes.v0, no.v0);
  v.v1 = IfNegativeThenElse(v.v1, yes.v1, no.v1);
  return v;
}

template <typename T, HWY_IF_FLOAT(T)>
HWY_API Vec256<T> ZeroIfNegative(Vec256<T> v) {
  return IfThenZeroElse(v < Zero(DFromV<decltype(v)>()), v);
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

template <typename T>
HWY_API Mask256<T> ExclusiveNeither(const Mask256<T> a, Mask256<T> b) {
  const Full256<T> d;
  return MaskFromVec(AndNot(VecFromMask(d, a), Not(VecFromMask(d, b))));
}

// ------------------------------ Shl (BroadcastSignBit, IfThenElse)
template <typename T>
HWY_API Vec256<T> operator<<(Vec256<T> v, const Vec256<T> bits) {
  v.v0 = operator<<(v.v0, bits.v0);
  v.v1 = operator<<(v.v1, bits.v1);
  return v;
}

// ------------------------------ Shr (BroadcastSignBit, IfThenElse)
template <typename T>
HWY_API Vec256<T> operator>>(Vec256<T> v, const Vec256<T> bits) {
  v.v0 = operator>>(v.v0, bits.v0);
  v.v1 = operator>>(v.v1, bits.v1);
  return v;
}

// ------------------------------ BroadcastSignBit (compare, VecFromMask)

template <typename T, HWY_IF_NOT_T_SIZE(T, 1)>
HWY_API Vec256<T> BroadcastSignBit(const Vec256<T> v) {
  return ShiftRight<sizeof(T) * 8 - 1>(v);
}
HWY_API Vec256<int8_t> BroadcastSignBit(const Vec256<int8_t> v) {
  const DFromV<decltype(v)> d;
  return VecFromMask(d, v < Zero(d));
}

// ================================================== MEMORY

// ------------------------------ Load

template <class D, HWY_IF_V_SIZE_D(D, 32)>
HWY_API VFromD<D> Load(D d, const TFromD<D>* HWY_RESTRICT aligned) {
  const Half<decltype(d)> dh;
  VFromD<D> ret;
  ret.v0 = Load(dh, aligned);
  ret.v1 = Load(dh, aligned + Lanes(dh));
  return ret;
}

template <class D, typename T = TFromD<D>>
HWY_API Vec256<T> MaskedLoad(Mask256<T> m, D d, const T* HWY_RESTRICT aligned) {
  return IfThenElseZero(m, Load(d, aligned));
}

template <class D, typename T = TFromD<D>>
HWY_API Vec256<T> MaskedLoadOr(Vec256<T> v, Mask256<T> m, D d,
                               const T* HWY_RESTRICT aligned) {
  return IfThenElse(m, Load(d, aligned), v);
}

// LoadU == Load.
template <class D, HWY_IF_V_SIZE_D(D, 32)>
HWY_API VFromD<D> LoadU(D d, const TFromD<D>* HWY_RESTRICT p) {
  return Load(d, p);
}

template <class D, HWY_IF_V_SIZE_D(D, 32)>
HWY_API VFromD<D> LoadDup128(D d, const TFromD<D>* HWY_RESTRICT p) {
  const Half<decltype(d)> dh;
  VFromD<D> ret;
  ret.v0 = ret.v1 = Load(dh, p);
  return ret;
}

// ------------------------------ Store

template <class D, typename T = TFromD<D>>
HWY_API void Store(Vec256<T> v, D d, T* HWY_RESTRICT aligned) {
  const Half<decltype(d)> dh;
  Store(v.v0, dh, aligned);
  Store(v.v1, dh, aligned + Lanes(dh));
}

// StoreU == Store.
template <class D, typename T = TFromD<D>>
HWY_API void StoreU(Vec256<T> v, D d, T* HWY_RESTRICT p) {
  Store(v, d, p);
}

template <class D, typename T = TFromD<D>>
HWY_API void BlendedStore(Vec256<T> v, Mask256<T> m, D d, T* HWY_RESTRICT p) {
  StoreU(IfThenElse(m, v, LoadU(d, p)), d, p);
}

// ------------------------------ Stream
template <class D, typename T = TFromD<D>>
HWY_API void Stream(Vec256<T> v, D d, T* HWY_RESTRICT aligned) {
  // Same as aligned stores.
  Store(v, d, aligned);
}

// ------------------------------ Scatter, Gather defined in wasm_128

// ================================================== SWIZZLE

// ------------------------------ ExtractLane
template <typename T>
HWY_API T ExtractLane(const Vec256<T> v, size_t i) {
  alignas(32) T lanes[32 / sizeof(T)];
  Store(v, DFromV<decltype(v)>(), lanes);
  return lanes[i];
}

// ------------------------------ InsertLane
template <typename T>
HWY_API Vec256<T> InsertLane(const Vec256<T> v, size_t i, T t) {
  DFromV<decltype(v)> d;
  alignas(32) T lanes[32 / sizeof(T)];
  Store(v, d, lanes);
  lanes[i] = t;
  return Load(d, lanes);
}

// ------------------------------ LowerHalf

template <class D, typename T = TFromD<D>>
HWY_API Vec128<T> LowerHalf(D /* tag */, Vec256<T> v) {
  return v.v0;
}

template <typename T>
HWY_API Vec128<T> LowerHalf(Vec256<T> v) {
  return v.v0;
}

// ------------------------------ GetLane (LowerHalf)
template <typename T>
HWY_API T GetLane(const Vec256<T> v) {
  return GetLane(LowerHalf(v));
}

// ------------------------------ ShiftLeftBytes

template <int kBytes, class D, typename T = TFromD<D>>
HWY_API Vec256<T> ShiftLeftBytes(D d, Vec256<T> v) {
  const Half<decltype(d)> dh;
  v.v0 = ShiftLeftBytes<kBytes>(dh, v.v0);
  v.v1 = ShiftLeftBytes<kBytes>(dh, v.v1);
  return v;
}

template <int kBytes, typename T>
HWY_API Vec256<T> ShiftLeftBytes(Vec256<T> v) {
  return ShiftLeftBytes<kBytes>(DFromV<decltype(v)>(), v);
}

// ------------------------------ ShiftLeftLanes

template <int kLanes, class D, typename T = TFromD<D>>
HWY_API Vec256<T> ShiftLeftLanes(D d, const Vec256<T> v) {
  const Repartition<uint8_t, decltype(d)> d8;
  return BitCast(d, ShiftLeftBytes<kLanes * sizeof(T)>(BitCast(d8, v)));
}

template <int kLanes, typename T>
HWY_API Vec256<T> ShiftLeftLanes(const Vec256<T> v) {
  return ShiftLeftLanes<kLanes>(DFromV<decltype(v)>(), v);
}

// ------------------------------ ShiftRightBytes
template <int kBytes, class D, typename T = TFromD<D>>
HWY_API Vec256<T> ShiftRightBytes(D d, Vec256<T> v) {
  const Half<decltype(d)> dh;
  v.v0 = ShiftRightBytes<kBytes>(dh, v.v0);
  v.v1 = ShiftRightBytes<kBytes>(dh, v.v1);
  return v;
}

// ------------------------------ ShiftRightLanes
template <int kLanes, class D, typename T = TFromD<D>>
HWY_API Vec256<T> ShiftRightLanes(D d, const Vec256<T> v) {
  const Repartition<uint8_t, decltype(d)> d8;
  return BitCast(d, ShiftRightBytes<kLanes * sizeof(T)>(d8, BitCast(d8, v)));
}

// ------------------------------ UpperHalf (ShiftRightBytes)
template <class D, typename T = TFromD<D>>
HWY_API Vec128<T> UpperHalf(D /* tag */, const Vec256<T> v) {
  return v.v1;
}

// ------------------------------ CombineShiftRightBytes

template <int kBytes, class D, typename T = TFromD<D>>
HWY_API Vec256<T> CombineShiftRightBytes(D d, Vec256<T> hi, Vec256<T> lo) {
  const Half<decltype(d)> dh;
  hi.v0 = CombineShiftRightBytes<kBytes>(dh, hi.v0, lo.v0);
  hi.v1 = CombineShiftRightBytes<kBytes>(dh, hi.v1, lo.v1);
  return hi;
}

// ------------------------------ Broadcast/splat any lane

template <int kLane, typename T>
HWY_API Vec256<T> Broadcast(const Vec256<T> v) {
  Vec256<T> ret;
  ret.v0 = Broadcast<kLane>(v.v0);
  ret.v1 = Broadcast<kLane>(v.v1);
  return ret;
}

// ------------------------------ TableLookupBytes

// Both full
template <typename T, typename TI>
HWY_API Vec256<TI> TableLookupBytes(const Vec256<T> bytes, Vec256<TI> from) {
  from.v0 = TableLookupBytes(bytes.v0, from.v0);
  from.v1 = TableLookupBytes(bytes.v1, from.v1);
  return from;
}

// Partial index vector
template <typename T, typename TI, size_t NI>
HWY_API Vec128<TI, NI> TableLookupBytes(Vec256<T> bytes,
                                        const Vec128<TI, NI> from) {
  // First expand to full 128, then 256.
  const auto from_256 = ZeroExtendVector(Full256<TI>(), Vec128<TI>{from.raw});
  const auto tbl_full = TableLookupBytes(bytes, from_256);
  // Shrink to 128, then partial.
  return Vec128<TI, NI>{LowerHalf(Full128<TI>(), tbl_full).raw};
}

// Partial table vector
template <typename T, size_t N, typename TI>
HWY_API Vec256<TI> TableLookupBytes(Vec128<T, N> bytes, const Vec256<TI> from) {
  // First expand to full 128, then 256.
  const auto bytes_256 = ZeroExtendVector(Full256<T>(), Vec128<T>{bytes.raw});
  return TableLookupBytes(bytes_256, from);
}

// Partial both are handled by wasm_128.

template <class V, class VI>
HWY_API VI TableLookupBytesOr0(V bytes, VI from) {
  // wasm out-of-bounds policy already zeros, so TableLookupBytes is fine.
  return TableLookupBytes(bytes, from);
}

// ------------------------------ Hard-coded shuffles

template <typename T>
HWY_API Vec256<T> Shuffle01(Vec256<T> v) {
  v.v0 = Shuffle01(v.v0);
  v.v1 = Shuffle01(v.v1);
  return v;
}

template <typename T>
HWY_API Vec256<T> Shuffle2301(Vec256<T> v) {
  v.v0 = Shuffle2301(v.v0);
  v.v1 = Shuffle2301(v.v1);
  return v;
}

template <typename T>
HWY_API Vec256<T> Shuffle1032(Vec256<T> v) {
  v.v0 = Shuffle1032(v.v0);
  v.v1 = Shuffle1032(v.v1);
  return v;
}

template <typename T>
HWY_API Vec256<T> Shuffle0321(Vec256<T> v) {
  v.v0 = Shuffle0321(v.v0);
  v.v1 = Shuffle0321(v.v1);
  return v;
}

template <typename T>
HWY_API Vec256<T> Shuffle2103(Vec256<T> v) {
  v.v0 = Shuffle2103(v.v0);
  v.v1 = Shuffle2103(v.v1);
  return v;
}

template <typename T>
HWY_API Vec256<T> Shuffle0123(Vec256<T> v) {
  v.v0 = Shuffle0123(v.v0);
  v.v1 = Shuffle0123(v.v1);
  return v;
}

// Used by generic_ops-inl.h
namespace detail {

template <typename T, HWY_IF_T_SIZE(T, 4)>
HWY_API Vec256<T> ShuffleTwo2301(Vec256<T> a, const Vec256<T> b) {
  a.v0 = ShuffleTwo2301(a.v0, b.v0);
  a.v1 = ShuffleTwo2301(a.v1, b.v1);
  return a;
}
template <typename T, HWY_IF_T_SIZE(T, 4)>
HWY_API Vec256<T> ShuffleTwo1230(Vec256<T> a, const Vec256<T> b) {
  a.v0 = ShuffleTwo1230(a.v0, b.v0);
  a.v1 = ShuffleTwo1230(a.v1, b.v1);
  return a;
}
template <typename T, HWY_IF_T_SIZE(T, 4)>
HWY_API Vec256<T> ShuffleTwo3012(Vec256<T> a, const Vec256<T> b) {
  a.v0 = ShuffleTwo3012(a.v0, b.v0);
  a.v1 = ShuffleTwo3012(a.v1, b.v1);
  return a;
}

}  // namespace detail

// ------------------------------ TableLookupLanes

// Returned by SetTableIndices for use by TableLookupLanes.
template <typename T>
struct Indices256 {
  __v128_u i0;
  __v128_u i1;
};

template <class D, typename T = TFromD<D>, typename TI>
HWY_API Indices256<T> IndicesFromVec(D /* tag */, Vec256<TI> vec) {
  static_assert(sizeof(T) == sizeof(TI), "Index size must match lane");
  Indices256<T> ret;
  ret.i0 = vec.v0.raw;
  ret.i1 = vec.v1.raw;
  return ret;
}

template <class D, HWY_IF_V_SIZE_D(D, 32), typename TI>
HWY_API Indices256<TFromD<D>> SetTableIndices(D d, const TI* idx) {
  const Rebind<TI, decltype(d)> di;
  return IndicesFromVec(d, LoadU(di, idx));
}

template <typename T>
HWY_API Vec256<T> TableLookupLanes(const Vec256<T> v, Indices256<T> idx) {
  const DFromV<decltype(v)> d;
  const Half<decltype(d)> dh;
  const auto idx_i0 = IndicesFromVec(dh, Vec128<T>{idx.i0});
  const auto idx_i1 = IndicesFromVec(dh, Vec128<T>{idx.i1});

  Vec256<T> result;
  result.v0 = TwoTablesLookupLanes(v.v0, v.v1, idx_i0);
  result.v1 = TwoTablesLookupLanes(v.v0, v.v1, idx_i1);
  return result;
}

template <typename T>
HWY_API Vec256<T> TableLookupLanesOr0(Vec256<T> v, Indices256<T> idx) {
  // The out of bounds behavior will already zero lanes.
  return TableLookupLanesOr0(v, idx);
}

template <typename T>
HWY_API Vec256<T> TwoTablesLookupLanes(const Vec256<T> a, const Vec256<T> b,
                                       Indices256<T> idx) {
  const DFromV<decltype(a)> d;
  const Half<decltype(d)> dh;
  const RebindToUnsigned<decltype(d)> du;
  using TU = MakeUnsigned<T>;
  constexpr size_t kLanesPerVect = 32 / sizeof(TU);

  Vec256<TU> vi;
  vi.v0 = Vec128<TU>{idx.i0};
  vi.v1 = Vec128<TU>{idx.i1};
  const auto vmod = vi & Set(du, TU{kLanesPerVect - 1});
  const auto is_lo = RebindMask(d, vi == vmod);

  const auto idx_i0 = IndicesFromVec(dh, vmod.v0);
  const auto idx_i1 = IndicesFromVec(dh, vmod.v1);

  Vec256<T> result_lo;
  Vec256<T> result_hi;
  result_lo.v0 = TwoTablesLookupLanes(a.v0, a.v1, idx_i0);
  result_lo.v1 = TwoTablesLookupLanes(a.v0, a.v1, idx_i1);
  result_hi.v0 = TwoTablesLookupLanes(b.v0, b.v1, idx_i0);
  result_hi.v1 = TwoTablesLookupLanes(b.v0, b.v1, idx_i1);
  return IfThenElse(is_lo, result_lo, result_hi);
}

// ------------------------------ Reverse
template <class D, typename T = TFromD<D>>
HWY_API Vec256<T> Reverse(D d, const Vec256<T> v) {
  const Half<decltype(d)> dh;
  Vec256<T> ret;
  ret.v1 = Reverse(dh, v.v0);  // note reversed v1 member order
  ret.v0 = Reverse(dh, v.v1);
  return ret;
}

// ------------------------------ Reverse2
template <class D, typename T = TFromD<D>>
HWY_API Vec256<T> Reverse2(D d, Vec256<T> v) {
  const Half<decltype(d)> dh;
  v.v0 = Reverse2(dh, v.v0);
  v.v1 = Reverse2(dh, v.v1);
  return v;
}

// ------------------------------ Reverse4

// Each block has only 2 lanes, so swap blocks and their lanes.
template <class D, typename T = TFromD<D>, HWY_IF_T_SIZE(T, 8)>
HWY_API Vec256<T> Reverse4(D d, const Vec256<T> v) {
  const Half<decltype(d)> dh;
  Vec256<T> ret;
  ret.v0 = Reverse2(dh, v.v1);  // swapped
  ret.v1 = Reverse2(dh, v.v0);
  return ret;
}

template <class D, typename T = TFromD<D>, HWY_IF_NOT_T_SIZE(T, 8)>
HWY_API Vec256<T> Reverse4(D d, Vec256<T> v) {
  const Half<decltype(d)> dh;
  v.v0 = Reverse4(dh, v.v0);
  v.v1 = Reverse4(dh, v.v1);
  return v;
}

// ------------------------------ Reverse8

template <class D, typename T = TFromD<D>, HWY_IF_T_SIZE(T, 8)>
HWY_API Vec256<T> Reverse8(D /* tag */, Vec256<T> /* v */) {
  HWY_ASSERT(0);  // don't have 8 u64 lanes
}

// Each block has only 4 lanes, so swap blocks and their lanes.
template <class D, typename T = TFromD<D>, HWY_IF_T_SIZE(T, 4)>
HWY_API Vec256<T> Reverse8(D d, const Vec256<T> v) {
  const Half<decltype(d)> dh;
  Vec256<T> ret;
  ret.v0 = Reverse4(dh, v.v1);  // swapped
  ret.v1 = Reverse4(dh, v.v0);
  return ret;
}

template <class D, typename T = TFromD<D>,
          HWY_IF_T_SIZE_ONE_OF(T, (1 << 1) | (1 << 2))>
HWY_API Vec256<T> Reverse8(D d, Vec256<T> v) {
  const Half<decltype(d)> dh;
  v.v0 = Reverse8(dh, v.v0);
  v.v1 = Reverse8(dh, v.v1);
  return v;
}

// ------------------------------ InterleaveLower

template <typename T>
HWY_API Vec256<T> InterleaveLower(Vec256<T> a, Vec256<T> b) {
  a.v0 = InterleaveLower(a.v0, b.v0);
  a.v1 = InterleaveLower(a.v1, b.v1);
  return a;
}

// wasm_128 already defines a template with D, V, V args.

// ------------------------------ InterleaveUpper (UpperHalf)

template <class D, typename T = TFromD<D>>
HWY_API Vec256<T> InterleaveUpper(D d, Vec256<T> a, Vec256<T> b) {
  const Half<decltype(d)> dh;
  a.v0 = InterleaveUpper(dh, a.v0, b.v0);
  a.v1 = InterleaveUpper(dh, a.v1, b.v1);
  return a;
}

// ------------------------------ ZipLower/ZipUpper defined in wasm_128

// ================================================== COMBINE

// ------------------------------ Combine (InterleaveLower)
template <class D, typename T = TFromD<D>>
HWY_API Vec256<T> Combine(D /* d */, Vec128<T> hi, Vec128<T> lo) {
  Vec256<T> ret;
  ret.v1 = hi;
  ret.v0 = lo;
  return ret;
}

// ------------------------------ ZeroExtendVector (Combine)
template <class D, typename T = TFromD<D>>
HWY_API Vec256<T> ZeroExtendVector(D d, Vec128<T> lo) {
  const Half<decltype(d)> dh;
  return Combine(d, Zero(dh), lo);
}

// ------------------------------ ZeroExtendResizeBitCast

namespace detail {

template <size_t kFromVectSize, class DTo, class DFrom,
          HWY_IF_LANES_LE(kFromVectSize, 8)>
HWY_INLINE VFromD<DTo> ZeroExtendResizeBitCast(
    hwy::SizeTag<kFromVectSize> /* from_size_tag */,
    hwy::SizeTag<32> /* to_size_tag */, DTo d_to, DFrom d_from,
    VFromD<DFrom> v) {
  const Half<decltype(d_to)> dh_to;
  return ZeroExtendVector(d_to, ZeroExtendResizeBitCast(dh_to, d_from, v));
}

}  // namespace detail

// ------------------------------ ConcatLowerLower
template <class D, typename T = TFromD<D>>
HWY_API Vec256<T> ConcatLowerLower(D /* tag */, Vec256<T> hi, Vec256<T> lo) {
  Vec256<T> ret;
  ret.v1 = hi.v0;
  ret.v0 = lo.v0;
  return ret;
}

// ------------------------------ ConcatUpperUpper
template <class D, typename T = TFromD<D>>
HWY_API Vec256<T> ConcatUpperUpper(D /* tag */, Vec256<T> hi, Vec256<T> lo) {
  Vec256<T> ret;
  ret.v1 = hi.v1;
  ret.v0 = lo.v1;
  return ret;
}

// ------------------------------ ConcatLowerUpper
template <class D, typename T = TFromD<D>>
HWY_API Vec256<T> ConcatLowerUpper(D /* tag */, Vec256<T> hi, Vec256<T> lo) {
  Vec256<T> ret;
  ret.v1 = hi.v0;
  ret.v0 = lo.v1;
  return ret;
}

// ------------------------------ ConcatUpperLower
template <class D, typename T = TFromD<D>>
HWY_API Vec256<T> ConcatUpperLower(D /* tag */, Vec256<T> hi, Vec256<T> lo) {
  Vec256<T> ret;
  ret.v1 = hi.v1;
  ret.v0 = lo.v0;
  return ret;
}

// ------------------------------ ConcatOdd
template <class D, typename T = TFromD<D>>
HWY_API Vec256<T> ConcatOdd(D d, Vec256<T> hi, Vec256<T> lo) {
  const Half<decltype(d)> dh;
  Vec256<T> ret;
  ret.v0 = ConcatOdd(dh, lo.v1, lo.v0);
  ret.v1 = ConcatOdd(dh, hi.v1, hi.v0);
  return ret;
}

// ------------------------------ ConcatEven
template <class D, typename T = TFromD<D>>
HWY_API Vec256<T> ConcatEven(D d, Vec256<T> hi, Vec256<T> lo) {
  const Half<decltype(d)> dh;
  Vec256<T> ret;
  ret.v0 = ConcatEven(dh, lo.v1, lo.v0);
  ret.v1 = ConcatEven(dh, hi.v1, hi.v0);
  return ret;
}

// ------------------------------ DupEven
template <typename T>
HWY_API Vec256<T> DupEven(Vec256<T> v) {
  v.v0 = DupEven(v.v0);
  v.v1 = DupEven(v.v1);
  return v;
}

// ------------------------------ DupOdd
template <typename T>
HWY_API Vec256<T> DupOdd(Vec256<T> v) {
  v.v0 = DupOdd(v.v0);
  v.v1 = DupOdd(v.v1);
  return v;
}

// ------------------------------ OddEven
template <typename T>
HWY_API Vec256<T> OddEven(Vec256<T> a, const Vec256<T> b) {
  a.v0 = OddEven(a.v0, b.v0);
  a.v1 = OddEven(a.v1, b.v1);
  return a;
}

// ------------------------------ OddEvenBlocks
template <typename T>
HWY_API Vec256<T> OddEvenBlocks(Vec256<T> odd, Vec256<T> even) {
  odd.v0 = even.v0;
  return odd;
}

// ------------------------------ SwapAdjacentBlocks
template <typename T>
HWY_API Vec256<T> SwapAdjacentBlocks(Vec256<T> v) {
  Vec256<T> ret;
  ret.v0 = v.v1;  // swapped order
  ret.v1 = v.v0;
  return ret;
}

// ------------------------------ ReverseBlocks
template <class D, typename T = TFromD<D>>
HWY_API Vec256<T> ReverseBlocks(D /* tag */, const Vec256<T> v) {
  return SwapAdjacentBlocks(v);  // 2 blocks, so Swap = Reverse
}

// ================================================== CONVERT

// ------------------------------ Promotions (part w/ narrow lanes -> full)

namespace detail {

// Unsigned: zero-extend.
template <class D, HWY_IF_U16_D(D)>
HWY_API Vec128<uint16_t> PromoteUpperTo(D /* tag */, Vec128<uint8_t> v) {
  return Vec128<uint16_t>{wasm_u16x8_extend_high_u8x16(v.raw)};
}
template <class D, HWY_IF_U32_D(D)>
HWY_API Vec128<uint32_t> PromoteUpperTo(D /* tag */, Vec128<uint8_t> v) {
  return Vec128<uint32_t>{
      wasm_u32x4_extend_high_u16x8(wasm_u16x8_extend_high_u8x16(v.raw))};
}
template <class D, HWY_IF_I16_D(D)>
HWY_API Vec128<int16_t> PromoteUpperTo(D /* tag */, Vec128<uint8_t> v) {
  return Vec128<int16_t>{wasm_u16x8_extend_high_u8x16(v.raw)};
}
template <class D, HWY_IF_I32_D(D)>
HWY_API Vec128<int32_t> PromoteUpperTo(D /* tag */, Vec128<uint8_t> v) {
  return Vec128<int32_t>{
      wasm_u32x4_extend_high_u16x8(wasm_u16x8_extend_high_u8x16(v.raw))};
}
template <class D, HWY_IF_U32_D(D)>
HWY_API Vec128<uint32_t> PromoteUpperTo(D /* tag */, Vec128<uint16_t> v) {
  return Vec128<uint32_t>{wasm_u32x4_extend_high_u16x8(v.raw)};
}
template <class D, HWY_IF_U64_D(D)>
HWY_API Vec128<uint64_t> PromoteUpperTo(D /* tag */, Vec128<uint32_t> v) {
  return Vec128<uint64_t>{wasm_u64x2_extend_high_u32x4(v.raw)};
}
template <class D, HWY_IF_I32_D(D)>
HWY_API Vec128<int32_t> PromoteUpperTo(D /* tag */, Vec128<uint16_t> v) {
  return Vec128<int32_t>{wasm_u32x4_extend_high_u16x8(v.raw)};
}
template <class D, HWY_IF_I64_D(D)>
HWY_API Vec128<int64_t> PromoteUpperTo(D /* tag */, Vec128<uint32_t> v) {
  return Vec128<int64_t>{wasm_u64x2_extend_high_u32x4(v.raw)};
}

// Signed: replicate sign bit.
template <class D, HWY_IF_I16_D(D)>
HWY_API Vec128<int16_t> PromoteUpperTo(D /* tag */, Vec128<int8_t> v) {
  return Vec128<int16_t>{wasm_i16x8_extend_high_i8x16(v.raw)};
}
template <class D, HWY_IF_I32_D(D)>
HWY_API Vec128<int32_t> PromoteUpperTo(D /* tag */, Vec128<int8_t> v) {
  return Vec128<int32_t>{
      wasm_i32x4_extend_high_i16x8(wasm_i16x8_extend_high_i8x16(v.raw))};
}
template <class D, HWY_IF_I32_D(D)>
HWY_API Vec128<int32_t> PromoteUpperTo(D /* tag */, Vec128<int16_t> v) {
  return Vec128<int32_t>{wasm_i32x4_extend_high_i16x8(v.raw)};
}
template <class D, HWY_IF_I64_D(D)>
HWY_API Vec128<int64_t> PromoteUpperTo(D /* tag */, Vec128<int32_t> v) {
  return Vec128<int64_t>{wasm_i64x2_extend_high_i32x4(v.raw)};
}

template <class D, HWY_IF_F64_D(D)>
HWY_API Vec128<double> PromoteUpperTo(D dd, Vec128<int32_t> v) {
  // There is no wasm_f64x2_convert_high_i32x4.
  const Full64<int32_t> di32h;
  return PromoteTo(dd, UpperHalf(di32h, v));
}

template <class D, HWY_IF_F32_D(D)>
HWY_API Vec128<float> PromoteUpperTo(D df32, Vec128<float16_t> v) {
  const RebindToSigned<decltype(df32)> di32;
  const RebindToUnsigned<decltype(df32)> du32;
  // Expand to u32 so we can shift.
  const auto bits16 = PromoteUpperTo(du32, Vec128<uint16_t>{v.raw});
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

template <class D, HWY_IF_F32_D(D)>
HWY_API Vec128<float> PromoteUpperTo(D df32, Vec128<bfloat16_t> v) {
  const Full128<uint16_t> du16;
  const RebindToSigned<decltype(df32)> di32;
  return BitCast(df32, ShiftLeft<16>(PromoteUpperTo(di32, BitCast(du16, v))));
}

}  // namespace detail

template <class D, HWY_IF_V_SIZE_D(D, 32), typename TN,
          HWY_IF_T_SIZE_D(D, sizeof(TN) * 2)>
HWY_API VFromD<D> PromoteTo(D d, Vec128<TN> v) {
  const Half<decltype(d)> dh;
  VFromD<D> ret;
  ret.v0 = PromoteTo(dh, LowerHalf(v));
  ret.v1 = detail::PromoteUpperTo(dh, v);
  return ret;
}

// 4x promotion: 8-bit to 32-bit or 16-bit to 64-bit
template <class DW, HWY_IF_V_SIZE_D(DW, 32),
          HWY_IF_T_SIZE_ONE_OF_D(DW, (1 << 4) | (1 << 8)),
          HWY_IF_NOT_FLOAT_D(DW), typename TN,
          HWY_IF_T_SIZE_D(DW, sizeof(TN) * 4),
          HWY_IF_NOT_FLOAT_NOR_SPECIAL(TN)>
HWY_API Vec256<TFromD<DW>> PromoteTo(DW d, Vec64<TN> v) {
  const Half<decltype(d)> dh;
  // 16-bit lanes for UI8->UI32, 32-bit lanes for UI16->UI64
  const Rebind<MakeWide<TN>, decltype(d)> d2;
  const auto v_2x = PromoteTo(d2, v);
  Vec256<TFromD<DW>> ret;
  ret.v0 = PromoteTo(dh, LowerHalf(v_2x));
  ret.v1 = detail::PromoteUpperTo(dh, v_2x);
  return ret;
}

// 8x promotion: 8-bit to 64-bit
template <class DW, HWY_IF_V_SIZE_D(DW, 32), HWY_IF_T_SIZE_D(DW, 8),
          HWY_IF_NOT_FLOAT_D(DW), typename TN, HWY_IF_T_SIZE(TN, 1)>
HWY_API Vec256<TFromD<DW>> PromoteTo(DW d, Vec32<TN> v) {
  const Half<decltype(d)> dh;
  const Repartition<MakeWide<MakeWide<TN>>, decltype(dh)> d4;  // 32-bit lanes
  const auto v32 = PromoteTo(d4, v);
  Vec256<TFromD<DW>> ret;
  ret.v0 = PromoteTo(dh, LowerHalf(v32));
  ret.v1 = detail::PromoteUpperTo(dh, v32);
  return ret;
}

// ------------------------------ DemoteTo

template <class D, HWY_IF_U16_D(D)>
HWY_API Vec128<uint16_t> DemoteTo(D /* tag */, Vec256<int32_t> v) {
  return Vec128<uint16_t>{wasm_u16x8_narrow_i32x4(v.v0.raw, v.v1.raw)};
}

template <class D, HWY_IF_I16_D(D)>
HWY_API Vec128<int16_t> DemoteTo(D /* tag */, Vec256<int32_t> v) {
  return Vec128<int16_t>{wasm_i16x8_narrow_i32x4(v.v0.raw, v.v1.raw)};
}

template <class D, HWY_IF_U8_D(D)>
HWY_API Vec64<uint8_t> DemoteTo(D /* tag */, Vec256<int32_t> v) {
  const auto intermediate = wasm_i16x8_narrow_i32x4(v.v0.raw, v.v1.raw);
  return Vec64<uint8_t>{wasm_u8x16_narrow_i16x8(intermediate, intermediate)};
}

template <class D, HWY_IF_U8_D(D)>
HWY_API Vec128<uint8_t> DemoteTo(D /* tag */, Vec256<int16_t> v) {
  return Vec128<uint8_t>{wasm_u8x16_narrow_i16x8(v.v0.raw, v.v1.raw)};
}

template <class D, HWY_IF_I8_D(D)>
HWY_API Vec64<int8_t> DemoteTo(D /* tag */, Vec256<int32_t> v) {
  const auto intermediate = wasm_i16x8_narrow_i32x4(v.v0.raw, v.v1.raw);
  return Vec64<int8_t>{wasm_i8x16_narrow_i16x8(intermediate, intermediate)};
}

template <class D, HWY_IF_I8_D(D)>
HWY_API Vec128<int8_t> DemoteTo(D /* tag */, Vec256<int16_t> v) {
  return Vec128<int8_t>{wasm_i8x16_narrow_i16x8(v.v0.raw, v.v1.raw)};
}

template <class D, HWY_IF_I32_D(D)>
HWY_API Vec128<int32_t> DemoteTo(D di, Vec256<double> v) {
  const Vec64<int32_t> lo{wasm_i32x4_trunc_sat_f64x2_zero(v.v0.raw)};
  const Vec64<int32_t> hi{wasm_i32x4_trunc_sat_f64x2_zero(v.v1.raw)};
  return Combine(di, hi, lo);
}

template <class D, HWY_IF_F16_D(D)>
HWY_API Vec128<float16_t> DemoteTo(D d16, Vec256<float> v) {
  const Half<decltype(d16)> d16h;
  const Vec64<float16_t> lo = DemoteTo(d16h, v.v0);
  const Vec64<float16_t> hi = DemoteTo(d16h, v.v1);
  return Combine(d16, hi, lo);
}

template <class D, HWY_IF_BF16_D(D)>
HWY_API Vec128<bfloat16_t> DemoteTo(D dbf16, Vec256<float> v) {
  const Half<decltype(dbf16)> dbf16h;
  const Vec64<bfloat16_t> lo = DemoteTo(dbf16h, v.v0);
  const Vec64<bfloat16_t> hi = DemoteTo(dbf16h, v.v1);
  return Combine(dbf16, hi, lo);
}

// For already range-limited input [0, 255].
HWY_API Vec64<uint8_t> U8FromU32(Vec256<uint32_t> v) {
  const Full64<uint8_t> du8;
  const Full256<int32_t> di32;  // no unsigned DemoteTo
  return DemoteTo(du8, BitCast(di32, v));
}

// ------------------------------ Truncations

template <class D, HWY_IF_U8_D(D)>
HWY_API Vec32<uint8_t> TruncateTo(D /* tag */, Vec256<uint64_t> v) {
  return Vec32<uint8_t>{wasm_i8x16_shuffle(v.v0.raw, v.v1.raw, 0, 8, 16, 24, 0,
                                           8, 16, 24, 0, 8, 16, 24, 0, 8, 16,
                                           24)};
}

template <class D, HWY_IF_U16_D(D)>
HWY_API Vec64<uint16_t> TruncateTo(D /* tag */, Vec256<uint64_t> v) {
  return Vec64<uint16_t>{wasm_i8x16_shuffle(v.v0.raw, v.v1.raw, 0, 1, 8, 9, 16,
                                            17, 24, 25, 0, 1, 8, 9, 16, 17, 24,
                                            25)};
}

template <class D, HWY_IF_U32_D(D)>
HWY_API Vec128<uint32_t> TruncateTo(D /* tag */, Vec256<uint64_t> v) {
  return Vec128<uint32_t>{wasm_i8x16_shuffle(v.v0.raw, v.v1.raw, 0, 1, 2, 3, 8,
                                             9, 10, 11, 16, 17, 18, 19, 24, 25,
                                             26, 27)};
}

template <class D, HWY_IF_U8_D(D)>
HWY_API Vec64<uint8_t> TruncateTo(D /* tag */, Vec256<uint32_t> v) {
  return Vec64<uint8_t>{wasm_i8x16_shuffle(v.v0.raw, v.v1.raw, 0, 4, 8, 12, 16,
                                           20, 24, 28, 0, 4, 8, 12, 16, 20, 24,
                                           28)};
}

template <class D, HWY_IF_U16_D(D)>
HWY_API Vec128<uint16_t> TruncateTo(D /* tag */, Vec256<uint32_t> v) {
  return Vec128<uint16_t>{wasm_i8x16_shuffle(v.v0.raw, v.v1.raw, 0, 1, 4, 5, 8,
                                             9, 12, 13, 16, 17, 20, 21, 24, 25,
                                             28, 29)};
}

template <class D, HWY_IF_U8_D(D)>
HWY_API Vec128<uint8_t> TruncateTo(D /* tag */, Vec256<uint16_t> v) {
  return Vec128<uint8_t>{wasm_i8x16_shuffle(v.v0.raw, v.v1.raw, 0, 2, 4, 6, 8,
                                            10, 12, 14, 16, 18, 20, 22, 24, 26,
                                            28, 30)};
}

// ------------------------------ ReorderDemote2To
template <class DBF16, HWY_IF_BF16_D(DBF16)>
HWY_API Vec256<bfloat16_t> ReorderDemote2To(DBF16 dbf16, Vec256<float> a,
                                            Vec256<float> b) {
  const RebindToUnsigned<decltype(dbf16)> du16;
  return BitCast(dbf16, ConcatOdd(du16, BitCast(du16, b), BitCast(du16, a)));
}

template <class DN, typename V, HWY_IF_V_SIZE_D(DN, 32),
          HWY_IF_NOT_FLOAT_NOR_SPECIAL(TFromD<DN>), HWY_IF_SIGNED_V(V),
          HWY_IF_T_SIZE_ONE_OF_D(DN, (1 << 1) | (1 << 2) | (1 << 4)),
          HWY_IF_T_SIZE_V(V, sizeof(TFromD<DN>) * 2)>
HWY_API VFromD<DN> ReorderDemote2To(DN dn, V a, V b) {
  const Half<decltype(dn)> dnh;
  VFromD<DN> demoted;
  demoted.v0 = DemoteTo(dnh, a);
  demoted.v1 = DemoteTo(dnh, b);
  return demoted;
}

template <class DN, typename V, HWY_IF_V_SIZE_D(DN, 32), HWY_IF_UNSIGNED_D(DN),
          HWY_IF_UNSIGNED_V(V),
          HWY_IF_T_SIZE_ONE_OF_D(DN, (1 << 1) | (1 << 2) | (1 << 4)),
          HWY_IF_T_SIZE_V(V, sizeof(TFromD<DN>) * 2)>
HWY_API VFromD<DN> ReorderDemote2To(DN dn, V a, V b) {
  const Half<decltype(dn)> dnh;
  VFromD<DN> demoted;
  demoted.v0 = DemoteTo(dnh, a);
  demoted.v1 = DemoteTo(dnh, b);
  return demoted;
}

// ------------------------------ Convert i32 <=> f32 (Round)

template <class DTo, typename TFrom, typename TTo = TFromD<DTo>>
HWY_API Vec256<TTo> ConvertTo(DTo d, const Vec256<TFrom> v) {
  const Half<decltype(d)> dh;
  Vec256<TTo> ret;
  ret.v0 = ConvertTo(dh, v.v0);
  ret.v1 = ConvertTo(dh, v.v1);
  return ret;
}

HWY_API Vec256<int32_t> NearestInt(const Vec256<float> v) {
  return ConvertTo(Full256<int32_t>(), Round(v));
}

// ================================================== MISC

// ------------------------------ LoadMaskBits (TestBit)

// `p` points to at least 8 readable bytes, not all of which need be valid.
template <class D, HWY_IF_V_SIZE_D(D, 32),
          HWY_IF_T_SIZE_ONE_OF_D(D, (1 << 4) | (1 << 8))>
HWY_API MFromD<D> LoadMaskBits(D d, const uint8_t* HWY_RESTRICT bits) {
  const Half<decltype(d)> dh;
  MFromD<D> ret;
  ret.m0 = LoadMaskBits(dh, bits);
  // If size=4, one 128-bit vector has 4 mask bits; otherwise 2 for size=8.
  // Both halves fit in one byte's worth of mask bits.
  constexpr size_t kBitsPerHalf = 16 / sizeof(TFromD<D>);
  const uint8_t bits_upper[8] = {static_cast<uint8_t>(bits[0] >> kBitsPerHalf)};
  ret.m1 = LoadMaskBits(dh, bits_upper);
  return ret;
}

template <class D, HWY_IF_V_SIZE_D(D, 32),
          HWY_IF_T_SIZE_ONE_OF_D(D, (1 << 1) | (1 << 2))>
HWY_API MFromD<D> LoadMaskBits(D d, const uint8_t* HWY_RESTRICT bits) {
  const Half<decltype(d)> dh;
  MFromD<D> ret;
  ret.m0 = LoadMaskBits(dh, bits);
  constexpr size_t kLanesPerHalf = 16 / sizeof(TFromD<D>);
  constexpr size_t kBytesPerHalf = kLanesPerHalf / 8;
  static_assert(kBytesPerHalf != 0, "Lane size <= 16 bits => at least 8 lanes");
  ret.m1 = LoadMaskBits(dh, bits + kBytesPerHalf);
  return ret;
}

// ------------------------------ Mask

// `p` points to at least 8 writable bytes.
template <class D, typename T = TFromD<D>,
          HWY_IF_T_SIZE_ONE_OF(T, (1 << 4) | (1 << 8))>
HWY_API size_t StoreMaskBits(D d, const Mask256<T> mask, uint8_t* bits) {
  const Half<decltype(d)> dh;
  StoreMaskBits(dh, mask.m0, bits);
  const uint8_t lo = bits[0];
  StoreMaskBits(dh, mask.m1, bits);
  // If size=4, one 128-bit vector has 4 mask bits; otherwise 2 for size=8.
  // Both halves fit in one byte's worth of mask bits.
  constexpr size_t kBitsPerHalf = 16 / sizeof(T);
  bits[0] = static_cast<uint8_t>(lo | (bits[0] << kBitsPerHalf));
  return (kBitsPerHalf * 2 + 7) / 8;
}

template <class D, typename T = TFromD<D>,
          HWY_IF_T_SIZE_ONE_OF(T, (1 << 1) | (1 << 2))>
HWY_API size_t StoreMaskBits(D d, const Mask256<T> mask, uint8_t* bits) {
  const Half<decltype(d)> dh;
  constexpr size_t kLanesPerHalf = 16 / sizeof(T);
  constexpr size_t kBytesPerHalf = kLanesPerHalf / 8;
  static_assert(kBytesPerHalf != 0, "Lane size <= 16 bits => at least 8 lanes");
  StoreMaskBits(dh, mask.m0, bits);
  StoreMaskBits(dh, mask.m1, bits + kBytesPerHalf);
  return kBytesPerHalf * 2;
}

template <class D, typename T = TFromD<D>>
HWY_API size_t CountTrue(D d, const Mask256<T> m) {
  const Half<decltype(d)> dh;
  return CountTrue(dh, m.m0) + CountTrue(dh, m.m1);
}

template <class D, typename T = TFromD<D>>
HWY_API bool AllFalse(D d, const Mask256<T> m) {
  const Half<decltype(d)> dh;
  return AllFalse(dh, m.m0) && AllFalse(dh, m.m1);
}

template <class D, typename T = TFromD<D>>
HWY_API bool AllTrue(D d, const Mask256<T> m) {
  const Half<decltype(d)> dh;
  return AllTrue(dh, m.m0) && AllTrue(dh, m.m1);
}

template <class D, typename T = TFromD<D>>
HWY_API size_t FindKnownFirstTrue(D d, const Mask256<T> mask) {
  const Half<decltype(d)> dh;
  const intptr_t lo = FindFirstTrue(dh, mask.m0);  // not known
  constexpr size_t kLanesPerHalf = 16 / sizeof(T);
  return lo >= 0 ? static_cast<size_t>(lo)
                 : kLanesPerHalf + FindKnownFirstTrue(dh, mask.m1);
}

template <class D, typename T = TFromD<D>>
HWY_API intptr_t FindFirstTrue(D d, const Mask256<T> mask) {
  const Half<decltype(d)> dh;
  const intptr_t lo = FindFirstTrue(dh, mask.m0);
  constexpr int kLanesPerHalf = 16 / sizeof(T);
  if (lo >= 0) return lo;

  const intptr_t hi = FindFirstTrue(dh, mask.m1);
  return hi + (hi >= 0 ? kLanesPerHalf : 0);
}

template <class D, typename T = TFromD<D>>
HWY_API size_t FindKnownLastTrue(D d, const Mask256<T> mask) {
  const Half<decltype(d)> dh;
  const intptr_t hi = FindLastTrue(dh, mask.m1);  // not known
  constexpr size_t kLanesPerHalf = 16 / sizeof(T);
  return hi >= 0 ? kLanesPerHalf + static_cast<size_t>(hi)
                 : FindKnownLastTrue(dh, mask.m0);
}

template <class D, typename T = TFromD<D>>
HWY_API intptr_t FindLastTrue(D d, const Mask256<T> mask) {
  const Half<decltype(d)> dh;
  constexpr int kLanesPerHalf = 16 / sizeof(T);
  const intptr_t hi = FindLastTrue(dh, mask.m1);
  return hi >= 0 ? kLanesPerHalf + hi : FindLastTrue(dh, mask.m0);
}

// ------------------------------ CompressStore
template <class D, typename T = TFromD<D>>
HWY_API size_t CompressStore(Vec256<T> v, const Mask256<T> mask, D d,
                             T* HWY_RESTRICT unaligned) {
  const Half<decltype(d)> dh;
  const size_t count = CompressStore(v.v0, mask.m0, dh, unaligned);
  const size_t count2 = CompressStore(v.v1, mask.m1, dh, unaligned + count);
  return count + count2;
}

// ------------------------------ CompressBlendedStore
template <class D, typename T = TFromD<D>>
HWY_API size_t CompressBlendedStore(Vec256<T> v, const Mask256<T> m, D d,
                                    T* HWY_RESTRICT unaligned) {
  const Half<decltype(d)> dh;
  const size_t count = CompressBlendedStore(v.v0, m.m0, dh, unaligned);
  const size_t count2 = CompressBlendedStore(v.v1, m.m1, dh, unaligned + count);
  return count + count2;
}

// ------------------------------ CompressBitsStore

template <class D, typename T = TFromD<D>>
HWY_API size_t CompressBitsStore(Vec256<T> v, const uint8_t* HWY_RESTRICT bits,
                                 D d, T* HWY_RESTRICT unaligned) {
  const Mask256<T> m = LoadMaskBits(d, bits);
  return CompressStore(v, m, d, unaligned);
}

// ------------------------------ Compress
template <typename T>
HWY_API Vec256<T> Compress(const Vec256<T> v, const Mask256<T> mask) {
  const DFromV<decltype(v)> d;
  alignas(32) T lanes[32 / sizeof(T)] = {};
  (void)CompressStore(v, mask, d, lanes);
  return Load(d, lanes);
}

// ------------------------------ CompressNot
template <typename T>
HWY_API Vec256<T> CompressNot(Vec256<T> v, const Mask256<T> mask) {
  return Compress(v, Not(mask));
}

// ------------------------------ CompressBlocksNot
HWY_API Vec256<uint64_t> CompressBlocksNot(Vec256<uint64_t> v,
                                           Mask256<uint64_t> mask) {
  const Full128<uint64_t> dh;
  // Because the non-selected (mask=1) blocks are undefined, we can return the
  // input unless mask = 01, in which case we must bring down the upper block.
  return AllTrue(dh, AndNot(mask.m1, mask.m0)) ? SwapAdjacentBlocks(v) : v;
}

// ------------------------------ CompressBits
template <typename T>
HWY_API Vec256<T> CompressBits(Vec256<T> v, const uint8_t* HWY_RESTRICT bits) {
  const Mask256<T> m = LoadMaskBits(DFromV<decltype(v)>(), bits);
  return Compress(v, m);
}

// ------------------------------ Expand
template <typename T>
HWY_API Vec256<T> Expand(const Vec256<T> v, const Mask256<T> mask) {
  Vec256<T> ret;
  const Full256<T> d;
  const Half<decltype(d)> dh;
  alignas(32) T lanes[32 / sizeof(T)] = {};
  Store(v, d, lanes);
  ret.v0 = Expand(v.v0, mask.m0);
  ret.v1 = Expand(LoadU(dh, lanes + CountTrue(dh, mask.m0)), mask.m1);
  return ret;
}

// ------------------------------ LoadExpand
template <class D, HWY_IF_V_SIZE_D(D, 32)>
HWY_API VFromD<D> LoadExpand(MFromD<D> mask, D d,
                             const TFromD<D>* HWY_RESTRICT unaligned) {
  return Expand(LoadU(d, unaligned), mask);
}

// ------------------------------ LoadInterleaved3/4

// Implemented in generic_ops, we just overload LoadTransposedBlocks3/4.

namespace detail {

// Input:
// 1 0 (<- first block of unaligned)
// 3 2
// 5 4
// Output:
// 3 0
// 4 1
// 5 2
template <class D, typename T = TFromD<D>>
HWY_API void LoadTransposedBlocks3(D d, const T* HWY_RESTRICT unaligned,
                                   Vec256<T>& A, Vec256<T>& B, Vec256<T>& C) {
  const Vec256<T> v10 = LoadU(d, unaligned + 0 * MaxLanes(d));
  const Vec256<T> v32 = LoadU(d, unaligned + 1 * MaxLanes(d));
  const Vec256<T> v54 = LoadU(d, unaligned + 2 * MaxLanes(d));

  A = ConcatUpperLower(d, v32, v10);
  B = ConcatLowerUpper(d, v54, v10);
  C = ConcatUpperLower(d, v54, v32);
}

// Input (128-bit blocks):
// 1 0 (first block of unaligned)
// 3 2
// 5 4
// 7 6
// Output:
// 4 0 (LSB of A)
// 5 1
// 6 2
// 7 3
template <class D, typename T = TFromD<D>>
HWY_API void LoadTransposedBlocks4(D d, const T* HWY_RESTRICT unaligned,
                                   Vec256<T>& vA, Vec256<T>& vB, Vec256<T>& vC,
                                   Vec256<T>& vD) {
  const Vec256<T> v10 = LoadU(d, unaligned + 0 * MaxLanes(d));
  const Vec256<T> v32 = LoadU(d, unaligned + 1 * MaxLanes(d));
  const Vec256<T> v54 = LoadU(d, unaligned + 2 * MaxLanes(d));
  const Vec256<T> v76 = LoadU(d, unaligned + 3 * MaxLanes(d));

  vA = ConcatLowerLower(d, v54, v10);
  vB = ConcatUpperUpper(d, v54, v10);
  vC = ConcatLowerLower(d, v76, v32);
  vD = ConcatUpperUpper(d, v76, v32);
}

}  // namespace detail

// ------------------------------ StoreInterleaved2/3/4 (ConcatUpperLower)

// Implemented in generic_ops, we just overload StoreTransposedBlocks2/3/4.

namespace detail {

// Input (128-bit blocks):
// 2 0 (LSB of i)
// 3 1
// Output:
// 1 0
// 3 2
template <class D, typename T = TFromD<D>>
HWY_API void StoreTransposedBlocks2(Vec256<T> i, Vec256<T> j, D d,
                                    T* HWY_RESTRICT unaligned) {
  const Vec256<T> out0 = ConcatLowerLower(d, j, i);
  const Vec256<T> out1 = ConcatUpperUpper(d, j, i);
  StoreU(out0, d, unaligned + 0 * MaxLanes(d));
  StoreU(out1, d, unaligned + 1 * MaxLanes(d));
}

// Input (128-bit blocks):
// 3 0 (LSB of i)
// 4 1
// 5 2
// Output:
// 1 0
// 3 2
// 5 4
template <class D, typename T = TFromD<D>>
HWY_API void StoreTransposedBlocks3(Vec256<T> i, Vec256<T> j, Vec256<T> k, D d,
                                    T* HWY_RESTRICT unaligned) {
  const Vec256<T> out0 = ConcatLowerLower(d, j, i);
  const Vec256<T> out1 = ConcatUpperLower(d, i, k);
  const Vec256<T> out2 = ConcatUpperUpper(d, k, j);
  StoreU(out0, d, unaligned + 0 * MaxLanes(d));
  StoreU(out1, d, unaligned + 1 * MaxLanes(d));
  StoreU(out2, d, unaligned + 2 * MaxLanes(d));
}

// Input (128-bit blocks):
// 4 0 (LSB of i)
// 5 1
// 6 2
// 7 3
// Output:
// 1 0
// 3 2
// 5 4
// 7 6
template <class D, typename T = TFromD<D>>
HWY_API void StoreTransposedBlocks4(Vec256<T> i, Vec256<T> j, Vec256<T> k,
                                    Vec256<T> l, D d,
                                    T* HWY_RESTRICT unaligned) {
  // Write lower halves, then upper.
  const Vec256<T> out0 = ConcatLowerLower(d, j, i);
  const Vec256<T> out1 = ConcatLowerLower(d, l, k);
  StoreU(out0, d, unaligned + 0 * MaxLanes(d));
  StoreU(out1, d, unaligned + 1 * MaxLanes(d));
  const Vec256<T> out2 = ConcatUpperUpper(d, j, i);
  const Vec256<T> out3 = ConcatUpperUpper(d, l, k);
  StoreU(out2, d, unaligned + 2 * MaxLanes(d));
  StoreU(out3, d, unaligned + 3 * MaxLanes(d));
}

}  // namespace detail

// ------------------------------ ReorderWidenMulAccumulate
template <class D32, typename T16, typename T32 = TFromD<D32>>
HWY_API Vec256<T32> ReorderWidenMulAccumulate(D32 d32, Vec256<T16> a,
                                              Vec256<T16> b, Vec256<T32> sum0,
                                              Vec256<T32>& sum1) {
  const Half<decltype(d32)> d32h;
  sum0.v0 = ReorderWidenMulAccumulate(d32h, a.v0, b.v0, sum0.v0, sum1.v0);
  sum0.v1 = ReorderWidenMulAccumulate(d32h, a.v1, b.v1, sum0.v1, sum1.v1);
  return sum0;
}

// ------------------------------ RearrangeToOddPlusEven
template <typename TW>
HWY_API Vec256<TW> RearrangeToOddPlusEven(Vec256<TW> sum0, Vec256<TW> sum1) {
  sum0.v0 = RearrangeToOddPlusEven(sum0.v0, sum1.v0);
  sum0.v1 = RearrangeToOddPlusEven(sum0.v1, sum1.v1);
  return sum0;
}

// ------------------------------ Reductions

template <class D, typename T = TFromD<D>>
HWY_API Vec256<T> SumOfLanes(D d, const Vec256<T> v) {
  const Half<decltype(d)> dh;
  const Vec128<T> lo = SumOfLanes(dh, Add(v.v0, v.v1));
  return Combine(d, lo, lo);
}

template <class D, typename T = TFromD<D>>
HWY_API T ReduceSum(D d, const Vec256<T> v) {
  const Half<decltype(d)> dh;
  return ReduceSum(dh, Add(v.v0, v.v1));
}

template <class D, typename T = TFromD<D>>
HWY_API Vec256<T> MinOfLanes(D d, const Vec256<T> v) {
  const Half<decltype(d)> dh;
  const Vec128<T> lo = MinOfLanes(dh, Min(v.v0, v.v1));
  return Combine(d, lo, lo);
}

template <class D, typename T = TFromD<D>>
HWY_API Vec256<T> MaxOfLanes(D d, const Vec256<T> v) {
  const Half<decltype(d)> dh;
  const Vec128<T> lo = MaxOfLanes(dh, Max(v.v0, v.v1));
  return Combine(d, lo, lo);
}

// ------------------------------ Lt128

template <class D, typename T = TFromD<D>>
HWY_INLINE Mask256<T> Lt128(D d, Vec256<T> a, Vec256<T> b) {
  const Half<decltype(d)> dh;
  Mask256<T> ret;
  ret.m0 = Lt128(dh, a.v0, b.v0);
  ret.m1 = Lt128(dh, a.v1, b.v1);
  return ret;
}

template <class D, typename T = TFromD<D>>
HWY_INLINE Mask256<T> Lt128Upper(D d, Vec256<T> a, Vec256<T> b) {
  const Half<decltype(d)> dh;
  Mask256<T> ret;
  ret.m0 = Lt128Upper(dh, a.v0, b.v0);
  ret.m1 = Lt128Upper(dh, a.v1, b.v1);
  return ret;
}

template <class D, typename T = TFromD<D>>
HWY_INLINE Mask256<T> Eq128(D d, Vec256<T> a, Vec256<T> b) {
  const Half<decltype(d)> dh;
  Mask256<T> ret;
  ret.m0 = Eq128(dh, a.v0, b.v0);
  ret.m1 = Eq128(dh, a.v1, b.v1);
  return ret;
}

template <class D, typename T = TFromD<D>>
HWY_INLINE Mask256<T> Eq128Upper(D d, Vec256<T> a, Vec256<T> b) {
  const Half<decltype(d)> dh;
  Mask256<T> ret;
  ret.m0 = Eq128Upper(dh, a.v0, b.v0);
  ret.m1 = Eq128Upper(dh, a.v1, b.v1);
  return ret;
}

template <class D, typename T = TFromD<D>>
HWY_INLINE Mask256<T> Ne128(D d, Vec256<T> a, Vec256<T> b) {
  const Half<decltype(d)> dh;
  Mask256<T> ret;
  ret.m0 = Ne128(dh, a.v0, b.v0);
  ret.m1 = Ne128(dh, a.v1, b.v1);
  return ret;
}

template <class D, typename T = TFromD<D>>
HWY_INLINE Mask256<T> Ne128Upper(D d, Vec256<T> a, Vec256<T> b) {
  const Half<decltype(d)> dh;
  Mask256<T> ret;
  ret.m0 = Ne128Upper(dh, a.v0, b.v0);
  ret.m1 = Ne128Upper(dh, a.v1, b.v1);
  return ret;
}

template <class D, typename T = TFromD<D>>
HWY_INLINE Vec256<T> Min128(D d, Vec256<T> a, Vec256<T> b) {
  const Half<decltype(d)> dh;
  Vec256<T> ret;
  ret.v0 = Min128(dh, a.v0, b.v0);
  ret.v1 = Min128(dh, a.v1, b.v1);
  return ret;
}

template <class D, typename T = TFromD<D>>
HWY_INLINE Vec256<T> Max128(D d, Vec256<T> a, Vec256<T> b) {
  const Half<decltype(d)> dh;
  Vec256<T> ret;
  ret.v0 = Max128(dh, a.v0, b.v0);
  ret.v1 = Max128(dh, a.v1, b.v1);
  return ret;
}

template <class D, typename T = TFromD<D>>
HWY_INLINE Vec256<T> Min128Upper(D d, Vec256<T> a, Vec256<T> b) {
  const Half<decltype(d)> dh;
  Vec256<T> ret;
  ret.v0 = Min128Upper(dh, a.v0, b.v0);
  ret.v1 = Min128Upper(dh, a.v1, b.v1);
  return ret;
}

template <class D, typename T = TFromD<D>>
HWY_INLINE Vec256<T> Max128Upper(D d, Vec256<T> a, Vec256<T> b) {
  const Half<decltype(d)> dh;
  Vec256<T> ret;
  ret.v0 = Max128Upper(dh, a.v0, b.v0);
  ret.v1 = Max128Upper(dh, a.v1, b.v1);
  return ret;
}

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace hwy
HWY_AFTER_NAMESPACE();
