////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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

#include "gtest/gtest.h"

#include "search/scorers.hpp"
#include "utils/misc.hpp"

#include <velocypack/Parser.h>

#include "IResearch/ExpressionContextMock.h"
#include "IResearch/common.h"
#include "Mocks/LogLevels.h"
#include "Mocks/StorageEngineMock.h"

#include "ApplicationFeatures/ApplicationServer.h"
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
#include "RestServer/DatabaseFeature.h"
#include "RestServer/MetricsFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "RestServer/ViewTypesFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "Transaction/Methods.h"
#include "Transaction/StandaloneContext.h"

namespace {

struct dummy_scorer : public irs::sort {
  static std::function<bool(irs::string_ref const&)> validateArgs;
  static constexpr irs::string_ref type_name() noexcept {
    return "TEST::TFIDF";
  }
  static ptr make(const irs::string_ref& args) {
    if (!validateArgs(args)) return nullptr;
    PTR_NAMED(dummy_scorer, ptr);
    return ptr;
  }
  dummy_scorer() : irs::sort(irs::type<dummy_scorer>::get()) {}
  virtual sort::prepared::ptr prepare() const override { return nullptr; }
};

/*static*/ std::function<bool(irs::string_ref const&)> dummy_scorer::validateArgs =
    [](irs::string_ref const&) -> bool { return true; };

REGISTER_SCORER_JSON(dummy_scorer, dummy_scorer::make);

void assertOrder(arangodb::application_features::ApplicationServer& server, bool parseOk,
                 bool execOk, std::string const& queryString, irs::order const& expected,
                 arangodb::aql::ExpressionContext* exprCtx = nullptr,
                 std::shared_ptr<arangodb::velocypack::Builder> bindVars = nullptr,
                 std::string const& refName = "d") {
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server));

  arangodb::aql::Query query(arangodb::transaction::StandaloneContext::Create(vocbase),
                             arangodb::aql::QueryString(queryString), bindVars,
                             std::make_shared<arangodb::velocypack::Builder>());

  auto const parseResult = query.parse();
  ASSERT_TRUE(parseResult.result.ok());

  auto* ast = query.ast();
  ASSERT_TRUE(ast);

  auto* root = ast->root();
  ASSERT_TRUE(root);

  // find first SORT node
  arangodb::aql::AstNode* orderNode = nullptr;
  for (size_t i = 0; i < root->numMembers(); ++i) {
    auto* node = root->getMemberUnchecked(i);
    ASSERT_TRUE(node);

    if (arangodb::aql::NODE_TYPE_SORT == node->type) {
      orderNode = node;
      break;
    }
  }
  ASSERT_TRUE((orderNode && arangodb::aql::NODE_TYPE_SORT == orderNode->type));

  auto* sortNode = orderNode->getMember(0);
  ASSERT_TRUE(sortNode);

  // find referenced variable
  auto* allVars = ast->variables();
  ASSERT_TRUE(allVars);
  arangodb::aql::Variable* ref = nullptr;
  for (auto entry : allVars->variables(true)) {
    if (entry.second == refName) {
      ref = allVars->getVariable(entry.first);
      break;
    }
  }
  ASSERT_TRUE(ref);

  // optimization time check
  {
    arangodb::iresearch::QueryContext const ctx{nullptr, nullptr, nullptr, nullptr, nullptr, ref};

    for (size_t i = 0, count = sortNode->numMembers(); i < count; ++i) {
      auto const* sort = sortNode->getMember(i);
      auto const* expr = sort->getMember(0);

      EXPECT_TRUE(parseOk == arangodb::iresearch::OrderFactory::scorer(nullptr, *expr, ctx));
    }
  }

  // execution time check
  {
    irs::order actual;
    irs::sort::ptr actualScorer;

    auto dummyPlan = arangodb::tests::planFromQuery(vocbase, "RETURN 1");

    arangodb::transaction::Methods trx(arangodb::transaction::StandaloneContext::Create(vocbase),
                                       {}, {}, {}, arangodb::transaction::Options());
    
    auto* mockCtx = dynamic_cast<ExpressionContextMock*>(exprCtx);
    if (mockCtx) {  // simon: hack to make expression context work again
      mockCtx->setTrx(&trx);
    }

    arangodb::iresearch::QueryContext const ctx{&trx, dummyPlan.get(), ast, exprCtx, &irs::sub_reader::empty(), ref};

    for (size_t i = 0, count = sortNode->numMembers(); i < count; ++i) {
      auto const* sort = sortNode->getMember(i);
      auto const* expr = sort->getMember(0);
      auto const asc = sort->getMember(1)->getBoolValue();

      EXPECT_TRUE(execOk == arangodb::iresearch::OrderFactory::scorer(&actualScorer,
                                                                      *expr, ctx));

      if (execOk) {
        actual.add(!asc, std::move(actualScorer));
      }
    }
    EXPECT_TRUE((!execOk || expected == actual));
  }
}

