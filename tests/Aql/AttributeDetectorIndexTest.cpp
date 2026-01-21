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

TEST_F(AttributeDetectorTest, FilterWithIndex) {
  auto query = executeQuery(R"aql(
    FOR p IN products
    FILTER p.price > 20
    RETURN p.name
  )aql");

  auto const& accesses = query->abacAccesses();

  ASSERT_EQ(accesses.size(), 1);
  EXPECT_EQ(accesses[0].collectionName, "products");
  EXPECT_TRUE(accesses[0].readAttributes.contains("name"));
  EXPECT_TRUE(accesses[0].readAttributes.contains("price"));
  EXPECT_FALSE(accesses[0].requiresAllAttributesRead);
  EXPECT_FALSE(accesses[0].requiresAllAttributesWrite);
}

TEST_F(AttributeDetectorTest, JoinTwoCollections) {
  ensureHashIndex("users", "_key");
  ensureHashIndex("posts", "userId");

  auto query = executeQuery(R"aql(
    FOR u IN users
      FOR p IN posts
        FILTER u._key == p.userId
        RETURN { user: u.name, post: p.title }
  )aql");

  auto const& accesses = query->abacAccesses();

  ASSERT_EQ(accesses.size(), 2);

  auto usersAccess =
      std::find_if(accesses.begin(), accesses.end(),
                   [](auto const& a) { return a.collectionName == "users"; });
  ASSERT_NE(usersAccess, accesses.end());
  EXPECT_TRUE(usersAccess->readAttributes.contains("name"));
  EXPECT_TRUE(usersAccess->readAttributes.contains("_key"));
  EXPECT_FALSE(usersAccess->requiresAllAttributesRead);
  EXPECT_FALSE(usersAccess->requiresAllAttributesWrite);

  auto postsAccess =
      std::find_if(accesses.begin(), accesses.end(),
                   [](auto const& a) { return a.collectionName == "posts"; });
  ASSERT_NE(postsAccess, accesses.end());
  EXPECT_TRUE(postsAccess->readAttributes.contains("title"));
  EXPECT_TRUE(postsAccess->readAttributes.contains("userId"));
  EXPECT_FALSE(postsAccess->requiresAllAttributesRead);
  EXPECT_FALSE(postsAccess->requiresAllAttributesWrite);
}

TEST_F(AttributeDetectorTest, JoinReturnFullDocuments) {
  ensureHashIndex("users", "_key");
  ensureHashIndex("posts", "userId");

  auto query = executeQuery(R"aql(
    FOR u IN users
      FOR p IN posts
        FILTER u._key == p.userId
        RETURN { user: u, post: p }
  )aql");

  auto const& accesses = query->abacAccesses();

  ASSERT_EQ(accesses.size(), 2);

  auto usersAccess =
      std::find_if(accesses.begin(), accesses.end(),
                   [](auto const& a) { return a.collectionName == "users"; });
  ASSERT_NE(usersAccess, accesses.end());
  EXPECT_TRUE(usersAccess->requiresAllAttributesRead);
  EXPECT_FALSE(usersAccess->requiresAllAttributesWrite);

  auto postsAccess =
      std::find_if(accesses.begin(), accesses.end(),
                   [](auto const& a) { return a.collectionName == "posts"; });
  ASSERT_NE(postsAccess, accesses.end());
  EXPECT_TRUE(postsAccess->requiresAllAttributesRead);
  EXPECT_FALSE(postsAccess->requiresAllAttributesWrite);
}

TEST_F(AttributeDetectorTest, JoinWithFilterCondition) {
  ensureHashIndex("users", "age");
  ensureHashIndex("users", "_key");
  ensureHashIndex("posts", "userId");

  auto query = executeQuery(R"aql(
    FOR u IN users
      FILTER u.age > 25
      FOR p IN posts
        FILTER u._key == p.userId
        RETURN { userName: u.name, postTitle: p.title }
  )aql");

  auto const& accesses = query->abacAccesses();

  ASSERT_EQ(accesses.size(), 2);

  auto usersAccess =
      std::find_if(accesses.begin(), accesses.end(),
                   [](auto const& a) { return a.collectionName == "users"; });
  ASSERT_NE(usersAccess, accesses.end());
  EXPECT_TRUE(usersAccess->readAttributes.contains("name"));
  EXPECT_TRUE(usersAccess->readAttributes.contains("age"));
  EXPECT_TRUE(usersAccess->readAttributes.contains("_key"));

  auto postsAccess =
      std::find_if(accesses.begin(), accesses.end(),
                   [](auto const& a) { return a.collectionName == "posts"; });
  ASSERT_NE(postsAccess, accesses.end());
  EXPECT_TRUE(postsAccess->readAttributes.contains("title"));
  EXPECT_TRUE(postsAccess->readAttributes.contains("userId"));
}

}  // namespace arangodb::tests::aql
