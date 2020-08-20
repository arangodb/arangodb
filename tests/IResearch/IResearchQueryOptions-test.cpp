////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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
#include "IResearch/IResearchLink.h"
#include "IResearch/IResearchLinkHelper.h"
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

class IResearchQueryOptionsTest : public IResearchQueryTest {};

}  // namespace

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

TEST_F(IResearchQueryOptionsTest, Collections) {
  // ==, !=, <, <=, >, >=, range
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

  // add collection_3
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"collection_3\" }");
    auto logicalCollection3 = vocbase.createCollection(collectionJson->slice());
    ASSERT_NE(nullptr, logicalCollection3);
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

  std::deque<arangodb::ManagedDocumentResult> insertedDocs;

  // populate view with the data
  {
    arangodb::OperationOptions opt;

    arangodb::transaction::Methods trx(arangodb::transaction::StandaloneContext::Create(vocbase),
                                       EMPTY, EMPTY, EMPTY,
                                       arangodb::transaction::Options());
    EXPECT_TRUE(trx.begin().ok());

    // insert into collections
    {
      irs::utf8_path resource;
      resource /= irs::string_ref(arangodb::tests::testResourceDir);
      resource /= irs::string_ref("simple_sequential.json");

      auto builder =
          arangodb::basics::VelocyPackHelper::velocyPackFromFile(resource.utf8());
      auto root = builder.slice();
      ASSERT_TRUE(root.isArray());

      size_t i = 0;

      std::shared_ptr<arangodb::LogicalCollection> collections[]{logicalCollection1,
                                                                 logicalCollection2};

      for (auto doc : arangodb::velocypack::ArrayIterator(root)) {
        {
          insertedDocs.emplace_back();
          auto const res =
              collections[0]->insert(&trx, doc, insertedDocs.back(), opt);
          EXPECT_TRUE(res.ok());
        }
        {
          insertedDocs.emplace_back();
          auto const res =
              collections[1]->insert(&trx, doc, insertedDocs.back(), opt);
          EXPECT_TRUE(res.ok());
        }
        ++i;
      }
    }

    EXPECT_TRUE(trx.commit().ok());
    EXPECT_TRUE(
        (arangodb::tests::executeQuery(vocbase,
                                       "FOR d IN testView SEARCH 1 ==1 OPTIONS "
                                       "{ waitForSync: true } RETURN d")
             .result.ok()));  // commit
  }

  // -----------------------------------------------------------------------------
  // --SECTION--                                              'collections' option
  // -----------------------------------------------------------------------------

  // collection by name
  {
    std::string const query =
        "FOR d IN testView SEARCH d.name == 'A' OPTIONS { collections : [ "
        "'collection_1' ] }"
        " RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(
        vocbase, query, {arangodb::aql::OptimizerRule::handleArangoSearchViewsRule}));

    std::map<irs::string_ref, arangodb::ManagedDocumentResult const*> expectedDocs{
        {"A", &insertedDocs[0]}};

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
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
      EXPECT_TRUE(0 == arangodb::basics::VelocyPackHelper::compare(
                           arangodb::velocypack::Slice(expectedDoc->second->vpack()),
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
        vocbase, query, {arangodb::aql::OptimizerRule::handleArangoSearchViewsRule},
        arangodb::velocypack::Parser::fromJson(
            "{ \"collectionName\" : \"collection_1\" }")));

    std::map<irs::string_ref, arangodb::ManagedDocumentResult const*> expectedDocs{
        {"A", &insertedDocs[0]}};

    auto queryResult = arangodb::tests::executeQuery(
        vocbase, query, arangodb::velocypack::Parser::fromJson("{ \"collectionName\" : \"collection_1\" }"));
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
      EXPECT_TRUE(0 == arangodb::basics::VelocyPackHelper::compare(
                           arangodb::velocypack::Slice(expectedDoc->second->vpack()),
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
        vocbase, query, {arangodb::aql::OptimizerRule::handleArangoSearchViewsRule},
        arangodb::velocypack::Parser::fromJson(
            "{ \"collections\" : [ \"collection_1\" ] }")));

    std::map<irs::string_ref, arangodb::ManagedDocumentResult const*> expectedDocs{
        {"A", &insertedDocs[0]}};

    auto queryResult = arangodb::tests::executeQuery(
        vocbase, query, arangodb::velocypack::Parser::fromJson("{ \"collections\" : [ \"collection_1\" ] }"));
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
      EXPECT_TRUE(0 == arangodb::basics::VelocyPackHelper::compare(
                           arangodb::velocypack::Slice(expectedDoc->second->vpack()),
                           resolved, true));
      expectedDocs.erase(expectedDoc);
    }
    EXPECT_TRUE(expectedDocs.empty());
  }

  // collection by id
  {
    std::string const query =
        "FOR d IN testView SEARCH d.name == 'A'"
        " OPTIONS { collections : [ " + std::to_string(logicalCollection2->id()) + " ] }"
        " RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(
        vocbase, query, {arangodb::aql::OptimizerRule::handleArangoSearchViewsRule}));

    std::map<irs::string_ref, arangodb::ManagedDocumentResult const*> expectedDocs{
        {"A", &insertedDocs[1]}};

    // check node estimation
    {
      auto explanationResult =
          arangodb::tests::explainQuery(vocbase, query);
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
      ASSERT_EQ(insertedDocs.size()/2 + 1., viewNode.get("estimatedCost").getDouble());
      ASSERT_EQ(insertedDocs.size()/2, viewNode.get("estimatedNrItems").getNumber<size_t>());
    }

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
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
      EXPECT_TRUE(0 == arangodb::basics::VelocyPackHelper::compare(
                           arangodb::velocypack::Slice(expectedDoc->second->vpack()),
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
        std::to_string(logicalCollection2->id()) +
        "' ] }"
        " RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(
        vocbase, query, {arangodb::aql::OptimizerRule::handleArangoSearchViewsRule}));

    std::map<irs::string_ref, arangodb::ManagedDocumentResult const*> expectedDocs{
        {"A", &insertedDocs[1]}};

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
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
      EXPECT_TRUE(0 == arangodb::basics::VelocyPackHelper::compare(
                           arangodb::velocypack::Slice(expectedDoc->second->vpack()),
                           resolved, true));
      expectedDocs.erase(expectedDoc);
    }
    EXPECT_TRUE(expectedDocs.empty());
  }

  // multiple collections
  {
    std::string const query =
        "FOR d IN testView SEARCH d.name == 'A'"
        " OPTIONS { collections : [ '" + std::to_string(logicalCollection2->id()) + "', 'collection_1' ] }"
        " SORT d._id"
        " RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(
        vocbase, query, {arangodb::aql::OptimizerRule::handleArangoSearchViewsRule}));

    std::map<irs::string_ref, std::vector<arangodb::ManagedDocumentResult const*>> expectedDocs{
        {"A", {&insertedDocs[0], &insertedDocs[1]}}};

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_EQ(2, resultIt.size());

    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      auto const keySlice = resolved.get("name");
      auto const key = arangodb::iresearch::getStringRef(keySlice);

      auto it = expectedDocs.find(key);
      ASSERT_NE(it, expectedDocs.end());
      ASSERT_FALSE(it->second.empty());
      auto expectedDoc = it->second.begin();
      ASSERT_NE(expectedDoc, it->second.end());
      EXPECT_TRUE(0 == arangodb::basics::VelocyPackHelper::compare(
                           arangodb::velocypack::Slice((**expectedDoc).vpack()),
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
        vocbase, query, {arangodb::aql::OptimizerRule::handleArangoSearchViewsRule}));

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_EQ(0, resultIt.size());
  }

  // null means "no restrictions"
  {
    std::string const query =
        "FOR d IN testView SEARCH d.name == 'A' OPTIONS { collections : null }"
        " SORT d._id"
        " RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(
        vocbase, query, {arangodb::aql::OptimizerRule::handleArangoSearchViewsRule}));

    // check node estimation
    {
      auto explanationResult =
          arangodb::tests::explainQuery(vocbase, query);
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

    std::map<irs::string_ref, std::vector<arangodb::ManagedDocumentResult const*>> expectedDocs{
        {"A", {&insertedDocs[0], &insertedDocs[1]}}};

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_EQ(2, resultIt.size());

    for (auto const actualDoc : resultIt) {
      auto const resolved = actualDoc.resolveExternals();
      auto const keySlice = resolved.get("name");
      auto const key = arangodb::iresearch::getStringRef(keySlice);

      auto it = expectedDocs.find(key);
      ASSERT_NE(it, expectedDocs.end());
      ASSERT_FALSE(it->second.empty());
      auto expectedDoc = it->second.begin();
      ASSERT_NE(expectedDoc, it->second.end());
      EXPECT_TRUE(0 == arangodb::basics::VelocyPackHelper::compare(
                           arangodb::velocypack::Slice((**expectedDoc).vpack()),
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
        vocbase, query, {arangodb::aql::OptimizerRule::handleArangoSearchViewsRule}));

    std::map<irs::string_ref, std::vector<arangodb::ManagedDocumentResult const*>> expectedDocs{
        {"A", {&insertedDocs[0], &insertedDocs[1]}}};

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
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
      ASSERT_EQ(2, it->second.size());
      ASSERT_TRUE(0 == arangodb::basics::VelocyPackHelper::compare(
                           arangodb::velocypack::Slice(it->second[0]->vpack()),
                           resolvedd, true));
      ASSERT_TRUE(0 == arangodb::basics::VelocyPackHelper::compare(
                           arangodb::velocypack::Slice(it->second[1]->vpack()),
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

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    ASSERT_TRUE(queryResult.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // wrong collection id is specified
  {
    std::string const query =
        "FOR d IN testView SEARCH d.name == 'A' OPTIONS { collections : [ "
        "'collection_1', 32112312 ] }"
        " SORT d._id"
        " RETURN d";

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    ASSERT_TRUE(queryResult.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // wrong collection id as string is specified
  {
    std::string const query =
        "FOR d IN testView SEARCH d.name == 'A' OPTIONS { collections : [ "
        "'collection_1', '32112312' ] }"
        " SORT d._id"
        " RETURN d";

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    ASSERT_TRUE(queryResult.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // invalid option type
  {
    std::string const query =
        "FOR d IN testView SEARCH d.name == 'A' OPTIONS { collections : [ "
        "'collection_1', null ] }"
        " SORT d._id"
        " RETURN d";

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    ASSERT_TRUE(queryResult.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // invalid option type
  {
    std::string const query =
        "FOR d IN testView SEARCH d.name == 'A' OPTIONS { collections : [ "
        "'collection_1', {} ] }"
        " SORT d._id"
        " RETURN d";

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    ASSERT_TRUE(queryResult.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // invalid option type
  {
    std::string const query =
        "FOR d IN testView SEARCH d.name == 'A' OPTIONS { collections : [ "
        "'collection_1', true ] }"
        " SORT d._id"
        " RETURN d";

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    ASSERT_TRUE(queryResult.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // invalid option type
  {
    std::string const query =
        "FOR d IN testView SEARCH d.name == 'A' OPTIONS { collections : [ "
        "'collection_1', null ] }"
        " SORT d._id"
        " RETURN d";

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    ASSERT_TRUE(queryResult.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // invalid options type
  {
    std::string const query =
        "FOR d IN testView SEARCH d.name == 'A' OPTIONS { collections : true }"
        " SORT d._id"
        " RETURN d";

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    ASSERT_TRUE(queryResult.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // invalid options type
  {
    std::string const query =
        "FOR d IN testView SEARCH d.name == 'A' OPTIONS { collections : 1 }"
        " SORT d._id"
        " RETURN d";

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    ASSERT_TRUE(queryResult.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // invalid options type
  {
    std::string const query =
        "FOR d IN testView SEARCH d.name == 'A' OPTIONS { collections : {} }"
        " SORT d._id"
        " RETURN d";

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    ASSERT_TRUE(queryResult.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // collection which is not registered witht a view is specified
  {
    std::string const query =
        "FOR d IN testView SEARCH d.name == 'A' OPTIONS { collections : [ "
        "'collection_1', 'collection_3' ] }"
        " SORT d._id"
        " RETURN d";

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    ASSERT_TRUE(queryResult.result.is(TRI_ERROR_BAD_PARAMETER));
  }
}

TEST_F(IResearchQueryOptionsTest, WaitForSync) {
  // ==, !=, <, <=, >, >=, range
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

  std::deque<arangodb::ManagedDocumentResult> insertedDocs;

  // populate view with the data
  {
    arangodb::OperationOptions opt;

    arangodb::transaction::Methods trx(arangodb::transaction::StandaloneContext::Create(vocbase),
                                       EMPTY, EMPTY, EMPTY,
                                       arangodb::transaction::Options());
    EXPECT_TRUE(trx.begin().ok());

    // insert into collections
    {
      irs::utf8_path resource;
      resource /= irs::string_ref(arangodb::tests::testResourceDir);
      resource /= irs::string_ref("simple_sequential.json");

      auto builder =
          arangodb::basics::VelocyPackHelper::velocyPackFromFile(resource.utf8());
      auto root = builder.slice();
      ASSERT_TRUE(root.isArray());

      size_t i = 0;

      std::shared_ptr<arangodb::LogicalCollection> collections[]{logicalCollection1,
                                                                 logicalCollection2};

      for (auto doc : arangodb::velocypack::ArrayIterator(root)) {
        insertedDocs.emplace_back();
        auto const res =
            collections[i % 2]->insert(&trx, doc, insertedDocs.back(), opt);
        EXPECT_TRUE(res.ok());
        ++i;
      }
    }

    EXPECT_TRUE(trx.commit().ok());
  }

  // -----------------------------------------------------------------------------
  // --SECTION--                                              'waitForSync' option
  // -----------------------------------------------------------------------------

  // wrong option type is specified
  {
    std::string const query =
        "FOR d IN testView SEARCH d.name == 'A' OPTIONS { waitForSync: null }"
        " SORT d._id"
        " RETURN d";

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    ASSERT_TRUE(queryResult.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // wrong option type is specified
  {
    std::string const query =
        "FOR d IN testView SEARCH d.name == 'A' OPTIONS { waitForSync: 1 }"
        " SORT d._id"
        " RETURN d";

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    ASSERT_TRUE(queryResult.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // wrong option type is specified
  {
    std::string const query =
        "FOR d IN testView SEARCH d.name == 'A' OPTIONS { waitForSync: 'true' }"
        " SORT d._id"
        " RETURN d";

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    ASSERT_TRUE(queryResult.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // wrong option type is specified
  {
    std::string const query =
        "FOR d IN testView SEARCH d.name == 'A' OPTIONS { waitForSync: [] }"
        " SORT d._id"
        " RETURN d";

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    ASSERT_TRUE(queryResult.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // wrong option type is specified
  {
    std::string const query =
        "FOR d IN testView SEARCH d.name == 'A' OPTIONS { waitForSync: {} }"
        " SORT d._id"
        " RETURN d";

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    ASSERT_TRUE(queryResult.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // don't sync
  {
    std::string const query =
        "FOR d IN testView SEARCH d.name == 'A' OPTIONS { waitForSync: false }"
        " RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(
        vocbase, query, {arangodb::aql::OptimizerRule::handleArangoSearchViewsRule}));

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_EQ(0, resultIt.size());
  }

  // do sync, bind parameter
  {
    std::string const query =
        "FOR d IN testView SEARCH d.name == 'A' OPTIONS { waitForSync: @doSync "
        "}"
        " RETURN d";

    EXPECT_TRUE(arangodb::tests::assertRules(
        vocbase, query, {arangodb::aql::OptimizerRule::handleArangoSearchViewsRule},
        arangodb::velocypack::Parser::fromJson("{ \"doSync\" : true }")));

    std::map<irs::string_ref, arangodb::ManagedDocumentResult const*> expectedDocs{
        {"A", &insertedDocs[0]}};

    auto queryResult = arangodb::tests::executeQuery(
        vocbase, query, arangodb::velocypack::Parser::fromJson("{ \"doSync\" : true }"));
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
      EXPECT_TRUE(0 == arangodb::basics::VelocyPackHelper::compare(
                           arangodb::velocypack::Slice(expectedDoc->second->vpack()),
                           resolved, true));
      expectedDocs.erase(expectedDoc);
    }
    EXPECT_TRUE(expectedDocs.empty());
  }
}

TEST_F(IResearchQueryOptionsTest, noMaterialization) {
  static std::vector<std::string> const EMPTY;

  auto createJson = arangodb::velocypack::Parser::fromJson(
      "{ \
        \"name\": \"testView\", \
        \"type\": \"arangosearch\", \
        \"storedValues\": [{\"fields\":[\"str\"]}, {\"fields\":[\"value\"]}, {\"fields\":[\"_id\"]}] \
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
    EXPECT_TRUE(slice.get("deleted").isNone()); // no system properties
    auto tmpSlice = slice.get("links");
    EXPECT_TRUE(tmpSlice.isObject() && 2 == tmpSlice.length());
  }

  std::deque<arangodb::ManagedDocumentResult> insertedDocs;

  // populate view with the data
  {
    arangodb::OperationOptions opt;
    static std::vector<std::string> const EMPTY;
    arangodb::transaction::Methods trx(
        arangodb::transaction::StandaloneContext::Create(vocbase),
        EMPTY, EMPTY, EMPTY, arangodb::transaction::Options());
    EXPECT_TRUE(trx.begin().ok());

    // insert into collection_1
    {
      auto builder = VPackParser::fromJson(
          "["
            "{\"_key\": \"c0\", \"str\": \"cat\", \"foo\": \"foo0\", \"value\": 0},"
            "{\"_key\": \"c1\", \"str\": \"cat\", \"foo\": \"foo1\", \"value\": 1},"
            "{\"_key\": \"c2\", \"str\": \"cat\", \"foo\": \"foo2\", \"value\": 2},"
            "{\"_key\": \"c3\", \"str\": \"cat\", \"foo\": \"foo3\", \"value\": 3}"
          "]");

      auto root = builder->slice();
      ASSERT_TRUE(root.isArray());

      for (auto doc : arangodb::velocypack::ArrayIterator(root)) {
        insertedDocs.emplace_back();
        auto const res =
            logicalCollection1->insert(&trx, doc, insertedDocs.back(), opt);
        EXPECT_TRUE(res.ok());
      }
    }

    // insert into collection_2
    {
      auto builder = VPackParser::fromJson(
          "["
            "{\"_key\": \"c_0\", \"str\": \"cat\", \"foo\": \"foo_0\", \"value\": 10},"
            "{\"_key\": \"c_1\", \"str\": \"cat\", \"foo\": \"foo_1\", \"value\": 11},"
            "{\"_key\": \"c_2\", \"str\": \"cat\", \"foo\": \"foo_2\", \"value\": 12},"
            "{\"_key\": \"c_3\", \"str\": \"cat\", \"foo\": \"foo_3\", \"value\": 13}"
          "]");

      auto root = builder->slice();
      ASSERT_TRUE(root.isArray());

      for (auto doc : arangodb::velocypack::ArrayIterator(root)) {
        insertedDocs.emplace_back();
        auto const res =
            logicalCollection2->insert(&trx, doc, insertedDocs.back(), opt);
        EXPECT_TRUE(res.ok());
      }
    }

    EXPECT_TRUE(trx.commit().ok());

    EXPECT_TRUE(arangodb::iresearch::IResearchLinkHelper::find(*logicalCollection1, *view)
                ->commit().ok());

    EXPECT_TRUE(arangodb::iresearch::IResearchLinkHelper::find(*logicalCollection2, *view)
                ->commit().ok());
  }

  // -----------------------------------------------------------------------------
  // --SECTION--                                        'noMaterialization' option
  // -----------------------------------------------------------------------------

  // wrong option type is specified
  {
    std::string const query =
        "FOR d IN testView SEARCH d.str == 'cat' OPTIONS { noMaterialization: null }"
        " SORT d._id"
        " RETURN d.value";

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    ASSERT_TRUE(queryResult.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // wrong option type is specified
  {
    std::string const query =
        "FOR d IN testView SEARCH d.str == 'cat' OPTIONS { noMaterialization: 1 }"
        " SORT d._id"
        " RETURN d.value";

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    ASSERT_TRUE(queryResult.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // wrong option type is specified
  {
    std::string const query =
        "FOR d IN testView SEARCH d.str == 'cat' OPTIONS { noMaterialization: 'true' }"
        " SORT d._id"
        " RETURN d.value";

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    ASSERT_TRUE(queryResult.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // wrong option type is specified
  {
    std::string const query =
        "FOR d IN testView SEARCH d.str == 'cat' OPTIONS { noMaterialization: [] }"
        " SORT d._id"
        " RETURN d.value";

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    ASSERT_TRUE(queryResult.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // wrong option type is specified
  {
    std::string const query =
        "FOR d IN testView SEARCH d.str == 'cat' OPTIONS { noMaterialization: {} }"
        " SORT d._id"
        " RETURN d.value";

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    ASSERT_TRUE(queryResult.result.is(TRI_ERROR_BAD_PARAMETER));
  }

  // do not materialize
  {
    std::string const queryString =
        "FOR d IN testView SEARCH d.str == 'cat' OPTIONS { noMaterialization: true }"
        " RETURN d.value";

    EXPECT_TRUE(arangodb::tests::assertRules(
        vocbase, queryString, {arangodb::aql::OptimizerRule::handleArangoSearchViewsRule}));

    arangodb::aql::Query query(arangodb::transaction::StandaloneContext::Create(vocbase), arangodb::aql::QueryString(queryString), nullptr,
                               arangodb::velocypack::Parser::fromJson("{}"));
    auto const res = query.explain();
    ASSERT_TRUE(res.data);
    auto const explanation = res.data->slice();
    arangodb::velocypack::ArrayIterator nodes(explanation.get("nodes"));
    auto found = false;
    for (auto const node : nodes) {
      if (node.hasKey("type") && node.get("type").isString() && node.get("type").stringRef() == "EnumerateViewNode") {
        EXPECT_TRUE(node.hasKey("noMaterialization") && node.get("noMaterialization").isBool() && node.get("noMaterialization").getBool());
        found = true;
        break;
      }
    }
    EXPECT_TRUE(found);

    auto queryResult = arangodb::tests::executeQuery(vocbase, queryString);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_EQ(8, resultIt.size());
  }

  // materialize
  {
    std::string const queryString =
        "FOR d IN testView SEARCH d.str == 'cat' OPTIONS { noMaterialization: false }"
        " RETURN d.value";

    EXPECT_TRUE(arangodb::tests::assertRules(
        vocbase, queryString, {arangodb::aql::OptimizerRule::handleArangoSearchViewsRule}));

    arangodb::aql::Query query(arangodb::transaction::StandaloneContext::Create(vocbase), arangodb::aql::QueryString(queryString), nullptr,
                               arangodb::velocypack::Parser::fromJson("{}"));
    auto const res = query.explain();
    ASSERT_TRUE(res.data);
    auto const explanation = res.data->slice();
    arangodb::velocypack::ArrayIterator nodes(explanation.get("nodes"));
    auto found = false;
    for (auto const node : nodes) {
      if (node.hasKey("type") && node.get("type").isString() && node.get("type").stringRef() == "EnumerateViewNode") {
        EXPECT_FALSE(node.hasKey("noMaterialization"));
        found = true;
        break;
      }
    }
    EXPECT_TRUE(found);

    auto queryResult = arangodb::tests::executeQuery(vocbase, queryString);
    ASSERT_TRUE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    EXPECT_TRUE(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    EXPECT_EQ(8, resultIt.size());
  }
}
