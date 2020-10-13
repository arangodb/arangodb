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

class IResearchFilterBooleanTest
    : public ::testing::Test,
      public arangodb::tests::LogSuppressor<arangodb::Logger::AUTHENTICATION, arangodb::LogLevel::ERR> {
 protected:
  arangodb::tests::mocks::MockAqlServer server;

 private:
  TRI_vocbase_t* _vocbase;

 protected:
  IResearchFilterBooleanTest() {
    arangodb::tests::init();

    auto& functions = server.getFeature<arangodb::aql::AqlFunctionFeature>();

    // register fake non-deterministic function in order to suppress optimizations
    functions.add(arangodb::aql::Function{
        "_NONDETERM_", ".",
        arangodb::aql::Function::makeFlags(
            // fake non-deterministic
            arangodb::aql::Function::Flags::CanRunOnDBServer),
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
            arangodb::aql::Function::Flags::CanRunOnDBServer),
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
      arangodb::velocypack::Parser::fromJson("{ \"args\": \"abc\" }")->slice()); // cache analyzer
  }

  TRI_vocbase_t& vocbase() { return *_vocbase; }
};  // IResearchFilterSetup

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

TEST_F(IResearchFilterBooleanTest, Ternary) {

//  arangodb::aql::Query q(arangodb::transaction::StandaloneContext::Create(this->vocbase()),
//                         arangodb::aql::QueryString(), nullptr, nullptr);
//  
  // can evaluate expression
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("x", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                              arangodb::aql::AqlValueHintInt{3})));

    irs::Or expected;
    expected.add<irs::all>();

    assertFilterSuccess(
        vocbase(),
        "LET x=3 FOR d IN collection FILTER x > 2 ? true : false RETURN d",
        expected, &ctx);
  }

  // can evaluate expression, boost
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("x", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                              arangodb::aql::AqlValueHintInt{3})));

    irs::Or expected;
    expected.add<irs::all>().boost(1.5);

    assertFilterSuccess(
        vocbase(),
        "LET x=3 FOR d IN collection FILTER BOOST(x > 2 ? true : false, 1.5) "
        "RETURN d",
        expected, &ctx);
  }

  // can evaluate expression
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("x", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                              arangodb::aql::AqlValueHintInt{1})));

    irs::Or expected;
    expected.add<irs::empty>();

    assertFilterSuccess(
        vocbase(),
        "LET x=1 FOR d IN collection FILTER x > 2 ? true : false RETURN d",
        expected, &ctx);
  }

  // nondeterministic expression -> wrap it
  assertExpressionFilter(
      vocbase(),
      "LET x=1 FOR d IN collection FILTER x > 2 ? _NONDETERM_(true) : false "
      "RETURN d");
  assertExpressionFilter(
      vocbase(),
      "LET x=1 FOR d IN collection FILTER BOOST(x > 2 ? _NONDETERM_(true) : "
      "false, 1.5) RETURN d",
      1.5, wrappedExpressionExtractor);

  // can't evaluate expression: no referenced variable in context
  assertFilterExecutionFail(
      vocbase(),
      "LET x=1 FOR d IN collection FILTER x > 2 ? true : false RETURN d",
      &ExpressionContextMock::EMPTY);
}

