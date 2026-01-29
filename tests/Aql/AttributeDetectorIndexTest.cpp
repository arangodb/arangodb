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

// This test file is dedicated for IndexNode (collection "products" is indexed).

TEST_F(AttributeDetectorTest, IndexSimpleProjection) {
  auto query = executeQuery(R"aql(
    FOR doc IN products
      FILTER doc.name == "Keyboard"
      RETURN doc.name
  )aql");
  auto const& accesses = query->abacAccesses();

  ASSERT_EQ(accesses.size(), 1);
  EXPECT_EQ(accesses[0].collectionName, "products");
  EXPECT_TRUE(accesses[0].readAttributes.contains(
      makePath("name", query->resourceMonitor())));
  EXPECT_FALSE(accesses[0].requiresAllAttributesRead);
  EXPECT_FALSE(accesses[0].requiresAllAttributesWrite);
}

TEST_F(AttributeDetectorTest, IndexMultipleAttributes) {
  auto query = executeQuery(R"aql(
    FOR doc IN products
      FILTER doc.name == "Keyboard"
      RETURN {name: doc.name, price: doc.price}
  )aql");
  auto const& accesses = query->abacAccesses();

  ASSERT_EQ(accesses.size(), 1);
  EXPECT_EQ(accesses[0].collectionName, "products");
  EXPECT_TRUE(accesses[0].readAttributes.contains(
      makePath("name", query->resourceMonitor())));
  EXPECT_TRUE(accesses[0].readAttributes.contains(
      makePath("price", query->resourceMonitor())));
  EXPECT_EQ(accesses[0].readAttributes.size(), 2);
  EXPECT_FALSE(accesses[0].requiresAllAttributesRead);
  EXPECT_FALSE(accesses[0].requiresAllAttributesWrite);
}

// Using _id/_key explicitly is still attribute access.
TEST_F(AttributeDetectorTest, IndexReturnIdAndKey) {
  auto query = executeQuery(R"aql(
    FOR p IN products
      FILTER p.name == "Keyboard"
      RETURN {id: p._id, key: p._key}
  )aql");
  auto const& accesses = query->abacAccesses();

  ASSERT_EQ(accesses.size(), 1);
  EXPECT_EQ(accesses[0].collectionName, "products");
  EXPECT_TRUE(accesses[0].readAttributes.contains(
      makePath("name", query->resourceMonitor())));  // index condition
  EXPECT_TRUE(accesses[0].readAttributes.contains(
      makePath("_id", query->resourceMonitor())));
  EXPECT_TRUE(accesses[0].readAttributes.contains(
      makePath("_key", query->resourceMonitor())));
  EXPECT_FALSE(accesses[0].requiresAllAttributesRead);
  EXPECT_FALSE(accesses[0].requiresAllAttributesWrite);
}

TEST_F(AttributeDetectorTest, IndexFullDocumentAccess) {
  auto query = executeQuery(R"aql(
    FOR doc IN products
      FILTER doc.name == "Keyboard"
      RETURN doc
  )aql");
  auto const& accesses = query->abacAccesses();

  ASSERT_EQ(accesses.size(), 1);
  EXPECT_EQ(accesses[0].collectionName, "products");
  EXPECT_TRUE(accesses[0].requiresAllAttributesRead);
  EXPECT_FALSE(accesses[0].requiresAllAttributesWrite);
}

TEST_F(AttributeDetectorTest, IndexFilterOnAttributeReturnFullDocument) {
  auto query = executeQuery(R"aql(
    FOR p IN products
      FILTER p.price == 49.99
      RETURN p
  )aql");
  auto const& accesses = query->abacAccesses();

  ASSERT_EQ(accesses.size(), 1);
  EXPECT_EQ(accesses[0].collectionName, "products");
  EXPECT_TRUE(accesses[0].requiresAllAttributesRead);
  EXPECT_FALSE(accesses[0].requiresAllAttributesWrite);
}

