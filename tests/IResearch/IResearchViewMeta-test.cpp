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

#include "../Mocks/StorageEngineMock.h"

#include "utils/locale_utils.hpp"
#include "utils/lz4compression.hpp"

#include "ApplicationFeatures/ApplicationServer.h"
#include "IResearch/IResearchViewMeta.h"
#include "IResearch/VelocyPackHelper.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "velocypack/Iterator.h"
#include "velocypack/Parser.h"
#include "Basics/VelocyPackHelper.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

class IResearchViewMetaTest : public ::testing::Test {
 protected:
  StorageEngineMock engine;
  arangodb::application_features::ApplicationServer server;

  IResearchViewMetaTest() : engine(server), server(nullptr, nullptr) {
    auto& selector = server.addFeature<arangodb::EngineSelectorFeature>();
    selector.setEngineTesting(&engine);
  }

  ~IResearchViewMetaTest() {
    server.getFeature<arangodb::EngineSelectorFeature>().setEngineTesting(nullptr);
  }
};

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief setup
////////////////////////////////////////////////////////////////////////////////

TEST_F(IResearchViewMetaTest, test_defaults) {
  arangodb::iresearch::IResearchViewMeta meta;
  arangodb::iresearch::IResearchViewMetaState metaState;

  EXPECT_TRUE(true == metaState._collections.empty());
  EXPECT_TRUE(true == (2 == meta._cleanupIntervalStep));
  EXPECT_TRUE(true == (1000 == meta._commitIntervalMsec));
  EXPECT_TRUE(true == (10 * 1000 == meta._consolidationIntervalMsec));
  EXPECT_TRUE(std::string("tier") ==
              meta._consolidationPolicy.properties().get("type").copyString());
  EXPECT_TRUE(false == !meta._consolidationPolicy.policy());
  EXPECT_TRUE(1 == meta._consolidationPolicy.properties().get("segmentsMin").getNumber<size_t>());
  EXPECT_TRUE(10 == meta._consolidationPolicy.properties().get("segmentsMax").getNumber<size_t>());
  EXPECT_TRUE(size_t(2) * (1 << 20) ==
              meta._consolidationPolicy.properties().get("segmentsBytesFloor").getNumber<size_t>());
  EXPECT_TRUE(size_t(5) * (1 << 30) ==
              meta._consolidationPolicy.properties().get("segmentsBytesMax").getNumber<size_t>());
  EXPECT_TRUE(std::string("C") == irs::locale_utils::name(meta._locale));
  EXPECT_TRUE(0 == meta._writebufferActive);
  EXPECT_TRUE(64 == meta._writebufferIdle);
  EXPECT_TRUE(32 * (size_t(1) << 20) == meta._writebufferSizeMax);
  EXPECT_TRUE(meta._primarySort.empty());
}

TEST_F(IResearchViewMetaTest, test_inheritDefaults) {
  auto viewJson = arangodb::velocypack::Parser::fromJson(
      "{ \"id\": 123, \"name\": \"testView\", \"type\": \"testType\" }");
  arangodb::iresearch::IResearchViewMeta defaults;
  arangodb::iresearch::IResearchViewMetaState defaultsState;
  arangodb::iresearch::IResearchViewMeta meta;
  arangodb::iresearch::IResearchViewMetaState metaState;
  std::string tmpString;

  defaultsState._collections.insert(arangodb::DataSourceId{42});
  defaults._cleanupIntervalStep = 654;
  defaults._commitIntervalMsec = 321;
  defaults._consolidationIntervalMsec = 456;
  defaults._consolidationPolicy = arangodb::iresearch::IResearchViewMeta::ConsolidationPolicy(
      irs::index_writer::consolidation_policy_t(),
      std::move(*arangodb::velocypack::Parser::fromJson(
          "{ \"type\": \"tier\", \"threshold\": 0.11 }")));
  defaults._locale = irs::locale_utils::locale("C");
  defaults._writebufferActive = 10;
  defaults._writebufferIdle = 11;
  defaults._writebufferSizeMax = 12;
  defaults._primarySort.emplace_back(
      std::vector<arangodb::basics::AttributeName>{
          arangodb::basics::AttributeName(VPackStringRef("nested")),
          arangodb::basics::AttributeName(VPackStringRef("field"))},
      true);
  defaults._primarySort.emplace_back(
      std::vector<arangodb::basics::AttributeName>{
          arangodb::basics::AttributeName(VPackStringRef("another")),
          arangodb::basics::AttributeName(VPackStringRef("nested")),
          arangodb::basics::AttributeName(VPackStringRef("field"))},
      true);

  {
    auto json = arangodb::velocypack::Parser::fromJson("{}");
    EXPECT_TRUE(meta.init(json->slice(), tmpString, defaults));
    EXPECT_TRUE(metaState.init(json->slice(), tmpString, defaultsState));
    EXPECT_EQ(1, metaState._collections.size());
    EXPECT_EQ(42, metaState._collections.begin()->id());
    EXPECT_EQ(654, meta._cleanupIntervalStep);
    EXPECT_EQ(321, meta._commitIntervalMsec);
    EXPECT_EQ(456, meta._consolidationIntervalMsec);
    EXPECT_EQ(std::string("tier"),
              meta._consolidationPolicy.properties().get("type").copyString());
    EXPECT_EQ(nullptr, meta._consolidationPolicy.policy());
    EXPECT_EQ(.11f, meta._consolidationPolicy.properties().get("threshold").getNumber<float>());
    EXPECT_EQ(std::string("C"), irs::locale_utils::name(meta._locale));
    EXPECT_EQ(10, meta._writebufferActive);
    EXPECT_EQ(11, meta._writebufferIdle);
    EXPECT_EQ(12, meta._writebufferSizeMax);
    EXPECT_EQ(meta._primarySort, defaults._primarySort);
    EXPECT_EQ(meta._primarySortCompression, defaults._primarySortCompression);
  }
}

