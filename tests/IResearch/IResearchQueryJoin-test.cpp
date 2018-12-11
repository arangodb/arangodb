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

#include "Aql/Ast.h"
#include "Aql/ExpressionContext.h"
#include "Aql/OptimizerRulesFeature.h"
#include "Aql/Query.h"
#include "V8/v8-globals.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/LogicalView.h"
#include "Transaction/Methods.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/OperationOptions.h"
#include "Aql/AqlFunctionFeature.h"
#include "Aql/OptimizerRulesFeature.h"
#include "Aql/ExecutionPlan.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "IResearch/ApplicationServerHelper.h"
#include "IResearch/IResearchCommon.h"
#include "IResearch/IResearchFeature.h"
#include "IResearch/IResearchFilterFactory.h"
#include "IResearch/IResearchView.h"
#include "IResearch/IResearchAnalyzerFeature.h"
#include "Logger/Logger.h"
#include "Logger/LogTopic.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "RestServer/DatabasePathFeature.h"
#include "RestServer/ViewTypesFeature.h"
#include "RestServer/AqlFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "RestServer/SystemDatabaseFeature.h"
#include "RestServer/TraverserEngineRegistryFeature.h"
#include "Sharding/ShardingFeature.h"
#include "Basics/VelocyPackHelper.h"
#include "3rdParty/iresearch/tests/tests_config.hpp"
#include "VocBase/ManagedDocumentResult.h"

#include "IResearch/VelocyPackHelper.h"
#include "analysis/analyzers.hpp"
#include "analysis/token_attributes.hpp"
#include "search/scorers.hpp"
#include "utils/utf8_path.hpp"

#include <velocypack/Iterator.h>

extern const char* ARGV0; // defined in main.cpp

NS_LOCAL

struct CustomScorer : public irs::sort {
  struct Prepared: public irs::sort::prepared_base<float_t> {
   public:
    DECLARE_FACTORY(Prepared);

    Prepared(float_t i)
      : i(i) {
    }

    virtual void add(irs::byte_type* dst, const irs::byte_type* src) const override {
      score_cast(dst) += score_cast(src);
    }

    virtual irs::flags const& features() const override {
      return irs::flags::empty_instance();
    }

    virtual bool less(const irs::byte_type* lhs, const irs::byte_type* rhs) const override {
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
      irs::attribute_store const&,
      irs::attribute_view const&
    ) const override {
      struct Scorer : public irs::sort::scorer {
        Scorer(float_t score): i(score) { }

        virtual void score(irs::byte_type* score_buf) override {
          *reinterpret_cast<score_t*>(score_buf) = i;
        }

        float_t i;
      };

      return irs::sort::scorer::make<Scorer>(i);
    }

    float_t i;
  };

  static ::iresearch::sort::type_id const& type() {
    static ::iresearch::sort::type_id TYPE("customscorer");
    return TYPE;
  }

  static irs::sort::ptr make(irs::string_ref const& args) {
    if (args.null()) {
      return std::make_shared<CustomScorer>(0.f);
    }

    // velocypack::Parser::fromJson(...) will throw exception on parse error
    auto json = arangodb::velocypack::Parser::fromJson(args.c_str(), args.size());
    auto slice = json ? json->slice() : arangodb::velocypack::Slice();

    if (!slice.isArray()) {
      return nullptr; // incorrect argument format
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
}; // CustomScorer

REGISTER_SCORER_JSON(CustomScorer, CustomScorer::make);

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

struct IResearchQueryJoinSetup {
  StorageEngineMock engine;
  arangodb::application_features::ApplicationServer server;
  std::unique_ptr<TRI_vocbase_t> system;
  std::vector<std::pair<arangodb::application_features::ApplicationFeature*, bool>> features;

  IResearchQueryJoinSetup(): engine(server), server(nullptr, nullptr) {
    arangodb::EngineSelectorFeature::ENGINE = &engine;
    arangodb::aql::AqlFunctionFeature* functions = nullptr;

    arangodb::tests::init(true);

    // suppress INFO {authentication} Authentication is turned on (system only), authentication for unix sockets is turned on
    arangodb::LogTopic::setLogLevel(arangodb::Logger::AUTHENTICATION.name(), arangodb::LogLevel::WARN);

    // suppress log messages since tests check error conditions
    arangodb::LogTopic::setLogLevel(arangodb::Logger::FIXME.name(), arangodb::LogLevel::ERR); // suppress WARNING DefaultCustomTypeHandler called
    arangodb::LogTopic::setLogLevel(arangodb::iresearch::TOPIC.name(), arangodb::LogLevel::FATAL);
    irs::logger::output_le(iresearch::logger::IRL_FATAL, stderr);

    // setup required application features
    features.emplace_back(new arangodb::ViewTypesFeature(server), true);
    features.emplace_back(new arangodb::AuthenticationFeature(server), true);
    features.emplace_back(new arangodb::DatabasePathFeature(server), false);
    features.emplace_back(new arangodb::DatabaseFeature(server), false);
    features.emplace_back(new arangodb::ShardingFeature(server), false);
    features.emplace_back(new arangodb::QueryRegistryFeature(server), false); // must be first
    arangodb::application_features::ApplicationServer::server->addFeature(features.back().first); // need QueryRegistryFeature feature to be added now in order to create the system database
    system = irs::memory::make_unique<TRI_vocbase_t>(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 0, TRI_VOC_SYSTEM_DATABASE);
    features.emplace_back(new arangodb::SystemDatabaseFeature(server, system.get()), false); // required for IResearchAnalyzerFeature
    features.emplace_back(new arangodb::TraverserEngineRegistryFeature(server), false); // must be before AqlFeature
    features.emplace_back(new arangodb::AqlFeature(server), true);
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
      "_NONDETERM_",
      ".",
      arangodb::aql::Function::makeFlags(
        // fake non-deterministic
        arangodb::aql::Function::Flags::CanRunOnDBServer
      ), 
      [](arangodb::aql::ExpressionContext*, arangodb::transaction::Methods*, arangodb::aql::VPackFunctionParameters const& params) {
        TRI_ASSERT(!params.empty());
        return params[0];
    }});

    // register fake non-deterministic function in order to suppress optimizations
    functions->add(arangodb::aql::Function{
      "_FORWARD_",
      ".",
      arangodb::aql::Function::makeFlags(
        // fake deterministic
        arangodb::aql::Function::Flags::Deterministic,
        arangodb::aql::Function::Flags::Cacheable,
        arangodb::aql::Function::Flags::CanRunOnDBServer
      ), 
      [](arangodb::aql::ExpressionContext*, arangodb::transaction::Methods*, arangodb::aql::VPackFunctionParameters const& params) {
        TRI_ASSERT(!params.empty());
        return params[0];
    }});

    // external function names must be registred in upper-case
    // user defined functions have ':' in the external function name
    // function arguments string format: requiredArg1[,requiredArg2]...[|optionalArg1[,optionalArg2]...]
    arangodb::aql::Function customScorer("CUSTOMSCORER", ".|+", 
      arangodb::aql::Function::makeFlags(
        arangodb::aql::Function::Flags::Deterministic,
        arangodb::aql::Function::Flags::Cacheable,
        arangodb::aql::Function::Flags::CanRunOnDBServer
      ));
    arangodb::iresearch::addFunction(*arangodb::aql::AqlFunctionFeature::AQLFUNCTIONS, customScorer);

    auto* analyzers = arangodb::application_features::ApplicationServer::lookupFeature<
      arangodb::iresearch::IResearchAnalyzerFeature
    >();

    analyzers->emplace("test_analyzer", "TestAnalyzer", "abc"); // cache analyzer
    analyzers->emplace("test_csv_analyzer", "TestDelimAnalyzer", ","); // cache analyzer

    auto* dbPathFeature = arangodb::application_features::ApplicationServer::getFeature<arangodb::DatabasePathFeature>("DatabasePath");
    arangodb::tests::setDatabasePath(*dbPathFeature); // ensure test data is stored in a unique directory
  }

