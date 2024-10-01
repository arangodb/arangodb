////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include <absl/strings/str_replace.h>

#include <regex>

#include "Aql/OptimizerRule.h"
#include "IResearchQueryCommon.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/LogicalView.h"

namespace arangodb::tests {
namespace {

class QueryJoin : public QueryTest {
 protected:
  void createCollections1() {
    {
      auto json = VPackParser::fromJson(R"({ "name": "entities" })");
      auto collection = _vocbase.createCollection(json->slice());
      ASSERT_TRUE(collection);
    }
    {
      auto json = VPackParser::fromJson(R"({ "name": "links", "type": 3 })");
      auto collection = _vocbase.createCollection(json->slice());
      ASSERT_TRUE(collection);
    }
  }

  void createCollections23() {
    {
      auto json = VPackParser::fromJson(R"({ "name": "testCollection0" })");
      auto collection = _vocbase.createCollection(json->slice());
      ASSERT_TRUE(collection);
    }
    {
      auto json = VPackParser::fromJson(R"({ "name": "testCollection1" })");
      auto collection = _vocbase.createCollection(json->slice());
      ASSERT_TRUE(collection);
    }
    {
      auto json = VPackParser::fromJson(R"({ "name": "testCollection2" })");
      auto collection = _vocbase.createCollection(json->slice());
      ASSERT_TRUE(collection);
    }
  }

  void queryTests1() {
    auto entities = _vocbase.lookupCollection("entities");
    ASSERT_TRUE(entities);
    auto links = _vocbase.lookupCollection("links");
    ASSERT_TRUE(links);

    auto entities_view = _vocbase.lookupView("entities_view");
    ASSERT_TRUE(entities_view);
    auto links_view = _vocbase.lookupView("links_view");
    ASSERT_TRUE(links_view);

    std::vector<std::string> const collections{"entities", "links"};

    // populate views with the data
    {
      OperationOptions opt;

      transaction::Methods trx(
          transaction::StandaloneContext::create(
              _vocbase, transaction::OperationOriginTestCase{}),
          collections, collections, collections, transaction::Options());
      EXPECT_TRUE(trx.begin().ok());

      // insert into entities collection
      {
        auto builder = VPackParser::fromJson(
            "[{ \"_key\": \"person1\", \"_id\": \"entities/person1\", "
            "\"_rev\": "
            "\"_YOr40eu--_\", \"type\": \"person\", \"id\": \"person1\" },"
            " { \"_key\": \"person5\", \"_id\": \"entities/person5\", "
            "\"_rev\": "
            "\"_YOr48rO---\", \"type\": \"person\", \"id\": \"person5\" },"
            " { \"_key\": \"person4\", \"_id\": \"entities/person4\", "
            "\"_rev\": "
            "\"_YOr5IGu--_\", \"type\": \"person\", \"id\": \"person4\" },"
            " { \"_key\": \"person3\", \"_id\": \"entities/person3\", "
            "\"_rev\": "
            "\"_YOr5PBK--_\", \"type\": \"person\", \"id\": \"person3\" }, "
            " { \"_key\": \"person2\", \"_id\": \"entities/person2\", "
            "\"_rev\": "
            "\"_YOr5Umq--_\", \"type\": \"person\", \"id\": \"person2\" } ]");

        auto root = builder->slice();
        ASSERT_TRUE(root.isArray());

        for (auto doc : velocypack::ArrayIterator(root)) {
          auto const res = trx.insert(entities->name(), doc, opt);
          EXPECT_TRUE(res.ok());
        }
      }

      // insert into links collection
      {
        auto builder = VPackParser::fromJson(
            "[ { \"_key\": \"3301\", \"_id\": \"links/3301\", \"_from\": "
            "\"entities/person1\", \"_to\": \"entities/person2\", \"_rev\": "
            "\"_YOrbp_S--_\", \"type\": \"relationship\", \"subType\": "
            "\"married\", \"from\": \"person1\", \"to\": \"person2\" },"
            "  { \"_key\": \"3377\", \"_id\": \"links/3377\", \"_from\": "
            "\"entities/person4\", \"_to\": \"entities/person5\", \"_rev\": "
            "\"_YOrbxN2--_\", \"type\": \"relationship\", \"subType\": "
            "\"married\", \"from\": \"person4\", \"to\": \"person5\" },"
            "  { \"_key\": \"3346\", \"_id\": \"links/3346\", \"_from\": "
            "\"entities/person1\", \"_to\": \"entities/person3\", \"_rev\": "
            "\"_YOrb4kq--_\", \"type\": \"relationship\", \"subType\": "
            "\"married\", \"from\": \"person1\", \"to\": \"person3\" }]");

        auto root = builder->slice();
        ASSERT_TRUE(root.isArray());

        for (auto doc : velocypack::ArrayIterator(root)) {
          auto const res = trx.insert(links->name(), doc, opt);
          EXPECT_TRUE(res.ok());
        }
      }

      EXPECT_TRUE(trx.commit().ok());
      EXPECT_TRUE(executeQuery(_vocbase,
                               "FOR d IN entities_view SEARCH 1 == 1 OPTIONS "
                               "{ waitForSync: true } RETURN d")
                      .result.ok());
      EXPECT_TRUE(executeQuery(_vocbase,
                               "FOR d IN links_view SEARCH 1 == 1 OPTIONS "
                               "{ waitForSync: true } RETURN d")
                      .result.ok());
    }

    // check query
    {
      auto expectedResultBuilder = VPackParser::fromJson(
          "[ { \"id\": \"person1\", \"marriedIds\": [\"person2\", \"person3\"] "
          "},"
          "  { \"id\": \"person2\", \"marriedIds\": [\"person1\" ] },"
          "  { \"id\": \"person3\", \"marriedIds\": [\"person1\" ] },"
          "  { \"id\": \"person4\", \"marriedIds\": [\"person5\" ] },"
          "  { \"id\": \"person5\", \"marriedIds\": [\"person4\" ] } ]");

      std::string const query =
          "FOR org IN entities_view SEARCH org.type == 'person' "
          "LET marriedIds = ("
          "LET entityIds = ("
          "  FOR l IN links_view SEARCH l.type == 'relationship' AND l.subType "
          "== 'married' AND (l.from == org.id OR l.to == org.id)"
          "  RETURN DISTINCT l.from == org.id ? l.to : l.from"
          ") "
          "FOR entityId IN entityIds SORT entityId RETURN entityId "
          ") "
          "LIMIT 10 "
          "SORT org._key "
          "RETURN { id: org._key, marriedIds: marriedIds }";

      auto queryResult = executeQuery(_vocbase, query);
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      auto expectedResult = expectedResultBuilder->slice();
      EXPECT_TRUE(expectedResult.isArray());

      velocypack::ArrayIterator expectedResultIt(expectedResult);
      velocypack::ArrayIterator resultIt(result);
      ASSERT_EQ(expectedResultIt.size(), resultIt.size());

      // Check documents
      for (; resultIt.valid(); resultIt.next(), expectedResultIt.next()) {
        ASSERT_TRUE(expectedResultIt.valid());
        auto const expectedDoc = expectedResultIt.value();
        auto const actualDoc = resultIt.value();
        auto const resolved = actualDoc.resolveExternals();

        EXPECT_EQUAL_SLICES(velocypack::Slice(expectedDoc), resolved);
      }
      EXPECT_FALSE(expectedResultIt.valid());
    }
  }

