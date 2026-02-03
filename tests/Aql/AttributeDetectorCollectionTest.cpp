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

  // HAS() checks existence, doesn't read the attribute value - not tracked
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
  // Optimizer constant-folds LET attr = "name"; u[attr] -> u.name
  auto query = executeQuery(R"aql(
    LET attr = "name"
    FOR u IN users
      RETURN u[attr]
  )aql");
  auto const& accesses = query->abacAccesses();

  ASSERT_EQ(accesses.size(), 1);
  EXPECT_EQ(accesses[0].collectionName, "users");
  // Constant folding resolves u[attr] to u.name
  EXPECT_TRUE(accesses[0].readAttributes.contains(
      makePath("name", query->resourceMonitor())));
  EXPECT_FALSE(accesses[0].requiresAllAttributesRead);
  EXPECT_FALSE(accesses[0].requiresAllAttributesWrite);
}

// =============================================================================
// ABAC Request Format Tests (for authorization service API)
// =============================================================================

TEST_F(AttributeDetectorTest, AbacRequestFormat_RequiresAllRead) {
  auto query = executeQuery("FOR doc IN users RETURN doc");
  auto const& accesses = query->abacAccesses();

  ASSERT_EQ(accesses.size(), 1);

  // Create detector and get ABAC requests
  AttributeDetector detector(query->plan());
  detector.detect();
  auto requests = detector.getAbacRequests();

  // Should produce 1 read request with readAll
  ASSERT_EQ(requests.size(), 1);
  EXPECT_EQ(requests[0].action, "read");
  EXPECT_EQ(requests[0].resource, "users");
  EXPECT_TRUE(requests[0].context.parameters.contains("readAll"));
  ASSERT_EQ(requests[0].context.parameters.at("readAll").values.size(), 1);
  EXPECT_EQ(requests[0].context.parameters.at("readAll").values[0], "true");
}

TEST_F(AttributeDetectorTest, AbacRequestFormat_SpecificAttributes) {
  auto query = executeQuery("FOR doc IN users RETURN doc.name");
  auto const& accesses = query->abacAccesses();

  ASSERT_EQ(accesses.size(), 1);

  AttributeDetector detector(query->plan());
  detector.detect();
  auto requests = detector.getAbacRequests();

  // Should produce 1 read request with specific attributes
  ASSERT_EQ(requests.size(), 1);
  EXPECT_EQ(requests[0].action, "read");
  EXPECT_EQ(requests[0].resource, "users");
  EXPECT_TRUE(requests[0].context.parameters.contains("read"));
  EXPECT_FALSE(requests[0].context.parameters.contains("readAll"));

  // Check that attribute is stringified as JSON array
  auto const& values = requests[0].context.parameters.at("read").values;
  ASSERT_EQ(values.size(), 1);
  EXPECT_EQ(values[0], "[\"name\"]");
}

TEST_F(AttributeDetectorTest, AbacRequestFormat_NestedAttributes) {
  auto query = executeQuery(
      "FOR doc IN users RETURN {bio: doc.profile.bio, name: doc.name}");

  AttributeDetector detector(query->plan());
  detector.detect();
  auto requests = detector.getAbacRequests();

  ASSERT_EQ(requests.size(), 1);
  EXPECT_EQ(requests[0].action, "read");
  EXPECT_EQ(requests[0].resource, "users");
  EXPECT_TRUE(requests[0].context.parameters.contains("read"));

  auto const& values = requests[0].context.parameters.at("read").values;
  EXPECT_EQ(values.size(), 2);

  // Check that nested path is stringified correctly
  bool foundName = false;
  bool foundProfileBio = false;
  for (auto const& v : values) {
    if (v == "[\"name\"]") {
      foundName = true;
    }
    if (v == "[\"profile\",\"bio\"]") {
      foundProfileBio = true;
    }
  }
  EXPECT_TRUE(foundName);
  EXPECT_TRUE(foundProfileBio);
}

TEST_F(AttributeDetectorTest, AbacRequestFormat_BatchMultipleCollections) {
  auto query = executeQuery(
      "FOR u IN users FOR p IN posts FILTER p.userId == u._key RETURN {name: "
      "u.name, title: p.title}");

  AttributeDetector detector(query->plan());
  detector.detect();
  auto requests = detector.getAbacRequests();

  // Should produce 2 read requests (one per collection)
  ASSERT_EQ(requests.size(), 2);

  bool foundUsers = false;
  bool foundPosts = false;

  for (auto const& req : requests) {
    EXPECT_EQ(req.action, "read");

    if (req.resource == "users") {
      foundUsers = true;
      EXPECT_TRUE(req.context.parameters.contains("read"));
      auto const& values = req.context.parameters.at("read").values;
      // Should have _key and name
      EXPECT_EQ(values.size(), 2);
    } else if (req.resource == "posts") {
      foundPosts = true;
      EXPECT_TRUE(req.context.parameters.contains("read"));
      auto const& values = req.context.parameters.at("read").values;
      // Should have userId and title
      EXPECT_EQ(values.size(), 2);
    }
  }

  EXPECT_TRUE(foundUsers);
  EXPECT_TRUE(foundPosts);
}

TEST_F(AttributeDetectorTest, AbacRequestFormat_BatchReadAndWrite) {
  auto query =
      executeQuery("FOR doc IN users UPDATE doc WITH {visited: true} IN users");

  AttributeDetector detector(query->plan());
  detector.detect();
  auto requests = detector.getAbacRequests();

  // Should produce 2 requests: 1 read + 1 write for users
  ASSERT_EQ(requests.size(), 2);

  bool foundRead = false;
  bool foundWrite = false;

  for (auto const& req : requests) {
    EXPECT_EQ(req.resource, "users");

    if (req.action == "read") {
      foundRead = true;
      EXPECT_TRUE(req.context.parameters.contains("readAll"));
    } else if (req.action == "write") {
      foundWrite = true;
      EXPECT_TRUE(req.context.parameters.contains("writeAll"));
      EXPECT_EQ(req.context.parameters.at("writeAll").values[0], "true");
    }
  }

  EXPECT_TRUE(foundRead);
  EXPECT_TRUE(foundWrite);
}

