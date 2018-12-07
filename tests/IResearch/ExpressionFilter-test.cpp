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

#if USE_ENTERPRISE
  #include "Enterprise/Ldap/LdapFeature.h"
#endif

#include "Aql/AqlFunctionFeature.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/OptimizerRulesFeature.h"
#include "Aql/Ast.h"
#include "Aql/Query.h"
#include "Aql/OptimizerRulesFeature.h"
#include "Basics/VelocyPackHelper.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "IResearch/ExpressionFilter.h"
#include "IResearch/IResearchCommon.h"
#include "IResearch/IResearchFeature.h"
#include "IResearch/IResearchFilterFactory.h"
#include "IResearch/IResearchView.h"
#include "IResearch/IResearchAnalyzerFeature.h"
#include "IResearch/VelocyPackHelper.h"
#include "Logger/Logger.h"
#include "Logger/LogTopic.h"
#include "RestServer/DatabasePathFeature.h"
#include "RestServer/ViewTypesFeature.h"
#include "RestServer/AqlFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "RestServer/SystemDatabaseFeature.h"
#include "RestServer/TraverserEngineRegistryFeature.h"
#include "Sharding/ShardingFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "Transaction/Methods.h"
#include "Transaction/StandaloneContext.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/LogicalView.h"
#include "VocBase/ManagedDocumentResult.h"

#include "utils/utf8_path.hpp"
#include "velocypack/Iterator.h"
#include "ExpressionContextMock.h"

#include "IResearch/ExpressionFilter.h"
#include "index/directory_reader.hpp"
#include "store/memory_directory.hpp"
#include "store/store_utils.hpp"
#include "search/all_filter.hpp"
#include "analysis/token_attributes.hpp"
#include "search/scorers.hpp"
#include "search/score.hpp"
#include "search/cost.hpp"
#include "utils/type_limits.hpp"
#include "3rdParty/iresearch/tests/tests_config.hpp"

extern const char* ARGV0; // defined in main.cpp

NS_LOCAL

struct custom_sort: public irs::sort {
  DECLARE_SORT_TYPE();

  class prepared: public irs::sort::prepared_base<irs::doc_id_t> {
   public:
    class collector: public irs::sort::collector {
     public:
      virtual void collect(
        const irs::sub_reader& segment,
        const irs::term_reader& field,
        const irs::attribute_view& term_attrs
      ) override {
        if (sort_.collector_collect) {
          sort_.collector_collect(segment, field, term_attrs);
        }
      }

      virtual void finish(
          irs::attribute_store& filter_attrs,
          const irs::index_reader& index
      ) override {
        if (sort_.collector_finish) {
          sort_.collector_finish(filter_attrs, index);
        }
      }

      collector(const custom_sort& sort): sort_(sort) {
      }

     private:
      const custom_sort& sort_;
    };

    class scorer: public irs::sort::scorer_base<irs::doc_id_t> {
     public:
      virtual void score(irs::byte_type* score_buf) override {
        UNUSED(filter_node_attrs_);
        UNUSED(segment_reader_);
        UNUSED(term_reader_);
        CHECK(score_buf);
        auto& doc_id = *reinterpret_cast<irs::doc_id_t*>(score_buf);

        doc_id = document_attrs_.get<irs::document>()->value;

        if (sort_.scorer_score) {
          sort_.scorer_score(doc_id);
        }
      }

      scorer(
        const custom_sort& sort,
        const irs::sub_reader& segment_reader,
        const irs::term_reader& term_reader,
        const irs::attribute_store& filter_node_attrs,
        const irs::attribute_view& document_attrs
      ): document_attrs_(document_attrs),
         filter_node_attrs_(filter_node_attrs),
         segment_reader_(segment_reader),
         sort_(sort),
         term_reader_(term_reader) {
      }

     private:
      const irs::attribute_view& document_attrs_;
      const irs::attribute_store& filter_node_attrs_;
      const irs::sub_reader& segment_reader_;
      const custom_sort& sort_;
      const irs::term_reader& term_reader_;
    };

    DECLARE_FACTORY(prepared);

    prepared(const custom_sort& sort): sort_(sort) {
    }

    virtual const iresearch::flags& features() const override {
      return iresearch::flags::empty_instance();
    }

    virtual collector::ptr prepare_collector() const override {
      if (sort_.prepare_collector) {
        return sort_.prepare_collector();
      }

      return irs::sort::collector::make<custom_sort::prepared::collector>(sort_);
    }

