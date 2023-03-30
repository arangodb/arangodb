// Copyright 2021 Google Inc. All Rights Reserved.
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

#include "s2/s2shapeutil_conversion.h"

#include <memory>
#include <utility>

#include "s2/s2shape_measures.h"

namespace s2shapeutil {

using absl::make_unique;

std::vector<S2Point> ShapeToS2Points(const S2Shape& multipoint) {
  S2_DCHECK_EQ(multipoint.dimension(), 0);
  std::vector<S2Point> points;
  points.reserve(multipoint.num_edges());
  for (int i = 0; i < multipoint.num_edges(); ++i) {
    points.push_back(multipoint.edge(i).v0);
  }
  return points;
}

std::unique_ptr<S2Polyline> ShapeToS2Polyline(const S2Shape& line) {
  S2_DCHECK_EQ(line.dimension(), 1);
  S2_DCHECK_EQ(line.num_chains(), 1);
  std::vector<S2Point> vertices;
  S2::GetChainVertices(line, 0, &vertices);
  return make_unique<S2Polyline>(std::move(vertices));
}

std::unique_ptr<S2Polygon> ShapeToS2Polygon(const S2Shape& poly) {
  if (poly.is_full()) {
    return make_unique<S2Polygon>(make_unique<S2Loop>(S2Loop::kFull()));
  }
  S2_DCHECK_EQ(poly.dimension(), 2);
  std::vector<std::unique_ptr<S2Loop>> loops;
  std::vector<S2Point> vertices;
  for (int i = 0; i < poly.num_chains(); ++i) {
    S2::GetChainVertices(poly, i, &vertices);
    loops.push_back(make_unique<S2Loop>(vertices));
  }
  auto output_poly = make_unique<S2Polygon>();
  output_poly->InitOriented(std::move(loops));

  return output_poly;
}

}  // namespace s2shapeutil
