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

TEST_F(AttributeDetectorTest, SimpleQuery) {
  auto query = executeQuery("FOR doc IN users RETURN doc");
  auto const& accesses = query->abacAccesses();

  ASSERT_EQ(accesses.size(), 1);
  EXPECT_EQ(accesses[0].collectionName, "users");
  EXPECT_TRUE(accesses[0].requiresAllAttributesRead);
  EXPECT_FALSE(accesses[0].requiresAllAttributesWrite);
}

TEST_F(AttributeDetectorTest, SimpleProjection) {
  auto query = executeQuery("FOR doc IN users RETURN doc.name");
  auto const& accesses = query->abacAccesses();

  ASSERT_EQ(accesses.size(), 1);
  EXPECT_EQ(accesses[0].collectionName, "users");
  EXPECT_TRUE(accesses[0].readAttributes.contains(
      makePath("name", query->resourceMonitor())));
  EXPECT_FALSE(accesses[0].requiresAllAttributesRead);
  EXPECT_FALSE(accesses[0].requiresAllAttributesWrite);
}

TEST_F(AttributeDetectorTest, MultipleAttributes) {
  auto query =
      executeQuery("FOR doc IN users RETURN {name: doc.name, age: doc.age}");
  auto const& accesses = query->abacAccesses();

  ASSERT_EQ(accesses.size(), 1);
  EXPECT_EQ(accesses[0].collectionName, "users");
  EXPECT_TRUE(accesses[0].readAttributes.contains(
      makePath("name", query->resourceMonitor())));
  EXPECT_TRUE(accesses[0].readAttributes.contains(
      makePath("age", query->resourceMonitor())));
  EXPECT_EQ(accesses[0].readAttributes.size(), 2);
  EXPECT_FALSE(accesses[0].requiresAllAttributesRead);
  EXPECT_FALSE(accesses[0].requiresAllAttributesWrite);
}

// Using _id/_key explicitly is still attribute access.
TEST_F(AttributeDetectorTest, ReturnIdAndKey) {
  auto query = executeQuery(R"aql(
    FOR u IN users
      RETURN {id: u._id, key: u._key}
  )aql");
  auto const& accesses = query->abacAccesses();

  ASSERT_EQ(accesses.size(), 1);
  EXPECT_EQ(accesses[0].collectionName, "users");
  EXPECT_TRUE(accesses[0].readAttributes.contains(
      makePath("_id", query->resourceMonitor())));
  EXPECT_TRUE(accesses[0].readAttributes.contains(
      makePath("_key", query->resourceMonitor())));
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

TEST_F(AttributeDetectorTest, FilterWithComputation) {
  auto query = executeQuery(R"aql(
    FOR u IN users
      FILTER u.name == "Alice" OR u.age > 30
      RETURN u.address
  )aql");
  auto const& accesses = query->abacAccesses();

  ASSERT_EQ(accesses.size(), 1);
  EXPECT_EQ(accesses[0].collectionName, "users");
  EXPECT_EQ(accesses[0].readAttributes.size(), 3);
  EXPECT_TRUE(accesses[0].readAttributes.contains(
      makePath("name", query->resourceMonitor())));
  EXPECT_TRUE(accesses[0].readAttributes.contains(
      makePath("age", query->resourceMonitor())));
  EXPECT_TRUE(accesses[0].readAttributes.contains(
      makePath("address", query->resourceMonitor())));
  EXPECT_FALSE(accesses[0].requiresAllAttributesRead);
  EXPECT_FALSE(accesses[0].requiresAllAttributesWrite);
}

TEST_F(AttributeDetectorTest, MultipleCollections1) {
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
      EXPECT_TRUE(access.readAttributes.contains(
          makePath("name", query->resourceMonitor())));
      EXPECT_TRUE(access.readAttributes.contains(
          makePath("_key", query->resourceMonitor())));
    } else if (access.collectionName == "posts") {
      foundPosts = true;
      EXPECT_FALSE(access.requiresAllAttributesRead);
      EXPECT_FALSE(access.requiresAllAttributesWrite);
      EXPECT_TRUE(access.readAttributes.contains(
          makePath("title", query->resourceMonitor())));
      EXPECT_TRUE(access.readAttributes.contains(
          makePath("userId", query->resourceMonitor())));
    }
  }

  EXPECT_TRUE(foundUsers);
  EXPECT_TRUE(foundPosts);
}

