// Copyright 2020 Google Inc. All Rights Reserved.
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

#include "s2/s2buffer_operation.h"

#include <algorithm>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <utility>

#include "s2/base/casts.h"
#include "s2/base/logging.h"
#include <gtest/gtest.h>
#include "absl/base/call_once.h"
#include "absl/flags/flag.h"
#include "absl/memory/memory.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "s2/s1angle.h"
#include "s2/s2builderutil_lax_polygon_layer.h"
#include "s2/s2builderutil_snap_functions.h"
#include "s2/s2closest_edge_query.h"
#include "s2/s2contains_point_query.h"
#include "s2/s2error.h"
#include "s2/s2lax_loop_shape.h"
#include "s2/s2lax_polygon_shape.h"
#include "s2/s2lax_polyline_shape.h"
#include "s2/s2loop.h"
#include "s2/s2metrics.h"
#include "s2/s2point_vector_shape.h"
#include "s2/s2shape_measures.h"
#include "s2/s2testing.h"
#include "s2/s2text_format.h"

using absl::make_unique;
using std::max;
using std::string;
using std::unique_ptr;
using std::vector;

using EndCapStyle = S2BufferOperation::EndCapStyle;
using PolylineSide = S2BufferOperation::PolylineSide;

namespace {

// A callback that allows adding input to an S2BufferOperation.  This is
// convenient for testing the various input methods (AddPoint, AddShape, etc).
using InputCallback = std::function<void (S2BufferOperation* op)>;

// Convenience function that calls the given lambda expression to add input to
// an S2BufferOperation and returns the buffered result.
unique_ptr<S2LaxPolygonShape> DoBuffer(
    InputCallback input_callback, const S2BufferOperation::Options& options) {
  auto output = make_unique<S2LaxPolygonShape>();
  S2BufferOperation op(
      make_unique<s2builderutil::LaxPolygonLayer>(output.get()), options);
  input_callback(&op);
  S2Error error;
  EXPECT_TRUE(op.Build(&error)) << error;
  if (S2_VLOG_IS_ON(1) && output->num_vertices() < 1000) {
    std::cerr << "\nS2Polygon: " << s2textformat::ToString(*output) << "\n";
  }
  return output;
}

// Simpler version that accepts a buffer radius and error fraction.
unique_ptr<S2LaxPolygonShape> DoBuffer(
    InputCallback input_callback,
    S1Angle buffer_radius, double error_fraction) {
  S2BufferOperation::Options options;
  options.set_buffer_radius(buffer_radius);
  options.set_error_fraction(error_fraction);
  return DoBuffer(std::move(input_callback), options);
}

// Given a callback that adds empty geometry to the S2BufferOperation,
// verifies that the result is empty after buffering.
void TestBufferEmpty(InputCallback input) {
  // Test code paths that involve buffering by negative, zero, and positive
  // values, and also values where the result is usually empty or full.
  EXPECT_TRUE(DoBuffer(input, S1Angle::Degrees(-200), 0.1)->is_empty());
  EXPECT_TRUE(DoBuffer(input, S1Angle::Degrees(-1), 0.1)->is_empty());
  EXPECT_TRUE(DoBuffer(input, S1Angle::Degrees(0), 0.1)->is_empty());
  EXPECT_TRUE(DoBuffer(input, S1Angle::Degrees(1), 0.1)->is_empty());
  EXPECT_TRUE(DoBuffer(input, S1Angle::Degrees(200), 0.1)->is_empty());
}

TEST(S2BufferOperation, NoInput) {
  TestBufferEmpty([](S2BufferOperation* op) {});
}

TEST(S2BufferOperation, EmptyPolyline) {
  // Note that polylines with 1 vertex are defined to have no edges.
  TestBufferEmpty([](S2BufferOperation* op) {
      op->AddPolyline(vector<S2Point>{S2Point(1, 0, 0)});
    });
}

TEST(S2BufferOperation, EmptyLoop) {
  TestBufferEmpty([](S2BufferOperation* op) {
      op->AddLoop(vector<S2Point>{});
    });
}

TEST(S2BufferOperation, EmptyPointShape) {
  TestBufferEmpty([](S2BufferOperation* op) {
      op->AddShape(S2PointVectorShape{});
    });
}

TEST(S2BufferOperation, EmptyPolylineShape) {
  TestBufferEmpty([](S2BufferOperation* op) {
      op->AddShape(*s2textformat::MakeLaxPolylineOrDie(""));
    });
}

TEST(S2BufferOperation, EmptyPolygonShape) {
  TestBufferEmpty([](S2BufferOperation* op) {
      op->AddShape(*s2textformat::MakeLaxPolygonOrDie(""));
    });
}

TEST(S2BufferOperation, EmptyShapeIndex) {
  TestBufferEmpty([](S2BufferOperation* op) {
      op->AddShapeIndex(*s2textformat::MakeIndexOrDie("# #"));
    });
}

TEST(S2BufferOperation, Options) {
  // Provide test coverage for `options()`.
  S2BufferOperation::Options options(S1Angle::Radians(1e-12));
  S2LaxPolygonShape output;
  S2BufferOperation op(
      make_unique<s2builderutil::LaxPolygonLayer>(&output),
      options);
  EXPECT_EQ(options.buffer_radius(), op.options().buffer_radius());
}

TEST(S2BufferOperation, PoorlyNormalizedPoint) {
  // Verify that debugging assertions are not triggered when an input point is
  // not unit length (but within the limits guaranteed by S2Point::Normalize).
  //
  // The purpose of this test is not to check that the result is correct
  // (which is done elsewhere), simply that no assertions occur.
  DoBuffer([](S2BufferOperation* op) {
      S2Point p(1 - 2 * DBL_EPSILON, 0, 0);  // Maximum error allowed.
      S2_CHECK(S2::IsUnitLength(p));
      op->AddPoint(p);
    }, S1Angle::Degrees(1), 0.01);
}

// Given a callback that adds the full polygon to the S2BufferOperation,
// verifies that the result is full after buffering.
void TestBufferFull(InputCallback input) {
  // Test code paths that involve buffering by negative, zero, and positive
  // values, and also values where the result is usually empty or full.
  EXPECT_TRUE(DoBuffer(input, S1Angle::Degrees(-200), 0.1)->is_full());
  EXPECT_TRUE(DoBuffer(input, S1Angle::Degrees(-1), 0.1)->is_full());
  EXPECT_TRUE(DoBuffer(input, S1Angle::Degrees(0), 0.1)->is_full());
  EXPECT_TRUE(DoBuffer(input, S1Angle::Degrees(1), 0.1)->is_full());
  EXPECT_TRUE(DoBuffer(input, S1Angle::Degrees(200), 0.1)->is_full());
}

TEST(S2BufferOperation, FullPolygonShape) {
  TestBufferFull([](S2BufferOperation* op) {
      op->AddShape(*s2textformat::MakeLaxPolygonOrDie("full"));
    });
}

TEST(S2BufferOperation, FullShapeIndex) {
  TestBufferFull([](S2BufferOperation* op) {
      op->AddShapeIndex(*s2textformat::MakeIndexOrDie("# # full"));
    });
}

TEST(S2BufferOperation, PointsAndPolylinesAreRemoved) {
  // Test that points and polylines are removed with a negative buffer radius.
  auto output = DoBuffer([](S2BufferOperation* op) {
      op->AddShapeIndex(*s2textformat::MakeIndexOrDie("0:0 # 2:2, 2:3#"));
    }, S1Angle::Degrees(-1), 0.1);
  EXPECT_TRUE(output->is_empty());
}

TEST(S2BufferOperation, BufferedPointsAreSymmetric) {
  // Test that points are buffered into regular polygons.  (This is not
  // guaranteed by the API but makes the output nicer to look at. :)
  auto output = DoBuffer([](S2BufferOperation* op) {
      op->AddPoint(S2Point(1, 0, 0));
    }, S1Angle::Degrees(5), 0.001234567);

  // We use the length of the last edge as our reference length.
  int n = output->num_vertices();
  S1Angle edge_len(output->loop_vertex(0, 0), output->loop_vertex(0, n - 1));
  for (int i = 1; i < n; ++i) {
    EXPECT_LE(abs(edge_len - S1Angle(output->loop_vertex(0, i - 1),
                                     output->loop_vertex(0, i))),
              S1Angle::Radians(1e-14));
  }
}

TEST(S2BufferOperation, SetCircleSegments) {
  // Test that when a point is buffered with a small radius the number of
  // edges matches options.circle_segments().  (This is not true for large
  // radii because large circles on the sphere curve less than 360 degrees.)
  // Using a tiny radius helps to catch rounding problems.
  S2BufferOperation::Options options(S1Angle::Radians(1e-12));
  for (int circle_segments = 3; circle_segments <= 20; ++circle_segments) {
    options.set_circle_segments(circle_segments);
    EXPECT_FLOAT_EQ(circle_segments, options.circle_segments());
    auto output = DoBuffer([](S2BufferOperation* op) {
        op->AddPoint(S2Point(1, 0, 0));
      }, options);
    EXPECT_EQ(output->num_vertices(), circle_segments);
  }
}

TEST(S2BufferOperation, SetSnapFunction) {
  // Verify that the snap function is passed through to S2Builder.
  // We use a buffer radius of zero to make the test simpler.
  S2BufferOperation::Options options;
  options.set_snap_function(s2builderutil::IntLatLngSnapFunction(0));
  auto output = DoBuffer([](S2BufferOperation* op) {
      op->AddPoint(s2textformat::MakePointOrDie("0.1:-0.4"));
      }, options);
  EXPECT_EQ(output->num_vertices(), 1);
  EXPECT_EQ(output->loop_vertex(0, 0), s2textformat::MakePointOrDie("0:0"));
}

TEST(S2BufferOperation, NegativeBufferRadiusMultipleLayers) {
  // Verify that with a negative buffer radius, at most one polygon layer is
  // allowed.
  S2LaxPolygonShape output;
  S2BufferOperation op(
      make_unique<s2builderutil::LaxPolygonLayer>(&output),
      S2BufferOperation::Options(S1Angle::Radians(-1)));
  op.AddLoop(S2PointLoopSpan(s2textformat::ParsePointsOrDie("0:0, 0:1, 1:0")));
  op.AddShapeIndex(*s2textformat::MakeIndexOrDie("# # 2:2, 2:3, 3:2"));
  S2Error error;
  EXPECT_FALSE(op.Build(&error));
  EXPECT_EQ(error.code(), S2Error::FAILED_PRECONDITION);
}

// If buffer_radius > max_error, tests that "output" contains "input".
// If buffer_radius < -max_error tests that "input" contains "output".
// Otherwise does nothing.
void TestContainment(const MutableS2ShapeIndex& input,
                     const MutableS2ShapeIndex& output,
                     S1Angle buffer_radius, S1Angle max_error) {
  S2BooleanOperation::Options options;
  options.set_polygon_model(S2BooleanOperation::PolygonModel::CLOSED);
  options.set_polyline_model(S2BooleanOperation::PolylineModel::CLOSED);
  if (buffer_radius > max_error) {
    // For positive buffer radii, the output should contain the input.
    EXPECT_TRUE(S2BooleanOperation::Contains(output, input, options));
  } else if (buffer_radius < -max_error) {
    // For negative buffer radii, the input should contain the output.
    EXPECT_TRUE(S2BooleanOperation::Contains(input, output, options));
  }
}

// Tests that the minimum distance from the boundary of "output" to the
// boundary of "input" is at least "min_dist" using exact predicates.
void TestMinimumDistance(const MutableS2ShapeIndex& input,
                         const MutableS2ShapeIndex& output,
                         S1ChordAngle min_dist) {
  if (min_dist == S1ChordAngle::Zero()) return;

  // We do one query to find the edges of "input" that might be too close to
  // "output", then for each such edge we do another query to find the edges
  // of "output" that might be too close to it.  Then we check the distance
  // between each edge pair (A, B) using exact predicates.

  // We make the distance limit big enough to find all edges whose true
  // distance might be less than "min_dist".
  S2ClosestEdgeQuery::Options query_options;
  query_options.set_include_interiors(false);
  query_options.set_max_distance(
      min_dist.PlusError(S2::GetUpdateMinDistanceMaxError(min_dist)));

  S2ClosestEdgeQuery in_query(&input, query_options);
  S2ClosestEdgeQuery::ShapeIndexTarget out_target(&output);
  out_target.set_include_interiors(false);
  S2ClosestEdgeQuery out_query(&output, query_options);
  for (const auto& in_result : in_query.FindClosestEdges(&out_target)) {
    auto a = input.shape(in_result.shape_id())->edge(in_result.edge_id());
    S2ClosestEdgeQuery::EdgeTarget in_target(a.v0, a.v1);
    for (const auto& out_result : out_query.FindClosestEdges(&in_target)) {
      auto b = output.shape(out_result.shape_id())->edge(out_result.edge_id());
      ASSERT_GE(
          s2pred::CompareEdgePairDistance(a.v0, a.v1, b.v0, b.v1, min_dist), 0);
    }
  }
}

// Tests that the Hausdorff distance from the boundary of "output" to the
// boundary of "input" is at most (1 + error_fraction) * buffer_radius.  The
// implementation approximates this by measuring the distance at a set of
// points along the boundary of "output".
void TestHausdorffDistance(const MutableS2ShapeIndex& input,
                           const MutableS2ShapeIndex& output,
                           S1ChordAngle max_dist) {
  S2ClosestEdgeQuery::Options query_options;
  query_options.set_include_interiors(false);
  query_options.set_max_distance(
      max_dist.PlusError(S2::GetUpdateMinDistanceMaxError(max_dist)));

  S2ClosestEdgeQuery in_query(&input, query_options);
  for (const S2Shape* out_shape : output) {
    for (int i = 0; i < out_shape->num_edges(); ++i) {
      S2Shape::Edge e = out_shape->edge(i);
      // Measure the distance at 5 points along the edge.
      for (double t = 0; t <= 1.0; t += 0.25) {
        S2Point b = S2::Interpolate(e.v0, e.v1, t);
        S2ClosestEdgeQuery::PointTarget out_target(b);
        // We check the distance bound using exact predicates.
        for (const auto& in_result : in_query.FindClosestEdges(&out_target)) {
          auto a = input.shape(in_result.shape_id())->edge(in_result.edge_id());
          ASSERT_LE(s2pred::CompareEdgeDistance(b, a.v0, a.v1, max_dist), 0);
        }
      }
    }
  }
}

// Buffers the given input with the given buffer_radius and error_fraction and
// verifies that the output is correct.
void TestBuffer(const MutableS2ShapeIndex& input, S1Angle buffer_radius,
                double error_fraction) {
  // Ideally we would verify the correctness of buffering as follows.  Suppose
  // that B = Buffer(A, r) and let ~X denote the complement of region X.  Then
  // if r > 0, we would verify:
  //
  //   1a. Minimum distance between ~B and A >= r_min
  //   1b. Directed Hausdorff distance from B to A <= r_max
  //
  // Buffering A by r < 0 is equivalent to buffering ~A by |r|, so instead we
  // would verify the following (where r_min and r_max are based on |r|):
  //
  //   2a. Minimum distance between B and ~A >= r_min
  //   2b. Directed Hausdorff distance from ~B to ~A <= r_max
  //
  // Conditions 1a and 2a can be implemented as follows:
  //
  //   1a*: B.Contains(A) && minimum distance between @B and @A >= r_min
  //   2a*: A.Contains(B) && minimum distance between @B and @A >= r_min
  //
  // Note that if r_min <= 0 then there is nothing to be tested, since the
  // containment condition may not hold.  (Note that even when the specified
  // buffer radius is zero, edges can move slightly when crossing edges are
  // split during the snapping step.)  The correct approach would be to test
  // instead that the directed Hausdorff distance from A to ~B is at most
  // -r_min, but Hausdorff distance is not yet implemented.
  //
  // Similarly, conditions 1b and 2b need to be approximated because Hausdorff
  // distance is not yet implemented.  We do this by measuring the distance at
  // a set of points on the boundary of B:
  //
  //   1b*: Minimum distance from P to @A <= r_max for a set of points P on @B
  //   2b*: Minimum distance from P to @A <= r_max for a set of points P on @B
  //
  // This is not perfect (e.g., it won't detect cases where an entire boundary
  // loop of B is missing, such as returning a disc in the place of an
  // annulus) but it is sufficient to detect many types of errors.
  S2BufferOperation::Options options;
  options.set_buffer_radius(buffer_radius);
  options.set_error_fraction(error_fraction);
  MutableS2ShapeIndex output;
  output.Add(DoBuffer(
      [&input](S2BufferOperation* op) { op->AddShapeIndex(input); }, options));

  SCOPED_TRACE(absl::StrFormat(
      "\nradius = %.17g, error_fraction = %.17g\ninput = %s\noutput = %s",
      buffer_radius.radians(), error_fraction,
      s2textformat::ToString(input), s2textformat::ToString(output)));

  // Check the 1a*/1b* condition above.
  S1Angle max_error = options.max_error();
  TestContainment(input, output, buffer_radius, max_error);

  S1ChordAngle min_dist(max(S1Angle::Zero(), abs(buffer_radius) - max_error));
  TestMinimumDistance(input, output, min_dist);

  // Check the 2a*/2b* condition (i.e., directed Hausdorff distance).
  S1ChordAngle max_dist(abs(buffer_radius) + max_error);
  TestHausdorffDistance(input, output, max_dist);
}

// Convenience function that takes an S2ShapeIndex in s2textformat format.
void TestBuffer(absl::string_view index_str, S1Angle buffer_radius,
                double error_fraction) {
  TestBuffer(*s2textformat::MakeIndexOrDie(index_str), buffer_radius,
             error_fraction);
}

// Convenience function that tests buffering using +/- the given radius.
void TestSignedBuffer(absl::string_view index_str, S1Angle buffer_radius,
                      double error_fraction) {
  TestBuffer(index_str, buffer_radius, error_fraction);
  TestBuffer(index_str, -buffer_radius, error_fraction);
}

TEST(S2BufferOperation, PointShell) {
  TestSignedBuffer("# # 0:0", S1Angle::Radians(M_PI_2), 0.01);
}

TEST(S2BufferOperation, SiblingPairShell) {
  TestSignedBuffer("# # 0:0, 0:5", S1Angle::Radians(M_PI_2), 0.01);
}

TEST(S2BufferOperation, SiblingPairHole) {
  TestSignedBuffer("# # 0:0, 0:10, 7:7; 3:4, 3:6", S1Angle::Degrees(1), 0.01);
}

TEST(S2BufferOperation, Square) {
  TestSignedBuffer("# # -3:-3, -3:3, 3:3, 3:-3", S1Angle::Degrees(1), 0.01);
  TestSignedBuffer("# # -3:-3, -3:3, 3:3, 3:-3", S1Angle::Degrees(170), 1e-4);
}

TEST(S2BufferOperation, HollowSquare) {
  TestSignedBuffer("# # -3:-3, -3:3, 3:3, 3:-3; 2:2, -2:2, -2:-2, 2:-2",
                   S1Angle::Degrees(1), 0.01);
}

TEST(S2BufferOperation, ZigZagLoop) {
  TestSignedBuffer("# # 0:0, 0:7, 5:3, 5:10, 6:10, 6:1, 1:5, 1:0",
                   S1Angle::Degrees(0.2), 0.01);
}

TEST(S2BufferOperation, Fractals) {
  for (double dimension : {1.02, 1.8}) {
    S2Testing::Fractal fractal;
    fractal.SetLevelForApproxMaxEdges(3 * 64);
    fractal.set_fractal_dimension(dimension);
    auto loop = fractal.MakeLoop(S2::GetFrame(S2Point(1, 0, 0)),
                                 S1Angle::Degrees(10));
    MutableS2ShapeIndex input;
    input.Add(make_unique<S2Loop::Shape>(loop.get()));
    TestBuffer(input, S1Angle::Degrees(0.4), 0.01);
  }
}

TEST(S2BufferOperation, S2Curve) {
  // Tests buffering the S2 curve by an amount that yields the full polygon.
  constexpr int kLevel = 2;  // Number of input edges == 6 * (4 ** kLevel)
  vector<S2Point> points;
  for (S2CellId id = S2CellId::Begin(kLevel);
       id != S2CellId::End(kLevel); id = id.next()) {
    points.push_back(id.ToPoint());
  }
  // Buffering by this amount or more is guaranteed to yield the full polygon.
  // (Note that the bound is not tight for S2CellIds at low levels.)
  S1Angle full_radius = S1Angle::Radians(0.5 * S2::kMaxDiag.GetValue(kLevel));
  EXPECT_TRUE(DoBuffer([&points](S2BufferOperation* op) {
      op->AddShape(S2LaxClosedPolylineShape(points));
    }, full_radius, 0.1)->is_full());
}

// Tests buffering the given S2ShapeIndex with a variety of radii and error
// fractions.  This method is intended to be used with relatively simple
// shapes since calling it is quite expensive.
void TestRadiiAndErrorFractions(absl::string_view index_str) {
  // Try the full range of radii with a representative error fraction.
  constexpr double kFrac = 0.01;
  vector<double> kTestRadiiRadians = {
    0, 1e-300, 1e-15, 2e-15, 3e-15, 1e-5, 0.01, 0.1, 1.0,
    (1 - kFrac) * M_PI_2, M_PI_2 - 1e-15, M_PI_2, M_PI_2 + 1e-15,
    (1 - kFrac) * M_PI, M_PI - 1e-6, M_PI, 1e300
  };
  for (double radius : kTestRadiiRadians) {
    TestSignedBuffer(index_str, S1Angle::Radians(radius), kFrac);
  }

  // Now try the full range of error fractions with a few selected radii.
  vector<double> kTestErrorFractions =
      {S2BufferOperation::Options::kMinErrorFraction, 0.001, 0.01, 0.1, 1.0};
  for (double error_fraction : kTestErrorFractions) {
    TestBuffer(index_str, S1Angle::Radians(-1e-6), error_fraction);
    TestBuffer(index_str, S1Angle::Radians(1e-14), error_fraction);
    TestBuffer(index_str, S1Angle::Radians(1e-2), error_fraction);
    TestBuffer(index_str, S1Angle::Radians(M_PI - 1e-3), error_fraction);
  }
}

TEST(S2BufferOperation, RadiiAndErrorFractionCoverage) {
  // Test buffering simple shapes with a wide range of different buffer radii
  // and error fractions.

  // A single point.
  TestRadiiAndErrorFractions("1:1 # #");

  // A zig-zag polyline.
  TestRadiiAndErrorFractions("# 0:0, 0:30, 30:30, 30:60 #");

  // A triangular polygon with a triangular hole.  (The hole is clockwise.)
  TestRadiiAndErrorFractions("# # 0:0, 0:100, 70:50; 10:20, 50:50, 10:80");

  // A triangle with one very short and two very long edges.
  TestRadiiAndErrorFractions("# # 0:0, 0:179.99999999999, 1e-300:0");
}

class TestBufferPolyline {
 public:
  TestBufferPolyline(const string& input_str,
                     const S2BufferOperation::Options& options);