TEST_F(AttributeDetectorTest, IndexFilterWithComputation) {
  // OR conditions prevent precise projection optimization
  auto query = executeQuery(R"aql(
    FOR p IN products
      FILTER p.name == "Keyboard" OR p.price > 100
      RETURN p.price
  )aql");
  auto const& accesses = query->abacAccesses();

  ASSERT_EQ(accesses.size(), 1);
  EXPECT_EQ(accesses[0].collectionName, "products");
  // OR conditions trigger conservative fallback
  EXPECT_TRUE(accesses[0].requiresAllAttributesRead);
  EXPECT_FALSE(accesses[0].requiresAllAttributesWrite);
}

TEST_F(AttributeDetectorTest, IndexMultipleCollections1) {
  auto query = executeQuery(R"aql(
    FOR p IN products
      FILTER p.name == "Keyboard"
      FOR u IN users
        FILTER u._key == "u1"
        RETURN {user: u.name, product: p.name}
  )aql");
  auto const& accesses = query->abacAccesses();

  ASSERT_EQ(accesses.size(), 2);

  bool foundUsers = false;
  bool foundProducts = false;

  for (auto const& access : accesses) {
    if (access.collectionName == "products") {
      foundProducts = true;
      EXPECT_FALSE(access.requiresAllAttributesRead);
      EXPECT_FALSE(access.requiresAllAttributesWrite);
      EXPECT_TRUE(access.readAttributes.contains(
          makePath("name", query->resourceMonitor())));  // index + return
    } else if (access.collectionName == "users") {
      foundUsers = true;
      EXPECT_FALSE(access.requiresAllAttributesRead);
      EXPECT_FALSE(access.requiresAllAttributesWrite);
      EXPECT_TRUE(access.readAttributes.contains(
          makePath("_key", query->resourceMonitor())));
      EXPECT_TRUE(access.readAttributes.contains(
          makePath("name", query->resourceMonitor())));
    }
  }

  EXPECT_TRUE(foundUsers);
  EXPECT_TRUE(foundProducts);
}

