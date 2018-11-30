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

#include <memory>
#include <set>
#include <vector>

#include <gtest/gtest.h>
#include "s2/third_party/absl/memory/memory.h"
#include "s2/mutable_s2shape_index.h"
#include "s2/s1angle.h"
#include "s2/s2cap.h"
#include "s2/s2cell.h"
#include "s2/s2cell_id.h"
#include "s2/s2closest_edge_query_testing.h"
#include "s2/s2coords.h"
#include "s2/s2edge_distances.h"
#include "s2/s2furthest_edge_query.h"
#include "s2/s2loop.h"
#include "s2/s2metrics.h"
#include "s2/s2point.h"
#include "s2/s2polygon.h"
#include "s2/s2predicates.h"
#include "s2/s2testing.h"
#include "s2/s2text_format.h"

using absl::make_unique;
using s2textformat::MakeIndexOrDie;
using s2textformat::MakePointOrDie;
using s2textformat::ParsePointsOrDie;
using std::make_pair;
using std::min;
using std::pair;
using std::unique_ptr;
using std::vector;

TEST(S2FurthestEdgeQuery, NoEdges) {
  MutableS2ShapeIndex index;
  S2FurthestEdgeQuery query(&index);
  S2FurthestEdgeQuery::PointTarget target(S2Point(1, 0, 0));
  const auto edge = query.FindFurthestEdge(&target);
  EXPECT_EQ(S1ChordAngle::Negative(), edge.distance());
  EXPECT_EQ(-1, edge.edge_id());
  EXPECT_EQ(-1, edge.shape_id());
  EXPECT_EQ(S1ChordAngle::Negative(), query.GetDistance(&target));
}

TEST(S2FurthestEdgeQuery, OptionsNotModified) {
  // Tests that FindFurthestEdge(), GetDistance(), and IsDistanceGreater() do
  // not modify query.options(), even though all of these methods have their
  // own specific options requirements.
  S2FurthestEdgeQuery::Options options;
  options.set_max_results(3);
  options.set_min_distance(S1ChordAngle::Degrees(1));
  options.set_max_error(S1ChordAngle::Degrees(0.001));
  auto index = MakeIndexOrDie("0:1 | 0:2 | 0:3 # #");
  S2FurthestEdgeQuery query(index.get(), options);
  S2FurthestEdgeQuery::PointTarget target(MakePointOrDie("0:4"));
  EXPECT_EQ(0, query.FindFurthestEdge(&target).edge_id());
  EXPECT_NEAR(3.0, query.GetDistance(&target).degrees(), 1e-15);
  EXPECT_TRUE(query.IsDistanceGreater(&target, S1ChordAngle::Degrees(1.5)));

  // Verify that none of the options above were modified.
  EXPECT_EQ(options.max_results(), query.options().max_results());
  EXPECT_EQ(options.min_distance(), query.options().min_distance());
  EXPECT_EQ(options.max_error(), query.options().max_error());
}

// In furthest edge queries, the following distance computation is used when
// updating max distances.
S1ChordAngle GetMaxDistanceToEdge(
    const S2Point& x, const S2Point& y0, const S2Point& y1) {
  S1ChordAngle dist = S1ChordAngle::Negative();
  S2::UpdateMaxDistance(x, y0, y1, &dist);
  return dist;
}

