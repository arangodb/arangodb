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

#include "s2/s1interval.h"

#include <cfloat>
#include <cmath>

#include <gtest/gtest.h>

using std::fabs;

class S1IntervalTestBase : public testing::Test {
 public:
  // Create some standard intervals to use in the tests.  These include the
  // empty and full intervals, intervals containing a single point, and
  // intervals spanning one or more "quadrants" which are numbered as follows:
  //    quad1 == [0, Pi/2]
  //    quad2 == [Pi/2, Pi]
  //    quad3 == [-Pi, -Pi/2]
  //    quad4 == [-Pi/2, 0]
  S1IntervalTestBase()
      : empty(S1Interval::Empty()),
        full(S1Interval::Full()),
        // Single-point intervals:
        zero(0, 0),
        pi2(M_PI_2, M_PI_2),
        pi(M_PI, M_PI),
        mipi(-M_PI, -M_PI),  // Same as "pi" after normalization.
        mipi2(-M_PI_2, -M_PI_2),
        // Single quadrants:
        quad1(0, M_PI_2),
        quad2(M_PI_2, -M_PI),
        quad3(M_PI, -M_PI_2),
        quad4(-M_PI_2, 0),
        // Quadrant pairs:
        quad12(0, -M_PI),
        quad23(M_PI_2, -M_PI_2),
        quad34(-M_PI, 0),
        quad41(-M_PI_2, M_PI_2),
        // Quadrant triples:
        quad123(0, -M_PI_2),
        quad234(M_PI_2, 0),
        quad341(M_PI, M_PI_2),
        quad412(-M_PI_2, -M_PI),
        // Small intervals around the midpoints between quadrants, such that
        // the center of each interval is offset slightly CCW from the midpoint.
        mid12(M_PI_2 - 0.01, M_PI_2 + 0.02),
        mid23(M_PI - 0.01, -M_PI + 0.02),
        mid34(-M_PI_2 - 0.01, -M_PI_2 + 0.02),
        mid41(-0.01, 0.02) {
  }

 protected:
  const S1Interval empty, full;
  const S1Interval zero, pi2, pi, mipi, mipi2;
  const S1Interval quad1, quad2, quad3, quad4;
  const S1Interval quad12, quad23, quad34, quad41;
  const S1Interval quad123, quad234, quad341, quad412;
  const S1Interval mid12, mid23, mid34, mid41;
};

TEST_F(S1IntervalTestBase, ConstructorsAndAccessors) {
  // Spot-check the constructors and accessors.
  EXPECT_EQ(quad12.lo(), 0);
  EXPECT_EQ(quad12.hi(), M_PI);
  EXPECT_EQ(quad34[0], M_PI);
  EXPECT_EQ(quad34[1], 0);
  EXPECT_EQ(quad34.bounds(), Vector2_d(M_PI, 0));
  EXPECT_EQ(pi.lo(), M_PI);
  EXPECT_EQ(pi.hi(), M_PI);

  // Check that [-Pi, -Pi] is normalized to [Pi, Pi].
  EXPECT_EQ(mipi.lo(), M_PI);
  EXPECT_EQ(mipi.hi(), M_PI);
  EXPECT_EQ(quad23.lo(), M_PI_2);
  EXPECT_EQ(quad23.hi(), -M_PI_2);

  // Check that the default S1Interval is identical to Empty().
  S1Interval default_empty;
  EXPECT_TRUE(default_empty.is_valid());
  EXPECT_TRUE(default_empty.is_empty());
  EXPECT_EQ(empty.lo(), default_empty.lo());
  EXPECT_EQ(empty.hi(), default_empty.hi());
}

TEST_F(S1IntervalTestBase, SimplePredicates) {
  // is_valid(), is_empty(), is_full(), is_inverted()
  EXPECT_TRUE(zero.is_valid() && !zero.is_empty() && !zero.is_full());
  EXPECT_TRUE(empty.is_valid() && empty.is_empty() && !empty.is_full());
  EXPECT_TRUE(empty.is_inverted());
  EXPECT_TRUE(full.is_valid() && !full.is_empty() && full.is_full());
  EXPECT_TRUE(!quad12.is_empty() && !quad12.is_full() && !quad12.is_inverted());
  EXPECT_TRUE(!quad23.is_empty() && !quad23.is_full() && quad23.is_inverted());
  EXPECT_TRUE(pi.is_valid() && !pi.is_empty() && !pi.is_inverted());
  EXPECT_TRUE(mipi.is_valid() && !mipi.is_empty() && !mipi.is_inverted());
}