 private:
  const double kArcLo = 0.001;
  const double kArcHi = 0.999;
  const int kArcSamples = 7;

  S2Point GetEdgeAxis(const S2Point& a, const S2Point& b) {
    return S2::RobustCrossProd(a, b).Normalize();
  }

  bool PointBufferingUncertain(const S2Point& p, bool expect_contained) {
    // The only case where a point might be excluded from the buffered output is
    // if it is on the unbuffered side of the polyline.
    if (expect_contained && two_sided_) return false;

    int n = polyline_.size();
    for (int i = 0; i < n - 1; ++i) {
      const S2Point& a = polyline_[i];
      const S2Point& b = polyline_[i + 1];
      if (!two_sided_) {
        // Ignore points on the buffered side if expect_contained is true,
        // and on the unbuffered side if expect_contained is false.
        if ((s2pred::Sign(a, b, p) < 0) == expect_contained) continue;
      }
      // TODO(ericv): Depending on how the erasing optimization is implemented,
      // it might be possible to add "&& expect_contained" to the test below.
      if (round_) {
        if (S2::IsDistanceLess(p, a, b, max_dist_)) return true;
      } else {
        if (S2::IsInteriorDistanceLess(p, a, b, max_dist_)) return true;
        if (i > 0 && S1ChordAngle(p, a) < max_dist_) return true;
        if (i == n - 2 && S1ChordAngle(p, b) < max_dist_) return true;
      }
    }
    return false;
  }

