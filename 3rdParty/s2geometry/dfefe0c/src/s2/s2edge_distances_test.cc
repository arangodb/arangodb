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

#include "s2/s2edge_distances.h"

#include <memory>
#include <string>

#include <gtest/gtest.h>
#include "s2/s1chord_angle.h"
#include "s2/s2pointutil.h"
#include "s2/s2polyline.h"
#include "s2/s2predicates.h"
#include "s2/s2testing.h"
#include "s2/s2text_format.h"

using std::unique_ptr;

// Checks that the error returned by S2::GetUpdateMinDistanceMaxError() for
// the distance "input" (measured in radians) corresponds to a distance error
// of less than "max_error" (measured in radians).
//
// The reason for the awkward phraseology above is that the value returned by
// GetUpdateMinDistanceMaxError() is not a distance; it represents an error in
// the *squared* distance.
void CheckUpdateMinDistanceMaxError(double actual, double max_error) {
  S1ChordAngle ca(S1Angle::Radians(actual));
  S1Angle bound = ca.PlusError(S2::GetUpdateMinDistanceMaxError(ca)).ToAngle();
  EXPECT_LE(bound.radians() - actual,  max_error) << actual;
}

TEST(S2, GetUpdateMinDistanceMaxError) {
  // Verify that the error is "reasonable" for a sampling of distances.
  CheckUpdateMinDistanceMaxError(0, 1.5e-15);
  CheckUpdateMinDistanceMaxError(1e-8, 1e-15);
  CheckUpdateMinDistanceMaxError(1e-5, 1e-15);
  CheckUpdateMinDistanceMaxError(0.05, 1e-15);
  CheckUpdateMinDistanceMaxError(M_PI_2 - 1e-8, 2e-15);
  CheckUpdateMinDistanceMaxError(M_PI_2, 2e-15);
  CheckUpdateMinDistanceMaxError(M_PI_2 + 1e-8, 2e-15);
  CheckUpdateMinDistanceMaxError(M_PI - 1e-5, 2e-10);
  CheckUpdateMinDistanceMaxError(M_PI, 0);
}

TEST(S2, GetUpdateMinInteriorDistanceMaxError) {
  // Check that the error bound returned by
  // GetUpdateMinInteriorDistanceMaxError() is large enough.
  auto& rnd = S2Testing::rnd;
  for (int iter = 0; iter < 10000; ++iter) {
    S2Point a0 = S2Testing::RandomPoint();
    S1Angle len = S1Angle::Radians(M_PI * pow(1e-20, rnd.RandDouble()));
    S2Point a1 = S2::InterpolateAtDistance(len, a0, S2Testing::RandomPoint());
    // TODO(ericv): If s2pred::RobustCrossProd() is implemented, then we can
    // also test nearly-antipodal points here.  In theory the error bound can
    // be exceeded when the edge endpoints are antipodal to within 0.8e-13
    // radians, but the only examples found in testing require the endpoints
    // to be nearly-antipodal to within 1e-16 radians.
    S2Point n = S2::RobustCrossProd(a0, a1).Normalize();
    double f = pow(1e-20, rnd.RandDouble());
    S2Point a = ((1 - f) * a0 + f * a1).Normalize();
    S1Angle r = S1Angle::Radians(M_PI_2 * pow(1e-20, rnd.RandDouble()));
    if (rnd.OneIn(2)) r = S1Angle::Radians(M_PI_2) - r;
    S2Point x = S2::InterpolateAtDistance(r, a, n);
    S1ChordAngle min_dist = S1ChordAngle::Infinity();
    if (!S2::UpdateMinInteriorDistance(x, a0, a1, &min_dist)) {
      --iter; continue;
    }
    double error = S2::GetUpdateMinDistanceMaxError(min_dist);
    EXPECT_LE(s2pred::CompareEdgeDistance(x, a0, a1,
                                          min_dist.PlusError(error)), 0);
    EXPECT_GE(s2pred::CompareEdgeDistance(x, a0, a1,
                                          min_dist.PlusError(-error)), 0);
  }
}

