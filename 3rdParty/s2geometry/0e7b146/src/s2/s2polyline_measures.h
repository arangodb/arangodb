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
// Defines various measures for polylines on the sphere.  These are low-level
// methods that work directly with arrays of S2Points.  They are used to
// implement the methods in s2shapeindex_measures.h, s2shape_measures.h, and
// s2polyline.h.
//
// See s2loop_measures.h, s2edge_distances.h, and s2measures.h for additional
// low-level methods.

#ifndef S2_S2POLYLINE_MEASURES_H_
#define S2_S2POLYLINE_MEASURES_H_

#include "s2/s1angle.h"
#include "s2/s2point.h"
#include "s2/s2point_span.h"

namespace S2 {

// Returns the length of the polyline.  Returns zero for polylines with fewer
// than two vertices.
S1Angle GetLength(S2PointSpan polyline);

// Returns the true centroid of the polyline multiplied by the length of the
// polyline (see s2centroids.h for details on centroids).  The result is not
// unit length, so you may want to normalize it.
//
// Scaling by the polyline length makes it easy to compute the centroid of
// several polylines (by simply adding up their centroids).
//
// CAVEAT: Returns S2Point() for degenerate polylines (e.g., AA).  [Note that
// this answer is correct; the result of this function is a line integral over
// the polyline, whose value is always zero if the polyline is degenerate.]
S2Point GetCentroid(S2PointSpan polyline);

}  // namespace S2

#endif  // S2_S2POLYLINE_MEASURES_H_
