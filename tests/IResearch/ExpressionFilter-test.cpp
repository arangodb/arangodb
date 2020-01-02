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
#include "RestServer/QueryRegistryFeature.h"
#include "RestServer/SystemDatabaseFeature.h"
#include "RestServer/TraverserEngineRegistryFeature.h"
#include "RestServer/ViewTypesFeature.h"
#include "Sharding/ShardingFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
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
  DECLARE_SORT_TYPE();

  class prepared : public irs::sort::prepared_base<irs::doc_id_t, void> {
   public:
    class collector : public irs::sort::field_collector, public irs::sort::term_collector {
     public:
      collector(const custom_sort& sort) : sort_(sort) {}

      virtual void collect(const irs::sub_reader& segment,
                           const irs::term_reader& field) override {
        if (sort_.field_collector_collect) {
          sort_.field_collector_collect(segment, field);
        }
      }

      virtual void collect(const irs::sub_reader& segment, const irs::term_reader& field,
                           const irs::attribute_view& term_attrs) override {
        if (sort_.term_collector_collect) {
          sort_.term_collector_collect(segment, field, term_attrs);
        }
      }

      virtual void collect(const irs::bytes_ref& in) override {}

      virtual void write(irs::data_output& out) const override {}

     private:
      const custom_sort& sort_;
    };

    struct scorer : public irs::score_ctx {
      scorer(const custom_sort& sort,
             const irs::sub_reader& segment_reader,
             const irs::term_reader& term_reader,
             const irs::byte_type* stats,
             const irs::attribute_view& document_attrs)
          : document_attrs_(document_attrs),
            stats_(stats),
            segment_reader_(segment_reader),
            sort_(sort),
            term_reader_(term_reader) {}

      const irs::attribute_view& document_attrs_;
      const irs::byte_type* stats_;
      const irs::sub_reader& segment_reader_;
      const custom_sort& sort_;
      const irs::term_reader& term_reader_;
    };

    DECLARE_FACTORY(prepared);

    prepared(const custom_sort& sort) : sort_(sort) {}

    virtual void collect(irs::byte_type* filter_attrs,
                         const irs::index_reader& index,
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

      return irs::memory::make_unique<custom_sort::prepared::collector>(sort_);
    }

    virtual std::pair<irs::score_ctx_ptr, irs::score_f> prepare_scorer(
        irs::sub_reader const& segment_reader,
        irs::term_reader const& term_reader,
        irs::byte_type const* filter_node_attrs,
        irs::attribute_view const& document_attrs,
        irs::boost_t boost) const override {
      if (sort_.prepare_scorer) {
        return sort_.prepare_scorer(
          segment_reader, term_reader,
          filter_node_attrs, document_attrs, boost);
      }

      return {
        std::make_unique<custom_sort::prepared::scorer>(
          sort_, segment_reader, term_reader,
          filter_node_attrs, document_attrs),
        [](const irs::score_ctx* ctx, irs::byte_type* score_buf) {
          auto& ctxImpl = *reinterpret_cast<const custom_sort::prepared::scorer*>(ctx);

          EXPECT_TRUE(score_buf);
          auto& doc_id = *reinterpret_cast<irs::doc_id_t*>(score_buf);

          doc_id = ctxImpl.document_attrs_.get<irs::document>()->value;

          if (ctxImpl.sort_.scorer_score) {
            ctxImpl.sort_.scorer_score(doc_id);
          }
        }
      };
    }

    virtual irs::sort::term_collector::ptr prepare_term_collector() const override {
      if (sort_.prepare_term_collector) {
        return sort_.prepare_term_collector();
      }

      return irs::memory::make_unique<custom_sort::prepared::collector>(sort_);
    }

    virtual void prepare_score(irs::byte_type* score) const override {
      score_cast(score) = irs::doc_limits::invalid();
    }

    virtual void add(irs::byte_type* dst, irs::byte_type const* src) const override {
      if (sort_.scorer_add) {
        sort_.scorer_add(score_cast(dst), score_cast(src));
      }
    }

    virtual void  merge(irs::byte_type* dst, const irs::byte_type** src_start,
      const size_t size, size_t offset) const  override {
      auto& casted_dst = score_cast(dst + offset);
      casted_dst = irs::doc_limits::invalid();
      for (size_t i = 0; i < size; ++i) {
        if (sort_.scorer_add) {
          sort_.scorer_add(casted_dst, score_cast(src_start[i] + offset));
        }
      }
    }

    virtual bool less(irs::byte_type const* lhs, const irs::byte_type* rhs) const override {
      return sort_.scorer_less ? sort_.scorer_less(score_cast(lhs), score_cast(rhs)) : false;
    }

   private:
    const custom_sort& sort_;
  };

  std::function<void(const irs::sub_reader&, const irs::term_reader&)> field_collector_collect;
  std::function<void(const irs::sub_reader&, const irs::term_reader&, const irs::attribute_view&)> term_collector_collect;
  std::function<void(irs::byte_type*, const irs::index_reader&)> collector_finish;
  std::function<irs::sort::field_collector::ptr()> prepare_field_collector;
  std::function<std::pair<irs::score_ctx_ptr, irs::score_f>(const irs::sub_reader&, const irs::term_reader&, const irs::byte_type*, const irs::attribute_view&, irs::boost_t)> prepare_scorer;
  std::function<irs::sort::term_collector::ptr()> prepare_term_collector;
  std::function<void(irs::doc_id_t&, const irs::doc_id_t&)> scorer_add;
  std::function<bool(const irs::doc_id_t&, const irs::doc_id_t&)> scorer_less;
  std::function<void(irs::doc_id_t&)> scorer_score;

  DECLARE_FACTORY();
  custom_sort() : sort(custom_sort::type()) {}
  virtual prepared::ptr prepare() const override {
    return std::make_unique<custom_sort::prepared>(*this);
  }
};