// Given a point X and an edge AB, check that the distance from X to AB is
// "distance_radians" and the closest point on AB is "expected_closest".
void CheckDistance(S2Point x, S2Point a, S2Point b,
                   double distance_radians, S2Point expected_closest) {
  x = x.Normalize();
  a = a.Normalize();
  b = b.Normalize();
  expected_closest = expected_closest.Normalize();
  EXPECT_NEAR(distance_radians, S2::GetDistance(x, a, b).radians(), 1e-15);
  S2Point closest = S2::Project(x, a, b);
  if (expected_closest == S2Point(0, 0, 0)) {
    // This special value says that the result should be A or B.
    EXPECT_TRUE(closest == a || closest == b);
  } else {
    EXPECT_TRUE(S2::ApproxEquals(closest, expected_closest));
  }
  S1ChordAngle min_distance = S1ChordAngle::Zero();
  EXPECT_FALSE(S2::UpdateMinDistance(x, a, b, &min_distance));
  min_distance = S1ChordAngle::Infinity();
  EXPECT_TRUE(S2::UpdateMinDistance(x, a, b, &min_distance));
  EXPECT_NEAR(distance_radians, min_distance.ToAngle().radians(), 1e-15);
}

TEST(S2, Distance) {
  CheckDistance(S2Point(1, 0, 0), S2Point(1, 0, 0), S2Point(0, 1, 0),
                0, S2Point(1, 0, 0));
  CheckDistance(S2Point(0, 1, 0), S2Point(1, 0, 0), S2Point(0, 1, 0),
                0, S2Point(0, 1, 0));
  CheckDistance(S2Point(1, 3, 0), S2Point(1, 0, 0), S2Point(0, 1, 0),
                0, S2Point(1, 3, 0));
  CheckDistance(S2Point(0, 0, 1), S2Point(1, 0, 0), S2Point(0, 1, 0),
                M_PI_2, S2Point(1, 0, 0));
  CheckDistance(S2Point(0, 0, -1), S2Point(1, 0, 0), S2Point(0, 1, 0),
                M_PI_2, S2Point(1, 0, 0));
  CheckDistance(S2Point(-1, -1, 0), S2Point(1, 0, 0), S2Point(0, 1, 0),
                0.75 * M_PI, S2Point(0, 0, 0));

  CheckDistance(S2Point(0, 1, 0), S2Point(1, 0, 0), S2Point(1, 1, 0),
                M_PI_4, S2Point(1, 1, 0));
  CheckDistance(S2Point(0, -1, 0), S2Point(1, 0, 0), S2Point(1, 1, 0),
                M_PI_2, S2Point(1, 0, 0));

  CheckDistance(S2Point(0, -1, 0), S2Point(1, 0, 0), S2Point(-1, 1, 0),
                M_PI_2, S2Point(1, 0, 0));
  CheckDistance(S2Point(-1, -1, 0), S2Point(1, 0, 0), S2Point(-1, 1, 0),
                M_PI_2, S2Point(-1, 1, 0));

  CheckDistance(S2Point(1, 1, 1), S2Point(1, 0, 0), S2Point(0, 1, 0),
                asin(sqrt(1./3)), S2Point(1, 1, 0));
  CheckDistance(S2Point(1, 1, -1), S2Point(1, 0, 0), S2Point(0, 1, 0),
                asin(sqrt(1./3)), S2Point(1, 1, 0));

  CheckDistance(S2Point(-1, 0, 0), S2Point(1, 1, 0), S2Point(1, 1, 0),
                0.75 * M_PI, S2Point(1, 1, 0));
  CheckDistance(S2Point(0, 0, -1), S2Point(1, 1, 0), S2Point(1, 1, 0),
                M_PI_2, S2Point(1, 1, 0));
  CheckDistance(S2Point(-1, 0, 0), S2Point(1, 0, 0), S2Point(1, 0, 0),
                M_PI, S2Point(1, 0, 0));
}

TEST(S2, DistanceOptimizationIsConservative) {
  // Verifies that AlwaysUpdateMinInteriorDistance() computes the lower bound
  // on the true distance conservatively.  (This test used to fail.)
  S2Point x(-0.017952729194524016, -0.30232422079175203, 0.95303607751077712);
  S2Point a(-0.017894725505830295, -0.30229974986194175, 0.95304493075220664);
  S2Point b(-0.017986591360900289, -0.30233851195954353, 0.95303090543659963);
  S1ChordAngle min_distance = S1ChordAngle::Infinity();
  EXPECT_TRUE(S2::UpdateMinDistance(x, a, b, &min_distance));
  min_distance = min_distance.Successor();
  EXPECT_TRUE(S2::UpdateMinDistance(x, a, b, &min_distance));
}