TEST_F(IResearchFilterBooleanTest, UnaryNot) {
  // simple attribute, string
  {
    irs::Or expected;
    auto& filter = expected.add<irs::Not>().filter<irs::And>().add<irs::by_term>();
    *filter.mutable_field() = mangleStringIdentity("a");
    filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("1"));

    assertFilterSuccess(vocbase(),
                        "FOR d IN collection FILTER not (d.a == '1') RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER not (d['a'] == '1') RETURN d", expected);
    assertFilterSuccess(vocbase(),
                        "FOR d IN collection FILTER not ('1' == d.a) RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER not ('1' == d['a']) RETURN d", expected);
  }

  // simple offset, string
  {
    irs::Or expected;
    auto& filter = expected.add<irs::Not>().filter<irs::And>().add<irs::by_term>();
    *filter.mutable_field() = mangleStringIdentity("[1]");
    filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("1"));

    assertFilterSuccess(vocbase(),
                        "FOR d IN collection FILTER not (d[1] == '1') RETURN d", expected);
    assertFilterSuccess(vocbase(),
                        "FOR d IN collection FILTER not ('1' == d[1]) RETURN d", expected);
  }

  // complex attribute, string
  {
    irs::Or expected;
    auto& filter = expected.add<irs::Not>().filter<irs::And>().add<irs::by_term>();
    *filter.mutable_field() = mangleStringIdentity("a.b.c");
    filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("1"));

    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER not (d.a.b.c == '1') RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER not (d['a']['b']['c'] == '1') RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER not ('1' == d.a.b.c) RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER not ('1' == d['a']['b']['c']) RETURN d", expected);
  }

  // complex attribute with offset, string
  {
    irs::Or expected;
    auto& filter = expected.add<irs::Not>().filter<irs::And>().add<irs::by_term>();
    *filter.mutable_field() = mangleStringIdentity("a.b[42].c");
    filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("1"));

    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER not (d.a.b[42].c == '1') RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER not (d['a']['b'][42]['c'] == '1') RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER not ('1' == d.a.b[42].c) RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER not ('1' == d['a']['b'][42]['c']) RETURN d", expected);
  }

  // complex attribute with offset, string, boost
  {
    irs::Or expected;
    auto& root = expected.add<irs::Not>();
    root.boost(2.5);
    auto& filter = root.filter<irs::And>().add<irs::by_term>();
    *filter.mutable_field() = mangleStringIdentity("a.b[42].c");
    filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("1"));

    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER BOOST(not (d.a.b[42].c == '1'), 2.5) "
        "RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER boost(not (d['a']['b'][42]['c'] == '1'), "
        "2.5) RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER boost(not ('1' == d.a.b[42].c), 2.5) "
        "RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER boost(not ('1' == d['a']['b'][42]['c']), "
        "2.5) RETURN d",
        expected);
  }

  // complex attribute with offset, string, boost
  {
    irs::Or expected;
    auto& root = expected.add<irs::Not>().filter<irs::And>();
    auto& filter = root.add<irs::by_term>();
    filter.boost(2.5);
    *filter.mutable_field() = mangleStringIdentity("a.b[42].c");
    filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("1"));

    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER not boost('1' == d['a']['b'][42]['c'], "
        "2.5) RETURN d",
        expected);
  }

  // complex attribute with offset, string, boost, analyzer
  {
    irs::Or expected;
    auto& root = expected.add<irs::Not>();
    root.boost(2.5);
    auto& filter = root.filter<irs::And>().add<irs::by_term>();
    *filter.mutable_field() = mangleString("a.b[42].c", "test_analyzer");
    filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("1"));

    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER analyzer(BOOST(not (d.a.b[42].c == '1'), "
        "2.5), 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER analyzer(boost(not (d['a']['b'][42]['c'] "
        "== '1'), 2.5), 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER boost(analyzer(not ('1' == d.a.b[42].c), "
        "'test_analyzer'), 2.5) RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER boost(analyzer(not ('1' == "
        "d['a']['b'][42]['c']), 'test_analyzer'), 2.5) RETURN d",
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
    auto& filter = expected.add<irs::Not>().filter<irs::And>().add<irs::by_term>();
    *filter.mutable_field() = mangleStringIdentity("a.b[23].c");
    filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("42"));

    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER not (d.a.b[23].c == "
        "TO_STRING(c+1)) RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER not (d.a['b'][23].c == "
        "TO_STRING(c+1)) RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER not (d['a']['b'][23].c == "
        "TO_STRING(c+1)) RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER not (TO_STRING(c+1) == "
        "d.a.b[23].c) RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER not (TO_STRING(c+1) == "
        "d.a['b'][23].c) RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER not (TO_STRING(c+1) == "
        "d['a']['b'][23]['c']) RETURN d",
        expected, &ctx);
  }

  // string expression, analyzer
  {
    arangodb::aql::Variable var("c", 0, /*isDataFromColl*/ false);
    arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt{41});
    arangodb::aql::AqlValueGuard guard(value, true);

    ExpressionContextMock ctx;
    ctx.vars.emplace(var.name, value);

    irs::Or expected;
    auto& filter = expected.add<irs::Not>().filter<irs::And>().add<irs::by_term>();
    *filter.mutable_field() = mangleString("a.b[23].c", "test_analyzer");
    filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("42"));

    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER ANALYZER(not (d.a.b[23].c == "
        "TO_STRING(c+1)), 'test_analyzer') RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER ANALYZER(not (d.a['b'][23].c == "
        "TO_STRING(c+1)), 'test_analyzer') RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER ANALYZER(not (d['a']['b'][23].c "
        "== TO_STRING(c+1)), 'test_analyzer') RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER ANALYZER(not (TO_STRING(c+1) == "
        "d.a.b[23].c), 'test_analyzer') RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER ANALYZER(not (TO_STRING(c+1) == "
        "d.a['b'][23].c), 'test_analyzer') RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER ANALYZER(not (TO_STRING(c+1) == "
        "d['a']['b'][23]['c']), 'test_analyzer') RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER not ANALYZER(TO_STRING(c+1) == "
        "d['a']['b'][23]['c'], 'test_analyzer') RETURN d",
        expected, &ctx);
  }
  // filter with constexpr analyzer
  {
    arangodb::aql::Variable var("c", 0, /*isDataFromColl*/ false);
    arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt{41});
    arangodb::aql::AqlValueGuard guard(value, true);

    ExpressionContextMock ctx;
    ctx.vars.emplace(var.name, value);
    irs::Or expected;
    auto& filter = expected.add<irs::Not>().filter<irs::And>().add<irs::by_term>();
    *filter.mutable_field() = mangleStringIdentity("a.b[23].c");
    filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("42"));

    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER not (ANALYZER(TO_STRING(c+1), "
        "'test_analyzer') == d['a']['b'][23]['c']) RETURN d",
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
    auto& filter = expected.add<irs::Not>().filter<irs::And>().add<irs::by_term>();
    *filter.mutable_field() = mangleStringIdentity("a.b.c.e[4].f[5].g[3].g.a");
    filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("1"));

    assertFilterSuccess(
        vocbase(),
        "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER not "
        "(d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')]"
        " == '1') RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER not ('1' == "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')])"
        " RETURN d",
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
        "collection FILTER not "
        "(d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')]"
        " == '1') RETURN d",
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
        "collection FILTER not "
        "(d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')]"
        " == '1') RETURN d",
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
        "collection FILTER not "
        "(d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')]"
        " == '1') RETURN d",
        &ctx);
  }

  // complex attribute, true
  {
    irs::Or expected;
    auto& filter = expected.add<irs::Not>().filter<irs::And>().add<irs::by_term>();
    *filter.mutable_field() = mangleBool("a.b.c");
    filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::boolean_token_stream::value_true());

    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER not (d.a.b.c == true) RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER not (d['a'].b.c == true) RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER not (true == d.a.b.c) RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER not (true == d.a['b']['c']) RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER analyzer(not (d.a.b.c == true), "
        "'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER not analyzer(d['a'].b.c == true, "
        "'identity') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER not analyzer(true == d.a.b.c, "
        "'test_analyzer') RETURN d",
        expected);
  }

  // complex attribute, false
  {
    irs::Or expected;
    auto& filter = expected.add<irs::Not>().filter<irs::And>().add<irs::by_term>();
    *filter.mutable_field() = mangleBool("a.b.c.bool");
    filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::boolean_token_stream::value_false());

    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER not (d.a.b.c.bool == false) RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER not (d['a'].b.c.bool == false) RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER not (false == d.a.b.c.bool) RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER not (false == d.a['b']['c'].bool) RETURN d", expected);
  }

  // complex attribute with offset, false
  {
    irs::Or expected;
    auto& filter = expected.add<irs::Not>().filter<irs::And>().add<irs::by_term>();
    *filter.mutable_field() = mangleBool("a[1].b.c.bool");
    filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::boolean_token_stream::value_false());

    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER not (d.a[1].b.c.bool == false) RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER not (d['a'][1].b.c.bool == false) RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER not (false == d.a[1].b.c.bool) RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER not (false == d.a[1]['b']['c'].bool) "
        "RETURN d",
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
    auto& filter = expected.add<irs::Not>().filter<irs::And>().add<irs::by_term>();
    *filter.mutable_field() = mangleBool("a.b[23].c");
    filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::boolean_token_stream::value_false());

    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER not (d.a.b[23].c == "
        "TO_BOOL(c-41)) RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER not (d.a['b'][23].c == "
        "TO_BOOL(c-41)) RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER not (d['a']['b'][23].c == "
        "TO_BOOL(c-41)) RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER not (TO_BOOL(c-41) == "
        "d.a.b[23].c) RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER not (TO_BOOL(c-41) == "
        "d.a['b'][23].c) RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER not (TO_BOOL(c-41) == "
        "d['a']['b'][23]['c']) RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER not analyzer((TO_BOOL(c-41) == "
        "d.a['b'][23].c), 'test_analyzer') RETURN d",
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
    auto& filter = expected.add<irs::Not>().filter<irs::And>().add<irs::by_term>();
    *filter.mutable_field() = mangleBool("a.b.c.e[4].f[5].g[3].g.a");
    filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::boolean_token_stream::value_true());

    assertFilterSuccess(
        vocbase(),
        "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER not "
        "(d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')]"
        " == true) RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER not (true == "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')])"
        " RETURN d",
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
        "collection FILTER not "
        "(d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')]"
        " == true) RETURN d",
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
        "collection FILTER not "
        "(d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')]"
        " == true) RETURN d",
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
        "collection FILTER not "
        "(d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')]"
        " == true) RETURN d",
        &ctx);
  }

  // complex attribute, null
  {
    irs::Or expected;
    auto& filter = expected.add<irs::Not>().filter<irs::And>().add<irs::by_term>();
    *filter.mutable_field() = mangleNull("a.b.c.bool");
    filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::null_token_stream::value_null());

    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER not (d.a.b.c.bool == null) RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER not (d.a['b']['c'].bool == null) RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER not (null == d.a.b.c.bool) RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER not (null == d['a']['b']['c'].bool) RETURN "
        "d",
        expected);
  }

  // complex attribute, null
  {
    irs::Or expected;
    auto& filter = expected.add<irs::Not>().filter<irs::And>().add<irs::by_term>();
    *filter.mutable_field() = mangleNull("a.b.c.bool[42]");
    filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::null_token_stream::value_null());

    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER not (d.a.b.c.bool[42] == null) RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER not (d.a['b']['c'].bool[42] == null) "
        "RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER not (null == d.a.b.c.bool[42]) RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER not (null == d['a']['b']['c'].bool[42]) "
        "RETURN d",
        expected);
  }

  // null expression
  {
    arangodb::aql::Variable var("c", 0, /*isDataFromColl*/ false);
    arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintNull{});
    arangodb::aql::AqlValueGuard guard(value, true);

    ExpressionContextMock ctx;
    ctx.vars.emplace(var.name, value);

    irs::Or expected;
    auto& filter = expected.add<irs::Not>().filter<irs::And>().add<irs::by_term>();
    *filter.mutable_field() = mangleNull("a.b[23].c");
    filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::null_token_stream::value_null());

    assertFilterSuccess(
        vocbase(),
        "LET c=null FOR d IN collection FILTER not (d.a.b[23].c == (c && "
        "true)) RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=null FOR d IN collection FILTER not (d.a['b'][23].c == (c && "
        "false)) RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=null FOR d IN collection FILTER not (d['a']['b'][23].c == (c && "
        "true)) RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=null FOR d IN collection FILTER not ((c && false) == "
        "d.a.b[23].c) RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=null FOR d IN collection FILTER not ((c && false) == "
        "d.a['b'][23].c) RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=null FOR d IN collection FILTER not ((c && false) == "
        "d['a']['b'][23]['c']) RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=null FOR d IN collection FILTER not analyzer((c && false) == "
        "d['a']['b'][23]['c'], 'test_analyzer') RETURN d",
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
    auto& filter = expected.add<irs::Not>().filter<irs::And>().add<irs::by_term>();
    *filter.mutable_field() = mangleNull("a.b.c.e[4].f[5].g[3].g.a");
    filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::null_token_stream::value_null());

    assertFilterSuccess(
        vocbase(),
        "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER not "
        "(d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')]"
        " == null) RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER not (null == "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')])"
        " RETURN d",
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
        "collection FILTER not "
        "(d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')]"
        " == null) RETURN d",
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
        "collection FILTER not "
        "(d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')]"
        " == null) RETURN d",
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
        "collection FILTER not "
        "(d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')]"
        " == null) RETURN d",
        &ctx);
  }

  // complex attribute, numeric
  {
    irs::numeric_token_stream stream;
    stream.reset(3.);
    EXPECT_TRUE(stream.next());
    auto* term = irs::get<irs::term_attribute>(stream);

    irs::Or expected;
    auto& filter = expected.add<irs::Not>().filter<irs::And>().add<irs::by_term>();
    *filter.mutable_field() = mangleNumeric("a.b.c.numeric");
    filter.mutable_options()->term = term->value;

    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER not (d.a.b.c.numeric == 3) RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER not (d['a']['b']['c'].numeric == 3) RETURN "
        "d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER not (d.a.b.c.numeric == 3.0) RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER not (3 == d.a.b.c.numeric) RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER not (3.0 == d.a.b.c.numeric) RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER not (3.0 == d.a['b']['c'].numeric) RETURN "
        "d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER not analyzer(3.0 == d.a['b']['c'].numeric, "
        "'test_analyzer') RETURN d",
        expected);
  }

  // according to ArangoDB rules, expression : not '1' == false
  {
    irs::Or expected;
    auto& filter = expected.add<irs::by_term>();
    *filter.mutable_field() = mangleBool("a");
    filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::boolean_token_stream::value_false());

    assertFilterSuccess(vocbase(),
                        "FOR d IN collection FILTER d.a == not '1' RETURN d",
                        expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess(vocbase(),
                        "FOR d IN collection FILTER not '1' == d.a RETURN d",
                        expected, &ExpressionContextMock::EMPTY);
  }

  // complex attribute, numeric
  {
    irs::numeric_token_stream stream;
    stream.reset(3.);
    EXPECT_TRUE(stream.next());
    auto* term = irs::get<irs::term_attribute>(stream);

    irs::Or expected;
    auto& filter = expected.add<irs::Not>().filter<irs::And>().add<irs::by_term>();
    *filter.mutable_field() = mangleNumeric("a.b.c.numeric[42]");
    filter.mutable_options()->term = term->value;

    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER not (d.a.b.c.numeric[42] == 3) RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER not (d['a']['b']['c'].numeric[42] == 3) "
        "RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER not (d.a.b.c.numeric[42] == 3.0) RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER not (3 == d.a.b.c.numeric[42]) RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER not (3.0 == d.a.b.c.numeric[42]) RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER not (3.0 == d.a['b']['c'].numeric[42]) "
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
    EXPECT_TRUE(stream.next());
    auto* term = irs::get<irs::term_attribute>(stream);

    irs::Or expected;
    auto& filter = expected.add<irs::Not>().filter<irs::And>().add<irs::by_term>();
    *filter.mutable_field() = mangleNumeric("a.b[23].c");
    filter.mutable_options()->term = term->value;

    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER not (d.a.b[23].c == (c + 1.5)) "
        "RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER not (d.a['b'][23].c == (c + 1.5)) "
        "RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER not (d['a']['b'][23].c == (c + "
        "1.5)) RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER not ((c + 1.5) == d.a.b[23].c) "
        "RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET c=41 FOR d IN collection FILTER not ((c + 1.5) == d.a['b'][23].c) "
        "RETURN d",
        expected, &ctx);
    assertFilterSuccess(vocbase(),
                        "LET c=41 FOR d IN collection FILTER not ((c + 1.5) == "
                        "d['a']['b'][23]['c']) RETURN d",
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
    auto& filter = expected.add<irs::Not>().filter<irs::And>().add<irs::by_term>();
    *filter.mutable_field() = mangleNumeric("a.b.c.e[4].f[5].g[3].g.a");
    filter.mutable_options()->term = term->value;

    assertFilterSuccess(
        vocbase(),
        "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER not "
        "(d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')]"
        " == 42.5) RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER not (42.5 == "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')])"
        " RETURN d",
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
        "collection FILTER not "
        "(d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')]"
        " == 42.5) RETURN d",
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
        "collection FILTER not "
        "(d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')]"
        " == 42.5) RETURN d",
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
        "collection FILTER not "
        "(d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')]"
        " == 42.5) RETURN d",
        &ctx);
  }

  // array in expression
  {
    irs::Or expected;
    expected.add<irs::empty>();

    assertFilterSuccess(vocbase(),
                        "FOR d IN collection FILTER not [] == '1' RETURN d",
                        expected, &ExpressionContextMock::EMPTY);
  }

  // nondeterministic expression -> wrap it
  {
    std::string const& refName = "d";
    std::string const& queryString =
        "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER not "
        "(d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_NONDETERM_('a'"
        ")] == '1') RETURN d";
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));

    arangodb::aql::Query query(arangodb::transaction::StandaloneContext::Create(vocbase),
                               arangodb::aql::QueryString(queryString), nullptr,
                               std::make_shared<arangodb::velocypack::Builder>());

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
      arangodb::iresearch::QueryContext const ctx{nullptr, nullptr, nullptr, nullptr, nullptr, ref};
      EXPECT_TRUE(
          (arangodb::iresearch::FilterFactory::filter(nullptr, ctx, *filterNode).ok()));
    }

    // iteratorForCondition
    {
      arangodb::transaction ::Methods trx(arangodb::transaction::StandaloneContext::Create(vocbase),
                                          {}, {}, {}, arangodb::transaction::Options());

      auto dummyPlan = arangodb::tests::planFromQuery(vocbase, "RETURN 1");

      irs::Or expected;
      auto& root = expected.add<irs::Not>().filter<irs::And>();
      root.add<arangodb::iresearch::ByExpression>().init(
          *dummyPlan, *ast,
          *filterNode->getMember(0)->getMember(0)  // d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_NONDETERM_('a')] == '1'
      );
      
      ExpressionContextMock exprCtx;
      exprCtx.setTrx(&trx);

      irs::Or actual;
      arangodb::iresearch::QueryContext const ctx{&trx, dummyPlan.get(), ast,
                                                  &exprCtx, &irs::sub_reader::empty(), ref};
      EXPECT_TRUE(
          (arangodb::iresearch::FilterFactory::filter(&actual, ctx, *filterNode).ok()));
      EXPECT_EQ(expected, actual);
    }
  }

  // nondeterministic expression -> wrap it
  {
    std::string const& refName = "d";
    std::string const& queryString =
        "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER not ('1' < "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_NONDETERM_('a')"
        "]) RETURN d";
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));

    arangodb::aql::Query query(arangodb::transaction::StandaloneContext::Create(vocbase),
                               arangodb::aql::QueryString(queryString), nullptr,
                               std::make_shared<arangodb::velocypack::Builder>());

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
      arangodb::iresearch::QueryContext const ctx{nullptr, nullptr, nullptr, nullptr, nullptr, ref};
      EXPECT_TRUE(
          (arangodb::iresearch::FilterFactory::filter(nullptr, ctx, *filterNode).ok()));
    }

    // iteratorForCondition
    {
      arangodb::transaction ::Methods trx(arangodb::transaction::StandaloneContext::Create(vocbase),
                                          {}, {}, {}, arangodb::transaction::Options());

      auto dummyPlan = arangodb::tests::planFromQuery(vocbase, "RETURN 1");

      irs::Or expected;
      auto& root = expected.add<irs::Not>().filter<irs::And>();
      root.add<arangodb::iresearch::ByExpression>().init(
          *dummyPlan, *ast,
          *filterNode->getMember(0)->getMember(0)  // '1' < d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_NONDETERM_('a')]
      );
      
      ExpressionContextMock exprCtx;
      exprCtx.setTrx(&trx);

      irs::Or actual;
      arangodb::iresearch::QueryContext const ctx{&trx, dummyPlan.get(), ast,
                                                  &exprCtx,
                                                  &irs::sub_reader::empty(), ref};
      EXPECT_TRUE(
          (arangodb::iresearch::FilterFactory::filter(&actual, ctx, *filterNode).ok()));
      EXPECT_EQ(expected, actual);
    }
  }

  // nondeterministic expression -> wrap it
  {
    std::string const& refName = "d";
    std::string const& queryString =
        "FOR d IN collection FILTER not (d.a < _NONDETERM_('1')) RETURN d";
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));

    arangodb::aql::Query query(arangodb::transaction::StandaloneContext::Create(vocbase),
                               arangodb::aql::QueryString(queryString), nullptr,
                               std::make_shared<arangodb::velocypack::Builder>());

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
      arangodb::iresearch::QueryContext const ctx{nullptr, nullptr, nullptr, nullptr, nullptr, ref};
      EXPECT_TRUE(
          (arangodb::iresearch::FilterFactory::filter(nullptr, ctx, *filterNode).ok()));
    }

    // iteratorForCondition
    {
      arangodb::transaction ::Methods trx(arangodb::transaction::StandaloneContext::Create(vocbase),
                                          {}, {}, {}, arangodb::transaction::Options());

      auto dummyPlan = arangodb::tests::planFromQuery(vocbase, "RETURN 1");

      irs::Or expected;
      auto& root = expected.add<irs::Not>().filter<irs::And>();
      root.add<arangodb::iresearch::ByExpression>().init(
          *dummyPlan, *ast,
          *filterNode->getMember(0)->getMember(0)  // d.a < _NONDETERM_('1')
      );
      
      ExpressionContextMock exprCtx;
      exprCtx.setTrx(&trx);

      irs::Or actual;
      arangodb::iresearch::QueryContext const ctx{&trx, dummyPlan.get(), ast,
                                                  &exprCtx,
                                                  &irs::sub_reader::empty(), ref};
      EXPECT_TRUE(
          (arangodb::iresearch::FilterFactory::filter(&actual, ctx, *filterNode).ok()));
      EXPECT_EQ(expected, actual);
    }
  }

  // nondeterministic expression -> wrap it
  {
    std::string const& refName = "d";
    std::string const& queryString =
        "FOR d IN collection FILTER BOOST(not (d.a < _NONDETERM_('1')), 2.5) "
        "RETURN d";
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));

    arangodb::aql::Query query(arangodb::transaction::StandaloneContext::Create(vocbase),
                               arangodb::aql::QueryString(queryString), nullptr,
                               std::make_shared<arangodb::velocypack::Builder>());

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
      arangodb::iresearch::QueryContext const ctx{nullptr, nullptr, nullptr, nullptr, nullptr, ref};
      EXPECT_TRUE((arangodb::iresearch::FilterFactory::filter(nullptr, ctx, *filterNode))
                      .ok());
    }

    // iteratorForCondition
    {
      arangodb::transaction ::Methods trx(arangodb::transaction::StandaloneContext::Create(vocbase),
                                          {}, {}, {}, arangodb::transaction::Options());

      auto dummyPlan = arangodb::tests::planFromQuery(vocbase, "RETURN 1");

      irs::Or expected;
      auto& root = expected.add<irs::Not>();
      root.boost(2.5);
      root.filter<irs::And>().add<arangodb::iresearch::ByExpression>().init(
          *dummyPlan, *ast,
          *filterNode->getMember(0)->getMember(0)->getMember(0)->getMember(0)  // d.a < _NONDETERM_('1')
      );
      
      ExpressionContextMock exprCtx;
      exprCtx.setTrx(&trx);

      irs::Or actual;
      arangodb::iresearch::QueryContext const ctx{&trx, dummyPlan.get(), ast,
                                                  &exprCtx,
                                                  &irs::sub_reader::empty(), ref};
      EXPECT_TRUE(
          (arangodb::iresearch::FilterFactory::filter(&actual, ctx, *filterNode).ok()));
      EXPECT_EQ(expected, actual);
      assertFilterBoost(expected, actual);
    }
  }

  // nondeterministic expression -> wrap it
  {
    std::string const& refName = "d";
    std::string const& queryString =
        "LET k={} FOR d IN collection FILTER not (k.a < _NONDETERM_('1')) "
        "RETURN d";
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));

    arangodb::aql::Query query(arangodb::transaction::StandaloneContext::Create(vocbase),
                               arangodb::aql::QueryString(queryString), nullptr,
                               std::make_shared<arangodb::velocypack::Builder>());

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
      arangodb::iresearch::QueryContext const ctx{nullptr, nullptr, nullptr, nullptr, nullptr, ref};
      EXPECT_TRUE(
          (arangodb::iresearch::FilterFactory::filter(nullptr, ctx, *filterNode).ok()));
    }

    // iteratorForCondition
    {
      arangodb::transaction ::Methods trx(arangodb::transaction::StandaloneContext::Create(vocbase),
                                          {}, {}, {}, arangodb::transaction::Options());

      auto dummyPlan = arangodb::tests::planFromQuery(vocbase, "RETURN 1");

      irs::Or expected;
      auto& root = expected.add<irs::Not>().filter<irs::And>();
      root.add<arangodb::iresearch::ByExpression>().init(
          *dummyPlan, *ast,
          *filterNode->getMember(0)->getMember(0)  // k.a < _NONDETERM_('1')
      );
      
      ExpressionContextMock exprCtx;
      exprCtx.setTrx(&trx);

      irs::Or actual;
      arangodb::iresearch::QueryContext const ctx{&trx, dummyPlan.get(), ast,
                                                  &exprCtx,
                                                  &irs::sub_reader::empty(), ref};
      EXPECT_TRUE(
          (arangodb::iresearch::FilterFactory::filter(&actual, ctx, *filterNode).ok()));
      EXPECT_EQ(expected, actual);
    }
  }

  // nondeterministic expression -> wrap it, boost
  {
    std::string const& refName = "d";
    std::string const& queryString =
        "LET k={} FOR d IN collection FILTER not BOOST(k.a < _NONDETERM_('1'), "
        "1.5) RETURN d";
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));

    arangodb::aql::Query query(arangodb::transaction::StandaloneContext::Create(vocbase),
                               arangodb::aql::QueryString(queryString), nullptr,
                               std::make_shared<arangodb::velocypack::Builder>());

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
      arangodb::iresearch::QueryContext const ctx{nullptr, nullptr, nullptr, nullptr, nullptr, ref};
      EXPECT_TRUE(
          (arangodb::iresearch::FilterFactory::filter(nullptr, ctx, *filterNode).ok()));
    }

    // iteratorForCondition
    {
      arangodb::transaction ::Methods trx(arangodb::transaction::StandaloneContext::Create(vocbase),
                                          {}, {}, {}, arangodb::transaction::Options());

      auto dummyPlan = arangodb::tests::planFromQuery(vocbase, "RETURN 1");

      irs::Or expected;
      auto& root = expected.add<irs::Not>().filter<irs::And>();
      auto& expr = root.add<arangodb::iresearch::ByExpression>();
      expr.boost(1.5);
      expr.init(*dummyPlan, *ast,
                *filterNode->getMember(0)->getMember(0)->getMember(0)->getMember(0)  // k.a < _NONDETERM_('1')
      );
      
      ExpressionContextMock exprCtx;
      exprCtx.setTrx(&trx);

      irs::Or actual;
      arangodb::iresearch::QueryContext const ctx{&trx, dummyPlan.get(), ast,
                                                  &exprCtx,
                                                  &irs::sub_reader::empty(), ref};
      EXPECT_TRUE(
          (arangodb::iresearch::FilterFactory::filter(&actual, ctx, *filterNode).ok()));
      EXPECT_EQ(expected, actual);
      assertFilterBoost(expected, actual);
    }
  }

  // expression with self-reference is not supported by IResearch -> wrap it
  {
    std::string const& refName = "d";
    std::string const& queryString =
        "FOR d IN collection FILTER not (d.a < 1+d.b) RETURN d";
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));

    arangodb::aql::Query query(arangodb::transaction::StandaloneContext::Create(vocbase),
                               arangodb::aql::QueryString(queryString), nullptr,
                               std::make_shared<arangodb::velocypack::Builder>());

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
      arangodb::iresearch::QueryContext const ctx{nullptr, nullptr, nullptr, nullptr, nullptr, ref};
      EXPECT_TRUE(
          (arangodb::iresearch::FilterFactory::filter(nullptr, ctx, *filterNode).ok()));
    }

    // iteratorForCondition
    {
      arangodb::transaction ::Methods trx(arangodb::transaction::StandaloneContext::Create(vocbase),
                                          {}, {}, {}, arangodb::transaction::Options());

      auto dummyPlan = arangodb::tests::planFromQuery(vocbase, "RETURN 1");

      irs::Or expected;
      auto& root = expected.add<irs::Not>().filter<irs::And>();
      root.add<arangodb::iresearch::ByExpression>().init(
          *dummyPlan, *ast,
          *filterNode->getMember(0)->getMember(0)  // d.a < 1+d.b
      );
      
      ExpressionContextMock exprCtx;
      exprCtx.setTrx(&trx);

      irs::Or actual;
      arangodb::iresearch::QueryContext const ctx{&trx, dummyPlan.get(), ast,
                                                  &exprCtx,
                                                  &irs::sub_reader::empty(), ref};
      EXPECT_TRUE(
          (arangodb::iresearch::FilterFactory::filter(&actual, ctx, *filterNode).ok()));
      EXPECT_EQ(expected, actual);
    }
  }

  // expression is not supported by IResearch -> wrap it
  assertExpressionFilter(vocbase(),
                         "FOR d IN collection FILTER not d == '1' RETURN d");
  assertExpressionFilter(vocbase(),
                         "FOR d IN collection FILTER not d[*] == '1' RETURN d");
  assertExpressionFilter(
      vocbase(), "FOR d IN collection FILTER not d.a[*] == '1' RETURN d");
  assertExpressionFilter(vocbase(),
                         "FOR d IN collection FILTER not d.a == '1' RETURN d");
  assertExpressionFilter(
      vocbase(), "FOR d IN collection FILTER not '1' == not d.a RETURN d");
  assertExpressionFilter(vocbase(),
                         "FOR d IN collection FILTER '1' == not d.a RETURN d");
}