TEST(S2FurthestEdgeQuery, DistanceEqualToLimit) {
  // Tests the behavior of IsDistanceGreater, IsDistanceGreaterOrEqual, and
  // IsConservativeDistanceGreaterOrEqual (and the corresponding Options) when
  // the distance to the target exactly equals the chosen limit.
  S2Point p0(MakePointOrDie("23:12"));
  S2Point p1(MakePointOrDie("47:11"));
  vector<S2Point> index_points{p0};
  MutableS2ShapeIndex index;
  index.Add(make_unique<S2PointVectorShape>(index_points));
  S2FurthestEdgeQuery query(&index);

  // Start with antipodal points and a maximum (180 degrees) distance.
  S2FurthestEdgeQuery::PointTarget target0(-p0);
  S1ChordAngle dist_max = S1ChordAngle::Straight();
  EXPECT_FALSE(query.IsDistanceGreater(&target0, dist_max));
  EXPECT_TRUE(query.IsDistanceGreaterOrEqual(&target0, dist_max));
  EXPECT_TRUE(query.IsConservativeDistanceGreaterOrEqual(&target0, dist_max));

  // Now try two points separated by a non-maximal distance.
  S2FurthestEdgeQuery::PointTarget target1(-p1);
  S1ChordAngle dist1 = GetMaxDistanceToEdge(p0, -p1, -p1);
  EXPECT_FALSE(query.IsDistanceGreater(&target1, dist1));
  EXPECT_TRUE(query.IsDistanceGreaterOrEqual(&target1, dist1));
  EXPECT_TRUE(query.IsConservativeDistanceGreaterOrEqual(&target1, dist1));
}

TEST(S2FurthestEdgeQuery, TrueDistanceGreaterThanS1ChordAngleDistance) {
  // Tests that IsConservativeDistanceGreaterOrEqual returns points where the
  // true distance is slightly greater than the one computed by S1ChordAngle.
  //
  // The points below had the worst error from among 1x10^6 random pairs.
  S2Point p0(0.72362949088190598, -0.39019820403414807, -0.56930283812266336);
  S2Point p1(0.54383822931548842, 0.758981734255934404, 0.35803171284238039);

  // The S1ChordAngle distance is ~3 ulps greater than the true distance.
  S1ChordAngle dist1 = GetMaxDistanceToEdge(p0, p1, p1);
  auto limit = dist1.Successor().Successor().Successor();
  ASSERT_GT(s2pred::CompareDistance(p0, p1, limit), 0);

  // Verify that IsConservativeDistanceGreaterOrEqual() still returns "p1".
  vector<S2Point> index_points{p0};
  MutableS2ShapeIndex index;
  index.Add(make_unique<S2PointVectorShape>(index_points));
  S2FurthestEdgeQuery query(&index);
  S2FurthestEdgeQuery::PointTarget target1(p1);
  EXPECT_FALSE(query.IsDistanceGreater(&target1, limit));
  EXPECT_FALSE(query.IsDistanceGreaterOrEqual(&target1, limit));
  EXPECT_TRUE(query.IsConservativeDistanceGreaterOrEqual(&target1, limit));
}

TEST(S2FurthestEdgeQuery, AntipodalPointInsideIndexedPolygon) {
  // Tests a target point antipodal to the interior of an indexed polygon.
  // (The index also includes a polyline loop with no interior.)
  auto index = MakeIndexOrDie("# 0:0, 0:5, 5:5, 5:0 # 0:10, 0:15, 5:15, 5:10");
  S2FurthestEdgeQuery::Options options;

  // First check that with include_interiors set to true, the distance is 180.
  options.set_include_interiors(true);
  options.set_min_distance(S1Angle::Degrees(178));
  S2FurthestEdgeQuery query(index.get(), options);
  S2FurthestEdgeQuery::PointTarget target(-MakePointOrDie("2:12"));
  auto results = query.FindFurthestEdges(&target);
  ASSERT_GT(results.size(), 0);
  EXPECT_EQ(S1ChordAngle::Straight(), results[0].distance());
  // Should find the polygon shape (id = 1).
  EXPECT_EQ(1, results[0].shape_id());
  // Should find the interior, so no specific edge id.
  EXPECT_EQ(-1, results[0].edge_id());

  // Next check that with include_interiors set to false, the distance is less
  // than 180 for the same target and index.
  query.mutable_options()->set_include_interiors(false);
  results = query.FindFurthestEdges(&target);
  ASSERT_GT(results.size(), 0);
  EXPECT_LE(results[0].distance(), S1ChordAngle::Straight());
  EXPECT_EQ(1, results[0].shape_id());
  // Found a specific edge, so id should be positive.
  EXPECT_NE(-1, results[0].edge_id());
}

