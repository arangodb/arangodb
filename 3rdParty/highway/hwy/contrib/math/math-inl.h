// Copyright 2020 Google LLC
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

// Include guard (still compiled once per target)
#if defined(HIGHWAY_HWY_CONTRIB_MATH_MATH_INL_H_) == \
    defined(HWY_TARGET_TOGGLE)
#ifdef HIGHWAY_HWY_CONTRIB_MATH_MATH_INL_H_
#undef HIGHWAY_HWY_CONTRIB_MATH_MATH_INL_H_
#else
#define HIGHWAY_HWY_CONTRIB_MATH_MATH_INL_H_
#endif

#include "hwy/highway.h"

HWY_BEFORE_NAMESPACE();
namespace hwy {
namespace HWY_NAMESPACE {

/**
 * Highway SIMD version of std::acos(x).
 *
 * Valid Lane Types: float32, float64
 *        Max Error: ULP = 2
 *      Valid Range: [-1, +1]
 * @return arc cosine of 'x'
 */
template <class D, class V>
HWY_INLINE V Acos(const D d, V x);
template <class D, class V>
HWY_NOINLINE V CallAcos(const D d, VecArg<V> x) {
  return Acos(d, x);
}

/**
 * Highway SIMD version of std::acosh(x).
 *
 * Valid Lane Types: float32, float64
 *        Max Error: ULP = 3
 *      Valid Range: float32[1, +FLT_MAX], float64[1, +DBL_MAX]
 * @return hyperbolic arc cosine of 'x'
 */
template <class D, class V>
HWY_INLINE V Acosh(const D d, V x);
template <class D, class V>
HWY_NOINLINE V CallAcosh(const D d, VecArg<V> x) {
  return Acosh(d, x);
}

/**
 * Highway SIMD version of std::asin(x).
 *
 * Valid Lane Types: float32, float64
 *        Max Error: ULP = 2
 *      Valid Range: [-1, +1]
 * @return arc sine of 'x'
 */
template <class D, class V>
HWY_INLINE V Asin(const D d, V x);
template <class D, class V>
HWY_NOINLINE V CallAsin(const D d, VecArg<V> x) {
  return Asin(d, x);
}

/**
 * Highway SIMD version of std::asinh(x).
 *
 * Valid Lane Types: float32, float64
 *        Max Error: ULP = 3
 *      Valid Range: float32[-FLT_MAX, +FLT_MAX], float64[-DBL_MAX, +DBL_MAX]
 * @return hyperbolic arc sine of 'x'
 */
template <class D, class V>
HWY_INLINE V Asinh(const D d, V x);
template <class D, class V>
HWY_NOINLINE V CallAsinh(const D d, VecArg<V> x) {
  return Asinh(d, x);
}

/**
 * Highway SIMD version of std::atan(x).
 *
 * Valid Lane Types: float32, float64
 *        Max Error: ULP = 3
 *      Valid Range: float32[-FLT_MAX, +FLT_MAX], float64[-DBL_MAX, +DBL_MAX]
 * @return arc tangent of 'x'
 */
template <class D, class V>
HWY_INLINE V Atan(const D d, V x);
template <class D, class V>
HWY_NOINLINE V CallAtan(const D d, VecArg<V> x) {
  return Atan(d, x);
}

/**
 * Highway SIMD version of std::atanh(x).
 *
 * Valid Lane Types: float32, float64
 *        Max Error: ULP = 3
 *      Valid Range: (-1, +1)
 * @return hyperbolic arc tangent of 'x'
 */
template <class D, class V>
HWY_INLINE V Atanh(const D d, V x);
template <class D, class V>
HWY_NOINLINE V CallAtanh(const D d, VecArg<V> x) {
  return Atanh(d, x);
}

/**
 * Highway SIMD version of std::cos(x).
 *
 * Valid Lane Types: float32, float64
 *        Max Error: ULP = 3
 *      Valid Range: [-39000, +39000]
 * @return cosine of 'x'
 */
template <class D, class V>
HWY_INLINE V Cos(const D d, V x);
template <class D, class V>
HWY_NOINLINE V CallCos(const D d, VecArg<V> x) {
  return Cos(d, x);
}

/**
 * Highway SIMD version of std::exp(x).
 *
 * Valid Lane Types: float32, float64
 *        Max Error: ULP = 1
 *      Valid Range: float32[-FLT_MAX, +104], float64[-DBL_MAX, +706]
 * @return e^x
 */
template <class D, class V>
HWY_INLINE V Exp(const D d, V x);
template <class D, class V>
HWY_NOINLINE V CallExp(const D d, VecArg<V> x) {
  return Exp(d, x);
}

/**
 * Highway SIMD version of std::expm1(x).
 *
 * Valid Lane Types: float32, float64
 *        Max Error: ULP = 4
 *      Valid Range: float32[-FLT_MAX, +104], float64[-DBL_MAX, +706]
 * @return e^x - 1
 */
template <class D, class V>
HWY_INLINE V Expm1(const D d, V x);
template <class D, class V>
HWY_NOINLINE V CallExpm1(const D d, VecArg<V> x) {
  return Expm1(d, x);
}

/**
 * Highway SIMD version of std::log(x).
 *
 * Valid Lane Types: float32, float64
 *        Max Error: ULP = 4
 *      Valid Range: float32(0, +FLT_MAX], float64(0, +DBL_MAX]
 * @return natural logarithm of 'x'
 */
template <class D, class V>
HWY_INLINE V Log(const D d, V x);
template <class D, class V>
HWY_NOINLINE V CallLog(const D d, VecArg<V> x) {
  return Log(d, x);
}

/**
 * Highway SIMD version of std::log10(x).
 *
 * Valid Lane Types: float32, float64
 *        Max Error: ULP = 2
 *      Valid Range: float32(0, +FLT_MAX], float64(0, +DBL_MAX]
 * @return base 10 logarithm of 'x'
 */
template <class D, class V>
HWY_INLINE V Log10(const D d, V x);
template <class D, class V>
HWY_NOINLINE V CallLog10(const D d, VecArg<V> x) {
  return Log10(d, x);
}

/**
 * Highway SIMD version of std::log1p(x).
 *
 * Valid Lane Types: float32, float64
 *        Max Error: ULP = 2
 *      Valid Range: float32[0, +FLT_MAX], float64[0, +DBL_MAX]
 * @return log(1 + x)
 */
template <class D, class V>
HWY_INLINE V Log1p(const D d, V x);
template <class D, class V>
HWY_NOINLINE V CallLog1p(const D d, VecArg<V> x) {
  return Log1p(d, x);
}

/**
 * Highway SIMD version of std::log2(x).
 *
 * Valid Lane Types: float32, float64
 *        Max Error: ULP = 2
 *      Valid Range: float32(0, +FLT_MAX], float64(0, +DBL_MAX]
 * @return base 2 logarithm of 'x'
 */
template <class D, class V>
HWY_INLINE V Log2(const D d, V x);
template <class D, class V>
HWY_NOINLINE V CallLog2(const D d, VecArg<V> x) {
  return Log2(d, x);
}

/**
 * Highway SIMD version of std::sin(x).
 *
 * Valid Lane Types: float32, float64
 *        Max Error: ULP = 3
 *      Valid Range: [-39000, +39000]
 * @return sine of 'x'
 */
template <class D, class V>
HWY_INLINE V Sin(const D d, V x);
template <class D, class V>
HWY_NOINLINE V CallSin(const D d, VecArg<V> x) {
  return Sin(d, x);
}

/**
 * Highway SIMD version of std::sinh(x).
 *
 * Valid Lane Types: float32, float64
 *        Max Error: ULP = 4
 *      Valid Range: float32[-88.7228, +88.7228], float64[-709, +709]
 * @return hyperbolic sine of 'x'
 */
template <class D, class V>
HWY_INLINE V Sinh(const D d, V x);
template <class D, class V>
HWY_NOINLINE V CallSinh(const D d, VecArg<V> x) {
  return Sinh(d, x);
}

/**
 * Highway SIMD version of std::tanh(x).
 *
 * Valid Lane Types: float32, float64
 *        Max Error: ULP = 4
 *      Valid Range: float32[-FLT_MAX, +FLT_MAX], float64[-DBL_MAX, +DBL_MAX]
 * @return hyperbolic tangent of 'x'
 */
template <class D, class V>
HWY_INLINE V Tanh(const D d, V x);
template <class D, class V>
HWY_NOINLINE V CallTanh(const D d, VecArg<V> x) {
  return Tanh(d, x);
}

////////////////////////////////////////////////////////////////////////////////
// Implementation
////////////////////////////////////////////////////////////////////////////////
namespace impl {

// Estrin's Scheme is a faster method for evaluating large polynomials on
// super scalar architectures. It works by factoring the Horner's Method
// polynomial into power of two sub-trees that can be evaluated in parallel.
// Wikipedia Link: https://en.wikipedia.org/wiki/Estrin%27s_scheme
template <class T>
HWY_INLINE HWY_MAYBE_UNUSED T Estrin(T x, T c0, T c1) {
  return MulAdd(c1, x, c0);
}
template <class T>
HWY_INLINE HWY_MAYBE_UNUSED T Estrin(T x, T c0, T c1, T c2) {
  T x2 = Mul(x, x);
  return MulAdd(x2, c2, MulAdd(c1, x, c0));
}
template <class T>
HWY_INLINE HWY_MAYBE_UNUSED T Estrin(T x, T c0, T c1, T c2, T c3) {
  T x2 = Mul(x, x);
  return MulAdd(x2, MulAdd(c3, x, c2), MulAdd(c1, x, c0));
}
template <class T>
HWY_INLINE HWY_MAYBE_UNUSED T Estrin(T x, T c0, T c1, T c2, T c3, T c4) {
  T x2 = Mul(x, x);
  T x4 = Mul(x2, x2);
  return MulAdd(x4, c4, MulAdd(x2, MulAdd(c3, x, c2), MulAdd(c1, x, c0)));
}
template <class T>
HWY_INLINE HWY_MAYBE_UNUSED T Estrin(T x, T c0, T c1, T c2, T c3, T c4, T c5) {
  T x2 = Mul(x, x);
  T x4 = Mul(x2, x2);
  return MulAdd(x4, MulAdd(c5, x, c4),
                MulAdd(x2, MulAdd(c3, x, c2), MulAdd(c1, x, c0)));
}
template <class T>
HWY_INLINE HWY_MAYBE_UNUSED T Estrin(T x, T c0, T c1, T c2, T c3, T c4, T c5,
                                     T c6) {
  T x2 = Mul(x, x);
  T x4 = Mul(x2, x2);
  return MulAdd(x4, MulAdd(x2, c6, MulAdd(c5, x, c4)),
                MulAdd(x2, MulAdd(c3, x, c2), MulAdd(c1, x, c0)));
}
template <class T>
HWY_INLINE HWY_MAYBE_UNUSED T Estrin(T x, T c0, T c1, T c2, T c3, T c4, T c5,
                                     T c6, T c7) {
  T x2 = Mul(x, x);
  T x4 = Mul(x2, x2);
  return MulAdd(x4, MulAdd(x2, MulAdd(c7, x, c6), MulAdd(c5, x, c4)),
                MulAdd(x2, MulAdd(c3, x, c2), MulAdd(c1, x, c0)));
}
template <class T>
HWY_INLINE HWY_MAYBE_UNUSED T Estrin(T x, T c0, T c1, T c2, T c3, T c4, T c5,
                                     T c6, T c7, T c8) {
  T x2 = Mul(x, x);
  T x4 = Mul(x2, x2);
  T x8 = Mul(x4, x4);
  return MulAdd(x8, c8,
                MulAdd(x4, MulAdd(x2, MulAdd(c7, x, c6), MulAdd(c5, x, c4)),
                       MulAdd(x2, MulAdd(c3, x, c2), MulAdd(c1, x, c0))));
}
template <class T>
HWY_INLINE HWY_MAYBE_UNUSED T Estrin(T x, T c0, T c1, T c2, T c3, T c4, T c5,
                                     T c6, T c7, T c8, T c9) {
  T x2 = Mul(x, x);
  T x4 = Mul(x2, x2);
  T x8 = Mul(x4, x4);
  return MulAdd(x8, MulAdd(c9, x, c8),
                MulAdd(x4, MulAdd(x2, MulAdd(c7, x, c6), MulAdd(c5, x, c4)),
                       MulAdd(x2, MulAdd(c3, x, c2), MulAdd(c1, x, c0))));
}
template <class T>
HWY_INLINE HWY_MAYBE_UNUSED T Estrin(T x, T c0, T c1, T c2, T c3, T c4, T c5,
                                     T c6, T c7, T c8, T c9, T c10) {
  T x2 = Mul(x, x);
  T x4 = Mul(x2, x2);
  T x8 = Mul(x4, x4);
  return MulAdd(x8, MulAdd(x2, c10, MulAdd(c9, x, c8)),
                MulAdd(x4, MulAdd(x2, MulAdd(c7, x, c6), MulAdd(c5, x, c4)),
                       MulAdd(x2, MulAdd(c3, x, c2), MulAdd(c1, x, c0))));
}
template <class T>
HWY_INLINE HWY_MAYBE_UNUSED T Estrin(T x, T c0, T c1, T c2, T c3, T c4, T c5,
                                     T c6, T c7, T c8, T c9, T c10, T c11) {
  T x2 = Mul(x, x);
  T x4 = Mul(x2, x2);
  T x8 = Mul(x4, x4);
  return MulAdd(x8, MulAdd(x2, MulAdd(c11, x, c10), MulAdd(c9, x, c8)),
                MulAdd(x4, MulAdd(x2, MulAdd(c7, x, c6), MulAdd(c5, x, c4)),
                       MulAdd(x2, MulAdd(c3, x, c2), MulAdd(c1, x, c0))));
}
template <class T>
HWY_INLINE HWY_MAYBE_UNUSED T Estrin(T x, T c0, T c1, T c2, T c3, T c4, T c5,
                                     T c6, T c7, T c8, T c9, T c10, T c11,
                                     T c12) {
  T x2 = Mul(x, x);
  T x4 = Mul(x2, x2);
  T x8 = Mul(x4, x4);
  return MulAdd(
      x8, MulAdd(x4, c12, MulAdd(x2, MulAdd(c11, x, c10), MulAdd(c9, x, c8))),
      MulAdd(x4, MulAdd(x2, MulAdd(c7, x, c6), MulAdd(c5, x, c4)),
             MulAdd(x2, MulAdd(c3, x, c2), MulAdd(c1, x, c0))));
}
template <class T>
HWY_INLINE HWY_MAYBE_UNUSED T Estrin(T x, T c0, T c1, T c2, T c3, T c4, T c5,
                                     T c6, T c7, T c8, T c9, T c10, T c11,
                                     T c12, T c13) {
  T x2 = Mul(x, x);
  T x4 = Mul(x2, x2);
  T x8 = Mul(x4, x4);
  return MulAdd(x8,
                MulAdd(x4, MulAdd(c13, x, c12),
                       MulAdd(x2, MulAdd(c11, x, c10), MulAdd(c9, x, c8))),
                MulAdd(x4, MulAdd(x2, MulAdd(c7, x, c6), MulAdd(c5, x, c4)),
                       MulAdd(x2, MulAdd(c3, x, c2), MulAdd(c1, x, c0))));
}
template <class T>
HWY_INLINE HWY_MAYBE_UNUSED T Estrin(T x, T c0, T c1, T c2, T c3, T c4, T c5,
                                     T c6, T c7, T c8, T c9, T c10, T c11,
                                     T c12, T c13, T c14) {
  T x2 = Mul(x, x);
  T x4 = Mul(x2, x2);
  T x8 = Mul(x4, x4);
  return MulAdd(x8,
                MulAdd(x4, MulAdd(x2, c14, MulAdd(c13, x, c12)),
                       MulAdd(x2, MulAdd(c11, x, c10), MulAdd(c9, x, c8))),
                MulAdd(x4, MulAdd(x2, MulAdd(c7, x, c6), MulAdd(c5, x, c4)),
                       MulAdd(x2, MulAdd(c3, x, c2), MulAdd(c1, x, c0))));
}
template <class T>
HWY_INLINE HWY_MAYBE_UNUSED T Estrin(T x, T c0, T c1, T c2, T c3, T c4, T c5,
                                     T c6, T c7, T c8, T c9, T c10, T c11,
                                     T c12, T c13, T c14, T c15) {
  T x2 = Mul(x, x);
  T x4 = Mul(x2, x2);
  T x8 = Mul(x4, x4);
  return MulAdd(x8,
                MulAdd(x4, MulAdd(x2, MulAdd(c15, x, c14), MulAdd(c13, x, c12)),
                       MulAdd(x2, MulAdd(c11, x, c10), MulAdd(c9, x, c8))),
                MulAdd(x4, MulAdd(x2, MulAdd(c7, x, c6), MulAdd(c5, x, c4)),
                       MulAdd(x2, MulAdd(c3, x, c2), MulAdd(c1, x, c0))));
}
template <class T>
HWY_INLINE HWY_MAYBE_UNUSED T Estrin(T x, T c0, T c1, T c2, T c3, T c4, T c5,
                                     T c6, T c7, T c8, T c9, T c10, T c11,
                                     T c12, T c13, T c14, T c15, T c16) {
  T x2 = Mul(x, x);
  T x4 = Mul(x2, x2);
  T x8 = Mul(x4, x4);
  T x16 = Mul(x8, x8);
  return MulAdd(
      x16, c16,
      MulAdd(x8,
             MulAdd(x4, MulAdd(x2, MulAdd(c15, x, c14), MulAdd(c13, x, c12)),
                    MulAdd(x2, MulAdd(c11, x, c10), MulAdd(c9, x, c8))),
             MulAdd(x4, MulAdd(x2, MulAdd(c7, x, c6), MulAdd(c5, x, c4)),
                    MulAdd(x2, MulAdd(c3, x, c2), MulAdd(c1, x, c0)))));
}
template <class T>
HWY_INLINE HWY_MAYBE_UNUSED T Estrin(T x, T c0, T c1, T c2, T c3, T c4, T c5,
                                     T c6, T c7, T c8, T c9, T c10, T c11,
                                     T c12, T c13, T c14, T c15, T c16, T c17) {
  T x2 = Mul(x, x);
  T x4 = Mul(x2, x2);
  T x8 = Mul(x4, x4);
  T x16 = Mul(x8, x8);
  return MulAdd(
      x16, MulAdd(c17, x, c16),
      MulAdd(x8,
             MulAdd(x4, MulAdd(x2, MulAdd(c15, x, c14), MulAdd(c13, x, c12)),
                    MulAdd(x2, MulAdd(c11, x, c10), MulAdd(c9, x, c8))),
             MulAdd(x4, MulAdd(x2, MulAdd(c7, x, c6), MulAdd(c5, x, c4)),
                    MulAdd(x2, MulAdd(c3, x, c2), MulAdd(c1, x, c0)))));
}
template <class T>
HWY_INLINE HWY_MAYBE_UNUSED T Estrin(T x, T c0, T c1, T c2, T c3, T c4, T c5,
                                     T c6, T c7, T c8, T c9, T c10, T c11,
                                     T c12, T c13, T c14, T c15, T c16, T c17,
                                     T c18) {
  T x2 = Mul(x, x);
  T x4 = Mul(x2, x2);
  T x8 = Mul(x4, x4);
  T x16 = Mul(x8, x8);
  return MulAdd(
      x16, MulAdd(x2, c18, MulAdd(c17, x, c16)),
      MulAdd(x8,
             MulAdd(x4, MulAdd(x2, MulAdd(c15, x, c14), MulAdd(c13, x, c12)),
                    MulAdd(x2, MulAdd(c11, x, c10), MulAdd(c9, x, c8))),
             MulAdd(x4, MulAdd(x2, MulAdd(c7, x, c6), MulAdd(c5, x, c4)),
                    MulAdd(x2, MulAdd(c3, x, c2), MulAdd(c1, x, c0)))));
}

template <class FloatOrDouble>
struct AsinImpl {};
template <class FloatOrDouble>
struct AtanImpl {};
template <class FloatOrDouble>
struct CosSinImpl {};
template <class FloatOrDouble>
struct ExpImpl {};
template <class FloatOrDouble>
struct LogImpl {};

template <>
struct AsinImpl<float> {
  // Polynomial approximation for asin(x) over the range [0, 0.5).
  template <class D, class V>
  HWY_INLINE V AsinPoly(D d, V x2, V /*x*/) {
    const auto k0 = Set(d, +0.1666677296f);
    const auto k1 = Set(d, +0.07495029271f);
    const auto k2 = Set(d, +0.04547423869f);
    const auto k3 = Set(d, +0.02424046025f);
    const auto k4 = Set(d, +0.04197454825f);

    return Estrin(x2, k0, k1, k2, k3, k4);
  }
};

#if HWY_HAVE_FLOAT64 && HWY_HAVE_INTEGER64

template <>
struct AsinImpl<double> {
  // Polynomial approximation for asin(x) over the range [0, 0.5).
  template <class D, class V>
  HWY_INLINE V AsinPoly(D d, V x2, V /*x*/) {
    const auto k0 = Set(d, +0.1666666666666497543);
    const auto k1 = Set(d, +0.07500000000378581611);
    const auto k2 = Set(d, +0.04464285681377102438);
    const auto k3 = Set(d, +0.03038195928038132237);
    const auto k4 = Set(d, +0.02237176181932048341);
    const auto k5 = Set(d, +0.01735956991223614604);
    const auto k6 = Set(d, +0.01388715184501609218);
    const auto k7 = Set(d, +0.01215360525577377331);
    const auto k8 = Set(d, +0.006606077476277170610);
    const auto k9 = Set(d, +0.01929045477267910674);
    const auto k10 = Set(d, -0.01581918243329996643);
    const auto k11 = Set(d, +0.03161587650653934628);

    return Estrin(x2, k0, k1, k2, k3, k4, k5, k6, k7, k8, k9, k10, k11);
  }
};

#endif

template <>
struct AtanImpl<float> {
  // Polynomial approximation for atan(x) over the range [0, 1.0).
  template <class D, class V>
  HWY_INLINE V AtanPoly(D d, V x) {
    const auto k0 = Set(d, -0.333331018686294555664062f);
    const auto k1 = Set(d, +0.199926957488059997558594f);
    const auto k2 = Set(d, -0.142027363181114196777344f);
    const auto k3 = Set(d, +0.106347933411598205566406f);
    const auto k4 = Set(d, -0.0748900920152664184570312f);
    const auto k5 = Set(d, +0.0425049886107444763183594f);
    const auto k6 = Set(d, -0.0159569028764963150024414f);
    const auto k7 = Set(d, +0.00282363896258175373077393f);

    const auto y = Mul(x, x);
    return MulAdd(Estrin(y, k0, k1, k2, k3, k4, k5, k6, k7), Mul(y, x), x);
  }
};

#if HWY_HAVE_FLOAT64 && HWY_HAVE_INTEGER64

template <>
struct AtanImpl<double> {
  // Polynomial approximation for atan(x) over the range [0, 1.0).
  template <class D, class V>
  HWY_INLINE V AtanPoly(D d, V x) {
    const auto k0 = Set(d, -0.333333333333311110369124);
    const auto k1 = Set(d, +0.199999999996591265594148);
    const auto k2 = Set(d, -0.14285714266771329383765);
    const auto k3 = Set(d, +0.111111105648261418443745);
    const auto k4 = Set(d, -0.090908995008245008229153);
    const auto k5 = Set(d, +0.0769219538311769618355029);
    const auto k6 = Set(d, -0.0666573579361080525984562);
    const auto k7 = Set(d, +0.0587666392926673580854313);
    const auto k8 = Set(d, -0.0523674852303482457616113);
    const auto k9 = Set(d, +0.0466667150077840625632675);
    const auto k10 = Set(d, -0.0407629191276836500001934);
    const auto k11 = Set(d, +0.0337852580001353069993897);
    const auto k12 = Set(d, -0.0254517624932312641616861);
    const auto k13 = Set(d, +0.016599329773529201970117);
    const auto k14 = Set(d, -0.00889896195887655491740809);
    const auto k15 = Set(d, +0.00370026744188713119232403);
    const auto k16 = Set(d, -0.00110611831486672482563471);
    const auto k17 = Set(d, +0.000209850076645816976906797);
    const auto k18 = Set(d, -1.88796008463073496563746e-5);

    const auto y = Mul(x, x);
    return MulAdd(Estrin(y, k0, k1, k2, k3, k4, k5, k6, k7, k8, k9, k10, k11,
                         k12, k13, k14, k15, k16, k17, k18),
                  Mul(y, x), x);
  }
};

#endif

template <>
struct CosSinImpl<float> {
  // Rounds float toward zero and returns as int32_t.
  template <class D, class V>
  HWY_INLINE Vec<Rebind<int32_t, D>> ToInt32(D /*unused*/, V x) {
    return ConvertTo(Rebind<int32_t, D>(), x);
  }

