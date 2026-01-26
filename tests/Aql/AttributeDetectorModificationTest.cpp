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

TEST_F(AttributeDetectorTest, InsertOperation) {
  auto query = executeQuery("INSERT {name: 'Alice', age: 30} INTO users");
  auto const& accesses = query->abacAccesses();

  ASSERT_EQ(accesses.size(), 1);
  EXPECT_EQ(accesses[0].collectionName, "users");
  EXPECT_TRUE(accesses[0].requiresAllAttributesRead);
  EXPECT_TRUE(accesses[0].requiresAllAttributesWrite);
}

TEST_F(AttributeDetectorTest, UpdateOperation) {
  auto query =
      executeQuery("FOR doc IN users UPDATE doc WITH {age: 31} IN users");
  auto const& accesses = query->abacAccesses();

  ASSERT_EQ(accesses.size(), 1);
  EXPECT_EQ(accesses[0].collectionName, "users");
  EXPECT_TRUE(accesses[0].requiresAllAttributesRead);
  EXPECT_TRUE(accesses[0].requiresAllAttributesWrite);
}

TEST_F(AttributeDetectorTest, ReplaceOperation) {
  auto query = executeQuery(
      "FOR doc IN users REPLACE doc WITH {name: 'Carol', age: 25} IN users");
  auto const& accesses = query->abacAccesses();

  ASSERT_EQ(accesses.size(), 1);
  EXPECT_EQ(accesses[0].collectionName, "users");
  EXPECT_TRUE(accesses[0].readAttributes.contains(makePath("_key", *query)));
  EXPECT_TRUE(accesses[0].requiresAllAttributesRead);
  EXPECT_TRUE(accesses[0].requiresAllAttributesWrite);
}

TEST_F(AttributeDetectorTest, RemoveOperation) {
  auto query = executeQuery("FOR doc IN users REMOVE doc IN users");
  auto const& accesses = query->abacAccesses();

  ASSERT_EQ(accesses.size(), 1);
  EXPECT_EQ(accesses[0].collectionName, "users");
  EXPECT_TRUE(accesses[0].readAttributes.contains(makePath("_key", *query)));
  EXPECT_TRUE(accesses[0].requiresAllAttributesRead);
  EXPECT_TRUE(accesses[0].requiresAllAttributesWrite);
}

TEST_F(AttributeDetectorTest, UpsertOperation) {
  auto query = executeQuery(R"aql(
    UPSERT {_key: 'u999'}
    INSERT {_key: 'u999', name: 'Diana', age: 28}
    UPDATE {age: 29}
    IN users
  )aql");
  auto const& accesses = query->abacAccesses();

  ASSERT_EQ(accesses.size(), 1);
  EXPECT_EQ(accesses[0].collectionName, "users");
  EXPECT_TRUE(accesses[0].requiresAllAttributesRead);
  EXPECT_TRUE(accesses[0].requiresAllAttributesWrite);
}

TEST_F(AttributeDetectorTest, UpdateWithSpecificFields) {
  auto query = executeQuery("UPDATE {_key: 'u1', name: 'Carol'} IN users");
  auto const& accesses = query->abacAccesses();

  ASSERT_EQ(accesses.size(), 1);
  EXPECT_EQ(accesses[0].collectionName, "users");
  EXPECT_TRUE(accesses[0].requiresAllAttributesWrite);
}

TEST_F(AttributeDetectorTest, ReadAndInsertSameCollectionDoesNotLoseReadFlag) {
  auto query = executeQuery("FOR d IN users INSERT d INTO users");
  auto const& accesses = query->abacAccesses();

  ASSERT_EQ(accesses.size(), 1);
  EXPECT_EQ(accesses[0].collectionName, "users");
  EXPECT_TRUE(accesses[0].requiresAllAttributesRead);
  EXPECT_TRUE(accesses[0].requiresAllAttributesWrite);
}

TEST_F(AttributeDetectorTest, InsertRequiresAllReadAndWrite) {
  auto query = executeQuery("INSERT {name: 'Eve', age: 35} INTO users");
  auto const& accesses = query->abacAccesses();

  ASSERT_EQ(accesses.size(), 1);
  EXPECT_EQ(accesses[0].collectionName, "users");
  EXPECT_TRUE(accesses[0].requiresAllAttributesRead);
  EXPECT_TRUE(accesses[0].requiresAllAttributesWrite);
  EXPECT_EQ(accesses[0].readAttributes.size(), 0);
}

TEST_F(AttributeDetectorTest, UpdateInLoop) {
  auto query = executeQuery(R"aql(
    FOR u IN users
    FILTER u.age < 30
    UPDATE u WITH {age: u.age + 1} IN users
  )aql");
  auto const& accesses = query->abacAccesses();

  ASSERT_EQ(accesses.size(), 1);
  EXPECT_EQ(accesses[0].collectionName, "users");
  EXPECT_TRUE(accesses[0].requiresAllAttributesRead);
  EXPECT_TRUE(accesses[0].requiresAllAttributesWrite);
}

TEST_F(AttributeDetectorTest, MultipleModifications) {
  auto insertQuery = executeQuery("INSERT {name: 'Frank'} INTO users");
  auto updateQuery =
      executeQuery("UPDATE {_key: 'u1'} WITH {age: 40} IN users");

  auto const& insertAccesses = insertQuery->abacAccesses();
  ASSERT_EQ(insertAccesses.size(), 1);
  EXPECT_TRUE(insertAccesses[0].requiresAllAttributesRead);
  EXPECT_TRUE(insertAccesses[0].requiresAllAttributesWrite);

  auto const& updateAccesses = updateQuery->abacAccesses();
  ASSERT_EQ(updateAccesses.size(), 1);
  EXPECT_TRUE(updateAccesses[0].requiresAllAttributesRead);
  EXPECT_TRUE(updateAccesses[0].requiresAllAttributesWrite);
}

}  // namespace arangodb::tests::aql
