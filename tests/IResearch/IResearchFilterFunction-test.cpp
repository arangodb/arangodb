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
#include "search/wildcard_filter.hpp"
#include "search/levenshtein_filter.hpp"
#include "search/ngram_similarity_filter.hpp"

#include "IResearch/ExpressionContextMock.h"
#include "IResearch/IResearchPDP.h"
#include "IResearch/common.h"
#include "Mocks/LogLevels.h"
#include "Mocks/Servers.h"
#include "Mocks/StorageEngineMock.h"

#include "Aql/AqlFunctionFeature.h"
#include "Aql/Ast.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/ExpressionContext.h"
#include "Aql/Query.h"
#include "Aql/OptimizerRulesFeature.h"
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
#include "RestServer/ViewTypesFeature.h"
#include "Sharding/ShardingFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "Transaction/Methods.h"
#include "Transaction/StandaloneContext.h"
#include "V8Server/V8DealerFeature.h"
#include "VocBase/Methods/Collections.h"

#if USE_ENTERPRISE
#include "Enterprise/Ldap/LdapFeature.h"
#endif

static const VPackBuilder systemDatabaseBuilder = dbArgsBuilder();
static const VPackSlice   systemDatabaseArgs = systemDatabaseBuilder.slice();
// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

class IResearchFilterFunctionTest
    : public ::testing::Test,
      public arangodb::tests::LogSuppressor<arangodb::Logger::AUTHENTICATION, arangodb::LogLevel::ERR> {
 protected:
  arangodb::tests::mocks::MockAqlServer server;

 private:
  TRI_vocbase_t* _vocbase;

 protected:
  IResearchFilterFunctionTest() {
    arangodb::tests::init();

    auto& functions = server.getFeature<arangodb::aql::AqlFunctionFeature>();

    // register fake non-deterministic function in order to suppress optimizations
    functions.add(arangodb::aql::Function{
        "_NONDETERM_", ".",
        arangodb::aql::Function::makeFlags(
            // fake non-deterministic
            arangodb::aql::Function::Flags::CanRunOnDBServerCluster,
            arangodb::aql::Function::Flags::CanRunOnDBServerOneShard),
        [](arangodb::aql::ExpressionContext*, arangodb::aql::AstNode const&,
           arangodb::aql::VPackFunctionParameters const& params) {
          TRI_ASSERT(!params.empty());
          return params[0];
        }});

    // register fake non-deterministic function in order to suppress optimizations
    functions.add(arangodb::aql::Function{
        "_FORWARD_", ".",
        arangodb::aql::Function::makeFlags(
            // fake deterministic
            arangodb::aql::Function::Flags::Deterministic, arangodb::aql::Function::Flags::Cacheable,
            arangodb::aql::Function::Flags::CanRunOnDBServerCluster,
            arangodb::aql::Function::Flags::CanRunOnDBServerOneShard),
        [](arangodb::aql::ExpressionContext*, arangodb::aql::AstNode const&,
           arangodb::aql::VPackFunctionParameters const& params) {
          TRI_ASSERT(!params.empty());
          return params[0];
        }});

    auto& analyzers = server.getFeature<arangodb::iresearch::IResearchAnalyzerFeature>();
    arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;

    auto& dbFeature = server.getFeature<arangodb::DatabaseFeature>();
    dbFeature.createDatabase(testDBInfo(server.server()), _vocbase);  // required for IResearchAnalyzerFeature::emplace(...)
    std::shared_ptr<arangodb::LogicalCollection> unused;
    arangodb::OperationOptions options(arangodb::ExecContext::current());
    arangodb::methods::Collections::createSystem(*_vocbase, options,
                                                 arangodb::tests::AnalyzerCollectionName,
                                                 false, unused);
    analyzers.emplace(
        result, "testVocbase::test_analyzer", "TestAnalyzer",
        arangodb::velocypack::Parser::fromJson("{ \"args\": \"abc\"}")->slice());  // cache analyzer
  }

  TRI_vocbase_t& vocbase() { return *_vocbase; }
};  // IResearchFilterSetup

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

TEST_F(IResearchFilterFunctionTest, AttributeAccess) {
  // attribute access, non empty object
  {
    auto obj =
        VPackParser::fromJson("{ \"a\": { \"b\": \"1\" } }");

    ExpressionContextMock ctx;
    ctx.vars.emplace("x", arangodb::aql::AqlValue(obj->slice()));

    irs::Or expected;
    expected.add<irs::all>();

    assertFilterSuccess(vocbase(),
                        "LET x={} FOR d IN collection FILTER x.a.b RETURN d",
                        expected, &ctx);
  }

  // attribute access, non empty object, boost
  {
    auto obj =
        VPackParser::fromJson("{ \"a\": { \"b\": \"1\" } }");

    ExpressionContextMock ctx;
    ctx.vars.emplace("x", arangodb::aql::AqlValue(obj->slice()));

    irs::Or expected;
    expected.add<irs::all>().boost(1.5f);

    assertFilterSuccess(
        vocbase(),
        "LET x={} FOR d IN collection FILTER BOOST(x.a.b, 1.5) RETURN d", expected, &ctx);
  }

  // attribute access, empty object
  {
    auto obj = VPackParser::fromJson("{}");

    ExpressionContextMock ctx;
    ctx.vars.emplace("x", arangodb::aql::AqlValue(obj->slice()));

    irs::Or expected;
    expected.add<irs::empty>();

    assertFilterSuccess(vocbase(),
                        "LET x={} FOR d IN collection FILTER x.a.b RETURN d",
                        expected, &ctx);
  }

  // attribute access, empty object, boost
  {
    auto obj = VPackParser::fromJson("{}");

    ExpressionContextMock ctx;
    ctx.vars.emplace("x", arangodb::aql::AqlValue(obj->slice()));

    irs::Or expected;
    expected.add<irs::empty>();

    assertFilterSuccess(
        vocbase(),
        "LET x={} FOR d IN collection FILTER BOOST(x.a.b, 2.5) RETURN d", expected, &ctx);
  }

  assertExpressionFilter(vocbase(), "FOR d IN collection FILTER d RETURN d");  // no reference to `d`
  assertExpressionFilter(
      vocbase(),
      "FOR d IN collection FILTER ANALYZER(d, 'test_analyzer') RETURN d", 1,
      wrappedExpressionExtractor);  // no reference to `d`
  assertExpressionFilter(vocbase(),
                         "FOR d IN collection FILTER BOOST(d, 1.5) RETURN d", 1.5,
                         wrappedExpressionExtractor);  // no reference to `d`
  assertExpressionFilter(vocbase(),
                         "FOR d IN collection FILTER d.a.b.c RETURN d");  // no reference to `d`
  assertExpressionFilter(vocbase(),
                         "FOR d IN collection FILTER d.a.b.c RETURN d");  // no reference to `d`
  assertExpressionFilter(
      vocbase(), "FOR d IN collection FILTER BOOST(d.a.b.c, 2.5) RETURN d", 2.5,
      wrappedExpressionExtractor);  // no reference to `d`
  assertExpressionFilter(
      vocbase(),
      "FOR d IN collection FILTER ANALYZER(d.a.b[TO_STRING('c')], "
      "'test_analyzer') RETURN d",
      1, wrappedExpressionExtractor);  // no reference to `d`
  assertExpressionFilter(
      vocbase(),
      "FOR d IN collection FILTER BOOST(d.a.b[TO_STRING('c')], 3.5) RETURN d",
      3.5, wrappedExpressionExtractor);  // no reference to `d`

  // nondeterministic expression -> wrap it
  assertExpressionFilter(
      vocbase(), "FOR d IN collection FILTER d.a.b[_NONDETERM_('c')] RETURN d");
  assertExpressionFilter(
      vocbase(),
      "FOR d IN collection FILTER ANALYZER(d.a.b[_NONDETERM_('c')], "
      "'test_analyzer') RETURN d",
      1.0, wrappedExpressionExtractor);
  assertExpressionFilter(
      vocbase(),
      "FOR d IN collection FILTER BOOST(d.a.b[_NONDETERM_('c')], 1.5) RETURN d",
      1.5, wrappedExpressionExtractor);
}

TEST_F(IResearchFilterFunctionTest, ValueReference) {
  // string value == true
  {
    irs::Or expected;
    expected.add<irs::all>();

    assertFilterSuccess(vocbase(), "FOR d IN collection FILTER '1' RETURN d", expected);
  }

  // string reference
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("x",
                     arangodb::aql::AqlValue(arangodb::aql::AqlValue("abc")));

    irs::Or expected;
    expected.add<irs::all>();

    assertFilterSuccess(vocbase(),
                        "LET x='abc' FOR d IN collection FILTER x RETURN d",
                        expected, &ctx);  // reference
  }

  // string empty value == false
  {
    irs::Or expected;
    expected.add<irs::empty>();

    assertFilterSuccess(vocbase(), "FOR d IN collection FILTER '' RETURN d", expected);
  }

  // empty string reference false
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("x", arangodb::aql::AqlValue(arangodb::aql::AqlValue("")));

    irs::Or expected;
    expected.add<irs::empty>();

    assertFilterSuccess(vocbase(),
                        "LET x='' FOR d IN collection FILTER x RETURN d",
                        expected, &ctx);  // reference
  }

  // true value
  {
    irs::Or expected;
    expected.add<irs::all>();

    assertFilterSuccess(vocbase(), "FOR d IN collection FILTER true RETURN d", expected);
  }

  // boolean reference
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("x", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                              arangodb::aql::AqlValueHintBool{true})));

    irs::Or expected;
    expected.add<irs::all>();

    assertFilterSuccess(vocbase(),
                        "LET x=true FOR d IN collection FILTER x RETURN d",
                        expected, &ctx);  // reference
  }

  // false
  {
    irs::Or expected;
    expected.add<irs::empty>();

    assertFilterSuccess(vocbase(), "FOR d IN collection FILTER false RETURN d", expected);
  }

  // boolean reference
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("x", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                              arangodb::aql::AqlValueHintBool{false})));

    irs::Or expected;
    expected.add<irs::empty>();

    assertFilterSuccess(vocbase(),
                        "LET x=false FOR d IN collection FILTER x RETURN d",
                        expected, &ctx);  // reference
  }

  // null == value
  {
    irs::Or expected;
    expected.add<irs::empty>();

    assertFilterSuccess(vocbase(), "FOR d IN collection FILTER null RETURN d", expected);
  }

  // non zero numeric value
  {
    irs::Or expected;
    expected.add<irs::all>();

    assertFilterSuccess(vocbase(), "FOR d IN collection FILTER 1 RETURN d", expected);
  }

  // non zero numeric reference
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("x", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                              arangodb::aql::AqlValueHintInt{1})));

    irs::Or expected;
    expected.add<irs::all>();

    assertFilterSuccess(vocbase(),
                        "LET x=1 FOR d IN collection FILTER x RETURN d",
                        expected, &ctx);  // reference
  }

  // zero numeric value
  {
    irs::Or expected;
    expected.add<irs::empty>();

    assertFilterSuccess(vocbase(), "FOR d IN collection FILTER 0 RETURN d", expected);
  }

  // zero numeric reference
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("x", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                              arangodb::aql::AqlValueHintInt{0})));

    irs::Or expected;
    expected.add<irs::empty>();

    assertFilterSuccess(vocbase(),
                        "LET x=0 FOR d IN collection FILTER x RETURN d",
                        expected, &ctx);  // reference
  }

  // zero floating value
  {
    irs::Or expected;
    expected.add<irs::empty>();

    assertFilterSuccess(vocbase(), "FOR d IN collection FILTER 0.0 RETURN d", expected);
  }

  // zero floating reference
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("x", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                              arangodb::aql::AqlValueHintDouble{0.0})));

    irs::Or expected;
    expected.add<irs::empty>();

    assertFilterSuccess(vocbase(),
                        "LET x=0.0 FOR d IN collection FILTER x RETURN d",
                        expected, &ctx);  // reference
  }

  // non zero floating value
  {
    irs::Or expected;
    expected.add<irs::all>();

    assertFilterSuccess(vocbase(), "FOR d IN collection FILTER 0.1 RETURN d", expected);
  }

  // non zero floating reference
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("x", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                              arangodb::aql::AqlValueHintDouble{0.1})));

    irs::Or expected;
    expected.add<irs::all>();

    assertFilterSuccess(vocbase(),
                        "LET x=0.1 FOR d IN collection FILTER x RETURN d",
                        expected, &ctx);  // reference
  }

  // Array == true
  {
    irs::Or expected;
    expected.add<irs::all>();

    assertFilterSuccess(vocbase(), "FOR d IN collection FILTER [] RETURN d", expected);
  }

  // Array reference
  {
    auto obj = VPackParser::fromJson("[]");

    ExpressionContextMock ctx;
    ctx.vars.emplace("x", arangodb::aql::AqlValue(arangodb::aql::AqlValue(obj->slice())));

    irs::Or expected;
    expected.add<irs::all>();

    assertFilterSuccess(vocbase(),
                        "LET x=[] FOR d IN collection FILTER x RETURN d",
                        expected, &ctx);  // reference
  }

  // Range == true
  {
    irs::Or expected;
    expected.add<irs::all>();

    assertFilterSuccess(vocbase(), "FOR d IN collection FILTER 1..2 RETURN d", expected);
  }

  // Range reference
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("x", arangodb::aql::AqlValue(arangodb::aql::AqlValue(1, 1)));

    irs::Or expected;
    expected.add<irs::all>();

    assertFilterSuccess(vocbase(),
                        "LET x=1..1 FOR d IN collection FILTER x RETURN d",
                        expected, &ctx);  // reference
  }

  // Object == true
  {
    irs::Or expected;
    expected.add<irs::all>();

    assertFilterSuccess(vocbase(), "FOR d IN collection FILTER {} RETURN d", expected);
  }

  // Object reference
  {
    auto obj = VPackParser::fromJson("{}");

    ExpressionContextMock ctx;
    ctx.vars.emplace("x", arangodb::aql::AqlValue(arangodb::aql::AqlValue(obj->slice())));

    irs::Or expected;
    expected.add<irs::all>();

    assertFilterSuccess(vocbase(),
                        "LET x={} FOR d IN collection FILTER x RETURN d",
                        expected, &ctx);  // reference
  }

  // numeric expression
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("numVal", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt(2)));

    irs::Or expected;
    expected.add<irs::empty>();

    assertFilterSuccess(
        vocbase(), "LET numVal=2 FOR d IN collection FILTER numVal-2 RETURN d",
        expected, &ctx);
  }

  // boolean expression
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("numVal", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt(2)));

    irs::Or expected;
    expected.add<irs::empty>();

    assertFilterSuccess(
        vocbase(),
        "LET numVal=2 FOR d IN collection FILTER ((numVal+1) < 2) RETURN d",
        expected, &ctx);
  }

  // null expression
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("nullVal", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintNull{}));

    irs::Or expected;
    auto& root = expected.add<irs::And>();
    root.add<irs::empty>();
    root.add<irs::all>();

    assertFilterSuccess(
        vocbase(),
        "LET nullVal=null FOR d IN collection FILTER (nullVal && true) RETURN "
        "d",
        expected, &ctx);
  }

  // string value == true, boosted
  {
    irs::Or expected;
    expected.add<irs::all>().boost(2.5);

    assertFilterSuccess(vocbase(),
                        "FOR d IN collection FILTER BOOST('1', 2.5) RETURN d", expected);
  }

  // string value == true, analyzer
  {
    irs::Or expected;
    expected.add<irs::all>();

    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER ANALYZER('1', 'test_analyzer') RETURN d", expected);
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

    assertFilterSuccess(
        vocbase(),
        "LET nullVal=null FOR d IN collection FILTER BOOST(nullVal && true, "
        "0.75) RETURN d",
        expected, &ctx);
  }

  // self-reference
  assertExpressionFilter(vocbase(), "FOR d IN collection FILTER d RETURN d");
  assertExpressionFilter(vocbase(), "FOR d IN collection FILTER d[1] RETURN d");
  assertExpressionFilter(vocbase(),
                         "FOR d IN collection FILTER BOOST(d[1], 1.5) RETURN d",
                         1.5, wrappedExpressionExtractor);
  assertExpressionFilter(
      vocbase(),
      "FOR d IN collection FILTER ANALYZER(d[1], 'test_analyzer') RETURN d",
      1.0, wrappedExpressionExtractor);
  assertExpressionFilter(vocbase(),
                         "FOR d IN collection FILTER d.a[1] RETURN d");
  assertExpressionFilter(vocbase(), "FOR d IN collection FILTER d[*] RETURN d");
  assertExpressionFilter(vocbase(),
                         "FOR d IN collection FILTER BOOST(d[*], 0.5) RETURN d",
                         0.5, wrappedExpressionExtractor);
  assertExpressionFilter(vocbase(),
                         "FOR d IN collection FILTER d.a[*] RETURN d");
}

TEST_F(IResearchFilterFunctionTest, SystemFunctions) {
  // scalar
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("x", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                              arangodb::aql::AqlValueHintInt{1})));

    irs::Or expected;
    expected.add<irs::all>();

    assertFilterSuccess(
        vocbase(), "LET x=1 FOR d IN collection FILTER TO_STRING(x) RETURN d",
        expected, &ctx);  // reference
  }

  // scalar
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("x", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                              arangodb::aql::AqlValueHintInt{0})));

    irs::Or expected;
    expected.add<irs::empty>();

    assertFilterSuccess(
        vocbase(), "LET x=0 FOR d IN collection FILTER TO_BOOL(x) RETURN d",
        expected, &ctx);  // reference
  }

  // scalar with boost
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("x", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                              arangodb::aql::AqlValueHintInt{1})));

    irs::Or expected;
    expected.add<irs::all>().boost(1.5f);

    assertFilterSuccess(
        vocbase(),
        "LET x=1 FOR d IN collection FILTER BOOST(TO_STRING(x), 1.5) RETURN d",
        expected, &ctx);  // reference
  }

  // scalar with boost
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("x", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                              arangodb::aql::AqlValueHintInt{0})));

    irs::Or expected;
    expected.add<irs::empty>();

    assertFilterSuccess(
        vocbase(),
        "LET x=0 FOR d IN collection FILTER BOOST(TO_BOOL(x), 1.5) RETURN d",
        expected, &ctx);  // reference
  }

  // nondeterministic expression : wrap it
  assertExpressionFilter(vocbase(), "FOR d IN myView FILTER RAND() RETURN d");
  assertExpressionFilter(vocbase(),
                         "FOR d IN myView FILTER BOOST(RAND(), 1.5) RETURN d",
                         1.5, wrappedExpressionExtractor);
  assertExpressionFilter(
      vocbase(),
      "FOR d IN myView FILTER ANALYZER(RAND(), 'test_analyzer') RETURN d", 1.0,
      wrappedExpressionExtractor);
}

TEST_F(IResearchFilterFunctionTest, UnsupportedUserFunctions) {
  //  FIXME need V8 context up and running to execute user functions
  //  assertFilterFail(vocbase(),"FOR d IN myView FILTER ir::unknownFunction() RETURN d", &ExpressionContextMock::EMPTY);
  //  assertFilterFail(vocbase(),"FOR d IN myView FILTER ir::unknownFunction1(d) RETURN d", &ExpressionContextMock::EMPTY);
  //  assertFilterFail(vocbase(),"FOR d IN myView FILTER ir::unknownFunction2(d, 'quick') RETURN d", &ExpressionContextMock::EMPTY);
}

TEST_F(IResearchFilterFunctionTest, Boost) {
  // simple boost
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("x", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                              arangodb::aql::AqlValueHintDouble{1.5})));

    irs::Or expected;
    auto& termFilter = expected.add<irs::by_term>();
    *termFilter.mutable_field() = mangleStringIdentity("foo");
    termFilter.boost(1.5);
    irs::assign(termFilter.mutable_options()->term,
                irs::ref_cast<irs::byte_type>(irs::string_ref("abc")));

    assertFilterSuccess(
        vocbase(),
        "LET x=1.5 FOR d IN collection FILTER BOOST(d.foo == 'abc', x) RETURN "
        "d",
        expected, &ctx);
  }

  // embedded boost
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("x", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                              arangodb::aql::AqlValueHintDouble{1.5})));

    irs::Or expected;
    auto& termFilter = expected.add<irs::by_term>();
    *termFilter.mutable_field() = mangleStringIdentity("foo");
    termFilter.boost(6.0f); // 1.5*4 or 1.5*2*2
    irs::assign(termFilter.mutable_options()->term,
                irs::ref_cast<irs::byte_type>(irs::string_ref("abc")));

    assertFilterSuccess(
        vocbase(),
        "LET x=1.5 FOR d IN collection FILTER BOOST(BOOST(d.foo == 'abc', x), "
        "4) RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET x=1.5 FOR d IN collection FILTER BOOST(BOOST(BOOST(d.foo == "
        "'abc', x), 2), 2) RETURN d",
        expected, &ctx);
  }

  // wrong number of arguments
  assertFilterParseFail(
      vocbase(), "FOR d IN collection FILTER BOOST(d.foo == 'abc') RETURN d");

  // wrong argument type
  assertFilterFail(
      vocbase(),
      "FOR d IN collection FILTER BOOST(d.foo == 'abc', '2') RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN collection FILTER BOOST(d.foo == 'abc', null) RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN collection FILTER BOOST(d.foo == 'abc', true) RETURN d");

  // non-deterministic expression
  assertFilterFail(
      vocbase(),
      "FOR d IN collection FILTER BOOST(d.foo == 'abc', RAND()) RETURN d");

  // can't execute boost function
  assertFilterExecutionFail(
      vocbase(),
      "LET x=1.5 FOR d IN collection FILTER BOOST(d.foo == 'abc', BOOST(x, 2)) "
      "RETURN d",
      &ExpressionContextMock::EMPTY);
}

TEST_F(IResearchFilterFunctionTest, Analyzer) {
  // simple analyzer
  {
    irs::Or expected;
    auto& termFilter = expected.add<irs::by_term>();
    *termFilter.mutable_field() = mangleString("foo", "test_analyzer");
    irs::assign(termFilter.mutable_options()->term,
                irs::ref_cast<irs::byte_type>(irs::string_ref("bar")));

    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER ANALYZER(d.foo == 'bar', 'test_analyzer') "
        "RETURN d",
        expected);
  }

  // overriden analyzer
  {
    irs::Or expected;
    auto& termFilter = expected.add<irs::by_term>();
    *termFilter.mutable_field() = mangleStringIdentity("foo");
    irs::assign(termFilter.mutable_options()->term,
                irs::ref_cast<irs::byte_type>(irs::string_ref("bar")));

    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER ANALYZER(ANALYZER(d.foo == 'bar', "
        "'identity'), 'test_analyzer') RETURN d",
        expected);
  }

  // expression as the parameter
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("x",
                     arangodb::aql::AqlValue(arangodb::aql::AqlValue("test_")));

    irs::Or expected;
    auto& termFilter = expected.add<irs::by_term>();
    *termFilter.mutable_field() = mangleString("foo", "test_analyzer");
    irs::assign(termFilter.mutable_options()->term,
                irs::ref_cast<irs::byte_type>(irs::string_ref("bar")));

    assertFilterSuccess(
        vocbase(),
        "LET x='test_' FOR d IN collection FILTER ANALYZER(d.foo == 'bar', "
        "CONCAT(x, 'analyzer')) RETURN d",
        expected, &ctx);
  }

  // wrong number of arguments
  assertFilterParseFail(
      vocbase(),
      "FOR d IN collection FILTER ANALYZER(d.foo == 'bar') RETURN d");

  // wrong argument type
  assertFilterFail(
      vocbase(),
      "FOR d IN collection FILTER ANALYZER(d.foo == 'abc', 'invalid analzyer') "
      "RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN collection FILTER ANALYZER(d.foo == 'abc', 3.14) RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN collection FILTER ANALYZER(d.foo == 'abc', null) RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN collection FILTER ANALYZER(d.foo == 'abc', true) RETURN d");

  // non-deterministic expression
  assertFilterFail(
      vocbase(),
      "FOR d IN collection FILTER ANALYZER(d.foo == 'abc', RAND() > 0 ? "
      "'test_analyzer' : 'identity') RETURN d");

  // can't execute boost function
  assertFilterExecutionFail(
      vocbase(),
      "LET x=1.5 FOR d IN collection FILTER ANALYZER(d.foo == 'abc', "
      "ANALYZER(x, 'test_analyzer')) RETURN d",
      &ExpressionContextMock::EMPTY);
}

TEST_F(IResearchFilterFunctionTest, MinMatch) {
  // simplest MIN_MATCH
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("x", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                              arangodb::aql::AqlValueHintInt{2})));

    irs::Or expected;
    auto& minMatch = expected.add<irs::Or>();
    minMatch.min_match_count(2);
    auto& termFilter = minMatch.add<irs::Or>().add<irs::by_term>();
    *termFilter.mutable_field() = mangleStringIdentity("foobar");
    irs::assign(termFilter.mutable_options()->term,
                irs::ref_cast<irs::byte_type>(irs::string_ref("bar")));

    assertFilterSuccess(
        vocbase(),
        "LET x=2 FOR d IN collection FILTER MIN_MATCH(d.foobar == 'bar', x) "
        "RETURN d",
        expected, &ctx);
  }

  // simple MIN_MATCH
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("x", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                              arangodb::aql::AqlValueHintInt{2})));

    irs::Or expected;
    auto& minMatch = expected.add<irs::Or>();
    minMatch.min_match_count(2);
    {
      auto& termFilter = minMatch.add<irs::Or>().add<irs::by_term>();
      *termFilter.mutable_field() = mangleStringIdentity("foobar");
      irs::assign(termFilter.mutable_options()->term,
                  irs::ref_cast<irs::byte_type>(irs::string_ref("bar")));
    }
    {
      auto& termFilter = minMatch.add<irs::Or>().add<irs::by_term>();
      *termFilter.mutable_field() = mangleStringIdentity("foobaz");
      irs::assign(termFilter.mutable_options()->term,
                  irs::ref_cast<irs::byte_type>(irs::string_ref("baz")));
    }
    {
      auto& termFilter = minMatch.add<irs::Or>().add<irs::by_term>();
      *termFilter.mutable_field() = mangleStringIdentity("foobad");
      irs::assign(termFilter.mutable_options()->term,
                  irs::ref_cast<irs::byte_type>(irs::string_ref("bad")));
    }

    assertFilterSuccess(
        vocbase(),
        "LET x=2 FOR d IN collection FILTER MIN_MATCH(d.foobar == 'bar', "
        "d.foobaz == 'baz', d.foobad == 'bad', x) RETURN d",
        expected, &ctx);
  }

  // simple MIN_MATCH
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("x", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                              arangodb::aql::AqlValueHintDouble{1.5})));

    irs::Or expected;
    auto& minMatch = expected.add<irs::Or>();
    minMatch.min_match_count(2);
    {
      auto& termFilter = minMatch.add<irs::Or>().add<irs::by_term>();
      *termFilter.mutable_field() = mangleStringIdentity("foobar");
      irs::assign(termFilter.mutable_options()->term,
                  irs::ref_cast<irs::byte_type>(irs::string_ref("bar")));
    }
    {
      auto& termFilter = minMatch.add<irs::Or>().add<irs::by_term>();
      *termFilter.mutable_field() = mangleStringIdentity("foobaz");
      termFilter.boost(1.5f);
      irs::assign(termFilter.mutable_options()->term,
                  irs::ref_cast<irs::byte_type>(irs::string_ref("baz")));
    }
    {
      auto& termFilter = minMatch.add<irs::Or>().add<irs::by_term>();
      *termFilter.mutable_field() = mangleStringIdentity("foobad");
      irs::assign(termFilter.mutable_options()->term,
                  irs::ref_cast<irs::byte_type>(irs::string_ref("bad")));
    }

    assertFilterSuccess(
        vocbase(),
        "LET x=1.5 FOR d IN collection FILTER MIN_MATCH(d.foobar == 'bar', "
        "BOOST(d.foobaz == 'baz', x), d.foobad == 'bad', x) RETURN d",
        expected, &ctx);
  }

  // wrong sub-expression
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("x", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                              arangodb::aql::AqlValueHintDouble{1.5})));

    assertFilterExecutionFail(
        vocbase(),
        "LET x=1.5 FOR d IN collection FILTER MIN_MATCH(d.foobar == 'bar', "
        "BOOST(d.foobaz == 'baz', TO_STRING(x)), d.foobad == 'bad', x) RETURN "
        "d",
        &ctx);
  }

  // boosted MIN_MATCH
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("x", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                              arangodb::aql::AqlValueHintDouble{1.5})));

    irs::Or expected;
    auto& minMatch = expected.add<irs::Or>();
    minMatch.boost(3.0f);
    minMatch.min_match_count(2);
    {
      auto& termFilter = minMatch.add<irs::Or>().add<irs::by_term>();
      *termFilter.mutable_field() = mangleStringIdentity("foobar");
      irs::assign(termFilter.mutable_options()->term,
                  irs::ref_cast<irs::byte_type>(irs::string_ref("bar")));
    }
    {
      auto& termFilter = minMatch.add<irs::Or>().add<irs::by_term>();
      *termFilter.mutable_field() = mangleStringIdentity("foobaz");
      termFilter.boost(1.5f);
      irs::assign(termFilter.mutable_options()->term,
                  irs::ref_cast<irs::byte_type>(irs::string_ref("baz")));
    }
    {
      auto& termFilter = minMatch.add<irs::Or>().add<irs::by_term>();
      *termFilter.mutable_field() = mangleStringIdentity("foobad");
      irs::assign(termFilter.mutable_options()->term,
                  irs::ref_cast<irs::byte_type>(irs::string_ref("bad")));
    }

    assertFilterSuccess(
        vocbase(),
        "LET x=1.5 FOR d IN collection FILTER BOOST(MIN_MATCH(d.foobar == "
        "'bar', BOOST(d.foobaz == 'baz', x), d.foobad == 'bad', x), x*2) "
        "RETURN d",
        expected, &ctx);
  }

  // boosted embedded MIN_MATCH
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("x", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                              arangodb::aql::AqlValueHintDouble{1.5})));

    irs::Or expected;
    auto& minMatch = expected.add<irs::Or>();
    minMatch.boost(3.0f);
    minMatch.min_match_count(2);
    {
      auto& termFilter = minMatch.add<irs::Or>().add<irs::by_term>();
      *termFilter.mutable_field() = mangleStringIdentity("foobar");
      irs::assign(termFilter.mutable_options()->term,
                  irs::ref_cast<irs::byte_type>(irs::string_ref("bar")));
    }
    {
      auto& termFilter = minMatch.add<irs::Or>().add<irs::by_term>();
      *termFilter.mutable_field() = mangleStringIdentity("foobaz");
      termFilter.boost(1.5f);
      irs::assign(termFilter.mutable_options()->term,
                  irs::ref_cast<irs::byte_type>(irs::string_ref("baz")));
    }
    auto& subMinMatch = minMatch.add<irs::Or>().add<irs::Or>();
    subMinMatch.min_match_count(2);
    {
      auto& termFilter = subMinMatch.add<irs::Or>().add<irs::by_term>();
      *termFilter.mutable_field() = mangleStringIdentity("foobar");
      irs::assign(termFilter.mutable_options()->term,
                  irs::ref_cast<irs::byte_type>(irs::string_ref("bar")));
    }
    {
      auto& rangeFilter = subMinMatch.add<irs::Or>().add<irs::by_range>();
      *rangeFilter.mutable_field() = mangleStringIdentity("foobaz");
      rangeFilter.mutable_options()->range.min_type = irs::BoundType::EXCLUSIVE;
      irs::assign(rangeFilter.mutable_options()->range.min,
                  irs::ref_cast<irs::byte_type>(irs::string_ref("baz")));
    }
    {
      auto& termFilter = subMinMatch.add<irs::Or>().add<irs::by_term>();
      *termFilter.mutable_field() = mangleStringIdentity("foobad");
      termFilter.boost(2.7f);
      irs::assign(termFilter.mutable_options()->term,
                  irs::ref_cast<irs::byte_type>(irs::string_ref("bad")));
    }

    assertFilterSuccess(
        vocbase(),
        "LET x=1.5 FOR d IN collection FILTER "
        "  BOOST("
        "    MIN_MATCH("
        "      d.foobar == 'bar', "
        "      BOOST(d.foobaz == 'baz', x), "
        "      MIN_MATCH(d.foobar == 'bar', d.foobaz > 'baz', BOOST(d.foobad "
        "== 'bad', 2.7), x),"
        "    x), "
        "  x*2) "
        "RETURN d",
        expected, &ctx);
  }

  // wrong number of arguments
  assertFilterParseFail(
      vocbase(),
      "FOR d IN collection FILTER MIN_MATCH(d.foobar == 'bar') RETURN d");

  // wrong argument type
  assertFilterFail(
      vocbase(),
      "FOR d IN collection FILTER MIN_MATCH(d.foobar == 'bar', d.foobaz == "
      "'baz', d.foobad == 'bad', '2') RETURN d",
      &ExpressionContextMock::EMPTY);
  assertFilterFail(
      vocbase(),
      "FOR d IN collection FILTER MIN_MATCH(d.foobar == 'bar', d.foobaz == "
      "'baz', d.foobad == 'bad', null) RETURN d",
      &ExpressionContextMock::EMPTY);
  assertFilterFail(
      vocbase(),
      "FOR d IN collection FILTER MIN_MATCH(d.foobar == 'bar', d.foobaz == "
      "'baz', d.foobad == 'bad', true) RETURN d",
      &ExpressionContextMock::EMPTY);

  // non-deterministic expression
  assertFilterFail(
      vocbase(),
      "FOR d IN collection FILTER MIN_MATCH(d.foobar == 'bar', d.foobaz == "
      "'baz', d.foobad == 'bad', RAND()) RETURN d",
      &ExpressionContextMock::EMPTY);
}