    virtual scorer::ptr prepare_scorer(
        const irs::sub_reader& segment_reader,
        const irs::term_reader& term_reader,
        const irs::attribute_store& filter_node_attrs,
        const irs::attribute_view& document_attrs
    ) const override {
      if (sort_.prepare_scorer) {
        return sort_.prepare_scorer(
          segment_reader, term_reader, filter_node_attrs, document_attrs
        );
      }

      return sort::scorer::make<custom_sort::prepared::scorer>(
        sort_, segment_reader, term_reader, filter_node_attrs, document_attrs
      );
    }

    virtual void prepare_score(irs::byte_type* score) const override {
      score_cast(score) = irs::type_limits<irs::type_t::doc_id_t>::invalid();
    }

    virtual void add(irs::byte_type* dst, irs::byte_type const* src) const override {
      if (sort_.scorer_add) {
        sort_.scorer_add(score_cast(dst), score_cast(src));
      }
    }

    virtual bool less(irs::byte_type const* lhs, const irs::byte_type* rhs) const override {
      return sort_.scorer_less ? sort_.scorer_less(score_cast(lhs), score_cast(rhs)) : false;
    }

   private:
    const custom_sort& sort_;
  };

  std::function<void(const irs::sub_reader&, const irs::term_reader&, const irs::attribute_view&)> collector_collect;
  std::function<void(irs::attribute_store&, const irs::index_reader&)> collector_finish;
  std::function<collector::ptr()> prepare_collector;
  std::function<scorer::ptr(const irs::sub_reader&, const irs::term_reader&, const irs::attribute_store&, const irs::attribute_view&)> prepare_scorer;
  std::function<void(irs::doc_id_t&, const irs::doc_id_t&)> scorer_add;
  std::function<bool(const irs::doc_id_t&, const irs::doc_id_t&)> scorer_less;
  std::function<void(irs::doc_id_t&)> scorer_score;

  DECLARE_FACTORY();
  custom_sort(): sort(custom_sort::type()) {}
  virtual prepared::ptr prepare() const override {
    return std::make_unique<custom_sort::prepared>(*this);
  }
};

DEFINE_SORT_TYPE(custom_sort);
DEFINE_FACTORY_DEFAULT(custom_sort);

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

struct IResearchExpressionFilterSetup {
  StorageEngineMock engine;
  arangodb::application_features::ApplicationServer server;
  std::unique_ptr<TRI_vocbase_t> system;
  std::vector<std::pair<arangodb::application_features::ApplicationFeature*, bool>> features;

  IResearchExpressionFilterSetup(): engine(server), server(nullptr, nullptr) {
    arangodb::EngineSelectorFeature::ENGINE = &engine;
    arangodb::aql::AqlFunctionFeature* functions = nullptr;

    arangodb::tests::init(true);

    // suppress INFO {authentication} Authentication is turned on (system only), authentication for unix sockets is turned on
    arangodb::LogTopic::setLogLevel(arangodb::Logger::AUTHENTICATION.name(), arangodb::LogLevel::WARN);

    // suppress log messages since tests check error conditions
    arangodb::LogTopic::setLogLevel(arangodb::iresearch::TOPIC.name(), arangodb::LogLevel::FATAL);
    irs::logger::output_le(iresearch::logger::IRL_FATAL, stderr);

    // setup required application features
    features.emplace_back(new arangodb::ViewTypesFeature(server), true);
    features.emplace_back(new arangodb::AuthenticationFeature(server), true);
    features.emplace_back(new arangodb::DatabasePathFeature(server), false);
    features.emplace_back(new arangodb::DatabaseFeature(server), false);
    features.emplace_back(new arangodb::QueryRegistryFeature(server), false); // must be first
    arangodb::application_features::ApplicationServer::server->addFeature(features.back().first); // need QueryRegistryFeature feature to be added now in order to create the system database
    system = irs::memory::make_unique<TRI_vocbase_t>(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 0, TRI_VOC_SYSTEM_DATABASE);
    features.emplace_back(new arangodb::SystemDatabaseFeature(server, system.get()), false); // required for IResearchAnalyzerFeature
    features.emplace_back(new arangodb::TraverserEngineRegistryFeature(server), false); // must be before AqlFeature
    features.emplace_back(new arangodb::AqlFeature(server), true);
    features.emplace_back(new arangodb::ShardingFeature(server), false);
    features.emplace_back(new arangodb::aql::OptimizerRulesFeature(server), true);
    features.emplace_back(functions = new arangodb::aql::AqlFunctionFeature(server), true); // required for IResearchAnalyzerFeature
    features.emplace_back(new arangodb::iresearch::IResearchAnalyzerFeature(server), true);
    features.emplace_back(new arangodb::iresearch::IResearchFeature(server), true);

    #if USE_ENTERPRISE
      features.emplace_back(new arangodb::LdapFeature(server), false); // required for AuthenticationFeature with USE_ENTERPRISE
    #endif

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

    // register fake non-deterministic function in order to suppress optimizations
    functions->add(arangodb::aql::Function{
      "_REFERENCE_",
      ".",
      arangodb::aql::Function::makeFlags(
        // fake non-deterministic
        arangodb::aql::Function::Flags::CanRunOnDBServer
      ),
      [](arangodb::aql::ExpressionContext*, arangodb::transaction::Methods*, arangodb::aql::VPackFunctionParameters const& params) {
        TRI_ASSERT(!params.empty());
        return params[0];
    }});

    auto* analyzers = arangodb::application_features::ApplicationServer::lookupFeature<
      arangodb::iresearch::IResearchAnalyzerFeature
    >();

    analyzers->emplace("test_analyzer", "TestAnalyzer", "abc"); // cache analyzer
    analyzers->emplace("test_csv_analyzer", "TestDelimAnalyzer", ","); // cache analyzer

    auto* dbPathFeature = arangodb::application_features::ApplicationServer::getFeature<arangodb::DatabasePathFeature>("DatabasePath");
    arangodb::tests::setDatabasePath(*dbPathFeature); // ensure test data is stored in a unique directory
  }