  void TestPoint(const S2Point& p, const S2Point& dir, bool expect_contained) {
    S2Point x = S2::GetPointOnRay(
        p, dir, expect_contained ? buffer_radius_ - max_error_ : max_error_);
    if (!PointBufferingUncertain(x, expect_contained)) {
      EXPECT_EQ(MakeS2ContainsPointQuery(&output_).Contains(x),
                expect_contained);
    }
  }

  void TestVertexArc(const S2Point& p, const S2Point& start, const S2Point& end,
                     bool expect_contained) {
    for (double t = kArcLo; t < 1; t += (kArcHi - kArcLo) / kArcSamples) {
      S2Point dir = S2::Interpolate(start, end, t);
      TestPoint(p, dir, expect_contained);
    }
  }

  void TestEdgeArc(const S2Point& ba_axis, const S2Point& a, const S2Point& b,
                   bool expect_contained) {
    for (double t = kArcLo; t < 1; t += (kArcHi - kArcLo) / kArcSamples) {
      S2Point p = S2::Interpolate(a, b, t);
      TestPoint(p, ba_axis, expect_contained);
    }
  }

  void TestEdgeAndVertex(const S2Point& a, const S2Point& b, const S2Point& c,
                         bool expect_contained) {
    S2Point ba_axis = GetEdgeAxis(b, a);
    S2Point cb_axis = GetEdgeAxis(c, b);
    TestEdgeArc(ba_axis, a, b, expect_contained);
    TestVertexArc(b, ba_axis, cb_axis, expect_contained);
  }

