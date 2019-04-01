//////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 EMC Corporation
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
/// Copyright holder is EMC Corporation
///
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include "catch.hpp"
#include "common.h"
#include "ExpressionContextMock.h"
#include "../Mocks/StorageEngineMock.h"

#if USE_ENTERPRISE
  #include "Enterprise/Ldap/LdapFeature.h"
#endif

#include "Aql/AqlFunctionFeature.h"
#include "Aql/Ast.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/ExpressionContext.h"
#include "Aql/Query.h"
#include "Cluster/ClusterFeature.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "IResearch/IResearchCommon.h"
#include "IResearch/IResearchFeature.h"
#include "IResearch/IResearchFilterFactory.h"
#include "IResearch/IResearchLinkMeta.h"
#include "IResearch/IResearchViewMeta.h"
#include "IResearch/IResearchAnalyzerFeature.h"
#include "IResearch/ExpressionFilter.h"
#include "IResearch/AqlHelper.h"
#include "Logger/Logger.h"
#include "Logger/LogTopic.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "Transaction/Methods.h"
#include "Transaction/StandaloneContext.h"
#include "RestServer/AqlFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "RestServer/SystemDatabaseFeature.h"
#include "RestServer/TraverserEngineRegistryFeature.h"
#include "RestServer/ViewTypesFeature.h"
#include "Sharding/ShardingFeature.h"
#include "V8Server/V8DealerFeature.h"

#include "analysis/analyzers.hpp"
#include "analysis/token_streams.hpp"
#include "analysis/token_attributes.hpp"
#include "search/term_filter.hpp"
#include "search/all_filter.hpp"
#include "search/prefix_filter.hpp"
#include "search/range_filter.hpp"
#include "search/granular_range_filter.hpp"
#include "search/column_existence_filter.hpp"
#include "search/boolean_filter.hpp"
#include "search/phrase_filter.hpp"

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

struct IResearchFilterFunctionSetup {
  StorageEngineMock engine;
  arangodb::application_features::ApplicationServer server;
  std::vector<std::pair<arangodb::application_features::ApplicationFeature*, bool>> features;

  IResearchFilterFunctionSetup(): engine(server), server(nullptr, nullptr) {
    arangodb::EngineSelectorFeature::ENGINE = &engine;
    arangodb::aql::AqlFunctionFeature* functions = nullptr;

    arangodb::tests::init();

    // suppress INFO {authentication} Authentication is turned on (system only), authentication for unix sockets is turned on
    // suppress WARNING {authentication} --server.jwt-secret is insecure. Use --server.jwt-secret-keyfile instead
    arangodb::LogTopic::setLogLevel(arangodb::Logger::AUTHENTICATION.name(), arangodb::LogLevel::ERR);

    // suppress log messages since tests check error conditions
    arangodb::LogTopic::setLogLevel(arangodb::iresearch::TOPIC.name(), arangodb::LogLevel::FATAL);
    irs::logger::output_le(iresearch::logger::IRL_FATAL, stderr);

    // setup required application features
    features.emplace_back(new arangodb::AuthenticationFeature(server), true);
    features.emplace_back(new arangodb::DatabaseFeature(server), false);
    features.emplace_back(new arangodb::QueryRegistryFeature(server), false); // must be first
    features.emplace_back(new arangodb::ShardingFeature(server), false);
    arangodb::application_features::ApplicationServer::server->addFeature(features.back().first); // need QueryRegistryFeature feature to be added now in order to create the system database
    features.emplace_back(new arangodb::SystemDatabaseFeature(server), true); // required for IResearchAnalyzerFeature
    features.emplace_back(new arangodb::TraverserEngineRegistryFeature(server), false); // must be before AqlFeature
    features.emplace_back(new arangodb::V8DealerFeature(server), false); // required for DatabaseFeature::createDatabase(...)
    features.emplace_back(new arangodb::ViewTypesFeature(server), false); // required for IResearchFeature
    features.emplace_back(new arangodb::AqlFeature(server), true);
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
  }

  ~IResearchFilterFunctionSetup() {
    arangodb::AqlFeature(server).stop(); // unset singleton instance
    arangodb::LogTopic::setLogLevel(arangodb::iresearch::TOPIC.name(), arangodb::LogLevel::DEFAULT);
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
}; // IResearchFilterSetup

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief setup
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("IResearchFilterFunctionTest", "[iresearch][iresearch-filter]") {
  IResearchFilterFunctionSetup s;
  UNUSED(s);

SECTION("AttributeAccess") {
  // attribute access, non empty object
  {
    auto obj = arangodb::velocypack::Parser::fromJson("{ \"a\": { \"b\": \"1\" } }");

    ExpressionContextMock ctx;
    ctx.vars.emplace("x", arangodb::aql::AqlValue(obj->slice()));

    irs::Or expected;
    expected.add<irs::all>();

    assertFilterSuccess("LET x={} FOR d IN collection FILTER x.a.b RETURN d", expected, &ctx);
  }

  // attribute access, non empty object, boost
  {
    auto obj = arangodb::velocypack::Parser::fromJson("{ \"a\": { \"b\": \"1\" } }");

    ExpressionContextMock ctx;
    ctx.vars.emplace("x", arangodb::aql::AqlValue(obj->slice()));

    irs::Or expected;
    expected.add<irs::all>().boost(1.5f);

    assertFilterSuccess("LET x={} FOR d IN collection FILTER BOOST(x.a.b, 1.5) RETURN d", expected, &ctx);
  }

  // attribute access, empty object
  {
    auto obj = arangodb::velocypack::Parser::fromJson("{}");

    ExpressionContextMock ctx;
    ctx.vars.emplace("x", arangodb::aql::AqlValue(obj->slice()));

    irs::Or expected;
    expected.add<irs::empty>();

    assertFilterSuccess("LET x={} FOR d IN collection FILTER x.a.b RETURN d", expected, &ctx);
  }

  // attribute access, empty object, boost
  {
    auto obj = arangodb::velocypack::Parser::fromJson("{}");

    ExpressionContextMock ctx;
    ctx.vars.emplace("x", arangodb::aql::AqlValue(obj->slice()));

    irs::Or expected;
    expected.add<irs::empty>();

    assertFilterSuccess("LET x={} FOR d IN collection FILTER BOOST(x.a.b, 2.5) RETURN d", expected, &ctx);
  }

  assertExpressionFilter("FOR d IN collection FILTER d RETURN d"); // no reference to `d`
  assertExpressionFilter("FOR d IN collection FILTER ANALYZER(d, 'test_analyzer') RETURN d", 1, wrappedExpressionExtractor); // no reference to `d`
  assertExpressionFilter("FOR d IN collection FILTER BOOST(d, 1.5) RETURN d", 1.5, wrappedExpressionExtractor); // no reference to `d`
  assertExpressionFilter("FOR d IN collection FILTER d.a.b.c RETURN d"); // no reference to `d`
  assertExpressionFilter("FOR d IN collection FILTER d.a.b.c RETURN d"); // no reference to `d`
  assertExpressionFilter("FOR d IN collection FILTER BOOST(d.a.b.c, 2.5) RETURN d", 2.5, wrappedExpressionExtractor); // no reference to `d`
  assertExpressionFilter("FOR d IN collection FILTER ANALYZER(d.a.b[TO_STRING('c')], 'test_analyzer') RETURN d", 1, wrappedExpressionExtractor); // no reference to `d`
  assertExpressionFilter("FOR d IN collection FILTER BOOST(d.a.b[TO_STRING('c')], 3.5) RETURN d", 3.5, wrappedExpressionExtractor); // no reference to `d`

  // nondeterministic expression -> wrap it
  assertExpressionFilter("FOR d IN collection FILTER d.a.b[_NONDETERM_('c')] RETURN d");
  assertExpressionFilter("FOR d IN collection FILTER ANALYZER(d.a.b[_NONDETERM_('c')], 'test_analyzer') RETURN d", 1.0, wrappedExpressionExtractor);
  assertExpressionFilter("FOR d IN collection FILTER BOOST(d.a.b[_NONDETERM_('c')], 1.5) RETURN d", 1.5, wrappedExpressionExtractor);
}

SECTION("ValueReference") {
  // string value == true
  {
    irs::Or expected;
    expected.add<irs::all>();

    assertFilterSuccess("FOR d IN collection FILTER '1' RETURN d", expected);
  }

  // string reference
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("x", arangodb::aql::AqlValue(arangodb::aql::AqlValue("abc")));

    irs::Or expected;
    expected.add<irs::all>();

    assertFilterSuccess("LET x='abc' FOR d IN collection FILTER x RETURN d", expected, &ctx); // reference
  }

  // string empty value == false
  {
    irs::Or expected;
    expected.add<irs::empty>();

    assertFilterSuccess("FOR d IN collection FILTER '' RETURN d", expected);
  }

  // empty string reference false
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("x", arangodb::aql::AqlValue(arangodb::aql::AqlValue("")));

    irs::Or expected;
    expected.add<irs::empty>();

    assertFilterSuccess("LET x='' FOR d IN collection FILTER x RETURN d", expected, &ctx); // reference
  }

  // true value
  {
    irs::Or expected;
    expected.add<irs::all>();

    assertFilterSuccess("FOR d IN collection FILTER true RETURN d", expected);
  }

  // boolean reference
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("x", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintBool{true})));

    irs::Or expected;
    expected.add<irs::all>();

    assertFilterSuccess("LET x=true FOR d IN collection FILTER x RETURN d", expected, &ctx); // reference
  }

  // false
  {
    irs::Or expected;
    expected.add<irs::empty>();

    assertFilterSuccess("FOR d IN collection FILTER false RETURN d", expected);
  }

  // boolean reference
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("x", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintBool{false})));

    irs::Or expected;
    expected.add<irs::empty>();

    assertFilterSuccess("LET x=false FOR d IN collection FILTER x RETURN d", expected, &ctx); // reference
  }

  // null == value
  {
    irs::Or expected;
    expected.add<irs::empty>();

    assertFilterSuccess("FOR d IN collection FILTER null RETURN d", expected);
  }

  // non zero numeric value
  {
    irs::Or expected;
    expected.add<irs::all>();

    assertFilterSuccess("FOR d IN collection FILTER 1 RETURN d", expected);
  }

  // non zero numeric reference
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("x", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{1})));

    irs::Or expected;
    expected.add<irs::all>();

    assertFilterSuccess("LET x=1 FOR d IN collection FILTER x RETURN d", expected, &ctx); // reference
  }

  // zero numeric value
  {
    irs::Or expected;
    expected.add<irs::empty>();

    assertFilterSuccess("FOR d IN collection FILTER 0 RETURN d", expected);
  }

  // zero numeric reference
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("x", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{0})));

    irs::Or expected;
    expected.add<irs::empty>();

    assertFilterSuccess("LET x=0 FOR d IN collection FILTER x RETURN d", expected, &ctx); // reference
  }

  // zero floating value
  {
    irs::Or expected;
    expected.add<irs::empty>();

    assertFilterSuccess("FOR d IN collection FILTER 0.0 RETURN d", expected);
  }

  // zero floating reference
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("x", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{0.0})));

    irs::Or expected;
    expected.add<irs::empty>();

    assertFilterSuccess("LET x=0.0 FOR d IN collection FILTER x RETURN d", expected, &ctx); // reference
  }

  // non zero floating value
  {
    irs::Or expected;
    expected.add<irs::all>();

    assertFilterSuccess("FOR d IN collection FILTER 0.1 RETURN d", expected);
  }

  // non zero floating reference
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("x", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{0.1})));

    irs::Or expected;
    expected.add<irs::all>();

    assertFilterSuccess("LET x=0.1 FOR d IN collection FILTER x RETURN d", expected, &ctx); // reference
  }

  // Array == true
  {
    irs::Or expected;
    expected.add<irs::all>();

    assertFilterSuccess("FOR d IN collection FILTER [] RETURN d", expected);
  }

  // Array reference
  {
    auto obj = arangodb::velocypack::Parser::fromJson("[]");

    ExpressionContextMock ctx;
    ctx.vars.emplace("x", arangodb::aql::AqlValue(arangodb::aql::AqlValue(obj->slice())));

    irs::Or expected;
    expected.add<irs::all>();

    assertFilterSuccess("LET x=[] FOR d IN collection FILTER x RETURN d", expected, &ctx); // reference
  }

  // Range == true
  {
    irs::Or expected;
    expected.add<irs::all>();

    assertFilterSuccess("FOR d IN collection FILTER 1..2 RETURN d", expected);
  }

  // Range reference
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("x", arangodb::aql::AqlValue(arangodb::aql::AqlValue(1, 1)));

    irs::Or expected;
    expected.add<irs::all>();

    assertFilterSuccess("LET x=1..1 FOR d IN collection FILTER x RETURN d", expected, &ctx); // reference
  }

  // Object == true
  {
    irs::Or expected;
    expected.add<irs::all>();

    assertFilterSuccess("FOR d IN collection FILTER {} RETURN d", expected);
  }

  // Object reference
  {
    auto obj = arangodb::velocypack::Parser::fromJson("{}");

    ExpressionContextMock ctx;
    ctx.vars.emplace("x", arangodb::aql::AqlValue(arangodb::aql::AqlValue(obj->slice())));

    irs::Or expected;
    expected.add<irs::all>();

    assertFilterSuccess("LET x={} FOR d IN collection FILTER x RETURN d", expected, &ctx); // reference
  }

  // numeric expression
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("numVal", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt(2)));

    irs::Or expected;
    expected.add<irs::empty>();

    assertFilterSuccess("LET numVal=2 FOR d IN collection FILTER numVal-2 RETURN d", expected, &ctx);
  }

  // boolean expression
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("numVal", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt(2)));

    irs::Or expected;
    expected.add<irs::empty>();

    assertFilterSuccess("LET numVal=2 FOR d IN collection FILTER ((numVal+1) < 2) RETURN d", expected, &ctx);
  }

  // null expression
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("nullVal", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintNull{}));

    irs::Or expected;
    auto& root = expected.add<irs::And>();
    root.add<irs::empty>();
    root.add<irs::all>();

    assertFilterSuccess("LET nullVal=null FOR d IN collection FILTER (nullVal && true) RETURN d", expected, &ctx);
  }

  // string value == true, boosted
  {
    irs::Or expected;
    expected.add<irs::all>().boost(2.5);

    assertFilterSuccess("FOR d IN collection FILTER BOOST('1', 2.5) RETURN d", expected);
  }

  // string value == true, analyzer
  {
    irs::Or expected;
    expected.add<irs::all>();

    assertFilterSuccess("FOR d IN collection FILTER ANALYZER('1', 'test_analyzer') RETURN d", expected);
  }

  // null expression, boost
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("nullVal", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintNull{}));

    irs::Or expected;
    auto& root = expected.add<irs::And>();
    root.boost(0.75);
    root.add<irs::empty>();
    root.add<irs::all>();

    assertFilterSuccess("LET nullVal=null FOR d IN collection FILTER BOOST(nullVal && true, 0.75) RETURN d", expected, &ctx);
  }

  // self-reference
  assertExpressionFilter("FOR d IN collection FILTER d RETURN d");
  assertExpressionFilter("FOR d IN collection FILTER d[1] RETURN d");
  assertExpressionFilter("FOR d IN collection FILTER BOOST(d[1], 1.5) RETURN d", 1.5, wrappedExpressionExtractor);
  assertExpressionFilter("FOR d IN collection FILTER ANALYZER(d[1], 'test_analyzer') RETURN d", 1.0, wrappedExpressionExtractor);
  assertExpressionFilter("FOR d IN collection FILTER d.a[1] RETURN d");
  assertExpressionFilter("FOR d IN collection FILTER d[*] RETURN d");
  assertExpressionFilter("FOR d IN collection FILTER BOOST(d[*], 0.5) RETURN d", 0.5, wrappedExpressionExtractor);
  assertExpressionFilter("FOR d IN collection FILTER d.a[*] RETURN d");
}

SECTION("SystemFunctions") {
  // scalar
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("x", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{1})));

    irs::Or expected;
    expected.add<irs::all>();

    assertFilterSuccess("LET x=1 FOR d IN collection FILTER TO_STRING(x) RETURN d", expected, &ctx); // reference
  }

  // scalar
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("x", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{0})));

    irs::Or expected;
    expected.add<irs::empty>();

    assertFilterSuccess("LET x=0 FOR d IN collection FILTER TO_BOOL(x) RETURN d", expected, &ctx); // reference
  }

  // scalar with boost
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("x", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{1})));

    irs::Or expected;
    expected.add<irs::all>().boost(1.5f);

    assertFilterSuccess("LET x=1 FOR d IN collection FILTER BOOST(TO_STRING(x), 1.5) RETURN d", expected, &ctx); // reference
  }

  // scalar with boost
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("x", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{0})));

    irs::Or expected;
    expected.add<irs::empty>();

    assertFilterSuccess("LET x=0 FOR d IN collection FILTER BOOST(TO_BOOL(x), 1.5) RETURN d", expected, &ctx); // reference
  }

  // nondeterministic expression : wrap it
  assertExpressionFilter("FOR d IN myView FILTER RAND() RETURN d");
  assertExpressionFilter("FOR d IN myView FILTER BOOST(RAND(), 1.5) RETURN d", 1.5, wrappedExpressionExtractor);
  assertExpressionFilter("FOR d IN myView FILTER ANALYZER(RAND(), 'test_analyzer') RETURN d", 1.0, wrappedExpressionExtractor);
}

SECTION("UnsupportedUserFunctions") {
//  FIXME need V8 context up and running to execute user functions
//  assertFilterFail("FOR d IN myView FILTER ir::unknownFunction() RETURN d", &ExpressionContextMock::EMPTY);
//  assertFilterFail("FOR d IN myView FILTER ir::unknownFunction1(d) RETURN d", &ExpressionContextMock::EMPTY);
//  assertFilterFail("FOR d IN myView FILTER ir::unknownFunction2(d, 'quick') RETURN d", &ExpressionContextMock::EMPTY);
}

