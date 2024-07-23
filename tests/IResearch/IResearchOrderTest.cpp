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
#include "Aql/Query.h"
#include "Aql/SortCondition.h"
#include "Cluster/ClusterFeature.h"
#include "IResearch/AqlHelper.h"
#include "IResearch/IResearchCommon.h"
#include "IResearch/IResearchFeature.h"
#include "IResearch/IResearchFilterContext.h"
#include "IResearch/IResearchOrderFactory.h"
#include "RestServer/AqlFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "Metrics/ClusterMetricsFeature.h"
#include "Metrics/MetricsFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "RestServer/ViewTypesFeature.h"
#include "Statistics/StatisticsFeature.h"
#include "Statistics/StatisticsWorker.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "Transaction/Methods.h"
#include "Transaction/StandaloneContext.h"

namespace {

bool operator==(std::span<irs::Scorer::ptr const> lhs,
                std::vector<irs::Scorer::ptr> const& rhs) noexcept {
  if (lhs.size() != rhs.size()) {
    return false;
  }

  for (size_t i = 0, count = lhs.size(); i < count; ++i) {
    if (!lhs[i] != !rhs[i]) {
      return false;
    }
    if (!lhs[i]) {
      continue;
    }
    auto& sort = *lhs[i];
    auto& other_sort = *rhs[i];

    // FIXME TODO operator==(...) should be specialized for every sort child
    // class based on init config
    if (sort.type() != other_sort.type()) {
      return false;
    }
  }

  return true;
}

struct dummy_scorer final : public irs::ScorerBase<void> {
  static std::function<bool(std::string_view)> validateArgs;
  static constexpr std::string_view type_name() noexcept {
    return "TEST::TFIDF";
  }
  static ptr make(std::string_view args) {
    if (!validateArgs(args)) {
      return nullptr;
    }
    return std::make_unique<dummy_scorer>();
  }

  irs::IndexFeatures index_features() const noexcept final {
    return irs::IndexFeatures::NONE;
  }
  irs::ScoreFunction prepare_scorer(
      const irs::ColumnProvider& /*segment*/,
      const std::map<irs::type_info::type_id, irs::field_id>& /*features*/,
      const irs::byte_type* /*stats*/,
      const irs::attribute_provider& /*doc_attrs*/,
      irs::score_t /*boost*/) const noexcept final {
    return irs::ScoreFunction::Default(0);
  }

  dummy_scorer() = default;
};

/*static*/ std::function<bool(std::string_view)> dummy_scorer::validateArgs =
    [](std::string_view) -> bool { return true; };

REGISTER_SCORER_JSON(dummy_scorer, dummy_scorer::make);

void assertOrder(
    arangodb::ArangodServer& server, bool parseOk, bool execOk,
    std::string const& queryString, std::span<irs::Scorer::ptr const> expected,
    arangodb::aql::ExpressionContext* exprCtx = nullptr,
    std::shared_ptr<arangodb::velocypack::Builder> bindVars = nullptr,
    std::string const& refName = "d") {
  TRI_vocbase_t vocbase(testDBInfo(server));

  auto query = arangodb::aql::Query::create(
      arangodb::transaction::StandaloneContext::create(
          vocbase, arangodb::transaction::OperationOriginTestCase{}),
      arangodb::aql::QueryString(queryString), bindVars);

  auto const parseResult = query->parse();
  ASSERT_TRUE(parseResult.result.ok());

  auto* ast = query->ast();
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
    arangodb::iresearch::QueryContext const ctx{.ref = ref};

    for (size_t i = 0, count = sortNode->numMembers(); i < count; ++i) {
      auto const* sort = sortNode->getMember(i);
      auto const* expr = sort->getMember(0);

      EXPECT_TRUE(parseOk == arangodb::iresearch::order_factory::scorer(
                                 nullptr, *expr, ctx));
    }
  }