DEFINE_SORT_TYPE(custom_sort)
DEFINE_FACTORY_DEFAULT(custom_sort)

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

struct IResearchExpressionFilterTest
    : public ::testing::Test,
      public arangodb::tests::LogSuppressor<arangodb::Logger::AUTHENTICATION, arangodb::LogLevel::ERR>,
      public arangodb::tests::LogSuppressor<arangodb::iresearch::TOPIC, arangodb::LogLevel::FATAL>,
      public arangodb::tests::IResearchLogSuppressor {
  StorageEngineMock engine;
  arangodb::application_features::ApplicationServer server;
  std::unique_ptr<TRI_vocbase_t> system;
  std::vector<std::pair<arangodb::application_features::ApplicationFeature&, bool>> features;

  IResearchExpressionFilterTest() : engine(server), server(nullptr, nullptr) {
    arangodb::EngineSelectorFeature::ENGINE = &engine;

    arangodb::tests::init(true);

    // setup required application features
    features.emplace_back(server.addFeature<arangodb::ViewTypesFeature>(), true);
    features.emplace_back(server.addFeature<arangodb::AuthenticationFeature>(), true);
    features.emplace_back(server.addFeature<arangodb::DatabasePathFeature>(), false);
    features.emplace_back(server.addFeature<arangodb::DatabaseFeature>(), false);
    features.emplace_back(server.addFeature<arangodb::QueryRegistryFeature>(), false);  // must be first
    system = irs::memory::make_unique<TRI_vocbase_t>(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, systemDBInfo(server));
    features.emplace_back(server.addFeature<arangodb::SystemDatabaseFeature>(
                              system.get()),
                          false);  // required for IResearchAnalyzerFeature
    features.emplace_back(server.addFeature<arangodb::TraverserEngineRegistryFeature>(),
                          false);  // must be before AqlFeature
    features.emplace_back(server.addFeature<arangodb::AqlFeature>(), true);
    features.emplace_back(server.addFeature<arangodb::ShardingFeature>(), false);
    features.emplace_back(server.addFeature<arangodb::aql::OptimizerRulesFeature>(), true);
    features.emplace_back(server.addFeature<arangodb::aql::AqlFunctionFeature>(),
                          true);  // required for IResearchAnalyzerFeature
    features.emplace_back(server.addFeature<arangodb::iresearch::IResearchAnalyzerFeature>(),
                          true);
    features.emplace_back(server.addFeature<arangodb::iresearch::IResearchFeature>(), true);

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
            arangodb::aql::Function::Flags::CanRunOnDBServer),
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
    arangodb::EngineSelectorFeature::ENGINE = nullptr;

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
    EXPECT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), docs->value());
    EXPECT_FALSE(docs->next());
    EXPECT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), docs->value());
  }

  // query with false expression without order
  {
    std::shared_ptr<arangodb::velocypack::Builder> bindVars;
    auto options = std::make_shared<arangodb::velocypack::Builder>();
    std::string const queryString =
        "LET c=1 LET b=2 FOR d IN testView FILTER c==b RETURN d";

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

    // find expression root
    auto* expression = filterNode->getMember(0);
    ASSERT_TRUE(expression);

    // setup filter
    std::vector<std::string> EMPTY;
    arangodb::transaction::Methods trx(arangodb::transaction::StandaloneContext::Create(vocbase),
                                       EMPTY, EMPTY, EMPTY,
                                       arangodb::transaction::Options());
    std::unique_ptr<arangodb::aql::ExecutionPlan> plan(
        arangodb::aql::ExecutionPlan::instantiateFromAst(ast));

    arangodb::iresearch::ByExpression filter;
    EXPECT_FALSE(filter);
    filter.init(*plan, *ast, *expression);
    EXPECT_TRUE(filter);

    arangodb::iresearch::ExpressionExecutionContext execCtx;
    execCtx.trx = &trx;
    execCtx.ctx = &ctx;
    irs::attribute_view queryCtx;
    queryCtx.emplace(execCtx);

    auto prepared = filter.prepare(*reader, irs::order::prepared::unordered(), queryCtx);
    auto docs = prepared->execute(segment, irs::order::prepared::unordered(), queryCtx);
    EXPECT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), docs->value());
    EXPECT_FALSE(docs->next());
    EXPECT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), docs->value());
  }

  // query with false expression without order (deferred execution)
  {
    std::shared_ptr<arangodb::velocypack::Builder> bindVars;
    auto options = std::make_shared<arangodb::velocypack::Builder>();
    std::string const queryString =
        "LET c=1 LET b=2 FOR d IN testView FILTER c==b RETURN d";

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

    // find expression root
    auto* expression = filterNode->getMember(0);
    ASSERT_TRUE(expression);

    // setup filter
    std::vector<std::string> EMPTY;
    arangodb::transaction::Methods trx(arangodb::transaction::StandaloneContext::Create(vocbase),
                                       EMPTY, EMPTY, EMPTY,
                                       arangodb::transaction::Options());
    std::unique_ptr<arangodb::aql::ExecutionPlan> plan(
        arangodb::aql::ExecutionPlan::instantiateFromAst(ast));

    arangodb::iresearch::ByExpression filter;
    EXPECT_FALSE(filter);
    filter.init(*plan, *ast, *expression);
    EXPECT_TRUE(filter);

    arangodb::iresearch::ExpressionExecutionContext execCtx;
    execCtx.trx = &trx;
    execCtx.ctx = &ctx;
    irs::attribute_view queryCtx;
    queryCtx.emplace(execCtx);

    auto prepared = filter.prepare(*reader, irs::order::prepared::unordered());
    auto docs = prepared->execute(segment, irs::order::prepared::unordered(), queryCtx);
    EXPECT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), docs->value());
    EXPECT_FALSE(docs->next());
    EXPECT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), docs->value());
  }

  // query with true expression without order
  {
    std::shared_ptr<arangodb::velocypack::Builder> bindVars;
    auto options = std::make_shared<arangodb::velocypack::Builder>();
    std::string const queryString =
        "LET c=1 LET b=2 FOR d IN testView FILTER c<b RETURN d";

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

    // find expression root
    auto* expression = filterNode->getMember(0);
    ASSERT_TRUE(expression);

    // setup filter
    std::vector<std::string> EMPTY;
    arangodb::transaction::Methods trx(arangodb::transaction::StandaloneContext::Create(vocbase),
                                       EMPTY, EMPTY, EMPTY,
                                       arangodb::transaction::Options());
    std::unique_ptr<arangodb::aql::ExecutionPlan> plan(
        arangodb::aql::ExecutionPlan::instantiateFromAst(ast));

    arangodb::iresearch::ByExpression filter;
    EXPECT_FALSE(filter);
    filter.init(*plan, *ast, *expression);
    EXPECT_TRUE(filter);

    arangodb::iresearch::ExpressionExecutionContext execCtx;
    execCtx.trx = &trx;
    execCtx.ctx = &ctx;
    irs::attribute_view queryCtx;
    queryCtx.emplace(execCtx);

    auto prepared = filter.prepare(*reader, irs::order::prepared::unordered(), queryCtx);
    EXPECT_EQ(irs::no_boost(), prepared->boost());  // no boost set
    EXPECT_EQ(typeid(prepared.get()), typeid(irs::all().prepare(*reader).get()));  // should be same type
    auto column = segment.column_reader("name");
    ASSERT_TRUE(column);
    auto columnValues = column->values();
    ASSERT_TRUE(columnValues);
    auto docs = prepared->execute(segment, irs::order::prepared::unordered(), queryCtx);
    EXPECT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), docs->value());
    auto& cost = docs->attributes().get<irs::cost>();
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
    EXPECT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), docs->value());
  }

  // query with true expression without order (deferred execution)
  {
    std::shared_ptr<arangodb::velocypack::Builder> bindVars;
    auto options = std::make_shared<arangodb::velocypack::Builder>();
    std::string const queryString =
        "LET c=1 LET b=2 FOR d IN testView FILTER c<b RETURN d";

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

    // find expression root
    auto* expression = filterNode->getMember(0);
    ASSERT_TRUE(expression);

    // setup filter
    std::vector<std::string> EMPTY;
    arangodb::transaction::Methods trx(arangodb::transaction::StandaloneContext::Create(vocbase),
                                       EMPTY, EMPTY, EMPTY,
                                       arangodb::transaction::Options());
    std::unique_ptr<arangodb::aql::ExecutionPlan> plan(
        arangodb::aql::ExecutionPlan::instantiateFromAst(ast));

    arangodb::iresearch::ByExpression filter;
    EXPECT_FALSE(filter);
    filter.init(*plan, *ast, *expression);
    EXPECT_TRUE(filter);

    arangodb::iresearch::ExpressionExecutionContext execCtx;
    execCtx.trx = &trx;
    execCtx.ctx = &ctx;
    irs::attribute_view queryCtx;
    queryCtx.emplace(execCtx);

    auto prepared = filter.prepare(*reader, irs::order::prepared::unordered());  // no context provided
    EXPECT_EQ(irs::no_boost(), prepared->boost());  // no boost set
    EXPECT_EQ(typeid(prepared.get()), typeid(irs::all().prepare(*reader).get()));  // should be same type
    auto column = segment.column_reader("name");
    ASSERT_TRUE(column);
    auto columnValues = column->values();
    ASSERT_TRUE(columnValues);
    auto docs = prepared->execute(segment, irs::order::prepared::unordered(), queryCtx);
    EXPECT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), docs->value());
    auto& cost = docs->attributes().get<irs::cost>();
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
    EXPECT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), docs->value());
  }

  // query with true expression without order (deferred execution)
  {
    std::shared_ptr<arangodb::velocypack::Builder> bindVars;
    auto options = std::make_shared<arangodb::velocypack::Builder>();
    std::string const queryString =
        "LET c=1 LET b=2 FOR d IN testView FILTER c<b RETURN d";

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

    // find expression root
    auto* expression = filterNode->getMember(0);
    ASSERT_TRUE(expression);

    // setup filter
    std::vector<std::string> EMPTY;
    arangodb::transaction::Methods trx(arangodb::transaction::StandaloneContext::Create(vocbase),
                                       EMPTY, EMPTY, EMPTY,
                                       arangodb::transaction::Options());
    std::unique_ptr<arangodb::aql::ExecutionPlan> plan(
        arangodb::aql::ExecutionPlan::instantiateFromAst(ast));

    arangodb::iresearch::ByExpression filter;
    EXPECT_FALSE(filter);
    filter.init(*plan, *ast, *expression);
    EXPECT_TRUE(filter);

    arangodb::iresearch::ExpressionExecutionContext execCtx;
    execCtx.trx = &trx;
    execCtx.ctx = nullptr;
    irs::attribute_view queryCtx;
    queryCtx.emplace(execCtx);

    auto prepared = filter.prepare(*reader, irs::order::prepared::unordered(),
                                   queryCtx);  // invalid context provided
    EXPECT_EQ(irs::no_boost(), prepared->boost());  // no boost set
    auto column = segment.column_reader("name");
    ASSERT_TRUE(column);
    auto columnValues = column->values();
    ASSERT_TRUE(columnValues);
    execCtx.ctx = &ctx;  // fix context
    auto docs = prepared->execute(segment, irs::order::prepared::unordered(), queryCtx);
    EXPECT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), docs->value());
    auto& cost = docs->attributes().get<irs::cost>();
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
    EXPECT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), docs->value());
  }

  // query with true expression without order (deferred execution with invalid context)
  {
    std::shared_ptr<arangodb::velocypack::Builder> bindVars;
    auto options = std::make_shared<arangodb::velocypack::Builder>();
    std::string const queryString =
        "LET c=1 LET b=2 FOR d IN testView FILTER c<b RETURN d";

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

    // find expression root
    auto* expression = filterNode->getMember(0);
    ASSERT_TRUE(expression);

    // setup filter
    std::vector<std::string> EMPTY;
    arangodb::transaction::Methods trx(arangodb::transaction::StandaloneContext::Create(vocbase),
                                       EMPTY, EMPTY, EMPTY,
                                       arangodb::transaction::Options());
    std::unique_ptr<arangodb::aql::ExecutionPlan> plan(
        arangodb::aql::ExecutionPlan::instantiateFromAst(ast));

    arangodb::iresearch::ByExpression filter;
    EXPECT_FALSE(filter);
    filter.init(*plan, *ast, *expression);
    EXPECT_TRUE(filter);

    arangodb::iresearch::ExpressionExecutionContext execCtx;
    execCtx.trx = &trx;
    execCtx.ctx = nullptr;
    irs::attribute_view queryCtx;
    queryCtx.emplace(execCtx);

    auto prepared = filter.prepare(*reader, irs::order::prepared::unordered());  // no context provided
    EXPECT_EQ(irs::no_boost(), prepared->boost());  // no boost set
    auto docs = prepared->execute(segment, irs::order::prepared::unordered(), queryCtx);
    EXPECT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs->value()));
    EXPECT_FALSE(docs->next());
  }

  // query with true expression without order (deferred execution with invalid context)
  {
    std::shared_ptr<arangodb::velocypack::Builder> bindVars;
    auto options = std::make_shared<arangodb::velocypack::Builder>();
    std::string const queryString =
        "LET c=1 LET b=2 FOR d IN testView FILTER c<b RETURN d";

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

    // find expression root
    auto* expression = filterNode->getMember(0);
    ASSERT_TRUE(expression);

    // setup filter
    std::vector<std::string> EMPTY;
    arangodb::transaction::Methods trx(arangodb::transaction::StandaloneContext::Create(vocbase),
                                       EMPTY, EMPTY, EMPTY,
                                       arangodb::transaction::Options());
    std::unique_ptr<arangodb::aql::ExecutionPlan> plan(
        arangodb::aql::ExecutionPlan::instantiateFromAst(ast));

    arangodb::iresearch::ByExpression filter;
    EXPECT_FALSE(filter);
    filter.init(*plan, *ast, *expression);
    EXPECT_TRUE(filter);

    arangodb::iresearch::ExpressionExecutionContext execCtx;
    execCtx.trx = nullptr;
    execCtx.ctx = &ctx;
    irs::attribute_view queryCtx;
    queryCtx.emplace(execCtx);

    auto prepared = filter.prepare(*reader, irs::order::prepared::unordered());  // no context provided
    EXPECT_EQ(irs::no_boost(), prepared->boost());  // no boost set
    auto docs = prepared->execute(segment, irs::order::prepared::unordered(), queryCtx);
    EXPECT_TRUE(irs::type_limits<irs::type_t::doc_id_t>::eof(docs->value()));
    EXPECT_FALSE(docs->next());
  }

  // query with nondeterministic expression without order
  {
    std::shared_ptr<arangodb::velocypack::Builder> bindVars;
    auto options = std::make_shared<arangodb::velocypack::Builder>();
    std::string const queryString =
        "LET c=1 LET b=2 FOR d IN testView FILTER "
        "_REFERENCE_(c)==_REFERENCE_(b) RETURN d";

    ExpressionContextMock ctx;
    {
      arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt{2});
      arangodb::aql::AqlValueGuard guard(value, true);
      ctx.vars.emplace("b", value);
    }

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

    // find expression root
    auto* expression = filterNode->getMember(0);
    ASSERT_TRUE(expression);

    // setup filter
    std::vector<std::string> EMPTY;
    arangodb::transaction::Methods trx(arangodb::transaction::StandaloneContext::Create(vocbase),
                                       EMPTY, EMPTY, EMPTY,
                                       arangodb::transaction::Options());
    std::unique_ptr<arangodb::aql::ExecutionPlan> plan(
        arangodb::aql::ExecutionPlan::instantiateFromAst(ast));

    arangodb::iresearch::ByExpression filter;
    EXPECT_FALSE(filter);
    filter.init(*plan, *ast, *expression);
    EXPECT_TRUE(filter);

    arangodb::iresearch::ExpressionExecutionContext execCtx;
    execCtx.trx = &trx;
    execCtx.ctx = &ctx;
    irs::attribute_view queryCtx;
    queryCtx.emplace(execCtx);

    auto prepared = filter.prepare(*reader, irs::order::prepared::unordered(), queryCtx);
    auto column = segment.column_reader("name");
    ASSERT_TRUE(column);
    auto columnValues = column->values();
    ASSERT_TRUE(columnValues);
    auto docs = prepared->execute(segment, irs::order::prepared::unordered(), queryCtx);
    EXPECT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), docs->value());
    EXPECT_FALSE(docs->attributes().get<irs::score>());

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
    EXPECT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), docs->value());
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
                                        const irs::attribute_view&) -> void {
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

    ExpressionContextMock ctx;
    {
      arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt{2});
      arangodb::aql::AqlValueGuard guard(value, true);
      ctx.vars.emplace("b", value);
    }

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

    // find expression root
    auto* expression = filterNode->getMember(0);
    ASSERT_TRUE(expression);

    // setup filter
    std::vector<std::string> EMPTY;
    arangodb::transaction::Methods trx(arangodb::transaction::StandaloneContext::Create(vocbase),
                                       EMPTY, EMPTY, EMPTY,
                                       arangodb::transaction::Options());
    std::unique_ptr<arangodb::aql::ExecutionPlan> plan(
        arangodb::aql::ExecutionPlan::instantiateFromAst(ast));

    arangodb::iresearch::ByExpression filter;
    EXPECT_FALSE(filter);
    filter.init(*plan, *ast, *expression);
    EXPECT_TRUE(filter);

    arangodb::iresearch::ExpressionExecutionContext execCtx;
    execCtx.trx = &trx;
    execCtx.ctx = &ctx;
    irs::attribute_view queryCtx;
    queryCtx.emplace(execCtx);

    filter.boost(1.5f);
    EXPECT_EQ(1.5f, filter.boost());

    auto prepared = filter.prepare(*reader, preparedOrder, queryCtx);
    EXPECT_EQ(1.5f, prepared->boost());

    auto column = segment.column_reader("name");
    ASSERT_TRUE(column);
    auto columnValues = column->values();
    ASSERT_TRUE(columnValues);
    auto docs = prepared->execute(segment, preparedOrder, queryCtx);
    EXPECT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), docs->value());
    auto& score = docs->attributes().get<irs::score>();
    EXPECT_TRUE(score);
    EXPECT_FALSE(score->empty());
    auto& cost = docs->attributes().get<irs::cost>();
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
      score->evaluate();
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
    EXPECT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), docs->value());

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

    ExpressionContextMock ctx;
    {
      arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt{2});
      arangodb::aql::AqlValueGuard guard(value, true);
      ctx.vars.emplace("b", value);
    }

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

    // find expression root
    auto* expression = filterNode->getMember(0);
    ASSERT_TRUE(expression);

    // setup filter
    std::vector<std::string> EMPTY;
    arangodb::transaction::Methods trx(arangodb::transaction::StandaloneContext::Create(vocbase),
                                       EMPTY, EMPTY, EMPTY,
                                       arangodb::transaction::Options());
    std::unique_ptr<arangodb::aql::ExecutionPlan> plan(
        arangodb::aql::ExecutionPlan::instantiateFromAst(ast));

    arangodb::iresearch::ByExpression filter;
    EXPECT_FALSE(filter);
    filter.init(*plan, *ast, *expression);
    EXPECT_TRUE(filter);

    arangodb::iresearch::ExpressionExecutionContext execCtx;
    execCtx.trx = &trx;
    execCtx.ctx = &ctx;
    irs::attribute_view queryCtx;
    queryCtx.emplace(execCtx);

    auto prepared = filter.prepare(*reader, irs::order::prepared::unordered(), queryCtx);
    auto column = segment.column_reader("name");
    ASSERT_TRUE(column);
    auto columnValues = column->values();
    ASSERT_TRUE(columnValues);
    auto docs = prepared->execute(segment, irs::order::prepared::unordered(), queryCtx);
    EXPECT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), docs->value());
    EXPECT_FALSE(docs->attributes().get<irs::score>());
    auto& cost = docs->attributes().get<irs::cost>();
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
    EXPECT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), docs->value());
  }
}
