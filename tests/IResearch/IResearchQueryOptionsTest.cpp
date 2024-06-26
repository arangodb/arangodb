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

#include <velocypack/Iterator.h>

#include "Aql/OptimizerRule.h"
#include "Aql/Query.h"
#include "IResearch/IResearchLink.h"
#include "IResearch/IResearchLinkHelper.h"
#include "IResearch/IResearchVPackComparer.h"
#include "IResearch/IResearchView.h"
#include "IResearch/IResearchViewSort.h"
#include "IResearchQueryCommon.h"
#include "Transaction/Helpers.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/OperationOptions.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/LogicalCollection.h"
#include "store/mmap_directory.hpp"
#include "utils/index_utils.hpp"

namespace arangodb::tests {
namespace {

static std::vector<std::string> const kEmpty;

class QueryOptions : public QueryTest {
 protected:
  std::deque<std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>>>
      _insertedDocs;

  void create() {
    // add collection_1
    {
      auto collectionJson = arangodb::velocypack::Parser::fromJson(
          "{ \"name\": \"collection_1\" }");
      auto logicalCollection1 =
          _vocbase.createCollection(collectionJson->slice());
      ASSERT_NE(nullptr, logicalCollection1);
    }

    // add collection_2
    {
      auto collectionJson = arangodb::velocypack::Parser::fromJson(
          "{ \"name\": \"collection_2\" }");
      auto logicalCollection2 =
          _vocbase.createCollection(collectionJson->slice());
      ASSERT_NE(nullptr, logicalCollection2);
    }

    // add collection_3
    {
      auto collectionJson = arangodb::velocypack::Parser::fromJson(
          "{ \"name\": \"collection_3\" }");
      auto logicalCollection3 =
          _vocbase.createCollection(collectionJson->slice());
      ASSERT_NE(nullptr, logicalCollection3);
    }
  }

