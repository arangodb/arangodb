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

  std::set<ConsolidationPolicy::Type> expectedItem = { ConsolidationPolicy::Type::BYTES, ConsolidationPolicy::Type::BYTES_ACCUM, ConsolidationPolicy::Type::COUNT, ConsolidationPolicy::Type::FILL };

  for (auto& entry: meta._consolidationPolicies) {
    CHECK(true == (1 == expectedItem.erase(entry.type())));
    CHECK(true == (300 == entry.segmentThreshold()));
    CHECK(true == (false == !entry.policy()));
    CHECK(true == (0.85f == entry.threshold()));
  }

  CHECK(true == (expectedItem.empty()));
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
  defaults._consolidationPolicies.clear();
  defaults._consolidationPolicies.emplace_back(ConsolidationPolicy::Type::BYTES, 101, .11f);
  defaults._consolidationPolicies.emplace_back(ConsolidationPolicy::Type::BYTES_ACCUM, 151, .151f);
  defaults._consolidationPolicies.emplace_back(ConsolidationPolicy::Type::COUNT, 201, .21f);
  defaults._consolidationPolicies.emplace_back(ConsolidationPolicy::Type::FILL, 301, .31f);
  defaults._locale = irs::locale_utils::locale("ru");

  {
    auto json = arangodb::velocypack::Parser::fromJson("{}");
    CHECK(true == meta.init(json->slice(), tmpString, defaults));
    CHECK((true == metaState.init(json->slice(), tmpString, defaultsState)));
    CHECK((1 == metaState._collections.size()));
    CHECK((42 == *(metaState._collections.begin())));
    CHECK(654 == meta._cleanupIntervalStep);
    CHECK(456 == meta._commitIntervalMsec);

    std::set<ConsolidationPolicy::Type> expectedItem = { ConsolidationPolicy::Type::BYTES, ConsolidationPolicy::Type::BYTES_ACCUM, ConsolidationPolicy::Type::COUNT, ConsolidationPolicy::Type::FILL };

    for (auto& entry: meta._consolidationPolicies) {
      CHECK(true == (1 == expectedItem.erase(entry.type())));

      switch(entry.type()) {
       case ConsolidationPolicy::Type::BYTES:
        CHECK(true == (101 == entry.segmentThreshold()));
        CHECK(true == (false == !entry.policy()));
        CHECK(true == (.11f == entry.threshold()));
        break;
       case ConsolidationPolicy::Type::BYTES_ACCUM:
        CHECK(true == (151 == entry.segmentThreshold()));
        CHECK(true == (false == !entry.policy()));
        CHECK(true == (.151f == entry.threshold()));
        break;
       case ConsolidationPolicy::Type::COUNT:
        CHECK(true == (201 == entry.segmentThreshold()));
        CHECK(true == (false == !entry.policy()));
        CHECK(true == (.21f == entry.threshold()));
        break;
       case ConsolidationPolicy::Type::FILL:
        CHECK(true == (301 == entry.segmentThreshold()));
        CHECK(true == (false == !entry.policy()));
        CHECK(true == (.31f == entry.threshold()));
        break;
      }
    }

    CHECK(true == (expectedItem.empty()));
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

    std::set<ConsolidationPolicy::Type> expectedItem = { ConsolidationPolicy::Type::BYTES, ConsolidationPolicy::Type::BYTES_ACCUM, ConsolidationPolicy::Type::COUNT, ConsolidationPolicy::Type::FILL };

    for (auto& entry: meta._consolidationPolicies) {
      CHECK(true == (1 == expectedItem.erase(entry.type())));
      CHECK(true == (300 == entry.segmentThreshold()));
      CHECK(true == (false == !entry.policy()));
      CHECK(true == (.85f == entry.threshold()));
    }

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
    auto json = arangodb::velocypack::Parser::fromJson("{ \"consolidate\": { \"invalid\": \"abc\" } }");
    CHECK((true == metaState.init(json->slice(), errorField)));
    CHECK(false == meta.init(json->slice(), errorField));
    CHECK(std::string("consolidate=>invalid") == errorField);
  }

  {
    std::string errorField;
    auto json = arangodb::velocypack::Parser::fromJson("{ \"consolidate\": { \"invalid\": 0.5 } }");
    CHECK((true == metaState.init(json->slice(), errorField)));
    CHECK(false == meta.init(json->slice(), errorField));
    CHECK(std::string("consolidate=>invalid") == errorField);
  }

  {
    std::string errorField;
    auto json = arangodb::velocypack::Parser::fromJson("{ \"consolidate\": { \"bytes\": { \"segmentThreshold\": 0.5, \"threshold\": 1 } } }");
    CHECK(false == meta.init(json->slice(), errorField));
    CHECK(std::string("consolidate=>bytes=>segmentThreshold") == errorField);
  }

  {
    std::string errorField;
    auto json = arangodb::velocypack::Parser::fromJson("{ \"consolidate\": { \"bytes\": { \"threshold\": -0.5 } } }");
    CHECK((true == metaState.init(json->slice(), errorField)));
    CHECK(false == meta.init(json->slice(), errorField));
    CHECK(std::string("consolidate=>bytes=>threshold") == errorField);
  }

  {
    std::string errorField;
    auto json = arangodb::velocypack::Parser::fromJson("{ \"consolidate\": { \"bytes\": { \"threshold\": 1.5 } } }");
    CHECK((true == metaState.init(json->slice(), errorField)));
    CHECK(false == meta.init(json->slice(), errorField));
    CHECK(std::string("consolidate=>bytes=>threshold") == errorField);
  }

  // .............................................................................
  // test valid value
  // .............................................................................

  // test disabled consolidate (all empty)
  {
    std::string errorField;
    auto json = arangodb::velocypack::Parser::fromJson("{ \
      \"consolidate\": {} \
    }");
    CHECK(true == meta.init(json->slice(), errorField));
    CHECK((true == metaState.init(json->slice(), errorField)));
    CHECK(true == (meta._consolidationPolicies.empty()));
  }

  // test disabled consolidate (implicit disable due to value)
  {
    std::string errorField;
    auto json = arangodb::velocypack::Parser::fromJson("{ \
      \"consolidate\": { \"bytes_accum\": { \"segmentThreshold\": 0, \"threshold\": 0.2 }, \"fill\": { \"segmentThreshold\": 0 } } \
    }");
    CHECK(true == meta.init(json->slice(), errorField));
    CHECK((true == metaState.init(json->slice(), errorField)));
    CHECK(true == (meta._consolidationPolicies.empty()));
  }

  // test all parameters set to custom values
  std::string errorField;
  auto json = arangodb::velocypack::Parser::fromJson("{ \
        \"collections\": [ 42 ], \
        \"commitIntervalMsec\": 456, \
        \"cleanupIntervalStep\": 654, \
        \"consolidate\": { \"bytes\": { \"segmentThreshold\": 1001, \"threshold\": 0.11 }, \"bytes_accum\": { \"segmentThreshold\": 1501, \"threshold\": 0.151 }, \"count\": { \"segmentThreshold\": 2001 }, \"fill\": {} }, \
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

  std::set<ConsolidationPolicy::Type> expectedItem = { ConsolidationPolicy::Type::BYTES, ConsolidationPolicy::Type::BYTES_ACCUM, ConsolidationPolicy::Type::COUNT, ConsolidationPolicy::Type::FILL };

  for (auto& entry: meta._consolidationPolicies) {
    CHECK(true == (1 == expectedItem.erase(entry.type())));

    switch(entry.type()) {
     case ConsolidationPolicy::Type::BYTES:
      CHECK(true == (1001 == entry.segmentThreshold()));
      CHECK(true == (false == !entry.policy()));
      CHECK(true == (.11f == entry.threshold()));
      break;
     case ConsolidationPolicy::Type::BYTES_ACCUM:
      CHECK(true == (1501 == entry.segmentThreshold()));
      CHECK(true == (false == !entry.policy()));
      CHECK(true == (.151f == entry.threshold()));
      break;
     case ConsolidationPolicy::Type::COUNT:
      CHECK(true == (2001 == entry.segmentThreshold()));
      CHECK(true == (false == !entry.policy()));
      CHECK(true == (.85f == entry.threshold()));
      break;
     case ConsolidationPolicy::Type::FILL:
      CHECK(true == (300 == entry.segmentThreshold()));
      CHECK(true == (false == !entry.policy()));
      CHECK(true == (.85f == entry.threshold()));
      break;
    }
  }

  CHECK(true == (expectedItem.empty()));
  CHECK(std::string("ru_RU.KOI8-R") == iresearch::locale_utils::name(meta._locale));
}