TEST(S2FurthestEdgeQuery, AntipodalPointOutsideIndexedPolygon) {
  // Tests a target point antipodal to the interior of a polyline loop with no
  // interior.  The index also includes a polygon almost antipodal to the
  // target, but with all edges closer than the min_distance threshold.
  auto index = MakeIndexOrDie("# 0:0, 0:5, 5:5, 5:0 # 0:10, 0:15, 5:15, 5:10");
  S2FurthestEdgeQuery::Options options;
  options.set_include_interiors(true);
  options.set_min_distance(S1Angle::Degrees(179));
  S2FurthestEdgeQuery query(index.get(), options);
  S2FurthestEdgeQuery::PointTarget target(-MakePointOrDie("2:2"));
  auto results = query.FindFurthestEdges(&target);
  EXPECT_EQ(0, results.size());
}

TEST(S2FurthestEdgeQuery, TargetPolygonContainingIndexedPoints) {
  // Two points are contained within a polyline loop (no interior) and two
  // points are contained within a polygon.
  auto index = MakeIndexOrDie("2:2 | 4:4 | 1:11 | 3:12 # #");
  S2FurthestEdgeQuery query(index.get());
  query.mutable_options()->set_use_brute_force(false);
  auto target_index = MakeIndexOrDie(
      "# 0:0, 0:5, 5:5, 5:0 # 0:10, 0:15, 5:15, 5:10");
  S2FurthestEdgeQuery::ShapeIndexTarget target(target_index.get());
  target.set_include_interiors(true);
  target.set_use_brute_force(true);
  auto results1 = query.FindFurthestEdges(&target);
  // All points should be returned since we did not specify max_results.
  ASSERT_EQ(4, results1.size());
  EXPECT_NE(S1ChordAngle::Zero(), results1[0].distance());
  EXPECT_EQ(0, results1[0].shape_id());
  EXPECT_EQ(0, results1[0].edge_id());  // 2:2 (to 5:15)
  EXPECT_NE(S1ChordAngle::Zero(), results1[1].distance());
  EXPECT_EQ(0, results1[1].shape_id());
  EXPECT_EQ(3, results1[1].edge_id());  // 3:12 (to 0:0)
}

TEST(S2FurthestEdgeQuery, AntipodalPolygonContainingIndexedPoints) {
  // Two antipodal points are contained within a polyline loop (no interior)
  // and two antipodal points are contained within a polygon.
  auto points = ParsePointsOrDie("2:2, 3:3, 1:11, 3:13");
  auto index = make_unique<MutableS2ShapeIndex>();
  vector<S2Point> antipodal_points;
  for (const auto& p : points) {
    antipodal_points.push_back(-p);
  }
  index->Add(make_unique<S2PointVectorShape>(std::move(antipodal_points)));

  S2FurthestEdgeQuery query(index.get());
  query.mutable_options()->set_min_distance(S1Angle::Degrees(179));
  auto target_index = MakeIndexOrDie(
      "# 0:0, 0:5, 5:5, 5:0 # 0:10, 0:15, 5:15, 5:10");
  S2FurthestEdgeQuery::ShapeIndexTarget target(target_index.get());
  target.set_include_interiors(true);
  auto results = query.FindFurthestEdges(&target);
  ASSERT_EQ(2, results.size());
  EXPECT_EQ(S1ChordAngle::Straight(), results[0].distance());
  EXPECT_EQ(0, results[0].shape_id());
  EXPECT_EQ(2, results[0].edge_id());  // 1:11
  EXPECT_EQ(S1ChordAngle::Straight(), results[1].distance());
  EXPECT_EQ(0, results[1].shape_id());
  EXPECT_EQ(3, results[1].edge_id());  // 3:13
}

