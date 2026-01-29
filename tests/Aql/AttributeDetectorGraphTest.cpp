////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Julia Puget
////////////////////////////////////////////////////////////////////////////////

#include "AttributeDetectorTestBase.h"

namespace arangodb::tests::aql {

TEST_F(AttributeDetectorTest, TraversalReturnVertexDocument) {
  auto query = executeQuery(R"aql(
    FOR v IN 1..1 OUTBOUND "users/u1" GRAPH testGraph
      RETURN v
  )aql");

  auto const& accesses = query->abacAccesses();

  ASSERT_EQ(accesses.size(), 3);

  std::set<std::string> seen;
  for (auto& access : accesses) {
    seen.insert(access.collectionName);
    if (access.collectionName == "ordered") {
      EXPECT_TRUE(access.readAttributes.contains(
          makePath("_from", query->resourceMonitor())));
      EXPECT_TRUE(access.readAttributes.contains(
          makePath("_to", query->resourceMonitor())));
      EXPECT_FALSE(access.requiresAllAttributesRead);
    } else {
      EXPECT_TRUE(access.requiresAllAttributesRead);
    }
    EXPECT_FALSE(access.requiresAllAttributesWrite);
  }

  EXPECT_TRUE(seen.contains("users"));
  EXPECT_TRUE(seen.contains("products"));
  EXPECT_TRUE(seen.contains("ordered"));
}

TEST_F(AttributeDetectorTest, ShortestPathReturnVertexDocs) {
  auto query = executeQuery(R"aql(
    FOR v, e IN OUTBOUND SHORTEST_PATH "users/u1" TO "products/p3" ordered
      RETURN v
  )aql");

  auto const& accesses = query->abacAccesses();

  ASSERT_EQ(accesses.size(), 3);

  std::set<std::string> seen;
  for (auto const& a : accesses) {
    seen.insert(a.collectionName);
    EXPECT_TRUE(a.requiresAllAttributesRead);
    EXPECT_FALSE(a.requiresAllAttributesWrite);
  }

  EXPECT_TRUE(seen.contains("users"));
  EXPECT_TRUE(seen.contains("products"));
  EXPECT_TRUE(seen.contains("ordered"));
}

TEST_F(AttributeDetectorTest, TraversalReturnVertexDocument_StoreGraph) {
  auto query = executeQuery(R"aql(
    FOR v IN 1..1 OUTBOUND "stores/s1" GRAPH storeGraph
      RETURN v
  )aql");

  auto const& accesses = query->abacAccesses();
  ASSERT_EQ(accesses.size(), 3);

  std::set<std::string> seen;
  for (auto const& a : accesses) {
    seen.insert(a.collectionName);

    if (a.collectionName == "sells") {
      EXPECT_TRUE(a.readAttributes.contains(
          makePath("_from", query->resourceMonitor())));
      EXPECT_TRUE(
          a.readAttributes.contains(makePath("_to", query->resourceMonitor())));
      EXPECT_FALSE(a.requiresAllAttributesRead);
    } else {
      // vertices: stores + products
      EXPECT_TRUE(a.requiresAllAttributesRead);
    }
    EXPECT_FALSE(a.requiresAllAttributesWrite);
  }

  EXPECT_TRUE(seen.contains("stores"));
  EXPECT_TRUE(seen.contains("products"));
  EXPECT_TRUE(seen.contains("sells"));
}

TEST_F(AttributeDetectorTest, ShortestPathReturnVertexDocs_StoreGraph) {
  // storeGraph is OUTBOUND stores -> products via sells
  auto query = executeQuery(R"aql(
    FOR v, e IN OUTBOUND SHORTEST_PATH "stores/s1" TO "products/p3" sells
      RETURN v
  )aql");

  auto const& accesses = query->abacAccesses();
  ASSERT_EQ(accesses.size(), 3);

  std::set<std::string> seen;
  for (auto const& a : accesses) {
    seen.insert(a.collectionName);
    // _from/_to are internal graph navigation, not tracked as user attributes
    EXPECT_TRUE(a.requiresAllAttributesRead);
    EXPECT_FALSE(a.requiresAllAttributesWrite);
  }

  EXPECT_TRUE(seen.contains("stores"));
  EXPECT_TRUE(seen.contains("products"));
  EXPECT_TRUE(seen.contains("sells"));
}

TEST_F(AttributeDetectorTest, TwoTraversalsAcrossTwoGraphs_StoreThenTestGraph) {
  // First traversal: stores -> products (storeGraph / sells)
  // Second traversal: from product to users/products (testGraph / ordered)
  // Only _key is accessed on vertices, so optimizer creates precise projections
  auto query = executeQuery(R"aql(
    FOR p IN 1..1 OUTBOUND "stores/s1" GRAPH storeGraph
      FOR u IN 1..1 OUTBOUND p._id GRAPH testGraph
        RETURN {p: p._key, u: u._key}
  )aql");

  auto const& accesses = query->abacAccesses();

  // Expect: stores, products, sells, users, ordered  => 5 collections
  ASSERT_EQ(accesses.size(), 5);

  std::set<std::string> seen;
  for (auto const& a : accesses) {
    seen.insert(a.collectionName);
    EXPECT_FALSE(a.requiresAllAttributesWrite);
  }

  EXPECT_TRUE(seen.contains("stores"));
  EXPECT_TRUE(seen.contains("products"));
  EXPECT_TRUE(seen.contains("sells"));
  EXPECT_TRUE(seen.contains("users"));
  EXPECT_TRUE(seen.contains("ordered"));
}

TEST_F(AttributeDetectorTest, TraversalEdgesOnly_NoVertexProduction_TestGraph) {
  auto query = executeQuery(R"aql(
    FOR v, e IN 1..2 OUTBOUND "users/u1" GRAPH testGraph
      RETURN e.qty
  )aql");

  auto const& accesses = query->abacAccesses();

  // If vertices are not produced, AttributeDetector::TRAVERSAL will only
  // touch edge collections here.
  ASSERT_EQ(accesses.size(), 1);
  EXPECT_EQ(accesses[0].collectionName, "ordered");

  EXPECT_TRUE(accesses[0].readAttributes.contains(
      makePath("_from", query->resourceMonitor())));
  EXPECT_TRUE(accesses[0].readAttributes.contains(
      makePath("_to", query->resourceMonitor())));
  EXPECT_TRUE(accesses[0].readAttributes.contains(
      makePath("qty", query->resourceMonitor())));

  EXPECT_FALSE(accesses[0].requiresAllAttributesRead);
  EXPECT_FALSE(accesses[0].requiresAllAttributesWrite);
}

// 2) Traversal where we return the edge document itself (still no vertices
// needed) Expect: edge collection only
TEST_F(AttributeDetectorTest,
       TraversalReturnEdgeDocsOnly_NoVertexProduction_TestGraph) {
  auto query = executeQuery(R"aql(
    FOR v, e IN 1..2 OUTBOUND "users/u1" GRAPH testGraph
      RETURN e
  )aql");

  auto const& accesses = query->abacAccesses();

  ASSERT_EQ(accesses.size(), 1);
  EXPECT_EQ(accesses[0].collectionName, "ordered");
  // _from/_to are internal graph navigation, not tracked as user attributes
  EXPECT_TRUE(accesses[0].requiresAllAttributesRead);
  EXPECT_FALSE(accesses[0].requiresAllAttributesWrite);
}

// 3) Traversal where path is returned -> vertices ARE produced (p.vertices /
// p.edges) Expect: users, products, ordered
TEST_F(AttributeDetectorTest, TraversalReturnPath_ProducesVertices_TestGraph) {
  auto query = executeQuery(R"aql(
    FOR v, e, p IN 1..2 OUTBOUND "users/u1" GRAPH testGraph
      RETURN p
  )aql");

  auto const& accesses = query->abacAccesses();
  ASSERT_EQ(accesses.size(), 3);

  std::set<std::string> seen;
  for (auto const& a : accesses) {
    seen.insert(a.collectionName);
    // _from/_to are internal graph navigation, not tracked as user attributes
    EXPECT_TRUE(a.requiresAllAttributesRead);
    EXPECT_FALSE(a.requiresAllAttributesWrite);
  }

  EXPECT_TRUE(seen.contains("users"));
  EXPECT_TRUE(seen.contains("products"));
  EXPECT_TRUE(seen.contains("ordered"));
}

// 4) Same "edges only" test but for the NEW graph (storeGraph / sells)
TEST_F(AttributeDetectorTest,
       TraversalEdgesOnly_NoVertexProduction_StoreGraph) {
  auto query = executeQuery(R"aql(
    FOR v, e IN 1..2 OUTBOUND "stores/s1" GRAPH storeGraph
      RETURN e.stock
  )aql");

  auto const& accesses = query->abacAccesses();

  ASSERT_EQ(accesses.size(), 1);
  EXPECT_EQ(accesses[0].collectionName, "sells");

  EXPECT_TRUE(accesses[0].readAttributes.contains(
      makePath("_from", query->resourceMonitor())));
  EXPECT_TRUE(accesses[0].readAttributes.contains(
      makePath("_to", query->resourceMonitor())));
  EXPECT_TRUE(accesses[0].readAttributes.contains(
      makePath("stock", query->resourceMonitor())));

  EXPECT_FALSE(accesses[0].requiresAllAttributesRead);
  EXPECT_FALSE(accesses[0].requiresAllAttributesWrite);
}

// 5) Two-graph, multi-hop, edges-only on both traversals
// Graph traversal still touches vertex collections for navigation even if
// vertices aren't explicitly accessed
TEST_F(AttributeDetectorTest, TwoGraphs_MultiHop_EdgesOnly_NoVertexProduction) {
  auto query = executeQuery(R"aql(
    FOR p, se IN 1..2 OUTBOUND "stores/s1" GRAPH storeGraph
      FOR v, oe IN 1..2 OUTBOUND p._id GRAPH testGraph
        RETURN {stock: se.stock, qty: oe.qty}
  )aql");

  auto const& accesses = query->abacAccesses();

  // Graph traversal touches vertex collections for navigation
  // Edge collections: sells, ordered
  // Vertex collections may also appear due to graph structure
  ASSERT_GE(accesses.size(), 2);

  bool seenSells = false;
  bool seenOrdered = false;

  for (auto const& a : accesses) {
    if (a.collectionName == "sells") {
      seenSells = true;
      EXPECT_TRUE(a.readAttributes.contains(
          makePath("stock", query->resourceMonitor())));
      EXPECT_FALSE(a.requiresAllAttributesWrite);
    } else if (a.collectionName == "ordered") {
      seenOrdered = true;
      EXPECT_TRUE(
          a.readAttributes.contains(makePath("qty", query->resourceMonitor())));
      EXPECT_FALSE(a.requiresAllAttributesWrite);
    }
    // Vertex collections may also appear - that's expected for graph traversal
  }

  EXPECT_TRUE(seenSells);
  EXPECT_TRUE(seenOrdered);
}

// ENUMERATE_PATHS node: K_SHORTEST_PATHS
TEST_F(AttributeDetectorTest, KShortestPathsReturnPath) {
  auto query = executeQuery(R"aql(
    FOR p IN OUTBOUND K_SHORTEST_PATHS "users/u1" TO "products/p3" ordered
      RETURN p
  )aql");

  auto const& accesses = query->abacAccesses();
  ASSERT_GE(accesses.size(), 1);

  std::set<std::string> seen;
  for (auto const& a : accesses) {
    seen.insert(a.collectionName);
    EXPECT_TRUE(a.requiresAllAttributesRead);
    EXPECT_FALSE(a.requiresAllAttributesWrite);
  }

  EXPECT_TRUE(seen.contains("ordered"));
}

// ENUMERATE_PATHS node: K_PATHS with depth
TEST_F(AttributeDetectorTest, KPathsReturnPath) {
  auto query = executeQuery(R"aql(
    FOR p IN 1..3 OUTBOUND K_PATHS "users/u1" TO "products/p3" ordered
      RETURN p
  )aql");

  auto const& accesses = query->abacAccesses();
  ASSERT_GE(accesses.size(), 1);

  std::set<std::string> seen;
  for (auto const& a : accesses) {
    seen.insert(a.collectionName);
    EXPECT_TRUE(a.requiresAllAttributesRead);
    EXPECT_FALSE(a.requiresAllAttributesWrite);
  }

  EXPECT_TRUE(seen.contains("ordered"));
}

// ENUMERATE_PATHS node: ALL_SHORTEST_PATHS
TEST_F(AttributeDetectorTest, AllShortestPathsReturnPath) {
  auto query = executeQuery(R"aql(
    FOR p IN OUTBOUND ALL_SHORTEST_PATHS "users/u1" TO "products/p3" ordered
      RETURN p
  )aql");

  auto const& accesses = query->abacAccesses();
  ASSERT_GE(accesses.size(), 1);

  std::set<std::string> seen;
  for (auto const& a : accesses) {
    seen.insert(a.collectionName);
    EXPECT_TRUE(a.requiresAllAttributesRead);
    EXPECT_FALSE(a.requiresAllAttributesWrite);
  }

  EXPECT_TRUE(seen.contains("ordered"));
}

}  // namespace arangodb::tests::aql
