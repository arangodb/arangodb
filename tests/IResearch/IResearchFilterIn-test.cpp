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

#include "gtest/gtest.h"

#include "../Mocks/StorageEngineMock.h"
#include "ExpressionContextMock.h"
#include "common.h"

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
#include "IResearch/AqlHelper.h"
#include "IResearch/ExpressionFilter.h"
#include "IResearch/IResearchAnalyzerFeature.h"
#include "IResearch/IResearchCommon.h"
#include "IResearch/IResearchFeature.h"
#include "IResearch/IResearchFilterFactory.h"
#include "IResearch/IResearchLinkMeta.h"
#include "IResearch/IResearchViewMeta.h"
#include "Logger/LogTopic.h"
#include "Logger/Logger.h"
#include "RestServer/AqlFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "RestServer/SystemDatabaseFeature.h"
#include "RestServer/TraverserEngineRegistryFeature.h"
#include "RestServer/ViewTypesFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "Transaction/StandaloneContext.h"
#include "V8Server/V8DealerFeature.h"

#include "analysis/analyzers.hpp"
#include "analysis/token_attributes.hpp"
#include "analysis/token_streams.hpp"
#include "search/all_filter.hpp"
#include "search/boolean_filter.hpp"
#include "search/column_existence_filter.hpp"
#include "search/granular_range_filter.hpp"
#include "search/phrase_filter.hpp"
#include "search/prefix_filter.hpp"
#include "search/range_filter.hpp"
#include "search/term_filter.hpp"

static const VPackBuilder systemDatabaseBuilder = dbArgsBuilder();
static const VPackSlice   systemDatabaseArgs = systemDatabaseBuilder.slice();
static const VPackBuilder testDatabaseBuilder = dbArgsBuilder("testVocbase");
static const VPackSlice   testDatabaseArgs = testDatabaseBuilder.slice();
// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

class IResearchFilterInTest : public ::testing::Test {
 protected:
  StorageEngineMock engine;
  arangodb::application_features::ApplicationServer server;
  std::vector<std::pair<arangodb::application_features::ApplicationFeature*, bool>> features;

  IResearchFilterInTest() : engine(server), server(nullptr, nullptr) {
    arangodb::EngineSelectorFeature::ENGINE = &engine;
    arangodb::aql::AqlFunctionFeature* functions = nullptr;

    arangodb::tests::init();

    // suppress INFO {authentication} Authentication is turned on (system only), authentication for unix sockets is turned on
    // suppress WARNING {authentication} --server.jwt-secret is insecure. Use --server.jwt-secret-keyfile instead
    arangodb::LogTopic::setLogLevel(arangodb::Logger::AUTHENTICATION.name(),
                                    arangodb::LogLevel::ERR);

    // suppress log messages since tests check error conditions
    arangodb::LogTopic::setLogLevel(arangodb::iresearch::TOPIC.name(),
                                    arangodb::LogLevel::FATAL);
    irs::logger::output_le(iresearch::logger::IRL_FATAL, stderr);

    // setup required application features
    features.emplace_back(new arangodb::AuthenticationFeature(server), true);
    features.emplace_back(new arangodb::DatabaseFeature(server), false);
    features.emplace_back(new arangodb::QueryRegistryFeature(server), false);  // must be first
    arangodb::application_features::ApplicationServer::server->addFeature(
        features.back().first);  // need QueryRegistryFeature feature to be added now in order to create the system database
    features.emplace_back(new arangodb::SystemDatabaseFeature(server), true);  // required for IResearchAnalyzerFeature
    features.emplace_back(new arangodb::TraverserEngineRegistryFeature(server), false);  // must be before AqlFeature
    features.emplace_back(new arangodb::V8DealerFeature(server),
                          false);  // required for DatabaseFeature::createDatabase(...)
    features.emplace_back(new arangodb::ViewTypesFeature(server), false);  // required for IResearchFeature
    features.emplace_back(new arangodb::AqlFeature(server), true);
    features.emplace_back(functions = new arangodb::aql::AqlFunctionFeature(server),
                          true);  // required for IResearchAnalyzerFeature
    features.emplace_back(new arangodb::iresearch::IResearchAnalyzerFeature(server), true);
    features.emplace_back(new arangodb::iresearch::IResearchFeature(server), true);

#if USE_ENTERPRISE
    features.emplace_back(new arangodb::LdapFeature(server),
                          false);  // required for AuthenticationFeature with USE_ENTERPRISE
#endif

    // required for V8DealerFeature::prepare(), ClusterFeature::prepare() not required
    arangodb::application_features::ApplicationServer::server->addFeature(
        new arangodb::ClusterFeature(server));

    for (auto& f : features) {
      arangodb::application_features::ApplicationServer::server->addFeature(f.first);
    }

    for (auto& f : features) {
      f.first->prepare();
    }

    auto databases = VPackBuilder();
    databases.openArray();
    databases.add(systemDatabaseArgs);
    databases.close();

    auto* dbFeature =
        arangodb::application_features::ApplicationServer::lookupFeature<arangodb::DatabaseFeature>(
            "Database");
    dbFeature->loadDatabases(databases.slice());

    for (auto& f : features) {
      if (f.second) {
        f.first->start();
      }
    }

    // register fake non-deterministic function in order to suppress optimizations
    functions->add(arangodb::aql::Function{
        "_NONDETERM_", ".",
        arangodb::aql::Function::makeFlags(
            // fake non-deterministic
            arangodb::aql::Function::Flags::CanRunOnDBServer),
        [](arangodb::aql::ExpressionContext*, arangodb::transaction::Methods*,
           arangodb::aql::VPackFunctionParameters const& params) {
          TRI_ASSERT(!params.empty());
          return params[0];
        }});

    // register fake non-deterministic function in order to suppress optimizations
    functions->add(arangodb::aql::Function{
        "_FORWARD_", ".",
        arangodb::aql::Function::makeFlags(
            // fake deterministic
            arangodb::aql::Function::Flags::Deterministic, arangodb::aql::Function::Flags::Cacheable,
            arangodb::aql::Function::Flags::CanRunOnDBServer),
        [](arangodb::aql::ExpressionContext*, arangodb::transaction::Methods*,
           arangodb::aql::VPackFunctionParameters const& params) {
          TRI_ASSERT(!params.empty());
          return params[0];
        }});

    auto* analyzers =
        arangodb::application_features::ApplicationServer::lookupFeature<arangodb::iresearch::IResearchAnalyzerFeature>();
    arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;
    TRI_vocbase_t* vocbase;

    dbFeature->createDatabase(1, "testVocbase", arangodb::velocypack::Slice::emptyObjectSlice(), vocbase);  // required for IResearchAnalyzerFeature::emplace(...)
    analyzers->emplace(
      result,
      "testVocbase::test_analyzer",
      "TestAnalyzer",
      arangodb::velocypack::Parser::fromJson("{ \"args\": \"abc\" }")->slice());
  }

  ~IResearchFilterInTest() {
    arangodb::AqlFeature(server).stop();  // unset singleton instance
    arangodb::LogTopic::setLogLevel(arangodb::iresearch::TOPIC.name(),
                                    arangodb::LogLevel::DEFAULT);
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

    arangodb::LogTopic::setLogLevel(arangodb::Logger::AUTHENTICATION.name(),
                                    arangodb::LogLevel::DEFAULT);
    arangodb::EngineSelectorFeature::ENGINE = nullptr;
  }
};  // IResearchFilterSetup

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

