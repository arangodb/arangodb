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

#include "catch.hpp"
#include "common.h"
#include "StorageEngineMock.h"

#include "ApplicationFeatures/JemallocFeature.h"
#include "Aql/Ast.h"
#include "Aql/AqlFunctionFeature.h"
#include "Aql/Query.h"
#include "Aql/SortCondition.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "IResearch/IResearchAnalyzerFeature.h"
#include "IResearch/IResearchFeature.h"
#include "IResearch/AttributeScorer.h"
#include "IResearch/IResearchView.h"
#include "RestServer/AqlFeature.h"
#include "MMFiles/MMFilesDocumentPosition.h"
#include "RestServer/FlushFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/DatabasePathFeature.h"
#include "RestServer/FeatureCacheFeature.h"
#include "RestServer/TraverserEngineRegistryFeature.h"
#include "RestServer/ViewTypesFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "Transaction/StandaloneContext.h"
#include "Transaction/UserTransaction.h"
#include "Utils/OperationOptions.h"
#include "Views/ViewIterator.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/LogicalView.h"
#include "VocBase/ManagedDocumentResult.h"
#include "velocypack/Parser.h"

NS_LOCAL

void assertOrderSuccess(
    arangodb::LogicalView& view,
    std::string const& queryString,
    std::string const& field,
    std::vector<size_t> const& expected
) {
  auto* vocbase = view.vocbase();
  std::shared_ptr<arangodb::velocypack::Builder> bindVars;
  auto options = std::make_shared<arangodb::velocypack::Builder>();

  arangodb::aql::Query query(
     false, vocbase, arangodb::aql::QueryString(queryString),
     bindVars, options,
     arangodb::aql::PART_MAIN
  );

  auto const parseResult = query.parse();
  REQUIRE(TRI_ERROR_NO_ERROR == parseResult.code);

  auto* root = query.ast()->root();
  REQUIRE(root);
  auto* filterNode = root->getMember(1);
  REQUIRE(filterNode);
  auto* orderNode = root->getMember(2);
  REQUIRE(orderNode);
  auto* sortNode = orderNode->getMember(0);
  REQUIRE(sortNode);

  std::vector<std::vector<arangodb::basics::AttributeName>> attrs;
  std::vector<std::pair<arangodb::aql::Variable const*, bool>> sorts;
  std::unordered_map<arangodb::aql::VariableId, arangodb::aql::AstNode const*> variableNodes;
  std::vector<arangodb::aql::Variable> variables;

  variables.reserve(sortNode->numMembers());

  for (size_t i = 0, count = sortNode->numMembers(); i < count; ++i) {
    variables.emplace_back("arg", i);
    sorts.emplace_back(&variables.back(), sortNode->getMember(i)->getMember(1)->value.value._bool);
    variableNodes.emplace(variables.back().id, sortNode->getMember(i)->getMember(0));
  }

  static std::vector<std::string> const EMPTY;
  arangodb::transaction::UserTransaction trx(arangodb::transaction::StandaloneContext::Create(vocbase), EMPTY, EMPTY, EMPTY, arangodb::transaction::Options());
  arangodb::aql::SortCondition order(nullptr, sorts, attrs, variableNodes);
  CHECK((trx.begin().ok()));
  std::shared_ptr<arangodb::ViewIterator> itr(view.iteratorForCondition(&trx, filterNode, nullptr, &order));
  CHECK((false == !itr));

  size_t next = 0;
  auto callback = [&field, &expected, itr, &next](arangodb::DocumentIdentifierToken const& token) {
    arangodb::ManagedDocumentResult result;

    CHECK((itr->readDocument(token, result)));

    arangodb::velocypack::Slice doc(result.vpack());

    CHECK((next < expected.size()));
    CHECK((doc.hasKey(field)));
    CHECK((doc.get(field).isNumber()));
    CHECK((expected[next++] == doc.get(field).getNumber<size_t>()));
  };

  CHECK((!itr->next(callback, size_t(-1)))); // false because no more results
  CHECK((trx.commit().ok()));
  CHECK((expected.size() == next));
}

NS_END

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

