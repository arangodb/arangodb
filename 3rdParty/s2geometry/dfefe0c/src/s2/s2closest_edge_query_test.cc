// Copyright 2013 Google Inc. All Rights Reserved.
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

#include "s2/s2closest_edge_query.h"

#include <memory>
#include <vector>

#include <gtest/gtest.h>
#include "s2/third_party/absl/memory/memory.h"
#include "s2/encoded_s2shape_index.h"
#include "s2/mutable_s2shape_index.h"
#include "s2/s1angle.h"
#include "s2/s2cap.h"
#include "s2/s2cell.h"
#include "s2/s2cell_id.h"
#include "s2/s2closest_edge_query_testing.h"
#include "s2/s2edge_distances.h"
#include "s2/s2loop.h"
#include "s2/s2metrics.h"
#include "s2/s2point_vector_shape.h"
#include "s2/s2polygon.h"
#include "s2/s2predicates.h"
#include "s2/s2shapeutil_coding.h"
#include "s2/s2testing.h"
#include "s2/s2text_format.h"

using absl::make_unique;
using s2shapeutil::ShapeEdgeId;
using s2textformat::MakeIndexOrDie;
using s2textformat::MakePointOrDie;
using s2textformat::MakePolygonOrDie;
using s2textformat::ParsePointsOrDie;
using std::min;
using std::pair;
using std::unique_ptr;
using std::vector;

TEST(S2ClosestEdgeQuery, NoEdges) {
  MutableS2ShapeIndex index;
  S2ClosestEdgeQuery query(&index);
  S2ClosestEdgeQuery::PointTarget target(S2Point(1, 0, 0));
  const auto edge = query.FindClosestEdge(&target);
  EXPECT_EQ(S1ChordAngle::Infinity(), edge.distance());
  EXPECT_EQ(-1, edge.shape_id());
  EXPECT_EQ(-1, edge.edge_id());
  EXPECT_FALSE(edge.is_interior());
  EXPECT_TRUE(edge.is_empty());
  EXPECT_EQ(S1ChordAngle::Infinity(), query.GetDistance(&target));
}

TEST(S2ClosestEdgeQuery, OptionsNotModified) {
  // Tests that FindClosestEdge(), GetDistance(), and IsDistanceLess() do not
  // modify query.options(), even though all of these methods have their own
  // specific options requirements.
  S2ClosestEdgeQuery::Options options;
  options.set_max_results(3);
  options.set_max_distance(S1ChordAngle::Degrees(3));
  options.set_max_error(S1ChordAngle::Degrees(0.001));
  auto index = MakeIndexOrDie("1:1 | 1:2 | 1:3 # #");
  S2ClosestEdgeQuery query(index.get(), options);
  S2ClosestEdgeQuery::PointTarget target(MakePointOrDie("2:2"));
  EXPECT_EQ(1, query.FindClosestEdge(&target).edge_id());
  EXPECT_NEAR(1.0, query.GetDistance(&target).degrees(), 1e-15);
  EXPECT_TRUE(query.IsDistanceLess(&target, S1ChordAngle::Degrees(1.5)));

  // Verify that none of the options above were modified.
  EXPECT_EQ(options.max_results(), query.options().max_results());
  EXPECT_EQ(options.max_distance(), query.options().max_distance());
  EXPECT_EQ(options.max_error(), query.options().max_error());
}

TEST(S2ClosestEdgeQuery, DistanceEqualToLimit) {
  // Tests the behavior of IsDistanceLess, IsDistanceLessOrEqual, and
  // IsConservativeDistanceLessOrEqual (and the corresponding Options) when
  // the distance to the target exactly equals the chosen limit.
  S2Point p0(MakePointOrDie("23:12")), p1(MakePointOrDie("47:11"));
  vector<S2Point> index_points{p0};
  MutableS2ShapeIndex index;
  index.Add(make_unique<S2PointVectorShape>(index_points));
  S2ClosestEdgeQuery query(&index);

  // Start with two identical points and a zero distance.
  S2ClosestEdgeQuery::PointTarget target0(p0);
  S1ChordAngle dist0 = S1ChordAngle::Zero();
  EXPECT_FALSE(query.IsDistanceLess(&target0, dist0));
  EXPECT_TRUE(query.IsDistanceLessOrEqual(&target0, dist0));
  EXPECT_TRUE(query.IsConservativeDistanceLessOrEqual(&target0, dist0));

  // Now try two points separated by a non-zero distance.
  S2ClosestEdgeQuery::PointTarget target1(p1);
  S1ChordAngle dist1(p0, p1);
  EXPECT_FALSE(query.IsDistanceLess(&target1, dist1));
  EXPECT_TRUE(query.IsDistanceLessOrEqual(&target1, dist1));
  EXPECT_TRUE(query.IsConservativeDistanceLessOrEqual(&target1, dist1));
}

