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

#include <vector>

#include <gtest/gtest.h>
#include "s2/mutable_s2shape_index.h"
#include "s2/s1angle.h"
#include "s2/s2cap.h"
#include "s2/s2cell.h"
#include "s2/s2edge_distances.h"
#include "s2/s2shape_index.h"
#include "s2/s2text_format.h"
#include "s2/util/gtl/btree_set.h"

using s2textformat::MakeIndexOrDie;
using s2textformat::MakePointOrDie;
using s2textformat::ParsePointsOrDie;
using std::vector;

TEST(PointTarget, UpdateMinDistanceToEdgeWhenEqual) {
  // Verifies that UpdateMinDistance only returns true when the new distance
  // is less than the old distance (not less than or equal to).
  S2MinDistancePointTarget target(MakePointOrDie("1:0"));
  S2MinDistance dist(S1ChordAngle::Infinity());
  auto edge = ParsePointsOrDie("0:-1, 0:1");
  EXPECT_TRUE(target.UpdateMinDistance(edge[0], edge[1], &dist));
  EXPECT_FALSE(target.UpdateMinDistance(edge[0], edge[1], &dist));
}

TEST(PointTarget, UpdateMinDistanceToCellWhenEqual) {
  // Verifies that UpdateMinDistance only returns true when the new distance
  // is less than the old distance (not less than or equal to).
  S2MinDistancePointTarget target(MakePointOrDie("1:0"));
  S2MinDistance dist(S1ChordAngle::Infinity());
  S2Cell cell{S2CellId(MakePointOrDie("0:0"))};
  EXPECT_TRUE(target.UpdateMinDistance(cell, &dist));
  EXPECT_FALSE(target.UpdateMinDistance(cell, &dist));
}

TEST(EdgeTarget, UpdateMinDistanceToEdgeWhenEqual) {
  S2MinDistanceEdgeTarget target(MakePointOrDie("1:0"), MakePointOrDie("1:1"));
  S2MinDistance dist(S1ChordAngle::Infinity());
  auto edge = ParsePointsOrDie("0:-1, 0:1");
  EXPECT_TRUE(target.UpdateMinDistance(edge[0], edge[1], &dist));
  EXPECT_FALSE(target.UpdateMinDistance(edge[0], edge[1], &dist));
}

TEST(EdgeTarget, UpdateMinDistanceToCellWhenEqual) {
  S2MinDistanceEdgeTarget target(MakePointOrDie("1:0"), MakePointOrDie("1:1"));
  S2MinDistance dist(S1ChordAngle::Infinity());
  S2Cell cell{S2CellId(MakePointOrDie("0:0"))};
  EXPECT_TRUE(target.UpdateMinDistance(cell, &dist));
  EXPECT_FALSE(target.UpdateMinDistance(cell, &dist));
}

TEST(CellTarget, UpdateMinDistanceToEdgeWhenEqual) {
  S2MinDistanceCellTarget target{S2Cell{S2CellId{MakePointOrDie("0:1")}}};
  S2MinDistance dist(S1ChordAngle::Infinity());
  auto edge = ParsePointsOrDie("0:-1, 0:1");
  EXPECT_TRUE(target.UpdateMinDistance(edge[0], edge[1], &dist));
  EXPECT_FALSE(target.UpdateMinDistance(edge[0], edge[1], &dist));
}

TEST(CellTarget, UpdateMinDistanceToCellWhenEqual) {
  S2MinDistanceCellTarget target{S2Cell{S2CellId{MakePointOrDie("0:1")}}};
  S2MinDistance dist(S1ChordAngle::Infinity());
  S2Cell cell{S2CellId(MakePointOrDie("0:0"))};
  EXPECT_TRUE(target.UpdateMinDistance(cell, &dist));
  EXPECT_FALSE(target.UpdateMinDistance(cell, &dist));
}

TEST(CellUnionTarget, UpdateMinDistanceToEdgeWhenEqual) {
  S2MinDistanceCellUnionTarget
      target{S2CellUnion({S2CellId{MakePointOrDie("0:1")}})};
  S2MinDistance dist(S1ChordAngle::Infinity());
  auto edge = ParsePointsOrDie("0:-1, 0:1");
  EXPECT_TRUE(target.UpdateMinDistance(edge[0], edge[1], &dist));
  EXPECT_FALSE(target.UpdateMinDistance(edge[0], edge[1], &dist));
}

