// Copyright 2015 Google Inc. All Rights Reserved.
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

#include "s2/s2closest_point_query.h"

#include <memory>
#include <vector>

#include <gtest/gtest.h>
#include "s2/third_party/absl/memory/memory.h"
#include "s2/s1angle.h"
#include "s2/s2cap.h"
#include "s2/s2cell_id.h"
#include "s2/s2closest_edge_query_testing.h"
#include "s2/s2edge_distances.h"
#include "s2/s2loop.h"
#include "s2/s2pointutil.h"
#include "s2/s2testing.h"

using absl::make_unique;
using std::pair;
using std::unique_ptr;
using std::vector;

using TestIndex = S2PointIndex<int>;
using TestQuery = S2ClosestPointQuery<int>;

TEST(S2ClosestPointQuery, NoPoints) {
  TestIndex index;
  TestQuery query(&index);
  S2ClosestPointQueryPointTarget target(S2Point(1, 0, 0));
  const auto results = query.FindClosestPoints(&target);
  EXPECT_EQ(0, results.size());
}

TEST(S2ClosestPointQuery, ManyDuplicatePoints) {
  static const int kNumPoints = 10000;
  const S2Point kTestPoint(1, 0, 0);
  TestIndex index;
  for (int i = 0; i < kNumPoints; ++i) {
    index.Add(kTestPoint, i);
  }
  TestQuery query(&index);
  S2ClosestPointQueryPointTarget target(kTestPoint);
  const auto results = query.FindClosestPoints(&target);
  EXPECT_EQ(kNumPoints, results.size());
}

// An abstract class that adds points to an S2PointIndex for benchmarking.
struct PointIndexFactory {
 public:
  virtual ~PointIndexFactory() {}

  // Requests that approximately "num_points" points located within the given
  // S2Cap bound should be added to "index".
  virtual void AddPoints(const S2Cap& index_cap, int num_points,
                         TestIndex* index) const = 0;
};

// Generates points that are regularly spaced along a circle.  Points along a
// circle are nearly the worst case for distance calculations, since many
// points are nearly equidistant from any query point that is not immediately
// adjacent to the circle.
struct CirclePointIndexFactory : public PointIndexFactory {
  void AddPoints(const S2Cap& index_cap, int num_points,
                 TestIndex* index) const override {
    vector<S2Point> points = S2Testing::MakeRegularPoints(
        index_cap.center(), index_cap.GetRadius(), num_points);
    for (int i = 0; i < points.size(); ++i) {
      index->Add(points[i], i);
    }
  }
};

// Generates the vertices of a fractal whose convex hull approximately
// matches the given cap.
struct FractalPointIndexFactory : public PointIndexFactory {
  void AddPoints(const S2Cap& index_cap, int num_points,
                 TestIndex* index) const override {
    S2Testing::Fractal fractal;
    fractal.SetLevelForApproxMaxEdges(num_points);
    fractal.set_fractal_dimension(1.5);
    unique_ptr<S2Loop> loop(
        fractal.MakeLoop(S2Testing::GetRandomFrameAt(index_cap.center()),
                         index_cap.GetRadius()));
    for (int i = 0; i < loop->num_vertices(); ++i) {
      index->Add(loop->vertex(i), i);
    }
  }
};

// Generates vertices on a square grid that includes the entire given cap.
struct GridPointIndexFactory : public PointIndexFactory {
  void AddPoints(const S2Cap& index_cap, int num_points,
                 TestIndex* index) const override {
    int sqrt_num_points = ceil(sqrt(num_points));
    Matrix3x3_d frame = S2Testing::GetRandomFrameAt(index_cap.center());
    double radius = index_cap.GetRadius().radians();
    double spacing = 2 * radius / sqrt_num_points;
    for (int i = 0; i < sqrt_num_points; ++i) {
      for (int j = 0; j < sqrt_num_points; ++j) {
        S2Point point(tan((i + 0.5) * spacing - radius),
                      tan((j + 0.5) * spacing - radius), 1.0);
        index->Add(S2::FromFrame(frame, point.Normalize()),
                   i * sqrt_num_points + j);
      }
    }
  }
};