  void populateData0() {
    auto logicalCollection1 = _vocbase.lookupCollection("collection_1");
    ASSERT_TRUE(logicalCollection1);
    auto logicalCollection2 = _vocbase.lookupCollection("collection_2");
    ASSERT_TRUE(logicalCollection2);

    // populate view with the data
    {
      arangodb::OperationOptions opt;

      arangodb::transaction::Methods trx(
          arangodb::transaction::StandaloneContext::create(
              _vocbase, arangodb::transaction::OperationOriginTestCase{}),
          kEmpty, {logicalCollection1->name(), logicalCollection2->name()},
          kEmpty, arangodb::transaction::Options());
      EXPECT_TRUE(trx.begin().ok());

      // insert into collections
      {
        std::filesystem::path resource;
        resource /= std::string_view(arangodb::tests::testResourceDir);
        resource /= std::string_view("simple_sequential.json");

        auto builder = arangodb::basics::VelocyPackHelper::velocyPackFromFile(
            resource.string());
        auto root = builder.slice();
        ASSERT_TRUE(root.isArray());

        std::shared_ptr<arangodb::LogicalCollection> collections[]{
            logicalCollection1, logicalCollection2};

        for (auto doc : arangodb::velocypack::ArrayIterator(root)) {
          {
            auto res = trx.insert(collections[0]->name(), doc, opt);
            EXPECT_TRUE(res.ok());

            res = trx.document(collections[0]->name(), res.slice(), opt);
            EXPECT_TRUE(res.ok());
            _insertedDocs.emplace_back(std::move(res.buffer));
          }
          {
            auto res = trx.insert(collections[1]->name(), doc, opt);
            EXPECT_TRUE(res.ok());

            res = trx.document(collections[1]->name(), res.slice(), opt);
            EXPECT_TRUE(res.ok());
            _insertedDocs.emplace_back(std::move(res.buffer));
          }
        }
      }

      EXPECT_TRUE(trx.commit().ok());
      EXPECT_TRUE((arangodb::tests::executeQuery(
                       _vocbase,
                       "FOR d IN testView SEARCH 1 ==1 OPTIONS "
                       "{ waitForSync: true } RETURN d")
                       .result.ok()));  // commit
    }
  }
  void populateData1() {
    auto logicalCollection1 = _vocbase.lookupCollection("collection_1");
    ASSERT_TRUE(logicalCollection1);
    auto logicalCollection2 = _vocbase.lookupCollection("collection_2");
    ASSERT_TRUE(logicalCollection2);

    // populate view with the data
    {
      arangodb::OperationOptions opt;

      arangodb::transaction::Methods trx(
          arangodb::transaction::StandaloneContext::create(
              _vocbase, arangodb::transaction::OperationOriginTestCase{}),
          kEmpty, {logicalCollection1->name(), logicalCollection2->name()},
          kEmpty, arangodb::transaction::Options());
      EXPECT_TRUE(trx.begin().ok());

      // insert into collections
      {
        std::filesystem::path resource;
        resource /= std::string_view(arangodb::tests::testResourceDir);
        resource /= std::string_view("simple_sequential.json");

        auto builder = arangodb::basics::VelocyPackHelper::velocyPackFromFile(
            resource.string());
        auto root = builder.slice();
        ASSERT_TRUE(root.isArray());

        size_t i = 0;

        std::shared_ptr<arangodb::LogicalCollection> collections[]{
            logicalCollection1, logicalCollection2};

        for (auto doc : arangodb::velocypack::ArrayIterator(root)) {
          auto res = trx.insert(collections[i % 2]->name(), doc, opt);
          EXPECT_TRUE(res.ok());

          res = trx.document(collections[i % 2]->name(), res.slice(), opt);
          EXPECT_TRUE(res.ok());
          _insertedDocs.emplace_back(std::move(res.buffer));
          ++i;
        }
      }

      EXPECT_TRUE(trx.commit().ok());
    }
  }
  void populateData2() {
    auto logicalCollection1 = _vocbase.lookupCollection("collection_1");
    ASSERT_TRUE(logicalCollection1);
    auto logicalCollection2 = _vocbase.lookupCollection("collection_2");
    ASSERT_TRUE(logicalCollection2);

    // populate view with the data
    {
      arangodb::OperationOptions opt;
      static std::vector<std::string> const kEmpty;
      arangodb::transaction::Methods trx(
          arangodb::transaction::StandaloneContext::create(
              _vocbase, arangodb::transaction::OperationOriginTestCase{}),
          kEmpty, {logicalCollection1->name(), logicalCollection2->name()},
          kEmpty, arangodb::transaction::Options());
      EXPECT_TRUE(trx.begin().ok());

      // insert into collection_1
      {
        auto builder = VPackParser::fromJson(
            "["
            "{\"_key\": \"c0\", \"str\": \"cat\", \"foo\": \"foo0\", "
            "\"value\": "
            "0},"
            "{\"_key\": \"c1\", \"str\": \"cat\", \"foo\": \"foo1\", "
            "\"value\": "
            "1},"
            "{\"_key\": \"c2\", \"str\": \"cat\", \"foo\": \"foo2\", "
            "\"value\": "
            "2},"
            "{\"_key\": \"c3\", \"str\": \"cat\", \"foo\": \"foo3\", "
            "\"value\": "
            "3}"
            "]");

        auto root = builder->slice();
        ASSERT_TRUE(root.isArray());

        for (auto doc : arangodb::velocypack::ArrayIterator(root)) {
          auto res = trx.insert(logicalCollection1->name(), doc, opt);
          EXPECT_TRUE(res.ok());
        }
      }

      // insert into collection_2
      {
        auto builder = VPackParser::fromJson(
            "["
            "{\"_key\": \"c_0\", \"str\": \"cat\", \"foo\": \"foo_0\", "
            "\"value\": 10},"
            "{\"_key\": \"c_1\", \"str\": \"cat\", \"foo\": \"foo_1\", "
            "\"value\": 11},"
            "{\"_key\": \"c_2\", \"str\": \"cat\", \"foo\": \"foo_2\", "
            "\"value\": 12},"
            "{\"_key\": \"c_3\", \"str\": \"cat\", \"foo\": \"foo_3\", "
            "\"value\": 13}"
            "]");

        auto root = builder->slice();
        ASSERT_TRUE(root.isArray());

        for (auto doc : arangodb::velocypack::ArrayIterator(root)) {
          auto res = trx.insert(logicalCollection2->name(), doc, opt);
          EXPECT_TRUE(res.ok());
        }
      }

      EXPECT_TRUE(trx.commit().ok());

      EXPECT_TRUE(
          tests::executeQuery(
              _vocbase,
              R"(FOR d IN testView SEARCH 1==1 OPTIONS { waitForSync: true } RETURN d)")
              .result.ok());
    }
  }

  void queryTestCollections() {
    auto logicalCollection1 = _vocbase.lookupCollection("collection_1");
    ASSERT_TRUE(logicalCollection1);
    auto logicalCollection2 = _vocbase.lookupCollection("collection_2");
    ASSERT_TRUE(logicalCollection2);

    // ==, !=, <, <=, >, >=, range
    // -----------------------------------------------------------------------------
    // --SECTION--                                              'collections'
    // option
    // -----------------------------------------------------------------------------

    // collection by name
    {
      std::string const query =
          "FOR d IN testView SEARCH d.name == 'A' OPTIONS { collections : [ "
          "'collection_1' ] }"
          " RETURN d";

      EXPECT_TRUE(arangodb::tests::assertRules(
          _vocbase, query,
          {arangodb::aql::OptimizerRule::handleArangoSearchViewsRule}));

      std::map<std::string_view,
               std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>>>
          expectedDocs{{"A", _insertedDocs[0]}};

      auto queryResult = arangodb::tests::executeQuery(_vocbase, query);
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(expectedDocs.size(), resultIt.size());

      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        auto const keySlice = resolved.get("name");
        auto const key = arangodb::iresearch::getStringRef(keySlice);

        auto expectedDoc = expectedDocs.find(key);
        ASSERT_NE(expectedDoc, expectedDocs.end());
        EXPECT_TRUE(
            0 == arangodb::basics::VelocyPackHelper::compare(
                     arangodb::velocypack::Slice(expectedDoc->second->data()),
                     resolved, true));
        expectedDocs.erase(expectedDoc);
      }
      EXPECT_TRUE(expectedDocs.empty());
    }

