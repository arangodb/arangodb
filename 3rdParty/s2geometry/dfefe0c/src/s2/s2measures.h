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
// Defines various angle and area measures on the sphere.  Also see
// s2edge_distances.h, s2loop_measures.h, and s2polyline_measures.h.

#ifndef S2_S2MEASURES_H_
#define S2_S2MEASURES_H_

#include "s2/s2point.h"

namespace S2 {

// Return the interior angle at the vertex B in the triangle ABC.  The
// return value is always in the range [0, Pi].  All points should be
// normalized.  Ensures that Angle(a,b,c) == Angle(c,b,a) for all a,b,c.
//
// The angle is undefined if A or C is diametrically opposite from B, and
// becomes numerically unstable as the length of edge AB or BC approaches
// 180 degrees.
double Angle(const S2Point& a, const S2Point& b, const S2Point& c);

// Return the exterior angle at vertex B in the triangle ABC.  The return
// value is positive if ABC is counterclockwise and negative otherwise.  If
// you imagine an ant walking from A to B to C, this is the angle that the
// ant turns at vertex B (positive = left = CCW, negative = right = CW).
// This quantity is also known as the "geodesic curvature" at B.
//
// Ensures that TurnAngle(a,b,c) == -TurnAngle(c,b,a) for all distinct
// a,b,c. The result is undefined if (a == b || b == c), but is either
// -Pi or Pi if (a == c).  All points should be normalized.
double TurnAngle(const S2Point& a, const S2Point& b, const S2Point& c);

// Return the area of triangle ABC.  This method combines two different
// algorithms to get accurate results for both large and small triangles.
// The maximum error is about 5e-15 (about 0.25 square meters on the Earth's
// surface), the same as GirardArea() below, but unlike that method it is
// also accurate for small triangles.  Example: when the true area is 100
// square meters, Area() yields an error about 1 trillion times smaller than
// GirardArea().
//
// All points should be unit length, and no two points should be antipodal.
// The area is always positive.
double Area(const S2Point& a, const S2Point& b, const S2Point& c);

// Return the area of the triangle computed using Girard's formula.  All
// points should be unit length, and no two points should be antipodal.
//
// This method is about twice as fast as Area() but has poor relative
// accuracy for small triangles.  The maximum error is about 5e-15 (about
// 0.25 square meters on the Earth's surface) and the average error is about
// 1e-15.  These bounds apply to triangles of any size, even as the maximum
// edge length of the triangle approaches 180 degrees.  But note that for
// such triangles, tiny perturbations of the input points can change the
// true mathematical area dramatically.
double GirardArea(const S2Point& a, const S2Point& b, const S2Point& c);

// Like Area(), but returns a positive value for counterclockwise triangles
// and a negative value otherwise.
double SignedArea(const S2Point& a, const S2Point& b, const S2Point& c);

}  // namespace S2

#endif  // S2_S2MEASURES_H_