  void queryTests2() {
    static std::vector<std::string> const EMPTY;

    auto logicalCollection1 = _vocbase.lookupCollection("testCollection0");
    ASSERT_TRUE(logicalCollection1);
    auto logicalCollection2 = _vocbase.lookupCollection("testCollection1");
    ASSERT_TRUE(logicalCollection2);
    auto logicalCollection3 = _vocbase.lookupCollection("testCollection2");
    ASSERT_TRUE(logicalCollection3);
    // add view
    auto view = _vocbase.lookupView("testView");
    ASSERT_TRUE(view);

    // add logical collection with the same name as view
    {
      auto collectionJson = VPackParser::fromJson("{ \"name\": \"testView\" }");
      // TRI_vocbase_t::createCollection(...) throws exception instead of
      // returning a nullptr
      EXPECT_ANY_THROW(_vocbase.createCollection(collectionJson->slice()));
    }

    // populate view with the data
    {
      OperationOptions opt;

      transaction::Methods trx(
          transaction::StandaloneContext::create(
              _vocbase, transaction::OperationOriginTestCase{}),
          EMPTY,
          {logicalCollection1->name(), logicalCollection2->name(),
           logicalCollection3->name()},
          EMPTY, transaction::Options());
      EXPECT_TRUE(trx.begin().ok());

      // insert into collections
      {
        std::filesystem::path resource;
        resource /= std::string_view(testResourceDir);
        resource /= std::string_view("simple_sequential.json");

        auto builder =
            basics::VelocyPackHelper::velocyPackFromFile(resource.string());
        auto root = builder.slice();
        ASSERT_TRUE(root.isArray());

        size_t i = 0;

        std::shared_ptr<LogicalCollection> collections[]{logicalCollection1,
                                                         logicalCollection2};

        for (auto doc : velocypack::ArrayIterator(root)) {
          auto res = trx.insert(collections[i % 2]->name(), doc, opt);
          EXPECT_TRUE(res.ok());
          ++i;
        }
      }

      // insert into testCollection2
      {
        std::filesystem::path resource;
        resource /= std::string_view(testResourceDir);
        resource /= std::string_view("simple_sequential_order.json");

        auto builder =
            basics::VelocyPackHelper::velocyPackFromFile(resource.string());
        auto root = builder.slice();
        ASSERT_TRUE(root.isArray());

        for (auto doc : velocypack::ArrayIterator(root)) {
          auto res = trx.insert(logicalCollection3->name(), doc, opt);
          EXPECT_TRUE(res.ok());
        }
      }

      EXPECT_TRUE(trx.commit().ok());
      EXPECT_TRUE((executeQuery(_vocbase,
                                "FOR d IN testView SEARCH 1 == 1 OPTIONS "
                                "{ waitForSync: true } RETURN d")
                       .result.ok()));  // commit
    }

    // using search keyword for collection is prohibited
    {
      std::string const query =
          "LET c=5 FOR x IN testCollection0 SEARCH x.seq == c RETURN x";
      auto const boundParameters = VPackParser::fromJson("{ }");

      // aql::ExecutionPlan::fromNodeFor(...) throws
      // TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND
      auto queryResult = executeQuery(_vocbase, query, boundParameters);
      EXPECT_TRUE(
          queryResult.result.is(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND));
    }

    // using search keyword for bound collection is prohibited
    {
      std::string const query =
          "LET c=5 FOR x IN @@dataSource SEARCH x.seq == c  RETURN x";
      auto const boundParameters =
          VPackParser::fromJson("{ \"@dataSource\" : \"testCollection0\" }");
      auto queryResult = executeQuery(_vocbase, query, boundParameters);
      EXPECT_TRUE(
          queryResult.result.is(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND));
    }
  }

  void queryTests3() {
    static std::vector<std::string> const EMPTY;
    auto logicalCollection1 = _vocbase.lookupCollection("testCollection0");
    ASSERT_TRUE(logicalCollection1);
    auto logicalCollection2 = _vocbase.lookupCollection("testCollection1");
    ASSERT_TRUE(logicalCollection2);
    auto logicalCollection3 = _vocbase.lookupCollection("testCollection2");
    ASSERT_TRUE(logicalCollection3);
    // add view
    auto view = _vocbase.lookupView("testView");
    ASSERT_TRUE(view);

    std::deque<std::shared_ptr<velocypack::Buffer<uint8_t>>> insertedDocsView;
    std::deque<std::shared_ptr<velocypack::Buffer<uint8_t>>>
        insertedDocsCollection;

    // populate view with the data
    {
      OperationOptions opt;

      transaction::Methods trx(
          transaction::StandaloneContext::create(
              _vocbase, transaction::OperationOriginTestCase{}),
          EMPTY,
          {logicalCollection1->name(), logicalCollection2->name(),
           logicalCollection3->name()},
          EMPTY, transaction::Options());
      EXPECT_TRUE(trx.begin().ok());

      // insert into collections
      {
        std::filesystem::path resource;
        resource /= std::string_view(testResourceDir);
        resource /= std::string_view("simple_sequential.json");

        auto builder =
            basics::VelocyPackHelper::velocyPackFromFile(resource.string());
        auto root = builder.slice();
        ASSERT_TRUE(root.isArray());

        size_t i = 0;

        std::shared_ptr<LogicalCollection> collections[]{logicalCollection1,
                                                         logicalCollection2};

        for (auto doc : velocypack::ArrayIterator(root)) {
          auto res = trx.insert(collections[i % 2]->name(), doc, opt);
          EXPECT_TRUE(res.ok());

          res = trx.document(collections[i % 2]->name(), res.slice(), opt);
          EXPECT_TRUE(res.ok());
          insertedDocsView.emplace_back(std::move(res.buffer));
          ++i;
        }
      }

      // insert into testCollection2
      {
        std::filesystem::path resource;
        resource /= std::string_view(testResourceDir);
        resource /= std::string_view("simple_sequential_order.json");

        auto builder =
            basics::VelocyPackHelper::velocyPackFromFile(resource.string());
        auto root = builder.slice();
        ASSERT_TRUE(root.isArray());

        for (auto doc : velocypack::ArrayIterator(root)) {
          auto res = trx.insert(logicalCollection3->name(), doc, opt);
          EXPECT_TRUE(res.ok());

          res = trx.document(logicalCollection3->name(), res.slice(), opt);
          EXPECT_TRUE(res.ok());
          insertedDocsCollection.emplace_back(std::move(res.buffer));
        }
      }

      EXPECT_TRUE(trx.commit().ok());
      EXPECT_TRUE((executeQuery(_vocbase,
                                "FOR d IN testView SEARCH 1 ==1 OPTIONS "
                                "{ waitForSync: true } RETURN d")
                       .result.ok()));  // commit
    }

    // deterministic filter condition in a loop
    // (should not recreate view iterator each loop iteration, `reset` instead)
    //
    // LET c=5
    // FOR x IN 1..7
    //   FOR d IN testView
    //   SEARCH c == x.seq
    // RETURN d;
    {
      std::string const query =
          "LET c=5 FOR x IN 1..7 FOR d IN testView SEARCH c == d.seq RETURN d";

      EXPECT_TRUE(
          assertRules(_vocbase, query,
                      {
                          aql::OptimizerRule::handleArangoSearchViewsRule,
                      }));

      std::vector<velocypack::Slice> expectedDocs{
          velocypack::Slice(insertedDocsView[5]->data()),
          velocypack::Slice(insertedDocsView[5]->data()),
          velocypack::Slice(insertedDocsView[5]->data()),
          velocypack::Slice(insertedDocsView[5]->data()),
          velocypack::Slice(insertedDocsView[5]->data()),
          velocypack::Slice(insertedDocsView[5]->data()),
          velocypack::Slice(insertedDocsView[5]->data()),
      };

      auto queryResult = executeQuery(_vocbase, query);
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      velocypack::ArrayIterator resultIt(result);
      ASSERT_EQ(expectedDocs.size(), resultIt.size());

      // Check documents
      auto expectedDoc = expectedDocs.begin();
      for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
        auto const actualDoc = resultIt.value();
        auto const resolved = actualDoc.resolveExternals();

        EXPECT_EQUAL_SLICES(velocypack::Slice(*expectedDoc), resolved);
      }
      EXPECT_EQ(expectedDoc, expectedDocs.end());
    }

    // number of iterations bigger than internal batch size
    {
      std::string const query =
          "FOR x IN 1..10000 FOR d IN testView SEARCH 1 == d.seq RETURN d";

      EXPECT_TRUE(
          assertRules(_vocbase, query,
                      {
                          aql::OptimizerRule::handleArangoSearchViewsRule,
                      }));

      auto queryResult = executeQuery(_vocbase, query);
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      velocypack::ArrayIterator resultIt(result);
      ASSERT_EQ(10000U, resultIt.size());

      // Check documents
      for (; resultIt.valid(); resultIt.next()) {
        auto const actualDoc = resultIt.value();
        auto const resolved = actualDoc.resolveExternals();

        EXPECT_EQ(0, basics::VelocyPackHelper::compare(
                         velocypack::Slice(insertedDocsView[1]->data()),
                         resolved, true));
      }
    }

