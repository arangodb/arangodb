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

#include "../Mocks/StorageEngineMock.h"

#include "utils/locale_utils.hpp"

#include "IResearch/IResearchViewMeta.h"
#include "IResearch/VelocyPackHelper.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "velocypack/Iterator.h"
#include "velocypack/Parser.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

struct IResearchViewMetaSetup {
  StorageEngineMock engine;
  arangodb::application_features::ApplicationServer server;

  IResearchViewMetaSetup(): engine(server), server(nullptr, nullptr) {
    arangodb::EngineSelectorFeature::ENGINE = &engine;
  }

  ~IResearchViewMetaSetup() {
    arangodb::application_features::ApplicationServer::server = nullptr;
    arangodb::EngineSelectorFeature::ENGINE = nullptr;
  }
};

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief setup
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("IResearchViewMetaTest", "[iresearch][iresearch-viewmeta]") {
  IResearchViewMetaSetup s;
  UNUSED(s);
  typedef arangodb::iresearch::IResearchViewMeta::ConsolidationPolicy ConsolidationPolicy;

SECTION("test_defaults") {
  arangodb::iresearch::IResearchViewMeta meta;
  arangodb::iresearch::IResearchViewMetaState metaState;

  CHECK(true == metaState._collections.empty());
  CHECK(true == (10 == meta._cleanupIntervalStep));
  CHECK(true == (1000 == meta._commitIntervalMsec));
  CHECK(true == (60 * 1000 == meta._consolidationIntervalMsec));
  CHECK(std::string("tier") == meta._consolidationPolicy.properties().get("type").copyString());
  CHECK(false == !meta._consolidationPolicy.policy());
  CHECK(1 == meta._consolidationPolicy.properties().get("segmentsMin").getNumber<size_t>());
  CHECK(10 == meta._consolidationPolicy.properties().get("segmentsMax").getNumber<size_t>());
  CHECK(size_t(2)*(1<<20) == meta._consolidationPolicy.properties().get("segmentsBytesFloor").getNumber<size_t>());
  CHECK(size_t(5)*(1<<30) == meta._consolidationPolicy.properties().get("segmentsBytesMax").getNumber<size_t>());
  CHECK(std::string("C") == irs::locale_utils::name(meta._locale));
  CHECK(0 == meta._writebufferActive);
  CHECK(64 == meta._writebufferIdle);
  CHECK(32*(size_t(1)<<20) == meta._writebufferSizeMax);
  CHECK(meta._primarySort.empty());
}

SECTION("test_inheritDefaults") {
  auto viewJson = arangodb::velocypack::Parser::fromJson("{ \"id\": 123, \"name\": \"testView\", \"type\": \"testType\" }");
  arangodb::iresearch::IResearchViewMeta defaults;
  arangodb::iresearch::IResearchViewMetaState defaultsState;
  arangodb::iresearch::IResearchViewMeta meta;
  arangodb::iresearch::IResearchViewMetaState metaState;
  std::string tmpString;

  defaultsState._collections.insert(42);
  defaults._cleanupIntervalStep = 654;
  defaults._commitIntervalMsec = 321;
  defaults._consolidationIntervalMsec = 456;
  defaults._consolidationPolicy = ConsolidationPolicy(
    irs::index_writer::consolidation_policy_t(),
    std::move(*arangodb::velocypack::Parser::fromJson("{ \"type\": \"tier\", \"threshold\": 0.11 }"))
  );
  defaults._locale = irs::locale_utils::locale("C");
  defaults._writebufferActive = 10;
  defaults._writebufferIdle = 11;
  defaults._writebufferSizeMax = 12;
  defaults._primarySort.emplace_back(
    std::vector<arangodb::basics::AttributeName>{
      arangodb::basics::AttributeName(VPackStringRef("nested")),
      arangodb::basics::AttributeName(VPackStringRef("field"))
    }, true);
  defaults._primarySort.emplace_back(
    std::vector<arangodb::basics::AttributeName>{
      arangodb::basics::AttributeName(VPackStringRef("another")),
      arangodb::basics::AttributeName(VPackStringRef("nested")),
      arangodb::basics::AttributeName(VPackStringRef("field"))
    }, true);

  {
    auto json = arangodb::velocypack::Parser::fromJson("{}");
    CHECK(true == meta.init(json->slice(), tmpString, defaults));
    CHECK((true == metaState.init(json->slice(), tmpString, defaultsState)));
    CHECK((1 == metaState._collections.size()));
    CHECK((42 == *(metaState._collections.begin())));
    CHECK(654 == meta._cleanupIntervalStep);
    CHECK((321 == meta._commitIntervalMsec));
    CHECK(456 == meta._consolidationIntervalMsec);
    CHECK((std::string("tier") == meta._consolidationPolicy.properties().get("type").copyString()));
    CHECK((true == !meta._consolidationPolicy.policy()));
    CHECK((.11f == meta._consolidationPolicy.properties().get("threshold").getNumber<float>()));
    CHECK(std::string("C") == irs::locale_utils::name(meta._locale));
    CHECK((10 == meta._writebufferActive));
    CHECK((11 == meta._writebufferIdle));
    CHECK((12 == meta._writebufferSizeMax));
    CHECK(meta._primarySort == defaults._primarySort);
  }
}

SECTION("test_readDefaults") {
  arangodb::iresearch::IResearchViewMeta meta;
  arangodb::iresearch::IResearchViewMetaState metaState;
  std::string tmpString;

  {
    auto viewJson = arangodb::velocypack::Parser::fromJson("{ \"id\": 123, \"name\": \"testView\", \"type\": \"testType\" }");
    auto json = arangodb::velocypack::Parser::fromJson("{}");
    CHECK(true == meta.init(json->slice(), tmpString));
    CHECK((true == metaState.init(json->slice(), tmpString)));
    CHECK((true == metaState._collections.empty()));
    CHECK(10 == meta._cleanupIntervalStep);
    CHECK((1000 == meta._commitIntervalMsec));
    CHECK(60 * 1000 == meta._consolidationIntervalMsec);
    CHECK((std::string("tier") == meta._consolidationPolicy.properties().get("type").copyString()));
    CHECK((false == !meta._consolidationPolicy.policy()));
    CHECK(1 == meta._consolidationPolicy.properties().get("segmentsMin").getNumber<size_t>());
    CHECK(10 == meta._consolidationPolicy.properties().get("segmentsMax").getNumber<size_t>());
    CHECK(size_t(2)*(1<<20) == meta._consolidationPolicy.properties().get("segmentsBytesFloor").getNumber<size_t>());
    CHECK(size_t(5)*(1<<30) == meta._consolidationPolicy.properties().get("segmentsBytesMax").getNumber<size_t>());
    CHECK(std::string("C") == irs::locale_utils::name(meta._locale));
    CHECK((0 == meta._writebufferActive));
    CHECK((64 == meta._writebufferIdle));
    CHECK((32*(size_t(1)<<20) == meta._writebufferSizeMax));
    CHECK(meta._primarySort.empty());
  }
}

SECTION("test_readCustomizedValues") {
  std::unordered_set<TRI_voc_cid_t> expectedCollections = { 42 };
  arangodb::iresearch::IResearchViewMeta meta;
  arangodb::iresearch::IResearchViewMetaState metaState;

  // .............................................................................
  // test invalid value
  // .............................................................................

  {
    std::string errorField;
    auto json = arangodb::velocypack::Parser::fromJson("{ \"collections\": \"invalid\" }");
    CHECK((true == meta.init(json->slice(), errorField)));
    CHECK((false == metaState.init(json->slice(), errorField)));
    CHECK(std::string("collections") == errorField);
  }

  {
    std::string errorField;
    auto json = arangodb::velocypack::Parser::fromJson("{ \"commitIntervalMsec\": 0.5 }");
    CHECK((true == metaState.init(json->slice(), errorField)));
    CHECK(false == meta.init(json->slice(), errorField));
    CHECK(std::string("commitIntervalMsec") == errorField);
  }

  {
    std::string errorField;
    auto json = arangodb::velocypack::Parser::fromJson("{ \"consolidationIntervalMsec\": 0.5 }");
    CHECK((true == metaState.init(json->slice(), errorField)));
    CHECK(false == meta.init(json->slice(), errorField));
    CHECK(std::string("consolidationIntervalMsec") == errorField);
  }

  {
    std::string errorField;
    auto json = arangodb::velocypack::Parser::fromJson("{ \"cleanupIntervalStep\": 0.5 }");
    CHECK((true == metaState.init(json->slice(), errorField)));
    CHECK(false == meta.init(json->slice(), errorField));
    CHECK(std::string("cleanupIntervalStep") == errorField);
  }

  {
    std::string errorField;
    auto json = arangodb::velocypack::Parser::fromJson("{ \"consolidationPolicy\": \"invalid\" }");
    CHECK((true == metaState.init(json->slice(), errorField)));
    CHECK(false == meta.init(json->slice(), errorField));
    CHECK(std::string("consolidationPolicy") == errorField);
  }

  {
    std::string errorField;
    auto json = arangodb::velocypack::Parser::fromJson("{ \"consolidationPolicy\": null }");
    CHECK(false == meta.init(json->slice(), errorField));
    CHECK((std::string("consolidationPolicy") == errorField));
  }

  // consolidation policy "bytes_accum"

  {
    std::string errorField;
    auto json = arangodb::velocypack::Parser::fromJson("{ \"consolidationPolicy\": { \"type\": \"bytes_accum\", \"threshold\": -0.5 } }");
    CHECK((true == metaState.init(json->slice(), errorField)));
    CHECK(false == meta.init(json->slice(), errorField));
    CHECK((std::string("consolidationPolicy=>threshold") == errorField));
  }

  {
    std::string errorField;
    auto json = arangodb::velocypack::Parser::fromJson("{ \"consolidationPolicy\": { \"type\": \"bytes_accum\", \"threshold\": 1.5 } }");
    CHECK((true == metaState.init(json->slice(), errorField)));
    CHECK(false == meta.init(json->slice(), errorField));
    CHECK((std::string("consolidationPolicy=>threshold") == errorField));
  }

  // consolidation policy "tier"

  {
    std::string errorField;
    auto json = arangodb::velocypack::Parser::fromJson("{ \"consolidationPolicy\": { \"type\": \"tier\", \"minScore\": -0.5 } }");
    CHECK((true == metaState.init(json->slice(), errorField)));
    CHECK(false == meta.init(json->slice(), errorField));
    CHECK((std::string("consolidationPolicy=>minScore") == errorField));
  }

  {
    std::string errorField;
    auto json = arangodb::velocypack::Parser::fromJson("{ \"consolidationPolicy\": { \"type\": \"tier\", \"segmentsMin\": -1  } }");
    CHECK((true == metaState.init(json->slice(), errorField)));
    CHECK(false == meta.init(json->slice(), errorField));
    CHECK((std::string("consolidationPolicy=>segmentsMin") == errorField));
  }

  {
    std::string errorField;
    auto json = arangodb::velocypack::Parser::fromJson("{ \"consolidationPolicy\": { \"type\": \"tier\", \"segmentsMax\": -1  } }");
    CHECK((true == metaState.init(json->slice(), errorField)));
    CHECK(false == meta.init(json->slice(), errorField));
    CHECK((std::string("consolidationPolicy=>segmentsMax") == errorField));
  }

  {
    std::string errorField;
    auto json = arangodb::velocypack::Parser::fromJson("{ \"consolidationPolicy\": { \"type\": \"tier\", \"segmentsBytesFloor\": -1  } }");
    CHECK((true == metaState.init(json->slice(), errorField)));
    CHECK(false == meta.init(json->slice(), errorField));
    CHECK((std::string("consolidationPolicy=>segmentsBytesFloor") == errorField));
  }

  {
    std::string errorField;
    auto json = arangodb::velocypack::Parser::fromJson("{ \"consolidationPolicy\": { \"type\": \"tier\", \"segmentsBytesMax\": -1  } }");
    CHECK((true == metaState.init(json->slice(), errorField)));
    CHECK(false == meta.init(json->slice(), errorField));
    CHECK((std::string("consolidationPolicy=>segmentsBytesMax") == errorField));
  }

  {
    std::string errorField;
    auto json = arangodb::velocypack::Parser::fromJson("{ \"consolidationPolicy\": { \"type\": \"invalid\" } }");
    CHECK((true == metaState.init(json->slice(), errorField)));
    CHECK(false == meta.init(json->slice(), errorField));
    CHECK((std::string("consolidationPolicy=>type") == errorField));
  }

  {
    std::string errorField;
    auto json = arangodb::velocypack::Parser::fromJson("{ \"version\": -0.5 }");
    CHECK((true == metaState.init(json->slice(), errorField)));
    CHECK(false == meta.init(json->slice(), errorField));
    CHECK((std::string("version") == errorField));
  }

  {
    std::string errorField;
    auto json = arangodb::velocypack::Parser::fromJson("{ \"version\": 1.5 }");
    CHECK((true == metaState.init(json->slice(), errorField)));
    CHECK(false == meta.init(json->slice(), errorField));
    CHECK((std::string("version") == errorField));
  }

  {
    std::string errorField;
    auto json = arangodb::velocypack::Parser::fromJson("{ \"primarySort\": {} }");
    CHECK((true == metaState.init(json->slice(), errorField)));
    CHECK(false == meta.init(json->slice(), errorField));
    CHECK("primarySort" == errorField);
  }

  {
    std::string errorField;
    auto json = arangodb::velocypack::Parser::fromJson("{ \"primarySort\": [ 1 ] }");
    CHECK((true == metaState.init(json->slice(), errorField)));
    CHECK(false == meta.init(json->slice(), errorField));
    CHECK("primarySort=>[0]" == errorField);
  }

  {
    std::string errorField;
    auto json = arangodb::velocypack::Parser::fromJson("{ \"primarySort\": [ { \"field\":{ }, \"direction\":\"aSc\" } ] }");
    CHECK((true == metaState.init(json->slice(), errorField)));
    CHECK(false == meta.init(json->slice(), errorField));
    CHECK("primarySort=>[0]=>field" == errorField);
  }

  {
    std::string errorField;
    auto json = arangodb::velocypack::Parser::fromJson("{ \"primarySort\": [ { \"field\":{ }, \"asc\":true } ] }");
    CHECK((true == metaState.init(json->slice(), errorField)));
    CHECK(false == meta.init(json->slice(), errorField));
    CHECK("primarySort=>[0]=>field" == errorField);
  }

  {
    std::string errorField;
    auto json = arangodb::velocypack::Parser::fromJson("{ \"primarySort\": [ { \"field\":\"nested.field\", \"direction\":\"xxx\" }, 4 ] }");
    CHECK((true == metaState.init(json->slice(), errorField)));
    CHECK(false == meta.init(json->slice(), errorField));
    CHECK("primarySort=>[0]=>direction" == errorField);
  }

  {
    std::string errorField;
    auto json = arangodb::velocypack::Parser::fromJson("{ \"primarySort\": [ { \"field\":\"nested.field\", \"asc\":\"xxx\" }, 4 ] }");
    CHECK((true == metaState.init(json->slice(), errorField)));
    CHECK(false == meta.init(json->slice(), errorField));
    CHECK("primarySort=>[0]=>asc" == errorField);
  }

  {
    std::string errorField;
    auto json = arangodb::velocypack::Parser::fromJson("{ \"primarySort\": [ { \"field\":\"nested.field\", \"direction\":\"aSc\" }, 4 ] }");
    CHECK((true == metaState.init(json->slice(), errorField)));
    CHECK(false == meta.init(json->slice(), errorField));
    CHECK("primarySort=>[1]" == errorField);
  }

  {
    std::string errorField;
    auto json = arangodb::velocypack::Parser::fromJson("{ \"primarySort\": [ { \"field\":\"nested.field\", \"asc\": true }, { \"field\":1, \"direction\":\"aSc\" } ] }");
    CHECK((true == metaState.init(json->slice(), errorField)));
    CHECK(false == meta.init(json->slice(), errorField));
    CHECK("primarySort=>[1]=>field" == errorField);
  }

  // .............................................................................
  // test valid value
  // .............................................................................

  // test all parameters set to custom values
  std::string errorField;
  auto json = arangodb::velocypack::Parser::fromJson("{ \
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
        \"primarySort\" : [ \
          { \"field\": \"nested.field\", \"direction\": \"desc\" }, \
          { \"field\": \"another.nested.field\", \"direction\": \"asc\" }, \
          { \"field\": \"field\", \"asc\": false }, \
          { \"field\": \".field\", \"asc\": true } \
        ] \
    }");
  CHECK(true == meta.init(json->slice(), errorField));
  CHECK((true == metaState.init(json->slice(), errorField)));
  CHECK((1 == metaState._collections.size()));

  for (auto& collection: metaState._collections) {
    CHECK(1 == expectedCollections.erase(collection));
  }

  CHECK(true == expectedCollections.empty());
  CHECK((42 == *(metaState._collections.begin())));
  CHECK(654 == meta._cleanupIntervalStep);
  CHECK((321 == meta._commitIntervalMsec));
  CHECK(456 == meta._consolidationIntervalMsec);
  CHECK((std::string("bytes_accum") == meta._consolidationPolicy.properties().get("type").copyString()));
  CHECK((false == !meta._consolidationPolicy.policy()));
  CHECK((.11f == meta._consolidationPolicy.properties().get("threshold").getNumber<float>()));
  CHECK(std::string("C") == iresearch::locale_utils::name(meta._locale));
  CHECK((9 == meta._version));
  CHECK((10 == meta._writebufferActive));
  CHECK((11 == meta._writebufferIdle));
  CHECK((12 == meta._writebufferSizeMax));

  {
    CHECK(4 == meta._primarySort.size());
    {
      auto& field = meta._primarySort.field(0);
      CHECK(2 == field.size());
      CHECK("nested" == field[0].name);
      CHECK(false == field[0].shouldExpand);
      CHECK("field" == field[1].name);
      CHECK(false == field[1].shouldExpand);
      CHECK(false == meta._primarySort.direction(0));
    }

    {
      auto& field = meta._primarySort.field(1);
      CHECK(3 == field.size());
      CHECK("another" == field[0].name);
      CHECK(false == field[0].shouldExpand);
      CHECK("nested" == field[1].name);
      CHECK(false == field[1].shouldExpand);
      CHECK("field" == field[2].name);
      CHECK(false == field[2].shouldExpand);
      CHECK(true == meta._primarySort.direction(1));
    }

    {
      auto& field = meta._primarySort.field(2);
      CHECK(1 == field.size());
      CHECK("field" == field[0].name);
      CHECK(false == field[0].shouldExpand);
      CHECK(false == meta._primarySort.direction(2));
    }

    {
      auto& field = meta._primarySort.field(3);
      CHECK(2 == field.size());
      CHECK("" == field[0].name);
      CHECK(false == field[0].shouldExpand);
      CHECK("field" == field[1].name);
      CHECK(false == field[1].shouldExpand);
      CHECK(true == meta._primarySort.direction(3));
    }
  }
}

SECTION("test_writeDefaults") {
  arangodb::iresearch::IResearchViewMeta meta;
  arangodb::iresearch::IResearchViewMetaState metaState;
  arangodb::velocypack::Builder builder;
  arangodb::velocypack::Slice tmpSlice;
  arangodb::velocypack::Slice tmpSlice2;

  builder.openObject();
  CHECK((true == meta.json(builder)));
  CHECK((true == metaState.json(builder)));
  builder.close();

  auto slice = builder.slice();

  CHECK((10U == slice.length()));
  tmpSlice = slice.get("collections");
  CHECK((true == tmpSlice.isArray() && 0 == tmpSlice.length()));
  tmpSlice = slice.get("cleanupIntervalStep");
  CHECK((true == tmpSlice.isNumber<size_t>() && 10 == tmpSlice.getNumber<size_t>()));
  tmpSlice = slice.get("commitIntervalMsec");
  CHECK((true == tmpSlice.isNumber<size_t>() && 1000 == tmpSlice.getNumber<size_t>()));
  tmpSlice = slice.get("consolidationIntervalMsec");
  CHECK((true == tmpSlice.isNumber<size_t>() && 60000 == tmpSlice.getNumber<size_t>()));
  tmpSlice = slice.get("consolidationPolicy");
  CHECK((true == tmpSlice.isObject() && 6 == tmpSlice.length()));
  tmpSlice2 = tmpSlice.get("type");
  CHECK((tmpSlice2.isString() && std::string("tier") == tmpSlice2.copyString()));
  tmpSlice2 = tmpSlice.get("segmentsMin");
  CHECK((tmpSlice2.isNumber() && 1 == tmpSlice2.getNumber<size_t>()));
  tmpSlice2 = tmpSlice.get("segmentsMax");
  CHECK((tmpSlice2.isNumber() && 10 == tmpSlice2.getNumber<size_t>()));
  tmpSlice2 = tmpSlice.get("segmentsBytesFloor");
  CHECK((tmpSlice2.isNumber() && (size_t(2)*(1<<20)) == tmpSlice2.getNumber<size_t>()));
  tmpSlice2 = tmpSlice.get("segmentsBytesMax");
  CHECK((tmpSlice2.isNumber() && (size_t(5)*(1<<30)) == tmpSlice2.getNumber<size_t>()));
  tmpSlice2 = tmpSlice.get("minScore");
  CHECK((tmpSlice2.isNumber() && (0. == tmpSlice2.getNumber<double>())));
  tmpSlice = slice.get("version");
  CHECK((true == tmpSlice.isNumber<uint32_t>() && 1 == tmpSlice.getNumber<uint32_t>()));
  tmpSlice = slice.get("writebufferActive");
  CHECK((true == tmpSlice.isNumber<size_t>() && 0 == tmpSlice.getNumber<size_t>()));
  tmpSlice = slice.get("writebufferIdle");
  CHECK((true == tmpSlice.isNumber<size_t>() && 64 == tmpSlice.getNumber<size_t>()));
  tmpSlice = slice.get("writebufferSizeMax");
  CHECK((true == tmpSlice.isNumber<size_t>() && 32*(size_t(1)<<20) == tmpSlice.getNumber<size_t>()));
  tmpSlice = slice.get("primarySort");
  CHECK(true == tmpSlice.isArray());
  CHECK(0 == tmpSlice.length());
}

SECTION("test_writeCustomizedValues") {
  // test disabled consolidationPolicy
  {
    arangodb::iresearch::IResearchViewMeta meta;
    arangodb::iresearch::IResearchViewMetaState metaState;
    meta._commitIntervalMsec = 321;
    meta._consolidationIntervalMsec = 0;
    meta._consolidationPolicy = ConsolidationPolicy(
      irs::index_writer::consolidation_policy_t(),
      std::move(*arangodb::velocypack::Parser::fromJson("{ \"type\": \"bytes_accum\", \"threshold\": 0.2 }"))
    );

    arangodb::velocypack::Builder builder;
    arangodb::velocypack::Slice tmpSlice;
    arangodb::velocypack::Slice tmpSlice2;

    builder.openObject();
    CHECK((true == meta.json(builder)));
    CHECK((true == metaState.json(builder)));
    builder.close();

    auto slice = builder.slice();
    tmpSlice = slice.get("commitIntervalMsec");
    CHECK((tmpSlice.isNumber<size_t>() && 321 == tmpSlice.getNumber<size_t>()));
    tmpSlice = slice.get("consolidationIntervalMsec");
    CHECK((tmpSlice.isNumber<size_t>() && 0 == tmpSlice.getNumber<size_t>()));
    tmpSlice = slice.get("consolidationPolicy");
    CHECK((true == tmpSlice.isObject() && 2 == tmpSlice.length()));
    tmpSlice2 = tmpSlice.get("threshold");
    CHECK((tmpSlice2.isNumber<float>() && .2f == tmpSlice2.getNumber<float>()));
    tmpSlice2 = tmpSlice.get("type");
    CHECK((tmpSlice2.isString() && std::string("bytes_accum") == tmpSlice2.copyString()));
  }

  arangodb::iresearch::IResearchViewMeta meta;
  arangodb::iresearch::IResearchViewMetaState metaState;

  // test all parameters set to custom values
  metaState._collections.insert(42);
  metaState._collections.insert(52);
  metaState._collections.insert(62);
  meta._cleanupIntervalStep = 654;
  meta._commitIntervalMsec = 321;
  meta._consolidationIntervalMsec = 456;
  meta._consolidationPolicy = ConsolidationPolicy(
    irs::index_writer::consolidation_policy_t(),
    std::move(*arangodb::velocypack::Parser::fromJson("{ \"type\": \"tier\", \"threshold\": 0.11 }"))
  );
  meta._locale = iresearch::locale_utils::locale("en_UK.UTF-8");
  meta._version = 42;
  meta._writebufferActive = 10;
  meta._writebufferIdle = 11;
  meta._writebufferSizeMax = 12;
  meta._primarySort.emplace_back(
    std::vector<arangodb::basics::AttributeName>{
      arangodb::basics::AttributeName(VPackStringRef("nested")),
      arangodb::basics::AttributeName(VPackStringRef("field"))
    }, true);
  meta._primarySort.emplace_back(
    std::vector<arangodb::basics::AttributeName>{
      arangodb::basics::AttributeName(VPackStringRef("another")),
      arangodb::basics::AttributeName(VPackStringRef("nested")),
      arangodb::basics::AttributeName(VPackStringRef("field"))
    }, false);

  std::unordered_set<TRI_voc_cid_t> expectedCollections = { 42, 52, 62 };
  arangodb::velocypack::Builder builder;
  arangodb::velocypack::Slice tmpSlice;
  arangodb::velocypack::Slice tmpSlice2;

  builder.openObject();
  CHECK((true == meta.json(builder)));
  CHECK((true == metaState.json(builder)));
  builder.close();

  auto slice = builder.slice();

  CHECK((10U == slice.length()));
  tmpSlice = slice.get("collections");
  CHECK((true == tmpSlice.isArray() && 3 == tmpSlice.length()));

  for (arangodb::velocypack::ArrayIterator itr(tmpSlice); itr.valid(); ++itr) {
    auto value = itr.value();
    CHECK((true == value.isUInt() && 1 == expectedCollections.erase(value.getUInt())));
  }

  CHECK(true == expectedCollections.empty());
  tmpSlice = slice.get("cleanupIntervalStep");
  CHECK((true == tmpSlice.isNumber<size_t>() && 654 == tmpSlice.getNumber<size_t>()));
  tmpSlice = slice.get("commitIntervalMsec");
  CHECK((true == tmpSlice.isNumber<size_t>() && 321 == tmpSlice.getNumber<size_t>()));
  tmpSlice = slice.get("consolidationIntervalMsec");
  CHECK((true == tmpSlice.isNumber<size_t>() && 456 == tmpSlice.getNumber<size_t>()));
  tmpSlice = slice.get("consolidationPolicy");
  CHECK((true == tmpSlice.isObject() && 2 == tmpSlice.length()));
  tmpSlice2 = tmpSlice.get("threshold");
  CHECK((tmpSlice2.isNumber<float>() && .11f == tmpSlice2.getNumber<float>()));
  tmpSlice2 = tmpSlice.get("type");
  CHECK((tmpSlice2.isString() && std::string("tier") == tmpSlice2.copyString()));
  tmpSlice = slice.get("version");
  CHECK((true == tmpSlice.isNumber<uint32_t>() && 42 == tmpSlice.getNumber<uint32_t>()));
  tmpSlice = slice.get("writebufferActive");
  CHECK((true == tmpSlice.isNumber<size_t>() && 10 == tmpSlice.getNumber<size_t>()));
  tmpSlice = slice.get("writebufferIdle");
  CHECK((true == tmpSlice.isNumber<size_t>() && 11 == tmpSlice.getNumber<size_t>()));
  tmpSlice = slice.get("writebufferSizeMax");
  CHECK((true == tmpSlice.isNumber<size_t>() && 12 == tmpSlice.getNumber<size_t>()));

  tmpSlice = slice.get("primarySort");
  CHECK(true == tmpSlice.isArray());
  CHECK(2 == tmpSlice.length());

  size_t i = 0;
  for (auto const sortSlice : arangodb::velocypack::ArrayIterator(tmpSlice)) {
    CHECK(sortSlice.isObject());
    auto const fieldSlice = sortSlice.get("field");
    CHECK(fieldSlice.isString());
    auto const directionSlice = sortSlice.get("asc");
    CHECK(directionSlice.isBoolean());

    std::string expectedName;
    arangodb::basics::TRI_AttributeNamesToString(meta._primarySort.field(i), expectedName, false);
    CHECK(expectedName == arangodb::iresearch::getStringRef(fieldSlice));
    CHECK(meta._primarySort.direction(i) == directionSlice.getBoolean());
    ++i;
  }
}

SECTION("test_readMaskAll") {
  auto viewJson = arangodb::velocypack::Parser::fromJson("{ \"id\": 123, \"name\": \"testView\", \"type\": \"testType\" }");
  arangodb::iresearch::IResearchViewMeta meta;
  arangodb::iresearch::IResearchViewMetaState metaState;
  arangodb::iresearch::IResearchViewMeta::Mask mask;
  arangodb::iresearch::IResearchViewMetaState::Mask maskState;
  std::string errorField;

  auto json = arangodb::velocypack::Parser::fromJson("{ \
    \"collections\": [ 42 ], \
    \"commitIntervalMsec\": 321, \
    \"consolidationIntervalMsec\": 654, \
    \"cleanupIntervalStep\": 456, \
    \"consolidationPolicy\": { \"type\": \"tier\", \"threshold\": 0.1 }, \
    \"locale\": \"ru_RU.KOI8-R\", \
    \"version\": 42, \
    \"writebufferActive\": 10, \
    \"writebufferIdle\": 11, \
    \"writebufferSizeMax\": 12 \
  }");
  CHECK(true == meta.init(json->slice(), errorField, arangodb::iresearch::IResearchViewMeta::DEFAULT(), &mask));
  CHECK((true == metaState.init(json->slice(), errorField, arangodb::iresearch::IResearchViewMetaState::DEFAULT(), &maskState)));
  CHECK((true == maskState._collections));
  CHECK((true == mask._commitIntervalMsec));
  CHECK(true == mask._consolidationIntervalMsec);
  CHECK(true == mask._cleanupIntervalStep);
  CHECK((true == mask._consolidationPolicy));
  CHECK((false == mask._locale));
  CHECK((true == mask._writebufferActive));
  CHECK((true == mask._writebufferIdle));
  CHECK((true == mask._writebufferSizeMax));
}

SECTION("test_readMaskNone") {
  auto viewJson = arangodb::velocypack::Parser::fromJson("{ \"id\": 123, \"name\": \"testView\", \"type\": \"testType\" }");
  arangodb::iresearch::IResearchViewMeta meta;
  arangodb::iresearch::IResearchViewMetaState metaState;
  arangodb::iresearch::IResearchViewMeta::Mask mask;
  arangodb::iresearch::IResearchViewMetaState::Mask maskState;
  std::string errorField;

  auto json = arangodb::velocypack::Parser::fromJson("{}");
  CHECK(true == meta.init(json->slice(), errorField, arangodb::iresearch::IResearchViewMeta::DEFAULT(), &mask));
  CHECK((true == metaState.init(json->slice(), errorField, arangodb::iresearch::IResearchViewMetaState::DEFAULT(), &maskState)));
  CHECK((false == maskState._collections));
  CHECK((false == mask._commitIntervalMsec));
  CHECK(false == mask._consolidationIntervalMsec);
  CHECK(false == mask._cleanupIntervalStep);
  CHECK((false == mask._consolidationPolicy));
  CHECK(false == mask._locale);
  CHECK((false == mask._writebufferActive));
  CHECK((false == mask._writebufferIdle));
  CHECK((false == mask._writebufferSizeMax));
}

SECTION("test_writeMaskAll") {
  arangodb::iresearch::IResearchViewMeta meta;
  arangodb::iresearch::IResearchViewMetaState metaState;
  arangodb::iresearch::IResearchViewMeta::Mask mask(true);
  arangodb::iresearch::IResearchViewMetaState::Mask maskState(true);
  arangodb::velocypack::Builder builder;
  arangodb::velocypack::Slice tmpSlice;

  builder.openObject();
  CHECK((true == meta.json(builder, nullptr, &mask)));
  CHECK((true == metaState.json(builder, nullptr, &maskState)));
  builder.close();

  auto slice = builder.slice();

  CHECK((10U == slice.length()));
  CHECK(true == slice.hasKey("collections"));
  CHECK(true == slice.hasKey("cleanupIntervalStep"));
  CHECK((true == slice.hasKey("commitIntervalMsec")));
  CHECK(true == slice.hasKey("consolidationIntervalMsec"));
  CHECK(true == slice.hasKey("consolidationPolicy"));
  CHECK((false == slice.hasKey("locale")));
  CHECK((true == slice.hasKey("version")));
  CHECK((true == slice.hasKey("writebufferActive")));
  CHECK((true == slice.hasKey("writebufferIdle")));
  CHECK((true == slice.hasKey("writebufferSizeMax")));
  CHECK((true == slice.hasKey("primarySort")));
}

SECTION("test_writeMaskNone") {
  arangodb::iresearch::IResearchViewMeta meta;
  arangodb::iresearch::IResearchViewMetaState metaState;
  arangodb::iresearch::IResearchViewMeta::Mask mask(false);
  arangodb::iresearch::IResearchViewMetaState::Mask maskState(false);
  arangodb::velocypack::Builder builder;

  builder.openObject();
  CHECK((true == meta.json(builder, nullptr, &mask)));
  CHECK((true == metaState.json(builder, nullptr, &maskState)));
  builder.close();

  auto slice = builder.slice();

  CHECK(0 == slice.length());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate tests
////////////////////////////////////////////////////////////////////////////////

}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