TEST(S2ClosestEdgeQuery, TrueDistanceLessThanS1ChordAngleDistance) {
  // Tests that IsConservativeDistanceLessOrEqual returns points where the
  // true distance is slightly less than the one computed by S1ChordAngle.
  //
  // The points below had the worst error from among 100,000 random pairs.
  S2Point p0(0.78516762584829192, -0.50200400690845970, -0.36263449417782678);
  S2Point p1(0.78563011732429433, -0.50187655940493503, -0.36180828883938054);

  // The S1ChordAngle distance is ~4 ulps greater than the true distance.
  S1ChordAngle dist1(p0, p1);
  auto limit = dist1.Predecessor().Predecessor().Predecessor().Predecessor();
  ASSERT_LT(s2pred::CompareDistance(p0, p1, limit), 0);

  // Verify that IsConservativeDistanceLessOrEqual() still returns "p1".
  vector<S2Point> index_points{p0};
  MutableS2ShapeIndex index;
  index.Add(make_unique<S2PointVectorShape>(index_points));
  S2ClosestEdgeQuery query(&index);
  S2ClosestEdgeQuery::PointTarget target1(p1);
  EXPECT_FALSE(query.IsDistanceLess(&target1, limit));
  EXPECT_FALSE(query.IsDistanceLessOrEqual(&target1, limit));
  EXPECT_TRUE(query.IsConservativeDistanceLessOrEqual(&target1, limit));
}

TEST(S2ClosestEdgeQuery, TestReuseOfQuery) {
  // Tests that between queries, the internal mechanism for de-duplicating
  // results is re-set.  See b/71646017.
  auto index = MakeIndexOrDie("2:2 # #");
  S2ClosestEdgeQuery query(index.get());
  query.mutable_options()->set_max_error(S1Angle::Degrees(1));
  auto target_index = MakeIndexOrDie("## 0:0, 0:5, 5:5, 5:0");
  S2ClosestEdgeQuery::ShapeIndexTarget target(target_index.get());
  auto results1 = query.FindClosestEdges(&target);
  auto results2 = query.FindClosestEdges(&target);
  EXPECT_EQ(results1.size(), results2.size());
}

TEST(S2ClosestEdgeQuery, TargetPointInsideIndexedPolygon) {
  // Tests a target point in the interior of an indexed polygon.
  // (The index also includes a polyline loop with no interior.)
  auto index = MakeIndexOrDie("# 0:0, 0:5, 5:5, 5:0 # 0:10, 0:15, 5:15, 5:10");
  S2ClosestEdgeQuery::Options options;
  options.set_include_interiors(true);
  options.set_max_distance(S1Angle::Degrees(1));
  S2ClosestEdgeQuery query(index.get(), options);
  S2ClosestEdgeQuery::PointTarget target(MakePointOrDie("2:12"));
  auto results = query.FindClosestEdges(&target);
  ASSERT_EQ(1, results.size());
  EXPECT_EQ(S1ChordAngle::Zero(), results[0].distance());
  EXPECT_EQ(1, results[0].shape_id());
  EXPECT_EQ(-1, results[0].edge_id());
  EXPECT_TRUE(results[0].is_interior());
  EXPECT_FALSE(results[0].is_empty());
}

TEST(S2ClosestEdgeQuery, TargetPointOutsideIndexedPolygon) {
  // Tests a target point in the interior of a polyline loop with no
  // interior.  (The index also includes a nearby polygon.)
  auto index = MakeIndexOrDie("# 0:0, 0:5, 5:5, 5:0 # 0:10, 0:15, 5:15, 5:10");
  S2ClosestEdgeQuery::Options options;
  options.set_include_interiors(true);
  options.set_max_distance(S1Angle::Degrees(1));
  S2ClosestEdgeQuery query(index.get(), options);
  S2ClosestEdgeQuery::PointTarget target(MakePointOrDie("2:2"));
  auto results = query.FindClosestEdges(&target);
  EXPECT_EQ(0, results.size());
}

