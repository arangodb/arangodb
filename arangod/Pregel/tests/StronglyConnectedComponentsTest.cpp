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
/// @author Roman Rabinovich
////////////////////////////////////////////////////////////////////////////////

#include <gtest/gtest.h>
#include "SCCGraph.h"
#include "GraphsSource.h"

TEST(GWEN_SCC, test_readVertices_2path) {
  bool const checkDuplicateVertices = true;
  auto graph = SCCGraph<EmptyEdgeProperties, SCCVertexProperties>(
      setup2Path(), checkDuplicateVertices);
  EXPECT_EQ(graph.vertices.size(), 3u);
  std::set<VertexKey> vertices;
  for (auto const& vertex : graph.vertices) {
    vertices.insert(vertex._key);
  }
  std::set<VertexKey> expectedVertices{"A", "B", "C"};
  EXPECT_EQ(vertices, expectedVertices);
  EXPECT_EQ(graph.getVertexPosition("A"), 0u);
  EXPECT_EQ(graph.getVertexPosition("B"), 1u);
  EXPECT_EQ(graph.getVertexPosition("C"), 2u);
}

TEST(GWEN_SCC, test_readVertices_ThreeDisjointDirectedCycles) {
  bool const checkDuplicateVertices = true;
  auto graph = SCCGraph<EmptyEdgeProperties, SCCVertexProperties>(
      setupThreeDisjointDirectedCycles(), checkDuplicateVertices);
  EXPECT_EQ(graph.vertices.size(), 12u);
  std::set<VertexKey> vertices;
  for (auto const& vertex : graph.vertices) {
    vertices.insert(vertex._key);
  }
  std::set<VertexKey> expectedVertices{"a0", "a1", "a2", "b0", "b1", "b2",
                                       "b3", "c0", "c1", "c2", "c3", "c4"};
  EXPECT_EQ(vertices, expectedVertices);
  EXPECT_EQ(graph.getVertexPosition("a0"), 0u);
}

TEST(GWEN_SCC, test_readVertices_DuplicateVertices) {
  bool const checkDuplicateVertices = true;
  auto graphJSON = setupDuplicateVertices();
  using SCCSimpleGraph = SCCGraph<EmptyEdgeProperties, SCCVertexProperties>;
  EXPECT_THROW((SCCSimpleGraph{graphJSON, checkDuplicateVertices}),
               std::runtime_error);
  EXPECT_NO_THROW((SCCSimpleGraph{graphJSON, false}));
}

TEST(GWEN_SCC, test_number_sccs_undirectedEdge) {
  bool const checkDuplicateVertices = true;
  auto graphJSON = setupUndirectedEdge();
  auto graph = SCCGraph<EmptyEdgeProperties, SCCVertexProperties>(
      graphJSON, checkDuplicateVertices);
  graph.readEdgesBuildSCCs(graphJSON);
  EXPECT_EQ(graph.writeEquivalenceRelationIntoVertices(graph.sccs), 1u);
}

TEST(GWEN_SCC, test_number_sccs_2Path) {
  bool const checkDuplicateVertices = true;
  auto graphJSON = setup2Path();
  auto graph = SCCGraph<EmptyEdgeProperties, SCCVertexProperties>(
      graphJSON, checkDuplicateVertices);
  graph.readEdgesBuildSCCs(graphJSON);
  EXPECT_EQ(graph.writeEquivalenceRelationIntoVertices(graph.sccs), 3u);
}

TEST(GWEN_SCC, test_number_sccs_1SingleVertex) {
  bool const checkDuplicateVertices = true;
  auto graphJSON = setup1SingleVertex();
  auto graph = SCCGraph<EmptyEdgeProperties, SCCVertexProperties>(
      graphJSON, checkDuplicateVertices);
  graph.readEdgesBuildSCCs(graphJSON);
  EXPECT_EQ(graph.writeEquivalenceRelationIntoVertices(graph.sccs), 1u);
}

TEST(GWEN_SCC, test_number_sccs_2IsolatedVertices) {
  bool const checkDuplicateVertices = true;
  auto graphJSON = setup2IsolatedVertices();
  auto graph = SCCGraph<EmptyEdgeProperties, SCCVertexProperties>(
      graphJSON, checkDuplicateVertices);
  graph.readEdgesBuildSCCs(graphJSON);
  EXPECT_EQ(graph.writeEquivalenceRelationIntoVertices(graph.sccs), 2u);
}

TEST(GWEN_SCC, test_number_sccs_directedTree) {
  bool const checkDuplicateVertices = true;
  auto graphJSON = setup1DirectedTree();
  auto graph = SCCGraph<EmptyEdgeProperties, SCCVertexProperties>(
      graphJSON, checkDuplicateVertices);
  graph.readEdgesBuildSCCs(graphJSON);
  EXPECT_EQ(graph.writeEquivalenceRelationIntoVertices(graph.sccs), 15u);
}

TEST(GWEN_SCC, test_number_sccs_alternatingTree) {
  bool const checkDuplicateVertices = true;
  auto graphJSON = setup1AlternatingTree();
  auto graph = SCCGraph<EmptyEdgeProperties, SCCVertexProperties>(
      graphJSON, checkDuplicateVertices);
  graph.readEdgesBuildSCCs(graphJSON);
  EXPECT_EQ(graph.writeEquivalenceRelationIntoVertices(graph.sccs), 15u);
}

TEST(GWEN_SCC, test_number_sccs_2CliquesConnectedByDirectedEdge) {
  bool const checkDuplicateVertices = true;
  auto graphJSON = setup2CliquesConnectedByDirectedEdge();
  auto graph = SCCGraph<EmptyEdgeProperties, SCCVertexProperties>(
      graphJSON, checkDuplicateVertices);
  graph.readEdgesBuildSCCs(graphJSON);
  EXPECT_EQ(graph.writeEquivalenceRelationIntoVertices(graph.sccs), 2u);
}

TEST(GWEN_SCC, test_number_sccs_threeDisjointDirectedCycles) {
  bool const checkDuplicateVertices = true;
  auto graphJSON = setupThreeDisjointDirectedCycles();
  auto graph = SCCGraph<EmptyEdgeProperties, SCCVertexProperties>(
      graphJSON, checkDuplicateVertices);
  graph.readEdgesBuildSCCs(graphJSON);
  EXPECT_EQ(graph.writeEquivalenceRelationIntoVertices(graph.sccs), 3u);
}

auto main(int argc, char** argv) -> int {
  testing::InitGoogleTest();

  return RUN_ALL_TESTS();
}
