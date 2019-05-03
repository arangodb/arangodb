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
#include "../Mocks/StorageEngineMock.h"
#include "ExpressionContextMock.h"

#include "Aql/AqlFunctionFeature.h"
#include "Aql/Ast.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/IResearchViewNode.h"
#include "Aql/Query.h"
#include "Aql/SortCondition.h"
#include "IResearch/AqlHelper.h"
#include "IResearch/IResearchCommon.h"
#include "IResearch/IResearchFeature.h"
#include "IResearch/IResearchOrderFactory.h"
#include "RestServer/AqlFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "RestServer/TraverserEngineRegistryFeature.h"
#include "RestServer/ViewTypesFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "Transaction/Methods.h"
#include "Transaction/StandaloneContext.h"

#include "search/scorers.hpp"
#include "utils/misc.hpp"

namespace {

struct dummy_scorer: public irs::sort {
  static std::function<bool(irs::string_ref const&)> validateArgs;
  DECLARE_SORT_TYPE() { static irs::sort::type_id type("TEST::TFIDF"); return type; }
  static ptr make(const irs::string_ref& args) { if (!validateArgs(args)) return nullptr; PTR_NAMED(dummy_scorer, ptr); return ptr; }
  dummy_scorer(): irs::sort(dummy_scorer::type()) { }
  virtual sort::prepared::ptr prepare() const override { return nullptr; }
};

/*static*/ std::function<bool(irs::string_ref const&)> dummy_scorer::validateArgs =
  [](irs::string_ref const&)->bool { return true; };

REGISTER_SCORER_JSON(dummy_scorer, dummy_scorer::make);

void assertOrder(
    bool parseOk,
    bool execOk,
    std::string const& queryString,
    irs::order const& expected,
    arangodb::aql::ExpressionContext* exprCtx = nullptr,
    std::shared_ptr<arangodb::velocypack::Builder> bindVars = nullptr,
    std::string const& refName = "d") {
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");

  arangodb::aql::Query query(
    false,
    vocbase,
    arangodb::aql::QueryString(queryString),
    bindVars,
    std::make_shared<arangodb::velocypack::Builder>(),
    arangodb::aql::PART_MAIN
  );

  auto const parseResult = query.parse();
  REQUIRE(parseResult.result.ok());

  auto* ast = query.ast();
  REQUIRE(ast);

  auto* root = ast->root();
  REQUIRE(root);

  // find first SORT node
  arangodb::aql::AstNode* orderNode = nullptr;
  for (size_t i = 0; i < root->numMembers(); ++i) {
    auto* node = root->getMemberUnchecked(i);
    REQUIRE(node);

    if (arangodb::aql::NODE_TYPE_SORT == node->type) {
      orderNode = node;
      break;
    }
  }
  REQUIRE((orderNode && arangodb::aql::NODE_TYPE_SORT == orderNode->type));

  auto* sortNode = orderNode->getMember(0);
  REQUIRE(sortNode);

  // find referenced variable
  auto* allVars = ast->variables();
  REQUIRE(allVars);
  arangodb::aql::Variable* ref = nullptr;
  for (auto entry : allVars->variables(true)) {
    if (entry.second == refName) {
      ref = allVars->getVariable(entry.first);
      break;
    }
  }
  REQUIRE(ref);

  // optimization time check
  {
    arangodb::iresearch::QueryContext const ctx {
      nullptr, nullptr, nullptr, nullptr, ref
    };

    for (size_t i = 0, count = sortNode->numMembers(); i < count; ++i) {
      auto const* sort = sortNode->getMember(i);
      auto const* expr = sort->getMember(0);

      CHECK(parseOk == arangodb::iresearch::OrderFactory::scorer(nullptr, *expr, ctx));
    }
  }

  // execution time check
  {
    irs::order actual;
    irs::sort::ptr actualScorer;

    auto dummyPlan = arangodb::tests::planFromQuery(vocbase, "RETURN 1");

    arangodb::transaction::Methods trx(
      arangodb::transaction::StandaloneContext::Create(vocbase),
      {},
      {},
      {},
      arangodb::transaction::Options()
    );

    arangodb::iresearch::QueryContext const ctx{
      &trx, dummyPlan.get(), ast, exprCtx, ref
    };

    for (size_t i = 0, count = sortNode->numMembers(); i < count; ++i) {
      auto const* sort = sortNode->getMember(i);
      auto const* expr = sort->getMember(0);
      auto const asc = sort->getMember(1)->getBoolValue();

      CHECK(execOk == arangodb::iresearch::OrderFactory::scorer(&actualScorer, *expr, ctx));

      if (execOk) {
        actual.add(!asc, std::move(actualScorer));
      }
    }
    CHECK((!execOk || expected == actual));
  }
}

void assertOrderSuccess(
    std::string const& queryString,
    irs::order const& expected,
    arangodb::aql::ExpressionContext* exprCtx = nullptr,
    std::shared_ptr<arangodb::velocypack::Builder> bindVars = nullptr,
    std::string const& refName = "d") {
  return assertOrder(true, true, queryString, expected, exprCtx, bindVars, refName);
}

void assertOrderFail(
    std::string const& queryString,
    arangodb::aql::ExpressionContext* exprCtx = nullptr,
    std::shared_ptr<arangodb::velocypack::Builder> bindVars = nullptr,
    std::string const& refName = "d") {
  irs::order expected;
  return assertOrder(false, false, queryString, expected, exprCtx, bindVars, refName);
}

void assertOrderExecutionFail(
    std::string const& queryString,
    arangodb::aql::ExpressionContext* exprCtx = nullptr,
    std::shared_ptr<arangodb::velocypack::Builder> bindVars = nullptr,
    std::string const& refName = "d") {
  irs::order expected;
  return assertOrder(true, false, queryString, expected, exprCtx, bindVars, refName);
}

void assertOrderParseFail(std::string const& queryString, size_t parseCode) {
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");

  arangodb::aql::Query query(
    false,
    vocbase,
    arangodb::aql::QueryString(queryString),
    nullptr,
    nullptr,
    arangodb::aql::PART_MAIN
  );

  auto const parseResult = query.parse();
  REQUIRE(parseCode == parseResult.result.errorNumber());
}

}

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

