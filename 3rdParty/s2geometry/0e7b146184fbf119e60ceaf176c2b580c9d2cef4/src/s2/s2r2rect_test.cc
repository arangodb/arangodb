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
//
// Most of the S2R2Rect methods have trivial implementations in terms of the
// R1Interval class, so most of the testing is done in that unit test.

#include "s2/s2r2rect.h"

#include <gtest/gtest.h>

#include "s2/base/integral_types.h"
#include "s2/r1interval.h"
#include "s2/s2cap.h"
#include "s2/s2cell.h"
#include "s2/s2cell_id.h"
#include "s2/s2coords.h"
#include "s2/s2latlng.h"
#include "s2/s2latlng_rect.h"
#include "s2/s2pointutil.h"
#include "s2/s2testing.h"
#include "s2/third_party/absl/strings/str_cat.h"

using absl::StrCat;

static void TestIntervalOps(const S2R2Rect& x, const S2R2Rect& y,
                            const char* expected_rexion,
                            const S2R2Rect& expected_union,
                            const S2R2Rect& expected_intersection) {
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

  if (y.GetSize() == R2Point(0, 0)) {
    S2R2Rect r = x;
    r.AddPoint(y.lo());
    EXPECT_EQ(expected_union, r);
  }
}

static void TestCellOps(const S2R2Rect& r, const S2Cell& cell, int level) {
  // Test the relationship between the given rectangle and cell:
  // 0 == no intersection, 2 == Intersects,
  // 3 == Intersects and one region contains a vertex of the other,
  // 4 == Contains

  bool vertex_contained = false;
  for (int i = 0; i < 4; ++i) {
    // This would be easier to do by constructing an S2R2Rect from the cell,
    // but that would defeat the purpose of testing this code independently.
    double u, v;
    if (S2::FaceXYZtoUV(0, cell.GetVertexRaw(i), &u, &v)) {
      if (r.Contains(R2Point(S2::UVtoST(u), S2::UVtoST(v))))
        vertex_contained = true;
    }
    if (!r.is_empty() && cell.Contains(S2R2Rect::ToS2Point(r.GetVertex(i))))
      vertex_contained = true;
  }
  EXPECT_EQ(level >= 2, r.MayIntersect(cell));
  EXPECT_EQ(level >= 3, vertex_contained);
  EXPECT_EQ(level >= 4, r.Contains(cell));
}

TEST(S2R2Rect, EmptyRectangles) {
  // Test basic properties of empty rectangles.
  S2R2Rect empty = S2R2Rect::Empty();
  EXPECT_TRUE(empty.is_valid());
  EXPECT_TRUE(empty.is_empty());
}

TEST(S2R2Rect, ConstructorsAndAccessors) {
  // Check various constructors and accessor methods.
  S2R2Rect d1 = S2R2Rect(R2Point(0.1, 0), R2Point(0.25, 1));
  EXPECT_EQ(0.1, d1.x().lo());
  EXPECT_EQ(0.25, d1.x().hi());
  EXPECT_EQ(0.0, d1.y().lo());
  EXPECT_EQ(1.0, d1.y().hi());
  EXPECT_EQ(R1Interval(0.1, 0.25), d1.x());
  EXPECT_EQ(R1Interval(0, 1), d1.y());
}

TEST(S2R2Rect, FromCell) {
  // FromCell, FromCellId
  EXPECT_EQ(S2R2Rect(R2Point(0, 0), R2Point(0.5, 0.5)),
            S2R2Rect::FromCell(S2Cell::FromFacePosLevel(0, 0, 1)));
  EXPECT_EQ(S2R2Rect(R2Point(0, 0), R2Point(1, 1)),
            S2R2Rect::FromCellId(S2CellId::FromFacePosLevel(0, 0, 0)));
}

TEST(S2R2Rect, FromCenterSize) {
  // FromCenterSize()
  EXPECT_TRUE(S2R2Rect::FromCenterSize(R2Point(0.3, 0.5), R2Point(0.2, 0.4)).
              ApproxEquals(S2R2Rect(R2Point(0.2, 0.3), R2Point(0.4, 0.7))));
  EXPECT_TRUE(S2R2Rect::FromCenterSize(R2Point(1, 0.1), R2Point(0, 2)).
              ApproxEquals(S2R2Rect(R2Point(1, -0.9), R2Point(1, 1.1))));
}

TEST(S2R2Rect, FromPoint) {
  // FromPoint(), FromPointPair()
  S2R2Rect d1 = S2R2Rect(R2Point(0.1, 0), R2Point(0.25, 1));
  EXPECT_EQ(S2R2Rect(d1.lo(), d1.lo()), S2R2Rect::FromPoint(d1.lo()));
  EXPECT_EQ(S2R2Rect(R2Point(0.15, 0.3), R2Point(0.35, 0.9)),
            S2R2Rect::FromPointPair(R2Point(0.15, 0.9), R2Point(0.35, 0.3)));
  EXPECT_EQ(S2R2Rect(R2Point(0.12, 0), R2Point(0.83, 0.5)),
            S2R2Rect::FromPointPair(R2Point(0.83, 0), R2Point(0.12, 0.5)));
}

