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

#ifndef S2_S2SHAPEUTIL_VISIT_CROSSING_EDGE_PAIRS_H_
#define S2_S2SHAPEUTIL_VISIT_CROSSING_EDGE_PAIRS_H_

#include <functional>
#include "s2/s2crossing_edge_query.h"
#include "s2/s2shape_index.h"
#include "s2/s2shapeutil_shape_edge.h"

class S2Error;

namespace s2shapeutil {

// A function that is called with pairs of crossing edges.  The function may
// return false in order to request that the algorithm should be terminated,
// i.e. no further crossings are needed.
//
// "is_interior" indicates that the crossing is at a point interior to both
// edges (i.e., not at a vertex).  (The calling function already has this
// information and it is moderately expensive to recompute.)
using EdgePairVisitor = std::function<
  bool (const ShapeEdge& a, const ShapeEdge& b, bool is_interior)>;

// Visits all pairs of crossing edges in the given S2ShapeIndex, terminating
// early if the given EdgePairVisitor function returns false (in which case
// VisitCrossings returns false as well).  "type" indicates whether all
// crossings should be visited, or only interior crossings.
//
// CAVEAT: Crossings may be visited more than once.
bool VisitCrossingEdgePairs(const S2ShapeIndex& index, CrossingType type,
                            const EdgePairVisitor& visitor);

// Like the above, but visits all pairs of crossing edges where one edge comes
// from each S2ShapeIndex.
//
// CAVEAT: Crossings may be visited more than once.
bool VisitCrossingEdgePairs(const S2ShapeIndex& a_index,
                            const S2ShapeIndex& b_index,
                            CrossingType type, const EdgePairVisitor& visitor);

// Given an S2ShapeIndex containing a single polygonal shape (e.g., an
// S2Polygon or S2Loop), return true if any loop has a self-intersection
// (including duplicate vertices) or crosses any other loop (including vertex
// crossings and duplicate edges) and set "error" to a human-readable error
// message.  Otherwise return false and leave "error" unchanged.
//
// This method is used to implement the FindValidationError methods of S2Loop
// and S2Polygon.
//
// TODO(ericv): Add an option to support S2LaxPolygonShape rules (i.e.,
// duplicate vertices and edges are allowed, but loop crossings are not).
bool FindSelfIntersection(const S2ShapeIndex& index, S2Error* error);

}  // namespace s2shapeutil

#endif  // S2_S2SHAPEUTIL_VISIT_CROSSING_EDGE_PAIRS_H_
