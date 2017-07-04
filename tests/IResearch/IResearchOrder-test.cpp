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
#include "StorageEngineMock.h"

#include "Aql/Ast.h"
#include "Aql/AqlFunctionFeature.h"
#include "Aql/Query.h"
#include "Aql/SortCondition.h"
#include "IResearch/AttributeScorer.h"
#include "IResearch/IResearchDocument.h"
#include "IResearch/IResearchViewMeta.h"
#include "RestServer/AqlFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "RestServer/TraverserEngineRegistryFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "Transaction/StandaloneContext.h"
#include "Transaction/UserTransaction.h"

#include "search/scorers.hpp"

NS_LOCAL

struct dummy_scorer: public irs::sort {
  DECLARE_SORT_TYPE() { static irs::sort::type_id type("TEST::TFIDF"); return type; }
  static ptr make(const irs::string_ref&) { PTR_NAMED(dummy_scorer, ptr); return ptr; }
  dummy_scorer(): irs::sort(dummy_scorer::type()) { }
  virtual sort::prepared::ptr prepare() const override { return nullptr; }
};

REGISTER_SCORER(dummy_scorer);

void assertOrderSuccess(std::string const& queryString, irs::order const& expected) {
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");

  std::shared_ptr<arangodb::velocypack::Builder> bindVars;
  auto options = std::make_shared<arangodb::velocypack::Builder>();

  arangodb::aql::Query query(
     false, &vocbase, arangodb::aql::QueryString(queryString),
     bindVars, options,
     arangodb::aql::PART_MAIN
  );

  auto const parseResult = query.parse();
  REQUIRE(TRI_ERROR_NO_ERROR == parseResult.code);

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

  static std::vector<std::string> const EMPTY;
  arangodb::transaction::Options trxOptions;
  trxOptions.waitForSync = false;
  trxOptions.allowImplicitCollections = false;

  arangodb::transaction::UserTransaction trx(arangodb::transaction::StandaloneContext::Create(&vocbase), EMPTY, EMPTY, EMPTY, trxOptions);
  irs::order actual;
  arangodb::iresearch::OrderFactory::OrderContext ctx { actual, trx };
  arangodb::aql::SortCondition order(nullptr, sorts, attrs, variableNodes);
  arangodb::iresearch::IResearchViewMeta meta;

  CHECK((arangodb::iresearch::OrderFactory::order(nullptr, order, meta)));
  CHECK((arangodb::iresearch::OrderFactory::order(&ctx, order, meta)));
  // FIXME TODO uncomment once iResearch code is updated with irs::order::operator==(...)
  //CHECK((expected == actual));
}

void assertOrderFail(std::string const& queryString, size_t parseCode) {
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

  static std::vector<std::string> const EMPTY;
  arangodb::transaction::Options trxOptions;
  trxOptions.waitForSync = false;
  trxOptions.allowImplicitCollections = false;

  arangodb::transaction::UserTransaction trx(arangodb::transaction::StandaloneContext::Create(&vocbase), EMPTY, EMPTY, EMPTY, trxOptions);
  irs::order actual;
  arangodb::iresearch::OrderFactory::OrderContext ctx { actual, trx };
  arangodb::aql::SortCondition order(nullptr, sorts, attrs, variableNodes);
  arangodb::iresearch::IResearchViewMeta meta;

  CHECK((!arangodb::iresearch::OrderFactory::order(nullptr, order, meta)));
  CHECK((!arangodb::iresearch::OrderFactory::order(&ctx, order, meta)));
}

NS_END

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

struct IResearchOrderSetup {
  StorageEngineMock engine;
  arangodb::application_features::ApplicationServer server;

