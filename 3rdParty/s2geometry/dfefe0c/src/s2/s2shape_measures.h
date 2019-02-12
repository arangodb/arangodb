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
// Defines various angle and area measures for S2Shape objects.  Unlike the
// built-in S2Polygon and S2Polyline methods, these methods allow the
// underlying data to be represented arbitrarily.

#ifndef S2_S2SHAPE_MEASURES_H_
#define S2_S2SHAPE_MEASURES_H_

#include <vector>

#include "s2/s1angle.h"
#include "s2/s2point.h"
#include "s2/s2shape.h"

namespace S2 {

// For shapes of dimension 1, returns the sum of all polyline lengths on the
// unit sphere.  Otherwise returns zero.  (See GetPerimeter for shapes of
// dimension 2.)
//
// All edges are modeled as spherical geodesics.  The result can be converted
// to a distance on the Earth's surface (with a worst-case error of 0.562%
// near the equator) using the functions in s2earth.h.
S1Angle GetLength(const S2Shape& shape);

// For shapes of dimension 2, returns the sum of all loop perimeters on the
// unit sphere.  Otherwise returns zero.  (See GetLength for shapes of
// dimension 1.)
//
// All edges are modeled as spherical geodesics.  The result can be converted
// to a distance on the Earth's surface (with a worst-case error of 0.562%
// near the equator) using the functions in s2earth.h.
S1Angle GetPerimeter(const S2Shape& shape);

// For shapes of dimension 2, returns the area of the shape on the unit
// sphere.  The result is between 0 and 4*Pi steradians.  Otherwise returns
// zero.  This method has good relative accuracy for both very large and very
// small regions.
//
// All edges are modeled as spherical geodesics.  The result can be converted
// to an area on the Earth's surface (with a worst-case error of 0.900% near
// the poles) using the functions in s2earth.h.
double GetArea(const S2Shape& shape);

// Like GetArea(), except that this method is faster and has more error.  The
// additional error is at most 2.22e-15 steradians per vertex, which works out
// to about 0.09 square meters per vertex on the Earth's surface.  For
// example, a loop with 100 vertices has a maximum error of about 9 square
// meters.  (The actual error is typically much smaller than this.)
double GetApproxArea(const S2Shape& shape);

// Returns the centroid of the shape multiplied by the measure of the shape,
// which is defined as follows:
//
//  - For dimension 0 shapes, the measure is shape->num_edges().
//  - For dimension 1 shapes, the measure is GetLength(shape).
//  - For dimension 2 shapes, the measure is GetArea(shape).
//
// Note that the result is not unit length, so you may need to call
// Normalize() before passing it to other S2 functions.
//
// The result is scaled by the measure defined above for two reasons: (1) it
// is cheaper to compute this way, and (2) this makes it easier to compute the
// centroid of a collection of shapes.  (This requires simply summing the
// centroids of all shapes in the collection whose dimension is maximal.)
S2Point GetCentroid(const S2Shape& shape);

// Overwrites "vertices" with the vertices of the given edge chain of "shape".
// If dimension == 1, the chain will have (chain.length + 1) vertices, and
// otherwise it will have (chain.length) vertices.
//
// This is a low-level helper method used in the implementations of some of
// the methods above.
void GetChainVertices(const S2Shape& shape, int chain_id,
                      std::vector<S2Point>* vertices);

}  // namespace S2

#endif  // S2_S2SHAPE_MEASURES_H_
