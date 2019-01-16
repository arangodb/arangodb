// Copyright 2005 Google Inc. All Rights Reserved.
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

#include "s2/s2polyline.h"

#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include "s2/base/commandlineflags.h"
#include <gtest/gtest.h>

#include "s2/s1angle.h"
#include "s2/s2cell.h"
#include "s2/s2debug.h"
#include "s2/s2latlng.h"
#include "s2/s2pointutil.h"
#include "s2/s2testing.h"
#include "s2/s2text_format.h"
#include "s2/third_party/absl/memory/memory.h"
#include "s2/third_party/absl/strings/str_cat.h"
#include "s2/util/coding/coder.h"

using absl::StrCat;
using absl::make_unique;
using std::fabs;
using std::unique_ptr;
using std::vector;

namespace {

// Wraps s2textformat::MakePolyline in order to test Encode/Decode.
unique_ptr<S2Polyline> MakePolyline(const string& str,
                                    S2Debug debug_override = S2Debug::ALLOW) {
  unique_ptr<S2Polyline> polyline =
      s2textformat::MakePolyline(str, debug_override);
  Encoder encoder;
  polyline->Encode(&encoder);
  Decoder decoder(encoder.base(), encoder.length());
  auto decoded_polyline = make_unique<S2Polyline>();
  decoded_polyline->set_s2debug_override(debug_override);
  decoded_polyline->Decode(&decoder);
  return decoded_polyline;
}

TEST(S2Polyline, Basic) {
  vector<S2Point> vertices;
  S2Polyline empty(vertices);
  EXPECT_EQ(S2LatLngRect::Empty(), empty.GetRectBound());
  empty.Reverse();
  EXPECT_EQ(0, empty.num_vertices());

  vector<S2LatLng> latlngs = {S2LatLng::FromDegrees(0, 0),
                              S2LatLng::FromDegrees(0, 90),
                              S2LatLng::FromDegrees(0, 180)};
  S2Polyline semi_equator(latlngs);
  EXPECT_TRUE(S2::ApproxEquals(semi_equator.Interpolate(0.5),
                               S2Point(0, 1, 0)));
  semi_equator.Reverse();
  EXPECT_EQ(S2Point(1, 0, 0), semi_equator.vertex(2));
}

TEST(S2Polyline, MoveConstruct) {
  unique_ptr<S2Polyline> line = MakePolyline("1:1, 4:4");
  S2Polyline moved(std::move(*line));
  ASSERT_EQ(0, line->num_vertices());
  ASSERT_EQ(2, moved.num_vertices());
}

TEST(S2Polyline, MoveAssign) {
  unique_ptr<S2Polyline> line = MakePolyline("1:1, 4:4");
  S2Polyline copied;
  copied = std::move(*line);
  ASSERT_EQ(0, line->num_vertices());
  ASSERT_EQ(2, copied.num_vertices());
}

TEST(S2Polyline, GetLengthAndCentroid) {
  // Construct random great circles and divide them randomly into segments.
  // Then make sure that the length and centroid are correct.  Note that
  // because of the way the centroid is computed, it does not matter how
  // we split the great circle into segments.

  for (int i = 0; i < 100; ++i) {
    // Choose a coordinate frame for the great circle.
    Vector3_d x, y, z;
    S2Testing::GetRandomFrame(&x, &y, &z);

    vector<S2Point> vertices;
    for (double theta = 0; theta < 2 * M_PI;
         theta += pow(S2Testing::rnd.RandDouble(), 10)) {
      S2Point p = cos(theta) * x + sin(theta) * y;
      if (vertices.empty() || p != vertices.back())
        vertices.push_back(p);
    }
    // Close the circle.
    vertices.push_back(vertices[0]);
    S2Polyline line(vertices);
    S1Angle length = line.GetLength();
    EXPECT_LE(fabs(length.radians() - 2 * M_PI), 2e-14);
    S2Point centroid = line.GetCentroid();
    EXPECT_LE(centroid.Norm(), 2e-14);
  }
}

TEST(S2Polyline, MayIntersect) {
  vector<S2Point> vertices = {S2Point(1, -1.1, 0.8).Normalize(),
                              S2Point(1, -0.8, 1.1).Normalize()};
  S2Polyline line(vertices);
  for (int face = 0; face < 6; ++face) {
    S2Cell cell = S2Cell::FromFace(face);
    EXPECT_EQ((face & 1) == 0, line.MayIntersect(cell));
  }
}

TEST(S2Polyline, Interpolate) {
  vector<S2Point> vertices = {S2Point(1, 0, 0),
                              S2Point(0, 1, 0),
                              S2Point(0, 1, 1).Normalize(),
                              S2Point(0, 0, 1)};
  S2Polyline line(vertices);
  EXPECT_EQ(vertices[0], line.Interpolate(-0.1));
  EXPECT_TRUE(S2::ApproxEquals(line.Interpolate(0.1),
                               S2Point(1, tan(0.2 * M_PI / 2), 0).Normalize()));
  EXPECT_TRUE(S2::ApproxEquals(line.Interpolate(0.25),
                               S2Point(1, 1, 0).Normalize()));
  EXPECT_EQ(vertices[1], line.Interpolate(0.5));
  EXPECT_TRUE(S2::ApproxEquals(vertices[2], line.Interpolate(0.75)));
  int next_vertex;
  EXPECT_EQ(vertices[0], line.GetSuffix(-0.1, &next_vertex));
  EXPECT_EQ(1, next_vertex);
  EXPECT_TRUE(S2::ApproxEquals(vertices[2],
                               line.GetSuffix(0.75, &next_vertex)));
  EXPECT_EQ(3, next_vertex);
  EXPECT_EQ(vertices[3], line.GetSuffix(1.1, &next_vertex));
  EXPECT_EQ(4, next_vertex);

  // Check the case where the interpolation fraction is so close to 1 that
  // the interpolated point is identical to the last vertex.
  vertices = {S2Point(1, 1, 1).Normalize(),
              S2Point(1, 1, 1 + 1e-15).Normalize(),
              S2Point(1, 1, 1 + 2e-15).Normalize()};
  S2Polyline short_line(vertices);
  EXPECT_EQ(vertices[2], short_line.GetSuffix(1.0 - 2e-16, &next_vertex));
  EXPECT_EQ(3, next_vertex);
}

TEST(S2Polyline, UnInterpolate) {
  vector<S2Point> vertices = {S2Point(1, 0, 0)};
  S2Polyline point_line(vertices);
  EXPECT_DOUBLE_EQ(0.0, point_line.UnInterpolate(S2Point(0, 1, 0), 1));

  vertices.push_back(S2Point(0, 1, 0));
  vertices.push_back(S2Point(0, 1, 1).Normalize());
  vertices.push_back(S2Point(0, 0, 1));
  S2Polyline line(vertices);

  S2Point interpolated;
  int next_vertex;
  interpolated = line.GetSuffix(-0.1, &next_vertex);
  EXPECT_DOUBLE_EQ(0.0, line.UnInterpolate(interpolated, next_vertex));
  interpolated = line.GetSuffix(0.0, &next_vertex);
  EXPECT_DOUBLE_EQ(0.0, line.UnInterpolate(interpolated, next_vertex));
  interpolated = line.GetSuffix(0.5, &next_vertex);
  EXPECT_DOUBLE_EQ(0.5, line.UnInterpolate(interpolated, next_vertex));
  interpolated = line.GetSuffix(0.75, &next_vertex);
  EXPECT_DOUBLE_EQ(0.75, line.UnInterpolate(interpolated, next_vertex));
  interpolated = line.GetSuffix(1.1, &next_vertex);
  EXPECT_DOUBLE_EQ(1.0, line.UnInterpolate(interpolated, next_vertex));

  // Check that the return value is clamped to 1.0.
  EXPECT_DOUBLE_EQ(1.0, line.UnInterpolate(S2Point(0, 1, 0), vertices.size()));
}

TEST(S2Polyline, Project) {
  vector<S2LatLng> latlngs = {
      S2LatLng::FromDegrees(0, 0), S2LatLng::FromDegrees(0, 1),
      S2LatLng::FromDegrees(0, 2), S2LatLng::FromDegrees(1, 2)};
  S2Polyline line(latlngs);

  int next_vertex;
  EXPECT_TRUE(S2::ApproxEquals(line.Project(
                                   S2LatLng::FromDegrees(0.5, -0.5).ToPoint(),
                                   &next_vertex),
                               S2LatLng::FromDegrees(0, 0).ToPoint()));
  EXPECT_EQ(1, next_vertex);
  EXPECT_TRUE(S2::ApproxEquals(line.Project(
                                   S2LatLng::FromDegrees(0.5, 0.5).ToPoint(),
                                   &next_vertex),
                               S2LatLng::FromDegrees(0, 0.5).ToPoint()));
  EXPECT_EQ(1, next_vertex);
  EXPECT_TRUE(S2::ApproxEquals(line.Project(
                                   S2LatLng::FromDegrees(0.5, 1).ToPoint(),
                                   &next_vertex),
                               S2LatLng::FromDegrees(0, 1).ToPoint()));
  EXPECT_EQ(2, next_vertex);
  EXPECT_TRUE(S2::ApproxEquals(line.Project(
                                   S2LatLng::FromDegrees(-0.5, 2.5).ToPoint(),
                                   &next_vertex),
                               S2LatLng::FromDegrees(0, 2).ToPoint()));
  EXPECT_EQ(3, next_vertex);
  EXPECT_TRUE(S2::ApproxEquals(line.Project(
                                   S2LatLng::FromDegrees(2, 2).ToPoint(),
                                   &next_vertex),
                               S2LatLng::FromDegrees(1, 2).ToPoint()));
  EXPECT_EQ(4, next_vertex);
}

TEST(S2Polyline, IsOnRight) {
  vector<S2LatLng> latlngs = {
      S2LatLng::FromDegrees(0, 0), S2LatLng::FromDegrees(0, 1),
      S2LatLng::FromDegrees(0, 2), S2LatLng::FromDegrees(1, 2)};
  S2Polyline line(latlngs);

  EXPECT_TRUE(line.IsOnRight(S2LatLng::FromDegrees(-0.5, 0.5).ToPoint()));
  EXPECT_FALSE(line.IsOnRight(S2LatLng::FromDegrees(0.5, -0.5).ToPoint()));
  EXPECT_FALSE(line.IsOnRight(S2LatLng::FromDegrees(0.5, 0.5).ToPoint()));
  EXPECT_FALSE(line.IsOnRight(S2LatLng::FromDegrees(0.5, 1).ToPoint()));
  EXPECT_TRUE(line.IsOnRight(S2LatLng::FromDegrees(-0.5, 2.5).ToPoint()));
  EXPECT_TRUE(line.IsOnRight(S2LatLng::FromDegrees(1.5, 2.5).ToPoint()));

  // Explicitly test the case where the closest point is an interior vertex.
  latlngs = {S2LatLng::FromDegrees(0, 0), S2LatLng::FromDegrees(0, 1),
             S2LatLng::FromDegrees(-1, 0)};
  S2Polyline line2(latlngs);

  // The points are chosen such that they are on different sides of the two
  // edges that the interior vertex is on.
  EXPECT_FALSE(line2.IsOnRight(S2LatLng::FromDegrees(-0.5, 5).ToPoint()));
  EXPECT_FALSE(line2.IsOnRight(S2LatLng::FromDegrees(5.5, 5).ToPoint()));
}

TEST(S2Polyline, IntersectsEmptyPolyline) {
  unique_ptr<S2Polyline> line1(MakePolyline("1:1, 4:4"));
  S2Polyline empty_polyline;
  EXPECT_FALSE(empty_polyline.Intersects(line1.get()));
}

TEST(S2Polyline, IntersectsOnePointPolyline) {
  unique_ptr<S2Polyline> line1(MakePolyline("1:1, 4:4"));
  unique_ptr<S2Polyline> line2(MakePolyline("1:1"));
  EXPECT_FALSE(line1->Intersects(line2.get()));
}

TEST(S2Polyline, Intersects) {
  unique_ptr<S2Polyline> line1(MakePolyline("1:1, 4:4"));
  unique_ptr<S2Polyline> small_crossing(MakePolyline("1:2, 2:1"));
  unique_ptr<S2Polyline> small_noncrossing(MakePolyline("1:2, 2:3"));
  unique_ptr<S2Polyline> big_crossing(MakePolyline("1:2, 2:3, 4:3"));

  EXPECT_TRUE(line1->Intersects(small_crossing.get()));
  EXPECT_FALSE(line1->Intersects(small_noncrossing.get()));
  EXPECT_TRUE(line1->Intersects(big_crossing.get()));
}

TEST(S2Polyline, IntersectsAtVertex) {
  unique_ptr<S2Polyline> line1(MakePolyline("1:1, 4:4, 4:6"));
  unique_ptr<S2Polyline> line2(MakePolyline("1:1, 1:2"));
  unique_ptr<S2Polyline> line3(MakePolyline("5:1, 4:4, 2:2"));
  EXPECT_TRUE(line1->Intersects(line2.get()));
  EXPECT_TRUE(line1->Intersects(line3.get()));
}

TEST(S2Polyline, IntersectsVertexOnEdge)  {
  unique_ptr<S2Polyline> horizontal_left_to_right(MakePolyline("0:1, 0:3"));
  unique_ptr<S2Polyline> vertical_bottom_to_top(MakePolyline("-1:2, 0:2, 1:2"));
  unique_ptr<S2Polyline> horizontal_right_to_left(MakePolyline("0:3, 0:1"));
  unique_ptr<S2Polyline> vertical_top_to_bottom(MakePolyline("1:2, 0:2, -1:2"));
  EXPECT_TRUE(horizontal_left_to_right->Intersects(
      vertical_bottom_to_top.get()));
  EXPECT_TRUE(horizontal_left_to_right->Intersects(
      vertical_top_to_bottom.get()));
  EXPECT_TRUE(horizontal_right_to_left->Intersects(
      vertical_bottom_to_top.get()));
  EXPECT_TRUE(horizontal_right_to_left->Intersects(
      vertical_top_to_bottom.get()));
}

TEST(S2Polyline, SpaceUsedEmptyPolyline)  {
  unique_ptr<S2Polyline> line(MakePolyline(""));
  EXPECT_GT(line->SpaceUsed(), 0);
}

TEST(S2Polyline, SpaceUsedNonEmptyPolyline)  {
  unique_ptr<S2Polyline> line(MakePolyline("1:1, 4:4, 4:6"));
  EXPECT_GT(line->SpaceUsed(), 3 * sizeof(S2Point));
}

static string JoinInts(const vector<int>& ints) {
  string result;
  int n = ints.size();
  for (int i = 0; i + 1 < n; ++i) {
    absl::StrAppend(&result, ints[i], ",");
  }
  if (n > 0) {
    absl::StrAppend(&result, ints[n - 1]);
  }
  return result;
}

void CheckSubsample(const char* polyline_str, double tolerance_degrees,
                    const char* expected_str,
                    S2Debug debug_override = S2Debug::ALLOW) {
  SCOPED_TRACE(StrCat("\"", polyline_str, "\", tolerance ",
                      tolerance_degrees));
  unique_ptr<S2Polyline> polyline(MakePolyline(polyline_str, debug_override));
  vector<int> indices;
  polyline->SubsampleVertices(S1Angle::Degrees(tolerance_degrees), &indices);
  EXPECT_EQ(expected_str, JoinInts(indices));
}

TEST(S2Polyline, SubsampleVerticesTrivialInputs) {
  // No vertices.
  CheckSubsample("", 1.0, "");
  // One vertex.
  CheckSubsample("0:1", 1.0, "0");
  // Two vertices.
  CheckSubsample("10:10, 11:11", 5.0, "0,1");
  // Three points on a straight line.
  // In theory, zero tolerance should work, but in practice there are floating
  // point errors.
  CheckSubsample("-1:0, 0:0, 1:0", 1e-15, "0,2");
  // Zero tolerance on a non-straight line.
  CheckSubsample("-1:0, 0:0, 1:1", 0.0, "0,1,2");
  // Negative tolerance should return all vertices.
  CheckSubsample("-1:0, 0:0, 1:1", -1.0, "0,1,2");
  // Non-zero tolerance with a straight line.
  CheckSubsample("0:1, 0:2, 0:3, 0:4, 0:5", 1.0, "0,4");

  // And finally, verify that we still do something reasonable if the client
  // passes in an invalid polyline with two or more adjacent vertices.
  CheckSubsample("0:1, 0:1, 0:1, 0:2", 0.0, "0,3", S2Debug::DISABLE);
}

TEST(S2Polyline, SubsampleVerticesSimpleExample) {
  const char* poly_str("0:0, 0:1, -1:2, 0:3, 0:4, 1:4, 2:4.5, 3:4, 3.5:4, 4:4");
  CheckSubsample(poly_str, 3.0, "0,9");
  CheckSubsample(poly_str, 2.0, "0,6,9");
  CheckSubsample(poly_str, 0.9, "0,2,6,9");
  CheckSubsample(poly_str, 0.4, "0,1,2,3,4,6,9");
  CheckSubsample(poly_str, 0, "0,1,2,3,4,5,6,7,8,9");
}

TEST(S2Polyline, SubsampleVerticesGuarantees) {
  // Check that duplicate vertices are never generated.
  CheckSubsample("10:10, 12:12, 10:10", 5.0, "0");
  CheckSubsample("0:0, 1:1, 0:0, 0:120, 0:130", 5.0, "0,3,4");

  // Check that points are not collapsed if they would create a line segment
  // longer than 90 degrees, and also that the code handles original polyline
  // segments longer than 90 degrees.
  CheckSubsample("90:0, 50:180, 20:180, -20:180, -50:180, -90:0, 30:0, 90:0",
                 5.0, "0,2,4,5,6,7");

  // Check that the output polyline is parametrically equivalent and not just
  // geometrically equivalent, i.e. that backtracking is preserved.  The
  // algorithm achieves this by requiring that the points must be encountered
  // in increasing order of distance along each output segment, except for
  // points that are within "tolerance" of the first vertex of each segment.
  CheckSubsample("10:10, 10:20, 10:30, 10:15, 10:40", 5.0, "0,2,3,4");
  CheckSubsample("10:10, 10:20, 10:30, 10:10, 10:30, 10:40", 5.0, "0,2,3,5");
  CheckSubsample("10:10, 12:12, 9:9, 10:20, 10:30", 5.0, "0,4");
}


static bool TestEquals(const char* a_str,
                       const char* b_str,
                       S1Angle max_error) {
  unique_ptr<S2Polyline> a(MakePolyline(a_str));
  unique_ptr<S2Polyline> b(MakePolyline(b_str));
  return a->ApproxEquals(*b, max_error);
}

TEST(S2Polyline, ApproxEquals) {
  S1Angle degree = S1Angle::Degrees(1);

  // Close lines, differences within max_error.
  EXPECT_TRUE(TestEquals("0:0, 0:10, 5:5",
                         "0:0.1, -0.1:9.9, 5:5.2",
                         0.5 * degree));

  // Close lines, differences outside max_error.
  EXPECT_FALSE(TestEquals("0:0, 0:10, 5:5",
                          "0:0.1, -0.1:9.9, 5:5.2",
                          0.01 * degree));

  // Same line, but different number of vertices.
  EXPECT_FALSE(TestEquals("0:0, 0:10, 0:20", "0:0, 0:20", 0.1 * degree));

  // Same vertices, in different order.
  EXPECT_FALSE(TestEquals("0:0, 5:5, 0:10", "5:5, 0:10, 0:0", 0.1 * degree));
}

TEST(S2Polyline, EncodeDecode) {
  unique_ptr<S2Polyline> polyline(MakePolyline("0:0, 0:10, 10:20, 20:30"));
  Encoder encoder;
  polyline->Encode(&encoder);
  Decoder decoder(encoder.base(), encoder.length());
  S2Polyline decoded_polyline;
  EXPECT_TRUE(decoded_polyline.Decode(&decoder));
  EXPECT_TRUE(decoded_polyline.ApproxEquals(*polyline, S1Angle::Zero()));
}

TEST(S2PolylineShape, Basic) {
  unique_ptr<S2Polyline> polyline(MakePolyline("0:0, 1:0, 1:1, 2:1"));
  S2Polyline::Shape shape(polyline.get());
  EXPECT_EQ(polyline.get(), shape.polyline());
  EXPECT_EQ(3, shape.num_edges());
  EXPECT_EQ(1, shape.num_chains());
  EXPECT_EQ(0, shape.chain(0).start);
  EXPECT_EQ(3, shape.chain(0).length);
  auto edge2 = shape.edge(2);
  EXPECT_EQ(S2LatLng::FromDegrees(1, 1).ToPoint(), edge2.v0);
  EXPECT_EQ(S2LatLng::FromDegrees(2, 1).ToPoint(), edge2.v1);
  EXPECT_EQ(1, shape.dimension());
  EXPECT_FALSE(shape.is_empty());
  EXPECT_FALSE(shape.is_full());
  EXPECT_FALSE(shape.GetReferencePoint().contained);
}

TEST(S2PolylineShape, EmptyPolyline) {
  S2Polyline polyline;
  S2Polyline::Shape shape(&polyline);
  EXPECT_EQ(0, shape.num_edges());
  EXPECT_EQ(0, shape.num_chains());
  EXPECT_TRUE(shape.is_empty());
  EXPECT_FALSE(shape.is_full());
  EXPECT_FALSE(shape.GetReferencePoint().contained);
}

TEST(S2PolylineOwningShape, Ownership) {
  // Debug mode builds will catch any memory leak below.
  vector<S2Point> vertices;
  auto polyline = make_unique<S2Polyline>(vertices);
  S2Polyline::OwningShape shape(std::move(polyline));
}

void TestNearlyCovers(const string& a_str, const string& b_str,
                      double max_error_degrees, bool expect_b_covers_a,
                      bool expect_a_covers_b,
                      S2Debug debug_override = S2Debug::ALLOW) {
  SCOPED_TRACE(StrCat("a=\"", a_str, "\", b=\"", b_str, "\", max error=",
                      max_error_degrees));
  unique_ptr<S2Polyline> a(MakePolyline(a_str, debug_override));
  unique_ptr<S2Polyline> b(MakePolyline(b_str, debug_override));
  S1Angle max_error = S1Angle::Degrees(max_error_degrees);
  EXPECT_EQ(expect_b_covers_a, b->NearlyCovers(*a, max_error));
  EXPECT_EQ(expect_a_covers_b, a->NearlyCovers(*b, max_error));
}

TEST(S2PolylineCoveringTest, PolylineOverlapsSelf) {
  string pline = "1:1, 2:2, -1:10";
  TestNearlyCovers(pline, pline, 1e-10, true, true);
}

TEST(S2PolylineCoveringTest, PolylineDoesNotOverlapReverse) {
  TestNearlyCovers("1:1, 2:2, -1:10", "-1:10, 2:2, 1:1", 1e-10, false, false);
}

TEST(S2PolylineCoveringTest, PolylineOverlapsEquivalent) {
  // These two polylines trace the exact same polyline, but the second one uses
  // three points instead of two.
  TestNearlyCovers("1:1, 2:1", "1:1, 1.5:1, 2:1", 1e-10, true, true);
}

TEST(S2PolylineCoveringTest, ShortCoveredByLong) {
  // The second polyline is always within 0.001 degrees of the first polyline,
  // but the first polyline is too long to be covered by the second.
  TestNearlyCovers(
      "-5:1, 10:1, 10:5, 5:10", "9:1, 9.9995:1, 10.0005:5", 1e-3, false, true);
}

TEST(S2PolylineCoveringTest, PartialOverlapOnly) {
  // These two polylines partially overlap each other, but neither fully
  // overlaps the other.
  TestNearlyCovers("-5:1, 10:1", "0:1, 20:1", 1.0, false, false);
}

TEST(S2PolylineCoveringTest, ShortBacktracking) {
  // Two lines that backtrack a bit (less than 1.5 degrees) on different edges.
  // A simple greedy matching algorithm would fail on this example.
  const string& t1 = "0:0, 0:2, 0:1, 0:4, 0:5";
  const string& t2 = "0:0, 0:2, 0:4, 0:3, 0:5";
  TestNearlyCovers(t1, t2, 1.5, true, true);
  TestNearlyCovers(t1, t2, 0.5, false, false);
}

TEST(S2PolylineCoveringTest, LongBacktracking) {
  // Two arcs with opposite direction do not overlap if the shorter arc is
  // longer than max_error, but do if the shorter arc is shorter than max-error.
  TestNearlyCovers("5:1, -5:1", "1:1, 3:1", 1.0, false, false);
  TestNearlyCovers("5:1, -5:1", "1:1, 3:1", 2.5, false, true);
}

TEST(S2PolylineCoveringTest, IsResilientToDuplicatePoints) {
  // S2Polyines are not generally supposed to contain adjacent, identical
  // points, but it happens in practice.  We also set S2Debug::DISABLE so
  // debug binaries won't abort on such polylines.
  TestNearlyCovers("0:1, 0:2, 0:2, 0:3", "0:1, 0:1, 0:1, 0:3",
                   1e-10, true, true, S2Debug::DISABLE);
}

TEST(S2PolylineCoveringTest, CanChooseBetweenTwoPotentialStartingPoints) {
  // Can handle two possible starting points, only one of which leads to finding
  // a correct path.  In the first polyline, the edge from 0:1.1 to 0:0 and the
  // edge from 0:0.9 to 0:2 might be lucrative starting states for covering the
  // second polyline, because both edges are with the max_error of 1.5 degrees
  // from 0:10.  However, only the latter is actually effective.
  TestNearlyCovers("0:11, 0:0, 0:9, 0:20", "0:10, 0:15", 1.5, false, true);
}

TEST(S2PolylineCoveringTest, StraightAndWigglyPolylinesCoverEachOther) {
  TestNearlyCovers("40:1, 20:1",
                   "39.9:0.9, 40:1.1, 30:1.15, 29:0.95, 28:1.1, 27:1.15, "
                   "26:1.05, 25:0.85, 24:1.1, 23:0.9, 20:0.99",
                   0.2, true, true);
}

TEST(S2PolylineCoveringTest, MatchStartsAtLastVertex) {
  // The first polyline covers the second, but the matching segment starts at
  // the last vertex of the first polyline.
  TestNearlyCovers(
      "0:0, 0:2", "0:2, 0:3", 1.5, false, true);
}

TEST(S2PolylineCoveringTest, MatchStartsAtDuplicatedLastVertex) {
  TestNearlyCovers(
      "0:0, 0:2, 0:2, 0:2", "0:2, 0:3", 1.5, false, true, S2Debug::DISABLE);
}

TEST(S2PolylineCoveringTest, EmptyPolylines) {
  // We expect:
  //    anything.covers(empty) = true
  //    empty.covers(nonempty) = false
  TestNearlyCovers("0:1, 0:2", "", 0.0, false, true);
  TestNearlyCovers("", "", 0.0, true, true);
}

}  // namespace