  template <class D, class V>
  HWY_INLINE V Poly(D d, V x) {
    const auto k0 = Set(d, -1.66666597127914428710938e-1f);
    const auto k1 = Set(d, +8.33307858556509017944336e-3f);
    const auto k2 = Set(d, -1.981069071916863322258e-4f);
    const auto k3 = Set(d, +2.6083159809786593541503e-6f);

    const auto y = Mul(x, x);
    return MulAdd(Estrin(y, k0, k1, k2, k3), Mul(y, x), x);
  }

  template <class D, class V, class VI32>
  HWY_INLINE V CosReduce(D d, V x, VI32 q) {
    // kHalfPiPart0f + kHalfPiPart1f + kHalfPiPart2f + kHalfPiPart3f ~= -pi/2
    const V kHalfPiPart0f = Set(d, -0.5f * 3.140625f);
    const V kHalfPiPart1f = Set(d, -0.5f * 0.0009670257568359375f);
    const V kHalfPiPart2f = Set(d, -0.5f * 6.2771141529083251953e-7f);
    const V kHalfPiPart3f = Set(d, -0.5f * 1.2154201256553420762e-10f);

    // Extended precision modular arithmetic.
    const V qf = ConvertTo(d, q);
    x = MulAdd(qf, kHalfPiPart0f, x);
    x = MulAdd(qf, kHalfPiPart1f, x);
    x = MulAdd(qf, kHalfPiPart2f, x);
    x = MulAdd(qf, kHalfPiPart3f, x);
    return x;
  }