TEST_F(IResearchFilterBooleanTest, BinaryOr) {
  // string and string
  {
    irs::Or expected;
    auto& root = expected.add<irs::Or>();
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleStringIdentity("a");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("1"));
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleStringIdentity("b");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("2"));
    }

    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a == '1' or d.b == '2' RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d['a'] == '1' or d.b == '2' RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a == '1' or '2' == d.b RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER '1' == d.a or d.b == '2' RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER '1' == d.a or '2' == d.b RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER '1' == d['a'] or '2' == d.b RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER '1' == d['a'] or '2' == d['b'] RETURN d", expected);
  }

  // string or string
  {
    irs::Or expected;
    auto& root = expected.add<irs::Or>();
    {
      auto& filter = root.add<irs::by_range>();
      *filter.mutable_field() = mangleStringIdentity("a.b.c");
      auto* opts = filter.mutable_options();
      opts->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("1"));
      opts->range.max_type = irs::BoundType::EXCLUSIVE;
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleStringIdentity("c.b.a");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("2"));
    }

    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a.b.c < '1' or d.c.b.a == '2' RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d['a']['b']['c'] < '1' or d.c.b.a == '2' "
        "RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a.b.c < '1' or '2' == d.c.b.a RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER '1' > d.a.b.c or d.c.b.a == '2' RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER '1' > d.a.b.c or '2' == d.c.b.a RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER '1' > d['a']['b']['c'] or '2' == d.c.b.a "
        "RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER '1' > d['a'].b.c or '2' == d.c.b.a RETURN "
        "d",
        expected);
  }

  // string or string, analyzer
  {
    irs::Or expected;
    auto& root = expected.add<irs::Or>();
    {
      auto& filter = root.add<irs::by_range>();
      *filter.mutable_field() = mangleString("a.b.c", "test_analyzer");
      auto* opts = filter.mutable_options();
      opts->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("1"));
      opts->range.max_type = irs::BoundType::EXCLUSIVE;
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleString("c.b.a", "test_analyzer");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("2"));
    }

    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER analyzer(d.a.b.c < '1' or d.c.b.a == '2', "
        "'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER analyzer(d['a']['b']['c'] < '1', "
        "'test_analyzer') or analyzER(d.c.b.a == '2', 'test_analyzer') RETURN "
        "d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER analyzer(analyzer(d.a.b.c < '1', "
        "'test_analyzer') or analyzer('2' == d.c.b.a, 'test_analyzer'), "
        "'identity') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER analyzer(analyzer(analyzer('1' > d.a.b.c, "
        "'test_analyzer'), 'identity') or d.c.b.a == '2', 'test_analyzer') "
        "RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER boost(boost(analyzer(d.a.b.c < '1' or "
        "d.c.b.a == '2', 'test_analyzer'), 0.5), 2) RETURN d",
        expected);
  }

  // string or string, analyzer, boost
  {
    irs::Or expected;
    auto& root = expected.add<irs::Or>();
    root.boost(0.5);
    {
      auto& filter = root.add<irs::by_range>();
      *filter.mutable_field() = mangleString("a.b.c", "test_analyzer");
      auto* opts = filter.mutable_options();
      opts->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("1"));
      opts->range.max_type = irs::BoundType::EXCLUSIVE;
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleString("c.b.a", "test_analyzer");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("2"));
    }

    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER boost(analyzer(d.a.b.c < '1' or d.c.b.a == "
        "'2', 'test_analyzer'), 0.5) RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER analyzer(boost(d.a.b.c < '1' or d.c.b.a == "
        "'2', 0.5), 'test_analyzer') RETURN d",
        expected);
  }

  // string or string, analyzer, boost
  {
    irs::Or expected;
    auto& root = expected.add<irs::Or>();
    root.boost(0.5);
    {
      auto& filter = root.add<irs::by_range>();
      filter.boost(2.5);
      *filter.mutable_field() = mangleString("a.b.c", "test_analyzer");
      auto* opts = filter.mutable_options();
      opts->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("1"));
      opts->range.max_type = irs::BoundType::EXCLUSIVE;
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleStringIdentity("c.b.a");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("2"));
    }

    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER boost(analyzer(boost(d.a.b.c < '1', 2.5), "
        "'test_analyzer') or d.c.b.a == '2', 0.5) RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER boost(boost(analyzer(d.a.b.c < '1', "
        "'test_analyzer'), 2.5) or d.c.b.a == '2', 0.5) RETURN d",
        expected);
  }

  // string or string or not string
  {
    irs::Or expected;
    auto& root = expected.add<irs::Or>();
    auto& subRoot = root.add<irs::Or>();
    {
      auto& filter = subRoot.add<irs::by_term>();
      *filter.mutable_field() = mangleStringIdentity("a");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("1"));
    }
    {
      auto& filter = subRoot.add<irs::by_term>();
      *filter.mutable_field() = mangleStringIdentity("a");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("2"));
    }
    {
      auto& filter = root.add<irs::Not>().filter<irs::by_term>();
      *filter.mutable_field() = mangleStringIdentity("b");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("3"));
    }


    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a == '1' or '2' == d.a or d.b != '3' "
        "RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d['a'] == '1' or '2' == d['a'] or d.b != "
        "'3' RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a == '1' or '2' == d.a or '3' != d.b "
        "RETURN d",
        expected);
  }

  // string or string or not string
  {
    irs::Or expected;
    auto& root = expected.add<irs::Or>();
    root.boost(2.5);
    auto& subRoot = root.add<irs::Or>();
    {
      auto& filter = subRoot.add<irs::by_term>();
      filter.boost(0.5);
      *filter.mutable_field() = mangleString("a", "test_analyzer");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("1"));
    }
    {
      auto& filter = subRoot.add<irs::by_term>();
      *filter.mutable_field() = mangleStringIdentity("a");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("2"));
    }
    {
      auto& filter = root.add<irs::Not>().filter<irs::by_term>();
      filter.boost(1.5);
      *filter.mutable_field() = mangleString("b", "test_analyzer");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("3"));
    }

    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER boost(analyzer(analyzer(boost(d.a == '1', "
        "0.5), 'test_analyzer') or analyzer('2' == d.a, 'identity') or "
        "boost(d.b != '3', 1.5), 'test_analyzer'), 2.5) RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER boost(analyzer(boost(d['a'] == '1', 0.5), "
        "'test_analyzer') or '2' == d['a'] or boost(analyzer(d.b != '3', "
        "'test_analyzer'), 1.5), 2.5) RETURN d",
        expected);
  }

  // string in or not string
  {
    irs::Or expected;
    auto& root = expected.add<irs::Or>();
    auto& subRoot = root.add<irs::Or>();
    {
      auto& filter = subRoot.add<irs::by_term>();
      *filter.mutable_field() = mangleStringIdentity("a");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("1"));
    }
    {
      auto& filter = subRoot.add<irs::by_term>();
      *filter.mutable_field() = mangleStringIdentity("a");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("2"));
    }
    {
      auto& filter = root.add<irs::Not>().filter<irs::by_term>();
      *filter.mutable_field() = mangleStringIdentity("b");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("3"));
    }

    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a in ['1', '2'] or d.b != '3' RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d['a'] in ['1', '2'] or d.b != '3' RETURN "
        "d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a in ['1', '2'] or '3' != d.b RETURN d", expected);
  }

  // bool and null
  {
    irs::Or expected;
    auto& root = expected.add<irs::Or>();
    {
      auto& filter = root.add<irs::by_range>();
      *filter.mutable_field() = mangleBool("b.c");
      auto* opts = filter.mutable_options();
      opts->range.min = irs::ref_cast<irs::byte_type>(irs::boolean_token_stream::value_false());
      opts->range.min_type = irs::BoundType::EXCLUSIVE;
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleNull("a.b.c");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::null_token_stream::value_null());
    }

    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.b.c > false or d.a.b.c == null RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER analyzer(d['b']['c'] > false or d.a.b.c == "
        "null, 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER false < d.b.c or d.a.b.c == null RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.b.c > false or null == d.a.b.c RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER false < d.b.c or null == d.a.b.c RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER false < d.b.c or null == d['a']['b']['c'] "
        "RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER false < d['b']['c'] or null == "
        "d['a']['b']['c'] RETURN d",
        expected);
  }

  // bool and null, boost
  {
    irs::Or expected;
    auto& root = expected.add<irs::Or>();
    root.boost(1.5);
    {
      auto& filter = root.add<irs::by_range>();
      *filter.mutable_field() = mangleBool("b.c");
      auto* opts = filter.mutable_options();
      opts->range.min = irs::ref_cast<irs::byte_type>(irs::boolean_token_stream::value_false());
      opts->range.min_type = irs::BoundType::EXCLUSIVE;
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleNull("a.b.c");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::null_token_stream::value_null());
    }

    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER boost(analyzer(d['b']['c'] > false or "
        "d.a.b.c == null, 'test_analyzer'), 1.5) RETURN d",
        expected);
  }

  // bool and null, boost
  {
    irs::Or expected;
    auto& root = expected.add<irs::Or>();
    {
      auto& filter = root.add<irs::by_range>();
      filter.boost(1.5);
      *filter.mutable_field() = mangleBool("b.c");
      auto* opts = filter.mutable_options();
      opts->range.min = irs::ref_cast<irs::byte_type>(irs::boolean_token_stream::value_false());
      opts->range.min_type = irs::BoundType::EXCLUSIVE;
    }
    {
      auto& filter = root.add<irs::by_term>();
      filter.boost(0.5);
      *filter.mutable_field() = mangleNull("a.b.c");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::null_token_stream::value_null());
    }

    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER boost(d['b']['c'] > false, 1.5) or "
        "boost(d.a.b.c == null, 0.5) RETURN d",
        expected);
  }

  // numeric range
  {
    irs::numeric_token_stream minTerm;
    minTerm.reset(15.);
    irs::numeric_token_stream maxTerm;
    maxTerm.reset(40.);

    irs::Or expected;
    auto& root = expected.add<irs::Or>();
    {
      auto& filter = root.add<irs::by_granular_range>();
      *filter.mutable_field() = mangleNumeric("a.b.c");
      auto* opts = filter.mutable_options();
      irs::set_granular_term(opts->range.min, minTerm);
      opts->range.min_type = irs::BoundType::EXCLUSIVE;
    }
    {
      auto& filter = root.add<irs::by_granular_range>();
      *filter.mutable_field() = mangleNumeric("a.b.c");
      auto* opts = filter.mutable_options();
      irs::set_granular_term(opts->range.max, maxTerm);
      opts->range.max_type = irs::BoundType::EXCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a.b.c > 15 or d.a.b.c < 40 RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d['a']['b']['c'] > 15 or d['a']['b']['c'] "
        "< 40 RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER 15 < d['a']['b']['c'] or d.a.b.c < 40 "
        "RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a.b.c > 15 or 40 > d.a.b.c RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER 15 < d.a.b.c or 40 > d.a.b.c RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER 15 < d.a['b']['c'] or 40 > d.a.b.c RETURN "
        "d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a.b.c > 15.0 or d.a.b.c < 40.0 RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d['a'].b.c > 15.0 or d['a']['b'].c < 40.0 "
        "RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER 15.0 < d.a.b.c or d.a.b.c < 40.0 RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a.b.c > 15.0 or 40.0 > d.a.b.c RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER 15.0 < d.a.b.c or 40.0 > d.a.b.c RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER 15.0 < d['a']['b']['c'] or 40.0 > d.a.b.c "
        "RETURN d",
        expected);
  }

  // numeric range
  {
    irs::numeric_token_stream minTerm;
    minTerm.reset(15.);
    irs::numeric_token_stream maxTerm;
    maxTerm.reset(40.);

    irs::Or expected;
    auto& root = expected.add<irs::Or>();
    root.boost(1.5);
    {
      auto& filter = root.add<irs::by_granular_range>();
      *filter.mutable_field() = mangleNumeric("a.b.c");
      auto* opts = filter.mutable_options();
      irs::set_granular_term(opts->range.min, minTerm);
      opts->range.min_type = irs::BoundType::EXCLUSIVE;
    }
    {
      auto& filter = root.add<irs::by_granular_range>();
      *filter.mutable_field() = mangleNumeric("a.b.c");
      auto* opts = filter.mutable_options();
      irs::set_granular_term(opts->range.max, maxTerm);
      opts->range.max_type = irs::BoundType::EXCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER boost(d.a.b.c > 15 or d.a.b.c < 40, 1.5) "
        "RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER analyzer(boost(d['a']['b']['c'] > 15 or "
        "d['a']['b']['c'] < 40, 1.5), 'test_analyzer') RETURN d",
        expected);
  }

  // numeric range
  {
    irs::numeric_token_stream minTerm;
    minTerm.reset(15.);
    irs::numeric_token_stream maxTerm;
    maxTerm.reset(40.);

    irs::Or expected;
    auto& root = expected.add<irs::Or>();
    {
      auto& filter = root.add<irs::by_granular_range>();
      filter.boost(1.5);
      *filter.mutable_field() = mangleNumeric("a.b.c");
      auto* opts = filter.mutable_options();
      irs::set_granular_term(opts->range.min, minTerm);
      opts->range.min_type = irs::BoundType::EXCLUSIVE;
    }
    {
      auto& filter = root.add<irs::by_granular_range>();
      filter.boost(0.5);
      *filter.mutable_field() = mangleNumeric("a.b.c");
      auto* opts = filter.mutable_options();
      irs::set_granular_term(opts->range.max, maxTerm);
      opts->range.max_type = irs::BoundType::EXCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER boost(d.a.b.c > 15, 1.5) or boost(d.a.b.c "
        "< 40, 0.5) RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER analyzer(boost(d['a']['b']['c'] > 15, 1.5) "
        "or boost(d['a']['b']['c'] < 40, 0.5), 'test_analyzer') RETURN d",
        expected);
  }

  // numeric range
  {
    irs::numeric_token_stream minTerm;
    minTerm.reset(15.);
    irs::numeric_token_stream maxTerm;
    maxTerm.reset(40.);

    irs::Or expected;
    auto& root = expected.add<irs::Or>();
    {
      auto& filter = root.add<irs::by_granular_range>();
      *filter.mutable_field() = mangleNumeric("a.b.c");
      auto* opts = filter.mutable_options();
      irs::set_granular_term(opts->range.min, minTerm);
      opts->range.min_type = irs::BoundType::INCLUSIVE;
    }
    {
      auto& filter = root.add<irs::by_granular_range>();
      *filter.mutable_field() = mangleNumeric("a.b.c");
      auto* opts = filter.mutable_options();
      irs::set_granular_term(opts->range.max, maxTerm);
      opts->range.max_type = irs::BoundType::EXCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a.b.c >= 15 or d.a.b.c < 40 RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER 15 <= d.a.b.c or d.a.b.c < 40 RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER 15 <= d['a']['b']['c'] or d['a']['b']['c'] "
        "< 40 RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a.b.c >= 15 or 40 > d.a.b.c RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a['b']['c'] >= 15 or 40 > d.a.b.c RETURN "
        "d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER 15 <= d.a.b.c or 40 > d.a.b.c RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a.b.c >= 15.0 or d.a.b.c < 40.0 RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d['a']['b']['c'] >= 15.0 or d['a']['b'].c "
        "< 40.0 RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER 15.0 <= d.a.b.c or d.a.b.c < 40.0 RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a.b.c >= 15.0 or 40.0 > d.a.b.c RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER 15.0 <= d.a.b.c or 40.0 > d.a.b.c RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER 15.0 <= d['a']['b'].c or 40.0 > d.a.b.c "
        "RETURN d",
        expected);
  }

  // numeric range
  {
    irs::numeric_token_stream minTerm;
    minTerm.reset(15.);
    irs::numeric_token_stream maxTerm;
    maxTerm.reset(40.);

    irs::Or expected;
    auto& root = expected.add<irs::Or>();
    {
      auto& filter = root.add<irs::by_granular_range>();
      *filter.mutable_field() = mangleNumeric("a.b.c");
      auto* opts = filter.mutable_options();
      irs::set_granular_term(opts->range.min, minTerm);
      opts->range.min_type = irs::BoundType::INCLUSIVE;
    }
    {
      auto& filter = root.add<irs::by_granular_range>();
      *filter.mutable_field() = mangleNumeric("a.b.c");
      auto* opts = filter.mutable_options();
      irs::set_granular_term(opts->range.max, maxTerm);
      opts->range.max_type = irs::BoundType::INCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a.b.c >= 15 or d.a.b.c <= 40 RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a['b']['c'] >= 15 or d['a']['b']['c'] <= "
        "40 RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER 15 <= d.a.b.c or d.a.b.c <= 40 RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a.b.c >= 15 or 40 >= d.a.b.c RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER 15 <= d.a.b.c or 40 >= d.a.b.c RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER 15 <= d['a'].b.c or 40 >= d['a'].b.c "
        "RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a.b.c >= 15.0 or d.a.b.c <= 40.0 RETURN "
        "d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER 15.0 <= d.a.b.c or d.a.b.c <= 40.0 RETURN "
        "d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER 15.0 <= d.a['b']['c'] or d['a']['b']['c'] "
        "<= 40.0 RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a.b.c >= 15.0 or 40.0 >= d.a.b.c RETURN "
        "d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER 15.0 <= d.a.b.c or 40.0 >= d.a.b.c RETURN "
        "d",
        expected);
  }

  // numeric range
  {
    irs::numeric_token_stream minTerm;
    minTerm.reset(15.);
    irs::numeric_token_stream maxTerm;
    maxTerm.reset(40.);

    irs::Or expected;
    auto& root = expected.add<irs::Or>();
    {
      auto& filter = root.add<irs::by_granular_range>();
      *filter.mutable_field() = mangleNumeric("a.b.c");
      auto* opts = filter.mutable_options();
      irs::set_granular_term(opts->range.min, minTerm);
      opts->range.min_type = irs::BoundType::EXCLUSIVE;
    }
    {
      auto& filter = root.add<irs::by_granular_range>();
      *filter.mutable_field() = mangleNumeric("a.b.c");
      auto* opts = filter.mutable_options();
      irs::set_granular_term(opts->range.max, maxTerm);
      opts->range.max_type = irs::BoundType::INCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a.b.c > 15 or d.a.b.c <= 40 RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d['a']['b']['c'] > 15 or d.a.b.c <= 40 "
        "RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER 15 < d.a.b.c or d.a.b.c <= 40 RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER 15 < d['a'].b.c or d['a'].b.c <= 40 RETURN "
        "d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a.b.c > 15 or 40 >= d.a.b.c RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a['b']['c'] > 15 or 40 >= "
        "d['a']['b']['c'] RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER 15 < d.a.b.c or 40 >= d.a.b.c RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a.b.c > 15.0 or d.a.b.c <= 40.0 RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a['b']['c'] > 15.0 or d.a['b']['c'] <= "
        "40.0 RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER 15.0 < d.a.b.c or d.a.b.c <= 40.0 RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a.b.c > 15.0 or 40.0 >= d.a.b.c RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER 15.0 < d.a.b.c or 40.0 >= d.a.b.c RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER 15.0 < d['a'].b.c or 40.0 >= "
        "d['a']['b']['c'] RETURN d",
        expected);
  }

  // heterogeneous expression
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("boolVal",
                     arangodb::aql::AqlValue(arangodb::aql::AqlValueHintBool(false)));

    irs::Or expected;
    auto& root = expected.add<irs::Or>();
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleStringIdentity("a.b.c.e.f");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("1"));
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleBool("a.b.c.e.f");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::boolean_token_stream::value_false());
    }

    assertFilterSuccess(
        vocbase(),
        "LET boolVal=false FOR d IN collection FILTER d.a.b.c.e.f=='1' OR "
        "d.a.b.c.e.f==boolVal RETURN d",
        expected,
        &ctx  // expression context
    );
  }

  // heterogeneous expression
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("strVal", arangodb::aql::AqlValue("str"));
    ctx.vars.emplace("numVal", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt(2)));

    irs::numeric_token_stream stream;
    stream.reset(3.);
    EXPECT_TRUE(stream.next());
    auto* term = irs::get<irs::term_attribute>(stream);

    irs::Or expected;
    auto& root = expected.add<irs::Or>();
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleStringIdentity("a.b.c.e.f");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("str"));
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleNumeric("a.b.c.e.f");
      filter.mutable_options()->term = term->value;
    }

    assertFilterSuccess(
        vocbase(),
        "LET strVal='str' LET numVal=2 FOR d IN collection FILTER "
        "d.a.b.c.e.f==strVal OR d.a.b.c.e.f==(numVal+1) RETURN d",
        expected,
        &ctx  // expression context
    );
  }

  // heterogeneous expression
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("boolVal",
                     arangodb::aql::AqlValue(arangodb::aql::AqlValueHintBool(false)));
    ctx.vars.emplace("nullVal", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintNull{}));

    irs::Or expected;
    auto& root = expected.add<irs::Or>();
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleBool("a.b.c.e.f");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::boolean_token_stream::value_false());
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleNull("a.b.c.e.f");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::null_token_stream::value_null());
    }

    assertFilterSuccess(
        vocbase(),
        "LET boolVal=false LET nullVal=null FOR d IN collection FILTER "
        "d.a.b.c.e.f==boolVal OR d.a.b.c.e.f==nullVal RETURN d",
        expected,
        &ctx  // expression context
    );
  }

  // noneterministic expression -> wrap it
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));

    std::string const refName = "d";
    std::string const queryString =
        "FOR d IN collection FILTER d.a.b.c > _NONDETERM_('15') or d.a.b.c < "
        "'40' RETURN d";

    arangodb::aql::Query query(arangodb::transaction::StandaloneContext::Create(vocbase),
                               arangodb::aql::QueryString(queryString), nullptr,
                               std::make_shared<arangodb::velocypack::Builder>());

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
      arangodb::iresearch::QueryContext const ctx{nullptr, nullptr, nullptr, nullptr, nullptr, ref};
      EXPECT_TRUE(
          (arangodb::iresearch::FilterFactory::filter(nullptr, ctx, *filterNode).ok()));
    }

    // iteratorForCondition
    {
      arangodb::transaction ::Methods trx(arangodb::transaction::StandaloneContext::Create(vocbase),
                                          {}, {}, {}, arangodb::transaction::Options());

      auto dummyPlan = arangodb::tests::planFromQuery(vocbase, "RETURN 1");

      irs::Or expected;
      auto& root = expected.add<irs::Or>();
      root.add<arangodb::iresearch::ByExpression>().init(
          *dummyPlan, *ast,
          *filterNode->getMember(0)->getMember(0)  // d.a.b.c > _NONDETERM_(15)
      );
      {
        auto& filter = root.add<irs::by_range>();
        *filter.mutable_field() = mangleStringIdentity("a.b.c");
        auto* opts = filter.mutable_options();
        opts->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("40")); // d.a.b.c < 40
        opts->range.max_type = irs::BoundType::EXCLUSIVE;
      }

      ExpressionContextMock exprCtx;
      exprCtx.setTrx(&trx);
      
      irs::Or actual;
      arangodb::iresearch::QueryContext const ctx{&trx, dummyPlan.get(), ast,
                                                  &exprCtx,
                                                  &irs::sub_reader::empty(), ref};
      EXPECT_TRUE(
          (arangodb::iresearch::FilterFactory::filter(&actual, ctx, *filterNode).ok()));
      EXPECT_EQ(expected, actual);
    }
  }

  // noneterministic expression -> wrap it, boost
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));

    std::string const refName = "d";
    std::string const queryString =
        "FOR d IN collection FILTER boost(d.a.b.c > _NONDETERM_('15') or "
        "d.a.b.c < '40', 2.5) RETURN d";

    arangodb::aql::Query query(arangodb::transaction::StandaloneContext::Create(vocbase),
                               arangodb::aql::QueryString(queryString), nullptr,
                               std::make_shared<arangodb::velocypack::Builder>());

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
      arangodb::iresearch::QueryContext const ctx{nullptr, nullptr, nullptr, nullptr, nullptr, ref};
      EXPECT_TRUE(
          (arangodb::iresearch::FilterFactory::filter(nullptr, ctx, *filterNode).ok()));
    }

    // iteratorForCondition
    {
      arangodb::transaction ::Methods trx(arangodb::transaction::StandaloneContext::Create(vocbase),
                                          {}, {}, {}, arangodb::transaction::Options());

      auto dummyPlan = arangodb::tests::planFromQuery(vocbase, "RETURN 1");

      irs::Or expected;
      auto& root = expected.add<irs::Or>();
      root.boost(2.5);
      root.add<arangodb::iresearch::ByExpression>().init(
          *dummyPlan, *ast,
          *filterNode->getMember(0)->getMember(0)->getMember(0)->getMember(0)  // d.a.b.c > _NONDETERM_(15)
      );
      {
        auto& filter = root.add<irs::by_range>();
        *filter.mutable_field() = mangleStringIdentity("a.b.c");
        auto* opts = filter.mutable_options();
        opts->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("40")); // d.a.b.c < 40
        opts->range.max_type = irs::BoundType::EXCLUSIVE;
      }

      ExpressionContextMock exprCtx;
      exprCtx.setTrx(&trx);
      
      irs::Or actual;
      arangodb::iresearch::QueryContext const ctx{&trx, dummyPlan.get(), ast,
                                                  &exprCtx,
                                                  &irs::sub_reader::empty(), ref};
      EXPECT_TRUE(
          (arangodb::iresearch::FilterFactory::filter(&actual, ctx, *filterNode).ok()));
      EXPECT_EQ(expected, actual);
      assertFilterBoost(expected, actual);
    }
  }
}