TEST_F(AttributeDetectorTest, MultipleCollections2) {
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

TEST_F(AttributeDetectorTest, FilterAndReturnDifferentAttributes1) {
  auto query = executeQuery(
      "FOR p IN users FILTER p.name IN ['Alice', 'Bob'] RETURN p.age");
  auto const& accesses = query->abacAccesses();

  ASSERT_EQ(accesses.size(), 1);
  EXPECT_EQ(accesses[0].collectionName, "users");
  EXPECT_FALSE(accesses[0].requiresAllAttributesRead);
  EXPECT_FALSE(accesses[0].requiresAllAttributesWrite);
  EXPECT_TRUE(accesses[0].readAttributes.contains(
      makePath("age", query->resourceMonitor())));
  EXPECT_TRUE(accesses[0].readAttributes.contains(
      makePath("name", query->resourceMonitor())));
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
  EXPECT_TRUE(accesses[0].readAttributes.contains(
      makePath("age", query->resourceMonitor())));
  EXPECT_TRUE(accesses[0].readAttributes.contains(
      makePath("name", query->resourceMonitor())));
  EXPECT_TRUE(accesses[0].readAttributes.contains(
      makePath("address", query->resourceMonitor())));
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

TEST_F(AttributeDetectorTest, CalculationNodeWithAttributeAccess1) {
  auto query = executeQuery(
      "FOR u IN users LET ageDouble = u.age * 2 RETURN {name: u.name, "
      "ageDouble}");
  auto const& accesses = query->abacAccesses();

  ASSERT_EQ(accesses.size(), 1);
  EXPECT_EQ(accesses[0].collectionName, "users");
  EXPECT_TRUE(accesses[0].readAttributes.contains(
      makePath("name", query->resourceMonitor())));
  EXPECT_TRUE(accesses[0].readAttributes.contains(
      makePath("age", query->resourceMonitor())));
}

TEST_F(AttributeDetectorTest, CalculationNodeWithAttributeAccess2) {
  auto query = executeQuery(R"aql(
    FOR u IN users
    LET score = u.age * 2
    RETURN {name: u.name, score: score}
  )aql");
  auto const& accesses = query->abacAccesses();

  ASSERT_EQ(accesses.size(), 1);
  EXPECT_EQ(accesses[0].collectionName, "users");
  EXPECT_TRUE(accesses[0].readAttributes.contains(
      makePath("age", query->resourceMonitor())));
  EXPECT_TRUE(accesses[0].readAttributes.contains(
      makePath("name", query->resourceMonitor())));
  EXPECT_FALSE(accesses[0].requiresAllAttributesRead);
  EXPECT_FALSE(accesses[0].requiresAllAttributesWrite);
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
  EXPECT_TRUE(accesses[0].readAttributes.contains(
      makePath("age", query->resourceMonitor())));
  EXPECT_TRUE(accesses[0].readAttributes.contains(
      makePath("name", query->resourceMonitor())));
  EXPECT_FALSE(accesses[0].requiresAllAttributesRead);
  EXPECT_FALSE(accesses[0].requiresAllAttributesWrite);
}

TEST_F(AttributeDetectorTest, LimitDoesNotChangeProjection) {
  auto query = executeQuery(R"aql(
    FOR u IN users
      LIMIT 2
      RETURN u.name
  )aql");
  auto const& accesses = query->abacAccesses();

  ASSERT_EQ(accesses.size(), 1);
  EXPECT_EQ(accesses[0].collectionName, "users");
  EXPECT_TRUE(accesses[0].readAttributes.contains(
      makePath("name", query->resourceMonitor())));
  EXPECT_FALSE(accesses[0].requiresAllAttributesRead);
  EXPECT_FALSE(accesses[0].requiresAllAttributesWrite);
}

TEST_F(AttributeDetectorTest, CollectByAttribute) {
  auto query = executeQuery(R"aql(
    FOR u IN users
      COLLECT city = u.address
      RETURN city
  )aql");
  auto const& accesses = query->abacAccesses();

  ASSERT_EQ(accesses.size(), 1);
  EXPECT_EQ(accesses[0].collectionName, "users");
  EXPECT_EQ(accesses[0].readAttributes.size(), 1);
  EXPECT_TRUE(accesses[0].readAttributes.contains(
      makePath("address", query->resourceMonitor())));
  EXPECT_FALSE(accesses[0].requiresAllAttributesRead);
  EXPECT_FALSE(accesses[0].requiresAllAttributesWrite);
}

TEST_F(AttributeDetectorTest, CollectAggregateUsesAttribute) {
  auto query = executeQuery(R"aql(
    FOR u IN users
      COLLECT AGGREGATE maxAge = MAX(u.age)
      RETURN maxAge
  )aql");
  auto const& accesses = query->abacAccesses();

  ASSERT_EQ(accesses.size(), 1);
  EXPECT_EQ(accesses[0].collectionName, "users");
  EXPECT_EQ(accesses[0].readAttributes.size(), 1);
  EXPECT_TRUE(accesses[0].readAttributes.contains(
      makePath("age", query->resourceMonitor())));
  EXPECT_FALSE(accesses[0].requiresAllAttributesRead);
  EXPECT_FALSE(accesses[0].requiresAllAttributesWrite);
}

TEST_F(AttributeDetectorTest, ReturnDistinctAttribute) {
  auto query = executeQuery(R"aql(
    FOR u IN users
      RETURN DISTINCT u.address
  )aql");
  auto const& accesses = query->abacAccesses();

  ASSERT_EQ(accesses.size(), 1);
  EXPECT_EQ(accesses[0].collectionName, "users");
  EXPECT_EQ(accesses[0].readAttributes.size(), 1);
  EXPECT_TRUE(accesses[0].readAttributes.contains(
      makePath("address", query->resourceMonitor())));
  EXPECT_FALSE(accesses[0].requiresAllAttributesRead);
  EXPECT_FALSE(accesses[0].requiresAllAttributesWrite);
}

TEST_F(AttributeDetectorTest, DynamicCollectionAccessWithBindParameters1) {
  auto bind = VPackParser::fromJson(R"({ "@coll": "users" })");
  auto query = executeQuery(R"aql(
    FOR u IN @@coll
      RETURN u.name
  )aql",
                            bind);

  auto const& accesses = query->abacAccesses();

  ASSERT_EQ(accesses.size(), 1);
  EXPECT_EQ(accesses[0].collectionName, "users");
  EXPECT_EQ(accesses[0].readAttributes.size(), 1);
  EXPECT_TRUE(accesses[0].readAttributes.contains(
      makePath("name", query->resourceMonitor())));
  EXPECT_FALSE(accesses[0].requiresAllAttributesRead);
  EXPECT_FALSE(accesses[0].requiresAllAttributesWrite);
}

TEST_F(AttributeDetectorTest, DynamicAttributeAccessWithBindParameters1) {
  auto bind = VPackParser::fromJson(R"({ "attr": "age" })");
  auto query = executeQuery(R"aql(
    FOR u IN users
      RETURN u[@attr]
  )aql",
                            bind);

  auto const& accesses = query->abacAccesses();

  ASSERT_EQ(accesses.size(), 1);
  EXPECT_EQ(accesses[0].collectionName, "users");
  EXPECT_EQ(accesses[0].readAttributes.size(), 1);
  EXPECT_TRUE(accesses[0].readAttributes.contains(
      makePath("age", query->resourceMonitor())));
  EXPECT_FALSE(accesses[0].requiresAllAttributesRead);
  EXPECT_FALSE(accesses[0].requiresAllAttributesWrite);
}