  template <class D, class V, class VI32>
  HWY_INLINE V SinReduce(D d, V x, VI32 q) {
    // kPiPart0f + kPiPart1f + kPiPart2f + kPiPart3f ~= -pi
    const V kPiPart0f = Set(d, -3.140625f);
    const V kPiPart1f = Set(d, -0.0009670257568359375f);
    const V kPiPart2f = Set(d, -6.2771141529083251953e-7f);
    const V kPiPart3f = Set(d, -1.2154201256553420762e-10f);

    // Extended precision modular arithmetic.
    const V qf = ConvertTo(d, q);
    x = MulAdd(qf, kPiPart0f, x);
    x = MulAdd(qf, kPiPart1f, x);
    x = MulAdd(qf, kPiPart2f, x);
    x = MulAdd(qf, kPiPart3f, x);
    return x;
  }

  // (q & 2) == 0 ? -0.0 : +0.0
  template <class D, class VI32>
  HWY_INLINE Vec<Rebind<float, D>> CosSignFromQuadrant(D d, VI32 q) {
    const VI32 kTwo = Set(Rebind<int32_t, D>(), 2);
    return BitCast(d, ShiftLeft<30>(AndNot(q, kTwo)));
  }

  // ((q & 1) ? -0.0 : +0.0)
  template <class D, class VI32>
  HWY_INLINE Vec<Rebind<float, D>> SinSignFromQuadrant(D d, VI32 q) {
    const VI32 kOne = Set(Rebind<int32_t, D>(), 1);
    return BitCast(d, ShiftLeft<31>(And(q, kOne)));
  }
};

#if HWY_HAVE_FLOAT64 && HWY_HAVE_INTEGER64

template <>
struct CosSinImpl<double> {
  // Rounds double toward zero and returns as int32_t.
  template <class D, class V>
  HWY_INLINE Vec<Rebind<int32_t, D>> ToInt32(D /*unused*/, V x) {
    return DemoteTo(Rebind<int32_t, D>(), x);
  }

