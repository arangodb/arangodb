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

#include "s2/s2shapeutil_edge_iterator.h"

#include <vector>

#include <gtest/gtest.h>
#include "s2/mutable_s2shape_index.h"
#include "s2/s2text_format.h"

namespace s2shapeutil {

namespace {

// Returns the full list of edges in g.
// The edges are collected from points, lines, and polygons in that order.
std::vector<S2Shape::Edge> GetEdges(const S2ShapeIndex* index) {
  std::vector<S2Shape::Edge> result;
  for (S2Shape* shape : *index) {
    if (shape == nullptr) continue;
    for (int j = 0; j < shape->num_edges(); ++j) {
      result.push_back(shape->edge(j));
    }
  }
  return result;
}

// Verifies that the edges produced by an EdgeIterator matches GetEdges.
void Verify(const S2ShapeIndex* index) {
  std::vector<S2Shape::Edge> expected = GetEdges(index);

  int i = 0;
  for (EdgeIterator it(index); !it.Done(); it.Next(), ++i) {
    ASSERT_TRUE(i < expected.size());
    EXPECT_EQ(expected[i], it.edge());
  }
}

}  // namespace

TEST(S2ShapeutilEdgeIteratorTest, Empty) {
  auto index = s2textformat::MakeIndex("##");
  Verify(index.get());
}

TEST(S2ShapeutilEdgeIteratorTest, Points) {
  auto index = s2textformat::MakeIndex("0:0|1:1##");
  Verify(index.get());
}

TEST(S2ShapeutilEdgeIteratorTest, Lines) {
  auto index = s2textformat::MakeIndex("#0:0,10:10|5:5,5:10|1:2,2:1#");
  Verify(index.get());
}

TEST(S2ShapeutilEdgeIteratorTest, Polygons) {
  auto index =
      s2textformat::MakeIndex("##10:10,10:0,0:0|-10:-10,-10:0,0:0,0:-10");
  Verify(index.get());
}

TEST(S2ShapeutilEdgeIteratorTest, Collection) {
  auto index = s2textformat::MakeIndex(
      "1:1|7:2#1:1,2:2,3:3|2:2,1:7#"
      "10:10,10:0,0:0;20:20,20:10,10:10|15:15,15:0,0:0");
  Verify(index.get());
}

TEST(S2ShapeutilEdgeIteratorTest, Remove) {
  auto index = s2textformat::MakeIndex(
      "1:1|7:2#1:1,2:2,3:3|2:2,1:7#"
      "10:10,10:0,0:0;20:20,20:10,10:10|15:15,15:0,0:0");
  index->Release(0);

  Verify(index.get());
}

TEST(S2ShapeutilEdgeIteratorTest, AssignmentAndEquality) {
  auto index1 = s2textformat::MakeIndex(
      "1:1|7:2#1:1,2:2,3:3|2:2,1:7#"
      "10:10,10:0,0:0;20:20,20:10,10:10|15:15,15:0,0:0");

  auto index2 = s2textformat::MakeIndex(
      "1:1|7:2#1:1,2:2,3:3|2:2,1:7#"
      "10:10,10:0,0:0;20:20,20:10,10:10|15:15,15:0,0:0");

  EdgeIterator it1(index1.get());
  EdgeIterator it2(index2.get());

  // Different indices.
  EXPECT_TRUE(it1 != it2);

  it1 = it2;
  EXPECT_EQ(it1, it2);

  it1.Next();
  EXPECT_TRUE(it1 != it2);

  it2.Next();
  EXPECT_EQ(it1, it2);
}

}  // namespace s2shapeutil
