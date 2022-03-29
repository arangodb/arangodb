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

// Author: ericv@google.com (Eric Veach)

#include "s2/encoded_s2shape_index.h"

#include <algorithm>
#include <map>
#include <utility>
#include <vector>

#include <gtest/gtest.h>
#include "absl/base/call_once.h"
#include "absl/flags/flag.h"
#include "absl/memory/memory.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_cat.h"
#include "s2/mutable_s2shape_index.h"
#include "s2/s2builder.h"
#include "s2/s2builderutil_s2polyline_layer.h"
#include "s2/s2builderutil_snap_functions.h"
#include "s2/s2cap.h"
#include "s2/s2closest_edge_query.h"
#include "s2/s2contains_point_query.h"
#include "s2/s2edge_distances.h"
#include "s2/s2latlng.h"
#include "s2/s2lax_polygon_shape.h"
#include "s2/s2lax_polyline_shape.h"
#include "s2/s2loop.h"
#include "s2/s2point_vector_shape.h"
#include "s2/s2pointutil.h"
#include "s2/s2shapeutil_coding.h"
#include "s2/s2shapeutil_testing.h"
#include "s2/s2testing.h"
#include "s2/s2text_format.h"
#include "s2/thread_testing.h"

using absl::make_unique;
using absl::StrCat;
using s2builderutil::S2CellIdSnapFunction;
using s2builderutil::S2PolylineLayer;
using std::max;
using std::string;
using std::unique_ptr;
using std::vector;

template <class Shape>
bool DecodeHomegeneousShapeIndex(EncodedS2ShapeIndex* index, Decoder* decoder) {
  return index->Init(decoder,
                     s2shapeutil::HomogeneousShapeFactory<Shape>(decoder));
}

template <class InShape, class OutShape>
void TestEncodedS2ShapeIndex(const MutableS2ShapeIndex& expected,
                             size_t expected_bytes) {
  Encoder encoder;
  s2shapeutil::EncodeHomogeneousShapes<InShape>(expected, &encoder);
  size_t shapes_bytes = encoder.length();
  expected.Encode(&encoder);
  EXPECT_EQ(expected_bytes, encoder.length() - shapes_bytes);
  Decoder decoder(encoder.base(), encoder.length());
  EncodedS2ShapeIndex actual;
  ASSERT_TRUE(DecodeHomegeneousShapeIndex<OutShape>(&actual, &decoder));
  EXPECT_EQ(expected.options().max_edges_per_cell(),
            actual.options().max_edges_per_cell());
  s2testing::ExpectEqual(expected, actual);
}

TEST(EncodedS2ShapeIndex, Empty) {
  MutableS2ShapeIndex index;
  TestEncodedS2ShapeIndex<S2LaxPolylineShape, S2LaxPolylineShape>(index, 4);
}

TEST(EncodedS2ShapeIndex, OneEdge) {
  MutableS2ShapeIndex index;
  index.Add(s2textformat::MakeLaxPolylineOrDie("1:1, 2:2"));
  TestEncodedS2ShapeIndex<S2LaxPolylineShape, S2LaxPolylineShape>(index, 8);
}

TEST(EncodedS2ShapeIndex, RegularLoops) {
  struct TestCase {
    int num_edges;
    size_t expected_bytes;
  };
  vector<TestCase> test_cases = {
    {4, 8},
    {8, 8},
    {16, 16},
    {64, 77},
    {256, 327},
    {4096, 8813},
    {65536, 168291},
  };
  for (const auto& test_case : test_cases) {
    MutableS2ShapeIndex index;
    S2Testing::rnd.Reset(test_case.num_edges);
    SCOPED_TRACE(StrCat("num_edges = ", test_case.num_edges));
    S2Polygon polygon(S2Loop::MakeRegularLoop(S2Point(3, 2, 1).Normalize(),
                                              S1Angle::Degrees(0.1),
                                              test_case.num_edges));
    index.Add(make_unique<S2LaxPolygonShape>(polygon));
    TestEncodedS2ShapeIndex<S2LaxPolygonShape, EncodedS2LaxPolygonShape>(
        index, test_case.expected_bytes);
  }
}