  template <class D, class V>
  HWY_INLINE V Poly(D d, V x) {
    const auto k0 = Set(d, -0.166666666666666657414808);
    const auto k1 = Set(d, +0.00833333333333332974823815);
    const auto k2 = Set(d, -0.000198412698412696162806809);
    const auto k3 = Set(d, +2.75573192239198747630416e-6);
    const auto k4 = Set(d, -2.50521083763502045810755e-8);
    const auto k5 = Set(d, +1.60590430605664501629054e-10);
    const auto k6 = Set(d, -7.64712219118158833288484e-13);
    const auto k7 = Set(d, +2.81009972710863200091251e-15);
    const auto k8 = Set(d, -7.97255955009037868891952e-18);

    const auto y = Mul(x, x);
    return MulAdd(Estrin(y, k0, k1, k2, k3, k4, k5, k6, k7, k8), Mul(y, x), x);
  }

  template <class D, class V, class VI32>
  HWY_INLINE V CosReduce(D d, V x, VI32 q) {
    // kHalfPiPart0d + kHalfPiPart1d + kHalfPiPart2d + kHalfPiPart3d ~= -pi/2
    const V kHalfPiPart0d = Set(d, -0.5 * 3.1415926218032836914);
    const V kHalfPiPart1d = Set(d, -0.5 * 3.1786509424591713469e-8);
    const V kHalfPiPart2d = Set(d, -0.5 * 1.2246467864107188502e-16);
    const V kHalfPiPart3d = Set(d, -0.5 * 1.2736634327021899816e-24);

    // Extended precision modular arithmetic.
    const V qf = PromoteTo(d, q);
    x = MulAdd(qf, kHalfPiPart0d, x);
    x = MulAdd(qf, kHalfPiPart1d, x);
    x = MulAdd(qf, kHalfPiPart2d, x);
    x = MulAdd(qf, kHalfPiPart3d, x);
    return x;
  }

