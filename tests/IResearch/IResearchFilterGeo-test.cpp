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
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include <s2/s2latlng.h>

#include "analysis/analyzers.hpp"
#include "analysis/token_attributes.hpp"
#include "analysis/token_streams.hpp"
#include "search/boolean_filter.hpp"

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
#include "Aql/Function.h"
#include "Aql/Query.h"
#include "Aql/OptimizerRulesFeature.h"
#include "Cluster/ClusterFeature.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "IResearch/AqlHelper.h"
#include "IResearch/ExpressionFilter.h"
#include "IResearch/GeoFilter.h"
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

class IResearchFilterGeoFunctionsTest
    : public ::testing::Test,
      public arangodb::tests::LogSuppressor<arangodb::Logger::AUTHENTICATION, arangodb::LogLevel::ERR> {
 protected:
  arangodb::tests::mocks::MockAqlServer server;

 private:
  TRI_vocbase_t* _vocbase;

 protected:
  IResearchFilterGeoFunctionsTest() {
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
}; // IResearchFilterGeoFunctionsTest

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

TEST_F(IResearchFilterGeoFunctionsTest, GeoIntersects) {
  {
    auto json = VPackParser::fromJson(R"([ 1, 2 ])");

    irs::Or expected;
    auto& filter = expected.add<arangodb::iresearch::GeoFilter>();
    *filter.mutable_field() = mangleStringIdentity("name");
    auto* opts = filter.mutable_options();
    opts->type = arangodb::iresearch::GeoFilterType::INTERSECTS;
    opts->prefix = "";
    ASSERT_TRUE(opts->shape.parseCoordinates(json->slice(), true).ok());

    assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER GEO_INTERSECTS(d.name, { "type": "Point", "coordinates": [ 1, 2 ] }) RETURN d)",
      expected);
    assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER GEO_INTERSECTS({ "type": "Point", "coordinates": [ 1, 2 ] }, d.name) RETURN d)",
      expected);
    assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER GEO_INTERSECTS(d['name'],  [ 1, 2 ] ) RETURN d)",
      expected);
    assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER GEO_INTERSECTS([ 1, 2 ], d['name']) RETURN d)",
      expected);
    assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER GEO_INTERSECTS(d.name, GEO_POINT(1, 2)) RETURN d)",
      expected, &ExpressionContextMock::EMPTY);
  }

  {
    auto json = VPackParser::fromJson(R"([ 1, 2 ])");

    irs::Or expected;
    auto& filter = expected.add<arangodb::iresearch::GeoFilter>();
    *filter.mutable_field() = mangleStringIdentity("name");
    filter.boost(1.5);
    auto* opts = filter.mutable_options();
    opts->type = arangodb::iresearch::GeoFilterType::INTERSECTS;
    opts->prefix = "";
    ASSERT_TRUE(opts->shape.parseCoordinates(json->slice(), true).ok());

    ExpressionContextMock ctx;
    ctx.vars.emplace("lat", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{ 2 }));
    ctx.vars.emplace("lng", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{ 1 }));

    assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER BOOST(GEO_INTERSECTS(d[_FORWARD_('name')], { "type": "Point", "coordinates": [ 1, 2 ] }), 1.5) RETURN d)",
      expected, &ctx);
    assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER BOOST(GEO_INTERSECTS({ "type": "Point", "coordinates": [ 1, 2 ] }, d.name), 1.5) RETURN d)",
      expected);
    assertFilterSuccess(
      vocbase(),
      R"(LET lng=1 LET lat=2 FOR d IN myView FILTER BOOST(GEO_INTERSECTS(d['name'], [lng, lat] ), 1.5) RETURN d)",
      expected, &ctx);
    assertFilterSuccess(
      vocbase(),
      R"(LET lng=1 LET lat=2 FOR d IN myView FILTER booSt(GEO_INTERSECTS([ lng, lat ], d['name']), 1.5) RETURN d)",
      expected, &ctx);
  }

  // wrong number of arguments
  assertFilterParseFail(
    vocbase(),
    R"(FOR d IN myView FILTER GEO_INTERSECTS(d.name) RETURN d)");
  assertFilterParseFail(
      vocbase(),
      R"(FOR d IN myView FILTER GEO_INTERSECTS(d['name'], [ 1, 2 ], null) RETURN d)");

  // non-deterministic arg
  assertFilterParseFail(
      vocbase(),
      R"(FOR d IN myView FILTER GEO_INTERSECTS(d['name'], RAND() > 0.5 ? [ 1, 2 ] : [2 : 1]) RETURN d)");

  // wrong first arg type
  assertFilterFail(
    vocbase(),
    R"(FOR d IN myView FILTER GEO_INTERSECTS(d[*],  [ 1, 2 ] ) RETURN d)");
  assertFilterFail(
    vocbase(),
    R"(FOR d IN myView FILTER GEO_INTERSECTS([1, 2],  [ 1, 2 ] ) RETURN d)");
  assertFilterFail(
    vocbase(),
    R"(FOR d IN myView FILTER GEO_INTERSECTS(1,  [ 1, 2 ] ) RETURN d)");
  assertFilterFail(
    vocbase(),
    R"(FOR d IN myView FILTER GEO_INTERSECTS('[1,2]',  [ 1, 2 ] ) RETURN d)");
  assertFilterFail(
    vocbase(),
    R"(FOR d IN myView FILTER GEO_INTERSECTS(null,  [ 1, 2 ] ) RETURN d)");
  assertFilterFail(
    vocbase(),
    R"(FOR d IN myView FILTER GEO_INTERSECTS(['1', '2'],  [ 1, 2 ] ) RETURN d)");
  assertFilterFail(
    vocbase(),
    R"(FOR d IN myView FILTER GEO_INTERSECTS({ "type": "Point", "coordinates": [ 1, 2 ] },  [ 1, 2 ] ) RETURN d)");

  // wrong second arg
  assertFilterFail(
    vocbase(),
    R"(FOR d IN myView FILTER GEO_INTERSECTS(d['name'], [ '1', '2' ] ) RETURN d)");
  assertFilterFail(
    vocbase(),
    R"(FOR d IN myView FILTER GEO_INTERSECTS(d['name'], 1 ) RETURN d)");
  assertFilterFail(
    vocbase(),
    R"(FOR d IN myView FILTER GEO_INTERSECTS(d['name'], '[1,2]') RETURN d)");
  assertFilterFail(
    vocbase(),
    R"(FOR d IN myView FILTER GEO_INTERSECTS(d['name'], true) RETURN d)");
  assertFilterFail(
    vocbase(),
    R"(FOR d IN myView FILTER GEO_INTERSECTS(d['name'], null) RETURN d)");
  assertFilterFail(
    vocbase(),
    R"(FOR d IN myView FILTER GEO_INTERSECTS(d['name'], {foo:[1,2]}) RETURN d)");
  assertFilterFail(
    vocbase(),
    R"(FOR d IN myView FILTER GEO_INTERSECTS(d['name'], { "type": "Pointt", "coordinates": [ 1, 2 ] }) RETURN d)");
}