TEST_F(S1IntervalTestBase, AlmostEmptyOrFull) {
  // Test that rounding errors don't cause intervals that are almost empty or
  // full to be considered empty or full.  The following value is the greatest
  // representable value less than Pi.
  const double kAlmostPi = M_PI - 2 * DBL_EPSILON;
  EXPECT_FALSE(S1Interval(-kAlmostPi, M_PI).is_full());
  EXPECT_FALSE(S1Interval(-M_PI, kAlmostPi).is_full());
  EXPECT_FALSE(S1Interval(M_PI, -kAlmostPi).is_empty());
  EXPECT_FALSE(S1Interval(kAlmostPi, -M_PI).is_empty());
}

TEST_F(S1IntervalTestBase, GetCenter) {
  EXPECT_EQ(quad12.GetCenter(), M_PI_2);
  EXPECT_DOUBLE_EQ(S1Interval(3.1, 2.9).GetCenter(), 3.0 - M_PI);
  EXPECT_DOUBLE_EQ(S1Interval(-2.9, -3.1).GetCenter(), M_PI - 3.0);
  EXPECT_DOUBLE_EQ(S1Interval(2.1, -2.1).GetCenter(), M_PI);
  EXPECT_EQ(pi.GetCenter(), M_PI);
  EXPECT_EQ(mipi.GetCenter(), M_PI);
  EXPECT_EQ(fabs(quad23.GetCenter()), M_PI);
  EXPECT_DOUBLE_EQ(quad123.GetCenter(), 0.75 * M_PI);
}

TEST_F(S1IntervalTestBase, GetLength) {
  EXPECT_EQ(quad12.GetLength(), M_PI);
  EXPECT_EQ(pi.GetLength(), 0);
  EXPECT_EQ(mipi.GetLength(), 0);
  EXPECT_DOUBLE_EQ(quad123.GetLength(), 1.5 * M_PI);
  EXPECT_EQ(fabs(quad23.GetLength()), M_PI);
  EXPECT_EQ(full.GetLength(), 2 * M_PI);
  EXPECT_LT(empty.GetLength(), 0);
}

TEST_F(S1IntervalTestBase, Complement) {
  EXPECT_TRUE(empty.Complement().is_full());
  EXPECT_TRUE(full.Complement().is_empty());
  EXPECT_TRUE(pi.Complement().is_full());
  EXPECT_TRUE(mipi.Complement().is_full());
  EXPECT_TRUE(zero.Complement().is_full());
  EXPECT_TRUE(quad12.Complement().ApproxEquals(quad34));
  EXPECT_TRUE(quad34.Complement().ApproxEquals(quad12));
  EXPECT_TRUE(quad123.Complement().ApproxEquals(quad4));
}

TEST_F(S1IntervalTestBase, Contains) {
  // Contains(double), InteriorContains(double)
  EXPECT_TRUE(!empty.Contains(0) && !empty.Contains(M_PI) &&
              !empty.Contains(-M_PI));
  EXPECT_TRUE(!empty.InteriorContains(M_PI) && !empty.InteriorContains(-M_PI));
  EXPECT_TRUE(full.Contains(0) && full.Contains(M_PI) && full.Contains(-M_PI));
  EXPECT_TRUE(full.InteriorContains(M_PI) && full.InteriorContains(-M_PI));
  EXPECT_TRUE(quad12.Contains(0) && quad12.Contains(M_PI) &&
              quad12.Contains(-M_PI));
  EXPECT_TRUE(quad12.InteriorContains(M_PI_2) && !quad12.InteriorContains(0));
  EXPECT_TRUE(!quad12.InteriorContains(M_PI) &&
              !quad12.InteriorContains(-M_PI));
  EXPECT_TRUE(quad23.Contains(M_PI_2) && quad23.Contains(-M_PI_2));
  EXPECT_TRUE(quad23.Contains(M_PI) && quad23.Contains(-M_PI));
  EXPECT_TRUE(!quad23.Contains(0));
  EXPECT_TRUE(!quad23.InteriorContains(M_PI_2) &&
              !quad23.InteriorContains(-M_PI_2));
  EXPECT_TRUE(quad23.InteriorContains(M_PI) && quad23.InteriorContains(-M_PI));
  EXPECT_TRUE(!quad23.InteriorContains(0));
  EXPECT_TRUE(pi.Contains(M_PI) && pi.Contains(-M_PI) && !pi.Contains(0));
  EXPECT_TRUE(!pi.InteriorContains(M_PI) && !pi.InteriorContains(-M_PI));
  EXPECT_TRUE(mipi.Contains(M_PI) && mipi.Contains(-M_PI) && !mipi.Contains(0));
  EXPECT_TRUE(!mipi.InteriorContains(M_PI) && !mipi.InteriorContains(-M_PI));
  EXPECT_TRUE(zero.Contains(0) && !zero.InteriorContains(0));
}