void CheckMaxDistance(S2Point x, S2Point a, S2Point b,
                      double distance_radians) {
  x = x.Normalize();
  a = a.Normalize();
  b = b.Normalize();

  S1ChordAngle max_distance = S1ChordAngle::Straight();
  EXPECT_FALSE(S2::UpdateMaxDistance(x, a, b, &max_distance));
  max_distance = S1ChordAngle::Negative();
  EXPECT_TRUE(S2::UpdateMaxDistance(x, a, b, &max_distance));

  EXPECT_NEAR(distance_radians, max_distance.radians(), 1e-15);
}

TEST(S2, MaxDistance) {
  CheckMaxDistance(S2Point(1, 0, 1), S2Point(1, 0, 0), S2Point(0, 1, 0),
                   M_PI_2);
  CheckMaxDistance(S2Point(1, 0, -1), S2Point(1, 0, 0), S2Point(0, 1, 0),
                   M_PI_2);
  CheckMaxDistance(S2Point(0, 1, 1), S2Point(1, 0, 0), S2Point(0, 1, 0),
                   M_PI_2);
  CheckMaxDistance(S2Point(0, 1, -1), S2Point(1, 0, 0), S2Point(0, 1, 0),
                   M_PI_2);

  CheckMaxDistance(S2Point(1, 1, 1), S2Point(1, 0, 0), S2Point(0, 1, 0),
                   asin(sqrt(2./3)));
  CheckMaxDistance(S2Point(1, 1, -1), S2Point(1, 0, 0), S2Point(0, 1, 0),
                   asin(sqrt(2./3)));

  CheckMaxDistance(S2Point(1, 0, 0), S2Point(1, 1, 0), S2Point(1, -1, 0),
                   M_PI_4);
  CheckMaxDistance(S2Point(0, 1, 0), S2Point(1, 1, 0), S2Point(-1, 1, 0),
                   M_PI_4);
  CheckMaxDistance(S2Point(0, 0, 1), S2Point(0, 1, 1), S2Point(0, -1, 1),
                   M_PI_4);

  CheckMaxDistance(S2Point(0, 0, 1), S2Point(1, 0, 0), S2Point(1, 0, -1),
                   3 * M_PI_4);
  CheckMaxDistance(S2Point(0, 0, 1), S2Point(1, 0, 0), S2Point(1, 1, -M_SQRT2),
                   3 * M_PI_4);

  CheckMaxDistance(S2Point(0, 0, 1), S2Point(0, 0, -1), S2Point(0, 0, -1),
                   M_PI);
}

void CheckInterpolate(double t, S2Point a, S2Point b, S2Point expected) {
  a = a.Normalize();
  b = b.Normalize();
  expected = expected.Normalize();
  S2Point actual = S2::Interpolate(t, a, b);

  // We allow a bit more than the usual 1e-15 error tolerance because
  // Interpolate() uses trig functions.
  EXPECT_TRUE(S2::ApproxEquals(expected, actual, S1Angle::Radians(3e-15)))
      << "Expected: " << expected << ", actual: " << actual;
}