TEST_F(AttributeDetectorTest, DynamicAttributeAccessWithBindParameters2) {
  auto query = executeQuery(R"aql(
    LET a = "name"
    FOR u IN users
      RETURN u[a]
  )aql");
  auto const& accesses = query->abacAccesses();

  ASSERT_EQ(accesses.size(), 1);
  EXPECT_EQ(accesses[0].collectionName, "users");
  EXPECT_EQ(accesses[0].readAttributes.size(), 1);
  EXPECT_TRUE(accesses[0].readAttributes.contains(
      makePath("name", query->resourceMonitor())));
  EXPECT_FALSE(accesses[0].requiresAllAttributesRead);
  EXPECT_FALSE(accesses[0].requiresAllAttributesWrite);
}

TEST_F(AttributeDetectorTest, DynamicAttributeAccessWithBindParameters3) {
  auto bind = VPackParser::fromJson(R"({ "attr": "age" })");
  auto query = executeQuery(R"aql(
    FOR u IN users
      FILTER u[@attr] >= 30
      RETURN { name: u.name, address: u.address }
  )aql",
                            bind);
  auto const& accesses = query->abacAccesses();

  ASSERT_EQ(accesses.size(), 1);
  EXPECT_EQ(accesses[0].collectionName, "users");
  EXPECT_EQ(accesses[0].readAttributes.size(), 3);
  EXPECT_TRUE(accesses[0].readAttributes.contains(
      makePath("name", query->resourceMonitor())));
  EXPECT_TRUE(accesses[0].readAttributes.contains(
      makePath("address", query->resourceMonitor())));
  EXPECT_TRUE(accesses[0].readAttributes.contains(
      makePath("age", query->resourceMonitor())));
  EXPECT_FALSE(accesses[0].requiresAllAttributesRead);
  EXPECT_FALSE(accesses[0].requiresAllAttributesWrite);
}

