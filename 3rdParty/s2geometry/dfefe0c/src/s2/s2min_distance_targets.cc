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

#include "s2/s2min_distance_targets.h"

#include <memory>
#include "s2/third_party/absl/memory/memory.h"
#include "s2/s1angle.h"
#include "s2/s2cap.h"
#include "s2/s2cell.h"
#include "s2/s2closest_cell_query.h"
#include "s2/s2closest_edge_query.h"
#include "s2/s2edge_distances.h"
#include "s2/s2shape_index_region.h"

S2Cap S2MinDistancePointTarget::GetCapBound() {
  return S2Cap(point_, S1ChordAngle::Zero());
}

bool S2MinDistancePointTarget::UpdateMinDistance(
    const S2Point& p, S2MinDistance* min_dist) {
  return min_dist->UpdateMin(S2MinDistance(S1ChordAngle(p, point_)));
}

bool S2MinDistancePointTarget::UpdateMinDistance(
    const S2Point& v0, const S2Point& v1, S2MinDistance* min_dist) {
  return S2::UpdateMinDistance(point_, v0, v1, min_dist);
}

bool S2MinDistancePointTarget::UpdateMinDistance(
    const S2Cell& cell, S2MinDistance* min_dist) {
  return min_dist->UpdateMin(S2MinDistance(cell.GetDistance(point_)));
}

bool S2MinDistancePointTarget::VisitContainingShapes(
    const S2ShapeIndex& index, const ShapeVisitor& visitor) {
  return MakeS2ContainsPointQuery(&index).VisitContainingShapes(
      point_, [this, &visitor](S2Shape* shape) {
        return visitor(shape, point_);
      });
}

S2Cap S2MinDistanceEdgeTarget::GetCapBound() {
  // The following computes a radius equal to half the edge length in an
  // efficient and numerically stable way.
  double d2 = S1ChordAngle(a_, b_).length2();
  double r2 = (0.5 * d2) / (1 + sqrt(1 - 0.25 * d2));
  return S2Cap((a_ + b_).Normalize(), S1ChordAngle::FromLength2(r2));
}

bool S2MinDistanceEdgeTarget::UpdateMinDistance(
    const S2Point& p, S2MinDistance* min_dist) {
  return S2::UpdateMinDistance(p, a_, b_, min_dist);
}

bool S2MinDistanceEdgeTarget::UpdateMinDistance(
    const S2Point& v0, const S2Point& v1, S2MinDistance* min_dist) {
  return S2::UpdateEdgePairMinDistance(a_, b_, v0, v1, min_dist);
}

bool S2MinDistanceEdgeTarget::UpdateMinDistance(
    const S2Cell& cell, S2MinDistance* min_dist) {
  return min_dist->UpdateMin(S2MinDistance(cell.GetDistance(a_, b_)));
}

bool S2MinDistanceEdgeTarget::VisitContainingShapes(
    const S2ShapeIndex& index, const ShapeVisitor& visitor) {
  // We test the center of the edge in order to ensure that edge targets AB
  // and BA yield identical results (which is not guaranteed by the API but
  // users might expect).  Other options would be to test both endpoints, or
  // return different results for AB and BA in some cases.
  S2MinDistancePointTarget target((a_ + b_).Normalize());
  return target.VisitContainingShapes(index, visitor);
}

S2MinDistanceCellTarget::S2MinDistanceCellTarget(const S2Cell& cell)
    : cell_(cell) {
}

S2Cap S2MinDistanceCellTarget::GetCapBound() {
  return cell_.GetCapBound();
}

bool S2MinDistanceCellTarget::UpdateMinDistance(const S2Point& p,
                                                S2MinDistance* min_dist) {
  return min_dist->UpdateMin(S2MinDistance(cell_.GetDistance(p)));
}

