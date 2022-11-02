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

#ifndef S2_S2SHAPEUTIL_SHAPE_EDGE_H_
#define S2_S2SHAPEUTIL_SHAPE_EDGE_H_

#include "s2/s2point.h"
#include "s2/s2shape.h"
#include "s2/s2shapeutil_shape_edge_id.h"

namespace s2shapeutil {

// A class representing a ShapeEdgeId together with the two endpoints of that
// edge.  It should be passed by reference.
struct ShapeEdge {
 public:
  ShapeEdge() {}
  ShapeEdge(const S2Shape& shape, int32 edge_id);
  ShapeEdge(int32 shape_id, int32 edge_id, const S2Shape::Edge& edge);
  ShapeEdgeId id() const { return id_; }
  const S2Point& v0() const { return edge_.v0; }
  const S2Point& v1() const { return edge_.v1; }

 private:
  ShapeEdgeId id_;
  S2Shape::Edge edge_;
};


//////////////////   Implementation details follow   ////////////////////


inline ShapeEdge::ShapeEdge(const S2Shape& shape, int32 edge_id)
    : ShapeEdge(shape.id(), edge_id, shape.edge(edge_id)) {
}

inline ShapeEdge::ShapeEdge(int32 shape_id, int32 edge_id,
                            const S2Shape::Edge& edge)
    : id_(shape_id, edge_id), edge_(edge) {
}

}  // namespace s2shapeutil

#endif  // S2_S2SHAPEUTIL_SHAPE_EDGE_H_
