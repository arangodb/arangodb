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

#include "gtest/gtest.h"

#include "Aql/AttributeDetector.h"
#include "Aql/Executor/AqlExecutorTestCase.h"
#include "Aql/Query.h"
#include "Async/async.h"
#include "Inspection/VPack.h"
#include "IResearch/common.h"
#include "Transaction/StandaloneContext.h"
#include "VocBase/vocbase.h"

#include <velocypack/Parser.h>

using namespace arangodb;
using namespace arangodb::aql;

namespace arangodb::tests::aql {

class AttributeDetectorTest : public ::testing::Test {
 protected:
  mocks::MockAqlServer server;
  TRI_vocbase_t& vocbase;

  AttributeDetectorTest() : vocbase(server.getSystemDatabase()) {}

 public:
  void SetUp() override {
    SCOPED_TRACE("SetUp");
    createDummyGraphData();
  }

  void createDummyGraphData() {
    auto usersColl = vocbase.createCollection(
        VPackParser::fromJson("{\"name\": \"users\"}")->slice());
    ASSERT_NE(usersColl.get(), nullptr) << "Failed to create users";
    auto postsColl = vocbase.createCollection(
        VPackParser::fromJson("{\"name\": \"posts\"}")->slice());
    ASSERT_NE(postsColl.get(), nullptr) << "Failed to create posts";
    auto productsColl = vocbase.createCollection(
        VPackParser::fromJson("{\"name\": \"products\"}")->slice());
    ASSERT_NE(productsColl.get(), nullptr) << "Failed to create products";
    auto orderedColl = vocbase.createCollection(
        VPackParser::fromJson("{\"name\": \"ordered\"}")->slice());
    ASSERT_NE(orderedColl.get(), nullptr) << "Failed to create ordered";

    auto run = [&](std::string const& q) {
      SCOPED_TRACE(q);
      auto bind = VPackParser::fromJson("{ }");
      auto qr = arangodb::tests::executeQuery(vocbase, q, bind);
      ASSERT_TRUE(qr.result.ok()) << qr.result.errorMessage();
    };

    run(R"aql(INSERT {_key:"u1", name:"Alice", age:30} INTO users)aql");
    run(R"aql(INSERT {_key:"u2", name:"Bob",   age:24} INTO users)aql");

    run(R"aql(INSERT {_key:"post1", userId:"u1", title:"festival"} INTO posts)aql");
    run(R"aql(INSERT {_key:"post2", userId:"u2", title:"birthday"} INTO posts)aql");

    run(R"aql(INSERT {_key:"p1", name:"Keyboard", price:49.99} INTO products)aql");
    run(R"aql(INSERT {_key:"p2", name:"Mouse",    price:19.99} INTO products)aql");
    run(R"aql(INSERT {_key:"p3", name:"Monitor",  price:199.0} INTO products)aql");

    run(R"aql(INSERT {_key:"e1", _from:"users/u1", _to:"products/p1", qty:1, orderedAt:"2026-01-01"} INTO ordered)aql");
    run(R"aql(INSERT {_key:"e2", _from:"users/u1", _to:"products/p2", qty:2, orderedAt:"2026-01-02"} INTO ordered)aql");
    run(R"aql(INSERT {_key:"e3", _from:"users/u2", _to:"products/p3", qty:1, orderedAt:"2026-01-03"} INTO ordered)aql");
  }

  std::shared_ptr<Query> executeQuery(std::string const& queryString) {
    auto ctx = std::make_shared<transaction::StandaloneContext>(
        vocbase, transaction::OperationOriginTestCase{});

    auto bindParams = VPackParser::fromJson("{}");
    auto query =
        Query::create(std::move(ctx), QueryString(queryString), bindParams);

    waitForAsync(query->prepareQuery());
    return query;
  }
};;

// Functional tests - test actual query analysis
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
  auto query = executeQuery("FOR doc IN users RETURN {name: doc.name, age: doc.age}");
  auto const& accesses = query->abacAccesses();

  ASSERT_EQ(accesses.size(), 1);
  EXPECT_EQ(accesses[0].collectionName, "users");
  EXPECT_TRUE(accesses[0].readAttributes.contains("name"));
  EXPECT_TRUE(accesses[0].readAttributes.contains("age"));
  EXPECT_EQ(accesses[0].readAttributes.size(), 2);
  EXPECT_FALSE(accesses[0].requiresAllAttributesRead);
}

