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

// Returns a unique "origin" on the sphere for operations that need a fixed
// reference point.  In particular, this is the "point at infinity" used for
// point-in-polygon testing (by counting the number of edge crossings).
// To be clear, this is NOT (0,0,0), the origin of the coordinate system.
inline S2Point Origin();

// Returns true if the given point is approximately unit length
// (this is mainly useful for assertions).
bool IsUnitLength(const S2Point& p);

// Returns true if two points are within the given distance of each other
// (this is mainly useful for testing).  It is an error if either point is a
// zero-length vector (default S2Point), but this is only checked in debug
// mode.  In non-debug mode it will always return true.
bool ApproxEquals(const S2Point& a, const S2Point& b,
                  S1Angle max_error = S1Angle::Radians(1e-15));

// Returns a unit-length vector that is orthogonal to "a".  Satisfies
// Ortho(-a) = -Ortho(a) for all a.
//
// Note that Vector3_d also defines an "Ortho" method, but this one is
// preferred for use in S2 code because it explicitly tries to avoid result
// coordinates that are zero.  (This is a performance optimization that
// reduces the amount of time spent in functions that handle degeneracies.)
S2Point Ortho(const S2Point& a);

// Returns a unit-length vector used as the reference direction for deciding
// whether a polygon with semi-open boundaries contains the given vertex "a"
// (see S2ContainsVertexQuery).  The result is unit length and is guaranteed
// to be different from the given point "a".
S2Point RefDir(const S2Point& a);

// Rotates the given point about the given axis by the given angle.  "p" and
// "axis" must be unit length; "angle" has no restrictions (e.g., it can be
// positive, negative, greater than 360 degrees, etc).
//
// See also the closely related functions S2::GetPointOnRay() and
// S2::GetPointOnLine(), which are declared in s2edge_distances.h.
S2Point Rotate(const S2Point& p, const S2Point& axis, S1Angle angle);

// Extends the given point "z" on the unit sphere into a right-handed
// coordinate frame of unit-length column vectors m = (x,y,z).  Note that the
// vectors (x,y) are an orthonormal frame for the tangent space at "z", while
// "z" itself is an orthonormal frame for the normal space at "z".
Matrix3x3_d GetFrame(const S2Point& z);
void GetFrame(const S2Point& z, Matrix3x3_d* m);

// Given an orthonormal basis "m" of column vectors and a point "p", returns
// the coordinates of "p" with respect to the basis "m".  The resulting
// point "q" satisfies the identity (m * q == p).
S2Point ToFrame(const Matrix3x3_d& m, const S2Point& p);

// Given an orthonormal basis "m" of column vectors and a point "q" with
// respect to that basis, returns the equivalent point "p" with respect to
// the standard axis-aligned basis.  The result satisfies (p == m * q).
S2Point FromFrame(const Matrix3x3_d& m, const S2Point& q);


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

inline S2Point RefDir(const S2Point& a) {
  return S2::Ortho(a);
}

}  // namespace S2

#endif  // S2_S2POINTUTIL_H_