TEST(S2ClosestEdgeQuery, TargetPolygonContainingIndexedPoints) {
  // Two points are contained within a polyline loop (no interior) and two
  // points are contained within a polygon.
  auto index = MakeIndexOrDie("2:2 | 3:3 | 1:11 | 3:13 # #");
  S2ClosestEdgeQuery query(index.get());
  query.mutable_options()->set_max_distance(S1Angle::Degrees(1));
  auto target_index = MakeIndexOrDie(
      "# 0:0, 0:5, 5:5, 5:0 # 0:10, 0:15, 5:15, 5:10");
  S2ClosestEdgeQuery::ShapeIndexTarget target(target_index.get());
  target.set_include_interiors(true);
  auto results = query.FindClosestEdges(&target);
  ASSERT_EQ(2, results.size());
  EXPECT_EQ(S1ChordAngle::Zero(), results[0].distance());
  EXPECT_EQ(0, results[0].shape_id());
  EXPECT_EQ(2, results[0].edge_id());  // 1:11
  EXPECT_EQ(S1ChordAngle::Zero(), results[1].distance());
  EXPECT_EQ(0, results[1].shape_id());
  EXPECT_EQ(3, results[1].edge_id());  // 3:13
}

TEST(S2ClosestEdgeQuery, EmptyPolygonTarget) {
  // Verifies that distances are measured correctly to empty polygon targets.
  auto empty_polygon_index = MakeIndexOrDie("# # empty");
  auto point_index = MakeIndexOrDie("1:1 # #");
  auto full_polygon_index = MakeIndexOrDie("# # full");
  S2ClosestEdgeQuery::ShapeIndexTarget target(empty_polygon_index.get());
  target.set_include_interiors(true);

  S2ClosestEdgeQuery empty_query(empty_polygon_index.get());
  empty_query.mutable_options()->set_include_interiors(true);
  EXPECT_EQ(S1ChordAngle::Infinity(), empty_query.GetDistance(&target));

  S2ClosestEdgeQuery point_query(point_index.get());
  point_query.mutable_options()->set_include_interiors(true);
  EXPECT_EQ(S1ChordAngle::Infinity(), point_query.GetDistance(&target));

  S2ClosestEdgeQuery full_query(full_polygon_index.get());
  full_query.mutable_options()->set_include_interiors(true);
  EXPECT_EQ(S1ChordAngle::Infinity(), full_query.GetDistance(&target));
}

TEST(S2ClosestEdgeQuery, FullLaxPolygonTarget) {
  // Verifies that distances are measured correctly to full LaxPolygon targets.
  auto empty_polygon_index = MakeIndexOrDie("# # empty");
  auto point_index = MakeIndexOrDie("1:1 # #");
  auto full_polygon_index = MakeIndexOrDie("# # full");
  S2ClosestEdgeQuery::ShapeIndexTarget target(full_polygon_index.get());
  target.set_include_interiors(true);

  S2ClosestEdgeQuery empty_query(empty_polygon_index.get());
  empty_query.mutable_options()->set_include_interiors(true);
  EXPECT_EQ(S1ChordAngle::Infinity(), empty_query.GetDistance(&target));

  S2ClosestEdgeQuery point_query(point_index.get());
  point_query.mutable_options()->set_include_interiors(true);
  EXPECT_EQ(S1ChordAngle::Zero(), point_query.GetDistance(&target));

  S2ClosestEdgeQuery full_query(full_polygon_index.get());
  full_query.mutable_options()->set_include_interiors(true);
  EXPECT_EQ(S1ChordAngle::Zero(), full_query.GetDistance(&target));
}

TEST(S2ClosestEdgeQuery, FullS2PolygonTarget) {
  // Verifies that distances are measured correctly to full S2Polygon targets
  // (which use a different representation of "full" than LaxPolygon does).
  auto empty_polygon_index = MakeIndexOrDie("# # empty");
  auto point_index = MakeIndexOrDie("1:1 # #");
  auto full_polygon_index = MakeIndexOrDie("# #");
  full_polygon_index->Add(make_unique<S2Polygon::OwningShape>(
      s2textformat::MakePolygonOrDie("full")));

  S2ClosestEdgeQuery::ShapeIndexTarget target(full_polygon_index.get());
  target.set_include_interiors(true);

  S2ClosestEdgeQuery empty_query(empty_polygon_index.get());
  empty_query.mutable_options()->set_include_interiors(true);
  EXPECT_EQ(S1ChordAngle::Infinity(), empty_query.GetDistance(&target));

  S2ClosestEdgeQuery point_query(point_index.get());
  point_query.mutable_options()->set_include_interiors(true);
  EXPECT_EQ(S1ChordAngle::Zero(), point_query.GetDistance(&target));

  S2ClosestEdgeQuery full_query(full_polygon_index.get());
  full_query.mutable_options()->set_include_interiors(true);
  EXPECT_EQ(S1ChordAngle::Zero(), full_query.GetDistance(&target));
}