  template <class D, class V, class VI32>
  HWY_INLINE V SinReduce(D d, V x, VI32 q) {
    // kPiPart0d + kPiPart1d + kPiPart2d + kPiPart3d ~= -pi
    const V kPiPart0d = Set(d, -3.1415926218032836914);
    const V kPiPart1d = Set(d, -3.1786509424591713469e-8);
    const V kPiPart2d = Set(d, -1.2246467864107188502e-16);
    const V kPiPart3d = Set(d, -1.2736634327021899816e-24);

    // Extended precision modular arithmetic.
    const V qf = PromoteTo(d, q);
    x = MulAdd(qf, kPiPart0d, x);
    x = MulAdd(qf, kPiPart1d, x);
    x = MulAdd(qf, kPiPart2d, x);
    x = MulAdd(qf, kPiPart3d, x);
    return x;
  }

  // (q & 2) == 0 ? -0.0 : +0.0
  template <class D, class VI32>
  HWY_INLINE Vec<Rebind<double, D>> CosSignFromQuadrant(D d, VI32 q) {
    const VI32 kTwo = Set(Rebind<int32_t, D>(), 2);
    return BitCast(
        d, ShiftLeft<62>(PromoteTo(Rebind<int64_t, D>(), AndNot(q, kTwo))));
  }

  // ((q & 1) ? -0.0 : +0.0)
  template <class D, class VI32>
  HWY_INLINE Vec<Rebind<double, D>> SinSignFromQuadrant(D d, VI32 q) {
    const VI32 kOne = Set(Rebind<int32_t, D>(), 1);
    return BitCast(
        d, ShiftLeft<63>(PromoteTo(Rebind<int64_t, D>(), And(q, kOne))));
  }
};

#endif

template <>
struct ExpImpl<float> {
  // Rounds float toward zero and returns as int32_t.
  template <class D, class V>
  HWY_INLINE Vec<Rebind<int32_t, D>> ToInt32(D /*unused*/, V x) {
    return ConvertTo(Rebind<int32_t, D>(), x);
  }

  template <class D, class V>
  HWY_INLINE V ExpPoly(D d, V x) {
    const auto k0 = Set(d, +0.5f);
    const auto k1 = Set(d, +0.166666671633720397949219f);
    const auto k2 = Set(d, +0.0416664853692054748535156f);
    const auto k3 = Set(d, +0.00833336077630519866943359f);
    const auto k4 = Set(d, +0.00139304355252534151077271f);
    const auto k5 = Set(d, +0.000198527617612853646278381f);

    return MulAdd(Estrin(x, k0, k1, k2, k3, k4, k5), Mul(x, x), x);
  }

  // Computes 2^x, where x is an integer.
  template <class D, class VI32>
  HWY_INLINE Vec<D> Pow2I(D d, VI32 x) {
    const Rebind<int32_t, D> di32;
    const VI32 kOffset = Set(di32, 0x7F);
    return BitCast(d, ShiftLeft<23>(Add(x, kOffset)));
  }

  // Sets the exponent of 'x' to 2^e.
  template <class D, class V, class VI32>
  HWY_INLINE V LoadExpShortRange(D d, V x, VI32 e) {
    const VI32 y = ShiftRight<1>(e);
    return Mul(Mul(x, Pow2I(d, y)), Pow2I(d, Sub(e, y)));
  }

  template <class D, class V, class VI32>
  HWY_INLINE V ExpReduce(D d, V x, VI32 q) {
    // kLn2Part0f + kLn2Part1f ~= -ln(2)
    const V kLn2Part0f = Set(d, -0.693145751953125f);
    const V kLn2Part1f = Set(d, -1.428606765330187045e-6f);

    // Extended precision modular arithmetic.
    const V qf = ConvertTo(d, q);
    x = MulAdd(qf, kLn2Part0f, x);
    x = MulAdd(qf, kLn2Part1f, x);
    return x;
  }
};

template <>
struct LogImpl<float> {
  template <class D, class V>
  HWY_INLINE Vec<Rebind<int32_t, D>> Log2p1NoSubnormal(D /*d*/, V x) {
    const Rebind<int32_t, D> di32;
    const Rebind<uint32_t, D> du32;
    const auto kBias = Set(di32, 0x7F);
    return Sub(BitCast(di32, ShiftRight<23>(BitCast(du32, x))), kBias);
  }

