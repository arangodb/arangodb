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

#include "s2/s2edge_crossings.h"
#include "s2/s2edge_crossings_internal.h"

#include <vector>

#include "s2/base/logging.h"
#include <gtest/gtest.h>
#include "s2/s2edge_distances.h"
#include "s2/s2predicates.h"
#include "s2/s2testing.h"

// CrossingSign, VertexCrossing, and EdgeOrVertexCrossing are tested in
// s2edge_crosser_test.cc.

using S2::internal::IntersectionMethod;
using std::max;
using std::swap;
using std::vector;

// This class records statistics about the intersection methods used by
// GetIntersection().
class GetIntersectionStats {
 public:
  GetIntersectionStats() {
    S2::internal::intersection_method_tally_ = tally_;
  }

  ~GetIntersectionStats() {
    S2::internal::intersection_method_tally_ = nullptr;
  }

  void Print() {
    int total = 0;
    int totals[kNumMethods];
    for (int i = kNumMethods; --i >= 0; ) {
      total += tally_[i];
      totals[i] = total;
    }
    printf("%10s %16s %16s  %6s\n",
           "Method", "Successes", "Attempts", "Rate");
    for (int i = 0; i < kNumMethods; ++i) {
      if (tally_[i] == 0) continue;
      printf("%10s %9d %5.1f%% %9d %5.1f%%  %5.1f%%\n",
             S2::internal::GetIntersectionMethodName(
                 static_cast<IntersectionMethod>(i)),
             tally_[i], 100.0 * tally_[i] / total,
             totals[i], 100.0 * totals[i] / total,
             100.0 * tally_[i] / totals[i]);
    }
    for (int i = 0; i < kNumMethods; ++i) tally_[i] = 0;
  }

 private:
  static int constexpr kNumMethods =
      static_cast<int>(IntersectionMethod::NUM_METHODS);
  int tally_[kNumMethods] = {0};
};

// This returns the true intersection point of two line segments (a0,a1) and
// (b0,b1), with a relative error of at most DBL_EPSILON in each coordinate
// (i.e., one ulp, or twice the double precision rounding error).
static S2Point GetIntersectionExact(const S2Point& a0, const S2Point& a1,
                                    const S2Point& b0, const S2Point& b1) {
  S2Point x = S2::internal::GetIntersectionExact(a0, a1, b0, b1);
  if (x.DotProd((a0 + a1) + (b0 + b1)) < 0) x = -x;
  return x;
}

// The approximate maximum error in GetDistance() for small distances.
S1Angle kGetDistanceAbsError = S1Angle::Radians(3 * DBL_EPSILON);

TEST(S2EdgeUtil, IntersectionError) {
  // We repeatedly construct two edges that cross near a random point "p", and
  // measure the distance from the actual intersection point "x" to the
  // exact intersection point and also to the edges.

  GetIntersectionStats stats;
  S1Angle max_point_dist, max_edge_dist;
  S2Testing::Random* rnd = &S2Testing::rnd;
  for (int iter = 0; iter < 5000; ++iter) {
    // We construct two edges AB and CD that intersect near "p".  The angle
    // between AB and CD (expressed as a slope) is chosen randomly between
    // 1e-15 and 1e15 such that its logarithm is uniformly distributed.
    // Similarly, two edge lengths approximately between 1e-15 and 1 are
    // chosen.  The edge endpoints are chosen such that they are often very
    // close to the other edge (i.e., barely crossing).  Taken together this
    // ensures that we test both long and very short edges that intersect at
    // both large and very small angles.
    //
    // Sometimes the edges we generate will not actually cross, in which case
    // we simply try again.
    Vector3_d p, d1, d2;
    S2Testing::GetRandomFrame(&p, &d1, &d2);
    double slope = 1e-15 * pow(1e30, rnd->RandDouble());
    d2 = (d1 + slope * d2).Normalize();
    S2Point a, b, c, d;
    do {
      double ab_len = pow(1e-15, rnd->RandDouble());
      double cd_len = pow(1e-15, rnd->RandDouble());
      double a_fraction = pow(1e-5, rnd->RandDouble());
      if (rnd->OneIn(2)) a_fraction = 1 - a_fraction;
      double c_fraction = pow(1e-5, rnd->RandDouble());
      if (rnd->OneIn(2)) c_fraction = 1 - c_fraction;
      a = (p - a_fraction * ab_len * d1).Normalize();
      b = (p + (1 - a_fraction) * ab_len * d1).Normalize();
      c = (p - c_fraction * cd_len * d2).Normalize();
      d = (p + (1 - c_fraction) * cd_len * d2).Normalize();
    } while (S2::CrossingSign(a, b, c, d) <= 0);

    // Each constructed edge should be at most 1.5 * DBL_EPSILON away from the
    // original point P.
    EXPECT_LE(S2::GetDistance(p, a, b),
              S1Angle::Radians(1.5 * DBL_EPSILON) + kGetDistanceAbsError);
    EXPECT_LE(S2::GetDistance(p, c, d),
              S1Angle::Radians(1.5 * DBL_EPSILON) + kGetDistanceAbsError);

    // Verify that the expected intersection point is close to both edges and
    // also close to the original point P.  (It might not be very close to P
    // if the angle between the edges is very small.)
    S2Point expected = GetIntersectionExact(a, b, c, d);
    EXPECT_LE(S2::GetDistance(expected, a, b),
              S1Angle::Radians(3 * DBL_EPSILON) + kGetDistanceAbsError);
    EXPECT_LE(S2::GetDistance(expected, c, d),
              S1Angle::Radians(3 * DBL_EPSILON) + kGetDistanceAbsError);
    EXPECT_LE(S1Angle(expected, p),
              S1Angle::Radians(3 * DBL_EPSILON / slope) +
              S2::kIntersectionError);

    // Now we actually test the GetIntersection() method.
    S2Point actual = S2::GetIntersection(a, b, c, d);
    S1Angle dist_ab = S2::GetDistance(actual, a, b);
    S1Angle dist_cd = S2::GetDistance(actual, c, d);
    EXPECT_LE(dist_ab, S2::kIntersectionError + kGetDistanceAbsError);
    EXPECT_LE(dist_cd, S2::kIntersectionError + kGetDistanceAbsError);
    max_edge_dist = max(max_edge_dist, max(dist_ab, dist_cd));
    S1Angle point_dist(expected, actual);
    EXPECT_LE(point_dist, S2::kIntersectionError);
    max_point_dist = max(max_point_dist, point_dist);
  }
  stats.Print();
  S2_LOG(INFO) << "Max distance to either edge being intersected: "
            << max_edge_dist.radians();
  S2_LOG(INFO) << "Maximum distance to expected intersection point: "
            << max_point_dist.radians();
}

