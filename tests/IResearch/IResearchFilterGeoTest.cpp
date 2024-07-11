////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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
#ifdef USE_V8
#include "V8Server/V8DealerFeature.h"
#endif
#include "VocBase/Methods/Collections.h"
#include "Geo/GeoJson.h"

static const VPackBuilder systemDatabaseBuilder = dbArgsBuilder();
static const VPackSlice systemDatabaseArgs = systemDatabaseBuilder.slice();
// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

class IResearchFilterGeoFunctionsTest
    : public ::testing::Test,
      public arangodb::tests::LogSuppressor<arangodb::Logger::AUTHENTICATION,
                                            arangodb::LogLevel::ERR> {
 protected:
  arangodb::tests::mocks::MockAqlServer server;

 private:
  TRI_vocbase_t* _vocbase;

 protected:
  IResearchFilterGeoFunctionsTest() {
    arangodb::tests::init();

    auto& functions = server.getFeature<arangodb::aql::AqlFunctionFeature>();

    // register fake non-deterministic function in order to suppress
    // optimizations
    functions.add(arangodb::aql::Function{
        "_NONDETERM_", ".",
        arangodb::aql::Function::makeFlags(
            // fake non-deterministic
            arangodb::aql::Function::Flags::CanRunOnDBServerCluster,
            arangodb::aql::Function::Flags::CanRunOnDBServerOneShard),
        [](arangodb::aql::ExpressionContext*, arangodb::aql::AstNode const&,
           arangodb::aql::functions::VPackFunctionParametersView params) {
          TRI_ASSERT(!params.empty());
          return params[0];
        }});

    // register fake non-deterministic function in order to suppress
    // optimizations
    functions.add(arangodb::aql::Function{
        "_FORWARD_", ".",
        arangodb::aql::Function::makeFlags(
            // fake deterministic
            arangodb::aql::Function::Flags::Deterministic,
            arangodb::aql::Function::Flags::Cacheable,
            arangodb::aql::Function::Flags::CanRunOnDBServerCluster,
            arangodb::aql::Function::Flags::CanRunOnDBServerOneShard),
        [](arangodb::aql::ExpressionContext*, arangodb::aql::AstNode const&,
           arangodb::aql::functions::VPackFunctionParametersView params) {
          TRI_ASSERT(!params.empty());
          return params[0];
        }});

    auto& analyzers =
        server.getFeature<arangodb::iresearch::IResearchAnalyzerFeature>();
    arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;

    auto& dbFeature = server.getFeature<arangodb::DatabaseFeature>();
    dbFeature.createDatabase(
        testDBInfo(server.server()),
        _vocbase);  // required for IResearchAnalyzerFeature::emplace(...)
    std::shared_ptr<arangodb::LogicalCollection> unused;
    arangodb::OperationOptions options(arangodb::ExecContext::current());
    arangodb::methods::Collections::createSystem(
        *_vocbase, options, arangodb::tests::AnalyzerCollectionName, false,
        unused);
    analyzers.emplace(
        result, "testVocbase::test_analyzer", "TestAnalyzer",
        arangodb::velocypack::Parser::fromJson("{ \"args\": \"abc\"}")->slice(),
        arangodb::transaction::OperationOriginTestCase{});  // cache analyzer
    {
      auto json = VPackParser::fromJson(R"({})");
      EXPECT_TRUE(analyzers
                      .emplace(result, "testVocbase::mygeojson", "geojson",
                               json->slice(),
                               arangodb::transaction::OperationOriginTestCase{})
                      .ok());
    }
  }

  TRI_vocbase_t& vocbase() { return *_vocbase; }
};

using arangodb::geo::json::parseCoordinates;

TEST_F(IResearchFilterGeoFunctionsTest, GeoIntersects) {
  {
    auto json = VPackParser::fromJson(R"([ 1, 2 ])");

    irs::Or expected;
    auto& filter = expected.add<arangodb::iresearch::GeoFilter>();
    *filter.mutable_field() = mangleString("name", "mygeojson");
    auto* opts = filter.mutable_options();
    opts->type = arangodb::iresearch::GeoFilterType::INTERSECTS;
    opts->prefix = "";
    ASSERT_TRUE(parseCoordinates<true>(json->slice(), opts->shape, true).ok());

    assertFilterSuccess(
        vocbase(),
        R"(FOR d IN myView FILTER ANALYZER(GEO_INTERSECTS(d.name, { "type": "Point", "coordinates": [ 1, 2 ] }), "mygeojson") RETURN d)",
        expected);
    assertFilterSuccess(
        vocbase(),
        R"(FOR d IN myView FILTER ANALYZER(GEO_INTERSECTS({ "type": "Point", "coordinates": [ 1, 2 ] }, d.name), "mygeojson") RETURN d)",
        expected);
    assertFilterSuccess(
        vocbase(),
        R"(FOR d IN myView FILTER ANALYZER(GEO_INTERSECTS(d['name'],  [ 1, 2 ] ), "mygeojson") RETURN d)",
        expected);
    assertFilterSuccess(
        vocbase(),
        R"(FOR d IN myView FILTER ANALYZER(GEO_INTERSECTS([ 1, 2 ], d['name']), "mygeojson") RETURN d)",
        expected);
    assertFilterSuccess(
        vocbase(),
        R"(FOR d IN myView FILTER ANALYZER(GEO_INTERSECTS(d.name, GEO_POINT(1, 2)), "mygeojson") RETURN d)",
        expected, &ExpressionContextMock::EMPTY);
  }

  {
    auto json = VPackParser::fromJson(R"([ 1, 2 ])");

    irs::Or expected;
    auto& filter = expected.add<arangodb::iresearch::GeoFilter>();
    *filter.mutable_field() = mangleString("name", "mygeojson");
    filter.boost(1.5);
    auto* opts = filter.mutable_options();
    opts->type = arangodb::iresearch::GeoFilterType::INTERSECTS;
    opts->prefix = "";
    ASSERT_TRUE(parseCoordinates<true>(json->slice(), opts->shape, true).ok());

    ExpressionContextMock ctx;
    ctx.vars.emplace(
        "lat", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{2}));
    ctx.vars.emplace(
        "lng", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{1}));

    assertFilterSuccess(
        vocbase(),
        R"(FOR d IN myView FILTER ANALYZER(BOOST(GEO_INTERSECTS(d[_FORWARD_('name')], { "type": "Point", "coordinates": [ 1, 2 ] }), 1.5), "mygeojson") RETURN d)",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        R"(FOR d IN myView FILTER ANALYZER(BOOST(GEO_INTERSECTS({ "type": "Point", "coordinates": [ 1, 2 ] }, d.name), 1.5), "mygeojson") RETURN d)",
        expected);
    assertFilterSuccess(
        vocbase(),
        R"(LET lng=1 LET lat=2 FOR d IN myView FILTER ANALYZER(BOOST(GEO_INTERSECTS(d['name'], [lng, lat] ), 1.5), "mygeojson") RETURN d)",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        R"(LET lng=1 LET lat=2 FOR d IN myView FILTER ANALYZER(booSt(GEO_INTERSECTS([ lng, lat ], d['name']), 1.5), "mygeojson") RETURN d)",
        expected, &ctx);
  }

  // wrong number of arguments
  assertFilterParseFail(
      vocbase(),
      R"(FOR d IN myView FILTER ANALYZER(GEO_INTERSECTS(d.name), "mygeojson") RETURN d)");
  assertFilterParseFail(
      vocbase(),
      R"(FOR d IN myView FILTER ANALYZER(GEO_INTERSECTS(d['name'], [ 1, 2 ], null), "mygeojson") RETURN d)");

  // non-deterministic arg
  assertFilterParseFail(
      vocbase(),
      R"(FOR d IN myView FILTER ANALYZER(GEO_INTERSECTS(d['name'], RAND() > 0.5 ? [ 1, 2 ] : [2 : 1]), "mygeojson") RETURN d)");

  // wrong first arg type
  assertFilterFail(
      vocbase(),
      R"(FOR d IN myView FILTER ANALYZER(GEO_INTERSECTS(d[*],  [ 1, 2 ] ), "mygeojson") RETURN d)");
  assertFilterFail(
      vocbase(),
      R"(FOR d IN myView FILTER ANALYZER(GEO_INTERSECTS([1, 2],  [ 1, 2 ] ), "mygeojson") RETURN d)");
  assertFilterFail(
      vocbase(),
      R"(FOR d IN myView FILTER ANALYZER(GEO_INTERSECTS(1,  [ 1, 2 ] ), "mygeojson") RETURN d)");
  assertFilterFail(
      vocbase(),
      R"(FOR d IN myView FILTER ANALYZER(GEO_INTERSECTS('[1,2]',  [ 1, 2 ] ), "mygeojson") RETURN d)");
  assertFilterFail(
      vocbase(),
      R"(FOR d IN myView FILTER ANALYZER(GEO_INTERSECTS(null,  [ 1, 2 ] ), "mygeojson") RETURN d)");
  assertFilterFail(
      vocbase(),
      R"(FOR d IN myView FILTER ANALYZER(GEO_INTERSECTS(['1', '2'],  [ 1, 2 ] ), "mygeojson") RETURN d)");
  assertFilterFail(
      vocbase(),
      R"(FOR d IN myView FILTER ANALYZER(GEO_INTERSECTS({ "type": "Point", "coordinates": [ 1, 2 ] },  [ 1, 2 ] ), "mygeojson") RETURN d)");

  // wrong second arg
  assertFilterExecutionFail(
      vocbase(),
      R"(FOR d IN myView FILTER ANALYZER(GEO_INTERSECTS(d['name'], [ '1', '2' ] ), "mygeojson") RETURN d)");
  assertFilterExecutionFail(
      vocbase(),
      R"(FOR d IN myView FILTER ANALYZER(GEO_INTERSECTS(d['name'], 1 ), "mygeojson") RETURN d)");
  assertFilterExecutionFail(
      vocbase(),
      R"(FOR d IN myView FILTER ANALYZER(GEO_INTERSECTS(d['name'], '[1,2]'), "mygeojson") RETURN d)");
  assertFilterExecutionFail(
      vocbase(),
      R"(FOR d IN myView FILTER ANALYZER(GEO_INTERSECTS(d['name'], true), "mygeojson") RETURN d)");
  assertFilterExecutionFail(
      vocbase(),
      R"(FOR d IN myView FILTER ANALYZER(GEO_INTERSECTS(d['name'], null), "mygeojson") RETURN d)");
  assertFilterExecutionFail(
      vocbase(),
      R"(FOR d IN myView FILTER ANALYZER(GEO_INTERSECTS(d['name'], {foo:[1,2]}), "mygeojson") RETURN d)");
  assertFilterExecutionFail(
      vocbase(),
      R"(FOR d IN myView FILTER ANALYZER(GEO_INTERSECTS(d['name'], { "type": "Pointt", "coordinates": [ 1, 2 ] }), "mygeojson") RETURN d)");
}

