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
// Defines a collection of functions for computing the distance to an edge,
// interpolating along an edge, projecting points onto edges, etc.

#ifndef S2_S2EDGE_DISTANCES_H_
#define S2_S2EDGE_DISTANCES_H_

#include <utility>

#include "s2/s1angle.h"
#include "s2/s1chord_angle.h"
#include "s2/s2point.h"

namespace S2 {

/////////////////////////////////////////////////////////////////////////////
///////////////            (point, edge) functions            ///////////////

// Returns the minimum distance from X to any point on the edge AB.  All
// arguments should be unit length.  The result is very accurate for small
// distances but may have some numerical error if the distance is large
// (approximately Pi/2 or greater).  The case A == B is handled correctly.
//
// If you want to compare a distance against a fixed threshold, e.g.
//    if (S2::GetDistance(x, a, b) < limit)
// then it is significantly faster to use UpdateMinDistance() below.
S1Angle GetDistance(const S2Point& x, const S2Point& a, const S2Point& b);

// Returns true if the distance from X to the edge AB is less than "limit".
// (Specify limit.Successor() for "less than or equal to".)  This method is
// significantly faster than GetDistance().  If you want to compare against a
// fixed S1Angle, you should convert it to an S1ChordAngle once and save the
// value, since this step is relatively expensive.
//
// See s2pred::CompareEdgeDistance() for an exact version of this predicate.
bool IsDistanceLess(const S2Point& x, const S2Point& a, const S2Point& b,
                    S1ChordAngle limit);

// If the distance from X to the edge AB is less than "min_dist", this
// method updates "min_dist" and returns true.  Otherwise it returns false.
// The case A == B is handled correctly.
//
// Use this method when you want to compute many distances and keep track of
// the minimum.  It is significantly faster than using GetDistance(),
// because (1) using S1ChordAngle is much faster than S1Angle, and (2) it
// can save a lot of work by not actually computing the distance when it is
// obviously larger than the current minimum.
bool UpdateMinDistance(const S2Point& x, const S2Point& a, const S2Point& b,
                       S1ChordAngle* min_dist);

// If the distance from X to the edge AB is greater than "max_dist", this
// method updates "max_dist" and returns true.  Otherwise it returns false.
// The case A == B is handled correctly.
bool UpdateMaxDistance(const S2Point& x, const S2Point& a, const S2Point& b,
                       S1ChordAngle* max_dist);

// Returns the maximum error in the result of UpdateMinDistance (and
// associated functions such as UpdateMinInteriorDistance, IsDistanceLess,
// etc), assuming that all input points are normalized to within the bounds
// guaranteed by S2Point::Normalize().  The error can be added or subtracted
// from an S1ChordAngle "x" using x.PlusError(error).
//
// Note that accuracy goes down as the distance approaches 0 degrees or 180
// degrees (for different reasons).  Near 0 degrees the error is acceptable
// for all practical purposes (about 1.2e-15 radians ~= 8 nanometers).  For
// exactly antipodal points the maximum error is quite high (0.5 meters), but
// this error drops rapidly as the points move away from antipodality
// (approximately 1 millimeter for points that are 50 meters from antipodal,
// and 1 micrometer for points that are 50km from antipodal).
//
// TODO(ericv): Currently the error bound does not hold for edges whose
// endpoints are antipodal to within about 1e-15 radians (less than 1 micron).
// This could be fixed by extending S2::RobustCrossProd to use higher
// precision when necessary.
double GetUpdateMinDistanceMaxError(S1ChordAngle dist);

// Returns true if the minimum distance from X to the edge AB is attained at
// an interior point of AB (i.e., not an endpoint), and that distance is less
// than "limit".  (Specify limit.Successor() for "less than or equal to".)
bool IsInteriorDistanceLess(const S2Point& x,
                            const S2Point& a, const S2Point& b,
                            S1ChordAngle limit);

// If the minimum distance from X to AB is attained at an interior point of AB
// (i.e., not an endpoint), and that distance is less than "min_dist", then
// this method updates "min_dist" and returns true.  Otherwise returns false.
bool UpdateMinInteriorDistance(const S2Point& x,
                               const S2Point& a, const S2Point& b,
                               S1ChordAngle* min_dist);

// Returns the point along the edge AB that is closest to the point X.
// The fractional distance of this point along the edge AB can be obtained
// using GetDistanceFraction() above.  Requires that all vectors have
// unit length.
S2Point Project(const S2Point& x, const S2Point& a, const S2Point& b);

// A slightly more efficient version of Project() where the cross product of
// the two endpoints has been precomputed.  The cross product does not need to
// be normalized, but should be computed using S2::RobustCrossProd() for the
// most accurate results.  Requires that x, a, and b have unit length.
S2Point Project(const S2Point& x, const S2Point& a, const S2Point& b,
                const Vector3_d& a_cross_b);


/////////////////////////////////////////////////////////////////////////////
///////////////         (point along edge) functions          ///////////////


// Given a point X and an edge AB, returns the distance ratio AX / (AX + BX).
// If X happens to be on the line segment AB, this is the fraction "t" such
// that X == Interpolate(t, A, B).  Requires that A and B are distinct.
double GetDistanceFraction(const S2Point& x,
                           const S2Point& a, const S2Point& b);

// Returns the point X along the line segment AB whose distance from A is the
// given fraction "t" of the distance AB.  Does NOT require that "t" be
// between 0 and 1.  Note that all distances are measured on the surface of
// the sphere, so this is more complicated than just computing (1-t)*a + t*b
// and normalizing the result.
S2Point Interpolate(double t, const S2Point& a, const S2Point& b);

// Like Interpolate(), except that the parameter "ax" represents the desired
// distance from A to the result X rather than a fraction between 0 and 1.
S2Point InterpolateAtDistance(S1Angle ax, const S2Point& a, const S2Point& b);


/////////////////////////////////////////////////////////////////////////////
///////////////            (edge, edge) functions             ///////////////


// Like UpdateMinDistance(), but computes the minimum distance between the
// given pair of edges.  (If the two edges cross, the distance is zero.)
// The cases a0 == a1 and b0 == b1 are handled correctly.
bool UpdateEdgePairMinDistance(const S2Point& a0, const S2Point& a1,
                               const S2Point& b0, const S2Point& b1,
                               S1ChordAngle* min_dist);

// As above, but for maximum distances.  If one edge crosses the antipodal
// reflection of the other, the distance is Pi.
bool UpdateEdgePairMaxDistance(const S2Point& a0, const S2Point& a1,
                               const S2Point& b0, const S2Point& b1,
                               S1ChordAngle* max_dist);

// Returns the pair of points (a, b) that achieves the minimum distance
// between edges a0a1 and b0b1, where "a" is a point on a0a1 and "b" is a
// point on b0b1.  If the two edges intersect, "a" and "b" are both equal to
// the intersection point.  Handles a0 == a1 and b0 == b1 correctly.
std::pair<S2Point, S2Point> GetEdgePairClosestPoints(
    const S2Point& a0, const S2Point& a1,
    const S2Point& b0, const S2Point& b1);

// Returns true if every point on edge B=b0b1 is no further than "tolerance"
// from some point on edge A=a0a1.  Equivalently, returns true if the directed
// Hausdorff distance from B to A is no more than "tolerance".
// Requires that tolerance is less than 90 degrees.
bool IsEdgeBNearEdgeA(const S2Point& a0, const S2Point& a1,
                      const S2Point& b0, const S2Point& b1,
                      S1Angle tolerance);


//////////////////   Implementation details follow   ////////////////////


inline bool IsDistanceLess(const S2Point& x, const S2Point& a,
                           const S2Point& b, S1ChordAngle limit) {
  return UpdateMinDistance(x, a, b, &limit);
}

inline bool IsInteriorDistanceLess(const S2Point& x, const S2Point& a,
                                   const S2Point& b, S1ChordAngle limit) {
  return UpdateMinInteriorDistance(x, a, b, &limit);
}

}  // namespace S2

#endif  // S2_S2EDGE_DISTANCES_H_