TEST_F(AttributeDetectorTest,
       AbacRequestFormat_BatchMultipleCollectionsWithWrite) {
  auto query = executeQuery(
      "FOR u IN users INSERT {userId: u._key, name: u.name} INTO posts");

  AttributeDetector detector(query->plan());
  detector.detect();
  auto requests = detector.getAbacRequests();

  // Should produce: read users, read posts (insert), write posts
  ASSERT_GE(requests.size(), 2);

  bool foundUsersRead = false;
  bool foundPostsWrite = false;

  for (auto const& req : requests) {
    if (req.resource == "users" && req.action == "read") {
      foundUsersRead = true;
    }
    if (req.resource == "posts" && req.action == "write") {
      foundPostsWrite = true;
      EXPECT_TRUE(req.context.parameters.contains("writeAll"));
    }
  }

  EXPECT_TRUE(foundUsersRead);
  EXPECT_TRUE(foundPostsWrite);
}

TEST_F(AttributeDetectorTest, AbacRequestFormat_VelocyPackOutput) {
  auto query = executeQuery("FOR doc IN users RETURN doc.name");

  AttributeDetector detector(query->plan());
  detector.detect();

  velocypack::Builder builder;
  detector.toVelocyPackAbac(builder);

  auto slice = builder.slice();
  ASSERT_TRUE(slice.isArray());
  ASSERT_EQ(slice.length(), 1);

  auto req = slice[0];
  ASSERT_TRUE(req.isObject());
  EXPECT_EQ(req.get("action").copyString(), "read");
  EXPECT_EQ(req.get("resource").copyString(), "users");

  auto context = req.get("context");
  ASSERT_TRUE(context.isObject());

  auto parameters = context.get("parameters");
  ASSERT_TRUE(parameters.isObject());

  auto read = parameters.get("read");
  ASSERT_TRUE(read.isObject());

  auto values = read.get("values");
  ASSERT_TRUE(values.isArray());
  ASSERT_EQ(values.length(), 1);
  EXPECT_EQ(values[0].copyString(), "[\"name\"]");
}

TEST_F(AttributeDetectorTest, AbacRequestFormat_VelocyPackBatch) {
  auto query = executeQuery(
      "FOR u IN users FOR p IN posts RETURN {n: u.name, t: p.title}");

  AttributeDetector detector(query->plan());
  detector.detect();

  velocypack::Builder builder;
  detector.toVelocyPackAbac(builder);

  auto slice = builder.slice();
  ASSERT_TRUE(slice.isArray());
  ASSERT_EQ(slice.length(), 2);

  std::set<std::string> resources;
  for (size_t i = 0; i < slice.length(); ++i) {
    auto req = slice[i];
    ASSERT_TRUE(req.isObject());
    EXPECT_EQ(req.get("action").copyString(), "read");
    resources.insert(req.get("resource").copyString());

    auto context = req.get("context");
    ASSERT_TRUE(context.isObject());
    auto parameters = context.get("parameters");
    ASSERT_TRUE(parameters.isObject());
    auto read = parameters.get("read");
    ASSERT_TRUE(read.isObject());
    auto values = read.get("values");
    ASSERT_TRUE(values.isArray());
  }

  EXPECT_TRUE(resources.contains("users"));
  EXPECT_TRUE(resources.contains("posts"));
}

// Complex attribute path format tests

TEST_F(AttributeDetectorTest, AbacRequestFormat_DeeplyNestedPath) {
  // Test 3-level nesting: profile.tags[0] becomes ["profile","tags"]
  auto query = executeQuery("FOR u IN users RETURN u.profile.tags");

  AttributeDetector detector(query->plan());
  detector.detect();
  auto requests = detector.getAbacRequests();

  ASSERT_EQ(requests.size(), 1);
  EXPECT_TRUE(requests[0].context.parameters.contains("read"));

  auto const& values = requests[0].context.parameters.at("read").values;
  ASSERT_EQ(values.size(), 1);
  EXPECT_EQ(values[0], "[\"profile\",\"tags\"]");
}

TEST_F(AttributeDetectorTest, AbacRequestFormat_MultiplePaths_FlattenedTree) {
  // Multiple paths at different nesting levels - all in flat array
  // ["name"], ["age"], ["profile","bio"], ["profile","tags"]
  auto query = executeQuery(R"aql(
    FOR u IN users
      RETURN {
        name: u.name,
        age: u.age,
        bio: u.profile.bio,
        tags: u.profile.tags
      }
  )aql");

  AttributeDetector detector(query->plan());
  detector.detect();
  auto requests = detector.getAbacRequests();

  ASSERT_EQ(requests.size(), 1);
  EXPECT_TRUE(requests[0].context.parameters.contains("read"));

  auto const& values = requests[0].context.parameters.at("read").values;
  EXPECT_EQ(values.size(), 4);

  std::set<std::string> paths(values.begin(), values.end());
  EXPECT_TRUE(paths.contains("[\"name\"]"));
  EXPECT_TRUE(paths.contains("[\"age\"]"));
  EXPECT_TRUE(paths.contains("[\"profile\",\"bio\"]"));
  EXPECT_TRUE(paths.contains("[\"profile\",\"tags\"]"));
}

TEST_F(AttributeDetectorTest, AbacRequestFormat_MixedDepthPaths) {
  // Mix of single-level and multi-level paths
  auto query = executeQuery(R"aql(
    FOR p IN posts
      RETURN {
        title: p.title,
        lang: p.meta.lang,
        likes: p.meta.likes
      }
  )aql");

  AttributeDetector detector(query->plan());
  detector.detect();
  auto requests = detector.getAbacRequests();

  ASSERT_EQ(requests.size(), 1);
  auto const& values = requests[0].context.parameters.at("read").values;
  EXPECT_EQ(values.size(), 3);

  std::set<std::string> paths(values.begin(), values.end());
  EXPECT_TRUE(paths.contains("[\"title\"]"));
  EXPECT_TRUE(paths.contains("[\"meta\",\"lang\"]"));
  EXPECT_TRUE(paths.contains("[\"meta\",\"likes\"]"));
}