    // non deterministic filter condition in a loop
    // (must recreate view iterator each loop iteration)
    //
    // FOR x IN 1..7
    //   FOR d IN testView
    //   SEARCH _FORWARD_(5) == x.seq
    // RETURN d;
    {
      std::string const query =
          "FOR x IN 1..7 FOR d IN testView SEARCH _FORWARD_(5) == d.seq RETURN "
          "d";

      EXPECT_TRUE(
          assertRules(_vocbase, query,
                      {
                          aql::OptimizerRule::handleArangoSearchViewsRule,
                      }));

      std::vector<velocypack::Slice> expectedDocs{
          velocypack::Slice(insertedDocsView[5]->data()),
          velocypack::Slice(insertedDocsView[5]->data()),
          velocypack::Slice(insertedDocsView[5]->data()),
          velocypack::Slice(insertedDocsView[5]->data()),
          velocypack::Slice(insertedDocsView[5]->data()),
          velocypack::Slice(insertedDocsView[5]->data()),
          velocypack::Slice(insertedDocsView[5]->data()),
      };

      auto queryResult = executeQuery(_vocbase, query);
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      velocypack::ArrayIterator resultIt(result);
      ASSERT_EQ(expectedDocs.size(), resultIt.size());

      // Check documents
      auto expectedDoc = expectedDocs.begin();
      for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
        auto const actualDoc = resultIt.value();
        auto const resolved = actualDoc.resolveExternals();

        EXPECT_EQUAL_SLICES(velocypack::Slice(*expectedDoc), resolved);
      }
      EXPECT_EQ(expectedDoc, expectedDocs.end());
    }

    // non deterministic filter condition with self-reference in a loop
    // (must recreate view iterator each loop iteration)
    //
    // FOR x IN 1..7
    //   FOR d IN testView
    //   SEARCH _NONDETERM_(5) == x.seq
    // RETURN d;
    {
      std::string const query =
          "FOR x IN 1..7 FOR d IN testView SEARCH _NONDETERM_(5) == d.seq "
          "RETURN "
          "d";

      EXPECT_TRUE(
          assertRules(_vocbase, query,
                      {
                          aql::OptimizerRule::handleArangoSearchViewsRule,
                      }));

      std::vector<velocypack::Slice> expectedDocs{
          velocypack::Slice(insertedDocsView[5]->data()),
          velocypack::Slice(insertedDocsView[5]->data()),
          velocypack::Slice(insertedDocsView[5]->data()),
          velocypack::Slice(insertedDocsView[5]->data()),
          velocypack::Slice(insertedDocsView[5]->data()),
          velocypack::Slice(insertedDocsView[5]->data()),
          velocypack::Slice(insertedDocsView[5]->data()),
      };

      auto queryResult = executeQuery(_vocbase, query);
      ASSERT_TRUE(queryResult.result.is(
          TRI_ERROR_NOT_IMPLEMENTED));  // can't handle self-referenced variable
                                        // now

      //    auto result = queryResult.data->slice();
      //    EXPECT_TRUE(result.isArray());
      //
      //    velocypack::ArrayIterator resultIt(result);
      //    ASSERT_EQ(expectedDocs.size(), resultIt.size());
      //
      //    // Check documents
      //    auto expectedDoc = expectedDocs.begin();
      //    for (;resultIt.valid(); resultIt.next(), ++expectedDoc) {
      //      auto const actualDoc = resultIt.value();
      //      auto const resolved = actualDoc.resolveExternals();
      //
      //      EXPECT_EQ(0,
      //      basics::VelocyPackHelper::compare(velocypack::Slice(*expectedDoc),
      //      resolved, true));
      //    }
      //    EXPECT_EQ(expectedDoc, expectedDocs.end());
    }

    // nondeterministic filter condition in a loop
    // (must recreate view iterator each loop iteration)
    //
    // LET c=_NONDETERM_(4)
    // FOR x IN 1..7
    //   FOR d IN testView
    //   SEARCH c == x.seq
    // RETURN d;
    {
      std::string const query =
          "LET c=_NONDETERM_(4) FOR x IN 1..7 FOR d IN testView SEARCH c == "
          "d.seq RETURN d";

      EXPECT_TRUE(
          assertRules(_vocbase, query,
                      {
                          aql::OptimizerRule::handleArangoSearchViewsRule,
                      }));

      std::vector<velocypack::Slice> expectedDocs{
          velocypack::Slice(insertedDocsView[4]->data()),
          velocypack::Slice(insertedDocsView[4]->data()),
          velocypack::Slice(insertedDocsView[4]->data()),
          velocypack::Slice(insertedDocsView[4]->data()),
          velocypack::Slice(insertedDocsView[4]->data()),
          velocypack::Slice(insertedDocsView[4]->data()),
          velocypack::Slice(insertedDocsView[4]->data()),
      };

      auto queryResult = executeQuery(_vocbase, query);
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      velocypack::ArrayIterator resultIt(result);
      ASSERT_EQ(expectedDocs.size(), resultIt.size());

      // Check documents
      auto expectedDoc = expectedDocs.begin();
      for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
        auto const actualDoc = resultIt.value();
        auto const resolved = actualDoc.resolveExternals();

        EXPECT_EQUAL_SLICES(velocypack::Slice(*expectedDoc), resolved);
      }
      EXPECT_EQ(expectedDoc, expectedDocs.end());
    }