TEST_F(IResearchFilterFunctionTest, Exists) {
  // field only
  {
    irs::Or expected;
    auto& exists = expected.add<irs::by_column_existence>();
    *exists.mutable_field() = "name";
    exists.mutable_options()->prefix_match = true;

    assertFilterSuccess(vocbase(),
                        "FOR d IN myView FILTER exists(d.name) RETURN d", expected);
    assertFilterSuccess(vocbase(),
                        "FOR d IN myView FILTER exists(d['name']) RETURN d", expected);
    assertFilterSuccess(vocbase(),
                        "FOR d IN myView FILTER eXists(d.name) RETURN d", expected);
    assertFilterSuccess(vocbase(),
                        "FOR d IN myView FILTER eXists(d['name']) RETURN d", expected);
  }

  // field with simple offset
  {
    irs::Or expected;
    auto& exists = expected.add<irs::by_column_existence>();
    *exists.mutable_field() = "[42]";
    exists.mutable_options()->prefix_match = true;

    assertFilterSuccess(vocbase(),
                        "FOR d IN myView FILTER exists(d[42]) RETURN d", expected);
  }

  // complex field
  {
    irs::Or expected;
    auto& exists = expected.add<irs::by_column_existence>();
    *exists.mutable_field() = "obj.prop.name";
    exists.mutable_options()->prefix_match = true;

    assertFilterSuccess(
        vocbase(), "FOR d IN myView FILTER exists(d.obj.prop.name) RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER exists(d['obj']['prop']['name']) RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN myView FILTER eXists(d.obj.prop.name) RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN myView FILTER eXists(d['obj'].prop.name) RETURN d", expected);
  }

  // complex field with offset
  {
    irs::Or expected;
    auto& exists = expected.add<irs::by_column_existence>();
    *exists.mutable_field() = "obj.prop[3].name";
    exists.mutable_options()->prefix_match = true;

    assertFilterSuccess(
        vocbase(), "FOR d IN myView FILTER exists(d.obj.prop[3].name) RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER exists(d['obj']['prop'][3]['name']) RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN myView FILTER eXists(d.obj.prop[3].name) RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER eXists(d['obj'].prop[3].name) RETURN d", expected);
  }

  // complex field with offset
  {
    irs::Or expected;
    auto& exists = expected.add<irs::by_column_existence>();
    *exists.mutable_field() = "obj.prop[3].name";
    exists.boost(1.5f);
    exists.mutable_options()->prefix_match = true;

    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER BOOST(exists(d.obj.prop[3].name), 1.5) RETURN "
        "d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER BooSt(exists(d['obj']['prop'][3]['name']), "
        "0.5*3) RETURN d",
        expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER booSt(eXists(d.obj.prop[3].name), 1+0.5) "
        "RETURN d",
        expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER BoOSt(eXists(d['obj'].prop[3].name), 1.5) "
        "RETURN d",
        expected);
  }

  // complex field with offset
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("index", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt(2)));

    irs::Or expected;
    auto& exists = expected.add<irs::by_column_existence>();
    *exists.mutable_field() = "obj.prop[3].name";
    exists.mutable_options()->prefix_match = true;

    assertFilterSuccess(
        vocbase(),
        "LET index=2 FOR d IN myView FILTER exists(d.obj.prop[index+1].name) "
        "RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER exists(d['obj']['prop'][3]['name']) RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN myView FILTER eXists(d.obj.prop[3].name) RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER eXists(d['obj'].prop[3].name) RETURN d", expected);
  }

  // dynamic complex attribute field
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                                      arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                                      arangodb::aql::AqlValueHintDouble{5.6})));

    irs::Or expected;
    auto& exists = expected.add<irs::by_column_existence>();
    *exists.mutable_field() = "a.b.c.e[4].f[5].g[3].g.a";
    exists.mutable_options()->prefix_match = true;

    assertFilterSuccess(
        vocbase(),
        "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER "
        "exists(d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_"
        "('a')]) RETURN d",
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
        vocbase(),
        "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER "
        "exists(d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_"
        "('a')]) RETURN d",
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
        vocbase(),
        "LET a=null LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER "
        "exists(d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_"
        "('a')]) RETURN d",
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
        vocbase(),
        "LET a=false LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER "
        "exists(d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_"
        "('a')]) RETURN d",
        &ctx);
  }

  // invalid attribute access
  assertFilterFail(vocbase(), "FOR d IN myView FILTER exists(d) RETURN d");
  assertFilterFail(vocbase(), "FOR d IN myView FILTER exists(d[*]) RETURN d");
  assertFilterFail(vocbase(),
                   "FOR d IN myView FILTER exists(d.a.b[*]) RETURN d");
  assertFilterFail(vocbase(),
                   "FOR d IN myView FILTER exists('d.name') RETURN d");
  assertFilterFail(vocbase(), "FOR d IN myView FILTER exists(123) RETURN d");
  assertFilterFail(vocbase(), "FOR d IN myView FILTER exists(123.5) RETURN d");
  assertFilterFail(vocbase(), "FOR d IN myView FILTER exists(null) RETURN d");
  assertFilterFail(vocbase(), "FOR d IN myView FILTER exists(true) RETURN d");
  assertFilterFail(vocbase(), "FOR d IN myView FILTER exists(false) RETURN d");

  // field + type
  {
    irs::Or expected;
    auto& exists = expected.add<irs::by_column_existence>();
    *exists.mutable_field() = mangleType("name");
    exists.mutable_options()->prefix_match = true;

    assertFilterSuccess(
        vocbase(), "FOR d IN myView FILTER exists(d.name, 'type') RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN myView FILTER eXists(d.name, 'type') RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN myView FILTER exists(d.name, 'Type') RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN myView FILTER exists(d.name, 'TYPE') RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER ANALYZER(exists(d.name, 'TYPE'), "
        "'test_analyzer') RETURN d",
        expected);

    // invalid 2nd argument
    assertFilterFail(
        vocbase(), "FOR d IN myView FILTER exists(d.name, 'invalid') RETURN d");
    assertFilterExecutionFail(
        vocbase(), "FOR d IN myView FILTER exists(d.name, d) RETURN d",
        &ExpressionContextMock::EMPTY);
    assertFilterFail(vocbase(),
                     "FOR d IN myView FILTER exists(d.name, null) RETURN d");
    assertFilterFail(vocbase(),
                     "FOR d IN myView FILTER exists(d.name, 123) RETURN d");
    assertFilterFail(vocbase(),
                     "FOR d IN myView FILTER exists(d.name, 123.5) RETURN d");
    assertFilterFail(vocbase(),
                     "FOR d IN myView FILTER exists(d.name, true) RETURN d");
    assertFilterFail(vocbase(),
                     "FOR d IN myView FILTER exists(d.name, false) RETURN d");

    // invalid 3rd argument
    assertFilterFail(
        vocbase(),
        "FOR d IN myView FILTER exists(d.name, 'TYPE', 'test_analyzer') RETURN "
        "d");
  }

  // field + any string value
  {
    irs::Or expected;
    auto& exists = expected.add<irs::by_column_existence>();
    *exists.mutable_field() = mangleAnalyzer("name");
    exists.mutable_options()->prefix_match = true;

    assertFilterSuccess(
        vocbase(), "FOR d IN myView FILTER exists(d.name, 'string') RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN myView FILTER eXists(d.name, 'string') RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN myView FILTER exists(d.name, 'String') RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN myView FILTER exists(d.name, 'STRING') RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER analyzer(exists(d.name, 'STRING'), "
        "'test_analyzer') RETURN d",
        expected);

    // invalid 3rd argument
    assertFilterFail(
        vocbase(),
        "FOR d IN myView FILTER exists(d.name, 'string', 'test_analyzer') "
        "RETURN d");
  }

  // invalid 2nd argument
  assertFilterFail(vocbase(),
                   "FOR d IN myView FILTER exists(d.name, 'foo') RETURN d");
  assertFilterExecutionFail(vocbase(),
                            "FOR d IN myView FILTER exists(d.name, d) RETURN d",
                            &ExpressionContextMock::EMPTY);
  assertFilterFail(vocbase(),
                   "FOR d IN myView FILTER exists(d.name, null) RETURN d");
  assertFilterFail(vocbase(),
                   "FOR d IN myView FILTER exists(d.name, 123) RETURN d");
  assertFilterFail(vocbase(),
                   "FOR d IN myView FILTER exists(d.name, 123.5) RETURN d");
  assertFilterFail(vocbase(),
                   "FOR d IN myView FILTER exists(d.name, true) RETURN d");
  assertFilterFail(vocbase(),
                   "FOR d IN myView FILTER exists(d.name, false) RETURN d");

  // field + any string value mode as an expression
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("anl", arangodb::aql::AqlValue("str"));

    irs::Or expected;
    auto& exists = expected.add<irs::by_column_existence>();
    *exists.mutable_field() = mangleAnalyzer("name");
    exists.mutable_options()->prefix_match = true;

    assertFilterSuccess(vocbase(),
                        "LET anl='str' FOR d IN myView FILTER exists(d.name, "
                        "CONCAT(anl,'ing')) RETURN d",
                        expected, &ctx);
    assertFilterSuccess(vocbase(),
                        "LET anl='str' FOR d IN myView FILTER eXists(d.name, "
                        "CONCAT(anl,'ing')) RETURN d",
                        expected, &ctx);

    // invalid 3rd argument
    assertFilterExecutionFail(
        vocbase(),
        "LET anl='str' FOR d IN myView FILTER eXists(d.name, "
        "CONCAT(anl,'ing'), 'test_analyzer') RETURN d",
        &ctx);
  }

  // field + analyzer as invalid expression
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("anl", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintNull{}));

    assertFilterExecutionFail(
        vocbase(),
        "LET anl='analyz' FOR d IN myView FILTER exists(d.name, anl) RETURN d", &ctx);
    assertFilterExecutionFail(
        vocbase(),
        "LET anl='analyz' FOR d IN myView FILTER eXists(d.name, anl) RETURN d", &ctx);
  }

  // field + analyzer
  {
    irs::Or expected;
    auto& exists = expected.add<irs::by_column_existence>();
    *exists.mutable_field() = mangleStringIdentity("name");
    exists.mutable_options()->prefix_match = false;

    assertFilterSuccess(
        vocbase(), "FOR d IN myView FILTER exists(d.name, 'analyzer') RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN myView FILTER eXists(d.name, 'analyzer') RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER analyzer(eXists(d.name, 'analyzer'), "
        "'identity') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER eXists(d.name, 'analyzer', 'identity') RETURN "
        "d",
        expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN myView FILTER exists(d.name, 'Analyzer') RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN myView FILTER exists(d.name, 'ANALYZER') RETURN d", expected);

    // invalid 2nd argument
    assertFilterFail(
        vocbase(), "FOR d IN myView FILTER exists(d.name, 'invalid') RETURN d");

    // invalid analyzer argument
    assertFilterFail(
        vocbase(),
        "FOR d IN myView FILTER exists(d.name, 'analyzer', 'invalid') RETURN "
        "d");
  }

  // field + analyzer as an expression
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("type", arangodb::aql::AqlValue("analy"));

    irs::Or expected;
    auto& exists = expected.add<irs::by_column_existence>();
    *exists.mutable_field() = mangleStringIdentity("name");
    exists.mutable_options()->prefix_match = false;

    assertFilterSuccess(
        vocbase(),
        "LET type='analy' FOR d IN myView FILTER exists(d.name, "
        "CONCAT(type,'zer')) RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET type='analy' FOR d IN myView FILTER eXists(d.name, "
        "CONCAT(type,'zer')) RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET type='analy' FOR d IN myView FILTER analyzer(eXists(d.name, "
        "CONCAT(type,'zer')), 'identity') RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET type='analy' FOR d IN myView FILTER eXists(d.name, "
        "CONCAT(type,'zer'), 'identity') RETURN d",
        expected, &ctx);
  }

  // field + numeric
  {
    irs::Or expected;
    auto& exists = expected.add<irs::by_column_existence>();
    *exists.mutable_field() = mangleNumeric("obj.name");
    exists.mutable_options()->prefix_match = false;

    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER exists(d.obj.name, 'numeric') RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER eXists(d.obj.name, 'numeric') RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER eXists(d.obj.name, 'Numeric') RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER eXists(d.obj.name, 'NUMERIC') RETURN d", expected);

    // invalid argument
    assertFilterFail(
        vocbase(), "FOR d IN myView FILTER exists(d.obj.name, 'foo') RETURN d");

    // invalid 3rd argument
    assertFilterFail(
        vocbase(),
        "FOR d IN myView FILTER exists(d.obj.name, 'numeric', 'test_analyzer') "
        "RETURN d");
  }

  // field + numeric as an expression
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("type", arangodb::aql::AqlValue("nume"));

    irs::Or expected;
    auto& exists = expected.add<irs::by_column_existence>();
    *exists.mutable_field() = mangleNumeric("name");
    exists.mutable_options()->prefix_match = false;

    assertFilterSuccess(vocbase(),
                        "LET type='nume' FOR d IN myView FILTER exists(d.name, "
                        "CONCAT(type,'ric')) RETURN d",
                        expected, &ctx);
    assertFilterSuccess(vocbase(),
                        "LET type='nume' FOR d IN myView FILTER eXists(d.name, "
                        "CONCAT(type,'ric')) RETURN d",
                        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET type='nume' FOR d IN myView FILTER ANALYZER(eXists(d.name, "
        "CONCAT(type,'ric')), 'test_analyzer') RETURN d",
        expected, &ctx);

    // invalid 3rd argument
    assertFilterExecutionFail(
        vocbase(),
        "LET type='nume' FOR d IN myView FILTER eXists(d.name, "
        "CONCAT(type,'ric'), 'test_analyzer') RETURN d",
        &ctx);
  }

  // field + bool
  {
    irs::Or expected;
    auto& exists = expected.add<irs::by_column_existence>();
    *exists.mutable_field() = mangleBool("name");
    exists.mutable_options()->prefix_match = false;

    assertFilterSuccess(
        vocbase(), "FOR d IN myView FILTER exists(d.name, 'bool') RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN myView FILTER eXists(d.name, 'bool') RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN myView FILTER eXists(d.name, 'Bool') RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN myView FILTER eXists(d.name, 'BOOL') RETURN d", expected);

    // invalid 2nd argument
    assertFilterFail(
        vocbase(),
        "FOR d IN myView FILTER exists(d.name, 'asdfasdfa') RETURN d");

    // invalid 3rd argument
    assertFilterFail(
        vocbase(),
        "FOR d IN myView FILTER exists(d.obj.name, 'bool', 'test_analyzer') "
        "RETURN d");
  }

  // field + type + boolean
  {
    irs::Or expected;
    auto& exists = expected.add<irs::by_column_existence>();
    *exists.mutable_field() = mangleBool("name");
    exists.mutable_options()->prefix_match = false;

    assertFilterSuccess(
        vocbase(), "FOR d IN myView FILTER exists(d.name, 'boolean') RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN myView FILTER eXists(d.name, 'boolean') RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER analyzer(eXists(d.name, 'boolean'), "
        "'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN myView FILTER eXists(d.name, 'Boolean') RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN myView FILTER eXists(d.name, 'BOOLEAN') RETURN d", expected);

    // invalid 2nd argument
    assertFilterFail(
        vocbase(),
        "FOR d IN myView FILTER exists(d.name, 'asdfasdfa') RETURN d");

    // invalid 3rd argument
    assertFilterFail(
        vocbase(),
        "FOR d IN myView FILTER exists(d.obj.name, 'boolean', 'test_analyzer') "
        "RETURN d");
  }

  // field + boolean as an expression
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("type", arangodb::aql::AqlValue("boo"));

    irs::Or expected;
    auto& exists = expected.add<irs::by_column_existence>();
    *exists.mutable_field() = mangleBool("name");
    exists.mutable_options()->prefix_match = false;

    assertFilterSuccess(vocbase(),
                        "LET type='boo' FOR d IN myView FILTER exists(d.name, "
                        "CONCAT(type,'lean')) RETURN d",
                        expected, &ctx);
    assertFilterSuccess(vocbase(),
                        "LET type='boo' FOR d IN myView FILTER eXists(d.name, "
                        "CONCAT(type,'lean')) RETURN d",
                        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET type='boo' FOR d IN myView FILTER ANALYZER(eXists(d.name, "
        "CONCAT(type,'lean')), 'test_analyzer') RETURN d",
        expected, &ctx);

    // invalid 3rd argument
    assertFilterExecutionFail(
        vocbase(),
        "LET type='boo' FOR d IN myView FILTER eXists(d.name, "
        "CONCAT(type,'lean'), 'test_analyzer') RETURN d",
        &ctx);
  }

  // field + null
  {
    irs::Or expected;
    auto& exists = expected.add<irs::by_column_existence>();
    *exists.mutable_field() = mangleNull("name");
    exists.mutable_options()->prefix_match = false;

    assertFilterSuccess(
        vocbase(), "FOR d IN myView FILTER exists(d.name, 'null') RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN myView FILTER eXists(d.name, 'null') RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN myView FILTER eXists(d.name, 'Null') RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN myView FILTER eXists(d.name, 'NULL') RETURN d", expected);

    // invalid 2nd argument
    assertFilterFail(
        vocbase(),
        "FOR d IN myView FILTER exists(d.name, 'asdfasdfa') RETURN d");

    // invalid 3rd argument
    assertFilterFail(
        vocbase(),
        "FOR d IN myView FILTER eXists(d.name, 'NULL', 'test_analyzer') RETURN "
        "d");
  }

  // field + null as an expression
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("type", arangodb::aql::AqlValue("nu"));

    irs::Or expected;
    auto& exists = expected.add<irs::by_column_existence>();
    *exists.mutable_field() = mangleNull("name");
    exists.mutable_options()->prefix_match = false;

    assertFilterSuccess(vocbase(),
                        "LET type='nu' FOR d IN myView FILTER exists(d.name, "
                        "CONCAT(type,'ll')) RETURN d",
                        expected, &ctx);
    assertFilterSuccess(vocbase(),
                        "LET type='nu' FOR d IN myView FILTER eXists(d.name, "
                        "CONCAT(type,'ll')) RETURN d",
                        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET type='nu' FOR d IN myView FILTER ANALYZER(eXists(d.name, "
        "CONCAT(type,'ll')), 'identity') RETURN d",
        expected, &ctx);

    // invalid 3rd argument
    assertFilterExecutionFail(
        vocbase(),
        "LET type='nu' FOR d IN myView FILTER eXists(d.name, "
        "CONCAT(type,'ll'), 'identity') RETURN d",
        &ctx);
  }

  // field + type + invalid expression
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("type", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintNull{}));

    assertFilterExecutionFail(
        vocbase(),
        "LET type=null FOR d IN myView FILTER exists(d.name, type) RETURN d", &ctx);
    assertFilterExecutionFail(
        vocbase(),
        "LET type=null FOR d IN myView FILTER eXists(d.name, type) RETURN d", &ctx);
  }

  // invalid 2nd argument
  assertFilterExecutionFail(vocbase(),
                            "FOR d IN myView FILTER exists(d.name, d) RETURN d",
                            &ExpressionContextMock::EMPTY);
  assertFilterFail(vocbase(),
                   "FOR d IN myView FILTER exists(d.name, null) RETURN d");
  assertFilterFail(vocbase(),
                   "FOR d IN myView FILTER exists(d.name, 123) RETURN d");
  assertFilterFail(vocbase(),
                   "FOR d IN myView FILTER exists(d.name, 123.5) RETURN d");
  assertFilterFail(vocbase(),
                   "FOR d IN myView FILTER exists(d.name, true) RETURN d");
  assertFilterFail(vocbase(),
                   "FOR d IN myView FILTER exists(d.name, false) RETURN d");

  // field + default analyzer
  {
    irs::Or expected;
    auto& exists = expected.add<irs::by_column_existence>();
    *exists.mutable_field() = mangleStringIdentity("name");
    exists.mutable_options()->prefix_match = false;

    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER analyzer(exists(d.name, 'analyzer'), "
        "'identity') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN myView FILTER eXists(d.name, 'analyzer') RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER eXists(d.name, 'analyzer', 'identity') RETURN "
        "d",
        expected);
  }

  // field + analyzer
  {
    irs::Or expected;
    auto& exists = expected.add<irs::by_column_existence>();
    *exists.mutable_field() = mangleString("name", "test_analyzer");
    exists.mutable_options()->prefix_match = false;

    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER analyzer(exists(d.name, 'analyzer'), "
        "'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER exists(d.name, 'analyzer', 'test_analyzer') "
        "RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER analyzer(eXists(d.name, 'analyzer'), "
        "'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER eXists(d.name, 'analyzer', 'test_analyzer') "
        "RETURN d",
        expected);

    // invalid analyzer
    assertFilterFail(
        vocbase(),
        "FOR d IN myView FILTER analyzer(exists(d.name, 'analyzer'), 'foo') "
        "RETURN d");
    assertFilterFail(
        vocbase(),
        "FOR d IN myView FILTER analyzer(exists(d.name, 'analyzer'), "
        "'invalid') RETURN d");
    assertFilterFail(
        vocbase(),
        "FOR d IN myView FILTER analyzer(exists(d.name, 'analyzer'), '') "
        "RETURN d");
    assertFilterExecutionFail(
        vocbase(),
        "FOR d IN myView FILTER analyzer(exists(d.name, 'analyzer'), d) RETURN "
        "d",
        &ExpressionContextMock::EMPTY);
    assertFilterFail(
        vocbase(),
        "FOR d IN myView FILTER analyzer(exists(d.name, 'analyzer'), null) "
        "RETURN d");
    assertFilterFail(
        vocbase(),
        "FOR d IN myView FILTER analyzer(exists(d.name, 'analyzer'), 123) "
        "RETURN d");
    assertFilterFail(
        vocbase(),
        "FOR d IN myView FILTER analyzer(exists(d.name, 'analyzer'), 123.5) "
        "RETURN d");
    assertFilterFail(
        vocbase(),
        "FOR d IN myView FILTER analyzer(exists(d.name, 'analyzer'), true) "
        "RETURN d");
    assertFilterFail(
        vocbase(),
        "FOR d IN myView FILTER analyzer(exists(d.name, 'analyzer'), false) "
        "RETURN d");
    assertFilterFail(
        vocbase(),
        "FOR d IN myView FILTER exists(d.name, 'analyzer', 'foo') RETURN d");
    assertFilterFail(
        vocbase(),
        "FOR d IN myView FILTER exists(d.name, 'analyzer', 'invalid') RETURN "
        "d");
    assertFilterFail(
        vocbase(),
        "FOR d IN myView FILTER exists(d.name, 'analyzer', '') RETURN d");
    assertFilterExecutionFail(
        vocbase(),
        "FOR d IN myView FILTER exists(d.name, 'analyzer', d) RETURN d",
        &ExpressionContextMock::EMPTY);
    assertFilterFail(
        vocbase(),
        "FOR d IN myView FILTER exists(d.name, 'analyzer', null) RETURN d");
    assertFilterFail(
        vocbase(),
        "FOR d IN myView FILTER exists(d.name, 'analyzer', 123) RETURN d");
    assertFilterFail(
        vocbase(),
        "FOR d IN myView FILTER exists(d.name, 'analyzer', 123.5) RETURN d");
    assertFilterFail(
        vocbase(),
        "FOR d IN myView FILTER exists(d.name, 'analyzer', true) RETURN d");
    assertFilterFail(
        vocbase(),
        "FOR d IN myView FILTER exists(d.name, 'analyzer', false) RETURN d");
  }

  // field + type + analyzer as an expression
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("anl", arangodb::aql::AqlValue("test_"));
    ctx.vars.emplace("type", arangodb::aql::AqlValue("analyz"));

    irs::Or expected;
    auto& exists = expected.add<irs::by_column_existence>();
    *exists.mutable_field() = mangleString("name", "test_analyzer");
    exists.mutable_options()->prefix_match = false;

    assertFilterSuccess(
        vocbase(),
        "LET type='analyz' LET anl='test_' FOR d IN myView FILTER "
        "analyzer(exists(d.name, CONCAT(type,'er')), CONCAT(anl,'analyzer')) "
        "RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET type='analyz' LET anl='test_' FOR d IN myView FILTER "
        "analyzer(eXists(d.name, CONCAT(type,'er')), CONCAT(anl,'analyzer')) "
        "RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET type='analyz' LET anl='test_' FOR d IN myView FILTER "
        "exists(d.name, CONCAT(type,'er'), CONCAT(anl,'analyzer')) RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET type='analyz' LET anl='test_' FOR d IN myView FILTER "
        "eXists(d.name, CONCAT(type,'er'), CONCAT(anl,'analyzer')) RETURN d",
        expected, &ctx);
  }

  // field + analyzer via []
  {
    irs::Or expected;
    auto& exists = expected.add<irs::by_column_existence>();
    *exists.mutable_field() = mangleString("name", "test_analyzer");
    exists.mutable_options()->prefix_match = false;

    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER analyzer(exists(d['name'], 'analyzer'), "
        "'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER analyzer(eXists(d['name'], 'analyzer'), "
        "'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER exists(d['name'], 'analyzer', 'test_analyzer') "
        "RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER eXists(d['name'], 'analyzer', 'test_analyzer') "
        "RETURN d",
        expected);

    // invalid analyzer argument
    assertFilterFail(
        vocbase(),
        "FOR d IN myView FILTER analyzer(exists(d['name'], 'analyzer'), 'foo') "
        "RETURN d");
    assertFilterFail(
        vocbase(),
        "FOR d IN myView FILTER analyzer(exists(d['name'], 'analyzer'), "
        "'invalid') RETURN d");
    assertFilterFail(
        vocbase(),
        "FOR d IN myView FILTER analyzer(exists(d['name'], 'analyzer'), '') "
        "RETURN d");
    assertFilterExecutionFail(
        vocbase(),
        "FOR d IN myView FILTER analyzer(exists(d['name'], 'analyzer'), d) "
        "RETURN d",
        &ExpressionContextMock::EMPTY);
    assertFilterFail(
        vocbase(),
        "FOR d IN myView FILTER analyzer(exists(d['name'], 'analyzer'), null) "
        "RETURN d");
    assertFilterFail(
        vocbase(),
        "FOR d IN myView FILTER analyzer(exists(d['name'], 'analyzer'), 123) "
        "RETURN d");
    assertFilterFail(
        vocbase(),
        "FOR d IN myView FILTER analyzer(exists(d['name'], 'analyzer'), 123.5) "
        "RETURN d");
    assertFilterFail(
        vocbase(),
        "FOR d IN myView FILTER analyzer(exists(d['name'], 'analyzer'), true) "
        "RETURN d");
    assertFilterFail(
        vocbase(),
        "FOR d IN myView FILTER analyzer(exists(d['name'], 'analyzer'), false) "
        "RETURN d");
    assertFilterFail(
        vocbase(),
        "FOR d IN myView FILTER exists(d['name'], 'analyzer', 'foo') RETURN d");
    assertFilterFail(
        vocbase(),
        "FOR d IN myView FILTER exists(d['name'], 'analyzer', 'invalid') "
        "RETURN d");
    assertFilterFail(
        vocbase(),
        "FOR d IN myView FILTER exists(d['name'], 'analyzer', '') RETURN d");
    assertFilterExecutionFail(
        vocbase(),
        "FOR d IN myView FILTER exists(d['name'], 'analyzer', d) RETURN d",
        &ExpressionContextMock::EMPTY);
    assertFilterFail(
        vocbase(),
        "FOR d IN myView FILTER exists(d['name'], 'analyzer', null) RETURN d");
    assertFilterFail(
        vocbase(),
        "FOR d IN myView FILTER exists(d['name'], 'analyzer', 123) RETURN d");
    assertFilterFail(
        vocbase(),
        "FOR d IN myView FILTER exists(d['name'], 'analyzer', 123.5) RETURN d");
    assertFilterFail(
        vocbase(),
        "FOR d IN myView FILTER exists(d['name'], 'analyzer', true) RETURN d");
    assertFilterFail(
        vocbase(),
        "FOR d IN myView FILTER exists(d['name'], 'analyzer', false) RETURN d");
  }

  // field + identity analyzer
  {
    irs::Or expected;
    auto& exists = expected.add<irs::by_column_existence>();
    *exists.mutable_field() = mangleStringIdentity("name");
    exists.mutable_options()->prefix_match = false;

    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER analyzer(exists(d.name, 'analyzer'), "
        "'identity') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN myView FILTER eXists(d.name, 'analyzer') RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER eXists(d.name, 'analyzer', 'identity') RETURN "
        "d",
        expected);
  }

  // invalid number of arguments
  assertFilterParseFail(vocbase(), "FOR d IN myView FILTER exists() RETURN d");
  assertFilterParseFail(
      vocbase(),
      "FOR d IN myView FILTER exists(d.name, 'type', 'null', d) RETURN d");
  assertFilterParseFail(
      vocbase(),
      "FOR d IN myView FILTER exists(d.name, 'analyzer', 'test_analyzer', "
      "false) RETURN d");

  // non-deterministic arguments
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER exists(d[RAND() ? 'name' : 'x']) RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER exists(d.name, RAND() > 2 ? 'null' : 'string') "
      "RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER exists(d.name, 'analyzer', RAND() > 2 ? "
      "'test_analyzer' : 'identity') RETURN d");
}