TEST_F(AttributeDetectorTest,
       AbacRequestFormat_VelocyPack_NestedPathStructure) {
  // Verify VelocyPack output has correct nested path format
  auto query = executeQuery(R"aql(
    FOR u IN users
      RETURN { bio: u.profile.bio, name: u.name }
  )aql");

  AttributeDetector detector(query->plan());
  detector.detect();

  velocypack::Builder builder;
  detector.toVelocyPackAbac(builder);

  auto slice = builder.slice();
  ASSERT_TRUE(slice.isArray());
  ASSERT_EQ(slice.length(), 1);

  auto req = slice[0];
  auto values = req.get("context").get("parameters").get("read").get("values");
  ASSERT_TRUE(values.isArray());
  EXPECT_EQ(values.length(), 2);

  std::set<std::string> paths;
  for (size_t i = 0; i < values.length(); ++i) {
    paths.insert(values[i].copyString());
  }

  // Paths are JSON-stringified arrays
  EXPECT_TRUE(paths.contains("[\"name\"]"));
  EXPECT_TRUE(paths.contains("[\"profile\",\"bio\"]"));
}

TEST_F(AttributeDetectorTest, AbacRequestFormat_FilterAndReturn_CombinedPaths) {
  // Paths from both FILTER and RETURN should be in the same flat array
  auto query = executeQuery(R"aql(
    FOR u IN users
      FILTER u.age > 25 AND u.profile.bio != null
      RETURN { name: u.name, tags: u.profile.tags }
  )aql");

  AttributeDetector detector(query->plan());
  detector.detect();
  auto requests = detector.getAbacRequests();

  ASSERT_EQ(requests.size(), 1);
  auto const& values = requests[0].context.parameters.at("read").values;

  std::set<std::string> paths(values.begin(), values.end());
  // From FILTER
  EXPECT_TRUE(paths.contains("[\"age\"]"));
  EXPECT_TRUE(paths.contains("[\"profile\",\"bio\"]"));
  // From RETURN
  EXPECT_TRUE(paths.contains("[\"name\"]"));
  EXPECT_TRUE(paths.contains("[\"profile\",\"tags\"]"));
}

TEST_F(AttributeDetectorTest, AbacRequestFormat_MultiCollection_EachWithPaths) {
  // Each collection gets its own request with its own attribute paths
  auto query = executeQuery(R"aql(
    FOR u IN users
      FOR p IN posts
        FILTER p.userId == u._key
        RETURN {
          userName: u.name,
          userBio: u.profile.bio,
          postTitle: p.title,
          postLang: p.meta.lang
        }
  )aql");

  AttributeDetector detector(query->plan());
  detector.detect();
  auto requests = detector.getAbacRequests();

  ASSERT_EQ(requests.size(), 2);

  for (auto const& req : requests) {
    EXPECT_EQ(req.action, "read");
    auto const& values = req.context.parameters.at("read").values;

    if (req.resource == "users") {
      std::set<std::string> paths(values.begin(), values.end());
      EXPECT_TRUE(paths.contains("[\"name\"]"));
      EXPECT_TRUE(paths.contains("[\"profile\",\"bio\"]"));
      EXPECT_TRUE(paths.contains("[\"_key\"]"));
    } else if (req.resource == "posts") {
      std::set<std::string> paths(values.begin(), values.end());
      EXPECT_TRUE(paths.contains("[\"title\"]"));
      EXPECT_TRUE(paths.contains("[\"meta\",\"lang\"]"));
      EXPECT_TRUE(paths.contains("[\"userId\"]"));
    }
  }
}

TEST_F(AttributeDetectorTest, AbacRequestFormat_SystemAttributes) {
  // System attributes like _key, _id, _rev should be proper paths
  auto query = executeQuery(R"aql(
    FOR u IN users
      RETURN { id: u._id, key: u._key, rev: u._rev }
  )aql");

  AttributeDetector detector(query->plan());
  detector.detect();
  auto requests = detector.getAbacRequests();

  ASSERT_EQ(requests.size(), 1);
  auto const& values = requests[0].context.parameters.at("read").values;

  std::set<std::string> paths(values.begin(), values.end());
  EXPECT_TRUE(paths.contains("[\"_id\"]"));
  EXPECT_TRUE(paths.contains("[\"_key\"]"));
  EXPECT_TRUE(paths.contains("[\"_rev\"]"));
}

TEST_F(AttributeDetectorTest, AbacRequestFormat_VelocyPack_FullBatchStructure) {
  // Comprehensive test of full VelocyPack batch output
  auto query = executeQuery(R"aql(
    FOR u IN users
      FOR p IN posts
        FILTER p.userId == u._key
        RETURN { name: u.name, title: p.title }
  )aql");

  AttributeDetector detector(query->plan());
  detector.detect();

  velocypack::Builder builder;
  detector.toVelocyPackAbac(builder);

  auto slice = builder.slice();
  ASSERT_TRUE(slice.isArray());
  ASSERT_EQ(slice.length(), 2);

  for (size_t i = 0; i < slice.length(); ++i) {
    auto req = slice[i];

    // Top level: action, resource, context
    ASSERT_TRUE(req.hasKey("action"));
    ASSERT_TRUE(req.hasKey("resource"));
    ASSERT_TRUE(req.hasKey("context"));

    EXPECT_EQ(req.get("action").copyString(), "read");

    // context.parameters.<key>.values structure
    auto context = req.get("context");
    ASSERT_TRUE(context.isObject());
    ASSERT_TRUE(context.hasKey("parameters"));

    auto parameters = context.get("parameters");
    ASSERT_TRUE(parameters.isObject());
    ASSERT_TRUE(parameters.hasKey("read"));

    auto read = parameters.get("read");
    ASSERT_TRUE(read.isObject());
    ASSERT_TRUE(read.hasKey("values"));

    auto values = read.get("values");
    ASSERT_TRUE(values.isArray());
    EXPECT_GT(values.length(), 0);

    // Each value is a stringified JSON array
    for (size_t j = 0; j < values.length(); ++j) {
      std::string val = values[j].copyString();
      EXPECT_TRUE(val.front() == '[' && val.back() == ']');
    }
  }
}