TEST_F(AttributeDetectorTest, IndexMultipleCollections2) {
  auto query = executeQuery(R"aql(
    FOR p IN products
      FILTER p.price == 49.99
      FOR u IN users
        FILTER u._key == "u1"
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
  EXPECT_TRUE(seen.contains("products"));
}

TEST_F(AttributeDetectorTest, IndexFilterAndReturnDifferentAttributes1) {
  auto query = executeQuery(R"aql(
    FOR p IN products
      FILTER p.name IN ["Keyboard", "Mouse"]
      RETURN p.price
  )aql");
  auto const& accesses = query->abacAccesses();

  ASSERT_EQ(accesses.size(), 1);
  EXPECT_EQ(accesses[0].collectionName, "products");
  EXPECT_FALSE(accesses[0].requiresAllAttributesRead);
  EXPECT_FALSE(accesses[0].requiresAllAttributesWrite);
  EXPECT_TRUE(accesses[0].readAttributes.contains(
      makePath("name", query->resourceMonitor())));
  EXPECT_TRUE(accesses[0].readAttributes.contains(
      makePath("price", query->resourceMonitor())));
}

TEST_F(AttributeDetectorTest, IndexFilterAndReturnDifferentAttributes2) {
  auto query = executeQuery(R"aql(
    FOR p IN products
      FILTER p.name == "Keyboard"
      FOR q IN products
        FILTER q.price == p.price
        RETURN q.name
  )aql");
  auto const& accesses = query->abacAccesses();

  ASSERT_EQ(accesses.size(), 1);
  EXPECT_EQ(accesses[0].collectionName, "products");
  EXPECT_FALSE(accesses[0].requiresAllAttributesRead);
  EXPECT_FALSE(accesses[0].requiresAllAttributesWrite);

  EXPECT_TRUE(accesses[0].readAttributes.contains(
      makePath("name", query->resourceMonitor())));  // outer index
  EXPECT_TRUE(accesses[0].readAttributes.contains(
      makePath("price", query->resourceMonitor())));  // join + inner index
}

TEST_F(AttributeDetectorTest, IndexFilterAndReturnDifferentAttributes3) {
  auto query = executeQuery(R"aql(
    FOR p IN products
      FILTER p.name == "Keyboard"
      FOR q IN products
        FILTER q.price == p.price
        RETURN q
  )aql");
  auto const& accesses = query->abacAccesses();

  ASSERT_EQ(accesses.size(), 1);
  EXPECT_EQ(accesses[0].collectionName, "products");
  EXPECT_TRUE(accesses[0].requiresAllAttributesRead);
  EXPECT_FALSE(accesses[0].requiresAllAttributesWrite);
}

TEST_F(AttributeDetectorTest, IndexCalculationWithAttributeAccess1) {
  auto query = executeQuery(R"aql(
    FOR p IN products
      FILTER p.name == "Keyboard"
      LET priceDouble = p.price * 2
      RETURN {name: p.name, priceDouble}
  )aql");
  auto const& accesses = query->abacAccesses();

  ASSERT_EQ(accesses.size(), 1);
  EXPECT_EQ(accesses[0].collectionName, "products");
  EXPECT_TRUE(accesses[0].readAttributes.contains(
      makePath("name", query->resourceMonitor())));
  EXPECT_TRUE(accesses[0].readAttributes.contains(
      makePath("price", query->resourceMonitor())));
}

TEST_F(AttributeDetectorTest, IndexCalculationWithAttributeAccess2) {
  auto query = executeQuery(R"aql(
    FOR p IN products
      FILTER p.price == 49.99
      LET score = p.price * 2
      RETURN {name: p.name, score: score}
  )aql");
  auto const& accesses = query->abacAccesses();

  ASSERT_EQ(accesses.size(), 1);
  EXPECT_EQ(accesses[0].collectionName, "products");
  EXPECT_TRUE(accesses[0].readAttributes.contains(
      makePath("price", query->resourceMonitor())));
  EXPECT_TRUE(accesses[0].readAttributes.contains(
      makePath("name", query->resourceMonitor())));
  EXPECT_FALSE(accesses[0].requiresAllAttributesRead);
  EXPECT_FALSE(accesses[0].requiresAllAttributesWrite);
}

TEST_F(AttributeDetectorTest, IndexSortWithAttributes) {
  auto query = executeQuery(R"aql(
    FOR p IN products
      FILTER p.name IN ["Keyboard", "Mouse", "Monitor"]
      SORT p.price DESC
      RETURN p.name
  )aql");
  auto const& accesses = query->abacAccesses();

  ASSERT_EQ(accesses.size(), 1);
  EXPECT_EQ(accesses[0].collectionName, "products");
  EXPECT_TRUE(accesses[0].readAttributes.contains(
      makePath("name", query->resourceMonitor())));
  EXPECT_TRUE(accesses[0].readAttributes.contains(
      makePath("price", query->resourceMonitor())));
  EXPECT_FALSE(accesses[0].requiresAllAttributesRead);
  EXPECT_FALSE(accesses[0].requiresAllAttributesWrite);
}

TEST_F(AttributeDetectorTest, IndexLimitDoesNotChangeProjection) {
  auto query = executeQuery(R"aql(
    FOR p IN products
      FILTER p.name IN ["Keyboard", "Mouse", "Monitor"]
      LIMIT 2
      RETURN p.name
  )aql");
  auto const& accesses = query->abacAccesses();

  ASSERT_EQ(accesses.size(), 1);
  EXPECT_EQ(accesses[0].collectionName, "products");
  EXPECT_TRUE(accesses[0].readAttributes.contains(
      makePath("name", query->resourceMonitor())));
  EXPECT_FALSE(accesses[0].requiresAllAttributesRead);
  EXPECT_FALSE(accesses[0].requiresAllAttributesWrite);
}

TEST_F(AttributeDetectorTest, IndexCollectByAttribute) {
  auto query = executeQuery(R"aql(
    FOR p IN products
      FILTER p.name IN ["Keyboard", "Mouse", "Monitor"]
      COLLECT c = p.category
      RETURN c
  )aql");
  auto const& accesses = query->abacAccesses();

  ASSERT_EQ(accesses.size(), 1);
  EXPECT_EQ(accesses[0].collectionName, "products");
  EXPECT_TRUE(accesses[0].readAttributes.contains(
      makePath("name", query->resourceMonitor())));  // index
  EXPECT_TRUE(accesses[0].readAttributes.contains(
      makePath("category", query->resourceMonitor())));  // collect
  EXPECT_FALSE(accesses[0].requiresAllAttributesRead);
  EXPECT_FALSE(accesses[0].requiresAllAttributesWrite);
}

TEST_F(AttributeDetectorTest, IndexCollectAggregateUsesAttribute) {
  auto query = executeQuery(R"aql(
    FOR p IN products
      FILTER p.name IN ["Keyboard", "Mouse", "Monitor"]
      COLLECT AGGREGATE maxP = MAX(p.price)
      RETURN maxP
  )aql");
  auto const& accesses = query->abacAccesses();

  ASSERT_EQ(accesses.size(), 1);
  EXPECT_EQ(accesses[0].collectionName, "products");
  EXPECT_TRUE(accesses[0].readAttributes.contains(
      makePath("name", query->resourceMonitor())));  // index
  EXPECT_TRUE(accesses[0].readAttributes.contains(
      makePath("price", query->resourceMonitor())));  // aggregate
  EXPECT_FALSE(accesses[0].requiresAllAttributesRead);
  EXPECT_FALSE(accesses[0].requiresAllAttributesWrite);
}

TEST_F(AttributeDetectorTest, IndexReturnDistinctAttribute) {
  auto query = executeQuery(R"aql(
    FOR p IN products
      FILTER p.name IN ["Keyboard", "Mouse", "Monitor"]
      RETURN DISTINCT p.category
  )aql");
  auto const& accesses = query->abacAccesses();

  ASSERT_EQ(accesses.size(), 1);
  EXPECT_EQ(accesses[0].collectionName, "products");
  EXPECT_TRUE(accesses[0].readAttributes.contains(
      makePath("name", query->resourceMonitor())));  // index
  EXPECT_TRUE(accesses[0].readAttributes.contains(
      makePath("category", query->resourceMonitor())));  // distinct
  EXPECT_FALSE(accesses[0].requiresAllAttributesRead);
  EXPECT_FALSE(accesses[0].requiresAllAttributesWrite);
}

TEST_F(AttributeDetectorTest, IndexDynamicCollectionAccessWithBindParameters1) {
  auto bind = VPackParser::fromJson(R"({ "@coll": "products" })");
  auto query = executeQuery(R"aql(
    FOR p IN @@coll
      FILTER p.name == "Keyboard"
      RETURN p.price
  )aql",
                            bind);

  auto const& accesses = query->abacAccesses();

  ASSERT_EQ(accesses.size(), 1);
  EXPECT_EQ(accesses[0].collectionName, "products");
  EXPECT_TRUE(accesses[0].readAttributes.contains(
      makePath("name", query->resourceMonitor())));
  EXPECT_TRUE(accesses[0].readAttributes.contains(
      makePath("price", query->resourceMonitor())));
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
  EXPECT_TRUE(usersAccess->readAttributes.contains(
      makePath("name", query->resourceMonitor())));
  EXPECT_TRUE(usersAccess->readAttributes.contains(
      makePath("_key", query->resourceMonitor())));
  EXPECT_FALSE(usersAccess->requiresAllAttributesRead);
  EXPECT_FALSE(usersAccess->requiresAllAttributesWrite);

  auto postsAccess =
      std::find_if(accesses.begin(), accesses.end(),
                   [](auto const& a) { return a.collectionName == "posts"; });
  ASSERT_NE(postsAccess, accesses.end());
  EXPECT_TRUE(postsAccess->readAttributes.contains(
      makePath("title", query->resourceMonitor())));
  EXPECT_TRUE(postsAccess->readAttributes.contains(
      makePath("userId", query->resourceMonitor())));
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
  EXPECT_TRUE(usersAccess->readAttributes.contains(
      makePath("name", query->resourceMonitor())));
  EXPECT_TRUE(usersAccess->readAttributes.contains(
      makePath("age", query->resourceMonitor())));
  EXPECT_TRUE(usersAccess->readAttributes.contains(
      makePath("_key", query->resourceMonitor())));

  auto postsAccess =
      std::find_if(accesses.begin(), accesses.end(),
                   [](auto const& a) { return a.collectionName == "posts"; });
  ASSERT_NE(postsAccess, accesses.end());
  EXPECT_TRUE(postsAccess->readAttributes.contains(
      makePath("title", query->resourceMonitor())));
  EXPECT_TRUE(postsAccess->readAttributes.contains(
      makePath("userId", query->resourceMonitor())));
}

