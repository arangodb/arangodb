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
#include "ExpressionContextMock.h"

#include "Aql/Ast.h"
#include "Aql/AqlFunctionFeature.h"
#include "Aql/Query.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/SortCondition.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "IResearch/ApplicationServerHelper.h"
#include "IResearch/IResearchAnalyzerFeature.h"
#include "IResearch/IResearchFeature.h"
#include "IResearch/AttributeScorer.h"
#include "IResearch/IResearchOrderFactory.h"
#include "IResearch/IResearchView.h"
#include "IResearch/SystemDatabaseFeature.h"
#include "MMFiles/MMFilesDocumentPosition.h"
#include "RestServer/AqlFeature.h"
#include "RestServer/FlushFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/DatabasePathFeature.h"
#include "RestServer/TraverserEngineRegistryFeature.h"
#include "RestServer/ViewTypesFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "Transaction/StandaloneContext.h"
#include "Transaction/UserTransaction.h"
#include "Utils/OperationOptions.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/LogicalView.h"
#include "VocBase/ManagedDocumentResult.h"
#include "velocypack/Parser.h"

NS_LOCAL

void assertOrderFail(
    arangodb::LogicalView& view,
    std::string const& queryString,
    size_t parseCode
) {
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");

  arangodb::aql::Query query(
     false, &vocbase, arangodb::aql::QueryString(queryString),
     nullptr, nullptr,
     arangodb::aql::PART_MAIN
  );

  auto const parseResult = query.parse();
  REQUIRE(parseCode == parseResult.code);

  if (TRI_ERROR_NO_ERROR != parseCode) {
    return; // expecting a parse error, nothing more to check
  }

  auto* root = query.ast()->root();
  REQUIRE(root);
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

  irs::order actual;
  std::vector<irs::stored_attribute::ptr> actualAttrs;
  arangodb::iresearch::OrderFactory::OrderContext ctx { actualAttrs, actual };
  arangodb::aql::SortCondition order(nullptr, sorts, attrs, variableNodes);
  arangodb::iresearch::IResearchViewMeta meta;

  CHECK((!arangodb::iresearch::OrderFactory::order(nullptr, order, meta)));
  CHECK((!arangodb::iresearch::OrderFactory::order(&ctx, order, meta)));
}

void assertOrderSuccess(
    arangodb::LogicalView& view,
    std::string const& queryString,
    std::string const& field,
    std::vector<size_t> const& expected
) {
  std::vector<size_t> expectedOrdered;
  std::unordered_multiset<size_t> expectedUnordered;
  bool ordered = true;

  for (auto entry: expected) {
    if (size_t(-1) == entry) { // unordered delimiter
      ordered = false;
    } else if (ordered) {
      expectedOrdered.emplace_back(entry);
    } else {
      expectedUnordered.emplace(entry);
    }
  }

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
  auto* allVars  = query.ast()->variables();
  REQUIRE(allVars);
  CHECK(1 == allVars->variables(true).size()); // expect that we have exactly 1 variable here
  auto* var = allVars->getVariable(0);
  REQUIRE(var);

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
  REQUIRE(!variables.empty());


  static std::vector<std::string> const EMPTY;
  arangodb::transaction::UserTransaction trx(arangodb::transaction::StandaloneContext::Create(vocbase), EMPTY, EMPTY, EMPTY, arangodb::transaction::Options());
  arangodb::aql::SortCondition order(nullptr, sorts, attrs, variableNodes);
  CHECK((trx.begin().ok()));
  arangodb::aql::ExecutionPlan plan(query.ast());
  std::shared_ptr<arangodb::ViewIterator> itr;
  //std::shared_ptr<arangodb::ViewIterator> itr(view.iteratorForCondition(&trx, &plan, &ExpressionContextMock::EMPTY, filterNode, var, &order));
  CHECK((false == !itr));

  size_t next = 0;
  auto callback = [&field, &expectedOrdered, &expectedUnordered, itr, &next](arangodb::LocalDocumentId const& token)->void {
    arangodb::ManagedDocumentResult result;

    CHECK((itr->readDocument(token, result)));

    arangodb::velocypack::Slice doc(result.vpack());

    CHECK((doc.hasKey(field)));
    CHECK((doc.get(field).isNumber()));

    if (next < expectedOrdered.size()) {
      CHECK((expectedOrdered[next++] == doc.get(field).getNumber<size_t>()));
    } else {
      auto itr = expectedUnordered.find(doc.get(field).getNumber<size_t>());

      CHECK((itr != expectedUnordered.end()));
      expectedUnordered.erase(itr);
    }
  };

  CHECK((!itr->next(callback, size_t(-1)))); // false because no more results
  CHECK((trx.commit().ok()));
  CHECK((next == expectedOrdered.size() && expectedUnordered.empty()));
}