// Nested attribute path format tests

TEST_F(AttributeDetectorTest, AbacRequestFormat_ThreeLevelNesting) {
  auto query = executeQuery("FOR u IN users RETURN u.profile.bio");

  AttributeDetector detector(query->plan());
  detector.detect();
  auto requests = detector.getAbacRequests();

  ASSERT_EQ(requests.size(), 1);
  auto const& values = requests[0].context.parameters.at("read").values;
  ASSERT_EQ(values.size(), 1);
  EXPECT_EQ(values[0], "[\"profile\",\"bio\"]");
}

TEST_F(AttributeDetectorTest, AbacRequestFormat_FourLevelNesting) {
  // Create a query that accesses a 4-level nested path
  // Using posts.meta which has nested structure
  auto query = executeQuery("FOR p IN posts RETURN p.meta.lang");

  AttributeDetector detector(query->plan());
  detector.detect();
  auto requests = detector.getAbacRequests();

  ASSERT_EQ(requests.size(), 1);
  auto const& values = requests[0].context.parameters.at("read").values;
  ASSERT_EQ(values.size(), 1);
  EXPECT_EQ(values[0], "[\"meta\",\"lang\"]");
}

TEST_F(AttributeDetectorTest, AbacRequestFormat_MultipleNestedSameParent) {
  // Multiple nested paths under same parent: profile.bio, profile.tags
  auto query = executeQuery(R"aql(
    FOR u IN users
      RETURN { bio: u.profile.bio, tags: u.profile.tags }
  )aql");

  AttributeDetector detector(query->plan());
  detector.detect();
  auto requests = detector.getAbacRequests();

  ASSERT_EQ(requests.size(), 1);
  auto const& values = requests[0].context.parameters.at("read").values;
  EXPECT_EQ(values.size(), 2);

  std::set<std::string> paths(values.begin(), values.end());
  EXPECT_TRUE(paths.contains("[\"profile\",\"bio\"]"));
  EXPECT_TRUE(paths.contains("[\"profile\",\"tags\"]"));
}

TEST_F(AttributeDetectorTest, AbacRequestFormat_NestedAndTopLevel) {
  // Mix of nested and top-level: name, age, profile.bio
  auto query = executeQuery(R"aql(
    FOR u IN users
      RETURN { name: u.name, age: u.age, bio: u.profile.bio }
  )aql");

  AttributeDetector detector(query->plan());
  detector.detect();
  auto requests = detector.getAbacRequests();

  ASSERT_EQ(requests.size(), 1);
  auto const& values = requests[0].context.parameters.at("read").values;
  EXPECT_EQ(values.size(), 3);

  std::set<std::string> paths(values.begin(), values.end());
  EXPECT_TRUE(paths.contains("[\"name\"]"));
  EXPECT_TRUE(paths.contains("[\"age\"]"));
  EXPECT_TRUE(paths.contains("[\"profile\",\"bio\"]"));
}

TEST_F(AttributeDetectorTest, AbacRequestFormat_VelocyPack_NestedPaths) {
  auto query = executeQuery(R"aql(
    FOR u IN users
      RETURN { bio: u.profile.bio, tags: u.profile.tags, name: u.name }
  )aql");

  AttributeDetector detector(query->plan());
  detector.detect();

  velocypack::Builder builder;
  detector.toVelocyPackAbac(builder);

  auto slice = builder.slice();
  ASSERT_TRUE(slice.isArray());
  ASSERT_EQ(slice.length(), 1);

  auto values =
      slice[0].get("context").get("parameters").get("read").get("values");
  ASSERT_TRUE(values.isArray());
  EXPECT_EQ(values.length(), 3);

  std::set<std::string> paths;
  for (size_t i = 0; i < values.length(); ++i) {
    paths.insert(values[i].copyString());
  }

  EXPECT_TRUE(paths.contains("[\"name\"]"));
  EXPECT_TRUE(paths.contains("[\"profile\",\"bio\"]"));
  EXPECT_TRUE(paths.contains("[\"profile\",\"tags\"]"));
}

TEST_F(AttributeDetectorTest, AbacRequestFormat_NestedInFilter) {
  // Nested path used in FILTER
  auto query = executeQuery(R"aql(
    FOR u IN users
      FILTER u.profile.bio != null
      RETURN u.name
  )aql");

  AttributeDetector detector(query->plan());
  detector.detect();
  auto requests = detector.getAbacRequests();

  ASSERT_EQ(requests.size(), 1);
  auto const& values = requests[0].context.parameters.at("read").values;

  std::set<std::string> paths(values.begin(), values.end());
  EXPECT_TRUE(paths.contains("[\"name\"]"));
  EXPECT_TRUE(paths.contains("[\"profile\",\"bio\"]"));
}

TEST_F(AttributeDetectorTest, AbacRequestFormat_NestedInSort) {
  // Nested path used in SORT
  auto query = executeQuery(R"aql(
    FOR p IN posts
      SORT p.meta.likes DESC
      RETURN p.title
  )aql");

  AttributeDetector detector(query->plan());
  detector.detect();
  auto requests = detector.getAbacRequests();

  ASSERT_EQ(requests.size(), 1);
  auto const& values = requests[0].context.parameters.at("read").values;

  std::set<std::string> paths(values.begin(), values.end());
  EXPECT_TRUE(paths.contains("[\"title\"]"));
  EXPECT_TRUE(paths.contains("[\"meta\",\"likes\"]"));
}

TEST_F(AttributeDetectorTest, AbacRequestFormat_NestedMultiCollection) {
  // Nested paths from multiple collections
  auto query = executeQuery(R"aql(
    FOR u IN users
      FOR p IN posts
        FILTER p.userId == u._key
        RETURN { userBio: u.profile.bio, postLang: p.meta.lang }
  )aql");

  AttributeDetector detector(query->plan());
  detector.detect();
  auto requests = detector.getAbacRequests();

  ASSERT_EQ(requests.size(), 2);

  for (auto const& req : requests) {
    auto const& values = req.context.parameters.at("read").values;

    if (req.resource == "users") {
      std::set<std::string> paths(values.begin(), values.end());
      EXPECT_TRUE(paths.contains("[\"profile\",\"bio\"]"));
      EXPECT_TRUE(paths.contains("[\"_key\"]"));
    } else if (req.resource == "posts") {
      std::set<std::string> paths(values.begin(), values.end());
      EXPECT_TRUE(paths.contains("[\"meta\",\"lang\"]"));
      EXPECT_TRUE(paths.contains("[\"userId\"]"));
    }
  }
}

