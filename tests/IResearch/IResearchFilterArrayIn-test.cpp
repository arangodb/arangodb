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

class IResearchFilterArrayInTest
    : public ::testing::Test,
      public arangodb::tests::LogSuppressor<arangodb::Logger::AUTHENTICATION, arangodb::LogLevel::ERR> {
 protected:
  arangodb::tests::mocks::MockAqlServer server;

 private:
  TRI_vocbase_t* _vocbase;

 protected:
  IResearchFilterArrayInTest() {
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
    analyzers.emplace(
        result, "testVocbase::test_analyzer", "TestAnalyzer",
        arangodb::velocypack::Parser::fromJson("{ \"args\": \"abc\"}")->slice());  // cache analyzer
  }

  TRI_vocbase_t& vocbase() { return *_vocbase; }
};  // IResearchFilterSetup

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

TEST_F(IResearchFilterArrayInTest, BinaryIn) {
  // simple attribute ANY
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
      *filter.mutable_field() = mangleStringIdentity("a");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("2"));
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleStringIdentity("a");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("3"));
    }

    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER ['1','2','3'] ANY IN d.a RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER ['1','2','3'] ANY IN d['a'] RETURN d", expected);

    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER ['1','2','3'] ANY == d.a RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER ['1','2','3'] ANY == d['a'] RETURN d", expected);
  }

  // simple attribute ALL
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
      *filter.mutable_field() = mangleStringIdentity("a");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("2"));
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleStringIdentity("a");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("3"));
    }

    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER ['1','2','3'] ALL IN d.a RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER ['1','2','3'] ALL IN d['a'] RETURN d", expected);

    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER ['1','2','3'] ALL == d.a RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER ['1','2','3'] ALL == d['a'] RETURN d", expected);
  }

  // simple attribute NONE
  {
    irs::Or expected;
    auto& root = expected.add<irs::Not>().filter<irs::Or>();
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleStringIdentity("a");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("1"));
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleStringIdentity("a");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("2"));
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleStringIdentity("a");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("3"));
    }

    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER ['1','2','3'] NONE IN d.a RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER ['1','2','3'] NONE IN d['a'] RETURN d", expected);

    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER ['1','2','3'] NONE == d.a RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER ['1','2','3'] NONE == d['a'] RETURN d", expected);
  }

  // simple offset ANY
  {
    irs::Or expected;
    auto& root = expected.add<irs::Or>();
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleStringIdentity("[1]");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("1"));
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleStringIdentity("[1]");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("2"));
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleStringIdentity("[1]");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("3"));
    }

    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER ['1','2','3'] ANY IN d[1] RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER ANALYZER(['1','2','3'] ANY IN d[1], "
        "'identity') RETURN d",
        expected);

    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER ['1','2','3'] ANY == d[1] RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER ANALYZER(['1','2','3'] ANY == d[1], "
        "'identity') RETURN d",
        expected);
  }
  // simple offset ALL
  {
    irs::Or expected;
    auto& root = expected.add<irs::And>();
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleStringIdentity("[1]");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("1"));
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleStringIdentity("[1]");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("2"));
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleStringIdentity("[1]");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("3"));
    }

    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER ['1','2','3'] ALL IN d[1] RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER ANALYZER(['1','2','3'] ALL IN d[1], "
        "'identity') RETURN d",
        expected);

    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER ['1','2','3'] ALL == d[1] RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER ANALYZER(['1','2','3'] ALL == d[1], "
        "'identity') RETURN d",
        expected);
  }
  // simple offset NONE
  {
    irs::Or expected;
    auto& root = expected.add<irs::Not>().filter<irs::Or>();
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleStringIdentity("[1]");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("1"));
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleStringIdentity("[1]");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("2"));
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleStringIdentity("[1]");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("3"));
    }

    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER ['1','2','3'] NONE IN d[1] RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER ANALYZER(['1','2','3'] NONE IN d[1], "
        "'identity') RETURN d",
        expected);

    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER ['1','2','3'] NONE == d[1] RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER ANALYZER(['1','2','3'] NONE == d[1], "
        "'identity') RETURN d",
        expected);
  }

  // complex attribute name with offset, analyzer ANY
  {
    irs::Or expected;
    auto& root = expected.add<irs::Or>();
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleString("a.b.c[412].e.f", "test_analyzer");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("1"));
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleString("a.b.c[412].e.f", "test_analyzer");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("2"));
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleString("a.b.c[412].e.f", "test_analyzer");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("3"));
    }

    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER ANALYZER("
        "['1','2','3'] ANY IN d.a['b']['c'][412].e.f, 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER ANALYZER("
        "['1','2','3'] ANY IN d.a.b.c[412].e.f, 'test_analyzer') RETURN d",
        expected);

    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER ANALYZER("
        "['1','2','3'] ANY == d.a['b']['c'][412].e.f, 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER ANALYZER("
        "['1','2','3'] ANY == d.a.b.c[412].e.f, 'test_analyzer') RETURN d",
        expected);
  }
  // complex attribute name with offset, analyzer ALL
  {
    irs::Or expected;
    auto& root = expected.add<irs::And>();
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleString("a.b.c[412].e.f", "test_analyzer");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("1"));
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleString("a.b.c[412].e.f", "test_analyzer");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("2"));
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleString("a.b.c[412].e.f", "test_analyzer");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("3"));
    }

    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER ANALYZER("
        "['1','2','3'] ALL IN d.a['b']['c'][412].e.f, 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER ANALYZER("
        "['1','2','3'] ALL IN d.a.b.c[412].e.f, 'test_analyzer') RETURN d",
        expected);

    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER ANALYZER("
        "['1','2','3'] ALL == d.a['b']['c'][412].e.f, 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER ANALYZER("
        "['1','2','3'] ALL == d.a.b.c[412].e.f, 'test_analyzer') RETURN d",
        expected);
  }
  // complex attribute name with offset, analyzer NONE
  {
    irs::Or expected;
    auto& root = expected.add<irs::Not>().filter<irs::Or>();
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleString("a.b.c[412].e.f", "test_analyzer");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("1"));
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleString("a.b.c[412].e.f", "test_analyzer");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("2"));
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleString("a.b.c[412].e.f", "test_analyzer");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("3"));
    }

    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER ANALYZER("
        "['1','2','3'] NONE IN d.a['b']['c'][412].e.f, 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER ANALYZER("
        "['1','2','3'] NONE IN d.a.b.c[412].e.f, 'test_analyzer') RETURN d",
        expected);

    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER ANALYZER("
        "['1','2','3'] NONE == d.a['b']['c'][412].e.f, 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER ANALYZER("
        "['1','2','3'] NONE == d.a.b.c[412].e.f, 'test_analyzer') RETURN d",
        expected);
  }

  // complex attribute name with offset, boost ANY
  {
    irs::Or expected;
    auto& root = expected.add<irs::Or>();
    root.boost(2.5);
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleStringIdentity("a.b.c[412].e.f");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("1"));
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleStringIdentity("a.b.c[412].e.f");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("2"));
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleStringIdentity("a.b.c[412].e.f");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("3"));
    }

    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER BOOST("
        "['1','2','3'] ANY IN d.a['b']['c'][412].e.f, 2.5) RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER BOOST(['1','2','3'] ANY IN d.a.b.c[412].e.f, "
        "2.5) RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER BOOST("
        "['1','2','3'] ANY == d.a['b']['c'][412].e.f, 2.5) RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER BOOST(['1','2','3'] ANY == d.a.b.c[412].e.f, "
        "2.5) RETURN d",
        expected);
  }

  // complex attribute name with offset, boost ALL
  {
    irs::Or expected;
    auto& root = expected.add<irs::And>();
    root.boost(2.5);
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleStringIdentity("a.b.c[412].e.f");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("1"));
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleStringIdentity("a.b.c[412].e.f");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("2"));
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleStringIdentity("a.b.c[412].e.f");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("3"));
    }

    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER BOOST("
        "['1','2','3'] ALL IN d.a['b']['c'][412].e.f, 2.5) RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER BOOST(['1','2','3'] ALL IN d.a.b.c[412].e.f, "
        "2.5) RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER BOOST("
        "['1','2','3'] ALL == d.a['b']['c'][412].e.f, 2.5) RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER BOOST(['1','2','3'] ALL == d.a.b.c[412].e.f, "
        "2.5) RETURN d",
        expected);
  }
  // complex attribute name with offset, boost NONE
  {
    irs::Or expected;
    auto& root = expected.add<irs::Not>().filter<irs::Or>();
    root.boost(2.5);
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleStringIdentity("a.b.c[412].e.f");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("1"));
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleStringIdentity("a.b.c[412].e.f");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("2"));
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleStringIdentity("a.b.c[412].e.f");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("3"));
    }

    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER BOOST("
        "['1','2','3'] NONE IN d.a['b']['c'][412].e.f, 2.5) RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER BOOST(['1','2','3'] NONE IN d.a.b.c[412].e.f, "
        "2.5) RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER BOOST("
        "['1','2','3'] NONE == d.a['b']['c'][412].e.f, 2.5) RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER BOOST(['1','2','3'] NONE == d.a.b.c[412].e.f, "
        "2.5) RETURN d",
        expected);
  }

  // complex attribute name with offset, boost, analyzer ANY
  {
    irs::Or expected;
    auto& root = expected.add<irs::Or>();
    root.boost(2.5);
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleString("a.b.c[412].e.f", "test_analyzer");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("1"));
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleString("a.b.c[412].e.f", "test_analyzer");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("2"));
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleString("a.b.c[412].e.f", "test_analyzer");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("3"));
    }

    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER ANALYZER(BOOST("
        "['1','2','3'] ANY IN d.a['b']['c'][412].e.f, 2.5), 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER BOOST(ANALYZER("
        "['1','2','3'] ANY IN d.a.b.c[412].e.f, 'test_analyzer'), 2.5) RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER ANALYZER(BOOST("
        "['1','2','3'] ANY == d.a['b']['c'][412].e.f, 2.5), 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER BOOST(ANALYZER("
        "['1','2','3'] ANY == d.a.b.c[412].e.f, 'test_analyzer'), 2.5) RETURN d",
        expected);
  }
  // complex attribute name with offset, boost, analyzer ALL
  {
    irs::Or expected;
    auto& root = expected.add<irs::And>();
    root.boost(2.5);
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleString("a.b.c[412].e.f", "test_analyzer");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("1"));
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleString("a.b.c[412].e.f", "test_analyzer");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("2"));
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleString("a.b.c[412].e.f", "test_analyzer");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("3"));
    }

    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER ANALYZER(BOOST("
        "['1','2','3'] ALL IN d.a['b']['c'][412].e.f, 2.5), 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER BOOST(ANALYZER("
        "['1','2','3'] ALL IN d.a.b.c[412].e.f, 'test_analyzer'), 2.5) RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER ANALYZER(BOOST("
        "['1','2','3'] ALL == d.a['b']['c'][412].e.f, 2.5), 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER BOOST(ANALYZER("
        "['1','2','3'] ALL == d.a.b.c[412].e.f, 'test_analyzer'), 2.5) RETURN d",
        expected);
  }
  // complex attribute name with offset, boost, analyzer NONE
  {
    irs::Or expected;
    auto& root = expected.add<irs::Not>().filter<irs::Or>();
    root.boost(2.5);
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleString("a.b.c[412].e.f", "test_analyzer");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("1"));
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleString("a.b.c[412].e.f", "test_analyzer");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("2"));
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleString("a.b.c[412].e.f", "test_analyzer");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("3"));
    }

    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER ANALYZER(BOOST("
        "['1','2','3'] NONE IN d.a['b']['c'][412].e.f, 2.5), 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER BOOST(ANALYZER("
        "['1','2','3'] NONE IN d.a.b.c[412].e.f, 'test_analyzer'), 2.5) RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER ANALYZER(BOOST("
        "['1','2','3'] NONE == d.a['b']['c'][412].e.f, 2.5), 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER BOOST(ANALYZER("
        "['1','2','3'] NONE == d.a.b.c[412].e.f, 'test_analyzer'), 2.5) RETURN d",
        expected);
  }
  // heterogeneous array values, analyzer, boost ANY
  {
    irs::Or expected;
    auto& root = expected.add<irs::Or>();
    root.boost(1.5);
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleString("quick.brown.fox", "test_analyzer");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("1"));
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleNull("quick.brown.fox");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::null_token_stream::value_null());
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleBool("quick.brown.fox");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::boolean_token_stream::value_true());
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleBool("quick.brown.fox");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::boolean_token_stream::value_false());
    }
    {
      irs::numeric_token_stream stream;
      auto* term = irs::get<irs::term_attribute>(stream);
      stream.reset(2.);
      EXPECT_TRUE(stream.next());
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleNumeric("quick.brown.fox");
      filter.mutable_options()->term = term->value;
    }

    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER ANALYZER(BOOST("
        "['1',null,true,false,2] ANY IN d.quick.brown.fox, 1.5), 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER BOOST(ANALYZER( "
        "['1',null,true,false,2] ANY IN d.quick['brown'].fox, 'test_analyzer'), 1.5) RETURN d",
        expected);

    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER ANALYZER(BOOST("
        "['1',null,true,false,2] ANY == d.quick.brown.fox, 1.5), 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER BOOST(ANALYZER( "
        "['1',null,true,false,2] ANY == d.quick['brown'].fox, 'test_analyzer'), 1.5) RETURN d",
        expected);
  }
  // heterogeneous array values, analyzer, boost ALL
  {
    irs::Or expected;
    auto& root = expected.add<irs::And>();
    root.boost(1.5);
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleString("quick.brown.fox", "test_analyzer");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("1"));
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleNull("quick.brown.fox");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::null_token_stream::value_null());
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleBool("quick.brown.fox");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::boolean_token_stream::value_true());
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleBool("quick.brown.fox");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::boolean_token_stream::value_false());
    }
    {
      irs::numeric_token_stream stream;
      auto* term = irs::get<irs::term_attribute>(stream);
      stream.reset(2.);
      EXPECT_TRUE(stream.next());
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleNumeric("quick.brown.fox");
      filter.mutable_options()->term = term->value;
    }

    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER ANALYZER(BOOST("
        "['1',null,true,false,2] ALL IN d.quick.brown.fox, 1.5), 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER BOOST(ANALYZER( "
        "['1',null,true,false,2] ALL IN d.quick['brown'].fox, 'test_analyzer'), 1.5) RETURN d",
        expected);

    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER ANALYZER(BOOST("
        "['1',null,true,false,2] ALL == d.quick.brown.fox, 1.5), 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER BOOST(ANALYZER( "
        "['1',null,true,false,2] ALL == d.quick['brown'].fox, 'test_analyzer'), 1.5) RETURN d",
        expected);
  }
  // heterogeneous array values, analyzer, boost NONE
  {
    irs::Or expected;
    auto& root = expected.add<irs::Not>().filter<irs::Or>();
    root.boost(1.5);
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleString("quick.brown.fox", "test_analyzer");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("1"));
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleNull("quick.brown.fox");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::null_token_stream::value_null());
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleBool("quick.brown.fox");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::boolean_token_stream::value_true());
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleBool("quick.brown.fox");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::boolean_token_stream::value_false());
    }
    {
      irs::numeric_token_stream stream;
      auto* term = irs::get<irs::term_attribute>(stream);
      stream.reset(2.);
      EXPECT_TRUE(stream.next());
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleNumeric("quick.brown.fox");
      filter.mutable_options()->term = term->value;
    }

    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER ANALYZER(BOOST("
        "['1',null,true,false,2] NONE IN d.quick.brown.fox, 1.5), 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER BOOST(ANALYZER( "
        "['1',null,true,false,2] NONE IN d.quick['brown'].fox, 'test_analyzer'), 1.5) RETURN d",
        expected);

    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER ANALYZER(BOOST("
        "['1',null,true,false,2] NONE == d.quick.brown.fox, 1.5), 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER BOOST(ANALYZER( "
        "['1',null,true,false,2] NONE == d.quick['brown'].fox, 'test_analyzer'), 1.5) RETURN d",
        expected);
  }

  // empty array ANY
  {
    irs::Or expected;
    expected.add<irs::empty>();

    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER [] ANY IN d.quick.brown.fox RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER [] ANY IN d['quick'].brown.fox RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER [] ANY == d.quick.brown.fox RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER [] ANY == d['quick'].brown.fox RETURN d", expected);
  }

  // empty array ALL
  {
    irs::Or expected;
    expected.add<irs::all>();
    expected.boost(2.5);
    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER BOOST([] ALL IN d.quick.brown.fox, 2.5) RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER BOOST([] ALL IN d['quick'].brown.fox, 2.5) RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER BOOST([] ALL == d.quick.brown.fox, 2.5) RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER BOOST([] ALL == d['quick'].brown.fox, 2.5) RETURN d", expected);
  }

  // empty array NONE
  {
    irs::Or expected;
    expected.add<irs::all>();
    expected.boost(2.5);
    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER BOOST([] NONE IN d.quick.brown.fox, 2.5) RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER BOOST([] NONE IN d['quick'].brown.fox, 2.5) RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER BOOST([] NONE == d.quick.brown.fox, 2.5) RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER BOOST([] NONE == d['quick'].brown.fox, 2.5) RETURN d", expected);
  }

  // dynamic complex attribute name ANY
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
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleStringIdentity("a.b.c.e[4].f[5].g[3].g.a");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("1"));
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleStringIdentity("a.b.c.e[4].f[5].g[3].g.a");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("2"));
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleStringIdentity("a.b.c.e[4].f[5].g[3].g.a");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("3"));
    }

    assertFilterSuccess(
        vocbase(),
        "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER "
        " ['1','2','3'] ANY IN "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        " RETURN d",
        expected, &ctx);
  }
  // dynamic complex attribute name ALL
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
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleStringIdentity("a.b.c.e[4].f[5].g[3].g.a");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("1"));
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleStringIdentity("a.b.c.e[4].f[5].g[3].g.a");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("2"));
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleStringIdentity("a.b.c.e[4].f[5].g[3].g.a");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("3"));
    }

    assertFilterSuccess(
        vocbase(),
        "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER "
        " ['1','2','3'] ALL IN "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        " RETURN d",
        expected, &ctx);
  }
  // dynamic complex attribute name NONE
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                                      arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                                      arangodb::aql::AqlValueHintDouble{5.6})));

    irs::Or expected;
    auto& root = expected.add<irs::Not>().filter<irs::Or>();
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleStringIdentity("a.b.c.e[4].f[5].g[3].g.a");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("1"));
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleStringIdentity("a.b.c.e[4].f[5].g[3].g.a");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("2"));
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleStringIdentity("a.b.c.e[4].f[5].g[3].g.a");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("3"));
    }

    assertFilterSuccess(
        vocbase(),
        "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER "
        " ['1','2','3'] NONE IN "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
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
        "collection FILTER "
        " ['1','2','3'] ANY IN "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        " RETURN d",
        &ctx);
    assertFilterExecutionFail(
        vocbase(),
        "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER "
        " ['1','2','3'] ALL IN "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        " RETURN d",
        &ctx);
    assertFilterExecutionFail(
        vocbase(),
        "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER "
        " ['1','2','3'] NONE IN "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        " RETURN d",
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
        " ['1','2','3'] ANY IN "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        " RETURN d",
        &ctx);
    assertFilterExecutionFail(
        vocbase(),
        "LET a=null LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER "
        " ['1','2','3'] ALL IN "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        " RETURN d",
        &ctx);
    assertFilterExecutionFail(
        vocbase(),
        "LET a=null LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER "
        " ['1','2','3'] NONE IN "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        " RETURN d",
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
        "in ['1','2','3'] RETURN d",
        &ctx);
  }

  // reference in array ANY
  {
    arangodb::aql::Variable var("c", 0, /*isDataFromColl*/ false);
    arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt(2));
    arangodb::aql::AqlValueGuard guard(value, true);

    irs::numeric_token_stream stream;
    stream.reset(2.);
    EXPECT_TRUE(stream.next());
    auto* term = irs::get<irs::term_attribute>(stream);

    ExpressionContextMock ctx;
    ctx.vars.emplace(var.name, value);

    irs::Or expected;
    auto& root = expected.add<irs::Or>();
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleStringIdentity("a.b.c.e.f");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("1"));
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleNumeric("a.b.c.e.f");
      filter.mutable_options()->term = term->value;
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleStringIdentity("a.b.c.e.f");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("3"));
    }

    // not a constant in array
    assertFilterSuccess(
        vocbase(),
        "LET c=2 FOR d IN collection FILTER ['1', c, '3'] ANY IN d.a.b.c.e.f "
        "RETURN d",
        expected,
        &ctx);
  }
  // reference in array ALL
  {
    arangodb::aql::Variable var("c", 0, /*isDataFromColl*/ false);
    arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt(2));
    arangodb::aql::AqlValueGuard guard(value, true);

    irs::numeric_token_stream stream;
    stream.reset(2.);
    EXPECT_TRUE(stream.next());
    auto* term = irs::get<irs::term_attribute>(stream);

    ExpressionContextMock ctx;
    ctx.vars.emplace(var.name, value);

    irs::Or expected;
    auto& root = expected.add<irs::And>();
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleStringIdentity("a.b.c.e.f");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("1"));
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleNumeric("a.b.c.e.f");
      filter.mutable_options()->term = term->value;
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleStringIdentity("a.b.c.e.f");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("3"));
    }

    // not a constant in array
    assertFilterSuccess(
        vocbase(),
        "LET c=2 FOR d IN collection FILTER ['1', c, '3'] ALL IN d.a.b.c.e.f "
        "RETURN d",
        expected,
        &ctx);
  }
  // reference in array NONE
  {
    arangodb::aql::Variable var("c", 0, /*isDataFromColl*/ false);
    arangodb::aql::AqlValue value(arangodb::aql::AqlValueHintInt(2));
    arangodb::aql::AqlValueGuard guard(value, true);

    irs::numeric_token_stream stream;
    stream.reset(2.);
    EXPECT_TRUE(stream.next());
    auto* term = irs::get<irs::term_attribute>(stream);

    ExpressionContextMock ctx;
    ctx.vars.emplace(var.name, value);

    irs::Or expected;
    auto& root = expected.add<irs::Not>().filter<irs::Or>();
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleStringIdentity("a.b.c.e.f");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("1"));
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleNumeric("a.b.c.e.f");
      filter.mutable_options()->term = term->value;
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleStringIdentity("a.b.c.e.f");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("3"));
    }

    // not a constant in array
    assertFilterSuccess(
        vocbase(),
        "LET c=2 FOR d IN collection FILTER ['1', c, '3'] NONE IN d.a.b.c.e.f "
        "RETURN d",
        expected,
        &ctx);
  }
  // array as reference, boost, analyzer ANY
  {
    auto obj = arangodb::velocypack::Parser::fromJson("[ \"1\", 2, \"3\"]");
    arangodb::aql::AqlValue value(obj->slice());
    arangodb::aql::AqlValueGuard guard(value, true);

    irs::numeric_token_stream stream;
    stream.reset(2.);
    EXPECT_TRUE(stream.next());
    auto* term = irs::get<irs::term_attribute>(stream);

    ExpressionContextMock ctx;
    ctx.vars.emplace("x", value);

    irs::Or expected;
    auto& root = expected.add<irs::Or>();
    root.boost(1.5);
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleString("a.b.c.e.f", "test_analyzer");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("1"));
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleNumeric("a.b.c.e.f");
      filter.mutable_options()->term = term->value;
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleString("a.b.c.e.f", "test_analyzer");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("3"));
    }

    assertFilterSuccess(
        vocbase(),
        "LET x=['1', 2, '3'] FOR d IN collection FILTER "
        "ANALYZER(BOOST(x ANY IN d.a.b.c.e.f, 1.5), 'test_analyzer') RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET x=['1', 2, '3'] FOR d IN collection FILTER "
        "BOOST(ANALYZER(x ANY IN d.a.b.c.e.f, 'test_analyzer'), 1.5) RETURN d",
        expected, &ctx);
  }
  // array as reference, boost, analyzer ALL
  {
    auto obj = arangodb::velocypack::Parser::fromJson("[ \"1\", 2, \"3\"]");
    arangodb::aql::AqlValue value(obj->slice());
    arangodb::aql::AqlValueGuard guard(value, true);

    irs::numeric_token_stream stream;
    stream.reset(2.);
    EXPECT_TRUE(stream.next());
    auto* term = irs::get<irs::term_attribute>(stream);

    ExpressionContextMock ctx;
    ctx.vars.emplace("x", value);

    irs::Or expected;
    auto& root = expected.add<irs::And>();
    root.boost(1.5);
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleString("a.b.c.e.f", "test_analyzer");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("1"));
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleNumeric("a.b.c.e.f");
      filter.mutable_options()->term = term->value;
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleString("a.b.c.e.f", "test_analyzer");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("3"));
    }

    assertFilterSuccess(
        vocbase(),
        "LET x=['1', 2, '3'] FOR d IN collection FILTER "
        "ANALYZER(BOOST(x ALL IN d.a.b.c.e.f, 1.5), 'test_analyzer') RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET x=['1', 2, '3'] FOR d IN collection FILTER "
        "BOOST(ANALYZER(x ALL IN d.a.b.c.e.f, 'test_analyzer'), 1.5) RETURN d",
        expected, &ctx);
  }
  // array as reference, boost, analyzer NONE
  {
    auto obj = arangodb::velocypack::Parser::fromJson("[ \"1\", 2, \"3\"]");
    arangodb::aql::AqlValue value(obj->slice());
    arangodb::aql::AqlValueGuard guard(value, true);

    irs::numeric_token_stream stream;
    stream.reset(2.);
    EXPECT_TRUE(stream.next());
    auto* term = irs::get<irs::term_attribute>(stream);

    ExpressionContextMock ctx;
    ctx.vars.emplace("x", value);

    irs::Or expected;
    auto& root = expected.add<irs::Not>().filter<irs::Or>();
    root.boost(1.5);
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleString("a.b.c.e.f", "test_analyzer");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("1"));
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleNumeric("a.b.c.e.f");
      filter.mutable_options()->term = term->value;
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleString("a.b.c.e.f", "test_analyzer");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("3"));
    }

    assertFilterSuccess(
        vocbase(),
        "LET x=['1', 2, '3'] FOR d IN collection FILTER "
        "ANALYZER(BOOST(x NONE IN d.a.b.c.e.f, 1.5), 'test_analyzer') RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET x=['1', 2, '3'] FOR d IN collection FILTER "
        "BOOST(ANALYZER(x NONE IN d.a.b.c.e.f, 'test_analyzer'), 1.5) RETURN d",
        expected, &ctx);
  }

  // empty array ANY
  {
    irs::Or expected;
    expected.add<irs::empty>();

    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER [] ANY IN d.a RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER [] ANY IN d['a'] RETURN d", expected);

    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER [] ANY == d.a RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER [] ANY == d['a'] RETURN d", expected);
  }
  // empty array ALL/NONE
  {
    irs::Or expected;
    expected.add<irs::all>();

    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER [] ALL IN d.a RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER [] ALL IN d['a'] RETURN d", expected);

    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER [] ALL == d.a RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER [] ALL == d['a'] RETURN d", expected);

    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER [] NONE IN d.a RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER [] NONE IN d['a'] RETURN d", expected);

    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER [] NONE == d.a RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER [] NONE == d['a'] RETURN d", expected);
  }

  // Auxilary check lambdas. Need them to check root part of expected filterd
  // direct == check is not possible as we will have byExpresssion filters generated on the fly
  auto checkAny = [](irs::Or& actual, iresearch::boost_t boost) {
    EXPECT_EQ(1, actual.size());
    auto& root = dynamic_cast<const irs::Or&>(*actual.begin());
    EXPECT_EQ(irs::type<irs::Or>::id(), root.type());
    EXPECT_EQ(3, root.size());
    EXPECT_EQ(boost, root.boost());
    return root.begin();
  };
  auto checkAll = [](irs::Or& actual, iresearch::boost_t boost) {
    EXPECT_EQ(1, actual.size());
    auto& root = dynamic_cast<const irs::And&>(*actual.begin());
    EXPECT_EQ(irs::type<irs::And>::id(), root.type());
    EXPECT_EQ(3, root.size());
    EXPECT_EQ(boost, root.boost());
    return root.begin();
  };
  auto checkNone = [](irs::Or& actual, iresearch::boost_t boost) {
    EXPECT_EQ(1, actual.size());
    auto& notFilter = dynamic_cast<irs::Not&>(*actual.begin());
    auto& root = dynamic_cast<const irs::Or&>(*notFilter.filter());
    EXPECT_EQ(irs::type<irs::Or>::id(), root.type());
    EXPECT_EQ(3, root.size());
    EXPECT_EQ(boost, root.boost());
    return root.begin();
  };

  // nondeterministic value
  {
    std::vector<std::pair<std::string, std::function<irs::boolean_filter::const_iterator(irs::Or&, iresearch::boost_t)>>> const testCases = {
       { "FOR d IN collection FILTER [ '1', RAND(), '3' ] ANY IN d.a.b.c.e.f RETURN d ", checkAny},
       { "FOR d IN collection FILTER [ '1', RAND(), '3' ] ALL IN d.a.b.c.e.f RETURN d ", checkAll},
       { "FOR d IN collection FILTER [ '1', RAND(), '3' ] NONE IN d.a.b.c.e.f RETURN d ", checkNone},
       { "FOR d IN collection FILTER [ '1', RAND(), '3' ] ANY == d.a.b.c.e.f RETURN d ", checkAny},
       { "FOR d IN collection FILTER [ '1', RAND(), '3' ] ALL == d.a.b.c.e.f RETURN d ", checkAll},
       { "FOR d IN collection FILTER [ '1', RAND(), '3' ] NONE == d.a.b.c.e.f RETURN d ", checkNone}
    };

    for (auto caseData : testCases) {
      const auto& queryString = caseData.first;
      SCOPED_TRACE(testing::Message("Testing with non-determenistic value. Query: ") << queryString);
      std::string const refName = "d";

      TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));

      auto options = std::make_shared<arangodb::velocypack::Builder>();

      arangodb::aql::Query query(arangodb::transaction::StandaloneContext::Create(vocbase), arangodb::aql::QueryString(queryString),
        nullptr, options);

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


      // iteratorForCondition
      {
        arangodb::transaction::Methods trx(arangodb::transaction::StandaloneContext::Create(vocbase),
          {}, {}, {}, arangodb::transaction::Options());

        auto dummyPlan = arangodb::tests::planFromQuery(vocbase, "RETURN 1");
        
        ExpressionContextMock exprCtx;
        exprCtx.setTrx(&trx);

        irs::Or actual;
        arangodb::iresearch::QueryContext const ctx{
          &trx, dummyPlan.get(), ast,
          &exprCtx, &irs::sub_reader::empty(), ref
        };
        EXPECT_TRUE(
          (arangodb::iresearch::FilterFactory::filter(&actual, ctx, *filterNode).ok()));

        {
          auto begin = caseData.second(actual, 1);

          // 1st filter
          {
            irs::by_term expected;
            *expected.mutable_field() = mangleStringIdentity("a.b.c.e.f");
            expected.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("1"));
            EXPECT_EQ(expected, *begin);
          }

          // 2nd filter
          {
            ++begin;
            EXPECT_EQ(irs::type<arangodb::iresearch::ByExpression>::id(), begin->type());
            EXPECT_NE(nullptr,
              dynamic_cast<arangodb::iresearch::ByExpression const*>(&*begin));
          }

          // 3rd filter
          {
            ++begin;
            irs::by_term expected;
            *expected.mutable_field() = mangleStringIdentity("a.b.c.e.f");
            expected.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("3"));
            EXPECT_EQ(expected, *begin);
          }
        }
      }
    }
  }

  // self-referenced value
  {
    std::vector<std::pair<std::string, std::function<irs::boolean_filter::const_iterator(irs::Or&, iresearch::boost_t)>>> const testCases = {
      {"FOR d IN collection FILTER [ '1', d, '3' ] ANY IN d.a.b.c.e.f RETURN d", checkAny},
      {"FOR d IN collection FILTER [ '1', d, '3' ] ALL IN d.a.b.c.e.f RETURN d", checkAll},
      {"FOR d IN collection FILTER [ '1', d, '3' ] NONE IN d.a.b.c.e.f RETURN d", checkNone},
      {"FOR d IN collection FILTER [ '1', d, '3' ] ANY == d.a.b.c.e.f RETURN d", checkAny},
      {"FOR d IN collection FILTER [ '1', d, '3' ] ALL == d.a.b.c.e.f RETURN d", checkAll},
      {"FOR d IN collection FILTER [ '1', d, '3' ] NONE == d.a.b.c.e.f RETURN d", checkNone}
    };
    for (auto caseData : testCases) {
      const auto& queryString = caseData.first;
      SCOPED_TRACE(testing::Message("Testing with self-referenced value. Query: ") << queryString);
      std::string const refName = "d";

      TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));

      auto options = std::make_shared<arangodb::velocypack::Builder>();

      arangodb::aql::Query query(arangodb::transaction::StandaloneContext::Create(vocbase), arangodb::aql::QueryString(queryString),
        nullptr, options);

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
        arangodb::iresearch::QueryContext const ctx{ nullptr, nullptr, nullptr, nullptr, nullptr, ref };
        EXPECT_TRUE(
          (arangodb::iresearch::FilterFactory::filter(nullptr, ctx, *filterNode).ok()));
      }

      // iteratorForCondition
      {
        arangodb::transaction::Methods trx(arangodb::transaction::StandaloneContext::Create(vocbase),
          {}, {}, {}, arangodb::transaction::Options());

        auto dummyPlan = arangodb::tests::planFromQuery(vocbase, "RETURN 1");
        
        ExpressionContextMock exprCtx;
        exprCtx.setTrx(&trx);


        irs::Or actual;
        arangodb::iresearch::QueryContext const ctx{
          &trx, dummyPlan.get(), ast,
          &exprCtx, &irs::sub_reader::empty(), ref
        };

        EXPECT_TRUE(
          (arangodb::iresearch::FilterFactory::filter(&actual, ctx, *filterNode).ok()));

        {
          auto begin = caseData.second(actual, 1);

          // 1st filter
          {
            irs::by_term expected;
            *expected.mutable_field() = mangleStringIdentity("a.b.c.e.f");
            expected.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("1"));
            EXPECT_EQ(expected, *begin);
          }

          // 2nd filter
          {
            ++begin;
            EXPECT_EQ(irs::type<arangodb::iresearch::ByExpression>::id(), begin->type());
            EXPECT_NE(nullptr,
              dynamic_cast<arangodb::iresearch::ByExpression const*>(&*begin));
          }

          // 3rd filter
          {
            ++begin;
            irs::by_term expected;
            *expected.mutable_field() = mangleStringIdentity("a.b.c.e.f");
            expected.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("3"));
            EXPECT_EQ(expected, *begin);
          }
        }
      }
    }
  }

  // self-referenced value
  {
    std::vector<std::pair<std::string, std::function<irs::boolean_filter::const_iterator(irs::Or&, iresearch::boost_t)>>> const testCases = {
      {"FOR d IN collection FILTER [ '1', d.e, d.a.b.c.e.f ] ANY IN d.a.b.c.e.f RETURN d", checkAny},
      {"FOR d IN collection FILTER [ '1', d.e, d.a.b.c.e.f ] ALL IN d.a.b.c.e.f RETURN d", checkAll},
      {"FOR d IN collection FILTER [ '1', d.e, d.a.b.c.e.f ] NONE IN d.a.b.c.e.f RETURN d", checkNone},
      {"FOR d IN collection FILTER [ '1', d.e, d.a.b.c.e.f ] ANY == d.a.b.c.e.f RETURN d", checkAny},
      {"FOR d IN collection FILTER [ '1', d.e, d.a.b.c.e.f ] ALL == d.a.b.c.e.f RETURN d", checkAll},
      {"FOR d IN collection FILTER [ '1', d.e, d.a.b.c.e.f ] NONE == d.a.b.c.e.f RETURN d", checkNone},
    };
    for (auto caseData : testCases) {
      const auto& queryString = caseData.first;
      SCOPED_TRACE(testing::Message("Testing with self-referenced value. Query: ") << queryString);
      std::string const refName = "d";

      TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));

      auto options = std::make_shared<arangodb::velocypack::Builder>();

      arangodb::aql::Query query(arangodb::transaction::StandaloneContext::Create(vocbase), arangodb::aql::QueryString(queryString),
        nullptr, options);

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
        arangodb::iresearch::QueryContext const ctx{ nullptr, nullptr, nullptr, nullptr, nullptr, ref };
        EXPECT_TRUE(
          (arangodb::iresearch::FilterFactory::filter(nullptr, ctx, *filterNode).ok()));
      }

      // iteratorForCondition
      {
        arangodb::transaction::Methods trx(arangodb::transaction::StandaloneContext::Create(vocbase),
          {}, {}, {}, arangodb::transaction::Options());

        auto dummyPlan = arangodb::tests::planFromQuery(vocbase, "RETURN 1");

        ExpressionContextMock exprCtx;
        exprCtx.setTrx(&trx);
        
        irs::Or actual;
        arangodb::iresearch::QueryContext const ctx{
          &trx, dummyPlan.get(), ast,
          &exprCtx, &irs::sub_reader::empty(), ref };
        EXPECT_TRUE(
          (arangodb::iresearch::FilterFactory::filter(&actual, ctx, *filterNode).ok()));

        {
          auto begin = caseData.second(actual, 1);

          // 1st filter
          {
            irs::by_term expected;
            *expected.mutable_field() = mangleStringIdentity("a.b.c.e.f");
            expected.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("1"));
            EXPECT_EQ(expected, *begin);
          }

          // 2nd filter
          {
            ++begin;
            EXPECT_EQ(irs::type<arangodb::iresearch::ByExpression>::id(), begin->type());
            EXPECT_NE(nullptr,
              dynamic_cast<arangodb::iresearch::ByExpression const*>(&*begin));
          }

          // 3rd filter
          {
            ++begin;
            EXPECT_EQ(irs::type<arangodb::iresearch::ByExpression>::id(), begin->type());
            EXPECT_TRUE(nullptr !=
              dynamic_cast<arangodb::iresearch::ByExpression const*>(&*begin));
          }
        }
      }
    }
  }

  // self-referenced value
  {
      std::vector<std::pair<std::string, std::function<irs::boolean_filter::const_iterator(irs::Or&, iresearch::boost_t)>>> const testCases = {
      {"FOR d IN collection FILTER BOOST([ '1', 1+d.b, '3' ] ANY IN d.a.b.c.e.f, 2.5) RETURN d", checkAny},
      {"FOR d IN collection FILTER BOOST([ '1', 1+d.b, '3' ] ALL IN d.a.b.c.e.f, 2.5) RETURN d", checkAll},
      {"FOR d IN collection FILTER BOOST([ '1', 1+d.b, '3' ] NONE IN d.a.b.c.e.f, 2.5) RETURN d", checkNone},
      {"FOR d IN collection FILTER BOOST([ '1', 1+d.b, '3' ] ANY == d.a.b.c.e.f, 2.5) RETURN d", checkAny},
      {"FOR d IN collection FILTER BOOST([ '1', 1+d.b, '3' ] ALL == d.a.b.c.e.f, 2.5) RETURN d", checkAll},
      {"FOR d IN collection FILTER BOOST([ '1', 1+d.b, '3' ] NONE == d.a.b.c.e.f, 2.5) RETURN d", checkNone},
    };
    for (auto caseData : testCases) {
      const auto& queryString = caseData.first;
      SCOPED_TRACE(testing::Message("Testing with self-referenced value. Query: ") << queryString);
      std::string const refName = "d";

      TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));

      auto options = std::make_shared<arangodb::velocypack::Builder>();

      arangodb::aql::Query query(arangodb::transaction::StandaloneContext::Create(vocbase), arangodb::aql::QueryString(queryString),
        nullptr, options);

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
        arangodb::iresearch::QueryContext const ctx{ nullptr, nullptr, nullptr, nullptr, nullptr, ref };
        EXPECT_TRUE(
          (arangodb::iresearch::FilterFactory::filter(nullptr, ctx, *filterNode).ok()));
      }

      // iteratorForCondition
      {
        arangodb::transaction::Methods trx(arangodb::transaction::StandaloneContext::Create(vocbase),
          {}, {}, {}, arangodb::transaction::Options());

        auto dummyPlan = arangodb::tests::planFromQuery(vocbase, "RETURN 1");
        
        ExpressionContextMock exprCtx;
        exprCtx.setTrx(&trx);

        irs::Or actual;
        arangodb::iresearch::QueryContext const ctx{ &trx, dummyPlan.get(), ast,
                                                    &exprCtx, &irs::sub_reader::empty(), ref };
        EXPECT_TRUE(
          (arangodb::iresearch::FilterFactory::filter(&actual, ctx, *filterNode).ok()));

        {
          auto begin = caseData.second(actual, 2.5);

          // 1st filter
          {
            irs::by_term expected;
            *expected.mutable_field() = mangleStringIdentity("a.b.c.e.f");
            expected.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("1"));
            EXPECT_EQ(expected, *begin);
          }

          // 2nd filter
          {
            ++begin;
            EXPECT_EQ(irs::type<arangodb::iresearch::ByExpression>::id(), begin->type());
            EXPECT_NE(nullptr,
              dynamic_cast<arangodb::iresearch::ByExpression const*>(&*begin));
          }

          // 3rd filter
          {
            ++begin;
            irs::by_term expected;
            *expected.mutable_field() = mangleStringIdentity("a.b.c.e.f");
            expected.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("3"));
            EXPECT_EQ(expected, *begin);
          }
        }
      }
    }
  }
  // not array as left argument
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintBool{false}));  // invalid value type
    ctx.vars.emplace("b", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                                      arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("e", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                                      arangodb::aql::AqlValueHintDouble{5.6})));
    assertFilterExecutionFail(
        vocbase(),
        "LET a=null LET b='b' LET c=4 LET e=5.6 FOR d IN collection FILTER a ANY IN d.a RETURN d",
        &ctx);
    assertFilterExecutionFail(
        vocbase(),
        "LET a=null LET b='b' LET c=4 LET e=5.6 FOR d IN collection FILTER b ANY == d.a  RETURN d",
        &ctx);
    assertFilterExecutionFail(
        vocbase(),
        "LET a=null LET b='b' LET c=4 LET e=5.6 FOR d IN collection FILTER c ALL IN d.a RETURN d",
        &ctx);
    assertFilterExecutionFail(
        vocbase(),
        "LET a=null LET b='b' LET c=4 LET e=5.6 FOR d IN collection FILTER e ALL == d.a RETURN d",
        &ctx);
  }

  // heterogeneous references and expression in array, analyzer, boost ANY
  {
    SCOPED_TRACE("heterogeneous references and expression in array, analyzer, boost ANY");
    ExpressionContextMock ctx;
    ctx.vars.emplace("strVal", arangodb::aql::AqlValue("str"));
    ctx.vars.emplace("boolVal",
                     arangodb::aql::AqlValue(arangodb::aql::AqlValueHintBool(false)));
    ctx.vars.emplace("numVal", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt(2)));
    ctx.vars.emplace("nullVal", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintNull{}));

    irs::numeric_token_stream stream;
    stream.reset(3.);
    EXPECT_TRUE(stream.next());
    auto* term = irs::get<irs::term_attribute>(stream);

    irs::Or expected;
    auto& root = expected.add<irs::Or>();
    root.boost(2.5);
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleString("a.b.c.e.f", "test_analyzer");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("1"));
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleString("a.b.c.e.f", "test_analyzer");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("str"));
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleBool("a.b.c.e.f");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::boolean_token_stream::value_false());
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleNumeric("a.b.c.e.f");
      filter.mutable_options()->term = term->value;
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleNull("a.b.c.e.f");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::null_token_stream::value_null());
    }

    // not a constant in array
    assertFilterSuccess(
        vocbase(),
        "LET strVal='str' LET boolVal=false LET numVal=2 LET nullVal=null FOR "
        "d IN collection FILTER boost(ANALYZER(['1', strVal, "
        "boolVal, numVal+1, nullVal] ANY IN d.a.b.c.e.f, 'test_analyzer'),2.5) RETURN d",
        expected,
        &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET strVal='str' LET boolVal=false LET numVal=2 LET nullVal=null FOR "
        "d IN collection FILTER ANALYZER(boost(['1', strVal, "
        "boolVal, numVal+1, nullVal] ANY IN d.a.b.c.e.f , 2.5), 'test_analyzer') RETURN d",
        expected,
        &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET strVal='str' LET boolVal=false LET numVal=2 LET nullVal=null FOR "
        "d IN collection FILTER boost(ANALYZER(['1', strVal, "
        "boolVal, numVal+1, nullVal] ANY == d.a.b.c.e.f, 'test_analyzer'),2.5) RETURN d",
        expected,
        &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET strVal='str' LET boolVal=false LET numVal=2 LET nullVal=null FOR "
        "d IN collection FILTER ANALYZER(boost(['1', strVal, "
        "boolVal, numVal+1, nullVal] ANY == d.a.b.c.e.f , 2.5), 'test_analyzer') RETURN d",
        expected,
        &ctx);
  }
  // heterogeneous references and expression in array, analyzer, boost ALL
  {
    SCOPED_TRACE("heterogeneous references and expression in array, analyzer, boost ALL");
    ExpressionContextMock ctx;
    ctx.vars.emplace("strVal", arangodb::aql::AqlValue("str"));
    ctx.vars.emplace("boolVal",
                     arangodb::aql::AqlValue(arangodb::aql::AqlValueHintBool(false)));
    ctx.vars.emplace("numVal", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt(2)));
    ctx.vars.emplace("nullVal", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintNull{}));

    irs::numeric_token_stream stream;
    stream.reset(3.);
    EXPECT_TRUE(stream.next());
    auto* term = irs::get<irs::term_attribute>(stream);

    irs::Or expected;
    auto& root = expected.add<irs::And>();
    root.boost(2.5);
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleString("a.b.c.e.f", "test_analyzer");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("1"));
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleString("a.b.c.e.f", "test_analyzer");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("str"));
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleBool("a.b.c.e.f");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::boolean_token_stream::value_false());
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleNumeric("a.b.c.e.f");
      filter.mutable_options()->term = term->value;
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleNull("a.b.c.e.f");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::null_token_stream::value_null());
    }

    // not a constant in array
    assertFilterSuccess(
        vocbase(),
        "LET strVal='str' LET boolVal=false LET numVal=2 LET nullVal=null FOR "
        "d IN collection FILTER boost(ANALYZER(['1', strVal, "
        "boolVal, numVal+1, nullVal] ALL IN d.a.b.c.e.f, 'test_analyzer'),2.5) RETURN d",
        expected,
        &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET strVal='str' LET boolVal=false LET numVal=2 LET nullVal=null FOR "
        "d IN collection FILTER ANALYZER(boost(['1', strVal, "
        "boolVal, numVal+1, nullVal] ALL IN d.a.b.c.e.f , 2.5), 'test_analyzer') RETURN d",
        expected,
        &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET strVal='str' LET boolVal=false LET numVal=2 LET nullVal=null FOR "
        "d IN collection FILTER boost(ANALYZER(['1', strVal, "
        "boolVal, numVal+1, nullVal] ALL == d.a.b.c.e.f, 'test_analyzer'),2.5) RETURN d",
        expected,
        &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET strVal='str' LET boolVal=false LET numVal=2 LET nullVal=null FOR "
        "d IN collection FILTER ANALYZER(boost(['1', strVal, "
        "boolVal, numVal+1, nullVal] ALL == d.a.b.c.e.f , 2.5), 'test_analyzer') RETURN d",
        expected,
        &ctx);
  }
  // heterogeneous references and expression in array, analyzer, boost NONE
  {
    SCOPED_TRACE("heterogeneous references and expression in array, analyzer, boost NONE");
    ExpressionContextMock ctx;
    ctx.vars.emplace("strVal", arangodb::aql::AqlValue("str"));
    ctx.vars.emplace("boolVal",
                     arangodb::aql::AqlValue(arangodb::aql::AqlValueHintBool(false)));
    ctx.vars.emplace("numVal", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt(2)));
    ctx.vars.emplace("nullVal", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintNull{}));

    irs::numeric_token_stream stream;
    stream.reset(3.);
    EXPECT_TRUE(stream.next());
    auto* term = irs::get<irs::term_attribute>(stream);

    irs::Or expected;
    auto& root = expected.add<irs::Not>().filter<irs::Or>();
    root.boost(2.5);
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleString("a.b.c.e.f", "test_analyzer");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("1"));
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleString("a.b.c.e.f", "test_analyzer");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("str"));
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleBool("a.b.c.e.f");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::boolean_token_stream::value_false());
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleNumeric("a.b.c.e.f");
      filter.mutable_options()->term = term->value;
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleNull("a.b.c.e.f");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::null_token_stream::value_null());
    }

    // not a constant in array
    assertFilterSuccess(
        vocbase(),
        "LET strVal='str' LET boolVal=false LET numVal=2 LET nullVal=null FOR "
        "d IN collection FILTER boost(ANALYZER(['1', strVal, "
        "boolVal, numVal+1, nullVal] NONE IN d.a.b.c.e.f, 'test_analyzer'),2.5) RETURN d",
        expected,
        &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET strVal='str' LET boolVal=false LET numVal=2 LET nullVal=null FOR "
        "d IN collection FILTER ANALYZER(boost(['1', strVal, "
        "boolVal, numVal+1, nullVal] NONE IN d.a.b.c.e.f , 2.5), 'test_analyzer') RETURN d",
        expected,
        &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET strVal='str' LET boolVal=false LET numVal=2 LET nullVal=null FOR "
        "d IN collection FILTER boost(ANALYZER(['1', strVal, "
        "boolVal, numVal+1, nullVal] NONE == d.a.b.c.e.f, 'test_analyzer'),2.5) RETURN d",
        expected,
        &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET strVal='str' LET boolVal=false LET numVal=2 LET nullVal=null FOR "
        "d IN collection FILTER ANALYZER(boost(['1', strVal, "
        "boolVal, numVal+1, nullVal] NONE == d.a.b.c.e.f , 2.5), 'test_analyzer') RETURN d",
        expected,
        &ctx);
  }

  // self-reference
  assertExpressionFilter(vocbase(),
                         "FOR d IN myView FILTER [1,2,'3'] ANY IN d RETURN d");
  assertExpressionFilter(vocbase(),
                         "FOR d IN myView FILTER [1,2,'3'] ALL IN d RETURN d");
  assertExpressionFilter(vocbase(),
                         "FOR d IN myView FILTER [1,2,'3'] NONE IN d RETURN d");
  assertExpressionFilter(vocbase(),
                         "FOR d IN myView FILTER [1,2,'3'] ANY == d RETURN d");
  assertExpressionFilter(vocbase(),
                         "FOR d IN myView FILTER [1,2,'3'] ALL == d RETURN d");
  assertExpressionFilter(vocbase(),
                         "FOR d IN myView FILTER [1,2,'3'] NONE == d RETURN d");

  // non-deterministic expression name in array
  assertExpressionFilter(
      vocbase(),
      "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
      "collection FILTER "
      " ['1','2','3'] ANY IN d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_NONDETERM_('a')]  RETURN d");
  assertExpressionFilter(
      vocbase(),
      "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
      "collection FILTER "
      " ['1','2','3'] ALL IN d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_NONDETERM_('a')]  RETURN d");
  assertExpressionFilter(
      vocbase(),
      "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
      "collection FILTER "
      " ['1','2','3'] NONE IN d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_NONDETERM_('a')]  RETURN d");
  assertExpressionFilter(
      vocbase(),
      "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
      "collection FILTER "
      " ['1','2','3'] ANY == d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_NONDETERM_('a')]  RETURN d");
  assertExpressionFilter(
      vocbase(),
      "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
      "collection FILTER "
      " ['1','2','3'] ALL == d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_NONDETERM_('a')]  RETURN d");
  assertExpressionFilter(
      vocbase(),
      "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
      "collection FILTER "
      " ['1','2','3'] NONE == d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_NONDETERM_('a')]  RETURN d");

  // no reference provided
  assertFilterExecutionFail(
      vocbase(), "LET x={} FOR d IN myView FILTER [1,x.a,3] ANY IN d.a RETURN d",
      &ExpressionContextMock::EMPTY);
  assertFilterExecutionFail(
      vocbase(), "LET x={} FOR d IN myView FILTER [1,x.a,3] ALL IN d.a RETURN d",
      &ExpressionContextMock::EMPTY);
  assertFilterExecutionFail(
      vocbase(), "LET x={} FOR d IN myView FILTER [1,x.a,3] NONE IN d.a RETURN d",
      &ExpressionContextMock::EMPTY);
  assertFilterExecutionFail(
      vocbase(), "LET x={} FOR d IN myView FILTER [1,x.a,3] ANY == d.a RETURN d",
      &ExpressionContextMock::EMPTY);
  assertFilterExecutionFail(
      vocbase(), "LET x={} FOR d IN myView FILTER [1,x.a,3] ALL == d.a RETURN d",
      &ExpressionContextMock::EMPTY);
  assertFilterExecutionFail(
      vocbase(), "LET x={} FOR d IN myView FILTER [1,x.a,3] NONE == d.a RETURN d",
      &ExpressionContextMock::EMPTY);

  // not a value in array
  assertFilterFail(
      vocbase(), "FOR d IN collection FILTER ['1',['2'],'3'] ANY IN d.a RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN collection FILTER ['1', {\"abc\": \"def\"},'3'] ANY IN d.a RETURN d");
  assertFilterFail(
      vocbase(), "FOR d IN collection FILTER ['1',['2'],'3'] ANY == d.a RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN collection FILTER ['1', {\"abc\": \"def\"},'3'] ANY == d.a RETURN d");
  assertFilterFail(
      vocbase(), "FOR d IN collection FILTER ['1',['2'],'3'] ALL IN d.a RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN collection FILTER ['1', {\"abc\": \"def\"},'3'] ALL IN d.a RETURN d");
  assertFilterFail(
      vocbase(), "FOR d IN collection FILTER ['1',['2'],'3'] ALL == d.a RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN collection FILTER ['1', {\"abc\": \"def\"},'3'] ALL == d.a RETURN d");
  assertFilterFail(
      vocbase(), "FOR d IN collection FILTER ['1',['2'],'3'] NONE IN d.a RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN collection FILTER ['1', {\"abc\": \"def\"},'3'] NONE IN d.a RETURN d");
  assertFilterFail(
      vocbase(), "FOR d IN collection FILTER ['1',['2'],'3'] NONE == d.a RETURN d");
  assertFilterFail(
      vocbase(),
      "FOR d IN collection FILTER ['1', {\"abc\": \"def\"},'3'] NONE == d.a RETURN d");
}

