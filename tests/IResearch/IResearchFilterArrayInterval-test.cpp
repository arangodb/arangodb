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
/// @author Andrei Lobov
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

class IResearchFilterArrayIntervalTest
    : public ::testing::Test,
      public arangodb::tests::LogSuppressor<arangodb::Logger::AUTHENTICATION, arangodb::LogLevel::ERR> {
 protected:
  arangodb::tests::mocks::MockAqlServer server;

 private:
  TRI_vocbase_t* _vocbase;

 protected:
  IResearchFilterArrayIntervalTest() {
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

namespace {
  // Auxilary check lambdas. Need them to check by_range part of expected filter
  auto checkLess = [](irs::boolean_filter::const_iterator& filter,
                      irs::bytes_ref const& term,
                      irs::string_ref const& field) {
     ASSERT_EQ(irs::type<irs::by_range>::id(), filter->type());
     auto& actual = dynamic_cast<irs::by_range const&>(*filter);
     irs::by_range expected;
     *expected.mutable_field() = field;
     expected.mutable_options()->range.min = term;
     expected.mutable_options()->range.min_type = irs::BoundType::EXCLUSIVE;
     EXPECT_EQ(expected, actual);
  };

  auto checkLessEqual = [](irs::boolean_filter::const_iterator& filter,
                      irs::bytes_ref const& term,
                      irs::string_ref const& field) {
     ASSERT_EQ(irs::type<irs::by_range>::id(), filter->type());
     auto& actual = dynamic_cast<irs::by_range const&>(*filter);
     irs::by_range expected;
     *expected.mutable_field() = field;
     expected.mutable_options()->range.min = term;
     expected.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
     EXPECT_EQ(expected, actual);
  };

  auto checkGreaterEqual = [](irs::boolean_filter::const_iterator& filter,
                      irs::bytes_ref const& term,
                      irs::string_ref const& field) {
     ASSERT_EQ(irs::type<irs::by_range>::id(), filter->type());
     auto& actual = dynamic_cast<irs::by_range const&>(*filter);
     irs::by_range expected;
     *expected.mutable_field() = field;
     expected.mutable_options()->range.max = term;
     expected.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;
     EXPECT_EQ(expected, actual);
  };

  auto checkGreater = [](irs::boolean_filter::const_iterator& filter,
                      irs::bytes_ref const& term,
                      irs::string_ref const& field) {
    ASSERT_EQ(irs::type<irs::by_range>::id(), filter->type());
    auto& actual = dynamic_cast<irs::by_range const&>(*filter);
    irs::by_range expected;
    *expected.mutable_field() = field;
    expected.mutable_options()->range.max = term;
    expected.mutable_options()->range.max_type = irs::BoundType::EXCLUSIVE;
    EXPECT_EQ(expected, actual);
  };

  // Auxilary check lambdas. Need them to check root part of expected filter
  auto checkAny = [](irs::Or& actual, iresearch::boost_t boost) {
    EXPECT_EQ(1, actual.size());
    EXPECT_EQ(irs::type<irs::Or>::id(), actual.begin()->type());
    auto& root = dynamic_cast<const irs::Or&>(*actual.begin());
    EXPECT_EQ(3, root.size());
    EXPECT_EQ(boost, root.boost());
    return root.begin();
  };
  auto checkAll = [](irs::Or& actual, iresearch::boost_t boost) {
    EXPECT_EQ(1, actual.size());
    EXPECT_EQ(irs::type<irs::And>::id(), actual.begin()->type());
    auto& root = dynamic_cast<const irs::And&>(*actual.begin());
    EXPECT_EQ(3, root.size());
    EXPECT_EQ(boost, root.boost());
    return root.begin();
  };
  auto checkNone = [](irs::Or& actual, iresearch::boost_t boost) {
    // none for now is like All but with inverted interval check
    EXPECT_EQ(1, actual.size());
    EXPECT_EQ(irs::type<irs::And>::id(), actual.begin()->type());
    auto& root = dynamic_cast<const irs::And&>(*actual.begin());
    EXPECT_EQ(3, root.size());
    EXPECT_EQ(boost, root.boost());
    return root.begin();
  };
  std::vector<std::pair<std::string,
                        std::pair<std::function<irs::boolean_filter::const_iterator(irs::Or&, iresearch::boost_t)>,
                                  std::function<void(irs::boolean_filter::const_iterator&,
                                           irs::bytes_ref const&,
                                           irs::string_ref const&)>>>>
  intervalOperations = {
    {"ANY >", {checkAny, checkGreater}}, {"ANY >=", {checkAny, checkGreaterEqual}},
    {"ANY <", {checkAny, checkLess}}, {"ANY <=", {checkAny, checkLessEqual}},
    {"ALL >", {checkAll, checkGreater}}, {"ALL >=", {checkAll, checkGreaterEqual}},
    {"ALL <", {checkAll, checkLess}}, {"ALL <=", {checkAll, checkLessEqual}},
    {"NONE >", {checkNone, checkLessEqual}}, {"NONE >=", {checkNone, checkLess}},
    {"NONE <", {checkNone, checkGreaterEqual}}, {"NONE <=", {checkNone, checkGreater}}
  };

  std::string buildQueryString(const irs::string_ref queryPrefix,
                               const irs::string_ref operation,
                               const irs::string_ref querySuffix) {
    return std::string(queryPrefix).append(" ")
           .append(operation).append(" ").append(querySuffix);
  }
} // namespace

TEST_F(IResearchFilterArrayIntervalTest, Interval) {
  // simple attribute
  {
    for (auto operation : intervalOperations) {
      auto queryString = buildQueryString("FOR d IN collection FILTER ['1','2','3']",
        operation.first, "d.a RETURN d");
      SCOPED_TRACE(testing::Message("Query:") << queryString);
      irs::Or actual;
      buildActualFilter(vocbase(), queryString, actual);
      auto subFiltersIterator = operation.second.first(actual, 1);
      operation.second.second(subFiltersIterator, irs::ref_cast<irs::byte_type>(irs::string_ref("1")), mangleStringIdentity("a"));
      ++subFiltersIterator;
      operation.second.second(subFiltersIterator, irs::ref_cast<irs::byte_type>(irs::string_ref("2")), mangleStringIdentity("a"));
      ++subFiltersIterator;
      operation.second.second(subFiltersIterator, irs::ref_cast<irs::byte_type>(irs::string_ref("3")), mangleStringIdentity("a"));
      ++subFiltersIterator;
    }
  }

  // complex attribute name with offset, boost, analyzer
  {
    for (auto operation : intervalOperations) {
      auto queryString = buildQueryString("FOR d IN collection FILTER BOOST(ANALYZER(['1','2','3']",
        operation.first, "d.a['b']['c'][412].e.f, 'test_analyzer'), 2.5) RETURN d");
      SCOPED_TRACE(testing::Message("Query:") << queryString);
      irs::Or actual;
      buildActualFilter(vocbase(), queryString, actual);
      auto subFiltersIterator = operation.second.first(actual, 2.5);
      operation.second.second(subFiltersIterator, irs::ref_cast<irs::byte_type>(irs::string_ref("1")), mangleString("a.b.c[412].e.f", "test_analyzer"));
      ++subFiltersIterator;
      operation.second.second(subFiltersIterator, irs::ref_cast<irs::byte_type>(irs::string_ref("2")), mangleString("a.b.c[412].e.f", "test_analyzer"));
      ++subFiltersIterator;
      operation.second.second(subFiltersIterator, irs::ref_cast<irs::byte_type>(irs::string_ref("3")), mangleString("a.b.c[412].e.f", "test_analyzer"));
      ++subFiltersIterator;
    }
  }
  // heterogeneous array values, analyzer, boost
  {
    for (auto operation : intervalOperations) {
      auto queryString = buildQueryString("FOR d IN collection FILTER ANALYZER(BOOST(['1',null,true]",
        operation.first, "d.quick.brown.fox, 1.5), 'test_analyzer') RETURN d");
      SCOPED_TRACE(testing::Message("Query:") << queryString);
      irs::Or actual;
      buildActualFilter(vocbase(), queryString, actual);
      auto subFiltersIterator = operation.second.first(actual, 1.5);
      operation.second.second(subFiltersIterator,
                              irs::ref_cast<irs::byte_type>(irs::string_ref("1")),
                              mangleString("quick.brown.fox", "test_analyzer"));
      ++subFiltersIterator;
      operation.second.second(subFiltersIterator,
                              irs::ref_cast<irs::byte_type>(irs::null_token_stream::value_null()),
                              mangleNull("quick.brown.fox"));
      ++subFiltersIterator;
      operation.second.second(subFiltersIterator,
                              irs::ref_cast<irs::byte_type>(irs::ref_cast<irs::byte_type>(irs::boolean_token_stream::value_true())),
                              mangleBool("quick.brown.fox"));
      ++subFiltersIterator;
    }
  }
  // heterogeneous non string values, analyzer, boost
  {
    for (auto operation : intervalOperations) {
      auto queryString = buildQueryString("FOR d IN collection FILTER ANALYZER(BOOST([2, null,false]",
        operation.first, "d.quick.brown.fox, 1.5), 'test_analyzer') RETURN d");
      SCOPED_TRACE(testing::Message("Query:") << queryString);
      irs::Or actual;
      buildActualFilter(vocbase(), queryString, actual);
      auto subFiltersIterator = operation.second.first(actual, 1.5);
      irs::numeric_token_stream stream;
      stream.reset(2.);
      ASSERT_EQ(irs::type<irs::by_granular_range>::id(), subFiltersIterator->type());
      {
        auto& by_range_actual = dynamic_cast<irs::by_granular_range const&>(*subFiltersIterator);
        irs::by_granular_range expected;
        *expected.mutable_field() = mangleNumeric("quick.brown.fox");

       // granular range handled separately (it is used only for numerics, just check it here once)
        if (operation.first == "ANY >" || operation.first == "ALL >" || operation.first == "NONE <=") {
          irs::set_granular_term(expected.mutable_options()->range.max, stream);
          expected.mutable_options()->range.max_type = irs::BoundType::EXCLUSIVE;
        } else if (operation.first == "ANY >=" || operation.first == "ALL >=" || operation.first == "NONE <") {
          irs::set_granular_term(expected.mutable_options()->range.max, stream);
          expected.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;
        } else if (operation.first == "ANY <" || operation.first == "ALL <" || operation.first == "NONE >=") {
          irs::set_granular_term(expected.mutable_options()->range.min, stream);
          expected.mutable_options()->range.min_type = irs::BoundType::EXCLUSIVE;
        } else if (operation.first == "ANY <=" || operation.first == "ALL <=" || operation.first == "NONE >") {
          irs::set_granular_term(expected.mutable_options()->range.min, stream);
          expected.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
        } else {
          ASSERT_TRUE(false); // new array comparison operator added? Need to update checks here!
        }
        EXPECT_EQ(expected, by_range_actual);
      }
      ++subFiltersIterator;
      operation.second.second(subFiltersIterator,
                              irs::ref_cast<irs::byte_type>(irs::null_token_stream::value_null()),
                              mangleNull("quick.brown.fox"));
      ++subFiltersIterator;
      operation.second.second(subFiltersIterator,
                              irs::ref_cast<irs::byte_type>(irs::ref_cast<irs::byte_type>(irs::boolean_token_stream::value_false())),
                              mangleBool("quick.brown.fox"));
      ++subFiltersIterator;
    }
  }
  // dynamic complex attribute name
  {
    for (auto operation : intervalOperations) {
      auto queryString = buildQueryString("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
                                          "collection FILTER "
                                          " ['1','2','3']",
                                          operation.first,
                                          "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
                                          " RETURN d");
      ExpressionContextMock ctx;
      ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
      ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
      ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                                      arangodb::aql::AqlValueHintInt{4})));
      ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                                      arangodb::aql::AqlValueHintDouble{5.6})));
      SCOPED_TRACE(testing::Message("Query:") << queryString);
      irs::Or actual;
      buildActualFilter(vocbase(), queryString, actual, &ctx);
      auto subFiltersIterator = operation.second.first(actual, 1);

      operation.second.second(subFiltersIterator,
                              irs::ref_cast<irs::byte_type>(irs::string_ref("1")),
                              mangleStringIdentity("a.b.c.e[4].f[5].g[3].g.a"));
      ++subFiltersIterator;
      operation.second.second(subFiltersIterator,
                              irs::ref_cast<irs::byte_type>(irs::string_ref("2")),
                              mangleStringIdentity("a.b.c.e[4].f[5].g[3].g.a"));
      ++subFiltersIterator;
      operation.second.second(subFiltersIterator,
                              irs::ref_cast<irs::byte_type>(irs::string_ref("3")),
                              mangleStringIdentity("a.b.c.e[4].f[5].g[3].g.a"));
      ++subFiltersIterator;
    }
  }
  // invalid dynamic attribute name (null value)
  {
    for (auto operation : intervalOperations) {
      auto queryString = buildQueryString("LET a=null LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER "
        " ['1','2','3']",
        operation.first,
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        " RETURN d");
      SCOPED_TRACE(testing::Message("Query:") << queryString);
      ExpressionContextMock ctx;
      ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintNull{}));  // invalid value type
      ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{ "c" }));
      ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
        arangodb::aql::AqlValueHintInt{ 4 })));
      ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
        arangodb::aql::AqlValueHintDouble{ 5.6 })));
      assertFilterExecutionFail(vocbase(), queryString, &ctx);
    }
  }
  // invalid dynamic attribute name (missing value)
  {
    for (auto operation : intervalOperations) {
      auto queryString = buildQueryString("LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER "
        " ['1','2','3']",
        operation.first,
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        " RETURN d");
      SCOPED_TRACE(testing::Message("Query:") << queryString);
      ExpressionContextMock ctx;
      ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
      ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{ "c" }));
      ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
        arangodb::aql::AqlValueHintDouble{ 5.6 })));
      assertFilterExecutionFail(vocbase(), queryString, &ctx);
    }
  }
  // invalid dynamic attribute name (bool value)
  {
    for (auto operation : intervalOperations) {
      auto queryString = buildQueryString("LET a=false LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER "
        " ['1','2','3']",
        operation.first,
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        " RETURN d");
      SCOPED_TRACE(testing::Message("Query:") << queryString);
      ExpressionContextMock ctx;
      ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintBool{false}));
      ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{ "c" }));
      ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
        arangodb::aql::AqlValueHintInt{ 4 })));
      ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
        arangodb::aql::AqlValueHintDouble{ 5.6 })));
      assertFilterExecutionFail(vocbase(), queryString, &ctx);
    }
  }
  // reference in array
  {
    for (auto operation : intervalOperations) {
      auto queryString = buildQueryString("LET c=2 FOR d IN collection FILTER ['1', c, '3']",
        operation.first,
        "d.a.b.c.e.f RETURN d");
      SCOPED_TRACE(testing::Message("Query:") << queryString);

      arangodb::aql::Variable var("c", 0, /*isDataFromColl*/ false);
      arangodb::aql::AqlValue value(arangodb::aql::AqlValue("2"));
      arangodb::aql::AqlValueGuard guard(value, true);
      ExpressionContextMock ctx;
      ctx.vars.emplace(var.name, value);
      irs::Or actual;
      buildActualFilter(vocbase(), queryString, actual, &ctx);
      auto subFiltersIterator = operation.second.first(actual, 1);

      operation.second.second(subFiltersIterator,
                              irs::ref_cast<irs::byte_type>(irs::string_ref("1")),
                              mangleStringIdentity("a.b.c.e.f"));
      ++subFiltersIterator;
      operation.second.second(subFiltersIterator,
                              irs::ref_cast<irs::byte_type>(irs::string_ref("2")),
                              mangleStringIdentity("a.b.c.e.f"));
      ++subFiltersIterator;
      operation.second.second(subFiltersIterator,
                              irs::ref_cast<irs::byte_type>(irs::string_ref("3")),
                              mangleStringIdentity("a.b.c.e.f"));
      ++subFiltersIterator;
    }
  }
  // array as reference, boost, analyzer
  {
    for (auto operation : intervalOperations) {
      auto queryString = buildQueryString("LET x=['1', '2', '3'] FOR d IN collection FILTER "
        "ANALYZER(BOOST(x",
        operation.first,
        "d.a.b.c.e.f, 1.5), 'test_analyzer') RETURN d");
      SCOPED_TRACE(testing::Message("Query:") << queryString);

      auto obj = arangodb::velocypack::Parser::fromJson("[ \"1\", \"2\", \"3\"]");
      arangodb::aql::AqlValue value(obj->slice());
      arangodb::aql::AqlValueGuard guard(value, true);

      ExpressionContextMock ctx;
      ctx.vars.emplace("x", value);

      irs::Or actual;
      buildActualFilter(vocbase(), queryString, actual, &ctx);
      auto subFiltersIterator = operation.second.first(actual, 1.5);

      operation.second.second(subFiltersIterator,
                              irs::ref_cast<irs::byte_type>(irs::string_ref("1")),
                              mangleString("a.b.c.e.f", "test_analyzer"));
      ++subFiltersIterator;
      operation.second.second(subFiltersIterator,
                              irs::ref_cast<irs::byte_type>(irs::string_ref("2")),
                              mangleString("a.b.c.e.f", "test_analyzer"));
      ++subFiltersIterator;
      operation.second.second(subFiltersIterator,
                              irs::ref_cast<irs::byte_type>(irs::string_ref("3")),
                              mangleString("a.b.c.e.f", "test_analyzer"));
      ++subFiltersIterator;
    }
  }
  // nondeterministic value
  {
    for (auto operation : intervalOperations) {
      auto queryString = buildQueryString("FOR d IN collection FILTER [ '1', RAND(), '3' ]",
        operation.first,
        "d.a.b.c.e.f RETURN d");
      SCOPED_TRACE(testing::Message("Query:") << queryString);
       irs::Or actual;
      buildActualFilter(vocbase(), queryString, actual, nullptr);
      auto subFiltersIterator = operation.second.first(actual, 1);
       operation.second.second(subFiltersIterator,
                              irs::ref_cast<irs::byte_type>(irs::string_ref("1")),
                              mangleStringIdentity("a.b.c.e.f"));
      ++subFiltersIterator;
      EXPECT_EQ(irs::type<arangodb::iresearch::ByExpression>::id(), subFiltersIterator->type());
      EXPECT_NE(nullptr, dynamic_cast<arangodb::iresearch::ByExpression const*>(&*subFiltersIterator));

      ++subFiltersIterator;
      operation.second.second(subFiltersIterator,
                              irs::ref_cast<irs::byte_type>(irs::string_ref("3")),
                              mangleStringIdentity("a.b.c.e.f"));
      ++subFiltersIterator;
    }
  }
  // self-referenced value
  {
    for (auto operation : intervalOperations) {
      auto queryString = buildQueryString("FOR d IN collection FILTER [ '1', d, '3' ]",
        operation.first,
        "d.a.b.c.e.f RETURN d");
      SCOPED_TRACE(testing::Message("Query:") << queryString);
       irs::Or actual;
      buildActualFilter(vocbase(), queryString, actual, nullptr);
      auto subFiltersIterator = operation.second.first(actual, 1);
       operation.second.second(subFiltersIterator,
                              irs::ref_cast<irs::byte_type>(irs::string_ref("1")),
                              mangleStringIdentity("a.b.c.e.f"));
      ++subFiltersIterator;
      EXPECT_EQ(irs::type<arangodb::iresearch::ByExpression>::id(), subFiltersIterator->type());
      EXPECT_NE(nullptr, dynamic_cast<arangodb::iresearch::ByExpression const*>(&*subFiltersIterator));

      ++subFiltersIterator;
      operation.second.second(subFiltersIterator,
                              irs::ref_cast<irs::byte_type>(irs::string_ref("3")),
                              mangleStringIdentity("a.b.c.e.f"));
      ++subFiltersIterator;
    }
  }
  // self-referenced value
  {
    for (auto operation : intervalOperations) {
      auto queryString = buildQueryString("FOR d IN collection FILTER [ '1', d.e, d.a.b.c.e.f  ]",
        operation.first,
        "d.a.b.c.e.f RETURN d");
      SCOPED_TRACE(testing::Message("Query:") << queryString);
       irs::Or actual;
      buildActualFilter(vocbase(), queryString, actual, nullptr);
      auto subFiltersIterator = operation.second.first(actual, 1);
       operation.second.second(subFiltersIterator,
                              irs::ref_cast<irs::byte_type>(irs::string_ref("1")),
                              mangleStringIdentity("a.b.c.e.f"));
      ++subFiltersIterator;
      EXPECT_EQ(irs::type<arangodb::iresearch::ByExpression>::id(), subFiltersIterator->type());
      EXPECT_NE(nullptr, dynamic_cast<arangodb::iresearch::ByExpression const*>(&*subFiltersIterator));

      ++subFiltersIterator;
      EXPECT_EQ(irs::type<arangodb::iresearch::ByExpression>::id(), subFiltersIterator->type());
      EXPECT_NE(nullptr, dynamic_cast<arangodb::iresearch::ByExpression const*>(&*subFiltersIterator));
    }
  }
  // self-referenced value
  {
    for (auto operation : intervalOperations) {
      auto queryString = buildQueryString("FOR d IN collection FILTER [ '1', 1 + d.b, '3' ]",
        operation.first,
        "d.a.b.c.e.f RETURN d");
      SCOPED_TRACE(testing::Message("Query:") << queryString);
      irs::Or actual;
      buildActualFilter(vocbase(), queryString, actual, nullptr);
      auto subFiltersIterator = operation.second.first(actual, 1);
      operation.second.second(subFiltersIterator,
                              irs::ref_cast<irs::byte_type>(irs::string_ref("1")),
                              mangleStringIdentity("a.b.c.e.f"));
      ++subFiltersIterator;
      EXPECT_EQ(irs::type<arangodb::iresearch::ByExpression>::id(), subFiltersIterator->type());
      EXPECT_NE(nullptr, dynamic_cast<arangodb::iresearch::ByExpression const*>(&*subFiltersIterator));

      ++subFiltersIterator;
      operation.second.second(subFiltersIterator,
                              irs::ref_cast<irs::byte_type>(irs::string_ref("3")),
                              mangleStringIdentity("a.b.c.e.f"));
    }
  }
  // heterogeneous references and expression in array, analyzer, boost
  {
    for (auto operation : intervalOperations) {
      auto queryString = buildQueryString(
          "LET strVal='str' LET boolVal=false LET nullVal=null FOR "
          "d IN collection FILTER boost(ANALYZER([CONCAT(strVal, '2'), boolVal, nullVal]",
          operation.first,
          "d.a.b.c.e.f, 'test_analyzer'),2.5) RETURN d");
      SCOPED_TRACE(testing::Message("Query:") << queryString);
      ExpressionContextMock ctx;
      ctx.vars.emplace("strVal", arangodb::aql::AqlValue("str"));
      ctx.vars.emplace("boolVal",
                     arangodb::aql::AqlValue(arangodb::aql::AqlValueHintBool(false)));
      ctx.vars.emplace("nullVal", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintNull{}));
      irs::Or actual;
      buildActualFilter(vocbase(), queryString, actual, &ctx);
      auto subFiltersIterator = operation.second.first(actual, 2.5);

      operation.second.second(subFiltersIterator,
                              irs::ref_cast<irs::byte_type>(irs::string_ref("str2")),
                              mangleString("a.b.c.e.f", "test_analyzer"));
      ++subFiltersIterator;
      operation.second.second(subFiltersIterator,
                              irs::ref_cast<irs::byte_type>(irs::ref_cast<irs::byte_type>(irs::boolean_token_stream::value_false())),
                              mangleBool("a.b.c.e.f"));
      ++subFiltersIterator;
      operation.second.second(subFiltersIterator,
                              irs::ref_cast<irs::byte_type>(irs::null_token_stream::value_null()),
                              mangleNull("a.b.c.e.f"));
      ++subFiltersIterator;
    }
  }
  // not array as left argument
  {
    for (auto operation : intervalOperations) {
      auto queryString = buildQueryString(
          "LET a=null LET b='b' LET c=4 LET e=5.6 FOR d IN collection FILTER a ",
          operation.first,
          "d.a RETURN d");
      SCOPED_TRACE(testing::Message("Query:") << queryString);
      ExpressionContextMock ctx;
      ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintBool{false}));  // invalid value type
      ctx.vars.emplace("b", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
      ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                                      arangodb::aql::AqlValueHintInt{4})));
      ctx.vars.emplace("e", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                                      arangodb::aql::AqlValueHintDouble{5.6})));
      assertFilterExecutionFail(
        vocbase(),
        queryString,
        &ctx);
    }
  }
  // self-reference
  {
    for (auto operation : intervalOperations) {
      auto queryString = buildQueryString(
          "FOR d IN myView FILTER [1,2,'3']",
          operation.first,
          " d RETURN d");
      SCOPED_TRACE(testing::Message("Query:") << queryString);
      assertExpressionFilter(vocbase(), queryString);
    }
  }
  // non-deterministic expression name in array
  {
    for (auto operation : intervalOperations) {
      auto queryString = buildQueryString(
         "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
         "collection FILTER "
         " ['1','2','3']",
         operation.first,
         " d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_NONDETERM_('a')]  RETURN d ");
      SCOPED_TRACE(testing::Message("Query:") << queryString);
      assertExpressionFilter(vocbase(), queryString);
    }
  }
  // no reference provided
  {
    for (auto operation : intervalOperations) {
      auto queryString = buildQueryString(
         "LET x={} FOR d IN myView FILTER [1,x.a,3] ",
         operation.first,
         "d.a RETURN d");
      SCOPED_TRACE(testing::Message("Query:") << queryString);
      assertFilterExecutionFail(
      vocbase(), queryString, &ExpressionContextMock::EMPTY);
    }
  }
  // not a value in array
  {
    for (auto operation : intervalOperations) {
      auto queryString = buildQueryString(
         "FOR d IN collection FILTER ['1',['2'],'3'] ",
         operation.first,
         "d.a RETURN d");
      SCOPED_TRACE(testing::Message("Query:") << queryString);
      assertFilterFail(vocbase(), queryString);
    }
  }
  // empty array
  {
    for (auto operation : intervalOperations) {
      auto queryString = buildQueryString(
        "FOR d IN collection FILTER BOOST([]",
        operation.first,
        "d.a, 2.5) RETURN d");
      SCOPED_TRACE(testing::Message("Query") << queryString);
      irs::Or expected;
      if (operation.first.find("ANY") != std::string::npos) {
        expected.add<irs::empty>();
      } else {
        expected.add<irs::all>();
      }
      expected.boost(2.5);
      assertFilterSuccess(
        vocbase(), queryString, expected);
    }
  }
}