TEST_F(IResearchViewMetaTest, test_readDefaults) {
  arangodb::iresearch::IResearchViewMeta meta;
  arangodb::iresearch::IResearchViewMetaState metaState;
  std::string tmpString;

  {
    auto viewJson = arangodb::velocypack::Parser::fromJson(
        "{ \"id\": 123, \"name\": \"testView\", \"type\": \"testType\" }");
    auto json = arangodb::velocypack::Parser::fromJson("{}");
    EXPECT_TRUE(true == meta.init(json->slice(), tmpString));
    EXPECT_TRUE((true == metaState.init(json->slice(), tmpString)));
    EXPECT_TRUE((true == metaState._collections.empty()));
    EXPECT_TRUE(2 == meta._cleanupIntervalStep);
    EXPECT_TRUE((1000 == meta._commitIntervalMsec));
    EXPECT_TRUE(10 * 1000 == meta._consolidationIntervalMsec);
    EXPECT_TRUE((std::string("tier") ==
                 meta._consolidationPolicy.properties().get("type").copyString()));
    EXPECT_TRUE((false == !meta._consolidationPolicy.policy()));
    EXPECT_TRUE(1 == meta._consolidationPolicy.properties().get("segmentsMin").getNumber<size_t>());
    EXPECT_TRUE(10 == meta._consolidationPolicy.properties().get("segmentsMax").getNumber<size_t>());
    EXPECT_TRUE(size_t(2) * (1 << 20) ==
                meta._consolidationPolicy.properties().get("segmentsBytesFloor").getNumber<size_t>());
    EXPECT_TRUE(size_t(5) * (1 << 30) ==
                meta._consolidationPolicy.properties().get("segmentsBytesMax").getNumber<size_t>());
    EXPECT_TRUE(std::string("C") == irs::locale_utils::name(meta._locale));
    EXPECT_TRUE((0 == meta._writebufferActive));
    EXPECT_TRUE((64 == meta._writebufferIdle));
    EXPECT_TRUE((32 * (size_t(1) << 20) == meta._writebufferSizeMax));
    EXPECT_TRUE(meta._primarySort.empty());
    EXPECT_EQ(irs::type<irs::compression::lz4>::id(),
              meta._primarySortCompression);
  }
} 

