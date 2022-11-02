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

#include "s2/s2builderutil_testing.h"

#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include "absl/memory/memory.h"

#include "s2/s2text_format.h"

using absl::make_unique;
using std::unique_ptr;
using std::vector;

using EdgeType = S2Builder::EdgeType;
using Graph = S2Builder::Graph;
using GraphOptions = S2Builder::GraphOptions;

using DegenerateEdges = GraphOptions::DegenerateEdges;
using DuplicateEdges = GraphOptions::DuplicateEdges;
using SiblingPairs = GraphOptions::SiblingPairs;

namespace s2builderutil {

TEST(GraphCloningLayer, MakeIndependentCopy) {
  // Also tests GraphClone.
  GraphClone gc;
  S2Builder builder{S2Builder::Options()};
  GraphOptions graph_options(EdgeType::DIRECTED, DegenerateEdges::DISCARD,
                             DuplicateEdges::MERGE, SiblingPairs::KEEP);
  builder.StartLayer(make_unique<GraphCloningLayer>(graph_options, &gc));
  S2Point v0(1, 0, 0), v1(0, 1, 0);
  builder.set_label(14);
  builder.AddEdge(v0, v1);
  S2Error error;
  EXPECT_TRUE(builder.Build(&error));
  const Graph& g = gc.graph();
  EXPECT_TRUE(graph_options == g.options());
  EXPECT_EQ((vector<S2Point>{v0, v1}), g.vertices());
  EXPECT_EQ((vector<Graph::Edge>{{0, 1}}), g.edges());
  EXPECT_EQ(1, g.input_edge_ids(0).size());
  EXPECT_EQ(0, *g.input_edge_ids(0).begin());
  Graph::LabelFetcher fetcher(g, g.options().edge_type());
  vector<S2Builder::Label> labels;
  fetcher.Fetch(0, &labels);
  EXPECT_EQ((vector<S2Builder::Label>{14}), labels);
  // S2Builder sets a default IsFullPolygonPredicate that returns an error.
  EXPECT_TRUE(g.is_full_polygon_predicate() != nullptr);
}

TEST(GraphAppendingLayer, AppendsTwoGraphs) {
  vector<Graph> graphs;
  vector<unique_ptr<GraphClone>> clones;
  S2Builder builder{S2Builder::Options()};
  builder.StartLayer(make_unique<GraphAppendingLayer>(
      GraphOptions(), &graphs, &clones));
  S2Point v0(1, 0, 0), v1(0, 1, 0);
  builder.AddEdge(v0, v1);
  builder.StartLayer(make_unique<GraphAppendingLayer>(
      GraphOptions(), &graphs, &clones));
  builder.AddEdge(v1, v0);
  S2Error error;
  EXPECT_TRUE(builder.Build(&error));
  EXPECT_EQ(2, graphs.size());
  EXPECT_EQ(2, clones.size());
  EXPECT_EQ(v0, graphs[0].vertex(graphs[0].edge(0).first));
  EXPECT_EQ(v1, graphs[1].vertex(graphs[1].edge(0).first));
}

TEST(IndexMatchingLayer, SameResult) {
  // Two indices with the same edges in different orders.
  auto expected = s2textformat::MakeIndexOrDie(
      "0:0 | 1:0 # 1:1, 2:2, 3:3 # 3:3, 3:4, 4:4");
  auto actual = s2textformat::MakeIndexOrDie(
      "1:0 | 0:0 # 2:2, 3:3 | 1:1, 2:2 # 3:4, 4:4, 3:3");
  S2Builder builder{S2Builder::Options{}};
  GraphOptions graph_options(EdgeType::DIRECTED, DegenerateEdges::KEEP,
                             DuplicateEdges::KEEP, SiblingPairs::KEEP);
  for (int dim = -1; dim < 3; ++dim) {
    builder.StartLayer(make_unique<IndexMatchingLayer>(
        graph_options, expected.get(), dim));
    for (S2Shape* shape : *actual) {
      if (dim < 0 || shape->dimension() == dim) builder.AddShape(*shape);
    }
    S2Error error;
    EXPECT_TRUE(builder.Build(&error)) << error;
  }
}

TEST(IndexMatchingLayer, DifferentResult) {
  // Two indices where edges have different multiplicities.
  auto expected = s2textformat::MakeIndexOrDie(
      "0:0 | 0:0 # 1:1, 2:2, 3:3 | 1:1, 2:2 # 3:3, 3:4, 3:3, 3:4, 4:4");
  auto actual = s2textformat::MakeIndexOrDie(
      "0:0 | 1:0 # 1:1, 2:2, 3:3 # 3:3, 3:4, 4:4");
  S2Builder builder{S2Builder::Options{}};
  GraphOptions graph_options(EdgeType::DIRECTED, DegenerateEdges::KEEP,
                             DuplicateEdges::KEEP, SiblingPairs::KEEP);
  builder.StartLayer(make_unique<IndexMatchingLayer>(graph_options,
                                                     expected.get()));
  for (S2Shape* shape : *actual) builder.AddShape(*shape);
  S2Error error;
  EXPECT_FALSE(builder.Build(&error));
  EXPECT_FALSE(error.ok());
  EXPECT_EQ(error.text(),
            "Missing edges: 3:4, 3:3; 3:3, 3:4; 1:1, 2:2; 0:0, 0:0 "
            "Extra edges: 1:0, 1:0\n");
}

}  // namespace s2builderutil
