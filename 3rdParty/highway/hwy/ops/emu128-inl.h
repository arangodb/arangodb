// Copyright 2022 Google LLC
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

#include <cmath>  // std::abs, std::isnan

#include "hwy/ops/shared-inl.h"

HWY_BEFORE_NAMESPACE();
namespace hwy {
namespace HWY_NAMESPACE {

template <typename T>
using Full128 = Simd<T, 16 / sizeof(T), 0>;

// (Wrapper class required for overloading comparison operators.)
template <typename T, size_t N = 16 / sizeof(T)>
struct Vec128 {
  using PrivateT = T;                     // only for DFromV
  static constexpr size_t kPrivateN = N;  // only for DFromV

  HWY_INLINE Vec128() = default;
  Vec128(const Vec128&) = default;
  Vec128& operator=(const Vec128&) = default;

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

  // Behave like wasm128 (vectors can always hold 128 bits). generic_ops-inl.h
  // relies on this for LoadInterleaved*. CAVEAT: this method of padding
  // prevents using range for, especially in SumOfLanes, where it would be
  // incorrect. Moving padding to another field would require handling the case
  // where N = 16 / sizeof(T) (i.e. there is no padding), which is also awkward.
  T raw[16 / sizeof(T)] = {};
};

// 0 or FF..FF, same size as Vec128.
template <typename T, size_t N = 16 / sizeof(T)>
struct Mask128 {
  using Raw = hwy::MakeUnsigned<T>;
  static HWY_INLINE Raw FromBool(bool b) {
    return b ? static_cast<Raw>(~Raw{0}) : 0;
  }

