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

TEST_F(AttributeDetectorTest, SimpleArangoSearchReturnDoc) {
  auto query = executeQuery(R"aql(
    FOR d IN usersViewSimple
      SEARCH d.name == "Alice"
      RETURN d
  )aql");

  auto const& accesses = query->abacAccesses();

  ASSERT_EQ(accesses.size(), 1);
  ASSERT_EQ(accesses[0].collectionName, "users");
  EXPECT_TRUE(accesses[0].requiresAllAttributesRead);
  EXPECT_FALSE(accesses[0].requiresAllAttributesWrite);
}

TEST_F(AttributeDetectorTest, ComplicatedArangoSearchReturnDoc) {
  auto query = executeQuery(R"aql(
    FOR d IN usersViewComplicated
      SEARCH d.name == "Alice"
      RETURN d.name
  )aql");
  auto const& accesses = query->abacAccesses();

  const std::set<std::string> expected{"users", "products", "posts", "ordered"};
  ASSERT_EQ(accesses.size(), expected.size());

  std::set<std::string> actual;
  for (auto const& access : accesses) {
    actual.insert(access.collectionName);
    EXPECT_TRUE(access.requiresAllAttributesRead);
    EXPECT_FALSE(access.requiresAllAttributesWrite);
  }
  EXPECT_EQ(actual, expected);
}

}  // namespace arangodb::tests::aql