struct IResearchOrderSetup {
  StorageEngineMock engine;
  arangodb::application_features::ApplicationServer server;
  std::vector<std::pair<arangodb::application_features::ApplicationFeature*, bool>> features;

  IResearchOrderSetup(): engine(server), server(nullptr, nullptr) {
    arangodb::EngineSelectorFeature::ENGINE = &engine;

    arangodb::tests::init();

    // suppress log messages since tests check error conditions
    arangodb::LogTopic::setLogLevel(arangodb::iresearch::TOPIC.name(), arangodb::LogLevel::FATAL);
    irs::logger::output_le(iresearch::logger::IRL_FATAL, stderr);

    // setup required application features
    features.emplace_back(new arangodb::AqlFeature(server), true);
    features.emplace_back(new arangodb::QueryRegistryFeature(server), false);
    features.emplace_back(new arangodb::TraverserEngineRegistryFeature(server), false);
    features.emplace_back(new arangodb::ViewTypesFeature(server), false); // required for IResearchFeature
    features.emplace_back(new arangodb::aql::AqlFunctionFeature(server), true);
    features.emplace_back(new arangodb::iresearch::IResearchFeature(server), true);

    for (auto& f: features) {
      arangodb::application_features::ApplicationServer::server->addFeature(f.first);
    }

    for (auto& f: features) {
      f.first->prepare();
    }

    for (auto& f: features) {
      if (f.second) {
        f.first->start();
      }
    }

    // external function names must be registred in upper-case
    // user defined functions have ':' in the external function name
    // function arguments string format: requiredArg1[,requiredArg2]...[|optionalArg1[,optionalArg2]...]
    auto& functions = *arangodb::aql::AqlFunctionFeature::AQLFUNCTIONS;
    arangodb::aql::Function invalid("INVALID", "|.",
      arangodb::aql::Function::makeFlags(
        arangodb::aql::Function::Flags::CanRunOnDBServer
      )); 

    functions.add(invalid);
  }

