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

#include <vector>

#include <gtest/gtest.h>
#include "s2/mutable_s2shape_index.h"
#include "s2/s1angle.h"
#include "s2/s2cap.h"
#include "s2/s2cell.h"
#include "s2/s2edge_distances.h"
#include "s2/s2lax_polygon_shape.h"
#include "s2/s2point_vector_shape.h"
#include "s2/s2polygon.h"
#include "s2/s2shape_index.h"
#include "s2/s2testing.h"
#include "s2/s2text_format.h"
#include "s2/util/gtl/btree_set.h"

using absl::make_unique;
using s2textformat::MakeIndexOrDie;
using s2textformat::MakePointOrDie;
using s2textformat::ParsePointsOrDie;
using std::vector;

TEST(CellTarget, GetCapBound) {
  for (int i = 0; i < 100; i++) {
    S2Cell cell = S2Cell{S2Testing::GetRandomCellId()};
    S2MaxDistanceCellTarget target(cell);
    S2Cap cap = target.GetCapBound();

    for (int j = 0; j < 100; j++) {
      S2Point p_test = S2Testing::RandomPoint();
      // Check points outside of cap to be away from S2MaxDistance::Zero().
      if (!(cap.Contains(p_test))) {
        S1ChordAngle dist = cell.GetMaxDistance(p_test);
        EXPECT_TRUE(S2MaxDistance::Zero() < S2MaxDistance(dist));
      }
    }
  }
}

TEST(IndexTarget, GetCapBound) {
  auto index = make_unique<MutableS2ShapeIndex>();

  S2Polygon polygon(S2Cell{S2Testing::GetRandomCellId()});
  index->Add(make_unique<S2Polygon::Shape>(&polygon));

  S2Point p = S2Testing::RandomPoint();
  vector<S2Point> pts = {p};
  index->Add(make_unique<S2PointVectorShape>(pts));

  S2MaxDistanceShapeIndexTarget target(index.get());
  S2Cap cap = target.GetCapBound();

  for (int j = 0; j < 100; j++) {
    S2Point p_test = S2Testing::RandomPoint();
    // Check points outside of cap to be away from S2MaxDistance::Zero().
    if (!(cap.Contains(p_test))) {
      S2MaxDistance cur_dist = S2MaxDistance::Infinity();
      EXPECT_TRUE(target.UpdateMinDistance(p_test, &cur_dist));
      EXPECT_TRUE(S2MaxDistance::Zero() < cur_dist);
    }
  }
}

TEST(PointTarget, UpdateMaxDistance) {
  S2MaxDistancePointTarget target(MakePointOrDie("0:0"));
  S2MaxDistance dist0 = S2MaxDistance(S1ChordAngle::Degrees(0));
  S2MaxDistance dist10 = S2MaxDistance(S1ChordAngle::Degrees(10));

  // Update max distance target to point.
  S2Point p = MakePointOrDie("1:0");
  EXPECT_TRUE(target.UpdateMinDistance(p, &dist0));
  EXPECT_NEAR(1.0, S1ChordAngle(dist0).degrees(), 1e-15);
  EXPECT_FALSE(target.UpdateMinDistance(p, &dist10));

  // Reset dist0 which was updated.
  dist0 = S2MaxDistance(S1ChordAngle::Degrees(0));
  // Test for edges.
  auto edge = ParsePointsOrDie("0:-1, 0:1");
  EXPECT_TRUE(target.UpdateMinDistance(edge[0], edge[1], &dist0));
  EXPECT_NEAR(1.0, S1ChordAngle(dist0).degrees(), 1e-15);
  EXPECT_FALSE(target.UpdateMinDistance(p, &dist10));

  // Reset dist0 which was updated.
  dist0 = S2MaxDistance(S1ChordAngle::Degrees(0));
  // Test for cell.
  S2Cell cell{S2CellId(MakePointOrDie("0:0"))};
  EXPECT_TRUE(target.UpdateMinDistance(cell, &dist0));
  // Leaf cell will be tiny compared to 10 degrees - expect no update.
  EXPECT_FALSE(target.UpdateMinDistance(cell, &dist10));
}

