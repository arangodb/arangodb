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

#include "s2/s2lax_loop_shape.h"

#include <memory>
#include <utility>
#include <vector>

#include <gtest/gtest.h>
#include "absl/memory/memory.h"
#include "s2/s2loop.h"
#include "s2/s2shapeutil_contains_brute_force.h"
#include "s2/s2shapeutil_testing.h"
#include "s2/s2text_format.h"

using absl::make_unique;
using std::vector;

TEST(S2LaxLoopShape, EmptyLoop) {
  // Test S2Loop constructor.
  S2LaxLoopShape shape;
  shape.Init(S2Loop(S2Loop::kEmpty()));
  EXPECT_EQ(0, shape.num_vertices());
  EXPECT_EQ(0, shape.num_edges());
  EXPECT_EQ(0, shape.num_chains());
  EXPECT_EQ(2, shape.dimension());
  EXPECT_TRUE(shape.is_empty());
  EXPECT_FALSE(shape.is_full());
  EXPECT_FALSE(shape.GetReferencePoint().contained);
}

TEST(S2LaxLoopShape, Move) {
  // Construct a shape to use as the correct answer and a second identical shape
  // to be moved.
  const std::vector<S2Point> vertices =
      s2textformat::ParsePointsOrDie("0:0, 0:1, 1:1, 1:0");
  const S2LaxLoopShape correct(vertices);
  S2LaxLoopShape to_move(vertices);

  // Test the move constructor.
  S2LaxLoopShape move1(std::move(to_move));
  s2testing::ExpectEqual(correct, move1);
  EXPECT_EQ(correct.id(), move1.id());
  ASSERT_EQ(correct.num_vertices(), move1.num_vertices());
  for (int i = 0; i < correct.num_vertices(); ++i) {
    EXPECT_EQ(correct.vertex(i), move1.vertex(i));
  }

  // Test the move-assignment operator.
  S2LaxLoopShape move2;
  move2 = std::move(move1);
  s2testing::ExpectEqual(correct, move2);
  EXPECT_EQ(correct.id(), move2.id());
  ASSERT_EQ(correct.num_vertices(), move2.num_vertices());
  for (int i = 0; i < correct.num_vertices(); ++i) {
    EXPECT_EQ(correct.vertex(i), move2.vertex(i));
  }
}

TEST(S2LaxLoopShape, MoveFromShapeIndex) {
  // Construct an index containing shapes to be moved.
  MutableS2ShapeIndex index;
  index.Add(make_unique<S2LaxLoopShape>(
      s2textformat::ParsePointsOrDie("0:0, 0:1, 1:1, 1:0")));
  index.Add(make_unique<S2LaxLoopShape>(
      s2textformat::ParsePointsOrDie("0:0, 0:2, 2:2, 2:0")));
  ASSERT_EQ(index.num_shape_ids(), 2);

  // Verify that the move constructor moves the id.
  S2LaxLoopShape& shape0 = *down_cast<S2LaxLoopShape*>(index.shape(0));
  S2LaxLoopShape moved_shape0 = std::move(shape0);
  EXPECT_EQ(moved_shape0.id(), 0);

  // Verify that the move-assignment operator moves the id.
  S2LaxLoopShape& shape1 = *down_cast<S2LaxLoopShape*>(index.shape(1));
  S2LaxLoopShape moved_shape1;
  moved_shape1 = std::move(shape1);
  EXPECT_EQ(moved_shape1.id(), 1);
}

TEST(S2LaxLoopShape, NonEmptyLoop) {
  // Test vector<S2Point> constructor.
  vector<S2Point> vertices =
      s2textformat::ParsePointsOrDie("0:0, 0:1, 1:1, 1:0");
  S2LaxLoopShape shape(vertices);
  EXPECT_EQ(vertices.size(), shape.num_vertices());
  EXPECT_EQ(vertices.size(), shape.num_edges());
  EXPECT_EQ(1, shape.num_chains());
  EXPECT_EQ(0, shape.chain(0).start);
  EXPECT_EQ(vertices.size(), shape.chain(0).length);
  for (int i = 0; i < vertices.size(); ++i) {
    EXPECT_EQ(vertices[i], shape.vertex(i));
    auto edge = shape.edge(i);
    EXPECT_EQ(vertices[i], edge.v0);
    EXPECT_EQ(vertices[(i + 1) % vertices.size()], edge.v1);
  }
  EXPECT_EQ(2, shape.dimension());
  EXPECT_FALSE(shape.is_empty());
  EXPECT_FALSE(shape.is_full());
  EXPECT_FALSE(shape.GetReferencePoint().contained);
}

TEST(S2LaxClosedPolylineShape, NoInterior) {
  vector<S2Point> vertices =
      s2textformat::ParsePointsOrDie("0:0, 0:1, 1:1, 1:0");
  S2LaxClosedPolylineShape shape(vertices);
  EXPECT_EQ(1, shape.dimension());
  EXPECT_FALSE(shape.is_empty());
  EXPECT_FALSE(shape.is_full());
  EXPECT_FALSE(shape.GetReferencePoint().contained);
}