TEST_F(IResearchFilterFunctionTest, Phrase) {
  // wrong number of arguments
  assertFilterParseFail(vocbase(), "FOR d IN myView FILTER phrase() RETURN d");

  // identity analyzer
  {
    irs::Or expected;
    auto& phrase = expected.add<irs::by_phrase>();
    *phrase.mutable_field() = mangleStringIdentity("name");
    auto* opts = phrase.mutable_options();
    opts->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("quick"));

    // implicit (by default)
    assertFilterSuccess(
        vocbase(), "FOR d IN myView FILTER phrase(d.name, 'quick') RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN myView FILTER phrase(d['name'], 'quick') RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN myView FILTER phRase(d.name, 'quick') RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN myView FILTER phRase(d['name'], 'quick') RETURN d", expected);

    // explicit
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER Analyzer(phrase(d.name, 'quick'), 'identity') "
        "RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER Analyzer(phrase(d['name'], 'quick'), "
        "'identity') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER Analyzer(phRase(d.name, 'quick'), 'identity') "
        "RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER Analyzer(phRase(d['name'], 'quick'), "
        "'identity') RETURN d",
        expected);

    // overridden
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER phrase(d.name, 'quick', 'identity') RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER phrase(d['name'], 'quick', 'identity') RETURN "
        "d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER phRase(d.name, 'quick', 'identity') RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER phRase(d['name'], 'quick', 'identity') RETURN "
        "d",
        expected);

    // overridden
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER Analyzer(phrase(d.name, 'quick', 'identity'), "
        "'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER Analyzer(phrase(d['name'], 'quick', "
        "'identity'), 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER Analyzer(phRase(d.name, 'quick', 'identity'), "
        "'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER Analyzer(phRase(d['name'], 'quick', "
        "'identity'), 'test_analyzer') RETURN d",
        expected);
  }

  // without offset, custom analyzer
  // quick
  {
    irs::Or expected;
    auto& phrase = expected.add<irs::by_phrase>();
    *phrase.mutable_field() = mangleString("name", "test_analyzer");
    auto* opts = phrase.mutable_options();
    opts->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("q"));
    opts->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("u"));
    opts->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("i"));
    opts->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("c"));
    opts->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("k"));

    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER Analyzer(phrase(d.name, 'quick'), "
        "'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER Analyzer(phrase(d['name'], 'quick'), "
        "'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER Analyzer(phRase(d.name, 'quick'), "
        "'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER Analyzer(phRase(d['name'], 'quick'), "
        "'test_analyzer') RETURN d",
        expected);

    // overridden
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER phrase(d.name, 'quick', 'test_analyzer') "
        "RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER phrase(d['name'], 'quick', 'test_analyzer') "
        "RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER phRase(d.name, 'quick', 'test_analyzer') "
        "RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER phRase(d['name'], 'quick', 'test_analyzer') "
        "RETURN d",
        expected);

    // invalid attribute access
    assertFilterFail(
        vocbase(),
        "FOR d IN myView FILTER analYzER(phrase(d, 'quick'), 'test_analyzer') "
        "RETURN d");
    assertFilterFail(vocbase(),
                     "FOR d IN myView FILTER analYzER(phrase(d[*], 'quick'), "
                     "'test_analyzer') RETURN d");
    assertFilterFail(
        vocbase(),
        "FOR d IN myView FILTER analYzER(phrase(d.a.b[*].c, 'quick'), "
        "'test_analyzer') RETURN d");
    assertFilterFail(
        vocbase(),
        "FOR d IN myView FILTER analYzER(phrase('d.name', 'quick'), "
        "'test_analyzer') RETURN d");
    assertFilterFail(vocbase(),
                     "FOR d IN myView FILTER analYzER(phrase(123, 'quick'), "
                     "'test_analyzer') RETURN d");
    assertFilterFail(vocbase(),
                     "FOR d IN myView FILTER analYzER(phrase(123.5, 'quick'), "
                     "'test_analyzer') RETURN d");
    assertFilterFail(vocbase(),
                     "FOR d IN myView FILTER analYzER(phrase(null, 'quick'), "
                     "'test_analyzer') RETURN d");
    assertFilterFail(vocbase(),
                     "FOR d IN myView FILTER analYzER(phrase(true, 'quick'), "
                     "'test_analyzer') RETURN d");
    assertFilterFail(vocbase(),
                     "FOR d IN myView FILTER analYzER(phrase(false, 'quick'), "
                     "'test_analyzer') RETURN d");

    // empty phrase
    irs::Or expectedEmpty;
    auto& phraseEmpty = expectedEmpty.add<irs::by_phrase>();
    *phraseEmpty.mutable_field() = mangleString("name", "test_analyzer");
    assertFilterSuccess(
      vocbase(),
      "FOR d IN myView FILTER ANALYZER(phrase(d.name, [ ]), 'test_analyzer') "
      "RETURN d",
      expectedEmpty);
    assertFilterSuccess(
      vocbase(),
      "FOR d IN myView FILTER ANALYZER(phrase(d['name'], [ ]), "
      "'test_analyzer') RETURN d",
      expectedEmpty);

    // accumulating offsets
    irs::Or expectedAccumulated;
    auto& phraseAccumulated = expectedAccumulated.add<irs::by_phrase>();
    *phraseAccumulated.mutable_field() = mangleString("name", "test_analyzer");
    auto* optsAccumulated = phraseAccumulated.mutable_options();
    optsAccumulated->push_back<irs::by_term_options>( ).term = irs::ref_cast<irs::byte_type>(irs::string_ref("q"));
    optsAccumulated->push_back<irs::by_term_options>(7).term = irs::ref_cast<irs::byte_type>(irs::string_ref("u"));
    optsAccumulated->push_back<irs::by_term_options>(3).term = irs::ref_cast<irs::byte_type>(irs::string_ref("i"));
    optsAccumulated->push_back<irs::by_term_options>(4).term = irs::ref_cast<irs::byte_type>(irs::string_ref("c"));
    optsAccumulated->push_back<irs::by_term_options>(5).term = irs::ref_cast<irs::byte_type>(irs::string_ref("k"));

    assertFilterSuccess(
      vocbase(),
      "FOR d IN myView FILTER ANALYZER(phrase(d.name, "
      " 'q', 0, [], 3, [], 4, 'u', 3, [], 0, 'i', 0, [], 4, 'c', 1, [], 1, [], 2, [], 1, 'k'), "
      " 'test_analyzer') "
      "RETURN d",
      expectedAccumulated);

    // invalid input
    assertFilterFail(
        vocbase(),
        "FOR d IN myView FILTER ANALYZER(phrase(d.name, [ 1, \"abc\" ]), "
        "'test_analyzer') RETURN d");
    assertFilterFail(
        vocbase(),
        "FOR d IN myView FILTER ANALYZER(phrase(d['name'], [ 1, \"abc\" ]), "
        "'test_analyzer') RETURN d");
    assertFilterFail(vocbase(),
                     "FOR d IN myView FILTER ANALYZER(phrase(d.name, true), "
                     "'test_analyzer') RETURN d");
    assertFilterFail(
        vocbase(),
        "FOR d IN myView FILTER ANALYZER(phrase(d['name'], false), "
        "'test_analyzer') RETURN d");
    assertFilterFail(vocbase(),
                     "FOR d IN myView FILTER ANALYZER(phrase(d.name, null), "
                     "'test_analyzer') RETURN d");
    assertFilterFail(vocbase(),
                     "FOR d IN myView FILTER ANALYZER(phrase(d['name'], null), "
                     "'test_analyzer') RETURN d");
    assertFilterFail(vocbase(),
                     "FOR d IN myView FILTER ANALYZER(phrase(d.name, 3.14), "
                     "'test_analyzer') RETURN d");
    assertFilterFail(vocbase(),
                     "FOR d IN myView FILTER ANALYZER(phrase(d['name'], 1234), "
                     "'test_analyzer') RETURN d");
    assertFilterFail(
        vocbase(),
        "FOR d IN myView FILTER ANALYZER(phrase(d.name, { \"a\": 7, \"b\": "
        "\"c\" }), 'test_analyzer') RETURN d");
    assertFilterFail(
        vocbase(),
        "FOR d IN myView FILTER ANALYZER(phrase(d['name'], { \"a\": 7, \"b\": "
        "\"c\" }), 'test_analyzer') RETURN d");
  }

  // dynamic complex attribute field
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                                      arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                                      arangodb::aql::AqlValueHintDouble{5.6})));

    irs::Or expected;
    auto& phrase = expected.add<irs::by_phrase>();
    *phrase.mutable_field() = mangleString("a.b.c.e[4].f[5].g[3].g.a", "test_analyzer");
    auto* opts= phrase.mutable_options();
    opts->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("q"));
    opts->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("u"));
    opts->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("i"));
    opts->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("c"));
    opts->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("k"));

    assertFilterSuccess(
        vocbase(),
        "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER "
        "analyzer(phrase(d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g["
        "_FORWARD_('a')], 'quick'), 'test_analyzer') RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER "
        "phrase(d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_"
        "('a')], 'quick', 'test_analyzer') RETURN d",
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
        vocbase(),
        "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER "
        "analyzer(phrase(d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g["
        "_FORWARD_('a')], 'quick'), 'test_analyzer') RETURN d",
        &ctx);
    assertFilterExecutionFail(
        vocbase(),
        "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER "
        "phrase(d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_"
        "('a')], 'quick', 'test_analyzer') RETURN d",
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
        vocbase(),
        "LET a=null LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER "
        "analyzer(phrase(d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g["
        "_FORWARD_('a')], 'quick'), 'test_analyzer') RETURN d",
        &ctx);
    assertFilterExecutionFail(
        vocbase(),
        "LET a=null LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER "
        "phrase(d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_"
        "('a')], 'quick', 'test_analyzer') RETURN d",
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
        vocbase(),
        "LET a=false LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER "
        "AnalyzeR(phrase(d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g["
        "_FORWARD_('a')], 'quick'), 'test_analyzer') RETURN d",
        &ctx);
    assertFilterExecutionFail(
        vocbase(),
        "LET a=false LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER "
        "phrase(d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_"
        "('a')], 'quick', 'test_analyzer') RETURN d",
        &ctx);
  }

  // field with simple offset
  // without offset, custom analyzer
  // quick
  {
    irs::Or expected;
    auto& phrase = expected.add<irs::by_phrase>();
    *phrase.mutable_field() = mangleString("[42]", "test_analyzer");
    auto* opts= phrase.mutable_options();
    opts->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("q"));
    opts->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("u"));
    opts->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("i"));
    opts->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("c"));
    opts->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("k"));

    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER AnalYZER(phrase(d[42], 'quick'), "
        "'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER phrase(d[42], 'quick', 'test_analyzer') RETURN "
        "d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER AnalYZER(phrase(d[42], [ 'quick' ]), "
        "'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER phrase(d[42], [ 'quick' ], 'test_analyzer') "
        "RETURN d",
        expected);
  }

  // without offset, custom analyzer, expressions
  // quick
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("value", arangodb::aql::AqlValue("qui"));
    ctx.vars.emplace("analyzer", arangodb::aql::AqlValue("test_"));

    irs::Or expected;
    auto& phrase = expected.add<irs::by_phrase>();
    *phrase.mutable_field() = mangleString("name", "test_analyzer");
    auto* opts= phrase.mutable_options();
    opts->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("q"));
    opts->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("u"));
    opts->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("i"));
    opts->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("c"));
    opts->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("k"));

    assertFilterSuccess(
        vocbase(),
        "LET value='qui' LET analyzer='test_' FOR d IN myView FILTER "
        "AnAlYzEr(phrase(d.name, CONCAT(value,'ck')), CONCAT(analyzer, "
        "'analyzer')) RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET value='qui' LET analyzer='test_' FOR d IN myView FILTER "
        "AnAlYzEr(phrase(d['name'], CONCAT(value, 'ck')), CONCAT(analyzer, "
        "'analyzer')) RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET value='qui' LET analyzer='test_' FOR d IN myView FILTER "
        "AnALYzEr(phrase(d.name, [ CONCAT(value,'ck') ]), CONCAT(analyzer, "
        "'analyzer')) RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET value='qui' LET analyzer='test_' FOR d IN myView FILTER "
        "AnAlYzEr(phrase(d['name'], [ CONCAT(value, 'ck') ]), CONCAT(analyzer, "
        "'analyzer')) RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET value='qui' LET analyzer='test_' FOR d IN myView FILTER "
        "AnALYzEr(phRase(d.name, CONCAT(value, 'ck')), CONCAT(analyzer, "
        "'analyzer')) RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET value='qui' LET analyzer='test_' FOR d IN myView FILTER "
        "AnAlYZEr(phRase(d['name'], CONCAT(value, 'ck')), CONCAT(analyzer, "
        "'analyzer')) RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET value='qui' LET analyzer='test_' FOR d IN myView FILTER "
        "AnAlYzEr(phRase(d.name, [ CONCAT(value, 'ck') ]), CONCAT(analyzer, "
        "'analyzer')) RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET value='qui' LET analyzer='test_' FOR d IN myView FILTER "
        "AnAlYzEr(phRase(d['name'], [ CONCAT(value, 'ck') ]), CONCAT(analyzer, "
        "'analyzer')) RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET value='qui' LET analyzer='test_' FOR d IN myView FILTER "
        "phrase(d.name, CONCAT(value,'ck'), CONCAT(analyzer, 'analyzer')) "
        "RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET value='qui' LET analyzer='test_' FOR d IN myView FILTER "
        "phrase(d['name'], CONCAT(value, 'ck'), CONCAT(analyzer, 'analyzer')) "
        "RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET value='qui' LET analyzer='test_' FOR d IN myView FILTER "
        "phrase(d.name, [ CONCAT(value,'ck') ], CONCAT(analyzer, 'analyzer')) "
        "RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET value='qui' LET analyzer='test_' FOR d IN myView FILTER "
        "phrase(d['name'], [ CONCAT(value, 'ck') ], CONCAT(analyzer, "
        "'analyzer')) RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET value='qui' LET analyzer='test_' FOR d IN myView FILTER "
        "phRase(d.name, CONCAT(value, 'ck'), CONCAT(analyzer, 'analyzer')) "
        "RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET value='qui' LET analyzer='test_' FOR d IN myView FILTER "
        "phRase(d['name'], CONCAT(value, 'ck'), CONCAT(analyzer, 'analyzer')) "
        "RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET value='qui' LET analyzer='test_' FOR d IN myView FILTER "
        "phRase(d.name, [ CONCAT(value, 'ck') ], CONCAT(analyzer, 'analyzer')) "
        "RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET value='qui' LET analyzer='test_' FOR d IN myView FILTER "
        "phRase(d['name'], [ CONCAT(value, 'ck') ], CONCAT(analyzer, "
        "'analyzer')) RETURN d",
        expected, &ctx);
  }

  // without offset, custom analyzer, invalid expressions
  // quick
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("value", arangodb::aql::AqlValue("qui"));
    ctx.vars.emplace("analyzer",
                     arangodb::aql::AqlValue(arangodb::aql::AqlValueHintBool{false}));

    assertFilterExecutionFail(
        vocbase(),
        "LET value='qui' LET analyzer='test_' FOR d IN myView FILTER "
        "ANALYZER(phrase(d.name, CONCAT(value,'ck')), analyzer) RETURN d",
        &ctx);
    assertFilterExecutionFail(
        vocbase(),
        "LET value='qui' LET analyzer='test_' FOR d IN myView FILTER "
        "ANALYZER(phrase(d['name'], CONCAT(value, 'ck')), analyzer) RETURN d",
        &ctx);
    assertFilterExecutionFail(
        vocbase(),
        "LET value='qui' LET analyzer='test_' FOR d IN myView FILTER "
        "ANALYZER(phrase(d.name, [ CONCAT(value,'ck') ]), analyzer) RETURN d",
        &ctx);
    assertFilterExecutionFail(
        vocbase(),
        "LET value='qui' LET analyzer='test_' FOR d IN myView FILTER "
        "ANALYZER(phrase(d['name'], [ CONCAT(value, 'ck') ]), analyzer) RETURN "
        "d",
        &ctx);
    assertFilterExecutionFail(
        vocbase(),
        "LET value='qui' LET analyzer='test_' FOR d IN myView FILTER "
        "ANALYZER(phRase(d.name, CONCAT(value, 'ck')), analyzer) RETURN d",
        &ctx);
    assertFilterExecutionFail(
        vocbase(),
        "LET value='qui' LET analyzer='test_' FOR d IN myView FILTER "
        "ANALYZER(phRase(d['name'], CONCAT(value, 'ck')), analyzer) RETURN d",
        &ctx);
    assertFilterExecutionFail(
        vocbase(),
        "LET value='qui' LET analyzer='test_' FOR d IN myView FILTER "
        "ANALYZER(phRase(d.name, [ CONCAT(value, 'ck') ]), analyzer) RETURN d",
        &ctx);
    assertFilterExecutionFail(
        vocbase(),
        "LET value='qui' LET analyzer='test_' FOR d IN myView FILTER "
        "ANALYZER(phRase(d['name'], [ CONCAT(value, 'ck') ]), analyzer) RETURN "
        "d",
        &ctx);
    assertFilterExecutionFail(
        vocbase(),
        "LET value='qui' LET analyzer='test_' FOR d IN myView FILTER "
        "phrase(d.name, CONCAT(value,'ck'), analyzer) RETURN d",
        &ctx);
    assertFilterExecutionFail(
        vocbase(),
        "LET value='qui' LET analyzer='test_' FOR d IN myView FILTER "
        "phrase(d['name'], CONCAT(value, 'ck'), analyzer) RETURN d",
        &ctx);
    assertFilterExecutionFail(
        vocbase(),
        "LET value='qui' LET analyzer='test_' FOR d IN myView FILTER "
        "phrase(d.name, [ CONCAT(value,'ck') ], analyzer) RETURN d",
        &ctx);
    assertFilterExecutionFail(
        vocbase(),
        "LET value='qui' LET analyzer='test_' FOR d IN myView FILTER "
        "phrase(d['name'], [ CONCAT(value, 'ck') ], analyzer) RETURN d",
        &ctx);
    assertFilterExecutionFail(
        vocbase(),
        "LET value='qui' LET analyzer='test_' FOR d IN myView FILTER "
        "phRase(d.name, CONCAT(value, 'ck'), analyzer) RETURN d",
        &ctx);
    assertFilterExecutionFail(
        vocbase(),
        "LET value='qui' LET analyzer='test_' FOR d IN myView FILTER "
        "phRase(d['name'], CONCAT(value, 'ck'), analyzer) RETURN d",
        &ctx);
    assertFilterExecutionFail(
        vocbase(),
        "LET value='qui' LET analyzer='test_' FOR d IN myView FILTER "
        "phRase(d.name, [ CONCAT(value, 'ck') ], analyzer) RETURN d",
        &ctx);
    assertFilterExecutionFail(
        vocbase(),
        "LET value='qui' LET analyzer='test_' FOR d IN myView FILTER "
        "phRase(d['name'], [ CONCAT(value, 'ck') ], analyzer) RETURN d",
        &ctx);
    assertFilterExecutionFail(
        vocbase(),
        "LET value='qui' LET analyzer='test_' FOR d IN myView FILTER "
        "analyzer(phRase(d['name'], [ CONCAT(value, 'ck') ], analyzer), "
        "'identity') RETURN d",
        &ctx);
  }

  // with offset, custom analyzer
  // quick brown
  {
    irs::Or expected;
    auto& phrase = expected.add<irs::by_phrase>();
    *phrase.mutable_field() = mangleString("name", "test_analyzer");
    auto* opts= phrase.mutable_options();
    opts->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("q"));
    opts->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("u"));
    opts->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("i"));
    opts->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("c"));
    opts->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("k"));
    opts->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("b"));
    opts->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("r"));
    opts->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("o"));
    opts->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("w"));
    opts->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("n"));

    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER aNALYZER(phrase(d.name, 'quick', 0, 'brown'), "
        "'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER aNALYZER(phrase(d.name, 'quick', 0.0, "
        "'brown'), 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER aNALYZER(phrase(d.name, 'quick', 0.5, "
        "'brown'), 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER aNALYZER(phrase(d.name, [ 'quick', 0, 'brown' "
        "]), 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER aNALYZER(phrase(d.name, [ 'quick', 0.0, "
        "'brown' ]), 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER aNALYZER(phrase(d.name, [ 'quick', 0.5, "
        "'brown' ]), 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER phrase(d.name, 'quick', 0, 'brown', "
        "'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER phrase(d.name, 'quick', 0.0, 'brown', "
        "'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER phrase(d.name, 'quick', 0.5, 'brown', "
        "'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER phrase(d.name, [ 'quick', 0, 'brown' ], "
        "'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER phrase(d.name, [ 'quick', 0.0, 'brown' ], "
        "'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER phrase(d.name, [ 'quick', 0.5, 'brown' ], "
        "'test_analyzer') RETURN d",
        expected);

    // wrong offset argument
    assertFilterFail(
        vocbase(),
        "FOR d IN myView FILTER Analyzer(phrase(d.name, 'quick', '0', "
        "'brown'), 'test_analyzer') RETURN d");
    assertFilterFail(
        vocbase(),
        "FOR d IN myView FILTER Analyzer(phrase(d.name, 'quick', null, "
        "'brown'), 'test_analyzer') RETURN d");
    assertFilterFail(
        vocbase(),
        "FOR d IN myView FILTER Analyzer(phrase(d.name, 'quick', true, "
        "'brown'), 'test_analyzer') RETURN d");
    assertFilterFail(
        vocbase(),
        "FOR d IN myView FILTER Analyzer(phrase(d.name, 'quick', false, "
        "'brown'), 'test_analyzer') RETURN d");
    assertFilterExecutionFail(
        vocbase(),
        "FOR d IN myView FILTER AnalYZER(phrase(d.name, 'quick', d.name, "
        "'brown'), 'test_analyzer') RETURN d",
        &ExpressionContextMock::EMPTY);
    assertFilterFail(
        vocbase(),
        "FOR d IN myView FILTER AnaLYZER(phrase(d.name, [ 'quick', null, "
        "'brown' ]), 'test_analyzer') RETURN d");
    assertFilterFail(
        vocbase(),
        "FOR d IN myView FILTER AnaLYZER(phrase(d.name, [ 'quick', true, "
        "'brown' ]), 'test_analyzer') RETURN d");
    assertFilterFail(
        vocbase(),
        "FOR d IN myView FILTER AnaLYZER(phrase(d.name, [ 'quick', false, "
        "'brown' ]), 'test_analyzer') RETURN d");
    assertFilterExecutionFail(
        vocbase(),
        "FOR d IN myView FILTER ANALYZER(phrase(d.name, [ 'quick', d.name, "
        "'brown' ]), 'test_analyzer') RETURN d",
        &ExpressionContextMock::EMPTY);
  }

  {
    irs::Or expected;
    auto& phrase = expected.add<irs::by_phrase>();
    *phrase.mutable_field() = mangleString("name", "test_analyzer");
    auto* opts= phrase.mutable_options();
    opts->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("q"));
    opts->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("u"));
    opts->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("i"));
    opts->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("c"));
    opts->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("k"));
    opts->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("0"));
    opts->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("b"));
    opts->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("r"));
    opts->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("o"));
    opts->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("w"));
    opts->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("n"));

    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER AnaLYZER(phrase(d.name, [ 'quick', '0', "
        "'brown' ]), 'test_analyzer') RETURN d", expected);
    assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER AnaLYZER(phrase(d.name,  'quick', '0', "
      "'brown'), 'test_analyzer') RETURN d");
  }

  // with offset, complex name, custom analyzer
  // quick <...> <...> <...> <...> <...> brown
  {
    irs::Or expected;
    auto& phrase = expected.add<irs::by_phrase>();
    *phrase.mutable_field() = mangleString("obj.name", "test_analyzer");
    auto* opts= phrase.mutable_options();
    opts->push_back<irs::by_term_options>( ).term = irs::ref_cast<irs::byte_type>(irs::string_ref("q"));
    opts->push_back<irs::by_term_options>( ).term = irs::ref_cast<irs::byte_type>(irs::string_ref("u"));
    opts->push_back<irs::by_term_options>( ).term = irs::ref_cast<irs::byte_type>(irs::string_ref("i"));
    opts->push_back<irs::by_term_options>( ).term = irs::ref_cast<irs::byte_type>(irs::string_ref("c"));
    opts->push_back<irs::by_term_options>( ).term = irs::ref_cast<irs::byte_type>(irs::string_ref("k"));
    opts->push_back<irs::by_term_options>(5).term = irs::ref_cast<irs::byte_type>(irs::string_ref("b"));
    opts->push_back<irs::by_term_options>( ).term = irs::ref_cast<irs::byte_type>(irs::string_ref("r"));
    opts->push_back<irs::by_term_options>( ).term = irs::ref_cast<irs::byte_type>(irs::string_ref("o"));
    opts->push_back<irs::by_term_options>( ).term = irs::ref_cast<irs::byte_type>(irs::string_ref("w"));
    opts->push_back<irs::by_term_options>( ).term = irs::ref_cast<irs::byte_type>(irs::string_ref("n"));

    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER analyzer(phrase(d['obj']['name'], 'quick', 5, "
        "'brown'), 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER analyzer(phrase(d.obj.name, 'quick', 5, "
        "'brown'), 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER analyzer(phrase(d.obj.name, 'quick', 5.0, "
        "'brown'), 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER analyzer(phrase(d.obj['name'], 'quick', 5.0, "
        "'brown'), 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER analyzer(phrase(d.obj.name, 'quick', 5.6, "
        "'brown'), 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER analyzer(phrase(d['obj']['name'], 'quick', "
        "5.5, 'brown'), 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER analyzer(phrase(d['obj']['name'], [ 'quick', "
        "5, 'brown' ]), 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER analyzer(phrase(d.obj.name, [ 'quick', 5, "
        "'brown' ]), 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER analyzer(phrase(d.obj.name, [ 'quick', 5.0, "
        "'brown' ]), 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER analyzer(phrase(d.obj['name'], [ 'quick', 5.0, "
        "'brown' ]), 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER analyzer(phrase(d.obj.name, [ 'quick', 5.6, "
        "'brown' ]), 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER analyzer(phrase(d['obj']['name'], [ 'quick', "
        "5.5, 'brown' ]), 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER phrase(d['obj']['name'], 'quick', 5, 'brown', "
        "'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER phrase(d.obj.name, 'quick', 5, 'brown', "
        "'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER phrase(d.obj.name, 'quick', 5.0, 'brown', "
        "'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER phrase(d.obj['name'], 'quick', 5.0, 'brown', "
        "'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER phrase(d.obj.name, 'quick', 5.6, 'brown', "
        "'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER phrase(d['obj']['name'], 'quick', 5.5, "
        "'brown', 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER phrase(d['obj']['name'], [ 'quick', 5, 'brown' "
        "], 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER phrase(d.obj.name, [ 'quick', 5, 'brown' ], "
        "'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER phrase(d.obj.name, [ 'quick', 5.0, 'brown' ], "
        "'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER phrase(d.obj['name'], [ 'quick', 5.0, 'brown' "
        "], 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER phrase(d.obj.name, [ 'quick', 5.6, 'brown' ], "
        "'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER phrase(d['obj']['name'], [ 'quick', 5.5, "
        "'brown' ], 'test_analyzer') RETURN d",
        expected);
  }

  // with offset, complex name, custom analyzer, boost
  // quick <...> <...> <...> <...> <...> brown
  {
    irs::Or expected;
    auto& phrase = expected.add<irs::by_phrase>();
    *phrase.mutable_field() = mangleString("obj.name", "test_analyzer");
    phrase.boost(3.0f);
    auto* opts= phrase.mutable_options();
    opts->push_back<irs::by_term_options>( ).term = irs::ref_cast<irs::byte_type>(irs::string_ref("q"));
    opts->push_back<irs::by_term_options>( ).term = irs::ref_cast<irs::byte_type>(irs::string_ref("u"));
    opts->push_back<irs::by_term_options>( ).term = irs::ref_cast<irs::byte_type>(irs::string_ref("i"));
    opts->push_back<irs::by_term_options>( ).term = irs::ref_cast<irs::byte_type>(irs::string_ref("c"));
    opts->push_back<irs::by_term_options>( ).term = irs::ref_cast<irs::byte_type>(irs::string_ref("k"));
    opts->push_back<irs::by_term_options>(5).term = irs::ref_cast<irs::byte_type>(irs::string_ref("b"));
    opts->push_back<irs::by_term_options>( ).term = irs::ref_cast<irs::byte_type>(irs::string_ref("r"));
    opts->push_back<irs::by_term_options>( ).term = irs::ref_cast<irs::byte_type>(irs::string_ref("o"));
    opts->push_back<irs::by_term_options>( ).term = irs::ref_cast<irs::byte_type>(irs::string_ref("w"));
    opts->push_back<irs::by_term_options>( ).term = irs::ref_cast<irs::byte_type>(irs::string_ref("n"));

    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER BOOST(analyzer(phrase(d['obj']['name'], "
        "'quick', 5, 'brown'), 'test_analyzer'), 3) RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER BOoST(analyzer(phrase(d.obj.name, 'quick', 5, "
        "'brown'), 'test_analyzer'), 2.9+0.1) RETURN d",
        expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER Boost(analyzer(phrase(d.obj.name, 'quick', "
        "5.0, 'brown'), 'test_analyzer'), 3.0) RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER BOOST(phrase(d['obj']['name'], 'quick', 5, "
        "'brown', 'test_analyzer'), 3) RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER BOoST(phrase(d.obj.name, 'quick', 5, 'brown', "
        "'test_analyzer'), 2.9+0.1) RETURN d",
        expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER Boost(phrase(d.obj.name, 'quick', 5.0, "
        "'brown', 'test_analyzer'), 3.0) RETURN d",
        expected);
  }

  // with offset, complex name with offset, custom analyzer
  // quick <...> <...> <...> <...> <...> brown
  {
    irs::Or expected;
    auto& phrase = expected.add<irs::by_phrase>();
    *phrase.mutable_field() = mangleString("obj[3].name[1]", "test_analyzer");
    auto* opts= phrase.mutable_options();
    opts->push_back<irs::by_term_options>( ).term = irs::ref_cast<irs::byte_type>(irs::string_ref("q"));
    opts->push_back<irs::by_term_options>( ).term = irs::ref_cast<irs::byte_type>(irs::string_ref("u"));
    opts->push_back<irs::by_term_options>( ).term = irs::ref_cast<irs::byte_type>(irs::string_ref("i"));
    opts->push_back<irs::by_term_options>( ).term = irs::ref_cast<irs::byte_type>(irs::string_ref("c"));
    opts->push_back<irs::by_term_options>( ).term = irs::ref_cast<irs::byte_type>(irs::string_ref("k"));
    opts->push_back<irs::by_term_options>(5).term = irs::ref_cast<irs::byte_type>(irs::string_ref("b"));
    opts->push_back<irs::by_term_options>( ).term = irs::ref_cast<irs::byte_type>(irs::string_ref("r"));
    opts->push_back<irs::by_term_options>( ).term = irs::ref_cast<irs::byte_type>(irs::string_ref("o"));
    opts->push_back<irs::by_term_options>( ).term = irs::ref_cast<irs::byte_type>(irs::string_ref("w"));
    opts->push_back<irs::by_term_options>( ).term = irs::ref_cast<irs::byte_type>(irs::string_ref("n"));

    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER Analyzer(phrase(d['obj'][3].name[1], 'quick', "
        "5, 'brown'), 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER Analyzer(phrase(d.obj[3].name[1], 'quick', 5, "
        "'brown'), 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER Analyzer(phrase(d.obj[3].name[1], 'quick', "
        "5.0, 'brown'), 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER Analyzer(phrase(d.obj[3]['name'][1], 'quick', "
        "5.0, 'brown'), 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER Analyzer(phrase(d.obj[3].name[1], 'quick', "
        "5.5, 'brown'), 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER Analyzer(phrase(d['obj'][3]['name'][1], "
        "'quick', 5.5, 'brown'), 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER Analyzer(phrase(d['obj'][3].name[1], [ "
        "'quick', 5, 'brown' ]), 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER Analyzer(phrase(d.obj[3].name[1], [ 'quick', "
        "5, 'brown' ]), 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER Analyzer(phrase(d.obj[3].name[1], [ 'quick', "
        "5.0, 'brown' ]), 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER Analyzer(phrase(d.obj[3]['name'][1], [ "
        "'quick', 5.0, 'brown' ]), 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER Analyzer(phrase(d.obj[3].name[1], [ 'quick', "
        "5.5, 'brown' ]), 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER Analyzer(phrase(d['obj'][3]['name'][1], [ "
        "'quick', 5.5, 'brown' ]), 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER phrase(d['obj'][3].name[1], 'quick', 5, "
        "'brown', 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER phrase(d.obj[3].name[1], 'quick', 5, 'brown', "
        "'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER phrase(d.obj[3].name[1], 'quick', 5.0, "
        "'brown', 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER phrase(d.obj[3]['name'][1], 'quick', 5.0, "
        "'brown', 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER phrase(d.obj[3].name[1], 'quick', 5.5, "
        "'brown', 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER phrase(d['obj'][3]['name'][1], 'quick', 5.5, "
        "'brown', 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER phrase(d['obj'][3].name[1], [ 'quick', 5, "
        "'brown' ], 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER phrase(d.obj[3].name[1], [ 'quick', 5, 'brown' "
        "], 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER phrase(d.obj[3].name[1], [ 'quick', 5.0, "
        "'brown' ], 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER phrase(d.obj[3]['name'][1], [ 'quick', 5.0, "
        "'brown' ], 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER phrase(d.obj[3].name[1], [ 'quick', 5.5, "
        "'brown' ], 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER phrase(d['obj'][3]['name'][1], [ 'quick', 5.5, "
        "'brown' ], 'test_analyzer') RETURN d",
        expected);
  }

  // with offset, complex name, custom analyzer
  // quick <...> <...> <...> <...> <...> brown
  {
    irs::Or expected;
    auto& phrase = expected.add<irs::by_phrase>();
    *phrase.mutable_field() = mangleString("[5].obj.name[100]", "test_analyzer");
    auto* opts= phrase.mutable_options();
    opts->push_back<irs::by_term_options>( ).term = irs::ref_cast<irs::byte_type>(irs::string_ref("q"));
    opts->push_back<irs::by_term_options>( ).term = irs::ref_cast<irs::byte_type>(irs::string_ref("u"));
    opts->push_back<irs::by_term_options>( ).term = irs::ref_cast<irs::byte_type>(irs::string_ref("i"));
    opts->push_back<irs::by_term_options>( ).term = irs::ref_cast<irs::byte_type>(irs::string_ref("c"));
    opts->push_back<irs::by_term_options>( ).term = irs::ref_cast<irs::byte_type>(irs::string_ref("k"));
    opts->push_back<irs::by_term_options>(5).term = irs::ref_cast<irs::byte_type>(irs::string_ref("b"));
    opts->push_back<irs::by_term_options>( ).term = irs::ref_cast<irs::byte_type>(irs::string_ref("r"));
    opts->push_back<irs::by_term_options>( ).term = irs::ref_cast<irs::byte_type>(irs::string_ref("o"));
    opts->push_back<irs::by_term_options>( ).term = irs::ref_cast<irs::byte_type>(irs::string_ref("w"));
    opts->push_back<irs::by_term_options>( ).term = irs::ref_cast<irs::byte_type>(irs::string_ref("n"));

    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER ANALYZER(phrase(d[5]['obj'].name[100], "
        "'quick', 5, 'brown'), 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER ANALYZER(phrase(d[5].obj.name[100], 'quick', "
        "5, 'brown'), 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER ANALYZER(phrase(d[5].obj.name[100], 'quick', "
        "5.0, 'brown'), 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER ANALYZER(phrase(d[5].obj['name'][100], "
        "'quick', 5.0, 'brown'), 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER ANALYZER(phrase(d[5].obj.name[100], 'quick', "
        "5.5, 'brown'), 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER ANALYZER(phrase(d[5]['obj']['name'][100], "
        "'quick', 5.5, 'brown'), 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER ANALYZER(phrase(d[5]['obj'].name[100], [ "
        "'quick', 5, 'brown' ]), 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER ANALYZER(phrase(d[5].obj.name[100], [ 'quick', "
        "5, 'brown' ]), 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER ANALYZER(phrase(d[5].obj.name[100], [ 'quick', "
        "5.0, 'brown' ]), 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER ANALYZER(phrase(d[5].obj['name'][100], [ "
        "'quick', 5.0, 'brown' ]), 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER ANALYZER(phrase(d[5].obj.name[100], [ 'quick', "
        "5.5, 'brown' ]), 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER ANALYZER(phrase(d[5]['obj']['name'][100], [ "
        "'quick', 5.5, 'brown' ]), 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER phrase(d[5]['obj'].name[100], 'quick', 5, "
        "'brown', 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER phrase(d[5].obj.name[100], 'quick', 5, "
        "'brown', 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER phrase(d[5].obj.name[100], 'quick', 5.0, "
        "'brown', 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER phrase(d[5].obj['name'][100], 'quick', 5.0, "
        "'brown', 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER phrase(d[5].obj.name[100], 'quick', 5.5, "
        "'brown', 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER phrase(d[5]['obj']['name'][100], 'quick', 5.5, "
        "'brown', 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER phrase(d[5]['obj'].name[100], [ 'quick', 5, "
        "'brown' ], 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER phrase(d[5].obj.name[100], [ 'quick', 5, "
        "'brown' ], 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER phrase(d[5].obj.name[100], [ 'quick', 5.0, "
        "'brown' ], 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER phrase(d[5].obj['name'][100], [ 'quick', 5.0, "
        "'brown' ], 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER phrase(d[5].obj.name[100], [ 'quick', 5.5, "
        "'brown' ], 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER phrase(d[5]['obj']['name'][100], [ 'quick', "
        "5.5, 'brown' ], 'test_analyzer') RETURN d",
        expected);
  }

  // multiple offsets, complex name, custom analyzer
  // quick <...> <...> <...> brown <...> <...> fox jumps
  {
    irs::Or expected;
    auto& phrase = expected.add<irs::by_phrase>();
    *phrase.mutable_field() = mangleString("obj.properties.id.name", "test_analyzer");
    auto* opts= phrase.mutable_options();
    opts->push_back<irs::by_term_options>( ).term = irs::ref_cast<irs::byte_type>(irs::string_ref("q"));
    opts->push_back<irs::by_term_options>( ).term = irs::ref_cast<irs::byte_type>(irs::string_ref("u"));
    opts->push_back<irs::by_term_options>( ).term = irs::ref_cast<irs::byte_type>(irs::string_ref("i"));
    opts->push_back<irs::by_term_options>( ).term = irs::ref_cast<irs::byte_type>(irs::string_ref("c"));
    opts->push_back<irs::by_term_options>( ).term = irs::ref_cast<irs::byte_type>(irs::string_ref("k"));
    opts->push_back<irs::by_term_options>(3).term = irs::ref_cast<irs::byte_type>(irs::string_ref("b"));
    opts->push_back<irs::by_term_options>( ).term = irs::ref_cast<irs::byte_type>(irs::string_ref("r"));
    opts->push_back<irs::by_term_options>( ).term = irs::ref_cast<irs::byte_type>(irs::string_ref("o"));
    opts->push_back<irs::by_term_options>( ).term = irs::ref_cast<irs::byte_type>(irs::string_ref("w"));
    opts->push_back<irs::by_term_options>( ).term = irs::ref_cast<irs::byte_type>(irs::string_ref("n"));
    opts->push_back<irs::by_term_options>(2).term = irs::ref_cast<irs::byte_type>(irs::string_ref("f"));
    opts->push_back<irs::by_term_options>( ).term = irs::ref_cast<irs::byte_type>(irs::string_ref("o"));
    opts->push_back<irs::by_term_options>( ).term = irs::ref_cast<irs::byte_type>(irs::string_ref("x"));
    opts->push_back<irs::by_term_options>( ).term = irs::ref_cast<irs::byte_type>(irs::string_ref("j"));
    opts->push_back<irs::by_term_options>( ).term = irs::ref_cast<irs::byte_type>(irs::string_ref("u"));
    opts->push_back<irs::by_term_options>( ).term = irs::ref_cast<irs::byte_type>(irs::string_ref("m"));
    opts->push_back<irs::by_term_options>( ).term = irs::ref_cast<irs::byte_type>(irs::string_ref("p"));
    opts->push_back<irs::by_term_options>( ).term = irs::ref_cast<irs::byte_type>(irs::string_ref("s"));

    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER analyzer(phrase(d.obj.properties.id.name, "
        "'quick', 3, 'brown', 2, 'fox', 0, 'jumps'), 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER analyzer(phrase(d.obj.properties.id.name, "
        "'quick', 3.0, 'brown', 2, 'fox', 0, 'jumps'), 'test_analyzer') RETURN "
        "d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER analyzer(phrase(d.obj.properties.id['name'], "
        "'quick', 3.0, 'brown', 2, 'fox', 0, 'jumps'), 'test_analyzer') RETURN "
        "d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER analyzer(phrase(d.obj.properties.id.name, "
        "'quick', 3.6, 'brown', 2, 'fox', 0, 'jumps'), 'test_analyzer') RETURN "
        "d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER analyzer(phrase(d.obj['properties'].id.name, "
        "'quick', 3.6, 'brown', 2, 'fox', 0, 'jumps'), 'test_analyzer') RETURN "
        "d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER analyzer(phrase(d.obj.properties.id.name, "
        "'quick', 3, 'brown', 2.0, 'fox', 0, 'jumps'), 'test_analyzer') RETURN "
        "d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER analyzer(phrase(d.obj.properties.id.name, "
        "'quick', 3, 'brown', 2.5, 'fox', 0.0, 'jumps'), 'test_analyzer') "
        "RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER analyzer(phrase(d.obj.properties.id.name, "
        "'quick', 3.2, 'brown', 2.0, 'fox', 0.0, 'jumps'), 'test_analyzer') "
        "RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER "
        "analyzer(phrase(d['obj']['properties']['id']['name'], 'quick', 3.2, "
        "'brown', 2.0, 'fox', 0.0, 'jumps'), 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER analyzer(phrase(d.obj.properties.id.name, [ "
        "'quick', 3, 'brown', 2, 'fox', 0, 'jumps' ]), 'test_analyzer') RETURN "
        "d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER analyzer(phrase(d.obj.properties.id.name, [ "
        "'quick', 3.0, 'brown', 2, 'fox', 0, 'jumps' ]), 'test_analyzer') "
        "RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER analyzer(phrase(d.obj.properties.id['name'], [ "
        "'quick', 3.0, 'brown', 2, 'fox', 0, 'jumps' ]), 'test_analyzer') "
        "RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER analyzer(phrase(d.obj.properties.id.name, [ "
        "'quick', 3.6, 'brown', 2, 'fox', 0, 'jumps' ]), 'test_analyzer') "
        "RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER analyzer(phrase(d.obj['properties'].id.name, [ "
        "'quick', 3.6, 'brown', 2, 'fox', 0, 'jumps' ]), 'test_analyzer') "
        "RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER analyzer(phrase(d.obj.properties.id.name, [ "
        "'quick', 3, 'brown', 2.0, 'fox', 0, 'jumps' ]), 'test_analyzer') "
        "RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER analyzer(phrase(d.obj.properties.id.name, [ "
        "'quick', 3, 'brown', 2.5, 'fox', 0.0, 'jumps' ]), 'test_analyzer') "
        "RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER analyzer(phrase(d.obj.properties.id.name, [ "
        "'quick', 3.2, 'brown', 2.0, 'fox', 0.0, 'jumps' ]), 'test_analyzer') "
        "RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER "
        "analyzer(phrase(d['obj']['properties']['id']['name'], [ 'quick', 3.2, "
        "'brown', 2.0, 'fox', 0.0, 'jumps']), 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER phrase(d.obj.properties.id.name, 'quick', 3, "
        "'brown', 2, 'fox', 0, 'jumps', 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER phrase(d.obj.properties.id.name, 'quick', 3.0, "
        "'brown', 2, 'fox', 0, 'jumps', 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER phrase(d.obj.properties.id['name'], 'quick', "
        "3.0, 'brown', 2, 'fox', 0, 'jumps', 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER phrase(d.obj.properties.id.name, 'quick', 3.6, "
        "'brown', 2, 'fox', 0, 'jumps', 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER phrase(d.obj['properties'].id.name, 'quick', "
        "3.6, 'brown', 2, 'fox', 0, 'jumps', 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER phrase(d.obj.properties.id.name, 'quick', 3, "
        "'brown', 2.0, 'fox', 0, 'jumps', 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER phrase(d.obj.properties.id.name, 'quick', 3, "
        "'brown', 2.5, 'fox', 0.0, 'jumps', 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER phrase(d.obj.properties.id.name, 'quick', 3.2, "
        "'brown', 2.0, 'fox', 0.0, 'jumps', 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER phrase(d['obj']['properties']['id']['name'], "
        "'quick', 3.2, 'brown', 2.0, 'fox', 0.0, 'jumps', 'test_analyzer') "
        "RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER phrase(d.obj.properties.id.name, [ 'quick', 3, "
        "'brown', 2, 'fox', 0, 'jumps' ], 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER phrase(d.obj.properties.id.name, [ 'quick', "
        "3.0, 'brown', 2, 'fox', 0, 'jumps' ], 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER phrase(d.obj.properties.id['name'], [ 'quick', "
        "3.0, 'brown', 2, 'fox', 0, 'jumps' ], 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER phrase(d.obj.properties.id.name, [ 'quick', "
        "3.6, 'brown', 2, 'fox', 0, 'jumps' ], 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER phrase(d.obj['properties'].id.name, [ 'quick', "
        "3.6, 'brown', 2, 'fox', 0, 'jumps' ], 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER phrase(d.obj.properties.id.name, [ 'quick', 3, "
        "'brown', 2.0, 'fox', 0, 'jumps' ], 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER phrase(d.obj.properties.id.name, [ 'quick', 3, "
        "'brown', 2.5, 'fox', 0.0, 'jumps' ], 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER phrase(d.obj.properties.id.name, [ 'quick', "
        "3.2, 'brown', 2.0, 'fox', 0.0, 'jumps' ], 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER phrase(d['obj']['properties']['id']['name'], [ "
        "'quick', 3.2, 'brown', 2.0, 'fox', 0.0, 'jumps'], 'test_analyzer') "
        "RETURN d",
        expected);

    // wrong value
    assertFilterExecutionFail(
        vocbase(),
        "FOR d IN myView FILTER analyzer(phrase(d.obj.properties.id.name, "
        "'quick', 3, d.brown, 2, 'fox', 0, 'jumps'), 'test_analyzer') RETURN d",
        &ExpressionContextMock::EMPTY);
    assertFilterFail(
        vocbase(),
        "FOR d IN myView FILTER analyzer(phrase(d.obj.properties.id.name, "
        "'quick', 3, 2, 2, 'fox', 0, 'jumps'), 'test_analyzer') RETURN d");
    assertFilterFail(
        vocbase(),
        "FOR d IN myView FILTER analyzer(phrase(d.obj.properties.id.name, "
        "'quick', 3, 2.5, 2, 'fox', 0, 'jumps'), 'test_analyzer') RETURN d");
    assertFilterFail(
        vocbase(),
        "FOR d IN myView FILTER analyzer(phrase(d.obj.properties.id.name, "
        "'quick', 3, null, 2, 'fox', 0, 'jumps'), 'test_analyzer') RETURN d");
    assertFilterFail(
        vocbase(),
        "FOR d IN myView FILTER analyzer(phrase(d.obj.properties.id.name, "
        "'quick', 3, true, 2, 'fox', 0, 'jumps'), 'test_analyzer') RETURN d");
    assertFilterFail(
        vocbase(),
        "FOR d IN myView FILTER analyzer(phrase(d.obj.properties.id.name, "
        "'quick', 3, false, 2, 'fox', 0, 'jumps'), 'test_analyzer') RETURN d");
    assertFilterExecutionFail(
        vocbase(),
        "FOR d IN myView FILTER analyzer(phrase(d.obj.properties.id.name, "
        "'quick', 3, 'brown', 2, 'fox', 0, d), 'test_analyzer') RETURN d",
        &ExpressionContextMock::EMPTY);
    assertFilterExecutionFail(
        vocbase(),
        "FOR d IN myView FILTER analyzer(phrase(d.obj.properties.id.name, [ "
        "'quick', 3, d.brown, 2, 'fox', 0, 'jumps' ]), 'test_analyzer') RETURN "
        "d",
        &ExpressionContextMock::EMPTY);
    assertFilterFail(
        vocbase(),
        "FOR d IN myView FILTER analyZer(phrase(d.obj.properties.id.name, [ "
        "'quick', 3, 2, 2, 'fox', 0, 'jumps']), 'test_analyzer') RETURN d");
    assertFilterFail(
        vocbase(),
        "FOR d IN myView FILTER analyZer(phrase(d.obj.properties.id.name, [ "
        "'quick', 3, 2.5, 2, 'fox', 0, 'jumps' ]), 'test_analyzer') RETURN d");
    assertFilterFail(
        vocbase(),
        "FOR d IN myView FILTER analyZer(phrase(d.obj.properties.id.name, [ "
        "'quick', 3, null, 2, 'fox', 0, 'jumps' ]), 'test_analyzer') RETURN d");
    assertFilterFail(
        vocbase(),
        "FOR d IN myView FILTER analyZer(phrase(d.obj.properties.id.name, [ "
        "'quick', 3, true, 2, 'fox', 0, 'jumps' ]), 'test_analyzer') RETURN d");
    assertFilterFail(
        vocbase(),
        "FOR d IN myView FILTER analyZer(phrase(d.obj.properties.id.name, [ "
        "'quick', 3, false, 2, 'fox', 0, 'jumps' ]), 'test_analyzer') RETURN "
        "d");
    assertFilterExecutionFail(
        vocbase(),
        "FOR d IN myView FILTER analYZER(phrase(d.obj.properties.id.name, [ "
        "'quick', 3, 'brown', 2, 'fox', 0, d ]), 'test_analyzer') RETURN d",
        &ExpressionContextMock::EMPTY);

    // wrong offset argument
    assertFilterFail(
        vocbase(),
        "FOR d IN myView FILTER analyzer(phrase(d.obj.properties.id.name, "
        "'quick', 3, 'brown', '2', 'fox', 0, 'jumps'), 'test_analyzer') RETURN "
        "d");
    assertFilterFail(
        vocbase(),
        "FOR d IN myView FILTER analyzer(phrase(d.obj.properties.id.name, "
        "'quick', 3, 'brown', null, 'fox', 0, 'jumps'), 'test_analyzer') "
        "RETURN d");
    assertFilterFail(
        vocbase(),
        "FOR d IN myView FILTER analyzer(phrase(d.obj.properties.id.name, "
        "'quick', 3, 'brown', true, 'fox', 0, 'jumps'), 'test_analyzer') "
        "RETURN d");
    assertFilterFail(
        vocbase(),
        "FOR d IN myView FILTER analyzer(phrase(d.obj.properties.id.name, "
        "'quick', 3, 'brown', false, 'fox', 0, 'jumps'), 'test_analyzer') "
        "RETURN d");
    assertFilterFail(
        vocbase(),
        "FOR d IN myView FILTER analyzer(phrase(d.obj.properties.id.name, [ "
        "'quick', 3, 'brown', null, 'fox', 0, 'jumps' ]), 'test_analyzer') "
        "RETURN d");
    assertFilterFail(
        vocbase(),
        "FOR d IN myView FILTER analyzer(phrase(d.obj.properties.id.name, [ "
        "'quick', 3, 'brown', true, 'fox', 0, 'jumps' ]), 'test_analyzer') "
        "RETURN d");
    assertFilterFail(
        vocbase(),
        "FOR d IN myView FILTER analyzer(phrase(d.obj.properties.id.name, [ "
        "'quick', 3, 'brown', false, 'fox', 0, 'jumps' ]), 'test_analyzer') "
        "RETURN d");
  }

  // multiple offsets, complex name, custom analyzer, expressions
  // quick <...> <...> <...> brown <...> <...> fox jumps
  {
    irs::Or expected;
    auto& phrase = expected.add<irs::by_phrase>();
    *phrase.mutable_field() = mangleString("obj.properties.id.name", "test_analyzer");
    auto* opts= phrase.mutable_options();
    opts->push_back<irs::by_term_options>( ).term = irs::ref_cast<irs::byte_type>(irs::string_ref("q"));
    opts->push_back<irs::by_term_options>( ).term = irs::ref_cast<irs::byte_type>(irs::string_ref("u"));
    opts->push_back<irs::by_term_options>( ).term = irs::ref_cast<irs::byte_type>(irs::string_ref("i"));
    opts->push_back<irs::by_term_options>( ).term = irs::ref_cast<irs::byte_type>(irs::string_ref("c"));
    opts->push_back<irs::by_term_options>( ).term = irs::ref_cast<irs::byte_type>(irs::string_ref("k"));
    opts->push_back<irs::by_term_options>(3).term = irs::ref_cast<irs::byte_type>(irs::string_ref("b"));
    opts->push_back<irs::by_term_options>( ).term = irs::ref_cast<irs::byte_type>(irs::string_ref("r"));
    opts->push_back<irs::by_term_options>( ).term = irs::ref_cast<irs::byte_type>(irs::string_ref("o"));
    opts->push_back<irs::by_term_options>( ).term = irs::ref_cast<irs::byte_type>(irs::string_ref("w"));
    opts->push_back<irs::by_term_options>( ).term = irs::ref_cast<irs::byte_type>(irs::string_ref("n"));
    opts->push_back<irs::by_term_options>(2).term = irs::ref_cast<irs::byte_type>(irs::string_ref("f"));
    opts->push_back<irs::by_term_options>( ).term = irs::ref_cast<irs::byte_type>(irs::string_ref("o"));
    opts->push_back<irs::by_term_options>( ).term = irs::ref_cast<irs::byte_type>(irs::string_ref("x"));
    opts->push_back<irs::by_term_options>( ).term = irs::ref_cast<irs::byte_type>(irs::string_ref("j"));
    opts->push_back<irs::by_term_options>( ).term = irs::ref_cast<irs::byte_type>(irs::string_ref("u"));
    opts->push_back<irs::by_term_options>( ).term = irs::ref_cast<irs::byte_type>(irs::string_ref("m"));
    opts->push_back<irs::by_term_options>( ).term = irs::ref_cast<irs::byte_type>(irs::string_ref("p"));
    opts->push_back<irs::by_term_options>( ).term = irs::ref_cast<irs::byte_type>(irs::string_ref("s"));

    ExpressionContextMock ctx;
    ctx.vars.emplace("offset", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt(2)));
    ctx.vars.emplace("input", arangodb::aql::AqlValue("bro"));

    assertFilterSuccess(
        vocbase(),
        "LET offset=2 LET input='bro' FOR d IN myView FILTER "
        "analyzer(phrase(d.obj.properties.id.name, 'quick', offset+1, "
        "CONCAT(input, 'wn'), offset, 'fox', 0, 'jumps'), 'test_analyzer') "
        "RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET offset=2 LET input='bro' FOR d IN myView FILTER "
        "analyzer(phrase(d.obj.properties.id.name, 'quick', offset + 1.5, "
        "'brown', 2, 'fox', 0, 'jumps'), 'test_analyzer') RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET offset=2 LET input='bro' FOR d IN myView FILTER "
        "analyzer(phrase(d.obj.properties.id['name'], 'quick', 3.0, 'brown', "
        "offset, 'fox', 0, 'jumps'), 'test_analyzer') RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET offset=2 LET input='bro' FOR d IN myView FILTER "
        "analyzer(phrase(d.obj.properties.id.name, 'quick', 3.6, 'brown', 2, "
        "'fox', offset-2, 'jumps'), 'test_analyzer') RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET offset=2 LET input='bro' FOR d IN myView FILTER "
        "analyzer(phrase(d.obj['properties'].id.name, 'quick', 3.6, "
        "CONCAT(input, 'wn'), 2, 'fox', 0, 'jumps'), 'test_analyzer') RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET offset=2 LET input='bro' FOR d IN myView FILTER "
        "analyzer(phrase(d.obj.properties.id.name, 'quick', 3, 'brown', "
        "offset+0.5, 'fox', 0.0, 'jumps'), 'test_analyzer') RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET offset=2 LET input='bro' FOR d IN myView FILTER "
        "analyzer(phrase(d.obj.properties.id.name, [ 'quick', offset+1, "
        "CONCAT(input, 'wn'), offset, 'fox', 0, 'jumps' ]), 'test_analyzer') "
        "RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET offset=2 LET input='bro' FOR d IN myView FILTER "
        "analyzer(phrase(d.obj.properties.id.name, [ 'quick', offset + 1.5, "
        "'brown', 2, 'fox', 0, 'jumps' ]), 'test_analyzer') RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET offset=2 LET input='bro' FOR d IN myView FILTER "
        "analyzer(phrase(d.obj.properties.id['name'], [ 'quick', 3.0, 'brown', "
        "offset, 'fox', 0, 'jumps' ]), 'test_analyzer') RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET offset=2 LET input='bro' FOR d IN myView FILTER "
        "analyzer(phrase(d.obj.properties.id.name, [ 'quick', 3.6, 'brown', 2, "
        "'fox', offset-2, 'jumps' ]), 'test_analyzer') RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET offset=2 LET input='bro' FOR d IN myView FILTER "
        "analyzer(phrase(d.obj['properties'].id.name, [ 'quick', 3.6, "
        "CONCAT(input, 'wn'), 2, 'fox', 0, 'jumps' ]), 'test_analyzer') RETURN "
        "d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET offset=2 LET input='bro' FOR d IN myView FILTER "
        "analyzer(phrase(d.obj.properties.id.name, [ 'quick', 3, 'brown', "
        "offset+0.5, 'fox', 0.0, 'jumps' ]), 'test_analyzer') RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET offset=2 LET input='bro' FOR d IN myView FILTER "
        "phrase(d.obj.properties.id.name, 'quick', offset+1, CONCAT(input, "
        "'wn'), offset, 'fox', 0, 'jumps', 'test_analyzer') RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET offset=2 LET input='bro' FOR d IN myView FILTER "
        "phrase(d.obj.properties.id.name, 'quick', offset + 1.5, 'brown', 2, "
        "'fox', 0, 'jumps', 'test_analyzer') RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET offset=2 LET input='bro' FOR d IN myView FILTER "
        "phrase(d.obj.properties.id['name'], 'quick', 3.0, 'brown', offset, "
        "'fox', 0, 'jumps', 'test_analyzer') RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET offset=2 LET input='bro' FOR d IN myView FILTER "
        "phrase(d.obj.properties.id.name, 'quick', 3.6, 'brown', 2, 'fox', "
        "offset-2, 'jumps', 'test_analyzer') RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET offset=2 LET input='bro' FOR d IN myView FILTER "
        "phrase(d.obj['properties'].id.name, 'quick', 3.6, CONCAT(input, "
        "'wn'), 2, 'fox', 0, 'jumps', 'test_analyzer') RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET offset=2 LET input='bro' FOR d IN myView FILTER "
        "phrase(d.obj.properties.id.name, 'quick', 3, 'brown', offset+0.5, "
        "'fox', 0.0, 'jumps', 'test_analyzer') RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET offset=2 LET input='bro' FOR d IN myView FILTER "
        "phrase(d.obj.properties.id.name, [ 'quick', offset+1, CONCAT(input, "
        "'wn'), offset, 'fox', 0, 'jumps' ], 'test_analyzer') RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET offset=2 LET input='bro' FOR d IN myView FILTER "
        "phrase(d.obj.properties.id.name, [ 'quick', offset + 1.5, 'brown', 2, "
        "'fox', 0, 'jumps' ], 'test_analyzer') RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET offset=2 LET input='bro' FOR d IN myView FILTER "
        "phrase(d.obj.properties.id['name'], [ 'quick', 3.0, 'brown', offset, "
        "'fox', 0, 'jumps' ], 'test_analyzer') RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET offset=2 LET input='bro' FOR d IN myView FILTER "
        "phrase(d.obj.properties.id.name, [ 'quick', 3.6, 'brown', 2, 'fox', "
        "offset-2, 'jumps' ], 'test_analyzer') RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET offset=2 LET input='bro' FOR d IN myView FILTER "
        "phrase(d.obj['properties'].id.name, [ 'quick', 3.6, CONCAT(input, "
        "'wn'), 2, 'fox', 0, 'jumps' ], 'test_analyzer') RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET offset=2 LET input='bro' FOR d IN myView FILTER "
        "phrase(d.obj.properties.id.name, [ 'quick', 3, 'brown', offset+0.5, "
        "'fox', 0.0, 'jumps' ], 'test_analyzer') RETURN d",
        expected, &ctx);
  }
  {
    irs::Or expected;
    auto& phrase = expected.add<irs::by_phrase>();
    *phrase.mutable_field() = mangleString("obj.properties.id.name", "test_analyzer");
    auto* opts= phrase.mutable_options();
    opts->push_back<irs::by_term_options>( ).term = irs::ref_cast<irs::byte_type>(irs::string_ref("q"));
    opts->push_back<irs::by_term_options>( ).term = irs::ref_cast<irs::byte_type>(irs::string_ref("u"));
    opts->push_back<irs::by_term_options>( ).term = irs::ref_cast<irs::byte_type>(irs::string_ref("i"));
    opts->push_back<irs::by_term_options>( ).term = irs::ref_cast<irs::byte_type>(irs::string_ref("c"));
    opts->push_back<irs::by_term_options>( ).term = irs::ref_cast<irs::byte_type>(irs::string_ref("k"));
    opts->push_back<irs::by_term_options>(3).term = irs::ref_cast<irs::byte_type>(irs::string_ref("b"));
    opts->push_back<irs::by_term_options>( ).term = irs::ref_cast<irs::byte_type>(irs::string_ref("r"));
    opts->push_back<irs::by_term_options>( ).term = irs::ref_cast<irs::byte_type>(irs::string_ref("o"));
    opts->push_back<irs::by_term_options>( ).term = irs::ref_cast<irs::byte_type>(irs::string_ref("w"));
    opts->push_back<irs::by_term_options>( ).term = irs::ref_cast<irs::byte_type>(irs::string_ref("n"));
    opts->push_back<irs::by_term_options>( ).term = irs::ref_cast<irs::byte_type>(irs::string_ref("f"));
    opts->push_back<irs::by_term_options>( ).term = irs::ref_cast<irs::byte_type>(irs::string_ref("o"));
    opts->push_back<irs::by_term_options>( ).term = irs::ref_cast<irs::byte_type>(irs::string_ref("x"));
    opts->push_back<irs::by_term_options>( ).term = irs::ref_cast<irs::byte_type>(irs::string_ref("j"));
    opts->push_back<irs::by_term_options>( ).term = irs::ref_cast<irs::byte_type>(irs::string_ref("u"));
    opts->push_back<irs::by_term_options>( ).term = irs::ref_cast<irs::byte_type>(irs::string_ref("m"));
    opts->push_back<irs::by_term_options>( ).term = irs::ref_cast<irs::byte_type>(irs::string_ref("p"));
    opts->push_back<irs::by_term_options>( ).term = irs::ref_cast<irs::byte_type>(irs::string_ref("s"));

    ExpressionContextMock ctx;
    ctx.vars.emplace("offset", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt(2)));
    ctx.vars.emplace("input", arangodb::aql::AqlValue("bro"));

    // implicit zero offsets
    assertFilterSuccess(
      vocbase(),
      "LET offset=2 LET input='bro' FOR d IN myView FILTER "
      "analyzer(phrase(d.obj.properties.id.name, ['quick', offset+1, "
      "CONCAT(input, 'wn'), 'fox', 'jumps']), 'test_analyzer') "
      "RETURN d",
      expected, &ctx);

    // explicit zero offsets on top level
    assertFilterSuccess(
      vocbase(),
      "LET offset=2 LET input='bro' FOR d IN myView FILTER "
      "analyzer(phrase(d.obj.properties.id.name, ['quick'], offset+1, "
      "CONCAT(input, 'wn'), 0, ['f', 'o', 'x'], 0, ['j', 'u', 'mps']), 'test_analyzer') "
      "RETURN d",
      expected, &ctx);

    // recurring arrays not allowed
    assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER "
      "analyzer(phrase(d.obj.properties.id.name, ['quick'], 3, "
      "'123', 'wn', 0, ['f', 'o', 'x'], 0, [['j', ['u'], 'mps']]), 'test_analyzer') "
      "RETURN d", &ctx);

    assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER "
      "analyzer(phrase(d.obj.properties.id.name, ['quick', 3, "
      "'123', 'wn', 0, 'f', 'o', 'x', 0, [['j']], 'u', 'mps']), 'test_analyzer') "
      "RETURN d", &ctx);
  }

  {
    irs::Or expected;
    auto& phrase = expected.add<irs::by_phrase>();
    *phrase.mutable_field() = mangleString("obj.properties.id.name", "test_analyzer");
    auto* opts= phrase.mutable_options();
    opts->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("q"));
    opts->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("u"));
    opts->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("i"));
    {
      auto& part = opts->push_back<irs::by_prefix_options>();
      part.term = irs::ref_cast<irs::byte_type>(irs::string_ref("c"));
      part.scored_terms_limit = arangodb::iresearch::FilterConstants::DefaultScoringTermsLimit;
    }
    opts->push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("k"));
    {
      auto& part = opts->push_back<irs::by_wildcard_options>(3);
      part.term = irs::ref_cast<irs::byte_type>(irs::string_ref("b"));
      part.scored_terms_limit = arangodb::iresearch::FilterConstants::DefaultScoringTermsLimit;
    }
    opts->push_back<irs::by_term_options>( ).term = irs::ref_cast<irs::byte_type>(irs::string_ref("r"));
    {
      auto& part = opts->push_back<irs::by_range_options>();
      part.range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("n"));
      part.range.min_type = irs::BoundType::EXCLUSIVE;
      part.range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("p"));
      part.range.max_type = irs::BoundType::EXCLUSIVE;
      part.scored_terms_limit = arangodb::iresearch::FilterConstants::DefaultScoringTermsLimit;
    }
    opts->push_back<irs::by_term_options>( ).term = irs::ref_cast<irs::byte_type>(irs::string_ref("w"));
    opts->push_back<irs::by_term_options>( ).term = irs::ref_cast<irs::byte_type>(irs::string_ref("n"));
    {
      auto& part = opts->push_back<irs::by_edit_distance_filter_options>();
      part.max_distance = 1;
      part.with_transpositions = true;
      part.provider = &arangodb::iresearch::getParametricDescription;
      part.term = irs::ref_cast<irs::byte_type>(irs::string_ref("p"));
    }
    opts->push_back<irs::by_term_options>( ).term = irs::ref_cast<irs::byte_type>(irs::string_ref("o"));
    opts->push_back<irs::by_term_options>( ).term = irs::ref_cast<irs::byte_type>(irs::string_ref("x"));
    {
      auto& part = opts->push_back<irs::by_terms_options>();
      part.terms.emplace(irs::ref_cast<irs::byte_type>(irs::string_ref("g")));
      part.terms.emplace(irs::ref_cast<irs::byte_type>(irs::string_ref("j")));
    }
    opts->push_back<irs::by_term_options>( ).term = irs::ref_cast<irs::byte_type>(irs::string_ref("u"));
    opts->push_back<irs::by_term_options>( ).term = irs::ref_cast<irs::byte_type>(irs::string_ref("m"));
    {
      auto& part = opts->push_back<irs::by_terms_options>();
      part.terms.emplace(irs::ref_cast<irs::byte_type>(irs::string_ref("b")));
      part.terms.emplace(irs::ref_cast<irs::byte_type>(irs::string_ref("p")));
    }
    opts->push_back<irs::by_term_options>( ).term = irs::ref_cast<irs::byte_type>(irs::string_ref("s"));

    ExpressionContextMock ctx;
    ctx.vars.emplace("offset", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt(2)));
    ctx.vars.emplace("input_st", arangodb::aql::AqlValue("q"));
    ctx.vars.emplace("input_pt", arangodb::aql::AqlValue("c"));
    ctx.vars.emplace("input_wt", arangodb::aql::AqlValue("b"));
    ctx.vars.emplace("input_lt", arangodb::aql::AqlValue("p"));
    ctx.vars.emplace("input_ct", arangodb::aql::AqlValue("g"));
    ctx.vars.emplace("input_ct2", arangodb::aql::AqlValue("b"));
    ctx.vars.emplace("input_rt", arangodb::aql::AqlValue("n"));

    // TERM, STARTS_WITH, WILDCARD, LEVENSHTEIN_MATCH, TERMS, IN_RANGE
    assertFilterSuccess(
      vocbase(),
      "FOR d IN myView FILTER "
      "analyzer(phrase(d.obj.properties.id.name, [{term: 'q'}, 'ui', {starts_with: 'c'}, 'k', 3, "
      "{wildcard: 'b'}, 'r', {in_range: ['n', 'p', false, false]}, 'wn', {levenshtein_match: ['p', 1, true, 0]}, 'ox', {terms: ['g', 'j']}, 'um', ['b', 'p'], 's']), 'test_analyzer') "
      "RETURN d",
      expected, &ctx);

    // TERM, STARTS_WITH, WILDCARD, LEVENSHTEIN_MATCH, TERMS, IN_RANGE with variables
    assertFilterSuccess(
      vocbase(),
      "LET offset=2 LET input_st='q' LET input_pt='c' LET input_wt='b' LET input_lt='p' LET input_ct='g' LET input_ct2='b' LET input_rt='n' "
      "FOR d IN myView FILTER analyzer(phrase(d.obj.properties.id.name, [{term: input_st}, 'ui', {starts_with: input_pt}, 'k', offset+1, "
        "{'wildcard': input_wt}, 'r', {in_range: [input_rt, 'p', false, false]}, 'wn', {levenshtein_match: [input_lt, 1, true, 0]}, 'ox', "
        "{terms: [input_ct, 'j']}, 'um', [input_ct2, 'p'], 's']), 'test_analyzer') "
      "RETURN d",
      expected, &ctx);

    assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER "
      "analyzer(phrase(d.obj.properties.id.name, [{term: {t: 'q'}}, 'ui', {starts_with: 'c'}, 'k', 3, "
      "{wildcard: 'b'}, 'r', {in_range: ['n', 'p', false, false]}, 'wn', {levenshtein_match: ['p', 1, true, 0]}, 'ox', {terms: ['g', 'j']}, 'umps']), 'test_analyzer') "
      "RETURN d",
      &ctx);
    assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER "
      "analyzer(phrase(d.obj.properties.id.name, [{term: true}, 'ui', {starts_with: 'c'}, 'k', 3, "
      "{wildcard: 'b'}, 'r', {in_range: ['n', 'p', false, false]}, 'wn', {levenshtein_match: ['p', 1, true, 0]}, 'ox', {terms: ['g', 'j']}, 'umps']), 'test_analyzer') "
      "RETURN d",
      &ctx);
    assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER "
      "analyzer(phrase(d.obj.properties.id.name, [{term: 1}, 'ui', {starts_with: 'c'}, 'k', 3, "
      "{wildcard: 'b'}, 'r', {in_range: ['n', 'p', false, false]}, 'wn', {levenshtein_match: ['p', 1, true, 0]}, 'ox', {terms: ['g', 'j']}, 'umps']), 'test_analyzer') "
      "RETURN d",
      &ctx);
    assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER "
      "analyzer(phrase(d.obj.properties.id.name, [{term: 1.2}, 'ui', {starts_with: 'c'}, 'k', 3, "
      "{wildcard: 'b'}, 'r', {in_range: ['n', 'p', false, false]}, 'wn', {levenshtein_match: ['p', 1, true, 0]}, 'ox', {terms: ['g', 'j']}, 'umps']), 'test_analyzer') "
      "RETURN d",
      &ctx);
    assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER "
      "analyzer(phrase(d.obj.properties.id.name, [{term: null}, 'ui', {starts_with: 'c'}, 'k', 3, "
      "{wildcard: 'b'}, 'r', {in_range: ['n', 'p', false, false]}, 'wn', {levenshtein_match: ['p', 1, true, 0]}, 'ox', {terms: ['g', 'j']}, 'umps']), 'test_analyzer') "
      "RETURN d",
      &ctx);
    assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER "
      "analyzer(phrase(d.obj.properties.id.name, [{term: 'q'}, 'ui', {starts_with: {t: 'c'}}, 'k', 3, "
      "{wildcard: 'b'}, 'r', {in_range: ['n', 'p', false, false]}, 'wn', {levenshtein_match: ['p', 1, true, 0]}, 'ox', {terms: ['g', 'j']}, 'umps']), 'test_analyzer') "
      "RETURN d",
      &ctx);
    assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER "
      "analyzer(phrase(d.obj.properties.id.name, [{term: 'q'}, 'ui', {starts_with: true}, 'k', 3, "
      "{wildcard: 'b'}, 'r', {in_range: ['n', 'p', false, false]}, 'wn', {levenshtein_match: ['p', 1, true, 0]}, 'ox', {terms: ['g', 'j']}, 'umps']), 'test_analyzer') "
      "RETURN d",
      &ctx);
    assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER "
      "analyzer(phrase(d.obj.properties.id.name, [{term: 'q'}, 'ui', {starts_with: 1}, 'k', 3, "
      "{wildcard: 'b'}, 'r', {in_range: ['n', 'p', false, false]}, 'wn', {levenshtein_match: ['p', 1, true, 0]}, 'ox', {terms: ['g', 'j']}, 'umps']), 'test_analyzer') "
      "RETURN d",
      &ctx);
    assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER "
      "analyzer(phrase(d.obj.properties.id.name, [{term: 'q'}, 'ui', {starts_with: 1.2}, 'k', 3, "
      "{wildcard: 'b'}, 'r', {in_range: ['n', 'p', false, false]}, 'wn', {levenshtein_match: ['p', 1, true, 0]}, 'ox', {terms: ['g', 'j']}, 'umps']), 'test_analyzer') "
      "RETURN d",
      &ctx);
    assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER "
      "analyzer(phrase(d.obj.properties.id.name, [{term: 'q'}, 'ui', {starts_with: null}, 'k', 3, "
      "{wildcard: 'b'}, 'r', {in_range: ['n', 'p', false, false]}, 'wn', {levenshtein_match: ['p', 1, true, 0]}, 'ox', {terms: ['g', 'j']}, 'umps']), 'test_analyzer') "
      "RETURN d",
      &ctx);
    assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER "
      "analyzer(phrase(d.obj.properties.id.name, [{term: 'q'}, 'ui', {starts_with: 'c'}, 'k', 3, "
      "{wildcard: {t: 'b'}}, 'r', {in_range: ['n', 'p', false, false]}, 'wn', {levenshtein_match: ['p', 1, true, 0]}, 'ox', {terms: ['g', 'j']}, 'umps']), 'test_analyzer') "
      "RETURN d",
      &ctx);
    assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER "
      "analyzer(phrase(d.obj.properties.id.name, [{term: 'q'}, 'ui', {starts_with: 'c'}, 'k', 3, "
      "{wildcard: true}, 'r', {in_range: ['n', 'p', false, false]}, 'wn', {levenshtein_match: ['p', 1, true, 0]}, 'ox', {terms: ['g', 'j']}, 'umps']), 'test_analyzer') "
      "RETURN d",
      &ctx);
    assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER "
      "analyzer(phrase(d.obj.properties.id.name, [{term: 'q'}, 'ui', {starts_with: 'c'}, 'k', 3, "
      "{wildcard: 1}, 'r', {in_range: ['n', 'p', false, false]}, 'wn', {levenshtein_match: ['p', 1, true, 0]}, 'ox', {terms: ['g', 'j']}, 'umps']), 'test_analyzer') "
      "RETURN d",
      &ctx);
    assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER "
      "analyzer(phrase(d.obj.properties.id.name, [{term: 'q'}, 'ui', {starts_with: 'c'}, 'k', 3, "
      "{wildcard: 1.2}, 'r', {in_range: ['n', 'p', false, false]}, 'wn', {levenshtein_match: ['p', 1, true, 0]}, 'ox', {terms: ['g', 'j']}, 'umps']), 'test_analyzer') "
      "RETURN d",
      &ctx);
    assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER "
      "analyzer(phrase(d.obj.properties.id.name, [{term: 'q'}, 'ui', {starts_with: 'c'}, 'k', 3, "
      "{wildcard: null}, 'r', {in_range: ['n', 'p', false, false]}, 'wn', {levenshtein_match: ['p', 1, true, 0]}, 'ox', {terms: ['g', 'j']}, 'umps']), 'test_analyzer') "
      "RETURN d",
      &ctx);
    assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER "
      "analyzer(phrase(d.obj.properties.id.name, [{term: 'q'}, 'ui', {starts_with: 'c'}, 'k', 3, "
      "{wildcard: 'b'}, 'r', {in_range: ['n', 'p', false, false]}, 'wn', {levenshtein_match: [{t: 'p'}, 1, true, 0]}, 'ox', {terms: ['g', 'j']}, 'umps']), 'test_analyzer') "
      "RETURN d",
      &ctx);
    assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER "
      "analyzer(phrase(d.obj.properties.id.name, [{term: 'q'}, 'ui', {starts_with: 'c'}, 'k', 3, "
      "{wildcard: 'b'}, 'r', {in_range: ['n', 'p', false, false]}, 'wn', {levenshtein_match: [['p'], 1, true, 0]}, 'ox', {terms: ['g', 'j']}, 'umps']), 'test_analyzer') "
      "RETURN d",
      &ctx);
    assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER "
      "analyzer(phrase(d.obj.properties.id.name, [{term: 'q'}, 'ui', {starts_with: 'c'}, 'k', 3, "
      "{wildcard: 'b'}, 'r', {in_range: ['n', 'p', false, false]}, 'wn', {levenshtein_match: [true, 1, true, 0]}, 'ox', {terms: ['g', 'j']}, 'umps']), 'test_analyzer') "
      "RETURN d",
      &ctx);
    assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER "
      "analyzer(phrase(d.obj.properties.id.name, [{term: 'q'}, 'ui', {starts_with: 'c'}, 'k', 3, "
      "{wildcard: 'b'}, 'r', {in_range: ['n', 'p', false, false]}, 'wn', {levenshtein_match: [1, 1, true, 0]}, 'ox', {terms: ['g', 'j']}, 'umps']), 'test_analyzer') "
      "RETURN d",
      &ctx);
    assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER "
      "analyzer(phrase(d.obj.properties.id.name, [{term: 'q'}, 'ui', {starts_with: 'c'}, 'k', 3, "
      "{wildcard: 'b'}, 'r', {in_range: ['n', 'p', false, false]}, 'wn', {levenshtein_match: [1.2, 1, true, 0]}, 'ox', {terms: ['g', 'j']}, 'umps']), 'test_analyzer') "
      "RETURN d",
      &ctx);
    assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER "
      "analyzer(phrase(d.obj.properties.id.name, [{term: 'q'}, 'ui', {starts_with: 'c'}, 'k', 3, "
      "{wildcard: 'b'}, 'r', {in_range: ['n', 'p', false, false]}, 'wn', {levenshtein_match: [null, 1, true, 0]}, 'ox', {terms: ['g', 'j']}, 'umps']), 'test_analyzer') "
      "RETURN d",
      &ctx);
    assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER "
      "analyzer(phrase(d.obj.properties.id.name, [{term: 'q'}, 'ui', {starts_with: 'c'}, 'k', 3, "
      "{wildcard: 'b'}, 'r', {in_range: ['n', 'p', false, false]}, 'wn', {levenshtein_match: ['p', {t: 1}, true, 0]}, 'ox', {terms: ['g', 'j']}, 'umps']), 'test_analyzer') "
      "RETURN d",
      &ctx);
    assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER "
      "analyzer(phrase(d.obj.properties.id.name, [{term: 'q'}, 'ui', {starts_with: 'c'}, 'k', 3, "
      "{wildcard: 'b'}, 'r', {in_range: ['n', 'p', false, false]}, 'wn', {levenshtein_match: ['p', [1], true, 0]}, 'ox', {terms: ['g', 'j']}, 'umps']), 'test_analyzer') "
      "RETURN d",
      &ctx);
    assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER "
      "analyzer(phrase(d.obj.properties.id.name, [{term: 'q'}, 'ui', {starts_with: 'c'}, 'k', 3, "
      "{wildcard: 'b'}, 'r', {in_range: ['n', 'p', false, false]}, 'wn', {levenshtein_match: ['p', true, true]}, 'ox', {terms: ['g', 'j']}, 'umps']), 'test_analyzer') "
      "RETURN d",
      &ctx);
    assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER "
      "analyzer(phrase(d.obj.properties.id.name, [{term: 'q'}, 'ui', {starts_with: 'c'}, 'k', 3, "
      "{wildcard: 'b'}, 'r', {in_range: ['n', 'p', false, false]}, 'wn', {levenshtein_match: ['p', '1', true]}, 'ox', {terms: ['g', 'j']}, 'umps']), 'test_analyzer') "
      "RETURN d",
      &ctx);
    assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER "
      "analyzer(phrase(d.obj.properties.id.name, [{term: 'q'}, 'ui', {starts_with: 'c'}, 'k', 3, "
      "{wildcard: 'b'}, 'r', {in_range: ['n', 'p', false, false]}, 'wn', {levenshtein_match: ['p', null, true]}, 'ox', {terms: ['g', 'j']}, 'umps']), 'test_analyzer') "
      "RETURN d",
      &ctx);
    assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER "
      "analyzer(phrase(d.obj.properties.id.name, [{term: 'q'}, 'ui', {starts_with: 'c'}, 'k', 3, "
      "{wildcard: 'b'}, 'r', {in_range: ['n', 'p', false, false]}, 'wn', {levenshtein_match: ['p', 1, {t: true}]}, 'ox', {terms: ['g', 'j']}, 'umps']), 'test_analyzer') "
      "RETURN d",
      &ctx);
    assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER "
      "analyzer(phrase(d.obj.properties.id.name, [{term: 'q'}, 'ui', {starts_with: 'c'}, 'k', 3, "
      "{wildcard: 'b'}, 'r', {in_range: ['n', 'p', false, false]}, 'wn', {levenshtein_match: ['p', 1, [true]]}, 'ox', {terms: ['g', 'j']}, 'umps']), 'test_analyzer') "
      "RETURN d",
      &ctx);
    assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER "
      "analyzer(phrase(d.obj.properties.id.name, [{term: 'q'}, 'ui', {starts_with: 'c'}, 'k', 3, "
      "{wildcard: 'b'}, 'r', {in_range: ['n', 'p', false, false]}, 'wn', {levenshtein_match: ['p', 1, 'true']}, 'ox', {terms: ['g', 'j']}, 'umps']), 'test_analyzer') "
      "RETURN d",
      &ctx);
    assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER "
      "analyzer(phrase(d.obj.properties.id.name, [{term: 'q'}, 'ui', {starts_with: 'c'}, 'k', 3, "
      "{wildcard: 'b'}, 'r', {in_range: ['n', 'p', false, false]}, 'wn', {levenshtein_match: ['p', 1, 1]}, 'ox', {terms: ['g', 'j']}, 'umps']), 'test_analyzer') "
      "RETURN d",
      &ctx);
    assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER "
      "analyzer(phrase(d.obj.properties.id.name, [{term: 'q'}, 'ui', {starts_with: 'c'}, 'k', 3, "
      "{wildcard: 'b'}, 'r', {in_range: ['n', 'p', false, false]}, 'wn', {levenshtein_match: ['p', 1, 1.2]}, 'ox', {terms: ['g', 'j']}, 'umps']), 'test_analyzer') "
      "RETURN d",
      &ctx);
    assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER "
      "analyzer(phrase(d.obj.properties.id.name, [{term: 'q'}, 'ui', {starts_with: 'c'}, 'k', 3, "
      "{wildcard: 'b'}, 'r', {in_range: ['n', 'p', false, false]}, 'wn', {levenshtein_match: ['p', 1, null]}, 'ox', {terms: ['g', 'j']}, 'umps']), 'test_analyzer') "
      "RETURN d",
      &ctx);
    assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER "
      "analyzer(phrase(d.obj.properties.id.name, [{term: 'q'}, 'ui', {starts_with: 'c'}, 'k', 3, "
      "{wildcard: 'b'}, 'r', {in_range: ['n', 'p', false, false]}, 'wn', {levenshtein_match: ['p', 1, true]}, 'ox', {terms: [{t: 'g'}, 'j']}, 'umps']), 'test_analyzer') "
      "RETURN d",
      &ctx);
    assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER "
      "analyzer(phrase(d.obj.properties.id.name, [{term: 'q'}, 'ui', {starts_with: 'c'}, 'k', 3, "
      "{wildcard: 'b'}, 'r', {in_range: ['n', 'p', false, false]}, 'wn', {levenshtein_match: ['p', 1, true]}, 'ox', {terms: [['g'], 'j']}, 'umps']), 'test_analyzer') "
      "RETURN d",
      &ctx);
    assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER "
      "analyzer(phrase(d.obj.properties.id.name, [{term: 'q'}, 'ui', {starts_with: 'c'}, 'k', 3, "
      "{wildcard: 'b'}, 'r', {in_range: ['n', 'p', false, false]}, 'wn', {levenshtein_match: ['p', 1, true]}, 'ox', {terms: [true, 'j']}, 'umps']), 'test_analyzer') "
      "RETURN d",
      &ctx);
    assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER "
      "analyzer(phrase(d.obj.properties.id.name, [{term: 'q'}, 'ui', {starts_with: 'c'}, 'k', 3, "
      "{wildcard: 'b'}, 'r', {in_range: ['n', 'p', false, false]}, 'wn', {levenshtein_match: ['p', 1, true]}, 'ox', {terms: [1, 'j']}, 'umps']), 'test_analyzer') "
      "RETURN d",
      &ctx);
    assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER "
      "analyzer(phrase(d.obj.properties.id.name, [{term: 'q'}, 'ui', {starts_with: 'c'}, 'k', 3, "
      "{wildcard: 'b'}, 'r', {in_range: ['n', 'p', false, false]}, 'wn', {levenshtein_match: ['p', 1, true]}, 'ox', {terms: [1.2, 'j']}, 'umps']), 'test_analyzer') "
      "RETURN d",
      &ctx);
    assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER "
      "analyzer(phrase(d.obj.properties.id.name, [{term: 'q'}, 'ui', {starts_with: 'c'}, 'k', 3, "
      "{wildcard: 'b'}, 'r', {in_range: ['n', 'p', false, false]}, 'wn', {levenshtein_match: ['p', 1, true]}, 'ox', {terms: [null, 'j']}, 'umps']), 'test_analyzer') "
      "RETURN d",
      &ctx);
    assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER "
      "analyzer(phrase(d.obj.properties.id.name, [{term: 'q'}, 'ui', {starts_with: 'c'}, 'k', 3, "
      "{wildcard: 'b'}, 'r', {in_range: ['n', 'p', false, false]}, 'wn', {levenshtein_match: ['p', 1, true]}, 'ox', [{t: 'g'}, 'j'], 'umps']), 'test_analyzer') "
      "RETURN d",
      &ctx);
    assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER "
      "analyzer(phrase(d.obj.properties.id.name, [{term: 'q'}, 'ui', {starts_with: 'c'}, 'k', 3, "
      "{wildcard: 'b'}, 'r', {in_range: ['n', 'p', false, false]}, 'wn', {levenshtein_match: ['p', 1, true]}, 'ox', [['g'], 'j'], 'umps']), 'test_analyzer') "
      "RETURN d",
      &ctx);
    assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER "
      "analyzer(phrase(d.obj.properties.id.name, [{term: 'q'}, 'ui', {starts_with: 'c'}, 'k', 3, "
      "{wildcard: 'b'}, 'r', {in_range: ['n', 'p', false, false]}, 'wn', {levenshtein_match: ['p', 1, true]}, 'ox', [true, 'j'], 'umps']), 'test_analyzer') "
      "RETURN d",
      &ctx);
    assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER "
      "analyzer(phrase(d.obj.properties.id.name, [{term: 'q'}, 'ui', {starts_with: 'c'}, 'k', 3, "
      "{wildcard: 'b'}, 'r', {in_range: ['n', 'p', false, false]}, 'wn', {levenshtein_match: ['p', 1, true]}, 'ox', [1, 'j'], 'umps']), 'test_analyzer') "
      "RETURN d",
      &ctx);
    assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER "
      "analyzer(phrase(d.obj.properties.id.name, [{term: 'q'}, 'ui', {starts_with: 'c'}, 'k', 3, "
      "{wildcard: 'b'}, 'r', {in_range: ['n', 'p', false, false]}, 'wn', {levenshtein_match: ['p', 1, true]}, 'ox', [1.2, 'j'], 'umps']), 'test_analyzer') "
      "RETURN d",
      &ctx);
    assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER "
      "analyzer(phrase(d.obj.properties.id.name, [{term: 'q'}, 'ui', {starts_with: 'c'}, 'k', 3, "
      "{wildcard: 'b'}, 'r', {in_range: ['n', 'p', false, false]}, 'wn', {levenshtein_match: ['p', 1, true]}, 'ox', [null, 'j'], 'umps']), 'test_analyzer') "
      "RETURN d",
      &ctx);
    assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER "
      "analyzer(phrase(d.obj.properties.id.name, [{term: 'q'}, 'ui', {starts_with: 'c'}, 'k', 3, "
      "{wildcard: 'b'}, 'r', {in_range: [{t: 'n'}, 'p', false, false]}, 'wn', {levenshtein_match: ['p', 1, true]}, 'ox', ['g', 'j'], 'umps']), 'test_analyzer') "
      "RETURN d",
      &ctx);
    assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER "
      "analyzer(phrase(d.obj.properties.id.name, [{term: 'q'}, 'ui', {starts_with: 'c'}, 'k', 3, "
      "{wildcard: 'b'}, 'r', {in_range: [['n'], 'p', false, false]}, 'wn', {levenshtein_match: ['p', 1, true]}, 'ox', ['g', 'j'], 'umps']), 'test_analyzer') "
      "RETURN d",
      &ctx);
    assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER "
      "analyzer(phrase(d.obj.properties.id.name, [{term: 'q'}, 'ui', {starts_with: 'c'}, 'k', 3, "
      "{wildcard: 'b'}, 'r', {in_range: [1, 'p', false, false]}, 'wn', {levenshtein_match: ['p', 1, true]}, 'ox', ['g', 'j'], 'umps']), 'test_analyzer') "
      "RETURN d",
      &ctx);
    assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER "
      "analyzer(phrase(d.obj.properties.id.name, [{term: 'q'}, 'ui', {starts_with: 'c'}, 'k', 3, "
      "{wildcard: 'b'}, 'r', {in_range: [1.2, 'p', false, false]}, 'wn', {levenshtein_match: ['p', 1, true]}, 'ox', ['g', 'j'], 'umps']), 'test_analyzer') "
      "RETURN d",
      &ctx);
    assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER "
      "analyzer(phrase(d.obj.properties.id.name, [{term: 'q'}, 'ui', {starts_with: 'c'}, 'k', 3, "
      "{wildcard: 'b'}, 'r', {in_range: [true, 'p', false, false]}, 'wn', {levenshtein_match: ['p', 1, true]}, 'ox', ['g', 'j'], 'umps']), 'test_analyzer') "
      "RETURN d",
      &ctx);
    assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER "
      "analyzer(phrase(d.obj.properties.id.name, [{term: 'q'}, 'ui', {starts_with: 'c'}, 'k', 3, "
      "{wildcard: 'b'}, 'r', {in_range: [null, 'p', false, false]}, 'wn', {levenshtein_match: ['p', 1, true]}, 'ox', ['g', 'j'], 'umps']), 'test_analyzer') "
      "RETURN d",
      &ctx);
    assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER "
      "analyzer(phrase(d.obj.properties.id.name, [{term: 'q'}, 'ui', {starts_with: 'c'}, 'k', 3, "
      "{wildcard: 'b'}, 'r', {in_range: ['n', {t: 'p'}, false, false]}, 'wn', {levenshtein_match: ['p', 1, true]}, 'ox', ['g', 'j'], 'umps']), 'test_analyzer') "
      "RETURN d",
      &ctx);
    assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER "
      "analyzer(phrase(d.obj.properties.id.name, [{term: 'q'}, 'ui', {starts_with: 'c'}, 'k', 3, "
      "{wildcard: 'b'}, 'r', {in_range: ['n', ['p'], false, false]}, 'wn', {levenshtein_match: ['p', 1, true]}, 'ox', ['g', 'j'], 'umps']), 'test_analyzer') "
      "RETURN d",
      &ctx);
    assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER "
      "analyzer(phrase(d.obj.properties.id.name, [{term: 'q'}, 'ui', {starts_with: 'c'}, 'k', 3, "
      "{wildcard: 'b'}, 'r', {in_range: ['n', 1, false, false]}, 'wn', {levenshtein_match: ['p', 1, true]}, 'ox', ['g', 'j'], 'umps']), 'test_analyzer') "
      "RETURN d",
      &ctx);
    assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER "
      "analyzer(phrase(d.obj.properties.id.name, [{term: 'q'}, 'ui', {starts_with: 'c'}, 'k', 3, "
      "{wildcard: 'b'}, 'r', {in_range: ['n', 1.2, false, false]}, 'wn', {levenshtein_match: ['p', 1, true]}, 'ox', ['g', 'j'], 'umps']), 'test_analyzer') "
      "RETURN d",
      &ctx);
    assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER "
      "analyzer(phrase(d.obj.properties.id.name, [{term: 'q'}, 'ui', {starts_with: 'c'}, 'k', 3, "
      "{wildcard: 'b'}, 'r', {in_range: ['n', true, false, false]}, 'wn', {levenshtein_match: ['p', 1, true]}, 'ox', ['g', 'j'], 'umps']), 'test_analyzer') "
      "RETURN d",
      &ctx);
    assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER "
      "analyzer(phrase(d.obj.properties.id.name, [{term: 'q'}, 'ui', {starts_with: 'c'}, 'k', 3, "
      "{wildcard: 'b'}, 'r', {in_range: ['n', null, false, false]}, 'wn', {levenshtein_match: ['p', 1, true]}, 'ox', ['g', 'j'], 'umps']), 'test_analyzer') "
      "RETURN d",
      &ctx);
    assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER "
      "analyzer(phrase(d.obj.properties.id.name, [{term: 'q'}, 'ui', {starts_with: 'c'}, 'k', 3, "
      "{wildcard: 'b'}, 'r', {in_range: ['n', 'p', {t: false}, false]}, 'wn', {levenshtein_match: ['p', 1, true]}, 'ox', ['g', 'j'], 'umps']), 'test_analyzer') "
      "RETURN d",
      &ctx);
    assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER "
      "analyzer(phrase(d.obj.properties.id.name, [{term: 'q'}, 'ui', {starts_with: 'c'}, 'k', 3, "
      "{wildcard: 'b'}, 'r', {in_range: ['n', 'p', [false], false]}, 'wn', {levenshtein_match: ['p', 1, true]}, 'ox', ['g', 'j'], 'umps']), 'test_analyzer') "
      "RETURN d",
      &ctx);
    assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER "
      "analyzer(phrase(d.obj.properties.id.name, [{term: 'q'}, 'ui', {starts_with: 'c'}, 'k', 3, "
      "{wildcard: 'b'}, 'r', {in_range: ['n', 'p', 'false', false]}, 'wn', {levenshtein_match: ['p', 1, true]}, 'ox', ['g', 'j'], 'umps']), 'test_analyzer') "
      "RETURN d",
      &ctx);
    assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER "
      "analyzer(phrase(d.obj.properties.id.name, [{term: 'q'}, 'ui', {starts_with: 'c'}, 'k', 3, "
      "{wildcard: 'b'}, 'r', {in_range: ['n', 'p', 1, false]}, 'wn', {levenshtein_match: ['p', 1, true]}, 'ox', ['g', 'j'], 'umps']), 'test_analyzer') "
      "RETURN d",
      &ctx);
    assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER "
      "analyzer(phrase(d.obj.properties.id.name, [{term: 'q'}, 'ui', {starts_with: 'c'}, 'k', 3, "
      "{wildcard: 'b'}, 'r', {in_range: ['n', 'p', 1.2, false]}, 'wn', {levenshtein_match: ['p', 1, true]}, 'ox', ['g', 'j'], 'umps']), 'test_analyzer') "
      "RETURN d",
      &ctx);
    assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER "
      "analyzer(phrase(d.obj.properties.id.name, [{term: 'q'}, 'ui', {starts_with: 'c'}, 'k', 3, "
      "{wildcard: 'b'}, 'r', {in_range: ['n', 'p', null, false]}, 'wn', {levenshtein_match: ['p', 1, true]}, 'ox', ['g', 'j'], 'umps']), 'test_analyzer') "
      "RETURN d",
      &ctx);
    assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER "
      "analyzer(phrase(d.obj.properties.id.name, [{term: 'q'}, 'ui', {starts_with: 'c'}, 'k', 3, "
      "{wildcard: 'b'}, 'r', {in_range: ['n', 'p', false, {t: false}]}, 'wn', {levenshtein_match: ['p', 1, true]}, 'ox', ['g', 'j'], 'umps']), 'test_analyzer') "
      "RETURN d",
      &ctx);
    assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER "
      "analyzer(phrase(d.obj.properties.id.name, [{term: 'q'}, 'ui', {starts_with: 'c'}, 'k', 3, "
      "{wildcard: 'b'}, 'r', {in_range: ['n', 'p', false, [false]]}, 'wn', {levenshtein_match: ['p', 1, true]}, 'ox', ['g', 'j'], 'umps']), 'test_analyzer') "
      "RETURN d",
      &ctx);
    assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER "
      "analyzer(phrase(d.obj.properties.id.name, [{term: 'q'}, 'ui', {starts_with: 'c'}, 'k', 3, "
      "{wildcard: 'b'}, 'r', {in_range: ['n', 'p', false, 'false']}, 'wn', {levenshtein_match: ['p', 1, true]}, 'ox', ['g', 'j'], 'umps']), 'test_analyzer') "
      "RETURN d",
      &ctx);
    assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER "
      "analyzer(phrase(d.obj.properties.id.name, [{term: 'q'}, 'ui', {starts_with: 'c'}, 'k', 3, "
      "{wildcard: 'b'}, 'r', {in_range: ['n', 'p', false, 1]}, 'wn', {levenshtein_match: ['p', 1, true]}, 'ox', ['g', 'j'], 'umps']), 'test_analyzer') "
      "RETURN d",
      &ctx);
    assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER "
      "analyzer(phrase(d.obj.properties.id.name, [{term: 'q'}, 'ui', {starts_with: 'c'}, 'k', 3, "
      "{wildcard: 'b'}, 'r', {in_range: ['n', 'p', false, 1.2]}, 'wn', {levenshtein_match: ['p', 1, true]}, 'ox', ['g', 'j'], 'umps']), 'test_analyzer') "
      "RETURN d",
      &ctx);
    assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER "
      "analyzer(phrase(d.obj.properties.id.name, [{term: 'q'}, 'ui', {starts_with: 'c'}, 'k', 3, "
      "{wildcard: 'b'}, 'r', {in_range: ['n', 'p', false, null]}, 'wn', {levenshtein_match: ['p', 1, true]}, 'ox', ['g', 'j'], 'umps']), 'test_analyzer') "
      "RETURN d",
      &ctx);
  }

  // multiple offsets, complex name, custom analyzer, invalid expressions
  // quick <...> <...> <...> brown <...> <...> fox jumps
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("offset", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt(2)));
    ctx.vars.emplace("input", arangodb::aql::AqlValue("bro"));

    assertFilterExecutionFail(
        vocbase(),
        "LET offset=2 LET input='bro' FOR d IN myView FILTER "
        "analyzer(phrase(d.obj.properties.id.name, 'quick', TO_BOOL(offset+1), "
        "CONCAT(input, 'wn'), offset, 'fox', 0, 'jumps'), 'test_analyzer') "
        "RETURN d",
        &ctx);
    assertFilterExecutionFail(
        vocbase(),
        "LET offset=2 LET input='bro' FOR d IN myView FILTER "
        "analyzer(phrase(d.obj.properties.id.name, 'quick', offset + 1.5, "
        "'brown', TO_STRING(2), 'fox', 0, 'jumps'), 'test_analyzer') RETURN d",
        &ctx);
    assertFilterExecutionFail(
        vocbase(),
        "LET offset=2 LET input='bro' FOR d IN myView FILTER "
        "analyzer(phrase(d.obj.properties.id['name'], 'quick', 3.0, 'brown', "
        "offset, 'fox', 0, 'jumps'), TO_BOOL('test_analyzer')) RETURN d",
        &ctx);
    assertFilterExecutionFail(
        vocbase(),
        "LET offset=2 LET input='bro' FOR d IN myView FILTER "
        "analyzer(phrase(d.obj.properties.id.name, 'quick', TO_BOOL(3.6), "
        "'brown', 2, 'fox', offset-2, 'jumps'), 'test_analyzer') RETURN d",
        &ctx);
    assertFilterExecutionFail(
        vocbase(),
        "LET offset=2 LET input='bro' FOR d IN myView FILTER "
        "analyzer(phrase(d.obj.properties.id.name, [ 'quick', "
        "TO_BOOL(offset+1), CONCAT(input, 'wn'), offset, 'fox', 0, 'jumps' ]), "
        "'test_analyzer') RETURN d",
        &ctx);
    assertFilterExecutionFail(
        vocbase(),
        "LET offset=2 LET input='bro' FOR d IN myView FILTER "
        "analyzer(phrase(d.obj.properties.id['name'], [ 'quick', 3.0, 'brown', "
        "offset, 'fox', 0, 'jumps' ]), TO_BOOL('test_analyzer')) RETURN d",
        &ctx);
    assertFilterExecutionFail(
        vocbase(),
        "LET offset=2 LET input='bro' FOR d IN myView FILTER "
        "analyzer(phrase(d.obj.properties.id.name, [ 'quick', TO_BOOL(3.6), "
        "'brown', 2, 'fox', offset-2, 'jumps' ]), 'test_analyzer') RETURN d",
        &ctx);
    assertFilterExecutionFail(
        vocbase(),
        "LET offset=2 LET input='bro' FOR d IN myView FILTER "
        "phrase(d.obj.properties.id.name, 'quick', TO_BOOL(offset+1), "
        "CONCAT(input, 'wn'), offset, 'fox', 0, 'jumps', 'test_analyzer') "
        "RETURN d",
        &ctx);
    assertFilterExecutionFail(
        vocbase(),
        "LET offset=2 LET input='bro' FOR d IN myView FILTER "
        "phrase(d.obj.properties.id.name, 'quick', offset + 1.5, 'brown', "
        "TO_STRING(2), 'fox', 0, 'jumps', 'test_analyzer') RETURN d",
        &ctx);
    assertFilterExecutionFail(
        vocbase(),
        "LET offset=2 LET input='bro' FOR d IN myView FILTER "
        "phrase(d.obj.properties.id['name'], 'quick', 3.0, 'brown', offset, "
        "'fox', 0, 'jumps', TO_BOOL('test_analyzer')) RETURN d",
        &ctx);
    assertFilterExecutionFail(
        vocbase(),
        "LET offset=2 LET input='bro' FOR d IN myView FILTER "
        "phrase(d.obj.properties.id.name, 'quick', TO_BOOL(3.6), 'brown', 2, "
        "'fox', offset-2, 'jumps', 'test_analyzer') RETURN d",
        &ctx);
    assertFilterExecutionFail(
        vocbase(),
        "LET offset=2 LET input='bro' FOR d IN myView FILTER "
        "phrase(d.obj.properties.id.name, [ 'quick', TO_BOOL(offset+1), "
        "CONCAT(input, 'wn'), offset, 'fox', 0, 'jumps' ], 'test_analyzer') "
        "RETURN d",
        &ctx);
    assertFilterExecutionFail(
        vocbase(),
        "LET offset=2 LET input='bro' FOR d IN myView FILTER "
        "phrase(d.obj.properties.id['name'], [ 'quick', 3.0, 'brown', offset, "
        "'fox', 0, 'jumps' ], TO_BOOL('test_analyzer')) RETURN d",
        &ctx);
    assertFilterExecutionFail(
        vocbase(),
        "LET offset=2 LET input='bro' FOR d IN myView FILTER "
        "phrase(d.obj.properties.id.name, [ 'quick', TO_BOOL(3.6), 'brown', 2, "
        "'fox', offset-2, 'jumps' ], 'test_analyzer') RETURN d",
        &ctx);
  }

  // invalid analyzer
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER analyzer(phrase(d.name, 'quick'), [ 1, \"abc\" "
      "]) RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER analyzer(phrase(d['name'], 'quick'), [ 1, "
      "\"abc\" ]) RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER analyzer(phrase(d.name, 'quick'), true) RETURN "
      "d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER analyzer(phrase(d['name'], 'quick'), false) "
      "RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER analyzer(phrase(d.name, 'quick'), null) RETURN "
      "d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER analyzer(phrase(d['name'], 'quick'), null) "
      "RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER analyzer(phrase(d.name, 'quick'), 3.14) RETURN "
      "d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER analyzer(phrase(d['name'], 'quick'), 1234) "
      "RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER analyzer(phrase(d.name, 'quick'), { \"a\": 7, "
      "\"b\": \"c\" }) RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER analyzer(phrase(d['name'], 'quick'), { \"a\": 7, "
      "\"b\": \"c\" }) RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER analyzer(phrase(d.name, 'quick'), "
      "'invalid_analyzer') RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER analyzer(phrase(d['name'], 'quick'), "
      "'invalid_analyzer') RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER analyzer(phrase(d.name, [ 'quick' ]), [ 1, "
      "\"abc\" ]) RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER analyzer(phrase(d['name'], [ 'quick' ]), [ 1, "
      "\"abc\" ]) RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER analyzer(phrase(d.name, [ 'quick' ]), true) "
      "RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER analyzer(phrase(d['name'], [ 'quick' ]), false) "
      "RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER analyzer(phrase(d.name, [ 'quick' ]), null) "
      "RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER analyzer(phrase(d['name'], [ 'quick' ]), null) "
      "RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER analyzer(phrase(d.name, [ 'quick' ]), 3.14) "
      "RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER analyzer(phrase(d['name'], [ 'quick' ]), 1234) "
      "RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER analyzer(phrase(d.name, [ 'quick' ]), { \"a\": "
      "7, \"b\": \"c\" }) RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER analyzer(phrase(d['name'], [ 'quick' ]), { "
      "\"a\": 7, \"b\": \"c\" }) RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER analyzer(phrase(d.name, [ 'quick' ]), "
      "'invalid_analyzer') RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER analyzer(phrase(d['name'], [ 'quick' ]), "
      "'invalid_analyzer') RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER phrase(d.name, 'quick', [ 1, \"abc\" ]) RETURN "
      "d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER phrase(d['name'], 'quick', [ 1, \"abc\" ]) "
      "RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER phrase(d.name, 'quick', true) RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER phrase(d['name'], 'quick', false) RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER phrase(d.name, 'quick', null) RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER phrase(d['name'], 'quick', null) RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER phrase(d.name, 'quick', 3.14) RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER phrase(d['name'], 'quick', 1234) RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER phrase(d.name, 'quick', { \"a\": 7, \"b\": \"c\" "
      "}) RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER phrase(d['name'], 'quick', { \"a\": 7, \"b\": "
      "\"c\" }) RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER phrase(d.name, 'quick', 'invalid_analyzer') "
      "RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER phrase(d['name'], 'quick', 'invalid_analyzer') "
      "RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER phrase(d.name, [ 'quick' ], [ 1, \"abc\" ]) "
      "RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER phrase(d['name'], [ 'quick' ], [ 1, \"abc\" ]) "
      "RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER phrase(d.name, [ 'quick' ], true) RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER phrase(d['name'], [ 'quick' ], false) RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER phrase(d.name, [ 'quick' ], null) RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER phrase(d['name'], [ 'quick' ], null) RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER phrase(d.name, [ 'quick' ], 3.14) RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER phrase(d['name'], [ 'quick' ], 1234) RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER phrase(d.name, [ 'quick' ], { \"a\": 7, \"b\": "
      "\"c\" }) RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER phrase(d['name'], [ 'quick' ], { \"a\": 7, "
      "\"b\": \"c\" }) RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER phrase(d.name, [ 'quick' ], 'invalid_analyzer') "
      "RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER phrase(d['name'], [ 'quick' ], "
      "'invalid_analyzer') RETURN d");

  // wrong analylzer
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER analyzer(phrase(d.name, 'quick'), ['d']) RETURN "
      "d");
  assertFilterExecutionFail(
      vocbase(),
      "FOR d IN myView FILTER analyzer(phrase(d.name, 'quick'), [d]) RETURN d",
      &ExpressionContextMock::EMPTY);
  assertFilterExecutionFail(
      vocbase(),
      "FOR d IN myView FILTER analyzer(phrase(d.name, 'quick'), d) RETURN d",
      &ExpressionContextMock::EMPTY);
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER analyzer(phrase(d.name, 'quick'), 3) RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER analyzer(phrase(d.name, 'quick'), 3.0) RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER analyzer(phrase(d.name, 'quick'), true) RETURN "
      "d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER analyzer(phrase(d.name, 'quick'), false) RETURN "
      "d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER analyzer(phrase(d.name, 'quick'), null) RETURN "
      "d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER analyzer(phrase(d.name, 'quick'), "
      "'invalidAnalyzer') RETURN d");
  assertFilterExecutionFail(
      vocbase(),
      "FOR d IN myView FILTER analyzer(phrase(d.name, 'quick', 3, 'brown'), d) "
      "RETURN d",
      &ExpressionContextMock::EMPTY);
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER analyzer(phrase(d.name, 'quick', 3, 'brown'), 3) "
      "RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER analyzer(phrase(d.name, 'quick', 3, 'brown'), "
      "3.0) RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER analyzer(phrase(d.name, 'quick', 3, 'brown'), "
      "true) RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER analyzer(phrase(d.name, 'quick', 3, 'brown'), "
      "false) RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER analyzer(phrase(d.name, 'quick', 3, 'brown'), "
      "null) RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER analyzer(phrase(d.name, 'quick', 3, 'brown'), "
      "'invalidAnalyzer') RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER analyzer(phrase(d.name, [ 'quick' ]), ['d']) "
      "RETURN d");
  assertFilterExecutionFail(
      vocbase(),
      "FOR d IN myView FILTER analyzer(phrase(d.name, [ 'quick' ]), [d]) "
      "RETURN d",
      &ExpressionContextMock::EMPTY);
  assertFilterExecutionFail(
      vocbase(),
      "FOR d IN myView FILTER analyzer(phrase(d.name, [ 'quick' ]), d) RETURN "
      "d",
      &ExpressionContextMock::EMPTY);
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER analyzer(phrase(d.name, [ 'quick' ]), 3) RETURN "
      "d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER analyzer(phrase(d.name, [ 'quick' ]), 3.0) "
      "RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER analyzer(phrase(d.name, [ 'quick' ]), true) "
      "RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER analyzer(phrase(d.name, [ 'quick' ]), false) "
      "RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER analyzer(phrase(d.name, [ 'quick' ]), null) "
      "RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER analyzer(phrase(d.name, [ 'quick' ]), "
      "'invalidAnalyzer') RETURN d");
  assertFilterExecutionFail(
      vocbase(),
      "FOR d IN myView FILTER ANALYZER(phrase(d.name, [ 'quick', 3, 'brown' "
      "]), d) RETURN d",
      &ExpressionContextMock::EMPTY);
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER analyzer(phrase(d.name, [ 'quick', 3, 'brown' "
      "]), 3) RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER analyzer(phrase(d.name, [ 'quick', 3, 'brown' "
      "]), 3.0) RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER analyzer(phrase(d.name, [ 'quick', 3, 'brown' "
      "]), true) RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER analyzer(phrase(d.name, [ 'quick', 3, 'brown' "
      "]), false) RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER analyzer(phrase(d.name, [ 'quick', 3, 'brown' "
      "]), null) RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER analyzer(phrase(d.name, [ 'quick', 3, 'brown' "
      "]), 'invalidAnalyzer') RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER phrase(d.name, 'quick', ['d']) RETURN d");
  assertFilterExecutionFail(
      vocbase(), "FOR d IN myView FILTER phrase(d.name, 'quick', [d]) RETURN d",
      &ExpressionContextMock::EMPTY);
  assertFilterExecutionFail(
      vocbase(), "FOR d IN myView FILTER phrase(d.name, 'quick', d) RETURN d",
      &ExpressionContextMock::EMPTY);
  assertFilterFail(
      vocbase(), "FOR d IN myView FILTER phrase(d.name, 'quick', 3) RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER phrase(d.name, 'quick', 3.0) RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER phrase(d.name, 'quick', true) RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER phrase(d.name, 'quick', false) RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER phrase(d.name, 'quick', null) RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER phrase(d.name, 'quick', 'invalidAnalyzer') "
      "RETURN d");
  assertFilterExecutionFail(
      vocbase(),
      "FOR d IN myView FILTER phrase(d.name, 'quick', 3, 'brown', d) RETURN d",
      &ExpressionContextMock::EMPTY);
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER phrase(d.name, 'quick', 3, 'brown', 3) RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER phrase(d.name, 'quick', 3, 'brown', 3.0) RETURN "
      "d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER phrase(d.name, 'quick', 3, 'brown', true) RETURN "
      "d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER phrase(d.name, 'quick', 3, 'brown', false) "
      "RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER phrase(d.name, 'quick', 3, 'brown', null) RETURN "
      "d");
  assertFilterFail(vocbase(),
                   "FOR d IN myView FILTER phrase(d.name, 'quick', 3, 'brown', "
                   "'invalidAnalyzer') RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER phrase(d.name, [ 'quick' ], ['d']) RETURN d");
  assertFilterExecutionFail(
      vocbase(),
      "FOR d IN myView FILTER phrase(d.name, [ 'quick' ], [d]) RETURN d",
      &ExpressionContextMock::EMPTY);
  assertFilterExecutionFail(
      vocbase(),
      "FOR d IN myView FILTER phrase(d.name, [ 'quick' ], d) RETURN d",
      &ExpressionContextMock::EMPTY);
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER phrase(d.name, [ 'quick' ], 3) RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER phrase(d.name, [ 'quick' ], 3.0) RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER phrase(d.name, [ 'quick' ], true) RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER phrase(d.name, [ 'quick' ], false) RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER phrase(d.name, [ 'quick' ], null) RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER phrase(d.name, [ 'quick' ], 'invalidAnalyzer') "
      "RETURN d");
  assertFilterExecutionFail(
      vocbase(),
      "FOR d IN myView FILTER phrase(d.name, [ 'quick', 3, 'brown' ], d) "
      "RETURN d",
      &ExpressionContextMock::EMPTY);
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER phrase(d.name, [ 'quick', 3, 'brown' ], 3) "
      "RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER phrase(d.name, [ 'quick', 3, 'brown' ], 3.0) "
      "RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER phrase(d.name, [ 'quick', 3, 'brown' ], true) "
      "RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER phrase(d.name, [ 'quick', 3, 'brown' ], false) "
      "RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER phrase(d.name, [ 'quick', 3, 'brown' ], null) "
      "RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER phrase(d.name, [ 'quick', 3, 'brown' ], "
      "'invalidAnalyzer') RETURN d");

  // non-deterministic arguments
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER analyzer(phrase(d[RAND() ? 'name' : 0], 'quick', "
      "0, 'brown'), 'test_analyzer') RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER analyzer(phrase(d.name, RAND() ? 'quick' : "
      "'slow', 0, 'brown'), 'test_analyzer') RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER analyzer(phrase(d.name, 'quick', 0, RAND() ? "
      "'brown' : 'red'), 'test_analyzer') RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER analyzer(phrase(d.name, 'quick', 0, 'brown'), "
      "RAND() ? 'test_analyzer' : 'invalid_analyzer') RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER analyzer(phrase(d[RAND() ? 'name' : 0], [ "
      "'quick', 0, 'brown' ]), 'test_analyzer') RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER analyzer(phrase(d.name, [ RAND() ? 'quick' : "
      "'slow', 0, 'brown' ]), 'test_analyzer') RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER analyzer(phrase(d.name, [ 'quick', 0, RAND() ? "
      "'brown' : 'red' ]), 'test_analyzer') RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER analyzer(phrase(d.name, [ 'quick', 0, 'brown' "
      "]), RAND() ? 'test_analyzer' : 'invalid_analyzer') RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER phrase(d[RAND() ? 'name' : 0], 'quick', 0, "
      "'brown', 'test_analyzer') RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER phrase(d.name, RAND() ? 'quick' : 'slow', 0, "
      "'brown', 'test_analyzer') RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER phrase(d.name, 'quick', 0, RAND() ? 'brown' : "
      "'red', 'test_analyzer') RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER phrase(d.name, 'quick', 0, 'brown', RAND() ? "
      "'test_analyzer' : 'invalid_analyzer') RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER phrase(d[RAND() ? 'name' : 0], [ 'quick', 0, "
      "'brown' ], 'test_analyzer') RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER phrase(d.name, [ RAND() ? 'quick' : 'slow', 0, "
      "'brown' ], 'test_analyzer') RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER phrase(d.name, [ 'quick', 0, RAND() ? 'brown' : "
      "'red' ], 'test_analyzer') RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER phrase(d.name, [ 'quick', 0, 'brown' ], RAND() ? "
      "'test_analyzer' : 'invalid_analyzer') RETURN d");
}

