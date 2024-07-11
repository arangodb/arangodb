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

#include "../3rdParty/iresearch/tests/tests_config.hpp"
#include "analysis/token_attributes.hpp"
#include "index/directory_reader.hpp"
#include "search/all_filter.hpp"
#include "search/cost.hpp"
#include "search/score.hpp"
#include "search/scorers.hpp"
#include "store/memory_directory.hpp"
#include "store/store_utils.hpp"
#include "utils/type_limits.hpp"
#include <filesystem>

#include "velocypack/Iterator.h"

#include "IResearch/ExpressionContextMock.h"
#include "IResearch/common.h"
#include "Mocks/LogLevels.h"
#include "Mocks/StorageEngineMock.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Aql/AqlFunctionFeature.h"
#include "Aql/Ast.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/OptimizerRulesFeature.h"
#include "Aql/Query.h"
#include "Basics/VelocyPackHelper.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "IResearch/ExpressionFilter.h"
#include "IResearch/IResearchAnalyzerFeature.h"
#include "IResearch/IResearchCommon.h"
#include "IResearch/IResearchFeature.h"
#include "IResearch/IResearchFilterContext.h"
#include "IResearch/IResearchFilterFactory.h"
#include "IResearch/IResearchView.h"
#include "IResearch/VelocyPackHelper.h"
#include "Logger/LogTopic.h"
#include "Logger/Logger.h"
#include "RestServer/AqlFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/DatabasePathFeature.h"
#include "Metrics/MetricsFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "RestServer/SystemDatabaseFeature.h"
#include "RestServer/ViewTypesFeature.h"
#include "Sharding/ShardingFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "ProgramOptions/ProgramOptions.h"
#include "Transaction/Methods.h"
#include "Transaction/StandaloneContext.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/LogicalView.h"
#include "Cluster/ClusterFeature.h"
#include "Metrics/ClusterMetricsFeature.h"
#include "Statistics/StatisticsFeature.h"
#include "Statistics/StatisticsWorker.h"

extern const char* ARGV0;  // defined in main.cpp

namespace {

struct custom_sort final : public irs::ScorerBase<void> {
  static constexpr std::string_view type_name() noexcept {
    return "custom_sort";
  }

  class field_collector final : public irs::FieldCollector {
   public:
    field_collector(const custom_sort& sort) : sort_(sort) {}

    void collect(const irs::SubReader& segment,
                 const irs::term_reader& field) final {
      if (sort_.fieldCollectorCollect) {
        sort_.fieldCollectorCollect(segment, field);
      }
    }

    void collect(irs::bytes_view in) final {}

    void reset() final {}

    void write(irs::data_output& out) const final {}

   private:
    const custom_sort& sort_;
  };

  class term_collector final : public irs::TermCollector {
   public:
    term_collector(const custom_sort& sort) : sort_(sort) {}

    void collect(const irs::SubReader& segment, const irs::term_reader& field,
                 const irs::attribute_provider& term_attrs) final {
      if (sort_.termCollectorCollect) {
        sort_.termCollectorCollect(segment, field, term_attrs);
      }
    }

    void collect(irs::bytes_view in) final {}

    void reset() final {}

    void write(irs::data_output& out) const final {}

   private:
    const custom_sort& sort_;
  };

  struct scorer final : public irs::score_ctx {
    scorer(const custom_sort& sort, irs::ColumnProvider const& segment,
           std::map<irs::type_info::type_id, irs::field_id> const& features,
           irs::byte_type const* stats,
           irs::attribute_provider const& doc_attrs)
        : document_attrs_(doc_attrs),
          stats_(stats),
          segment_reader_(segment),
          sort_(sort) {}

    const irs::attribute_provider& document_attrs_;
    const irs::byte_type* stats_;
    const irs::ColumnProvider& segment_reader_;
    const custom_sort& sort_;
  };

  void collect(irs::byte_type* filter_attrs, const irs::FieldCollector* field,
               const irs::TermCollector* term) const final {
    if (collectorFinish) {
      collectorFinish(filter_attrs, field, term);
    }
  }

  irs::IndexFeatures index_features() const final {
    return irs::IndexFeatures::NONE;
  }

  irs::FieldCollector::ptr prepare_field_collector() const final {
    if (prepareFieldCollector) {
      return prepareFieldCollector();
    }

    return std::make_unique<custom_sort::field_collector>(*this);
  }

  irs::ScoreFunction prepare_scorer(
      irs::ColumnProvider const& segment,
      std::map<irs::type_info::type_id, irs::field_id> const& features,
      irs::byte_type const* stats, irs::attribute_provider const& doc_attrs,
      irs::score_t boost) const final {
    if (prepareScorer) {
      return prepareScorer(segment, features, stats, doc_attrs, boost);
    }

    return irs::ScoreFunction::Make<custom_sort::scorer>(
        [](irs::score_ctx* ctx, irs::score_t* res) noexcept {
          auto& ctxImpl = *reinterpret_cast<const custom_sort::scorer*>(ctx);

          EXPECT_TRUE(res);

          auto doc_id = irs::get<irs::document>(ctxImpl.document_attrs_)->value;

          if (ctxImpl.sort_.scorerScore) {
            ctxImpl.sort_.scorerScore(doc_id, res);
          }
        },
        irs::ScoreFunction::DefaultMin, *this, segment, features, stats,
        doc_attrs);
  }

  irs::TermCollector::ptr prepare_term_collector() const final {
    if (prepareTermCollector) {
      return prepareTermCollector();
    }

    return std::make_unique<custom_sort::term_collector>(*this);
  }