TEST(S2FurthestEdgeQuery, EmptyPolygonTarget) {
  // Verifies that distances are measured correctly to empty polygon targets.
  auto empty_polygon_index = MakeIndexOrDie("# # empty");
  auto point_index = MakeIndexOrDie("1:1 # #");
  auto full_polygon_index = MakeIndexOrDie("# # full");
  S2FurthestEdgeQuery::ShapeIndexTarget target(empty_polygon_index.get());
  target.set_include_interiors(true);

  S2FurthestEdgeQuery empty_query(empty_polygon_index.get());
  empty_query.mutable_options()->set_include_interiors(true);
  EXPECT_EQ(S1ChordAngle::Negative(), empty_query.GetDistance(&target));

  S2FurthestEdgeQuery point_query(point_index.get());
  point_query.mutable_options()->set_include_interiors(true);
  EXPECT_EQ(S1ChordAngle::Negative(), point_query.GetDistance(&target));

  S2FurthestEdgeQuery full_query(full_polygon_index.get());
  full_query.mutable_options()->set_include_interiors(true);
  EXPECT_EQ(S1ChordAngle::Negative(), full_query.GetDistance(&target));
}

TEST(S2FurthestEdgeQuery, FullLaxPolygonTarget) {
  // Verifies that distances are measured correctly to full LaxPolygon targets.
  auto empty_polygon_index = MakeIndexOrDie("# # empty");
  auto point_index = MakeIndexOrDie("1:1 # #");
  auto full_polygon_index = MakeIndexOrDie("# # full");
  S2FurthestEdgeQuery::ShapeIndexTarget target(full_polygon_index.get());
  target.set_include_interiors(true);

  S2FurthestEdgeQuery empty_query(empty_polygon_index.get());
  empty_query.mutable_options()->set_include_interiors(true);
  EXPECT_EQ(S1ChordAngle::Negative(), empty_query.GetDistance(&target));

  S2FurthestEdgeQuery point_query(point_index.get());
  point_query.mutable_options()->set_include_interiors(true);
  EXPECT_EQ(S1ChordAngle::Straight(), point_query.GetDistance(&target));

  S2FurthestEdgeQuery full_query(full_polygon_index.get());
  full_query.mutable_options()->set_include_interiors(true);
  EXPECT_EQ(S1ChordAngle::Straight(), full_query.GetDistance(&target));
}

TEST(S2FurthestEdgeQuery, FullS2PolygonTarget) {
  // Verifies that distances are measured correctly to full S2Polygon targets
  // (which use a different representation of "full" than LaxPolygon does).
  auto empty_polygon_index = MakeIndexOrDie("# # empty");
  auto point_index = MakeIndexOrDie("1:1 # #");
  auto full_polygon_index = MakeIndexOrDie("# #");
  full_polygon_index->Add(make_unique<S2Polygon::OwningShape>(
      s2textformat::MakePolygonOrDie("full")));

  S2FurthestEdgeQuery::ShapeIndexTarget target(full_polygon_index.get());
  target.set_include_interiors(true);

  S2FurthestEdgeQuery empty_query(empty_polygon_index.get());
  empty_query.mutable_options()->set_include_interiors(true);
  EXPECT_EQ(S1ChordAngle::Negative(), empty_query.GetDistance(&target));

  S2FurthestEdgeQuery point_query(point_index.get());
  point_query.mutable_options()->set_include_interiors(true);
  EXPECT_EQ(S1ChordAngle::Straight(), point_query.GetDistance(&target));

  S2FurthestEdgeQuery full_query(full_polygon_index.get());
  full_query.mutable_options()->set_include_interiors(true);
  EXPECT_EQ(S1ChordAngle::Straight(), full_query.GetDistance(&target));
}

//////////////////////////////////////////////////////////////////////////////
//  General query testing by comparing with brute force method.
//////////////////////////////////////////////////////////////////////////////

// The approximate radius of S2Cap from which query edges are chosen.
static const S1Angle kTestCapRadius = S2Testing::KmToAngle(10);

using Result = pair<S2MaxDistance, s2shapeutil::ShapeEdgeId>;

