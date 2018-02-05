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
#include "VocBase/LogicalView.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

struct IResearchViewMetaSetup {
  StorageEngineMock engine;

  IResearchViewMetaSetup() {
    arangodb::EngineSelectorFeature::ENGINE = &engine;
  }

  ~IResearchViewMetaSetup() {
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
  typedef arangodb::iresearch::IResearchViewMeta::CommitMeta::ConsolidationPolicy ConsolidationPolicy;

SECTION("test_defaults") {
  arangodb::iresearch::IResearchViewMeta meta;

  CHECK(true == meta._collections.empty());
  CHECK(true == (10 == meta._commit._cleanupIntervalStep));
  CHECK(true == (60 * 1000 == meta._commit._commitIntervalMsec));
  CHECK(true == (5 * 1000 == meta._commit._commitTimeoutMsec));

  std::set<ConsolidationPolicy::Type> expectedItem = { ConsolidationPolicy::Type::BYTES, ConsolidationPolicy::Type::BYTES_ACCUM, ConsolidationPolicy::Type::COUNT, ConsolidationPolicy::Type::FILL };

  for (auto& entry: meta._commit._consolidationPolicies) {
    CHECK(true == (1 == expectedItem.erase(entry.type())));
    CHECK(true == (300 == entry.segmentThreshold()));
    CHECK(true == (false == !entry.policy()));
    CHECK(true == (0.85f == entry.threshold()));
  }

  CHECK(true == (expectedItem.empty()));
  CHECK(std::string("") == meta._dataPath);
  CHECK(std::string("C") == irs::locale_utils::name(meta._locale));
  CHECK(5 == meta._threadsMaxIdle);
  CHECK(5 == meta._threadsMaxTotal);
}

SECTION("test_inheritDefaults") {
  auto viewJson = arangodb::velocypack::Parser::fromJson("{ \"id\": 123, \"name\": \"testView\", \"type\": \"testType\" }");
  arangodb::LogicalView logicalView(nullptr, viewJson->slice());
  arangodb::iresearch::IResearchViewMeta defaults;
  arangodb::iresearch::IResearchViewMeta meta;
  std::string tmpString;

  defaults._collections.insert(42);
  defaults._commit._cleanupIntervalStep = 654;
  defaults._commit._commitIntervalMsec = 456;
  defaults._commit._commitTimeoutMsec = 789;
  defaults._commit._consolidationPolicies.clear();
  defaults._commit._consolidationPolicies.emplace_back(ConsolidationPolicy::Type::BYTES, 101, .11f);
  defaults._commit._consolidationPolicies.emplace_back(ConsolidationPolicy::Type::BYTES_ACCUM, 151, .151f);
  defaults._commit._consolidationPolicies.emplace_back(ConsolidationPolicy::Type::COUNT, 201, .21f);
  defaults._commit._consolidationPolicies.emplace_back(ConsolidationPolicy::Type::FILL, 301, .31f);
  defaults._dataPath = "path";
  defaults._locale = irs::locale_utils::locale("ru");
  defaults._threadsMaxIdle = 8;
  defaults._threadsMaxTotal = 16;

  {
    auto json = arangodb::velocypack::Parser::fromJson("{}");
    CHECK(true == meta.init(json->slice(), tmpString, logicalView, defaults));
    CHECK(1 == meta._collections.size());
    CHECK(42 == *(meta._collections.begin()));
    CHECK(654 == meta._commit._cleanupIntervalStep);
    CHECK(456 == meta._commit._commitIntervalMsec);
    CHECK(789 == meta._commit._commitTimeoutMsec);

    std::set<ConsolidationPolicy::Type> expectedItem = { ConsolidationPolicy::Type::BYTES, ConsolidationPolicy::Type::BYTES_ACCUM, ConsolidationPolicy::Type::COUNT, ConsolidationPolicy::Type::FILL };

    for (auto& entry: meta._commit._consolidationPolicies) {
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
    CHECK(std::string("path") == meta._dataPath);
    CHECK(std::string("ru") == irs::locale_utils::name(meta._locale));
    CHECK(8 == meta._threadsMaxIdle);
    CHECK(16 == meta._threadsMaxTotal);
  }
}

SECTION("test_readDefaults") {
  arangodb::iresearch::IResearchViewMeta meta;
  std::string tmpString;

  {
    auto viewJson = arangodb::velocypack::Parser::fromJson("{ \"id\": 123, \"name\": \"testView\", \"type\": \"testType\" }");
    arangodb::LogicalView logicalView(nullptr, viewJson->slice());
    auto json = arangodb::velocypack::Parser::fromJson("{}");
    CHECK(true == meta.init(json->slice(), tmpString, logicalView));
    CHECK(true == meta._collections.empty());
    CHECK(10 == meta._commit._cleanupIntervalStep);
    CHECK(60 * 1000 == meta._commit._commitIntervalMsec);
    CHECK(5 * 1000 == meta._commit._commitTimeoutMsec);

    std::set<ConsolidationPolicy::Type> expectedItem = { ConsolidationPolicy::Type::BYTES, ConsolidationPolicy::Type::BYTES_ACCUM, ConsolidationPolicy::Type::COUNT, ConsolidationPolicy::Type::FILL };

    for (auto& entry: meta._commit._consolidationPolicies) {
      CHECK(true == (1 == expectedItem.erase(entry.type())));
      CHECK(true == (300 == entry.segmentThreshold()));
      CHECK(true == (false == !entry.policy()));
      CHECK(true == (.85f == entry.threshold()));
    }

    CHECK((std::string("testType-123") == meta._dataPath));
    CHECK(std::string("C") == irs::locale_utils::name(meta._locale));
    CHECK(5 == meta._threadsMaxIdle);
    CHECK(5 == meta._threadsMaxTotal);
  }
}

SECTION("test_readCustomizedValues") {
  auto viewJson = arangodb::velocypack::Parser::fromJson("{ \"id\": 123, \"name\": \"testView\", \"type\": \"testType\" }");
  arangodb::LogicalView logicalView(nullptr, viewJson->slice());
  std::unordered_set<TRI_voc_cid_t> expectedCollections = { 42 };
  arangodb::iresearch::IResearchViewMeta meta;

  // .............................................................................
  // test invalid value
  // .............................................................................

  {
    std::string errorField;
    auto json = arangodb::velocypack::Parser::fromJson("{ \"collections\": \"invalid\" }");
    CHECK(false == meta.init(json->slice(), errorField, logicalView));
    CHECK(std::string("collections") == errorField);
  }

  {
    std::string errorField;
    auto json = arangodb::velocypack::Parser::fromJson("{ \"commit\": \"invalid\" }");
    CHECK(false == meta.init(json->slice(), errorField, logicalView));
    CHECK(std::string("commit") == errorField);
  }

  {
    std::string errorField;
    auto json = arangodb::velocypack::Parser::fromJson("{ \"commit\": { \"commitIntervalMsec\": 0.5 } }");
    CHECK(false == meta.init(json->slice(), errorField, logicalView));
    CHECK(std::string("commit=>commitIntervalMsec") == errorField);
  }

  {
    std::string errorField;
    auto json = arangodb::velocypack::Parser::fromJson("{ \"commit\": { \"cleanupIntervalStep\": 0.5 } }");
    CHECK(false == meta.init(json->slice(), errorField, logicalView));
    CHECK(std::string("commit=>cleanupIntervalStep") == errorField);
  }

  {
    std::string errorField;
    auto json = arangodb::velocypack::Parser::fromJson("{ \"commit\": { \"consolidate\": \"invalid\" } }");
    CHECK(false == meta.init(json->slice(), errorField, logicalView));
    CHECK(std::string("commit=>consolidate") == errorField);
  }

  {
    std::string errorField;
    auto json = arangodb::velocypack::Parser::fromJson("{ \"commit\": { \"consolidate\": { \"invalid\": \"abc\" } } }");
    CHECK(false == meta.init(json->slice(), errorField, logicalView));
    CHECK(std::string("commit=>consolidate=>invalid") == errorField);
  }

  {
    std::string errorField;
    auto json = arangodb::velocypack::Parser::fromJson("{ \"commit\": { \"consolidate\": { \"invalid\": 0.5 } } }");
    CHECK(false == meta.init(json->slice(), errorField, logicalView));
    CHECK(std::string("commit=>consolidate=>invalid") == errorField);
  }

  {
    std::string errorField;
    auto json = arangodb::velocypack::Parser::fromJson("{ \"commit\": { \"consolidate\": { \"bytes\": { \"segmentThreshold\": 0.5, \"threshold\": 1 } } } }");
    CHECK(false == meta.init(json->slice(), errorField, logicalView));
    CHECK(std::string("commit=>consolidate=>bytes=>segmentThreshold") == errorField);
  }

  {
    std::string errorField;
    auto json = arangodb::velocypack::Parser::fromJson("{ \"commit\": { \"consolidate\": { \"bytes\": { \"threshold\": -0.5 } } } }");
    CHECK(false == meta.init(json->slice(), errorField, logicalView));
    CHECK(std::string("commit=>consolidate=>bytes=>threshold") == errorField);
  }

  {
    std::string errorField;
    auto json = arangodb::velocypack::Parser::fromJson("{ \"commit\": { \"consolidate\": { \"bytes\": { \"threshold\": 1.5 } } } }");
    CHECK(false == meta.init(json->slice(), errorField, logicalView));
    CHECK(std::string("commit=>consolidate=>bytes=>threshold") == errorField);
  }

  {
    std::string errorField;
    auto json = arangodb::velocypack::Parser::fromJson("{ \"threadsMaxIdle\": 0.5 }");
    CHECK(false == meta.init(json->slice(), errorField, logicalView));
    CHECK(std::string("threadsMaxIdle") == errorField);
  }

  {
    std::string errorField;
    auto json = arangodb::velocypack::Parser::fromJson("{ \"threadsMaxTotal\": 0.5 }");
    CHECK(false == meta.init(json->slice(), errorField, logicalView));
    CHECK(std::string("threadsMaxTotal") == errorField);
  }

  {
    std::string errorField;
    auto json = arangodb::velocypack::Parser::fromJson("{ \"threadsMaxTotal\": 0 }");
    CHECK(false == meta.init(json->slice(), errorField, logicalView));
    CHECK(std::string("threadsMaxTotal") == errorField);
  }

  // .............................................................................
  // test valid value
  // .............................................................................

  // test disabled consolidate (all empty)
  {
    std::string errorField;
    auto json = arangodb::velocypack::Parser::fromJson("{ \
      \"commit\": { \"consolidate\": {} } \
    }");
    CHECK(true == meta.init(json->slice(), errorField, logicalView));
    CHECK(true == (meta._commit._consolidationPolicies.empty()));
  }

  // test disabled consolidate (implicit disable due to value)
  {
    std::string errorField;
    auto json = arangodb::velocypack::Parser::fromJson("{ \
      \"commit\": { \"consolidate\": { \"bytes_accum\": { \"segmentThreshold\": 0, \"threshold\": 0.2 }, \"fill\": { \"segmentThreshold\": 0 } } } \
    }");
    CHECK(true == meta.init(json->slice(), errorField, logicalView));
    CHECK(true == (meta._commit._consolidationPolicies.empty()));
  }

  // test all parameters set to custom values
  std::string errorField;
  auto json = arangodb::velocypack::Parser::fromJson("{ \
        \"collections\": [ 42 ], \
        \"commit\": { \"commitIntervalMsec\": 456, \"cleanupIntervalStep\": 654, \"commitTimeoutMsec\": 789, \"consolidate\": { \"bytes\": { \"segmentThreshold\": 1001, \"threshold\": 0.11 }, \"bytes_accum\": { \"segmentThreshold\": 1501, \"threshold\": 0.151 }, \"count\": { \"segmentThreshold\": 2001 }, \"fill\": {} } }, \
        \"dataPath\": \"somepath\", \
        \"locale\": \"ru_RU.KOI8-R\", \
        \"threadsMaxIdle\": 8, \
        \"threadsMaxTotal\": 16 \
    }");
  CHECK(true == meta.init(json->slice(), errorField, logicalView));
  CHECK(1 == meta._collections.size());

  for (auto& collection: meta._collections) {
    CHECK(1 == expectedCollections.erase(collection));
  }

  CHECK(true == expectedCollections.empty());
  CHECK(42 == *(meta._collections.begin()));
  CHECK(654 == meta._commit._cleanupIntervalStep);
  CHECK(456 == meta._commit._commitIntervalMsec);
  CHECK(789 == meta._commit._commitTimeoutMsec);

  std::set<ConsolidationPolicy::Type> expectedItem = { ConsolidationPolicy::Type::BYTES, ConsolidationPolicy::Type::BYTES_ACCUM, ConsolidationPolicy::Type::COUNT, ConsolidationPolicy::Type::FILL };

  for (auto& entry: meta._commit._consolidationPolicies) {
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
  CHECK(std::string("somepath") == meta._dataPath);
  CHECK(std::string("ru_RU.UTF-8") == iresearch::locale_utils::name(meta._locale));
  CHECK(8 == meta._threadsMaxIdle);
  CHECK(16 == meta._threadsMaxTotal);
}

SECTION("test_writeDefaults") {
  std::unordered_map<std::string, std::unordered_map<std::string, double>> expectedCommitItemConsolidate = {
    { "bytes",{ { "segmentThreshold", 300 },{ "threshold", .85f } } },
    { "bytes_accum",{ { "segmentThreshold", 300 },{ "threshold", .85f } } },
    { "count",{ { "segmentThreshold", 300 },{ "threshold", .85f } } },
    { "fill",{ { "segmentThreshold", 300 },{ "threshold", .85f } } }
  };
  arangodb::iresearch::IResearchViewMeta meta;
  arangodb::velocypack::Builder builder;
  arangodb::velocypack::Slice tmpSlice;
  arangodb::velocypack::Slice tmpSlice2;

  CHECK(meta.json(arangodb::velocypack::ObjectBuilder(&builder)));

  auto slice = builder.slice();

  CHECK((5U == slice.length()));
  tmpSlice = slice.get("collections");
  CHECK((true == tmpSlice.isArray() && 0 == tmpSlice.length()));
  tmpSlice = slice.get("commit");
  CHECK((true == tmpSlice.isObject() && 4 == tmpSlice.length()));
  tmpSlice2 = tmpSlice.get("cleanupIntervalStep");
  CHECK((true == tmpSlice2.isNumber<size_t>() && 10 == tmpSlice2.getNumber<size_t>()));
  tmpSlice2 = tmpSlice.get("commitIntervalMsec");
  CHECK((true == tmpSlice2.isNumber<size_t>() && 60000 == tmpSlice2.getNumber<size_t>()));
  tmpSlice2 = tmpSlice.get("commitTimeoutMsec");
  CHECK((true == tmpSlice2.isNumber<size_t>() && 5000 == tmpSlice2.getNumber<size_t>()));
  tmpSlice2 = tmpSlice.get("consolidate");
  CHECK((true == tmpSlice2.isObject() && 4 == tmpSlice2.length()));

  for (arangodb::velocypack::ObjectIterator itr(tmpSlice2); itr.valid(); ++itr) {
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
  tmpSlice = slice.get("threadsMaxIdle");
  CHECK((true == tmpSlice.isNumber() && 5 == tmpSlice.getUInt()));
  tmpSlice = slice.get("threadsMaxTotal");
  CHECK((true == tmpSlice.isNumber() && 5 == tmpSlice.getUInt()));
}

SECTION("test_writeCustomizedValues") {
  // test disabled consolidate
  {
    arangodb::iresearch::IResearchViewMeta meta;
    meta._commit._consolidationPolicies.clear();
    meta._commit._consolidationPolicies.emplace_back(ConsolidationPolicy::Type::BYTES, 0, ConsolidationPolicy::DEFAULT(ConsolidationPolicy::Type::BYTES).threshold());
    meta._commit._consolidationPolicies.emplace_back(ConsolidationPolicy::Type::BYTES_ACCUM, 0, std::numeric_limits<float>::infinity());
    meta._commit._consolidationPolicies.emplace_back(ConsolidationPolicy::Type::COUNT, 0, ConsolidationPolicy::DEFAULT(ConsolidationPolicy::Type::COUNT).threshold());
    meta._commit._consolidationPolicies.emplace_back(ConsolidationPolicy::Type::FILL, 0, .2f);

    arangodb::velocypack::Builder builder;
    arangodb::velocypack::Slice tmpSlice;

    CHECK(true == meta.json(arangodb::velocypack::ObjectBuilder(&builder)));

    auto slice = builder.slice();
    tmpSlice = slice.get("commit");
    CHECK(true == tmpSlice.isObject());
    tmpSlice = tmpSlice.get("consolidate");
    CHECK((true == tmpSlice.isObject() && 0 == tmpSlice.length()));
  }

  arangodb::iresearch::IResearchViewMeta meta;

  // test all parameters set to custom values
  meta._collections.insert(42);
  meta._collections.insert(52);
  meta._collections.insert(62);
  meta._commit._cleanupIntervalStep = 654;
  meta._commit._commitIntervalMsec = 456;
  meta._commit._commitTimeoutMsec = 789;
  meta._commit._consolidationPolicies.clear();
  meta._commit._consolidationPolicies.emplace_back(ConsolidationPolicy::Type::BYTES, 101, .11f);
  meta._commit._consolidationPolicies.emplace_back(ConsolidationPolicy::Type::BYTES_ACCUM, 151, .151f);
  meta._commit._consolidationPolicies.emplace_back(ConsolidationPolicy::Type::COUNT, 201, .21f);
  meta._commit._consolidationPolicies.emplace_back(ConsolidationPolicy::Type::FILL, 301, .31f);
  meta._dataPath = "somepath";
  meta._locale = iresearch::locale_utils::locale("en_UK.UTF-8");
  meta._threadsMaxIdle = 8;
  meta._threadsMaxTotal = 16;

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

  CHECK(meta.json(arangodb::velocypack::ObjectBuilder(&builder)));

  auto slice = builder.slice();

  CHECK((6U == slice.length()));
  tmpSlice = slice.get("collections");
  CHECK((true == tmpSlice.isArray() && 3 == tmpSlice.length()));

  for (arangodb::velocypack::ArrayIterator itr(tmpSlice); itr.valid(); ++itr) {
    auto value = itr.value();
    CHECK((true == value.isUInt() && 1 == expectedCollections.erase(value.getUInt())));
  }

  CHECK(true == expectedCollections.empty());
  tmpSlice = slice.get("commit");
  CHECK((true == tmpSlice.isObject() && 4 == tmpSlice.length()));
  tmpSlice2 = tmpSlice.get("cleanupIntervalStep");
  CHECK((true == tmpSlice2.isNumber<size_t>() && 654 == tmpSlice2.getNumber<size_t>()));
  tmpSlice2 = tmpSlice.get("commitIntervalMsec");
  CHECK((true == tmpSlice2.isNumber<size_t>() && 456 == tmpSlice2.getNumber<size_t>()));
  tmpSlice2 = tmpSlice.get("commitTimeoutMsec");
  CHECK((true == tmpSlice2.isNumber<size_t>() && 789 == tmpSlice2.getNumber<size_t>()));
  tmpSlice2 = tmpSlice.get("consolidate");
  CHECK((true == tmpSlice2.isObject() && 4 == tmpSlice2.length()));

  for (arangodb::velocypack::ObjectIterator itr(tmpSlice2); itr.valid(); ++itr) {
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
  tmpSlice = slice.get("dataPath");
  CHECK((tmpSlice.isString() && std::string("somepath") == tmpSlice.copyString()));
  tmpSlice = slice.get("locale");
  CHECK((tmpSlice.isString() && std::string("en_UK.UTF-8") == tmpSlice.copyString()));
  tmpSlice = slice.get("threadsMaxIdle");
  CHECK((true == tmpSlice.isNumber() && 8 == tmpSlice.getUInt()));
  tmpSlice = slice.get("threadsMaxTotal");
  CHECK((true == tmpSlice.isNumber() && 16 == tmpSlice.getUInt()));
}

SECTION("test_readMaskAll") {
  auto viewJson = arangodb::velocypack::Parser::fromJson("{ \"id\": 123, \"name\": \"testView\", \"type\": \"testType\" }");
  arangodb::LogicalView logicalView(nullptr, viewJson->slice());
  arangodb::iresearch::IResearchViewMeta meta;
  arangodb::iresearch::IResearchViewMeta::Mask mask;
  std::string errorField;

  auto json = arangodb::velocypack::Parser::fromJson("{ \
    \"collections\": [ 42 ], \
    \"commit\": { \"commitIntervalMsec\": 654, \"cleanupIntervalStep\": 456, \"consolidate\": {\"bytes_accum\": { \"threshold\": 0.1 } } }, \
    \"dataPath\": \"somepath\", \
    \"locale\": \"ru_RU.KOI8-R\", \
    \"threadsMaxIdle\": 8, \
    \"threadsMaxTotal\": 16 \
  }");
  CHECK(true == meta.init(json->slice(), errorField, logicalView, arangodb::iresearch::IResearchViewMeta::DEFAULT(), &mask));
  CHECK(true == mask._collections);
  CHECK(true == mask._commit);
  CHECK(true == mask._dataPath);
  CHECK(true == mask._locale);
  CHECK(true == mask._threadsMaxIdle);
  CHECK(true == mask._threadsMaxTotal);
}

SECTION("test_readMaskNone") {
  auto viewJson = arangodb::velocypack::Parser::fromJson("{ \"id\": 123, \"name\": \"testView\", \"type\": \"testType\" }");
  arangodb::LogicalView logicalView(nullptr, viewJson->slice());
  arangodb::iresearch::IResearchViewMeta meta;
  arangodb::iresearch::IResearchViewMeta::Mask mask;
  std::string errorField;

  auto json = arangodb::velocypack::Parser::fromJson("{}");
  CHECK(true == meta.init(json->slice(), errorField, logicalView, arangodb::iresearch::IResearchViewMeta::DEFAULT(), &mask));
  CHECK(false == mask._collections);
  CHECK(false == mask._commit);
  CHECK(false == mask._dataPath);
  CHECK(false == mask._locale);
  CHECK(false == mask._threadsMaxIdle);
  CHECK(false == mask._threadsMaxTotal);
}

SECTION("test_writeMaskAll") {
  arangodb::iresearch::IResearchViewMeta meta;
  arangodb::iresearch::IResearchViewMeta::Mask mask(true);
  arangodb::velocypack::Builder builder;
  arangodb::velocypack::Slice tmpSlice;

  meta._dataPath = "path"; // add a value so that attribute is not omitted

  CHECK(true == meta.json(arangodb::velocypack::ObjectBuilder(&builder), nullptr, &mask));

  auto slice = builder.slice();

  CHECK(6U == slice.length());
  CHECK(true == slice.hasKey("collections"));
  CHECK(true == slice.hasKey("commit"));
  tmpSlice = slice.get("commit");
  CHECK(true == tmpSlice.hasKey("cleanupIntervalStep"));
  CHECK(true == tmpSlice.hasKey("commitIntervalMsec"));
  CHECK(true == tmpSlice.hasKey("commitTimeoutMsec"));
  CHECK(true == tmpSlice.hasKey("consolidate"));
  CHECK(true == slice.hasKey("dataPath"));
  CHECK(true == slice.hasKey("locale"));
  CHECK(true == slice.hasKey("threadsMaxIdle"));
  CHECK(true == slice.hasKey("threadsMaxTotal"));
}

SECTION("test_writeMaskNone") {
  arangodb::iresearch::IResearchViewMeta meta;
  arangodb::iresearch::IResearchViewMeta::Mask mask(false);
  arangodb::velocypack::Builder builder;

  CHECK(true == meta.json(arangodb::velocypack::ObjectBuilder(&builder), nullptr, &mask));

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