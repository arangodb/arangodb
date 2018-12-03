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

#include "s2/s2shapeutil_contains_brute_force.h"

#include <utility>
#include "s2/s2edge_crosser.h"

namespace s2shapeutil {

bool ContainsBruteForce(const S2Shape& shape, const S2Point& point) {
  if (shape.dimension() < 2) return false;

  S2Shape::ReferencePoint ref_point = shape.GetReferencePoint();
  if (ref_point.point == point) return ref_point.contained;

  S2CopyingEdgeCrosser crosser(ref_point.point, point);
  bool inside = ref_point.contained;
  for (int e = 0; e < shape.num_edges(); ++e) {
    auto edge = shape.edge(e);
    inside ^= crosser.EdgeOrVertexCrossing(edge.v0, edge.v1);
  }
  return inside;
}

}  // namespace s2shapeutil