TEST_F(IResearchFilterFunctionTest, StartsWith) {
  // without scoring limit
  {
    irs::Or expected;
    auto& prefix = expected.add<irs::by_prefix>();
    *prefix.mutable_field() = mangleStringIdentity("name");
    auto* opt = prefix.mutable_options();
    opt->term = irs::ref_cast<irs::byte_type>(irs::string_ref("abc"));
    opt->scored_terms_limit = arangodb::iresearch::FilterConstants::DefaultScoringTermsLimit;

    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER starts_with(d['name'], 'abc') RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN myView FILTER starts_with(d.name, 'abc') RETURN d", expected);
  }

  // without scoring limit via []
  {
    irs::Or expected;
    auto& orFilter = expected.add<irs::Or>();
    orFilter.min_match_count(arangodb::iresearch::FilterConstants::DefaultStartsWithMinMatchCount);
    auto& prefix0 = orFilter.add<irs::by_prefix>();
    *prefix0.mutable_field() = mangleStringIdentity("name");
    auto* opt0 = prefix0.mutable_options();
    opt0->term = irs::ref_cast<irs::byte_type>(irs::string_ref("abc"));
    opt0->scored_terms_limit = arangodb::iresearch::FilterConstants::DefaultScoringTermsLimit;

    auto& prefix1 = orFilter.add<irs::by_prefix>();
    *prefix1.mutable_field() = mangleStringIdentity("name");
    auto* opt1 = prefix1.mutable_options();
    opt1->term = irs::ref_cast<irs::byte_type>(irs::string_ref("def"));
    opt1->scored_terms_limit = arangodb::iresearch::FilterConstants::DefaultScoringTermsLimit;

    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER starts_with(d['name'], ['abc', 'def']) RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN myView FILTER starts_with(d.name, ['abc', 'def']) RETURN d", expected);
  }

  // dynamic complex attribute field
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                                      arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                                      arangodb::aql::AqlValueHintDouble{5.6})));

    irs::Or expected;
    auto& prefix = expected.add<irs::by_prefix>();
    *prefix.mutable_field() = mangleStringIdentity("a.b.c.e[4].f[5].g[3].g.a");
    auto* opt = prefix.mutable_options();
    opt->term = irs::ref_cast<irs::byte_type>(irs::string_ref("abc"));
    opt->scored_terms_limit = arangodb::iresearch::FilterConstants::DefaultScoringTermsLimit;

    assertFilterSuccess(
        vocbase(),
        "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER "
        "starts_with(d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_"
        "FORWARD_('a')], 'abc') RETURN d",
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
        vocbase(),
        "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER "
        "starts_with(d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_"
        "FORWARD_('a')], 'abc') RETURN d",
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
        vocbase(),
        "LET a=null LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER "
        "starts_with(d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_"
        "FORWARD_('a')], 'abc') RETURN d",
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
        vocbase(),
        "LET a=false LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER "
        "starts_with(d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_"
        "FORWARD_('a')], 'abc') RETURN d",
        &ctx);
  }

  // without scoring limit, name with offset
  {
    irs::Or expected;
    auto& prefix = expected.add<irs::by_prefix>();
    *prefix.mutable_field() = mangleStringIdentity("name[1]");
    auto* opt = prefix.mutable_options();
    opt->term = irs::ref_cast<irs::byte_type>(irs::string_ref("abc"));
    opt->scored_terms_limit = arangodb::iresearch::FilterConstants::DefaultScoringTermsLimit;

    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER starts_with(d['name'][1], 'abc') RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER starts_with(d.name[1], 'abc') RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER analyzer(starts_with(d.name[1], 'abc'), "
        "'identity') RETURN d",
        expected);
  }

  // without scoring limit, complex name
  {
    irs::Or expected;
    auto& prefix = expected.add<irs::by_prefix>();
    *prefix.mutable_field() = mangleStringIdentity("obj.properties.name");
    auto* opt = prefix.mutable_options();
    opt->term = irs::ref_cast<irs::byte_type>(irs::string_ref("abc"));
    opt->scored_terms_limit = arangodb::iresearch::FilterConstants::DefaultScoringTermsLimit;

    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER starts_with(d['obj']['properties']['name'], "
        "'abc') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER starts_with(d.obj['properties']['name'], "
        "'abc') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER starts_with(d.obj['properties'].name, 'abc') "
        "RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER analyzer(starts_with(d.obj['properties'].name, "
        "'abc'), 'identity') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER starts_with(d.obj.properties.name, 'abc') "
        "RETURN d",
        expected);
  }

  // without scoring limit, complex name with offset
  {
    irs::Or expected;
    auto& prefix = expected.add<irs::by_prefix>();
    *prefix.mutable_field() = mangleStringIdentity("obj[400].properties[3].name");
    auto* opt = prefix.mutable_options();
    opt->term = irs::ref_cast<irs::byte_type>(irs::string_ref("abc"));
    opt->scored_terms_limit = arangodb::iresearch::FilterConstants::DefaultScoringTermsLimit;

    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER "
        "starts_with(d['obj'][400]['properties'][3]['name'], 'abc') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER "
        "starts_with(d.obj[400]['properties[3]']['name'], 'abc') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER starts_with(d.obj[400]['properties[3]'].name, "
        "'abc') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER starts_with(d.obj[400].properties[3].name, "
        "'abc') RETURN d",
        expected);
  }

  // without scoring limit, complex name with offset, analyzer
  {
    irs::Or expected;
    auto& prefix = expected.add<irs::by_prefix>();
    *prefix.mutable_field() = mangleString("obj[400].properties[3].name", "test_analyzer");
    auto* opt = prefix.mutable_options();
    opt->term = irs::ref_cast<irs::byte_type>(irs::string_ref("abc"));
    opt->scored_terms_limit = arangodb::iresearch::FilterConstants::DefaultScoringTermsLimit;

    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER "
        "Analyzer(starts_with(d['obj'][400]['properties'][3]['name'], 'abc'), "
        "'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER "
        "Analyzer(starts_with(d.obj[400]['properties[3]']['name'], 'abc'), "
        "'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER "
        "Analyzer(starts_with(d.obj[400]['properties[3]'].name, 'abc'), "
        "'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER "
        "Analyzer(starts_with(d.obj[400].properties[3].name, 'abc'), "
        "'test_analyzer') RETURN d",
        expected);
  }

  // without scoring limit, complex name with offset, prefix as an expression
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("prefix", arangodb::aql::AqlValue("ab"));

    irs::Or expected;
    auto& prefix = expected.add<irs::by_prefix>();
    *prefix.mutable_field() = mangleStringIdentity("obj[400].properties[3].name");
    auto* opt = prefix.mutable_options();
    opt->term = irs::ref_cast<irs::byte_type>(irs::string_ref("abc"));
    opt->scored_terms_limit = arangodb::iresearch::FilterConstants::DefaultScoringTermsLimit;

    assertFilterSuccess(
        vocbase(),
        "LET prefix='ab' FOR d IN myView FILTER "
        "starts_with(d['obj'][400]['properties'][3]['name'], CONCAT(prefix, "
        "'c')) RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET prefix='ab' FOR d IN myView FILTER "
        "starts_with(d.obj[400]['properties[3]']['name'], CONCAT(prefix, 'c')) "
        "RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET prefix='ab' FOR d IN myView FILTER "
        "starts_with(d.obj[400]['properties[3]'].name, CONCAT(prefix, 'c')) "
        "RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET prefix='ab' FOR d IN myView FILTER "
        "starts_with(d.obj[400].properties[3].name, CONCAT(prefix, 'c')) "
        "RETURN d",
        expected, &ctx);
  }

  // without scoring limit, complex name with offset, prefix as an expression via []
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("prefix", arangodb::aql::AqlValue("ab"));

    irs::Or expected;
    auto& orFilter = expected.add<irs::Or>();
    orFilter.min_match_count(arangodb::iresearch::FilterConstants::DefaultStartsWithMinMatchCount);
    auto& prefix0 = orFilter.add<irs::by_prefix>();
    *prefix0.mutable_field() = mangleStringIdentity("obj[400].properties[3].name");
    auto* opt0 = prefix0.mutable_options();
    opt0->term = irs::ref_cast<irs::byte_type>(irs::string_ref("abc"));
    opt0->scored_terms_limit = arangodb::iresearch::FilterConstants::DefaultScoringTermsLimit;

    auto& prefix1 = orFilter.add<irs::by_prefix>();
    *prefix1.mutable_field() = mangleStringIdentity("obj[400].properties[3].name");
    auto* opt1 = prefix1.mutable_options();
    opt1->term = irs::ref_cast<irs::byte_type>(irs::string_ref("def"));
    opt1->scored_terms_limit = arangodb::iresearch::FilterConstants::DefaultScoringTermsLimit;

    assertFilterSuccess(
        vocbase(),
        "LET prefix='ab' FOR d IN myView FILTER "
        "starts_with(d['obj'][400]['properties'][3]['name'], [CONCAT(prefix, "
        "'c'), 'def']) RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET prefix='ab' FOR d IN myView FILTER "
        "starts_with(d.obj[400]['properties[3]']['name'], [CONCAT(prefix, 'c'), 'def']) "
        "RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET prefix='ab' FOR d IN myView FILTER "
        "starts_with(d.obj[400]['properties[3]'].name, [CONCAT(prefix, 'c'), 'def']) "
        "RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET prefix='ab' FOR d IN myView FILTER "
        "starts_with(d.obj[400].properties[3].name, [CONCAT(prefix, 'c'), 'def']) "
        "RETURN d",
        expected, &ctx);
  }

  // without scoring limit, complex name with offset, prefix as an expression of invalid type
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("prefix",
                     arangodb::aql::AqlValue(arangodb::aql::AqlValueHintBool(false)));

    assertFilterExecutionFail(
        vocbase(),
        "LET prefix=false FOR d IN myView FILTER "
        "starts_with(d['obj'][400]['properties'][3]['name'], prefix) RETURN d",
        &ctx);
    assertFilterExecutionFail(
        vocbase(),
        "LET prefix=false FOR d IN myView FILTER "
        "starts_with(d.obj[400]['properties[3]']['name'], prefix) RETURN d",
        &ctx);
    assertFilterExecutionFail(
        vocbase(),
        "LET prefix=false FOR d IN myView FILTER "
        "starts_with(d.obj[400]['properties[3]'].name, prefix) RETURN d",
        &ctx);
    assertFilterExecutionFail(
        vocbase(),
        "LET prefix=false FOR d IN myView FILTER "
        "starts_with(d.obj[400].properties[3].name, prefix) RETURN d",
        &ctx);
  }

  // without scoring limit, complex name with offset, prefix as an expression of invalid type via []
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("prefix",
                     arangodb::aql::AqlValue(arangodb::aql::AqlValueHintBool(false)));

    assertFilterExecutionFail(
        vocbase(),
        "LET prefix=false FOR d IN myView FILTER "
        "starts_with(d['obj'][400]['properties'][3]['name'], [prefix, 'def']) RETURN d",
        &ctx);
    assertFilterExecutionFail(
        vocbase(),
        "LET prefix=false FOR d IN myView FILTER "
        "starts_with(d.obj[400]['properties[3]']['name'], [prefix, 'def']) RETURN d",
        &ctx);
    assertFilterExecutionFail(
        vocbase(),
        "LET prefix=false FOR d IN myView FILTER "
        "starts_with(d.obj[400]['properties[3]'].name, [prefix, 'def']) RETURN d",
        &ctx);
    assertFilterExecutionFail(
        vocbase(),
        "LET prefix=false FOR d IN myView FILTER "
        "starts_with(d.obj[400].properties[3].name, [prefix, 'def']) RETURN d",
        &ctx);
  }

  // empty array
  {
    irs::Or expected;
    auto& orFilter = expected.add<irs::Or>();
    orFilter.min_match_count(arangodb::iresearch::FilterConstants::DefaultStartsWithMinMatchCount);

    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER starts_with(d['name'], []) RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER starts_with(d.name, []) RETURN d", expected);
  }

  // with scoring limit (int)
  {
    irs::Or expected;
    auto& prefix = expected.add<irs::by_prefix>();
    *prefix.mutable_field() = mangleStringIdentity("name");
    auto* opt = prefix.mutable_options();
    opt->term = irs::ref_cast<irs::byte_type>(irs::string_ref("abc"));
    opt->scored_terms_limit = 1024;

    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER starts_with(d['name'], 'abc', 1024) RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER starts_with(d.name, 'abc', 1024) RETURN d", expected);
  }

  // with min match count (int) via[]
  {
    irs::Or expected;
    auto& orFilter = expected.add<irs::Or>();
    orFilter.min_match_count(2);
    auto& prefix = orFilter.add<irs::by_prefix>();
    *prefix.mutable_field() = mangleStringIdentity("name");
    auto* opt = prefix.mutable_options();
    opt->term = irs::ref_cast<irs::byte_type>(irs::string_ref("abc"));
    opt->scored_terms_limit = arangodb::iresearch::FilterConstants::DefaultScoringTermsLimit;

    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER starts_with(d['name'], ['abc'], 2) RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER starts_with(d.name, ['abc'], 2) RETURN d", expected);
  }

  // with scoring limit with min match count (int) via[]
  {
    irs::Or expected;
    auto& orFilter = expected.add<irs::Or>();
    orFilter.min_match_count(2);
    auto& prefix = orFilter.add<irs::by_prefix>();
    *prefix.mutable_field() = mangleStringIdentity("name");
    auto* opt = prefix.mutable_options();
    opt->term = irs::ref_cast<irs::byte_type>(irs::string_ref("abc"));
    opt->scored_terms_limit = 1024;

    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER starts_with(d['name'], ['abc'], 2, 1024) RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER starts_with(d.name, ['abc'], 2, 1024) RETURN d", expected);
  }

  // with scoring limit (double)
  {
    irs::Or expected;
    auto& prefix = expected.add<irs::by_prefix>();
    *prefix.mutable_field() = mangleStringIdentity("name");
    auto* opt = prefix.mutable_options();
    opt->term = irs::ref_cast<irs::byte_type>(irs::string_ref("abc"));
    opt->scored_terms_limit = 100;

    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER starts_with(d['name'], 'abc', 100.5) RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER starts_with(d.name, 'abc', 100.5) RETURN d", expected);
  }

  // with min match count (double) via[]
  {
    irs::Or expected;
    auto& orFilter = expected.add<irs::Or>();
    orFilter.min_match_count(2);
    auto& prefix = orFilter.add<irs::by_prefix>();
    *prefix.mutable_field() = mangleStringIdentity("name");
    auto* opt = prefix.mutable_options();
    opt->term = irs::ref_cast<irs::byte_type>(irs::string_ref("abc"));
    opt->scored_terms_limit = arangodb::iresearch::FilterConstants::DefaultScoringTermsLimit;

    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER starts_with(d['name'], ['abc'], 2.5) RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER starts_with(d.name, ['abc'], 2.5) RETURN d", expected);
  }

  // with scoring limit with min match count (double) via[]
  {
    irs::Or expected;
    auto& orFilter = expected.add<irs::Or>();
    orFilter.min_match_count(2);
    auto& prefix = orFilter.add<irs::by_prefix>();
    *prefix.mutable_field() = mangleStringIdentity("name");
    auto* opt = prefix.mutable_options();
    opt->term = irs::ref_cast<irs::byte_type>(irs::string_ref("abc"));
    opt->scored_terms_limit = 100;

    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER starts_with(d['name'], ['abc'], 2.5, 100.5) RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER starts_with(d.name, ['abc'], 2.5, 100.5) RETURN d", expected);
  }

  // with scoring limit (double), boost
  {
    irs::Or expected;
    expected.boost(3.1f);
    auto& prefix = expected.add<irs::by_prefix>();
    *prefix.mutable_field() = mangleStringIdentity("name");
    auto* opt = prefix.mutable_options();
    opt->term = irs::ref_cast<irs::byte_type>(irs::string_ref("abc"));
    opt->scored_terms_limit = 100;

    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER boost(starts_with(d['name'], 'abc', 100.5), "
        "0.1+3) RETURN d",
        expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER BooST(starts_with(d.name, 'abc', 100.5), 3.1) "
        "RETURN d",
        expected);
  }

  // with scoring limit with min match count (double), boost
  {
    irs::Or expected;
    expected.boost(3.1f);
    auto& orFilter = expected.add<irs::Or>();
    orFilter.min_match_count(2);
    auto& prefix = orFilter.add<irs::by_prefix>();
    *prefix.mutable_field() = mangleStringIdentity("name");
    auto* opt = prefix.mutable_options();
    opt->term = irs::ref_cast<irs::byte_type>(irs::string_ref("abc"));
    opt->scored_terms_limit = 100;

    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER boost(starts_with(d['name'], ['abc'], 2, 100.5), "
        "0.1+3) RETURN d",
        expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER BooST(starts_with(d.name, ['abc'], 2, 100.5), 3.1) "
        "RETURN d",
        expected);
  }

  // without scoring limit, complex name with offset, scoringLimit as an expression
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("prefix", arangodb::aql::AqlValue("ab"));
    ctx.vars.emplace("scoringLimit",
                     arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt(5)));

    irs::Or expected;
    auto& prefix = expected.add<irs::by_prefix>();
    *prefix.mutable_field() = mangleStringIdentity("obj[400].properties[3].name");
    auto* opt = prefix.mutable_options();
    opt->term = irs::ref_cast<irs::byte_type>(irs::string_ref("abc"));
    opt->scored_terms_limit = 6;

    assertFilterSuccess(
        vocbase(),
        "LET scoringLimit=5 LET prefix='ab' FOR d IN myView FILTER "
        "starts_with(d['obj'][400]['properties'][3]['name'], CONCAT(prefix, "
        "'c'), (scoringLimit + 1)) RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET scoringLimit=5 LET prefix='ab' FOR d IN myView FILTER "
        "starts_with(d.obj[400]['properties[3]']['name'], CONCAT(prefix, 'c'), "
        "(scoringLimit + 1)) RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET scoringLimit=5 LET prefix='ab' FOR d IN myView FILTER "
        "starts_with(d.obj[400]['properties[3]'].name, CONCAT(prefix, 'c'), "
        "(scoringLimit + 1)) RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET scoringLimit=5 LET prefix='ab' FOR d IN myView FILTER "
        "starts_with(d.obj[400].properties[3].name, CONCAT(prefix, 'c'), "
        "(scoringLimit + 1)) RETURN d",
        expected, &ctx);
  }

  // without scoring limit, complex name with offset, scoringLimit as an expression
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("prefix", arangodb::aql::AqlValue("ab"));
    ctx.vars.emplace("scoringLimit",
                     arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt(5)));

    irs::Or expected;
    auto& prefix = expected.add<irs::by_prefix>();
    *prefix.mutable_field() = mangleStringIdentity("obj[400].properties[3].name");
    auto* opt = prefix.mutable_options();
    opt->term = irs::ref_cast<irs::byte_type>(irs::string_ref("abc"));
    opt->scored_terms_limit = 6;

    assertFilterSuccess(
        vocbase(),
        "LET scoringLimit=5 LET prefix='ab' FOR d IN myView FILTER "
        "starts_with(d['obj'][400]['properties'][3]['name'], CONCAT(prefix, "
        "'c'), (scoringLimit + 1.5)) RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET scoringLimit=5 LET prefix='ab' FOR d IN myView FILTER "
        "starts_with(d.obj[400]['properties[3]']['name'], CONCAT(prefix, 'c'), "
        "(scoringLimit + 1.5)) RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET scoringLimit=5 LET prefix='ab' FOR d IN myView FILTER "
        "starts_with(d.obj[400]['properties[3]'].name, CONCAT(prefix, 'c'), "
        "(scoringLimit + 1.5)) RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET scoringLimit=5 LET prefix='ab' FOR d IN myView FILTER "
        "starts_with(d.obj[400].properties[3].name, CONCAT(prefix, 'c'), "
        "(scoringLimit + 1.5)) RETURN d",
        expected, &ctx);
  }

  // without scoring limit, complex name with offset, scoringLimit as an expression, analyzer
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("prefix", arangodb::aql::AqlValue("ab"));
    ctx.vars.emplace("analyzer", arangodb::aql::AqlValue("analyzer"));
    ctx.vars.emplace("scoringLimit",
                     arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt(5)));

    irs::Or expected;
    auto& prefix = expected.add<irs::by_prefix>();
    *prefix.mutable_field() = mangleString("obj[400].properties[3].name", "test_analyzer");
    auto* opt = prefix.mutable_options();
    opt->term = irs::ref_cast<irs::byte_type>(irs::string_ref("abc"));
    opt->scored_terms_limit = 6;

    assertFilterSuccess(
        vocbase(),
        "LET scoringLimit=5 LET prefix='ab' LET analyzer='analyzer' FOR d IN "
        "myView FILTER "
        "analyzer(starts_with(d['obj'][400]['properties'][3]['name'], "
        "CONCAT(prefix, 'c'), (scoringLimit + 1.5)), CONCAT('test_',analyzer)) "
        "RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET scoringLimit=5 LET prefix='ab' LET analyzer='analyzer' FOR d IN "
        "myView FILTER "
        "analyzer(starts_with(d.obj[400]['properties[3]']['name'], "
        "CONCAT(prefix, 'c'), (scoringLimit + 1.5)), CONCAT('test_',analyzer)) "
        " RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET scoringLimit=5 LET prefix='ab' LET analyzer='analyzer' FOR d IN "
        "myView FILTER analyzer(starts_with(d.obj[400]['properties[3]'].name, "
        "CONCAT(prefix, 'c'), (scoringLimit + 1.5)), CONCAT('test_',analyzer)) "
        " RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET scoringLimit=5 LET prefix='ab' LET analyzer='analyzer' FOR d IN "
        "myView FILTER analyzer(starts_with(d.obj[400].properties[3].name, "
        "CONCAT(prefix, 'c'), (scoringLimit + 1.5)), CONCAT('test_',analyzer)) "
        " RETURN d",
        expected, &ctx);
  }

  // without scoring limit, complex name with offset, scoringLimit as an expression of invalid type
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("prefix", arangodb::aql::AqlValue("ab"));
    ctx.vars.emplace("scoringLimit", arangodb::aql::AqlValue("ab"));

    assertFilterExecutionFail(
        vocbase(),
        "LET scoringLimit='ab' LET prefix=false FOR d IN myView FILTER "
        "starts_with(d['obj'][400]['properties'][3]['name'], prefix, "
        "scoringLimit) RETURN d",
        &ctx);
    assertFilterExecutionFail(
        vocbase(),
        "LET scoringLimit='ab' LET prefix=false FOR d IN myView FILTER "
        "starts_with(d.obj[400]['properties[3]']['name'], prefix, "
        "scoringLimit) RETURN d",
        &ctx);
    assertFilterExecutionFail(
        vocbase(),
        "LET scoringLimit='ab' LET prefix=false FOR d IN myView FILTER "
        "starts_with(d.obj[400]['properties[3]'].name, prefix, scoringLimit) "
        "RETURN d",
        &ctx);
    assertFilterExecutionFail(
        vocbase(),
        "LET scoringLimit='ab' LET prefix=false FOR d IN myView FILTER "
        "starts_with(d.obj[400].properties[3].name, prefix, scoringLimit) "
        "RETURN d",
        &ctx);
  }

  // with min match count and scoring limit (int) via[]
  {
    irs::Or expected;
    auto& orFilter = expected.add<irs::Or>();
    auto& prefix = orFilter.add<irs::by_prefix>();
    *prefix.mutable_field() = mangleStringIdentity("name");
    auto* opt = prefix.mutable_options();
    opt->term = irs::ref_cast<irs::byte_type>(irs::string_ref("abc"));
    opt->scored_terms_limit = 1024;

    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER starts_with(d['name'], ['abc'], 1, 1024) RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER starts_with(d.name, ['abc'], 1, 1024) RETURN d", expected);
  }

  // with min match count as an expression via[]
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("minMatchCount",
                     arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt(5)));

    irs::Or expected;
    auto& orFilter = expected.add<irs::Or>();
    orFilter.min_match_count(5);
    auto& prefix = orFilter.add<irs::by_prefix>();
    *prefix.mutable_field() = mangleStringIdentity("name");
    auto* opt = prefix.mutable_options();
    opt->term = irs::ref_cast<irs::byte_type>(irs::string_ref("abc"));
    opt->scored_terms_limit = arangodb::iresearch::FilterConstants::DefaultScoringTermsLimit;

    assertFilterSuccess(
        vocbase(),
        "LET minMatchCount=5 FOR d IN myView FILTER starts_with(d['name'], ['abc'], minMatchCount) RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET minMatchCount=5 FOR d IN myView FILTER starts_with(d.name, ['abc'], minMatchCount) RETURN d",
        expected, &ctx);
  }

  // wrong number of arguments
  assertFilterParseFail(vocbase(),
                        "FOR d IN myView FILTER starts_with() RETURN d");
  assertFilterParseFail(
      vocbase(),
      "FOR d IN myView FILTER starts_with(d.name, 'abc', 100, 100, 'abc') RETURN d");

  // invalid attribute access
  assertFilterFail(vocbase(),
                   "FOR d IN myView FILTER starts_with(['d'], 'abc') RETURN d");
  assertFilterFail(vocbase(),
                   "FOR d IN myView FILTER starts_with([d], 'abc') RETURN d");
  assertFilterFail(vocbase(),
                   "FOR d IN myView FILTER starts_with(d, 'abc') RETURN d");
  assertFilterFail(vocbase(),
                   "FOR d IN myView FILTER starts_with(d[*], 'abc') RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER starts_with(d.a[*].c, 'abc') RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER starts_with('d.name', 'abc') RETURN d");
  assertFilterFail(vocbase(),
                   "FOR d IN myView FILTER starts_with(123, 'abc') RETURN d");
  assertFilterFail(vocbase(),
                   "FOR d IN myView FILTER starts_with(123.5, 'abc') RETURN d");
  assertFilterFail(vocbase(),
                   "FOR d IN myView FILTER starts_with(null, 'abc') RETURN d");
  assertFilterFail(vocbase(),
                   "FOR d IN myView FILTER starts_with(true, 'abc') RETURN d");
  assertFilterFail(vocbase(),
                   "FOR d IN myView FILTER starts_with(false, 'abc') RETURN d");

  // invalid value
  assertFilterFail(vocbase(),
                   "FOR d IN myView FILTER starts_with(d.name, 1) RETURN d");
  assertFilterFail(vocbase(),
                   "FOR d IN myView FILTER starts_with(d.name, 1.5) RETURN d");
  assertFilterFail(vocbase(),
                   "FOR d IN myView FILTER starts_with(d.name, true) RETURN d");
  assertFilterFail(
      vocbase(), "FOR d IN myView FILTER starts_with(d.name, false) RETURN d");
  assertFilterFail(vocbase(),
                   "FOR d IN myView FILTER starts_with(d.name, null) RETURN d");
  assertFilterExecutionFail(
      vocbase(), "FOR d IN myView FILTER starts_with(d.name, d) RETURN d",
      &ExpressionContextMock::EMPTY);

  // invalid scoring limit
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER starts_with(d.name, 'abc', '1024') RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER starts_with(d.name, 'abc', true) RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER starts_with(d.name, 'abc', false) RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER starts_with(d.name, 'abc', null) RETURN d");
  assertFilterExecutionFail(
      vocbase(), "FOR d IN myView FILTER starts_with(d.name, 'abc', d) RETURN d",
      &ExpressionContextMock::EMPTY);

  // invalid min match count
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER starts_with(d.name, ['abc'], '1024') RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER starts_with(d.name, ['abc'], true) RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER starts_with(d.name, ['abc'], false) RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER starts_with(d.name, ['abc'], null) RETURN d");
  assertFilterExecutionFail(
      vocbase(), "FOR d IN myView FILTER starts_with(d.name, ['abc'], d) RETURN d",
      &ExpressionContextMock::EMPTY);

  // invalid scoring limit with min match count
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER starts_with(d.name, ['abc'], 1, '1024') RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER starts_with(d.name, ['abc'], 1, true) RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER starts_with(d.name, ['abc'], 1, false) RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER starts_with(d.name, ['abc'], 1, null) RETURN d");
  assertFilterExecutionFail(
      vocbase(), "FOR d IN myView FILTER starts_with(d.name, ['abc'], 1, d) RETURN d",
      &ExpressionContextMock::EMPTY);

  // non-deterministic arguments
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER starts_with(d[RAND() ? 'name' : 'x'], 'abc') "
      "RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER starts_with(d.name, RAND() ? 'abc' : 'def') "
      "RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER starts_with(d.name, 'abc', RAND() ? 128 : 10) "
      "RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER starts_with(d.name, ['abc'], RAND() ? 128 : 10) "
      "RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER starts_with(d.name, ['abc'], 1, RAND() ? 128 : 10) "
      "RETURN d");
}

