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
// There are several notions of the "centroid" of a triangle.  First, there
// is the planar centroid, which is simply the centroid of the ordinary
// (non-spherical) triangle defined by the three vertices.  Second, there is
// the surface centroid, which is defined as the intersection of the three
// medians of the spherical triangle.  It is possible to show that this
// point is simply the planar centroid projected to the surface of the
// sphere.  Finally, there is the true centroid (mass centroid), which is
// defined as the surface integral over the spherical triangle of (x,y,z)
// divided by the triangle area.  This is the point that the triangle would
// rotate around if it was spinning in empty space.
//
// The best centroid for most purposes is the true centroid.  Unlike the
// planar and surface centroids, the true centroid behaves linearly as
// regions are added or subtracted.  That is, if you split a triangle into
// pieces and compute the average of their centroids (weighted by triangle
// area), the result equals the centroid of the original triangle.  This is
// not true of the other centroids.
//
// Also note that the surface centroid may be nowhere near the intuitive
// "center" of a spherical triangle.  For example, consider the triangle
// with vertices A=(1,eps,0), B=(0,0,1), C=(-1,eps,0) (a quarter-sphere).
// The surface centroid of this triangle is at S=(0, 2*eps, 1), which is
// within a distance of 2*eps of the vertex B.  Note that the median from A
// (the segment connecting A to the midpoint of BC) passes through S, since
// this is the shortest path connecting the two endpoints.  On the other
// hand, the true centroid is at M=(0, 0.5, 0.5), which when projected onto
// the surface is a much more reasonable interpretation of the "center" of
// this triangle.

#ifndef S2_S2CENTROIDS_H_
#define S2_S2CENTROIDS_H_

#include "s2/s2point.h"

namespace S2 {

// Returns the centroid of the planar triangle ABC.  This can be normalized to
// unit length to obtain the "surface centroid" of the corresponding spherical
// triangle, i.e. the intersection of the three medians.  However, note that
// for large spherical triangles the surface centroid may be nowhere near the
// intuitive "center" (see example above).
S2Point PlanarCentroid(const S2Point& a, const S2Point& b, const S2Point& c);

// Returns the true centroid of the spherical triangle ABC multiplied by the
// signed area of spherical triangle ABC.  The reasons for multiplying by the
// signed area are (1) this is the quantity that needs to be summed to compute
// the centroid of a union or difference of triangles, and (2) it's actually
// easier to calculate this way.  All points must have unit length.
//
// Note that the result of this function is defined to be S2Point(0, 0, 0) if
// the triangle is degenerate (and that this is intended behavior).
S2Point TrueCentroid(const S2Point& a, const S2Point& b, const S2Point& c);

// Returns the true centroid of the spherical geodesic edge AB multiplied by
// the length of the edge AB.  As with triangles, the true centroid of a
// collection of edges may be computed simply by summing the result of this
// method for each edge.
//
// Note that the planar centroid of a geodesic edge simply 0.5 * (a + b),
// while the surface centroid is (a + b).Normalize().  However neither of
// these values is appropriate for computing the centroid of a collection of
// edges (such as a polyline).
//
// Also note that the result of this function is defined to be S2Point(0, 0, 0)
// if the edge is degenerate (and that this is intended behavior).
S2Point TrueCentroid(const S2Point& a, const S2Point& b);

}  // namespace S2

#endif  // S2_S2CENTROIDS_H_
