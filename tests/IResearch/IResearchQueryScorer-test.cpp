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

#include "../Mocks/StorageEngineMock.h"

#if USE_ENTERPRISE
  #include "Enterprise/Ldap/LdapFeature.h"
#endif

#include "Aql/Ast.h"
#include "Aql/ExpressionContext.h"
#include "Aql/IResearchViewNode.h"
#include "Aql/OptimizerRulesFeature.h"
#include "Aql/Query.h"
#include "Cluster/ClusterFeature.h"
#include "V8/v8-globals.h"
#include "V8Server/V8DealerFeature.h"
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
#include "Basics/SmallVector.h"
#include "3rdParty/iresearch/tests/tests_config.hpp"
#include "VocBase/ManagedDocumentResult.h"

#include "IResearch/VelocyPackHelper.h"
#include "analysis/analyzers.hpp"
#include "analysis/token_attributes.hpp"
#include "utils/utf8_path.hpp"

#include <velocypack/Iterator.h>

extern const char* ARGV0; // defined in main.cpp

namespace {

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

struct IResearchQueryScorerSetup {
  StorageEngineMock engine;
  arangodb::application_features::ApplicationServer server;
  std::vector<std::pair<arangodb::application_features::ApplicationFeature*, bool>> features;

  IResearchQueryScorerSetup(): engine(server), server(nullptr, nullptr) {
    arangodb::EngineSelectorFeature::ENGINE = &engine;
    arangodb::aql::AqlFunctionFeature* functions = nullptr;

    arangodb::tests::init(true);

    // suppress INFO {authentication} Authentication is turned on (system only), authentication for unix sockets is turned on
    // suppress WARNING {authentication} --server.jwt-secret is insecure. Use --server.jwt-secret-keyfile instead
    arangodb::LogTopic::setLogLevel(arangodb::Logger::AUTHENTICATION.name(), arangodb::LogLevel::ERR);

    // suppress log messages since tests check error conditions
    arangodb::LogTopic::setLogLevel(arangodb::Logger::FIXME.name(), arangodb::LogLevel::ERR); // suppress WARNING DefaultCustomTypeHandler called
    arangodb::LogTopic::setLogLevel(arangodb::iresearch::TOPIC.name(), arangodb::LogLevel::FATAL);
    irs::logger::output_le(iresearch::logger::IRL_FATAL, stderr);

    // setup required application features
    features.emplace_back(new arangodb::V8DealerFeature(server), false); // required for DatabaseFeature::createDatabase(...)
    features.emplace_back(new arangodb::ViewTypesFeature(server), true);
    features.emplace_back(new arangodb::AuthenticationFeature(server), true);
    features.emplace_back(new arangodb::DatabasePathFeature(server), false);
    features.emplace_back(new arangodb::DatabaseFeature(server), false);
    features.emplace_back(new arangodb::ShardingFeature(server), false);
    features.emplace_back(new arangodb::QueryRegistryFeature(server), false); // must be first
    arangodb::application_features::ApplicationServer::server->addFeature(features.back().first); // need QueryRegistryFeature feature to be added now in order to create the system database
    features.emplace_back(new arangodb::SystemDatabaseFeature(server), true); // required for IResearchAnalyzerFeature
    features.emplace_back(new arangodb::TraverserEngineRegistryFeature(server), false); // must be before AqlFeature
    features.emplace_back(new arangodb::AqlFeature(server), true);
    features.emplace_back(new arangodb::aql::OptimizerRulesFeature(server), true);
    features.emplace_back(functions = new arangodb::aql::AqlFunctionFeature(server), true); // required for IResearchAnalyzerFeature
    features.emplace_back(new arangodb::iresearch::IResearchAnalyzerFeature(server), true);
    features.emplace_back(new arangodb::iresearch::IResearchFeature(server), true);

    #if USE_ENTERPRISE
      features.emplace_back(new arangodb::LdapFeature(server), false); // required for AuthenticationFeature with USE_ENTERPRISE
    #endif

    // required for V8DealerFeature::prepare(), ClusterFeature::prepare() not required
    arangodb::application_features::ApplicationServer::server->addFeature(
      new arangodb::ClusterFeature(server)
    );

    for (auto& f : features) {
      arangodb::application_features::ApplicationServer::server->addFeature(f.first);
    }

    for (auto& f : features) {
      f.first->prepare();
    }

    auto const databases = arangodb::velocypack::Parser::fromJson(std::string("[ { \"name\": \"") + arangodb::StaticStrings::SystemDatabase + "\" } ]");
    auto* dbFeature = arangodb::application_features::ApplicationServer::lookupFeature<
      arangodb::DatabaseFeature
    >("Database");
    dbFeature->loadDatabases(databases->slice());

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
    arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;
    TRI_vocbase_t* vocbase;

    dbFeature->createDatabase(1, "testVocbase", vocbase); // required for IResearchAnalyzerFeature::emplace(...)
    analyzers->emplace(result, "testVocbase::test_analyzer", "TestAnalyzer", "abc"); // cache analyzer
    analyzers->emplace(result, "testVocbase::test_csv_analyzer", "TestDelimAnalyzer", ","); // cache analyzer

    auto* dbPathFeature = arangodb::application_features::ApplicationServer::getFeature<arangodb::DatabasePathFeature>("DatabasePath");
    arangodb::tests::setDatabasePath(*dbPathFeature); // ensure test data is stored in a unique directory
  }