SECTION("test_writeDefaults") {
  std::unordered_map<std::string, std::unordered_map<std::string, double>> expectedCommitItemConsolidate = {
    { "bytes",{ { "segmentThreshold", 300 },{ "threshold", .85f } } },
    { "bytes_accum",{ { "segmentThreshold", 300 },{ "threshold", .85f } } },
    { "count",{ { "segmentThreshold", 300 },{ "threshold", .85f } } },
    { "fill",{ { "segmentThreshold", 300 },{ "threshold", .85f } } }
  };
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
  CHECK((true == tmpSlice.isObject() && 4 == tmpSlice.length()));

  for (arangodb::velocypack::ObjectIterator itr(tmpSlice); itr.valid(); ++itr) {
    auto key = itr.key();
    auto value = itr.value();
    auto& expectedPolicy = expectedCommitItemConsolidate[key.copyString()];
    CHECK(true == key.isString());
    CHECK((true == value.isObject() && 2 == value.length()));

    for (arangodb::velocypack::ObjectIterator itr2(value); itr2.valid(); ++itr2) {
      auto key2 = itr2.key();
      auto value2 = itr2.value();
      CHECK((key2.isString() && expectedPolicy[key2.copyString()] == value2.getNumber<double>()));
      expectedPolicy.erase(key2.copyString());
    }

    expectedCommitItemConsolidate.erase(key.copyString());
  }

  CHECK(true == expectedCommitItemConsolidate.empty());
  tmpSlice = slice.get("locale");
  CHECK((true == tmpSlice.isString() && std::string("C") == tmpSlice.copyString()));
}