TEST(EncodedS2ShapeIndex, OverlappingPointClouds) {
  struct TestCase {
    int num_shapes, num_points_per_shape;
    size_t expected_bytes;
  };
  vector<TestCase> test_cases = {
    {1, 50, 83},
    {2, 100, 583},
    {4, 100, 1383},
  };
  S2Cap cap(S2Point(0.1, -0.4, 0.3).Normalize(), S1Angle::Degrees(1));
  for (const auto& test_case : test_cases) {
    MutableS2ShapeIndex index;
    S2Testing::rnd.Reset(test_case.num_shapes);
    SCOPED_TRACE(StrCat("num_shapes = ", test_case.num_shapes));
    for (int i = 0; i < test_case.num_shapes; ++i) {
      vector<S2Point> points;
      for (int j = 0; j < test_case.num_points_per_shape; ++j) {
        points.push_back(S2Testing::SamplePoint(cap));
      }
      index.Add(make_unique<S2PointVectorShape>(points));
    }
    TestEncodedS2ShapeIndex<S2PointVectorShape, EncodedS2PointVectorShape>(
        index, test_case.expected_bytes);
  }
}

TEST(EncodedS2ShapeIndex, OverlappingPolylines) {
  struct TestCase {
    int num_shapes, num_shape_edges;
    size_t expected_bytes;
  };
  vector<TestCase> test_cases = {
    {2, 50, 139},
    {10, 50, 777},
    {20, 50, 2219},
  };
  S2Cap cap(S2Point(-0.2, -0.3, 0.4).Normalize(), S1Angle::Degrees(0.1));
  for (const auto& test_case : test_cases) {
    S1Angle edge_len = 2 * cap.GetRadius() / test_case.num_shape_edges;
    MutableS2ShapeIndex index;
    S2Testing::rnd.Reset(test_case.num_shapes);
    SCOPED_TRACE(StrCat("num_shapes = ", test_case.num_shapes));
    for (int i = 0; i < test_case.num_shapes; ++i) {
      S2Point a = S2Testing::SamplePoint(cap), b = S2Testing::RandomPoint();
      vector<S2Point> vertices;
      int n = test_case.num_shape_edges;
      for (int j = 0; j <= n; ++j) {
        vertices.push_back(S2::GetPointOnLine(a, b, j * edge_len));
      }
      index.Add(make_unique<S2LaxPolylineShape>(vertices));
    }
    TestEncodedS2ShapeIndex<S2LaxPolylineShape, EncodedS2LaxPolylineShape>(
        index, test_case.expected_bytes);
  }
}

TEST(EncodedS2ShapeIndex, OverlappingLoops) {
  struct TestCase {
    int num_shapes, max_edges_per_loop;
    size_t expected_bytes;
  };
  vector<TestCase> test_cases = {
    {2, 250, 138},
    {5, 250, 1084},
    {25, 50, 3673},
  };
  S2Cap cap(S2Point(-0.1, 0.25, 0.2).Normalize(), S1Angle::Degrees(3));
  for (const auto& test_case : test_cases) {
    MutableS2ShapeIndex index;
    S2Testing::rnd.Reset(test_case.num_shapes);
    SCOPED_TRACE(StrCat("num_shapes = ", test_case.num_shapes));
    for (int i = 0; i < test_case.num_shapes; ++i) {
      S2Point center = S2Testing::SamplePoint(cap);
      double radius_fraction = S2Testing::rnd.RandDouble();
      // Scale the number of edges so that they are all about the same length
      // (similar to modeling all geometry at a similar resolution).
      int num_edges = max(3.0, test_case.max_edges_per_loop * radius_fraction);
      S2Polygon polygon(S2Loop::MakeRegularLoop(
          center, cap.GetRadius() * radius_fraction, num_edges));
      index.Add(make_unique<S2LaxPolygonShape>(polygon));
    }
    TestEncodedS2ShapeIndex<S2LaxPolygonShape, EncodedS2LaxPolygonShape>(
        index, test_case.expected_bytes);
  }
}

// Like S2PolylineLayer, but converts the polyline to an S2LaxPolylineShape
// and adds it to an S2ShapeIndex (if the polyline is non-empty).
class IndexedLaxPolylineLayer : public S2Builder::Layer {
 public:
  using Options = S2PolylineLayer::Options;
  explicit IndexedLaxPolylineLayer(MutableS2ShapeIndex* index,
                                   const Options& options = Options())
      : index_(index), polyline_(make_unique<S2Polyline>()),
        layer_(polyline_.get(), options) {}

  GraphOptions graph_options() const override {
    return layer_.graph_options();
  }

  void Build(const Graph& g, S2Error* error) override {
    layer_.Build(g, error);
    if (error->ok() && polyline_->num_vertices() > 0) {
      index_->Add(absl::make_unique<S2LaxPolylineShape>(*polyline_));
    }
  }

 private:
  MutableS2ShapeIndex* index_;
  std::unique_ptr<S2Polyline> polyline_;
  S2PolylineLayer layer_;
};