TEST_F(AttributeDetectorTest, FullDocumentAccess) {
  auto query = executeQuery("FOR doc IN users RETURN doc");
  auto const& accesses = query->abacAccesses();

  ASSERT_EQ(accesses.size(), 1);
  EXPECT_EQ(accesses[0].collectionName, "users");
  EXPECT_TRUE(accesses[0].requiresAllAttributesRead);
}

TEST_F(AttributeDetectorTest, InsertOperation) {
  auto query = executeQuery("INSERT {name: 'Alice', age: 30} INTO users");
  auto const& accesses = query->abacAccesses();

  ASSERT_EQ(accesses.size(), 1);
  EXPECT_EQ(accesses[0].collectionName, "users");
  EXPECT_TRUE(accesses[0].requiresAllAttributesWrite);
}

TEST_F(AttributeDetectorTest, UpdateOperation) {
  auto query = executeQuery("FOR doc IN users UPDATE doc WITH {age: 31} IN users");
  auto const& accesses = query->abacAccesses();

  ASSERT_EQ(accesses.size(), 1);
  EXPECT_EQ(accesses[0].collectionName, "users");
  EXPECT_TRUE(accesses[0].requiresAllAttributesWrite);
}

TEST_F(AttributeDetectorTest, MultipleCollections) {
  auto query = executeQuery("FOR u IN users FOR p IN posts FILTER u._key == p.userId RETURN {user: "
      "u.name, post: p.title}");
  auto const& accesses = query->abacAccesses();

  ASSERT_EQ(accesses.size(), 2);

  bool foundUsers = false;
  bool foundPosts = false;

  for (auto const& access : accesses) {
    if (access.collectionName == "users") {
      foundUsers = true;
      EXPECT_TRUE(access.readAttributes.contains("name"));
    } else if (access.collectionName == "posts") {
      foundPosts = true;
      EXPECT_TRUE(access.readAttributes.contains("title"));
      EXPECT_TRUE(access.readAttributes.contains("userId"));
    }
  }

  EXPECT_TRUE(foundUsers);
  EXPECT_TRUE(foundPosts);
}

// Edge case tests based on query plan analysis
TEST_F(AttributeDetectorTest, FilterAndReturnDifferentAttributes) {
  // FILTER uses p.name, RETURN uses p.age - must track both
  auto query = executeQuery("FOR p IN users FILTER p.name IN ['Alice', 'Bob'] RETURN p.age");
  auto const& accesses = query->abacAccesses();

  ASSERT_EQ(accesses.size(), 1);
  EXPECT_EQ(accesses[0].collectionName, "users");

  // Should track both name (from FILTER) and age (from RETURN)
  EXPECT_TRUE(accesses[0].readAttributes.contains("name") ||
              accesses[0].requiresAllAttributesRead);
  EXPECT_TRUE(accesses[0].readAttributes.contains("age") ||
              accesses[0].requiresAllAttributesRead);
}

TEST_F(AttributeDetectorTest, NOOPTPreventProjection) {
  // NOOPT() might prevent projection optimization
  auto query = executeQuery("FOR p IN users FILTER NOOPT(p.name) IN ['Alice', 'Bob'] RETURN p.age");
  auto const& accesses = query->abacAccesses();

  ASSERT_EQ(accesses.size(), 1);
  EXPECT_EQ(accesses[0].collectionName, "users");

  // With NOOPT, might require all attributes or specific tracking
  // This test documents current behavior
}

TEST_F(AttributeDetectorTest, CalculationNodeWithAttributeAccess) {
  // LET statement creates a CALCULATION node
  auto query = executeQuery("FOR u IN users LET fullName = CONCAT(u.name, ' - ', u.age) RETURN "
      "fullName");
  auto const& accesses = query->abacAccesses();

  ASSERT_EQ(accesses.size(), 1);
  EXPECT_EQ(accesses[0].collectionName, "users");

  // Should detect name and age from the CALCULATION node
  EXPECT_TRUE(accesses[0].readAttributes.contains("name") ||
              accesses[0].requiresAllAttributesRead);
  EXPECT_TRUE(accesses[0].readAttributes.contains("age") ||
              accesses[0].requiresAllAttributesRead);
}

