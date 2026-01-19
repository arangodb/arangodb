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

#pragma once

#include "gtest/gtest.h"

#include "Aql/Executor/AqlExecutorTestCase.h"
#include "Aql/Query.h"
#include "Async/async.h"
#include "Basics/Result.h"
#include "IResearch/common.h"
#include "IResearch/IResearchView.h"
#include "Mocks/StorageEngineMock.h"
#include "RestServer/FlushFeature.h"
#include "Transaction/StandaloneContext.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/vocbase.h"
#include "VocBase/Methods/Indexes.h"

#include <velocypack/Parser.h>

using namespace arangodb;
using namespace arangodb::aql;

namespace arangodb::tests::aql {

class AttributeDetectorTest : public ::testing::Test {
 protected:
  mocks::MockAqlServer server{false};
  TRI_vocbase_t* vocbase{nullptr};

  AttributeDetectorTest() {
    init();
    server.addFeature<FlushFeature>(false);
    server.startFeatures();
    vocbase = &server.getSystemDatabase();
  }

 public:
  void SetUp() override {
    SCOPED_TRACE("SetUp");
    createDummyGraphData();
  }

  void createDummyGraphData() {
    auto mkColl = [&](std::string const& name,
                      std::string const& extra =
                          "") -> std::shared_ptr<arangodb::LogicalCollection> {
      auto json =
          VPackParser::fromJson(std::string("{\"name\":\"") + name + "\"" +
                                (extra.empty() ? "" : (", " + extra)) + "}");
      auto c = vocbase->createCollection(json->slice());
      EXPECT_NE(c.get(), nullptr) << "Failed to create " << name;
      return c;
    };

    auto run = [&](std::string const& q) {
      SCOPED_TRACE(q);
      auto qr = tests::executeQuery(*vocbase, q, VPackParser::fromJson("{ }"));
      ASSERT_TRUE(qr.result.ok()) << qr.result.errorMessage();
    };

    // Collections
    auto usersColl = mkColl("users");
    auto postsColl = mkColl("posts");
    auto productsColl = mkColl("products");
    auto graphsColl = mkColl("_graphs", "\"isSystem\":true");
    auto orderedColl = mkColl("ordered", "\"type\":3");  // edge collection

    // Edge index required for traversal
    {
      bool created = false;
      auto idx =
          orderedColl
              ->createIndex(
                  VPackParser::fromJson(R"({"type":"edge"})")->slice(), created)
              .waitAndGet();
      ASSERT_TRUE(idx);
      ASSERT_TRUE(created);
    }

    // Data
    run(R"aql(INSERT {_key:"u1", name:"Alice", age:30, address:"Cologne",
                     profile:{ bio:"likes databases", tags:["aql","graph"] } } INTO users)aql");
    run(R"aql(INSERT {_key:"u2", name:"Bob",   age:24, address:"Tokyo",
                     profile:{ bio:"search fan", tags:["arangosearch","text"] } } INTO users)aql");
    run(R"aql(INSERT {_key:"u3", name:"Carol", age:42, address:"SF",
                     profile:{ bio:"security", tags:["abac","rbac"] } } INTO users)aql");

    run(R"aql(INSERT {_key:"post1", userId:"u1", title:"festival",
                     body:"ArangoDB graph traversal", meta:{lang:"en", likes:3} } INTO posts)aql");
    run(R"aql(INSERT {_key:"post2", userId:"u2", title:"birthday",
                     body:"ArangoSearch analyzers", meta:{lang:"en", likes:7} } INTO posts)aql");

    run(R"aql(INSERT {_key:"p1", name:"Keyboard", price:49.99,
                     category:"input", specs:{ brand:"ACME", color:"black" } } INTO products)aql");
    run(R"aql(INSERT {_key:"p2", name:"Mouse",    price:19.99,
                     category:"input", specs:{ brand:"ACME", color:"white" } } INTO products)aql");
    run(R"aql(INSERT {_key:"p3", name:"Monitor",  price:199.0,
                     category:"display", specs:{ brand:"ViewCo", color:"black" } } INTO products)aql");
    run(R"aql(INSERT {_key:"p4", name:"USB-C Hub", price:39.0,
                 category:"accessory", specs:{ brand:"ACME", color:"gray" } } INTO products)aql");

    ensureHashIndex("products", "name");
    ensureHashIndex("products", "price");

    run(R"aql(INSERT {_key:"e1", _from:"users/u1", _to:"products/p1", qty:1, orderedAt:"2026-01-01"} INTO ordered)aql");
    run(R"aql(INSERT {_key:"e2", _from:"users/u1", _to:"products/p2", qty:2, orderedAt:"2026-01-02"} INTO ordered)aql");
    run(R"aql(INSERT {_key:"e3", _from:"users/u2", _to:"products/p3", qty:1, orderedAt:"2026-01-03"} INTO ordered)aql");
    run(R"aql(INSERT {_key:"e4", _from:"products/p1", _to:"products/p3", qty:1, orderedAt:"2026-01-04"} INTO ordered)aql");
    run(R"aql(INSERT {_key:"e5", _from:"users/u1", _to:"products/p3", qty:1, orderedAt:"2026-01-05"} INTO ordered)aql");
    run(R"aql(INSERT {_key:"e6", _from:"users/u1", _to:"products/p4", qty:1, orderedAt:"2026-01-06"} INTO ordered)aql");
    run(R"aql(INSERT {_key:"e7", _from:"products/p4", _to:"products/p3", qty:1, orderedAt:"2026-01-07"} INTO ordered)aql");

    run(R"aql(
      INSERT {
        _key: "testGraph",
        name: "testGraph",
        edgeDefinitions: [
          {
            collection: "ordered",
            from: ["users", "products"],
            to: ["users", "products"]
          }
        ]
      } INTO _graphs
    )aql");

    auto viewSimple = createArangoSearchView("usersViewSimple");
    auto viewComplicated = createArangoSearchView("usersViewComplicated");
    linkUsersViewSimple(*viewSimple);
    linkUsersViewComplicated(*viewComplicated);
  }

  std::shared_ptr<arangodb::LogicalView> createArangoSearchView(
      std::string const& name) {
    auto createJson = VPackParser::fromJson(std::string("{\"name\":\"") + name +
                                            "\",\"type\":\"arangosearch\"}");
    auto v = vocbase->createView(createJson->slice(), false);
    EXPECT_FALSE(!v) << "Failed to create view " << name;
    return v;
  }

  void linkUsersViewSimple(LogicalView& logicalView) {
    auto* impl = dynamic_cast<iresearch::IResearchView*>(&logicalView);
    ASSERT_FALSE(!impl);

    auto linkJson = VPackParser::fromJson(R"({
    "links": {
      "users": {
        "fields": {
          "name": { "analyzers": ["text_en"] }
        }
      }
    }
  })");

    auto res = impl->properties(linkJson->slice(), /*partialUpdate*/ true,
                                /*doSync*/ false);
    ASSERT_TRUE(res.ok()) << res.errorMessage();
  }

  void linkUsersViewComplicated(LogicalView& logicalView) {
    auto* impl = dynamic_cast<iresearch::IResearchView*>(&logicalView);
    ASSERT_FALSE(!impl);

    auto updateJson = VPackParser::fromJson(R"({
      "links": {
        "users": {
          "fields": {
            "name":    { "analyzers": ["text_en"] },
            "address": { "analyzers": ["text_en"] },
            "profile": {
              "fields": {
                "bio":  { "analyzers": ["text_en"] },
                "tags": { "analyzers": ["identity"] }
              }
            }
          },
          "storedValues": [
            { "fields": ["name", "age"] }
          ]
        },
        "posts": {
          "fields": {
            "title": { "analyzers": ["text_en"] },
            "body":  { "analyzers": ["text_en"] },
            "meta":  { "fields": { "lang": {}, "likes": {} } }
          }
        },
        "products": {
          "includeAllFields": true
        },
        "ordered": {
          "includeAllFields": true
        }
      }
    })");

    auto res = impl->properties(updateJson->slice(), true, false);
    ASSERT_TRUE(res.ok()) << res.errorMessage();
  }

  std::shared_ptr<Query> executeQuery(std::string const& queryString) {
    auto ctx = std::make_shared<transaction::StandaloneContext>(
        *vocbase, transaction::OperationOriginTestCase{});
    auto query = Query::create(std::move(ctx), QueryString(queryString),
                               VPackParser::fromJson("{}"));
    waitForAsync(query->prepareQuery());
    return query;
  }

  void ensureHashIndex(std::string const& collName, std::string const& attr) {
    auto coll = vocbase->lookupCollection(collName);
    ASSERT_NE(coll, nullptr) << "Missing collection " << collName;

    auto json = VPackParser::fromJson(
        std::string("{\"type\":\"hash\",\"fields\":[\"") + attr + "\"]}");
    bool created = false;
    auto idx = coll->createIndex(json->slice(), created).waitAndGet();
    ASSERT_TRUE(idx) << "createIndex returned nullptr";
    ASSERT_TRUE(created) << "index was not created";
  }

  void ensurePersistentIndex(std::string const& collName,
                             std::vector<std::string> const& fields) {
    auto coll = vocbase->lookupCollection(collName);
    ASSERT_NE(coll, nullptr) << "Missing collection " << collName;

    VPackBuilder builder;
    builder.openObject();
    builder.add("type", VPackValue("persistent"));
    builder.add(VPackValue("fields"));
    builder.openArray();
    for (auto const& field : fields) {
      builder.add(VPackValue(field));
    }
    builder.close();
    builder.close();

    bool created = false;
    auto idx = coll->createIndex(builder.slice(), created).waitAndGet();
    ASSERT_TRUE(idx) << "createIndex returned nullptr";
    ASSERT_TRUE(created) << "index was not created";
  }
};

}  // namespace arangodb::tests::aql