  // execution time check
  {
    std::vector<irs::Scorer::ptr> actual;
    irs::Scorer::ptr actualScorer;

    arangodb::transaction::Methods trx(
        arangodb::transaction::StandaloneContext::create(
            vocbase, arangodb::transaction::OperationOriginTestCase{}),
        {}, {}, {}, arangodb::transaction::Options());

    auto* mockCtx = dynamic_cast<ExpressionContextMock*>(exprCtx);
    if (mockCtx) {  // simon: hack to make expression context work again
      mockCtx->setTrx(&trx);
    }

    arangodb::iresearch::QueryContext const ctx{&trx, ast, exprCtx,
                                                &irs::SubReader::empty(), ref};

    for (size_t i = 0, count = sortNode->numMembers(); i < count; ++i) {
      auto const* sort = sortNode->getMember(i);
      auto const* expr = sort->getMember(0);
      EXPECT_TRUE(execOk == arangodb::iresearch::order_factory::scorer(
                                &actualScorer, *expr, ctx));

      if (execOk) {
        actual.emplace_back(std::move(actualScorer));
      }
    }

    EXPECT_TRUE(!execOk || expected == actual);
  }
}

void assertOrderSuccess(
    arangodb::ArangodServer& server, std::string const& queryString,
    std::span<const irs::Scorer::ptr> expected,
    arangodb::aql::ExpressionContext* exprCtx = nullptr,
    std::shared_ptr<arangodb::velocypack::Builder> bindVars = nullptr,
    std::string const& refName = "d") {
  return assertOrder(server, true, true, queryString, expected, exprCtx,
                     bindVars, refName);
}

void assertOrderFail(
    arangodb::ArangodServer& server, std::string const& queryString,
    arangodb::aql::ExpressionContext* exprCtx = nullptr,
    std::shared_ptr<arangodb::velocypack::Builder> bindVars = nullptr,
    std::string const& refName = "d") {
  return assertOrder(server, false, false, queryString, {}, exprCtx, bindVars,
                     refName);
}

void assertOrderExecutionFail(
    arangodb::ArangodServer& server, std::string const& queryString,
    arangodb::aql::ExpressionContext* exprCtx = nullptr,
    std::shared_ptr<arangodb::velocypack::Builder> bindVars = nullptr,
    std::string const& refName = "d") {
  return assertOrder(server, true, false, queryString, {}, exprCtx, bindVars,
                     refName);
}

void assertOrderParseFail(arangodb::ArangodServer& server,
                          std::string const& queryString, ErrorCode parseCode) {
  TRI_vocbase_t vocbase(testDBInfo(server));

  auto query = arangodb::aql::Query::create(
      arangodb::transaction::StandaloneContext::create(
          vocbase, arangodb::transaction::OperationOriginTestCase{}),
      arangodb::aql::QueryString(queryString), nullptr);

  auto const parseResult = query->parse();
  ASSERT_EQ(parseCode, parseResult.result.errorNumber());
}

}  // namespace

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

