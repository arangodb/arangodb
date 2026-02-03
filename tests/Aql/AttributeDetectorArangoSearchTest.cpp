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

// --- Simple view: return single attribute (view still reports full read) ---
TEST_F(AttributeDetectorTest, SimpleArangoSearchReturnSingleAttribute) {
  auto query = executeQuery(R"aql(
    FOR d IN usersViewSimple
      SEARCH d.name == "Alice"
      RETURN d.name
  )aql");

  auto const& accesses = query->abacAccesses();
  ASSERT_EQ(accesses.size(), 1);
  EXPECT_EQ(accesses[0].collectionName, "users");
  EXPECT_TRUE(accesses[0].requiresAllAttributesRead);
  EXPECT_FALSE(accesses[0].requiresAllAttributesWrite);
}

// --- Simple view: COUNT only (no document returned) ---
TEST_F(AttributeDetectorTest, SimpleArangoSearchCountOnly) {
  auto query = executeQuery(R"aql(
    FOR d IN usersViewSimple
      SEARCH d.name == "Alice"
      COLLECT WITH COUNT INTO c
      RETURN c
  )aql");

  auto const& accesses = query->abacAccesses();
  ASSERT_EQ(accesses.size(), 1);
  EXPECT_EQ(accesses[0].collectionName, "users");
  EXPECT_TRUE(accesses[0].requiresAllAttributesRead);
  EXPECT_FALSE(accesses[0].requiresAllAttributesWrite);
}

// --- Simple view: LIMIT 0 (edge case, plan still touches view/collections) ---
TEST_F(AttributeDetectorTest, SimpleArangoSearchLimitZero) {
  auto query = executeQuery(R"aql(
    FOR d IN usersViewSimple
      SEARCH d.name == "Alice"
      LIMIT 0
      RETURN d
  )aql");

  auto const& accesses = query->abacAccesses();
  ASSERT_EQ(accesses.size(), 1);
  EXPECT_EQ(accesses[0].collectionName, "users");
  EXPECT_TRUE(accesses[0].requiresAllAttributesRead);
  EXPECT_FALSE(accesses[0].requiresAllAttributesWrite);
}

// --- Simple view: SEARCH with OR condition ---
TEST_F(AttributeDetectorTest, SimpleArangoSearchWithOrCondition) {
  auto query = executeQuery(R"aql(
    FOR d IN usersViewSimple
      SEARCH d.name == "Alice" OR d.name == "Bob"
      RETURN d
  )aql");

  auto const& accesses = query->abacAccesses();
  ASSERT_EQ(accesses.size(), 1);
  EXPECT_EQ(accesses[0].collectionName, "users");
  EXPECT_TRUE(accesses[0].requiresAllAttributesRead);
  EXPECT_FALSE(accesses[0].requiresAllAttributesWrite);
}

// --- Simple view: SEARCH with explicit ANALYZER ---
TEST_F(AttributeDetectorTest, SimpleArangoSearchWithAnalyzer) {
  auto query = executeQuery(R"aql(
    FOR d IN usersViewSimple
      SEARCH ANALYZER(d.name == "Alice", "text_en")
      RETURN d
  )aql");

  auto const& accesses = query->abacAccesses();
  ASSERT_EQ(accesses.size(), 1);
  EXPECT_EQ(accesses[0].collectionName, "users");
  EXPECT_TRUE(accesses[0].requiresAllAttributesRead);
  EXPECT_FALSE(accesses[0].requiresAllAttributesWrite);
}

// --- Simple view: SEARCH with no-match condition (same plan shape) ---
TEST_F(AttributeDetectorTest, SimpleArangoSearchNoMatchCondition) {
  auto query = executeQuery(R"aql(
    FOR d IN usersViewSimple
      SEARCH d.name == "nonexistent"
      RETURN d
  )aql");

  auto const& accesses = query->abacAccesses();
  ASSERT_EQ(accesses.size(), 1);
  EXPECT_EQ(accesses[0].collectionName, "users");
  EXPECT_TRUE(accesses[0].requiresAllAttributesRead);
  EXPECT_FALSE(accesses[0].requiresAllAttributesWrite);
}

// --- Simple view: LET then RETURN (document still read from view) ---
TEST_F(AttributeDetectorTest, SimpleArangoSearchWithLet) {
  auto query = executeQuery(R"aql(
    FOR d IN usersViewSimple
      SEARCH d.name == "Alice"
      LET x = d.name
      RETURN x
  )aql");

  auto const& accesses = query->abacAccesses();
  ASSERT_EQ(accesses.size(), 1);
  EXPECT_EQ(accesses[0].collectionName, "users");
  EXPECT_TRUE(accesses[0].requiresAllAttributesRead);
  EXPECT_FALSE(accesses[0].requiresAllAttributesWrite);
}