TEST(S2, Interpolate) {
  // Choose test points designed to expose floating-point errors.
  S2Point p1 = S2Point(0.1, 1e-30, 0.3).Normalize();
  S2Point p2 = S2Point(-0.7, -0.55, -1e30).Normalize();

  // A zero-length edge.
  CheckInterpolate(0, p1, p1, p1);
  CheckInterpolate(1, p1, p1, p1);

  // Start, end, and middle of a medium-length edge.
  CheckInterpolate(0, p1, p2, p1);
  CheckInterpolate(1, p1, p2, p2);
  CheckInterpolate(0.5, p1, p2, 0.5 * (p1 + p2));

  // Test that interpolation is done using distances on the sphere rather than
  // linear distances.
  CheckInterpolate(1./3, S2Point(1, 0, 0), S2Point(0, 1, 0),
                   S2Point(sqrt(3), 1, 0));
  CheckInterpolate(2./3, S2Point(1, 0, 0), S2Point(0, 1, 0),
                   S2Point(1, sqrt(3), 0));

  // Test that interpolation is accurate on a long edge (but not so long that
  // the definition of the edge itself becomes too unstable).
  {
    const double kLng = M_PI - 1e-2;
    S2Point a = S2LatLng::FromRadians(0, 0).ToPoint();
    S2Point b = S2LatLng::FromRadians(0, kLng).ToPoint();
    for (double f = 0.4; f > 1e-15; f *= 0.1) {
      CheckInterpolate(f, a, b,
                       S2LatLng::FromRadians(0, f * kLng).ToPoint());
      CheckInterpolate(1 - f, a, b,
                       S2LatLng::FromRadians(0, (1 - f) * kLng).ToPoint());
    }
  }

  // Test that interpolation on a 180 degree edge (antipodal endpoints) yields
  // a result with the correct distance from each endpoint.
  for (double t = 0; t <= 1; t += 0.125) {
    S2Point actual = S2::Interpolate(t, p1, -p1);
    EXPECT_NEAR(S1Angle(actual, p1).radians(), t * M_PI, 3e-15);
  }
}

TEST(S2, InterpolateCanExtrapolate) {
  const S2Point i(1, 0, 0);
  const S2Point j(0, 1, 0);
  // Initial vectors at 90 degrees.
  CheckInterpolate(0, i, j, S2Point(1, 0, 0));
  CheckInterpolate(1, i, j, S2Point(0, 1, 0));
  CheckInterpolate(1.5, i, j, S2Point(-1, 1, 0));
  CheckInterpolate(2, i, j, S2Point(-1, 0, 0));
  CheckInterpolate(3, i, j, S2Point(0, -1, 0));
  CheckInterpolate(4, i, j, S2Point(1, 0, 0));

  // Negative values of t.
  CheckInterpolate(-1, i, j, S2Point(0, -1, 0));
  CheckInterpolate(-2, i, j, S2Point(-1, 0, 0));
  CheckInterpolate(-3, i, j, S2Point(0, 1, 0));
  CheckInterpolate(-4, i, j, S2Point(1, 0, 0));

  // Initial vectors at 45 degrees.
  CheckInterpolate(2, i, S2Point(1, 1, 0), S2Point(0, 1, 0));
  CheckInterpolate(3, i, S2Point(1, 1, 0), S2Point(-1, 1, 0));
  CheckInterpolate(4, i, S2Point(1, 1, 0), S2Point(-1, 0, 0));

  // Initial vectors at 135 degrees.
  CheckInterpolate(2, i, S2Point(-1, 1, 0), S2Point(0, -1, 0));

  // Take a small fraction along the curve.
  S2Point p(S2::Interpolate(0.001, i, j));
  // We should get back where we started.
  CheckInterpolate(1000, i, p, j);
}


TEST(S2, RepeatedInterpolation) {
  // Check that points do not drift away from unit length when repeated
  // interpolations are done.
  for (int i = 0; i < 100; ++i) {
    S2Point a = S2Testing::RandomPoint();
    S2Point b = S2Testing::RandomPoint();
    for (int j = 0; j < 1000; ++j) {
      a = S2::Interpolate(0.01, a, b);
    }
    EXPECT_TRUE(S2::IsUnitLength(a));
  }
}

