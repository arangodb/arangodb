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

#include "utils/utf8_path.hpp"

#include "catch.hpp"
#include "common.h"
#include "ExpressionContextMock.h"
#include "Agency/AgencyComm.h"
#include "Aql/OptimizerRulesFeature.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/ExpressionContext.h"
#include "Aql/Ast.h"
#include "ClusterEngine/ClusterEngine.h"
#include "Random/RandomGenerator.h"
#include "Basics/files.h"
#include "RestServer/DatabasePathFeature.h"
#include "V8/v8-utils.h"
#include "VocBase/KeyGenerator.h"
#include "Transaction/StandaloneContext.h"
#include "RestServer/QueryRegistryFeature.h"
#include "IResearch/AqlHelper.h"
#include "IResearch/ExpressionFilter.h"
#include "IResearch/IResearchFilterFactory.h"
#include "IResearch/IResearchKludge.h"
#include "IResearch/VelocyPackHelper.h"
#include "tests/Basics/icu-helper.h"
#include "search/scorers.hpp"

#include "search/boolean_filter.hpp"

#include <velocypack/Iterator.h>
#include <velocypack/Parser.h>

#include <unordered_set>

extern const char* ARGV0; // defined in main.cpp

namespace {

struct BoostScorer : public irs::sort {
  struct Prepared: public irs::sort::prepared_base<irs::boost::boost_t> {
   public:
    DECLARE_FACTORY(Prepared);

    Prepared() = default;

    virtual void add(irs::byte_type* dst, irs::byte_type const* src) const override {
      score_cast(dst) += score_cast(src);
    }

    virtual irs::flags const& features() const override {
      return irs::flags::empty_instance();
    }

    virtual bool less(irs::byte_type const* lhs, irs::byte_type const* rhs) const override {
      return score_cast(lhs) < score_cast(rhs);
    }

    virtual irs::sort::collector::ptr prepare_collector() const override {
      return nullptr;
    }

    virtual void prepare_score(irs::byte_type* score) const override {
      score_cast(score) = 0.f;
    }

    virtual irs::sort::scorer::ptr prepare_scorer(
      irs::sub_reader const&,
      irs::term_reader const&,
      irs::attribute_store const& query_attributes,
      irs::attribute_view const&
    ) const override {
      struct Scorer : public irs::sort::scorer {
        Scorer(irs::boost::boost_t score): scr(score) { }

        virtual void score(irs::byte_type* score_buf) override {
          *reinterpret_cast<score_t*>(score_buf) = scr;
        }

        irs::boost::boost_t scr;
      };

      return irs::sort::scorer::make<Scorer>(
        irs::boost::extract(query_attributes)
      );
    }
  };

  static ::iresearch::sort::type_id const& type() {
    static ::iresearch::sort::type_id TYPE("boostscorer");
    return TYPE;
  }

  static irs::sort::ptr make(irs::string_ref const&) {
    return std::make_shared<BoostScorer>();
  }

  BoostScorer() : irs::sort(BoostScorer::type()) {}

  virtual irs::sort::prepared::ptr prepare() const override {
    return irs::memory::make_unique<Prepared>();
  }
}; // BoostScorer

REGISTER_SCORER_JSON(BoostScorer, BoostScorer::make);

}