TEST_F(IResearchFilterFunctionTest, wildcard) {
  // d.name LIKE 'foo'
  {
    irs::Or expected;
    auto& wildcard = expected.add<irs::by_wildcard>();
    *wildcard.mutable_field() = mangleStringIdentity("name");
    auto* opt = wildcard.mutable_options();
    opt->term = irs::ref_cast<irs::byte_type>(irs::string_ref("foo"));
    opt->scored_terms_limit = arangodb::iresearch::FilterConstants::DefaultScoringTermsLimit;

    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER d.name LIKE 'foo' RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER LIKE(d['name'], 'foo') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER LIKE(d.name, 'foo') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER LIKE(d.name, 'foo') RETURN d",
        expected);
  }

  // ANALYZER(d.name.foo LIKE 'foo%', 'test_analyzer')
  {
    irs::Or expected;
    auto& wildcard = expected.add<irs::by_wildcard>();
    *wildcard.mutable_field() = mangleString("name.foo", "test_analyzer");
    auto* opt = wildcard.mutable_options();
    opt->term = irs::ref_cast<irs::byte_type>(irs::string_ref("foo%"));
    opt->scored_terms_limit = arangodb::iresearch::FilterConstants::DefaultScoringTermsLimit;

    ExpressionContextMock ctx;
    ctx.vars.emplace("x", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"foo"}));

    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER ANALYZER(d.name.foo LIKE 'foo%', 'test_analyzer') RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER ANALYZER(LIKE(d.name[_FORWARD_('foo')], 'foo%'), 'test_analyzer') RETURN d",
        expected, &ctx);

    assertFilterSuccess(
        vocbase(),
        "LET x = 'foo' FOR d IN myView FILTER ANALYZER(LIKE(d.name[x], 'foo%'), 'test_analyzer') RETURN d",
        expected,
        &ctx);

    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER ANALYZER(LIKE(d['name'].foo, 'foo%'), 'test_analyzer') RETURN d",
        expected, &ctx);
  }

  // BOOST(ANALYZER(d.name[4] LIKE '_foo%', 'test_analyzer'), 0.5)
  {
    irs::Or expected;
    auto& wildcard = expected.add<irs::by_wildcard>();
    *wildcard.mutable_field() = mangleString("name[4]", "test_analyzer");
    wildcard.boost(0.5f);
    auto* opt = wildcard.mutable_options();
    opt->term = irs::ref_cast<irs::byte_type>(irs::string_ref("_foo%"));
    opt->scored_terms_limit = arangodb::iresearch::FilterConstants::DefaultScoringTermsLimit;

    ExpressionContextMock ctx;
    ctx.vars.emplace("x", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4}));

    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER BOOST(ANALYZER(d.name[4] LIKE '_foo%', 'test_analyzer'), 0.5) RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER BOOST(ANALYZER(LIKE(d['name'][_FORWARD_(4)], '_foo%'), 'test_analyzer'), 0.5) RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET x = 4 FOR d IN myView FILTER BOOST(ANALYZER(LIKE(d['name'][x], '_foo%'), 'test_analyzer'), 0.5) RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER BOOST(ANALYZER(LIKE(d.name[4], '_foo%'), 'test_analyzer'), 0.5) RETURN d",
        expected);
  }

  // invalid attribute access
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER [d] LIKE '_foo%' RETURN d",
      &ExpressionContextMock::EMPTY);
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER d[*] LIKE '_foo%' RETURN d",
      &ExpressionContextMock::EMPTY);
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER d.name[*] LIKE '_foo%' RETURN d",
      &ExpressionContextMock::EMPTY);
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER LIKE(d.foo[*].name, '_foo%') RETURN d",
      &ExpressionContextMock::EMPTY);
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER 'foo' LIKE 'f%' RETURN d",
      &ExpressionContextMock::EMPTY);
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER [] LIKE 'f%' RETURN d",
      &ExpressionContextMock::EMPTY);
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER {} LIKE 'f%' RETURN d",
      &ExpressionContextMock::EMPTY);
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER null LIKE 'f%' RETURN d",
      &ExpressionContextMock::EMPTY);
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER true LIKE 'f%' RETURN d",
      &ExpressionContextMock::EMPTY);

  // non-deterministic attribute access
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER LIKE(RAND() > 0.5 ? d.foo.name : d.foo.bar, '_foo%') RETURN d",
      &ExpressionContextMock::EMPTY);

  // invalid pattern
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER LIKE(d.foo, true) RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER LIKE(d.foo, null) RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER LIKE(d.foo, 1) RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER LIKE(d.foo, []) RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER LIKE(d.foo, {}) RETURN d");
  assertFilterExecutionFail(
      vocbase(),
      "FOR d IN myView FILTER LIKE(d.foo, _FORWARD_({})) RETURN d",
      &ExpressionContextMock::EMPTY);
  assertFilterExecutionFail(
      vocbase(),
      "FOR d IN myView FILTER LIKE(d.foo, d) RETURN d",
      &ExpressionContextMock::EMPTY);

  // wrong number of arguments
  assertFilterParseFail(
      vocbase(),
      "FOR d IN myView FILTER LIKE(d.name, 'abc', true, 'z') RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER LIKE(d.name, 'abc', true) RETURN d");
  assertFilterParseFail(
      vocbase(),
      "FOR d IN myView FILTER LIKE(d.name) RETURN d");
}

