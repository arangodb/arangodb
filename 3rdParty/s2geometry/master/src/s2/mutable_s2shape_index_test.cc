// Copyright 2012 Google Inc. All Rights Reserved.
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

#include "s2/mutable_s2shape_index.h"

#include <algorithm>
#include <functional>
#include <memory>
#include <numeric>
#include <thread>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "absl/flags/reflection.h"
#include "absl/flags/flag.h"
#include "absl/memory/memory.h"
#include "absl/strings/str_format.h"

#include "s2/base/commandlineflags.h"
#include "s2/base/logging.h"
#include "s2/base/log_severity.h"
#include "s2/r2.h"
#include "s2/r2rect.h"
#include "s2/s1angle.h"
#include "s2/s2cap.h"
#include "s2/s2cell.h"
#include "s2/s2cell_id.h"
#include "s2/s2cell_union.h"
#include "s2/s2debug.h"
#include "s2/s2edge_clipping.h"
#include "s2/s2edge_crosser.h"
#include "s2/s2edge_distances.h"
#include "s2/s2edge_vector_shape.h"
#include "s2/s2error.h"
#include "s2/s2lax_polygon_shape.h"
#include "s2/s2loop.h"
#include "s2/s2point_vector_shape.h"
#include "s2/s2pointutil.h"
#include "s2/s2polygon.h"
#include "s2/s2shapeutil_coding.h"
#include "s2/s2shapeutil_contains_brute_force.h"
#include "s2/s2shapeutil_testing.h"
#include "s2/s2shapeutil_visit_crossing_edge_pairs.h"
#include "s2/s2testing.h"
#include "s2/s2text_format.h"
#include "s2/thread_testing.h"

using absl::make_unique;
using absl::WrapUnique;
using s2textformat::MakePolylineOrDie;
using std::string;
using std::unique_ptr;
using std::vector;

S2_DECLARE_double(s2shape_index_min_short_edge_fraction);
S2_DECLARE_double(s2shape_index_cell_size_to_long_edge_ratio);

class MutableS2ShapeIndexTest : public ::testing::Test {
 protected:
  // This test harness owns a MutableS2ShapeIndex for convenience.
  MutableS2ShapeIndex index_;

  // Verifies that that every cell of the index contains the correct edges, and
  // that no cells are missing from the index.  The running time of this
  // function is quadratic in the number of edges.
  void QuadraticValidate();

  // Given an edge and a cell id, determines whether or not the edge should be
  // present in that cell and verify that this matches "index_has_edge".
  void ValidateEdge(const S2Point& a, const S2Point& b,
                    S2CellId id, bool index_has_edge);

  // Given a shape and a cell id, determines whether or not the shape contains
  // the cell center and verify that this matches "index_contains_center".
  void ValidateInterior(const S2Shape* shape, S2CellId id,
                        bool index_contains_center);

  // Verifies that the index can be encoded and decoded without change.
  void TestEncodeDecode();

  using BatchDescriptor = MutableS2ShapeIndex::BatchDescriptor;

  // Converts the given vector of batches to a human-readable form.
  static string ToString(const vector<BatchDescriptor>& batches);

  // Verifies that removing and adding the given combination of shapes with
  // the given memory budget yields the expected vector of batches.
  void TestBatchGenerator(
      int num_edges_removed, const vector<int>& shape_edges_added,
      int64 tmp_memory_budget, int shape_id_begin,
      const vector<BatchDescriptor>& expected_batches);
};