// The approximate radius of S2Cap from which query points are chosen.
static const S1Angle kTestCapRadius = S2Testing::KmToAngle(10);

// The result format required by CheckDistanceResults() in s2testing.h.
using TestingResult = pair<S2MinDistance, int>;

// Use "query" to find the closest point(s) to the given target, and extract
// the query results into the given vector.  Also verify that the results
// satisfy the search criteria.
static void GetClosestPoints(TestQuery::Target* target, TestQuery* query,
                             vector<TestingResult>* results) {
  const auto query_results = query->FindClosestPoints(target);
  EXPECT_LE(query_results.size(), query->options().max_results());
  const S2Region* region = query->options().region();
  if (!region && query->options().max_distance() == S1ChordAngle::Infinity()) {
    // We can predict exactly how many points should be returned.
    EXPECT_EQ(std::min(query->options().max_results(),
                       query->index().num_points()),
              query_results.size());
  }
  for (const auto& result : query_results) {
    // Check that the point satisfies the region() condition.
    if (region) EXPECT_TRUE(region->Contains(result.point()));

    // Check that it satisfies the max_distance() condition.
    EXPECT_LT(result.distance(), query->options().max_distance());
    results->push_back(TestingResult(result.distance(), result.data()));
  }
}

static void TestFindClosestPoints(TestQuery::Target* target, TestQuery *query) {
  vector<TestingResult> expected, actual;
  query->mutable_options()->set_use_brute_force(true);
  GetClosestPoints(target, query, &expected);
  query->mutable_options()->set_use_brute_force(false);
  GetClosestPoints(target, query, &actual);
  EXPECT_TRUE(CheckDistanceResults(expected, actual,
                                   query->options().max_results(),
                                   query->options().max_distance(),
                                   query->options().max_error()))
      << "max_results=" << query->options().max_results()
      << ", max_distance=" << query->options().max_distance()
      << ", max_error=" << query->options().max_error();

  if (expected.empty()) return;

  // Note that when options.max_error() > 0, expected[0].distance may not be
  // the minimum distance.  It is never larger by more than max_error(), but
  // the actual value also depends on max_results().
  //
  // Here we verify that GetDistance() and IsDistanceLess() return results
  // that are consistent with the max_error() setting.
  S1ChordAngle max_error = query->options().max_error();
  S1ChordAngle min_distance = expected[0].first;
  EXPECT_LE(query->GetDistance(target), min_distance + max_error);

  // Test IsDistanceLess().
  EXPECT_FALSE(query->IsDistanceLess(target, min_distance - max_error));
  EXPECT_TRUE(query->IsConservativeDistanceLessOrEqual(target, min_distance));
}