TEST(S2ClosestEdgeQuery, IsConservativeDistanceLessOrEqual) {
  // Test
  int num_tested = 0;
  int num_conservative_needed = 0;
  auto& rnd = S2Testing::rnd;
  for (int iter = 0; iter < 1000; ++iter) {
    rnd.Reset(iter + 1);  // Easier to reproduce a specific case.
    S2Point x = S2Testing::RandomPoint();
    S2Point dir = S2Testing::RandomPoint();
    S1Angle r = S1Angle::Radians(M_PI * pow(1e-30, rnd.RandDouble()));
    S2Point y = S2::InterpolateAtDistance(r, x, dir);
    S1ChordAngle limit(r);
    if (s2pred::CompareDistance(x, y, limit) <= 0) {
      MutableS2ShapeIndex index;
      index.Add(make_unique<S2PointVectorShape>(vector<S2Point>({x})));
      S2ClosestEdgeQuery query(&index);
      S2ClosestEdgeQuery::PointTarget target(y);
      EXPECT_TRUE(query.IsConservativeDistanceLessOrEqual(&target, limit));
      ++num_tested;
      if (!query.IsDistanceLess(&target, limit)) ++num_conservative_needed;
    }
  }
  // Verify that in most test cases, the distance between the target points
  // was close to the desired value.  Also verify that at least in some test
  // cases, the conservative distance test was actually necessary.
  EXPECT_GE(num_tested, 300);
  EXPECT_LE(num_tested, 700);
  EXPECT_GE(num_conservative_needed, 25);
}

// The approximate radius of S2Cap from which query edges are chosen.
static const S1Angle kTestCapRadius = S2Testing::KmToAngle(10);

// An approximate bound on the distance measurement error for "reasonable"
// distances (say, less than Pi/2) due to using S1ChordAngle.
static const double kTestChordAngleError = 1e-15;

using TestingResult = pair<S2MinDistance, ShapeEdgeId>;

// Converts to the format required by CheckDistanceResults() in s2testing.h.
vector<TestingResult> ConvertResults(
    const vector<S2ClosestEdgeQuery::Result>& results) {
  vector<TestingResult> testing_results;
  for (const auto& result : results) {
    testing_results.push_back(
        TestingResult(result.distance(),
                      ShapeEdgeId(result.shape_id(), result.edge_id())));
  }
  return testing_results;
}

// Use "query" to find the closest edge(s) to the given target.  Verify that
// the results satisfy the search criteria.
static void GetClosestEdges(S2ClosestEdgeQuery::Target* target,
                            S2ClosestEdgeQuery *query,
                            vector<S2ClosestEdgeQuery::Result>* edges) {
  query->FindClosestEdges(target, edges);
  EXPECT_LE(edges->size(), query->options().max_results());
  if (query->options().max_distance() ==
      S2ClosestEdgeQuery::Distance::Infinity()) {
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
    // Check that the edge satisfies the max_distance() condition.
    EXPECT_LT(edge.distance(), query->options().max_distance());
  }
}