TEST(S2R2Rect, SimplePredicates) {
  // GetCenter(), GetVertex(), Contains(R2Point), InteriorContains(R2Point).
  R2Point sw1 = R2Point(0, 0.25);
  R2Point ne1 = R2Point(0.5, 0.75);
  S2R2Rect r1(sw1, ne1);

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
    SCOPED_TRACE(StrCat("k=", k));
    EXPECT_TRUE(S2::SimpleCCW(S2R2Rect::ToS2Point(r1.GetVertex(k - 1)),
                              S2R2Rect::ToS2Point(r1.GetVertex(k)),
                              S2R2Rect::ToS2Point(r1.GetVertex(k + 1))));
  }
}

TEST(S2R2Rect, IntervalOperations) {
  // Contains(S2R2Rect), InteriorContains(S2R2Rect),
  // Intersects(), InteriorIntersects(), Union(), Intersection().
  //
  // Much more testing of these methods is done in s1interval_test
  // and r1interval_test.

  S2R2Rect empty = S2R2Rect::Empty();
  R2Point sw1 = R2Point(0, 0.25);
  R2Point ne1 = R2Point(0.5, 0.75);
  S2R2Rect r1(sw1, ne1);
  S2R2Rect r1_mid = S2R2Rect(R2Point(0.25, 0.5), R2Point(0.25, 0.5));
  S2R2Rect r_sw1(sw1, sw1);
  S2R2Rect r_ne1(ne1, ne1);

  TestIntervalOps(r1, r1_mid, "TTTT", r1, r1_mid);
  TestIntervalOps(r1, r_sw1, "TFTF", r1, r_sw1);
  TestIntervalOps(r1, r_ne1, "TFTF", r1, r_ne1);

  EXPECT_EQ(S2R2Rect(R2Point(0, 0.25), R2Point(0.5, 0.75)), r1);
  TestIntervalOps(r1, S2R2Rect(R2Point(0.45, 0.1), R2Point(0.75, 0.3)), "FFTT",
                  S2R2Rect(R2Point(0, 0.1), R2Point(0.75, 0.75)),
                  S2R2Rect(R2Point(0.45, 0.25), R2Point(0.5, 0.3)));
  TestIntervalOps(r1, S2R2Rect(R2Point(0.5, 0.1), R2Point(0.7, 0.3)), "FFTF",
                  S2R2Rect(R2Point(0, 0.1), R2Point(0.7, 0.75)),
                  S2R2Rect(R2Point(0.5, 0.25), R2Point(0.5, 0.3)));
  TestIntervalOps(r1, S2R2Rect(R2Point(0.45, 0.1), R2Point(0.7, 0.25)), "FFTF",
                  S2R2Rect(R2Point(0, 0.1), R2Point(0.7, 0.75)),
                  S2R2Rect(R2Point(0.45, 0.25), R2Point(0.5, 0.25)));

  TestIntervalOps(S2R2Rect(R2Point(0.1, 0.2), R2Point(0.1, 0.3)),
                  S2R2Rect(R2Point(0.15, 0.7), R2Point(0.2, 0.8)), "FFFF",
                  S2R2Rect(R2Point(0.1, 0.2), R2Point(0.2, 0.8)),
                  empty);

  // Check that the intersection of two rectangles that overlap in x but not y
  // is valid, and vice versa.
  TestIntervalOps(S2R2Rect(R2Point(0.1, 0.2), R2Point(0.4, 0.5)),
                  S2R2Rect(R2Point(0, 0), R2Point(0.2, 0.1)), "FFFF",
                  S2R2Rect(R2Point(0, 0), R2Point(0.4, 0.5)), empty);
  TestIntervalOps(S2R2Rect(R2Point(0, 0), R2Point(0.1, 0.3)),
                  S2R2Rect(R2Point(0.2, 0.1), R2Point(0.3, 0.4)), "FFFF",
                  S2R2Rect(R2Point(0, 0), R2Point(0.3, 0.4)), empty);
}

TEST(S2R2Rect, AddPoint) {
  // AddPoint()
  R2Point sw1 = R2Point(0, 0.25);
  R2Point ne1 = R2Point(0.5, 0.75);
  S2R2Rect r1(sw1, ne1);

  S2R2Rect r2 = S2R2Rect::Empty();
  r2.AddPoint(R2Point(0, 0.25));
  r2.AddPoint(R2Point(0.5, 0.25));
  r2.AddPoint(R2Point(0, 0.75));
  r2.AddPoint(R2Point(0.1, 0.4));
  EXPECT_EQ(r1, r2);
}

