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
#include <Inspection/VPackPure.h>
#include "WCCGraph.h"
#include "DisjointSet.h"

TEST(GWEN_DISJOINT_SET, test_constructor) {
  auto ds = DisjointSet(10);
  EXPECT_EQ(ds.capacity(), static_cast<size_t>(10));
  auto ds0 = DisjointSet();
  EXPECT_EQ(ds0.capacity(), static_cast<size_t>(0));
}

TEST(GWEN_DISJOINT_SET, test_add_singleton) {
  auto ds = DisjointSet();
  EXPECT_TRUE(ds.addSingleton(2));
  EXPECT_EQ(ds.capacity(), 3u);
  EXPECT_FALSE(ds.addSingleton(2));
  EXPECT_TRUE(ds.addSingleton(1));
  EXPECT_EQ(ds.capacity(), 3u);
  EXPECT_TRUE(ds.addSingleton(4));
  EXPECT_EQ(ds.capacity(), 5u);
  EXPECT_TRUE(ds.addSingleton(0));
  EXPECT_EQ(ds.capacity(), 5u);
  EXPECT_TRUE(ds.addSingleton(5, 6));
  EXPECT_EQ(ds.capacity(), 6u);
  EXPECT_TRUE(ds.addSingleton(6, 8));
  EXPECT_EQ(ds.capacity(), 8u);
  EXPECT_TRUE(ds.addSingleton(3, 5));
  EXPECT_EQ(ds.capacity(), 8u);
  EXPECT_TRUE(ds.addSingleton(7, 5));
  EXPECT_EQ(ds.capacity(), 8u);
}

TEST(GWEN_DISJOINT_SET, test_merge_and_representatives) {
  auto ds = DisjointSet(10);
  for (size_t i = 0; i < 10; ++i) {
    ds.addSingleton(i);
  }
  ds.merge(0, 0);
  EXPECT_EQ(ds.representative(0), 0u);
  ds.merge(0, 1);
  // 0 and 1 have the same rank, so the second parameter
  // becomes the representative
  EXPECT_EQ(ds.representative(0), 1u);
  EXPECT_EQ(ds.representative(1), 1u);
  ds.merge(0, 1);
  EXPECT_EQ(ds.representative(0), 1u);
  EXPECT_EQ(ds.representative(1), 1u);
  ds.merge(1, 2);
  EXPECT_EQ(ds.representative(0), 1u);
  EXPECT_EQ(ds.representative(1), 1u);
  // 1 has a higher rank
  EXPECT_EQ(ds.representative(2), 1u);
  ds.merge(3, 4);
  ds.merge(5, 6);
  ds.merge(3, 5);
  ds.merge(2, 3);
  // the representative of 3 has a higher rank than that of 2
  EXPECT_EQ(ds.representative(2), 6u);
}

SharedSlice setup2Path() {
  return
      R"({ "vertices": [ {"_key": "A", "value": 5},
                         {"_key": "B", "value": 10},
                         {"_key": "C", "value": 15} ],
           "edges":    [ {"_key": "", "_from": "A", "_to": "B"},
                         {"_key": "", "_from": "B", "_to": "C"} ] })"_vpack;
}

SharedSlice setupThreeDisjointDirectedCycles() {
  return
      R"({ "vertices": [ {"_key": "a0", "value": 0},
                         {"_key": "a1", "value": 0},
                         {"_key": "a2", "value": 0},
                         {"_key": "b0", "value": 0},
                         {"_key": "b1", "value": 0},
                         {"_key": "b2", "value": 0},
                         {"_key": "b3", "value": 0},
                         {"_key": "c0", "value": 0},
                         {"_key": "c1", "value": 0},
                         {"_key": "c2", "value": 0},
                         {"_key": "c3", "value": 0},
                         {"_key": "c4", "value": 0}],
           "edges":    [ {"_key": "", "_from": "a0", "_to": "a1"},
                         {"_key": "", "_from": "a1", "_to": "a2"},
                         {"_key": "", "_from": "a2", "_to": "a0"},
                         {"_key": "", "_from": "b0", "_to": "b1"},
                         {"_key": "", "_from": "b1", "_to": "b2"},
                         {"_key": "", "_from": "b2", "_to": "b3"},
                         {"_key": "", "_from": "b3", "_to": "b0"},
                         {"_key": "", "_from": "c0", "_to": "c1"},
                         {"_key": "", "_from": "c1", "_to": "c2"},
                         {"_key": "", "_from": "c2", "_to": "c3"},
                         {"_key": "", "_from": "c3", "_to": "c4"},
                         {"_key": "", "_from": "c4", "_to": "c0"}] })"_vpack;
}