TEST_F(IResearchFilterInTest, BinaryIn) {
  // simple attribute
  {
    irs::Or expected;
    auto& root = expected.add<irs::Or>();
    root.add<irs::by_term>().field(mangleStringIdentity("a")).term("1");
    root.add<irs::by_term>().field(mangleStringIdentity("a")).term("2");
    root.add<irs::by_term>().field(mangleStringIdentity("a")).term("3");

    assertFilterSuccess(
        "FOR d IN collection FILTER d.a in ['1','2','3'] RETURN d", expected);
    assertFilterSuccess(
        "FOR d IN collection FILTER d['a'] in ['1','2','3'] RETURN d", expected);
  }

  // simple offset
  {
    irs::Or expected;
    auto& root = expected.add<irs::Or>();
    root.add<irs::by_term>().field(mangleStringIdentity("[1]")).term("1");
    root.add<irs::by_term>().field(mangleStringIdentity("[1]")).term("2");
    root.add<irs::by_term>().field(mangleStringIdentity("[1]")).term("3");

    assertFilterSuccess(
        "FOR d IN collection FILTER d[1] in ['1','2','3'] RETURN d", expected);
    assertFilterSuccess(
        "FOR d IN collection FILTER ANALYZER(d[1] in ['1','2','3'], "
        "'identity') RETURN d",
        expected);
  }

  // simple offset
  {
    irs::Or expected;
    auto& root = expected.add<irs::Or>();
    root.add<irs::by_term>().field(mangleStringIdentity("a[1]")).term("1");
    root.add<irs::by_term>().field(mangleStringIdentity("a[1]")).term("2");
    root.add<irs::by_term>().field(mangleStringIdentity("a[1]")).term("3");

    assertFilterSuccess(
        "FOR d IN collection FILTER d.a[1] in ['1','2','3'] RETURN d", expected);
    assertFilterSuccess(
        "FOR d IN collection FILTER d['a'][1] in ['1','2','3'] RETURN d", expected);
  }

  // complex attribute name
  {
    irs::Or expected;
    auto& root = expected.add<irs::Or>();
    root.add<irs::by_term>().field(mangleStringIdentity("a.b.c.e.f")).term("1");
    root.add<irs::by_term>().field(mangleStringIdentity("a.b.c.e.f")).term("2");
    root.add<irs::by_term>().field(mangleStringIdentity("a.b.c.e.f")).term("3");

    assertFilterSuccess(
        "FOR d IN collection FILTER d.a['b']['c'].e.f in ['1','2','3'] RETURN "
        "d",
        expected);
    assertFilterSuccess(
        "FOR d IN collection FILTER d.a.b.c.e.f in ['1','2','3'] RETURN d", expected);
  }

  // complex attribute name with offset
  {
    irs::Or expected;
    auto& root = expected.add<irs::Or>();
    root.add<irs::by_term>()
        .field(mangleStringIdentity("a.b.c[412].e.f"))
        .term("1");
    root.add<irs::by_term>()
        .field(mangleStringIdentity("a.b.c[412].e.f"))
        .term("2");
    root.add<irs::by_term>()
        .field(mangleStringIdentity("a.b.c[412].e.f"))
        .term("3");

    assertFilterSuccess(
        "FOR d IN collection FILTER d.a['b']['c'][412].e.f in ['1','2','3'] "
        "RETURN d",
        expected);
    assertFilterSuccess(
        "FOR d IN collection FILTER d.a.b.c[412].e.f in ['1','2','3'] RETURN d", expected);
  }

  // complex attribute name with offset, analyzer
  {
    irs::Or expected;
    auto& root = expected.add<irs::Or>();
    root.add<irs::by_term>()
        .field(mangleString("a.b.c[412].e.f", "test_analyzer"))
        .term("1");
    root.add<irs::by_term>()
        .field(mangleString("a.b.c[412].e.f", "test_analyzer"))
        .term("2");
    root.add<irs::by_term>()
        .field(mangleString("a.b.c[412].e.f", "test_analyzer"))
        .term("3");

    assertFilterSuccess(
        "FOR d IN collection FILTER ANALYZER(d.a['b']['c'][412].e.f in "
        "['1','2','3'], 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        "FOR d IN collection FILTER ANALYZER(d.a.b.c[412].e.f in "
        "['1','2','3'], 'test_analyzer') RETURN d",
        expected);
  }

  // complex attribute name with offset, boost
  {
    irs::Or expected;
    auto& root = expected.add<irs::Or>();
    root.boost(2.5);
    root.add<irs::by_term>()
        .field(mangleStringIdentity("a.b.c[412].e.f"))
        .term("1");
    root.add<irs::by_term>()
        .field(mangleStringIdentity("a.b.c[412].e.f"))
        .term("2");
    root.add<irs::by_term>()
        .field(mangleStringIdentity("a.b.c[412].e.f"))
        .term("3");

    assertFilterSuccess(
        "FOR d IN collection FILTER BOOST(d.a['b']['c'][412].e.f in "
        "['1','2','3'], 2.5) RETURN d",
        expected);
    assertFilterSuccess(
        "FOR d IN collection FILTER BOOST(d.a.b.c[412].e.f in ['1','2','3'], "
        "2.5) RETURN d",
        expected);
  }

  // complex attribute name with offset, boost, analyzer
  {
    irs::Or expected;
    auto& root = expected.add<irs::Or>();
    root.boost(2.5);
    root.add<irs::by_term>()
        .field(mangleString("a.b.c[412].e.f", "test_analyzer"))
        .term("1");
    root.add<irs::by_term>()
        .field(mangleString("a.b.c[412].e.f", "test_analyzer"))
        .term("2");
    root.add<irs::by_term>()
        .field(mangleString("a.b.c[412].e.f", "test_analyzer"))
        .term("3");

    assertFilterSuccess(
        "FOR d IN collection FILTER ANALYZER(BOOST(d.a['b']['c'][412].e.f in "
        "['1','2','3'], 2.5), 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        "FOR d IN collection FILTER BOOST(ANALYZER(d.a.b.c[412].e.f in "
        "['1','2','3'], 'test_analyzer'), 2.5) RETURN d",
        expected);
  }

  // heterogeneous array values
  {
    irs::Or expected;
    auto& root = expected.add<irs::Or>();
    root.add<irs::by_term>()
        .field(mangleStringIdentity("quick.brown.fox"))
        .term("1");
    root.add<irs::by_term>().field(mangleNull("quick.brown.fox")).term(irs::null_token_stream::value_null());
    root.add<irs::by_term>().field(mangleBool("quick.brown.fox")).term(irs::boolean_token_stream::value_true());
    root.add<irs::by_term>().field(mangleBool("quick.brown.fox")).term(irs::boolean_token_stream::value_false());
    {
      irs::numeric_token_stream stream;
      auto& term = stream.attributes().get<irs::term_attribute>();
      stream.reset(2.);
      EXPECT_TRUE(stream.next());
      root.add<irs::by_term>().field(mangleNumeric("quick.brown.fox")).term(term->value());
    }

    assertFilterSuccess(
        "FOR d IN collection FILTER d.quick.brown.fox in "
        "['1',null,true,false,2] RETURN d",
        expected);
    assertFilterSuccess(
        "FOR d IN collection FILTER d.quick['brown'].fox in "
        "['1',null,true,false,2] RETURN d",
        expected);
  }

  // heterogeneous array values, analyzer
  {
    irs::Or expected;
    auto& root = expected.add<irs::Or>();
    root.add<irs::by_term>()
        .field(mangleString("quick.brown.fox", "test_analyzer"))
        .term("1");
    root.add<irs::by_term>().field(mangleNull("quick.brown.fox")).term(irs::null_token_stream::value_null());
    root.add<irs::by_term>().field(mangleBool("quick.brown.fox")).term(irs::boolean_token_stream::value_true());
    root.add<irs::by_term>().field(mangleBool("quick.brown.fox")).term(irs::boolean_token_stream::value_false());
    {
      irs::numeric_token_stream stream;
      auto& term = stream.attributes().get<irs::term_attribute>();
      stream.reset(2.);
      EXPECT_TRUE(stream.next());
      root.add<irs::by_term>().field(mangleNumeric("quick.brown.fox")).term(term->value());
    }

    assertFilterSuccess(
        "FOR d IN collection FILTER ANALYZER(d.quick.brown.fox in "
        "['1',null,true,false,2], 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        "FOR d IN collection FILTER ANALYZER(d.quick['brown'].fox in "
        "['1',null,true,false,2], 'test_analyzer') RETURN d",
        expected);
  }

  // heterogeneous array values, boost
  {
    irs::Or expected;
    auto& root = expected.add<irs::Or>();
    root.boost(1.5);
    root.add<irs::by_term>()
        .field(mangleStringIdentity("quick.brown.fox"))
        .term("1");
    root.add<irs::by_term>().field(mangleNull("quick.brown.fox")).term(irs::null_token_stream::value_null());
    root.add<irs::by_term>().field(mangleBool("quick.brown.fox")).term(irs::boolean_token_stream::value_true());
    root.add<irs::by_term>().field(mangleBool("quick.brown.fox")).term(irs::boolean_token_stream::value_false());
    {
      irs::numeric_token_stream stream;
      auto& term = stream.attributes().get<irs::term_attribute>();
      stream.reset(2.);
      EXPECT_TRUE(stream.next());
      root.add<irs::by_term>().field(mangleNumeric("quick.brown.fox")).term(term->value());
    }

    assertFilterSuccess(
        "FOR d IN collection FILTER booST(d.quick.brown.fox in "
        "['1',null,true,false,2], 1.5) RETURN d",
        expected);
    assertFilterSuccess(
        "FOR d IN collection FILTER Boost(d.quick['brown'].fox in "
        "['1',null,true,false,2], 1.5) RETURN d",
        expected);
  }

  // heterogeneous array values, analyzer, boost
  {
    irs::Or expected;
    auto& root = expected.add<irs::Or>();
    root.boost(1.5);
    root.add<irs::by_term>()
        .field(mangleString("quick.brown.fox", "test_analyzer"))
        .term("1");
    root.add<irs::by_term>().field(mangleNull("quick.brown.fox")).term(irs::null_token_stream::value_null());
    root.add<irs::by_term>().field(mangleBool("quick.brown.fox")).term(irs::boolean_token_stream::value_true());
    root.add<irs::by_term>().field(mangleBool("quick.brown.fox")).term(irs::boolean_token_stream::value_false());
    {
      irs::numeric_token_stream stream;
      auto& term = stream.attributes().get<irs::term_attribute>();
      stream.reset(2.);
      EXPECT_TRUE(stream.next());
      root.add<irs::by_term>().field(mangleNumeric("quick.brown.fox")).term(term->value());
    }

    assertFilterSuccess(
        "FOR d IN collection FILTER ANALYZER(BOOST(d.quick.brown.fox in "
        "['1',null,true,false,2], 1.5), 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        "FOR d IN collection FILTER BOOST(ANALYZER(d.quick['brown'].fox in "
        "['1',null,true,false,2], 'test_analyzer'), 1.5) RETURN d",
        expected);
  }

  // empty array
  {
    irs::Or expected;
    expected.add<irs::empty>();

    assertFilterSuccess(
        "FOR d IN collection FILTER d.quick.brown.fox in [] RETURN d", expected);
    assertFilterSuccess(
        "FOR d IN collection FILTER d['quick'].brown.fox in [] RETURN d", expected);
  }

  // dynamic complex attribute name
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                                      arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                                      arangodb::aql::AqlValueHintDouble{5.6})));

    irs::Or expected;
    auto& root = expected.add<irs::Or>();
    root.add<irs::by_term>()
        .field(mangleStringIdentity("a.b.c.e[4].f[5].g[3].g.a"))
        .term("1");
    root.add<irs::by_term>()
        .field(mangleStringIdentity("a.b.c.e[4].f[5].g[3].g.a"))
        .term("2");
    root.add<irs::by_term>()
        .field(mangleStringIdentity("a.b.c.e[4].f[5].g[3].g.a"))
        .term("3");

    assertFilterSuccess(
        "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "in ['1','2','3'] RETURN d",
        expected, &ctx);
  }

  // invalid dynamic attribute name
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                                      arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail(
        "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "in ['1','2','3'] RETURN d",
        &ctx);
  }

  // invalid dynamic attribute name (null value)
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintNull{}));  // invalid value type
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                                      arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                                      arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail(
        "LET a=null LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "in ['1','2','3'] RETURN d",
        &ctx);
  }

  // invalid dynamic attribute name (bool value)
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintBool{false}));  // invalid value type
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                                      arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                                      arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail(
        "LET a=false LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "in ['1','2','3'] RETURN d",
        &ctx);
  }

  // reference in array
  {
    arangodb::aql::Variable var("c", 0);
    arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt(2));
    arangodb::aql::AqlValueGuard guard(value, true);

    irs::numeric_token_stream stream;
    stream.reset(2.);
    EXPECT_TRUE(stream.next());
    auto& term = stream.attributes().get<irs::term_attribute>();

    ExpressionContextMock ctx;
    ctx.vars.emplace(var.name, value);

    irs::Or expected;
    auto& root = expected.add<irs::Or>();
    root.add<irs::by_term>().field(mangleStringIdentity("a.b.c.e.f")).term("1");
    root.add<irs::by_term>().field(mangleNumeric("a.b.c.e.f")).term(term->value());
    root.add<irs::by_term>().field(mangleStringIdentity("a.b.c.e.f")).term("3");

    // not a constant in array
    assertFilterSuccess(
        "LET c=2 FOR d IN collection FILTER d.a.b.c.e.f in ['1', c, '3'] "
        "RETURN d",
        expected,
        &ctx  // expression context
    );
  }

  // array as reference
  {
    auto obj = arangodb::velocypack::Parser::fromJson("[ \"1\", 2, \"3\"]");
    arangodb::aql::AqlValue value(obj->slice());
    arangodb::aql::AqlValueGuard guard(value, true);

    irs::numeric_token_stream stream;
    stream.reset(2.);
    EXPECT_TRUE(stream.next());
    auto& term = stream.attributes().get<irs::term_attribute>();

    ExpressionContextMock ctx;
    ctx.vars.emplace("x", value);

    irs::Or expected;
    auto& root = expected.add<irs::Or>();
    root.add<irs::by_term>().field(mangleStringIdentity("a.b.c.e.f")).term("1");
    root.add<irs::by_term>().field(mangleNumeric("a.b.c.e.f")).term(term->value());
    root.add<irs::by_term>().field(mangleStringIdentity("a.b.c.e.f")).term("3");

    assertFilterSuccess(
        "LET x=['1', 2, '3'] FOR d IN collection FILTER d.a.b.c.e.f in x "
        "RETURN d",
        expected, &ctx);
  }

  // array as reference, analyzer
  {
    auto obj = arangodb::velocypack::Parser::fromJson("[ \"1\", 2, \"3\"]");
    arangodb::aql::AqlValue value(obj->slice());
    arangodb::aql::AqlValueGuard guard(value, true);

    irs::numeric_token_stream stream;
    stream.reset(2.);
    EXPECT_TRUE(stream.next());
    auto& term = stream.attributes().get<irs::term_attribute>();

    ExpressionContextMock ctx;
    ctx.vars.emplace("x", value);

    irs::Or expected;
    auto& root = expected.add<irs::Or>();
    root.add<irs::by_term>()
        .field(mangleString("a.b.c.e.f", "test_analyzer"))
        .term("1");
    root.add<irs::by_term>().field(mangleNumeric("a.b.c.e.f")).term(term->value());
    root.add<irs::by_term>()
        .field(mangleString("a.b.c.e.f", "test_analyzer"))
        .term("3");

    assertFilterSuccess(
        "LET x=['1', 2, '3'] FOR d IN collection FILTER ANALYZER(d.a.b.c.e.f "
        "in x, 'test_analyzer') RETURN d",
        expected, &ctx);
  }

  // array as reference, boost
  {
    auto obj = arangodb::velocypack::Parser::fromJson("[ \"1\", 2, \"3\"]");
    arangodb::aql::AqlValue value(obj->slice());
    arangodb::aql::AqlValueGuard guard(value, true);

    irs::numeric_token_stream stream;
    stream.reset(2.);
    EXPECT_TRUE(stream.next());
    auto& term = stream.attributes().get<irs::term_attribute>();

    ExpressionContextMock ctx;
    ctx.vars.emplace("x", value);

    irs::Or expected;
    auto& root = expected.add<irs::Or>();
    root.boost(1.5);
    root.add<irs::by_term>().field(mangleStringIdentity("a.b.c.e.f")).term("1");
    root.add<irs::by_term>().field(mangleNumeric("a.b.c.e.f")).term(term->value());
    root.add<irs::by_term>().field(mangleStringIdentity("a.b.c.e.f")).term("3");

    assertFilterSuccess(
        "LET x=['1', 2, '3'] FOR d IN collection FILTER BOOST(d.a.b.c.e.f in "
        "x, 1.5) RETURN d",
        expected, &ctx);
  }

  // array as reference, boost, analyzer
  {
    auto obj = arangodb::velocypack::Parser::fromJson("[ \"1\", 2, \"3\"]");
    arangodb::aql::AqlValue value(obj->slice());
    arangodb::aql::AqlValueGuard guard(value, true);

    irs::numeric_token_stream stream;
    stream.reset(2.);
    EXPECT_TRUE(stream.next());
    auto& term = stream.attributes().get<irs::term_attribute>();

    ExpressionContextMock ctx;
    ctx.vars.emplace("x", value);

    irs::Or expected;
    auto& root = expected.add<irs::Or>();
    root.boost(1.5);
    root.add<irs::by_term>()
        .field(mangleString("a.b.c.e.f", "test_analyzer"))
        .term("1");
    root.add<irs::by_term>().field(mangleNumeric("a.b.c.e.f")).term(term->value());
    root.add<irs::by_term>()
        .field(mangleString("a.b.c.e.f", "test_analyzer"))
        .term("3");

    assertFilterSuccess(
        "LET x=['1', 2, '3'] FOR d IN collection FILTER "
        "ANALYZER(BOOST(d.a.b.c.e.f in x, 1.5), 'test_analyzer') RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        "LET x=['1', 2, '3'] FOR d IN collection FILTER "
        "BOOST(ANALYZER(d.a.b.c.e.f in x, 'test_analyzer'), 1.5) RETURN d",
        expected, &ctx);
  }

  // nondeterministic value
  {
    std::string const queryString =
        "FOR d IN collection FILTER d.a.b.c.e.f in [ '1', RAND(), '3' ] RETURN "
        "d";
    std::string const refName = "d";

    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                          testDatabaseArgs);

    auto options = std::make_shared<arangodb::velocypack::Builder>();

    arangodb::aql::Query query(false, vocbase, arangodb::aql::QueryString(queryString),
                               nullptr, options, arangodb::aql::PART_MAIN);

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
      arangodb::iresearch::QueryContext const ctx{nullptr, nullptr, nullptr, nullptr, ref};
      EXPECT_TRUE(
          (arangodb::iresearch::FilterFactory::filter(nullptr, ctx, *filterNode).ok()));
    }

    // iteratorForCondition
    {
      arangodb::transaction::Methods trx(arangodb::transaction::StandaloneContext::Create(vocbase),
                                         {}, {}, {}, arangodb::transaction::Options());

      auto dummyPlan = arangodb::tests::planFromQuery(vocbase, "RETURN 1");

      irs::Or actual;
      arangodb::iresearch::QueryContext const ctx{&trx, dummyPlan.get(), ast,
                                                  &ExpressionContextMock::EMPTY, ref};
      EXPECT_TRUE(
          (arangodb::iresearch::FilterFactory::filter(&actual, ctx, *filterNode).ok()));

      {
        EXPECT_TRUE(1 == actual.size());
        auto& root = dynamic_cast<irs::Or&>(*actual.begin());
        EXPECT_TRUE(irs::Or::type() == root.type());
        EXPECT_TRUE(3 == root.size());
        auto begin = root.begin();

        // 1st filter
        {
          irs::by_term expected;
          expected.field(mangleStringIdentity("a.b.c.e.f")).term("1");
          EXPECT_TRUE(expected == *begin);
        }

        // 2nd filter
        {
          ++begin;
          EXPECT_TRUE(arangodb::iresearch::ByExpression::type() == begin->type());
          EXPECT_TRUE(nullptr !=
                      dynamic_cast<arangodb::iresearch::ByExpression const*>(&*begin));
        }

        // 3rd filter
        {
          ++begin;
          irs::by_term expected;
          expected.field(mangleStringIdentity("a.b.c.e.f")).term("3");
          EXPECT_TRUE(expected == *begin);
        }

        EXPECT_TRUE(root.end() == ++begin);
      }
    }
  }

  // self-referenced value
  {
    std::string const queryString =
        "FOR d IN collection FILTER d.a.b.c.e.f in [ '1', d, '3' ] RETURN d";
    std::string const refName = "d";

    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                          testDatabaseArgs);

    auto options = std::make_shared<arangodb::velocypack::Builder>();

    arangodb::aql::Query query(false, vocbase, arangodb::aql::QueryString(queryString),
                               nullptr, options, arangodb::aql::PART_MAIN);

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
      arangodb::iresearch::QueryContext const ctx{nullptr, nullptr, nullptr, nullptr, ref};
      EXPECT_TRUE(
          (arangodb::iresearch::FilterFactory::filter(nullptr, ctx, *filterNode).ok()));
    }

    // iteratorForCondition
    {
      arangodb::transaction::Methods trx(arangodb::transaction::StandaloneContext::Create(vocbase),
                                         {}, {}, {}, arangodb::transaction::Options());

      auto dummyPlan = arangodb::tests::planFromQuery(vocbase, "RETURN 1");

      irs::Or actual;
      arangodb::iresearch::QueryContext const ctx{&trx, dummyPlan.get(), ast,
                                                  &ExpressionContextMock::EMPTY, ref};
      EXPECT_TRUE(
          (arangodb::iresearch::FilterFactory::filter(&actual, ctx, *filterNode).ok()));

      {
        EXPECT_TRUE(1 == actual.size());
        auto& root = dynamic_cast<irs::Or&>(*actual.begin());
        EXPECT_TRUE(irs::Or::type() == root.type());
        EXPECT_TRUE(3 == root.size());
        auto begin = root.begin();

        // 1st filter
        {
          irs::by_term expected;
          expected.field(mangleStringIdentity("a.b.c.e.f")).term("1");
          EXPECT_TRUE(expected == *begin);
        }

        // 2nd filter
        {
          ++begin;
          EXPECT_TRUE(arangodb::iresearch::ByExpression::type() == begin->type());
          EXPECT_TRUE(nullptr !=
                      dynamic_cast<arangodb::iresearch::ByExpression const*>(&*begin));
        }

        // 3rd filter
        {
          ++begin;
          irs::by_term expected;
          expected.field(mangleStringIdentity("a.b.c.e.f")).term("3");
          EXPECT_TRUE(expected == *begin);
        }

        EXPECT_TRUE(root.end() == ++begin);
      }
    }
  }

  // self-referenced value
  {
    std::string const queryString =
        "FOR d IN collection FILTER d.a.b.c.e.f in [ '1', d.e, d.a.b.c.e.f ] "
        "RETURN d";
    std::string const refName = "d";

    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                          testDatabaseArgs);

    auto options = std::make_shared<arangodb::velocypack::Builder>();

    arangodb::aql::Query query(false, vocbase, arangodb::aql::QueryString(queryString),
                               nullptr, options, arangodb::aql::PART_MAIN);

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
      arangodb::iresearch::QueryContext const ctx{nullptr, nullptr, nullptr, nullptr, ref};
      EXPECT_TRUE(
          (arangodb::iresearch::FilterFactory::filter(nullptr, ctx, *filterNode).ok()));
    }

    // iteratorForCondition
    {
      arangodb::transaction::Methods trx(arangodb::transaction::StandaloneContext::Create(vocbase),
                                         {}, {}, {}, arangodb::transaction::Options());

      auto dummyPlan = arangodb::tests::planFromQuery(vocbase, "RETURN 1");

      irs::Or actual;
      arangodb::iresearch::QueryContext const ctx{&trx, dummyPlan.get(), ast,
                                                  &ExpressionContextMock::EMPTY, ref};
      EXPECT_TRUE(
          (arangodb::iresearch::FilterFactory::filter(&actual, ctx, *filterNode).ok()));

      {
        EXPECT_TRUE(1 == actual.size());
        auto& root = dynamic_cast<irs::Or&>(*actual.begin());
        EXPECT_TRUE(irs::Or::type() == root.type());
        EXPECT_TRUE(3 == root.size());
        auto begin = root.begin();

        // 1st filter
        {
          irs::by_term expected;
          expected.field(mangleStringIdentity("a.b.c.e.f")).term("1");
          EXPECT_TRUE(expected == *begin);
        }

        // 2nd filter
        {
          ++begin;
          EXPECT_TRUE(arangodb::iresearch::ByExpression::type() == begin->type());
          EXPECT_TRUE(nullptr !=
                      dynamic_cast<arangodb::iresearch::ByExpression const*>(&*begin));
        }

        // 3rd filter
        {
          ++begin;
          EXPECT_TRUE(arangodb::iresearch::ByExpression::type() == begin->type());
          EXPECT_TRUE(nullptr !=
                      dynamic_cast<arangodb::iresearch::ByExpression const*>(&*begin));
        }

        EXPECT_TRUE(root.end() == ++begin);
      }
    }
  }

  // self-referenced value
  {
    std::string const queryString =
        "FOR d IN collection FILTER d.a.b.c.e.f in [ '1', 1+d.b, '3' ] RETURN "
        "d";
    std::string const refName = "d";

    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                          testDatabaseArgs);

    auto options = std::make_shared<arangodb::velocypack::Builder>();

    arangodb::aql::Query query(false, vocbase, arangodb::aql::QueryString(queryString),
                               nullptr, options, arangodb::aql::PART_MAIN);

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
      arangodb::iresearch::QueryContext const ctx{nullptr, nullptr, nullptr, nullptr, ref};
      EXPECT_TRUE(
          (arangodb::iresearch::FilterFactory::filter(nullptr, ctx, *filterNode).ok()));
    }

    // iteratorForCondition
    {
      arangodb::transaction::Methods trx(arangodb::transaction::StandaloneContext::Create(vocbase),
                                         {}, {}, {}, arangodb::transaction::Options());

      auto dummyPlan = arangodb::tests::planFromQuery(vocbase, "RETURN 1");

      irs::Or actual;
      arangodb::iresearch::QueryContext const ctx{&trx, dummyPlan.get(), ast,
                                                  &ExpressionContextMock::EMPTY, ref};
      EXPECT_TRUE(
          (arangodb::iresearch::FilterFactory::filter(&actual, ctx, *filterNode).ok()));

      {
        EXPECT_TRUE(1 == actual.size());
        auto& root = dynamic_cast<irs::Or&>(*actual.begin());
        EXPECT_TRUE(irs::Or::type() == root.type());
        EXPECT_TRUE(3 == root.size());
        auto begin = root.begin();

        // 1st filter
        {
          irs::by_term expected;
          expected.field(mangleStringIdentity("a.b.c.e.f")).term("1");
          EXPECT_TRUE(expected == *begin);
        }

        // 2nd filter
        {
          ++begin;
          EXPECT_TRUE(arangodb::iresearch::ByExpression::type() == begin->type());
          EXPECT_TRUE(nullptr !=
                      dynamic_cast<arangodb::iresearch::ByExpression const*>(&*begin));
        }

        // 3rd filter
        {
          ++begin;
          irs::by_term expected;
          expected.field(mangleStringIdentity("a.b.c.e.f")).term("3");
          EXPECT_TRUE(expected == *begin);
        }

        EXPECT_TRUE(root.end() == ++begin);
      }
    }
  }

  // nondeterministic attribute access in value
  {
    std::string const queryString =
        "FOR d IN collection FILTER 4 in [ 1, d.a[_NONDETERM_('abc')], 4 ] "
        "RETURN d";
    std::string const refName = "d";

    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                          testDatabaseArgs);

    auto options = std::make_shared<arangodb::velocypack::Builder>();

    arangodb::aql::Query query(false, vocbase, arangodb::aql::QueryString(queryString),
                               nullptr, options, arangodb::aql::PART_MAIN);

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
      arangodb::iresearch::QueryContext const ctx{nullptr, nullptr, nullptr, nullptr, ref};
      EXPECT_TRUE(
          (arangodb::iresearch::FilterFactory::filter(nullptr, ctx, *filterNode).ok()));
    }

    // iteratorForCondition
    {
      arangodb::transaction::Methods trx(arangodb::transaction::StandaloneContext::Create(vocbase),
                                         {}, {}, {}, arangodb::transaction::Options());

      auto dummyPlan = arangodb::tests::planFromQuery(vocbase, "RETURN 1");

      irs::Or actual;
      arangodb::iresearch::QueryContext const ctx{&trx, dummyPlan.get(), ast,
                                                  &ExpressionContextMock::EMPTY, ref};
      EXPECT_TRUE(
          (arangodb::iresearch::FilterFactory::filter(&actual, ctx, *filterNode).ok()));

      {
        EXPECT_TRUE(1 == actual.size());
        auto& root = dynamic_cast<irs::Or&>(*actual.begin());
        EXPECT_TRUE(irs::Or::type() == root.type());
        EXPECT_TRUE(3 == root.size());
        auto begin = root.begin();

        // 1st filter
        { EXPECT_TRUE(irs::empty() == *begin); }

        // 2nd filter
        {
          ++begin;
          EXPECT_TRUE(arangodb::iresearch::ByExpression::type() == begin->type());
          EXPECT_TRUE(nullptr !=
                      dynamic_cast<arangodb::iresearch::ByExpression const*>(&*begin));
        }

        // 3rd filter
        { EXPECT_TRUE(irs::all() == *++begin); }

        EXPECT_TRUE(root.end() == ++begin);
      }
    }
  }

  // self-reference in value
  {
    std::string const queryString =
        "FOR d IN collection FILTER 4 in [ 1, d.b.a, 4 ] RETURN d";
    std::string const refName = "d";

    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                          testDatabaseArgs);

    auto options = std::make_shared<arangodb::velocypack::Builder>();

    arangodb::aql::Query query(false, vocbase, arangodb::aql::QueryString(queryString),
                               nullptr, options, arangodb::aql::PART_MAIN);

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
      arangodb::iresearch::QueryContext const ctx{nullptr, nullptr, nullptr, nullptr, ref};
      EXPECT_TRUE(
          (arangodb::iresearch::FilterFactory::filter(nullptr, ctx, *filterNode).ok()));
    }

    // iteratorForCondition
    {
      arangodb::transaction::Methods trx(arangodb::transaction::StandaloneContext::Create(vocbase),
                                         {}, {}, {}, arangodb::transaction::Options());

      auto dummyPlan = arangodb::tests::planFromQuery(vocbase, "RETURN 1");

      irs::Or actual;
      arangodb::iresearch::QueryContext const ctx{&trx, dummyPlan.get(), ast,
                                                  &ExpressionContextMock::EMPTY, ref};
      EXPECT_TRUE(
          (arangodb::iresearch::FilterFactory::filter(&actual, ctx, *filterNode).ok()));

      {
        EXPECT_TRUE(1 == actual.size());
        auto& root = dynamic_cast<irs::Or&>(*actual.begin());
        EXPECT_TRUE(irs::Or::type() == root.type());
        EXPECT_TRUE(3 == root.size());
        auto begin = root.begin();

        // 1st filter
        { EXPECT_TRUE(irs::empty() == *begin); }

        // 2nd filter
        {
          irs::numeric_token_stream stream;
          stream.reset(4.0);
          auto& term = stream.attributes().get<irs::term_attribute>();
          EXPECT_TRUE(stream.next());

          irs::by_term expected;
          expected.field(mangleNumeric("b.a")).term(term->value());
          EXPECT_TRUE(expected == *++begin);
        }

        // 3rd filter
        { EXPECT_TRUE(irs::all() == *++begin); }

        EXPECT_TRUE(root.end() == ++begin);
      }
    }
  }

  assertExpressionFilter(
      "FOR d IN collection FILTER 4 in [ 1, 1+d.b, 4 ] RETURN d");

  // heterogeneous references and expression in array
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("strVal", arangodb::aql::AqlValue("str"));
    ctx.vars.emplace("boolVal",
                     arangodb::aql::AqlValue(arangodb::aql::AqlValueHintBool(false)));
    ctx.vars.emplace("numVal", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt(2)));
    ctx.vars.emplace("nullVal", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintNull{}));

    irs::numeric_token_stream stream;
    stream.reset(3.);
    EXPECT_TRUE(stream.next());
    auto& term = stream.attributes().get<irs::term_attribute>();

    irs::Or expected;
    auto& root = expected.add<irs::Or>();
    root.add<irs::by_term>().field(mangleStringIdentity("a.b.c.e.f")).term("1");
    root.add<irs::by_term>()
        .field(mangleStringIdentity("a.b.c.e.f"))
        .term("str");
    root.add<irs::by_term>().field(mangleBool("a.b.c.e.f")).term(irs::boolean_token_stream::value_false());
    root.add<irs::by_term>().field(mangleNumeric("a.b.c.e.f")).term(term->value());
    root.add<irs::by_term>().field(mangleNull("a.b.c.e.f")).term(irs::null_token_stream::value_null());

    // not a constant in array
    assertFilterSuccess(
        "LET strVal='str' LET boolVal=false LET numVal=2 LET nullVal=null FOR "
        "d IN collection FILTER d.a.b.c.e.f in ['1', strVal, boolVal, "
        "numVal+1, nullVal] RETURN d",
        expected,
        &ctx  // expression context
    );
  }

  // heterogeneous references and expression in array, boost
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("strVal", arangodb::aql::AqlValue("str"));
    ctx.vars.emplace("boolVal",
                     arangodb::aql::AqlValue(arangodb::aql::AqlValueHintBool(false)));
    ctx.vars.emplace("numVal", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt(2)));
    ctx.vars.emplace("nullVal", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintNull{}));

    irs::numeric_token_stream stream;
    stream.reset(3.);
    EXPECT_TRUE(stream.next());
    auto& term = stream.attributes().get<irs::term_attribute>();

    irs::Or expected;
    auto& root = expected.add<irs::Or>();
    root.boost(1.5);
    root.add<irs::by_term>().field(mangleStringIdentity("a.b.c.e.f")).term("1");
    root.add<irs::by_term>()
        .field(mangleStringIdentity("a.b.c.e.f"))
        .term("str");
    root.add<irs::by_term>().field(mangleBool("a.b.c.e.f")).term(irs::boolean_token_stream::value_false());
    root.add<irs::by_term>().field(mangleNumeric("a.b.c.e.f")).term(term->value());
    root.add<irs::by_term>().field(mangleNull("a.b.c.e.f")).term(irs::null_token_stream::value_null());

    // not a constant in array
    assertFilterSuccess(
        "LET strVal='str' LET boolVal=false LET numVal=2 LET nullVal=null FOR "
        "d IN collection FILTER boost(boost(d.a.b.c.e.f in ['1', strVal, "
        "boolVal, numVal+1, nullVal], 1), 1.5) RETURN d",
        expected,
        &ctx  // expression context
    );
  }

  // heterogeneous references and expression in array, analyzer
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("strVal", arangodb::aql::AqlValue("str"));
    ctx.vars.emplace("boolVal",
                     arangodb::aql::AqlValue(arangodb::aql::AqlValueHintBool(false)));
    ctx.vars.emplace("numVal", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt(2)));
    ctx.vars.emplace("nullVal", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintNull{}));

    irs::numeric_token_stream stream;
    stream.reset(3.);
    EXPECT_TRUE(stream.next());
    auto& term = stream.attributes().get<irs::term_attribute>();

    irs::Or expected;
    auto& root = expected.add<irs::Or>();
    root.add<irs::by_term>()
        .field(mangleString("a.b.c.e.f", "test_analyzer"))
        .term("1");
    root.add<irs::by_term>()
        .field(mangleString("a.b.c.e.f", "test_analyzer"))
        .term("str");
    root.add<irs::by_term>().field(mangleBool("a.b.c.e.f")).term(irs::boolean_token_stream::value_false());
    root.add<irs::by_term>().field(mangleNumeric("a.b.c.e.f")).term(term->value());
    root.add<irs::by_term>().field(mangleNull("a.b.c.e.f")).term(irs::null_token_stream::value_null());

    // not a constant in array
    assertFilterSuccess(
        "LET strVal='str' LET boolVal=false LET numVal=2 LET nullVal=null FOR "
        "d IN collection FILTER ANALYZER(d.a.b.c.e.f in ['1', strVal, boolVal, "
        "numVal+1, nullVal], 'test_analyzer') RETURN d",
        expected,
        &ctx  // expression context
    );
  }

  // heterogeneous references and expression in array, analyzer, boost
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("strVal", arangodb::aql::AqlValue("str"));
    ctx.vars.emplace("boolVal",
                     arangodb::aql::AqlValue(arangodb::aql::AqlValueHintBool(false)));
    ctx.vars.emplace("numVal", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt(2)));
    ctx.vars.emplace("nullVal", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintNull{}));

    irs::numeric_token_stream stream;
    stream.reset(3.);
    EXPECT_TRUE(stream.next());
    auto& term = stream.attributes().get<irs::term_attribute>();

    irs::Or expected;
    auto& root = expected.add<irs::Or>();
    root.boost(2.5);
    root.add<irs::by_term>()
        .field(mangleString("a.b.c.e.f", "test_analyzer"))
        .term("1");
    root.add<irs::by_term>()
        .field(mangleString("a.b.c.e.f", "test_analyzer"))
        .term("str");
    root.add<irs::by_term>().field(mangleBool("a.b.c.e.f")).term(irs::boolean_token_stream::value_false());
    root.add<irs::by_term>().field(mangleNumeric("a.b.c.e.f")).term(term->value());
    root.add<irs::by_term>().field(mangleNull("a.b.c.e.f")).term(irs::null_token_stream::value_null());

    // not a constant in array
    assertFilterSuccess(
        "LET strVal='str' LET boolVal=false LET numVal=2 LET nullVal=null FOR "
        "d IN collection FILTER boost(ANALYZER(d.a.b.c.e.f in ['1', strVal, "
        "boolVal, numVal+1, nullVal], 'test_analyzer'),2.5) RETURN d",
        expected,
        &ctx  // expression context
    );
    assertFilterSuccess(
        "LET strVal='str' LET boolVal=false LET numVal=2 LET nullVal=null FOR "
        "d IN collection FILTER ANALYZER(boost(d.a.b.c.e.f in ['1', strVal, "
        "boolVal, numVal+1, nullVal], 2.5), 'test_analyzer') RETURN d",
        expected,
        &ctx  // expression context
    );
  }

  assertExpressionFilter("FOR d IN myView FILTER [1,2,'3'] in d.a RETURN d");

  // non-deterministic expression name in array
  assertExpressionFilter(
      "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
      "collection FILTER "
      "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_NONDETERM_('a')] "
      "in ['1','2','3'] RETURN d");

  // self-reference
  assertExpressionFilter("FOR d IN myView FILTER d in [1,2,3] RETURN d");
  assertExpressionFilter("FOR d IN myView FILTER d[*] in [1,2,3] RETURN d");
  assertExpressionFilter("FOR d IN myView FILTER d.a[*] in [1,2,3] RETURN d");

  // no reference provided
  assertFilterExecutionFail(
      "LET x={} FOR d IN myView FILTER d.a in [1,x.a,3] RETURN d",
      &ExpressionContextMock::EMPTY);

  // false expression
  {
    irs::Or expected;
    expected.add<irs::empty>();

    assertFilterSuccess("FOR d IN myView FILTER [] in [1,2,3] RETURN d",
                        expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess("FOR d IN myView FILTER ['d'] in [1,2,3] RETURN d",
                        expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess("FOR d IN myView FILTER 'd.a' in [1,2,3] RETURN d",
                        expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess("FOR d IN myView FILTER null in [1,2,3] RETURN d",
                        expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess("FOR d IN myView FILTER true in [1,2,3] RETURN d",
                        expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess("FOR d IN myView FILTER false in [1,2,3] RETURN d",
                        expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess("FOR d IN myView FILTER 4 in [1,2,3] RETURN d",
                        expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess("FOR d IN myView FILTER 4.5 in [1,2,3] RETURN d",
                        expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess("FOR d IN myView FILTER 1..2 in [1,2,3] RETURN d", expected,
                        &ExpressionContextMock::EMPTY);  // by some reason arangodb evaluates it to false
  }

  // true expression
  {
    irs::Or expected;
    expected.add<irs::all>();

    assertFilterSuccess("FOR d IN myView FILTER 4 in [1,2,3,4] RETURN d",
                        expected, &ExpressionContextMock::EMPTY);
  }

  // not a value in array
  assertFilterFail(
      "FOR d IN collection FILTER d.a in ['1',['2'],'3'] RETURN d");
  assertFilterFail(
      "FOR d IN collection FILTER d.a in ['1', {\"abc\": \"def\"},'3'] RETURN "
      "d");

  // numeric range
  {
    irs::numeric_token_stream minTerm;
    minTerm.reset(4.0);
    irs::numeric_token_stream maxTerm;
    maxTerm.reset(5.0);

    irs::Or expected;
    auto& range = expected.add<irs::by_granular_range>();
    range.field(mangleNumeric("a.b.c.e.f"));
    range.include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm);
    range.include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess(
        "FOR d IN collection FILTER d.a.b.c.e.f in 4..5 RETURN d", expected,
        &ExpressionContextMock::EMPTY);
    assertFilterSuccess(
        "FOR d IN collection FILTER d['a'].b['c'].e.f in 4..5 RETURN d",
        expected, &ExpressionContextMock::EMPTY);
  }

  // numeric range, boost
  {
    irs::numeric_token_stream minTerm;
    minTerm.reset(4.0);
    irs::numeric_token_stream maxTerm;
    maxTerm.reset(5.0);

    irs::Or expected;
    auto& range = expected.add<irs::by_granular_range>();
    range.boost(2.5);
    range.field(mangleNumeric("a.b.c.e.f"));
    range.include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm);
    range.include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess(
        "FOR d IN collection FILTER boost(d.a.b.c.e.f in 4..5, 2.5) RETURN d",
        expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess(
        "FOR d IN collection FILTER boost(d['a'].b['c'].e.f in 4..5, 2.5) "
        "RETURN d",
        expected, &ExpressionContextMock::EMPTY);
  }

  // numeric range, boost
  {
    irs::numeric_token_stream minTerm;
    minTerm.reset(4.0);
    irs::numeric_token_stream maxTerm;
    maxTerm.reset(5.0);

    irs::Or expected;
    auto& range = expected.add<irs::by_granular_range>();
    range.boost(2.5);
    range.field(mangleNumeric("a.b.c.e.f"));
    range.include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm);
    range.include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess(
        "FOR d IN collection FILTER analyZER(boost(d.a.b.c.e.f in 4..5, 2.5), "
        "'test_analyzer') RETURN d",
        expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess(
        "FOR d IN collection FILTER analyZER(boost(d['a'].b['c'].e.f in 4..5, "
        "2.5), 'test_analyzer') RETURN d",
        expected, &ExpressionContextMock::EMPTY);
  }

  // numeric floating range
  {
    irs::numeric_token_stream minTerm;
    minTerm.reset(4.0);
    irs::numeric_token_stream maxTerm;
    maxTerm.reset(5.0);

    irs::Or expected;
    auto& range = expected.add<irs::by_granular_range>();
    range.field(mangleNumeric("a.b.c.e.f"));
    range.include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm);
    range.include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess(
        "FOR d IN collection FILTER d.a.b.c.e.f in 4.5..5.0 RETURN d", expected,
        &ExpressionContextMock::EMPTY);
    assertFilterSuccess(
        "FOR d IN collection FILTER d.a.b['c.e.f'] in 4.5..5.0 RETURN d",
        expected, &ExpressionContextMock::EMPTY);
  }

  // numeric int-float range
  {
    irs::numeric_token_stream minTerm;
    minTerm.reset(4.0);
    irs::numeric_token_stream maxTerm;
    maxTerm.reset(5.0);

    irs::Or expected;
    auto& range = expected.add<irs::by_granular_range>();
    range.field(mangleNumeric("a.b.c.e.f"));
    range.include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm);
    range.include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess(
        "FOR d IN collection FILTER d.a.b.c.e.f in 4..5.0 RETURN d", expected,
        &ExpressionContextMock::EMPTY);
    assertFilterSuccess(
        "FOR d IN collection FILTER d['a']['b'].c.e['f'] in 4..5.0 RETURN d",
        expected, &ExpressionContextMock::EMPTY);
  }

  // numeric int-float range, boost
  {
    irs::numeric_token_stream minTerm;
    minTerm.reset(4.0);
    irs::numeric_token_stream maxTerm;
    maxTerm.reset(5.0);

    irs::Or expected;
    auto& range = expected.add<irs::by_granular_range>();
    range.boost(1.5);
    range.field(mangleNumeric("a.b.c.e.f"));
    range.include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm);
    range.include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess(
        "FOR d IN collection FILTER boost(d.a.b.c.e.f in 4..5.0, 1.5) RETURN d",
        expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess(
        "FOR d IN collection FILTER boost(d['a']['b'].c.e['f'] in 4..5.0, 1.5) "
        "RETURN d",
        expected, &ExpressionContextMock::EMPTY);
  }

  // numeric expression in range
  {
    arangodb::aql::Variable var("c", 0);
    arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt(2));
    arangodb::aql::AqlValueGuard guard(value, true);

    ExpressionContextMock ctx;
    ctx.vars.emplace(var.name, value);

    irs::numeric_token_stream minTerm;
    minTerm.reset(2.0);
    irs::numeric_token_stream maxTerm;
    maxTerm.reset(102.0);

    irs::Or expected;
    auto& range = expected.add<irs::by_granular_range>();
    range.field(mangleNumeric("a[100].b.c[1].e.f"));
    range.include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm);
    range.include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess(
        "LET c=2 FOR d IN collection FILTER d.a[100].b.c[1].e.f in c..c+100 "
        "RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        "LET c=2 FOR d IN collection FILTER d.a[100]['b'].c[1].e.f in c..c+100 "
        "RETURN d",
        expected, &ctx);
  }

  // numeric expression in range, boost
  {
    arangodb::aql::Variable var("c", 0);
    arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt(2));
    arangodb::aql::AqlValueGuard guard(value, true);

    ExpressionContextMock ctx;
    ctx.vars.emplace(var.name, value);

    irs::numeric_token_stream minTerm;
    minTerm.reset(2.0);
    irs::numeric_token_stream maxTerm;
    maxTerm.reset(102.0);

    irs::Or expected;
    auto& range = expected.add<irs::by_granular_range>();
    range.boost(1.5);
    range.field(mangleNumeric("a[100].b.c[1].e.f"));
    range.include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm);
    range.include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess(
        "LET c=2 FOR d IN collection FILTER boost(d.a[100].b.c[1].e.f in "
        "c..c+100, 1.5) RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        "LET c=2 FOR d IN collection FILTER boost(d.a[100]['b'].c[1].e.f in "
        "c..c+100, 1.5) RETURN d",
        expected, &ctx);
  }

  // dynamic complex attribute name in range
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                                      arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                                      arangodb::aql::AqlValueHintDouble{5.6})));

    irs::numeric_token_stream minTerm;
    minTerm.reset(2.0);
    irs::numeric_token_stream maxTerm;
    maxTerm.reset(102.0);

    irs::Or expected;
    auto& range = expected.add<irs::by_granular_range>();
    range.field(mangleNumeric("a.b.c.e[4].f[5].g[3].g.a"));
    range.include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm);
    range.include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess(
        "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "in 2..102 RETURN d",
        expected, &ctx);
  }

  // invalid dynamic attribute name
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                                      arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail(
        "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "in 2..102 RETURN d",
        &ctx);
  }

  // invalid dynamic attribute name (null value)
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintNull{}));  // invalid value type
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                                      arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                                      arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail(
        "LET a=null LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "in 2..102 RETURN d",
        &ctx);
  }

  // invalid dynamic attribute name (bool value)
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintBool{false}));  // invalid value type
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                                      arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                                      arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail(
        "LET a=false LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "in 2..102 RETURN d",
        &ctx);
  }

  // string range
  {
    irs::numeric_token_stream minTerm;
    minTerm.reset(4.0);
    irs::numeric_token_stream maxTerm;
    maxTerm.reset(5.0);
    irs::Or expected;
    auto& range = expected.add<irs::by_granular_range>();
    range.field(mangleNumeric("a.b.c.e.f"));
    range.include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm);
    range.include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess(
        "FOR d IN collection FILTER d.a.b.c.e.f in '4'..'5' RETURN d", expected,
        &ExpressionContextMock::EMPTY);
    assertFilterSuccess(
        "FOR d IN collection FILTER d.a['b.c.e.f'] in '4'..'5' RETURN d",
        expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess(
        "FOR d IN collection FILTER d['a']['b.c.e.f'] in '4'..'5' RETURN d",
        expected, &ExpressionContextMock::EMPTY);
  }

  // string range, attribute offset
  {
    irs::numeric_token_stream minTerm;
    minTerm.reset(4.0);
    irs::numeric_token_stream maxTerm;
    maxTerm.reset(5.0);
    irs::Or expected;
    auto& range = expected.add<irs::by_granular_range>();
    range.field(mangleNumeric("a.b.c.e.f[4]"));
    range.include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm);
    range.include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess(
        "FOR d IN collection FILTER d.a.b.c.e.f[4] in '4'..'5' RETURN d",
        expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess(
        "FOR d IN collection FILTER d.a['b.c.e.f'][4] in '4'..'5' RETURN d",
        expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess(
        "FOR d IN collection FILTER d['a']['b.c.e.f[4]'] in '4'..'5' RETURN d",
        expected, &ExpressionContextMock::EMPTY);
  }

  // string range, attribute offset, boost
  {
    irs::numeric_token_stream minTerm;
    minTerm.reset(4.0);
    irs::numeric_token_stream maxTerm;
    maxTerm.reset(5.0);
    irs::Or expected;
    auto& range = expected.add<irs::by_granular_range>();
    range.boost(1.5);
    range.field(mangleNumeric("a.b.c.e.f[4]"));
    range.include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm);
    range.include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess(
        "FOR d IN collection FILTER boost(d.a.b.c.e.f[4] in '4'..'5', 1.5) "
        "RETURN d",
        expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess(
        "FOR d IN collection FILTER boost(d.a['b.c.e.f'][4] in '4'..'5', 1.5) "
        "RETURN d",
        expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess(
        "FOR d IN collection FILTER boost(d['a']['b.c.e.f[4]'] in '4'..'5', "
        "1.5) RETURN d",
        expected, &ExpressionContextMock::EMPTY);
  }

  // string range, attribute offset
  {
    irs::numeric_token_stream minTerm;
    minTerm.reset(4.0);
    irs::numeric_token_stream maxTerm;
    maxTerm.reset(5.0);
    irs::Or expected;
    auto& range = expected.add<irs::by_granular_range>();
    range.field(mangleNumeric("a.b.c.e.f[4]"));
    range.include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm);
    range.include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess(
        "FOR d IN collection FILTER d.a.b.c.e.f[4] in '4a'..'5' RETURN d",
        expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess(
        "FOR d IN collection FILTER d['a']['b.c.e.f[4]'] in '4'..'5av' RETURN "
        "d",
        expected, &ExpressionContextMock::EMPTY);
  }

  // string range, attribute offset
  {
    irs::numeric_token_stream minTerm;
    minTerm.reset(0.0);
    irs::numeric_token_stream maxTerm;
    maxTerm.reset(5.0);
    irs::Or expected;
    auto& range = expected.add<irs::by_granular_range>();
    range.field(mangleNumeric("a.b.c.e.f[4]"));
    range.include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm);
    range.include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess(
        "FOR d IN collection FILTER d.a['b.c.e.f'][4] in 'a4'..'5' RETURN d",
        expected, &ExpressionContextMock::EMPTY);
  }

  // string expression in range
  {
    arangodb::aql::Variable var("c", 0);
    arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt(2));
    arangodb::aql::AqlValueGuard guard(value, true);

    ExpressionContextMock ctx;
    ctx.vars.emplace(var.name, value);

    irs::numeric_token_stream minTerm;
    minTerm.reset(2.0);
    irs::numeric_token_stream maxTerm;
    maxTerm.reset(4.0);
    irs::Or expected;
    auto& range = expected.add<irs::by_granular_range>();
    range.field(mangleNumeric("a[100].b.c[1].e.f"));
    range.include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm);
    range.include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess(
        "LET c=2 FOR d IN collection FILTER d.a[100].b.c[1].e.f in "
        "TO_STRING(c)..TO_STRING(c+2) RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        "LET c=2 FOR d IN collection FILTER d.a[100].b.c[1]['e'].f in "
        "TO_STRING(c)..TO_STRING(c+2) RETURN d",
        expected, &ctx);
  }

  // dynamic complex attribute name in range
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                                      arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                                      arangodb::aql::AqlValueHintDouble{5.6})));

    irs::numeric_token_stream minTerm;
    minTerm.reset(2.0);
    irs::numeric_token_stream maxTerm;
    maxTerm.reset(4.0);
    irs::Or expected;
    auto& range = expected.add<irs::by_granular_range>();
    range.field(mangleNumeric("a.b.c.e[4].f[5].g[3].g.a"));
    range.include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm);
    range.include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess(
        "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "in '2'..'4' RETURN d",
        expected, &ctx);
  }

  // invalid dynamic attribute name in range
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                                      arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail(
        "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "in '2'..'4' RETURN d",
        &ctx);
  }

  // invalid dynamic attribute name in range (null value)
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintNull{}));  // invalid value type
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                                      arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                                      arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail(
        "LET a=null LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "in '2'..'4' RETURN d",
        &ctx);
  }

  // invalid dynamic attribute name in range (bool value)
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintBool{false}));  // invalid value type
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                                      arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                                      arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail(
        "LET a=false LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "in '2'..'4' RETURN d",
        &ctx);
  }

  // boolean range
  {
    irs::numeric_token_stream minTerm;
    minTerm.reset(0.0);
    irs::numeric_token_stream maxTerm;
    maxTerm.reset(1.0);
    irs::Or expected;
    auto& range = expected.add<irs::by_granular_range>();
    range.field(mangleNumeric("a.b.c.e.f"));
    range.include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm);
    range.include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess(
        "FOR d IN collection FILTER d.a.b.c.e.f in false..true RETURN d",
        expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess(
        "FOR d IN collection FILTER d['a'].b.c.e.f in false..true RETURN d",
        expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess(
        "FOR d IN collection FILTER d['a'].b['c.e.f'] in false..true RETURN d",
        expected, &ExpressionContextMock::EMPTY);
  }

  // boolean range, attribute offset
  {
    irs::numeric_token_stream minTerm;
    minTerm.reset(0.0);
    irs::numeric_token_stream maxTerm;
    maxTerm.reset(1.0);
    irs::Or expected;
    auto& range = expected.add<irs::by_granular_range>();
    range.field(mangleNumeric("[100].a.b.c.e.f"));
    range.include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm);
    range.include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess(
        "FOR d IN collection FILTER d[100].a.b.c.e.f in false..true RETURN d",
        expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess(
        "FOR d IN collection FILTER d[100]['a'].b.c.e.f in false..true RETURN "
        "d",
        expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess(
        "FOR d IN collection FILTER d[100]['a'].b['c.e.f'] in false..true "
        "RETURN d",
        expected, &ExpressionContextMock::EMPTY);
  }

  // boolean range, attribute offset
  {
    irs::numeric_token_stream minTerm;
    minTerm.reset(0.0);
    irs::numeric_token_stream maxTerm;
    maxTerm.reset(1.0);
    irs::Or expected;
    auto& range = expected.add<irs::by_granular_range>();
    range.boost(2.5);
    range.field(mangleNumeric("[100].a.b.c.e.f"));
    range.include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm);
    range.include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess(
        "FOR d IN collection FILTER BOOST(d[100].a.b.c.e.f in false..true, "
        "2.5) RETURN d",
        expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess(
        "FOR d IN collection FILTER BOOST(d[100]['a'].b.c.e.f in false..true, "
        "2.5) RETURN d",
        expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess(
        "FOR d IN collection FILTER BOOST(d[100]['a'].b['c.e.f'] in "
        "false..true, 2.5) RETURN d",
        expected, &ExpressionContextMock::EMPTY);
  }

  // boolean expression in range
  {
    arangodb::aql::Variable var("c", 0);
    arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt(2));
    arangodb::aql::AqlValueGuard guard(value, true);

    ExpressionContextMock ctx;
    ctx.vars.emplace(var.name, value);

    irs::numeric_token_stream minTerm;
    minTerm.reset(1.0);
    irs::numeric_token_stream maxTerm;
    maxTerm.reset(0.0);
    irs::Or expected;
    auto& range = expected.add<irs::by_granular_range>();
    range.field(mangleNumeric("a[100].b.c[1].e.f"));
    range.include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm);
    range.include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess(
        "LET c=2 FOR d IN collection FILTER d.a[100].b.c[1].e.f in "
        "TO_BOOL(c)..IS_NULL(TO_BOOL(c-2)) RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        "LET c=2 FOR d IN collection FILTER d.a[100].b.c[1]['e'].f in "
        "TO_BOOL(c)..TO_BOOL(c-2) RETURN d",
        expected, &ctx);
  }

  // dynamic complex attribute name in range
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                                      arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                                      arangodb::aql::AqlValueHintDouble{5.6})));

    irs::numeric_token_stream minTerm;
    minTerm.reset(1.0);
    irs::numeric_token_stream maxTerm;
    maxTerm.reset(0.0);
    irs::Or expected;
    auto& range = expected.add<irs::by_granular_range>();
    range.field(mangleNumeric("a.b.c.e[4].f[5].g[3].g.a"));
    range.include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm);
    range.include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess(
        "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "in true..false RETURN d",
        expected, &ctx);
  }

  // invalid dynamic attribute name in range
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                                      arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail(
        "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "in true..false RETURN d",
        &ctx);
  }

  // invalid dynamic attribute name in range (null value)
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintNull{}));  // invalid value type
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                                      arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                                      arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail(
        "LET a=null LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "in true..false RETURN d",
        &ctx);
  }

  // invalid dynamic attribute name in range (bool value)
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintBool{false}));  // invalid value type
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                                      arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                                      arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail(
        "LET a=false LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "in false..true RETURN d",
        &ctx);
  }

  // null range
  {
    irs::numeric_token_stream minTerm;
    minTerm.reset(0.0);
    irs::numeric_token_stream maxTerm;
    maxTerm.reset(0.0);
    irs::Or expected;
    auto& range = expected.add<irs::by_granular_range>();
    range.field(mangleNumeric("a.b.c.e.f"));
    range.include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm);
    range.include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess(
        "FOR d IN collection FILTER d.a.b.c.e.f in null..null RETURN d",
        expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess(
        "FOR d IN collection FILTER d['a.b.c.e.f'] in null..null RETURN d",
        expected, &ExpressionContextMock::EMPTY);
  }

  // null range
  {
    irs::numeric_token_stream minTerm;
    minTerm.reset(0.0);
    irs::numeric_token_stream maxTerm;
    maxTerm.reset(0.0);
    irs::Or expected;
    auto& range = expected.add<irs::by_granular_range>();
    range.field(mangleNumeric("a[100].b.c[1].e[32].f"));
    range.include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm);
    range.include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess(
        "FOR d IN collection FILTER d.a[100].b.c[1].e[32].f in null..null "
        "RETURN d",
        expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess(
        "FOR d IN collection FILTER d['a[100].b.c[1].e[32].f'] in null..null "
        "RETURN d",
        expected, &ExpressionContextMock::EMPTY);
  }

  // null expression in range
  {
    arangodb::aql::Variable var("c", 0);
    arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintNull{});
    arangodb::aql::AqlValueGuard guard(value, true);

    ExpressionContextMock ctx;
    ctx.vars.emplace(var.name, value);

    irs::numeric_token_stream minTerm;
    minTerm.reset(0.0);
    irs::numeric_token_stream maxTerm;
    maxTerm.reset(0.0);
    irs::Or expected;
    auto& range = expected.add<irs::by_granular_range>();
    range.field(mangleNumeric("a[100].b.c[1].e.f"));
    range.include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm);
    range.include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess(
        "LET c=null FOR d IN collection FILTER d.a[100].b.c[1].e.f in c..null "
        "RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        "LET c=null FOR d IN collection FILTER d.a[100].b.c[1]['e'].f in "
        "c..null RETURN d",
        expected, &ctx);
  }

  // null expression in range
  {
    arangodb::aql::Variable var("c", 0);
    arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintNull{});
    arangodb::aql::AqlValueGuard guard(value, true);

    ExpressionContextMock ctx;
    ctx.vars.emplace(var.name, value);

    irs::numeric_token_stream minTerm;
    minTerm.reset(0.0);
    irs::numeric_token_stream maxTerm;
    maxTerm.reset(0.0);
    irs::Or expected;
    auto& range = expected.add<irs::by_granular_range>();
    range.boost(1.5);
    range.field(mangleNumeric("a[100].b.c[1].e.f"));
    range.include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm);
    range.include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess(
        "LET c=null FOR d IN collection FILTER boost(d.a[100].b.c[1].e.f in "
        "c..null, 1.5) RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        "LET c=null FOR d IN collection FILTER boost(d.a[100].b.c[1]['e'].f in "
        "c..null, 1.5) RETURN d",
        expected, &ctx);
  }

  // dynamic complex attribute name in range
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                                      arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                                      arangodb::aql::AqlValueHintDouble{5.6})));

    irs::numeric_token_stream minTerm;
    minTerm.reset(0.0);
    irs::numeric_token_stream maxTerm;
    maxTerm.reset(0.0);
    irs::Or expected;
    auto& range = expected.add<irs::by_granular_range>();
    range.field(mangleNumeric("a.b.c.e[4].f[5].g[3].g.a"));
    range.include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm);
    range.include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess(
        "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "in null..null RETURN d",
        expected, &ctx);
  }

  // invalid dynamic attribute name in range
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                                      arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail(
        "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "in null..null RETURN d",
        &ctx);
  }

  // invalid dynamic attribute name in range (null value)
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintNull{}));  // invalid value type
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                                      arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                                      arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail(
        "LET a=null LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "in null..null RETURN d",
        &ctx);
  }

  // invalid dynamic attribute name in range (bool value)
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintBool{false}));  // invalid value type
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                                      arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                                      arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail(
        "LET a=false LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "in null..null RETURN d",
        &ctx);
  }

  // heterogeneous range
  {
    irs::numeric_token_stream minTerm;
    minTerm.reset(0.0);
    irs::numeric_token_stream maxTerm;
    maxTerm.reset(4.0);
    irs::Or expected;
    auto& range = expected.add<irs::by_granular_range>();
    range.field(mangleNumeric("a"));
    range.include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm);
    range.include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess("FOR d IN myView FILTER d.a in 'a'..4 RETURN d",
                        expected, &ExpressionContextMock::EMPTY);
  }

  // heterogeneous range
  {
    irs::numeric_token_stream minTerm;
    minTerm.reset(1.0);
    irs::numeric_token_stream maxTerm;
    maxTerm.reset(0.0);
    irs::Or expected;
    auto& range = expected.add<irs::by_granular_range>();
    range.field(mangleNumeric("a"));
    range.include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm);
    range.include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess("FOR d IN myView FILTER d.a in 1..null RETURN d",
                        expected, &ExpressionContextMock::EMPTY);
  }

  // heterogeneous range
  {
    irs::numeric_token_stream minTerm;
    minTerm.reset(0.0);
    irs::numeric_token_stream maxTerm;
    maxTerm.reset(5.0);
    irs::Or expected;
    auto& range = expected.add<irs::by_granular_range>();
    range.field(mangleNumeric("a"));
    range.include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm);
    range.include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess("FOR d IN myView FILTER d.a in false..5.5 RETURN d",
                        expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess("FOR d IN myView FILTER d.a in 1..4..5 RETURN d",
                        expected, &ExpressionContextMock::EMPTY);
  }

  // heterogeneous range
  {
    irs::numeric_token_stream minTerm;
    minTerm.reset(0.0);
    irs::numeric_token_stream maxTerm;
    maxTerm.reset(1.0);
    irs::Or expected;
    auto& range = expected.add<irs::by_granular_range>();
    range.field(mangleNumeric("a"));
    range.include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm);
    range.include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess("FOR d IN myView FILTER d.a in 'false'..1 RETURN d",
                        expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess("FOR d IN myView FILTER d.a in 0..true RETURN d",
                        expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess("FOR d IN myView FILTER d.a in null..true RETURN d",
                        expected, &ExpressionContextMock::EMPTY);
  }

  // range as reference
  {
    arangodb::aql::AqlValue value(1, 3);
    arangodb::aql::AqlValueGuard guard(value, true);

    irs::numeric_token_stream stream;
    stream.reset(2.);
    EXPECT_TRUE(stream.next());
    stream.attributes().get<irs::term_attribute>();

    ExpressionContextMock ctx;
    ctx.vars.emplace("x", arangodb::aql::AqlValue(1, 3));

    irs::numeric_token_stream minTerm;
    minTerm.reset(1.0);
    irs::numeric_token_stream maxTerm;
    maxTerm.reset(3.0);
    irs::Or expected;
    auto& range = expected.add<irs::by_granular_range>();
    range.field(mangleNumeric("a.b.c.e.f"));
    range.include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm);
    range.include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess(
        "LET x=1..3 FOR d IN collection FILTER d.a.b.c.e.f in x RETURN d", expected, &ctx);
  }

  // non-deterministic expression name in range
  assertExpressionFilter(
      "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
      "collection FILTER "
      "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_NONDETERM_('a')] "
      "in 4..5 RETURN d");
  assertExpressionFilter(
      "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
      "collection FILTER "
      "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
      "in _NONDETERM_(4)..5 RETURN d");

  // self-reference
  assertExpressionFilter("FOR d IN myView FILTER d in 4..5 RETURN d");
  assertExpressionFilter("for d IN myView filter d[*] in 4..5 return d");
  assertExpressionFilter("for d IN myView filter d.a[*] in 4..5 return d");
  assertExpressionFilter("FOR d IN myView FILTER d.a in d.b..5 RETURN d");
  assertFilterExecutionFail(
      "LET x={} FOR d IN myView FILTER 4..5 in x.a RETURN d",
      &ExpressionContextMock::EMPTY);  // no reference to x
  assertFilterExecutionFail("LET x={} FOR d IN myView FILTER 4 in x.a RETURN d",
                            &ExpressionContextMock::EMPTY);  // no reference to x
  assertExpressionFilter("for d IN myView filter 4..5 in d.a return d");  // self-reference
  assertExpressionFilter("FOR d IN myView FILTER 4 in d.b..5 RETURN d");  // self-reference

  // false expression
  {
    irs::Or expected;
    expected.add<irs::empty>();

    assertFilterSuccess("FOR d IN myView FILTER [] in 4..5 RETURN d", expected,
                        &ExpressionContextMock::EMPTY);
    assertFilterSuccess("FOR d IN myView FILTER ['d'] in 4..5 RETURN d",
                        expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess("FOR d IN myView FILTER 'd.a' in 4..5 RETURN d",
                        expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess("FOR d IN myView FILTER null in 4..5 RETURN d",
                        expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess("FOR d IN myView FILTER true in 4..5 RETURN d",
                        expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess("FOR d IN myView FILTER false in 4..5 RETURN d",
                        expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess("FOR d IN myView FILTER 4.3 in 4..5 RETURN d", expected,
                        &ExpressionContextMock::EMPTY);  // ArangoDB feature
  }

  // true expression
  {
    irs::Or expected;
    expected.add<irs::all>();

    assertFilterSuccess("FOR d IN myView FILTER 4 in 4..5 RETURN d", expected,
                        &ExpressionContextMock::EMPTY);
    assertFilterSuccess("FOR d IN myView FILTER 4 in 4..4+1 RETURN d", expected,
                        &ExpressionContextMock::EMPTY);
  }
}

TEST_F(IResearchFilterInTest, BinaryNotIn) {
  // simple attribute
  {
    irs::Or expected;
    auto& root = expected.add<irs::Not>().filter<irs::And>();
    root.add<irs::by_term>().field(mangleStringIdentity("a")).term("1");
    root.add<irs::by_term>().field(mangleStringIdentity("a")).term("2");
    root.add<irs::by_term>().field(mangleStringIdentity("a")).term("3");

    assertFilterSuccess(
        "FOR d IN collection FILTER d.a not in ['1','2','3'] RETURN d", expected);
    assertFilterSuccess(
        "FOR d IN collection FILTER d['a'] not in ['1','2','3'] RETURN d", expected);
  }

  // simple offset
  {
    irs::Or expected;
    auto& root = expected.add<irs::Not>().filter<irs::And>();
    root.add<irs::by_term>().field(mangleStringIdentity("[1]")).term("1");
    root.add<irs::by_term>().field(mangleStringIdentity("[1]")).term("2");
    root.add<irs::by_term>().field(mangleStringIdentity("[1]")).term("3");

    assertFilterSuccess(
        "FOR d IN collection FILTER d[1] not in ['1','2','3'] RETURN d", expected);
  }

  // complex attribute name
  {
    irs::Or expected;
    auto& root = expected.add<irs::Not>().filter<irs::And>();
    root.add<irs::by_term>().field(mangleStringIdentity("a.b.c.e.f")).term("1");
    root.add<irs::by_term>().field(mangleStringIdentity("a.b.c.e.f")).term("2");
    root.add<irs::by_term>().field(mangleStringIdentity("a.b.c.e.f")).term("3");

    assertFilterSuccess(
        "FOR d IN collection FILTER d.a.b.c.e.f not in ['1','2','3'] RETURN d", expected);
    assertFilterSuccess(
        "FOR d IN collection FILTER d.a['b'].c.e.f not in ['1','2','3'] RETURN "
        "d",
        expected);
    assertFilterSuccess(
        "FOR d IN collection FILTER d.a['b']['c'].e.f not in ['1','2','3'] "
        "RETURN d",
        expected);
  }

  // complex attribute name, offset
  {
    irs::Or expected;
    auto& root = expected.add<irs::Not>().filter<irs::And>();
    root.add<irs::by_term>()
        .field(mangleStringIdentity("a.b.c[323].e.f"))
        .term("1");
    root.add<irs::by_term>()
        .field(mangleStringIdentity("a.b.c[323].e.f"))
        .term("2");
    root.add<irs::by_term>()
        .field(mangleStringIdentity("a.b.c[323].e.f"))
        .term("3");

    assertFilterSuccess(
        "FOR d IN collection FILTER d.a.b.c[323].e.f not in ['1','2','3'] "
        "RETURN d",
        expected);
    assertFilterSuccess(
        "FOR d IN collection FILTER d.a['b'].c[323].e.f not in ['1','2','3'] "
        "RETURN d",
        expected);
    assertFilterSuccess(
        "FOR d IN collection FILTER d.a['b']['c'][323].e.f not in "
        "['1','2','3'] RETURN d",
        expected);
  }

  // complex attribute name, offset
  {
    irs::Or expected;
    auto& root = expected.add<irs::Not>().filter<irs::And>();
    root.boost(1.5);
    root.add<irs::by_term>()
        .field(mangleStringIdentity("a.b.c[323].e.f"))
        .term("1");
    root.add<irs::by_term>()
        .field(mangleStringIdentity("a.b.c[323].e.f"))
        .term("2");
    root.add<irs::by_term>()
        .field(mangleStringIdentity("a.b.c[323].e.f"))
        .term("3");

    assertFilterSuccess(
        "FOR d IN collection FILTER boost(d.a.b.c[323].e.f not in "
        "['1','2','3'], 1.5) RETURN d",
        expected);
    assertFilterSuccess(
        "FOR d IN collection FILTER boost(d.a['b'].c[323].e.f not in "
        "['1','2','3'], 1.5) RETURN d",
        expected);
    assertFilterSuccess(
        "FOR d IN collection FILTER boost(d.a['b']['c'][323].e.f not in "
        "['1','2','3'], 1.5) RETURN d",
        expected);
  }

  // complex attribute name, offset, analyzer
  {
    irs::Or expected;
    auto& root = expected.add<irs::Not>().filter<irs::And>();
    root.add<irs::by_term>()
        .field(mangleString("a.b.c[323].e.f", "test_analyzer"))
        .term("1");
    root.add<irs::by_term>()
        .field(mangleString("a.b.c[323].e.f", "test_analyzer"))
        .term("2");
    root.add<irs::by_term>()
        .field(mangleString("a.b.c[323].e.f", "test_analyzer"))
        .term("3");

    assertFilterSuccess(
        "FOR d IN collection FILTER analyzer(d.a.b.c[323].e.f not in "
        "['1','2','3'], 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        "FOR d IN collection FILTER analyzer(d.a['b'].c[323].e.f not in "
        "['1','2','3'], 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        "FOR d IN collection FILTER analyzer(d.a['b']['c'][323].e.f not in "
        "['1','2','3'], 'test_analyzer') RETURN d",
        expected);
  }

  // complex attribute name, offset, analyzer, boost
  {
    irs::Or expected;
    auto& root = expected.add<irs::Not>().filter<irs::And>();
    root.boost(2.5);
    root.add<irs::by_term>()
        .field(mangleString("a.b.c[323].e.f", "test_analyzer"))
        .term("1");
    root.add<irs::by_term>()
        .field(mangleString("a.b.c[323].e.f", "test_analyzer"))
        .term("2");
    root.add<irs::by_term>()
        .field(mangleString("a.b.c[323].e.f", "test_analyzer"))
        .term("3");

    assertFilterSuccess(
        "FOR d IN collection FILTER boost(analyzer(d.a.b.c[323].e.f not in "
        "['1','2','3'], 'test_analyzer'), 2.5) RETURN d",
        expected);
    assertFilterSuccess(
        "FOR d IN collection FILTER analyzer(boost(d.a['b'].c[323].e.f not in "
        "['1','2','3'], 2.5), 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        "FOR d IN collection FILTER boost(analyzer(d.a['b']['c'][323].e.f not "
        "in ['1','2','3'], 'test_analyzer'), 2.5) RETURN d",
        expected);
  }

  // heterogeneous array values
  {
    irs::Or expected;
    auto& root = expected.add<irs::Not>().filter<irs::And>();
    root.add<irs::by_term>()
        .field(mangleStringIdentity("quick.brown.fox"))
        .term("1");
    root.add<irs::by_term>().field(mangleNull("quick.brown.fox")).term(irs::null_token_stream::value_null());
    root.add<irs::by_term>().field(mangleBool("quick.brown.fox")).term(irs::boolean_token_stream::value_true());
    root.add<irs::by_term>().field(mangleBool("quick.brown.fox")).term(irs::boolean_token_stream::value_false());
    {
      irs::numeric_token_stream stream;
      auto& term = stream.attributes().get<irs::term_attribute>();
      stream.reset(2.);
      EXPECT_TRUE(stream.next());
      root.add<irs::by_term>().field(mangleNumeric("quick.brown.fox")).term(term->value());
    }

    assertFilterSuccess(
        "FOR d IN collection FILTER d.quick.brown.fox not in "
        "['1',null,true,false,2] RETURN d",
        expected);
    assertFilterSuccess(
        "FOR d IN collection FILTER d.quick['brown'].fox not in "
        "['1',null,true,false,2] RETURN d",
        expected);
    assertFilterSuccess(
        "FOR d IN collection FILTER analyzer(d.quick['brown'].fox not in "
        "['1',null,true,false,2], 'identity') RETURN d",
        expected);
  }

  // heterogeneous array values, analyzer
  {
    irs::Or expected;
    auto& root = expected.add<irs::Not>().filter<irs::And>();
    root.add<irs::by_term>()
        .field(mangleString("quick.brown.fox", "test_analyzer"))
        .term("1");
    root.add<irs::by_term>().field(mangleNull("quick.brown.fox")).term(irs::null_token_stream::value_null());
    root.add<irs::by_term>().field(mangleBool("quick.brown.fox")).term(irs::boolean_token_stream::value_true());
    root.add<irs::by_term>().field(mangleBool("quick.brown.fox")).term(irs::boolean_token_stream::value_false());
    {
      irs::numeric_token_stream stream;
      auto& term = stream.attributes().get<irs::term_attribute>();
      stream.reset(2.);
      EXPECT_TRUE(stream.next());
      root.add<irs::by_term>().field(mangleNumeric("quick.brown.fox")).term(term->value());
    }

    assertFilterSuccess(
        "FOR d IN collection FILTER ANALYZER(d.quick.brown.fox not in "
        "['1',null,true,false,2], 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        "FOR d IN collection FILTER ANALYZER(d.quick['brown'].fox not in "
        "['1',null,true,false,2], 'test_analyzer') RETURN d",
        expected);
  }

  // heterogeneous array values, analyzer, boost
  {
    irs::Or expected;
    auto& root = expected.add<irs::Not>().filter<irs::And>();
    root.boost(1.5);
    root.add<irs::by_term>()
        .field(mangleString("quick.brown.fox", "test_analyzer"))
        .term("1");
    root.add<irs::by_term>().field(mangleNull("quick.brown.fox")).term(irs::null_token_stream::value_null());
    root.add<irs::by_term>().field(mangleBool("quick.brown.fox")).term(irs::boolean_token_stream::value_true());
    root.add<irs::by_term>().field(mangleBool("quick.brown.fox")).term(irs::boolean_token_stream::value_false());
    {
      irs::numeric_token_stream stream;
      auto& term = stream.attributes().get<irs::term_attribute>();
      stream.reset(2.);
      EXPECT_TRUE(stream.next());
      root.add<irs::by_term>().field(mangleNumeric("quick.brown.fox")).term(term->value());
    }

    assertFilterSuccess(
        "FOR d IN collection FILTER BOOST(ANALYZER(d.quick.brown.fox not in "
        "['1',null,true,false,2], 'test_analyzer'), 1.5) RETURN d",
        expected);
    assertFilterSuccess(
        "FOR d IN collection FILTER ANALYZER(BOOST(d.quick['brown'].fox not in "
        "['1',null,true,false,2], 1.5), 'test_analyzer') RETURN d",
        expected);
  }

  // empty array
  {
    irs::Or expected;
    expected.add<irs::all>();

    assertFilterSuccess(
        "FOR d IN collection FILTER d.quick.brown.fox not in [] RETURN d", expected);
  }

  // dynamic complex attribute name
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                                      arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                                      arangodb::aql::AqlValueHintDouble{5.6})));

    irs::Or expected;
    auto& root = expected.add<irs::Not>().filter<irs::And>();
    root.add<irs::by_term>()
        .field(mangleStringIdentity("a.b.c.e[4].f[5].g[3].g.a"))
        .term("1");
    root.add<irs::by_term>()
        .field(mangleStringIdentity("a.b.c.e[4].f[5].g[3].g.a"))
        .term("2");
    root.add<irs::by_term>()
        .field(mangleStringIdentity("a.b.c.e[4].f[5].g[3].g.a"))
        .term("3");

    assertFilterSuccess(
        "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "not in ['1','2','3'] RETURN d",
        expected, &ctx);
  }

  // invalid dynamic attribute name
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                                      arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail(
        "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "not in ['1','2','3'] RETURN d",
        &ctx);
  }

  // invalid dynamic attribute name (null value)
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintNull{}));  // invalid value type
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                                      arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                                      arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail(
        "LET a=null LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "not in ['1','2','3'] RETURN d",
        &ctx);
  }

  // invalid dynamic attribute name (bool value)
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintBool{false}));  // invalid value type
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                                      arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                                      arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail(
        "LET a=false LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "not in ['1','2','3'] RETURN d",
        &ctx);
  }

  // array as reference
  {
    auto obj = arangodb::velocypack::Parser::fromJson("[ \"1\", 2, \"3\"]");
    arangodb::aql::AqlValue value(obj->slice());
    arangodb::aql::AqlValueGuard guard(value, true);

    irs::numeric_token_stream stream;
    stream.reset(2.);
    EXPECT_TRUE(stream.next());
    auto& term = stream.attributes().get<irs::term_attribute>();

    ExpressionContextMock ctx;
    ctx.vars.emplace("x", value);

    irs::Or expected;
    auto& root = expected.add<irs::Not>().filter<irs::And>();
    root.add<irs::by_term>().field(mangleStringIdentity("a.b.c.e.f")).term("1");
    root.add<irs::by_term>().field(mangleNumeric("a.b.c.e.f")).term(term->value());
    root.add<irs::by_term>().field(mangleStringIdentity("a.b.c.e.f")).term("3");

    assertFilterSuccess(
        "LET x=['1', 2, '3'] FOR d IN collection FILTER d.a.b.c.e.f not in x "
        "RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        "LET x=['1', 2, '3'] FOR d IN collection FILTER analyzer(d.a.b.c.e.f "
        "not in x, 'identity') RETURN d",
        expected, &ctx);
  }

  // array as reference, analyzer
  {
    auto obj = arangodb::velocypack::Parser::fromJson("[ \"1\", 2, \"3\"]");
    arangodb::aql::AqlValue value(obj->slice());
    arangodb::aql::AqlValueGuard guard(value, true);

    irs::numeric_token_stream stream;
    stream.reset(2.);
    EXPECT_TRUE(stream.next());
    auto& term = stream.attributes().get<irs::term_attribute>();

    ExpressionContextMock ctx;
    ctx.vars.emplace("x", value);

    irs::Or expected;
    auto& root = expected.add<irs::Not>().filter<irs::And>();
    root.add<irs::by_term>()
        .field(mangleString("a.b.c.e.f", "test_analyzer"))
        .term("1");
    root.add<irs::by_term>().field(mangleNumeric("a.b.c.e.f")).term(term->value());
    root.add<irs::by_term>()
        .field(mangleString("a.b.c.e.f", "test_analyzer"))
        .term("3");

    assertFilterSuccess(
        "LET x=['1', 2, '3'] FOR d IN collection FILTER analyzer(d.a.b.c.e.f "
        "not in x, 'test_analyzer') RETURN d",
        expected, &ctx);
  }

  // array as reference, analyzer, boost
  {
    auto obj = arangodb::velocypack::Parser::fromJson("[ \"1\", 2, \"3\"]");
    arangodb::aql::AqlValue value(obj->slice());
    arangodb::aql::AqlValueGuard guard(value, true);

    irs::numeric_token_stream stream;
    stream.reset(2.);
    EXPECT_TRUE(stream.next());
    auto& term = stream.attributes().get<irs::term_attribute>();

    ExpressionContextMock ctx;
    ctx.vars.emplace("x", value);

    irs::Or expected;
    auto& root = expected.add<irs::Not>().filter<irs::And>();
    root.boost(3.5);
    root.add<irs::by_term>()
        .field(mangleString("a.b.c.e.f", "test_analyzer"))
        .term("1");
    root.add<irs::by_term>().field(mangleNumeric("a.b.c.e.f")).term(term->value());
    root.add<irs::by_term>()
        .field(mangleString("a.b.c.e.f", "test_analyzer"))
        .term("3");

    assertFilterSuccess(
        "LET x=['1', 2, '3'] FOR d IN collection FILTER "
        "boost(analyzer(d.a.b.c.e.f not in x, 'test_analyzer'), 3.5) RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        "LET x=['1', 2, '3'] FOR d IN collection FILTER "
        "analyzer(boost(d.a.b.c.e.f not in x, 3.5), 'test_analyzer') RETURN d",
        expected, &ctx);
  }

  // reference in array
  {
    arangodb::aql::Variable var("c", 0);
    arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt(2));
    arangodb::aql::AqlValueGuard guard(value, true);

    irs::numeric_token_stream stream;
    stream.reset(2.);
    EXPECT_TRUE(stream.next());
    auto& term = stream.attributes().get<irs::term_attribute>();

    ExpressionContextMock ctx;
    ctx.vars.emplace(var.name, value);

    irs::Or expected;
    auto& root = expected.add<irs::Not>().filter<irs::And>();
    root.add<irs::by_term>().field(mangleStringIdentity("a.b.c.e.f")).term("1");
    root.add<irs::by_term>().field(mangleNumeric("a.b.c.e.f")).term(term->value());
    root.add<irs::by_term>().field(mangleStringIdentity("a.b.c.e.f")).term("3");

    // not a constant in array
    assertFilterSuccess(
        "LET c=2 FOR d IN collection FILTER d.a.b.c.e.f not in ['1', c, '3'] "
        "RETURN d",
        expected,
        &ctx  // expression context
    );
  }

  // reference in array, analyzer
  {
    arangodb::aql::Variable var("c", 0);
    arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt(2));
    arangodb::aql::AqlValueGuard guard(value, true);

    irs::numeric_token_stream stream;
    stream.reset(2.);
    EXPECT_TRUE(stream.next());
    auto& term = stream.attributes().get<irs::term_attribute>();

    ExpressionContextMock ctx;
    ctx.vars.emplace(var.name, value);

    irs::Or expected;
    auto& root = expected.add<irs::Not>().filter<irs::And>();
    root.add<irs::by_term>()
        .field(mangleString("a.b.c.e.f", "test_analyzer"))
        .term("1");
    root.add<irs::by_term>().field(mangleNumeric("a.b.c.e.f")).term(term->value());
    root.add<irs::by_term>()
        .field(mangleString("a.b.c.e.f", "test_analyzer"))
        .term("3");

    // not a constant in array
    assertFilterSuccess(
        "LET c=2 FOR d IN collection FILTER analyzer(d.a.b.c.e.f not in ['1', "
        "c, '3'], 'test_analyzer') RETURN d",
        expected,
        &ctx  // expression context
    );
  }

  // reference in array, analyzer, boost
  {
    arangodb::aql::Variable var("c", 0);
    arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt(2));
    arangodb::aql::AqlValueGuard guard(value, true);

    irs::numeric_token_stream stream;
    stream.reset(2.);
    EXPECT_TRUE(stream.next());
    auto& term = stream.attributes().get<irs::term_attribute>();

    ExpressionContextMock ctx;
    ctx.vars.emplace(var.name, value);

    irs::Or expected;
    auto& root = expected.add<irs::Not>().filter<irs::And>();
    root.boost(1.5);
    root.add<irs::by_term>()
        .field(mangleString("a.b.c.e.f", "test_analyzer"))
        .term("1");
    root.add<irs::by_term>().field(mangleNumeric("a.b.c.e.f")).term(term->value());
    root.add<irs::by_term>()
        .field(mangleString("a.b.c.e.f", "test_analyzer"))
        .term("3");

    // not a constant in array
    assertFilterSuccess(
        "LET c=2 FOR d IN collection FILTER boost(analyzer(d.a.b.c.e.f not in "
        "['1', c, '3'], 'test_analyzer'), 1.5) RETURN d",
        expected,
        &ctx  // expression context
    );
    assertFilterSuccess(
        "LET c=2 FOR d IN collection FILTER analyzer(boost(d.a.b.c.e.f not in "
        "['1', c, '3'], 1.5), 'test_analyzer') RETURN d",
        expected,
        &ctx  // expression context
    );
  }

  // nondeterministic value
  {
    std::string const queryString =
        "FOR d IN collection FILTER d.a.b.c.e.f not in [ '1', RAND(), '3' ] "
        "RETURN d";
    std::string const refName = "d";

    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                          testDatabaseArgs);

    auto options = std::make_shared<arangodb::velocypack::Builder>();

    arangodb::aql::Query query(false, vocbase, arangodb::aql::QueryString(queryString),
                               nullptr, options, arangodb::aql::PART_MAIN);

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
      arangodb::iresearch::QueryContext const ctx{nullptr, nullptr, nullptr, nullptr, ref};
      EXPECT_TRUE(
          (arangodb::iresearch::FilterFactory::filter(nullptr, ctx, *filterNode).ok()));
    }

    // iteratorForCondition
    {
      arangodb::transaction::Methods trx(arangodb::transaction::StandaloneContext::Create(vocbase),
                                         {}, {}, {}, arangodb::transaction::Options());

      auto dummyPlan = arangodb::tests::planFromQuery(vocbase, "RETURN 1");

      irs::Or actual;
      arangodb::iresearch::QueryContext const ctx{&trx, dummyPlan.get(), ast,
                                                  &ExpressionContextMock::EMPTY, ref};
      EXPECT_TRUE(
          (arangodb::iresearch::FilterFactory::filter(&actual, ctx, *filterNode).ok()));

      {
        EXPECT_TRUE(1 == actual.size());

        auto& notNode = dynamic_cast<irs::Not&>(*actual.begin());
        EXPECT_TRUE(irs::Not::type() == notNode.type());

        auto const* andNode = dynamic_cast<irs::And const*>(notNode.filter());
        EXPECT_TRUE(andNode);
        EXPECT_TRUE(irs::And::type() == andNode->type());
        EXPECT_TRUE(3 == andNode->size());

        auto begin = andNode->begin();

        // 1st filter
        {
          irs::by_term expected;
          expected.field(mangleStringIdentity("a.b.c.e.f")).term("1");
          EXPECT_TRUE(expected == *begin);
        }

        // 2nd filter
        {
          ++begin;
          EXPECT_TRUE(arangodb::iresearch::ByExpression::type() == begin->type());
          EXPECT_TRUE(nullptr !=
                      dynamic_cast<arangodb::iresearch::ByExpression const*>(&*begin));
        }

        // 3rd filter
        {
          ++begin;
          irs::by_term expected;
          expected.field(mangleStringIdentity("a.b.c.e.f")).term("3");
          EXPECT_TRUE(expected == *begin);
        }

        EXPECT_TRUE(andNode->end() == ++begin);
      }
    }
  }

  // self-referenced value
  {
    std::string const queryString =
        "FOR d IN collection FILTER d.a.b.c.e.f not in [ '1', d.a, '3' ] "
        "RETURN d";
    std::string const refName = "d";

    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                          testDatabaseArgs);

    auto options = std::make_shared<arangodb::velocypack::Builder>();

    arangodb::aql::Query query(false, vocbase, arangodb::aql::QueryString(queryString),
                               nullptr, options, arangodb::aql::PART_MAIN);

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
      arangodb::iresearch::QueryContext const ctx{nullptr, nullptr, nullptr, nullptr, ref};
      EXPECT_TRUE(
          (arangodb::iresearch::FilterFactory::filter(nullptr, ctx, *filterNode).ok()));
    }

    // iteratorForCondition
    {
      arangodb::transaction::Methods trx(arangodb::transaction::StandaloneContext::Create(vocbase),
                                         {}, {}, {}, arangodb::transaction::Options());

      auto dummyPlan = arangodb::tests::planFromQuery(vocbase, "RETURN 1");

      irs::Or actual;
      arangodb::iresearch::QueryContext const ctx{&trx, dummyPlan.get(), ast,
                                                  &ExpressionContextMock::EMPTY, ref};
      EXPECT_TRUE(
          (arangodb::iresearch::FilterFactory::filter(&actual, ctx, *filterNode).ok()));

      {
        EXPECT_TRUE(1 == actual.size());

        auto& notNode = dynamic_cast<irs::Not&>(*actual.begin());
        EXPECT_TRUE(irs::Not::type() == notNode.type());

        auto const* andNode = dynamic_cast<irs::And const*>(notNode.filter());
        EXPECT_TRUE(andNode);
        EXPECT_TRUE(irs::And::type() == andNode->type());
        EXPECT_TRUE(3 == andNode->size());

        auto begin = andNode->begin();

        // 1st filter
        {
          irs::by_term expected;
          expected.field(mangleStringIdentity("a.b.c.e.f")).term("1");
          EXPECT_TRUE(expected == *begin);
        }

        // 2nd filter
        {
          ++begin;
          EXPECT_TRUE(arangodb::iresearch::ByExpression::type() == begin->type());
          EXPECT_TRUE(nullptr !=
                      dynamic_cast<arangodb::iresearch::ByExpression const*>(&*begin));
        }

        // 3rd filter
        {
          ++begin;
          irs::by_term expected;
          expected.field(mangleStringIdentity("a.b.c.e.f")).term("3");
          EXPECT_TRUE(expected == *begin);
        }

        EXPECT_TRUE(andNode->end() == ++begin);
      }
    }
  }

  // self-referenced value
  {
    std::string const queryString =
        "FOR d IN collection FILTER d.a.b.c.e.f not in [ '1', 1+d.a, '3' ] "
        "RETURN d";
    std::string const refName = "d";

    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                          testDatabaseArgs);

    auto options = std::make_shared<arangodb::velocypack::Builder>();

    arangodb::aql::Query query(false, vocbase, arangodb::aql::QueryString(queryString),
                               nullptr, options, arangodb::aql::PART_MAIN);

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
      arangodb::iresearch::QueryContext const ctx{nullptr, nullptr, nullptr, nullptr, ref};
      EXPECT_TRUE(
          (arangodb::iresearch::FilterFactory::filter(nullptr, ctx, *filterNode).ok()));
    }

    // iteratorForCondition
    {
      arangodb::transaction::Methods trx(arangodb::transaction::StandaloneContext::Create(vocbase),
                                         {}, {}, {}, arangodb::transaction::Options());

      auto dummyPlan = arangodb::tests::planFromQuery(vocbase, "RETURN 1");

      irs::Or actual;
      arangodb::iresearch::QueryContext const ctx{&trx, dummyPlan.get(), ast,
                                                  &ExpressionContextMock::EMPTY, ref};
      EXPECT_TRUE(
          (arangodb::iresearch::FilterFactory::filter(&actual, ctx, *filterNode).ok()));

      {
        EXPECT_TRUE(1 == actual.size());

        auto& notNode = dynamic_cast<irs::Not&>(*actual.begin());
        EXPECT_TRUE(irs::Not::type() == notNode.type());

        auto const* andNode = dynamic_cast<irs::And const*>(notNode.filter());
        EXPECT_TRUE(andNode);
        EXPECT_TRUE(irs::And::type() == andNode->type());
        EXPECT_TRUE(3 == andNode->size());

        auto begin = andNode->begin();

        // 1st filter
        {
          irs::by_term expected;
          expected.field(mangleStringIdentity("a.b.c.e.f")).term("1");
          EXPECT_TRUE(expected == *begin);
        }

        // 2nd filter
        {
          ++begin;
          EXPECT_TRUE(arangodb::iresearch::ByExpression::type() == begin->type());
          EXPECT_TRUE(nullptr !=
                      dynamic_cast<arangodb::iresearch::ByExpression const*>(&*begin));
        }

        // 3rd filter
        {
          ++begin;
          irs::by_term expected;
          expected.field(mangleStringIdentity("a.b.c.e.f")).term("3");
          EXPECT_TRUE(expected == *begin);
        }

        EXPECT_TRUE(andNode->end() == ++begin);
      }
    }
  }

  // self-referenced value, boost
  {
    std::string const queryString =
        "FOR d IN collection FILTER boost(d.a.b.c.e.f not in [ '1', 1+d.a, '3' "
        "], 1.5) RETURN d";
    std::string const refName = "d";

    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                          testDatabaseArgs);

    auto options = std::make_shared<arangodb::velocypack::Builder>();

    arangodb::aql::Query query(false, vocbase, arangodb::aql::QueryString(queryString),
                               nullptr, options, arangodb::aql::PART_MAIN);

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
      arangodb::iresearch::QueryContext const ctx{nullptr, nullptr, nullptr, nullptr, ref};
      EXPECT_TRUE(
          (arangodb::iresearch::FilterFactory::filter(nullptr, ctx, *filterNode).ok()));
    }

    // iteratorForCondition
    {
      arangodb::transaction::Methods trx(arangodb::transaction::StandaloneContext::Create(vocbase),
                                         {}, {}, {}, arangodb::transaction::Options());

      auto dummyPlan = arangodb::tests::planFromQuery(vocbase, "RETURN 1");

      irs::Or actual;
      arangodb::iresearch::QueryContext const ctx{&trx, dummyPlan.get(), ast,
                                                  &ExpressionContextMock::EMPTY, ref};
      EXPECT_TRUE(
          (arangodb::iresearch::FilterFactory::filter(&actual, ctx, *filterNode).ok()));

      {
        EXPECT_TRUE(1 == actual.size());

        auto& notNode = dynamic_cast<irs::Not&>(*actual.begin());
        EXPECT_TRUE(irs::Not::type() == notNode.type());

        auto const* andNode = dynamic_cast<irs::And const*>(notNode.filter());
        EXPECT_TRUE(andNode);
        EXPECT_TRUE(irs::And::type() == andNode->type());
        EXPECT_TRUE(3 == andNode->size());
        EXPECT_TRUE(1.5f == andNode->boost());

        auto begin = andNode->begin();

        // 1st filter
        {
          irs::by_term expected;
          expected.field(mangleStringIdentity("a.b.c.e.f")).term("1");
          EXPECT_TRUE(expected == *begin);
        }

        // 2nd filter
        {
          ++begin;
          EXPECT_TRUE(arangodb::iresearch::ByExpression::type() == begin->type());
          EXPECT_TRUE(nullptr !=
                      dynamic_cast<arangodb::iresearch::ByExpression const*>(&*begin));
        }

        // 3rd filter
        {
          ++begin;
          irs::by_term expected;
          expected.field(mangleStringIdentity("a.b.c.e.f")).term("3");
          EXPECT_TRUE(expected == *begin);
        }

        EXPECT_TRUE(andNode->end() == ++begin);
      }
    }
  }

  // self-referenced value
  {
    std::string const queryString =
        "FOR d IN collection FILTER d.a.b.c.e.f not in [ '1', d.e, d.a.b.c.e.f "
        "] RETURN d";
    std::string const refName = "d";

    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                          testDatabaseArgs);

    auto options = std::make_shared<arangodb::velocypack::Builder>();

    arangodb::aql::Query query(false, vocbase, arangodb::aql::QueryString(queryString),
                               nullptr, options, arangodb::aql::PART_MAIN);

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
      arangodb::iresearch::QueryContext const ctx{nullptr, nullptr, nullptr, nullptr, ref};
      EXPECT_TRUE(
          (arangodb::iresearch::FilterFactory::filter(nullptr, ctx, *filterNode).ok()));
    }

    // iteratorForCondition
    {
      arangodb::transaction::Methods trx(arangodb::transaction::StandaloneContext::Create(vocbase),
                                         {}, {}, {}, arangodb::transaction::Options());

      auto dummyPlan = arangodb::tests::planFromQuery(vocbase, "RETURN 1");

      irs::Or actual;
      arangodb::iresearch::QueryContext const ctx{&trx, dummyPlan.get(), ast,
                                                  &ExpressionContextMock::EMPTY, ref};
      EXPECT_TRUE(
          (arangodb::iresearch::FilterFactory::filter(&actual, ctx, *filterNode).ok()));

      {
        EXPECT_TRUE(1 == actual.size());

        auto& notNode = dynamic_cast<irs::Not&>(*actual.begin());
        EXPECT_TRUE(irs::Not::type() == notNode.type());

        auto const* andNode = dynamic_cast<irs::And const*>(notNode.filter());
        EXPECT_TRUE(andNode);
        EXPECT_TRUE(irs::And::type() == andNode->type());
        EXPECT_TRUE(3 == andNode->size());

        auto begin = andNode->begin();

        // 1st filter
        {
          irs::by_term expected;
          expected.field(mangleStringIdentity("a.b.c.e.f")).term("1");
          EXPECT_TRUE(expected == *begin);
        }

        // 2nd filter
        {
          ++begin;
          EXPECT_TRUE(arangodb::iresearch::ByExpression::type() == begin->type());
          EXPECT_TRUE(nullptr !=
                      dynamic_cast<arangodb::iresearch::ByExpression const*>(&*begin));
        }

        // 3rd filter
        {
          ++begin;
          EXPECT_TRUE(arangodb::iresearch::ByExpression::type() == begin->type());
          EXPECT_TRUE(nullptr !=
                      dynamic_cast<arangodb::iresearch::ByExpression const*>(&*begin));
        }

        EXPECT_TRUE(andNode->end() == ++begin);
      }
    }
  }

  // nondeterministic attribute access in value
  {
    std::string const queryString =
        "FOR d IN collection FILTER 4 not in [ 1, d.a[_NONDETERM_('abc')], 4 ] "
        "RETURN d";
    std::string const refName = "d";

    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                          testDatabaseArgs);

    auto options = std::make_shared<arangodb::velocypack::Builder>();

    arangodb::aql::Query query(false, vocbase, arangodb::aql::QueryString(queryString),
                               nullptr, options, arangodb::aql::PART_MAIN);

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
      arangodb::iresearch::QueryContext const ctx{nullptr, nullptr, nullptr, nullptr, ref};
      EXPECT_TRUE(
          (arangodb::iresearch::FilterFactory::filter(nullptr, ctx, *filterNode).ok()));
    }

    // iteratorForCondition
    {
      arangodb::transaction::Methods trx(arangodb::transaction::StandaloneContext::Create(vocbase),
                                         {}, {}, {}, arangodb::transaction::Options());

      auto dummyPlan = arangodb::tests::planFromQuery(vocbase, "RETURN 1");

      irs::Or actual;
      arangodb::iresearch::QueryContext const ctx{&trx, dummyPlan.get(), ast,
                                                  &ExpressionContextMock::EMPTY, ref};
      EXPECT_TRUE(
          (arangodb::iresearch::FilterFactory::filter(&actual, ctx, *filterNode).ok()));

      {
        EXPECT_TRUE(1 == actual.size());
        auto& notNode = dynamic_cast<irs::Not&>(*actual.begin());
        EXPECT_TRUE(irs::Not::type() == notNode.type());
        auto const* andNode = dynamic_cast<irs::And const*>(notNode.filter());
        EXPECT_TRUE(andNode);
        EXPECT_TRUE(irs::And::type() == andNode->type());
        EXPECT_TRUE(3 == andNode->size());
        auto begin = andNode->begin();

        // 1st filter
        { EXPECT_TRUE(irs::empty() == *begin); }

        // 2nd filter
        {
          ++begin;
          EXPECT_TRUE(arangodb::iresearch::ByExpression::type() == begin->type());
          EXPECT_TRUE(nullptr !=
                      dynamic_cast<arangodb::iresearch::ByExpression const*>(&*begin));
        }

        // 3rd filter
        { EXPECT_TRUE(irs::all() == *++begin); }

        EXPECT_TRUE(andNode->end() == ++begin);
      }
    }
  }

  // self-reference in value
  {
    std::string const queryString =
        "FOR d IN collection FILTER 4 not in [ 1, d.b.a, 4 ] RETURN d";
    std::string const refName = "d";

    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                          testDatabaseArgs);

    auto options = std::make_shared<arangodb::velocypack::Builder>();

    arangodb::aql::Query query(false, vocbase, arangodb::aql::QueryString(queryString),
                               nullptr, options, arangodb::aql::PART_MAIN);

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
      arangodb::iresearch::QueryContext const ctx{nullptr, nullptr, nullptr, nullptr, ref};
      EXPECT_TRUE(
          (arangodb::iresearch::FilterFactory::filter(nullptr, ctx, *filterNode).ok()));
    }

    // iteratorForCondition
    {
      arangodb::transaction::Methods trx(arangodb::transaction::StandaloneContext::Create(vocbase),
                                         {}, {}, {}, arangodb::transaction::Options());

      auto dummyPlan = arangodb::tests::planFromQuery(vocbase, "RETURN 1");

      irs::Or actual;
      arangodb::iresearch::QueryContext const ctx{&trx, dummyPlan.get(), ast,
                                                  &ExpressionContextMock::EMPTY, ref};
      EXPECT_TRUE(
          (arangodb::iresearch::FilterFactory::filter(&actual, ctx, *filterNode).ok()));

      {
        EXPECT_TRUE(1 == actual.size());

        auto& notNode = dynamic_cast<irs::Not&>(*actual.begin());
        EXPECT_TRUE(irs::Not::type() == notNode.type());

        auto const* andNode = dynamic_cast<irs::And const*>(notNode.filter());
        EXPECT_TRUE(andNode);
        EXPECT_TRUE(irs::And::type() == andNode->type());
        EXPECT_TRUE(3 == andNode->size());

        auto begin = andNode->begin();

        // 1st filter
        { EXPECT_TRUE(irs::empty() == *begin); }

        // 2nd filter
        {
          irs::numeric_token_stream stream;
          stream.reset(4.0);
          auto& term = stream.attributes().get<irs::term_attribute>();
          EXPECT_TRUE(stream.next());

          irs::by_term expected;
          expected.field(mangleNumeric("b.a")).term(term->value());
          EXPECT_TRUE(expected == *++begin);
        }

        // 3rd filter
        { EXPECT_TRUE(irs::all() == *++begin); }

        EXPECT_TRUE(andNode->end() == ++begin);
      }
    }
  }

  assertExpressionFilter(
      "FOR d IN collection FILTER 4 not in [ 1, 1+d.b, 4 ] RETURN d");

  // heterogeneous references and expression in array
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("strVal", arangodb::aql::AqlValue("str"));
    ctx.vars.emplace("boolVal",
                     arangodb::aql::AqlValue(arangodb::aql::AqlValueHintBool(false)));
    ctx.vars.emplace("numVal", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt(2)));
    ctx.vars.emplace("nullVal", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintNull{}));

    irs::numeric_token_stream stream;
    stream.reset(3.);
    EXPECT_TRUE(stream.next());
    auto& term = stream.attributes().get<irs::term_attribute>();

    irs::Or expected;
    auto& root = expected.add<irs::Not>().filter<irs::And>();
    root.add<irs::by_term>().field(mangleStringIdentity("a.b.c.e.f")).term("1");
    root.add<irs::by_term>()
        .field(mangleStringIdentity("a.b.c.e.f"))
        .term("str");
    root.add<irs::by_term>().field(mangleBool("a.b.c.e.f")).term(irs::boolean_token_stream::value_false());
    root.add<irs::by_term>().field(mangleNumeric("a.b.c.e.f")).term(term->value());
    root.add<irs::by_term>().field(mangleNull("a.b.c.e.f")).term(irs::null_token_stream::value_null());

    // not a constant in array
    assertFilterSuccess(
        "LET strVal='str' LET boolVal=false LET numVal=2 LET nullVal=null FOR "
        "d IN collection FILTER d.a.b.c.e.f not in ['1', strVal, boolVal, "
        "numVal+1, nullVal] RETURN d",
        expected,
        &ctx  // expression context
    );
  }

  // heterogeneous references and expression in array
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("strVal", arangodb::aql::AqlValue("str"));
    ctx.vars.emplace("boolVal",
                     arangodb::aql::AqlValue(arangodb::aql::AqlValueHintBool(false)));
    ctx.vars.emplace("numVal", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt(2)));
    ctx.vars.emplace("nullVal", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintNull{}));

    irs::numeric_token_stream stream;
    stream.reset(3.);
    EXPECT_TRUE(stream.next());
    auto& term = stream.attributes().get<irs::term_attribute>();

    irs::Or expected;
    auto& root = expected.add<irs::Not>().filter<irs::And>();
    root.boost(2.5);
    root.add<irs::by_term>().field(mangleStringIdentity("a.b.c.e.f")).term("1");
    root.add<irs::by_term>()
        .field(mangleStringIdentity("a.b.c.e.f"))
        .term("str");
    root.add<irs::by_term>().field(mangleBool("a.b.c.e.f")).term(irs::boolean_token_stream::value_false());
    root.add<irs::by_term>().field(mangleNumeric("a.b.c.e.f")).term(term->value());
    root.add<irs::by_term>().field(mangleNull("a.b.c.e.f")).term(irs::null_token_stream::value_null());

    // not a constant in array
    assertFilterSuccess(
        "LET strVal='str' LET boolVal=false LET numVal=2 LET nullVal=null FOR "
        "d IN collection FILTER BOOST(d.a.b.c.e.f not in ['1', strVal, "
        "boolVal, numVal+1, nullVal], 2.5) RETURN d",
        expected,
        &ctx  // expression context
    );
  }

  assertExpressionFilter(
      "FOR d IN myView FILTER [1,2,'3'] not in d.a RETURN d");

  // self-reference
  assertExpressionFilter("FOR d IN myView FILTER d not in [1,2,3] RETURN d");
  assertExpressionFilter("FOR d IN myView FILTER d[*] not in [1,2,3] RETURN d");
  assertExpressionFilter(
      "FOR d IN myView FILTER d.a[*] not in [1,2,3] RETURN d");
  assertExpressionFilter("FOR d IN myView FILTER 4 not in [1,d,3] RETURN d");

  // no reference provided
  assertFilterExecutionFail(
      "LET x={} FOR d IN myView FILTER d.a not in [1,x.a,3] RETURN d",
      &ExpressionContextMock::EMPTY);

  // false expression
  {
    irs::Or expected;
    expected.add<irs::empty>();

    assertFilterSuccess("FOR d IN myView FILTER 4 not in [1,2,3,4] RETURN d",
                        expected, &ExpressionContextMock::EMPTY);
  }

  // true expression
  {
    irs::Or expected;
    expected.add<irs::all>();

    assertFilterSuccess("FOR d IN myView FILTER [] not in [1,2,3] RETURN d",
                        expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess("FOR d IN myView FILTER ['d'] not in [1,2,3] RETURN d",
                        expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess("FOR d IN myView FILTER 'd.a' not in [1,2,3] RETURN d",
                        expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess("FOR d IN myView FILTER null not in [1,2,3] RETURN d",
                        expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess("FOR d IN myView FILTER true not in [1,2,3] RETURN d",
                        expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess("FOR d IN myView FILTER false not in [1,2,3] RETURN d",
                        expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess("FOR d IN myView FILTER 4 not in [1,2,3] RETURN d",
                        expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess("FOR d IN myView FILTER 4.5 not in [1,2,3] RETURN d",
                        expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess("FOR d IN myView FILTER 1..2 not in [1,2,3] RETURN d", expected,
                        &ExpressionContextMock::EMPTY);  // by some reason arangodb evaluates it to true
  }

  // true expression, boost
  {
    irs::Or expected;
    expected.add<irs::all>().boost(1.5);

    assertFilterSuccess(
        "FOR d IN myView FILTER BOOST([] not in [1,2,3],1.5) RETURN d",
        expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess(
        "FOR d IN myView FILTER BOOST(['d'] not in [1,2,3],1.5) RETURN d",
        expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess(
        "FOR d IN myView FILTER BOOST('d.a' not in [1,2,3],1.5) RETURN d",
        expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess(
        "FOR d IN myView FILTER BOOST(null not in [1,2,3],1.5) RETURN d",
        expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess(
        "FOR d IN myView FILTER BOOST(true not in [1,2,3],1.5) RETURN d",
        expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess(
        "FOR d IN myView FILTER BOOST(false not in [1,2,3],1.5) RETURN d",
        expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess(
        "FOR d IN myView FILTER BOOST(4 not in [1,2,3],1.5) RETURN d", expected,
        &ExpressionContextMock::EMPTY);
    assertFilterSuccess(
        "FOR d IN myView FILTER BOOST(4.5 not in [1,2,3],1.5) RETURN d",
        expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess(
        "FOR d IN myView FILTER BOOST(1..2 not in [1,2,3],1.5) RETURN d", expected,
        &ExpressionContextMock::EMPTY);  // by some reason arangodb evaluates it to true
  }

  // not a value in array
  assertFilterFail(
      "FOR d IN collection FILTER d.a not in ['1',['2'],'3'] RETURN d");

  // numeric range
  {
    irs::numeric_token_stream minTerm;
    minTerm.reset(4.0);
    irs::numeric_token_stream maxTerm;
    maxTerm.reset(5.0);

    irs::Or expected;
    auto& range =
        expected.add<irs::Not>().filter<irs::Or>().add<irs::by_granular_range>();
    range.field(mangleNumeric("a.b.c.e.f"));
    range.include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm);
    range.include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess(
        "FOR d IN collection FILTER d.a.b.c.e.f not in 4..5 RETURN d", expected,
        &ExpressionContextMock::EMPTY);
    assertFilterSuccess(
        "FOR d IN collection FILTER d.a['b.c.e.f'] not in 4..5 RETURN d",
        expected, &ExpressionContextMock::EMPTY);
  }

  // numeric range, boost
  {
    irs::numeric_token_stream minTerm;
    minTerm.reset(4.0);
    irs::numeric_token_stream maxTerm;
    maxTerm.reset(5.0);

    irs::Or expected;
    auto& range =
        expected.add<irs::Not>().filter<irs::Or>().add<irs::by_granular_range>();
    range.boost(2.5);
    range.field(mangleNumeric("a.b.c.e.f"));
    range.include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm);
    range.include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess(
        "FOR d IN collection FILTER boost(d.a.b.c.e.f not in 4..5, 2.5) RETURN "
        "d",
        expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess(
        "FOR d IN collection FILTER BOOST(d.a['b.c.e.f'] not in 4..5, 2.5) "
        "RETURN d",
        expected, &ExpressionContextMock::EMPTY);
  }

  // numeric range, attribute offset
  {
    irs::numeric_token_stream minTerm;
    minTerm.reset(4.0);
    irs::numeric_token_stream maxTerm;
    maxTerm.reset(5.0);

    irs::Or expected;
    auto& range =
        expected.add<irs::Not>().filter<irs::Or>().add<irs::by_granular_range>();
    range.field(mangleNumeric("a.b[4].c.e.f"));
    range.include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm);
    range.include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess(
        "FOR d IN collection FILTER d.a.b[4].c.e.f not in 4..5 RETURN d",
        expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess(
        "FOR d IN collection FILTER d.a['b[4].c.e.f'] not in 4..5 RETURN d",
        expected, &ExpressionContextMock::EMPTY);
  }

  // numeric floating range
  {
    irs::numeric_token_stream minTerm;
    minTerm.reset(4.0);
    irs::numeric_token_stream maxTerm;
    maxTerm.reset(5.0);

    irs::Or expected;
    auto& range =
        expected.add<irs::Not>().filter<irs::Or>().add<irs::by_granular_range>();
    range.field(mangleNumeric("a.b.c.e.f"));
    range.include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm);
    range.include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess(
        "FOR d IN collection FILTER d.a.b.c.e.f not in 4.5..5.0 RETURN d",
        expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess(
        "FOR d IN collection FILTER d.a['b'].c.e.f not in 4.5..5.0 RETURN d",
        expected, &ExpressionContextMock::EMPTY);
  }

  // numeric floating range, boost
  {
    irs::numeric_token_stream minTerm;
    minTerm.reset(4.0);
    irs::numeric_token_stream maxTerm;
    maxTerm.reset(5.0);

    irs::Or expected;
    auto& range =
        expected.add<irs::Not>().filter<irs::Or>().add<irs::by_granular_range>();
    range.boost(1.5);
    range.field(mangleNumeric("a.b.c.e.f"));
    range.include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm);
    range.include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess(
        "FOR d IN collection FILTER boost(d.a.b.c.e.f not in 4.5..5.0, 1.5) "
        "RETURN d",
        expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess(
        "FOR d IN collection FILTER boost(d.a['b'].c.e.f not in 4.5..5.0, 1.5) "
        "RETURN d",
        expected, &ExpressionContextMock::EMPTY);
  }

  // numeric floating range, attribute offset
  {
    irs::numeric_token_stream minTerm;
    minTerm.reset(4.0);
    irs::numeric_token_stream maxTerm;
    maxTerm.reset(5.0);

    irs::Or expected;
    auto& range =
        expected.add<irs::Not>().filter<irs::Or>().add<irs::by_granular_range>();
    range.field(mangleNumeric("a[3].b[1].c.e.f"));
    range.include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm);
    range.include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess(
        "FOR d IN collection FILTER d.a[3].b[1].c.e.f not in 4.5..5.0 RETURN d",
        expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess(
        "FOR d IN collection FILTER d.a[3]['b'][1].c.e.f not in 4.5..5.0 "
        "RETURN d",
        expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess(
        "FOR d IN collection FILTER analyzer(d.a[3]['b'][1].c.e.f not in "
        "4.5..5.0, 'test_analyzer') RETURN d",
        expected, &ExpressionContextMock::EMPTY);
  }

  // numeric int-float range
  {
    irs::numeric_token_stream minTerm;
    minTerm.reset(4.0);
    irs::numeric_token_stream maxTerm;
    maxTerm.reset(5.0);

    irs::Or expected;
    auto& range =
        expected.add<irs::Not>().filter<irs::Or>().add<irs::by_granular_range>();
    range.field(mangleNumeric("a.b.c.e.f"));
    range.include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm);
    range.include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess(
        "FOR d IN collection FILTER d.a.b.c.e.f not in 4..5.0 RETURN d",
        expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess(
        "FOR d IN collection FILTER d.a.b.c['e'].f not in 4..5.0 RETURN d",
        expected, &ExpressionContextMock::EMPTY);
  }

  // numeric expression in range
  {
    arangodb::aql::Variable var("c", 0);
    arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt(2));
    arangodb::aql::AqlValueGuard guard(value, true);

    ExpressionContextMock ctx;
    ctx.vars.emplace(var.name, value);

    irs::numeric_token_stream minTerm;
    minTerm.reset(2.0);
    irs::numeric_token_stream maxTerm;
    maxTerm.reset(102.0);

    irs::Or expected;
    auto& range =
        expected.add<irs::Not>().filter<irs::Or>().add<irs::by_granular_range>();
    range.field(mangleNumeric("a[100].b.c[1].e.f"));
    range.include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm);
    range.include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess(
        "LET c=2 FOR d IN collection FILTER d.a[100].b.c[1].e.f not in "
        "c..c+100 RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        "LET c=2 FOR d IN collection FILTER d.a[100].b.c[1].e.f not in "
        "c..c+100 LIMIT 100 RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        "LET c=2 FOR d IN collection FILTER d.a[100]['b'].c[1].e.f not in "
        "c..c+100 RETURN d",
        expected, &ctx);
  }

  // dynamic complex attribute name in range
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                                      arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                                      arangodb::aql::AqlValueHintDouble{5.6})));

    irs::numeric_token_stream minTerm;
    minTerm.reset(2.0);
    irs::numeric_token_stream maxTerm;
    maxTerm.reset(102.0);

    irs::Or expected;
    auto& range =
        expected.add<irs::Not>().filter<irs::Or>().add<irs::by_granular_range>();
    range.field(mangleNumeric("a.b.c.e[4].f[5].g[3].g.a"));
    range.include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm);
    range.include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess(
        "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "not in 2..102 RETURN d",
        expected, &ctx);
  }

  // invalid dynamic attribute name
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                                      arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail(
        "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "not in 2..102 RETURN d",
        &ctx);
  }

  // invalid dynamic attribute name (null value)
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintNull{}));  // invalid value type
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                                      arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                                      arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail(
        "LET a=null LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "not in 2..102 RETURN d",
        &ctx);
  }

  // invalid dynamic attribute name (bool value)
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintBool{false}));  // invalid value type
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                                      arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                                      arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail(
        "LET a=false LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "not in 2..102 RETURN d",
        &ctx);
  }

  // string range
  {
    irs::numeric_token_stream minTerm;
    minTerm.reset(4.0);
    irs::numeric_token_stream maxTerm;
    maxTerm.reset(5.0);
    irs::Or expected;
    auto& range =
        expected.add<irs::Not>().filter<irs::Or>().add<irs::by_granular_range>();
    range.field(mangleNumeric("a.b.c.e.f"));
    range.include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm);
    range.include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess(
        "FOR d IN collection FILTER d.a.b.c.e.f not in '4'..'5' RETURN d",
        expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess(
        "FOR d IN collection FILTER d.a['b'].c.e.f not in '4'..'5' RETURN d",
        expected, &ExpressionContextMock::EMPTY);
  }

  // string range, boost
  {
    irs::numeric_token_stream minTerm;
    minTerm.reset(4.0);
    irs::numeric_token_stream maxTerm;
    maxTerm.reset(5.0);
    irs::Or expected;
    auto& range =
        expected.add<irs::Not>().filter<irs::Or>().add<irs::by_granular_range>();
    range.boost(2.5);
    range.field(mangleNumeric("a.b.c.e.f"));
    range.include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm);
    range.include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess(
        "FOR d IN collection FILTER boost(d.a.b.c.e.f not in '4'..'5', 2.5) "
        "RETURN d",
        expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess(
        "FOR d IN collection FILTER boost(d.a['b'].c.e.f not in '4'..'5', 2.5) "
        "RETURN d",
        expected, &ExpressionContextMock::EMPTY);
  }

  // string range, attribute offset
  {
    irs::numeric_token_stream minTerm;
    minTerm.reset(4.0);
    irs::numeric_token_stream maxTerm;
    maxTerm.reset(5.0);
    irs::Or expected;
    auto& range =
        expected.add<irs::Not>().filter<irs::Or>().add<irs::by_granular_range>();
    range.field(mangleNumeric("a.b[3].c.e.f"));
    range.include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm);
    range.include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess(
        "FOR d IN collection FILTER d.a.b[3].c.e.f not in '4'..'5' RETURN d",
        expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess(
        "FOR d IN collection FILTER d.a['b'][3].c.e.f not in '4'..'5' RETURN d",
        expected, &ExpressionContextMock::EMPTY);
  }

  // string expression in range
  {
    arangodb::aql::Variable var("c", 0);
    arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt(2));
    arangodb::aql::AqlValueGuard guard(value, true);

    ExpressionContextMock ctx;
    ctx.vars.emplace(var.name, value);

    irs::numeric_token_stream minTerm;
    minTerm.reset(2.0);
    irs::numeric_token_stream maxTerm;
    maxTerm.reset(4.0);
    irs::Or expected;
    auto& range =
        expected.add<irs::Not>().filter<irs::Or>().add<irs::by_granular_range>();
    range.field(mangleNumeric("a[100].b.c[1].e.f"));
    range.include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm);
    range.include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess(
        "LET c=2 FOR d IN collection FILTER d.a[100].b.c[1].e.f not in "
        "TO_STRING(c)..TO_STRING(c+2) RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        "LET c=2 FOR d IN collection FILTER d.a[100].b.c[1]['e'].f not in "
        "TO_STRING(c)..TO_STRING(c+2) RETURN d",
        expected, &ctx);
  }

  // dynamic complex attribute name in range
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                                      arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                                      arangodb::aql::AqlValueHintDouble{5.6})));

    irs::numeric_token_stream minTerm;
    minTerm.reset(2.0);
    irs::numeric_token_stream maxTerm;
    maxTerm.reset(4.0);
    irs::Or expected;
    auto& range =
        expected.add<irs::Not>().filter<irs::Or>().add<irs::by_granular_range>();
    range.field(mangleNumeric("a.b.c.e[4].f[5].g[3].g.a"));
    range.include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm);
    range.include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess(
        "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "not in '2'..'4' RETURN d",
        expected, &ctx);
  }

  // invalid dynamic attribute name in range
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                                      arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail(
        "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "not in '2'..'4' RETURN d",
        &ctx);
  }

  // invalid dynamic attribute name in range (null value)
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintNull{}));  // invalid value type
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                                      arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                                      arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail(
        "LET a=null LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "not in '2'..'4' RETURN d",
        &ctx);
  }

  // invalid dynamic attribute name in range (bool value)
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintBool{false}));  // invalid value type
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                                      arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                                      arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail(
        "LET a=false LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "not in '2'..'4' RETURN d",
        &ctx);
  }

  // boolean range
  {
    irs::numeric_token_stream minTerm;
    minTerm.reset(0.0);
    irs::numeric_token_stream maxTerm;
    maxTerm.reset(1.0);
    irs::Or expected;
    auto& range =
        expected.add<irs::Not>().filter<irs::Or>().add<irs::by_granular_range>();
    range.field(mangleNumeric("a.b.c.e.f"));
    range.include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm);
    range.include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess(
        "FOR d IN collection FILTER d.a.b.c.e.f not in false..true RETURN d",
        expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess(
        "FOR d IN collection FILTER d['a'].b.c.e.f not in false..true RETURN d",
        expected, &ExpressionContextMock::EMPTY);
  }

  // boolean range, attribute offset
  {
    irs::numeric_token_stream minTerm;
    minTerm.reset(0.0);
    irs::numeric_token_stream maxTerm;
    maxTerm.reset(1.0);
    irs::Or expected;
    auto& range =
        expected.add<irs::Not>().filter<irs::Or>().add<irs::by_granular_range>();
    range.field(mangleNumeric("a.b.c.e.f[1]"));
    range.include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm);
    range.include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess(
        "FOR d IN collection FILTER d.a.b.c.e.f[1] not in false..true RETURN d",
        expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess(
        "FOR d IN collection FILTER d['a'].b.c.e.f[1] not in false..true "
        "RETURN d",
        expected, &ExpressionContextMock::EMPTY);
  }

  // boolean expression in range
  {
    arangodb::aql::Variable var("c", 0);
    arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt(2));
    arangodb::aql::AqlValueGuard guard(value, true);

    ExpressionContextMock ctx;
    ctx.vars.emplace(var.name, value);

    irs::numeric_token_stream minTerm;
    minTerm.reset(1.0);
    irs::numeric_token_stream maxTerm;
    maxTerm.reset(0.0);
    irs::Or expected;
    auto& range =
        expected.add<irs::Not>().filter<irs::Or>().add<irs::by_granular_range>();
    range.field(mangleNumeric("a[100].b.c[1].e.f"));
    range.include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm);
    range.include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess(
        "LET c=2 FOR d IN collection FILTER d.a[100].b.c[1].e.f not in "
        "TO_BOOL(c)..IS_NULL(TO_BOOL(c-2)) RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        "LET c=2 FOR d IN collection FILTER d.a[100].b.c[1]['e'].f not in "
        "TO_BOOL(c)..TO_BOOL(c-2) RETURN d",
        expected, &ctx);
  }

  // dynamic complex attribute name in range
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                                      arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                                      arangodb::aql::AqlValueHintDouble{5.6})));

    irs::numeric_token_stream minTerm;
    minTerm.reset(1.0);
    irs::numeric_token_stream maxTerm;
    maxTerm.reset(0.0);
    irs::Or expected;
    auto& range =
        expected.add<irs::Not>().filter<irs::Or>().add<irs::by_granular_range>();
    range.field(mangleNumeric("a.b.c.e[4].f[5].g[3].g.a"));
    range.include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm);
    range.include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess(
        "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "not in true..false RETURN d",
        expected, &ctx);
  }

  // invalid dynamic attribute name in range
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                                      arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail(
        "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "not in true..false RETURN d",
        &ctx);
  }

  // invalid dynamic attribute name in range (null value)
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintNull{}));  // invalid value type
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                                      arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                                      arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail(
        "LET a=null LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "not in true..false RETURN d",
        &ctx);
  }

  // invalid dynamic attribute name in range (bool value)
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintBool{false}));  // invalid value type
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                                      arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                                      arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail(
        "LET a=false LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "not in false..true RETURN d",
        &ctx);
  }

  // null range
  {
    irs::numeric_token_stream minTerm;
    minTerm.reset(0.0);
    irs::numeric_token_stream maxTerm;
    maxTerm.reset(0.0);
    irs::Or expected;
    auto& range =
        expected.add<irs::Not>().filter<irs::Or>().add<irs::by_granular_range>();
    range.field(mangleNumeric("a.b.c.e.f"));
    range.include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm);
    range.include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess(
        "FOR d IN collection FILTER d.a.b.c.e.f not in null..null RETURN d",
        expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess(
        "FOR d IN collection FILTER d.a.b.c['e'].f not in null..null RETURN d",
        expected, &ExpressionContextMock::EMPTY);
  }

  // null range, attribute offset
  {
    irs::numeric_token_stream minTerm;
    minTerm.reset(0.0);
    irs::numeric_token_stream maxTerm;
    maxTerm.reset(0.0);
    irs::Or expected;
    auto& range =
        expected.add<irs::Not>().filter<irs::Or>().add<irs::by_granular_range>();
    range.field(mangleNumeric("a.b.c.e[3].f"));
    range.include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm);
    range.include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess(
        "FOR d IN collection FILTER d.a.b.c.e[3].f not in null..null RETURN d",
        expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess(
        "FOR d IN collection FILTER d.a.b.c['e'][3].f not in null..null RETURN "
        "d",
        expected, &ExpressionContextMock::EMPTY);
  }

  // null expression in range
  {
    arangodb::aql::Variable var("c", 0);
    arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintNull{});
    arangodb::aql::AqlValueGuard guard(value, true);

    ExpressionContextMock ctx;
    ctx.vars.emplace(var.name, value);

    irs::numeric_token_stream minTerm;
    minTerm.reset(0.0);
    irs::numeric_token_stream maxTerm;
    maxTerm.reset(0.0);
    irs::Or expected;
    auto& range =
        expected.add<irs::Not>().filter<irs::Or>().add<irs::by_granular_range>();
    range.field(mangleNumeric("a[100].b.c[1].e.f"));
    range.include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm);
    range.include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess(
        "LET c=null FOR d IN collection FILTER d.a[100].b.c[1].e.f not in "
        "c..null RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        "LET c=null FOR d IN collection FILTER d.a[100].b.c[1]['e'].f not in "
        "c..null RETURN d",
        expected, &ctx);
  }

  // dynamic complex attribute name in range
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                                      arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                                      arangodb::aql::AqlValueHintDouble{5.6})));

    irs::numeric_token_stream minTerm;
    minTerm.reset(0.0);
    irs::numeric_token_stream maxTerm;
    maxTerm.reset(0.0);
    irs::Or expected;
    auto& range =
        expected.add<irs::Not>().filter<irs::Or>().add<irs::by_granular_range>();
    range.field(mangleNumeric("a.b.c.e[4].f[5].g[3].g.a"));
    range.include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm);
    range.include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess(
        "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "not in null..null RETURN d",
        expected, &ctx);
  }

  // invalid dynamic attribute name in range
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                                      arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail(
        "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "not in null..null RETURN d",
        &ctx);
  }

  // invalid dynamic attribute name in range (null value)
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintNull{}));  // invalid value type
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                                      arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                                      arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail(
        "LET a=null LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "not in null..null RETURN d",
        &ctx);
  }

  // invalid dynamic attribute name in range (bool value)
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintBool{false}));  // invalid value type
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                                      arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                                      arangodb::aql::AqlValueHintDouble{5.6})));

    assertFilterExecutionFail(
        "LET a=false LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "not in null..null RETURN d",
        &ctx);
  }

  // heterogeneous range
  {
    irs::numeric_token_stream minTerm;
    minTerm.reset(0.0);
    irs::numeric_token_stream maxTerm;
    maxTerm.reset(4.0);
    irs::Or expected;
    auto& range =
        expected.add<irs::Not>().filter<irs::Or>().add<irs::by_granular_range>();
    range.field(mangleNumeric("a"));
    range.include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm);
    range.include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess("FOR d IN myView FILTER d.a not in 'a'..4 RETURN d",
                        expected, &ExpressionContextMock::EMPTY);
  }

  // heterogeneous range
  {
    irs::numeric_token_stream minTerm;
    minTerm.reset(1.0);
    irs::numeric_token_stream maxTerm;
    maxTerm.reset(0.0);
    irs::Or expected;
    auto& range =
        expected.add<irs::Not>().filter<irs::Or>().add<irs::by_granular_range>();
    range.field(mangleNumeric("a"));
    range.include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm);
    range.include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess("FOR d IN myView FILTER d.a not in 1..null RETURN d",
                        expected, &ExpressionContextMock::EMPTY);
  }

  // heterogeneous range
  {
    irs::numeric_token_stream minTerm;
    minTerm.reset(0.0);
    irs::numeric_token_stream maxTerm;
    maxTerm.reset(5.0);
    irs::Or expected;
    auto& range =
        expected.add<irs::Not>().filter<irs::Or>().add<irs::by_granular_range>();
    range.field(mangleNumeric("a"));
    range.include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm);
    range.include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess("FOR d IN myView FILTER d.a not in false..5.5 RETURN d",
                        expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess("FOR d IN myView FILTER d.a not in 1..4..5 RETURN d",
                        expected, &ExpressionContextMock::EMPTY);
  }

  // heterogeneous range
  {
    irs::numeric_token_stream minTerm;
    minTerm.reset(0.0);
    irs::numeric_token_stream maxTerm;
    maxTerm.reset(1.0);
    irs::Or expected;
    auto& range =
        expected.add<irs::Not>().filter<irs::Or>().add<irs::by_granular_range>();
    range.field(mangleNumeric("a"));
    range.include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm);
    range.include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess("FOR d IN myView FILTER d.a not in 'false'..1 RETURN d",
                        expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess("FOR d IN myView FILTER d.a not in 0..true RETURN d",
                        expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess("FOR d IN myView FILTER d.a not in null..true RETURN d",
                        expected, &ExpressionContextMock::EMPTY);
  }

  // range as reference
  {
    arangodb::aql::AqlValue value(1, 3);
    arangodb::aql::AqlValueGuard guard(value, true);

    irs::numeric_token_stream stream;
    stream.reset(2.);
    EXPECT_TRUE(stream.next());

    ExpressionContextMock ctx;
    ctx.vars.emplace("x", arangodb::aql::AqlValue(1, 3));

    irs::numeric_token_stream minTerm;
    minTerm.reset(1.0);
    irs::numeric_token_stream maxTerm;
    maxTerm.reset(3.0);
    irs::Or expected;
    auto& range =
        expected.add<irs::Not>().filter<irs::Or>().add<irs::by_granular_range>();
    range.field(mangleNumeric("a.b.c.e.f"));
    range.include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>(minTerm);
    range.include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>(maxTerm);

    assertFilterSuccess(
        "LET x=1..3 FOR d IN collection FILTER d.a.b.c.e.f not in x RETURN d",
        expected, &ctx);
  }

  // non-deterministic expression name in range
  assertExpressionFilter(
      "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
      "collection FILTER "
      "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_NONDETERM_('a')] "
      "not in 4..5 RETURN d");
  assertExpressionFilter(
      "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
      "collection FILTER "
      "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
      "not in _NONDETERM_(4)..5 RETURN d");

  // self-reference
  assertExpressionFilter("FOR d IN myView FILTER d not in 4..5 RETURN d");
  assertExpressionFilter("for d IN myView FILTER d[*] not in 4..5 RETURN d");
  assertExpressionFilter("for d IN myView FILTER d.a[*] not in 4..5 RETURN d");
  assertExpressionFilter("FOR d IN myView FILTER d.a not in d.b..5 RETURN d");
  assertExpressionFilter("FOR d IN myView FILTER 4..5 not in d.a RETURN d");
  assertExpressionFilter(
      "FOR d IN myView FILTER [1,2,'3'] not in d.a RETURN d");
  assertExpressionFilter("FOR d IN myView FILTER 4 not in d.a RETURN d");
  assertFilterExecutionFail(
      "LET x={} FOR d IN myView FILTER 4..5 not in x.a RETURN d",
      &ExpressionContextMock::EMPTY);  // no reference to x
  assertFilterExecutionFail(
      "LET x={} FOR d IN myView FILTER 4 in not x.a RETURN d",
      &ExpressionContextMock::EMPTY);  // no reference to x
  assertExpressionFilter("for d IN myView filter 4..5 not in d.a return d");  // self-reference
  assertExpressionFilter("FOR d IN myView FILTER 4 not in d.b..5 RETURN d");  // self-reference

  // true expression
  {
    irs::Or expected;
    expected.add<irs::all>();

    assertFilterSuccess("FOR d IN myView FILTER [] not in 4..5 RETURN d",
                        expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess("FOR d IN myView FILTER ['d'] not in 4..5 RETURN d",
                        expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess("FOR d IN myView FILTER 'd.a' not in 4..5 RETURN d",
                        expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess("FOR d IN myView FILTER null not in 4..5 RETURN d",
                        expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess("FOR d IN myView FILTER true not in 4..5 RETURN d",
                        expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess("FOR d IN myView FILTER false not in 4..5 RETURN d",
                        expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess("FOR d IN myView FILTER 4.3 not in 4..5 RETURN d", expected,
                        &ExpressionContextMock::EMPTY);  // ArangoDB feature
  }

  // false expression
  {
    irs::Or expected;
    expected.add<irs::empty>();

    assertFilterSuccess("FOR d IN myView FILTER 4 not in 4..5 RETURN d",
                        expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess("FOR d IN myView FILTER 4 not in 4..4+1 RETURN d",
                        expected, &ExpressionContextMock::EMPTY);
  }
}