SECTION("Boost") {
  // simple boost
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("x", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{1.5})));

    irs::Or expected;
    auto& termFilter = expected.add<irs::by_term>();
    termFilter.field(mangleStringIdentity("foo")).term("abc").boost(1.5);

    assertFilterSuccess("LET x=1.5 FOR d IN collection FILTER BOOST(d.foo == 'abc', x) RETURN d", expected, &ctx);
  }

  // embedded boost
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("x", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{1.5})));

    irs::Or expected;
    auto& termFilter = expected.add<irs::by_term>();
    termFilter.field(mangleStringIdentity("foo")).term("abc").boost(6.0f); // 1.5*4 or 1.5*2*2

    assertFilterSuccess("LET x=1.5 FOR d IN collection FILTER BOOST(BOOST(d.foo == 'abc', x), 4) RETURN d", expected, &ctx);
    assertFilterSuccess("LET x=1.5 FOR d IN collection FILTER BOOST(BOOST(BOOST(d.foo == 'abc', x), 2), 2) RETURN d", expected, &ctx);
  }

  // wrong number of arguments
  assertFilterParseFail("FOR d IN collection FILTER BOOST(d.foo == 'abc') RETURN d");

  // wrong argument type
  assertFilterFail("FOR d IN collection FILTER BOOST(d.foo == 'abc', '2') RETURN d");
  assertFilterFail("FOR d IN collection FILTER BOOST(d.foo == 'abc', null) RETURN d");
  assertFilterFail("FOR d IN collection FILTER BOOST(d.foo == 'abc', true) RETURN d");

  // non-deterministic expression
  assertFilterFail("FOR d IN collection FILTER BOOST(d.foo == 'abc', RAND()) RETURN d");

  // can't execute boost function
  assertFilterExecutionFail(
    "LET x=1.5 FOR d IN collection FILTER BOOST(d.foo == 'abc', BOOST(x, 2)) RETURN d",
    &ExpressionContextMock::EMPTY
  );
}

SECTION("Analyzer") {
  // simple analyzer
  {
    irs::Or expected;
    expected.add<irs::by_term>().field(mangleString("foo", "testVocbase::test_analyzer")).term("bar");

    assertFilterSuccess(
      "FOR d IN collection FILTER ANALYZER(d.foo == 'bar', 'test_analyzer') RETURN d",
      expected
    );
  }

  // overriden analyzer
  {
    irs::Or expected;
    expected.add<irs::by_term>().field(mangleStringIdentity("foo")).term("bar");

    assertFilterSuccess(
      "FOR d IN collection FILTER ANALYZER(ANALYZER(d.foo == 'bar', 'identity'), 'test_analyzer') RETURN d",
      expected
    );
  }

  // expression as the parameter
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("x", arangodb::aql::AqlValue(arangodb::aql::AqlValue("test_")));

    irs::Or expected;
    expected.add<irs::by_term>().field(mangleString("foo", "testVocbase::test_analyzer")).term("bar");

    assertFilterSuccess(
      "LET x='test_' FOR d IN collection FILTER ANALYZER(d.foo == 'bar', CONCAT(x, 'analyzer')) RETURN d",
      expected,
      &ctx
    );
  }

  // wrong number of arguments
  assertFilterParseFail("FOR d IN collection FILTER ANALYZER(d.foo == 'bar') RETURN d");

  // wrong argument type
  assertFilterFail("FOR d IN collection FILTER ANALYZER(d.foo == 'abc', 'invalid analzyer') RETURN d");
  assertFilterFail("FOR d IN collection FILTER ANALYZER(d.foo == 'abc', 3.14) RETURN d");
  assertFilterFail("FOR d IN collection FILTER ANALYZER(d.foo == 'abc', null) RETURN d");
  assertFilterFail("FOR d IN collection FILTER ANALYZER(d.foo == 'abc', true) RETURN d");

  // non-deterministic expression
  assertFilterFail("FOR d IN collection FILTER ANALYZER(d.foo == 'abc', RAND() > 0 ? 'test_analyzer' : 'identity') RETURN d");

  // can't execute boost function
  assertFilterExecutionFail(
    "LET x=1.5 FOR d IN collection FILTER ANALYZER(d.foo == 'abc', ANALYZER(x, 'test_analyzer')) RETURN d",
    &ExpressionContextMock::EMPTY
  );
}

SECTION("MinMatch") {
  // simplest MIN_MATCH
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("x", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{2})));

    irs::Or expected;
    auto& minMatch = expected.add<irs::Or>();
    minMatch.min_match_count(2);
    minMatch.add<irs::Or>().add<irs::by_term>().field(mangleStringIdentity("foobar")).term("bar");

    assertFilterSuccess(
      "LET x=2 FOR d IN collection FILTER MIN_MATCH(d.foobar == 'bar', x) RETURN d",
      expected,
      &ctx
    );
  }

  // simple MIN_MATCH
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("x", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{2})));

    irs::Or expected;
    auto& minMatch = expected.add<irs::Or>();
    minMatch.min_match_count(2);
    minMatch.add<irs::Or>().add<irs::by_term>().field(mangleStringIdentity("foobar")).term("bar");
    minMatch.add<irs::Or>().add<irs::by_term>().field(mangleStringIdentity("foobaz")).term("baz");
    minMatch.add<irs::Or>().add<irs::by_term>().field(mangleStringIdentity("foobad")).term("bad");

    assertFilterSuccess(
      "LET x=2 FOR d IN collection FILTER MIN_MATCH(d.foobar == 'bar', d.foobaz == 'baz', d.foobad == 'bad', x) RETURN d",
      expected,
      &ctx
    );
  }

  // simple MIN_MATCH
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("x", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{1.5})));

    irs::Or expected;
    auto& minMatch = expected.add<irs::Or>();
    minMatch.min_match_count(2);
    minMatch.add<irs::Or>().add<irs::by_term>().field(mangleStringIdentity("foobar")).term("bar");
    minMatch.add<irs::Or>().add<irs::by_term>().field(mangleStringIdentity("foobaz")).term("baz").boost(1.5f);
    minMatch.add<irs::Or>().add<irs::by_term>().field(mangleStringIdentity("foobad")).term("bad");

    assertFilterSuccess(
      "LET x=1.5 FOR d IN collection FILTER MIN_MATCH(d.foobar == 'bar', BOOST(d.foobaz == 'baz', x), d.foobad == 'bad', x) RETURN d",
      expected,
      &ctx
    );
  }

  // wrong sub-expression
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("x", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{1.5})));

    assertFilterExecutionFail(
      "LET x=1.5 FOR d IN collection FILTER MIN_MATCH(d.foobar == 'bar', BOOST(d.foobaz == 'baz', TO_STRING(x)), d.foobad == 'bad', x) RETURN d",
      &ctx
    );
  }

  // boosted MIN_MATCH
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("x", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{1.5})));

    irs::Or expected;
    auto& minMatch = expected.add<irs::Or>();
    minMatch.boost(3.0f);
    minMatch.min_match_count(2);
    minMatch.add<irs::Or>().add<irs::by_term>().field(mangleStringIdentity("foobar")).term("bar");
    minMatch.add<irs::Or>().add<irs::by_term>().field(mangleStringIdentity("foobaz")).term("baz").boost(1.5f);
    minMatch.add<irs::Or>().add<irs::by_term>().field(mangleStringIdentity("foobad")).term("bad");

    assertFilterSuccess(
      "LET x=1.5 FOR d IN collection FILTER BOOST(MIN_MATCH(d.foobar == 'bar', BOOST(d.foobaz == 'baz', x), d.foobad == 'bad', x), x*2) RETURN d",
      expected,
      &ctx
    );
  }

  // boosted embedded MIN_MATCH
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("x", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{1.5})));

    irs::Or expected;
    auto& minMatch = expected.add<irs::Or>();
    minMatch.boost(3.0f);
    minMatch.min_match_count(2);
    minMatch.add<irs::Or>().add<irs::by_term>().field(mangleStringIdentity("foobar")).term("bar");
    minMatch.add<irs::Or>().add<irs::by_term>().field(mangleStringIdentity("foobaz")).term("baz").boost(1.5f);
    auto& subMinMatch = minMatch.add<irs::Or>().add<irs::Or>();
    subMinMatch.min_match_count(2);
    subMinMatch.add<irs::Or>().add<irs::by_term>().field(mangleStringIdentity("foobar")).term("bar");
    subMinMatch.add<irs::Or>().add<irs::by_range>().field(mangleStringIdentity("foobaz")).term<irs::Bound::MIN>("baz").include<irs::Bound::MIN>(false);
    subMinMatch.add<irs::Or>().add<irs::by_term>().field(mangleStringIdentity("foobad")).term("bad").boost(2.7f);

    assertFilterSuccess(
      "LET x=1.5 FOR d IN collection FILTER "
      "  BOOST("
      "    MIN_MATCH("
      "      d.foobar == 'bar', "
      "      BOOST(d.foobaz == 'baz', x), "
      "      MIN_MATCH(d.foobar == 'bar', d.foobaz > 'baz', BOOST(d.foobad == 'bad', 2.7), x),"
      "    x), "
      "  x*2) "
      "RETURN d",
      expected,
      &ctx
    );
  }

  // wrong number of arguments
  assertFilterParseFail("FOR d IN collection FILTER MIN_MATCH(d.foobar == 'bar') RETURN d");

  // wrong argument type
  assertFilterFail(
    "FOR d IN collection FILTER MIN_MATCH(d.foobar == 'bar', d.foobaz == 'baz', d.foobad == 'bad', '2') RETURN d",
    &ExpressionContextMock::EMPTY
  );
  assertFilterFail(
    "FOR d IN collection FILTER MIN_MATCH(d.foobar == 'bar', d.foobaz == 'baz', d.foobad == 'bad', null) RETURN d",
    &ExpressionContextMock::EMPTY
  );
  assertFilterFail(
    "FOR d IN collection FILTER MIN_MATCH(d.foobar == 'bar', d.foobaz == 'baz', d.foobad == 'bad', true) RETURN d",
    &ExpressionContextMock::EMPTY
  );

  // non-deterministic expression
  assertFilterFail(
    "FOR d IN collection FILTER MIN_MATCH(d.foobar == 'bar', d.foobaz == 'baz', d.foobad == 'bad', RAND()) RETURN d",
    &ExpressionContextMock::EMPTY
  );
}

