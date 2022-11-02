// Copyright 2013 Google Inc. All Rights Reserved.
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

#ifndef S2_S2SHAPEUTIL_GET_REFERENCE_POINT_H_
#define S2_S2SHAPEUTIL_GET_REFERENCE_POINT_H_

#include "s2/s2shape_index.h"

namespace s2shapeutil {

// This is a helper function for implementing S2Shape::GetReferencePoint().
//
// Given a shape consisting of closed polygonal loops, the interior of the
// shape is defined as the region to the left of all edges (which must be
// oriented consistently).  This function then chooses an arbitrary point and
// returns true if that point is contained by the shape.
//
// Unlike S2Loop and S2Polygon, this method allows duplicate vertices and
// edges, which requires some extra care with definitions.  The rule that we
// apply is that an edge and its reverse edge "cancel" each other: the result
// is the same as if that edge pair were not present.  Therefore shapes that
// consist only of degenerate loop(s) are either empty or full; by convention,
// the shape is considered full if and only if it contains an empty loop (see
// S2LaxPolygonShape for details).
//
// Determining whether a loop on the sphere contains a point is harder than
// the corresponding problem in 2D plane geometry.  It cannot be implemented
// just by counting edge crossings because there is no such thing as a "point
// at infinity" that is guaranteed to be outside the loop.
S2Shape::ReferencePoint GetReferencePoint(const S2Shape& shape);

}  // namespace s2shapeutil

#endif  // S2_S2SHAPEUTIL_GET_REFERENCE_POINT_H_