TEST_F(IResearchFilterGeoFunctionsTest, GeoContains) {
  {
    auto json = VPackParser::fromJson(R"([ 1, 2 ])");

    irs::Or expected;
    auto& filter = expected.add<arangodb::iresearch::GeoFilter>();
    *filter.mutable_field() = mangleStringIdentity("name");
    auto* opts = filter.mutable_options();
    opts->type = arangodb::iresearch::GeoFilterType::IS_CONTAINED;
    opts->prefix = "";
    ASSERT_TRUE(opts->shape.parseCoordinates(json->slice(), true).ok());

    ExpressionContextMock ctx;
    ctx.vars.emplace("lat", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{ 2 }));
    ctx.vars.emplace("lng", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{ 1 }));

    assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER GEO_CONTAINS(d.name, { "type": "Point", "coordinates": [ 1, 2 ] }) RETURN d)",
      expected);
    assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER GEO_CONTAINS(d.name, GEO_POINT(1, 2)) RETURN d)",
      expected, &ctx);
    assertFilterSuccess(
      vocbase(),
      R"(LET lng = 1 LET lat = 2 FOR d IN myView FILTER GEO_CONTAINS(d.name, GEO_POINT(lng, lat)) RETURN d)",
      expected, &ctx);
    assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER GEO_CONTAINS(d['name'],  [ 1, 2 ] ) RETURN d)",
      expected);
    assertFilterSuccess(
      vocbase(),
      R"(LET lat = 2 LET lng = 1 FOR d IN myView FILTER GEO_CONTAINS(d['name'],  [ lng, lat ] ) RETURN d)",
      expected, &ctx);
  }

  {
    auto json = VPackParser::fromJson(R"([ 1, 2 ])");

    irs::Or expected;
    auto& filter = expected.add<arangodb::iresearch::GeoFilter>();
    *filter.mutable_field() = mangleStringIdentity("name");
    auto* opts = filter.mutable_options();
    opts->type = arangodb::iresearch::GeoFilterType::CONTAINS;
    opts->prefix = "";
    ASSERT_TRUE(opts->shape.parseCoordinates(json->slice(), true).ok());

    assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER GEO_CONTAINS({ "type": "Point", "coordinates": [ 1, 2 ] }, d.name) RETURN d)",
      expected);
    assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER GEO_CONTAINS([ 1, 2 ], d['name']) RETURN d)",
      expected);
  }

  {
    auto json = VPackParser::fromJson(R"([ 1, 2 ])");

    irs::Or expected;
    auto& filter = expected.add<arangodb::iresearch::GeoFilter>();
    *filter.mutable_field() = mangleStringIdentity("name");
    filter.boost(1.5);
    auto* opts = filter.mutable_options();
    opts->type = arangodb::iresearch::GeoFilterType::IS_CONTAINED;
    opts->prefix = "";
    ASSERT_TRUE(opts->shape.parseCoordinates(json->slice(), true).ok());

    assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER BOOST(GEO_CONTAINS(d.name, { "type": "Point", "coordinates": [ 1, 2 ] }), 1.5) RETURN d)",
      expected);
    assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER BOOST(GEO_CONTAINS(d['name'],  [ 1, 2 ] ), 1.5) RETURN d)",
      expected);
  }

  {
    auto json = VPackParser::fromJson(R"([ 1, 2 ])");

    irs::Or expected;
    auto& filter = expected.add<arangodb::iresearch::GeoFilter>();
    *filter.mutable_field() = mangleStringIdentity("name");
    filter.boost(1.5);
    auto* opts = filter.mutable_options();
    opts->type = arangodb::iresearch::GeoFilterType::CONTAINS;
    opts->prefix = "";
    ASSERT_TRUE(opts->shape.parseCoordinates(json->slice(), true).ok());
    assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER BOOST(GEO_CONTAINS({ "type": "Point", "coordinates": [ 1, 2 ] }, d.name), 1.5) RETURN d)",
      expected);
    assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER booSt(GEO_CONTAINS([ 1, 2 ], d['name']), 1.5) RETURN d)",
      expected);
  }

  // wrong number of arguments
  assertFilterParseFail(
    vocbase(),
    R"(FOR d IN myView FILTER GEO_CONTAINS(d.name) RETURN d)");
  assertFilterParseFail(
    vocbase(),
    R"(FOR d IN myView FILTER GEO_CONTAINS(d['name'], [ 1, 2 ], null) RETURN d)");

  // non-deterministic arg
  assertFilterParseFail(
      vocbase(),
      R"(FOR d IN myView FILTER GEO_CONTAINS(d['name'], RAND() > 0.5 ? [ 1, 2 ] : [2 : 1]) RETURN d)");

  // wrong first arg type
  assertFilterFail(
    vocbase(),
    R"(FOR d IN myView FILTER GEO_CONTAINS(d[*],  [ 1, 2 ] ) RETURN d)");
  assertFilterFail(
    vocbase(),
    R"(FOR d IN myView FILTER GEO_CONTAINS([1, 2],  [ 1, 2 ] ) RETURN d)");
  assertFilterFail(
    vocbase(),
    R"(FOR d IN myView FILTER GEO_CONTAINS(1,  [ 1, 2 ] ) RETURN d)");
  assertFilterFail(
    vocbase(),
    R"(FOR d IN myView FILTER GEO_CONTAINS('[1,2]',  [ 1, 2 ] ) RETURN d)");
  assertFilterFail(
    vocbase(),
    R"(FOR d IN myView FILTER GEO_CONTAINS(null,  [ 1, 2 ] ) RETURN d)");
  assertFilterFail(
    vocbase(),
    R"(FOR d IN myView FILTER GEO_CONTAINS(['1', '2'],  [ 1, 2 ] ) RETURN d)");
  assertFilterFail(
    vocbase(),
    R"(FOR d IN myView FILTER GEO_CONTAINS({ "type": "Point", "coordinates": [ 1, 2 ] },  [ 1, 2 ] ) RETURN d)");

  // wrong second arg
  assertFilterFail(
    vocbase(),
    R"(FOR d IN myView FILTER GEO_CONTAINS(d['name'], [ '1', '2' ] ) RETURN d)");
  assertFilterFail(
    vocbase(),
    R"(FOR d IN myView FILTER GEO_CONTAINS(d['name'], 1 ) RETURN d)");
  assertFilterFail(
    vocbase(),
    R"(FOR d IN myView FILTER GEO_CONTAINS(d['name'], '[1,2]') RETURN d)");
  assertFilterFail(
    vocbase(),
    R"(FOR d IN myView FILTER GEO_CONTAINS(d['name'], true) RETURN d)");
  assertFilterFail(
    vocbase(),
    R"(FOR d IN myView FILTER GEO_CONTAINS(d['name'], null) RETURN d)");
  assertFilterFail(
    vocbase(),
    R"(FOR d IN myView FILTER GEO_CONTAINS(d['name'], {foo:[1,2]}) RETURN d)");
  assertFilterFail(
    vocbase(),
    R"(FOR d IN myView FILTER GEO_CONTAINS(d['name'], { "type": "Pointt", "coordinates": [ 1, 2 ] }) RETURN d)");
}