static void TestIntervalOps(const S1Interval& x, const S1Interval& y,
                            const char* expected_relation,
                            const S1Interval& expected_union,
                            const S1Interval& expected_intersection) {
  // Test all of the interval operations on the given pair of intervals.
  // "expected_relation" is a sequence of "T" and "F" characters corresponding
  // to the expected results of Contains(), InteriorContains(), Intersects(),
  // and InteriorIntersects() respectively.

  EXPECT_EQ(x.Contains(y), expected_relation[0] == 'T');
  EXPECT_EQ(x.InteriorContains(y), expected_relation[1] == 'T');
  EXPECT_EQ(x.Intersects(y), expected_relation[2] == 'T');
  EXPECT_EQ(x.InteriorIntersects(y), expected_relation[3] == 'T');

  // bounds() returns a const reference to a member variable, so we need to
  // make a copy when invoking it on a temporary object.
  EXPECT_EQ(Vector2_d(x.Union(y).bounds()), expected_union.bounds());
  EXPECT_EQ(Vector2_d(x.Intersection(y).bounds()),
            expected_intersection.bounds());

  EXPECT_EQ(x.Contains(y), x.Union(y) == x);
  EXPECT_EQ(x.Intersects(y), !x.Intersection(y).is_empty());

  if (y.lo() == y.hi()) {
    S1Interval r = x;
    r.AddPoint(y.lo());
    EXPECT_EQ(r.bounds(), expected_union.bounds());
  }
}

