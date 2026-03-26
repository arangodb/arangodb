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

// Single-element vectors and operations.
// External include guard in highway.h - see comment there.

#ifndef HWY_NO_LIBCXX
#include <math.h>  // sqrtf
#endif

#include "hwy/ops/shared-inl.h"

HWY_BEFORE_NAMESPACE();
namespace hwy {
namespace HWY_NAMESPACE {

// Single instruction, single data.
template <typename T>
using Sisd = Simd<T, 1, 0>;

// (Wrapper class required for overloading comparison operators.)
template <typename T>
struct Vec1 {
  using PrivateT = T;                     // only for DFromV
  static constexpr size_t kPrivateN = 1;  // only for DFromV

  HWY_INLINE Vec1() = default;
  Vec1(const Vec1&) = default;
  Vec1& operator=(const Vec1&) = default;
  HWY_INLINE explicit Vec1(const T t) : raw(t) {}

  HWY_INLINE Vec1& operator*=(const Vec1 other) {
    return *this = (*this * other);
  }
  HWY_INLINE Vec1& operator/=(const Vec1 other) {
    return *this = (*this / other);
  }
  HWY_INLINE Vec1& operator+=(const Vec1 other) {
    return *this = (*this + other);
  }
  HWY_INLINE Vec1& operator-=(const Vec1 other) {
    return *this = (*this - other);
  }
  HWY_INLINE Vec1& operator&=(const Vec1 other) {
    return *this = (*this & other);
  }
  HWY_INLINE Vec1& operator|=(const Vec1 other) {
    return *this = (*this | other);
  }
  HWY_INLINE Vec1& operator^=(const Vec1 other) {
    return *this = (*this ^ other);
  }

  T raw;
};

// 0 or FF..FF, same size as Vec1.
template <typename T>
class Mask1 {
  using Raw = hwy::MakeUnsigned<T>;

 public:
  static HWY_INLINE Mask1<T> FromBool(bool b) {
    Mask1<T> mask;
    mask.bits = b ? static_cast<Raw>(~Raw{0}) : 0;
    return mask;
  }