    // boound option
    {
      std::string const query =
          "FOR d IN testView SEARCH d.name == 'A' OPTIONS { collections : [ "
          "@collectionName ] }"
          " RETURN d";

      EXPECT_TRUE(arangodb::tests::assertRules(
          _vocbase, query,
          {arangodb::aql::OptimizerRule::handleArangoSearchViewsRule},
          arangodb::velocypack::Parser::fromJson(
              "{ \"collectionName\" : \"collection_1\" }")));

      std::map<std::string_view,
               std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>>>
          expectedDocs{{"A", _insertedDocs[0]}};

      auto queryResult = arangodb::tests::executeQuery(
          _vocbase, query,
          arangodb::velocypack::Parser::fromJson(
              "{ \"collectionName\" : \"collection_1\" }"));
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(expectedDocs.size(), resultIt.size());

      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        auto const keySlice = resolved.get("name");
        auto const key = arangodb::iresearch::getStringRef(keySlice);

        auto expectedDoc = expectedDocs.find(key);
        ASSERT_NE(expectedDoc, expectedDocs.end());
        EXPECT_TRUE(
            0 == arangodb::basics::VelocyPackHelper::compare(
                     arangodb::velocypack::Slice(expectedDoc->second->data()),
                     resolved, true));
        expectedDocs.erase(expectedDoc);
      }
      EXPECT_TRUE(expectedDocs.empty());
    }

    // boound options
    {
      std::string const query =
          "FOR d IN testView SEARCH d.name == 'A' OPTIONS { collections : "
          "@collections }"
          " RETURN d";

      EXPECT_TRUE(arangodb::tests::assertRules(
          _vocbase, query,
          {arangodb::aql::OptimizerRule::handleArangoSearchViewsRule},
          arangodb::velocypack::Parser::fromJson(
              "{ \"collections\" : [ \"collection_1\" ] }")));

      std::map<std::string_view,
               std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>>>
          expectedDocs{{"A", _insertedDocs[0]}};

      auto queryResult = arangodb::tests::executeQuery(
          _vocbase, query,
          arangodb::velocypack::Parser::fromJson(
              "{ \"collections\" : [ \"collection_1\" ] }"));
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(expectedDocs.size(), resultIt.size());

      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        auto const keySlice = resolved.get("name");
        auto const key = arangodb::iresearch::getStringRef(keySlice);

        auto expectedDoc = expectedDocs.find(key);
        ASSERT_NE(expectedDoc, expectedDocs.end());
        EXPECT_TRUE(
            0 == arangodb::basics::VelocyPackHelper::compare(
                     arangodb::velocypack::Slice(expectedDoc->second->data()),
                     resolved, true));
        expectedDocs.erase(expectedDoc);
      }
      EXPECT_TRUE(expectedDocs.empty());
    }