  ~IResearchOrderSetup() {
    arangodb::aql::AqlFunctionFeature(server).unprepare(); // unset singleton instance
    arangodb::AqlFeature(server).stop(); // unset singleton instance
    arangodb::LogTopic::setLogLevel(arangodb::iresearch::TOPIC.name(), arangodb::LogLevel::DEFAULT);
    arangodb::application_features::ApplicationServer::server = nullptr;
    arangodb::EngineSelectorFeature::ENGINE = nullptr;

    // destroy application features
    for (auto& f: features) {
      if (f.second) {
        f.first->stop();
      }
    }

    for (auto& f: features) {
      f.first->unprepare();
    }
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
  // invalid function (not an iResearch function)
  {
    std::string query = "FOR d IN collection FILTER '1' SORT invalid(d) RETURN d";

    assertOrderParseFail(query, TRI_ERROR_NO_ERROR);
  }

  // undefined function (not a function registered with ArangoDB)
  {
    std::string query = "FOR d IN collection FILTER '1' SORT undefined(d) RETURN d";

    assertOrderParseFail(query, TRI_ERROR_QUERY_FUNCTION_NAME_UNKNOWN);
  }
}

SECTION("test_FCall_tfidf") {
  // tfidf
  {
    std::string query = "FOR d IN collection FILTER '1' SORT tfidf(d) RETURN d";
    irs::order expected;
    auto scorer = irs::scorers::get("tfidf", irs::text_format::json, irs::string_ref::NIL);

    expected.add(false, scorer); // SortCondition is by default ascending
    assertOrderSuccess(query, expected);
  }

  // tfidf ASC
  {
    std::string query = "FOR d IN collection FILTER '1' SORT tfidf(d) ASC RETURN d";
    irs::order expected;
    auto scorer = irs::scorers::get("tfidf", irs::text_format::json, irs::string_ref::NIL);

    expected.add(false, scorer);
    assertOrderSuccess(query, expected);
  }

  // tfidf DESC
  {
    std::string query = "FOR d IN collection FILTER '1' SORT tfidf(d) DESC RETURN d";
    irs::order expected;
    auto scorer = irs::scorers::get("tfidf", irs::text_format::json, irs::string_ref::NIL);

    expected.add(true, scorer);
    assertOrderSuccess(query, expected);
  }

  // tfidf with norms
  {
    std::string query = "FOR d IN collection FILTER '1' SORT tfidf(d, true) DESC RETURN d";
    irs::order expected;
    auto scorer = irs::scorers::get("tfidf", irs::text_format::json, "[ true ]");

    expected.add(true, scorer);
    assertOrderSuccess(query, expected);
  }

  // reference as an argument
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("withNorms", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintBool{true}));

    std::string query = "LET withNorms=true FOR d IN collection FILTER '1' SORT tfidf(d, withNorms) DESC RETURN d";
    auto scorer = irs::scorers::get("tfidf", irs::text_format::json, "[ true ]");
    irs::order expected;

    expected.add(true, scorer);

    assertOrderSuccess(query, expected, &ctx);
  }

  // deterministic expression as an argument
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("x", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{5}));

    std::string query = "LET x=5 FOR d IN collection FILTER '1' SORT tfidf(d, 1+x > 3) DESC RETURN d";
    auto scorer = irs::scorers::get("tfidf", irs::text_format::json, "[ true ]");
    irs::order expected;

    expected.add(true, scorer);

    assertOrderSuccess(query, expected, &ctx);
  }

  // non-deterministic expression as an argument
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("x", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{5}));

    std::string query = "LET x=5 FOR d IN collection FILTER '1' SORT tfidf(d, RAND()+x > 3) DESC RETURN d";
    assertOrderFail(query, &ctx);
  }

  // invalid number of arguments function
  {
    std::string query = "FOR d IN collection FILTER '1' SORT tfidf(d, true, false) RETURN d";

    assertOrderExecutionFail(query);
  }

  // invalid reference (invalid output variable reference)
  {
    auto obj = arangodb::velocypack::Parser::fromJson("{}");

    ExpressionContextMock ctx;
    ctx.vars.emplace("c", arangodb::aql::AqlValue(obj->slice()));

    std::string query = "LET c={} FOR d IN collection FILTER '1' SORT tfidf(c) RETURN d";

    assertOrderFail(query, &ctx);
  }

  // invalid function (invalid 1st argument)
  {
    std::string query = "FOR d IN collection FILTER '1' SORT tfidf('d') RETURN d";

    assertOrderFail(query);
  }

  // invalid function (no 1st parameter output variable reference)
  {
    std::string query = "FOR d IN collection FILTER '1' SORT tfidf() RETURN d";

    assertOrderParseFail(query, TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH);
  }
}

