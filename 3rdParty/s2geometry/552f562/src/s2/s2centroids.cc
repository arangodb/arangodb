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

#include "s2/s2centroids.h"

#include <cmath>

#include "s2/s2pointutil.h"

namespace S2 {

S2Point PlanarCentroid(const S2Point& a, const S2Point& b,
                       const S2Point& c) {
  return (1./3) * (a + b + c);
}

S2Point TrueCentroid(const S2Point& a, const S2Point& b,
                     const S2Point& c) {
  S2_DCHECK(IsUnitLength(a));
  S2_DCHECK(IsUnitLength(b));
  S2_DCHECK(IsUnitLength(c));

  // I couldn't find any references for computing the true centroid of a
  // spherical triangle...  I have a truly marvellous demonstration of this
  // formula which this margin is too narrow to contain :)

  // Use Angle() in order to get accurate results for small triangles.
  double angle_a = b.Angle(c);
  double angle_b = c.Angle(a);
  double angle_c = a.Angle(b);
  double ra = (angle_a == 0) ? 1 : (angle_a / std::sin(angle_a));
  double rb = (angle_b == 0) ? 1 : (angle_b / std::sin(angle_b));
  double rc = (angle_c == 0) ? 1 : (angle_c / std::sin(angle_c));

  // Now compute a point M such that:
  //
  //  [Ax Ay Az] [Mx]                       [ra]
  //  [Bx By Bz] [My]  = 0.5 * det(A,B,C) * [rb]
  //  [Cx Cy Cz] [Mz]                       [rc]
  //
  // To improve the numerical stability we subtract the first row (A) from the
  // other two rows; this reduces the cancellation error when A, B, and C are
  // very close together.  Then we solve it using Cramer's rule.
  //
  // TODO(ericv): This code still isn't as numerically stable as it could be.
  // The biggest potential improvement is to compute B-A and C-A more
  // accurately so that (B-A)x(C-A) is always inside triangle ABC.
  S2Point x(a.x(), b.x() - a.x(), c.x() - a.x());
  S2Point y(a.y(), b.y() - a.y(), c.y() - a.y());
  S2Point z(a.z(), b.z() - a.z(), c.z() - a.z());
  S2Point r(ra, rb - ra, rc - ra);
  return 0.5 * S2Point(y.CrossProd(z).DotProd(r),
                       z.CrossProd(x).DotProd(r),
                       x.CrossProd(y).DotProd(r));
}

}  // namespace S2