TEST_F(IResearchViewMetaTest, test_readCustomizedValues) {
  std::unordered_set<arangodb::DataSourceId> expectedCollections = {
      arangodb::DataSourceId{42}};
  arangodb::iresearch::IResearchViewMeta meta;
  arangodb::iresearch::IResearchViewMetaState metaState;

  // .............................................................................
  // test invalid value
  // .............................................................................

  {
    std::string errorField;
    auto json = arangodb::velocypack::Parser::fromJson(
        "{ \"collections\": \"invalid\" }");
    EXPECT_TRUE((true == meta.init(json->slice(), errorField)));
    EXPECT_TRUE((false == metaState.init(json->slice(), errorField)));
    EXPECT_TRUE(std::string("collections") == errorField);
  }

  {
    std::string errorField;
    auto json = arangodb::velocypack::Parser::fromJson(
        "{ \"commitIntervalMsec\": 0.5 }");
    EXPECT_TRUE((true == metaState.init(json->slice(), errorField)));
    EXPECT_TRUE(false == meta.init(json->slice(), errorField));
    EXPECT_TRUE(std::string("commitIntervalMsec") == errorField);
  }

  {
    std::string errorField;
    auto json = arangodb::velocypack::Parser::fromJson(
        "{ \"consolidationIntervalMsec\": 0.5 }");
    EXPECT_TRUE((true == metaState.init(json->slice(), errorField)));
    EXPECT_TRUE(false == meta.init(json->slice(), errorField));
    EXPECT_TRUE(std::string("consolidationIntervalMsec") == errorField);
  }

  {
    std::string errorField;
    auto json = arangodb::velocypack::Parser::fromJson(
        "{ \"cleanupIntervalStep\": 0.5 }");
    EXPECT_TRUE((true == metaState.init(json->slice(), errorField)));
    EXPECT_TRUE(false == meta.init(json->slice(), errorField));
    EXPECT_TRUE(std::string("cleanupIntervalStep") == errorField);
  }

  {
    std::string errorField;
    auto json = arangodb::velocypack::Parser::fromJson(
        "{ \"consolidationPolicy\": \"invalid\" }");
    EXPECT_TRUE((true == metaState.init(json->slice(), errorField)));
    EXPECT_TRUE(false == meta.init(json->slice(), errorField));
    EXPECT_TRUE(std::string("consolidationPolicy") == errorField);
  }

  {
    std::string errorField;
    auto json = arangodb::velocypack::Parser::fromJson(
        "{ \"consolidationPolicy\": null }");
    EXPECT_TRUE(false == meta.init(json->slice(), errorField));
    EXPECT_TRUE((std::string("consolidationPolicy") == errorField));
  }

  // consolidation policy "bytes_accum"

  {
    std::string errorField;
    auto json = arangodb::velocypack::Parser::fromJson(
        "{ \"consolidationPolicy\": { \"type\": \"bytes_accum\", "
        "\"threshold\": -0.5 } }");
    EXPECT_TRUE((true == metaState.init(json->slice(), errorField)));
    EXPECT_TRUE(false == meta.init(json->slice(), errorField));
    EXPECT_TRUE((std::string("consolidationPolicy.threshold") == errorField));
  }

  {
    std::string errorField;
    auto json = arangodb::velocypack::Parser::fromJson(
        "{ \"consolidationPolicy\": { \"type\": \"bytes_accum\", "
        "\"threshold\": 1.5 } }");
    EXPECT_TRUE((true == metaState.init(json->slice(), errorField)));
    EXPECT_TRUE(false == meta.init(json->slice(), errorField));
    EXPECT_TRUE((std::string("consolidationPolicy.threshold") == errorField));
  }

  // consolidation policy "tier"

  {
    std::string errorField;
    auto json = arangodb::velocypack::Parser::fromJson(
        "{ \"consolidationPolicy\": { \"type\": \"tier\", \"segmentsMin\": -1  "
        "} }");
    EXPECT_TRUE((true == metaState.init(json->slice(), errorField)));
    EXPECT_TRUE(false == meta.init(json->slice(), errorField));
    EXPECT_TRUE((std::string("consolidationPolicy.segmentsMin") == errorField));
  }

  {
    std::string errorField;
    auto json = arangodb::velocypack::Parser::fromJson(
        "{ \"consolidationPolicy\": { \"type\": \"tier\", \"segmentsMax\": -1  "
        "} }");
    EXPECT_TRUE((true == metaState.init(json->slice(), errorField)));
    EXPECT_TRUE(false == meta.init(json->slice(), errorField));
    EXPECT_TRUE((std::string("consolidationPolicy.segmentsMax") == errorField));
  }

  {
    std::string errorField;
    auto json = arangodb::velocypack::Parser::fromJson(
        "{ \"consolidationPolicy\": { \"type\": \"tier\", "
        "\"segmentsBytesFloor\": -1  } }");
    EXPECT_TRUE((true == metaState.init(json->slice(), errorField)));
    EXPECT_TRUE(false == meta.init(json->slice(), errorField));
    EXPECT_TRUE((std::string("consolidationPolicy.segmentsBytesFloor") == errorField));
  }

  {
    std::string errorField;
    auto json = arangodb::velocypack::Parser::fromJson(
        "{ \"consolidationPolicy\": { \"type\": \"tier\", "
        "\"segmentsBytesMax\": -1  } }");
    EXPECT_TRUE((true == metaState.init(json->slice(), errorField)));
    EXPECT_TRUE(false == meta.init(json->slice(), errorField));
    EXPECT_TRUE((std::string("consolidationPolicy.segmentsBytesMax") == errorField));
  }

  {
    std::string errorField;
    auto json = arangodb::velocypack::Parser::fromJson(
        "{ \"consolidationPolicy\": { \"type\": \"invalid\" } }");
    EXPECT_TRUE((true == metaState.init(json->slice(), errorField)));
    EXPECT_TRUE(false == meta.init(json->slice(), errorField));
    EXPECT_EQ(std::string("consolidationPolicy.type"), errorField);
  }

  {
    std::string errorField;
    auto json = arangodb::velocypack::Parser::fromJson("{ \"version\": -0.5 }");
    EXPECT_TRUE((true == metaState.init(json->slice(), errorField)));
    EXPECT_TRUE(false == meta.init(json->slice(), errorField));
    EXPECT_TRUE((std::string("version") == errorField));
  }

  {
    std::string errorField;
    auto json = arangodb::velocypack::Parser::fromJson("{ \"version\": 1.5 }");
    EXPECT_TRUE((true == metaState.init(json->slice(), errorField)));
    EXPECT_TRUE(false == meta.init(json->slice(), errorField));
    EXPECT_TRUE((std::string("version") == errorField));
  }

  {
    std::string errorField;
    auto json =
        arangodb::velocypack::Parser::fromJson("{ \"primarySort\": {} }");
    EXPECT_TRUE((true == metaState.init(json->slice(), errorField)));
    EXPECT_TRUE(false == meta.init(json->slice(), errorField));
    EXPECT_TRUE("primarySort" == errorField);
  }

  {
    std::string errorField;
    auto json =
        arangodb::velocypack::Parser::fromJson("{ \"primarySort\": [ 1 ] }");
    EXPECT_TRUE((true == metaState.init(json->slice(), errorField)));
    EXPECT_TRUE(false == meta.init(json->slice(), errorField));
    EXPECT_EQ("primarySort[0]", errorField);
  }

  {
    std::string errorField;
    auto json = arangodb::velocypack::Parser::fromJson(
        "{ \"primarySort\": [ { \"field\":{ }, \"direction\":\"aSc\" } ] }");
    EXPECT_TRUE((true == metaState.init(json->slice(), errorField)));
    EXPECT_TRUE(false == meta.init(json->slice(), errorField));
    EXPECT_EQ("primarySort[0].field", errorField);
  }

  {
    std::string errorField;
    auto json = arangodb::velocypack::Parser::fromJson(
        "{ \"primarySort\": [ { \"field\":{ }, \"asc\":true } ] }");
    EXPECT_TRUE((true == metaState.init(json->slice(), errorField)));
    EXPECT_TRUE(false == meta.init(json->slice(), errorField));
    EXPECT_EQ("primarySort[0].field", errorField);
  }

  {
    std::string errorField;
    auto json = arangodb::velocypack::Parser::fromJson(
        "{ \"primarySort\": [ { \"field\":\"nested.field\", "
        "\"direction\":\"xxx\" }, 4 ] }");
    EXPECT_TRUE((true == metaState.init(json->slice(), errorField)));
    EXPECT_TRUE(false == meta.init(json->slice(), errorField));
    EXPECT_EQ("primarySort[0].direction", errorField);
  }

  {
    std::string errorField;
    auto json = arangodb::velocypack::Parser::fromJson(
        "{ \"primarySort\": [ { \"field\":\"nested.field\", \"asc\":\"xxx\" }, "
        "4 ] }");
    EXPECT_TRUE((true == metaState.init(json->slice(), errorField)));
    EXPECT_TRUE(false == meta.init(json->slice(), errorField));
    EXPECT_TRUE("primarySort[0].asc" == errorField);
  }

  {
    std::string errorField;
    auto json = arangodb::velocypack::Parser::fromJson(
        "{ \"primarySort\": [ { \"field\":\"nested.field\", "
        "\"direction\":\"aSc\" }, 4 ] }");
    EXPECT_TRUE((true == metaState.init(json->slice(), errorField)));
    EXPECT_TRUE(false == meta.init(json->slice(), errorField));
    EXPECT_TRUE("primarySort[1]" == errorField);
  }

  {
    std::string errorField;
    auto json = arangodb::velocypack::Parser::fromJson(
        "{ \"primarySort\": [ { \"field\":\"nested.field\", \"asc\": true }, { "
        "\"field\":1, \"direction\":\"aSc\" } ] }");
    EXPECT_TRUE((true == metaState.init(json->slice(), errorField)));
    EXPECT_TRUE(false == meta.init(json->slice(), errorField));
    EXPECT_TRUE("primarySort[1].field" == errorField);
  }

  {
    std::string errorField;
    auto json = arangodb::velocypack::Parser::fromJson(
      "{ \"storedValues\": { \"fields\":[\"nested.field\"], \"compression\": \"lz4\" }}");
    EXPECT_TRUE(metaState.init(json->slice(), errorField));
    EXPECT_FALSE(meta.init(json->slice(), errorField));
    EXPECT_EQ("storedValues", errorField);
  }
  {
    std::string errorField;
    auto json = arangodb::velocypack::Parser::fromJson(
      "{ \"storedValues\": [ { \"fields\":[\"nested.field\"], \"compression\": \"lz4\" }, { "
      "\"fields\":1, \"compression\":\"none\" } ] }");
    EXPECT_TRUE(metaState.init(json->slice(), errorField));
    EXPECT_FALSE(meta.init(json->slice(), errorField));
    EXPECT_EQ("storedValues[1].fields", errorField);
  }
  {
    std::string errorField;
    auto json = arangodb::velocypack::Parser::fromJson(
      "{ \"storedValues\": [ { \"fields\":[\"nested.field\"], \"compression\": \"lz4\" }, { "
      "\"fields\":[\"1\"], \"compression\":\"InVaLid\" } ] }");
    EXPECT_TRUE(metaState.init(json->slice(), errorField));
    EXPECT_FALSE(meta.init(json->slice(), errorField));
    EXPECT_EQ("storedValues[1].compression", errorField);
  }

  {
    std::string errorField;
    auto json = arangodb::velocypack::Parser::fromJson(
      "{ \"storedValues\": [ { \"fields\":\"nested.field\", \"compression\": \"lz4\" }, { "
      "\"fields\":\"1\"} ] }");
    EXPECT_TRUE(metaState.init(json->slice(), errorField));
    EXPECT_FALSE(meta.init(json->slice(), errorField));
    EXPECT_EQ("storedValues[0].fields", errorField);
  }

  {
    std::string errorField;
    auto json = arangodb::velocypack::Parser::fromJson(
      "{ \"storedValues\": [ { \"fields\":[\"nested.field\"], \"compression\": \"lz4\" }, { "
      "\"field\":\"1\"} ] }");
    EXPECT_TRUE(metaState.init(json->slice(), errorField));
    EXPECT_FALSE(meta.init(json->slice(), errorField));
    EXPECT_EQ("storedValues[1]", errorField);
  }

  // .............................................................................
  // test valid value
  // .............................................................................

  // test all parameters set to custom values
  std::string errorField;
  auto json = arangodb::velocypack::Parser::fromJson(
      "{ \
        \"collections\": [ 42 ], \
        \"commitIntervalMsec\": 321, \
        \"consolidationIntervalMsec\": 456, \
        \"cleanupIntervalStep\": 654, \
        \"consolidationPolicy\": { \"type\": \"bytes_accum\", \"threshold\": 0.11 }, \
        \"locale\": \"ru_RU.KOI8-R\", \
        \"version\": 9, \
        \"writebufferActive\": 10, \
        \"writebufferIdle\": 11, \
        \"writebufferSizeMax\": 12, \
        \"primarySortCompression\": \"none\",\
        \"primarySort\" : [ \
          { \"field\": \"nested.field\", \"direction\": \"desc\" }, \
          { \"field\": \"another.nested.field\", \"direction\": \"asc\" }, \
          { \"field\": \"field\", \"asc\": false }, \
          { \"field\": \".field\", \"asc\": true } \
        ] \
    }");
  EXPECT_TRUE(true == meta.init(json->slice(), errorField));
  EXPECT_TRUE((true == metaState.init(json->slice(), errorField)));
  EXPECT_TRUE((1 == metaState._collections.size()));

  for (auto& collection : metaState._collections) {
    EXPECT_TRUE(1 == expectedCollections.erase(collection));
  }

  EXPECT_TRUE(true == expectedCollections.empty());
  EXPECT_TRUE((42 == metaState._collections.begin()->id()));
  EXPECT_TRUE(654 == meta._cleanupIntervalStep);
  EXPECT_TRUE((321 == meta._commitIntervalMsec));
  EXPECT_TRUE(456 == meta._consolidationIntervalMsec);
  EXPECT_TRUE((std::string("bytes_accum") ==
               meta._consolidationPolicy.properties().get("type").copyString()));
  EXPECT_TRUE((false == !meta._consolidationPolicy.policy()));
  EXPECT_TRUE((.11f == meta._consolidationPolicy.properties().get("threshold").getNumber<float>()));
  EXPECT_TRUE(std::string("C") == iresearch::locale_utils::name(meta._locale));
  EXPECT_TRUE((9 == meta._version));
  EXPECT_TRUE((10 == meta._writebufferActive));
  EXPECT_TRUE((11 == meta._writebufferIdle));
  EXPECT_TRUE((12 == meta._writebufferSizeMax));
  EXPECT_EQ(irs::type<irs::compression::none>::id(),
            meta._primarySortCompression);
  {
    EXPECT_TRUE(4 == meta._primarySort.size());
    {
      auto& field = meta._primarySort.field(0);
      EXPECT_TRUE(2 == field.size());
      EXPECT_TRUE("nested" == field[0].name);
      EXPECT_TRUE(false == field[0].shouldExpand);
      EXPECT_TRUE("field" == field[1].name);
      EXPECT_TRUE(false == field[1].shouldExpand);
      EXPECT_TRUE(false == meta._primarySort.direction(0));
    }

    {
      auto& field = meta._primarySort.field(1);
      EXPECT_TRUE(3 == field.size());
      EXPECT_TRUE("another" == field[0].name);
      EXPECT_TRUE(false == field[0].shouldExpand);
      EXPECT_TRUE("nested" == field[1].name);
      EXPECT_TRUE(false == field[1].shouldExpand);
      EXPECT_TRUE("field" == field[2].name);
      EXPECT_TRUE(false == field[2].shouldExpand);
      EXPECT_TRUE(true == meta._primarySort.direction(1));
    }

    {
      auto& field = meta._primarySort.field(2);
      EXPECT_TRUE(1 == field.size());
      EXPECT_TRUE("field" == field[0].name);
      EXPECT_TRUE(false == field[0].shouldExpand);
      EXPECT_TRUE(false == meta._primarySort.direction(2));
    }

    {
      auto& field = meta._primarySort.field(3);
      EXPECT_TRUE(2 == field.size());
      EXPECT_TRUE("" == field[0].name);
      EXPECT_TRUE(false == field[0].shouldExpand);
      EXPECT_TRUE("field" == field[1].name);
      EXPECT_TRUE(false == field[1].shouldExpand);
      EXPECT_TRUE(true == meta._primarySort.direction(3));
    }
  }
}