TEST_F(IResearchFilterGeoFunctionsTest, GeoDistance) {
  {
    irs::Or expected;
    auto& filter = expected.add<arangodb::iresearch::GeoDistanceFilter>();
    *filter.mutable_field() = mangleStringIdentity("name");
    auto* opts = filter.mutable_options();
    opts->origin = S2LatLng::FromDegrees(0., 0.).Normalized().ToPoint();
    opts->range.max = 5000;
    opts->range.max_type = irs::BoundType::INCLUSIVE;
    opts->prefix = "";

    ExpressionContextMock ctx;
    ctx.vars.emplace("lat", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{ 0 }));
    ctx.vars.emplace("lng", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{ 0 }));
    ctx.vars.emplace("dist", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{ 5000 }));

    assertFilterSuccess(
      vocbase(),
      R"(LET lat = 0 LET dist = 5000 FOR d IN myView FILTER GEO_DISTANCE(d.name, { "type": "Point", "coordinates": [ lat, 0 ] }) <= dist RETURN d)",
      expected, &ctx);
    assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER GEO_DISTANCE({ "type": "Point", "coordinates": [ 0, 0 ] }, d.name) <= 5000 RETURN d)",
      expected);
    assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER GEO_DISTANCE(d[_FORWARD_('name')], [ 0, 0 ]) <= 5000 RETURN d)",
      expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess(
      vocbase(),
      R"(LET lat = 0 LET lng = 0 LET dist = 5000 FOR d IN myView FILTER GEO_DISTANCE([ lat, lng ], d[_FORWARD_('name')]) <= dist RETURN d)",
      expected, &ctx);
  }

  {
    irs::Or expected;
    auto& filter = expected.add<arangodb::iresearch::GeoDistanceFilter>();
    *filter.mutable_field() = mangleStringIdentity("name");
    auto* opts = filter.mutable_options();
    opts->origin = S2LatLng::FromDegrees(0., 0.).Normalized().ToPoint();
    opts->range.max = 5000;
    opts->range.max_type = irs::BoundType::EXCLUSIVE;
    opts->prefix = "";

    ExpressionContextMock ctx;
    ctx.vars.emplace("lat", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{ 0 }));
    ctx.vars.emplace("lng", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{ 0 }));
    ctx.vars.emplace("dist", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{ 5000 }));

    assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER GEO_DISTANCE(d.name, { "type": "Point", "coordinates": [ 0, 0 ] }) < 5000 RETURN d)",
      expected);
    assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER GEO_DISTANCE({ "type": "Point", "coordinates": [ 0, 0 ] }, d.name) < 5000 RETURN d)",
      expected);
    assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER GEO_DISTANCE(d[_FORWARD_('name')], [ 0, 0 ]) < 5000 RETURN d)",
      expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess(
      vocbase(),
      R"(LET lat = 0 LET lng = 0 LET dist = 5000 FOR d IN myView FILTER GEO_DISTANCE([ lng, lat ], d[_FORWARD_('name')]) < dist RETURN d)",
      expected, &ctx);
  }

  {
    irs::Or expected;
    auto& filter = expected.add<arangodb::iresearch::GeoDistanceFilter>();
    *filter.mutable_field() = mangleStringIdentity("name");
    auto* opts = filter.mutable_options();
    opts->origin = S2LatLng::FromDegrees(0., 0.).Normalized().ToPoint();
    opts->range.min = 5000;
    opts->range.min_type = irs::BoundType::EXCLUSIVE;
    opts->prefix = "";

    ExpressionContextMock ctx;
    ctx.vars.emplace("lat", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{ 0 }));
    ctx.vars.emplace("lng", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{ 0 }));
    ctx.vars.emplace("dist", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{ 5000 }));

    assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER GEO_DISTANCE(d.name, { "type": "Point", "coordinates": [ 0, 0 ] }) > 5000 RETURN d)",
      expected);
    assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER GEO_DISTANCE({ "type": "Point", "coordinates": [ 0, 0 ] }, d.name) > 5000 RETURN d)",
      expected);
    assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER GEO_DISTANCE(d[_FORWARD_('name')], [ 0, 0 ]) > 5000 RETURN d)",
      expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess(
      vocbase(),
      R"(LET lat = 0 LET lng = 0 LET dist = 5000 FOR d IN myView FILTER GEO_DISTANCE([ 0, 0 ], d[_FORWARD_('name')]) > dist RETURN d)",
      expected, &ctx);
  }

  {
    irs::Or expected;
    auto& filter = expected.add<arangodb::iresearch::GeoDistanceFilter>();
    filter.boost(1.5f);
    *filter.mutable_field() = mangleStringIdentity("name");
    auto* opts = filter.mutable_options();
    opts->origin = S2LatLng::FromDegrees(0., 0.).Normalized().ToPoint();
    opts->range.min = 5000;
    opts->range.min_type = irs::BoundType::EXCLUSIVE;
    opts->prefix = "";

    ExpressionContextMock ctx;
    ctx.vars.emplace("lat", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{ 0 }));
    ctx.vars.emplace("lng", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{ 0 }));
    ctx.vars.emplace("dist", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{ 5000 }));

    assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER BOOST(GEO_DISTANCE(d.name, { "type": "Point", "coordinates": [ 0, 0 ] }) > 5000, 1.5) RETURN d)",
      expected);
    assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER BOOST(GEO_DISTANCE({ "type": "Point", "coordinates": [ 0, 0 ] }, d.name) > 5000, 1.5) RETURN d)",
      expected);
    assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER BOOST(GEO_DISTANCE(d[_FORWARD_('name')], [ 0, 0 ]) > 5000, 1.5) RETURN d)",
      expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess(
      vocbase(),
      R"(LET lat = 0 LET lng = 0 LET dist = 5000 FOR d IN myView FILTER BOOST(GEO_DISTANCE([ 0, 0 ], d[_FORWARD_('name')]) > dist, 1.5) RETURN d)",
      expected, &ctx);
  }

  {
    irs::Or expected;
    auto& filter = expected.add<arangodb::iresearch::GeoDistanceFilter>();
    *filter.mutable_field() = mangleStringIdentity("name");
    auto* opts = filter.mutable_options();
    opts->origin = S2LatLng::FromDegrees(0., 0.).Normalized().ToPoint();
    opts->range.min = 5000;
    opts->range.min_type = irs::BoundType::INCLUSIVE;
    opts->prefix = "";

    ExpressionContextMock ctx;
    ctx.vars.emplace("lat", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{ 0 }));
    ctx.vars.emplace("lng", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{ 0 }));
    ctx.vars.emplace("dist", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{ 5000 }));

    assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER GEO_DISTANCE(d.name, { "type": "Point", "coordinates": [ 0, 0 ] }) >= 5000 RETURN d)",
      expected);
    assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER GEO_DISTANCE({ "type": "Point", "coordinates": [ 0, 0 ] }, d.name) >= 5000 RETURN d)",
      expected);
    assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER GEO_DISTANCE(d[_FORWARD_('name')], [ 0, 0 ]) >= 5000 RETURN d)",
      expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess(
      vocbase(),
      R"(LET lat = 0 LET lng = 0 LET dist = 5000 FOR d IN myView FILTER GEO_DISTANCE([ 0, 0 ], d[_FORWARD_('name')]) >= dist RETURN d)",
      expected, &ctx);
  }

  {
    irs::Or expected;
    auto& filter = expected.add<arangodb::iresearch::GeoDistanceFilter>();
    *filter.mutable_field() = mangleStringIdentity("name");
    auto* opts = filter.mutable_options();
    opts->origin = S2LatLng::FromDegrees(0., 0.).Normalized().ToPoint();
    opts->range.min = 5000;
    opts->range.min_type = irs::BoundType::INCLUSIVE;
    opts->range.max = 5000;
    opts->range.max_type = irs::BoundType::INCLUSIVE;
    opts->prefix = "";

    ExpressionContextMock ctx;
    ctx.vars.emplace("lat", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{ 0 }));
    ctx.vars.emplace("lng", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{ 0 }));
    ctx.vars.emplace("dist", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{ 5000 }));

    assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER GEO_DISTANCE(d.name, { "type": "Point", "coordinates": [ 0, 0 ] }) == 5000 RETURN d)",
      expected);
    assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER GEO_DISTANCE({ "type": "Point", "coordinates": [ 0, 0 ] }, d.name) == 5000 RETURN d)",
      expected);
    assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER GEO_DISTANCE(d[_FORWARD_('name')], [ 0, 0 ]) == 5000 RETURN d)",
      expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess(
      vocbase(),
      R"(LET lat = 0 LET lng = 0 LET dist = 5000 FOR d IN myView FILTER GEO_DISTANCE([ 0, 0 ], d[_FORWARD_('name')]) == dist RETURN d)",
      expected, &ctx);
  }

  {
    irs::Or expected;
    auto& filter = expected.add<irs::Not>().filter<arangodb::iresearch::GeoDistanceFilter>();
    *filter.mutable_field() = mangleStringIdentity("name");
    auto* opts = filter.mutable_options();
    opts->origin = S2LatLng::FromDegrees(0., 0.).Normalized().ToPoint();
    opts->range.min = 5000;
    opts->range.min_type = irs::BoundType::INCLUSIVE;
    opts->range.max = 5000;
    opts->range.max_type = irs::BoundType::INCLUSIVE;
    opts->prefix = "";

    ExpressionContextMock ctx;
    ctx.vars.emplace("lat", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{ 0 }));
    ctx.vars.emplace("lng", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{ 0 }));
    ctx.vars.emplace("dist", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{ 5000 }));

    assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER GEO_DISTANCE(d.name, { "type": "Point", "coordinates": [ 0, 0 ] }) != 5000 RETURN d)",
      expected);
    assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER GEO_DISTANCE({ "type": "Point", "coordinates": [ 0, 0 ] }, d.name) != 5000 RETURN d)",
      expected);
    assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER GEO_DISTANCE(d[_FORWARD_('name')], [ 0, 0 ]) != 5000 RETURN d)",
      expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess(
      vocbase(),
      R"(LET lat = 0 LET lng = 0 LET dist = 5000 FOR d IN myView FILTER GEO_DISTANCE([ 0, 0 ], d[_FORWARD_('name')]) != dist RETURN d)",
      expected, &ctx);
  }

  {
    irs::Or expected;
    auto& andGroup = expected.add<irs::And>();
    {
      auto& filter = andGroup.add<arangodb::iresearch::GeoDistanceFilter>();
      *filter.mutable_field() = mangleStringIdentity("name");
      auto* opts = filter.mutable_options();
      opts->origin = S2LatLng::FromDegrees(0., 0.).Normalized().ToPoint();
      opts->range.min = 5000;
      opts->range.min_type = irs::BoundType::EXCLUSIVE;
      opts->prefix = "";
    }
    {
      auto& filter = andGroup.add<arangodb::iresearch::GeoDistanceFilter>();
      *filter.mutable_field() = mangleStringIdentity("name");
      auto* opts = filter.mutable_options();
      opts->origin = S2LatLng::FromDegrees(0., 0.).Normalized().ToPoint();
      opts->range.max = 10000;
      opts->range.max_type = irs::BoundType::INCLUSIVE;
      opts->prefix = "";
    }

    ExpressionContextMock ctx;
    ctx.vars.emplace("lat", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{ 0 }));
    ctx.vars.emplace("lng", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{ 0 }));
    ctx.vars.emplace("dist", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{ 5000 }));

    assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView
         FILTER GEO_DISTANCE(d.name, { "type": "Point", "coordinates": [ 0, 0 ] }) > 5000
             && GEO_DISTANCE(d.name, { "type": "Point", "coordinates": [ 0, 0 ] }) <= 10000
         RETURN d)",
      expected);
    assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView
         FILTER GEO_DISTANCE({ "type": "Point", "coordinates": [ 0, 0 ] }, d.name) > 5000
             && GEO_DISTANCE({ "type": "Point", "coordinates": [ 0, 0 ] }, d.name) <= 10000
         RETURN d)",
      expected);
    assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView
         FILTER GEO_DISTANCE(d[_FORWARD_('name')], [ 0, 0 ]) > 5000
             && GEO_DISTANCE(d[_FORWARD_('name')], [ 0, 0 ]) <= 10000
         RETURN d)",
      expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess(
      vocbase(),
      R"(LET lat = 0 LET lng = 0 LET dist = 5000
         FOR d IN myView
         FILTER GEO_DISTANCE([ 0, 0 ], d[_FORWARD_('name')]) > dist
             && GEO_DISTANCE([ 0, 0 ], d[_FORWARD_('name')]) <= 2*dist
         RETURN d)",
      expected, &ctx);
  }

  // wrong number of arguments
  assertFilterParseFail(
    vocbase(),
    R"(FOR d IN myView FILTER GEO_DISTANCE(d.name) < 5000 RETURN d)");
  assertFilterParseFail(
    vocbase(),
    R"(FOR d IN myView FILTER GEO_DISTANCE(d['name'], [ 1, 2 ], null, null) < 5000 RETURN d)");
  assertExpressionFilter(
    vocbase(),
    R"(FOR d IN myView FILTER GEO_DISTANCE(d['name'], [ 1, 2 ], null) < 5000 RETURN d)");

  // non-deterministic arg
  assertFilterParseFail(
      vocbase(),
      R"(FOR d IN myView FILTER GEO_DISTANCE(d['name'], RAND() > 0.5 ? [ 1, 2 ] : [2 : 1]) < 5000 RETURN d)");

  assertExpressionFilter(
    vocbase(),
    R"(FOR d IN myView FILTER GEO_DISTANCE(d[*],  [ 1, 2 ] ) < 5000 RETURN d)");

  // wrong second arg
  assertExpressionFilter(
    vocbase(),
    R"(FOR d IN myView FILTER GEO_DISTANCE(d['name'], 1 ) < 5000 RETURN d)");
  assertExpressionFilter(
    vocbase(),
    R"(FOR d IN myView FILTER GEO_DISTANCE(d['name'], '[1,2]') < 5000 RETURN d)");
  assertExpressionFilter(
    vocbase(),
    R"(FOR d IN myView FILTER GEO_DISTANCE(d['name'], true) < 5000 RETURN d)");
  assertExpressionFilter(
    vocbase(),
    R"(FOR d IN myView FILTER GEO_DISTANCE(d['name'], null) < 5000 RETURN d)");
  assertExpressionFilter(
    vocbase(),
    R"(FOR d IN myView FILTER GEO_DISTANCE(d['name'], {foo:[1,2]}) < 5000 RETURN d)");
  assertExpressionFilter(
    vocbase(),
    R"(FOR d IN myView FILTER GEO_DISTANCE(d['name'], { "type": "Pointt", "coordinates": [ 1, 2 ] }) < 5000 RETURN d)");

  // wrong distance
  assertExpressionFilter(
    vocbase(),
    R"(FOR d IN myView FILTER GEO_DISTANCE(d['name'], [ 1, 2 ] ) < '5000' RETURN d)");
  assertExpressionFilter(
    vocbase(),
    R"(FOR d IN myView FILTER GEO_DISTANCE(d['name'], [ 1, 2 ] ) < false RETURN d)");
  assertExpressionFilter(
    vocbase(),
    R"(FOR d IN myView FILTER GEO_DISTANCE(d['name'], [ 1, 2 ] ) < null RETURN d)");
  assertExpressionFilter(
    vocbase(),
    R"(FOR d IN myView FILTER GEO_DISTANCE(d['name'], [ 1, 2 ] ) < [5000] RETURN d)");
  assertExpressionFilter(
    vocbase(),
    R"(FOR d IN myView FILTER GEO_DISTANCE(d['name'], [ 1, 2 ] ) < {} RETURN d)");
  assertExpressionFilter(
    vocbase(),
    R"(FOR d IN myView FILTER GEO_DISTANCE(d['name'], [ 1, 2 ] ) < d RETURN d)");
}