TEST_F(AttributeDetectorTest, DynamicAttributeAccessWithConcat) {
  auto query = executeQuery(R"aql(
    FOR p IN posts
      FILTER p[CONCAT("ti", "tle")] == "festival"
      RETURN { user: p.userId, title: p.title }
  )aql");
  auto const& accesses = query->abacAccesses();

  ASSERT_EQ(accesses.size(), 1);
  EXPECT_EQ(accesses[0].collectionName, "posts");
  EXPECT_EQ(accesses[0].readAttributes.size(), 2);
  EXPECT_TRUE(accesses[0].readAttributes.contains(
      makePath("userId", query->resourceMonitor())));
  EXPECT_TRUE(accesses[0].readAttributes.contains(
      makePath("title", query->resourceMonitor())));
  EXPECT_FALSE(accesses[0].requiresAllAttributesRead);
  EXPECT_FALSE(accesses[0].requiresAllAttributesWrite);
}

TEST_F(AttributeDetectorTest, NestedAttributeAccess1) {
  auto query = executeQuery(R"aql(
    FOR p IN posts
      FILTER p.title == "festival"
      RETURN p.meta
  )aql");
  auto const& accesses = query->abacAccesses();

  ASSERT_EQ(accesses.size(), 1);
  EXPECT_EQ(accesses[0].collectionName, "posts");
  EXPECT_EQ(accesses[0].readAttributes.size(), 2);
  EXPECT_TRUE(accesses[0].readAttributes.contains(
      makePath("meta", query->resourceMonitor())));
  EXPECT_TRUE(accesses[0].readAttributes.contains(
      makePath("title", query->resourceMonitor())));
  EXPECT_FALSE(accesses[0].requiresAllAttributesRead);
  EXPECT_FALSE(accesses[0].requiresAllAttributesWrite);
}