TEST(PointTarget, UpdateMaxDistanceToEdgeWhenEqual) {
  // Verifies that UpdateMinDistance only returns true when the new distance
  // is greater than the old distance (not greater than or equal to).
  S2MaxDistancePointTarget target(MakePointOrDie("1:0"));
  auto dist = S2MaxDistance::Infinity();
  auto edge = ParsePointsOrDie("0:-1, 0:1");
  EXPECT_TRUE(target.UpdateMinDistance(edge[0], edge[1], &dist));
  EXPECT_FALSE(target.UpdateMinDistance(edge[0], edge[1], &dist));
}

TEST(PointTarget, UpdateMaxDistanceToCellWhenEqual) {
  S2MaxDistancePointTarget target(MakePointOrDie("1:0"));
  auto dist = S2MaxDistance::Infinity();
  S2Cell cell{S2CellId(MakePointOrDie("0:0"))};
  EXPECT_TRUE(target.UpdateMinDistance(cell, &dist));
  EXPECT_FALSE(target.UpdateMinDistance(cell, &dist));
}

TEST(EdgeTarget, UpdateMaxDistance) {
  auto target_edge = ParsePointsOrDie("0:-1, 0:1");
  S2MaxDistanceEdgeTarget target(target_edge[0], target_edge[1]);
  S2MaxDistance dist0 = S2MaxDistance(S1ChordAngle::Degrees(0));
  S2MaxDistance dist10 = S2MaxDistance(S1ChordAngle::Degrees(10));

  // Update max distance target to point.
  S2Point p = MakePointOrDie("0:2");
  EXPECT_TRUE(target.UpdateMinDistance(p, &dist0));
  EXPECT_NEAR(3.0, S1ChordAngle(dist0).degrees(), 1e-15);
  EXPECT_FALSE(target.UpdateMinDistance(p, &dist10));

  // Reset dist0 which was updated.
  dist0 = S2MaxDistance(S1ChordAngle::Degrees(0));
  // Test for edges.
  auto test_edge = ParsePointsOrDie("0:2, 0:3");
  EXPECT_TRUE(target.UpdateMinDistance(test_edge[0], test_edge[1], &dist0));
  EXPECT_NEAR(4.0, S1ChordAngle(dist0).degrees(), 1e-15);
  EXPECT_FALSE(target.UpdateMinDistance(p, &dist10));

  // Reset dist0 which was updated.
  dist0 = S2MaxDistance(S1ChordAngle::Degrees(0));
  // Test for cell.
  S2Cell cell{S2CellId(MakePointOrDie("0:0"))};
  EXPECT_TRUE(target.UpdateMinDistance(cell, &dist0));
  // Leaf cell will be tiny compared to 10 degrees - expect no update.
  EXPECT_FALSE(target.UpdateMinDistance(cell, &dist10));
}

TEST(EdgeTarget, UpdateMaxDistanceToEdgeWhenEqual) {
  S2MaxDistanceEdgeTarget target(MakePointOrDie("1:0"), MakePointOrDie("1:1"));
  S2MaxDistance dist = S2MaxDistance::Infinity();
  auto edge = ParsePointsOrDie("0:-1, 0:1");
  EXPECT_TRUE(target.UpdateMinDistance(edge[0], edge[1], &dist));
  EXPECT_FALSE(target.UpdateMinDistance(edge[0], edge[1], &dist));
}

TEST(EdgeTarget, UpdateMaxDistanceToEdgeAntipodal) {
  S2MaxDistanceEdgeTarget target(MakePointOrDie("0:89"),
                                 MakePointOrDie("0:91"));
  S2MaxDistance dist = S2MaxDistance::Infinity();
  auto edge = ParsePointsOrDie("1:-90, -1:-90");
  EXPECT_TRUE(target.UpdateMinDistance(edge[0], edge[1], &dist));
  EXPECT_EQ(S1ChordAngle(dist), S1ChordAngle::Straight());
}

TEST(EdgeTarget, UpdateMaxDistanceToCellWhenEqual) {
  S2MaxDistanceEdgeTarget target(MakePointOrDie("1:0"), MakePointOrDie("1:1"));
  S2MaxDistance dist = S2MaxDistance::Infinity();
  S2Cell cell{S2CellId(MakePointOrDie("0:0"))};
  EXPECT_TRUE(target.UpdateMinDistance(cell, &dist));
  EXPECT_FALSE(target.UpdateMinDistance(cell, &dist));
}