    // nondeterministic range
    // (must recreate view iterator each loop iteration)
    //
    // LET range=_NONDETERM_(0).._NONDETERM_(7)
    // FOR x IN range
    //   FOR d IN testView
    //   SEARCH d.seq == x.seq
    // RETURN d;
    {
      std::string const query =
          " FOR x IN _NONDETERM_(0).._NONDETERM_(7) FOR d IN testView SEARCH x "
          "== d.seq RETURN d";

      EXPECT_TRUE(
          assertRules(_vocbase, query,
                      {
                          aql::OptimizerRule::handleArangoSearchViewsRule,
                      }));

      std::vector<velocypack::Slice> expectedDocs{
          velocypack::Slice(insertedDocsView[0]->data()),
          velocypack::Slice(insertedDocsView[1]->data()),
          velocypack::Slice(insertedDocsView[2]->data()),
          velocypack::Slice(insertedDocsView[3]->data()),
          velocypack::Slice(insertedDocsView[4]->data()),
          velocypack::Slice(insertedDocsView[5]->data()),
          velocypack::Slice(insertedDocsView[6]->data()),
          velocypack::Slice(insertedDocsView[7]->data())};

      auto queryResult = executeQuery(_vocbase, query);
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      velocypack::ArrayIterator resultIt(result);
      ASSERT_EQ(expectedDocs.size(), resultIt.size());

      // Check documents
      auto expectedDoc = expectedDocs.begin();
      for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
        auto const actualDoc = resultIt.value();
        auto const resolved = actualDoc.resolveExternals();

        EXPECT_EQUAL_SLICES(velocypack::Slice(*expectedDoc), resolved);
      }
      EXPECT_EQ(expectedDoc, expectedDocs.end());
    }

    // FOR x IN testCollection2
    //   FOR d IN testView
    //   SEARCH d.seq == x.seq
    // RETURN d;
    {
      std::string const query =
          "FOR x IN testCollection2 SORT x._key FOR d IN testView SEARCH x.seq "
          "== "
          "d.seq RETURN d";

      EXPECT_TRUE(
          assertRules(_vocbase, query,
                      {
                          aql::OptimizerRule::handleArangoSearchViewsRule,
                      }));

      std::vector<velocypack::Slice> expectedDocs{
          velocypack::Slice(insertedDocsView[0]->data()),
          velocypack::Slice(insertedDocsView[1]->data()),
          velocypack::Slice(insertedDocsView[2]->data()),
          velocypack::Slice(insertedDocsView[3]->data()),
          velocypack::Slice(insertedDocsView[4]->data()),
          velocypack::Slice(insertedDocsView[5]->data()),
          velocypack::Slice(insertedDocsView[6]->data()),
          velocypack::Slice(insertedDocsView[7]->data())};

      auto queryResult = executeQuery(_vocbase, query);
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      velocypack::ArrayIterator resultIt(result);
      ASSERT_EQ(expectedDocs.size(), resultIt.size());

      // Check documents
      auto expectedDoc = expectedDocs.begin();
      for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
        auto const actualDoc = resultIt.value();
        auto const resolved = actualDoc.resolveExternals();
        EXPECT_TRUE(basics::VelocyPackHelper::equal(
            velocypack::Slice(*expectedDoc), resolved, true))
            << velocypack::Slice(*expectedDoc).toJson() << " vs. "
            << resolved.toJson();
      }
      EXPECT_EQ(expectedDoc, expectedDocs.end());
    }

    // FOR x IN testCollection2
    //   FOR d IN testView
    //   SEARCH d.seq == x.seq
    // SORT d.seq DESC
    // RETURN d;
    {
      std::string const query =
          "FOR x IN testCollection2 FOR d IN testView SEARCH x.seq == d.seq "
          "SORT "
          "d.seq DESC RETURN d";

      EXPECT_TRUE(
          assertRules(_vocbase, query,
                      {
                          aql::OptimizerRule::handleArangoSearchViewsRule,
                      }));

      std::vector<velocypack::Slice> expectedDocs{
          velocypack::Slice(insertedDocsView[7]->data()),
          velocypack::Slice(insertedDocsView[6]->data()),
          velocypack::Slice(insertedDocsView[5]->data()),
          velocypack::Slice(insertedDocsView[4]->data()),
          velocypack::Slice(insertedDocsView[3]->data()),
          velocypack::Slice(insertedDocsView[2]->data()),
          velocypack::Slice(insertedDocsView[1]->data()),
          velocypack::Slice(insertedDocsView[0]->data())};

      // check node estimation
      {
        auto explanationResult = explainQuery(_vocbase, query);
        ASSERT_TRUE(explanationResult.result.ok());
        auto const explanationSlice = explanationResult.data->slice();
        ASSERT_TRUE(explanationSlice.isObject());
        auto const nodesSlice = explanationSlice.get("nodes");
        ASSERT_TRUE(nodesSlice.isArray());
        VPackSlice viewNode;
        for (auto node : VPackArrayIterator(nodesSlice)) {
          if ("EnumerateViewNode" == node.get("type").toString() &&
              "testView" == node.get("view").toString()) {
            viewNode = node;
            break;
          }
        }

        ASSERT_TRUE(viewNode.isObject());
        ASSERT_EQ(insertedDocsView.size() * insertedDocsCollection.size() +
                      insertedDocsCollection.size() +
                      1.     // cost of collection node
                      + 1.,  // cost of singleton node
                  viewNode.get("estimatedCost").getDouble());
        ASSERT_EQ(insertedDocsView.size() * insertedDocsCollection.size(),
                  viewNode.get("estimatedNrItems").getNumber<size_t>());
      }

      auto queryResult = executeQuery(_vocbase, query);
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      velocypack::ArrayIterator resultIt(result);
      ASSERT_EQ(expectedDocs.size(), resultIt.size());

      // Check documents
      auto expectedDoc = expectedDocs.begin();
      for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
        auto const actualDoc = resultIt.value();
        auto const resolved = actualDoc.resolveExternals();

        EXPECT_EQUAL_SLICES(velocypack::Slice(*expectedDoc), resolved);
      }
      EXPECT_EQ(expectedDoc, expectedDocs.end());
    }

    // FOR x IN testCollection2
    //   FOR d IN testView
    //   SEARCH d.seq == x.seq
    // SORT d.seq DESC
    // LIMIT 3
    // RETURN d;
    {
      std::string const query =
          "FOR x IN testCollection2 FOR d IN testView SEARCH x.seq == d.seq "
          "SORT "
          "d.seq DESC LIMIT 3 RETURN d";

      EXPECT_TRUE(
          assertRules(_vocbase, query,
                      {
                          aql::OptimizerRule::handleArangoSearchViewsRule,
                      }));

      std::vector<velocypack::Slice> expectedDocs{
          velocypack::Slice(insertedDocsView[7]->data()),
          velocypack::Slice(insertedDocsView[6]->data()),
          velocypack::Slice(insertedDocsView[5]->data())};

      auto queryResult = executeQuery(_vocbase, query);
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      velocypack::ArrayIterator resultIt(result);
      ASSERT_EQ(expectedDocs.size(), resultIt.size());

      // Check documents
      auto expectedDoc = expectedDocs.begin();
      for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
        auto const actualDoc = resultIt.value();
        auto const resolved = actualDoc.resolveExternals();

        EXPECT_EQUAL_SLICES(velocypack::Slice(*expectedDoc), resolved);
      }
      EXPECT_EQ(expectedDoc, expectedDocs.end());
    }

    // FOR x IN testCollection2
    //   FOR d IN testView
    //   SEARCH d.seq == x.seq && (d.value > 5 && d.value <= 100)
    // RETURN d;
    {
      std::string const query =
          "FOR x IN testCollection2 FOR d IN testView SEARCH x.seq == d.seq && "
          "(d.value > 5 && d.value <= 100) SORT d.seq DESC RETURN d";

      EXPECT_TRUE(
          assertRules(_vocbase, query,
                      {
                          aql::OptimizerRule::handleArangoSearchViewsRule,
                      }));

      std::vector<velocypack::Slice> expectedDocs{
          velocypack::Slice(insertedDocsView[6]->data()),
          velocypack::Slice(insertedDocsView[4]->data()),
          velocypack::Slice(insertedDocsView[3]->data()),
          velocypack::Slice(insertedDocsView[0]->data())};

      auto queryResult = executeQuery(_vocbase, query);
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      velocypack::ArrayIterator resultIt(result);
      ASSERT_EQ(expectedDocs.size(), resultIt.size());

      // Check documents
      auto expectedDoc = expectedDocs.begin();
      for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
        auto const actualDoc = resultIt.value();
        auto const resolved = actualDoc.resolveExternals();

        EXPECT_EQUAL_SLICES(velocypack::Slice(*expectedDoc), resolved);
      }
      EXPECT_EQ(expectedDoc, expectedDocs.end());
    }

    // FOR x IN testCollection2
    //   FOR d IN testView
    //   SEARCH d.seq == x.seq
    //   SORT BM25(d) ASC, d.seq DESC
    // RETURN d;
    {
      std::vector<velocypack::Slice> expectedDocs{
          velocypack::Slice(insertedDocsView[7]->data()),
          velocypack::Slice(insertedDocsView[6]->data()),
          velocypack::Slice(insertedDocsView[5]->data()),
          velocypack::Slice(insertedDocsView[4]->data()),
          velocypack::Slice(insertedDocsView[3]->data()),
          velocypack::Slice(insertedDocsView[2]->data()),
          velocypack::Slice(insertedDocsView[1]->data()),
          velocypack::Slice(insertedDocsView[0]->data())};

      auto queryResult = executeQuery(_vocbase,
                                      "FOR x IN testCollection2 FOR d IN "
                                      "testView SEARCH x.seq == d.seq SORT "
                                      "BM25(d) ASC, d.seq DESC RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      velocypack::ArrayIterator resultIt(result);
      ASSERT_EQ(expectedDocs.size(), resultIt.size());

      // Check documents
      auto expectedDoc = expectedDocs.begin();
      for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
        auto const actualDoc = resultIt.value();
        auto const resolved = actualDoc.resolveExternals();

        EXPECT_EQUAL_SLICES(velocypack::Slice(*expectedDoc), resolved);
      }
      EXPECT_EQ(expectedDoc, expectedDocs.end());
    }

    // Note: unable to push condition to the `View` now
    // FOR d IN testView
    //   FOR x IN testCollection2
    //   SEARCH d.seq == x.seq
    // RETURN d;
    {
      std::string const query =
          "FOR d IN testView FOR x IN testCollection2 FILTER d.seq == x.seq "
          "SORT "
          "d.seq RETURN d";

      EXPECT_TRUE(assertRules(_vocbase, query, {}));

      std::vector<velocypack::Slice> expectedDocs{
          velocypack::Slice(insertedDocsView[0]->data()),
          velocypack::Slice(insertedDocsView[1]->data()),
          velocypack::Slice(insertedDocsView[2]->data()),
          velocypack::Slice(insertedDocsView[3]->data()),
          velocypack::Slice(insertedDocsView[4]->data()),
          velocypack::Slice(insertedDocsView[5]->data()),
          velocypack::Slice(insertedDocsView[6]->data()),
          velocypack::Slice(insertedDocsView[7]->data())};

      auto queryResult = executeQuery(_vocbase, query);
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      velocypack::ArrayIterator resultIt(result);
      ASSERT_EQ(expectedDocs.size(), resultIt.size());

      // Check documents
      auto expectedDoc = expectedDocs.begin();
      for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
        auto const actualDoc = resultIt.value();
        auto const resolved = actualDoc.resolveExternals();

        EXPECT_EQUAL_SLICES(velocypack::Slice(*expectedDoc), resolved);
      }
      EXPECT_EQ(expectedDoc, expectedDocs.end());
    }

    // Note: unable to push condition to the `View` now
    // FOR d IN testView
    //   FOR x IN testCollection2
    //   SEARCH d.seq == x.seq && d.name == 'B'
    // RETURN d;
    {
      std::string const query =
          "FOR d IN testView FOR x IN testCollection2 FILTER d.seq == x.seq && "
          "d.name == 'B' RETURN d";

      EXPECT_TRUE(assertRules(_vocbase, query, {}));

      std::vector<velocypack::Slice> expectedDocs{
          velocypack::Slice(insertedDocsView[1]->data())};

      auto queryResult = executeQuery(_vocbase, query);
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      velocypack::ArrayIterator resultIt(result);
      ASSERT_EQ(expectedDocs.size(), resultIt.size());

      // Check documents
      auto expectedDoc = expectedDocs.begin();
      for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
        auto const actualDoc = resultIt.value();
        auto const resolved = actualDoc.resolveExternals();

        EXPECT_EQUAL_SLICES(velocypack::Slice(*expectedDoc), resolved);
      }
      EXPECT_EQ(expectedDoc, expectedDocs.end());
    }

    // Note: unable to push condition to the `View` now
    // FOR d IN (FOR c IN testView SEARCH c.name >= 'E' && c.seq < 10 RETURN c)
    //   FOR x IN testCollection2
    //   SEARCH d.seq == x.seq
    // RETURN d;
    {
      std::string const query =
          "FOR d IN (FOR c IN testView SEARCH c.name >= 'E' && c.seq < 10 "
          "RETURN "
          "c) FOR x IN testCollection2 FILTER d.seq == x.seq SORT d.seq RETURN "
          "d";

      EXPECT_TRUE(
          assertRules(_vocbase, query,
                      {
                          aql::OptimizerRule::handleArangoSearchViewsRule,
                      }));

      std::vector<velocypack::Slice> expectedDocs{
          velocypack::Slice(insertedDocsView[4]->data()),
          velocypack::Slice(insertedDocsView[5]->data()),
          velocypack::Slice(insertedDocsView[6]->data()),
          velocypack::Slice(insertedDocsView[7]->data())};

      auto queryResult = executeQuery(_vocbase, query);
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      velocypack::ArrayIterator resultIt(result);
      ASSERT_EQ(expectedDocs.size(), resultIt.size());

      // Check documents
      auto expectedDoc = expectedDocs.begin();
      for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
        auto const actualDoc = resultIt.value();
        auto const resolved = actualDoc.resolveExternals();

        EXPECT_EQUAL_SLICES(velocypack::Slice(*expectedDoc), resolved);
      }
      EXPECT_EQ(expectedDoc, expectedDocs.end());
    }

    // Note: unable to push condition to the `View` now
    // FOR d IN (FOR c IN testView SEARCH c.name >= 'E' && c.seq < 10 SORT
    // TFIDF(c) ASC, c.seq DESC RETURN c)
    //   FOR x IN testCollection2
    //   SEARCH d.seq == x.seq
    // RETURN d;
    {
      std::string const query =
          "FOR d IN (FOR c IN testView SEARCH c.name >= 'E' && c.seq < 10 SORT "
          "TFIDF(c) ASC, c.seq DESC RETURN c) FOR x IN testCollection2 FILTER "
          "d.seq "
          "== x.seq RETURN d";

      EXPECT_TRUE(
          assertRules(_vocbase, query,
                      {
                          aql::OptimizerRule::handleArangoSearchViewsRule,
                      }));

      std::vector<velocypack::Slice> expectedDocs{
          velocypack::Slice(insertedDocsView[7]->data()),
          velocypack::Slice(insertedDocsView[6]->data()),
          velocypack::Slice(insertedDocsView[5]->data()),
          velocypack::Slice(insertedDocsView[4]->data())};

      auto queryResult = executeQuery(_vocbase, query);
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      velocypack::ArrayIterator resultIt(result);
      ASSERT_EQ(expectedDocs.size(), resultIt.size());

      // Check documents
      auto expectedDoc = expectedDocs.begin();
      for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
        auto const actualDoc = resultIt.value();
        auto const resolved = actualDoc.resolveExternals();

        EXPECT_EQUAL_SLICES(velocypack::Slice(*expectedDoc), resolved);
      }
      EXPECT_EQ(expectedDoc, expectedDocs.end());
    }

    // Note: unable to push condition to the `View` now
    // FOR d IN (FOR c IN testView SEARCH c.name >= 'E' && c.seq < 10 SORT
    // TFIDF(c) ASC, c.seq DESC RETURN c)
    //   FOR x IN testCollection2
    //   SEARCH d.seq == x.seq
    // LIMIT 2
    // RETURN d;
    {
      std::string const query =
          "FOR d IN (FOR c IN testView SEARCH c.name >= 'E' && c.seq < 10 SORT "
          "TFIDF(c) ASC, c.seq DESC RETURN c) FOR x IN testCollection2 FILTER "
          "d.seq "
          "== x.seq LIMIT 2 RETURN d";

      EXPECT_TRUE(
          assertRules(_vocbase, query,
                      {
                          aql::OptimizerRule::handleArangoSearchViewsRule,
                      }));

      std::vector<velocypack::Slice> expectedDocs{
          velocypack::Slice(insertedDocsView[7]->data()),
          velocypack::Slice(insertedDocsView[6]->data()),
      };

      auto queryResult = executeQuery(_vocbase, query);
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      velocypack::ArrayIterator resultIt(result);
      ASSERT_EQ(expectedDocs.size(), resultIt.size());

      // Check documents
      auto expectedDoc = expectedDocs.begin();
      for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
        auto const actualDoc = resultIt.value();
        auto const resolved = actualDoc.resolveExternals();

        EXPECT_EQUAL_SLICES(velocypack::Slice(*expectedDoc), resolved);
      }
      EXPECT_EQ(expectedDoc, expectedDocs.end());
    }

    // Note: unable to push condition to the `View` now
    // FOR d IN (FOR c IN testView SEARCH c.name >= 'E' && c.seq < 10 SORT
    // TFIDF(c) ASC, c.seq DESC LIMIT 3 RETURN c)
    //   FOR x IN testCollection2
    //   SEARCH d.seq == x.seq
    // RETURN d;
    {
      std::string const query =
          "FOR d IN (FOR c IN testView SEARCH c.name >= 'E' && c.seq < 10 SORT "
          "TFIDF(c) ASC, c.seq DESC LIMIT 5 RETURN c) FOR x IN testCollection2 "
          "FILTER d.seq == x.seq RETURN d";

      EXPECT_TRUE(assertRules(
          _vocbase, query, {aql::OptimizerRule::handleArangoSearchViewsRule}));

      std::vector<velocypack::Slice> expectedDocs{
          velocypack::Slice(insertedDocsView[7]->data()),
          velocypack::Slice(insertedDocsView[6]->data()),
          velocypack::Slice(insertedDocsView[5]->data())};

      auto queryResult = executeQuery(_vocbase, query);
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      velocypack::ArrayIterator resultIt(result);
      ASSERT_EQ(expectedDocs.size(), resultIt.size());

      // Check documents
      auto expectedDoc = expectedDocs.begin();
      for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
        auto const actualDoc = resultIt.value();
        auto const resolved = actualDoc.resolveExternals();

        EXPECT_EQUAL_SLICES(velocypack::Slice(*expectedDoc), resolved);
      }
      EXPECT_EQ(expectedDoc, expectedDocs.end());
    }

    // Invalid bound collection name
    {
      auto queryResult = executeQuery(
          _vocbase,
          "FOR d IN (FOR c IN testView SEARCH c.name >= 'E' && c.seq < 10 SORT "
          "TFIDF(c) ASC, c.seq DESC LIMIT 5 RETURN c) FOR x IN @@collection "
          "SEARCH d.seq == x.seq RETURN d",
          VPackParser::fromJson(
              "{ \"@collection\": \"invlaidCollectionName\" }"));

      ASSERT_TRUE(
          queryResult.result.is(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND));
    }

    // dependent sort condition in inner loop + custom scorer
    // (must recreate view iterator each loop iteration)
    //
    // FOR x IN 0..5
    //   FOR d IN testView
    //   SEARCH d.seq == x
    //   SORT customscorer(d,x)
    // RETURN d;
    {
      std::string const query =
          "FOR x IN 0..5 FOR d IN testView SEARCH d.seq == x SORT "
          "customscorer(d, x) DESC RETURN d";

      EXPECT_TRUE(
          assertRules(_vocbase, query,
                      {
                          aql::OptimizerRule::handleArangoSearchViewsRule,
                      }));

      std::vector<velocypack::Slice> expectedDocs{
          velocypack::Slice(insertedDocsView[5]->data()),
          velocypack::Slice(insertedDocsView[4]->data()),
          velocypack::Slice(insertedDocsView[3]->data()),
          velocypack::Slice(insertedDocsView[2]->data()),
          velocypack::Slice(insertedDocsView[1]->data()),
          velocypack::Slice(insertedDocsView[0]->data()),
      };

      auto queryResult = executeQuery(_vocbase, query);
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      velocypack::ArrayIterator resultIt(result);
      ASSERT_EQ(expectedDocs.size(), resultIt.size());

      // Check documents
      auto expectedDoc = expectedDocs.begin();
      for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
        auto const actualDoc = resultIt.value();
        auto const resolved = actualDoc.resolveExternals();

        EXPECT_EQUAL_SLICES(velocypack::Slice(*expectedDoc), resolved);
      }
      EXPECT_EQ(expectedDoc, expectedDocs.end());
    }

    // invalid reference in scorer
    {
      std::string const query =
          "FOR d IN testView FOR i IN 0..5 SORT tfidf(i) DESC RETURN d";

      EXPECT_TRUE(assertRules(_vocbase, query, {}));

      auto queryResult = executeQuery(_vocbase, query);
      ASSERT_TRUE(queryResult.result.is(
          TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH));
    }

    // FOR i IN 1..5
    //  FOR x IN testCollection0
    //    FOR d IN  SEARCH d.seq == i && d.name == x.name
    // SORT customscorer(d, x.seq)
    {
      std::string const query =
          "FOR i IN 1..5 FOR x IN testCollection0 FOR d IN testView SEARCH "
          "d.seq "
          "== "
          "i AND d.name == x.name SORT customscorer(d, x.seq) DESC RETURN d";

      EXPECT_TRUE(
          assertRules(_vocbase, query,
                      {
                          aql::OptimizerRule::handleArangoSearchViewsRule,
                      }));

      std::vector<velocypack::Slice> expectedDocs{
          velocypack::Slice(insertedDocsView[4]->data()),
          velocypack::Slice(insertedDocsView[2]->data())};

      auto queryResult = executeQuery(_vocbase, query);
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      velocypack::ArrayIterator resultIt(result);
      ASSERT_EQ(expectedDocs.size(), resultIt.size());

      // Check documents
      auto expectedDoc = expectedDocs.begin();
      for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
        auto const actualDoc = resultIt.value();
        auto const resolved = actualDoc.resolveExternals();

        EXPECT_EQUAL_SLICES(velocypack::Slice(*expectedDoc), resolved);
      }
      EXPECT_EQ(expectedDoc, expectedDocs.end());
    }

    // FOR i IN 1..5
    //  FOR x IN testCollection0 SEARCH x.seq == i
    //    FOR d IN  SEARCH d.seq == x.seq && d.name == x.name
    // SORT customscorer(d, x.seq)
    {
      std::string const query =
          "FOR i IN 1..5 FOR x IN testCollection0 FILTER x.seq == i FOR d IN "
          "testView SEARCH d.seq == x.seq AND d.name == x.name SORT "
          "customscorer(d, x.seq) DESC RETURN d";

      EXPECT_TRUE(
          assertRules(_vocbase, query,
                      {
                          aql::OptimizerRule::handleArangoSearchViewsRule,
                      }));

      std::vector<velocypack::Slice> expectedDocs{
          velocypack::Slice(insertedDocsView[4]->data()),
          velocypack::Slice(insertedDocsView[2]->data()),
      };

      auto queryResult = executeQuery(_vocbase, query);
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      velocypack::ArrayIterator resultIt(result);
      ASSERT_EQ(expectedDocs.size(), resultIt.size());

      // Check documents
      auto expectedDoc = expectedDocs.begin();
      for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
        auto const actualDoc = resultIt.value();
        auto const resolved = actualDoc.resolveExternals();

        EXPECT_EQUAL_SLICES(velocypack::Slice(*expectedDoc), resolved);
      }
      EXPECT_EQ(expectedDoc, expectedDocs.end());
    }

    {
      std::string const query =
          "LET attr = _NONDETERM_('seq') "
          "FOR i IN 1..5 "
          "  FOR x IN testCollection0 FILTER x.seq == i "
          "    FOR d IN testView SEARCH d.seq == x.seq AND d.name == x.name "
          "      SORT customscorer(d, x[attr]) DESC "
          "RETURN d";

      EXPECT_TRUE(
          assertRules(_vocbase, query,
                      {
                          aql::OptimizerRule::handleArangoSearchViewsRule,
                      }));

      std::vector<velocypack::Slice> expectedDocs{
          velocypack::Slice(insertedDocsView[4]->data()),
          velocypack::Slice(insertedDocsView[2]->data()),
      };

      auto queryResult = executeQuery(_vocbase, query);
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      velocypack::ArrayIterator resultIt(result);
      ASSERT_EQ(expectedDocs.size(), resultIt.size());

      // Check documents
      auto expectedDoc = expectedDocs.begin();
      for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
        auto const actualDoc = resultIt.value();
        auto const resolved = actualDoc.resolveExternals();

        EXPECT_EQUAL_SLICES(velocypack::Slice(*expectedDoc), resolved);
      }
      EXPECT_EQ(expectedDoc, expectedDocs.end());
    }

    // FOR i IN 1..5
    //  FOR x IN testCollection0 SEARCH x.seq == i
    //    FOR d IN  SEARCH d.seq == x.seq && d.name == x.name
    // SORT customscorer(d, x.seq)
    {
      std::string const query =
          "FOR i IN 1..5 FOR x IN testCollection0 FILTER x.seq == i FOR d IN "
          "testView SEARCH d.seq == x.seq AND d.name == x.name SORT "
          "customscorer(d, x['seq']) DESC RETURN d";

      EXPECT_TRUE(
          assertRules(_vocbase, query,
                      {
                          aql::OptimizerRule::handleArangoSearchViewsRule,
                      }));

      std::vector<velocypack::Slice> expectedDocs{
          velocypack::Slice(insertedDocsView[4]->data()),
          velocypack::Slice(insertedDocsView[2]->data()),
      };

      auto queryResult = executeQuery(_vocbase, query);
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      velocypack::ArrayIterator resultIt(result);
      ASSERT_EQ(expectedDocs.size(), resultIt.size());

      // Check documents
      auto expectedDoc = expectedDocs.begin();
      for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
        auto const actualDoc = resultIt.value();
        auto const resolved = actualDoc.resolveExternals();

        EXPECT_EQUAL_SLICES(velocypack::Slice(*expectedDoc), resolved);
      }
      EXPECT_EQ(expectedDoc, expectedDocs.end());
    }

    // unable to retrieve `d.seq` from self-referenced variable
    // FOR i IN 1..5
    //  FOR d IN  SEARCH d.seq == i SORT customscorer(d, d.seq)
    //    FOR x IN testCollection0 SEARCH x.seq == d.seq && x.name == d.name
    // SORT customscorer(d, d.seq) DESC
    {
      std::string const query =
          "FOR i IN 1..5 FOR d IN testView SEARCH d.seq == i FOR x IN "
          "testCollection0 FILTER x.seq == d.seq && x.name == d.name SORT "
          "customscorer(d, d.seq) DESC RETURN d";

      EXPECT_TRUE(
          assertRules(_vocbase, query,
                      {
                          aql::OptimizerRule::handleArangoSearchViewsRule,
                      }));

      auto queryResult = executeQuery(_vocbase, query);
      ASSERT_TRUE(queryResult.result.is(TRI_ERROR_BAD_PARAMETER));
    }

    // unable to retrieve `x.seq` from inner loop
    // FOR i IN 1..5
    //  FOR d IN  SEARCH d.seq == i SORT customscorer(d, d.seq)
    //    FOR x IN testCollection0 SEARCH x.seq == d.seq && x.name == d.name
    // SORT customscorer(d, x.seq) DESC
    {
      std::string const query =
          "FOR i IN 1..5 FOR d IN testView SEARCH d.seq == i FOR x IN "
          "testCollection0 FILTER x.seq == d.seq && x.name == d.name SORT "
          "customscorer(d, x.seq) DESC RETURN d";

      auto queryResult = explainQuery(_vocbase, query);
      ASSERT_TRUE(queryResult.result.is(TRI_ERROR_BAD_PARAMETER));
      ASSERT_TRUE(std::regex_search(
          std::string(queryResult.errorMessage()),
          std::regex(
              "variable '.+' is used in search function.*CUSTOMSCORER")));

      queryResult = executeQuery(_vocbase, query);
      ASSERT_TRUE(queryResult.result.is(TRI_ERROR_BAD_PARAMETER));
    }

    // FOR i IN 1..5
    //  FOR d IN  SEARCH d.seq == i SORT customscorer(d, i) ASC
    //    FOR x IN testCollection0 SEARCH x.seq == d.seq && x.name == d.name
    // SORT customscorer(d, i) DESC
    {
      std::string const query =
          "FOR i IN 1..5 "
          "  FOR d IN testView SEARCH d.seq == i SORT customscorer(d, i) ASC "
          "    FOR x IN testCollection0 FILTER x.seq == d.seq && x.name == "
          "d.name "
          "SORT customscorer(d, i) DESC RETURN d";

      EXPECT_TRUE(
          assertRules(_vocbase, query,
                      {
                          aql::OptimizerRule::handleArangoSearchViewsRule,
                      }));

      std::vector<velocypack::Slice> expectedDocs{
          velocypack::Slice(insertedDocsView[4]->data()),
          velocypack::Slice(insertedDocsView[2]->data()),
      };

      auto queryResult = executeQuery(_vocbase, query);
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      velocypack::ArrayIterator resultIt(result);
      ASSERT_EQ(expectedDocs.size(), resultIt.size());

      // Check documents
      auto expectedDoc = expectedDocs.begin();
      for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
        auto const actualDoc = resultIt.value();
        auto const resolved = actualDoc.resolveExternals();

        EXPECT_EQUAL_SLICES(velocypack::Slice(*expectedDoc), resolved);
      }
      EXPECT_EQ(expectedDoc, expectedDocs.end());
    }

    // dedicated to https://github.com/arangodb/planning/issues/3065$
    // Optimizer rule "inline sub-queries" which doesn't handle views correctly$
    {
      std::string const query =
          "LET fullAccounts = (FOR acc1 IN [1] RETURN { 'key': 'A' }) for a IN "
          "fullAccounts for d IN testView SEARCH d.name == a.key return d";

      EXPECT_TRUE(assertRules(_vocbase, query,
                              {aql::OptimizerRule::handleArangoSearchViewsRule,
                               aql::OptimizerRule::inlineSubqueriesRule}));

      std::vector<velocypack::Slice> expectedDocs{
          velocypack::Slice(insertedDocsView[0]->data()),
      };

      auto queryResult = executeQuery(_vocbase, query);
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      velocypack::ArrayIterator resultIt(result);
      ASSERT_EQ(expectedDocs.size(), resultIt.size());

      // Check documents
      auto expectedDoc = expectedDocs.begin();
      for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
        auto const actualDoc = resultIt.value();
        auto const resolved = actualDoc.resolveExternals();

        EXPECT_EQUAL_SLICES(velocypack::Slice(*expectedDoc), resolved);
      }
      EXPECT_EQ(expectedDoc, expectedDocs.end());
    }

    // FOR i IN 1..5
    //   FOR d IN testView SEARCH d.seq == i
    //     FOR x IN testCollection0 FILTER x.seq == d.seq && x.seq == TFIDF(d)
    {
      std::string const query =
          "FOR i IN 1..5 "
          "  FOR d IN testView SEARCH d.seq == i "
          "    FOR x IN testCollection0 FILTER x.seq == d.seq && x.seq == "
          "customscorer(d, i)"
          "RETURN x";

      EXPECT_TRUE(
          assertRules(_vocbase, query,
                      {
                          aql::OptimizerRule::handleArangoSearchViewsRule,
                      }));

      std::vector<velocypack::Slice> expectedDocs{
          velocypack::Slice(insertedDocsView[2]->data()),
          velocypack::Slice(insertedDocsView[4]->data()),
      };

      auto queryResult = executeQuery(_vocbase, query);
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      velocypack::ArrayIterator resultIt(result);
      ASSERT_EQ(expectedDocs.size(), resultIt.size());

      // Check documents
      auto expectedDoc = expectedDocs.begin();
      for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
        auto const actualDoc = resultIt.value();
        auto const resolved = actualDoc.resolveExternals();

        EXPECT_EQUAL_SLICES(velocypack::Slice(*expectedDoc), resolved);
      }
      EXPECT_EQ(expectedDoc, expectedDocs.end());
    }

    {
      std::string const query =
          "FOR i IN 1..5 "
          "  FOR d IN testView SEARCH d.seq == i "
          "    FOR x IN testCollection0 FILTER x.seq == d.seq "
          "SORT 1 + customscorer(d, i) DESC "
          "RETURN d";

      EXPECT_TRUE(
          assertRules(_vocbase, query,
                      {
                          aql::OptimizerRule::handleArangoSearchViewsRule,
                      }));

      std::vector<velocypack::Slice> expectedDocs{
          velocypack::Slice(insertedDocsView[4]->data()),
          velocypack::Slice(insertedDocsView[2]->data()),
      };

      auto queryResult = executeQuery(_vocbase, query);
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      velocypack::ArrayIterator resultIt(result);
      ASSERT_EQ(expectedDocs.size(), resultIt.size());

      // Check documents
      auto expectedDoc = expectedDocs.begin();
      for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
        auto const actualDoc = resultIt.value();
        auto const resolved = actualDoc.resolveExternals();

        EXPECT_EQUAL_SLICES(velocypack::Slice(*expectedDoc), resolved);
      }
      EXPECT_EQ(expectedDoc, expectedDocs.end());
    }

    // multiple sorts
    {
      std::string const query =
          "FOR i IN 1..5 "
          "  FOR d IN testView SEARCH d.seq == i SORT tfidf(d, i > 0) ASC "
          "    FOR x IN testCollection0 FILTER x.seq == d.seq && x.name == "
          "d.name "
          "SORT customscorer(d, i) DESC RETURN d";

      EXPECT_TRUE(
          assertRules(_vocbase, query,
                      {
                          aql::OptimizerRule::handleArangoSearchViewsRule,
                      }));

      std::vector<velocypack::Slice> expectedDocs{
          velocypack::Slice(insertedDocsView[4]->data()),
          velocypack::Slice(insertedDocsView[2]->data()),
      };

      auto queryResult = executeQuery(_vocbase, query);
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      velocypack::ArrayIterator resultIt(result);
      ASSERT_EQ(expectedDocs.size(), resultIt.size());

      // Check documents
      auto expectedDoc = expectedDocs.begin();
      for (; resultIt.valid(); resultIt.next(), ++expectedDoc) {
        auto const actualDoc = resultIt.value();
        auto const resolved = actualDoc.resolveExternals();

        EXPECT_EQUAL_SLICES(velocypack::Slice(*expectedDoc), resolved);
      }
      EXPECT_EQ(expectedDoc, expectedDocs.end());
    }

    // x.seq is used before being assigned
    {
      std::string const query =
          "FOR d IN testView SEARCH d.name >= 'E' && d.seq < 10 "
          "  SORT customscorer(d) DESC "
          "  LIMIT 3 "
          "  FOR x IN testCollection0 FILTER x.seq == d.seq "
          "    SORT customscorer(d, x.seq) "
          "RETURN x";

      auto queryResult = explainQuery(_vocbase, query);
      ASSERT_TRUE(queryResult.result.is(TRI_ERROR_BAD_PARAMETER));
      ASSERT_TRUE(std::regex_search(
          std::string(queryResult.errorMessage()),
          std::regex(
              "variable '.+' is used in search function.*CUSTOMSCORER")));

      queryResult = executeQuery(_vocbase, query);
      ASSERT_TRUE(queryResult.result.is(TRI_ERROR_BAD_PARAMETER));
    }

    // x.seq is used before being assigned
    {
      std::string const query =
          "FOR d IN (FOR c IN testView SEARCH c.name >= 'E' && c.seq < 10 SORT "
          "customscorer(c) DESC LIMIT 3 RETURN c) "
          "  FOR x IN testCollection0 FILTER x.seq == d.seq "
          "    SORT customscorer(d, x.seq) "
          "RETURN x";

      auto queryResult = explainQuery(_vocbase, query);
      ASSERT_TRUE(queryResult.result.is(TRI_ERROR_BAD_PARAMETER));
      ASSERT_TRUE(std::regex_search(
          std::string(queryResult.errorMessage()),
          std::regex(
              "variable '.+' is used in search function.*CUSTOMSCORER")));

      queryResult = executeQuery(_vocbase, query);
      ASSERT_TRUE(queryResult.result.is(TRI_ERROR_BAD_PARAMETER));
    }
  }
};

