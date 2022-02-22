// Copyright 2018 Google Inc. All Rights Reserved.
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
// Defines various angle and area measures for loops on the sphere.  These are
// low-level methods that work directly with arrays of S2Points.  They are
// used to implement the methods in s2shapeindex_measures.h,
// s2shape_measures.h, s2loop.h, and s2polygon.h.
//
// See s2polyline_measures.h, s2edge_distances.h, and s2measures.h for
// additional low-level methods.

#ifndef S2_S2LOOP_MEASURES_H_
#define S2_S2LOOP_MEASURES_H_

#include <cmath>
#include <ostream>
#include <vector>

#include "s2/s1angle.h"
#include "s2/s2point.h"
#include "s2/s2point_span.h"
#include "s2/s2pointutil.h"

namespace S2 {

// Returns the perimeter of the loop.
S1Angle GetPerimeter(S2PointLoopSpan loop);

// Returns the area of the loop interior, i.e. the region on the left side of
// the loop.  The result is between 0 and 4*Pi steradians.  The implementation
// ensures that nearly-degenerate clockwise loops have areas close to zero,
// while nearly-degenerate counter-clockwise loops have areas close to 4*Pi.
double GetArea(S2PointLoopSpan loop);

// Like GetArea(), except that this method is faster and has more error.  The
// result is between 0 and 4*Pi steradians.  The maximum error is 2.22e-15
// steradians per loop vertex, which works out to about 0.09 square meters per
// vertex on the Earth's surface.  For example, a loop with 100 vertices has a
// maximum error of about 9 square meters.  (The actual error is typically
// much smaller than this.)  The error bound can be computed using
// GetCurvatureMaxError(), which returns the maximum error in steradians.
double GetApproxArea(S2PointLoopSpan loop);

// Returns either the positive area of the region on the left side of the
// loop, or the negative area of the region on the right side of the loop,
// whichever is smaller in magnitude.  The result is between -2*Pi and 2*Pi
// steradians.  This method is used to accurately compute the area of polygons
// consisting of multiple loops.
//
// The following cases are handled specially:
//
//  - Counter-clockwise loops are guaranteed to have positive area, and
//    clockwise loops are guaranteed to have negative area.
//
//  - Degenerate loops (consisting of an isolated vertex or composed entirely
//    of sibling edge pairs) have an area of exactly zero.
//
//  - The full loop (containing all points, and represented as a loop with no
//    vertices) has a negative area with the minimum possible magnitude.
//    (This is the "signed equivalent" of having an area of 4*Pi.)
double GetSignedArea(S2PointLoopSpan loop);

// Returns the geodesic curvature of the loop, defined as the sum of the turn
// angles at each vertex (see S2::TurnAngle).  The result is positive if the
// loop is counter-clockwise, negative if the loop is clockwise, and zero if
// the loop is a great circle.  The geodesic curvature is equal to 2*Pi minus
// the area of the loop.
//
// The following cases are handled specially:
//
//  - Degenerate loops (consisting of an isolated vertex or composed entirely
//    of sibling edge pairs) have a curvature of 2*Pi exactly.
//
//  - The full loop (containing all points, and represented as a loop with no
//    vertices) has a curvature of -2*Pi exactly.
//
//  - All other loops have a non-zero curvature in the range (-2*Pi, 2*Pi).
//    For any such loop, reversing the order of the vertices is guaranteed to
//    negate the curvature.  This property can be used to define a unique
//    normalized orientation for every loop.
double GetCurvature(S2PointLoopSpan loop);

// Returns the maximum error in GetCurvature() for the given loop.  This value
// is also an upper bound on the error in GetArea(), GetSignedArea(), and
// GetApproxArea().
double GetCurvatureMaxError(S2PointLoopSpan loop);

// Returns the true centroid of the loop multiplied by the area of the loop
// (see s2centroids.h for details on centroids).  The result is not unit
// length, so you may want to normalize it.  Also note that in general, the
// centroid may not be contained by the loop.
//
// The result is scaled by the loop area for two reasons: (1) it is cheaper to
// compute this way, and (2) it makes it easier to compute the centroid of
// more complicated shapes (by splitting them into disjoint regions and adding
// their centroids).
S2Point GetCentroid(S2PointLoopSpan loop);

// Returns true if the loop area is at most 2*Pi.  (A small amount of error is
// allowed in order to ensure that loops representing an entire hemisphere are
// always considered normalized.)
//
// Degenerate loops are handled consistently with s2pred::Sign(), i.e., if a
// loop can be expressed as the union of degenerate or nearly-degenerate
// counter-clockwise triangles then this method will return true.
bool IsNormalized(S2PointLoopSpan loop);

// LoopOrder represents a cyclic ordering of the loop vertices, starting at
// the index "first" and proceeding in direction "dir" (either +1 or -1).
// "first" and "dir" must be chosen such that (first, ..., first + n * dir)
// are all in the range [0, 2*n-1] as required by S2PointLoopSpan::operator[].
struct LoopOrder {
  LoopOrder(int _first, int _dir) : first(_first), dir(_dir) {}
  int first;
  int dir;
};
bool operator==(LoopOrder x, LoopOrder y);
std::ostream& operator<<(std::ostream& os, LoopOrder order);

// Returns an index "first" and a direction "dir" such that the vertex
// sequence (first, first + dir, ..., first + (n - 1) * dir) does not change
// when the loop vertex order is rotated or reversed.  This allows the loop
// vertices to be traversed in a canonical order.
LoopOrder GetCanonicalLoopOrder(S2PointLoopSpan loop);

// Returns the oriented surface integral of some quantity f(x) over the loop
// interior, given a function f_tri(A,B,C) that returns the corresponding
// integral over the spherical triangle ABC.  Here "oriented surface integral"
// means:
//
// (1) f_tri(A,B,C) must be the integral of f if ABC is counterclockwise,
//     and the integral of -f if ABC is clockwise.
//
// (2) The result of this function is *either* the integral of f over the
//     loop interior, or the integral of (-f) over the loop exterior.
//
// Note that there are at least two common situations where property (2) above
// is not a limitation:
//
//  - If the integral of f over the entire sphere is zero, then it doesn't
//    matter which case is returned because they are always equal.
//
//  - If f is non-negative, then it is easy to detect when the integral over
//    the loop exterior has been returned, and the integral over the loop
//    interior can be obtained by adding the integral of f over the entire
//    unit sphere (a constant) to the result.
//
// REQUIRES: The default constructor for T must initialize the value to zero.
//           (This is true for built-in types such as "double".)
template <class T>
T GetSurfaceIntegral(S2PointLoopSpan loop,
                     T f_tri(const S2Point&, const S2Point&, const S2Point&));

// Returns a new loop obtained by removing all degeneracies from "loop".  In
// particular, the result will not contain any adjacent duplicate vertices or
// sibling edge pairs, i.e. vertex sequences of the form (A, A) or (A, B, A).
//
// "new_vertices" represents storage where new loop vertices may be written.
// Note that the S2PointLoopSpan result may be a subsequence of either "loop"
// or "new_vertices", and therefore "new_vertices" must persist until the
// result of this method is no longer needed.
S2PointLoopSpan PruneDegeneracies(S2PointLoopSpan loop,
                                  std::vector<S2Point>* new_vertices);


//////////////////// Implementation details follow ////////////////////////


inline bool operator==(LoopOrder x, LoopOrder y) {
  return x.first == y.first && x.dir == y.dir;
}

template <class T>
T GetSurfaceIntegral(S2PointLoopSpan loop,
                     T f_tri(const S2Point&, const S2Point&, const S2Point&)) {
  // We sum "f_tri" over a collection T of oriented triangles, possibly
  // overlapping.  Let the sign of a triangle be +1 if it is CCW and -1
  // otherwise, and let the sign of a point "x" be the sum of the signs of the
  // triangles containing "x".  Then the collection of triangles T is chosen
  // such that either:
  //
  //  (1) Each point in the loop interior has sign +1, and sign 0 otherwise; or
  //  (2) Each point in the loop exterior has sign -1, and sign 0 otherwise.
  //
  // The triangles basically consist of a "fan" from vertex 0 to every loop
  // edge that does not include vertex 0.  These triangles will always satisfy
  // either (1) or (2).  However, what makes this a bit tricky is that
  // spherical edges become numerically unstable as their length approaches
  // 180 degrees.  Of course there is not much we can do if the loop itself
  // contains such edges, but we would like to make sure that all the triangle
  // edges under our control (i.e., the non-loop edges) are stable.  For
  // example, consider a loop around the equator consisting of four equally
  // spaced points.  This is a well-defined loop, but we cannot just split it
  // into two triangles by connecting vertex 0 to vertex 2.
  //
  // We handle this type of situation by moving the origin of the triangle fan
  // whenever we are about to create an unstable edge.  We choose a new
  // location for the origin such that all relevant edges are stable.  We also
  // create extra triangles with the appropriate orientation so that the sum
  // of the triangle signs is still correct at every point.

  // The maximum length of an edge for it to be considered numerically stable.
  // The exact value is fairly arbitrary since it depends on the stability of
  // the "f_tri" function.  The value below is quite conservative but could be
  // reduced further if desired.
  static const double kMaxLength = M_PI - 1e-5;

  // The default constructor for T must initialize the value to zero.
  // (This is true for built-in types such as "double".)
  T sum = T();
  if (loop.size() < 3) return sum;

  S2Point origin = loop[0];
  for (int i = 1; i + 1 < loop.size(); ++i) {
    // Let V_i be loop[i], let O be the current origin, and let length(A,B)
    // be the length of edge (A,B).  At the start of each loop iteration, the
    // "leading edge" of the triangle fan is (O,V_i), and we want to extend
    // the triangle fan so that the leading edge is (O,V_i+1).
    //
    // Invariants:
    //  1. length(O,V_i) < kMaxLength for all (i > 1).
    //  2. Either O == V_0, or O is approximately perpendicular to V_0.
    //  3. "sum" is the oriented integral of f over the area defined by
    //     (O, V_0, V_1, ..., V_i).
    S2_DCHECK(i == 1 || origin.Angle(loop[i]) < kMaxLength);
    S2_DCHECK(origin == loop[0] || std::fabs(origin.DotProd(loop[0])) < 1e-15);

    if (loop[i + 1].Angle(origin) > kMaxLength) {
      // We are about to create an unstable edge, so choose a new origin O'
      // for the triangle fan.
      S2Point old_origin = origin;
      if (origin == loop[0]) {
        // The following point is well-separated from V_i and V_0 (and
        // therefore V_i+1 as well).
        origin = S2::RobustCrossProd(loop[0], loop[i]).Normalize();
      } else if (loop[i].Angle(loop[0]) < kMaxLength) {
        // All edges of the triangle (O, V_0, V_i) are stable, so we can
        // revert to using V_0 as the origin.
        origin = loop[0];
      } else {
        // (O, V_i+1) and (V_0, V_i) are antipodal pairs, and O and V_0 are
        // perpendicular.  Therefore V_0.CrossProd(O) is approximately
        // perpendicular to all of {O, V_0, V_i, V_i+1}, and we can choose
        // this point O' as the new origin.
        origin = loop[0].CrossProd(old_origin);

        // Advance the edge (V_0,O) to (V_0,O').
        sum += f_tri(loop[0], old_origin, origin);
      }
      // Advance the edge (O,V_i) to (O',V_i).
      sum += f_tri(old_origin, loop[i], origin);
    }
    // Advance the edge (O,V_i) to (O,V_i+1).
    sum += f_tri(origin, loop[i], loop[i+1]);
  }
  // If the origin is not V_0, we need to sum one more triangle.
  if (origin != loop[0]) {
    // Advance the edge (O,V_n-1) to (O,V_0).
    sum += f_tri(origin, loop[loop.size() - 1], loop[0]);
  }
  return sum;
}

}  // namespace S2

#endif  // S2_S2LOOP_MEASURES_H_