TEST_F(IResearchFilterFunctionTest, levenshteinMatch) {
  // LEVENSHTEIN_MATCH(d.name, 'foo', 1)
  {
    irs::Or expected;
    auto& filter = expected.add<irs::by_edit_distance>();
    *filter.mutable_field() = mangleStringIdentity("name");
    auto* opts = filter.mutable_options();
    opts->max_distance = 1;
    opts->with_transpositions = true;
    opts->term = irs::ref_cast<irs::byte_type>(irs::string_ref("foo"));
    opts->max_terms = arangodb::iresearch::FilterConstants::DefaultLevenshteinTermsLimit;

    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER LEVENSHTEIN_MATCH(d.name, 'foo', 1) RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER LEVENSHTEIN_match(d['name'], 'foo', 1) RETURN d",
        expected);
  }

  // LEVENSHTEIN_MATCH(d.name, 'foo', 1, false, 42)
  {
    irs::Or expected;
    auto& filter = expected.add<irs::by_edit_distance>();
    *filter.mutable_field() = mangleStringIdentity("name");
    auto* opts = filter.mutable_options();
    opts->max_distance = 1;
    opts->with_transpositions = false;
    opts->term = irs::ref_cast<irs::byte_type>(irs::string_ref("foo"));
    opts->max_terms = 42;

    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER LEVENSHTEIN_MATCH(d.name, 'foo', 1, false, 42) RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER LEVENSHTEIN_match(d['name'], 'foo', 1, false, 42) RETURN d",
        expected);
  }

  // ANALYZER(LEVENSHTEIN_MATCH(d.name.foo, 'foo', 0, true), 'test_analyzer')
  {
    irs::Or expected;
    auto& filter = expected.add<irs::by_edit_distance>();
    *filter.mutable_field() = mangleString("name.foo", "test_analyzer");
    auto* opts = filter.mutable_options();
    opts->max_distance = 0;
    opts->with_transpositions = true;
    opts->term = irs::ref_cast<irs::byte_type>(irs::string_ref("fooo"));
    opts->max_terms = arangodb::iresearch::FilterConstants::DefaultLevenshteinTermsLimit;

    ExpressionContextMock ctx;
    ctx.vars.emplace("x", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"foo"}));
    ctx.vars.emplace("y", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"o"}));
    ctx.vars.emplace("dist", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{1}));
    ctx.vars.emplace("transp", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintBool{true}));

    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER ANALYZER(LEVENSHTEIN_MATCH(d.name.foo, 'fooo', 0, true), 'test_analyzer') RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER ANALYZER(LEVENSHTEIN_MATCH(d.name[_FORWARD_('foo')], 'fooo', 0, true), 'test_analyzer') RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER ANALYZER(LEVENSHTEIN_MATCH(d.name[_FORWARD_('foo')], 'fooo', 0), 'test_analyzer') RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET y='o' LET transp=true LET dist=1 LET x='foo' FOR d IN myView FILTER ANALYZER(LEVENSHTEIN_MATCH(d.name[x], CONCAT('foo', y), dist-1, transp), 'test_analyzer') RETURN d",
        expected,
        &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET transp=true LET dist=1 LET x='foo' FOR d IN myView FILTER ANALYZER(LEVENSHTEIN_MATCH(d['name'].foo, 'fooo', dist-1, transp), 'test_analyzer') RETURN d",
        expected,
        &ctx);
  }

  // BOOST(ANALYZER(LEVENSHTEIN_DISTANCE(d.name[4], 'fooo', 2, false), 'test_analyzer'), 0.5)
  {
    irs::Or expected;
    auto& filter = expected.add<irs::by_edit_distance>();
    filter.boost(0.5);
    *filter.mutable_field() = mangleString("name[4]", "test_analyzer");
    auto* opts = filter.mutable_options();
    opts->max_distance = 2;
    opts->with_transpositions = false;
    opts->term = irs::ref_cast<irs::byte_type>(irs::string_ref("fooo"));
    opts->max_terms = arangodb::iresearch::FilterConstants::DefaultLevenshteinTermsLimit;

    ExpressionContextMock ctx;
    ctx.vars.emplace("x", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4}));
    ctx.vars.emplace("y", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"o"}));
    ctx.vars.emplace("dist", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{1}));
    ctx.vars.emplace("transp", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintBool{false}));

    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER BOOST(ANALYZER(LEVENSHTEIN_MATCH(d.name[4], 'fooo', 2, false), 'test_analyzer'), 0.5) RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER ANALYZER(BOOST(LEVENSHTEIN_MATCH(d.name[4], 'fooo', 2, false), 0.5), 'test_analyzer') RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER BOOST(ANALYZER(LEVENSHTEIN_MATCH(d.name[_FORWARD_(4)], 'fooo', 2, false), 'test_analyzer'), 0.5) RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET y='o' LET transp=false LET dist=1 LET x='foo' FOR d IN myView FILTER ANALYZER(BOOST(LEVENSHTEIN_MATCH(d.name[x], CONCAT('foo', y), dist+1, transp), 0.5), 'test_analyzer') RETURN d",
        expected,
        &ctx);
  }

  // BOOST(ANALYZER(LEVENSHTEIN_DISTANCE(d.name[4], 'fooo', 2, false, 0), 'test_analyzer'), 0.5)
  {
    irs::Or expected;
    auto& filter = expected.add<irs::by_edit_distance>();
    filter.boost(0.5);
    *filter.mutable_field() = mangleString("name[4]", "test_analyzer");
    auto* opts = filter.mutable_options();
    opts->max_distance = 2;
    opts->with_transpositions = false;
    opts->term = irs::ref_cast<irs::byte_type>(irs::string_ref("fooo"));
    opts->max_terms = 0;

    ExpressionContextMock ctx;
    ctx.vars.emplace("x", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{4}));
    ctx.vars.emplace("y", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"o"}));
    ctx.vars.emplace("dist", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{1}));
    ctx.vars.emplace("transp", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintBool{false}));

    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER BOOST(ANALYZER(LEVENSHTEIN_MATCH(d.name[4], 'fooo', 2, false, 0), 'test_analyzer'), 0.5) RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER ANALYZER(BOOST(LEVENSHTEIN_MATCH(d.name[4], 'fooo', 2, false, _FORWARD_(0)), 0.5), 'test_analyzer') RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER BOOST(ANALYZER(LEVENSHTEIN_MATCH(d.name[_FORWARD_(4)], 'fooo', 2, false, 0), 'test_analyzer'), 0.5) RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER ANALYZER(BOOST(LEVENSHTEIN_MATCH(d.name[4], 'fooo', 2, false, 0.1), 0.5), 'test_analyzer') RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET y='o' LET transp=false LET dist=1 LET x='foo' FOR d IN myView FILTER ANALYZER(BOOST(LEVENSHTEIN_MATCH(d.name[x], CONCAT('foo', y), dist+1, transp, x*10+2-42), 0.5), 'test_analyzer') RETURN d",
        expected,
        &ctx);
  }

  // invalid attribute access
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER LEVENSHTEIN_MATCH([d], 'fooo', 1, false) RETURN d",
      &ExpressionContextMock::EMPTY);
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER LEVENSHTEIN_MATCH(d[*], 'fooo', 1, false) RETURN d",
      &ExpressionContextMock::EMPTY);
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER LEVENSHTEIN_MATCH(d.name[*], 'fooo', 1, false) RETURN d",
      &ExpressionContextMock::EMPTY);
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER LEVENSHTEIN_MATCH(d.foo[*].name, 'fooo', 1, false) RETURN d",
      &ExpressionContextMock::EMPTY);
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER LEVENSHTEIN_MATCH('foo', 'fooo', 1, false) RETURN d",
      &ExpressionContextMock::EMPTY);
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER LEVENSHTEIN_MATCH([], 'fooo', 1, false) RETURN d",
      &ExpressionContextMock::EMPTY);
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER LEVENSHTEIN_MATCH({}, 'fooo', 1, false) RETURN d",
      &ExpressionContextMock::EMPTY);
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER LEVENSHTEIN_MATCH(null, 'fooo', 1, false) RETURN d",
      &ExpressionContextMock::EMPTY);
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER LEVENSHTEIN_MATCH(true, 'fooo', 1, false) RETURN d",
      &ExpressionContextMock::EMPTY);

  // non-deterministic attribute access
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER LEVENSHTEIN_MATCH(RAND() > 0.5 ? d.foo.name : d.foo.bar, 'fooo', 1, false) RETURN d",
      &ExpressionContextMock::EMPTY);

  // invalid target
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER LEVENSHTEIN_MATCH(d.foo, true, 1, false) RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER LEVENSHTEIN_MATCH(d.foo, null, 1, false) RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER LEVENSHTEIN_MATCH(d.foo, 1, 1, false) RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER LEVENSHTEIN_MATCH(d.foo, [], 1, false) RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER LEVENSHTEIN_MATCH(d.foo, {}, 1, false) RETURN d");
  assertFilterExecutionFail(
      vocbase(),
      "FOR d IN myView FILTER LEVENSHTEIN_MATCH(d.foo, _FORWARD_({}), 1, false) RETURN d",
      &ExpressionContextMock::EMPTY);
  assertFilterExecutionFail(
      vocbase(),
      "FOR d IN myView FILTER LEVENSHTEIN_MATCH(d.foo, d, 1, false) RETURN d",
      &ExpressionContextMock::EMPTY);

  // invalid distance
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER LEVENSHTEIN_MATCH(d.foo, 'foo', 5, false) RETURN d");
  assertFilterExecutionFail(
      vocbase(),
      "FOR d IN myView FILTER LEVENSHTEIN_MATCH(d.foo, 'foo', -1, false) RETURN d",
      &ExpressionContextMock::EMPTY);
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("x", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{-1}));

    assertFilterExecutionFail(
        vocbase(),
        "LET x=-1 FOR d IN myView FILTER LEVENSHTEIN_MATCH(d.foo, 'foo', x, false) RETURN d",
        &ExpressionContextMock::EMPTY);
  }
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER LEVENSHTEIN_MATCH(d.foo, 'foo', null, false) RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER LEVENSHTEIN_MATCH(d.foo, 'foo', true, false) RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER LEVENSHTEIN_MATCH(d.foo, 'foo', '1', false) RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER LEVENSHTEIN_MATCH(d.foo, 'foo', [1], false) RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER LEVENSHTEIN_MATCH(d.foo, 'foo', {}, false) RETURN d");
  assertFilterExecutionFail(
      vocbase(),
      "FOR d IN myView FILTER LEVENSHTEIN_MATCH(d.foo, 'foo', _FORWARD_({}), false) RETURN d",
      &ExpressionContextMock::EMPTY);
  assertFilterExecutionFail(
      vocbase(),
      "FOR d IN myView FILTER LEVENSHTEIN_MATCH(d.foo, 'foo', d, false) RETURN d",
      &ExpressionContextMock::EMPTY);

  // invalid "with transpositions"
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER LEVENSHTEIN_MATCH(d.foo, 'foo', 1, 'true') RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER LEVENSHTEIN_MATCH(d.foo, 'foo', 1, null) RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER LEVENSHTEIN_MATCH(d.foo, 'foo', 1, 1) RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER LEVENSHTEIN_MATCH(d.foo, 'foo', 1, [false]) RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER LEVENSHTEIN_MATCH(d.foo, 'foo', 1, {}) RETURN d");
  assertFilterExecutionFail(
      vocbase(),
      "FOR d IN myView FILTER LEVENSHTEIN_MATCH(d.foo, 'foo', 1, _FORWARD_({})) RETURN d",
      &ExpressionContextMock::EMPTY);
  assertFilterExecutionFail(
      vocbase(),
      "FOR d IN myView FILTER LEVENSHTEIN_MATCH(d.foo, 'foo', 1, d) RETURN d",
      &ExpressionContextMock::EMPTY);

  // invalid "max_terms transpositions"
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER LEVENSHTEIN_MATCH(d.foo, 'foo', 1, true, '42') RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER LEVENSHTEIN_MATCH(d.foo, 'foo', 1, true, null) RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER LEVENSHTEIN_MATCH(d.foo, 'foo', 1, true, [42]) RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER LEVENSHTEIN_MATCH(d.foo, 'foo', 1, true, {}) RETURN d");
  assertFilterExecutionFail(
      vocbase(),
      "FOR d IN myView FILTER LEVENSHTEIN_MATCH(d.foo, 'foo', 1, true, _FORWARD_([42])) RETURN d",
      &ExpressionContextMock::EMPTY);
  assertFilterExecutionFail(
      vocbase(),
      "FOR d IN myView FILTER LEVENSHTEIN_MATCH(d.foo, 'foo', 1, true, d) RETURN d",
      &ExpressionContextMock::EMPTY);

  // wrong number of arguments
  assertFilterParseFail(
      vocbase(),
      "FOR d IN myView FILTER LEVENSHTEIN_MATCH(d.foo, 'true', 1, false, 1, 'z') RETURN d");
  assertFilterParseFail(
      vocbase(),
      "FOR d IN myView FILTER LEVENSHTEIN_MATCH(d.foo, 'true') RETURN d");
  assertFilterParseFail(
      vocbase(),
      "FOR d IN myView FILTER LEVENSHTEIN_MATCH(d.foo) RETURN d");
}