class QueryJoinView : public QueryJoin {
 protected:
  ViewType type() const final { return ViewType::kArangoSearch; }

  void createView1() {
    {
      auto json = VPackParser::fromJson(absl::Substitute(R"({
      "name" : "entities_view",
      "writebufferSizeMax": 33554432,
      "consolidationPolicy": {
        "type": "bytes_accum",
        "threshold": 0.10000000149011612
      },
      "globallyUniqueId": "hB4A95C21732A/218",
      "id": "218",
      "writebufferActive": 0,
      "consolidationIntervalMsec": 60000,
      "cleanupIntervalStep": 10,
      "links": {
        "entities": {
          "analyzers": [ "identity" ],
          "fields": {},
          "includeAllFields": true,
          "storeValues": "id",
          "version": $0,
          "trackListPositions": false }
      },
      "type": "arangosearch",
      "writebufferIdle": 64
      })",
                                                         version()));
      std::shared_ptr<LogicalView> view;
      ASSERT_TRUE(
          LogicalView::create(view, _vocbase, json->slice(), true).ok());
      ASSERT_TRUE(view);
    }
    {
      auto json = VPackParser::fromJson(absl::Substitute(R"({
      "name" : "links_view",
      "writebufferSizeMax": 33554432,
      "consolidationPolicy": {
        "type": "bytes_accum", "threshold": 0.10000000149011612
      },
      "globallyUniqueId": "hB4A95C21732A/181",
      "id": "181",
      "writebufferActive": 0,
      "consolidationIntervalMsec": 60000,
      "cleanupIntervalStep": 10,
      "links": {
        "links": {
          "analyzers": [ "identity" ],
          "fields": {},
          "includeAllFields": true,
          "storeValues": "id",
          "version": $0,
          "trackListPositions": false }
      },
      "type": "arangosearch",
      "writebufferIdle": 64 })",
                                                         version()));
      std::shared_ptr<LogicalView> view;
      ASSERT_TRUE(
          LogicalView::create(view, _vocbase, json->slice(), true).ok());
      ASSERT_TRUE(view);
    }
  }
};