TEST_F(AttributeDetectorTest, IndexDynamicAttributeAccessWithBindParameters1) {
  auto bind = VPackParser::fromJson(R"({ "attr": "price" })");
  auto query = executeQuery(R"aql(
    FOR p IN products
      FILTER p.name == "Keyboard"
      RETURN p[@attr]
  )aql",
                            bind);

  auto const& accesses = query->abacAccesses();

  ASSERT_EQ(accesses.size(), 1);
  EXPECT_EQ(accesses[0].collectionName, "products");
  EXPECT_TRUE(accesses[0].readAttributes.contains(
      makePath("name", query->resourceMonitor())));  // index condition
  EXPECT_TRUE(accesses[0].readAttributes.contains(
      makePath("price", query->resourceMonitor())));  // projection
  EXPECT_FALSE(accesses[0].requiresAllAttributesRead);
  EXPECT_FALSE(accesses[0].requiresAllAttributesWrite);
}

TEST_F(AttributeDetectorTest, IndexDynamicAttributeAccessWithBindParameters2) {
  auto query = executeQuery(R"aql(
    LET a = "price"
    FOR p IN products
      FILTER p.name == "Keyboard"
      RETURN p[a]
  )aql");
  auto const& accesses = query->abacAccesses();

  ASSERT_EQ(accesses.size(), 1);
  EXPECT_EQ(accesses[0].collectionName, "products");
  EXPECT_TRUE(accesses[0].readAttributes.contains(
      makePath("name", query->resourceMonitor())));
  EXPECT_TRUE(accesses[0].readAttributes.contains(
      makePath("price", query->resourceMonitor())));
  EXPECT_FALSE(accesses[0].requiresAllAttributesRead);
  EXPECT_FALSE(accesses[0].requiresAllAttributesWrite);
}