TEST_F(IResearchFilterFunctionTest, inRange) {
  // d.name > 'a' && d.name < 'z'
  {
    irs::Or expected;
    auto& range = expected.add<irs::by_range>();
    *range.mutable_field() = mangleStringIdentity("name");
    auto* opts = range.mutable_options();
    opts->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("a"));
    opts->range.min_type = irs::BoundType::EXCLUSIVE;
    opts->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("z"));
    opts->range.max_type = irs::BoundType::EXCLUSIVE;

    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER in_range(d['name'], 'a', 'z', false, false) "
        "RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER in_range(d.name, 'a', 'z', false, false) "
        "RETURN d",
        expected);
  }

  // BOOST(d.name >= 'a' && d.name <= 'z', 1.5)
  {
    irs::Or expected;
    auto& range = expected.add<irs::by_range>();
    range.boost(1.5);
    *range.mutable_field() = mangleStringIdentity("name");
    auto* opts = range.mutable_options();
    opts->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("a"));
    opts->range.min_type = irs::BoundType::INCLUSIVE;
    opts->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("z"));
    opts->range.max_type = irs::BoundType::INCLUSIVE;

    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER boost(in_range(d['name'], 'a', 'z', true, "
        "true), 1.5) RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER boost(in_range(d.name, 'a', 'z', true, true), "
        "1.5) RETURN d",
        expected);
  }

  // ANALYZER(BOOST(d.name > 'a' && d.name <= 'z', 1.5), "testVocbase::test_analyzer")
  {
    irs::Or expected;
    auto& range = expected.add<irs::by_range>();
    range.boost(1.5);
    *range.mutable_field() = mangleString("name", "test_analyzer");
    auto* opts = range.mutable_options();
    opts->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("a"));
    opts->range.min_type = irs::BoundType::EXCLUSIVE;
    opts->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("z"));
    opts->range.max_type = irs::BoundType::INCLUSIVE;

    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER analyzer(boost(in_range(d['name'], 'a', 'z', "
        "false, true), 1.5), 'testVocbase::test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER analyzer(boost(in_range(d.name, 'a', 'z', "
        "false, true), 1.5), 'testVocbase::test_analyzer') RETURN d",
        expected);
  }

  // dynamic complex attribute field
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                                      arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                                      arangodb::aql::AqlValueHintDouble{5.6})));

    irs::Or expected;
    auto& range = expected.add<irs::by_range>();
    *range.mutable_field() = mangleStringIdentity("a.b.c.e[4].f[5].g[3].g.a");
    auto* opts = range.mutable_options();
    opts->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("abc"));
    opts->range.min_type = irs::BoundType::INCLUSIVE;
    opts->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("bce"));
    opts->range.max_type = irs::BoundType::EXCLUSIVE;

    assertFilterSuccess(
        vocbase(),
        "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER "
        "in_range(d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_"
        "FORWARD_('a')], 'abc', 'bce', true, false) RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER "
        "in_range(d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_"
        "FORWARD_('a')], CONCAT(_FORWARD_('a'), _FORWARD_('bc')), "
        "CONCAT(_FORWARD_('bc'), _FORWARD_('e')), _FORWARD_(5) > _FORWARD_(4), "
        "_FORWARD_(5) > _FORWARD_(6)) RETURN d",
        expected, &ctx);
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
        vocbase(),
        "LET a=null LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER "
        "in_range(d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_"
        "FORWARD_('a')], 'abc', 'bce', true, false) RETURN d",
        &ctx);
  }

  // boolean expression in range, boost
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("numVal", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt(2)));

    irs::Or expected;
    auto& range = expected.add<irs::by_range>();
    range.boost(1.5);
    *range.mutable_field() = mangleBool("a.b.c.e.f");
    auto* opts = range.mutable_options();
    opts->range.min = irs::ref_cast<irs::byte_type>(irs::boolean_token_stream::value_true());
    opts->range.min_type = irs::BoundType::INCLUSIVE;
    opts->range.max = irs::ref_cast<irs::byte_type>(irs::boolean_token_stream::value_true());
    opts->range.max_type = irs::BoundType::INCLUSIVE;

    assertFilterSuccess(
        vocbase(),
        "LET numVal=2 FOR d IN collection FILTER boost(in_rangE(d.a.b.c.e.f, "
        "(numVal < 13), (numVal > 1), true, true), 1.5) RETURN d",
        expected,
        &ctx  // expression context
    );

    assertFilterSuccess(
        vocbase(),
        "LET numVal=2 FOR d IN collection FILTER boost(in_rangE(d.a.b.c.e.f, "
        "(numVal < 13), (numVal > 1), true, true), 1.5) RETURN d",
        expected,
        &ctx  // expression context
    );
  }

  // null expression in range, boost
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("nullVal", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintNull{}));

    irs::Or expected;
    auto& range = expected.add<irs::by_range>();
    range.boost(1.5);
    *range.mutable_field() = mangleNull("a.b.c.e.f");
    auto* opts = range.mutable_options();
    opts->range.min = irs::ref_cast<irs::byte_type>(irs::null_token_stream::value_null());
    opts->range.min_type = irs::BoundType::INCLUSIVE;
    opts->range.max = irs::ref_cast<irs::byte_type>(irs::null_token_stream::value_null());
    opts->range.max_type = irs::BoundType::INCLUSIVE;

    assertFilterSuccess(
        vocbase(),
        "LET nullVal=null FOR d IN collection FILTER "
        "BOOST(in_range(d.a.b.c.e.f, (nullVal && true), (nullVal && false), "
        "true, true), 1.5) RETURN d",
        expected,
        &ctx  // expression context
    );

    assertFilterSuccess(
        vocbase(),
        "LET nullVal=null FOR d IN collection FILTER "
        "bOoST(in_range(d.a.b.c.e.f, (nullVal && false), (nullVal && true), "
        "true, true), 1.5) RETURN d",
        expected,
        &ctx  // expression context
    );
  }

  // numeric expression in range, boost
  {
    irs::numeric_token_stream minTerm;
    minTerm.reset(15.5);
    irs::numeric_token_stream maxTerm;
    maxTerm.reset(40.);

    ExpressionContextMock ctx;
    ctx.vars.emplace("numVal", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt(2)));

    irs::Or expected;
    auto& range = expected.add<irs::by_granular_range>();
    range.boost(1.5);
    *range.mutable_field() = mangleNumeric("a.b.c.e.f");
    auto* opts = range.mutable_options();
    irs::set_granular_term(opts->range.min, minTerm);
    opts->range.min_type = irs::BoundType::INCLUSIVE;
    irs::set_granular_term(opts->range.max, maxTerm);
    opts->range.max_type = irs::BoundType::EXCLUSIVE;

    assertFilterSuccess(
        vocbase(),
        "LET numVal=2 FOR d IN collection FILTER "
        "boost(in_range(d.a['b'].c.e.f, (numVal + 13.5), (numVal + 38), true, "
        "false), 1.5) RETURN d",
        expected,
        &ctx  // expression context
    );

    assertFilterSuccess(
        vocbase(),
        "LET numVal=2 FOR d IN collection FILTER boost(IN_RANGE(d.a.b.c.e.f, "
        "(numVal + 13.5), (numVal + 38), true, false), 1.5) RETURN d",
        expected,
        &ctx  // expression context
    );

    assertFilterSuccess(
        vocbase(),
        "LET numVal=2 FOR d IN collection FILTER "
        "analyzer(boost(in_range(d.a.b.c.e.f, (numVal + 13.5), (numVal + 38), "
        "true, false), 1.5), 'test_analyzer') RETURN d",
        expected,
        &ctx  // expression context
    );
  }

  // invalid attribute access
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER in_range(['d'], 'abc', true, 'z', false) RETURN "
      "d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER in_range([d], 'abc', true, 'z', false) RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER in_range(d, 'abc', true, 'z', false) RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER in_range(d[*], 'abc', true, 'z', false) RETURN "
      "d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER in_range(d.a[*].c, 'abc', true, 'z', false) "
      "RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER in_range('d.name', 'abc', true, 'z', false) "
      "RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER in_range(123, 'abc', true, 'z', false) RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER in_range(123.5, 'abc', true, 'z', false) RETURN "
      "d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER in_range(null, 'abc', true, 'z', false) RETURN "
      "d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER in_range(true, 'abc', true, 'z', false) RETURN "
      "d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER in_range(false, 'abc', true, 'z', false) RETURN "
      "d");

  // invalid type of inclusion argument
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER in_range(d.name, 'abc', true, 'z', 'false') "
      "RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER in_range(d.name, 'abc', true, 'z', 0) RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER in_range(d.name, 'abc', true, 'z', null) RETURN "
      "d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER in_range(d.name, 'abc', 'true', 'z', false) "
      "RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER in_range(d.name, 'abc', 1, 'z', false) RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER in_range(d.name, 'abc', null, 'z', false) RETURN "
      "d");

  // non-deterministic argument
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER in_range(d[RAND() ? 'name' : 'x'], 'abc', true, "
      "'z', false) RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER in_range(d.name, RAND() ? 'abc' : 'def', true, "
      "'z', false) RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER in_range(d.name, 'abc', RAND() ? true : false, "
      "'z', false) RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER in_range(d.name, 'abc', true, RAND() ? 'z' : "
      "'x', false) RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER in_range(d.name, 'abc', true, 'z', RAND() ? "
      "false : true) RETURN d");

  // lower/upper boundary type mismatch
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER in_range(d.name, 1, true, 'z', false) RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER in_range(d.name, 'abc', true, null, false) "
      "RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER in_range(d.name, bool, true, null, false) RETURN "
      "d");
  assertFilterFail(
      vocbase(),
      "FOR d IN myView FILTER in_range(d.name, bool, true, 1, false) RETURN d");

  // wrong number of arguments
  assertFilterParseFail(
      vocbase(),
      "FOR d IN myView FILTER in_range(d.name, 'abc', true, 'z') RETURN d");
  assertFilterParseFail(
      vocbase(),
      "FOR d IN myView FILTER in_range(d.name, 'abc', true, 'z', false, false) "
      "RETURN d");
}