class QueryJoinSearch : public QueryJoin {
 protected:
  ViewType type() const final { return ViewType::kSearchAlias; }

  void createSearch1() {
    auto createIndexName = [&](std::string_view name) {
      bool created = false;
      // TODO remove fields, also see SEARCH-334
      auto createJson = velocypack::Parser::fromJson(absl::Substitute(
          R"({ "name": "$1Index", "type": "inverted", "version": $0,
               "includeAllFields": true,
               "fields": [ { "name": "this_field_no_exist_just_stub_for_definition_parser" } ] })",
          version(), name));
      auto collection = _vocbase.lookupCollection(name);
      EXPECT_TRUE(collection);
      collection->createIndex(createJson->slice(), created).waitAndGet();
      ASSERT_TRUE(created);
    };
    auto createSearchName = [&](std::string_view name) {
      auto createJson = velocypack::Parser::fromJson(absl::Substitute(
          R"({ "name": "$0_view", "type": "search-alias" })", name));
      auto logicalView = _vocbase.createView(createJson->slice(), false);
      ASSERT_FALSE(!logicalView);
      auto& implView = basics::downCast<iresearch::Search>(*logicalView);
      auto updateJson = velocypack::Parser::fromJson(absl::Substitute(
          R"({ "indexes": [ { "collection": "$0", "index": "$0Index" } ] })",
          name));
      auto r = implView.properties(updateJson->slice(), true, true);
      EXPECT_TRUE(r.ok()) << r.errorMessage();
    };
    createIndexName("entities");
    createSearchName("entities");
    createIndexName("links");
    createSearchName("links");
  }
};

