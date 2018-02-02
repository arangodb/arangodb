// Copyright 2008 Google Inc. All Rights Reserved.
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


#include "s2/util/math/mathutil.h"

#include <cmath>
#include <cstdlib>

namespace {
// Returns the sign of x:
//   -1 if x < 0,
//   +1 if x > 0,
//    0 if x = 0.
template <class T>
inline T sgn(const T x) {
  return (x == 0 ? 0 : (x < 0 ? -1 : 1));
}
}  // namespace

bool MathUtil::RealRootsForCubic(long double const a,
                                 long double const b,
                                 long double const c,
                                 long double *const r1,
                                 long double *const r2,
                                 long double *const r3) {
  // According to Numerical Recipes (pp. 184-5), what
  // follows is an arrangement of computations to
  // compute the roots of a cubic that minimizes
  // roundoff error (as pointed out by A.J. Glassman).

  long double const a_squared = a * a, a_third = a / 3.0, b_tripled = 3.0 * b;
  long double const Q = (a_squared - b_tripled) / 9.0;
  long double const R =
      (2.0 * a_squared * a - 3.0 * a * b_tripled + 27.0 * c) / 54.0;

  long double const R_squared = R * R;
  long double const Q_cubed = Q * Q * Q;

  if (R_squared < Q_cubed) {
    long double const root_Q = sqrt(Q);
    long double const two_pi_third = 2.0 * M_PI / 3.0;
    long double const theta_third = acos(R / sqrt(Q_cubed)) / 3.0;
    long double const minus_two_root_Q = -2.0 * root_Q;

    *r1 = minus_two_root_Q * cos(theta_third) - a_third;
    *r2 = minus_two_root_Q * cos(theta_third + two_pi_third) - a_third;
    *r3 = minus_two_root_Q * cos(theta_third - two_pi_third) - a_third;

    return true;
  }

  long double const A =
    -sgn(R) * pow(std::abs(R) + sqrt(R_squared - Q_cubed), 1.0 / 3.0L);

  if (A != 0.0) {  // in which case, B from NR is zero
    *r1 = A + Q / A - a_third;
    return false;
  }

  *r1 = *r2 = *r3 = -a_third;
  return true;
}