void MutableS2ShapeIndexTest::QuadraticValidate() {
  // Iterate through a sequence of nonoverlapping cell ids that cover the
  // sphere and include as a subset all the cell ids used in the index.  For
  // each cell id, verify that the expected set of edges is present.

  // "min_cellid" is the first S2CellId that has not been validated yet.
  S2CellId min_cellid = S2CellId::Begin(S2CellId::kMaxLevel);
  for (MutableS2ShapeIndex::Iterator it(&index_, S2ShapeIndex::BEGIN);
       ; it.Next()) {
    // Generate a list of S2CellIds ("skipped cells") that cover the gap
    // between the last cell we validated and the next cell in the index.
    S2CellUnion skipped;
    if (!it.done()) {
      S2CellId cellid = it.id();
      EXPECT_GE(cellid, min_cellid);
      skipped.InitFromBeginEnd(min_cellid, cellid.range_min());
      min_cellid = cellid.range_max().next();
    } else {
      // Validate the empty cells beyond the last cell in the index.
      skipped.InitFromBeginEnd(min_cellid, S2CellId::End(S2CellId::kMaxLevel));
    }
    // Iterate through all the shapes, simultaneously validating the current
    // index cell and all the skipped cells.
    int num_edges = 0;              // all edges in the cell
    int num_short_edges = 0;        // "short" edges
    int num_containing_shapes = 0;  // shapes containing cell's entry vertex
    for (int id = 0; id < index_.num_shape_ids(); ++id) {
      const S2Shape* shape = index_.shape(id);
      const S2ClippedShape* clipped = nullptr;
      if (!it.done()) clipped = it.cell().find_clipped(id);

      // First check that contains_center() is set correctly.
      for (S2CellId skipped_id : skipped) {
        ValidateInterior(shape, skipped_id, false);
      }
      if (!it.done()) {
        bool contains_center = clipped && clipped->contains_center();
        ValidateInterior(shape, it.id(), contains_center);
        S2PaddedCell pcell(it.id(), MutableS2ShapeIndex::kCellPadding);
        if (shape != nullptr) {
          num_containing_shapes +=
              s2shapeutil::ContainsBruteForce(*shape, pcell.GetEntryVertex());
        }
      }
      // If this shape has been released, it should not be present at all.
      if (shape == nullptr) {
        EXPECT_EQ(nullptr, clipped);
        continue;
      }
      // Otherwise check that the appropriate edges are present.
      for (int e = 0; e < shape->num_edges(); ++e) {
        auto edge = shape->edge(e);
        for (int j = 0; j < skipped.num_cells(); ++j) {
          ValidateEdge(edge.v0, edge.v1, skipped.cell_id(j), false);
        }
        if (!it.done()) {
          bool has_edge = clipped && clipped->ContainsEdge(e);
          ValidateEdge(edge.v0, edge.v1, it.id(), has_edge);
          int max_level = index_.GetEdgeMaxLevel(edge);
          if (has_edge) {
            ++num_edges;
            if (it.id().level() < max_level) ++num_short_edges;
          }
        }
      }
    }
    // This mirrors the calculation in MutableS2ShapeIndex::MakeIndexCell().
    // It is designed to ensure that the index size is always linear in the
    // number of indexed edges.
    int max_short_edges = std::max(
        index_.options().max_edges_per_cell(),
        static_cast<int>(
            absl::GetFlag(FLAGS_s2shape_index_min_short_edge_fraction) *
            (num_edges + num_containing_shapes)));
    EXPECT_LE(num_short_edges, max_short_edges);
    if (it.done()) break;
  }
}

// Verify that "index_has_edge" is true if and only if the edge AB intersects
// the given cell id.
void MutableS2ShapeIndexTest::ValidateEdge(const S2Point& a, const S2Point& b,
                                           S2CellId id, bool index_has_edge) {
  // Expand or shrink the padding slightly to account for errors in the
  // function we use to test for intersection (IntersectsRect).
  double padding = MutableS2ShapeIndex::kCellPadding;
  padding += (index_has_edge ? 1 : -1) * S2::kIntersectsRectErrorUVDist;
  R2Rect bound = id.GetBoundUV().Expanded(padding);
  R2Point a_uv, b_uv;
  EXPECT_EQ(S2::ClipToPaddedFace(a, b, id.face(), padding, &a_uv, &b_uv)
            && S2::IntersectsRect(a_uv, b_uv, bound),
            index_has_edge);
}

void MutableS2ShapeIndexTest::ValidateInterior(
    const S2Shape* shape, S2CellId id, bool index_contains_center) {
  if (shape == nullptr) {
    EXPECT_FALSE(index_contains_center);
  } else {
    EXPECT_EQ(s2shapeutil::ContainsBruteForce(*shape, id.ToPoint()),
              index_contains_center);
  }
}

void MutableS2ShapeIndexTest::TestEncodeDecode() {
  Encoder encoder;
  index_.Encode(&encoder);
  Decoder decoder(encoder.base(), encoder.length());
  MutableS2ShapeIndex index2;
  ASSERT_TRUE(index2.Init(&decoder, s2shapeutil::WrappedShapeFactory(&index_)));
  s2testing::ExpectEqual(index_, index2);
}

/*static*/ string MutableS2ShapeIndexTest::ToString(
    const vector<BatchDescriptor>& batches) {
  string result;
  for (const auto& batch : batches) {
    if (!result.empty()) result += ", ";
    absl::StrAppendFormat(&result, "(%d:%d, %d:%d, %d)",
                          batch.begin.shape_id, batch.begin.edge_id,
                          batch.end.shape_id, batch.end.edge_id,
                          batch.num_edges);
  }
  return result;
}