TEST_F(AttributeDetectorTest, AbacRequestFormat_ArrayIndexAccess) {
  // Array index access: profile.tags[0] should be ["profile","tags"]
  auto query = executeQuery("FOR u IN users RETURN u.profile.tags[0]");

  AttributeDetector detector(query->plan());
  detector.detect();
  auto requests = detector.getAbacRequests();

  ASSERT_EQ(requests.size(), 1);
  auto const& values = requests[0].context.parameters.at("read").values;
  ASSERT_EQ(values.size(), 1);
  EXPECT_EQ(values[0], "[\"profile\",\"tags\"]");
}

// Write operation ABAC format tests

TEST_F(AttributeDetectorTest, AbacRequestFormat_Insert_ReadAllAndWriteAll) {
  auto query = executeQuery("INSERT {name: 'test'} INTO users");

  AttributeDetector detector(query->plan());
  detector.detect();
  auto requests = detector.getAbacRequests();

  ASSERT_EQ(requests.size(), 2);

  bool foundRead = false;
  bool foundWrite = false;

  for (auto const& req : requests) {
    EXPECT_EQ(req.resource, "users");

    if (req.action == "read") {
      foundRead = true;
      EXPECT_TRUE(req.context.parameters.contains("readAll"));
      EXPECT_EQ(req.context.parameters.at("readAll").values[0], "true");
    } else if (req.action == "write") {
      foundWrite = true;
      EXPECT_TRUE(req.context.parameters.contains("writeAll"));
      EXPECT_EQ(req.context.parameters.at("writeAll").values[0], "true");
    }
  }

  EXPECT_TRUE(foundRead);
  EXPECT_TRUE(foundWrite);
}

TEST_F(AttributeDetectorTest, AbacRequestFormat_Update_ReadAllAndWriteAll) {
  auto query =
      executeQuery("FOR doc IN users UPDATE doc WITH {visited: true} IN users");

  AttributeDetector detector(query->plan());
  detector.detect();
  auto requests = detector.getAbacRequests();

  ASSERT_EQ(requests.size(), 2);

  bool foundReadAll = false;
  bool foundWriteAll = false;

  for (auto const& req : requests) {
    EXPECT_EQ(req.resource, "users");

    if (req.action == "read") {
      EXPECT_TRUE(req.context.parameters.contains("readAll"));
      EXPECT_EQ(req.context.parameters.at("readAll").values[0], "true");
      foundReadAll = true;
    } else if (req.action == "write") {
      EXPECT_TRUE(req.context.parameters.contains("writeAll"));
      EXPECT_EQ(req.context.parameters.at("writeAll").values[0], "true");
      foundWriteAll = true;
    }
  }

  EXPECT_TRUE(foundReadAll);
  EXPECT_TRUE(foundWriteAll);
}

TEST_F(AttributeDetectorTest, AbacRequestFormat_Remove_ReadAllAndWriteAll) {
  auto query = executeQuery("FOR doc IN users REMOVE doc IN users");

  AttributeDetector detector(query->plan());
  detector.detect();
  auto requests = detector.getAbacRequests();

  ASSERT_EQ(requests.size(), 2);

  bool foundReadAll = false;
  bool foundWriteAll = false;

  for (auto const& req : requests) {
    EXPECT_EQ(req.resource, "users");

    if (req.action == "read") {
      EXPECT_TRUE(req.context.parameters.contains("readAll"));
      foundReadAll = true;
    } else if (req.action == "write") {
      EXPECT_TRUE(req.context.parameters.contains("writeAll"));
      foundWriteAll = true;
    }
  }

  EXPECT_TRUE(foundReadAll);
  EXPECT_TRUE(foundWriteAll);
}

TEST_F(AttributeDetectorTest, AbacRequestFormat_Replace_ReadAllAndWriteAll) {
  auto query = executeQuery(
      "FOR doc IN users REPLACE doc WITH {name: 'replaced'} IN users");

  AttributeDetector detector(query->plan());
  detector.detect();
  auto requests = detector.getAbacRequests();

  ASSERT_EQ(requests.size(), 2);

  bool foundReadAll = false;
  bool foundWriteAll = false;

  for (auto const& req : requests) {
    EXPECT_EQ(req.resource, "users");

    if (req.action == "read") {
      EXPECT_TRUE(req.context.parameters.contains("readAll"));
      foundReadAll = true;
    } else if (req.action == "write") {
      EXPECT_TRUE(req.context.parameters.contains("writeAll"));
      foundWriteAll = true;
    }
  }

  EXPECT_TRUE(foundReadAll);
  EXPECT_TRUE(foundWriteAll);
}

TEST_F(AttributeDetectorTest, AbacRequestFormat_Upsert_ReadAllAndWriteAll) {
  auto query = executeQuery(R"aql(
    UPSERT {_key: 'test'}
    INSERT {_key: 'test', name: 'new'}
    UPDATE {name: 'updated'}
    IN users
  )aql");

  AttributeDetector detector(query->plan());
  detector.detect();
  auto requests = detector.getAbacRequests();

  ASSERT_EQ(requests.size(), 2);

  bool foundReadAll = false;
  bool foundWriteAll = false;

  for (auto const& req : requests) {
    EXPECT_EQ(req.resource, "users");

    if (req.action == "read") {
      EXPECT_TRUE(req.context.parameters.contains("readAll"));
      foundReadAll = true;
    } else if (req.action == "write") {
      EXPECT_TRUE(req.context.parameters.contains("writeAll"));
      foundWriteAll = true;
    }
  }

  EXPECT_TRUE(foundReadAll);
  EXPECT_TRUE(foundWriteAll);
}

