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
//
// Most of S2Builder::Graph is tested by the S2Builder::Layer implementations
// rather than here.

#include "s2/s2builder_graph.h"

#include <iosfwd>
#include <memory>
#include <vector>

#include <gtest/gtest.h>
#include "absl/memory/memory.h"
#include "s2/id_set_lexicon.h"
#include "s2/s2builderutil_testing.h"
#include "s2/s2lax_polyline_shape.h"
#include "s2/s2text_format.h"

using absl::make_unique;
using s2builderutil::GraphClone;
using s2builderutil::GraphCloningLayer;
using s2textformat::MakeLaxPolylineOrDie;
using s2textformat::ParsePointsOrDie;
using std::vector;

using EdgeType = S2Builder::EdgeType;
using Graph = S2Builder::Graph;
using GraphOptions = S2Builder::GraphOptions;

using DegenerateEdges = GraphOptions::DegenerateEdges;
using DuplicateEdges = GraphOptions::DuplicateEdges;
using SiblingPairs = GraphOptions::SiblingPairs;

using DegenerateBoundaries = Graph::DegenerateBoundaries;
using Edge = Graph::Edge;
using EdgeId = Graph::EdgeId;
using InputEdgeId = Graph::InputEdgeId;
using InputEdgeIdSetId = Graph::InputEdgeIdSetId;
using LabelSetId = Graph::LabelSetId;
using LoopType = Graph::LoopType;
using PolylineType = Graph::PolylineType;
using VertexId = Graph::VertexId;