void MutableS2ShapeIndexTest::TestBatchGenerator(
    int num_edges_removed, const vector<int>& shape_edges_added,
    int64 tmp_memory_budget, int shape_id_begin,
    const vector<BatchDescriptor>& expected_batches) {
  absl::FlagSaver fs;
  absl::SetFlag(&FLAGS_s2shape_index_tmp_memory_budget, tmp_memory_budget);

  int num_edges_added = 0;
  for (auto n : shape_edges_added) num_edges_added += n;
  MutableS2ShapeIndex::BatchGenerator bgen(num_edges_removed, num_edges_added,
                                           shape_id_begin);
  for (int i = 0; i < shape_edges_added.size(); ++i) {
    bgen.AddShape(shape_id_begin + i, shape_edges_added[i]);
  }
  auto actual_batches = bgen.Finish();
  EXPECT_EQ(ToString(actual_batches), ToString(expected_batches));
}

namespace {

void TestIteratorMethods(const MutableS2ShapeIndex& index) {
  MutableS2ShapeIndex::Iterator it(&index, S2ShapeIndex::BEGIN);
  EXPECT_FALSE(it.Prev());
  it.Finish();
  EXPECT_TRUE(it.done());
  vector<S2CellId> ids;
  MutableS2ShapeIndex::Iterator it2(&index);
  S2CellId min_cellid = S2CellId::Begin(S2CellId::kMaxLevel);
  for (it.Begin(); !it.done(); it.Next()) {
    S2CellId cellid = it.id();
    auto skipped = S2CellUnion::FromBeginEnd(min_cellid, cellid.range_min());
    for (S2CellId skipped_id : skipped) {
      EXPECT_FALSE(it2.Locate(skipped_id.ToPoint()));
      EXPECT_EQ(S2ShapeIndex::DISJOINT, it2.Locate(skipped_id));
      it2.Begin();
      it2.Seek(skipped_id);
      EXPECT_EQ(cellid, it2.id());
    }
    if (!ids.empty()) {
      it2 = it;
      EXPECT_TRUE(it2.Prev());
      EXPECT_EQ(ids.back(), it2.id());
      it2.Next();
      EXPECT_EQ(cellid, it2.id());
      it2.Seek(ids.back());
      EXPECT_EQ(ids.back(), it2.id());
    }
    it2.Begin();
    EXPECT_EQ(cellid.ToPoint(), it.center());
    EXPECT_TRUE(it2.Locate(it.center()));
    EXPECT_EQ(cellid, it2.id());
    it2.Begin();
    EXPECT_EQ(S2ShapeIndex::INDEXED, it2.Locate(cellid));
    EXPECT_EQ(cellid, it2.id());
    if (!cellid.is_face()) {
      it2.Begin();
      EXPECT_EQ(S2ShapeIndex::SUBDIVIDED, it2.Locate(cellid.parent()));
      EXPECT_LE(it2.id(), cellid);
      EXPECT_GE(it2.id(), cellid.parent().range_min());
    }
    if (!cellid.is_leaf()) {
      for (int i = 0; i < 4; ++i) {
        it2.Begin();
        EXPECT_EQ(S2ShapeIndex::INDEXED, it2.Locate(cellid.child(i)));
        EXPECT_EQ(cellid, it2.id());
      }
    }
    ids.push_back(cellid);
    min_cellid = cellid.range_max().next();
  }
}

// NOTE(ericv): The tests below are all somewhat fragile since they depend on
// the internal BatchGenerator heuristics; if these heuristics change
// (including constants) then the tests below may need to change as well.

TEST_F(MutableS2ShapeIndexTest, RemoveFullPolygonBatch) {
  TestBatchGenerator(0, {}, 100 /*bytes*/, 7,
                     {{{7, 0}, {7, 0}, 0}});
}

TEST_F(MutableS2ShapeIndexTest, AddFullPolygonBatch) {
  TestBatchGenerator(0, {0}, 100 /*bytes*/, 7,
                     {{{7, 0}, {8, 0}, 0}});
}

TEST_F(MutableS2ShapeIndexTest, RemoveManyEdgesInOneBatch) {
  // Test removing more edges than would normally fit in a batch.  For good
  // measure we also add two full polygons in the same batch.
  TestBatchGenerator(1000, {0, 0}, 100 /*bytes*/, 7,
                     {{{7, 0}, {9, 0}, 1000}});
}

TEST_F(MutableS2ShapeIndexTest, RemoveAndAddEdgesInOneBatch) {
  // Test removing and adding edges in one batch.
  TestBatchGenerator(3, {4, 5}, 10000 /*bytes*/, 7,
                     {{{7, 0}, {9, 0}, 12}});
}

TEST_F(MutableS2ShapeIndexTest, RemoveAndAddEdgesInTwoBatches) {
  // Test removing many edges and then adding a few.
  TestBatchGenerator(1000, {3}, 1000 /*bytes*/, 7,
                     {{{7, 0}, {7, 0}, 1000},
                      {{7, 0}, {8, 0}, 3}});
}

TEST_F(MutableS2ShapeIndexTest, RemoveAndAddEdgesAndFullPolygonsInTwoBatches) {
  // Like the above, but also add two full polygons such that one polygon is
  // processed in each batch.
  TestBatchGenerator(1000, {0, 3, 0}, 1000 /*bytes*/, 7,
                     {{{7, 0}, {8, 0}, 1000},
                      {{8, 0}, {10, 0}, 3}});
}

TEST_F(MutableS2ShapeIndexTest, SeveralShapesInOneBatch) {
  // Test adding several shapes in one batch.
  TestBatchGenerator(0, {3, 4, 5}, 10000 /*bytes*/, 7,
                     {{{7, 0}, {10, 0}, 12}});
}

TEST_F(MutableS2ShapeIndexTest, GroupSmallShapesIntoBatches) {
  // Test adding several small shapes that must be split into batches.
  // 10000 bytes ~= temporary space to process 48 edges.
  TestBatchGenerator(0, {20, 20, 20, 20, 20}, 10000 /*bytes*/, 7,
                     {{{7, 0}, {9, 0}, 40},
                      {{9, 0}, {11, 0}, 40},
                      {{11, 0}, {12, 0}, 20}});
}

TEST_F(MutableS2ShapeIndexTest, AvoidPartialShapeInBatch) {
  // Test adding a small shape followed by a large shape that won't fit in the
  // same batch as the small shape, but will fit in its own separate batch.
  // 10000 bytes ~= temporary space to process 48 edges.
  TestBatchGenerator(0, {20, 40, 20}, 10000 /*bytes*/, 7,
                     {{{7, 0}, {8, 0}, 20},
                      {{8, 0}, {9, 0}, 40},
                      {{9, 0}, {10, 0}, 20}});
}

TEST_F(MutableS2ShapeIndexTest, SplitShapeIntoTwoBatches) {
  // Test adding a few small shapes, then a large shape that can be split
  // across the remainder of the first batch plus the next batch.  The first
  // two batches should have the same amount of remaining space relative to
  // their maximum size.  (For 10000 bytes of temporary space, the ideal batch
  // sizes are 48, 46, 45.)
  //
  // Note that we need a separate batch for the full polygon at the end, even
  // though it has no edges, because partial shapes must always be the last
  // shape in their batch.
  TestBatchGenerator(0, {20, 60, 0}, 10000 /*bytes*/, 7,
                     {{{7, 0}, {8, 21}, 41},
                      {{8, 21}, {9, 0}, 39},
                      {{9, 0}, {10, 0}, 0}});
}

TEST_F(MutableS2ShapeIndexTest, RemoveEdgesAndAddPartialShapeInSameBatch) {
  // Test a batch that consists of removing some edges and then adding a
  // partial shape.  We also check that the small shape at the end is put into
  // its own batch, since partial shapes must be the last shape in their batch.
  TestBatchGenerator(20, {60, 5}, 10000 /*bytes*/, 7,
                     {{{7, 0}, {7, 21}, 41},
                      {{7, 21}, {8, 0}, 39},
                      {{8, 0}, {9, 0}, 5}});
}

TEST_F(MutableS2ShapeIndexTest, SplitShapeIntoManyBatches) {
  // Like the above except that the shape is split into 10 batches.  With
  // 10000 bytes of temporary space, the ideal batch sizes are 63, 61, 59, 57,
  // 55, 53, 51, 49, 48, 46.  The first 8 batches are as full as possible,
  // while the last two batches have the same amount of remaining space
  // relative to their ideal size.  There is also a small batch at the end.
  TestBatchGenerator(0, {20, 500, 5}, 10000 /*bytes*/, 7,
                     {{{7, 0}, {8, 43}, 63},
                      {{8, 43}, {8, 104}, 61},
                      {{8, 104}, {8, 163}, 59},
                      {{8, 163}, {8, 220}, 57},
                      {{8, 220}, {8, 275}, 55},
                      {{8, 275}, {8, 328}, 53},
                      {{8, 328}, {8, 379}, 51},
                      {{8, 379}, {8, 428}, 49},
                      {{8, 428}, {8, 465}, 37},
                      {{8, 465}, {9, 0}, 35},
                      {{9, 0}, {10, 0}, 5}});
}

TEST_F(MutableS2ShapeIndexTest, SpaceUsed) {
  index_.Add(make_unique<S2EdgeVectorShape>(S2Point(1, 0, 0),
                                            S2Point(0, 1, 0)));
  EXPECT_FALSE(index_.is_fresh());
  size_t size_before = index_.SpaceUsed();
  EXPECT_FALSE(index_.is_fresh());

  QuadraticValidate();
  size_t size_after = index_.SpaceUsed();

  EXPECT_TRUE(index_.is_fresh());

  EXPECT_TRUE(size_after > size_before);
}

TEST_F(MutableS2ShapeIndexTest, NoEdges) {
  MutableS2ShapeIndex::Iterator it(&index_, S2ShapeIndex::BEGIN);
  EXPECT_TRUE(it.done());
  TestIteratorMethods(index_);
  TestEncodeDecode();
}

TEST_F(MutableS2ShapeIndexTest, OneEdge) {
  EXPECT_EQ(0, index_.Add(make_unique<S2EdgeVectorShape>(S2Point(1, 0, 0),
                                                         S2Point(0, 1, 0))));
  QuadraticValidate();
  TestIteratorMethods(index_);
  TestEncodeDecode();
}

TEST_F(MutableS2ShapeIndexTest, ShrinkToFitOptimization) {
  // This used to trigger a bug in the ShrinkToFit optimization.  The loop
  // below contains almost all of face 0 except for a small region in the
  // 0/00000 subcell.  That subcell is the only one that contains any edges.
  // This caused the index to be built only in that subcell.  However, all the
  // other cells on that face should also have index entries, in order to
  // indicate that they are contained by the loop.
  unique_ptr<S2Loop> loop(S2Loop::MakeRegularLoop(
      S2Point(1, 0.5, 0.5).Normalize(), S1Angle::Degrees(89), 100));
  index_.Add(make_unique<S2Loop::Shape>(loop.get()));
  QuadraticValidate();
  TestEncodeDecode();
}

TEST_F(MutableS2ShapeIndexTest, LoopsSpanningThreeFaces) {
  S2Polygon polygon;
  const int kNumEdges = 100;  // Validation is quadratic
  // Construct two loops consisting of kNumEdges vertices each, centered
  // around the cube vertex at the start of the Hilbert curve.
  S2Testing::ConcentricLoopsPolygon(S2Point(1, -1, -1).Normalize(), 2,
                                    kNumEdges, &polygon);
  vector<unique_ptr<S2Loop>> loops = polygon.Release();
  for (auto& loop : loops) {
    index_.Add(make_unique<S2Loop::Shape>(&*loop));
  }
  QuadraticValidate();
  TestIteratorMethods(index_);
  TestEncodeDecode();
}

TEST_F(MutableS2ShapeIndexTest, ManyIdenticalEdges) {
  const int kNumEdges = 100;  // Validation is quadratic
  S2Point a = S2Point(0.99, 0.99, 1).Normalize();
  S2Point b = S2Point(-0.99, -0.99, 1).Normalize();
  for (int i = 0; i < kNumEdges; ++i) {
    EXPECT_EQ(i, index_.Add(make_unique<S2EdgeVectorShape>(a, b)));
  }
  QuadraticValidate();
  TestIteratorMethods(index_);
  TestEncodeDecode();
  // Since all edges span the diagonal of a face, no subdivision should
  // have occurred (with the default index options).
  for (MutableS2ShapeIndex::Iterator it(&index_, S2ShapeIndex::BEGIN);
       !it.done(); it.Next()) {
    EXPECT_EQ(0, it.id().level());
  }
}

TEST_F(MutableS2ShapeIndexTest, DegenerateEdge) {
  // This test verifies that degenerate edges are supported.  The following
  // point is a cube face vertex, and so it should be indexed in 3 cells.
  S2Point a = S2Point(1, 1, 1).Normalize();
  auto shape = make_unique<S2EdgeVectorShape>();
  shape->Add(a, a);
  index_.Add(std::move(shape));
  QuadraticValidate();
  TestEncodeDecode();
  // Check that exactly 3 index cells contain the degenerate edge.
  int count = 0;
  for (MutableS2ShapeIndex::Iterator it(&index_, S2ShapeIndex::BEGIN);
       !it.done(); it.Next(), ++count) {
    EXPECT_TRUE(it.id().is_leaf());
    EXPECT_EQ(1, it.cell().num_clipped());
    EXPECT_EQ(1, it.cell().clipped(0).num_edges());
  }
  EXPECT_EQ(3, count);
}

TEST_F(MutableS2ShapeIndexTest, ManyTinyEdges) {
  // This test adds many edges to a single leaf cell, to check that
  // subdivision stops when no further subdivision is possible.
  const int kNumEdges = 100;  // Validation is quadratic
  // Construct two points in the same leaf cell.
  S2Point a = S2CellId(S2Point(1, 0, 0)).ToPoint();
  S2Point b = (a + S2Point(0, 1e-12, 0)).Normalize();
  auto shape = make_unique<S2EdgeVectorShape>();
  for (int i = 0; i < kNumEdges; ++i) {
    shape->Add(a, b);
  }
  index_.Add(std::move(shape));
  QuadraticValidate();
  TestEncodeDecode();
  // Check that there is exactly one index cell and that it is a leaf cell.
  MutableS2ShapeIndex::Iterator it(&index_, S2ShapeIndex::BEGIN);
  ASSERT_TRUE(!it.done());
  EXPECT_TRUE(it.id().is_leaf());
  it.Next();
  EXPECT_TRUE(it.done());
}

TEST_F(MutableS2ShapeIndexTest, SimpleUpdates) {
  // Add 5 loops one at a time, then release them one at a time,
  // validating the index at each step.
  S2Polygon polygon;
  S2Testing::ConcentricLoopsPolygon(S2Point(1, 0, 0), 5, 20, &polygon);
  for (int i = 0; i < polygon.num_loops(); ++i) {
    index_.Add(make_unique<S2Loop::Shape>(polygon.loop(i)));
    QuadraticValidate();
  }
  for (int id = 0; id < polygon.num_loops(); ++id) {
    index_.Release(id);
    EXPECT_EQ(index_.shape(id), nullptr);
    QuadraticValidate();
    TestEncodeDecode();
  }
}

TEST_F(MutableS2ShapeIndexTest, RandomUpdates) {
  // Set the temporary memory budget such that at least one shape needs to be
  // split into multiple update batches (namely, the "5 concentric rings"
  // polygon below which needs ~25KB of temporary space).
  absl::FlagSaver fs;
  absl::SetFlag(&FLAGS_s2shape_index_tmp_memory_budget, 10000);

  // Allow the seed to be varied from the command line.
  S2Testing::rnd.Reset(absl::GetFlag(FLAGS_s2_random_seed));

  // A few polylines.
  index_.Add(make_unique<S2Polyline::OwningShape>(
      MakePolylineOrDie("0:0, 2:1, 0:2, 2:3, 0:4, 2:5, 0:6")));
  index_.Add(make_unique<S2Polyline::OwningShape>(
      MakePolylineOrDie("1:0, 3:1, 1:2, 3:3, 1:4, 3:5, 1:6")));
  index_.Add(make_unique<S2Polyline::OwningShape>(
      MakePolylineOrDie("2:0, 4:1, 2:2, 4:3, 2:4, 4:5, 2:6")));

  // A loop that used to trigger an indexing bug.
  index_.Add(make_unique<S2Loop::OwningShape>(S2Loop::MakeRegularLoop(
      S2Point(1, 0.5, 0.5).Normalize(), S1Angle::Degrees(89), 20)));

  // Five concentric loops.
  S2Polygon polygon5;
  S2Testing::ConcentricLoopsPolygon(S2Point(1, -1, -1).Normalize(),
                                    5, 20, &polygon5);
  for (int i = 0; i < polygon5.num_loops(); ++i) {
    index_.Add(make_unique<S2Loop::Shape>(polygon5.loop(i)));
  }

  // Two clockwise loops around S2Cell cube vertices.
  index_.Add(make_unique<S2Loop::OwningShape>(S2Loop::MakeRegularLoop(
      S2Point(-1, 1, 1).Normalize(), S1Angle::Radians(M_PI - 0.001), 10)));
  index_.Add(make_unique<S2Loop::OwningShape>(S2Loop::MakeRegularLoop(
      S2Point(-1, -1, -1).Normalize(), S1Angle::Radians(M_PI - 0.001), 10)));

  // A shape with no edges and no interior.
  index_.Add(make_unique<S2Loop::OwningShape>(
      make_unique<S2Loop>(S2Loop::kEmpty())));

  // A shape with no edges that covers the entire sphere.
  index_.Add(make_unique<S2Loop::OwningShape>(
      make_unique<S2Loop>(S2Loop::kFull())));

  vector<unique_ptr<S2Shape>> released;
  vector<int> added(index_.num_shape_ids());
  std::iota(added.begin(), added.end(), 0);
  QuadraticValidate();
  TestEncodeDecode();
  for (int iter = 0; iter < 100; ++iter) {
    S2_VLOG(1) << "Iteration: " << iter;
    // Choose some shapes to add and release.
    int num_updates = 1 + S2Testing::rnd.Skewed(5);
    for (int n = 0; n < num_updates; ++n) {
      if (S2Testing::rnd.OneIn(2) && !added.empty()) {
        int i = S2Testing::rnd.Uniform(added.size());
        S2_VLOG(1) << "  Released shape " << added[i]
                << " (" << index_.shape(added[i]) << ")";
        released.push_back(index_.Release(added[i]));
        added.erase(added.begin() + i);
      } else if (!released.empty()) {
        int i = S2Testing::rnd.Uniform(released.size());
        S2Shape* shape = released[i].get();
        index_.Add(std::move(released[i]));  // Changes shape->id().
        released.erase(released.begin() + i);
        added.push_back(shape->id());
        S2_VLOG(1) << "  Added shape " << shape->id()
                << " (" << shape << ")";
      }
    }
    QuadraticValidate();
    TestEncodeDecode();
  }
}

// A test that repeatedly updates "index_" in one thread and attempts to
// concurrently read the index_ from several other threads.  When all threads
// have finished reading, the first thread makes another update.
//
// Note that we only test concurrent read access, since MutableS2ShapeIndex
// requires all updates to be single-threaded and not concurrent with any
// reads.
class LazyUpdatesTest : public s2testing::ReaderWriterTest {
 public:
  LazyUpdatesTest() {}

