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

#ifndef S2_S2CONTAINS_VERTEX_QUERY_H_
#define S2_S2CONTAINS_VERTEX_QUERY_H_

#include "s2/s2point.h"
#include "s2/util/gtl/btree_map.h"

// This class determines whether a polygon contains one of its vertices given
// the edges incident to that vertex.  The result is +1 if the vertex is
// contained, -1 if it is not contained, and 0 if the incident edges consist
// of matched sibling pairs (in which case the result cannot be determined
// locally).
//
// Point containment is defined according to the "semi-open" boundary model
// (see S2VertexModel), which means that if several polygons tile the region
// around a vertex, then exactly one of those polygons contains that vertex.
//
// This class is not thread-safe.  To use it in parallel, each thread should
// construct its own instance (this is not expensive).
class S2ContainsVertexQuery {
 public:
  // "target" is the vertex whose containment will be determined.
  explicit S2ContainsVertexQuery(const S2Point& target);

  // Indicates that the polygon has an edge between "target" and "v" in the
  // given direction (+1 = outgoing, -1 = incoming, 0 = degenerate).
  void AddEdge(const S2Point& v, int direction);

  // Returns +1 if the vertex is contained, -1 if it is not contained, and 0
  // if the incident edges consisted of matched sibling pairs.
  int ContainsSign();

 private:
  S2Point target_;
  gtl::btree_map<S2Point, int> edge_map_;
};


//////////////////   Implementation details follow   ////////////////////


inline S2ContainsVertexQuery::S2ContainsVertexQuery(const S2Point& target)
    : target_(target) {
}

inline void S2ContainsVertexQuery::AddEdge(const S2Point& v, int direction) {
  edge_map_[v] += direction;
}

#endif  // S2_S2CONTAINS_VERTEX_QUERY_H_
