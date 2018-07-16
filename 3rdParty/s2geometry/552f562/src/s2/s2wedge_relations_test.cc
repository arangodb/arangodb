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

#include "s2/s2wedge_relations.h"

#include <gtest/gtest.h>
#include "s2/s2point.h"

void TestWedge(S2Point a0, S2Point ab1, S2Point a2, S2Point b0, S2Point b2,
               bool contains, bool intersects,
               S2::WedgeRelation wedge_relation) {
  a0 = a0.Normalize();
  ab1 = ab1.Normalize();
  a2 = a2.Normalize();
  b0 = b0.Normalize();
  b2 = b2.Normalize();
  EXPECT_EQ(contains, S2::WedgeContains(a0, ab1, a2, b0, b2));
  EXPECT_EQ(intersects, S2::WedgeIntersects(a0, ab1, a2, b0, b2));
  EXPECT_EQ(wedge_relation, S2::GetWedgeRelation(a0, ab1, a2, b0, b2));
}

TEST(S2WedgeRelations, Wedges) {
  // For simplicity, all of these tests use an origin of (0, 0, 1).
  // This shouldn't matter as long as the lower-level primitives are
  // implemented correctly.

  // Intersection in one wedge.
  TestWedge(S2Point(-1, 0, 10), S2Point(0, 0, 1), S2Point(1, 2, 10),
            S2Point(0, 1, 10), S2Point(1, -2, 10),
            false, true, S2::WEDGE_PROPERLY_OVERLAPS);
  // Intersection in two wedges.
  TestWedge(S2Point(-1, -1, 10), S2Point(0, 0, 1), S2Point(1, -1, 10),
            S2Point(1, 0, 10), S2Point(-1, 1, 10),
            false, true, S2::WEDGE_PROPERLY_OVERLAPS);

  // Normal containment.
  TestWedge(S2Point(-1, -1, 10), S2Point(0, 0, 1), S2Point(1, -1, 10),
            S2Point(-1, 0, 10), S2Point(1, 0, 10),
            true, true, S2::WEDGE_PROPERLY_CONTAINS);
  // Containment with equality on one side.
  TestWedge(S2Point(2, 1, 10), S2Point(0, 0, 1), S2Point(-1, -1, 10),
            S2Point(2, 1, 10), S2Point(1, -5, 10),
            true, true, S2::WEDGE_PROPERLY_CONTAINS);
  // Containment with equality on the other side.
  TestWedge(S2Point(2, 1, 10), S2Point(0, 0, 1), S2Point(-1, -1, 10),
            S2Point(1, -2, 10), S2Point(-1, -1, 10),
            true, true, S2::WEDGE_PROPERLY_CONTAINS);

  // Containment with equality on both sides.
  TestWedge(S2Point(-2, 3, 10), S2Point(0, 0, 1), S2Point(4, -5, 10),
            S2Point(-2, 3, 10), S2Point(4, -5, 10),
            true, true, S2::WEDGE_EQUALS);

  // Disjoint with equality on one side.
  TestWedge(S2Point(-2, 3, 10), S2Point(0, 0, 1), S2Point(4, -5, 10),
            S2Point(4, -5, 10), S2Point(-2, -3, 10),
            false, false, S2::WEDGE_IS_DISJOINT);
  // Disjoint with equality on the other side.
  TestWedge(S2Point(-2, 3, 10), S2Point(0, 0, 1), S2Point(0, 5, 10),
            S2Point(4, -5, 10), S2Point(-2, 3, 10),
            false, false, S2::WEDGE_IS_DISJOINT);
  // Disjoint with equality on both sides.
  TestWedge(S2Point(-2, 3, 10), S2Point(0, 0, 1), S2Point(4, -5, 10),
            S2Point(4, -5, 10), S2Point(-2, 3, 10),
            false, false, S2::WEDGE_IS_DISJOINT);

  // B contains A with equality on one side.
  TestWedge(S2Point(2, 1, 10), S2Point(0, 0, 1), S2Point(1, -5, 10),
            S2Point(2, 1, 10), S2Point(-1, -1, 10),
            false, true, S2::WEDGE_IS_PROPERLY_CONTAINED);
  // B contains A with equality on the other side.
  TestWedge(S2Point(2, 1, 10), S2Point(0, 0, 1), S2Point(1, -5, 10),
            S2Point(-2, 1, 10), S2Point(1, -5, 10),
            false, true, S2::WEDGE_IS_PROPERLY_CONTAINED);
}
