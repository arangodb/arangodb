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

#include "Agency/AgencyComm.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Aql/AqlItemBlockSerializationFormat.h"
#include "Aql/Ast.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/ExpressionContext.h"
#include "Aql/IResearchViewNode.h"
#include "Aql/OptimizerRulesFeature.h"
#include "Aql/QueryRegistry.h"
#include "Basics/FileUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/files.h"
#include "Basics/system-functions.h"
#include "ClusterEngine/ClusterEngine.h"
#include "ExpressionContextMock.h"
#include "IResearch/AqlHelper.h"
#include "IResearch/ExpressionFilter.h"
#include "IResearch/IResearchFilterFactory.h"
#include "IResearch/IResearchKludge.h"
#include "IResearch/VelocyPackHelper.h"
#include "Logger/LogMacros.h"
#include "Random/RandomGenerator.h"
#include "RestServer/DatabasePathFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "Transaction/Methods.h"
#include "Transaction/StandaloneContext.h"
#include "VocBase/KeyGenerator.h"
#include "common.h"
#include "gtest/gtest.h"
#include "search/scorers.hpp"

#include "search/boolean_filter.hpp"

#include "3rdParty/iresearch/tests/tests_config.hpp"

#include <libplatform/libplatform.h>
#include <v8.h>

#include <velocypack/Iterator.h>
#include <velocypack/Parser.h>

#include <unordered_set>

extern const char* ARGV0;  // defined in main.cpp

namespace {

struct BoostScorer : public irs::sort {
  struct Prepared : public prepared_base<irs::boost_t, void> {
   public:
    DECLARE_FACTORY(Prepared);

    Prepared() = default;

    virtual void add(irs::byte_type* dst, irs::byte_type const* src) const override {
      score_cast(dst) += score_cast(src);
    }

    virtual void collect(irs::byte_type* stats, const irs::index_reader& index,
                         const irs::sort::field_collector* field,
                         const irs::sort::term_collector* term) const override {
      // NOOP
    }

    virtual irs::flags const& features() const override {
      return irs::flags::empty_instance();
    }

    virtual bool less(irs::byte_type const* lhs, irs::byte_type const* rhs) const override {
      return score_cast(lhs) < score_cast(rhs);
    }

    virtual irs::sort::field_collector::ptr prepare_field_collector() const override {
      return nullptr;
    }

    virtual void prepare_score(irs::byte_type* score) const override {
      score_cast(score) = 0.f;
    }

    virtual irs::sort::term_collector::ptr prepare_term_collector() const override {
      return nullptr;
    }

    virtual std::pair<score_ctx::ptr, irs::score_f> prepare_scorer(
        irs::sub_reader const&, irs::term_reader const&, irs::byte_type const*,
        irs::attribute_view const&, irs::boost_t boost) const override {
      struct ScoreCtx : public irs::sort::score_ctx {
        ScoreCtx(irs::boost_t score) : scr(score) {}

        irs::boost_t scr;
      };

      return {std::make_unique<ScoreCtx>(boost),
              [](const void* ctx, irs::byte_type* score_buf) noexcept {
                  auto & state = *static_cast<const ScoreCtx*>(ctx);
      irs::sort::score_cast<irs::boost_t>(score_buf) = state.scr;
    }
  };
}
};  // namespace

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
}
;  // BoostScorer

REGISTER_SCORER_JSON(BoostScorer, BoostScorer::make);

struct CustomScorer : public irs::sort {
  struct Prepared : public irs::sort::prepared_base<float_t, void> {
   public:
    DECLARE_FACTORY(Prepared);

    Prepared(float_t i) : i(i) {}

    virtual void add(irs::byte_type* dst, const irs::byte_type* src) const override {
      score_cast(dst) += score_cast(src);
    }

    virtual void collect(irs::byte_type* stats, const irs::index_reader& index,
                         const irs::sort::field_collector* field,
                         const irs::sort::term_collector* term) const override {
      // NOOP
    }

    virtual irs::flags const& features() const override {
      return irs::flags::empty_instance();
    }