TEST_F(S1IntervalTestBase, IntervalOps) {
  // Contains(S1Interval), InteriorContains(S1Interval),
  // Intersects(), InteriorIntersects(), Union(), Intersection()
  TestIntervalOps(empty, empty, "TTFF", empty, empty);
  TestIntervalOps(empty, full, "FFFF", full, empty);
  TestIntervalOps(empty, zero, "FFFF", zero, empty);
  TestIntervalOps(empty, pi, "FFFF", pi, empty);
  TestIntervalOps(empty, mipi, "FFFF", mipi, empty);

  TestIntervalOps(full, empty, "TTFF", full, empty);
  TestIntervalOps(full, full, "TTTT", full, full);
  TestIntervalOps(full, zero, "TTTT", full, zero);
  TestIntervalOps(full, pi, "TTTT", full, pi);
  TestIntervalOps(full, mipi, "TTTT", full, mipi);
  TestIntervalOps(full, quad12, "TTTT", full, quad12);
  TestIntervalOps(full, quad23, "TTTT", full, quad23);

  TestIntervalOps(zero, empty, "TTFF", zero, empty);
  TestIntervalOps(zero, full, "FFTF", full, zero);
  TestIntervalOps(zero, zero, "TFTF", zero, zero);
  TestIntervalOps(zero, pi, "FFFF", S1Interval(0, M_PI), empty);
  TestIntervalOps(zero, pi2, "FFFF", quad1, empty);
  TestIntervalOps(zero, mipi, "FFFF", quad12, empty);
  TestIntervalOps(zero, mipi2, "FFFF", quad4, empty);
  TestIntervalOps(zero, quad12, "FFTF", quad12, zero);
  TestIntervalOps(zero, quad23, "FFFF", quad123, empty);

  TestIntervalOps(pi2, empty, "TTFF", pi2, empty);
  TestIntervalOps(pi2, full, "FFTF", full, pi2);
  TestIntervalOps(pi2, zero, "FFFF", quad1, empty);
  TestIntervalOps(pi2, pi, "FFFF", S1Interval(M_PI_2, M_PI), empty);
  TestIntervalOps(pi2, pi2, "TFTF", pi2, pi2);
  TestIntervalOps(pi2, mipi, "FFFF", quad2, empty);
  TestIntervalOps(pi2, mipi2, "FFFF", quad23, empty);
  TestIntervalOps(pi2, quad12, "FFTF", quad12, pi2);
  TestIntervalOps(pi2, quad23, "FFTF", quad23, pi2);

  TestIntervalOps(pi, empty, "TTFF", pi, empty);
  TestIntervalOps(pi, full, "FFTF", full, pi);
  TestIntervalOps(pi, zero, "FFFF", S1Interval(M_PI, 0), empty);
  TestIntervalOps(pi, pi, "TFTF", pi, pi);
  TestIntervalOps(pi, pi2, "FFFF", S1Interval(M_PI_2, M_PI), empty);
  TestIntervalOps(pi, mipi, "TFTF", pi, pi);
  TestIntervalOps(pi, mipi2, "FFFF", quad3, empty);
  TestIntervalOps(pi, quad12, "FFTF", S1Interval(0, M_PI), pi);
  TestIntervalOps(pi, quad23, "FFTF", quad23, pi);

  TestIntervalOps(mipi, empty, "TTFF", mipi, empty);
  TestIntervalOps(mipi, full, "FFTF", full, mipi);
  TestIntervalOps(mipi, zero, "FFFF", quad34, empty);
  TestIntervalOps(mipi, pi, "TFTF", mipi, mipi);
  TestIntervalOps(mipi, pi2, "FFFF", quad2, empty);
  TestIntervalOps(mipi, mipi, "TFTF", mipi, mipi);
  TestIntervalOps(mipi, mipi2, "FFFF", S1Interval(-M_PI, -M_PI_2), empty);
  TestIntervalOps(mipi, quad12, "FFTF", quad12, mipi);
  TestIntervalOps(mipi, quad23, "FFTF", quad23, mipi);

  TestIntervalOps(quad12, empty, "TTFF", quad12, empty);
  TestIntervalOps(quad12, full, "FFTT", full, quad12);
  TestIntervalOps(quad12, zero, "TFTF", quad12, zero);
  TestIntervalOps(quad12, pi, "TFTF", quad12, pi);
  TestIntervalOps(quad12, mipi, "TFTF", quad12, mipi);
  TestIntervalOps(quad12, quad12, "TFTT", quad12, quad12);
  TestIntervalOps(quad12, quad23, "FFTT", quad123, quad2);
  TestIntervalOps(quad12, quad34, "FFTF", full, quad12);

  TestIntervalOps(quad23, empty, "TTFF", quad23, empty);
  TestIntervalOps(quad23, full, "FFTT", full, quad23);
  TestIntervalOps(quad23, zero, "FFFF", quad234, empty);
  TestIntervalOps(quad23, pi, "TTTT", quad23, pi);
  TestIntervalOps(quad23, mipi, "TTTT", quad23, mipi);
  TestIntervalOps(quad23, quad12, "FFTT", quad123, quad2);
  TestIntervalOps(quad23, quad23, "TFTT", quad23, quad23);
  TestIntervalOps(quad23, quad34, "FFTT", quad234, S1Interval(-M_PI, -M_PI_2));

  TestIntervalOps(quad1, quad23, "FFTF", quad123, S1Interval(M_PI_2, M_PI_2));
  TestIntervalOps(quad2, quad3, "FFTF", quad23, mipi);
  TestIntervalOps(quad3, quad2, "FFTF", quad23, pi);
  TestIntervalOps(quad2, pi, "TFTF", quad2, pi);
  TestIntervalOps(quad2, mipi, "TFTF", quad2, mipi);
  TestIntervalOps(quad3, pi, "TFTF", quad3, pi);
  TestIntervalOps(quad3, mipi, "TFTF", quad3, mipi);

  TestIntervalOps(quad12, mid12, "TTTT", quad12, mid12);
  TestIntervalOps(mid12, quad12, "FFTT", quad12, mid12);

  S1Interval quad12eps(quad12.lo(), mid23.hi());
  S1Interval quad2hi(mid23.lo(), quad12.hi());
  TestIntervalOps(quad12, mid23, "FFTT", quad12eps, quad2hi);
  TestIntervalOps(mid23, quad12, "FFTT", quad12eps, quad2hi);

  // This test checks that the union of two disjoint intervals is the smallest
  // interval that contains both of them.  Note that the center of "mid34"
  // slightly CCW of -Pi/2 so that there is no ambiguity about the result.
  S1Interval quad412eps(mid34.lo(), quad12.hi());
  TestIntervalOps(quad12, mid34, "FFFF", quad412eps, empty);
  TestIntervalOps(mid34, quad12, "FFFF", quad412eps, empty);

  S1Interval quadeps12(mid41.lo(), quad12.hi());
  S1Interval quad1lo(quad12.lo(), mid41.hi());
  TestIntervalOps(quad12, mid41, "FFTT", quadeps12, quad1lo);
  TestIntervalOps(mid41, quad12, "FFTT", quadeps12, quad1lo);

  S1Interval quad2lo(quad23.lo(), mid12.hi());
  S1Interval quad3hi(mid34.lo(), quad23.hi());
  S1Interval quadeps23(mid12.lo(), quad23.hi());
  S1Interval quad23eps(quad23.lo(), mid34.hi());
  S1Interval quadeps123(mid41.lo(), quad23.hi());
  TestIntervalOps(quad23, mid12, "FFTT", quadeps23, quad2lo);
  TestIntervalOps(mid12, quad23, "FFTT", quadeps23, quad2lo);
  TestIntervalOps(quad23, mid23, "TTTT", quad23, mid23);
  TestIntervalOps(mid23, quad23, "FFTT", quad23, mid23);
  TestIntervalOps(quad23, mid34, "FFTT", quad23eps, quad3hi);
  TestIntervalOps(mid34, quad23, "FFTT", quad23eps, quad3hi);
  TestIntervalOps(quad23, mid41, "FFFF", quadeps123, empty);
  TestIntervalOps(mid41, quad23, "FFFF", quadeps123, empty);
}

