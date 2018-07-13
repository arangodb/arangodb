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
#include "s2/s2loop.h"
#include "s2/s2pointutil.h"
#include "s2/s2text_format.h"

using s2textformat::MakePoint;

TEST(S2ContainsVertexQuery, Undetermined) {
  S2ContainsVertexQuery q(MakePoint("1:2"));
  q.AddEdge(MakePoint("3:4"), 1);
  q.AddEdge(MakePoint("3:4"), -1);
  EXPECT_EQ(0, q.ContainsSign());
}

TEST(S2ContainsVertexQuery, ContainedWithDuplicates) {
  // The S2::Ortho reference direction points approximately due west.
  // Containment is determined by the unmatched edge immediately clockwise.
  S2ContainsVertexQuery q(MakePoint("0:0"));
  q.AddEdge(MakePoint("3:-3"), -1);
  q.AddEdge(MakePoint("1:-5"), 1);
  q.AddEdge(MakePoint("2:-4"), 1);
  q.AddEdge(MakePoint("1:-5"), -1);
  EXPECT_EQ(1, q.ContainsSign());
}

TEST(S2ContainsVertexQuery, NotContainedWithDuplicates) {
  // The S2::Ortho reference direction points approximately due west.
  // Containment is determined by the unmatched edge immediately clockwise.
  S2ContainsVertexQuery q(MakePoint("1:1"));
  q.AddEdge(MakePoint("1:-5"), 1);
  q.AddEdge(MakePoint("2:-4"), -1);
  q.AddEdge(MakePoint("3:-3"), 1);
  q.AddEdge(MakePoint("1:-5"), -1);
  EXPECT_EQ(-1, q.ContainsSign());
}

TEST(S2ContainsVertexQuery, MatchesLoopContainment) {
  // Check that the containment function defined is compatible with S2Loop
  // (which at least currently does not use this class).
  auto loop = S2Loop::MakeRegularLoop(MakePoint("89:-179"),
                                      S1Angle::Degrees(10), 1000);
  for (int i = 1; i <= loop->num_vertices(); ++i) {
    S2ContainsVertexQuery q(loop->vertex(i));
    q.AddEdge(loop->vertex(i - 1), -1);
    q.AddEdge(loop->vertex(i + 1), 1);
    EXPECT_EQ(q.ContainsSign() > 0, loop->Contains(loop->vertex(i)));
  }
}
