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

#include "s2/s2lax_polygon_shape.h"

#include <vector>

#include <gtest/gtest.h>
#include "s2/third_party/absl/memory/memory.h"
#include "s2/mutable_s2shape_index.h"
#include "s2/s2cap.h"
#include "s2/s2contains_point_query.h"
#include "s2/s2lax_loop_shape.h"
#include "s2/s2polygon.h"
#include "s2/s2shapeutil_contains_brute_force.h"
#include "s2/s2testing.h"
#include "s2/s2text_format.h"

using absl::make_unique;
using s2textformat::MakePolygonOrDie;
using std::unique_ptr;
using std::vector;

TEST(S2LaxPolygonShape, EmptyPolygon) {
  S2LaxPolygonShape shape((S2Polygon()));
  EXPECT_EQ(0, shape.num_loops());
  EXPECT_EQ(0, shape.num_vertices());
  EXPECT_EQ(0, shape.num_edges());
  EXPECT_EQ(0, shape.num_chains());
  EXPECT_EQ(2, shape.dimension());
  EXPECT_TRUE(shape.is_empty());
  EXPECT_FALSE(shape.is_full());
  EXPECT_FALSE(shape.GetReferencePoint().contained);
}

TEST(S2LaxPolygonShape, FullPolygon) {
  S2LaxPolygonShape shape(S2Polygon(s2textformat::MakeLoopOrDie("full")));
  EXPECT_EQ(1, shape.num_loops());
  EXPECT_EQ(0, shape.num_vertices());
  EXPECT_EQ(0, shape.num_edges());
  EXPECT_EQ(1, shape.num_chains());
  EXPECT_EQ(2, shape.dimension());
  EXPECT_FALSE(shape.is_empty());
  EXPECT_TRUE(shape.is_full());
  EXPECT_TRUE(shape.GetReferencePoint().contained);
}

TEST(S2LaxPolygonShape, SingleVertexPolygon) {
  // S2Polygon doesn't support single-vertex loops, so we need to construct
  // the S2LaxPolygonShape directly.
  vector<vector<S2Point>> loops;
  loops.push_back(s2textformat::ParsePoints("0:0"));
  S2LaxPolygonShape shape(loops);
  EXPECT_EQ(1, shape.num_loops());
  EXPECT_EQ(1, shape.num_vertices());
  EXPECT_EQ(1, shape.num_edges());
  EXPECT_EQ(1, shape.num_chains());
  EXPECT_EQ(0, shape.chain(0).start);
  EXPECT_EQ(1, shape.chain(0).length);
  auto edge = shape.edge(0);
  EXPECT_EQ(loops[0][0], edge.v0);
  EXPECT_EQ(loops[0][0], edge.v1);
  EXPECT_TRUE(edge == shape.chain_edge(0, 0));
  EXPECT_EQ(2, shape.dimension());
  EXPECT_FALSE(shape.is_empty());
  EXPECT_FALSE(shape.is_full());
  EXPECT_FALSE(shape.GetReferencePoint().contained);
}

TEST(S2LaxPolygonShape, SingleLoopPolygon) {
  // Test S2Polygon constructor.
  vector<S2Point> vertices = s2textformat::ParsePoints("0:0, 0:1, 1:1, 1:0");
  S2LaxPolygonShape shape(S2Polygon(make_unique<S2Loop>(vertices)));
  EXPECT_EQ(1, shape.num_loops());
  EXPECT_EQ(vertices.size(), shape.num_vertices());
  EXPECT_EQ(vertices.size(), shape.num_loop_vertices(0));
  EXPECT_EQ(vertices.size(), shape.num_edges());
  EXPECT_EQ(1, shape.num_chains());
  EXPECT_EQ(0, shape.chain(0).start);
  EXPECT_EQ(vertices.size(), shape.chain(0).length);
  for (int i = 0; i < vertices.size(); ++i) {
    EXPECT_EQ(vertices[i], shape.loop_vertex(0, i));
    auto edge = shape.edge(i);
    EXPECT_EQ(vertices[i], edge.v0);
    EXPECT_EQ(vertices[(i + 1) % vertices.size()], edge.v1);
    EXPECT_EQ(edge.v0, shape.chain_edge(0, i).v0);
    EXPECT_EQ(edge.v1, shape.chain_edge(0, i).v1);
  }
  EXPECT_EQ(2, shape.dimension());
  EXPECT_FALSE(shape.is_empty());
  EXPECT_FALSE(shape.is_full());
  EXPECT_FALSE(s2shapeutil::ContainsBruteForce(shape, S2::Origin()));
}

TEST(S2LaxPolygonShape, MultiLoopPolygon) {
  // Test vector<vector<S2Point>> constructor.  Make sure that the loops are
  // oriented so that the interior of the polygon is always on the left.
  vector<S2LaxPolygonShape::Loop> loops = {
    s2textformat::ParsePoints("0:0, 0:3, 3:3"),  // CCW
    s2textformat::ParsePoints("1:1, 2:2, 1:2")   // CW
  };
  S2LaxPolygonShape shape(loops);

  EXPECT_EQ(loops.size(), shape.num_loops());
  int num_vertices = 0;
  EXPECT_EQ(loops.size(), shape.num_chains());
  for (int i = 0; i < loops.size(); ++i) {
    EXPECT_EQ(loops[i].size(), shape.num_loop_vertices(i));
    EXPECT_EQ(num_vertices, shape.chain(i).start);
    EXPECT_EQ(loops[i].size(), shape.chain(i).length);
    for (int j = 0; j < loops[i].size(); ++j) {
      EXPECT_EQ(loops[i][j], shape.loop_vertex(i, j));
      auto edge = shape.edge(num_vertices + j);
      EXPECT_EQ(loops[i][j], edge.v0);
      EXPECT_EQ(loops[i][(j + 1) % loops[i].size()], edge.v1);
    }
    num_vertices += loops[i].size();
  }
  EXPECT_EQ(num_vertices, shape.num_vertices());
  EXPECT_EQ(num_vertices, shape.num_edges());
  EXPECT_EQ(2, shape.dimension());
  EXPECT_FALSE(shape.is_empty());
  EXPECT_FALSE(shape.is_full());
  EXPECT_FALSE(s2shapeutil::ContainsBruteForce(shape, S2::Origin()));
}