// Chooses a point in the XY plane that is separated from X by at least 1e-15
// (to avoid choosing too many duplicate points) and by at most Pi/2 - 1e-3
// (to avoid nearly-diametric edges, since the test below is not sophisticated
// enough to test such edges).
static S2Point ChooseSemicirclePoint(const S2Point& x, const S2Point& y) {
  S2Testing::Random* rnd = &S2Testing::rnd;
  double sign = (2 * rnd->Uniform(2)) - 1;
  return (x + sign * 1e3 * pow(1e-18, rnd->RandDouble()) * y).Normalize();
}

TEST(S2EdgeUtil, GrazingIntersections) {
  // This test choose 5 points along a great circle (i.e., as collinear as
  // possible), and uses them to construct an edge AB and a triangle CDE such
  // that CD and CE both cross AB.  It then checks that the intersection
  // points returned by GetIntersection() have the correct relative ordering
  // along AB (to within kIntersectionError).
  GetIntersectionStats stats;
  for (int iter = 0; iter < 1000; ++iter) {
    Vector3_d x, y, z;
    S2Testing::GetRandomFrame(&x, &y, &z);
    S2Point a, b, c, d, e, ab;
    do {
      a = ChooseSemicirclePoint(x, y);
      b = ChooseSemicirclePoint(x, y);
      c = ChooseSemicirclePoint(x, y);
      d = ChooseSemicirclePoint(x, y);
      e = ChooseSemicirclePoint(x, y);
      ab = (a - b).CrossProd(a + b);
    } while (ab.Norm() < 50 * DBL_EPSILON ||
             S2::CrossingSign(a, b, c, d) <= 0 ||
             S2::CrossingSign(a, b, c, e) <= 0);
    S2Point xcd = S2::GetIntersection(a, b, c, d);
    S2Point xce = S2::GetIntersection(a, b, c, e);
    // Essentially this says that if CDE and CAB have the same orientation,
    // then CD and CE should intersect along AB in that order.
    ab = ab.Normalize();
    if (S1Angle(xcd, xce) > 2 * S2::kIntersectionError) {
      EXPECT_EQ(s2pred::Sign(c, d, e) == s2pred::Sign(c, a, b),
                s2pred::Sign(ab, xcd, xce) > 0);
    }
  }
  stats.Print();
}

TEST(S2EdgeUtil, ExactIntersectionUnderflow) {
  // Tests that a correct intersection is computed even when two edges are
  // exactly collinear and the normals of both edges underflow in double
  // precision when normalized (see S2PointFromExact function for details).
  S2Point a0(1, 0, 0), a1(1, 2e-300, 0);
  S2Point b0(1, 1e-300, 0), b1(1, 3e-300, 0);
  EXPECT_EQ(S2Point(1, 1e-300, 0), S2::GetIntersection(a0, a1, b0, b1));
}

TEST(S2EdgeUtil, GetIntersectionInvariants) {
  // Test that the result of GetIntersection does not change when the edges
  // are swapped and/or reversed.  The number of iterations is high because it
  // is difficult to generate test cases that show that CompareEdges() is
  // necessary and correct, for example.
  const int kIters = google::DEBUG_MODE ? 5000 : 50000;
  for (int iter = 0; iter < kIters; ++iter) {
    S2Point a, b, c, d;
    do {
      // GetIntersectionStable() sorts the two edges by length, so construct
      // edges (a,b) and (c,d) that cross and have exactly the same length.
      // This can be done by swapping the "x" and "y" coordinates.
      // [Swapping other coordinate pairs doesn't work because it changes the
      // order of addition in Norm2() == (x**2 + y**2) + z**2.]
      a = c = S2Testing::RandomPoint();
      b = d = S2Testing::RandomPoint();
      swap(c[0], c[1]);
      swap(d[0], d[1]);
    } while (S2::CrossingSign(a, b, c, d) <= 0);
    EXPECT_EQ((a - b).Norm2(), (c - d).Norm2());

    // Now verify that GetIntersection returns exactly the same result when
    // the edges are swapped and/or reversed.
    S2Point result = S2::GetIntersection(a, b, c, d);
    if (S2Testing::rnd.OneIn(2)) { swap(a, b); }
    if (S2Testing::rnd.OneIn(2)) { swap(c, d); }
    if (S2Testing::rnd.OneIn(2)) { swap(a, c); swap(b, d); }
    EXPECT_EQ(result, S2::GetIntersection(a, b, c, d));
  }
}
