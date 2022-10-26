////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///

/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

#include <gtest/gtest.h>
#include <variant>
#include "WCCGraph.h"
#include "GraphsSource.h"

using namespace arangodb::slicegraph;

TEST(GWEN_WCC, test_wcc_2_path) {
  bool const checkDuplicateVertices = true;
  WCCGraph<EmptyEdgeProperties, WCCVertexProperties> graph(
      setup2Path(), checkDuplicateVertices);
  size_t numComponents = graph.writeEquivalenceRelationIntoVertices(graph.wccs);
  ASSERT_EQ(numComponents, 1u);
}

template<typename EdgeProperties>
auto testWCC(WCCGraph<EdgeProperties, WCCVertexProperties>& graph,
             size_t expectedNumComponents) {
  size_t numComponents = graph.writeEquivalenceRelationIntoVertices(graph.wccs);
  ASSERT_EQ(numComponents, expectedNumComponents);
  std::unordered_map<char, uint64_t> value;
  for (auto const& v : graph.vertices) {
    EXPECT_GE(v._key.size(), 1u);
    if (value.contains(v._key[0])) {
      ASSERT_EQ(v.properties.value, value[v._key[0]]);
    } else {
      value[v._key[0]] = v.properties.value;
    }
  }
}

TEST(GWEN_WCC, test_wcc_three_disjoint_directed_cycles) {
  bool const checkDuplicateVertices = true;
  WCCGraph<EmptyEdgeProperties, WCCVertexProperties> graph(
      setupThreeDisjointDirectedCycles(), checkDuplicateVertices);
  size_t const expectedNumComponents = 3;
  testWCC(graph, expectedNumComponents);
}

TEST(GWEN_WCC, test_wcc_three_disjoint_alternating_cycles) {
  bool const checkDuplicateVertices = true;
  WCCGraph<EmptyEdgeProperties, WCCVertexProperties> graph(
      setupThreeDisjointAlternatingCycles(), checkDuplicateVertices);
  size_t const expectedNumComponents = 3;
  testWCC(graph, expectedNumComponents);
}

TEST(GWEN_WCC, test_wcc_one_single_vertex) {
  bool const checkDuplicateVertices = true;
  WCCGraph<EmptyEdgeProperties, WCCVertexProperties> graph(
      setup1SingleVertex(), checkDuplicateVertices);
  size_t const expectedNumComponents = 1;
  testWCC(graph, expectedNumComponents);
}

TEST(GWEN_WCC, test_wcc_two_isolated_vertices) {
  bool const checkDuplicateVertices = true;
  WCCGraph<EmptyEdgeProperties, WCCVertexProperties> graph(
      setup2IsolatedVertices(), checkDuplicateVertices);
  size_t const expectedNumComponents = 2;
  testWCC(graph, expectedNumComponents);
}

TEST(GWEN_WCC, test_wcc_one_directed_tree) {
  bool const checkDuplicateVertices = true;
  WCCGraph<EmptyEdgeProperties, WCCVertexProperties> graph(
      setup1DirectedTree(), checkDuplicateVertices);
  size_t const expectedNumComponents = 1;
  testWCC(graph, expectedNumComponents);
}

TEST(GWEN_WCC, test_wcc_one_alternating_tree) {
  bool const checkDuplicateVertices = true;
  WCCGraph<EmptyEdgeProperties, WCCVertexProperties> graph(
      setup1AlternatingTree(), checkDuplicateVertices);
  size_t const expectedNumComponents = 1;
  testWCC(graph, expectedNumComponents);
}

TEST(GWEN_WCC, test_wcc_2_cliques_connected_by_directed_edge) {
  bool const checkDuplicateVertices = true;
  WCCGraph<EmptyEdgeProperties, WCCVertexProperties> graph(
      setup2CliquesConnectedByDirectedEdge(), checkDuplicateVertices);
  size_t const expectedNumComponents = 1;
  testWCC(graph, expectedNumComponents);
}

auto main(int argc, char** argv) -> int {
  testing::InitGoogleTest();

  return RUN_ALL_TESTS();
}