namespace arangodb {
namespace tests {

void init(bool withICU /*= false*/) {
  arangodb::transaction::Methods::clearDataSourceRegistrationCallbacks();
  ClusterEngine::Mocking = true;
  arangodb::RandomGenerator::initialize(arangodb::RandomGenerator::RandomType::MERSENNE);
}

// @Note: once V8 is initialized all 'CATCH' errors will result in SIGILL
void v8Init() {
  struct init_t {
    std::shared_ptr<v8::Platform> platform;
    init_t() {
      platform = std::shared_ptr<v8::Platform>(
        v8::platform::CreateDefaultPlatform(),
        [](v8::Platform* p)->void {
          v8::V8::Dispose();
          v8::V8::ShutdownPlatform();
          delete p;
        }
      );
      v8::V8::InitializePlatform(platform.get()); // avoid SIGSEGV duing 8::Isolate::New(...)
      v8::V8::Initialize(); // avoid error: "Check failed: thread_data_table_"
    }
  };
  static const init_t init;
  (void)(init);
}

bool assertRules(
    TRI_vocbase_t& vocbase,
    std::string const& queryString,
    std::vector<int> expectedRulesIds,
    std::shared_ptr<arangodb::velocypack::Builder> bindVars /* = nullptr */
) {
  std::unordered_set<std::string> expectedRules;
  for (auto ruleId : expectedRulesIds) {
    expectedRules.emplace(arangodb::aql::OptimizerRulesFeature::translateRule(ruleId));
  }

  auto options = arangodb::velocypack::Parser::fromJson(
//    "{ \"tracing\" : 1 }"
    "{ }"
  );

  arangodb::aql::Query query(
    false,
    vocbase,
    arangodb::aql::QueryString(queryString),
    bindVars,
    options,
    arangodb::aql::PART_MAIN
  );

  auto const res = query.explain();

  if (res.result) {
    auto const explanation = res.result->slice();

    arangodb::velocypack::ArrayIterator rules(explanation.get("rules"));

    for (auto const rule : rules) {
      auto const strRule = arangodb::iresearch::getStringRef(rule);
      expectedRules.erase(strRule);
    }
  }

  return expectedRules.empty();
}

arangodb::aql::QueryResult executeQuery(
    TRI_vocbase_t& vocbase,
    std::string const& queryString,
    std::shared_ptr<arangodb::velocypack::Builder> bindVars /*= nullptr*/,
    bool waitForSync /* = false*/
) {
  auto options = arangodb::velocypack::Parser::fromJson(
//    "{ \"tracing\" : 1 }"
    waitForSync ? "{ \"waitForSync\": true }" : "{ }"
  );

  arangodb::aql::Query query(
    false,
    vocbase,
    arangodb::aql::QueryString(queryString),
    bindVars,
    options,
    arangodb::aql::PART_MAIN
  );

  std::shared_ptr<arangodb::aql::SharedQueryState> ss = query.sharedState();

  arangodb::aql::QueryResult result;
  while (true) {
    auto state = query.execute(arangodb::QueryRegistryFeature::registry(), result);
    if (state == arangodb::aql::ExecutionState::WAITING) {
      ss->waitForAsyncResponse();
    } else {
      break;
    }
  }

  return result;
}

std::unique_ptr<arangodb::aql::ExecutionPlan> planFromQuery(
  TRI_vocbase_t& vocbase,
  std::string const& queryString,
  std::shared_ptr<arangodb::velocypack::Builder> bindVars /* = nullptr */
) {
  auto options = arangodb::velocypack::Parser::fromJson(
//    "{ \"tracing\" : 1 }"
    "{ }"
  );

  arangodb::aql::Query query(
    false,
    vocbase,
    arangodb::aql::QueryString(queryString),
    nullptr,
    options,
    arangodb::aql::PART_MAIN
  );

  auto result = query.parse();

  if (result.code != TRI_ERROR_NO_ERROR || !query.ast()) {
    return nullptr;
  }

  return arangodb::aql::ExecutionPlan::instantiateFromAst(query.ast());
}

uint64_t getCurrentPlanVersion() {
  auto const result = arangodb::AgencyComm().getValues("Plan");
  auto const planVersionSlice = result.slice()[0].get<std::string>(
    { arangodb::AgencyCommManager::path(), "Plan", "Version" }
  );
  return planVersionSlice.getNumber<uint64_t>();
}

void setDatabasePath(arangodb::DatabasePathFeature& feature) {
  irs::utf8_path path;

  path /= TRI_GetTempPath();
  path /= std::string("arangodb_tests.") + std::to_string(TRI_microtime());
  const_cast<std::string&>(feature.directory()) = path.utf8();
}

} // tests
} // arangodb

std::string mangleType(std::string name) {
  arangodb::iresearch::kludge::mangleType(name);
  return name;
}

std::string mangleAnalyzer(std::string name) {
  arangodb::iresearch::kludge::mangleAnalyzer(name);
  return name;
}

std::string mangleBool(std::string name) {
  arangodb::iresearch::kludge::mangleBool(name);
  return name;
}