  Raw bits;
};

template <class V>
using DFromV = Simd<typename V::PrivateT, V::kPrivateN, 0>;

template <class V>
using TFromV = typename V::PrivateT;

// ------------------------------ BitCast

template <class DTo, typename TTo = TFromD<DTo>, typename TFrom>
HWY_API Vec1<TTo> BitCast(DTo /* tag */, Vec1<TFrom> v) {
  static_assert(sizeof(TTo) <= sizeof(TFrom), "Promoting is undefined");
  TTo to;
  CopyBytes<sizeof(TTo)>(&v.raw, &to);  // not same size - ok to shrink
  return Vec1<TTo>(to);
}

// ------------------------------ Zero

template <class D, HWY_IF_LANES_D(D, 1), typename T = TFromD<D>>
HWY_API Vec1<T> Zero(D /* tag */) {
  return Vec1<T>(T(0));
}

template <class D>
using VFromD = decltype(Zero(D()));

// ------------------------------ Tuple (VFromD)
#include "hwy/ops/tuple-inl.h"

// ------------------------------ Set
template <class D, HWY_IF_LANES_D(D, 1), typename T = TFromD<D>, typename T2>
HWY_API Vec1<T> Set(D /* tag */, const T2 t) {
  return Vec1<T>(static_cast<T>(t));
}

// ------------------------------ Undefined
template <class D, HWY_IF_LANES_D(D, 1), typename T = TFromD<D>>
HWY_API Vec1<T> Undefined(D d) {
  return Zero(d);
}

// ------------------------------ Iota
template <class D, HWY_IF_LANES_D(D, 1), typename T = TFromD<D>, typename T2>
HWY_API Vec1<T> Iota(const D /* tag */, const T2 first) {
  return Vec1<T>(static_cast<T>(first));
}

// ------------------------------ ResizeBitCast

template <class D, typename FromV>
HWY_API VFromD<D> ResizeBitCast(D /* tag */, FromV v) {
  using TFrom = TFromV<FromV>;
  using TTo = TFromD<D>;
  constexpr size_t kCopyLen = HWY_MIN(sizeof(TFrom), sizeof(TTo));
  TTo to = TTo{0};
  CopyBytes<kCopyLen>(&v.raw, &to);
  return VFromD<D>(to);
}

namespace detail {

// ResizeBitCast on the HWY_SCALAR target has zero-extending semantics if
// sizeof(TFromD<DTo>) is greater than sizeof(TFromV<FromV>)
template <class FromSizeTag, class ToSizeTag, class DTo, class DFrom>
HWY_INLINE VFromD<DTo> ZeroExtendResizeBitCast(
    FromSizeTag /* from_size_tag */, ToSizeTag /* to_size_tag */, DTo d_to, DFrom /*d_from*/,
    VFromD<DFrom> v) {
  return ResizeBitCast(d_to, v);
}

}  // namespace scalar


// ================================================== LOGICAL

// ------------------------------ Not

template <typename T>
HWY_API Vec1<T> Not(const Vec1<T> v) {
  using TU = MakeUnsigned<T>;
  const Sisd<TU> du;
  return BitCast(Sisd<T>(), Vec1<TU>(static_cast<TU>(~BitCast(du, v).raw)));
}

// ------------------------------ And

template <typename T>
HWY_API Vec1<T> And(const Vec1<T> a, const Vec1<T> b) {
  using TU = MakeUnsigned<T>;
  const Sisd<TU> du;
  return BitCast(Sisd<T>(), Vec1<TU>(BitCast(du, a).raw & BitCast(du, b).raw));
}
template <typename T>
HWY_API Vec1<T> operator&(const Vec1<T> a, const Vec1<T> b) {
  return And(a, b);
}

// ------------------------------ AndNot

template <typename T>
HWY_API Vec1<T> AndNot(const Vec1<T> a, const Vec1<T> b) {
  using TU = MakeUnsigned<T>;
  const Sisd<TU> du;
  return BitCast(Sisd<T>(), Vec1<TU>(static_cast<TU>(~BitCast(du, a).raw &
                                                     BitCast(du, b).raw)));
}

// ------------------------------ Or

template <typename T>
HWY_API Vec1<T> Or(const Vec1<T> a, const Vec1<T> b) {
  using TU = MakeUnsigned<T>;
  const Sisd<TU> du;
  return BitCast(Sisd<T>(), Vec1<TU>(BitCast(du, a).raw | BitCast(du, b).raw));
}
template <typename T>
HWY_API Vec1<T> operator|(const Vec1<T> a, const Vec1<T> b) {
  return Or(a, b);
}

// ------------------------------ Xor

template <typename T>
HWY_API Vec1<T> Xor(const Vec1<T> a, const Vec1<T> b) {
  using TU = MakeUnsigned<T>;
  const Sisd<TU> du;
  return BitCast(Sisd<T>(), Vec1<TU>(BitCast(du, a).raw ^ BitCast(du, b).raw));
}
template <typename T>
HWY_API Vec1<T> operator^(const Vec1<T> a, const Vec1<T> b) {
  return Xor(a, b);
}

// ------------------------------ Xor3

template <typename T>
HWY_API Vec1<T> Xor3(Vec1<T> x1, Vec1<T> x2, Vec1<T> x3) {
  return Xor(x1, Xor(x2, x3));
}

// ------------------------------ Or3

template <typename T>
HWY_API Vec1<T> Or3(Vec1<T> o1, Vec1<T> o2, Vec1<T> o3) {
  return Or(o1, Or(o2, o3));
}

// ------------------------------ OrAnd

template <typename T>
HWY_API Vec1<T> OrAnd(const Vec1<T> o, const Vec1<T> a1, const Vec1<T> a2) {
  return Or(o, And(a1, a2));
}

// ------------------------------ Mask

template <class DTo, typename TTo = TFromD<DTo>, typename TFrom>
HWY_API Mask1<TTo> RebindMask(DTo /*tag*/, Mask1<TFrom> m) {
  static_assert(sizeof(TFrom) == sizeof(TTo), "Must have same size");
  return Mask1<TTo>{m.bits};
}

// v must be 0 or FF..FF.
template <typename T>
HWY_API Mask1<T> MaskFromVec(const Vec1<T> v) {
  Mask1<T> mask;
  CopySameSize(&v, &mask);
  return mask;
}

template <class D>
using MFromD = decltype(MaskFromVec(VFromD<D>()));

template <typename T>
Vec1<T> VecFromMask(const Mask1<T> mask) {
  Vec1<T> v;
  CopySameSize(&mask, &v);
  return v;
}

template <class D, typename T = TFromD<D>>
Vec1<T> VecFromMask(D /* tag */, const Mask1<T> mask) {
  Vec1<T> v;
  CopySameSize(&mask, &v);
  return v;
}

template <class D, HWY_IF_LANES_D(D, 1), typename T = TFromD<D>>
HWY_API Mask1<T> FirstN(D /*tag*/, size_t n) {
  return Mask1<T>::FromBool(n != 0);
}

// ------------------------------ IfVecThenElse

template <typename T>
HWY_API Vec1<T> IfVecThenElse(Vec1<T> mask, Vec1<T> yes, Vec1<T> no) {
  return IfThenElse(MaskFromVec(mask), yes, no);
}

// ------------------------------ CopySign

template <typename T>
HWY_API Vec1<T> CopySign(const Vec1<T> magn, const Vec1<T> sign) {
  static_assert(IsFloat<T>(), "Only makes sense for floating-point");
  const auto msb = SignBit(Sisd<T>());
  return Or(AndNot(msb, magn), And(msb, sign));
}

template <typename T>
HWY_API Vec1<T> CopySignToAbs(const Vec1<T> abs, const Vec1<T> sign) {
  static_assert(IsFloat<T>(), "Only makes sense for floating-point");
  return Or(abs, And(SignBit(Sisd<T>()), sign));
}

// ------------------------------ BroadcastSignBit

template <typename T>
HWY_API Vec1<T> BroadcastSignBit(const Vec1<T> v) {
  // This is used inside ShiftRight, so we cannot implement in terms of it.
  return v.raw < 0 ? Vec1<T>(T(-1)) : Vec1<T>(0);
}

// ------------------------------ PopulationCount

#ifdef HWY_NATIVE_POPCNT
#undef HWY_NATIVE_POPCNT
#else
#define HWY_NATIVE_POPCNT
#endif

template <typename T>
HWY_API Vec1<T> PopulationCount(Vec1<T> v) {
  return Vec1<T>(static_cast<T>(PopCount(v.raw)));
}

// ------------------------------ IfThenElse

// Returns mask ? yes : no.
template <typename T>
HWY_API Vec1<T> IfThenElse(const Mask1<T> mask, const Vec1<T> yes,
                           const Vec1<T> no) {
  return mask.bits ? yes : no;
}

template <typename T>
HWY_API Vec1<T> IfThenElseZero(const Mask1<T> mask, const Vec1<T> yes) {
  return mask.bits ? yes : Vec1<T>(0);
}

template <typename T>
HWY_API Vec1<T> IfThenZeroElse(const Mask1<T> mask, const Vec1<T> no) {
  return mask.bits ? Vec1<T>(0) : no;
}

template <typename T>
HWY_API Vec1<T> IfNegativeThenElse(Vec1<T> v, Vec1<T> yes, Vec1<T> no) {
  return v.raw < 0 ? yes : no;
}

template <typename T>
HWY_API Vec1<T> ZeroIfNegative(const Vec1<T> v) {
  return v.raw < 0 ? Vec1<T>(0) : v;
}

// ------------------------------ Mask logical

template <typename T>
HWY_API Mask1<T> Not(const Mask1<T> m) {
  return MaskFromVec(Not(VecFromMask(Sisd<T>(), m)));
}

template <typename T>
HWY_API Mask1<T> And(const Mask1<T> a, Mask1<T> b) {
  const Sisd<T> d;
  return MaskFromVec(And(VecFromMask(d, a), VecFromMask(d, b)));
}

template <typename T>
HWY_API Mask1<T> AndNot(const Mask1<T> a, Mask1<T> b) {
  const Sisd<T> d;
  return MaskFromVec(AndNot(VecFromMask(d, a), VecFromMask(d, b)));
}

template <typename T>
HWY_API Mask1<T> Or(const Mask1<T> a, Mask1<T> b) {
  const Sisd<T> d;
  return MaskFromVec(Or(VecFromMask(d, a), VecFromMask(d, b)));
}

template <typename T>
HWY_API Mask1<T> Xor(const Mask1<T> a, Mask1<T> b) {
  const Sisd<T> d;
  return MaskFromVec(Xor(VecFromMask(d, a), VecFromMask(d, b)));
}

template <typename T>
HWY_API Mask1<T> ExclusiveNeither(const Mask1<T> a, Mask1<T> b) {
  const Sisd<T> d;
  return MaskFromVec(AndNot(VecFromMask(d, a), Not(VecFromMask(d, b))));
}

// ================================================== SHIFTS

// ------------------------------ ShiftLeft/ShiftRight (BroadcastSignBit)

template <int kBits, typename T>
HWY_API Vec1<T> ShiftLeft(const Vec1<T> v) {
  static_assert(0 <= kBits && kBits < sizeof(T) * 8, "Invalid shift");
  return Vec1<T>(
      static_cast<T>(static_cast<hwy::MakeUnsigned<T>>(v.raw) << kBits));
}

template <int kBits, typename T>
HWY_API Vec1<T> ShiftRight(const Vec1<T> v) {
  static_assert(0 <= kBits && kBits < sizeof(T) * 8, "Invalid shift");
#if __cplusplus >= 202002L
  // Signed right shift is now guaranteed to be arithmetic (rounding toward
  // negative infinity, i.e. shifting in the sign bit).
  return Vec1<T>(static_cast<T>(v.raw >> kBits));
#else
  if (IsSigned<T>()) {
    // Emulate arithmetic shift using only logical (unsigned) shifts, because
    // signed shifts are still implementation-defined.
    using TU = hwy::MakeUnsigned<T>;
    const Sisd<TU> du;
    const TU shifted = static_cast<TU>(BitCast(du, v).raw >> kBits);
    const TU sign = BitCast(du, BroadcastSignBit(v)).raw;
    const size_t sign_shift =
        static_cast<size_t>(static_cast<int>(sizeof(TU)) * 8 - 1 - kBits);
    const TU upper = static_cast<TU>(sign << sign_shift);
    return BitCast(Sisd<T>(), Vec1<TU>(shifted | upper));
  } else {  // T is unsigned
    return Vec1<T>(static_cast<T>(v.raw >> kBits));
  }
#endif
}

// ------------------------------ RotateRight (ShiftRight)
template <int kBits, typename T>
HWY_API Vec1<T> RotateRight(const Vec1<T> v) {
  constexpr size_t kSizeInBits = sizeof(T) * 8;
  static_assert(0 <= kBits && kBits < kSizeInBits, "Invalid shift");
  if (kBits == 0) return v;
  return Or(ShiftRight<kBits>(v),
            ShiftLeft<HWY_MIN(kSizeInBits - 1, kSizeInBits - kBits)>(v));
}

// ------------------------------ ShiftLeftSame (BroadcastSignBit)

template <typename T>
HWY_API Vec1<T> ShiftLeftSame(const Vec1<T> v, int bits) {
  return Vec1<T>(
      static_cast<T>(static_cast<hwy::MakeUnsigned<T>>(v.raw) << bits));
}

template <typename T>
HWY_API Vec1<T> ShiftRightSame(const Vec1<T> v, int bits) {
#if __cplusplus >= 202002L
  // Signed right shift is now guaranteed to be arithmetic (rounding toward
  // negative infinity, i.e. shifting in the sign bit).
  return Vec1<T>(static_cast<T>(v.raw >> bits));
#else
  if (IsSigned<T>()) {
    // Emulate arithmetic shift using only logical (unsigned) shifts, because
    // signed shifts are still implementation-defined.
    using TU = hwy::MakeUnsigned<T>;
    const Sisd<TU> du;
    const TU shifted = static_cast<TU>(BitCast(du, v).raw >> bits);
    const TU sign = BitCast(du, BroadcastSignBit(v)).raw;
    const size_t sign_shift =
        static_cast<size_t>(static_cast<int>(sizeof(TU)) * 8 - 1 - bits);
    const TU upper = static_cast<TU>(sign << sign_shift);
    return BitCast(Sisd<T>(), Vec1<TU>(shifted | upper));
  } else {  // T is unsigned
    return Vec1<T>(static_cast<T>(v.raw >> bits));
  }
#endif
}

// ------------------------------ Shl

// Single-lane => same as ShiftLeftSame except for the argument type.
template <typename T>
HWY_API Vec1<T> operator<<(const Vec1<T> v, const Vec1<T> bits) {
  return ShiftLeftSame(v, static_cast<int>(bits.raw));
}

template <typename T>
HWY_API Vec1<T> operator>>(const Vec1<T> v, const Vec1<T> bits) {
  return ShiftRightSame(v, static_cast<int>(bits.raw));
}

// ================================================== ARITHMETIC

template <typename T>
HWY_API Vec1<T> operator+(Vec1<T> a, Vec1<T> b) {
  const uint64_t a64 = static_cast<uint64_t>(a.raw);
  const uint64_t b64 = static_cast<uint64_t>(b.raw);
  return Vec1<T>(static_cast<T>((a64 + b64) & static_cast<uint64_t>(~T(0))));
}
HWY_API Vec1<float> operator+(const Vec1<float> a, const Vec1<float> b) {
  return Vec1<float>(a.raw + b.raw);
}
HWY_API Vec1<double> operator+(const Vec1<double> a, const Vec1<double> b) {
  return Vec1<double>(a.raw + b.raw);
}

template <typename T>
HWY_API Vec1<T> operator-(Vec1<T> a, Vec1<T> b) {
  const uint64_t a64 = static_cast<uint64_t>(a.raw);
  const uint64_t b64 = static_cast<uint64_t>(b.raw);
  return Vec1<T>(static_cast<T>((a64 - b64) & static_cast<uint64_t>(~T(0))));
}
HWY_API Vec1<float> operator-(const Vec1<float> a, const Vec1<float> b) {
  return Vec1<float>(a.raw - b.raw);
}
HWY_API Vec1<double> operator-(const Vec1<double> a, const Vec1<double> b) {
  return Vec1<double>(a.raw - b.raw);
}

// ------------------------------ SumsOf8

HWY_API Vec1<uint64_t> SumsOf8(const Vec1<uint8_t> v) {
  return Vec1<uint64_t>(v.raw);
}

// ------------------------------ SaturatedAdd

// Returns a + b clamped to the destination range.

// Unsigned
HWY_API Vec1<uint8_t> SaturatedAdd(const Vec1<uint8_t> a,
                                   const Vec1<uint8_t> b) {
  return Vec1<uint8_t>(
      static_cast<uint8_t>(HWY_MIN(HWY_MAX(0, a.raw + b.raw), 255)));
}
HWY_API Vec1<uint16_t> SaturatedAdd(const Vec1<uint16_t> a,
                                    const Vec1<uint16_t> b) {
  return Vec1<uint16_t>(static_cast<uint16_t>(
      HWY_MIN(HWY_MAX(0, static_cast<int32_t>(a.raw) + b.raw), 65535)));
}

// Signed
HWY_API Vec1<int8_t> SaturatedAdd(const Vec1<int8_t> a, const Vec1<int8_t> b) {
  return Vec1<int8_t>(
      static_cast<int8_t>(HWY_MIN(HWY_MAX(-128, a.raw + b.raw), 127)));
}
HWY_API Vec1<int16_t> SaturatedAdd(const Vec1<int16_t> a,
                                   const Vec1<int16_t> b) {
  return Vec1<int16_t>(static_cast<int16_t>(
      HWY_MIN(HWY_MAX(-32768, static_cast<int32_t>(a.raw) + b.raw), 32767)));
}

// ------------------------------ Saturating subtraction

// Returns a - b clamped to the destination range.

// Unsigned
HWY_API Vec1<uint8_t> SaturatedSub(const Vec1<uint8_t> a,
                                   const Vec1<uint8_t> b) {
  return Vec1<uint8_t>(
      static_cast<uint8_t>(HWY_MIN(HWY_MAX(0, a.raw - b.raw), 255)));
}
HWY_API Vec1<uint16_t> SaturatedSub(const Vec1<uint16_t> a,
                                    const Vec1<uint16_t> b) {
  return Vec1<uint16_t>(static_cast<uint16_t>(
      HWY_MIN(HWY_MAX(0, static_cast<int32_t>(a.raw) - b.raw), 65535)));
}

// Signed
HWY_API Vec1<int8_t> SaturatedSub(const Vec1<int8_t> a, const Vec1<int8_t> b) {
  return Vec1<int8_t>(
      static_cast<int8_t>(HWY_MIN(HWY_MAX(-128, a.raw - b.raw), 127)));
}
HWY_API Vec1<int16_t> SaturatedSub(const Vec1<int16_t> a,
                                   const Vec1<int16_t> b) {
  return Vec1<int16_t>(static_cast<int16_t>(
      HWY_MIN(HWY_MAX(-32768, static_cast<int32_t>(a.raw) - b.raw), 32767)));
}

// ------------------------------ Average

// Returns (a + b + 1) / 2

HWY_API Vec1<uint8_t> AverageRound(const Vec1<uint8_t> a,
                                   const Vec1<uint8_t> b) {
  return Vec1<uint8_t>(static_cast<uint8_t>((a.raw + b.raw + 1) / 2));
}
HWY_API Vec1<uint16_t> AverageRound(const Vec1<uint16_t> a,
                                    const Vec1<uint16_t> b) {
  return Vec1<uint16_t>(static_cast<uint16_t>((a.raw + b.raw + 1) / 2));
}

// ------------------------------ Absolute value

template <typename T>
HWY_API Vec1<T> Abs(const Vec1<T> a) {
  const T i = a.raw;
  if (i >= 0 || i == hwy::LimitsMin<T>()) return a;
  return Vec1<T>(static_cast<T>(-i & T{-1}));
}
HWY_API Vec1<float> Abs(Vec1<float> a) {
  int32_t i;
  CopyBytes<sizeof(i)>(&a.raw, &i);
  i &= 0x7FFFFFFF;
  CopyBytes<sizeof(i)>(&i, &a.raw);
  return a;
}
HWY_API Vec1<double> Abs(Vec1<double> a) {
  int64_t i;
  CopyBytes<sizeof(i)>(&a.raw, &i);
  i &= 0x7FFFFFFFFFFFFFFFL;
  CopyBytes<sizeof(i)>(&i, &a.raw);
  return a;
}

// ------------------------------ Min/Max

// <cmath> may be unavailable, so implement our own.
namespace detail {

static inline float Abs(float f) {
  uint32_t i;
  CopyBytes<4>(&f, &i);
  i &= 0x7FFFFFFFu;
  CopyBytes<4>(&i, &f);
  return f;
}
static inline double Abs(double f) {
  uint64_t i;
  CopyBytes<8>(&f, &i);
  i &= 0x7FFFFFFFFFFFFFFFull;
  CopyBytes<8>(&i, &f);
  return f;
}

static inline bool SignBit(float f) {
  uint32_t i;
  CopyBytes<4>(&f, &i);
  return (i >> 31) != 0;
}
static inline bool SignBit(double f) {
  uint64_t i;
  CopyBytes<8>(&f, &i);
  return (i >> 63) != 0;
}

}  // namespace detail

template <typename T, HWY_IF_NOT_FLOAT(T)>
HWY_API Vec1<T> Min(const Vec1<T> a, const Vec1<T> b) {
  return Vec1<T>(HWY_MIN(a.raw, b.raw));
}

template <typename T, HWY_IF_FLOAT(T)>
HWY_API Vec1<T> Min(const Vec1<T> a, const Vec1<T> b) {
  if (isnan(a.raw)) return b;
  if (isnan(b.raw)) return a;
  return Vec1<T>(HWY_MIN(a.raw, b.raw));
}

template <typename T, HWY_IF_NOT_FLOAT(T)>
HWY_API Vec1<T> Max(const Vec1<T> a, const Vec1<T> b) {
  return Vec1<T>(HWY_MAX(a.raw, b.raw));
}

template <typename T, HWY_IF_FLOAT(T)>
HWY_API Vec1<T> Max(const Vec1<T> a, const Vec1<T> b) {
  if (isnan(a.raw)) return b;
  if (isnan(b.raw)) return a;
  return Vec1<T>(HWY_MAX(a.raw, b.raw));
}

// ------------------------------ Floating-point negate

template <typename T, HWY_IF_FLOAT(T)>
HWY_API Vec1<T> Neg(const Vec1<T> v) {
  return Xor(v, SignBit(Sisd<T>()));
}

template <typename T, HWY_IF_NOT_FLOAT(T)>
HWY_API Vec1<T> Neg(const Vec1<T> v) {
  return Zero(Sisd<T>()) - v;
}

// ------------------------------ mul/div

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

template <typename T, HWY_IF_FLOAT(T)>
HWY_API Vec1<T> operator*(const Vec1<T> a, const Vec1<T> b) {
  return Vec1<T>(static_cast<T>(double{a.raw} * b.raw));
}

template <typename T, HWY_IF_NOT_FLOAT(T)>
HWY_API Vec1<T> operator*(const Vec1<T> a, const Vec1<T> b) {
  return Vec1<T>(static_cast<T>(static_cast<uint64_t>(a.raw) *
                                static_cast<uint64_t>(b.raw)));
}

template <typename T>
HWY_API Vec1<T> operator/(const Vec1<T> a, const Vec1<T> b) {
  return Vec1<T>(a.raw / b.raw);
}

// Returns the upper 16 bits of a * b in each lane.
HWY_API Vec1<int16_t> MulHigh(const Vec1<int16_t> a, const Vec1<int16_t> b) {
  return Vec1<int16_t>(static_cast<int16_t>((a.raw * b.raw) >> 16));
}
HWY_API Vec1<uint16_t> MulHigh(const Vec1<uint16_t> a, const Vec1<uint16_t> b) {
  // Cast to uint32_t first to prevent overflow. Otherwise the result of
  // uint16_t * uint16_t is in "int" which may overflow. In practice the result
  // is the same but this way it is also defined.
  return Vec1<uint16_t>(static_cast<uint16_t>(
      (static_cast<uint32_t>(a.raw) * static_cast<uint32_t>(b.raw)) >> 16));
}

HWY_API Vec1<int16_t> MulFixedPoint15(Vec1<int16_t> a, Vec1<int16_t> b) {
  return Vec1<int16_t>(static_cast<int16_t>((2 * a.raw * b.raw + 32768) >> 16));
}

// Multiplies even lanes (0, 2 ..) and returns the double-wide result.
HWY_API Vec1<int64_t> MulEven(const Vec1<int32_t> a, const Vec1<int32_t> b) {
  const int64_t a64 = a.raw;
  return Vec1<int64_t>(a64 * b.raw);
}
HWY_API Vec1<uint64_t> MulEven(const Vec1<uint32_t> a, const Vec1<uint32_t> b) {
  const uint64_t a64 = a.raw;
  return Vec1<uint64_t>(a64 * b.raw);
}

// Approximate reciprocal
HWY_API Vec1<float> ApproximateReciprocal(const Vec1<float> v) {
  // Zero inputs are allowed, but callers are responsible for replacing the
  // return value with something else (typically using IfThenElse). This check
  // avoids a ubsan error. The return value is arbitrary.
  if (v.raw == 0.0f) return Vec1<float>(0.0f);
  return Vec1<float>(1.0f / v.raw);
}

// Absolute value of difference.
HWY_API Vec1<float> AbsDiff(const Vec1<float> a, const Vec1<float> b) {
  return Abs(a - b);
}

// ------------------------------ Floating-point multiply-add variants

template <typename T>
HWY_API Vec1<T> MulAdd(const Vec1<T> mul, const Vec1<T> x, const Vec1<T> add) {
  return mul * x + add;
}

template <typename T>
HWY_API Vec1<T> NegMulAdd(const Vec1<T> mul, const Vec1<T> x,
                          const Vec1<T> add) {
  return add - mul * x;
}

template <typename T>
HWY_API Vec1<T> MulSub(const Vec1<T> mul, const Vec1<T> x, const Vec1<T> sub) {
  return mul * x - sub;
}

template <typename T>
HWY_API Vec1<T> NegMulSub(const Vec1<T> mul, const Vec1<T> x,
                          const Vec1<T> sub) {
  return Neg(mul) * x - sub;
}

// ------------------------------ Floating-point square root

// Approximate reciprocal square root
HWY_API Vec1<float> ApproximateReciprocalSqrt(const Vec1<float> v) {
  float f = v.raw;
  const float half = f * 0.5f;
  uint32_t bits;
  CopySameSize(&f, &bits);
  // Initial guess based on log2(f)
  bits = 0x5F3759DF - (bits >> 1);
  CopySameSize(&bits, &f);
  // One Newton-Raphson iteration
  return Vec1<float>(f * (1.5f - (half * f * f)));
}

// Square root
HWY_API Vec1<float> Sqrt(Vec1<float> v) {
#if defined(HWY_NO_LIBCXX)
#if HWY_COMPILER_GCC_ACTUAL
  return Vec1<float>(__builtin_sqrt(v.raw));
#else
  uint32_t bits;
  CopyBytes<sizeof(bits)>(&v, &bits);
  // Coarse approximation, letting the exponent LSB leak into the mantissa
  bits = (1 << 29) + (bits >> 1) - (1 << 22);
  CopyBytes<sizeof(bits)>(&bits, &v);
  return v;
#endif  // !HWY_COMPILER_GCC_ACTUAL
#else
  return Vec1<float>(sqrtf(v.raw));
#endif  // !HWY_NO_LIBCXX
}
HWY_API Vec1<double> Sqrt(Vec1<double> v) {
#if defined(HWY_NO_LIBCXX)
#if HWY_COMPILER_GCC_ACTUAL
  return Vec1<double>(__builtin_sqrt(v.raw));
#else
  uint64_t bits;
  CopyBytes<sizeof(bits)>(&v, &bits);
  // Coarse approximation, letting the exponent LSB leak into the mantissa
  bits = (1ULL << 61) + (bits >> 1) - (1ULL << 51);
  CopyBytes<sizeof(bits)>(&bits, &v);
  return v;
#endif  // !HWY_COMPILER_GCC_ACTUAL
#else
  return Vec1<double>(sqrt(v.raw));
#endif  // HWY_NO_LIBCXX
}

// ------------------------------ Floating-point rounding

template <typename T>
HWY_API Vec1<T> Round(const Vec1<T> v) {
  using TI = MakeSigned<T>;
  if (!(Abs(v).raw < MantissaEnd<T>())) {  // Huge or NaN
    return v;
  }
  const T bias = v.raw < T(0.0) ? T(-0.5) : T(0.5);
  const TI rounded = static_cast<TI>(v.raw + bias);
  if (rounded == 0) return CopySignToAbs(Vec1<T>(0), v);
  // Round to even
  if ((rounded & 1) && detail::Abs(static_cast<T>(rounded) - v.raw) == T(0.5)) {
    return Vec1<T>(static_cast<T>(rounded - (v.raw < T(0) ? -1 : 1)));
  }
  return Vec1<T>(static_cast<T>(rounded));
}

// Round-to-nearest even.
HWY_API Vec1<int32_t> NearestInt(const Vec1<float> v) {
  using T = float;
  using TI = int32_t;

  const T abs = Abs(v).raw;
  const bool is_sign = detail::SignBit(v.raw);

  if (!(abs < MantissaEnd<T>())) {  // Huge or NaN
    // Check if too large to cast or NaN
    if (!(abs <= static_cast<T>(LimitsMax<TI>()))) {
      return Vec1<TI>(is_sign ? LimitsMin<TI>() : LimitsMax<TI>());
    }
    return Vec1<int32_t>(static_cast<TI>(v.raw));
  }
  const T bias = v.raw < T(0.0) ? T(-0.5) : T(0.5);
  const TI rounded = static_cast<TI>(v.raw + bias);
  if (rounded == 0) return Vec1<int32_t>(0);
  // Round to even
  if ((rounded & 1) && detail::Abs(static_cast<T>(rounded) - v.raw) == T(0.5)) {
    return Vec1<TI>(rounded - (is_sign ? -1 : 1));
  }
  return Vec1<TI>(rounded);
}

template <typename T>
HWY_API Vec1<T> Trunc(const Vec1<T> v) {
  using TI = MakeSigned<T>;
  if (!(Abs(v).raw <= MantissaEnd<T>())) {  // Huge or NaN
    return v;
  }
  const TI truncated = static_cast<TI>(v.raw);
  if (truncated == 0) return CopySignToAbs(Vec1<T>(0), v);
  return Vec1<T>(static_cast<T>(truncated));
}

template <typename Float, typename Bits, int kMantissaBits, int kExponentBits,
          class V>
V Ceiling(const V v) {
  const Bits kExponentMask = (1ull << kExponentBits) - 1;
  const Bits kMantissaMask = (1ull << kMantissaBits) - 1;
  const Bits kBias = kExponentMask / 2;

  Float f = v.raw;
  const bool positive = f > Float(0.0);

  Bits bits;
  CopySameSize(&v, &bits);

  const int exponent =
      static_cast<int>(((bits >> kMantissaBits) & kExponentMask) - kBias);
  // Already an integer.
  if (exponent >= kMantissaBits) return v;
  // |v| <= 1 => 0 or 1.
  if (exponent < 0) return positive ? V(1) : V(-0.0);

  const Bits mantissa_mask = kMantissaMask >> exponent;
  // Already an integer
  if ((bits & mantissa_mask) == 0) return v;

  // Clear fractional bits and round up
  if (positive) bits += (kMantissaMask + 1) >> exponent;
  bits &= ~mantissa_mask;

  CopySameSize(&bits, &f);
  return V(f);
}

template <typename Float, typename Bits, int kMantissaBits, int kExponentBits,
          class V>
V Floor(const V v) {
  const Bits kExponentMask = (1ull << kExponentBits) - 1;
  const Bits kMantissaMask = (1ull << kMantissaBits) - 1;
  const Bits kBias = kExponentMask / 2;

  Float f = v.raw;
  const bool negative = f < Float(0.0);

  Bits bits;
  CopySameSize(&v, &bits);

  const int exponent =
      static_cast<int>(((bits >> kMantissaBits) & kExponentMask) - kBias);
  // Already an integer.
  if (exponent >= kMantissaBits) return v;
  // |v| <= 1 => -1 or 0.
  if (exponent < 0) return V(negative ? Float(-1.0) : Float(0.0));

  const Bits mantissa_mask = kMantissaMask >> exponent;
  // Already an integer
  if ((bits & mantissa_mask) == 0) return v;

  // Clear fractional bits and round down
  if (negative) bits += (kMantissaMask + 1) >> exponent;
  bits &= ~mantissa_mask;

  CopySameSize(&bits, &f);
  return V(f);
}

// Toward +infinity, aka ceiling
HWY_API Vec1<float> Ceil(const Vec1<float> v) {
  return Ceiling<float, uint32_t, 23, 8>(v);
}
HWY_API Vec1<double> Ceil(const Vec1<double> v) {
  return Ceiling<double, uint64_t, 52, 11>(v);
}

// Toward -infinity, aka floor
HWY_API Vec1<float> Floor(const Vec1<float> v) {
  return Floor<float, uint32_t, 23, 8>(v);
}
HWY_API Vec1<double> Floor(const Vec1<double> v) {
  return Floor<double, uint64_t, 52, 11>(v);
}

// ================================================== COMPARE

template <typename T>
HWY_API Mask1<T> operator==(const Vec1<T> a, const Vec1<T> b) {
  return Mask1<T>::FromBool(a.raw == b.raw);
}

template <typename T>
HWY_API Mask1<T> operator!=(const Vec1<T> a, const Vec1<T> b) {
  return Mask1<T>::FromBool(a.raw != b.raw);
}

template <typename T>
HWY_API Mask1<T> TestBit(const Vec1<T> v, const Vec1<T> bit) {
  static_assert(!hwy::IsFloat<T>(), "Only integer vectors supported");
  return (v & bit) == bit;
}

template <typename T>
HWY_API Mask1<T> operator<(const Vec1<T> a, const Vec1<T> b) {
  return Mask1<T>::FromBool(a.raw < b.raw);
}
template <typename T>
HWY_API Mask1<T> operator>(const Vec1<T> a, const Vec1<T> b) {
  return Mask1<T>::FromBool(a.raw > b.raw);
}

template <typename T>
HWY_API Mask1<T> operator<=(const Vec1<T> a, const Vec1<T> b) {
  return Mask1<T>::FromBool(a.raw <= b.raw);
}
template <typename T>
HWY_API Mask1<T> operator>=(const Vec1<T> a, const Vec1<T> b) {
  return Mask1<T>::FromBool(a.raw >= b.raw);
}

// ------------------------------ Floating-point classification (==)

template <typename T>
HWY_API Mask1<T> IsNaN(const Vec1<T> v) {
  // std::isnan returns false for 0x7F..FF in clang AVX3 builds, so DIY.
  MakeUnsigned<T> bits;
  CopySameSize(&v, &bits);
  bits += bits;
  bits >>= 1;  // clear sign bit
  // NaN if all exponent bits are set and the mantissa is not zero.
  return Mask1<T>::FromBool(bits > ExponentMask<T>());
}

HWY_API Mask1<float> IsInf(const Vec1<float> v) {
  const Sisd<float> d;
  const RebindToUnsigned<decltype(d)> du;
  const Vec1<uint32_t> vu = BitCast(du, v);
  // 'Shift left' to clear the sign bit, check for exponent=max and mantissa=0.
  return RebindMask(d, (vu + vu) == Set(du, 0xFF000000u));
}
HWY_API Mask1<double> IsInf(const Vec1<double> v) {
  const Sisd<double> d;
  const RebindToUnsigned<decltype(d)> du;
  const Vec1<uint64_t> vu = BitCast(du, v);
  // 'Shift left' to clear the sign bit, check for exponent=max and mantissa=0.
  return RebindMask(d, (vu + vu) == Set(du, 0xFFE0000000000000ull));
}

HWY_API Mask1<float> IsFinite(const Vec1<float> v) {
  const Vec1<uint32_t> vu = BitCast(Sisd<uint32_t>(), v);
  // Shift left to clear the sign bit, check whether exponent != max value.
  return Mask1<float>::FromBool((vu.raw << 1) < 0xFF000000u);
}
HWY_API Mask1<double> IsFinite(const Vec1<double> v) {
  const Vec1<uint64_t> vu = BitCast(Sisd<uint64_t>(), v);
  // Shift left to clear the sign bit, check whether exponent != max value.
  return Mask1<double>::FromBool((vu.raw << 1) < 0xFFE0000000000000ull);
}

// ================================================== MEMORY

// ------------------------------ Load

template <class D, HWY_IF_LANES_D(D, 1), typename T = TFromD<D>>
HWY_API Vec1<T> Load(D /* tag */, const T* HWY_RESTRICT aligned) {
  T t;
  CopySameSize(aligned, &t);
  return Vec1<T>(t);
}

template <class D, typename T = TFromD<D>>
HWY_API Vec1<T> MaskedLoad(Mask1<T> m, D d, const T* HWY_RESTRICT aligned) {
  return IfThenElseZero(m, Load(d, aligned));
}

template <class D, typename T = TFromD<D>>
HWY_API Vec1<T> MaskedLoadOr(Vec1<T> v, Mask1<T> m, D d,
                             const T* HWY_RESTRICT aligned) {
  return IfThenElse(m, Load(d, aligned), v);
}

template <class D, HWY_IF_LANES_D(D, 1), typename T = TFromD<D>>
HWY_API Vec1<T> LoadU(D d, const T* HWY_RESTRICT p) {
  return Load(d, p);
}

// In some use cases, "load single lane" is sufficient; otherwise avoid this.
template <class D, HWY_IF_LANES_D(D, 1), typename T = TFromD<D>>
HWY_API Vec1<T> LoadDup128(D d, const T* HWY_RESTRICT aligned) {
  return Load(d, aligned);
}

// ------------------------------ Store

template <class D, typename T = TFromD<D>>
HWY_API void Store(const Vec1<T> v, D /* tag */, T* HWY_RESTRICT aligned) {
  CopySameSize(&v.raw, aligned);
}

template <class D, typename T = TFromD<D>>
HWY_API void StoreU(const Vec1<T> v, D d, T* HWY_RESTRICT p) {
  return Store(v, d, p);
}

template <class D, typename T = TFromD<D>>
HWY_API void BlendedStore(const Vec1<T> v, Mask1<T> m, D d, T* HWY_RESTRICT p) {
  if (!m.bits) return;
  StoreU(v, d, p);
}

// ------------------------------ LoadInterleaved2/3/4

// Per-target flag to prevent generic_ops-inl.h from defining StoreInterleaved2.
#ifdef HWY_NATIVE_LOAD_STORE_INTERLEAVED
#undef HWY_NATIVE_LOAD_STORE_INTERLEAVED
#else
#define HWY_NATIVE_LOAD_STORE_INTERLEAVED
#endif

template <class D, typename T = TFromD<D>>
HWY_API void LoadInterleaved2(D d, const T* HWY_RESTRICT unaligned, Vec1<T>& v0,
                              Vec1<T>& v1) {
  v0 = LoadU(d, unaligned + 0);
  v1 = LoadU(d, unaligned + 1);
}

template <class D, typename T = TFromD<D>>
HWY_API void LoadInterleaved3(D d, const T* HWY_RESTRICT unaligned, Vec1<T>& v0,
                              Vec1<T>& v1, Vec1<T>& v2) {
  v0 = LoadU(d, unaligned + 0);
  v1 = LoadU(d, unaligned + 1);
  v2 = LoadU(d, unaligned + 2);
}

template <class D, typename T = TFromD<D>>
HWY_API void LoadInterleaved4(D d, const T* HWY_RESTRICT unaligned, Vec1<T>& v0,
                              Vec1<T>& v1, Vec1<T>& v2, Vec1<T>& v3) {
  v0 = LoadU(d, unaligned + 0);
  v1 = LoadU(d, unaligned + 1);
  v2 = LoadU(d, unaligned + 2);
  v3 = LoadU(d, unaligned + 3);
}

// ------------------------------ StoreInterleaved2/3/4

template <class D, typename T = TFromD<D>>
HWY_API void StoreInterleaved2(const Vec1<T> v0, const Vec1<T> v1, D d,
                               T* HWY_RESTRICT unaligned) {
  StoreU(v0, d, unaligned + 0);
  StoreU(v1, d, unaligned + 1);
}

template <class D, typename T = TFromD<D>>
HWY_API void StoreInterleaved3(const Vec1<T> v0, const Vec1<T> v1,
                               const Vec1<T> v2, D d,
                               T* HWY_RESTRICT unaligned) {
  StoreU(v0, d, unaligned + 0);
  StoreU(v1, d, unaligned + 1);
  StoreU(v2, d, unaligned + 2);
}

template <class D, typename T = TFromD<D>>
HWY_API void StoreInterleaved4(const Vec1<T> v0, const Vec1<T> v1,
                               const Vec1<T> v2, const Vec1<T> v3, D d,
                               T* HWY_RESTRICT unaligned) {
  StoreU(v0, d, unaligned + 0);
  StoreU(v1, d, unaligned + 1);
  StoreU(v2, d, unaligned + 2);
  StoreU(v3, d, unaligned + 3);
}

// ------------------------------ Stream

template <class D, typename T = TFromD<D>>
HWY_API void Stream(const Vec1<T> v, D d, T* HWY_RESTRICT aligned) {
  return Store(v, d, aligned);
}

// ------------------------------ Scatter

template <class D, typename T = TFromD<D>, typename TI>
HWY_API void ScatterOffset(Vec1<T> v, D d, T* base, Vec1<TI> offset) {
  static_assert(sizeof(T) == sizeof(TI), "Index/lane size must match");
  uint8_t* const base8 = reinterpret_cast<uint8_t*>(base) + offset.raw;
  return Store(v, d, reinterpret_cast<T*>(base8));
}

template <class D, typename T = TFromD<D>, typename TI>
HWY_API void ScatterIndex(Vec1<T> v, D d, T* HWY_RESTRICT base,
                          Vec1<TI> index) {
  static_assert(sizeof(T) == sizeof(TI), "Index/lane size must match");
  return Store(v, d, base + index.raw);
}

// ------------------------------ Gather

template <class D, typename T = TFromD<D>, typename TI>
HWY_API Vec1<T> GatherOffset(D d, const T* base, Vec1<TI> offset) {
  static_assert(sizeof(T) == sizeof(TI), "Index/lane size must match");
  const intptr_t addr =
      reinterpret_cast<intptr_t>(base) + static_cast<intptr_t>(offset.raw);
  return Load(d, reinterpret_cast<const T*>(addr));
}

template <class D, typename T = TFromD<D>, typename TI>
HWY_API Vec1<T> GatherIndex(D d, const T* HWY_RESTRICT base, Vec1<TI> index) {
  static_assert(sizeof(T) == sizeof(TI), "Index/lane size must match");
  return Load(d, base + index.raw);
}

// ================================================== CONVERT

// ConvertTo and DemoteTo with floating-point input and integer output truncate
// (rounding toward zero).

template <class DTo, typename TTo = TFromD<DTo>, typename TFrom>
HWY_API Vec1<TTo> PromoteTo(DTo /* tag */, Vec1<TFrom> from) {
  static_assert(sizeof(TTo) > sizeof(TFrom), "Not promoting");
  // For bits Y > X, floatX->floatY and intX->intY are always representable.
  return Vec1<TTo>(static_cast<TTo>(from.raw));
}

// MSVC 19.10 cannot deduce the argument type if HWY_IF_FLOAT(TFrom) is here,
// so we overload for TFrom=double and TTo={float,int32_t}.
template <class D, HWY_IF_F32_D(D)>
HWY_API Vec1<float> DemoteTo(D /* tag */, Vec1<double> from) {
  // Prevent ubsan errors when converting float to narrower integer/float
  if (IsInf(from).bits ||
      Abs(from).raw > static_cast<double>(HighestValue<float>())) {
    return Vec1<float>(detail::SignBit(from.raw) ? LowestValue<float>()
                                                 : HighestValue<float>());
  }
  return Vec1<float>(static_cast<float>(from.raw));
}
template <class D, HWY_IF_I32_D(D)>
HWY_API Vec1<int32_t> DemoteTo(D /* tag */, Vec1<double> from) {
  // Prevent ubsan errors when converting int32_t to narrower integer/int32_t
  if (IsInf(from).bits ||
      Abs(from).raw > static_cast<double>(HighestValue<int32_t>())) {
    return Vec1<int32_t>(detail::SignBit(from.raw) ? LowestValue<int32_t>()
                                                   : HighestValue<int32_t>());
  }
  return Vec1<int32_t>(static_cast<int32_t>(from.raw));
}

template <class DTo, typename TTo = TFromD<DTo>, typename TFrom,
          HWY_IF_SIGNED(TFrom), HWY_IF_NOT_FLOAT_NOR_SPECIAL(TFromD<DTo>)>
HWY_API Vec1<TTo> DemoteTo(DTo /* tag */, Vec1<TFrom> from) {
  static_assert(!IsFloat<TFrom>(), "TFrom=double are handled above");
  static_assert(sizeof(TTo) < sizeof(TFrom), "Not demoting");

  // Int to int: choose closest value in TTo to `from` (avoids UB)
  from.raw = HWY_MIN(HWY_MAX(LimitsMin<TTo>(), from.raw), LimitsMax<TTo>());
  return Vec1<TTo>(static_cast<TTo>(from.raw));
}

template <class DTo, typename TTo = TFromD<DTo>, typename TFrom,
          HWY_IF_UNSIGNED(TFrom), HWY_IF_UNSIGNED_D(DTo)>
HWY_API Vec1<TTo> DemoteTo(DTo /* tag */, Vec1<TFrom> from) {
  static_assert(!IsFloat<TFrom>(), "TFrom=double are handled above");
  static_assert(sizeof(TTo) < sizeof(TFrom), "Not demoting");

  // Int to int: choose closest value in TTo to `from` (avoids UB)
  from.raw = HWY_MIN(from.raw, LimitsMax<TTo>());
  return Vec1<TTo>(static_cast<TTo>(from.raw));
}

template <class D, HWY_IF_F32_D(D)>
HWY_API Vec1<float> PromoteTo(D /* tag */, const Vec1<float16_t> v) {
  uint16_t bits16;
  CopySameSize(&v.raw, &bits16);
  const uint32_t sign = static_cast<uint32_t>(bits16 >> 15);
  const uint32_t biased_exp = (bits16 >> 10) & 0x1F;
  const uint32_t mantissa = bits16 & 0x3FF;

  // Subnormal or zero
  if (biased_exp == 0) {
    const float subnormal =
        (1.0f / 16384) * (static_cast<float>(mantissa) * (1.0f / 1024));
    return Vec1<float>(sign ? -subnormal : subnormal);
  }

  // Normalized: convert the representation directly (faster than ldexp/tables).
  const uint32_t biased_exp32 = biased_exp + (127 - 15);
  const uint32_t mantissa32 = mantissa << (23 - 10);
  const uint32_t bits32 = (sign << 31) | (biased_exp32 << 23) | mantissa32;
  float out;
  CopySameSize(&bits32, &out);
  return Vec1<float>(out);
}

template <class D, HWY_IF_F32_D(D)>
HWY_API Vec1<float> PromoteTo(D d, const Vec1<bfloat16_t> v) {
  return Set(d, F32FromBF16(v.raw));
}

template <class D, HWY_IF_F16_D(D)>
HWY_API Vec1<float16_t> DemoteTo(D /* tag */, const Vec1<float> v) {
  uint32_t bits32;
  CopySameSize(&v.raw, &bits32);
  const uint32_t sign = bits32 >> 31;
  const uint32_t biased_exp32 = (bits32 >> 23) & 0xFF;
  const uint32_t mantissa32 = bits32 & 0x7FFFFF;

  const int32_t exp = HWY_MIN(static_cast<int32_t>(biased_exp32) - 127, 15);

  // Tiny or zero => zero.
  Vec1<float16_t> out;
  if (exp < -24) {
    const uint16_t zero = 0;
    CopySameSize(&zero, &out.raw);
    return out;
  }

  uint32_t biased_exp16, mantissa16;

  // exp = [-24, -15] => subnormal
  if (exp < -14) {
    biased_exp16 = 0;
    const uint32_t sub_exp = static_cast<uint32_t>(-14 - exp);
    HWY_DASSERT(1 <= sub_exp && sub_exp < 11);
    mantissa16 = static_cast<uint32_t>((1u << (10 - sub_exp)) +
                                       (mantissa32 >> (13 + sub_exp)));
  } else {
    // exp = [-14, 15]
    biased_exp16 = static_cast<uint32_t>(exp + 15);
    HWY_DASSERT(1 <= biased_exp16 && biased_exp16 < 31);
    mantissa16 = mantissa32 >> 13;
  }

  HWY_DASSERT(mantissa16 < 1024);
  const uint32_t bits16 = (sign << 15) | (biased_exp16 << 10) | mantissa16;
  HWY_DASSERT(bits16 < 0x10000);
  const uint16_t narrowed = static_cast<uint16_t>(bits16);  // big-endian safe
  CopySameSize(&narrowed, &out.raw);
  return out;
}

template <class D, HWY_IF_BF16_D(D)>
HWY_API Vec1<bfloat16_t> DemoteTo(D d, const Vec1<float> v) {
  return Set(d, BF16FromF32(v.raw));
}

template <class DTo, typename TTo = TFromD<DTo>, typename TFrom,
          HWY_IF_FLOAT(TFrom)>
HWY_API Vec1<TTo> ConvertTo(DTo /* tag */, Vec1<TFrom> from) {
  static_assert(sizeof(TTo) == sizeof(TFrom), "Should have same size");
  // float## -> int##: return closest representable value. We cannot exactly
  // represent LimitsMax<TTo> in TFrom, so use double.
  const double f = static_cast<double>(from.raw);
  if (IsInf(from).bits ||
      Abs(Vec1<double>(f)).raw > static_cast<double>(LimitsMax<TTo>())) {
    return Vec1<TTo>(detail::SignBit(from.raw) ? LimitsMin<TTo>()
                                               : LimitsMax<TTo>());
  }
  return Vec1<TTo>(static_cast<TTo>(from.raw));
}

template <class DTo, typename TTo = TFromD<DTo>, typename TFrom,
          HWY_IF_NOT_FLOAT(TFrom)>
HWY_API Vec1<TTo> ConvertTo(DTo /* tag */, Vec1<TFrom> from) {
  static_assert(sizeof(TTo) == sizeof(TFrom), "Should have same size");
  // int## -> float##: no check needed
  return Vec1<TTo>(static_cast<TTo>(from.raw));
}

HWY_API Vec1<uint8_t> U8FromU32(const Vec1<uint32_t> v) {
  return DemoteTo(Sisd<uint8_t>(), v);
}

// ------------------------------ TruncateTo

template <class D, HWY_IF_U8_D(D)>
HWY_API Vec1<uint8_t> TruncateTo(D /* tag */, Vec1<uint64_t> v) {
  return Vec1<uint8_t>{static_cast<uint8_t>(v.raw & 0xFF)};
}

template <class D, HWY_IF_U16_D(D)>
HWY_API Vec1<uint16_t> TruncateTo(D /* tag */, Vec1<uint64_t> v) {
  return Vec1<uint16_t>{static_cast<uint16_t>(v.raw & 0xFFFF)};
}

template <class D, HWY_IF_U32_D(D)>
HWY_API Vec1<uint32_t> TruncateTo(D /* tag */, Vec1<uint64_t> v) {
  return Vec1<uint32_t>{static_cast<uint32_t>(v.raw & 0xFFFFFFFFu)};
}

template <class D, HWY_IF_U8_D(D)>
HWY_API Vec1<uint8_t> TruncateTo(D /* tag */, Vec1<uint32_t> v) {
  return Vec1<uint8_t>{static_cast<uint8_t>(v.raw & 0xFF)};
}

template <class D, HWY_IF_U16_D(D)>
HWY_API Vec1<uint16_t> TruncateTo(D /* tag */, Vec1<uint32_t> v) {
  return Vec1<uint16_t>{static_cast<uint16_t>(v.raw & 0xFFFF)};
}

template <class D, HWY_IF_U8_D(D)>
HWY_API Vec1<uint8_t> TruncateTo(D /* tag */, Vec1<uint16_t> v) {
  return Vec1<uint8_t>{static_cast<uint8_t>(v.raw & 0xFF)};
}

// ================================================== COMBINE
// UpperHalf, ZeroExtendVector, Combine, Concat* are unsupported.

template <typename T>
HWY_API Vec1<T> LowerHalf(Vec1<T> v) {
  return v;
}

template <class D, typename T = TFromD<D>>
HWY_API Vec1<T> LowerHalf(D /* tag */, Vec1<T> v) {
  return v;
}

// ================================================== SWIZZLE

template <typename T>
HWY_API T GetLane(const Vec1<T> v) {
  return v.raw;
}

template <typename T>
HWY_API T ExtractLane(const Vec1<T> v, size_t i) {
  HWY_DASSERT(i == 0);
  (void)i;
  return v.raw;
}

template <typename T>
HWY_API Vec1<T> InsertLane(Vec1<T> v, size_t i, T t) {
  HWY_DASSERT(i == 0);
  (void)i;
  v.raw = t;
  return v;
}

template <typename T>
HWY_API Vec1<T> DupEven(Vec1<T> v) {
  return v;
}
// DupOdd is unsupported.

template <typename T>
HWY_API Vec1<T> OddEven(Vec1<T> /* odd */, Vec1<T> even) {
  return even;
}

template <typename T>
HWY_API Vec1<T> OddEvenBlocks(Vec1<T> /* odd */, Vec1<T> even) {
  return even;
}

// ------------------------------ SwapAdjacentBlocks

template <typename T>
HWY_API Vec1<T> SwapAdjacentBlocks(Vec1<T> v) {
  return v;
}

// ------------------------------ TableLookupLanes

// Returned by SetTableIndices for use by TableLookupLanes.
template <typename T>
struct Indices1 {
  MakeSigned<T> raw;
};

template <class D, typename T = TFromD<D>, typename TI>
HWY_API Indices1<T> IndicesFromVec(D, Vec1<TI> vec) {
  static_assert(sizeof(T) == sizeof(TI), "Index size must match lane size");
  HWY_DASSERT(vec.raw <= 1);
  return Indices1<T>{static_cast<MakeSigned<T>>(vec.raw)};
}

template <class D, HWY_IF_LANES_D(D, 1), typename T = TFromD<D>, typename TI>
HWY_API Indices1<T> SetTableIndices(D d, const TI* idx) {
  return IndicesFromVec(d, LoadU(Sisd<TI>(), idx));
}

template <typename T>
HWY_API Vec1<T> TableLookupLanes(const Vec1<T> v, const Indices1<T> /* idx */) {
  return v;
}

template <typename T>
HWY_API Vec1<T> TwoTablesLookupLanes(const Vec1<T> a, const Vec1<T> b,
                                     const Indices1<T> idx) {
  return (idx.raw == 0) ? a : b;
}

// ------------------------------ ReverseBlocks

// Single block: no change
template <class D, typename T = TFromD<D>>
HWY_API Vec1<T> ReverseBlocks(D /* tag */, const Vec1<T> v) {
  return v;
}

// ------------------------------ Reverse

template <class D, typename T = TFromD<D>>
HWY_API Vec1<T> Reverse(D /* tag */, const Vec1<T> v) {
  return v;
}

// Per-target flag to prevent generic_ops-inl.h defining 8-bit Reverse2/4/8.
#ifdef HWY_NATIVE_REVERSE2_8
#undef HWY_NATIVE_REVERSE2_8
#else
#define HWY_NATIVE_REVERSE2_8
#endif

// Must not be called:
template <class D, typename T = TFromD<D>>
HWY_API Vec1<T> Reverse2(D /* tag */, const Vec1<T> v) {
  return v;
}

template <class D, typename T = TFromD<D>>
HWY_API Vec1<T> Reverse4(D /* tag */, const Vec1<T> v) {
  return v;
}

template <class D, typename T = TFromD<D>>
HWY_API Vec1<T> Reverse8(D /* tag */, const Vec1<T> v) {
  return v;
}

// ------------------------------ ReverseLaneBytes

#ifdef HWY_NATIVE_REVERSE_LANE_BYTES
#undef HWY_NATIVE_REVERSE_LANE_BYTES
#else
#define HWY_NATIVE_REVERSE_LANE_BYTES
#endif

HWY_API Vec1<uint16_t> ReverseLaneBytes(Vec1<uint16_t> v) {
  const uint32_t val{v.raw};
  return Vec1<uint16_t>(
      static_cast<uint16_t>(((val << 8) & 0xFF00u) | ((val >> 8) & 0x00FFu)));
}

HWY_API Vec1<uint32_t> ReverseLaneBytes(Vec1<uint32_t> v) {
  const uint32_t val = v.raw;
  return Vec1<uint32_t>(static_cast<uint32_t>(
      ((val << 24) & 0xFF000000u) | ((val << 8) & 0x00FF0000u) |
      ((val >> 8) & 0x0000FF00u) | ((val >> 24) & 0x000000FFu)));
}

HWY_API Vec1<uint64_t> ReverseLaneBytes(Vec1<uint64_t> v) {
  const uint64_t val = v.raw;
  return Vec1<uint64_t>(static_cast<uint64_t>(
      ((val << 56) & 0xFF00000000000000u) |
      ((val << 40) & 0x00FF000000000000u) |
      ((val << 24) & 0x0000FF0000000000u) | ((val << 8) & 0x000000FF00000000u) |
      ((val >> 8) & 0x00000000FF000000u) | ((val >> 24) & 0x0000000000FF0000u) |
      ((val >> 40) & 0x000000000000FF00u) |
      ((val >> 56) & 0x00000000000000FFu)));
}

template <class V, HWY_IF_SIGNED_V(V),
          HWY_IF_T_SIZE_ONE_OF_V(V, (1 << 2) | (1 << 4) | (1 << 8))>
HWY_API V ReverseLaneBytes(V v) {
  const DFromV<decltype(v)> d;
  const RebindToUnsigned<decltype(d)> du;
  return BitCast(d, ReverseLaneBytes(BitCast(du, v)));
}

// ------------------------------ ReverseBits
#ifdef HWY_NATIVE_REVERSE_BITS_UI8
#undef HWY_NATIVE_REVERSE_BITS_UI8
#else
#define HWY_NATIVE_REVERSE_BITS_UI8
#endif

#ifdef HWY_NATIVE_REVERSE_BITS_UI16_32_64
#undef HWY_NATIVE_REVERSE_BITS_UI16_32_64
#else
#define HWY_NATIVE_REVERSE_BITS_UI16_32_64
#endif

namespace detail {

template <class T>
HWY_INLINE T ReverseBitsOfEachByte(T val) {
  using TU = MakeUnsigned<T>;
  constexpr TU kMaxUnsignedVal{LimitsMax<TU>()};
  constexpr TU kShrMask1 =
      static_cast<TU>(0x5555555555555555u & kMaxUnsignedVal);
  constexpr TU kShrMask2 =
      static_cast<TU>(0x3333333333333333u & kMaxUnsignedVal);
  constexpr TU kShrMask3 =
      static_cast<TU>(0x0F0F0F0F0F0F0F0Fu & kMaxUnsignedVal);

  constexpr TU kShlMask1 = static_cast<TU>(~kShrMask1);
  constexpr TU kShlMask2 = static_cast<TU>(~kShrMask2);
  constexpr TU kShlMask3 = static_cast<TU>(~kShrMask3);

  TU result = static_cast<TU>(val);
  result = static_cast<TU>(((result << 1) & kShlMask1) |
                           ((result >> 1) & kShrMask1));
  result = static_cast<TU>(((result << 2) & kShlMask2) |
                           ((result >> 2) & kShrMask2));
  result = static_cast<TU>(((result << 4) & kShlMask3) |
                           ((result >> 4) & kShrMask3));
  return static_cast<T>(result);
}

}  // namespace detail

template <class V, HWY_IF_UNSIGNED_V(V), HWY_IF_T_SIZE_V(V, 1)>
HWY_API V ReverseBits(V v) {
  return V(detail::ReverseBitsOfEachByte(v.raw));
}

template <class V, HWY_IF_UNSIGNED_V(V),
          HWY_IF_T_SIZE_ONE_OF_V(V, (1 << 2) | (1 << 4) | (1 << 8))>
HWY_API V ReverseBits(V v) {
  return ReverseLaneBytes(V(detail::ReverseBitsOfEachByte(v.raw)));
}

template <class V, HWY_IF_SIGNED_V(V)>
HWY_API V ReverseBits(V v) {
  const DFromV<decltype(v)> d;
  const RebindToUnsigned<decltype(d)> du;
  return BitCast(d, ReverseBits(BitCast(du, v)));
}

// ================================================== BLOCKWISE
// Shift*Bytes, CombineShiftRightBytes, Interleave*, Shuffle* are unsupported.

// ------------------------------ Broadcast/splat any lane

template <int kLane, typename T>
HWY_API Vec1<T> Broadcast(const Vec1<T> v) {
  static_assert(kLane == 0, "Scalar only has one lane");
  return v;
}

// ------------------------------ TableLookupBytes, TableLookupBytesOr0

template <typename T, typename TI>
HWY_API Vec1<TI> TableLookupBytes(const Vec1<T> in, const Vec1<TI> indices) {
  uint8_t in_bytes[sizeof(T)];
  uint8_t idx_bytes[sizeof(T)];
  uint8_t out_bytes[sizeof(T)];
  CopyBytes<sizeof(T)>(&in, &in_bytes);  // copy to bytes
  CopyBytes<sizeof(T)>(&indices, &idx_bytes);
  for (size_t i = 0; i < sizeof(T); ++i) {
    out_bytes[i] = in_bytes[idx_bytes[i]];
  }
  TI out;
  CopyBytes<sizeof(TI)>(&out_bytes, &out);
  return Vec1<TI>{out};
}

template <typename T, typename TI>
HWY_API Vec1<TI> TableLookupBytesOr0(const Vec1<T> in, const Vec1<TI> indices) {
  uint8_t in_bytes[sizeof(T)];
  uint8_t idx_bytes[sizeof(T)];
  uint8_t out_bytes[sizeof(T)];
  CopyBytes<sizeof(T)>(&in, &in_bytes);  // copy to bytes
  CopyBytes<sizeof(T)>(&indices, &idx_bytes);
  for (size_t i = 0; i < sizeof(T); ++i) {
    out_bytes[i] = idx_bytes[i] & 0x80 ? 0 : in_bytes[idx_bytes[i]];
  }
  TI out;
  CopyBytes<sizeof(TI)>(&out_bytes, &out);
  return Vec1<TI>{out};
}

// ------------------------------ ZipLower

HWY_API Vec1<uint16_t> ZipLower(Vec1<uint8_t> a, Vec1<uint8_t> b) {
  return Vec1<uint16_t>(static_cast<uint16_t>((uint32_t{b.raw} << 8) + a.raw));
}
HWY_API Vec1<uint32_t> ZipLower(Vec1<uint16_t> a, Vec1<uint16_t> b) {
  return Vec1<uint32_t>((uint32_t{b.raw} << 16) + a.raw);
}
HWY_API Vec1<uint64_t> ZipLower(Vec1<uint32_t> a, Vec1<uint32_t> b) {
  return Vec1<uint64_t>((uint64_t{b.raw} << 32) + a.raw);
}
HWY_API Vec1<int16_t> ZipLower(Vec1<int8_t> a, Vec1<int8_t> b) {
  return Vec1<int16_t>(static_cast<int16_t>((int32_t{b.raw} << 8) + a.raw));
}
HWY_API Vec1<int32_t> ZipLower(Vec1<int16_t> a, Vec1<int16_t> b) {
  return Vec1<int32_t>((int32_t{b.raw} << 16) + a.raw);
}
HWY_API Vec1<int64_t> ZipLower(Vec1<int32_t> a, Vec1<int32_t> b) {
  return Vec1<int64_t>((int64_t{b.raw} << 32) + a.raw);
}

template <class DW, typename TW = TFromD<DW>, typename TN = MakeNarrow<TW>>
HWY_API Vec1<TW> ZipLower(DW /* tag */, Vec1<TN> a, Vec1<TN> b) {
  return Vec1<TW>(static_cast<TW>((TW{b.raw} << (sizeof(TN) * 8)) + a.raw));
}

// ================================================== MASK

template <class D, typename T = TFromD<D>>
HWY_API bool AllFalse(D /* tag */, const Mask1<T> mask) {
  return mask.bits == 0;
}

template <class D, typename T = TFromD<D>>
HWY_API bool AllTrue(D /* tag */, const Mask1<T> mask) {
  return mask.bits != 0;
}

// `p` points to at least 8 readable bytes, not all of which need be valid.
template <class D, HWY_IF_LANES_D(D, 1), typename T = TFromD<D>>
HWY_API Mask1<T> LoadMaskBits(D /* tag */, const uint8_t* HWY_RESTRICT bits) {
  return Mask1<T>::FromBool((bits[0] & 1) != 0);
}

// `p` points to at least 8 writable bytes.
template <class D, typename T = TFromD<D>>
HWY_API size_t StoreMaskBits(D d, const Mask1<T> mask, uint8_t* bits) {
  *bits = AllTrue(d, mask);
  return 1;
}

template <class D, typename T = TFromD<D>>
HWY_API size_t CountTrue(D /* tag */, const Mask1<T> mask) {
  return mask.bits == 0 ? 0 : 1;
}

template <class D, typename T = TFromD<D>>
HWY_API intptr_t FindFirstTrue(D /* tag */, const Mask1<T> mask) {
  return mask.bits == 0 ? -1 : 0;
}

template <class D, typename T = TFromD<D>>
HWY_API size_t FindKnownFirstTrue(D /* tag */, const Mask1<T> /* m */) {
  return 0;  // There is only one lane and we know it is true.
}

template <class D, typename T = TFromD<D>>
HWY_API intptr_t FindLastTrue(D /* tag */, const Mask1<T> mask) {
  return mask.bits == 0 ? -1 : 0;
}

template <class D, typename T = TFromD<D>>
HWY_API size_t FindKnownLastTrue(D /* tag */, const Mask1<T> /* m */) {
  return 0;  // There is only one lane and we know it is true.
}

// ------------------------------ Compress, CompressBits

template <typename T>
struct CompressIsPartition {
  enum { value = 1 };
};

template <typename T>
HWY_API Vec1<T> Compress(Vec1<T> v, const Mask1<T> /* mask */) {
  // A single lane is already partitioned by definition.
  return v;
}

template <typename T>
HWY_API Vec1<T> CompressNot(Vec1<T> v, const Mask1<T> /* mask */) {
  // A single lane is already partitioned by definition.
  return v;
}

// ------------------------------ CompressStore
template <class D, typename T = TFromD<D>>
HWY_API size_t CompressStore(Vec1<T> v, const Mask1<T> mask, D d,
                             T* HWY_RESTRICT unaligned) {
  StoreU(Compress(v, mask), d, unaligned);
  return CountTrue(d, mask);
}

// ------------------------------ CompressBlendedStore
template <class D, typename T = TFromD<D>>
HWY_API size_t CompressBlendedStore(Vec1<T> v, const Mask1<T> mask, D d,
                                    T* HWY_RESTRICT unaligned) {
  if (!mask.bits) return 0;
  StoreU(v, d, unaligned);
  return 1;
}

// ------------------------------ CompressBits
template <typename T>
HWY_API Vec1<T> CompressBits(Vec1<T> v, const uint8_t* HWY_RESTRICT /*bits*/) {
  return v;
}

// ------------------------------ CompressBitsStore
template <class D, typename T = TFromD<D>>
HWY_API size_t CompressBitsStore(Vec1<T> v, const uint8_t* HWY_RESTRICT bits,
                                 D d, T* HWY_RESTRICT unaligned) {
  const Mask1<T> mask = LoadMaskBits(d, bits);
  StoreU(Compress(v, mask), d, unaligned);
  return CountTrue(d, mask);
}

// ------------------------------ Expand

// generic_ops-inl.h requires Vec64/128, so implement [Load]Expand here.
#ifdef HWY_NATIVE_EXPAND
#undef HWY_NATIVE_EXPAND
#else
#define HWY_NATIVE_EXPAND
#endif

template <typename T>
HWY_API Vec1<T> Expand(Vec1<T> v, const Mask1<T> mask) {
  return IfThenElseZero(mask, v);
}

// ------------------------------ LoadExpand
template <class D>
HWY_API VFromD<D> LoadExpand(MFromD<D> mask, D d,
                             const TFromD<D>* HWY_RESTRICT unaligned) {
  return MaskedLoad(mask, d, unaligned);
}

// ------------------------------ ReorderWidenMulAccumulate (MulAdd, ZipLower)

template <class D32, HWY_IF_F32_D(D32)>
HWY_API Vec1<float> ReorderWidenMulAccumulate(D32 /* tag */, Vec1<bfloat16_t> a,
                                              Vec1<bfloat16_t> b,
                                              const Vec1<float> sum0,
                                              Vec1<float>& /* sum1 */) {
  return MulAdd(Vec1<float>(F32FromBF16(a.raw)),
                Vec1<float>(F32FromBF16(b.raw)), sum0);
}

template <class D32, HWY_IF_I32_D(D32)>
HWY_API Vec1<int32_t> ReorderWidenMulAccumulate(D32 /* tag */, Vec1<int16_t> a,
                                                Vec1<int16_t> b,
                                                const Vec1<int32_t> sum0,
                                                Vec1<int32_t>& /* sum1 */) {
  return Vec1<int32_t>(a.raw * b.raw + sum0.raw);
}

// ------------------------------ RearrangeToOddPlusEven
template <typename TW>
HWY_API Vec1<TW> RearrangeToOddPlusEven(Vec1<TW> sum0, Vec1<TW> /* sum1 */) {
  return sum0;  // invariant already holds
}

// ================================================== REDUCTIONS

// Sum of all lanes, i.e. the only one.
template <class D, typename T = TFromD<D>>
HWY_API Vec1<T> SumOfLanes(D /* tag */, const Vec1<T> v) {
  return v;
}
template <class D, typename T = TFromD<D>>
HWY_API T ReduceSum(D /* tag */, const Vec1<T> v) {
  return GetLane(v);
}
template <class D, typename T = TFromD<D>>
HWY_API Vec1<T> MinOfLanes(D /* tag */, const Vec1<T> v) {
  return v;
}
template <class D, typename T = TFromD<D>>
HWY_API Vec1<T> MaxOfLanes(D /* tag */, const Vec1<T> v) {
  return v;
}

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace hwy
HWY_AFTER_NAMESPACE();
