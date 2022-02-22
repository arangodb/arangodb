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

#include "s2/s2shapeutil_get_reference_point.h"

#include <algorithm>

#include "s2/base/logging.h"
#include "s2/s2contains_vertex_query.h"

using std::vector;
using ReferencePoint = S2Shape::ReferencePoint;

namespace s2shapeutil {

// This is a helper function for GetReferencePoint() below.
//
// If the given vertex "vtest" is unbalanced (see definition below), sets
// "result" to a ReferencePoint indicating whther "vtest" is contained and
// returns true.  Otherwise returns false.
static bool GetReferencePointAtVertex(
    const S2Shape& shape, const S2Point& vtest, ReferencePoint* result) {
  // Let P be an unbalanced vertex.  Vertex P is defined to be inside the
  // region if the region contains a particular direction vector starting from
  // P, namely the direction S2::Ortho(P).  This can be calculated using
  // S2ContainsVertexQuery.
  S2ContainsVertexQuery contains_query(vtest);
  int n = shape.num_edges();
  for (int e = 0; e < n; ++e) {
    auto edge = shape.edge(e);
    if (edge.v0 == vtest) contains_query.AddEdge(edge.v1, 1);
    if (edge.v1 == vtest) contains_query.AddEdge(edge.v0, -1);
  }
  int contains_sign = contains_query.ContainsSign();
  if (contains_sign == 0) {
    return false;  // There are no unmatched edges incident to this vertex.
  }
  result->point = vtest;
  result->contained = contains_sign > 0;
  return true;
}

// See documentation in header file.
S2Shape::ReferencePoint GetReferencePoint(const S2Shape& shape) {
  S2_DCHECK_EQ(shape.dimension(), 2);
  if (shape.num_edges() == 0) {
    // A shape with no edges is defined to be full if and only if it
    // contains at least one chain.
    return ReferencePoint::Contained(shape.num_chains() > 0);
  }
  // Define a "matched" edge as one that can be paired with a corresponding
  // reversed edge.  Define a vertex as "balanced" if all of its edges are
  // matched. In order to determine containment, we must find an unbalanced
  // vertex.  Often every vertex is unbalanced, so we start by trying an
  // arbitrary vertex.
  auto edge = shape.edge(0);
  ReferencePoint result;
  if (GetReferencePointAtVertex(shape, edge.v0, &result)) {
    return result;
  }
  // That didn't work, so now we do some extra work to find an unbalanced
  // vertex (if any).  Essentially we gather a list of edges and a list of
  // reversed edges, and then sort them.  The first edge that appears in one
  // list but not the other is guaranteed to be unmatched.
  int n = shape.num_edges();
  vector<S2Shape::Edge> edges(n), rev_edges(n);
  for (int i = 0; i < n; ++i) {
    auto edge = shape.edge(i);
    edges[i] = edge;
    rev_edges[i] = S2Shape::Edge(edge.v1, edge.v0);
  }
  std::sort(edges.begin(), edges.end());
  std::sort(rev_edges.begin(), rev_edges.end());
  for (int i = 0; i < n; ++i) {
    if (edges[i] < rev_edges[i]) {  // edges[i] is unmatched
      S2_CHECK(GetReferencePointAtVertex(shape, edges[i].v0, &result));
      return result;
    }
    if (rev_edges[i] < edges[i]) {  // rev_edges[i] is unmatched
      S2_CHECK(GetReferencePointAtVertex(shape, rev_edges[i].v0, &result));
      return result;
    }
  }
  // All vertices are balanced, so this polygon is either empty or full except
  // for degeneracies.  By convention it is defined to be full if it contains
  // any chain with no edges.
  for (int i = 0; i < shape.num_chains(); ++i) {
    if (shape.chain(i).length == 0) return ReferencePoint::Contained(true);
  }
  return ReferencePoint::Contained(false);
}

}  // namespace s2shapeutil
