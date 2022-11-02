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

#include "s2/r1interval.h"

#include <cfloat>

#include <gtest/gtest.h>

static void TestIntervalOps(const R1Interval& x, const R1Interval& y,
                            const char* expected) {
  // Test all of the interval operations on the given pair of intervals.
  // "expected" is a sequence of "T" and "F" characters corresponding to
  // the expected results of Contains(), InteriorContains(), Intersects(),
  // and InteriorIntersects() respectively.

  EXPECT_EQ(expected[0] == 'T', x.Contains(y));
  EXPECT_EQ(expected[1] == 'T', x.InteriorContains(y));
  EXPECT_EQ(expected[2] == 'T', x.Intersects(y));
  EXPECT_EQ(expected[3] == 'T', x.InteriorIntersects(y));

  EXPECT_EQ(x.Contains(y), x.Union(y) == x);
  EXPECT_EQ(x.Intersects(y), !x.Intersection(y).is_empty());

  R1Interval z = x;
  z.AddInterval(y);
  EXPECT_EQ(x.Union(y), z);
}

TEST(R1Interval, TestBasic) {
  // Constructors and accessors.
  R1Interval unit(0, 1);
  R1Interval negunit(-1, 0);
  EXPECT_EQ(0, unit.lo());
  EXPECT_EQ(1, unit.hi());
  EXPECT_EQ(-1, negunit[0]);
  EXPECT_EQ(0, negunit[1]);
  R1Interval ten(0, 0);
  ten.set_hi(10);
  EXPECT_EQ(R1Interval(0, 10), ten);
  ten[0] = -10;
  EXPECT_EQ(R1Interval(-10, 10), ten);
  ten[1] = 0;
  EXPECT_EQ(Vector2_d(-10, 0), ten.bounds());
  *ten.mutable_bounds() = Vector2_d(0, 10);
  EXPECT_EQ(R1Interval(0, 10), ten);

  // is_empty()
  R1Interval half(0.5, 0.5);
  EXPECT_FALSE(unit.is_empty());
  EXPECT_FALSE(half.is_empty());
  R1Interval empty = R1Interval::Empty();
  EXPECT_TRUE(empty.is_empty());

  // == and !=
  EXPECT_TRUE(empty == empty);
  EXPECT_TRUE(unit == unit);
  EXPECT_TRUE(unit != empty);
  EXPECT_TRUE(R1Interval(1, 2) != R1Interval(1, 3));

  // Check that the default R1Interval is identical to Empty().
  R1Interval default_empty;
  EXPECT_TRUE(default_empty.is_empty());
  EXPECT_EQ(empty.lo(), default_empty.lo());
  EXPECT_EQ(empty.hi(), default_empty.hi());

  // GetCenter(), GetLength()
  EXPECT_EQ(unit.GetCenter(), 0.5);
  EXPECT_EQ(half.GetCenter(), 0.5);
  EXPECT_EQ(negunit.GetLength(), 1.0);
  EXPECT_EQ(half.GetLength(), 0);
  EXPECT_LT(empty.GetLength(), 0);

  // Contains(double), InteriorContains(double)
  EXPECT_TRUE(unit.Contains(0.5));
  EXPECT_TRUE(unit.InteriorContains(0.5));
  EXPECT_TRUE(unit.Contains(0));
  EXPECT_FALSE(unit.InteriorContains(0));
  EXPECT_TRUE(unit.Contains(1));
  EXPECT_FALSE(unit.InteriorContains(1));

  // Contains(R1Interval), InteriorContains(R1Interval)
  // Intersects(R1Interval), InteriorIntersects(R1Interval)
  { SCOPED_TRACE(""); TestIntervalOps(empty, empty, "TTFF"); }
  { SCOPED_TRACE(""); TestIntervalOps(empty, unit, "FFFF"); }
  { SCOPED_TRACE(""); TestIntervalOps(unit, half, "TTTT"); }
  { SCOPED_TRACE(""); TestIntervalOps(unit, unit, "TFTT"); }
  { SCOPED_TRACE(""); TestIntervalOps(unit, empty, "TTFF"); }
  { SCOPED_TRACE(""); TestIntervalOps(unit, negunit, "FFTF"); }
  { SCOPED_TRACE(""); TestIntervalOps(unit, R1Interval(0, 0.5), "TFTT"); }
  { SCOPED_TRACE(""); TestIntervalOps(half, R1Interval(0, 0.5), "FFTF"); }

  // AddPoint()
  R1Interval r = empty;
  r.AddPoint(5);
  EXPECT_EQ(5, r.lo());
  EXPECT_EQ(5, r.hi());
  r.AddPoint(-1);
  EXPECT_EQ(-1, r.lo());
  EXPECT_EQ(5, r.hi());
  r.AddPoint(0);
  EXPECT_EQ(-1, r.lo());
  EXPECT_EQ(5, r.hi());

  // Project()
  EXPECT_EQ(0.3, R1Interval(0.1, 0.4).Project(0.3));
  EXPECT_EQ(0.1, R1Interval(0.1, 0.4).Project(-7.0));
  EXPECT_EQ(0.4, R1Interval(0.1, 0.4).Project(0.6));

  // FromPointPair()
  EXPECT_EQ(R1Interval(4, 4), R1Interval::FromPointPair(4, 4));
  EXPECT_EQ(R1Interval(-2, -1), R1Interval::FromPointPair(-1, -2));
  EXPECT_EQ(R1Interval(-5, 3), R1Interval::FromPointPair(-5, 3));

  // Expanded()
  EXPECT_EQ(empty, empty.Expanded(0.45));
  EXPECT_EQ(R1Interval(-0.5, 1.5), unit.Expanded(0.5));
  EXPECT_EQ(R1Interval(0.5, 0.5), unit.Expanded(-0.5));
  EXPECT_TRUE(unit.Expanded(-0.51).is_empty());
  EXPECT_TRUE(unit.Expanded(-0.51).Expanded(0.51).is_empty());

  // Union(), Intersection()
  EXPECT_EQ(R1Interval(99, 100), R1Interval(99, 100).Union(empty));
  EXPECT_EQ(R1Interval(99, 100), empty.Union(R1Interval(99, 100)));
  EXPECT_TRUE(R1Interval(5, 3).Union(R1Interval(0, -2)).is_empty());
  EXPECT_TRUE(R1Interval(0, -2).Union(R1Interval(5, 3)).is_empty());
  EXPECT_EQ(unit, unit.Union(unit));
  EXPECT_EQ(R1Interval(-1, 1), unit.Union(negunit));
  EXPECT_EQ(R1Interval(-1, 1), negunit.Union(unit));
  EXPECT_EQ(unit, half.Union(unit));
  EXPECT_EQ(half, unit.Intersection(half));
  EXPECT_EQ(R1Interval(0, 0), unit.Intersection(negunit));
  EXPECT_TRUE(negunit.Intersection(half).is_empty());
  EXPECT_TRUE(unit.Intersection(empty).is_empty());
  EXPECT_TRUE(empty.Intersection(unit).is_empty());
}