TEST_F(AttributeDetectorTest, NestedAttributeAccess2) {
  auto query = executeQuery(R"aql(
    FOR p IN posts
      FILTER p.title == "festival"
      RETURN { lang: p.meta.lang, likes: p.meta.likes}
  )aql");
  auto const& accesses = query->abacAccesses();

  ASSERT_EQ(accesses.size(), 1);
  EXPECT_EQ(accesses[0].collectionName, "posts");
  // Full paths: ["title"], ["meta", "lang"], ["meta", "likes"]
  EXPECT_EQ(accesses[0].readAttributes.size(), 3);
  EXPECT_TRUE(accesses[0].readAttributes.contains(
      makePath("title", query->resourceMonitor())));
  EXPECT_TRUE(accesses[0].readAttributes.contains(makePath(
      std::vector<std::string>{"meta", "lang"}, query->resourceMonitor())));
  EXPECT_TRUE(accesses[0].readAttributes.contains(makePath(
      std::vector<std::string>{"meta", "likes"}, query->resourceMonitor())));
  // "meta" alone should not be present
  EXPECT_FALSE(accesses[0].readAttributes.contains(
      makePath("meta", query->resourceMonitor())));
  EXPECT_FALSE(accesses[0].requiresAllAttributesRead);
  EXPECT_FALSE(accesses[0].requiresAllAttributesWrite);
}

TEST_F(AttributeDetectorTest, HasAttribute) {
  auto query = executeQuery(R"aql(
    FOR u IN users
      FILTER HAS(u, "profile")
      RETURN u.name
  )aql");
  auto const& accesses = query->abacAccesses();

  ASSERT_EQ(accesses.size(), 1);
  EXPECT_EQ(accesses[0].collectionName, "users");

  EXPECT_TRUE(accesses[0].readAttributes.contains(
      makePath("profile", query->resourceMonitor())));

  EXPECT_TRUE(accesses[0].readAttributes.contains(
      makePath("name", query->resourceMonitor())));
  EXPECT_FALSE(accesses[0].requiresAllAttributesRead);
  EXPECT_FALSE(accesses[0].requiresAllAttributesWrite);
}

// KEEP() is supposed to read only the specified attribute, or requiresAll?
TEST_F(AttributeDetectorTest, KeepRequiresAll) {
  auto query = executeQuery(R"aql(
    FOR u IN users
      RETURN KEEP(u, "name")
  )aql");
  auto const& accesses = query->abacAccesses();

  ASSERT_EQ(accesses.size(), 1);
  EXPECT_EQ(accesses[0].collectionName, "users");
  EXPECT_TRUE(accesses[0].requiresAllAttributesRead);
  EXPECT_FALSE(accesses[0].requiresAllAttributesWrite);
}

// UNSET() is supposed to read only the specified attribute, or requiresAll?
TEST_F(AttributeDetectorTest, UnsetRequiresAll) {
  auto query = executeQuery(R"aql(
    FOR u IN users
      RETURN UNSET(u, "address")
  )aql");
  auto const& accesses = query->abacAccesses();

  ASSERT_EQ(accesses.size(), 1);
  EXPECT_EQ(accesses[0].collectionName, "users");
  EXPECT_TRUE(accesses[0].requiresAllAttributesRead);
  EXPECT_FALSE(accesses[0].requiresAllAttributesWrite);
}

// ATTRIBUTES() inspects the whole document.
TEST_F(AttributeDetectorTest, AttributesFunctionRequiresAll) {
  auto query = executeQuery(R"aql(
    FOR u IN users
      RETURN ATTRIBUTES(u)
  )aql");
  auto const& accesses = query->abacAccesses();

  ASSERT_EQ(accesses.size(), 1);
  EXPECT_EQ(accesses[0].collectionName, "users");
  EXPECT_TRUE(accesses[0].requiresAllAttributesRead);
  EXPECT_FALSE(accesses[0].requiresAllAttributesWrite);
}