NS_END

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

struct IResearchAttributeScorerSetup {
  StorageEngineMock engine;
  arangodb::application_features::ApplicationServer server;
  std::unique_ptr<TRI_vocbase_t> system;
  std::vector<std::pair<arangodb::application_features::ApplicationFeature*, bool>> features;

  IResearchAttributeScorerSetup(): server(nullptr, nullptr) {
    arangodb::EngineSelectorFeature::ENGINE = &engine;

    arangodb::tests::init();

    // suppress INFO {authentication} Authentication is turned on (system only), authentication for unix sockets is turned on
    arangodb::LogTopic::setLogLevel(arangodb::Logger::AUTHENTICATION.name(), arangodb::LogLevel::WARN);

    // setup required application features
    features.emplace_back(new arangodb::QueryRegistryFeature(&server), false); // must be first
    arangodb::application_features::ApplicationServer::server->addFeature(features.back().first);
    system = irs::memory::make_unique<TRI_vocbase_t>(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 0, TRI_VOC_SYSTEM_DATABASE);
    features.emplace_back(new arangodb::TraverserEngineRegistryFeature(&server), false); // must be before AqlFeature
    features.emplace_back(new arangodb::AqlFeature(&server), true);
    features.emplace_back(new arangodb::AuthenticationFeature(&server), true);
    features.emplace_back(new arangodb::DatabaseFeature(&server), false);
    features.emplace_back(new arangodb::DatabasePathFeature(&server), false);
    features.emplace_back(new arangodb::aql::AqlFunctionFeature(&server), true); // required for IResearchAnalyzerFeature
    features.emplace_back(new arangodb::iresearch::IResearchAnalyzerFeature(&server), true);
    features.emplace_back(new arangodb::iresearch::IResearchFeature(&server), true);
    features.emplace_back(new arangodb::iresearch::SystemDatabaseFeature(&server, system.get()), false); // required for IResearchAnalyzerFeature
    features.emplace_back(new arangodb::ViewTypesFeature(&server), true);
    features.emplace_back(new arangodb::FlushFeature(&server), false); // do not start the thread

    for (auto& f : features) {
      arangodb::application_features::ApplicationServer::server->addFeature(f.first);
    }

    for (auto& f : features) {
      f.first->prepare();
    }

    for (auto& f : features) {
      if (f.second) {
        f.first->start();
      }
    }

    // external function names must be registred in upper-case
    // user defined functions have ':' in the external function name
    // function arguments string format: requiredArg1[,requiredArg2]...[|optionalArg1[,optionalArg2]...]
    auto& functions = *arangodb::aql::AqlFunctionFeature::AQLFUNCTIONS;
    arangodb::aql::Function attr("@", ".|+", false, true, true, false);

    arangodb::iresearch::addFunction(functions, attr);

    // suppress log messages since tests check error conditions
    arangodb::LogTopic::setLogLevel(arangodb::iresearch::IResearchFeature::IRESEARCH.name(), arangodb::LogLevel::FATAL);
    irs::logger::output_le(iresearch::logger::IRL_FATAL, stderr);
  }