  void WriteOp() override {
    index_.Clear();
    int num_vertices = 4 * S2Testing::rnd.Skewed(10);  // Up to 4K vertices
    unique_ptr<S2Loop> loop(S2Loop::MakeRegularLoop(
        S2Testing::RandomPoint(), S2Testing::KmToAngle(5), num_vertices));
    index_.Add(make_unique<S2Loop::OwningShape>(std::move(loop)));
  }

  void ReadOp() override {
    for (MutableS2ShapeIndex::Iterator it(&index_, S2ShapeIndex::BEGIN);
         !it.done(); it.Next()) {
      continue;  // NOLINT
    }
  }

 protected:
  MutableS2ShapeIndex index_;
};

TEST(MutableS2ShapeIndex, ConstMethodsThreadSafe) {
  // Ensure that lazy updates are thread-safe.  In other words, make sure that
  // nothing bad happens when multiple threads call "const" methods that
  // cause pending updates to be applied.
  LazyUpdatesTest test;

  // The number of readers should be large enough so that it is likely that
  // several readers will be running at once (with a multiple-core CPU).
  const int kNumReaders = 8;
  const int kIters = 100;
  test.Run(kNumReaders, kIters);
}

TEST(MutableS2ShapeIndex, MixedGeometry) {
  // This test used to trigger a bug where the presence of a shape with an
  // interior could cause shapes that don't have an interior to suddenly
  // acquire one.  This would cause extra S2ShapeIndex cells to be created
  // that are outside the bounds of the given geometry.
  vector<unique_ptr<S2Polyline>> polylines;
  polylines.push_back(MakePolylineOrDie("0:0, 2:1, 0:2, 2:3, 0:4, 2:5, 0:6"));
  polylines.push_back(MakePolylineOrDie("1:0, 3:1, 1:2, 3:3, 1:4, 3:5, 1:6"));
  polylines.push_back(MakePolylineOrDie("2:0, 4:1, 2:2, 4:3, 2:4, 4:5, 2:6"));
  MutableS2ShapeIndex index;
  for (auto& polyline : polylines) {
    index.Add(make_unique<S2Polyline::OwningShape>(std::move(polyline)));
  }
  S2Loop loop(S2Cell(S2CellId::Begin(S2CellId::kMaxLevel)));
  index.Add(make_unique<S2Loop::Shape>(&loop));
  MutableS2ShapeIndex::Iterator it(&index);
  // No geometry intersects face 1, so there should be no index cells there.
  EXPECT_EQ(S2ShapeIndex::DISJOINT, it.Locate(S2CellId::FromFace(1)));
}

TEST_F(MutableS2ShapeIndexTest, LinearSpace) {
  // Build an index that requires FLAGS_s2shape_index_min_short_edge_fraction
  // to be non-zero in order to use a non-quadratic amount of space.

  // Uncomment the following line to check whether this test works properly.
  // FLAGS_s2shape_index_min_short_edge_fraction = 0;

  // Set the maximum number of "short" edges per cell to 1 so that we can
  // implement this test using a smaller index.
  MutableS2ShapeIndex::Options options;
  options.set_max_edges_per_cell(1);
  index_.Init(options);

  // The idea is to create O(n) copies of a single long edge, along with O(n)
  // clusters of (M + 1) points equally spaced along the long edge, where "M"
  // is the max_edges_per_cell() parameter.  The edges are divided such that
  // there are equal numbers of long and short edges; this maximizes the index
  // size when FLAGS_s2shape_index_min_short_edge_fraction is set to zero.
  const int kNumEdges = 100;  // Validation is quadratic
  int edges_per_cluster = options.max_edges_per_cell() + 1;
  int num_clusters = (kNumEdges / 2) / edges_per_cluster;

  // Create the long edges.
  S2Point a(1, 0, 0), b(0, 1, 0);
  for (int i = 0; i < kNumEdges / 2; ++i) {
    index_.Add(make_unique<S2EdgeVectorShape>(a, b));
  }
  // Create the clusters of short edges.
  for (int k = 0; k < num_clusters; ++k) {
    S2Point p = S2::Interpolate(a, b, k / (num_clusters - 1.0));
    vector<S2Point> points(edges_per_cluster, p);
    index_.Add(make_unique<S2PointVectorShape>(points));
  }
  QuadraticValidate();

  // The number of index cells should not exceed the number of clusters.
  int cell_count = 0;
  for (MutableS2ShapeIndex::Iterator it(&index_, S2ShapeIndex::BEGIN);
       !it.done(); it.Next()) {
    ++cell_count;
  }
  EXPECT_LE(cell_count, num_clusters);
}

TEST_F(MutableS2ShapeIndexTest, LongIndexEntriesBound) {
  // This test demonstrates that the c2 = 366 upper bound (using default
  // parameter values) mentioned in the .cc file is achievable.

  // Set the maximum number of "short" edges per cell to 1 so that we can test
  // using a smaller index.
  MutableS2ShapeIndex::Options options;
  options.set_max_edges_per_cell(1);
  index_.Init(options);

  // This is a worst-case edge AB that touches as many cells as possible at
  // level 30 while still being considered "short" at level 29.  We create an
  // index consisting of two copies of this edge plus a full polygon.
  S2Point a = S2::FaceSiTitoXYZ(0, 0, (1 << 30) + 0).Normalize();
  S2Point b = S2::FaceSiTitoXYZ(0, 0, (1 << 30) + 6).Normalize();
  for (int i = 0; i < 2; ++i) {
    index_.Add(make_unique<S2EdgeVectorShape>(a, b));
  }
  index_.Add(make_unique<S2LaxPolygonShape>(vector<vector<S2Point>>{{}}));

  // Count the number of index cells at each level.
  vector<int> counts(S2CellId::kMaxLevel + 1);
  for (MutableS2ShapeIndex::Iterator it(&index_, S2ShapeIndex::BEGIN);
       !it.done(); it.Next()) {
    ++counts[it.id().level()];
  }
  int sum = 0;
  for (int i = 0; i < counts.size(); ++i) {
    S2_LOG(INFO) << i << ": " << counts[i];
    sum += counts[i];
  }
  EXPECT_EQ(sum, 366);
}

// Test move-construct and move-assign functionality of `S2Shape`.  It has an id
// value which is set when it's added to an index.  So we can create two
// `S2LaxPolygonShape`s, add them to an index, then
TEST(MutableS2ShapeIndex, ShapeIdSwaps) {
  MutableS2ShapeIndex index;
  index.Add(s2textformat::MakeLaxPolylineOrDie("1:1, 2:2"));
  index.Add(s2textformat::MakeLaxPolylineOrDie("3:3, 4:4"));
  index.Add(s2textformat::MakeLaxPolylineOrDie("5:5, 6:6"));

  S2LaxPolylineShape& a = *down_cast<S2LaxPolylineShape*>(index.shape(1));
  S2LaxPolylineShape& b = *down_cast<S2LaxPolylineShape*>(index.shape(2));
  EXPECT_EQ(a.id(), 1);
  EXPECT_EQ(b.id(), 2);

  // Verify move construction moves the id value.
  S2LaxPolylineShape c(std::move(a));
  EXPECT_EQ(c.id(), 1);
  s2testing::ExpectEqual(c, *s2textformat::MakeLaxPolylineOrDie("3:3, 4:4"));

  // Verify move assignment moves the id value.
  S2LaxPolylineShape d;
  d = std::move(b);
  EXPECT_EQ(d.id(), 2);
  s2testing::ExpectEqual(d, *s2textformat::MakeLaxPolylineOrDie("5:5, 6:6"));
}

TEST(S2Shape, user_data) {
  struct MyData {
    int x, y;
    MyData(int _x, int _y) : x(_x), y(_y) {}
  };
  class MyEdgeVectorShape : public S2EdgeVectorShape {
   public:
    explicit MyEdgeVectorShape(const MyData& data)
        : S2EdgeVectorShape(), data_(data) {
    }
    const void* user_data() const override { return &data_; }
    void* mutable_user_data() override { return &data_; }

   private:
    MyData data_;
  };
  MyEdgeVectorShape shape(MyData(3, 5));
  MyData* data = static_cast<MyData*>(shape.mutable_user_data());
  S2_DCHECK_EQ(3, data->x);
  data->y = 10;
  S2_DCHECK_EQ(10, static_cast<const MyData*>(shape.user_data())->y);
}

}  // namespace