  ~IResearchQueryJoinSetup() {
    system.reset(); // destroy before reseting the 'ENGINE'
    arangodb::AqlFeature(server).stop(); // unset singleton instance
    arangodb::LogTopic::setLogLevel(arangodb::iresearch::TOPIC.name(), arangodb::LogLevel::DEFAULT);
    arangodb::LogTopic::setLogLevel(arangodb::Logger::FIXME.name(), arangodb::LogLevel::DEFAULT);
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
}; // IResearchQuerySetup

NS_END

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

TEST_CASE("IResearchQueryTestJoinVolatileBlock", "[iresearch][iresearch-query]") {
  // should not recreate iterator each loop iteration in case of deterministic/independent inner loop scope
}

TEST_CASE("IResearchQueryTestJoinDuplicateDataSource", "[iresearch][iresearch-query]") {
  IResearchQueryJoinSetup s;
  UNUSED(s);

  static std::vector<std::string> const EMPTY;

  auto createJson = arangodb::velocypack::Parser::fromJson("{ \
    \"name\": \"testView\", \
    \"type\": \"arangosearch\" \
  }");

  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
  std::shared_ptr<arangodb::LogicalCollection> logicalCollection1;
  std::shared_ptr<arangodb::LogicalCollection> logicalCollection2;
  std::shared_ptr<arangodb::LogicalCollection> logicalCollection3;

  // add collection_1
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"collection_1\" }");
    logicalCollection1 = vocbase.createCollection(collectionJson->slice());
    REQUIRE((nullptr != logicalCollection1));
  }

  // add collection_2
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"collection_2\" }");
    logicalCollection2 = vocbase.createCollection(collectionJson->slice());
    REQUIRE((nullptr != logicalCollection2));
  }

  // add collection_3
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"collection_3\" }");
    logicalCollection3 = vocbase.createCollection(collectionJson->slice());
    REQUIRE((nullptr != logicalCollection3));
  }

  // add view
  auto view = std::dynamic_pointer_cast<arangodb::iresearch::IResearchView>(
    vocbase.createView(createJson->slice())
  );
  REQUIRE((false == !view));

  // add logical collection with the same name as view
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\" }");
    // TRI_vocbase_t::createCollection(...) throws exception instead of returning a nullptr
    CHECK_THROWS(vocbase.createCollection(collectionJson->slice()));
  }

  // add link to collection
  {
    auto updateJson = arangodb::velocypack::Parser::fromJson(
      "{ \"links\": {"
      "\"collection_1\": { \"analyzers\": [ \"test_analyzer\", \"identity\" ], \"includeAllFields\": true, \"trackListPositions\": true },"
      "\"collection_2\": { \"analyzers\": [ \"test_analyzer\", \"identity\" ], \"includeAllFields\": true }"
      "}}"
    );
    CHECK((view->properties(updateJson->slice(), true).ok()));

    arangodb::velocypack::Builder builder;

    builder.openObject();
    view->properties(builder, true, false);
    builder.close();

    auto slice = builder.slice();
    CHECK(slice.isObject());
    CHECK(slice.get("name").copyString() == "testView");
    CHECK(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
    CHECK(slice.get("deleted").isNone()); // no system properties
    auto tmpSlice = slice.get("links");
    CHECK((true == tmpSlice.isObject() && 2 == tmpSlice.length()));
  }

  std::deque<arangodb::ManagedDocumentResult> insertedDocsView;

  // populate view with the data
  {
    arangodb::OperationOptions opt;
    TRI_voc_tick_t tick;

    arangodb::transaction::Methods trx(
      arangodb::transaction::StandaloneContext::Create(vocbase),
      EMPTY,
      EMPTY,
      EMPTY,
      arangodb::transaction::Options()
    );
    CHECK((trx.begin().ok()));

    // insert into collections
    {
      irs::utf8_path resource;
      resource/=irs::string_ref(IResearch_test_resource_dir);
      resource/=irs::string_ref("simple_sequential.json");

      auto builder = arangodb::basics::VelocyPackHelper::velocyPackFromFile(resource.utf8());
      auto root = builder.slice();
      REQUIRE(root.isArray());

      size_t i = 0;

      std::shared_ptr<arangodb::LogicalCollection> collections[] {
        logicalCollection1, logicalCollection2
      };

      for (auto doc : arangodb::velocypack::ArrayIterator(root)) {
        insertedDocsView.emplace_back();
        auto const res = collections[i % 2]->insert(&trx, doc, insertedDocsView.back(), opt, tick, false);
        CHECK(res.ok());
        ++i;
      }
    }

    // insert into collection_3
    std::deque<arangodb::ManagedDocumentResult> insertedDocsCollection;

    {
      irs::utf8_path resource;
      resource/=irs::string_ref(IResearch_test_resource_dir);
      resource/=irs::string_ref("simple_sequential_order.json");

      auto builder = arangodb::basics::VelocyPackHelper::velocyPackFromFile(resource.utf8());
      auto root = builder.slice();
      REQUIRE(root.isArray());

      for (auto doc : arangodb::velocypack::ArrayIterator(root)) {
        insertedDocsCollection.emplace_back();
        auto const res = logicalCollection3->insert(&trx, doc, insertedDocsCollection.back(), opt, tick, false);
        CHECK(res.ok());
      }
    }

    CHECK((trx.commit().ok()));
    CHECK(view->commit().ok());
  }

  // using search keyword for collection is prohibited
  {
    std::string const query = "LET c=5 FOR x IN collection_1 SEARCH x.seq == c RETURN x";
    auto const boundParameters = arangodb::velocypack::Parser::fromJson("{ }");

    // arangodb::aql::ExecutionPlan::fromNodeFor(...) throws TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND
    auto queryResult = arangodb::tests::executeQuery(vocbase, query, boundParameters);
    CHECK(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND == queryResult.code);
  }

  // using search keyword for bound collection is prohibited
  {
    std::string const query = "LET c=5 FOR x IN @@dataSource SEARCH x.seq == c  RETURN x";
    auto const boundParameters = arangodb::velocypack::Parser::fromJson("{ \"@dataSource\" : \"collection_1\" }");
    auto queryResult = arangodb::tests::executeQuery(vocbase, query, boundParameters);
    CHECK(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND == queryResult.code);
  }
}