void assertOrderSuccess(arangodb::application_features::ApplicationServer& server,
                        std::string const& queryString, irs::order const& expected,
                        arangodb::aql::ExpressionContext* exprCtx = nullptr,
                        std::shared_ptr<arangodb::velocypack::Builder> bindVars = nullptr,
                        std::string const& refName = "d") {
  return assertOrder(server, true, true, queryString, expected, exprCtx, bindVars, refName);
}

void assertOrderFail(arangodb::application_features::ApplicationServer& server,
                     std::string const& queryString,
                     arangodb::aql::ExpressionContext* exprCtx = nullptr,
                     std::shared_ptr<arangodb::velocypack::Builder> bindVars = nullptr,
                     std::string const& refName = "d") {
  irs::order expected;
  return assertOrder(server, false, false, queryString, expected, exprCtx, bindVars, refName);
}

void assertOrderExecutionFail(arangodb::application_features::ApplicationServer& server,
                              std::string const& queryString,
                              arangodb::aql::ExpressionContext* exprCtx = nullptr,
                              std::shared_ptr<arangodb::velocypack::Builder> bindVars = nullptr,
                              std::string const& refName = "d") {
  irs::order expected;
  return assertOrder(server, true, false, queryString, expected, exprCtx, bindVars, refName);
}

void assertOrderParseFail(arangodb::application_features::ApplicationServer& server,
                          std::string const& queryString, int parseCode) {
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server));

  arangodb::aql::Query query(arangodb::transaction::StandaloneContext::Create(vocbase), arangodb::aql::QueryString(queryString),
                             nullptr, nullptr);

  auto const parseResult = query.parse();
  ASSERT_EQ(parseCode, parseResult.result.errorNumber());
}

}  // namespace

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

class IResearchOrderTest
    : public ::testing::Test,
      public arangodb::tests::LogSuppressor<arangodb::iresearch::TOPIC, arangodb::LogLevel::FATAL>,
      public arangodb::tests::IResearchLogSuppressor {
 protected:
  StorageEngineMock engine;
  arangodb::application_features::ApplicationServer server;
  std::vector<std::pair<arangodb::application_features::ApplicationFeature&, bool>> features;

  IResearchOrderTest() : engine(server), server(nullptr, nullptr) {

    arangodb::tests::init();

    // setup required application features
    auto& selector = server.addFeature<arangodb::EngineSelectorFeature>();
    selector.setEngineTesting(&engine);
    features.emplace_back(selector, false);
    features.emplace_back(server.addFeature<arangodb::MetricsFeature>(), false);
    features.emplace_back(server.addFeature<arangodb::AqlFeature>(), true);
    features.emplace_back(server.addFeature<arangodb::QueryRegistryFeature>(), false);
    features.emplace_back(server.addFeature<arangodb::ViewTypesFeature>(), false);  // required for IResearchFeature
    features.emplace_back(server.addFeature<arangodb::aql::AqlFunctionFeature>(), true);
    features.emplace_back(server.addFeature<arangodb::iresearch::IResearchFeature>(), true);
    features.emplace_back(server.addFeature<arangodb::DatabaseFeature>(), false); // required for calculationVocbase

    for (auto& f : features) {
      f.first.prepare();
    }

    for (auto& f : features) {
      if (f.second) {
        f.first.start();
      }
    }

    // external function names must be registred in upper-case
    // user defined functions have ':' in the external function name
    // function arguments string format: requiredArg1[,requiredArg2]...[|optionalArg1[,optionalArg2]...]
    auto& functions = server.getFeature<arangodb::aql::AqlFunctionFeature>();
    arangodb::aql::Function invalid("INVALID", "|.",
                                    arangodb::aql::Function::makeFlags(
                                        arangodb::aql::Function::Flags::CanRunOnDBServer));

    functions.add(invalid);
  }

  ~IResearchOrderTest() {
    arangodb::aql::AqlFunctionFeature(server).unprepare();  // unset singleton instance
    arangodb::AqlFeature(server).stop();  // unset singleton instance
    server.getFeature<arangodb::EngineSelectorFeature>().setEngineTesting(nullptr);

    // destroy application features
    for (auto& f : features) {
      if (f.second) {
        f.first.stop();
      }
    }

    for (auto& f : features) {
      f.first.unprepare();
    }
  }
};  // IResearchOrderSetup

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

