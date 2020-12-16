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

#include "3rdParty/iresearch/tests/tests_config.hpp"
#include "analysis/token_attributes.hpp"
#include "index/directory_reader.hpp"
#include "search/all_filter.hpp"
#include "search/cost.hpp"
#include "search/score.hpp"
#include "search/scorers.hpp"
#include "store/memory_directory.hpp"
#include "store/store_utils.hpp"
#include "utils/type_limits.hpp"
#include "utils/utf8_path.hpp"

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
#include "IResearch/IResearchFilterFactory.h"
#include "IResearch/IResearchView.h"
#include "IResearch/VelocyPackHelper.h"
#include "Logger/LogTopic.h"
#include "Logger/Logger.h"
#include "RestServer/AqlFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/DatabasePathFeature.h"
#include "RestServer/MetricsFeature.h"
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
#include "VocBase/ManagedDocumentResult.h"

#if USE_ENTERPRISE
#include "Enterprise/Ldap/LdapFeature.h"
#endif

extern const char* ARGV0;  // defined in main.cpp

namespace {

struct custom_sort : public irs::sort {
  static constexpr irs::string_ref type_name() noexcept {
    return "custom_sort";
  }

  class prepared : public irs::prepared_sort_base<irs::doc_id_t, void> {
   public:
    class field_collector : public irs::sort::field_collector {
     public:
      field_collector(const custom_sort& sort) : sort_(sort) {}

      virtual void collect(const irs::sub_reader& segment,
                           const irs::term_reader& field) override {
        if (sort_.field_collector_collect) {
          sort_.field_collector_collect(segment, field);
        }
      }

      virtual void collect(const irs::bytes_ref& in) override {}

      virtual void reset() override { }

      virtual void write(irs::data_output& out) const override {}

     private:
      const custom_sort& sort_;
    };

    class term_collector : public irs::sort::term_collector {
     public:
      term_collector(const custom_sort& sort) : sort_(sort) {}

      virtual void collect(const irs::sub_reader& segment, const irs::term_reader& field,
                           const irs::attribute_provider& term_attrs) override {
        if (sort_.term_collector_collect) {
          sort_.term_collector_collect(segment, field, term_attrs);
        }
      }

      virtual void collect(const irs::bytes_ref& in) override {}

      virtual void reset() override { }

      virtual void write(irs::data_output& out) const override {}

     private:
      const custom_sort& sort_;
    };

    struct scorer : public irs::score_ctx {
      scorer(const custom_sort& sort, const irs::sub_reader& segment_reader,
             const irs::term_reader& term_reader, const irs::byte_type* stats,
             irs::byte_type* score_buf, const irs::attribute_provider& document_attrs)
          : document_attrs_(document_attrs),
            stats_(stats),
            score_buf_(score_buf),
            segment_reader_(segment_reader),
            sort_(sort),
            term_reader_(term_reader) {}

      const irs::attribute_provider& document_attrs_;
      const irs::byte_type* stats_;
      irs::byte_type* score_buf_;
      const irs::sub_reader& segment_reader_;
      const custom_sort& sort_;
      const irs::term_reader& term_reader_;
    };

    DECLARE_FACTORY(prepared);

    prepared(const custom_sort& sort) : sort_(sort) {}

    virtual void collect(irs::byte_type* filter_attrs, const irs::index_reader& index,
                         const irs::sort::field_collector* field,
                         const irs::sort::term_collector* term) const override {
      if (sort_.collector_finish) {
        sort_.collector_finish(filter_attrs, index);
      }
    }

    virtual const iresearch::flags& features() const override {
      return iresearch::flags::empty_instance();
    }

    virtual irs::sort::field_collector::ptr prepare_field_collector() const override {
      if (sort_.prepare_field_collector) {
        return sort_.prepare_field_collector();
      }

      return irs::memory::make_unique<custom_sort::prepared::field_collector>(sort_);
    }

    virtual irs::score_function prepare_scorer(
        irs::sub_reader const& segment_reader, irs::term_reader const& term_reader,
        irs::byte_type const* filter_node_attrs, irs::byte_type* score_buf,
        irs::attribute_provider const& document_attrs, irs::boost_t boost) const override {
      if (sort_.prepare_scorer) {
        return sort_.prepare_scorer(segment_reader, term_reader,
                                    filter_node_attrs, score_buf, document_attrs, boost);
      }

      return {
        std::make_unique<custom_sort::prepared::scorer>(sort_, segment_reader, term_reader,
                                                        filter_node_attrs, score_buf, document_attrs),
              [](irs::score_ctx* ctx) -> const irs::byte_type* {
                auto& ctxImpl =
                    *reinterpret_cast<const custom_sort::prepared::scorer*>(ctx);

                EXPECT_TRUE(ctxImpl.score_buf_);
                auto& doc_id = *reinterpret_cast<irs::doc_id_t*>(ctxImpl.score_buf_);

                doc_id = irs::get<irs::document>(ctxImpl.document_attrs_)->value;

                if (ctxImpl.sort_.scorer_score) {
                  ctxImpl.sort_.scorer_score(doc_id);
                }

                return ctxImpl.score_buf_;
              }};
    }