TEST_P(QueryJoinView, Subquery) {
  createCollections1();
  createView1();
  queryTests1();
}

TEST_P(QueryJoinSearch, Subquery) {
  createCollections1();
  createSearch1();
  queryTests1();
}

TEST_P(QueryJoinView, DuplicateDataSource) {
  createCollections23();
  createView(
      R"("analyzers": [ "test_analyzer", "identity" ],
         "trackListPositions": true, "storeValues": "id",)",
      R"("analyzers": [ "test_analyzer", "identity" ],
         "storeValues": "id",)");
  queryTests2();
}

TEST_P(QueryJoinView, DuplicateDataSourceWithoutStoreValues) {
  createCollections23();
  createView(
      R"("analyzers": [ "test_analyzer", "identity" ], "trackListPositions": true,)",
      R"("analyzers": [ "test_analyzer", "identity" ],)");
  queryTests2();
}

TEST_P(QueryJoinSearch, DuplicateDataSourceIdentity) {
  createCollections23();
  createIndexes(R"("analyzer": "identity", "trackListPositions": true,)",
                R"("analyzer": "identity",)");
  createSearch();
  queryTests2();
}

TEST_P(QueryJoinSearch, DuplicateDataSourceTestAnalyzer) {
  createCollections23();
  createIndexes(R"("analyzer": "test_analyzer", "trackListPositions": true,)",
                R"("analyzer": "test_analyzer",)");
  createSearch();
  queryTests2();
}