TEST_F(IResearchOrderTest, test_FCall) {
  // invalid function (not an iResearch function)
  {
    std::string query =
        "FOR d IN collection FILTER '1' SORT invalid(d) RETURN d";

    assertOrderParseFail(server, query, TRI_ERROR_NO_ERROR);
  }

  // undefined function (not a function registered with ArangoDB)
  {
    std::string query =
        "FOR d IN collection FILTER '1' SORT undefined(d) RETURN d";

    assertOrderParseFail(server, query, TRI_ERROR_QUERY_FUNCTION_NAME_UNKNOWN);
  }
}

TEST_F(IResearchOrderTest, test_FCall_tfidf) {
  // tfidf
  {
    std::string query = "FOR d IN collection FILTER '1' SORT tfidf(d) RETURN d";
    irs::order expected;
    auto scorer =
        irs::scorers::get("tfidf", irs::type<irs::text_format::json>::get(), irs::string_ref::NIL);

    expected.add(false, std::move(scorer));  // SortCondition is by default ascending
    assertOrderSuccess(server, query, expected);
  }

  // tfidf ASC
  {
    std::string query =
        "FOR d IN collection FILTER '1' SORT tfidf(d) ASC RETURN d";
    irs::order expected;
    auto scorer =
        irs::scorers::get("tfidf", irs::type<irs::text_format::json>::get(), irs::string_ref::NIL);

    expected.add(false, std::move(scorer));
    assertOrderSuccess(server, query, expected);
  }

  // tfidf DESC
  {
    std::string query =
        "FOR d IN collection FILTER '1' SORT tfidf(d) DESC RETURN d";
    irs::order expected;
    auto scorer =
        irs::scorers::get("tfidf", irs::type<irs::text_format::json>::get(), irs::string_ref::NIL);

    expected.add(true, std::move(scorer));
    assertOrderSuccess(server, query, expected);
  }

  // tfidf with norms
  {
    std::string query =
        "FOR d IN collection FILTER '1' SORT tfidf(d, true) DESC RETURN d";
    irs::order expected;
    auto scorer = irs::scorers::get("tfidf", irs::type<irs::text_format::json>::get(), "[ true ]");

    expected.add(true, std::move(scorer));
    assertOrderSuccess(server, query, expected);
  }

  // reference as an argument
  {
    std::string query =
        "LET withNorms=true FOR d IN collection FILTER '1' SORT tfidf(d, "
        "withNorms) DESC RETURN d";
    auto scorer = irs::scorers::get("tfidf", irs::type<irs::text_format::json>::get(), "[ true ]");
    irs::order expected;

    expected.add(true, std::move(scorer));
    
    ExpressionContextMock ctx;
    ctx.vars.emplace("withNorms",
                     arangodb::aql::AqlValue(arangodb::aql::AqlValueHintBool{true}));

    assertOrderSuccess(server, query, expected, &ctx);
  }

  // deterministic expression as an argument
  {
    std::string query =
        "LET x=5 FOR d IN collection FILTER '1' SORT tfidf(d, 1+x > 3) DESC "
        "RETURN d";
    auto scorer = irs::scorers::get("tfidf", irs::type<irs::text_format::json>::get(), "[ true ]");
    irs::order expected;

    expected.add(true, std::move(scorer));
    
    ExpressionContextMock ctx;
    ctx.vars.emplace("x", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{5}));


    assertOrderSuccess(server, query, expected, &ctx);
  }

  // non-deterministic expression as an argument
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("x", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{5}));

    std::string query =
        "LET x=5 FOR d IN collection FILTER '1' SORT tfidf(d, RAND()+x > 3) "
        "DESC RETURN d";
    assertOrderFail(server, query, &ctx);
  }

  // invalid number of arguments function
  {
    std::string query =
        "FOR d IN collection FILTER '1' SORT tfidf(d, true, false) RETURN d";

    assertOrderExecutionFail(server, query);
  }

  // invalid reference (invalid output variable reference)
  {
    auto obj = arangodb::velocypack::Parser::fromJson("{}");

    ExpressionContextMock ctx;
    ctx.vars.emplace("c", arangodb::aql::AqlValue(obj->slice()));

    std::string query =
        "LET c={} FOR d IN collection FILTER '1' SORT tfidf(c) RETURN d";

    assertOrderFail(server, query, &ctx);
  }

  // invalid function (invalid 1st argument)
  {
    std::string query =
        "FOR d IN collection FILTER '1' SORT tfidf('d') RETURN d";

    assertOrderFail(server, query);
  }

  // invalid function (no 1st parameter output variable reference)
  {
    std::string query = "FOR d IN collection FILTER '1' SORT tfidf() RETURN d";

    assertOrderParseFail(server, query, TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH);
  }
}