TEST(CellTarget, UpdateMaxDistance) {
  S2MaxDistanceCellTarget target{S2Cell{S2CellId{MakePointOrDie("0:1")}}};
  S2MaxDistance dist0 = S2MaxDistance(S1ChordAngle::Degrees(0));
  S2MaxDistance dist10 = S2MaxDistance(S1ChordAngle::Degrees(10));

  // Update max distance target to point.
  S2Point p = MakePointOrDie("0:0");
  EXPECT_TRUE(target.UpdateMinDistance(p, &dist0));
  EXPECT_FALSE(target.UpdateMinDistance(p, &dist10));

  // Reset dist0 which was updated.
  dist0 = S2MaxDistance(S1ChordAngle::Degrees(0));
  // Test for edges.
  auto test_edge = ParsePointsOrDie("0:2, 0:3");
  EXPECT_TRUE(target.UpdateMinDistance(test_edge[0], test_edge[1], &dist0));
  EXPECT_FALSE(target.UpdateMinDistance(p, &dist10));

  // Reset dist0 which was updated.
  dist0 = S2MaxDistance(S1ChordAngle::Degrees(0));
  // Test for cell.
  S2Cell cell{S2CellId(MakePointOrDie("0:0"))};
  EXPECT_TRUE(target.UpdateMinDistance(cell, &dist0));
  // Leaf cell extent will be tiny compared to 10 degrees - expect no update.
  EXPECT_FALSE(target.UpdateMinDistance(cell, &dist10));
}

TEST(CellTarget, UpdateMaxDistanceToEdgeWhenEqual) {
  S2MaxDistanceCellTarget target{S2Cell{S2CellId{MakePointOrDie("0:1")}}};
  S2MaxDistance dist = S2MaxDistance::Infinity();
  auto edge = ParsePointsOrDie("0:-1, 0:1");
  EXPECT_TRUE(target.UpdateMinDistance(edge[0], edge[1], &dist));
  EXPECT_FALSE(target.UpdateMinDistance(edge[0], edge[1], &dist));
}

TEST(CellTarget, UpdateMaxDistanceToCellWhenEqual) {
  S2MaxDistanceCellTarget target{S2Cell{S2CellId{MakePointOrDie("0:1")}}};
  S2MaxDistance dist = S2MaxDistance::Infinity();
  S2Cell cell{S2CellId(MakePointOrDie("0:0"))};
  EXPECT_TRUE(target.UpdateMinDistance(cell, &dist));
  EXPECT_FALSE(target.UpdateMinDistance(cell, &dist));
}

TEST(ShapeIndexTarget, UpdateMaxDistanceToEdgeWhenEqual) {
  auto target_index = MakeIndexOrDie("1:0 # #");
  S2MaxDistanceShapeIndexTarget target(target_index.get());
  S2MaxDistance dist = S2MaxDistance::Infinity();
  auto edge = ParsePointsOrDie("0:-1, 0:1");
  EXPECT_TRUE(target.UpdateMinDistance(edge[0], edge[1], &dist));
  EXPECT_FALSE(target.UpdateMinDistance(edge[0], edge[1], &dist));
}

TEST(ShapeIndexTarget, UpdateMaxDistanceToCellWhenEqual) {
  auto target_index = MakeIndexOrDie("1:0 # #");
  S2MaxDistanceShapeIndexTarget target(target_index.get());
  S2MaxDistance dist = S2MaxDistance::Infinity();
  S2Cell cell{S2CellId(MakePointOrDie("0:0"))};
  EXPECT_TRUE(target.UpdateMinDistance(cell, &dist));
  EXPECT_FALSE(target.UpdateMinDistance(cell, &dist));
}

TEST(CellTarget, UpdateMaxDistanceToCellAntipodal) {
  S2Point p = MakePointOrDie("0:0");
  S2MaxDistanceCellTarget target{S2Cell{p}};
  S2MaxDistance dist = S2MaxDistance::Infinity();
  S2Cell cell{-p};
  EXPECT_TRUE(target.UpdateMinDistance(cell, &dist));
  EXPECT_EQ(S1ChordAngle(dist), S1ChordAngle::Straight());
  // Expect a second update to do nothing.
  EXPECT_FALSE(target.UpdateMinDistance(cell, &dist));
}

