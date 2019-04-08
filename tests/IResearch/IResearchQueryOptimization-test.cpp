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

#include "../Mocks/StorageEngineMock.h"

#if USE_ENTERPRISE
  #include "Enterprise/Ldap/LdapFeature.h"
#endif

#include "Aql/Ast.h"
#include "Aql/ExpressionContext.h"
#include "Aql/ExecutionNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/QueryRegistry.h"
#include "Aql/Query.h"
#include "Cluster/ClusterFeature.h"
#include "IResearch/IResearchLink.h"
#include "IResearch/IResearchLinkHelper.h"
#include "V8/v8-globals.h"
#include "V8Server/V8DealerFeature.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/LogicalView.h"
#include "VocBase/ManagedDocumentResult.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/OperationOptions.h"
#include "Transaction/Methods.h"
#include "Aql/AqlFunctionFeature.h"
#include "Aql/OptimizerRulesFeature.h"
#include "GeneralServer/AuthenticationFeature.h"
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

#include "IResearch/VelocyPackHelper.h"
#include "analysis/analyzers.hpp"
#include "analysis/token_attributes.hpp"
#include "search/boolean_filter.hpp"
#include "search/term_filter.hpp"
#include "search/range_filter.hpp"
#include "utils/utf8_path.hpp"

#include <velocypack/Iterator.h>

extern const char* ARGV0; // defined in main.cpp

NS_LOCAL

bool findEmptyNodes(
    TRI_vocbase_t& vocbase,
    std::string const& queryString,
    std::shared_ptr<arangodb::velocypack::Builder> bindVars = nullptr
) {
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

  query.prepare(arangodb::QueryRegistryFeature::registry());

  arangodb::SmallVector<arangodb::aql::ExecutionNode*>::allocator_type::arena_type a;
  arangodb::SmallVector<arangodb::aql::ExecutionNode*> nodes{a};

  // try to find `EnumerateViewNode`s and process corresponding filters and sorts
  query.plan()->findNodesOfType(nodes, arangodb::aql::ExecutionNode::NORESULTS, true);
  return !nodes.empty();
}

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

struct IResearchQueryStringTermSetup {
  StorageEngineMock engine;
  arangodb::application_features::ApplicationServer server;
  std::vector<std::pair<arangodb::application_features::ApplicationFeature*, bool>> features;

  IResearchQueryStringTermSetup(): engine(server), server(nullptr, nullptr) {
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

  ~IResearchQueryStringTermSetup() {
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
}; // IResearchQuerySetup

NS_END

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief setup
////////////////////////////////////////////////////////////////////////////////

// dedicated to https://github.com/arangodb/arangodb/issues/8294
TEST_CASE("IResearchQueryTestOptimization", "[iresearch][iresearch-query]") {
  IResearchQueryStringTermSetup s;
  UNUSED(s);

  static std::vector<std::string> const EMPTY;

  auto createJson = arangodb::velocypack::Parser::fromJson("{ \
    \"name\": \"testView\", \
    \"type\": \"arangosearch\" \
  }");

  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
  std::shared_ptr<arangodb::LogicalCollection> logicalCollection1;
  std::shared_ptr<arangodb::LogicalCollection> logicalCollection2;

  // add collection_1
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"collection_1\" }");
    logicalCollection1 = vocbase.createCollection(collectionJson->slice());
    REQUIRE((nullptr != logicalCollection1));
  }

  // add view
  auto view = std::dynamic_pointer_cast<arangodb::iresearch::IResearchView>(
    vocbase.createView(createJson->slice())
  );
  REQUIRE((false == !view));