  ~IResearchQueryScorerSetup() {
    arangodb::AqlFeature(server).stop(); // unset singleton instance
    arangodb::LogTopic::setLogLevel(arangodb::iresearch::TOPIC.name(), arangodb::LogLevel::DEFAULT);
    arangodb::LogTopic::setLogLevel(arangodb::Logger::FIXME.name(), arangodb::LogLevel::DEFAULT);
    arangodb::application_features::ApplicationServer::server = nullptr;

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
    arangodb::EngineSelectorFeature::ENGINE = nullptr;
  }
}; // IResearchQueryScorerSetup

}

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
        auto const res = collections[i % 2]->insert(&trx, doc, insertedDocsView.back(), opt, false);
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
        auto const res = logicalCollection3->insert(&trx, doc, insertedDocsCollection.back(), opt, false);
        CHECK(res.ok());
      }
    }

    CHECK((trx.commit().ok()));
    CHECK((arangodb::tests::executeQuery(vocbase, "FOR d IN testView SEARCH 1 ==1 OPTIONS { waitForSync: true } RETURN d").result.ok())); // commit
  }

  {
    std::string const query =
      "LET arr = [0,1] "
      "FOR i in 0..1 "
      "  LET rnd = _NONDETERM_(i) "
      "  FOR d IN testView SEARCH IN_RANGE(d.name, 'A', 'C', true, true) "
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
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // ensure subqueries outstide a loop work fine
  {
    std::string const query =
      "LET x = (FOR j IN testView SEARCH j.name == 'A' SORT BM25(j) RETURN j) "
      "FOR d in testView SEARCH d.name == 'B' "
      "SORT customscorer(d, x[0].seq) "
      "RETURN { d, 'score' : customscorer(d, x[0].seq) }";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule,
      }
    ));

    std::map<size_t, arangodb::velocypack::Slice> expectedDocs {
      { 0, arangodb::velocypack::Slice(insertedDocsView[1].vpack()) },
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // FIXME
  // inline subqueries aren't supported, e.g. the query below will be transformed into
  //
  // FOR d in testView SEARCH d.name == 'B' LET #1 = customscorer(d, #2[0].seq)
  // LET #2 = (FOR j IN testView SEARCH j.name == 'A' SORT BM25(j) RETURN j)
  // RETURN { d, 'score' : #1 ) }
  {
    std::string const query =
      "FOR d in testView SEARCH d.name == 'B' "
      "RETURN { d, 'score' : customscorer(d, (FOR j IN testView SEARCH j.name == 'A' SORT BM25(j) RETURN j)[0].seq) }";

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.is(TRI_ERROR_INTERNAL));
  }

  // ensure scorers are deduplicated
  {
    std::string const queryString =
      "LET i = 1"
      "FOR d IN testView SEARCH IN_RANGE(d.name, 'A', 'B', true, false) "
      "RETURN [ customscorer(d, i), customscorer(d, 1) ] ";

    CHECK(arangodb::tests::assertRules(
      vocbase, queryString, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule,
      }
    ));

    arangodb::aql::Query query(
      false,
      vocbase,
      arangodb::aql::QueryString(queryString),
      std::shared_ptr<arangodb::velocypack::Builder>(),
      arangodb::velocypack::Parser::fromJson("{}"),
      arangodb::aql::PART_MAIN
    );

    query.prepare(arangodb::QueryRegistryFeature::registry());
    auto* plan = query.plan();
    REQUIRE(plan);

    arangodb::SmallVector<arangodb::aql::ExecutionNode*>::allocator_type::arena_type a;
    arangodb::SmallVector<arangodb::aql::ExecutionNode*> nodes{a};

    // only one scorer
    plan->findNodesOfType(nodes, arangodb::aql::ExecutionNode::ENUMERATE_IRESEARCH_VIEW, true);
    REQUIRE(1 == nodes.size());
    auto* viewNode = arangodb::aql::ExecutionNode::castTo<arangodb::iresearch::IResearchViewNode*>(nodes.front());
    REQUIRE(viewNode);
    auto& scorers = viewNode->scorers();
    REQUIRE(1 == scorers.size());
    auto* var = scorers.front().var;
    REQUIRE(var);

    // check scorer
    {
      auto* expr = scorers.front().node;
      REQUIRE(expr);
      REQUIRE(arangodb::aql::NODE_TYPE_FCALL == expr->type);
      auto* fn = static_cast<arangodb::aql::Function*>(expr->getData());
      REQUIRE(fn);
      REQUIRE(arangodb::iresearch::isScorer(*fn));
      CHECK("CUSTOMSCORER" == fn->name);

      REQUIRE(1 == expr->numMembers());
      auto* args = expr->getMember(0);
      REQUIRE(args);
      REQUIRE(arangodb::aql::NODE_TYPE_ARRAY == args->type);
      REQUIRE(2 == args->numMembers());
      auto* arg0 = args->getMember(0); // reference to d
      REQUIRE(arg0);
      REQUIRE(static_cast<void const*>(&viewNode->outVariable()) == arg0->getData());
      auto* arg1 = args->getMember(1);
      REQUIRE(arg1);
      REQUIRE(arangodb::aql::NODE_TYPE_VALUE == arg1->type);
      REQUIRE(arangodb::aql::VALUE_TYPE_INT == arg1->value.type);
      REQUIRE(1 == arg1->getIntValue());
    }

    // and 2 references
    nodes.clear();
    plan->findNodesOfType(nodes, arangodb::aql::ExecutionNode::CALCULATION, true);
    REQUIRE(1 == nodes.size());
    auto* calcNode = arangodb::aql::ExecutionNode::castTo<arangodb::aql::CalculationNode*>(nodes.front());
    REQUIRE(calcNode);
    REQUIRE(calcNode->expression());
    auto* node = calcNode->expression()->node();
    REQUIRE(node);
    REQUIRE(arangodb::aql::NODE_TYPE_ARRAY == node->type);
    REQUIRE(2 == node->numMembers());
    for (size_t i = 0; i < node->numMembers(); ++i) {
      auto* sub = node->getMember(i);
      REQUIRE(arangodb::aql::NODE_TYPE_REFERENCE == sub->type);
      CHECK(static_cast<const void*>(var) == sub->getData());
    }

    // check execution
    auto queryResult = arangodb::tests::executeQuery(vocbase, queryString);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    REQUIRE(1 == resultIt.size());

    for (;resultIt.valid(); resultIt.next()) {
      auto const actualValue = resultIt.value();
      REQUIRE(actualValue.isArray());

      VPackArrayIterator scoreIt(actualValue);
      CHECK(2 == scoreIt.size());

      for (; scoreIt.valid(); scoreIt.next()) {
        auto const value = scoreIt.value();
        REQUIRE(value.isNumber());
        CHECK(1 == value.getNumber<size_t>());
      }
    }
  }

  // ensure scorers are deduplicated (attribute access)
  {
    std::string const queryString =
      "LET obj = _NONDETERM_({ value : 2 }) "
      "FOR d IN testView SEARCH IN_RANGE(d.name, 'A', 'C', true, true) "
      "RETURN [ customscorer(d, obj.value), customscorer(d, obj.value) ] ";

    CHECK(arangodb::tests::assertRules(
      vocbase, queryString, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule,
      }
    ));

    arangodb::aql::Query query(
      false,
      vocbase,
      arangodb::aql::QueryString(queryString),
      std::shared_ptr<arangodb::velocypack::Builder>(),
      arangodb::velocypack::Parser::fromJson("{}"),
      arangodb::aql::PART_MAIN
    );

    query.prepare(arangodb::QueryRegistryFeature::registry());
    auto* plan = query.plan();
    REQUIRE(plan);

    arangodb::SmallVector<arangodb::aql::ExecutionNode*>::allocator_type::arena_type a;
    arangodb::SmallVector<arangodb::aql::ExecutionNode*> nodes{a};

    // only one scorer
    plan->findNodesOfType(nodes, arangodb::aql::ExecutionNode::ENUMERATE_IRESEARCH_VIEW, true);
    REQUIRE(1 == nodes.size());
    auto* viewNode = arangodb::aql::ExecutionNode::castTo<arangodb::iresearch::IResearchViewNode*>(nodes.front());
    REQUIRE(viewNode);
    auto& scorers = viewNode->scorers();
    REQUIRE(1 == scorers.size());
    auto* var = scorers.front().var;
    REQUIRE(var);

    // check scorer
    {
      auto* expr = scorers.front().node;
      REQUIRE(expr);
      REQUIRE(arangodb::aql::NODE_TYPE_FCALL == expr->type);
      auto* fn = static_cast<arangodb::aql::Function*>(expr->getData());
      REQUIRE(fn);
      REQUIRE(arangodb::iresearch::isScorer(*fn));
      CHECK("CUSTOMSCORER" == fn->name);

      REQUIRE(1 == expr->numMembers());
      auto* args = expr->getMember(0);
      REQUIRE(args);
      REQUIRE(arangodb::aql::NODE_TYPE_ARRAY == args->type);
      REQUIRE(2 == args->numMembers());
      auto* arg0 = args->getMember(0); // reference to d
      REQUIRE(arg0);
      REQUIRE(static_cast<void const*>(&viewNode->outVariable()) == arg0->getData());
      auto* arg1 = args->getMember(1);
      REQUIRE(arg1);
      REQUIRE(arangodb::aql::NODE_TYPE_ATTRIBUTE_ACCESS == arg1->type);
    }

    // and 2 references
    nodes.clear();
    plan->findNodesOfType(nodes, arangodb::aql::ExecutionNode::CALCULATION, true);
    REQUIRE(2 == nodes.size());
    for (auto const* node : nodes) {
      auto* calcNode = arangodb::aql::ExecutionNode::castTo<arangodb::aql::CalculationNode const*>(node);
      REQUIRE(calcNode);
      REQUIRE(calcNode->expression());

      if (calcNode->outVariable()->name == "obj") {
        continue;
      }

      auto* exprNode = calcNode->expression()->node();
      REQUIRE(exprNode);
      REQUIRE(arangodb::aql::NODE_TYPE_ARRAY == exprNode->type);
      REQUIRE(2 == exprNode->numMembers());
      for (size_t i = 0; i < exprNode->numMembers(); ++i) {
        auto* sub = exprNode->getMember(i);
        REQUIRE(arangodb::aql::NODE_TYPE_REFERENCE == sub->type);
        CHECK(static_cast<const void*>(var) == sub->getData());
      }
    }

    // check execution
    auto queryResult = arangodb::tests::executeQuery(vocbase, queryString);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    REQUIRE(3 == resultIt.size());

    for (;resultIt.valid(); resultIt.next()) {
      auto const actualValue = resultIt.value();
      REQUIRE(actualValue.isArray());

      VPackArrayIterator scoreIt(actualValue);
      CHECK(2 == scoreIt.size());

      for (; scoreIt.valid(); scoreIt.next()) {
        auto const value = scoreIt.value();
        REQUIRE(value.isNumber());
        CHECK(2 == value.getNumber<size_t>());
      }
    }
  }

  // ensure scorers are deduplicated (expression)
  {
    std::string const queryString =
      "LET obj = _NONDETERM_({ value : 2 }) "
      "FOR d IN testView SEARCH IN_RANGE(d.name, 'A', 'C', true, true) "
      "RETURN [ customscorer(d, obj.value+1), customscorer(d, obj.value+1) ] ";

    CHECK(arangodb::tests::assertRules(
      vocbase, queryString, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule,
      }
    ));

    arangodb::aql::Query query(
      false,
      vocbase,
      arangodb::aql::QueryString(queryString),
      std::shared_ptr<arangodb::velocypack::Builder>(),
      arangodb::velocypack::Parser::fromJson("{}"),
      arangodb::aql::PART_MAIN
    );

    query.prepare(arangodb::QueryRegistryFeature::registry());
    auto* plan = query.plan();
    REQUIRE(plan);

    arangodb::SmallVector<arangodb::aql::ExecutionNode*>::allocator_type::arena_type a;
    arangodb::SmallVector<arangodb::aql::ExecutionNode*> nodes{a};

    // only one scorer
    plan->findNodesOfType(nodes, arangodb::aql::ExecutionNode::ENUMERATE_IRESEARCH_VIEW, true);
    REQUIRE(1 == nodes.size());
    auto* viewNode = arangodb::aql::ExecutionNode::castTo<arangodb::iresearch::IResearchViewNode*>(nodes.front());
    REQUIRE(viewNode);
    auto& scorers = viewNode->scorers();
    REQUIRE(1 == scorers.size());
    auto* var = scorers.front().var;
    REQUIRE(var);

    // check scorer
    {
      auto* expr = scorers.front().node;
      REQUIRE(expr);
      REQUIRE(arangodb::aql::NODE_TYPE_FCALL == expr->type);
      auto* fn = static_cast<arangodb::aql::Function*>(expr->getData());
      REQUIRE(fn);
      REQUIRE(arangodb::iresearch::isScorer(*fn));
      CHECK("CUSTOMSCORER" == fn->name);

      REQUIRE(1 == expr->numMembers());
      auto* args = expr->getMember(0);
      REQUIRE(args);
      REQUIRE(arangodb::aql::NODE_TYPE_ARRAY == args->type);
      REQUIRE(2 == args->numMembers());
      auto* arg0 = args->getMember(0); // reference to d
      REQUIRE(arg0);
      REQUIRE(static_cast<void const*>(&viewNode->outVariable()) == arg0->getData());
      auto* arg1 = args->getMember(1);
      REQUIRE(arg1);
      REQUIRE(arangodb::aql::NODE_TYPE_OPERATOR_BINARY_PLUS == arg1->type);
    }

    // and 2 references
    nodes.clear();
    plan->findNodesOfType(nodes, arangodb::aql::ExecutionNode::CALCULATION, true);
    REQUIRE(2 == nodes.size());
    for (auto const* node : nodes) {
      auto* calcNode = arangodb::aql::ExecutionNode::castTo<arangodb::aql::CalculationNode const*>(node);
      REQUIRE(calcNode);
      REQUIRE(calcNode->expression());

      if (calcNode->outVariable()->name == "obj") {
        continue;
      }

      auto* exprNode = calcNode->expression()->node();
      REQUIRE(exprNode);
      REQUIRE(arangodb::aql::NODE_TYPE_ARRAY == exprNode->type);
      REQUIRE(2 == exprNode->numMembers());
      for (size_t i = 0; i < exprNode->numMembers(); ++i) {
        auto* sub = exprNode->getMember(i);
        REQUIRE(arangodb::aql::NODE_TYPE_REFERENCE == sub->type);
        CHECK(static_cast<const void*>(var) == sub->getData());
      }
    }

    // check execution
    auto queryResult = arangodb::tests::executeQuery(vocbase, queryString);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    REQUIRE(3 == resultIt.size());

    for (;resultIt.valid(); resultIt.next()) {
      auto const actualValue = resultIt.value();
      REQUIRE(actualValue.isArray());

      VPackArrayIterator scoreIt(actualValue);
      CHECK(2 == scoreIt.size());

      for (; scoreIt.valid(); scoreIt.next()) {
        auto const value = scoreIt.value();
        REQUIRE(value.isNumber());
        CHECK(3 == value.getNumber<size_t>());
      }
    }
  }

  // ensure scorers are deduplicated (indexed access)
  {
    std::string const queryString =
      "LET obj = _NONDETERM_([ 2, 5 ]) "
      "FOR d IN testView SEARCH IN_RANGE(d.name, 'A', 'C', true, true) "
      "RETURN [ customscorer(d, obj[1]), customscorer(d, obj[1]) ] ";

    CHECK(arangodb::tests::assertRules(
      vocbase, queryString, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule,
      }
    ));

    arangodb::aql::Query query(
      false,
      vocbase,
      arangodb::aql::QueryString(queryString),
      std::shared_ptr<arangodb::velocypack::Builder>(),
      arangodb::velocypack::Parser::fromJson("{}"),
      arangodb::aql::PART_MAIN
    );

    query.prepare(arangodb::QueryRegistryFeature::registry());
    auto* plan = query.plan();
    REQUIRE(plan);

    arangodb::SmallVector<arangodb::aql::ExecutionNode*>::allocator_type::arena_type a;
    arangodb::SmallVector<arangodb::aql::ExecutionNode*> nodes{a};

    // only one scorer
    plan->findNodesOfType(nodes, arangodb::aql::ExecutionNode::ENUMERATE_IRESEARCH_VIEW, true);
    REQUIRE(1 == nodes.size());
    auto* viewNode = arangodb::aql::ExecutionNode::castTo<arangodb::iresearch::IResearchViewNode*>(nodes.front());
    REQUIRE(viewNode);
    auto& scorers = viewNode->scorers();
    REQUIRE(1 == scorers.size());
    auto* var = scorers.front().var;
    REQUIRE(var);

    // check scorer
    {
      auto* expr = scorers.front().node;
      REQUIRE(expr);
      REQUIRE(arangodb::aql::NODE_TYPE_FCALL == expr->type);
      auto* fn = static_cast<arangodb::aql::Function*>(expr->getData());
      REQUIRE(fn);
      REQUIRE(arangodb::iresearch::isScorer(*fn));
      CHECK("CUSTOMSCORER" == fn->name);

      REQUIRE(1 == expr->numMembers());
      auto* args = expr->getMember(0);
      REQUIRE(args);
      REQUIRE(arangodb::aql::NODE_TYPE_ARRAY == args->type);
      REQUIRE(2 == args->numMembers());
      auto* arg0 = args->getMember(0); // reference to d
      REQUIRE(arg0);
      REQUIRE(static_cast<void const*>(&viewNode->outVariable()) == arg0->getData());
      auto* arg1 = args->getMember(1);
      REQUIRE(arg1);
      REQUIRE(arangodb::aql::NODE_TYPE_INDEXED_ACCESS == arg1->type);
    }

    // and 2 references
    nodes.clear();
    plan->findNodesOfType(nodes, arangodb::aql::ExecutionNode::CALCULATION, true);
    REQUIRE(2 == nodes.size());
    for (auto const* node : nodes) {
      auto* calcNode = arangodb::aql::ExecutionNode::castTo<arangodb::aql::CalculationNode const*>(node);
      REQUIRE(calcNode);
      REQUIRE(calcNode->expression());

      if (calcNode->outVariable()->name == "obj") {
        continue;
      }

      auto* exprNode = calcNode->expression()->node();
      REQUIRE(exprNode);
      REQUIRE(arangodb::aql::NODE_TYPE_ARRAY == exprNode->type);
      REQUIRE(2 == exprNode->numMembers());
      for (size_t i = 0; i < exprNode->numMembers(); ++i) {
        auto* sub = exprNode->getMember(i);
        REQUIRE(arangodb::aql::NODE_TYPE_REFERENCE == sub->type);
        CHECK(static_cast<const void*>(var) == sub->getData());
      }
    }

    // check execution
    auto queryResult = arangodb::tests::executeQuery(vocbase, queryString);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    REQUIRE(3 == resultIt.size());

    for (;resultIt.valid(); resultIt.next()) {
      auto const actualValue = resultIt.value();
      REQUIRE(actualValue.isArray());

      VPackArrayIterator scoreIt(actualValue);
      CHECK(2 == scoreIt.size());

      for (; scoreIt.valid(); scoreIt.next()) {
        auto const value = scoreIt.value();
        REQUIRE(value.isNumber());
        CHECK(5 == value.getNumber<size_t>());
      }
    }
  }

  // ensure scorers are deduplicated (ternary)
  {
    std::string const queryString =
      "LET obj = _NONDETERM_([ 2, 5 ]) "
      "FOR d IN testView SEARCH IN_RANGE(d.name, 'A', 'C', true, true) "
      "RETURN [ customscorer(d, obj[0] > obj[1] ? 1 : 2), customscorer(d, obj[0] > obj[1] ? 1 : 2) ] ";

    CHECK(arangodb::tests::assertRules(
      vocbase, queryString, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule,
      }
    ));

    arangodb::aql::Query query(
      false,
      vocbase,
      arangodb::aql::QueryString(queryString),
      std::shared_ptr<arangodb::velocypack::Builder>(),
      arangodb::velocypack::Parser::fromJson("{}"),
      arangodb::aql::PART_MAIN
    );

    query.prepare(arangodb::QueryRegistryFeature::registry());
    auto* plan = query.plan();
    REQUIRE(plan);

    arangodb::SmallVector<arangodb::aql::ExecutionNode*>::allocator_type::arena_type a;
    arangodb::SmallVector<arangodb::aql::ExecutionNode*> nodes{a};

    // only one scorer
    plan->findNodesOfType(nodes, arangodb::aql::ExecutionNode::ENUMERATE_IRESEARCH_VIEW, true);
    REQUIRE(1 == nodes.size());
    auto* viewNode = arangodb::aql::ExecutionNode::castTo<arangodb::iresearch::IResearchViewNode*>(nodes.front());
    REQUIRE(viewNode);
    auto& scorers = viewNode->scorers();
    REQUIRE(1 == scorers.size());
    auto* var = scorers.front().var;
    REQUIRE(var);

    // check scorer
    {
      auto* expr = scorers.front().node;
      REQUIRE(expr);
      REQUIRE(arangodb::aql::NODE_TYPE_FCALL == expr->type);
      auto* fn = static_cast<arangodb::aql::Function*>(expr->getData());
      REQUIRE(fn);
      REQUIRE(arangodb::iresearch::isScorer(*fn));
      CHECK("CUSTOMSCORER" == fn->name);

      REQUIRE(1 == expr->numMembers());
      auto* args = expr->getMember(0);
      REQUIRE(args);
      REQUIRE(arangodb::aql::NODE_TYPE_ARRAY == args->type);
      REQUIRE(2 == args->numMembers());
      auto* arg0 = args->getMember(0); // reference to d
      REQUIRE(arg0);
      REQUIRE(static_cast<void const*>(&viewNode->outVariable()) == arg0->getData());
      auto* arg1 = args->getMember(1);
      REQUIRE(arg1);
      REQUIRE(arangodb::aql::NODE_TYPE_OPERATOR_TERNARY == arg1->type);
    }

    // and 2 references
    nodes.clear();
    plan->findNodesOfType(nodes, arangodb::aql::ExecutionNode::CALCULATION, true);
    REQUIRE(2 == nodes.size());
    for (auto const* node : nodes) {
      auto* calcNode = arangodb::aql::ExecutionNode::castTo<arangodb::aql::CalculationNode const*>(node);
      REQUIRE(calcNode);
      REQUIRE(calcNode->expression());

      if (calcNode->outVariable()->name == "obj") {
        continue;
      }

      auto* exprNode = calcNode->expression()->node();
      REQUIRE(exprNode);
      REQUIRE(arangodb::aql::NODE_TYPE_ARRAY == exprNode->type);
      REQUIRE(2 == exprNode->numMembers());
      for (size_t i = 0; i < exprNode->numMembers(); ++i) {
        auto* sub = exprNode->getMember(i);
        REQUIRE(arangodb::aql::NODE_TYPE_REFERENCE == sub->type);
        CHECK(static_cast<const void*>(var) == sub->getData());
      }
    }

    // check execution
    auto queryResult = arangodb::tests::executeQuery(vocbase, queryString);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    REQUIRE(3 == resultIt.size());

    for (;resultIt.valid(); resultIt.next()) {
      auto const actualValue = resultIt.value();
      REQUIRE(actualValue.isArray());

      VPackArrayIterator scoreIt(actualValue);
      CHECK(2 == scoreIt.size());

      for (; scoreIt.valid(); scoreIt.next()) {
        auto const value = scoreIt.value();
        REQUIRE(value.isNumber());
        CHECK(2 == value.getNumber<size_t>());
      }
    }
  }

  // ensure scorers aren't deduplicated (ternary)
  {
    std::string const queryString =
      "LET obj = _NONDETERM_([ 2, 5 ]) "
      "FOR d IN testView SEARCH IN_RANGE(d.name, 'A', 'C', true, true) "
      "RETURN [ customscorer(d, obj[0] > obj[1] ? 1 : 2), customscorer(d, obj[1] > obj[2] ? 1 : 2) ] ";

    CHECK(arangodb::tests::assertRules(
      vocbase, queryString, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule,
      }
    ));

    arangodb::aql::Query query(
      false,
      vocbase,
      arangodb::aql::QueryString(queryString),
      std::shared_ptr<arangodb::velocypack::Builder>(),
      arangodb::velocypack::Parser::fromJson("{}"),
      arangodb::aql::PART_MAIN
    );

    query.prepare(arangodb::QueryRegistryFeature::registry());
    auto* plan = query.plan();
    REQUIRE(plan);

    arangodb::SmallVector<arangodb::aql::ExecutionNode*>::allocator_type::arena_type a;
    arangodb::SmallVector<arangodb::aql::ExecutionNode*> nodes{a};

    // only one scorer
    plan->findNodesOfType(nodes, arangodb::aql::ExecutionNode::ENUMERATE_IRESEARCH_VIEW, true);
    REQUIRE(1 == nodes.size());
    auto* viewNode = arangodb::aql::ExecutionNode::castTo<arangodb::iresearch::IResearchViewNode*>(nodes.front());
    REQUIRE(viewNode);
    auto& scorers = viewNode->scorers();
    REQUIRE(2 == scorers.size());
    auto* var = scorers.front().var;
    REQUIRE(var);

    // check scorers
    {
      auto* expr = scorers[0].node;
      REQUIRE(expr);
      REQUIRE(arangodb::aql::NODE_TYPE_FCALL == expr->type);
      auto* fn = static_cast<arangodb::aql::Function*>(expr->getData());
      REQUIRE(fn);
      REQUIRE(arangodb::iresearch::isScorer(*fn));
      CHECK("CUSTOMSCORER" == fn->name);

      REQUIRE(1 == expr->numMembers());
      auto* args = expr->getMember(0);
      REQUIRE(args);
      REQUIRE(arangodb::aql::NODE_TYPE_ARRAY == args->type);
      REQUIRE(2 == args->numMembers());
      auto* arg0 = args->getMember(0); // reference to d
      REQUIRE(arg0);
      REQUIRE(static_cast<void const*>(&viewNode->outVariable()) == arg0->getData());
      auto* arg1 = args->getMember(1);
      REQUIRE(arg1);
      REQUIRE(arangodb::aql::NODE_TYPE_OPERATOR_TERNARY == arg1->type);
    }

    {
      auto* expr = scorers[1].node;
      REQUIRE(expr);
      REQUIRE(arangodb::aql::NODE_TYPE_FCALL == expr->type);
      auto* fn = static_cast<arangodb::aql::Function*>(expr->getData());
      REQUIRE(fn);
      REQUIRE(arangodb::iresearch::isScorer(*fn));
      CHECK("CUSTOMSCORER" == fn->name);

      REQUIRE(1 == expr->numMembers());
      auto* args = expr->getMember(0);
      REQUIRE(args);
      REQUIRE(arangodb::aql::NODE_TYPE_ARRAY == args->type);
      REQUIRE(2 == args->numMembers());
      auto* arg0 = args->getMember(0); // reference to d
      REQUIRE(arg0);
      REQUIRE(static_cast<void const*>(&viewNode->outVariable()) == arg0->getData());
      auto* arg1 = args->getMember(1);
      REQUIRE(arg1);
      REQUIRE(arangodb::aql::NODE_TYPE_OPERATOR_TERNARY == arg1->type);
    }

    // check execution
    auto queryResult = arangodb::tests::executeQuery(vocbase, queryString);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    REQUIRE(3 == resultIt.size());

    for (;resultIt.valid(); resultIt.next()) {
      auto const actualValue = resultIt.value();
      REQUIRE(actualValue.isArray());

      VPackArrayIterator scoreIt(actualValue);
      CHECK(2 == scoreIt.size());
      REQUIRE(scoreIt.valid());

      {
        auto const value = scoreIt.value();
        REQUIRE(value.isNumber());
        CHECK(2 == value.getNumber<size_t>());
        scoreIt.next();
      }

      {
        auto const value = scoreIt.value();
        REQUIRE(value.isNumber());
        CHECK(1 == value.getNumber<size_t>());
        scoreIt.next();
      }

      REQUIRE(!scoreIt.valid());
    }
  }

  // ensure scorers are deduplicated (complex expression)
  {
    std::string const queryString =
      "LET obj = _NONDETERM_([ 2, 5 ]) "
      "FOR d IN testView SEARCH IN_RANGE(d.name, 'A', 'C', true, true) "
      "RETURN [ customscorer(d, 5*obj[0]*TO_NUMBER(obj[1] > obj[2])/obj[1] - 1), customscorer(d, 5*obj[0]*TO_NUMBER(obj[1] > obj[2])/obj[1] - 1) ] ";

    CHECK(arangodb::tests::assertRules(
      vocbase, queryString, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule,
      }
    ));

    arangodb::aql::Query query(
      false,
      vocbase,
      arangodb::aql::QueryString(queryString),
      std::shared_ptr<arangodb::velocypack::Builder>(),
      arangodb::velocypack::Parser::fromJson("{}"),
      arangodb::aql::PART_MAIN
    );

    query.prepare(arangodb::QueryRegistryFeature::registry());
    auto* plan = query.plan();
    REQUIRE(plan);

    arangodb::SmallVector<arangodb::aql::ExecutionNode*>::allocator_type::arena_type a;
    arangodb::SmallVector<arangodb::aql::ExecutionNode*> nodes{a};

    // only one scorer
    plan->findNodesOfType(nodes, arangodb::aql::ExecutionNode::ENUMERATE_IRESEARCH_VIEW, true);
    REQUIRE(1 == nodes.size());
    auto* viewNode = arangodb::aql::ExecutionNode::castTo<arangodb::iresearch::IResearchViewNode*>(nodes.front());
    REQUIRE(viewNode);
    auto& scorers = viewNode->scorers();
    REQUIRE(1 == scorers.size());
    auto* var = scorers.front().var;
    REQUIRE(var);

    // check scorer
    {
      auto* expr = scorers.front().node;
      REQUIRE(expr);
      REQUIRE(arangodb::aql::NODE_TYPE_FCALL == expr->type);
      auto* fn = static_cast<arangodb::aql::Function*>(expr->getData());
      REQUIRE(fn);
      REQUIRE(arangodb::iresearch::isScorer(*fn));
      CHECK("CUSTOMSCORER" == fn->name);

      REQUIRE(1 == expr->numMembers());
      auto* args = expr->getMember(0);
      REQUIRE(args);
      REQUIRE(arangodb::aql::NODE_TYPE_ARRAY == args->type);
      REQUIRE(2 == args->numMembers());
      auto* arg0 = args->getMember(0); // reference to d
      REQUIRE(arg0);
      REQUIRE(static_cast<void const*>(&viewNode->outVariable()) == arg0->getData());
      auto* arg1 = args->getMember(1);
      REQUIRE(arg1);
      CHECK(arangodb::aql::NODE_TYPE_OPERATOR_BINARY_MINUS == arg1->type);
    }

    // and 2 references
    nodes.clear();
    plan->findNodesOfType(nodes, arangodb::aql::ExecutionNode::CALCULATION, true);
    REQUIRE(2 == nodes.size());
    for (auto const* node : nodes) {
      auto* calcNode = arangodb::aql::ExecutionNode::castTo<arangodb::aql::CalculationNode const*>(node);
      REQUIRE(calcNode);
      REQUIRE(calcNode->expression());

      if (calcNode->outVariable()->name == "obj") {
        continue;
      }

      auto* exprNode = calcNode->expression()->node();
      REQUIRE(exprNode);
      REQUIRE(arangodb::aql::NODE_TYPE_ARRAY == exprNode->type);
      REQUIRE(2 == exprNode->numMembers());
      for (size_t i = 0; i < exprNode->numMembers(); ++i) {
        auto* sub = exprNode->getMember(i);
        REQUIRE(arangodb::aql::NODE_TYPE_REFERENCE == sub->type);
        CHECK(static_cast<const void*>(var) == sub->getData());
      }
    }

    // check execution
    auto queryResult = arangodb::tests::executeQuery(vocbase, queryString);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    REQUIRE(3 == resultIt.size());

    for (;resultIt.valid(); resultIt.next()) {
      auto const actualValue = resultIt.value();
      REQUIRE(actualValue.isArray());

      VPackArrayIterator scoreIt(actualValue);
      CHECK(2 == scoreIt.size());

      for (; scoreIt.valid(); scoreIt.next()) {
        auto const value = scoreIt.value();
        REQUIRE(value.isNumber());
        CHECK(1 == value.getNumber<size_t>());
      }
    }
  }

  // ensure scorers are deduplicated (dynamic object attribute name)
  {
    std::string const queryString =
      "LET obj = _NONDETERM_([ 2, 5 ]) "
      "FOR d IN testView SEARCH IN_RANGE(d.name, 'A', 'C', true, true) "
      "RETURN [ customscorer(d, { [ CONCAT(obj[0], obj[1]) ] : 1 }), customscorer(d, { [ CONCAT(obj[0], obj[1]) ] : 1 }) ]";

    CHECK(arangodb::tests::assertRules(
      vocbase, queryString, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule,
      }
    ));

    arangodb::aql::Query query(
      false,
      vocbase,
      arangodb::aql::QueryString(queryString),
      std::shared_ptr<arangodb::velocypack::Builder>(),
      arangodb::velocypack::Parser::fromJson("{}"),
      arangodb::aql::PART_MAIN
    );

    query.prepare(arangodb::QueryRegistryFeature::registry());
    auto* plan = query.plan();
    REQUIRE(plan);

    arangodb::SmallVector<arangodb::aql::ExecutionNode*>::allocator_type::arena_type a;
    arangodb::SmallVector<arangodb::aql::ExecutionNode*> nodes{a};

    // only one scorer
    plan->findNodesOfType(nodes, arangodb::aql::ExecutionNode::ENUMERATE_IRESEARCH_VIEW, true);
    REQUIRE(1 == nodes.size());
    auto* viewNode = arangodb::aql::ExecutionNode::castTo<arangodb::iresearch::IResearchViewNode*>(nodes.front());
    REQUIRE(viewNode);
    auto& scorers = viewNode->scorers();
    REQUIRE(1 == scorers.size());
    auto* var = scorers.front().var;
    REQUIRE(var);

    // check scorer
    {
      auto* expr = scorers.front().node;
      REQUIRE(expr);
      REQUIRE(arangodb::aql::NODE_TYPE_FCALL == expr->type);
      auto* fn = static_cast<arangodb::aql::Function*>(expr->getData());
      REQUIRE(fn);
      REQUIRE(arangodb::iresearch::isScorer(*fn));
      CHECK("CUSTOMSCORER" == fn->name);

      REQUIRE(1 == expr->numMembers());
      auto* args = expr->getMember(0);
      REQUIRE(args);
      REQUIRE(arangodb::aql::NODE_TYPE_ARRAY == args->type);
      REQUIRE(2 == args->numMembers());
      auto* arg0 = args->getMember(0); // reference to d
      REQUIRE(arg0);
      REQUIRE(static_cast<void const*>(&viewNode->outVariable()) == arg0->getData());
      auto* arg1 = args->getMember(1);
      REQUIRE(arg1);
      CHECK(arangodb::aql::NODE_TYPE_OBJECT == arg1->type);
    }

    // and 2 references
    nodes.clear();
    plan->findNodesOfType(nodes, arangodb::aql::ExecutionNode::CALCULATION, true);
    REQUIRE(2 == nodes.size());
    for (auto const* node : nodes) {
      auto* calcNode = arangodb::aql::ExecutionNode::castTo<arangodb::aql::CalculationNode const*>(node);
      REQUIRE(calcNode);
      REQUIRE(calcNode->expression());

      if (calcNode->outVariable()->name == "obj") {
        continue;
      }

      auto* exprNode = calcNode->expression()->node();
      REQUIRE(exprNode);
      REQUIRE(arangodb::aql::NODE_TYPE_ARRAY == exprNode->type);
      REQUIRE(2 == exprNode->numMembers());
      for (size_t i = 0; i < exprNode->numMembers(); ++i) {
        auto* sub = exprNode->getMember(i);
        REQUIRE(arangodb::aql::NODE_TYPE_REFERENCE == sub->type);
        CHECK(static_cast<const void*>(var) == sub->getData());
      }
    }
  }

  // ensure scorers are deduplicated (dynamic object value)
  {
    std::string const queryString =
      "LET obj = _NONDETERM_([ 2, 5 ]) "
      "FOR d IN testView SEARCH IN_RANGE(d.name, 'A', 'C', true, true) "
      "RETURN [ customscorer(d, { foo : obj[1] }), customscorer(d, { foo : obj[1] }) ]";

    CHECK(arangodb::tests::assertRules(
      vocbase, queryString, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule,
      }
    ));

    arangodb::aql::Query query(
      false,
      vocbase,
      arangodb::aql::QueryString(queryString),
      std::shared_ptr<arangodb::velocypack::Builder>(),
      arangodb::velocypack::Parser::fromJson("{}"),
      arangodb::aql::PART_MAIN
    );

    query.prepare(arangodb::QueryRegistryFeature::registry());
    auto* plan = query.plan();
    REQUIRE(plan);

    arangodb::SmallVector<arangodb::aql::ExecutionNode*>::allocator_type::arena_type a;
    arangodb::SmallVector<arangodb::aql::ExecutionNode*> nodes{a};

    // only one scorer
    plan->findNodesOfType(nodes, arangodb::aql::ExecutionNode::ENUMERATE_IRESEARCH_VIEW, true);
    REQUIRE(1 == nodes.size());
    auto* viewNode = arangodb::aql::ExecutionNode::castTo<arangodb::iresearch::IResearchViewNode*>(nodes.front());
    REQUIRE(viewNode);
    auto& scorers = viewNode->scorers();
    REQUIRE(1 == scorers.size());
    auto* var = scorers.front().var;
    REQUIRE(var);

    // check scorer
    {
      auto* expr = scorers.front().node;
      REQUIRE(expr);
      REQUIRE(arangodb::aql::NODE_TYPE_FCALL == expr->type);
      auto* fn = static_cast<arangodb::aql::Function*>(expr->getData());
      REQUIRE(fn);
      REQUIRE(arangodb::iresearch::isScorer(*fn));
      CHECK("CUSTOMSCORER" == fn->name);

      REQUIRE(1 == expr->numMembers());
      auto* args = expr->getMember(0);
      REQUIRE(args);
      REQUIRE(arangodb::aql::NODE_TYPE_ARRAY == args->type);
      REQUIRE(2 == args->numMembers());
      auto* arg0 = args->getMember(0); // reference to d
      REQUIRE(arg0);
      REQUIRE(static_cast<void const*>(&viewNode->outVariable()) == arg0->getData());
      auto* arg1 = args->getMember(1);
      REQUIRE(arg1);
      CHECK(arangodb::aql::NODE_TYPE_OBJECT == arg1->type);
    }

    // and 2 references
    nodes.clear();
    plan->findNodesOfType(nodes, arangodb::aql::ExecutionNode::CALCULATION, true);
    REQUIRE(2 == nodes.size());
    for (auto const* node : nodes) {
      auto* calcNode = arangodb::aql::ExecutionNode::castTo<arangodb::aql::CalculationNode const*>(node);
      REQUIRE(calcNode);
      REQUIRE(calcNode->expression());

      if (calcNode->outVariable()->name == "obj") {
        continue;
      }

      auto* exprNode = calcNode->expression()->node();
      REQUIRE(exprNode);
      REQUIRE(arangodb::aql::NODE_TYPE_ARRAY == exprNode->type);
      REQUIRE(2 == exprNode->numMembers());
      for (size_t i = 0; i < exprNode->numMembers(); ++i) {
        auto* sub = exprNode->getMember(i);
        REQUIRE(arangodb::aql::NODE_TYPE_REFERENCE == sub->type);
        CHECK(static_cast<const void*>(var) == sub->getData());
      }
    }
  }

  // ensure scorers aren't deduplicated (complex expression)
  {
    std::string const queryString =
      "LET obj = _NONDETERM_([ 2, 5 ]) "
      "FOR d IN testView SEARCH IN_RANGE(d.name, 'A', 'C', true, true) "
      "RETURN [ customscorer(d, 5*obj[0]*TO_NUMBER(obj[1] > obj[2])/obj[1] - 1), customscorer(d, 5*obj[0]*TO_NUMBER(obj[1] > obj[2])/obj[1] - 2) ] ";

    CHECK(arangodb::tests::assertRules(
      vocbase, queryString, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule,
      }
    ));

    arangodb::aql::Query query(
      false,
      vocbase,
      arangodb::aql::QueryString(queryString),
      std::shared_ptr<arangodb::velocypack::Builder>(),
      arangodb::velocypack::Parser::fromJson("{}"),
      arangodb::aql::PART_MAIN
    );

    query.prepare(arangodb::QueryRegistryFeature::registry());
    auto* plan = query.plan();
    REQUIRE(plan);

    arangodb::SmallVector<arangodb::aql::ExecutionNode*>::allocator_type::arena_type a;
    arangodb::SmallVector<arangodb::aql::ExecutionNode*> nodes{a};

    // only one scorer
    plan->findNodesOfType(nodes, arangodb::aql::ExecutionNode::ENUMERATE_IRESEARCH_VIEW, true);
    REQUIRE(1 == nodes.size());
    auto* viewNode = arangodb::aql::ExecutionNode::castTo<arangodb::iresearch::IResearchViewNode*>(nodes.front());
    REQUIRE(viewNode);
    auto& scorers = viewNode->scorers();
    REQUIRE(2 == scorers.size());
    auto* var = scorers.front().var;
    REQUIRE(var);

    // check scorers
    {
      auto* expr = scorers[0].node;
      REQUIRE(expr);
      REQUIRE(arangodb::aql::NODE_TYPE_FCALL == expr->type);
      auto* fn = static_cast<arangodb::aql::Function*>(expr->getData());
      REQUIRE(fn);
      REQUIRE(arangodb::iresearch::isScorer(*fn));
      CHECK("CUSTOMSCORER" == fn->name);

      REQUIRE(1 == expr->numMembers());
      auto* args = expr->getMember(0);
      REQUIRE(args);
      REQUIRE(arangodb::aql::NODE_TYPE_ARRAY == args->type);
      REQUIRE(2 == args->numMembers());
      auto* arg0 = args->getMember(0); // reference to d
      REQUIRE(arg0);
      REQUIRE(static_cast<void const*>(&viewNode->outVariable()) == arg0->getData());
      auto* arg1 = args->getMember(1);
      REQUIRE(arg1);
      CHECK(arangodb::aql::NODE_TYPE_OPERATOR_BINARY_MINUS == arg1->type);
    }

    {
      auto* expr = scorers[1].node;
      REQUIRE(expr);
      REQUIRE(arangodb::aql::NODE_TYPE_FCALL == expr->type);
      auto* fn = static_cast<arangodb::aql::Function*>(expr->getData());
      REQUIRE(fn);
      REQUIRE(arangodb::iresearch::isScorer(*fn));
      CHECK("CUSTOMSCORER" == fn->name);

      REQUIRE(1 == expr->numMembers());
      auto* args = expr->getMember(0);
      REQUIRE(args);
      REQUIRE(arangodb::aql::NODE_TYPE_ARRAY == args->type);
      REQUIRE(2 == args->numMembers());
      auto* arg0 = args->getMember(0); // reference to d
      REQUIRE(arg0);
      REQUIRE(static_cast<void const*>(&viewNode->outVariable()) == arg0->getData());
      auto* arg1 = args->getMember(1);
      REQUIRE(arg1);
      CHECK(arangodb::aql::NODE_TYPE_OPERATOR_BINARY_MINUS == arg1->type);
    }

    // check execution
    auto queryResult = arangodb::tests::executeQuery(vocbase, queryString);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
    CHECK(result.isArray());

    arangodb::velocypack::ArrayIterator resultIt(result);
    REQUIRE(3 == resultIt.size());

    for (;resultIt.valid(); resultIt.next()) {
      auto const actualValue = resultIt.value();
      REQUIRE(actualValue.isArray());

      VPackArrayIterator scoreIt(actualValue);
      CHECK(2 == scoreIt.size());
      REQUIRE(scoreIt.valid());

      {
        auto const value = scoreIt.value();
        REQUIRE(value.isNumber());
        CHECK(1 == value.getNumber<size_t>());
        scoreIt.next();
      }

      {
        auto const value = scoreIt.value();
        REQUIRE(value.isNumber());
        CHECK(0 == value.getNumber<size_t>());
        scoreIt.next();
      }

      REQUIRE(!scoreIt.valid());
    }
  }

  // ensure scorers are deduplicated (array comparison operators)
  {
    std::string const queryString =
      "LET obj = _NONDETERM_([ 2, 5 ]) "
      "FOR d IN testView SEARCH IN_RANGE(d.name, 'A', 'C', true, true) "
      "RETURN [ customscorer(d, obj any == 3), customscorer(d, obj any == 3) ]";

    CHECK(arangodb::tests::assertRules(
      vocbase, queryString, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule,
      }
    ));

    arangodb::aql::Query query(
      false,
      vocbase,
      arangodb::aql::QueryString(queryString),
      std::shared_ptr<arangodb::velocypack::Builder>(),
      arangodb::velocypack::Parser::fromJson("{}"),
      arangodb::aql::PART_MAIN
    );

    query.prepare(arangodb::QueryRegistryFeature::registry());
    auto* plan = query.plan();
    REQUIRE(plan);

    arangodb::SmallVector<arangodb::aql::ExecutionNode*>::allocator_type::arena_type a;
    arangodb::SmallVector<arangodb::aql::ExecutionNode*> nodes{a};

    // only one scorer
    plan->findNodesOfType(nodes, arangodb::aql::ExecutionNode::ENUMERATE_IRESEARCH_VIEW, true);
    REQUIRE(1 == nodes.size());
    auto* viewNode = arangodb::aql::ExecutionNode::castTo<arangodb::iresearch::IResearchViewNode*>(nodes.front());
    REQUIRE(viewNode);
    auto& scorers = viewNode->scorers();
    REQUIRE(1 == scorers.size());
    auto* var = scorers.front().var;
    REQUIRE(var);

    // check scorer
    {
      auto* expr = scorers.front().node;
      REQUIRE(expr);
      REQUIRE(arangodb::aql::NODE_TYPE_FCALL == expr->type);
      auto* fn = static_cast<arangodb::aql::Function*>(expr->getData());
      REQUIRE(fn);
      REQUIRE(arangodb::iresearch::isScorer(*fn));
      CHECK("CUSTOMSCORER" == fn->name);

      REQUIRE(1 == expr->numMembers());
      auto* args = expr->getMember(0);
      REQUIRE(args);
      REQUIRE(arangodb::aql::NODE_TYPE_ARRAY == args->type);
      REQUIRE(2 == args->numMembers());
      auto* arg0 = args->getMember(0); // reference to d
      REQUIRE(arg0);
      REQUIRE(static_cast<void const*>(&viewNode->outVariable()) == arg0->getData());
      auto* arg1 = args->getMember(1);
      REQUIRE(arg1);
      CHECK(arangodb::aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_EQ == arg1->type);
    }

    // and 2 references
    nodes.clear();
    plan->findNodesOfType(nodes, arangodb::aql::ExecutionNode::CALCULATION, true);
    REQUIRE(2 == nodes.size());
    for (auto const* node : nodes) {
      auto* calcNode = arangodb::aql::ExecutionNode::castTo<arangodb::aql::CalculationNode const*>(node);
      REQUIRE(calcNode);
      REQUIRE(calcNode->expression());

      if (calcNode->outVariable()->name == "obj") {
        continue;
      }

      auto* exprNode = calcNode->expression()->node();
      REQUIRE(exprNode);
      REQUIRE(arangodb::aql::NODE_TYPE_ARRAY == exprNode->type);
      REQUIRE(2 == exprNode->numMembers());
      for (size_t i = 0; i < exprNode->numMembers(); ++i) {
        auto* sub = exprNode->getMember(i);
        REQUIRE(arangodb::aql::NODE_TYPE_REFERENCE == sub->type);
        CHECK(static_cast<const void*>(var) == sub->getData());
      }
    }
  }

  // ensure scorers aren't deduplicated (array comparison operator)
  {
    std::string const queryString =
      "LET obj = _NONDETERM_([ 2, 5 ]) "
      "FOR d IN testView SEARCH IN_RANGE(d.name, 'A', 'C', true, true) "
      "RETURN [ customscorer(d, obj any == 3), customscorer(d, obj all == 3) ]";

    CHECK(arangodb::tests::assertRules(
      vocbase, queryString, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule,
      }
    ));

    arangodb::aql::Query query(
      false,
      vocbase,
      arangodb::aql::QueryString(queryString),
      std::shared_ptr<arangodb::velocypack::Builder>(),
      arangodb::velocypack::Parser::fromJson("{}"),
      arangodb::aql::PART_MAIN
    );

    query.prepare(arangodb::QueryRegistryFeature::registry());
    auto* plan = query.plan();
    REQUIRE(plan);

    arangodb::SmallVector<arangodb::aql::ExecutionNode*>::allocator_type::arena_type a;
    arangodb::SmallVector<arangodb::aql::ExecutionNode*> nodes{a};

    // only one scorer
    plan->findNodesOfType(nodes, arangodb::aql::ExecutionNode::ENUMERATE_IRESEARCH_VIEW, true);
    REQUIRE(1 == nodes.size());
    auto* viewNode = arangodb::aql::ExecutionNode::castTo<arangodb::iresearch::IResearchViewNode*>(nodes.front());
    REQUIRE(viewNode);
    auto& scorers = viewNode->scorers();
    REQUIRE(2 == scorers.size());
    auto* var = scorers.front().var;
    REQUIRE(var);

    // check scorers
    {
      auto* expr = scorers[0].node;
      REQUIRE(expr);
      REQUIRE(arangodb::aql::NODE_TYPE_FCALL == expr->type);
      auto* fn = static_cast<arangodb::aql::Function*>(expr->getData());
      REQUIRE(fn);
      REQUIRE(arangodb::iresearch::isScorer(*fn));
      CHECK("CUSTOMSCORER" == fn->name);

      REQUIRE(1 == expr->numMembers());
      auto* args = expr->getMember(0);
      REQUIRE(args);
      REQUIRE(arangodb::aql::NODE_TYPE_ARRAY == args->type);
      REQUIRE(2 == args->numMembers());
      auto* arg0 = args->getMember(0); // reference to d
      REQUIRE(arg0);
      REQUIRE(static_cast<void const*>(&viewNode->outVariable()) == arg0->getData());
      auto* arg1 = args->getMember(1);
      REQUIRE(arg1);
      CHECK(arangodb::aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_EQ == arg1->type);
    }

    {
      auto* expr = scorers[1].node;
      REQUIRE(expr);
      REQUIRE(arangodb::aql::NODE_TYPE_FCALL == expr->type);
      auto* fn = static_cast<arangodb::aql::Function*>(expr->getData());
      REQUIRE(fn);
      REQUIRE(arangodb::iresearch::isScorer(*fn));
      CHECK("CUSTOMSCORER" == fn->name);

      REQUIRE(1 == expr->numMembers());
      auto* args = expr->getMember(0);
      REQUIRE(args);
      REQUIRE(arangodb::aql::NODE_TYPE_ARRAY == args->type);
      REQUIRE(2 == args->numMembers());
      auto* arg0 = args->getMember(0); // reference to d
      REQUIRE(arg0);
      REQUIRE(static_cast<void const*>(&viewNode->outVariable()) == arg0->getData());
      auto* arg1 = args->getMember(1);
      REQUIRE(arg1);
      CHECK(arangodb::aql::NODE_TYPE_OPERATOR_BINARY_ARRAY_EQ == arg1->type);
    }
  }

  // con't deduplicate scorers with default values
  {
    std::string const queryString =
      "FOR d IN testView SEARCH IN_RANGE(d.name, 'A', 'C', true, true) "
      "RETURN [ tfidf(d), tfidf(d, false) ] ";

    CHECK(arangodb::tests::assertRules(
      vocbase, queryString, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule,
      }
    ));

    arangodb::aql::Query query(
      false,
      vocbase,
      arangodb::aql::QueryString(queryString),
      std::shared_ptr<arangodb::velocypack::Builder>(),
      arangodb::velocypack::Parser::fromJson("{}"),
      arangodb::aql::PART_MAIN
    );

    query.prepare(arangodb::QueryRegistryFeature::registry());
    auto* plan = query.plan();
    REQUIRE(plan);

    arangodb::SmallVector<arangodb::aql::ExecutionNode*>::allocator_type::arena_type a;
    arangodb::SmallVector<arangodb::aql::ExecutionNode*> nodes{a};

    // 2 scorers scorer
    plan->findNodesOfType(nodes, arangodb::aql::ExecutionNode::ENUMERATE_IRESEARCH_VIEW, true);
    REQUIRE(1 == nodes.size());
    auto* viewNode = arangodb::aql::ExecutionNode::castTo<arangodb::iresearch::IResearchViewNode*>(nodes.front());
    REQUIRE(viewNode);
    auto scorers = viewNode->scorers();
    std::sort(
      scorers.begin(), scorers.end(),
      [](auto const& lhs, auto const& rhs) noexcept {
        return lhs.var->name < rhs.var->name;
    });

    // check "tfidf(d)" scorer
    {
      auto* expr = scorers[0].node;
      REQUIRE(expr);
      REQUIRE(arangodb::aql::NODE_TYPE_FCALL == expr->type);
      auto* fn = static_cast<arangodb::aql::Function*>(expr->getData());
      REQUIRE(fn);
      REQUIRE(arangodb::iresearch::isScorer(*fn));
      CHECK("TFIDF" == fn->name);

      REQUIRE(1 == expr->numMembers());
      auto* args = expr->getMember(0);
      REQUIRE(args);
      REQUIRE(arangodb::aql::NODE_TYPE_ARRAY == args->type);
      REQUIRE(1 == args->numMembers());
      auto* arg0 = args->getMember(0); // reference to d
      REQUIRE(arg0);
      REQUIRE(static_cast<void const*>(&viewNode->outVariable()) == arg0->getData());
    }

    // check "tfidf(d, false)" scorer
    {
      auto* expr = scorers[1].node;
      REQUIRE(expr);
      REQUIRE(arangodb::aql::NODE_TYPE_FCALL == expr->type);
      auto* fn = static_cast<arangodb::aql::Function*>(expr->getData());
      REQUIRE(fn);
      REQUIRE(arangodb::iresearch::isScorer(*fn));
      CHECK("TFIDF" == fn->name);

      REQUIRE(1 == expr->numMembers());
      auto* args = expr->getMember(0);
      REQUIRE(args);
      REQUIRE(arangodb::aql::NODE_TYPE_ARRAY == args->type);
      REQUIRE(2 == args->numMembers());
      auto* arg0 = args->getMember(0); // reference to d
      REQUIRE(arg0);
      REQUIRE(static_cast<void const*>(&viewNode->outVariable()) == arg0->getData());
      auto* arg1 = args->getMember(1);
      REQUIRE(arg1);
      REQUIRE(arangodb::aql::NODE_TYPE_VALUE == arg1->type);
      REQUIRE(arangodb::aql::VALUE_TYPE_BOOL == arg1->value.type);
      REQUIRE(false == arg1->getBoolValue());
    }

    // and 2 references
    nodes.clear();
    plan->findNodesOfType(nodes, arangodb::aql::ExecutionNode::CALCULATION, true);
    REQUIRE(1 == nodes.size());
    auto* calcNode = arangodb::aql::ExecutionNode::castTo<arangodb::aql::CalculationNode*>(nodes.front());
    REQUIRE(calcNode);
    REQUIRE(calcNode->expression());
    auto* node = calcNode->expression()->node();
    REQUIRE(node);
    REQUIRE(arangodb::aql::NODE_TYPE_ARRAY == node->type);
    REQUIRE(2 == node->numMembers());

    for (size_t i = 0; i < node->numMembers(); ++i) {
      auto* sub = node->getMember(i);
      REQUIRE(arangodb::aql::NODE_TYPE_REFERENCE == sub->type);
      CHECK(static_cast<const void*>(scorers[i].var) == sub->getData());
    }
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