SECTION("test_FCall_bm25") {
  // bm25
  {
    std::string query = "FOR d IN collection FILTER '1' SORT bm25(d) RETURN d";
    irs::order expected;
    auto scorer = irs::scorers::get("bm25", irs::text_format::json, irs::string_ref::NIL);

    expected.add(false, scorer); // SortCondition is by default ascending
    assertOrderSuccess(query, expected);
  }

  // bm25 ASC
  {
    std::string query = "FOR d IN collection FILTER '1' SORT bm25(d) ASC RETURN d";
    irs::order expected;
    auto scorer = irs::scorers::get("bm25", irs::text_format::json, irs::string_ref::NIL);

    expected.add(false, scorer);
    assertOrderSuccess(query, expected);
  }

  // bm25 DESC
  {
    std::string query = "FOR d IN collection FILTER '1' SORT bm25(d) DESC RETURN d";
    irs::order expected;
    auto scorer = irs::scorers::get("bm25", irs::text_format::json, irs::string_ref::NIL);

    expected.add(true, scorer);
    assertOrderSuccess(query, expected);
  }

  // bm25 with k coefficient
  {
    std::string query = "FOR d IN collection FILTER '1' SORT bm25(d, 0.99) DESC RETURN d";
    irs::order expected;
    auto scorer = irs::scorers::get("bm25", irs::text_format::json, "[ 0.99 ]");

    expected.add(true, scorer);
    assertOrderSuccess(query, expected);
  }

  // reference as k coefficient
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("k", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{0.99}));

    std::string query = "LET k=0.99 FOR d IN collection FILTER '1' SORT bm25(d, k) DESC RETURN d";
    auto scorer = irs::scorers::get("bm25", irs::text_format::json, "[ 0.99 ]");
    irs::order expected;

    expected.add(true, scorer);

    assertOrderSuccess(query, expected, &ctx);
  }

  // deterministic expression as k coefficient
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("x", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{0.97}));

    std::string query = "LET x=0.97 FOR d IN collection FILTER '1' SORT bm25(d, x+0.02) DESC RETURN d";
    auto scorer = irs::scorers::get("bm25", irs::text_format::json, "[ 0.99 ]");
    irs::order expected;

    expected.add(true, scorer);

    assertOrderSuccess(query, expected, &ctx);
  }

  // non-deterministic expression as k coefficient
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("x", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{0.97}));

    std::string query = "LET x=0.97 FOR d IN collection FILTER '1' SORT bm25(d, RAND()+x) DESC RETURN d";
    assertOrderFail(query, &ctx);
  }

  // bm25 with k coefficient, b coefficient
  {
    std::string query = "FOR d IN collection FILTER '1' SORT bm25(d, 0.99, 1.2) DESC RETURN d";
    auto scorer = irs::scorers::get("bm25", irs::text_format::json, "[ 0.99, 1.2 ]");
    irs::order expected;

    expected.add(true, scorer);

    assertOrderSuccess(query, expected);
  }

  // reference as k,b coefficients
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("k", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{0.97}));
    ctx.vars.emplace("b", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{1.2}));

    std::string query = "LET k=0.97 LET b=1.2 FOR d IN collection FILTER '1' SORT bm25(d, k, b) DESC RETURN d";
    auto scorer = irs::scorers::get("bm25", irs::text_format::json, "[ 0.97, 1.2 ]");
    irs::order expected;

    expected.add(true, scorer);

    assertOrderSuccess(query, expected, &ctx);
  }

  // deterministic expressions as k,b coefficients
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("x", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{0.97}));
    ctx.vars.emplace("y", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{0.1}));

    std::string query = "LET x=0.97 LET y=0.1 FOR d IN collection FILTER '1' SORT bm25(d, x+0.02, 1+y) DESC RETURN d";
    auto scorer = irs::scorers::get("bm25", irs::text_format::json, "[ 0.99, 1.2 ]");
    irs::order expected;

    expected.add(true, scorer);

    assertOrderSuccess(query, expected, &ctx);
  }

  // non-deterministic expression as b coefficient
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("x", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{0.97}));

    std::string query = "LET x=0.97 FOR d IN collection FILTER '1' SORT bm25(d, x, RAND()) DESC RETURN d";
    assertOrderFail(query, &ctx);
  }

  // bm25 with k coefficient, b coefficient
  {
    std::string query = "FOR d IN collection FILTER '1' SORT bm25(d, 0.99, 1.2) DESC RETURN d";
    irs::order expected;
    auto scorer = irs::scorers::get("bm25", irs::text_format::json, "[ 0.99, 1.2 ]");

    expected.add(true, scorer);
    assertOrderSuccess(query, expected);
  }

  // reference as k,b coefficients
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("k", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{0.97}));
    ctx.vars.emplace("b", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{1.2}));
    ctx.vars.emplace("withNorms", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintBool{true}));

    std::string query = "LET k=0.97 LET b=1.2 FOR d IN collection FILTER '1' SORT bm25(d, k, b) DESC RETURN d";
    auto scorer = irs::scorers::get("bm25", irs::text_format::json, "[ 0.97, 1.2 ]");
    irs::order expected;

    expected.add(true, scorer);

    assertOrderSuccess(query, expected, &ctx);
  }

  // deterministic expressions as k,b coefficients
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("x", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{0.97}));
    ctx.vars.emplace("y", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{0.1}));

    std::string query = "LET x=0.97 LET y=0.1 FOR d IN collection FILTER '1' SORT bm25(d, x+0.02, 1+y) DESC RETURN d";
    auto scorer = irs::scorers::get("bm25", irs::text_format::json, "[ 0.99, 1.1 ]");
    irs::order expected;

    expected.add(true, scorer);

    assertOrderSuccess(query, expected, &ctx);
  }

  // non-deterministic expression as b coefficient
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("x", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{0.97}));

    std::string query = "LET x=0.97 FOR d IN collection FILTER '1' SORT bm25(d, x, x, RAND() > 0.5) DESC RETURN d";
    assertOrderFail(query, &ctx);
  }

  // invalid number of arguments function
  {
    std::string query = "FOR d IN collection FILTER '1' SORT bm25(d, 0.97, 0.07, false) RETURN d";

    assertOrderParseFail(query, TRI_ERROR_NO_ERROR);
  }

  // invalid reference (invalid output variable reference)
  {
    auto obj = arangodb::velocypack::Parser::fromJson("{}");

    ExpressionContextMock ctx;
    ctx.vars.emplace("c", arangodb::aql::AqlValue(obj->slice()));

    std::string query = "LET c={} FOR d IN collection FILTER '1' SORT bm25(c) RETURN d";

    assertOrderFail(query, &ctx);
  }

  // invalid function (invalid 1st argument)
  {
    std::string query = "FOR d IN collection FILTER '1' SORT bm25('d') RETURN d";

    assertOrderFail(query);
  }

  // invalid function (no 1st parameter output variable reference)
  {
    std::string query = "FOR d IN collection FILTER '1' SORT bm25() RETURN d";

    assertOrderParseFail(query, TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH);
  }
}