TEST_F(IResearchFilterGeoFunctionsTest, GeoContains) {
  {
    auto json = VPackParser::fromJson(R"([ 1, 2 ])");

    irs::Or expected;
    auto& filter = expected.add<arangodb::iresearch::GeoFilter>();
    *filter.mutable_field() = mangleString("name", "mygeojson");
    auto* opts = filter.mutable_options();
    opts->type = arangodb::iresearch::GeoFilterType::IS_CONTAINED;
    opts->prefix = "";
    ASSERT_TRUE(parseCoordinates<true>(json->slice(), opts->shape, true).ok());

    ExpressionContextMock ctx;
    ctx.vars.emplace(
        "lat", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{2}));
    ctx.vars.emplace(
        "lng", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{1}));

    assertFilterSuccess(
        vocbase(),
        R"(FOR d IN myView FILTER ANALYZER(GEO_CONTAINS(d.name, { "type": "Point", "coordinates": [ 1, 2 ] }), "mygeojson") RETURN d)",
        expected);
    assertFilterSuccess(
        vocbase(),
        R"(FOR d IN myView FILTER ANALYZER(GEO_CONTAINS(d.name, GEO_POINT(1, 2)), "mygeojson") RETURN d)",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        R"(LET lng = 1 LET lat = 2 FOR d IN myView FILTER ANALYZER(GEO_CONTAINS(d.name, GEO_POINT(lng, lat)), "mygeojson") RETURN d)",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        R"(FOR d IN myView FILTER ANALYZER(GEO_CONTAINS(d['name'],  [ 1, 2 ] ), "mygeojson") RETURN d)",
        expected);
    assertFilterSuccess(
        vocbase(),
        R"(LET lat = 2 LET lng = 1 FOR d IN myView FILTER ANALYZER(GEO_CONTAINS(d['name'],  [ lng, lat ] ), "mygeojson") RETURN d)",
        expected, &ctx);
  }

  {
    auto json = VPackParser::fromJson(R"([ 1, 2 ])");

    irs::Or expected;
    auto& filter = expected.add<arangodb::iresearch::GeoFilter>();
    *filter.mutable_field() = mangleString("name", "mygeojson");
    auto* opts = filter.mutable_options();
    opts->type = arangodb::iresearch::GeoFilterType::CONTAINS;
    opts->prefix = "";
    ASSERT_TRUE(parseCoordinates<true>(json->slice(), opts->shape, true).ok());

    assertFilterSuccess(
        vocbase(),
        R"(FOR d IN myView FILTER ANALYZER(GEO_CONTAINS({ "type": "Point", "coordinates": [ 1, 2 ] }, d.name), "mygeojson") RETURN d)",
        expected);
    assertFilterSuccess(
        vocbase(),
        R"(FOR d IN myView FILTER ANALYZER(GEO_CONTAINS([ 1, 2 ], d['name']), "mygeojson") RETURN d)",
        expected);
  }

  {
    auto json = VPackParser::fromJson(R"([ 1, 2 ])");

    irs::Or expected;
    auto& filter = expected.add<arangodb::iresearch::GeoFilter>();
    *filter.mutable_field() = mangleString("name", "mygeojson");
    filter.boost(1.5);
    auto* opts = filter.mutable_options();
    opts->type = arangodb::iresearch::GeoFilterType::IS_CONTAINED;
    opts->prefix = "";
    ASSERT_TRUE(parseCoordinates<true>(json->slice(), opts->shape, true).ok());

    assertFilterSuccess(
        vocbase(),
        R"(FOR d IN myView FILTER ANALYZER(BOOST(GEO_CONTAINS(d.name, { "type": "Point", "coordinates": [ 1, 2 ] }), 1.5), "mygeojson") RETURN d)",
        expected);
    assertFilterSuccess(
        vocbase(),
        R"(FOR d IN myView FILTER ANALYZER(BOOST(GEO_CONTAINS(d['name'],  [ 1, 2 ] ), 1.5), "mygeojson") RETURN d)",
        expected);
  }

  {
    auto json = VPackParser::fromJson(R"([ 1, 2 ])");

    irs::Or expected;
    auto& filter = expected.add<arangodb::iresearch::GeoFilter>();
    *filter.mutable_field() = mangleString("name", "mygeojson");
    filter.boost(1.5);
    auto* opts = filter.mutable_options();
    opts->type = arangodb::iresearch::GeoFilterType::CONTAINS;
    opts->prefix = "";
    ASSERT_TRUE(parseCoordinates<true>(json->slice(), opts->shape, true).ok());
    assertFilterSuccess(
        vocbase(),
        R"(FOR d IN myView FILTER ANALYZER(BOOST(GEO_CONTAINS({ "type": "Point", "coordinates": [ 1, 2 ] }, d.name), 1.5), "mygeojson") RETURN d)",
        expected);
    assertFilterSuccess(
        vocbase(),
        R"(FOR d IN myView FILTER ANALYZER(booSt(GEO_CONTAINS([ 1, 2 ], d['name']), 1.5), "mygeojson") RETURN d)",
        expected);
  }

  // wrong number of arguments
  assertFilterParseFail(
      vocbase(),
      R"(FOR d IN myView FILTER ANALYZER(GEO_CONTAINS(d.name), "mygeojson") RETURN d)");
  assertFilterParseFail(
      vocbase(),
      R"(FOR d IN myView FILTER ANALYZER(GEO_CONTAINS(d['name'], [ 1, 2 ], null), "mygeojson") RETURN d)");

  // non-deterministic arg
  assertFilterParseFail(
      vocbase(),
      R"(FOR d IN myView FILTER ANALYZER(GEO_CONTAINS(d['name'], RAND() > 0.5 ? [ 1, 2 ] : [2 : 1]), "mygeojson") RETURN d)");

  // wrong first arg type
  assertFilterFail(
      vocbase(),
      R"(FOR d IN myView FILTER ANALYZER(GEO_CONTAINS(d[*],  [ 1, 2 ] ), "mygeojson") RETURN d)");
  assertFilterFail(
      vocbase(),
      R"(FOR d IN myView FILTER ANALYZER(GEO_CONTAINS([1, 2],  [ 1, 2 ] ), "mygeojson") RETURN d)");
  assertFilterFail(
      vocbase(),
      R"(FOR d IN myView FILTER ANALYZER(GEO_CONTAINS(1,  [ 1, 2 ] ), "mygeojson") RETURN d)");
  assertFilterFail(
      vocbase(),
      R"(FOR d IN myView FILTER ANALYZER(GEO_CONTAINS('[1,2]',  [ 1, 2 ] ), "mygeojson") RETURN d)");
  assertFilterFail(
      vocbase(),
      R"(FOR d IN myView FILTER ANALYZER(GEO_CONTAINS(null,  [ 1, 2 ] ), "mygeojson") RETURN d)");
  assertFilterFail(
      vocbase(),
      R"(FOR d IN myView FILTER ANALYZER(GEO_CONTAINS(['1', '2'],  [ 1, 2 ] ), "mygeojson") RETURN d)");
  assertFilterFail(
      vocbase(),
      R"(FOR d IN myView FILTER ANALYZER(GEO_CONTAINS({ "type": "Point", "coordinates": [ 1, 2 ] },  [ 1, 2 ] ), "mygeojson") RETURN d)");

  // wrong second arg
  assertFilterExecutionFail(
      vocbase(),
      R"(FOR d IN myView FILTER ANALYZER(GEO_CONTAINS(d['name'], [ '1', '2' ] ), "mygeojson") RETURN d)");
  assertFilterExecutionFail(
      vocbase(),
      R"(FOR d IN myView FILTER ANALYZER(GEO_CONTAINS(d['name'], 1 ), "mygeojson") RETURN d)");
  assertFilterExecutionFail(
      vocbase(),
      R"(FOR d IN myView FILTER ANALYZER(GEO_CONTAINS(d['name'], '[1,2]'), "mygeojson") RETURN d)");
  assertFilterExecutionFail(
      vocbase(),
      R"(FOR d IN myView FILTER ANALYZER(GEO_CONTAINS(d['name'], true), "mygeojson") RETURN d)");
  assertFilterExecutionFail(
      vocbase(),
      R"(FOR d IN myView FILTER ANALYZER(GEO_CONTAINS(d['name'], null), "mygeojson") RETURN d)");
  assertFilterExecutionFail(
      vocbase(),
      R"(FOR d IN myView FILTER ANALYZER(GEO_CONTAINS(d['name'], {foo:[1,2]}), "mygeojson") RETURN d)");
  assertFilterExecutionFail(
      vocbase(),
      R"(FOR d IN myView FILTER ANALYZER(GEO_CONTAINS(d['name'], { "type": "Pointt", "coordinates": [ 1, 2 ] }), "mygeojson") RETURN d)");
}

