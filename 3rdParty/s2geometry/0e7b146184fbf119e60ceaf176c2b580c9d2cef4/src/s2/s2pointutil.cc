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

#include "s2/s2pointutil.h"

#include <cfloat>
#include <cmath>

using std::fabs;

namespace S2 {

bool IsUnitLength(const S2Point& p) {
  // Normalize() is guaranteed to return a vector whose L2-norm differs from 1
  // by less than 2 * DBL_EPSILON.  Thus the squared L2-norm differs by less
  // than 4 * DBL_EPSILON.  The actual calculated Norm2() can have up to 1.5 *
  // DBL_EPSILON of additional error.  The total error of 5.5 * DBL_EPSILON
  // can then be rounded down since the result must be a representable
  // double-precision value.
  return fabs(p.Norm2() - 1) <= 5 * DBL_EPSILON;  // About 1.11e-15
}

bool ApproxEquals(const S2Point& a, const S2Point& b, S1Angle max_error) {
  return S1Angle(a, b) <= max_error;
}

S2Point Ortho(const S2Point& a) {
#ifdef S2_TEST_DEGENERACIES
  // Vector3::Ortho() always returns a point on the X-Y, Y-Z, or X-Z planes.
  // This leads to many more degenerate cases in polygon operations.
  return a.Ortho();
#else
  int k = a.LargestAbsComponent() - 1;
  if (k < 0) k = 2;
  S2Point temp(0.012, 0.0053, 0.00457);
  temp[k] = 1;
  return a.CrossProd(temp).Normalize();
#endif
}

Vector3_d RobustCrossProd(const S2Point& a, const S2Point& b) {
  // The direction of a.CrossProd(b) becomes unstable as (a + b) or (a - b)
  // approaches zero.  This leads to situations where a.CrossProd(b) is not
  // very orthogonal to "a" and/or "b".  We could fix this using Gram-Schmidt,
  // but we also want b.RobustCrossProd(a) == -a.RobustCrossProd(b).
  //
  // The easiest fix is to just compute the cross product of (b+a) and (b-a).
  // Mathematically, this cross product is exactly twice the cross product of
  // "a" and "b", but it has the numerical advantage that (b+a) and (b-a)
  // are always perpendicular (since "a" and "b" are unit length).  This
  // yields a result that is nearly orthogonal to both "a" and "b" even if
  // these two values differ only in the lowest bit of one component.

  S2_DCHECK(IsUnitLength(a));
  S2_DCHECK(IsUnitLength(b));
  Vector3_d x = (b + a).CrossProd(b - a);
  if (x != S2Point(0, 0, 0)) return x;

  // The only result that makes sense mathematically is to return zero, but
  // we find it more convenient to return an arbitrary orthogonal vector.
  return Ortho(a);
}

S2Point Rotate(const S2Point& p, const S2Point& axis, S1Angle angle) {
  S2_DCHECK(IsUnitLength(p));
  S2_DCHECK(IsUnitLength(axis));
  // Let M be the plane through P that is perpendicular to "axis", and let
  // "center" be the point where M intersects "axis".  We construct a
  // right-handed orthogonal frame (dx, dy, center) such that "dx" is the
  // vector from "center" to P, and "dy" has the same length as "dx".  The
  // result can then be expressed as (cos(angle)*dx + sin(angle)*dy + center).
  S2Point center = p.DotProd(axis) * axis;
  S2Point dx = p - center;
  S2Point dy = axis.CrossProd(p);
  // Mathematically the result is unit length, but normalization is necessary
  // to ensure that numerical errors don't accumulate.
  return (cos(angle) * dx + sin(angle) * dy + center).Normalize();
}

Matrix3x3_d GetFrame(const S2Point& z) {
  Matrix3x3_d m;
  GetFrame(z, &m);
  return m;
}

void GetFrame(const S2Point& z, Matrix3x3_d* m) {
  S2_DCHECK(IsUnitLength(z));
  m->SetCol(2, z);
  m->SetCol(1, Ortho(z));
  m->SetCol(0, m->Col(1).CrossProd(z));  // Already unit-length.
}

S2Point ToFrame(const Matrix3x3_d& m, const S2Point& p) {
  // The inverse of an orthonormal matrix is its transpose.
  return m.Transpose() * p;
}

S2Point FromFrame(const Matrix3x3_d& m, const S2Point& q) {
  return m * q;
}

bool SimpleCCW(const S2Point& a, const S2Point& b, const S2Point& c) {
  // We compute the signed volume of the parallelepiped ABC.  The usual
  // formula for this is (AxB).C, but we compute it here using (CxA).B
  // in order to ensure that ABC and CBA are not both CCW.  This follows
  // from the following identities (which are true numerically, not just
  // mathematically):
  //
  //     (1) x.CrossProd(y) == -(y.CrossProd(x))
  //     (2) (-x).DotProd(y) == -(x.DotProd(y))

  return c.CrossProd(a).DotProd(b) > 0;
}

}  // namespace S2
