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

#include "s2/s2shapeutil_build_polygon_boundaries.h"

#include <vector>

#include <gtest/gtest.h>
#include "s2/s2lax_loop_shape.h"
#include "s2/s2text_format.h"

using std::vector;

namespace s2shapeutil {

class TestLaxLoop : public S2LaxLoopShape {
 public:
  explicit TestLaxLoop(const string& vertex_str) {
    vector<S2Point> vertices = s2textformat::ParsePoints(vertex_str);
    Init(vertices);
  }
};

TEST(BuildPolygonBoundaries, NoComponents) {
  vector<vector<S2Shape*>> faces, components;
  BuildPolygonBoundaries(components, &faces);
  EXPECT_EQ(0, faces.size());
}

TEST(BuildPolygonBoundaries, OneLoop) {
  TestLaxLoop a0("0:0, 1:0, 0:1");  // Outer face
  TestLaxLoop a1("0:0, 0:1, 1:0");
  vector<vector<S2Shape*>> faces, components = {{&a0, &a1}};
  BuildPolygonBoundaries(components, &faces);
  EXPECT_EQ(2, faces.size());
}

TEST(BuildPolygonBoundaries, TwoLoopsSameComponent) {
  TestLaxLoop a0("0:0, 1:0, 0:1");  // Outer face
  TestLaxLoop a1("0:0, 0:1, 1:0");
  TestLaxLoop a2("1:0, 0:1, 1:1");
  vector<vector<S2Shape*>> faces, components = {{&a0, &a1, &a2}};
  BuildPolygonBoundaries(components, &faces);
  EXPECT_EQ(3, faces.size());
}

TEST(BuildPolygonBoundaries, TwoNestedLoops) {
  TestLaxLoop a0("0:0, 3:0, 0:3");  // Outer face
  TestLaxLoop a1("0:0, 0:3, 3:0");
  TestLaxLoop b0("1:1, 2:0, 0:2");  // Outer face
  TestLaxLoop b1("1:1, 0:2, 2:0");
  vector<vector<S2Shape*>> faces, components = {{&a0, &a1}, {&b0, &b1}};
  BuildPolygonBoundaries(components, &faces);
  EXPECT_EQ(3, faces.size());
  EXPECT_EQ((vector<S2Shape*>{&b0, &a1}), faces[0]);
}

TEST(BuildPolygonBoundaries, TwoLoopsDifferentComponents) {
  TestLaxLoop a0("0:0, 1:0, 0:1");  // Outer face
  TestLaxLoop a1("0:0, 0:1, 1:0");
  TestLaxLoop b0("0:2, 1:2, 0:3");  // Outer face
  TestLaxLoop b1("0:2, 0:3, 1:2");
  vector<vector<S2Shape*>> faces, components = {{&a0, &a1}, {&b0, &b1}};
  BuildPolygonBoundaries(components, &faces);
  EXPECT_EQ(3, faces.size());
  EXPECT_EQ((vector<S2Shape*>{&a0, &b0}), faces[2]);
}

TEST(BuildPolygonBoundaries, OneDegenerateLoop) {
  TestLaxLoop a0("0:0, 1:0, 0:0");
  vector<vector<S2Shape*>> faces, components = {{&a0}};
  BuildPolygonBoundaries(components, &faces);
  EXPECT_EQ(1, faces.size());
}

TEST(BuildPolygonBoundaries, TwoDegenerateLoops) {
  TestLaxLoop a0("0:0, 1:0, 0:0");
  TestLaxLoop b0("2:0, 3:0, 2:0");
  vector<vector<S2Shape*>> faces, components = {{&a0}, {&b0}};
  BuildPolygonBoundaries(components, &faces);
  EXPECT_EQ(1, faces.size());
  EXPECT_EQ(2, faces[0].size());
}

static void SortFaces(vector<vector<S2Shape*>>* faces) {
  for (auto& face : *faces) {
    std::sort(face.begin(), face.end());
  }
  std::sort(faces->begin(), faces->end());
}

TEST(BuildPolygonBoundaries, ComplexTest1) {
  // Loops at index 0 are the outer (clockwise) loops.
  // Component "a" consists of 4 adjacent squares forming a larger square.
  TestLaxLoop a0("0:0, 25:0, 50:0, 50:25, 50:50, 25:50, 0:50, 0:50");
  TestLaxLoop a1("0:0, 0:25, 25:25, 25:0");
  TestLaxLoop a2("0:25, 0:50, 25:50, 25:25");
  TestLaxLoop a3("25:0, 25:25, 50:25, 50:0");
  TestLaxLoop a4("25:25, 25:50, 50:50, 50:25");
  // Component "b" consists of a degenerate loop to the left of "a".
  TestLaxLoop b0("0:-10, 10:-10");
  // Components "a1_a", "a1_b", and "a1_c" are located within "a1".
  TestLaxLoop a1_a0("5:5, 20:5, 20:10, 5:10");
  TestLaxLoop a1_a1("5:5, 5:10, 10:10, 10:5");
  TestLaxLoop a1_a2("10:5, 10:10, 15:10, 15:5");
  TestLaxLoop a1_a3("15:5, 15:10, 20:10, 20:5");
  TestLaxLoop a1_b0("5:15, 20:15, 20:20, 5:20");
  TestLaxLoop a1_b1("5:15, 5:20, 20:20, 20:15");
  TestLaxLoop a1_c0("2:5, 2:10, 2:5");
  // Two components located inside "a1_a2" and "a1_a3".
  TestLaxLoop a1_a2_a0("11:6, 14:6, 14:9, 11:9");
  TestLaxLoop a1_a2_a1("11:6, 11:9, 14:9, 14:6");
  TestLaxLoop a1_a3_a0("16:6, 19:9, 16:6");
  // Five component located inside "a3" and "a4".
  TestLaxLoop a3_a0("30:5, 45:5, 45:20, 30:20");
  TestLaxLoop a3_a1("30:5, 30:20, 45:20, 45:5");
  TestLaxLoop a4_a0("30:30, 40:30, 30:30");
  TestLaxLoop a4_b0("30:35, 40:35, 30:35");
  TestLaxLoop a4_c0("30:40, 40:40, 30:40");
  TestLaxLoop a4_d0("30:45, 40:45, 30:45");
  vector<vector<S2Shape*>> components = {
    {&a0, &a1, &a2, &a3, &a4},
    {&b0},
    {&a1_a0, &a1_a1, &a1_a2, &a1_a3},
    {&a1_b0, &a1_b1},
    {&a1_c0},
    {&a1_a2_a0, &a1_a2_a1},
    {&a1_a3_a0},
    {&a3_a0, &a3_a1},
    {&a4_a0},
    {&a4_b0},
    {&a4_c0},
    {&a4_d0}
  };
  vector<vector<S2Shape*>> expected_faces = {
    {&a0, &b0},
    {&a1, &a1_a0, &a1_b0, &a1_c0},
    {&a1_a1},
    {&a1_a2, &a1_a2_a0},
    {&a1_a2_a1},
    {&a1_a3, &a1_a3_a0},
    {&a1_b1},
    {&a2},
    {&a3, &a3_a0},
    {&a3_a1},
    {&a4, &a4_a0, &a4_b0, &a4_c0, &a4_d0}
  };
  vector<vector<S2Shape*>> faces;
  BuildPolygonBoundaries(components, &faces);
  EXPECT_EQ(expected_faces.size(), faces.size());
  SortFaces(&expected_faces);
  SortFaces(&faces);
  EXPECT_EQ(expected_faces, faces);
}

}  // namespace s2shapeutil
