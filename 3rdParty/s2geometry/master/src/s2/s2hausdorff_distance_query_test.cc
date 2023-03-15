// Copyright Google Inc. All Rights Reserved.
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

#include "s2/s2hausdorff_distance_query.h"

#include <algorithm>
#include <utility>
#include <vector>

#include <gtest/gtest.h>
#include "absl/memory/memory.h"
#include "s2/mutable_s2shape_index.h"
#include "s2/s1angle.h"
#include "s2/s1chord_angle.h"
#include "s2/s2latlng.h"
#include "s2/s2lax_polygon_shape.h"
#include "s2/s2lax_polyline_shape.h"
#include "s2/s2point.h"
#include "s2/s2point_vector_shape.h"
#include "s2/s2predicates.h"
#include "s2/s2text_format.h"

using absl::make_unique;
using s2textformat::ParsePointsOrDie;
using DirectedResult = S2HausdorffDistanceQuery::DirectedResult;
using Result = S2HausdorffDistanceQuery::Result;
using Options = S2HausdorffDistanceQuery::Options;

// Test the constructors and accessors of Result and DirectedResult.
TEST(S2HausdorffDistanceQueryTest, ResultConstructorsAndAccessorsWork) {
  S2Point point1 = S2LatLng::FromDegrees(3, 4).ToPoint();
  S2Point point2 = S2LatLng::FromDegrees(5, 6).ToPoint();
  S1ChordAngle distance1 = S1ChordAngle::Degrees(5);
  S1ChordAngle distance2 = S1ChordAngle::Degrees(5);

  DirectedResult directed_result1(distance1, point1);
  DirectedResult directed_result2(distance2, point2);
  Result result12(directed_result1, directed_result2);

  EXPECT_EQ(directed_result1.target_point(), point1);
  EXPECT_EQ(directed_result1.distance(), distance1);
  EXPECT_EQ(directed_result2.target_point(), point2);
  EXPECT_EQ(directed_result2.distance(), distance2);

  EXPECT_EQ(result12.target_to_source().target_point(), point1);
  EXPECT_EQ(result12.source_to_target().target_point(), point2);
  EXPECT_EQ(result12.distance(), directed_result2.distance());
}

// Test the constructors and accessors of the Options.
TEST(S2HausdorffDistanceQueryTest, OptionsConstructorsAndAccessorsWork) {
  Options default_options;
  Options options;
  options.set_include_interiors(!default_options.include_interiors());

  EXPECT_TRUE(default_options.include_interiors());
  EXPECT_FALSE(options.include_interiors());
}

// Test the constructors and accessors of the Options.
TEST(S2HausdorffDistanceQueryTest, QueryOptionsAccessorsWorks) {
  S2HausdorffDistanceQuery query;
  const bool default_include_interiors = query.options().include_interiors();

  query.mutable_options()->set_include_interiors(!default_include_interiors);
  const bool modified_include_interiors = query.options().include_interiors();

  EXPECT_TRUE(default_include_interiors);
  EXPECT_FALSE(modified_include_interiors);
}

// Test involving 2 simple polyline shape indexes..
TEST(S2HausdorffDistanceQueryTest, SimplePolylineQueriesSucceed) {
  const std::vector<S2Point> a0 = ParsePointsOrDie("0:0, 0:1, 0:1.5");
  const std::vector<S2Point> a1 = ParsePointsOrDie("0:2, 0:1.5, -10:1");
  const std::vector<S2Point> b0 = ParsePointsOrDie("1:0, 1:1, 3:2");

  // Setup the shape indexes.
  const MutableS2ShapeIndex empty_index;

  // Shape index a consists or 2 polylines, a0 and a1.
  MutableS2ShapeIndex a;
  a.Add(make_unique<S2LaxPolylineShape>(a0));
  a.Add(make_unique<S2LaxPolylineShape>(a1));

  // Shape index b consists or 1 polylines: b0.
  MutableS2ShapeIndex b;
  b.Add(make_unique<S2LaxPolylineShape>(b0));

  // Calculate expected distances.
  // Directed a to b HD is achieved at the vertex 2 of a1 and vertex 1 of b0.
  const S1ChordAngle expected_a_to_b(a1[2], b0[1]);
  // Directed b to a HD is achieved at the vertex 2 of b0 and vertex 0 of a1.
  const S1ChordAngle expected_b_to_a(b0[2], a1[0]);

  Options options;
  S2HausdorffDistanceQuery query(options);

  absl::optional<DirectedResult> directed_empty_to_a =
      query.GetDirectedResult(&empty_index, &a);
  absl::optional<DirectedResult> directed_a_to_empty =
      query.GetDirectedResult(&a, &empty_index);
  S1ChordAngle directed_a_to_empty_distance =
      query.GetDirectedDistance(&a, &empty_index);

  absl::optional<DirectedResult> directed_a_to_b =
      query.GetDirectedResult(&a, &b);
  absl::optional<DirectedResult> directed_b_to_a =
      query.GetDirectedResult(&b, &a);
  S1ChordAngle directed_a_to_b_distance = query.GetDirectedDistance(&a, &b);

  absl::optional<Result> a_to_b = query.GetResult(&a, &b);
  absl::optional<Result> b_to_a = query.GetResult(&b, &a);
  S1ChordAngle b_to_a_distance = query.GetDistance(&b, &a);
  absl::optional<Result> bb = query.GetResult(&b, &b);

  EXPECT_FALSE(directed_empty_to_a);
  EXPECT_FALSE(directed_a_to_empty);
  EXPECT_TRUE(directed_a_to_empty_distance.is_infinity());

  EXPECT_TRUE(directed_a_to_b);
  EXPECT_TRUE(directed_b_to_a);

  EXPECT_DOUBLE_EQ(directed_a_to_b->distance().degrees(),
                   expected_a_to_b.degrees());
  EXPECT_DOUBLE_EQ(directed_a_to_b_distance.degrees(),
                   expected_a_to_b.degrees());
  EXPECT_DOUBLE_EQ(directed_b_to_a->distance().degrees(),
                   expected_b_to_a.degrees());

  EXPECT_TRUE(a_to_b);
  EXPECT_TRUE(b_to_a);
  EXPECT_TRUE(bb);

  EXPECT_DOUBLE_EQ(a_to_b->distance().degrees(), b_to_a->distance().degrees());
  EXPECT_DOUBLE_EQ(bb->distance().degrees(), 0);
  EXPECT_EQ(
      a_to_b->distance().degrees(),
      std::max(a_to_b->distance().degrees(), b_to_a->distance().degrees()));
  EXPECT_EQ(b_to_a_distance.degrees(), b_to_a->distance().degrees());
}

