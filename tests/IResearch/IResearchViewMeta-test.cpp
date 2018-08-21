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

#include "StorageEngineMock.h"

#include "utils/locale_utils.hpp"

#include "IResearch/IResearchViewMeta.h"
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

  CHECK((true == metaState._collections.empty()));
  CHECK(true == (10 == meta._cleanupIntervalStep));
  CHECK(true == (60 * 1000 == meta._commitIntervalMsec));
  CHECK((ConsolidationPolicy::Type::BYTES_ACCUM == meta._consolidationPolicy.type()));
  CHECK((300 == meta._consolidationPolicy.segmentThreshold()));
  CHECK((false == !meta._consolidationPolicy.policy()));
  CHECK((0.85f == meta._consolidationPolicy.threshold()));
  CHECK(std::string("C") == irs::locale_utils::name(meta._locale));
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
  defaults._commitIntervalMsec = 456;
  defaults._consolidationPolicy = ConsolidationPolicy(ConsolidationPolicy::Type::BYTES, 101, .11f);
  defaults._locale = irs::locale_utils::locale("ru");

  {
    auto json = arangodb::velocypack::Parser::fromJson("{}");
    CHECK(true == meta.init(json->slice(), tmpString, defaults));
    CHECK((true == metaState.init(json->slice(), tmpString, defaultsState)));
    CHECK((1 == metaState._collections.size()));
    CHECK((42 == *(metaState._collections.begin())));
    CHECK(654 == meta._cleanupIntervalStep);
    CHECK(456 == meta._commitIntervalMsec);
    CHECK((ConsolidationPolicy::Type::BYTES == meta._consolidationPolicy.type()));
    CHECK((101 == meta._consolidationPolicy.segmentThreshold()));
    CHECK((false == !meta._consolidationPolicy.policy()));
    CHECK((.11f == meta._consolidationPolicy.threshold()));
    CHECK(std::string("ru") == irs::locale_utils::name(meta._locale));
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
    CHECK(60 * 1000 == meta._commitIntervalMsec);
    CHECK((ConsolidationPolicy::Type::BYTES_ACCUM == meta._consolidationPolicy.type()));
    CHECK((300 == meta._consolidationPolicy.segmentThreshold()));
    CHECK((false == !meta._consolidationPolicy.policy()));
    CHECK((0.85f == meta._consolidationPolicy.threshold()));
    CHECK(std::string("C") == irs::locale_utils::name(meta._locale));
  }
}

