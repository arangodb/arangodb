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
#include <vector>

#include <gtest/gtest.h>
#include "s2/id_set_lexicon.h"

using std::vector;

using EdgeType = S2Builder::EdgeType;
using Graph = S2Builder::Graph;
using GraphOptions = S2Builder::GraphOptions;

using DegenerateEdges = GraphOptions::DegenerateEdges;
using DuplicateEdges = GraphOptions::DuplicateEdges;
using SiblingPairs = GraphOptions::SiblingPairs;

using Edge = Graph::Edge;
using EdgeId = Graph::EdgeId;
using InputEdgeId = Graph::InputEdgeId;
using InputEdgeIdSetId = Graph::InputEdgeIdSetId;
using VertexId = Graph::VertexId;

namespace s2builder {

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
  // Test that DISCARD_EXCESS does not merge any remaining duplicate
  // degenerate edges together.
  TestProcessEdges({{0, 0, {1}}, {0, 0, {2}}},
                   {{0, 0, {1}}, {0, 0, {2}}}, &options);
}

TEST(ProcessEdges, DiscardExcessUndirectedIsolatedDegenerateEdges) {
  GraphOptions options(EdgeType::UNDIRECTED, DegenerateEdges::DISCARD_EXCESS,
                       DuplicateEdges::KEEP, SiblingPairs::KEEP);
  // Test that DISCARD_EXCESS does not merge any remaining duplicate
  // undirected degenerate edges together.
  TestProcessEdges({{0, 0, {1}}, {0, 0}, {0, 0, {2}}, {0, 0}},
                   {{0, 0, {1}}, {0, 0}, {0, 0, {2}}, {0, 0}}, &options);
}

TEST(ProcessEdges, DiscardExcessConvertedUndirectedIsolatedDegenerateEdges) {
  GraphOptions options(EdgeType::UNDIRECTED, DegenerateEdges::DISCARD_EXCESS,
                       DuplicateEdges::KEEP, SiblingPairs::REQUIRE);
  // Converting from UNDIRECTED to DIRECTED cuts the edge count in half and
  // merges edge labels.
  TestProcessEdges({{0, 0, {1}}, {0, 0, {2}}, {0, 0, {3}}, {0, 0}},
                   {{0, 0, {1, 2, 3}}, {0, 0, {1, 2, 3}}}, &options);
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

}  // namespace s2builder