TEST_F(IResearchFilterGeoFunctionsTest, GeoDistance) {
  {
    irs::Or expected;
    auto& filter = expected.add<arangodb::iresearch::GeoDistanceFilter>();
    *filter.mutable_field() = mangleString("name", "mygeojson");
    auto* opts = filter.mutable_options();
    opts->origin = S2LatLng::FromDegrees(0., 0.).Normalized().ToPoint();
    opts->range.max = 5000;
    opts->range.max_type = irs::BoundType::INCLUSIVE;
    opts->prefix = "";

    ExpressionContextMock ctx;
    ctx.vars.emplace(
        "lat", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{0}));
    ctx.vars.emplace(
        "lng", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{0}));
    ctx.vars.emplace(
        "dist", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{5000}));

    assertFilterSuccess(
        vocbase(),
        R"(LET lat = 0 LET dist = 5000 FOR d IN myView FILTER ANALYZER(GEO_DISTANCE(d.name, { "type": "Point", "coordinates": [ lat, 0 ] }) <= dist, "mygeojson") RETURN d)",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        R"(FOR d IN myView FILTER ANALYZER(GEO_DISTANCE({ "type": "Point", "coordinates": [ 0, 0 ] }, d.name) <= 5000, "mygeojson") RETURN d)",
        expected);
    assertFilterSuccess(
        vocbase(),
        R"(FOR d IN myView FILTER ANALYZER(GEO_DISTANCE(d[_FORWARD_('name')], [ 0, 0 ]) <= 5000, "mygeojson") RETURN d)",
        expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess(
        vocbase(),
        R"(LET lat = 0 LET lng = 0 LET dist = 5000 FOR d IN myView FILTER ANALYZER(GEO_DISTANCE([ lat, lng ], d[_FORWARD_('name')]) <= dist, "mygeojson") RETURN d)",
        expected, &ctx);
  }

  {
    irs::Or expected;
    auto& filter = expected.add<arangodb::iresearch::GeoDistanceFilter>();
    *filter.mutable_field() = mangleString("name", "mygeojson");
    auto* opts = filter.mutable_options();
    opts->origin = S2LatLng::FromDegrees(0., 0.).Normalized().ToPoint();
    opts->range.max = 5000;
    opts->range.max_type = irs::BoundType::EXCLUSIVE;
    opts->prefix = "";

    ExpressionContextMock ctx;
    ctx.vars.emplace(
        "lat", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{0}));
    ctx.vars.emplace(
        "lng", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{0}));
    ctx.vars.emplace(
        "dist", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{5000}));

    assertFilterSuccess(
        vocbase(),
        R"(FOR d IN myView FILTER ANALYZER(GEO_DISTANCE(d.name, { "type": "Point", "coordinates": [ 0, 0 ] }) < 5000, "mygeojson") RETURN d)",
        expected);
    assertFilterSuccess(
        vocbase(),
        R"(FOR d IN myView FILTER ANALYZER(GEO_DISTANCE({ "type": "Point", "coordinates": [ 0, 0 ] }, d.name) < 5000, "mygeojson") RETURN d)",
        expected);
    assertFilterSuccess(
        vocbase(),
        R"(FOR d IN myView FILTER ANALYZER(GEO_DISTANCE(d[_FORWARD_('name')], [ 0, 0 ]) < 5000, "mygeojson") RETURN d)",
        expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess(
        vocbase(),
        R"(LET lat = 0 LET lng = 0 LET dist = 5000 FOR d IN myView FILTER ANALYZER(GEO_DISTANCE([ lng, lat ], d[_FORWARD_('name')]) < dist, "mygeojson") RETURN d)",
        expected, &ctx);
  }

  {
    irs::Or expected;
    auto& filter = expected.add<arangodb::iresearch::GeoDistanceFilter>();
    *filter.mutable_field() = mangleString("name", "mygeojson");
    auto* opts = filter.mutable_options();
    opts->origin = S2LatLng::FromDegrees(0., 0.).Normalized().ToPoint();
    opts->range.min = 5000;
    opts->range.min_type = irs::BoundType::EXCLUSIVE;
    opts->prefix = "";

    ExpressionContextMock ctx;
    ctx.vars.emplace(
        "lat", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{0}));
    ctx.vars.emplace(
        "lng", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{0}));
    ctx.vars.emplace(
        "dist", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{5000}));

    assertFilterSuccess(
        vocbase(),
        R"(FOR d IN myView FILTER ANALYZER(GEO_DISTANCE(d.name, { "type": "Point", "coordinates": [ 0, 0 ] }) > 5000, "mygeojson") RETURN d)",
        expected);
    assertFilterSuccess(
        vocbase(),
        R"(FOR d IN myView FILTER ANALYZER(GEO_DISTANCE({ "type": "Point", "coordinates": [ 0, 0 ] }, d.name) > 5000, "mygeojson") RETURN d)",
        expected);
    assertFilterSuccess(
        vocbase(),
        R"(FOR d IN myView FILTER ANALYZER(GEO_DISTANCE(d[_FORWARD_('name')], [ 0, 0 ]) > 5000, "mygeojson") RETURN d)",
        expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess(
        vocbase(),
        R"(LET lat = 0 LET lng = 0 LET dist = 5000 FOR d IN myView FILTER ANALYZER(GEO_DISTANCE([ 0, 0 ], d[_FORWARD_('name')]) > dist, "mygeojson") RETURN d)",
        expected, &ctx);
  }

  {
    irs::Or expected;
    auto& filter = expected.add<arangodb::iresearch::GeoDistanceFilter>();
    filter.boost(1.5f);
    *filter.mutable_field() = mangleString("name", "mygeojson");
    auto* opts = filter.mutable_options();
    opts->origin = S2LatLng::FromDegrees(0., 0.).Normalized().ToPoint();
    opts->range.min = 5000;
    opts->range.min_type = irs::BoundType::EXCLUSIVE;
    opts->prefix = "";

    ExpressionContextMock ctx;
    ctx.vars.emplace(
        "lat", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{0}));
    ctx.vars.emplace(
        "lng", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{0}));
    ctx.vars.emplace(
        "dist", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{5000}));

    assertFilterSuccess(
        vocbase(),
        R"(FOR d IN myView FILTER ANALYZER(BOOST(GEO_DISTANCE(d.name, { "type": "Point", "coordinates": [ 0, 0 ] }) > 5000, 1.5), "mygeojson") RETURN d)",
        expected);
    assertFilterSuccess(
        vocbase(),
        R"(FOR d IN myView FILTER ANALYZER(BOOST(GEO_DISTANCE({ "type": "Point", "coordinates": [ 0, 0 ] }, d.name) > 5000, 1.5), "mygeojson") RETURN d)",
        expected);
    assertFilterSuccess(
        vocbase(),
        R"(FOR d IN myView FILTER ANALYZER(BOOST(GEO_DISTANCE(d[_FORWARD_('name')], [ 0, 0 ]) > 5000, 1.5), "mygeojson") RETURN d)",
        expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess(
        vocbase(),
        R"(LET lat = 0 LET lng = 0 LET dist = 5000 FOR d IN myView FILTER ANALYZER(BOOST(GEO_DISTANCE([ 0, 0 ], d[_FORWARD_('name')]) > dist, 1.5), "mygeojson") RETURN d)",
        expected, &ctx);
  }

  {
    irs::Or expected;
    auto& filter = expected.add<arangodb::iresearch::GeoDistanceFilter>();
    *filter.mutable_field() = mangleString("name", "mygeojson");
    auto* opts = filter.mutable_options();
    opts->origin = S2LatLng::FromDegrees(0., 0.).Normalized().ToPoint();
    opts->range.min = 5000;
    opts->range.min_type = irs::BoundType::INCLUSIVE;
    opts->prefix = "";

    ExpressionContextMock ctx;
    ctx.vars.emplace(
        "lat", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{0}));
    ctx.vars.emplace(
        "lng", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{0}));
    ctx.vars.emplace(
        "dist", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{5000}));

    assertFilterSuccess(
        vocbase(),
        R"(FOR d IN myView FILTER ANALYZER(GEO_DISTANCE(d.name, { "type": "Point", "coordinates": [ 0, 0 ] }) >= 5000, "mygeojson") RETURN d)",
        expected);
    assertFilterSuccess(
        vocbase(),
        R"(FOR d IN myView FILTER ANALYZER(GEO_DISTANCE({ "type": "Point", "coordinates": [ 0, 0 ] }, d.name) >= 5000, "mygeojson") RETURN d)",
        expected);
    assertFilterSuccess(
        vocbase(),
        R"(FOR d IN myView FILTER ANALYZER(GEO_DISTANCE(d[_FORWARD_('name')], [ 0, 0 ]) >= 5000, "mygeojson") RETURN d)",
        expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess(
        vocbase(),
        R"(LET lat = 0 LET lng = 0 LET dist = 5000 FOR d IN myView FILTER ANALYZER(GEO_DISTANCE([ 0, 0 ], d[_FORWARD_('name')]) >= dist, "mygeojson") RETURN d)",
        expected, &ctx);
  }

  {
    irs::Or expected;
    auto& filter = expected.add<arangodb::iresearch::GeoDistanceFilter>();
    *filter.mutable_field() = mangleString("name", "mygeojson");
    auto* opts = filter.mutable_options();
    opts->origin = S2LatLng::FromDegrees(0., 0.).Normalized().ToPoint();
    opts->range.min = 5000;
    opts->range.min_type = irs::BoundType::INCLUSIVE;
    opts->range.max = 5000;
    opts->range.max_type = irs::BoundType::INCLUSIVE;
    opts->prefix = "";

    ExpressionContextMock ctx;
    ctx.vars.emplace(
        "lat", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{0}));
    ctx.vars.emplace(
        "lng", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{0}));
    ctx.vars.emplace(
        "dist", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{5000}));

    assertFilterSuccess(
        vocbase(),
        R"(FOR d IN myView FILTER ANALYZER(GEO_DISTANCE(d.name, { "type": "Point", "coordinates": [ 0, 0 ] }) == 5000, "mygeojson") RETURN d)",
        expected);
    assertFilterSuccess(
        vocbase(),
        R"(FOR d IN myView FILTER ANALYZER(GEO_DISTANCE({ "type": "Point", "coordinates": [ 0, 0 ] }, d.name) == 5000, "mygeojson") RETURN d)",
        expected);
    assertFilterSuccess(
        vocbase(),
        R"(FOR d IN myView FILTER ANALYZER(GEO_DISTANCE(d[_FORWARD_('name')], [ 0, 0 ]) == 5000, "mygeojson") RETURN d)",
        expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess(
        vocbase(),
        R"(LET lat = 0 LET lng = 0 LET dist = 5000 FOR d IN myView FILTER ANALYZER(GEO_DISTANCE([ 0, 0 ], d[_FORWARD_('name')]) == dist, "mygeojson") RETURN d)",
        expected, &ctx);
  }

  {
    irs::Or expected;
    auto& filter = expected.add<irs::And>()
                       .add<irs::Not>()
                       .filter<arangodb::iresearch::GeoDistanceFilter>();
    *filter.mutable_field() = mangleString("name", "mygeojson");
    auto* opts = filter.mutable_options();
    opts->origin = S2LatLng::FromDegrees(0., 0.).Normalized().ToPoint();
    opts->range.min = 5000;
    opts->range.min_type = irs::BoundType::INCLUSIVE;
    opts->range.max = 5000;
    opts->range.max_type = irs::BoundType::INCLUSIVE;
    opts->prefix = "";

    ExpressionContextMock ctx;
    ctx.vars.emplace(
        "lat", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{0}));
    ctx.vars.emplace(
        "lng", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{0}));
    ctx.vars.emplace(
        "dist", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{5000}));

    assertFilterSuccess(
        vocbase(),
        R"(FOR d IN myView FILTER ANALYZER(GEO_DISTANCE(d.name, { "type": "Point", "coordinates": [ 0, 0 ] }) != 5000, "mygeojson") RETURN d)",
        expected);
    assertFilterSuccess(
        vocbase(),
        R"(FOR d IN myView FILTER ANALYZER(GEO_DISTANCE({ "type": "Point", "coordinates": [ 0, 0 ] }, d.name) != 5000, "mygeojson") RETURN d)",
        expected);
    assertFilterSuccess(
        vocbase(),
        R"(FOR d IN myView FILTER ANALYZER(GEO_DISTANCE(d[_FORWARD_('name')], [ 0, 0 ]) != 5000, "mygeojson") RETURN d)",
        expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess(
        vocbase(),
        R"(LET lat = 0 LET lng = 0 LET dist = 5000 FOR d IN myView FILTER ANALYZER(GEO_DISTANCE([ 0, 0 ], d[_FORWARD_('name')]) != dist, "mygeojson") RETURN d)",
        expected, &ctx);
  }

  {
    irs::Or expected;
    auto& andGroup = expected.add<irs::And>();
    {
      auto& filter = andGroup.add<arangodb::iresearch::GeoDistanceFilter>();
      *filter.mutable_field() = mangleString("name", "mygeojson");
      auto* opts = filter.mutable_options();
      opts->origin = S2LatLng::FromDegrees(0., 0.).Normalized().ToPoint();
      opts->range.min = 5000;
      opts->range.min_type = irs::BoundType::EXCLUSIVE;
      opts->prefix = "";
    }
    {
      auto& filter = andGroup.add<arangodb::iresearch::GeoDistanceFilter>();
      *filter.mutable_field() = mangleString("name", "mygeojson");
      auto* opts = filter.mutable_options();
      opts->origin = S2LatLng::FromDegrees(0., 0.).Normalized().ToPoint();
      opts->range.max = 10000;
      opts->range.max_type = irs::BoundType::INCLUSIVE;
      opts->prefix = "";
    }

    ExpressionContextMock ctx;
    ctx.vars.emplace(
        "lat", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{0}));
    ctx.vars.emplace(
        "lng", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{0}));
    ctx.vars.emplace(
        "dist", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{5000}));

    assertFilterSuccess(vocbase(),
                        R"(FOR d IN myView
         FILTER ANALYZER(GEO_DISTANCE(d.name, { "type": "Point", "coordinates": [ 0, 0 ] }) > 5000
             && GEO_DISTANCE(d.name, { "type": "Point", "coordinates": [ 0, 0 ] }) <= 10000, "mygeojson")
         RETURN d)",
                        expected);
    assertFilterSuccess(vocbase(),
                        R"(FOR d IN myView
         FILTER ANALYZER(GEO_DISTANCE({ "type": "Point", "coordinates": [ 0, 0 ] }, d.name) > 5000
             && GEO_DISTANCE({ "type": "Point", "coordinates": [ 0, 0 ] }, d.name) <= 10000, "mygeojson")
         RETURN d)",
                        expected);
    assertFilterSuccess(vocbase(),
                        R"(FOR d IN myView
         FILTER ANALYZER(GEO_DISTANCE(d[_FORWARD_('name')], [ 0, 0 ]) > 5000
             && GEO_DISTANCE(d[_FORWARD_('name')], [ 0, 0 ]) <= 10000, "mygeojson")
         RETURN d)",
                        expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess(vocbase(),
                        R"(LET lat = 0 LET lng = 0 LET dist = 5000
         FOR d IN myView
         FILTER ANALYZER(GEO_DISTANCE([ 0, 0 ], d[_FORWARD_('name')]) > dist
             && GEO_DISTANCE([ 0, 0 ], d[_FORWARD_('name')]) <= 2*dist, "mygeojson")
         RETURN d)",
                        expected, &ctx);
  }

  // wrong number of arguments
  assertFilterParseFail(
      vocbase(),
      R"(FOR d IN myView FILTER ANALYZER(GEO_DISTANCE(d.name) < 5000, "mygeojson") RETURN d)");
  assertFilterParseFail(
      vocbase(),
      R"(FOR d IN myView FILTER ANALYZER(GEO_DISTANCE(d['name'], [ 1, 2 ], null, null) < 5000, "mygeojson") RETURN d)");
  assertExpressionFilter(
      vocbase(),
      R"(FOR d IN myView FILTER GEO_DISTANCE(d['name'], [ 1, 2 ], null) < 5000 RETURN d)");

  // non-deterministic arg
  assertFilterParseFail(
      vocbase(),
      R"(FOR d IN myView FILTER ANALYZER(GEO_DISTANCE(d['name'], RAND() > 0.5 ? [ 1, 2 ] : [2 : 1]) < 5000, "mygeojson") RETURN d)");

  assertExpressionFilter(
      vocbase(),
      R"(FOR d IN myView FILTER GEO_DISTANCE(d[*],  [ 1, 2 ] ) < 5000 RETURN d)");

  assertExpressionFilter(
      vocbase(),
      R"(FOR d IN myView FILTER GEO_DISTANCE(d['name'],  [ 1, 2 ] ) < 5000 RETURN d)");

  // wrong second arg
  assertExpressionFilter(
      vocbase(),
      R"(FOR d IN myView FILTER ANALYZER(GEO_DISTANCE(d['name'], 1 ) < 5000, "mygeojson") RETURN d)",
      irs::kNoBoost, wrappedExpressionExtractor);
  assertExpressionFilter(
      vocbase(),
      R"(FOR d IN myView FILTER ANALYZER(GEO_DISTANCE(d['name'], '[1,2]') < 5000, "mygeojson") RETURN d)",
      irs::kNoBoost, wrappedExpressionExtractor);
  assertExpressionFilter(
      vocbase(),
      R"(FOR d IN myView FILTER ANALYZER(GEO_DISTANCE(d['name'], true) < 5000, "mygeojson") RETURN d)",
      irs::kNoBoost, wrappedExpressionExtractor);
  assertExpressionFilter(
      vocbase(),
      R"(FOR d IN myView FILTER ANALYZER(GEO_DISTANCE(d['name'], null) < 5000, "mygeojson") RETURN d)",
      irs::kNoBoost, wrappedExpressionExtractor);
  assertExpressionFilter(
      vocbase(),
      R"(FOR d IN myView FILTER ANALYZER(GEO_DISTANCE(d['name'], {foo:[1,2]}) < 5000, "mygeojson") RETURN d)",
      irs::kNoBoost, wrappedExpressionExtractor);
  assertExpressionFilter(
      vocbase(),
      R"(FOR d IN myView FILTER ANALYZER(GEO_DISTANCE(d['name'], { "type": "Pointt", "coordinates": [ 1, 2 ] }) < 5000, "mygeojson") RETURN d)",
      irs::kNoBoost, wrappedExpressionExtractor);

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
    *filter.mutable_field() = mangleString("name", "mygeojson");
    auto* opts = filter.mutable_options();
    opts->origin = S2LatLng::FromDegrees(0., 0.).Normalized().ToPoint();
    ASSERT_EQ(irs::BoundType::UNBOUNDED, opts->range.min_type);
    ASSERT_EQ(0, opts->range.min);
    opts->range.max = 5000;
    opts->range.max_type = irs::BoundType::INCLUSIVE;
    opts->prefix = "";

    ExpressionContextMock ctx;
    ctx.vars.emplace(
        "lat", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{0}));
    ctx.vars.emplace(
        "lng", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{0}));
    ctx.vars.emplace(
        "dist", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{5000}));

    assertFilterSuccess(
        vocbase(),
        R"(LET lat = 0 LET dist = 5000 FOR d IN myView FILTER ANALYZER(GEO_IN_RANGE(d.name, { "type": "Point", "coordinates": [ lat, 0 ] }, 0, 5000), "mygeojson") RETURN d)",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        R"(FOR d IN myView FILTER ANALYZER(GEO_IN_RANGE(d.name, { "type": "Point", "coordinates": [ 0, 0 ] }, 0, 5000), "mygeojson") RETURN d)",
        expected);
    assertFilterSuccess(
        vocbase(),
        R"(FOR d IN myView FILTER ANALYZER(GEO_IN_RANGE(d[_FORWARD_('name')], [ 0, 0 ], 0, 5000, true), "mygeojson") RETURN d)",
        expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess(
        vocbase(),
        R"(LET lat = 0 LET lng = 0 LET dist = 5000 FOR d IN myView FILTER ANALYZER(GEO_IN_RANGE(d[_FORWARD_('name')], [ lat, lng ], lat, dist), "mygeojson") RETURN d)",
        expected, &ctx);
  }

  {
    irs::Or expected;
    auto& filter = expected.add<arangodb::iresearch::GeoDistanceFilter>();
    filter.boost(1.5f);
    *filter.mutable_field() = mangleString("name", "mygeojson");
    auto* opts = filter.mutable_options();
    opts->origin = S2LatLng::FromDegrees(0., 0.).Normalized().ToPoint();
    ASSERT_EQ(irs::BoundType::UNBOUNDED, opts->range.min_type);
    ASSERT_EQ(0, opts->range.min);
    opts->range.max = 5000;
    opts->range.max_type = irs::BoundType::INCLUSIVE;
    opts->prefix = "";

    ExpressionContextMock ctx;
    ctx.vars.emplace(
        "lat", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{0}));
    ctx.vars.emplace(
        "lng", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{0}));
    ctx.vars.emplace(
        "dist", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{5000}));

    assertFilterSuccess(
        vocbase(),
        R"(LET lat = 0 LET dist = 5000 FOR d IN myView FILTER ANALYZER(BOOST(GEO_IN_RANGE(d.name, { "type": "Point", "coordinates": [ lat, 0 ] }, 0, 5000), 1.5), "mygeojson") RETURN d)",
        expected, &ctx);
    assertFilterSuccess(
        vocbase(),
        R"(FOR d IN myView FILTER ANALYZER(boosT(GEO_IN_RANGE(d.name, { "type": "Point", "coordinates": [ 0, 0 ] }, 0, 5000), 1.5), "mygeojson") RETURN d)",
        expected);
    assertFilterSuccess(
        vocbase(),
        R"(FOR d IN myView FILTER ANALYZER(boosT(GEO_IN_RANGE(d[_FORWARD_('name')], [ 0, 0 ], 0, 5000, true), 1.5), "mygeojson") RETURN d)",
        expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess(
        vocbase(),
        R"(LET lat = 0 LET lng = 0 LET dist = 5000 FOR d IN myView FILTER ANALYZER(boosT(GEO_IN_RANGE(d[_FORWARD_('name')], [ lat, lng ], lat, dist), 1.5), "mygeojson") RETURN d)",
        expected, &ctx);
  }

  {
    irs::Or expected;
    auto& filter = expected.add<arangodb::iresearch::GeoDistanceFilter>();
    *filter.mutable_field() = mangleString("name", "mygeojson");
    auto* opts = filter.mutable_options();
    opts->origin = S2LatLng::FromDegrees(0., 0.).Normalized().ToPoint();
    ASSERT_EQ(irs::BoundType::UNBOUNDED, opts->range.min_type);
    ASSERT_EQ(0, opts->range.min);
    opts->range.max = 5000;
    opts->range.max_type = irs::BoundType::EXCLUSIVE;
    opts->prefix = "";

    ExpressionContextMock ctx;
    ctx.vars.emplace(
        "lat", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{0}));
    ctx.vars.emplace(
        "lng", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{0}));
    ctx.vars.emplace(
        "dist", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{5000}));

    assertFilterSuccess(
        vocbase(),
        R"(FOR d IN myView FILTER ANALYZER(GEO_iN_RANGE(d.name, { "type": "Point", "coordinates": [ 0, 0 ] }, 0, 5000, false, false), "mygeojson") RETURN d)",
        expected);
    assertFilterSuccess(
        vocbase(),
        R"(FOR d IN myView FILTER ANALYZER(GEO_iN_RANGE(d.name, { "type": "Point", "coordinates": [ 0, 0 ] }, 0, 5000, false, false), "mygeojson") RETURN d)",
        expected);
    assertFilterSuccess(
        vocbase(),
        R"(FOR d IN myView FILTER ANALYZER(GEO_iN_RANGE(d[_FORWARD_('name')], [ 0, 0 ], 0, 5000, false, false), "mygeojson") RETURN d)",
        expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess(
        vocbase(),
        R"(LET lat = 0 LET lng = 0 LET dist = 5000 FOR d IN myView FILTER ANALYZER(GEO_iN_RANGE(d[_FORWARD_('name')], [ lng, lat ], lat, dist, lat != lng, lat != lng), "mygeojson") RETURN d)",
        expected, &ctx);
  }

  {
    irs::Or expected;
    auto& filter = expected.add<arangodb::iresearch::GeoDistanceFilter>();
    *filter.mutable_field() = mangleString("name", "mygeojson");
    auto* opts = filter.mutable_options();
    opts->origin = S2LatLng::FromDegrees(0., 0.).Normalized().ToPoint();
    opts->range.min = 1000;
    opts->range.min_type = irs::BoundType::INCLUSIVE;
    opts->range.max = 5000;
    opts->range.max_type = irs::BoundType::EXCLUSIVE;
    opts->prefix = "";

    ExpressionContextMock ctx;
    ctx.vars.emplace(
        "lat", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{0}));
    ctx.vars.emplace(
        "lng", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{0}));
    ctx.vars.emplace(
        "dist", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{5000}));

    assertFilterSuccess(
        vocbase(),
        R"(FOR d IN myView FILTER ANALYZER(GEO_iN_RANGE(d.name, { "type": "Point", "coordinates": [ 0, 0 ] }, 1000, 5000, true, false), "mygeojson") RETURN d)",
        expected);
    assertFilterSuccess(
        vocbase(),
        R"(FOR d IN myView FILTER ANALYZER(GEO_iN_RANGE(d.name, { "type": "Point", "coordinates": [ 0, 0 ] }, 1000, 5000, true, false), "mygeojson") RETURN d)",
        expected);
    assertFilterSuccess(
        vocbase(),
        R"(FOR d IN myView FILTER ANALYZER(GEO_iN_RANGE(d[_FORWARD_('name')], [ 0, 0 ], 1000, 5000, true, false), "mygeojson") RETURN d)",
        expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess(
        vocbase(),
        R"(LET lat = 0 LET lng = 0 LET dist = 5000 FOR d IN myView FILTER ANALYZER(GEO_iN_RANGE(d[_FORWARD_('name')], [ lng, lat ], 1000 + lat, dist, lat == lng, lat != lng), "mygeojson") RETURN d)",
        expected, &ctx);
  }

  {
    irs::Or expected;
    auto& filter = expected.add<arangodb::iresearch::GeoDistanceFilter>();
    *filter.mutable_field() = mangleString("name", "mygeojson");
    auto* opts = filter.mutable_options();
    opts->origin = S2LatLng::FromDegrees(0., 0.).Normalized().ToPoint();
    opts->range.min = 5000;
    opts->range.min_type = irs::BoundType::INCLUSIVE;
    opts->range.max = 5000;
    opts->range.max_type = irs::BoundType::INCLUSIVE;
    opts->prefix = "";

    ExpressionContextMock ctx;
    ctx.vars.emplace(
        "lat", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{0}));
    ctx.vars.emplace(
        "lng", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{0}));
    ctx.vars.emplace(
        "dist", arangodb::aql::AqlValue(arangodb::aql::AqlValueHintInt{5000}));

    assertFilterSuccess(
        vocbase(),
        R"(FOR d IN myView FILTER ANALYZER(GEO_iN_RANGE(d.name, { "type": "Point", "coordinates": [ 0, 0 ] }, 5000, 5000, true, true), "mygeojson") RETURN d)",
        expected);
    assertFilterSuccess(
        vocbase(),
        R"(FOR d IN myView FILTER ANALYZER(GEO_iN_RANGE(d.name, { "type": "Point", "coordinates": [ 0, 0 ] }, 5000, 5000, true, true), "mygeojson") RETURN d)",
        expected);
    assertFilterSuccess(
        vocbase(),
        R"(FOR d IN myView FILTER ANALYZER(GEO_iN_RANGE(d[_FORWARD_('name')], [ 0, 0 ], 5000, 5000, true, true), "mygeojson") RETURN d)",
        expected, &ExpressionContextMock::EMPTY);
    assertFilterSuccess(
        vocbase(),
        R"(LET lat = 0 LET lng = 0 LET dist = 5000 FOR d IN myView FILTER ANALYZER(GEO_iN_RANGE(d[_FORWARD_('name')], [ lng, lat ], dist, dist, lat == lng, lat == lng), "mygeojson") RETURN d)",
        expected, &ctx);
  }

  // wrong number of arguments
  assertFilterParseFail(
      vocbase(),
      R"(FOR d IN myView FILTER ANALYZER(GEO_IN_RANGE(d.name), "mygeojson") RETURN d)");
  assertFilterParseFail(
      vocbase(),
      R"(FOR d IN myView FILTER ANALYZER(GEO_IN_RANGE(d.name, [1,2]), "mygeojson") RETURN d)");
  assertFilterParseFail(
      vocbase(),
      R"(FOR d IN myView FILTER ANALYZER(GEO_IN_RANGE(d.name, [1,2], 0), "mygeojson") RETURN d)");
  assertFilterParseFail(
      vocbase(),
      R"(FOR d IN myView FILTER ANALYZER(GEO_IN_RANGE(d.name, [1,2], 0, 5000, true, false, "sphere", null), "mygeojson") RETURN d)");

  // ellipsoid is set
  assertFilterFail(
      vocbase(),
      R"(FOR d IN myView FILTER ANALYZER(GEO_IN_RANGE(d.name, [1,2], 0, 5000, true, false, "sphere"), "mygeojson") RETURN d)");

  // non-deterministic arg
  assertFilterParseFail(
      vocbase(),
      R"(FOR d IN myView FILTER ANALYZER(GEO_IN_RANGE(d['name'], RAND() > 0.5 ? [ 1, 2 ] : [2 : 1], 0, 5000), "mygeojson") RETURN d)");

  // wrong first arg
  assertFilterFail(
      vocbase(),
      R"(FOR d IN myView FILTER ANALYZER(GEO_IN_RANGE(d[*],  [ 1, 2 ], 0, 5000, true, true), "mygeojson") RETURN d)",
      &ExpressionContextMock::EMPTY);
  assertFilterFail(
      vocbase(),
      R"(FOR d IN myView FILTER ANALYZER(GEO_IN_RANGE([1,2],  [ 1, 2 ], 0, 5000, true, true), "mygeojson") RETURN d)");
  assertFilterFail(
      vocbase(),
      R"(FOR d IN myView FILTER ANALYZER(GEO_IN_RANGE(1,  [ 1, 2 ], 0, 5000, true, true), "mygeojson") RETURN d)");
  assertFilterFail(
      vocbase(),
      R"(FOR d IN myView FILTER ANALYZER(GEO_IN_RANGE('[1,2]',  [ 1, 2 ], 0, 5000, true, true), "mygeojson") RETURN d)");
  assertFilterFail(
      vocbase(),
      R"(FOR d IN myView FILTER ANALYZER(GEO_IN_RANGE(null,  [ 1, 2 ], 0, 5000, true, true), "mygeojson") RETURN d)");
  assertFilterFail(
      vocbase(),
      R"(FOR d IN myView FILTER ANALYZER(GEO_IN_RANGE(['1', '2'],  [ 1, 2 ], 0, 5000, true, true), "mygeojson") RETURN d)");
  assertFilterFail(
      vocbase(),
      R"(FOR d IN myView FILTER ANALYZER(GEO_IN_RANGE({ "type": "Point", "coordinates": [ 1, 2 ] },  [ 1, 2 ], 0, 5000, true, true), "mygeojson") RETURN d)");

  // wrong second arg
  assertFilterExecutionFail(
      vocbase(),
      R"(FOR d IN myView FILTER ANALYZER(GEO_IN_RANGE(d.name, d[*], 0, 5000, true, true), "mygeojson") RETURN d)",
      &ExpressionContextMock::EMPTY);
  assertFilterExecutionFail(
      vocbase(),
      R"(FOR d IN myView FILTER ANALYZER(GEO_IN_RANGE(d.name, '[1,2]', 0, 5000, true, true), "mygeojson") RETURN d)");
  assertFilterExecutionFail(
      vocbase(),
      R"(FOR d IN myView FILTER ANALYZER(GEO_IN_RANGE(d.name, ['1','2'], 0, 5000, true, true), "mygeojson") RETURN d)");
  assertFilterExecutionFail(
      vocbase(),
      R"(FOR d IN myView FILTER ANALYZER(GEO_IN_RANGE(d.name, 1, 0, 5000, true, true), "mygeojson") RETURN d)");
  assertFilterExecutionFail(
      vocbase(),
      R"(FOR d IN myView FILTER ANALYZER(GEO_IN_RANGE(d.name, null, 0, 5000, true, true), "mygeojson") RETURN d)");
  assertFilterExecutionFail(
      vocbase(),
      R"(FOR d IN myView FILTER ANALYZER(GEO_IN_RANGE(d.name, true, 0, 5000, true, true), "mygeojson") RETURN d)");
  assertFilterExecutionFail(
      vocbase(),
      R"(FOR d IN myView FILTER ANALYZER(GEO_IN_RANGE(d.name, { "type": "Pointt", "coordinates": [ 1, 2 ] }, 0, 5000, true, true), "mygeojson") RETURN d)");

  // wrong third arg
  assertFilterFail(
      vocbase(),
      R"(FOR d IN myView FILTER ANALYZER(GEO_IN_RANGE(d.name, [1,2], '0', 5000, true, true), "mygeojson") RETURN d)");
  assertFilterFail(
      vocbase(),
      R"(FOR d IN myView FILTER ANALYZER(GEO_IN_RANGE(d.name, [1,2], true, 5000, true, true), "mygeojson") RETURN d)");
  assertFilterFail(
      vocbase(),
      R"(FOR d IN myView FILTER ANALYZER(GEO_IN_RANGE(d.name, [1,2], null, 5000, true, true), "mygeojson") RETURN d)");
  assertFilterFail(
      vocbase(),
      R"(FOR d IN myView FILTER ANALYZER(GEO_IN_RANGE(d.name, [1,2], [0], 5000, true, true), "mygeojson") RETURN d)");
  assertFilterFail(
      vocbase(),
      R"(FOR d IN myView FILTER ANALYZER(GEO_IN_RANGE(d.name, [1,2], {}, 5000, true, true), "mygeojson") RETURN d)");
  assertFilterExecutionFail(
      vocbase(),
      R"(FOR d IN myView FILTER ANALYZER(GEO_IN_RANGE(d.name, [1,2], d[*], 5000, true, true), "mygeojson") RETURN d)",
      &ExpressionContextMock::EMPTY);

  // wrong 4th arg
  assertFilterFail(
      vocbase(),
      R"(FOR d IN myView FILTER ANALYZER(GEO_IN_RANGE(d.name, [1,2], 0, '5000', true, true), "mygeojson") RETURN d)");
  assertFilterFail(
      vocbase(),
      R"(FOR d IN myView FILTER ANALYZER(GEO_IN_RANGE(d.name, [1,2], 0, true, true, true), "mygeojson") RETURN d)");
  assertFilterFail(
      vocbase(),
      R"(FOR d IN myView FILTER ANALYZER(GEO_IN_RANGE(d.name, [1,2], 0, null, true, true), "mygeojson") RETURN d)");
  assertFilterFail(
      vocbase(),
      R"(FOR d IN myView FILTER ANALYZER(GEO_IN_RANGE(d.name, [1,2], 0, [5000], true, true), "mygeojson") RETURN d)");
  assertFilterFail(
      vocbase(),
      R"(FOR d IN myView FILTER ANALYZER(GEO_IN_RANGE(d.name, [1,2], 0, {}, true, true), "mygeojson") RETURN d)");
  assertFilterExecutionFail(
      vocbase(),
      R"(FOR d IN myView FILTER ANALYZER(GEO_IN_RANGE(d.name, [1,2], 0, d[*], true, true), "mygeojson") RETURN d)",
      &ExpressionContextMock::EMPTY);

  // wrong 5th arg
  assertFilterFail(
      vocbase(),
      R"(FOR d IN myView FILTER ANALYZER(GEO_IN_RANGE(d.name, [1,2], 0, 5000, 'true', true), "mygeojson") RETURN d)");
  assertFilterFail(
      vocbase(),
      R"(FOR d IN myView FILTER ANALYZER(GEO_IN_RANGE(d.name, [1,2], 0, 5000, null, true), "mygeojson") RETURN d)");
  assertFilterFail(
      vocbase(),
      R"(FOR d IN myView FILTER ANALYZER(GEO_IN_RANGE(d.name, [1,2], 0, 5000, 0, true), "mygeojson") RETURN d)");
  assertFilterFail(
      vocbase(),
      R"(FOR d IN myView FILTER ANALYZER(GEO_IN_RANGE(d.name, [1,2], 0, 5000, [true], true), "mygeojson") RETURN d)");
  assertFilterFail(
      vocbase(),
      R"(FOR d IN myView FILTER ANALYZER(GEO_IN_RANGE(d.name, [1,2], 0, 5000, {}, true), "mygeojson") RETURN d)");
  assertFilterExecutionFail(
      vocbase(),
      R"(FOR d IN myView FILTER ANALYZER(GEO_IN_RANGE(d.name, [1,2], 0, 5000, d[*], true), "mygeojson") RETURN d)",
      &ExpressionContextMock::EMPTY);

  // wrong 6th arg
  assertFilterFail(
      vocbase(),
      R"(FOR d IN myView FILTER ANALYZER(GEO_IN_RANGE(d.name, [1,2], 0, 5000, true, 'true'), "mygeojson") RETURN d)");
  assertFilterFail(
      vocbase(),
      R"(FOR d IN myView FILTER ANALYZER(GEO_IN_RANGE(d.name, [1,2], 0, 5000, true, null), "mygeojson") RETURN d)");
  assertFilterFail(
      vocbase(),
      R"(FOR d IN myView FILTER ANALYZER(GEO_IN_RANGE(d.name, [1,2], 0, 5000, true, 0), "mygeojson") RETURN d)");
  assertFilterFail(
      vocbase(),
      R"(FOR d IN myView FILTER ANALYZER(GEO_IN_RANGE(d.name, [1,2], 0, 5000, true, [true]), "mygeojson") RETURN d)");
  assertFilterFail(
      vocbase(),
      R"(FOR d IN myView FILTER ANALYZER(GEO_IN_RANGE(d.name, [1,2], 0, 5000, true, {}), "mygeojson") RETURN d)");
  assertFilterExecutionFail(
      vocbase(),
      R"(FOR d IN myView FILTER ANALYZER(GEO_IN_RANGE(d.name, [1,2], 0, 5000, true, d[*]), "mygeojson") RETURN d)",
      &ExpressionContextMock::EMPTY);
}