bool S2MinDistanceCellTarget::UpdateMinDistance(
    const S2Point& v0, const S2Point& v1, S2MinDistance* min_dist) {
  return min_dist->UpdateMin(S2MinDistance(cell_.GetDistance(v0, v1)));
}

bool S2MinDistanceCellTarget::UpdateMinDistance(
    const S2Cell& cell, S2MinDistance* min_dist) {
  return min_dist->UpdateMin(S2MinDistance(cell_.GetDistance(cell)));
}

bool S2MinDistanceCellTarget::VisitContainingShapes(
    const S2ShapeIndex& index, const ShapeVisitor& visitor) {
  // The simplest approach is simply to return the polygons that contain the
  // cell center.  Alternatively, if the index cell is smaller than the target
  // cell then we could return all polygons that are present in the
  // S2ShapeIndexCell, but since the index is built conservatively this may
  // include some polygons that don't quite intersect the cell.  So we would
  // either need to recheck for intersection more accurately, or weaken the
  // VisitContainingShapes contract so that it only guarantees approximate
  // intersection, neither of which seems like a good tradeoff.
  S2MinDistancePointTarget target(cell_.GetCenter());
  return target.VisitContainingShapes(index, visitor);
}

S2MinDistanceCellUnionTarget::S2MinDistanceCellUnionTarget(
    S2CellUnion cell_union)
    : cell_union_(std::move(cell_union)),
      query_(absl::make_unique<S2ClosestCellQuery>(&index_)) {
  for (S2CellId cell_id : cell_union_) {
    index_.Add(cell_id, 0);
  }
  index_.Build();
}

S2MinDistanceCellUnionTarget::~S2MinDistanceCellUnionTarget() {
}

bool S2MinDistanceCellUnionTarget::use_brute_force() const {
  return query_->options().use_brute_force();
}

void S2MinDistanceCellUnionTarget::set_use_brute_force(
    bool use_brute_force) {
  query_->mutable_options()->set_use_brute_force(use_brute_force);
}

bool S2MinDistanceCellUnionTarget::set_max_error(
    const S1ChordAngle& max_error) {
  query_->mutable_options()->set_max_error(max_error);
  return true;  // Indicates that we may return suboptimal results.
}

S2Cap S2MinDistanceCellUnionTarget::GetCapBound() {
  return cell_union_.GetCapBound();
}

inline bool S2MinDistanceCellUnionTarget::UpdateMinDistance(
    S2MinDistanceTarget* target, S2MinDistance* min_dist) {
  query_->mutable_options()->set_max_distance(*min_dist);
  S2ClosestCellQuery::Result r = query_->FindClosestCell(target);
  if (r.is_empty()) return false;
  *min_dist = r.distance();
  return true;
}

bool S2MinDistanceCellUnionTarget::UpdateMinDistance(
    const S2Point& p, S2MinDistance* min_dist) {
  S2ClosestCellQuery::PointTarget target(p);
  return UpdateMinDistance(&target, min_dist);
}

bool S2MinDistanceCellUnionTarget::UpdateMinDistance(
    const S2Point& v0, const S2Point& v1, S2MinDistance* min_dist) {
  S2ClosestCellQuery::EdgeTarget target(v0, v1);
  return UpdateMinDistance(&target, min_dist);
}

bool S2MinDistanceCellUnionTarget::UpdateMinDistance(
    const S2Cell& cell, S2MinDistance* min_dist) {
  S2ClosestCellQuery::CellTarget target(cell);
  return UpdateMinDistance(&target, min_dist);
}

bool S2MinDistanceCellUnionTarget::VisitContainingShapes(
    const S2ShapeIndex& query_index, const ShapeVisitor& visitor) {
  for (S2CellId cell_id : cell_union_) {
    S2MinDistancePointTarget target(cell_id.ToPoint());
    if (!target.VisitContainingShapes(query_index, visitor)) {
      return false;
    }
  }
  return true;
}

