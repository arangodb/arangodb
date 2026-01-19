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

TEST_F(AttributeDetectorTest, SimpleProjection) {
  auto query = executeQuery("FOR doc IN users RETURN doc.name");
  auto const& accesses = query->abacAccesses();

  ASSERT_EQ(accesses.size(), 1);
  EXPECT_EQ(accesses[0].collectionName, "users");
  EXPECT_TRUE(accesses[0].readAttributes.contains("name"));
  EXPECT_FALSE(accesses[0].requiresAllAttributesRead);
  EXPECT_FALSE(accesses[0].requiresAllAttributesWrite);
}

TEST_F(AttributeDetectorTest, MultipleAttributes) {
  auto query =
      executeQuery("FOR doc IN users RETURN {name: doc.name, age: doc.age}");
  auto const& accesses = query->abacAccesses();

  ASSERT_EQ(accesses.size(), 1);
  EXPECT_EQ(accesses[0].collectionName, "users");
  EXPECT_TRUE(accesses[0].readAttributes.contains("name"));
  EXPECT_TRUE(accesses[0].readAttributes.contains("age"));
  EXPECT_EQ(accesses[0].readAttributes.size(), 2);
  EXPECT_FALSE(accesses[0].requiresAllAttributesRead);
  EXPECT_FALSE(accesses[0].requiresAllAttributesWrite);
}

TEST_F(AttributeDetectorTest, FullDocumentAccess) {
  auto query = executeQuery("FOR doc IN users RETURN doc");
  auto const& accesses = query->abacAccesses();

  ASSERT_EQ(accesses.size(), 1);
  EXPECT_EQ(accesses[0].collectionName, "users");
  EXPECT_TRUE(accesses[0].requiresAllAttributesRead);
  EXPECT_FALSE(accesses[0].requiresAllAttributesWrite);
}

TEST_F(AttributeDetectorTest, FilterOnAttributeReturnFullDocument) {
  auto query = executeQuery("FOR u IN users FILTER u.name == 'Alice' RETURN u");
  auto const& accesses = query->abacAccesses();

  ASSERT_EQ(accesses.size(), 1);
  EXPECT_EQ(accesses[0].collectionName, "users");
  EXPECT_TRUE(accesses[0].requiresAllAttributesRead);
  EXPECT_FALSE(accesses[0].requiresAllAttributesWrite);
}

TEST_F(AttributeDetectorTest, MultipleCollections) {
  auto query = executeQuery(
      "FOR u IN users FOR p IN posts FILTER u._key == p.userId RETURN {user: "
      "u.name, post: p.title}");
  auto const& accesses = query->abacAccesses();

  ASSERT_EQ(accesses.size(), 2);

  bool foundUsers = false;
  bool foundPosts = false;

  for (auto const& access : accesses) {
    if (access.collectionName == "users") {
      foundUsers = true;
      EXPECT_FALSE(access.requiresAllAttributesRead);
      EXPECT_FALSE(access.requiresAllAttributesWrite);
      EXPECT_TRUE(access.readAttributes.contains("name"));
      EXPECT_TRUE(access.readAttributes.contains("_key"));
    } else if (access.collectionName == "posts") {
      foundPosts = true;
      EXPECT_FALSE(access.requiresAllAttributesRead);
      EXPECT_FALSE(access.requiresAllAttributesWrite);
      EXPECT_TRUE(access.readAttributes.contains("title"));
      EXPECT_TRUE(access.readAttributes.contains("userId"));
    }
  }

  EXPECT_TRUE(foundUsers);
  EXPECT_TRUE(foundPosts);
}

TEST_F(AttributeDetectorTest, FilterAndReturnDifferentAttributes1) {
  auto query = executeQuery(
      "FOR p IN users FILTER p.name IN ['Alice', 'Bob'] RETURN p.age");
  auto const& accesses = query->abacAccesses();

  ASSERT_EQ(accesses.size(), 1);
  EXPECT_EQ(accesses[0].collectionName, "users");
  EXPECT_FALSE(accesses[0].requiresAllAttributesRead);
  EXPECT_FALSE(accesses[0].requiresAllAttributesWrite);
  EXPECT_TRUE(accesses[0].readAttributes.contains("age"));
  EXPECT_TRUE(accesses[0].readAttributes.contains("name"));
}