class IResearchOrderTest
    : public ::testing::Test,
      public arangodb::tests::LogSuppressor<arangodb::iresearch::TOPIC,
                                            arangodb::LogLevel::FATAL>,
      public arangodb::tests::IResearchLogSuppressor {
 protected:
  arangodb::ArangodServer server;
  StorageEngineMock engine;
  std::vector<
      std::pair<arangodb::application_features::ApplicationFeature&, bool>>
      features;

  IResearchOrderTest()
      : server(
            std::make_shared<arangodb::options::ProgramOptions>("", "", "", ""),
            nullptr),
        engine(server) {
    arangodb::tests::init();

    // setup required application features
    auto& selector = server.addFeature<arangodb::EngineSelectorFeature>();
    selector.setEngineTesting(&engine);
    features.emplace_back(selector, false);
    features.emplace_back(
        server.addFeature<arangodb::metrics::MetricsFeature>(
            arangodb::LazyApplicationFeatureReference<
                arangodb::QueryRegistryFeature>(server),
            arangodb::LazyApplicationFeatureReference<
                arangodb::StatisticsFeature>(nullptr),
            selector,
            arangodb::LazyApplicationFeatureReference<
                arangodb::metrics::ClusterMetricsFeature>(nullptr),
            arangodb::LazyApplicationFeatureReference<arangodb::ClusterFeature>(
                nullptr)),
        false);
    features.emplace_back(server.addFeature<arangodb::AqlFeature>(), true);
    features.emplace_back(
        server.addFeature<arangodb::QueryRegistryFeature>(
            server.template getFeature<arangodb::metrics::MetricsFeature>()),
        false);
    features.emplace_back(server.addFeature<arangodb::ViewTypesFeature>(),
                          false);  // required for IResearchFeature
    features.emplace_back(
        server.addFeature<arangodb::aql::AqlFunctionFeature>(), true);
    {
      auto& feature =
          features
              .emplace_back(
                  server.addFeature<arangodb::iresearch::IResearchFeature>(),
                  true)
              .first;
      feature.validateOptions(server.options());
      feature.collectOptions(server.options());
    }
    features.emplace_back(server.addFeature<arangodb::DatabaseFeature>(),
                          false);  // required for calculationVocbase

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
    // function arguments string format:
    // requiredArg1[,requiredArg2]...[|optionalArg1[,optionalArg2]...]
    auto& functions = server.getFeature<arangodb::aql::AqlFunctionFeature>();
    arangodb::aql::Function invalid(
        "INVALID", "|.",
        arangodb::aql::Function::makeFlags(
            arangodb::aql::Function::Flags::CanRunOnDBServerCluster,
            arangodb::aql::Function::Flags::CanRunOnDBServerOneShard),
        nullptr);
    functions.add(invalid);
  }

  ~IResearchOrderTest() {
    arangodb::aql::AqlFunctionFeature(server)
        .unprepare();                     // unset singleton instance
    arangodb::AqlFeature(server).stop();  // unset singleton instance
    server.getFeature<arangodb::EngineSelectorFeature>().setEngineTesting(
        nullptr);

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
    std::array expected{irs::scorers::get(
        "tfidf", irs::type<irs::text_format::json>::get(), std::string_view{})};

    assertOrderSuccess(server, query, expected);
  }

  // tfidf ASC
  {
    std::string query =
        "FOR d IN collection FILTER '1' SORT tfidf(d) ASC RETURN d";
    std::array expected{irs::scorers::get(
        "tfidf", irs::type<irs::text_format::json>::get(), std::string_view{})};

    assertOrderSuccess(server, query, expected);
  }

  // tfidf DESC
  {
    std::string query =
        "FOR d IN collection FILTER '1' SORT tfidf(d) DESC RETURN d";
    std::array expected{irs::scorers::get(
        "tfidf", irs::type<irs::text_format::json>::get(), std::string_view{})};

    assertOrderSuccess(server, query, expected);
  }

  // tfidf with norms
  {
    std::string query =
        "FOR d IN collection FILTER '1' SORT tfidf(d, true) DESC RETURN d";
    std::array expected{irs::scorers::get(
        "tfidf", irs::type<irs::text_format::json>::get(), "[ true ]")};

    assertOrderSuccess(server, query, expected);
  }

  // reference as an argument
  {
    std::string query =
        "LET withNorms=true FOR d IN collection FILTER '1' SORT tfidf(d, "
        "withNorms) DESC RETURN d";

    std::array expected{irs::scorers::get(
        "tfidf", irs::type<irs::text_format::json>::get(), "[ true ]")};

    ExpressionContextMock ctx;
    ctx.vars.emplace("withNorms", arangodb::aql::AqlValue(
                                      arangodb::aql::AqlValueHintBool{true}));

    assertOrderSuccess(server, query, expected, &ctx);
  }

  // deterministic expression as an argument
  {
    std::string query =
        "LET x=5 FOR d IN collection FILTER '1' SORT tfidf(d, 1+x > 3) DESC "
        "RETURN d";
    std::array expected{irs::scorers::get(
        "tfidf", irs::type<irs::text_format::json>::get(), "[ true ]")};

    ExpressionContextMock ctx;
    ctx.vars.emplace(
        "x", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{5}));

    assertOrderSuccess(server, query, expected, &ctx);
  }

  // non-deterministic expression as an argument
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace(
        "x", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{5}));

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

    assertOrderParseFail(server, query,
                         TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH);
  }
}

