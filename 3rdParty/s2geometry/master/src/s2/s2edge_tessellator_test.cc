// Copyright 2017 Google Inc. All Rights Reserved.
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

#include "s2/s2edge_tessellator.h"

#include <algorithm>
#include <iostream>

#include <gtest/gtest.h>

#include "absl/strings/str_cat.h"

#include "s2/base/log_severity.h"
#include "s2/s2edge_distances.h"
#include "s2/s2loop.h"
#include "s2/s2pointutil.h"
#include "s2/s2projections.h"
#include "s2/s2testing.h"
#include "s2/s2text_format.h"

using absl::StrCat;
using s2textformat::ParsePointsOrDie;
using std::cout;
using std::endl;
using std::fabs;
using std::min;
using std::max;
using std::string;
using std::vector;

namespace {

class Stats {
 public:
  Stats() : max_(-std::numeric_limits<double>::infinity()),
            sum_(0), count_(0) {
  }

  void Tally(double v) {
    if (std::isnan(v)) S2_LOG(FATAL) << "NaN";
    max_ = std::max(v, max_);
    sum_ += v;
    count_ += 1;
  }

  double max() const { return max_; }
  double avg() const { return sum_ / count_; }

  string ToString() const {
    return StrCat("avg = ", sum_ / count_, ", max = ", max_);
  }

