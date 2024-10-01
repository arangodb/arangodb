// Copyright 2018 Google Inc. All Rights Reserved.
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

#include "s2/s2shapeutil_testing.h"

#include <gtest/gtest.h>

namespace s2testing {

void ExpectEqual(const S2Shape& a, const S2Shape& b) {
  ASSERT_EQ(a.num_edges(), b.num_edges());
  for (int i = 0; i < a.num_edges(); ++i) {
    EXPECT_EQ(a.edge(i), b.edge(i));
    EXPECT_EQ(a.chain_position(i), b.chain_position(i));
  }
  EXPECT_EQ(a.dimension(), b.dimension());
  EXPECT_EQ(a.GetReferencePoint(), b.GetReferencePoint());
  ASSERT_EQ(a.num_chains(), b.num_chains());
  for (int i = 0; i < a.num_chains(); ++i) {
    ASSERT_EQ(a.chain(i), b.chain(i));
    int chain_length = a.chain(i).length;
    for (int j = 0; j < chain_length; ++j) {
      EXPECT_EQ(a.chain_edge(i, j), b.chain_edge(i, j));
    }
  }
}

// Verifies that all methods of the two S2ShapeIndexes return identical
// results (including all the S2Shapes in both indexes).
void ExpectEqual(const S2ShapeIndex& a, const S2ShapeIndex& b) {
  // Check that both indexes have identical shapes.
  ASSERT_EQ(a.num_shape_ids(), b.num_shape_ids());
  for (int shape_id = 0; shape_id < a.num_shape_ids(); ++shape_id) {
    S2Shape* a_shape = a.shape(shape_id);
    S2Shape* b_shape = b.shape(shape_id);
    if (a_shape == nullptr || b_shape == nullptr) {
      EXPECT_EQ(a_shape, b_shape);
    } else {
      EXPECT_EQ(a_shape->id(), b_shape->id());
      s2testing::ExpectEqual(*a_shape, *b_shape);
    }
  }

  // Check that both indexes have identical cell contents.
  S2ShapeIndex::Iterator a_it(&a, S2ShapeIndex::BEGIN);
  S2ShapeIndex::Iterator b_it(&b, S2ShapeIndex::BEGIN);
  for (; !a_it.done(); a_it.Next(), b_it.Next()) {
    ASSERT_FALSE(b_it.done());
    ASSERT_EQ(a_it.id(), b_it.id());
    const S2ShapeIndexCell& a_cell = a_it.cell();
    const S2ShapeIndexCell& b_cell = b_it.cell();
    ASSERT_EQ(a_cell.num_clipped(), b_cell.num_clipped());
    for (int i = 0; i < a_cell.num_clipped(); ++i) {
      const S2ClippedShape& a_clipped = a_cell.clipped(i);
      const S2ClippedShape& b_clipped = b_cell.clipped(i);
      EXPECT_EQ(a_clipped.shape_id(), b_clipped.shape_id());
      EXPECT_EQ(a_clipped.contains_center(), b_clipped.contains_center());
      ASSERT_EQ(a_clipped.num_edges(), b_clipped.num_edges());
      for (int j = 0; j < a_clipped.num_edges(); ++j) {
        EXPECT_EQ(a_clipped.edge(j), b_clipped.edge(j));
      }
    }
  }
  EXPECT_TRUE(b_it.done());

  // Spot-check the other iterator methods.  (We know that both indexes have
  // the same contents, so any differences are due to implementation bugs.)
  a_it.Begin();
  b_it.Begin();
  EXPECT_EQ(a_it.id(), b_it.id());
  if (!a_it.done()) {
    a_it.Next();
    b_it.Next();
    EXPECT_EQ(a_it.id(), b_it.id());
    EXPECT_EQ(a_it.done(), b_it.done());
    EXPECT_TRUE(a_it.Prev());
    EXPECT_TRUE(b_it.Prev());
    EXPECT_EQ(a_it.id(), b_it.id());
  }
  EXPECT_FALSE(a_it.Prev());
  EXPECT_FALSE(b_it.Prev());
  a_it.Finish();
  b_it.Finish();
  EXPECT_EQ(a_it.id(), b_it.id());
  a_it.Seek(a_it.id().next());
  b_it.Seek(b_it.id().next());
  EXPECT_EQ(a_it.id(), b_it.id());
}

}  // namespace s2testing