    virtual bool less(const irs::byte_type* lhs, const irs::byte_type* rhs) const override {
      return score_cast(lhs) < score_cast(rhs);
    }

    virtual irs::sort::field_collector::ptr prepare_field_collector() const override {
      return nullptr;
    }

    virtual void prepare_score(irs::byte_type* score) const override {
      score_cast(score) = 0.f;
    }

    virtual irs::sort::term_collector::ptr prepare_term_collector() const override {
      return nullptr;
    }

    virtual std::pair<score_ctx::ptr, irs::score_f> prepare_scorer(
        irs::sub_reader const&, irs::term_reader const&, irs::byte_type const*,
        irs::attribute_view const&, irs::boost_t) const override {
      struct ScoreCtx : public irs::sort::score_ctx {
        ScoreCtx(float_t score) : i(score) {}

        float_t i;
      };

      return {std::make_unique<ScoreCtx>(i),
              [](const void* ctx, irs::byte_type* score_buf) noexcept {
                  auto & state = *static_cast<const ScoreCtx*>(ctx);
      irs::sort::score_cast<float_t>(score_buf) = state.i;
    }
  };
}

float_t i;
}
;

static ::iresearch::sort::type_id const& type() {
  static ::iresearch::sort::type_id TYPE("customscorer");
  return TYPE;
}

static irs::sort::ptr make(irs::string_ref const& args) {
  if (args.null()) {
    return std::make_shared<CustomScorer>(0u);
  }

  // velocypack::Parser::fromJson(...) will throw exception on parse error
  auto json = arangodb::velocypack::Parser::fromJson(args.c_str(), args.size());
  auto slice = json ? json->slice() : arangodb::velocypack::Slice();

  if (!slice.isArray()) {
    return nullptr;  // incorrect argument format
  }

  arangodb::velocypack::ArrayIterator itr(slice);

  if (!itr.valid()) {
    return nullptr;
  }

  auto const value = itr.value();

  if (!value.isNumber()) {
    return nullptr;
  }

  return std::make_shared<CustomScorer>(itr.value().getNumber<size_t>());
}

CustomScorer(size_t i) : irs::sort(CustomScorer::type()), i(i) {}

virtual irs::sort::prepared::ptr prepare() const override {
  return irs::memory::make_unique<Prepared>(static_cast<float_t>(i));
}

size_t i;
}
;  // CustomScorer

REGISTER_SCORER_JSON(CustomScorer, CustomScorer::make);

static const VPackBuilder testDatabaseBuilder = dbArgsBuilder("testVocbase");
static const VPackSlice   testDatabaseArgs = testDatabaseBuilder.slice();
}  // namespace

