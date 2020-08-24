////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
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

#include "IResearchQueryCommon.h"

#include "Aql/OptimizerRulesFeature.h"
#include "IResearch/IResearchView.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/OperationOptions.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ManagedDocumentResult.h"

#include <velocypack/Iterator.h>

extern const char* ARGV0;  // defined in main.cpp

namespace {

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

class IResearchQuerySelectAllTest : public IResearchQueryTest {};

}  // namespace

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

TEST_F(IResearchQuerySelectAllTest, test) {
  static std::vector<std::string> const EMPTY;

  auto createJson = arangodb::velocypack::Parser::fromJson(
      "{ \
    \"name\": \"testView\", \
    \"type\": \"arangosearch\" \
  }");

  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
  std::shared_ptr<arangodb::LogicalCollection> logicalCollection1;
  std::shared_ptr<arangodb::LogicalCollection> logicalCollection2;

  // add collection_1
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"collection_1\" }");
    logicalCollection1 = vocbase.createCollection(collectionJson->slice());
    ASSERT_NE(nullptr, logicalCollection1);
  }

  // add collection_2
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"collection_2\" }");
    logicalCollection2 = vocbase.createCollection(collectionJson->slice());
    ASSERT_NE(nullptr, logicalCollection2);
  }

  // add view
  auto view = std::dynamic_pointer_cast<arangodb::iresearch::IResearchView>(
      vocbase.createView(createJson->slice()));
  ASSERT_FALSE(!view);

  // add link to collection
  {
    auto updateJson = arangodb::velocypack::Parser::fromJson(
        "{ \"links\" : {"
        "\"collection_1\" : { \"includeAllFields\" : true },"
        "\"collection_2\" : { \"includeAllFields\" : true }"
        "}}");
    EXPECT_TRUE(view->properties(updateJson->slice(), true).ok());

    arangodb::velocypack::Builder builder;

    builder.openObject();
    view->properties(builder, arangodb::LogicalDataSource::Serialization::Properties);
    builder.close();

    auto slice = builder.slice();
    EXPECT_TRUE(slice.isObject());
    EXPECT_EQ(slice.get("name").copyString(), "testView");
    EXPECT_TRUE(slice.get("type").copyString() ==
                arangodb::iresearch::DATA_SOURCE_TYPE.name());
    EXPECT_TRUE(slice.get("deleted").isNone());  // no system properties
    auto tmpSlice = slice.get("links");
    EXPECT_TRUE(tmpSlice.isObject() && 2 == tmpSlice.length());
  }

  // need more than 100 docs for constrained heap optimization to be applied
  std::vector<arangodb::ManagedDocumentResult> insertedDocs(101);

  // populate view with the data
  {
    arangodb::OperationOptions opt;

    arangodb::transaction::Methods trx(arangodb::transaction::StandaloneContext::Create(vocbase),
                                       EMPTY, EMPTY, EMPTY,
                                       arangodb::transaction::Options());
    EXPECT_TRUE(trx.begin().ok());

    size_t i = 0;

    // insert into collection_1
    for (; i < insertedDocs.size() / 2; ++i) {
      auto const doc = arangodb::velocypack::Parser::fromJson(
          "{ \"key\": " + std::to_string(i) + "}");
      auto const res =
          logicalCollection1->insert(&trx, doc->slice(), insertedDocs[i], opt);
      EXPECT_TRUE(res.ok());
    }

    // insert into collection_2
    for (; i < insertedDocs.size(); ++i) {
      auto const doc = arangodb::velocypack::Parser::fromJson(
          "{ \"key\": " + std::to_string(i) + "}");
      auto const res =
          logicalCollection1->insert(&trx, doc->slice(), insertedDocs[i], opt);
      EXPECT_TRUE(res.ok());
    }

    EXPECT_TRUE(trx.commit().ok());
    EXPECT_TRUE(
        (arangodb::tests::executeQuery(vocbase,
                                       "FOR d IN testView SEARCH 1 ==1 OPTIONS "
                                       "{ waitForSync: true } RETURN d")
             .result.ok()));  // commit
  }

  // unordered
  {
    std::map<size_t, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto keySlice = docSlice.get("key");
      expectedDocs.emplace(keySlice.getNumber<size_t>(), &doc);
    }

    std::string const queryString = "FOR d IN testView RETURN d";

    // check node estimation
    {
      auto explanationResult =
          arangodb::tests::explainQuery(vocbase, queryString);
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
      ASSERT_EQ(insertedDocs.size() + 1., viewNode.get("estimatedCost").getDouble());
      ASSERT_EQ(insertedDocs.size(), viewNode.get("estimatedNrItems").getNumber<size_t>());
    }

    auto queryResult =
        arangodb::tests::executeQuery(vocbase, queryString);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    for (auto const actualDoc : arangodb::velocypack::ArrayIterator(result)) {
      auto const resolved = actualDoc.resolveExternals();
      auto const keySlice = resolved.get("key");
      auto const key = keySlice.getNumber<size_t>();

      auto expectedDoc = expectedDocs.find(key);
      ASSERT_NE(expectedDoc, expectedDocs.end());
      EXPECT_TRUE(0 == arangodb::basics::VelocyPackHelper::compare(
                           arangodb::velocypack::Slice(expectedDoc->second->vpack()),
                           resolved, true));
      expectedDocs.erase(expectedDoc);
    }
    EXPECT_TRUE(expectedDocs.empty());
  }

  // key ASC
  {
    auto const& expectedDocs = insertedDocs;

    auto queryResult = arangodb::tests::executeQuery(
        vocbase, "FOR d IN testView SORT d.key ASC RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    auto expectedDoc = expectedDocs.begin();
    for (auto const actualDoc : arangodb::velocypack::ArrayIterator(result)) {
      auto const resolved = actualDoc.resolveExternals();
      EXPECT_TRUE(0 == arangodb::basics::VelocyPackHelper::compare(
                           arangodb::velocypack::Slice(expectedDoc->vpack()), resolved, true));
      ++expectedDoc;
    }
    EXPECT_EQ(expectedDoc, expectedDocs.end());
  }

  // key DESC
  {
    auto const& expectedDocs = insertedDocs;

    auto queryResult = arangodb::tests::executeQuery(
        vocbase, "FOR d IN testView SORT d.key DESC RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    auto expectedDoc = expectedDocs.rbegin();
    for (auto const actualDoc : arangodb::velocypack::ArrayIterator(result)) {
      auto const resolved = actualDoc.resolveExternals();
      EXPECT_TRUE(0 == arangodb::basics::VelocyPackHelper::compare(
                           arangodb::velocypack::Slice(expectedDoc->vpack()), resolved, true));
      ++expectedDoc;
    }
    EXPECT_EQ(expectedDoc, expectedDocs.rend());
  }

  // TFIDF() ASC
  {
    std::map<size_t, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto keySlice = docSlice.get("key");
      expectedDocs.emplace(keySlice.getNumber<size_t>(), &doc);
    }

    auto queryResult = arangodb::tests::executeQuery(
        vocbase, "FOR d IN testView SORT TFIDF(d) RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    for (auto const actualDoc : arangodb::velocypack::ArrayIterator(result)) {
      auto const resolved = actualDoc.resolveExternals();
      auto const keySlice = resolved.get("key");
      auto const key = keySlice.getNumber<size_t>();

      auto expectedDoc = expectedDocs.find(key);
      ASSERT_NE(expectedDoc, expectedDocs.end());
      EXPECT_TRUE(0 == arangodb::basics::VelocyPackHelper::compare(
                           arangodb::velocypack::Slice(expectedDoc->second->vpack()),
                           resolved, true));
      expectedDocs.erase(expectedDoc);
    }
    EXPECT_TRUE(expectedDocs.empty());
  }

  // TFIDF() DESC
  {
    std::map<size_t, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto keySlice = docSlice.get("key");
      expectedDocs.emplace(keySlice.getNumber<size_t>(), &doc);
    }

    auto queryResult = arangodb::tests::executeQuery(
        vocbase, "FOR d IN testView SORT TFIDF(d) DESC RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    for (auto const actualDoc : arangodb::velocypack::ArrayIterator(result)) {
      auto const resolved = actualDoc.resolveExternals();
      auto const keySlice = resolved.get("key");
      auto const key = keySlice.getNumber<size_t>();

      auto expectedDoc = expectedDocs.find(key);
      ASSERT_NE(expectedDoc, expectedDocs.end());
      EXPECT_TRUE(0 == arangodb::basics::VelocyPackHelper::compare(
                           arangodb::velocypack::Slice(expectedDoc->second->vpack()),
                           resolved, true));
      expectedDocs.erase(expectedDoc);
    }
    EXPECT_TRUE(expectedDocs.empty());
  }

  // BM25() ASC
  {
    std::map<size_t, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto keySlice = docSlice.get("key");
      expectedDocs.emplace(keySlice.getNumber<size_t>(), &doc);
    }

    auto queryResult = arangodb::tests::executeQuery(
        vocbase, "FOR d IN testView SORT BM25(d) RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    for (auto const actualDoc : arangodb::velocypack::ArrayIterator(result)) {
      auto const resolved = actualDoc.resolveExternals();
      auto const keySlice = resolved.get("key");
      auto const key = keySlice.getNumber<size_t>();

      auto expectedDoc = expectedDocs.find(key);
      ASSERT_NE(expectedDoc, expectedDocs.end());
      EXPECT_TRUE(0 == arangodb::basics::VelocyPackHelper::compare(
                           arangodb::velocypack::Slice(expectedDoc->second->vpack()),
                           resolved, true));
      expectedDocs.erase(expectedDoc);
    }
    EXPECT_TRUE(expectedDocs.empty());
  }

  // BM25() DESC
  {
    std::map<size_t, arangodb::ManagedDocumentResult const*> expectedDocs;
    for (auto const& doc : insertedDocs) {
      arangodb::velocypack::Slice docSlice(doc.vpack());
      auto keySlice = docSlice.get("key");
      expectedDocs.emplace(keySlice.getNumber<size_t>(), &doc);
    }

    auto queryResult = arangodb::tests::executeQuery(
        vocbase, "FOR d IN testView SORT BM25(d) DESC RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    for (auto const actualDoc : arangodb::velocypack::ArrayIterator(result)) {
      auto const resolved = actualDoc.resolveExternals();
      auto const keySlice = resolved.get("key");
      auto const key = keySlice.getNumber<size_t>();

      auto expectedDoc = expectedDocs.find(key);
      ASSERT_NE(expectedDoc, expectedDocs.end());
      EXPECT_TRUE(0 == arangodb::basics::VelocyPackHelper::compare(
                           arangodb::velocypack::Slice(expectedDoc->second->vpack()),
                           resolved, true));
      expectedDocs.erase(expectedDoc);
    }
    EXPECT_TRUE(expectedDocs.empty());
  }

  // TFIDF() ASC, key ASC
  {
    auto const& expectedDocs = insertedDocs;

    auto queryResult = arangodb::tests::executeQuery(
        vocbase, "FOR d IN testView SORT TFIDF(d), d.key ASC RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    auto expectedDoc = expectedDocs.begin();
    for (auto const actualDoc : arangodb::velocypack::ArrayIterator(result)) {
      auto const resolved = actualDoc.resolveExternals();
      EXPECT_TRUE(0 == arangodb::basics::VelocyPackHelper::compare(
                           arangodb::velocypack::Slice(expectedDoc->vpack()), resolved, true));
      ++expectedDoc;
    }
    EXPECT_EQ(expectedDoc, expectedDocs.end());
  }

  // TFIDF ASC, key DESC
  {
    auto const& expectedDocs = insertedDocs;

    auto queryResult = arangodb::tests::executeQuery(
        vocbase, "FOR d IN testView SORT TFIDF(d), d.key DESC RETURN d");
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    auto expectedDoc = expectedDocs.rbegin();
    for (auto const actualDoc : arangodb::velocypack::ArrayIterator(result)) {
      auto const resolved = actualDoc.resolveExternals();
      EXPECT_TRUE(0 == arangodb::basics::VelocyPackHelper::compare(
                           arangodb::velocypack::Slice(expectedDoc->vpack()), resolved, true));
      ++expectedDoc;
    }
    EXPECT_EQ(expectedDoc, expectedDocs.rend());
  }

  // check full stats with optimization
  {
    auto const queryString =
        "FOR d IN testView SORT BM25(d), d.key DESC LIMIT 10, 10 RETURN d";
    auto const& expectedDocs = insertedDocs;

    EXPECT_TRUE(arangodb::tests::assertRules(vocbase, queryString,
                                             {arangodb::aql::OptimizerRule::handleArangoSearchViewsRule,
                                              arangodb::aql::OptimizerRule::applySortLimitRule}));

    auto queryResult = arangodb::tests::executeQuery(
        vocbase, queryString, {},
        "{ \"optimizer\" : { \"rules\": [ \"+sort-limit\"] }, \"fullCount\": "
        "true }");
    ASSERT_TRUE(queryResult.result.ok());

    auto root = queryResult.extra->slice();
    ASSERT_TRUE(root.isObject());
    auto stats = root.get("stats");
    ASSERT_TRUE(stats.isObject());
    auto fullCountSlice = stats.get("fullCount");
    ASSERT_TRUE(fullCountSlice.isNumber());
    EXPECT_EQ(insertedDocs.size(), fullCountSlice.getNumber<size_t>());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    auto expectedDoc = expectedDocs.rbegin() + 10;
    for (auto const actualDoc : arangodb::velocypack::ArrayIterator(result)) {
      auto const resolved = actualDoc.resolveExternals();
      EXPECT_TRUE(0 == arangodb::basics::VelocyPackHelper::compare(
                           arangodb::velocypack::Slice(expectedDoc->vpack()), resolved, true));
      ++expectedDoc;
    }
    EXPECT_EQ(expectedDoc, expectedDocs.rbegin() + 20);
  }

  // check full stats without optimization
  {
    auto const queryString =
        "FOR d IN testView SORT BM25(d), d.key DESC LIMIT 10, 10 RETURN d";
    auto const& expectedDocs = insertedDocs;

    auto queryResult = arangodb::tests::executeQuery(
        vocbase, queryString, {},
        "{ \"optimizer\" : { \"rules\": [ \"-sort-limit\"] }, \"fullCount\": "
        "true }");
    ASSERT_TRUE(queryResult.result.ok());

    auto root = queryResult.extra->slice();
    ASSERT_TRUE(root.isObject());
    auto stats = root.get("stats");
    ASSERT_TRUE(stats.isObject());
    auto fullCountSlice = stats.get("fullCount");
    ASSERT_TRUE(fullCountSlice.isNumber());
    EXPECT_EQ(insertedDocs.size(), fullCountSlice.getNumber<size_t>());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    auto expectedDoc = expectedDocs.rbegin() + 10;
    for (auto const actualDoc : arangodb::velocypack::ArrayIterator(result)) {
      auto const resolved = actualDoc.resolveExternals();
      EXPECT_TRUE(0 == arangodb::basics::VelocyPackHelper::compare(
                           arangodb::velocypack::Slice(expectedDoc->vpack()), resolved, true));
      ++expectedDoc;
    }
    EXPECT_EQ(expectedDoc, expectedDocs.rbegin() + 20);
  }
}