TEST_F(IResearchFilterArrayInTest, BinaryNotIn) {
  // simple attribute ANY
  {
    irs::Or expected;
    auto& root = expected.add<irs::Not>().filter<irs::And>();
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleStringIdentity("a");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("1"));
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleStringIdentity("a");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("2"));
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleStringIdentity("a");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("3"));
    }

    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER ['1','2','3'] ANY NOT IN d.a RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER ['1','2','3'] ANY NOT IN d['a'] RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER ['1','2','3'] ANY != d.a RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER ['1','2','3'] ANY != d['a'] RETURN d", expected);
  }

  // simple attribute ALL
  {
    irs::Or expected;
    auto& root = expected.add<irs::Not>().filter<irs::Or>();
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleStringIdentity("a");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("1"));
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleStringIdentity("a");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("2"));
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleStringIdentity("a");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("3"));
    }

    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER ['1','2','3'] ALL NOT IN d.a RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER ['1','2','3'] ALL NOT IN d['a'] RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER ['1','2','3'] ALL != d.a RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER ['1','2','3'] ALL != d['a'] RETURN d", expected);
  }

  // simple attribute NONE
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
      *filter.mutable_field() = mangleStringIdentity("a");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("2"));
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleStringIdentity("a");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("3"));
    }

    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER ['1','2','3'] NONE NOT IN d.a RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER ['1','2','3'] NONE NOT IN d['a'] RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER ['1','2','3'] NONE != d.a RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER ['1','2','3'] NONE != d['a'] RETURN d", expected);
  }

  // simple offset ANY
  {
    irs::Or expected;
    auto& root = expected.add<irs::Not>().filter<irs::And>();
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleStringIdentity("[1]");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("1"));
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleStringIdentity("[1]");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("2"));
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleStringIdentity("[1]");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("3"));
    }

    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER ['1','2','3'] ANY NOT IN d[1] RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER ['1','2','3'] ANY != d[1] RETURN d", expected);
  }

  // simple offset ALL
  {
    irs::Or expected;
    auto& root = expected.add<irs::Not>().filter<irs::Or>();
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleStringIdentity("[1]");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("1"));
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleStringIdentity("[1]");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("2"));
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleStringIdentity("[1]");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("3"));
    }

    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER ['1','2','3'] ALL NOT IN d[1] RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER ['1','2','3'] ALL != d[1] RETURN d", expected);
  }

  // simple offset NONE
  {
    irs::Or expected;
    auto& root = expected.add<irs::And>();
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleStringIdentity("[1]");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("1"));
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleStringIdentity("[1]");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("2"));
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleStringIdentity("[1]");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("3"));
    }

    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER ['1','2','3'] NONE NOT IN d[1] RETURN d", expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER ['1','2','3'] NONE != d[1] RETURN d", expected);
  }

  // complex attribute name, offset, analyzer, boost ANY
  {
    irs::Or expected;
    auto& root = expected.add<irs::Not>().filter<irs::And>();
    root.boost(2.5);
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleString("a.b.c[323].e.f", "test_analyzer");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("1"));
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleString("a.b.c[323].e.f", "test_analyzer");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("2"));
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleString("a.b.c[323].e.f", "test_analyzer");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("3"));
    }

    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER boost(analyzer( "
        "['1','2','3'] ANY NOT IN d.a.b.c[323].e.f , 'test_analyzer'), 2.5) RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER analyzer(boost( "
        "['1','2','3'] ANY NOT IN d.a['b'].c[323].e.f, 2.5), 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER boost(analyzer("
        "['1','2','3'] ANY NOT IN d.a['b']['c'][323].e.f, 'test_analyzer'), 2.5) RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER boost(analyzer( "
        "['1','2','3'] ANY != d.a.b.c[323].e.f , 'test_analyzer'), 2.5) RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER analyzer(boost( "
        "['1','2','3'] ANY != d.a['b'].c[323].e.f, 2.5), 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER boost(analyzer("
        "['1','2','3'] ANY != d.a['b']['c'][323].e.f, 'test_analyzer'), 2.5) RETURN d",
        expected);
  }
  // complex attribute name, offset, analyzer, boost ALL
  {
    irs::Or expected;
    auto& root = expected.add<irs::Not>().filter<irs::Or>();
    root.boost(2.5);
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleString("a.b.c[323].e.f", "test_analyzer");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("1"));
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleString("a.b.c[323].e.f", "test_analyzer");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("2"));
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleString("a.b.c[323].e.f", "test_analyzer");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("3"));
    }

    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER boost(analyzer( "
        "['1','2','3'] ALL NOT IN d.a.b.c[323].e.f , 'test_analyzer'), 2.5) RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER analyzer(boost( "
        "['1','2','3'] ALL NOT IN d.a['b'].c[323].e.f, 2.5), 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER boost(analyzer("
        "['1','2','3'] ALL NOT IN d.a['b']['c'][323].e.f, 'test_analyzer'), 2.5) RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER boost(analyzer( "
        "['1','2','3'] ALL != d.a.b.c[323].e.f , 'test_analyzer'), 2.5) RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER analyzer(boost( "
        "['1','2','3'] ALL != d.a['b'].c[323].e.f, 2.5), 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER boost(analyzer("
        "['1','2','3'] ALL != d.a['b']['c'][323].e.f, 'test_analyzer'), 2.5) RETURN d",
        expected);
  }
  // complex attribute name, offset, analyzer, boost NONE
  {
    irs::Or expected;
    auto& root = expected.add<irs::And>();
    root.boost(2.5);
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleString("a.b.c[323].e.f", "test_analyzer");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("1"));
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleString("a.b.c[323].e.f", "test_analyzer");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("2"));
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleString("a.b.c[323].e.f", "test_analyzer");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("3"));
    }

    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER boost(analyzer( "
        "['1','2','3'] NONE NOT IN d.a.b.c[323].e.f , 'test_analyzer'), 2.5) RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER analyzer(boost( "
        "['1','2','3'] NONE NOT IN d.a['b'].c[323].e.f, 2.5), 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER boost(analyzer("
        "['1','2','3'] NONE NOT IN d.a['b']['c'][323].e.f, 'test_analyzer'), 2.5) RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER boost(analyzer( "
        "['1','2','3'] NONE != d.a.b.c[323].e.f , 'test_analyzer'), 2.5) RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER analyzer(boost( "
        "['1','2','3'] NONE != d.a['b'].c[323].e.f, 2.5), 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER boost(analyzer("
        "['1','2','3'] NONE != d.a['b']['c'][323].e.f, 'test_analyzer'), 2.5) RETURN d",
        expected);
  }
  // heterogeneous array values, analyzer, boost ANY
  {
    irs::Or expected;
    auto& root = expected.add<irs::Not>().filter<irs::And>();
    root.boost(1.5);
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleString("quick.brown.fox", "test_analyzer");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("1"));
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleNull("quick.brown.fox");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::null_token_stream::value_null());
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleBool("quick.brown.fox");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::boolean_token_stream::value_true());
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleBool("quick.brown.fox");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::boolean_token_stream::value_false());
    }
    {
      irs::numeric_token_stream stream;
      auto* term = irs::get<irs::term_attribute>(stream);
      stream.reset(2.);
      EXPECT_TRUE(stream.next());

      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleNumeric("quick.brown.fox");
      filter.mutable_options()->term = term->value;
    }

    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER BOOST(ANALYZER(['1',null,true,false,2] ANY NOT IN "
        "d.quick.brown.fox, 'test_analyzer'), 1.5) RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER ANALYZER(BOOST(['1',null,true,false,2] ANY NOT IN "
        "d.quick['brown'].fox, 1.5), 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER BOOST(ANALYZER(['1',null,true,false,2] ANY != "
        "d.quick.brown.fox, 'test_analyzer'), 1.5) RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER ANALYZER(BOOST(['1',null,true,false,2] ANY != "
        "d.quick['brown'].fox, 1.5), 'test_analyzer') RETURN d",
        expected);
  }
  // heterogeneous array values, analyzer, boost ALL
  {
    irs::Or expected;
    auto& root = expected.add<irs::Not>().filter<irs::Or>();
    root.boost(1.5);
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleString("quick.brown.fox", "test_analyzer");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("1"));
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleNull("quick.brown.fox");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::null_token_stream::value_null());
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleBool("quick.brown.fox");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::boolean_token_stream::value_true());
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleBool("quick.brown.fox");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::boolean_token_stream::value_false());
    }
    {
      irs::numeric_token_stream stream;
      auto* term = irs::get<irs::term_attribute>(stream);
      stream.reset(2.);
      EXPECT_TRUE(stream.next());

      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleNumeric("quick.brown.fox");
      filter.mutable_options()->term = term->value;
    }

    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER BOOST(ANALYZER(['1',null,true,false,2] ALL NOT IN "
        "d.quick.brown.fox, 'test_analyzer'), 1.5) RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER ANALYZER(BOOST(['1',null,true,false,2] ALL NOT IN "
        "d.quick['brown'].fox, 1.5), 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER BOOST(ANALYZER(['1',null,true,false,2] ALL != "
        "d.quick.brown.fox, 'test_analyzer'), 1.5) RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER ANALYZER(BOOST(['1',null,true,false,2] ALL != "
        "d.quick['brown'].fox, 1.5), 'test_analyzer') RETURN d",
        expected);
  }
  // heterogeneous array values, analyzer, boost NONE
  {
    irs::Or expected;
    auto& root = expected.add<irs::And>();
    root.boost(1.5);
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleString("quick.brown.fox", "test_analyzer");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("1"));
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleNull("quick.brown.fox");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::null_token_stream::value_null());
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleBool("quick.brown.fox");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::boolean_token_stream::value_true());
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleBool("quick.brown.fox");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::boolean_token_stream::value_false());
    }
    {
      irs::numeric_token_stream stream;
      auto* term = irs::get<irs::term_attribute>(stream);
      stream.reset(2.);
      EXPECT_TRUE(stream.next());

      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleNumeric("quick.brown.fox");
      filter.mutable_options()->term = term->value;
    }

    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER BOOST(ANALYZER(['1',null,true,false,2] NONE NOT IN "
        "d.quick.brown.fox, 'test_analyzer'), 1.5) RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER ANALYZER(BOOST(['1',null,true,false,2] NONE NOT IN "
        "d.quick['brown'].fox, 1.5), 'test_analyzer') RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER BOOST(ANALYZER(['1',null,true,false,2] NONE != "
        "d.quick.brown.fox, 'test_analyzer'), 1.5) RETURN d",
        expected);
    assertFilterSuccess(
        vocbase(),
        "FOR d IN collection FILTER ANALYZER(BOOST(['1',null,true,false,2] NONE != "
        "d.quick['brown'].fox, 1.5), 'test_analyzer') RETURN d",
        expected);
  }

  // dynamic complex attribute name ANY
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
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleStringIdentity("a.b.c.e[4].f[5].g[3].g.a");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("1"));
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleStringIdentity("a.b.c.e[4].f[5].g[3].g.a");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("2"));
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleStringIdentity("a.b.c.e[4].f[5].g[3].g.a");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("3"));
    }

    assertFilterSuccess(
        vocbase(),
        "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER "
        " ['1','2','3'] ANY NOT IN "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER "
        " ['1','2','3'] ANY != "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] RETURN d",
        expected, &ctx);
  }
  // dynamic complex attribute name ALL
  {
    ExpressionContextMock ctx;
    ctx.vars.emplace("a", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"a"}));
    ctx.vars.emplace("c", arangodb::aql::AqlValue(arangodb::aql::AqlValue{"c"}));
    ctx.vars.emplace("offsetInt", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                                      arangodb::aql::AqlValueHintInt{4})));
    ctx.vars.emplace("offsetDbl", arangodb::aql::AqlValue(arangodb::aql::AqlValue(
                                      arangodb::aql::AqlValueHintDouble{5.6})));

    irs::Or expected;
    auto& root = expected.add<irs::Not>().filter<irs::Or>();
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleStringIdentity("a.b.c.e[4].f[5].g[3].g.a");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("1"));
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleStringIdentity("a.b.c.e[4].f[5].g[3].g.a");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("2"));
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleStringIdentity("a.b.c.e[4].f[5].g[3].g.a");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("3"));
    }

    assertFilterSuccess(
        vocbase(),
        "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER "
        " ['1','2','3'] ALL NOT IN "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER "
        " ['1','2','3'] ALL != "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] RETURN d",
        expected, &ctx);
  }
  // dynamic complex attribute name NONE
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
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleStringIdentity("a.b.c.e[4].f[5].g[3].g.a");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("1"));
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleStringIdentity("a.b.c.e[4].f[5].g[3].g.a");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("2"));
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleStringIdentity("a.b.c.e[4].f[5].g[3].g.a");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("3"));
    }

    assertFilterSuccess(
        vocbase(),
        "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER "
        " ['1','2','3'] NONE NOT IN "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER "
        " ['1','2','3'] NONE != "
        "d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] RETURN d",
        expected, &ctx);
  }

  // invalid dynamic attribute name ANY
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
        "['1','2','3'] ANY NOT IN d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "RETURN d",
        &ctx);
    assertFilterExecutionFail(
        vocbase(),
        "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER "
        "['1','2','3'] ANY != d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "RETURN d",
        &ctx);
  }
  // invalid dynamic attribute name ALL
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
        "['1','2','3'] ALL NOT IN d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "RETURN d",
        &ctx);
    assertFilterExecutionFail(
        vocbase(),
        "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER "
        "['1','2','3'] ALL != d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "RETURN d",
        &ctx);
  }
  // invalid dynamic attribute name NONE
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
        "['1','2','3'] NONE NOT IN d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "RETURN d",
        &ctx);
    assertFilterExecutionFail(
        vocbase(),
        "LET a='a' LET c='c' LET offsetInt=4 LET offsetDbl=5.6 FOR d IN "
        "collection FILTER "
        "['1','2','3'] NONE != d[a].b[c].e[offsetInt].f[offsetDbl].g[_FORWARD_(3)].g[_FORWARD_('a')] "
        "RETURN d",
        &ctx);
  }

  // array as reference, analyzer, boost ANY
  {
    auto obj = arangodb::velocypack::Parser::fromJson("[ \"1\", 2, \"3\"]");
    arangodb::aql::AqlValue value(obj->slice());
    arangodb::aql::AqlValueGuard guard(value, true);

    irs::numeric_token_stream stream;
    stream.reset(2.);
    EXPECT_TRUE(stream.next());
    auto* term = irs::get<irs::term_attribute>(stream);

    ExpressionContextMock ctx;
    ctx.vars.emplace("x", value);

    irs::Or expected;
    auto& root = expected.add<irs::Not>().filter<irs::And>();
    root.boost(3.5);
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleString("a.b.c.e.f", "test_analyzer");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("1"));
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleNumeric("a.b.c.e.f");
      filter.mutable_options()->term = term->value;
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleString("a.b.c.e.f", "test_analyzer");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("3"));
    }

    assertFilterSuccess(
        vocbase(),
        "LET x=['1', 2, '3'] FOR d IN collection FILTER "
        "boost(analyzer(x ANY NOT IN d.a.b.c.e.f, 'test_analyzer'), 3.5) RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET x=['1', 2, '3'] FOR d IN collection FILTER "
        "analyzer(boost(x ANY NOT IN d.a.b.c.e.f, 3.5), 'test_analyzer') RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET x=['1', 2, '3'] FOR d IN collection FILTER "
        "boost(analyzer(x ANY != d.a.b.c.e.f, 'test_analyzer'), 3.5) RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET x=['1', 2, '3'] FOR d IN collection FILTER "
        "analyzer(boost(x ANY != d.a.b.c.e.f, 3.5), 'test_analyzer') RETURN d",
        expected, &ctx);
  }

  // array as reference, analyzer, boost ALL
  {
    auto obj = arangodb::velocypack::Parser::fromJson("[ \"1\", 2, \"3\"]");
    arangodb::aql::AqlValue value(obj->slice());
    arangodb::aql::AqlValueGuard guard(value, true);

    irs::numeric_token_stream stream;
    stream.reset(2.);
    EXPECT_TRUE(stream.next());
    auto* term = irs::get<irs::term_attribute>(stream);

    ExpressionContextMock ctx;
    ctx.vars.emplace("x", value);

    irs::Or expected;
    auto& root = expected.add<irs::Not>().filter<irs::Or>();
    root.boost(3.5);
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleString("a.b.c.e.f", "test_analyzer");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("1"));
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleNumeric("a.b.c.e.f");
      filter.mutable_options()->term = term->value;
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleString("a.b.c.e.f", "test_analyzer");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("3"));
    }

    assertFilterSuccess(
        vocbase(),
        "LET x=['1', 2, '3'] FOR d IN collection FILTER "
        "boost(analyzer(x ALL NOT IN d.a.b.c.e.f, 'test_analyzer'), 3.5) RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET x=['1', 2, '3'] FOR d IN collection FILTER "
        "analyzer(boost(x ALL NOT IN d.a.b.c.e.f, 3.5), 'test_analyzer') RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET x=['1', 2, '3'] FOR d IN collection FILTER "
        "boost(analyzer(x ALL != d.a.b.c.e.f, 'test_analyzer'), 3.5) RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET x=['1', 2, '3'] FOR d IN collection FILTER "
        "analyzer(boost(x ALL != d.a.b.c.e.f, 3.5), 'test_analyzer') RETURN d",
        expected, &ctx);
  }

  // array as reference, analyzer, boost NONE
  {
    auto obj = arangodb::velocypack::Parser::fromJson("[ \"1\", 2, \"3\"]");
    arangodb::aql::AqlValue value(obj->slice());
    arangodb::aql::AqlValueGuard guard(value, true);

    irs::numeric_token_stream stream;
    stream.reset(2.);
    EXPECT_TRUE(stream.next());
    auto* term = irs::get<irs::term_attribute>(stream);

    ExpressionContextMock ctx;
    ctx.vars.emplace("x", value);

    irs::Or expected;
    auto& root = expected.add<irs::And>();
    root.boost(3.5);
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleString("a.b.c.e.f", "test_analyzer");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("1"));
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleNumeric("a.b.c.e.f");
      filter.mutable_options()->term = term->value;
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleString("a.b.c.e.f", "test_analyzer");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("3"));
    }

    assertFilterSuccess(
        vocbase(),
        "LET x=['1', 2, '3'] FOR d IN collection FILTER "
        "boost(analyzer(x NONE NOT IN d.a.b.c.e.f, 'test_analyzer'), 3.5) RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET x=['1', 2, '3'] FOR d IN collection FILTER "
        "analyzer(boost(x NONE NOT IN d.a.b.c.e.f, 3.5), 'test_analyzer') RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET x=['1', 2, '3'] FOR d IN collection FILTER "
        "boost(analyzer(x NONE != d.a.b.c.e.f, 'test_analyzer'), 3.5) RETURN d",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        "LET x=['1', 2, '3'] FOR d IN collection FILTER "
        "analyzer(boost(x NONE != d.a.b.c.e.f, 3.5), 'test_analyzer') RETURN d",
        expected, &ctx);
  }
  // Auxilary check lambdas. Need them to check root part of expected filterd
  // direct == check is not possible as we will have byExpresssion filters generated on the fly
  auto checkNotAny = [](irs::Or& actual, iresearch::boost_t boost) {
    EXPECT_EQ(1, actual.size());
    auto & notFilter = dynamic_cast<irs::Not&>(*actual.begin());
    auto& root = dynamic_cast<const irs::And&>(*notFilter.filter());
    EXPECT_EQ(irs::type<irs::And>::id(), root.type());
    EXPECT_EQ(3, root.size());
    EXPECT_EQ(boost, root.boost());
    return root.begin();
  };
  auto checkNotAll = [](irs::Or& actual, iresearch::boost_t boost) {
    EXPECT_EQ(1, actual.size());
    auto & notFilter = dynamic_cast<irs::Not&>(*actual.begin());
    auto& root = dynamic_cast<const irs::Or&>(*notFilter.filter());
    EXPECT_EQ(irs::type<irs::Or>::id(), root.type());
    EXPECT_EQ(3, root.size());
    EXPECT_EQ(boost, root.boost());
    return root.begin();
  };
  auto checkNotNone = [](irs::Or& actual, iresearch::boost_t boost) {
    EXPECT_EQ(1, actual.size());
    auto& root = dynamic_cast<const irs::And&>(*actual.begin());
    EXPECT_EQ(irs::type<irs::And>::id(), root.type());
    EXPECT_EQ(3, root.size());
    EXPECT_EQ(boost, root.boost());
    return root.begin();
  };
  // nondeterministic value
  {
    std::vector<std::pair<std::string, std::function<irs::boolean_filter::const_iterator(irs::Or&, iresearch::boost_t)>>> const testCases = {
      {"FOR d IN collection FILTER [ '1', RAND(), '3' ] ANY NOT IN d.a.b.c.e.f RETURN d", checkNotAny},
      {"FOR d IN collection FILTER [ '1', RAND(), '3' ] ALL NOT IN d.a.b.c.e.f RETURN d", checkNotAll},
      {"FOR d IN collection FILTER [ '1', RAND(), '3' ] NONE NOT IN d.a.b.c.e.f RETURN d", checkNotNone},
      {"FOR d IN collection FILTER [ '1', RAND(), '3' ] ANY != d.a.b.c.e.f RETURN d", checkNotAny},
      {"FOR d IN collection FILTER [ '1', RAND(), '3' ] ALL != d.a.b.c.e.f RETURN d", checkNotAll},
      {"FOR d IN collection FILTER [ '1', RAND(), '3' ] NONE != d.a.b.c.e.f RETURN d", checkNotNone}
    };
    for (auto& testData : testCases) {
      auto const& queryString = testData.first;
      SCOPED_TRACE(testing::Message("Query: ") << queryString);
      std::string const refName = "d";

      TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));

      auto options = std::make_shared<arangodb::velocypack::Builder>();

      arangodb::aql::Query query(arangodb::transaction::StandaloneContext::Create(vocbase), arangodb::aql::QueryString(queryString),
        nullptr, options);

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
        arangodb::iresearch::QueryContext const ctx{ nullptr, nullptr, nullptr, nullptr, nullptr, ref };
        EXPECT_TRUE(
          (arangodb::iresearch::FilterFactory::filter(nullptr, ctx, *filterNode).ok()));
      }

      // iteratorForCondition
      {
        arangodb::transaction::Methods trx(arangodb::transaction::StandaloneContext::Create(vocbase),
          {}, {}, {}, arangodb::transaction::Options());

        auto dummyPlan = arangodb::tests::planFromQuery(vocbase, "RETURN 1");
        
        ExpressionContextMock exprCtx;
        exprCtx.setTrx(&trx);

        irs::Or actual;
        arangodb::iresearch::QueryContext const ctx{ &trx, dummyPlan.get(), ast,
                                                     &exprCtx, &irs::sub_reader::empty(), ref };
        EXPECT_TRUE(
          (arangodb::iresearch::FilterFactory::filter(&actual, ctx, *filterNode).ok()));

        {
          auto begin = testData.second(actual, 1);

          // 1st filter
          {
            irs::by_term expected;
            *expected.mutable_field() = mangleStringIdentity("a.b.c.e.f");
            expected.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("1"));
            EXPECT_EQ(expected, *begin);
          }

          // 2nd filter
          {
            ++begin;
            EXPECT_EQ(irs::type<arangodb::iresearch::ByExpression>::id(), begin->type());
            EXPECT_NE(nullptr,
              dynamic_cast<arangodb::iresearch::ByExpression const*>(&*begin));
          }

          // 3rd filter
          {
            ++begin;
            irs::by_term expected;
            *expected.mutable_field() = mangleStringIdentity("a.b.c.e.f");
            expected.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("3"));
            EXPECT_EQ(expected, *begin);
          }
        }
      }
    }
  }

  // self-referenced value
  {
    std::vector<std::pair<std::string, std::function<irs::boolean_filter::const_iterator(irs::Or&, iresearch::boost_t)>>> const testCases = {
      {"FOR d IN collection FILTER [ '1', d.a, '3' ] ANY NOT IN d.a.b.c.e.f RETURN d", checkNotAny},
      {"FOR d IN collection FILTER [ '1', d.a, '3' ] ALL NOT IN d.a.b.c.e.f RETURN d", checkNotAll},
      {"FOR d IN collection FILTER [ '1', d.a, '3' ] NONE NOT IN d.a.b.c.e.f RETURN d", checkNotNone},
      {"FOR d IN collection FILTER [ '1', d.a, '3' ] ANY != d.a.b.c.e.f RETURN d", checkNotAny},
      {"FOR d IN collection FILTER [ '1', d.a, '3' ] ALL != d.a.b.c.e.f RETURN d", checkNotAll},
      {"FOR d IN collection FILTER [ '1', d.a, '3' ] NONE != d.a.b.c.e.f RETURN d", checkNotNone}
    };
    for (auto testData : testCases) {
      auto const& queryString = testData.first;
      SCOPED_TRACE(testing::Message("Query:") << queryString);

      std::string const refName = "d";

      TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));

      auto options = std::make_shared<arangodb::velocypack::Builder>();

      arangodb::aql::Query query(arangodb::transaction::StandaloneContext::Create(vocbase), arangodb::aql::QueryString(queryString),
        nullptr, options);

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
        arangodb::iresearch::QueryContext const ctx{ nullptr, nullptr, nullptr, nullptr, nullptr, ref };
        EXPECT_TRUE(
          (arangodb::iresearch::FilterFactory::filter(nullptr, ctx, *filterNode).ok()));
      }

      // iteratorForCondition
      {
        arangodb::transaction::Methods trx(arangodb::transaction::StandaloneContext::Create(vocbase),
          {}, {}, {}, arangodb::transaction::Options());

        auto dummyPlan = arangodb::tests::planFromQuery(vocbase, "RETURN 1");
        
        ExpressionContextMock exprCtx;
        exprCtx.setTrx(&trx);

        irs::Or actual;
        arangodb::iresearch::QueryContext const ctx{ &trx, dummyPlan.get(), ast,
                                                    &exprCtx,
                                                    &irs::sub_reader::empty(), ref };
        EXPECT_TRUE(
          (arangodb::iresearch::FilterFactory::filter(&actual, ctx, *filterNode).ok()));

        {
          auto begin = testData.second(actual, 1);

          // 1st filter
          {
            irs::by_term expected;
            *expected.mutable_field() = mangleStringIdentity("a.b.c.e.f");
            expected.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("1"));
            EXPECT_EQ(expected, *begin);
          }

          // 2nd filter
          {
            ++begin;
            EXPECT_EQ(irs::type<arangodb::iresearch::ByExpression>::id(), begin->type());
            EXPECT_NE(nullptr,
              dynamic_cast<arangodb::iresearch::ByExpression const*>(&*begin));
          }

          // 3rd filter
          {
            ++begin;
            irs::by_term expected;
            *expected.mutable_field() = mangleStringIdentity("a.b.c.e.f");
            expected.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("3"));
            EXPECT_EQ(expected, *begin);
          }
        }
      }
    }
  }

  // self-referenced value, boost
  {
    std::vector<std::pair<std::string, std::function<irs::boolean_filter::const_iterator(irs::Or&, iresearch::boost_t)>>> const testCases = {
      {"FOR d IN collection FILTER boost([ '1', 1+d.a, '3'] ANY NOT IN d.a.b.c.e.f, 1.5) RETURN d", checkNotAny},
      {"FOR d IN collection FILTER boost([ '1', 1+d.a, '3'] ALL NOT IN d.a.b.c.e.f, 1.5) RETURN d", checkNotAll},
      {"FOR d IN collection FILTER boost([ '1', 1+d.a, '3'] NONE NOT IN d.a.b.c.e.f, 1.5) RETURN d", checkNotNone},
      {"FOR d IN collection FILTER boost([ '1', 1+d.a, '3'] ANY NOT IN d.a.b.c.e.f, 1.5) RETURN d", checkNotAny},
      {"FOR d IN collection FILTER boost([ '1', 1+d.a, '3'] ALL NOT IN d.a.b.c.e.f, 1.5) RETURN d", checkNotAll},
      {"FOR d IN collection FILTER boost([ '1', 1+d.a, '3'] NONE NOT IN d.a.b.c.e.f, 1.5) RETURN d", checkNotNone}
    };

    for (auto testData : testCases) {
      auto const& queryString = testData.first;
      SCOPED_TRACE(testing::Message("Query:") << queryString);
      std::string const refName = "d";

      TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));

      auto options = std::make_shared<arangodb::velocypack::Builder>();

      arangodb::aql::Query query(arangodb::transaction::StandaloneContext::Create(vocbase), arangodb::aql::QueryString(queryString),
        nullptr, options);

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
        arangodb::iresearch::QueryContext const ctx{ nullptr, nullptr, nullptr, nullptr, nullptr, ref };
        EXPECT_TRUE(
          (arangodb::iresearch::FilterFactory::filter(nullptr, ctx, *filterNode).ok()));
      }

      // iteratorForCondition
      {
        arangodb::transaction::Methods trx(arangodb::transaction::StandaloneContext::Create(vocbase),
          {}, {}, {}, arangodb::transaction::Options());

        auto dummyPlan = arangodb::tests::planFromQuery(vocbase, "RETURN 1");
        
        ExpressionContextMock exprCtx;
        exprCtx.setTrx(&trx);

        irs::Or actual;
        arangodb::iresearch::QueryContext const ctx{ &trx, dummyPlan.get(), ast,
                                                    &exprCtx,
                                                    &irs::sub_reader::empty(), ref };
        EXPECT_TRUE(
          (arangodb::iresearch::FilterFactory::filter(&actual, ctx, *filterNode).ok()));

        {
          auto begin = testData.second(actual, 1.5);

          // 1st filter
          {
            irs::by_term expected;
            *expected.mutable_field() = mangleStringIdentity("a.b.c.e.f");
            expected.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("1"));
            EXPECT_EQ(expected, *begin);
          }

          // 2nd filter
          {
            ++begin;
            EXPECT_EQ(irs::type<arangodb::iresearch::ByExpression>::id(), begin->type());
            EXPECT_NE(nullptr,
              dynamic_cast<arangodb::iresearch::ByExpression const*>(&*begin));
          }

          // 3rd filter
          {
            ++begin;
            irs::by_term expected;
            *expected.mutable_field() = mangleStringIdentity("a.b.c.e.f");
            expected.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("3"));
            EXPECT_EQ(expected, *begin);
          }
        }
      }
    }
  }
  // heterogeneous references and expression in array ANY
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
    auto* term = irs::get<irs::term_attribute>(stream);

    irs::Or expected;
    auto& root = expected.add<irs::Not>().filter<irs::And>();
    root.boost(2.5);
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleStringIdentity("a.b.c.e.f");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("1"));
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleStringIdentity("a.b.c.e.f");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("str"));
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleBool("a.b.c.e.f");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::boolean_token_stream::value_false());
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleNumeric("a.b.c.e.f");
      filter.mutable_options()->term = term->value;
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleNull("a.b.c.e.f");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::null_token_stream::value_null());
    }

    // not a constant in array
    assertFilterSuccess(
        vocbase(),
        "LET strVal='str' LET boolVal=false LET numVal=2 LET nullVal=null FOR "
        "d IN collection FILTER BOOST(['1', strVal, "
        "boolVal, numVal+1, nullVal] ANY NOT IN d.a.b.c.e.f, 2.5) RETURN d",
        expected,
        &ctx);

    assertFilterSuccess(
        vocbase(),
        "LET strVal='str' LET boolVal=false LET numVal=2 LET nullVal=null FOR "
        "d IN collection FILTER BOOST(['1', strVal, "
        "boolVal, numVal+1, nullVal] ANY != d.a.b.c.e.f, 2.5) RETURN d",
        expected,
        &ctx);
  }
  // heterogeneous references and expression in array ALL
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
    auto* term = irs::get<irs::term_attribute>(stream);

    irs::Or expected;
    auto& root = expected.add<irs::Not>().filter<irs::Or>();
    root.boost(2.5);
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleStringIdentity("a.b.c.e.f");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("1"));
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleStringIdentity("a.b.c.e.f");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("str"));
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleBool("a.b.c.e.f");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::boolean_token_stream::value_false());
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleNumeric("a.b.c.e.f");
      filter.mutable_options()->term = term->value;
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleNull("a.b.c.e.f");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::null_token_stream::value_null());
    }

    // not a constant in array
    assertFilterSuccess(
        vocbase(),
        "LET strVal='str' LET boolVal=false LET numVal=2 LET nullVal=null FOR "
        "d IN collection FILTER BOOST(['1', strVal, "
        "boolVal, numVal+1, nullVal] ALL NOT IN d.a.b.c.e.f, 2.5) RETURN d",
        expected,
        &ctx);

    assertFilterSuccess(
        vocbase(),
        "LET strVal='str' LET boolVal=false LET numVal=2 LET nullVal=null FOR "
        "d IN collection FILTER BOOST(['1', strVal, "
        "boolVal, numVal+1, nullVal] ALL != d.a.b.c.e.f, 2.5) RETURN d",
        expected,
        &ctx);
  }
  // heterogeneous references and expression in array NONE
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
    auto* term = irs::get<irs::term_attribute>(stream);

    irs::Or expected;
    auto& root = expected.add<irs::And>();
    root.boost(2.5);
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleStringIdentity("a.b.c.e.f");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("1"));
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleStringIdentity("a.b.c.e.f");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("str"));
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleBool("a.b.c.e.f");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::boolean_token_stream::value_false());
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleNumeric("a.b.c.e.f");
      filter.mutable_options()->term = term->value;
    }
    {
      auto& filter = root.add<irs::by_term>();
      *filter.mutable_field() = mangleNull("a.b.c.e.f");
      filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::null_token_stream::value_null());
    }

    // not a constant in array
    assertFilterSuccess(
        vocbase(),
        "LET strVal='str' LET boolVal=false LET numVal=2 LET nullVal=null FOR "
        "d IN collection FILTER BOOST(['1', strVal, "
        "boolVal, numVal+1, nullVal] NONE NOT IN d.a.b.c.e.f, 2.5) RETURN d",
        expected,
        &ctx);

    assertFilterSuccess(
        vocbase(),
        "LET strVal='str' LET boolVal=false LET numVal=2 LET nullVal=null FOR "
        "d IN collection FILTER BOOST(['1', strVal, "
        "boolVal, numVal+1, nullVal] NONE != d.a.b.c.e.f, 2.5) RETURN d",
        expected,
        &ctx);
  }

  // no reference provided
  assertFilterExecutionFail(
      vocbase(), "LET x={} FOR d IN myView FILTER [1,x.a,3] ANY NOT IN d.a RETURN d",
      &ExpressionContextMock::EMPTY);

  assertFilterExecutionFail(
      vocbase(), "LET x={} FOR d IN myView FILTER [1,x.a,3] ANY != d.a RETURN d",
      &ExpressionContextMock::EMPTY);

  assertFilterExecutionFail(
      vocbase(), "LET x={} FOR d IN myView FILTER [1,x.a,3] ALL NOT IN d.a RETURN d",
      &ExpressionContextMock::EMPTY);

  assertFilterExecutionFail(
      vocbase(), "LET x={} FOR d IN myView FILTER [1,x.a,3] ALL != d.a RETURN d",
      &ExpressionContextMock::EMPTY);

  assertFilterExecutionFail(
      vocbase(), "LET x={} FOR d IN myView FILTER [1,x.a,3] NONE NOT IN d.a RETURN d",
      &ExpressionContextMock::EMPTY);

  assertFilterExecutionFail(
      vocbase(), "LET x={} FOR d IN myView FILTER [1,x.a,3] NONE != d.a RETURN d",
      &ExpressionContextMock::EMPTY);

  // empty array ANY
  {
    irs::Or expected;
    expected.add<irs::empty>();
    expected.boost(2.5);
    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER BOOST([] ANY NOT IN d.a, 2.5) RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER BOOST([] ANY NOT IN d['a'], 2.5) RETURN d", expected);

    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER BOOST([] ANY != d.a, 2.5) RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER BOOST([] ANY != d['a'], 2.5) RETURN d", expected);
  }

  // empty array ALL/NONE
  {
    irs::Or expected;
    expected.add<irs::all>();
    expected.boost(2.5);

    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER BOOST([] ALL NOT IN d.a, 2.5) RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER BOOST([] ALL NOT IN d['a'], 2.5) RETURN d", expected);

    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER BOOST([] ALL != d.a, 2.5) RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER BOOST([] ALL != d['a'], 2.5) RETURN d", expected);

    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER BOOST([] NONE NOT IN d.a, 2.5) RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER BOOST([] NONE NOT IN d['a'], 2.5) RETURN d", expected);

    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER BOOST([] NONE != d.a, 2.5) RETURN d", expected);
    assertFilterSuccess(
        vocbase(), "FOR d IN collection FILTER BOOST([] NONE != d['a'], 2.5) RETURN d", expected);
  }
}
