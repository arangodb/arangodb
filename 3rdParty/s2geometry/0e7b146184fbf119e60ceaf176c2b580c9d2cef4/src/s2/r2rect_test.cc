// Copyright 2012 Google Inc. All Rights Reserved.
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
//
// Most of the R2Rect methods have trivial implementations in terms of the
// R1Interval class, so most of the testing is done in that unit test.

#include "s2/r2rect.h"

#include <gtest/gtest.h>
#include "s2/r2.h"

static void TestIntervalOps(const R2Rect& x, const R2Rect& y,
                            const char* expected_rexion,
                            const R2Rect& expected_union,
                            const R2Rect& expected_intersection) {
  // Test all of the interval operations on the given pair of intervals.
  // "expected_rexion" is a sequence of "T" and "F" characters corresponding
  // to the expected results of Contains(), InteriorContains(), Intersects(),
  // and InteriorIntersects() respectively.

  EXPECT_EQ(expected_rexion[0] == 'T', x.Contains(y));
  EXPECT_EQ(expected_rexion[1] == 'T', x.InteriorContains(y));
  EXPECT_EQ(expected_rexion[2] == 'T', x.Intersects(y));
  EXPECT_EQ(expected_rexion[3] == 'T', x.InteriorIntersects(y));

  EXPECT_EQ(x.Union(y) == x, x.Contains(y));
  EXPECT_EQ(!x.Intersection(y).is_empty(), x.Intersects(y));

  EXPECT_EQ(expected_union, x.Union(y));
  EXPECT_EQ(expected_intersection, x.Intersection(y));

  R2Rect r = x;
  r.AddRect(y);
  EXPECT_EQ(expected_union, r);
  if (y.GetSize() == R2Point(0, 0)) {
    r = x;
    r.AddPoint(y.lo());
    EXPECT_EQ(expected_union, r);
  }
}

TEST(R2Rect, EmptyRectangles) {
  // Test basic properties of empty rectangles.
  R2Rect empty = R2Rect::Empty();
  EXPECT_TRUE(empty.is_valid());
  EXPECT_TRUE(empty.is_empty());
}

TEST(R2Rect, ConstructorsAndAccessors) {
  // Check various constructors and accessor methods.
  R2Rect r = R2Rect(R2Point(0.1, 0), R2Point(0.25, 1));
  EXPECT_EQ(0.1, r.x().lo());
  EXPECT_EQ(0.25, r.x().hi());
  EXPECT_EQ(0.0, r.y().lo());
  EXPECT_EQ(1.0, r.y().hi());

  EXPECT_EQ(0.1, r[0][0]);
  EXPECT_EQ(0.25, r[0][1]);
  EXPECT_EQ(0.0, r[1][0]);
  EXPECT_EQ(1.0, r[1][1]);

  EXPECT_EQ(R1Interval(0.1, 0.25), r.x());
  EXPECT_EQ(R1Interval(0, 1), r.y());

  EXPECT_EQ(R1Interval(0.1, 0.25), r[0]);
  EXPECT_EQ(R1Interval(0, 1), r[1]);

  r[0] = R1Interval(3, 4);
  r[1][0] = 5;
  r[1][1] = 6;
  EXPECT_EQ(R1Interval(3, 4), r[0]);
  EXPECT_EQ(R1Interval(5, 6), r[1]);

  R2Rect r2;
  EXPECT_TRUE(r2.is_empty());
}

TEST(R2Rect, FromCenterSize) {
  // FromCenterSize()
  EXPECT_TRUE(R2Rect::FromCenterSize(R2Point(0.3, 0.5), R2Point(0.2, 0.4)).
              ApproxEquals(R2Rect(R2Point(0.2, 0.3), R2Point(0.4, 0.7))));
  EXPECT_TRUE(R2Rect::FromCenterSize(R2Point(1, 0.1), R2Point(0, 2)).
              ApproxEquals(R2Rect(R2Point(1, -0.9), R2Point(1, 1.1))));
}