std::string mangleNull(std::string name) {
  arangodb::iresearch::kludge::mangleNull(name);
  return name;
}

std::string mangleNumeric(std::string name) {
  arangodb::iresearch::kludge::mangleNumeric(name);
  return name;
}

std::string mangleString(std::string name, std::string suffix) {
  arangodb::iresearch::kludge::mangleAnalyzer(name);
  name += suffix;
  return name;
}

std::string mangleStringIdentity(std::string name) {
  arangodb::iresearch::kludge::mangleStringField(
    name,
    *arangodb::iresearch::IResearchAnalyzerFeature::identity()
  );
  return name;
}

void assertExpressionFilter(
    std::string const& queryString,
    irs::boost::boost_t boost /*= irs::boost::no_boost()*/,
    std::function<arangodb::aql::AstNode*(arangodb::aql::AstNode*)> const& expressionExtractor /*= &defaultExpressionExtractor*/,
    std::string const& refName /*= "d"*/
) {
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");

  arangodb::aql::Query query(
    false,
    vocbase,
    arangodb::aql::QueryString(queryString),
    nullptr,
    std::make_shared<arangodb::velocypack::Builder>(),
    arangodb::aql::PART_MAIN
  );

  auto const parseResult = query.parse();
  REQUIRE(TRI_ERROR_NO_ERROR == parseResult.code);

  auto* ast = query.ast();
  REQUIRE(ast);

  auto* root = ast->root();
  REQUIRE(root);

  // find first FILTER node
  arangodb::aql::AstNode* filterNode = nullptr;
  for (size_t i = 0; i < root->numMembers(); ++i) {
    auto* node = root->getMemberUnchecked(i);
    REQUIRE(node);

    if (arangodb::aql::NODE_TYPE_FILTER == node->type) {
      filterNode = node;
      break;
    }
  }
  REQUIRE(filterNode);

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

  // supportsFilterCondition
  {
    arangodb::iresearch::QueryContext const ctx{ nullptr, nullptr, nullptr, nullptr, ref };
    CHECK((arangodb::iresearch::FilterFactory::filter(nullptr, ctx, *filterNode)));
  }

  // iteratorForCondition
  {
    arangodb::transaction::Methods trx(
      arangodb::transaction::StandaloneContext::Create(vocbase),
      {},
      {},
      {},
      arangodb::transaction::Options()
    );

    auto dummyPlan = arangodb::tests::planFromQuery(vocbase, "RETURN 1");

    irs::Or expected;
    expected.add<arangodb::iresearch::ByExpression>().init(
      *dummyPlan,
      *ast,
      *expressionExtractor(filterNode)
    );

    irs::Or actual;
    arangodb::iresearch::QueryContext const ctx{ &trx, dummyPlan.get(), ast, &ExpressionContextMock::EMPTY, ref };
    CHECK((arangodb::iresearch::FilterFactory::filter(&actual, ctx, *filterNode)));
    CHECK((expected == actual));
    CHECK(boost == actual.begin()->boost());
  }
}

void assertFilterBoost(
    irs::filter const& expected,
    irs::filter const& actual
) {
  CHECK(expected.boost() == actual.boost());

  auto* expectedBooleanFilter = dynamic_cast<irs::boolean_filter const*>(&expected);

  if (expectedBooleanFilter) {
    auto* actualBooleanFilter = dynamic_cast<irs::boolean_filter const*>(&actual);
    REQUIRE(nullptr != actualBooleanFilter);
    REQUIRE(expectedBooleanFilter->size() == actualBooleanFilter->size());

    auto expectedBegin = expectedBooleanFilter->begin();
    auto expectedEnd = expectedBooleanFilter->end();

    for (auto actualBegin = actualBooleanFilter->begin(); expectedBegin != expectedEnd;) {
      assertFilterBoost(*expectedBegin, *actualBegin);
      ++expectedBegin;
      ++actualBegin;
    }

    return; // we're done
  }

  auto* expectedNegationFilter = dynamic_cast<irs::Not const*>(&expected);

  if (expectedNegationFilter) {
    auto* actualNegationFilter = dynamic_cast<irs::Not const*>(&actual);
    REQUIRE(nullptr != expectedNegationFilter);

    assertFilterBoost(*expectedNegationFilter->filter(), *actualNegationFilter->filter());

    return; // we're done
  }
}