TEST(CellUnionTarget, UpdateMinDistanceToCellWhenEqual) {
  S2MinDistanceCellUnionTarget
      target{S2CellUnion({S2CellId{MakePointOrDie("0:1")}})};
  S2MinDistance dist(S1ChordAngle::Infinity());
  S2Cell cell{S2CellId(MakePointOrDie("0:0"))};
  EXPECT_TRUE(target.UpdateMinDistance(cell, &dist));
  EXPECT_FALSE(target.UpdateMinDistance(cell, &dist));
}

TEST(ShapeIndexTarget, UpdateMinDistanceToEdgeWhenEqual) {
  auto target_index = MakeIndexOrDie("1:0 # #");
  S2MinDistanceShapeIndexTarget target(target_index.get());
  S2MinDistance dist(S1ChordAngle::Infinity());
  auto edge = ParsePointsOrDie("0:-1, 0:1");
  EXPECT_TRUE(target.UpdateMinDistance(edge[0], edge[1], &dist));
  EXPECT_FALSE(target.UpdateMinDistance(edge[0], edge[1], &dist));
}

TEST(ShapeIndexTarget, UpdateMinDistanceToCellWhenEqual) {
  auto target_index = MakeIndexOrDie("1:0 # #");
  S2MinDistanceShapeIndexTarget target(target_index.get());
  S2MinDistance dist(S1ChordAngle::Infinity());
  S2Cell cell{S2CellId(MakePointOrDie("0:0"))};
  EXPECT_TRUE(target.UpdateMinDistance(cell, &dist));
  EXPECT_FALSE(target.UpdateMinDistance(cell, &dist));
}

vector<int> GetContainingShapes(S2MinDistanceTarget* target,
                                const S2ShapeIndex& index, int max_shapes) {
  gtl::btree_set<int32> shape_ids;
  (void) target->VisitContainingShapes(
      index, [&shape_ids, max_shapes](S2Shape* containing_shape,
                                      const S2Point& target_point) {
        shape_ids.insert(containing_shape->id());
        return shape_ids.size() < max_shapes;
      });
  return vector<int>(shape_ids.begin(), shape_ids.end());
}

// Given two sorted vectors "x" and "y", returns true if x is a subset of y
// and x.size() == x_size.
bool IsSubsetOfSize(const vector<int>& x, const vector<int>& y,
                    int x_size) {
  if (x.size() != x_size) return false;
  return std::includes(y.begin(), y.end(), x.begin(), x.end());
}

TEST(PointTarget, VisitContainingShapes) {
  // Only shapes 2 and 4 should contain the target point.
  auto index = MakeIndexOrDie(
      "1:1 # 1:1, 2:2 # 0:0, 0:3, 3:0 | 6:6, 6:9, 9:6 | 0:0, 0:4, 4:0");
  S2MinDistancePointTarget target(MakePointOrDie("1:1"));
  EXPECT_TRUE(IsSubsetOfSize(GetContainingShapes(&target, *index, 1),
                             vector<int>{2, 4}, 1));
  EXPECT_EQ((vector<int>{2, 4}), GetContainingShapes(&target, *index, 5));
}

TEST(EdgeTarget, VisitContainingShapes) {
  // Only shapes 2 and 4 should contain the target point.
  auto index = MakeIndexOrDie(
      "1:1 # 1:1, 2:2 # 0:0, 0:3, 3:0 | 6:6, 6:9, 9:6 | 0:0, 0:4, 4:0");
  S2MinDistanceEdgeTarget target(MakePointOrDie("1:2"), MakePointOrDie("2:1"));
  EXPECT_TRUE(IsSubsetOfSize(GetContainingShapes(&target, *index, 1),
                             vector<int>{2, 4}, 1));
  EXPECT_EQ((vector<int>{2, 4}), GetContainingShapes(&target, *index, 5));
}