    // collection by id
    {
      std::string const query =
          "FOR d IN testView SEARCH d.name == 'A'"
          " OPTIONS { collections : [ " +
          std::to_string(logicalCollection2->id().id()) +
          " ] }"
          " RETURN d";

      EXPECT_TRUE(arangodb::tests::assertRules(
          _vocbase, query,
          {arangodb::aql::OptimizerRule::handleArangoSearchViewsRule}));

      std::map<std::string_view,
               std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>>>
          expectedDocs{{"A", _insertedDocs[1]}};

      // check node estimation
      {
        auto explanationResult = arangodb::tests::explainQuery(_vocbase, query);
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
        ASSERT_EQ(_insertedDocs.size() / 2 + 1.,
                  viewNode.get("estimatedCost").getDouble());
        ASSERT_EQ(_insertedDocs.size() / 2,
                  viewNode.get("estimatedNrItems").getNumber<size_t>());
      }

      auto queryResult = arangodb::tests::executeQuery(_vocbase, query);
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(expectedDocs.size(), resultIt.size());

      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        auto const keySlice = resolved.get("name");
        auto const key = arangodb::iresearch::getStringRef(keySlice);

        auto expectedDoc = expectedDocs.find(key);
        ASSERT_NE(expectedDoc, expectedDocs.end());
        EXPECT_TRUE(
            0 == arangodb::basics::VelocyPackHelper::compare(
                     arangodb::velocypack::Slice(expectedDoc->second->data()),
                     resolved, true));
        expectedDocs.erase(expectedDoc);
      }
      EXPECT_TRUE(expectedDocs.empty());
    }

    // collection by id as string
    {
      std::string const query =
          "FOR d IN testView SEARCH d.name == 'A'"
          " OPTIONS { collections : [ '" +
          std::to_string(logicalCollection2->id().id()) +
          "' ] }"
          " RETURN d";

      EXPECT_TRUE(arangodb::tests::assertRules(
          _vocbase, query,
          {arangodb::aql::OptimizerRule::handleArangoSearchViewsRule}));

      std::map<std::string_view,
               std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>>>
          expectedDocs{{"A", _insertedDocs[1]}};

      auto queryResult = arangodb::tests::executeQuery(_vocbase, query);
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(expectedDocs.size(), resultIt.size());

      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        auto const keySlice = resolved.get("name");
        auto const key = arangodb::iresearch::getStringRef(keySlice);

        auto expectedDoc = expectedDocs.find(key);
        ASSERT_NE(expectedDoc, expectedDocs.end());
        EXPECT_TRUE(
            0 == arangodb::basics::VelocyPackHelper::compare(
                     arangodb::velocypack::Slice(expectedDoc->second->data()),
                     resolved, true));
        expectedDocs.erase(expectedDoc);
      }
      EXPECT_TRUE(expectedDocs.empty());
    }

    // multiple collections
    {
      std::string const query =
          "FOR d IN testView SEARCH d.name == 'A'"
          " OPTIONS { collections : [ '" +
          std::to_string(logicalCollection2->id().id()) +
          "', 'collection_1' ] }"
          " SORT d._id"
          " RETURN d";

      EXPECT_TRUE(arangodb::tests::assertRules(
          _vocbase, query,
          {arangodb::aql::OptimizerRule::handleArangoSearchViewsRule}));

      std::map<
          std::string_view,
          std::vector<std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>>>>
          expectedDocs{{"A", {_insertedDocs[0], _insertedDocs[1]}}};

      auto queryResult = arangodb::tests::executeQuery(_vocbase, query);
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(2U, resultIt.size());

      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        auto const keySlice = resolved.get("name");
        auto const key = arangodb::iresearch::getStringRef(keySlice);

        auto it = expectedDocs.find(key);
        ASSERT_NE(it, expectedDocs.end());
        ASSERT_FALSE(it->second.empty());
        auto expectedDoc = it->second.begin();
        ASSERT_NE(expectedDoc, it->second.end());
        EXPECT_TRUE(0 ==
                    arangodb::basics::VelocyPackHelper::compare(
                        arangodb::velocypack::Slice((**expectedDoc).data()),
                        resolved, true));
        it->second.erase(expectedDoc);

        if (it->second.empty()) {
          expectedDocs.erase(it);
        }
      }
      EXPECT_TRUE(expectedDocs.empty());
    }

    // empty array means no data
    {
      std::string const query =
          "FOR d IN testView SEARCH d.name == 'A' OPTIONS { collections : [ ] }"
          " RETURN d";

      EXPECT_TRUE(arangodb::tests::assertRules(
          _vocbase, query,
          {arangodb::aql::OptimizerRule::handleArangoSearchViewsRule}));

      auto queryResult = arangodb::tests::executeQuery(_vocbase, query);
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(0U, resultIt.size());
    }

    // null means "no restrictions"
    {
      std::string const query =
          "FOR d IN testView SEARCH d.name == 'A' OPTIONS { collections : null "
          "}"
          " SORT d._id"
          " RETURN d";

      EXPECT_TRUE(arangodb::tests::assertRules(
          _vocbase, query,
          {arangodb::aql::OptimizerRule::handleArangoSearchViewsRule}));

      // check node estimation
      {
        auto explanationResult = arangodb::tests::explainQuery(_vocbase, query);
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
        ASSERT_EQ(_insertedDocs.size() + 1.,
                  viewNode.get("estimatedCost").getDouble());
        ASSERT_EQ(_insertedDocs.size(),
                  viewNode.get("estimatedNrItems").getNumber<size_t>());
      }

      std::map<
          std::string_view,
          std::vector<std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>>>>
          expectedDocs{{"A", {_insertedDocs[0], _insertedDocs[1]}}};

      auto queryResult = arangodb::tests::executeQuery(_vocbase, query);
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(2U, resultIt.size());

      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        auto const keySlice = resolved.get("name");
        auto const key = arangodb::iresearch::getStringRef(keySlice);

        auto it = expectedDocs.find(key);
        ASSERT_NE(it, expectedDocs.end());
        ASSERT_FALSE(it->second.empty());
        auto expectedDoc = it->second.begin();
        ASSERT_NE(expectedDoc, it->second.end());
        EXPECT_TRUE(0 ==
                    arangodb::basics::VelocyPackHelper::compare(
                        arangodb::velocypack::Slice((**expectedDoc).data()),
                        resolved, true));
        it->second.erase(expectedDoc);

        if (it->second.empty()) {
          expectedDocs.erase(it);
        }
      }
      EXPECT_TRUE(expectedDocs.empty());
    }

    // join restricted views
    {
      std::string const query =
          "FOR d IN testView SEARCH d.name == 'A' OPTIONS { collections : [ "
          "'collection_1' ] }"
          " FOR x IN testView SEARCH x.name == 'A' OPTIONS { collections : [ "
          "'collection_2' ] }"
          " RETURN { d, x }";

      EXPECT_TRUE(arangodb::tests::assertRules(
          _vocbase, query,
          {arangodb::aql::OptimizerRule::handleArangoSearchViewsRule}));

      std::map<
          std::string_view,
          std::vector<std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>>>>
          expectedDocs{{"A", {_insertedDocs[0], _insertedDocs[1]}}};

      auto queryResult = arangodb::tests::executeQuery(_vocbase, query);
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(expectedDocs.size(), resultIt.size());

      for (auto const actualValue : resultIt) {
        EXPECT_TRUE(actualValue.isObject());

        auto const d = actualValue.get("d");
        EXPECT_TRUE(d.isObject());
        auto const resolvedd = d.resolveExternal();
        auto const x = actualValue.get("x");
        EXPECT_TRUE(x.isObject());
        auto const resolvedx = x.resolveExternal();

        auto const keySliced = resolvedx.get("name");
        auto const keyd = arangodb::iresearch::getStringRef(keySliced);
        auto const keySlicex = resolvedx.get("name");
        auto const keyx = arangodb::iresearch::getStringRef(keySlicex);
        EXPECT_EQ(keyd, keyx);

        auto it = expectedDocs.find(keyd);
        ASSERT_NE(it, expectedDocs.end());
        ASSERT_EQ(2U, it->second.size());
        ASSERT_TRUE(0 == arangodb::basics::VelocyPackHelper::compare(
                             arangodb::velocypack::Slice(it->second[0]->data()),
                             resolvedd, true));
        ASSERT_TRUE(0 == arangodb::basics::VelocyPackHelper::compare(
                             arangodb::velocypack::Slice(it->second[1]->data()),
                             resolvedx, true));

        expectedDocs.erase(it);
      }
      EXPECT_TRUE(expectedDocs.empty());
    }

    // wrong collection name is specified
    {
      std::string const query =
          "FOR d IN testView SEARCH d.name == 'A' OPTIONS { collections : [ "
          "'collection_1', 'collection_0' ] }"
          " SORT d._id"
          " RETURN d";

      auto queryResult = arangodb::tests::executeQuery(_vocbase, query);
      ASSERT_TRUE(queryResult.result.is(TRI_ERROR_BAD_PARAMETER));
    }

    // wrong collection id is specified
    {
      std::string const query =
          "FOR d IN testView SEARCH d.name == 'A' OPTIONS { collections : [ "
          "'collection_1', 32112312 ] }"
          " SORT d._id"
          " RETURN d";

      auto queryResult = arangodb::tests::executeQuery(_vocbase, query);
      ASSERT_TRUE(queryResult.result.is(TRI_ERROR_BAD_PARAMETER));
    }

    // wrong collection id as string is specified
    {
      std::string const query =
          "FOR d IN testView SEARCH d.name == 'A' OPTIONS { collections : [ "
          "'collection_1', '32112312' ] }"
          " SORT d._id"
          " RETURN d";

      auto queryResult = arangodb::tests::executeQuery(_vocbase, query);
      ASSERT_TRUE(queryResult.result.is(TRI_ERROR_BAD_PARAMETER));
    }

    // invalid option type
    {
      std::string const query =
          "FOR d IN testView SEARCH d.name == 'A' OPTIONS { collections : [ "
          "'collection_1', null ] }"
          " SORT d._id"
          " RETURN d";

      auto queryResult = arangodb::tests::executeQuery(_vocbase, query);
      ASSERT_TRUE(queryResult.result.is(TRI_ERROR_BAD_PARAMETER));
    }

    // invalid option type
    {
      std::string const query =
          "FOR d IN testView SEARCH d.name == 'A' OPTIONS { collections : [ "
          "'collection_1', {} ] }"
          " SORT d._id"
          " RETURN d";

      auto queryResult = arangodb::tests::executeQuery(_vocbase, query);
      ASSERT_TRUE(queryResult.result.is(TRI_ERROR_BAD_PARAMETER));
    }

    // invalid option type
    {
      std::string const query =
          "FOR d IN testView SEARCH d.name == 'A' OPTIONS { collections : [ "
          "'collection_1', true ] }"
          " SORT d._id"
          " RETURN d";

      auto queryResult = arangodb::tests::executeQuery(_vocbase, query);
      ASSERT_TRUE(queryResult.result.is(TRI_ERROR_BAD_PARAMETER));
    }

    // invalid option type
    {
      std::string const query =
          "FOR d IN testView SEARCH d.name == 'A' OPTIONS { collections : [ "
          "'collection_1', null ] }"
          " SORT d._id"
          " RETURN d";

      auto queryResult = arangodb::tests::executeQuery(_vocbase, query);
      ASSERT_TRUE(queryResult.result.is(TRI_ERROR_BAD_PARAMETER));
    }

    // invalid options type
    {
      std::string const query =
          "FOR d IN testView SEARCH d.name == 'A' OPTIONS { collections : true "
          "}"
          " SORT d._id"
          " RETURN d";

      auto queryResult = arangodb::tests::executeQuery(_vocbase, query);
      ASSERT_TRUE(queryResult.result.is(TRI_ERROR_BAD_PARAMETER));
    }

    // invalid options type
    {
      std::string const query =
          "FOR d IN testView SEARCH d.name == 'A' OPTIONS { collections : 1 }"
          " SORT d._id"
          " RETURN d";

      auto queryResult = arangodb::tests::executeQuery(_vocbase, query);
      ASSERT_TRUE(queryResult.result.is(TRI_ERROR_BAD_PARAMETER));
    }

    // invalid options type
    {
      std::string const query =
          "FOR d IN testView SEARCH d.name == 'A' OPTIONS { collections : {} }"
          " SORT d._id"
          " RETURN d";

      auto queryResult = arangodb::tests::executeQuery(_vocbase, query);
      ASSERT_TRUE(queryResult.result.is(TRI_ERROR_BAD_PARAMETER));
    }

    // collection which is not registered witht a view is specified
    {
      std::string const query =
          "FOR d IN testView SEARCH d.name == 'A' OPTIONS { collections : [ "
          "'collection_1', 'collection_3' ] }"
          " SORT d._id"
          " RETURN d";

      auto queryResult = arangodb::tests::executeQuery(_vocbase, query);
      ASSERT_TRUE(queryResult.result.is(TRI_ERROR_BAD_PARAMETER));
    }
  }

  void queryTestWaitForSync() {
    // ==, !=, <, <=, >, >=, range

    // -----------------------------------------------------------------------------
    // --SECTION--                                              'waitForSync'
    // option
    // -----------------------------------------------------------------------------

    // wrong option type is specified
    {
      std::string const query =
          "FOR d IN testView SEARCH d.name == 'A' OPTIONS { waitForSync: null }"
          " SORT d._id"
          " RETURN d";

      auto queryResult = arangodb::tests::executeQuery(_vocbase, query);
      ASSERT_TRUE(queryResult.result.is(TRI_ERROR_BAD_PARAMETER));
    }

    // wrong option type is specified
    {
      std::string const query =
          "FOR d IN testView SEARCH d.name == 'A' OPTIONS { waitForSync: 1 }"
          " SORT d._id"
          " RETURN d";

      auto queryResult = arangodb::tests::executeQuery(_vocbase, query);
      ASSERT_TRUE(queryResult.result.is(TRI_ERROR_BAD_PARAMETER));
    }

    // wrong option type is specified
    {
      std::string const query =
          "FOR d IN testView SEARCH d.name == 'A' OPTIONS { waitForSync: "
          "'true' }"
          " SORT d._id"
          " RETURN d";

      auto queryResult = arangodb::tests::executeQuery(_vocbase, query);
      ASSERT_TRUE(queryResult.result.is(TRI_ERROR_BAD_PARAMETER));
    }

    // wrong option type is specified
    {
      std::string const query =
          "FOR d IN testView SEARCH d.name == 'A' OPTIONS { waitForSync: [] }"
          " SORT d._id"
          " RETURN d";

      auto queryResult = arangodb::tests::executeQuery(_vocbase, query);
      ASSERT_TRUE(queryResult.result.is(TRI_ERROR_BAD_PARAMETER));
    }

    // wrong option type is specified
    {
      std::string const query =
          "FOR d IN testView SEARCH d.name == 'A' OPTIONS { waitForSync: {} }"
          " SORT d._id"
          " RETURN d";

      auto queryResult = arangodb::tests::executeQuery(_vocbase, query);
      ASSERT_TRUE(queryResult.result.is(TRI_ERROR_BAD_PARAMETER));
    }

    // don't sync
    {
      std::string const query =
          "FOR d IN testView SEARCH d.name == 'A' OPTIONS { waitForSync: false "
          "}"
          " RETURN d";

      EXPECT_TRUE(arangodb::tests::assertRules(
          _vocbase, query,
          {arangodb::aql::OptimizerRule::handleArangoSearchViewsRule}));

      auto queryResult = arangodb::tests::executeQuery(_vocbase, query);
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(0U, resultIt.size());
    }

    // do sync, bind parameter
    {
      std::string const query =
          "FOR d IN testView SEARCH d.name == 'A' OPTIONS { waitForSync: "
          "@doSync "
          "}"
          " RETURN d";

      EXPECT_TRUE(arangodb::tests::assertRules(
          _vocbase, query,
          {arangodb::aql::OptimizerRule::handleArangoSearchViewsRule},
          arangodb::velocypack::Parser::fromJson("{ \"doSync\" : true }")));

      std::map<std::string_view,
               std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>>>
          expectedDocs{{"A", _insertedDocs[0]}};

      auto queryResult = arangodb::tests::executeQuery(
          _vocbase, query,
          arangodb::velocypack::Parser::fromJson("{ \"doSync\" : true }"));
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(expectedDocs.size(), resultIt.size());

      for (auto const actualDoc : resultIt) {
        auto const resolved = actualDoc.resolveExternals();
        auto const keySlice = resolved.get("name");
        auto const key = arangodb::iresearch::getStringRef(keySlice);

        auto expectedDoc = expectedDocs.find(key);
        ASSERT_NE(expectedDoc, expectedDocs.end());
        EXPECT_TRUE(
            0 == arangodb::basics::VelocyPackHelper::compare(
                     arangodb::velocypack::Slice(expectedDoc->second->data()),
                     resolved, true));
        expectedDocs.erase(expectedDoc);
      }
      EXPECT_TRUE(expectedDocs.empty());
    }
  }

  void queryTestNoMaterialization() {
    // -----------------------------------------------------------------------------
    // --SECTION--                                        'noMaterialization'
    // option
    // -----------------------------------------------------------------------------

    // wrong option type is specified
    {
      std::string const query =
          "FOR d IN testView SEARCH d.str == 'cat' OPTIONS { "
          "noMaterialization: "
          "null }"
          " SORT d._id"
          " RETURN d.value";

      auto queryResult = arangodb::tests::executeQuery(_vocbase, query);
      ASSERT_TRUE(queryResult.result.is(TRI_ERROR_BAD_PARAMETER));
    }

    // wrong option type is specified
    {
      std::string const query =
          "FOR d IN testView SEARCH d.str == 'cat' OPTIONS { "
          "noMaterialization: "
          "1 }"
          " SORT d._id"
          " RETURN d.value";

      auto queryResult = arangodb::tests::executeQuery(_vocbase, query);
      ASSERT_TRUE(queryResult.result.is(TRI_ERROR_BAD_PARAMETER));
    }

    // wrong option type is specified
    {
      std::string const query =
          "FOR d IN testView SEARCH d.str == 'cat' OPTIONS { "
          "noMaterialization: "
          "'true' }"
          " SORT d._id"
          " RETURN d.value";

      auto queryResult = arangodb::tests::executeQuery(_vocbase, query);
      ASSERT_TRUE(queryResult.result.is(TRI_ERROR_BAD_PARAMETER));
    }

    // wrong option type is specified
    {
      std::string const query =
          "FOR d IN testView SEARCH d.str == 'cat' OPTIONS { "
          "noMaterialization: "
          "[] }"
          " SORT d._id"
          " RETURN d.value";

      auto queryResult = arangodb::tests::executeQuery(_vocbase, query);
      ASSERT_TRUE(queryResult.result.is(TRI_ERROR_BAD_PARAMETER));
    }

    // wrong option type is specified
    {
      std::string const query =
          "FOR d IN testView SEARCH d.str == 'cat' OPTIONS { "
          "noMaterialization: "
          "{} }"
          " SORT d._id"
          " RETURN d.value";

      auto queryResult = arangodb::tests::executeQuery(_vocbase, query);
      ASSERT_TRUE(queryResult.result.is(TRI_ERROR_BAD_PARAMETER));
    }

    // do not materialize
    {
      std::string const queryString =
          "FOR d IN testView SEARCH d.str == 'cat' OPTIONS { "
          "noMaterialization: "
          "true }"
          " RETURN d.value";

      EXPECT_TRUE(arangodb::tests::assertRules(
          _vocbase, queryString,
          {arangodb::aql::OptimizerRule::handleArangoSearchViewsRule}));

      auto query = arangodb::aql::Query::create(
          arangodb::transaction::StandaloneContext::create(
              _vocbase, arangodb::transaction::OperationOriginTestCase{}),
          arangodb::aql::QueryString(queryString), nullptr);
      auto const res = query->explain();
      ASSERT_TRUE(res.data);
      auto const explanation = res.data->slice();
      arangodb::velocypack::ArrayIterator nodes(explanation.get("nodes"));
      auto found = false;
      for (auto const node : nodes) {
        if (node.hasKey("type") && node.get("type").isString() &&
            node.get("type").stringView() == "EnumerateViewNode") {
          EXPECT_TRUE(node.hasKey("noMaterialization") &&
                      node.get("noMaterialization").isBool() &&
                      node.get("noMaterialization").getBool());
          found = true;
          break;
        }
      }
      EXPECT_TRUE(found);

      auto queryResult = arangodb::tests::executeQuery(_vocbase, queryString);
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(8, resultIt.size());
    }

    // materialize
    {
      std::string const queryString =
          "FOR d IN testView SEARCH d.str == 'cat' OPTIONS { "
          "noMaterialization: "
          "false }"
          " RETURN d.value";

      EXPECT_TRUE(arangodb::tests::assertRules(
          _vocbase, queryString,
          {arangodb::aql::OptimizerRule::handleArangoSearchViewsRule}));

      auto query = arangodb::aql::Query::create(
          arangodb::transaction::StandaloneContext::create(
              _vocbase, arangodb::transaction::OperationOriginTestCase{}),
          arangodb::aql::QueryString(queryString), nullptr);
      auto const res = query->explain();
      ASSERT_TRUE(res.data);
      auto const explanation = res.data->slice();
      arangodb::velocypack::ArrayIterator nodes(explanation.get("nodes"));
      auto found = false;
      for (auto const node : nodes) {
        if (node.get("type").isString() &&
            node.get("type").stringView() == "EnumerateViewNode") {
          EXPECT_FALSE(node.hasKey("noMaterialization"));
          found = true;
          break;
        }
      }
      EXPECT_TRUE(found);

      auto queryResult = arangodb::tests::executeQuery(_vocbase, queryString);
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      arangodb::velocypack::ArrayIterator resultIt(result);
      EXPECT_EQ(8, resultIt.size());
    }
  }
};