// (Note that every query is checked using the brute force algorithm.)
static void TestWithIndexFactory(const PointIndexFactory& factory,
                                 int num_indexes, int num_points,
                                 int num_queries) {
  // Build a set of S2PointIndexes containing the desired geometry.
  vector<S2Cap> index_caps;
  vector<unique_ptr<TestIndex>> indexes;
  for (int i = 0; i < num_indexes; ++i) {
    S2Testing::rnd.Reset(FLAGS_s2_random_seed + i);
    index_caps.push_back(S2Cap(S2Testing::RandomPoint(), kTestCapRadius));
    indexes.push_back(make_unique<TestIndex>());
    factory.AddPoints(index_caps.back(), num_points, indexes.back().get());
  }
  for (int i = 0; i < num_queries; ++i) {
    S2Testing::rnd.Reset(FLAGS_s2_random_seed + i);
    int i_index = S2Testing::rnd.Uniform(num_indexes);
    const S2Cap& index_cap = index_caps[i_index];

    // Choose query points from an area approximately 4x larger than the
    // geometry being tested.
    S1Angle query_radius = 2 * index_cap.GetRadius();
    S2Cap query_cap(index_cap.center(), query_radius);
    TestQuery query(indexes[i_index].get());

    // Occasionally we don't set any limit on the number of result points.
    // (This may return all points if we also don't set a distance limit.)
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
    S2LatLngRect filter_rect = S2LatLngRect::FromCenterSize(
        S2LatLng(S2Testing::SamplePoint(query_cap)),
        S2LatLng(S2Testing::rnd.RandDouble() * kTestCapRadius,
                 S2Testing::rnd.RandDouble() * kTestCapRadius));
    if (S2Testing::rnd.OneIn(5)) {
      query.mutable_options()->set_region(&filter_rect);
    }
    int target_type = S2Testing::rnd.Uniform(4);
    if (target_type == 0) {
      // Find the points closest to a given point.
      S2Point point = S2Testing::SamplePoint(query_cap);
      TestQuery::PointTarget target(point);
      TestFindClosestPoints(&target, &query);
    } else if (target_type == 1) {
      // Find the points closest to a given edge.
      S2Point a = S2Testing::SamplePoint(query_cap);
      S2Point b = S2Testing::SamplePoint(
          S2Cap(a, pow(1e-4, S2Testing::rnd.RandDouble()) * query_radius));
      TestQuery::EdgeTarget target(a, b);
      TestFindClosestPoints(&target, &query);
    } else if (target_type == 2) {
      // Find the points closest to a given cell.
      int min_level = S2::kMaxDiag.GetLevelForMaxValue(query_radius.radians());
      int level = min_level + S2Testing::rnd.Uniform(
          S2CellId::kMaxLevel - min_level + 1);
      S2Point a = S2Testing::SamplePoint(query_cap);
      S2Cell cell(S2CellId(a).parent(level));
      TestQuery::CellTarget target(cell);
      TestFindClosestPoints(&target, &query);
    } else {
      S2_DCHECK_EQ(3, target_type);
      MutableS2ShapeIndex target_index;
      s2testing::FractalLoopShapeIndexFactory().AddEdges(index_cap, 100,
                                                         &target_index);
      TestQuery::ShapeIndexTarget target(&target_index);
      target.set_include_interiors(S2Testing::rnd.OneIn(2));
      TestFindClosestPoints(&target, &query);
    }
  }
}

static const int kNumIndexes = 10;
static const int kNumPoints = 1000;
static const int kNumQueries = 50;

TEST(S2ClosestPointQueryTest, CirclePoints) {
  TestWithIndexFactory(CirclePointIndexFactory(),
                       kNumIndexes, kNumPoints, kNumQueries);
}

TEST(S2ClosestPointQueryTest, FractalPoints) {
  TestWithIndexFactory(FractalPointIndexFactory(),
                       kNumIndexes, kNumPoints, kNumQueries);
}

TEST(S2ClosestPointQueryTest, GridPoints) {
  TestWithIndexFactory(GridPointIndexFactory(),
                       kNumIndexes, kNumPoints, kNumQueries);
}

TEST(S2ClosestPointQueryTest, ConservativeCellDistanceIsUsed) {
  const int saved_seed = FLAGS_s2_random_seed;
  // These specific test cases happen to fail if max_error() is not properly
  // taken into account when measuring distances to S2PointIndex cells.  They
  // all involve S2ShapeIndexTarget, which takes advantage of max_error() to
  // optimize its distance calculation.
  for (int seed : {16, 586, 589, 822, 1959, 2298, 3155, 3490, 3723, 4953}) {
    FLAGS_s2_random_seed = seed;
    TestWithIndexFactory(FractalPointIndexFactory(), 5, 100, 10);
  }
  FLAGS_s2_random_seed = saved_seed;
}