// Given two edges a0a1 and b0b1, check that the minimum distance between them
// is "distance_radians", and that GetEdgePairClosestPoints() returns
// "expected_a" and "expected_b" as the points that achieve this distance.
// S2Point(0, 0, 0) may be passed for "expected_a" or "expected_b" to indicate
// that both endpoints of the corresponding edge are equally distant, and
// therefore either one might be returned.
//
// Parameters are passed by value so that this function can normalize them.
void CheckEdgePairMinDistance(S2Point a0, S2Point a1, S2Point b0, S2Point b1,
                              double distance_radians,
                              S2Point expected_a, S2Point expected_b) {
  a0 = a0.Normalize();
  a1 = a1.Normalize();
  b0 = b0.Normalize();
  b1 = b1.Normalize();
  expected_a = expected_a.Normalize();
  expected_b = expected_b.Normalize();
  const auto& closest = S2::GetEdgePairClosestPoints(a0, a1, b0, b1);
  const S2Point& actual_a = closest.first;
  const S2Point& actual_b = closest.second;
  if (expected_a == S2Point(0, 0, 0)) {
    // This special value says that the result should be a0 or a1.
    EXPECT_TRUE(actual_a == a0 || actual_a == a1);
  } else {
    EXPECT_TRUE(S2::ApproxEquals(expected_a, actual_a));
  }
  if (expected_b == S2Point(0, 0, 0)) {
    // This special value says that the result should be b0 or b1.
    EXPECT_TRUE(actual_b == b0 || actual_b == b1);
  } else {
    EXPECT_TRUE(S2::ApproxEquals(expected_b, actual_b));
  }
  S1ChordAngle min_distance = S1ChordAngle::Zero();
  EXPECT_FALSE(S2::UpdateEdgePairMinDistance(a0, a1, b0, b1, &min_distance));
  min_distance = S1ChordAngle::Infinity();
  EXPECT_TRUE(S2::UpdateEdgePairMinDistance(a0, a1, b0, b1, &min_distance));
  EXPECT_NEAR(distance_radians, min_distance.radians(), 1e-15);
}

TEST(S2, EdgePairMinDistance) {
  // One edge is degenerate.
  CheckEdgePairMinDistance(S2Point(1, 0, 1), S2Point(1, 0, 1),
                           S2Point(1, -1, 0), S2Point(1, 1, 0),
                           M_PI_4, S2Point(1, 0, 1), S2Point(1, 0, 0));
  CheckEdgePairMinDistance(S2Point(1, -1, 0), S2Point(1, 1, 0),
                           S2Point(1, 0, 1), S2Point(1, 0, 1),
                           M_PI_4, S2Point(1, 0, 0), S2Point(1, 0, 1));

  // Both edges are degenerate.
  CheckEdgePairMinDistance(S2Point(1, 0, 0), S2Point(1, 0, 0),
                           S2Point(0, 1, 0), S2Point(0, 1, 0),
                           M_PI_2, S2Point(1, 0, 0), S2Point(0, 1, 0));

  // Both edges are degenerate and antipodal.
  CheckEdgePairMinDistance(S2Point(1, 0, 0), S2Point(1, 0, 0),
                           S2Point(-1, 0, 0), S2Point(-1, 0, 0),
                           M_PI, S2Point(1, 0, 0), S2Point(-1, 0, 0));

  // Two identical edges.
  CheckEdgePairMinDistance(S2Point(1, 0, 0), S2Point(0, 1, 0),
                           S2Point(1, 0, 0), S2Point(0, 1, 0),
                           0, S2Point(0, 0, 0), S2Point(0, 0, 0));

  // Both edges are degenerate and identical.
  CheckEdgePairMinDistance(S2Point(1, 0, 0), S2Point(1, 0, 0),
                           S2Point(1, 0, 0), S2Point(1, 0, 0),
                           0, S2Point(1, 0, 0), S2Point(1, 0, 0));

  // Edges that share exactly one vertex (all 4 possibilities).
  CheckEdgePairMinDistance(S2Point(1, 0, 0), S2Point(0, 1, 0),
                           S2Point(0, 1, 0), S2Point(0, 1, 1),
                           0, S2Point(0, 1, 0), S2Point(0, 1, 0));
  CheckEdgePairMinDistance(S2Point(0, 1, 0), S2Point(1, 0, 0),
                           S2Point(0, 1, 0), S2Point(0, 1, 1),
                           0, S2Point(0, 1, 0), S2Point(0, 1, 0));
  CheckEdgePairMinDistance(S2Point(1, 0, 0), S2Point(0, 1, 0),
                           S2Point(0, 1, 1), S2Point(0, 1, 0),
                           0, S2Point(0, 1, 0), S2Point(0, 1, 0));
  CheckEdgePairMinDistance(S2Point(0, 1, 0), S2Point(1, 0, 0),
                           S2Point(0, 1, 1), S2Point(0, 1, 0),
                           0, S2Point(0, 1, 0), S2Point(0, 1, 0));

  // Two edges whose interiors cross.
  CheckEdgePairMinDistance(S2Point(1, -1, 0), S2Point(1, 1, 0),
                           S2Point(1, 0, -1), S2Point(1, 0, 1),
                           0, S2Point(1, 0, 0), S2Point(1, 0, 0));

  // The closest distance occurs between two edge endpoints, but more than one
  // endpoint pair is equally distant.
  CheckEdgePairMinDistance(S2Point(1, -1, 0), S2Point(1, 1, 0),
                           S2Point(-1, 0, 0), S2Point(-1, 0, 1),
                           acos(-0.5), S2Point(0, 0, 0), S2Point(-1, 0, 1));
  CheckEdgePairMinDistance(S2Point(-1, 0, 0), S2Point(-1, 0, 1),
                           S2Point(1, -1, 0), S2Point(1, 1, 0),
                           acos(-0.5), S2Point(-1, 0, 1), S2Point(0, 0, 0));
  CheckEdgePairMinDistance(S2Point(1, -1, 0), S2Point(1, 1, 0),
                           S2Point(-1, 0, -1), S2Point(-1, 0, 1),
                           acos(-0.5), S2Point(0, 0, 0), S2Point(0, 0, 0));
}

