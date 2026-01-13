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
    auto usersColl = vocbase.createCollection(VPackParser::fromJson("{\"name\": \"users\"}")->slice());
    ASSERT_NE(usersColl.get(), nullptr) << "Failed to create users";
    auto postsColl = vocbase.createCollection(VPackParser::fromJson("{\"name\": \"posts\"}")->slice());
    ASSERT_NE(postsColl.get(), nullptr) << "Failed to create posts";
    auto productsColl = vocbase.createCollection(VPackParser::fromJson("{\"name\": \"products\"}")->slice());
    ASSERT_NE(productsColl.get(), nullptr) << "Failed to create products";
    auto orderedColl = vocbase.createCollection(VPackParser::fromJson("{\"name\": \"ordered\"}")->slice());
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
  auto query = executeQuery("FOR doc IN users RETURN {name: doc.name, age: doc.age}");
  
  auto& accesses = query->abacAccesses();
  
  ASSERT_EQ(accesses.size(), 1);
  EXPECT_EQ(accesses[0].collectionName, "users");
  EXPECT_TRUE(accesses[0].readAttributes.contains("name"));
  EXPECT_TRUE(accesses[0].readAttributes.contains("age"));
  EXPECT_EQ(accesses[0].readAttributes.size(), 2);
  EXPECT_FALSE(accesses[0].requiresAllAttributesRead);
}

TEST_F(AttributeDetectorTest, FullDocumentAccess) {
  auto query = executeQuery("FOR doc IN users RETURN doc");
  
  auto& accesses = query->abacAccesses();
  
  ASSERT_EQ(accesses.size(), 1);
  EXPECT_EQ(accesses[0].collectionName, "users");
  EXPECT_TRUE(accesses[0].requiresAllAttributesRead);
}

TEST_F(AttributeDetectorTest, InsertOperation) {
  auto query = executeQuery("INSERT {name: 'Alice', age: 30} INTO users");
  
  auto& accesses = query->abacAccesses();
  
  ASSERT_EQ(accesses.size(), 1);
  EXPECT_EQ(accesses[0].collectionName, "users");
  EXPECT_TRUE(accesses[0].requiresAllAttributesWrite);
}

TEST_F(AttributeDetectorTest, UpdateOperation) {
  auto query = executeQuery("FOR doc IN users UPDATE doc WITH {age: 31} IN users");
  
  auto& accesses = query->abacAccesses();
  
  ASSERT_EQ(accesses.size(), 1);
  EXPECT_EQ(accesses[0].collectionName, "users");
  EXPECT_TRUE(accesses[0].requiresAllAttributesWrite);
}

TEST_F(AttributeDetectorTest, MultipleCollections) {
  auto query = executeQuery("FOR u IN users FOR p IN posts FILTER u._key == p.userId RETURN {user: u.name, post: p.title}");
  
  auto& accesses = query->abacAccesses();
  
  ASSERT_EQ(accesses.size(), 2);
  
  bool foundUsers = false;
  bool foundPosts = false;
  
  for (auto const& access : accesses) {
    if (access.collectionName == "users") {
      foundUsers = true;
      EXPECT_TRUE(access.readAttributes.contains("name"));
      //EXPECT_TRUE(access.readAttributes.contains("_key"));
    } else if (access.collectionName == "posts") {
      foundPosts = true;
      EXPECT_TRUE(access.readAttributes.contains("title"));
      EXPECT_TRUE(access.readAttributes.contains("userId"));
    }
  }
  
  EXPECT_TRUE(foundUsers);
  EXPECT_TRUE(foundPosts);
}

TEST_F(AttributeDetectorTest, VelocyPackSerialization) {
  auto query = executeQuery("FOR doc IN users RETURN doc.name");
  
  auto& accesses = query->abacAccesses();
  
  velocypack::Builder builder;
  velocypack::serialize(builder, accesses);
  
  velocypack::Slice slice = builder.slice();
  ASSERT_TRUE(slice.isArray());
  ASSERT_EQ(slice.length(), 1);
  
  velocypack::Slice first = slice[0];
  ASSERT_TRUE(first.isObject());
  EXPECT_EQ(first["collection"].copyString(), "users");
  
  velocypack::Slice read = first["read"];
  ASSERT_TRUE(read.isObject());
  EXPECT_FALSE(read["requiresAll"].getBool());
  
  velocypack::Slice attrs = read["attributes"];
  ASSERT_TRUE(attrs.isArray());
  ASSERT_EQ(attrs.length(), 1);
  EXPECT_EQ(attrs[0].copyString(), "name");
}

}  // namespace arangodb::tests::aql