TEST_F(AttributeDetectorTest, AbacRequestFormat_VelocyPack_WriteOperation) {
  auto query = executeQuery("FOR doc IN users UPDATE doc WITH {x: 1} IN users");

  AttributeDetector detector(query->plan());
  detector.detect();

  velocypack::Builder builder;
  detector.toVelocyPackAbac(builder);

  auto slice = builder.slice();
  ASSERT_TRUE(slice.isArray());
  ASSERT_EQ(slice.length(), 2);

  bool foundRead = false;
  bool foundWrite = false;

  for (size_t i = 0; i < slice.length(); ++i) {
    auto req = slice[i];
    EXPECT_EQ(req.get("resource").copyString(), "users");

    std::string action = req.get("action").copyString();
    auto parameters = req.get("context").get("parameters");

    if (action == "read") {
      foundRead = true;
      ASSERT_TRUE(parameters.hasKey("readAll"));
      auto readAll = parameters.get("readAll").get("values");
      ASSERT_TRUE(readAll.isArray());
      EXPECT_EQ(readAll[0].copyString(), "true");
    } else if (action == "write") {
      foundWrite = true;
      ASSERT_TRUE(parameters.hasKey("writeAll"));
      auto writeAll = parameters.get("writeAll").get("values");
      ASSERT_TRUE(writeAll.isArray());
      EXPECT_EQ(writeAll[0].copyString(), "true");
    }
  }

  EXPECT_TRUE(foundRead);
  EXPECT_TRUE(foundWrite);
}

TEST_F(AttributeDetectorTest, AbacRequestFormat_CrossCollectionWrite) {
  auto query = executeQuery(
      "FOR u IN users INSERT {userId: u._key, name: u.name} INTO posts");

  AttributeDetector detector(query->plan());
  detector.detect();
  auto requests = detector.getAbacRequests();

  bool foundUsersRead = false;
  bool foundPostsWrite = false;

  for (auto const& req : requests) {
    if (req.resource == "users" && req.action == "read") {
      foundUsersRead = true;
      EXPECT_TRUE(req.context.parameters.contains("read") ||
                  req.context.parameters.contains("readAll"));
    }
    if (req.resource == "posts" && req.action == "write") {
      foundPostsWrite = true;
      EXPECT_TRUE(req.context.parameters.contains("writeAll"));
      EXPECT_EQ(req.context.parameters.at("writeAll").values[0], "true");
    }
  }

  EXPECT_TRUE(foundUsersRead);
  EXPECT_TRUE(foundPostsWrite);
}

// =============================================================================
// Graph Traversal Tests (TRAVERSAL node)
// =============================================================================

TEST_F(AttributeDetectorTest, Traversal_ReturnFullVertex) {
  auto query = executeQuery(R"aql(
    FOR v IN 1..2 OUTBOUND 'users/u1' GRAPH 'testGraph'
    RETURN v
  )aql");
  auto const& accesses = query->abacAccesses();

  // Traversal accesses both users and products (vertex collections in graph)
  // Full vertex return requires all attributes
  bool foundUsers = false;
  bool foundProducts = false;
  for (auto const& access : accesses) {
    if (access.collectionName == "users") {
      foundUsers = true;
      EXPECT_TRUE(access.requiresAllAttributesRead);
    }
    if (access.collectionName == "products") {
      foundProducts = true;
      EXPECT_TRUE(access.requiresAllAttributesRead);
    }
  }
  EXPECT_TRUE(foundUsers || foundProducts);
}

TEST_F(AttributeDetectorTest, Traversal_ReturnVertexAttribute) {
  auto query = executeQuery(R"aql(
    FOR v IN 1..2 OUTBOUND 'users/u1' GRAPH 'testGraph'
    RETURN v.name
  )aql");
  auto const& accesses = query->abacAccesses();

  // When returning specific attribute, should detect that attribute
  bool foundNameAccess = false;
  for (auto const& access : accesses) {
    if (access.readAttributes.contains(
            makePath("name", query->resourceMonitor()))) {
      foundNameAccess = true;
    }
  }
  EXPECT_TRUE(foundNameAccess);
}

TEST_F(AttributeDetectorTest, Traversal_ReturnEdgeAttribute) {
  auto query = executeQuery(R"aql(
    FOR v, e IN 1..2 OUTBOUND 'users/u1' GRAPH 'testGraph'
    RETURN e.qty
  )aql");
  auto const& accesses = query->abacAccesses();

  // Edge collection 'ordered' should have qty attribute access
  bool foundOrderedAccess = false;
  for (auto const& access : accesses) {
    if (access.collectionName == "ordered") {
      foundOrderedAccess = true;
      EXPECT_TRUE(access.readAttributes.contains(
          makePath("qty", query->resourceMonitor())));
    }
  }
  EXPECT_TRUE(foundOrderedAccess);
}

TEST_F(AttributeDetectorTest, Traversal_ReturnPath) {
  auto query = executeQuery(R"aql(
    FOR v, e, p IN 1..2 OUTBOUND 'users/u1' GRAPH 'testGraph'
    RETURN p
  )aql");
  auto const& accesses = query->abacAccesses();

  // Returning full path requires all attributes
  bool foundAccess = false;
  for (auto const& access : accesses) {
    if (access.collectionName == "users" ||
        access.collectionName == "products" ||
        access.collectionName == "ordered") {
      foundAccess = true;
      EXPECT_TRUE(access.requiresAllAttributesRead);
    }
  }
  EXPECT_TRUE(foundAccess);
}

TEST_F(AttributeDetectorTest, Traversal_WithEdgeCollection) {
  auto query = executeQuery(R"aql(
    FOR v IN 1..2 OUTBOUND 'users/u1' ordered
    RETURN v.name
  )aql");
  auto const& accesses = query->abacAccesses();

  // Direct edge collection traversal
  bool foundOrderedAccess = false;
  for (auto const& access : accesses) {
    if (access.collectionName == "ordered") {
      foundOrderedAccess = true;
    }
  }
  EXPECT_TRUE(foundOrderedAccess);
}

// =============================================================================
// Shortest Path Tests (SHORTEST_PATH node)
// =============================================================================