 private:
  double max_, sum_;
  int count_;
};

// Determines whether the distance between the two edges is measured
// geometrically or parameterically (see algorithm description in .cc file).
enum class DistType { GEOMETRIC, PARAMETRIC };

S1Angle GetMaxDistance(const S2::Projection& proj,
                       const R2Point& px, const S2Point& x,
                       const R2Point& py, const S2Point& y,
                       DistType dist_type = DistType::GEOMETRIC) {
  // Step along the projected edge at a fine resolution and keep track of the
  // maximum distance of any point to the current geodesic edge.
  const int kNumSteps = 100;
  S1ChordAngle max_dist = S1ChordAngle::Zero();
  for (double f = 0.5 / kNumSteps; f < 1.0; f += 1.0 / kNumSteps) {
    S1ChordAngle dist = S1ChordAngle::Infinity();
    S2Point p = proj.Unproject(proj.Interpolate(f, px, py));
    if (dist_type == DistType::GEOMETRIC) {
      S2::UpdateMinDistance(p, x, y, &dist);
    } else {
      S2_DCHECK(dist_type == DistType::PARAMETRIC);
      dist = S1ChordAngle(p, S2::Interpolate(x, y, f));
    }
    if (dist > max_dist) max_dist = dist;
  }
  // Ensure that the maximum distance estimate is a lower bound, not an upper
  // bound, since we only want to record a failure of the distance estimation
  // algorithm if the number it returns is definitely too small.
  return S1Angle(
      max_dist.PlusError(-S2::GetUpdateMinDistanceMaxError(max_dist)));
}

// When there are longitudes greater than 180 degrees due to wrapping, the
// combination of projecting and unprojecting an S2Point can have slightly more
// error than is allowed by S2::ApproxEquals.
const S1Angle kMaxProjError(S1Angle::Radians(2e-15));

// Converts a projected edge to a sequence of geodesic edges and verifies that
// the result satisfies the given tolerance.
Stats TestUnprojected(const S2::Projection& proj, S1Angle tolerance,
                      const R2Point& pa, const R2Point& pb_in, bool log_stats) {
  S2EdgeTessellator tess(&proj, tolerance);
  vector<S2Point> vertices;
  tess.AppendUnprojected(pa, pb_in, &vertices);
  R2Point pb = proj.WrapDestination(pa, pb_in);
  EXPECT_LE(S1Angle(proj.Unproject(pa), vertices.front()), kMaxProjError);
  EXPECT_LE(S1Angle(proj.Unproject(pb), vertices.back()), kMaxProjError);
  Stats stats;
  if (pa == pb) {
    EXPECT_EQ(1, vertices.size());
    return stats;
  }
  // Precompute the normal to the projected edge.
  Vector2_d norm = (pb - pa).Ortho().Normalize();
  S2Point x = vertices[0];
  R2Point px = proj.Project(x);
  for (int i = 1; i < vertices.size(); ++i) {
    S2Point y = vertices[i];
    R2Point py = proj.WrapDestination(px, proj.Project(y));
    // Check that every vertex is on the projected edge.
    EXPECT_LE((py - pa).DotProd(norm), 1e-14 * py.Norm());
    stats.Tally(GetMaxDistance(proj, px, x, py, y) / tolerance);
    x = y;
    px = py;
  }
  if (log_stats) {
    cout << pa << " to " << pb << ": " << vertices.size() << " vertices, "
         << stats.ToString() << endl;
  }
  return stats;
}

// Converts a geodesic edge to a sequence of projected edges and verifies that
// the result satisfies the given tolerance.
Stats TestProjected(const S2::Projection& proj, S1Angle tolerance,
                    const S2Point& a, const S2Point& b, bool log_stats) {
  S2EdgeTessellator tess(&proj, tolerance);
  vector<R2Point> vertices;
  tess.AppendProjected(a, b, &vertices);
  EXPECT_LE(S1Angle(a, proj.Unproject(vertices.front())), kMaxProjError);
  EXPECT_LE(S1Angle(b, proj.Unproject(vertices.back())), kMaxProjError);
  Stats stats;
  if (a == b) {
    EXPECT_EQ(1, vertices.size());
    return stats;
  }
  R2Point px = vertices[0];
  S2Point x = proj.Unproject(px);
  for (int i = 1; i < vertices.size(); ++i) {
    R2Point py = vertices[i];
    S2Point y = proj.Unproject(py);
    // Check that every vertex is on the geodesic edge.
    static S1ChordAngle kMaxInterpolationError(S1Angle::Radians(1e-14));
    EXPECT_TRUE(S2::IsDistanceLess(y, a, b, kMaxInterpolationError));
    stats.Tally(GetMaxDistance(proj, px, x, py, y) / tolerance);
    x = y;
    px = py;
  }
  if (log_stats) {
    cout << vertices[0] << " to " << px << ": " << vertices.size()
         << " vertices, " << stats.ToString() << endl;
  }
  return stats;
}

TEST(S2EdgeTessellator, ProjectedNoTessellation) {
  S2::PlateCarreeProjection proj(180);
  S2EdgeTessellator tess(&proj, S1Angle::Degrees(0.01));
  vector<R2Point> vertices;
  tess.AppendProjected(S2Point(1, 0, 0), S2Point(0, 1, 0), &vertices);
  EXPECT_EQ(2, vertices.size());
}

TEST(S2EdgeTessellator, UnprojectedNoTessellation) {
  S2::PlateCarreeProjection proj(180);
  S2EdgeTessellator tess(&proj, S1Angle::Degrees(0.01));
  vector<S2Point> vertices;
  tess.AppendUnprojected(R2Point(0, 30), R2Point(0, 50), &vertices);
  EXPECT_EQ(2, vertices.size());
}

TEST(S2EdgeTessellator, UnprojectedWrapping) {
  // This tests that a projected edge that crosses the 180 degree meridian
  // goes the "short way" around the sphere.

  S2::PlateCarreeProjection proj(180);
  S2EdgeTessellator tess(&proj, S1Angle::Degrees(0.01));
  vector<S2Point> vertices;
  tess.AppendUnprojected(R2Point(-170, 0), R2Point(170, 80), &vertices);
  for (const auto& v : vertices) {
    EXPECT_GE(fabs(S2LatLng::Longitude(v).degrees()), 170);
  }
}

TEST(S2EdgeTessellator, ProjectedWrapping) {
  // This tests projecting a geodesic edge that crosses the 180 degree
  // meridian.  This results in a set of vertices that may be non-canonical
  // (i.e., absolute longitudes greater than 180 degrees) but that don't have
  // any sudden jumps in value, which is convenient for interpolating them.
  S2::PlateCarreeProjection proj(180);
  S2EdgeTessellator tess(&proj, S1Angle::Degrees(0.01));
  vector<R2Point> vertices;
  tess.AppendProjected(S2LatLng::FromDegrees(0, -170).ToPoint(),
                       S2LatLng::FromDegrees(0, 170).ToPoint(), &vertices);
  for (const auto& v : vertices) {
    EXPECT_LE(v.x(), -170);
  }
}

TEST(S2EdgeTessellator, UnprojectedWrappingMultipleCrossings) {
  // Tests an edge chain that crosses the 180 degree meridian multiple times.
  // Note that due to coordinate wrapping, the last vertex of one edge may not
  // exactly match the first edge of the next edge after unprojection.
  S2::PlateCarreeProjection proj(180);
  S2EdgeTessellator tess(&proj, S1Angle::Degrees(0.01));
  vector<S2Point> vertices;
  for (double lat = 1; lat <= 60; ++lat) {
    tess.AppendUnprojected(R2Point(180 - 0.03 * lat, lat),
                           R2Point(-180 + 0.07 * lat, lat), &vertices);
    tess.AppendUnprojected(R2Point(-180 + 0.07 * lat, lat),
                           R2Point(180 - 0.03 * (lat + 1), lat + 1), &vertices);
  }
  for (const auto& v : vertices) {
    EXPECT_GE(fabs(S2LatLng::Longitude(v).degrees()), 175);
  }
}

TEST(S2EdgeTessellator, ProjectedWrappingMultipleCrossings) {
  // The following loop crosses the 180 degree meridian four times (twice in
  // each direction).
  auto loop = ParsePointsOrDie("0:160, 0:-40, 0:120, 0:-80, 10:120, "
                               "10:-40, 0:160");
  S2::PlateCarreeProjection proj(180);
  S1Angle tolerance(S1Angle::E7(1));
  S2EdgeTessellator tess(&proj, tolerance);
  vector<R2Point> vertices;
  for (int i = 0; i + 1 < loop.size(); ++i) {
    tess.AppendProjected(loop[i], loop[i + 1], &vertices);
  }
  EXPECT_EQ(vertices.front(), vertices.back());

  // Note that the R2Point coordinates are in (lng, lat) order.
  double min_lng = vertices[0].x();
  double max_lng = vertices[0].x();
  for (const R2Point& v : vertices) {
    min_lng = min(min_lng, v.x());
    max_lng = max(max_lng, v.x());
  }
  EXPECT_EQ(160, min_lng);
  EXPECT_EQ(640, max_lng);
}

TEST(S2EdgeTessellator, InfiniteRecursionBug) {
  S2::PlateCarreeProjection proj(180);
  S1Angle kOneMicron = S1Angle::Radians(1e-6 / 6371.0);
  S2EdgeTessellator tess(&proj, kOneMicron);
  vector<R2Point> vertices;
  tess.AppendProjected(S2LatLng::FromDegrees(3, 21).ToPoint(),
                       S2LatLng::FromDegrees(1, -159).ToPoint(), &vertices);
  EXPECT_EQ(36, vertices.size());
}

TEST(S2EdgeTessellator, UnprojectedAccuracy) {
  S2::MercatorProjection proj(180);
  S1Angle tolerance(S1Angle::Degrees(1e-5));
  R2Point pa(0, 0), pb(89.999999, 179);
  Stats stats = TestUnprojected(proj, tolerance, pa, pb, true);
  EXPECT_LE(stats.max(), 1.0);
}

// Repro case for b/110719057.
TEST(S2EdgeTessellator, UnprojectedAccuracyCrossEquator) {
  S2::MercatorProjection proj(180);
  S1Angle tolerance(S1Angle::Degrees(1e-5));
  R2Point pa(-10, -10), pb(10, 10);
  Stats stats = TestUnprojected(proj, tolerance, pa, pb, true);
  EXPECT_LT(stats.max(), 1.0);
}

TEST(S2EdgeTessellator, ProjectedAccuracy) {
  S2::PlateCarreeProjection proj(180);
  S1Angle tolerance(S1Angle::E7(1));
  S2Point a = S2LatLng::FromDegrees(-89.999, -170).ToPoint();
  S2Point b = S2LatLng::FromDegrees(50, 100).ToPoint();
  Stats stats = TestProjected(proj, tolerance, a, b, true);
  EXPECT_LE(stats.max(), 1.0);
}

TEST(S2EdgeTessellator, UnprojectedAccuracyMidpointEquator) {
  S2::PlateCarreeProjection proj(180);
  S1Angle tolerance = S2Testing::MetersToAngle(1);
  R2Point a(80, 50), b(-80, -50);
  Stats stats = TestUnprojected(proj, tolerance, a, b, true);
  EXPECT_LE(stats.max(), 1.0);
}

TEST(S2EdgeTessellator, ProjectedAccuracyMidpointEquator) {
  S2::PlateCarreeProjection proj(180);
  S1Angle tolerance = S2Testing::MetersToAngle(1);
  S2Point a = S2LatLng::FromDegrees(50, 80).ToPoint();
  S2Point b = S2LatLng::FromDegrees(-50, -80).ToPoint();
  Stats stats = TestProjected(proj, tolerance, a, b, true);
  EXPECT_LE(stats.max(), 1.0);
}

// Repro case for b/110719057.
TEST(S2EdgeTessellator, ProjectedAccuracyCrossEquator) {
  S2::PlateCarreeProjection proj(180);
  S1Angle tolerance(S1Angle::E7(1));
  S2Point a = S2LatLng::FromDegrees(-20, -20).ToPoint();
  S2Point b = S2LatLng::FromDegrees(20, 20).ToPoint();
  Stats stats = TestProjected(proj, tolerance, a, b, true);
  EXPECT_LT(stats.max(), 1.0);
}

TEST(S2EdgeTessellator, ProjectedAccuracySeattleToNewYork) {
  S2::PlateCarreeProjection proj(180);
  S1Angle tolerance = S2Testing::MetersToAngle(1);
  S2Point seattle(S2LatLng::FromDegrees(47.6062, -122.3321).ToPoint());
  S2Point newyork(S2LatLng::FromDegrees(40.7128, -74.0059).ToPoint());
  Stats stats = TestProjected(proj, tolerance, seattle, newyork, true);
  EXPECT_LE(stats.max(), 1.0);
}

// Given a projection, this function repeatedly chooses a pair of edge
// endpoints and measures the true distance between the geodesic and projected
// edges that connect those two endpoints.  It then compares this to against
// the distance measurement algorithm used by S2EdgeTessellator, which
// consists of measuring the point-to-point distance between the edges at each
// of two fractions "t" and "1-t", computing the maximum of those two
// distances, and then scaling by the constant documented in the .cc file
// (based on the idea that the distance between the edges as a function of the
// interpolation fraction can be accurately modeled as a cubic polynomial).
//
// This function is used to (1) verify that the distance estimates are always
// conservative, (2) verify the optimality of the interpolation fraction "t",
// and (3) estimate the amount of overtessellation that occurs for various
// types of edges (e.g., short vs. long edges, edges that follow lines of
// latitude or longitude, etc).
void TestEdgeError(const S2::Projection& proj, double t) {
  // Here we compute how much we need to scale the error measured at the
  // chosen interpolation fraction "t" in order to bound the error along the
  // entire edge, under the assumption that the error is a convex combination
  // of E1(x) and E2(x) (see comments in the .cc file).
  const double x = 1 - 2 * t;
  const double dlat = sin(0.5 * M_PI_4 * (1 - x));
  const double dlng = sin(M_PI_4 * (1 - x));
  const double dsin2 = dlat * dlat + dlng * dlng * sin(M_PI_4 * x) * M_SQRT1_2;
  const double dsin2_max = 0.5 * (1 - M_SQRT1_2);
  // Note that this is the reciprocal of the value used in the .cc file!
  const double kScaleFactor = max((2 * sqrt(3) / 9) / (x * (1 - x * x)),
                                  asin(sqrt(dsin2_max)) / asin(sqrt(dsin2)));

  // Keep track of the average and maximum geometric and parametric errors.
  Stats stats_g, stats_p;
  const int kIters = google::DEBUG_MODE ? 10000 : 100000;
  for (int iter = 0; iter < kIters; ++iter) {
    S2Testing::rnd.Reset(iter);
    S2Point a = S2Testing::RandomPoint();
    S2Point b = S2Testing::RandomPoint();
    // Uncomment to test edges longer than 90 degrees.
    if (a.DotProd(b) < -1e-14) continue;
    // Uncomment to test edges than span more than 90 degrees longitude.
    // if (a[0] * b[0] + a[1] * b[1] < 0) continue;
    // Uncomment to only test edges of a certain length.
    // b = S2::InterpolateAtDistance(S1Angle::Radians(1e-5), a, b);
    // Uncomment to only test edges that stay in one hemisphere.
    // if (a[2] * b[2] <= 0) continue;
    R2Point pa = proj.Project(a);
    R2Point pb = proj.WrapDestination(pa, proj.Project(b));
    S1Angle max_dist_g = GetMaxDistance(proj, pa, a, pb, b,
                                        DistType::GEOMETRIC);
    // Ignore edges where the error is too small.
    if (max_dist_g <= S2EdgeTessellator::kMinTolerance()) continue;
    S1Angle max_dist_p = GetMaxDistance(proj, pa, a, pb, b,
                                        DistType::PARAMETRIC);
    if (max_dist_p <= S2EdgeTessellator::kMinTolerance()) continue;

    // Compute the estimated error bound.
    S1Angle d1(S2::Interpolate(a, b, t), proj.Unproject((1 - t) * pa + t * pb));
    S1Angle d2(S2::Interpolate(a, b, 1 - t),
               proj.Unproject(t * pa + (1 - t) * pb));
    S1Angle dist = kScaleFactor * max(S1Angle::Radians(1e-300), max(d1, d2));

    // Compute the ratio of the true geometric/parametric errors to the
    // estimate error bound.
    double r_g = max_dist_g / dist;
    double r_p = max_dist_p / dist;

    // Our objective is to ensure that the geometric error ratio is at most 1.
    // (The parametric ratio is computed only for analysis purposes.)
    if (r_g > 0.99999) {
      // Log any edges where the ratio is exceeded (or nearly so).
      cout << pa << " to " << pb << ": ratio = " << r_g
           << ", dist = " << max_dist_g.degrees() << endl;
    }
    stats_g.Tally(r_g);
    stats_p.Tally(r_p);
  }
  cout << "t = " << t << ", scale = " << kScaleFactor << ", G["
       << stats_g.ToString() << "], P[" << stats_p.ToString() << "]" << endl;
  EXPECT_LE(stats_g.max(), kScaleFactor);
}

// The interpolation parameter actually used in the .cc file.
static constexpr double kBestFraction = 0.31215691082248312;

TEST(S2EdgeTessellator, MaxEdgeErrorPlateCarree) {
  S2::PlateCarreeProjection proj(180);
  // Uncomment to test some nearby parameter values.
  // TestEdgeError(proj, 0.311);
  TestEdgeError(proj, kBestFraction);
  // TestEdgeError(proj, 0.313);
}

TEST(S2EdgeTessellator, MaxEdgeErrorMercator) {
  S2::MercatorProjection proj(180);
  // Uncomment to test some nearby parameter values.
  // TestEdgeError(proj, 0.311);
  TestEdgeError(proj, kBestFraction);
  // TestEdgeError(proj, 0.313);
}

// Tessellates random edges using the given projection and tolerance, and
// verifies that the expected criteria are satisfied.
void TestRandomEdges(const S2::Projection& proj, S1Angle tolerance) {
  const int kIters = google::DEBUG_MODE ? 50 : 500;
  double max_r2 = 0, max_s2 = 0;
  for (int iter = 0; iter < kIters; ++iter) {
    S2Testing::rnd.Reset(iter);
    S2Point a = S2Testing::RandomPoint();
    S2Point b = S2Testing::RandomPoint();
    max_r2 = max(max_r2, TestProjected(proj, tolerance, a, b, false).max());
    R2Point pa = proj.Project(a);
    R2Point pb = proj.Project(b);
    max_s2 = max(max_s2, TestUnprojected(proj, tolerance, pa, pb, false).max());
  }
  cout << "max_r2 = " << max_r2 << ", max_s2 = " << max_s2 << endl;
  EXPECT_LE(max_r2, 1.0);
  EXPECT_LE(max_s2, 1.0);
}

TEST(S2EdgeTessellator, RandomEdgesPlateCarree) {
  S2::PlateCarreeProjection proj(180);
  S1Angle tolerance = S2Testing::MetersToAngle(100);
  TestRandomEdges(proj, tolerance);
}

TEST(S2EdgeTessellator, RandomEdgesMercator) {
  S2::MercatorProjection proj(180);
  S1Angle tolerance = S2Testing::MetersToAngle(100);
  TestRandomEdges(proj, tolerance);
}

// TODO(ericv): Superceded by random edge tests above, remove?
TEST(S2EdgeTessellator, UnprojectedAccuracyRandomCheck) {
  S2::PlateCarreeProjection proj(180);
  S1Angle tolerance(S1Angle::Degrees(1e-3));
  S2Testing::Random rand;
  const int kIters = google::DEBUG_MODE ? 250 : 5000;
  for (int i = 0; i < kIters; ++i) {
    S2Testing::rnd.Reset(i);
    double alat = rand.UniformDouble(-89.99, 89.99);
    double blat = rand.UniformDouble(-89.99, 89.99);
    double blon = rand.UniformDouble(0, 179);

    R2Point pa(0, alat), pb(blon, blat);
    Stats stats = TestUnprojected(proj, tolerance, pa, pb, false);
    EXPECT_LT(stats.max(), 1.0) << pa << ", " << pb;
  }
}

// XXX(ericv): Superceded by random edge tests above, remove?
TEST(S2EdgeTessellator, ProjectedAccuracyRandomCheck) {
  S2::PlateCarreeProjection proj(180);
  S1Angle tolerance(S1Angle::Degrees(1e-3));
  S2Testing::Random rand;
  const int kIters = google::DEBUG_MODE ? 250 : 5000;
  for (int i = 0; i < kIters; ++i) {
    S2Testing::rnd.Reset(i);
    double alat = rand.UniformDouble(-89.99, 89.99);
    double blat = rand.UniformDouble(-89.99, 89.99);
    double blon = rand.UniformDouble(-180, 180);

    S2Point a = S2LatLng::FromDegrees(alat, 0).ToPoint();
    S2Point b = S2LatLng::FromDegrees(blat, blon).ToPoint();
    Stats stats = TestProjected(proj, tolerance, a, b, false);
    EXPECT_LT(stats.max(), 1.0) << a << ", " << b;
  }
}

}  // namespace