SECTION("Exists") {
  // field only
  {
    irs::Or expected;
    auto& exists = expected.add<irs::by_column_existence>();
    exists.field("name").prefix_match(true);

    assertFilterSuccess("FOR d IN myView FILTER exists(d.name) RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER exists(d['name']) RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER eXists(d.name) RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER eXists(d['name']) RETURN d", expected);
  }

  // field with simple offset
  {
    irs::Or expected;
    auto& exists = expected.add<irs::by_column_existence>();
    exists.field("[42]").prefix_match(true);

    assertFilterSuccess("FOR d IN myView FILTER exists(d[42]) RETURN d", expected);
  }

  // complex field
  {
    irs::Or expected;
    auto& exists = expected.add<irs::by_column_existence>();
    exists.field("obj.prop.name").prefix_match(true);

    assertFilterSuccess("FOR d IN myView FILTER exists(d.obj.prop.name) RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER exists(d['obj']['prop']['name']) RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER eXists(d.obj.prop.name) RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER eXists(d['obj'].prop.name) RETURN d", expected);
  }

  // complex field with offset
  {
    irs::Or expected;
    auto& exists = expected.add<irs::by_column_existence>();
    exists.field("obj.prop[3].name").prefix_match(true);

    assertFilterSuccess("FOR d IN myView FILTER exists(d.obj.prop[3].name) RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER exists(d['obj']['prop'][3]['name']) RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER eXists(d.obj.prop[3].name) RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER eXists(d['obj'].prop[3].name) RETURN d", expected);
  }

  // complex field with offset
  {
    irs::Or expected;
    auto& exists = expected.add<irs::by_column_existence>();
    exists.field("obj.prop[3].name").prefix_match(true).boost(1.5f);

    assertFilterSuccess("FOR d IN myView FILTER BOOST(exists(d.obj.prop[3].name), 1.5) RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER BooSt(exists(d['obj']['prop'][3]['name']), 0.5*3) RETURN d", expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess("FOR d IN myView FILTER booSt(eXists(d.obj.prop[3].name), 1+0.5) RETURN d", expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess("FOR d IN myView FILTER BoOSt(eXists(d['obj'].prop[3].name), 1.5) RETURN d", expected);
  }

  // complex field with offset
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("index", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt(2)));

    irs::Or expected;
    auto& exists = expected.add<irs::by_column_existence>();
    exists.field("obj.prop[3].name").prefix_match(true);

    assertFilterSuccess("LET index=2 FOR d IN myView FILTER exists(d.obj.prop[index+1].name) RETURN d", expected, &ctx);
    assertFilterSuccess("FOR d IN myView FILTER exists(d['obj']['prop'][3]['name']) RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER eXists(d.obj.prop[3].name) RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER eXists(d['obj'].prop[3].name) RETURN d", expected);
  }

  // dynamic complex attribute field
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    irs::Or expected;
    auto& exists = expected.add<irs::by_column_existence>();
    exists.field("a.b.c.e[4].f[5].g[3].g.a").prefix_match(true);

    assertFilterSuccess("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER exists(d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')]) RETURN d", expected, &ctx);
  }

  // invalid dynamic attribute name
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER exists(d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')]) RETURN d", &ctx);
  }

  // invalid dynamic attribute name (null value)
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintNull{})); // invalid value type
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a=null LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER exists(d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')]) RETURN d", &ctx);
  }

  // invalid dynamic attribute name (bool value)
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintBool{false})); // invalid value type
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a=false LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER exists(d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')]) RETURN d", &ctx);
  }

  // invalid attribute access
  assertFilterFail("FOR d IN myView FILTER exists(d) RETURN d");
  assertFilterFail("FOR d IN myView FILTER exists(d[*]) RETURN d");
  assertFilterFail("FOR d IN myView FILTER exists(d.a.b[*]) RETURN d");
  assertFilterFail("FOR d IN myView FILTER exists('d.name') RETURN d");
  assertFilterFail("FOR d IN myView FILTER exists(123) RETURN d");
  assertFilterFail("FOR d IN myView FILTER exists(123.5) RETURN d");
  assertFilterFail("FOR d IN myView FILTER exists(null) RETURN d");
  assertFilterFail("FOR d IN myView FILTER exists(true) RETURN d");
  assertFilterFail("FOR d IN myView FILTER exists(false) RETURN d");

  // field + type
  {
    irs::Or expected;
    auto& exists = expected.add<irs::by_column_existence>();
    exists.field(mangleType("name")).prefix_match(true);

    assertFilterSuccess("FOR d IN myView FILTER exists(d.name, 'type') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER eXists(d.name, 'type') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER exists(d.name, 'Type') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER exists(d.name, 'TYPE') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER ANALYZER(exists(d.name, 'TYPE'), 'test_analyzer') RETURN d", expected);

    // invalid 2nd argument
    assertFilterFail("FOR d IN myView FILTER exists(d.name, 'invalid') RETURN d");
    assertFilterExecutionFail("FOR d IN myView FILTER exists(d.name, d) RETURN d", &ExpressionContextMock::EMPTY);
    assertFilterFail("FOR d IN myView FILTER exists(d.name, null) RETURN d");
    assertFilterFail("FOR d IN myView FILTER exists(d.name, 123) RETURN d");
    assertFilterFail("FOR d IN myView FILTER exists(d.name, 123.5) RETURN d");
    assertFilterFail("FOR d IN myView FILTER exists(d.name, true) RETURN d");
    assertFilterFail("FOR d IN myView FILTER exists(d.name, false) RETURN d");

    // invalid 3rd argument
    assertFilterFail("FOR d IN myView FILTER exists(d.name, 'TYPE', 'test_analyzer') RETURN d");
  }

  // field + any string value
  {
    irs::Or expected;
    auto& exists = expected.add<irs::by_column_existence>();
    exists.field(mangleAnalyzer("name")).prefix_match(true);

    assertFilterSuccess("FOR d IN myView FILTER exists(d.name, 'string') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER eXists(d.name, 'string') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER exists(d.name, 'String') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER exists(d.name, 'STRING') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER analyzer(exists(d.name, 'STRING'), 'test_analyzer') RETURN d", expected);

    // invalid 3rd argument
    assertFilterFail("FOR d IN myView FILTER exists(d.name, 'string', 'test_analyzer') RETURN d");
  }

  // invalid 2nd argument
  assertFilterFail("FOR d IN myView FILTER exists(d.name, 'foo') RETURN d");
  assertFilterExecutionFail("FOR d IN myView FILTER exists(d.name, d) RETURN d", &ExpressionContextMock::EMPTY);
  assertFilterFail("FOR d IN myView FILTER exists(d.name, null) RETURN d");
  assertFilterFail("FOR d IN myView FILTER exists(d.name, 123) RETURN d");
  assertFilterFail("FOR d IN myView FILTER exists(d.name, 123.5) RETURN d");
  assertFilterFail("FOR d IN myView FILTER exists(d.name, true) RETURN d");
  assertFilterFail("FOR d IN myView FILTER exists(d.name, false) RETURN d");

  // field + any string value mode as an expression
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("anl", arangodb::aql::AqlValue("str"));

    irs::Or expected;
    auto& exists = expected.add<irs::by_column_existence>();
    exists.field(mangleAnalyzer("name")).prefix_match(true);

    assertFilterSuccess("LET anl='str' FOR d IN myView FILTER exists(d.name, CONCAT(anl,'ing')) RETURN d", expected, &ctx);
    assertFilterSuccess("LET anl='str' FOR d IN myView FILTER eXists(d.name, CONCAT(anl,'ing')) RETURN d", expected, &ctx);

    // invalid 3rd argument
    assertFilterExecutionFail("LET anl='str' FOR d IN myView FILTER eXists(d.name, CONCAT(anl,'ing'), 'test_analyzer') RETURN d", &ctx);
  }

  // field + analyzer as invalid expression
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("anl", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintNull{}));

    assertFilterExecutionFail("LET anl='analyz' FOR d IN myView FILTER exists(d.name, anl) RETURN d", &ctx);
    assertFilterExecutionFail("LET anl='analyz' FOR d IN myView FILTER eXists(d.name, anl) RETURN d", &ctx);
  }

  // field + analyzer
  {
    irs::Or expected;
    auto& exists = expected.add<irs::by_column_existence>();
    exists.field(mangleStringIdentity("name")).prefix_match(false);

    assertFilterSuccess("FOR d IN myView FILTER exists(d.name, 'analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER eXists(d.name, 'analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER analyzer(eXists(d.name, 'analyzer'), 'identity') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER eXists(d.name, 'analyzer', 'identity') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER exists(d.name, 'Analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER exists(d.name, 'ANALYZER') RETURN d", expected);

    // invalid 2nd argument
    assertFilterFail("FOR d IN myView FILTER exists(d.name, 'invalid') RETURN d");

    // invalid analyzer argument
    assertFilterFail("FOR d IN myView FILTER exists(d.name, 'analyzer', 'invalid') RETURN d");
  }

  // field + analyzer as an expression
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("type", arangodb::aql::AqlValue("analy"));

    irs::Or expected;
    auto& exists = expected.add<irs::by_column_existence>();
    exists.field(mangleStringIdentity("name")).prefix_match(false);

    assertFilterSuccess("LET type='analy' FOR d IN myView FILTER exists(d.name, CONCAT(type,'zer')) RETURN d", expected, &ctx);
    assertFilterSuccess("LET type='analy' FOR d IN myView FILTER eXists(d.name, CONCAT(type,'zer')) RETURN d", expected, &ctx);
    assertFilterSuccess("LET type='analy' FOR d IN myView FILTER analyzer(eXists(d.name, CONCAT(type,'zer')), 'identity') RETURN d", expected, &ctx);
    assertFilterSuccess("LET type='analy' FOR d IN myView FILTER eXists(d.name, CONCAT(type,'zer'), 'identity') RETURN d", expected, &ctx);
  }

  // field + numeric
  {
    irs::Or expected;
    auto& exists = expected.add<irs::by_column_existence>();
    exists.field(mangleNumeric("obj.name")).prefix_match(false);

    assertFilterSuccess("FOR d IN myView FILTER exists(d.obj.name, 'numeric') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER eXists(d.obj.name, 'numeric') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER eXists(d.obj.name, 'Numeric') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER eXists(d.obj.name, 'NUMERIC') RETURN d", expected);

    // invalid argument
    assertFilterFail("FOR d IN myView FILTER exists(d.obj.name, 'foo') RETURN d");

    // invalid 3rd argument
    assertFilterFail("FOR d IN myView FILTER exists(d.obj.name, 'numeric', 'test_analyzer') RETURN d");
  }

  // field + numeric as an expression
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("type", arangodb::aql::AqlValue("nume"));

    irs::Or expected;
    auto& exists = expected.add<irs::by_column_existence>();
    exists.field(mangleNumeric("name")).prefix_match(false);

    assertFilterSuccess("LET type='nume' FOR d IN myView FILTER exists(d.name, CONCAT(type,'ric')) RETURN d", expected, &ctx);
    assertFilterSuccess("LET type='nume' FOR d IN myView FILTER eXists(d.name, CONCAT(type,'ric')) RETURN d", expected, &ctx);
    assertFilterSuccess("LET type='nume' FOR d IN myView FILTER ANALYZER(eXists(d.name, CONCAT(type,'ric')), 'test_analyzer') RETURN d", expected, &ctx);

    // invalid 3rd argument
    assertFilterExecutionFail("LET type='nume' FOR d IN myView FILTER eXists(d.name, CONCAT(type,'ric'), 'test_analyzer') RETURN d", &ctx);
  }

  // field + bool
  {
    irs::Or expected;
    auto& exists = expected.add<irs::by_column_existence>();
    exists.field(mangleBool("name")).prefix_match(false);

    assertFilterSuccess("FOR d IN myView FILTER exists(d.name, 'bool') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER eXists(d.name, 'bool') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER eXists(d.name, 'Bool') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER eXists(d.name, 'BOOL') RETURN d", expected);

    // invalid 2nd argument
    assertFilterFail("FOR d IN myView FILTER exists(d.name, 'asdfasdfa') RETURN d");

    // invalid 3rd argument
    assertFilterFail("FOR d IN myView FILTER exists(d.obj.name, 'bool', 'test_analyzer') RETURN d");
  }

  // field + type + boolean
  {
    irs::Or expected;
    auto& exists = expected.add<irs::by_column_existence>();
    exists.field(mangleBool("name")).prefix_match(false);

    assertFilterSuccess("FOR d IN myView FILTER exists(d.name, 'boolean') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER eXists(d.name, 'boolean') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER analyzer(eXists(d.name, 'boolean'), 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER eXists(d.name, 'Boolean') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER eXists(d.name, 'BOOLEAN') RETURN d", expected);

    // invalid 2nd argument
    assertFilterFail("FOR d IN myView FILTER exists(d.name, 'asdfasdfa') RETURN d");

    // invalid 3rd argument
    assertFilterFail("FOR d IN myView FILTER exists(d.obj.name, 'boolean', 'test_analyzer') RETURN d");
  }

  // field + boolean as an expression
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("type", arangodb::aql::AqlValue("boo"));

    irs::Or expected;
    auto& exists = expected.add<irs::by_column_existence>();
    exists.field(mangleBool("name")).prefix_match(false);

    assertFilterSuccess("LET type='boo' FOR d IN myView FILTER exists(d.name, CONCAT(type,'lean')) RETURN d", expected, &ctx);
    assertFilterSuccess("LET type='boo' FOR d IN myView FILTER eXists(d.name, CONCAT(type,'lean')) RETURN d", expected, &ctx);
    assertFilterSuccess("LET type='boo' FOR d IN myView FILTER ANALYZER(eXists(d.name, CONCAT(type,'lean')), 'test_analyzer') RETURN d", expected, &ctx);

    // invalid 3rd argument
    assertFilterExecutionFail("LET type='boo' FOR d IN myView FILTER eXists(d.name, CONCAT(type,'lean'), 'test_analyzer') RETURN d", &ctx);
  }

  // field + null
  {
    irs::Or expected;
    auto& exists = expected.add<irs::by_column_existence>();
    exists.field(mangleNull("name")).prefix_match(false);

    assertFilterSuccess("FOR d IN myView FILTER exists(d.name, 'null') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER eXists(d.name, 'null') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER eXists(d.name, 'Null') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER eXists(d.name, 'NULL') RETURN d", expected);

    // invalid 2nd argument
    assertFilterFail("FOR d IN myView FILTER exists(d.name, 'asdfasdfa') RETURN d");

    // invalid 3rd argument
    assertFilterFail("FOR d IN myView FILTER eXists(d.name, 'NULL', 'test_analyzer') RETURN d");
  }

  // field + null as an expression
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("type", arangodb::aql::AqlValue("nu"));

    irs::Or expected;
    auto& exists = expected.add<irs::by_column_existence>();
    exists.field(mangleNull("name")).prefix_match(false);

    assertFilterSuccess("LET type='nu' FOR d IN myView FILTER exists(d.name, CONCAT(type,'ll')) RETURN d", expected, &ctx);
    assertFilterSuccess("LET type='nu' FOR d IN myView FILTER eXists(d.name, CONCAT(type,'ll')) RETURN d", expected, &ctx);
    assertFilterSuccess("LET type='nu' FOR d IN myView FILTER ANALYZER(eXists(d.name, CONCAT(type,'ll')), 'identity') RETURN d", expected, &ctx);

    // invalid 3rd argument
    assertFilterExecutionFail("LET type='nu' FOR d IN myView FILTER eXists(d.name, CONCAT(type,'ll'), 'identity') RETURN d", &ctx);
  }

  // field + type + invalid expression
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("type", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintNull{}));

    assertFilterExecutionFail("LET type=null FOR d IN myView FILTER exists(d.name, type) RETURN d", &ctx);
    assertFilterExecutionFail("LET type=null FOR d IN myView FILTER eXists(d.name, type) RETURN d", &ctx);
  }

  // invalid 2nd argument
  assertFilterExecutionFail("FOR d IN myView FILTER exists(d.name, d) RETURN d", &ExpressionContextMock::EMPTY);
  assertFilterFail("FOR d IN myView FILTER exists(d.name, null) RETURN d");
  assertFilterFail("FOR d IN myView FILTER exists(d.name, 123) RETURN d");
  assertFilterFail("FOR d IN myView FILTER exists(d.name, 123.5) RETURN d");
  assertFilterFail("FOR d IN myView FILTER exists(d.name, true) RETURN d");
  assertFilterFail("FOR d IN myView FILTER exists(d.name, false) RETURN d");

  // field + default analyzer
  {
    irs::Or expected;
    auto& exists = expected.add<irs::by_column_existence>();
    exists.field(mangleStringIdentity("name")).prefix_match(false);

    assertFilterSuccess("FOR d IN myView FILTER analyzer(exists(d.name, 'analyzer'), 'identity') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER eXists(d.name, 'analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER eXists(d.name, 'analyzer', 'identity') RETURN d", expected);
  }

  // field + analyzer
  {
    irs::Or expected;
    auto& exists = expected.add<irs::by_column_existence>();
    exists.field(mangleString("name", "testVocbase::test_analyzer")).prefix_match(false);

    assertFilterSuccess("FOR d IN myView FILTER analyzer(exists(d.name, 'analyzer'), 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER exists(d.name, 'analyzer', 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER analyzer(eXists(d.name, 'analyzer'), 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER eXists(d.name, 'analyzer', 'test_analyzer') RETURN d", expected);

    // invalid analyzer
    assertFilterFail("FOR d IN myView FILTER analyzer(exists(d.name, 'analyzer'), 'foo') RETURN d");
    assertFilterFail("FOR d IN myView FILTER analyzer(exists(d.name, 'analyzer'), 'invalid') RETURN d");
    assertFilterFail("FOR d IN myView FILTER analyzer(exists(d.name, 'analyzer'), '') RETURN d");
    assertFilterExecutionFail("FOR d IN myView FILTER analyzer(exists(d.name, 'analyzer'), d) RETURN d", &ExpressionContextMock::EMPTY);
    assertFilterFail("FOR d IN myView FILTER analyzer(exists(d.name, 'analyzer'), null) RETURN d");
    assertFilterFail("FOR d IN myView FILTER analyzer(exists(d.name, 'analyzer'), 123) RETURN d");
    assertFilterFail("FOR d IN myView FILTER analyzer(exists(d.name, 'analyzer'), 123.5) RETURN d");
    assertFilterFail("FOR d IN myView FILTER analyzer(exists(d.name, 'analyzer'), true) RETURN d");
    assertFilterFail("FOR d IN myView FILTER analyzer(exists(d.name, 'analyzer'), false) RETURN d");
    assertFilterFail("FOR d IN myView FILTER exists(d.name, 'analyzer', 'foo') RETURN d");
    assertFilterFail("FOR d IN myView FILTER exists(d.name, 'analyzer', 'invalid') RETURN d");
    assertFilterFail("FOR d IN myView FILTER exists(d.name, 'analyzer', '') RETURN d");
    assertFilterExecutionFail("FOR d IN myView FILTER exists(d.name, 'analyzer', d) RETURN d", &ExpressionContextMock::EMPTY);
    assertFilterFail("FOR d IN myView FILTER exists(d.name, 'analyzer', null) RETURN d");
    assertFilterFail("FOR d IN myView FILTER exists(d.name, 'analyzer', 123) RETURN d");
    assertFilterFail("FOR d IN myView FILTER exists(d.name, 'analyzer', 123.5) RETURN d");
    assertFilterFail("FOR d IN myView FILTER exists(d.name, 'analyzer', true) RETURN d");
    assertFilterFail("FOR d IN myView FILTER exists(d.name, 'analyzer', false) RETURN d");
  }

  // field + type + analyzer as an expression
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("anl", arangodb::aql::AqlValue("test_"));
    ctx.vars.emplace("type", arangodb::aql::AqlValue("analyz"));

    irs::Or expected;
    auto& exists = expected.add<irs::by_column_existence>();
    exists.field(mangleString("name", "testVocbase::test_analyzer")).prefix_match(false);

    assertFilterSuccess("LET type='analyz' LET anl='test_' FOR d IN myView FILTER analyzer(exists(d.name, CONCAT(type,'er')), CONCAT(anl,'analyzer')) RETURN d", expected, &ctx);
    assertFilterSuccess("LET type='analyz' LET anl='test_' FOR d IN myView FILTER analyzer(eXists(d.name, CONCAT(type,'er')), CONCAT(anl,'analyzer')) RETURN d", expected, &ctx);
    assertFilterSuccess("LET type='analyz' LET anl='test_' FOR d IN myView FILTER exists(d.name, CONCAT(type,'er'), CONCAT(anl,'analyzer')) RETURN d", expected, &ctx);
    assertFilterSuccess("LET type='analyz' LET anl='test_' FOR d IN myView FILTER eXists(d.name, CONCAT(type,'er'), CONCAT(anl,'analyzer')) RETURN d", expected, &ctx);
  }

  // field + analyzer via []
  {
    irs::Or expected;
    auto& exists = expected.add<irs::by_column_existence>();
    exists.field(mangleString("name", "testVocbase::test_analyzer")).prefix_match(false);

    assertFilterSuccess("FOR d IN myView FILTER analyzer(exists(d['name'], 'analyzer'), 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER analyzer(eXists(d['name'], 'analyzer'), 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER exists(d['name'], 'analyzer', 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER eXists(d['name'], 'analyzer', 'test_analyzer') RETURN d", expected);

    // invalid analyzer argument
    assertFilterFail("FOR d IN myView FILTER analyzer(exists(d['name'], 'analyzer'), 'foo') RETURN d");
    assertFilterFail("FOR d IN myView FILTER analyzer(exists(d['name'], 'analyzer'), 'invalid') RETURN d");
    assertFilterFail("FOR d IN myView FILTER analyzer(exists(d['name'], 'analyzer'), '') RETURN d");
    assertFilterExecutionFail("FOR d IN myView FILTER analyzer(exists(d['name'], 'analyzer'), d) RETURN d", &ExpressionContextMock::EMPTY);
    assertFilterFail("FOR d IN myView FILTER analyzer(exists(d['name'], 'analyzer'), null) RETURN d");
    assertFilterFail("FOR d IN myView FILTER analyzer(exists(d['name'], 'analyzer'), 123) RETURN d");
    assertFilterFail("FOR d IN myView FILTER analyzer(exists(d['name'], 'analyzer'), 123.5) RETURN d");
    assertFilterFail("FOR d IN myView FILTER analyzer(exists(d['name'], 'analyzer'), true) RETURN d");
    assertFilterFail("FOR d IN myView FILTER analyzer(exists(d['name'], 'analyzer'), false) RETURN d");
    assertFilterFail("FOR d IN myView FILTER exists(d['name'], 'analyzer', 'foo') RETURN d");
    assertFilterFail("FOR d IN myView FILTER exists(d['name'], 'analyzer', 'invalid') RETURN d");
    assertFilterFail("FOR d IN myView FILTER exists(d['name'], 'analyzer', '') RETURN d");
    assertFilterExecutionFail("FOR d IN myView FILTER exists(d['name'], 'analyzer', d) RETURN d", &ExpressionContextMock::EMPTY);
    assertFilterFail("FOR d IN myView FILTER exists(d['name'], 'analyzer', null) RETURN d");
    assertFilterFail("FOR d IN myView FILTER exists(d['name'], 'analyzer', 123) RETURN d");
    assertFilterFail("FOR d IN myView FILTER exists(d['name'], 'analyzer', 123.5) RETURN d");
    assertFilterFail("FOR d IN myView FILTER exists(d['name'], 'analyzer', true) RETURN d");
    assertFilterFail("FOR d IN myView FILTER exists(d['name'], 'analyzer', false) RETURN d");
  }

  // field + identity analyzer
  {
    irs::Or expected;
    auto& exists = expected.add<irs::by_column_existence>();
    exists.field(mangleStringIdentity("name")).prefix_match(false);

    assertFilterSuccess("FOR d IN myView FILTER analyzer(exists(d.name, 'analyzer'), 'identity') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER eXists(d.name, 'analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER eXists(d.name, 'analyzer', 'identity') RETURN d", expected);
  }

  // invalid number of arguments
  assertFilterParseFail("FOR d IN myView FILTER exists() RETURN d");
  assertFilterParseFail("FOR d IN myView FILTER exists(d.name, 'type', 'null', d) RETURN d");
  assertFilterParseFail("FOR d IN myView FILTER exists(d.name, 'analyzer', 'test_analyzer', false) RETURN d");

  // non-deterministic arguments
  assertFilterFail("FOR d IN myView FILTER exists(d[RAND() ? 'name' : 'x']) RETURN d");
  assertFilterFail("FOR d IN myView FILTER exists(d.name, RAND() > 2 ? 'null' : 'string') RETURN d");
  assertFilterFail("FOR d IN myView FILTER exists(d.name, 'analyzer', RAND() > 2 ? 'test_analyzer' : 'identity') RETURN d");
}