SECTION("test_FCallUser") {
  // function
  {
    std::string query = "FOR d IN collection FILTER '1' SORT test::tfidf(d) RETURN d";
    irs::order expected;

    expected.add<dummy_scorer>(false, irs::string_ref::NIL);
    assertOrderSuccess(query, expected);
  }

  // function string scorer arg (expecting string)
  {
    auto validateOrig = dummy_scorer::validateArgs;
    auto restore = irs::make_finally([&validateOrig]()->void { dummy_scorer::validateArgs = validateOrig; });
    std::string query = "FOR d IN collection FILTER '1' SORT test::tfidf(d, \"abc\") RETURN d";
    irs::order expected;

    expected.add<dummy_scorer>(false, irs::string_ref::NIL);
    dummy_scorer::validateArgs = [](irs::string_ref const& args)->bool {
      CHECK((irs::string_ref("[\"abc\"]") == args));
      return true;
    };
    assertOrderSuccess(query, expected);
  }

  // function string scorer arg (expecting jSON)
  {
    auto validateOrig = dummy_scorer::validateArgs;
    auto restore = irs::make_finally([&validateOrig]()->void { dummy_scorer::validateArgs = validateOrig; });
    std::string query = "FOR d IN collection FILTER '1' SORT test::tfidf(d, \"abc\") RETURN d";
    irs::order expected;
    bool valid = true;

    expected.add<dummy_scorer>(false, irs::string_ref::NIL);

    size_t attempt = 0;
    dummy_scorer::validateArgs = [&valid, &attempt](irs::string_ref const& args)->bool {
      attempt++;
      return valid == (args == "[\"abc\"]");
    };
    assertOrderSuccess(query, expected);
    CHECK((valid));
    CHECK(1 == attempt);
  }

  // function string jSON scorer arg (expecting string)
  {
    auto validateOrig = dummy_scorer::validateArgs;
    auto restore = irs::make_finally([&validateOrig]()->void { dummy_scorer::validateArgs = validateOrig; });
    std::string query = "FOR d IN collection FILTER '1' SORT test::tfidf(d, \"{\\\"abc\\\": \\\"def\\\"}\") RETURN d";
    irs::order expected;

    expected.add<dummy_scorer>(false, irs::string_ref::NIL);

    size_t attempt = 0;
    dummy_scorer::validateArgs = [&attempt](irs::string_ref const& args)->bool {
      attempt++;
      CHECK((irs::string_ref("[\"{\\\"abc\\\": \\\"def\\\"}\"]") == args));
      return true;
    };
    assertOrderSuccess(query, expected);
    CHECK(1 == attempt);
  }

  // function string jSON scorer arg (expecting jSON)
  {
    auto validateOrig = dummy_scorer::validateArgs;
    auto restore = irs::make_finally([&validateOrig]()->void { dummy_scorer::validateArgs = validateOrig; });
    std::string query = "FOR d IN collection FILTER '1' SORT test::tfidf(d, \"{\\\"abc\\\": \\\"def\\\"}\") RETURN d";
    irs::order expected;
    bool valid = true;

    expected.add<dummy_scorer>(false, irs::string_ref::NIL);
    size_t attempt = 0;
    dummy_scorer::validateArgs = [&valid, &attempt](irs::string_ref const& args)->bool {
      attempt++;
      valid = irs::string_ref("[\"{\\\"abc\\\": \\\"def\\\"}\"]") == args;
      return valid;
    };
    assertOrderSuccess(query, expected);
    CHECK((valid));
    CHECK(1 == attempt);
  }

  // function raw jSON scorer arg
  {
    auto validateOrig = dummy_scorer::validateArgs;
    auto restore = irs::make_finally([&validateOrig]()->void { dummy_scorer::validateArgs = validateOrig; });
    std::string query = "FOR d IN collection FILTER '1' SORT test::tfidf(d, {\"abc\": \"def\"}) RETURN d";
    irs::order expected;

    expected.add<dummy_scorer>(false, irs::string_ref::NIL);
    size_t attempt = 0;
    dummy_scorer::validateArgs = [&attempt](irs::string_ref const& args)->bool {
      ++attempt;
      CHECK((irs::string_ref("[{\"abc\":\"def\"}]") == args));
      return true;
    };
    assertOrderSuccess(query, expected);
    CHECK(1 == attempt);
  }

  // function 2 string scorer args
  {
    auto validateOrig = dummy_scorer::validateArgs;
    auto restore = irs::make_finally([&validateOrig]()->void { dummy_scorer::validateArgs = validateOrig; });
    std::string query = "FOR d IN collection FILTER '1' SORT test::tfidf(d, \"abc\", \"def\") RETURN d";
    irs::order expected;

    expected.add<dummy_scorer>(false, irs::string_ref::NIL);
    size_t attempt = 0;
    dummy_scorer::validateArgs = [&attempt](irs::string_ref const& args)->bool {
      ++attempt;
      CHECK((irs::string_ref("[\"abc\",\"def\"]") == args));
      return true;
    };
    assertOrderSuccess(query, expected);
    CHECK(1 == attempt);
  }

  // function string+jSON(string) scorer args
  {
    auto validateOrig = dummy_scorer::validateArgs;
    auto restore = irs::make_finally([&validateOrig]()->void { dummy_scorer::validateArgs = validateOrig; });
    std::string query = "FOR d IN collection FILTER '1' SORT test::tfidf(d, \"abc\", \"{\\\"def\\\": \\\"ghi\\\"}\") RETURN d";
    irs::order expected;

    expected.add<dummy_scorer>(false, irs::string_ref::NIL);
    size_t attempt = 0;
    dummy_scorer::validateArgs = [&attempt](irs::string_ref const& args)->bool {
      ++attempt;
      CHECK((irs::string_ref("[\"abc\",\"{\\\"def\\\": \\\"ghi\\\"}\"]") == args));
      return true;
    };
    assertOrderSuccess(query, expected);
    CHECK(1 == attempt);
  }

  // function string+jSON(raw) scorer args
  {
    auto validateOrig = dummy_scorer::validateArgs;
    auto restore = irs::make_finally([&validateOrig]()->void { dummy_scorer::validateArgs = validateOrig; });
    std::string query = "FOR d IN collection FILTER '1' SORT test::tfidf(d, \"abc\", {\"def\": \"ghi\"}) RETURN d";
    irs::order expected;

    expected.add<dummy_scorer>(false, irs::string_ref::NIL);
    size_t attempt = 0;
    dummy_scorer::validateArgs = [&attempt](irs::string_ref const& args)->bool {
      ++attempt;
      CHECK((irs::string_ref("[\"abc\",{\"def\":\"ghi\"}]") == args));
      return true;
    };
    assertOrderSuccess(query, expected);
    CHECK(1 == attempt);
  }

  // function ASC
  {
    std::string query = "FOR d IN collection FILTER '1' SORT test::tfidf(d) ASC RETURN d";
    irs::order expected;

    expected.add<dummy_scorer>(false, irs::string_ref::NIL);
    assertOrderSuccess(query, expected);
  }

  // function DESC
  {
    std::string query = "FOR d IN collection FILTER '1' SORT test::tfidf(d) DESC RETURN d";
    irs::order expected;

    expected.add<dummy_scorer>(true, irs::string_ref::NIL);
    assertOrderSuccess(query, expected);
  }

  // invalid function (no 1st parameter output variable reference)
  {
    std::string query = "FOR d IN collection FILTER '1' SORT test::tfidf() RETURN d";

    assertOrderFail(query);
  }

  // invalid function (not an iResearch function)
  {
    std::string query = "FOR d IN collection FILTER '1' SORT test::invalid(d) DESC RETURN d";

    assertOrderFail(query);
  }
}