// Converts to the format required by CheckDistanceResults() in s2testing.h
// TODO(user): When S2ClosestEdgeQuery::Result is made into a class, some
// of the following code may become redundant with that in
// s2closest_edge_query.cc.
vector<Result> ConvertResults(
    const vector<S2FurthestEdgeQuery::Result>& edges) {
  vector<Result> results;
  for (const auto& edge : edges) {
    results.push_back(
        make_pair(S2MaxDistance(edge.distance()),
                  s2shapeutil::ShapeEdgeId(edge.shape_id(), edge.edge_id())));
  }

  return results;
}

// Use "query" to find the furthest edge(s) to the given target.  Verify that
// the results satisfy the search criteria.
static void GetFurthestEdges(S2FurthestEdgeQuery::Target* target,
                            S2FurthestEdgeQuery *query,
                            vector<S2FurthestEdgeQuery::Result>* edges) {
  query->FindFurthestEdges(target, edges);
  EXPECT_LE(edges->size(), query->options().max_results());
  if (query->options().min_distance() == S1ChordAngle::Negative()) {
    int min_expected = min(query->options().max_results(),
                           s2shapeutil::CountEdges(query->index()));
    if (!query->options().include_interiors()) {
      // We can predict exactly how many edges should be returned.
      EXPECT_EQ(min_expected, edges->size());
    } else {
      // All edges should be returned, and possibly some shape interiors.
      EXPECT_LE(min_expected, edges->size());
    }
  }
  for (const auto& edge : *edges) {
    // Check that the edge satisfies the min_distance() condition.
    EXPECT_GE(edge.distance(), S1ChordAngle(query->options().min_distance()));
  }
}

static void TestFindFurthestEdges(
    S2FurthestEdgeQuery::Target* target, S2FurthestEdgeQuery *query) {
  vector<S2FurthestEdgeQuery::Result> expected, actual;
  query->mutable_options()->set_use_brute_force(true);
  GetFurthestEdges(target, query, &expected);
  query->mutable_options()->set_use_brute_force(false);
  GetFurthestEdges(target, query, &actual);

  S1ChordAngle min_distance = query->options().min_distance();
  S1ChordAngle max_error = query->options().max_error();
  EXPECT_TRUE(CheckDistanceResults(
      ConvertResults(expected),
      ConvertResults(actual),
      query->options().max_results(),
      S2MaxDistance(min_distance),
      max_error))
      << "max_results=" << query->options().max_results()
      << ", max_distance=" << S1ChordAngle(min_distance)
      << ", max_error=" << max_error;

  if (expected.empty()) {
    return;
  }

  // Note that when options.max_error() > 0, expected[0].distance may not be
  // the maximum distance.  It is never smaller by more than max_error(), but
  // the actual value also depends on max_results().
  //
  // Here we verify that GetDistance() and IsDistanceGreater() return results
  // that are consistent with the max_error() setting.
  S1ChordAngle expected_distance = expected[0].distance();
  EXPECT_GE(query->GetDistance(target), expected_distance - max_error);

  // Test IsDistanceGreater().
  EXPECT_FALSE(query->IsDistanceGreater(
      target, expected_distance + max_error));
  EXPECT_TRUE(query->IsDistanceGreater(
      target, expected[0].distance().Predecessor()));
}