  // add link to collection
  {
    auto updateJson = arangodb::velocypack::Parser::fromJson(
      "{ \"links\" : {"
        "\"collection_1\" : { \"includeAllFields\" : true }"
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
    CHECK((true == tmpSlice.isObject() && 1 == tmpSlice.length()));
  }

  std::deque<arangodb::ManagedDocumentResult> insertedDocs;

  // populate view with the data
  {
    arangodb::OperationOptions opt;

    arangodb::transaction::Methods trx(
      arangodb::transaction::StandaloneContext::Create(vocbase),
      EMPTY, EMPTY, EMPTY,
      arangodb::transaction::Options()
    );
    CHECK((trx.begin().ok()));

    // insert into collection
    auto builder = arangodb::velocypack::Parser::fromJson(
     "[{ \"values\" : [ \"A\", \"C\", \"B\" ] }]");

    auto root = builder->slice();
    REQUIRE(root.isArray());

    for (auto doc : arangodb::velocypack::ArrayIterator(root)) {
      insertedDocs.emplace_back();
      auto const res = logicalCollection1->insert(&trx, doc, insertedDocs.back(), opt, false);
      CHECK(res.ok());
    }

    CHECK((trx.commit().ok()));
    CHECK((arangodb::iresearch::IResearchLinkHelper::find(*logicalCollection1, *view)->commit().ok()));
  }

  // a IN [ x ] && a == y, x < y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values IN [ '@', 'A' ] AND d.values == 'C' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& sub = root.add<irs::Or>();
        sub.add<irs::by_term>().field(mangleStringIdentity("values")).term("C");
        sub.add<irs::by_term>().field(mangleStringIdentity("values")).term("A");
      }
      root.add<irs::by_term>().field(mangleStringIdentity("values")).term("C");
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a IN [ x ] && a == y, x == y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values IN [ 'C', 'B', 'A' ] AND d.values == 'A' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    //FIXME
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& sub = root.add<irs::Or>();
        sub.add<irs::by_term>().field(mangleStringIdentity("values")).term("C");
        sub.add<irs::by_term>().field(mangleStringIdentity("values")).term("B");
        sub.add<irs::by_term>().field(mangleStringIdentity("values")).term("A");
      }
      root.add<irs::by_term>().field(mangleStringIdentity("values")).term("A");
    }
    //{
    //  irs::Or expected;
    //  auto& root = expected.add<irs::And>();
    //  {
    //    auto& sub = root.add<irs::Or>();
    //    sub.add<irs::by_term>().field(mangleStringIdentity("values")).term("C");
    //    sub.add<irs::by_term>().field(mangleStringIdentity("values")).term("B");
    //  }
    //  root.add<irs::by_term>().field(mangleStringIdentity("values")).term("A");
    //}

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a IN [ x ] && a == y, x > y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values IN [ 'C', 'B' ] AND d.values == 'A' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& sub = root.add<irs::Or>();
        sub.add<irs::by_term>().field(mangleStringIdentity("values")).term("C");
        sub.add<irs::by_term>().field(mangleStringIdentity("values")).term("B");
      }
      root.add<irs::by_term>().field(mangleStringIdentity("values")).term("A");
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a IN [ x ] && a == y, x < y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values IN [ '@', 'A' ] AND d.values != 'D' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& sub = root.add<irs::Or>();
        sub.add<irs::by_term>().field(mangleStringIdentity("values")).term("@");
        sub.add<irs::by_term>().field(mangleStringIdentity("values")).term("A");
      }
      root.add<irs::Not>().filter<irs::by_term>().field(mangleStringIdentity("values")).term("B");
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a IN [ x ] && a == y, x < y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values IN [ '@', 'A' ] AND d.values != 'B' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& sub = root.add<irs::Or>();
        sub.add<irs::by_term>().field(mangleStringIdentity("values")).term("@");
        sub.add<irs::by_term>().field(mangleStringIdentity("values")).term("A");
      }
      root.add<irs::Not>().filter<irs::by_term>().field(mangleStringIdentity("values")).term("B");
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a IN [ x ] && a == y, x > y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values IN [ 'C', 'D' ] AND d.values != 'D' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // FIXME
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& sub = root.add<irs::Or>();
        sub.add<irs::by_term>().field(mangleStringIdentity("values")).term("C");
        sub.add<irs::by_term>().field(mangleStringIdentity("values")).term("A");
      }
      root.add<irs::Not>().filter<irs::by_term>().field(mangleStringIdentity("values")).term("A");
    }
    //{
    //  irs::Or expected;
    //  auto& root = expected.add<irs::And>();
    //  {
    //    auto& sub = root.add<irs::Or>();
    //    sub.add<irs::by_term>().field(mangleStringIdentity("values")).term("C");
    //  }
    //  root.add<irs::Not>().filter<irs::by_term>().field(mangleStringIdentity("values")).term("A");
    //}

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  /*
  //FIXME
  // a IN [ x ] && a == y, x == y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values IN [ 'A', 'A' ] AND d.values != 'A' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(findEmptyNodes(vocbase, query));

    std::vector<arangodb::velocypack::Slice> expectedDocs {
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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
  */

  // a IN [ x ] && a != y, x > y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values IN [ 'C', 'B' ] AND d.values != 'A' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& sub = root.add<irs::Or>();
        sub.add<irs::by_term>().field(mangleStringIdentity("values")).term("C");
        sub.add<irs::by_term>().field(mangleStringIdentity("values")).term("B");
      }
      root.add<irs::Not>().filter<irs::by_term>().field(mangleStringIdentity("values")).term("A");
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a IN [ x ] && a != y, x > y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values IN [ 'C', 'B' ] AND d.values != '@' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& sub = root.add<irs::Or>();
        sub.add<irs::by_term>().field(mangleStringIdentity("values")).term("C");
        sub.add<irs::by_term>().field(mangleStringIdentity("values")).term("B");
      }
      root.add<irs::Not>().filter<irs::by_term>().field(mangleStringIdentity("values")).term("@");
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a IN [ x ] && a < y, x < y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values IN [ 'A', 'B' ] AND d.values < 'C' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    //FIXME
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& sub = root.add<irs::Or>();
        sub.add<irs::by_term>().field(mangleStringIdentity("values")).term("A");
        sub.add<irs::by_term>().field(mangleStringIdentity("values")).term("B");
      }
      root.add<irs::by_range>()
          .field(mangleStringIdentity("values"))
          .include<irs::Bound::MAX>(false)
          .term<irs::Bound::MAX>("C");
    }
    //{
    //  irs::Or expected;
    //  {
    //    auto& sub = root.add<irs::Or>();
    //    sub.add<irs::by_term>().field(mangleStringIdentity("values")).term("A");
    //    sub.add<irs::by_term>().field(mangleStringIdentity("values")).term("B");
    //  }
    //}

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a IN [ x ] && a < y, x == y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values IN [ 'A', 'C' ] AND d.values < 'C' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& sub = root.add<irs::Or>();
        sub.add<irs::by_term>().field(mangleStringIdentity("values")).term("A");
        sub.add<irs::by_term>().field(mangleStringIdentity("values")).term("C");
      }
      root.add<irs::by_range>()
          .field(mangleStringIdentity("values"))
          .include<irs::Bound::MAX>(false)
          .term<irs::Bound::MAX>("C");
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a IN [ x ] && a < y, x > y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values IN [ 'D', 'C' ] AND d.values < 'B' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& sub = root.add<irs::Or>();
        sub.add<irs::by_term>().field(mangleStringIdentity("values")).term("D");
        sub.add<irs::by_term>().field(mangleStringIdentity("values")).term("C");
      }
      root.add<irs::by_range>()
          .field(mangleStringIdentity("values"))
          .include<irs::Bound::MAX>(false)
          .term<irs::Bound::MAX>("B");
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a IN [ x ] && a <= y, x < y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values IN [ 'B', 'C' ] AND d.values <= 'D' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    //FIXME
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& sub = root.add<irs::Or>();
        sub.add<irs::by_term>().field(mangleStringIdentity("values")).term("B");
        sub.add<irs::by_term>().field(mangleStringIdentity("values")).term("C");
      }
      root.add<irs::by_range>()
          .field(mangleStringIdentity("values"))
          .include<irs::Bound::MAX>(true)
          .term<irs::Bound::MAX>("D");
    }
    //{
    //  irs::Or expected;
    //  auto& root = expected.add<irs::And>();
    //  {
    //    auto& sub = root.add<irs::Or>();
    //    sub.add<irs::by_term>().field(mangleStringIdentity("values")).term("B");
    //    sub.add<irs::by_term>().field(mangleStringIdentity("values")).term("D");
    //  }
    //}

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a IN [ x ] && a <= y, x == y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values IN [ 'B', 'C' ] AND d.values <= 'C' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    //FIXME
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& sub = root.add<irs::Or>();
        sub.add<irs::by_term>().field(mangleStringIdentity("values")).term("B");
        sub.add<irs::by_term>().field(mangleStringIdentity("values")).term("C");
      }
      root.add<irs::by_range>()
          .field(mangleStringIdentity("values"))
          .include<irs::Bound::MAX>(true)
          .term<irs::Bound::MAX>("C");
    }
    //{
    //  irs::Or expected;
    //  auto& root = expected.add<irs::And>();
    //  {
    //    auto& sub = root.add<irs::Or>();
    //    sub.add<irs::by_term>().field(mangleStringIdentity("values")).term("B");
    //    sub.add<irs::by_term>().field(mangleStringIdentity("values")).term("C");
    //  }
    //}

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a IN [ x ] && a <= y, x > y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values IN [ 'B', 'C' ] AND d.values <= 'A' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& sub = root.add<irs::Or>();
        sub.add<irs::by_term>().field(mangleStringIdentity("values")).term("B");
        sub.add<irs::by_term>().field(mangleStringIdentity("values")).term("C");
      }
      root.add<irs::by_range>()
          .field(mangleStringIdentity("values"))
          .include<irs::Bound::MAX>(true)
          .term<irs::Bound::MAX>("A");
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a IN [ x ] && a >= y, x < y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values IN [ '@', 'A' ] AND d.values >= 'B' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& sub = root.add<irs::Or>();
        sub.add<irs::by_term>().field(mangleStringIdentity("values")).term("@");
        sub.add<irs::by_term>().field(mangleStringIdentity("values")).term("A");
      }
      root.add<irs::by_range>()
          .field(mangleStringIdentity("values"))
          .include<irs::Bound::MIN>(true)
          .term<irs::Bound::MIN>("B");
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a IN [ x ] && a >= y, x == y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values IN [ '@', 'A' ] AND d.values >= 'A' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    //FIXME
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& sub = root.add<irs::Or>();
        sub.add<irs::by_term>().field(mangleStringIdentity("values")).term("@");
        sub.add<irs::by_term>().field(mangleStringIdentity("values")).term("A");
      }
      root.add<irs::by_range>()
          .field(mangleStringIdentity("values"))
          .include<irs::Bound::MIN>(true)
          .term<irs::Bound::MIN>("A");
    }
    //{
    //  irs::Or expected;
    //  auto& root = expected.add<irs::And>();
    //  root.add<irs::by_term>().field(mangleStringIdentity("values")).term("A");
    //}

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a IN [ x ] && a >= y, x > y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values IN [ 'C', 'D' ] AND d.values >= 'B' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    //FIXME
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& sub = root.add<irs::Or>();
        sub.add<irs::by_term>().field(mangleStringIdentity("values")).term("C");
        sub.add<irs::by_term>().field(mangleStringIdentity("values")).term("D");
      }
      root.add<irs::by_range>()
          .field(mangleStringIdentity("values"))
          .include<irs::Bound::MIN>(true)
          .term<irs::Bound::MIN>("B");
    }
    //{
    //  irs::Or expected;
    //  auto& root = expected.add<irs::And>();
    //  {
    //    auto& sub = root.add<irs::Or>();
    //    sub.add<irs::by_term>().field(mangleStringIdentity("values")).term("C");
    //    sub.add<irs::by_term>().field(mangleStringIdentity("values")).term("D");
    //  }
    //}

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a IN [ x ] && a > y, x < y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values IN [ '@', 'A' ] AND d.values > 'B' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& sub = root.add<irs::Or>();
        sub.add<irs::by_term>().field(mangleStringIdentity("values")).term("@");
        sub.add<irs::by_term>().field(mangleStringIdentity("values")).term("A");
      }
      root.add<irs::by_range>()
          .field(mangleStringIdentity("values"))
          .include<irs::Bound::MIN>(false)
          .term<irs::Bound::MIN>("B");
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a IN [ x ] && a > y, x == y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values IN [ 'C', 'B' ] AND d.values > 'B' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    //FIXME
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& sub = root.add<irs::Or>();
        sub.add<irs::by_term>().field(mangleStringIdentity("values")).term("C");
        sub.add<irs::by_term>().field(mangleStringIdentity("values")).term("B");
      }
      root.add<irs::by_range>()
          .field(mangleStringIdentity("values"))
          .include<irs::Bound::MIN>(false)
          .term<irs::Bound::MIN>("B");
    }
    //{
    //  irs::Or expected;
    //  auto& root = expected.add<irs::And>();
    //  root.add<irs::by_term>().field(mangleStringIdentity("values")).term("C");
    //}

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a IN [ x ] && a > y, x > y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values IN [ 'C', 'D' ] AND d.values > 'B' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    //FIXME
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& sub = root.add<irs::Or>();
        sub.add<irs::by_term>().field(mangleStringIdentity("values")).term("C");
        sub.add<irs::by_term>().field(mangleStringIdentity("values")).term("D");
      }
      root.add<irs::by_range>()
          .field(mangleStringIdentity("values"))
          .include<irs::Bound::MIN>(false)
          .term<irs::Bound::MIN>("B");
    }
    //{
    //  irs::Or expected;
    //  auto& root = expected.add<irs::And>();
    //  {
    //    auto& sub = root.add<irs::Or>();
    //    sub.add<irs::by_term>().field(mangleStringIdentity("values")).term("C");
    //    sub.add<irs::by_term>().field(mangleStringIdentity("values")).term("D");
    //  }
    //}

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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


  // a IN [ x ] && a IN [ y ]
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values IN [ 'A', 'B' ] AND d.values IN [ 'A', 'B', 'C' ] RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // FIXME optimize
    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& sub = root.add<irs::Or>();
        sub.add<irs::by_term>().field(mangleStringIdentity("values")).term("A");
        sub.add<irs::by_term>().field(mangleStringIdentity("values")).term("B");
      }
      {
        auto& sub = root.add<irs::Or>();
        sub.add<irs::by_term>().field(mangleStringIdentity("values")).term("A");
        sub.add<irs::by_term>().field(mangleStringIdentity("values")).term("B");
        sub.add<irs::by_term>().field(mangleStringIdentity("values")).term("C");
      }
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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


  // a IN [ x ] && a == y, x < y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values IN [ 'B' ] AND d.values == 'C' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::by_term>().field(mangleStringIdentity("values")).term("B");
      root.add<irs::by_term>().field(mangleStringIdentity("values")).term("C");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a IN [ x ] && a == y, x == y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values IN [ 'C' ] AND d.values == 'C' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::by_term>().field(mangleStringIdentity("values")).term("C");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a IN [ x ] && a == y, x > y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values IN [ 'C' ] AND d.values == 'B' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::by_term>().field(mangleStringIdentity("values")).term("C");
      root.add<irs::by_term>().field(mangleStringIdentity("values")).term("B");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a IN [ x ] && a != y, x < y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values IN [ 'A' ] AND d.values != 'B' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::Not>().filter<irs::by_term>().field(mangleStringIdentity("values")).term("B");
      root.add<irs::by_term>().field(mangleStringIdentity("values")).term("A");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a IN [ x ] && a != y, x == y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values IN [ 'C' ] AND d.values != 'C' RETURN d";

    //FIXME
    //CHECK(arangodb::tests::assertRules(
    //  vocbase, query, {
    //    arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
    //  }
    //));

    CHECK(findEmptyNodes(vocbase, query));

    std::vector<arangodb::velocypack::Slice> expectedDocs {
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a IN [ x ] && a != y, x > y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values IN ['B'] AND d.values != 'C' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::Not>().filter<irs::by_term>().field(mangleStringIdentity("values")).term("C");
      root.add<irs::by_term>().field(mangleStringIdentity("values")).term("B");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a IN [ x ] && a < y, x < y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values IN [ 'B' ] AND d.values < 'C' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::by_term>().field(mangleStringIdentity("values")).term("B");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a IN [ x ] && a < y, x == y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values IN [ 'C' ] AND d.values < 'C' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::by_term>().field(mangleStringIdentity("values")).term("C");
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MAX>(false).term<irs::Bound::MAX>("C");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a IN [ x ] && a < y, x > y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values IN [ 'C' ] AND d.values < 'B' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::by_term>().field(mangleStringIdentity("values")).term("C");
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MAX>(false).term<irs::Bound::MAX>("B");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a IN [ x ] && a <= y, x < y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values IN [ 'B' ] AND d.values <= 'C' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::by_term>().field(mangleStringIdentity("values")).term("B");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a IN [x] && a <= y, x == y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values IN [ 'B' ] AND d.values <= 'B' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::by_term>().field(mangleStringIdentity("values")).term("B");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a IN [ x ] && a <= y, x > y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values IN [ 'C' ] AND d.values <= 'B' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::by_term>().field(mangleStringIdentity("values")).term("C");
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MAX>(true).term<irs::Bound::MAX>("B");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a IN [ x ] && a >= y, x < y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values IN [ 'A' ] AND d.values >= 'B' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MIN>(true).term<irs::Bound::MIN>("B");
      root.add<irs::by_term>().field(mangleStringIdentity("values")).term("A");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a IN [ x ] && a >= y, x == y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values IN [ 'B' ] AND d.values >= 'B' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::by_term>().field(mangleStringIdentity("values")).term("B");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a IN [x] && a >= y, x > y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values IN ['C'] AND d.values >= 'B' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::by_term>().field(mangleStringIdentity("values")).term("C");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a IN [x] && a > y, x < y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values IN ['A'] AND d.values > 'B' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MIN>(false).term<irs::Bound::MIN>("B");
      root.add<irs::by_term>().field(mangleStringIdentity("values")).term("A");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a IN [x] && a > y, x == y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values IN ['B'] AND d.values > 'B' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MIN>(false).term<irs::Bound::MIN>("B");
      root.add<irs::by_term>().field(mangleStringIdentity("values")).term("B");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a IN [x] && a > y, x > y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values IN ['C'] AND d.values > 'B' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::by_term>().field(mangleStringIdentity("values")).term("C");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a == x && a == y, x < y
  {
    std::string const query = 
      "FOR d IN testView SEARCH d.values == 'B' AND d.values == 'C' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::by_term>().field(mangleStringIdentity("values")).term("B");
      root.add<irs::by_term>().field(mangleStringIdentity("values")).term("C");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a == x && a == y, x == y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values == 'C' AND d.values == 'C' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::by_term>().field(mangleStringIdentity("values")).term("C");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a == x && a == y, x > y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values == 'C' AND d.values == 'B' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::by_term>().field(mangleStringIdentity("values")).term("C");
      root.add<irs::by_term>().field(mangleStringIdentity("values")).term("B");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a == x && a != y, x < y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values == 'A' AND d.values != 'B' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::Not>().filter<irs::by_term>().field(mangleStringIdentity("values")).term("B");
      root.add<irs::by_term>().field(mangleStringIdentity("values")).term("A");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a == x && a != y, x == y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values == 'C' AND d.values != 'C' RETURN d";

    //FIXME
    //CHECK(arangodb::tests::assertRules(
    //  vocbase, query, {
    //    arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
    //  }
    //));

    CHECK(findEmptyNodes(vocbase, query));

    std::vector<arangodb::velocypack::Slice> expectedDocs {
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a == x && a != y, x > y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values == 'B' AND d.values != 'C' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::Not>().filter<irs::by_term>().field(mangleStringIdentity("values")).term("C");
      root.add<irs::by_term>().field(mangleStringIdentity("values")).term("B");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a == x && a < y, x < y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values == 'B' AND d.values < 'C' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::by_term>().field(mangleStringIdentity("values")).term("B");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a == x && a < y, x == y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values == 'C' AND d.values < 'C' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::by_term>().field(mangleStringIdentity("values")).term("C");
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MAX>(false).term<irs::Bound::MAX>("C");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a == x && a < y, x > y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values == 'C' AND d.values < 'B' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::by_term>().field(mangleStringIdentity("values")).term("C");
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MAX>(false).term<irs::Bound::MAX>("B");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a == x && a <= y, x < y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values == 'B' AND d.values <= 'C' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::by_term>().field(mangleStringIdentity("values")).term("B");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a == x && a <= y, x == y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values == 'B' AND d.values <= 'B' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::by_term>().field(mangleStringIdentity("values")).term("B");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a == x && a <= y, x > y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values == 'C' AND d.values <= 'B' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::by_term>().field(mangleStringIdentity("values")).term("C");
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MAX>(true).term<irs::Bound::MAX>("B");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a == x && a >= y, x < y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values == 'A' AND d.values >= 'B' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MIN>(true).term<irs::Bound::MIN>("B");
      root.add<irs::by_term>().field(mangleStringIdentity("values")).term("A");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a == x && a >= y, x == y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values == 'B' AND d.values >= 'B' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::by_term>().field(mangleStringIdentity("values")).term("B");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a == x && a >= y, x > y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values == 'C' AND d.values >= 'B' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::by_term>().field(mangleStringIdentity("values")).term("C");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a == x && a > y, x < y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values == 'A' AND d.values > 'B' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MIN>(false).term<irs::Bound::MIN>("B");
      root.add<irs::by_term>().field(mangleStringIdentity("values")).term("A");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a == x && a > y, x == y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values == 'B' AND d.values > 'B' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MIN>(false).term<irs::Bound::MIN>("B");
      root.add<irs::by_term>().field(mangleStringIdentity("values")).term("B");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a == x && a > y, x > y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values == 'C' AND d.values > 'A' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::by_term>().field(mangleStringIdentity("values")).term("C");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a != x && a == y, x < y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values != '@' AND d.values == 'B' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::Not>().filter<irs::by_term>().field(mangleStringIdentity("values")).term("@");
      root.add<irs::by_term>().field(mangleStringIdentity("values")).term("B");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a != x && a == y, x < y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values != 'A' AND d.values == 'B' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::Not>().filter<irs::by_term>().field(mangleStringIdentity("values")).term("A");
      root.add<irs::by_term>().field(mangleStringIdentity("values")).term("B");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a != x && a == y, x == y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values != 'A' AND d.values == 'A' RETURN d";

    //FIXME
    //CHECK(arangodb::tests::assertRules(
    //  vocbase, query, {
    //    arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
    //  }
    //));

    CHECK(findEmptyNodes(vocbase, query));

    std::vector<arangodb::velocypack::Slice> expectedDocs {
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a != x && a == y, x > y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values != 'D' AND d.values == 'A' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::Not>().filter<irs::by_term>().field(mangleStringIdentity("values")).term("D");
      root.add<irs::by_term>().field(mangleStringIdentity("values")).term("A");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a != x && a == y, x > y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values != 'B' AND d.values == 'A' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::Not>().filter<irs::by_term>().field(mangleStringIdentity("values")).term("B");
      root.add<irs::by_term>().field(mangleStringIdentity("values")).term("A");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a != x && a != y, x < y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values != '@' AND d.values != 'D' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::Not>().filter<irs::by_term>().field(mangleStringIdentity("values")).term("@");
      root.add<irs::Not>().filter<irs::by_term>().field(mangleStringIdentity("values")).term("D");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a != x && a != y, x < y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values != 'A' AND d.values != 'B' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::Not>().filter<irs::by_term>().field(mangleStringIdentity("values")).term("A");
      root.add<irs::Not>().filter<irs::by_term>().field(mangleStringIdentity("values")).term("B");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a != x && a != y, x == y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values != 'D' AND d.values != 'D' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::Not>().filter<irs::by_term>().field(mangleStringIdentity("values")).term("D");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a != x && a != y, x == y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values != 'A' AND d.values != 'A' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::Not>().filter<irs::by_term>().field(mangleStringIdentity("values")).term("A");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a != x && a != y, x > y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values != 'B' AND d.values != 'A' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::Not>().filter<irs::by_term>().field(mangleStringIdentity("values")).term("B");
      root.add<irs::Not>().filter<irs::by_term>().field(mangleStringIdentity("values")).term("A");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a != x && a < y, x < y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values != '0' AND d.values < 'C' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::Not>().filter<irs::by_term>().field(mangleStringIdentity("values")).term("0");
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MAX>(false).term<irs::Bound::MAX>("C");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a != x && a < y, x == y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values != 'A' AND d.values < 'B' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::Not>().filter<irs::by_term>().field(mangleStringIdentity("values")).term("A");
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MAX>(false).term<irs::Bound::MAX>("B");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a != x && a < y, x == y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values != '@' AND d.values < 'B' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::Not>().filter<irs::by_term>().field(mangleStringIdentity("values")).term("@");
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MAX>(false).term<irs::Bound::MAX>("B");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a != x && a < y, x == y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values != 'D' AND d.values < 'D' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::Not>().filter<irs::by_term>().field(mangleStringIdentity("values")).term("D");
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MAX>(false).term<irs::Bound::MAX>("D");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a != x && a < y, x > y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values != 'D' AND d.values < 'B' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::Not>().filter<irs::by_term>().field(mangleStringIdentity("values")).term("D");
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MAX>(false).term<irs::Bound::MAX>("B");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a != x && a < y, x > y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values != 'C' AND d.values < 'B' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::Not>().filter<irs::by_term>().field(mangleStringIdentity("values")).term("C");
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MAX>(false).term<irs::Bound::MAX>("B");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a != x && a <= y, x < y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values != '0' AND d.values <= 'B' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::Not>().filter<irs::by_term>().field(mangleStringIdentity("values")).term("0");
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MAX>(true).term<irs::Bound::MAX>("B");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a != x && a <= y, x < y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values != 'A' AND d.values <= 'B' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::Not>().filter<irs::by_term>().field(mangleStringIdentity("values")).term("A");
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MAX>(true).term<irs::Bound::MAX>("B");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a != x && a <= y, x == y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values != 'D' AND d.values <= 'D' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::Not>().filter<irs::by_term>().field(mangleStringIdentity("values")).term("D");
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MAX>(true).term<irs::Bound::MAX>("D");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a != x && a <= y, x == y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values != 'B' AND d.values <= 'B' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::Not>().filter<irs::by_term>().field(mangleStringIdentity("values")).term("B");
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MAX>(true).term<irs::Bound::MAX>("B");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a != x && a <= y, x > y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values != 'D' AND d.values <= 'B' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::Not>().filter<irs::by_term>().field(mangleStringIdentity("values")).term("D");
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MAX>(true).term<irs::Bound::MAX>("B");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a != x && a <= y, x > y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values != 'C' AND d.values <= 'B' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::Not>().filter<irs::by_term>().field(mangleStringIdentity("values")).term("C");
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MAX>(true).term<irs::Bound::MAX>("B");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a != x && a >= y, x < y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values != '0' AND d.values >= 'B' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::Not>().filter<irs::by_term>().field(mangleStringIdentity("values")).term("0");
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MIN>(true).term<irs::Bound::MIN>("B");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a != x && a >= y, x < y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values != 'A' AND d.values >= 'B' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::Not>().filter<irs::by_term>().field(mangleStringIdentity("values")).term("A");
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MIN>(true).term<irs::Bound::MIN>("B");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a != x && a >= y, x == y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values != '0' AND d.values >= '0' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::Not>().filter<irs::by_term>().field(mangleStringIdentity("values")).term("0");
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MIN>(true).term<irs::Bound::MIN>("0");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a != x && a >= y, x == y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values != 'A' AND d.values >= 'A' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::Not>().filter<irs::by_term>().field(mangleStringIdentity("values")).term("A");
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MIN>(true).term<irs::Bound::MIN>("A");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a != x && a >= y, x > y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values != 'D' AND d.values >= 'A' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::Not>().filter<irs::by_term>().field(mangleStringIdentity("values")).term("D");
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MIN>(true).term<irs::Bound::MIN>("A");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a != x && a >= y, x > y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values != 'C' AND d.values >= 'A' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::Not>().filter<irs::by_term>().field(mangleStringIdentity("values")).term("C");
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MIN>(true).term<irs::Bound::MIN>("A");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a != x && a > y, x < y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values != '0' AND d.values > 'B' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::Not>().filter<irs::by_term>().field(mangleStringIdentity("values")).term("0");
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MIN>(false).term<irs::Bound::MIN>("B");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a != x && a > y, x < y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values != 'A' AND d.values > 'B' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::Not>().filter<irs::by_term>().field(mangleStringIdentity("values")).term("A");
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MIN>(false).term<irs::Bound::MIN>("B");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a != x && a > y, x == y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values != '0' AND d.values > '0' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::Not>().filter<irs::by_term>().field(mangleStringIdentity("values")).term("0");
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MIN>(false).term<irs::Bound::MIN>("0");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a != x && a > y, x == y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values != 'A' AND d.values > 'A' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::Not>().filter<irs::by_term>().field(mangleStringIdentity("values")).term("A");
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MIN>(false).term<irs::Bound::MIN>("A");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a != x && a > y, x > y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values != 'D' AND d.values > 'A' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::Not>().filter<irs::by_term>().field(mangleStringIdentity("values")).term("D");
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MIN>(false).term<irs::Bound::MIN>("A");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a != x && a > y, x > y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values != 'C' AND d.values > 'A' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::Not>().filter<irs::by_term>().field(mangleStringIdentity("values")).term("C");
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MIN>(false).term<irs::Bound::MIN>("A");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a < x && a == y, x < y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values < 'B' AND d.values == 'C' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::by_term>().field(mangleStringIdentity("values")).term("C");
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MAX>(false).term<irs::Bound::MAX>("B");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a < x && a == y, x == y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values < 'B' AND d.values == 'B' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::by_term>().field(mangleStringIdentity("values")).term("B");
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MAX>(false).term<irs::Bound::MAX>("B");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a < x && a == y, x > y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values < 'C' AND d.values == 'B' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::by_term>().field(mangleStringIdentity("values")).term("B");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a < x && a != y, x < y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values < 'B' AND d.values != 'D' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::Not>().filter<irs::by_term>().field(mangleStringIdentity("values")).term("D");
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MAX>(false).term<irs::Bound::MAX>("B");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a < x && a != y, x < y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values < 'B' AND d.values != 'C' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::Not>().filter<irs::by_term>().field(mangleStringIdentity("values")).term("C");
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MAX>(false).term<irs::Bound::MAX>("B");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a < x && a != y, x == y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values < 'D' AND d.values != 'D' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::Not>().filter<irs::by_term>().field(mangleStringIdentity("values")).term("D");
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MAX>(false).term<irs::Bound::MAX>("D");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a < x && a != y, x == y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values < 'B' AND d.values != 'B' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::Not>().filter<irs::by_term>().field(mangleStringIdentity("values")).term("B");
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MAX>(false).term<irs::Bound::MAX>("B");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a < x && a != y, x > y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values < 'C' AND d.values != '0' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::Not>().filter<irs::by_term>().field(mangleStringIdentity("values")).term("0");
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MAX>(false).term<irs::Bound::MAX>("C");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a < x && a != y, x > y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values < 'C' AND d.values != 'B' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::Not>().filter<irs::by_term>().field(mangleStringIdentity("values")).term("B");
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MAX>(false).term<irs::Bound::MAX>("C");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a < x && a < y, x < y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values < 'B' AND d.values < 'C' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MAX>(false).term<irs::Bound::MAX>("B");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a < x && a < y, x == y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values < 'B' AND d.values < 'B' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MAX>(false).term<irs::Bound::MAX>("B");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a < x && a < y, x > y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values < 'C' AND d.values < 'B' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MAX>(false).term<irs::Bound::MAX>("B");
      assertFilterOptimized(vocbase, query, expected);
    }

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a < x && a <= y, x < y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values < 'B' AND d.values <= 'C' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MAX>(false).term<irs::Bound::MAX>("B");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a < x && a <= y, x == y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values < 'C' AND d.values <= 'C' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MAX>(false).term<irs::Bound::MAX>("C");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a < x && a <= y, x > y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values < 'C' AND d.values <= 'B' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MAX>(true).term<irs::Bound::MAX>("B");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a < x && a >= y, x < y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values < 'B' AND d.values >= 'C' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MIN>(true).term<irs::Bound::MIN>("C");
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MAX>(false).term<irs::Bound::MAX>("B");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a < x && a >= y, x == y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values < 'B' AND d.values >= 'B' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MIN>(true).term<irs::Bound::MIN>("B");
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MAX>(false).term<irs::Bound::MAX>("B");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a < x && a >= y, x > y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values < 'C' AND d.values >= 'B' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MIN>(true).term<irs::Bound::MIN>("B");
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MAX>(false).term<irs::Bound::MAX>("C");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a < x && a > y, x < y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values < 'B' AND d.values > 'C' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MIN>(false).term<irs::Bound::MIN>("C");
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MAX>(false).term<irs::Bound::MAX>("B");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a < x && a > y, x == y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values < 'B' AND d.values > 'B' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MIN>(false).term<irs::Bound::MIN>("B");
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MAX>(false).term<irs::Bound::MAX>("B");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a < x && a > y, x > y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values < 'C' AND d.values > 'A' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MIN>(false).term<irs::Bound::MIN>("A");
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MAX>(false).term<irs::Bound::MAX>("C");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a <= x && a == y, x < y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values <= 'A' AND d.values == 'B' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::by_term>().field(mangleStringIdentity("values")).term("B");
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MAX>(true).term<irs::Bound::MAX>("A");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a <= x && a == y, x == y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values <= 'A' AND d.values == 'A' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::by_term>().field(mangleStringIdentity("values")).term("A");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a <= x && a == y, x > y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values <= 'B' AND d.values == 'A' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::by_term>().field(mangleStringIdentity("values")).term("A");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a <= x && a != y, x < y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values <= 'A' AND d.values != 'D' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::Not>().filter<irs::by_term>().field(mangleStringIdentity("values")).term("D");
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MAX>(true).term<irs::Bound::MAX>("A");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a <= x && a != y, x < y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values <= 'A' AND d.values != 'B' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::Not>().filter<irs::by_term>().field(mangleStringIdentity("values")).term("B");
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MAX>(true).term<irs::Bound::MAX>("A");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a <= x && a != y, x == y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values <= 'B' AND d.values != 'B' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::Not>().filter<irs::by_term>().field(mangleStringIdentity("values")).term("B");
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MAX>(true).term<irs::Bound::MAX>("B");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a <= x && a != y, x == y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values <= 'D' AND d.values != 'D' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::Not>().filter<irs::by_term>().field(mangleStringIdentity("values")).term("D");
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MAX>(true).term<irs::Bound::MAX>("D");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a <= x && a != y, x > y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values <= 'C' AND d.values != '@' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::Not>().filter<irs::by_term>().field(mangleStringIdentity("values")).term("@");
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MAX>(true).term<irs::Bound::MAX>("C");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a <= x && a != y, x > y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values <= 'C' AND d.values != 'B' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::Not>().filter<irs::by_term>().field(mangleStringIdentity("values")).term("B");
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MAX>(true).term<irs::Bound::MAX>("C");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a <= x && a < y, x < y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values <= 'A' AND d.values < 'B' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MAX>(true).term<irs::Bound::MAX>("A");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a <= x && a < y, x == y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values <= 'B' AND d.values < 'B' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MAX>(false).term<irs::Bound::MAX>("B");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a <= x && a < y, x > y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values <= 'C' AND d.values < 'B' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MAX>(false).term<irs::Bound::MAX>("B");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a <= x && a <= y, x < y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values <= 'A' AND d.values <= 'B' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MAX>(true).term<irs::Bound::MAX>("A");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a <= x && a <= y, x == y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values <= 'B' AND d.values <= 'B' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MAX>(true).term<irs::Bound::MAX>("B");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a <= x && a <= y, x > y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values <= 'C' AND d.values <= 'B' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MAX>(true).term<irs::Bound::MAX>("B");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a <= x && a >= y, x < y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values <= 'A' AND d.values >= 'B' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MIN>(true).term<irs::Bound::MIN>("B");
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MAX>(true).term<irs::Bound::MAX>("A");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a <= x && a >= y, x == y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values <= 'A' AND d.values >= 'A' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MIN>(true).term<irs::Bound::MIN>("A");
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MAX>(true).term<irs::Bound::MAX>("A");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a <= x && a >= y, x > y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values <= 'C' AND d.values >= 'A' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MIN>(true).term<irs::Bound::MIN>("A");
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MAX>(true).term<irs::Bound::MAX>("C");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a <= x && a > y, x < y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values <= 'A' AND d.values > 'B' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MIN>(false).term<irs::Bound::MIN>("B");
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MAX>(true).term<irs::Bound::MAX>("A");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a <= x && a > y, x == y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values <= 'A' AND d.values > 'A' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MIN>(false).term<irs::Bound::MIN>("A");
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MAX>(true).term<irs::Bound::MAX>("A");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a <= x && a > y, x > y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values <= 'C' AND d.values > 'A' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MIN>(false).term<irs::Bound::MIN>("A");
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MAX>(true).term<irs::Bound::MAX>("C");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a >= x && a == y, x < y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values >= 'A' AND d.values == 'B' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::by_term>().field(mangleStringIdentity("values")).term("B");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a >= x && a == y, x == y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values >= 'A' AND d.values == 'A' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::by_term>().field(mangleStringIdentity("values")).term("A");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a >= x && a == y, x > y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values >= 'B' AND d.values == 'A' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MIN>(true).term<irs::Bound::MIN>("B");
      root.add<irs::by_term>().field(mangleStringIdentity("values")).term("A");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a >= x && a != y, x < y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values >= 'A' AND d.values != 'D' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::Not>().filter<irs::by_term>().field(mangleStringIdentity("values")).term("D");
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MIN>(true).term<irs::Bound::MIN>("A");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a >= x && a != y, x < y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values >= 'A' AND d.values != 'B' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::Not>().filter<irs::by_term>().field(mangleStringIdentity("values")).term("B");
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MIN>(true).term<irs::Bound::MIN>("A");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a >= x && a != y, x == y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values >= '@' AND d.values != '@' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::Not>().filter<irs::by_term>().field(mangleStringIdentity("values")).term("@");
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MIN>(true).term<irs::Bound::MIN>("@");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a >= x && a != y, x == y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values >= 'A' AND d.values != 'A' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::Not>().filter<irs::by_term>().field(mangleStringIdentity("values")).term("A");
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MIN>(true).term<irs::Bound::MIN>("A");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a >= x && a != y, x > y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values >= 'B' AND d.values != 'D' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::Not>().filter<irs::by_term>().field(mangleStringIdentity("values")).term("D");
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MIN>(true).term<irs::Bound::MIN>("B");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a >= x && a != y, x > y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values >= 'B' AND d.values != 'A' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::Not>().filter<irs::by_term>().field(mangleStringIdentity("values")).term("A");
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MIN>(true).term<irs::Bound::MIN>("B");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a >= x && a < y, x < y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values >= 'A' AND d.values < 'B' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MIN>(true).term<irs::Bound::MIN>("A");
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MAX>(false).term<irs::Bound::MAX>("B");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a >= x && a < y, x == y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values >= 'B' AND d.values < 'B' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MIN>(true).term<irs::Bound::MIN>("B");
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MAX>(false).term<irs::Bound::MAX>("B");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a >= x && a < y, x > y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values >= 'C' AND d.values < 'B' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MIN>(true).term<irs::Bound::MIN>("C");
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MAX>(false).term<irs::Bound::MAX>("B");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a >= x && a <= y, x < y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values >= 'A' AND d.values <= 'B' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MIN>(true).term<irs::Bound::MIN>("A");
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MAX>(true).term<irs::Bound::MAX>("B");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a >= x && a <= y, x == y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values >= 'B' AND d.values <= 'B' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MIN>(true).term<irs::Bound::MIN>("B");
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MAX>(true).term<irs::Bound::MAX>("B");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a >= x && a <= y, x > y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values >= 'C' AND d.values <= 'A' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MIN>(true).term<irs::Bound::MIN>("C");
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MAX>(true).term<irs::Bound::MAX>("A");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a >= x && a >= y, x < y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values >= 'A' AND d.values >= 'B' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MIN>(true).term<irs::Bound::MIN>("B");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a >= x && a >= y, x == y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values >= 'B' AND d.values >= 'B' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MIN>(true).term<irs::Bound::MIN>("B");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a >= x && a >= y, x > y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values >= 'C' AND d.values >= 'B' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MIN>(true).term<irs::Bound::MIN>("C");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a >= x && a > y, x < y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values >= 'A' AND d.values > 'B' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MIN>(false).term<irs::Bound::MIN>("B");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a >= x && a > y, x == y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values >= 'B' AND d.values > 'B' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MIN>(false).term<irs::Bound::MIN>("B");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a >= x && a > y, x > y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values >= 'B' AND d.values > 'A' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MIN>(true).term<irs::Bound::MIN>("B");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a > x && a == y, x < y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values > 'A' AND d.values == 'B' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::by_term>().field(mangleStringIdentity("values")).term("B");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a > x && a == y, x == y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values > 'B' AND d.values == 'B' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MIN>(false).term<irs::Bound::MIN>("B");
      root.add<irs::by_term>().field(mangleStringIdentity("values")).term("B");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a > x && a == y, x > y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values > 'B' AND d.values == 'A' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MIN>(false).term<irs::Bound::MIN>("B");
      root.add<irs::by_term>().field(mangleStringIdentity("values")).term("A");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a > x && a != y, x < y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values > 'A' AND d.values != 'D' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::Not>().filter<irs::by_term>().field(mangleStringIdentity("values")).term("D");
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MIN>(false).term<irs::Bound::MIN>("A");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a > x && a != y, x < y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values > 'A' AND d.values != 'B' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::Not>().filter<irs::by_term>().field(mangleStringIdentity("values")).term("B");
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MIN>(false).term<irs::Bound::MIN>("A");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a > x && a != y, x == y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values > '@' AND d.values != '@' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::Not>().filter<irs::by_term>().field(mangleStringIdentity("values")).term("@");
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MIN>(false).term<irs::Bound::MIN>("@");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a > x && a != y, x == y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values > 'A' AND d.values != 'A' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::Not>().filter<irs::by_term>().field(mangleStringIdentity("values")).term("A");
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MIN>(false).term<irs::Bound::MIN>("A");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a > x && a != y, x > y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values > 'B' AND d.values != '@' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::Not>().filter<irs::by_term>().field(mangleStringIdentity("values")).term("@");
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MIN>(false).term<irs::Bound::MIN>("B");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a > x && a != y, x > y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values > 'B' AND d.values != 'A' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::Not>().filter<irs::by_term>().field(mangleStringIdentity("values")).term("A");
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MIN>(false).term<irs::Bound::MIN>("B");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a > x && a < y, x < y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values > 'A' AND d.values < 'C' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MIN>(false).term<irs::Bound::MIN>("A");
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MAX>(false).term<irs::Bound::MAX>("C");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a > x && a < y, x == y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values > 'B' AND d.values < 'B' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MIN>(false).term<irs::Bound::MIN>("B");
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MAX>(false).term<irs::Bound::MAX>("B");
      assertFilterOptimized(vocbase, query, expected);
    }

    CHECK(!findEmptyNodes(vocbase, query));

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a > x && a < y, x > y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values > 'C' AND d.values < 'A' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MIN>(false).term<irs::Bound::MIN>("C");
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MAX>(false).term<irs::Bound::MAX>("A");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a > x && a <= y, x < y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values > 'A' AND d.values <= 'B' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MIN>(false).term<irs::Bound::MIN>("A");
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MAX>(true).term<irs::Bound::MAX>("B");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a > x && a <= y, x == y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values > 'B' AND d.values <= 'B' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MIN>(false).term<irs::Bound::MIN>("B");
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MAX>(true).term<irs::Bound::MAX>("B");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a > x && a <= y, x > y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values > 'B' AND d.values <= 'A' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MIN>(false).term<irs::Bound::MIN>("B");
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MAX>(true).term<irs::Bound::MAX>("A");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a > x && a >= y, x < y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values > 'A' AND d.values >= 'B' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MIN>(true).term<irs::Bound::MIN>("B");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a > x && a >= y, x == y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values > 'B' AND d.values >= 'B' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MIN>(false).term<irs::Bound::MIN>("B");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a > x && a >= y, x > y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values > 'B' AND d.values >= 'A' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MIN>(false).term<irs::Bound::MIN>("B");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a > x && a > y, x < y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values > 'A' AND d.values > 'B' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MIN>(false).term<irs::Bound::MIN>("B");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a > x && a > y, x == y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values > 'B' AND d.values > 'B' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MIN>(false).term<irs::Bound::MIN>("B");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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

  // a > x && a > y, x > y
  {
    std::string const query =
      "FOR d IN testView SEARCH d.values > 'B' AND d.values > 'A' RETURN d";

    CHECK(arangodb::tests::assertRules(
      vocbase, query, {
        arangodb::aql::OptimizerRule::handleArangoSearchViewsRule
      }
    ));

    CHECK(!findEmptyNodes(vocbase, query));

    // check structure
    {
      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<irs::by_range>().field(mangleStringIdentity("values")).include<irs::Bound::MIN>(false).term<irs::Bound::MIN>("B");
      assertFilterOptimized(vocbase, query, expected);
    }

    std::vector<arangodb::velocypack::Slice> expectedDocs {
      arangodb::velocypack::Slice(insertedDocs[0].vpack()),
    };

    auto queryResult = arangodb::tests::executeQuery(vocbase, query);
    REQUIRE(queryResult.result.ok());

    auto result = queryResult.data->slice();
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
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