SECTION("test_writeCustomizedValues") {
  // test disabled consolidate
  {
    arangodb::iresearch::IResearchViewMeta meta;
    arangodb::iresearch::IResearchViewMetaState metaState;
    meta._consolidationPolicies.clear();
    meta._consolidationPolicies.emplace_back(ConsolidationPolicy::Type::BYTES, 0, ConsolidationPolicy::DEFAULT(ConsolidationPolicy::Type::BYTES).threshold());
    meta._consolidationPolicies.emplace_back(ConsolidationPolicy::Type::BYTES_ACCUM, 0, std::numeric_limits<float>::infinity());
    meta._consolidationPolicies.emplace_back(ConsolidationPolicy::Type::COUNT, 0, ConsolidationPolicy::DEFAULT(ConsolidationPolicy::Type::COUNT).threshold());
    meta._consolidationPolicies.emplace_back(ConsolidationPolicy::Type::FILL, 0, .2f);

    arangodb::velocypack::Builder builder;
    arangodb::velocypack::Slice tmpSlice;

    CHECK(true == meta.json(arangodb::velocypack::ObjectBuilder(&builder)));
    CHECK((true == metaState.json(arangodb::velocypack::ObjectBuilder(&builder))));

    auto slice = builder.slice();
    tmpSlice = slice.get("consolidate");
    CHECK((true == tmpSlice.isObject() && 0 == tmpSlice.length()));
  }

  arangodb::iresearch::IResearchViewMeta meta;
  arangodb::iresearch::IResearchViewMetaState metaState;

  // test all parameters set to custom values
  metaState._collections.insert(42);
  metaState._collections.insert(52);
  metaState._collections.insert(62);
  meta._cleanupIntervalStep = 654;
  meta._commitIntervalMsec = 456;
  meta._consolidationPolicies.clear();
  meta._consolidationPolicies.emplace_back(ConsolidationPolicy::Type::BYTES, 101, .11f);
  meta._consolidationPolicies.emplace_back(ConsolidationPolicy::Type::BYTES_ACCUM, 151, .151f);
  meta._consolidationPolicies.emplace_back(ConsolidationPolicy::Type::COUNT, 201, .21f);
  meta._consolidationPolicies.emplace_back(ConsolidationPolicy::Type::FILL, 301, .31f);
  meta._locale = iresearch::locale_utils::locale("en_UK.UTF-8");

  std::unordered_set<TRI_voc_cid_t> expectedCollections = { 42, 52, 62 };
  std::unordered_map<std::string, std::unordered_map<std::string, double>> expectedCommitItemConsolidate = {
    { "bytes",{ { "segmentThreshold", 101 },{ "threshold", .11f } } },
    { "bytes_accum",{ { "segmentThreshold", 151 },{ "threshold", .151f } } },
    { "count",{ { "segmentThreshold", 201 },{ "threshold", .21f } } },
    { "fill",{ { "segmentThreshold", 301 },{ "threshold", .31f } } }
  };
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
  CHECK((true == tmpSlice.isObject() && 4 == tmpSlice.length()));

  for (arangodb::velocypack::ObjectIterator itr(tmpSlice); itr.valid(); ++itr) {
    auto key = itr.key();
    auto value = itr.value();
    auto& expectedPolicy = expectedCommitItemConsolidate[key.copyString()];
    CHECK(true == key.isString());
    CHECK((true == value.isObject() && 2 == value.length()));

    for (arangodb::velocypack::ObjectIterator itr2(value); itr2.valid(); ++itr2) {
      auto key2 = itr2.key();
      auto value2 = itr2.value();
      CHECK((true == key2.isString() && expectedPolicy[key2.copyString()] == value2.getNumber<double>()));
      expectedPolicy.erase(key2.copyString());
    }

    expectedCommitItemConsolidate.erase(key.copyString());
  }

  CHECK(true == expectedCommitItemConsolidate.empty());
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
    \"consolidate\": {\"bytes_accum\": { \"threshold\": 0.1 } }, \
    \"locale\": \"ru_RU.KOI8-R\" \
  }");
  CHECK(true == meta.init(json->slice(), errorField, arangodb::iresearch::IResearchViewMeta::DEFAULT(), &mask));
  CHECK((true == metaState.init(json->slice(), errorField, arangodb::iresearch::IResearchViewMetaState::DEFAULT(), &maskState)));
  CHECK((true == maskState._collections));
  CHECK(true == mask._commitIntervalMsec);
  CHECK(true == mask._cleanupIntervalStep);
  CHECK(true == mask._consolidationPolicies);
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
  CHECK(false == mask._consolidationPolicies);
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