SECTION("Phrase") {
  // wrong number of arguments
  assertFilterParseFail("FOR d IN myView FILTER phrase() RETURN d");

  // identity analyzer
  {
    irs::Or expected;
    auto& phrase = expected.add<irs::by_phrase>();
    phrase.field(mangleStringIdentity("name"));
    phrase.push_back("quick");

    // implicit (by default)
    assertFilterSuccess("FOR d IN myView FILTER phrase(d.name, 'quick') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER phrase(d['name'], 'quick') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER phRase(d.name, 'quick') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER phRase(d['name'], 'quick') RETURN d", expected);

    // explicit
    assertFilterSuccess("FOR d IN myView FILTER Analyzer(phrase(d.name, 'quick'), 'identity') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER Analyzer(phrase(d['name'], 'quick'), 'identity') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER Analyzer(phRase(d.name, 'quick'), 'identity') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER Analyzer(phRase(d['name'], 'quick'), 'identity') RETURN d", expected);

    // overridden
    assertFilterSuccess("FOR d IN myView FILTER phrase(d.name, 'quick', 'identity') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER phrase(d['name'], 'quick', 'identity') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER phRase(d.name, 'quick', 'identity') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER phRase(d['name'], 'quick', 'identity') RETURN d", expected);

    // overridden
    assertFilterSuccess("FOR d IN myView FILTER Analyzer(phrase(d.name, 'quick', 'identity'), 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER Analyzer(phrase(d['name'], 'quick', 'identity'), 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER Analyzer(phRase(d.name, 'quick', 'identity'), 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER Analyzer(phRase(d['name'], 'quick', 'identity'), 'test_analyzer') RETURN d", expected);
  }

  // without offset, custom analyzer
  // quick
  {
    irs::Or expected;
    auto& phrase = expected.add<irs::by_phrase>();
    phrase.field(mangleString("name", "testVocbase::test_analyzer"));
    phrase.push_back("q").push_back("u").push_back("i").push_back("c").push_back("k");

    assertFilterSuccess("FOR d IN myView FILTER Analyzer(phrase(d.name, 'quick'), 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER Analyzer(phrase(d['name'], 'quick'), 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER Analyzer(phRase(d.name, 'quick'), 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER Analyzer(phRase(d['name'], 'quick'), 'test_analyzer') RETURN d", expected);

    // overridden
    assertFilterSuccess("FOR d IN myView FILTER phrase(d.name, 'quick', 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER phrase(d['name'], 'quick', 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER phRase(d.name, 'quick', 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER phRase(d['name'], 'quick', 'test_analyzer') RETURN d", expected);

    // invalid attribute access
    assertFilterFail("FOR d IN myView FILTER analYzER(phrase(d, 'quick'), 'test_analyzer') RETURN d");
    assertFilterFail("FOR d IN myView FILTER analYzER(phrase(d[*], 'quick'), 'test_analyzer') RETURN d");
    assertFilterFail("FOR d IN myView FILTER analYzER(phrase(d.a.b[*].c, 'quick'), 'test_analyzer') RETURN d");
    assertFilterFail("FOR d IN myView FILTER analYzER(phrase('d.name', 'quick'), 'test_analyzer') RETURN d");
    assertFilterFail("FOR d IN myView FILTER analYzER(phrase(123, 'quick'), 'test_analyzer') RETURN d");
    assertFilterFail("FOR d IN myView FILTER analYzER(phrase(123.5, 'quick'), 'test_analyzer') RETURN d");
    assertFilterFail("FOR d IN myView FILTER analYzER(phrase(null, 'quick'), 'test_analyzer') RETURN d");
    assertFilterFail("FOR d IN myView FILTER analYzER(phrase(true, 'quick'), 'test_analyzer') RETURN d");
    assertFilterFail("FOR d IN myView FILTER analYzER(phrase(false, 'quick'), 'test_analyzer') RETURN d");

    // invalid input
    assertFilterFail("FOR d IN myView FILTER ANALYZER(phrase(d.name, [ ]), 'test_analyzer') RETURN d");
    assertFilterFail("FOR d IN myView FILTER ANALYZER(phrase(d['name'], [ ]), 'test_analyzer') RETURN d");
    assertFilterFail("FOR d IN myView FILTER ANALYZER(phrase(d.name, [ 1, \"abc\" ]), 'test_analyzer') RETURN d");
    assertFilterFail("FOR d IN myView FILTER ANALYZER(phrase(d['name'], [ 1, \"abc\" ]), 'test_analyzer') RETURN d");
    assertFilterFail("FOR d IN myView FILTER ANALYZER(phrase(d.name, true), 'test_analyzer') RETURN d");
    assertFilterFail("FOR d IN myView FILTER ANALYZER(phrase(d['name'], false), 'test_analyzer') RETURN d");
    assertFilterFail("FOR d IN myView FILTER ANALYZER(phrase(d.name, null), 'test_analyzer') RETURN d");
    assertFilterFail("FOR d IN myView FILTER ANALYZER(phrase(d['name'], null), 'test_analyzer') RETURN d");
    assertFilterFail("FOR d IN myView FILTER ANALYZER(phrase(d.name, 3.14), 'test_analyzer') RETURN d");
    assertFilterFail("FOR d IN myView FILTER ANALYZER(phrase(d['name'], 1234), 'test_analyzer') RETURN d");
    assertFilterFail("FOR d IN myView FILTER ANALYZER(phrase(d.name, { \"a\": 7, \"b\": \"c\" }), 'test_analyzer') RETURN d");
    assertFilterFail("FOR d IN myView FILTER ANALYZER(phrase(d['name'], { \"a\": 7, \"b\": \"c\" }), 'test_analyzer') RETURN d");
  }

  // dynamic complex attribute field
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    irs::Or expected;
    auto& phrase = expected.add<irs::by_phrase>();
    phrase.field(mangleString("a.b.c.e[4].f[5].g[3].g.a", "testVocbase::test_analyzer"));
    phrase.push_back("q").push_back("u").push_back("i").push_back("c").push_back("k");

    assertFilterSuccess("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER analyzer(phrase(d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')], 'quick'), 'test_analyzer') RETURN d", expected, &ctx);
    assertFilterSuccess("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER phrase(d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')], 'quick', 'test_analyzer') RETURN d", expected, &ctx);
  }

  // invalid dynamic attribute name
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER analyzer(phrase(d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')], 'quick'), 'test_analyzer') RETURN d", &ctx);
    assertFilterExecutionFail("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER phrase(d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')], 'quick', 'test_analyzer') RETURN d", &ctx);
  }

  // invalid dynamic attribute name (null value)
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintNull{})); // invalid value type
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a=null LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER analyzer(phrase(d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')], 'quick'), 'test_analyzer') RETURN d", &ctx);
    assertFilterExecutionFail("LET a=null LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER phrase(d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')], 'quick', 'test_analyzer') RETURN d", &ctx);
  }

  // invalid dynamic attribute name (bool value)
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintBool{false})); // invalid value type
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a=false LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER AnalyzeR(phrase(d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')], 'quick'), 'test_analyzer') RETURN d", &ctx);
    assertFilterExecutionFail("LET a=false LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER phrase(d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')], 'quick', 'test_analyzer') RETURN d", &ctx);
  }

  // field with simple offset
  // without offset, custom analyzer
  // quick
  {
    irs::Or expected;
    auto& phrase = expected.add<irs::by_phrase>();
    phrase.field(mangleString("[42]", "testVocbase::test_analyzer"));
    phrase.push_back("q").push_back("u").push_back("i").push_back("c").push_back("k");

    assertFilterSuccess("FOR d IN myView FILTER AnalYZER(phrase(d[42], 'quick'), 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER phrase(d[42], 'quick', 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER AnalYZER(phrase(d[42], [ 'quick' ]), 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER phrase(d[42], [ 'quick' ], 'test_analyzer') RETURN d", expected);
  }

  // without offset, custom analyzer, expressions
  // quick
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("value", arangodb::aql::AqlValue("qui"));
    ctx.vars.emplace("analyzer", arangodb::aql::AqlValue("test_"));

    irs::Or expected;
    auto& phrase = expected.add<irs::by_phrase>();
    phrase.field(mangleString("name", "testVocbase::test_analyzer"));
    phrase.push_back("q").push_back("u").push_back("i").push_back("c").push_back("k");

    assertFilterSuccess("LET value='qui' LET analyzer='test_' FOR d IN myView FILTER AnAlYzEr(phrase(d.name, CONCAT(value,'ck')), CONCAT(analyzer, 'analyzer')) RETURN d", expected, &ctx);
    assertFilterSuccess("LET value='qui' LET analyzer='test_' FOR d IN myView FILTER AnAlYzEr(phrase(d['name'], CONCAT(value, 'ck')), CONCAT(analyzer, 'analyzer')) RETURN d", expected, &ctx);
    assertFilterSuccess("LET value='qui' LET analyzer='test_' FOR d IN myView FILTER AnALYzEr(phrase(d.name, [ CONCAT(value,'ck') ]), CONCAT(analyzer, 'analyzer')) RETURN d", expected, &ctx);
    assertFilterSuccess("LET value='qui' LET analyzer='test_' FOR d IN myView FILTER AnAlYzEr(phrase(d['name'], [ CONCAT(value, 'ck') ]), CONCAT(analyzer, 'analyzer')) RETURN d", expected, &ctx);
    assertFilterSuccess("LET value='qui' LET analyzer='test_' FOR d IN myView FILTER AnALYzEr(phRase(d.name, CONCAT(value, 'ck')), CONCAT(analyzer, 'analyzer')) RETURN d", expected, &ctx);
    assertFilterSuccess("LET value='qui' LET analyzer='test_' FOR d IN myView FILTER AnAlYZEr(phRase(d['name'], CONCAT(value, 'ck')), CONCAT(analyzer, 'analyzer')) RETURN d", expected, &ctx);
    assertFilterSuccess("LET value='qui' LET analyzer='test_' FOR d IN myView FILTER AnAlYzEr(phRase(d.name, [ CONCAT(value, 'ck') ]), CONCAT(analyzer, 'analyzer')) RETURN d", expected, &ctx);
    assertFilterSuccess("LET value='qui' LET analyzer='test_' FOR d IN myView FILTER AnAlYzEr(phRase(d['name'], [ CONCAT(value, 'ck') ]), CONCAT(analyzer, 'analyzer')) RETURN d", expected, &ctx);
    assertFilterSuccess("LET value='qui' LET analyzer='test_' FOR d IN myView FILTER phrase(d.name, CONCAT(value,'ck'), CONCAT(analyzer, 'analyzer')) RETURN d", expected, &ctx);
    assertFilterSuccess("LET value='qui' LET analyzer='test_' FOR d IN myView FILTER phrase(d['name'], CONCAT(value, 'ck'), CONCAT(analyzer, 'analyzer')) RETURN d", expected, &ctx);
    assertFilterSuccess("LET value='qui' LET analyzer='test_' FOR d IN myView FILTER phrase(d.name, [ CONCAT(value,'ck') ], CONCAT(analyzer, 'analyzer')) RETURN d", expected, &ctx);
    assertFilterSuccess("LET value='qui' LET analyzer='test_' FOR d IN myView FILTER phrase(d['name'], [ CONCAT(value, 'ck') ], CONCAT(analyzer, 'analyzer')) RETURN d", expected, &ctx);
    assertFilterSuccess("LET value='qui' LET analyzer='test_' FOR d IN myView FILTER phRase(d.name, CONCAT(value, 'ck'), CONCAT(analyzer, 'analyzer')) RETURN d", expected, &ctx);
    assertFilterSuccess("LET value='qui' LET analyzer='test_' FOR d IN myView FILTER phRase(d['name'], CONCAT(value, 'ck'), CONCAT(analyzer, 'analyzer')) RETURN d", expected, &ctx);
    assertFilterSuccess("LET value='qui' LET analyzer='test_' FOR d IN myView FILTER phRase(d.name, [ CONCAT(value, 'ck') ], CONCAT(analyzer, 'analyzer')) RETURN d", expected, &ctx);
    assertFilterSuccess("LET value='qui' LET analyzer='test_' FOR d IN myView FILTER phRase(d['name'], [ CONCAT(value, 'ck') ], CONCAT(analyzer, 'analyzer')) RETURN d", expected, &ctx);
  }

  // without offset, custom analyzer, invalid expressions
  // quick
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("value", arangodb::aql::AqlValue("qui"));
    ctx.vars.emplace("analyzer", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintBool{false}));

    assertFilterExecutionFail("LET value='qui' LET analyzer='test_' FOR d IN myView FILTER ANALYZER(phrase(d.name, CONCAT(value,'ck')), analyzer) RETURN d", &ctx);
    assertFilterExecutionFail("LET value='qui' LET analyzer='test_' FOR d IN myView FILTER ANALYZER(phrase(d['name'], CONCAT(value, 'ck')), analyzer) RETURN d", &ctx);
    assertFilterExecutionFail("LET value='qui' LET analyzer='test_' FOR d IN myView FILTER ANALYZER(phrase(d.name, [ CONCAT(value,'ck') ]), analyzer) RETURN d", &ctx);
    assertFilterExecutionFail("LET value='qui' LET analyzer='test_' FOR d IN myView FILTER ANALYZER(phrase(d['name'], [ CONCAT(value, 'ck') ]), analyzer) RETURN d", &ctx);
    assertFilterExecutionFail("LET value='qui' LET analyzer='test_' FOR d IN myView FILTER ANALYZER(phRase(d.name, CONCAT(value, 'ck')), analyzer) RETURN d", &ctx);
    assertFilterExecutionFail("LET value='qui' LET analyzer='test_' FOR d IN myView FILTER ANALYZER(phRase(d['name'], CONCAT(value, 'ck')), analyzer) RETURN d", &ctx);
    assertFilterExecutionFail("LET value='qui' LET analyzer='test_' FOR d IN myView FILTER ANALYZER(phRase(d.name, [ CONCAT(value, 'ck') ]), analyzer) RETURN d", &ctx);
    assertFilterExecutionFail("LET value='qui' LET analyzer='test_' FOR d IN myView FILTER ANALYZER(phRase(d['name'], [ CONCAT(value, 'ck') ]), analyzer) RETURN d", &ctx);
    assertFilterExecutionFail("LET value='qui' LET analyzer='test_' FOR d IN myView FILTER phrase(d.name, CONCAT(value,'ck'), analyzer) RETURN d", &ctx);
    assertFilterExecutionFail("LET value='qui' LET analyzer='test_' FOR d IN myView FILTER phrase(d['name'], CONCAT(value, 'ck'), analyzer) RETURN d", &ctx);
    assertFilterExecutionFail("LET value='qui' LET analyzer='test_' FOR d IN myView FILTER phrase(d.name, [ CONCAT(value,'ck') ], analyzer) RETURN d", &ctx);
    assertFilterExecutionFail("LET value='qui' LET analyzer='test_' FOR d IN myView FILTER phrase(d['name'], [ CONCAT(value, 'ck') ], analyzer) RETURN d", &ctx);
    assertFilterExecutionFail("LET value='qui' LET analyzer='test_' FOR d IN myView FILTER phRase(d.name, CONCAT(value, 'ck'), analyzer) RETURN d", &ctx);
    assertFilterExecutionFail("LET value='qui' LET analyzer='test_' FOR d IN myView FILTER phRase(d['name'], CONCAT(value, 'ck'), analyzer) RETURN d", &ctx);
    assertFilterExecutionFail("LET value='qui' LET analyzer='test_' FOR d IN myView FILTER phRase(d.name, [ CONCAT(value, 'ck') ], analyzer) RETURN d", &ctx);
    assertFilterExecutionFail("LET value='qui' LET analyzer='test_' FOR d IN myView FILTER phRase(d['name'], [ CONCAT(value, 'ck') ], analyzer) RETURN d", &ctx);
    assertFilterExecutionFail("LET value='qui' LET analyzer='test_' FOR d IN myView FILTER analyzer(phRase(d['name'], [ CONCAT(value, 'ck') ], analyzer), 'identity') RETURN d", &ctx);
  }

  // with offset, custom analyzer
  // quick brown
  {
    irs::Or expected;
    auto& phrase = expected.add<irs::by_phrase>();
    phrase.field(mangleString("name", "testVocbase::test_analyzer"));
    phrase.push_back("q").push_back("u").push_back("i").push_back("c").push_back("k");
    phrase.push_back("b").push_back("r").push_back("o").push_back("w").push_back("n");

    assertFilterSuccess("FOR d IN myView FILTER aNALYZER(phrase(d.name, 'quick', 0, 'brown'), 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER aNALYZER(phrase(d.name, 'quick', 0.0, 'brown'), 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER aNALYZER(phrase(d.name, 'quick', 0.5, 'brown'), 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER aNALYZER(phrase(d.name, [ 'quick', 0, 'brown' ]), 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER aNALYZER(phrase(d.name, [ 'quick', 0.0, 'brown' ]), 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER aNALYZER(phrase(d.name, [ 'quick', 0.5, 'brown' ]), 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER phrase(d.name, 'quick', 0, 'brown', 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER phrase(d.name, 'quick', 0.0, 'brown', 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER phrase(d.name, 'quick', 0.5, 'brown', 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER phrase(d.name, [ 'quick', 0, 'brown' ], 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER phrase(d.name, [ 'quick', 0.0, 'brown' ], 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER phrase(d.name, [ 'quick', 0.5, 'brown' ], 'test_analyzer') RETURN d", expected);

    // wrong offset argument
    assertFilterFail("FOR d IN myView FILTER Analyzer(phrase(d.name, 'quick', '0', 'brown'), 'test_analyzer') RETURN d");
    assertFilterFail("FOR d IN myView FILTER Analyzer(phrase(d.name, 'quick', null, 'brown'), 'test_analyzer') RETURN d");
    assertFilterFail("FOR d IN myView FILTER Analyzer(phrase(d.name, 'quick', true, 'brown'), 'test_analyzer') RETURN d");
    assertFilterFail("FOR d IN myView FILTER Analyzer(phrase(d.name, 'quick', false, 'brown'), 'test_analyzer') RETURN d");
    assertFilterExecutionFail("FOR d IN myView FILTER AnalYZER(phrase(d.name, 'quick', d.name, 'brown'), 'test_analyzer') RETURN d", &ExpressionContextMock::EMPTY);
    assertFilterFail("FOR d IN myView FILTER AnaLYZER(phrase(d.name, [ 'quick', '0', 'brown' ]), 'test_analyzer') RETURN d");
    assertFilterFail("FOR d IN myView FILTER AnaLYZER(phrase(d.name, [ 'quick', null, 'brown' ]), 'test_analyzer') RETURN d");
    assertFilterFail("FOR d IN myView FILTER AnaLYZER(phrase(d.name, [ 'quick', true, 'brown' ]), 'test_analyzer') RETURN d");
    assertFilterFail("FOR d IN myView FILTER AnaLYZER(phrase(d.name, [ 'quick', false, 'brown' ]), 'test_analyzer') RETURN d");
    assertFilterExecutionFail("FOR d IN myView FILTER ANALYZER(phrase(d.name, [ 'quick', d.name, 'brown' ]), 'test_analyzer') RETURN d", &ExpressionContextMock::EMPTY);
  }

  // with offset, complex name, custom analyzer
  // quick <...> <...> <...> <...> <...> brown
  {
    irs::Or expected;
    auto& phrase = expected.add<irs::by_phrase>();
    phrase.field(mangleString("obj.name", "testVocbase::test_analyzer"));
    phrase.push_back("q").push_back("u").push_back("i").push_back("c").push_back("k");
    phrase.push_back("b", 5).push_back("r").push_back("o").push_back("w").push_back("n");

    assertFilterSuccess("FOR d IN myView FILTER analyzer(phrase(d['obj']['name'], 'quick', 5, 'brown'), 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER analyzer(phrase(d.obj.name, 'quick', 5, 'brown'), 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER analyzer(phrase(d.obj.name, 'quick', 5.0, 'brown'), 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER analyzer(phrase(d.obj['name'], 'quick', 5.0, 'brown'), 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER analyzer(phrase(d.obj.name, 'quick', 5.6, 'brown'), 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER analyzer(phrase(d['obj']['name'], 'quick', 5.5, 'brown'), 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER analyzer(phrase(d['obj']['name'], [ 'quick', 5, 'brown' ]), 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER analyzer(phrase(d.obj.name, [ 'quick', 5, 'brown' ]), 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER analyzer(phrase(d.obj.name, [ 'quick', 5.0, 'brown' ]), 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER analyzer(phrase(d.obj['name'], [ 'quick', 5.0, 'brown' ]), 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER analyzer(phrase(d.obj.name, [ 'quick', 5.6, 'brown' ]), 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER analyzer(phrase(d['obj']['name'], [ 'quick', 5.5, 'brown' ]), 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER phrase(d['obj']['name'], 'quick', 5, 'brown', 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER phrase(d.obj.name, 'quick', 5, 'brown', 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER phrase(d.obj.name, 'quick', 5.0, 'brown', 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER phrase(d.obj['name'], 'quick', 5.0, 'brown', 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER phrase(d.obj.name, 'quick', 5.6, 'brown', 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER phrase(d['obj']['name'], 'quick', 5.5, 'brown', 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER phrase(d['obj']['name'], [ 'quick', 5, 'brown' ], 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER phrase(d.obj.name, [ 'quick', 5, 'brown' ], 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER phrase(d.obj.name, [ 'quick', 5.0, 'brown' ], 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER phrase(d.obj['name'], [ 'quick', 5.0, 'brown' ], 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER phrase(d.obj.name, [ 'quick', 5.6, 'brown' ], 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER phrase(d['obj']['name'], [ 'quick', 5.5, 'brown' ], 'test_analyzer') RETURN d", expected);
  }

  // with offset, complex name, custom analyzer, boost
  // quick <...> <...> <...> <...> <...> brown
  {
    irs::Or expected;
    auto& phrase = expected.add<irs::by_phrase>();
    phrase.field(mangleString("obj.name", "testVocbase::test_analyzer")).boost(3.0f);
    phrase.push_back("q").push_back("u").push_back("i").push_back("c").push_back("k");
    phrase.push_back("b", 5).push_back("r").push_back("o").push_back("w").push_back("n");

    assertFilterSuccess("FOR d IN myView FILTER BOOST(analyzer(phrase(d['obj']['name'], 'quick', 5, 'brown'), 'test_analyzer'), 3) RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER BOoST(analyzer(phrase(d.obj.name, 'quick', 5, 'brown'), 'test_analyzer'), 2.9+0.1) RETURN d", expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess("FOR d IN myView FILTER Boost(analyzer(phrase(d.obj.name, 'quick', 5.0, 'brown'), 'test_analyzer'), 3.0) RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER BOOST(phrase(d['obj']['name'], 'quick', 5, 'brown', 'test_analyzer'), 3) RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER BOoST(phrase(d.obj.name, 'quick', 5, 'brown', 'test_analyzer'), 2.9+0.1) RETURN d", expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess("FOR d IN myView FILTER Boost(phrase(d.obj.name, 'quick', 5.0, 'brown', 'test_analyzer'), 3.0) RETURN d", expected);
  }

  // with offset, complex name with offset, custom analyzer
  // quick <...> <...> <...> <...> <...> brown
  {
    irs::Or expected;
    auto& phrase = expected.add<irs::by_phrase>();
    phrase.field(mangleString("obj[3].name[1]", "testVocbase::test_analyzer"));
    phrase.push_back("q").push_back("u").push_back("i").push_back("c").push_back("k");
    phrase.push_back("b", 5).push_back("r").push_back("o").push_back("w").push_back("n");

    assertFilterSuccess("FOR d IN myView FILTER Analyzer(phrase(d['obj'][3].name[1], 'quick', 5, 'brown'), 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER Analyzer(phrase(d.obj[3].name[1], 'quick', 5, 'brown'), 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER Analyzer(phrase(d.obj[3].name[1], 'quick', 5.0, 'brown'), 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER Analyzer(phrase(d.obj[3]['name'][1], 'quick', 5.0, 'brown'), 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER Analyzer(phrase(d.obj[3].name[1], 'quick', 5.5, 'brown'), 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER Analyzer(phrase(d['obj'][3]['name'][1], 'quick', 5.5, 'brown'), 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER Analyzer(phrase(d['obj'][3].name[1], [ 'quick', 5, 'brown' ]), 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER Analyzer(phrase(d.obj[3].name[1], [ 'quick', 5, 'brown' ]), 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER Analyzer(phrase(d.obj[3].name[1], [ 'quick', 5.0, 'brown' ]), 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER Analyzer(phrase(d.obj[3]['name'][1], [ 'quick', 5.0, 'brown' ]), 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER Analyzer(phrase(d.obj[3].name[1], [ 'quick', 5.5, 'brown' ]), 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER Analyzer(phrase(d['obj'][3]['name'][1], [ 'quick', 5.5, 'brown' ]), 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER phrase(d['obj'][3].name[1], 'quick', 5, 'brown', 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER phrase(d.obj[3].name[1], 'quick', 5, 'brown', 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER phrase(d.obj[3].name[1], 'quick', 5.0, 'brown', 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER phrase(d.obj[3]['name'][1], 'quick', 5.0, 'brown', 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER phrase(d.obj[3].name[1], 'quick', 5.5, 'brown', 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER phrase(d['obj'][3]['name'][1], 'quick', 5.5, 'brown', 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER phrase(d['obj'][3].name[1], [ 'quick', 5, 'brown' ], 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER phrase(d.obj[3].name[1], [ 'quick', 5, 'brown' ], 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER phrase(d.obj[3].name[1], [ 'quick', 5.0, 'brown' ], 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER phrase(d.obj[3]['name'][1], [ 'quick', 5.0, 'brown' ], 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER phrase(d.obj[3].name[1], [ 'quick', 5.5, 'brown' ], 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER phrase(d['obj'][3]['name'][1], [ 'quick', 5.5, 'brown' ], 'test_analyzer') RETURN d", expected);
  }

  // with offset, complex name, custom analyzer
  // quick <...> <...> <...> <...> <...> brown
  {
    irs::Or expected;
    auto& phrase = expected.add<irs::by_phrase>();
    phrase.field(mangleString("[5].obj.name[100]", "testVocbase::test_analyzer"));
    phrase.push_back("q").push_back("u").push_back("i").push_back("c").push_back("k");
    phrase.push_back("b", 5).push_back("r").push_back("o").push_back("w").push_back("n");

    assertFilterSuccess("FOR d IN myView FILTER ANALYZER(phrase(d[5]['obj'].name[100], 'quick', 5, 'brown'), 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER ANALYZER(phrase(d[5].obj.name[100], 'quick', 5, 'brown'), 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER ANALYZER(phrase(d[5].obj.name[100], 'quick', 5.0, 'brown'), 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER ANALYZER(phrase(d[5].obj['name'][100], 'quick', 5.0, 'brown'), 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER ANALYZER(phrase(d[5].obj.name[100], 'quick', 5.5, 'brown'), 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER ANALYZER(phrase(d[5]['obj']['name'][100], 'quick', 5.5, 'brown'), 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER ANALYZER(phrase(d[5]['obj'].name[100], [ 'quick', 5, 'brown' ]), 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER ANALYZER(phrase(d[5].obj.name[100], [ 'quick', 5, 'brown' ]), 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER ANALYZER(phrase(d[5].obj.name[100], [ 'quick', 5.0, 'brown' ]), 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER ANALYZER(phrase(d[5].obj['name'][100], [ 'quick', 5.0, 'brown' ]), 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER ANALYZER(phrase(d[5].obj.name[100], [ 'quick', 5.5, 'brown' ]), 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER ANALYZER(phrase(d[5]['obj']['name'][100], [ 'quick', 5.5, 'brown' ]), 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER phrase(d[5]['obj'].name[100], 'quick', 5, 'brown', 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER phrase(d[5].obj.name[100], 'quick', 5, 'brown', 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER phrase(d[5].obj.name[100], 'quick', 5.0, 'brown', 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER phrase(d[5].obj['name'][100], 'quick', 5.0, 'brown', 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER phrase(d[5].obj.name[100], 'quick', 5.5, 'brown', 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER phrase(d[5]['obj']['name'][100], 'quick', 5.5, 'brown', 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER phrase(d[5]['obj'].name[100], [ 'quick', 5, 'brown' ], 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER phrase(d[5].obj.name[100], [ 'quick', 5, 'brown' ], 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER phrase(d[5].obj.name[100], [ 'quick', 5.0, 'brown' ], 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER phrase(d[5].obj['name'][100], [ 'quick', 5.0, 'brown' ], 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER phrase(d[5].obj.name[100], [ 'quick', 5.5, 'brown' ], 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER phrase(d[5]['obj']['name'][100], [ 'quick', 5.5, 'brown' ], 'test_analyzer') RETURN d", expected);
  }

  // multiple offsets, complex name, custom analyzer
  // quick <...> <...> <...> brown <...> <...> fox jumps
  {
    irs::Or expected;
    auto& phrase = expected.add<irs::by_phrase>();
    phrase.field(mangleString("obj.properties.id.name", "testVocbase::test_analyzer"));
    phrase.push_back("q").push_back("u").push_back("i").push_back("c").push_back("k");
    phrase.push_back("b", 3).push_back("r").push_back("o").push_back("w").push_back("n");
    phrase.push_back("f", 2).push_back("o").push_back("x");
    phrase.push_back("j").push_back("u").push_back("m").push_back("p").push_back("s");

    assertFilterSuccess("FOR d IN myView FILTER analyzer(phrase(d.obj.properties.id.name, 'quick', 3, 'brown', 2, 'fox', 0, 'jumps'), 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER analyzer(phrase(d.obj.properties.id.name, 'quick', 3.0, 'brown', 2, 'fox', 0, 'jumps'), 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER analyzer(phrase(d.obj.properties.id['name'], 'quick', 3.0, 'brown', 2, 'fox', 0, 'jumps'), 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER analyzer(phrase(d.obj.properties.id.name, 'quick', 3.6, 'brown', 2, 'fox', 0, 'jumps'), 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER analyzer(phrase(d.obj['properties'].id.name, 'quick', 3.6, 'brown', 2, 'fox', 0, 'jumps'), 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER analyzer(phrase(d.obj.properties.id.name, 'quick', 3, 'brown', 2.0, 'fox', 0, 'jumps'), 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER analyzer(phrase(d.obj.properties.id.name, 'quick', 3, 'brown', 2.5, 'fox', 0.0, 'jumps'), 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER analyzer(phrase(d.obj.properties.id.name, 'quick', 3.2, 'brown', 2.0, 'fox', 0.0, 'jumps'), 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER analyzer(phrase(d['obj']['properties']['id']['name'], 'quick', 3.2, 'brown', 2.0, 'fox', 0.0, 'jumps'), 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER analyzer(phrase(d.obj.properties.id.name, [ 'quick', 3, 'brown', 2, 'fox', 0, 'jumps' ]), 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER analyzer(phrase(d.obj.properties.id.name, [ 'quick', 3.0, 'brown', 2, 'fox', 0, 'jumps' ]), 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER analyzer(phrase(d.obj.properties.id['name'], [ 'quick', 3.0, 'brown', 2, 'fox', 0, 'jumps' ]), 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER analyzer(phrase(d.obj.properties.id.name, [ 'quick', 3.6, 'brown', 2, 'fox', 0, 'jumps' ]), 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER analyzer(phrase(d.obj['properties'].id.name, [ 'quick', 3.6, 'brown', 2, 'fox', 0, 'jumps' ]), 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER analyzer(phrase(d.obj.properties.id.name, [ 'quick', 3, 'brown', 2.0, 'fox', 0, 'jumps' ]), 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER analyzer(phrase(d.obj.properties.id.name, [ 'quick', 3, 'brown', 2.5, 'fox', 0.0, 'jumps' ]), 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER analyzer(phrase(d.obj.properties.id.name, [ 'quick', 3.2, 'brown', 2.0, 'fox', 0.0, 'jumps' ]), 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER analyzer(phrase(d['obj']['properties']['id']['name'], [ 'quick', 3.2, 'brown', 2.0, 'fox', 0.0, 'jumps']), 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER phrase(d.obj.properties.id.name, 'quick', 3, 'brown', 2, 'fox', 0, 'jumps', 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER phrase(d.obj.properties.id.name, 'quick', 3.0, 'brown', 2, 'fox', 0, 'jumps', 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER phrase(d.obj.properties.id['name'], 'quick', 3.0, 'brown', 2, 'fox', 0, 'jumps', 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER phrase(d.obj.properties.id.name, 'quick', 3.6, 'brown', 2, 'fox', 0, 'jumps', 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER phrase(d.obj['properties'].id.name, 'quick', 3.6, 'brown', 2, 'fox', 0, 'jumps', 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER phrase(d.obj.properties.id.name, 'quick', 3, 'brown', 2.0, 'fox', 0, 'jumps', 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER phrase(d.obj.properties.id.name, 'quick', 3, 'brown', 2.5, 'fox', 0.0, 'jumps', 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER phrase(d.obj.properties.id.name, 'quick', 3.2, 'brown', 2.0, 'fox', 0.0, 'jumps', 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER phrase(d['obj']['properties']['id']['name'], 'quick', 3.2, 'brown', 2.0, 'fox', 0.0, 'jumps', 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER phrase(d.obj.properties.id.name, [ 'quick', 3, 'brown', 2, 'fox', 0, 'jumps' ], 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER phrase(d.obj.properties.id.name, [ 'quick', 3.0, 'brown', 2, 'fox', 0, 'jumps' ], 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER phrase(d.obj.properties.id['name'], [ 'quick', 3.0, 'brown', 2, 'fox', 0, 'jumps' ], 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER phrase(d.obj.properties.id.name, [ 'quick', 3.6, 'brown', 2, 'fox', 0, 'jumps' ], 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER phrase(d.obj['properties'].id.name, [ 'quick', 3.6, 'brown', 2, 'fox', 0, 'jumps' ], 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER phrase(d.obj.properties.id.name, [ 'quick', 3, 'brown', 2.0, 'fox', 0, 'jumps' ], 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER phrase(d.obj.properties.id.name, [ 'quick', 3, 'brown', 2.5, 'fox', 0.0, 'jumps' ], 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER phrase(d.obj.properties.id.name, [ 'quick', 3.2, 'brown', 2.0, 'fox', 0.0, 'jumps' ], 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER phrase(d['obj']['properties']['id']['name'], [ 'quick', 3.2, 'brown', 2.0, 'fox', 0.0, 'jumps'], 'test_analyzer') RETURN d", expected);

    // wrong value
    assertFilterExecutionFail("FOR d IN myView FILTER analyzer(phrase(d.obj.properties.id.name, 'quick', 3, d.brown, 2, 'fox', 0, 'jumps'), 'test_analyzer') RETURN d", &ExpressionContextMock::EMPTY);
    assertFilterFail("FOR d IN myView FILTER analyzer(phrase(d.obj.properties.id.name, 'quick', 3, 2, 2, 'fox', 0, 'jumps'), 'test_analyzer') RETURN d");
    assertFilterFail("FOR d IN myView FILTER analyzer(phrase(d.obj.properties.id.name, 'quick', 3, 2.5, 2, 'fox', 0, 'jumps'), 'test_analyzer') RETURN d");
    assertFilterFail("FOR d IN myView FILTER analyzer(phrase(d.obj.properties.id.name, 'quick', 3, null, 2, 'fox', 0, 'jumps'), 'test_analyzer') RETURN d");
    assertFilterFail("FOR d IN myView FILTER analyzer(phrase(d.obj.properties.id.name, 'quick', 3, true, 2, 'fox', 0, 'jumps'), 'test_analyzer') RETURN d");
    assertFilterFail("FOR d IN myView FILTER analyzer(phrase(d.obj.properties.id.name, 'quick', 3, false, 2, 'fox', 0, 'jumps'), 'test_analyzer') RETURN d");
    assertFilterExecutionFail("FOR d IN myView FILTER analyzer(phrase(d.obj.properties.id.name, 'quick', 3, 'brown', 2, 'fox', 0, d), 'test_analyzer') RETURN d", &ExpressionContextMock::EMPTY);
    assertFilterExecutionFail("FOR d IN myView FILTER analyzer(phrase(d.obj.properties.id.name, [ 'quick', 3, d.brown, 2, 'fox', 0, 'jumps' ]), 'test_analyzer') RETURN d", &ExpressionContextMock::EMPTY);
    assertFilterFail("FOR d IN myView FILTER analyZer(phrase(d.obj.properties.id.name, [ 'quick', 3, 2, 2, 'fox', 0, 'jumps']), 'test_analyzer') RETURN d");
    assertFilterFail("FOR d IN myView FILTER analyZer(phrase(d.obj.properties.id.name, [ 'quick', 3, 2.5, 2, 'fox', 0, 'jumps' ]), 'test_analyzer') RETURN d");
    assertFilterFail("FOR d IN myView FILTER analyZer(phrase(d.obj.properties.id.name, [ 'quick', 3, null, 2, 'fox', 0, 'jumps' ]), 'test_analyzer') RETURN d");
    assertFilterFail("FOR d IN myView FILTER analyZer(phrase(d.obj.properties.id.name, [ 'quick', 3, true, 2, 'fox', 0, 'jumps' ]), 'test_analyzer') RETURN d");
    assertFilterFail("FOR d IN myView FILTER analyZer(phrase(d.obj.properties.id.name, [ 'quick', 3, false, 2, 'fox', 0, 'jumps' ]), 'test_analyzer') RETURN d");
    assertFilterExecutionFail("FOR d IN myView FILTER analYZER(phrase(d.obj.properties.id.name, [ 'quick', 3, 'brown', 2, 'fox', 0, d ]), 'test_analyzer') RETURN d", &ExpressionContextMock::EMPTY);

    // wrong offset argument
    assertFilterFail("FOR d IN myView FILTER analyzer(phrase(d.obj.properties.id.name, 'quick', 3, 'brown', '2', 'fox', 0, 'jumps'), 'test_analyzer') RETURN d");
    assertFilterFail("FOR d IN myView FILTER analyzer(phrase(d.obj.properties.id.name, 'quick', 3, 'brown', null, 'fox', 0, 'jumps'), 'test_analyzer') RETURN d");
    assertFilterFail("FOR d IN myView FILTER analyzer(phrase(d.obj.properties.id.name, 'quick', 3, 'brown', true, 'fox', 0, 'jumps'), 'test_analyzer') RETURN d");
    assertFilterFail("FOR d IN myView FILTER analyzer(phrase(d.obj.properties.id.name, 'quick', 3, 'brown', false, 'fox', 0, 'jumps'), 'test_analyzer') RETURN d");
    assertFilterFail("FOR d IN myView FILTER analyzer(phrase(d.obj.properties.id.name, [ 'quick', 3, 'brown', '2', 'fox', 0, 'jumps' ]), 'test_analyzer') RETURN d");
    assertFilterFail("FOR d IN myView FILTER analyzer(phrase(d.obj.properties.id.name, [ 'quick', 3, 'brown', null, 'fox', 0, 'jumps' ]), 'test_analyzer') RETURN d");
    assertFilterFail("FOR d IN myView FILTER analyzer(phrase(d.obj.properties.id.name, [ 'quick', 3, 'brown', true, 'fox', 0, 'jumps' ]), 'test_analyzer') RETURN d");
    assertFilterFail("FOR d IN myView FILTER analyzer(phrase(d.obj.properties.id.name, [ 'quick', 3, 'brown', false, 'fox', 0, 'jumps' ]), 'test_analyzer') RETURN d");
  }

  // multiple offsets, complex name, custom analyzer, expressions
  // quick <...> <...> <...> brown <...> <...> fox jumps
  {
    irs::Or expected;
    auto& phrase = expected.add<irs::by_phrase>();
    phrase.field(mangleString("obj.properties.id.name", "testVocbase::test_analyzer"));
    phrase.push_back("q").push_back("u").push_back("i").push_back("c").push_back("k");
    phrase.push_back("b", 3).push_back("r").push_back("o").push_back("w").push_back("n");
    phrase.push_back("f", 2).push_back("o").push_back("x");
    phrase.push_back("j").push_back("u").push_back("m").push_back("p").push_back("s");

    ExpressionContextMock ctx;
    ctx.vars.emplace("offset", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt(2)));
    ctx.vars.emplace("input", arangodb::aql::AqlValue("bro"));

    assertFilterSuccess("LET offset=2 LET input='bro' FOR d IN myView FILTER analyzer(phrase(d.obj.properties.id.name, 'quick', offset+1, CONCAT(input, 'wn'), offset, 'fox', 0, 'jumps'), 'test_analyzer') RETURN d", expected, &ctx);
    assertFilterSuccess("LET offset=2 LET input='bro' FOR d IN myView FILTER analyzer(phrase(d.obj.properties.id.name, 'quick', offset + 1.5, 'brown', 2, 'fox', 0, 'jumps'), 'test_analyzer') RETURN d", expected, &ctx);
    assertFilterSuccess("LET offset=2 LET input='bro' FOR d IN myView FILTER analyzer(phrase(d.obj.properties.id['name'], 'quick', 3.0, 'brown', offset, 'fox', 0, 'jumps'), 'test_analyzer') RETURN d", expected, &ctx);
    assertFilterSuccess("LET offset=2 LET input='bro' FOR d IN myView FILTER analyzer(phrase(d.obj.properties.id.name, 'quick', 3.6, 'brown', 2, 'fox', offset-2, 'jumps'), 'test_analyzer') RETURN d", expected, &ctx);
    assertFilterSuccess("LET offset=2 LET input='bro' FOR d IN myView FILTER analyzer(phrase(d.obj['properties'].id.name, 'quick', 3.6, CONCAT(input, 'wn'), 2, 'fox', 0, 'jumps'), 'test_analyzer') RETURN d", expected, &ctx);
    assertFilterSuccess("LET offset=2 LET input='bro' FOR d IN myView FILTER analyzer(phrase(d.obj.properties.id.name, 'quick', 3, 'brown', offset+0.5, 'fox', 0.0, 'jumps'), 'test_analyzer') RETURN d", expected, &ctx);
    assertFilterSuccess("LET offset=2 LET input='bro' FOR d IN myView FILTER analyzer(phrase(d.obj.properties.id.name, [ 'quick', offset+1, CONCAT(input, 'wn'), offset, 'fox', 0, 'jumps' ]), 'test_analyzer') RETURN d", expected, &ctx);
    assertFilterSuccess("LET offset=2 LET input='bro' FOR d IN myView FILTER analyzer(phrase(d.obj.properties.id.name, [ 'quick', offset + 1.5, 'brown', 2, 'fox', 0, 'jumps' ]), 'test_analyzer') RETURN d", expected, &ctx);
    assertFilterSuccess("LET offset=2 LET input='bro' FOR d IN myView FILTER analyzer(phrase(d.obj.properties.id['name'], [ 'quick', 3.0, 'brown', offset, 'fox', 0, 'jumps' ]), 'test_analyzer') RETURN d", expected, &ctx);
    assertFilterSuccess("LET offset=2 LET input='bro' FOR d IN myView FILTER analyzer(phrase(d.obj.properties.id.name, [ 'quick', 3.6, 'brown', 2, 'fox', offset-2, 'jumps' ]), 'test_analyzer') RETURN d", expected, &ctx);
    assertFilterSuccess("LET offset=2 LET input='bro' FOR d IN myView FILTER analyzer(phrase(d.obj['properties'].id.name, [ 'quick', 3.6, CONCAT(input, 'wn'), 2, 'fox', 0, 'jumps' ]), 'test_analyzer') RETURN d", expected, &ctx);
    assertFilterSuccess("LET offset=2 LET input='bro' FOR d IN myView FILTER analyzer(phrase(d.obj.properties.id.name, [ 'quick', 3, 'brown', offset+0.5, 'fox', 0.0, 'jumps' ]), 'test_analyzer') RETURN d", expected, &ctx);
    assertFilterSuccess("LET offset=2 LET input='bro' FOR d IN myView FILTER phrase(d.obj.properties.id.name, 'quick', offset+1, CONCAT(input, 'wn'), offset, 'fox', 0, 'jumps', 'test_analyzer') RETURN d", expected, &ctx);
    assertFilterSuccess("LET offset=2 LET input='bro' FOR d IN myView FILTER phrase(d.obj.properties.id.name, 'quick', offset + 1.5, 'brown', 2, 'fox', 0, 'jumps', 'test_analyzer') RETURN d", expected, &ctx);
    assertFilterSuccess("LET offset=2 LET input='bro' FOR d IN myView FILTER phrase(d.obj.properties.id['name'], 'quick', 3.0, 'brown', offset, 'fox', 0, 'jumps', 'test_analyzer') RETURN d", expected, &ctx);
    assertFilterSuccess("LET offset=2 LET input='bro' FOR d IN myView FILTER phrase(d.obj.properties.id.name, 'quick', 3.6, 'brown', 2, 'fox', offset-2, 'jumps', 'test_analyzer') RETURN d", expected, &ctx);
    assertFilterSuccess("LET offset=2 LET input='bro' FOR d IN myView FILTER phrase(d.obj['properties'].id.name, 'quick', 3.6, CONCAT(input, 'wn'), 2, 'fox', 0, 'jumps', 'test_analyzer') RETURN d", expected, &ctx);
    assertFilterSuccess("LET offset=2 LET input='bro' FOR d IN myView FILTER phrase(d.obj.properties.id.name, 'quick', 3, 'brown', offset+0.5, 'fox', 0.0, 'jumps', 'test_analyzer') RETURN d", expected, &ctx);
    assertFilterSuccess("LET offset=2 LET input='bro' FOR d IN myView FILTER phrase(d.obj.properties.id.name, [ 'quick', offset+1, CONCAT(input, 'wn'), offset, 'fox', 0, 'jumps' ], 'test_analyzer') RETURN d", expected, &ctx);
    assertFilterSuccess("LET offset=2 LET input='bro' FOR d IN myView FILTER phrase(d.obj.properties.id.name, [ 'quick', offset + 1.5, 'brown', 2, 'fox', 0, 'jumps' ], 'test_analyzer') RETURN d", expected, &ctx);
    assertFilterSuccess("LET offset=2 LET input='bro' FOR d IN myView FILTER phrase(d.obj.properties.id['name'], [ 'quick', 3.0, 'brown', offset, 'fox', 0, 'jumps' ], 'test_analyzer') RETURN d", expected, &ctx);
    assertFilterSuccess("LET offset=2 LET input='bro' FOR d IN myView FILTER phrase(d.obj.properties.id.name, [ 'quick', 3.6, 'brown', 2, 'fox', offset-2, 'jumps' ], 'test_analyzer') RETURN d", expected, &ctx);
    assertFilterSuccess("LET offset=2 LET input='bro' FOR d IN myView FILTER phrase(d.obj['properties'].id.name, [ 'quick', 3.6, CONCAT(input, 'wn'), 2, 'fox', 0, 'jumps' ], 'test_analyzer') RETURN d", expected, &ctx);
    assertFilterSuccess("LET offset=2 LET input='bro' FOR d IN myView FILTER phrase(d.obj.properties.id.name, [ 'quick', 3, 'brown', offset+0.5, 'fox', 0.0, 'jumps' ], 'test_analyzer') RETURN d", expected, &ctx);
  }

  // multiple offsets, complex name, custom analyzer, invalid expressions
  // quick <...> <...> <...> brown <...> <...> fox jumps
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("offset", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt(2)));
    ctx.vars.emplace("input", arangodb::aql::AqlValue("bro"));

    assertFilterExecutionFail("LET offset=2 LET input='bro' FOR d IN myView FILTER analyzer(phrase(d.obj.properties.id.name, 'quick', TO_BOOL(offset+1), CONCAT(input, 'wn'), offset, 'fox', 0, 'jumps'), 'test_analyzer') RETURN d", &ctx);
    assertFilterExecutionFail("LET offset=2 LET input='bro' FOR d IN myView FILTER analyzer(phrase(d.obj.properties.id.name, 'quick', offset + 1.5, 'brown', TO_STRING(2), 'fox', 0, 'jumps'), 'test_analyzer') RETURN d", &ctx);
    assertFilterExecutionFail("LET offset=2 LET input='bro' FOR d IN myView FILTER analyzer(phrase(d.obj.properties.id['name'], 'quick', 3.0, 'brown', offset, 'fox', 0, 'jumps'), TO_BOOL('test_analyzer')) RETURN d", &ctx);
    assertFilterExecutionFail("LET offset=2 LET input='bro' FOR d IN myView FILTER analyzer(phrase(d.obj.properties.id.name, 'quick', TO_BOOL(3.6), 'brown', 2, 'fox', offset-2, 'jumps'), 'test_analyzer') RETURN d", &ctx);
    assertFilterExecutionFail("LET offset=2 LET input='bro' FOR d IN myView FILTER analyzer(phrase(d.obj.properties.id.name, [ 'quick', TO_BOOL(offset+1), CONCAT(input, 'wn'), offset, 'fox', 0, 'jumps' ]), 'test_analyzer') RETURN d", &ctx);
    assertFilterExecutionFail("LET offset=2 LET input='bro' FOR d IN myView FILTER analyzer(phrase(d.obj.properties.id.name, [ 'quick', offset + 1.5, 'brown', TO_STRING(2), 'fox', 0, 'jumps' ]), 'test_analyzer') RETURN d", &ctx);
    assertFilterExecutionFail("LET offset=2 LET input='bro' FOR d IN myView FILTER analyzer(phrase(d.obj.properties.id['name'], [ 'quick', 3.0, 'brown', offset, 'fox', 0, 'jumps' ]), TO_BOOL('test_analyzer')) RETURN d", &ctx);
    assertFilterExecutionFail("LET offset=2 LET input='bro' FOR d IN myView FILTER analyzer(phrase(d.obj.properties.id.name, [ 'quick', TO_BOOL(3.6), 'brown', 2, 'fox', offset-2, 'jumps' ]), 'test_analyzer') RETURN d", &ctx);
    assertFilterExecutionFail("LET offset=2 LET input='bro' FOR d IN myView FILTER phrase(d.obj.properties.id.name, 'quick', TO_BOOL(offset+1), CONCAT(input, 'wn'), offset, 'fox', 0, 'jumps', 'test_analyzer') RETURN d", &ctx);
    assertFilterExecutionFail("LET offset=2 LET input='bro' FOR d IN myView FILTER phrase(d.obj.properties.id.name, 'quick', offset + 1.5, 'brown', TO_STRING(2), 'fox', 0, 'jumps', 'test_analyzer') RETURN d", &ctx);
    assertFilterExecutionFail("LET offset=2 LET input='bro' FOR d IN myView FILTER phrase(d.obj.properties.id['name'], 'quick', 3.0, 'brown', offset, 'fox', 0, 'jumps', TO_BOOL('test_analyzer')) RETURN d", &ctx);
    assertFilterExecutionFail("LET offset=2 LET input='bro' FOR d IN myView FILTER phrase(d.obj.properties.id.name, 'quick', TO_BOOL(3.6), 'brown', 2, 'fox', offset-2, 'jumps', 'test_analyzer') RETURN d", &ctx);
    assertFilterExecutionFail("LET offset=2 LET input='bro' FOR d IN myView FILTER phrase(d.obj.properties.id.name, [ 'quick', TO_BOOL(offset+1), CONCAT(input, 'wn'), offset, 'fox', 0, 'jumps' ], 'test_analyzer') RETURN d", &ctx);
    assertFilterExecutionFail("LET offset=2 LET input='bro' FOR d IN myView FILTER phrase(d.obj.properties.id.name, [ 'quick', offset + 1.5, 'brown', TO_STRING(2), 'fox', 0, 'jumps' ], 'test_analyzer') RETURN d", &ctx);
    assertFilterExecutionFail("LET offset=2 LET input='bro' FOR d IN myView FILTER phrase(d.obj.properties.id['name'], [ 'quick', 3.0, 'brown', offset, 'fox', 0, 'jumps' ], TO_BOOL('test_analyzer')) RETURN d", &ctx);
    assertFilterExecutionFail("LET offset=2 LET input='bro' FOR d IN myView FILTER phrase(d.obj.properties.id.name, [ 'quick', TO_BOOL(3.6), 'brown', 2, 'fox', offset-2, 'jumps' ], 'test_analyzer') RETURN d", &ctx);
  }

  // invalid analyzer
  assertFilterFail("FOR d IN myView FILTER analyzer(phrase(d.name, 'quick'), [ 1, \"abc\" ]) RETURN d");
  assertFilterFail("FOR d IN myView FILTER analyzer(phrase(d['name'], 'quick'), [ 1, \"abc\" ]) RETURN d");
  assertFilterFail("FOR d IN myView FILTER analyzer(phrase(d.name, 'quick'), true) RETURN d");
  assertFilterFail("FOR d IN myView FILTER analyzer(phrase(d['name'], 'quick'), false) RETURN d");
  assertFilterFail("FOR d IN myView FILTER analyzer(phrase(d.name, 'quick'), null) RETURN d");
  assertFilterFail("FOR d IN myView FILTER analyzer(phrase(d['name'], 'quick'), null) RETURN d");
  assertFilterFail("FOR d IN myView FILTER analyzer(phrase(d.name, 'quick'), 3.14) RETURN d");
  assertFilterFail("FOR d IN myView FILTER analyzer(phrase(d['name'], 'quick'), 1234) RETURN d");
  assertFilterFail("FOR d IN myView FILTER analyzer(phrase(d.name, 'quick'), { \"a\": 7, \"b\": \"c\" }) RETURN d");
  assertFilterFail("FOR d IN myView FILTER analyzer(phrase(d['name'], 'quick'), { \"a\": 7, \"b\": \"c\" }) RETURN d");
  assertFilterFail("FOR d IN myView FILTER analyzer(phrase(d.name, 'quick'), 'invalid_analyzer') RETURN d");
  assertFilterFail("FOR d IN myView FILTER analyzer(phrase(d['name'], 'quick'), 'invalid_analyzer') RETURN d");
  assertFilterFail("FOR d IN myView FILTER analyzer(phrase(d.name, [ 'quick' ]), [ 1, \"abc\" ]) RETURN d");
  assertFilterFail("FOR d IN myView FILTER analyzer(phrase(d['name'], [ 'quick' ]), [ 1, \"abc\" ]) RETURN d");
  assertFilterFail("FOR d IN myView FILTER analyzer(phrase(d.name, [ 'quick' ]), true) RETURN d");
  assertFilterFail("FOR d IN myView FILTER analyzer(phrase(d['name'], [ 'quick' ]), false) RETURN d");
  assertFilterFail("FOR d IN myView FILTER analyzer(phrase(d.name, [ 'quick' ]), null) RETURN d");
  assertFilterFail("FOR d IN myView FILTER analyzer(phrase(d['name'], [ 'quick' ]), null) RETURN d");
  assertFilterFail("FOR d IN myView FILTER analyzer(phrase(d.name, [ 'quick' ]), 3.14) RETURN d");
  assertFilterFail("FOR d IN myView FILTER analyzer(phrase(d['name'], [ 'quick' ]), 1234) RETURN d");
  assertFilterFail("FOR d IN myView FILTER analyzer(phrase(d.name, [ 'quick' ]), { \"a\": 7, \"b\": \"c\" }) RETURN d");
  assertFilterFail("FOR d IN myView FILTER analyzer(phrase(d['name'], [ 'quick' ]), { \"a\": 7, \"b\": \"c\" }) RETURN d");
  assertFilterFail("FOR d IN myView FILTER analyzer(phrase(d.name, [ 'quick' ]), 'invalid_analyzer') RETURN d");
  assertFilterFail("FOR d IN myView FILTER analyzer(phrase(d['name'], [ 'quick' ]), 'invalid_analyzer') RETURN d");
  assertFilterFail("FOR d IN myView FILTER phrase(d.name, 'quick', [ 1, \"abc\" ]) RETURN d");
  assertFilterFail("FOR d IN myView FILTER phrase(d['name'], 'quick', [ 1, \"abc\" ]) RETURN d");
  assertFilterFail("FOR d IN myView FILTER phrase(d.name, 'quick', true) RETURN d");
  assertFilterFail("FOR d IN myView FILTER phrase(d['name'], 'quick', false) RETURN d");
  assertFilterFail("FOR d IN myView FILTER phrase(d.name, 'quick', null) RETURN d");
  assertFilterFail("FOR d IN myView FILTER phrase(d['name'], 'quick', null) RETURN d");
  assertFilterFail("FOR d IN myView FILTER phrase(d.name, 'quick', 3.14) RETURN d");
  assertFilterFail("FOR d IN myView FILTER phrase(d['name'], 'quick', 1234) RETURN d");
  assertFilterFail("FOR d IN myView FILTER phrase(d.name, 'quick', { \"a\": 7, \"b\": \"c\" }) RETURN d");
  assertFilterFail("FOR d IN myView FILTER phrase(d['name'], 'quick', { \"a\": 7, \"b\": \"c\" }) RETURN d");
  assertFilterFail("FOR d IN myView FILTER phrase(d.name, 'quick', 'invalid_analyzer') RETURN d");
  assertFilterFail("FOR d IN myView FILTER phrase(d['name'], 'quick', 'invalid_analyzer') RETURN d");
  assertFilterFail("FOR d IN myView FILTER phrase(d.name, [ 'quick' ], [ 1, \"abc\" ]) RETURN d");
  assertFilterFail("FOR d IN myView FILTER phrase(d['name'], [ 'quick' ], [ 1, \"abc\" ]) RETURN d");
  assertFilterFail("FOR d IN myView FILTER phrase(d.name, [ 'quick' ], true) RETURN d");
  assertFilterFail("FOR d IN myView FILTER phrase(d['name'], [ 'quick' ], false) RETURN d");
  assertFilterFail("FOR d IN myView FILTER phrase(d.name, [ 'quick' ], null) RETURN d");
  assertFilterFail("FOR d IN myView FILTER phrase(d['name'], [ 'quick' ], null) RETURN d");
  assertFilterFail("FOR d IN myView FILTER phrase(d.name, [ 'quick' ], 3.14) RETURN d");
  assertFilterFail("FOR d IN myView FILTER phrase(d['name'], [ 'quick' ], 1234) RETURN d");
  assertFilterFail("FOR d IN myView FILTER phrase(d.name, [ 'quick' ], { \"a\": 7, \"b\": \"c\" }) RETURN d");
  assertFilterFail("FOR d IN myView FILTER phrase(d['name'], [ 'quick' ], { \"a\": 7, \"b\": \"c\" }) RETURN d");
  assertFilterFail("FOR d IN myView FILTER phrase(d.name, [ 'quick' ], 'invalid_analyzer') RETURN d");
  assertFilterFail("FOR d IN myView FILTER phrase(d['name'], [ 'quick' ], 'invalid_analyzer') RETURN d");

  // wrong analylzer
  assertFilterFail("FOR d IN myView FILTER analyzer(phrase(d.name, 'quick'), ['d']) RETURN d");
  assertFilterExecutionFail("FOR d IN myView FILTER analyzer(phrase(d.name, 'quick'), [d]) RETURN d", &ExpressionContextMock::EMPTY);
  assertFilterExecutionFail("FOR d IN myView FILTER analyzer(phrase(d.name, 'quick'), d) RETURN d", &ExpressionContextMock::EMPTY);
  assertFilterFail("FOR d IN myView FILTER analyzer(phrase(d.name, 'quick'), 3) RETURN d");
  assertFilterFail("FOR d IN myView FILTER analyzer(phrase(d.name, 'quick'), 3.0) RETURN d");
  assertFilterFail("FOR d IN myView FILTER analyzer(phrase(d.name, 'quick'), true) RETURN d");
  assertFilterFail("FOR d IN myView FILTER analyzer(phrase(d.name, 'quick'), false) RETURN d");
  assertFilterFail("FOR d IN myView FILTER analyzer(phrase(d.name, 'quick'), null) RETURN d");
  assertFilterFail("FOR d IN myView FILTER analyzer(phrase(d.name, 'quick'), 'invalidAnalyzer') RETURN d");
  assertFilterExecutionFail("FOR d IN myView FILTER analyzer(phrase(d.name, 'quick', 3, 'brown'), d) RETURN d", &ExpressionContextMock::EMPTY);
  assertFilterFail("FOR d IN myView FILTER analyzer(phrase(d.name, 'quick', 3, 'brown'), 3) RETURN d");
  assertFilterFail("FOR d IN myView FILTER analyzer(phrase(d.name, 'quick', 3, 'brown'), 3.0) RETURN d");
  assertFilterFail("FOR d IN myView FILTER analyzer(phrase(d.name, 'quick', 3, 'brown'), true) RETURN d");
  assertFilterFail("FOR d IN myView FILTER analyzer(phrase(d.name, 'quick', 3, 'brown'), false) RETURN d");
  assertFilterFail("FOR d IN myView FILTER analyzer(phrase(d.name, 'quick', 3, 'brown'), null) RETURN d");
  assertFilterFail("FOR d IN myView FILTER analyzer(phrase(d.name, 'quick', 3, 'brown'), 'invalidAnalyzer') RETURN d");
  assertFilterFail("FOR d IN myView FILTER analyzer(phrase(d.name, [ 'quick' ]), ['d']) RETURN d");
  assertFilterExecutionFail("FOR d IN myView FILTER analyzer(phrase(d.name, [ 'quick' ]), [d]) RETURN d", &ExpressionContextMock::EMPTY);
  assertFilterExecutionFail("FOR d IN myView FILTER analyzer(phrase(d.name, [ 'quick' ]), d) RETURN d", &ExpressionContextMock::EMPTY);
  assertFilterFail("FOR d IN myView FILTER analyzer(phrase(d.name, [ 'quick' ]), 3) RETURN d");
  assertFilterFail("FOR d IN myView FILTER analyzer(phrase(d.name, [ 'quick' ]), 3.0) RETURN d");
  assertFilterFail("FOR d IN myView FILTER analyzer(phrase(d.name, [ 'quick' ]), true) RETURN d");
  assertFilterFail("FOR d IN myView FILTER analyzer(phrase(d.name, [ 'quick' ]), false) RETURN d");
  assertFilterFail("FOR d IN myView FILTER analyzer(phrase(d.name, [ 'quick' ]), null) RETURN d");
  assertFilterFail("FOR d IN myView FILTER analyzer(phrase(d.name, [ 'quick' ]), 'invalidAnalyzer') RETURN d");
  assertFilterExecutionFail("FOR d IN myView FILTER ANALYZER(phrase(d.name, [ 'quick', 3, 'brown' ]), d) RETURN d", &ExpressionContextMock::EMPTY);
  assertFilterFail("FOR d IN myView FILTER analyzer(phrase(d.name, [ 'quick', 3, 'brown' ]), 3) RETURN d");
  assertFilterFail("FOR d IN myView FILTER analyzer(phrase(d.name, [ 'quick', 3, 'brown' ]), 3.0) RETURN d");
  assertFilterFail("FOR d IN myView FILTER analyzer(phrase(d.name, [ 'quick', 3, 'brown' ]), true) RETURN d");
  assertFilterFail("FOR d IN myView FILTER analyzer(phrase(d.name, [ 'quick', 3, 'brown' ]), false) RETURN d");
  assertFilterFail("FOR d IN myView FILTER analyzer(phrase(d.name, [ 'quick', 3, 'brown' ]), null) RETURN d");
  assertFilterFail("FOR d IN myView FILTER analyzer(phrase(d.name, [ 'quick', 3, 'brown' ]), 'invalidAnalyzer') RETURN d");
  assertFilterFail("FOR d IN myView FILTER phrase(d.name, 'quick', ['d']) RETURN d");
  assertFilterExecutionFail("FOR d IN myView FILTER phrase(d.name, 'quick', [d]) RETURN d", &ExpressionContextMock::EMPTY);
  assertFilterExecutionFail("FOR d IN myView FILTER phrase(d.name, 'quick', d) RETURN d", &ExpressionContextMock::EMPTY);
  assertFilterFail("FOR d IN myView FILTER phrase(d.name, 'quick', 3) RETURN d");
  assertFilterFail("FOR d IN myView FILTER phrase(d.name, 'quick', 3.0) RETURN d");
  assertFilterFail("FOR d IN myView FILTER phrase(d.name, 'quick', true) RETURN d");
  assertFilterFail("FOR d IN myView FILTER phrase(d.name, 'quick', false) RETURN d");
  assertFilterFail("FOR d IN myView FILTER phrase(d.name, 'quick', null) RETURN d");
  assertFilterFail("FOR d IN myView FILTER phrase(d.name, 'quick', 'invalidAnalyzer') RETURN d");
  assertFilterExecutionFail("FOR d IN myView FILTER phrase(d.name, 'quick', 3, 'brown', d) RETURN d", &ExpressionContextMock::EMPTY);
  assertFilterFail("FOR d IN myView FILTER phrase(d.name, 'quick', 3, 'brown', 3) RETURN d");
  assertFilterFail("FOR d IN myView FILTER phrase(d.name, 'quick', 3, 'brown', 3.0) RETURN d");
  assertFilterFail("FOR d IN myView FILTER phrase(d.name, 'quick', 3, 'brown', true) RETURN d");
  assertFilterFail("FOR d IN myView FILTER phrase(d.name, 'quick', 3, 'brown', false) RETURN d");
  assertFilterFail("FOR d IN myView FILTER phrase(d.name, 'quick', 3, 'brown', null) RETURN d");
  assertFilterFail("FOR d IN myView FILTER phrase(d.name, 'quick', 3, 'brown', 'invalidAnalyzer') RETURN d");
  assertFilterFail("FOR d IN myView FILTER phrase(d.name, [ 'quick' ], ['d']) RETURN d");
  assertFilterExecutionFail("FOR d IN myView FILTER phrase(d.name, [ 'quick' ], [d]) RETURN d", &ExpressionContextMock::EMPTY);
  assertFilterExecutionFail("FOR d IN myView FILTER phrase(d.name, [ 'quick' ], d) RETURN d", &ExpressionContextMock::EMPTY);
  assertFilterFail("FOR d IN myView FILTER phrase(d.name, [ 'quick' ], 3) RETURN d");
  assertFilterFail("FOR d IN myView FILTER phrase(d.name, [ 'quick' ], 3.0) RETURN d");
  assertFilterFail("FOR d IN myView FILTER phrase(d.name, [ 'quick' ], true) RETURN d");
  assertFilterFail("FOR d IN myView FILTER phrase(d.name, [ 'quick' ], false) RETURN d");
  assertFilterFail("FOR d IN myView FILTER phrase(d.name, [ 'quick' ], null) RETURN d");
  assertFilterFail("FOR d IN myView FILTER phrase(d.name, [ 'quick' ], 'invalidAnalyzer') RETURN d");
  assertFilterExecutionFail("FOR d IN myView FILTER phrase(d.name, [ 'quick', 3, 'brown' ], d) RETURN d", &ExpressionContextMock::EMPTY);
  assertFilterFail("FOR d IN myView FILTER phrase(d.name, [ 'quick', 3, 'brown' ], 3) RETURN d");
  assertFilterFail("FOR d IN myView FILTER phrase(d.name, [ 'quick', 3, 'brown' ], 3.0) RETURN d");
  assertFilterFail("FOR d IN myView FILTER phrase(d.name, [ 'quick', 3, 'brown' ], true) RETURN d");
  assertFilterFail("FOR d IN myView FILTER phrase(d.name, [ 'quick', 3, 'brown' ], false) RETURN d");
  assertFilterFail("FOR d IN myView FILTER phrase(d.name, [ 'quick', 3, 'brown' ], null) RETURN d");
  assertFilterFail("FOR d IN myView FILTER phrase(d.name, [ 'quick', 3, 'brown' ], 'invalidAnalyzer') RETURN d");

  // non-deterministic arguments
  assertFilterFail("FOR d IN myView FILTER analyzer(phrase(d[RAND() ? 'name' : 0], 'quick', 0, 'brown'), 'test_analyzer') RETURN d");
  assertFilterFail("FOR d IN myView FILTER analyzer(phrase(d.name, RAND() ? 'quick' : 'slow', 0, 'brown'), 'test_analyzer') RETURN d");
  assertFilterFail("FOR d IN myView FILTER analyzer(phrase(d.name, 'quick', 0, RAND() ? 'brown' : 'red'), 'test_analyzer') RETURN d");
  assertFilterFail("FOR d IN myView FILTER analyzer(phrase(d.name, 'quick', 0, 'brown'), RAND() ? 'test_analyzer' : 'invalid_analyzer') RETURN d");
  assertFilterFail("FOR d IN myView FILTER analyzer(phrase(d[RAND() ? 'name' : 0], [ 'quick', 0, 'brown' ]), 'test_analyzer') RETURN d");
  assertFilterFail("FOR d IN myView FILTER analyzer(phrase(d.name, [ RAND() ? 'quick' : 'slow', 0, 'brown' ]), 'test_analyzer') RETURN d");
  assertFilterFail("FOR d IN myView FILTER analyzer(phrase(d.name, [ 'quick', 0, RAND() ? 'brown' : 'red' ]), 'test_analyzer') RETURN d");
  assertFilterFail("FOR d IN myView FILTER analyzer(phrase(d.name, [ 'quick', 0, 'brown' ]), RAND() ? 'test_analyzer' : 'invalid_analyzer') RETURN d");
  assertFilterFail("FOR d IN myView FILTER phrase(d[RAND() ? 'name' : 0], 'quick', 0, 'brown', 'test_analyzer') RETURN d");
  assertFilterFail("FOR d IN myView FILTER phrase(d.name, RAND() ? 'quick' : 'slow', 0, 'brown', 'test_analyzer') RETURN d");
  assertFilterFail("FOR d IN myView FILTER phrase(d.name, 'quick', 0, RAND() ? 'brown' : 'red', 'test_analyzer') RETURN d");
  assertFilterFail("FOR d IN myView FILTER phrase(d.name, 'quick', 0, 'brown', RAND() ? 'test_analyzer' : 'invalid_analyzer') RETURN d");
  assertFilterFail("FOR d IN myView FILTER phrase(d[RAND() ? 'name' : 0], [ 'quick', 0, 'brown' ], 'test_analyzer') RETURN d");
  assertFilterFail("FOR d IN myView FILTER phrase(d.name, [ RAND() ? 'quick' : 'slow', 0, 'brown' ], 'test_analyzer') RETURN d");
  assertFilterFail("FOR d IN myView FILTER phrase(d.name, [ 'quick', 0, RAND() ? 'brown' : 'red' ], 'test_analyzer') RETURN d");
  assertFilterFail("FOR d IN myView FILTER phrase(d.name, [ 'quick', 0, 'brown' ], RAND() ? 'test_analyzer' : 'invalid_analyzer') RETURN d");
}

