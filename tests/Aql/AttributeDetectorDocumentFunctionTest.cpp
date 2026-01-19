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

TEST_F(AttributeDetectorTest, DocumentFunction) {
  auto query = executeQuery(R"aql(
    RETURN DOCUMENT(users, "users/u1")
  )aql");
  auto const& accesses = query->abacAccesses();

  ASSERT_EQ(accesses.size(), 1);
  EXPECT_EQ(accesses[0].collectionName, "users");
  EXPECT_TRUE(accesses[0].requiresAllAttributesRead);
  EXPECT_FALSE(accesses[0].requiresAllAttributesWrite);
}

TEST_F(AttributeDetectorTest, DocumentFunctionWithNOOPT) {
  auto query = executeQuery(R"aql(
    FOR u IN users
    LET doc = NOOPT(DOCUMENT(users, "users/u1"))
    RETURN u.name
  )aql");
  auto const& accesses = query->abacAccesses();

  ASSERT_EQ(accesses.size(), 1);
  EXPECT_EQ(accesses[0].collectionName, "users");
  EXPECT_TRUE(accesses[0].requiresAllAttributesRead);
  EXPECT_FALSE(accesses[0].requiresAllAttributesWrite);
  EXPECT_TRUE(accesses[0].readAttributes.contains("name"));
}

TEST_F(AttributeDetectorTest, MergeMultipleDocumentCalls) {
  auto query = executeQuery(R"aql(
    FOR u IN users
    LET merged = MERGE(DOCUMENT(users, "users/u2"), DOCUMENT(products, "products/p2"))
    RETURN merged
  )aql");
  auto const& accesses = query->abacAccesses();

  ASSERT_EQ(accesses.size(), 2);
  for (auto const& access : accesses) {
    EXPECT_TRUE(access.requiresAllAttributesRead)
        << "Collection " << access.collectionName
        << " should require all attributes read";
    EXPECT_FALSE(accesses[0].requiresAllAttributesWrite);
  }
}

TEST_F(AttributeDetectorTest, NOOPTMergeMultipleDocumentCalls) {
  auto query = executeQuery(R"aql(
    FOR u IN users
    LET merged = NOOPT(MERGE(DOCUMENT(users, "users/u2"), DOCUMENT(products, "products/p2")))
    RETURN merged
  )aql");
  auto const& accesses = query->abacAccesses();

  ASSERT_EQ(accesses.size(), 2);
  for (auto const& access : accesses) {
    EXPECT_TRUE(access.requiresAllAttributesRead)
        << "Collection " << access.collectionName
        << " should require all attributes read";
    EXPECT_FALSE(accesses[0].requiresAllAttributesWrite);
  }
}

}  // namespace arangodb::tests::aql