TEST(CellTarget, VisitContainingShapes) {
  auto index = MakeIndexOrDie(
      "1:1 # 1:1, 2:2 # 0:0, 0:3, 3:0 | 6:6, 6:9, 9:6 | -1:-1, -1:5, 5:-1");
  // Only shapes 2 and 4 should contain a very small cell near 1:1.
  S2CellId cellid1(MakePointOrDie("1:1"));
  S2MinDistanceCellTarget target1{S2Cell(cellid1)};
  EXPECT_TRUE(IsSubsetOfSize(GetContainingShapes(&target1, *index, 1),
                             vector<int>{2, 4}, 1));
  EXPECT_EQ((vector<int>{2, 4}), GetContainingShapes(&target1, *index, 5));

  // For a larger cell that properly contains one or more index cells, all
  // shapes that intersect the first such cell in S2CellId order are returned.
  // In the test below, this happens to again be the 1st and 3rd polygons
  // (whose shape_ids are 2 and 4).
  S2CellId cellid2 = cellid1.parent(5);
  S2MinDistanceCellTarget target2{S2Cell(cellid2)};
  EXPECT_EQ((vector<int>{2, 4}), GetContainingShapes(&target2, *index, 5));
}

TEST(CellUnionTarget, VisitContainingShapes) {
  auto index = MakeIndexOrDie(
      "1:1 # 1:1, 2:2 # 0:0, 0:3, 3:0 | 6:6, 6:9, 9:6 | -1:-1, -1:5, 5:-1");
  // Shapes 2 and 4 contain the leaf cell near 1:1, while shape 3 contains the
  // leaf cell near 7:7.
  S2CellId cellid1(MakePointOrDie("1:1")), cellid2(MakePointOrDie("7:7"));
  S2MinDistanceCellUnionTarget target1{S2CellUnion({cellid1, cellid2})};
  EXPECT_TRUE(IsSubsetOfSize(GetContainingShapes(&target1, *index, 1),
                             vector<int>{2, 3, 4}, 1));
  EXPECT_EQ((vector<int>{2, 3, 4}), GetContainingShapes(&target1, *index, 5));
}

TEST(ShapeIndexTarget, VisitContainingShapes) {
  // Create an index containing a repeated grouping of one point, one
  // polyline, and one polygon.
  auto index = MakeIndexOrDie(
      "1:1 | 4:4 | 7:7 | 10:10 # "
      "1:1, 1:2 | 4:4, 4:5 | 7:7, 7:8 | 10:10, 10:11 # "
      "0:0, 0:3, 3:0 | 3:3, 3:6, 6:3 | 6:6, 6:9, 9:6 | 9:9, 9:12, 12:9");

  // Construct a target consisting of one point, one polyline, and one polygon
  // with two loops where only the second loop is contained by a polygon in
  // the index above.
  auto target_index = MakeIndexOrDie(
      "1:1 # 4:5, 5:4 # 20:20, 20:21, 21:20; 10:10, 10:11, 11:10");

  S2MinDistanceShapeIndexTarget target(target_index.get());
  // These are the shape_ids of the 1st, 2nd, and 4th polygons of "index"
  // (noting that the 4 points are represented by one S2PointVectorShape).
  EXPECT_EQ((vector<int>{5, 6, 8}), GetContainingShapes(&target, *index, 5));
}

TEST(ShapeIndexTarget, VisitContainingShapesEmptyAndFull) {
  // Verify that VisitContainingShapes never returns empty polygons and always
  // returns full polygons (i.e., those containing the entire sphere).

  // Creating an index containing one empty and one full polygon.
  auto index = MakeIndexOrDie("# # empty | full");

  // Check only the full polygon is returned for a point target.
  auto point_index = MakeIndexOrDie("1:1 # #");
  S2MinDistanceShapeIndexTarget point_target(point_index.get());
  EXPECT_EQ((vector<int>{1}), GetContainingShapes(&point_target, *index, 5));

  // Check only the full polygon is returned for a full polygon target.
  auto full_polygon_index = MakeIndexOrDie("# # full");
  S2MinDistanceShapeIndexTarget full_target(full_polygon_index.get());
  EXPECT_EQ((vector<int>{1}), GetContainingShapes(&full_target, *index, 5));

  // Check that nothing is returned for an empty polygon target.  (An empty
  // polygon has no connected components and does not intersect anything, so
  // according to the API of GetContainingShapes nothing should be returned.)
  auto empty_polygon_index = MakeIndexOrDie("# # empty");
  S2MinDistanceShapeIndexTarget empty_target(empty_polygon_index.get());
  EXPECT_EQ((vector<int>{}), GetContainingShapes(&empty_target, *index, 5));
}