// Given two edges a0a1 and b0b1, check that the maximum distance between them
// is "distance_radians".  Parameters are passed by value so that this
// function can normalize them.
void CheckEdgePairMaxDistance(S2Point a0, S2Point a1, S2Point b0, S2Point b1,
                              double distance_radians) {
  a0 = a0.Normalize();
  a1 = a1.Normalize();
  b0 = b0.Normalize();
  b1 = b1.Normalize();

  S1ChordAngle max_distance = S1ChordAngle::Straight();
  EXPECT_FALSE(S2::UpdateEdgePairMaxDistance(a0, a1, b0, b1, &max_distance));
  max_distance = S1ChordAngle::Negative();
  EXPECT_TRUE(S2::UpdateEdgePairMaxDistance(a0, a1, b0, b1, &max_distance));
  EXPECT_NEAR(distance_radians, max_distance.radians(), 1e-15);
}

TEST(S2, EdgePairMaxDistance) {
  // Standard situation.  Same hemisphere, not degenerate.
  CheckEdgePairMaxDistance(S2Point(1, 0, 0), S2Point(0, 1, 0),
                           S2Point(1, 1, 0), S2Point(1, 1, 1),
                           acos(1/sqrt(3)));

  // One edge is degenerate.
  CheckEdgePairMaxDistance(S2Point(1, 0, 1), S2Point(1, 0, 1),
                           S2Point(1, -1, 0), S2Point(1, 1, 0),
                           acos(0.5));
  CheckEdgePairMaxDistance(S2Point(1, -1, 0), S2Point(1, 1, 0),
                           S2Point(1, 0, 1), S2Point(1, 0, 1),
                           acos(0.5));

  // Both edges are degenerate.
  CheckEdgePairMaxDistance(S2Point(1, 0, 0), S2Point(1, 0, 0),
                           S2Point(0, 1, 0), S2Point(0, 1, 0),
                           M_PI_2);

  // Both edges are degenerate and antipodal.
  CheckEdgePairMaxDistance(S2Point(1, 0, 0), S2Point(1, 0, 0),
                           S2Point(-1, 0, 0), S2Point(-1, 0, 0),
                           M_PI);

  // Two identical edges.
  CheckEdgePairMaxDistance(S2Point(1, 0, 0), S2Point(0, 1, 0),
                           S2Point(1, 0, 0), S2Point(0, 1, 0),
                           M_PI_2);

  // Both edges are degenerate and identical.
  CheckEdgePairMaxDistance(S2Point(1, 0, 0), S2Point(1, 0, 0),
                           S2Point(1, 0, 0), S2Point(1, 0, 0),
                           0);

  // Antipodal reflection of one edge crosses the other edge.
  CheckEdgePairMaxDistance(S2Point(1, 0, 1), S2Point(1, 0, -1),
                           S2Point(-1, -1, 0), S2Point(-1, 1, 0),
                           M_PI);

  // One vertex of one edge touches the interior of the antipodal reflection
  // of the other edge.
  CheckEdgePairMaxDistance(S2Point(1, 0, 1), S2Point(1, 0, 0),
                           S2Point(-1, -1, 0), S2Point(-1, 1, 0),
                           M_PI);
}