TEST_F(AttributeDetectorTest, FilterAndReturnDifferentAttributes2) {
  auto query = executeQuery(
      "FOR p IN users FILTER p.name == 'Alice' FOR q IN users FILTER q.age == "
      "p.age RETURN q.address");
  auto const& accesses = query->abacAccesses();

  ASSERT_EQ(accesses.size(), 1);
  EXPECT_EQ(accesses[0].collectionName, "users");
  EXPECT_FALSE(accesses[0].requiresAllAttributesRead);
  EXPECT_FALSE(accesses[0].requiresAllAttributesWrite);
  EXPECT_TRUE(accesses[0].readAttributes.contains("age"));
  EXPECT_TRUE(accesses[0].readAttributes.contains("name"));
  EXPECT_TRUE(accesses[0].readAttributes.contains("address"));
}

TEST_F(AttributeDetectorTest, FilterAndReturnDifferentAttributes3) {
  auto query = executeQuery(
      "FOR p IN users FILTER p.name == 'Alice' FOR q IN users FILTER q.age == "
      "p.age RETURN q");
  auto const& accesses = query->abacAccesses();

  ASSERT_EQ(accesses.size(), 1);
  EXPECT_EQ(accesses[0].collectionName, "users");
  EXPECT_TRUE(accesses[0].requiresAllAttributesRead);
  EXPECT_FALSE(accesses[0].requiresAllAttributesWrite);
}

TEST_F(AttributeDetectorTest, CalculationNodeWithAttributeAccess) {
  auto query = executeQuery(
      "FOR u IN users LET ageDouble = u.age * 2 RETURN {name: u.name, "
      "ageDouble}");
  auto const& accesses = query->abacAccesses();

  ASSERT_EQ(accesses.size(), 1);
  EXPECT_EQ(accesses[0].collectionName, "users");
  EXPECT_TRUE(accesses[0].readAttributes.contains("name"));
  EXPECT_TRUE(accesses[0].readAttributes.contains("age"));
}

TEST_F(AttributeDetectorTest, NestedAttributeAccess) {
  auto query = executeQuery("FOR u IN users RETURN u.name");
  auto const& accesses = query->abacAccesses();

  ASSERT_EQ(accesses.size(), 1);
  EXPECT_EQ(accesses[0].collectionName, "users");
  EXPECT_FALSE(accesses[0].requiresAllAttributesRead);
  EXPECT_FALSE(accesses[0].requiresAllAttributesWrite);
  EXPECT_TRUE(accesses[0].readAttributes.contains("name"));
}

TEST_F(AttributeDetectorTest, SortWithAttributes) {
  auto query = executeQuery(R"aql(
    FOR u IN users
    SORT u.age DESC
    RETURN u.name
  )aql");
  auto const& accesses = query->abacAccesses();

  ASSERT_EQ(accesses.size(), 1);
  EXPECT_EQ(accesses[0].collectionName, "users");
  EXPECT_TRUE(accesses[0].readAttributes.contains("age"));
  EXPECT_TRUE(accesses[0].readAttributes.contains("name"));
  EXPECT_FALSE(accesses[0].requiresAllAttributesRead);
  EXPECT_FALSE(accesses[0].requiresAllAttributesWrite);
}

TEST_F(AttributeDetectorTest, ComplexCalculation) {
  auto query = executeQuery(R"aql(
    FOR u IN users
    LET score = u.age * 2
    RETURN {name: u.name, score: score}
  )aql");
  auto const& accesses = query->abacAccesses();

  ASSERT_EQ(accesses.size(), 1);
  EXPECT_EQ(accesses[0].collectionName, "users");
  EXPECT_TRUE(accesses[0].readAttributes.contains("age"));
  EXPECT_TRUE(accesses[0].readAttributes.contains("name"));
  EXPECT_FALSE(accesses[0].requiresAllAttributesRead);
  EXPECT_FALSE(accesses[0].requiresAllAttributesWrite);
}

TEST_F(AttributeDetectorTest, NestedForLoopsReturnFullDocs) {
  auto query = executeQuery(R"aql(
    FOR u IN users
      FOR p IN posts
        FILTER p.userId == u._key
        RETURN { u, p }
  )aql");

  auto const& accesses = query->abacAccesses();

  ASSERT_EQ(accesses.size(), 2);

  std::set<std::string> seen;
  for (auto const& a : accesses) {
    seen.insert(a.collectionName);
    EXPECT_TRUE(a.requiresAllAttributesRead);
    EXPECT_FALSE(a.requiresAllAttributesWrite);
  }

  EXPECT_TRUE(seen.contains("users"));
  EXPECT_TRUE(seen.contains("posts"));
}

}  // namespace arangodb::tests::aql