TEST_F(S1IntervalTestBase, AddPoint) {
  S1Interval r = empty; r.AddPoint(0);
  EXPECT_EQ(r, zero);
  r = empty; r.AddPoint(M_PI);
  EXPECT_EQ(r, pi);
  r = empty; r.AddPoint(-M_PI);
  EXPECT_EQ(r, mipi);
  r = empty; r.AddPoint(M_PI); r.AddPoint(-M_PI);
  EXPECT_EQ(r, pi);
  r = empty; r.AddPoint(-M_PI); r.AddPoint(M_PI);
  EXPECT_EQ(r, mipi);
  r = empty; r.AddPoint(mid12.lo()); r.AddPoint(mid12.hi());
  EXPECT_EQ(r, mid12);
  r = empty; r.AddPoint(mid23.lo()); r.AddPoint(mid23.hi());
  EXPECT_EQ(r, mid23);
  r = quad1; r.AddPoint(-0.9*M_PI); r.AddPoint(-M_PI_2);
  EXPECT_EQ(r, quad123);
  r = full; r.AddPoint(0);
  EXPECT_TRUE(r.is_full());
  r = full; r.AddPoint(M_PI);
  EXPECT_TRUE(r.is_full());
  r = full; r.AddPoint(-M_PI);
  EXPECT_TRUE(r.is_full());
}