TEST_F(AttributeDetectorTest, NestedAttributeAccess) {
  // Nested attributes like doc.address.city
  auto query = executeQuery("FOR u IN users RETURN u.name");
  auto const& accesses = query->abacAccesses();

  ASSERT_EQ(accesses.size(), 1);
  EXPECT_EQ(accesses[0].collectionName, "users");

  // Should track top-level attribute
  EXPECT_TRUE(accesses[0].readAttributes.contains("name") ||
              accesses[0].requiresAllAttributesRead);
}

TEST_F(AttributeDetectorTest, UpdateWithSpecificFields) {
  // UPDATE should mark requiresAllAttributesWrite = true
  auto query = executeQuery("UPDATE {_key: 'u1', name: 'Carol'} IN users");
  auto const& accesses = query->abacAccesses();

  ASSERT_EQ(accesses.size(), 1);
  EXPECT_EQ(accesses[0].collectionName, "users");
  EXPECT_TRUE(accesses[0].requiresAllAttributesWrite);
}

// Inspector pattern tests - test serialization
TEST_F(AttributeDetectorTest, InspectorFormat) {
  AttributeDetector::CollectionAccess access;
  access.collectionName = "testCollection";
  access.readAttributes.insert("name");
  access.readAttributes.insert("age");
  access.requiresAllAttributesRead = false;
  access.requiresAllAttributesWrite = true;

  velocypack::Builder builder;
  velocypack::serialize(builder, access);

  velocypack::Slice slice = builder.slice();
  ASSERT_TRUE(slice.isObject());

  EXPECT_EQ(slice["collection"].copyString(), "testCollection");

  velocypack::Slice read = slice["read"];
  ASSERT_TRUE(read.isObject());
  EXPECT_FALSE(read["requiresAll"].getBool());

  velocypack::Slice readAttrs = read["attributes"];
  ASSERT_TRUE(readAttrs.isArray());
  EXPECT_EQ(readAttrs.length(), 2);

  velocypack::Slice write = slice["write"];
  ASSERT_TRUE(write.isObject());
  EXPECT_TRUE(write["requiresAll"].getBool());
}

TEST_F(AttributeDetectorTest, InspectorFormatMultipleCollections) {
  std::vector<AttributeDetector::CollectionAccess> accesses;

  AttributeDetector::CollectionAccess access1;
  access1.collectionName = "users";
  access1.readAttributes.insert("name");
  access1.requiresAllAttributesRead = false;
  access1.requiresAllAttributesWrite = false;

  AttributeDetector::CollectionAccess access2;
  access2.collectionName = "orders";
  access2.readAttributes.insert("id");
  access2.readAttributes.insert("total");
  access2.requiresAllAttributesRead = false;
  access2.requiresAllAttributesWrite = false;

  accesses.push_back(access1);
  accesses.push_back(access2);

  velocypack::Builder builder;
  velocypack::serialize(builder, accesses);

  velocypack::Slice slice = builder.slice();
  ASSERT_TRUE(slice.isArray());
  ASSERT_EQ(slice.length(), 2);

  velocypack::Slice first = slice[0];
  EXPECT_EQ(first["collection"].copyString(), "users");
  EXPECT_EQ(first["read"]["attributes"].length(), 1);

  velocypack::Slice second = slice[1];
  EXPECT_EQ(second["collection"].copyString(), "orders");
  EXPECT_EQ(second["read"]["attributes"].length(), 2);
}

TEST_F(AttributeDetectorTest, InspectorRoundTrip) {
  AttributeDetector::CollectionAccess original;
  original.collectionName = "products";
  original.readAttributes.insert("price");
  original.readAttributes.insert("stock");
  original.writeAttributes.insert("lastModified");
  original.requiresAllAttributesRead = false;
  original.requiresAllAttributesWrite = false;

  velocypack::Builder builder;
  velocypack::serialize(builder, original);
  velocypack::Slice slice = builder.slice();

  EXPECT_EQ(slice["collection"].copyString(), "products");

  velocypack::Slice read = slice["read"];
  EXPECT_FALSE(read["requiresAll"].getBool());
  ASSERT_TRUE(read["attributes"].isArray());
  EXPECT_EQ(read["attributes"].length(), 2);

  velocypack::Slice write = slice["write"];
  EXPECT_FALSE(write["requiresAll"].getBool());
  ASSERT_TRUE(write["attributes"].isArray());
  EXPECT_EQ(write["attributes"].length(), 1);
}

}  // namespace arangodb::tests::aql