namespace arangodb {

namespace tests {

std::string const AnalyzerCollectionName("_analyzers");

std::string testResourceDir;

static void findIResearchTestResources() {
  std::string toBeFound =
      basics::FileUtils::buildFilename("3rdParty", "iresearch", "tests",
                                       "resources");

  // peek into environment variable first
  char const* dir = getenv("IRESEARCH_TEST_RESOURCE_DIR");
  if (dir != nullptr) {
    // environment variable set, so use it
    testResourceDir = std::string(dir);
  } else {
    // environment variable not set, so try to auto-detect the location
    testResourceDir = ".";
    do {
      if (basics::FileUtils::isDirectory(
              basics::FileUtils::buildFilename(testResourceDir, toBeFound))) {
        testResourceDir = basics::FileUtils::buildFilename(testResourceDir, toBeFound);
        return;
      }
      testResourceDir = basics::FileUtils::buildFilename(testResourceDir, "..");
      if (!basics::FileUtils::isDirectory(testResourceDir)) {
        testResourceDir = IResearch_test_resource_dir;
        break;
      }
    } while (true);
  }

  if (!basics::FileUtils::isDirectory(testResourceDir)) {
    LOG_TOPIC("45f9d", ERR, Logger::FIXME)
        << "unable to find directory for IResearch test resources. use "
           "environment variable IRESEARCH_TEST_RESOURCE_DIR to set it";
  }
}

void init(bool withICU /*= false*/) {
  arangodb::transaction::Methods::clearDataSourceRegistrationCallbacks();
  ClusterEngine::Mocking = true;
  arangodb::RandomGenerator::initialize(arangodb::RandomGenerator::RandomType::MERSENNE);

  // try to locate directory for iresearch test resource files
  if (testResourceDir.empty()) {
    findIResearchTestResources();
  }
}

// @Note: once V8 is initialized all 'CATCH' errors will result in SIGILL
void v8Init() {
  struct init_t {
    std::shared_ptr<v8::Platform> platform;
    init_t() {
      platform = std::shared_ptr<v8::Platform>(v8::platform::CreateDefaultPlatform(),
                                               [](v8::Platform* p) -> void {
                                                 v8::V8::Dispose();
                                                 v8::V8::ShutdownPlatform();
                                                 delete p;
                                               });
      v8::V8::InitializePlatform(platform.get());  // avoid SIGSEGV duing 8::Isolate::New(...)
      v8::V8::Initialize();  // avoid error: "Check failed: thread_data_table_"
    }
  };
  static const init_t init;
  (void)(init);
}

bool assertRules(TRI_vocbase_t& vocbase, std::string const& queryString,
                 std::vector<int> expectedRulesIds,
                 std::shared_ptr<arangodb::velocypack::Builder> bindVars /* = nullptr */,
                 std::string const& optionsString /*= "{}"*/
) {
  std::unordered_set<std::string> expectedRules;
  for (auto ruleId : expectedRulesIds) {
    arangodb::velocypack::StringRef translated = arangodb::aql::OptimizerRulesFeature::translateRule(ruleId);
    expectedRules.emplace(translated.toString());
  }

  arangodb::aql::Query query(false, vocbase, arangodb::aql::QueryString(queryString), bindVars,
                             arangodb::velocypack::Parser::fromJson(optionsString),
                             arangodb::aql::PART_MAIN);

  auto const res = query.explain();

  if (res.data) {
    auto const explanation = res.data->slice();

    arangodb::velocypack::ArrayIterator rules(explanation.get("rules"));

    for (auto const& rule : rules) {
      auto const strRule = arangodb::iresearch::getStringRef(rule);
      expectedRules.erase(strRule);
    }
  }

  return expectedRules.empty();
}

arangodb::aql::QueryResult executeQuery(TRI_vocbase_t& vocbase, std::string const& queryString,
                                        std::shared_ptr<arangodb::velocypack::Builder> bindVars /*= nullptr*/,
                                        std::string const& optionsString /*= "{}"*/
) {
  arangodb::aql::Query query(false, vocbase, arangodb::aql::QueryString(queryString), bindVars,
                             arangodb::velocypack::Parser::fromJson(optionsString),
                             arangodb::aql::PART_MAIN);

  std::shared_ptr<arangodb::aql::SharedQueryState> ss = query.sharedState();
  ss->resetWakeupHandler();

  arangodb::aql::QueryResult result;
  while (true) {
    auto state = query.execute(arangodb::QueryRegistryFeature::registry(), result);
    if (state == arangodb::aql::ExecutionState::WAITING) {
      ss->waitForAsyncWakeup();
    } else {
      break;
    }
  }
  return result;
}

std::unique_ptr<arangodb::aql::ExecutionPlan> planFromQuery(
    TRI_vocbase_t& vocbase, std::string const& queryString,
    std::shared_ptr<arangodb::velocypack::Builder> bindVars /* = nullptr */,
    std::string const& optionsString /*= "{}"*/
) {
  arangodb::aql::Query query(false, vocbase, arangodb::aql::QueryString(queryString),
                             nullptr, arangodb::velocypack::Parser::fromJson(optionsString),
                             arangodb::aql::PART_MAIN);

  auto result = query.parse();

  if (result.result.fail() || !query.ast()) {
    return nullptr;
  }

  return arangodb::aql::ExecutionPlan::instantiateFromAst(query.ast());
}

std::unique_ptr<arangodb::aql::Query> prepareQuery(
    TRI_vocbase_t& vocbase, std::string const& queryString,
    std::shared_ptr<arangodb::velocypack::Builder> bindVars /* = nullptr */,
    std::string const& optionsString /*= "{}"*/
) {
  auto query = std::make_unique<arangodb::aql::Query>(
      false, vocbase, arangodb::aql::QueryString(queryString), nullptr,
      arangodb::velocypack::Parser::fromJson(optionsString), arangodb::aql::PART_MAIN);

  query->prepare(arangodb::QueryRegistryFeature::registry(),
                 arangodb::aql::SerializationFormat::SHADOWROWS);
  return query;
}

uint64_t getCurrentPlanVersion() {
  auto const result = arangodb::AgencyComm().getValues("Plan");
  auto const planVersionSlice = result.slice()[0].get<std::string>(
      {arangodb::AgencyCommManager::path(), "Plan", "Version"});
  return planVersionSlice.getNumber<uint64_t>();
}

void setDatabasePath(arangodb::DatabasePathFeature& feature) {
  irs::utf8_path path;

  path /= TRI_GetTempPath();
  path /= std::string("arangodb_tests.") + std::to_string(TRI_microtime());
  const_cast<std::string&>(feature.directory()) = path.utf8();
}

void expectEqualSlices_(const VPackSlice& lhs, const VPackSlice& rhs, const char* where) {
  SCOPED_TRACE(rhs.toString());
  SCOPED_TRACE(rhs.toHex());
  SCOPED_TRACE("----ACTUAL----");
  SCOPED_TRACE(lhs.toString());
  SCOPED_TRACE(lhs.toHex());
  SCOPED_TRACE("---EXPECTED---");
  SCOPED_TRACE(where);
  EXPECT_EQ(0, arangodb::basics::VelocyPackHelper::compare(lhs, rhs, true));
}

}  // namespace tests
}  // namespace arangodb

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
  arangodb::iresearch::kludge::mangleStringField(name, arangodb::iresearch::FieldMeta::Analyzer()  // args
  );

  return name;
}