    virtual irs::sort::term_collector::ptr prepare_term_collector() const override {
      if (sort_.prepare_term_collector) {
        return sort_.prepare_term_collector();
      }

      return irs::memory::make_unique<custom_sort::prepared::term_collector>(sort_);
    }

    virtual bool less(irs::byte_type const* lhs, const irs::byte_type* rhs) const override {
      return sort_.scorer_less ? sort_.scorer_less(traits_t::score_cast(lhs), traits_t::score_cast(rhs)) : false;
    }

   private:
    const custom_sort& sort_;
  };

  std::function<void(const irs::sub_reader&, const irs::term_reader&)> field_collector_collect;
  std::function<void(const irs::sub_reader&, const irs::term_reader&, const irs::attribute_provider&)> term_collector_collect;
  std::function<void(irs::byte_type*, const irs::index_reader&)> collector_finish;
  std::function<irs::sort::field_collector::ptr()> prepare_field_collector;
  std::function<irs::score_function(
      const irs::sub_reader&, const irs::term_reader&,
      const irs::byte_type*, irs::byte_type*,
      const irs::attribute_provider&, irs::boost_t)> prepare_scorer;
  std::function<irs::sort::term_collector::ptr()> prepare_term_collector;
  std::function<void(irs::doc_id_t&, const irs::doc_id_t&)> scorer_add;
  std::function<bool(const irs::doc_id_t&, const irs::doc_id_t&)> scorer_less;
  std::function<void(irs::doc_id_t&)> scorer_score;

  DECLARE_FACTORY();
  custom_sort() : sort(irs::type<custom_sort>::get()) {}
  virtual prepared::ptr prepare() const override {
    return std::make_unique<custom_sort::prepared>(*this);
  }
};

DEFINE_FACTORY_DEFAULT(custom_sort)

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

struct IResearchExpressionFilterTest
    : public ::testing::Test,
      public arangodb::tests::LogSuppressor<arangodb::Logger::AUTHENTICATION, arangodb::LogLevel::ERR>,
      public arangodb::tests::LogSuppressor<arangodb::iresearch::TOPIC, arangodb::LogLevel::FATAL>,
      public arangodb::tests::IResearchLogSuppressor {
  arangodb::application_features::ApplicationServer server;
  StorageEngineMock engine;
  std::unique_ptr<TRI_vocbase_t> system;
  std::vector<std::pair<arangodb::application_features::ApplicationFeature&, bool>> features;

  IResearchExpressionFilterTest()
    : server(std::make_shared<arangodb::options::ProgramOptions>("", "", "", ""), nullptr),
      engine(server) {
    arangodb::tests::init(true);

    // setup required application features
    features.emplace_back(server.addFeature<arangodb::ViewTypesFeature>(), true);
    features.emplace_back(server.addFeature<arangodb::AuthenticationFeature>(), true);
    features.emplace_back(server.addFeature<arangodb::DatabasePathFeature>(), false);
    features.emplace_back(server.addFeature<arangodb::DatabaseFeature>(), false);
    features.emplace_back(server.addFeature<arangodb::EngineSelectorFeature>(), false);
    server.getFeature<arangodb::EngineSelectorFeature>().setEngineTesting(&engine);
    features.emplace_back(server.addFeature<arangodb::MetricsFeature>(), false);
    features.emplace_back(server.addFeature<arangodb::QueryRegistryFeature>(), false);  // must be first
    system = irs::memory::make_unique<TRI_vocbase_t>(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL,
                                                     systemDBInfo(server));
    features.emplace_back(server.addFeature<arangodb::SystemDatabaseFeature>(
                              system.get()),
                          false);  // required for IResearchAnalyzerFeature
    features.emplace_back(server.addFeature<arangodb::AqlFeature>(), true);
    features.emplace_back(server.addFeature<arangodb::ShardingFeature>(), false);
    features.emplace_back(server.addFeature<arangodb::aql::OptimizerRulesFeature>(), true);
    features.emplace_back(server.addFeature<arangodb::aql::AqlFunctionFeature>(),
                          true);  // required for IResearchAnalyzerFeature
    features.emplace_back(server.addFeature<arangodb::iresearch::IResearchAnalyzerFeature>(),
                          true);


    auto& feature = features.emplace_back(server.addFeature<arangodb::iresearch::IResearchFeature>(), true).first;
    feature.collectOptions(server.options());
    feature.validateOptions(server.options());

#if USE_ENTERPRISE
    features.emplace_back(server.addFeature<arangodb::LdapFeature>(), false);  // required for AuthenticationFeature with USE_ENTERPRISE
#endif

    for (auto& f : features) {
      f.first.prepare();
    }

    for (auto& f : features) {
      if (f.second) {
        f.first.start();
      }
    }

    // register fake non-deterministic function in order to suppress optimizations
    server.getFeature<arangodb::aql::AqlFunctionFeature>().add(arangodb::aql::Function{
        "_REFERENCE_", ".",
        arangodb::aql::Function::makeFlags(
            // fake non-deterministic
            arangodb::aql::Function::Flags::CanRunOnDBServerCluster,
            arangodb::aql::Function::Flags::CanRunOnDBServerOneShard),
        [](arangodb::aql::ExpressionContext*, arangodb::transaction::Methods*,
           arangodb::aql::VPackFunctionParameters const& params) {
          TRI_ASSERT(!params.empty());
          return params[0];
        }});

    auto& dbPathFeature = server.getFeature<arangodb::DatabasePathFeature>();
    arangodb::tests::setDatabasePath(dbPathFeature);  // ensure test data is stored in a unique directory
  }

  ~IResearchExpressionFilterTest() {
    system.reset();  // destroy before reseting the 'ENGINE'
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
};  // TestSetup

struct FilterCtx : irs::attribute_provider {
  explicit FilterCtx(arangodb::iresearch::ExpressionExecutionContext& ctx) noexcept
    : _execCtx(&ctx) {
  }

  irs::attribute* get_mutable(irs::type_info::type_id type) noexcept override {
    return irs::type<arangodb::iresearch::ExpressionExecutionContext>::id() == type
      ? _execCtx
      : nullptr;
  }

  arangodb::iresearch::ExpressionExecutionContext* _execCtx;  // expression execution context
}; // FilterCtx

}  // namespace

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

TEST_F(IResearchExpressionFilterTest, test) {
  arangodb::velocypack::Builder testData;
  {
    irs::utf8_path resource;
    resource /= irs::string_ref(arangodb::tests::testResourceDir);
    resource /= irs::string_ref("simple_sequential.json");
    testData = arangodb::basics::VelocyPackHelper::velocyPackFromFile(resource.utf8());
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

      irs::string_ref name() const { return "name"; }

      irs::string_ref str;
    } storedField;

    auto writer = irs::index_writer::make(dir, irs::formats::get("1_0"), irs::OM_CREATE);
    ASSERT_TRUE(writer);

    for (auto data : arangodb::velocypack::ArrayIterator(testDataRoot)) {
      storedField.str = arangodb::iresearch::getStringRef(data.get("name"));

      auto ctx = writer->documents();
      auto doc = ctx.insert();
      EXPECT_TRUE(doc.insert<irs::Action::STORE>(storedField));
      EXPECT_TRUE(doc);
    }

    writer->commit();
  }