TEST(S2LaxPolygonShape, MultiLoopS2Polygon) {
  // Verify that the orientation of loops representing holes is reversed when
  // converting from an S2Polygon to an S2LaxPolygonShape.
  auto polygon = MakePolygonOrDie("0:0, 0:3, 3:3; 1:1, 1:2, 2:2");
  S2LaxPolygonShape shape(*polygon);
  for (int i = 0; i < polygon->num_loops(); ++i) {
    S2Loop* loop = polygon->loop(i);
    for (int j = 0; j < loop->num_vertices(); ++j) {
      EXPECT_EQ(loop->oriented_vertex(j),
                shape.loop_vertex(i, j));
    }
  }
}

TEST(S2LaxPolygonShape, ManyLoopPolygon) {
  // Test a polygon with enough loops so that cumulative_vertices_ is used.
  vector<vector<S2Point>> loops;
  for (int i = 0; i < 100; ++i) {
    S2Point center(S2LatLng::FromDegrees(0, i));
    loops.push_back(
        S2Testing::MakeRegularPoints(center, S1Angle::Degrees(0.1),
                                     S2Testing::rnd.Uniform(3)));
  }
  S2LaxPolygonShape shape(loops);

  EXPECT_EQ(loops.size(), shape.num_loops());
  int num_vertices = 0;
  EXPECT_EQ(loops.size(), shape.num_chains());
  for (int i = 0; i < loops.size(); ++i) {
    EXPECT_EQ(loops[i].size(), shape.num_loop_vertices(i));
    EXPECT_EQ(num_vertices, shape.chain(i).start);
    EXPECT_EQ(loops[i].size(), shape.chain(i).length);
    for (int j = 0; j < loops[i].size(); ++j) {
      EXPECT_EQ(loops[i][j], shape.loop_vertex(i, j));
      auto edge = shape.edge(num_vertices + j);
      EXPECT_EQ(loops[i][j], edge.v0);
      EXPECT_EQ(loops[i][(j + 1) % loops[i].size()], edge.v1);
    }
    num_vertices += loops[i].size();
  }
  EXPECT_EQ(num_vertices, shape.num_vertices());
  EXPECT_EQ(num_vertices, shape.num_edges());
}

TEST(S2LaxPolygonShape, DegenerateLoops) {
  vector<S2LaxPolygonShape::Loop> loops = {
    s2textformat::ParsePoints("1:1, 1:2, 2:2, 1:2, 1:3, 1:2, 1:1"),
    s2textformat::ParsePoints("0:0, 0:3, 0:6, 0:9, 0:6, 0:3, 0:0"),
    s2textformat::ParsePoints("5:5, 6:6")
  };
  S2LaxPolygonShape shape(loops);
  EXPECT_FALSE(shape.GetReferencePoint().contained);
}

TEST(S2LaxPolygonShape, InvertedLoops) {
  vector<S2LaxPolygonShape::Loop> loops = {
    s2textformat::ParsePoints("1:2, 1:1, 2:2"),
    s2textformat::ParsePoints("3:4, 3:3, 4:4")
  };
  S2LaxPolygonShape shape(loops);
  EXPECT_TRUE(s2shapeutil::ContainsBruteForce(shape, S2::Origin()));
}

void CompareS2LoopToShape(const S2Loop& loop, unique_ptr<S2Shape> shape) {
  MutableS2ShapeIndex index;
  index.Add(std::move(shape));
  S2Cap cap = loop.GetCapBound();
  auto query = MakeS2ContainsPointQuery(&index);
  for (int iter = 0; iter < 100; ++iter) {
    S2Point point = S2Testing::SamplePoint(cap);
    EXPECT_EQ(loop.Contains(point),
              query.ShapeContains(*index.shape(0), point));
  }
}

TEST(S2LaxPolygonShape, CompareToS2Loop) {
  for (int iter = 0; iter < 100; ++iter) {
    S2Testing::Fractal fractal;
    fractal.set_max_level(S2Testing::rnd.Uniform(5));
    fractal.set_fractal_dimension(1 + S2Testing::rnd.RandDouble());
    S2Point center = S2Testing::RandomPoint();
    unique_ptr<S2Loop> loop(fractal.MakeLoop(
        S2Testing::GetRandomFrameAt(center), S1Angle::Degrees(5)));

    // Compare S2Loop to S2LaxLoopShape.
    CompareS2LoopToShape(*loop, make_unique<S2LaxLoopShape>(*loop));

    // Compare S2Loop to S2LaxPolygonShape.
    vector<S2LaxPolygonShape::Loop> loops(
        1, vector<S2Point>(&loop->vertex(0),
                           &loop->vertex(0) + loop->num_vertices()));
    CompareS2LoopToShape(*loop, make_unique<S2LaxPolygonShape>(loops));
  }
}