TEST_F(IResearchViewMetaTest, test_writeDefaults) {
  arangodb::iresearch::IResearchViewMeta meta;
  arangodb::iresearch::IResearchViewMetaState metaState;
  arangodb::velocypack::Builder builder;
  arangodb::velocypack::Slice tmpSlice;
  arangodb::velocypack::Slice tmpSlice2;

  builder.openObject();
  EXPECT_TRUE((true == meta.json(builder)));
  EXPECT_TRUE((true == metaState.json(builder)));
  builder.close();

  auto slice = builder.slice();

  EXPECT_EQ(12, slice.length());
  tmpSlice = slice.get("collections");
  EXPECT_TRUE((true == tmpSlice.isArray() && 0 == tmpSlice.length()));
  tmpSlice = slice.get("cleanupIntervalStep");
  EXPECT_TRUE((true == tmpSlice.isNumber<size_t>() && 2 == tmpSlice.getNumber<size_t>()));
  tmpSlice = slice.get("commitIntervalMsec");
  EXPECT_TRUE((true == tmpSlice.isNumber<size_t>() && 1000 == tmpSlice.getNumber<size_t>()));
  tmpSlice = slice.get("consolidationIntervalMsec");
  EXPECT_TRUE((true == tmpSlice.isNumber<size_t>() && 10000 == tmpSlice.getNumber<size_t>()));
  tmpSlice = slice.get("consolidationPolicy");
  EXPECT_TRUE((true == tmpSlice.isObject() && 6 == tmpSlice.length()));
  tmpSlice2 = tmpSlice.get("type");
  EXPECT_TRUE((tmpSlice2.isString() && std::string("tier") == tmpSlice2.copyString()));
  tmpSlice2 = tmpSlice.get("segmentsMin");
  EXPECT_TRUE((tmpSlice2.isNumber() && 1 == tmpSlice2.getNumber<size_t>()));
  tmpSlice2 = tmpSlice.get("segmentsMax");
  EXPECT_TRUE((tmpSlice2.isNumber() && 10 == tmpSlice2.getNumber<size_t>()));
  tmpSlice2 = tmpSlice.get("segmentsBytesFloor");
  EXPECT_TRUE((tmpSlice2.isNumber() &&
               (size_t(2) * (1 << 20)) == tmpSlice2.getNumber<size_t>()));
  tmpSlice2 = tmpSlice.get("segmentsBytesMax");
  EXPECT_TRUE((tmpSlice2.isNumber() &&
               (size_t(5) * (1 << 30)) == tmpSlice2.getNumber<size_t>()));
  tmpSlice2 = tmpSlice.get("minScore");
  EXPECT_TRUE((tmpSlice2.isNumber() && (0. == tmpSlice2.getNumber<double>())));
  tmpSlice = slice.get("version");
  EXPECT_TRUE((true == tmpSlice.isNumber<uint32_t>() && 1 == tmpSlice.getNumber<uint32_t>()));
  tmpSlice = slice.get("writebufferActive");
  EXPECT_TRUE((true == tmpSlice.isNumber<size_t>() && 0 == tmpSlice.getNumber<size_t>()));
  tmpSlice = slice.get("writebufferIdle");
  EXPECT_TRUE((true == tmpSlice.isNumber<size_t>() && 64 == tmpSlice.getNumber<size_t>()));
  tmpSlice = slice.get("writebufferSizeMax");
  EXPECT_TRUE((true == tmpSlice.isNumber<size_t>() &&
               32 * (size_t(1) << 20) == tmpSlice.getNumber<size_t>()));
  tmpSlice = slice.get("primarySort");
  EXPECT_TRUE(true == tmpSlice.isArray());
  EXPECT_TRUE(0 == tmpSlice.length());
  tmpSlice = slice.get("primarySortCompression");
  EXPECT_TRUE(tmpSlice.isString());
  EXPECT_EQ(std::string("lz4"), tmpSlice.copyString());
  tmpSlice = slice.get("storedValues");
  EXPECT_TRUE(tmpSlice.isArray());
  EXPECT_EQ(0, tmpSlice.length());
}