  // Approximates Log(x) over the range [sqrt(2) / 2, sqrt(2)].
  template <class D, class V>
  HWY_INLINE V LogPoly(D d, V x) {
    const V k0 = Set(d, 0.66666662693f);
    const V k1 = Set(d, 0.40000972152f);
    const V k2 = Set(d, 0.28498786688f);
    const V k3 = Set(d, 0.24279078841f);

    const V x2 = Mul(x, x);
    const V x4 = Mul(x2, x2);
    return MulAdd(MulAdd(k2, x4, k0), x2, Mul(MulAdd(k3, x4, k1), x4));
  }
};

#if HWY_HAVE_FLOAT64 && HWY_HAVE_INTEGER64
template <>
struct ExpImpl<double> {
  // Rounds double toward zero and returns as int32_t.
  template <class D, class V>
  HWY_INLINE Vec<Rebind<int32_t, D>> ToInt32(D /*unused*/, V x) {
    return DemoteTo(Rebind<int32_t, D>(), x);
  }

  template <class D, class V>
  HWY_INLINE V ExpPoly(D d, V x) {
    const auto k0 = Set(d, +0.5);
    const auto k1 = Set(d, +0.166666666666666851703837);
    const auto k2 = Set(d, +0.0416666666666665047591422);
    const auto k3 = Set(d, +0.00833333333331652721664984);
    const auto k4 = Set(d, +0.00138888888889774492207962);
    const auto k5 = Set(d, +0.000198412698960509205564975);
    const auto k6 = Set(d, +2.4801587159235472998791e-5);
    const auto k7 = Set(d, +2.75572362911928827629423e-6);
    const auto k8 = Set(d, +2.75573911234900471893338e-7);
    const auto k9 = Set(d, +2.51112930892876518610661e-8);
    const auto k10 = Set(d, +2.08860621107283687536341e-9);

    return MulAdd(Estrin(x, k0, k1, k2, k3, k4, k5, k6, k7, k8, k9, k10),
                  Mul(x, x), x);
  }

  // Computes 2^x, where x is an integer.
  template <class D, class VI32>
  HWY_INLINE Vec<D> Pow2I(D d, VI32 x) {
    const Rebind<int32_t, D> di32;
    const Rebind<int64_t, D> di64;
    const VI32 kOffset = Set(di32, 0x3FF);
    return BitCast(d, ShiftLeft<52>(PromoteTo(di64, Add(x, kOffset))));
  }

  // Sets the exponent of 'x' to 2^e.
  template <class D, class V, class VI32>
  HWY_INLINE V LoadExpShortRange(D d, V x, VI32 e) {
    const VI32 y = ShiftRight<1>(e);
    return Mul(Mul(x, Pow2I(d, y)), Pow2I(d, Sub(e, y)));
  }

  template <class D, class V, class VI32>
  HWY_INLINE V ExpReduce(D d, V x, VI32 q) {
    // kLn2Part0d + kLn2Part1d ~= -ln(2)
    const V kLn2Part0d = Set(d, -0.6931471805596629565116018);
    const V kLn2Part1d = Set(d, -0.28235290563031577122588448175e-12);

    // Extended precision modular arithmetic.
    const V qf = PromoteTo(d, q);
    x = MulAdd(qf, kLn2Part0d, x);
    x = MulAdd(qf, kLn2Part1d, x);
    return x;
  }
};

template <>
struct LogImpl<double> {
  template <class D, class V>
  HWY_INLINE Vec<Rebind<int64_t, D>> Log2p1NoSubnormal(D /*d*/, V x) {
    const Rebind<int64_t, D> di64;
    const Rebind<uint64_t, D> du64;
    return Sub(BitCast(di64, ShiftRight<52>(BitCast(du64, x))),
               Set(di64, 0x3FF));
  }