TEST(R2Rect, FromPoint) {
  // FromPoint(), FromPointPair()
  R2Rect d1 = R2Rect(R2Point(0.1, 0), R2Point(0.25, 1));
  EXPECT_EQ(R2Rect(d1.lo(), d1.lo()), R2Rect::FromPoint(d1.lo()));
  EXPECT_EQ(R2Rect(R2Point(0.15, 0.3), R2Point(0.35, 0.9)),
            R2Rect::FromPointPair(R2Point(0.15, 0.9), R2Point(0.35, 0.3)));
  EXPECT_EQ(R2Rect(R2Point(0.12, 0), R2Point(0.83, 0.5)),
            R2Rect::FromPointPair(R2Point(0.83, 0), R2Point(0.12, 0.5)));
}

TEST(R2Rect, SimplePredicates) {
  // GetCenter(), GetVertex(), Contains(R2Point), InteriorContains(R2Point).
  R2Point sw1 = R2Point(0, 0.25);
  R2Point ne1 = R2Point(0.5, 0.75);
  R2Rect r1(sw1, ne1);

  EXPECT_EQ(R2Point(0.25, 0.5), r1.GetCenter());
  EXPECT_EQ(R2Point(0, 0.25), r1.GetVertex(0));
  EXPECT_EQ(R2Point(0.5, 0.25), r1.GetVertex(1));
  EXPECT_EQ(R2Point(0.5, 0.75), r1.GetVertex(2));
  EXPECT_EQ(R2Point(0, 0.75), r1.GetVertex(3));
  EXPECT_TRUE(r1.Contains(R2Point(0.2, 0.4)));
  EXPECT_FALSE(r1.Contains(R2Point(0.2, 0.8)));
  EXPECT_FALSE(r1.Contains(R2Point(-0.1, 0.4)));
  EXPECT_FALSE(r1.Contains(R2Point(0.6, 0.1)));
  EXPECT_TRUE(r1.Contains(sw1));
  EXPECT_TRUE(r1.Contains(ne1));
  EXPECT_FALSE(r1.InteriorContains(sw1));
  EXPECT_FALSE(r1.InteriorContains(ne1));

  // Make sure that GetVertex() returns vertices in CCW order.
  for (int k = 0; k < 4; ++k) {
    R2Point a = r1.GetVertex(k - 1);
    R2Point b = r1.GetVertex(k);
    R2Point c = r1.GetVertex(k + 1);
    EXPECT_GT((b - a).Ortho().DotProd(c - a), 0);
  }
}

TEST(R2Rect, IntervalOperations) {
  // Contains(R2Rect), InteriorContains(R2Rect),
  // Intersects(), InteriorIntersects(), Union(), Intersection().
  //
  // Much more testing of these methods is done in s1interval_test
  // and r1interval_test.

  R2Rect empty = R2Rect::Empty();
  R2Point sw1 = R2Point(0, 0.25);
  R2Point ne1 = R2Point(0.5, 0.75);
  R2Rect r1(sw1, ne1);
  R2Rect r1_mid = R2Rect(R2Point(0.25, 0.5), R2Point(0.25, 0.5));
  R2Rect r_sw1(sw1, sw1);
  R2Rect r_ne1(ne1, ne1);

  TestIntervalOps(r1, r1_mid, "TTTT", r1, r1_mid);
  TestIntervalOps(r1, r_sw1, "TFTF", r1, r_sw1);
  TestIntervalOps(r1, r_ne1, "TFTF", r1, r_ne1);

  EXPECT_EQ(R2Rect(R2Point(0, 0.25), R2Point(0.5, 0.75)), r1);
  TestIntervalOps(r1, R2Rect(R2Point(0.45, 0.1), R2Point(0.75, 0.3)), "FFTT",
                  R2Rect(R2Point(0, 0.1), R2Point(0.75, 0.75)),
                  R2Rect(R2Point(0.45, 0.25), R2Point(0.5, 0.3)));
  TestIntervalOps(r1, R2Rect(R2Point(0.5, 0.1), R2Point(0.7, 0.3)), "FFTF",
                  R2Rect(R2Point(0, 0.1), R2Point(0.7, 0.75)),
                  R2Rect(R2Point(0.5, 0.25), R2Point(0.5, 0.3)));
  TestIntervalOps(r1, R2Rect(R2Point(0.45, 0.1), R2Point(0.7, 0.25)), "FFTF",
                  R2Rect(R2Point(0, 0.1), R2Point(0.7, 0.75)),
                  R2Rect(R2Point(0.45, 0.25), R2Point(0.5, 0.25)));

  TestIntervalOps(R2Rect(R2Point(0.1, 0.2), R2Point(0.1, 0.3)),
                  R2Rect(R2Point(0.15, 0.7), R2Point(0.2, 0.8)), "FFFF",
                  R2Rect(R2Point(0.1, 0.2), R2Point(0.2, 0.8)),
                  empty);

  // Check that the intersection of two rectangles that overlap in x but not y
  // is valid, and vice versa.
  TestIntervalOps(R2Rect(R2Point(0.1, 0.2), R2Point(0.4, 0.5)),
                  R2Rect(R2Point(0, 0), R2Point(0.2, 0.1)), "FFFF",
                  R2Rect(R2Point(0, 0), R2Point(0.4, 0.5)), empty);
  TestIntervalOps(R2Rect(R2Point(0, 0), R2Point(0.1, 0.3)),
                  R2Rect(R2Point(0.2, 0.1), R2Point(0.3, 0.4)), "FFFF",
                  R2Rect(R2Point(0, 0), R2Point(0.3, 0.4)), empty);
}