  std::function<void(const irs::SubReader&, const irs::term_reader&)>
      fieldCollectorCollect;
  std::function<void(const irs::SubReader&, const irs::term_reader&,
                     const irs::attribute_provider&)>
      termCollectorCollect;
  std::function<void(irs::byte_type*, irs::FieldCollector const*,
                     irs::TermCollector const*)>
      collectorFinish;
  std::function<irs::FieldCollector::ptr()> prepareFieldCollector;
  std::function<irs::ScoreFunction(
      const irs::ColumnProvider& segment,
      std::map<irs::type_info::type_id, irs::field_id> const& features,
      irs::byte_type const* stats, irs::attribute_provider const& doc_attrs,
      irs::score_t boost)>
      prepareScorer;
  std::function<irs::TermCollector::ptr()> prepareTermCollector;
  std::function<void(irs::doc_id_t&, irs::score_t*)> scorerScore;

  custom_sort() = default;
};

struct IResearchExpressionFilterTest
    : public ::testing::Test,
      public arangodb::tests::LogSuppressor<arangodb::Logger::AUTHENTICATION,
                                            arangodb::LogLevel::ERR>,
      public arangodb::tests::LogSuppressor<arangodb::iresearch::TOPIC,
                                            arangodb::LogLevel::FATAL>,
      public arangodb::tests::IResearchLogSuppressor {
  arangodb::ArangodServer server;
  StorageEngineMock engine;
  std::unique_ptr<TRI_vocbase_t> system;
  std::vector<
      std::pair<arangodb::application_features::ApplicationFeature&, bool>>
      features;

  IResearchExpressionFilterTest()
      : server(
            std::make_shared<arangodb::options::ProgramOptions>("", "", "", ""),
            nullptr),
        engine(server) {
    arangodb::tests::init(true);

    // setup required application features
    features.emplace_back(server.addFeature<arangodb::ViewTypesFeature>(),
                          true);
    features.emplace_back(server.addFeature<arangodb::AuthenticationFeature>(),
                          true);
    features.emplace_back(server.addFeature<arangodb::DatabasePathFeature>(),
                          false);
    features.emplace_back(server.addFeature<arangodb::DatabaseFeature>(),
                          false);
    auto& selector = server.addFeature<arangodb::EngineSelectorFeature>();
    features.emplace_back(selector, false);
    server.getFeature<arangodb::EngineSelectorFeature>().setEngineTesting(
        &engine);
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
    features.emplace_back(
        server.addFeature<arangodb::QueryRegistryFeature>(
            server.template getFeature<arangodb::metrics::MetricsFeature>()),
        false);  // must be first
    system = std::make_unique<TRI_vocbase_t>(systemDBInfo(server));
    features.emplace_back(
        server.addFeature<arangodb::SystemDatabaseFeature>(system.get()),
        false);  // required for IResearchAnalyzerFeature
    features.emplace_back(server.addFeature<arangodb::AqlFeature>(), true);
    features.emplace_back(server.addFeature<arangodb::ShardingFeature>(),
                          false);
    features.emplace_back(
        server.addFeature<arangodb::aql::OptimizerRulesFeature>(), true);
    features.emplace_back(
        server.addFeature<arangodb::aql::AqlFunctionFeature>(),
        true);  // required for IResearchAnalyzerFeature
    features.emplace_back(
        server.addFeature<arangodb::iresearch::IResearchAnalyzerFeature>(),
        true);

    auto& feature =
        features
            .emplace_back(
                server.addFeature<arangodb::iresearch::IResearchFeature>(),
                true)
            .first;
    feature.collectOptions(server.options());
    feature.validateOptions(server.options());

    for (auto& f : features) {
      f.first.prepare();
    }

    for (auto& f : features) {
      if (f.second) {
        f.first.start();
      }
    }

    // register fake non-deterministic function in order to suppress
    // optimizations
    server.getFeature<arangodb::aql::AqlFunctionFeature>().add(
        arangodb::aql::Function{
            "_REFERENCE_", ".",
            arangodb::aql::Function::makeFlags(
                // fake non-deterministic
                arangodb::aql::Function::Flags::CanRunOnDBServerCluster,
                arangodb::aql::Function::Flags::CanRunOnDBServerOneShard),
            [](arangodb::aql::ExpressionContext*, arangodb::aql::AstNode const&,
               arangodb::aql::functions::VPackFunctionParametersView params) {
              TRI_ASSERT(!params.empty());
              return params[0];
            }});

    auto& dbPathFeature = server.getFeature<arangodb::DatabasePathFeature>();
    arangodb::tests::setDatabasePath(
        dbPathFeature);  // ensure test data is stored in a unique directory
  }

  ~IResearchExpressionFilterTest() {
    system.reset();  // destroy before reseting the 'ENGINE'
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
};  // TestSetup

struct FilterCtx : irs::attribute_provider {
  explicit FilterCtx(
      arangodb::iresearch::ExpressionExecutionContext& ctx) noexcept
      : _execCtx(&ctx) {}

  irs::attribute* get_mutable(irs::type_info::type_id type) noexcept override {
    return irs::type<arangodb::iresearch::ExpressionExecutionContext>::id() ==
                   type
               ? _execCtx
               : nullptr;
  }

  arangodb::iresearch::ExpressionExecutionContext*
      _execCtx;  // expression execution context
};               // FilterCtx

}  // namespace

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

TEST_F(IResearchExpressionFilterTest, test) {
  arangodb::velocypack::Builder testData;
  {
    std::filesystem::path resource;
    resource /= std::string_view(arangodb::tests::testResourceDir);
    resource /= std::string_view("simple_sequential.json");
    testData = arangodb::basics::VelocyPackHelper::velocyPackFromFile(
        resource.string());
  }
  auto testDataRoot = testData.slice();
  ASSERT_TRUE(testDataRoot.isArray());

  irs::memory_directory dir;

  // populate directory with data
  {
    struct {
      bool write(irs::data_output& out) {
        irs::write_string(out, str);
        return true;
      }

      std::string_view name() const { return "name"; }

      std::string_view str;
    } storedField;

    auto writer =
        irs::IndexWriter::Make(dir, irs::formats::get("1_0"), irs::OM_CREATE);
    ASSERT_TRUE(writer);

    for (auto data : arangodb::velocypack::ArrayIterator(testDataRoot)) {
      storedField.str = arangodb::iresearch::getStringRef(data.get("name"));

      auto ctx = writer->GetBatch();
      auto doc = ctx.Insert();
      EXPECT_TRUE(doc.Insert<irs::Action::STORE>(storedField));
      EXPECT_TRUE(doc);
    }

    writer->Commit();
  }

  // setup ArangoDB database
  TRI_vocbase_t vocbase(testDBInfo(server));

  // create view
  {
    auto createJson = arangodb::velocypack::Parser::fromJson(
        "{ \
      \"name\": \"testView\", \
      \"type\": \"arangosearch\" \
    }");

    // add view
    auto view = std::dynamic_pointer_cast<arangodb::iresearch::IResearchView>(
        vocbase.createView(createJson->slice(), false));
    ASSERT_FALSE(!view);
  }

  // open reader
  auto reader = irs::DirectoryReader(dir);
  ASSERT_TRUE(reader);
  ASSERT_EQ(1U, reader->size());
  auto& segment = (*reader)[0];
  EXPECT_TRUE(reader->docs_count() > 0);

  // uninitialized query
  {
    arangodb::iresearch::ByExpression filter;
    EXPECT_FALSE(filter);

    auto prepared = filter.prepare({.index = *reader});
    auto docs = prepared->execute({.segment = segment});
    EXPECT_EQ(irs::doc_limits::eof(), docs->value());
    EXPECT_FALSE(docs->next());
    EXPECT_EQ(irs::doc_limits::eof(), docs->value());
  }

  // query with false expression without order
  {
    std::string const queryString =
        "LET c=1 LET b=2 FOR d IN testView FILTER c==b RETURN d";

    auto query = arangodb::aql::Query::create(
        arangodb::transaction::StandaloneContext::create(
            vocbase, arangodb::transaction::OperationOriginTestCase{}),
        arangodb::aql::QueryString(queryString), nullptr);
    query->initTrxForTests();

    ExpressionContextMock ctx;
    {
      arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt{1});
      arangodb::aql::AqlValueGuard guard(value, true);
      ctx.vars.emplace("c", value);
    }
    {
      arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt{2});
      arangodb::aql::AqlValueGuard guard(value, true);
      ctx.vars.emplace("b", value);
    }