S2MinDistanceShapeIndexTarget::S2MinDistanceShapeIndexTarget(
    const S2ShapeIndex* index)
    : index_(index), query_(absl::make_unique<S2ClosestEdgeQuery>(index)) {
}

S2MinDistanceShapeIndexTarget::~S2MinDistanceShapeIndexTarget() {
}

bool S2MinDistanceShapeIndexTarget::include_interiors() const {
  return query_->options().include_interiors();
}

void S2MinDistanceShapeIndexTarget::set_include_interiors(
    bool include_interiors) {
  query_->mutable_options()->set_include_interiors(include_interiors);
}

bool S2MinDistanceShapeIndexTarget::use_brute_force() const {
  return query_->options().use_brute_force();
}

void S2MinDistanceShapeIndexTarget::set_use_brute_force(
    bool use_brute_force) {
  query_->mutable_options()->set_use_brute_force(use_brute_force);
}

bool S2MinDistanceShapeIndexTarget::set_max_error(
    const S1ChordAngle& max_error) {
  query_->mutable_options()->set_max_error(max_error);
  return true;  // Indicates that we may return suboptimal results.
}

S2Cap S2MinDistanceShapeIndexTarget::GetCapBound() {
  return MakeS2ShapeIndexRegion(index_).GetCapBound();
}

inline bool S2MinDistanceShapeIndexTarget::UpdateMinDistance(
    S2MinDistanceTarget* target, S2MinDistance* min_dist) {
  query_->mutable_options()->set_max_distance(*min_dist);
  S2ClosestEdgeQuery::Result r = query_->FindClosestEdge(target);
  if (r.is_empty()) return false;
  *min_dist = r.distance();
  return true;
}

bool S2MinDistanceShapeIndexTarget::UpdateMinDistance(
    const S2Point& p, S2MinDistance* min_dist) {
  S2ClosestEdgeQuery::PointTarget target(p);
  return UpdateMinDistance(&target, min_dist);
}

bool S2MinDistanceShapeIndexTarget::UpdateMinDistance(
    const S2Point& v0, const S2Point& v1, S2MinDistance* min_dist) {
  S2ClosestEdgeQuery::EdgeTarget target(v0, v1);
  return UpdateMinDistance(&target, min_dist);
}

bool S2MinDistanceShapeIndexTarget::UpdateMinDistance(
    const S2Cell& cell, S2MinDistance* min_dist) {
  S2ClosestEdgeQuery::CellTarget target(cell);
  return UpdateMinDistance(&target, min_dist);
}

bool S2MinDistanceShapeIndexTarget::VisitContainingShapes(
    const S2ShapeIndex& query_index, const ShapeVisitor& visitor) {
  // It is sufficient to find the set of chain starts in the target index
  // (i.e., one vertex per connected component of edges) that are contained by
  // the query index, except for one special case to handle full polygons.
  //
  // TODO(ericv): Do this by merge-joining the two S2ShapeIndexes, and share
  // the code with S2BooleanOperation.

  for (S2Shape* shape : *index_) {
    if (shape == nullptr) continue;
    int num_chains = shape->num_chains();
    // Shapes that don't have any edges require a special case (below).
    bool tested_point = false;
    for (int c = 0; c < num_chains; ++c) {
      S2Shape::Chain chain = shape->chain(c);
      if (chain.length == 0) continue;
      tested_point = true;
      S2Point v0 = shape->chain_edge(c, 0).v0;
      S2MinDistancePointTarget target(v0);
      if (!target.VisitContainingShapes(query_index, visitor)) {
        return false;
      }
    }
    if (!tested_point) {
      // Special case to handle full polygons.
      S2Shape::ReferencePoint ref = shape->GetReferencePoint();
      if (!ref.contained) continue;
      S2MinDistancePointTarget target(ref.point);
      if (!target.VisitContainingShapes(query_index, visitor)) {
        return false;
      }
    }
  }
  return true;
}