struct IResearchAttributeScorerSetup {
  StorageEngineMock engine;
  arangodb::application_features::ApplicationServer server;
  std::vector<std::pair<arangodb::application_features::ApplicationFeature*, bool>> features;

  IResearchAttributeScorerSetup(): server(nullptr, nullptr) {
    arangodb::EngineSelectorFeature::ENGINE = &engine;

    arangodb::tests::init();

    // setup required application features
    features.emplace_back(new arangodb::QueryRegistryFeature(&server), false); // must be first
    features.emplace_back(new arangodb::TraverserEngineRegistryFeature(&server), false); // must be before AqlFeature
    features.emplace_back(new arangodb::AqlFeature(&server), true);
    features.emplace_back(new arangodb::AuthenticationFeature(&server), true);
    features.emplace_back(new arangodb::DatabaseFeature(&server), false);
    features.emplace_back(new arangodb::DatabasePathFeature(&server), false);
    features.emplace_back(new arangodb::JemallocFeature(&server), false); // required for DatabasePathFeature
    features.emplace_back(new arangodb::FeatureCacheFeature(&server), true);
    features.emplace_back(new arangodb::aql::AqlFunctionFeature(&server), true); // required for IResearchAnalyzerFeature
    features.emplace_back(new arangodb::iresearch::IResearchAnalyzerFeature(&server), true);
    features.emplace_back(new arangodb::iresearch::IResearchFeature(&server), true);
    features.emplace_back(new arangodb::ViewTypesFeature(&server), true);
    features.emplace_back(new arangodb::FlushFeature(&server), false); // do not start the thread

    for (auto& entry: features) {
      arangodb::application_features::ApplicationServer::server->addFeature(entry.first);
      entry.first->prepare();

      if (entry.second) {
        entry.first->start();
      }
    }
  }