SECTION("StartsWith") {
  // without scoring limit
  {
    irs::Or expected;
    auto& prefix = expected.add<irs::by_prefix>();
    prefix.field(mangleStringIdentity("name")).term("abc");
    prefix.scored_terms_limit(128);

    assertFilterSuccess("FOR d IN myView FILTER starts_with(d['name'], 'abc') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER starts_with(d.name, 'abc') RETURN d", expected);
  }

  // dynamic complex attribute field
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    irs::Or expected;
    auto& prefix = expected.add<irs::by_prefix>();
    prefix.field(mangleStringIdentity("a.b.c.e[4].f[5].g[3].g.a")).term("abc");
    prefix.scored_terms_limit(128);

    assertFilterSuccess("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER starts_with(d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')], 'abc') RETURN d", expected, &ctx);
  }

  // invalid dynamic attribute name
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER starts_with(d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')], 'abc') RETURN d", &ctx);
  }

  // invalid dynamic attribute name (null value)
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintNull{})); // invalid value type
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a=null LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER starts_with(d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')], 'abc') RETURN d", &ctx);
  }

  // invalid dynamic attribute name (bool value)
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintBool{false})); // invalid value type
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a=false LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER starts_with(d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')], 'abc') RETURN d", &ctx);
  }

  // without scoring limit, name with offset
  {
    irs::Or expected;
    auto& prefix = expected.add<irs::by_prefix>();
    prefix.field(mangleStringIdentity("name[1]")).term("abc");
    prefix.scored_terms_limit(128);

    assertFilterSuccess("FOR d IN myView FILTER starts_with(d['name'][1], 'abc') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER starts_with(d.name[1], 'abc') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER analyzer(starts_with(d.name[1], 'abc'), 'identity') RETURN d", expected);
  }

  // without scoring limit, complex name
  {
    irs::Or expected;
    auto& prefix = expected.add<irs::by_prefix>();
    prefix.field(mangleStringIdentity("obj.properties.name")).term("abc");
    prefix.scored_terms_limit(128);

    assertFilterSuccess("FOR d IN myView FILTER starts_with(d['obj']['properties']['name'], 'abc') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER starts_with(d.obj['properties']['name'], 'abc') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER starts_with(d.obj['properties'].name, 'abc') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER analyzer(starts_with(d.obj['properties'].name, 'abc'), 'identity') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER starts_with(d.obj.properties.name, 'abc') RETURN d", expected);
  }

  // without scoring limit, complex name with offset
  {
    irs::Or expected;
    auto& prefix = expected.add<irs::by_prefix>();
    prefix.field(mangleStringIdentity("obj[400].properties[3].name")).term("abc");
    prefix.scored_terms_limit(128);

    assertFilterSuccess("FOR d IN myView FILTER starts_with(d['obj'][400]['properties'][3]['name'], 'abc') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER starts_with(d.obj[400]['properties[3]']['name'], 'abc') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER starts_with(d.obj[400]['properties[3]'].name, 'abc') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER starts_with(d.obj[400].properties[3].name, 'abc') RETURN d", expected);
  }

  // without scoring limit, complex name with offset, analyzer
  {
    irs::Or expected;
    auto& prefix = expected.add<irs::by_prefix>();
    prefix.field(mangleString("obj[400].properties[3].name", "testVocbase::test_analyzer")).term("abc");
    prefix.scored_terms_limit(128);

    assertFilterSuccess("FOR d IN myView FILTER Analyzer(starts_with(d['obj'][400]['properties'][3]['name'], 'abc'), 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER Analyzer(starts_with(d.obj[400]['properties[3]']['name'], 'abc'), 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER Analyzer(starts_with(d.obj[400]['properties[3]'].name, 'abc'), 'test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER Analyzer(starts_with(d.obj[400].properties[3].name, 'abc'), 'test_analyzer') RETURN d", expected);
  }

  // without scoring limit, complex name with offset, prefix as an expression
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("prefix", arangodb::aql::AqlValue("ab"));

    irs::Or expected;
    auto& prefix = expected.add<irs::by_prefix>();
    prefix.field(mangleStringIdentity("obj[400].properties[3].name")).term("abc");
    prefix.scored_terms_limit(128);

    assertFilterSuccess("LET prefix='ab' FOR d IN myView FILTER starts_with(d['obj'][400]['properties'][3]['name'], CONCAT(prefix, 'c')) RETURN d", expected, &ctx);
    assertFilterSuccess("LET prefix='ab' FOR d IN myView FILTER starts_with(d.obj[400]['properties[3]']['name'], CONCAT(prefix, 'c')) RETURN d", expected, &ctx);
    assertFilterSuccess("LET prefix='ab' FOR d IN myView FILTER starts_with(d.obj[400]['properties[3]'].name, CONCAT(prefix, 'c')) RETURN d", expected, &ctx);
    assertFilterSuccess("LET prefix='ab' FOR d IN myView FILTER starts_with(d.obj[400].properties[3].name, CONCAT(prefix, 'c')) RETURN d", expected, &ctx);
  }

  // without scoring limit, complex name with offset, prefix as an expression of invalid type
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("prefix", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintBool(false)));

    assertFilterExecutionFail("LET prefix=false FOR d IN myView FILTER starts_with(d['obj'][400]['properties'][3]['name'], prefix) RETURN d", &ctx);
    assertFilterExecutionFail("LET prefix=false FOR d IN myView FILTER starts_with(d.obj[400]['properties[3]']['name'], prefix) RETURN d", &ctx);
    assertFilterExecutionFail("LET prefix=false FOR d IN myView FILTER starts_with(d.obj[400]['properties[3]'].name, prefix) RETURN d", &ctx);
    assertFilterExecutionFail("LET prefix=false FOR d IN myView FILTER starts_with(d.obj[400].properties[3].name, prefix) RETURN d", &ctx);
  }

  // with scoring limit (int)
  {
    irs::Or expected;
    auto& prefix = expected.add<irs::by_prefix>();
    prefix.field(mangleStringIdentity("name")).term("abc");
    prefix.scored_terms_limit(1024);

    assertFilterSuccess("FOR d IN myView FILTER starts_with(d['name'], 'abc', 1024) RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER starts_with(d.name, 'abc', 1024) RETURN d", expected);
  }

  // with scoring limit (double)
  {
    irs::Or expected;
    auto& prefix = expected.add<irs::by_prefix>();
    prefix.field(mangleStringIdentity("name")).term("abc");
    prefix.scored_terms_limit(100);

    assertFilterSuccess("FOR d IN myView FILTER starts_with(d['name'], 'abc', 100.5) RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER starts_with(d.name, 'abc', 100.5) RETURN d", expected);
  }

  // with scoring limit (double), boost
  {
    irs::Or expected;
    auto& prefix = expected.add<irs::by_prefix>();
    prefix.field(mangleStringIdentity("name")).term("abc");
    prefix.scored_terms_limit(100);
    prefix.boost(3.1f);

    assertFilterSuccess("FOR d IN myView FILTER boost(starts_with(d['name'], 'abc', 100.5), 0.1+3) RETURN d", expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess("FOR d IN myView FILTER BooST(starts_with(d.name, 'abc', 100.5), 3.1) RETURN d", expected);
  }

  // without scoring limit, complex name with offset, scoringLimit as an expression
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("prefix", arangodb::aql::AqlValue("ab"));
    ctx.vars.emplace("scoringLimit", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt(5)));

    irs::Or expected;
    auto& prefix = expected.add<irs::by_prefix>();
    prefix.field(mangleStringIdentity("obj[400].properties[3].name")).term("abc");
    prefix.scored_terms_limit(6);

    assertFilterSuccess("LET scoringLimit=5 LET prefix='ab' FOR d IN myView FILTER starts_with(d['obj'][400]['properties'][3]['name'], CONCAT(prefix, 'c'), (scoringLimit + 1)) RETURN d", expected, &ctx);
    assertFilterSuccess("LET scoringLimit=5 LET prefix='ab' FOR d IN myView FILTER starts_with(d.obj[400]['properties[3]']['name'], CONCAT(prefix, 'c'), (scoringLimit + 1)) RETURN d", expected, &ctx);
    assertFilterSuccess("LET scoringLimit=5 LET prefix='ab' FOR d IN myView FILTER starts_with(d.obj[400]['properties[3]'].name, CONCAT(prefix, 'c'), (scoringLimit + 1)) RETURN d", expected, &ctx);
    assertFilterSuccess("LET scoringLimit=5 LET prefix='ab' FOR d IN myView FILTER starts_with(d.obj[400].properties[3].name, CONCAT(prefix, 'c'), (scoringLimit + 1)) RETURN d", expected, &ctx);
  }

  // without scoring limit, complex name with offset, scoringLimit as an expression
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("prefix", arangodb::aql::AqlValue("ab"));
    ctx.vars.emplace("scoringLimit", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt(5)));

    irs::Or expected;
    auto& prefix = expected.add<irs::by_prefix>();
    prefix.field(mangleStringIdentity("obj[400].properties[3].name")).term("abc");
    prefix.scored_terms_limit(6);

    assertFilterSuccess("LET scoringLimit=5 LET prefix='ab' FOR d IN myView FILTER starts_with(d['obj'][400]['properties'][3]['name'], CONCAT(prefix, 'c'), (scoringLimit + 1.5)) RETURN d", expected, &ctx);
    assertFilterSuccess("LET scoringLimit=5 LET prefix='ab' FOR d IN myView FILTER starts_with(d.obj[400]['properties[3]']['name'], CONCAT(prefix, 'c'), (scoringLimit + 1.5)) RETURN d", expected, &ctx);
    assertFilterSuccess("LET scoringLimit=5 LET prefix='ab' FOR d IN myView FILTER starts_with(d.obj[400]['properties[3]'].name, CONCAT(prefix, 'c'), (scoringLimit + 1.5)) RETURN d", expected, &ctx);
    assertFilterSuccess("LET scoringLimit=5 LET prefix='ab' FOR d IN myView FILTER starts_with(d.obj[400].properties[3].name, CONCAT(prefix, 'c'), (scoringLimit + 1.5)) RETURN d", expected, &ctx);
  }

  // without scoring limit, complex name with offset, scoringLimit as an expression, analyzer
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("prefix", arangodb::aql::AqlValue("ab"));
    ctx.vars.emplace("analyzer", arangodb::aql::AqlValue("analyzer"));
    ctx.vars.emplace("scoringLimit", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt(5)));

    irs::Or expected;
    auto& prefix = expected.add<irs::by_prefix>();
    prefix.field(mangleString("obj[400].properties[3].name", "testVocbase::test_analyzer")).term("abc");
    prefix.scored_terms_limit(6);

    assertFilterSuccess("LET scoringLimit=5 LET prefix='ab' LET analyzer='analyzer' FOR d IN myView FILTER analyzer(starts_with(d['obj'][400]['properties'][3]['name'], CONCAT(prefix, 'c'), (scoringLimit + 1.5)), CONCAT('test_',analyzer)) RETURN d", expected, &ctx);
    assertFilterSuccess("LET scoringLimit=5 LET prefix='ab' LET analyzer='analyzer' FOR d IN myView FILTER analyzer(starts_with(d.obj[400]['properties[3]']['name'], CONCAT(prefix, 'c'), (scoringLimit + 1.5)), CONCAT('test_',analyzer))  RETURN d", expected, &ctx);
    assertFilterSuccess("LET scoringLimit=5 LET prefix='ab' LET analyzer='analyzer' FOR d IN myView FILTER analyzer(starts_with(d.obj[400]['properties[3]'].name, CONCAT(prefix, 'c'), (scoringLimit + 1.5)), CONCAT('test_',analyzer))  RETURN d", expected, &ctx);
    assertFilterSuccess("LET scoringLimit=5 LET prefix='ab' LET analyzer='analyzer' FOR d IN myView FILTER analyzer(starts_with(d.obj[400].properties[3].name, CONCAT(prefix, 'c'), (scoringLimit + 1.5)), CONCAT('test_',analyzer))  RETURN d", expected, &ctx);
  }

  // without scoring limit, complex name with offset, scoringLimit as an expression of invalid type
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("prefix", arangodb::aql::AqlValue("ab"));
    ctx.vars.emplace("scoringLimit", arangodb::aql::AqlValue("ab"));

    assertFilterExecutionFail("LET scoringLimit='ab' LET prefix=false FOR d IN myView FILTER starts_with(d['obj'][400]['properties'][3]['name'], prefix, scoringLimit) RETURN d", &ctx);
    assertFilterExecutionFail("LET scoringLimit='ab' LET prefix=false FOR d IN myView FILTER starts_with(d.obj[400]['properties[3]']['name'], prefix, scoringLimit) RETURN d", &ctx);
    assertFilterExecutionFail("LET scoringLimit='ab' LET prefix=false FOR d IN myView FILTER starts_with(d.obj[400]['properties[3]'].name, prefix, scoringLimit) RETURN d", &ctx);
    assertFilterExecutionFail("LET scoringLimit='ab' LET prefix=false FOR d IN myView FILTER starts_with(d.obj[400].properties[3].name, prefix, scoringLimit) RETURN d", &ctx);
  }

  // wrong number of arguments
  assertFilterParseFail("FOR d IN myView FILTER starts_with() RETURN d");
  assertFilterParseFail("FOR d IN myView FILTER starts_with(d.name, 'abc', 100, 'abc') RETURN d");

  // invalid attribute access
  assertFilterFail("FOR d IN myView FILTER starts_with(['d'], 'abc') RETURN d");
  assertFilterFail("FOR d IN myView FILTER starts_with([d], 'abc') RETURN d");
  assertFilterFail("FOR d IN myView FILTER starts_with(d, 'abc') RETURN d");
  assertFilterFail("FOR d IN myView FILTER starts_with(d[*], 'abc') RETURN d");
  assertFilterFail("FOR d IN myView FILTER starts_with(d.a[*].c, 'abc') RETURN d");
  assertFilterFail("FOR d IN myView FILTER starts_with('d.name', 'abc') RETURN d");
  assertFilterFail("FOR d IN myView FILTER starts_with(123, 'abc') RETURN d");
  assertFilterFail("FOR d IN myView FILTER starts_with(123.5, 'abc') RETURN d");
  assertFilterFail("FOR d IN myView FILTER starts_with(null, 'abc') RETURN d");
  assertFilterFail("FOR d IN myView FILTER starts_with(true, 'abc') RETURN d");
  assertFilterFail("FOR d IN myView FILTER starts_with(false, 'abc') RETURN d");

  // invalid value
  assertFilterFail("FOR d IN myView FILTER starts_with(d.name, 1) RETURN d");
  assertFilterFail("FOR d IN myView FILTER starts_with(d.name, 1.5) RETURN d");
  assertFilterFail("FOR d IN myView FILTER starts_with(d.name, true) RETURN d");
  assertFilterFail("FOR d IN myView FILTER starts_with(d.name, false) RETURN d");
  assertFilterFail("FOR d IN myView FILTER starts_with(d.name, null) RETURN d");
  assertFilterExecutionFail("FOR d IN myView FILTER starts_with(d.name, d) RETURN d", &ExpressionContextMock::EMPTY);

  // invalid scoring limit
  assertFilterFail("FOR d IN myView FILTER starts_with(d.name, 'abc', '1024') RETURN d");
  assertFilterFail("FOR d IN myView FILTER starts_with(d.name, 'abc', true) RETURN d");
  assertFilterFail("FOR d IN myView FILTER starts_with(d.name, 'abc', false) RETURN d");
  assertFilterFail("FOR d IN myView FILTER starts_with(d.name, 'abc', null) RETURN d");
  assertFilterExecutionFail("FOR d IN myView FILTER starts_with(d.name, 'abc', d) RETURN d", &ExpressionContextMock::EMPTY);

  // non-deterministic arguments
  assertFilterFail("FOR d IN myView FILTER starts_with(d[RAND() ? 'name' : 'x'], 'abc') RETURN d");
  assertFilterFail("FOR d IN myView FILTER starts_with(d.name, RAND() ? 'abc' : 'def') RETURN d");
  assertFilterFail("FOR d IN myView FILTER starts_with(d.name, 'abc', RAND() ? 128 : 10) RETURN d");
}