TEST_F(IResearchOrderTest, test_FCall_bm25) {
  // bm25
  {
    std::string query = "FOR d IN collection FILTER '1' SORT bm25(d) RETURN d";
    irs::order expected;
    auto scorer = irs::scorers::get("bm25", irs::type<irs::text_format::json>::get(), irs::string_ref::NIL);

    expected.add(false, std::move(scorer));  // SortCondition is by default ascending
    assertOrderSuccess(server, query, expected);
  }

  // bm25 ASC
  {
    std::string query =
        "FOR d IN collection FILTER '1' SORT bm25(d) ASC RETURN d";
    irs::order expected;
    auto scorer = irs::scorers::get("bm25", irs::type<irs::text_format::json>::get(), irs::string_ref::NIL);

    expected.add(false, std::move(scorer));
    assertOrderSuccess(server, query, expected);
  }

  // bm25 DESC
  {
    std::string query =
        "FOR d IN collection FILTER '1' SORT bm25(d) DESC RETURN d";
    irs::order expected;
    auto scorer = irs::scorers::get("bm25", irs::type<irs::text_format::json>::get(), irs::string_ref::NIL);

    expected.add(true, std::move(scorer));
    assertOrderSuccess(server, query, expected);
  }

  // bm25 with k coefficient
  {
    std::string query =
        "FOR d IN collection FILTER '1' SORT bm25(d, 0.99) DESC RETURN d";
    irs::order expected;
    auto scorer = irs::scorers::get("bm25", irs::type<irs::text_format::json>::get(), "[ 0.99 ]");

    expected.add(true, std::move(scorer));
    assertOrderSuccess(server, query, expected);
  }

  // reference as k coefficient
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("k", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{0.99}));

    std::string query =
        "LET k=0.99 FOR d IN collection FILTER '1' SORT bm25(d, k) DESC RETURN "
        "d";
    auto scorer = irs::scorers::get("bm25", irs::type<irs::text_format::json>::get(), "[ 0.99 ]");
    irs::order expected;

    expected.add(true, std::move(scorer));

    assertOrderSuccess(server, query, expected, &ctx);
  }

  // deterministic expression as k coefficient
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("x", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{0.97}));

    std::string query =
        "LET x=0.97 FOR d IN collection FILTER '1' SORT bm25(d, x+0.02) DESC "
        "RETURN d";
    auto scorer = irs::scorers::get("bm25", irs::type<irs::text_format::json>::get(), "[ 0.99 ]");
    irs::order expected;

    expected.add(true, std::move(scorer));

    assertOrderSuccess(server, query, expected, &ctx);
  }

  // non-deterministic expression as k coefficient
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("x", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{0.97}));

    std::string query =
        "LET x=0.97 FOR d IN collection FILTER '1' SORT bm25(d, RAND()+x) DESC "
        "RETURN d";
    assertOrderFail(server, query, &ctx);
  }

  // bm25 with k coefficient, b coefficient
  {
    std::string query =
        "FOR d IN collection FILTER '1' SORT bm25(d, 0.99, 1.2) DESC RETURN d";
    auto scorer =
        irs::scorers::get("bm25", irs::type<irs::text_format::json>::get(), "[ 0.99, 1.2 ]");
    irs::order expected;

    expected.add(true, std::move(scorer));

    assertOrderSuccess(server, query, expected);
  }

  // reference as k,b coefficients
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("k", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{0.97}));
    ctx.vars.emplace("b", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{1.2}));

    std::string query =
        "LET k=0.97 LET b=1.2 FOR d IN collection FILTER '1' SORT bm25(d, k, "
        "b) DESC RETURN d";
    auto scorer =
        irs::scorers::get("bm25", irs::type<irs::text_format::json>::get(), "[ 0.97, 1.2 ]");
    irs::order expected;

    expected.add(true, std::move(scorer));

    assertOrderSuccess(server, query, expected, &ctx);
  }

  // deterministic expressions as k,b coefficients
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("x", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{0.97}));
    ctx.vars.emplace("y", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{0.1}));

    std::string query =
        "LET x=0.97 LET y=0.1 FOR d IN collection FILTER '1' SORT bm25(d, "
        "x+0.02, 1+y) DESC RETURN d";
    auto scorer =
        irs::scorers::get("bm25", irs::type<irs::text_format::json>::get(), "[ 0.99, 1.2 ]");
    irs::order expected;

    expected.add(true, std::move(scorer));

    assertOrderSuccess(server, query, expected, &ctx);
  }

  // non-deterministic expression as b coefficient
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("x", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{0.97}));

    std::string query =
        "LET x=0.97 FOR d IN collection FILTER '1' SORT bm25(d, x, RAND()) "
        "DESC RETURN d";
    assertOrderFail(server, query, &ctx);
  }

  // bm25 with k coefficient, b coefficient
  {
    std::string query =
        "FOR d IN collection FILTER '1' SORT bm25(d, 0.99, 1.2) DESC RETURN d";
    irs::order expected;
    auto scorer =
        irs::scorers::get("bm25", irs::type<irs::text_format::json>::get(), "[ 0.99, 1.2 ]");

    expected.add(true, std::move(scorer));
    assertOrderSuccess(server, query, expected);
  }

  // reference as k,b coefficients
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("k", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{0.97}));
    ctx.vars.emplace("b", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{1.2}));
    ctx.vars.emplace("withNorms",
                     arangodb::aql::AqlValue(arangodb::aql::AqlValueHintBool{true}));

    std::string query =
        "LET k=0.97 LET b=1.2 FOR d IN collection FILTER '1' SORT bm25(d, k, "
        "b) DESC RETURN d";
    auto scorer =
        irs::scorers::get("bm25", irs::type<irs::text_format::json>::get(), "[ 0.97, 1.2 ]");
    irs::order expected;

    expected.add(true, std::move(scorer));

    assertOrderSuccess(server, query, expected, &ctx);
  }

  // deterministic expressions as k,b coefficients
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("x", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{0.97}));
    ctx.vars.emplace("y", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{0.1}));

    std::string query =
        "LET x=0.97 LET y=0.1 FOR d IN collection FILTER '1' SORT bm25(d, "
        "x+0.02, 1+y) DESC RETURN d";
    auto scorer =
        irs::scorers::get("bm25", irs::type<irs::text_format::json>::get(), "[ 0.99, 1.1 ]");
    irs::order expected;

    expected.add(true, std::move(scorer));

    assertOrderSuccess(server, query, expected, &ctx);
  }

  // non-deterministic expression as b coefficient
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("x", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{0.97}));

    std::string query =
        "LET x=0.97 FOR d IN collection FILTER '1' SORT bm25(d, x, x, RAND() > "
        "0.5) DESC RETURN d";
    assertOrderFail(server, query, &ctx);
  }

  // invalid number of arguments function
  {
    std::string query =
        "FOR d IN collection FILTER '1' SORT bm25(d, 0.97, 0.07, false) RETURN "
        "d";

    assertOrderParseFail(server, query, TRI_ERROR_NO_ERROR);
  }

  // invalid reference (invalid output variable reference)
  {
    auto obj = arangodb::velocypack::Parser::fromJson("{}");

    ExpressionContextMock ctx;
    ctx.vars.emplace("c", arangodb::aql::AqlValue(obj->slice()));

    std::string query =
        "LET c={} FOR d IN collection FILTER '1' SORT bm25(c) RETURN d";

    assertOrderFail(server, query, &ctx);
  }

  // invalid function (invalid 1st argument)
  {
    std::string query =
        "FOR d IN collection FILTER '1' SORT bm25('d') RETURN d";

    assertOrderFail(server, query);
  }

  // invalid function (no 1st parameter output variable reference)
  {
    std::string query = "FOR d IN collection FILTER '1' SORT bm25() RETURN d";

    assertOrderParseFail(server, query, TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH);
  }
}