  ~IResearchExpressionFilterSetup() {
    system.reset(); // destroy before reseting the 'ENGINE'
    arangodb::AqlFeature(server).stop(); // unset singleton instance
    arangodb::LogTopic::setLogLevel(arangodb::iresearch::TOPIC.name(), arangodb::LogLevel::DEFAULT);
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
}; // TestSetup

NS_END

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

TEST_CASE("IResearchExpressionFilterTest", "[iresearch][iresearch-expression-filter]") {
  IResearchExpressionFilterSetup setup;
  UNUSED(setup);

  arangodb::velocypack::Builder testData;
  {
    irs::utf8_path resource;
    resource/=irs::string_ref(IResearch_test_resource_dir);
    resource/=irs::string_ref("simple_sequential.json");

    testData = arangodb::basics::VelocyPackHelper::velocyPackFromFile(resource.utf8());
  }
  auto testDataRoot = testData.slice();
  REQUIRE(testDataRoot.isArray());

  irs::memory_directory dir;

  // populate directory with data
  {
    struct {
      bool write(irs::data_output& out) {
        irs::write_string(out, str);
        return true;
      }

      irs::string_ref name() const {
        return "name";
      }

      irs::string_ref str;
    } storedField;

    auto writer = irs::index_writer::make(
      dir, irs::formats::get("1_0"), irs::OM_CREATE
    );
    REQUIRE(writer);

    for (auto data : arangodb::velocypack::ArrayIterator(testDataRoot)) {
      storedField.str = arangodb::iresearch::getStringRef(data.get("name"));

      auto ctx = writer->documents();
      auto doc = ctx.insert();
      CHECK(doc.insert(irs::action::store, storedField));
      CHECK(doc);
    }

    writer->commit();
  }


  // setup ArangoDB database
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");

  // create view
  {
    auto createJson = arangodb::velocypack::Parser::fromJson("{ \
      \"name\": \"testView\", \
      \"type\": \"arangosearch\" \
    }");

    // add view
    auto view = std::dynamic_pointer_cast<arangodb::iresearch::IResearchView>(
      vocbase.createView(createJson->slice())
    );
    REQUIRE((false == !view));
  }

  // open reader
  auto reader = irs::directory_reader::open(dir);
  REQUIRE(reader);
  REQUIRE(1 == reader->size());
  auto& segment = (*reader)[0];
  CHECK(reader->docs_count() > 0);

  // uninitialized query
  {
    arangodb::iresearch::ByExpression filter;
    CHECK(!filter);

    auto prepared = filter.prepare(*reader);
    auto docs = prepared->execute(segment);
    CHECK(irs::type_limits<irs::type_t::doc_id_t>::eof() == docs->value());
    CHECK(!docs->next());
    CHECK(irs::type_limits<irs::type_t::doc_id_t>::eof() == docs->value());
  }

  // query with false expression without order
  {
    std::shared_ptr<arangodb::velocypack::Builder> bindVars;
    auto options = std::make_shared<arangodb::velocypack::Builder>();
    std::string const queryString = "LET c=1 LET b=2 FOR d IN testView FILTER c==b RETURN d";

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

    // find expression root
    auto* expression = filterNode->getMember(0);
    REQUIRE(expression);

    // setup filter
    std::vector<std::string> EMPTY;
    arangodb::transaction::Methods trx(
      arangodb::transaction::StandaloneContext::Create(vocbase),
      EMPTY,
      EMPTY,
      EMPTY,
      arangodb::transaction::Options()
    );
    std::unique_ptr<arangodb::aql::ExecutionPlan> plan(arangodb::aql::ExecutionPlan::instantiateFromAst(ast));

    arangodb::iresearch::ByExpression filter;
    CHECK(!filter);
    filter.init(*plan, *ast, *expression);
    CHECK(filter);

    arangodb::iresearch::ExpressionExecutionContext execCtx;
    execCtx.trx = &trx;
    execCtx.ctx = &ctx;
    irs::attribute_view queryCtx;
    queryCtx.emplace(execCtx);

    auto prepared = filter.prepare(*reader, irs::order::prepared::unordered(), queryCtx);
    auto docs = prepared->execute(segment, irs::order::prepared::unordered(), queryCtx);
    CHECK(irs::type_limits<irs::type_t::doc_id_t>::eof() == docs->value());
    CHECK(!docs->next());
    CHECK(irs::type_limits<irs::type_t::doc_id_t>::eof() == docs->value());
  }

  // query with false expression without order (deferred execution)
  {
    std::shared_ptr<arangodb::velocypack::Builder> bindVars;
    auto options = std::make_shared<arangodb::velocypack::Builder>();
    std::string const queryString = "LET c=1 LET b=2 FOR d IN testView FILTER c==b RETURN d";

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

    // find expression root
    auto* expression = filterNode->getMember(0);
    REQUIRE(expression);

    // setup filter
    std::vector<std::string> EMPTY;
    arangodb::transaction::Methods trx(
      arangodb::transaction::StandaloneContext::Create(vocbase),
      EMPTY,
      EMPTY,
      EMPTY,
      arangodb::transaction::Options()
    );
    std::unique_ptr<arangodb::aql::ExecutionPlan> plan(arangodb::aql::ExecutionPlan::instantiateFromAst(ast));

    arangodb::iresearch::ByExpression filter;
    CHECK(!filter);
    filter.init(*plan, *ast, *expression);
    CHECK(filter);

    arangodb::iresearch::ExpressionExecutionContext execCtx;
    execCtx.trx = &trx;
    execCtx.ctx = &ctx;
    irs::attribute_view queryCtx;
    queryCtx.emplace(execCtx);

    auto prepared = filter.prepare(*reader, irs::order::prepared::unordered());
    auto docs = prepared->execute(segment, irs::order::prepared::unordered(), queryCtx);
    CHECK(irs::type_limits<irs::type_t::doc_id_t>::eof() == docs->value());
    CHECK(!docs->next());
    CHECK(irs::type_limits<irs::type_t::doc_id_t>::eof() == docs->value());
  }

  // query with true expression without order
  {
    std::shared_ptr<arangodb::velocypack::Builder> bindVars;
    auto options = std::make_shared<arangodb::velocypack::Builder>();
    std::string const queryString = "LET c=1 LET b=2 FOR d IN testView FILTER c<b RETURN d";

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

    // find expression root
    auto* expression = filterNode->getMember(0);
    REQUIRE(expression);

    // setup filter
    std::vector<std::string> EMPTY;
    arangodb::transaction::Methods trx(
      arangodb::transaction::StandaloneContext::Create(vocbase),
      EMPTY,
      EMPTY,
      EMPTY,
      arangodb::transaction::Options()
    );
    std::unique_ptr<arangodb::aql::ExecutionPlan> plan(arangodb::aql::ExecutionPlan::instantiateFromAst(ast));

    arangodb::iresearch::ByExpression filter;
    CHECK(!filter);
    filter.init(*plan, *ast, *expression);
    CHECK(filter);

    arangodb::iresearch::ExpressionExecutionContext execCtx;
    execCtx.trx = &trx;
    execCtx.ctx = &ctx;
    irs::attribute_view queryCtx;
    queryCtx.emplace(execCtx);

    auto prepared = filter.prepare(*reader, irs::order::prepared::unordered(), queryCtx);
    CHECK(!prepared->attributes().get<irs::boost>()); // no boost set
    CHECK(typeid(prepared.get()) == typeid(irs::all().prepare(*reader).get())); // should be same type
    auto column = segment.column_reader("name");
    REQUIRE(column);
    auto columnValues = column->values();
    REQUIRE(columnValues);
    auto docs = prepared->execute(segment, irs::order::prepared::unordered(), queryCtx);
    CHECK(irs::type_limits<irs::type_t::doc_id_t>::invalid() == docs->value());
    auto& cost = docs->attributes().get<irs::cost>();
    REQUIRE(cost);
    CHECK(arangodb::velocypack::ArrayIterator(testDataRoot).size() == cost->estimate());

    irs::bytes_ref value;
    for (auto doc : arangodb::velocypack::ArrayIterator(testDataRoot)) {
      CHECK(docs->next());
      CHECK(columnValues(docs->value(), value));
      CHECK(arangodb::iresearch::getStringRef(doc.get("name")) == irs::to_string<irs::string_ref>(value.c_str()));
    }
    CHECK(!docs->next());
    CHECK(irs::type_limits<irs::type_t::doc_id_t>::eof() == docs->value());
  }

  // query with true expression without order (deferred execution)
  {
    std::shared_ptr<arangodb::velocypack::Builder> bindVars;
    auto options = std::make_shared<arangodb::velocypack::Builder>();
    std::string const queryString = "LET c=1 LET b=2 FOR d IN testView FILTER c<b RETURN d";

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

    // find expression root
    auto* expression = filterNode->getMember(0);
    REQUIRE(expression);

    // setup filter
    std::vector<std::string> EMPTY;
    arangodb::transaction::Methods trx(
      arangodb::transaction::StandaloneContext::Create(vocbase),
      EMPTY,
      EMPTY,
      EMPTY,
      arangodb::transaction::Options()
    );
    std::unique_ptr<arangodb::aql::ExecutionPlan> plan(arangodb::aql::ExecutionPlan::instantiateFromAst(ast));

    arangodb::iresearch::ByExpression filter;
    CHECK(!filter);
    filter.init(*plan, *ast, *expression);
    CHECK(filter);

    arangodb::iresearch::ExpressionExecutionContext execCtx;
    execCtx.trx = &trx;
    execCtx.ctx = &ctx;
    irs::attribute_view queryCtx;
    queryCtx.emplace(execCtx);

    auto prepared = filter.prepare(*reader, irs::order::prepared::unordered()); // no context provided
    CHECK(!prepared->attributes().get<irs::boost>()); // no boost set
    CHECK(typeid(prepared.get()) == typeid(irs::all().prepare(*reader).get())); // should be same type
    auto column = segment.column_reader("name");
    REQUIRE(column);
    auto columnValues = column->values();
    REQUIRE(columnValues);
    auto docs = prepared->execute(segment, irs::order::prepared::unordered(), queryCtx);
    CHECK(irs::type_limits<irs::type_t::doc_id_t>::invalid() == docs->value());
    auto& cost = docs->attributes().get<irs::cost>();
    REQUIRE(cost);
    CHECK(arangodb::velocypack::ArrayIterator(testDataRoot).size() == cost->estimate());

    irs::bytes_ref value;
    for (auto doc : arangodb::velocypack::ArrayIterator(testDataRoot)) {
      CHECK(docs->next());
      CHECK(columnValues(docs->value(), value));
      CHECK(arangodb::iresearch::getStringRef(doc.get("name")) == irs::to_string<irs::string_ref>(value.c_str()));
    }
    CHECK(!docs->next());
    CHECK(irs::type_limits<irs::type_t::doc_id_t>::eof() == docs->value());
  }

  // query with true expression without order (deferred execution)
  {
    std::shared_ptr<arangodb::velocypack::Builder> bindVars;
    auto options = std::make_shared<arangodb::velocypack::Builder>();
    std::string const queryString = "LET c=1 LET b=2 FOR d IN testView FILTER c<b RETURN d";

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

    // find expression root
    auto* expression = filterNode->getMember(0);
    REQUIRE(expression);

    // setup filter
    std::vector<std::string> EMPTY;
    arangodb::transaction::Methods trx(
      arangodb::transaction::StandaloneContext::Create(vocbase),
      EMPTY,
      EMPTY,
      EMPTY,
      arangodb::transaction::Options()
    );
    std::unique_ptr<arangodb::aql::ExecutionPlan> plan(arangodb::aql::ExecutionPlan::instantiateFromAst(ast));

    arangodb::iresearch::ByExpression filter;
    CHECK(!filter);
    filter.init(*plan, *ast, *expression);
    CHECK(filter);

    arangodb::iresearch::ExpressionExecutionContext execCtx;
    execCtx.trx = &trx;
    execCtx.ctx = nullptr;
    irs::attribute_view queryCtx;
    queryCtx.emplace(execCtx);

    auto prepared = filter.prepare(*reader, irs::order::prepared::unordered(), queryCtx); // invalid context provided
    CHECK(!prepared->attributes().get<irs::boost>()); // no boost set
    auto column = segment.column_reader("name");
    REQUIRE(column);
    auto columnValues = column->values();
    REQUIRE(columnValues);
    execCtx.ctx = &ctx; // fix context
    auto docs = prepared->execute(segment, irs::order::prepared::unordered(), queryCtx);
    CHECK(irs::type_limits<irs::type_t::doc_id_t>::invalid() == docs->value());
    auto& cost = docs->attributes().get<irs::cost>();
    REQUIRE(cost);
    CHECK(arangodb::velocypack::ArrayIterator(testDataRoot).size() == cost->estimate());

    irs::bytes_ref value;
    for (auto doc : arangodb::velocypack::ArrayIterator(testDataRoot)) {
      CHECK(docs->next());
      CHECK(columnValues(docs->value(), value));
      CHECK(arangodb::iresearch::getStringRef(doc.get("name")) == irs::to_string<irs::string_ref>(value.c_str()));
    }
    CHECK(!docs->next());
    CHECK(irs::type_limits<irs::type_t::doc_id_t>::eof() == docs->value());
  }

  // query with true expression without order (deferred execution with invalid context)
  {
    std::shared_ptr<arangodb::velocypack::Builder> bindVars;
    auto options = std::make_shared<arangodb::velocypack::Builder>();
    std::string const queryString = "LET c=1 LET b=2 FOR d IN testView FILTER c<b RETURN d";

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

    // find expression root
    auto* expression = filterNode->getMember(0);
    REQUIRE(expression);

    // setup filter
    std::vector<std::string> EMPTY;
    arangodb::transaction::Methods trx(
      arangodb::transaction::StandaloneContext::Create(vocbase),
      EMPTY,
      EMPTY,
      EMPTY,
      arangodb::transaction::Options()
    );
    std::unique_ptr<arangodb::aql::ExecutionPlan> plan(arangodb::aql::ExecutionPlan::instantiateFromAst(ast));

    arangodb::iresearch::ByExpression filter;
    CHECK(!filter);
    filter.init(*plan, *ast, *expression);
    CHECK(filter);

    arangodb::iresearch::ExpressionExecutionContext execCtx;
    execCtx.trx = &trx;
    execCtx.ctx = nullptr;
    irs::attribute_view queryCtx;
    queryCtx.emplace(execCtx);

    auto prepared = filter.prepare(*reader, irs::order::prepared::unordered()); // no context provided
    CHECK(!prepared->attributes().get<irs::boost>()); // no boost set
    auto docs = prepared->execute(segment, irs::order::prepared::unordered(), queryCtx);
    CHECK(irs::type_limits<irs::type_t::doc_id_t>::eof(docs->value()));
    CHECK(!docs->next());
  }

  // query with true expression without order (deferred execution with invalid context)
  {
    std::shared_ptr<arangodb::velocypack::Builder> bindVars;
    auto options = std::make_shared<arangodb::velocypack::Builder>();
    std::string const queryString = "LET c=1 LET b=2 FOR d IN testView FILTER c<b RETURN d";

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

    // find expression root
    auto* expression = filterNode->getMember(0);
    REQUIRE(expression);

    // setup filter
    std::vector<std::string> EMPTY;
    arangodb::transaction::Methods trx(
      arangodb::transaction::StandaloneContext::Create(vocbase),
      EMPTY,
      EMPTY,
      EMPTY,
      arangodb::transaction::Options()
    );
    std::unique_ptr<arangodb::aql::ExecutionPlan> plan(arangodb::aql::ExecutionPlan::instantiateFromAst(ast));

    arangodb::iresearch::ByExpression filter;
    CHECK(!filter);
    filter.init(*plan, *ast, *expression);
    CHECK(filter);

    arangodb::iresearch::ExpressionExecutionContext execCtx;
    execCtx.trx = nullptr;
    execCtx.ctx = &ctx;
    irs::attribute_view queryCtx;
    queryCtx.emplace(execCtx);

    auto prepared = filter.prepare(*reader, irs::order::prepared::unordered()); // no context provided
    CHECK(!prepared->attributes().get<irs::boost>()); // no boost set
    auto docs = prepared->execute(segment, irs::order::prepared::unordered(), queryCtx);
    CHECK(irs::type_limits<irs::type_t::doc_id_t>::eof(docs->value()));
    CHECK(!docs->next());
  }

  // query with nondeterministic expression without order
  {
    std::shared_ptr<arangodb::velocypack::Builder> bindVars;
    auto options = std::make_shared<arangodb::velocypack::Builder>();
    std::string const queryString = "LET c=1 LET b=2 FOR d IN testView FILTER _REFERENCE_(c)==_REFERENCE_(b) RETURN d";

    ExpressionContextMock ctx;
    {
      arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt{2});
      arangodb::aql::AqlValueGuard guard(value, true);
      ctx.vars.emplace("b", value);
    }

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

    // find expression root
    auto* expression = filterNode->getMember(0);
    REQUIRE(expression);

    // setup filter
    std::vector<std::string> EMPTY;
    arangodb::transaction::Methods trx(
      arangodb::transaction::StandaloneContext::Create(vocbase),
      EMPTY,
      EMPTY,
      EMPTY,
      arangodb::transaction::Options()
    );
    std::unique_ptr<arangodb::aql::ExecutionPlan> plan(arangodb::aql::ExecutionPlan::instantiateFromAst(ast));

    arangodb::iresearch::ByExpression filter;
    CHECK(!filter);
    filter.init(*plan, *ast, *expression);
    CHECK(filter);

    arangodb::iresearch::ExpressionExecutionContext execCtx;
    execCtx.trx = &trx;
    execCtx.ctx = &ctx;
    irs::attribute_view queryCtx;
    queryCtx.emplace(execCtx);

    auto prepared = filter.prepare(*reader, irs::order::prepared::unordered(), queryCtx);
    auto column = segment.column_reader("name");
    REQUIRE(column);
    auto columnValues = column->values();
    REQUIRE(columnValues);
    auto docs = prepared->execute(segment, irs::order::prepared::unordered(), queryCtx);
    CHECK(irs::type_limits<irs::type_t::doc_id_t>::invalid() == docs->value());
    CHECK(!docs->attributes().get<irs::score>());

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
      REQUIRE(it.valid());
      auto doc = *it;
      CHECK(docs->next());
      CHECK(columnValues(docs->value(), keyValue));
      CHECK(arangodb::iresearch::getStringRef(doc.get("name")) == irs::to_string<irs::string_ref>(keyValue.c_str()));
      it.next();
    }