TEST_F(IResearchViewMetaTest, test_writeCustomizedValues) {
  // test disabled consolidationPolicy
  {
    arangodb::iresearch::IResearchViewMeta meta;
    arangodb::iresearch::IResearchViewMetaState metaState;
    meta._commitIntervalMsec = 321;
    meta._consolidationIntervalMsec = 0;
    meta._consolidationPolicy = arangodb::iresearch::IResearchViewMeta::ConsolidationPolicy(
        irs::index_writer::consolidation_policy_t(),
        std::move(*arangodb::velocypack::Parser::fromJson(
            "{ \"type\": \"bytes_accum\", \"threshold\": 0.2 }")));

    arangodb::velocypack::Builder builder;
    arangodb::velocypack::Slice tmpSlice;
    arangodb::velocypack::Slice tmpSlice2;

    builder.openObject();
    EXPECT_TRUE((true == meta.json(builder)));
    EXPECT_TRUE((true == metaState.json(builder)));
    builder.close();

    auto slice = builder.slice();
    tmpSlice = slice.get("commitIntervalMsec");
    EXPECT_TRUE((tmpSlice.isNumber<size_t>() && 321 == tmpSlice.getNumber<size_t>()));
    tmpSlice = slice.get("consolidationIntervalMsec");
    EXPECT_TRUE((tmpSlice.isNumber<size_t>() && 0 == tmpSlice.getNumber<size_t>()));
    tmpSlice = slice.get("consolidationPolicy");
    EXPECT_TRUE((true == tmpSlice.isObject() && 2 == tmpSlice.length()));
    tmpSlice2 = tmpSlice.get("threshold");
    EXPECT_TRUE((tmpSlice2.isNumber<float>() && .2f == tmpSlice2.getNumber<float>()));
    tmpSlice2 = tmpSlice.get("type");
    EXPECT_TRUE((tmpSlice2.isString() &&
                 std::string("bytes_accum") == tmpSlice2.copyString()));
  }

  arangodb::iresearch::IResearchViewMeta meta;
  arangodb::iresearch::IResearchViewMetaState metaState;

  // test all parameters set to custom values
  metaState._collections.insert(arangodb::DataSourceId{42});
  metaState._collections.insert(arangodb::DataSourceId{52});
  metaState._collections.insert(arangodb::DataSourceId{62});
  meta._cleanupIntervalStep = 654;
  meta._commitIntervalMsec = 321;
  meta._consolidationIntervalMsec = 456;
  meta._consolidationPolicy = arangodb::iresearch::IResearchViewMeta::ConsolidationPolicy(
      irs::index_writer::consolidation_policy_t(),
      std::move(*arangodb::velocypack::Parser::fromJson(
          "{ \"type\": \"tier\", \"threshold\": 0.11 }")));
  meta._locale = iresearch::locale_utils::locale("en_UK.UTF-8");
  meta._version = 42;
  meta._writebufferActive = 10;
  meta._writebufferIdle = 11;
  meta._writebufferSizeMax = 12;
  meta._primarySort.emplace_back(
      std::vector<arangodb::basics::AttributeName>{
          arangodb::basics::AttributeName(VPackStringRef("nested")),
          arangodb::basics::AttributeName(VPackStringRef("field"))},
      true);
  meta._primarySort.emplace_back(
      std::vector<arangodb::basics::AttributeName>{
          arangodb::basics::AttributeName(VPackStringRef("another")),
          arangodb::basics::AttributeName(VPackStringRef("nested")),
          arangodb::basics::AttributeName(VPackStringRef("field"))},
      false);
  meta._primarySortCompression = irs::type<irs::compression::none>::id();
  auto storedValuesJSON = arangodb::velocypack::Parser::fromJson("[[], [\"\"], [\"\"], [\"test.t\"], {\"fields\":[\"a.a\", \"b.b\"], \"compression\":\"none\"}]");
  std::string error;
  meta._storedValues.fromVelocyPack(storedValuesJSON->slice(), error);
  EXPECT_TRUE(error.empty());

  std::unordered_set<arangodb::DataSourceId> expectedCollections = {
      arangodb::DataSourceId{42}, arangodb::DataSourceId{52}, arangodb::DataSourceId{62}};
  arangodb::velocypack::Builder builder;
  arangodb::velocypack::Slice tmpSlice;
  arangodb::velocypack::Slice tmpSlice2;

  builder.openObject();
  EXPECT_TRUE((true == meta.json(builder)));
  EXPECT_TRUE((true == metaState.json(builder)));
  builder.close();

  auto slice = builder.slice();

  EXPECT_EQ(12, slice.length());
  tmpSlice = slice.get("collections");
  EXPECT_TRUE((true == tmpSlice.isArray() && 3 == tmpSlice.length()));

  for (arangodb::velocypack::ArrayIterator itr(tmpSlice); itr.valid(); ++itr) {
    auto value = itr.value();
    EXPECT_TRUE((true == value.isUInt() &&
                 1 == expectedCollections.erase(arangodb::DataSourceId{value.getUInt()})));
  }

  EXPECT_TRUE(true == expectedCollections.empty());
  tmpSlice = slice.get("cleanupIntervalStep");
  EXPECT_TRUE((true == tmpSlice.isNumber<size_t>() && 654 == tmpSlice.getNumber<size_t>()));
  tmpSlice = slice.get("commitIntervalMsec");
  EXPECT_TRUE((true == tmpSlice.isNumber<size_t>() && 321 == tmpSlice.getNumber<size_t>()));
  tmpSlice = slice.get("consolidationIntervalMsec");
  EXPECT_TRUE((true == tmpSlice.isNumber<size_t>() && 456 == tmpSlice.getNumber<size_t>()));
  tmpSlice = slice.get("consolidationPolicy");
  EXPECT_TRUE((true == tmpSlice.isObject() && 2 == tmpSlice.length()));
  tmpSlice2 = tmpSlice.get("threshold");
  EXPECT_TRUE((tmpSlice2.isNumber<float>() && .11f == tmpSlice2.getNumber<float>()));
  tmpSlice2 = tmpSlice.get("type");
  EXPECT_TRUE((tmpSlice2.isString() && std::string("tier") == tmpSlice2.copyString()));
  tmpSlice = slice.get("version");
  EXPECT_TRUE((true == tmpSlice.isNumber<uint32_t>() &&
               42 == tmpSlice.getNumber<uint32_t>()));
  tmpSlice = slice.get("writebufferActive");
  EXPECT_TRUE((true == tmpSlice.isNumber<size_t>() && 10 == tmpSlice.getNumber<size_t>()));
  tmpSlice = slice.get("writebufferIdle");
  EXPECT_TRUE((true == tmpSlice.isNumber<size_t>() && 11 == tmpSlice.getNumber<size_t>()));
  tmpSlice = slice.get("writebufferSizeMax");
  EXPECT_TRUE((true == tmpSlice.isNumber<size_t>() && 12 == tmpSlice.getNumber<size_t>()));

  tmpSlice = slice.get("primarySortCompression");
  EXPECT_TRUE(tmpSlice.isString());
  EXPECT_EQ("none", tmpSlice.copyString());

  tmpSlice = slice.get("primarySort");
  EXPECT_TRUE(true == tmpSlice.isArray());
  EXPECT_TRUE(2 == tmpSlice.length());

  size_t i = 0;
  for (auto const sortSlice : arangodb::velocypack::ArrayIterator(tmpSlice)) {
    EXPECT_TRUE(sortSlice.isObject());
    auto const fieldSlice = sortSlice.get("field");
    EXPECT_TRUE(fieldSlice.isString());
    auto const directionSlice = sortSlice.get("asc");
    EXPECT_TRUE(directionSlice.isBoolean());

    std::string expectedName;
    arangodb::basics::TRI_AttributeNamesToString(meta._primarySort.field(i),
                                                 expectedName, false);
    EXPECT_TRUE(expectedName == arangodb::iresearch::getStringRef(fieldSlice));
    EXPECT_TRUE(meta._primarySort.direction(i) == directionSlice.getBoolean());
    ++i;
  }

  tmpSlice = slice.get("storedValues");
  EXPECT_TRUE(tmpSlice.isArray());
  EXPECT_EQ(2, tmpSlice.length());
  auto expectedStoredValue = arangodb::velocypack::Parser::fromJson(
    "[{\"fields\":[\"test.t\"], \"compression\":\"lz4\"}, {\"fields\":[\"a.a\", \"b.b\"], \"compression\":\"none\"}]");
  EXPECT_TRUE(arangodb::basics::VelocyPackHelper::equal(expectedStoredValue->slice(), tmpSlice, true));
}