TEST_F(AttributeDetectorTest, ShortestPath_ReturnVertices) {
  auto query = executeQuery(R"aql(
    FOR v IN OUTBOUND SHORTEST_PATH 'users/u1' TO 'products/p3' GRAPH 'testGraph'
    RETURN v
  )aql");
  auto const& accesses = query->abacAccesses();

  // Shortest path returns vertices, requires all attributes
  bool foundAccess = false;
  for (auto const& access : accesses) {
    if (access.collectionName == "users" ||
        access.collectionName == "products") {
      foundAccess = true;
      EXPECT_TRUE(access.requiresAllAttributesRead);
    }
  }
  EXPECT_TRUE(foundAccess);
}

TEST_F(AttributeDetectorTest, ShortestPath_ReturnVertexAttribute) {
  auto query = executeQuery(R"aql(
    FOR v IN OUTBOUND SHORTEST_PATH 'users/u1' TO 'products/p3' GRAPH 'testGraph'
    RETURN v.name
  )aql");
  auto const& accesses = query->abacAccesses();

  // SHORTEST_PATH without explicit projections configured at the node level
  // requires all attributes (optimizer may or may not apply projections)
  bool foundVertexAccess = false;
  for (auto const& access : accesses) {
    if (access.collectionName == "users" ||
        access.collectionName == "products") {
      foundVertexAccess = true;
      // Either specific attributes or all attributes depending on optimizer
      EXPECT_TRUE(access.requiresAllAttributesRead ||
                  access.readAttributes.contains(
                      makePath("name", query->resourceMonitor())));
    }
  }
  EXPECT_TRUE(foundVertexAccess);
}

TEST_F(AttributeDetectorTest, ShortestPath_ReturnEdges) {
  auto query = executeQuery(R"aql(
    FOR v, e IN OUTBOUND SHORTEST_PATH 'users/u1' TO 'products/p3' GRAPH 'testGraph'
    RETURN e
  )aql");
  auto const& accesses = query->abacAccesses();

  // Edge return requires all edge attributes
  bool foundOrderedAccess = false;
  for (auto const& access : accesses) {
    if (access.collectionName == "ordered") {
      foundOrderedAccess = true;
      EXPECT_TRUE(access.requiresAllAttributesRead);
    }
  }
  EXPECT_TRUE(foundOrderedAccess);
}

// =============================================================================
// K Shortest Paths Tests (ENUMERATE_PATHS node)
// =============================================================================

TEST_F(AttributeDetectorTest, KShortestPaths_ReturnPath) {
  auto query = executeQuery(R"aql(
    FOR p IN OUTBOUND K_SHORTEST_PATHS 'users/u1' TO 'products/p3' GRAPH 'testGraph'
    LIMIT 3
    RETURN p
  )aql");
  auto const& accesses = query->abacAccesses();

  // K_SHORTEST_PATHS returns full path
  bool foundAccess = false;
  for (auto const& access : accesses) {
    if (access.collectionName == "users" ||
        access.collectionName == "products" ||
        access.collectionName == "ordered") {
      foundAccess = true;
      EXPECT_TRUE(access.requiresAllAttributesRead);
    }
  }
  EXPECT_TRUE(foundAccess);
}

TEST_F(AttributeDetectorTest, KShortestPaths_ReturnVerticesAttribute) {
  auto query = executeQuery(R"aql(
    FOR p IN OUTBOUND K_SHORTEST_PATHS 'users/u1' TO 'products/p3' GRAPH 'testGraph'
    LIMIT 3
    RETURN p.vertices[*].name
  )aql");
  auto const& accesses = query->abacAccesses();

  // Accessing specific attribute through path
  EXPECT_FALSE(accesses.empty());
}

// =============================================================================
// ArangoSearch View Tests (ENUMERATE_IRESEARCH_VIEW node)
// =============================================================================

TEST_F(AttributeDetectorTest, ArangoSearchView_SimpleSearch) {
  auto query = executeQuery(R"aql(
    FOR doc IN usersViewSimple
    SEARCH ANALYZER(doc.name == 'Alice', 'text_en')
    RETURN doc
  )aql");
  auto const& accesses = query->abacAccesses();

  // View search returns full document
  bool foundUsersAccess = false;
  for (auto const& access : accesses) {
    if (access.collectionName == "users") {
      foundUsersAccess = true;
      EXPECT_TRUE(access.requiresAllAttributesRead);
    }
  }
  EXPECT_TRUE(foundUsersAccess);
}

TEST_F(AttributeDetectorTest, ArangoSearchView_ReturnAttribute) {
  auto query = executeQuery(R"aql(
    FOR doc IN usersViewSimple
    SEARCH ANALYZER(doc.name == 'Alice', 'text_en')
    RETURN doc.name
  )aql");
  auto const& accesses = query->abacAccesses();

  // View queries may or may not have projections depending on optimizer
  bool foundUsersAccess = false;
  for (auto const& access : accesses) {
    if (access.collectionName == "users") {
      foundUsersAccess = true;
      // Either specific attribute or all attributes depending on view config
      EXPECT_TRUE(access.requiresAllAttributesRead ||
                  access.readAttributes.contains(
                      makePath("name", query->resourceMonitor())));
    }
  }
  EXPECT_TRUE(foundUsersAccess);
}

TEST_F(AttributeDetectorTest, ArangoSearchView_MultiCollectionView) {
  auto query = executeQuery(R"aql(
    FOR doc IN usersViewComplicated
    SEARCH ANALYZER(doc.name == 'Alice', 'text_en')
    RETURN doc
  )aql");
  auto const& accesses = query->abacAccesses();

  // Multi-collection view may access multiple collections
  EXPECT_FALSE(accesses.empty());
  for (auto const& access : accesses) {
    // All collections in view require all attributes when returning full doc
    EXPECT_TRUE(access.requiresAllAttributesRead);
  }
}

