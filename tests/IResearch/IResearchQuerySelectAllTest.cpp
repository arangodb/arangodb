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

class QuerySelectAll : public QueryTest {
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
  }

  void populateData() {
    _insertedDocs =
        std::deque<std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>>>(101);
    auto logicalCollection1 = _vocbase.lookupCollection("collection_1");
    ASSERT_TRUE(logicalCollection1);
    auto logicalCollection2 = _vocbase.lookupCollection("collection_2");
    ASSERT_TRUE(logicalCollection2);
    arangodb::OperationOptions opt;

    arangodb::transaction::Methods trx(
        arangodb::transaction::StandaloneContext::create(
            _vocbase, arangodb::transaction::OperationOriginTestCase{}),
        kEmpty, {logicalCollection1->name()}, kEmpty,
        arangodb::transaction::Options());
    EXPECT_TRUE(trx.begin().ok());

    size_t i = 0;

    // insert into collection_1
    for (; i < _insertedDocs.size() / 2; ++i) {
      auto const doc = arangodb::velocypack::Parser::fromJson(
          "{ \"key\": " + std::to_string(i) + "}");
      auto res = trx.insert(logicalCollection1->name(), doc->slice(), opt);
      EXPECT_TRUE(res.ok());

      res = trx.document(logicalCollection1->name(), res.slice(), opt);
      EXPECT_TRUE(res.ok());
      _insertedDocs[i] = std::move(res.buffer);
    }

    // insert into collection_2 (actually, this is also collection 1...)
    for (; i < _insertedDocs.size(); ++i) {
      auto const doc = arangodb::velocypack::Parser::fromJson(
          "{ \"key\": " + std::to_string(i) + "}");
      auto res = trx.insert(logicalCollection1->name(), doc->slice(), opt);
      EXPECT_TRUE(res.ok());

      res = trx.document(logicalCollection1->name(), res.slice(), opt);
      EXPECT_TRUE(res.ok());
      _insertedDocs[i] = std::move(res.buffer);
    }

    EXPECT_TRUE(trx.commit().ok());
    EXPECT_TRUE(
        (arangodb::tests::executeQuery(_vocbase,
                                       "FOR d IN testView SEARCH 1 ==1 OPTIONS "
                                       "{ waitForSync: true } RETURN d")
             .result.ok()));  // commit
  }

  void queryTests() {
    // unordered
    {
      std::map<size_t, std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>>>
          expectedDocs;
      for (auto const& doc : _insertedDocs) {
        arangodb::velocypack::Slice docSlice(doc->data());
        auto keySlice = docSlice.get("key");
        expectedDocs.emplace(keySlice.getNumber<size_t>(), doc);
      }

      std::string const queryString = "FOR d IN testView RETURN d";

      // check node estimation
      {
        auto explanationResult =
            arangodb::tests::explainQuery(_vocbase, queryString);
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

      auto queryResult = arangodb::tests::executeQuery(_vocbase, queryString);
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      for (auto const actualDoc : arangodb::velocypack::ArrayIterator(result)) {
        auto const resolved = actualDoc.resolveExternals();
        auto const keySlice = resolved.get("key");
        auto const key = keySlice.getNumber<size_t>();

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

    // key ASC
    {
      auto const& expectedDocs = _insertedDocs;

      auto queryResult = arangodb::tests::executeQuery(
          _vocbase, "FOR d IN testView SORT d.key ASC RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      auto expectedDoc = expectedDocs.begin();
      for (auto const actualDoc : arangodb::velocypack::ArrayIterator(result)) {
        auto const resolved = actualDoc.resolveExternals();
        EXPECT_TRUE(0 ==
                    arangodb::basics::VelocyPackHelper::compare(
                        arangodb::velocypack::Slice((*expectedDoc)->data()),
                        resolved, true));
        ++expectedDoc;
      }
      EXPECT_EQ(expectedDoc, expectedDocs.end());
    }

    // key DESC
    {
      auto const& expectedDocs = _insertedDocs;

      auto queryResult = arangodb::tests::executeQuery(
          _vocbase, "FOR d IN testView SORT d.key DESC RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      auto expectedDoc = expectedDocs.rbegin();
      for (auto const actualDoc : arangodb::velocypack::ArrayIterator(result)) {
        auto const resolved = actualDoc.resolveExternals();
        EXPECT_TRUE(0 ==
                    arangodb::basics::VelocyPackHelper::compare(
                        arangodb::velocypack::Slice((*expectedDoc)->data()),
                        resolved, true));
        ++expectedDoc;
      }
      EXPECT_EQ(expectedDoc, expectedDocs.rend());
    }

    // TFIDF() ASC
    {
      std::map<size_t, std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>>>
          expectedDocs;
      for (auto const& doc : _insertedDocs) {
        arangodb::velocypack::Slice docSlice(doc->data());
        auto keySlice = docSlice.get("key");
        expectedDocs.emplace(keySlice.getNumber<size_t>(), doc);
      }

      auto queryResult = arangodb::tests::executeQuery(
          _vocbase, "FOR d IN testView SORT TFIDF(d) RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      for (auto const actualDoc : arangodb::velocypack::ArrayIterator(result)) {
        auto const resolved = actualDoc.resolveExternals();
        auto const keySlice = resolved.get("key");
        auto const key = keySlice.getNumber<size_t>();

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

    // TFIDF() DESC
    {
      std::map<size_t, std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>>>
          expectedDocs;
      for (auto const& doc : _insertedDocs) {
        arangodb::velocypack::Slice docSlice(doc->data());
        auto keySlice = docSlice.get("key");
        expectedDocs.emplace(keySlice.getNumber<size_t>(), doc);
      }

      auto queryResult = arangodb::tests::executeQuery(
          _vocbase, "FOR d IN testView SORT TFIDF(d) DESC RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      for (auto const actualDoc : arangodb::velocypack::ArrayIterator(result)) {
        auto const resolved = actualDoc.resolveExternals();
        auto const keySlice = resolved.get("key");
        auto const key = keySlice.getNumber<size_t>();

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

    // BM25() ASC
    {
      std::map<size_t, std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>>>
          expectedDocs;
      for (auto const& doc : _insertedDocs) {
        arangodb::velocypack::Slice docSlice(doc->data());
        auto keySlice = docSlice.get("key");
        expectedDocs.emplace(keySlice.getNumber<size_t>(), doc);
      }

      auto queryResult = arangodb::tests::executeQuery(
          _vocbase, "FOR d IN testView SORT BM25(d) RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      for (auto const actualDoc : arangodb::velocypack::ArrayIterator(result)) {
        auto const resolved = actualDoc.resolveExternals();
        auto const keySlice = resolved.get("key");
        auto const key = keySlice.getNumber<size_t>();

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

    // BM25() DESC
    {
      std::map<size_t, std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>>>
          expectedDocs;
      for (auto const& doc : _insertedDocs) {
        arangodb::velocypack::Slice docSlice(doc->data());
        auto keySlice = docSlice.get("key");
        expectedDocs.emplace(keySlice.getNumber<size_t>(), doc);
      }

      auto queryResult = arangodb::tests::executeQuery(
          _vocbase, "FOR d IN testView SORT BM25(d) DESC RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      for (auto const actualDoc : arangodb::velocypack::ArrayIterator(result)) {
        auto const resolved = actualDoc.resolveExternals();
        auto const keySlice = resolved.get("key");
        auto const key = keySlice.getNumber<size_t>();

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

    // TFIDF() ASC, key ASC
    {
      auto const& expectedDocs = _insertedDocs;

      auto queryResult = arangodb::tests::executeQuery(
          _vocbase, "FOR d IN testView SORT TFIDF(d), d.key ASC RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      auto expectedDoc = expectedDocs.begin();
      for (auto const actualDoc : arangodb::velocypack::ArrayIterator(result)) {
        auto const resolved = actualDoc.resolveExternals();
        EXPECT_TRUE(0 ==
                    arangodb::basics::VelocyPackHelper::compare(
                        arangodb::velocypack::Slice((*expectedDoc)->data()),
                        resolved, true));
        ++expectedDoc;
      }
      EXPECT_EQ(expectedDoc, expectedDocs.end());
    }

    // TFIDF ASC, key DESC
    {
      auto const& expectedDocs = _insertedDocs;

      auto queryResult = arangodb::tests::executeQuery(
          _vocbase, "FOR d IN testView SORT TFIDF(d), d.key DESC RETURN d");
      ASSERT_TRUE(queryResult.result.ok());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      auto expectedDoc = expectedDocs.rbegin();
      for (auto const actualDoc : arangodb::velocypack::ArrayIterator(result)) {
        auto const resolved = actualDoc.resolveExternals();
        EXPECT_TRUE(0 ==
                    arangodb::basics::VelocyPackHelper::compare(
                        arangodb::velocypack::Slice((*expectedDoc)->data()),
                        resolved, true));
        ++expectedDoc;
      }
      EXPECT_EQ(expectedDoc, expectedDocs.rend());
    }

    // check full stats with optimization
    {
      auto const queryString =
          "FOR d IN testView SORT BM25(d), d.key DESC LIMIT 10, 10 RETURN d";
      auto const& expectedDocs = _insertedDocs;

      EXPECT_TRUE(arangodb::tests::assertRules(
          _vocbase, queryString,
          {arangodb::aql::OptimizerRule::handleArangoSearchViewsRule,
           arangodb::aql::OptimizerRule::applySortLimitRule}));

      auto queryResult = arangodb::tests::executeQuery(
          _vocbase, queryString, {},
          "{ \"optimizer\" : { \"rules\": [ \"+sort-limit\"] }, \"fullCount\": "
          "true }");
      ASSERT_TRUE(queryResult.result.ok());

      auto root = queryResult.extra->slice();
      ASSERT_TRUE(root.isObject());
      auto stats = root.get("stats");
      ASSERT_TRUE(stats.isObject());
      auto fullCountSlice = stats.get("fullCount");
      ASSERT_TRUE(fullCountSlice.isNumber());
      EXPECT_EQ(_insertedDocs.size(), fullCountSlice.getNumber<size_t>());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      auto expectedDoc = expectedDocs.rbegin() + 10;
      for (auto const actualDoc : arangodb::velocypack::ArrayIterator(result)) {
        auto const resolved = actualDoc.resolveExternals();
        EXPECT_TRUE(0 ==
                    arangodb::basics::VelocyPackHelper::compare(
                        arangodb::velocypack::Slice((*expectedDoc)->data()),
                        resolved, true));
        ++expectedDoc;
      }
      EXPECT_EQ(expectedDoc, expectedDocs.rbegin() + 20);
    }

    // check full stats without optimization
    {
      auto const queryString =
          "FOR d IN testView SORT BM25(d), d.key DESC LIMIT 10, 10 RETURN d";
      auto const& expectedDocs = _insertedDocs;

      auto queryResult = arangodb::tests::executeQuery(
          _vocbase, queryString, {},
          "{ \"optimizer\" : { \"rules\": [ \"-sort-limit\"] }, \"fullCount\": "
          "true }");
      ASSERT_TRUE(queryResult.result.ok());

      auto root = queryResult.extra->slice();
      ASSERT_TRUE(root.isObject());
      auto stats = root.get("stats");
      ASSERT_TRUE(stats.isObject());
      auto fullCountSlice = stats.get("fullCount");
      ASSERT_TRUE(fullCountSlice.isNumber());
      EXPECT_EQ(_insertedDocs.size(), fullCountSlice.getNumber<size_t>());

      auto result = queryResult.data->slice();
      EXPECT_TRUE(result.isArray());

      auto expectedDoc = expectedDocs.rbegin() + 10;
      for (auto const actualDoc : arangodb::velocypack::ArrayIterator(result)) {
        auto const resolved = actualDoc.resolveExternals();
        EXPECT_TRUE(0 ==
                    arangodb::basics::VelocyPackHelper::compare(
                        arangodb::velocypack::Slice((*expectedDoc)->data()),
                        resolved, true));
        ++expectedDoc;
      }
      EXPECT_EQ(expectedDoc, expectedDocs.rbegin() + 20);
    }
  }
};

class QuerySelectAllView : public QuerySelectAll {
 protected:
  ViewType type() const final { return arangodb::ViewType::kArangoSearch; }

  void createView() {
    auto createJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testView\", \"type\": \"arangosearch\"}");
    // add view
    auto view = std::dynamic_pointer_cast<arangodb::iresearch::IResearchView>(
        _vocbase.createView(createJson->slice(), false));
    ASSERT_FALSE(!view);
    // add link to collection
    {
      auto viewDefinitionTemplate = R"({
        "links": {
          "collection_1": {
            "includeAllFields": true,
            "version": $0 },
          "collection_2": {
            "version": $1,
            "includeAllFields": true }
      }})";

      auto viewDefinition = absl::Substitute(
          viewDefinitionTemplate, static_cast<uint32_t>(linkVersion()),
          static_cast<uint32_t>(linkVersion()));

      auto updateJson = arangodb::velocypack::Parser::fromJson(viewDefinition);
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

class QuerySelectAllSearch : public QuerySelectAll {
 protected:
  ViewType type() const final { return arangodb::ViewType::kSearchAlias; }

  void createSearch() {
    // create indexes
    auto createIndex = [this](int name) {
      bool created = false;
      auto createJson = VPackParser::fromJson(absl::Substitute(
          R"({ "name": "index_$0", "type": "inverted",
               "version": $1,
               "includeAllFields": true })",
          name, version()));
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

TEST_P(QuerySelectAllView, Test) {
  create();
  createView();
  populateData();
  queryTests();
}

TEST_P(QuerySelectAllSearch, Test) {
  create();
  createSearch();
  populateData();
  queryTests();
}

INSTANTIATE_TEST_CASE_P(IResearch, QuerySelectAllView, GetLinkVersions());

INSTANTIATE_TEST_CASE_P(IResearch, QuerySelectAllSearch, GetIndexVersions());

}  // namespace
}  // namespace arangodb::tests