    auto const parseResult = query->parse();
    ASSERT_TRUE(parseResult.result.ok());

    auto* ast = query->ast();
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

    // find expression root
    auto* expression = filterNode->getMember(0);
    ASSERT_TRUE(expression);

    // setup filter
    std::vector<std::string> EMPTY;
    arangodb::transaction::Methods trx(
        arangodb::transaction::StandaloneContext::create(
            vocbase, arangodb::transaction::OperationOriginTestCase{}),
        EMPTY, EMPTY, EMPTY, arangodb::transaction::Options());

    arangodb::iresearch::ByExpression filter;
    EXPECT_FALSE(filter);
    filter.init({.ast = ast}, *expression);
    EXPECT_TRUE(filter);

    arangodb::iresearch::ExpressionExecutionContext execCtx;
    ctx.setTrx(&trx);
    execCtx.ctx = &ctx;
    FilterCtx queryCtx(execCtx);

    auto prepared = filter.prepare({.index = *reader, .ctx = &queryCtx});
    auto docs = prepared->execute({
        .segment = segment,
        .ctx = &queryCtx,
    });
    EXPECT_EQ(irs::doc_limits::eof(), docs->value());
    EXPECT_FALSE(docs->next());
    EXPECT_EQ(irs::doc_limits::eof(), docs->value());
  }

  // query with false expression without order (deferred execution)
  {
    std::string const queryString =
        "LET c=1 LET b=2 FOR d IN testView FILTER c==b RETURN d";

    auto query = arangodb::aql::Query::create(
        arangodb::transaction::StandaloneContext::create(
            vocbase, arangodb::transaction::OperationOriginTestCase{}),
        arangodb::aql::QueryString(queryString), nullptr);
    query->initTrxForTests();

    ExpressionContextMock ctx;
    {
      arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt{1});
      arangodb::aql::AqlValueGuard guard(value, true);
      ctx.vars.emplace("c", value);
    }
    {
      arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt{2});
      arangodb::aql::AqlValueGuard guard(value, true);
      ctx.vars.emplace("b", value);
    }

    auto const parseResult = query->parse();
    ASSERT_TRUE(parseResult.result.ok());

    auto* ast = query->ast();
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

    // find expression root
    auto* expression = filterNode->getMember(0);
    ASSERT_TRUE(expression);

    // setup filter
    std::vector<std::string> EMPTY;
    arangodb::transaction::Methods trx(
        arangodb::transaction::StandaloneContext::create(
            vocbase, arangodb::transaction::OperationOriginTestCase{}),
        EMPTY, EMPTY, EMPTY, arangodb::transaction::Options());

    arangodb::iresearch::ByExpression filter;
    EXPECT_FALSE(filter);
    filter.init({.ast = ast}, *expression);
    EXPECT_TRUE(filter);

    arangodb::iresearch::ExpressionExecutionContext execCtx;
    ctx.setTrx(&trx);
    execCtx.ctx = &ctx;
    FilterCtx queryCtx(execCtx);

    auto prepared = filter.prepare({.index = *reader});
    auto docs = prepared->execute({
        .segment = segment,
        .scorers = irs::Scorers::kUnordered,
        .ctx = &queryCtx,
        .wand = {},
    });
    EXPECT_EQ(irs::doc_limits::eof(), docs->value());
    EXPECT_FALSE(docs->next());
    EXPECT_EQ(irs::doc_limits::eof(), docs->value());
  }

  // query with true expression without order
  {
    std::string const queryString =
        "LET c=1 LET b=2 FOR d IN testView FILTER c<b RETURN d";

    auto query = arangodb::aql::Query::create(
        arangodb::transaction::StandaloneContext::create(
            vocbase, arangodb::transaction::OperationOriginTestCase{}),
        arangodb::aql::QueryString(queryString), nullptr);
    query->initTrxForTests();

    ExpressionContextMock ctx;
    {
      arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt{1});
      arangodb::aql::AqlValueGuard guard(value, true);
      ctx.vars.emplace("c", value);
    }
    {
      arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt{2});
      arangodb::aql::AqlValueGuard guard(value, true);
      ctx.vars.emplace("b", value);
    }

    auto const parseResult = query->parse();
    ASSERT_TRUE(parseResult.result.ok());

    auto* ast = query->ast();
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

    // find expression root
    auto* expression = filterNode->getMember(0);
    ASSERT_TRUE(expression);

    // setup filter
    std::vector<std::string> EMPTY;
    arangodb::transaction::Methods trx(
        arangodb::transaction::StandaloneContext::create(
            vocbase, arangodb::transaction::OperationOriginTestCase{}),
        EMPTY, EMPTY, EMPTY, arangodb::transaction::Options());

    arangodb::iresearch::ByExpression filter;
    EXPECT_FALSE(filter);
    filter.init({.ast = ast}, *expression);
    EXPECT_TRUE(filter);

    arangodb::iresearch::ExpressionExecutionContext execCtx;
    ctx.setTrx(&trx);
    execCtx.ctx = &ctx;
    FilterCtx queryCtx(execCtx);

    auto prepared = filter.prepare({.index = *reader, .ctx = &queryCtx});
    EXPECT_EQ(irs::kNoBoost, prepared->boost());  // no boost set
    EXPECT_EQ(typeid(prepared.get()),
              typeid(irs::all().prepare({.index = *reader}).get()));
    auto column = segment.column("name");
    ASSERT_TRUE(column);
    auto columnValues = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, columnValues);
    auto* value = irs::get<irs::payload>(*columnValues);
    ASSERT_NE(nullptr, value);
    auto docs = prepared->execute({
        .segment = segment,
        .scorers = irs::Scorers::kUnordered,
        .ctx = &queryCtx,
        .wand = {},
    });
    EXPECT_EQ(irs::doc_limits::invalid(), docs->value());
    auto* cost = irs::get<irs::cost>(*docs);
    ASSERT_TRUE(cost);
    EXPECT_EQ(arangodb::velocypack::ArrayIterator(testDataRoot).size(),
              cost->estimate());

    for (auto doc : arangodb::velocypack::ArrayIterator(testDataRoot)) {
      EXPECT_TRUE(docs->next());
      EXPECT_EQ(docs->value(), columnValues->seek(docs->value()));
      EXPECT_TRUE(arangodb::iresearch::getStringRef(doc.get("name")) ==
                  irs::to_string<std::string_view>(value->value.data()));
    }
    EXPECT_FALSE(docs->next());
    EXPECT_EQ(irs::doc_limits::eof(), docs->value());
  }

  // query with true expression without order (deferred execution)
  {
    std::string const queryString =
        "LET c=1 LET b=2 FOR d IN testView FILTER c<b RETURN d";

    auto query = arangodb::aql::Query::create(
        arangodb::transaction::StandaloneContext::create(
            vocbase, arangodb::transaction::OperationOriginTestCase{}),
        arangodb::aql::QueryString(queryString), nullptr);
    query->initTrxForTests();

    ExpressionContextMock ctx;
    {
      arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt{1});
      arangodb::aql::AqlValueGuard guard(value, true);
      ctx.vars.emplace("c", value);
    }
    {
      arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt{2});
      arangodb::aql::AqlValueGuard guard(value, true);
      ctx.vars.emplace("b", value);
    }

    auto const parseResult = query->parse();
    ASSERT_TRUE(parseResult.result.ok());

    auto* ast = query->ast();
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

    // find expression root
    auto* expression = filterNode->getMember(0);
    ASSERT_TRUE(expression);

    // setup filter
    std::vector<std::string> EMPTY;
    arangodb::transaction::Methods trx(
        arangodb::transaction::StandaloneContext::create(
            vocbase, arangodb::transaction::OperationOriginTestCase{}),
        EMPTY, EMPTY, EMPTY, arangodb::transaction::Options());

    arangodb::iresearch::ByExpression filter;
    EXPECT_FALSE(filter);
    filter.init({.ast = ast}, *expression);
    EXPECT_TRUE(filter);

    arangodb::iresearch::ExpressionExecutionContext execCtx;
    ctx.setTrx(&trx);
    execCtx.ctx = &ctx;
    FilterCtx queryCtx(execCtx);

    auto prepared = filter.prepare({.index = *reader});  // no context provided
    EXPECT_EQ(irs::kNoBoost, prepared->boost());         // no boost set
    EXPECT_EQ(typeid(prepared.get()),
              typeid(irs::all().prepare({.index = *reader}).get()));
    auto column = segment.column("name");
    ASSERT_TRUE(column);
    auto columnValues = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, columnValues);
    auto* value = irs::get<irs::payload>(*columnValues);
    ASSERT_NE(nullptr, value);
    auto docs = prepared->execute({
        .segment = segment,
        .scorers = irs::Scorers::kUnordered,
        .ctx = &queryCtx,
        .wand = {},
    });
    EXPECT_EQ(irs::doc_limits::invalid(), docs->value());
    auto* cost = irs::get<irs::cost>(*docs);
    ASSERT_TRUE(cost);
    EXPECT_EQ(arangodb::velocypack::ArrayIterator(testDataRoot).size(),
              cost->estimate());

    for (auto doc : arangodb::velocypack::ArrayIterator(testDataRoot)) {
      EXPECT_TRUE(docs->next());
      EXPECT_EQ(docs->value(), columnValues->seek(docs->value()));
      EXPECT_TRUE(arangodb::iresearch::getStringRef(doc.get("name")) ==
                  irs::to_string<std::string_view>(value->value.data()));
    }
    EXPECT_FALSE(docs->next());
    EXPECT_EQ(irs::doc_limits::eof(), docs->value());
  }

  // query with true expression without order (deferred execution)
  {
    std::string const queryString =
        "LET c=1 LET b=2 FOR d IN testView FILTER c<b RETURN d";

    auto query = arangodb::aql::Query::create(
        arangodb::transaction::StandaloneContext::create(
            vocbase, arangodb::transaction::OperationOriginTestCase{}),
        arangodb::aql::QueryString(queryString), nullptr);
    query->initTrxForTests();

    ExpressionContextMock ctx;
    {
      arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt{1});
      arangodb::aql::AqlValueGuard guard(value, true);
      ctx.vars.emplace("c", value);
    }
    {
      arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt{2});
      arangodb::aql::AqlValueGuard guard(value, true);
      ctx.vars.emplace("b", value);
    }

    auto const parseResult = query->parse();
    ASSERT_TRUE(parseResult.result.ok());

    auto* ast = query->ast();
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

    // find expression root
    auto* expression = filterNode->getMember(0);
    ASSERT_TRUE(expression);

    // setup filter
    std::vector<std::string> EMPTY;
    arangodb::transaction::Methods trx(
        arangodb::transaction::StandaloneContext::create(
            vocbase, arangodb::transaction::OperationOriginTestCase{}),
        EMPTY, EMPTY, EMPTY, arangodb::transaction::Options());

    arangodb::iresearch::ByExpression filter;
    EXPECT_FALSE(filter);
    filter.init({.ast = ast}, *expression);
    EXPECT_TRUE(filter);

    arangodb::iresearch::ExpressionExecutionContext execCtx;
    ctx.setTrx(&trx);
    execCtx.ctx = nullptr;
    FilterCtx queryCtx(execCtx);

    auto prepared = filter.prepare(
        {.index = *reader, .ctx = &queryCtx});    // invalid context provided
    EXPECT_EQ(irs::kNoBoost, prepared->boost());  // no boost set
    auto column = segment.column("name");
    ASSERT_TRUE(column);
    auto columnValues = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, columnValues);
    auto* value = irs::get<irs::payload>(*columnValues);
    ASSERT_NE(nullptr, value);
    execCtx.ctx = &ctx;  // fix context
    auto docs = prepared->execute({
        .segment = segment,
        .scorers = irs::Scorers::kUnordered,
        .ctx = &queryCtx,
        .wand = {},
    });
    EXPECT_EQ(irs::doc_limits::invalid(), docs->value());
    auto* cost = irs::get<irs::cost>(*docs);
    ASSERT_TRUE(cost);
    EXPECT_EQ(arangodb::velocypack::ArrayIterator(testDataRoot).size(),
              cost->estimate());

    for (auto doc : arangodb::velocypack::ArrayIterator(testDataRoot)) {
      EXPECT_TRUE(docs->next());
      EXPECT_EQ(docs->value(), columnValues->seek(docs->value()));
      EXPECT_TRUE(arangodb::iresearch::getStringRef(doc.get("name")) ==
                  irs::to_string<std::string_view>(value->value.data()));
    }
    EXPECT_FALSE(docs->next());
    EXPECT_EQ(irs::doc_limits::eof(), docs->value());
  }

  // query with true expression without order (deferred execution with invalid
  // context)
  {
    std::string const queryString =
        "LET c=1 LET b=2 FOR d IN testView FILTER c<b RETURN d";

    auto query = arangodb::aql::Query::create(
        arangodb::transaction::StandaloneContext::create(
            vocbase, arangodb::transaction::OperationOriginTestCase{}),
        arangodb::aql::QueryString(queryString), nullptr);
    query->initTrxForTests();

    ExpressionContextMock ctx;
    {
      arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt{1});
      arangodb::aql::AqlValueGuard guard(value, true);
      ctx.vars.emplace("c", value);
    }
    {
      arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt{2});
      arangodb::aql::AqlValueGuard guard(value, true);
      ctx.vars.emplace("b", value);
    }

    auto const parseResult = query->parse();
    ASSERT_TRUE(parseResult.result.ok());

    auto* ast = query->ast();
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

    // find expression root
    auto* expression = filterNode->getMember(0);
    ASSERT_TRUE(expression);

    // setup filter
    std::vector<std::string> EMPTY;
    arangodb::transaction::Methods trx(
        arangodb::transaction::StandaloneContext::create(
            vocbase, arangodb::transaction::OperationOriginTestCase{}),
        EMPTY, EMPTY, EMPTY, arangodb::transaction::Options());

    arangodb::iresearch::ByExpression filter;
    EXPECT_FALSE(filter);
    filter.init({.ast = ast}, *expression);
    EXPECT_TRUE(filter);

    arangodb::iresearch::ExpressionExecutionContext execCtx;
    ctx.setTrx(&trx);
    execCtx.ctx = nullptr;
    FilterCtx queryCtx(execCtx);

    auto prepared = filter.prepare({.index = *reader});  // no context provided
    EXPECT_EQ(irs::kNoBoost, prepared->boost());         // no boost set
    auto docs = prepared->execute({
        .segment = segment,
        .scorers = irs::Scorers::kUnordered,
        .ctx = &queryCtx,
        .wand = {},
    });
    EXPECT_TRUE(irs::doc_limits::eof(docs->value()));
    EXPECT_FALSE(docs->next());
  }

  // query with true expression without order (deferred execution with invalid
  // context)
  {
    std::string const queryString =
        "LET c=1 LET b=2 FOR d IN testView FILTER c<b RETURN d";

    auto query = arangodb::aql::Query::create(
        arangodb::transaction::StandaloneContext::create(
            vocbase, arangodb::transaction::OperationOriginTestCase{}),
        arangodb::aql::QueryString(queryString), nullptr);
    query->initTrxForTests();

    ExpressionContextMock ctx;
    {
      arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt{1});
      arangodb::aql::AqlValueGuard guard(value, true);
      ctx.vars.emplace("c", value);
    }
    {
      arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt{2});
      arangodb::aql::AqlValueGuard guard(value, true);
      ctx.vars.emplace("b", value);
    }

    auto const parseResult = query->parse();
    ASSERT_TRUE(parseResult.result.ok());

    auto* ast = query->ast();
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

    // find expression root
    auto* expression = filterNode->getMember(0);
    ASSERT_TRUE(expression);

    // setup filter
    std::vector<std::string> EMPTY;
    arangodb::transaction::Methods trx(
        arangodb::transaction::StandaloneContext::create(
            vocbase, arangodb::transaction::OperationOriginTestCase{}),
        EMPTY, EMPTY, EMPTY, arangodb::transaction::Options());

    arangodb::iresearch::ByExpression filter;
    EXPECT_FALSE(filter);
    filter.init({.ast = ast}, *expression);
    EXPECT_TRUE(filter);

    arangodb::iresearch::ExpressionExecutionContext execCtx;
    ctx.setTrx(&trx);
    execCtx.ctx = nullptr;
    FilterCtx queryCtx(execCtx);

    auto prepared = filter.prepare({.index = *reader});  // no context provided
    EXPECT_EQ(irs::kNoBoost, prepared->boost());         // no boost set
    auto docs = prepared->execute({
        .segment = segment,
        .scorers = irs::Scorers::kUnordered,
        .ctx = &queryCtx,
        .wand = {},
    });
    EXPECT_TRUE(irs::doc_limits::eof(docs->value()));
    EXPECT_FALSE(docs->next());
  }

  // query with nondeterministic expression without order
  {
    std::string const queryString =
        "LET c=1 LET b=2 FOR d IN testView FILTER "
        "_REFERENCE_(c)==_REFERENCE_(b) RETURN d";

    auto query = arangodb::aql::Query::create(
        arangodb::transaction::StandaloneContext::create(
            vocbase, arangodb::transaction::OperationOriginTestCase{}),
        arangodb::aql::QueryString(queryString), nullptr);
    query->initTrxForTests();

    ExpressionContextMock ctx;
    {
      arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt{2});
      arangodb::aql::AqlValueGuard guard(value, true);
      ctx.vars.emplace("b", value);
    }

    auto const parseResult = query->parse();
    ASSERT_TRUE(parseResult.result.ok());

    auto* ast = query->ast();
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

    // find expression root
    auto* expression = filterNode->getMember(0);
    ASSERT_TRUE(expression);

    // setup filter
    std::vector<std::string> EMPTY;
    arangodb::transaction::Methods trx(
        arangodb::transaction::StandaloneContext::create(
            vocbase, arangodb::transaction::OperationOriginTestCase{}),
        EMPTY, EMPTY, EMPTY, arangodb::transaction::Options());

    arangodb::iresearch::ByExpression filter;
    EXPECT_FALSE(filter);
    filter.init({.ast = ast}, *expression);
    EXPECT_TRUE(filter);

    arangodb::iresearch::ExpressionExecutionContext execCtx;
    ctx.setTrx(&trx);
    execCtx.ctx = &ctx;
    FilterCtx queryCtx(execCtx);

    auto prepared = filter.prepare({.index = *reader, .ctx = &queryCtx});
    auto column = segment.column("name");
    ASSERT_TRUE(column);
    auto columnValues = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, columnValues);
    auto* value = irs::get<irs::payload>(*columnValues);
    ASSERT_NE(nullptr, value);
    auto docs = prepared->execute({
        .segment = segment,
        .scorers = irs::Scorers::kUnordered,
        .ctx = &queryCtx,
        .wand = {},
    });
    EXPECT_EQ(irs::doc_limits::invalid(), docs->value());
    auto* score = irs::get<irs::score>(*docs);
    EXPECT_TRUE(score);
    EXPECT_TRUE(score->IsDefault());

    // set reachable filter condition
    {
      ctx.vars.erase("c");
      arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt{2});
      arangodb::aql::AqlValueGuard guard(value, true);
      ctx.vars.emplace("c", value);
    }

    auto it = arangodb::velocypack::ArrayIterator(testDataRoot);
    for (size_t i = 0; i < it.size() / 2; ++i) {
      ASSERT_TRUE(it.valid());
      auto doc = *it;
      EXPECT_TRUE(docs->next());
      EXPECT_EQ(docs->value(), columnValues->seek(docs->value()));
      EXPECT_TRUE(arangodb::iresearch::getStringRef(doc.get("name")) ==
                  irs::to_string<std::string_view>(value->value.data()));
      it.next();
    }

    EXPECT_TRUE(it.valid());

    // set unreachable filter condition
    {
      ctx.vars.erase("c");
      arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt{1});
      arangodb::aql::AqlValueGuard guard(value, true);
      ctx.vars.emplace("c", value);
    }
    EXPECT_FALSE(docs->next());
    EXPECT_EQ(irs::doc_limits::eof(), docs->value());
  }

  // query with nondeterministic expression and custom order
  {
    std::array<irs::Scorer::ptr, 1> order{std::make_unique<custom_sort>()};

    size_t collector_finish_count = 0;
    size_t field_collector_collect_count = 0;
    size_t term_collector_collect_count = 0;
    size_t scorer_score_count = 0;
    auto& sort = static_cast<custom_sort&>(*order.front());

    sort.fieldCollectorCollect = [&field_collector_collect_count](
                                     const irs::SubReader&,
                                     const irs::term_reader&) -> void {
      ++field_collector_collect_count;
    };
    sort.collectorFinish = [&collector_finish_count](
                               irs::byte_type*, irs::FieldCollector const*,
                               irs::TermCollector const*) -> void {
      ++collector_finish_count;
    };
    sort.termCollectorCollect = [&term_collector_collect_count](
                                    const irs::SubReader&,
                                    const irs::term_reader&,
                                    const irs::attribute_provider&) -> void {
      ++term_collector_collect_count;
    };
    sort.scorerScore = [&scorer_score_count](irs::doc_id_t doc,
                                             irs::score_t* res) -> void {
      ASSERT_NE(nullptr, res);
      ++scorer_score_count;
      *res = static_cast<irs::score_t>(doc);
    };
    auto preparedOrder = irs::Scorers::Prepare(order);

    std::string const queryString =
        "LET c=1 LET b=2 FOR d IN testView FILTER "
        "_REFERENCE_(c)==_REFERENCE_(b) RETURN d";

    auto query = arangodb::aql::Query::create(
        arangodb::transaction::StandaloneContext::create(
            vocbase, arangodb::transaction::OperationOriginTestCase{}),
        arangodb::aql::QueryString(queryString), nullptr);
    query->initTrxForTests();

    ExpressionContextMock ctx;
    {
      arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt{2});
      arangodb::aql::AqlValueGuard guard(value, true);
      ctx.vars.emplace("b", value);
    }

    auto const parseResult = query->parse();
    ASSERT_TRUE(parseResult.result.ok());

    auto* ast = query->ast();
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

    // find expression root
    auto* expression = filterNode->getMember(0);
    ASSERT_TRUE(expression);

    // setup filter
    std::vector<std::string> EMPTY;
    arangodb::transaction::Methods trx(
        arangodb::transaction::StandaloneContext::create(
            vocbase, arangodb::transaction::OperationOriginTestCase{}),
        EMPTY, EMPTY, EMPTY, arangodb::transaction::Options());

    arangodb::iresearch::ByExpression filter;
    EXPECT_FALSE(filter);
    filter.init({.ast = ast}, *expression);
    EXPECT_TRUE(filter);

    arangodb::iresearch::ExpressionExecutionContext execCtx;
    ctx.setTrx(&trx);
    execCtx.ctx = &ctx;
    FilterCtx queryCtx(execCtx);

    filter.boost(1.5f);
    EXPECT_EQ(1.5f, filter.boost());

    auto prepared = filter.prepare({
        .index = *reader,
        .scorers = preparedOrder,
        .ctx = &queryCtx,
    });
    EXPECT_EQ(1.5f, prepared->boost());

    auto column = segment.column("name");
    ASSERT_TRUE(column);
    auto columnValues = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, columnValues);
    auto* value = irs::get<irs::payload>(*columnValues);
    ASSERT_NE(nullptr, value);
    auto docs = prepared->execute({
        .segment = segment,
        .scorers = preparedOrder,
        .ctx = &queryCtx,
        .wand = {},
    });
    EXPECT_EQ(irs::doc_limits::invalid(), docs->value());
    auto* score = irs::get<irs::score>(*docs);
    EXPECT_TRUE(score);
    EXPECT_FALSE(score->IsDefault());
    auto* cost = irs::get<irs::cost>(*docs);
    ASSERT_TRUE(cost);
    EXPECT_EQ(arangodb::velocypack::ArrayIterator(testDataRoot).size(),
              cost->estimate());

    // set reachable filter condition
    {
      ctx.vars.erase("c");
      arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt{2});
      arangodb::aql::AqlValueGuard guard(value, true);
      ctx.vars.emplace("c", value);
    }

    auto it = arangodb::velocypack::ArrayIterator(testDataRoot);
    for (size_t i = 0; i < it.size() / 2; ++i) {
      ASSERT_TRUE(it.valid());
      auto doc = *it;
      EXPECT_TRUE(docs->next());
      [[maybe_unused]] irs::score_t scoreValue;
      (*score)(&scoreValue);
      EXPECT_EQ(docs->value(), columnValues->seek(docs->value()));
      EXPECT_TRUE(arangodb::iresearch::getStringRef(doc.get("name")) ==
                  irs::to_string<std::string_view>(value->value.data()));
      it.next();
    }

    EXPECT_TRUE(it.valid());

    // set unreachable filter condition
    {
      ctx.vars.erase("c");
      arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt{1});
      arangodb::aql::AqlValueGuard guard(value, true);
      ctx.vars.emplace("c", value);
    }
    EXPECT_FALSE(docs->next());
    EXPECT_EQ(irs::doc_limits::eof(), docs->value());

    // check order
    EXPECT_EQ(0, field_collector_collect_count);  // should not be executed
    EXPECT_EQ(0, term_collector_collect_count);   // should not be executed
    EXPECT_EQ(1, collector_finish_count);
    EXPECT_EQ(it.size() / 2, scorer_score_count);
  }

  // query with nondeterministic expression without order, seek + next
  {
    std::shared_ptr<arangodb::velocypack::Builder> bindVars;
    std::string const queryString =
        "LET c=1 LET b=2 FOR d IN testView FILTER "
        "_REFERENCE_(c)==_REFERENCE_(b) RETURN d";

    auto query = arangodb::aql::Query::create(
        arangodb::transaction::StandaloneContext::create(
            vocbase, arangodb::transaction::OperationOriginTestCase{}),
        arangodb::aql::QueryString(queryString), bindVars);
    query->initTrxForTests();

    ExpressionContextMock ctx;
    {
      arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt{2});
      arangodb::aql::AqlValueGuard guard(value, true);
      ctx.vars.emplace("b", value);
    }

    auto const parseResult = query->parse();
    ASSERT_TRUE(parseResult.result.ok());

    auto* ast = query->ast();
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

    // find expression root
    auto* expression = filterNode->getMember(0);
    ASSERT_TRUE(expression);

    // setup filter
    std::vector<std::string> EMPTY;
    arangodb::transaction::Methods trx(
        arangodb::transaction::StandaloneContext::create(
            vocbase, arangodb::transaction::OperationOriginTestCase{}),
        EMPTY, EMPTY, EMPTY, arangodb::transaction::Options());

    arangodb::iresearch::ByExpression filter;
    EXPECT_FALSE(filter);
    filter.init({.ast = ast}, *expression);
    EXPECT_TRUE(filter);

    arangodb::iresearch::ExpressionExecutionContext execCtx;
    ctx.setTrx(&trx);
    execCtx.ctx = &ctx;
    FilterCtx queryCtx(execCtx);

    auto prepared = filter.prepare({.index = *reader, .ctx = &queryCtx});
    auto column = segment.column("name");
    ASSERT_TRUE(column);
    auto columnValues = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, columnValues);
    auto* value = irs::get<irs::payload>(*columnValues);
    ASSERT_NE(nullptr, value);
    auto docs = prepared->execute({
        .segment = segment,
        .scorers = irs::Scorers::kUnordered,
        .ctx = &queryCtx,
        .wand = {},
    });
    EXPECT_EQ(irs::doc_limits::invalid(), docs->value());
    auto* score = irs::get<irs::score>(*docs);
    EXPECT_TRUE(score);
    EXPECT_TRUE(score->IsDefault());
    auto* cost = irs::get<irs::cost>(*docs);
    ASSERT_TRUE(cost);
    EXPECT_EQ(arangodb::velocypack::ArrayIterator(testDataRoot).size(),
              cost->estimate());

    // set reachable filter condition
    {
      ctx.vars.erase("c");
      arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt{2});
      arangodb::aql::AqlValueGuard guard(value, true);
      ctx.vars.emplace("c", value);
    }

    auto it = arangodb::velocypack::ArrayIterator(testDataRoot);

    size_t const seek_to = 7;
    for (size_t i = 0; i < seek_to; ++i) {
      it.next();
      ASSERT_TRUE(it.valid());
    }
    EXPECT_EQ(seek_to, docs->seek(seek_to));

    for (size_t i = seek_to; i < it.size() / 2; ++i) {
      ASSERT_TRUE(it.valid());
      auto doc = *it;
      EXPECT_TRUE(docs->next());
      EXPECT_EQ(docs->value(), columnValues->seek(docs->value()));
      EXPECT_TRUE(arangodb::iresearch::getStringRef(doc.get("name")) ==
                  irs::to_string<std::string_view>(value->value.data()));
      it.next();
    }

    EXPECT_TRUE(it.valid());

    // set unreachable filter condition
    {
      ctx.vars.erase("c");
      arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt{1});
      arangodb::aql::AqlValueGuard guard(value, true);
      ctx.vars.emplace("c", value);
    }
    EXPECT_FALSE(docs->next());
    EXPECT_EQ(irs::doc_limits::eof(), docs->value());
  }
}