TEST(R2Rect, AddPoint) {
  // AddPoint()
  R2Point sw1 = R2Point(0, 0.25);
  R2Point ne1 = R2Point(0.5, 0.75);
  R2Rect r1(sw1, ne1);

  R2Rect r2 = R2Rect::Empty();
  r2.AddPoint(R2Point(0, 0.25));
  r2.AddPoint(R2Point(0.5, 0.25));
  r2.AddPoint(R2Point(0, 0.75));
  r2.AddPoint(R2Point(0.1, 0.4));
  EXPECT_EQ(r1, r2);
}

TEST(R2Rect, Project) {
  R2Rect r1(R1Interval(0, 0.5), R1Interval(0.25, 0.75));

  EXPECT_EQ(R2Point(0, 0.25), r1.Project(R2Point(-0.01, 0.24)));
  EXPECT_EQ(R2Point(0, 0.48), r1.Project(R2Point(-5.0, 0.48)));
  EXPECT_EQ(R2Point(0, 0.75), r1.Project(R2Point(-5.0, 2.48)));
  EXPECT_EQ(R2Point(0.19, 0.75), r1.Project(R2Point(0.19, 2.48)));
  EXPECT_EQ(R2Point(0.5, 0.75), r1.Project(R2Point(6.19, 2.48)));
  EXPECT_EQ(R2Point(0.5, 0.53), r1.Project(R2Point(6.19, 0.53)));
  EXPECT_EQ(R2Point(0.5, 0.25), r1.Project(R2Point(6.19, -2.53)));
  EXPECT_EQ(R2Point(0.33, 0.25), r1.Project(R2Point(0.33, -2.53)));
  EXPECT_EQ(R2Point(0.33, 0.37), r1.Project(R2Point(0.33, 0.37)));
}

TEST(R2Rect, Expanded) {
  // Expanded()
  EXPECT_TRUE(R2Rect::Empty().Expanded(R2Point(0.1, 0.3)).is_empty());
  EXPECT_TRUE(R2Rect::Empty().Expanded(R2Point(-0.1, -0.3)).is_empty());
  EXPECT_TRUE(R2Rect(R2Point(0.2, 0.4), R2Point(0.3, 0.7)).
              Expanded(R2Point(0.1, 0.3)).
              ApproxEquals(R2Rect(R2Point(0.1, 0.1), R2Point(0.4, 1.0))));
  EXPECT_TRUE(R2Rect(R2Point(0.2, 0.4), R2Point(0.3, 0.7)).
              Expanded(R2Point(-0.1, 0.3)).is_empty());
  EXPECT_TRUE(R2Rect(R2Point(0.2, 0.4), R2Point(0.3, 0.7)).
              Expanded(R2Point(0.1, -0.2)).is_empty());
  EXPECT_TRUE(R2Rect(R2Point(0.2, 0.4), R2Point(0.3, 0.7)).
              Expanded(R2Point(0.1, -0.1)).
              ApproxEquals(R2Rect(R2Point(0.1, 0.5), R2Point(0.4, 0.6))));
  EXPECT_TRUE(R2Rect(R2Point(0.2, 0.4), R2Point(0.3, 0.7)).Expanded(0.1).
              ApproxEquals(R2Rect(R2Point(0.1, 0.3), R2Point(0.4, 0.8))));
}