class QueryOptionsView : public QueryOptions {
 protected:
  ViewType type() const final { return arangodb::ViewType::kArangoSearch; }

  void createView(bool storedValues) {
    auto createJson = arangodb::velocypack::Parser::fromJson(absl::Substitute(
        "{ \"name\": \"testView\", $0 \"type\": \"arangosearch\" }",
        (storedValues
             ? R"("storedValues": [{"fields":["str"]}, {"fields":["value"]}, {"fields":["_id"]}],)"
             : "")));

    // add view
    auto view = std::dynamic_pointer_cast<arangodb::iresearch::IResearchView>(
        _vocbase.createView(createJson->slice(), false));
    ASSERT_FALSE(!view);

    // add link to collection
    {
      auto versionStr = std::to_string(static_cast<uint32_t>(linkVersion()));

      auto updateJson = arangodb::velocypack::Parser::fromJson(
          "{ \"links\" : {"
          "\"collection_1\" : { \"includeAllFields\" : true, \"version\": " +
          versionStr +
          " },"
          "\"collection_2\" : { \"includeAllFields\" : true, \"version\": " +
          versionStr +
          " }"
          "}}");
      EXPECT_TRUE(view->properties(updateJson->slice(), true, true).ok());

      arangodb::velocypack::Builder builder;

      builder.openObject();
      view->properties(builder,
                       arangodb::LogicalDataSource::Serialization::Properties);
      builder.close();

      auto slice = builder.slice();
      EXPECT_TRUE(slice.isObject());
      EXPECT_EQ(slice.get("name").copyString(), "testView");
      EXPECT_TRUE(slice.get("type").copyString() ==
                  arangodb::iresearch::StaticStrings::ViewArangoSearchType);
      EXPECT_TRUE(slice.get("deleted").isNone());  // no system properties
      auto tmpSlice = slice.get("links");
      EXPECT_TRUE(tmpSlice.isObject() && 2 == tmpSlice.length());
    }
  }
};