void assertFilterOptimized(TRI_vocbase_t& vocbase, std::string const& queryString,
                           irs::filter const& expectedFilter,
                           arangodb::aql::ExpressionContext* exprCtx /*= nullptr*/,
                           std::shared_ptr<arangodb::velocypack::Builder> bindVars /* = nullptr */
) {
  auto options = arangodb::velocypack::Parser::fromJson(
      //    "{ \"tracing\" : 1 }"
      "{ }");

  arangodb::aql::Query query(false, vocbase, arangodb::aql::QueryString(queryString),
                             bindVars, options, arangodb::aql::PART_MAIN);

  query.prepare(arangodb::QueryRegistryFeature::registry(),
                arangodb::aql::SerializationFormat::SHADOWROWS);
  EXPECT_TRUE(query.plan());
  auto& plan = *query.plan();

  arangodb::containers::SmallVector<arangodb::aql::ExecutionNode*>::allocator_type::arena_type a;
  arangodb::containers::SmallVector<arangodb::aql::ExecutionNode*> nodes{a};
  plan.findNodesOfType(nodes, arangodb::aql::ExecutionNode::ENUMERATE_IRESEARCH_VIEW, true);

  EXPECT_EQ(nodes.size(), 1);

  auto* viewNode =
      arangodb::aql::ExecutionNode::castTo<arangodb::iresearch::IResearchViewNode*>(
          nodes.front());

  EXPECT_TRUE(viewNode);

  // execution time
  {
    arangodb::transaction ::Methods trx(arangodb::transaction::StandaloneContext::Create(vocbase),
                                        {}, {}, {}, arangodb::transaction::Options());

    irs::Or actualFilter;
    arangodb::iresearch::QueryContext const ctx{&trx, &plan, plan.getAst(),
                                                exprCtx, &viewNode->outVariable()};
    EXPECT_TRUE(arangodb::iresearch::FilterFactory::filter(&actualFilter, ctx,
                                                           viewNode->filterCondition())
                    .ok());
    EXPECT_FALSE(actualFilter.empty());
    EXPECT_EQ(expectedFilter, *actualFilter.begin());
  }
}

