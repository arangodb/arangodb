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

#include "s2/s2max_distance_targets.h"

#include <memory>
#include "s2/third_party/absl/memory/memory.h"
#include "s2/s1angle.h"
#include "s2/s2cap.h"
#include "s2/s2cell.h"
#include "s2/s2edge_distances.h"
#include "s2/s2furthest_edge_query.h"
#include "s2/s2shape_index_region.h"
#include "s2/s2text_format.h"

//////////////////   Point Target   ////////////////////

// This method returns an S2Cap that bounds the antipode of the target.  (This
// is the set of points whose S2MaxDistance to the target is
// S2MaxDistance::Zero().)
S2Cap S2MaxDistancePointTarget::GetCapBound() {
  return S2Cap(-point_, S1ChordAngle::Zero());
}

bool S2MaxDistancePointTarget::UpdateMinDistance(
    const S2Point& p, S2MaxDistance* min_dist) {
  return min_dist->UpdateMin(S2MaxDistance(S1ChordAngle(p, point_)));
}

bool S2MaxDistancePointTarget::UpdateMinDistance(
    const S2Point& v0, const S2Point& v1, S2MaxDistance* min_dist) {
  S1ChordAngle dist(*min_dist);
  if (S2::UpdateMaxDistance(point_, v0, v1, &dist)) {
    min_dist->UpdateMin(S2MaxDistance(dist));
    return true;
  }
  return false;
}

bool S2MaxDistancePointTarget::UpdateMinDistance(
    const S2Cell& cell, S2MaxDistance* min_dist) {
  return min_dist->UpdateMin(S2MaxDistance(cell.GetMaxDistance(point_)));
}

bool S2MaxDistancePointTarget::VisitContainingShapes(
    const S2ShapeIndex& index, const ShapeVisitor& visitor) {
  // For furthest points, we visit the polygons whose interior contains the
  // antipode of the target point.  (These are the polygons whose
  // S2MaxDistance to the target is S2MaxDistance::Zero().)
  return MakeS2ContainsPointQuery(&index).VisitContainingShapes(
      -point_, [this, &visitor](S2Shape* shape) {
        return visitor(shape, point_);
      });
}

//////////////////   Edge Target   ////////////////////

// This method returns an S2Cap that bounds the antipode of the target.  (This
// is the set of points whose S2MaxDistance to the target is
// S2MaxDistance::Zero().)
S2Cap S2MaxDistanceEdgeTarget::GetCapBound() {
  // The following computes a radius equal to half the edge length in an
  // efficient and numerically stable way.
  double d2 = S1ChordAngle(a_, b_).length2();
  double r2 = (0.5 * d2) / (1 + sqrt(1 - 0.25 * d2));
  return S2Cap(-(a_ + b_).Normalize(), S1ChordAngle::FromLength2(r2));
}

bool S2MaxDistanceEdgeTarget::UpdateMinDistance(
    const S2Point& p, S2MaxDistance* min_dist) {
  S1ChordAngle dist(*min_dist);
  if (S2::UpdateMaxDistance(p, a_, b_, &dist)) {
    min_dist->UpdateMin(S2MaxDistance(dist));
    return true;
  }
  return false;
}

bool S2MaxDistanceEdgeTarget::UpdateMinDistance(
    const S2Point& v0, const S2Point& v1, S2MaxDistance* min_dist) {
  S1ChordAngle dist(*min_dist);
  if (S2::UpdateEdgePairMaxDistance(a_, b_, v0, v1, &dist)) {
    min_dist->UpdateMin(S2MaxDistance(dist));
    return true;
  }
  return false;
}

bool S2MaxDistanceEdgeTarget::UpdateMinDistance(
    const S2Cell& cell, S2MaxDistance* min_dist) {
  return min_dist->UpdateMin(S2MaxDistance(cell.GetMaxDistance(a_, b_)));
}

bool S2MaxDistanceEdgeTarget::VisitContainingShapes(
    const S2ShapeIndex& index, const ShapeVisitor& visitor) {
  // We only need to test one edge point.  That is because the method *must*
  // visit a polygon if it fully contains the target, and *is allowed* to
  // visit a polygon if it intersects the target.  If the tested vertex is not
  // contained, we know the full edge is not contained; if the tested vertex is
  // contained, then the edge either is fully contained (must be visited) or it
  // intersects (is allowed to be visited).  We visit the center of the edge so
  // that edge AB gives identical results to BA.
  S2MaxDistancePointTarget target((a_ + b_).Normalize());
  return target.VisitContainingShapes(index, visitor);
}

//////////////////   Cell Target   ////////////////////

// This method returns an S2Cap that bounds the antipode of the target.  (This
// is the set of points whose S2MaxDistance to the target is
// S2MaxDistance::Zero().)
S2Cap S2MaxDistanceCellTarget::GetCapBound() {
  S2Cap cap = cell_.GetCapBound();
  return S2Cap(-cap.center(), cap.radius());
}

bool S2MaxDistanceCellTarget::UpdateMinDistance(
    const S2Point& p, S2MaxDistance* min_dist) {
  return min_dist->UpdateMin(S2MaxDistance(cell_.GetMaxDistance(p)));
}

