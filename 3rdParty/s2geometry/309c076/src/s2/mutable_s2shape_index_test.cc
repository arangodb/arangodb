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

#include <functional>
#include <memory>
#include <numeric>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "s2/base/commandlineflags.h"
#include "s2/base/logging.h"
#include "s2/base/mutex.h"
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
#include "s2/s2edge_vector_shape.h"
#include "s2/s2error.h"
#include "s2/s2loop.h"
#include "s2/s2pointutil.h"
#include "s2/s2polygon.h"
#include "s2/s2shapeutil_coding.h"
#include "s2/s2shapeutil_contains_brute_force.h"
#include "s2/s2shapeutil_testing.h"
#include "s2/s2shapeutil_visit_crossing_edge_pairs.h"
#include "s2/s2testing.h"
#include "s2/s2text_format.h"
#include "s2/third_party/absl/memory/memory.h"

using absl::WrapUnique;
using absl::make_unique;
using s2textformat::MakePolyline;
using std::unique_ptr;
using std::vector;

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
    int short_edges = 0;  // number of edges counted toward subdivision
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
          if (has_edge && it.id().level() < max_level) {
            ++short_edges;
          }
        }
      }
    }
    EXPECT_LE(short_edges, index_.options().max_edges_per_cell());
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
    QuadraticValidate();
    TestEncodeDecode();
  }
}

TEST_F(MutableS2ShapeIndexTest, RandomUpdates) {
  // Allow the seed to be varied from the command line.
  S2Testing::rnd.Reset(FLAGS_s2_random_seed);

  // A few polylines.
  index_.Add(make_unique<S2Polyline::OwningShape>(
      MakePolyline("0:0, 2:1, 0:2, 2:3, 0:4, 2:5, 0:6")));
  index_.Add(make_unique<S2Polyline::OwningShape>(
      MakePolyline("1:0, 3:1, 1:2, 3:3, 1:4, 3:5, 1:6")));
  index_.Add(make_unique<S2Polyline::OwningShape>(
      MakePolyline("2:0, 4:1, 2:2, 4:3, 2:4, 4:5, 2:6")));

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
class LazyUpdatesTest : public ::testing::Test {
 public:
  LazyUpdatesTest() : num_updates_(0), num_readers_left_(0) {
  }

  // The function executed by each reader thread.
  void ReaderThread();

 protected:
  class ReaderThreadPool {
   public:
    ReaderThreadPool(LazyUpdatesTest* test, int num_threads)
        : threads_(make_unique<std::thread[]>(num_threads)),
          num_threads_(num_threads) {
      for (int i = 0; i < num_threads_; ++i) {
        threads_[i] = std::thread(&LazyUpdatesTest::ReaderThread, test);
      }
    }
    ~ReaderThreadPool() {
      for (int i = 0; i < num_threads_; ++i) threads_[i].join();
    }

   private:
    unique_ptr<std::thread[]> threads_;
    int num_threads_;
  };

  MutableS2ShapeIndex index_;
  // The following fields are guarded by lock_.
  absl::Mutex lock_;
  int num_updates_;
  int num_readers_left_;

  // Signalled when a new update is ready to be processed.
  absl::CondVar update_ready_;
  // Signalled when all readers have processed the latest update.
  absl::CondVar all_readers_done_;
};

void LazyUpdatesTest::ReaderThread() {
  lock_.Lock();
  for (int last_update = 0; ; last_update = num_updates_) {
    while (num_updates_ == last_update) {
      update_ready_.Wait(&lock_);
    }
    if (num_updates_ < 0) break;

    // The index is built on demand the first time we attempt to use it.
    // We intentionally release the lock so that many threads have a chance
    // to access the MutableS2ShapeIndex in parallel.
    lock_.Unlock();
    for (MutableS2ShapeIndex::Iterator it(&index_, S2ShapeIndex::BEGIN);
         !it.done(); it.Next()) {
      continue;
    }
    lock_.Lock();
    if (--num_readers_left_ == 0) {
      all_readers_done_.Signal();
    }
  }
  lock_.Unlock();
}

TEST_F(LazyUpdatesTest, ConstMethodsThreadSafe) {
  // Ensure that lazy updates are thread-safe.  In other words, make sure that
  // nothing bad happens when multiple threads call "const" methods that
  // cause pending updates to be applied.

  // The number of readers should be large enough so that it is likely that
  // several readers will be running at once (with a multiple-core CPU).
  const int kNumReaders = 8;
  ReaderThreadPool pool(this, kNumReaders);
  lock_.Lock();
  const int kIters = 100;
  for (int iter = 0; iter < kIters; ++iter) {
    // Loop invariant: lock_ is held and num_readers_left_ == 0.
    S2_DCHECK_EQ(0, num_readers_left_);
    // Since there are no readers, it is safe to modify the index.
    index_.Clear();
    int num_vertices = 4 * S2Testing::rnd.Skewed(10);  // Up to 4K vertices
    unique_ptr<S2Loop> loop(S2Loop::MakeRegularLoop(
        S2Testing::RandomPoint(), S2Testing::KmToAngle(5), num_vertices));
    index_.Add(make_unique<S2Loop::Shape>(loop.get()));
    num_readers_left_ = kNumReaders;
    ++num_updates_;
    update_ready_.SignalAll();
    while (num_readers_left_ > 0) {
      all_readers_done_.Wait(&lock_);
    }
  }
  // Signal the readers to exit.
  num_updates_ = -1;
  update_ready_.SignalAll();
  lock_.Unlock();
  // ReaderThreadPool destructor waits for all threads to complete.
}

TEST(MutableS2ShapeIndex, MixedGeometry) {
  // This test used to trigger a bug where the presence of a shape with an
  // interior could cause shapes that don't have an interior to suddenly
  // acquire one.  This would cause extra S2ShapeIndex cells to be created
  // that are outside the bounds of the given geometry.
  vector<unique_ptr<S2Polyline>> polylines;
  polylines.push_back(MakePolyline("0:0, 2:1, 0:2, 2:3, 0:4, 2:5, 0:6"));
  polylines.push_back(MakePolyline("1:0, 3:1, 1:2, 3:3, 1:4, 3:5, 1:6"));
  polylines.push_back(MakePolyline("2:0, 4:1, 2:2, 4:3, 2:4, 4:5, 2:6"));
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