void assertExpressionFilter(
    TRI_vocbase_t& vocbase, std::string const& queryString, irs::boost_t boost /*= irs::no_boost()*/,
    std::function<arangodb::aql::AstNode*(arangodb::aql::AstNode*)> const& expressionExtractor /*= &defaultExpressionExtractor*/,
    std::string const& refName /*= "d"*/
) {
  arangodb::aql::Query query(false, vocbase, arangodb::aql::QueryString(queryString), nullptr,
                             std::make_shared<arangodb::velocypack::Builder>(),
                             arangodb::aql::PART_MAIN);

  auto const parseResult = query.parse();
  ASSERT_TRUE(parseResult.result.ok());

  auto* ast = query.ast();
  ASSERT_TRUE(ast);

  auto* root = ast->root();
  ASSERT_TRUE(root);

  // find first FILTER node
  arangodb::aql::AstNode* filterNode = nullptr;
  for (size_t i = 0; i < root->numMembers(); ++i) {
    auto* node = root->getMemberUnchecked(i);
    ASSERT_TRUE(node);

    if (arangodb::aql::NODE_TYPE_FILTER == node->type) {
      filterNode = node;
      break;
    }
  }
  ASSERT_TRUE(filterNode);

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

  // supportsFilterCondition
  {
    arangodb::transaction::Methods trx(arangodb::transaction::StandaloneContext::Create(vocbase),
                                       {}, {}, {}, arangodb::transaction::Options());
    arangodb::iresearch::QueryContext const ctx{&trx, nullptr, nullptr, nullptr, ref};
    EXPECT_TRUE(
        (arangodb::iresearch::FilterFactory::filter(nullptr, ctx, *filterNode).ok()));
  }

  // iteratorForCondition
  {
    arangodb::transaction::Methods trx(arangodb::transaction::StandaloneContext::Create(vocbase),
                                       {}, {}, {}, arangodb::transaction::Options());

    auto dummyPlan = arangodb::tests::planFromQuery(vocbase, "RETURN 1");

    irs::Or expected;
    expected.add<arangodb::iresearch::ByExpression>().init(*dummyPlan, *ast,
                                                           *expressionExtractor(filterNode));

    irs::Or actual;
    arangodb::iresearch::QueryContext const ctx{&trx, dummyPlan.get(), ast,
                                                &ExpressionContextMock::EMPTY, ref};
    EXPECT_TRUE(
        (arangodb::iresearch::FilterFactory::filter(&actual, ctx, *filterNode).ok()));
    EXPECT_EQ(expected, actual);
    EXPECT_EQ(boost, actual.begin()->boost());
  }
}

void assertFilterBoost(irs::filter const& expected, irs::filter const& actual) {
  EXPECT_EQ(expected.boost(), actual.boost());

  auto* expectedBooleanFilter = dynamic_cast<irs::boolean_filter const*>(&expected);

  if (expectedBooleanFilter) {
    auto* actualBooleanFilter = dynamic_cast<irs::boolean_filter const*>(&actual);
    ASSERT_NE(nullptr, actualBooleanFilter);
    ASSERT_EQ(expectedBooleanFilter->size(), actualBooleanFilter->size());

    auto expectedBegin = expectedBooleanFilter->begin();
    auto expectedEnd = expectedBooleanFilter->end();

    for (auto actualBegin = actualBooleanFilter->begin(); expectedBegin != expectedEnd;) {
      assertFilterBoost(*expectedBegin, *actualBegin);
      ++expectedBegin;
      ++actualBegin;
    }

    return;  // we're done
  }

  auto* expectedNegationFilter = dynamic_cast<irs::Not const*>(&expected);

  if (expectedNegationFilter) {
    auto* actualNegationFilter = dynamic_cast<irs::Not const*>(&actual);
    ASSERT_NE(nullptr, expectedNegationFilter);

    assertFilterBoost(*expectedNegationFilter->filter(), *actualNegationFilter->filter());

    return;  // we're done
  }
}