bool S2MaxDistanceCellTarget::UpdateMinDistance(
    const S2Point& v0, const S2Point& v1, S2MaxDistance* min_dist) {
  return min_dist->UpdateMin(S2MaxDistance(cell_.GetMaxDistance(v0, v1)));
}

bool S2MaxDistanceCellTarget::UpdateMinDistance(
    const S2Cell& cell, S2MaxDistance* min_dist) {
  return min_dist->UpdateMin(S2MaxDistance(cell_.GetMaxDistance(cell)));
}

bool S2MaxDistanceCellTarget::VisitContainingShapes(
    const S2ShapeIndex& index, const ShapeVisitor& visitor) {
  // We only need to check one point here - cell center is simplest.
  // See comment at S2MaxDistanceEdgeTarget::VisitContainingShapes.
  S2MaxDistancePointTarget target(cell_.GetCenter());
  return target.VisitContainingShapes(index, visitor);
}

//////////////////   Index Target   ////////////////////

S2MaxDistanceShapeIndexTarget::S2MaxDistanceShapeIndexTarget(
    const S2ShapeIndex* index)
    : index_(index), query_(absl::make_unique<S2FurthestEdgeQuery>(index)) {
}

S2MaxDistanceShapeIndexTarget::~S2MaxDistanceShapeIndexTarget() {
}

bool S2MaxDistanceShapeIndexTarget::include_interiors() const {
  return query_->options().include_interiors();
}

void S2MaxDistanceShapeIndexTarget::set_include_interiors(
    bool include_interiors) {
  query_->mutable_options()->set_include_interiors(include_interiors);
}

bool S2MaxDistanceShapeIndexTarget::use_brute_force() const {
  return query_->options().use_brute_force();
}

void S2MaxDistanceShapeIndexTarget::set_use_brute_force(
    bool use_brute_force) {
  query_->mutable_options()->set_use_brute_force(use_brute_force);
}

bool S2MaxDistanceShapeIndexTarget::set_max_error(
    const S1ChordAngle& max_error) {
  query_->mutable_options()->set_max_error(max_error);
  return true;  // Indicates that we may return suboptimal results.
}

// This method returns an S2Cap that bounds the antipode of the target.  (This
// is the set of points whose S2MaxDistance to the target is
// S2MaxDistance::Zero().)
S2Cap S2MaxDistanceShapeIndexTarget::GetCapBound() {
  S2Cap cap = MakeS2ShapeIndexRegion(index_).GetCapBound();
  return S2Cap(-cap.center(), cap.radius());
}

bool S2MaxDistanceShapeIndexTarget::UpdateMinDistance(
    const S2Point& p, S2MaxDistance* min_dist) {
  query_->mutable_options()->set_min_distance(S1ChordAngle(*min_dist));
  S2FurthestEdgeQuery::PointTarget target(p);
  S2FurthestEdgeQuery::Result r = query_->FindFurthestEdge(&target);
  if (r.shape_id() < 0) {
    return false;
  }
  *min_dist = S2MaxDistance(r.distance());
  return true;
}

bool S2MaxDistanceShapeIndexTarget::UpdateMinDistance(
    const S2Point& v0, const S2Point& v1, S2MaxDistance* min_dist) {
  query_->mutable_options()->set_min_distance(S1ChordAngle(*min_dist));
  S2FurthestEdgeQuery::EdgeTarget target(v0, v1);
  S2FurthestEdgeQuery::Result r = query_->FindFurthestEdge(&target);
  if (r.shape_id() < 0) return false;
  *min_dist = S2MaxDistance(r.distance());
  return true;
}

bool S2MaxDistanceShapeIndexTarget::UpdateMinDistance(
    const S2Cell& cell, S2MaxDistance* min_dist) {
  query_->mutable_options()->set_min_distance(S1ChordAngle(*min_dist));
  S2FurthestEdgeQuery::CellTarget target(cell);
  S2FurthestEdgeQuery::Result r = query_->FindFurthestEdge(&target);
  if (r.shape_id() < 0) return false;
  *min_dist = S2MaxDistance(r.distance());
  return true;
}

// For target types consisting of multiple connected components (such as
// S2MaxDistanceShapeIndexTarget), this method should return the
// polygons containing the antipodal reflection of *any* connected
// component.  (It is sufficient to test containment of one vertex per
// connected component, since the API allows us to also return any polygon
// whose boundary has S2MaxDistance::Zero() to the target.)
bool S2MaxDistanceShapeIndexTarget::VisitContainingShapes(
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
      S2MaxDistancePointTarget target(shape->chain_edge(c, 0).v0);
      if (!target.VisitContainingShapes(query_index, visitor)) {
        return false;
      }
    }
    if (!tested_point) {
      // Special case to handle full polygons.
      S2Shape::ReferencePoint ref = shape->GetReferencePoint();
      if (!ref.contained) continue;
      S2MaxDistancePointTarget target(ref.point);
      if (!target.VisitContainingShapes(query_index, visitor)) {
        return false;
      }
    }
  }
  return true;
}