SECTION("test_readCustomizedValues") {
  auto viewJson = arangodb::velocypack::Parser::fromJson("{ \"id\": 123, \"name\": \"testView\", \"type\": \"testType\" }");
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
    auto json = arangodb::velocypack::Parser::fromJson("{ \"cleanupIntervalStep\": 0.5 }");
    CHECK((true == metaState.init(json->slice(), errorField)));
    CHECK(false == meta.init(json->slice(), errorField));
    CHECK(std::string("cleanupIntervalStep") == errorField);
  }

  {
    std::string errorField;
    auto json = arangodb::velocypack::Parser::fromJson("{ \"consolidate\": \"invalid\" }");
    CHECK((true == metaState.init(json->slice(), errorField)));
    CHECK(false == meta.init(json->slice(), errorField));
    CHECK(std::string("consolidate") == errorField);
  }

  {
    std::string errorField;
    auto json = arangodb::velocypack::Parser::fromJson("{ \"consolidate\": { \"segmentThreshold\": 0.5 } }");
    CHECK(false == meta.init(json->slice(), errorField));
    CHECK((std::string("consolidate=>segmentThreshold") == errorField));
  }

  {
    std::string errorField;
    auto json = arangodb::velocypack::Parser::fromJson("{ \"consolidate\": { \"threshold\": -0.5 } }");
    CHECK((true == metaState.init(json->slice(), errorField)));
    CHECK(false == meta.init(json->slice(), errorField));
    CHECK((std::string("consolidate=>threshold") == errorField));
  }

  {
    std::string errorField;
    auto json = arangodb::velocypack::Parser::fromJson("{ \"consolidate\": { \"threshold\": 1.5 } }");
    CHECK((true == metaState.init(json->slice(), errorField)));
    CHECK(false == meta.init(json->slice(), errorField));
    CHECK((std::string("consolidate=>threshold") == errorField));
  }

  {
    std::string errorField;
    auto json = arangodb::velocypack::Parser::fromJson("{ \"consolidate\": { \"type\": \"invalid\" } }");
    CHECK((true == metaState.init(json->slice(), errorField)));
    CHECK(false == meta.init(json->slice(), errorField));
    CHECK((std::string("consolidate=>type") == errorField));
  }

  // .............................................................................
  // test valid value
  // .............................................................................

  // test disabled consolidate
  {
    std::string errorField;
    auto json = arangodb::velocypack::Parser::fromJson("{ \
      \"consolidate\": null \
    }");
    CHECK(true == meta.init(json->slice(), errorField));
    CHECK((true == metaState.init(json->slice(), errorField)));
    CHECK((ConsolidationPolicy::Type::BYTES_ACCUM == meta._consolidationPolicy.type()));
    CHECK((300 == meta._consolidationPolicy.segmentThreshold()));
    CHECK((true == !meta._consolidationPolicy.policy()));
    CHECK((.85f == meta._consolidationPolicy.threshold()));
  }

  // test all parameters set to custom values
  std::string errorField;
  auto json = arangodb::velocypack::Parser::fromJson("{ \
        \"collections\": [ 42 ], \
        \"commitIntervalMsec\": 456, \
        \"cleanupIntervalStep\": 654, \
        \"consolidate\": { \"type\": \"bytes\", \"segmentThreshold\": 1001, \"threshold\": 0.11 }, \
        \"locale\": \"ru_RU.KOI8-R\" \
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
  CHECK(456 == meta._commitIntervalMsec);
  CHECK((ConsolidationPolicy::Type::BYTES == meta._consolidationPolicy.type()));
  CHECK((1001 == meta._consolidationPolicy.segmentThreshold()));
  CHECK((false == !meta._consolidationPolicy.policy()));
  CHECK((.11f == meta._consolidationPolicy.threshold()));
  CHECK(std::string("ru_RU.KOI8-R") == iresearch::locale_utils::name(meta._locale));
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

  CHECK((5U == slice.length()));
  tmpSlice = slice.get("collections");
  CHECK((true == tmpSlice.isArray() && 0 == tmpSlice.length()));
  tmpSlice = slice.get("cleanupIntervalStep");
  CHECK((true == tmpSlice.isNumber<size_t>() && 10 == tmpSlice.getNumber<size_t>()));
  tmpSlice = slice.get("commitIntervalMsec");
  CHECK((true == tmpSlice.isNumber<size_t>() && 60000 == tmpSlice.getNumber<size_t>()));
  tmpSlice = slice.get("consolidate");
  CHECK((true == tmpSlice.isObject() && 3 == tmpSlice.length()));
  tmpSlice2 = tmpSlice.get("segmentThreshold");
  CHECK((tmpSlice2.isNumber<size_t>() && 300 == tmpSlice2.getNumber<size_t>()));
  tmpSlice2 = tmpSlice.get("threshold");
  CHECK((tmpSlice2.isNumber<float>() && .85f == tmpSlice2.getNumber<float>()));
  tmpSlice2 = tmpSlice.get("type");
  CHECK((tmpSlice2.isString() && std::string("bytes_accum") == tmpSlice2.copyString()));
  tmpSlice = slice.get("locale");
  CHECK((true == tmpSlice.isString() && std::string("C") == tmpSlice.copyString()));
}

