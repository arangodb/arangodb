// Copyright 2017 Google Inc. All Rights Reserved.
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

#include "s2/s2contains_vertex_query.h"

#include <gtest/gtest.h>
#include "s2/s2edge_crossings.h"
#include "s2/s2point_span.h"
#include "s2/s2pointutil.h"
#include "s2/s2testing.h"
#include "s2/s2text_format.h"

using s2textformat::MakePointOrDie;

TEST(S2ContainsVertexQuery, Undetermined) {
  S2ContainsVertexQuery q(MakePointOrDie("1:2"));
  q.AddEdge(MakePointOrDie("3:4"), 1);
  q.AddEdge(MakePointOrDie("3:4"), -1);
  EXPECT_EQ(0, q.ContainsSign());
}

TEST(S2ContainsVertexQuery, ContainedWithDuplicates) {
  // The S2::RefDir reference direction points approximately due west.
  // Containment is determined by the unmatched edge immediately clockwise.
  S2ContainsVertexQuery q(MakePointOrDie("0:0"));
  q.AddEdge(MakePointOrDie("3:-3"), -1);
  q.AddEdge(MakePointOrDie("1:-5"), 1);
  q.AddEdge(MakePointOrDie("2:-4"), 1);
  q.AddEdge(MakePointOrDie("1:-5"), -1);
  EXPECT_EQ(1, q.ContainsSign());
}

TEST(S2ContainsVertexQuery, NotContainedWithDuplicates) {
  // The S2::RefDir reference direction points approximately due west.
  // Containment is determined by the unmatched edge immediately clockwise.
  S2ContainsVertexQuery q(MakePointOrDie("1:1"));
  q.AddEdge(MakePointOrDie("1:-5"), 1);
  q.AddEdge(MakePointOrDie("2:-4"), -1);
  q.AddEdge(MakePointOrDie("3:-3"), 1);
  q.AddEdge(MakePointOrDie("1:-5"), -1);
  EXPECT_EQ(-1, q.ContainsSign());
}

// Tests that S2ContainsVertexQuery is compatible with S2::AngleContainsVertex.
TEST(S2ContainsVertexQuery, CompatibleWithAngleContainsVertex) {
  auto points = S2Testing::MakeRegularPoints(MakePointOrDie("89:1"),
                                             S1Angle::Degrees(5), 10);
  S2PointLoopSpan loop(points);
  for (int i = 0; i < loop.size(); ++i) {
    S2Point a = loop[i];
    S2Point b = loop[i + 1];
    S2Point c = loop[i + 2];
    S2ContainsVertexQuery q(b);
    q.AddEdge(a, -1);
    q.AddEdge(c, 1);
    EXPECT_EQ(q.ContainsSign() > 0, S2::AngleContainsVertex(a, b, c));
  }
}

// Tests compatibility with S2::AngleContainsVertex() for a degenerate edge.
TEST(S2ContainsVertexQuery, CompatibleWithAngleContainsVertexDegenerate) {
  S2Point a(1, 0, 0), b(0, 1, 0);
  S2ContainsVertexQuery q(b);
  q.AddEdge(a, -1);
  q.AddEdge(a, 1);
  EXPECT_EQ(q.ContainsSign() > 0, S2::AngleContainsVertex(a, b, a));
}