TEST(R1Interval, ApproxEquals) {
  // Choose two values kLo and kHi such that it's okay to shift an endpoint by
  // kLo (i.e., the resulting interval is equivalent) but not by kHi.
  static const double kLo = 4 * DBL_EPSILON;  // < max_error default
  static const double kHi = 6 * DBL_EPSILON;  // > max_error default

  // Empty intervals.
  R1Interval empty = R1Interval::Empty();
  EXPECT_TRUE(empty.ApproxEquals(empty));
  EXPECT_TRUE(R1Interval(0, 0).ApproxEquals(empty));
  EXPECT_TRUE(empty.ApproxEquals(R1Interval(0, 0)));
  EXPECT_TRUE(R1Interval(1, 1).ApproxEquals(empty));
  EXPECT_TRUE(empty.ApproxEquals(R1Interval(1, 1)));
  EXPECT_FALSE(empty.ApproxEquals(R1Interval(0, 1)));
  EXPECT_TRUE(empty.ApproxEquals(R1Interval(1, 1 + 2*kLo)));
  EXPECT_FALSE(empty.ApproxEquals(R1Interval(1, 1 + 2*kHi)));

  // Singleton intervals.
  EXPECT_TRUE(R1Interval(1, 1).ApproxEquals(R1Interval(1, 1)));
  EXPECT_TRUE(R1Interval(1, 1).ApproxEquals(R1Interval(1 - kLo, 1 - kLo)));
  EXPECT_TRUE(R1Interval(1, 1).ApproxEquals(R1Interval(1 + kLo, 1 + kLo)));
  EXPECT_FALSE(R1Interval(1, 1).ApproxEquals(R1Interval(1 - kHi, 1)));
  EXPECT_FALSE(R1Interval(1, 1).ApproxEquals(R1Interval(1, 1 + kHi)));
  EXPECT_TRUE(R1Interval(1, 1).ApproxEquals(R1Interval(1 - kLo, 1 + kLo)));
  EXPECT_FALSE(R1Interval(0, 0).ApproxEquals(R1Interval(1, 1)));

  // Other intervals.
  EXPECT_TRUE(R1Interval(1 - kLo, 2 + kLo).ApproxEquals(R1Interval(1, 2)));
  EXPECT_TRUE(R1Interval(1 + kLo, 2 - kLo).ApproxEquals(R1Interval(1, 2)));
  EXPECT_FALSE(R1Interval(1 - kHi, 2 + kLo).ApproxEquals(R1Interval(1, 2)));
  EXPECT_FALSE(R1Interval(1 + kHi, 2 - kLo).ApproxEquals(R1Interval(1, 2)));
  EXPECT_FALSE(R1Interval(1 - kLo, 2 + kHi).ApproxEquals(R1Interval(1, 2)));
  EXPECT_FALSE(R1Interval(1 + kLo, 2 - kHi).ApproxEquals(R1Interval(1, 2)));
}
