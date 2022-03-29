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

#include <cfloat>
#include <cmath>
#include <vector>

#include "s2/base/logging.h"
#include <gtest/gtest.h>
#include "s2/base/log_severity.h"
#include "absl/strings/str_format.h"
#include "s2/s2edge_crossings_internal.h"
#include "s2/s2edge_distances.h"
#include "s2/s2point_span.h"
#include "s2/s2predicates.h"
#include "s2/s2predicates_internal.h"
#include "s2/s2testing.h"
#include "s2/util/math/exactfloat/exactfloat.h"

using S2::internal::GetStableCrossProd;
using S2::internal::ExactCrossProd;
using S2::internal::SymbolicCrossProd;
using s2pred::DBL_ERR;
using s2pred::LD_ERR;
using s2pred::ToExact;
using s2pred::ToLD;
using std::string;

using Vector3_ld = s2pred::Vector3_ld;
using Vector3_xf = s2pred::Vector3_xf;

namespace {

enum Precision { DOUBLE, LONG_DOUBLE, EXACT, SYMBOLIC, NUM_PRECISIONS };

const char* kPrecisionNames[] = {
  "double", "long double", "exact", "symbolic"
};

// A helper class that keeps track of how often each precision was used and
// generates a string for logging purposes.
class PrecisionStats {
 public:
  PrecisionStats();
  void Tally(Precision precision) { ++counts_[precision]; }
  string ToString();