SECTION("test_writeCustomizedValues") {
  // test null consolidate
  {
    auto json = arangodb::velocypack::Parser::fromJson("{ \"consolidate\": null }");
    arangodb::iresearch::IResearchViewMeta meta;
    std::string tmpString;

    CHECK((true == meta.init(json->slice(), tmpString)));

    arangodb::velocypack::Builder builder;
    arangodb::velocypack::Slice tmpSlice;

    CHECK((true == meta.json(arangodb::velocypack::ObjectBuilder(&builder))));

    auto slice = builder.slice();
    tmpSlice = slice.get("consolidate");
    CHECK((true == tmpSlice.isNull()));
  }

  // test disabled consolidate
  {
    arangodb::iresearch::IResearchViewMeta meta;
    arangodb::iresearch::IResearchViewMetaState metaState;
    meta._consolidationPolicy = ConsolidationPolicy(ConsolidationPolicy::Type::FILL, 0, .2f);

    arangodb::velocypack::Builder builder;
    arangodb::velocypack::Slice tmpSlice;
    arangodb::velocypack::Slice tmpSlice2;

    CHECK(true == meta.json(arangodb::velocypack::ObjectBuilder(&builder)));
    CHECK((true == metaState.json(arangodb::velocypack::ObjectBuilder(&builder))));

    auto slice = builder.slice();
    tmpSlice = slice.get("consolidate");
    CHECK((true == tmpSlice.isObject() && 3 == tmpSlice.length()));
    tmpSlice2 = tmpSlice.get("segmentThreshold");
    CHECK((tmpSlice2.isNumber<size_t>() && 0 == tmpSlice2.getNumber<size_t>()));
    tmpSlice2 = tmpSlice.get("threshold");
    CHECK((tmpSlice2.isNumber<float>() && .2f == tmpSlice2.getNumber<float>()));
    tmpSlice2 = tmpSlice.get("type");
    CHECK((tmpSlice2.isString() && std::string("fill") == tmpSlice2.copyString()));
  }

  arangodb::iresearch::IResearchViewMeta meta;
  arangodb::iresearch::IResearchViewMetaState metaState;

  // test all parameters set to custom values
  metaState._collections.insert(42);
  metaState._collections.insert(52);
  metaState._collections.insert(62);
  meta._cleanupIntervalStep = 654;
  meta._commitIntervalMsec = 456;
  meta._consolidationPolicy = ConsolidationPolicy(ConsolidationPolicy::Type::BYTES, 101, .11f);
  meta._locale = iresearch::locale_utils::locale("en_UK.UTF-8");

  std::unordered_set<TRI_voc_cid_t> expectedCollections = { 42, 52, 62 };
  arangodb::velocypack::Builder builder;
  arangodb::velocypack::Slice tmpSlice;
  arangodb::velocypack::Slice tmpSlice2;

  builder.openObject();
  CHECK((true == meta.json(builder)));
  CHECK((true == metaState.json(builder)));
  builder.close();

  auto slice = builder.slice();

  CHECK((5U == slice.length()));
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
  CHECK((true == tmpSlice.isNumber<size_t>() && 456 == tmpSlice.getNumber<size_t>()));
  tmpSlice = slice.get("consolidate");
  CHECK((true == tmpSlice.isObject() && 3 == tmpSlice.length()));
  tmpSlice2 = tmpSlice.get("segmentThreshold");
  CHECK((tmpSlice2.isNumber<size_t>() && 101 == tmpSlice2.getNumber<size_t>()));
  tmpSlice2 = tmpSlice.get("threshold");
  CHECK((tmpSlice2.isNumber<float>() && .11f == tmpSlice2.getNumber<float>()));
  tmpSlice2 = tmpSlice.get("type");
  CHECK((tmpSlice2.isString() && std::string("bytes") == tmpSlice2.copyString()));
  tmpSlice = slice.get("locale");
  CHECK((tmpSlice.isString() && std::string("en_UK.UTF-8") == tmpSlice.copyString()));
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
    \"commitIntervalMsec\": 654, \
    \"cleanupIntervalStep\": 456, \
    \"consolidate\": { \"threshold\": 0.1 }, \
    \"locale\": \"ru_RU.KOI8-R\" \
  }");
  CHECK(true == meta.init(json->slice(), errorField, arangodb::iresearch::IResearchViewMeta::DEFAULT(), &mask));
  CHECK((true == metaState.init(json->slice(), errorField, arangodb::iresearch::IResearchViewMetaState::DEFAULT(), &maskState)));
  CHECK((true == maskState._collections));
  CHECK(true == mask._commitIntervalMsec);
  CHECK(true == mask._cleanupIntervalStep);
  CHECK((true == mask._consolidationPolicy));
  CHECK(true == mask._locale);
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
  CHECK(false == mask._commitIntervalMsec);
  CHECK(false == mask._cleanupIntervalStep);
  CHECK((false == mask._consolidationPolicy));
  CHECK(false == mask._locale);
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

  CHECK((5U == slice.length()));
  CHECK(true == slice.hasKey("collections"));
  CHECK(true == slice.hasKey("cleanupIntervalStep"));
  CHECK(true == slice.hasKey("commitIntervalMsec"));
  CHECK(true == slice.hasKey("consolidate"));
  CHECK(true == slice.hasKey("locale"));
}

SECTION("test_writeMaskNone") {
  arangodb::iresearch::IResearchViewMeta meta;
  arangodb::iresearch::IResearchViewMetaState metaState;
  arangodb::iresearch::IResearchViewMeta::Mask mask(false);
  arangodb::iresearch::IResearchViewMetaState::Mask maskState(false);
  arangodb::velocypack::Builder builder;

  CHECK(true == meta.json(arangodb::velocypack::ObjectBuilder(&builder), nullptr, &mask));
  CHECK((true == metaState.json(arangodb::velocypack::ObjectBuilder(&builder), nullptr, &maskState)));

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