  vector<S2Point> polyline_;
  MutableS2ShapeIndex output_;
  S1Angle buffer_radius_, max_error_;
  S1ChordAngle min_dist_, max_dist_;
  bool round_, two_sided_;
};

// Tests buffering a polyline with the given options.  This method is intended
// only for testing Options::EndCapStyle and Options::PolylineSide; if these
// options have their default values then TestBuffer() should be used
// instead.  Similarly TestBuffer should be used to test negative buffer radii
// and polylines with 0 or 1 vertices.
TestBufferPolyline::TestBufferPolyline(
    const string& input_str, const S2BufferOperation::Options& options)
    : polyline_(s2textformat::ParsePointsOrDie(input_str)),
      buffer_radius_(options.buffer_radius()),
      max_error_(options.max_error()),
      min_dist_(max(S1Angle::Zero(), buffer_radius_ - max_error_)),
      max_dist_(buffer_radius_ + max_error_),
      round_(options.end_cap_style() == EndCapStyle::ROUND),
      two_sided_(options.polyline_side() == PolylineSide::BOTH) {
  S2_DCHECK_GE(polyline_.size(), 2);
  S2_DCHECK(buffer_radius_ > S1Angle::Zero());

  MutableS2ShapeIndex input;
  input.Add(s2textformat::MakeLaxPolylineOrDie(input_str));
  output_.Add(DoBuffer(
      [&input](S2BufferOperation* op) { op->AddShapeIndex(input); }, options));

  // Even with one-sided buffering and flat end caps the Hausdorff distance
  // criterion should still be true.  (This checks that the buffered result
  // is never further than (buffer_radius + max_error) from the input.)
  TestHausdorffDistance(input, output_, max_dist_);

  // However the minimum distance criterion is different; it only applies to
  // the portions of the boundary that are buffered using the given radius.
  // We check this approximately by walking along the polyline and checking
  // that (1) on portions of the polyline that should be buffered, the output
  // contains the offset point at distance (buffer_radius - max_error) and (2)
  // on portions of the polyline that should not be buffered, the output does
  // not contain the offset point at distance max_error.  The tricky part is
  // that both of these conditions have exceptions: (1) may not hold if the
  // test point is closest to the non-buffered side of the polyline (see the
  // last caveat in the documentation for Options::polyline_side), and (2)
  // may not hold if the test point is within (buffer_radius + max_error) of
  // the buffered side of any portion of the polyline.
  if (min_dist_ == S1ChordAngle::Zero()) return;

  // Left-sided buffering is tested by reversing the polyline and then testing
  // whether it has been buffered correctly on the right.
  if (options.polyline_side() == PolylineSide::LEFT) {
    std::reverse(polyline_.begin(), polyline_.end());
  }

  int n = polyline_.size();
  S2Point start0 = polyline_[0], start1 = polyline_[1];
  S2Point start_begin = GetEdgeAxis(start0, start1);
  S2Point start_mid = start0.CrossProd(start_begin);
  TestVertexArc(start0, start_begin, start_mid, round_ && two_sided_);
  TestVertexArc(start0, start_mid, -start_begin, round_);
  for (int i = 0; i < n - 2; ++i) {
    TestEdgeAndVertex(polyline_[i], polyline_[i + 1], polyline_[i + 2], true);
  }
  S2Point end0 = polyline_[n - 1], end1 = polyline_[n - 2];
  S2Point end_begin = GetEdgeAxis(end0, end1);
  S2Point end_mid = end0.CrossProd(end_begin);
  TestEdgeArc(end_begin, end1, end0, true);
  TestVertexArc(end0, end_begin, end_mid, round_);
  TestVertexArc(end0, end_mid, -end_begin, round_ && two_sided_);
  for (int i = n - 3; i >= 0; --i) {
    TestEdgeAndVertex(polyline_[i + 2], polyline_[i + 1], polyline_[i],
                      two_sided_);
  }
  TestEdgeArc(start_begin, start1, start0, two_sided_);
}

TEST(S2BufferOperation, ZigZagPolyline) {
  S2BufferOperation::Options options(S1Angle::Degrees(1));
  for (PolylineSide polyline_side : {
      PolylineSide::LEFT, PolylineSide::RIGHT, PolylineSide::BOTH}) {
    for (EndCapStyle end_cap_style : {
        EndCapStyle::ROUND, EndCapStyle::FLAT}) {
      SCOPED_TRACE(absl::StrFormat(
          "two_sided = %d, round = %d",
          polyline_side == PolylineSide::BOTH,
          end_cap_style == EndCapStyle::ROUND));
      options.set_polyline_side(polyline_side);
      options.set_end_cap_style(end_cap_style);
      TestBufferPolyline("0:0, 0:7, 5:3, 5:10", options);  // NOLINT
      TestBufferPolyline("10:0, 0:0, 5:1", options);       // NOLINT
    }
  }
}

}  // namespace
