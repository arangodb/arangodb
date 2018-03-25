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
#include "s2/third_party/absl/base/macros.h"
#include "s2/third_party/absl/container/inlined_vector.h"
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
#include "s2/util/math/vector.h"

namespace S2 {

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

// Given two edges AB and CD where at least two vertices are identical
// (i.e. CrossingSign(a,b,c,d) == 0), this function defines whether the
// two edges "cross" in a such a way that point-in-polygon containment tests
// can be implemented by counting the number of edge crossings.  The basic
// rule is that a "crossing" occurs if AB is encountered after CD during a
// CCW sweep around the shared vertex starting from a fixed reference point.
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
extern const S1Angle kIntersectionError;

// This value can be used as the S2Builder snap_radius() to ensure that edges
// that have been displaced by up to kIntersectionError are merged back
// together again.  For example this can happen when geometry is intersected
// with a set of tiles and then unioned.  It is equal to twice the
// intersection error because input edges might have been displaced in
// opposite directions.
extern const S1Angle kIntersectionMergeRadius;  // 2 * kIntersectionError

}  // namespace S2

#endif  // S2_S2EDGE_CROSSINGS_H_