// The running time of this test is proportional to
//    (num_indexes + num_queries) * num_edges.
// (Note that every query is checked using the brute force algorithm.)
static void TestWithIndexFactory(const s2testing::ShapeIndexFactory& factory,
                                 int num_indexes, int num_edges,
                                 int num_queries) {
  // Build a set of MutableS2ShapeIndexes containing the desired geometry.
  vector<S2Cap> index_caps;
  vector<unique_ptr<MutableS2ShapeIndex>> indexes;
  for (int i = 0; i < num_indexes; ++i) {
    S2Testing::rnd.Reset(FLAGS_s2_random_seed + i);
    index_caps.push_back(S2Cap(S2Testing::RandomPoint(), kTestCapRadius));
    indexes.emplace_back(new MutableS2ShapeIndex);
    factory.AddEdges(index_caps.back(), num_edges, indexes.back().get());
  }

  for (int i = 0; i < num_queries; ++i) {
    S2Testing::rnd.Reset(FLAGS_s2_random_seed + i);
    int i_index = S2Testing::rnd.Uniform(num_indexes);
    const S2Cap& index_cap = index_caps[i_index];

    // Choose query points from an area approximately 4x larger than the
    // geometry being tested.
    S1Angle query_radius = 2 * index_cap.GetRadius();
    S2FurthestEdgeQuery query(indexes[i_index].get());

    // Exercise the opposite-hemisphere code 1/5 of the time.
    int antipodal = S2Testing::rnd.OneIn(5) ? -1 : 1;
    S2Cap query_cap(antipodal * index_cap.center(), query_radius);

    // Occasionally we don't set any limit on the number of result edges.
    // (This may return all edges if we also don't set a distance limit.)
    if (!S2Testing::rnd.OneIn(5)) {
      query.mutable_options()->set_max_results(1 + S2Testing::rnd.Uniform(10));
    }
    // We set a distance limit 2/3 of the time.
    if (!S2Testing::rnd.OneIn(3)) {
      query.mutable_options()->set_min_distance(
          S2Testing::rnd.RandDouble() * query_radius);
    }
    if (S2Testing::rnd.OneIn(2)) {
      // Choose a maximum error whose logarithm is uniformly distributed over
      // a reasonable range, except that it is sometimes zero.
      query.mutable_options()->set_max_error(S1Angle::Radians(
          pow(1e-4, S2Testing::rnd.RandDouble()) * query_radius.radians()));
    }
    query.mutable_options()->set_include_interiors(S2Testing::rnd.OneIn(2));
    int target_type = S2Testing::rnd.Uniform(4);
    if (target_type == 0) {
      // Find the edges furthest from a given point.
      S2Point point = S2Testing::SamplePoint(query_cap);
      S2FurthestEdgeQuery::PointTarget target(point);
      TestFindFurthestEdges(&target, &query);
    } else if (target_type == 1) {
      // Find the edges furthest from a given edge.
      S2Point a = S2Testing::SamplePoint(query_cap);
      S2Point b = S2Testing::SamplePoint(
          S2Cap(a, pow(1e-4, S2Testing::rnd.RandDouble()) * query_radius));
      S2FurthestEdgeQuery::EdgeTarget target(a, b);
      TestFindFurthestEdges(&target, &query);
    } else if (target_type == 2) {
      // Find the edges furthest from a given cell.
      int min_level = S2::kMaxDiag.GetLevelForMaxValue(query_radius.radians());
      int level = min_level + S2Testing::rnd.Uniform(
          S2CellId::kMaxLevel - min_level + 1);
      S2Point a = S2Testing::SamplePoint(query_cap);
      S2Cell cell(S2CellId(a).parent(level));
      S2FurthestEdgeQuery::CellTarget target(cell);
      TestFindFurthestEdges(&target, &query);
    } else {
      S2_DCHECK_EQ(3, target_type);
      // Use another one of the pre-built indexes as the target.
      int j_index = S2Testing::rnd.Uniform(num_indexes);
      S2FurthestEdgeQuery::ShapeIndexTarget target(indexes[j_index].get());
      target.set_include_interiors(S2Testing::rnd.OneIn(2));
      TestFindFurthestEdges(&target, &query);
    }
  }
}

static const int kNumIndexes = 50;
static const int kNumEdges = 100;
static const int kNumQueries = 200;

TEST(S2FurthestEdgeQuery, CircleEdges) {
  TestWithIndexFactory(s2testing::RegularLoopShapeIndexFactory(),
                       kNumIndexes, kNumEdges, kNumQueries);
}

TEST(S2FurthestEdgeQuery, FractalEdges) {
  TestWithIndexFactory(s2testing::FractalLoopShapeIndexFactory(),
                       kNumIndexes, kNumEdges, kNumQueries);
}

TEST(S2FurthestEdgeQuery, PointCloudEdges) {
  TestWithIndexFactory(s2testing::PointCloudShapeIndexFactory(),
                       kNumIndexes, kNumEdges, kNumQueries);
}