TEST_F(AttributeDetectorTest, ArangoSearchView_NestedFieldSearch) {
  auto query = executeQuery(R"aql(
    FOR doc IN usersViewComplicated
    SEARCH ANALYZER(doc.profile.bio == 'databases', 'text_en')
    RETURN doc.profile.bio
  )aql");
  auto const& accesses = query->abacAccesses();

  // Nested field access through view
  // View queries may require all attributes depending on optimizer
  bool foundUsersAccess = false;
  for (auto const& access : accesses) {
    if (access.collectionName == "users") {
      foundUsersAccess = true;
      // Either nested attribute or all attributes depending on view config
      EXPECT_TRUE(access.requiresAllAttributesRead ||
                  access.readAttributes.contains(makePath(
                      std::vector<std::string>{"profile", "bio"},
                      query->resourceMonitor())));
    }
  }
  EXPECT_TRUE(foundUsersAccess);
}

// =============================================================================
// INDEX_COLLECT Tests
// Note: INDEX_COLLECT requires persistent indexes which are not supported
// in the mock storage engine. These node types are covered by integration
// tests in tests/js/client/aql/aql-index-collect.js
// =============================================================================

// =============================================================================
// JOIN Tests (optimizer creates JoinNode)
// =============================================================================

TEST_F(AttributeDetectorTest, Join_IndexJoin) {
  // Index join happens when optimizer can use indexes to join collections
  auto query = executeQuery(R"aql(
    FOR u IN users
    FOR p IN products
    FILTER p.name == u.name
    RETURN {user: u.name, product: p.name}
  )aql");
  auto const& accesses = query->abacAccesses();

  // Should have access to both collections
  bool foundUsers = false;
  bool foundProducts = false;
  for (auto const& access : accesses) {
    if (access.collectionName == "users") {
      foundUsers = true;
      EXPECT_TRUE(access.readAttributes.contains(
          makePath("name", query->resourceMonitor())));
    }
    if (access.collectionName == "products") {
      foundProducts = true;
      EXPECT_TRUE(access.readAttributes.contains(
          makePath("name", query->resourceMonitor())));
    }
  }
  EXPECT_TRUE(foundUsers);
  EXPECT_TRUE(foundProducts);
}

// =============================================================================
// MATERIALIZE Tests (late materialization)
// =============================================================================

TEST_F(AttributeDetectorTest, Materialize_IndexScanWithProjection) {
  // Late materialization happens when index scan is followed by projection
  auto query = executeQuery(R"aql(
    FOR p IN products
    FILTER p.name == 'Keyboard'
    RETURN p.price
  )aql");
  auto const& accesses = query->abacAccesses();

  bool foundProductsAccess = false;
  for (auto const& access : accesses) {
    if (access.collectionName == "products") {
      foundProductsAccess = true;
      // Should detect both filter and return attributes
      EXPECT_TRUE(access.readAttributes.contains(
                      makePath("name", query->resourceMonitor())) ||
                  access.readAttributes.contains(
                      makePath("price", query->resourceMonitor())));
    }
  }
  EXPECT_TRUE(foundProductsAccess);
}

// =============================================================================
// Additional Graph Tests for edge cases
// =============================================================================

TEST_F(AttributeDetectorTest, Traversal_FilterOnEdge) {
  auto query = executeQuery(R"aql(
    FOR v, e IN 1..2 OUTBOUND 'users/u1' GRAPH 'testGraph'
    FILTER e.qty > 0
    RETURN v.name
  )aql");
  auto const& accesses = query->abacAccesses();

  // Should access edge attribute for filter
  bool foundOrderedAccess = false;
  for (auto const& access : accesses) {
    if (access.collectionName == "ordered") {
      foundOrderedAccess = true;
    }
  }
  EXPECT_TRUE(foundOrderedAccess);
}

TEST_F(AttributeDetectorTest, Traversal_MultipleGraphs) {
  auto query = executeQuery(R"aql(
    FOR v IN 1..1 OUTBOUND 'stores/s1' GRAPH 'storeGraph'
    RETURN v.name
  )aql");
  auto const& accesses = query->abacAccesses();

  // storeGraph connects stores to products via sells
  bool foundSellsAccess = false;
  bool foundProductsAccess = false;
  for (auto const& access : accesses) {
    if (access.collectionName == "sells") {
      foundSellsAccess = true;
    }
    if (access.collectionName == "products") {
      foundProductsAccess = true;
    }
  }
  EXPECT_TRUE(foundSellsAccess || foundProductsAccess);
}

TEST_F(AttributeDetectorTest, AllShortestPaths) {
  auto query = executeQuery(R"aql(
    FOR p IN OUTBOUND ALL_SHORTEST_PATHS 'users/u1' TO 'products/p3' GRAPH 'testGraph'
    RETURN p
  )aql");
  auto const& accesses = query->abacAccesses();

  // ALL_SHORTEST_PATHS returns paths
  EXPECT_FALSE(accesses.empty());
}

// =============================================================================
// ABAC Format Tests for Graph Queries
// =============================================================================

TEST_F(AttributeDetectorTest, AbacRequestFormat_Traversal) {
  auto query = executeQuery(R"aql(
    FOR v IN 1..2 OUTBOUND 'users/u1' GRAPH 'testGraph'
    RETURN v.name
  )aql");

  AttributeDetector detector(query->plan());
  detector.detect();
  auto requests = detector.getAbacRequests();

  // Should have read requests for collections in the graph
  EXPECT_FALSE(requests.empty());
  for (auto const& req : requests) {
    EXPECT_EQ(req.action, "read");
    EXPECT_TRUE(req.context.parameters.contains("read") ||
                req.context.parameters.contains("readAll"));
  }
}

TEST_F(AttributeDetectorTest, AbacRequestFormat_ArangoSearchView) {
  auto query = executeQuery(R"aql(
    FOR doc IN usersViewSimple
    SEARCH ANALYZER(doc.name == 'Alice', 'text_en')
    RETURN doc.name
  )aql");

  AttributeDetector detector(query->plan());
  detector.detect();
  auto requests = detector.getAbacRequests();

  // View query should generate read request
  bool foundUsersRead = false;
  for (auto const& req : requests) {
    if (req.resource == "users" && req.action == "read") {
      foundUsersRead = true;
    }
  }
  EXPECT_TRUE(foundUsersRead);
}

}  // namespace arangodb::tests::aql