void assertFilter(
    bool parseOk,
    bool execOk,
    std::string const& queryString,
    irs::filter const& expected,
    arangodb::aql::ExpressionContext* exprCtx /*= nullptr*/,
    std::shared_ptr<arangodb::velocypack::Builder> bindVars /*= nullptr*/,
    std::string const& refName /*= "d"*/
) {
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");

  auto options = std::make_shared<arangodb::velocypack::Builder>();

  arangodb::aql::Query query(
    false,
    vocbase,
    arangodb::aql::QueryString(queryString),
    bindVars,
    options,
    arangodb::aql::PART_MAIN
  );

  auto const parseResult = query.parse();
  REQUIRE(TRI_ERROR_NO_ERROR == parseResult.code);

  auto* ast = query.ast();
  REQUIRE(ast);

  auto* root = ast->root();
  REQUIRE(root);

  // find first FILTER node
  arangodb::aql::AstNode* filterNode = nullptr;
  for (size_t i = 0; i < root->numMembers(); ++i) {
    auto* node = root->getMemberUnchecked(i);
    REQUIRE(node);

    if (arangodb::aql::NODE_TYPE_FILTER == node->type) {
      filterNode = node;
      break;
    }
  }
  REQUIRE(filterNode);

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

  // optimization time
  {
    arangodb::iresearch::QueryContext const ctx{ nullptr, nullptr, nullptr, nullptr, ref };
    CHECK((parseOk == arangodb::iresearch::FilterFactory::filter(nullptr, ctx, *filterNode)));
  }

  // execution time
  {
    arangodb::transaction ::Methods trx(
      arangodb::transaction::StandaloneContext::Create(vocbase),
      {},
      {},
      {},
      arangodb::transaction::Options()
    );

    auto dummyPlan = arangodb::tests::planFromQuery(vocbase, "RETURN 1");

    irs::Or actual;
    arangodb::iresearch::QueryContext const ctx{ &trx, dummyPlan.get(), ast, exprCtx, ref };
    CHECK((execOk == arangodb::iresearch::FilterFactory::filter(&actual, ctx, *filterNode)));
    CHECK((!execOk || (expected == actual)));

    if (execOk) {
      assertFilterBoost(expected, actual);
    }
  }
}

void assertFilterSuccess(
    std::string const& queryString,
    irs::filter const& expected,
    arangodb::aql::ExpressionContext* exprCtx /*= nullptr*/,
    std::shared_ptr<arangodb::velocypack::Builder> bindVars /*= nullptr*/,
    std::string const& refName /*= "d"*/
) {
  return assertFilter(true, true, queryString, expected, exprCtx, bindVars, refName);
}

void assertFilterFail(
    std::string const& queryString,
    arangodb::aql::ExpressionContext* exprCtx /*= nullptr*/,
    std::shared_ptr<arangodb::velocypack::Builder> bindVars /*= nullptr*/,
    std::string const& refName /*= "d"*/
) {
  irs::Or expected;
  return assertFilter(false, false, queryString, expected, exprCtx, bindVars, refName);
}

void assertFilterExecutionFail(
    std::string const& queryString,
    arangodb::aql::ExpressionContext* exprCtx /*= nullptr*/,
    std::shared_ptr<arangodb::velocypack::Builder> bindVars /*= nullptr*/,
    std::string const& refName /*= "d"*/
) {
  irs::Or expected;
  return assertFilter(true, false, queryString, expected, exprCtx, bindVars, refName);
}

void assertFilterParseFail(
    std::string const& queryString,
    std::shared_ptr<arangodb::velocypack::Builder> bindVars /*= nullptr*/
) {
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");

  arangodb::aql::Query query(
    false,
    vocbase,
    arangodb::aql::QueryString(queryString),
    bindVars,
    nullptr,
    arangodb::aql::PART_MAIN
  );

  auto const parseResult = query.parse();
  CHECK(TRI_ERROR_NO_ERROR != parseResult.code);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