vector<int> GetContainingShapes(S2MaxDistanceTarget* target,
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

TEST(PointTarget, VisitContainingShapes) {
  // Only shapes 2 and 4 should contain the target point.
  auto index = MakeIndexOrDie(
      "1:1 # 1:1, 2:2 # 0:0, 0:3, 3:0 | 6:6, 6:9, 9:6 | 0:0, 0:4, 4:0");
  S2Point p = MakePointOrDie("1:1");
  // Test against antipodal point.
  S2MaxDistancePointTarget target(-p);
  EXPECT_EQ((vector<int>{2}), GetContainingShapes(&target, *index, 1));
  EXPECT_EQ((vector<int>{2, 4}), GetContainingShapes(&target, *index, 5));
}

TEST(EdgeTarget, VisitContainingShapes) {
  // Only shapes 2 and 4 should contain the target point.
  auto index = MakeIndexOrDie(
      "1:1 # 1:1, 2:2 # 0:0, 0:3, 3:0 | 6:6, 6:9, 9:6 | 0:0, 0:4, 4:0");
  // Test against antipodal edge.
  auto edge = ParsePointsOrDie("1:2, 2:1");
  S2MaxDistanceEdgeTarget target(-edge[0], -edge[1]);
  EXPECT_EQ((vector<int>{2}), GetContainingShapes(&target, *index, 1));
  EXPECT_EQ((vector<int>{2, 4}), GetContainingShapes(&target, *index, 5));
}

TEST(CellTarget, VisitContainingShapes) {
  auto index = MakeIndexOrDie(
      "1:1 # 1:1, 2:2 # 0:0, 0:3, 3:0 | 6:6, 6:9, 9:6 | -1:-1, -1:5, 5:-1");
  // Only shapes 2 and 4 should contain a very small cell near
  // the antipode of 1:1.
  S2CellId cellid1(-MakePointOrDie("1:1"));
  S2MaxDistanceCellTarget target1{S2Cell(cellid1)};
  EXPECT_EQ((vector<int>{2}), GetContainingShapes(&target1, *index, 1));
  EXPECT_EQ((vector<int>{2, 4}), GetContainingShapes(&target1, *index, 5));

  // For a larger antipodal cell that properly contains one or more index
  // cells, all shapes that intersect the first such cell in S2CellId order are
  // returned.  In the test below, this happens to again be the 1st and 3rd
  // polygons (whose shape_ids are 2 and 4).
  S2CellId cellid2 = cellid1.parent(5);
  S2MaxDistanceCellTarget target2{S2Cell(cellid2)};
  EXPECT_EQ((vector<int>{2, 4}), GetContainingShapes(&target2, *index, 5));
}

// Negates S2 points to reflect them through the sphere.
static vector<S2Point> Reflect(const vector<S2Point>& pts) {
  vector<S2Point> negative_pts;
  for (const auto& p : pts) {
    negative_pts.push_back(-p);
  }
  return negative_pts;
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
  auto target_index = make_unique<MutableS2ShapeIndex>();

  auto pts = Reflect(ParsePointsOrDie("1:1"));
  target_index->Add(make_unique<S2PointVectorShape>(pts));

  S2Polyline line(Reflect(ParsePointsOrDie("4:5, 5:4")));
  target_index->Add(make_unique<S2Polyline::Shape>(&line));

  vector<S2Point> loop1 = Reflect(ParsePointsOrDie("20:20, 20:21, 21:20"));
  vector<S2Point> loop2 = Reflect(ParsePointsOrDie("10:10, 10:11, 11:10"));
  vector<vector<S2Point>> loops = {loop1, loop2};
  target_index->Add(make_unique<S2LaxPolygonShape>(loops));

  S2MaxDistanceShapeIndexTarget target(target_index.get());
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
  S2MaxDistanceShapeIndexTarget point_target(point_index.get());
  EXPECT_EQ((vector<int>{1}), GetContainingShapes(&point_target, *index, 5));

  // Check only the full polygon is returned for a full polygon target.
  auto full_polygon_index = MakeIndexOrDie("# # full");
  S2MaxDistanceShapeIndexTarget full_target(full_polygon_index.get());
  EXPECT_EQ((vector<int>{1}), GetContainingShapes(&full_target, *index, 5));

  // Check that nothing is returned for an empty polygon target.  (An empty
  // polygon has no connected components and does not intersect anything, so
  // according to the API of GetContainingShapes nothing should be returned.)
  auto empty_polygon_index = MakeIndexOrDie("# # empty");
  S2MaxDistanceShapeIndexTarget empty_target(empty_polygon_index.get());
  EXPECT_EQ((vector<int>{}), GetContainingShapes(&empty_target, *index, 5));
}