TEST(S2R2Rect, Project) {
  S2R2Rect r1(R1Interval(0, 0.5), R1Interval(0.25, 0.75));

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

TEST(S2R2Rect, Expanded) {
  // Expanded()
  EXPECT_TRUE(S2R2Rect::Empty().Expanded(R2Point(0.1, 0.3)).is_empty());
  EXPECT_TRUE(S2R2Rect::Empty().Expanded(R2Point(-0.1, -0.3)).is_empty());
  EXPECT_TRUE(S2R2Rect(R2Point(0.2, 0.4), R2Point(0.3, 0.7)).
              Expanded(R2Point(0.1, 0.3)).
              ApproxEquals(S2R2Rect(R2Point(0.1, 0.1), R2Point(0.4, 1.0))));
  EXPECT_TRUE(S2R2Rect(R2Point(0.2, 0.4), R2Point(0.3, 0.7)).
              Expanded(R2Point(-0.1, 0.3)).is_empty());
  EXPECT_TRUE(S2R2Rect(R2Point(0.2, 0.4), R2Point(0.3, 0.7)).
              Expanded(R2Point(0.1, -0.2)).is_empty());
  EXPECT_TRUE(S2R2Rect(R2Point(0.2, 0.4), R2Point(0.3, 0.7)).
              Expanded(R2Point(0.1, -0.1)).
              ApproxEquals(S2R2Rect(R2Point(0.1, 0.5), R2Point(0.4, 0.6))));
  EXPECT_TRUE(S2R2Rect(R2Point(0.2, 0.4), R2Point(0.3, 0.7)).Expanded(0.1).
              ApproxEquals(S2R2Rect(R2Point(0.1, 0.3), R2Point(0.4, 0.8))));
}

TEST(S2R2Rect, Bounds) {
  // GetCapBound(), GetRectBound()
  S2R2Rect empty = S2R2Rect::Empty();
  EXPECT_TRUE(empty.GetCapBound().is_empty());
  EXPECT_TRUE(empty.GetRectBound().is_empty());
  EXPECT_EQ(S2Cap::FromPoint(S2Point(1, 0, 0)),
            S2R2Rect(R2Point(0.5, 0.5), R2Point(0.5, 0.5)).GetCapBound());
  EXPECT_EQ(S2LatLngRect::FromPoint(S2LatLng::FromDegrees(0, 0)),
            S2R2Rect(R2Point(0.5, 0.5), R2Point(0.5, 0.5)).GetRectBound());

  for (int i = 0; i < 10; ++i) {
    SCOPED_TRACE(StrCat("i=", i));
    S2R2Rect rect = S2R2Rect::FromCellId(S2Testing::GetRandomCellId());
    S2Cap cap = rect.GetCapBound();
    S2LatLngRect llrect = rect.GetRectBound();
    for (int k = 0; k < 4; ++k) {
      S2Point v = S2R2Rect::ToS2Point(rect.GetVertex(k));
      // v2 is a point that is well outside the rectangle.
      S2Point v2 = (cap.center() + 3 * (v - cap.center())).Normalize();
      EXPECT_TRUE(cap.Contains(v));
      EXPECT_FALSE(cap.Contains(v2));
      EXPECT_TRUE(llrect.Contains(v));
      EXPECT_FALSE(llrect.Contains(v2));
    }
  }
}

TEST(S2R2Rect, CellOperations) {
  // Contains(S2Cell), MayIntersect(S2Cell)

  S2R2Rect empty = S2R2Rect::Empty();
  TestCellOps(empty, S2Cell::FromFace(3), 0);

  // This rectangle includes the first quadrant of face 0.  It's expanded
  // slightly because cell bounding rectangles are slightly conservative.
  S2R2Rect r4 = S2R2Rect(R2Point(0, 0), R2Point(0.5, 0.5));
  TestCellOps(r4, S2Cell::FromFacePosLevel(0, 0, 0), 3);
  TestCellOps(r4, S2Cell::FromFacePosLevel(0, 0, 1), 4);
  TestCellOps(r4, S2Cell::FromFacePosLevel(1, 0, 1), 0);

  // This rectangle intersects the first quadrant of face 0.
  S2R2Rect r5 = S2R2Rect(R2Point(0, 0.45), R2Point(0.5, 0.55));
  TestCellOps(r5, S2Cell::FromFacePosLevel(0, 0, 0), 3);
  TestCellOps(r5, S2Cell::FromFacePosLevel(0, 0, 1), 3);
  TestCellOps(r5, S2Cell::FromFacePosLevel(1, 0, 1), 0);

  // Rectangle consisting of a single point.
  TestCellOps(S2R2Rect(R2Point(0.51, 0.51), R2Point(0.51, 0.51)),
              S2Cell::FromFace(0), 3);

  // Rectangle that intersects the bounding rectangle of face 0
  // but not the face itself.
  TestCellOps(S2R2Rect(R2Point(0.01, 1.001), R2Point(0.02, 1.002)),
              S2Cell::FromFace(0), 0);

  // Rectangle that intersects one corner of face 0.
  TestCellOps(S2R2Rect(R2Point(0.99, -0.01), R2Point(1.01, 0.01)),
              S2Cell::FromFacePosLevel(0, ~uint64{0} >> S2CellId::kFaceBits, 5),
              3);
}
