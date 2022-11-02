// Copyright 2015 Google Inc. All Rights Reserved.
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

#ifndef S2_S2CONVEX_HULL_QUERY_H_
#define S2_S2CONVEX_HULL_QUERY_H_

#include <memory>
#include <vector>

#include "s2/_fp_contract_off.h"
#include "s2/s2cap.h"
#include "s2/s2latlng_rect.h"
#include "s2/s2loop.h"
#include "s2/s2polygon.h"
#include "s2/s2polyline.h"

// S2ConvexHullQuery builds the convex hull of any collection of points,
// polylines, loops, and polygons.  It returns a single convex loop.
//
// The convex hull is defined as the smallest convex region on the sphere that
// contains all of your input geometry.  Recall that a region is "convex" if
// for every pair of points inside the region, the straight edge between them
// is also inside the region.  In our case, a "straight" edge is a geodesic,
// i.e. the shortest path on the sphere between two points.
//
// Containment of input geometry is defined as follows:
//
//  - Each input loop and polygon is contained by the convex hull exactly
//    (i.e., according to S2Polygon::Contains(S2Polygon)).
//
//  - Each input point is either contained by the convex hull or is a vertex
//    of the convex hull. (Recall that S2Loops do not necessarily contain their
//    vertices.)
//
//  - For each input polyline, the convex hull contains all of its vertices
//    according to the rule for points above.  (The definition of convexity
//    then ensures that the convex hull also contains the polyline edges.)
//
// To use this class, call the Add*() methods to add your input geometry, and
// then call GetConvexHull().  Note that GetConvexHull() does *not* reset the
// state; you can continue adding geometry if desired and compute the convex
// hull again.  If you want to start from scratch, simply declare a new
// S2ConvexHullQuery object (they are cheap to create).
//
// This class is not thread-safe.  There are no "const" methods.
class S2ConvexHullQuery {
 public:
  S2ConvexHullQuery();

  // Add a point to the input geometry.
  void AddPoint(const S2Point& point);

  // Add a polyline to the input geometry.
  void AddPolyline(const S2Polyline& polyline);

  // Add a loop to the input geometry.
  void AddLoop(const S2Loop& loop);

  // Add a polygon to the input geometry.
  void AddPolygon(const S2Polygon& polygon);

  // Compute a bounding cap for the input geometry provided.
  //
  // Note that this method does not clear the geometry; you can continue
  // adding to it and call this method again if desired.
  S2Cap GetCapBound();

  // Compute the convex hull of the input geometry provided.
  //
  // If there is no geometry, this method returns an empty loop containing no
  // points (see S2Loop::is_empty()).
  //
  // If the geometry spans more than half of the sphere, this method returns a
  // full loop containing the entire sphere (see S2Loop::is_full()).
  //
  // If the geometry contains 1 or 2 points, or a single edge, this method
  // returns a very small loop consisting of three vertices (which are a
  // superset of the input vertices).
  //
  // Note that this method does not clear the geometry; you can continue
  // adding to it and call this method again if desired.
  std::unique_ptr<S2Loop> GetConvexHull();

 private:
  void GetMonotoneChain(std::vector<S2Point>* output);
  std::unique_ptr<S2Loop> GetSinglePointLoop(const S2Point& p);
  std::unique_ptr<S2Loop> GetSingleEdgeLoop(const S2Point& a, const S2Point& b);

  S2LatLngRect bound_;
  std::vector<S2Point> points_;

  S2ConvexHullQuery(const S2ConvexHullQuery&) = delete;
  void operator=(const S2ConvexHullQuery&) = delete;
};

#endif  // S2_S2CONVEX_HULL_QUERY_H_