TEST_F(IResearchOrderTest, test_FCallUser) {
  // function
  {
    std::string query =
        "FOR d IN collection FILTER '1' SORT test::tfidf(d) RETURN d";
    irs::order expected;

    expected.add<dummy_scorer>(false, irs::string_ref::NIL);
    assertOrderSuccess(server, query, expected);
  }

  // function string scorer arg (expecting string)
  {
    auto validateOrig = dummy_scorer::validateArgs;
    auto restore = irs::make_finally([&validateOrig]() -> void {
      dummy_scorer::validateArgs = validateOrig;
    });
    std::string query =
        "FOR d IN collection FILTER '1' SORT test::tfidf(d, \"abc\") RETURN d";
    irs::order expected;

    expected.add<dummy_scorer>(false, irs::string_ref::NIL);
    dummy_scorer::validateArgs = [](irs::string_ref const& args) -> bool {
      EXPECT_TRUE((irs::string_ref("[\"abc\"]") == args));
      return true;
    };
    assertOrderSuccess(server, query, expected);
  }

  // function string scorer arg (expecting jSON)
  {
    auto validateOrig = dummy_scorer::validateArgs;
    auto restore = irs::make_finally([&validateOrig]() -> void {
      dummy_scorer::validateArgs = validateOrig;
    });
    std::string query =
        "FOR d IN collection FILTER '1' SORT test::tfidf(d, \"abc\") RETURN d";
    irs::order expected;
    bool valid = true;

    expected.add<dummy_scorer>(false, irs::string_ref::NIL);

    size_t attempt = 0;
    dummy_scorer::validateArgs = [&valid, &attempt](irs::string_ref const& args) -> bool {
      attempt++;
      return valid == (args == "[\"abc\"]");
    };
    assertOrderSuccess(server, query, expected);
    EXPECT_TRUE((valid));
    EXPECT_TRUE(1 == attempt);
  }

  // function string jSON scorer arg (expecting string)
  {
    auto validateOrig = dummy_scorer::validateArgs;
    auto restore = irs::make_finally([&validateOrig]() -> void {
      dummy_scorer::validateArgs = validateOrig;
    });
    std::string query =
        "FOR d IN collection FILTER '1' SORT test::tfidf(d, \"{\\\"abc\\\": "
        "\\\"def\\\"}\") RETURN d";
    irs::order expected;

    expected.add<dummy_scorer>(false, irs::string_ref::NIL);

    size_t attempt = 0;
    dummy_scorer::validateArgs = [&attempt](irs::string_ref const& args) -> bool {
      attempt++;
      EXPECT_TRUE((irs::string_ref("[\"{\\\"abc\\\": \\\"def\\\"}\"]") == args));
      return true;
    };
    assertOrderSuccess(server, query, expected);
    EXPECT_TRUE(1 == attempt);
  }

  // function string jSON scorer arg (expecting jSON)
  {
    auto validateOrig = dummy_scorer::validateArgs;
    auto restore = irs::make_finally([&validateOrig]() -> void {
      dummy_scorer::validateArgs = validateOrig;
    });
    std::string query =
        "FOR d IN collection FILTER '1' SORT test::tfidf(d, \"{\\\"abc\\\": "
        "\\\"def\\\"}\") RETURN d";
    irs::order expected;
    bool valid = true;

    expected.add<dummy_scorer>(false, irs::string_ref::NIL);
    size_t attempt = 0;
    dummy_scorer::validateArgs = [&valid, &attempt](irs::string_ref const& args) -> bool {
      attempt++;
      valid = irs::string_ref("[\"{\\\"abc\\\": \\\"def\\\"}\"]") == args;
      return valid;
    };
    assertOrderSuccess(server, query, expected);
    EXPECT_TRUE((valid));
    EXPECT_TRUE(1 == attempt);
  }

  // function raw jSON scorer arg
  {
    auto validateOrig = dummy_scorer::validateArgs;
    auto restore = irs::make_finally([&validateOrig]() -> void {
      dummy_scorer::validateArgs = validateOrig;
    });
    std::string query =
        "FOR d IN collection FILTER '1' SORT test::tfidf(d, {\"abc\": "
        "\"def\"}) RETURN d";
    irs::order expected;

    expected.add<dummy_scorer>(false, irs::string_ref::NIL);
    size_t attempt = 0;
    dummy_scorer::validateArgs = [&attempt](irs::string_ref const& args) -> bool {
      ++attempt;
      EXPECT_TRUE((irs::string_ref("[{\"abc\":\"def\"}]") == args));
      return true;
    };
    assertOrderSuccess(server, query, expected);
    EXPECT_TRUE(1 == attempt);
  }

  // function 2 string scorer args
  {
    auto validateOrig = dummy_scorer::validateArgs;
    auto restore = irs::make_finally([&validateOrig]() -> void {
      dummy_scorer::validateArgs = validateOrig;
    });
    std::string query =
        "FOR d IN collection FILTER '1' SORT test::tfidf(d, \"abc\", \"def\") "
        "RETURN d";
    irs::order expected;

    expected.add<dummy_scorer>(false, irs::string_ref::NIL);
    size_t attempt = 0;
    dummy_scorer::validateArgs = [&attempt](irs::string_ref const& args) -> bool {
      ++attempt;
      EXPECT_TRUE((irs::string_ref("[\"abc\",\"def\"]") == args));
      return true;
    };
    assertOrderSuccess(server, query, expected);
    EXPECT_TRUE(1 == attempt);
  }

  // function string+jSON(string) scorer args
  {
    auto validateOrig = dummy_scorer::validateArgs;
    auto restore = irs::make_finally([&validateOrig]() -> void {
      dummy_scorer::validateArgs = validateOrig;
    });
    std::string query =
        "FOR d IN collection FILTER '1' SORT test::tfidf(d, \"abc\", "
        "\"{\\\"def\\\": \\\"ghi\\\"}\") RETURN d";
    irs::order expected;

    expected.add<dummy_scorer>(false, irs::string_ref::NIL);
    size_t attempt = 0;
    dummy_scorer::validateArgs = [&attempt](irs::string_ref const& args) -> bool {
      ++attempt;
      EXPECT_TRUE(
          (irs::string_ref("[\"abc\",\"{\\\"def\\\": \\\"ghi\\\"}\"]") == args));
      return true;
    };
    assertOrderSuccess(server, query, expected);
    EXPECT_TRUE(1 == attempt);
  }

  // function string+jSON(raw) scorer args
  {
    auto validateOrig = dummy_scorer::validateArgs;
    auto restore = irs::make_finally([&validateOrig]() -> void {
      dummy_scorer::validateArgs = validateOrig;
    });
    std::string query =
        "FOR d IN collection FILTER '1' SORT test::tfidf(d, \"abc\", {\"def\": "
        "\"ghi\"}) RETURN d";
    irs::order expected;

    expected.add<dummy_scorer>(false, irs::string_ref::NIL);
    size_t attempt = 0;
    dummy_scorer::validateArgs = [&attempt](irs::string_ref const& args) -> bool {
      ++attempt;
      EXPECT_TRUE((irs::string_ref("[\"abc\",{\"def\":\"ghi\"}]") == args));
      return true;
    };
    assertOrderSuccess(server, query, expected);
    EXPECT_TRUE(1 == attempt);
  }

  // function ASC
  {
    std::string query =
        "FOR d IN collection FILTER '1' SORT test::tfidf(d) ASC RETURN d";
    irs::order expected;

    expected.add<dummy_scorer>(false, irs::string_ref::NIL);
    assertOrderSuccess(server, query, expected);
  }

  // function DESC
  {
    std::string query =
        "FOR d IN collection FILTER '1' SORT test::tfidf(d) DESC RETURN d";
    irs::order expected;

    expected.add<dummy_scorer>(true, irs::string_ref::NIL);
    assertOrderSuccess(server, query, expected);
  }

  // invalid function (no 1st parameter output variable reference)
  {
    std::string query =
        "FOR d IN collection FILTER '1' SORT test::tfidf() RETURN d";

    assertOrderFail(server, query);
  }

  // invalid function (not an iResearch function)
  {
    std::string query =
        "FOR d IN collection FILTER '1' SORT test::invalid(d) DESC RETURN d";

    assertOrderFail(server, query);
  }
}

