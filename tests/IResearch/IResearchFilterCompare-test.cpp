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

#include <velocypack/Parser.h>

#include "IResearch/ExpressionContextMock.h"
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

class IResearchFilterCompareTest
    : public ::testing::Test,
      public arangodb::tests::LogSuppressor<arangodb::Logger::AUTHENTICATION, arangodb::LogLevel::ERR> {
 protected:
  arangodb::tests::mocks::MockAqlServer server;

 private:
  TRI_vocbase_t* _vocbase;

 protected:
  IResearchFilterCompareTest() {
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
    unused = nullptr;
    analyzers.emplace(
        result, "testVocbase::test_analyzer", "TestAnalyzer",
        arangodb::velocypack::Parser::fromJson("{ \"args\": \"abc\"}")->slice());  // cache analyzer
  }

  TRI_vocbase_t& vocbase() { return *_vocbase; }
};  // IResearchFilterSetup

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

TEST_F(IResearchFilterCompareTest, BinaryEq) {
  // simple attribute, string
  {
    irs::Or expected;
    {
      auto& filter = expected.add<irs::by_term>();
      *filter.mutable_field() = mangleStringIdentity("a");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("1"));
    }

    assertFilterSuccess(vocbase(),
                        "FOR d IN collection FILTER d.a == '1' RETURN d", expected);
    assertFilterSuccess(vocbase(),
                        "FOR d IN collection FILTER d['a'] == '1' RETURN d", expected);
    assertFilterSuccess(vocbase(),
                        "FOR d IN collection FILTER '1' == d.a RETURN d", expected);
    assertFilterSuccess(vocbase(),
                        "FOR d IN collection FILTER '1' == d['a'] RETURN d", expected);
  }

  // simple offset, string
  {
    irs::Or expected;
    {
      auto& filter = expected.add<irs::by_term>();
      *filter.mutable_field() = mangleStringIdentity("[1]");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("1"));
    }

    assertFilterSuccess(vocbase(),
                        "FOR d IN collection FILTER d[1] == '1' RETURN d", expected);
    assertFilterSuccess(vocbase(),
                        "FOR d IN collection FILTER '1' == d[1] RETURN d", expected);
  }

  // complex attribute, string
  {
    irs::Or expected;
    {
      auto& filter = expected.add<irs::by_term>();
      *filter.mutable_field() = mangleStringIdentity("a.b.c");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("1"));
    }

    assertFilterSuccess(vocbase(),
                        "FOR d IN collection FILTER d.a.b.c == '1' RETURN d", expected);
    assertFilterSuccess(vocbase(),
                        "FOR d IN collection FILTER d.a['b'].c == '1' RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER d['a']['b'].c == '1' RETURN d", expected);
    assertFilterSuccess(vocbase(),
                        "FOR d IN collection FILTER '1' == d.a.b.c RETURN d", expected);
    assertFilterSuccess(vocbase(),
                        "FOR d IN collection FILTER '1' == d.a['b'].c RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER '1' == d['a']['b']['c'] RETURN d", expected);
  }

  // complex attribute with offset, string
  {
    irs::Or expected;
    {
      auto& filter = expected.add<irs::by_term>();
      *filter.mutable_field() = mangleStringIdentity("a.b[23].c");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("1"));
    }

    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER d.a.b[23].c == '1' RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER d.a['b'][23].c == '1' RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d['a']['b'][23].c == '1' RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER '1' == d.a.b[23].c RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER '1' == d.a['b'][23].c RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER '1' == d['a']['b'][23]['c'] RETURN d", expected);
  }

  // complex attribute with offset, string, boost
  {
    irs::Or expected;
    {
      auto& filter = expected.add<irs::by_term>();
      filter.boost(0.5);
      *filter.mutable_field() = mangleStringIdentity("a.b[23].c");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("1"));
    }

    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER boost(d.a.b[23].c == '1', 0.5) RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER boost(d.a['b'][23].c == '1', 0.5)  RETURN "
        "d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER boost(d['a']['b'][23].c == '1', 0.5)  "
        "RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER boost('1' == d.a.b[23].c, 0.5)  RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER boost('1' == d.a['b'][23].c, 0.5)  RETURN "
        "d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER boost('1' == d['a']['b'][23]['c'], 0.5)  "
        "RETURN d",
        expected);
  }

  // complex attribute with offset, string, analyzer
  {
    irs::Or expected;
    {
      auto& filter = expected.add<irs::by_term>();
      *filter.mutable_field() = mangleString("a.b[23].c", "test_analyzer");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("1"));
    }

    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER analyzer(d.a.b[23].c == '1', "
        "'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER analyzer(d.a['b'][23].c == '1', "
        "'test_analyzer')  RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER analyzer(d['a']['b'][23].c == '1', "
        "'test_analyzer')  RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER analyzer('1' == d.a.b[23].c, "
        "'test_analyzer')  RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER analyzer('1' == d.a['b'][23].c, "
        "'test_analyzer')  RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER analyzer('1' == d['a']['b'][23]['c'], "
        "'test_analyzer')  RETURN d",
        expected);
  }

  // complex attribute with offset, string, analyzer, boost
  {
    irs::Or expected;
    {
      auto& filter = expected.add<irs::by_term>();
      filter.boost(0.5);
      *filter.mutable_field() = mangleString("a.b[23].c", "test_analyzer");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("1"));
    }

    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER analyzer(boost(d.a.b[23].c == '1', 0.5), "
        "'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER boost(analyzer(d.a['b'][23].c == '1', "
        "'test_analyzer'), 0.5)  RETURN d",
        expected);
  }

  // string expression
  {
    arangodb::aql::Variable var("c", 0, /*isDataFromColl*/ false);
    arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt{41});
    arangodb::aql::AqlValueGuard guard(value, true);

    ExpressionContextMock ctx;
    ctx.vars.emplace(var.name, value);

    irs::Or expected;
    {
      auto& filter = expected.add<irs::by_term>();
      *filter.mutable_field() = mangleStringIdentity("a.b[23].c");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("42"));
    }

    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER d.a.b[23].c == TO_STRING(c+1) "
        "RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER d.a['b'][23].c == TO_STRING(c+1) "
        "RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER d['a']['b'][23].c == "
        "TO_STRING(c+1) RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER TO_STRING(c+1) == d.a.b[23].c "
        "RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER TO_STRING(c+1) == d.a['b'][23].c "
        "RETURN d",
        expected, &ctx);
    assertFilterSuccess(vocbase(),
                        "LET c=41 FOR d IN collection FILTER TO_STRING(c+1) == "
                        "d['a']['b'][23]['c'] RETURN d",
                        expected, &ctx);
  }

  // dynamic complex attribute name with deterministic expression
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                                      arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                                      arangodb::aql::AqlValueHintDouble{5.6})));

    irs::Or expected;
    {
      auto& filter = expected.add<irs::by_term>();
      *filter.mutable_field() = mangleStringIdentity("a.b.c.e[4].f[5].g[3].g.a");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("1"));
    }

    assertFilterSuccess(
        vocbase(),
        "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "== '1' RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER '1' == "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "RETURN d",
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
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "== '1' RETURN d",
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
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "== '1' RETURN d",
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
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "== '1' RETURN d",
        &ctx);
  }

  // complex attribute, true
  {
    irs::Or expected;
    {
      auto& filter = expected.add<irs::by_term>();
      *filter.mutable_field() = mangleBool("a.b.c");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::boolean_token_stream::value_true());
    }

    assertFilterSuccess(vocbase(),
                        "FOR d IN collection FILTER d.a.b.c == true RETURN d", expected);
    assertFilterSuccess(vocbase(),
                        "FOR d IN collection FILTER true == d.a.b.c RETURN d", expected);
  }

  // complex attribute with offset, true
  {
    irs::Or expected;
    {
      auto& filter = expected.add<irs::by_term>();
      *filter.mutable_field() = mangleBool("a[1].b.c");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::boolean_token_stream::value_true());
    }

    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER d.a[1].b.c == true RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER true == d.a[1].b.c RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER analyzer(true == d.a[1].b.c, "
        "'test_analyzer') RETURN d",
        expected);
  }

  // complex attribute with offset, true, boost
  {
    irs::Or expected;
    {
      auto& filter = expected.add<irs::by_term>();
      filter.boost(2.5);
      *filter.mutable_field() = mangleBool("a[1].b.c");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::boolean_token_stream::value_true());
    }

    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER boost(d.a[1].b.c == true, 2.5) RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER boost(true == d.a[1].b.c, 2.5) RETURN d", expected);
  }

  // complex attribute, false
  {
    irs::Or expected;
    {
      auto& filter = expected.add<irs::by_term>();
      *filter.mutable_field() = mangleBool("a.b.c.bool");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::boolean_token_stream::value_false());
    }

    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER d.a.b.c.bool == false RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a.b['c.bool'] == false RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER false == d.a.b.c.bool RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER false == d['a'].b['c'].bool RETURN d", expected);
  }

  // expression
  {
    irs::Or expected;
    expected.add<irs::empty>();

    assertFilterSuccess(vocbase(), "FOR d IN myView FILTER 1 == true RETURN d",
                        expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN myView FILTER analyzer(boost(1 == true, 1.5), "
        "'test_analyzer') RETURN d",
        expected, &ExpressionContextMock::EMPTY);
  }

  // boolean expression
  {
    arangodb::aql::Variable var("c", 0, /*isDataFromColl*/ false);
    arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt{41});
    arangodb::aql::AqlValueGuard guard(value, true);

    ExpressionContextMock ctx;
    ctx.vars.emplace(var.name, value);

    irs::Or expected;
    {
      auto& filter = expected.add<irs::by_term>();
      *filter.mutable_field() = mangleBool("a.b[23].c");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::boolean_token_stream::value_false());
    }

    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER d.a.b[23].c == TO_BOOL(c-41) "
        "RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER d.a['b'][23].c == TO_BOOL(c-41) "
        "RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER d['a']['b'][23].c == "
        "TO_BOOL(c-41) RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER TO_BOOL(c-41) == d.a.b[23].c "
        "RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER TO_BOOL(c-41) == d.a['b'][23].c "
        "RETURN d",
        expected, &ctx);
    assertFilterSuccess(vocbase(),
                        "LET c=41 FOR d IN collection FILTER TO_BOOL(c-41) == "
                        "d['a']['b'][23]['c'] RETURN d",
                        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER analyzer(TO_BOOL(c-41) == "
        "d['a']['b'][23]['c'], 'test_analyzer') RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER analyzer(TO_BOOL(c-41) == "
        "d['a']['b'][23]['c'], 'identity') RETURN d",
        expected, &ctx);
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
    {
      auto& filter = expected.add<irs::by_term>();
      *filter.mutable_field() = mangleBool("a.b.c.e[4].f[5].g[3].g.a");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::boolean_token_stream::value_true());
    }

    assertFilterSuccess(
        vocbase(),
        "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "== true RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER true == "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "RETURN d",
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
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "== true RETURN d",
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
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "== true RETURN d",
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
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "== true RETURN d",
        &ctx);
  }

  // complex attribute, null
  {
    irs::Or expected;
    {
      auto& filter = expected.add<irs::by_term>();
      *filter.mutable_field() = mangleNull("a.b.c.bool");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::null_token_stream::value_null());
    }

    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER d.a.b.c.bool == null RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER d.a['b'].c.bool == null RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d['a']['b'].c.bool == null RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER null == d.a.b.c.bool RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER null == d['a.b.c.bool'] RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER null == d.a.b.c['bool'] RETURN d", expected);
  }

  // complex attribute with offset, null
  {
    irs::Or expected;
    {
      auto& filter = expected.add<irs::by_term>();
      *filter.mutable_field() = mangleNull("a[1].b[2].c[3].bool");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::null_token_stream::value_null());
    }

    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a[1].b[2].c[3].bool == null RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a[1]['b'][2].c[3].bool == null RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d['a'][1]['b'][2].c[3].bool == null RETURN "
        "d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER null == d.a[1].b[2].c[3].bool RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER null == d['a[1].b[2].c[3].bool'] RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER null == d.a[1].b[2].c[3]['bool'] RETURN d", expected);
  }

  // null expression
  {
    arangodb::aql::Variable var("c", 0, /*isDataFromColl*/ false);
    arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintNull{});
    arangodb::aql::AqlValueGuard guard(value, true);

    ExpressionContextMock ctx;
    ctx.vars.emplace(var.name, value);

    irs::Or expected;
    {
      auto& filter = expected.add<irs::by_term>();
      *filter.mutable_field() = mangleNull("a.b[23].c");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::null_token_stream::value_null());
    }

    assertFilterSuccess(
        vocbase(),
        "LET c=null FOR d IN collection FILTER d.a.b[23].c == (c && true) "
        "RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=null FOR d IN collection FILTER analyzer(d.a.b[23].c == (c && "
        "true), 'test_analyzer') RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=null FOR d IN collection FILTER d.a['b'][23].c == (c && false) "
        "RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=null FOR d IN collection FILTER d['a']['b'][23].c == (c && "
        "true) RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=null FOR d IN collection FILTER (c && false) == d.a.b[23].c "
        "RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=null FOR d IN collection FILTER (c && false) == d.a['b'][23].c "
        "RETURN d",
        expected, &ctx);
    assertFilterSuccess(vocbase(),
                        "LET c=null FOR d IN collection FILTER (c && false) == "
                        "d['a']['b'][23]['c'] RETURN d",
                        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=null FOR d IN collection FILTER analyzer((c && false) == "
        "d['a']['b'][23]['c'], 'test_analyzer') RETURN d",
        expected, &ctx);
  }

  // null expression, boost
  {
    arangodb::aql::Variable var("c", 0, /*isDataFromColl*/ false);
    arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintNull{});
    arangodb::aql::AqlValueGuard guard(value, true);

    ExpressionContextMock ctx;
    ctx.vars.emplace(var.name, value);

    irs::Or expected;
    {
      auto& filter = expected.add<irs::by_term>();
      filter.boost(1.5);
      *filter.mutable_field() = mangleNull("a.b[23].c");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::null_token_stream::value_null());
    }

    assertFilterSuccess(
        vocbase(),
        "LET c=null FOR d IN collection FILTER boost(d.a.b[23].c == (c && "
        "true), 1.5) RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=null FOR d IN collection FILTER boost(d.a['b'][23].c == (c && "
        "false), 1.5) RETURN d",
        expected, &ctx);
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
    {
      auto& filter = expected.add<irs::by_term>();
      *filter.mutable_field() = mangleNull("a.b.c.e[4].f[5].g[3].g.a");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::null_token_stream::value_null());
    }

    assertFilterSuccess(
        vocbase(),
        "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "== null RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER null == "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "RETURN d",
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
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "== null RETURN d",
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
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "== null RETURN d",
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
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "== null RETURN d",
        &ctx);
  }

  // complex attribute, numeric
  {
    irs::numeric_token_stream stream;
    stream.reset(3.);
    EXPECT_TRUE(stream.next());
    auto* term = irs::get<irs::term_attribute>(stream);

    irs::Or expected;
    {
      auto& filter = expected.add<irs::by_term>();
      *filter.mutable_field() = mangleNumeric("a.b.c.numeric");
      filter.mutable_options()->term = term->value;
    }

    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER d.a.b.c.numeric == 3 RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER d.a['b'].c.numeric == 3 RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER d.a.b.c.numeric == 3.0 RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a.b.c['numeric'] == 3.0 RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER 3 == d.a.b.c.numeric RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER 3.0 == d.a.b.c.numeric RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER 3.0 == d['a.b.c'].numeric RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER 3.0 == d.a['b.c.numeric'] RETURN d", expected);
  }

  // complex attribute with offset, numeric
  {
    irs::numeric_token_stream stream;
    stream.reset(3.);
    EXPECT_TRUE(stream.next());
    auto* term = irs::get<irs::term_attribute>(stream);

    irs::Or expected;
    {
      auto& filter = expected.add<irs::by_term>();
      *filter.mutable_field() = mangleNumeric("a.b[3].c.numeric");
      filter.mutable_options()->term = term->value;
    }

    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER d.a.b[3].c.numeric == 3 RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a['b'][3].c.numeric == 3 RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a.b[3].c.numeric == 3.0 RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a.b[3].c['numeric'] == 3.0 RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER 3 == d.a.b[3].c.numeric RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER 3.0 == d.a.b[3].c.numeric RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER 3.0 == d['a.b[3].c'].numeric RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER 3.0 == d.a['b[3].c.numeric'] RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER analyzer(3.0 == d.a['b[3].c.numeric'], "
        "'test_analyzer') RETURN d",
        expected);
  }

  // complex attribute with offset, numeric
  {
    irs::numeric_token_stream stream;
    stream.reset(3.);
    EXPECT_TRUE(stream.next());
    auto* term = irs::get<irs::term_attribute>(stream);

    irs::Or expected;
    {
      auto& filter = expected.add<irs::by_term>();
      filter.boost(5);
      *filter.mutable_field() = mangleNumeric("a.b[3].c.numeric");
      filter.mutable_options()->term = term->value;
    }

    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER boost(d.a.b[3].c.numeric == 3, 5) RETURN d", expected);
  }

  // numeric expression
  {
    arangodb::aql::Variable var("c", 0, /*isDataFromColl*/ false);
    arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt{41});
    arangodb::aql::AqlValueGuard guard(value, true);

    ExpressionContextMock ctx;
    ctx.vars.emplace(var.name, value);

    irs::numeric_token_stream stream;
    stream.reset(42.5);
    EXPECT_TRUE(stream.next());
    auto* term = irs::get<irs::term_attribute>(stream);

    irs::Or expected;
    {
      auto& filter = expected.add<irs::by_term>();
      *filter.mutable_field() = mangleNumeric("a.b[23].c");
      filter.mutable_options()->term = term->value;
    }

    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER d.a.b[23].c == (c + 1.5) RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER d.a['b'][23].c == (c + 1.5) "
        "RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER d['a']['b'][23].c == (c + 1.5) "
        "RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER (c + 1.5) == d.a.b[23].c RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER (c + 1.5) == d.a['b'][23].c "
        "RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER (c + 1.5) == d['a']['b'][23]['c'] "
        "RETURN d",
        expected, &ctx);
  }

  // numeric expression, boost
  {
    arangodb::aql::Variable var("c", 0, /*isDataFromColl*/ false);
    arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt{41});
    arangodb::aql::AqlValueGuard guard(value, true);

    ExpressionContextMock ctx;
    ctx.vars.emplace(var.name, value);

    irs::numeric_token_stream stream;
    stream.reset(42.5);
    EXPECT_TRUE(stream.next());
    auto* term = irs::get<irs::term_attribute>(stream);

    irs::Or expected;
    {
      auto& filter = expected.add<irs::by_term>();
      filter.boost(42);
      *filter.mutable_field() = mangleNumeric("a.b[23].c");
      filter.mutable_options()->term = term->value;
    }

    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER boost((c + 1.5) == "
        "d.a['b'][23].c, c+1) RETURN d",
        expected, &ctx);
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

    irs::numeric_token_stream stream;
    stream.reset(42.5);
    EXPECT_TRUE(stream.next());
    auto* term = irs::get<irs::term_attribute>(stream);

    irs::Or expected;
    {
      auto& filter = expected.add<irs::by_term>();
      *filter.mutable_field() = mangleNumeric("a.b.c.e[4].f[5].g[3].g.a");
      filter.mutable_options()->term = term->value;
    }

    assertFilterSuccess(
        vocbase(),
        "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "== 42.5 RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER 42.5 == "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "RETURN d",
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
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "== 42.5 RETURN d",
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
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "== 42.5 RETURN d",
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
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "== 42.5 RETURN d",
        &ctx);
  }

  // complex range expression
  {
    irs::Or expected;
    {
      auto& filter = expected.add<irs::by_term>();
      *filter.mutable_field() = mangleBool("a.b.c");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::boolean_token_stream::value_false());
    }

    assertFilterSuccess(vocbase(),
                        "FOR d IN collection FILTER 3 == 2 == d.a.b.c RETURN d",
                        expected, &ExpressionContextMock::EMPTY);
  }

  // expression without reference to loop variable, unreachable criteria
  {
    auto obj = arangodb::velocypack::Parser::fromJson("{}");

    ExpressionContextMock ctx;
    ctx.vars.emplace("k", arangodb::aql::AqlValue(obj->slice()));

    irs::Or expected;
    expected.add<irs::empty>();

    assertFilterSuccess(
        vocbase(), "LET k={} FOR d IN collection FILTER k.a == '1' RETURN d",
        expected, &ctx);
  }

  // nondeterministic expression -> wrap it
  assertExpressionFilter(
      vocbase(),
      "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
      "collection FILTER "
      "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_NONDETERM_('a')] "
      "== '1' RETURN d");
  assertExpressionFilter(
      vocbase(),
      "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
      "collection FILTER '1' == "
      "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_NONDETERM_('a')] "
      "RETURN d");
  assertExpressionFilter(
      vocbase(), "FOR d IN collection FILTER d.a == _NONDETERM_('1') RETURN d");
  assertExpressionFilter(
      vocbase(),
      "LET k={} FOR d IN collection FILTER k.a == _NONDETERM_('1') RETURN d");

  // unsupported expression (d referenced inside) -> wrap it
  assertExpressionFilter(
      vocbase(), "FOR d IN collection FILTER 3 == (2 == d.a.b.c) RETURN d");
  assertExpressionFilter(
      vocbase(),
      "FOR d IN collection FILTER 3 == boost(2 == d.a.b.c, 1.5) RETURN d");
  assertExpressionFilter(
      vocbase(),
      "FOR d IN collection FILTER analyzer(3, 'test_analyzer') == (2 == "
      "d.a.b.c) RETURN d");

  // expression with self-reference is not supported by IResearch -> wrap it
  assertExpressionFilter(vocbase(),
                         "FOR d IN collection FILTER d == '1' RETURN d");
  assertExpressionFilter(vocbase(),
                         "FOR d IN collection FILTER d[*] == '1' RETURN d");
  assertExpressionFilter(vocbase(),
                         "FOR d IN collection FILTER d.a[*] == '1' RETURN d");
  assertExpressionFilter(vocbase(),
                         "FOR d IN collection FILTER '1' == d RETURN d");
  assertExpressionFilter(vocbase(),
                         "FOR d IN collection FILTER d.a == 1+d.b RETURN d");

  // unsupported node types : fail on parse
  assertFilterFail(vocbase(), "FOR d IN collection FILTER d.a == {} RETURN d");
  assertFilterFail(vocbase(), "FOR d IN collection FILTER {} == d.a RETURN d");

  // unsupported node types : fail on execution
  assertFilterExecutionFail(vocbase(),
                            "FOR d IN collection FILTER d.a == 1..2 RETURN d",
                            &ExpressionContextMock::EMPTY);
  assertFilterExecutionFail(vocbase(),
                            "FOR d IN collection FILTER 1..2 == d.a RETURN d",
                            &ExpressionContextMock::EMPTY);

  // expression is not supported by IResearch -> wrap it
  assertExpressionFilter(
      vocbase(),
      "FOR d IN collection FILTER d.a.b.c.numeric == 2 == 3 RETURN d");
  assertExpressionFilter(
      vocbase(),
      "FOR d IN collection FILTER 2 == d.a.b.c.numeric == 3 RETURN d");
  assertExpressionFilter(
      vocbase(),
      "FOR d IN collection FILTER 2 == boost(d.a.b.c.numeric == 3, 1.5) RETURN "
      "d");
}