namespace s2builder {

TEST(Graph, LabelsRequestedButNotProvided) {
  // Tests the situation where labels are requested but none were provided.
  GraphOptions options(EdgeType::DIRECTED, DegenerateEdges::KEEP,
                       DuplicateEdges::KEEP, SiblingPairs::KEEP);
  vector<S2Point> vertices{S2Point(1, 0, 0)};
  vector<Edge> edges{{0, 0}};
  vector<InputEdgeIdSetId> input_edge_id_set_ids{0};
  IdSetLexicon input_edge_id_set_lexicon, label_set_lexicon;
  vector<LabelSetId> label_set_ids;  // Empty means no labels are present.
  Graph g{
    options, &vertices, &edges, &input_edge_id_set_ids,
    &input_edge_id_set_lexicon, &label_set_ids, &label_set_lexicon, nullptr};
  EXPECT_TRUE(g.label_set_ids().empty());
  EXPECT_EQ(g.label_set_id(0), IdSetLexicon::EmptySetId());
  EXPECT_EQ(g.labels(0).size(), 0);  // Labels for input edge 0.
  Graph::LabelFetcher fetcher(g, EdgeType::DIRECTED);
  vector<S2Builder::Label> labels;
  fetcher.Fetch(0, &labels);         // Labels for graph edge 0.
  EXPECT_TRUE(labels.empty());
}

TEST(GetDirectedLoops, DegenerateEdges) {
  GraphClone gc;
  S2Builder builder{S2Builder::Options()};
  GraphOptions graph_options(
      EdgeType::DIRECTED, DegenerateEdges::DISCARD_EXCESS,
      DuplicateEdges::KEEP, SiblingPairs::KEEP);
  builder.StartLayer(make_unique<GraphCloningLayer>(graph_options, &gc));
  builder.AddShape(*MakeLaxPolylineOrDie("1:1, 1:1"));
  builder.AddShape(*MakeLaxPolylineOrDie("0:0, 0:2, 2:2, 2:0, 0:0"));
  builder.AddShape(*MakeLaxPolylineOrDie("0:3, 3:3, 0:3"));
  S2Error error;
  EXPECT_TRUE(builder.Build(&error));
  const Graph& g = gc.graph();
  vector<vector<EdgeId>> loops;
  ASSERT_TRUE(g.GetDirectedLoops(LoopType::SIMPLE, &loops, &error));
  ASSERT_EQ(loops.size(), 3);
  EXPECT_EQ(loops[0].size(), 1);
  EXPECT_EQ(loops[1].size(), 4);
  EXPECT_EQ(loops[2].size(), 2);
}

TEST(GetDirectedComponents, DegenerateEdges) {
  GraphClone gc;
  S2Builder builder{S2Builder::Options()};
  GraphOptions graph_options(
      EdgeType::DIRECTED, DegenerateEdges::DISCARD_EXCESS,
      DuplicateEdges::KEEP, SiblingPairs::CREATE);
  builder.StartLayer(make_unique<GraphCloningLayer>(graph_options, &gc));
  builder.AddShape(*MakeLaxPolylineOrDie("1:1, 1:1"));
  builder.AddShape(*MakeLaxPolylineOrDie("0:0, 0:2, 2:2, 2:0, 0:0"));
  S2Error error;
  EXPECT_TRUE(builder.Build(&error));
  const Graph& g = gc.graph();
  vector<Graph::DirectedComponent> components;
  ASSERT_TRUE(g.GetDirectedComponents(DegenerateBoundaries::KEEP, &components,
                                      &error));
  ASSERT_EQ(components.size(), 2);
  EXPECT_EQ(components[0].size(), 1);
  EXPECT_EQ(components[0][0].size(), 1);
  EXPECT_EQ(components[1].size(), 2);
  EXPECT_EQ(components[1][0].size(), 4);
  EXPECT_EQ(components[1][1].size(), 4);
}

TEST(GetUndirectedComponents, DegenerateEdges) {
  GraphClone gc;
  S2Builder builder{S2Builder::Options()};
  GraphOptions graph_options(
      EdgeType::UNDIRECTED, DegenerateEdges::DISCARD_EXCESS,
      DuplicateEdges::KEEP, SiblingPairs::DISCARD_EXCESS);
  builder.StartLayer(make_unique<GraphCloningLayer>(graph_options, &gc));
  builder.AddShape(*MakeLaxPolylineOrDie("1:1, 1:1"));
  builder.AddShape(*MakeLaxPolylineOrDie("0:0, 0:2, 2:2, 2:0, 0:0"));
  S2Error error;
  EXPECT_TRUE(builder.Build(&error));
  const Graph& g = gc.graph();
  vector<Graph::UndirectedComponent> components;
  ASSERT_TRUE(g.GetUndirectedComponents(LoopType::CIRCUIT, &components,
                                      &error));
  // The result consists of two components, each with two complements.  Each
  // complement in this example has exactly one loop.  The loops in both
  // complements of the first component have 1 vertex, while the loops in both
  // complements of the second component have 4 vertices.
  ASSERT_EQ(components.size(), 2);
  EXPECT_EQ(components[0][0].size(), 1);
  EXPECT_EQ(components[0][0][0].size(), 1);
  EXPECT_EQ(components[0][1].size(), 1);
  EXPECT_EQ(components[0][1][0].size(), 1);
  EXPECT_EQ(components[1][0].size(), 1);
  EXPECT_EQ(components[1][0][0].size(), 4);
  EXPECT_EQ(components[1][1].size(), 1);
  EXPECT_EQ(components[1][1][0].size(), 4);
}

TEST(GetPolylines, UndirectedDegeneratePaths) {
  GraphClone gc;
  S2Builder builder{S2Builder::Options()};
  GraphOptions graph_options(
      EdgeType::UNDIRECTED, DegenerateEdges::KEEP,
      DuplicateEdges::KEEP, SiblingPairs::KEEP);
  builder.StartLayer(make_unique<GraphCloningLayer>(graph_options, &gc));
  builder.AddShape(*MakeLaxPolylineOrDie("1:1, 1:1"));
  builder.AddShape(*MakeLaxPolylineOrDie("0:0, 0:0, 0:1, 0:1, 0:2, 0:2"));
  builder.AddShape(*MakeLaxPolylineOrDie("1:1, 1:1"));
  S2Error error;
  EXPECT_TRUE(builder.Build(&error));
  const Graph& g = gc.graph();
  auto polylines = g.GetPolylines(PolylineType::PATH);
  EXPECT_EQ(polylines.size(), 7);
}

TEST(GetPolylines, UndirectedDegenerateWalks) {
  GraphClone gc;
  S2Builder builder{S2Builder::Options()};
  GraphOptions graph_options(
      EdgeType::UNDIRECTED, DegenerateEdges::KEEP,
      DuplicateEdges::KEEP, SiblingPairs::KEEP);
  builder.StartLayer(make_unique<GraphCloningLayer>(graph_options, &gc));
  builder.AddShape(*MakeLaxPolylineOrDie("1:1, 1:1"));
  builder.AddShape(*MakeLaxPolylineOrDie("0:0, 0:0, 0:1, 0:1, 0:2, 0:2"));
  builder.AddShape(*MakeLaxPolylineOrDie("1:1, 1:1"));
  S2Error error;
  EXPECT_TRUE(builder.Build(&error));
  const Graph& g = gc.graph();
  auto polylines = g.GetPolylines(PolylineType::WALK);
  EXPECT_EQ(polylines.size(), 2);
  EXPECT_EQ(polylines[0].size(), 2);
  EXPECT_EQ(polylines[1].size(), 5);
}

struct TestEdge {
  VertexId first;
  VertexId second;
  vector<InputEdgeId> input_ids;
};

namespace {

std::ostream& operator<<(std::ostream& os, const Edge& edge) {
  return os << "(" << edge.first << ", " << edge.second << ")";
}

std::ostream& operator<<(std::ostream& os, const vector<InputEdgeId>& v) {
  os << "{";
  for (int i = 0; i < v.size(); ++i) {
    if (i > 0) os << ", ";
    os << v[i];
  }
  return os << "}";
}

void TestProcessEdges(const vector<TestEdge>& input,
                      const vector<TestEdge>& expected,
                      GraphOptions* options,
                      S2Error::Code expected_code = S2Error::OK) {
  vector<Edge> edges;
  vector<InputEdgeIdSetId> input_id_set_ids;
  IdSetLexicon id_set_lexicon;
  for (const auto& e : input) {
    edges.push_back(Edge(e.first, e.second));
    input_id_set_ids.push_back(id_set_lexicon.Add(e.input_ids));
  }
  S2Error error;
  Graph::ProcessEdges(options, &edges, &input_id_set_ids, &id_set_lexicon,
                      &error);
  EXPECT_EQ(expected_code, error.code());
  EXPECT_EQ(edges.size(), input_id_set_ids.size());
  for (int i = 0; i < expected.size(); ++i) {
    const auto& e = expected[i];
    ASSERT_LT(i, edges.size()) << "Not enough output edges";
    ASSERT_EQ(Edge(e.first, e.second), edges[i]) << "(edge " << i << ")";
    auto id_set = id_set_lexicon.id_set(input_id_set_ids[i]);
    vector<InputEdgeId> actual_ids(id_set.begin(), id_set.end());
    ASSERT_EQ(e.input_ids, actual_ids) << "(edge " << i << ")";
  }
  ASSERT_EQ(expected.size(), edges.size()) << "Too many output edges";
}

}  // namespace

TEST(ProcessEdges, DiscardDegenerateEdges) {
  GraphOptions options(EdgeType::DIRECTED, DegenerateEdges::DISCARD,
                       DuplicateEdges::KEEP, SiblingPairs::KEEP);
  TestProcessEdges({{0, 0}, {0, 0}}, {}, &options);
}

TEST(ProcessEdges, KeepDuplicateDegenerateEdges) {
  GraphOptions options(EdgeType::DIRECTED, DegenerateEdges::KEEP,
                       DuplicateEdges::KEEP, SiblingPairs::KEEP);
  TestProcessEdges({{0, 0}, {0, 0}}, {{0, 0}, {0, 0}}, &options);
}

TEST(ProcessEdges, MergeDuplicateDegenerateEdges) {
  GraphOptions options(EdgeType::DIRECTED, DegenerateEdges::KEEP,
                       DuplicateEdges::MERGE, SiblingPairs::KEEP);
  TestProcessEdges({{0, 0, {1}}, {0, 0, {2}}}, {{0, 0, {1, 2}}}, &options);
}

TEST(ProcessEdges, MergeUndirectedDuplicateDegenerateEdges) {
  // Edge count should be reduced to 2 (i.e., one undirected edge), and all
  // labels should be merged.
  GraphOptions options(EdgeType::UNDIRECTED, DegenerateEdges::KEEP,
                       DuplicateEdges::MERGE, SiblingPairs::KEEP);
  TestProcessEdges({{0, 0, {1}}, {0, 0}, {0, 0}, {0, 0, {2}}},
                   {{0, 0, {1, 2}}, {0, 0, {1, 2}}}, &options);
}

TEST(ProcessEdges, ConvertedUndirectedDegenerateEdges) {
  // Converting from UNDIRECTED to DIRECTED cuts the edge count in half and
  // merges any edge labels.
  GraphOptions options(EdgeType::UNDIRECTED, DegenerateEdges::KEEP,
                       DuplicateEdges::KEEP, SiblingPairs::REQUIRE);
  TestProcessEdges({{0, 0, {1}}, {0, 0}, {0, 0}, {0, 0, {2}}},
                   {{0, 0, {1, 2}}, {0, 0, {1, 2}}}, &options);
  EXPECT_EQ(EdgeType::DIRECTED, options.edge_type());
}

TEST(ProcessEdges, MergeConvertedUndirectedDuplicateDegenerateEdges) {
  // Like the test above, except that we also merge duplicates.
  GraphOptions options(EdgeType::UNDIRECTED, DegenerateEdges::KEEP,
                       DuplicateEdges::MERGE, SiblingPairs::REQUIRE);
  TestProcessEdges({{0, 0, {1}}, {0, 0}, {0, 0}, {0, 0, {2}}},
                   {{0, 0, {1, 2}}}, &options);
  EXPECT_EQ(EdgeType::DIRECTED, options.edge_type());
}

TEST(ProcessEdges, DiscardExcessConnectedDegenerateEdges) {
  GraphOptions options(EdgeType::DIRECTED, DegenerateEdges::DISCARD_EXCESS,
                       DuplicateEdges::KEEP, SiblingPairs::KEEP);
  // Test that degenerate edges are discarded if they are connnected to any
  // non-degenerate edges (whether they are incoming or outgoing, and whether
  // they are lexicographically before or after the degenerate edge).
  TestProcessEdges({{0, 0}, {0, 1}}, {{0, 1}}, &options);
  TestProcessEdges({{0, 0}, {1, 0}}, {{1, 0}}, &options);
  TestProcessEdges({{0, 1}, {1, 1}}, {{0, 1}}, &options);
  TestProcessEdges({{1, 0}, {1, 1}}, {{1, 0}}, &options);
}

TEST(ProcessEdges, DiscardExcessIsolatedDegenerateEdges) {
  GraphOptions options(EdgeType::DIRECTED, DegenerateEdges::DISCARD_EXCESS,
                       DuplicateEdges::KEEP, SiblingPairs::KEEP);
  // Test that DISCARD_EXCESS merges any duplicate degenerate edges together.
  TestProcessEdges({{0, 0, {1}}, {0, 0, {2}}},
                   {{0, 0, {1, 2}}}, &options);
}

TEST(ProcessEdges, DiscardExcessUndirectedIsolatedDegenerateEdges) {
  GraphOptions options(EdgeType::UNDIRECTED, DegenerateEdges::DISCARD_EXCESS,
                       DuplicateEdges::KEEP, SiblingPairs::KEEP);
  // Test that DISCARD_EXCESS merges any duplicate undirected degenerate edges
  // together.
  TestProcessEdges({{0, 0, {1}}, {0, 0}, {0, 0, {2}}, {0, 0}},
                   {{0, 0, {1, 2}}, {0, 0, {1, 2}}}, &options);
}

TEST(ProcessEdges, DiscardExcessConvertedUndirectedIsolatedDegenerateEdges) {
  GraphOptions options(EdgeType::UNDIRECTED, DegenerateEdges::DISCARD_EXCESS,
                       DuplicateEdges::KEEP, SiblingPairs::REQUIRE);
  // Test that DISCARD_EXCESS with SiblingPairs::REQUIRE merges any duplicate
  // edges together and converts the edges from UNDIRECTED to DIRECTED.
  TestProcessEdges({{0, 0, {1}}, {0, 0, {2}}, {0, 0, {3}}, {0, 0}},
                   {{0, 0, {1, 2, 3}}}, &options);
  EXPECT_EQ(EdgeType::DIRECTED, options.edge_type());
}

TEST(ProcessEdges, SiblingPairsDiscardMergesDegenerateEdgeLabels) {
  // Test that when SiblingPairs::DISCARD or SiblingPairs::DISCARD_EXCESS
  // is specified, the edge labels of degenerate edges are merged together
  // (for consistency, since these options merge the labels of all
  // non-degenerate edges as well).
  GraphOptions options(EdgeType::DIRECTED, DegenerateEdges::KEEP,
                       DuplicateEdges::KEEP, SiblingPairs::DISCARD);
  TestProcessEdges({{0, 0, {1}}, {0, 0, {2}}, {0, 0, {3}}},
                   {{0, 0, {1, 2, 3}}, {0, 0, {1, 2, 3}}, {0, 0, {1, 2, 3}}},
                   &options);
  options.set_sibling_pairs(SiblingPairs::DISCARD_EXCESS);
  TestProcessEdges({{0, 0, {1}}, {0, 0, {2}}, {0, 0, {3}}},
                   {{0, 0, {1, 2, 3}}, {0, 0, {1, 2, 3}}, {0, 0, {1, 2, 3}}},
                   &options);
}

TEST(ProcessEdges, KeepSiblingPairs) {
  GraphOptions options(EdgeType::DIRECTED, DegenerateEdges::DISCARD,
                       DuplicateEdges::KEEP, SiblingPairs::KEEP);
  TestProcessEdges({{0, 1}, {1, 0}}, {{0, 1}, {1, 0}}, &options);
}

TEST(ProcessEdges, MergeDuplicateSiblingPairs) {
  GraphOptions options(EdgeType::DIRECTED, DegenerateEdges::DISCARD,
                       DuplicateEdges::MERGE, SiblingPairs::KEEP);
  TestProcessEdges({{0, 1}, {0, 1}, {1, 0}}, {{0, 1}, {1, 0}}, &options);
}

TEST(ProcessEdges, DiscardSiblingPairs) {
  // Check that matched pairs are discarded, leaving behind any excess edges.
  GraphOptions options(EdgeType::DIRECTED, DegenerateEdges::DISCARD,
                       DuplicateEdges::KEEP, SiblingPairs::DISCARD);
  TestProcessEdges({{0, 1}, {1, 0}}, {}, &options);
  TestProcessEdges({{0, 1}, {0, 1}, {1, 0}, {1, 0}}, {}, &options);
  TestProcessEdges({{0, 1}, {0, 1}, {0, 1}, {1, 0}},
                   {{0, 1}, {0, 1}}, &options);
  TestProcessEdges({{0, 1}, {1, 0}, {1, 0}, {1, 0}},
                   {{1, 0}, {1, 0}}, &options);
}

TEST(ProcessEdges, DiscardSiblingPairsMergeDuplicates) {
  // Check that matched pairs are discarded, and then any remaining edges
  // are merged.
  GraphOptions options(EdgeType::DIRECTED, DegenerateEdges::DISCARD,
                       DuplicateEdges::MERGE, SiblingPairs::DISCARD);
  TestProcessEdges({{0, 1}, {0, 1}, {1, 0}, {1, 0}}, {}, &options);
  TestProcessEdges({{0, 1}, {0, 1}, {0, 1}, {1, 0}}, {{0, 1}}, &options);
  TestProcessEdges({{0, 1}, {1, 0}, {1, 0}, {1, 0}}, {{1, 0}}, &options);
}

TEST(ProcessEdges, DiscardUndirectedSiblingPairs) {
  // An undirected sibling pair consists of four edges, two in each direction
  // (see s2builder.h).  Since undirected edges always come in pairs, this
  // means that the result always consists of either 0 or 2 edges.
  GraphOptions options(EdgeType::UNDIRECTED, DegenerateEdges::DISCARD,
                       DuplicateEdges::KEEP, SiblingPairs::DISCARD);
  TestProcessEdges({{0, 1}, {1, 0}}, {{0, 1}, {1, 0}}, &options);
  TestProcessEdges({{0, 1}, {0, 1}, {1, 0}, {1, 0}}, {}, &options);
  TestProcessEdges({{0, 1}, {0, 1}, {0, 1}, {1, 0}, {1, 0}, {1, 0}},
                   {{0, 1}, {1, 0}}, &options);
}

TEST(ProcessEdges, DiscardExcessSiblingPairs) {
  // Like SiblingPairs::DISCARD, except that one sibling pair is kept if the
  // result would otherwise be empty.
  GraphOptions options(EdgeType::DIRECTED, DegenerateEdges::DISCARD,
                       DuplicateEdges::KEEP, SiblingPairs::DISCARD_EXCESS);
  TestProcessEdges({{0, 1}, {1, 0}}, {{0, 1}, {1, 0}}, &options);
  TestProcessEdges({{0, 1}, {0, 1}, {1, 0}, {1, 0}},
                   {{0, 1}, {1, 0}}, &options);
  TestProcessEdges({{0, 1}, {0, 1}, {0, 1}, {1, 0}},
                   {{0, 1}, {0, 1}}, &options);
  TestProcessEdges({{0, 1}, {1, 0}, {1, 0}, {1, 0}},
                   {{1, 0}, {1, 0}}, &options);
}

TEST(ProcessEdges, DiscardExcessSiblingPairsMergeDuplicates) {
  // Like SiblingPairs::DISCARD, except that one sibling pair is kept if the
  // result would otherwise be empty.
  GraphOptions options(EdgeType::DIRECTED, DegenerateEdges::DISCARD,
                       DuplicateEdges::MERGE, SiblingPairs::DISCARD_EXCESS);
  TestProcessEdges({{0, 1}, {0, 1}, {1, 0}, {1, 0}},
                   {{0, 1}, {1, 0}}, &options);
  TestProcessEdges({{0, 1}, {0, 1}, {0, 1}, {1, 0}}, {{0, 1}}, &options);
  TestProcessEdges({{0, 1}, {1, 0}, {1, 0}, {1, 0}}, {{1, 0}}, &options);
}

TEST(ProcessEdges, DiscardExcessUndirectedSiblingPairs) {
  // Like SiblingPairs::DISCARD, except that one undirected sibling pair
  // (4 edges) is kept if the result would otherwise be empty.
  GraphOptions options(EdgeType::UNDIRECTED, DegenerateEdges::DISCARD,
                       DuplicateEdges::KEEP, SiblingPairs::DISCARD_EXCESS);
  TestProcessEdges({{0, 1}, {1, 0}}, {{0, 1}, {1, 0}}, &options);
  TestProcessEdges({{0, 1}, {0, 1}, {1, 0}, {1, 0}},
                   {{0, 1}, {0, 1}, {1, 0}, {1, 0}}, &options);
  TestProcessEdges({{0, 1}, {0, 1}, {0, 1}, {1, 0}, {1, 0}, {1, 0}},
                   {{0, 1}, {1, 0}}, &options);
}

TEST(ProcessEdges, CreateSiblingPairs) {
  GraphOptions options(EdgeType::DIRECTED, DegenerateEdges::DISCARD,
                       DuplicateEdges::KEEP, SiblingPairs::CREATE);
  TestProcessEdges({{0, 1}}, {{0, 1}, {1, 0}}, &options);
  TestProcessEdges({{0, 1}, {0, 1}},
                   {{0, 1}, {0, 1}, {1, 0}, {1, 0}}, &options);
}

TEST(ProcessEdges, RequireSiblingPairs) {
  // Like SiblingPairs::CREATE, but generates an error.
  GraphOptions options(EdgeType::DIRECTED, DegenerateEdges::DISCARD,
                       DuplicateEdges::KEEP, SiblingPairs::REQUIRE);
  TestProcessEdges({{0, 1}, {1, 0}}, {{0, 1}, {1, 0}}, &options);
  TestProcessEdges({{0, 1}}, {{0, 1}, {1, 0}}, &options,
                   S2Error::BUILDER_MISSING_EXPECTED_SIBLING_EDGES);
}

TEST(ProcessEdges, CreateUndirectedSiblingPairs) {
  // An undirected sibling pair consists of 4 edges, but SiblingPairs::CREATE
  // also converts the graph to EdgeType::DIRECTED and cuts the number of
  // edges in half.
  GraphOptions options(EdgeType::DIRECTED, DegenerateEdges::DISCARD,
                       DuplicateEdges::KEEP, SiblingPairs::CREATE);
  TestProcessEdges({{0, 1}, {1, 0}},
                   {{0, 1}, {1, 0}}, &options);
  EXPECT_EQ(EdgeType::DIRECTED, options.edge_type());

  options.set_edge_type(EdgeType::UNDIRECTED);
  TestProcessEdges({{0, 1}, {0, 1}, {1, 0}, {1, 0}},
                   {{0, 1}, {1, 0}}, &options);
  EXPECT_EQ(EdgeType::DIRECTED, options.edge_type());

  options.set_edge_type(EdgeType::UNDIRECTED);
  TestProcessEdges({{0, 1}, {0, 1}, {0, 1}, {1, 0}, {1, 0}, {1, 0}},
                   {{0, 1}, {0, 1}, {1, 0}, {1, 0}}, &options);
  EXPECT_EQ(EdgeType::DIRECTED, options.edge_type());
}

TEST(ProcessEdges, CreateSiblingPairsMergeDuplicates) {
  GraphOptions options(EdgeType::DIRECTED, DegenerateEdges::DISCARD,
                       DuplicateEdges::MERGE, SiblingPairs::CREATE);
  TestProcessEdges({{0, 1}}, {{0, 1}, {1, 0}}, &options);
  TestProcessEdges({{0, 1}, {0, 1}}, {{0, 1}, {1, 0}}, &options);
}

TEST(ProcessEdges, CreateUndirectedSiblingPairsMergeDuplicates) {
  GraphOptions options(EdgeType::DIRECTED, DegenerateEdges::DISCARD,
                       DuplicateEdges::MERGE, SiblingPairs::CREATE);
  TestProcessEdges({{0, 1}, {1, 0}},
                   {{0, 1}, {1, 0}}, &options);
  EXPECT_EQ(EdgeType::DIRECTED, options.edge_type());

  options.set_edge_type(EdgeType::UNDIRECTED);
  TestProcessEdges({{0, 1}, {0, 1}, {0, 1}, {1, 0}, {1, 0}, {1, 0}},
                   {{0, 1}, {1, 0}}, &options);
  EXPECT_EQ(EdgeType::DIRECTED, options.edge_type());
}

void TestMakeSubgraph(
    const Graph& g,
    IdSetLexicon new_input_edge_id_set_lexicon,
    GraphOptions new_options,
    vector<Edge> new_edges,
    vector<InputEdgeIdSetId> new_input_edge_id_set_ids,
    const GraphOptions& expected_options,
    const vector<Edge>& expected_edges,
    const vector<InputEdgeIdSetId>& expected_input_edge_id_set_ids) {
  S2Error error;
  Graph new_g = g.MakeSubgraph(
      new_options, &new_edges, &new_input_edge_id_set_ids,
      &new_input_edge_id_set_lexicon, nullptr, &error);

  // Some parts of the graph should be the same.
  EXPECT_TRUE(&new_g.vertices() == &g.vertices());
  EXPECT_EQ(&new_g.label_set_ids(), &g.label_set_ids());
  EXPECT_EQ(&new_g.label_set_lexicon(), &g.label_set_lexicon());

  // Some parts of the graph should use the provided underlying storage.
  EXPECT_TRUE(&new_g.edges() == &new_edges);
  EXPECT_TRUE(&new_g.input_edge_id_set_ids() == &new_input_edge_id_set_ids);
  EXPECT_TRUE(&new_g.input_edge_id_set_lexicon() ==
              &new_input_edge_id_set_lexicon);

  // The new graph should have the expected options.
  EXPECT_EQ(new_g.options().edge_type(), expected_options.edge_type());
  EXPECT_EQ(new_g.options().degenerate_edges(),
            expected_options.degenerate_edges());
  EXPECT_EQ(new_g.options().duplicate_edges(),
            expected_options.duplicate_edges());
  EXPECT_EQ(new_g.options().sibling_pairs(), expected_options.sibling_pairs());

  // The edges should be updated according to the requested options.
  EXPECT_EQ(new_g.edges(), expected_edges);
  EXPECT_EQ(new_g.input_edge_id_set_ids(), expected_input_edge_id_set_ids);
  EXPECT_EQ(&new_g.input_edge_id_set_lexicon(),
            &new_input_edge_id_set_lexicon);
}

TEST(MakeSubgraph, UndirectedToUndirected) {
  // Test that MakeSubgraph() doesn't transform edges into edge pairs when
  // creating an undirected subgraph of an undirected graph.
  GraphOptions options(EdgeType::UNDIRECTED, DegenerateEdges::KEEP,
                       DuplicateEdges::KEEP, SiblingPairs::KEEP);
  auto vertices = ParsePointsOrDie("0:0, 0:1, 1:1");
  vector<Edge> edges{{0, 0}, {0, 0}, {1, 2}, {2, 1}};
  vector<InputEdgeIdSetId> input_edge_id_set_ids{0, 0, 1, 1};
  vector<LabelSetId> label_set_ids;
  IdSetLexicon input_edge_id_set_lexicon, label_set_lexicon;
  Graph graph{
    options, &vertices, &edges, &input_edge_id_set_ids,
    &input_edge_id_set_lexicon, &label_set_ids, &label_set_lexicon, nullptr};

  // Now create a subgraph with undirected edges but different options.
  GraphOptions new_options(EdgeType::UNDIRECTED, DegenerateEdges::DISCARD,
                           DuplicateEdges::KEEP, SiblingPairs::KEEP);
  vector<Edge> expected_edges{{1, 2}, {2, 1}};
  vector<InputEdgeIdSetId> expected_input_edge_id_set_ids{1, 1};
  TestMakeSubgraph(
      graph, input_edge_id_set_lexicon,
      new_options, edges, input_edge_id_set_ids,
      new_options, expected_edges, expected_input_edge_id_set_ids);
}

TEST(MakeSubgraph, DirectedToUndirected) {
  // Test transforming directed edges into undirected edges (which doubles the
  // number of edges).
  GraphOptions options(EdgeType::DIRECTED, DegenerateEdges::KEEP,
                       DuplicateEdges::KEEP, SiblingPairs::KEEP);
  auto vertices = ParsePointsOrDie("0:0, 0:1, 1:1");
  vector<Edge> edges{{0, 0}, {0, 1}, {1, 2}, {1, 2}, {2, 1}};
  vector<InputEdgeIdSetId> input_edge_id_set_ids{1, 2, 3, 3, 3};
  vector<LabelSetId> label_set_ids;
  IdSetLexicon input_edge_id_set_lexicon, label_set_lexicon;
  Graph graph{
    options, &vertices, &edges, &input_edge_id_set_ids,
    &input_edge_id_set_lexicon, &label_set_ids, &label_set_lexicon, nullptr};

  // Now create a subgraph with undirected edges and different options.
  GraphOptions new_options(EdgeType::UNDIRECTED, DegenerateEdges::KEEP,
                           DuplicateEdges::KEEP, SiblingPairs::DISCARD_EXCESS);
  vector<Edge> expected_edges{
    {0, 0}, {0, 0},  // Undirected degenerate edge.
    {0, 1}, {1, 0},  // Undirected edge.
    {1, 2}, {2, 1}   // Undirected edge after discarding sibling pair.
  };
  vector<InputEdgeIdSetId> expected_input_edge_id_set_ids{
    1, 1, 2, IdSetLexicon::EmptySetId(), 3, 3
  };
  TestMakeSubgraph(
      graph, input_edge_id_set_lexicon,
      new_options, edges, input_edge_id_set_ids,
      new_options, expected_edges, expected_input_edge_id_set_ids);
}

}  // namespace s2builder