void buildActualFilter(TRI_vocbase_t& vocbase,
                  std::string const& queryString, irs::filter& actual,
                  arangodb::aql::ExpressionContext* exprCtx /*= nullptr*/,
                  std::shared_ptr<arangodb::velocypack::Builder> bindVars /*= nullptr*/,
                  std::string const& refName /*= "d"*/
) {
  auto options = std::make_shared<arangodb::velocypack::Builder>();

  arangodb::aql::Query query(false, vocbase, arangodb::aql::QueryString(queryString),
                             bindVars, options, arangodb::aql::PART_MAIN);

  auto const parseResult = query.parse();
  ASSERT_TRUE(parseResult.result.ok());

  auto* ast = query.ast();
  ASSERT_TRUE(ast);

  auto* root = ast->root();
  ASSERT_TRUE(root);

  // find first FILTER node
  arangodb::aql::AstNode* filterNode = nullptr;
  for (size_t i = 0; i < root->numMembers(); ++i) {
    auto* node = root->getMemberUnchecked(i);
    ASSERT_TRUE(node);

    if (arangodb::aql::NODE_TYPE_FILTER == node->type) {
      filterNode = node;
      break;
    }
  }
  ASSERT_TRUE(filterNode);

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

  // optimization time
  {
    arangodb::transaction ::Methods trx(arangodb::transaction::StandaloneContext::Create(vocbase),
                                        {}, {}, {}, arangodb::transaction::Options());

    arangodb::iresearch::QueryContext const ctx{&trx, nullptr, nullptr, nullptr, ref};
    ASSERT_TRUE(arangodb::iresearch::FilterFactory::filter(nullptr, ctx, *filterNode).ok());
  }

  // execution time
  {
    arangodb::transaction ::Methods trx(arangodb::transaction::StandaloneContext::Create(vocbase),
                                        {}, {}, {}, arangodb::transaction::Options());

    auto dummyPlan = arangodb::tests::planFromQuery(vocbase, "RETURN 1");
    arangodb::iresearch::QueryContext const ctx{&trx, dummyPlan.get(), ast, exprCtx, ref};
    ASSERT_TRUE(arangodb::iresearch::FilterFactory::filter(dynamic_cast<irs::boolean_filter*>(&actual), ctx, *filterNode).ok());
  }
}

void assertFilter(TRI_vocbase_t& vocbase, bool parseOk, bool execOk,
                  std::string const& queryString, irs::filter const& expected,
                  arangodb::aql::ExpressionContext* exprCtx /*= nullptr*/,
                  std::shared_ptr<arangodb::velocypack::Builder> bindVars /*= nullptr*/,
                  std::string const& refName /*= "d"*/
) {
  SCOPED_TRACE(testing::Message("assertFilter failed for query:<") << queryString << "> parseOk:" << parseOk << " execOk:" << execOk);
  auto options = std::make_shared<arangodb::velocypack::Builder>();

  arangodb::aql::Query query(false, vocbase, arangodb::aql::QueryString(queryString),
                             bindVars, options, arangodb::aql::PART_MAIN);

  auto const parseResult = query.parse();
  ASSERT_TRUE(parseResult.result.ok());

  auto* ast = query.ast();
  ASSERT_TRUE(ast);

  auto* root = ast->root();
  ASSERT_TRUE(root);

  // find first FILTER node
  arangodb::aql::AstNode* filterNode = nullptr;
  for (size_t i = 0; i < root->numMembers(); ++i) {
    auto* node = root->getMemberUnchecked(i);
    ASSERT_TRUE(node);

    if (arangodb::aql::NODE_TYPE_FILTER == node->type) {
      filterNode = node;
      break;
    }
  }
  ASSERT_TRUE(filterNode);

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

  // optimization time
  {
    arangodb::transaction ::Methods trx(arangodb::transaction::StandaloneContext::Create(vocbase),
                                        {}, {}, {}, arangodb::transaction::Options());

    arangodb::iresearch::QueryContext const ctx{&trx, nullptr, nullptr, nullptr, ref};
    EXPECT_TRUE(
        (parseOk ==
         arangodb::iresearch::FilterFactory::filter(nullptr, ctx, *filterNode).ok()));
  }

  // execution time
  {
    arangodb::transaction ::Methods trx(arangodb::transaction::StandaloneContext::Create(vocbase),
                                        {}, {}, {}, arangodb::transaction::Options());

    auto dummyPlan = arangodb::tests::planFromQuery(vocbase, "RETURN 1");

    irs::Or actual;
    arangodb::iresearch::QueryContext const ctx{&trx, dummyPlan.get(), ast, exprCtx, ref};
    EXPECT_TRUE(
        (execOk ==
         arangodb::iresearch::FilterFactory::filter(&actual, ctx, *filterNode).ok()));
    EXPECT_TRUE(!execOk || (expected == actual));

    if (execOk) {
      assertFilterBoost(expected, actual);
    }
  }
}