TEST_F(IResearchFilterFunctionTest, ngramMatch) {
  // NGRAM_MATCH with default analyzer default threshold
  {
    irs::Or expected;
    auto& filter = expected.add<irs::by_ngram_similarity>();
    *filter.mutable_field() = mangleStringIdentity("name");
    auto* opts = filter.mutable_options();
    opts->threshold = 0.7f;
    irs::bstring ngram; irs::assign(ngram, irs::string_ref("foo"));
    opts->ngrams.push_back(std::move(ngram));

    assertFilterSuccess(
      vocbase(),
      "FOR d IN myView FILTER NGRAM_MATCH(d.name, 'foo') RETURN d",
      expected);
    assertFilterSuccess(
      vocbase(),
      "FOR d IN myView FILTER NGRAM_match(d['name'], 'foo') RETURN d",
      expected);
  }

  // NGRAM_MATCH with default analyzer default threshold value by var
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("strVal", arangodb::aql::AqlValue("foo"));

    irs::Or expected;
    auto& filter = expected.add<irs::by_ngram_similarity>();
    *filter.mutable_field() = mangleStringIdentity("name");
    auto* opts = filter.mutable_options();
    opts->threshold = 0.7f;
    irs::bstring ngram; irs::assign(ngram, irs::string_ref("foo"));
    opts->ngrams.push_back(std::move(ngram));

    assertFilterSuccess(
      vocbase(),
      "LET strVal = 'foo' FOR d IN myView FILTER NGRAM_MATCH(d.name, strVal) RETURN d",
      expected, &ctx);
    assertFilterSuccess(
      vocbase(),
      "LET strVal = 'foo' FOR d IN myView FILTER NGRAM_match(d['name'], strVal) RETURN d",
      expected, &ctx);
  }

  // NGRAM_MATCH with boost
  {
    irs::Or expected;
    auto& filter = expected.add<irs::by_ngram_similarity>();
    filter.boost(1.5);
    *filter.mutable_field() = mangleStringIdentity("name");
    auto* opts = filter.mutable_options();
    opts->threshold = 0.7f;
    irs::bstring ngram; irs::assign(ngram, irs::string_ref("foo"));
    opts->ngrams.push_back(std::move(ngram));

    assertFilterSuccess(
      vocbase(),
      "FOR d IN myView FILTER BOOST(NGRAM_MATCH(d.name, 'foo'), 1.5) RETURN d",
      expected);
    assertFilterSuccess(
      vocbase(),
      "FOR d IN myView FILTER BOOST(NGRAM_match(d['name'], 'foo'), 1.5) RETURN d",
      expected);
  }

  // NGRAM_MATCH with default analyzer explicit threshold
  {
    irs::Or expected;
    auto& filter = expected.add<irs::by_ngram_similarity>();
    *filter.mutable_field() = mangleStringIdentity("name");
    auto* opts = filter.mutable_options();
    opts->threshold = 0.8f;
    irs::bstring ngram; irs::assign(ngram, irs::string_ref("foo"));
    opts->ngrams.push_back(std::move(ngram));

    assertFilterSuccess(
      vocbase(),
      "FOR d IN myView FILTER NGRAM_MATCH(d.name, 'foo', 0.8) RETURN d",
      expected);
    assertFilterSuccess(
      vocbase(),
      "FOR d IN myView FILTER NGRAM_match(d['name'], 'foo', 0.8) RETURN d",
      expected);
  }

  // NGRAM_MATCH with default analyzer explicit threshold via variable
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("numVal", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble(0.8)));

    irs::Or expected;
    auto& filter = expected.add<irs::by_ngram_similarity>();
    *filter.mutable_field() = mangleStringIdentity("name");
    auto* opts = filter.mutable_options();
    opts->threshold = 0.8f;
    irs::bstring ngram; irs::assign(ngram, irs::string_ref("foo"));
    opts->ngrams.push_back(std::move(ngram));

    assertFilterSuccess(
      vocbase(),
      "LET numVal = 0.8 FOR d IN myView FILTER NGRAM_MATCH(d.name, 'foo', numVal) RETURN d",
      expected, &ctx);
    assertFilterSuccess(
      vocbase(),
      "LET numVal = 0.8 FOR d IN myView FILTER NGRAM_match(d['name'], 'foo', numVal) RETURN d",
      expected, &ctx);
  }
  // variables + function calls
  {
    irs::Or expected;
    auto& filter = expected.add<irs::by_ngram_similarity>();
    *filter.mutable_field() = mangleString("name.foo", "test_analyzer");
    auto* opts = filter.mutable_options();
    opts->threshold = 0.5;
    opts->ngrams.push_back({static_cast<irs::byte_type>('f')});
    opts->ngrams.push_back({static_cast<irs::byte_type>('o')});
    opts->ngrams.push_back({static_cast<irs::byte_type>('o')});
    opts->ngrams.push_back({static_cast<irs::byte_type>('o')});

    ExpressionContextMock ctx;
    ctx.vars.emplace("x", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble{ 0.5 }));
    ctx.vars.emplace("y", arangodb::aql::AqlValue(arangodb::aql::AqlValue{ "o" }));
    ctx.vars.emplace("idx", arangodb::aql::AqlValue("foo"));

    assertFilterSuccess(
      vocbase(),
      "FOR d IN myView FILTER ANALYZER(NGRAM_MATCH(d.name[_FORWARD_('foo')], 'fooo', 0.5), 'test_analyzer') RETURN d",
      expected, &ctx);
    assertFilterSuccess(
      vocbase(),
      "LET y='o' LET idx='foo' LET x=0.5 FOR d IN myView FILTER ANALYZER(NGRAM_MATCH(d.name[idx], CONCAT('foo', y), x), 'test_analyzer') RETURN d",
      expected,
      &ctx);
  }


  // NGRAM_MATCH with explicit analyzer default threshold
  {
    irs::Or expected;
    auto& filter = expected.add<irs::by_ngram_similarity>();
    *filter.mutable_field() = mangleString("name", "test_analyzer");
    auto* opts = filter.mutable_options();
    opts->threshold = 0.7f;
    opts->ngrams.push_back({static_cast<irs::byte_type>('f')});
    opts->ngrams.push_back({static_cast<irs::byte_type>('o')});
    opts->ngrams.push_back({static_cast<irs::byte_type>('o')});

    assertFilterSuccess(
      vocbase(),
      "FOR d IN myView FILTER NGRAM_MATCH(d.name, 'foo', 'test_analyzer') RETURN d",
      expected);
    assertFilterSuccess(
      vocbase(),
      "FOR d IN myView FILTER NGRAM_match(d['name'], 'foo', 'test_analyzer') RETURN d",
      expected);
  }

  // NGRAM_MATCH with explicit analyzer via ANALYZER default threshold
  {
    irs::Or expected;
    auto& filter = expected.add<irs::by_ngram_similarity>();
    *filter.mutable_field() = mangleString("name", "test_analyzer");
    auto* opts = filter.mutable_options();
    opts->threshold = 0.7f;
    opts->ngrams.push_back({static_cast<irs::byte_type>('f')});
    opts->ngrams.push_back({static_cast<irs::byte_type>('o')});
    opts->ngrams.push_back({static_cast<irs::byte_type>('o')});

    assertFilterSuccess(
      vocbase(),
      "FOR d IN myView FILTER ANALYZER(NGRAM_MATCH(d.name, 'foo'), 'test_analyzer') RETURN d",
      expected);
    assertFilterSuccess(
      vocbase(),
      "FOR d IN myView FILTER ANALYZER(NGRAM_match(d['name'], 'foo'), 'test_analyzer') RETURN d",
      expected);
  }

  // NGRAM_MATCH with explicit analyzer explicit threshold
  {
    irs::Or expected;
    auto& filter = expected.add<irs::by_ngram_similarity>();
    *filter.mutable_field() = mangleString("name", "test_analyzer");
    auto* opts = filter.mutable_options();
    opts->threshold = 0.25f;
    opts->ngrams.push_back({static_cast<irs::byte_type>('f')});
    opts->ngrams.push_back({static_cast<irs::byte_type>('o')});
    opts->ngrams.push_back({static_cast<irs::byte_type>('o')});

    assertFilterSuccess(
      vocbase(),
      "FOR d IN myView FILTER NGRAM_MATCH(d.name, 'foo', 0.25, 'test_analyzer') RETURN d",
      expected);
    assertFilterSuccess(
      vocbase(),
      "FOR d IN myView FILTER NGRAM_match(d['name'], 'foo', 0.25, 'test_analyzer') RETURN d",
      expected);
  }

  // NGRAM_MATCH with explicit analyzer via ANALYZER explicit threshold
  {
    irs::Or expected;
    auto& filter = expected.add<irs::by_ngram_similarity>();
    *filter.mutable_field() = mangleString("name", "test_analyzer");
    auto* opts = filter.mutable_options();
    opts->threshold = 0.25f;
    opts->ngrams.push_back({static_cast<irs::byte_type>('f')});
    opts->ngrams.push_back({static_cast<irs::byte_type>('o')});
    opts->ngrams.push_back({static_cast<irs::byte_type>('o')});

    assertFilterSuccess(
      vocbase(),
      "FOR d IN myView FILTER ANALYZER(NGRAM_MATCH(d.name, 'foo', 0.25), 'test_analyzer') RETURN d",
      expected);
    assertFilterSuccess(
      vocbase(),
      "FOR d IN myView FILTER ANALYZER(NGRAM_match(d['name'], 'foo', 0.25), 'test_analyzer') RETURN d",
      expected);
  }

  // wrong number of arguments
  assertFilterParseFail(
    vocbase(),
    "FOR d IN myView FILTER NGRAM_MATCH(d.name) RETURN d");
  assertFilterParseFail(
    vocbase(),
    "FOR d IN myView FILTER NGRAM_MATCH(d['name']) RETURN d");
  assertFilterParseFail(
    vocbase(),
    "FOR d IN myView FILTER NGRAM_MATCH(d['name'], 1, 2, 3, 4) RETURN d");

  // invalid parameter order (overload with default analyzer)
  assertFilterFail(
    vocbase(),
    "FOR d IN myView FILTER NGRAM_MATCH(d.name, 0.5, 'foo') RETURN d");
  assertFilterFail(
    vocbase(),
    "FOR d IN myView FILTER NGRAM_MATCH(d['name'], 0.5, 'foo') RETURN d");

  // invalid parameter order (overload with default threshold)
  assertFilterFail(
    vocbase(),
    "FOR d IN myView FILTER NGRAM_MATCH('foo', d.name, 'test_analyzer') RETURN d");
  assertFilterFail(
    vocbase(),
    "FOR d IN myView FILTER NGRAM_MATCH('foo', d['name'], 'test_analyzer') RETURN d");

  // wrong first arg type
  assertFilterFail(
    vocbase(),
    "FOR d IN myView FILTER ANALYZER(NGRAM_MATCH(d[*], 'foo', 0.25), 'test_analyzer') RETURN d");
  assertFilterFail(
    vocbase(),
    "FOR d IN myView FILTER ANALYZER(NGRAM_MATCH('a', 'foo', 0.25), 'test_analyzer') RETURN d");
  assertFilterFail(
    vocbase(),
    "FOR d IN myView FILTER ANALYZER(NGRAM_match('a', 'foo', 0.25), 'test_analyzer') RETURN d");
  assertFilterFail(
    vocbase(),
    "FOR d IN myView FILTER ANALYZER(NGRAM_MATCH(1, 'foo', 0.25), 'test_analyzer') RETURN d");
  assertFilterFail(
    vocbase(),
    "FOR d IN myView FILTER ANALYZER(NGRAM_match(1, 'foo', 0.25), 'test_analyzer') RETURN d");
  assertFilterFail(
    vocbase(),
    "FOR d IN myView FILTER ANALYZER(NGRAM_MATCH(null, 'foo', 0.25), 'test_analyzer') RETURN d");
  assertFilterFail(
    vocbase(),
    "FOR d IN myView FILTER ANALYZER(NGRAM_match(null, 'foo', 0.25), 'test_analyzer') RETURN d");
  assertFilterFail(
    vocbase(),
    "FOR d IN myView FILTER ANALYZER(NGRAM_MATCH(['a'], 'foo', 0.25), 'test_analyzer') RETURN d");
  assertFilterFail(
    vocbase(),
    "FOR d IN myView FILTER ANALYZER(NGRAM_match(['a'], 'foo', 0.25), 'test_analyzer') RETURN d");
  assertFilterFail(
    vocbase(),
    "FOR d IN myView FILTER ANALYZER(NGRAM_MATCH({a:1}, 'foo', 0.25), 'test_analyzer') RETURN d");
  assertFilterFail(
    vocbase(),
    "FOR d IN myView FILTER ANALYZER(NGRAM_match({a:1}, 'foo', 0.25), 'test_analyzer') RETURN d");

  // wrong second arg type
  assertFilterFail(
    vocbase(),
    "FOR d IN myView FILTER NGRAM_MATCH(d.name, 0.5) RETURN d");
  assertFilterFail(
    vocbase(),
    "FOR d IN myView FILTER NGRAM_MATCH(d['name'], 0.5) RETURN d");

  assertFilterFail(
    vocbase(),
    "FOR d IN myView FILTER NGRAM_MATCH(d.name, [1, 2]) RETURN d");
  assertFilterFail(
    vocbase(),
    "FOR d IN myView FILTER NGRAM_MATCH(d['name'], [1, 2]) RETURN d");

  assertFilterFail(
    vocbase(),
    "FOR d IN myView FILTER NGRAM_MATCH(d.name, {a: 1 }) RETURN d");
  assertFilterFail(
    vocbase(),
    "FOR d IN myView FILTER NGRAM_MATCH(d['name'], {a: 1 }) RETURN d");

  assertFilterFail(
    vocbase(),
    "FOR d IN myView FILTER NGRAM_MATCH(d.name, true) RETURN d");
  assertFilterFail(
    vocbase(),
    "FOR d IN myView FILTER NGRAM_MATCH(d['name'], true) RETURN d");

  assertFilterFail(
    vocbase(),
    "FOR d IN myView FILTER NGRAM_MATCH(d.name, null) RETURN d");
  assertFilterFail(
    vocbase(),
    "FOR d IN myView FILTER NGRAM_MATCH(d['name'], null) RETURN d");

  // wrong third argument type (may be only string or double)
  assertFilterFail(
    vocbase(),
    "FOR d IN myView FILTER NGRAM_MATCH(d.name, 'abc', null) RETURN d");
  assertFilterFail(
    vocbase(),
    "FOR d IN myView FILTER NGRAM_MATCH(d['name'], 'abc', null) RETURN d");
  assertFilterFail(
    vocbase(),
    "FOR d IN myView FILTER NGRAM_MATCH(d.name, 'abc', true) RETURN d");
  assertFilterFail(
    vocbase(),
    "FOR d IN myView FILTER NGRAM_MATCH(d['name'], 'abc', true) RETURN d");
  assertFilterFail(
    vocbase(),
    "FOR d IN myView FILTER NGRAM_MATCH(d.name, 'abc', [1, 2]) RETURN d");
  assertFilterFail(
    vocbase(),
    "FOR d IN myView FILTER NGRAM_MATCH(d['name'], 'abc', [1, 2]) RETURN d");
  assertFilterFail(
    vocbase(),
    "FOR d IN myView FILTER NGRAM_MATCH(d.name, 'abc', {a:1}) RETURN d");
  assertFilterFail(
    vocbase(),
    "FOR d IN myView FILTER NGRAM_MATCH(d['name'], 'abc', {a:1}) RETURN d");

  // invalid threshold value
  assertFilterFail(
    vocbase(),
    "FOR d IN myView FILTER NGRAM_MATCH(d.name, 'abc', 1.1) RETURN d");
  assertFilterFail(
    vocbase(),
    "FOR d IN myView FILTER NGRAM_MATCH(d['name'], 'abc', 1.1) RETURN d");
  assertFilterFail(
    vocbase(),
    "FOR d IN myView FILTER NGRAM_MATCH(d.name, 'abc', 0) RETURN d");
  assertFilterFail(
    vocbase(),
    "FOR d IN myView FILTER NGRAM_MATCH(d['name'], 'abc', 0) RETURN d");

  // invalid analyzer arg type
  assertFilterFail(
    vocbase(),
    "FOR d IN myView FILTER NGRAM_MATCH(d.name, 'abc', 0.5, true) RETURN d");
  assertFilterFail(
    vocbase(),
    "FOR d IN myView FILTER NGRAM_MATCH(d['name'], 'abc', 0.5, true) RETURN d");
  assertFilterFail(
    vocbase(),
    "FOR d IN myView FILTER NGRAM_MATCH(d.name, 'abc', 0.5, null) RETURN d");
  assertFilterFail(
    vocbase(),
    "FOR d IN myView FILTER NGRAM_MATCH(d['name'], 'abc', 0.5, null) RETURN d");
  assertFilterFail(
    vocbase(),
    "FOR d IN myView FILTER NGRAM_MATCH(d.name, 'abc', 0.5, 0.5) RETURN d");
  assertFilterFail(
    vocbase(),
    "FOR d IN myView FILTER NGRAM_MATCH(d['name'], 'abc', 0.5, 0.5) RETURN d");
  assertFilterFail(
    vocbase(),
    "FOR d IN myView FILTER NGRAM_MATCH(d.name, 'abc', 0.5, [1, 2]) RETURN d");
  assertFilterFail(
    vocbase(),
    "FOR d IN myView FILTER NGRAM_MATCH(d['name'], 'abc', 0.5, [1,2]) RETURN d");
  assertFilterFail(
    vocbase(),
    "FOR d IN myView FILTER NGRAM_MATCH(d.name, 'abc', 0.5, {a:1}) RETURN d");
  assertFilterFail(
    vocbase(),
    "FOR d IN myView FILTER NGRAM_MATCH(d['name'], 'abc', 0.5, {a:1}) RETURN d");
  assertFilterFail(
    vocbase(),
    "FOR d IN myView FILTER NGRAM_MATCH(d['name'], 'abc', 'test_analyzer', 'test_analyzer') RETURN d");

  // non-deterministic arg
  assertFilterFail(
    vocbase(),
    "FOR d IN myView FILTER NGRAM_MATCH(RAND() ? d.pui : d.name, 'def') RETURN d");
  assertFilterFail(
    vocbase(),
    "FOR d IN myView FILTER NGRAM_MATCH(RAND() ? d['pui'] : d['name'], 'def') RETURN d");
  assertFilterFail(
    vocbase(),
    "FOR d IN myView FILTER NGRAM_MATCH(d.name, RAND() ? 'abc' : 'def') RETURN d");
  assertFilterFail(
    vocbase(),
    "FOR d IN myView FILTER NGRAM_MATCH(d['name'], RAND() ? 'abc' : 'def') RETURN d");
  assertFilterFail(
    vocbase(),
    "FOR d IN myView FILTER NGRAM_MATCH(d.name, 'abc',  RAND() ? 0.5 : 0.6) RETURN d");
  assertFilterFail(
    vocbase(),
    "FOR d IN myView FILTER NGRAM_MATCH(d['name'], 'abc',  RAND() ? 0.5 : 0.6) RETURN d");
  assertFilterFail(
    vocbase(),
    "FOR d IN myView FILTER NGRAM_MATCH(d.name, 'abc', 0.5,  RAND() ? 'identity' : 'test_analyzer') RETURN d");
  assertFilterFail(
    vocbase(),
    "FOR d IN myView FILTER NGRAM_MATCH(d['name'], 'abc', 0.5, RAND() ? 'identity' : 'test_analyzer') RETURN d");
}