  // setup ArangoDB database
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server));

  // create view
  {
    auto createJson = arangodb::velocypack::Parser::fromJson(
        "{ \
      \"name\": \"testView\", \
      \"type\": \"arangosearch\" \
    }");

    // add view
    auto view = std::dynamic_pointer_cast<arangodb::iresearch::IResearchView>(
        vocbase.createView(createJson->slice()));
    ASSERT_FALSE(!view);
  }

  // open reader
  auto reader = irs::directory_reader::open(dir);
  ASSERT_TRUE(reader);
  ASSERT_EQ(1, reader->size());
  auto& segment = (*reader)[0];
  EXPECT_TRUE(reader->docs_count() > 0);

  // uninitialized query
  {
    arangodb::iresearch::ByExpression filter;
    EXPECT_FALSE(filter);

    auto prepared = filter.prepare(*reader);
    auto docs = prepared->execute(segment);
    EXPECT_EQ(irs::doc_limits::eof(), docs->value());
    EXPECT_FALSE(docs->next());
    EXPECT_EQ(irs::doc_limits::eof(), docs->value());
  }

  // query with false expression without order
  {
    std::shared_ptr<arangodb::velocypack::Builder> bindVars;
    auto options = std::make_shared<arangodb::velocypack::Builder>();
    std::string const queryString =
        "LET c=1 LET b=2 FOR d IN testView FILTER c==b RETURN d";
    
    arangodb::aql::Query query(arangodb::transaction::StandaloneContext::Create(vocbase),
                               arangodb::aql::QueryString(queryString),
                               bindVars, options);

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

    // find expression root
    auto* expression = filterNode->getMember(0);
    ASSERT_TRUE(expression);

    // setup filter
    std::vector<std::string> EMPTY;
    arangodb::transaction::Methods trx(arangodb::transaction::StandaloneContext::Create(vocbase),
                                       EMPTY, EMPTY, EMPTY,
                                       arangodb::transaction::Options());
    std::unique_ptr<arangodb::aql::ExecutionPlan> plan(
        arangodb::aql::ExecutionPlan::instantiateFromAst(ast, false));

    arangodb::iresearch::ByExpression filter;
    EXPECT_FALSE(filter);
    filter.init(*plan, *ast, *expression);
    EXPECT_TRUE(filter);

    arangodb::iresearch::ExpressionExecutionContext execCtx;
    ctx.setTrx(&trx);
    execCtx.ctx = &ctx;
    FilterCtx queryCtx(execCtx);

    auto prepared = filter.prepare(*reader, irs::order::prepared::unordered(), &queryCtx);
    auto docs = prepared->execute(segment, irs::order::prepared::unordered(), &queryCtx);
    EXPECT_EQ(irs::doc_limits::eof(), docs->value());
    EXPECT_FALSE(docs->next());
    EXPECT_EQ(irs::doc_limits::eof(), docs->value());
  }

  // query with false expression without order (deferred execution)
  {
    std::shared_ptr<arangodb::velocypack::Builder> bindVars;
    auto options = std::make_shared<arangodb::velocypack::Builder>();
    std::string const queryString =
        "LET c=1 LET b=2 FOR d IN testView FILTER c==b RETURN d";
    
    arangodb::aql::Query query(arangodb::transaction::StandaloneContext::Create(vocbase),
                               arangodb::aql::QueryString(queryString),
                               bindVars, options);
    
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

    // find expression root
    auto* expression = filterNode->getMember(0);
    ASSERT_TRUE(expression);

    // setup filter
    std::vector<std::string> EMPTY;
    arangodb::transaction::Methods trx(arangodb::transaction::StandaloneContext::Create(vocbase),
                                       EMPTY, EMPTY, EMPTY,
                                       arangodb::transaction::Options());
    std::unique_ptr<arangodb::aql::ExecutionPlan> plan(
        arangodb::aql::ExecutionPlan::instantiateFromAst(ast, false));

    arangodb::iresearch::ByExpression filter;
    EXPECT_FALSE(filter);
    filter.init(*plan, *ast, *expression);
    EXPECT_TRUE(filter);

    arangodb::iresearch::ExpressionExecutionContext execCtx;
    ctx.setTrx(&trx);
    execCtx.ctx = &ctx;
    FilterCtx queryCtx(execCtx);

    auto prepared = filter.prepare(*reader, irs::order::prepared::unordered());
    auto docs = prepared->execute(segment, irs::order::prepared::unordered(), &queryCtx);
    EXPECT_EQ(irs::doc_limits::eof(), docs->value());
    EXPECT_FALSE(docs->next());
    EXPECT_EQ(irs::doc_limits::eof(), docs->value());
  }

  // query with true expression without order
  {
    std::shared_ptr<arangodb::velocypack::Builder> bindVars;
    auto options = std::make_shared<arangodb::velocypack::Builder>();
    std::string const queryString =
        "LET c=1 LET b=2 FOR d IN testView FILTER c<b RETURN d";
    
    arangodb::aql::Query query(arangodb::transaction::StandaloneContext::Create(vocbase),
                               arangodb::aql::QueryString(queryString),
                               bindVars, options);

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

    // find expression root
    auto* expression = filterNode->getMember(0);
    ASSERT_TRUE(expression);

    // setup filter
    std::vector<std::string> EMPTY;
    arangodb::transaction::Methods trx(arangodb::transaction::StandaloneContext::Create(vocbase),
                                       EMPTY, EMPTY, EMPTY,
                                       arangodb::transaction::Options());
    std::unique_ptr<arangodb::aql::ExecutionPlan> plan(
        arangodb::aql::ExecutionPlan::instantiateFromAst(ast, false));

    arangodb::iresearch::ByExpression filter;
    EXPECT_FALSE(filter);
    filter.init(*plan, *ast, *expression);
    EXPECT_TRUE(filter);

    arangodb::iresearch::ExpressionExecutionContext execCtx;
    ctx.setTrx(&trx);
    execCtx.ctx = &ctx;
    FilterCtx queryCtx(execCtx);

    auto prepared = filter.prepare(*reader, irs::order::prepared::unordered(), &queryCtx);
    EXPECT_EQ(irs::no_boost(), prepared->boost());  // no boost set
    EXPECT_EQ(typeid(prepared.get()), typeid(irs::all().prepare(*reader).get()));  // should be same type
    auto column = segment.column_reader("name");
    ASSERT_TRUE(column);
    auto columnValues = column->values();
    ASSERT_TRUE(columnValues);
    auto docs = prepared->execute(segment, irs::order::prepared::unordered(), &queryCtx);
    EXPECT_EQ(irs::doc_limits::invalid(), docs->value());
    auto* cost = irs::get<irs::cost>(*docs);
    ASSERT_TRUE(cost);
    EXPECT_EQ(arangodb::velocypack::ArrayIterator(testDataRoot).size(), cost->estimate());

    irs::bytes_ref value;
    for (auto doc : arangodb::velocypack::ArrayIterator(testDataRoot)) {
      EXPECT_TRUE(docs->next());
      EXPECT_TRUE(columnValues(docs->value(), value));
      EXPECT_TRUE(arangodb::iresearch::getStringRef(doc.get("name")) ==
                  irs::to_string<irs::string_ref>(value.c_str()));
    }
    EXPECT_FALSE(docs->next());
    EXPECT_EQ(irs::doc_limits::eof(), docs->value());
  }

  // query with true expression without order (deferred execution)
  {
    std::shared_ptr<arangodb::velocypack::Builder> bindVars;
    auto options = std::make_shared<arangodb::velocypack::Builder>();
    std::string const queryString =
        "LET c=1 LET b=2 FOR d IN testView FILTER c<b RETURN d";
    
    arangodb::aql::Query query(arangodb::transaction::StandaloneContext::Create(vocbase),
                               arangodb::aql::QueryString(queryString),
                               bindVars, options);

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

    // find expression root
    auto* expression = filterNode->getMember(0);
    ASSERT_TRUE(expression);

    // setup filter
    std::vector<std::string> EMPTY;
    arangodb::transaction::Methods trx(arangodb::transaction::StandaloneContext::Create(vocbase),
                                       EMPTY, EMPTY, EMPTY,
                                       arangodb::transaction::Options());
    std::unique_ptr<arangodb::aql::ExecutionPlan> plan(
        arangodb::aql::ExecutionPlan::instantiateFromAst(ast, false));

    arangodb::iresearch::ByExpression filter;
    EXPECT_FALSE(filter);
    filter.init(*plan, *ast, *expression);
    EXPECT_TRUE(filter);

    arangodb::iresearch::ExpressionExecutionContext execCtx;
    ctx.setTrx(&trx);
    execCtx.ctx = &ctx;
    FilterCtx queryCtx(execCtx);

    auto prepared = filter.prepare(*reader, irs::order::prepared::unordered());  // no context provided
    EXPECT_EQ(irs::no_boost(), prepared->boost());  // no boost set
    EXPECT_EQ(typeid(prepared.get()), typeid(irs::all().prepare(*reader).get()));  // should be same type
    auto column = segment.column_reader("name");
    ASSERT_TRUE(column);
    auto columnValues = column->values();
    ASSERT_TRUE(columnValues);
    auto docs = prepared->execute(segment, irs::order::prepared::unordered(), &queryCtx);
    EXPECT_EQ(irs::doc_limits::invalid(), docs->value());
    auto* cost = irs::get<irs::cost>(*docs);
    ASSERT_TRUE(cost);
    EXPECT_EQ(arangodb::velocypack::ArrayIterator(testDataRoot).size(), cost->estimate());

    irs::bytes_ref value;
    for (auto doc : arangodb::velocypack::ArrayIterator(testDataRoot)) {
      EXPECT_TRUE(docs->next());
      EXPECT_TRUE(columnValues(docs->value(), value));
      EXPECT_TRUE(arangodb::iresearch::getStringRef(doc.get("name")) ==
                  irs::to_string<irs::string_ref>(value.c_str()));
    }
    EXPECT_FALSE(docs->next());
    EXPECT_EQ(irs::doc_limits::eof(), docs->value());
  }

  // query with true expression without order (deferred execution)
  {
    std::shared_ptr<arangodb::velocypack::Builder> bindVars;
    auto options = std::make_shared<arangodb::velocypack::Builder>();
    std::string const queryString =
        "LET c=1 LET b=2 FOR d IN testView FILTER c<b RETURN d";

    arangodb::aql::Query query(arangodb::transaction::StandaloneContext::Create(vocbase),
                               arangodb::aql::QueryString(queryString),
                               bindVars, options);

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

    // find expression root
    auto* expression = filterNode->getMember(0);
    ASSERT_TRUE(expression);

    // setup filter
    std::vector<std::string> EMPTY;
    arangodb::transaction::Methods trx(arangodb::transaction::StandaloneContext::Create(vocbase),
                                       EMPTY, EMPTY, EMPTY,
                                       arangodb::transaction::Options());
    std::unique_ptr<arangodb::aql::ExecutionPlan> plan(
        arangodb::aql::ExecutionPlan::instantiateFromAst(ast, false));

    arangodb::iresearch::ByExpression filter;
    EXPECT_FALSE(filter);
    filter.init(*plan, *ast, *expression);
    EXPECT_TRUE(filter);

    arangodb::iresearch::ExpressionExecutionContext execCtx;
    ctx.setTrx(&trx);
    execCtx.ctx = nullptr;
    FilterCtx queryCtx(execCtx);

    auto prepared = filter.prepare(*reader, irs::order::prepared::unordered(),
                                   &queryCtx);       // invalid context provided
    EXPECT_EQ(irs::no_boost(), prepared->boost());  // no boost set
    auto column = segment.column_reader("name");
    ASSERT_TRUE(column);
    auto columnValues = column->values();
    ASSERT_TRUE(columnValues);
    execCtx.ctx = &ctx;  // fix context
    auto docs = prepared->execute(segment, irs::order::prepared::unordered(), &queryCtx);
    EXPECT_EQ(irs::doc_limits::invalid(), docs->value());
    auto* cost = irs::get<irs::cost>(*docs);
    ASSERT_TRUE(cost);
    EXPECT_EQ(arangodb::velocypack::ArrayIterator(testDataRoot).size(), cost->estimate());

    irs::bytes_ref value;
    for (auto doc : arangodb::velocypack::ArrayIterator(testDataRoot)) {
      EXPECT_TRUE(docs->next());
      EXPECT_TRUE(columnValues(docs->value(), value));
      EXPECT_TRUE(arangodb::iresearch::getStringRef(doc.get("name")) ==
                  irs::to_string<irs::string_ref>(value.c_str()));
    }
    EXPECT_FALSE(docs->next());
    EXPECT_EQ(irs::doc_limits::eof(), docs->value());
  }

  // query with true expression without order (deferred execution with invalid context)
  {
    std::shared_ptr<arangodb::velocypack::Builder> bindVars;
    auto options = std::make_shared<arangodb::velocypack::Builder>();
    std::string const queryString =
        "LET c=1 LET b=2 FOR d IN testView FILTER c<b RETURN d";
    
    arangodb::aql::Query query(arangodb::transaction::StandaloneContext::Create(vocbase),
                               arangodb::aql::QueryString(queryString),
                               bindVars, options);

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

    // find expression root
    auto* expression = filterNode->getMember(0);
    ASSERT_TRUE(expression);

    // setup filter
    std::vector<std::string> EMPTY;
    arangodb::transaction::Methods trx(arangodb::transaction::StandaloneContext::Create(vocbase),
                                       EMPTY, EMPTY, EMPTY,
                                       arangodb::transaction::Options());
    std::unique_ptr<arangodb::aql::ExecutionPlan> plan(
        arangodb::aql::ExecutionPlan::instantiateFromAst(ast, false));

    arangodb::iresearch::ByExpression filter;
    EXPECT_FALSE(filter);
    filter.init(*plan, *ast, *expression);
    EXPECT_TRUE(filter);

    arangodb::iresearch::ExpressionExecutionContext execCtx;
    ctx.setTrx(&trx);
    execCtx.ctx = nullptr;
    FilterCtx queryCtx(execCtx);

    auto prepared = filter.prepare(*reader, irs::order::prepared::unordered());  // no context provided
    EXPECT_EQ(irs::no_boost(), prepared->boost());  // no boost set
    auto docs = prepared->execute(segment, irs::order::prepared::unordered(), &queryCtx);
    EXPECT_TRUE(irs::doc_limits::eof(docs->value()));
    EXPECT_FALSE(docs->next());
  }

  // query with true expression without order (deferred execution with invalid context)
  {
    std::shared_ptr<arangodb::velocypack::Builder> bindVars;
    auto options = std::make_shared<arangodb::velocypack::Builder>();
    std::string const queryString =
        "LET c=1 LET b=2 FOR d IN testView FILTER c<b RETURN d";
    
    arangodb::aql::Query query(arangodb::transaction::StandaloneContext::Create(vocbase),
                               arangodb::aql::QueryString(queryString),
                               bindVars, options);

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

    // find expression root
    auto* expression = filterNode->getMember(0);
    ASSERT_TRUE(expression);

    // setup filter
    std::vector<std::string> EMPTY;
    arangodb::transaction::Methods trx(arangodb::transaction::StandaloneContext::Create(vocbase),
                                       EMPTY, EMPTY, EMPTY,
                                       arangodb::transaction::Options());
    std::unique_ptr<arangodb::aql::ExecutionPlan> plan(
        arangodb::aql::ExecutionPlan::instantiateFromAst(ast, false));

    arangodb::iresearch::ByExpression filter;
    EXPECT_FALSE(filter);
    filter.init(*plan, *ast, *expression);
    EXPECT_TRUE(filter);

    arangodb::iresearch::ExpressionExecutionContext execCtx;
    ctx.setTrx(&trx);
    execCtx.ctx = nullptr;
    FilterCtx queryCtx(execCtx);

    auto prepared = filter.prepare(*reader, irs::order::prepared::unordered());  // no context provided
    EXPECT_EQ(irs::no_boost(), prepared->boost());  // no boost set
    auto docs = prepared->execute(segment, irs::order::prepared::unordered(), &queryCtx);
    EXPECT_TRUE(irs::doc_limits::eof(docs->value()));
    EXPECT_FALSE(docs->next());
  }

  // query with nondeterministic expression without order
  {
    std::shared_ptr<arangodb::velocypack::Builder> bindVars;
    auto options = std::make_shared<arangodb::velocypack::Builder>();
    std::string const queryString =
        "LET c=1 LET b=2 FOR d IN testView FILTER "
        "_REFERENCE_(c)==_REFERENCE_(b) RETURN d";
    
    arangodb::aql::Query query(arangodb::transaction::StandaloneContext::Create(vocbase),
                               arangodb::aql::QueryString(queryString),
                               bindVars, options);

    ExpressionContextMock ctx;
    {
      arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt{2});
      arangodb::aql::AqlValueGuard guard(value, true);
      ctx.vars.emplace("b", value);
    }

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

    // find expression root
    auto* expression = filterNode->getMember(0);
    ASSERT_TRUE(expression);

    // setup filter
    std::vector<std::string> EMPTY;
    arangodb::transaction::Methods trx(arangodb::transaction::StandaloneContext::Create(vocbase),
                                       EMPTY, EMPTY, EMPTY,
                                       arangodb::transaction::Options());
    std::unique_ptr<arangodb::aql::ExecutionPlan> plan(
        arangodb::aql::ExecutionPlan::instantiateFromAst(ast, false));

    arangodb::iresearch::ByExpression filter;
    EXPECT_FALSE(filter);
    filter.init(*plan, *ast, *expression);
    EXPECT_TRUE(filter);

    arangodb::iresearch::ExpressionExecutionContext execCtx;
    ctx.setTrx(&trx);
    execCtx.ctx = &ctx;
    FilterCtx queryCtx(execCtx);

    auto prepared = filter.prepare(*reader, irs::order::prepared::unordered(), &queryCtx);
    auto column = segment.column_reader("name");
    ASSERT_TRUE(column);
    auto columnValues = column->values();
    ASSERT_TRUE(columnValues);
    auto docs = prepared->execute(segment, irs::order::prepared::unordered(), &queryCtx);
    EXPECT_EQ(irs::doc_limits::invalid(), docs->value());
    auto* score = irs::get<irs::score>(*docs);
    EXPECT_TRUE(score);
    EXPECT_TRUE(score->is_default());

    // set reachable filter condition
    {
      ctx.vars.erase("c");
      arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt{2});
      arangodb::aql::AqlValueGuard guard(value, true);
      ctx.vars.emplace("c", value);
    }

    irs::bytes_ref keyValue;
    auto it = arangodb::velocypack::ArrayIterator(testDataRoot);
    for (size_t i = 0; i < it.size() / 2; ++i) {
      ASSERT_TRUE(it.valid());
      auto doc = *it;
      EXPECT_TRUE(docs->next());
      EXPECT_TRUE(columnValues(docs->value(), keyValue));
      EXPECT_TRUE(arangodb::iresearch::getStringRef(doc.get("name")) ==
                  irs::to_string<irs::string_ref>(keyValue.c_str()));
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
    irs::order order;
    size_t collector_finish_count = 0;
    size_t field_collector_collect_count = 0;
    size_t term_collector_collect_count = 0;
    size_t scorer_score_count = 0;
    auto& sort = order.add<custom_sort>(false);

    sort.field_collector_collect =
        [&field_collector_collect_count](const irs::sub_reader&,
                                         const irs::term_reader&) -> void {
      ++field_collector_collect_count;
    };
    sort.collector_finish = [&collector_finish_count](irs::byte_type*,
                                                      const irs::index_reader&) -> void {
      ++collector_finish_count;
    };
    sort.term_collector_collect =
        [&term_collector_collect_count](const irs::sub_reader&, const irs::term_reader&,
                                        const irs::attribute_provider&) -> void {
      ++term_collector_collect_count;
    };
    sort.scorer_add = [](irs::doc_id_t& dst, const irs::doc_id_t& src) -> void {
      dst = src;
    };
    sort.scorer_less = [](const irs::doc_id_t& lhs, const irs::doc_id_t& rhs) -> bool {
      return (lhs & 0xAAAAAAAAAAAAAAAA) < (rhs & 0xAAAAAAAAAAAAAAAA);
    };
    sort.scorer_score = [&scorer_score_count](irs::doc_id_t) -> void {
      ++scorer_score_count;
    };
    auto preparedOrder = order.prepare();

    std::shared_ptr<arangodb::velocypack::Builder> bindVars;
    auto options = std::make_shared<arangodb::velocypack::Builder>();
    std::string const queryString =
        "LET c=1 LET b=2 FOR d IN testView FILTER "
        "_REFERENCE_(c)==_REFERENCE_(b) RETURN d";
    
    arangodb::aql::Query query(arangodb::transaction::StandaloneContext::Create(vocbase),
                               arangodb::aql::QueryString(queryString),
                               bindVars, options);

    ExpressionContextMock ctx;
    {
      arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt{2});
      arangodb::aql::AqlValueGuard guard(value, true);
      ctx.vars.emplace("b", value);
    }

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

    // find expression root
    auto* expression = filterNode->getMember(0);
    ASSERT_TRUE(expression);

    // setup filter
    std::vector<std::string> EMPTY;
    arangodb::transaction::Methods trx(arangodb::transaction::StandaloneContext::Create(vocbase),
                                       EMPTY, EMPTY, EMPTY,
                                       arangodb::transaction::Options());
    std::unique_ptr<arangodb::aql::ExecutionPlan> plan(
        arangodb::aql::ExecutionPlan::instantiateFromAst(ast, false));

    arangodb::iresearch::ByExpression filter;
    EXPECT_FALSE(filter);
    filter.init(*plan, *ast, *expression);
    EXPECT_TRUE(filter);

    arangodb::iresearch::ExpressionExecutionContext execCtx;
    ctx.setTrx(&trx);
    execCtx.ctx = &ctx;
    FilterCtx queryCtx(execCtx);

    filter.boost(1.5f);
    EXPECT_EQ(1.5f, filter.boost());

    auto prepared = filter.prepare(*reader, preparedOrder, &queryCtx);
    EXPECT_EQ(1.5f, prepared->boost());

    auto column = segment.column_reader("name");
    ASSERT_TRUE(column);
    auto columnValues = column->values();
    ASSERT_TRUE(columnValues);
    auto docs = prepared->execute(segment, preparedOrder, &queryCtx);
    EXPECT_EQ(irs::doc_limits::invalid(), docs->value());
    auto* score = irs::get<irs::score>(*docs);
    EXPECT_TRUE(score);
    EXPECT_FALSE(score->is_default());
    auto* cost = irs::get<irs::cost>(*docs);
    ASSERT_TRUE(cost);
    EXPECT_EQ(arangodb::velocypack::ArrayIterator(testDataRoot).size(), cost->estimate());

    // set reachable filter condition
    {
      ctx.vars.erase("c");
      arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt{2});
      arangodb::aql::AqlValueGuard guard(value, true);
      ctx.vars.emplace("c", value);
    }

    irs::bytes_ref keyValue;
    auto it = arangodb::velocypack::ArrayIterator(testDataRoot);
    for (size_t i = 0; i < it.size() / 2; ++i) {
      ASSERT_TRUE(it.valid());
      auto doc = *it;
      EXPECT_TRUE(docs->next());
      [[maybe_unused]] auto* scoreValue = score->evaluate();
      EXPECT_TRUE(columnValues(docs->value(), keyValue));
      EXPECT_TRUE(arangodb::iresearch::getStringRef(doc.get("name")) ==
                  irs::to_string<irs::string_ref>(keyValue.c_str()));
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
    auto options = std::make_shared<arangodb::velocypack::Builder>();
    std::string const queryString =
        "LET c=1 LET b=2 FOR d IN testView FILTER "
        "_REFERENCE_(c)==_REFERENCE_(b) RETURN d";
    
    arangodb::aql::Query query(arangodb::transaction::StandaloneContext::Create(vocbase),
                               arangodb::aql::QueryString(queryString),
                               bindVars, options);

    ExpressionContextMock ctx;
    {
      arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt{2});
      arangodb::aql::AqlValueGuard guard(value, true);
      ctx.vars.emplace("b", value);
    }
    
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

    // find expression root
    auto* expression = filterNode->getMember(0);
    ASSERT_TRUE(expression);

    // setup filter
    std::vector<std::string> EMPTY;
    arangodb::transaction::Methods trx(arangodb::transaction::StandaloneContext::Create(vocbase),
                                       EMPTY, EMPTY, EMPTY,
                                       arangodb::transaction::Options());
    std::unique_ptr<arangodb::aql::ExecutionPlan> plan(
        arangodb::aql::ExecutionPlan::instantiateFromAst(ast, false));

    arangodb::iresearch::ByExpression filter;
    EXPECT_FALSE(filter);
    filter.init(*plan, *ast, *expression);
    EXPECT_TRUE(filter);

    arangodb::iresearch::ExpressionExecutionContext execCtx;
    ctx.setTrx(&trx);
    execCtx.ctx = &ctx;
    FilterCtx queryCtx(execCtx);

    auto prepared = filter.prepare(*reader, irs::order::prepared::unordered(), &queryCtx);
    auto column = segment.column_reader("name");
    ASSERT_TRUE(column);
    auto columnValues = column->values();
    ASSERT_TRUE(columnValues);
    auto docs = prepared->execute(segment, irs::order::prepared::unordered(), &queryCtx);
    EXPECT_EQ(irs::doc_limits::invalid(), docs->value());
    auto* score = irs::get<irs::score>(*docs);
    EXPECT_TRUE(score);
    EXPECT_TRUE(score->is_default());
    auto* cost = irs::get<irs::cost>(*docs);
    ASSERT_TRUE(cost);
    EXPECT_EQ(arangodb::velocypack::ArrayIterator(testDataRoot).size(), cost->estimate());

    // set reachable filter condition
    {
      ctx.vars.erase("c");
      arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt{2});
      arangodb::aql::AqlValueGuard guard(value, true);
      ctx.vars.emplace("c", value);
    }

    auto it = arangodb::velocypack::ArrayIterator(testDataRoot);
    irs::bytes_ref keyValue;

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
      EXPECT_TRUE(columnValues(docs->value(), keyValue));
      EXPECT_TRUE(arangodb::iresearch::getStringRef(doc.get("name")) ==
                  irs::to_string<irs::string_ref>(keyValue.c_str()));
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