// --- Complicated view: return full document ---
TEST_F(AttributeDetectorTest, ComplicatedArangoSearchReturnFullDocument) {
  auto query = executeQuery(R"aql(
    FOR d IN usersViewComplicated
      SEARCH d.name == "Alice"
      RETURN d
  )aql");

  auto const& accesses = query->abacAccesses();
  const std::set<std::string> expected{"users", "products", "posts", "ordered"};
  ASSERT_EQ(accesses.size(), static_cast<size_t>(expected.size()));

  std::set<std::string> actual;
  for (auto const& access : accesses) {
    actual.insert(access.collectionName);
    EXPECT_TRUE(access.requiresAllAttributesRead);
    EXPECT_FALSE(access.requiresAllAttributesWrite);
  }
  EXPECT_EQ(actual, expected);
}

// --- Complicated view: SORT and LIMIT ---
TEST_F(AttributeDetectorTest, ComplicatedArangoSearchSortAndLimit) {
  auto query = executeQuery(R"aql(
    FOR d IN usersViewComplicated
      SEARCH d.name == "Alice"
      SORT d.age
      LIMIT 5
      RETURN d
  )aql");

  auto const& accesses = query->abacAccesses();
  const std::set<std::string> expected{"users", "products", "posts", "ordered"};
  ASSERT_EQ(accesses.size(), static_cast<size_t>(expected.size()));

  std::set<std::string> actual;
  for (auto const& access : accesses) {
    actual.insert(access.collectionName);
    EXPECT_TRUE(access.requiresAllAttributesRead);
    EXPECT_FALSE(access.requiresAllAttributesWrite);
  }
  EXPECT_EQ(actual, expected);
}

// --- Complicated view: return only _key (view still reports full read for all
// linked colls) ---
TEST_F(AttributeDetectorTest, ComplicatedArangoSearchReturnKeyOnly) {
  auto query = executeQuery(R"aql(
    FOR d IN usersViewComplicated
      SEARCH d.name == "Alice"
      RETURN d._key
  )aql");

  auto const& accesses = query->abacAccesses();
  const std::set<std::string> expected{"users", "products", "posts", "ordered"};
  ASSERT_EQ(accesses.size(), static_cast<size_t>(expected.size()));

  std::set<std::string> actual;
  for (auto const& access : accesses) {
    actual.insert(access.collectionName);
    EXPECT_TRUE(access.requiresAllAttributesRead);
    EXPECT_FALSE(access.requiresAllAttributesWrite);
  }
  EXPECT_EQ(actual, expected);
}

// --- Two views in one query: collections merged by name ---
TEST_F(AttributeDetectorTest, TwoArangoSearchViewsInOneQuery) {
  auto query = executeQuery(R"aql(
    FOR d1 IN usersViewSimple
      SEARCH d1.name == "Alice"
      FOR d2 IN usersViewComplicated
        SEARCH d2.name == "Bob"
        RETURN [d1, d2]
  )aql");

  auto const& accesses = query->abacAccesses();
  const std::set<std::string> expected{"users", "products", "posts", "ordered"};
  ASSERT_EQ(accesses.size(), static_cast<size_t>(expected.size()));

  std::set<std::string> actual;
  for (auto const& access : accesses) {
    actual.insert(access.collectionName);
    EXPECT_TRUE(access.requiresAllAttributesRead);
    EXPECT_FALSE(access.requiresAllAttributesWrite);
  }
  EXPECT_EQ(actual, expected);
}

// --- View then collection: SEARCH on view + FOR on collection ---
TEST_F(AttributeDetectorTest, ArangoSearchThenCollectionJoin) {
  auto query = executeQuery(R"aql(
    FOR d IN usersViewSimple
      SEARCH d.name == "Alice"
      FOR p IN posts
        FILTER p.userId == d._key
        RETURN {user: d, post: p}
  )aql");

  auto const& accesses = query->abacAccesses();
  std::set<std::string> actual;
  for (auto const& access : accesses) {
    actual.insert(access.collectionName);
    EXPECT_TRUE(access.requiresAllAttributesRead);
    EXPECT_FALSE(access.requiresAllAttributesWrite);
  }
  EXPECT_TRUE(actual.count("users")) << "View links users";
  EXPECT_TRUE(actual.count("posts")) << "FOR posts enumerates posts";
  EXPECT_EQ(actual.size(), 2);
}

// --- Complicated view: COLLECT then RETURN (aggregation) ---
TEST_F(AttributeDetectorTest, ComplicatedArangoSearchWithCollect) {
  auto query = executeQuery(R"aql(
    FOR d IN usersViewComplicated
      SEARCH d.name != ""
      COLLECT name = d.name
      RETURN name
  )aql");

  auto const& accesses = query->abacAccesses();
  const std::set<std::string> expected{"users", "products", "posts", "ordered"};
  ASSERT_EQ(accesses.size(), static_cast<size_t>(expected.size()));

  std::set<std::string> actual;
  for (auto const& access : accesses) {
    actual.insert(access.collectionName);
    EXPECT_TRUE(access.requiresAllAttributesRead);
    EXPECT_FALSE(access.requiresAllAttributesWrite);
  }
  EXPECT_EQ(actual, expected);
}

}  // namespace arangodb::tests::aql