void assertFilterSuccess(TRI_vocbase_t& vocbase, std::string const& queryString,
                         irs::filter const& expected,
                         arangodb::aql::ExpressionContext* exprCtx /*= nullptr*/,
                         std::shared_ptr<arangodb::velocypack::Builder> bindVars /*= nullptr*/,
                         std::string const& refName /*= "d"*/
) {
  return assertFilter(vocbase, true, true, queryString, expected, exprCtx, bindVars, refName);
}

void assertFilterFail(TRI_vocbase_t& vocbase, std::string const& queryString,
                      arangodb::aql::ExpressionContext* exprCtx /*= nullptr*/,
                      std::shared_ptr<arangodb::velocypack::Builder> bindVars /*= nullptr*/,
                      std::string const& refName /*= "d"*/
) {
  irs::Or expected;
  return assertFilter(vocbase, false, false, queryString, expected, exprCtx, bindVars, refName);
}

void assertFilterExecutionFail(TRI_vocbase_t& vocbase, std::string const& queryString,
                               arangodb::aql::ExpressionContext* exprCtx /*= nullptr*/,
                               std::shared_ptr<arangodb::velocypack::Builder> bindVars /*= nullptr*/,
                               std::string const& refName /*= "d"*/
) {
  irs::Or expected;
  return assertFilter(vocbase, true, false, queryString, expected, exprCtx, bindVars, refName);
}

void assertFilterParseFail(TRI_vocbase_t& vocbase, std::string const& queryString,
                           std::shared_ptr<arangodb::velocypack::Builder> bindVars /*= nullptr*/
) {
  arangodb::aql::Query query(false, vocbase, arangodb::aql::QueryString(queryString),
                             bindVars, nullptr, arangodb::aql::PART_MAIN);

  auto const parseResult = query.parse();
  ASSERT_TRUE(parseResult.result.fail());
}

arangodb::CreateDatabaseInfo createInfo(arangodb::application_features::ApplicationServer& server, std::string const& name, uint64_t id, bool allowSystem) {
  arangodb::CreateDatabaseInfo info(server);
  info.allowSystemDB(allowSystem);
  auto rv = info.load(name, id);
  if(rv.fail()) {
    throw std::runtime_error(rv.errorMessage());
  }
  return info;
};

arangodb::CreateDatabaseInfo systemDBInfo(arangodb::application_features::ApplicationServer& server, std::string const& name, uint64_t id) {
  auto rv =  createInfo(server, name, id, true);
  return rv;
};

arangodb::CreateDatabaseInfo testDBInfo(arangodb::application_features::ApplicationServer& server, std::string const& name, uint64_t id) {
  return createInfo(server, name, id);
};

arangodb::CreateDatabaseInfo unknownDBInfo(arangodb::application_features::ApplicationServer& server, std::string const& name, uint64_t id) {
  return createInfo(server, name, id);
};

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