TEST_P(QueryJoinView, Test) {
  createCollections23();
  createView(
      R"("analyzers": [ "test_analyzer", "identity" ],
         "trackListPositions": true, "storeValues": "id",)",
      R"("analyzers": [ "test_analyzer", "identity" ],
         "storeValues": "id",)");
  queryTests3();
}

TEST_P(QueryJoinView, TestWithoutStoreValues) {
  createCollections23();
  createView(
      R"("analyzers": [ "test_analyzer", "identity" ], "trackListPositions": true,)",
      R"("analyzers": [ "test_analyzer", "identity" ],)");
  queryTests3();
}

TEST_P(QueryJoinSearch, TestIdentity) {
  createCollections23();
  createIndexes(R"("analyzer": "identity", "trackListPositions": true,)",
                R"("analyzer": "identity",)");
  createSearch();
  queryTests3();
}

TEST_P(QueryJoinSearch, TestTestAnalyzer) {
  createCollections23();
  createIndexes(R"("analyzer": "test_analyzer", "trackListPositions": true,)",
                R"("analyzer": "test_analyzer",)");
  createSearch();
  queryTests3();
}

INSTANTIATE_TEST_CASE_P(IResearch, QueryJoinView, GetLinkVersions());

INSTANTIATE_TEST_CASE_P(IResearch, QueryJoinSearch, GetIndexVersions());

}  // namespace
}  // namespace arangodb::tests