  IResearchOrderSetup(): server(nullptr, nullptr) {
    arangodb::EngineSelectorFeature::ENGINE = &engine;
    arangodb::application_features::ApplicationFeature* feature;

    // AqlFeature
    arangodb::application_features::ApplicationServer::server->addFeature(
      feature = new arangodb::AqlFeature(&server)
    );
    feature->start();
    feature->prepare();

    // QueryRegistryFeature
    arangodb::application_features::ApplicationServer::server->addFeature(
      feature = new arangodb::QueryRegistryFeature(&server)
    );
    feature->start();
    feature->prepare();

    // TraverserEngineRegistryFeature (required for AqlFeature::stop() to work)
    arangodb::application_features::ApplicationServer::server->addFeature(
      feature = new arangodb::TraverserEngineRegistryFeature(&server)
    );
    feature->start();
    feature->prepare();

    // AqlFunctionFeature
    arangodb::application_features::ApplicationServer::server->addFeature(
      feature = new arangodb::aql::AqlFunctionFeature(&server)
    );
    feature->start();
    feature->prepare();

    // external function names must be registred in upper-case
    // user defined functions have ':' in the external function name
    // function arguments string format: requiredArg1[,requiredArg2]...[|optionalArg1[,optionalArg2]...]
    auto& functions = *arangodb::aql::AqlFunctionFeature::AQLFUNCTIONS;
    arangodb::aql::Function valid("TFIDF", "internalName", "", false, false, true, true, false);
    arangodb::aql::Function invalid("INVALID", "internalName", "", false, false, true, true, false);

    functions.add(valid);
    functions.add(invalid);

    // initialize error messages, avoiding duplicate error message definitions
    // required to pass assert checking for existance of error message
    if (std::string("unknown error") == TRI_errno_string(0)) {
      TRI_InitializeErrorMessages();
    }
  }