TEST_F(IResearchOrderTest, test_FCall_bm25) {
  // bm25
  {
    std::string query = "FOR d IN collection FILTER '1' SORT bm25(d) RETURN d";
    std::array expected{irs::scorers::get(
        "bm25", irs::type<irs::text_format::json>::get(), std::string_view{})};

    assertOrderSuccess(server, query, expected);
  }

  // bm25 ASC
  {
    std::string query =
        "FOR d IN collection FILTER '1' SORT bm25(d) ASC RETURN d";
    std::array expected{irs::scorers::get(
        "bm25", irs::type<irs::text_format::json>::get(), std::string_view{})};

    assertOrderSuccess(server, query, expected);
  }

  // bm25 DESC
  {
    std::string query =
        "FOR d IN collection FILTER '1' SORT bm25(d) DESC RETURN d";
    std::array expected{irs::scorers::get(
        "bm25", irs::type<irs::text_format::json>::get(), std::string_view{})};

    assertOrderSuccess(server, query, expected);
  }

  // bm25 with k coefficient
  {
    std::string query =
        "FOR d IN collection FILTER '1' SORT bm25(d, 0.99) DESC RETURN d";
    std::array expected{irs::scorers::get(
        "bm25", irs::type<irs::text_format::json>::get(), "[ 0.99 ]")};

    assertOrderSuccess(server, query, expected);
  }

  // reference as k coefficient
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace(
        "k", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{0.99}));

    std::string query =
        "LET k=0.99 FOR d IN collection FILTER '1' SORT bm25(d, k) DESC RETURN "
        "d";
    std::array expected{irs::scorers::get(
        "bm25", irs::type<irs::text_format::json>::get(), "[ 0.99 ]")};

    assertOrderSuccess(server, query, expected, &ctx);
  }

  // deterministic expression as k coefficient
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace(
        "x", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{0.97}));

    std::string query =
        "LET x=0.97 FOR d IN collection FILTER '1' SORT bm25(d, x+0.02) DESC "
        "RETURN d";
    std::array expected{irs::scorers::get(
        "bm25", irs::type<irs::text_format::json>::get(), "[ 0.99 ]")};

    assertOrderSuccess(server, query, expected, &ctx);
  }

  // non-deterministic expression as k coefficient
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace(
        "x", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{0.97}));

    std::string query =
        "LET x=0.97 FOR d IN collection FILTER '1' SORT bm25(d, RAND()+x) DESC "
        "RETURN d";
    assertOrderFail(server, query, &ctx);
  }

  // bm25 with k coefficient, b coefficient
  {
    std::string query =
        "FOR d IN collection FILTER '1' SORT bm25(d, 0.99, 1.2) DESC RETURN d";
    std::array expected{irs::scorers::get(
        "bm25", irs::type<irs::text_format::json>::get(), "[ 0.99, 1.2 ]")};

    assertOrderSuccess(server, query, expected);
  }

  // reference as k,b coefficients
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace(
        "k", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{0.97}));
    ctx.vars.emplace(
        "b", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{1.2}));

    std::string query =
        "LET k=0.97 LET b=1.2 FOR d IN collection FILTER '1' SORT bm25(d, k, "
        "b) DESC RETURN d";
    std::array expected{irs::scorers::get(
        "bm25", irs::type<irs::text_format::json>::get(), "[ 0.97, 1.2 ]")};

    assertOrderSuccess(server, query, expected, &ctx);
  }

  // deterministic expressions as k,b coefficients
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace(
        "x", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{0.97}));
    ctx.vars.emplace(
        "y", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{0.1}));

    std::string query =
        "LET x=0.97 LET y=0.1 FOR d IN collection FILTER '1' SORT bm25(d, "
        "x+0.02, 1+y) DESC RETURN d";
    std::array expected{irs::scorers::get(
        "bm25", irs::type<irs::text_format::json>::get(), "[ 0.99, 1.2 ]")};

    assertOrderSuccess(server, query, expected, &ctx);
  }

  // non-deterministic expression as b coefficient
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace(
        "x", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{0.97}));

    std::string query =
        "LET x=0.97 FOR d IN collection FILTER '1' SORT bm25(d, x, RAND()) "
        "DESC RETURN d";
    assertOrderFail(server, query, &ctx);
  }

  // bm25 with k coefficient, b coefficient
  {
    std::string query =
        "FOR d IN collection FILTER '1' SORT bm25(d, 0.99, 1.2) DESC RETURN d";
    std::array expected{irs::scorers::get(
        "bm25", irs::type<irs::text_format::json>::get(), "[ 0.99, 1.2 ]")};

    assertOrderSuccess(server, query, expected);
  }

  // reference as k,b coefficients
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace(
        "k", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{0.97}));
    ctx.vars.emplace(
        "b", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{1.2}));
    ctx.vars.emplace("withNorms", arangodb::aql::AqlValue(
                                      arangodb::aql::AqlValueHintBool{true}));

    std::string query =
        "LET k=0.97 LET b=1.2 FOR d IN collection FILTER '1' SORT bm25(d, k, "
        "b) DESC RETURN d";
    std::array expected{irs::scorers::get(
        "bm25", irs::type<irs::text_format::json>::get(), "[ 0.97, 1.2 ]")};

    assertOrderSuccess(server, query, expected, &ctx);
  }

  // deterministic expressions as k,b coefficients
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace(
        "x", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{0.97}));
    ctx.vars.emplace(
        "y", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{0.1}));

    std::string query =
        "LET x=0.97 LET y=0.1 FOR d IN collection FILTER '1' SORT bm25(d, "
        "x+0.02, 1+y) DESC RETURN d";
    std::array expected{irs::scorers::get(
        "bm25", irs::type<irs::text_format::json>::get(), "[ 0.99, 1.1 ]")};

    assertOrderSuccess(server, query, expected, &ctx);
  }

  // non-deterministic expression as b coefficient
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace(
        "x", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{0.97}));

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

    assertOrderParseFail(server, query,
                         TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH);
  }
}