TEST_F(S1IntervalTestBase, Project) {
  S1Interval r(-M_PI, -M_PI);
  EXPECT_EQ(M_PI, r.Project(-M_PI));
  EXPECT_EQ(M_PI, r.Project(0));
  r = S1Interval(0, M_PI);
  EXPECT_EQ(0.1, r.Project(0.1));
  EXPECT_EQ(0, r.Project(-M_PI_2 + 1e-15));
  EXPECT_EQ(M_PI, r.Project(-M_PI_2 - 1e-15));
  r = S1Interval(M_PI - 0.1, -M_PI + 0.1);
  EXPECT_EQ(M_PI, r.Project(M_PI));
  EXPECT_EQ(M_PI - 0.1, r.Project(1e-15));
  EXPECT_EQ(-M_PI + 0.1, r.Project(-1e-15));
  EXPECT_EQ(0, S1Interval::Full().Project(0));
  EXPECT_EQ(M_PI, S1Interval::Full().Project(M_PI));
  EXPECT_EQ(M_PI, S1Interval::Full().Project(-M_PI));
}

TEST_F(S1IntervalTestBase, FromPointPair) {
  EXPECT_EQ(S1Interval::FromPointPair(-M_PI, M_PI), pi);
  EXPECT_EQ(S1Interval::FromPointPair(M_PI, -M_PI), pi);
  EXPECT_EQ(S1Interval::FromPointPair(mid34.hi(), mid34.lo()), mid34);
  EXPECT_EQ(S1Interval::FromPointPair(mid23.lo(), mid23.hi()), mid23);
}

TEST_F(S1IntervalTestBase, Expanded) {
  EXPECT_EQ(empty.Expanded(1), empty);
  EXPECT_EQ(full.Expanded(1), full);
  EXPECT_EQ(zero.Expanded(1), S1Interval(-1, 1));
  EXPECT_EQ(mipi.Expanded(0.01), S1Interval(M_PI - 0.01, -M_PI + 0.01));
  EXPECT_EQ(pi.Expanded(27), full);
  EXPECT_EQ(pi.Expanded(M_PI_2), quad23);
  EXPECT_EQ(pi2.Expanded(M_PI_2), quad12);
  EXPECT_EQ(mipi2.Expanded(M_PI_2), quad34);

  EXPECT_EQ(empty.Expanded(-1), empty);
  EXPECT_EQ(full.Expanded(-1), full);
  EXPECT_EQ(quad123.Expanded(-27), empty);
  EXPECT_EQ(quad234.Expanded(-27), empty);
  EXPECT_EQ(quad123.Expanded(-M_PI_2), quad2);
  EXPECT_EQ(quad341.Expanded(-M_PI_2), quad4);
  EXPECT_EQ(quad412.Expanded(-M_PI_2), quad1);
}