TEST_F(IResearchFilterBooleanTest, BinaryAnd) {
  // string and string
  {
    irs::Or expected;
    auto& root = expected.add<irs::And>();
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleStringIdentity("a");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("1"));
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleStringIdentity("b");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("2"));
    }

    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a == '1' and d.b == '2' RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d['a'] == '1' and d.b == '2' RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a == '1' and '2' == d.b RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER '1' == d.a and d.b == '2' RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER '1' == d.a and '2' == d.b RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER '1' == d['a'] and '2' == d['b'] RETURN d", expected);
  }

  // string and string
  {
    irs::Or expected;
    auto& root = expected.add<irs::And>();
    {
      auto& filter = root.add<irs::by_range>();
      *filter.mutable_field() = mangleStringIdentity("a.b.c");
      auto* opts = filter.mutable_options();
      opts->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("1"));
      opts->range.max_type = irs::BoundType::EXCLUSIVE;
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleStringIdentity("c.b.a");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("2"));
    }

    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a.b.c < '1' and d.c.b.a == '2' RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d['a']['b']['c'] < '1' and d.c.b['a'] == "
        "'2' RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d['a'].b.c < '1' and d.c.b['a'] == '2' "
        "RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a.b.c < '1' and '2' == d.c.b.a RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER '1' > d.a.b.c and d.c.b.a == '2' RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER '1' > d['a']['b']['c'] and d.c.b.a == '2' "
        "RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER '1' > d.a.b.c and '2' == d.c.b.a RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER '1' > d['a']['b']['c'] and '2' == "
        "d.c.b['a'] RETURN d",
        expected);
  }

  // string and string, boost, analyzer
  {
    irs::Or expected;
    auto& root = expected.add<irs::And>();
    root.boost(0.5);
    {
      auto& filter = root.add<irs::by_range>();
      *filter.mutable_field() = mangleString("a.b.c", "test_analyzer");
      auto* opts = filter.mutable_options();
      opts->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("1"));
      opts->range.max_type = irs::BoundType::EXCLUSIVE;
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleStringIdentity("c.b.a");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("2"));
    }

    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER analyzer(boost(d.a.b.c < '1' and "
        "analyzer(d.c.b.a == '2', 'identity'), 0.5), 'test_analyzer') RETURN d",
        expected);
  }

  // string and string, boost, analyzer
  {
    irs::Or expected;
    auto& root = expected.add<irs::And>();
    {
      auto& filter = root.add<irs::by_range>();
      filter.boost(0.5);
      *filter.mutable_field() = mangleString("a.b.c", "test_analyzer");
      auto* opts = filter.mutable_options();
      opts->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("1"));
      opts->range.max_type = irs::BoundType::EXCLUSIVE;
    }
    {
      auto& filter = root.add<irs::by_term>();
      filter.boost(0.5);
      *filter.mutable_field() = mangleStringIdentity("c.b.a");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("2"));
    }

    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER boost(analyzer(d['a']['b']['c'] < '1', "
        "'test_analyzer'), 0.5) and boost(d.c.b['a'] == '2', 0.5) RETURN d",
        expected);
  }

  // string and not string
  {
    irs::Or expected;
    auto& root = expected.add<irs::And>();
    {
      auto& filter = root.add<irs::by_range>();
      *filter.mutable_field() = mangleStringIdentity("a.b.c");
      auto* opts = filter.mutable_options();
      opts->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("1"));
      opts->range.max_type = irs::BoundType::EXCLUSIVE;
    }
    {
      auto& filter = root.add<irs::Not>().filter<irs::And>().add<irs::by_term>();
      *filter.mutable_field() = mangleStringIdentity("c.b.a");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("2"));
    }

    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a.b.c < '1' and not (d.c.b.a == '2') "
        "RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d['a'].b.c < '1' and not (d.c.b['a'] == "
        "'2') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a.b.c < '1' and not ('2' == d.c.b.a) "
        "RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d['a']['b']['c'] < '1' and not ('2' == "
        "d.c.b['a']) RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER '1' > d.a.b.c and not (d.c.b.a == '2') "
        "RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER '1' > d.a['b']['c'] and not (d.c.b.a == "
        "'2') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER '1' > d.a.b.c and not ('2' == d.c.b.a) "
        "RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER '1' > d['a'].b.c and not ('2' == "
        "d.c.b['a']) RETURN d",
        expected);
  }

  // string and not string, boost, analyzer
  {
    irs::Or expected;
    auto& root = expected.add<irs::And>();
    root.boost(0.5);
    {
      auto& filter = root.add<irs::by_range>();
      *filter.mutable_field() = mangleStringIdentity("a.b.c");
      auto* opts = filter.mutable_options();
      opts->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("1"));
      opts->range.max_type = irs::BoundType::EXCLUSIVE;
    }
    {
      auto& filter = root.add<irs::Not>().filter<irs::And>().add<irs::by_term>();
      *filter.mutable_field() = mangleString("c.b.a", "test_analyzer");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("2"));
    }

    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER boost(d.a.b.c < '1' and not "
        "analyzer(d.c.b.a == '2', 'test_analyzer'), 0.5) RETURN d",
        expected);
  }

  // string and not string, boost, analyzer
  {
    irs::Or expected;
    auto& root = expected.add<irs::And>();
    {
      auto& filter = root.add<irs::by_range>();
      *filter.mutable_field() = mangleStringIdentity("a.b.c");
      auto* opts = filter.mutable_options();
      opts->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("1"));
      opts->range.max_type = irs::BoundType::EXCLUSIVE;
    }
    {
      auto& filter = root.add<irs::Not>().filter<irs::And>().add<irs::by_term>();
      filter.boost(0.5);
      *filter.mutable_field() = mangleString("c.b.a", "test_analyzer");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("2"));
    }

    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a.b.c < '1' and not "
        "boost(analyzer(d.c.b.a == '2', 'test_analyzer'), 0.5) RETURN d",
        expected);
  }

  // expression is not supported by IResearch -> wrap it
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));

    std::string const refName = "d";
    std::string const queryString =
        "FOR d IN collection FILTER d.a.b.c < '1' and not d.c.b.a == '2' "
        "RETURN d";

    arangodb::aql::Query query(arangodb::transaction::StandaloneContext::Create(vocbase),
                               arangodb::aql::QueryString(queryString), nullptr,
                               std::make_shared<arangodb::velocypack::Builder>());

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
      arangodb::iresearch::QueryContext const ctx{nullptr, nullptr, nullptr, nullptr, nullptr, ref};
      EXPECT_TRUE(
          (arangodb::iresearch::FilterFactory::filter(nullptr, ctx, *filterNode).ok()));
    }

    // iteratorForCondition
    {
      arangodb::transaction ::Methods trx(arangodb::transaction::StandaloneContext::Create(vocbase),
                                          {}, {}, {}, arangodb::transaction::Options());

      auto dummyPlan = arangodb::tests::planFromQuery(vocbase, "RETURN 1");

      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& filter = root.add<irs::by_range>();
        *filter.mutable_field() = mangleStringIdentity("a.b.c");
        auto* opts = filter.mutable_options();
        opts->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("1"));
        opts->range.max_type = irs::BoundType::EXCLUSIVE;
      }

      root.add<arangodb::iresearch::ByExpression>().init(
          *dummyPlan, *ast,
          *filterNode->getMember(0)->getMember(1)  // not d.c.b.a == '2'
      );
      
      ExpressionContextMock exprCtx;
      exprCtx.setTrx(&trx);

      irs::Or actual;
      arangodb::iresearch::QueryContext const ctx{&trx, dummyPlan.get(), ast,
                                                  &exprCtx,
                                                  &irs::sub_reader::empty(), ref};
      EXPECT_TRUE(
          (arangodb::iresearch::FilterFactory::filter(&actual, ctx, *filterNode).ok()));
      EXPECT_EQ(expected, actual);
    }
  }

  // bool and null
  {
    irs::Or expected;
    auto& root = expected.add<irs::And>();
    {
      auto& filter = root.add<irs::by_range>();
      *filter.mutable_field() = mangleBool("b.c");
      auto* opts = filter.mutable_options();
      opts->range.min = irs::ref_cast<irs::byte_type>(irs::boolean_token_stream::value_false());
      opts->range.min_type = irs::BoundType::EXCLUSIVE;
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleNull("a.b.c");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::null_token_stream::value_null());
    }

    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.b.c > false and d.a.b.c == null RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d['b']['c'] > false and d['a']['b']['c'] "
        "== null RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d['b']['c'] > false and d['a'].b.c == null "
        "RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER false < d.b.c and d.a.b.c == null RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.b.c > false and null == d.a.b.c RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d['b']['c'] > false and null == d.a.b.c "
        "RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER false < d.b.c and null == d.a.b.c RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER false < d.b.c and null == d['a']['b']['c'] "
        "RETURN d",
        expected);
  }

  // bool and null, boost
  {
    irs::Or expected;
    auto& root = expected.add<irs::And>();
    root.boost(1.5);
    {
      auto& filter = root.add<irs::by_range>();
      *filter.mutable_field() = mangleBool("b.c");
      auto* opts = filter.mutable_options();
      opts->range.min = irs::ref_cast<irs::byte_type>(irs::boolean_token_stream::value_false());
      opts->range.min_type = irs::BoundType::EXCLUSIVE;
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleNull("a.b.c");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::null_token_stream::value_null());
    }

    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER boost(d.b.c > false and d.a.b.c == null, "
        "1.5) RETURN d",
        expected);
  }

  // bool and null, boost
  {
    irs::Or expected;
    auto& root = expected.add<irs::And>();
    {
      auto& filter = root.add<irs::by_range>();
      filter.boost(0.5);
      *filter.mutable_field() = mangleBool("b.c");
      auto* opts = filter.mutable_options();
      opts->range.min = irs::ref_cast<irs::byte_type>(irs::boolean_token_stream::value_false());
      opts->range.min_type = irs::BoundType::EXCLUSIVE;
    }
    {
      auto& filter = root.add<irs::by_term>();
      filter.boost(1.5);
      *filter.mutable_field() = mangleNull("a.b.c");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::null_token_stream::value_null());
    }

    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER boost(d.b.c > false, 0.5) and "
        "boost(d.a.b.c == null, 1.5) RETURN d",
        expected);
  }

  // numeric range
  {
    irs::numeric_token_stream minTerm;
    minTerm.reset(15.);
    irs::numeric_token_stream maxTerm;
    maxTerm.reset(40.);

    irs::Or expected;
    auto& root = expected.add<irs::And>();
    {
      auto& filter = root.add<irs::by_granular_range>();
      *filter.mutable_field() = mangleNumeric("a.b.c");
      auto* opts = filter.mutable_options();
      irs::set_granular_term(opts->range.min, minTerm);
      opts->range.min_type = irs::BoundType::EXCLUSIVE;
    }
    {
      auto& filter = root.add<irs::by_granular_range>();
      *filter.mutable_field() = mangleNumeric("a.b.c");
      auto* opts = filter.mutable_options();
      irs::set_granular_term(opts->range.max, maxTerm);
      opts->range.max_type = irs::BoundType::EXCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a.b.c > 15 and d.a.b.c < 40 RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d['a'].b.c > 15 and d['a']['b']['c'] < 40 "
        "RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a['b']['c'] > 15 and d['a']['b']['c'] < "
        "40 RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d['a'].b.c > 15 and d.a.b.c < 40 RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER 15 < d.a.b.c and d.a.b.c < 40 RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER 15 < d['a'].b.c and d.a.b.c < 40 RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a.b.c > 15 and 40 > d.a.b.c RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d['a']['b']['c'] > 15 and 40 > "
        "d['a']['b']['c'] RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER 15 < d.a.b.c and 40 > d.a.b.c RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a.b.c > 15.0 and d.a.b.c < 40.0 RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a['b']['c'] > 15.0 and d.a['b']['c'] < "
        "40.0 RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER 15.0 < d.a.b.c and d.a.b.c < 40.0 RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a.b.c > 15.0 and 40.0 > d.a.b.c RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d['a']['b']['c'] > 15.0 and 40.0 > "
        "d.a['b']['c'] RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER analyzer(15.0 < d.a.b.c and 40.0 > "
        "d.a.b.c, 'test_analyzer') RETURN d",
        expected);
  }

  // numeric range, boost
  {
    irs::numeric_token_stream minTerm;
    minTerm.reset(15.);
    irs::numeric_token_stream maxTerm;
    maxTerm.reset(40.);

    irs::Or expected;
    auto& root = expected.add<irs::And>();
    root.boost(1.5);
    {
      auto& filter = root.add<irs::by_granular_range>();
      *filter.mutable_field() = mangleNumeric("a.b.c");
      auto* opts = filter.mutable_options();
      irs::set_granular_term(opts->range.min, minTerm);
      opts->range.min_type = irs::BoundType::EXCLUSIVE;
    }
    {
      auto& filter = root.add<irs::by_granular_range>();
      *filter.mutable_field() = mangleNumeric("a.b.c");
      auto* opts = filter.mutable_options();
      irs::set_granular_term(opts->range.max, maxTerm);
      opts->range.max_type = irs::BoundType::EXCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER boost(d.a.b.c > 15 and d.a.b.c < 40, 1.5) "
        "RETURN d",
        expected);
  }

  // numeric range, boost
  {
    irs::numeric_token_stream minTerm;
    minTerm.reset(15.);
    irs::numeric_token_stream maxTerm;
    maxTerm.reset(40.);

    irs::Or expected;
    auto& root = expected.add<irs::And>();
    {
      auto& filter = root.add<irs::by_granular_range>();
      filter.boost(1.5);
      *filter.mutable_field() = mangleNumeric("a.b.c");
      auto* opts = filter.mutable_options();
      irs::set_granular_term(opts->range.min, minTerm);
      opts->range.min_type = irs::BoundType::EXCLUSIVE;
    }
    {
      auto& filter = root.add<irs::by_granular_range>();
      filter.boost(1.5);
      *filter.mutable_field() = mangleNumeric("a.b.c");
      auto* opts = filter.mutable_options();
      irs::set_granular_term(opts->range.max, maxTerm);
      opts->range.max_type = irs::BoundType::EXCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER boost(d.a.b.c > 15, 1.5) and boost(d.a.b.c "
        "< 40, 1.5) RETURN d",
        expected);
  }

  // numeric range, boost
  {
    irs::numeric_token_stream minTerm;
    minTerm.reset(15.);
    irs::numeric_token_stream maxTerm;
    maxTerm.reset(40.);

    irs::Or expected;
    auto& root = expected.add<irs::And>();
    {
      auto& filter = root.add<irs::by_granular_range>();
      filter.boost(0.5);
      *filter.mutable_field() = mangleNumeric("a.b.c");
      auto* opts = filter.mutable_options();
      irs::set_granular_term(opts->range.min, minTerm);
      opts->range.min_type = irs::BoundType::EXCLUSIVE;
    }
    {
      auto& filter = root.add<irs::by_granular_range>();
      filter.boost(1.5);
      *filter.mutable_field() = mangleNumeric("a.b.c");
      auto* opts = filter.mutable_options();
      irs::set_granular_term(opts->range.max, maxTerm);
      opts->range.max_type = irs::BoundType::EXCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER boost(d.a.b.c > 15, 0.5) and boost(d.a.b.c "
        "< 40, 1.5) RETURN d",
        expected);
  }

  // numeric range, boost
  {
    irs::numeric_token_stream minTerm;
    minTerm.reset(15.);
    irs::numeric_token_stream maxTerm;
    maxTerm.reset(40.);

    irs::Or expected;
    auto& root = expected.add<irs::And>();
    {
      auto& filter = root.add<irs::by_granular_range>();
      *filter.mutable_field() = mangleNumeric("a.b.c");
      auto* opts = filter.mutable_options();
      irs::set_granular_term(opts->range.min, minTerm);
      opts->range.min_type = irs::BoundType::EXCLUSIVE;
    }
    {
      auto& filter = root.add<irs::by_granular_range>();
      *filter.mutable_field() = mangleNumeric("a.b.c");
      auto* opts = filter.mutable_options();
      irs::set_granular_term(opts->range.max, maxTerm);
      opts->range.max_type = irs::BoundType::EXCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a.b.c > 15 and analyzer(d.a.b.c < 40, "
        "'test_analyzer') RETURN d",
        expected);
  }

  // expression is not supported by IResearch -> wrap it
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));

    std::string const refName = "d";
    std::string const queryString =
        "FOR d IN collection FILTER d.a[*].b > 15 and d.a[*].b < 40 RETURN d";

    arangodb::aql::Query query(arangodb::transaction::StandaloneContext::Create(vocbase),
                               arangodb::aql::QueryString(queryString), nullptr,
                               std::make_shared<arangodb::velocypack::Builder>());

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
      arangodb::iresearch::QueryContext const ctx{nullptr, nullptr, nullptr, nullptr, nullptr, ref};
      EXPECT_TRUE(
          (arangodb::iresearch::FilterFactory::filter(nullptr, ctx, *filterNode).ok()));
    }

    // iteratorForCondition
    {
      arangodb::transaction ::Methods trx(arangodb::transaction::StandaloneContext::Create(vocbase),
                                          {}, {}, {}, arangodb::transaction::Options());

      auto dummyPlan = arangodb::tests::planFromQuery(vocbase, "RETURN 1");

      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<arangodb::iresearch::ByExpression>().init(
          *dummyPlan, *ast,
          *filterNode->getMember(0)->getMember(0)  // d.a[*].b > 15
      );
      root.add<arangodb::iresearch::ByExpression>().init(
          *dummyPlan, *ast,
          *filterNode->getMember(0)->getMember(1)  // d.a[*].b < 40
      );
      
      ExpressionContextMock exprCtx;
      exprCtx.setTrx(&trx);

      irs::Or actual;
      arangodb::iresearch::QueryContext const ctx{&trx, dummyPlan.get(), ast,
                                                  &exprCtx,
                                                  &irs::sub_reader::empty(), ref};
      EXPECT_TRUE(
          (arangodb::iresearch::FilterFactory::filter(&actual, ctx, *filterNode).ok()));
      EXPECT_EQ(expected, actual);
    }
  }

  // expression is not supported by IResearch -> wrap it
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));

    std::string const refName = "d";
    std::string const queryString =
        "FOR d IN collection FILTER boost(d.a[*].b > 15, 0.5) and d.a[*].b < "
        "40 RETURN d";

    arangodb::aql::Query query(arangodb::transaction::StandaloneContext::Create(vocbase),
                               arangodb::aql::QueryString(queryString), nullptr,
                               std::make_shared<arangodb::velocypack::Builder>());

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
      arangodb::iresearch::QueryContext const ctx{nullptr, nullptr, nullptr, nullptr, nullptr, ref};
      EXPECT_TRUE(
          (arangodb::iresearch::FilterFactory::filter(nullptr, ctx, *filterNode).ok()));
    }

    // iteratorForCondition
    {
      arangodb::transaction ::Methods trx(arangodb::transaction::StandaloneContext::Create(vocbase),
                                          {}, {}, {}, arangodb::transaction::Options());

      auto dummyPlan = arangodb::tests::planFromQuery(vocbase, "RETURN 1");

      irs::Or expected;
      auto& root = expected.add<irs::And>();
      {
        auto& expr = root.add<arangodb::iresearch::ByExpression>();
        expr.boost(0.5);
        expr.init(*dummyPlan, *ast,
                  *filterNode->getMember(0)->getMember(0)->getMember(0)->getMember(0)  // d.a[*].b > 15
        );
      }
      root.add<arangodb::iresearch::ByExpression>().init(
          *dummyPlan, *ast,
          *filterNode->getMember(0)->getMember(1)  // d.a[*].b < 40
      );
      
      ExpressionContextMock exprCtx;
      exprCtx.setTrx(&trx);

      irs::Or actual;
      arangodb::iresearch::QueryContext const ctx{&trx, dummyPlan.get(), ast,
                                                  &exprCtx,
                                                  &irs::sub_reader::empty(), ref};
      EXPECT_TRUE(
          (arangodb::iresearch::FilterFactory::filter(&actual, ctx, *filterNode).ok()));
      EXPECT_EQ(expected, actual);
      assertFilterBoost(expected, actual);
    }
  }

  // numeric range with offset
  {
    irs::numeric_token_stream minTerm;
    minTerm.reset(15.);
    irs::numeric_token_stream maxTerm;
    maxTerm.reset(40.);

    irs::Or expected;
    auto& root = expected.add<irs::And>();
    {
      auto& filter = root.add<irs::by_granular_range>();
      *filter.mutable_field() = mangleNumeric("a.b[42].c");
      auto* opts = filter.mutable_options();
      irs::set_granular_term(opts->range.min, minTerm);
      opts->range.min_type = irs::BoundType::EXCLUSIVE;
    }
    {
      auto& filter = root.add<irs::by_granular_range>();
      *filter.mutable_field() = mangleNumeric("a.b[42].c");
      auto* opts = filter.mutable_options();
      irs::set_granular_term(opts->range.max, maxTerm);
      opts->range.max_type = irs::BoundType::EXCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a.b[42].c > 15 and d.a.b[42].c < 40 "
        "RETURN d",
        expected);
    assertFilterSuccess(vocbase(),
                        "FOR d IN collection FILTER d['a'].b[42].c > 15 and "
                        "d['a']['b'][42]['c'] < 40 RETURN d",
                        expected);
    assertFilterSuccess(vocbase(),
                        "FOR d IN collection FILTER d.a['b'][42]['c'] > 15 and "
                        "d['a']['b'][42]['c'] < 40 RETURN d",
                        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d['a'].b[42].c > 15 and d.a.b[42].c < 40 "
        "RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER 15 < d.a.b[42].c and d.a.b[42].c < 40 "
        "RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER 15 < d['a'].b[42].c and d.a.b[42].c < 40 "
        "RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a.b[42].c > 15 and 40 > d.a.b[42].c "
        "RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d['a']['b'][42]['c'] > 15 and 40 > "
        "d['a']['b'][42]['c'] RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER 15 < d.a.b[42].c and 40 > d.a.b[42].c "
        "RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a.b[42].c > 15.0 and d.a.b[42].c < 40.0 "
        "RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a['b'][42]['c'] > 15.0 and "
        "d.a['b'][42]['c'] < 40.0 RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER 15.0 < d.a.b[42].c and d.a.b[42].c < 40.0 "
        "RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a.b[42].c > 15.0 and 40.0 > d.a.b[42].c "
        "RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d['a']['b'][42]['c'] > 15.0 and 40.0 > "
        "d.a['b'][42]['c'] RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER 15.0 < d.a.b[42].c and 40.0 > d.a.b[42].c "
        "RETURN d",
        expected);
  }

  // numeric range
  {
    irs::numeric_token_stream minTerm;
    minTerm.reset(15.);
    irs::numeric_token_stream maxTerm;
    maxTerm.reset(40.);

    irs::Or expected;
    auto& root = expected.add<irs::And>();
    {
      auto& filter = root.add<irs::by_granular_range>();
      *filter.mutable_field() = mangleNumeric("a.b.c");
      auto* opts = filter.mutable_options();
      irs::set_granular_term(opts->range.min, minTerm);
      opts->range.min_type = irs::BoundType::INCLUSIVE;
    }
    {
      auto& filter = root.add<irs::by_granular_range>();
      *filter.mutable_field() = mangleNumeric("a.b.c");
      auto* opts = filter.mutable_options();
      irs::set_granular_term(opts->range.max, maxTerm);
      opts->range.max_type = irs::BoundType::EXCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a.b.c >= 15 and d.a.b.c < 40 RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a['b']['c'] >= 15 and d['a']['b']['c'] < "
        "40 RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER 15 <= d.a.b.c and d.a.b.c < 40 RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a.b.c >= 15 and 40 > d.a.b.c RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER 15 <= d.a.b.c and 40 > d.a.b.c RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER 15 <= d['a']['b']['c'] and 40 > d.a.b.c "
        "RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a.b.c >= 15.0 and d.a.b.c < 40.0 RETURN "
        "d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER 15.0 <= d.a['b']['c'] and d.a.b.c < 40.0 "
        "RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a.b.c >= 15.0 and 40.0 > d.a.b.c RETURN "
        "d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER 15.0 <= d.a.b.c and 40.0 > d.a.b.c RETURN "
        "d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER 15.0 <= d['a']['b']['c'] and 40.0 > "
        "d.a['b']['c'] RETURN d",
        expected);
  }

  // numeric range
  {
    irs::numeric_token_stream minTerm;
    minTerm.reset(15.);
    irs::numeric_token_stream maxTerm;
    maxTerm.reset(40.);

    irs::Or expected;
    auto& root = expected.add<irs::And>();
    {
      auto& filter = root.add<irs::by_granular_range>();
      *filter.mutable_field() = mangleNumeric("a.b.c");
      auto* opts = filter.mutable_options();
      irs::set_granular_term(opts->range.min, minTerm);
      opts->range.min_type = irs::BoundType::INCLUSIVE;
    }
    {
      auto& filter = root.add<irs::by_granular_range>();
      *filter.mutable_field() = mangleNumeric("a.b.c");
      auto* opts = filter.mutable_options();
      irs::set_granular_term(opts->range.max, maxTerm);
      opts->range.max_type = irs::BoundType::INCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a.b.c >= 15 and d.a.b.c <= 40 RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a['b']['c'] >= 15 and d.a.b.c <= 40 "
        "RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER 15 <= d.a.b.c and d.a.b.c <= 40 RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER 15 <= d['a']['b']['c'] and d.a['b']['c'] "
        "<= 40 RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a.b.c >= 15 and 40 >= d.a.b.c RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER 15 <= d.a.b.c and 40 >= d.a.b.c RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER 15 <= d['a']['b']['c'] and 40 >= "
        "d.a['b']['c'] RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a.b.c >= 15.0 and d.a.b.c <= 40.0 RETURN "
        "d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d['a'].b.c >= 15.0 and d['a']['b'].c <= "
        "40.0 RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER 15.0 <= d.a.b.c and d.a.b.c <= 40.0 RETURN "
        "d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a.b.c >= 15.0 and 40.0 >= d.a.b.c RETURN "
        "d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d['a']['b'].c >= 15.0 and 40.0 >= d.a.b.c "
        "RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER 15.0 <= d.a.b.c and 40.0 >= d.a.b.c RETURN "
        "d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER 15.0 <= d['a']['b']['c'] and 40.0 >= "
        "d.a.b.c RETURN d",
        expected);
  }

  // expression is not supported by IResearch -> wrap it
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));

    std::string const refName = "d";
    std::string const queryString =
        "FOR d IN collection FILTER d.a[*].b >= 15 and d.a[*].b <= 40 RETURN d";

    arangodb::aql::Query query(arangodb::transaction::StandaloneContext::Create(vocbase),
                               arangodb::aql::QueryString(queryString), nullptr,
                               std::make_shared<arangodb::velocypack::Builder>());

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
      arangodb::iresearch::QueryContext const ctx{nullptr, nullptr, nullptr, nullptr, nullptr, ref};
      EXPECT_TRUE(
          (arangodb::iresearch::FilterFactory::filter(nullptr, ctx, *filterNode).ok()));
    }

    // iteratorForCondition
    {
      arangodb::transaction ::Methods trx(arangodb::transaction::StandaloneContext::Create(vocbase),
                                          {}, {}, {}, arangodb::transaction::Options());

      auto dummyPlan = arangodb::tests::planFromQuery(vocbase, "RETURN 1");

      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<arangodb::iresearch::ByExpression>().init(
          *dummyPlan, *ast,
          *filterNode->getMember(0)->getMember(0)  // d.a[*].b >= 15
      );
      root.add<arangodb::iresearch::ByExpression>().init(
          *dummyPlan, *ast,
          *filterNode->getMember(0)->getMember(1)  // d.a[*].b <= 40
      );
      
      ExpressionContextMock exprCtx;
      exprCtx.setTrx(&trx);

      irs::Or actual;
      arangodb::iresearch::QueryContext const ctx{&trx, dummyPlan.get(), ast,
                                                  &exprCtx,
                                                  &irs::sub_reader::empty(), ref};
      EXPECT_TRUE(
          (arangodb::iresearch::FilterFactory::filter(&actual, ctx, *filterNode).ok()));
      EXPECT_EQ(expected, actual);
    }
  }

  // numeric range
  {
    irs::numeric_token_stream minTerm;
    minTerm.reset(15.);
    irs::numeric_token_stream maxTerm;
    maxTerm.reset(40.);

    irs::Or expected;
    auto& root = expected.add<irs::And>();
    {
      auto& filter = root.add<irs::by_granular_range>();
      *filter.mutable_field() = mangleNumeric("a.b.c");
      auto* opts = filter.mutable_options();
      irs::set_granular_term(opts->range.min, minTerm);
      opts->range.min_type = irs::BoundType::EXCLUSIVE;
    }
    {
      auto& filter = root.add<irs::by_granular_range>();
      *filter.mutable_field() = mangleNumeric("a.b.c");
      auto* opts = filter.mutable_options();
      irs::set_granular_term(opts->range.max, maxTerm);
      opts->range.max_type = irs::BoundType::INCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a.b.c > 15 and d.a.b.c <= 40 RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d['a'].b.c > 15 and d.a.b.c <= 40 RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER 15 < d.a.b.c and d.a.b.c <= 40 RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER 15 < d['a']['b']['c'] and d.a.b.c <= 40 "
        "RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER 15 < d.a.b.c and d.a.b.c <= 40 RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d['a']['b']['c'] > 15 and 40 >= "
        "d['a']['b']['c'] RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER 15 < d.a.b.c and 40 >= d.a.b.c RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER 15 < d['a']['b'].c and 40 >= d.a['b']['c'] "
        "RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a.b.c > 15.0 and d.a.b.c <= 40.0 RETURN "
        "d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER 15.0 < d.a.b.c and d.a.b.c <= 40.0 RETURN "
        "d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER 15.0 < d['a']['b'].c and d['a']['b']['c'] "
        "<= 40.0 RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a.b.c > 15.0 and 40.0 >= d.a.b.c RETURN "
        "d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER 15.0 < d.a.b.c and 40.0 >= d.a.b.c RETURN "
        "d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER 15.0 < d['a']['b'].c and 40.0 >= d.a.b.c "
        "RETURN d",
        expected);
  }

  // expression is not supported by IResearch -> wrap it
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));

    std::string const refName = "d";
    std::string const queryString =
        "FOR d IN collection FILTER d.a[*].b > 15 and d.a[*].b <= 40 RETURN d";

    arangodb::aql::Query query(arangodb::transaction::StandaloneContext::Create(vocbase),
                               arangodb::aql::QueryString(queryString), nullptr,
                               std::make_shared<arangodb::velocypack::Builder>());

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
      arangodb::iresearch::QueryContext const ctx{nullptr, nullptr, nullptr, nullptr, nullptr, ref};
      EXPECT_TRUE(
          (arangodb::iresearch::FilterFactory::filter(nullptr, ctx, *filterNode).ok()));
    }

    // iteratorForCondition
    {
      arangodb::transaction ::Methods trx(arangodb::transaction::StandaloneContext::Create(vocbase),
                                          {}, {}, {}, arangodb::transaction::Options());

      auto dummyPlan = arangodb::tests::planFromQuery(vocbase, "RETURN 1");

      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<arangodb::iresearch::ByExpression>().init(
          *dummyPlan, *ast,
          *filterNode->getMember(0)->getMember(0)  // d.a[*].b >= 15
      );
      root.add<arangodb::iresearch::ByExpression>().init(
          *dummyPlan, *ast,
          *filterNode->getMember(0)->getMember(1)  // d.a[*].b <= 40
      );
      
      ExpressionContextMock exprCtx;
      exprCtx.setTrx(&trx);

      irs::Or actual;
      arangodb::iresearch::QueryContext const ctx{&trx, dummyPlan.get(), ast,
                                                  &exprCtx,
                                                  &irs::sub_reader::empty(), ref};
      EXPECT_TRUE(
          (arangodb::iresearch::FilterFactory::filter(&actual, ctx, *filterNode).ok()));
      EXPECT_EQ(expected, actual);
    }
  }

  // dynamic complex attribute field in string range
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                                      arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                                      arangodb::aql::AqlValueHintDouble{5.6})));

    irs::numeric_token_stream minTerm;
    minTerm.reset(15.);
    irs::numeric_token_stream maxTerm;
    maxTerm.reset(40.);

    irs::Or expected;
    auto& root = expected.add<irs::And>();
    {
      auto& filter = root.add<irs::by_granular_range>();
      *filter.mutable_field() = mangleNumeric("a.b.c.e[4].f[5].g[3].g.a");
      auto* opts = filter.mutable_options();
      irs::set_granular_term(opts->range.min, minTerm);
      opts->range.min_type = irs::BoundType::EXCLUSIVE;
    }
    {
      auto& filter = root.add<irs::by_granular_range>();
      *filter.mutable_field() = mangleNumeric("a.b.c.e[4].f[5].g[3].g.a");
      auto* opts = filter.mutable_options();
      irs::set_granular_term(opts->range.max, maxTerm);
      opts->range.max_type = irs::BoundType::INCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(),
        "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "> 15 &&  "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        " <= 40 RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER 15 < "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "&&  40 >= "
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
        "> 15 &&  "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        " <= 40 RETURN d",
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
        "> 15 &&  "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        " <= 40 RETURN d",
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
        "> 15 &&  "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        " <= 40 RETURN d",
        &ctx);
  }

  // string range
  {
    irs::Or expected;
    auto& root = expected.add<irs::And>();
    {
      auto& filter = root.add<irs::by_range>();
      *filter.mutable_field() = mangleStringIdentity("a.b.c");
      auto* opts = filter.mutable_options();
      opts->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("15"));
      opts->range.min_type = irs::BoundType::EXCLUSIVE;
    }
    {
      auto& filter = root.add<irs::by_range>();
      *filter.mutable_field() = mangleStringIdentity("a.b.c");
      auto* opts = filter.mutable_options();
      opts->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("40"));
      opts->range.max_type = irs::BoundType::EXCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a.b.c > '15' and d.a.b.c < '40' RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d['a']['b']['c'] > '15' and d.a.b.c < '40' "
        "RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER '15' < d.a.b.c and d.a.b.c < '40' RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER '15' < d['a']['b'].c and d['a']['b']['c'] "
        "< '40' RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a.b.c > '15' and '40' > d.a.b.c RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a['b']['c'] > '15' and '40' > "
        "d['a']['b'].c RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER '15' < d.a.b.c and '40' > d.a.b.c RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER '15' < d.a.b.c and '40' > d.a['b']['c'] "
        "RETURN d",
        expected);
  }

  // string range
  {
    irs::Or expected;
    auto& root = expected.add<irs::And>();
    {
      auto& filter = root.add<irs::by_range>();
      *filter.mutable_field() = mangleStringIdentity("a.b.c");
      auto* opts = filter.mutable_options();
      opts->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("15"));
      opts->range.min_type = irs::BoundType::INCLUSIVE;
    }
    {
      auto& filter = root.add<irs::by_range>();
      *filter.mutable_field() = mangleStringIdentity("a.b.c");
      auto* opts = filter.mutable_options();
      opts->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("40"));
      opts->range.max_type = irs::BoundType::EXCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a.b.c >= '15' and d.a.b.c < '40' RETURN "
        "d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d['a']['b'].c >= '15' and d['a']['b']['c'] "
        "< '40' RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d['a']['b'].c >= '15' and d.a.b.c < '40' "
        "RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER '15' <= d.a.b.c and d.a.b.c < '40' RETURN "
        "d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a.b.c >= '15' and '40' > d.a.b.c RETURN "
        "d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a['b']['c'] >= '15' and '40' > d.a.b.c "
        "RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER '15' <= d.a.b.c and '40' > d.a.b.c RETURN "
        "d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER '15' <= d['a']['b']['c'] and '40' > "
        "d.a['b']['c'] RETURN d",
        expected);
  }

  // string range, boost, analyzer
  {
    irs::Or expected;
    auto& root = expected.add<irs::And>();
    root.boost(0.5);
    {
      auto& filter = root.add<irs::by_range>();
      *filter.mutable_field() = mangleString("a.b.c", "test_analyzer");
      auto* opts = filter.mutable_options();
      opts->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("15"));
      opts->range.min_type = irs::BoundType::INCLUSIVE;
    }
    {
      auto& filter = root.add<irs::by_range>();
      *filter.mutable_field() = mangleString("a.b.c", "test_analyzer");
      auto* opts = filter.mutable_options();
      opts->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("40"));
      opts->range.max_type = irs::BoundType::EXCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER analyzer(boost(d.a.b.c >= '15' and d.a.b.c "
        "< '40', 0.5), 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER boost(analyzer(d['a']['b'].c >= '15' and "
        "d['a']['b']['c'] < '40', 'test_analyzer'), 0.5) RETURN d",
        expected);
  }

  // string range
  {
    irs::Or expected;
    auto& root = expected.add<irs::And>();
    {
      auto& filter = root.add<irs::by_range>();
      *filter.mutable_field() = mangleStringIdentity("a.b.c");
      auto* opts = filter.mutable_options();
      opts->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("15"));
      opts->range.min_type = irs::BoundType::INCLUSIVE;
    }
    {
      auto& filter = root.add<irs::by_range>();
      *filter.mutable_field() = mangleStringIdentity("a.b.c");
      auto* opts = filter.mutable_options();
      opts->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("40"));
      opts->range.max_type = irs::BoundType::INCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a.b.c >= '15' and d.a.b.c <= '40' RETURN "
        "d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d['a']['b']['c'] >= '15' and d.a.b.c <= "
        "'40' RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER '15' <= d.a.b.c and d.a.b.c <= '40' RETURN "
        "d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER '15' <= d['a']['b'].c and d.a['b']['c'] <= "
        "'40' RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a.b.c >= '15' and '40' >= d.a.b.c RETURN "
        "d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER '15' <= d.a.b.c and '40' >= d.a.b.c RETURN "
        "d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER '15' <= d['a'].b.c and '40' >= "
        "d['a']['b'].c RETURN d",
        expected);
  }

  // string range, boost
  {
    irs::Or expected;
    auto& root = expected.add<irs::And>();
    {
      auto& filter = root.add<irs::by_range>();
      filter.boost(0.5);
      *filter.mutable_field() = mangleStringIdentity("a.b.c");
      auto* opts = filter.mutable_options();
      opts->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("15"));
      opts->range.min_type = irs::BoundType::INCLUSIVE;
    }
    {
      auto& filter = root.add<irs::by_range>();
      filter.boost(0.5);
      *filter.mutable_field() = mangleStringIdentity("a.b.c");
      auto* opts = filter.mutable_options();
      opts->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("40"));
      opts->range.max_type = irs::BoundType::INCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER boost(d.a.b.c >= '15', 0.5) and "
        "boost(d.a.b.c <= '40', 0.5) RETURN d",
        expected);
  }

  // string range, boost, analyzer
  {
    irs::Or expected;
    auto& root = expected.add<irs::And>();
    {
      auto& filter = root.add<irs::by_range>();
      filter.boost(0.5);
      *filter.mutable_field() = mangleString("a.b.c", "test_analyzer");
      auto* opts = filter.mutable_options();
      opts->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("15"));
      opts->range.min_type = irs::BoundType::INCLUSIVE;
    }
    {
      auto& filter = root.add<irs::by_range>();
      filter.boost(0.5);
      *filter.mutable_field() = mangleString("a.b.c", "test_analyzer");
      auto* opts = filter.mutable_options();
      opts->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("40"));
      opts->range.max_type = irs::BoundType::INCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER analyzer(boost(d.a.b.c >= '15', 0.5) and "
        "boost(d.a.b.c <= '40', 0.5), 'test_analyzer') RETURN d",
        expected);
  }

  // string range, boost, analyzer
  {
    irs::Or expected;
    auto& root = expected.add<irs::And>();
    root.boost(0.5);
    {
      auto& filter = root.add<irs::by_range>();
      *filter.mutable_field() = mangleString("a.b.c", "test_analyzer");
      auto* opts = filter.mutable_options();
      opts->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("15"));
      opts->range.min_type = irs::BoundType::INCLUSIVE;
    }
    {
      auto& filter = root.add<irs::by_range>();
      *filter.mutable_field() = mangleString("a.b.c", "test_analyzer");
      auto* opts = filter.mutable_options();
      opts->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("40"));
      opts->range.max_type = irs::BoundType::INCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER boost(analyzer(d.a.b.c >= '15', "
        "'test_analyzer') and analyzer(d.a.b.c <= '40', 'test_analyzer'), 0.5) "
        "RETURN d",
        expected);
  }

  // string range
  {
    irs::Or expected;
    auto& root = expected.add<irs::And>();
    {
      auto& filter = root.add<irs::by_range>();
      *filter.mutable_field() = mangleStringIdentity("a.b.c");
      auto* opts = filter.mutable_options();
      opts->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("15"));
      opts->range.min_type = irs::BoundType::EXCLUSIVE;
    }
    {
      auto& filter = root.add<irs::by_range>();
      *filter.mutable_field() = mangleStringIdentity("a.b.c");
      auto* opts = filter.mutable_options();
      opts->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("40"));
      opts->range.max_type = irs::BoundType::INCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a.b.c > '15' and d.a.b.c <= '40' RETURN "
        "d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a.b.c > '15' and d.a.b.c <= '40' RETURN "
        "d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER '15' < d.a.b.c and d.a.b.c <= '40' RETURN "
        "d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER '15' < d['a'].b.c and d['a'].b.c <= '40' "
        "RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a.b.c > '15' and '40' >= d.a.b.c RETURN "
        "d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d['a']['b']['c'] > '15' and '40' >= "
        "d.a.b.c RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER '15' < d.a.b.c and '40' >= d.a.b.c RETURN "
        "d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER '15' < d['a']['b'].c and '40' >= "
        "d['a']['b']['c'] RETURN d",
        expected);
  }

  // string expression in range
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("numVal", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt(2)));

    irs::Or expected;
    auto& root = expected.add<irs::And>();
    {
      auto& filter = root.add<irs::by_range>();
      *filter.mutable_field() = mangleStringIdentity("a.b.c.e.f");
      auto* opts = filter.mutable_options();
      opts->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("15"));
      opts->range.min_type = irs::BoundType::EXCLUSIVE;
    }
    {
      auto& filter = root.add<irs::by_range>();
      *filter.mutable_field() = mangleStringIdentity("a.b.c.e.f");
      auto* opts = filter.mutable_options();
      opts->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("40"));
      opts->range.max_type = irs::BoundType::INCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(),
        "LET numVal=2 FOR d IN collection FILTER d.a.b.c.e.f > "
        "TO_STRING(numVal+13) && d.a.b.c.e.f <= TO_STRING(numVal+38) RETURN d",
        expected,
        &ctx  // expression context
    );

    assertFilterSuccess(
        vocbase(),
        "LET numVal=2 FOR d IN collection FILTER TO_STRING(numVal+13) < "
        "d.a.b.c.e.f  && d.a.b.c.e.f <= TO_STRING(numVal+38) RETURN d",
        expected,
        &ctx  // expression context
    );
  }

  // string expression in range, boost, analyzer
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("numVal", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt(2)));

    irs::Or expected;
    auto& root = expected.add<irs::And>();
    root.boost(2.f);
    {
      auto& filter = root.add<irs::by_range>();
      *filter.mutable_field() = mangleString("a.b.c.e.f", "test_analyzer");
      auto* opts = filter.mutable_options();
      opts->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("15"));
      opts->range.min_type = irs::BoundType::EXCLUSIVE;
    }
    {
      auto& filter = root.add<irs::by_range>();
      *filter.mutable_field() = mangleString("a.b.c.e.f", "test_analyzer");
      auto* opts = filter.mutable_options();
      opts->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("40"));
      opts->range.max_type = irs::BoundType::INCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(),
        "LET numVal=2 FOR d IN collection FILTER boost(analyzer(d.a.b.c.e.f > "
        "TO_STRING(numVal+13) && d.a.b.c.e.f <= TO_STRING(numVal+38), "
        "'test_analyzer'), numVal) RETURN d",
        expected,
        &ctx  // expression context
    );

    assertFilterSuccess(
        vocbase(),
        "LET numVal=2 FOR d IN collection FILTER "
        "analyzer(boost(TO_STRING(numVal+13) < d.a.b.c.e.f  && d.a.b.c.e.f <= "
        "TO_STRING(numVal+38), numVal), 'test_analyzer') RETURN d",
        expected,
        &ctx  // expression context
    );
  }

  // dynamic complex attribute field in string range
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                                      arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                                      arangodb::aql::AqlValueHintDouble{5.6})));

    irs::Or expected;
    auto& root = expected.add<irs::And>();
    {
      auto& filter = root.add<irs::by_range>();
      *filter.mutable_field() = mangleStringIdentity("a.b.c.e[4].f[5].g[3].g.a");
      auto* opts = filter.mutable_options();
      opts->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("15"));
      opts->range.min_type = irs::BoundType::EXCLUSIVE;
    }
    {
      auto& filter = root.add<irs::by_range>();
      *filter.mutable_field() = mangleStringIdentity("a.b.c.e[4].f[5].g[3].g.a");
      auto* opts = filter.mutable_options();
      opts->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("40"));
      opts->range.max_type = irs::BoundType::INCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(),
        "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "> '15' && "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        " <= '40' RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER '15' < "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "&& '40' >= "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "RETURN d",
        expected, &ctx);
  }

  // dynamic complex attribute field in string range
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                                      arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                                      arangodb::aql::AqlValueHintDouble{5.6})));

    irs::Or expected;
    auto& root = expected.add<irs::And>();
    {
      auto& filter = root.add<irs::by_range>();
      *filter.mutable_field() = mangleStringIdentity("a.b.c.e.f[5].g[3].g.a");
      auto* opts = filter.mutable_options();
      opts->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("15"));
      opts->range.min_type = irs::BoundType::EXCLUSIVE;
    }
    {
      auto& filter = root.add<irs::by_range>();
      *filter.mutable_field() = mangleStringIdentity("a.b.c.e[4].f[5].g[3].g.a");
      auto* opts = filter.mutable_options();
      opts->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("40"));
      opts->range.max_type = irs::BoundType::INCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(),
        "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER "
        "d[a].b[c].e.f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] > '15' && "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        " <= '40' RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER '15' < "
        "d[a].b[c].e.f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] && '40' >= "
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
        "> '15' &&  "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        " <= '40' RETURN d",
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
        "> '15' &&  "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        " <= '40' RETURN d",
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
        "> '15' &&  "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        " <= '40' RETURN d",
        &ctx);
  }

  // heterogeneous range
  {
    irs::numeric_token_stream maxTerm;
    maxTerm.reset(40.);

    irs::Or expected;
    auto& root = expected.add<irs::And>();
    {
      auto& filter = root.add<irs::by_range>();
      *filter.mutable_field() = mangleStringIdentity("a.b.c");
      auto* opts = filter.mutable_options();
      opts->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("15"));
      opts->range.min_type = irs::BoundType::INCLUSIVE;
    }
    {
      auto& filter = root.add<irs::by_granular_range>();
      *filter.mutable_field() = mangleNumeric("a.b.c");
      auto* opts = filter.mutable_options();
      irs::set_granular_term(opts->range.max, maxTerm);
      opts->range.max_type = irs::BoundType::EXCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a.b.c >= '15' and d.a.b.c < 40 RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d['a']['b'].c >= '15' and d['a']['b'].c < "
        "40 RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d['a']['b']['c'] >= '15' and d.a.b.c < 40 "
        "RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER '15' <= d.a.b.c and d.a.b.c < 40 RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a.b.c >= '15' and 40 > d.a.b.c RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d['a']['b'].c >= '15' and 40 > "
        "d['a']['b'].c RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d['a'].b.c >= '15' and 40 > d.a.b.c RETURN "
        "d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER '15' <= d.a.b.c and 40 > d.a.b.c RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a.b.c >= '15' and d.a.b.c < 40.0 RETURN "
        "d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d['a']['b']['c'] >= '15' and "
        "d['a']['b']['c'] < 40.0 RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER '15' <= d.a.b.c and d.a.b.c < 40.0 RETURN "
        "d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a.b.c >= '15' and 40.0 > d.a.b.c RETURN "
        "d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d['a'].b.c >= '15' and 40.0 > "
        "d['a']['b'].c RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER '15' <= d.a.b.c and 40.0 > d.a.b.c RETURN "
        "d",
        expected);
  }

  // heterogeneous range, boost, analyzer
  {
    irs::numeric_token_stream maxTerm;
    maxTerm.reset(40.);

    irs::Or expected;
    auto& root = expected.add<irs::And>();
    root.boost(1.5);
    {
      auto& filter = root.add<irs::by_range>();
      *filter.mutable_field() = mangleString("a.b.c", "test_analyzer");
      auto* opts = filter.mutable_options();
      opts->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("15"));
      opts->range.min_type = irs::BoundType::INCLUSIVE;
    }
    {
      auto& filter = root.add<irs::by_granular_range>();
      *filter.mutable_field() = mangleNumeric("a.b.c");
      auto* opts = filter.mutable_options();
      irs::set_granular_term(opts->range.max, maxTerm);
      opts->range.max_type = irs::BoundType::EXCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER boost(analyzer(d.a.b.c >= '15' and d.a.b.c "
        "< 40, 'test_analyzer'), 1.5) RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER analyzer(boost('15' <= d.a.b.c and 40.0 > "
        "d.a.b.c, 1.5), 'test_analyzer') RETURN d",
        expected);
  }

  // heterogeneous expression
  {
    irs::numeric_token_stream maxTerm;
    maxTerm.reset(40.);

    ExpressionContextMock ctx;
    ctx.vars.emplace("numVal", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt(2)));

    irs::Or expected;
    auto& root = expected.add<irs::And>();
    {
      auto& filter = root.add<irs::by_range>();
      *filter.mutable_field() = mangleStringIdentity("a.b.c.e.f");
      auto* opts = filter.mutable_options();
      opts->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("15"));
      opts->range.min_type = irs::BoundType::INCLUSIVE;
    }
    {
      auto& filter = root.add<irs::by_granular_range>();
      *filter.mutable_field() = mangleNumeric("a.b.c.e.f");
      auto* opts = filter.mutable_options();
      irs::set_granular_term(opts->range.max, maxTerm);
      opts->range.max_type = irs::BoundType::EXCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(),
        "LET numVal=2 FOR d IN collection FILTER d.a.b.c.e.f >= "
        "TO_STRING(numVal+13) && d.a.b.c.e.f < (numVal+38) RETURN d",
        expected,
        &ctx  // expression context
    );

    assertFilterSuccess(
        vocbase(),
        "LET numVal=2 FOR d IN collection FILTER TO_STRING(numVal+13) <= "
        "d.a.b.c.e.f  && d.a.b.c.e.f < (numVal+38) RETURN d",
        expected,
        &ctx  // expression context
    );
  }

  // heterogeneous numeric range
  {
    irs::numeric_token_stream minTerm;
    minTerm.reset(15.5);
    irs::numeric_token_stream maxTerm;
    maxTerm.reset(40.);

    irs::Or expected;
    auto& root = expected.add<irs::And>();
    {
      auto& filter = root.add<irs::by_granular_range>();
      *filter.mutable_field() = mangleNumeric("a.b.c");
      auto* opts = filter.mutable_options();
      irs::set_granular_term(opts->range.min, minTerm);
      opts->range.min_type = irs::BoundType::INCLUSIVE;
    }
    {
      auto& filter = root.add<irs::by_granular_range>();
      *filter.mutable_field() = mangleNumeric("a.b.c");
      auto* opts = filter.mutable_options();
      irs::set_granular_term(opts->range.max, maxTerm);
      opts->range.max_type = irs::BoundType::EXCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a.b.c >= 15.5 and d.a.b.c < 40 RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d['a']['b'].c >= 15.5 and d['a']['b'].c < "
        "40 RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d['a']['b']['c'] >= 15.5 and d.a.b.c < 40 "
        "RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER 15.5 <= d.a.b.c and d.a.b.c < 40 RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a.b.c >= 15.5 and 40 > d.a.b.c RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d['a']['b'].c >= 15.5 and 40 > "
        "d['a']['b'].c RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d['a'].b.c >= 15.5 and 40 > d.a.b.c RETURN "
        "d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER 15.5 <= d.a.b.c and 40 > d.a.b.c RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a.b.c >= 15.5 and d.a.b.c < 40.0 RETURN "
        "d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d['a']['b']['c'] >= 15.5 and "
        "d['a']['b']['c'] < 40.0 RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER 15.5 <= d.a.b.c and d.a.b.c < 40.0 RETURN "
        "d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a.b.c >= 15.5 and 40.0 > d.a.b.c RETURN "
        "d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d['a'].b.c >= 15.5 and 40.0 > "
        "d['a']['b'].c RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER 15.5 <= d.a.b.c and 40.0 > d.a.b.c RETURN "
        "d",
        expected);
  }

  // heterogeneous range
  {
    irs::numeric_token_stream minTerm;
    minTerm.reset(15.);
    irs::numeric_token_stream maxTerm;
    maxTerm.reset(40.);

    irs::Or expected;
    auto& root = expected.add<irs::And>();
    {
      auto& filter = root.add<irs::by_granular_range>();
      *filter.mutable_field() = mangleNumeric("a.b.c");
      auto* opts = filter.mutable_options();
      irs::set_granular_term(opts->range.min, minTerm);
      opts->range.min_type = irs::BoundType::EXCLUSIVE;
    }
    {
      auto& filter = root.add<irs::by_range>();
      *filter.mutable_field() = mangleStringIdentity("a.b.c");
      auto* opts = filter.mutable_options();
      opts->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("40"));
      opts->range.max_type = irs::BoundType::INCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a.b.c > 15 and d.a.b.c <= '40' RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d['a']['b'].c > 15 and d['a']['b'].c <= "
        "'40' RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d['a'].b.c > 15 and d.a.b.c <= '40' RETURN "
        "d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER 15 < d.a.b.c and d.a.b.c <= '40' RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a.b.c > 15 and '40' >= d.a.b.c RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d['a']['b']['c'] > 15 and '40' >= "
        "d['a']['b'].c RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER 15 < d.a.b.c and '40' >= d.a.b.c RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a.b.c > 15.0 and d.a.b.c <= '40' RETURN "
        "d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d['a']['b']['c'] > 15.0 and d.a.b.c <= "
        "'40' RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER 15.0 < d.a.b.c and d.a.b.c <= '40' RETURN "
        "d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a.b.c > 15.0 and '40' >= d.a.b.c RETURN "
        "d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER 15.0 < d.a.b.c and '40' >= d.a.b.c RETURN "
        "d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER 15.0 < d['a'].b.c and '40' >= d.a.b.c "
        "RETURN d",
        expected);
  }

  // heterogeneous range
  {
    irs::numeric_token_stream maxTerm;
    maxTerm.reset(40.);

    irs::Or expected;
    auto& root = expected.add<irs::And>();
    {
      auto& filter = root.add<irs::by_range>();
      *filter.mutable_field() = mangleBool("a.b.c");
      auto* opts = filter.mutable_options();
      opts->range.min = irs::ref_cast<irs::byte_type>(irs::boolean_token_stream::value_false());
      opts->range.min_type = irs::BoundType::INCLUSIVE;
    }
    {
      auto& filter = root.add<irs::by_granular_range>();
      *filter.mutable_field() = mangleNumeric("a.b.c");
      auto* opts = filter.mutable_options();
      irs::set_granular_term(opts->range.max, maxTerm);
      opts->range.max_type = irs::BoundType::INCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a.b.c >= false and d.a.b.c <= 40 RETURN "
        "d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d['a'].b.c >= false and d.a.b.c <= 40 "
        "RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER false <= d.a.b.c and d.a.b.c <= 40 RETURN "
        "d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER false <= d.a['b']['c'] and d.a['b']['c'] "
        "<= 40 RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a.b.c >= false and 40 >= d.a.b.c RETURN "
        "d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER false <= d.a.b.c and 40 >= d.a.b.c RETURN "
        "d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER false <= d['a']['b']['c'] and 40 >= "
        "d.a.b.c RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a.b.c >= false and d.a.b.c <= 40.0 "
        "RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER false <= d.a.b.c and d.a.b.c <= 40.0 "
        "RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER false <= d.a['b']['c'] and d.a.b.c <= 40.0 "
        "RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER analyzer(d.a.b.c >= false and 40.0 >= "
        "d.a.b.c, 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a['b']['c'] >= false and 40.0 >= d.a.b.c "
        "RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER false <= d.a.b.c and 40.0 >= d.a.b.c "
        "RETURN d",
        expected);
  }

  // heterogeneous range, boost
  {
    irs::numeric_token_stream maxTerm;
    maxTerm.reset(40.);

    irs::Or expected;
    auto& root = expected.add<irs::And>();
    root.boost(1.5);
    {
      auto& filter = root.add<irs::by_range>();
      *filter.mutable_field() = mangleBool("a.b.c");
      auto* opts = filter.mutable_options();
      opts->range.min = irs::ref_cast<irs::byte_type>(irs::boolean_token_stream::value_false());
      opts->range.min_type = irs::BoundType::INCLUSIVE;
    }
    {
      auto& filter = root.add<irs::by_granular_range>();
      *filter.mutable_field() = mangleNumeric("a.b.c");
      auto* opts = filter.mutable_options();
      irs::set_granular_term(opts->range.max, maxTerm);
      opts->range.max_type = irs::BoundType::INCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER boost(d.a.b.c >= false and d.a.b.c <= 40, "
        "1.5) RETURN d",
        expected);
  }

  // heterogeneous range, boost
  {
    irs::numeric_token_stream maxTerm;
    maxTerm.reset(40.);

    irs::Or expected;
    auto& root = expected.add<irs::And>();
    {
      auto& filter = root.add<irs::by_range>();
      filter.boost(1.5);
      *filter.mutable_field() = mangleBool("a.b.c");
      auto* opts = filter.mutable_options();
      opts->range.min = irs::ref_cast<irs::byte_type>(irs::boolean_token_stream::value_false());
      opts->range.min_type = irs::BoundType::INCLUSIVE;
    }
    {
      auto& filter = root.add<irs::by_granular_range>();
      filter.boost(0.5);
      *filter.mutable_field() = mangleNumeric("a.b.c");
      auto* opts = filter.mutable_options();
      irs::set_granular_term(opts->range.max, maxTerm);
      opts->range.max_type = irs::BoundType::INCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER boost(d.a.b.c >= false, 1.5) and "
        "boost(d.a.b.c <= 40, 0.5) RETURN d",
        expected);
  }

  // heterogeneous range
  {
    irs::numeric_token_stream maxTerm;
    maxTerm.reset(40.5);

    irs::Or expected;
    auto& root = expected.add<irs::And>();
    {
      auto& filter = root.add<irs::by_range>();
      *filter.mutable_field() = mangleNull("a.b.c");
      auto* opts = filter.mutable_options();
      opts->range.min = irs::ref_cast<irs::byte_type>(irs::null_token_stream::value_null());
      opts->range.min_type = irs::BoundType::EXCLUSIVE;
    }
    {
      auto& filter = root.add<irs::by_granular_range>();
      *filter.mutable_field() = mangleNumeric("a.b.c");
      auto* opts = filter.mutable_options();
      irs::set_granular_term(opts->range.max, maxTerm);
      opts->range.max_type = irs::BoundType::INCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a.b.c > null and d.a.b.c <= 40.5 RETURN "
        "d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a['b']['c'] > null and d.a.b.c <= 40.5 "
        "RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER null < d.a.b.c and d.a.b.c <= 40.5 RETURN "
        "d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER null < d['a']['b']['c'] and d.a.b.c <= "
        "40.5 RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a.b.c > null and 40.5 >= d.a.b.c RETURN "
        "d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a['b']['c'] > null and 40.5 >= "
        "d.a['b']['c'] RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER null < d.a.b.c and 40.5 >= d.a.b.c RETURN "
        "d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER analyzer(null < d['a']['b']['c'] and 40.5 "
        ">= d['a']['b']['c'], 'test_analyzer') RETURN d",
        expected);
  }

  // heterogeneous range, boost
  {
    irs::numeric_token_stream maxTerm;
    maxTerm.reset(40.5);

    irs::Or expected;
    auto& root = expected.add<irs::And>();
    {
      auto& filter = root.add<irs::by_range>();
      filter.boost(1.5);
      *filter.mutable_field() = mangleNull("a.b.c");
      auto* opts = filter.mutable_options();
      opts->range.min = irs::ref_cast<irs::byte_type>(irs::null_token_stream::value_null());
      opts->range.min_type = irs::BoundType::EXCLUSIVE;
    }
    {
      auto& filter = root.add<irs::by_granular_range>();
      *filter.mutable_field() = mangleNumeric("a.b.c");
      auto* opts = filter.mutable_options();
      irs::set_granular_term(opts->range.max, maxTerm);
      opts->range.max_type = irs::BoundType::INCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER boost(d.a.b.c > null, 1.5) and d.a.b.c <= "
        "40.5 RETURN d",
        expected);
  }

  // range with different references
  {
    irs::numeric_token_stream maxTerm;
    maxTerm.reset(40.);

    irs::Or expected;
    auto& root = expected.add<irs::And>();
    {
      auto& filter = root.add<irs::by_range>();
      *filter.mutable_field() = mangleStringIdentity("a.b.c");
      auto* opts = filter.mutable_options();
      opts->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("15"));
      opts->range.min_type = irs::BoundType::INCLUSIVE;
    }
    {
      auto& filter = root.add<irs::by_granular_range>();
      *filter.mutable_field() = mangleNumeric("a.b.c");
      auto* opts = filter.mutable_options();
      irs::set_granular_term(opts->range.max, maxTerm);
      opts->range.max_type = irs::BoundType::EXCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a.b.c >= '15' and d.a.b.c < 40 RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d['a']['b']['c'] >= '15' and d.a.b.c < 40 "
        "RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER '15' <= d.a.b.c and d.a.b.c < 40 RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER '15' <= d.a['b']['c'] and d.a.b.c < 40 "
        "RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a.b.c >= '15' and 40 > d.a.b.c RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d['a'].b.c >= '15' and 40 > d.a.b.c RETURN "
        "d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER '15' <= d.a.b.c and 40 > d.a.b.c RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER '15' <= d.a['b']['c'] and 40 > "
        "d.a['b']['c'] RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a.b.c >= '15' and d.a.b.c < 40.0 RETURN "
        "d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d['a']['b']['c'] >= '15' and d.a.b.c < "
        "40.0 RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER '15' <= d.a.b.c and d.a.b.c < 40.0 RETURN "
        "d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER '15' <= d['a'].b.c and d['a']['b']['c'] < "
        "40.0 RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a.b.c >= '15' and 40.0 > d.a.b.c RETURN "
        "d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER '15' <= d.a.b.c and 40.0 > d.a.b.c RETURN "
        "d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER '15' <= d.a['b']['c'] and 40.0 > d.a.b.c "
        "RETURN d",
        expected);
  }

  // range with different references
  {
    irs::numeric_token_stream maxTerm;
    maxTerm.reset(40.);

    irs::Or expected;
    auto& root = expected.add<irs::And>();
    root.boost(0.5);
    {
      auto& filter = root.add<irs::by_range>();
      filter.boost(0.5);
      *filter.mutable_field() = mangleStringIdentity("a.b.c");
      auto* opts = filter.mutable_options();
      opts->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("15"));
      opts->range.min_type = irs::BoundType::INCLUSIVE;
    }
    {
      auto& filter = root.add<irs::by_granular_range>();
      filter.boost(1.5);
      *filter.mutable_field() = mangleNumeric("a.b.c");
      auto* opts = filter.mutable_options();
      irs::set_granular_term(opts->range.max, maxTerm);
      opts->range.max_type = irs::BoundType::EXCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER boost(boost(d.a.b.c >= '15', 0.5) and "
        "boost(d.a.b.c < 40, 1.5), 0.5) RETURN d",
        expected);
  }

  // range with different references
  {
    irs::numeric_token_stream minTerm;
    minTerm.reset(15.);
    irs::numeric_token_stream maxTerm;
    maxTerm.reset(40.);

    irs::Or expected;
    auto& root = expected.add<irs::And>();
    {
      auto& filter = root.add<irs::by_granular_range>();
      *filter.mutable_field() = mangleNumeric("a.b.c");
      auto* opts = filter.mutable_options();
      irs::set_granular_term(opts->range.min, minTerm);
      opts->range.min_type = irs::BoundType::EXCLUSIVE;
    }
    {
      auto& filter = root.add<irs::by_range>();
      *filter.mutable_field() = mangleStringIdentity("a.b.c");
      auto* opts = filter.mutable_options();
      opts->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("40"));
      opts->range.max_type = irs::BoundType::INCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a.b.c > 15 and d.a.b.c <= '40' RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a['b']['c'] > 15 and d.a.b.c <= '40' "
        "RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER 15 < d.a.b.c and d.a.b.c <= '40' RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER 15 < d['a']['b']['c'] and d.a.b.c <= '40' "
        "RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a.b.c > 15 and '40' >= d.a.b.c RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a['b']['c'] > 15 and '40' >= "
        "d['a']['b']['c'] RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER 15 < d.a.b.c and '40' >= d.a.b.c RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a.b.c > 15.0 and d.a.b.c <= '40' RETURN "
        "d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a['b']['c'] > 15.0 and d['a']['b']['c'] "
        "<= '40' RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER 15.0 < d.a.b.c and d.a.b.c <= '40' RETURN "
        "d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a.b.c > 15.0 and '40' >= d.a.b.c RETURN "
        "d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a['b']['c'] > 15.0 and '40' >= d.a.b.c "
        "RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER 15.0 < d.a.b.c and '40' >= d.a.b.c RETURN "
        "d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER 15.0 < d['a']['b']['c'] and '40' >= "
        "d.a.b.c RETURN d",
        expected);
  }

  // range with different references, boost, analyzer
  {
    irs::numeric_token_stream minTerm;
    minTerm.reset(15.);
    irs::numeric_token_stream maxTerm;
    maxTerm.reset(40.);

    irs::Or expected;
    auto& root = expected.add<irs::And>();
    root.boost(5);
    {
      auto& filter = root.add<irs::by_granular_range>();
      filter.boost(2.5);
      *filter.mutable_field() = mangleNumeric("a.b.c");
      auto* opts = filter.mutable_options();
      irs::set_granular_term(opts->range.min, minTerm);
      opts->range.min_type = irs::BoundType::EXCLUSIVE;
    }
    {
      auto& filter = root.add<irs::by_range>();
      filter.boost(0.5);
      *filter.mutable_field() = mangleStringIdentity("a.b.c");
      auto* opts = filter.mutable_options();
      opts->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("40"));
      opts->range.max_type = irs::BoundType::INCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER boost(analyzer(boost(d.a.b.c > 15, 2.5) "
        "and analyzer(boost(d.a.b.c <= '40', 0.5), 'identity'), "
        "'test_analyzer'), 5) RETURN d",
        expected);
  }

  // range with different references
  {
    irs::numeric_token_stream maxTerm;
    maxTerm.reset(40.);

    irs::Or expected;
    auto& root = expected.add<irs::And>();
    {
      auto& filter = root.add<irs::by_range>();
      *filter.mutable_field() = mangleBool("a.b.c");
      auto* opts = filter.mutable_options();
      opts->range.min = irs::ref_cast<irs::byte_type>(irs::boolean_token_stream::value_false());
      opts->range.min_type = irs::BoundType::INCLUSIVE;
    }
    {
      auto& filter = root.add<irs::by_granular_range>();
      *filter.mutable_field() = mangleNumeric("a.b.c");
      auto* opts = filter.mutable_options();
      irs::set_granular_term(opts->range.max, maxTerm);
      opts->range.max_type = irs::BoundType::INCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a.b.c >= false and d.a.b.c <= 40 RETURN "
        "d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER false <= d.a.b.c and d.a.b.c <= 40 RETURN "
        "d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER false <= d.a['b']['c'] and d.a.b.c <= 40 "
        "RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a.b.c >= false and 40 >= d.a.b.c RETURN "
        "d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER false <= d.a.b.c and 40 >= d.a.b.c RETURN "
        "d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a.b.c >= false and d.a.b.c <= 40.0 "
        "RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d['a']['b']['c'] >= false and d.a.b.c <= "
        "40.0 RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER false <= d.a.b.c and d.a.b.c <= 40.0 "
        "RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER false <= d['a'].b.c and d.a.b.c <= 40.0 "
        "RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a.b.c >= false and 40.0 >= d.a.b.c "
        "RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a['b']['c'] >= false and 40.0 >= "
        "d.a['b']['c'] RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER false <= d.a.b.c and 40.0 >= d.a.b.c "
        "RETURN d",
        expected);
  }

  // range with different references
  {
    irs::numeric_token_stream maxTerm;
    maxTerm.reset(40.5);

    irs::Or expected;
    auto& root = expected.add<irs::And>();
    {
      auto& filter = root.add<irs::by_range>();
      *filter.mutable_field() = mangleNull("a.b.c");
      auto* opts = filter.mutable_options();
      opts->range.min = irs::ref_cast<irs::byte_type>(irs::null_token_stream::value_null());
      opts->range.min_type = irs::BoundType::EXCLUSIVE;
    }
    {
      auto& filter = root.add<irs::by_granular_range>();
      *filter.mutable_field() = mangleNumeric("a.b.c");
      auto* opts = filter.mutable_options();
      irs::set_granular_term(opts->range.max, maxTerm);
      opts->range.max_type = irs::BoundType::INCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a.b.c > null and d.a.b.c <= 40.5 RETURN "
        "d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d['a']['b']['c'] > null and d.a.b.c <= "
        "40.5 RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER null < d.a.b.c and d.a.b.c <= 40.5 RETURN "
        "d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER null < d['a'].b.c and d.a.b.c <= 40.5 "
        "RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a.b.c > null and 40.5 >= d.a.b.c RETURN "
        "d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER d.a['b']['c'] > null and 40.5 >= d.a.b.c "
        "RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER null < d.a.b.c and 40.5 >= d.a.b.c RETURN "
        "d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER null < d['a']['b']['c'] and 40.5 >= "
        "d.a['b']['c'] RETURN d",
        expected);
  }

  // boolean expression in range
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("numVal", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt(2)));

    irs::Or expected;
    auto& root = expected.add<irs::And>();
    {
      auto& filter = root.add<irs::by_range>();
      *filter.mutable_field() = mangleBool("a.b.c.e.f");
      auto* opts = filter.mutable_options();
      opts->range.min = irs::ref_cast<irs::byte_type>(irs::boolean_token_stream::value_true());
      opts->range.min_type = irs::BoundType::INCLUSIVE;
    }
    {
      auto& filter = root.add<irs::by_range>();
      *filter.mutable_field() = mangleBool("a.b.c.e.f");
      auto* opts = filter.mutable_options();
      opts->range.max = irs::ref_cast<irs::byte_type>(irs::boolean_token_stream::value_true());
      opts->range.max_type = irs::BoundType::INCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(),
        "LET numVal=2 FOR d IN collection FILTER d.a.b.c.e.f >= (numVal < 13) "
        "&& d.a.b.c.e.f <= (numVal > 1) RETURN d",
        expected,
        &ctx  // expression context
    );

    assertFilterSuccess(
        vocbase(),
        "LET numVal=2 FOR d IN collection FILTER (numVal < 13) <= d.a.b.c.e.f  "
        "&& d.a.b.c.e.f <= (numVal > 1) RETURN d",
        expected,
        &ctx  // expression context
    );
  }

  // boolean expression in range, boost
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("numVal", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt(2)));

    irs::Or expected;
    auto& root = expected.add<irs::And>();
    root.boost(1.5);
    {
      auto& filter = root.add<irs::by_range>();
      *filter.mutable_field() = mangleBool("a.b.c.e.f");
      auto* opts = filter.mutable_options();
      opts->range.min = irs::ref_cast<irs::byte_type>(irs::boolean_token_stream::value_true());
      opts->range.min_type = irs::BoundType::INCLUSIVE;
    }
    {
      auto& filter = root.add<irs::by_range>();
      *filter.mutable_field() = mangleBool("a.b.c.e.f");
      auto* opts = filter.mutable_options();
      opts->range.max = irs::ref_cast<irs::byte_type>(irs::boolean_token_stream::value_true());
      opts->range.max_type = irs::BoundType::INCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(),
        "LET numVal=2 FOR d IN collection FILTER boost(d.a.b.c.e.f >= (numVal "
        "< 13) && d.a.b.c.e.f <= (numVal > 1), 1.5) RETURN d",
        expected,
        &ctx  // expression context
    );

    assertFilterSuccess(
        vocbase(),
        "LET numVal=2 FOR d IN collection FILTER boost((numVal < 13) <= "
        "d.a.b.c.e.f  && d.a.b.c.e.f <= (numVal > 1), 1.5) RETURN d",
        expected,
        &ctx  // expression context
    );
  }

  // boolean and numeric expression in range
  {
    irs::numeric_token_stream maxTerm;
    maxTerm.reset(3.);

    ExpressionContextMock ctx;
    ctx.vars.emplace("numVal", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt(2)));

    irs::Or expected;
    auto& root = expected.add<irs::And>();
    {
      auto& filter = root.add<irs::by_range>();
      *filter.mutable_field() = mangleBool("a.b.c.e.f");
      auto* opts = filter.mutable_options();
      opts->range.min = irs::ref_cast<irs::byte_type>(irs::boolean_token_stream::value_true());
      opts->range.min_type = irs::BoundType::INCLUSIVE;
    }
    {
      auto& filter = root.add<irs::by_granular_range>();
      *filter.mutable_field() = mangleNumeric("a.b.c.e.f");
      auto* opts = filter.mutable_options();
      irs::set_granular_term(opts->range.max, maxTerm);
      opts->range.max_type = irs::BoundType::INCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(),
        "LET numVal=2 FOR d IN collection FILTER d.a.b.c.e.f >= (numVal < 13) "
        "&& d.a.b.c.e.f <= (numVal + 1) RETURN d",
        expected,
        &ctx  // expression context
    );

    assertFilterSuccess(
        vocbase(),
        "LET numVal=2 FOR d IN collection FILTER (numVal < 13) <= d.a.b.c.e.f  "
        "&& d.a.b.c.e.f <= (numVal + 1) RETURN d",
        expected,
        &ctx  // expression context
    );
  }

  // null expression in range
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("nullVal", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintNull{}));

    irs::Or expected;
    auto& root = expected.add<irs::And>();
    {
      auto& filter = root.add<irs::by_range>();
      *filter.mutable_field() = mangleNull("a.b.c.e.f");
      auto* opts = filter.mutable_options();
      opts->range.min = irs::ref_cast<irs::byte_type>(irs::null_token_stream::value_null());
      opts->range.min_type = irs::BoundType::INCLUSIVE;
    }
    {
      auto& filter = root.add<irs::by_range>();
      *filter.mutable_field() = mangleNull("a.b.c.e.f");
      auto* opts = filter.mutable_options();
      opts->range.max = irs::ref_cast<irs::byte_type>(irs::null_token_stream::value_null());
      opts->range.max_type = irs::BoundType::INCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(),
        "LET nullVal=null FOR d IN collection FILTER d.a.b.c.e.f >= (nullVal "
        "&& true) && d.a.b.c.e.f <= (nullVal && false) RETURN d",
        expected,
        &ctx  // expression context
    );

    assertFilterSuccess(
        vocbase(),
        "LET nullVal=null FOR d IN collection FILTER (nullVal && false) <= "
        "d.a.b.c.e.f  && d.a.b.c.e.f <= (nullVal && true) RETURN d",
        expected,
        &ctx  // expression context
    );
  }

  // null expression in range, boost
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("nullVal", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintNull{}));

    irs::Or expected;
    auto& root = expected.add<irs::And>();
    root.boost(1.5);
    {
      auto& filter = root.add<irs::by_range>();
      *filter.mutable_field() = mangleNull("a.b.c.e.f");
      auto* opts = filter.mutable_options();
      opts->range.min = irs::ref_cast<irs::byte_type>(irs::null_token_stream::value_null());
      opts->range.min_type = irs::BoundType::INCLUSIVE;
    }
    {
      auto& filter = root.add<irs::by_range>();
      *filter.mutable_field() = mangleNull("a.b.c.e.f");
      auto* opts = filter.mutable_options();
      opts->range.max = irs::ref_cast<irs::byte_type>(irs::null_token_stream::value_null());
      opts->range.max_type = irs::BoundType::INCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(),
        "LET nullVal=null FOR d IN collection FILTER boost(d.a.b.c.e.f >= "
        "(nullVal && true) && d.a.b.c.e.f <= (nullVal && false), 1.5) RETURN d",
        expected,
        &ctx  // expression context
    );

    assertFilterSuccess(
        vocbase(),
        "LET nullVal=null FOR d IN collection FILTER boost((nullVal && false) "
        "<= d.a.b.c.e.f  && d.a.b.c.e.f <= (nullVal && true), 1.5) RETURN d",
        expected,
        &ctx  // expression context
    );
  }

  // numeric expression in range
  {
    irs::numeric_token_stream minTerm;
    minTerm.reset(15.5);
    irs::numeric_token_stream maxTerm;
    maxTerm.reset(40.);

    ExpressionContextMock ctx;
    ctx.vars.emplace("numVal", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt(2)));

    irs::Or expected;
    auto& root = expected.add<irs::And>();
    {
      auto& filter = root.add<irs::by_granular_range>();
      *filter.mutable_field() = mangleNumeric("a.b.c.e.f");
      auto* opts = filter.mutable_options();
      irs::set_granular_term(opts->range.min, minTerm);
      opts->range.min_type = irs::BoundType::INCLUSIVE;
    }
    {
      auto& filter = root.add<irs::by_granular_range>();
      *filter.mutable_field() = mangleNumeric("a.b.c.e.f");
      auto* opts = filter.mutable_options();
      irs::set_granular_term(opts->range.max, maxTerm);
      opts->range.max_type = irs::BoundType::EXCLUSIVE;
    }

    assertFilterSuccess(
        vocbase(),
        "LET numVal=2 FOR d IN collection FILTER d.a['b'].c.e.f >= (numVal + "
        "13.5) && d.a.b.c.e.f < (numVal + 38) RETURN d",
        expected,
        &ctx  // expression context
    );

    assertFilterSuccess(
        vocbase(),
        "LET numVal=2 FOR d IN collection FILTER (numVal + 13.5) <= "
        "d.a.b.c.e.f  && d.a.b.c.e.f < (numVal + 38) RETURN d",
        expected,
        &ctx  // expression context
    );
  }

  // noneterministic expression -> wrap it
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));

    std::string const refName = "d";
    std::string const queryString =
        "FOR d IN collection FILTER d.a.b.c > _NONDETERM_('15') and d.a.b.c < "
        "'40' RETURN d";

    arangodb::aql::Query query(arangodb::transaction::StandaloneContext::Create(vocbase),
                               arangodb::aql::QueryString(queryString), nullptr,
                               std::make_shared<arangodb::velocypack::Builder>());

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
      arangodb::iresearch::QueryContext const ctx{nullptr, nullptr, nullptr, nullptr, nullptr, ref};
      EXPECT_TRUE(
          (arangodb::iresearch::FilterFactory::filter(nullptr, ctx, *filterNode).ok()));
    }

    // iteratorForCondition
    {
      arangodb::transaction ::Methods trx(arangodb::transaction::StandaloneContext::Create(vocbase),
                                          {}, {}, {}, arangodb::transaction::Options());

      auto dummyPlan = arangodb::tests::planFromQuery(vocbase, "RETURN 1");

      irs::Or expected;
      auto& root = expected.add<irs::And>();
      root.add<arangodb::iresearch::ByExpression>().init(
          *dummyPlan, *ast,
          *filterNode->getMember(0)->getMember(0)  // d.a.b.c > _NONDETERM_(15)
      );
      {
        auto& filter = root.add<irs::by_range>();
        *filter.mutable_field() = mangleStringIdentity("a.b.c");
        auto* opts = filter.mutable_options();
        opts->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("40")); // d.a.b.c < 40
        opts->range.max_type = irs::BoundType::EXCLUSIVE;
      }
      
      ExpressionContextMock exprCtx;
      exprCtx.setTrx(&trx);

      irs::Or actual;
      arangodb::iresearch::QueryContext const ctx{&trx, dummyPlan.get(), ast,
                                                  &exprCtx,
                                                  &irs::sub_reader::empty(), ref};
      EXPECT_TRUE(
          (arangodb::iresearch::FilterFactory::filter(&actual, ctx, *filterNode).ok()));
      EXPECT_EQ(expected, actual);
    }
  }
}