SharedSlice setupThreeDisjointAlternatingCycles() {
  return
      R"({ "vertices": [ {"_key": "a0", "value": 0},
                         {"_key": "a1", "value": 0},
                         {"_key": "a2", "value": 0},
                         {"_key": "b0", "value": 0},
                         {"_key": "b1", "value": 0},
                         {"_key": "b2", "value": 0},
                         {"_key": "b3", "value": 0},
                         {"_key": "c0", "value": 0},
                         {"_key": "c1", "value": 0},
                         {"_key": "c2", "value": 0},
                         {"_key": "c3", "value": 0},
                         {"_key": "c4", "value": 0}],
           "edges":    [ {"_key": "", "_from": "a0", "_to": "a1"},
                         {"_key": "", "_from": "a2", "_to": "a1"},
                         {"_key": "", "_from": "a2", "_to": "a0"},
                         {"_key": "", "_from": "b0", "_to": "b1"},
                         {"_key": "", "_from": "b2", "_to": "b1"},
                         {"_key": "", "_from": "b2", "_to": "b3"},
                         {"_key": "", "_from": "b0", "_to": "b3"},
                         {"_key": "", "_from": "c0", "_to": "c1"},
                         {"_key": "", "_from": "c2", "_to": "c1"},
                         {"_key": "", "_from": "c2", "_to": "c3"},
                         {"_key": "", "_from": "c4", "_to": "c3"},
                         {"_key": "", "_from": "c0", "_to": "c4"}] })"_vpack;
}

SharedSlice setup1SingleVertex() {
  return
      R"({ "vertices": [ {"_key": "A", "value": 0} ],
           "edges":    [ ] })"_vpack;
}

SharedSlice setup2IsolatedVertices() {
  return
      R"({ "vertices": [ {"_key": "A", "value": 0},
                         {"_key": "B", "value": 0} ],
           "edges":    [ ] })"_vpack;
}

SharedSlice setup1DirectedTree() {
  return
      R"({ "vertices": [ {"_key": "a", "value": 5},
                         {"_key": "a0", "value": 10},
                         {"_key": "a1", "value": 15},
                         {"_key": "a00", "value": 10},
                         {"_key": "a01", "value": 15},
                         {"_key": "a10", "value": 10},
                         {"_key": "a11", "value": 15},
                         {"_key": "a000", "value": 10},
                         {"_key": "a001", "value": 15},
                         {"_key": "a010", "value": 10},
                         {"_key": "a011", "value": 15},
                         {"_key": "a100", "value": 10},
                         {"_key": "a101", "value": 15},
                         {"_key": "a110", "value": 10},
                         {"_key": "a111", "value": 15} ],
           "edges":   [ {"_key": "", "_from": "a", "_to": "a0"},
                        {"_key": "", "_from": "a", "_to": "a1"},
                        {"_key": "", "_from": "a0", "_to": "a00"},
                        {"_key": "", "_from": "a0", "_to": "a01"},
                        {"_key": "", "_from": "a1", "_to": "a10"},
                        {"_key": "", "_from": "a1", "_to": "a11"},
                        {"_key": "", "_from": "a00", "_to": "a000"},
                        {"_key": "", "_from": "a00", "_to": "a001"},
                        {"_key": "", "_from": "a01", "_to": "a010"},
                        {"_key": "", "_from": "a01", "_to": "a011"},
                        {"_key": "", "_from": "a10", "_to": "a100"},
                        {"_key": "", "_from": "a10", "_to": "a101"},
                        {"_key": "", "_from": "a11", "_to": "a110"},
                        {"_key": "", "_from": "a11", "_to": "a111"} ] })"_vpack;
}

