// Copyright 2022 Google Inc. All Rights Reserved.
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

#include "s2/s2hausdorff_distance_query.h"

#include <vector>

#include "absl/types/optional.h"
#include "s2/s1chord_angle.h"
#include "s2/s2closest_edge_query.h"
#include "s2/s2point.h"

using DirectedResult = S2HausdorffDistanceQuery::DirectedResult;
using Options = S2HausdorffDistanceQuery::Options;
using Result = S2HausdorffDistanceQuery::Result;

namespace {
// This internally used function computes the closest edge distance from point
// to the source index via closest_edge_query, and, if necessary, updates the
// max_distance, the target_point and the source_point.
void UpdateMaxDistance(const S2Point& point,
                       S2ClosestEdgeQuery& closest_edge_query,
                       S1ChordAngle& max_distance, S2Point& target_point,
                       S2Point& source_point) {
  // In case we already have a valid result, it can be used as the lower
  // bound estimate for the final Hausdorff distance. Therefore, if the
  // distance between the current target point and the last source point
  // does not exceed this lower bound, we can safely skip this target point, not
  // updating the maximim distance.
  if (!max_distance.is_negative() &&
      s2pred::CompareDistance(point, source_point, max_distance) <= 0) {
    return;
  }

  // Find the closest edge and the closest point in the source geometry
  // to the target point.
  S2ClosestEdgeQuery::PointTarget target(point);
  const S2ClosestEdgeQuery::Result closest_edge =
      closest_edge_query.FindClosestEdge(&target);
  if (!closest_edge.is_empty() && max_distance < closest_edge.distance()) {
    max_distance = closest_edge.distance();
    target_point = point;
    source_point = closest_edge_query.Project(point, closest_edge);
  }
}
}  // namespace

S2HausdorffDistanceQuery::S2HausdorffDistanceQuery(
    const S2HausdorffDistanceQuery::Options& options) {
  Init(options);
}

void S2HausdorffDistanceQuery::Init(
    const S2HausdorffDistanceQuery::Options& options) {
  options_ = options;
}

S1ChordAngle S2HausdorffDistanceQuery::GetDistance(
    const S2ShapeIndex* target, const S2ShapeIndex* source) const {
  absl::optional<Result> result = GetResult(target, source);
  return result.has_value() ? result->distance() : S1ChordAngle::Infinity();
}

absl::optional<Result> S2HausdorffDistanceQuery::GetResult(
    const S2ShapeIndex* target, const S2ShapeIndex* source) const {
  absl::optional<DirectedResult> target_to_source =
      GetDirectedResult(target, source);
  if (target_to_source) {
    return Result(*target_to_source, *GetDirectedResult(source, target));
  } else {
    return absl::nullopt;
  }
}

S1ChordAngle S2HausdorffDistanceQuery::GetDirectedDistance(
    const S2ShapeIndex* target, const S2ShapeIndex* source) const {
  absl::optional<DirectedResult> directed_result =
      GetDirectedResult(target, source);
  return directed_result.has_value() ? directed_result->distance()
                                     : S1ChordAngle::Infinity();
}

absl::optional<DirectedResult> S2HausdorffDistanceQuery::GetDirectedResult(
    const S2ShapeIndex* target, const S2ShapeIndex* source) const {
  S2ClosestEdgeQuery closest_edge_query(source);
  closest_edge_query.mutable_options()->set_max_results(1);
  closest_edge_query.mutable_options()->set_include_interiors(
      options_.include_interiors());
  S1ChordAngle max_distance = S1ChordAngle::Negative();
  S2Point source_point, target_point;

  // This approximation of Haussdorff distance is based on computing closest
  // point distances from the _vertices_ of the target index to _edges_ of the
  // source index.  Hence we iterate over all shapes in the target index, then
  // over all chains in those shapes, then over all edges in those chains, and
  // then over the edges' vertices.
  for (const S2Shape* shape : *target) {
    for (int chain_id = 0; chain_id < shape->num_chains(); ++chain_id) {
      const int chain_length = shape->chain(chain_id).length;
      // We include the first vertex (v0) of an edge only if this is the first
      // edge of a polyline. For point shapes (dim == 0) the first vertex is not
      // needed since it coincides with the second vertex (v1). For polygon
      // shapes (dim == 2) the first vertex is not needed since it coincides
      // with the second vertex of the previous edge (or of the last) edge.
      // TODO(b/212844787): Avoid loading vertices twice by using chain vertex
      // iterators.
      bool include_first_vertex = shape->dimension() == 1;
      for (int offset = 0; offset < chain_length; ++offset) {
        const S2Shape::Edge edge = shape->chain_edge(chain_id, offset);

        if (include_first_vertex) {
          UpdateMaxDistance(edge.v0, closest_edge_query, max_distance,
                            target_point, source_point);
          include_first_vertex = false;
        }
        UpdateMaxDistance(edge.v1, closest_edge_query, max_distance,
                          target_point, source_point);
      }
    }
  }

  if (max_distance.is_negative()) {
    return absl::nullopt;
  } else {
    return DirectedResult(max_distance, target_point);
  }
}
