////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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

struct IResearchQueryScorerSetup {
  StorageEngineMock engine;
  arangodb::application_features::ApplicationServer server;
  std::unique_ptr<TRI_vocbase_t> system;
  std::vector<std::pair<arangodb::application_features::ApplicationFeature*, bool>> features;

  IResearchQueryScorerSetup(): engine(server), server(nullptr, nullptr) {
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

  ~IResearchQueryScorerSetup() {
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
}; // IResearchQueryScorerSetup

NS_END

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

TEST_CASE("IResearchQueryScorer", "[iresearch][iresearch-query]") {
  IResearchQueryScorerSetup s;
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

  {
    std::string const query =
      "LET x = 5 "
      "LET arr = [0,1] "
      "FOR i in 0..1 "
      "  LET rnd = _NONDETERM_(i) "
      "  FOR d IN testView SEARCH d.name >= 'A' AND d.name <= 'C' "
      "LIMIT 10 "
      "RETURN { d, score: d.seq + 3*customscorer(d, arr[TO_NUMBER(rnd != 0)]) }";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule,
      }
    ));

    std::map<size_t, arangodb::velocypack::Slice> expectedDocs {
      { 0, arangodb::velocypack::Slice(insertedDocsView[0].vpack()) },
      { 1, arangodb::velocypack::Slice(insertedDocsView[1].vpack()) },
      { 2, arangodb::velocypack::Slice(insertedDocsView[2].vpack()) },
      { 3, arangodb::velocypack::Slice(insertedDocsView[0].vpack()) },
      { 4, arangodb::velocypack::Slice(insertedDocsView[1].vpack()) },
      { 5, arangodb::velocypack::Slice(insertedDocsView[2].vpack()) },
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(TRI_ERROR_NO_ERROR == queryResult.code);

    auto result = queryResult.result->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    REQUIRE(expectedDocs.size() == resultIt.size());

    // Check documents
    for (;resultIt.valid(); resultIt.next()) {
      auto const actualValue = resultIt.value();

      auto actualScoreSlice = actualValue.get("score");
      REQUIRE(actualScoreSlice.isNumber());
      auto const actualScore = actualScoreSlice.getNumber<size_t>();
      auto expectedValue = expectedDocs.find(actualScore);
      REQUIRE(expectedValue != expectedDocs.end());

      auto const actualDoc = actualValue.get("d");
      auto const resolved = actualDoc.resolveExternals();

      CHECK((0 == arangodb::basics::VelocyPackHelper::compare(arangodb::velocypack::Slice(expectedValue->second), resolved, true)));
      expectedDocs.erase(expectedValue);
    }
    CHECK(expectedDocs.empty());
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