    CHECK(it.valid());

    // set unreachable filter condition
    {
      ctx.vars.erase("c");
      arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt{1});
      arangodb::aql::AqlValueGuard guard(value, true);
      ctx.vars.emplace("c", value);
    }
    CHECK(!docs->next());
    CHECK(irs::type_limits<irs::type_t::doc_id_t>::eof() == docs->value());
  }

  // query with nondeterministic expression and custom order
  {
    irs::order order;
    size_t collector_collect_count = 0;
    size_t collector_finish_count = 0;
    size_t scorer_score_count = 0;
    auto& sort = order.add<custom_sort>(false);

    sort.collector_collect = [&collector_collect_count](const irs::sub_reader&, const irs::term_reader&, const irs::attribute_view&)->void {
      ++collector_collect_count;
    };
    sort.collector_finish = [&collector_finish_count](irs::attribute_store&, const irs::index_reader&)->void {
      ++collector_finish_count;
    };
    sort.scorer_add = [](irs::doc_id_t& dst, const irs::doc_id_t& src)->void {
      dst = src;
    };
    sort.scorer_less = [](const irs::doc_id_t& lhs, const irs::doc_id_t& rhs)->bool {
      return (lhs & 0xAAAAAAAAAAAAAAAA) < (rhs & 0xAAAAAAAAAAAAAAAA);
    };
    sort.scorer_score = [&scorer_score_count](irs::doc_id_t)->void {
      ++scorer_score_count;
    };
    auto preparedOrder = order.prepare();

    std::shared_ptr<arangodb::velocypack::Builder> bindVars;
    auto options = std::make_shared<arangodb::velocypack::Builder>();
    std::string const queryString = "LET c=1 LET b=2 FOR d IN testView FILTER _REFERENCE_(c)==_REFERENCE_(b) RETURN d";

    ExpressionContextMock ctx;
    {
      arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt{2});
      arangodb::aql::AqlValueGuard guard(value, true);
      ctx.vars.emplace("b", value);
    }

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

    // find expression root
    auto* expression = filterNode->getMember(0);
    REQUIRE(expression);

    // setup filter
    std::vector<std::string> EMPTY;
    arangodb::transaction::Methods trx(
      arangodb::transaction::StandaloneContext::Create(vocbase),
      EMPTY,
      EMPTY,
      EMPTY,
      arangodb::transaction::Options()
    );
    std::unique_ptr<arangodb::aql::ExecutionPlan> plan(arangodb::aql::ExecutionPlan::instantiateFromAst(ast));

    arangodb::iresearch::ByExpression filter;
    CHECK(!filter);
    filter.init(*plan, *ast, *expression);
    CHECK(filter);

    arangodb::iresearch::ExpressionExecutionContext execCtx;
    execCtx.trx = &trx;
    execCtx.ctx = &ctx;
    irs::attribute_view queryCtx;
    queryCtx.emplace(execCtx);

    filter.boost(1.5f);
    CHECK(1.5f == filter.boost());

    auto prepared = filter.prepare(*reader, preparedOrder, queryCtx);
    auto const& boost = prepared->attributes().get<irs::boost>();
    CHECK(boost);
    CHECK(1.5f == boost->value);

    auto column = segment.column_reader("name");
    REQUIRE(column);
    auto columnValues = column->values();
    REQUIRE(columnValues);
    auto docs = prepared->execute(segment, preparedOrder, queryCtx);
    CHECK(irs::type_limits<irs::type_t::doc_id_t>::invalid() == docs->value());
    auto& score = docs->attributes().get<irs::score>();
    CHECK(score);
    CHECK(!score->empty());
    auto& cost = docs->attributes().get<irs::cost>();
    REQUIRE(cost);
    CHECK(arangodb::velocypack::ArrayIterator(testDataRoot).size() == cost->estimate());

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
      REQUIRE(it.valid());
      auto doc = *it;
      CHECK(docs->next());
      score->evaluate();
      CHECK(columnValues(docs->value(), keyValue));
      CHECK(arangodb::iresearch::getStringRef(doc.get("name")) == irs::to_string<irs::string_ref>(keyValue.c_str()));
      it.next();
    }

    CHECK(it.valid());

    // set unreachable filter condition
    {
      ctx.vars.erase("c");
      arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt{1});
      arangodb::aql::AqlValueGuard guard(value, true);
      ctx.vars.emplace("c", value);
    }
    CHECK(!docs->next());
    CHECK(irs::type_limits<irs::type_t::doc_id_t>::eof() == docs->value());

    // check order
    CHECK(0 == collector_collect_count); // should not be executed
    CHECK(1 == collector_finish_count);
    CHECK(it.size() / 2 == scorer_score_count);
  }

  // query with nondeterministic expression without order, seek + next
  {
    std::shared_ptr<arangodb::velocypack::Builder> bindVars;
    auto options = std::make_shared<arangodb::velocypack::Builder>();
    std::string const queryString = "LET c=1 LET b=2 FOR d IN testView FILTER _REFERENCE_(c)==_REFERENCE_(b) RETURN d";

    ExpressionContextMock ctx;
    {
      arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt{2});
      arangodb::aql::AqlValueGuard guard(value, true);
      ctx.vars.emplace("b", value);
    }

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

    // find expression root
    auto* expression = filterNode->getMember(0);
    REQUIRE(expression);

    // setup filter
    std::vector<std::string> EMPTY;
    arangodb::transaction::Methods trx(
      arangodb::transaction::StandaloneContext::Create(vocbase),
      EMPTY,
      EMPTY,
      EMPTY,
      arangodb::transaction::Options()
    );
    std::unique_ptr<arangodb::aql::ExecutionPlan> plan(arangodb::aql::ExecutionPlan::instantiateFromAst(ast));

    arangodb::iresearch::ByExpression filter;
    CHECK(!filter);
    filter.init(*plan, *ast, *expression);
    CHECK(filter);

    arangodb::iresearch::ExpressionExecutionContext execCtx;
    execCtx.trx = &trx;
    execCtx.ctx = &ctx;
    irs::attribute_view queryCtx;
    queryCtx.emplace(execCtx);

    auto prepared = filter.prepare(*reader, irs::order::prepared::unordered(), queryCtx);
    auto column = segment.column_reader("name");
    REQUIRE(column);
    auto columnValues = column->values();
    REQUIRE(columnValues);
    auto docs = prepared->execute(segment, irs::order::prepared::unordered(), queryCtx);
    CHECK(irs::type_limits<irs::type_t::doc_id_t>::invalid() == docs->value());
    CHECK(!docs->attributes().get<irs::score>());
    auto& cost = docs->attributes().get<irs::cost>();
    REQUIRE(cost);
    CHECK(arangodb::velocypack::ArrayIterator(testDataRoot).size() == cost->estimate());

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
      REQUIRE(it.valid());
    }
    CHECK(seek_to == docs->seek(seek_to));

    for (size_t i = seek_to; i < it.size() / 2; ++i) {
      REQUIRE(it.valid());
      auto doc = *it;
      CHECK(docs->next());
      CHECK(columnValues(docs->value(), keyValue));
      CHECK(arangodb::iresearch::getStringRef(doc.get("name")) == irs::to_string<irs::string_ref>(keyValue.c_str()));
      it.next();
    }

    CHECK(it.valid());

    // set unreachable filter condition
    {
      ctx.vars.erase("c");
      arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt{1});
      arangodb::aql::AqlValueGuard guard(value, true);
      ctx.vars.emplace("c", value);
    }
    CHECK(!docs->next());
    CHECK(irs::type_limits<irs::type_t::doc_id_t>::eof() == docs->value());
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
