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
// Defines functions related to determining whether two geodesic edges cross
// and for computing intersection points.
//
// The predicates CrossingSign(), VertexCrossing(), and EdgeOrVertexCrossing()
// are robust, meaning that they produce correct, consistent results even in
// pathological cases.  See s2predicates.h for additional robust predicates.
//
// See also S2EdgeCrosser (which efficiently tests an edge against a sequence
// of other edges) and S2CrossingEdgeQuery (which uses an index to speed up
// the process).

#ifndef S2_S2EDGE_CROSSINGS_H_
#define S2_S2EDGE_CROSSINGS_H_

#include <cmath>

#include "s2/base/logging.h"
#include "absl/base/macros.h"
#include "absl/container/inlined_vector.h"
#include "s2/_fp_contract_off.h"
#include "s2/r2.h"
#include "s2/r2rect.h"
#include "s2/s1angle.h"
#include "s2/s1chord_angle.h"
#include "s2/s1interval.h"
#include "s2/s2latlng.h"
#include "s2/s2latlng_rect.h"
#include "s2/s2pointutil.h"
#include "s2/s2predicates.h"
#include "s2/s2predicates_internal.h"
#include "s2/util/math/vector.h"

namespace S2 {

// Returns a vector whose direction is guaranteed to be very close to the exact
// mathematical cross product of the given unit-length vectors "a" and "b", but
// whose magnitude is arbitrary.  Unlike a.CrossProd(b), this statement is true
// even when "a" and "b" are very nearly parallel (i.e., a ~= b or a ~= -b).
// Specifically, the direction of the result vector differs from the exact cross
// product by at most kRobustCrossProdError radians (see below).
//
// When a == -b exactly, the result is consistent with the symbolic perturbation
// model used by S2::Sign (see s2predicates.h).  In other words, even antipodal
// point pairs have a consistent and well-defined edge between them.  (In fact
// this is true for any pair of distinct points whose vectors are parallel.)
//
// When a == b exactly, an arbitrary vector orthogonal to "a" is returned.
// [From a strict mathematical viewpoint it would be better to return (0, 0, 0),
// but this behavior helps to avoid special cases in client code.]
//
// This function has the following properties (RCP == RobustCrossProd):
//
//   (1) RCP(a, b) != 0 for all a, b
//   (2) RCP(b, a) == -RCP(a, b) unless a == b
//   (3) RCP(-a, b) == -RCP(a, b) unless a and b are exactly proportional
//   (4) RCP(a, -b) == -RCP(a, b) unless a and b are exactly proportional
//
// Note that if you want the result to be unit-length, you must call Normalize()
// explicitly.  (The result is always scaled such that Normalize() can be called
// without precision loss due to floating-point underflow.)
S2Point RobustCrossProd(const S2Point& a, const S2Point& b);

// kRobustCrossProdError is an upper bound on the angle between the vector
// returned by RobustCrossProd(a, b) and the true cross product of "a" and "b".
// Note that cases where "a" and "b" are exactly proportional but not equal
// (e.g. a = -b or a = (1+epsilon)*b) are handled using symbolic perturbations
// in order to ensure that the result is non-zero and consistent with S2::Sign.
constexpr S1Angle kRobustCrossProdError = S1Angle::Radians(6 * s2pred::DBL_ERR);

// This function determines whether the edge AB intersects the edge CD.
// Returns +1 if AB crosses CD at a point that is interior to both edges.
// Returns  0 if any two vertices from different edges are the same.
// Returns -1 otherwise.
//
// Note that if an edge is degenerate (A == B or C == D), the return value
// is 0 if two vertices from different edges are the same and -1 otherwise.
//
// Properties of CrossingSign:
//
//  (1) CrossingSign(b,a,c,d) == CrossingSign(a,b,c,d)
//  (2) CrossingSign(c,d,a,b) == CrossingSign(a,b,c,d)
//  (3) CrossingSign(a,b,c,d) == 0 if a==c, a==d, b==c, b==d
//  (3) CrossingSign(a,b,c,d) <= 0 if a==b or c==d (see above)
//
// This function implements an exact, consistent perturbation model such
// that no three points are ever considered to be collinear.  This means
// that even if you have 4 points A, B, C, D that lie exactly in a line
// (say, around the equator), C and D will be treated as being slightly to
// one side or the other of AB.  This is done in a way such that the
// results are always consistent (see s2pred::Sign).
//
// Note that if you want to check an edge against a collection of other edges,
// it is much more efficient to use an S2EdgeCrosser (see s2edge_crosser.h).
int CrossingSign(const S2Point& a, const S2Point& b,
                 const S2Point& c, const S2Point& d);

// Returns true if the angle ABC contains its vertex B.  Containment is
// defined such that if several polygons tile the region around a vertex, then
// exactly one of those polygons contains that vertex.  Returns false for
// degenerate angles of the form ABA.
//
// Note that this method is not sufficient to determine vertex containment in
// polygons with duplicate vertices (such as the polygon ABCADE).  Use
// S2ContainsVertexQuery for such polygons.  S2::AngleContainsVertex(a, b, c)
// is equivalent to using S2ContainsVertexQuery as follows:
//
//    S2ContainsVertexQuery query(b);
//    query.AddEdge(a, -1);  // incoming
//    query.AddEdge(c, 1);   // outgoing
//    return query.ContainsSign() > 0;
//
// Useful properties of AngleContainsVertex:
//
//  (1) AngleContainsVertex(a,b,a) == false
//  (2) AngleContainsVertex(a,b,c) == !AngleContainsVertex(c,b,a) unless a == c
//  (3) Given vertices v_1 ... v_k ordered cyclically CCW around vertex b,
//      AngleContainsVertex(v_{i+1}, b, v_i) is true for exactly one value of i.
//
// REQUIRES: a != b && b != c
bool AngleContainsVertex(const S2Point& a, const S2Point& b, const S2Point& c);

// Given two edges AB and CD where at least two vertices are identical
// (i.e. CrossingSign(a,b,c,d) == 0), this function defines whether the
// two edges "cross" in such a way that point-in-polygon containment tests can
// be implemented by counting the number of edge crossings.  The basic rule is
// that a "crossing" occurs if AB is encountered after CD during a CCW sweep
// around the shared vertex starting from a fixed reference point.
//
// Note that according to this rule, if AB crosses CD then in general CD
// does not cross AB.  However, this leads to the correct result when
// counting polygon edge crossings.  For example, suppose that A,B,C are
// three consecutive vertices of a CCW polygon.  If we now consider the edge
// crossings of a segment BP as P sweeps around B, the crossing number
// changes parity exactly when BP crosses BA or BC.
//
// Useful properties of VertexCrossing (VC):
//
//  (1) VC(a,a,c,d) == VC(a,b,c,c) == false
//  (2) VC(a,b,a,b) == VC(a,b,b,a) == true
//  (3) VC(a,b,c,d) == VC(a,b,d,c) == VC(b,a,c,d) == VC(b,a,d,c)
//  (3) If exactly one of a,b equals one of c,d, then exactly one of
//      VC(a,b,c,d) and VC(c,d,a,b) is true
//
// It is an error to call this method with 4 distinct vertices.
bool VertexCrossing(const S2Point& a, const S2Point& b,
                    const S2Point& c, const S2Point& d);

// Like VertexCrossing() but returns -1 if AB crosses CD from left to right,
// +1 if AB crosses CD from right to left, and 0 otherwise.  This implies that
// if CD bounds some region according to the "interior is on the left" rule,
// this function returns -1 when AB exits the region and +1 when AB enters.
//
// This is a helper method that allows computing the change in winding number
// from point A to point B by summing the signed edge crossings of AB with the
// edges of the loop(s) used to define the winding number.
//
// Useful properties of SignedVertexCrossing (SVC):
//
//  (1) SVC(a,a,c,d) == SVC(a,b,c,c) == 0
//  (2) SVC(a,b,a,b) == +1
//  (3) SVC(a,b,b,a) == -1
//  (6) SVC(a,b,c,d) == -SVC(a,b,d,c) == -SVC(b,a,c,d) == SVC(b,a,d,c)
//  (3) If exactly one of a,b equals one of c,d, then exactly one of
//      SVC(a,b,c,d) and SVC(c,d,a,b) is non-zero
//
// It is an error to call this method with 4 distinct vertices.
int SignedVertexCrossing(const S2Point& a, const S2Point& b,
                         const S2Point& c, const S2Point& d);

// A convenience function that calls CrossingSign() to handle cases
// where all four vertices are distinct, and VertexCrossing() to handle
// cases where two or more vertices are the same.  This defines a crossing
// function such that point-in-polygon containment tests can be implemented
// by simply counting edge crossings.
bool EdgeOrVertexCrossing(const S2Point& a, const S2Point& b,
                          const S2Point& c, const S2Point& d);

// Given two edges AB and CD such that CrossingSign(A, B, C, D) > 0, returns
// their intersection point.  Useful properties of GetIntersection (GI):
//
//  (1) GI(b,a,c,d) == GI(a,b,d,c) == GI(a,b,c,d)
//  (2) GI(c,d,a,b) == GI(a,b,c,d)
//
// The returned intersection point X is guaranteed to be very close to the
// true intersection point of AB and CD, even if the edges intersect at a
// very small angle.  See "kIntersectionError" below for details.
S2Point GetIntersection(const S2Point& a, const S2Point& b,
                        const S2Point& c, const S2Point& d);

// kIntersectionError is an upper bound on the distance from the intersection
// point returned by GetIntersection() to the true intersection point.
constexpr S1Angle kIntersectionError = S1Angle::Radians(8 * s2pred::DBL_ERR);

// This value can be used as the S2Builder snap_radius() to ensure that edges
// that have been displaced by up to kIntersectionError are merged back
// together again.  For example this can happen when geometry is intersected
// with a set of tiles and then unioned.  It is equal to twice the
// intersection error because input edges might have been displaced in
// opposite directions.
constexpr S1Angle kIntersectionMergeRadius = 2 * kIntersectionError;


//////////////////   Implementation details follow   ////////////////////


inline bool AngleContainsVertex(const S2Point& a, const S2Point& b,
                                const S2Point& c) {
  // A loop with consecutive vertices A, B, C contains vertex B if and only if
  // the fixed vector R = S2::RefDir(B) is contained by the wedge ABC.  The
  // wedge is closed at A and open at C, i.e. the point B is inside the loop
  // if A = R but not if C = R.
  //
  // Note that the test below is written so as to get correct results when the
  // angle ABC is degenerate.  If A = C or C = R it returns false, and
  // otherwise if A = R it returns true.
  S2_DCHECK(a != b && b != c);
  return !s2pred::OrderedCCW(S2::RefDir(b), c, a, b);
}

}  // namespace S2

#endif  // S2_S2EDGE_CROSSINGS_H_