  // Must match the size of Vec128.
  Raw bits[16 / sizeof(T)] = {};
};

template <class V>
using DFromV = Simd<typename V::PrivateT, V::kPrivateN, 0>;

template <class V>
using TFromV = typename V::PrivateT;

// ------------------------------ Zero

// Use HWY_MAX_LANES_D here because VFromD is defined in terms of Zero.
template <class D, HWY_IF_V_SIZE_LE_D(D, 16)>
HWY_API Vec128<TFromD<D>, HWY_MAX_LANES_D(D)> Zero(D /* tag */) {
  Vec128<TFromD<D>, HWY_MAX_LANES_D(D)> v;  // zero-initialized
  return v;
}

template <class D>
using VFromD = decltype(Zero(D()));

// ------------------------------ Tuple (VFromD)
#include "hwy/ops/tuple-inl.h"

// ------------------------------ BitCast

template <class D, class VFrom>
HWY_API VFromD<D> BitCast(D /* tag */, VFrom v) {
  VFromD<D> to;
  CopySameSize(&v, &to);
  return to;
}

// ------------------------------ ResizeBitCast

template <class D, class VFrom>
HWY_API VFromD<D> ResizeBitCast(D d, VFrom v) {
  using DFrom = DFromV<VFrom>;
  using TFrom = TFromD<DFrom>;
  using TTo = TFromD<D>;

  constexpr size_t kFromByteLen = sizeof(TFrom) * HWY_MAX_LANES_D(DFrom);
  constexpr size_t kToByteLen = sizeof(TTo) * HWY_MAX_LANES_D(D);
  constexpr size_t kCopyByteLen = HWY_MIN(kFromByteLen, kToByteLen);

  VFromD<D> to = Zero(d);
  CopyBytes<kCopyByteLen>(&v, &to);
  return to;
}

namespace detail {

// ResizeBitCast on the HWY_EMU128 target has zero-extending semantics if
// VFromD<DTo> is a larger vector than FromV
template <class FromSizeTag, class ToSizeTag, class DTo, class DFrom>
HWY_INLINE VFromD<DTo> ZeroExtendResizeBitCast(FromSizeTag /* from_size_tag */,
                                               ToSizeTag /* to_size_tag */,
                                               DTo d_to, DFrom /* d_from */,
                                               VFromD<DFrom> v) {
  return ResizeBitCast(d_to, v);
}

}  // namespace detail

// ------------------------------ Set
template <class D, typename T2>
HWY_API VFromD<D> Set(D d, const T2 t) {
  VFromD<D> v;
  for (size_t i = 0; i < MaxLanes(d); ++i) {
    v.raw[i] = static_cast<TFromD<D>>(t);
  }
  return v;
}

// ------------------------------ Undefined
template <class D>
HWY_API VFromD<D> Undefined(D d) {
  return Zero(d);
}

// ------------------------------ Iota

template <class D, typename T = TFromD<D>, typename T2>
HWY_API VFromD<D> Iota(D d, T2 first) {
  VFromD<D> v;
  for (size_t i = 0; i < MaxLanes(d); ++i) {
    v.raw[i] =
        AddWithWraparound(hwy::IsFloatTag<T>(), static_cast<T>(first), i);
  }
  return v;
}

// ================================================== LOGICAL

// ------------------------------ Not
template <typename T, size_t N>
HWY_API Vec128<T, N> Not(Vec128<T, N> v) {
  const DFromV<decltype(v)> d;
  const RebindToUnsigned<decltype(d)> du;
  using TU = TFromD<decltype(du)>;
  VFromD<decltype(du)> vu = BitCast(du, v);
  for (size_t i = 0; i < N; ++i) {
    vu.raw[i] = static_cast<TU>(~vu.raw[i]);
  }
  return BitCast(d, vu);
}

// ------------------------------ And
template <typename T, size_t N>
HWY_API Vec128<T, N> And(Vec128<T, N> a, Vec128<T, N> b) {
  const DFromV<decltype(a)> d;
  const RebindToUnsigned<decltype(d)> du;
  auto au = BitCast(du, a);
  auto bu = BitCast(du, b);
  for (size_t i = 0; i < N; ++i) {
    au.raw[i] &= bu.raw[i];
  }
  return BitCast(d, au);
}
template <typename T, size_t N>
HWY_API Vec128<T, N> operator&(Vec128<T, N> a, Vec128<T, N> b) {
  return And(a, b);
}

// ------------------------------ AndNot
template <typename T, size_t N>
HWY_API Vec128<T, N> AndNot(Vec128<T, N> a, Vec128<T, N> b) {
  return And(Not(a), b);
}

// ------------------------------ Or
template <typename T, size_t N>
HWY_API Vec128<T, N> Or(Vec128<T, N> a, Vec128<T, N> b) {
  const DFromV<decltype(a)> d;
  const RebindToUnsigned<decltype(d)> du;
  auto au = BitCast(du, a);
  auto bu = BitCast(du, b);
  for (size_t i = 0; i < N; ++i) {
    au.raw[i] |= bu.raw[i];
  }
  return BitCast(d, au);
}
template <typename T, size_t N>
HWY_API Vec128<T, N> operator|(Vec128<T, N> a, Vec128<T, N> b) {
  return Or(a, b);
}

// ------------------------------ Xor
template <typename T, size_t N>
HWY_API Vec128<T, N> Xor(Vec128<T, N> a, Vec128<T, N> b) {
  const DFromV<decltype(a)> d;
  const RebindToUnsigned<decltype(d)> du;
  auto au = BitCast(du, a);
  auto bu = BitCast(du, b);
  for (size_t i = 0; i < N; ++i) {
    au.raw[i] ^= bu.raw[i];
  }
  return BitCast(d, au);
}
template <typename T, size_t N>
HWY_API Vec128<T, N> operator^(Vec128<T, N> a, Vec128<T, N> b) {
  return Xor(a, b);
}

// ------------------------------ Xor3
template <typename T, size_t N>
HWY_API Vec128<T, N> Xor3(Vec128<T, N> x1, Vec128<T, N> x2, Vec128<T, N> x3) {
  return Xor(x1, Xor(x2, x3));
}

// ------------------------------ Or3
template <typename T, size_t N>
HWY_API Vec128<T, N> Or3(Vec128<T, N> o1, Vec128<T, N> o2, Vec128<T, N> o3) {
  return Or(o1, Or(o2, o3));
}

// ------------------------------ OrAnd
template <typename T, size_t N>
HWY_API Vec128<T, N> OrAnd(Vec128<T, N> o, Vec128<T, N> a1, Vec128<T, N> a2) {
  return Or(o, And(a1, a2));
}

// ------------------------------ IfVecThenElse
template <typename T, size_t N>
HWY_API Vec128<T, N> IfVecThenElse(Vec128<T, N> mask, Vec128<T, N> yes,
                                   Vec128<T, N> no) {
  return Or(And(mask, yes), AndNot(mask, no));
}

// ------------------------------ CopySign
template <typename T, size_t N>
HWY_API Vec128<T, N> CopySign(Vec128<T, N> magn, Vec128<T, N> sign) {
  static_assert(IsFloat<T>(), "Only makes sense for floating-point");
  const DFromV<decltype(magn)> d;
  const auto msb = SignBit(d);
  return Or(AndNot(msb, magn), And(msb, sign));
}

template <typename T, size_t N>
HWY_API Vec128<T, N> CopySignToAbs(Vec128<T, N> abs, Vec128<T, N> sign) {
  static_assert(IsFloat<T>(), "Only makes sense for floating-point");
  const DFromV<decltype(abs)> d;
  return Or(abs, And(SignBit(d), sign));
}

// ------------------------------ BroadcastSignBit
template <typename T, size_t N>
HWY_API Vec128<T, N> BroadcastSignBit(Vec128<T, N> v) {
  // This is used inside ShiftRight, so we cannot implement in terms of it.
  for (size_t i = 0; i < N; ++i) {
    v.raw[i] = v.raw[i] < 0 ? T(-1) : T(0);
  }
  return v;
}

// ------------------------------ Mask

// v must be 0 or FF..FF.
template <typename T, size_t N>
HWY_API Mask128<T, N> MaskFromVec(Vec128<T, N> v) {
  Mask128<T, N> mask;
  CopySameSize(&v, &mask);
  return mask;
}

template <class D>
using MFromD = decltype(MaskFromVec(VFromD<D>()));

template <class DTo, class MFrom>
HWY_API MFromD<DTo> RebindMask(DTo /* tag */, MFrom mask) {
  MFromD<DTo> to;
  CopySameSize(&mask, &to);
  return to;
}

template <typename T, size_t N>
Vec128<T, N> VecFromMask(Mask128<T, N> mask) {
  Vec128<T, N> v;
  CopySameSize(&mask, &v);
  return v;
}

template <class D>
VFromD<D> VecFromMask(D /* tag */, MFromD<D> mask) {
  return VecFromMask(mask);
}

template <class D>
HWY_API MFromD<D> FirstN(D d, size_t n) {
  MFromD<D> m;
  for (size_t i = 0; i < MaxLanes(d); ++i) {
    m.bits[i] = MFromD<D>::FromBool(i < n);
  }
  return m;
}

// Returns mask ? yes : no.
template <typename T, size_t N>
HWY_API Vec128<T, N> IfThenElse(Mask128<T, N> mask, Vec128<T, N> yes,
                                Vec128<T, N> no) {
  return IfVecThenElse(VecFromMask(mask), yes, no);
}

template <typename T, size_t N>
HWY_API Vec128<T, N> IfThenElseZero(Mask128<T, N> mask, Vec128<T, N> yes) {
  const DFromV<decltype(yes)> d;
  return IfVecThenElse(VecFromMask(mask), yes, Zero(d));
}

template <typename T, size_t N>
HWY_API Vec128<T, N> IfThenZeroElse(Mask128<T, N> mask, Vec128<T, N> no) {
  const DFromV<decltype(no)> d;
  return IfVecThenElse(VecFromMask(mask), Zero(d), no);
}

template <typename T, size_t N>
HWY_API Vec128<T, N> IfNegativeThenElse(Vec128<T, N> v, Vec128<T, N> yes,
                                        Vec128<T, N> no) {
  for (size_t i = 0; i < N; ++i) {
    v.raw[i] = v.raw[i] < 0 ? yes.raw[i] : no.raw[i];
  }
  return v;
}

template <typename T, size_t N>
HWY_API Vec128<T, N> ZeroIfNegative(Vec128<T, N> v) {
  const DFromV<decltype(v)> d;
  return IfNegativeThenElse(v, Zero(d), v);
}

// ------------------------------ Mask logical

template <typename T, size_t N>
HWY_API Mask128<T, N> Not(Mask128<T, N> m) {
  return MaskFromVec(Not(VecFromMask(Simd<T, N, 0>(), m)));
}

template <typename T, size_t N>
HWY_API Mask128<T, N> And(Mask128<T, N> a, Mask128<T, N> b) {
  const Simd<T, N, 0> d;
  return MaskFromVec(And(VecFromMask(d, a), VecFromMask(d, b)));
}

template <typename T, size_t N>
HWY_API Mask128<T, N> AndNot(Mask128<T, N> a, Mask128<T, N> b) {
  const Simd<T, N, 0> d;
  return MaskFromVec(AndNot(VecFromMask(d, a), VecFromMask(d, b)));
}

template <typename T, size_t N>
HWY_API Mask128<T, N> Or(Mask128<T, N> a, Mask128<T, N> b) {
  const Simd<T, N, 0> d;
  return MaskFromVec(Or(VecFromMask(d, a), VecFromMask(d, b)));
}

template <typename T, size_t N>
HWY_API Mask128<T, N> Xor(Mask128<T, N> a, Mask128<T, N> b) {
  const Simd<T, N, 0> d;
  return MaskFromVec(Xor(VecFromMask(d, a), VecFromMask(d, b)));
}

template <typename T, size_t N>
HWY_API Mask128<T, N> ExclusiveNeither(Mask128<T, N> a, Mask128<T, N> b) {
  const Simd<T, N, 0> d;
  return MaskFromVec(AndNot(VecFromMask(d, a), Not(VecFromMask(d, b))));
}

// ================================================== SHIFTS

// ------------------------------ ShiftLeft/ShiftRight (BroadcastSignBit)

template <int kBits, typename T, size_t N>
HWY_API Vec128<T, N> ShiftLeft(Vec128<T, N> v) {
  static_assert(0 <= kBits && kBits < sizeof(T) * 8, "Invalid shift");
  for (size_t i = 0; i < N; ++i) {
    const auto shifted = static_cast<hwy::MakeUnsigned<T>>(v.raw[i]) << kBits;
    v.raw[i] = static_cast<T>(shifted);
  }
  return v;
}

template <int kBits, typename T, size_t N>
HWY_API Vec128<T, N> ShiftRight(Vec128<T, N> v) {
  static_assert(0 <= kBits && kBits < sizeof(T) * 8, "Invalid shift");
#if __cplusplus >= 202002L
  // Signed right shift is now guaranteed to be arithmetic (rounding toward
  // negative infinity, i.e. shifting in the sign bit).
  for (size_t i = 0; i < N; ++i) {
    v.raw[i] = static_cast<T>(v.raw[i] >> kBits);
  }
#else
  if (IsSigned<T>()) {
    // Emulate arithmetic shift using only logical (unsigned) shifts, because
    // signed shifts are still implementation-defined.
    using TU = hwy::MakeUnsigned<T>;
    for (size_t i = 0; i < N; ++i) {
      const TU shifted = static_cast<TU>(static_cast<TU>(v.raw[i]) >> kBits);
      const TU sign = v.raw[i] < 0 ? static_cast<TU>(~TU{0}) : 0;
      const size_t sign_shift =
          static_cast<size_t>(static_cast<int>(sizeof(TU)) * 8 - 1 - kBits);
      const TU upper = static_cast<TU>(sign << sign_shift);
      v.raw[i] = static_cast<T>(shifted | upper);
    }
  } else {  // T is unsigned
    for (size_t i = 0; i < N; ++i) {
      v.raw[i] = static_cast<T>(v.raw[i] >> kBits);
    }
  }
#endif
  return v;
}

// ------------------------------ RotateRight (ShiftRight)
template <int kBits, typename T, size_t N>
HWY_API Vec128<T, N> RotateRight(const Vec128<T, N> v) {
  constexpr size_t kSizeInBits = sizeof(T) * 8;
  static_assert(0 <= kBits && kBits < kSizeInBits, "Invalid shift count");
  if (kBits == 0) return v;
  return Or(ShiftRight<kBits>(v),
            ShiftLeft<HWY_MIN(kSizeInBits - 1, kSizeInBits - kBits)>(v));
}

// ------------------------------ ShiftLeftSame

template <typename T, size_t N>
HWY_API Vec128<T, N> ShiftLeftSame(Vec128<T, N> v, int bits) {
  for (size_t i = 0; i < N; ++i) {
    const auto shifted = static_cast<hwy::MakeUnsigned<T>>(v.raw[i]) << bits;
    v.raw[i] = static_cast<T>(shifted);
  }
  return v;
}

template <typename T, size_t N>
HWY_API Vec128<T, N> ShiftRightSame(Vec128<T, N> v, int bits) {
#if __cplusplus >= 202002L
  // Signed right shift is now guaranteed to be arithmetic (rounding toward
  // negative infinity, i.e. shifting in the sign bit).
  for (size_t i = 0; i < N; ++i) {
    v.raw[i] = static_cast<T>(v.raw[i] >> bits);
  }
#else
  if (IsSigned<T>()) {
    // Emulate arithmetic shift using only logical (unsigned) shifts, because
    // signed shifts are still implementation-defined.
    using TU = hwy::MakeUnsigned<T>;
    for (size_t i = 0; i < N; ++i) {
      const TU shifted = static_cast<TU>(static_cast<TU>(v.raw[i]) >> bits);
      const TU sign = v.raw[i] < 0 ? static_cast<TU>(~TU{0}) : 0;
      const size_t sign_shift =
          static_cast<size_t>(static_cast<int>(sizeof(TU)) * 8 - 1 - bits);
      const TU upper = static_cast<TU>(sign << sign_shift);
      v.raw[i] = static_cast<T>(shifted | upper);
    }
  } else {
    for (size_t i = 0; i < N; ++i) {
      v.raw[i] = static_cast<T>(v.raw[i] >> bits);  // unsigned, logical shift
    }
  }
#endif
  return v;
}

// ------------------------------ Shl

template <typename T, size_t N>
HWY_API Vec128<T, N> operator<<(Vec128<T, N> v, Vec128<T, N> bits) {
  for (size_t i = 0; i < N; ++i) {
    const auto shifted = static_cast<hwy::MakeUnsigned<T>>(v.raw[i])
                         << bits.raw[i];
    v.raw[i] = static_cast<T>(shifted);
  }
  return v;
}

template <typename T, size_t N>
HWY_API Vec128<T, N> operator>>(Vec128<T, N> v, Vec128<T, N> bits) {
#if __cplusplus >= 202002L
  // Signed right shift is now guaranteed to be arithmetic (rounding toward
  // negative infinity, i.e. shifting in the sign bit).
  for (size_t i = 0; i < N; ++i) {
    v.raw[i] = static_cast<T>(v.raw[i] >> bits.raw[i]);
  }
#else
  if (IsSigned<T>()) {
    // Emulate arithmetic shift using only logical (unsigned) shifts, because
    // signed shifts are still implementation-defined.
    using TU = hwy::MakeUnsigned<T>;
    for (size_t i = 0; i < N; ++i) {
      const TU shifted =
          static_cast<TU>(static_cast<TU>(v.raw[i]) >> bits.raw[i]);
      const TU sign = v.raw[i] < 0 ? static_cast<TU>(~TU{0}) : 0;
      const size_t sign_shift = static_cast<size_t>(
          static_cast<int>(sizeof(TU)) * 8 - 1 - bits.raw[i]);
      const TU upper = static_cast<TU>(sign << sign_shift);
      v.raw[i] = static_cast<T>(shifted | upper);
    }
  } else {  // T is unsigned
    for (size_t i = 0; i < N; ++i) {
      v.raw[i] = static_cast<T>(v.raw[i] >> bits.raw[i]);
    }
  }
#endif
  return v;
}

// ================================================== ARITHMETIC

// Tag dispatch instead of SFINAE for MSVC 2017 compatibility
namespace detail {

template <typename T, size_t N>
HWY_INLINE Vec128<T, N> Add(hwy::NonFloatTag /*tag*/, Vec128<T, N> a,
                            Vec128<T, N> b) {
  for (size_t i = 0; i < N; ++i) {
    const uint64_t a64 = static_cast<uint64_t>(a.raw[i]);
    const uint64_t b64 = static_cast<uint64_t>(b.raw[i]);
    a.raw[i] = static_cast<T>((a64 + b64) & static_cast<uint64_t>(~T(0)));
  }
  return a;
}
template <typename T, size_t N>
HWY_INLINE Vec128<T, N> Sub(hwy::NonFloatTag /*tag*/, Vec128<T, N> a,
                            Vec128<T, N> b) {
  for (size_t i = 0; i < N; ++i) {
    const uint64_t a64 = static_cast<uint64_t>(a.raw[i]);
    const uint64_t b64 = static_cast<uint64_t>(b.raw[i]);
    a.raw[i] = static_cast<T>((a64 - b64) & static_cast<uint64_t>(~T(0)));
  }
  return a;
}

template <typename T, size_t N>
HWY_INLINE Vec128<T, N> Add(hwy::FloatTag /*tag*/, Vec128<T, N> a,
                            Vec128<T, N> b) {
  for (size_t i = 0; i < N; ++i) {
    a.raw[i] += b.raw[i];
  }
  return a;
}

template <typename T, size_t N>
HWY_INLINE Vec128<T, N> Sub(hwy::FloatTag /*tag*/, Vec128<T, N> a,
                            Vec128<T, N> b) {
  for (size_t i = 0; i < N; ++i) {
    a.raw[i] -= b.raw[i];
  }
  return a;
}

}  // namespace detail

template <typename T, size_t N>
HWY_API Vec128<T, N> operator-(Vec128<T, N> a, Vec128<T, N> b) {
  return detail::Sub(hwy::IsFloatTag<T>(), a, b);
}
template <typename T, size_t N>
HWY_API Vec128<T, N> operator+(Vec128<T, N> a, Vec128<T, N> b) {
  return detail::Add(hwy::IsFloatTag<T>(), a, b);
}

// ------------------------------ SumsOf8

template <size_t N>
HWY_API Vec128<uint64_t, (N + 7) / 8> SumsOf8(Vec128<uint8_t, N> v) {
  Vec128<uint64_t, (N + 7) / 8> sums;
  for (size_t i = 0; i < N; ++i) {
    sums.raw[i / 8] += v.raw[i];
  }
  return sums;
}

// ------------------------------ SaturatedAdd
template <typename T, size_t N, HWY_IF_T_SIZE_ONE_OF(T, (1 << 1) | (1 << 2)),
          HWY_IF_NOT_FLOAT_NOR_SPECIAL(T)>
HWY_API Vec128<T, N> SaturatedAdd(Vec128<T, N> a, Vec128<T, N> b) {
  using TW = MakeSigned<MakeWide<T>>;
  for (size_t i = 0; i < N; ++i) {
    a.raw[i] = static_cast<T>(HWY_MIN(
        HWY_MAX(hwy::LowestValue<T>(), static_cast<TW>(a.raw[i]) + b.raw[i]),
        hwy::HighestValue<T>()));
  }
  return a;
}

// ------------------------------ SaturatedSub
template <typename T, size_t N, HWY_IF_T_SIZE_ONE_OF(T, (1 << 1) | (1 << 2)),
          HWY_IF_NOT_FLOAT_NOR_SPECIAL(T)>
HWY_API Vec128<T, N> SaturatedSub(Vec128<T, N> a, Vec128<T, N> b) {
  using TW = MakeSigned<MakeWide<T>>;
  for (size_t i = 0; i < N; ++i) {
    a.raw[i] = static_cast<T>(HWY_MIN(
        HWY_MAX(hwy::LowestValue<T>(), static_cast<TW>(a.raw[i]) - b.raw[i]),
        hwy::HighestValue<T>()));
  }
  return a;
}

// ------------------------------ AverageRound
template <typename T, size_t N>
HWY_API Vec128<T, N> AverageRound(Vec128<T, N> a, Vec128<T, N> b) {
  static_assert(!IsSigned<T>(), "Only for unsigned");
  for (size_t i = 0; i < N; ++i) {
    a.raw[i] = static_cast<T>((a.raw[i] + b.raw[i] + 1) / 2);
  }
  return a;
}

// ------------------------------ Abs

// Tag dispatch instead of SFINAE for MSVC 2017 compatibility
namespace detail {

template <typename T, size_t N>
HWY_INLINE Vec128<T, N> Abs(SignedTag /*tag*/, Vec128<T, N> a) {
  for (size_t i = 0; i < N; ++i) {
    const T s = a.raw[i];
    const T min = hwy::LimitsMin<T>();
    a.raw[i] = static_cast<T>((s >= 0 || s == min) ? a.raw[i] : -s);
  }
  return a;
}

template <typename T, size_t N>
HWY_INLINE Vec128<T, N> Abs(hwy::FloatTag /*tag*/, Vec128<T, N> v) {
  for (size_t i = 0; i < N; ++i) {
    v.raw[i] = std::abs(v.raw[i]);
  }
  return v;
}

}  // namespace detail

template <typename T, size_t N>
HWY_API Vec128<T, N> Abs(Vec128<T, N> a) {
  return detail::Abs(hwy::TypeTag<T>(), a);
}

// ------------------------------ Min/Max

// Tag dispatch instead of SFINAE for MSVC 2017 compatibility
namespace detail {

template <typename T, size_t N>
HWY_INLINE Vec128<T, N> Min(hwy::NonFloatTag /*tag*/, Vec128<T, N> a,
                            Vec128<T, N> b) {
  for (size_t i = 0; i < N; ++i) {
    a.raw[i] = HWY_MIN(a.raw[i], b.raw[i]);
  }
  return a;
}
template <typename T, size_t N>
HWY_INLINE Vec128<T, N> Max(hwy::NonFloatTag /*tag*/, Vec128<T, N> a,
                            Vec128<T, N> b) {
  for (size_t i = 0; i < N; ++i) {
    a.raw[i] = HWY_MAX(a.raw[i], b.raw[i]);
  }
  return a;
}

template <typename T, size_t N>
HWY_INLINE Vec128<T, N> Min(hwy::FloatTag /*tag*/, Vec128<T, N> a,
                            Vec128<T, N> b) {
  for (size_t i = 0; i < N; ++i) {
    if (std::isnan(a.raw[i])) {
      a.raw[i] = b.raw[i];
    } else if (std::isnan(b.raw[i])) {
      // no change
    } else {
      a.raw[i] = HWY_MIN(a.raw[i], b.raw[i]);
    }
  }
  return a;
}
template <typename T, size_t N>
HWY_INLINE Vec128<T, N> Max(hwy::FloatTag /*tag*/, Vec128<T, N> a,
                            Vec128<T, N> b) {
  for (size_t i = 0; i < N; ++i) {
    if (std::isnan(a.raw[i])) {
      a.raw[i] = b.raw[i];
    } else if (std::isnan(b.raw[i])) {
      // no change
    } else {
      a.raw[i] = HWY_MAX(a.raw[i], b.raw[i]);
    }
  }
  return a;
}

}  // namespace detail

template <typename T, size_t N>
HWY_API Vec128<T, N> Min(Vec128<T, N> a, Vec128<T, N> b) {
  return detail::Min(hwy::IsFloatTag<T>(), a, b);
}

template <typename T, size_t N>
HWY_API Vec128<T, N> Max(Vec128<T, N> a, Vec128<T, N> b) {
  return detail::Max(hwy::IsFloatTag<T>(), a, b);
}

// ------------------------------ Neg

// Tag dispatch instead of SFINAE for MSVC 2017 compatibility
namespace detail {

template <typename T, size_t N>
HWY_API Vec128<T, N> Neg(hwy::NonFloatTag /*tag*/, Vec128<T, N> v) {
  const DFromV<decltype(v)> d;
  return Zero(d) - v;
}

template <typename T, size_t N>
HWY_API Vec128<T, N> Neg(hwy::FloatTag /*tag*/, Vec128<T, N> v) {
  const DFromV<decltype(v)> d;
  return Xor(v, SignBit(d));
}

}  // namespace detail

template <typename T, size_t N>
HWY_API Vec128<T, N> Neg(Vec128<T, N> v) {
  return detail::Neg(hwy::IsFloatTag<T>(), v);
}

// ------------------------------ Mul/Div

// Tag dispatch instead of SFINAE for MSVC 2017 compatibility
namespace detail {

template <typename T, size_t N>
HWY_INLINE Vec128<T, N> Mul(hwy::FloatTag /*tag*/, Vec128<T, N> a,
                            Vec128<T, N> b) {
  for (size_t i = 0; i < N; ++i) {
    a.raw[i] *= b.raw[i];
  }
  return a;
}

template <typename T, size_t N>
HWY_INLINE Vec128<T, N> Mul(SignedTag /*tag*/, Vec128<T, N> a, Vec128<T, N> b) {
  for (size_t i = 0; i < N; ++i) {
    a.raw[i] = static_cast<T>(static_cast<uint64_t>(a.raw[i]) *
                              static_cast<uint64_t>(b.raw[i]));
  }
  return a;
}

template <typename T, size_t N>
HWY_INLINE Vec128<T, N> Mul(UnsignedTag /*tag*/, Vec128<T, N> a,
                            Vec128<T, N> b) {
  for (size_t i = 0; i < N; ++i) {
    a.raw[i] = static_cast<T>(static_cast<uint64_t>(a.raw[i]) *
                              static_cast<uint64_t>(b.raw[i]));
  }
  return a;
}

}  // namespace detail

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

template <typename T, size_t N>
HWY_API Vec128<T, N> operator*(Vec128<T, N> a, Vec128<T, N> b) {
  return detail::Mul(hwy::TypeTag<T>(), a, b);
}

template <typename T, size_t N>
HWY_API Vec128<T, N> operator/(Vec128<T, N> a, Vec128<T, N> b) {
  for (size_t i = 0; i < N; ++i) {
    a.raw[i] = (b.raw[i] == T{0}) ? 0 : a.raw[i] / b.raw[i];
  }
  return a;
}

// Returns the upper 16 bits of a * b in each lane.
template <size_t N>
HWY_API Vec128<int16_t, N> MulHigh(Vec128<int16_t, N> a, Vec128<int16_t, N> b) {
  for (size_t i = 0; i < N; ++i) {
    a.raw[i] = static_cast<int16_t>((int32_t{a.raw[i]} * b.raw[i]) >> 16);
  }
  return a;
}
template <size_t N>
HWY_API Vec128<uint16_t, N> MulHigh(Vec128<uint16_t, N> a,
                                    Vec128<uint16_t, N> b) {
  for (size_t i = 0; i < N; ++i) {
    // Cast to uint32_t first to prevent overflow. Otherwise the result of
    // uint16_t * uint16_t is in "int" which may overflow. In practice the
    // result is the same but this way it is also defined.
    a.raw[i] = static_cast<uint16_t>(
        (static_cast<uint32_t>(a.raw[i]) * static_cast<uint32_t>(b.raw[i])) >>
        16);
  }
  return a;
}

template <size_t N>
HWY_API Vec128<int16_t, N> MulFixedPoint15(Vec128<int16_t, N> a,
                                           Vec128<int16_t, N> b) {
  for (size_t i = 0; i < N; ++i) {
    a.raw[i] = static_cast<int16_t>((2 * a.raw[i] * b.raw[i] + 32768) >> 16);
  }
  return a;
}

// Multiplies even lanes (0, 2 ..) and returns the double-wide result.
template <size_t N>
HWY_API Vec128<int64_t, (N + 1) / 2> MulEven(Vec128<int32_t, N> a,
                                             Vec128<int32_t, N> b) {
  Vec128<int64_t, (N + 1) / 2> mul;
  for (size_t i = 0; i < N; i += 2) {
    const int64_t a64 = a.raw[i];
    mul.raw[i / 2] = a64 * b.raw[i];
  }
  return mul;
}
template <size_t N>
HWY_API Vec128<uint64_t, (N + 1) / 2> MulEven(Vec128<uint32_t, N> a,
                                              Vec128<uint32_t, N> b) {
  Vec128<uint64_t, (N + 1) / 2> mul;
  for (size_t i = 0; i < N; i += 2) {
    const uint64_t a64 = a.raw[i];
    mul.raw[i / 2] = a64 * b.raw[i];
  }
  return mul;
}

template <size_t N>
HWY_API Vec128<int64_t, (N + 1) / 2> MulOdd(Vec128<int32_t, N> a,
                                            Vec128<int32_t, N> b) {
  Vec128<int64_t, (N + 1) / 2> mul;
  for (size_t i = 0; i < N; i += 2) {
    const int64_t a64 = a.raw[i + 1];
    mul.raw[i / 2] = a64 * b.raw[i + 1];
  }
  return mul;
}
template <size_t N>
HWY_API Vec128<uint64_t, (N + 1) / 2> MulOdd(Vec128<uint32_t, N> a,
                                             Vec128<uint32_t, N> b) {
  Vec128<uint64_t, (N + 1) / 2> mul;
  for (size_t i = 0; i < N; i += 2) {
    const uint64_t a64 = a.raw[i + 1];
    mul.raw[i / 2] = a64 * b.raw[i + 1];
  }
  return mul;
}

template <size_t N>
HWY_API Vec128<float, N> ApproximateReciprocal(Vec128<float, N> v) {
  for (size_t i = 0; i < N; ++i) {
    // Zero inputs are allowed, but callers are responsible for replacing the
    // return value with something else (typically using IfThenElse). This check
    // avoids a ubsan error. The result is arbitrary.
    v.raw[i] = (std::abs(v.raw[i]) == 0.0f) ? 0.0f : 1.0f / v.raw[i];
  }
  return v;
}

template <size_t N>
HWY_API Vec128<float, N> AbsDiff(Vec128<float, N> a, Vec128<float, N> b) {
  return Abs(a - b);
}

// ------------------------------ Floating-point multiply-add variants

template <typename T, size_t N>
HWY_API Vec128<T, N> MulAdd(Vec128<T, N> mul, Vec128<T, N> x,
                            Vec128<T, N> add) {
  return mul * x + add;
}

template <typename T, size_t N>
HWY_API Vec128<T, N> NegMulAdd(Vec128<T, N> mul, Vec128<T, N> x,
                               Vec128<T, N> add) {
  return add - mul * x;
}

template <typename T, size_t N>
HWY_API Vec128<T, N> MulSub(Vec128<T, N> mul, Vec128<T, N> x,
                            Vec128<T, N> sub) {
  return mul * x - sub;
}

template <typename T, size_t N>
HWY_API Vec128<T, N> NegMulSub(Vec128<T, N> mul, Vec128<T, N> x,
                               Vec128<T, N> sub) {
  return Neg(mul) * x - sub;
}

// ------------------------------ Floating-point square root

template <size_t N>
HWY_API Vec128<float, N> ApproximateReciprocalSqrt(Vec128<float, N> v) {
  for (size_t i = 0; i < N; ++i) {
    const float half = v.raw[i] * 0.5f;
    uint32_t bits;
    CopySameSize(&v.raw[i], &bits);
    // Initial guess based on log2(f)
    bits = 0x5F3759DF - (bits >> 1);
    CopySameSize(&bits, &v.raw[i]);
    // One Newton-Raphson iteration
    v.raw[i] = v.raw[i] * (1.5f - (half * v.raw[i] * v.raw[i]));
  }
  return v;
}

template <typename T, size_t N>
HWY_API Vec128<T, N> Sqrt(Vec128<T, N> v) {
  for (size_t i = 0; i < N; ++i) {
    v.raw[i] = std::sqrt(v.raw[i]);
  }
  return v;
}

// ------------------------------ Floating-point rounding

template <typename T, size_t N>
HWY_API Vec128<T, N> Round(Vec128<T, N> v) {
  using TI = MakeSigned<T>;
  const Vec128<T, N> a = Abs(v);
  for (size_t i = 0; i < N; ++i) {
    if (!(a.raw[i] < MantissaEnd<T>())) {  // Huge or NaN
      continue;
    }
    const T bias = v.raw[i] < T(0.0) ? T(-0.5) : T(0.5);
    const TI rounded = static_cast<TI>(v.raw[i] + bias);
    if (rounded == 0) {
      v.raw[i] = v.raw[i] < 0 ? T{-0} : T{0};
      continue;
    }
    const T rounded_f = static_cast<T>(rounded);
    // Round to even
    if ((rounded & 1) && std::abs(rounded_f - v.raw[i]) == T(0.5)) {
      v.raw[i] = static_cast<T>(rounded - (v.raw[i] < T(0) ? -1 : 1));
      continue;
    }
    v.raw[i] = rounded_f;
  }
  return v;
}

// Round-to-nearest even.
template <size_t N>
HWY_API Vec128<int32_t, N> NearestInt(Vec128<float, N> v) {
  using T = float;
  using TI = int32_t;

  const Vec128<float, N> abs = Abs(v);
  Vec128<int32_t, N> ret;
  for (size_t i = 0; i < N; ++i) {
    const bool signbit = std::signbit(v.raw[i]);

    if (!(abs.raw[i] < MantissaEnd<T>())) {  // Huge or NaN
      // Check if too large to cast or NaN
      if (!(abs.raw[i] <= static_cast<T>(LimitsMax<TI>()))) {
        ret.raw[i] = signbit ? LimitsMin<TI>() : LimitsMax<TI>();
        continue;
      }
      ret.raw[i] = static_cast<TI>(v.raw[i]);
      continue;
    }
    const T bias = v.raw[i] < T(0.0) ? T(-0.5) : T(0.5);
    const TI rounded = static_cast<TI>(v.raw[i] + bias);
    if (rounded == 0) {
      ret.raw[i] = 0;
      continue;
    }
    const T rounded_f = static_cast<T>(rounded);
    // Round to even
    if ((rounded & 1) && std::abs(rounded_f - v.raw[i]) == T(0.5)) {
      ret.raw[i] = rounded - (signbit ? -1 : 1);
      continue;
    }
    ret.raw[i] = rounded;
  }
  return ret;
}

template <typename T, size_t N>
HWY_API Vec128<T, N> Trunc(Vec128<T, N> v) {
  using TI = MakeSigned<T>;
  const Vec128<T, N> abs = Abs(v);
  for (size_t i = 0; i < N; ++i) {
    if (!(abs.raw[i] <= MantissaEnd<T>())) {  // Huge or NaN
      continue;
    }
    const TI truncated = static_cast<TI>(v.raw[i]);
    if (truncated == 0) {
      v.raw[i] = v.raw[i] < 0 ? -T{0} : T{0};
      continue;
    }
    v.raw[i] = static_cast<T>(truncated);
  }
  return v;
}

// Toward +infinity, aka ceiling
template <typename Float, size_t N>
Vec128<Float, N> Ceil(Vec128<Float, N> v) {
  constexpr int kMantissaBits = MantissaBits<Float>();
  using Bits = MakeUnsigned<Float>;
  const Bits kExponentMask = MaxExponentField<Float>();
  const Bits kMantissaMask = MantissaMask<Float>();
  const Bits kBias = kExponentMask / 2;

  for (size_t i = 0; i < N; ++i) {
    const bool positive = v.raw[i] > Float(0.0);

    Bits bits;
    CopySameSize(&v.raw[i], &bits);

    const int exponent =
        static_cast<int>(((bits >> kMantissaBits) & kExponentMask) - kBias);
    // Already an integer.
    if (exponent >= kMantissaBits) continue;
    // |v| <= 1 => 0 or 1.
    if (exponent < 0) {
      v.raw[i] = positive ? Float{1} : Float{-0.0};
      continue;
    }

    const Bits mantissa_mask = kMantissaMask >> exponent;
    // Already an integer
    if ((bits & mantissa_mask) == 0) continue;

    // Clear fractional bits and round up
    if (positive) bits += (kMantissaMask + 1) >> exponent;
    bits &= ~mantissa_mask;

    CopySameSize(&bits, &v.raw[i]);
  }
  return v;
}

// Toward -infinity, aka floor
template <typename Float, size_t N>
Vec128<Float, N> Floor(Vec128<Float, N> v) {
  constexpr int kMantissaBits = MantissaBits<Float>();
  using Bits = MakeUnsigned<Float>;
  const Bits kExponentMask = MaxExponentField<Float>();
  const Bits kMantissaMask = MantissaMask<Float>();
  const Bits kBias = kExponentMask / 2;

  for (size_t i = 0; i < N; ++i) {
    const bool negative = v.raw[i] < Float(0.0);

    Bits bits;
    CopySameSize(&v.raw[i], &bits);

    const int exponent =
        static_cast<int>(((bits >> kMantissaBits) & kExponentMask) - kBias);
    // Already an integer.
    if (exponent >= kMantissaBits) continue;
    // |v| <= 1 => -1 or 0.
    if (exponent < 0) {
      v.raw[i] = negative ? Float(-1.0) : Float(0.0);
      continue;
    }

    const Bits mantissa_mask = kMantissaMask >> exponent;
    // Already an integer
    if ((bits & mantissa_mask) == 0) continue;

    // Clear fractional bits and round down
    if (negative) bits += (kMantissaMask + 1) >> exponent;
    bits &= ~mantissa_mask;

    CopySameSize(&bits, &v.raw[i]);
  }
  return v;
}

// ------------------------------ Floating-point classification

template <typename T, size_t N>
HWY_API Mask128<T, N> IsNaN(Vec128<T, N> v) {
  Mask128<T, N> ret;
  for (size_t i = 0; i < N; ++i) {
    // std::isnan returns false for 0x7F..FF in clang AVX3 builds, so DIY.
    MakeUnsigned<T> bits;
    CopySameSize(&v.raw[i], &bits);
    bits += bits;
    bits >>= 1;  // clear sign bit
    // NaN if all exponent bits are set and the mantissa is not zero.
    ret.bits[i] = Mask128<T, N>::FromBool(bits > ExponentMask<T>());
  }
  return ret;
}

template <typename T, size_t N>
HWY_API Mask128<T, N> IsInf(Vec128<T, N> v) {
  static_assert(IsFloat<T>(), "Only for float");
  const DFromV<decltype(v)> d;
  const RebindToSigned<decltype(d)> di;
  const VFromD<decltype(di)> vi = BitCast(di, v);
  // 'Shift left' to clear the sign bit, check for exponent=max and mantissa=0.
  return RebindMask(d, Eq(Add(vi, vi), Set(di, hwy::MaxExponentTimes2<T>())));
}

// Returns whether normal/subnormal/zero.
template <typename T, size_t N>
HWY_API Mask128<T, N> IsFinite(Vec128<T, N> v) {
  static_assert(IsFloat<T>(), "Only for float");
  const DFromV<decltype(v)> d;
  const RebindToUnsigned<decltype(d)> du;
  const RebindToSigned<decltype(d)> di;  // cheaper than unsigned comparison
  using VI = VFromD<decltype(di)>;
  using VU = VFromD<decltype(du)>;
  const VU vu = BitCast(du, v);
  // 'Shift left' to clear the sign bit, then right so we can compare with the
  // max exponent (cannot compare with MaxExponentTimes2 directly because it is
  // negative and non-negative floats would be greater).
  const VI exp =
      BitCast(di, ShiftRight<hwy::MantissaBits<T>() + 1>(Add(vu, vu)));
  return RebindMask(d, Lt(exp, Set(di, hwy::MaxExponentField<T>())));
}

// ================================================== COMPARE

template <typename T, size_t N>
HWY_API Mask128<T, N> operator==(Vec128<T, N> a, Vec128<T, N> b) {
  Mask128<T, N> m;
  for (size_t i = 0; i < N; ++i) {
    m.bits[i] = Mask128<T, N>::FromBool(a.raw[i] == b.raw[i]);
  }
  return m;
}

template <typename T, size_t N>
HWY_API Mask128<T, N> operator!=(Vec128<T, N> a, Vec128<T, N> b) {
  Mask128<T, N> m;
  for (size_t i = 0; i < N; ++i) {
    m.bits[i] = Mask128<T, N>::FromBool(a.raw[i] != b.raw[i]);
  }
  return m;
}

template <typename T, size_t N>
HWY_API Mask128<T, N> TestBit(Vec128<T, N> v, Vec128<T, N> bit) {
  static_assert(!hwy::IsFloat<T>(), "Only integer vectors supported");
  return (v & bit) == bit;
}

template <typename T, size_t N>
HWY_API Mask128<T, N> operator<(Vec128<T, N> a, Vec128<T, N> b) {
  Mask128<T, N> m;
  for (size_t i = 0; i < N; ++i) {
    m.bits[i] = Mask128<T, N>::FromBool(a.raw[i] < b.raw[i]);
  }
  return m;
}
template <typename T, size_t N>
HWY_API Mask128<T, N> operator>(Vec128<T, N> a, Vec128<T, N> b) {
  Mask128<T, N> m;
  for (size_t i = 0; i < N; ++i) {
    m.bits[i] = Mask128<T, N>::FromBool(a.raw[i] > b.raw[i]);
  }
  return m;
}

template <typename T, size_t N>
HWY_API Mask128<T, N> operator<=(Vec128<T, N> a, Vec128<T, N> b) {
  Mask128<T, N> m;
  for (size_t i = 0; i < N; ++i) {
    m.bits[i] = Mask128<T, N>::FromBool(a.raw[i] <= b.raw[i]);
  }
  return m;
}
template <typename T, size_t N>
HWY_API Mask128<T, N> operator>=(Vec128<T, N> a, Vec128<T, N> b) {
  Mask128<T, N> m;
  for (size_t i = 0; i < N; ++i) {
    m.bits[i] = Mask128<T, N>::FromBool(a.raw[i] >= b.raw[i]);
  }
  return m;
}

// ------------------------------ Lt128

// Only makes sense for full vectors of u64.
template <class D>
HWY_API MFromD<D> Lt128(D /* tag */, Vec128<uint64_t> a, Vec128<uint64_t> b) {
  const bool lt =
      (a.raw[1] < b.raw[1]) || (a.raw[1] == b.raw[1] && a.raw[0] < b.raw[0]);
  Mask128<uint64_t> ret;
  ret.bits[0] = ret.bits[1] = Mask128<uint64_t>::FromBool(lt);
  return ret;
}

template <class D>
HWY_API MFromD<D> Lt128Upper(D /* tag */, Vec128<uint64_t> a,
                             Vec128<uint64_t> b) {
  const bool lt = a.raw[1] < b.raw[1];
  Mask128<uint64_t> ret;
  ret.bits[0] = ret.bits[1] = Mask128<uint64_t>::FromBool(lt);
  return ret;
}

// ------------------------------ Eq128

// Only makes sense for full vectors of u64.
template <class D>
HWY_API MFromD<D> Eq128(D /* tag */, Vec128<uint64_t> a, Vec128<uint64_t> b) {
  const bool eq = a.raw[1] == b.raw[1] && a.raw[0] == b.raw[0];
  Mask128<uint64_t> ret;
  ret.bits[0] = ret.bits[1] = Mask128<uint64_t>::FromBool(eq);
  return ret;
}

template <class D>
HWY_API Mask128<uint64_t> Ne128(D /* tag */, Vec128<uint64_t> a,
                                Vec128<uint64_t> b) {
  const bool ne = a.raw[1] != b.raw[1] || a.raw[0] != b.raw[0];
  Mask128<uint64_t> ret;
  ret.bits[0] = ret.bits[1] = Mask128<uint64_t>::FromBool(ne);
  return ret;
}

template <class D>
HWY_API MFromD<D> Eq128Upper(D /* tag */, Vec128<uint64_t> a,
                             Vec128<uint64_t> b) {
  const bool eq = a.raw[1] == b.raw[1];
  Mask128<uint64_t> ret;
  ret.bits[0] = ret.bits[1] = Mask128<uint64_t>::FromBool(eq);
  return ret;
}

template <class D>
HWY_API MFromD<D> Ne128Upper(D /* tag */, Vec128<uint64_t> a,
                             Vec128<uint64_t> b) {
  const bool ne = a.raw[1] != b.raw[1];
  Mask128<uint64_t> ret;
  ret.bits[0] = ret.bits[1] = Mask128<uint64_t>::FromBool(ne);
  return ret;
}

// ------------------------------ Min128, Max128 (Lt128)

template <class D>
HWY_API VFromD<D> Min128(D d, VFromD<D> a, VFromD<D> b) {
  return IfThenElse(Lt128(d, a, b), a, b);
}

template <class D>
HWY_API VFromD<D> Max128(D d, VFromD<D> a, VFromD<D> b) {
  return IfThenElse(Lt128(d, b, a), a, b);
}

template <class D>
HWY_API VFromD<D> Min128Upper(D d, VFromD<D> a, VFromD<D> b) {
  return IfThenElse(Lt128Upper(d, a, b), a, b);
}

template <class D>
HWY_API VFromD<D> Max128Upper(D d, VFromD<D> a, VFromD<D> b) {
  return IfThenElse(Lt128Upper(d, b, a), a, b);
}

// ================================================== MEMORY

// ------------------------------ Load

template <class D>
HWY_API VFromD<D> Load(D d, const TFromD<D>* HWY_RESTRICT aligned) {
  VFromD<D> v;
  CopyBytes<d.MaxBytes()>(aligned, v.raw);  // copy from array
  return v;
}

template <class D>
HWY_API VFromD<D> MaskedLoad(MFromD<D> m, D d,
                             const TFromD<D>* HWY_RESTRICT p) {
  return IfThenElseZero(m, LoadU(d, p));
}

template <class D>
HWY_API VFromD<D> MaskedLoadOr(VFromD<D> v, MFromD<D> m, D d,
                               const TFromD<D>* HWY_RESTRICT p) {
  return IfThenElse(m, LoadU(d, p), v);
}

template <class D>
HWY_API VFromD<D> LoadU(D d, const TFromD<D>* HWY_RESTRICT p) {
  return Load(d, p);
}

// In some use cases, "load single lane" is sufficient; otherwise avoid this.
template <class D>
HWY_API VFromD<D> LoadDup128(D d, const TFromD<D>* HWY_RESTRICT aligned) {
  return Load(d, aligned);
}

// ------------------------------ Store

template <class D>
HWY_API void Store(VFromD<D> v, D d, TFromD<D>* HWY_RESTRICT aligned) {
  CopyBytes<d.MaxBytes()>(v.raw, aligned);  // copy to array
}

template <class D>
HWY_API void StoreU(VFromD<D> v, D d, TFromD<D>* HWY_RESTRICT p) {
  Store(v, d, p);
}

template <class D>
HWY_API void BlendedStore(VFromD<D> v, MFromD<D> m, D d,
                          TFromD<D>* HWY_RESTRICT p) {
  for (size_t i = 0; i < MaxLanes(d); ++i) {
    if (m.bits[i]) p[i] = v.raw[i];
  }
}

// ------------------------------ LoadInterleaved2/3/4

// Per-target flag to prevent generic_ops-inl.h from defining LoadInterleaved2.
// We implement those here because scalar code is likely faster than emulation
// via shuffles.
#ifdef HWY_NATIVE_LOAD_STORE_INTERLEAVED
#undef HWY_NATIVE_LOAD_STORE_INTERLEAVED
#else
#define HWY_NATIVE_LOAD_STORE_INTERLEAVED
#endif

template <class D, typename T = TFromD<D>>
HWY_API void LoadInterleaved2(D d, const T* HWY_RESTRICT unaligned,
                              VFromD<D>& v0, VFromD<D>& v1) {
  alignas(16) T buf0[MaxLanes(d)];
  alignas(16) T buf1[MaxLanes(d)];
  for (size_t i = 0; i < MaxLanes(d); ++i) {
    buf0[i] = *unaligned++;
    buf1[i] = *unaligned++;
  }
  v0 = Load(d, buf0);
  v1 = Load(d, buf1);
}

template <class D, typename T = TFromD<D>>
HWY_API void LoadInterleaved3(D d, const T* HWY_RESTRICT unaligned,
                              VFromD<D>& v0, VFromD<D>& v1, VFromD<D>& v2) {
  alignas(16) T buf0[MaxLanes(d)];
  alignas(16) T buf1[MaxLanes(d)];
  alignas(16) T buf2[MaxLanes(d)];
  for (size_t i = 0; i < MaxLanes(d); ++i) {
    buf0[i] = *unaligned++;
    buf1[i] = *unaligned++;
    buf2[i] = *unaligned++;
  }
  v0 = Load(d, buf0);
  v1 = Load(d, buf1);
  v2 = Load(d, buf2);
}

template <class D, typename T = TFromD<D>>
HWY_API void LoadInterleaved4(D d, const T* HWY_RESTRICT unaligned,
                              VFromD<D>& v0, VFromD<D>& v1, VFromD<D>& v2,
                              VFromD<D>& v3) {
  alignas(16) T buf0[MaxLanes(d)];
  alignas(16) T buf1[MaxLanes(d)];
  alignas(16) T buf2[MaxLanes(d)];
  alignas(16) T buf3[MaxLanes(d)];
  for (size_t i = 0; i < MaxLanes(d); ++i) {
    buf0[i] = *unaligned++;
    buf1[i] = *unaligned++;
    buf2[i] = *unaligned++;
    buf3[i] = *unaligned++;
  }
  v0 = Load(d, buf0);
  v1 = Load(d, buf1);
  v2 = Load(d, buf2);
  v3 = Load(d, buf3);
}

// ------------------------------ StoreInterleaved2/3/4

template <class D>
HWY_API void StoreInterleaved2(VFromD<D> v0, VFromD<D> v1, D d,
                               TFromD<D>* HWY_RESTRICT unaligned) {
  for (size_t i = 0; i < MaxLanes(d); ++i) {
    *unaligned++ = v0.raw[i];
    *unaligned++ = v1.raw[i];
  }
}

template <class D>
HWY_API void StoreInterleaved3(VFromD<D> v0, VFromD<D> v1, VFromD<D> v2, D d,
                               TFromD<D>* HWY_RESTRICT unaligned) {
  for (size_t i = 0; i < MaxLanes(d); ++i) {
    *unaligned++ = v0.raw[i];
    *unaligned++ = v1.raw[i];
    *unaligned++ = v2.raw[i];
  }
}

template <class D>
HWY_API void StoreInterleaved4(VFromD<D> v0, VFromD<D> v1, VFromD<D> v2,
                               VFromD<D> v3, D d,
                               TFromD<D>* HWY_RESTRICT unaligned) {
  for (size_t i = 0; i < MaxLanes(d); ++i) {
    *unaligned++ = v0.raw[i];
    *unaligned++ = v1.raw[i];
    *unaligned++ = v2.raw[i];
    *unaligned++ = v3.raw[i];
  }
}

// ------------------------------ Stream
template <class D>
HWY_API void Stream(VFromD<D> v, D d, TFromD<D>* HWY_RESTRICT aligned) {
  Store(v, d, aligned);
}

// ------------------------------ Scatter

template <class D, typename T = TFromD<D>, typename Offset>
HWY_API void ScatterOffset(VFromD<D> v, D d, T* base,
                           Vec128<Offset, HWY_MAX_LANES_D(D)> offset) {
  static_assert(sizeof(T) == sizeof(Offset), "Index/lane size must match");
  for (size_t i = 0; i < MaxLanes(d); ++i) {
    uint8_t* const base8 = reinterpret_cast<uint8_t*>(base) + offset.raw[i];
    CopyBytes<sizeof(T)>(&v.raw[i], base8);  // copy to bytes
  }
}

template <class D, typename T = TFromD<D>, typename Index>
HWY_API void ScatterIndex(VFromD<D> v, D d, T* HWY_RESTRICT base,
                          Vec128<Index, HWY_MAX_LANES_D(D)> index) {
  static_assert(sizeof(T) == sizeof(Index), "Index/lane size must match");
  for (size_t i = 0; i < MaxLanes(d); ++i) {
    base[index.raw[i]] = v.raw[i];
  }
}

// ------------------------------ Gather

template <class D, typename T = TFromD<D>, typename Offset>
HWY_API VFromD<D> GatherOffset(D d, const T* base,
                               Vec128<Offset, HWY_MAX_LANES_D(D)> offset) {
  static_assert(sizeof(T) == sizeof(Offset), "Index/lane size must match");
  VFromD<D> v;
  for (size_t i = 0; i < MaxLanes(d); ++i) {
    const uint8_t* base8 =
        reinterpret_cast<const uint8_t*>(base) + offset.raw[i];
    CopyBytes<sizeof(T)>(base8, &v.raw[i]);  // copy from bytes
  }
  return v;
}

template <class D, typename T = TFromD<D>, typename Index>
HWY_API VFromD<D> GatherIndex(D d, const T* HWY_RESTRICT base,
                              Vec128<Index, HWY_MAX_LANES_D(D)> index) {
  static_assert(sizeof(T) == sizeof(Index), "Index/lane size must match");
  VFromD<D> v;
  for (size_t i = 0; i < MaxLanes(d); ++i) {
    v.raw[i] = base[index.raw[i]];
  }
  return v;
}

// ================================================== CONVERT

// ConvertTo and DemoteTo with floating-point input and integer output truncate
// (rounding toward zero).

template <class DTo, typename TFrom, HWY_IF_NOT_SPECIAL_FLOAT(TFrom)>
HWY_API VFromD<DTo> PromoteTo(DTo d, Vec128<TFrom, HWY_MAX_LANES_D(DTo)> from) {
  static_assert(sizeof(TFromD<DTo>) > sizeof(TFrom), "Not promoting");
  VFromD<DTo> ret;
  for (size_t i = 0; i < MaxLanes(d); ++i) {
    // For bits Y > X, floatX->floatY and intX->intY are always representable.
    ret.raw[i] = static_cast<TFromD<DTo>>(from.raw[i]);
  }
  return ret;
}

// MSVC 19.10 cannot deduce the argument type if HWY_IF_FLOAT(TFrom) is here,
// so we overload for TFrom=double and ToT={float,int32_t}.
template <class D, HWY_IF_F32_D(D)>
HWY_API VFromD<D> DemoteTo(D d, VFromD<Rebind<double, D>> from) {
  VFromD<D> ret;
  for (size_t i = 0; i < MaxLanes(d); ++i) {
    // Prevent ubsan errors when converting float to narrower integer/float
    if (std::isinf(from.raw[i]) ||
        std::fabs(from.raw[i]) > static_cast<double>(HighestValue<float>())) {
      ret.raw[i] = std::signbit(from.raw[i]) ? LowestValue<float>()
                                             : HighestValue<float>();
      continue;
    }
    ret.raw[i] = static_cast<float>(from.raw[i]);
  }
  return ret;
}
template <class D, HWY_IF_I32_D(D)>
HWY_API VFromD<D> DemoteTo(D d, VFromD<Rebind<double, D>> from) {
  VFromD<D> ret;
  for (size_t i = 0; i < MaxLanes(d); ++i) {
    // Prevent ubsan errors when converting int32_t to narrower integer/int32_t
    if (std::isinf(from.raw[i]) ||
        std::fabs(from.raw[i]) > static_cast<double>(HighestValue<int32_t>())) {
      ret.raw[i] = std::signbit(from.raw[i]) ? LowestValue<int32_t>()
                                             : HighestValue<int32_t>();
      continue;
    }
    ret.raw[i] = static_cast<int32_t>(from.raw[i]);
  }
  return ret;
}

template <class DTo, typename TFrom, size_t N, HWY_IF_SIGNED(TFrom),
          HWY_IF_NOT_FLOAT_NOR_SPECIAL(TFromD<DTo>)>
HWY_API VFromD<DTo> DemoteTo(DTo /* tag */, Vec128<TFrom, N> from) {
  using TTo = TFromD<DTo>;
  static_assert(sizeof(TTo) < sizeof(TFrom), "Not demoting");

  VFromD<DTo> ret;
  for (size_t i = 0; i < N; ++i) {
    // Int to int: choose closest value in ToT to `from` (avoids UB)
    from.raw[i] =
        HWY_MIN(HWY_MAX(LimitsMin<TTo>(), from.raw[i]), LimitsMax<TTo>());
    ret.raw[i] = static_cast<TTo>(from.raw[i]);
  }
  return ret;
}

template <class DTo, typename TFrom, size_t N, HWY_IF_UNSIGNED(TFrom),
          HWY_IF_UNSIGNED_D(DTo)>
HWY_API VFromD<DTo> DemoteTo(DTo /* tag */, Vec128<TFrom, N> from) {
  using TTo = TFromD<DTo>;
  static_assert(sizeof(TTo) < sizeof(TFrom), "Not demoting");

  VFromD<DTo> ret;
  for (size_t i = 0; i < N; ++i) {
    // Int to int: choose closest value in ToT to `from` (avoids UB)
    from.raw[i] = HWY_MIN(from.raw[i], LimitsMax<TTo>());
    ret.raw[i] = static_cast<TTo>(from.raw[i]);
  }
  return ret;
}

template <class DBF16, HWY_IF_BF16_D(DBF16), class VF32>
HWY_API VFromD<DBF16> ReorderDemote2To(DBF16 dbf16, VF32 a, VF32 b) {
  const Repartition<uint32_t, decltype(dbf16)> du32;
  const VFromD<decltype(du32)> b_in_lower = ShiftRight<16>(BitCast(du32, b));
  // Avoid OddEven - we want the upper half of `a` even on big-endian systems.
  const VFromD<decltype(du32)> a_mask = Set(du32, 0xFFFF0000);
  return BitCast(dbf16, IfVecThenElse(a_mask, BitCast(du32, a), b_in_lower));
}

template <class DN, HWY_IF_NOT_FLOAT_NOR_SPECIAL(TFromD<DN>), class V,
          HWY_IF_SIGNED_V(V), HWY_IF_T_SIZE_V(V, sizeof(TFromD<DN>) * 2),
          HWY_IF_LANES_D(DN, HWY_MAX_LANES_D(DFromV<V>) * 2)>
HWY_API VFromD<DN> ReorderDemote2To(DN dn, V a, V b) {
  const RepartitionToWide<decltype(dn)> dw;
  const size_t NW = Lanes(dw);
  using TN = TFromD<DN>;
  const TN min = LimitsMin<TN>();
  const TN max = LimitsMax<TN>();
  VFromD<DN> ret;
  for (size_t i = 0; i < NW; ++i) {
    ret.raw[i] = static_cast<TN>(HWY_MIN(HWY_MAX(min, a.raw[i]), max));
  }
  for (size_t i = 0; i < NW; ++i) {
    ret.raw[NW + i] = static_cast<TN>(HWY_MIN(HWY_MAX(min, b.raw[i]), max));
  }
  return ret;
}

template <class DN, HWY_IF_UNSIGNED_D(DN), class V, HWY_IF_UNSIGNED_V(V),
          HWY_IF_T_SIZE_V(V, sizeof(TFromD<DN>) * 2),
          HWY_IF_LANES_D(DN, HWY_MAX_LANES_D(DFromV<V>) * 2)>
HWY_API VFromD<DN> ReorderDemote2To(DN dn, V a, V b) {
  const RepartitionToWide<decltype(dn)> dw;
  const size_t NW = Lanes(dw);
  using TN = TFromD<DN>;
  const TN max = LimitsMax<TN>();
  VFromD<DN> ret;
  for (size_t i = 0; i < NW; ++i) {
    ret.raw[i] = static_cast<TN>(HWY_MIN(a.raw[i], max));
  }
  for (size_t i = 0; i < NW; ++i) {
    ret.raw[NW + i] = static_cast<TN>(HWY_MIN(b.raw[i], max));
  }
  return ret;
}

template <class DN, HWY_IF_NOT_FLOAT_NOR_SPECIAL(TFromD<DN>), class V,
          HWY_IF_NOT_FLOAT_NOR_SPECIAL_V(V),
          HWY_IF_T_SIZE_V(V, sizeof(TFromD<DN>) * 2),
          HWY_IF_LANES_D(DN, HWY_MAX_LANES_D(DFromV<V>) * 2)>
HWY_API VFromD<DN> OrderedDemote2To(DN dn, V a, V b) {
  return ReorderDemote2To(dn, a, b);
}

template <class DN, HWY_IF_BF16_D(DN), class V, HWY_IF_F32_D(DFromV<V>),
          HWY_IF_LANES_D(DN, HWY_MAX_LANES_D(DFromV<V>) * 2)>
HWY_API VFromD<DN> OrderedDemote2To(DN dn, V a, V b) {
  const RebindToUnsigned<DFromV<decltype(a)>> du32;
  const size_t NW = Lanes(du32);
  VFromD<Repartition<uint16_t, DN>> ret;

  const auto a_bits = BitCast(du32, a);
  const auto b_bits = BitCast(du32, b);

  for (size_t i = 0; i < NW; ++i) {
    ret.raw[i] = static_cast<uint16_t>(a_bits.raw[i] >> 16);
  }
  for (size_t i = 0; i < NW; ++i) {
    ret.raw[NW + i] = static_cast<uint16_t>(b_bits.raw[i] >> 16);
  }
  return BitCast(dn, ret);
}

namespace detail {

HWY_INLINE void StoreU16ToF16(const uint16_t val,
                              hwy::float16_t* HWY_RESTRICT to) {
  CopySameSize(&val, to);
}

HWY_INLINE uint16_t U16FromF16(const hwy::float16_t* HWY_RESTRICT from) {
  uint16_t bits16;
  CopySameSize(from, &bits16);
  return bits16;
}

}  // namespace detail

template <class D, HWY_IF_F32_D(D), size_t N>
HWY_API VFromD<D> PromoteTo(D /* tag */, Vec128<float16_t, N> v) {
  VFromD<D> ret;
  for (size_t i = 0; i < N; ++i) {
    const uint16_t bits16 = detail::U16FromF16(&v.raw[i]);
    const uint32_t sign = static_cast<uint32_t>(bits16 >> 15);
    const uint32_t biased_exp = (bits16 >> 10) & 0x1F;
    const uint32_t mantissa = bits16 & 0x3FF;

    // Subnormal or zero
    if (biased_exp == 0) {
      const float subnormal =
          (1.0f / 16384) * (static_cast<float>(mantissa) * (1.0f / 1024));
      ret.raw[i] = sign ? -subnormal : subnormal;
      continue;
    }

    // Normalized: convert the representation directly (faster than
    // ldexp/tables).
    const uint32_t biased_exp32 = biased_exp + (127 - 15);
    const uint32_t mantissa32 = mantissa << (23 - 10);
    const uint32_t bits32 = (sign << 31) | (biased_exp32 << 23) | mantissa32;
    CopySameSize(&bits32, &ret.raw[i]);
  }
  return ret;
}

template <class D, HWY_IF_F32_D(D), size_t N>
HWY_API VFromD<D> PromoteTo(D /* tag */, Vec128<bfloat16_t, N> v) {
  VFromD<D> ret;
  for (size_t i = 0; i < N; ++i) {
    ret.raw[i] = F32FromBF16(v.raw[i]);
  }
  return ret;
}

template <class D, HWY_IF_F16_D(D), size_t N>
HWY_API VFromD<D> DemoteTo(D /* tag */, Vec128<float, N> v) {
  VFromD<D> ret;
  for (size_t i = 0; i < N; ++i) {
    uint32_t bits32;
    CopySameSize(&v.raw[i], &bits32);
    const uint32_t sign = bits32 >> 31;
    const uint32_t biased_exp32 = (bits32 >> 23) & 0xFF;
    const uint32_t mantissa32 = bits32 & 0x7FFFFF;

    const int32_t exp = HWY_MIN(static_cast<int32_t>(biased_exp32) - 127, 15);

    // Tiny or zero => zero.
    if (exp < -24) {
      ZeroBytes<sizeof(uint16_t)>(&ret.raw[i]);
      continue;
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
    detail::StoreU16ToF16(narrowed, &ret.raw[i]);
  }
  return ret;
}

template <class D, HWY_IF_BF16_D(D), size_t N>
HWY_API VFromD<D> DemoteTo(D /* tag */, Vec128<float, N> v) {
  VFromD<D> ret;
  for (size_t i = 0; i < N; ++i) {
    ret.raw[i] = BF16FromF32(v.raw[i]);
  }
  return ret;
}

// Tag dispatch instead of SFINAE for MSVC 2017 compatibility
namespace detail {

template <typename TFrom, typename DTo>
HWY_API VFromD<DTo> ConvertTo(hwy::FloatTag /*tag*/, DTo /*tag*/,
                              Vec128<TFrom, HWY_MAX_LANES_D(DTo)> from) {
  using ToT = TFromD<DTo>;
  static_assert(sizeof(ToT) == sizeof(TFrom), "Should have same size");
  VFromD<DTo> ret;
  constexpr size_t N = HWY_MAX_LANES_D(DTo);
  for (size_t i = 0; i < N; ++i) {
    // float## -> int##: return closest representable value. We cannot exactly
    // represent LimitsMax<ToT> in TFrom, so use double.
    const double f = static_cast<double>(from.raw[i]);
    if (std::isinf(from.raw[i]) ||
        std::fabs(f) > static_cast<double>(LimitsMax<ToT>())) {
      ret.raw[i] =
          std::signbit(from.raw[i]) ? LimitsMin<ToT>() : LimitsMax<ToT>();
      continue;
    }
    ret.raw[i] = static_cast<ToT>(from.raw[i]);
  }
  return ret;
}

template <typename TFrom, typename DTo>
HWY_API VFromD<DTo> ConvertTo(hwy::NonFloatTag /*tag*/, DTo /* tag */,
                              Vec128<TFrom, HWY_MAX_LANES_D(DTo)> from) {
  using ToT = TFromD<DTo>;
  static_assert(sizeof(ToT) == sizeof(TFrom), "Should have same size");
  VFromD<DTo> ret;
  constexpr size_t N = HWY_MAX_LANES_D(DTo);
  for (size_t i = 0; i < N; ++i) {
    // int## -> float##: no check needed
    ret.raw[i] = static_cast<ToT>(from.raw[i]);
  }
  return ret;
}

}  // namespace detail

template <class DTo, typename TFrom>
HWY_API VFromD<DTo> ConvertTo(DTo d, Vec128<TFrom, HWY_MAX_LANES_D(DTo)> from) {
  return detail::ConvertTo(hwy::IsFloatTag<TFrom>(), d, from);
}

template <size_t N>
HWY_API Vec128<uint8_t, N> U8FromU32(Vec128<uint32_t, N> v) {
  return DemoteTo(Simd<uint8_t, N, 0>(), v);
}

// ------------------------------ Truncations

template <class D, HWY_IF_U8_D(D), size_t N>
HWY_API VFromD<D> TruncateTo(D /* tag */, Vec128<uint64_t, N> v) {
  VFromD<D> ret;
  for (size_t i = 0; i < N; ++i) {
    ret.raw[i] = static_cast<uint8_t>(v.raw[i] & 0xFF);
  }
  return ret;
}

template <class D, HWY_IF_U16_D(D), size_t N>
HWY_API VFromD<D> TruncateTo(D /* tag */, Vec128<uint64_t, N> v) {
  VFromD<D> ret;
  for (size_t i = 0; i < N; ++i) {
    ret.raw[i] = static_cast<uint16_t>(v.raw[i] & 0xFFFF);
  }
  return ret;
}

template <class D, HWY_IF_U32_D(D), size_t N>
HWY_API VFromD<D> TruncateTo(D /* tag */, Vec128<uint64_t, N> v) {
  VFromD<D> ret;
  for (size_t i = 0; i < N; ++i) {
    ret.raw[i] = static_cast<uint32_t>(v.raw[i] & 0xFFFFFFFFu);
  }
  return ret;
}

template <class D, HWY_IF_U8_D(D), size_t N>
HWY_API VFromD<D> TruncateTo(D /* tag */, Vec128<uint32_t, N> v) {
  VFromD<D> ret;
  for (size_t i = 0; i < N; ++i) {
    ret.raw[i] = static_cast<uint8_t>(v.raw[i] & 0xFF);
  }
  return ret;
}

template <class D, HWY_IF_U16_D(D), size_t N>
HWY_API VFromD<D> TruncateTo(D /* tag */, Vec128<uint32_t, N> v) {
  VFromD<D> ret;
  for (size_t i = 0; i < N; ++i) {
    ret.raw[i] = static_cast<uint16_t>(v.raw[i] & 0xFFFF);
  }
  return ret;
}

template <class D, HWY_IF_U8_D(D), size_t N>
HWY_API VFromD<D> TruncateTo(D /* tag */, Vec128<uint16_t, N> v) {
  VFromD<D> ret;
  for (size_t i = 0; i < N; ++i) {
    ret.raw[i] = static_cast<uint8_t>(v.raw[i] & 0xFF);
  }
  return ret;
}

#ifdef HWY_NATIVE_ORDERED_TRUNCATE_2_TO
#undef HWY_NATIVE_ORDERED_TRUNCATE_2_TO
#else
#define HWY_NATIVE_ORDERED_TRUNCATE_2_TO
#endif

template <class DN, HWY_IF_UNSIGNED_D(DN), class V, HWY_IF_UNSIGNED_V(V),
          HWY_IF_T_SIZE_V(V, sizeof(TFromD<DN>) * 2),
          HWY_IF_LANES_D(DN, HWY_MAX_LANES_D(DFromV<V>) * 2)>
HWY_API VFromD<DN> OrderedTruncate2To(DN dn, V a, V b) {
  const RepartitionToWide<decltype(dn)> dw;
  const size_t NW = Lanes(dw);
  using TW = TFromD<decltype(dw)>;
  using TN = TFromD<decltype(dn)>;
  VFromD<DN> ret;
  constexpr TW max_val{LimitsMax<TN>()};

  for (size_t i = 0; i < NW; ++i) {
    ret.raw[i] = static_cast<TN>(a.raw[i] & max_val);
  }
  for (size_t i = 0; i < NW; ++i) {
    ret.raw[NW + i] = static_cast<TN>(b.raw[i] & max_val);
  }
  return ret;
}

// ================================================== COMBINE

template <typename T, size_t N>
HWY_API Vec128<T, N / 2> LowerHalf(Vec128<T, N> v) {
  Vec128<T, N / 2> ret;
  CopyBytes<N / 2 * sizeof(T)>(v.raw, ret.raw);
  return ret;
}

template <class D>
HWY_API VFromD<D> LowerHalf(D /* tag */, VFromD<Twice<D>> v) {
  return LowerHalf(v);
}

template <class D>
HWY_API VFromD<D> UpperHalf(D d, VFromD<Twice<D>> v) {
  VFromD<D> ret;
  CopyBytes<d.MaxBytes()>(&v.raw[MaxLanes(d)], ret.raw);
  return ret;
}

template <class D>
HWY_API VFromD<D> ZeroExtendVector(D d, VFromD<Half<D>> v) {
  const Half<decltype(d)> dh;
  VFromD<D> ret;  // zero-initialized
  CopyBytes<dh.MaxBytes()>(v.raw, ret.raw);
  return ret;
}

template <class D, class VH = VFromD<Half<D>>>
HWY_API VFromD<D> Combine(D d, VH hi_half, VH lo_half) {
  const Half<decltype(d)> dh;
  VFromD<D> ret;
  CopyBytes<dh.MaxBytes()>(lo_half.raw, &ret.raw[0]);
  CopyBytes<dh.MaxBytes()>(hi_half.raw, &ret.raw[MaxLanes(dh)]);
  return ret;
}

template <class D>
HWY_API VFromD<D> ConcatLowerLower(D d, VFromD<D> hi, VFromD<D> lo) {
  const Half<decltype(d)> dh;
  VFromD<D> ret;
  CopyBytes<dh.MaxBytes()>(lo.raw, &ret.raw[0]);
  CopyBytes<dh.MaxBytes()>(hi.raw, &ret.raw[MaxLanes(dh)]);
  return ret;
}

template <class D>
HWY_API VFromD<D> ConcatUpperUpper(D d, VFromD<D> hi, VFromD<D> lo) {
  const Half<decltype(d)> dh;
  VFromD<D> ret;
  CopyBytes<dh.MaxBytes()>(&lo.raw[MaxLanes(dh)], &ret.raw[0]);
  CopyBytes<dh.MaxBytes()>(&hi.raw[MaxLanes(dh)], &ret.raw[MaxLanes(dh)]);
  return ret;
}

template <class D>
HWY_API VFromD<D> ConcatLowerUpper(D d, VFromD<D> hi, VFromD<D> lo) {
  const Half<decltype(d)> dh;
  VFromD<D> ret;
  CopyBytes<dh.MaxBytes()>(&lo.raw[MaxLanes(dh)], &ret.raw[0]);
  CopyBytes<dh.MaxBytes()>(hi.raw, &ret.raw[MaxLanes(dh)]);
  return ret;
}

template <class D>
HWY_API VFromD<D> ConcatUpperLower(D d, VFromD<D> hi, VFromD<D> lo) {
  const Half<decltype(d)> dh;
  VFromD<D> ret;
  CopyBytes<dh.MaxBytes()>(lo.raw, &ret.raw[0]);
  CopyBytes<dh.MaxBytes()>(&hi.raw[MaxLanes(dh)], &ret.raw[MaxLanes(dh)]);
  return ret;
}

template <class D>
HWY_API VFromD<D> ConcatEven(D d, VFromD<D> hi, VFromD<D> lo) {
  const Half<decltype(d)> dh;
  VFromD<D> ret;
  for (size_t i = 0; i < MaxLanes(dh); ++i) {
    ret.raw[i] = lo.raw[2 * i];
  }
  for (size_t i = 0; i < MaxLanes(dh); ++i) {
    ret.raw[MaxLanes(dh) + i] = hi.raw[2 * i];
  }
  return ret;
}

template <class D>
HWY_API VFromD<D> ConcatOdd(D d, VFromD<D> hi, VFromD<D> lo) {
  const Half<decltype(d)> dh;
  VFromD<D> ret;
  for (size_t i = 0; i < MaxLanes(dh); ++i) {
    ret.raw[i] = lo.raw[2 * i + 1];
  }
  for (size_t i = 0; i < MaxLanes(dh); ++i) {
    ret.raw[MaxLanes(dh) + i] = hi.raw[2 * i + 1];
  }
  return ret;
}

// ------------------------------ CombineShiftRightBytes
template <int kBytes, class D>
HWY_API VFromD<D> CombineShiftRightBytes(D d, VFromD<D> hi, VFromD<D> lo) {
  VFromD<D> ret;
  const uint8_t* HWY_RESTRICT lo8 =
      reinterpret_cast<const uint8_t * HWY_RESTRICT>(lo.raw);
  uint8_t* HWY_RESTRICT ret8 =
      reinterpret_cast<uint8_t * HWY_RESTRICT>(ret.raw);
  CopyBytes<d.MaxBytes() - kBytes>(lo8 + kBytes, ret8);
  CopyBytes<kBytes>(hi.raw, ret8 + d.MaxBytes() - kBytes);
  return ret;
}

// ------------------------------ ShiftLeftBytes

template <int kBytes, class D>
HWY_API VFromD<D> ShiftLeftBytes(D d, VFromD<D> v) {
  static_assert(0 <= kBytes && kBytes <= 16, "Invalid kBytes");
  VFromD<D> ret;
  uint8_t* HWY_RESTRICT ret8 =
      reinterpret_cast<uint8_t * HWY_RESTRICT>(ret.raw);
  ZeroBytes<kBytes>(ret8);
  CopyBytes<d.MaxBytes() - kBytes>(v.raw, ret8 + kBytes);
  return ret;
}

template <int kBytes, typename T, size_t N>
HWY_API Vec128<T, N> ShiftLeftBytes(Vec128<T, N> v) {
  return ShiftLeftBytes<kBytes>(DFromV<decltype(v)>(), v);
}

// ------------------------------ ShiftLeftLanes

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
template <int kBytes, class D>
HWY_API VFromD<D> ShiftRightBytes(D d, VFromD<D> v) {
  static_assert(0 <= kBytes && kBytes <= 16, "Invalid kBytes");
  VFromD<D> ret;
  const uint8_t* HWY_RESTRICT v8 =
      reinterpret_cast<const uint8_t * HWY_RESTRICT>(v.raw);
  uint8_t* HWY_RESTRICT ret8 =
      reinterpret_cast<uint8_t * HWY_RESTRICT>(ret.raw);
  CopyBytes<d.MaxBytes() - kBytes>(v8 + kBytes, ret8);
  ZeroBytes<kBytes>(ret8 + d.MaxBytes() - kBytes);
  return ret;
}

// ------------------------------ ShiftRightLanes
template <int kLanes, class D>
HWY_API VFromD<D> ShiftRightLanes(D d, VFromD<D> v) {
  const Repartition<uint8_t, decltype(d)> d8;
  constexpr size_t kBytes = kLanes * sizeof(TFromD<D>);
  return BitCast(d, ShiftRightBytes<kBytes>(d8, BitCast(d8, v)));
}

// ================================================== SWIZZLE

template <typename T, size_t N>
HWY_API T GetLane(Vec128<T, N> v) {
  return v.raw[0];
}

template <typename T, size_t N>
HWY_API Vec128<T, N> InsertLane(Vec128<T, N> v, size_t i, T t) {
  v.raw[i] = t;
  return v;
}

template <typename T, size_t N>
HWY_API T ExtractLane(Vec128<T, N> v, size_t i) {
  return v.raw[i];
}

template <typename T, size_t N>
HWY_API Vec128<T, N> DupEven(Vec128<T, N> v) {
  for (size_t i = 0; i < N; i += 2) {
    v.raw[i + 1] = v.raw[i];
  }
  return v;
}

template <typename T, size_t N>
HWY_API Vec128<T, N> DupOdd(Vec128<T, N> v) {
  for (size_t i = 0; i < N; i += 2) {
    v.raw[i] = v.raw[i + 1];
  }
  return v;
}

template <typename T, size_t N>
HWY_API Vec128<T, N> OddEven(Vec128<T, N> odd, Vec128<T, N> even) {
  for (size_t i = 0; i < N; i += 2) {
    odd.raw[i] = even.raw[i];
  }
  return odd;
}

template <typename T, size_t N>
HWY_API Vec128<T, N> OddEvenBlocks(Vec128<T, N> /* odd */, Vec128<T, N> even) {
  return even;
}

// ------------------------------ SwapAdjacentBlocks
template <typename T, size_t N>
HWY_API Vec128<T, N> SwapAdjacentBlocks(Vec128<T, N> v) {
  return v;
}

// ------------------------------ TableLookupLanes

// Returned by SetTableIndices for use by TableLookupLanes.
template <typename T, size_t N>
struct Indices128 {
  MakeSigned<T> raw[N];
};

template <class D, typename TI, size_t N>
HWY_API Indices128<TFromD<D>, N> IndicesFromVec(D d, Vec128<TI, N> vec) {
  static_assert(sizeof(TFromD<D>) == sizeof(TI), "Index/lane size must match");
  Indices128<TFromD<D>, N> ret;
  CopyBytes<d.MaxBytes()>(vec.raw, ret.raw);
  return ret;
}

template <class D, typename TI>
HWY_API Indices128<TFromD<D>, HWY_MAX_LANES_D(D)> SetTableIndices(
    D d, const TI* idx) {
  return IndicesFromVec(d, LoadU(Rebind<TI, D>(), idx));
}

template <typename T, size_t N>
HWY_API Vec128<T, N> TableLookupLanes(Vec128<T, N> v, Indices128<T, N> idx) {
  Vec128<T, N> ret;
  for (size_t i = 0; i < N; ++i) {
    ret.raw[i] = v.raw[idx.raw[i]];
  }
  return ret;
}

template <typename T, size_t N>
HWY_API Vec128<T, N> TwoTablesLookupLanes(Vec128<T, N> a, Vec128<T, N> b,
                                          Indices128<T, N> idx) {
  using TI = MakeSigned<T>;
  Vec128<T, N> ret;
  constexpr TI kVecLaneIdxMask = static_cast<TI>(N - 1);
  for (size_t i = 0; i < N; ++i) {
    const auto src_idx = idx.raw[i];
    const auto masked_src_lane_idx = src_idx & kVecLaneIdxMask;
    ret.raw[i] = (src_idx < static_cast<TI>(N)) ? a.raw[masked_src_lane_idx]
                                                : b.raw[masked_src_lane_idx];
  }
  return ret;
}

// ------------------------------ ReverseBlocks
template <class D>
HWY_API VFromD<D> ReverseBlocks(D /* tag */, VFromD<D> v) {
  return v;  // Single block: no change
}

// ------------------------------ Reverse

template <class D>
HWY_API VFromD<D> Reverse(D d, VFromD<D> v) {
  VFromD<D> ret;
  for (size_t i = 0; i < MaxLanes(d); ++i) {
    ret.raw[i] = v.raw[MaxLanes(d) - 1 - i];
  }
  return ret;
}

// Per-target flag to prevent generic_ops-inl.h defining 8-bit Reverse2/4/8.
#ifdef HWY_NATIVE_REVERSE2_8
#undef HWY_NATIVE_REVERSE2_8
#else
#define HWY_NATIVE_REVERSE2_8
#endif

template <class D>
HWY_API VFromD<D> Reverse2(D d, VFromD<D> v) {
  VFromD<D> ret;
  for (size_t i = 0; i < MaxLanes(d); i += 2) {
    ret.raw[i + 0] = v.raw[i + 1];
    ret.raw[i + 1] = v.raw[i + 0];
  }
  return ret;
}

template <class D>
HWY_API VFromD<D> Reverse4(D d, VFromD<D> v) {
  VFromD<D> ret;
  for (size_t i = 0; i < MaxLanes(d); i += 4) {
    ret.raw[i + 0] = v.raw[i + 3];
    ret.raw[i + 1] = v.raw[i + 2];
    ret.raw[i + 2] = v.raw[i + 1];
    ret.raw[i + 3] = v.raw[i + 0];
  }
  return ret;
}

template <class D>
HWY_API VFromD<D> Reverse8(D d, VFromD<D> v) {
  VFromD<D> ret;
  for (size_t i = 0; i < MaxLanes(d); i += 8) {
    ret.raw[i + 0] = v.raw[i + 7];
    ret.raw[i + 1] = v.raw[i + 6];
    ret.raw[i + 2] = v.raw[i + 5];
    ret.raw[i + 3] = v.raw[i + 4];
    ret.raw[i + 4] = v.raw[i + 3];
    ret.raw[i + 5] = v.raw[i + 2];
    ret.raw[i + 6] = v.raw[i + 1];
    ret.raw[i + 7] = v.raw[i + 0];
  }
  return ret;
}

// ================================================== BLOCKWISE

// ------------------------------ Shuffle*

// Swap 32-bit halves in 64-bit halves.
template <typename T, size_t N>
HWY_API Vec128<T, N> Shuffle2301(Vec128<T, N> v) {
  static_assert(sizeof(T) == 4, "Only for 32-bit");
  static_assert(N == 2 || N == 4, "Does not make sense for N=1");
  return Reverse2(DFromV<decltype(v)>(), v);
}

// Swap 64-bit halves
template <typename T>
HWY_API Vec128<T> Shuffle1032(Vec128<T> v) {
  static_assert(sizeof(T) == 4, "Only for 32-bit");
  Vec128<T> ret;
  ret.raw[3] = v.raw[1];
  ret.raw[2] = v.raw[0];
  ret.raw[1] = v.raw[3];
  ret.raw[0] = v.raw[2];
  return ret;
}
template <typename T>
HWY_API Vec128<T> Shuffle01(Vec128<T> v) {
  static_assert(sizeof(T) == 8, "Only for 64-bit");
  return Reverse2(DFromV<decltype(v)>(), v);
}

// Rotate right 32 bits
template <typename T>
HWY_API Vec128<T> Shuffle0321(Vec128<T> v) {
  Vec128<T> ret;
  ret.raw[3] = v.raw[0];
  ret.raw[2] = v.raw[3];
  ret.raw[1] = v.raw[2];
  ret.raw[0] = v.raw[1];
  return ret;
}

// Rotate left 32 bits
template <typename T>
HWY_API Vec128<T> Shuffle2103(Vec128<T> v) {
  Vec128<T> ret;
  ret.raw[3] = v.raw[2];
  ret.raw[2] = v.raw[1];
  ret.raw[1] = v.raw[0];
  ret.raw[0] = v.raw[3];
  return ret;
}

template <typename T>
HWY_API Vec128<T> Shuffle0123(Vec128<T> v) {
  return Reverse4(DFromV<decltype(v)>(), v);
}

// ------------------------------ Broadcast
template <int kLane, typename T, size_t N>
HWY_API Vec128<T, N> Broadcast(Vec128<T, N> v) {
  for (size_t i = 0; i < N; ++i) {
    v.raw[i] = v.raw[kLane];
  }
  return v;
}

// ------------------------------ TableLookupBytes, TableLookupBytesOr0

template <typename T, size_t N, typename TI, size_t NI>
HWY_API Vec128<TI, NI> TableLookupBytes(Vec128<T, N> v,
                                        Vec128<TI, NI> indices) {
  const uint8_t* HWY_RESTRICT v_bytes =
      reinterpret_cast<const uint8_t * HWY_RESTRICT>(v.raw);
  const uint8_t* HWY_RESTRICT idx_bytes =
      reinterpret_cast<const uint8_t*>(indices.raw);
  Vec128<TI, NI> ret;
  uint8_t* HWY_RESTRICT ret_bytes =
      reinterpret_cast<uint8_t * HWY_RESTRICT>(ret.raw);
  for (size_t i = 0; i < NI * sizeof(TI); ++i) {
    const size_t idx = idx_bytes[i];
    // Avoid out of bounds reads.
    ret_bytes[i] = idx < sizeof(T) * N ? v_bytes[idx] : 0;
  }
  return ret;
}

template <typename T, size_t N, typename TI, size_t NI>
HWY_API Vec128<TI, NI> TableLookupBytesOr0(Vec128<T, N> v,
                                           Vec128<TI, NI> indices) {
  // Same as TableLookupBytes, which already returns 0 if out of bounds.
  return TableLookupBytes(v, indices);
}

// ------------------------------ InterleaveLower/InterleaveUpper

template <typename T, size_t N>
HWY_API Vec128<T, N> InterleaveLower(Vec128<T, N> a, Vec128<T, N> b) {
  Vec128<T, N> ret;
  for (size_t i = 0; i < N / 2; ++i) {
    ret.raw[2 * i + 0] = a.raw[i];
    ret.raw[2 * i + 1] = b.raw[i];
  }
  return ret;
}

// Additional overload for the optional tag.
template <class V>
HWY_API V InterleaveLower(DFromV<V> /* tag */, V a, V b) {
  return InterleaveLower(a, b);
}

template <class D>
HWY_API VFromD<D> InterleaveUpper(D d, VFromD<D> a, VFromD<D> b) {
  const Half<decltype(d)> dh;
  VFromD<D> ret;
  for (size_t i = 0; i < MaxLanes(dh); ++i) {
    ret.raw[2 * i + 0] = a.raw[MaxLanes(dh) + i];
    ret.raw[2 * i + 1] = b.raw[MaxLanes(dh) + i];
  }
  return ret;
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

// ================================================== MASK

template <class D>
HWY_API bool AllFalse(D d, MFromD<D> mask) {
  typename MFromD<D>::Raw or_sum = 0;
  for (size_t i = 0; i < MaxLanes(d); ++i) {
    or_sum |= mask.bits[i];
  }
  return or_sum == 0;
}

template <class D>
HWY_API bool AllTrue(D d, MFromD<D> mask) {
  constexpr uint64_t kAll = LimitsMax<typename MFromD<D>::Raw>();
  uint64_t and_sum = kAll;
  for (size_t i = 0; i < MaxLanes(d); ++i) {
    and_sum &= mask.bits[i];
  }
  return and_sum == kAll;
}

// `p` points to at least 8 readable bytes, not all of which need be valid.
template <class D>
HWY_API MFromD<D> LoadMaskBits(D d, const uint8_t* HWY_RESTRICT bits) {
  MFromD<D> m;
  for (size_t i = 0; i < MaxLanes(d); ++i) {
    const size_t bit = size_t{1} << (i & 7);
    const size_t idx_byte = i >> 3;
    m.bits[i] = MFromD<D>::FromBool((bits[idx_byte] & bit) != 0);
  }
  return m;
}

// `p` points to at least 8 writable bytes.
template <class D>
HWY_API size_t StoreMaskBits(D d, MFromD<D> mask, uint8_t* bits) {
  bits[0] = 0;
  if (MaxLanes(d) > 8) bits[1] = 0;  // MaxLanes(d) <= 16, so max two bytes
  for (size_t i = 0; i < MaxLanes(d); ++i) {
    const size_t bit = size_t{1} << (i & 7);
    const size_t idx_byte = i >> 3;
    if (mask.bits[i]) {
      bits[idx_byte] = static_cast<uint8_t>(bits[idx_byte] | bit);
    }
  }
  return MaxLanes(d) > 8 ? 2 : 1;
}

template <class D>
HWY_API size_t CountTrue(D d, MFromD<D> mask) {
  size_t count = 0;
  for (size_t i = 0; i < MaxLanes(d); ++i) {
    count += mask.bits[i] != 0;
  }
  return count;
}

template <class D>
HWY_API size_t FindKnownFirstTrue(D d, MFromD<D> mask) {
  for (size_t i = 0; i < MaxLanes(d); ++i) {
    if (mask.bits[i] != 0) return i;
  }
  HWY_DASSERT(false);
  return 0;
}

template <class D>
HWY_API intptr_t FindFirstTrue(D d, MFromD<D> mask) {
  for (size_t i = 0; i < MaxLanes(d); ++i) {
    if (mask.bits[i] != 0) return static_cast<intptr_t>(i);
  }
  return intptr_t{-1};
}

template <class D>
HWY_API size_t FindKnownLastTrue(D d, MFromD<D> mask) {
  for (intptr_t i = static_cast<intptr_t>(MaxLanes(d) - 1); i >= 0; i--) {
    if (mask.bits[i] != 0) return static_cast<size_t>(i);
  }
  HWY_DASSERT(false);
  return 0;
}

template <class D>
HWY_API intptr_t FindLastTrue(D d, MFromD<D> mask) {
  for (intptr_t i = static_cast<intptr_t>(MaxLanes(d) - 1); i >= 0; i--) {
    if (mask.bits[i] != 0) return i;
  }
  return intptr_t{-1};
}

// ------------------------------ Compress

template <typename T>
struct CompressIsPartition {
  enum { value = (sizeof(T) != 1) };
};

template <typename T, size_t N>
HWY_API Vec128<T, N> Compress(Vec128<T, N> v, Mask128<T, N> mask) {
  size_t count = 0;
  Vec128<T, N> ret;
  for (size_t i = 0; i < N; ++i) {
    if (mask.bits[i]) {
      ret.raw[count++] = v.raw[i];
    }
  }
  for (size_t i = 0; i < N; ++i) {
    if (!mask.bits[i]) {
      ret.raw[count++] = v.raw[i];
    }
  }
  HWY_DASSERT(count == N);
  return ret;
}

// ------------------------------ Expand

// Could also just allow generic_ops-inl.h to implement these, but use our
// simple implementation below to ensure the test is correct.
#ifdef HWY_NATIVE_EXPAND
#undef HWY_NATIVE_EXPAND
#else
#define HWY_NATIVE_EXPAND
#endif

template <typename T, size_t N>
HWY_API Vec128<T, N> Expand(Vec128<T, N> v, const Mask128<T, N> mask) {
  size_t in_pos = 0;
  Vec128<T, N> ret;
  for (size_t i = 0; i < N; ++i) {
    if (mask.bits[i]) {
      ret.raw[i] = v.raw[in_pos++];
    } else {
      ret.raw[i] = T();  // zero, also works for float16_t
    }
  }
  return ret;
}

// ------------------------------ LoadExpand

template <class D>
HWY_API VFromD<D> LoadExpand(MFromD<D> mask, D d,
                             const TFromD<D>* HWY_RESTRICT unaligned) {
  size_t in_pos = 0;
  VFromD<D> ret;
  for (size_t i = 0; i < Lanes(d); ++i) {
    if (mask.bits[i]) {
      ret.raw[i] = unaligned[in_pos++];
    } else {
      ret.raw[i] = TFromD<D>();  // zero, also works for float16_t
    }
  }
  return ret;
}

// ------------------------------ CompressNot
template <typename T, size_t N>
HWY_API Vec128<T, N> CompressNot(Vec128<T, N> v, Mask128<T, N> mask) {
  size_t count = 0;
  Vec128<T, N> ret;
  for (size_t i = 0; i < N; ++i) {
    if (!mask.bits[i]) {
      ret.raw[count++] = v.raw[i];
    }
  }
  for (size_t i = 0; i < N; ++i) {
    if (mask.bits[i]) {
      ret.raw[count++] = v.raw[i];
    }
  }
  HWY_DASSERT(count == N);
  return ret;
}

// ------------------------------ CompressBlocksNot
HWY_API Vec128<uint64_t> CompressBlocksNot(Vec128<uint64_t> v,
                                           Mask128<uint64_t> /* m */) {
  return v;
}

// ------------------------------ CompressBits
template <typename T, size_t N>
HWY_API Vec128<T, N> CompressBits(Vec128<T, N> v,
                                  const uint8_t* HWY_RESTRICT bits) {
  return Compress(v, LoadMaskBits(Simd<T, N, 0>(), bits));
}

// ------------------------------ CompressStore

// generic_ops-inl defines the 8-bit versions.
template <class D, HWY_IF_NOT_T_SIZE_D(D, 1)>
HWY_API size_t CompressStore(VFromD<D> v, MFromD<D> mask, D d,
                             TFromD<D>* HWY_RESTRICT unaligned) {
  size_t count = 0;
  for (size_t i = 0; i < MaxLanes(d); ++i) {
    if (mask.bits[i]) {
      unaligned[count++] = v.raw[i];
    }
  }
  return count;
}

// ------------------------------ CompressBlendedStore
template <class D, HWY_IF_NOT_T_SIZE_D(D, 1)>
HWY_API size_t CompressBlendedStore(VFromD<D> v, MFromD<D> mask, D d,
                                    TFromD<D>* HWY_RESTRICT unaligned) {
  return CompressStore(v, mask, d, unaligned);
}

// ------------------------------ CompressBitsStore
template <class D, HWY_IF_NOT_T_SIZE_D(D, 1)>
HWY_API size_t CompressBitsStore(VFromD<D> v, const uint8_t* HWY_RESTRICT bits,
                                 D d, TFromD<D>* HWY_RESTRICT unaligned) {
  const MFromD<D> mask = LoadMaskBits(d, bits);
  StoreU(Compress(v, mask), d, unaligned);
  return CountTrue(d, mask);
}

// ------------------------------ ReorderWidenMulAccumulate (MulAdd, ZipLower)

template <class D, HWY_IF_F32_D(D), size_t N, class VBF16>
HWY_API VFromD<D> ReorderWidenMulAccumulate(D df32, VBF16 a, VBF16 b,
                                            const Vec128<float, N> sum0,
                                            Vec128<float, N>& sum1) {
  const Rebind<uint32_t, decltype(df32)> du32;
  using VU32 = VFromD<decltype(du32)>;
  const VU32 odd = Set(du32, 0xFFFF0000u);  // bfloat16 is the upper half of f32
  // Avoid ZipLower/Upper so this also works on big-endian systems.
  const VU32 ae = ShiftLeft<16>(BitCast(du32, a));
  const VU32 ao = And(BitCast(du32, a), odd);
  const VU32 be = ShiftLeft<16>(BitCast(du32, b));
  const VU32 bo = And(BitCast(du32, b), odd);
  sum1 = MulAdd(BitCast(df32, ao), BitCast(df32, bo), sum1);
  return MulAdd(BitCast(df32, ae), BitCast(df32, be), sum0);
}

template <class D, HWY_IF_I32_D(D), size_t N, class VI16>
HWY_API VFromD<D> ReorderWidenMulAccumulate(D d32, VI16 a, VI16 b,
                                            const Vec128<int32_t, N> sum0,
                                            Vec128<int32_t, N>& sum1) {
  using VI32 = VFromD<decltype(d32)>;
  // Manual sign extension requires two shifts for even lanes.
  const VI32 ae = ShiftRight<16>(ShiftLeft<16>(BitCast(d32, a)));
  const VI32 be = ShiftRight<16>(ShiftLeft<16>(BitCast(d32, b)));
  const VI32 ao = ShiftRight<16>(BitCast(d32, a));
  const VI32 bo = ShiftRight<16>(BitCast(d32, b));
  sum1 = Add(Mul(ao, bo), sum1);
  return Add(Mul(ae, be), sum0);
}

// ------------------------------ RearrangeToOddPlusEven
template <class VW>
HWY_API VW RearrangeToOddPlusEven(VW sum0, VW sum1) {
  return Add(sum0, sum1);
}

// ================================================== REDUCTIONS

template <class D, typename T = TFromD<D>>
HWY_API VFromD<D> SumOfLanes(D d, VFromD<D> v) {
  T sum = T{0};
  for (size_t i = 0; i < MaxLanes(d); ++i) {
    sum += v.raw[i];
  }
  return Set(d, sum);
}
template <class D, typename T = TFromD<D>>
HWY_API T ReduceSum(D d, VFromD<D> v) {
  T sum = T{0};
  for (size_t i = 0; i < MaxLanes(d); ++i) {
    sum += v.raw[i];
  }
  return sum;
}
template <class D, typename T = TFromD<D>>
HWY_API VFromD<D> MinOfLanes(D d, VFromD<D> v) {
  T min = HighestValue<T>();
  for (size_t i = 0; i < MaxLanes(d); ++i) {
    min = HWY_MIN(min, v.raw[i]);
  }
  return Set(d, min);
}
template <class D, typename T = TFromD<D>>
HWY_API VFromD<D> MaxOfLanes(D d, VFromD<D> v) {
  T max = LowestValue<T>();
  for (size_t i = 0; i < MaxLanes(d); ++i) {
    max = HWY_MAX(max, v.raw[i]);
  }
  return Set(d, max);
}

// ================================================== OPS WITH DEPENDENCIES

// ------------------------------ MulEven/Odd 64x64 (UpperHalf)

HWY_INLINE Vec128<uint64_t> MulEven(Vec128<uint64_t> a, Vec128<uint64_t> b) {
  alignas(16) uint64_t mul[2];
  mul[0] = Mul128(GetLane(a), GetLane(b), &mul[1]);
  return Load(Full128<uint64_t>(), mul);
}

HWY_INLINE Vec128<uint64_t> MulOdd(Vec128<uint64_t> a, Vec128<uint64_t> b) {
  alignas(16) uint64_t mul[2];
  const Half<Full128<uint64_t>> d2;
  mul[0] =
      Mul128(GetLane(UpperHalf(d2, a)), GetLane(UpperHalf(d2, b)), &mul[1]);
  return Load(Full128<uint64_t>(), mul);
}

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace hwy
HWY_AFTER_NAMESPACE();
