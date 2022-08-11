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

#include <memory>
#include <utility>

#include <gtest/gtest.h>
#include "absl/flags/flag.h"
#include "s2/s2shapeutil_testing.h"
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

TEST(S2EdgeVectorShape, Move) {
  // Construct a shape to use as the correct answer and a second identical shape
  // to be moved.
  S2EdgeVectorShape correct;
  S2EdgeVectorShape to_move;
  S2Testing::rnd.Reset(absl::GetFlag(FLAGS_s2_random_seed));
  const int kNumEdges = 100;
  for (int i = 0; i < kNumEdges; ++i) {
    const S2Point start_point = S2Testing::RandomPoint();
    const S2Point end_point = S2Testing::RandomPoint();
    correct.Add(start_point, end_point);
    to_move.Add(start_point, end_point);
  }

  // Test the move constructor.
  S2EdgeVectorShape move1(std::move(to_move));
  s2testing::ExpectEqual(correct, move1);
  EXPECT_EQ(correct.id(), move1.id());

  // Test the move-assignment operator.
  S2EdgeVectorShape move2;
  move2 = std::move(move1);
  s2testing::ExpectEqual(correct, move2);
  EXPECT_EQ(correct.id(), move2.id());
}

TEST(S2EdgeVectorShape, EdgeAccess) {
  S2EdgeVectorShape shape;
  S2Testing::rnd.Reset(absl::GetFlag(FLAGS_s2_random_seed));
  const int kNumEdges = 100;
  std::vector<std::pair<S2Point, S2Point>> edges;
  for (int i = 0; i < kNumEdges; ++i) {
    S2Point a = S2Testing::RandomPoint();  // Control the evaluation order
    edges.emplace_back(a, S2Testing::RandomPoint());
    shape.Add(edges.back().first, edges.back().second);
  }
  EXPECT_EQ(kNumEdges, shape.num_edges());
  EXPECT_EQ(kNumEdges, shape.num_chains());
  EXPECT_EQ(1, shape.dimension());
  EXPECT_FALSE(shape.is_empty());
  EXPECT_FALSE(shape.is_full());
  for (int i = 0; i < kNumEdges; ++i) {
    EXPECT_EQ(i, shape.chain(i).start);
    EXPECT_EQ(1, shape.chain(i).length);
    auto edge = shape.edge(i);
    EXPECT_EQ(edges.at(i).first, edge.v0);
    EXPECT_EQ(edges.at(i).second, edge.v1);
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