TEST_F(S1IntervalTestBase, ApproxEquals) {
  // Choose two values kLo and kHi such that it's okay to shift an endpoint by
  // kLo (i.e., the resulting interval is equivalent) but not by kHi.
  static const double kLo = 4 * DBL_EPSILON;  // < max_error default
  static const double kHi = 6 * DBL_EPSILON;  // > max_error default

  // Empty intervals.
  EXPECT_TRUE(empty.ApproxEquals(empty));
  EXPECT_TRUE(zero.ApproxEquals(empty) && empty.ApproxEquals(zero));
  EXPECT_TRUE(pi.ApproxEquals(empty) && empty.ApproxEquals(pi));
  EXPECT_TRUE(mipi.ApproxEquals(empty) && empty.ApproxEquals(mipi));
  EXPECT_FALSE(empty.ApproxEquals(full));
  EXPECT_TRUE(empty.ApproxEquals(S1Interval(1, 1 + 2*kLo)));
  EXPECT_FALSE(empty.ApproxEquals(S1Interval(1, 1 + 2*kHi)));
  EXPECT_TRUE(S1Interval(M_PI - kLo, -M_PI + kLo).ApproxEquals(empty));

  // Full intervals.
  EXPECT_TRUE(full.ApproxEquals(full));
  EXPECT_FALSE(full.ApproxEquals(empty));
  EXPECT_FALSE(full.ApproxEquals(zero));
  EXPECT_FALSE(full.ApproxEquals(pi));
  EXPECT_TRUE(full.ApproxEquals(S1Interval(kLo, -kLo)));
  EXPECT_FALSE(full.ApproxEquals(S1Interval(2*kHi, 0)));
  EXPECT_TRUE(S1Interval(-M_PI + kLo, M_PI - kLo).ApproxEquals(full));
  EXPECT_FALSE(S1Interval(-M_PI, M_PI - 2*kHi).ApproxEquals(full));

  // Singleton intervals.
  EXPECT_TRUE(pi.ApproxEquals(pi) && mipi.ApproxEquals(pi));
  EXPECT_TRUE(pi.ApproxEquals(S1Interval(M_PI - kLo, M_PI - kLo)));
  EXPECT_FALSE(pi.ApproxEquals(S1Interval(M_PI - kHi, M_PI - kHi)));
  EXPECT_TRUE(pi.ApproxEquals(S1Interval(M_PI - kLo, -M_PI + kLo)));
  EXPECT_FALSE(pi.ApproxEquals(S1Interval(M_PI - kHi, -M_PI)));
  EXPECT_FALSE(zero.ApproxEquals(pi));
  EXPECT_TRUE(pi.Union(mid12).Union(zero).ApproxEquals(quad12));
  EXPECT_TRUE(quad2.Intersection(quad3).ApproxEquals(pi));
  EXPECT_TRUE(quad3.Intersection(quad2).ApproxEquals(pi));

  // Intervals whose corresponding endpoints are nearly the same but where the
  // endpoints are in opposite order (i.e., inverted intervals).
  EXPECT_FALSE(S1Interval(0, kLo).ApproxEquals(S1Interval(kLo, 0)));
  EXPECT_FALSE(S1Interval(M_PI - 0.5 * kLo, -M_PI + 0.5 * kLo).
               ApproxEquals(S1Interval(-M_PI + 0.5 * kLo, M_PI - 0.5 * kLo)));

  // Other intervals.
  EXPECT_TRUE(S1Interval(1 - kLo, 2 + kLo).ApproxEquals(S1Interval(1, 2)));
  EXPECT_TRUE(S1Interval(1 + kLo, 2 - kLo).ApproxEquals(S1Interval(1, 2)));
  EXPECT_TRUE(S1Interval(2 - kLo, 1 + kLo).ApproxEquals(S1Interval(2, 1)));
  EXPECT_TRUE(S1Interval(2 + kLo, 1 - kLo).ApproxEquals(S1Interval(2, 1)));
  EXPECT_FALSE(S1Interval(1 - kHi, 2 + kLo).ApproxEquals(S1Interval(1, 2)));
  EXPECT_FALSE(S1Interval(1 + kHi, 2 - kLo).ApproxEquals(S1Interval(1, 2)));
  EXPECT_FALSE(S1Interval(2 - kHi, 1 + kLo).ApproxEquals(S1Interval(2, 1)));
  EXPECT_FALSE(S1Interval(2 + kHi, 1 - kLo).ApproxEquals(S1Interval(2, 1)));
  EXPECT_FALSE(S1Interval(1 - kLo, 2 + kHi).ApproxEquals(S1Interval(1, 2)));
  EXPECT_FALSE(S1Interval(1 + kLo, 2 - kHi).ApproxEquals(S1Interval(1, 2)));
  EXPECT_FALSE(S1Interval(2 - kLo, 1 + kHi).ApproxEquals(S1Interval(2, 1)));
  EXPECT_FALSE(S1Interval(2 + kLo, 1 - kHi).ApproxEquals(S1Interval(2, 1)));
}

TEST_F(S1IntervalTestBase, GetDirectedHausdorffDistance) {
  EXPECT_FLOAT_EQ(0.0, empty.GetDirectedHausdorffDistance(empty));
  EXPECT_FLOAT_EQ(0.0, empty.GetDirectedHausdorffDistance(mid12));
  EXPECT_FLOAT_EQ(M_PI, mid12.GetDirectedHausdorffDistance(empty));

  EXPECT_EQ(0.0, quad12.GetDirectedHausdorffDistance(quad123));
  S1Interval in(3.0, -3.0);  // an interval whose complement center is 0.
  EXPECT_FLOAT_EQ(3.0,
                  S1Interval(-0.1, 0.2).GetDirectedHausdorffDistance(in));
  EXPECT_FLOAT_EQ(3.0 - 0.1,
                  S1Interval(0.1, 0.2).GetDirectedHausdorffDistance(in));
  EXPECT_FLOAT_EQ(3.0 - 0.1,
                  S1Interval(-0.2, -0.1).GetDirectedHausdorffDistance(in));
}