  ~IResearchOrderSetup() {
    arangodb::aql::AqlFunctionFeature(&server).unprepare(); // unset singleton instance
    arangodb::AqlFeature(&server).stop(); // unset singleton instance
    arangodb::application_features::ApplicationServer::server = nullptr;
    arangodb::EngineSelectorFeature::ENGINE = nullptr;
  }
}; // IResearchOrderSetup

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief setup
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("IResearchOrderTest", "[iresearch][iresearch-order]") {
  IResearchOrderSetup s;
  UNUSED(s);

SECTION("test_FCall") {
  // function
  {
    std::string query = "FOR d IN collection FILTER '1' SORT tfidf() RETURN d";
    irs::order expected;

    expected.add(irs::scorers::get("tfidf", irs::string_ref::nil));
    assertOrderSuccess(query, expected);
  }

  // function ASC
  {
    std::string query = "FOR d IN collection FILTER '1' SORT tfidf() ASC RETURN d";
    irs::order expected;

    expected.add(irs::scorers::get("tfidf", irs::string_ref::nil));
    assertOrderSuccess(query, expected);
  }

  // function DESC
  {
    std::string query = "FOR d IN collection FILTER '1' SORT tfidf() DESC RETURN d";
    irs::order expected;
    auto scorer = irs::scorers::get("tfidf", irs::string_ref::nil);

    scorer->reverse(true);
    expected.add(scorer);
    assertOrderSuccess(query, expected);
  }

  // invalid function (not an iResearch function)
  {
    std::string query = "FOR d IN collection FILTER '1' SORT invalid() RETURN d";

    assertOrderFail(query, TRI_ERROR_NO_ERROR);
  }

  // undefined function (not a function registered with ArangoDB)
  {
    std::string query = "FOR d IN collection FILTER '1' SORT undefined() RETURN d";

    assertOrderFail(query, TRI_ERROR_QUERY_FUNCTION_NAME_UNKNOWN);
  }
}

SECTION("test_FCallUser") {
  // function
  {
    std::string query = "FOR d IN collection FILTER '1' SORT test::tfidf() RETURN d";
    irs::order expected;

    expected.add<dummy_scorer>(irs::string_ref::nil);
    assertOrderSuccess(query, expected);
  }

  // function ASC
  {
    std::string query = "FOR d IN collection FILTER '1' SORT test::tfidf() ASC RETURN d";
    irs::order expected;

    expected.add<dummy_scorer>(irs::string_ref::nil);
    assertOrderSuccess(query, expected);
  }

  // function DESC
  {
    std::string query = "FOR d IN collection FILTER '1' SORT test::tfidf() DESC RETURN d";
    irs::order expected;

    expected.add<dummy_scorer>(irs::string_ref::nil).reverse(true);
    assertOrderSuccess(query, expected);
  }

  // invalid function (not an iResearch function)
  {
    std::string query = "FOR d IN collection FILTER '1' SORT test::invalid() DESC RETURN d";

    assertOrderFail(query, TRI_ERROR_NO_ERROR);
  }
}

SECTION("test_StringValue") {
  static std::vector<std::string> const EMPTY;
  arangodb::transaction::Options trxOptions;
  trxOptions.waitForSync = false;
  trxOptions.allowImplicitCollections = false;

  arangodb::transaction::UserTransaction trx(arangodb::transaction::StandaloneContext::Create(nullptr), EMPTY, EMPTY, EMPTY, trxOptions);

  // simple field
  {
    std::string query = "FOR d IN collection FILTER '1' SORT 'a' RETURN d";
    irs::order expected;

    expected.add<arangodb::iresearch::AttributeScorer>(trx, "a");
    assertOrderSuccess(query, expected);
  }

  // simple field ASC
  {
    std::string query = "FOR d IN collection FILTER '1' SORT 'a' ASC RETURN d";
    irs::order expected;

    expected.add<arangodb::iresearch::AttributeScorer>(trx, "a");
    assertOrderSuccess(query, expected);
  }

  // simple field DESC
  {
    std::string query = "FOR d IN collection FILTER '1' SORT 'a' DESC RETURN d";
    irs::order expected;

    expected.add<arangodb::iresearch::AttributeScorer>(trx, "a").reverse(true);
    assertOrderSuccess(query, expected);
  }

  // nested field
  {
    std::string query = "FOR d IN collection FILTER '1' SORT 'a.b.c' RETURN d";
    irs::order expected;

    expected.add<arangodb::iresearch::AttributeScorer>(trx, "a.b.c");
    assertOrderSuccess(query, expected);
  }

  // nested field ASC
  {
    std::string query = "FOR d IN collection FILTER '1' SORT 'a.b.c' ASC RETURN d";
    irs::order expected;

    expected.add<arangodb::iresearch::AttributeScorer>(trx, "a.b.c");
    assertOrderSuccess(query, expected);
  }

  // nested field DESC
  {
    std::string query = "FOR d IN collection FILTER '1' SORT 'a.b.c' DESC RETURN d";
    irs::order expected;

    expected.add<arangodb::iresearch::AttributeScorer>(trx, "a.b.c").reverse(true);
    assertOrderSuccess(query, expected);
  }
}

SECTION("test_order") {
  // test empty sort
  {
    std::vector<std::vector<arangodb::basics::AttributeName>> attrs;
    std::vector<std::pair<arangodb::aql::Variable const*, bool>> sorts;
    std::unordered_map<arangodb::aql::VariableId, arangodb::aql::AstNode const*> variableNodes;
    std::vector<arangodb::aql::Variable> variables;

    static std::vector<std::string> const EMPTY;
    arangodb::transaction::Options trxOptions;
    trxOptions.waitForSync = false;
    trxOptions.allowImplicitCollections = false;

    arangodb::transaction::UserTransaction trx(arangodb::transaction::StandaloneContext::Create(nullptr), EMPTY, EMPTY, EMPTY, trxOptions);
    irs::order actual;
    arangodb::iresearch::OrderFactory::OrderContext ctx { actual, trx };
    arangodb::aql::SortCondition order(nullptr, sorts, attrs, variableNodes);
    arangodb::iresearch::IResearchViewMeta meta;

    CHECK((arangodb::iresearch::OrderFactory::order(nullptr, order, meta)));
    CHECK((arangodb::iresearch::OrderFactory::order(&ctx, order, meta)));
    CHECK((0 == actual.size()));
  }

  // test mutiple sort
  {
    std::string query = "FOR d IN collection FILTER '1' SORT test::tfidf(), tfidf() RETURN d";
    irs::order expected;

    expected.add<dummy_scorer>(irs::string_ref::nil).reverse(true);
    expected.add(irs::scorers::get("tfidf", irs::string_ref::nil));
    assertOrderSuccess(query, expected);
  }

  // invalid field
  {
    std::string query = "FOR d IN collection FILTER '1' SORT a RETURN d";

    assertOrderFail(query, TRI_ERROR_NO_ERROR);
  }

}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate tests
////////////////////////////////////////////////////////////////////////////////

}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// ----------------------------------------------------------------------------