TEST_F(IResearchViewMetaTest, test_readMaskAll) {
  auto viewJson = arangodb::velocypack::Parser::fromJson(
      "{ \"id\": 123, \"name\": \"testView\", \"type\": \"testType\" }");
  arangodb::iresearch::IResearchViewMeta meta;
  arangodb::iresearch::IResearchViewMetaState metaState;
  arangodb::iresearch::IResearchViewMeta::Mask mask;
  arangodb::iresearch::IResearchViewMetaState::Mask maskState;
  std::string errorField;

  auto json = arangodb::velocypack::Parser::fromJson(
      "{ \
    \"collections\": [ 42 ], \
    \"commitIntervalMsec\": 321, \
    \"consolidationIntervalMsec\": 654, \
    \"cleanupIntervalStep\": 456, \
    \"consolidationPolicy\": { \"type\": \"tier\", \"threshold\": 0.1 }, \
    \"locale\": \"ru_RU.KOI8-R\", \
    \"version\": 42, \
    \"writebufferActive\": 10, \
    \"writebufferIdle\": 11, \
    \"writebufferSizeMax\": 12, \
    \"primarySort\":[{\"field\":\"aaa\", \"direction\":\"asc\"}],\
    \"primarySortCompression\":\"none\",\
    \"storedValues\":[{\"fields\":[\"foo\"], \"compression\":\"lz4\"}]\
  }");
  EXPECT_TRUE(meta.init(json->slice(), errorField,
                                arangodb::iresearch::IResearchViewMeta::DEFAULT(), &mask));
  EXPECT_TRUE(metaState.init(json->slice(), errorField,
                                      arangodb::iresearch::IResearchViewMetaState::DEFAULT(),
                                      &maskState));
  EXPECT_TRUE(maskState._collections);
  EXPECT_TRUE(mask._commitIntervalMsec);
  EXPECT_TRUE(mask._consolidationIntervalMsec);
  EXPECT_TRUE(mask._cleanupIntervalStep);
  EXPECT_TRUE(mask._consolidationPolicy);
  EXPECT_FALSE(mask._locale); // intentionally disabled
  EXPECT_TRUE(mask._writebufferActive);
  EXPECT_TRUE(mask._writebufferIdle);
  EXPECT_TRUE(mask._writebufferSizeMax);
  EXPECT_TRUE(mask._primarySort);
  EXPECT_TRUE(mask._storedValues);
  EXPECT_TRUE(mask._primarySortCompression);
}