  // Approximates Log(x) over the range [sqrt(2) / 2, sqrt(2)].
  template <class D, class V>
  HWY_INLINE V LogPoly(D d, V x) {
    const V k0 = Set(d, 0.6666666666666735130);
    const V k1 = Set(d, 0.3999999999940941908);
    const V k2 = Set(d, 0.2857142874366239149);
    const V k3 = Set(d, 0.2222219843214978396);
    const V k4 = Set(d, 0.1818357216161805012);
    const V k5 = Set(d, 0.1531383769920937332);
    const V k6 = Set(d, 0.1479819860511658591);

    const V x2 = Mul(x, x);
    const V x4 = Mul(x2, x2);
    return MulAdd(MulAdd(MulAdd(MulAdd(k6, x4, k4), x4, k2), x4, k0), x2,
                  (Mul(MulAdd(MulAdd(k5, x4, k3), x4, k1), x4)));
  }
};

#endif

template <class D, class V, bool kAllowSubnormals = true>
HWY_INLINE V Log(const D d, V x) {
  // http://git.musl-libc.org/cgit/musl/tree/src/math/log.c for more info.
  using T = TFromD<D>;
  impl::LogImpl<T> impl;

  constexpr bool kIsF32 = (sizeof(T) == 4);

  // Float Constants
  const V kLn2Hi = Set(d, kIsF32 ? static_cast<T>(0.69313812256f)
                                 : static_cast<T>(0.693147180369123816490));
  const V kLn2Lo = Set(d, kIsF32 ? static_cast<T>(9.0580006145e-6f)
                                 : static_cast<T>(1.90821492927058770002e-10));
  const V kOne = Set(d, static_cast<T>(+1.0));
  const V kMinNormal = Set(d, kIsF32 ? static_cast<T>(1.175494351e-38f)
                                     : static_cast<T>(2.2250738585072014e-308));
  const V kScale = Set(d, kIsF32 ? static_cast<T>(3.355443200e+7f)
                                 : static_cast<T>(1.8014398509481984e+16));

  // Integer Constants
  using TI = MakeSigned<T>;
  const Rebind<TI, D> di;
  using VI = decltype(Zero(di));
  const VI kLowerBits = Set(di, kIsF32 ? static_cast<TI>(0x00000000L)
                                       : static_cast<TI>(0xFFFFFFFFLL));
  const VI kMagic = Set(di, kIsF32 ? static_cast<TI>(0x3F3504F3L)
                                   : static_cast<TI>(0x3FE6A09E00000000LL));
  const VI kExpMask = Set(di, kIsF32 ? static_cast<TI>(0x3F800000L)
                                     : static_cast<TI>(0x3FF0000000000000LL));
  const VI kExpScale =
      Set(di, kIsF32 ? static_cast<TI>(-25) : static_cast<TI>(-54));
  const VI kManMask = Set(di, kIsF32 ? static_cast<TI>(0x7FFFFFL)
                                     : static_cast<TI>(0xFFFFF00000000LL));

  // Scale up 'x' so that it is no longer denormalized.
  VI exp_bits;
  V exp;
  if (kAllowSubnormals == true) {
    const auto is_denormal = Lt(x, kMinNormal);
    x = IfThenElse(is_denormal, Mul(x, kScale), x);

    // Compute the new exponent.
    exp_bits = Add(BitCast(di, x), Sub(kExpMask, kMagic));
    const VI exp_scale =
        BitCast(di, IfThenElseZero(is_denormal, BitCast(d, kExpScale)));
    exp = ConvertTo(
        d, Add(exp_scale, impl.Log2p1NoSubnormal(d, BitCast(d, exp_bits))));
  } else {
    // Compute the new exponent.
    exp_bits = Add(BitCast(di, x), Sub(kExpMask, kMagic));
    exp = ConvertTo(d, impl.Log2p1NoSubnormal(d, BitCast(d, exp_bits)));
  }

  // Renormalize.
  const V y = Or(And(x, BitCast(d, kLowerBits)),
                 BitCast(d, Add(And(exp_bits, kManMask), kMagic)));

  // Approximate and reconstruct.
  const V ym1 = Sub(y, kOne);
  const V z = Div(ym1, Add(y, kOne));

  return MulSub(
      exp, kLn2Hi,
      Sub(MulSub(z, Sub(ym1, impl.LogPoly(d, z)), Mul(exp, kLn2Lo)), ym1));
}

}  // namespace impl

template <class D, class V>
HWY_INLINE V Acos(const D d, V x) {
  using T = TFromD<D>;

  const V kZero = Zero(d);
  const V kHalf = Set(d, static_cast<T>(+0.5));
  const V kPi = Set(d, static_cast<T>(+3.14159265358979323846264));
  const V kPiOverTwo = Set(d, static_cast<T>(+1.57079632679489661923132169));

  const V sign_x = And(SignBit(d), x);
  const V abs_x = Xor(x, sign_x);
  const auto mask = Lt(abs_x, kHalf);
  const V yy =
      IfThenElse(mask, Mul(abs_x, abs_x), NegMulAdd(abs_x, kHalf, kHalf));
  const V y = IfThenElse(mask, abs_x, Sqrt(yy));

  impl::AsinImpl<T> impl;
  const V t = Mul(impl.AsinPoly(d, yy, y), Mul(y, yy));

  const V t_plus_y = Add(t, y);
  const V z =
      IfThenElse(mask, Sub(kPiOverTwo, Add(Xor(y, sign_x), Xor(t, sign_x))),
                 Add(t_plus_y, t_plus_y));
  return IfThenElse(Or(mask, Ge(x, kZero)), z, Sub(kPi, z));
}

template <class D, class V>
HWY_INLINE V Acosh(const D d, V x) {
  using T = TFromD<D>;

  const V kLarge = Set(d, static_cast<T>(268435456.0));
  const V kLog2 = Set(d, static_cast<T>(0.693147180559945286227));
  const V kOne = Set(d, static_cast<T>(+1.0));
  const V kTwo = Set(d, static_cast<T>(+2.0));

  const auto is_x_large = Gt(x, kLarge);
  const auto is_x_gt_2 = Gt(x, kTwo);

  const V x_minus_1 = Sub(x, kOne);
  const V y0 = MulSub(kTwo, x, Div(kOne, Add(Sqrt(MulSub(x, x, kOne)), x)));
  const V y1 =
      Add(Sqrt(MulAdd(x_minus_1, kTwo, Mul(x_minus_1, x_minus_1))), x_minus_1);
  const V y2 =
      IfThenElse(is_x_gt_2, IfThenElse(is_x_large, x, y0), Add(y1, kOne));
  const V z = impl::Log<D, V, /*kAllowSubnormals=*/false>(d, y2);

  const auto is_pole = Eq(y2, kOne);
  const auto divisor = Sub(IfThenZeroElse(is_pole, y2), kOne);
  return Add(IfThenElse(is_x_gt_2, z,
                        IfThenElse(is_pole, y1, Div(Mul(z, y1), divisor))),
             IfThenElseZero(is_x_large, kLog2));
}

template <class D, class V>
HWY_INLINE V Asin(const D d, V x) {
  using T = TFromD<D>;

  const V kHalf = Set(d, static_cast<T>(+0.5));
  const V kTwo = Set(d, static_cast<T>(+2.0));
  const V kPiOverTwo = Set(d, static_cast<T>(+1.57079632679489661923132169));

  const V sign_x = And(SignBit(d), x);
  const V abs_x = Xor(x, sign_x);
  const auto mask = Lt(abs_x, kHalf);
  const V yy =
      IfThenElse(mask, Mul(abs_x, abs_x), NegMulAdd(abs_x, kHalf, kHalf));
  const V y = IfThenElse(mask, abs_x, Sqrt(yy));

  impl::AsinImpl<T> impl;
  const V z0 = MulAdd(impl.AsinPoly(d, yy, y), Mul(yy, y), y);
  const V z1 = NegMulAdd(z0, kTwo, kPiOverTwo);
  return Or(IfThenElse(mask, z0, z1), sign_x);
}

template <class D, class V>
HWY_INLINE V Asinh(const D d, V x) {
  using T = TFromD<D>;

  const V kSmall = Set(d, static_cast<T>(1.0 / 268435456.0));
  const V kLarge = Set(d, static_cast<T>(268435456.0));
  const V kLog2 = Set(d, static_cast<T>(0.693147180559945286227));
  const V kOne = Set(d, static_cast<T>(+1.0));
  const V kTwo = Set(d, static_cast<T>(+2.0));

  const V sign_x = And(SignBit(d), x);  // Extract the sign bit
  const V abs_x = Xor(x, sign_x);

  const auto is_x_large = Gt(abs_x, kLarge);
  const auto is_x_lt_2 = Lt(abs_x, kTwo);

  const V x2 = Mul(x, x);
  const V sqrt_x2_plus_1 = Sqrt(Add(x2, kOne));

  const V y0 = MulAdd(abs_x, kTwo, Div(kOne, Add(sqrt_x2_plus_1, abs_x)));
  const V y1 = Add(Div(x2, Add(sqrt_x2_plus_1, kOne)), abs_x);
  const V y2 =
      IfThenElse(is_x_lt_2, Add(y1, kOne), IfThenElse(is_x_large, abs_x, y0));
  const V z = impl::Log<D, V, /*kAllowSubnormals=*/false>(d, y2);

  const auto is_pole = Eq(y2, kOne);
  const auto divisor = Sub(IfThenZeroElse(is_pole, y2), kOne);
  const auto large = IfThenElse(is_pole, y1, Div(Mul(z, y1), divisor));
  const V y = IfThenElse(Lt(abs_x, kSmall), x, large);
  return Or(Add(IfThenElse(is_x_lt_2, y, z), IfThenElseZero(is_x_large, kLog2)),
            sign_x);
}

template <class D, class V>
HWY_INLINE V Atan(const D d, V x) {
  using T = TFromD<D>;

  const V kOne = Set(d, static_cast<T>(+1.0));
  const V kPiOverTwo = Set(d, static_cast<T>(+1.57079632679489661923132169));

  const V sign = And(SignBit(d), x);
  const V abs_x = Xor(x, sign);
  const auto mask = Gt(abs_x, kOne);

  impl::AtanImpl<T> impl;
  const auto divisor = IfThenElse(mask, abs_x, kOne);
  const V y = impl.AtanPoly(d, IfThenElse(mask, Div(kOne, divisor), abs_x));
  return Or(IfThenElse(mask, Sub(kPiOverTwo, y), y), sign);
}

template <class D, class V>
HWY_INLINE V Atanh(const D d, V x) {
  using T = TFromD<D>;

  const V kHalf = Set(d, static_cast<T>(+0.5));
  const V kOne = Set(d, static_cast<T>(+1.0));

  const V sign = And(SignBit(d), x);  // Extract the sign bit
  const V abs_x = Xor(x, sign);
  return Mul(Log1p(d, Div(Add(abs_x, abs_x), Sub(kOne, abs_x))),
             Xor(kHalf, sign));
}

template <class D, class V>
HWY_INLINE V Cos(const D d, V x) {
  using T = TFromD<D>;
  impl::CosSinImpl<T> impl;

  // Float Constants
  const V kOneOverPi = Set(d, static_cast<T>(0.31830988618379067153));

  // Integer Constants
  const Rebind<int32_t, D> di32;
  using VI32 = decltype(Zero(di32));
  const VI32 kOne = Set(di32, 1);

  const V y = Abs(x);  // cos(x) == cos(|x|)

  // Compute the quadrant, q = int(|x| / pi) * 2 + 1
  const VI32 q = Add(ShiftLeft<1>(impl.ToInt32(d, Mul(y, kOneOverPi))), kOne);

  // Reduce range, apply sign, and approximate.
  return impl.Poly(
      d, Xor(impl.CosReduce(d, y, q), impl.CosSignFromQuadrant(d, q)));
}

template <class D, class V>
HWY_INLINE V Exp(const D d, V x) {
  using T = TFromD<D>;

  const V kHalf = Set(d, static_cast<T>(+0.5));
  const V kLowerBound =
      Set(d, static_cast<T>((sizeof(T) == 4 ? -104.0 : -1000.0)));
  const V kNegZero = Set(d, static_cast<T>(-0.0));
  const V kOne = Set(d, static_cast<T>(+1.0));
  const V kOneOverLog2 = Set(d, static_cast<T>(+1.442695040888963407359924681));

  impl::ExpImpl<T> impl;

  // q = static_cast<int32>((x / log(2)) + ((x < 0) ? -0.5 : +0.5))
  const auto q =
      impl.ToInt32(d, MulAdd(x, kOneOverLog2, Or(kHalf, And(x, kNegZero))));

  // Reduce, approximate, and then reconstruct.
  const V y = impl.LoadExpShortRange(
      d, Add(impl.ExpPoly(d, impl.ExpReduce(d, x, q)), kOne), q);
  return IfThenElseZero(Ge(x, kLowerBound), y);
}

template <class D, class V>
HWY_INLINE V Expm1(const D d, V x) {
  using T = TFromD<D>;

  const V kHalf = Set(d, static_cast<T>(+0.5));
  const V kLowerBound =
      Set(d, static_cast<T>((sizeof(T) == 4 ? -104.0 : -1000.0)));
  const V kLn2Over2 = Set(d, static_cast<T>(+0.346573590279972654708616));
  const V kNegOne = Set(d, static_cast<T>(-1.0));
  const V kNegZero = Set(d, static_cast<T>(-0.0));
  const V kOne = Set(d, static_cast<T>(+1.0));
  const V kOneOverLog2 = Set(d, static_cast<T>(+1.442695040888963407359924681));

  impl::ExpImpl<T> impl;

  // q = static_cast<int32>((x / log(2)) + ((x < 0) ? -0.5 : +0.5))
  const auto q =
      impl.ToInt32(d, MulAdd(x, kOneOverLog2, Or(kHalf, And(x, kNegZero))));

  // Reduce, approximate, and then reconstruct.
  const V y = impl.ExpPoly(d, impl.ExpReduce(d, x, q));
  const V z = IfThenElse(Lt(Abs(x), kLn2Over2), y,
                         Sub(impl.LoadExpShortRange(d, Add(y, kOne), q), kOne));
  return IfThenElse(Lt(x, kLowerBound), kNegOne, z);
}

template <class D, class V>
HWY_INLINE V Log(const D d, V x) {
  return impl::Log<D, V, /*kAllowSubnormals=*/true>(d, x);
}

template <class D, class V>
HWY_INLINE V Log10(const D d, V x) {
  using T = TFromD<D>;
  return Mul(Log(d, x), Set(d, static_cast<T>(0.4342944819032518276511)));
}

template <class D, class V>
HWY_INLINE V Log1p(const D d, V x) {
  using T = TFromD<D>;
  const V kOne = Set(d, static_cast<T>(+1.0));

  const V y = Add(x, kOne);
  const auto is_pole = Eq(y, kOne);
  const auto divisor = Sub(IfThenZeroElse(is_pole, y), kOne);
  const auto non_pole =
      Mul(impl::Log<D, V, /*kAllowSubnormals=*/false>(d, y), Div(x, divisor));
  return IfThenElse(is_pole, x, non_pole);
}

template <class D, class V>
HWY_INLINE V Log2(const D d, V x) {
  using T = TFromD<D>;
  return Mul(Log(d, x), Set(d, static_cast<T>(1.44269504088896340735992)));
}

template <class D, class V>
HWY_INLINE V Sin(const D d, V x) {
  using T = TFromD<D>;
  impl::CosSinImpl<T> impl;

  // Float Constants
  const V kOneOverPi = Set(d, static_cast<T>(0.31830988618379067153));
  const V kHalf = Set(d, static_cast<T>(0.5));

  // Integer Constants
  const Rebind<int32_t, D> di32;
  using VI32 = decltype(Zero(di32));

  const V abs_x = Abs(x);
  const V sign_x = Xor(abs_x, x);

  // Compute the quadrant, q = int((|x| / pi) + 0.5)
  const VI32 q = impl.ToInt32(d, MulAdd(abs_x, kOneOverPi, kHalf));

  // Reduce range, apply sign, and approximate.
  return impl.Poly(d, Xor(impl.SinReduce(d, abs_x, q),
                          Xor(impl.SinSignFromQuadrant(d, q), sign_x)));
}

template <class D, class V>
HWY_INLINE V Sinh(const D d, V x) {
  using T = TFromD<D>;
  const V kHalf = Set(d, static_cast<T>(+0.5));
  const V kOne = Set(d, static_cast<T>(+1.0));
  const V kTwo = Set(d, static_cast<T>(+2.0));

  const V sign = And(SignBit(d), x);  // Extract the sign bit
  const V abs_x = Xor(x, sign);
  const V y = Expm1(d, abs_x);
  const V z = Mul(Div(Add(y, kTwo), Add(y, kOne)), Mul(y, kHalf));
  return Xor(z, sign);  // Reapply the sign bit
}

template <class D, class V>
HWY_INLINE V Tanh(const D d, V x) {
  using T = TFromD<D>;
  const V kLimit = Set(d, static_cast<T>(18.714973875));
  const V kOne = Set(d, static_cast<T>(+1.0));
  const V kTwo = Set(d, static_cast<T>(+2.0));

  const V sign = And(SignBit(d), x);  // Extract the sign bit
  const V abs_x = Xor(x, sign);
  const V y = Expm1(d, Mul(abs_x, kTwo));
  const V z = IfThenElse(Gt(abs_x, kLimit), kOne, Div(y, Add(y, kTwo)));
  return Xor(z, sign);  // Reapply the sign bit
}

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace hwy
HWY_AFTER_NAMESPACE();

#endif  // HIGHWAY_HWY_CONTRIB_MATH_MATH_INL_H_
