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

#include "s2/s2predicates.h"

namespace S2 {

bool WedgeContains(
    const S2Point& a0, const S2Point& ab1, const S2Point& a2,
    const S2Point& b0, const S2Point& b2) {
  // For A to contain B (where each loop interior is defined to be its left
  // side), the CCW edge order around ab1 must be a2 b2 b0 a0.  We split
  // this test into two parts that test three vertices each.
  return (s2pred::OrderedCCW(a2, b2, b0, ab1) &&
          s2pred::OrderedCCW(b0, a0, a2, ab1));
}

bool WedgeIntersects(
    const S2Point& a0, const S2Point& ab1, const S2Point& a2,
    const S2Point& b0, const S2Point& b2) {
  // For A not to intersect B (where each loop interior is defined to be
  // its left side), the CCW edge order around ab1 must be a0 b2 b0 a2.
  // Note that it's important to write these conditions as negatives
  // (!OrderedCCW(a,b,c,o) rather than Ordered(c,b,a,o)) to get correct
  // results when two vertices are the same.
  return !(s2pred::OrderedCCW(a0, b2, b0, ab1) &&
           s2pred::OrderedCCW(b0, a2, a0, ab1));
}

WedgeRelation GetWedgeRelation(
    const S2Point& a0, const S2Point& ab1, const S2Point& a2,
    const S2Point& b0, const S2Point& b2) {
  // There are 6 possible edge orderings at a shared vertex (all
  // of these orderings are circular, i.e. abcd == bcda):
  //
  //  (1) a2 b2 b0 a0: A contains B
  //  (2) a2 a0 b0 b2: B contains A
  //  (3) a2 a0 b2 b0: A and B are disjoint
  //  (4) a2 b0 a0 b2: A and B intersect in one wedge
  //  (5) a2 b2 a0 b0: A and B intersect in one wedge
  //  (6) a2 b0 b2 a0: A and B intersect in two wedges
  //
  // We do not distinguish between 4, 5, and 6.
  // We pay extra attention when some of the edges overlap.  When edges
  // overlap, several of these orderings can be satisfied, and we take
  // the most specific.
  if (a0 == b0 && a2 == b2) return WEDGE_EQUALS;

  if (s2pred::OrderedCCW(a0, a2, b2, ab1)) {
    // The cases with this vertex ordering are 1, 5, and 6,
    // although case 2 is also possible if a2 == b2.
    if (s2pred::OrderedCCW(b2, b0, a0, ab1)) return WEDGE_PROPERLY_CONTAINS;

    // We are in case 5 or 6, or case 2 if a2 == b2.
    return (a2 == b2) ? WEDGE_IS_PROPERLY_CONTAINED : WEDGE_PROPERLY_OVERLAPS;
  }

  // We are in case 2, 3, or 4.
  if (s2pred::OrderedCCW(a0, b0, b2, ab1)) return WEDGE_IS_PROPERLY_CONTAINED;
  return s2pred::OrderedCCW(a0, b0, a2, ab1) ?
      WEDGE_IS_DISJOINT : WEDGE_PROPERLY_OVERLAPS;
}

}  // namespace S2