  ~IResearchAttributeScorerSetup() {
    system.reset(); // destroy before reseting the 'ENGINE'
    arangodb::LogTopic::setLogLevel(arangodb::iresearch::IResearchFeature::IRESEARCH.name(), arangodb::LogLevel::DEFAULT);
    arangodb::application_features::ApplicationServer::server = nullptr;
    arangodb::EngineSelectorFeature::ENGINE = nullptr;

    // destroy application features
    for (auto& f : features) {
      if (f.second) {
        f.first->stop();
      }
    }

    for (auto& f : features) {
      f.first->unprepare();
    }

    arangodb::LogTopic::setLogLevel(arangodb::Logger::AUTHENTICATION.name(), arangodb::LogLevel::DEFAULT);
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

//SECTION("test_query") {
//  static std::vector<std::string> const EMPTY;
//  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
//  auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
//  auto viewJson = arangodb::velocypack::Parser::fromJson("{ \
//    \"name\": \"testView\", \
//    \"type\": \"iresearch\", \
//    \"properties\": { \
//      \"links\": { } \
//    } \
//  }");
//  auto* logicalCollection = vocbase.createCollection(collectionJson->slice());
//  REQUIRE((nullptr != logicalCollection));
//  auto logicalView = vocbase.createView(viewJson->slice(), 0);
//  REQUIRE((false == !logicalView));
//  auto* view = logicalView->getImplementation();
//  REQUIRE((false == !view));
//
//  auto links = arangodb::velocypack::Parser::fromJson("{ \
//    \"links\": { \"testCollection\": { \"includeAllFields\" : true } } \
//  }");
//
//  arangodb::Result res = logicalView->updateProperties(links->slice(), true, false);
//  CHECK(true == res.ok());
//  CHECK((false == logicalCollection->getIndexes().empty()));
//
//  // fill with test data
//  {
//    auto doc0 = arangodb::velocypack::Parser::fromJson("{ \"key\": 1, \"testAttr\": \"A\" }");
//    auto doc1 = arangodb::velocypack::Parser::fromJson("{ \"key\": 2, \"testAttr\": \"B\" }");
//    auto doc2 = arangodb::velocypack::Parser::fromJson("{ \"key\": 3, \"testAttr\": \"C\" }");
//    auto doc3 = arangodb::velocypack::Parser::fromJson("{ \"key\": 4, \"testAttr\": 1 }");
//    auto doc4 = arangodb::velocypack::Parser::fromJson("{ \"key\": 5, \"testAttr\": 2.71828 }");
//    auto doc5 = arangodb::velocypack::Parser::fromJson("{ \"key\": 6, \"testAttr\": 3.14159 }");
//    auto doc6 = arangodb::velocypack::Parser::fromJson("{ \"key\": 7, \"testAttr\": true }");
//    auto doc7 = arangodb::velocypack::Parser::fromJson("{ \"key\": 8, \"testAttr\": false }");
//    auto doc8 = arangodb::velocypack::Parser::fromJson("{ \"key\": 9, \"testAttr\": null }");
//    auto doc9 = arangodb::velocypack::Parser::fromJson("{ \"key\": 10, \"testAttr\": [ -1 ] }");
//    auto doc10 = arangodb::velocypack::Parser::fromJson("{ \"key\": 11, \"testAttr\": { \"a\": \"b\" } }");
//    auto doc11 = arangodb::velocypack::Parser::fromJson("{ \"key\": 12 }");
//
//    arangodb::OperationOptions options;
//    arangodb::ManagedDocumentResult tmpResult;
//    TRI_voc_tick_t tmpResultTick;
//    arangodb::transaction::UserTransaction trx(arangodb::transaction::StandaloneContext::Create(&vocbase), EMPTY, EMPTY, EMPTY, arangodb::transaction::Options());
//    CHECK((trx.begin().ok()));
//    CHECK((logicalCollection->insert(&trx, doc0->slice(), tmpResult, options, tmpResultTick, false)).ok());
//    CHECK((logicalCollection->insert(&trx, doc1->slice(), tmpResult, options, tmpResultTick, false)).ok());
//    CHECK((logicalCollection->insert(&trx, doc2->slice(), tmpResult, options, tmpResultTick, false)).ok());
//    CHECK((logicalCollection->insert(&trx, doc3->slice(), tmpResult, options, tmpResultTick, false)).ok());
//    CHECK((logicalCollection->insert(&trx, doc4->slice(), tmpResult, options, tmpResultTick, false)).ok());
//    CHECK((logicalCollection->insert(&trx, doc5->slice(), tmpResult, options, tmpResultTick, false)).ok());
//    CHECK((logicalCollection->insert(&trx, doc6->slice(), tmpResult, options, tmpResultTick, false)).ok());
//    CHECK((logicalCollection->insert(&trx, doc7->slice(), tmpResult, options, tmpResultTick, false)).ok());
//    CHECK((logicalCollection->insert(&trx, doc8->slice(), tmpResult, options, tmpResultTick, false)).ok());
//    CHECK((logicalCollection->insert(&trx, doc9->slice(), tmpResult, options, tmpResultTick, false)).ok());
//    CHECK((logicalCollection->insert(&trx, doc10->slice(), tmpResult, options, tmpResultTick, false)).ok());
//    CHECK((logicalCollection->insert(&trx, doc11->slice(), tmpResult, options, tmpResultTick, false)).ok());
//    CHECK((trx.commit().ok()));
//    dynamic_cast<arangodb::iresearch::IResearchView*>(view)->sync();
//  }
//
//  // query view
//  {
//    // ArangoDB default type sort order:
//    // null < bool < number < string < array/list < object/document
//
//    // string values
//    {
//      std::vector<size_t> const expected = { 9, 8, 7, 4, 5, 6, 1, 2, 3, 10, 11, 12 };
//      std::string query0 = "FOR d IN testCollection FILTER d.key >= 1 SORT 'testAttr' RETURN d";
//      std::string query1 = "FOR d IN testCollection FILTER d.key >= 1 SORT d['testAttr'] RETURN d";
//      std::string query2 = "FOR d IN testCollection FILTER d.key >= 1 SORT d.testAttr RETURN d";
//
//      assertOrderSuccess(*logicalView, query0, "key", expected);
//      assertOrderSuccess(*logicalView, query1, "key", expected);
//      assertOrderSuccess(*logicalView, query2, "key", expected);
//    }
//
//    // array values
//    {
//      std::vector<size_t> const expected = { 10, 1, 2, 3, 4, 5, 6, 7, 8, 9, 11, 12 };
//      std::string query = "FOR d IN testCollection FILTER d.key >= 1 SORT d['testAttr'][0] RETURN d";
//
//      assertOrderSuccess(*logicalView, query, "key", expected);
//    }
//
//    // object values
//    {
//      std::vector<size_t> const expected = { 11, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 12 };
//      std::string query0 = "FOR d IN testCollection FILTER d.key >= 1 SORT d['testAttr']['a'] RETURN d";
//      std::string query1 = "FOR d IN testCollection FILTER d.key >= 1 SORT d.testAttr.a RETURN d";
//
//      assertOrderSuccess(*logicalView, query0, "key", expected);
//      assertOrderSuccess(*logicalView, query1, "key", expected);
//    }
//
//    // via function (no precendence)
//    {
//      std::vector<size_t> const expected = { 9, 8, 7, 4, 5, 6, 1, 2, 3, 10, 11, 12 };
//      std::string query0 = "FOR d IN testCollection FILTER d.key >= 1 SORT `@`('testAttr') RETURN d";
//      std::string query1 = "FOR d IN testCollection FILTER d.key >= 1 SORT `@`(d['testAttr']) RETURN d";
//      std::string query2 = "FOR d IN testCollection FILTER d.key >= 1 SORT `@`(d.testAttr) RETURN d";
//
//      assertOrderSuccess(*logicalView, query0, "key", expected);
//      assertOrderSuccess(*logicalView, query1, "key", expected);
//      assertOrderSuccess(*logicalView, query2, "key", expected);
//    }
//
//    // via function (with precendence)
//    {
//      std::vector<size_t> const expected0 = { 10, size_t(-1), 1, 2, 3, 4, 5, 6, 7, 8, 9, 11, 12 };
//      std::vector<size_t> const expected1 = { 8, 7, size_t(-1), 1, 2, 3, 4, 5, 6, 9, 10, 11, 12 };
//      std::vector<size_t> const expected2 = { 8, 7, size_t(-1), 1, 2, 3, 4, 5, 6, 9, 10, 11, 12 };
//      std::vector<size_t> const expected3 = { 9, size_t(-1), 1, 2, 3, 4, 5, 6, 7, 8, 10, 11, 12 };
//      std::vector<size_t> const expected4 = { 4, 5, 6, size_t(-1), 1, 2, 3, 7, 8, 9, 10, 11, 12 };
//      std::vector<size_t> const expected5 = { 11, size_t(-1), 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 12 };
//      std::vector<size_t> const expected6 = { 1, 2, 3, size_t(-1), 4, 5, 6, 7, 8, 9, 10, 11, 12 };
//      std::vector<size_t> const expected7 = { 12, size_t(-1), 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };
//      std::vector<size_t> const expected8 = { 10, 8, 7, 9, 4, 5, 6, 11, 1, 2, 3, 12 };
//      std::vector<size_t> const expected9 = { 10, 8, 7, 9, 4, 5, 6, 11, 1, 2, 3, 12 };
//      std::string query0 = "FOR d IN testCollection FILTER d.key >= 1 SORT `@`(d.testAttr, 'array') RETURN d";
//      std::string query1 = "FOR d IN testCollection FILTER d.key >= 1 SORT `@`(d.testAttr, 'bool') RETURN d";
//      std::string query2 = "FOR d IN testCollection FILTER d.key >= 1 SORT `@`(d.testAttr, 'boolean') RETURN d";
//      std::string query3 = "FOR d IN testCollection FILTER d.key >= 1 SORT `@`(d.testAttr, 'null') RETURN d";
//      std::string query4 = "FOR d IN testCollection FILTER d.key >= 1 SORT `@`(d.testAttr, 'numeric') RETURN d";
//      std::string query5 = "FOR d IN testCollection FILTER d.key >= 1 SORT `@`(d.testAttr, 'object') RETURN d";
//      std::string query6 = "FOR d IN testCollection FILTER d.key >= 1 SORT `@`(d.testAttr, 'string') RETURN d";
//      std::string query7 = "FOR d IN testCollection FILTER d.key >= 1 SORT `@`(d.testAttr, 'unknown') RETURN d";
//      std::string query8 = "FOR d IN testCollection FILTER d.key >= 1 SORT `@`(d.testAttr, 'array', 'bool', 'null', 'numeric', 'object', 'string', 'unknown') RETURN d";
//      std::string query9 = "FOR d IN testCollection FILTER d.key >= 1 SORT `@`(d.testAttr, 'array', 'boolean', 'null', 'numeric', 'object', 'string', 'unknown') RETURN d";
//
//      assertOrderSuccess(*logicalView, query0, "key", expected0);
//      assertOrderSuccess(*logicalView, query1, "key", expected1);
//      assertOrderSuccess(*logicalView, query2, "key", expected2);
//      assertOrderSuccess(*logicalView, query3, "key", expected3);
//      assertOrderSuccess(*logicalView, query4, "key", expected4);
//      assertOrderSuccess(*logicalView, query5, "key", expected5);
//      assertOrderSuccess(*logicalView, query6, "key", expected6);
//      assertOrderSuccess(*logicalView, query7, "key", expected7);
//      assertOrderSuccess(*logicalView, query8, "key", expected8);
//      assertOrderSuccess(*logicalView, query9, "key", expected9);
//    }
//
//    // via function (with invalid precedence)
//    {
//      std::string query0 = "FOR d IN testCollection FILTER d.key >= 1 SORT `@`(d.testAttr, []) RETURN d";
//      std::string query1 = "FOR d IN testCollection FILTER d.key >= 1 SORT `@`(d.testAttr, {}) RETURN d";
//      std::string query2 = "FOR d IN testCollection FILTER d.key >= 1 SORT `@`(d.testAttr, 42) RETURN d";
//      std::string query3 = "FOR d IN testCollection FILTER d.key >= 1 SORT `@`(d.testAttr, 'abc') RETURN d";
//
//      assertOrderFail(*logicalView, query0, TRI_ERROR_NO_ERROR);
//      assertOrderFail(*logicalView, query1, TRI_ERROR_NO_ERROR);
//      assertOrderFail(*logicalView, query2, TRI_ERROR_NO_ERROR);
//      assertOrderFail(*logicalView, query3, TRI_ERROR_NO_ERROR);
//    }
//  }
//
//  // query view ascending
//  {
//    // ArangoDB default type sort order:
//    // null < bool < number < string < array/list < object/document
//
//    // string values
//    {
//      std::vector<size_t> const expected = { 9, 8, 7, 4, 5, 6, 1, 2, 3, 10, 11, 12 };
//      std::string query0 = "FOR d IN testCollection FILTER d.key >= 1 SORT 'testAttr' ASC RETURN d";
//      std::string query1 = "FOR d IN testCollection FILTER d.key >= 1 SORT d['testAttr'] ASC RETURN d";
//      std::string query2 = "FOR d IN testCollection FILTER d.key >= 1 SORT d.testAttr ASC RETURN d";
//
//      assertOrderSuccess(*logicalView, query0, "key", expected);
//      assertOrderSuccess(*logicalView, query1, "key", expected);
//      assertOrderSuccess(*logicalView, query2, "key", expected);
//    }
//
//    // array values
//    {
//      std::vector<size_t> const expected = { 10, 1, 2, 3, 4, 5, 6, 7, 8, 9, 11, 12 };
//      std::string query = "FOR d IN testCollection FILTER d.key >= 1 SORT d['testAttr'][0] ASC RETURN d";
//
//      assertOrderSuccess(*logicalView, query, "key", expected);
//    }
//
//    // object values
//    {
//      std::vector<size_t> const expected =  { 11, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 12 };
//      std::string query0 = "FOR d IN testCollection FILTER d.key >= 1 SORT d['testAttr']['a'] ASC RETURN d";
//      std::string query1 = "FOR d IN testCollection FILTER d.key >= 1 SORT d.testAttr.a ASC RETURN d";
//
//      assertOrderSuccess(*logicalView, query0, "key", expected);
//      assertOrderSuccess(*logicalView, query1, "key", expected);
//    }
//  }
//
//  // query view descending
//  {
//    // ArangoDB default type sort order:
//    // null < bool < number < string < array/list < object/document
//
//    // string values
//    {
//      std::vector<size_t> const expected = { 12, 11, 10, 3, 2, 1, 6, 5, 4, 7, 8, 9 };
//      std::string query0 = "FOR d IN testCollection FILTER d.key >= 1 SORT 'testAttr' DESC RETURN d";
//      std::string query1 = "FOR d IN testCollection FILTER d.key >= 1 SORT d['testAttr'] DESC RETURN d";
//      std::string query2 = "FOR d IN testCollection FILTER d.key >= 1 SORT d.testAttr DESC RETURN d";
//
//      assertOrderSuccess(*logicalView, query0, "key", expected);
//      assertOrderSuccess(*logicalView, query1, "key", expected);
//      assertOrderSuccess(*logicalView, query2, "key", expected);
//    }
//
//    // array values
//    {
//      std::vector<size_t> const expected = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 11, 12, 10 };
//      std::string query = "FOR d IN testCollection FILTER d.key >= 1 SORT d['testAttr'][0] DESC RETURN d";
//
//      assertOrderSuccess(*logicalView, query, "key", expected);
//    }
//
//    // object values
//    {
//      std::vector<size_t> const expected =  { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 12, 11 };
//      std::string query0 = "FOR d IN testCollection FILTER d.key >= 1 SORT d['testAttr']['a'] DESC RETURN d";
//      std::string query1 = "FOR d IN testCollection FILTER d.key >= 1 SORT d.testAttr.a DESC RETURN d";
//
//      assertOrderSuccess(*logicalView, query0, "key", expected);
//      assertOrderSuccess(*logicalView, query1, "key", expected);
//    }
//  }
//}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate tests
////////////////////////////////////////////////////////////////////////////////

}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
