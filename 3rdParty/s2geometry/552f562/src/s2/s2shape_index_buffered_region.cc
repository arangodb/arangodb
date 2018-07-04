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

#include "s2/s2shape_index_buffered_region.h"

#include <algorithm>
#include <vector>
#include "s2/s2metrics.h"
#include "s2/s2shape_index_region.h"

using std::min;
using std::vector;

S2ShapeIndexBufferedRegion::S2ShapeIndexBufferedRegion() {
}

void S2ShapeIndexBufferedRegion::Init(const S2ShapeIndex* index,
                                      S1ChordAngle radius) {
  radius_ = radius;
  radius_successor_ = radius.Successor();
  query_.Init(index);
  query_.mutable_options()->set_include_interiors(true);
}

S2ShapeIndexBufferedRegion* S2ShapeIndexBufferedRegion::Clone() const {
  return new S2ShapeIndexBufferedRegion(&index(), radius_);
}

S2Cap S2ShapeIndexBufferedRegion::GetCapBound() const {
  S2Cap orig_cap = MakeS2ShapeIndexRegion(&index()).GetCapBound();
  return S2Cap(orig_cap.center(), orig_cap.radius() + radius_);
}

S2LatLngRect S2ShapeIndexBufferedRegion::GetRectBound() const {
  S2LatLngRect orig_rect = MakeS2ShapeIndexRegion(&index()).GetRectBound();
  return orig_rect.ExpandedByDistance(radius_.ToAngle());
}

void S2ShapeIndexBufferedRegion::GetCellUnionBound(vector<S2CellId> *cellids)
    const {
  // We start with a covering of the original S2ShapeIndex, and then expand it
  // by replacing each cell with a block of 4 cells whose union contains the
  // original cell buffered by the given radius.
  //
  // This increases the number of cells in the covering by a factor of 4 and
  // increases the covered area by a factor of 16, so it is not a very good
  // covering, but it is much better than always returning the 6 face cells.
  vector<S2CellId> orig_cellids;
  MakeS2ShapeIndexRegion(&index()).GetCellUnionBound(&orig_cellids);

  double radians = radius_.ToAngle().radians();
  int max_level = S2::kMinWidth.GetLevelForMinValue(radians) - 1;
  if (max_level < 0) {
    return S2Cap::Full().GetCellUnionBound(cellids);
  }
  cellids->clear();
  for (S2CellId id : orig_cellids) {
    if (id.is_face()) {
      return S2Cap::Full().GetCellUnionBound(cellids);
    }
    id.AppendVertexNeighbors(min(max_level, id.level() - 1), cellids);
  }
}

bool S2ShapeIndexBufferedRegion::Contains(const S2Cell& cell) const {
  // To implement this method perfectly would require computing the directed
  // Hausdorff distance, which is expensive (and not currently implemented).
  // However the following heuristic is almost as good in practice and much
  // cheaper to compute.

  // Return true if the unbuffered region contains this cell.
  if (MakeS2ShapeIndexRegion(&index()).Contains(cell)) return true;

  // Otherwise approximate the cell by its bounding cap.
  //
  // NOTE(ericv): It would be slightly more accurate to first find the closest
  // point in the indexed geometry to the cell, and then measure the actual
  // maximum distance from that point to the cell (a poor man's Hausdorff
  // distance).  But based on actual tests this is not worthwhile.
  S2Cap cap = cell.GetCapBound();
  if (radius_ < cap.radius()) return false;

  // Return true if the distance to the cell center plus the radius of the
  // cell's bounding cap is less than or equal to "radius_".
  S2ClosestEdgeQuery::PointTarget target(cell.GetCenter());
  return query_.IsDistanceLess(&target, radius_successor_ - cap.radius());
}

bool S2ShapeIndexBufferedRegion::MayIntersect(const S2Cell& cell) const {
  // Return true if the distance is less than or equal to "radius_".
  S2ClosestEdgeQuery::CellTarget target(cell);
  return query_.IsDistanceLess(&target, radius_successor_);
}

bool S2ShapeIndexBufferedRegion::Contains(const S2Point& p) const {
  S2ClosestEdgeQuery::PointTarget target(p);
  // Return true if the distance is less than or equal to "radius_".
  return query_.IsDistanceLess(&target, radius_successor_);
}
