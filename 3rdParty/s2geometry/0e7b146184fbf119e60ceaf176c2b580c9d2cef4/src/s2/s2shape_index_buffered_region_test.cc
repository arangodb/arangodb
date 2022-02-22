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

#include <iostream>
#include <memory>
#include <gtest/gtest.h>
#include "s2/third_party/absl/memory/memory.h"
#include "s2/mutable_s2shape_index.h"
#include "s2/s2cap.h"
#include "s2/s2cell_union.h"
#include "s2/s2polygon.h"
#include "s2/s2region_coverer.h"
#include "s2/s2shape_index.h"
#include "s2/s2testing.h"
#include "s2/s2text_format.h"

using absl::make_unique;
using s2textformat::MakeIndexOrDie;
using s2textformat::MakePointOrDie;
using std::cout;

TEST(S2ShapeIndexBufferedRegion, EmptyIndex) {
  // Test buffering an empty S2ShapeIndex.
  MutableS2ShapeIndex index;
  S1ChordAngle radius(S1Angle::Degrees(2));
  S2ShapeIndexBufferedRegion region(&index, radius);
  S2RegionCoverer coverer;
  S2CellUnion covering = coverer.GetCovering(region);
  EXPECT_TRUE(covering.empty());
}

TEST(S2ShapeIndexBufferedRegion, FullPolygon) {
  // Test buffering an S2ShapeIndex that contains a full polygon.
  auto index = MakeIndexOrDie("# # full");
  S1ChordAngle radius(S1Angle::Degrees(2));
  S2ShapeIndexBufferedRegion region(index.get(), radius);
  S2RegionCoverer coverer;
  S2CellUnion covering = coverer.GetCovering(region);
  ASSERT_EQ(6, covering.num_cells());
  for (S2CellId id : covering) {
    EXPECT_TRUE(id.is_face());
  }
}

TEST(S2ShapeIndexBufferedRegion, FullAfterBuffering) {
  // Test a region that becomes the full polygon after buffering.
  auto index = MakeIndexOrDie("0:0 | 0:90 | 0:180 | 0:-90 | 90:0 | -90:0 # #");
  S1ChordAngle radius(S1Angle::Degrees(60));
  S2ShapeIndexBufferedRegion region(index.get(), radius);
  S2RegionCoverer coverer;
  coverer.mutable_options()->set_max_cells(1000);
  S2CellUnion covering = coverer.GetCovering(region);
  ASSERT_EQ(6, covering.num_cells());
  for (S2CellId id : covering) {
    EXPECT_TRUE(id.is_face());
  }
}

TEST(S2ShapeIndexBufferedRegion, PointZeroRadius) {
  // Test that buffering a point using a zero radius produces a non-empty
  // covering.  (This requires using "less than or equal to" distance tests.)
  auto index = MakeIndexOrDie("34:25 # #");
  S2ShapeIndexBufferedRegion region(index.get(), S1ChordAngle::Zero());
  S2RegionCoverer coverer;
  S2CellUnion covering = coverer.GetCovering(region);
  EXPECT_EQ(1, covering.num_cells());
  for (S2CellId id : covering) {
    EXPECT_TRUE(id.is_leaf());
  }
}

TEST(S2ShapeIndexBufferedRegion, BufferedPointVsCap) {
  // Compute an S2Cell covering of a buffered S2Point, then make sure that the
  // covering is equivalent to the corresponding S2Cap.
  auto index = MakeIndexOrDie("3:5 # #");
  S2Point point = MakePointOrDie("3:5");
  S1ChordAngle radius(S1Angle::Degrees(2));
  S2ShapeIndexBufferedRegion region(index.get(), radius);
  S2RegionCoverer coverer;
  coverer.mutable_options()->set_max_cells(50);
  S2CellUnion covering = coverer.GetCovering(region);
  S2Cap equivalent_cap(point, radius);
  S2Testing::CheckCovering(equivalent_cap, covering, true);
}

// Verifies that an arbitrary S2ShapeIndex is buffered correctly, by first
// converting the covering to an S2Polygon and then checking that (a) the
// S2Polygon contains the original geometry and (b) the distance between the
// original geometry and the boundary of the S2Polygon is at least "radius".
//
// The "radius" parameter is an S1Angle for convenience.
// TODO(ericv): Add Degrees, Radians, etc, methods to S1ChordAngle?
void TestBufferIndex(const string& index_str, S1Angle radius_angle,
                     S2RegionCoverer* coverer) {
  auto index = MakeIndexOrDie(index_str);
  S1ChordAngle radius(radius_angle);
  S2ShapeIndexBufferedRegion region(index.get(), radius);
  S2CellUnion covering = coverer->GetCovering(region);
  S2_LOG(INFO) << "Covering uses " << covering.num_cells()
            << " cells vs. max of " << coverer->options().max_cells();
  if (S2_VLOG_IS_ON(2)) {
    // Output the cells in the covering for visualization purposes.
    for (S2CellId id : covering) {
      cout << "\nS2Polygon: " << s2textformat::ToString(S2Polygon(S2Cell(id)));
    }
    cout << "\n\n" << std::flush;
  }
  // Compute an S2Polygon representing the union of the cells in the covering.
  S2Polygon covering_polygon;
  covering_polygon.InitToCellUnionBorder(covering);
  MutableS2ShapeIndex covering_index;
  covering_index.Add(make_unique<S2Polygon::Shape>(&covering_polygon));

  // (a) Check that the covering contains the original index.
  EXPECT_TRUE(S2BooleanOperation::Contains(covering_index, *index));

  // (b) Check that the distance between the boundary of the covering and the
  // the original indexed geometry is at least "radius".
  S2ClosestEdgeQuery query(&covering_index);
  query.mutable_options()->set_include_interiors(false);
  S2ClosestEdgeQuery::ShapeIndexTarget target(index.get());
  EXPECT_FALSE(query.IsDistanceLess(&target, radius));
}

TEST(S2ShapeIndexBufferedRegion, PointSet) {
  // Test buffering a set of points.
  S2RegionCoverer coverer;
  coverer.mutable_options()->set_max_cells(100);
  TestBufferIndex("10:20 | 10:23 | 10:26 # #", S1Angle::Degrees(5), &coverer);
}

TEST(S2ShapeIndexBufferedRegion, Polyline) {
  // Test buffering a polyline.
  S2RegionCoverer coverer;
  coverer.mutable_options()->set_max_cells(100);
  TestBufferIndex("# 10:5, 20:30, -10:60, -60:100 #",
                  S1Angle::Degrees(2), &coverer);
}

TEST(S2ShapeIndexBufferedRegion, PolygonWithHole) {
  // Test buffering a polygon with a hole.
  S2RegionCoverer coverer;
  coverer.mutable_options()->set_max_cells(100);
  TestBufferIndex("# # 10:10, 10:100, 70:0; 11:11, 69:0, 11:99",
                  S1Angle::Degrees(2), &coverer);
}