SharedSlice setup1AlternatingTree() {
  return
      R"({ "vertices": [ {"_key": "a", "value": 5},
                         {"_key": "a0", "value": 10},
                         {"_key": "a1", "value": 15},
                         {"_key": "a00", "value": 10},
                         {"_key": "a01", "value": 15},
                         {"_key": "a10", "value": 10},
                         {"_key": "a11", "value": 15},
                         {"_key": "a000", "value": 10},
                         {"_key": "a001", "value": 15},
                         {"_key": "a010", "value": 10},
                         {"_key": "a011", "value": 15},
                         {"_key": "a100", "value": 10},
                         {"_key": "a101", "value": 15},
                         {"_key": "a110", "value": 10},
                         {"_key": "a111", "value": 15} ],
           "edges":    [ {"_key": "", "_from": "a", "_to": "a0"},
                         {"_key": "", "_from": "a", "_to": "a1"},
                         {"_key": "", "_from": "a00", "_to": "a0"},
                         {"_key": "", "_from": "a01", "_to": "a0"},
                         {"_key": "", "_from": "a10", "_to": "a1"},
                         {"_key": "", "_from": "a11", "_to": "a1"},
                         {"_key": "", "_from": "a00", "_to": "a000"},
                         {"_key": "", "_from": "a00", "_to": "a001"},
                         {"_key": "", "_from": "a01", "_to": "a010"},
                         {"_key": "", "_from": "a01", "_to": "a011"},
                         {"_key": "", "_from": "a10", "_to": "a100"},
                         {"_key": "", "_from": "a10", "_to": "a101"},
                         {"_key": "", "_from": "a11", "_to": "a110"},
                         {"_key": "", "_from": "a11", "_to": "a111"} ] })"_vpack;
}

SharedSlice setup2CliquesConnectedByDirectedEdge() {
  return
      R"({ "vertices": [ {"_key": "a0", "value": 5},
                         {"_key": "a1", "value": 10},
                         {"_key": "a2", "value": 15},
                         {"_key": "b0", "value": 5},
                         {"_key": "b1", "value": 10},
                         {"_key": "b2", "value": 15}
 ],
           "edges":    [ {"_key": "", "_from": "a0", "_to": "a1"},
                         {"_key": "", "_from": "a1", "_to": "a0"},
                         {"_key": "", "_from": "a0", "_to": "a2"},
                         {"_key": "", "_from": "a2", "_to": "a0"},
                         {"_key": "", "_from": "a1", "_to": "a2"},
                         {"_key": "", "_from": "a2", "_to": "a1"},
                         {"_key": "", "_from": "b0", "_to": "b1"},
                         {"_key": "", "_from": "b1", "_to": "b0"},
                         {"_key": "", "_from": "b0", "_to": "b2"},
                         {"_key": "", "_from": "b2", "_to": "b0"},
                         {"_key": "", "_from": "b1", "_to": "b2"},
                         {"_key": "", "_from": "b2", "_to": "b1"},
                         {"_key": "", "_from": "a0", "_to": "b0"} ] })"_vpack;
}

TEST(GWEN_WCC, test_wcc_2_path) {
  bool const checkDuplicateVertices = true;
  WCCGraph<EmptyEdgeProperties, WCCVertexProperties> graph(
      setup2Path(), checkDuplicateVertices);
  size_t numComponents = graph.computeWCC();
  ASSERT_EQ(numComponents, 1);
}

template<typename EdgeProperties>
auto testWCC(WCCGraph<EdgeProperties, WCCVertexProperties>& graph,
             size_t expectedNumComponents) {
  size_t numComponents = graph.computeWCC();
  ASSERT_EQ(numComponents, expectedNumComponents);
  std::unordered_map<char, uint64_t> value;
  for (auto const& v : graph.vertices) {
    EXPECT_GE(v._key.size(), 1);
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
