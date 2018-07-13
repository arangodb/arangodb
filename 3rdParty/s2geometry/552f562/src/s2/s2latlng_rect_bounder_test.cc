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

#include "s2/s2latlng_rect_bounder.h"

#include <cfloat>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include "s2/third_party/absl/strings/str_cat.h"
#include "s2/s2edge_distances.h"
#include "s2/s2pointutil.h"
#include "s2/s2testing.h"

using absl::StrCat;

S2LatLngRect GetEdgeBound(const S2Point& a, const S2Point& b) {
  S2LatLngRectBounder bounder;
  bounder.AddPoint(a);
  bounder.AddPoint(b);
  return bounder.GetBound();
}

S2LatLngRect GetEdgeBound(double x1, double y1, double z1,
                          double x2, double y2, double z2) {
  return GetEdgeBound(S2Point(x1, y1, z1).Normalize(),
                      S2Point(x2, y2, z2).Normalize());
}

const S2LatLng kRectError = S2LatLngRectBounder::MaxErrorForTests();

TEST(RectBounder, MaxLatitudeSimple) {
  // Check cases where the min/max latitude is attained at a vertex.
  static const double kCubeLat = asin(1 / sqrt(3));  // 35.26 degrees
  EXPECT_TRUE(GetEdgeBound(1,1,1, 1,-1,-1).ApproxEquals(  // NOLINT
      S2LatLngRect(R1Interval(-kCubeLat, kCubeLat),
                   S1Interval(-M_PI_4, M_PI_4)), kRectError));
  EXPECT_TRUE(GetEdgeBound(1,-1,1, 1,1,-1).ApproxEquals(  // NOLINT
      S2LatLngRect(R1Interval(-kCubeLat, kCubeLat),
                   S1Interval(-M_PI_4, M_PI_4)), kRectError));

  // Check cases where the min/max latitude occurs in the edge interior.
  // These tests expect the result to be pretty close to the middle of the
  // allowable error range (i.e., by adding 0.5 * kRectError).

  // Max latitude, CW edge
  EXPECT_DOUBLE_EQ(M_PI_4 + 0.5 * kRectError.lat().radians(),
                   GetEdgeBound(1,1,1, 1,-1,1).lat().hi());
  // Max latitude, CCW edge
  EXPECT_DOUBLE_EQ(M_PI_4 + 0.5 * kRectError.lat().radians(),
                   GetEdgeBound(1,-1,1, 1,1,1).lat().hi());  // NOLINT
  // Min latitude, CW edge
  EXPECT_DOUBLE_EQ(-M_PI_4 - 0.5 * kRectError.lat().radians(),
                   GetEdgeBound(1,-1,-1, -1,-1,-1).lat().lo());  // NOLINT
  // Min latitude, CCW edge
  EXPECT_DOUBLE_EQ(-M_PI_4 - 0.5 * kRectError.lat().radians(),
                   GetEdgeBound(-1,1,-1, -1,-1,-1).lat().lo());  // NOLINT

  // Check cases where the edge passes through one of the poles.
  EXPECT_EQ(M_PI_2, GetEdgeBound(.3,.4,1, -.3,-.4,1).lat().hi());  // NOLINT
  EXPECT_EQ(-M_PI_2, GetEdgeBound(.3,.4,-1, -.3,-.4,-1).lat().lo());  // NOLINT
}

TEST(RectBounder, MaxLatitudeRandom) {
  // Check that the maximum latitude of edges is computed accurately to within
  // 3 * DBL_EPSILON (the expected maximum error).  We concentrate on maximum
  // latitudes near the equator and north pole since these are the extremes.

  S2Testing::Random* rnd = &S2Testing::rnd;
  const int kIters = 100;
  for (int iter = 0; iter < kIters; ++iter) {
    // Construct a right-handed coordinate frame (U,V,W) such that U points
    // slightly above the equator, V points at the equator, and W is slightly
    // offset from the north pole.
    S2Point u = S2Testing::RandomPoint();
    u[2] = DBL_EPSILON * 1e-6 * pow(1e12, rnd->RandDouble());  // log is uniform
    u = u.Normalize();
    S2Point v = S2::RobustCrossProd(S2Point(0, 0, 1), u).Normalize();
    S2Point w = S2::RobustCrossProd(u, v).Normalize();

    // Construct a line segment AB that passes through U, and check that the
    // maximum latitude of this segment matches the latitude of U.
    S2Point a = (u - rnd->RandDouble() * v).Normalize();
    S2Point b = (u + rnd->RandDouble() * v).Normalize();
    S2LatLngRect ab_bound = GetEdgeBound(a, b);
    EXPECT_NEAR(S2LatLng::Latitude(u).radians(),
                ab_bound.lat().hi(), kRectError.lat().radians());

    // Construct a line segment CD that passes through W, and check that the
    // maximum latitude of this segment matches the latitude of W.
    S2Point c = (w - rnd->RandDouble() * v).Normalize();
    S2Point d = (w + rnd->RandDouble() * v).Normalize();
    S2LatLngRect cd_bound = GetEdgeBound(c, d);
    EXPECT_NEAR(S2LatLng::Latitude(w).radians(),
                cd_bound.lat().hi(), kRectError.lat().radians());
  }
}