TEST_F(IResearchFilterCompareTest, BinaryNotEq) {
  // simple string attribute
  {
    irs::Or expected;
    {
      auto& filter = expected.add<irs::Not>().filter<irs::by_term>();
      *filter.mutable_field() = mangleStringIdentity("a");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("1"));
    }

    assertFilterSuccess(vocbase(),
                        "FOR d IN collection FILTER d.a != '1' RETURN d", expected);
    assertFilterSuccess(vocbase(),
                        "FOR d IN collection FILTER d['a'] != '1' RETURN d", expected);
    assertFilterSuccess(vocbase(),
                        "FOR d IN collection FILTER '1' != d.a RETURN d", expected);
    assertFilterSuccess(vocbase(),
                        "FOR d IN collection FILTER '1' != d['a'] RETURN d", expected);
  }

  // simple offset
  {
    irs::Or expected;
    {
      auto& filter = expected.add<irs::Not>().filter<irs::by_term>();
      *filter.mutable_field() = mangleStringIdentity("[4]");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("1"));
    }

    assertFilterSuccess(vocbase(),
                        "FOR d IN collection FILTER d[4] != '1' RETURN d", expected);
    assertFilterSuccess(vocbase(),
                        "FOR d IN collection FILTER '1' != d[4] RETURN d", expected);
  }

  // complex attribute name, string
  {
    irs::Or expected;
    {
      auto& filter = expected.add<irs::Not>().filter<irs::by_term>();
      *filter.mutable_field() = mangleStringIdentity("a.b.c");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("1"));
    }

    assertFilterSuccess(vocbase(),
                        "FOR d IN collection FILTER d.a.b.c != '1' RETURN d", expected);
    assertFilterSuccess(vocbase(),
                        "FOR d IN collection FILTER d['a'].b.c != '1' RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER d['a']['b'].c != '1' RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER d['a']['b']['c'] != '1' RETURN d", expected);
    assertFilterSuccess(vocbase(),
                        "FOR d IN collection FILTER '1' != d.a.b.c RETURN d", expected);
    assertFilterSuccess(vocbase(),
                        "FOR d IN collection FILTER '1' != d['a'].b.c RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER '1' != d['a']['b'].c RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER '1' != d['a']['b']['c'] RETURN d", expected);
  }

  // complex attribute name with offset, string
  {
    irs::Or expected;
    {
      auto& filter = expected.add<irs::Not>().filter<irs::by_term>();
      *filter.mutable_field() = mangleStringIdentity("a.b[23].c");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("1"));
    }

    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER d.a.b[23].c != '1' RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER d['a'].b[23].c != '1' RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d['a']['b'][23].c != '1' RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d['a']['b'][23]['c'] != '1' RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER '1' != d.a.b[23].c RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER '1' != d['a'].b[23].c RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER '1' != d['a']['b'][23].c RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER '1' != d['a']['b'][23]['c'] RETURN d", expected);
  }

  // string expression
  {
    arangodb::aql::Variable var("c", 0, /*isDataFromColl*/ false);
    arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt{41});
    arangodb::aql::AqlValueGuard guard(value, true);

    ExpressionContextMock ctx;
    ctx.vars.emplace(var.name, value);

    irs::Or expected;
    {
      auto& filter = expected.add<irs::Not>().filter<irs::by_term>();
      *filter.mutable_field() = mangleStringIdentity("a.b[23].c");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("42"));
    }

    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER d.a.b[23].c != TO_STRING(c+1) "
        "RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER d.a['b'][23].c != TO_STRING(c+1) "
        "RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER d['a']['b'][23].c != "
        "TO_STRING(c+1) RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER TO_STRING(c+1) != d.a.b[23].c "
        "RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER TO_STRING(c+1) != d.a['b'][23].c "
        "RETURN d",
        expected, &ctx);
    assertFilterSuccess(vocbase(),
                        "LET c=41 FOR d IN collection FILTER TO_STRING(c+1) != "
                        "d['a']['b'][23]['c'] RETURN d",
                        expected, &ctx);
  }

  // string expression, boost, analyzer
  {
    arangodb::aql::Variable var("c", 0, /*isDataFromColl*/ false);
    arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt{41});
    arangodb::aql::AqlValueGuard guard(value, true);

    ExpressionContextMock ctx;
    ctx.vars.emplace(var.name, value);

    irs::Or expected;
    {
      auto& filter = expected.add<irs::Not>().filter<irs::by_term>();
      filter.boost(42);
      *filter.mutable_field() = mangleStringIdentity("a.b[23].c");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("42"));
    }


    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER "
        "analyzer(boost(analyzer(d.a.b[23].c != TO_STRING(c+1), 'identity'), "
        "c+1), 'test_analyzer') RETURN d",
        expected, &ctx);
  }

  // string expression, boost, analyzer
  {
    arangodb::aql::Variable var("c", 0, /*isDataFromColl*/ false);
    arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt{41});
    arangodb::aql::AqlValueGuard guard(value, true);

    ExpressionContextMock ctx;
    ctx.vars.emplace(var.name, value);

    irs::Or expected;
    {
      auto& filter = expected.add<irs::Not>().filter<irs::by_term>();
      filter.boost(42);
      *filter.mutable_field() = mangleString("a.b[23].c", "test_analyzer");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("42"));
    }

    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER analyzer(boost(d.a.b[23].c != "
        "TO_STRING(c+1), c+1), 'test_analyzer') RETURN d",
        expected, &ctx);
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
    {
      auto& filter = expected.add<irs::Not>().filter<irs::by_term>();
      *filter.mutable_field() = mangleStringIdentity("a.b.c.e[4].f[5].g[3].g.a");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("1"));
    }

    assertFilterSuccess(
        vocbase(),
        "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "!= '1' RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER '1' != "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "RETURN d",
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
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "!= '1' RETURN d",
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
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "!= '1' RETURN d",
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
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "!= '1' RETURN d",
        &ctx);
  }

  // complex boolean attribute, true
  {
    irs::Or expected;
    {
      auto& filter = expected.add<irs::Not>().filter<irs::by_term>();
      *filter.mutable_field() = mangleBool("a.b.c");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::boolean_token_stream::value_true());
    }

    assertFilterSuccess(vocbase(),
                        "FOR d IN collection FILTER d.a.b.c != true RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER d['a'].b.c != true RETURN d", expected);
    assertFilterSuccess(vocbase(),
                        "FOR d IN collection FILTER true != d.a.b.c RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER true != d['a']['b']['c'] RETURN d", expected);
  }

  // complex boolean attribute, false
  {
    irs::Or expected;
    {
      auto& filter = expected.add<irs::Not>().filter<irs::by_term>();
      *filter.mutable_field() = mangleBool("a.b.c.bool");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::boolean_token_stream::value_false());
    }

    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER d.a.b.c.bool != false RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d['a']['b']['c'].bool != false RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER false != d.a.b.c.bool RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER false != d['a']['b'].c.bool RETURN d", expected);
  }

  // complex boolean attribute with offset, false
  {
    irs::Or expected;
    {
      auto& filter = expected.add<irs::Not>().filter<irs::by_term>();
      *filter.mutable_field() = mangleBool("a[12].b.c.bool");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::boolean_token_stream::value_false());
    }

    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a[12].b.c.bool != false RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d['a'][12]['b']['c'].bool != false RETURN "
        "d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER false != d.a[12].b.c.bool RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER false != d['a'][12]['b'].c.bool RETURN d", expected);
  }

  // complex boolean attribute, null
  {
    irs::Or expected;
    {
      auto& filter = expected.add<irs::Not>().filter<irs::by_term>();
      *filter.mutable_field() = mangleNull("a.b.c.bool");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::null_token_stream::value_null());
    }

    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER d.a.b.c.bool != null RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a['b']['c'].bool != null RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER null != d.a.b.c.bool RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER null != d['a']['b'].c.bool RETURN d", expected);
  }

  // complex boolean attribute with offset, null
  {
    irs::Or expected;
    {
      auto& filter = expected.add<irs::Not>().filter<irs::by_term>();
      *filter.mutable_field() = mangleNull("a.b.c[3].bool");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::null_token_stream::value_null());
    }

    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER d.a.b.c[3].bool != null RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a['b']['c'][3].bool != null RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER null != d.a.b.c[3].bool RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER null != d['a']['b'].c[3].bool RETURN d", expected);
  }

  // boolean expression
  {
    arangodb::aql::Variable var("c", 0, /*isDataFromColl*/ false);
    arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt{41});
    arangodb::aql::AqlValueGuard guard(value, true);

    ExpressionContextMock ctx;
    ctx.vars.emplace(var.name, value);

    irs::Or expected;
    {
      auto& filter = expected.add<irs::Not>().filter<irs::by_term>();
      *filter.mutable_field() = mangleBool("a.b[23].c");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::boolean_token_stream::value_false());
    }

    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER d.a.b[23].c != TO_BOOL(c-41) "
        "RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER d.a['b'][23].c != TO_BOOL(c-41) "
        "RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER d['a']['b'][23].c != "
        "TO_BOOL(c-41) RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER analyzer(TO_BOOL(c-41) != "
        "d.a.b[23].c, 'test_analyzer') RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER TO_BOOL(c-41) != d.a['b'][23].c "
        "RETURN d",
        expected, &ctx);
    assertFilterSuccess(vocbase(),
                        "LET c=41 FOR d IN collection FILTER TO_BOOL(c-41) != "
                        "d['a']['b'][23]['c'] RETURN d",
                        expected, &ctx);
  }

  // boolean expression, boost
  {
    arangodb::aql::Variable var("c", 0, /*isDataFromColl*/ false);
    arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt{41});
    arangodb::aql::AqlValueGuard guard(value, true);

    ExpressionContextMock ctx;
    ctx.vars.emplace(var.name, value);

    irs::Or expected;
    {
      auto& filter = expected.add<irs::Not>().filter<irs::by_term>();
      filter.boost(42);
      *filter.mutable_field() = mangleBool("a.b[23].c");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::boolean_token_stream::value_false());
    }

    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER boost(d.a.b[23].c != "
        "TO_BOOL(c-41), c+1) RETURN d",
        expected, &ctx);
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
    {
      auto& filter = expected.add<irs::Not>().filter<irs::by_term>();
      *filter.mutable_field() = mangleBool("a.b.c.e[4].f[5].g[3].g.a");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::boolean_token_stream::value_true());
    }

    assertFilterSuccess(
        vocbase(),
        "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "!= true RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER true != "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "RETURN d",
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
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "!= true RETURN d",
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
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "!= true RETURN d",
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
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "!= true RETURN d",
        &ctx);
  }

  // null expression
  {
    arangodb::aql::Variable var("c", 0, /*isDataFromColl*/ false);
    arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintNull{});
    arangodb::aql::AqlValueGuard guard(value, true);

    ExpressionContextMock ctx;
    ctx.vars.emplace(var.name, value);

    irs::Or expected;
    {
      auto& filter = expected.add<irs::Not>().filter<irs::by_term>();
      *filter.mutable_field() = mangleNull("a.b[23].c");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::null_token_stream::value_null());
    }

    assertFilterSuccess(
        vocbase(),
        "LET c=null FOR d IN collection FILTER d.a.b[23].c != (c && true) "
        "RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=null FOR d IN collection FILTER d.a['b'][23].c != (c && false) "
        "RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=null FOR d IN collection FILTER d['a']['b'][23].c != (c && "
        "true) RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=null FOR d IN collection FILTER (c && false) != d.a.b[23].c "
        "RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=null FOR d IN collection FILTER analyzer((c && false) != "
        "d.a.b[23].c, 'test_analyzer') RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=null FOR d IN collection FILTER (c && false) != d.a['b'][23].c "
        "RETURN d",
        expected, &ctx);
    assertFilterSuccess(vocbase(),
                        "LET c=null FOR d IN collection FILTER (c && false) != "
                        "d['a']['b'][23]['c'] RETURN d",
                        expected, &ctx);
  }

  // null expression, boost
  {
    arangodb::aql::Variable var("c", 0, /*isDataFromColl*/ false);
    arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintNull{});
    arangodb::aql::AqlValueGuard guard(value, true);

    ExpressionContextMock ctx;
    ctx.vars.emplace(var.name, value);

    irs::Or expected;
    {
      auto& filter = expected.add<irs::Not>().filter<irs::by_term>();
      filter.boost(1.5);
      *filter.mutable_field() = mangleNull("a.b[23].c");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::null_token_stream::value_null());
    }

    assertFilterSuccess(
        vocbase(),
        "LET c=null FOR d IN collection FILTER boost(d.a.b[23].c != (c && "
        "true), 1.5) RETURN d",
        expected, &ctx);
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
    {
      auto& filter = expected.add<irs::Not>().filter<irs::by_term>();
      *filter.mutable_field() = mangleNull("a.b.c.e[4].f[5].g[3].g.a");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::null_token_stream::value_null());
    }

    assertFilterSuccess(
        vocbase(),
        "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "!= null RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER null != "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "RETURN d",
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
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "!= null RETURN d",
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
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "!= null RETURN d",
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
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "!= null RETURN d",
        &ctx);
  }

  // complex boolean attribute, numeric
  {
    irs::numeric_token_stream stream;
    stream.reset(3.);
    EXPECT_TRUE(stream.next());
    auto* term = irs::get<irs::term_attribute>(stream);

    irs::Or expected;
    {
      auto& filter = expected.add<irs::Not>().filter<irs::by_term>();
      *filter.mutable_field() = mangleNumeric("a.b.c.numeric");
      filter.mutable_options()->term = term->value;
    }

    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER d.a.b.c.numeric != 3 RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d['a']['b'].c.numeric != 3 RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER d.a.b.c.numeric != 3.0 RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER 3 != d.a.b.c.numeric RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER 3.0 != d.a.b.c.numeric RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER 3.0 != d.a['b']['c'].numeric RETURN d", expected);
  }

  // complex boolean attribute with offset, numeric
  {
    irs::numeric_token_stream stream;
    stream.reset(3.);
    EXPECT_TRUE(stream.next());
    auto* term = irs::get<irs::term_attribute>(stream);

    irs::Or expected;
    {
      auto& filter = expected.add<irs::Not>().filter<irs::by_term>();
      *filter.mutable_field() = mangleNumeric("a.b.c.numeric[1]");
      filter.mutable_options()->term = term->value;
    }

    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER d.a.b.c.numeric[1] != 3 RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d['a']['b'].c.numeric[1] != 3 RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a.b.c.numeric[1] != 3.0 RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER 3 != d.a.b.c.numeric[1] RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER 3.0 != d.a.b.c.numeric[1] RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER 3.0 != d.a['b']['c'].numeric[1] RETURN d", expected);
  }

  // numeric expression
  {
    arangodb::aql::Variable var("c", 0, /*isDataFromColl*/ false);
    arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt{41});
    arangodb::aql::AqlValueGuard guard(value, true);

    ExpressionContextMock ctx;
    ctx.vars.emplace(var.name, value);

    irs::numeric_token_stream stream;
    stream.reset(42.5);
    EXPECT_TRUE(stream.next());
    auto* term = irs::get<irs::term_attribute>(stream);

    irs::Or expected;
    {
      auto& filter = expected.add<irs::Not>().filter<irs::by_term>();
      *filter.mutable_field() = mangleNumeric("a.b[23].c");
      filter.mutable_options()->term = term->value;
    }

    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER d.a.b[23].c != (c + 1.5) RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER d.a['b'][23].c != (c + 1.5) "
        "RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER d['a']['b'][23].c != (c + 1.5) "
        "RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER (c + 1.5) != d.a.b[23].c RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER (c + 1.5) != d.a['b'][23].c "
        "RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER (c + 1.5) != d['a']['b'][23]['c'] "
        "RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER analyzer((c + 1.5) != "
        "d['a']['b'][23]['c'], 'test_analyzer') RETURN d",
        expected, &ctx);
  }

  // numeric expression, boost
  {
    arangodb::aql::Variable var("c", 0, /*isDataFromColl*/ false);
    arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt{41});
    arangodb::aql::AqlValueGuard guard(value, true);

    ExpressionContextMock ctx;
    ctx.vars.emplace(var.name, value);

    irs::numeric_token_stream stream;
    stream.reset(42.5);
    EXPECT_TRUE(stream.next());
    auto* term = irs::get<irs::term_attribute>(stream);

    irs::Or expected;
    {
      auto& filter = expected.add<irs::Not>().filter<irs::by_term>();
      filter.boost(42);
      *filter.mutable_field() = mangleNumeric("a.b[23].c");
      filter.mutable_options()->term = term->value;
    }

    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER boost(d.a.b[23].c != (c + 1.5), "
        "c+1) RETURN d",
        expected, &ctx);
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

    irs::numeric_token_stream stream;
    stream.reset(42.5);
    EXPECT_TRUE(stream.next());
    auto* term = irs::get<irs::term_attribute>(stream);

    irs::Or expected;
    {
      auto& filter = expected.add<irs::Not>().filter<irs::by_term>();
      *filter.mutable_field() = mangleNumeric("a.b.c.e[4].f[5].g[3].g.a");
      filter.mutable_options()->term = term->value;
    }

    assertFilterSuccess(
        vocbase(),
        "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "!= 42.5 RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER 42.5 != "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "RETURN d",
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
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "!= 42.5 RETURN d",
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
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "!= 42.5 RETURN d",
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
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "!= 42.5 RETURN d",
        &ctx);
  }

  // complex range expression
  {
    irs::Or expected;
    {
      auto& filter = expected.add<irs::Not>().filter<irs::by_term>();
      *filter.mutable_field() = mangleBool("a.b.c");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::boolean_token_stream::value_true());
    }

    assertFilterSuccess(vocbase(),
                        "FOR d IN collection FILTER 3 != 2 != d.a.b.c RETURN d",
                        expected, &ExpressionContextMock::EMPTY);
  }

  // expression without reference to loop variable, reachable criteria
  {
    auto obj = arangodb::velocypack::Parser::fromJson("{}");

    ExpressionContextMock ctx;
    ctx.vars.emplace("k", arangodb::aql::AqlValue(obj->slice()));

    irs::Or expected;
    expected.add<irs::all>();

    assertFilterSuccess(
        vocbase(), "LET k={} FOR d IN collection FILTER k.a != '1' RETURN d",
        expected, &ctx);
  }

  // expression without reference to loop variable, reachable criteria, boost
  {
    auto obj = arangodb::velocypack::Parser::fromJson("{}");

    ExpressionContextMock ctx;
    ctx.vars.emplace("k", arangodb::aql::AqlValue(obj->slice()));

    irs::Or expected;
    expected.add<irs::all>().boost(1.5);

    assertFilterSuccess(
        vocbase(),
        "LET k={} FOR d IN collection FILTER boost(k.a != '1', 1.5) RETURN d",
        expected, &ctx);
  }

  // array in expression
  {
    irs::Or expected;
    expected.add<irs::all>();

    assertFilterSuccess(vocbase(),
                        "FOR d IN collection FILTER ['d'] != '1' RETURN d",
                        expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess(vocbase(),
                        "FOR d IN collection FILTER [] != '1' RETURN d",
                        expected, &ExpressionContextMock::EMPTY);
  }

  // nondeterministic expression -> wrap it
  assertExpressionFilter(
      vocbase(),
      "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
      "collection FILTER "
      "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_NONDETERM_('a')] "
      "!= '1' RETURN d");
  assertExpressionFilter(
      vocbase(),
      "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
      "collection FILTER "
      "boost(d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_NONDETERM_("
      "'a')] != '1', 1.5) RETURN d",
      1.5, wrappedExpressionExtractor);
  assertExpressionFilter(
      vocbase(),
      "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
      "collection FILTER '1' != "
      "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_NONDETERM_('a')] "
      "RETURN d");
  assertExpressionFilter(
      vocbase(), "FOR d IN collection FILTER d.a != _NONDETERM_('1') RETURN d");
  assertExpressionFilter(
      vocbase(),
      "LET k={} FOR d IN collection FILTER k.a != _NONDETERM_('1') RETURN d");

  // unsupported expression (d referenced inside) -> wrap it
  assertExpressionFilter(
      vocbase(), "FOR d IN collection FILTER 3 != (2 != d.a.b.c) RETURN d");
  assertExpressionFilter(
      vocbase(),
      "FOR d IN collection FILTER 3 != boost(2 != d.a.b.c, 1.5) RETURN d");
  assertExpressionFilter(
      vocbase(),
      "FOR d IN collection FILTER analyzer(3, 'test_analyzer') != (2 != "
      "d.a.b.c) RETURN d");

  // expression is not supported by IResearch -> wrap it
  assertExpressionFilter(vocbase(),
                         "FOR d IN collection FILTER d != '1' RETURN d");
  assertExpressionFilter(vocbase(),
                         "FOR d IN collection FILTER d[*] != '1' RETURN d");
  assertExpressionFilter(vocbase(),
                         "FOR d IN collection FILTER d.a[*] != '1' RETURN d");
  assertExpressionFilter(vocbase(),
                         "FOR d IN collection FILTER '1' != d RETURN d");
  assertExpressionFilter(
      vocbase(),
      "FOR d IN collection FILTER 2 != d.a.b.c.numeric != 3 RETURN d");
  assertExpressionFilter(
      vocbase(),
      "FOR d IN collection FILTER 2 == d.a.b.c.numeric != 3 RETURN d");
  assertExpressionFilter(
      vocbase(),
      "FOR d IN collection FILTER d.a.b.c.numeric != 2 != 3 RETURN d");
  assertExpressionFilter(
      vocbase(),
      "FOR d IN collection FILTER d.a.b.c.numeric != 2 == 3 RETURN d");

  // expression with self-reference is not supported by IResearch -> wrap it
  assertExpressionFilter(vocbase(),
                         "FOR d IN collection FILTER d.a == 1+d.b RETURN d");

  // unsupported node types : fail on parse
  assertFilterFail(vocbase(), "FOR d IN collection FILTER d.a != {} RETURN d");
  assertFilterFail(vocbase(), "FOR d IN collection FILTER {} != d.a RETURN d");
  // unsupported node types : fail on execution
  assertFilterExecutionFail(vocbase(),
                            "FOR d IN collection FILTER d.a != 1..2 RETURN d",
                            &ExpressionContextMock::EMPTY);
  assertFilterExecutionFail(vocbase(),
                            "FOR d IN collection FILTER 1..2 != d.a RETURN d",
                            &ExpressionContextMock::EMPTY);
}