TEST_CASE("IResearchQueryTestJoin", "[iresearch][iresearch-query]") {
  IResearchQueryJoinSetup s;
  UNUSED(s);

  static std::vector<std::string> const EMPTY;

  auto createJson = arangodb::velocypack::Parser::fromJson("{ \
    \"name\": \"testView\", \
    \"type\": \"arangosearch\" \
  }");

  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
  std::shared_ptr<arangodb::LogicalCollection> logicalCollection1;
  std::shared_ptr<arangodb::LogicalCollection> logicalCollection2;
  std::shared_ptr<arangodb::LogicalCollection> logicalCollection3;

  // add collection_1
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"collection_1\" }");
    logicalCollection1 = vocbase.createCollection(collectionJson->slice());
    REQUIRE((nullptr != logicalCollection1));
  }

  // add collection_2
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"collection_2\" }");
    logicalCollection2 = vocbase.createCollection(collectionJson->slice());
    REQUIRE((nullptr != logicalCollection2));
  }

  // add collection_3
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"collection_3\" }");
    logicalCollection3 = vocbase.createCollection(collectionJson->slice());
    REQUIRE((nullptr != logicalCollection3));
  }

  // add view
  auto view = std::dynamic_pointer_cast<arangodb::iresearch::IResearchView>(
    vocbase.createView(createJson->slice())
  );
  REQUIRE((false == !view));

  // add link to collection
  {
    auto updateJson = arangodb::velocypack::Parser::fromJson(
      "{ \"links\": {"
      "\"collection_1\": { \"analyzers\": [ \"test_analyzer\", \"identity\" ], \"includeAllFields\": true, \"trackListPositions\": true },"
      "\"collection_2\": { \"analyzers\": [ \"test_analyzer\", \"identity\" ], \"includeAllFields\": true }"
      "}}"
    );
    CHECK((view->properties(updateJson->slice(), true).ok()));

    arangodb::velocypack::Builder builder;

    builder.openObject();
    view->properties(builder, true, false);
    builder.close();

    auto slice = builder.slice();
    CHECK(slice.isObject());
    CHECK(slice.get("name").copyString() == "testView");
    CHECK(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
    CHECK(slice.get("deleted").isNone()); // no system properties
    auto tmpSlice = slice.get("links");
    CHECK((true == tmpSlice.isObject() && 2 == tmpSlice.length()));
  }

  std::deque<arangodb::ManagedDocumentResult> insertedDocsView;

  // populate view with the data
  {
    arangodb::OperationOptions opt;
    TRI_voc_tick_t tick;

    arangodb::transaction::Methods trx(
      arangodb::transaction::StandaloneContext::Create(vocbase),
      EMPTY,
      EMPTY,
      EMPTY,
      arangodb::transaction::Options()
    );
    CHECK((trx.begin().ok()));

    // insert into collections
    {
      irs::utf8_path resource;
      resource/=irs::string_ref(IResearch_test_resource_dir);
      resource/=irs::string_ref("simple_sequential.json");

      auto builder = arangodb::basics::VelocyPackHelper::velocyPackFromFile(resource.utf8());
      auto root = builder.slice();
      REQUIRE(root.isArray());

      size_t i = 0;

      std::shared_ptr<arangodb::LogicalCollection> collections[] {
        logicalCollection1, logicalCollection2
      };

      for (auto doc : arangodb::velocypack::ArrayIterator(root)) {
        insertedDocsView.emplace_back();
        auto const res = collections[i % 2]->insert(&trx, doc, insertedDocsView.back(), opt, tick, false);
        CHECK(res.ok());
        ++i;
      }
    }

    // insert into collection_3
    std::deque<arangodb::ManagedDocumentResult> insertedDocsCollection;

    {
      irs::utf8_path resource;
      resource/=irs::string_ref(IResearch_test_resource_dir);
      resource/=irs::string_ref("simple_sequential_order.json");

      auto builder = arangodb::basics::VelocyPackHelper::velocyPackFromFile(resource.utf8());
      auto root = builder.slice();
      REQUIRE(root.isArray());

      for (auto doc : arangodb::velocypack::ArrayIterator(root)) {
        insertedDocsCollection.emplace_back();
        auto const res = logicalCollection3->insert(&trx, doc, insertedDocsCollection.back(), opt, tick, false);
        CHECK(res.ok());
      }
    }

    CHECK((trx.commit().ok()));
    CHECK(view->commit().ok());
  }

  // deterministic filter condition in a loop
  // (should not recreate view iterator each loop iteration, `reset` instead)
  //
  // LET c=5
  // FOR x IN 1..7
  //   FOR d IN testView
  //   SEARCH c == x.seq
  // RETURN d;
  {
    std::string const query = "LET c=5 FOR x IN 1..7 FOR d IN testView SEARCH c == d.seq RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query,
      {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule,
      }
    ));

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocsView[5].vpack()),
      arangodb::velocypack::Slice(insertedDocsView[5].vpack()),
      arangodb::velocypack::Slice(insertedDocsView[5].vpack()),
      arangodb::velocypack::Slice(insertedDocsView[5].vpack()),
      arangodb::velocypack::Slice(insertedDocsView[5].vpack()),
      arangodb::velocypack::Slice(insertedDocsView[5].vpack()),
      arangodb::velocypack::Slice(insertedDocsView[5].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    REQUIRE(expectedDocs.size() == resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (;resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      CHECK((0 == arangodb::basics::VelocyPackHelper::compare(arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    CHECK(expectedDoc == expectedDocs.end());
  }

  // non deterministic filter condition in a loop
  // (must recreate view iterator each loop iteration)
  //
  // FOR x IN 1..7
  //   FOR d IN testView
  //   SEARCH _FORWARD_(5) == x.seq
  // RETURN d;
  {
    std::string const query = "FOR x IN 1..7 FOR d IN testView SEARCH _FORWARD_(5) == d.seq RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query,
      {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule,
      }
    ));

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocsView[5].vpack()),
      arangodb::velocypack::Slice(insertedDocsView[5].vpack()),
      arangodb::velocypack::Slice(insertedDocsView[5].vpack()),
      arangodb::velocypack::Slice(insertedDocsView[5].vpack()),
      arangodb::velocypack::Slice(insertedDocsView[5].vpack()),
      arangodb::velocypack::Slice(insertedDocsView[5].vpack()),
      arangodb::velocypack::Slice(insertedDocsView[5].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    REQUIRE(expectedDocs.size() == resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (;resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      CHECK((0 == arangodb::basics::VelocyPackHelper::compare(arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    CHECK(expectedDoc == expectedDocs.end());
  }

  // non deterministic filter condition with self-reference in a loop
  // (must recreate view iterator each loop iteration)
  //
  // FOR x IN 1..7
  //   FOR d IN testView
  //   SEARCH _NONDETERM_(5) == x.seq
  // RETURN d;
  {
    std::string const query = "FOR x IN 1..7 FOR d IN testView SEARCH _NONDETERM_(5) == d.seq RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query,
      {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule,
      }
    ));

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocsView[5].vpack()),
      arangodb::velocypack::Slice(insertedDocsView[5].vpack()),
      arangodb::velocypack::Slice(insertedDocsView[5].vpack()),
      arangodb::velocypack::Slice(insertedDocsView[5].vpack()),
      arangodb::velocypack::Slice(insertedDocsView[5].vpack()),
      arangodb::velocypack::Slice(insertedDocsView[5].vpack()),
      arangodb::velocypack::Slice(insertedDocsView[5].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(TRI_ERROR_NOT_IMPLEMENTED == queryResult.code); // can't handle self-referenced variable now

//    auto result = queryResult.result->slice();
//    CHECK(result.isArray());
//
//    arangodb::velocypack::ArrayIterator resultIt(result);
//    REQUIRE(expectedDocs.size() == resultIt.size());
//
//    // Check documents
//    auto expectedDoc = expectedDocs.begin();
//    for (;resultIt.valid(); resultIt.next(), ++expectedDoc) {
//      auto const actualDoc = resultIt.value();
//      auto const resolved = actualDoc.resolveExternals();
//
//      CHECK((0 == arangodb::basics::VelocyPackHelper::compare(arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
//    }
//    CHECK(expectedDoc == expectedDocs.end());
  }

  // nondeterministic filter condition in a loop
  // (must recreate view iterator each loop iteration)
  //
  // LET c=_NONDETERM_(4)
  // FOR x IN 1..7
  //   FOR d IN testView
  //   SEARCH c == x.seq
  // RETURN d;
  {
    std::string const query = "LET c=_NONDETERM_(4) FOR x IN 1..7 FOR d IN testView SEARCH c == d.seq RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query,
      {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule,
      }
    ));

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocsView[4].vpack()),
      arangodb::velocypack::Slice(insertedDocsView[4].vpack()),
      arangodb::velocypack::Slice(insertedDocsView[4].vpack()),
      arangodb::velocypack::Slice(insertedDocsView[4].vpack()),
      arangodb::velocypack::Slice(insertedDocsView[4].vpack()),
      arangodb::velocypack::Slice(insertedDocsView[4].vpack()),
      arangodb::velocypack::Slice(insertedDocsView[4].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    REQUIRE(expectedDocs.size() == resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (;resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      CHECK((0 == arangodb::basics::VelocyPackHelper::compare(arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    CHECK(expectedDoc == expectedDocs.end());
  }

  // nondeterministic range
  // (must recreate view iterator each loop iteration)
  //
  // LET range=_NONDETERM_(0).._NONDETERM_(7)
  // FOR x IN range
  //   FOR d IN testView
  //   SEARCH d.seq == x.seq
  // RETURN d;
  {
    std::string const query = " FOR x IN _NONDETERM_(0).._NONDETERM_(7) FOR d IN testView SEARCH x == d.seq RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query,
      {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule,
      }
    ));

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocsView[0].vpack()),
      arangodb::velocypack::Slice(insertedDocsView[1].vpack()),
      arangodb::velocypack::Slice(insertedDocsView[2].vpack()),
      arangodb::velocypack::Slice(insertedDocsView[3].vpack()),
      arangodb::velocypack::Slice(insertedDocsView[4].vpack()),
      arangodb::velocypack::Slice(insertedDocsView[5].vpack()),
      arangodb::velocypack::Slice(insertedDocsView[6].vpack()),
      arangodb::velocypack::Slice(insertedDocsView[7].vpack())
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    REQUIRE(expectedDocs.size() == resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (;resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      CHECK((0 == arangodb::basics::VelocyPackHelper::compare(arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    CHECK(expectedDoc == expectedDocs.end());
  }

  // FOR x IN collection_3
  //   FOR d IN testView
  //   SEARCH d.seq == x.seq
  // RETURN d;
  {
    std::string const query =  "FOR x IN collection_3 FOR d IN testView SEARCH x.seq == d.seq RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query,
      {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule,
      }
    ));

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocsView[0].vpack()),
      arangodb::velocypack::Slice(insertedDocsView[1].vpack()),
      arangodb::velocypack::Slice(insertedDocsView[2].vpack()),
      arangodb::velocypack::Slice(insertedDocsView[3].vpack()),
      arangodb::velocypack::Slice(insertedDocsView[4].vpack()),
      arangodb::velocypack::Slice(insertedDocsView[5].vpack()),
      arangodb::velocypack::Slice(insertedDocsView[6].vpack()),
      arangodb::velocypack::Slice(insertedDocsView[7].vpack())
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    REQUIRE(expectedDocs.size() == resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (;resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      CHECK((0 == arangodb::basics::VelocyPackHelper::compare(arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    CHECK(expectedDoc == expectedDocs.end());
  }

  // FOR x IN collection_3
  //   FOR d IN testView
  //   SEARCH d.seq == x.seq
  // SORT d.seq DESC
  // RETURN d;
  {
    std::string const query = "FOR x IN collection_3 FOR d IN testView SEARCH x.seq == d.seq SORT d.seq DESC RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query,
      {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule,
      }
    ));

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocsView[7].vpack()),
      arangodb::velocypack::Slice(insertedDocsView[6].vpack()),
      arangodb::velocypack::Slice(insertedDocsView[5].vpack()),
      arangodb::velocypack::Slice(insertedDocsView[4].vpack()),
      arangodb::velocypack::Slice(insertedDocsView[3].vpack()),
      arangodb::velocypack::Slice(insertedDocsView[2].vpack()),
      arangodb::velocypack::Slice(insertedDocsView[1].vpack()),
      arangodb::velocypack::Slice(insertedDocsView[0].vpack())
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    REQUIRE(expectedDocs.size() == resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (;resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      CHECK((0 == arangodb::basics::VelocyPackHelper::compare(arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    CHECK(expectedDoc == expectedDocs.end());
  }

  // FOR x IN collection_3
  //   FOR d IN testView
  //   SEARCH d.seq == x.seq
  // SORT d.seq DESC
  // LIMIT 3
  // RETURN d;
  {
    std::string const query = "FOR x IN collection_3 FOR d IN testView SEARCH x.seq == d.seq SORT d.seq DESC LIMIT 3 RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query,
      {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule,
      }
    ));

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocsView[7].vpack()),
      arangodb::velocypack::Slice(insertedDocsView[6].vpack()),
      arangodb::velocypack::Slice(insertedDocsView[5].vpack())
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    REQUIRE(expectedDocs.size() == resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (;resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      CHECK((0 == arangodb::basics::VelocyPackHelper::compare(arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    CHECK(expectedDoc == expectedDocs.end());
  }

  // FOR x IN collection_3
  //   FOR d IN testView
  //   SEARCH d.seq == x.seq && (d.value > 5 && d.value <= 100)
  // RETURN d;
  {
    std::string const query = "FOR x IN collection_3 FOR d IN testView SEARCH x.seq == d.seq && (d.value > 5 && d.value <= 100) SORT d.seq DESC RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query,
      {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule,
      }
    ));

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocsView[6].vpack()),
      arangodb::velocypack::Slice(insertedDocsView[4].vpack()),
      arangodb::velocypack::Slice(insertedDocsView[3].vpack()),
      arangodb::velocypack::Slice(insertedDocsView[0].vpack())
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    REQUIRE(expectedDocs.size() == resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (;resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      CHECK((0 == arangodb::basics::VelocyPackHelper::compare(arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    CHECK(expectedDoc == expectedDocs.end());
  }

  // FOR x IN collection_3
  //   FOR d IN testView
  //   SEARCH d.seq == x.seq
  //   SORT BM25(d) ASC, d.seq DESC
  // RETURN d;
  {
    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocsView[7].vpack()),
      arangodb::velocypack::Slice(insertedDocsView[6].vpack()),
      arangodb::velocypack::Slice(insertedDocsView[5].vpack()),
      arangodb::velocypack::Slice(insertedDocsView[4].vpack()),
      arangodb::velocypack::Slice(insertedDocsView[3].vpack()),
      arangodb::velocypack::Slice(insertedDocsView[2].vpack()),
      arangodb::velocypack::Slice(insertedDocsView[1].vpack()),
      arangodb::velocypack::Slice(insertedDocsView[0].vpack())
    };

    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      "FOR x IN collection_3 FOR d IN testView SEARCH x.seq == d.seq SORT BM25(d) ASC, d.seq DESC RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    REQUIRE(expectedDocs.size() == resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (;resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      CHECK((0 == arangodb::basics::VelocyPackHelper::compare(arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    CHECK(expectedDoc == expectedDocs.end());
  }


  // Note: unable to push condition to the `View` now
  // FOR d IN testView
  //   FOR x IN collection_3
  //   SEARCH d.seq == x.seq
  // RETURN d;
  {
    std::string const query = "FOR d IN testView FOR x IN collection_3 FILTER d.seq == x.seq SORT d.seq RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query,
      { }
    ));

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocsView[0].vpack()),
      arangodb::velocypack::Slice(insertedDocsView[1].vpack()),
      arangodb::velocypack::Slice(insertedDocsView[2].vpack()),
      arangodb::velocypack::Slice(insertedDocsView[3].vpack()),
      arangodb::velocypack::Slice(insertedDocsView[4].vpack()),
      arangodb::velocypack::Slice(insertedDocsView[5].vpack()),
      arangodb::velocypack::Slice(insertedDocsView[6].vpack()),
      arangodb::velocypack::Slice(insertedDocsView[7].vpack())
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    REQUIRE(expectedDocs.size() == resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (;resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      CHECK((0 == arangodb::basics::VelocyPackHelper::compare(arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    CHECK(expectedDoc == expectedDocs.end());
  }

  // Note: unable to push condition to the `View` now
  // FOR d IN testView
  //   FOR x IN collection_3
  //   SEARCH d.seq == x.seq && d.name == 'B'
  // RETURN d;
  {
    std::string const query = "FOR d IN testView FOR x IN collection_3 FILTER d.seq == x.seq && d.name == 'B' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query,
      { }
    ));

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocsView[1].vpack())
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    REQUIRE(expectedDocs.size() == resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (;resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      CHECK((0 == arangodb::basics::VelocyPackHelper::compare(arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    CHECK(expectedDoc == expectedDocs.end());
  }

  // Note: unable to push condition to the `View` now
  // FOR d IN (FOR c IN testView SEARCH c.name >= 'E' && c.seq < 10 RETURN c)
  //   FOR x IN collection_3
  //   SEARCH d.seq == x.seq
  // RETURN d;
  {
    std::string const query = "FOR d IN (FOR c IN testView SEARCH c.name >= 'E' && c.seq < 10 RETURN c) FOR x IN collection_3 FILTER d.seq == x.seq SORT d.seq RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query,
      {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule,
      }
    ));

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocsView[4].vpack()),
      arangodb::velocypack::Slice(insertedDocsView[5].vpack()),
      arangodb::velocypack::Slice(insertedDocsView[6].vpack()),
      arangodb::velocypack::Slice(insertedDocsView[7].vpack())
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    REQUIRE(expectedDocs.size() == resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (;resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      CHECK((0 == arangodb::basics::VelocyPackHelper::compare(arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    CHECK(expectedDoc == expectedDocs.end());
  }

  // Note: unable to push condition to the `View` now
  // FOR d IN (FOR c IN testView SEARCH c.name >= 'E' && c.seq < 10 SORT TFIDF(c) ASC, c.seq DESC RETURN c)
  //   FOR x IN collection_3
  //   SEARCH d.seq == x.seq
  // RETURN d;
  {
    std::string const query = "FOR d IN (FOR c IN testView SEARCH c.name >= 'E' && c.seq < 10 SORT TFIDF(c) ASC, c.seq DESC RETURN c) FOR x IN collection_3 FILTER d.seq == x.seq RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query,
      {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule,
      }
    ));

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocsView[7].vpack()),
      arangodb::velocypack::Slice(insertedDocsView[6].vpack()),
      arangodb::velocypack::Slice(insertedDocsView[5].vpack()),
      arangodb::velocypack::Slice(insertedDocsView[4].vpack())
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    REQUIRE(expectedDocs.size() == resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (;resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      CHECK((0 == arangodb::basics::VelocyPackHelper::compare(arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    CHECK(expectedDoc == expectedDocs.end());
  }

  // Note: unable to push condition to the `View` now
  // FOR d IN (FOR c IN testView SEARCH c.name >= 'E' && c.seq < 10 SORT TFIDF(c) ASC, c.seq DESC RETURN c)
  //   FOR x IN collection_3
  //   SEARCH d.seq == x.seq
  // LIMIT 2
  // RETURN d;
  {
    std::string const query = "FOR d IN (FOR c IN testView SEARCH c.name >= 'E' && c.seq < 10 SORT TFIDF(c) ASC, c.seq DESC RETURN c) FOR x IN collection_3 FILTER d.seq == x.seq LIMIT 2 RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query,
      {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule,
      }
    ));

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocsView[7].vpack()),
      arangodb::velocypack::Slice(insertedDocsView[6].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    REQUIRE(expectedDocs.size() == resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (;resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      CHECK((0 == arangodb::basics::VelocyPackHelper::compare(arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    CHECK(expectedDoc == expectedDocs.end());
  }

  // Note: unable to push condition to the `View` now
  // FOR d IN (FOR c IN testView SEARCH c.name >= 'E' && c.seq < 10 SORT TFIDF(c) ASC, c.seq DESC LIMIT 3 RETURN c)
  //   FOR x IN collection_3
  //   SEARCH d.seq == x.seq
  // RETURN d;
  {
    std::string const query = "FOR d IN (FOR c IN testView SEARCH c.name >= 'E' && c.seq < 10 SORT TFIDF(c) ASC, c.seq DESC LIMIT 5 RETURN c) FOR x IN collection_3 FILTER d.seq == x.seq RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query,
      {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocsView[7].vpack()),
      arangodb::velocypack::Slice(insertedDocsView[6].vpack()),
      arangodb::velocypack::Slice(insertedDocsView[5].vpack())
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    REQUIRE(expectedDocs.size() == resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (;resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      CHECK((0 == arangodb::basics::VelocyPackHelper::compare(arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    CHECK(expectedDoc == expectedDocs.end());
  }

  // Invalid bound collection name
  {
    auto queryResult = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN (FOR c IN testView SEARCH c.name >= 'E' && c.seq < 10 SORT TFIDF(c) ASC, c.seq DESC LIMIT 5 RETURN c) FOR x IN @@collection SEARCH d.seq == x.seq RETURN d",
      arangodb::velocypack::Parser::fromJson("{ \"@collection\": \"invlaidCollectionName\" }")
    );

    REQUIRE(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND == queryResult.code);
  }

  // dependent sort condition in inner loop + custom scorer
  // (must recreate view iterator each loop iteration)
  //
  // FOR x IN 0..5
  //   FOR d IN testView
  //   SEARCH d.seq == x
  //   SORT customscorer(d,x)
  // RETURN d;
  {
    std::string const query = "FOR x IN 0..5 FOR d IN testView SEARCH d.seq == x SORT customscorer(d, x) DESC RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query,
      {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule,
      }
    ));

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocsView[5].vpack()),
      arangodb::velocypack::Slice(insertedDocsView[4].vpack()),
      arangodb::velocypack::Slice(insertedDocsView[3].vpack()),
      arangodb::velocypack::Slice(insertedDocsView[2].vpack()),
      arangodb::velocypack::Slice(insertedDocsView[1].vpack()),
      arangodb::velocypack::Slice(insertedDocsView[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    REQUIRE(expectedDocs.size() == resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (;resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      CHECK((0 == arangodb::basics::VelocyPackHelper::compare(arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    CHECK(expectedDoc == expectedDocs.end());
  }

  // invalid reference in scorer
  //
  // FOR x IN 0..5
  //   FOR d IN testView
  //   SEARCH d.seq == x
  //   SORT customscorer(x,x)
  // RETURN d;
  {
    std::string const query = "FOR d IN testView FOR i IN 0..5 SORT tfidf(i) DESC RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, { }
    ));

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(TRI_ERROR_NOT_IMPLEMENTED == queryResult.code);
  }

  // FOR i IN 1..5
  //  FOR x IN collection_0
  //    FOR d IN  SEARCH d.seq == i && d.name == x.name
  // SORT customscorer(d, x.seq)
  {
    std::string const query = "FOR i IN 1..5 FOR x IN collection_1 FOR d IN testView SEARCH d.seq == i AND d.name == x.name SORT customscorer(d, x.seq) DESC RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query,
      {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule,
      }
    ));

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocsView[4].vpack()),
      arangodb::velocypack::Slice(insertedDocsView[2].vpack())
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    REQUIRE(expectedDocs.size() == resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (;resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      CHECK((0 == arangodb::basics::VelocyPackHelper::compare(arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    CHECK(expectedDoc == expectedDocs.end());
  }

  // FOR i IN 1..5
  //  FOR x IN collection_0 SEARCH x.seq == i
  //    FOR d IN  SEARCH d.seq == x.seq && d.name == x.name
  // SORT customscorer(d, x.seq)
  {
    std::string const query = "FOR i IN 1..5 FOR x IN collection_1 FILTER x.seq == i FOR d IN testView SEARCH d.seq == x.seq AND d.name == x.name SORT customscorer(d, x.seq) DESC RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query,
      {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule,
      }
    ));

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocsView[4].vpack()),
      arangodb::velocypack::Slice(insertedDocsView[2].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    REQUIRE(expectedDocs.size() == resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (;resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      CHECK((0 == arangodb::basics::VelocyPackHelper::compare(arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    CHECK(expectedDoc == expectedDocs.end());
  }

  // unable to retrieve `d.seq` from self-referenced variable
  // FOR i IN 1..5
  //  FOR d IN  SEARCH d.seq == i SORT customscorer(d, d.seq)
  //    FOR x IN collection_0 SEARCH x.seq == d.seq && x.name == d.name
  // SORT customscorer(d, d.seq) DESC
  {
    std::string const query = "FOR i IN 1..5 FOR d IN testView SEARCH d.seq == i FOR x IN collection_1 FILTER x.seq == d.seq && x.name == d.name SORT customscorer(d, d.seq) DESC RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query,
      {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule,
      }
    ));

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(TRI_ERROR_BAD_PARAMETER == queryResult.code);
  }

  // unable to retrieve `x.seq` from inner loop
  // FOR i IN 1..5
  //  FOR d IN  SEARCH d.seq == i SORT customscorer(d, d.seq)
  //    FOR x IN collection_0 SEARCH x.seq == d.seq && x.name == d.name
  // SORT customscorer(d, x.seq) DESC
  {
    std::string const query = "FOR i IN 1..5 FOR d IN testView SEARCH d.seq == i FOR x IN collection_1 FILTER x.seq == d.seq && x.name == d.name SORT customscorer(d, x.seq) DESC RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query,
      {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule,
      }
    ));

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(TRI_ERROR_BAD_PARAMETER == queryResult.code);
  }

  // FOR i IN 1..5
  //  FOR d IN  SEARCH d.seq == i SORT customscorer(d, i) ASC
  //    FOR x IN collection_0 SEARCH x.seq == d.seq && x.name == d.name
  // SORT customscorer(d, i) DESC
  {
    std::string const query =
      "FOR i IN 1..5 "
      "  FOR d IN testView SEARCH d.seq == i SORT customscorer(d, i) ASC "
      "    FOR x IN collection_1 FILTER x.seq == d.seq && x.name == d.name "
      "SORT customscorer(d, i) DESC RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query,
      {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule,
      }
    ));

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocsView[4].vpack()),
      arangodb::velocypack::Slice(insertedDocsView[2].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    REQUIRE(expectedDocs.size() == resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (;resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      CHECK((0 == arangodb::basics::VelocyPackHelper::compare(arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    CHECK(expectedDoc == expectedDocs.end());
  }

  // dedicated to https://github.com/arangodb/planning/issues/3065$
  // Optimizer rule "inline sub-queries" which doesn't handle views correctly$
  {
    std::string const query = "LET fullAccounts = (FOR acc1 IN [1] RETURN { 'key': 'A' }) for a IN fullAccounts for d IN testView SEARCH d.name == a.key return d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule,
        arangodb::aql::OptimizerRule::inlineSubqueriesRule
      }
    ));

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocsView[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    REQUIRE(expectedDocs.size() == resultIt.size());

    // Check documents
    auto expectedDoc = expectedDocs.begin();
    for (;resultIt.valid(); resultIt.next(), ++expectedDoc) {
      auto const actualDoc = resultIt.value();
      auto const resolved = actualDoc.resolveExternals();

      CHECK((0 == arangodb::basics::VelocyPackHelper::compare(arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
    }
    CHECK(expectedDoc == expectedDocs.end());
  }

  // we don't support scorers as a part of any expression (in sort or filter)
  //
  // FOR i IN 1..5
  //   FOR d IN testView SEARCH d.seq == i
  //     FOR x IN collection_1 FILTER x.seq == d.seq && x.seq == TFIDF(d)
  {
    std::string const query =
      "FOR i IN 1..5 "
      "  FOR d IN testView SEARCH d.seq == i "
      "    FOR x IN collection_1 FILTER x.seq == d.seq && x.seq == TFIDF(d)"
      "RETURN x";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule,
      }
    ));

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(TRI_ERROR_NOT_IMPLEMENTED == queryResult.code);
  }

  // we don't support scorers as a part of any expression (in sort or filter)
  //
  // FOR i IN 1..5
  //   FOR d IN testView SEARCH d.seq == i
  //     FOR x IN collection_1 FILTER x.seq == d.seq && x.seq == TFIDF(d)
  {
    std::string const query =
      "FOR i IN 1..5 "
      "  FOR d IN testView SEARCH d.seq == i "
      "    FOR x IN collection_1 FILTER x.seq == d.seq "
      "SORT 1 + TFIDF(d)"
      "RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule,
      }
    ));

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(TRI_ERROR_NOT_IMPLEMENTED == queryResult.code);
  }

  // we don't support scorers outside the view node
  //
  // FOR d IN (FOR c IN testView SEARCH c.name >= 'E' && c.seq < 10 SORT customscorer(c) DESC LIMIT 3 RETURN c)
  //     FOR x IN collection_1 FILTER x.seq == d.seq
  // SORT customscorer(d, x.seq)
  {
    std::string const query =
     "FOR d IN (FOR c IN testView SEARCH c.name >= 'E' && c.seq < 10 SORT customscorer(c) DESC LIMIT 3 RETURN c) "
     "  FOR x IN collection_1 FILTER x.seq == d.seq "
     "    SORT customscorer(d, x.seq) "
     "RETURN x";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule,
      }
    ));

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    //REQUIRE(TRI_ERROR_NOT_IMPLEMENTED == queryResult.code);
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code); // FIXME TODO check, has this case been implmemented?
  }

  // multiple sorts (not supported now)
  // FOR i IN 1..5
  //  FOR d IN  SEARCH d.seq == i SORT customscorer(d, i) ASC
  //    FOR x IN collection_0 FILTER x.seq == d.seq && x.name == d.name
  // SORT customscorer(d, i) DESC
  {
    std::string const query =
      "FOR i IN 1..5 "
      "  FOR d IN testView SEARCH d.seq == i SORT tfidf(d, i) ASC "
      "    FOR x IN collection_1 FILTER x.seq == d.seq && x.name == d.name "
      "SORT customscorer(d, i) DESC RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule,
      }
    ));

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocsView[4].vpack()),
      arangodb::velocypack::Slice(insertedDocsView[2].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(TRI_ERROR_NOT_IMPLEMENTED == queryResult.code);

//    auto result = queryResult.result->slice();
//    CHECK(result.isArray());
//
//    arangodb::velocypack::ArrayIterator resultIt(result);
//    REQUIRE(expectedDocs.size() == resultIt.size());
//
//    // Check documents
//    auto expectedDoc = expectedDocs.begin();
//    for (;resultIt.valid(); resultIt.next(), ++expectedDoc) {
//      auto const actualDoc = resultIt.value();
//      auto const resolved = actualDoc.resolveExternals();
//
//      CHECK((0 == arangodb::basics::VelocyPackHelper::compare(arangodb::velocypack::Slice(*expectedDoc), resolved, true)));
//    }
//    CHECK(expectedDoc == expectedDocs.end());
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------