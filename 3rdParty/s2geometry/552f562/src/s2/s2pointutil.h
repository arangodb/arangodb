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
//
// Defines additional operations for points on the unit sphere (in addition to
// the standard vector operations defined in "util/math/vector.h").

#ifndef S2_S2POINTUTIL_H_
#define S2_S2POINTUTIL_H_

#include "s2/_fp_contract_off.h"
#include "s2/s1angle.h"
#include "s2/s2point.h"
#include "s2/util/math/matrix3x3.h"

// S2 is a namespace for constants and simple utility functions that are used
// throughout the S2 library.  The name "S2" is derived from the mathematical
// symbol for the two-dimensional unit sphere (note that the "2" refers to the
// dimension of the surface, not the space it is embedded in).
namespace S2 {

// Return a unique "origin" on the sphere for operations that need a fixed
// reference point.  In particular, this is the "point at infinity" used for
// point-in-polygon testing (by counting the number of edge crossings).
inline S2Point Origin();

// Return true if the given point is approximately unit length
// (this is mainly useful for assertions).
bool IsUnitLength(const S2Point& p);

// Return true if two points are within the given distance of each other
// (this is mainly useful for testing).
bool ApproxEquals(const S2Point& a, const S2Point& b,
                  S1Angle max_error = S1Angle::Radians(1e-15));

// Return a unit-length vector that is orthogonal to "a".  Satisfies
// Ortho(-a) = -Ortho(a) for all a.
//
// Note that Vector3_d also defines an "Ortho" method, but this one is
// preferred for use in S2 code because it explicitly tries to avoid result
// result coordinates that are zero.  (This is a performance optimization that
// reduces the amount of time spent in functions which handle degeneracies.)
S2Point Ortho(const S2Point& a);

// Return a vector "c" that is orthogonal to the given unit-length vectors
// "a" and "b".  This function is similar to a.CrossProd(b) except that it
// does a better job of ensuring orthogonality when "a" is nearly parallel
// to "b", and it returns a non-zero result even when a == b or a == -b.
//
// It satisfies the following properties (RCP == RobustCrossProd):
//
//   (1) RCP(a,b) != 0 for all a, b
//   (2) RCP(b,a) == -RCP(a,b) unless a == b or a == -b
//   (3) RCP(-a,b) == -RCP(a,b) unless a == b or a == -b
//   (4) RCP(a,-b) == -RCP(a,b) unless a == b or a == -b
//
// The result is not guaranteed to be unit length.
S2Point RobustCrossProd(const S2Point& a, const S2Point& b);

// Rotate the given point about the given axis by the given angle.  "p" and
// "axis" must be unit length; "angle" has no restrictions (e.g., it can be
// positive, negative, greater than 360 degrees, etc).
S2Point Rotate(const S2Point& p, const S2Point& axis, S1Angle angle);

// Extend the given point "z" on the unit sphere into a right-handed
// coordinate frame of unit-length column vectors m = (x,y,z).  Note that the
// vectors (x,y) are an orthonormal frame for the tangent space at "z", while
// "z" itself is an orthonormal frame for the normal space at "z".
Matrix3x3_d GetFrame(const S2Point& z);
void GetFrame(const S2Point& z, Matrix3x3_d* m);

// Given an orthonormal basis "m" of column vectors and a point "p", return
// the coordinates of "p" with respect to the basis "m".  The resulting
// point "q" satisfies the identity (m * q == p).
S2Point ToFrame(const Matrix3x3_d& m, const S2Point& p);

// Given an orthonormal basis "m" of column vectors and a point "q" with
// respect to that basis, return the equivalent point "p" with respect to
// the standard axis-aligned basis.  The result satisfies (p == m * q).
Matrix3x3_d GetFrame(const S2Point& z);
S2Point FromFrame(const Matrix3x3_d& m, const S2Point& q);

// Return true if the points A, B, C are strictly counterclockwise.  Return
// false if the points are clockwise or collinear (i.e. if they are all
// contained on some great circle).
//
// Due to numerical errors, situations may arise that are mathematically
// impossible, e.g. ABC may be considered strictly CCW while BCA is not.
// However, the implementation guarantees the following:
//
//   If SimpleCCW(a,b,c), then !SimpleCCW(c,b,a) for all a,b,c.
ABSL_DEPRECATED("Use s2pred::Sign instead.")
bool SimpleCCW(const S2Point& a, const S2Point& b, const S2Point& c);


//////////////////   Implementation details follow   ////////////////////

// Uncomment the following line for testing purposes only.
// #define S2_TEST_DEGENERACIES

inline S2Point Origin() {
#ifdef S2_TEST_DEGENERACIES
  // This value makes polygon operations much slower, because it greatly
  // increases the number of degenerate cases that need to be handled using
  // s2pred::ExpensiveSign().
  return S2Point(0, 0, 1);
#else
  // The origin should not be a point that is commonly used in edge tests in
  // order to avoid triggering code to handle degenerate cases.  (This rules
  // out the north and south poles.)  It should also not be on the boundary of
  // any low-level S2Cell for the same reason.
  //
  // The point chosen here is about 66km from the north pole towards the East
  // Siberian Sea.  See the unittest for more details.  It is written out
  // explicitly using floating-point literals because the optimizer doesn't
  // seem willing to evaluate Normalize() at compile time.
  return S2Point(-0.0099994664350250197, 0.0025924542609324121,
                 0.99994664350250195);
#endif
}

}  // namespace S2

#endif  // S2_S2POINTUTIL_H_