S2Point PerturbATowardsB(const S2Point& a, const S2Point& b) {
  S2Testing::Random* rnd = &S2Testing::rnd;
  double choice = rnd->RandDouble();
  if (choice < 0.1) {
    return a;
  }
  if (choice < 0.3) {
    // Return a point that is exactly proportional to A and that still
    // satisfies S2::IsUnitLength().
    for (;;) {
      S2Point b = (2 - a.Norm() + 5*(rnd->RandDouble()-0.5) * DBL_EPSILON) * a;
      if (b != a && S2::IsUnitLength(b))
        return b;
    }
  }
  if (choice < 0.5) {
    // Return a point such that the distance squared to A will underflow.
    return S2::InterpolateAtDistance(S1Angle::Radians(1e-300), a, b);
  }
  // Otherwise return a point whose distance from A is near DBL_EPSILON such
  // that the log of the pdf is uniformly distributed.
  double distance = DBL_EPSILON * 1e-5 * pow(1e6, rnd->RandDouble());
  return S2::InterpolateAtDistance(S1Angle::Radians(distance), a, b);
}

S2Point RandomPole() {
  return S2Point(0, 0, S2Testing::rnd.OneIn(2) ? 1 : -1);
}

S2Point PointNearPole() {
  return PerturbATowardsB(RandomPole(), S2Testing::RandomPoint());
}

S2Point PointNearEquator() {
  return PerturbATowardsB(S2Point(S2Testing::rnd.RandDouble(),
                                  S2Testing::rnd.RandDouble(), 0).Normalize(),
                          RandomPole());
}

TEST(RectBounder, NearlyIdenticalOrAntipodalPoints) {
  // Test pairs of points that are either:
  //  - identical
  //  - nearly or exactly proportional, e.g. (1,0,0) vs. (1+2e-16, 0, 0)
  //  - very close to each other
  // Furthermore we want to test cases where the two points are:
  //  - on a nearly-polar great circle
  //  - on a nearly-equatorial great circle
  //  - near the poles, but on any great circle
  //  - near the equator, but on any great circle
  //  - positioned arbitrarily
  // Also test the corresponding situations for antipodal points, i.e. by
  // negating one of the points so that they are almost 180 degrees apart.

  S2Testing::Random* rnd = &S2Testing::rnd;
  const int kIters = 10000;
  for (int iter = 0; iter < kIters; ++iter) {
    SCOPED_TRACE(StrCat("Iteration ", iter));
    S2Point a, b;
    switch (rnd->Uniform(5)) {
      case 0:
        // Two nearby points on a nearly-polar great circle.
        a = S2Testing::RandomPoint();
        b = PerturbATowardsB(a, PointNearPole());
        break;
      case 1:
        // Two nearby points on a nearly-equatorial great circle.
        a = PointNearEquator();
        b = PerturbATowardsB(a, PointNearEquator());
        break;
      case 2:
        // Two nearby points near a pole, but on any great circle.
        a = PointNearPole();
        b = PerturbATowardsB(a, S2Testing::RandomPoint());
        break;
      case 3:
        // Two nearby points near the equator, but on any great circle.
        a = PointNearEquator();
        b = PerturbATowardsB(a, S2Testing::RandomPoint());
        break;
      case 4:
        // Two nearby points anywhere on the sphere.
        a = S2Testing::RandomPoint();
        b = PerturbATowardsB(a, S2Testing::RandomPoint());
        break;
    }
    // The two points are chosen to be so close to each other that the min/max
    // latitudes are nearly always achieved at the edge endpoints.  The only
    // thing we need to watch out for is that the latitude error bound is
    // slightly larger if the min/max latitude occurs in the edge interior.
    S2LatLngRect expected_bound = S2LatLngRect::FromPointPair(S2LatLng(a),
                                                              S2LatLng(b));
    S2LatLngRect bound = GetEdgeBound(a, b);
    EXPECT_TRUE(bound.Contains(expected_bound));
    EXPECT_TRUE(expected_bound.Expanded(kRectError).PolarClosure().
                Contains(bound));

    // If the two points are close enough and one point is negated (antipodal
    // points), the bound should be the entire sphere.
    if ((a - b).CrossProd(a + b).Norm() <= 6.110 * DBL_EPSILON) {
      EXPECT_EQ(S2LatLngRect::Full(), GetEdgeBound(a, -b));
    }
  }
}

S2LatLngRect GetSubregionBound(double x_lat, double x_lng,
                               double y_lat, double y_lng) {
  S2LatLngRect in = S2LatLngRect::FromPointPair(
      S2LatLng::FromRadians(x_lat, x_lng),
      S2LatLng::FromRadians(y_lat, y_lng));
  S2LatLngRect out = S2LatLngRectBounder::ExpandForSubregions(in);

  // Test that the bound is actually expanded.
  EXPECT_TRUE(out.Contains(in));
  if (in.lat() == S2LatLngRect::FullLat()) {
    EXPECT_FALSE(in.lat().Contains(out.lat()));
  }
  return out;
}