#ifdef USE_V8
TEST_F(IResearchOrderTest, test_FCallUser) {
  // function
  {
    std::string query =
        "FOR d IN collection FILTER '1' SORT test::tfidf(d) RETURN d";

    std::array expected{dummy_scorer::make(std::string_view{})};

    assertOrderSuccess(server, query, expected);
  }

  // function string scorer arg (expecting string)
  {
    auto validateOrig = dummy_scorer::validateArgs;
    irs::Finally restore = [&validateOrig]() noexcept {
      dummy_scorer::validateArgs = validateOrig;
    };
    std::string query =
        "FOR d IN collection FILTER '1' SORT test::tfidf(d, \"abc\") RETURN d";
    std::array expected{dummy_scorer::make(std::string_view{})};
    dummy_scorer::validateArgs = [](std::string_view args) -> bool {
      EXPECT_TRUE((std::string_view("[\"abc\"]") == args));
      return true;
    };
    assertOrderSuccess(server, query, expected);
  }

  // function string scorer arg (expecting jSON)
  {
    auto validateOrig = dummy_scorer::validateArgs;
    irs::Finally restore = [&validateOrig]() noexcept {
      dummy_scorer::validateArgs = validateOrig;
    };
    std::string query =
        "FOR d IN collection FILTER '1' SORT test::tfidf(d, \"abc\") RETURN d";
    std::array expected{dummy_scorer::make(std::string_view{})};
    bool valid = true;

    size_t attempt = 0;
    dummy_scorer::validateArgs = [&valid,
                                  &attempt](std::string_view args) -> bool {
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
    irs::Finally restore = [&validateOrig]() noexcept {
      dummy_scorer::validateArgs = validateOrig;
    };
    std::string query =
        "FOR d IN collection FILTER '1' SORT test::tfidf(d, \"{\\\"abc\\\": "
        "\\\"def\\\"}\") RETURN d";
    std::array expected{dummy_scorer::make(std::string_view{})};

    size_t attempt = 0;
    dummy_scorer::validateArgs = [&attempt](std::string_view args) -> bool {
      attempt++;
      EXPECT_TRUE(
          (std::string_view("[\"{\\\"abc\\\": \\\"def\\\"}\"]") == args));
      return true;
    };
    assertOrderSuccess(server, query, expected);
    EXPECT_TRUE(1 == attempt);
  }

  // function string jSON scorer arg (expecting jSON)
  {
    auto validateOrig = dummy_scorer::validateArgs;
    irs::Finally restore = [&validateOrig]() noexcept {
      dummy_scorer::validateArgs = validateOrig;
    };
    std::string query =
        "FOR d IN collection FILTER '1' SORT test::tfidf(d, \"{\\\"abc\\\": "
        "\\\"def\\\"}\") RETURN d";
    std::array expected{dummy_scorer::make(std::string_view{})};
    bool valid = true;

    size_t attempt = 0;
    dummy_scorer::validateArgs = [&valid,
                                  &attempt](std::string_view args) -> bool {
      attempt++;
      valid = std::string_view("[\"{\\\"abc\\\": \\\"def\\\"}\"]") == args;
      return valid;
    };
    assertOrderSuccess(server, query, expected);
    EXPECT_TRUE((valid));
    EXPECT_TRUE(1 == attempt);
  }

  // function raw jSON scorer arg
  {
    auto validateOrig = dummy_scorer::validateArgs;
    irs::Finally restore = [&validateOrig]() noexcept {
      dummy_scorer::validateArgs = validateOrig;
    };
    std::string query =
        "FOR d IN collection FILTER '1' SORT test::tfidf(d, {\"abc\": "
        "\"def\"}) RETURN d";
    std::array expected{dummy_scorer::make(std::string_view{})};

    size_t attempt = 0;
    dummy_scorer::validateArgs = [&attempt](std::string_view args) -> bool {
      ++attempt;
      EXPECT_TRUE((std::string_view("[{\"abc\":\"def\"}]") == args));
      return true;
    };
    assertOrderSuccess(server, query, expected);
    EXPECT_TRUE(1 == attempt);
  }

  // function 2 string scorer args
  {
    auto validateOrig = dummy_scorer::validateArgs;
    irs::Finally restore = [&validateOrig]() noexcept {
      dummy_scorer::validateArgs = validateOrig;
    };
    std::string query =
        "FOR d IN collection FILTER '1' SORT test::tfidf(d, \"abc\", \"def\") "
        "RETURN d";
    std::array expected{dummy_scorer::make(std::string_view{})};
    size_t attempt = 0;
    dummy_scorer::validateArgs = [&attempt](std::string_view args) -> bool {
      ++attempt;
      EXPECT_TRUE((std::string_view("[\"abc\",\"def\"]") == args));
      return true;
    };
    assertOrderSuccess(server, query, expected);
    EXPECT_TRUE(1 == attempt);
  }

  // function string+jSON(string) scorer args
  {
    auto validateOrig = dummy_scorer::validateArgs;
    irs::Finally restore = [&validateOrig]() noexcept {
      dummy_scorer::validateArgs = validateOrig;
    };
    std::string query =
        "FOR d IN collection FILTER '1' SORT test::tfidf(d, \"abc\", "
        "\"{\\\"def\\\": \\\"ghi\\\"}\") RETURN d";
    std::array expected{dummy_scorer::make(std::string_view{})};
    size_t attempt = 0;
    dummy_scorer::validateArgs = [&attempt](std::string_view args) -> bool {
      ++attempt;
      EXPECT_TRUE((std::string_view(
                       "[\"abc\",\"{\\\"def\\\": \\\"ghi\\\"}\"]") == args));
      return true;
    };
    assertOrderSuccess(server, query, expected);
    EXPECT_TRUE(1 == attempt);
  }

  // function string+jSON(raw) scorer args
  {
    auto validateOrig = dummy_scorer::validateArgs;
    irs::Finally restore = [&validateOrig]() noexcept {
      dummy_scorer::validateArgs = validateOrig;
    };
    std::string query =
        "FOR d IN collection FILTER '1' SORT test::tfidf(d, \"abc\", {\"def\": "
        "\"ghi\"}) RETURN d";
    std::array expected{dummy_scorer::make(std::string_view{})};
    size_t attempt = 0;
    dummy_scorer::validateArgs = [&attempt](std::string_view args) -> bool {
      ++attempt;
      EXPECT_TRUE((std::string_view("[\"abc\",{\"def\":\"ghi\"}]") == args));
      return true;
    };
    assertOrderSuccess(server, query, expected);
    EXPECT_TRUE(1 == attempt);
  }

  // function ASC
  {
    std::string query =
        "FOR d IN collection FILTER '1' SORT test::tfidf(d) ASC RETURN d";
    std::array expected{dummy_scorer::make(std::string_view{})};
    assertOrderSuccess(server, query, expected);
  }

  // function DESC
  {
    std::string query =
        "FOR d IN collection FILTER '1' SORT test::tfidf(d) DESC RETURN d";
    std::array expected{dummy_scorer::make(std::string_view{})};
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
#endif

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

#ifdef USE_V8
TEST_F(IResearchOrderTest, test_order) {
  // test mutiple sort
  {
    std::string query =
        "FOR d IN collection FILTER '1' SORT test::tfidf(d) DESC, tfidf(d) "
        "RETURN d";
    std::array expected{
        dummy_scorer::make(std::string_view{}),
        irs::scorers::get("tfidf", irs::type<irs::text_format::json>::get(),
                          std::string_view{})};

    assertOrderSuccess(server, query, expected);
  }

  // invalid field
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace(
        "a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{5}));
    std::string query =
        "LET a=1 FOR d IN collection FILTER '1' SORT a RETURN d";

    assertOrderFail(server, query, &ctx);
  }
}
#endif