TEST(S2VertexIdLaxLoopShape, EmptyLoop) {
  S2VertexIdLaxLoopShape shape(vector<int32>(), nullptr);
  EXPECT_EQ(0, shape.num_edges());
  EXPECT_EQ(0, shape.num_vertices());
  EXPECT_EQ(0, shape.num_chains());
  EXPECT_EQ(2, shape.dimension());
  EXPECT_TRUE(shape.is_empty());
  EXPECT_FALSE(shape.is_full());
  EXPECT_FALSE(shape.GetReferencePoint().contained);
}

TEST(S2VertexIdLaxLoopShape, Move) {
  // Construct a shape to use as the correct answer and a second identical shape
  // to be moved.
  const std::vector<S2Point> vertices =
      s2textformat::ParsePointsOrDie("0:0, 0:1, 1:1, 1:0");
  const std::vector<int32> vertex_ids = {0, 3, 2, 1};  // Inverted.
  const S2VertexIdLaxLoopShape correct(vertex_ids, &vertices[0]);
  S2VertexIdLaxLoopShape to_move(vertex_ids, &vertices[0]);

  // Test the move constructor.
  S2VertexIdLaxLoopShape move1(std::move(to_move));
  s2testing::ExpectEqual(correct, move1);
  EXPECT_EQ(correct.id(), move1.id());
  ASSERT_EQ(correct.num_vertices(), move1.num_vertices());
  for (int i = 0; i < correct.num_vertices(); ++i) {
    EXPECT_EQ(correct.vertex(i), move1.vertex(i));
  }

  // Test the move-assignment operator.
  S2VertexIdLaxLoopShape move2;
  move2 = std::move(move1);
  s2testing::ExpectEqual(correct, move2);
  EXPECT_EQ(correct.id(), move2.id());
  ASSERT_EQ(correct.num_vertices(), move2.num_vertices());
  for (int i = 0; i < correct.num_vertices(); ++i) {
    EXPECT_EQ(correct.vertex(i), move2.vertex(i));
  }
}

TEST(S2VertexIdLaxLoopShape, MoveFromShapeIndex) {
  // Setup vertices and vertex ids.
  const std::vector<S2Point> vertices0 =
      s2textformat::ParsePointsOrDie("0:0, 0:1, 1:1, 1:0");
  const std::vector<int32> vertex_ids0 = {0, 3, 2, 1};
  const std::vector<S2Point> vertices1 =
      s2textformat::ParsePointsOrDie("0:0, 0:2, 2:2, 2:0");
  const std::vector<int32> vertex_ids1 = {0, 3, 2, 1};

  // Construct an index containing shapes to be moved.
  MutableS2ShapeIndex index;
  index.Add(
      make_unique<S2VertexIdLaxLoopShape>(vertex_ids0, &vertices0[0]));
  index.Add(
      make_unique<S2VertexIdLaxLoopShape>(vertex_ids1, &vertices1[0]));
  ASSERT_EQ(index.num_shape_ids(), 2);

  // Verify that the move constructor moves the id.
  S2VertexIdLaxLoopShape& shape0 =
      *down_cast<S2VertexIdLaxLoopShape*>(index.shape(0));
  S2VertexIdLaxLoopShape moved_shape0 = std::move(shape0);
  EXPECT_EQ(moved_shape0.id(), 0);

  // Verify that the move-assignment operator moves the id.
  S2VertexIdLaxLoopShape& shape1 =
      *down_cast<S2VertexIdLaxLoopShape*>(index.shape(1));
  S2VertexIdLaxLoopShape moved_shape1;
  moved_shape1 = std::move(shape1);
  EXPECT_EQ(moved_shape1.id(), 1);
}

TEST(S2VertexIdLaxLoopShape, InvertedLoop) {
  vector<S2Point> vertex_array =
      s2textformat::ParsePointsOrDie("0:0, 0:1, 1:1, 1:0");
  vector<int32> vertex_ids { 0, 3, 2, 1 };  // Inverted.
  S2VertexIdLaxLoopShape shape(vertex_ids, &vertex_array[0]);
  EXPECT_EQ(4, shape.num_edges());
  EXPECT_EQ(4, shape.num_vertices());
  EXPECT_EQ(1, shape.num_chains());
  EXPECT_EQ(0, shape.chain(0).start);
  EXPECT_EQ(4, shape.chain(0).length);
  EXPECT_EQ(&vertex_array[0], &shape.vertex(0));
  EXPECT_EQ(&vertex_array[3], &shape.vertex(1));
  EXPECT_EQ(&vertex_array[2], &shape.vertex(2));
  EXPECT_EQ(&vertex_array[1], &shape.vertex(3));
  EXPECT_EQ(2, shape.dimension());
  EXPECT_FALSE(shape.is_empty());
  EXPECT_FALSE(shape.is_full());
  EXPECT_TRUE(s2shapeutil::ContainsBruteForce(shape, S2::Origin()));
}