TEST_F(IResearchFilterGeoFunctionsTest, GeoInRange) {
  {
    irs::Or expected;
    auto& filter = expected.add<arangodb::iresearch::GeoDistanceFilter>();
    *filter.mutable_field() = mangleStringIdentity("name");
    auto* opts = filter.mutable_options();
    opts->origin = S2LatLng::FromDegrees(0., 0.).Normalized().ToPoint();
    ASSERT_EQ(irs::BoundType::UNBOUNDED, opts->range.min_type);
    ASSERT_EQ(0, opts->range.min);
    opts->range.max = 5000;
    opts->range.max_type = irs::BoundType::INCLUSIVE;
    opts->prefix = "";

    ExpressionContextMock ctx;
    ctx.vars.emplace("lat", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{ 0 }));
    ctx.vars.emplace("lng", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{ 0 }));
    ctx.vars.emplace("dist", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{ 5000 }));

    assertFilterSuccess(
      vocbase(),
      R"(LET lat = 0 LET dist = 5000 FOR d IN myView FILTER GEO_IN_RANGE(d.name, { "type": "Point", "coordinates": [ lat, 0 ] }, 0, 5000) RETURN d)",
      expected, &ctx);
    assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER GEO_IN_RANGE(d.name, { "type": "Point", "coordinates": [ 0, 0 ] }, 0, 5000) RETURN d)",
      expected);
    assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER GEO_IN_RANGE(d[_FORWARD_('name')], [ 0, 0 ], 0, 5000, true) RETURN d)",
      expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess(
      vocbase(),
      R"(LET lat = 0 LET lng = 0 LET dist = 5000 FOR d IN myView FILTER GEO_IN_RANGE(d[_FORWARD_('name')], [ lat, lng ], lat, dist) RETURN d)",
      expected, &ctx);
  }

  {
    irs::Or expected;
    auto& filter = expected.add<arangodb::iresearch::GeoDistanceFilter>();
    filter.boost(1.5f);
    *filter.mutable_field() = mangleStringIdentity("name");
    auto* opts = filter.mutable_options();
    opts->origin = S2LatLng::FromDegrees(0., 0.).Normalized().ToPoint();
    ASSERT_EQ(irs::BoundType::UNBOUNDED, opts->range.min_type);
    ASSERT_EQ(0, opts->range.min);
    opts->range.max = 5000;
    opts->range.max_type = irs::BoundType::INCLUSIVE;
    opts->prefix = "";

    ExpressionContextMock ctx;
    ctx.vars.emplace("lat", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{ 0 }));
    ctx.vars.emplace("lng", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{ 0 }));
    ctx.vars.emplace("dist", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{ 5000 }));

    assertFilterSuccess(
      vocbase(),
      R"(LET lat = 0 LET dist = 5000 FOR d IN myView FILTER BOOST(GEO_IN_RANGE(d.name, { "type": "Point", "coordinates": [ lat, 0 ] }, 0, 5000), 1.5) RETURN d)",
      expected, &ctx);
    assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER boosT(GEO_IN_RANGE(d.name, { "type": "Point", "coordinates": [ 0, 0 ] }, 0, 5000), 1.5) RETURN d)",
      expected);
    assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER boosT(GEO_IN_RANGE(d[_FORWARD_('name')], [ 0, 0 ], 0, 5000, true), 1.5) RETURN d)",
      expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess(
      vocbase(),
      R"(LET lat = 0 LET lng = 0 LET dist = 5000 FOR d IN myView FILTER boosT(GEO_IN_RANGE(d[_FORWARD_('name')], [ lat, lng ], lat, dist), 1.5) RETURN d)",
      expected, &ctx);
  }

  {
    irs::Or expected;
    auto& filter = expected.add<arangodb::iresearch::GeoDistanceFilter>();
    *filter.mutable_field() = mangleStringIdentity("name");
    auto* opts = filter.mutable_options();
    opts->origin = S2LatLng::FromDegrees(0., 0.).Normalized().ToPoint();
    ASSERT_EQ(irs::BoundType::UNBOUNDED, opts->range.min_type);
    ASSERT_EQ(0, opts->range.min);
    opts->range.max = 5000;
    opts->range.max_type = irs::BoundType::EXCLUSIVE;
    opts->prefix = "";

    ExpressionContextMock ctx;
    ctx.vars.emplace("lat", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{ 0 }));
    ctx.vars.emplace("lng", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{ 0 }));
    ctx.vars.emplace("dist", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{ 5000 }));

    assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER GEO_iN_RANGE(d.name, { "type": "Point", "coordinates": [ 0, 0 ] }, 0, 5000, false, false) RETURN d)",
      expected);
    assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER GEO_iN_RANGE(d.name, { "type": "Point", "coordinates": [ 0, 0 ] }, 0, 5000, false, false) RETURN d)",
      expected);
    assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER GEO_iN_RANGE(d[_FORWARD_('name')], [ 0, 0 ], 0, 5000, false, false) RETURN d)",
      expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess(
      vocbase(),
      R"(LET lat = 0 LET lng = 0 LET dist = 5000 FOR d IN myView FILTER GEO_iN_RANGE(d[_FORWARD_('name')], [ lng, lat ], lat, dist, lat != lng, lat != lng) RETURN d)",
      expected, &ctx);
  }

  {
    irs::Or expected;
    auto& filter = expected.add<arangodb::iresearch::GeoDistanceFilter>();
    *filter.mutable_field() = mangleStringIdentity("name");
    auto* opts = filter.mutable_options();
    opts->origin = S2LatLng::FromDegrees(0., 0.).Normalized().ToPoint();
    opts->range.min = 1000;
    opts->range.min_type = irs::BoundType::INCLUSIVE;
    opts->range.max = 5000;
    opts->range.max_type = irs::BoundType::EXCLUSIVE;
    opts->prefix = "";

    ExpressionContextMock ctx;
    ctx.vars.emplace("lat", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{ 0 }));
    ctx.vars.emplace("lng", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{ 0 }));
    ctx.vars.emplace("dist", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{ 5000 }));

    assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER GEO_iN_RANGE(d.name, { "type": "Point", "coordinates": [ 0, 0 ] }, 1000, 5000, true, false) RETURN d)",
      expected);
    assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER GEO_iN_RANGE(d.name, { "type": "Point", "coordinates": [ 0, 0 ] }, 1000, 5000, true, false) RETURN d)",
      expected);
    assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER GEO_iN_RANGE(d[_FORWARD_('name')], [ 0, 0 ], 1000, 5000, true, false) RETURN d)",
      expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess(
      vocbase(),
      R"(LET lat = 0 LET lng = 0 LET dist = 5000 FOR d IN myView FILTER GEO_iN_RANGE(d[_FORWARD_('name')], [ lng, lat ], 1000 + lat, dist, lat == lng, lat != lng) RETURN d)",
      expected, &ctx);
  }

  {
    irs::Or expected;
    auto& filter = expected.add<arangodb::iresearch::GeoDistanceFilter>();
    *filter.mutable_field() = mangleStringIdentity("name");
    auto* opts = filter.mutable_options();
    opts->origin = S2LatLng::FromDegrees(0., 0.).Normalized().ToPoint();
    opts->range.min = 5000;
    opts->range.min_type = irs::BoundType::INCLUSIVE;
    opts->range.max = 5000;
    opts->range.max_type = irs::BoundType::INCLUSIVE;
    opts->prefix = "";

    ExpressionContextMock ctx;
    ctx.vars.emplace("lat", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{ 0 }));
    ctx.vars.emplace("lng", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{ 0 }));
    ctx.vars.emplace("dist", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{ 5000 }));

    assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER GEO_iN_RANGE(d.name, { "type": "Point", "coordinates": [ 0, 0 ] }, 5000, 5000, true, true) RETURN d)",
      expected);
    assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER GEO_iN_RANGE(d.name, { "type": "Point", "coordinates": [ 0, 0 ] }, 5000, 5000, true, true) RETURN d)",
      expected);
    assertFilterSuccess(
      vocbase(),
      R"(FOR d IN myView FILTER GEO_iN_RANGE(d[_FORWARD_('name')], [ 0, 0 ], 5000, 5000, true, true) RETURN d)",
      expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess(
      vocbase(),
      R"(LET lat = 0 LET lng = 0 LET dist = 5000 FOR d IN myView FILTER GEO_iN_RANGE(d[_FORWARD_('name')], [ lng, lat ], dist, dist, lat == lng, lat == lng) RETURN d)",
      expected, &ctx);
  }

  // wrong number of arguments
  assertFilterParseFail(
    vocbase(),
    R"(FOR d IN myView FILTER GEO_IN_RANGE(d.name) RETURN d)");
  assertFilterParseFail(
    vocbase(),
    R"(FOR d IN myView FILTER GEO_IN_RANGE(d.name, [1,2]) RETURN d)");
  assertFilterParseFail(
    vocbase(),
    R"(FOR d IN myView FILTER GEO_IN_RANGE(d.name, [1,2], 0) RETURN d)");
  assertFilterParseFail(
    vocbase(),
    R"(FOR d IN myView FILTER GEO_IN_RANGE(d.name, [1,2], 0, 5000, true, false, "sphere", null) RETURN d)");

  // ellipsoid is set
  assertFilterFail(
    vocbase(),
    R"(FOR d IN myView FILTER GEO_IN_RANGE(d.name, [1,2], 0, 5000, true, false, "sphere") RETURN d)");

  // non-deterministic arg
  assertFilterParseFail(
      vocbase(),
      R"(FOR d IN myView FILTER GEO_IN_RANGE(d['name'], RAND() > 0.5 ? [ 1, 2 ] : [2 : 1], 0, 5000) RETURN d)");

  // wrong first arg
  assertFilterFail(
    vocbase(),
    R"(FOR d IN myView FILTER GEO_IN_RANGE(d[*],  [ 1, 2 ], 0, 5000, true, true) RETURN d)",
    &ExpressionContextMock::EMPTY);
  assertFilterFail(
    vocbase(),
    R"(FOR d IN myView FILTER GEO_IN_RANGE([1,2],  [ 1, 2 ], 0, 5000, true, true) RETURN d)");
  assertFilterFail(
    vocbase(),
    R"(FOR d IN myView FILTER GEO_IN_RANGE(1,  [ 1, 2 ], 0, 5000, true, true) RETURN d)");
  assertFilterFail(
    vocbase(),
    R"(FOR d IN myView FILTER GEO_IN_RANGE('[1,2]',  [ 1, 2 ], 0, 5000, true, true) RETURN d)");
  assertFilterFail(
    vocbase(),
    R"(FOR d IN myView FILTER GEO_IN_RANGE(null,  [ 1, 2 ], 0, 5000, true, true) RETURN d)");
  assertFilterFail(
    vocbase(),
    R"(FOR d IN myView FILTER GEO_IN_RANGE(['1', '2'],  [ 1, 2 ], 0, 5000, true, true) RETURN d)");
  assertFilterFail(
    vocbase(),
    R"(FOR d IN myView FILTER GEO_IN_RANGE({ "type": "Point", "coordinates": [ 1, 2 ] },  [ 1, 2 ], 0, 5000, true, true) RETURN d)");

  // wrong second arg
  assertFilterExecutionFail(
    vocbase(),
    R"(FOR d IN myView FILTER GEO_IN_RANGE(d.name, d[*], 0, 5000, true, true) RETURN d)",
    &ExpressionContextMock::EMPTY);
  assertFilterFail(
    vocbase(),
    R"(FOR d IN myView FILTER GEO_IN_RANGE(d.name, '[1,2]', 0, 5000, true, true) RETURN d)");
  assertFilterFail(
    vocbase(),
    R"(FOR d IN myView FILTER GEO_IN_RANGE(d.name, ['1','2'], 0, 5000, true, true) RETURN d)");
  assertFilterFail(
    vocbase(),
    R"(FOR d IN myView FILTER GEO_IN_RANGE(d.name, 1, 0, 5000, true, true) RETURN d)");
  assertFilterFail(
    vocbase(),
    R"(FOR d IN myView FILTER GEO_IN_RANGE(d.name, null, 0, 5000, true, true) RETURN d)");
  assertFilterFail(
    vocbase(),
    R"(FOR d IN myView FILTER GEO_IN_RANGE(d.name, true, 0, 5000, true, true) RETURN d)");
  assertFilterFail(
    vocbase(),
    R"(FOR d IN myView FILTER GEO_IN_RANGE(d.name, { "type": "Pointt", "coordinates": [ 1, 2 ] }, 0, 5000, true, true) RETURN d)");

  // wrong third arg
  assertFilterFail(
    vocbase(),
    R"(FOR d IN myView FILTER GEO_IN_RANGE(d.name, [1,2], '0', 5000, true, true) RETURN d)");
  assertFilterFail(
    vocbase(),
    R"(FOR d IN myView FILTER GEO_IN_RANGE(d.name, [1,2], true, 5000, true, true) RETURN d)");
  assertFilterFail(
    vocbase(),
    R"(FOR d IN myView FILTER GEO_IN_RANGE(d.name, [1,2], null, 5000, true, true) RETURN d)");
  assertFilterFail(
    vocbase(),
    R"(FOR d IN myView FILTER GEO_IN_RANGE(d.name, [1,2], [0], 5000, true, true) RETURN d)");
  assertFilterFail(
    vocbase(),
    R"(FOR d IN myView FILTER GEO_IN_RANGE(d.name, [1,2], {}, 5000, true, true) RETURN d)");
  assertFilterExecutionFail(
    vocbase(),
    R"(FOR d IN myView FILTER GEO_IN_RANGE(d.name, [1,2], d[*], 5000, true, true) RETURN d)",
    &ExpressionContextMock::EMPTY);

  // wrong 4th arg
  assertFilterFail(
    vocbase(),
    R"(FOR d IN myView FILTER GEO_IN_RANGE(d.name, [1,2], 0, '5000', true, true) RETURN d)");
  assertFilterFail(
    vocbase(),
    R"(FOR d IN myView FILTER GEO_IN_RANGE(d.name, [1,2], 0, true, true, true) RETURN d)");
  assertFilterFail(
    vocbase(),
    R"(FOR d IN myView FILTER GEO_IN_RANGE(d.name, [1,2], 0, null, true, true) RETURN d)");
  assertFilterFail(
    vocbase(),
    R"(FOR d IN myView FILTER GEO_IN_RANGE(d.name, [1,2], 0, [5000], true, true) RETURN d)");
  assertFilterFail(
    vocbase(),
    R"(FOR d IN myView FILTER GEO_IN_RANGE(d.name, [1,2], 0, {}, true, true) RETURN d)");
  assertFilterExecutionFail(
    vocbase(),
    R"(FOR d IN myView FILTER GEO_IN_RANGE(d.name, [1,2], 0, d[*], true, true) RETURN d)",
    &ExpressionContextMock::EMPTY);

  // wrong 5th arg
  assertFilterFail(
    vocbase(),
    R"(FOR d IN myView FILTER GEO_IN_RANGE(d.name, [1,2], 0, 5000, 'true', true) RETURN d)");
  assertFilterFail(
    vocbase(),
    R"(FOR d IN myView FILTER GEO_IN_RANGE(d.name, [1,2], 0, 5000, null, true) RETURN d)");
  assertFilterFail(
    vocbase(),
    R"(FOR d IN myView FILTER GEO_IN_RANGE(d.name, [1,2], 0, 5000, 0, true) RETURN d)");
  assertFilterFail(
    vocbase(),
    R"(FOR d IN myView FILTER GEO_IN_RANGE(d.name, [1,2], 0, 5000, [true], true) RETURN d)");
  assertFilterFail(
    vocbase(),
    R"(FOR d IN myView FILTER GEO_IN_RANGE(d.name, [1,2], 0, 5000, {}, true) RETURN d)");
  assertFilterExecutionFail(
    vocbase(),
    R"(FOR d IN myView FILTER GEO_IN_RANGE(d.name, [1,2], 0, 5000, d[*], true) RETURN d)",
    &ExpressionContextMock::EMPTY);

  // wrong 6th arg
  assertFilterFail(
    vocbase(),
    R"(FOR d IN myView FILTER GEO_IN_RANGE(d.name, [1,2], 0, 5000, true, 'true') RETURN d)");
  assertFilterFail(
    vocbase(),
    R"(FOR d IN myView FILTER GEO_IN_RANGE(d.name, [1,2], 0, 5000, true, null) RETURN d)");
  assertFilterFail(
    vocbase(),
    R"(FOR d IN myView FILTER GEO_IN_RANGE(d.name, [1,2], 0, 5000, true, 0) RETURN d)");
  assertFilterFail(
    vocbase(),
    R"(FOR d IN myView FILTER GEO_IN_RANGE(d.name, [1,2], 0, 5000, true, [true]) RETURN d)");
  assertFilterFail(
    vocbase(),
    R"(FOR d IN myView FILTER GEO_IN_RANGE(d.name, [1,2], 0, 5000, true, {}) RETURN d)");
  assertFilterExecutionFail(
    vocbase(),
    R"(FOR d IN myView FILTER GEO_IN_RANGE(d.name, [1,2], 0, 5000, true, d[*]) RETURN d)",
    &ExpressionContextMock::EMPTY);
}