bool IsEdgeBNearEdgeA(const string& a_str, const string& b_str,
                      double max_error_degrees) {
  unique_ptr<S2Polyline> a(s2textformat::MakePolyline(a_str));
  EXPECT_EQ(2, a->num_vertices());
  unique_ptr<S2Polyline> b(s2textformat::MakePolyline(b_str));
  EXPECT_EQ(2, b->num_vertices());
  return S2::IsEdgeBNearEdgeA(a->vertex(0), a->vertex(1),
                              b->vertex(0), b->vertex(1),
                              S1Angle::Degrees(max_error_degrees));
}

TEST(S2, EdgeBNearEdgeA) {
  // Edge is near itself.
  EXPECT_TRUE(IsEdgeBNearEdgeA("5:5, 10:-5", "5:5, 10:-5", 1e-6));

  // Edge is near its reverse
  EXPECT_TRUE(IsEdgeBNearEdgeA("5:5, 10:-5", "10:-5, 5:5", 1e-6));

  // Short edge is near long edge.
  EXPECT_TRUE(IsEdgeBNearEdgeA("10:0, -10:0", "2:1, -2:1", 1.0));

  // Long edges cannot be near shorter edges.
  EXPECT_FALSE(IsEdgeBNearEdgeA("2:1, -2:1", "10:0, -10:0", 1.0));

  // Orthogonal crossing edges are not near each other...
  EXPECT_FALSE(IsEdgeBNearEdgeA("10:0, -10:0", "0:1.5, 0:-1.5", 1.0));

  // ... unless all points on B are within tolerance of A.
  EXPECT_TRUE(IsEdgeBNearEdgeA("10:0, -10:0", "0:1.5, 0:-1.5", 2.0));

  // Very long edges whose endpoints are close may have interior points that are
  // far apart.  An implementation that only considers the vertices of polylines
  // will incorrectly consider such edges as "close" when they are not.
  // Consider, for example, two consecutive lines of longitude.  As they
  // approach the poles, they become arbitrarily close together, but along the
  // equator they bow apart.
  EXPECT_FALSE(IsEdgeBNearEdgeA("89:1, -89:1", "89:2, -89:2", 0.5));
  EXPECT_TRUE(IsEdgeBNearEdgeA("89:1, -89:1", "89:2, -89:2", 1.5));

  // The two arcs here are nearly as long as S2 edges can be (just shy of 180
  // degrees), and their endpoints are less than 1 degree apart.  Their
  // midpoints, however, are at opposite ends of the sphere along its equator.
  EXPECT_FALSE(IsEdgeBNearEdgeA(
                   "0:-179.75, 0:-0.25", "0:179.75, 0:0.25", 1.0));

  // At the equator, the second arc here is 9.75 degrees from the first, and
  // closer at all other points.  However, the southern point of the second arc
  // (-1, 9.75) is too far from the first arc for the short-circuiting logic in
  // IsEdgeBNearEdgeA to apply.
  EXPECT_TRUE(IsEdgeBNearEdgeA("40:0, -5:0", "39:0.975, -1:0.975", 1.0));

  // Same as above, but B's orientation is reversed, causing the angle between
  // the normal vectors of circ(B) and circ(A) to be (180-9.75) = 170.5 degrees,
  // not 9.75 degrees.  The greatest separation between the planes is still 9.75
  // degrees.
  EXPECT_TRUE(IsEdgeBNearEdgeA("10:0, -10:0", "-.4:0.975, 0.4:0.975", 1.0));

  // A and B are on the same great circle, A and B partially overlap, but the
  // only part of B that does not overlap A is shorter than tolerance.
  EXPECT_TRUE(IsEdgeBNearEdgeA("0:0, 1:0", "0.9:0, 1.1:0", 0.25));

  // A and B are on the same great circle, all points on B are close to A at its
  // second endpoint, (1,0).
  EXPECT_TRUE(IsEdgeBNearEdgeA("0:0, 1:0", "1.1:0, 1.2:0", 0.25));

  // Same as above, but B's orientation is reversed.  This case is special
  // because the projection of the normal defining A onto the plane containing B
  // is the null vector, and must be handled by a special case.
  EXPECT_TRUE(IsEdgeBNearEdgeA("0:0, 1:0", "1.2:0, 1.1:0", 0.25));
}