TEST_F(IResearchViewMetaTest, test_readMaskNone) {
  auto viewJson = arangodb::velocypack::Parser::fromJson(
      "{ \"id\": 123, \"name\": \"testView\", \"type\": \"testType\" }");
  arangodb::iresearch::IResearchViewMeta meta;
  arangodb::iresearch::IResearchViewMetaState metaState;
  arangodb::iresearch::IResearchViewMeta::Mask mask;
  arangodb::iresearch::IResearchViewMetaState::Mask maskState;
  std::string errorField;

  auto json = arangodb::velocypack::Parser::fromJson("{}");
  EXPECT_TRUE(meta.init(json->slice(), errorField,
                                arangodb::iresearch::IResearchViewMeta::DEFAULT(), &mask));
  EXPECT_TRUE(metaState.init(json->slice(), errorField,
                                      arangodb::iresearch::IResearchViewMetaState::DEFAULT(),
                                      &maskState));
  EXPECT_FALSE(maskState._collections);
  EXPECT_FALSE(mask._commitIntervalMsec);
  EXPECT_FALSE(mask._consolidationIntervalMsec);
  EXPECT_FALSE(mask._cleanupIntervalStep);
  EXPECT_FALSE(mask._consolidationPolicy);
  EXPECT_FALSE(mask._locale);
  EXPECT_FALSE(mask._writebufferActive);
  EXPECT_FALSE(mask._writebufferIdle);
  EXPECT_FALSE(mask._writebufferSizeMax);
  EXPECT_FALSE(mask._primarySort);
  EXPECT_FALSE(mask._storedValues);
  EXPECT_FALSE(mask._primarySortCompression);
}