TEST_F(AttributeDetectorTest, IndexDynamicAttributeAccessWithBindParameters3) {
  auto bind = VPackParser::fromJson(R"({ "attr": "price" })");
  auto query = executeQuery(R"aql(
    FOR p IN products
      FILTER p[@attr] >= 20
      RETURN { name: p.name, category: p.category }
  )aql",
                            bind);
  auto const& accesses = query->abacAccesses();

  ASSERT_EQ(accesses.size(), 1);
  EXPECT_EQ(accesses[0].collectionName, "products");
  EXPECT_TRUE(accesses[0].readAttributes.contains(
      makePath("price", query->resourceMonitor())));  // dynamic filter
  EXPECT_TRUE(accesses[0].readAttributes.contains(
      makePath("name", query->resourceMonitor())));  // return
  EXPECT_TRUE(accesses[0].readAttributes.contains(
      makePath("category", query->resourceMonitor())));  // return
  EXPECT_FALSE(accesses[0].requiresAllAttributesRead);
  EXPECT_FALSE(accesses[0].requiresAllAttributesWrite);
}

TEST_F(AttributeDetectorTest,
       IndexMissingCollectionThrowsWithCorrectErrorCode) {
  try {
    auto q = executeQuery(R"aql(
      FOR p IN missingProducts
        FILTER p.name == "Keyboard"
        RETURN p.price
    )aql");
    (void)q;
    FAIL() << "Expected exception";
  } catch (basics::Exception const& ex) {
    EXPECT_EQ(ex.code(), TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
  }
}

TEST_F(AttributeDetectorTest, IndexMissingAttributeStillRecorded1) {
  auto query = executeQuery(R"aql(
    FOR p IN products
      FILTER p.name == "Keyboard"
      RETURN p.attrNotExist
  )aql");
  auto const& accesses = query->abacAccesses();

  ASSERT_EQ(accesses.size(), 1);
  EXPECT_EQ(accesses[0].collectionName, "products");
  EXPECT_TRUE(accesses[0].readAttributes.contains(
      makePath("name", query->resourceMonitor())));
  EXPECT_TRUE(accesses[0].readAttributes.contains(
      makePath("attrNotExist", query->resourceMonitor())));
  EXPECT_FALSE(accesses[0].requiresAllAttributesRead);
  EXPECT_FALSE(accesses[0].requiresAllAttributesWrite);
}

TEST_F(AttributeDetectorTest, IndexMissingAttributeStillRecorded2) {
  auto query = executeQuery(R"aql(
    FOR p IN products
      FILTER p.name == "Keyboard" AND p.attrNotExist == 1
      RETURN p.price
  )aql");
  auto const& accesses = query->abacAccesses();

  ASSERT_EQ(accesses.size(), 1);
  EXPECT_EQ(accesses[0].collectionName, "products");
  EXPECT_TRUE(accesses[0].readAttributes.contains(
      makePath("name", query->resourceMonitor())));
  EXPECT_TRUE(accesses[0].readAttributes.contains(
      makePath("attrNotExist", query->resourceMonitor())));
  EXPECT_TRUE(accesses[0].readAttributes.contains(
      makePath("price", query->resourceMonitor())));
  EXPECT_FALSE(accesses[0].requiresAllAttributesRead);
  EXPECT_FALSE(accesses[0].requiresAllAttributesWrite);
}

TEST_F(AttributeDetectorTest, IndexMissingAttributeStillRecorded3) {
  auto query = executeQuery(R"aql(
    FOR p IN products
      FILTER p.name == "Keyboard" AND p.attrNotExist == 1
      RETURN p
  )aql");
  auto const& accesses = query->abacAccesses();

  ASSERT_EQ(accesses.size(), 1);
  EXPECT_EQ(accesses[0].collectionName, "products");
  EXPECT_TRUE(accesses[0].requiresAllAttributesRead);
  EXPECT_FALSE(accesses[0].requiresAllAttributesWrite);
}

// Sorted join pattern with index lookup
TEST_F(AttributeDetectorTest, SortedJoinPattern) {
  ensureHashIndex("users", "_key");
  ensureHashIndex("posts", "userId");

  auto query = executeQuery(R"aql(
    FOR u IN users
      SORT u._key
      FOR p IN posts
        FILTER u._key == p.userId
        RETURN {userName: u.name, postTitle: p.title}
  )aql");

  auto const& accesses = query->abacAccesses();
  ASSERT_EQ(accesses.size(), 2);

  for (auto const& a : accesses) {
    if (a.collectionName == "users") {
      EXPECT_TRUE(a.readAttributes.contains(
          makePath("_key", query->resourceMonitor())));
      EXPECT_TRUE(a.readAttributes.contains(
          makePath("name", query->resourceMonitor())));
    } else if (a.collectionName == "posts") {
      EXPECT_TRUE(a.readAttributes.contains(
          makePath("userId", query->resourceMonitor())));
      EXPECT_TRUE(a.readAttributes.contains(
          makePath("title", query->resourceMonitor())));
    }
    EXPECT_FALSE(a.requiresAllAttributesWrite);
  }
}

// COLLECT with multiple grouping fields
TEST_F(AttributeDetectorTest, CollectMultipleGroupFields) {
  auto query = executeQuery(R"aql(
    FOR p IN products
      COLLECT cat = p.category, n = p.name
      RETURN {category: cat, name: n}
  )aql");

  auto const& accesses = query->abacAccesses();
  ASSERT_EQ(accesses.size(), 1);
  EXPECT_EQ(accesses[0].collectionName, "products");
  EXPECT_TRUE(accesses[0].readAttributes.contains(
      makePath("category", query->resourceMonitor())));
  EXPECT_TRUE(accesses[0].readAttributes.contains(
      makePath("name", query->resourceMonitor())));
  EXPECT_FALSE(accesses[0].requiresAllAttributesRead);
  EXPECT_FALSE(accesses[0].requiresAllAttributesWrite);
}

// COLLECT with aggregation function
TEST_F(AttributeDetectorTest, CollectWithAggregateMax) {
  auto query = executeQuery(R"aql(
    FOR p IN products
      COLLECT cat = p.category
      AGGREGATE maxPrice = MAX(p.price)
      RETURN {category: cat, maxPrice}
  )aql");

  auto const& accesses = query->abacAccesses();
  ASSERT_EQ(accesses.size(), 1);
  EXPECT_EQ(accesses[0].collectionName, "products");
  EXPECT_TRUE(accesses[0].readAttributes.contains(
      makePath("category", query->resourceMonitor())));
  EXPECT_TRUE(accesses[0].readAttributes.contains(
      makePath("price", query->resourceMonitor())));
  EXPECT_FALSE(accesses[0].requiresAllAttributesWrite);
}

// Index filter returning non-indexed attributes
TEST_F(AttributeDetectorTest, IndexFilterWithNonIndexedReturn) {
  auto query = executeQuery(R"aql(
    FOR p IN products
      FILTER p.name == "Keyboard"
      RETURN {cat: p.category, price: p.price}
  )aql");

  auto const& accesses = query->abacAccesses();
  ASSERT_EQ(accesses.size(), 1);
  EXPECT_EQ(accesses[0].collectionName, "products");
  EXPECT_TRUE(accesses[0].readAttributes.contains(
      makePath("name", query->resourceMonitor())));
  EXPECT_TRUE(accesses[0].readAttributes.contains(
      makePath("category", query->resourceMonitor())));
  EXPECT_TRUE(accesses[0].readAttributes.contains(
      makePath("price", query->resourceMonitor())));
  EXPECT_FALSE(accesses[0].requiresAllAttributesRead);
  EXPECT_FALSE(accesses[0].requiresAllAttributesWrite);
}

// Index filter returning full document
TEST_F(AttributeDetectorTest, IndexFilterFullDocumentReturn) {
  auto query = executeQuery(R"aql(
    FOR p IN products
      FILTER p.price > 20
      RETURN p
  )aql");

  auto const& accesses = query->abacAccesses();
  ASSERT_EQ(accesses.size(), 1);
  EXPECT_EQ(accesses[0].collectionName, "products");
  EXPECT_TRUE(accesses[0].requiresAllAttributesRead);
  EXPECT_FALSE(accesses[0].requiresAllAttributesWrite);
}

}  // namespace arangodb::tests::aql