// VALUES() inspects the whole document.
TEST_F(AttributeDetectorTest, ValuesFunctionRequiresAll) {
  auto query = executeQuery(R"aql(
    FOR u IN users
      RETURN VALUES(u)
  )aql");
  auto const& accesses = query->abacAccesses();

  ASSERT_EQ(accesses.size(), 1);
  EXPECT_EQ(accesses[0].collectionName, "users");
  EXPECT_TRUE(accesses[0].requiresAllAttributesRead);
  EXPECT_FALSE(accesses[0].requiresAllAttributesWrite);
}

TEST_F(AttributeDetectorTest, MissingCollectionThrowsWithCorrectErrorCode) {
  try {
    auto q = executeQuery("FOR u IN missingCollection RETURN u.name");
    (void)q;
    FAIL() << "Expected exception";
  } catch (basics::Exception const& ex) {
    EXPECT_EQ(ex.code(), TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
  }
}

TEST_F(AttributeDetectorTest, MissingAttributeStillRecorded1) {
  auto query = executeQuery("FOR u IN users RETURN u.attrNotExist");
  auto const& accesses = query->abacAccesses();

  ASSERT_EQ(accesses.size(), 1);
  EXPECT_EQ(accesses[0].collectionName, "users");
  EXPECT_TRUE(accesses[0].readAttributes.contains(
      makePath("attrNotExist", query->resourceMonitor())));
  EXPECT_FALSE(accesses[0].requiresAllAttributesRead);
  EXPECT_FALSE(accesses[0].requiresAllAttributesWrite);
}

TEST_F(AttributeDetectorTest, MissingAttributeStillRecorded2) {
  auto query =
      executeQuery("FOR u IN users FILTER u.attrNotExist == 1 RETURN u.name");
  auto const& accesses = query->abacAccesses();

  ASSERT_EQ(accesses.size(), 1);
  EXPECT_EQ(accesses[0].collectionName, "users");
  EXPECT_TRUE(accesses[0].readAttributes.contains(
      makePath("attrNotExist", query->resourceMonitor())));
  EXPECT_TRUE(accesses[0].readAttributes.contains(
      makePath("name", query->resourceMonitor())));
  EXPECT_FALSE(accesses[0].requiresAllAttributesRead);
  EXPECT_FALSE(accesses[0].requiresAllAttributesWrite);
}

TEST_F(AttributeDetectorTest, MissingAttributeStillRecorded3) {
  auto query =
      executeQuery("FOR u IN users FILTER u.attrNotExist == 1 RETURN u");
  auto const& accesses = query->abacAccesses();

  ASSERT_EQ(accesses.size(), 1);
  EXPECT_EQ(accesses[0].collectionName, "users");
  EXPECT_TRUE(accesses[0].requiresAllAttributesRead);
  EXPECT_FALSE(accesses[0].requiresAllAttributesWrite);
}

// Edge case tests for special attribute/collection names

TEST_F(AttributeDetectorTest, UTF16CollectionName) {
  // Test with emoji collection name
  auto mkColl = [&](std::string const& name)
      -> std::shared_ptr<arangodb::LogicalCollection> {
    auto json =
        VPackParser::fromJson(std::string("{\"name\":\"") + name + "\"}");
    auto c = vocbase->createCollection(json->slice());
    EXPECT_NE(c.get(), nullptr) << "Failed to create " << name;
    return c;
  };

  auto emojiColl = mkColl("💩");
  auto run = [&](std::string const& q) {
    SCOPED_TRACE(q);
    auto qr = tests::executeQuery(*vocbase, q, VPackParser::fromJson("{ }"));
    ASSERT_TRUE(qr.result.ok()) << qr.result.errorMessage();
  };

  // Insert document with emoji attribute name
  run(R"aql(INSERT {_key:"e1", `🍌`:42, nested:{val:99}} INTO `💩`)aql");

  auto query = executeQuery(R"aql(FOR x IN `💩` RETURN x.`🍌`)aql");
  auto const& accesses = query->abacAccesses();

  ASSERT_EQ(accesses.size(), 1);
  EXPECT_EQ(accesses[0].collectionName, "💩");
  EXPECT_TRUE(accesses[0].readAttributes.contains(
      makePath("🍌", query->resourceMonitor())));
  EXPECT_FALSE(accesses[0].requiresAllAttributesRead);
  EXPECT_FALSE(accesses[0].requiresAllAttributesWrite);
}

TEST_F(AttributeDetectorTest, AttributeNameWithDot) {
  // Test distinguishing between:
  // 1. Nested access: meta.en (path ["meta", "en"])
  // 2. Literal attribute name: `meta.en` (path ["meta.en"])

  auto mkColl = [&](std::string const& name)
      -> std::shared_ptr<arangodb::LogicalCollection> {
    auto json =
        VPackParser::fromJson(std::string("{\"name\":\"") + name + "\"}");
    auto c = vocbase->createCollection(json->slice());
    EXPECT_NE(c.get(), nullptr) << "Failed to create " << name;
    return c;
  };

  auto dotAttrColl = mkColl("dotattrs");
  auto run = [&](std::string const& q) {
    SCOPED_TRACE(q);
    auto qr = tests::executeQuery(*vocbase, q, VPackParser::fromJson("{ }"));
    ASSERT_TRUE(qr.result.ok()) << qr.result.errorMessage();
  };

  // Insert document with both nested structure and literal dot-named attribute
  run(R"aql(INSERT {_key:"d1", `meta.en`:"literal", meta:{en:"nested"}} INTO dotattrs)aql");

  // Test 1: Access literal attribute name `meta.en`
  auto query1 = executeQuery(R"aql(FOR x IN dotattrs RETURN x.`meta.en`)aql");
  auto const& accesses1 = query1->abacAccesses();

  ASSERT_EQ(accesses1.size(), 1);
  EXPECT_EQ(accesses1[0].collectionName, "dotattrs");
  // Should contain the literal attribute "meta.en", not "meta"
  EXPECT_TRUE(accesses1[0].readAttributes.contains(
      makePath("meta.en", query1->resourceMonitor())));
  EXPECT_FALSE(accesses1[0].readAttributes.contains(
      makePath("meta", query1->resourceMonitor())));
  EXPECT_FALSE(accesses1[0].requiresAllAttributesRead);

  // Test 2: Access nested attribute meta.en (without backticks)
  auto query2 = executeQuery(R"aql(FOR x IN dotattrs RETURN x.meta.en)aql");
  auto const& accesses2 = query2->abacAccesses();

  ASSERT_EQ(accesses2.size(), 1);
  EXPECT_EQ(accesses2[0].collectionName, "dotattrs");
  // Should contain full path ["meta", "en"], not just "meta" or "en" alone
  EXPECT_TRUE(accesses2[0].readAttributes.contains(makePath(
      std::vector<std::string>{"meta", "en"}, query2->resourceMonitor())));
  EXPECT_FALSE(accesses2[0].readAttributes.contains(
      makePath("meta", query2->resourceMonitor())));
  EXPECT_FALSE(accesses2[0].readAttributes.contains(
      makePath("en", query2->resourceMonitor())));
  // Should NOT contain literal "meta.en" (that's a different attribute)
  EXPECT_FALSE(accesses2[0].readAttributes.contains(
      makePath("meta.en", query2->resourceMonitor())));
  EXPECT_FALSE(accesses2[0].requiresAllAttributesRead);
}

TEST_F(AttributeDetectorTest, DeeplyNestedAttributeAccess) {
  auto query = executeQuery(R"aql(
    FOR u IN users
      RETURN u.profile.tags[0]
  )aql");
  auto const& accesses = query->abacAccesses();

  ASSERT_EQ(accesses.size(), 1);
  EXPECT_EQ(accesses[0].collectionName, "users");
  // Full path ["profile", "tags"] should be tracked
  EXPECT_EQ(accesses[0].readAttributes.size(), 1);
  EXPECT_TRUE(accesses[0].readAttributes.contains(makePath(
      std::vector<std::string>{"profile", "tags"}, query->resourceMonitor())));
  // Individual components should NOT be present as separate entries
  EXPECT_FALSE(accesses[0].readAttributes.contains(
      makePath("profile", query->resourceMonitor())));
  EXPECT_FALSE(accesses[0].readAttributes.contains(
      makePath("tags", query->resourceMonitor())));
  EXPECT_FALSE(accesses[0].requiresAllAttributesRead);
  EXPECT_FALSE(accesses[0].requiresAllAttributesWrite);
}

TEST_F(AttributeDetectorTest, MultipleNestedPathsSameTopLevel) {
  auto query = executeQuery(R"aql(
    FOR u IN users
      RETURN {
        bio: u.profile.bio,
        tags: u.profile.tags,
        name: u.name
      }
  )aql");
  auto const& accesses = query->abacAccesses();

  ASSERT_EQ(accesses.size(), 1);
  EXPECT_EQ(accesses[0].collectionName, "users");
  // Full paths are tracked: ["profile", "bio"], ["profile", "tags"], ["name"]
  EXPECT_EQ(accesses[0].readAttributes.size(), 3);
  EXPECT_TRUE(accesses[0].readAttributes.contains(makePath(
      std::vector<std::string>{"profile", "bio"}, query->resourceMonitor())));
  EXPECT_TRUE(accesses[0].readAttributes.contains(makePath(
      std::vector<std::string>{"profile", "tags"}, query->resourceMonitor())));
  EXPECT_TRUE(accesses[0].readAttributes.contains(
      makePath("name", query->resourceMonitor())));
  // Individual components should NOT be present as separate entries
  EXPECT_FALSE(accesses[0].readAttributes.contains(
      makePath("profile", query->resourceMonitor())));
  EXPECT_FALSE(accesses[0].readAttributes.contains(
      makePath("bio", query->resourceMonitor())));
  EXPECT_FALSE(accesses[0].readAttributes.contains(
      makePath("tags", query->resourceMonitor())));
  EXPECT_FALSE(accesses[0].requiresAllAttributesRead);
  EXPECT_FALSE(accesses[0].requiresAllAttributesWrite);
}

TEST_F(AttributeDetectorTest, DynamicAttributeAccess) {
  // Test dynamic attribute access with bracket notation
  auto query = executeQuery(R"aql(
    FOR u IN users
      RETURN u["name"]
  )aql");
  auto const& accesses = query->abacAccesses();

  ASSERT_EQ(accesses.size(), 1);
  EXPECT_EQ(accesses[0].collectionName, "users");
  // Dynamic access with literal string should still track the attribute
  EXPECT_TRUE(accesses[0].readAttributes.contains(
      makePath("name", query->resourceMonitor())));
  EXPECT_FALSE(accesses[0].requiresAllAttributesRead);
  EXPECT_FALSE(accesses[0].requiresAllAttributesWrite);
}

TEST_F(AttributeDetectorTest, DynamicAttributeAccessVariable) {
  // Test dynamic attribute access with variable - should require all attributes
  auto query = executeQuery(R"aql(
    LET attr = "name"
    FOR u IN users
      RETURN u[attr]
  )aql");
  auto const& accesses = query->abacAccesses();

  ASSERT_EQ(accesses.size(), 1);
  EXPECT_EQ(accesses[0].collectionName, "users");
  // Dynamic access with variable cannot be determined statically
  EXPECT_TRUE(accesses[0].requiresAllAttributesRead);
  EXPECT_FALSE(accesses[0].requiresAllAttributesWrite);
}

}  // namespace arangodb::tests::aql