class QueryOptionsSearch : public QueryOptions {
 protected:
  ViewType type() const final { return arangodb::ViewType::kSearchAlias; }
  void createSearch(bool storedValues) {
    // create indexes
    auto createIndex = [&](int name) {
      bool created = false;
      auto createJson = VPackParser::fromJson(absl::Substitute(
          R"({ "name": "index_$0", "type": "inverted",
               "version": $1, $2
               "includeAllFields": true })",
          name, version(),
          (storedValues
               ? R"("storedValues": [{"fields":["str"]}, {"fields":["value"]}, {"fields":["_id"]}],)"
               : "")));
      auto collection =
          _vocbase.lookupCollection(absl::Substitute("collection_$0", name));
      ASSERT_TRUE(collection);
      collection->createIndex(createJson->slice(), created).waitAndGet();
      ASSERT_TRUE(created);
    };
    createIndex(1);
    createIndex(2);

    // add view
    auto createJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testView\", \"type\": \"search-alias\" }");

    auto view = std::dynamic_pointer_cast<arangodb::iresearch::Search>(
        _vocbase.createView(createJson->slice(), false));
    ASSERT_FALSE(!view);

    // add link to collection
    {
      auto const viewDefinition = R"({
        "indexes": [
          { "collection": "collection_1", "index": "index_1"},
          { "collection": "collection_2", "index": "index_2"}
        ]})";
      auto updateJson = arangodb::velocypack::Parser::fromJson(viewDefinition);
      auto r = view->properties(updateJson->slice(), true, true);
      EXPECT_TRUE(r.ok()) << r.errorMessage();
    }
  }
};

TEST_P(QueryOptionsView, TestCollections) {
  create();
  createView(false);
  populateData0();
  queryTestCollections();
}

TEST_P(QueryOptionsSearch, TestCollections) {
  create();
  createSearch(false);
  populateData0();
  queryTestCollections();
}

TEST_P(QueryOptionsView, TestWaitForSync) {
  create();
  createView(false);
  populateData1();
  queryTestWaitForSync();
}

TEST_P(QueryOptionsSearch, TestWaitForSync) {
  create();
  createSearch(false);
  populateData1();
  queryTestWaitForSync();
}

TEST_P(QueryOptionsView, TestNoMaterialization) {
  create();
  createView(true);
  populateData2();
  queryTestNoMaterialization();
}

TEST_P(QueryOptionsSearch, TestNoMaterialization) {
  create();
  createSearch(true);
  populateData2();
  queryTestNoMaterialization();
}

INSTANTIATE_TEST_CASE_P(IResearch, QueryOptionsView, GetLinkVersions());

INSTANTIATE_TEST_CASE_P(IResearch, QueryOptionsSearch, GetIndexVersions());

}  // namespace
}  // namespace arangodb::tests
