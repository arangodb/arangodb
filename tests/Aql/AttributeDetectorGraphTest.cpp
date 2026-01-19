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
      EXPECT_TRUE(access.readAttributes.contains("_from"));
      EXPECT_TRUE(access.readAttributes.contains("_to"));
      EXPECT_TRUE(access.requiresAllAttributesRead);
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

}  // namespace arangodb::tests::aql