static S2ClosestEdgeQuery::Result TestFindClosestEdges(
    S2ClosestEdgeQuery::Target* target, S2ClosestEdgeQuery *query) {
  vector<S2ClosestEdgeQuery::Result> expected, actual;
  query->mutable_options()->set_use_brute_force(true);
  GetClosestEdges(target, query, &expected);
  query->mutable_options()->set_use_brute_force(false);
  GetClosestEdges(target, query, &actual);
  EXPECT_TRUE(CheckDistanceResults(ConvertResults(expected),
                                   ConvertResults(actual),
                                   query->options().max_results(),
                                   query->options().max_distance(),
                                   query->options().max_error()))
      << "max_results=" << query->options().max_results()
      << ", max_distance=" << query->options().max_distance()
      << ", max_error=" << query->options().max_error();

  if (expected.empty()) return S2ClosestEdgeQuery::Result();

  // Note that when options.max_error() > 0, expected[0].distance() may not
  // be the minimum distance.  It is never larger by more than max_error(),
  // but the actual value also depends on max_results().
  //
  // Here we verify that GetDistance() and IsDistanceLess() return results
  // that are consistent with the max_error() setting.
  S1ChordAngle max_error = query->options().max_error();
  S1ChordAngle min_distance = expected[0].distance();
  EXPECT_LE(query->GetDistance(target), min_distance + max_error);

  // Test IsDistanceLess().
  EXPECT_FALSE(query->IsDistanceLess(target, min_distance - max_error));
  EXPECT_TRUE(query->IsConservativeDistanceLessOrEqual(target, min_distance));

  // Return the closest edge result so that we can also test Project.
  return expected[0];
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
    indexes.push_back(make_unique<MutableS2ShapeIndex>());
    factory.AddEdges(index_caps.back(), num_edges, indexes.back().get());
  }
  for (int i = 0; i < num_queries; ++i) {
    S2Testing::rnd.Reset(FLAGS_s2_random_seed + i);
    int i_index = S2Testing::rnd.Uniform(num_indexes);
    const S2Cap& index_cap = index_caps[i_index];

    // Choose query points from an area approximately 4x larger than the
    // geometry being tested.
    S1Angle query_radius = 2 * index_cap.GetRadius();
    S2Cap query_cap(index_cap.center(), query_radius);
    S2ClosestEdgeQuery query(indexes[i_index].get());

    // Occasionally we don't set any limit on the number of result edges.
    // (This may return all edges if we also don't set a distance limit.)
    if (!S2Testing::rnd.OneIn(5)) {
      query.mutable_options()->set_max_results(1 + S2Testing::rnd.Uniform(10));
    }
    // We set a distance limit 2/3 of the time.
    if (!S2Testing::rnd.OneIn(3)) {
      query.mutable_options()->set_max_distance(
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
      // Find the edges closest to a given point.
      S2Point point = S2Testing::SamplePoint(query_cap);
      S2ClosestEdgeQuery::PointTarget target(point);
      auto closest = TestFindClosestEdges(&target, &query);
      if (!closest.distance().is_infinity()) {
        // Also test the Project method.
        EXPECT_NEAR(
            closest.distance().ToAngle().radians(),
            S1Angle(point, query.Project(point, closest)).radians(),
            kTestChordAngleError);
      }
    } else if (target_type == 1) {
      // Find the edges closest to a given edge.
      S2Point a = S2Testing::SamplePoint(query_cap);
      S2Point b = S2Testing::SamplePoint(
          S2Cap(a, pow(1e-4, S2Testing::rnd.RandDouble()) * query_radius));
      S2ClosestEdgeQuery::EdgeTarget target(a, b);
      TestFindClosestEdges(&target, &query);
    } else if (target_type == 2) {
      // Find the edges closest to a given cell.
      int min_level = S2::kMaxDiag.GetLevelForMaxValue(query_radius.radians());
      int level = min_level + S2Testing::rnd.Uniform(
          S2CellId::kMaxLevel - min_level + 1);
      S2Point a = S2Testing::SamplePoint(query_cap);
      S2Cell cell(S2CellId(a).parent(level));
      S2ClosestEdgeQuery::CellTarget target(cell);
      TestFindClosestEdges(&target, &query);
    } else {
      S2_DCHECK_EQ(3, target_type);
      // Use another one of the pre-built indexes as the target.
      int j_index = S2Testing::rnd.Uniform(num_indexes);
      S2ClosestEdgeQuery::ShapeIndexTarget target(indexes[j_index].get());
      target.set_include_interiors(S2Testing::rnd.OneIn(2));
      TestFindClosestEdges(&target, &query);
    }
  }
}

static const int kNumIndexes = 50;
static const int kNumEdges = 100;
static const int kNumQueries = 200;

TEST(S2ClosestEdgeQuery, CircleEdges) {
  TestWithIndexFactory(s2testing::RegularLoopShapeIndexFactory(),
                       kNumIndexes, kNumEdges, kNumQueries);
}

TEST(S2ClosestEdgeQuery, FractalEdges) {
  TestWithIndexFactory(s2testing::FractalLoopShapeIndexFactory(),
                       kNumIndexes, kNumEdges, kNumQueries);
}

TEST(S2ClosestEdgeQuery, PointCloudEdges) {
  TestWithIndexFactory(s2testing::PointCloudShapeIndexFactory(),
                       kNumIndexes, kNumEdges, kNumQueries);
}

TEST(S2ClosestEdgeQuery, ConservativeCellDistanceIsUsed) {
  // Don't use google::FlagSaver, so it works in opensource without gflags.
  const int saved_seed = FLAGS_s2_random_seed;
  // These specific test cases happen to fail if max_error() is not properly
  // taken into account when measuring distances to S2ShapeIndex cells.
  for (int seed : {42, 681, 894, 1018, 1750, 1759, 2401}) {
    FLAGS_s2_random_seed = seed;
    TestWithIndexFactory(s2testing::FractalLoopShapeIndexFactory(),
                         5, 100, 10);
  }
  FLAGS_s2_random_seed = saved_seed;
}

