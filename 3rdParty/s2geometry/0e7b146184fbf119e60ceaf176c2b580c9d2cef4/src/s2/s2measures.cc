// Copyright 2005 Google Inc. All Rights Reserved.
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

#include "s2/s2measures.h"

#include <algorithm>
#include <cmath>

#include "s2/s2pointutil.h"
#include "s2/s2predicates.h"

// TODO(ericv): Convert to using s2pred::Sign().
//#include "util/geometry/s2predicates.h"

using std::atan;
using std::max;
using std::sqrt;
using std::tan;

namespace S2 {

double Angle(const S2Point& a, const S2Point& b, const S2Point& c) {
  // RobustCrossProd() is necessary to get good accuracy when two of the input
  // points are very close together.
  return RobustCrossProd(a, b).Angle(RobustCrossProd(c, b));
}

double TurnAngle(const S2Point& a, const S2Point& b, const S2Point& c) {
  // We use RobustCrossProd() to get good accuracy when two points are very
  // close together, and Sign() to ensure that the sign is correct for
  // turns that are close to 180 degrees.
  //
  // Unfortunately we can't save RobustCrossProd(a, b) and pass it as the
  // optional 4th argument to Sign(), because Sign() requires a.CrossProd(b)
  // exactly (the robust version differs in magnitude).
  double angle = RobustCrossProd(a, b).Angle(RobustCrossProd(b, c));

  // Don't return Sign() * angle because it is legal to have (a == c).
  return (s2pred::Sign(a, b, c) > 0) ? angle : -angle;
}

double Area(const S2Point& a, const S2Point& b, const S2Point& c) {
  S2_DCHECK(IsUnitLength(a));
  S2_DCHECK(IsUnitLength(b));
  S2_DCHECK(IsUnitLength(c));
  // This method is based on l'Huilier's theorem,
  //
  //   tan(E/4) = sqrt(tan(s/2) tan((s-a)/2) tan((s-b)/2) tan((s-c)/2))
  //
  // where E is the spherical excess of the triangle (i.e. its area),
  //       a, b, c, are the side lengths, and
  //       s is the semiperimeter (a + b + c) / 2 .
  //
  // The only significant source of error using l'Huilier's method is the
  // cancellation error of the terms (s-a), (s-b), (s-c).  This leads to a
  // *relative* error of about 1e-16 * s / min(s-a, s-b, s-c).  This compares
  // to a relative error of about 1e-15 / E using Girard's formula, where E is
  // the true area of the triangle.  Girard's formula can be even worse than
  // this for very small triangles, e.g. a triangle with a true area of 1e-30
  // might evaluate to 1e-5.
  //
  // So, we prefer l'Huilier's formula unless dmin < s * (0.1 * E), where
  // dmin = min(s-a, s-b, s-c).  This basically includes all triangles
  // except for extremely long and skinny ones.
  //
  // Since we don't know E, we would like a conservative upper bound on
  // the triangle area in terms of s and dmin.  It's possible to show that
  // E <= k1 * s * sqrt(s * dmin), where k1 = 2*sqrt(3)/Pi (about 1).
  // Using this, it's easy to show that we should always use l'Huilier's
  // method if dmin >= k2 * s^5, where k2 is about 1e-2.  Furthermore,
  // if dmin < k2 * s^5, the triangle area is at most k3 * s^4, where
  // k3 is about 0.1.  Since the best case error using Girard's formula
  // is about 1e-15, this means that we shouldn't even consider it unless
  // s >= 3e-4 or so.
  //
  // TODO(ericv): Implement rigorous error bounds (analysis already done).
  double sa = b.Angle(c);
  double sb = c.Angle(a);
  double sc = a.Angle(b);
  double s = 0.5 * (sa + sb + sc);
  if (s >= 3e-4) {
    // Consider whether Girard's formula might be more accurate.
    double s2 = s * s;
    double dmin = s - max(sa, max(sb, sc));
    if (dmin < 1e-2 * s * s2 * s2) {
      // This triangle is skinny enough to consider using Girard's formula.
      // We increase the area by the approximate maximum error in the Girard
      // calculation in order to ensure that this test is conservative.
      double area = GirardArea(a, b, c);
      if (dmin < s * (0.1 * (area + 5e-15))) return area;
    }
  }
  // Use l'Huilier's formula.
  return 4 * atan(sqrt(max(0.0, tan(0.5 * s) * tan(0.5 * (s - sa)) *
                           tan(0.5 * (s - sb)) * tan(0.5 * (s - sc)))));
}

double GirardArea(const S2Point& a, const S2Point& b, const S2Point& c) {
  // This is equivalent to the usual Girard's formula but is slightly more
  // accurate, faster to compute, and handles a == b == c without a special
  // case.  RobustCrossProd() is necessary to get good accuracy when two of
  // the input points are very close together.

  Vector3_d ab = RobustCrossProd(a, b);
  Vector3_d bc = RobustCrossProd(b, c);
  Vector3_d ac = RobustCrossProd(a, c);
  return max(0.0, ab.Angle(ac) - ab.Angle(bc) + bc.Angle(ac));
}

double SignedArea(const S2Point& a, const S2Point& b, const S2Point& c) {
  return s2pred::Sign(a, b, c) * Area(a, b, c);
}

}  // namespace S2
