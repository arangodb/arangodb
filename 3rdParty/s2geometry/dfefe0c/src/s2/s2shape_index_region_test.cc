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

#include "s2/s2shape_index_region.h"

#include <algorithm>
#include <gtest/gtest.h>
#include "s2/mutable_s2shape_index.h"
#include "s2/s2lax_loop_shape.h"

using std::vector;

namespace {

S2CellId MakeCellId(const string& str) {
  return S2CellId::FromDebugString(str);
}

// Pad by at least twice the maximum error for reliable results.
static const double kPadding = 2 * (S2::kFaceClipErrorUVCoord +
                                    S2::kIntersectsRectErrorUVDist);

std::unique_ptr<S2Shape> NewPaddedCell(S2CellId id, double padding_uv) {
  int ij[2], orientation;
  int face = id.ToFaceIJOrientation(&ij[0], &ij[1], &orientation);
  R2Rect uv = S2CellId::IJLevelToBoundUV(ij, id.level()).Expanded(padding_uv);
  vector<S2Point> vertices(4);
  for (int i = 0; i < 4; ++i) {
    vertices[i] = S2::FaceUVtoXYZ(face, uv.GetVertex(i)).Normalize();
  }
  return absl::make_unique<S2LaxLoopShape>(vertices);
}

TEST(S2ShapeIndexRegion, GetCapBound) {
  auto id = S2CellId::FromDebugString("3/0123012301230123012301230123");

  // Add a polygon that is slightly smaller than the cell being tested.
  MutableS2ShapeIndex index;
  index.Add(NewPaddedCell(id, -kPadding));
  S2Cap cell_bound = S2Cell(id).GetCapBound();
  S2Cap index_bound = MakeS2ShapeIndexRegion(&index).GetCapBound();
  EXPECT_TRUE(index_bound.Contains(cell_bound));

  // Note that S2CellUnion::GetCapBound returns a slightly larger bound than
  // S2Cell::GetBound even when the cell union consists of a single S2CellId.
  EXPECT_LE(index_bound.GetRadius(), 1.00001 * cell_bound.GetRadius());
}

TEST(S2ShapeIndexRegion, GetRectBound) {
  auto id = S2CellId::FromDebugString("3/0123012301230123012301230123");

  // Add a polygon that is slightly smaller than the cell being tested.
  MutableS2ShapeIndex index;
  index.Add(NewPaddedCell(id, -kPadding));
  S2LatLngRect cell_bound = S2Cell(id).GetRectBound();
  S2LatLngRect index_bound = MakeS2ShapeIndexRegion(&index).GetRectBound();
  EXPECT_EQ(index_bound, cell_bound);
}

TEST(S2ShapeIndexRegion, GetCellUnionBoundMultipleFaces) {
  vector<S2CellId> ids = { MakeCellId("3/00123"), MakeCellId("2/11200013") };
  MutableS2ShapeIndex index;
  for (auto id : ids) index.Add(NewPaddedCell(id, -kPadding));
  vector<S2CellId> covering;
  MakeS2ShapeIndexRegion(&index).GetCellUnionBound(&covering);
  std::sort(ids.begin(), ids.end());
  EXPECT_EQ(ids, covering);
}

TEST(S2ShapeIndexRegion, GetCellUnionBoundOneFace) {
  // This tests consists of 3 pairs of S2CellIds.  Each pair is located within
  // one of the children of face 5, namely the cells 5/0, 5/1, and 5/3.
  // We expect GetCellUnionBound to compute the smallest cell that bounds the
  // pair on each face.
  vector<S2CellId> input = {
    MakeCellId("5/010"), MakeCellId("5/0211030"),
    MakeCellId("5/110230123"), MakeCellId("5/11023021133"),
    MakeCellId("5/311020003003030303"), MakeCellId("5/311020023"),
  };
  vector<S2CellId> expected = {
    MakeCellId("5/0"), MakeCellId("5/110230"), MakeCellId("5/3110200")
  };
  MutableS2ShapeIndex index;
  for (auto id : input) {
    // Add each shape 3 times to ensure that the S2ShapeIndex subdivides.
    for (int copy = 0; copy < 3; ++copy) {
      index.Add(NewPaddedCell(id, -kPadding));
    }
  }
  vector<S2CellId> actual;
  MakeS2ShapeIndexRegion(&index).GetCellUnionBound(&actual);
  EXPECT_EQ(expected, actual);
}

TEST(S2ShapeIndexRegion, ContainsCellMultipleShapes) {
  auto id = S2CellId::FromDebugString("3/0123012301230123012301230123");

  // Add a polygon that is slightly smaller than the cell being tested.
  MutableS2ShapeIndex index;
  index.Add(NewPaddedCell(id, -kPadding));
  EXPECT_FALSE(MakeS2ShapeIndexRegion(&index).Contains(S2Cell(id)));

  // Add a second polygon that is slightly larger than the cell being tested.
  // Note that Contains() should return true if *any* shape contains the cell.
  index.Add(NewPaddedCell(id, kPadding));
  EXPECT_TRUE(MakeS2ShapeIndexRegion(&index).Contains(S2Cell(id)));

  // Verify that all children of the cell are also contained.
  for (S2CellId child = id.child_begin();
       child != id.child_end(); child = child.next()) {
    EXPECT_TRUE(MakeS2ShapeIndexRegion(&index).Contains(S2Cell(child)));
  }
}

TEST(S2ShapeIndexRegion, IntersectsShrunkenCell) {
  auto target = S2CellId::FromDebugString("3/0123012301230123012301230123");

  // Add a polygon that is slightly smaller than the cell being tested.
  MutableS2ShapeIndex index;
  index.Add(NewPaddedCell(target, -kPadding));
  auto region = MakeS2ShapeIndexRegion(&index);

  // Check that the index intersects the cell itself, but not any of the
  // neighboring cells.
  EXPECT_TRUE(region.MayIntersect(S2Cell(target)));
  vector<S2CellId> nbrs;
  target.AppendAllNeighbors(target.level(), &nbrs);
  for (S2CellId id : nbrs) {
    EXPECT_FALSE(region.MayIntersect(S2Cell(id)));
  }
}

TEST(S2ShapeIndexRegion, IntersectsExactCell) {
  auto target = S2CellId::FromDebugString("3/0123012301230123012301230123");

  // Adds a polygon that exactly follows a cell boundary.
  MutableS2ShapeIndex index;
  index.Add(NewPaddedCell(target, 0.0));
  auto region = MakeS2ShapeIndexRegion(&index);

  // Check that the index intersects the cell and all of its neighbors.
  vector<S2CellId> ids = {target};
  target.AppendAllNeighbors(target.level(), &ids);
  for (S2CellId id : ids) {
    EXPECT_TRUE(region.MayIntersect(S2Cell(id)));
  }
}

}  // namespace