SECTION("IN_RANGE") {
  // d.name > 'a' && d.name < 'z'
  {
    irs::Or expected;
    auto& range = expected.add<irs::by_range>();
    range.field(mangleStringIdentity("name"))
         .include<irs::Bound::MIN>(false).term<irs::Bound::MIN>("a")
         .include<irs::Bound::MAX>(false).term<irs::Bound::MAX>("z");

    assertFilterSuccess("FOR d IN myView FILTER in_range(d['name'], 'a', 'z', false, false) RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER in_range(d.name, 'a', 'z', false, false) RETURN d", expected);
  }

  // BOOST(d.name >= 'a' && d.name <= 'z', 1.5)
  {
    irs::Or expected;
    auto& range = expected.add<irs::by_range>();
    range.boost(1.5);
    range.field(mangleStringIdentity("name"))
         .include<irs::Bound::MIN>(true).term<irs::Bound::MIN>("a")
         .include<irs::Bound::MAX>(true).term<irs::Bound::MAX>("z");

    assertFilterSuccess("FOR d IN myView FILTER boost(in_range(d['name'], 'a', 'z', true, true), 1.5) RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER boost(in_range(d.name, 'a', 'z', true, true), 1.5) RETURN d", expected);
  }

  // ANALYZER(BOOST(d.name > 'a' && d.name <= 'z', 1.5), "testVocbase::test_analyzer")
  {
    irs::Or expected;
    auto& range = expected.add<irs::by_range>();
    range.boost(1.5);
    range.field(mangleString("name", "testVocbase::test_analyzer"))
         .include<irs::Bound::MIN>(false).term<irs::Bound::MIN>("a")
         .include<irs::Bound::MAX>(true).term<irs::Bound::MAX>("z");

    assertFilterSuccess("FOR d IN myView FILTER analyzer(boost(in_range(d['name'], 'a', 'z', false, true), 1.5), 'testVocbase::test_analyzer') RETURN d", expected);
    assertFilterSuccess("FOR d IN myView FILTER analyzer(boost(in_range(d.name, 'a', 'z', false, true), 1.5), 'testVocbase::test_analyzer') RETURN d", expected);
  }

  // dynamic complex attribute field
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    irs::Or expected;
    auto& range = expected.add<irs::by_range>();
    range.field(mangleStringIdentity("a.b.c.e[4].f[5].g[3].g.a"))
         .include<irs::Bound::MIN>(true).term<irs::Bound::MIN>("abc")
         .include<irs::Bound::MAX>(false).term<irs::Bound::MAX>("bce");

    assertFilterSuccess("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER in_range(d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')], 'abc', 'bce', true, false) RETURN d", expected, &ctx);
    assertFilterSuccess("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER in_range(d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')], CONCAT(_FORWARD_('a'), _FORWARD_('bc')), CONCAT(_FORWARD_('bc'), _FORWARD_('e')), _FORWARD_(5) > _FORWARD_(4), _FORWARD_(5) > _FORWARD_(6)) RETURN d", expected, &ctx);
  }

  // invalid dynamic attribute name (null value)
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintNull{})); // invalid value type
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail("LET a=null LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN collection FILTER in_range(d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')], 'abc', 'bce', true, false) RETURN d", &ctx);
  }

  // boolean expression in range, boost
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("numVal", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt(2)));

    irs::Or expected;
    auto& range = expected.add<irs::by_range>();
    range.boost(1.5);
    range.field(mangleBool("a.b.c.e.f"))
       .include<irs::Bound::MIN>(true).term<irs::Bound::MIN>(irs::boolean_token_stream::value_true())
       .include<irs::Bound::MAX>(true).term<irs::Bound::MAX>(irs::boolean_token_stream::value_true());

    assertFilterSuccess(
      "LET numVal=2 FOR d IN collection FILTER boost(in_rangE(d.a.b.c.e.f, (numVal < 13), (numVal > 1), true, true), 1.5) RETURN d",
      expected,
      &ctx // expression context
    );

    assertFilterSuccess(
      "LET numVal=2 FOR d IN collection FILTER boost(in_rangE(d.a.b.c.e.f, (numVal < 13), (numVal > 1), true, true), 1.5) RETURN d",
      expected,
      &ctx // expression context
    );
  }

  // null expression in range, boost
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("nullVal", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintNull{}));

    irs::Or expected;
    auto& range = expected.add<irs::by_range>();
    range.boost(1.5);
    range.field(mangleNull("a.b.c.e.f"))
         .include<irs::Bound::MIN>(true).term<irs::Bound::MIN>(irs::null_token_stream::value_null())
         .include<irs::Bound::MAX>(true).term<irs::Bound::MAX>(irs::null_token_stream::value_null());

    assertFilterSuccess(
      "LET nullVal=null FOR d IN collection FILTER BOOST(in_range(d.a.b.c.e.f, (nullVal && true), (nullVal && false), true, true), 1.5) RETURN d",
      expected,
      &ctx // expression context
    );

    assertFilterSuccess(
      "LET nullVal=null FOR d IN collection FILTER bOoST(in_range(d.a.b.c.e.f, (nullVal && false), (nullVal && true), true, true), 1.5) RETURN d",
      expected,
      &ctx // expression context
    );
  }

  // numeric expression in range, boost
  {
    irs::numeric_token_stream minTerm; minTerm.reset(15.5);
    irs::numeric_token_stream maxTerm; maxTerm.reset(40.);

    ExpressionContextMock ctx;
    ctx.vars.emplace("numVal", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt(2)));

    irs::Or expected;
    auto& range = expected.add<irs::by_granular_range>();
    range.boost(1.5);
    range.field(mangleNumeric("a.b.c.e.f"))
         .include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm)
         .include<irs::Bound::MAX>(false).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess(
      "LET numVal=2 FOR d IN collection FILTER boost(in_range(d.a['b'].c.e.f, (numVal + 13.5), (numVal + 38), true, false), 1.5) RETURN d",
      expected,
      &ctx // expression context
    );

    assertFilterSuccess(
      "LET numVal=2 FOR d IN collection FILTER boost(IN_RANGE(d.a.b.c.e.f, (numVal + 13.5), (numVal + 38), true, false), 1.5) RETURN d",
      expected,
      &ctx // expression context
    );

    assertFilterSuccess(
      "LET numVal=2 FOR d IN collection FILTER analyzer(boost(in_range(d.a.b.c.e.f, (numVal + 13.5), (numVal + 38), true, false), 1.5), 'test_analyzer') RETURN d",
      expected,
      &ctx // expression context
    );
  }

  // invalid attribute access
  assertFilterFail("FOR d IN myView FILTER in_range(['d'], 'abc', true, 'z', false) RETURN d");
  assertFilterFail("FOR d IN myView FILTER in_range([d], 'abc', true, 'z', false) RETURN d");
  assertFilterFail("FOR d IN myView FILTER in_range(d, 'abc', true, 'z', false) RETURN d");
  assertFilterFail("FOR d IN myView FILTER in_range(d[*], 'abc', true, 'z', false) RETURN d");
  assertFilterFail("FOR d IN myView FILTER in_range(d.a[*].c, 'abc', true, 'z', false) RETURN d");
  assertFilterFail("FOR d IN myView FILTER in_range('d.name', 'abc', true, 'z', false) RETURN d");
  assertFilterFail("FOR d IN myView FILTER in_range(123, 'abc', true, 'z', false) RETURN d");
  assertFilterFail("FOR d IN myView FILTER in_range(123.5, 'abc', true, 'z', false) RETURN d");
  assertFilterFail("FOR d IN myView FILTER in_range(null, 'abc', true, 'z', false) RETURN d");
  assertFilterFail("FOR d IN myView FILTER in_range(true, 'abc', true, 'z', false) RETURN d");
  assertFilterFail("FOR d IN myView FILTER in_range(false, 'abc', true, 'z', false) RETURN d");

  // invalid type of inclusion argument
  assertFilterFail("FOR d IN myView FILTER in_range(d.name, 'abc', true, 'z', 'false') RETURN d");
  assertFilterFail("FOR d IN myView FILTER in_range(d.name, 'abc', true, 'z', 0) RETURN d");
  assertFilterFail("FOR d IN myView FILTER in_range(d.name, 'abc', true, 'z', null) RETURN d");
  assertFilterFail("FOR d IN myView FILTER in_range(d.name, 'abc', 'true', 'z', false) RETURN d");
  assertFilterFail("FOR d IN myView FILTER in_range(d.name, 'abc', 1, 'z', false) RETURN d");
  assertFilterFail("FOR d IN myView FILTER in_range(d.name, 'abc', null, 'z', false) RETURN d");

  // non-deterministic argument
  assertFilterFail("FOR d IN myView FILTER in_range(d[RAND() ? 'name' : 'x'], 'abc', true, 'z', false) RETURN d");
  assertFilterFail("FOR d IN myView FILTER in_range(d.name, RAND() ? 'abc' : 'def', true, 'z', false) RETURN d");
  assertFilterFail("FOR d IN myView FILTER in_range(d.name, 'abc', RAND() ? true : false, 'z', false) RETURN d");
  assertFilterFail("FOR d IN myView FILTER in_range(d.name, 'abc', true, RAND() ? 'z' : 'x', false) RETURN d");
  assertFilterFail("FOR d IN myView FILTER in_range(d.name, 'abc', true, 'z', RAND() ? false : true) RETURN d");

  // lower/upper boundary type mismatch
  assertFilterFail("FOR d IN myView FILTER in_range(d.name, 1, true, 'z', false) RETURN d");
  assertFilterFail("FOR d IN myView FILTER in_range(d.name, 'abc', true, null, false) RETURN d");
  assertFilterFail("FOR d IN myView FILTER in_range(d.name, bool, true, null, false) RETURN d");
  assertFilterFail("FOR d IN myView FILTER in_range(d.name, bool, true, 1, false) RETURN d");

  // wrong number of arguments
  assertFilterParseFail("FOR d IN myView FILTER in_range(d.name, 'abc', true, 'z') RETURN d");
  assertFilterParseFail("FOR d IN myView FILTER in_range(d.name, 'abc', true, 'z', false, false) RETURN d");
}

}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
