// Copyright 2017 Google Inc. All Rights Reserved.
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

#ifndef S2_S2EDGE_TESSELLATOR_H_
#define S2_S2EDGE_TESSELLATOR_H_

#include <vector>
#include "s2/r2.h"
#include "s2/s1chord_angle.h"
#include "s2/s2point.h"
#include "s2/s2projections.h"

// Given an edge in some 2D projection (e.g., Mercator), S2EdgeTessellator
// converts the edge into a chain of spherical geodesic edges such that the
// maximum distance between the original edge and the geodesic edge chain is
// at most "tolerance".  Similarly, it can convert a spherical geodesic edge
// into a chain of edges in a given 2D projection such that the maximum
// distance between the geodesic edge and the chain of projected edges is at
// most "tolerance".
class S2EdgeTessellator {
 public:
  // Constructs an S2EdgeTessellator using the given projection and error
  // tolerance.  The projection object must be valid for the entire lifetime
  // of this object.  (Projections are typically declared once and reused.)
  //
  // Method            | Input                  | Output
  // ------------------|------------------------|-----------------------
  // AppendProjected   | S2 geodesics           | Planar projected edges
  // AppendUnprojected | Planar projected edges | S2 geodesics
  S2EdgeTessellator(const S2::Projection* projection, S1Angle tolerance);

  // Converts the spherical geodesic edge AB to a chain of planar edges in the
  // given projection and appends the corresponding vertices to "vertices".
  //
  // This method can be called multiple times with the same output vector to
  // convert an entire polyline or loop.  All vertices of the first edge are
  // appended, but the first vertex of each subsequent edge is omitted (and
  // must match the last vertex of the previous edge).
  //
  // If the given projection has one or more coordinate axes that "wrap", then
  // every vertex's coordinates will be as close as possible to the previous
  // vertex's coordinates.  Note that this may yield vertices whose
  // coordinates are outside the usual range.  For example, tessellating the
  // edge (0:170, 0:-170) (in lat:lng notation) yields (0:170, 0:190).
  void AppendProjected(const S2Point& a, const S2Point& b,
                       std::vector<R2Point>* vertices) const;

  // Converts the planar edge AB in the given projection to a chain of
  // spherical geodesic edges and appends the vertices to "vertices".
  //
  // This method can be called multiple times with the same output vector to
  // convert an entire polyline or loop.  All vertices of the first edge are
  // appended, but the first vertex of each subsequent edge is omitted (and is
  // required to match that last vertex of the previous edge).
  //
  // Note that to construct an S2Loop, you must call vertices->pop_back() at
  // the very end to eliminate the duplicate first and last vertex.  Note also
  // that if the given projection involves coordinate "wrapping" (e.g. across
  // the 180 degree meridian) then the first and last vertices may not be
  // exactly the same.
  void AppendUnprojected(const R2Point& a, const R2Point& b,
                         std::vector<S2Point>* vertices) const;

  // Returns the minimum supported tolerance (which corresponds to a distance
  // less than one micrometer on the Earth's surface).
  static S1Angle kMinTolerance();

 private:
  void AppendUnprojected(const R2Point& pa, const S2Point& a,
                         const R2Point& pb, const S2Point& b,
                         std::vector<S2Point>* vertices) const;

  void AppendProjected(const R2Point& pa, const S2Point& a,
                       const R2Point& pb, const S2Point& b,
                       std::vector<R2Point>* vertices) const;

  R2Point WrapDestination(const R2Point& pa, const R2Point& pb) const;

  const S2::Projection& proj_;
  S1ChordAngle tolerance_;
  R2Point wrap_distance_;  // Local copy
};

#endif  // S2_S2EDGE_TESSELLATOR_H_