TEST_F(IResearchFilterCompareTest, BinaryGE) {
  // simple string attribute
  {
    irs::Or expected;
    {
      auto& filter = expected.add<irs::by_range>();
      *filter.mutable_field() = mangleStringIdentity("a");
      auto* opts = filter.mutable_options();
      opts->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("1"));
      opts->range.min_type = irs::BoundType::INCLUSIVE;
    }

    assertFilterSuccess(vocbase(),
                        "FOR d IN collection FILTER d.a >= '1' RETURN d", expected);
    assertFilterSuccess(vocbase(),
                        "FOR d IN collection FILTER d['a'] >= '1' RETURN d", expected);
    assertFilterSuccess(vocbase(),
                        "FOR d IN collection FILTER '1' <= d.a RETURN d", expected);
    assertFilterSuccess(vocbase(),
                        "FOR d IN collection FILTER '1' <= d['a'] RETURN d", expected);
  }

  // simple string offset
  {
    irs::Or expected;
    {
      auto& filter = expected.add<irs::by_range>();
      *filter.mutable_field() = mangleStringIdentity("[23]");
      auto* opts = filter.mutable_options();
      opts->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("1"));
      opts->range.min_type = irs::BoundType::INCLUSIVE;
    }

    assertFilterSuccess(vocbase(),
                        "FOR d IN collection FILTER d[23] >= '1' RETURN d", expected);
    assertFilterSuccess(vocbase(),
                        "FOR d IN collection FILTER '1' <= d[23] RETURN d", expected);
  }

  // complex attribute name, string
  {
    irs::Or expected;
    {
      auto& filter = expected.add<irs::by_range>();
      *filter.mutable_field() = mangleStringIdentity("a.b.c");
      auto* opts = filter.mutable_options();
      opts->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("1"));
      opts->range.min_type = irs::BoundType::INCLUSIVE;
    }

    assertFilterSuccess(vocbase(),
                        "FOR d IN collection FILTER d.a.b.c >= '1' RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER d.a['b']['c'] >= '1' RETURN d", expected);
    assertFilterSuccess(vocbase(),
                        "FOR d IN collection FILTER '1' <= d.a.b.c RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER '1' <= d['a']['b'].c RETURN d", expected);
  }

  // complex attribute name with offset, string
  {
    irs::Or expected;
    {
      auto& filter = expected.add<irs::by_range>();
      *filter.mutable_field() = mangleStringIdentity("a.b[23].c");
      auto* opts = filter.mutable_options();
      opts->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("1"));
      opts->range.min_type = irs::BoundType::INCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER d.a.b[23].c >= '1' RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a['b'][23]['c'] >= '1' RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER '1' <= d.a.b[23].c RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER '1' <= d['a']['b'][23].c RETURN d", expected);
  }

  // string expression
  {
    arangodb::aql::Variable var("c", 0, /*isDataFromColl*/ false);
    arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt{41});
    arangodb::aql::AqlValueGuard guard(value, true);

    ExpressionContextMock ctx;
    ctx.vars.emplace(var.name, value);

    irs::Or expected;
    {
      auto& filter = expected.add<irs::by_range>();
      *filter.mutable_field() = mangleStringIdentity("a.b[23].c");
      auto* opts = filter.mutable_options();
      opts->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("42"));
      opts->range.min_type = irs::BoundType::INCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER d.a.b[23].c >= TO_STRING(c+1) "
        "RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER d.a['b'][23]['c'] >= "
        "TO_STRING(c+1) RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER TO_STRING(c+1) <= d.a.b[23].c "
        "RETURN d",
        expected, &ctx);
    assertFilterSuccess(vocbase(),
                        "LET c=41 FOR d IN collection FILTER TO_STRING(c+1) <= "
                        "d['a']['b'][23].c RETURN d",
                        expected, &ctx);
  }

  // string expression, boost, analyzer
  {
    arangodb::aql::Variable var("c", 0, /*isDataFromColl*/ false);
    arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt{41});
    arangodb::aql::AqlValueGuard guard(value, true);

    ExpressionContextMock ctx;
    ctx.vars.emplace(var.name, value);

    irs::Or expected;
    {
      auto& filter = expected.add<irs::by_range>();
      filter.boost(42);
      *filter.mutable_field() = mangleString("a.b[23].c", "test_analyzer");
      auto* opts = filter.mutable_options();
      opts->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("42"));
      opts->range.min_type = irs::BoundType::INCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER boost(analyzer(d.a.b[23].c >= "
        "TO_STRING(c+1), 'test_analyzer'), c+1) RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER analyzer(boost(d.a.b[23].c >= "
        "TO_STRING(c+1), c + 1), 'test_analyzer') RETURN d",
        expected, &ctx);
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
    {
      auto& filter = expected.add<irs::by_range>();
      *filter.mutable_field() = mangleStringIdentity("a.b.c.e[4].f[5].g[3].g.a");
      auto* opts = filter.mutable_options();
      opts->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("42"));
      opts->range.min_type = irs::BoundType::INCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(),
        "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        ">= '42' RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER '42' <= "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "RETURN d",
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
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        ">= '42' RETURN d",
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
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        ">= '42' RETURN d",
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
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        ">= '42' RETURN d",
        &ctx);
  }

  // complex boolean attribute, true
  {
    irs::Or expected;
    {
      auto& filter = expected.add<irs::by_range>();
      *filter.mutable_field() = mangleBool("a.b.c");
      auto* opts = filter.mutable_options();
      opts->range.min = irs::ref_cast<irs::byte_type>(irs::boolean_token_stream::value_true());
      opts->range.min_type = irs::BoundType::INCLUSIVE;
    }

    assertFilterSuccess(vocbase(),
                        "FOR d IN collection FILTER d.a.b.c >= true RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d['a']['b']['c'] >= true RETURN d", expected);
    assertFilterSuccess(vocbase(),
                        "FOR d IN collection FILTER true <= d.a.b.c RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER true <= d['a']['b']['c'] RETURN d", expected);
  }

  // complex boolean attribute with offset, true
  {
    irs::Or expected;
    {
      auto& filter = expected.add<irs::by_range>();
      *filter.mutable_field() = mangleBool("a.b.c[223]");
      auto* opts = filter.mutable_options();
      opts->range.min = irs::ref_cast<irs::byte_type>(irs::boolean_token_stream::value_true());
      opts->range.min_type = irs::BoundType::INCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER d.a.b.c[223] >= true RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d['a']['b']['c'][223] >= true RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER true <= d.a.b.c[223] RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER true <= d['a']['b']['c'][223] RETURN d", expected);
  }

  // complex boolean attribute, false
  {
    irs::Or expected;
    {
      auto& filter = expected.add<irs::by_range>();
      *filter.mutable_field() = mangleBool("a.b.c.bool");
      auto* opts = filter.mutable_options();
      opts->range.min = irs::ref_cast<irs::byte_type>(irs::boolean_token_stream::value_false());
      opts->range.min_type = irs::BoundType::INCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER d.a.b.c.bool >= false RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d['a']['b'].c.bool >= false RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER false <= d.a.b.c.bool RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER false <= d.a['b']['c'].bool RETURN d", expected);
  }

  // boolean expression
  {
    arangodb::aql::Variable var("c", 0, /*isDataFromColl*/ false);
    arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt{41});
    arangodb::aql::AqlValueGuard guard(value, true);

    ExpressionContextMock ctx;
    ctx.vars.emplace(var.name, value);

    irs::Or expected;
    {
      auto& filter = expected.add<irs::by_range>();
      *filter.mutable_field() = mangleBool("a.b[23].c");
      auto* opts = filter.mutable_options();
      opts->range.min = irs::ref_cast<irs::byte_type>(irs::boolean_token_stream::value_false());
      opts->range.min_type = irs::BoundType::INCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER d.a.b[23].c >= TO_BOOL(c-41) "
        "RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER d.a['b'][23]['c'] >= "
        "TO_BOOL(c-41) RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER TO_BOOL(c-41) <= d.a.b[23].c "
        "RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER analyzer(TO_BOOL(c-41) <= "
        "d['a']['b'][23].c, 'test_analyzer') RETURN d",
        expected, &ctx);
  }

  // boolean expression, boost
  {
    arangodb::aql::Variable var("c", 0, /*isDataFromColl*/ false);
    arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt{41});
    arangodb::aql::AqlValueGuard guard(value, true);

    ExpressionContextMock ctx;
    ctx.vars.emplace(var.name, value);

    irs::Or expected;
    {
      auto& filter = expected.add<irs::by_range>();
      filter.boost(1.5);
      *filter.mutable_field() = mangleBool("a.b[23].c");
      auto* opts = filter.mutable_options();
      opts->range.min = irs::ref_cast<irs::byte_type>(irs::boolean_token_stream::value_false());
      opts->range.min_type = irs::BoundType::INCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER boost(d.a.b[23].c >= "
        "TO_BOOL(c-41), 1.5) RETURN d",
        expected, &ctx);
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
    {
      auto& filter = expected.add<irs::by_range>();
      *filter.mutable_field() = mangleBool("a.b.c.e[4].f[5].g[3].g.a");
      auto* opts = filter.mutable_options();
      opts->range.min = irs::ref_cast<irs::byte_type>(irs::boolean_token_stream::value_false());
      opts->range.min_type = irs::BoundType::INCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(),
        "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        ">= false RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER false <= "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "RETURN d",
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
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        ">= false RETURN d",
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
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        ">= false RETURN d",
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
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        ">= false RETURN d",
        &ctx);
  }

  // complex boolean attribute, null
  {
    irs::Or expected;
    {
      auto& filter = expected.add<irs::by_range>();
      *filter.mutable_field() = mangleNull("a.b.c.nil");
      auto* opts = filter.mutable_options();
      opts->range.min = irs::ref_cast<irs::byte_type>(irs::null_token_stream::value_null());
      opts->range.min_type = irs::BoundType::INCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER d.a.b.c.nil >= null RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d['a']['b']['c'].nil >= null RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER null <= d.a.b.c.nil RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER null <= d['a']['b'].c.nil RETURN d", expected);
  }

  // complex null attribute with offset
  {
    irs::Or expected;
    {
      auto& filter = expected.add<irs::by_range>();
      *filter.mutable_field() = mangleNull("a.b[23].c.nil");
      auto* opts = filter.mutable_options();
      opts->range.min = irs::ref_cast<irs::byte_type>(irs::null_token_stream::value_null());
      opts->range.min_type = irs::BoundType::INCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER d.a.b[23].c.nil >= null RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d['a']['b'][23]['c'].nil >= null RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER null <= d.a.b[23].c.nil RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER null <= d['a']['b'][23].c.nil RETURN d", expected);
  }

  // null expression
  {
    arangodb::aql::Variable var("c", 0, /*isDataFromColl*/ false);
    arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintNull{});
    arangodb::aql::AqlValueGuard guard(value, true);

    ExpressionContextMock ctx;
    ctx.vars.emplace(var.name, value);

    irs::Or expected;
    {
      auto& filter = expected.add<irs::by_range>();
      *filter.mutable_field() = mangleNull("a.b[23].c");
      auto* opts = filter.mutable_options();
      opts->range.min = irs::ref_cast<irs::byte_type>(irs::null_token_stream::value_null());
      opts->range.min_type = irs::BoundType::INCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(),
        "LET c=null FOR d IN collection FILTER d.a.b[23].c >= (c && false) "
        "RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=null FOR d IN collection FILTER d.a['b'][23]['c'] >= (c && "
        "true) RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=null FOR d IN collection FILTER (c && false) <= d.a.b[23].c "
        "RETURN d",
        expected, &ctx);
    assertFilterSuccess(vocbase(),
                        "LET c=null FOR d IN collection FILTER (c && false) <= "
                        "d['a']['b'][23].c RETURN d",
                        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=null FOR d IN collection FILTER analyzer((c && false) <= "
        "d['a']['b'][23].c, 'test_analyzer') RETURN d",
        expected, &ctx);
  }

  // null expression, boost
  {
    arangodb::aql::Variable var("c", 0, /*isDataFromColl*/ false);
    arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintNull{});
    arangodb::aql::AqlValueGuard guard(value, true);

    ExpressionContextMock ctx;
    ctx.vars.emplace(var.name, value);

    irs::Or expected;
    {
      auto& filter = expected.add<irs::by_range>();
      filter.boost(1.5);
      *filter.mutable_field() = mangleNull("a.b[23].c");
      auto* opts = filter.mutable_options();
      opts->range.min = irs::ref_cast<irs::byte_type>(irs::null_token_stream::value_null());
      opts->range.min_type = irs::BoundType::INCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(),
        "LET c=null FOR d IN collection FILTER boost(d.a.b[23].c >= (c && "
        "false), 1.5) RETURN d",
        expected, &ctx);
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
    {
      auto& filter = expected.add<irs::by_range>();
      *filter.mutable_field() = mangleNull("a.b.c.e[4].f[5].g[3].g.a");
      auto* opts = filter.mutable_options();
      opts->range.min = irs::ref_cast<irs::byte_type>(irs::null_token_stream::value_null());
      opts->range.min_type = irs::BoundType::INCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(),
        "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        ">= null RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER null <= "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "RETURN d",
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
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        ">= null RETURN d",
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
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        ">= null RETURN d",
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
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        ">= null RETURN d",
        &ctx);
  }

  // complex numeric attribute
  {
    irs::numeric_token_stream stream;
    stream.reset(13.);

    irs::Or expected;
    {
      auto& filter = expected.add<irs::by_granular_range>();
      *filter.mutable_field() = mangleNumeric("a.b.c.numeric");
      auto* opts = filter.mutable_options();
      irs::set_granular_term(opts->range.min, stream);
      opts->range.min_type = irs::BoundType::INCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER d.a.b.c.numeric >= 13 RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d['a']['b'].c.numeric >= 13 RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER d.a.b.c.numeric >= 13.0 RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER 13 <= d.a.b.c.numeric RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER 13.0 <= d.a.b.c.numeric RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER 13.0 <= d['a']['b']['c'].numeric RETURN d", expected);
  }

  // complex numeric attribute, numeric
  {
    irs::numeric_token_stream stream;
    stream.reset(13.);

    irs::Or expected;
    {
      auto& filter = expected.add<irs::by_granular_range>();
      *filter.mutable_field() = mangleNumeric("a.b.c[223].numeric");
      auto* opts = filter.mutable_options();
      irs::set_granular_term(opts->range.min, stream);
      opts->range.min_type = irs::BoundType::INCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a.b.c[223].numeric >= 13 RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d['a']['b'].c[223].numeric >= 13 RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a.b.c[223].numeric >= 13.0 RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER 13 <= d.a.b.c[223].numeric RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER 13.0 <= d.a.b.c[223].numeric RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER 13.0 <= d['a']['b']['c'][223].numeric "
        "RETURN d",
        expected);
  }

  // numeric expression
  {
    arangodb::aql::Variable var("c", 0, /*isDataFromColl*/ false);
    arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt{41});
    arangodb::aql::AqlValueGuard guard(value, true);

    ExpressionContextMock ctx;
    ctx.vars.emplace(var.name, value);

    irs::numeric_token_stream stream;
    stream.reset(42.5);

    irs::Or expected;
    {
      auto& filter = expected.add<irs::by_granular_range>();
      *filter.mutable_field() = mangleNumeric("a.b[23].c");
      auto* opts = filter.mutable_options();
      irs::set_granular_term(opts->range.min, stream);
      opts->range.min_type = irs::BoundType::INCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER d.a.b[23].c >= (c+1.5) RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER d.a['b'][23]['c'] >= (c+1.5) "
        "RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER (c+1.5) <= d.a.b[23].c RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER analyzer((c+1.5) <= "
        "d['a']['b'][23].c, 'test_analyzer') RETURN d",
        expected, &ctx);
  }

  // numeric expression, boost
  {
    arangodb::aql::Variable var("c", 0, /*isDataFromColl*/ false);
    arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt{41});
    arangodb::aql::AqlValueGuard guard(value, true);

    ExpressionContextMock ctx;
    ctx.vars.emplace(var.name, value);

    irs::numeric_token_stream stream;
    stream.reset(42.5);

    irs::Or expected;
    {
      auto& filter = expected.add<irs::by_granular_range>();
      filter.boost(42);
      *filter.mutable_field() = mangleNumeric("a.b[23].c");
      auto* opts = filter.mutable_options();
      irs::set_granular_term(opts->range.min, stream);
      opts->range.min_type = irs::BoundType::INCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER boost(d.a.b[23].c >= (c+1.5), "
        "c+1) RETURN d",
        expected, &ctx);
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

    irs::numeric_token_stream stream;
    stream.reset(42.5);

    irs::Or expected;
    {
      auto& filter = expected.add<irs::by_granular_range>();
      *filter.mutable_field() = mangleNumeric("a.b.c.e[4].f[5].g[3].g.a");
      auto* opts = filter.mutable_options();
      irs::set_granular_term(opts->range.min, stream);
      opts->range.min_type = irs::BoundType::INCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(),
        "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        ">= 42.5 RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER 42.5 <= "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "RETURN d",
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
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        ">= 42.5 RETURN d",
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
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        ">= 42.5 RETURN d",
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
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        ">= 42.5 RETURN d",
        &ctx);
  }

  // complex expression
  {
    irs::Or expected;
    {
      auto& filter = expected.add<irs::by_range>();
      *filter.mutable_field() = mangleBool("a.b.c");
      auto* opts = filter.mutable_options();
      opts->range.max = irs::ref_cast<irs::byte_type>(irs::boolean_token_stream::value_true());
      opts->range.max_type = irs::BoundType::INCLUSIVE;
    }

    assertFilterSuccess(vocbase(),
                        "FOR d IN collection FILTER 3 >= 2 >= d.a.b.c RETURN d",
                        expected, &ExpressionContextMock::EMPTY);
  }

  // expression without reference to loop variable, unreachable criteria
  {
    auto obj = arangodb::velocypack::Parser::fromJson("{}");

    ExpressionContextMock ctx;
    ctx.vars.emplace("k", arangodb::aql::AqlValue(obj->slice()));

    irs::Or expected;
    expected.add<irs::empty>();

    assertFilterSuccess(
        vocbase(), "LET k='' FOR d IN collection FILTER k.a >= '1' RETURN d",
        expected, &ctx);
  }

  // array in expression
  {
    irs::Or expected;
    expected.add<irs::all>();

    assertFilterSuccess(vocbase(),
                        "FOR d IN collection FILTER [] >= '1' RETURN d",
                        expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess(vocbase(),
                        "FOR d IN collection FILTER ['d'] >= '1' RETURN d",
                        expected, &ExpressionContextMock::EMPTY);
  }

  // nondeterministic expression -> wrap it
  assertExpressionFilter(
      vocbase(),
      "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
      "collection FILTER "
      "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_NONDETERM_('a')] "
      ">= '1' RETURN d");
  assertExpressionFilter(
      vocbase(),
      "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
      "collection FILTER '1' >= "
      "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_NONDETERM_('a')] "
      "RETURN d");
  assertExpressionFilter(
      vocbase(), "FOR d IN collection FILTER d.a >= _NONDETERM_('1') RETURN d");
  assertExpressionFilter(
      vocbase(),
      "LET k={} FOR d IN collection FILTER k.a >= _NONDETERM_('1') RETURN d");

  // unsupported expression (d referenced inside) -> wrap it
  assertExpressionFilter(
      vocbase(), "FOR d IN collection FILTER 3 >= (2 >= d.a.b.c) RETURN d");
  assertExpressionFilter(
      vocbase(),
      "FOR d IN collection FILTER 3 >= boost(2 >= d.a.b.c, 1.5) RETURN d");
  assertExpressionFilter(
      vocbase(),
      "FOR d IN collection FILTER analyzer(3, 'test_analyzer') >= boost(2 >= "
      "d.a.b.c, 1.5) RETURN d");

  // expression is not supported by IResearch -> wrap it
  assertExpressionFilter(vocbase(),
                         "FOR d IN collection FILTER d >= '1' RETURN d");
  assertExpressionFilter(vocbase(),
                         "FOR d IN collection FILTER d[*] >= '1' RETURN d");
  assertExpressionFilter(vocbase(),
                         "FOR d IN collection FILTER d.a[*] >= '1' RETURN d");
  assertExpressionFilter(vocbase(),
                         "FOR d IN collection FILTER '1' <= d RETURN d");
  assertExpressionFilter(
      vocbase(),
      "FOR d IN collection FILTER 2 >= d.a.b.c.numeric >= 3 RETURN d");
  assertExpressionFilter(
      vocbase(),
      "FOR d IN collection FILTER d.a.b.c.numeric >= 2 >= 3 RETURN d");
  assertExpressionFilter(
      vocbase(),
      "FOR d IN collection FILTER d.a.b.c.numeric >= 2 >= 3 RETURN d");

  // expression with self-reference is not supported by IResearch -> wrap it
  assertExpressionFilter(vocbase(),
                         "FOR d IN collection FILTER d.a >= 1+d.b RETURN d");

  // unsupported node types
  assertFilterFail(vocbase(), "FOR d IN collection FILTER d.a >= {} RETURN d");
  assertFilterFail(vocbase(), "FOR d IN collection FILTER {} <= d.a RETURN d");
  assertFilterExecutionFail(vocbase(),
                            "FOR d IN collection FILTER d.a >= 1..2 RETURN d",
                            &ExpressionContextMock::EMPTY);
  assertFilterExecutionFail(vocbase(),
                            "FOR d IN collection FILTER 1..2 <= d.a RETURN d",
                            &ExpressionContextMock::EMPTY);
}

TEST_F(IResearchFilterCompareTest, BinaryGT) {
  // simple string attribute
  {
    irs::Or expected;
    {
      auto& filter = expected.add<irs::by_range>();
      *filter.mutable_field() = mangleStringIdentity("a");
      auto* opts = filter.mutable_options();
      opts->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("1"));
      opts->range.min_type = irs::BoundType::EXCLUSIVE;
    }

    assertFilterSuccess(vocbase(),
                        "FOR d IN collection FILTER d.a > '1' RETURN d", expected);
    assertFilterSuccess(vocbase(),
                        "FOR d IN collection FILTER d['a'] > '1' RETURN d", expected);
    assertFilterSuccess(vocbase(),
                        "FOR d IN collection FILTER '1' < d.a RETURN d", expected);
    assertFilterSuccess(vocbase(),
                        "FOR d IN collection FILTER '1' < d['a'] RETURN d", expected);
  }

  // simple string offset
  {
    irs::Or expected;
    {
      auto& filter = expected.add<irs::by_range>();
      *filter.mutable_field() = mangleStringIdentity("[23]");
      auto* opts = filter.mutable_options();
      opts->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("1"));
      opts->range.min_type = irs::BoundType::EXCLUSIVE;
    }

    assertFilterSuccess(vocbase(),
                        "FOR d IN collection FILTER d[23] > '1' RETURN d", expected);
    assertFilterSuccess(vocbase(),
                        "FOR d IN collection FILTER '1' < d[23] RETURN d", expected);
  }

  // complex attribute name, string
  {
    irs::Or expected;
    {
      auto& filter = expected.add<irs::by_range>();
      *filter.mutable_field() = mangleStringIdentity("a.b.c");
      auto* opts = filter.mutable_options();
      opts->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("1"));
      opts->range.min_type = irs::BoundType::EXCLUSIVE;
    }

    assertFilterSuccess(vocbase(),
                        "FOR d IN collection FILTER d.a.b.c > '1' RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER d['a']['b']['c'] > '1' RETURN d", expected);
    assertFilterSuccess(vocbase(),
                        "FOR d IN collection FILTER '1' < d.a.b.c RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER '1' < d['a']['b'].c RETURN d", expected);
  }

  // complex attribute name with offset, string
  {
    irs::Or expected;
    {
      auto& filter = expected.add<irs::by_range>();
      *filter.mutable_field() = mangleStringIdentity("a.b[23].c");
      auto* opts = filter.mutable_options();
      opts->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("1"));
      opts->range.min_type = irs::BoundType::EXCLUSIVE;
    }

    assertFilterSuccess(vocbase(),
                        "FOR d IN collection FILTER d.a.b[23].c > '1' RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER d.a['b'][23]['c'] > '1' RETURN d", expected);
    assertFilterSuccess(vocbase(),
                        "FOR d IN collection FILTER '1' < d.a.b[23].c RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER '1' < d['a']['b'][23].c RETURN d", expected);
  }

  // string expression
  {
    arangodb::aql::Variable var("c", 0, /*isDataFromColl*/ false);
    arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt{41});
    arangodb::aql::AqlValueGuard guard(value, true);

    ExpressionContextMock ctx;
    ctx.vars.emplace(var.name, value);

    irs::Or expected;
    {
      auto& filter = expected.add<irs::by_range>();
      *filter.mutable_field() = mangleStringIdentity("a.b[23].c");
      auto* opts = filter.mutable_options();
      opts->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("42"));
      opts->range.min_type = irs::BoundType::EXCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER d.a.b[23].c > TO_STRING(c+1) "
        "RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER d.a['b'][23]['c'] > "
        "TO_STRING(c+1) RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER TO_STRING(c+1) < d.a.b[23].c "
        "RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER analyzer(TO_STRING(c+1) < "
        "d['a']['b'][23].c, 'identity') RETURN d",
        expected, &ctx);
  }

  // string expression, boost, analyzer
  {
    arangodb::aql::Variable var("c", 0, /*isDataFromColl*/ false);
    arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt{41});
    arangodb::aql::AqlValueGuard guard(value, true);

    ExpressionContextMock ctx;
    ctx.vars.emplace(var.name, value);

    irs::Or expected;
    {
      auto& filter = expected.add<irs::by_range>();
      filter.boost(42);
      *filter.mutable_field() = mangleString("a.b[23].c", "test_analyzer");
      auto* opts = filter.mutable_options();
      opts->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("42"));
      opts->range.min_type = irs::BoundType::EXCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER analyzer(boost(d.a.b[23].c > "
        "TO_STRING(c+1), c+1), 'test_analyzer') RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER boost(analyzer(d.a['b'][23]['c'] "
        "> TO_STRING(c+1),'test_analyzer'),c+1) RETURN d",
        expected, &ctx);
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
    {
      auto& filter = expected.add<irs::by_range>();
      *filter.mutable_field() = mangleStringIdentity("a.b.c.e[4].f[5].g[3].g.a");
      auto* opts = filter.mutable_options();
      opts->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("42"));
      opts->range.min_type = irs::BoundType::EXCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(),
        "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "> '42' RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER '42' < "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "RETURN d",
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
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "> '42' RETURN d",
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
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "> '42' RETURN d",
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
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "> '42' RETURN d",
        &ctx);
  }

  // complex boolean attribute, true
  {
    irs::Or expected;
    {
      auto& filter = expected.add<irs::by_range>();
      *filter.mutable_field() = mangleBool("a.b.c");
      auto* opts = filter.mutable_options();
      opts->range.min = irs::ref_cast<irs::byte_type>(irs::boolean_token_stream::value_true());
      opts->range.min_type = irs::BoundType::EXCLUSIVE;
    }

    assertFilterSuccess(vocbase(),
                        "FOR d IN collection FILTER d.a.b.c > true RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER d['a']['b']['c'] > true RETURN d", expected);
    assertFilterSuccess(vocbase(),
                        "FOR d IN collection FILTER true < d.a.b.c RETURN d", expected);
    assertFilterSuccess(vocbase(),
                        "FOR d IN collection FILTER true < d['a'].b.c RETURN d", expected);
  }

  // complex boolean attribute, false
  {
    irs::Or expected;
    {
      auto& filter = expected.add<irs::by_range>();
      *filter.mutable_field() = mangleBool("a.b.c.bool");
      auto* opts = filter.mutable_options();
      opts->range.min = irs::ref_cast<irs::byte_type>(irs::boolean_token_stream::value_false());
      opts->range.min_type = irs::BoundType::EXCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER d.a.b.c.bool > false RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER d['a'].b.c.bool > false RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER false < d.a.b.c.bool RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER false < d['a']['b']['c'].bool RETURN d", expected);
  }

  // complex boolean attribute with, false
  {
    irs::Or expected;
    {
      auto& filter = expected.add<irs::by_range>();
      *filter.mutable_field() = mangleBool("a.b.c[223].bool");
      auto* opts = filter.mutable_options();
      opts->range.min = irs::ref_cast<irs::byte_type>(irs::boolean_token_stream::value_false());
      opts->range.min_type = irs::BoundType::EXCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a.b.c[223].bool > false RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d['a'].b.c[223].bool > false RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER false < d.a.b.c[223].bool RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER false < d['a']['b']['c'][223].bool RETURN "
        "d",
        expected);
  }

  // boolean expression
  {
    arangodb::aql::Variable var("c", 0, /*isDataFromColl*/ false);
    arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt{41});
    arangodb::aql::AqlValueGuard guard(value, true);

    ExpressionContextMock ctx;
    ctx.vars.emplace(var.name, value);

    irs::Or expected;
    {
      auto& filter = expected.add<irs::by_range>();
      *filter.mutable_field() = mangleBool("a.b[23].c");
      auto* opts = filter.mutable_options();
      opts->range.min = irs::ref_cast<irs::byte_type>(irs::boolean_token_stream::value_false());
      opts->range.min_type = irs::BoundType::EXCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER d.a.b[23].c > TO_BOOL(c-41) "
        "RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER d.a['b'][23]['c'] > TO_BOOL(c-41) "
        "RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER TO_BOOL(c-41) < d.a.b[23].c "
        "RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER TO_BOOL(c-41) < d['a']['b'][23].c "
        "RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER analyzer(TO_BOOL(c-41) < "
        "d['a']['b'][23].c, 'test_analyzer') RETURN d",
        expected, &ctx);
  }

  // boolean expression, boost
  {
    arangodb::aql::Variable var("c", 0, /*isDataFromColl*/ false);
    arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt{41});
    arangodb::aql::AqlValueGuard guard(value, true);

    ExpressionContextMock ctx;
    ctx.vars.emplace(var.name, value);

    irs::Or expected;
    {
      auto& filter = expected.add<irs::by_range>();
      filter.boost(42);
      *filter.mutable_field() = mangleBool("a.b[23].c");
      auto* opts = filter.mutable_options();
      opts->range.min = irs::ref_cast<irs::byte_type>(irs::boolean_token_stream::value_false());
      opts->range.min_type = irs::BoundType::EXCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER boost(d.a.b[23].c > "
        "TO_BOOL(c-41), c+1) RETURN d",
        expected, &ctx);
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
    {
      auto& filter = expected.add<irs::by_range>();
      *filter.mutable_field() = mangleBool("a.b.c.e[4].f[5].g[3].g.a");
      auto* opts = filter.mutable_options();
      opts->range.min = irs::ref_cast<irs::byte_type>(irs::boolean_token_stream::value_false());
      opts->range.min_type = irs::BoundType::EXCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(),
        "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "> false RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER false < "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "RETURN d",
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
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "> false RETURN d",
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
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "> false RETURN d",
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
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "> false RETURN d",
        &ctx);
  }

  // complex null attribute
  {
    irs::Or expected;
    {
      auto& filter = expected.add<irs::by_range>();
      *filter.mutable_field() = mangleNull("a.b.c.nil");
      auto* opts = filter.mutable_options();
      opts->range.min = irs::ref_cast<irs::byte_type>(irs::null_token_stream::value_null());
      opts->range.min_type = irs::BoundType::EXCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER d.a.b.c.nil > null RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER d['a'].b.c.nil > null RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER null < d.a.b.c.nil RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER null < d['a'].b.c.nil RETURN d", expected);
  }

  // null expression
  {
    arangodb::aql::Variable var("c", 0, /*isDataFromColl*/ false);
    arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintNull{});
    arangodb::aql::AqlValueGuard guard(value, true);

    ExpressionContextMock ctx;
    ctx.vars.emplace(var.name, value);

    irs::Or expected;
    {
      auto& filter = expected.add<irs::by_range>();
      *filter.mutable_field() = mangleNull("a.b[23].c");
      auto* opts = filter.mutable_options();
      opts->range.min = irs::ref_cast<irs::byte_type>(irs::null_token_stream::value_null());
      opts->range.min_type = irs::BoundType::EXCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(),
        "LET c=null FOR d IN collection FILTER d.a.b[23].c > (c && false) "
        "RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=null FOR d IN collection FILTER d.a['b'][23]['c'] > (c && true) "
        "RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=null FOR d IN collection FILTER (c && false) < d.a.b[23].c "
        "RETURN d",
        expected, &ctx);
    assertFilterSuccess(vocbase(),
                        "LET c=null FOR d IN collection FILTER (c && false) < "
                        "d['a']['b'][23].c RETURN d",
                        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=null FOR d IN collection FILTER analyzer((c && false) < "
        "d['a']['b'][23].c, 'test_analyzer') RETURN d",
        expected, &ctx);
  }

  // null expression, boost
  {
    arangodb::aql::Variable var("c", 0, /*isDataFromColl*/ false);
    arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintNull{});
    arangodb::aql::AqlValueGuard guard(value, true);

    ExpressionContextMock ctx;
    ctx.vars.emplace(var.name, value);

    irs::Or expected;
    {
      auto& filter = expected.add<irs::by_range>();
      filter.boost(42);
      *filter.mutable_field() = mangleNull("a.b[23].c");
      auto* opts = filter.mutable_options();
      opts->range.min = irs::ref_cast<irs::byte_type>(irs::null_token_stream::value_null());
      opts->range.min_type = irs::BoundType::EXCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(),
        "LET c=null FOR d IN collection FILTER boost(d.a.b[23].c > (c && "
        "false), c+42) RETURN d",
        expected, &ctx);
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
    {
      auto& filter = expected.add<irs::by_range>();
      *filter.mutable_field() = mangleNull("a.b.c.e[4].f[5].g[3].g.a");
      auto* opts = filter.mutable_options();
      opts->range.min = irs::ref_cast<irs::byte_type>(irs::null_token_stream::value_null());
      opts->range.min_type = irs::BoundType::EXCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(),
        "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "> null RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER null < "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "RETURN d",
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
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "> null RETURN d",
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
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "> null RETURN d",
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
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "> null RETURN d",
        &ctx);
  }

  // complex null attribute with offset
  {
    irs::Or expected;
    {
      auto& filter = expected.add<irs::by_range>();
      *filter.mutable_field() = mangleNull("a.b[23].c.nil");
      auto* opts = filter.mutable_options();
      opts->range.min = irs::ref_cast<irs::byte_type>(irs::null_token_stream::value_null());
      opts->range.min_type = irs::BoundType::EXCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER d.a.b[23].c.nil > null RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d['a']['b'][23]['c'].nil > null RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER null < d.a.b[23].c.nil RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER null < d['a']['b'][23].c.nil RETURN d", expected);
  }

  // complex boolean attribute, numeric
  {
    irs::numeric_token_stream stream;
    stream.reset(13.);

    irs::Or expected;
    {
      auto& filter = expected.add<irs::by_granular_range>();
      *filter.mutable_field() = mangleNumeric("a.b.c.numeric");
      auto* opts = filter.mutable_options();
      irs::set_granular_term(opts->range.min, stream);
      opts->range.min_type = irs::BoundType::EXCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER d.a.b.c.numeric > 13 RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d['a']['b']['c'].numeric > 13 RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER d.a.b.c.numeric > 13.0 RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER 13 < d.a.b.c.numeric RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER 13.0 < d.a.b.c.numeric RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER 13.0 < d['a']['b'].c.numeric RETURN d", expected);
  }

  // complex numeric attribute, floating
  {
    irs::numeric_token_stream stream;
    stream.reset(13.5);

    irs::Or expected;
    {
      auto& filter = expected.add<irs::by_granular_range>();
      *filter.mutable_field() = mangleNumeric("a.b.c.numeric");
      auto* opts = filter.mutable_options();
      irs::set_granular_term(opts->range.min, stream);
      opts->range.min_type = irs::BoundType::EXCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER d.a.b.c.numeric > 13.5 RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a['b']['c'].numeric > 13.5 RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER 13.5 < d.a.b.c.numeric RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER 13.5 < d['a']['b'].c.numeric RETURN d", expected);
  }

  // complex numeric attribute, integer
  {
    irs::numeric_token_stream stream;
    stream.reset(13.);

    irs::Or expected;
    {
      auto& filter = expected.add<irs::by_granular_range>();
      *filter.mutable_field() = mangleNumeric("a[1].b.c[223].numeric");
      auto* opts = filter.mutable_options();
      irs::set_granular_term(opts->range.min, stream);
      opts->range.min_type = irs::BoundType::EXCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a[1].b.c[223].numeric > 13 RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d['a'][1]['b'].c[223].numeric > 13 RETURN "
        "d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a[1].b.c[223].numeric > 13.0 RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER 13 < d.a[1].b.c[223].numeric RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER 13.0 < d.a[1].b.c[223].numeric RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER 13.0 < d['a'][1]['b']['c'][223].numeric "
        "RETURN d",
        expected);
  }

  // numeric expression
  {
    arangodb::aql::Variable var("c", 0, /*isDataFromColl*/ false);
    arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt{41});
    arangodb::aql::AqlValueGuard guard(value, true);

    ExpressionContextMock ctx;
    ctx.vars.emplace(var.name, value);

    irs::numeric_token_stream stream;
    stream.reset(42.5);

    irs::Or expected;
    {
      auto& filter = expected.add<irs::by_granular_range>();
      *filter.mutable_field() = mangleNumeric("a.b[23].c");
      auto* opts = filter.mutable_options();
      irs::set_granular_term(opts->range.min, stream);
      opts->range.min_type = irs::BoundType::EXCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER d.a.b[23].c > (c+1.5) RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER d.a['b'][23]['c'] > (c+1.5) "
        "RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER (c+1.5) < d.a.b[23].c RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER (c+1.5) < d['a']['b'][23].c "
        "RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER analyzer((c+1.5) < "
        "d['a']['b'][23].c, 'test_analyzer') RETURN d",
        expected, &ctx);
  }

  // numeric expression, boost
  {
    arangodb::aql::Variable var("c", 0, /*isDataFromColl*/ false);
    arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt{41});
    arangodb::aql::AqlValueGuard guard(value, true);

    ExpressionContextMock ctx;
    ctx.vars.emplace(var.name, value);

    irs::numeric_token_stream stream;
    stream.reset(42.5);

    irs::Or expected;
    {
      auto& filter = expected.add<irs::by_granular_range>();
      filter.boost(42);
      *filter.mutable_field() = mangleNumeric("a.b[23].c");
      auto* opts = filter.mutable_options();
      irs::set_granular_term(opts->range.min, stream);
      opts->range.min_type = irs::BoundType::EXCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER boost(d.a.b[23].c > (c+1.5),c+1) "
        "RETURN d",
        expected, &ctx);
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

    irs::numeric_token_stream stream;
    stream.reset(42.5);

    irs::Or expected;
    {
      auto& filter = expected.add<irs::by_granular_range>();
      *filter.mutable_field() = mangleNumeric("a.b.c.e[4].f[5].g[3].g.a");
      auto* opts = filter.mutable_options();
      irs::set_granular_term(opts->range.min, stream);
      opts->range.min_type = irs::BoundType::EXCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(),
        "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "> 42.5 RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER 42.5 < "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "RETURN d",
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
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "> 42.5 RETURN d",
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
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "> 42.5 RETURN d",
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
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "> 42.5 RETURN d",
        &ctx);
  }

  // complex expression
  {
    irs::Or expected;
    {
      auto& filter = expected.add<irs::by_range>();
      *filter.mutable_field() = mangleBool("a.b.c");
      auto* opts = filter.mutable_options();
      opts->range.max = irs::ref_cast<irs::byte_type>(irs::boolean_token_stream::value_true());
      opts->range.max_type = irs::BoundType::EXCLUSIVE;
    }

    assertFilterSuccess(vocbase(),
                        "FOR d IN collection FILTER 3 > 2 > d.a.b.c RETURN d",
                        expected, &ExpressionContextMock::EMPTY);
  }

  // expression without reference to loop variable, unreachable criteria
  {
    auto obj = arangodb::velocypack::Parser::fromJson("{}");

    ExpressionContextMock ctx;
    ctx.vars.emplace("k", arangodb::aql::AqlValue(obj->slice()));

    irs::Or expected;
    expected.add<irs::empty>();

    assertFilterSuccess(
        vocbase(), "LET k={} FOR d IN collection FILTER k.a > '1' RETURN d", expected, &ctx);
  }

  // array in expression
  {
    irs::Or expected;
    expected.add<irs::all>();

    assertFilterSuccess(vocbase(),
                        "FOR d IN collection FILTER [] > '1' RETURN d",
                        expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess(vocbase(),
                        "FOR d IN collection FILTER ['d'] > '1' RETURN d",
                        expected, &ExpressionContextMock::EMPTY);
  }

  // nondeterministic expression -> wrap it
  assertExpressionFilter(
      vocbase(),
      "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
      "collection FILTER "
      "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_NONDETERM_('a')] "
      "> '1' RETURN d");
  assertExpressionFilter(
      vocbase(),
      "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
      "collection FILTER '1' > "
      "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_NONDETERM_('a')] "
      "RETURN d");
  assertExpressionFilter(
      vocbase(), "FOR d IN collection FILTER d.a > _NONDETERM_('1') RETURN d");
  assertExpressionFilter(
      vocbase(),
      "LET k={} FOR d IN collection FILTER k.a > _NONDETERM_('1') RETURN d");

  // unsupported expression (d referenced inside) -> wrap it
  assertExpressionFilter(
      vocbase(), "FOR d IN collection FILTER 3 > (2 > d.a.b.c) RETURN d");
  assertExpressionFilter(
      vocbase(),
      "FOR d IN collection FILTER 3 > boost(2 > d.a.b.c, 1.5) RETURN d");
  assertExpressionFilter(
      vocbase(),
      "FOR d IN collection FILTER analyzer(3, 'test_analyzer') > boost(2 > "
      "d.a.b.c, 1.5) RETURN d");

  // expression is not supported by IResearch -> wrap it
  assertExpressionFilter(vocbase(),
                         "FOR d IN collection FILTER d > '1' RETURN d");
  assertExpressionFilter(vocbase(),
                         "FOR d IN collection FILTER d[*] > '1' RETURN d");
  assertExpressionFilter(vocbase(),
                         "FOR d IN collection FILTER d.a[*] > '1' RETURN d");
  assertExpressionFilter(vocbase(),
                         "FOR d IN collection FILTER '1' < d RETURN d");
  assertExpressionFilter(
      vocbase(), "FOR d IN collection FILTER 2 > d.a.b.c.numeric > 3 RETURN d");
  assertExpressionFilter(
      vocbase(), "FOR d IN collection FILTER d.a.b.c.numeric > 2 > 3 RETURN d");
  assertExpressionFilter(
      vocbase(), "FOR d IN collection FILTER d.a.b.c.numeric > 2 > 3 RETURN d");

  // expression with self-reference is not supported by IResearch -> wrap it
  assertExpressionFilter(vocbase(),
                         "FOR d IN collection FILTER d.a > 1+d.b RETURN d");

  // unsupported node types
  assertFilterFail(vocbase(), "FOR d IN collection FILTER d.a > {} RETURN d");
  assertFilterFail(vocbase(), "FOR d IN collection FILTER {} < d.a RETURN d");
  assertFilterExecutionFail(vocbase(),
                            "FOR d IN collection FILTER d.a > 1..2 RETURN d",
                            &ExpressionContextMock::EMPTY);
  assertFilterExecutionFail(vocbase(),
                            "FOR d IN collection FILTER 1..2 < d.a RETURN d",
                            &ExpressionContextMock::EMPTY);
}

TEST_F(IResearchFilterCompareTest, BinaryLE) {
  // simple string attribute
  {
    irs::Or expected;
    {
      auto& filter = expected.add<irs::by_range>();
      *filter.mutable_field() = mangleStringIdentity("a");
      auto* opts = filter.mutable_options();
      opts->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("1"));
      opts->range.max_type = irs::BoundType::INCLUSIVE;
    }

    assertFilterSuccess(vocbase(),
                        "FOR d IN collection FILTER d.a <= '1' RETURN d", expected);
    assertFilterSuccess(vocbase(),
                        "FOR d IN collection FILTER d['a'] <= '1' RETURN d", expected);
    assertFilterSuccess(vocbase(),
                        "FOR d IN collection FILTER '1' >= d.a RETURN d", expected);
    assertFilterSuccess(vocbase(),
                        "FOR d IN collection FILTER '1' >= d['a'] RETURN d", expected);
  }

  // simple string offset
  {
    irs::Or expected;
    {
      auto& filter = expected.add<irs::by_range>();
      *filter.mutable_field() = mangleStringIdentity("[23]");
      auto* opts = filter.mutable_options();
      opts->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("1"));
      opts->range.max_type = irs::BoundType::INCLUSIVE;
    }

    assertFilterSuccess(vocbase(),
                        "FOR d IN collection FILTER d[23] <= '1' RETURN d", expected);
    assertFilterSuccess(vocbase(),
                        "FOR d IN collection FILTER '1' >= d[23] RETURN d", expected);
  }

  // complex attribute name, string
  {
    irs::Or expected;
    {
      auto& filter = expected.add<irs::by_range>();
      *filter.mutable_field() = mangleStringIdentity("a.b.c");
      auto* opts = filter.mutable_options();
      opts->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("1"));
      opts->range.max_type = irs::BoundType::INCLUSIVE;
    }

    assertFilterSuccess(vocbase(),
                        "FOR d IN collection FILTER d.a.b.c <= '1' RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER d['a']['b'].c <= '1' RETURN d", expected);
    assertFilterSuccess(vocbase(),
                        "FOR d IN collection FILTER '1' >= d.a.b.c RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER '1' >= d['a']['b']['c'] RETURN d", expected);
  }

  // complex attribute name with offset, string
  {
    irs::Or expected;
    {
      auto& filter = expected.add<irs::by_range>();
      *filter.mutable_field() = mangleStringIdentity("a[1].b.c[42]");
      auto* opts = filter.mutable_options();
      opts->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("1"));
      opts->range.max_type = irs::BoundType::INCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER d.a[1].b.c[42] <= '1' RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d['a'][1]['b'].c[42] <= '1' RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER '1' >= d.a[1].b.c[42] RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER '1' >= d['a'][1]['b']['c'][42] RETURN d", expected);
  }

  // string expression
  {
    arangodb::aql::Variable var("c", 0, /*isDataFromColl*/ false);
    arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt{41});
    arangodb::aql::AqlValueGuard guard(value, true);

    ExpressionContextMock ctx;
    ctx.vars.emplace(var.name, value);

    irs::Or expected;
    {
      auto& filter = expected.add<irs::by_range>();
      *filter.mutable_field() = mangleStringIdentity("a.b[23].c");
      auto* opts = filter.mutable_options();
      opts->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("42"));
      opts->range.max_type = irs::BoundType::INCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER d.a.b[23].c <= TO_STRING(c+1) "
        "RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER d.a['b'][23]['c'] <= "
        "TO_STRING(c+1) RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER TO_STRING(c+1) >= d.a.b[23].c "
        "RETURN d",
        expected, &ctx);
    assertFilterSuccess(vocbase(),
                        "LET c=41 FOR d IN collection FILTER TO_STRING(c+1) >= "
                        "d['a']['b'][23].c RETURN d",
                        expected, &ctx);
  }

  // string expression, analyzer, boost
  {
    arangodb::aql::Variable var("c", 0, /*isDataFromColl*/ false);
    arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt{41});
    arangodb::aql::AqlValueGuard guard(value, true);

    ExpressionContextMock ctx;
    ctx.vars.emplace(var.name, value);

    irs::Or expected;
    {
      auto& filter = expected.add<irs::by_range>();
      filter.boost(42);
      *filter.mutable_field() = mangleString("a.b[23].c", "test_analyzer");
      auto* opts = filter.mutable_options();
      opts->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("42"));
      opts->range.max_type = irs::BoundType::INCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER analyzer(boost(d.a.b[23].c <= "
        "TO_STRING(c+1), c+1), 'test_analyzer') RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER boost(analyzer(d.a['b'][23]['c'] "
        "<= TO_STRING(c+1), 'test_analyzer'), c+1) RETURN d",
        expected, &ctx);
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
    {
      auto& filter = expected.add<irs::by_range>();
      *filter.mutable_field() = mangleStringIdentity("a.b.c.e[4].f[5].g[3].g.a");
      auto* opts = filter.mutable_options();
      opts->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("42"));
      opts->range.max_type = irs::BoundType::INCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(),
        "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "<= '42' RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER '42' >= "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "RETURN d",
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
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "<= '42' RETURN d",
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
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "<= '42' RETURN d",
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
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "<= '42' RETURN d",
        &ctx);
  }

  // complex boolean attribute, true
  {
    irs::Or expected;
    {
      auto& filter = expected.add<irs::by_range>();
      *filter.mutable_field() = mangleBool("a.b.c");
      auto* opts = filter.mutable_options();
      opts->range.max = irs::ref_cast<irs::byte_type>(irs::boolean_token_stream::value_true());
      opts->range.max_type = irs::BoundType::INCLUSIVE;
    }

    assertFilterSuccess(vocbase(),
                        "FOR d IN collection FILTER d.a.b.c <= true RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d['a']['b']['c'] <= true RETURN d", expected);
    assertFilterSuccess(vocbase(),
                        "FOR d IN collection FILTER true >= d.a.b.c RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER true >= d.a['b']['c'] RETURN d", expected);
  }

  // complex boolean attribute, true
  {
    irs::Or expected;
    {
      auto& filter = expected.add<irs::by_range>();
      *filter.mutable_field() = mangleBool("a.b[42].c");
      auto* opts = filter.mutable_options();
      opts->range.max = irs::ref_cast<irs::byte_type>(irs::boolean_token_stream::value_true());
      opts->range.max_type = irs::BoundType::INCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER d.a.b[42].c <= true RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d['a']['b'][42]['c'] <= true RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER true >= d.a.b[42].c RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER true >= d.a['b'][42]['c'] RETURN d", expected);
  }

  // complex boolean attribute, false
  {
    irs::Or expected;
    {
      auto& filter = expected.add<irs::by_range>();
      *filter.mutable_field() = mangleBool("a.b.c.bool");
      auto* opts = filter.mutable_options();
      opts->range.max = irs::ref_cast<irs::byte_type>(irs::boolean_token_stream::value_false());
      opts->range.max_type = irs::BoundType::INCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER d.a.b.c.bool <= false RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d['a'].b.c.bool <= false RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER false >= d.a.b.c.bool RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER false >= d.a['b']['c'].bool RETURN d", expected);
  }

  // boolean expression
  {
    arangodb::aql::Variable var("c", 0, /*isDataFromColl*/ false);
    arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt{41});
    arangodb::aql::AqlValueGuard guard(value, true);

    ExpressionContextMock ctx;
    ctx.vars.emplace(var.name, value);

    irs::Or expected;
    {
      auto& filter = expected.add<irs::by_range>();
      *filter.mutable_field() = mangleBool("a.b[23].c");
      auto* opts = filter.mutable_options();
      opts->range.max = irs::ref_cast<irs::byte_type>(irs::boolean_token_stream::value_false());
      opts->range.max_type = irs::BoundType::INCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER d.a.b[23].c <= TO_BOOL(c-41) "
        "RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER d.a['b'][23]['c'] <= "
        "TO_BOOL(c-41) RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER TO_BOOL(c-41) >= d.a.b[23].c "
        "RETURN d",
        expected, &ctx);
    assertFilterSuccess(vocbase(),
                        "LET c=41 FOR d IN collection FILTER TO_BOOL(c-41) >= "
                        "d['a']['b'][23].c RETURN d",
                        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER analyzer(TO_BOOL(c-41) >= "
        "d['a']['b'][23].c, 'test_analyzer') RETURN d",
        expected, &ctx);
  }

  // boolean expression, boost
  {
    arangodb::aql::Variable var("c", 0, /*isDataFromColl*/ false);
    arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt{41});
    arangodb::aql::AqlValueGuard guard(value, true);

    ExpressionContextMock ctx;
    ctx.vars.emplace(var.name, value);

    irs::Or expected;
    {
      auto& filter = expected.add<irs::by_range>();
      filter.boost(42);
      *filter.mutable_field() = mangleBool("a.b[23].c");
      auto* opts = filter.mutable_options();
      opts->range.max = irs::ref_cast<irs::byte_type>(irs::boolean_token_stream::value_false());
      opts->range.max_type = irs::BoundType::INCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER boost(d.a.b[23].c <= "
        "TO_BOOL(c-41),c+1) RETURN d",
        expected, &ctx);
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
    {
      auto& filter = expected.add<irs::by_range>();
      *filter.mutable_field() = mangleBool("a.b.c.e[4].f[5].g[3].g.a");
      auto* opts = filter.mutable_options();
      opts->range.max = irs::ref_cast<irs::byte_type>(irs::boolean_token_stream::value_false());
      opts->range.max_type = irs::BoundType::INCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(),
        "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "<= false RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER false >= "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "RETURN d",
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
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "<= false RETURN d",
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
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "<= false RETURN d",
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
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "<= false RETURN d",
        &ctx);
  }

  // complex null attribute
  {
    irs::Or expected;
    {
      auto& filter = expected.add<irs::by_range>();
      *filter.mutable_field() = mangleNull("a.b.c.nil");
      auto* opts = filter.mutable_options();
      opts->range.max = irs::ref_cast<irs::byte_type>(irs::null_token_stream::value_null());
      opts->range.max_type = irs::BoundType::INCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER d.a.b.c.nil <= null RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a['b']['c'].nil <= null RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER null >= d.a.b.c.nil RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER null >= d['a']['b']['c'].nil RETURN d", expected);
  }

  // complex null attribute with offset
  {
    irs::Or expected;
    {
      auto& filter = expected.add<irs::by_range>();
      *filter.mutable_field() = mangleNull("a.b.c.nil[1]");
      auto* opts = filter.mutable_options();
      opts->range.max = irs::ref_cast<irs::byte_type>(irs::null_token_stream::value_null());
      opts->range.max_type = irs::BoundType::INCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER d.a.b.c.nil[1] <= null RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a['b']['c'].nil[1] <= null RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER null >= d.a.b.c.nil[1] RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER null >= d['a']['b']['c'].nil[1] RETURN d", expected);
  }

  // null expression
  {
    arangodb::aql::Variable var("c", 0, /*isDataFromColl*/ false);
    arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintNull{});
    arangodb::aql::AqlValueGuard guard(value, true);

    ExpressionContextMock ctx;
    ctx.vars.emplace(var.name, value);

    irs::Or expected;
    {
      auto& filter = expected.add<irs::by_range>();
      *filter.mutable_field() = mangleNull("a.b[23].c");
      auto* opts = filter.mutable_options();
      opts->range.max = irs::ref_cast<irs::byte_type>(irs::null_token_stream::value_null());
      opts->range.max_type = irs::BoundType::INCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(),
        "LET c=null FOR d IN collection FILTER d.a.b[23].c <= (c && false) "
        "RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=null FOR d IN collection FILTER d.a['b'][23]['c'] <= (c && "
        "true) RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=null FOR d IN collection FILTER (c && false) >= d.a.b[23].c "
        "RETURN d",
        expected, &ctx);
    assertFilterSuccess(vocbase(),
                        "LET c=null FOR d IN collection FILTER (c && false) >= "
                        "d['a']['b'][23].c RETURN d",
                        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=null FOR d IN collection FILTER analyzer((c && false) >= "
        "d['a']['b'][23].c, 'test_analyzer') RETURN d",
        expected, &ctx);
  }

  // null expression, boost
  {
    arangodb::aql::Variable var("c", 0, /*isDataFromColl*/ false);
    arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintNull{});
    arangodb::aql::AqlValueGuard guard(value, true);

    ExpressionContextMock ctx;
    ctx.vars.emplace(var.name, value);

    irs::Or expected;
    {
      auto& filter = expected.add<irs::by_range>();
      filter.boost(42);
      *filter.mutable_field() = mangleNull("a.b[23].c");
      auto* opts = filter.mutable_options();
      opts->range.max = irs::ref_cast<irs::byte_type>(irs::null_token_stream::value_null());
      opts->range.max_type = irs::BoundType::INCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(),
        "LET c=null FOR d IN collection FILTER boost(d.a.b[23].c <= (c && "
        "false), c+42) RETURN d",
        expected, &ctx);
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
    {
      auto& filter = expected.add<irs::by_range>();
      *filter.mutable_field() = mangleNull("a.b.c.e[4].f[5].g[3].g.a");
      auto* opts = filter.mutable_options();
      opts->range.max = irs::ref_cast<irs::byte_type>(irs::null_token_stream::value_null());
      opts->range.max_type = irs::BoundType::INCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(),
        "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "<= null RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER null >= "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "RETURN d",
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
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "<= null RETURN d",
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
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "<= null RETURN d",
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
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "<= null RETURN d",
        &ctx);
  }

  // complex numeric attribute
  {
    irs::numeric_token_stream stream;
    stream.reset(13.);

    irs::Or expected;
    {
      auto& filter = expected.add<irs::by_granular_range>();
      *filter.mutable_field() = mangleNumeric("a.b.c.numeric");
      auto* opts = filter.mutable_options();
      irs::set_granular_term(opts->range.max, stream);
      opts->range.max_type = irs::BoundType::INCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER d.a.b.c.numeric <= 13 RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d['a']['b']['c'].numeric <= 13 RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER d.a.b.c.numeric <= 13.0 RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER 13 >= d.a.b.c.numeric RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER 13.0 >= d.a.b.c.numeric RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER 13.0 >= d.a['b']['c'].numeric RETURN d", expected);
  }

  // complex numeric attribute with offset
  {
    irs::numeric_token_stream stream;
    stream.reset(13.);

    irs::Or expected;
    {
      auto& filter = expected.add<irs::by_granular_range>();
      *filter.mutable_field() = mangleNumeric("a.b.c[223].numeric");
      auto* opts = filter.mutable_options();
      irs::set_granular_term(opts->range.max, stream);
      opts->range.max_type = irs::BoundType::INCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a.b.c[223].numeric <= 13 RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d['a']['b']['c'][223].numeric <= 13 RETURN "
        "d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a.b.c[223].numeric <= 13.0 RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER 13 >= d.a.b.c[223].numeric RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER 13.0 >= d.a.b.c[223].numeric RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER 13.0 >= d.a['b']['c'][223].numeric RETURN "
        "d",
        expected);
  }

  // numeric expression
  {
    arangodb::aql::Variable var("c", 0, /*isDataFromColl*/ false);
    arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt{41});
    arangodb::aql::AqlValueGuard guard(value, true);

    ExpressionContextMock ctx;
    ctx.vars.emplace(var.name, value);

    irs::numeric_token_stream stream;
    stream.reset(42.5);

    irs::Or expected;
    {
      auto& filter = expected.add<irs::by_granular_range>();
      *filter.mutable_field() = mangleNumeric("a.b[23].c");
      auto* opts = filter.mutable_options();
      irs::set_granular_term(opts->range.max, stream);
      opts->range.max_type = irs::BoundType::INCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER d.a.b[23].c <= (c+1.5) RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER d.a['b'][23]['c'] <= (c+1.5) "
        "RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER (c+1.5) >= d.a.b[23].c RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER (c+1.5) >= d['a']['b'][23].c "
        "RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER analyzer((c+1.5) >= "
        "d['a']['b'][23].c, 'test_analyzer') RETURN d",
        expected, &ctx);
  }

  // numeric expression, boost
  {
    arangodb::aql::Variable var("c", 0, /*isDataFromColl*/ false);
    arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt{41});
    arangodb::aql::AqlValueGuard guard(value, true);

    ExpressionContextMock ctx;
    ctx.vars.emplace(var.name, value);

    irs::numeric_token_stream stream;
    stream.reset(42.5);

    irs::Or expected;
    {
      auto& filter = expected.add<irs::by_granular_range>();
      filter.boost(42.5);
      *filter.mutable_field() = mangleNumeric("a.b[23].c");
      auto* opts = filter.mutable_options();
      irs::set_granular_term(opts->range.max, stream);
      opts->range.max_type = irs::BoundType::INCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER boost(d.a.b[23].c <= (c+1.5), "
        "c+1.5) RETURN d",
        expected, &ctx);
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

    irs::numeric_token_stream stream;
    stream.reset(42.5);

    irs::Or expected;
    {
      auto& filter = expected.add<irs::by_granular_range>();
      *filter.mutable_field() = mangleNumeric("a.b.c.e[4].f[5].g[3].g.a");
      auto* opts = filter.mutable_options();
      irs::set_granular_term(opts->range.max, stream);
      opts->range.max_type = irs::BoundType::INCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(),
        "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "<= 42.5 RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER 42.5 >= "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "RETURN d",
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
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "<= 42.5 RETURN d",
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
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "<= 42.5 RETURN d",
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
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "<= 42.5 RETURN d",
        &ctx);
  }

  // complex expression
  {
    irs::Or expected;
    {
      auto& filter = expected.add<irs::by_range>();
      *filter.mutable_field() = mangleBool("a.b.c");
      auto* opts = filter.mutable_options();
      opts->range.min = irs::ref_cast<irs::byte_type>(irs::boolean_token_stream::value_false());
      opts->range.min_type = irs::BoundType::INCLUSIVE;
    }

    assertFilterSuccess(vocbase(),
                        "FOR d IN collection FILTER 3 <= 2 <= d.a.b.c RETURN d",
                        expected, &ExpressionContextMock::EMPTY);
  }

  // expression without reference to loop variable, unreachable criteria
  {
    auto obj = arangodb::velocypack::Parser::fromJson("{}");

    ExpressionContextMock ctx;
    ctx.vars.emplace("k", arangodb::aql::AqlValue(obj->slice()));

    irs::Or expected;
    expected.add<irs::all>();

    assertFilterSuccess(
        vocbase(), "LET k={} FOR d IN collection FILTER k.a <= '1' RETURN d",
        expected, &ctx);
  }

  // array in expression
  {
    irs::Or expected;
    expected.add<irs::empty>();

    assertFilterSuccess(vocbase(),
                        "FOR d IN collection FILTER [] <= '1' RETURN d",
                        expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess(vocbase(),
                        "FOR d IN collection FILTER ['d'] <= '1' RETURN d",
                        expected, &ExpressionContextMock::EMPTY);
  }

  // nondeterministic expression -> wrap it
  assertExpressionFilter(
      vocbase(),
      "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
      "collection FILTER "
      "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_NONDETERM_('a')] "
      "<= '1' RETURN d");
  assertExpressionFilter(
      vocbase(),
      "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
      "collection FILTER '1' <= "
      "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_NONDETERM_('a')] "
      "RETURN d");
  assertExpressionFilter(
      vocbase(), "FOR d IN collection FILTER d.a <= _NONDETERM_('1') RETURN d");
  assertExpressionFilter(
      vocbase(),
      "LET k={} FOR d IN collection FILTER k.a <= _NONDETERM_('1') RETURN d");

  // unsupported expression (d referenced inside) -> wrap it
  assertExpressionFilter(
      vocbase(), "FOR d IN collection FILTER 3 <= (2 <= d.a.b.c) RETURN d");
  assertExpressionFilter(
      vocbase(),
      "FOR d IN collection FILTER 3 <= boost(2 <= d.a.b.c, 1.5) RETURN d");
  assertExpressionFilter(
      vocbase(),
      "FOR d IN collection FILTER analyzer(3, 'test_analyzer') <= (2 <= "
      "d.a.b.c) RETURN d");

  // expression is not supported by IResearch -> wrap it
  assertExpressionFilter(vocbase(),
                         "FOR d IN collection FILTER d <= '1' RETURN d");
  assertExpressionFilter(vocbase(),
                         "FOR d IN collection FILTER d[*] <= '1' RETURN d");
  assertExpressionFilter(vocbase(),
                         "FOR d IN collection FILTER d.a[*] <= '1' RETURN d");
  assertExpressionFilter(vocbase(),
                         "FOR d IN collection FILTER '1' >= d RETURN d");
  assertExpressionFilter(
      vocbase(),
      "FOR d IN collection FILTER 2 <= d.a.b.c.numeric <= 3 RETURN d");
  assertExpressionFilter(
      vocbase(),
      "FOR d IN collection FILTER d.a.b.c.numeric <= 2 <= 3 RETURN d");
  assertExpressionFilter(
      vocbase(),
      "FOR d IN collection FILTER d.a.b.c.numeric <= 2 <= 3 RETURN d");

  // expression with self-reference is not supported by IResearch -> wrap it
  assertExpressionFilter(vocbase(),
                         "FOR d IN collection FILTER d.a <= 1+d.b RETURN d");

  // unsupported node types
  assertFilterFail(vocbase(), "FOR d IN collection FILTER d.a <= {} RETURN d");
  assertFilterFail(vocbase(), "FOR d IN collection FILTER {} >= d.a RETURN d");
  assertFilterExecutionFail(vocbase(),
                            "FOR d IN collection FILTER d.a <= 1..2 RETURN d",
                            &ExpressionContextMock::EMPTY);
  assertFilterExecutionFail(vocbase(),
                            "FOR d IN collection FILTER 1..2 >= d.a RETURN d",
                            &ExpressionContextMock::EMPTY);
}

TEST_F(IResearchFilterCompareTest, BinaryLT) {
  // simple string attribute
  {
    irs::Or expected;
    {
      auto& filter = expected.add<irs::by_range>();
      *filter.mutable_field() = mangleStringIdentity("a");
      auto* opts = filter.mutable_options();
      opts->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("1"));
      opts->range.max_type = irs::BoundType::EXCLUSIVE;
    }

    assertFilterSuccess(vocbase(),
                        "FOR d IN collection FILTER d.a < '1' RETURN d", expected);
    assertFilterSuccess(vocbase(),
                        "FOR d IN collection FILTER d['a'] < '1' RETURN d", expected);
    assertFilterSuccess(vocbase(),
                        "FOR d IN collection FILTER '1' > d.a RETURN d", expected);
    assertFilterSuccess(vocbase(),
                        "FOR d IN collection FILTER '1' > d['a'] RETURN d", expected);
  }

  // simple offset
  {
    irs::Or expected;
    {
      auto& filter = expected.add<irs::by_range>();
      *filter.mutable_field() = mangleStringIdentity("[42]");
      auto* opts = filter.mutable_options();
      opts->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("1"));
      opts->range.max_type = irs::BoundType::EXCLUSIVE;
    }

    assertFilterSuccess(vocbase(),
                        "FOR d IN collection FILTER d[42] < '1' RETURN d", expected);
    assertFilterSuccess(vocbase(),
                        "FOR d IN collection FILTER '1' > d[42] RETURN d", expected);
  }

  // complex attribute name, string
  {
    irs::Or expected;
    {
      auto& filter = expected.add<irs::by_range>();
      *filter.mutable_field() = mangleStringIdentity("a.b.c");
      auto* opts = filter.mutable_options();
      opts->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("1"));
      opts->range.max_type = irs::BoundType::EXCLUSIVE;
    }

    assertFilterSuccess(vocbase(),
                        "FOR d IN collection FILTER d.a.b.c < '1' RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER d.a['b']['c'] < '1' RETURN d", expected);
    assertFilterSuccess(vocbase(),
                        "FOR d IN collection FILTER '1' > d.a.b.c RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER '1' > d['a']['b']['c'] RETURN d", expected);
  }

  // complex attribute name with offset, string
  {
    irs::Or expected;
    {
      auto& filter = expected.add<irs::by_range>();
      *filter.mutable_field() = mangleStringIdentity("a.b[42].c");
      auto* opts = filter.mutable_options();
      opts->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("1"));
      opts->range.max_type = irs::BoundType::EXCLUSIVE;
    }

    assertFilterSuccess(vocbase(),
                        "FOR d IN collection FILTER d.a.b[42].c < '1' RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER d.a['b'][42]['c'] < '1' RETURN d", expected);
    assertFilterSuccess(vocbase(),
                        "FOR d IN collection FILTER '1' > d.a.b[42].c RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER '1' > d['a']['b'][42]['c'] RETURN d", expected);
  }

  // string expression
  {
    arangodb::aql::Variable var("c", 0, /*isDataFromColl*/ false);
    arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt{41});
    arangodb::aql::AqlValueGuard guard(value, true);

    ExpressionContextMock ctx;
    ctx.vars.emplace(var.name, value);

    irs::Or expected;
    {
      auto& filter = expected.add<irs::by_range>();
      *filter.mutable_field() = mangleStringIdentity("a.b[23].c");
      auto* opts = filter.mutable_options();
      opts->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("42"));
      opts->range.max_type = irs::BoundType::EXCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER d.a.b[23].c < TO_STRING(c+1) "
        "RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER d.a['b'][23]['c'] < "
        "TO_STRING(c+1) RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER TO_STRING(c+1) > d.a.b[23].c "
        "RETURN d",
        expected, &ctx);
    assertFilterSuccess(vocbase(),
                        "LET c=41 FOR d IN collection FILTER TO_STRING(c+1) > "
                        "d['a']['b'][23].c RETURN d",
                        expected, &ctx);
  }

  // string expression, analyzer, boost
  {
    arangodb::aql::Variable var("c", 0, /*isDataFromColl*/ false);
    arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt{41});
    arangodb::aql::AqlValueGuard guard(value, true);

    ExpressionContextMock ctx;
    ctx.vars.emplace(var.name, value);

    irs::Or expected;
    {
      auto& filter = expected.add<irs::by_range>();
      filter.boost(42);
      *filter.mutable_field() = mangleString("a.b[23].c", "test_analyzer");
      auto* opts = filter.mutable_options();
      opts->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("42"));
      opts->range.max_type = irs::BoundType::EXCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER boost(analyzer(d.a.b[23].c < "
        "TO_STRING(c+1), 'test_analyzer'), c+1) RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER analyzer(boost(d.a['b'][23]['c'] "
        "< TO_STRING(c+1),c+1), 'test_analyzer') RETURN d",
        expected, &ctx);
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
    {
      auto& filter = expected.add<irs::by_range>();
      *filter.mutable_field() = mangleStringIdentity("a.b.c.e[4].f[5].g[3].g.a");
      auto* opts = filter.mutable_options();
      opts->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("42"));
      opts->range.max_type = irs::BoundType::EXCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(),
        "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "< '42' RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER '42' > "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "RETURN d",
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
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "<= '42' RETURN d",
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
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "<= '42' RETURN d",
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
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "<= '42' RETURN d",
        &ctx);
  }

  // complex boolean attribute, true
  {
    irs::Or expected;
    {
      auto& filter = expected.add<irs::by_range>();
      *filter.mutable_field() = mangleBool("a.b.c");
      auto* opts = filter.mutable_options();
      opts->range.max = irs::ref_cast<irs::byte_type>(irs::boolean_token_stream::value_true());
      opts->range.max_type = irs::BoundType::EXCLUSIVE;
    }

    assertFilterSuccess(vocbase(),
                        "FOR d IN collection FILTER d.a.b.c < true RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER d['a']['b']['c'] < true RETURN d", expected);
    assertFilterSuccess(vocbase(),
                        "FOR d IN collection FILTER true > d.a.b.c RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER true > d['a']['b']['c'] RETURN d", expected);
  }

  // complex boolean attribute, false
  {
    irs::Or expected;
    {
      auto& filter = expected.add<irs::by_range>();
      *filter.mutable_field() = mangleBool("a.b.c.bool");
      auto* opts = filter.mutable_options();
      opts->range.max = irs::ref_cast<irs::byte_type>(irs::boolean_token_stream::value_false());
      opts->range.max_type = irs::BoundType::EXCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER d.a.b.c.bool < false RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a['b']['c'].bool < false RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER false > d.a.b.c.bool RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER false > d['a'].b.c.bool RETURN d", expected);
  }

  // complex boolean attribute with offset, false
  {
    irs::Or expected;
    {
      auto& filter = expected.add<irs::by_range>();
      *filter.mutable_field() = mangleBool("a.b.c[42].bool[42]");
      auto* opts = filter.mutable_options();
      opts->range.max = irs::ref_cast<irs::byte_type>(irs::boolean_token_stream::value_false());
      opts->range.max_type = irs::BoundType::EXCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a.b.c[42].bool[42] < false RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a['b']['c'][42].bool[42] < false RETURN "
        "d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER false > d.a.b.c[42].bool[42] RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER false > d['a'].b.c[42].bool[42] RETURN d", expected);
  }

  // boolean expression
  {
    arangodb::aql::Variable var("c", 0, /*isDataFromColl*/ false);
    arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt{41});
    arangodb::aql::AqlValueGuard guard(value, true);

    ExpressionContextMock ctx;
    ctx.vars.emplace(var.name, value);

    irs::Or expected;
    {
      auto& filter = expected.add<irs::by_range>();
      *filter.mutable_field() = mangleBool("a.b[23].c");
      auto* opts = filter.mutable_options();
      opts->range.max = irs::ref_cast<irs::byte_type>(irs::boolean_token_stream::value_false());
      opts->range.max_type = irs::BoundType::EXCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER d.a.b[23].c < TO_BOOL(c-41) "
        "RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER d.a['b'][23]['c'] < TO_BOOL(c-41) "
        "RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER TO_BOOL(c-41) > d.a.b[23].c "
        "RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER TO_BOOL(c-41) > d['a']['b'][23].c "
        "RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER analyzer(TO_BOOL(c-41) > "
        "d['a']['b'][23].c, 'test_analyzer') RETURN d",
        expected, &ctx);
  }

  // boolean expression
  {
    arangodb::aql::Variable var("c", 0, /*isDataFromColl*/ false);
    arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt{41});
    arangodb::aql::AqlValueGuard guard(value, true);

    ExpressionContextMock ctx;
    ctx.vars.emplace(var.name, value);

    irs::Or expected;
    {
      auto& filter = expected.add<irs::by_range>();
      filter.boost(42);
      *filter.mutable_field() = mangleBool("a.b[23].c");
      auto* opts = filter.mutable_options();
      opts->range.max = irs::ref_cast<irs::byte_type>(irs::boolean_token_stream::value_false());
      opts->range.max_type = irs::BoundType::EXCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER boost(d.a.b[23].c < "
        "TO_BOOL(c-41),c+1) RETURN d",
        expected, &ctx);
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
    {
      auto& filter = expected.add<irs::by_range>();
      *filter.mutable_field() = mangleBool("a.b.c.e[4].f[5].g[3].g.a");
      auto* opts = filter.mutable_options();
      opts->range.max = irs::ref_cast<irs::byte_type>(irs::boolean_token_stream::value_false());
      opts->range.max_type = irs::BoundType::EXCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(),
        "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "< false RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER false > "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "RETURN d",
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
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "< false RETURN d",
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
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "< false RETURN d",
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
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "< false RETURN d",
        &ctx);
  }

  // complex null attribute
  {
    irs::Or expected;
    {
      auto& filter = expected.add<irs::by_range>();
      *filter.mutable_field() = mangleNull("a.b.c.nil");
      auto* opts = filter.mutable_options();
      opts->range.max = irs::ref_cast<irs::byte_type>(irs::null_token_stream::value_null());
      opts->range.max_type = irs::BoundType::EXCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER d.a.b.c.nil < null RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a['b']['c'].nil < null RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER null > d.a.b.c.nil RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER null > d['a'].b.c.nil RETURN d", expected);
  }

  // complex null attribute with offset
  {
    irs::Or expected;
    {
      auto& filter = expected.add<irs::by_range>();
      *filter.mutable_field() = mangleNull("a.b[42].c.nil");
      auto* opts = filter.mutable_options();
      opts->range.max = irs::ref_cast<irs::byte_type>(irs::null_token_stream::value_null());
      opts->range.max_type = irs::BoundType::EXCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER d.a.b[42].c.nil < null RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a['b'][42]['c'].nil < null RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER null > d.a.b[42].c.nil RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER null > d['a'].b[42].c.nil RETURN d", expected);
  }

  // null expression
  {
    arangodb::aql::Variable var("c", 0, /*isDataFromColl*/ false);
    arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintNull{});
    arangodb::aql::AqlValueGuard guard(value, true);

    ExpressionContextMock ctx;
    ctx.vars.emplace(var.name, value);

    irs::Or expected;
    {
      auto& filter = expected.add<irs::by_range>();
      *filter.mutable_field() = mangleNull("a.b[23].c");
      auto* opts = filter.mutable_options();
      opts->range.max = irs::ref_cast<irs::byte_type>(irs::null_token_stream::value_null());
      opts->range.max_type = irs::BoundType::EXCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(),
        "LET c=null FOR d IN collection FILTER d.a.b[23].c < (c && false) "
        "RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=null FOR d IN collection FILTER d.a['b'][23]['c'] < (c && true) "
        "RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=null FOR d IN collection FILTER (c && false) > d.a.b[23].c "
        "RETURN d",
        expected, &ctx);
    assertFilterSuccess(vocbase(),
                        "LET c=null FOR d IN collection FILTER (c && false) > "
                        "d['a']['b'][23].c RETURN d",
                        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=null FOR d IN collection FILTER analyzer((c && false) > "
        "d['a']['b'][23].c, 'test_analyzer') RETURN d",
        expected, &ctx);
  }

  // null expression, boost
  {
    arangodb::aql::Variable var("c", 0, /*isDataFromColl*/ false);
    arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintNull{});
    arangodb::aql::AqlValueGuard guard(value, true);

    ExpressionContextMock ctx;
    ctx.vars.emplace(var.name, value);

    irs::Or expected;
    {
      auto& filter = expected.add<irs::by_range>();
      filter.boost(42);
      *filter.mutable_field() = mangleNull("a.b[23].c");
      auto* opts = filter.mutable_options();
      opts->range.max = irs::ref_cast<irs::byte_type>(irs::null_token_stream::value_null());
      opts->range.max_type = irs::BoundType::EXCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(),
        "LET c=null FOR d IN collection FILTER boost(d.a.b[23].c < (c && "
        "false), c+42) RETURN d",
        expected, &ctx);
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
    {
      auto& filter = expected.add<irs::by_range>();
      *filter.mutable_field() = mangleNull("a.b.c.e[4].f[5].g[3].g.a");
      auto* opts = filter.mutable_options();
      opts->range.max = irs::ref_cast<irs::byte_type>(irs::null_token_stream::value_null());
      opts->range.max_type = irs::BoundType::EXCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(),
        "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "< null RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER null > "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "RETURN d",
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
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "< null RETURN d",
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
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "< null RETURN d",
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
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "< null RETURN d",
        &ctx);
  }

  // complex boolean attribute, numeric
  {
    irs::numeric_token_stream stream;
    stream.reset(13.);

    irs::Or expected;
    {
      auto& filter = expected.add<irs::by_granular_range>();
      *filter.mutable_field() = mangleNumeric("a.b.c.numeric");
      auto* opts = filter.mutable_options();
      irs::set_granular_term(opts->range.max, stream);
      opts->range.max_type = irs::BoundType::EXCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER d.a.b.c.numeric < 13 RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a['b']['c'].numeric < 13 RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER d.a.b.c.numeric < 13.0 RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER 13 > d.a.b.c.numeric RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER 13.0 > d.a.b.c.numeric RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER 13.0 > d['a']['b']['c'].numeric RETURN d", expected);
  }

  // complex boolean attribute, numeric
  {
    irs::numeric_token_stream stream;
    stream.reset(13.);

    irs::Or expected;
    {
      auto& filter = expected.add<irs::by_granular_range>();
      *filter.mutable_field() = mangleNumeric("a[1].b[42].c.numeric");
      auto* opts = filter.mutable_options();
      irs::set_granular_term(opts->range.max, stream);
      opts->range.max_type = irs::BoundType::EXCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a[1].b[42].c.numeric < 13 RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a[1]['b'][42]['c'].numeric < 13 RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a[1].b[42].c.numeric < 13.0 RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER 13 > d.a[1].b[42].c.numeric RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER 13.0 > d.a[1].b[42].c.numeric RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER 13.0 > d['a'][1]['b'][42]['c'].numeric "
        "RETURN d",
        expected);
  }

  // numeric expression
  {
    arangodb::aql::Variable var("c", 0, /*isDataFromColl*/ false);
    arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt{41});
    arangodb::aql::AqlValueGuard guard(value, true);

    ExpressionContextMock ctx;
    ctx.vars.emplace(var.name, value);

    irs::numeric_token_stream stream;
    stream.reset(42.5);

    irs::Or expected;
    {
      auto& filter = expected.add<irs::by_granular_range>();
      *filter.mutable_field() = mangleNumeric("a.b[23].c");
      auto* opts = filter.mutable_options();
      irs::set_granular_term(opts->range.max, stream);
      opts->range.max_type = irs::BoundType::EXCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER d.a.b[23].c < (c+1.5) RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER d.a['b'][23]['c'] < (c+1.5) "
        "RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER (c+1.5) > d.a.b[23].c RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER (c+1.5) > d['a']['b'][23].c "
        "RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER analyzer((c+1.5) > "
        "d['a']['b'][23].c, 'test_analyzer') RETURN d",
        expected, &ctx);
  }

  // numeric expression, boost
  {
    arangodb::aql::Variable var("c", 0, /*isDataFromColl*/ false);
    arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt{41});
    arangodb::aql::AqlValueGuard guard(value, true);

    ExpressionContextMock ctx;
    ctx.vars.emplace(var.name, value);

    irs::numeric_token_stream stream;
    stream.reset(42.5);

    irs::Or expected;
    {
      auto& filter = expected.add<irs::by_granular_range>();
      filter.boost(42);
      *filter.mutable_field() = mangleNumeric("a.b[23].c");
      auto* opts = filter.mutable_options();
      irs::set_granular_term(opts->range.max, stream);
      opts->range.max_type = irs::BoundType::EXCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER boost(d.a.b[23].c < (c+1.5), c+1) "
        "RETURN d",
        expected, &ctx);
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

    irs::numeric_token_stream stream;
    stream.reset(42.5);

    irs::Or expected;
    {
      auto& filter = expected.add<irs::by_granular_range>();
      *filter.mutable_field() = mangleNumeric("a.b.c.e[4].f[5].g[3].g.a");
      auto* opts = filter.mutable_options();
      irs::set_granular_term(opts->range.max, stream);
      opts->range.max_type = irs::BoundType::EXCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(),
        "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "< 42.5 RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER 42.5 > "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "RETURN d",
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
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "< 42.5 RETURN d",
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
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "< 42.5 RETURN d",
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
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "< 42.5 RETURN d",
        &ctx);
  }

  // complex expression
  {
    irs::Or expected;
    {
      auto& filter = expected.add<irs::by_range>();
      *filter.mutable_field() = mangleBool("a.b.c");
      auto* opts = filter.mutable_options();
      opts->range.min = irs::ref_cast<irs::byte_type>(irs::boolean_token_stream::value_false());
      opts->range.min_type = irs::BoundType::EXCLUSIVE;
    }


    assertFilterSuccess(vocbase(),
                        "FOR d IN collection FILTER 3 < 2 < d.a.b.c RETURN d",
                        expected, &ExpressionContextMock::EMPTY);
  }

  // expression without reference to loop variable, unreachable criteria
  {
    auto obj = arangodb::velocypack::Parser::fromJson("{}");

    ExpressionContextMock ctx;
    ctx.vars.emplace("k", arangodb::aql::AqlValue(obj->slice()));

    irs::Or expected;
    expected.add<irs::all>();

    assertFilterSuccess(
        vocbase(), "LET k={} FOR d IN collection FILTER k.a < '1' RETURN d", expected, &ctx);
  }

  // array in expression
  {
    irs::Or expected;
    expected.add<irs::empty>();

    assertFilterSuccess(vocbase(),
                        "FOR d IN collection FILTER [] < '1' RETURN d",
                        expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess(vocbase(),
                        "FOR d IN collection FILTER ['d'] < '1' RETURN d",
                        expected, &ExpressionContextMock::EMPTY);
  }

  // nondeterministic expression -> wrap it
  assertExpressionFilter(
      vocbase(),
      "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
      "collection FILTER "
      "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_NONDETERM_('a')] "
      "< '1' RETURN d");
  assertExpressionFilter(
      vocbase(),
      "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
      "collection FILTER '1' < "
      "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_NONDETERM_('a')] "
      "RETURN d");
  assertExpressionFilter(
      vocbase(), "FOR d IN collection FILTER d.a < _NONDETERM_('1') RETURN d");
  assertExpressionFilter(
      vocbase(),
      "LET k={} FOR d IN collection FILTER k.a < _NONDETERM_('1') RETURN d");

  // unsupported expression (d referenced inside) -> wrap it
  assertExpressionFilter(
      vocbase(), "FOR d IN collection FILTER 3 < (2 < d.a.b.c) RETURN d");
  assertExpressionFilter(
      vocbase(),
      "FOR d IN collection FILTER 3 < boost(2 < d.a.b.c, 1.5) RETURN d");
  assertExpressionFilter(
      vocbase(),
      "FOR d IN collection FILTER analyzer(3, 'test_analyzer') < boost(2 < "
      "d.a.b.c, 1.5) RETURN d");

  // expression is not supported by IResearch -> wrap it
  assertExpressionFilter(vocbase(),
                         "FOR d IN collection FILTER d < '1' RETURN d");
  assertExpressionFilter(vocbase(),
                         "FOR d IN collection FILTER d[*] < '1' RETURN d");
  assertExpressionFilter(vocbase(),
                         "FOR d IN collection FILTER d.a[*] < '1' RETURN d");
  assertExpressionFilter(vocbase(),
                         "FOR d IN collection FILTER '1' > d RETURN d");
  assertExpressionFilter(
      vocbase(), "FOR d IN collection FILTER 2 < d.a.b.c.numeric < 3 RETURN d");
  assertExpressionFilter(
      vocbase(), "FOR d IN collection FILTER d.a.b.c.numeric < 2 < 3 RETURN d");
  assertExpressionFilter(
      vocbase(), "FOR d IN collection FILTER d.a.b.c.numeric < 2 < 3 RETURN d");

  // expression with self-reference is not supported by IResearch -> wrap it
  assertExpressionFilter(vocbase(),
                         "FOR d IN collection FILTER d.a < 1+d.b RETURN d");

  // unsupported node types
  assertFilterFail(vocbase(), "FOR d IN collection FILTER d.a < {} RETURN d");
  assertFilterFail(vocbase(), "FOR d IN collection FILTER {} > d.a RETURN d");
  assertFilterExecutionFail(vocbase(),
                            "FOR d IN collection FILTER d.a < 1..2 RETURN d",
                            &ExpressionContextMock::EMPTY);
  assertFilterExecutionFail(vocbase(),
                            "FOR d IN collection FILTER 1..2 > d.a RETURN d",
                            &ExpressionContextMock::EMPTY);
}