TEST_F(IResearchOrderTest, test_StringValue) {
  // simple field
  {
    std::string query = "FOR d IN collection FILTER '1' SORT 'a' RETURN d";
    assertOrderFail(server, query);
  }

  // simple field ASC
  {
    std::string query = "FOR d IN collection FILTER '1' SORT 'a' ASC RETURN d";
    assertOrderFail(server, query);
  }

  // simple field DESC
  {
    std::string query = "FOR d IN collection FILTER '1' SORT 'a' DESC RETURN d";
    assertOrderFail(server, query);
  }

  // nested field
  {
    std::string query = "FOR d IN collection FILTER '1' SORT 'a.b.c' RETURN d";
    assertOrderFail(server, query);
  }

  // nested field ASC
  {
    std::string query =
        "FOR d IN collection FILTER '1' SORT 'a.b.c' ASC RETURN d";
    assertOrderFail(server, query);
  }

  // nested field DESC
  {
    std::string query =
        "FOR d IN collection FILTER '1' SORT 'a.b.c' DESC RETURN d";
    assertOrderFail(server, query);
  }
}

TEST_F(IResearchOrderTest, test_order) {
  // test mutiple sort
  {
    std::string query =
        "FOR d IN collection FILTER '1' SORT test::tfidf(d) DESC, tfidf(d) "
        "RETURN d";
    irs::order expected;
    auto scorer =
        irs::scorers::get("tfidf", irs::type<irs::text_format::json>::get(), irs::string_ref::NIL);

    expected.add<dummy_scorer>(true, irs::string_ref::NIL);
    expected.add(false, std::move(scorer));
    assertOrderSuccess(server, query, expected);
  }

  // invalid field
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{5}));
    std::string query =
        "LET a=1 FOR d IN collection FILTER '1' SORT a RETURN d";

    assertOrderFail(server, query, &ctx);
  }
}
