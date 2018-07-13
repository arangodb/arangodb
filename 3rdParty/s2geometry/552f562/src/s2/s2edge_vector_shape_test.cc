// Copyright 2013 Google Inc. All Rights Reserved.
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

#include "s2/s2edge_vector_shape.h"

#include <gtest/gtest.h>
#include "s2/s2testing.h"

TEST(S2EdgeVectorShape, Empty) {
  S2EdgeVectorShape shape;
  EXPECT_EQ(0, shape.num_edges());
  EXPECT_EQ(0, shape.num_chains());
  EXPECT_EQ(1, shape.dimension());
  EXPECT_TRUE(shape.is_empty());
  EXPECT_FALSE(shape.is_full());
  EXPECT_FALSE(shape.GetReferencePoint().contained);
}

TEST(S2EdgeVectorShape, EdgeAccess) {
  S2EdgeVectorShape shape;
  S2Testing::rnd.Reset(FLAGS_s2_random_seed);
  const int kNumEdges = 100;
  for (int i = 0; i < kNumEdges; ++i) {
    S2Point a = S2Testing::RandomPoint();  // Control the evaluation order
    shape.Add(a, S2Testing::RandomPoint());
  }
  EXPECT_EQ(kNumEdges, shape.num_edges());
  EXPECT_EQ(kNumEdges, shape.num_chains());
  EXPECT_EQ(1, shape.dimension());
  EXPECT_FALSE(shape.is_empty());
  EXPECT_FALSE(shape.is_full());
  S2Testing::rnd.Reset(FLAGS_s2_random_seed);
  for (int i = 0; i < kNumEdges; ++i) {
    EXPECT_EQ(i, shape.chain(i).start);
    EXPECT_EQ(1, shape.chain(i).length);
    auto edge = shape.edge(i);
    EXPECT_EQ(S2Testing::RandomPoint(), edge.v0);
    EXPECT_EQ(S2Testing::RandomPoint(), edge.v1);
  }
}

TEST(S2EdgeVectorShape, SingletonConstructor) {
  S2Point a(1, 0, 0), b(0, 1, 0);
  S2EdgeVectorShape shape(a, b);
  EXPECT_EQ(1, shape.num_edges());
  EXPECT_EQ(1, shape.num_chains());
  EXPECT_FALSE(shape.is_empty());
  EXPECT_FALSE(shape.is_full());
  auto edge = shape.edge(0);
  EXPECT_EQ(a, edge.v0);
  EXPECT_EQ(b, edge.v1);
}
