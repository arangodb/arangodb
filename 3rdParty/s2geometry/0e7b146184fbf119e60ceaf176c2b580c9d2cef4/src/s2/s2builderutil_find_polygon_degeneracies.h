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

#ifndef S2_S2BUILDERUTIL_FIND_POLYGON_DEGENERACIES_H_
#define S2_S2BUILDERUTIL_FIND_POLYGON_DEGENERACIES_H_

#include <vector>

#include "s2/base/integral_types.h"
#include "s2/s2builder_graph.h"
#include "s2/s2error.h"

namespace s2builderutil {

// A polygon degeneracy is either a degenerate edge (an edge from a vertex to
// itself) or a sibling edge pair (consisting of an edge and its corresponding
// reverse edge).  "is_hole" indicates whether the degeneracy corresponds to a
// polygon hole (as opposed to a polygon shell).
//
// Degeneracies are not allowed to coincide with any non-degenerate portions
// of the polygon's boundary (since that would make it impossible to classify
// the degeneracy as a shell or hole).  Specifically, degenerate edges must
// coincide only with other degenerate edges, and sibling pairs must coincide
// only with other sibling pairs.  (Below we require a slightly stronger
// condition, namely that sibling pairs cannot coincide with any other edges.)
struct PolygonDegeneracy {
  uint32 edge_id : 31;
  uint32 is_hole : 1;

  PolygonDegeneracy() : edge_id(0), is_hole(false) {}
  PolygonDegeneracy(S2Builder::Graph::EdgeId _edge_id, bool _is_hole)
      : edge_id(_edge_id), is_hole(_is_hole) {
  }
  bool operator==(PolygonDegeneracy y) const {
    return edge_id == y.edge_id && is_hole == y.is_hole;
  }
  bool operator<(PolygonDegeneracy y) const {
    return edge_id < y.edge_id || (edge_id == y.edge_id && is_hole < y.is_hole);
  }
};

// Given a graph representing a polygon, finds all degenerate edges and
// sibling pairs and classifies them as being either shells or holes.  The
// result vector is sorted by edge id.
//
// REQUIRES: graph.options().edge_type() == DIRECTED
// REQUIRES: graph.options().sibling_pairs() == DISCARD_EXCESS (or DISCARD)
// REQUIRES: graph.options().degenerate_edges() == DISCARD_EXCESS (or DISCARD)
//
// Usually callers will want to specify SiblingPairs::DISCARD_EXCESS and
// DegenerateEdges::DISCARD_EXCESS in order to remove all redundant
// degeneracies.  DISCARD is also allowed in case you want to keep only one
// type of degeneracy (i.e., degenerate edges or sibling pairs).
//
// If the graph edges cannot be assembled into loops, the result is undefined.
// (An error may or may not be returned.)
std::vector<PolygonDegeneracy> FindPolygonDegeneracies(
    const S2Builder::Graph& graph, S2Error* error);

}  // namespace s2builderutil

#endif  // S2_S2BUILDERUTIL_FIND_POLYGON_DEGENERACIES_H_
