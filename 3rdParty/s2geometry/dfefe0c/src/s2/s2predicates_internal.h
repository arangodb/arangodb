// Copyright 2016 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS-IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

// Author: ericv@google.com (Eric Veach)
//
// The following functions are not part of the public API.  Currently they are
// only used internally for testing purposes.

#ifndef S2_S2PREDICATES_INTERNAL_H_
#define S2_S2PREDICATES_INTERNAL_H_

#include <limits>

#include "s2/third_party/absl/base/casts.h"
#include "s2/s1chord_angle.h"
#include "s2/s2predicates.h"
#include "s2/util/math/exactfloat/exactfloat.h"
#include "s2/util/math/vector.h"

namespace s2pred {

// Returns 2 ** (-digits).  This could be implemented using "ldexp" except
// that std::ldexp is not constexpr in C++11.
constexpr double epsilon_for_digits(int digits) {
  return (digits < 64 ? 1.0 / (1ULL << digits) :
          epsilon_for_digits(digits - 63) / (1ULL << 63));
}

// Returns the maximum rounding error for arithmetic operations in type T.
// We could simply return 0.5 * numeric_limits<T>::epsilon(), except that some
// platforms implement "long double" using "double-double" arithmetic, and for
// those platforms we need to compute the rounding error manually based on
// numeric_limits<T>::digits (the number of bits of mantissa precision).
template <typename T> constexpr T rounding_epsilon() {
  return epsilon_for_digits(std::numeric_limits<T>::digits);
}

using Vector3_ld = Vector3<long double>;
using Vector3_xf = Vector3<ExactFloat>;

inline static Vector3_ld ToLD(const S2Point& x) {
  return Vector3_ld::Cast(x);
}

inline static long double ToLD(double x) {
  return absl::implicit_cast<long double>(x);
}

inline static Vector3_xf ToExact(const S2Point& x) {
  return Vector3_xf::Cast(x);
}

int StableSign(const S2Point& a, const S2Point& b, const S2Point& c);

int ExactSign(const S2Point& a, const S2Point& b, const S2Point& c,
              bool perturb);

int SymbolicallyPerturbedSign(
    const Vector3_xf& a, const Vector3_xf& b,
    const Vector3_xf& c, const Vector3_xf& b_cross_c);

template <class T>
int TriageCompareCosDistances(const Vector3<T>& x,
                              const Vector3<T>& a, const Vector3<T>& b);

template <class T>
int TriageCompareSin2Distances(const Vector3<T>& x,
                               const Vector3<T>& a, const Vector3<T>& b);

int ExactCompareDistances(const Vector3_xf& x,
                          const Vector3_xf& a, const Vector3_xf& b);

int SymbolicCompareDistances(const S2Point& x,
                             const S2Point& a, const S2Point& b);

template <class T>
int TriageCompareSin2Distance(const Vector3<T>& x, const Vector3<T>& y, T r2);

template <class T>
int TriageCompareCosDistance(const Vector3<T>& x, const Vector3<T>& y, T r2);

int ExactCompareDistance(const Vector3_xf& x, const Vector3_xf& y,
                         const ExactFloat& r2);

template <class T>
int TriageCompareEdgeDistance(const Vector3<T>& x, const Vector3<T>& a0,
                              const Vector3<T>& a1, T r2);

int ExactCompareEdgeDistance(const S2Point& x, const S2Point& a0,
                             const S2Point& a1, S1ChordAngle r);

template <class T>
int TriageCompareEdgeDirections(
    const Vector3<T>& a0, const Vector3<T>& a1,
    const Vector3<T>& b0, const Vector3<T>& b1);

int ExactCompareEdgeDirections(const Vector3_xf& a0, const Vector3_xf& a1,
                               const Vector3_xf& b0, const Vector3_xf& b1);

template <class T>
int TriageEdgeCircumcenterSign(const Vector3<T>& x0, const Vector3<T>& x1,
                               const Vector3<T>& a, const Vector3<T>& b,
                               const Vector3<T>& c, int abc_sign);

int ExactEdgeCircumcenterSign(const Vector3_xf& x0, const Vector3_xf& x1,
                              const Vector3_xf& a, const Vector3_xf& b,
                              const Vector3_xf& c, int abc_sign);

int SymbolicEdgeCircumcenterSign(
    const S2Point& x0, const S2Point& x1,
    const S2Point& a_arg, const S2Point& b_arg, const S2Point& c_arg);

template <class T>
Excluded TriageVoronoiSiteExclusion(const Vector3<T>& a, const Vector3<T>& b,
                                    const Vector3<T>& x0, const Vector3<T>& x1,
                                    T r2);

Excluded ExactVoronoiSiteExclusion(const Vector3_xf& a, const Vector3_xf& b,
                                   const Vector3_xf& x0, const Vector3_xf& x1,
                                   const ExactFloat& r2);
}  // namespace s2pred

#endif  // S2_S2PREDICATES_INTERNAL_H_