  ~IResearchAttributeScorerSetup() {
    arangodb::application_features::ApplicationServer::server = nullptr;
    arangodb::EngineSelectorFeature::ENGINE = nullptr;

    // destroy application features
    for (auto itr = features.rbegin(), end = features.rend(); itr != end; ++itr) {
      auto& entry = *itr;

      if (entry.second) {
        entry.first->stop();
      }

      entry.first->unprepare();
    }

    arangodb::FeatureCacheFeature::reset();
  }
}; // IResearchAttributeScorerSetup

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief setup
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("IResearchAttributeScorerTest", "[iresearch][iresearch-scorer][iresearch-attribute_scorer]") {
  IResearchAttributeScorerSetup s;
  UNUSED(s);

SECTION("test_query") {
  static std::vector<std::string> const EMPTY;
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
  auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
  auto viewJson = arangodb::velocypack::Parser::fromJson("{ \
    \"name\": \"testView\", \
    \"type\": \"iresearch\", \
    \"properties\": { \
      \"links\": { \"testCollection\": { \"includeAllFields\": true } } \
    } \
  }");
  auto* logicalCollection = vocbase.createCollection(collectionJson->slice());
  REQUIRE((nullptr != logicalCollection));
  auto logicalView = vocbase.createView(viewJson->slice(), 0);
  REQUIRE((false == !logicalView));
  auto* view = logicalView->getImplementation();
  REQUIRE((false == !view));

  // fill with test data
  {
    auto doc0 = arangodb::velocypack::Parser::fromJson("{ \"key\": 1, \"testAttr\": \"A\" }");
    auto doc1 = arangodb::velocypack::Parser::fromJson("{ \"key\": 2, \"testAttr\": \"B\" }");
    auto doc2 = arangodb::velocypack::Parser::fromJson("{ \"key\": 3, \"testAttr\": \"C\" }");
    auto doc3 = arangodb::velocypack::Parser::fromJson("{ \"key\": 4, \"testAttr\": 1 }");
    auto doc4 = arangodb::velocypack::Parser::fromJson("{ \"key\": 5, \"testAttr\": 2.71828 }");
    auto doc5 = arangodb::velocypack::Parser::fromJson("{ \"key\": 6, \"testAttr\": 3.14159 }");
    auto doc6 = arangodb::velocypack::Parser::fromJson("{ \"key\": 7, \"testAttr\": true }");
    auto doc7 = arangodb::velocypack::Parser::fromJson("{ \"key\": 8, \"testAttr\": false }");
    auto doc8 = arangodb::velocypack::Parser::fromJson("{ \"key\": 9, \"testAttr\": null }");
    auto doc9 = arangodb::velocypack::Parser::fromJson("{ \"key\": 10, \"testAttr\": [ -1 ] }");
    auto doc10 = arangodb::velocypack::Parser::fromJson("{ \"key\": 11, \"testAttr\": { \"a\": \"b\" } }");
    auto doc11 = arangodb::velocypack::Parser::fromJson("{ \"key\": 12 }");

    arangodb::OperationOptions options;
    arangodb::ManagedDocumentResult tmpResult;
    TRI_voc_tick_t tmpResultTick;
    arangodb::transaction::UserTransaction trx(arangodb::transaction::StandaloneContext::Create(&vocbase), EMPTY, EMPTY, EMPTY, arangodb::transaction::Options());
    CHECK((trx.begin().ok()));
    CHECK((logicalCollection->insert(&trx, doc0->slice(), tmpResult, options, tmpResultTick, false)).ok());
    CHECK((logicalCollection->insert(&trx, doc1->slice(), tmpResult, options, tmpResultTick, false)).ok());
    CHECK((logicalCollection->insert(&trx, doc2->slice(), tmpResult, options, tmpResultTick, false)).ok());
    CHECK((logicalCollection->insert(&trx, doc3->slice(), tmpResult, options, tmpResultTick, false)).ok());
    CHECK((logicalCollection->insert(&trx, doc4->slice(), tmpResult, options, tmpResultTick, false)).ok());
    CHECK((logicalCollection->insert(&trx, doc5->slice(), tmpResult, options, tmpResultTick, false)).ok());
    CHECK((logicalCollection->insert(&trx, doc6->slice(), tmpResult, options, tmpResultTick, false)).ok());
    CHECK((logicalCollection->insert(&trx, doc7->slice(), tmpResult, options, tmpResultTick, false)).ok());
    CHECK((logicalCollection->insert(&trx, doc8->slice(), tmpResult, options, tmpResultTick, false)).ok());
    CHECK((logicalCollection->insert(&trx, doc9->slice(), tmpResult, options, tmpResultTick, false)).ok());
    CHECK((logicalCollection->insert(&trx, doc10->slice(), tmpResult, options, tmpResultTick, false)).ok());
    CHECK((logicalCollection->insert(&trx, doc11->slice(), tmpResult, options, tmpResultTick, false)).ok());
    CHECK((trx.commit().ok()));
    dynamic_cast<arangodb::iresearch::IResearchView*>(view)->sync();
  }

  // query view
  {
    // ArangoDB default type sort order:
    // null < bool < number < string < array/list < object/document
    std::vector<size_t> const expected = { 9, 8, 7, 4, 5, 6, 1, 2, 3, 10, 11, 12 };
    std::string query = "FOR d IN testCollection FILTER d.key >= 1 SORT 'testAttr' RETURN d";

    assertOrderSuccess(*logicalView, query, "key", expected);
  }

  // query view ascending
  {
    // ArangoDB default type sort order:
    // null < bool < number < string < array/list < object/document
    std::vector<size_t> const expected = { 9, 8, 7, 4, 5, 6, 1, 2, 3, 10, 11, 12 };
    std::string query = "FOR d IN testCollection FILTER d.key >= 1 SORT 'testAttr' ASC RETURN d";

    assertOrderSuccess(*logicalView, query, "key", expected);
  }

  // query view descending
  {
    // ArangoDB default type sort order:
    // null < bool < number < string < array/list < object/document
    std::vector<size_t> const expected = { 12, 11, 10, 3, 2, 1, 6, 5, 4, 7, 8, 9 };
    std::string query = "FOR d IN testCollection FILTER d.key >= 1 SORT 'testAttr' DESC RETURN d";

    assertOrderSuccess(*logicalView, query, "key", expected);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate tests
////////////////////////////////////////////////////////////////////////////////

}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------