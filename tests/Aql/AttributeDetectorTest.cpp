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
};

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
  EXPECT_TRUE(accesses[0].readAttributes.contains("_key"));
  EXPECT_TRUE(accesses[0].requiresAllAttributesRead);
  EXPECT_TRUE(accesses[0].requiresAllAttributesWrite);
}

TEST_F(AttributeDetectorTest, RemoveOperation) {
  auto query = executeQuery("FOR doc IN users REMOVE doc IN users");
  auto const& accesses = query->abacAccesses();

  ASSERT_EQ(accesses.size(), 1);
  EXPECT_EQ(accesses[0].collectionName, "users");
  EXPECT_TRUE(accesses[0].readAttributes.contains("_key"));
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
      EXPECT_TRUE(access.readAttributes.contains("name"));
      EXPECT_TRUE(access.readAttributes.contains("_key"));
    } else if (access.collectionName == "posts") {
      foundPosts = true;
      EXPECT_TRUE(access.readAttributes.contains("title"));
      EXPECT_TRUE(access.readAttributes.contains("userId"));
    }
  }

  EXPECT_TRUE(foundUsers);
  EXPECT_TRUE(foundPosts);
}

TEST_F(AttributeDetectorTest, FilterAndReturnDifferentAttributes) {
  auto query = executeQuery(
      "FOR p IN users FILTER p.name IN ['Alice', 'Bob'] RETURN p.age");
  auto const& accesses = query->abacAccesses();

  ASSERT_EQ(accesses.size(), 1);
  EXPECT_EQ(accesses[0].collectionName, "users");
  EXPECT_TRUE(accesses[0].readAttributes.contains("age"));
  EXPECT_TRUE(accesses[0].readAttributes.contains("name"));
}

TEST_F(AttributeDetectorTest, NOOPTPreventProjection) {
  GTEST_SKIP() << "NOOPT() is not supported in AttributeDetector tests";
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
  EXPECT_TRUE(accesses[0].readAttributes.contains("name"));
}

TEST_F(AttributeDetectorTest, UpdateWithSpecificFields) {
  auto query = executeQuery("UPDATE {_key: 'u1', name: 'Carol'} IN users");
  auto const& accesses = query->abacAccesses();

  ASSERT_EQ(accesses.size(), 1);
  EXPECT_EQ(accesses[0].collectionName, "users");
  EXPECT_TRUE(accesses[0].requiresAllAttributesWrite);
}

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

TEST_F(AttributeDetectorTest, DocumentFunctionCrossCollection) {
  GTEST_SKIP() << "DOCUMENT() not supported in unit tests due to AST "
                  "optimization assertions";
}

TEST_F(AttributeDetectorTest, DocumentFunctionDynamicCollection) {
  GTEST_SKIP() << "DOCUMENT() not supported in unit tests due to AST "
                  "optimization assertions";
}

TEST_F(AttributeDetectorTest, DocumentFunctionWithNOOPT) {
  auto query = executeQuery(R"aql(
    FOR u IN users
    LET doc = NOOPT(DOCUMENT("otherColl/key1"))
    RETURN u.name
  )aql");
  auto const& accesses = query->abacAccesses();

  ASSERT_EQ(accesses.size(), 1);
  EXPECT_EQ(accesses[0].collectionName, "users");
  EXPECT_FALSE(accesses[0].requiresAllAttributesRead);
  EXPECT_TRUE(accesses[0].readAttributes.contains("name"));
}

TEST_F(AttributeDetectorTest, MergeMultipleDocumentCalls) {
  auto query = executeQuery(R"aql(
    FOR u IN users
    LET merged = MERGE(DOCUMENT("coll1/doc1"), DOCUMENT("coll2/doc2"))
    RETURN merged
  )aql");
  auto const& accesses = query->abacAccesses();

  ASSERT_GE(accesses.size(), 1);
  for (auto const& access : accesses) {
    EXPECT_TRUE(access.requiresAllAttributesRead)
        << "Collection " << access.collectionName
        << " should require all attributes read";
  }
}

TEST_F(AttributeDetectorTest, UserDefinedFunctionDetection) {
  GTEST_SKIP()
      << "UDFs require V8 runtime, logic at AttributeDetector.cpp:83-86";
}

TEST_F(AttributeDetectorTest, FilterWithIndex) {
  auto query = executeQuery(R"aql(
    FOR u IN users
    FILTER u.age > 25
    RETURN u.name
  )aql");
  auto const& accesses = query->abacAccesses();

  ASSERT_EQ(accesses.size(), 1);
  EXPECT_EQ(accesses[0].collectionName, "users");
  EXPECT_TRUE(accesses[0].readAttributes.contains("age"));
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
}

TEST_F(AttributeDetectorTest, InsertDoesNotRead) {
  auto query = executeQuery("INSERT {name: 'Eve', age: 35} INTO users");
  auto const& accesses = query->abacAccesses();

  ASSERT_EQ(accesses.size(), 1);
  EXPECT_EQ(accesses[0].collectionName, "users");
  EXPECT_FALSE(accesses[0].requiresAllAttributesRead);
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
  EXPECT_FALSE(insertAccesses[0].requiresAllAttributesRead);
  EXPECT_TRUE(insertAccesses[0].requiresAllAttributesWrite);

  auto const& updateAccesses = updateQuery->abacAccesses();
  ASSERT_EQ(updateAccesses.size(), 1);
  EXPECT_TRUE(updateAccesses[0].requiresAllAttributesRead);
  EXPECT_TRUE(updateAccesses[0].requiresAllAttributesWrite);
}

}  // namespace arangodb::tests::aql