TEST(RectBounder, ExpandForSubregions) {
  // First we check the various situations where the bound contains
  // nearly-antipodal points.  The tests are organized into pairs where the
  // two bounds are similar except that the first bound meets the
  // nearly-antipodal criteria while the second does not.

  // Cases where the bound does not straddle the equator (but almost does),
  // and spans nearly 180 degrees in longitude.
  EXPECT_TRUE(GetSubregionBound(3e-16, 0, 1e-14, M_PI).is_full());
  EXPECT_FALSE(GetSubregionBound(9e-16, 0, 1e-14, M_PI).is_full());
  EXPECT_TRUE(GetSubregionBound(1e-16, 7e-16, 1e-14, M_PI).is_full());
  EXPECT_FALSE(GetSubregionBound(3e-16, 14e-16, 1e-14, M_PI).is_full());
  EXPECT_TRUE(GetSubregionBound(1e-100, 14e-16, 1e-14, M_PI).is_full());
  EXPECT_FALSE(GetSubregionBound(1e-100, 22e-16, 1e-14, M_PI).is_full());

  // Cases where the bound spans at most 90 degrees in longitude, and almost
  // 180 degrees in latitude.  Note that DBL_EPSILON is about 2.22e-16, which
  // implies that the double-precision value just below Pi/2 can be written as
  // (M_PI_2 - 2e-16).
  EXPECT_TRUE(GetSubregionBound(-M_PI_2, -1e-15, M_PI_2 - 7e-16, 0).
              is_full());
  EXPECT_FALSE(GetSubregionBound(-M_PI_2, -1e-15, M_PI_2 - 30e-16, 0).
              is_full());
  EXPECT_TRUE(GetSubregionBound(-M_PI_2 + 4e-16, 0, M_PI_2 - 2e-16, 1e-7).
              is_full());
  EXPECT_FALSE(GetSubregionBound(-M_PI_2 + 30e-16, 0, M_PI_2, 1e-7).
              is_full());
  EXPECT_TRUE(GetSubregionBound(-M_PI_2 + 4e-16, 0, M_PI_2 - 4e-16, M_PI_2).
              is_full());
  EXPECT_FALSE(GetSubregionBound(-M_PI_2, 0, M_PI_2 - 30e-16, M_PI_2).
              is_full());

  // Cases where the bound straddles the equator and spans more than 90
  // degrees in longitude.  These are the cases where the critical distance is
  // between a corner of the bound and the opposite longitudinal edge.  Unlike
  // the cases above, here the bound may contain nearly-antipodal points (to
  // within 3.055 * DBL_EPSILON) even though the latitude and longitude ranges
  // are both significantly less than (Pi - 3.055 * DBL_EPSILON).
  EXPECT_TRUE(GetSubregionBound(-M_PI_2, 0, M_PI_2 - 1e-8, M_PI - 1e-7).
              is_full());
  EXPECT_FALSE(GetSubregionBound(-M_PI_2, 0, M_PI_2 - 1e-7, M_PI - 1e-7).
               is_full());
  EXPECT_TRUE(GetSubregionBound(-M_PI_2 + 1e-12, -M_PI + 1e-4, M_PI_2, 0).
              is_full());
  EXPECT_TRUE(GetSubregionBound(-M_PI_2 + 1e-11, -M_PI + 1e-4, M_PI_2, 0).
               is_full());

  // Now we test cases where the bound does not contain nearly-antipodal
  // points, but it does contain points that are approximately 180 degrees
  // apart in latitude.
  EXPECT_TRUE(GetSubregionBound(1.5, -M_PI_2, 1.5, M_PI_2 - 2e-16).
              ApproxEquals(S2LatLngRect(R1Interval(1.5, 1.5),
                                        S1Interval::Full()), kRectError));
  EXPECT_TRUE(GetSubregionBound(1.5, -M_PI_2, 1.5, M_PI_2 - 7e-16).
              ApproxEquals(S2LatLngRect(R1Interval(1.5, 1.5),
                                        S1Interval(-M_PI_2, M_PI_2 - 7e-16)),
                           kRectError));

  // Test the full and empty bounds.
  EXPECT_TRUE(S2LatLngRectBounder::ExpandForSubregions(
      S2LatLngRect::Full()).is_full());
  EXPECT_TRUE(S2LatLngRectBounder::ExpandForSubregions(
      S2LatLngRect::Empty()).is_empty());

  // Check for cases where the bound is expanded to include one of the poles.
  EXPECT_TRUE(GetSubregionBound(-M_PI_2 + 1e-15, 0, -M_PI_2 + 1e-15, 0).
              ApproxEquals(S2LatLngRect(R1Interval(-M_PI_2, -M_PI_2 + 1e-15),
                                        S1Interval::Full()), kRectError));
  EXPECT_TRUE(GetSubregionBound(M_PI_2 - 1e-15, 0, M_PI_2 - 1e-15, 0).
              ApproxEquals(S2LatLngRect(R1Interval(M_PI_2 - 1e-15, M_PI_2),
                                        S1Interval::Full()), kRectError));
}