TEST_F(IResearchViewMetaTest, test_writeMaskAll) {
  arangodb::iresearch::IResearchViewMeta meta;
  arangodb::iresearch::IResearchViewMetaState metaState;
  arangodb::iresearch::IResearchViewMeta::Mask mask(true);
  arangodb::iresearch::IResearchViewMetaState::Mask maskState(true);
  arangodb::velocypack::Builder builder;

  builder.openObject();
  EXPECT_TRUE(meta.json(builder, nullptr, &mask));
  EXPECT_TRUE(metaState.json(builder, nullptr, &maskState));
  builder.close();

  auto slice = builder.slice();

  EXPECT_EQ(12, slice.length());
  EXPECT_TRUE(slice.hasKey("collections"));
  EXPECT_TRUE(slice.hasKey("cleanupIntervalStep"));
  EXPECT_TRUE(slice.hasKey("commitIntervalMsec"));
  EXPECT_TRUE(slice.hasKey("consolidationIntervalMsec"));
  EXPECT_TRUE(slice.hasKey("consolidationPolicy"));
  EXPECT_FALSE(slice.hasKey("locale"));
  EXPECT_TRUE(slice.hasKey("version"));
  EXPECT_TRUE(slice.hasKey("writebufferActive"));
  EXPECT_TRUE(slice.hasKey("writebufferIdle"));
  EXPECT_TRUE(slice.hasKey("writebufferSizeMax"));
  EXPECT_TRUE(slice.hasKey("primarySort"));
  EXPECT_TRUE(slice.hasKey("storedValues"));
  EXPECT_TRUE(slice.hasKey("primarySortCompression"));
}

TEST_F(IResearchViewMetaTest, test_writeMaskNone) {
  arangodb::iresearch::IResearchViewMeta meta;
  arangodb::iresearch::IResearchViewMetaState metaState;
  arangodb::iresearch::IResearchViewMeta::Mask mask(false);
  arangodb::iresearch::IResearchViewMetaState::Mask maskState(false);
  arangodb::velocypack::Builder builder;

  builder.openObject();
  EXPECT_TRUE((true == meta.json(builder, nullptr, &mask)));
  EXPECT_TRUE((true == metaState.json(builder, nullptr, &maskState)));
  builder.close();

  auto slice = builder.slice();

  EXPECT_TRUE(0 == slice.length());
}