TEST(EncodedS2ShapeIndex, SnappedFractalPolylines) {
  MutableS2ShapeIndex index;
  S2Builder builder{S2Builder::Options{S2CellIdSnapFunction()}};
  for (int i = 0; i < 5; ++i) {
    builder.StartLayer(make_unique<IndexedLaxPolylineLayer>(&index));
    S2Testing::Fractal fractal;
    fractal.SetLevelForApproxMaxEdges(3 * 256);
    auto frame = S2::GetFrame(S2LatLng::FromDegrees(10, i).ToPoint());
    auto loop = fractal.MakeLoop(frame, S1Angle::Degrees(0.1));
    std::vector<S2Point> vertices;
    S2Testing::AppendLoopVertices(*loop, &vertices);
    S2Polyline polyline(vertices);
    builder.AddPolyline(polyline);
  }
  S2Error error;
  ASSERT_TRUE(builder.Build(&error)) << error.text();
  TestEncodedS2ShapeIndex<S2LaxPolylineShape, EncodedS2LaxPolylineShape>(
      index, 8698);
}

// A test that repeatedly minimizes "index_" in one thread and then reads the
// index_ concurrently from several other threads.  When all threads have
// finished reading, the first thread minimizes the index again.
//
// Note that Minimize() is non-const and therefore does not need to be tested
// concurrently with the const methods.
class LazyDecodeTest : public s2testing::ReaderWriterTest {
 public:
  LazyDecodeTest() {
    // We generate one shape per dimension.  Each shape has vertices uniformly
    // distributed across the sphere, and the vertices for each dimension are
    // different.  Having fewer cells in the index is more likely to trigger
    // race conditions, and so shape 0 has 384 points, shape 1 is a polyline
    // with 96 vertices, and shape 2 is a polygon with 24 vertices.
    MutableS2ShapeIndex input;
    for (int dim = 0; dim < 3; ++dim) {
      int level = 3 - dim;  // See comments above.
      vector<S2Point> vertices;
      for (auto id = S2CellId::Begin(level);
           id != S2CellId::End(level); id = id.next()) {
        vertices.push_back(id.ToPoint());
      }
      switch (dim) {
        case 0: input.Add(make_unique<S2PointVectorShape>(vertices)); break;
        case 1: input.Add(make_unique<S2LaxPolylineShape>(vertices)); break;
        default:
          input.Add(make_unique<S2LaxPolygonShape>(
              vector<vector<S2Point>>{std::move(vertices)}));
          break;
      }
    }
    Encoder encoder;
    s2shapeutil::CompactEncodeTaggedShapes(input, &encoder);
    input.Encode(&encoder);
    encoded_.assign(encoder.base(), encoder.length());

    Decoder decoder(encoded_.data(), encoded_.size());
    index_.Init(&decoder, s2shapeutil::LazyDecodeShapeFactory(&decoder));
  }

  void WriteOp() override {
    index_.Minimize();
  }

  void ReadOp() override {
    S2ClosestEdgeQuery query(&index_);
    for (int iter = 0; iter < 10; ++iter) {
      S2ClosestEdgeQuery::PointTarget target(S2Testing::RandomPoint());
      query.FindClosestEdge(&target);
    }
  }

 protected:
  string encoded_;
  EncodedS2ShapeIndex index_;
};

TEST(EncodedS2ShapeIndex, LazyDecode) {
  // Ensure that lazy decoding is thread-safe.  In other words, make sure that
  // nothing bad happens when multiple threads call "const" methods that cause
  // index and/or shape data to be decoded.
  LazyDecodeTest test;

  // The number of readers should be large enough so that it is likely that
  // several readers will be running at once (with a multiple-core CPU).
  const int kNumReaders = 8;
  const int kIters = 1000;
  test.Run(kNumReaders, kIters);
}

TEST(EncodedS2ShapeIndex, JavaByteCompatibility) {
  MutableS2ShapeIndex expected;
  expected.Add(make_unique<S2Polyline::OwningShape>(
      s2textformat::MakePolylineOrDie("0:0, 1:1")));
  expected.Add(make_unique<S2Polyline::OwningShape>(
      s2textformat::MakePolylineOrDie("1:1, 2:2")));
  expected.Release(0);

  // bytes is the encoded data of an S2ShapeIndex with a null shape and a
  // polyline with one edge. It was derived by base-16 encoding the buffer of
  // an encoder to which expected was encoded.
  string bytes = absl::HexStringToBytes(
      "100036020102000000B4825F3C81FDEF3F27DCF7C958DE913F1EDD892B0BDF913FFC7FB8"
      "B805F6EF3F28516A6D8FDBA13F27DCF7C958DEA13F28C809010408020010");
  Decoder decoder(bytes.data(), bytes.length());
  MutableS2ShapeIndex actual;
  actual.Init(&decoder, s2shapeutil::FullDecodeShapeFactory(&decoder));

  s2testing::ExpectEqual(expected, actual);
}

