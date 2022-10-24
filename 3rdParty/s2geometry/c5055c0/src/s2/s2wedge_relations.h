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
// Defines functions for determining the relationship between two angles
// ("wedges") that share a common vertex.

#ifndef S2_S2WEDGE_RELATIONS_H_
#define S2_S2WEDGE_RELATIONS_H_

#include "s2/s2point.h"

namespace S2 {

// Given an edge chain (x0, x1, x2), the wedge at x1 is the region to the
// left of the edges.  More precisely, it is the set of all rays from x1x0
// (inclusive) to x1x2 (exclusive) in the *clockwise* direction.
//
// The following functions compare two *non-empty* wedges that share the
// same middle vertex: A=(a0, ab1, a2) and B=(b0, ab1, b2).

// Detailed relation from one wedge A to another wedge B.
enum WedgeRelation {
  WEDGE_EQUALS,                 // A and B are equal.
  WEDGE_PROPERLY_CONTAINS,      // A is a strict superset of B.
  WEDGE_IS_PROPERLY_CONTAINED,  // A is a strict subset of B.
  WEDGE_PROPERLY_OVERLAPS,      // A-B, B-A, and A intersect B are non-empty.
  WEDGE_IS_DISJOINT,            // A and B are disjoint.
};

// Returns the relation from wedge A to B.
// REQUIRES: A and B are non-empty.
WedgeRelation GetWedgeRelation(
    const S2Point& a0, const S2Point& ab1, const S2Point& a2,
    const S2Point& b0, const S2Point& b2);

// Returns true if wedge A contains wedge B.  Equivalent to but faster than
// GetWedgeRelation() == WEDGE_PROPERLY_CONTAINS || WEDGE_EQUALS.
// REQUIRES: A and B are non-empty.
bool WedgeContains(const S2Point& a0, const S2Point& ab1, const S2Point& a2,
                   const S2Point& b0, const S2Point& b2);

// Returns true if wedge A intersects wedge B.  Equivalent to but faster
// than GetWedgeRelation() != WEDGE_IS_DISJOINT.
// REQUIRES: A and B are non-empty.
bool WedgeIntersects(const S2Point& a0, const S2Point& ab1, const S2Point& a2,
                     const S2Point& b0, const S2Point& b2);

}  // namespace S2

#endif  // S2_S2WEDGE_RELATIONS_H_