// Test involving a polyline shape (dimension == 1) and a point shape (dimension
// == 0).
TEST(S2HausdorffDistanceQueryTest, PointVectorShapeQueriesSucceed) {
  // Points for the polyline shape.
  const std::vector<S2Point> a_points =
      ParsePointsOrDie("2:0, 0:1, 1:2, 0:3, 0:4");
  // Points for the polint vector shape.
  const std::vector<S2Point> b_points =
      ParsePointsOrDie("-1:2, -0.5:0.5, -0.5:3.5");

  MutableS2ShapeIndex a;
  a.Add(make_unique<S2LaxPolylineShape>(a_points));

  MutableS2ShapeIndex b;
  b.Add(make_unique<S2PointVectorShape>(b_points));

  Options options;
  S2HausdorffDistanceQuery query(options);

  // Directed Hausdorff distance from a to b is achieved at the vertex 0 of a
  // and vertex 1 of b.
  const S1ChordAngle expected_a_to_b(a_points[0], b_points[1]);

  // Directed Hausdorff distance from b to a is achieved at the vertex 0 of b
  // and vertex 3 of a.
  const S1ChordAngle expected_b_to_a(b_points[0], a_points[3]);

  // Unddrected Hausdorff distance between a and b is the maximum of the two
  // directed Hausdorff distances.
  const S1ChordAngle expected_a_b = std::max(expected_a_to_b, expected_b_to_a);

  absl::optional<DirectedResult> directed_a_to_b =
      query.GetDirectedResult(&a, &b);
  absl::optional<DirectedResult> directed_b_to_a =
      query.GetDirectedResult(&b, &a);
  S1ChordAngle undirected_a_b = query.GetDistance(&a, &b);

  EXPECT_TRUE(directed_a_to_b);
  EXPECT_TRUE(directed_b_to_a);
  EXPECT_FALSE(undirected_a_b.is_infinity());

  EXPECT_DOUBLE_EQ(directed_a_to_b->distance().degrees(),
                   expected_a_to_b.degrees());
  EXPECT_EQ(directed_a_to_b->target_point(), a_points[0]);

  EXPECT_DOUBLE_EQ(directed_b_to_a->distance().degrees(),
                   expected_b_to_a.degrees());
  EXPECT_EQ(directed_b_to_a->target_point(), b_points[0]);

  EXPECT_EQ(undirected_a_b.degrees(), expected_a_b.degrees());
}

// Test involving partially overlapping polygons.
TEST(S2HausdorffDistanceQueryTest, OverlappingPolygons) {
  // The first polygon is a triangle. It's first two vertices are inside the
  // quadrangle b (defined below), and the last vertex is outside of b.
  MutableS2ShapeIndex a;
  a.Add(s2textformat::MakeLaxPolygonOrDie("1:1, 1:2, 3.5:1.5"));

  // The other polygon is a quadraangle.
  MutableS2ShapeIndex b;
  b.Add(s2textformat::MakeLaxPolygonOrDie("0:0, 0:3, 3:3, 3:0"));

  // The first query does not include the interiors.
  Options options;
  options.set_include_interiors(false);
  S2HausdorffDistanceQuery query_1(options);
  absl::optional<DirectedResult> a_to_b_1 = query_1.GetDirectedResult(&a, &b);

  // The second query has include_interiors set to true.
  options.set_include_interiors(true);
  S2HausdorffDistanceQuery query_2(options);
  absl::optional<DirectedResult> a_to_b_2 = query_2.GetDirectedResult(&a, &b);

  // Error tolerance to account for the difference between the northern edge of
  // the quadrangle, which is a geodesic line, and the parallel lat=3 connecting
  // the verices of that edge.
  static constexpr double kEpsilon = 3.0e-3;

  // The directed Hausdorff distance from the first query is achieved on the
  // vertex of the triangle that is inside the quadrangle, and is approximately
  // 1 degree away from the nearest edge of the quadrangle.
  S2Point expected_target_point_1 = S2LatLng::FromDegrees(1, 2).ToPoint();

  // The directed Hausdorff distance from the second query is achieved on the
  // last vertex of the triangle that is outside the quadrangle, and is about
  // 0.5 degrees away from the nearest edge of the quadrangle.
  S2Point expected_target_point_2 = S2LatLng::FromDegrees(3.5, 1.5).ToPoint();

  EXPECT_TRUE(a_to_b_1);
  EXPECT_NEAR(a_to_b_1->distance().degrees(), 1, kEpsilon);
  EXPECT_EQ(a_to_b_1->target_point(), expected_target_point_1);

  EXPECT_TRUE(a_to_b_2);
  EXPECT_NEAR(a_to_b_2->distance().degrees(), 0.5, kEpsilon);
  EXPECT_EQ(a_to_b_2->target_point(), expected_target_point_2);
}
