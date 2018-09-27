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

// Common code for testing furthest/closest edge/point queries.

#ifndef S2_S2CLOSEST_EDGE_QUERY_TESTING_H_
#define S2_S2CLOSEST_EDGE_QUERY_TESTING_H_

#include <vector>

#include "s2/mutable_s2shape_index.h"
#include "s2/s1angle.h"
#include "s2/s2cap.h"
#include "s2/s2cell.h"
#include "s2/s2edge_distances.h"
#include "s2/s2edge_vector_shape.h"
#include "s2/s2loop.h"
#include "s2/s2metrics.h"
#include "s2/s2point.h"
#include "s2/s2point_vector_shape.h"
#include "s2/s2shapeutil_count_edges.h"
#include "s2/s2shapeutil_shape_edge_id.h"
#include "s2/s2testing.h"

namespace s2testing {

// An abstract class that adds edges to a MutableS2ShapeIndex for benchmarking.
class ShapeIndexFactory {
 public:
  virtual ~ShapeIndexFactory() {}

  // Requests that approximately "num_edges" edges located within the given
  // S2Cap bound should be added to "index".
  virtual void AddEdges(const S2Cap& index_cap, int num_edges,
                        MutableS2ShapeIndex* index) const = 0;
};

// Generates a regular loop that approximately fills the given S2Cap.
//
// Regular loops are nearly the worst case for distance calculations, since
// many edges are nearly equidistant from any query point that is not
// immediately adjacent to the loop.
class RegularLoopShapeIndexFactory : public ShapeIndexFactory {
 public:
  void AddEdges(const S2Cap& index_cap, int num_edges,
                MutableS2ShapeIndex* index) const override {
    index->Add(absl::make_unique<S2Loop::OwningShape>(S2Loop::MakeRegularLoop(
        index_cap.center(), index_cap.GetRadius(), num_edges)));
  }
};

// Generates a fractal loop that approximately fills the given S2Cap.
class FractalLoopShapeIndexFactory : public ShapeIndexFactory {
 public:
  void AddEdges(const S2Cap& index_cap, int num_edges,
                MutableS2ShapeIndex* index) const override {
    S2Testing::Fractal fractal;
    fractal.SetLevelForApproxMaxEdges(num_edges);
    index->Add(absl::make_unique<S2Loop::OwningShape>(
        fractal.MakeLoop(S2Testing::GetRandomFrameAt(index_cap.center()),
                         index_cap.GetRadius())));
  }
};

// Generates a cloud of points that approximately fills the given S2Cap.
class PointCloudShapeIndexFactory : public ShapeIndexFactory {
 public:
  void AddEdges(const S2Cap& index_cap, int num_edges,
                MutableS2ShapeIndex* index) const override {
    std::vector<S2Point> points;
    for (int i = 0; i < num_edges; ++i) {
      points.push_back(S2Testing::SamplePoint(index_cap));
    }
    index->Add(absl::make_unique<S2PointVectorShape>(std::move(points)));
  }
};

}  // namespace s2testing
#endif  // S2_S2CLOSEST_EDGE_QUERY_TESTING_H_