SECTION("test_StringValue") {
  // simple field
  {
    std::string query = "FOR d IN collection FILTER '1' SORT 'a' RETURN d";
    assertOrderFail(query);
  }

  // simple field ASC
  {
    std::string query = "FOR d IN collection FILTER '1' SORT 'a' ASC RETURN d";
    assertOrderFail(query);
  }

  // simple field DESC
  {
    std::string query = "FOR d IN collection FILTER '1' SORT 'a' DESC RETURN d";
    assertOrderFail(query);
  }

  // nested field
  {
    std::string query = "FOR d IN collection FILTER '1' SORT 'a.b.c' RETURN d";
    assertOrderFail(query);
  }

  // nested field ASC
  {
    std::string query = "FOR d IN collection FILTER '1' SORT 'a.b.c' ASC RETURN d";
    assertOrderFail(query);
  }

  // nested field DESC
  {
    std::string query = "FOR d IN collection FILTER '1' SORT 'a.b.c' DESC RETURN d";
    assertOrderFail(query);
  }
}

SECTION("test_order") {
  // test mutiple sort
  {
    std::string query = "FOR d IN collection FILTER '1' SORT test::tfidf(d) DESC, tfidf(d) RETURN d";
    irs::order expected;
    auto scorer = irs::scorers::get("tfidf", irs::text_format::json, irs::string_ref::NIL);

    expected.add<dummy_scorer>(true, irs::string_ref::NIL);
    expected.add(false, scorer);
    assertOrderSuccess(query, expected);
  }

  // invalid field
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{5}));
    std::string query = "LET a=1 FOR d IN collection FILTER '1' SORT a RETURN d";

    assertOrderFail(query, &ctx);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate tests
////////////////////////////////////////////////////////////////////////////////

}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