 private:
  int counts_[NUM_PRECISIONS];
};

PrecisionStats::PrecisionStats() {
  for (int& count : counts_) count = 0;
}

string PrecisionStats::ToString() {
  string result;
  int total = 0;
  for (int i = 0; i < NUM_PRECISIONS; ++i) {
    absl::StrAppendFormat(&result, "%s=%6d, ", kPrecisionNames[i], counts_[i]);
    total += counts_[i];
  }
  absl::StrAppendFormat(&result, "total=%6d", total);
  return result;
}

// Checks that the RobustCrossProd result at one level of precision is
// consistent with the result at the next higher level of precision.  Also
// verifies that the results satisfy the identities documented in the header
// file, including consistency with S2::Sign.  Returns the minimum precision
// that yielded a non-zero result.
Precision TestRobustCrossProdError(const S2Point& a, const S2Point& b) {
  // The following error bound includes the call to RobustCrossProd(), the
  // call to ExactCrossProd() (which has a small amount of error due to rounding
  // to a representable result), and two calls to Normalize().  Altogether this
  // adds up to 9 * DBL_ERR == 4.5 * DBL_EPSILON.
  //
  // The maximum error observed during testing is less than 1/3 of this value.
  S1ChordAngle kMaxError(S2::kRobustCrossProdError +
                         S2::internal::kExactCrossProdError +
                         S1Angle::Radians(2 * DBL_ERR));

  S2Point result = S2::RobustCrossProd(a, b).Normalize();

  // Test that RobustCrossProd is consistent with S2::Sign.  We don't just check
  // that S2::Sign(a, b, result) == 1 since that would only test whether the two
  // functions agree on the direction of the cross product to within 90 degrees.
  // Instead we check that the sign of RobustCrossProd(a, b).DotProd(c) matches
  // S2::Sign(a, b, c) unless "c" is within kRobustCrossProdError of the plane
  // containing "a" and "b".  We test this by generating 2 pairs of vectors that
  // straddle the plane containing "a" and "b", where the two pairs are spaced
  // 90 degrees apart around the origin.
  Vector3_d offset = S2::kRobustCrossProdError.radians() * result;
  Vector3_d a90 = result.CrossProd(a);
  EXPECT_EQ(s2pred::Sign(a, b, result), 1);
  EXPECT_GT(result.DotProd(a + offset), 0);
  EXPECT_LT(result.DotProd(a - offset), 0);
  EXPECT_GT(result.DotProd(a90 + offset), 0);
  EXPECT_LT(result.DotProd(a90 - offset), 0);

  // The following identities are true unless a, b are linearly dependent.
  bool have_exact = !s2pred::IsZero(ToExact(a).CrossProd(ToExact(b)));
  if (have_exact) {
    EXPECT_EQ(S2::RobustCrossProd(-a, b).Normalize(), -result);
    EXPECT_EQ(S2::RobustCrossProd(a, -b).Normalize(), -result);
  }
  S2Point result_exact;
  if (a == b) {
    // This case isn't handled by ExactCrossProd() because it is supposed to be
    // handled long before we get to that point.
    result_exact = S2::Ortho(a).Normalize();
  } else {
    // Note that ExactCrossProd returns an exact *or* symbolic result.
    result_exact = ExactCrossProd(a, b).Normalize();

    // The following identity is true whenever a != b.
    EXPECT_EQ(S2::RobustCrossProd(b, a).Normalize(), -result);
  }

  Vector3_ld tmp_result_ld;
  bool have_ld = GetStableCrossProd(ToLD(a), ToLD(b), &tmp_result_ld);

  S2Point result_dbl;
  bool have_dbl = GetStableCrossProd(a, b, &result_dbl);
  if (have_dbl) {
    result_dbl = result_dbl.Normalize();
    EXPECT_TRUE(have_ld);
    EXPECT_EQ(result_dbl, result);
    EXPECT_LT(s2pred::CompareDistance(result_dbl, result_exact, kMaxError), 0);
    return DOUBLE;
  } else if (have_ld) {
    S2Point result_ld = Vector3_d::Cast(tmp_result_ld).Normalize();
    EXPECT_TRUE(have_exact);
    EXPECT_EQ(result_ld, result);
    EXPECT_LT(s2pred::CompareDistance(result_ld, result_exact, kMaxError), 0);
    return LONG_DOUBLE;
  } else {
    EXPECT_EQ(result_exact, result);
    return have_exact ? EXACT : SYMBOLIC;
  }
}

void TestRobustCrossProd(const S2Point& a, const S2Point& b,
                         const S2Point& expected_result,
                         Precision expected_prec) {
  EXPECT_EQ(s2pred::Sign(a, b, expected_result), 1);
  EXPECT_EQ(S2::RobustCrossProd(a, b).Normalize(), expected_result);
  EXPECT_EQ(TestRobustCrossProdError(a, b), expected_prec);
}

TEST(S2, RobustCrossProdCoverage) {
  // Note that RobustCrossProd() returns a non-unit length result.  In "double"
  // and "long double" precision the magnitude is twice the usual cross product.
  // In exact precision it is equal to the usual cross product unless the
  // magnitude is so small that calling Normalize() would lose precision, in
  // which case it is scaled up appropriately.  When symbolic perturbations are
  // used, the result magnitude is arbitrary.  For this reason we only check
  // the result after calling Normalize().
  TestRobustCrossProd(S2Point(1, 0, 0), S2Point(0, 1, 0),
                      S2Point(0, 0, 1), DOUBLE);
  TestRobustCrossProd(S2Point(20 * DBL_ERR, 1, 0), S2Point(0, 1, 0),
                      S2Point(0, 0, 1), DOUBLE);
  TestRobustCrossProd(S2Point(16 * DBL_ERR, 1, 0), S2Point(0, 1, 0),
                      S2Point(0, 0, 1), LONG_DOUBLE);

  // The following bounds hold for both 80-bit and "double double" precision.
  TestRobustCrossProd(S2Point(5 * LD_ERR, 1, 0), S2Point(0, 1, 0),
                      S2Point(0, 0, 1), LONG_DOUBLE);
  TestRobustCrossProd(S2Point(4 * LD_ERR, 1, 0), S2Point(0, 1, 0),
                      S2Point(0, 0, 1), EXACT);

  // Test that exact results are scaled up when they would be too small.
  TestRobustCrossProd(S2Point(5e-324, 1, 0), S2Point(0, 1, 0),
                      S2Point(0, 0, 1), EXACT);

  // And even when the exact cross product underflows in double precision.
  TestRobustCrossProd(S2Point(5e-324, 1, 0), S2Point(5e-324, 1 - DBL_ERR, 0),
                      S2Point(0, 0, -1), EXACT);

  // Test symbolic results.
  TestRobustCrossProd(S2Point(1, 0, 0), S2Point(1 + DBL_EPSILON, 0, 0),
                      S2Point(0, 1, 0), SYMBOLIC);
  TestRobustCrossProd(S2Point(0, 1 + DBL_EPSILON, 0), S2Point(0, 1, 0),
                      S2Point(1, 0, 0), SYMBOLIC);
  TestRobustCrossProd(S2Point(0, 0, 1), S2Point(0, 0, -1),
                      S2Point(-1, 0, 0), SYMBOLIC);

  // Finally, test some symbolic perturbation cases that can't happen in
  // practice but that are implemented for completeness.
  EXPECT_EQ(SymbolicCrossProd(S2Point(-1, 0, 0), S2Point(0, 0, 0)),
            S2Point(0, 1, 0));
  EXPECT_EQ(SymbolicCrossProd(S2Point(0, 0, 0), S2Point(0, -1, 0)),
            S2Point(1, 0, 0));
  EXPECT_EQ(SymbolicCrossProd(S2Point(0, 0, 0), S2Point(0, 0, -1)),
            S2Point(-1, 0, 0));
}

TEST(S2, SymbolicCrossProdConsistentWithSign) {
  // Tests that RobustCrossProd() is consistent with s2pred::Sign() when
  // symbolic perturbations are necessary.  This implies that the two vectors
  // A and B must be linearly dependent.  We test all possible orderings of
  // the components of vector A = (x, y, z) and all possible orderings of the
  // two vectors A and B.
  for (double x : {-1, 0, 1}) {
    for (double y : {-1, 0, 1}) {
      for (double z : {-1, 0, 1}) {
        S2Point a = S2Point(x, y, z).Normalize();
        if (a == S2Point()) continue;
        for (double scale : {-1.0, 1 - DBL_ERR, 1 + 2 * DBL_ERR}) {
          S2Point b = scale * a;
          S2_DCHECK(S2::IsUnitLength(b));
          EXPECT_GT(s2pred::Sign(a, b, S2::RobustCrossProd(a, b).Normalize()),
                    0);
        }
      }
    }
  }
}

TEST(S2, RobustCrossProdMagnitude) {
  // Test that angles can be measured between vectors returned by
  // RobustCrossProd without loss of precision due to underflow.  The following
  // magnitude (1e-100) is demonstrates that it is not sufficient to simply
  // ensure that the result of RobustCrossProd() is normalizable, since in this
  // case Normalize() could be called on the result of both cross products
  // without precision loss but it is still not possible to measure the angle
  // between them without scaling the result.
  EXPECT_EQ(S2::RobustCrossProd(S2Point(1, 0, 0),
                                S2Point(1, 1e-100, 0)).
            Angle(S2::RobustCrossProd(S2Point(1, 0, 0),
                                      S2Point(1, 0, 1e-100))),
            M_PI_2);

  // Verify that this is true even when symbolic perturbations are necessary.
  EXPECT_EQ(S2::RobustCrossProd(S2Point(-1e-100, 0, 1),
                                S2Point(1e-100, 0, -1)).
            Angle(S2::RobustCrossProd(S2Point(0, -1e-100, 1),
                                      S2Point(0, 1e-100, -1))),
            M_PI_2);
}

// Chooses a random S2Point that is often near the intersection of one of the
// coodinates planes or coordinate axes with the unit sphere.  (It is possible
// to represent very small perturbations near such points.)
S2Point ChoosePoint() {
  S2Point x = S2Testing::RandomPoint();
  for (int i = 0; i < 3; ++i) {
    if (S2Testing::rnd.OneIn(4)) {  // Denormalized
      x[i] *= pow(2, -1022 - 53 * S2Testing::rnd.RandDouble());
    } else if (S2Testing::rnd.OneIn(3)) {  // Zero when squared
      x[i] *= pow(2, -511 - 511 * S2Testing::rnd.RandDouble());
    } else if (S2Testing::rnd.OneIn(2)) {  // Simply small
      x[i] *= pow(2, -100 * S2Testing::rnd.RandDouble());
    }
  }
  if (x.Norm2() >= ldexp(1, -968)) {
    return x.Normalize();
  }
  return ChoosePoint();
}

// Perturbs the length of the given point slightly while ensuring that it still
// satisfies the conditions of S2::IsUnitLength().
S2Point PerturbLength(const S2Point& p) {
  S2Point q = p * S2Testing::rnd.UniformDouble(1 - 2 * DBL_EPSILON,
                                               1 + 2 * DBL_EPSILON);
  // S2::IsUnitLength() is not an exact test, since it tests the length using
  // ordinary double-precision arithmetic and allows for errors in its own
  // calculations.  This is fine for most purposes, but since here we are
  // explicitly trying to push the limits we instead use exact arithmetic to
  // test whether the point is within the error tolerance guaranteed by
  // Normalize().
  if (fabs(ToExact(q).Norm2() - 1) <= 4 * DBL_EPSILON) return q;
  return p;
}

TEST(S2, RobustCrossProdError) {
  // We repeatedly choose two points (usually linearly dependent, or almost so)
  // and measure the distance between the cross product returned by
  // RobustCrossProd and one returned by ExactCrossProd.
  PrecisionStats stats;
  auto& rnd = S2Testing::rnd;
  for (int iter = 0; iter < 5000; ++iter) {
    rnd.Reset(iter + 1);  // Easier to reproduce a specific case.
    S2Point a, b;
    do {
      a = PerturbLength(ChoosePoint());
      S2Point dir = ChoosePoint();
      S1Angle r = S1Angle::Radians(M_PI_2 * pow(2, -53 * rnd.RandDouble()));
      // Occasionally perturb the point by a tiny distance.
      if (rnd.OneIn(3)) r *= pow(2, -1022 * rnd.RandDouble());
      b = PerturbLength(S2::GetPointOnLine(a, dir, r));
      if (rnd.OneIn(2)) b = -b;
    } while (a == b);
    auto prec = TestRobustCrossProdError(a, b);
    stats.Tally(prec);
  }
  // These stats are very heavily skewed towards degenerate cases.
  S2_LOG(ERROR) << stats.ToString();
}

TEST(S2, AngleContainsVertex) {
  S2Point a(1, 0, 0), b(0, 1, 0);
  S2Point ref_b = S2::RefDir(b);

  // Degenerate angle ABA.
  EXPECT_FALSE(S2::AngleContainsVertex(a, b, a));

  // An angle where A == RefDir(B).
  EXPECT_TRUE(S2::AngleContainsVertex(ref_b, b, a));

  // An angle where C == RefDir(B).
  EXPECT_FALSE(S2::AngleContainsVertex(a, b, ref_b));

  // Verify that when a set of polygons tile the region around the vertex,
  // exactly one of those polygons contains the vertex.
  auto points = S2Testing::MakeRegularPoints(b, S1Angle::Degrees(10), 10);
  S2PointLoopSpan loop(points);
  int count = 0;
  for (int i = 0; i < loop.size(); ++i) {
    count += S2::AngleContainsVertex(loop[i+1], b, loop[i]);
  }
  EXPECT_EQ(count, 1);
}

// CrossingSign, VertexCrossing, SignedVertexCrossing, and EdgeOrVertexCrossing
// are tested in s2edge_crosser_test.cc.

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
S2Point GetIntersectionExact(const S2Point& a0, const S2Point& a1,
                             const S2Point& b0, const S2Point& b1) {
  S2Point x = S2::internal::GetIntersectionExact(a0, a1, b0, b1);
  if (x.DotProd((a0 + a1) + (b0 + b1)) < 0) x = -x;
  return x;
}

// The approximate maximum error in GetDistance() for small distances.
S1Angle kGetDistanceAbsError = S1Angle::Radians(3 * DBL_EPSILON);

TEST(S2, IntersectionError) {
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
S2Point ChooseSemicirclePoint(const S2Point& x, const S2Point& y) {
  S2Testing::Random* rnd = &S2Testing::rnd;
  double sign = (2 * rnd->Uniform(2)) - 1;
  return (x + sign * 1e3 * pow(1e-18, rnd->RandDouble()) * y).Normalize();
}

TEST(S2, GrazingIntersections) {
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

TEST(S2, ExactIntersectionUnderflow) {
  // Tests that a correct intersection is computed even when two edges are
  // exactly collinear and the normals of both edges underflow in double
  // precision when normalized (see the ToVector3 function for details).
  S2Point a0(1, 0, 0), a1(1, 2e-300, 0);
  S2Point b0(1, 1e-300, 0), b1(1, 3e-300, 0);
  EXPECT_EQ(S2Point(1, 1e-300, 0), S2::GetIntersection(a0, a1, b0, b1));
}

TEST(S2, ExactIntersectionSign) {
  // Tests that a correct intersection is computed even when two edges are
  // exactly collinear and both edges have nearly antipodal endpoints.
  S2Point a0(-1, -1.6065916409055676e-10, 0);
  S2Point a1(1, 0, 0);
  S2Point b0(1, -4.7617930898495072e-13, 0);
  S2Point b1(-1, 1.2678623820887328e-09, 0);
  EXPECT_EQ(S2Point(1, -4.7617930898495072e-13, 0),
            S2::GetIntersection(a0, a1, b0, b1));
}

TEST(S2, GetIntersectionInvariants) {
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

}  // namespace
