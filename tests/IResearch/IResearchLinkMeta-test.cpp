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
#include "utils/locale_utils.hpp"

#include "IResearch/common.h"
#include "Mocks/LogLevels.h"
#include "Mocks/Servers.h"
#include "Mocks/StorageEngineMock.h"

#include "ApplicationFeatures/CommunicationFeaturePhase.h"
#include "ApplicationFeatures/GreetingsFeaturePhase.h"
#include "Aql/AqlFunctionFeature.h"
#include "Aql/OptimizerRulesFeature.h"
#include "Cluster/ClusterFeature.h"
#include "FeaturePhases/BasicFeaturePhaseServer.h"
#include "FeaturePhases/ClusterFeaturePhase.h"
#include "FeaturePhases/DatabaseFeaturePhase.h"
#include "FeaturePhases/V8FeaturePhase.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "IResearch/IResearchCommon.h"
#include "IResearch/IResearchFeature.h"
#include "IResearch/IResearchLinkMeta.h"
#include "IResearch/VelocyPackHelper.h"
#include "RestServer/AqlFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "RestServer/SystemDatabaseFeature.h"
#include "Sharding/ShardingFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "V8Server/V8DealerFeature.h"
#include "VocBase/Methods/Collections.h"
#include "velocypack/Builder.h"
#include "velocypack/Iterator.h"
#include "velocypack/Parser.h"

#if USE_ENTERPRISE
#include "Enterprise/Ldap/LdapFeature.h"
#endif

namespace {

struct TestAttributeZ : public irs::attribute {
  static constexpr irs::string_ref type_name() noexcept {
    return "TestAttributeZ";
  }
};

REGISTER_ATTRIBUTE(TestAttributeZ);

class EmptyAnalyzer : public irs::analysis::analyzer {
 public:
  static constexpr irs::string_ref type_name() noexcept {
    return "empty";
  }

  static ptr make(irs::string_ref const&) {
    PTR_NAMED(EmptyAnalyzer, ptr);
    return ptr;
  }

  static bool normalize(irs::string_ref const& args, std::string& out) {
    auto slice = arangodb::iresearch::slice(args);
    arangodb::velocypack::Builder builder;
    if (slice.isString()) {
      VPackObjectBuilder scope(&builder);
      arangodb::iresearch::addStringRef(builder, "args",
                                        arangodb::iresearch::getStringRef(slice));
    } else if (slice.isObject() && slice.hasKey("args") && slice.get("args").isString()) {
      VPackObjectBuilder scope(&builder);
      arangodb::iresearch::addStringRef(builder, "args",
                                        arangodb::iresearch::getStringRef(slice.get("args")));
    } else {
      return false;
    }

    out = builder.buffer()->toString();
    return true;
  }

  EmptyAnalyzer() : irs::analysis::analyzer(irs::type<EmptyAnalyzer>::get()) { }
  virtual irs::attribute* get_mutable(irs::type_info::type_id type) noexcept override {
    if (type == irs::type<TestAttributeZ>::id()) {
      return &_attr;
    }
    return nullptr;
  }
  virtual bool next() override { return false; }
  virtual bool reset(irs::string_ref const& data) override { return true; }

 private:
  TestAttributeZ _attr;
};

REGISTER_ANALYZER_VPACK(EmptyAnalyzer, EmptyAnalyzer::make, EmptyAnalyzer::normalize);

static const VPackBuilder systemDatabaseBuilder = dbArgsBuilder();
static const VPackSlice   systemDatabaseArgs = systemDatabaseBuilder.slice();
}  // namespace

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

class IResearchLinkMetaTest
    : public ::testing::Test,
      public arangodb::tests::LogSuppressor<arangodb::Logger::AGENCYCOMM, arangodb::LogLevel::FATAL>,
      public arangodb::tests::LogSuppressor<arangodb::Logger::AUTHENTICATION, arangodb::LogLevel::ERR> {
 protected:
  arangodb::tests::mocks::MockAqlServer server;

  IResearchLinkMetaTest() {
    arangodb::tests::init();

    auto& dbFeature = server.getFeature<arangodb::DatabaseFeature>();
    auto sysvocbase = dbFeature.useDatabase(arangodb::StaticStrings::SystemDatabase);
    std::shared_ptr<arangodb::LogicalCollection> unused;
    arangodb::OperationOptions options(arangodb::ExecContext::current());
    arangodb::methods::Collections::createSystem(*sysvocbase, options,
                                                 arangodb::tests::AnalyzerCollectionName,
                                                 false, unused);
    unused = nullptr;
    
    TRI_vocbase_t* vocbase;
    dbFeature.createDatabase(testDBInfo(server.server()), vocbase);  // required for IResearchAnalyzerFeature::emplace(...)
    arangodb::methods::Collections::createSystem(*vocbase, options,
                                                 arangodb::tests::AnalyzerCollectionName,
                                                 false, unused);

    auto& analyzers = server.getFeature<arangodb::iresearch::IResearchAnalyzerFeature>();
    arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;

    analyzers.emplace(result, "testVocbase::empty", "empty",
                      VPackParser::fromJson("{ \"args\": \"de\" }")->slice(),
                      irs::flags{irs::type<irs::frequency>::get()});  // cache the 'empty' analyzer for 'testVocbase'
  }
};

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------
TEST_F(IResearchLinkMetaTest, test_defaults) {
  arangodb::iresearch::IResearchLinkMeta meta;

  {
    ASSERT_EQ(1, meta._analyzerDefinitions.size());
    EXPECT_TRUE("identity" == (*meta._analyzerDefinitions.begin())->name());
    EXPECT_TRUE(irs::flags({irs::type<irs::norm>::get(), irs::type<irs::frequency>::get()}) ==
                 (*meta._analyzerDefinitions.begin())->features());
    EXPECT_NE(nullptr, meta._analyzerDefinitions.begin()->get());
  }

  EXPECT_TRUE(meta._sort.empty());
  EXPECT_TRUE(true == meta._fields.empty());
  EXPECT_TRUE(false == meta._includeAllFields);
  EXPECT_TRUE(false == meta._trackListPositions);
  EXPECT_TRUE(arangodb::iresearch::ValueStorage::NONE == meta._storeValues);
  EXPECT_TRUE(1U == meta._analyzers.size());
  EXPECT_TRUE(*(meta._analyzers.begin()));
  EXPECT_TRUE("identity" == meta._analyzers.begin()->_pool->name());
  EXPECT_TRUE("identity" == meta._analyzers.begin()->_shortName);
  EXPECT_TRUE(irs::flags({irs::type<irs::norm>::get(), irs::type<irs::frequency>::get()}) ==
               meta._analyzers.begin()->_pool->features());
  EXPECT_FALSE(!meta._analyzers.begin()->_pool->get());
}

TEST_F(IResearchLinkMetaTest, test_inheritDefaults) {
  arangodb::iresearch::IResearchAnalyzerFeature analyzers(server.server());
  arangodb::iresearch::IResearchLinkMeta defaults;
  arangodb::iresearch::IResearchLinkMeta meta;
  std::unordered_set<std::string> expectedFields = {"abc"};
  std::unordered_set<std::string> expectedOverrides = {"xyz"};
  std::string tmpString;

  analyzers.start();

  defaults._fields["abc"] = arangodb::iresearch::FieldMeta();
  defaults._includeAllFields = true;
  defaults._trackListPositions = true;
  defaults._storeValues = arangodb::iresearch::ValueStorage::VALUE;
  defaults._analyzers.clear();
  defaults._analyzers.emplace_back(arangodb::iresearch::IResearchLinkMeta::Analyzer(
      analyzers.get("testVocbase::empty", arangodb::QueryAnalyzerRevisions::QUERY_LATEST), "empty"));
  defaults._fields["abc"]->_fields["xyz"] = arangodb::iresearch::IResearchLinkMeta();
  defaults._sort.emplace_back(std::vector<arangodb::basics::AttributeName>{{"foo",false}}, true);

  auto json = VPackParser::fromJson("{}");
  EXPECT_TRUE(meta.init(server.server(), json->slice(), false, tmpString, nullptr, defaults));
  EXPECT_EQ(1U, meta._fields.size());

  for (auto& field : meta._fields) {
    EXPECT_EQ(1U, expectedFields.erase(static_cast<std::string>(field.key()))); // FIXME: after C++20 remove cast and use heterogeneous lookup
    EXPECT_EQ(1U, field.value()->_fields.size());

    for (auto& fieldOverride : field.value()->_fields) {
      auto& actual = *(fieldOverride.value());
      EXPECT_EQ(1U, expectedOverrides.erase(static_cast<std::string>(fieldOverride.key()))); // FIXME: after C++20 remove cast and use heterogeneous lookup

      if ("xyz" == fieldOverride.key()) {
        EXPECT_TRUE(actual._fields.empty());
        EXPECT_FALSE(actual._includeAllFields);
        EXPECT_FALSE(actual._trackListPositions);
        EXPECT_EQ(arangodb::iresearch::ValueStorage::NONE, actual._storeValues);
        EXPECT_EQ(1U, actual._analyzers.size());
        EXPECT_TRUE(*(actual._analyzers.begin()));
        EXPECT_EQ("identity", actual._analyzers.begin()->_pool->name());
        EXPECT_EQ("identity", actual._analyzers.begin()->_shortName);
        EXPECT_TRUE((irs::flags({irs::type<irs::norm>::get(), irs::type<irs::frequency>::get()}) ==
                     actual._analyzers.begin()->_pool->features()));
        EXPECT_FALSE(!actual._analyzers.begin()->_pool->get());
      }
    }
  }

  EXPECT_TRUE(expectedOverrides.empty());
  EXPECT_TRUE(expectedFields.empty());
  EXPECT_TRUE(meta._includeAllFields);
  EXPECT_TRUE(meta._trackListPositions);
  EXPECT_TRUE((arangodb::iresearch::ValueStorage::VALUE == meta._storeValues));

  EXPECT_EQ(1U, meta._analyzers.size());
  EXPECT_TRUE(*(meta._analyzers.begin()));
  EXPECT_EQ("testVocbase::empty", meta._analyzers.begin()->_pool->name());
  EXPECT_EQ("empty", meta._analyzers.begin()->_shortName);
  EXPECT_TRUE((irs::flags({irs::type<irs::frequency>::get()}) ==
               meta._analyzers.begin()->_pool->features()));
  EXPECT_FALSE(!meta._analyzers.begin()->_pool->get());

  EXPECT_EQ(0, meta._sort.size());
}

TEST_F(IResearchLinkMetaTest, test_readDefaults) {
  auto json = VPackParser::fromJson("{}");

  // without active vobcase
  {
    arangodb::iresearch::IResearchLinkMeta meta;
    std::string tmpString;
    EXPECT_TRUE(meta.init(server.server(), json->slice(), false, tmpString));
    EXPECT_TRUE(meta._fields.empty());
    EXPECT_FALSE(meta._includeAllFields);
    EXPECT_FALSE(meta._trackListPositions);
    EXPECT_EQ(arangodb::iresearch::ValueStorage::NONE, meta._storeValues);
    EXPECT_EQ(1U, meta._analyzers.size());
    EXPECT_TRUE(*(meta._analyzers.begin()));
    EXPECT_EQ("identity", meta._analyzers.begin()->_pool->name());
    EXPECT_EQ("identity", meta._analyzers.begin()->_shortName);
    EXPECT_TRUE((irs::flags({irs::type<irs::norm>::get(), irs::type<irs::frequency>::get()}) ==
                 meta._analyzers.begin()->_pool->features()));
    EXPECT_FALSE(!meta._analyzers.begin()->_pool->get());
  }

  // with active vocbase
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
    arangodb::iresearch::IResearchLinkMeta meta;
    std::string tmpString;
    EXPECT_TRUE(meta.init(server.server(), json->slice(), false, tmpString, vocbase.name()));
    EXPECT_TRUE(meta._fields.empty());
    EXPECT_FALSE(meta._includeAllFields);
    EXPECT_FALSE(meta._trackListPositions);
    EXPECT_EQ(arangodb::iresearch::ValueStorage::NONE, meta._storeValues);
    EXPECT_EQ(1U, meta._analyzers.size());
    EXPECT_TRUE(*(meta._analyzers.begin()));
    EXPECT_EQ("identity", meta._analyzers.begin()->_pool->name());
    EXPECT_EQ("identity", meta._analyzers.begin()->_shortName);
    EXPECT_TRUE((irs::flags({irs::type<irs::norm>::get(), irs::type<irs::frequency>::get()}) ==
                 meta._analyzers.begin()->_pool->features()));
    EXPECT_FALSE(!meta._analyzers.begin()->_pool->get());
  }
}

TEST_F(IResearchLinkMetaTest, test_readCustomizedValues) {
  auto json = VPackParser::fromJson(
      "{ \
    \"fields\": { \
      \"a\": {}, \
      \"b\": {}, \
      \"c\": { \
        \"fields\": { \
          \"default\": { \"fields\": {}, \"includeAllFields\": false, \"trackListPositions\": false, \"storeValues\": \"none\", \"analyzers\": [ \"identity\" ] }, \
          \"all\": { \"fields\": {\"d\": {}, \"e\": {}}, \"includeAllFields\": true, \"trackListPositions\": true, \"storeValues\": \"value\", \"analyzers\": [ \"empty\" ] }, \
          \"some\": { \"trackListPositions\": true, \"storeValues\": \"id\" }, \
          \"none\": {} \
        } \
      } \
    }, \
    \"includeAllFields\": true, \
    \"trackListPositions\": true, \
    \"storeValues\": \"value\", \
    \"analyzers\": [ \"empty\", \"identity\" ]\
  }");

  // without active vocbase
  {
    arangodb::iresearch::IResearchLinkMeta meta;
    std::string tmpString;
    EXPECT_FALSE(meta.init(server.server(), json->slice(), false, tmpString));
  }

  // with active vocbase
  {
    std::unordered_set<std::string> expectedFields = {"a", "b", "c"};
    std::unordered_set<std::string> expectedOverrides = {"default", "all",
                                                         "some", "none"};
    std::unordered_set<std::string> expectedAnalyzers = {"empty", "identity"};
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
    arangodb::iresearch::IResearchLinkMeta meta;
    std::string tmpString;
    EXPECT_TRUE(meta.init(server.server(), json->slice(), false, tmpString, vocbase.name()));
    EXPECT_EQ(3U, meta._fields.size());

    for (auto& field : meta._fields) {
      EXPECT_EQ(1U, expectedFields.erase(static_cast<std::string>(field.key()))); // FIXME: after C++20 remove cast and use heterogeneous lookup

      for (auto& fieldOverride : field.value()->_fields) {
        auto& actual = *(fieldOverride.value());

        EXPECT_EQ(1U, expectedOverrides.erase(static_cast<std::string>(fieldOverride.key()))); // FIXME: after C++20 remove cast and use heterogeneous lookup

        if ("default" == fieldOverride.key()) {
          EXPECT_TRUE(actual._fields.empty());
          EXPECT_FALSE(actual._includeAllFields);
          EXPECT_FALSE(actual._trackListPositions);
          EXPECT_EQ(arangodb::iresearch::ValueStorage::NONE, actual._storeValues);
          EXPECT_EQ(1U, actual._analyzers.size());
          EXPECT_TRUE(*(actual._analyzers.begin()));
          EXPECT_EQ("identity", actual._analyzers.begin()->_pool->name());
          EXPECT_EQ("identity", actual._analyzers.begin()->_shortName);
          EXPECT_TRUE((irs::flags({irs::type<irs::norm>::get(), irs::type<irs::frequency>::get()}) ==
                       actual._analyzers.begin()->_pool->features()));
          EXPECT_FALSE(!actual._analyzers.begin()->_pool->get());
        } else if ("all" == fieldOverride.key()) {
          EXPECT_EQ(2U, actual._fields.size());
          EXPECT_TRUE((actual._fields.find("d") != actual._fields.end()));
          EXPECT_TRUE((actual._fields.find("e") != actual._fields.end()));
          EXPECT_TRUE(actual._includeAllFields);
          EXPECT_TRUE(actual._trackListPositions);
          EXPECT_EQ(arangodb::iresearch::ValueStorage::VALUE, actual._storeValues);
          EXPECT_EQ(1U, actual._analyzers.size());
          EXPECT_TRUE(*(actual._analyzers.begin()));
          EXPECT_EQ("testVocbase::empty", actual._analyzers.begin()->_pool->name());
          EXPECT_EQ("empty", actual._analyzers.begin()->_shortName);
          EXPECT_TRUE((irs::flags({irs::type<irs::frequency>::get()}) ==
                       actual._analyzers.begin()->_pool->features()));
          EXPECT_FALSE(!actual._analyzers.begin()->_pool->get());
        } else if ("some" == fieldOverride.key()) {
          EXPECT_TRUE(actual._fields.empty());    // not inherited
          EXPECT_TRUE(actual._includeAllFields);  // inherited
          EXPECT_TRUE(actual._trackListPositions);
          EXPECT_EQ(arangodb::iresearch::ValueStorage::ID, actual._storeValues);
          EXPECT_EQ(2U, actual._analyzers.size());
          auto itr = actual._analyzers.begin();
          EXPECT_TRUE(*itr);
          EXPECT_EQ("testVocbase::empty", itr->_pool->name());
          EXPECT_EQ("empty", itr->_shortName);
          EXPECT_EQ(irs::flags({irs::type<irs::frequency>::get()}), itr->_pool->features());
          EXPECT_FALSE(!itr->_pool->get());
          ++itr;
          EXPECT_TRUE(*itr);
          EXPECT_EQ("identity", itr->_pool->name());
          EXPECT_EQ("identity", itr->_shortName);
          EXPECT_TRUE((irs::flags({irs::type<irs::norm>::get(), irs::type<irs::frequency>::get()}) ==
                       itr->_pool->features()));
          EXPECT_FALSE(!itr->_pool->get());
        } else if ("none" == fieldOverride.key()) {
          EXPECT_TRUE(actual._fields.empty());      // not inherited
          EXPECT_TRUE(actual._includeAllFields);    // inherited
          EXPECT_TRUE(actual._trackListPositions);  // inherited
          EXPECT_EQ(arangodb::iresearch::ValueStorage::VALUE, actual._storeValues);
          auto itr = actual._analyzers.begin();
          EXPECT_TRUE(*itr);
          EXPECT_EQ("testVocbase::empty", itr->_pool->name());
          EXPECT_EQ("empty", itr->_shortName);
          EXPECT_EQ(irs::flags({irs::type<irs::frequency>::get()}), itr->_pool->features());
          EXPECT_FALSE(!itr->_pool->get());
          ++itr;
          EXPECT_TRUE(*itr);
          EXPECT_EQ("identity", itr->_pool->name());
          EXPECT_EQ("identity", itr->_shortName);
          EXPECT_TRUE((irs::flags({irs::type<irs::norm>::get(), irs::type<irs::frequency>::get()}) ==
                       itr->_pool->features()));
          EXPECT_FALSE(!itr->_pool->get());
        }
      }
    }

    EXPECT_TRUE(expectedOverrides.empty());
    EXPECT_TRUE(expectedFields.empty());
    EXPECT_TRUE(meta._includeAllFields);
    EXPECT_TRUE(meta._trackListPositions);
    EXPECT_EQ(arangodb::iresearch::ValueStorage::VALUE, meta._storeValues);
    auto itr = meta._analyzers.begin();
    EXPECT_TRUE(*itr);
    EXPECT_EQ("testVocbase::empty", itr->_pool->name());
    EXPECT_EQ("empty", itr->_shortName);
    EXPECT_EQ(irs::flags({irs::type<irs::frequency>::get()}), itr->_pool->features());
    EXPECT_FALSE(!itr->_pool->get());
    ++itr;
    EXPECT_TRUE(*itr);
    EXPECT_EQ("identity", itr->_pool->name());
    EXPECT_EQ("identity", itr->_shortName);
    EXPECT_TRUE((irs::flags({irs::type<irs::norm>::get(), irs::type<irs::frequency>::get()}) ==
                 itr->_pool->features()));
    EXPECT_FALSE(!itr->_pool->get());
  }
}

TEST_F(IResearchLinkMetaTest, test_readCustomizedValuesCluster) {
  auto oldRole = arangodb::ServerState::instance()->getRole();
  auto restoreRole  = irs::make_finally(
    [oldRole](){
      arangodb::ServerState::instance()->setRole(oldRole);});
  arangodb::ServerState::instance()->setRole(arangodb::ServerState::RoleEnum::ROLE_DBSERVER);
  auto json = VPackParser::fromJson(
      "{ \
    \"fields\": { \
      \"a\": {}, \
      \"b\": {}, \
      \"c\": { \
        \"fields\": { \
          \"default\": { \"fields\": {}, \"includeAllFields\": false, \"trackListPositions\": false, \"storeValues\": \"none\", \"analyzers\": [ \"identity\" ] }, \
          \"all\": { \"fields\": {\"d\": {}, \"e\": {}}, \"includeAllFields\": true, \"trackListPositions\": true, \"storeValues\": \"value\", \"analyzers\": [ \"empty\" ] }, \
          \"some\": { \"trackListPositions\": true, \"storeValues\": \"id\" }, \
          \"none\": {} \
        } \
      } \
    }, \
    \"includeAllFields\": true, \
    \"trackListPositions\": true, \
    \"storeValues\": \"value\", \
    \"analyzers\": [ \"empty\", \"identity\" ],\
    \"collectionName\":\"testCollection\"\
  }");

  // without active vocbase
  {
    arangodb::iresearch::IResearchLinkMeta meta;
    std::string tmpString;
    EXPECT_FALSE(meta.init(server.server(), json->slice(), false, tmpString));
  }

  // with active vocbase
  {
    std::unordered_set<std::string> expectedFields = {"a", "b", "c"};
    std::unordered_set<std::string> expectedOverrides = {"default", "all",
                                                         "some", "none"};
    std::unordered_set<std::string> expectedAnalyzers = {"empty", "identity"};
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
    arangodb::iresearch::IResearchLinkMeta meta;
    std::string tmpString;
    EXPECT_TRUE(meta.init(server.server(), json->slice(), false, tmpString, vocbase.name()));
    EXPECT_EQ(3U, meta._fields.size());

    for (auto& field : meta._fields) {
      EXPECT_EQ(1U, expectedFields.erase(static_cast<std::string>(field.key()))); // FIXME: after C++20 remove cast and use heterogeneous lookup

      for (auto& fieldOverride : field.value()->_fields) {
        auto& actual = *(fieldOverride.value());

        EXPECT_EQ(1U, expectedOverrides.erase(static_cast<std::string>(fieldOverride.key()))); // FIXME: after C++20 remove cast and use heterogeneous lookup

        if ("default" == fieldOverride.key()) {
          EXPECT_TRUE(actual._fields.empty());
          EXPECT_FALSE(actual._includeAllFields);
          EXPECT_FALSE(actual._trackListPositions);
          EXPECT_EQ(arangodb::iresearch::ValueStorage::NONE, actual._storeValues);
          EXPECT_EQ(1U, actual._analyzers.size());
          EXPECT_TRUE(*(actual._analyzers.begin()));
          EXPECT_EQ("identity", actual._analyzers.begin()->_pool->name());
          EXPECT_EQ("identity", actual._analyzers.begin()->_shortName);
          EXPECT_TRUE((irs::flags({irs::type<irs::norm>::get(), irs::type<irs::frequency>::get()}) ==
                       actual._analyzers.begin()->_pool->features()));
          EXPECT_FALSE(!actual._analyzers.begin()->_pool->get());
        } else if ("all" == fieldOverride.key()) {
          EXPECT_EQ(2U, actual._fields.size());
          EXPECT_TRUE((actual._fields.find("d") != actual._fields.end()));
          EXPECT_TRUE((actual._fields.find("e") != actual._fields.end()));
          EXPECT_TRUE(actual._includeAllFields);
          EXPECT_TRUE(actual._trackListPositions);
          EXPECT_EQ(arangodb::iresearch::ValueStorage::VALUE, actual._storeValues);
          EXPECT_EQ(1U, actual._analyzers.size());
          EXPECT_TRUE(*(actual._analyzers.begin()));
          EXPECT_EQ("testVocbase::empty", actual._analyzers.begin()->_pool->name());
          EXPECT_EQ("empty", actual._analyzers.begin()->_shortName);
          EXPECT_TRUE((irs::flags({irs::type<irs::frequency>::get()}) ==
                       actual._analyzers.begin()->_pool->features()));
          EXPECT_FALSE(!actual._analyzers.begin()->_pool->get());
        } else if ("some" == fieldOverride.key()) {
          EXPECT_TRUE(actual._fields.empty());    // not inherited
          EXPECT_TRUE(actual._includeAllFields);  // inherited
          EXPECT_TRUE(actual._trackListPositions);
          EXPECT_EQ(arangodb::iresearch::ValueStorage::ID, actual._storeValues);
          EXPECT_EQ(2U, actual._analyzers.size());
          auto itr = actual._analyzers.begin();
          EXPECT_TRUE(*itr);
          EXPECT_EQ("testVocbase::empty", itr->_pool->name());
          EXPECT_EQ("empty", itr->_shortName);
          EXPECT_EQ(irs::flags({irs::type<irs::frequency>::get()}), itr->_pool->features());
          EXPECT_FALSE(!itr->_pool->get());
          ++itr;
          EXPECT_TRUE(*itr);
          EXPECT_EQ("identity", itr->_pool->name());
          EXPECT_EQ("identity", itr->_shortName);
          EXPECT_TRUE((irs::flags({irs::type<irs::norm>::get(), irs::type<irs::frequency>::get()}) ==
                       itr->_pool->features()));
          EXPECT_FALSE(!itr->_pool->get());
        } else if ("none" == fieldOverride.key()) {
          EXPECT_TRUE(actual._fields.empty());      // not inherited
          EXPECT_TRUE(actual._includeAllFields);    // inherited
          EXPECT_TRUE(actual._trackListPositions);  // inherited
          EXPECT_EQ(arangodb::iresearch::ValueStorage::VALUE, actual._storeValues);
          auto itr = actual._analyzers.begin();
          EXPECT_TRUE(*itr);
          EXPECT_EQ("testVocbase::empty", itr->_pool->name());
          EXPECT_EQ("empty", itr->_shortName);
          EXPECT_EQ(irs::flags({irs::type<irs::frequency>::get()}), itr->_pool->features());
          EXPECT_FALSE(!itr->_pool->get());
          ++itr;
          EXPECT_TRUE(*itr);
          EXPECT_EQ("identity", itr->_pool->name());
          EXPECT_EQ("identity", itr->_shortName);
          EXPECT_TRUE((irs::flags({irs::type<irs::norm>::get(), irs::type<irs::frequency>::get()}) ==
                       itr->_pool->features()));
          EXPECT_FALSE(!itr->_pool->get());
        }
      }
    }

    EXPECT_TRUE(expectedOverrides.empty());
    EXPECT_TRUE(expectedFields.empty());
    EXPECT_TRUE(meta._includeAllFields);
    EXPECT_TRUE(meta._trackListPositions);
    EXPECT_EQ("testCollection", meta._collectionName);
    EXPECT_EQ(arangodb::iresearch::ValueStorage::VALUE, meta._storeValues);
    auto itr = meta._analyzers.begin();
    EXPECT_TRUE(*itr);
    EXPECT_EQ("testVocbase::empty", itr->_pool->name());
    EXPECT_EQ("empty", itr->_shortName);
    EXPECT_EQ(irs::flags({irs::type<irs::frequency>::get()}), itr->_pool->features());
    EXPECT_FALSE(!itr->_pool->get());
    ++itr;
    EXPECT_TRUE(*itr);
    EXPECT_EQ("identity", itr->_pool->name());
    EXPECT_EQ("identity", itr->_shortName);
    EXPECT_TRUE((irs::flags({irs::type<irs::norm>::get(), irs::type<irs::frequency>::get()}) ==
                 itr->_pool->features()));
    EXPECT_FALSE(!itr->_pool->get());
  }
}

TEST_F(IResearchLinkMetaTest, test_writeDefaults) {
  // without active vobcase (not fullAnalyzerDefinition)
  {
    arangodb::iresearch::IResearchLinkMeta meta;
    arangodb::velocypack::Builder builder;
    arangodb::velocypack::Slice tmpSlice;

    builder.openObject();
    EXPECT_TRUE(meta.json(server.server(), builder, false));
    builder.close();

    auto slice = builder.slice();

    EXPECT_EQ(5U, slice.length());
    tmpSlice = slice.get("fields");
    EXPECT_TRUE(tmpSlice.isObject() && 0 == tmpSlice.length());
    tmpSlice = slice.get("includeAllFields");
    EXPECT_TRUE(tmpSlice.isBool() && false == tmpSlice.getBool());
    tmpSlice = slice.get("trackListPositions");
    EXPECT_TRUE(tmpSlice.isBool() && false == tmpSlice.getBool());
    tmpSlice = slice.get("storeValues");
    EXPECT_TRUE(tmpSlice.isString() && std::string("none") == tmpSlice.copyString());
    tmpSlice = slice.get("analyzers");
    EXPECT_TRUE((true == tmpSlice.isArray() && 1 == tmpSlice.length() &&
                 tmpSlice.at(0).isString() &&
                 std::string("identity") == tmpSlice.at(0).copyString()));
  }

  // without active vobcase (with fullAnalyzerDefinition)
  {
    arangodb::iresearch::IResearchLinkMeta meta;
    arangodb::velocypack::Builder builder;
    arangodb::velocypack::Slice tmpSlice;

    builder.openObject();
    EXPECT_TRUE(meta.json(server.server(), builder, true));
    builder.close();

    auto slice = builder.slice();

    EXPECT_EQ(9, slice.length());
    tmpSlice = slice.get("fields");
    EXPECT_TRUE(tmpSlice.isObject() && 0 == tmpSlice.length());
    tmpSlice = slice.get("includeAllFields");
    EXPECT_TRUE(tmpSlice.isBool() && false == tmpSlice.getBool());
    tmpSlice = slice.get("trackListPositions");
    EXPECT_TRUE(tmpSlice.isBool() && false == tmpSlice.getBool());
    tmpSlice = slice.get("storeValues");
    EXPECT_TRUE(tmpSlice.isString() && std::string("none") == tmpSlice.copyString());
    tmpSlice = slice.get("analyzers");
    EXPECT_TRUE((true == tmpSlice.isArray() && 1 == tmpSlice.length() &&
                 tmpSlice.at(0).isString() &&
                 std::string("identity") == tmpSlice.at(0).copyString()));
    tmpSlice = slice.get("analyzerDefinitions");
    EXPECT_TRUE((true == tmpSlice.isArray() && 1 == tmpSlice.length() &&
                 tmpSlice.at(0).isObject() && tmpSlice.at(0).get("name").isString() &&
                 std::string("identity") == tmpSlice.at(0).get("name").copyString() &&
                 tmpSlice.at(0).get("type").isString() &&
                 std::string("identity") == tmpSlice.at(0).get("type").copyString() &&
                 tmpSlice.at(0).get("properties").isObject() &&
                 tmpSlice.at(0).get("features").isArray() &&
                 2 == tmpSlice.at(0).get("features").length()  // frequency+norm
                 ));
    EXPECT_EQUAL_SLICES(tmpSlice.at(0).get("properties"), VPackSlice::emptyObjectSlice());
    tmpSlice = slice.get("primarySort");
    EXPECT_TRUE(tmpSlice.isArray() && 0 == tmpSlice.length());
    tmpSlice = slice.get("storedValues");
    EXPECT_TRUE(tmpSlice.isArray() && 0 == tmpSlice.length());
  }

  // with active vocbase (not fullAnalyzerDefinition)
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
    arangodb::iresearch::IResearchLinkMeta meta;
    arangodb::velocypack::Builder builder;
    arangodb::velocypack::Slice tmpSlice;

    builder.openObject();
    EXPECT_TRUE(meta.json(server.server(), builder, false, nullptr, &vocbase));
    builder.close();

    auto slice = builder.slice();

    EXPECT_EQ(5U, slice.length());
    tmpSlice = slice.get("fields");
    EXPECT_TRUE(tmpSlice.isObject() && 0 == tmpSlice.length());
    tmpSlice = slice.get("includeAllFields");
    EXPECT_TRUE(tmpSlice.isBool() && false == tmpSlice.getBool());
    tmpSlice = slice.get("trackListPositions");
    EXPECT_TRUE(tmpSlice.isBool() && false == tmpSlice.getBool());
    tmpSlice = slice.get("storeValues");
    EXPECT_TRUE(tmpSlice.isString() && std::string("none") == tmpSlice.copyString());
    tmpSlice = slice.get("analyzers");
    EXPECT_TRUE((true == tmpSlice.isArray() && 1 == tmpSlice.length() &&
                 tmpSlice.at(0).isString() &&
                 std::string("identity") == tmpSlice.at(0).copyString()));
  }

  // with active vocbase (with fullAnalyzerDefinition)
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
    arangodb::iresearch::IResearchLinkMeta meta;
    arangodb::velocypack::Builder builder;
    arangodb::velocypack::Slice tmpSlice;

    builder.openObject();
    EXPECT_TRUE(meta.json(server.server(), builder, true, nullptr, &vocbase));
    builder.close();

    auto slice = builder.slice();

    EXPECT_EQ(9, slice.length());
    tmpSlice = slice.get("fields");
    EXPECT_TRUE(tmpSlice.isObject() && 0 == tmpSlice.length());
    tmpSlice = slice.get("includeAllFields");
    EXPECT_TRUE(tmpSlice.isBool() && false == tmpSlice.getBool());
    tmpSlice = slice.get("trackListPositions");
    EXPECT_TRUE(tmpSlice.isBool() && false == tmpSlice.getBool());
    tmpSlice = slice.get("storeValues");
    EXPECT_TRUE(tmpSlice.isString() && std::string("none") == tmpSlice.copyString());
    tmpSlice = slice.get("analyzers");
    EXPECT_TRUE((true == tmpSlice.isArray() && 1 == tmpSlice.length() &&
                 tmpSlice.at(0).isString() &&
                 std::string("identity") == tmpSlice.at(0).copyString()));
    tmpSlice = slice.get("analyzerDefinitions");
    EXPECT_TRUE((true == tmpSlice.isArray() && 1 == tmpSlice.length() &&
                 tmpSlice.at(0).isObject() && tmpSlice.at(0).get("name").isString() &&
                 std::string("identity") == tmpSlice.at(0).get("name").copyString() &&
                 tmpSlice.at(0).get("type").isString() &&
                 std::string("identity") == tmpSlice.at(0).get("type").copyString() &&
                 tmpSlice.at(0).get("properties").isObject() &&
                 tmpSlice.at(0).get("features").isArray() &&
                 2 == tmpSlice.at(0).get("features").length()  // frequency+norm
                 ));
    EXPECT_EQUAL_SLICES(tmpSlice.at(0).get("properties"), VPackSlice::emptyObjectSlice());
    tmpSlice = slice.get("primarySort");
    EXPECT_TRUE(tmpSlice.isArray() && 0 == tmpSlice.length());
    tmpSlice = slice.get("primarySortCompression");
    EXPECT_TRUE(tmpSlice.isString() && tmpSlice.copyString() == "lz4");
    tmpSlice = slice.get("storedValues");
    EXPECT_TRUE(tmpSlice.isArray() && 0 == tmpSlice.length());
  }
}

TEST_F(IResearchLinkMetaTest, test_writeCustomizedValues) {
  arangodb::iresearch::IResearchAnalyzerFeature analyzers(server.server());
  analyzers.prepare();  // add static analyzers
  arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult emplaceResult;
  arangodb::iresearch::IResearchLinkMeta meta;

  analyzers.emplace(emplaceResult,
                    arangodb::StaticStrings::SystemDatabase + "::empty", "empty",
                    VPackParser::fromJson("{ \"args\": \"en\" }")->slice(),
                    {irs::type<irs::frequency>::get()});

  meta._includeAllFields = true;
  meta._trackListPositions = true;
  meta._storeValues = arangodb::iresearch::ValueStorage::VALUE;
  meta._analyzerDefinitions.clear();
  meta._analyzerDefinitions.emplace(analyzers.get("identity", arangodb::QueryAnalyzerRevisions::QUERY_LATEST));
  meta._analyzerDefinitions.emplace(analyzers.get(arangodb::StaticStrings::SystemDatabase + "::empty", 
                                                  arangodb::QueryAnalyzerRevisions::QUERY_LATEST));
  meta._analyzers.clear();
  meta._analyzers.emplace_back(arangodb::iresearch::IResearchLinkMeta::Analyzer(
      analyzers.get("identity", arangodb::QueryAnalyzerRevisions::QUERY_LATEST), "identity"));
  meta._analyzers.emplace_back(arangodb::iresearch::IResearchLinkMeta::Analyzer(
      analyzers.get(arangodb::StaticStrings::SystemDatabase + "::empty", arangodb::QueryAnalyzerRevisions::QUERY_LATEST),
      "empty"));
  meta._fields["a"] = meta;            // copy from meta
  meta._fields["a"]->_fields.clear();  // do not inherit fields to match jSon inheritance
  meta._fields["b"] = meta;            // copy from meta
  meta._fields["b"]->_fields.clear();  // do not inherit fields to match jSon inheritance
  meta._fields["c"] = meta;            // copy from meta
  meta._fields["c"]->_fields.clear();  // do not inherit fields to match jSon inheritance
  meta._fields["c"]->_fields["default"];  // default values
  meta._fields["c"]->_fields["all"];      // will override values below
  meta._fields["c"]->_fields["some"] =
      meta._fields["c"];  // initialize with parent, override below
  meta._fields["c"]->_fields["none"] = meta._fields["c"];  // initialize with parent
  meta._sort.emplace_back({arangodb::basics::AttributeName("_key", false)}, true);
  meta._sort.emplace_back({arangodb::basics::AttributeName("_id", false)}, false);

  auto& overrideAll = *(meta._fields["c"]->_fields["all"]);
  auto& overrideSome = *(meta._fields["c"]->_fields["some"]);
  auto& overrideNone = *(meta._fields["c"]->_fields["none"]);

  overrideAll._fields.clear();  // do not inherit fields to match jSon inheritance
  overrideAll._fields["x"] = arangodb::iresearch::IResearchLinkMeta();
  overrideAll._fields["y"] = arangodb::iresearch::IResearchLinkMeta();
  overrideAll._includeAllFields = false;
  overrideAll._trackListPositions = false;
  overrideAll._storeValues = arangodb::iresearch::ValueStorage::NONE;
  overrideAll._analyzers.clear();
  overrideAll._analyzers.emplace_back(arangodb::iresearch::IResearchLinkMeta::Analyzer(
      analyzers.get(arangodb::StaticStrings::SystemDatabase + "::empty", arangodb::QueryAnalyzerRevisions::QUERY_LATEST),
      "empty"));
  overrideSome._fields.clear();  // do not inherit fields to match jSon inheritance
  overrideSome._trackListPositions = false;
  overrideSome._storeValues = arangodb::iresearch::ValueStorage::ID;
  overrideNone._fields.clear();  // do not inherit fields to match jSon inheritance

  // without active vobcase (not fullAnalyzerDefinition)
  {
    std::unordered_set<std::string> expectedFields = {"a", "b", "c"};
    std::unordered_set<std::string> expectedOverrides = {"default", "all",
                                                         "some", "none"};
    std::unordered_set<std::string> expectedAnalyzers = {
        arangodb::StaticStrings::SystemDatabase + "::empty", "identity"};
    arangodb::velocypack::Builder builder;
    arangodb::velocypack::Slice tmpSlice;

    builder.openObject();
    EXPECT_TRUE(meta.json(server.server(), builder, false));
    builder.close();

    auto slice = builder.slice();

    EXPECT_EQ(5U, slice.length());
    tmpSlice = slice.get("fields");
    EXPECT_TRUE(tmpSlice.isObject() && 3 == tmpSlice.length());

    for (arangodb::velocypack::ObjectIterator itr(tmpSlice); itr.valid(); ++itr) {
      auto key = itr.key();
      auto value = itr.value();
      EXPECT_TRUE(key.isString() && 1 == expectedFields.erase(key.copyString()));
      EXPECT_TRUE(value.isObject());

      if (!value.hasKey("fields")) {
        continue;
      }

      tmpSlice = value.get("fields");

      for (arangodb::velocypack::ObjectIterator overrideItr(tmpSlice);
           overrideItr.valid(); ++overrideItr) {
        auto fieldOverride = overrideItr.key();
        auto sliceOverride = overrideItr.value();
        EXPECT_TRUE(fieldOverride.isString() && sliceOverride.isObject());
        EXPECT_EQ(1U, expectedOverrides.erase(fieldOverride.copyString()));

        if ("default" == fieldOverride.copyString()) {
          EXPECT_EQ(4U, sliceOverride.length());
          tmpSlice = sliceOverride.get("includeAllFields");
          EXPECT_TRUE((false == tmpSlice.getBool()));
          tmpSlice = sliceOverride.get("trackListPositions");
          EXPECT_TRUE((false == tmpSlice.getBool()));
          tmpSlice = sliceOverride.get("storeValues");
          EXPECT_TRUE((true == tmpSlice.isString() &&
                       std::string("none") == tmpSlice.copyString()));
          tmpSlice = sliceOverride.get("analyzers");
          EXPECT_TRUE((true == tmpSlice.isArray() && 1 == tmpSlice.length() &&
                       tmpSlice.at(0).isString() &&
                       std::string("identity") == tmpSlice.at(0).copyString()));
        } else if ("all" == fieldOverride.copyString()) {
          std::unordered_set<std::string> expectedFields = {"x", "y"};
          EXPECT_EQ(5U, sliceOverride.length());
          tmpSlice = sliceOverride.get("fields");
          EXPECT_TRUE(tmpSlice.isObject() && 2 == tmpSlice.length());
          for (arangodb::velocypack::ObjectIterator overrideFieldItr(tmpSlice);
               overrideFieldItr.valid(); ++overrideFieldItr) {
            EXPECT_TRUE((true == overrideFieldItr.key().isString() &&
                         1 == expectedFields.erase(overrideFieldItr.key().copyString())));
          }
          EXPECT_TRUE(expectedFields.empty());
          tmpSlice = sliceOverride.get("includeAllFields");
          EXPECT_TRUE(tmpSlice.isBool() && false == tmpSlice.getBool());
          tmpSlice = sliceOverride.get("trackListPositions");
          EXPECT_TRUE(tmpSlice.isBool() && false == tmpSlice.getBool());
          tmpSlice = sliceOverride.get("storeValues");
          EXPECT_TRUE((true == tmpSlice.isString() &&
                       std::string("none") == tmpSlice.copyString()));
          tmpSlice = sliceOverride.get("analyzers");
          EXPECT_TRUE((true == tmpSlice.isArray() && 1 == tmpSlice.length() &&
                       tmpSlice.at(0).isString() &&
                       std::string(arangodb::StaticStrings::SystemDatabase +
                                   "::empty") == tmpSlice.at(0).copyString()));
        } else if ("some" == fieldOverride.copyString()) {
          EXPECT_EQ(2U, sliceOverride.length());
          tmpSlice = sliceOverride.get("trackListPositions");
          EXPECT_TRUE(tmpSlice.isBool() && false == tmpSlice.getBool());
          tmpSlice = sliceOverride.get("storeValues");
          EXPECT_TRUE((true == tmpSlice.isString() &&
                       std::string("id") == tmpSlice.copyString()));
        } else if ("none" == fieldOverride.copyString()) {
          EXPECT_EQ(0U, sliceOverride.length());
        }
      }
    }

    EXPECT_TRUE(expectedOverrides.empty());
    EXPECT_TRUE(expectedFields.empty());
    tmpSlice = slice.get("includeAllFields");
    EXPECT_TRUE(tmpSlice.isBool() && true == tmpSlice.getBool());
    tmpSlice = slice.get("trackListPositions");
    EXPECT_TRUE(tmpSlice.isBool() && true == tmpSlice.getBool());
    tmpSlice = slice.get("storeValues");
    EXPECT_TRUE(tmpSlice.isString() && std::string("value") == tmpSlice.copyString());
    tmpSlice = slice.get("analyzers");
    EXPECT_TRUE(tmpSlice.isArray() && 2 == tmpSlice.length());

    for (arangodb::velocypack::ArrayIterator analyzersItr(tmpSlice);
         analyzersItr.valid(); ++analyzersItr) {
      auto key = *analyzersItr;
      EXPECT_TRUE(key.isString() && 1 == expectedAnalyzers.erase(key.copyString()));
    }

    EXPECT_TRUE(expectedAnalyzers.empty());
  }

  // without active vobcase (with fullAnalyzerDefinition)
  {
    std::unordered_set<std::string> expectedFields = {"a", "b", "c"};
    std::unordered_set<std::string> expectedOverrides = {"default", "all",
                                                         "some", "none"};
    std::unordered_set<std::string> expectedAnalyzers = {
        arangodb::StaticStrings::SystemDatabase + "::empty", "identity"};
    std::set<std::pair<std::string, std::string>> expectedAnalyzerDefinitions = {
        {
          arangodb::StaticStrings::SystemDatabase + "::empty",
          VPackParser::fromJson("{\"args\":\"en\"}")->slice().toString()
        },
        {"identity", VPackSlice::emptyObjectSlice().toString()},
    };
    arangodb::velocypack::Builder builder;
    arangodb::velocypack::Slice tmpSlice;

    builder.openObject();
    EXPECT_TRUE(meta.json(server.server(), builder, true));
    builder.close();

    auto slice = builder.slice();

    EXPECT_EQ(9, slice.length());
    tmpSlice = slice.get("fields");
    EXPECT_TRUE(tmpSlice.isObject() && 3 == tmpSlice.length());

    for (arangodb::velocypack::ObjectIterator itr(tmpSlice); itr.valid(); ++itr) {
      auto key = itr.key();
      auto value = itr.value();
      EXPECT_TRUE(key.isString() && 1 == expectedFields.erase(key.copyString()));
      EXPECT_TRUE(value.isObject());

      if (!value.hasKey("fields")) {
        continue;
      }

      tmpSlice = value.get("fields");

      for (arangodb::velocypack::ObjectIterator overrideItr(tmpSlice);
           overrideItr.valid(); ++overrideItr) {
        auto fieldOverride = overrideItr.key();
        auto sliceOverride = overrideItr.value();
        EXPECT_TRUE(fieldOverride.isString() && sliceOverride.isObject());
        EXPECT_EQ(1U, expectedOverrides.erase(fieldOverride.copyString()));

        if ("default" == fieldOverride.copyString()) {
          EXPECT_EQ(4U, sliceOverride.length());
          tmpSlice = sliceOverride.get("includeAllFields");
          EXPECT_TRUE((false == tmpSlice.getBool()));
          tmpSlice = sliceOverride.get("trackListPositions");
          EXPECT_TRUE((false == tmpSlice.getBool()));
          tmpSlice = sliceOverride.get("storeValues");
          EXPECT_TRUE((true == tmpSlice.isString() &&
                       std::string("none") == tmpSlice.copyString()));
          tmpSlice = sliceOverride.get("analyzers");
          EXPECT_TRUE((true == tmpSlice.isArray() && 1 == tmpSlice.length() &&
                       tmpSlice.at(0).isString() &&
                       std::string("identity") == tmpSlice.at(0).copyString()));
        } else if ("all" == fieldOverride.copyString()) {
          std::unordered_set<std::string> expectedFields = {"x", "y"};
          EXPECT_EQ(5U, sliceOverride.length());
          tmpSlice = sliceOverride.get("fields");
          EXPECT_TRUE(tmpSlice.isObject() && 2 == tmpSlice.length());
          for (arangodb::velocypack::ObjectIterator overrideFieldItr(tmpSlice);
               overrideFieldItr.valid(); ++overrideFieldItr) {
            EXPECT_TRUE((true == overrideFieldItr.key().isString() &&
                         1 == expectedFields.erase(overrideFieldItr.key().copyString())));
          }
          EXPECT_TRUE(expectedFields.empty());
          tmpSlice = sliceOverride.get("includeAllFields");
          EXPECT_TRUE(tmpSlice.isBool() && false == tmpSlice.getBool());
          tmpSlice = sliceOverride.get("trackListPositions");
          EXPECT_TRUE(tmpSlice.isBool() && false == tmpSlice.getBool());
          tmpSlice = sliceOverride.get("storeValues");
          EXPECT_TRUE((true == tmpSlice.isString() &&
                       std::string("none") == tmpSlice.copyString()));
          tmpSlice = sliceOverride.get("analyzers");
          EXPECT_TRUE((true == tmpSlice.isArray() && 1 == tmpSlice.length() &&
                       tmpSlice.at(0).isString() &&
                       arangodb::StaticStrings::SystemDatabase + "::empty" ==
                           tmpSlice.at(0).copyString()));
        } else if ("some" == fieldOverride.copyString()) {
          EXPECT_EQ(2U, sliceOverride.length());
          tmpSlice = sliceOverride.get("trackListPositions");
          EXPECT_TRUE(tmpSlice.isBool() && false == tmpSlice.getBool());
          tmpSlice = sliceOverride.get("storeValues");
          EXPECT_TRUE((true == tmpSlice.isString() &&
                       std::string("id") == tmpSlice.copyString()));
        } else if ("none" == fieldOverride.copyString()) {
          EXPECT_EQ(0U, sliceOverride.length());
        }
      }
    }

    EXPECT_TRUE(expectedOverrides.empty());
    EXPECT_TRUE(expectedFields.empty());
    tmpSlice = slice.get("includeAllFields");
    EXPECT_TRUE(tmpSlice.isBool() && true == tmpSlice.getBool());
    tmpSlice = slice.get("trackListPositions");
    EXPECT_TRUE(tmpSlice.isBool() && true == tmpSlice.getBool());
    tmpSlice = slice.get("storeValues");
    EXPECT_TRUE(tmpSlice.isString() && std::string("value") == tmpSlice.copyString());
    tmpSlice = slice.get("analyzers");
    EXPECT_TRUE(tmpSlice.isArray() && 2 == tmpSlice.length());

    for (arangodb::velocypack::ArrayIterator analyzersItr(tmpSlice);
         analyzersItr.valid(); ++analyzersItr) {
      auto key = *analyzersItr;
      EXPECT_TRUE(key.isString() && 1 == expectedAnalyzers.erase(key.copyString()));
    }

    EXPECT_TRUE(expectedAnalyzers.empty());
    tmpSlice = slice.get("analyzerDefinitions");
    EXPECT_TRUE(tmpSlice.isArray() && 2 == tmpSlice.length());

    for (arangodb::velocypack::ArrayIterator analyzersItr(tmpSlice);
         analyzersItr.valid(); ++analyzersItr) {
      auto value = *analyzersItr;
      EXPECT_TRUE(
          (true == value.isObject() && value.hasKey("name") &&
           value.get("name").isString() && value.hasKey("type") &&
           value.get("type").isString() && value.hasKey("properties") &&
           (value.get("properties").isObject() || value.get("properties").isNull()) &&
           value.hasKey("features") && value.get("features").isArray() &&
           (1 == value.get("features").length() || 2 == value.get("features").length())  // empty/identity 1/2
           && 1 == expectedAnalyzerDefinitions.erase(
                       std::make_pair(value.get("name").copyString(),
                                      value.get("properties").isNull()
                                          ? ""
                                          : value.get("properties").toString()))));
    }

    EXPECT_TRUE(expectedAnalyzerDefinitions.empty());

    std::string errorField;
    tmpSlice = slice.get("primarySort");
    EXPECT_TRUE(tmpSlice.isArray());
    arangodb::iresearch::IResearchViewSort sort;
    EXPECT_TRUE(sort.fromVelocyPack(tmpSlice, errorField));
    EXPECT_EQ(2, sort.size());
    EXPECT_TRUE(sort.direction(0));
    EXPECT_TRUE((std::vector<arangodb::basics::AttributeName>{{"_key", false}} ==
                 sort.field(0)));
    EXPECT_FALSE(sort.direction(1));
    EXPECT_TRUE((std::vector<arangodb::basics::AttributeName>{{"_id", false}} ==
                 sort.field(1)));
  }

  // with active vocbase (not fullAnalyzerDefinition)
  {
    std::unordered_set<std::string> expectedFields = {"a", "b", "c"};
    std::unordered_set<std::string> expectedOverrides = {"default", "all",
                                                         "some", "none"};
    std::unordered_set<std::string> expectedAnalyzers = {"::empty", "identity"};
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
    arangodb::velocypack::Builder builder;
    arangodb::velocypack::Slice tmpSlice;

    builder.openObject();
    EXPECT_TRUE(meta.json(server.server(), builder, false, nullptr, &vocbase));
    builder.close();

    auto slice = builder.slice();

    EXPECT_EQ(5U, slice.length());
    tmpSlice = slice.get("fields");
    EXPECT_TRUE(tmpSlice.isObject() && 3 == tmpSlice.length());

    for (arangodb::velocypack::ObjectIterator itr(tmpSlice); itr.valid(); ++itr) {
      auto key = itr.key();
      auto value = itr.value();
      EXPECT_TRUE(key.isString() && 1 == expectedFields.erase(key.copyString()));
      EXPECT_TRUE(value.isObject());

      if (!value.hasKey("fields")) {
        continue;
      }

      tmpSlice = value.get("fields");

      for (arangodb::velocypack::ObjectIterator overrideItr(tmpSlice);
           overrideItr.valid(); ++overrideItr) {
        auto fieldOverride = overrideItr.key();
        auto sliceOverride = overrideItr.value();
        EXPECT_TRUE(fieldOverride.isString() && sliceOverride.isObject());
        EXPECT_EQ(1U, expectedOverrides.erase(fieldOverride.copyString()));

        if ("default" == fieldOverride.copyString()) {
          EXPECT_EQ(4U, sliceOverride.length());
          tmpSlice = sliceOverride.get("includeAllFields");
          EXPECT_TRUE((false == tmpSlice.getBool()));
          tmpSlice = sliceOverride.get("trackListPositions");
          EXPECT_TRUE((false == tmpSlice.getBool()));
          tmpSlice = sliceOverride.get("storeValues");
          EXPECT_TRUE((true == tmpSlice.isString() &&
                       std::string("none") == tmpSlice.copyString()));
          tmpSlice = sliceOverride.get("analyzers");
          EXPECT_TRUE((true == tmpSlice.isArray() && 1 == tmpSlice.length() &&
                       tmpSlice.at(0).isString() &&
                       std::string("identity") == tmpSlice.at(0).copyString()));
        } else if ("all" == fieldOverride.copyString()) {
          std::unordered_set<std::string> expectedFields = {"x", "y"};
          EXPECT_EQ(5U, sliceOverride.length());
          tmpSlice = sliceOverride.get("fields");
          EXPECT_TRUE(tmpSlice.isObject() && 2 == tmpSlice.length());
          for (arangodb::velocypack::ObjectIterator overrideFieldItr(tmpSlice);
               overrideFieldItr.valid(); ++overrideFieldItr) {
            EXPECT_TRUE((true == overrideFieldItr.key().isString() &&
                         1 == expectedFields.erase(overrideFieldItr.key().copyString())));
          }
          EXPECT_TRUE(expectedFields.empty());
          tmpSlice = sliceOverride.get("includeAllFields");
          EXPECT_TRUE(tmpSlice.isBool() && false == tmpSlice.getBool());
          tmpSlice = sliceOverride.get("trackListPositions");
          EXPECT_TRUE(tmpSlice.isBool() && false == tmpSlice.getBool());
          tmpSlice = sliceOverride.get("storeValues");
          EXPECT_TRUE((true == tmpSlice.isString() &&
                       std::string("none") == tmpSlice.copyString()));
          tmpSlice = sliceOverride.get("analyzers");
          EXPECT_TRUE((true == tmpSlice.isArray() && 1 == tmpSlice.length() &&
                       tmpSlice.at(0).isString() &&
                       std::string("::empty") == tmpSlice.at(0).copyString()));
        } else if ("some" == fieldOverride.copyString()) {
          EXPECT_EQ(2U, sliceOverride.length());
          tmpSlice = sliceOverride.get("trackListPositions");
          EXPECT_TRUE(tmpSlice.isBool() && false == tmpSlice.getBool());
          tmpSlice = sliceOverride.get("storeValues");
          EXPECT_TRUE((true == tmpSlice.isString() &&
                       std::string("id") == tmpSlice.copyString()));
        } else if ("none" == fieldOverride.copyString()) {
          EXPECT_EQ(0U, sliceOverride.length());
        }
      }
    }

    EXPECT_TRUE(expectedOverrides.empty());
    EXPECT_TRUE(expectedFields.empty());
    tmpSlice = slice.get("includeAllFields");
    EXPECT_TRUE(tmpSlice.isBool() && true == tmpSlice.getBool());
    tmpSlice = slice.get("trackListPositions");
    EXPECT_TRUE(tmpSlice.isBool() && true == tmpSlice.getBool());
    tmpSlice = slice.get("storeValues");
    EXPECT_TRUE(tmpSlice.isString() && std::string("value") == tmpSlice.copyString());
    tmpSlice = slice.get("analyzers");
    EXPECT_TRUE(tmpSlice.isArray() && 2 == tmpSlice.length());

    for (arangodb::velocypack::ArrayIterator analyzersItr(tmpSlice);
         analyzersItr.valid(); ++analyzersItr) {
      auto key = *analyzersItr;
      EXPECT_TRUE(key.isString() && 1 == expectedAnalyzers.erase(key.copyString()));
    }

    EXPECT_TRUE(expectedAnalyzers.empty());
  }

  // with active vocbase (with fullAnalyzerDefinition)
  {
    std::unordered_set<std::string> expectedFields = {"a", "b", "c"};
    std::unordered_set<std::string> expectedOverrides = {"default", "all",
                                                         "some", "none"};
    std::unordered_set<std::string> expectedAnalyzers = {
        "::empty", "identity"};
    std::set<std::pair<std::string, std::string>> expectedAnalyzerDefinitions = {
        {"::empty", VPackParser::fromJson("{\"args\":\"en\"}")->slice().toString()},
        {"identity", VPackSlice::emptyObjectSlice().toString()},
    };
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
    arangodb::velocypack::Builder builder;
    arangodb::velocypack::Slice tmpSlice;

    builder.openObject();
    EXPECT_TRUE(meta.json(server.server(), builder, true, nullptr, &vocbase));
    builder.close();

    auto slice = builder.slice();

    EXPECT_EQ(9, slice.length());
    tmpSlice = slice.get("fields");
    EXPECT_TRUE(tmpSlice.isObject() && 3 == tmpSlice.length());

    for (arangodb::velocypack::ObjectIterator itr(tmpSlice); itr.valid(); ++itr) {
      auto key = itr.key();
      auto value = itr.value();
      EXPECT_TRUE(key.isString() && 1 == expectedFields.erase(key.copyString()));
      EXPECT_TRUE(value.isObject());

      if (!value.hasKey("fields")) {
        continue;
      }

      tmpSlice = value.get("fields");

      for (arangodb::velocypack::ObjectIterator overrideItr(tmpSlice);
           overrideItr.valid(); ++overrideItr) {
        auto fieldOverride = overrideItr.key();
        auto sliceOverride = overrideItr.value();
        EXPECT_TRUE(fieldOverride.isString() && sliceOverride.isObject());
        EXPECT_EQ(1U, expectedOverrides.erase(fieldOverride.copyString()));

        if ("default" == fieldOverride.copyString()) {
          EXPECT_EQ(4U, sliceOverride.length());
          tmpSlice = sliceOverride.get("includeAllFields");
          EXPECT_TRUE((false == tmpSlice.getBool()));
          tmpSlice = sliceOverride.get("trackListPositions");
          EXPECT_TRUE((false == tmpSlice.getBool()));
          tmpSlice = sliceOverride.get("storeValues");
          EXPECT_TRUE((true == tmpSlice.isString() &&
                       std::string("none") == tmpSlice.copyString()));
          tmpSlice = sliceOverride.get("analyzers");
          EXPECT_TRUE((true == tmpSlice.isArray() && 1 == tmpSlice.length() &&
                       tmpSlice.at(0).isString() &&
                       std::string("identity") == tmpSlice.at(0).copyString()));
        } else if ("all" == fieldOverride.copyString()) {
          std::unordered_set<std::string> expectedFields = {"x", "y"};
          EXPECT_EQ(5U, sliceOverride.length());
          tmpSlice = sliceOverride.get("fields");
          EXPECT_TRUE(tmpSlice.isObject() && 2 == tmpSlice.length());
          for (arangodb::velocypack::ObjectIterator overrideFieldItr(tmpSlice);
               overrideFieldItr.valid(); ++overrideFieldItr) {
            EXPECT_TRUE((true == overrideFieldItr.key().isString() &&
                         1 == expectedFields.erase(overrideFieldItr.key().copyString())));
          }
          EXPECT_TRUE(expectedFields.empty());
          tmpSlice = sliceOverride.get("includeAllFields");
          EXPECT_TRUE(tmpSlice.isBool() && false == tmpSlice.getBool());
          tmpSlice = sliceOverride.get("trackListPositions");
          EXPECT_TRUE(tmpSlice.isBool() && false == tmpSlice.getBool());
          tmpSlice = sliceOverride.get("storeValues");
          EXPECT_TRUE((true == tmpSlice.isString() &&
                       std::string("none") == tmpSlice.copyString()));
          tmpSlice = sliceOverride.get("analyzers");
          EXPECT_TRUE((true == tmpSlice.isArray() && 1 == tmpSlice.length() &&
                       tmpSlice.at(0).isString() &&
                       "::empty" == tmpSlice.at(0).copyString()));
        } else if ("some" == fieldOverride.copyString()) {
          EXPECT_EQ(2U, sliceOverride.length());
          tmpSlice = sliceOverride.get("trackListPositions");
          EXPECT_TRUE(tmpSlice.isBool() && false == tmpSlice.getBool());
          tmpSlice = sliceOverride.get("storeValues");
          EXPECT_TRUE((true == tmpSlice.isString() &&
                       std::string("id") == tmpSlice.copyString()));
        } else if ("none" == fieldOverride.copyString()) {
          EXPECT_EQ(0U, sliceOverride.length());
        }
      }
    }

    EXPECT_TRUE(expectedOverrides.empty());
    EXPECT_TRUE(expectedFields.empty());
    tmpSlice = slice.get("includeAllFields");
    EXPECT_TRUE(tmpSlice.isBool() && true == tmpSlice.getBool());
    tmpSlice = slice.get("trackListPositions");
    EXPECT_TRUE(tmpSlice.isBool() && true == tmpSlice.getBool());
    tmpSlice = slice.get("storeValues");
    EXPECT_TRUE(tmpSlice.isString() && std::string("value") == tmpSlice.copyString());
    tmpSlice = slice.get("analyzers");
    EXPECT_TRUE(tmpSlice.isArray() && 2 == tmpSlice.length());

    for (arangodb::velocypack::ArrayIterator analyzersItr(tmpSlice);
         analyzersItr.valid(); ++analyzersItr) {
      auto key = *analyzersItr;
      EXPECT_TRUE(key.isString() && 1 == expectedAnalyzers.erase(key.copyString()));
    }

    EXPECT_TRUE(expectedAnalyzers.empty());
    tmpSlice = slice.get("analyzerDefinitions");
    EXPECT_TRUE(tmpSlice.isArray() && 2 == tmpSlice.length());

    for (arangodb::velocypack::ArrayIterator analyzersItr(tmpSlice);
         analyzersItr.valid(); ++analyzersItr) {
      auto value = *analyzersItr;
      EXPECT_TRUE(
          (true == value.isObject() && value.hasKey("name") &&
           value.get("name").isString() && value.hasKey("type") &&
           value.get("type").isString() && value.hasKey("properties") &&
           (value.get("properties").isObject() || value.get("properties").isNull()) &&
           value.hasKey("features") && value.get("features").isArray() &&
           (1 == value.get("features").length() || 2 == value.get("features").length())  // empty/identity 1/2
           && 1 == expectedAnalyzerDefinitions.erase(
                       std::make_pair(value.get("name").copyString(),
                                      value.get("properties").isNull()
                                          ? ""
                                          : value.get("properties").toString()))));
    }

    EXPECT_TRUE(expectedAnalyzerDefinitions.empty());

    std::string errorField;
    tmpSlice = slice.get("primarySort");
    EXPECT_TRUE(tmpSlice.isArray());
    EXPECT_EQ(2, tmpSlice.length());

    {
      auto valueSlice = tmpSlice.at(0);
      EXPECT_TRUE(valueSlice.isObject());
      EXPECT_EQ(2, valueSlice.length());
      EXPECT_TRUE(valueSlice.get("field").isString());
      EXPECT_EQ("_key", valueSlice.get("field").copyString());
      EXPECT_TRUE(valueSlice.get("asc").isBool());
      EXPECT_TRUE(valueSlice.get("asc").getBool());
    }

    {
      auto valueSlice = tmpSlice.at(1);
      EXPECT_TRUE(valueSlice.isObject());
      EXPECT_EQ(2, valueSlice.length());
      EXPECT_TRUE(valueSlice.get("field").isString());
      EXPECT_EQ("_id", valueSlice.get("field").copyString());
      EXPECT_TRUE(valueSlice.get("asc").isBool());
      EXPECT_FALSE(valueSlice.get("asc").getBool());
    }
  }
}

TEST_F(IResearchLinkMetaTest, test_readMaskAll) {
  arangodb::iresearch::IResearchLinkMeta meta;
  arangodb::iresearch::IResearchLinkMeta::Mask mask;
  std::string tmpString;

  auto json = VPackParser::fromJson(
      "{ \
    \"fields\": { \"a\": {} }, \
    \"includeAllFields\": true, \
    \"trackListPositions\": true, \
    \"storeValues\": \"value\", \
    \"analyzers\": [] \
  }");
  EXPECT_TRUE(true == meta.init(server.server(), json->slice(), false, tmpString, nullptr,
                                arangodb::iresearch::IResearchLinkMeta::DEFAULT(), &mask));
  EXPECT_TRUE(mask._fields);
  EXPECT_TRUE(mask._includeAllFields);
  EXPECT_TRUE(mask._trackListPositions);
  EXPECT_TRUE(mask._storeValues);
  EXPECT_TRUE(mask._analyzers);
}

TEST_F(IResearchLinkMetaTest, test_readMaskNone) {
  arangodb::iresearch::IResearchLinkMeta meta;
  arangodb::iresearch::IResearchLinkMeta::Mask mask;
  std::string tmpString;

  auto json = VPackParser::fromJson("{}");
  EXPECT_TRUE(true == meta.init(server.server(), json->slice(), false, tmpString, nullptr,
                                arangodb::iresearch::IResearchLinkMeta::DEFAULT(), &mask));
  EXPECT_FALSE(mask._fields);
  EXPECT_FALSE(mask._includeAllFields);
  EXPECT_FALSE(mask._trackListPositions);
  EXPECT_FALSE(mask._storeValues);
  EXPECT_FALSE(mask._analyzers);
}

TEST_F(IResearchLinkMetaTest, test_writeMaskAll) {
  // not fullAnalyzerDefinition
  {
    arangodb::iresearch::IResearchLinkMeta meta;
    arangodb::iresearch::IResearchLinkMeta::Mask mask(true);
    arangodb::velocypack::Builder builder;

    builder.openObject();
    EXPECT_TRUE(meta.json(server.server(), builder, false, nullptr, nullptr, &mask));
    builder.close();

    auto slice = builder.slice();

    EXPECT_EQ(5U, slice.length());
    EXPECT_TRUE(slice.hasKey("fields"));
    EXPECT_TRUE(slice.hasKey("includeAllFields"));
    EXPECT_TRUE(slice.hasKey("trackListPositions"));
    EXPECT_TRUE(slice.hasKey("storeValues"));
    EXPECT_TRUE(slice.hasKey("analyzers"));
  }

  // with fullAnalyzerDefinition
  {
    arangodb::iresearch::IResearchLinkMeta meta;
    arangodb::iresearch::IResearchLinkMeta::Mask mask(true);
    arangodb::velocypack::Builder builder;

    builder.openObject();
    EXPECT_TRUE(meta.json(server.server(), builder, true, nullptr, nullptr, &mask));
    builder.close();

    auto slice = builder.slice();

    EXPECT_EQ(9, slice.length());
    EXPECT_TRUE(slice.hasKey("fields"));
    EXPECT_TRUE(slice.hasKey("includeAllFields"));
    EXPECT_TRUE(slice.hasKey("trackListPositions"));
    EXPECT_TRUE(slice.hasKey("storeValues"));
    EXPECT_TRUE(slice.hasKey("analyzers"));
    EXPECT_TRUE(slice.hasKey("analyzerDefinitions"));
    EXPECT_TRUE(slice.hasKey("primarySort"));
    EXPECT_TRUE(slice.hasKey("storedValues"));
    EXPECT_TRUE(slice.hasKey("primarySortCompression"));
    EXPECT_FALSE(slice.hasKey("collectionName"));
  }
}

TEST_F(IResearchLinkMetaTest, test_writeMaskAllCluster) {
  auto oldRole = arangodb::ServerState::instance()->getRole();
  auto restoreRole  = irs::make_finally(
    [oldRole](){
      arangodb::ServerState::instance()->setRole(oldRole);});
  arangodb::ServerState::instance()->setRole(arangodb::ServerState::RoleEnum::ROLE_DBSERVER);
  // not fullAnalyzerDefinition
  {
    arangodb::iresearch::IResearchLinkMeta meta;
    arangodb::iresearch::IResearchLinkMeta::Mask mask(true);
    arangodb::velocypack::Builder builder;

    builder.openObject();
    EXPECT_TRUE(meta.json(server.server(), builder, false, nullptr, nullptr, &mask));
    builder.close();

    auto slice = builder.slice();

    EXPECT_EQ(5U, slice.length());
    EXPECT_TRUE(slice.hasKey("fields"));
    EXPECT_TRUE(slice.hasKey("includeAllFields"));
    EXPECT_TRUE(slice.hasKey("trackListPositions"));
    EXPECT_TRUE(slice.hasKey("storeValues"));
    EXPECT_TRUE(slice.hasKey("analyzers"));
  }

  // with fullAnalyzerDefinition
  {
    arangodb::iresearch::IResearchLinkMeta meta;
    arangodb::iresearch::IResearchLinkMeta::Mask mask(true);
    meta._collectionName = "Test"; // init to make it show in json
    arangodb::velocypack::Builder builder;

    builder.openObject();
    EXPECT_TRUE(meta.json(server.server(), builder, true, nullptr, nullptr, &mask));
    builder.close();

    auto slice = builder.slice();

    EXPECT_EQ(10, slice.length());
    EXPECT_TRUE(slice.hasKey("fields"));
    EXPECT_TRUE(slice.hasKey("includeAllFields"));
    EXPECT_TRUE(slice.hasKey("trackListPositions"));
    EXPECT_TRUE(slice.hasKey("storeValues"));
    EXPECT_TRUE(slice.hasKey("analyzers"));
    EXPECT_TRUE(slice.hasKey("analyzerDefinitions"));
    EXPECT_TRUE(slice.hasKey("primarySort"));
    EXPECT_TRUE(slice.hasKey("storedValues"));
    EXPECT_TRUE(slice.hasKey("primarySortCompression"));
    EXPECT_TRUE(slice.hasKey("collectionName"));
  }

  // with fullAnalyzerDefinition without collection name
  {
    arangodb::iresearch::IResearchLinkMeta meta;
    arangodb::iresearch::IResearchLinkMeta::Mask mask(true);
    arangodb::velocypack::Builder builder;

    builder.openObject();
    EXPECT_TRUE(meta.json(server.server(), builder, true, nullptr, nullptr, &mask));
    builder.close();

    auto slice = builder.slice();

    EXPECT_EQ(9, slice.length());
    EXPECT_TRUE(slice.hasKey("fields"));
    EXPECT_TRUE(slice.hasKey("includeAllFields"));
    EXPECT_TRUE(slice.hasKey("trackListPositions"));
    EXPECT_TRUE(slice.hasKey("storeValues"));
    EXPECT_TRUE(slice.hasKey("analyzers"));
    EXPECT_TRUE(slice.hasKey("analyzerDefinitions"));
    EXPECT_TRUE(slice.hasKey("primarySort"));
    EXPECT_TRUE(slice.hasKey("storedValues"));
    EXPECT_TRUE(slice.hasKey("primarySortCompression"));
    EXPECT_FALSE(slice.hasKey("collectionName"));
  }
}

TEST_F(IResearchLinkMetaTest, test_writeMaskNone) {
  // not fullAnalyzerDefinition
  {
    arangodb::iresearch::IResearchLinkMeta meta;
    arangodb::iresearch::IResearchLinkMeta::Mask mask(false);
    arangodb::velocypack::Builder builder;

    builder.openObject();
    EXPECT_TRUE(meta.json(server.server(), builder, false, nullptr, nullptr, &mask));
    builder.close();

    auto slice = builder.slice();

    EXPECT_EQ(0U, slice.length());
  }

  // with fullAnalyzerDefinition
  {
    arangodb::iresearch::IResearchLinkMeta meta;
    arangodb::iresearch::IResearchLinkMeta::Mask mask(false);
    arangodb::velocypack::Builder builder;

    builder.openObject();
    EXPECT_TRUE(meta.json(server.server(), builder, true, nullptr, nullptr, &mask));
    builder.close();

    auto slice = builder.slice();

    EXPECT_EQ(0U, slice.length());
  }
}

TEST_F(IResearchLinkMetaTest, test_readAnalyzerDefinitions) {
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));

  // missing analyzer (name only)
  {
    auto json = VPackParser::fromJson(
        "{ \
      \"analyzers\": [ \"empty1\" ] \
    }");
    arangodb::iresearch::IResearchLinkMeta meta;
    std::string errorField;
    EXPECT_FALSE(meta.init(server.server(), json->slice(), true, errorField, vocbase.name()));
    EXPECT_EQ(std::string("analyzers.empty1"), errorField);
  }

  // missing analyzer (name only) inRecovery
  {
    auto json = VPackParser::fromJson(
        "{ \
      \"analyzers\": [ \"empty1\" ] \
    }");
    auto before = StorageEngineMock::recoveryStateResult;
    StorageEngineMock::recoveryStateResult = arangodb::RecoveryState::IN_PROGRESS;
    auto restore = irs::make_finally(
        [&before]() -> void { StorageEngineMock::recoveryStateResult = before; });
    arangodb::iresearch::IResearchLinkMeta meta;
    std::string errorField;
    EXPECT_FALSE(meta.init(server.server(), json->slice(), true, errorField, vocbase.name()));
    EXPECT_EQ(std::string("analyzers.empty1"), errorField);
  }

  // missing analyzer (full) no name (fail) required
  {
    auto json = VPackParser::fromJson(
        "{ \
      \"analyzerDefinitions\": [ { \"type\": \"empty\", \"properties\": \"ru\", \"features\": [ \"frequency\" ] } ], \
      \"analyzers\": [ \"empty\" ] \
    }");
    arangodb::iresearch::IResearchLinkMeta meta;
    std::string errorField;
    EXPECT_FALSE(meta.init(server.server(), json->slice(), true, errorField, vocbase.name()));
    EXPECT_EQ(std::string("analyzerDefinitions[0].name"), errorField);
  }

  // missing analyzer (full) no type (fail) required
  {
    auto json = VPackParser::fromJson(
        "{ \
      \"analyzerDefinitions\": [ { \"name\": \"missing0\", \"properties\": \"ru\", \"features\": [ \"frequency\" ] } ], \
      \"analyzers\": [ \"missing0\" ] \
    }");
    arangodb::iresearch::IResearchLinkMeta meta;
    std::string errorField;
    EXPECT_FALSE(meta.init(server.server(), json->slice(), true, errorField, vocbase.name()));
    EXPECT_EQ(std::string("analyzerDefinitions[0].type"), errorField);
  }

  // missing analyzer definition single-server
  {
    auto json = VPackParser::fromJson(
        "{ \
      \"analyzers\": [ \"missing0\" ] \
    }");
    arangodb::iresearch::IResearchLinkMeta meta;
    std::string errorField;
    EXPECT_FALSE(meta.init(server.server(), json->slice(), true, errorField, vocbase.name()));
    EXPECT_EQ("analyzers.missing0", errorField);
  }

  // missing analyzer (full) single-server (ignore analyzer definition)
  {
    auto json = VPackParser::fromJson(
        "{ \
      \"analyzerDefinitions\": [ { \"name\": \"missing0\", \"type\": \"empty\", \"properties\": {\"args\":\"ru\"}, \"features\": [ \"frequency\" ] } ], \
      \"analyzers\": [ \"missing0\" ] \
    }");
    arangodb::iresearch::IResearchLinkMeta meta;
    std::string errorField;
    EXPECT_FALSE(meta.init(server.server(), json->slice(), false, errorField, vocbase.name()));
    EXPECT_EQ("analyzers.missing0", errorField);
  }

  // missing analyzer (full) single-server
  {
    auto json = VPackParser::fromJson(
        "{ \
      \"analyzerDefinitions\": [ { \"name\": \"missing0\", \"type\": \"empty\", \"properties\": {\"args\":\"ru\"}, \"features\": [ \"frequency\" ] } ], \
      \"analyzers\": [ \"missing0\" ] \
    }");
    arangodb::iresearch::IResearchLinkMeta meta;
    std::string errorField;
    EXPECT_TRUE(meta.init(server.server(), json->slice(), true, errorField, vocbase.name()));
    EXPECT_EQ(1, meta._analyzers.size());
    EXPECT_EQ(std::string("testVocbase::missing0"), meta._analyzers[0]._pool->name());
    EXPECT_EQ(std::string("empty"), meta._analyzers[0]._pool->type());
    EXPECT_EQUAL_SLICES(VPackParser::fromJson("{\"args\" : \"ru\"}")->slice(),
                                      meta._analyzers[0]._pool->properties());
    EXPECT_EQ(1, meta._analyzers[0]._pool->features().size());
    EXPECT_TRUE(meta._analyzers[0]._pool->features().check(irs::type<irs::frequency>::id()));
    EXPECT_EQ("missing0", meta._analyzers[0]._shortName);
  }

  // complex definition on single-server
  {
    auto json = VPackParser::fromJson(
    "{ \
      \"analyzerDefinitions\": [ \
         { \"name\": \"empty\", \"type\": \"empty\", \"properties\": {\"args\":\"ru\"}, \"features\": [ \"frequency\" ] }, \
         { \"name\": \"::empty\", \"type\": \"empty\", \"properties\": {\"args\":\"ru\"}, \"features\": [ \"frequency\" ] } \
      ], \
      \"fields\" : { \"field\": { \"analyzers\": [ \"testVocbase::empty\", \"empty\", \"_system::empty\", \"::empty\" ] } } \
    }");
    arangodb::iresearch::IResearchLinkMeta meta;
    std::string errorField;
    EXPECT_TRUE(meta.init(server.server(), json->slice(), true, errorField, vocbase.name()));
    EXPECT_EQ(2, meta._analyzerDefinitions.size());
    EXPECT_EQ(1, meta._analyzers.size());
    {
      auto& analyzer = meta._analyzers[0];
      EXPECT_EQ(arangodb::iresearch::IResearchAnalyzerFeature::identity(), analyzer._pool);
    }
    ASSERT_EQ(1, meta._fields.size());
    ASSERT_EQ(2, meta._fields.begin().value()->_analyzers.size());
    {
      auto& analyzer = meta._fields.begin().value()->_analyzers[0];
      EXPECT_EQ("testVocbase::empty", analyzer._pool->name());
      EXPECT_EQ("empty", analyzer._pool->type());
      EXPECT_EQUAL_SLICES(VPackParser::fromJson("{\"args\" : \"ru\"}")->slice(),
                          analyzer._pool->properties());
      EXPECT_EQ(1, analyzer._pool->features().size());
      EXPECT_TRUE(analyzer._pool->features().check(irs::type<irs::frequency>::id()));
      EXPECT_EQ("empty", analyzer._shortName);
      EXPECT_EQ((*meta._analyzerDefinitions.find(analyzer._pool->name())), analyzer._pool);
    }
    {
      auto& analyzer = meta._fields.begin().value()->_analyzers[1];
      EXPECT_EQ("_system::empty", analyzer._pool->name());
      EXPECT_EQ("empty", analyzer._pool->type());
      EXPECT_EQUAL_SLICES(VPackParser::fromJson("{\"args\" : \"ru\"}")->slice(),
                          analyzer._pool->properties());
      EXPECT_EQ(1, analyzer._pool->features().size());
      EXPECT_TRUE(analyzer._pool->features().check(irs::type<irs::frequency>::id()));
      EXPECT_EQ("::empty", analyzer._shortName);
      EXPECT_EQ((*meta._analyzerDefinitions.find(analyzer._pool->name())), analyzer._pool);
    }
  }

  // complex definition on single-server
  {
    auto json = VPackParser::fromJson(
    "{ \
      \"analyzerDefinitions\": [ \
         { \"name\": \"::empty\", \"type\": \"empty\", \"properties\": {\"args\":\"ru\"}, \"features\": [ \"frequency\" ] } \
      ], \
      \"fields\" : { \"field\": { \"analyzers\": [ \"testVocbase::empty\", \"empty\", \"_system::empty\", \"::empty\" ] } } \
    }");
    arangodb::iresearch::IResearchLinkMeta meta;
    std::string errorField;
    ASSERT_TRUE(meta.init(server.server(), json->slice(), true, errorField, vocbase.name()));
    EXPECT_EQ(2, meta._analyzerDefinitions.size());
    EXPECT_EQ(1, meta._analyzers.size());
    {
      auto& analyzer = meta._analyzers[0];
      EXPECT_EQ(arangodb::iresearch::IResearchAnalyzerFeature::identity(), analyzer._pool);
    }
    ASSERT_EQ(1, meta._fields.size());
    ASSERT_EQ(2, meta._fields.begin().value()->_analyzers.size());
    {
      auto& analyzer = meta._fields.begin().value()->_analyzers[0];
      EXPECT_EQ("testVocbase::empty", analyzer._pool->name());
      EXPECT_EQ("empty", analyzer._pool->type());
      // definition from cache since it's not presented "analyzerDefinitions"
      EXPECT_EQUAL_SLICES(VPackParser::fromJson("{\"args\" : \"de\"}")->slice(),
                          analyzer._pool->properties());
      EXPECT_EQ(1, analyzer._pool->features().size());
      EXPECT_TRUE(analyzer._pool->features().check(irs::type<irs::frequency>::id()));
      EXPECT_EQ("empty", analyzer._shortName);
      EXPECT_EQ((*meta._analyzerDefinitions.find(analyzer._pool->name())), analyzer._pool);
    }
    {
      auto& analyzer = meta._fields.begin().value()->_analyzers[1];
      EXPECT_EQ("_system::empty", analyzer._pool->name());
      EXPECT_EQ("empty", analyzer._pool->type());
      EXPECT_EQUAL_SLICES(VPackParser::fromJson("{\"args\" : \"ru\"}")->slice(),
                          analyzer._pool->properties());
      EXPECT_EQ(1, analyzer._pool->features().size());
      EXPECT_TRUE(analyzer._pool->features().check(irs::type<irs::frequency>::id()));
      EXPECT_EQ("::empty", analyzer._shortName);
      EXPECT_EQ((*meta._analyzerDefinitions.find(analyzer._pool->name())), analyzer._pool);
    }
  }

  // missing analyzer definition coordinator
  {
    auto before = arangodb::ServerState::instance()->getRole();
    arangodb::ServerState::instance()->setRole(arangodb::ServerState::ROLE_COORDINATOR);
    auto restore = irs::make_finally([&before]() -> void {
      arangodb::ServerState::instance()->setRole(before);
    });

    auto json = VPackParser::fromJson(
    "{ \
      \"analyzers\": [ \"missing1\" ] \
    }");
    arangodb::iresearch::IResearchLinkMeta meta;
    std::string errorField;
    EXPECT_FALSE(meta.init(server.server(), json->slice(), true, errorField, vocbase.name()));
    EXPECT_EQ("analyzers.missing1", errorField);
  }

  // missing analyzer definition coordinator
  {
    auto before = arangodb::ServerState::instance()->getRole();
    arangodb::ServerState::instance()->setRole(arangodb::ServerState::ROLE_COORDINATOR);
    auto restore = irs::make_finally([&before]() -> void {
      arangodb::ServerState::instance()->setRole(before);
    });

    auto json = VPackParser::fromJson(
    "{ \
      \"fields\" : { \"field\": { \"analyzers\": [ \"missing1\" ] } } \
    }");
    arangodb::iresearch::IResearchLinkMeta meta;
    std::string errorField;
    EXPECT_FALSE(meta.init(server.server(), json->slice(), true, errorField, vocbase.name()));
    EXPECT_EQ("fields.field.analyzers.missing1", errorField);
  }

  // missing analyzer (full) coordinator
  {
    auto before = arangodb::ServerState::instance()->getRole();
    arangodb::ServerState::instance()->setRole(arangodb::ServerState::ROLE_COORDINATOR);
    auto restore = irs::make_finally([&before]() -> void {
      arangodb::ServerState::instance()->setRole(before);
    });

    auto json = VPackParser::fromJson(
        "{ \
      \"analyzerDefinitions\": [ { \"name\": \"missing1\", \"type\": \"empty\", \"properties\": {\"args\":\"ru\"}, \"features\": [ \"frequency\" ] } ], \
      \"analyzers\": [ \"missing1\" ] \
    }");
    arangodb::iresearch::IResearchLinkMeta meta;
    std::string errorField;
    EXPECT_TRUE(meta.init(server.server(), json->slice(), true, errorField, vocbase.name()));
    EXPECT_EQ(1, meta._analyzers.size());
    EXPECT_EQ("testVocbase::missing1", meta._analyzers[0]._pool->name());
    EXPECT_EQ("empty", meta._analyzers[0]._pool->type());
    EXPECT_EQUAL_SLICES(VPackParser::fromJson("{\"args\" : \"ru\"}")->slice(),
                                      meta._analyzers[0]._pool->properties());
    EXPECT_EQ(1, meta._analyzers[0]._pool->features().size());
    EXPECT_TRUE(meta._analyzers[0]._pool->features().check(irs::type<irs::frequency>::id()));
    EXPECT_EQ("missing1", meta._analyzers[0]._shortName);
  }

  // missing analyzer (full) coordinator (ignore analyzer definition)
  {
    auto before = arangodb::ServerState::instance()->getRole();
    arangodb::ServerState::instance()->setRole(arangodb::ServerState::ROLE_COORDINATOR);
    auto restore = irs::make_finally([&before]() -> void {
      arangodb::ServerState::instance()->setRole(before);
    });

    auto json = VPackParser::fromJson(
        "{ \
      \"analyzerDefinitions\": [ { \"name\": \"missing1\", \"type\": \"empty\", \"properties\": {\"args\":\"ru\"}, \"features\": [ \"frequency\" ] } ], \
      \"analyzers\": [ \"missing1\" ] \
    }");
    arangodb::iresearch::IResearchLinkMeta meta;
    std::string errorField;
    EXPECT_FALSE(meta.init(server.server(), json->slice(), false, errorField, vocbase.name()));
    EXPECT_EQ("analyzers.missing1", errorField);
  }

  // complex definition on coordinator
  {
    auto before = arangodb::ServerState::instance()->getRole();
    arangodb::ServerState::instance()->setRole(arangodb::ServerState::ROLE_COORDINATOR);
    auto restore = irs::make_finally([&before]() -> void {
      arangodb::ServerState::instance()->setRole(before);
    });

    auto json = VPackParser::fromJson(
    "{ \
      \"analyzerDefinitions\": [ \
         { \"name\": \"empty\", \"type\": \"empty\", \"properties\": {\"args\":\"ru\"}, \"features\": [ \"frequency\" ] }, \
         { \"name\": \"::empty\", \"type\": \"empty\", \"properties\": {\"args\":\"ru\"}, \"features\": [ \"frequency\" ] } \
      ], \
      \"fields\" : { \"field\": { \"analyzers\": [ \"testVocbase::empty\", \"empty\", \"_system::empty\", \"::empty\" ] } } \
    }");
    arangodb::iresearch::IResearchLinkMeta meta;
    std::string errorField;
    EXPECT_TRUE(meta.init(server.server(), json->slice(), true, errorField, vocbase.name()));
    EXPECT_EQ(2, meta._analyzerDefinitions.size());
    EXPECT_EQ(1, meta._analyzers.size());
    {
      auto& analyzer = meta._analyzers[0];
      EXPECT_EQ(arangodb::iresearch::IResearchAnalyzerFeature::identity(), analyzer._pool);
    }

    ASSERT_EQ(1, meta._fields.size());
    ASSERT_EQ(2, meta._fields.begin().value()->_analyzers.size());
    {
      auto& analyzer = meta._fields.begin().value()->_analyzers[0];
      EXPECT_EQ(std::string("testVocbase::empty"), analyzer._pool->name());
      EXPECT_EQ(std::string("empty"), analyzer._pool->type());
      EXPECT_EQUAL_SLICES(VPackParser::fromJson("{\"args\" : \"ru\"}")->slice(),
                          analyzer._pool->properties());
      EXPECT_EQ(1, analyzer._pool->features().size());
      EXPECT_TRUE(analyzer._pool->features().check(irs::type<irs::frequency>::id()));
      EXPECT_EQ("empty", analyzer._shortName);
      EXPECT_EQ((*meta._analyzerDefinitions.find(analyzer._pool->name())), analyzer._pool);
    }
    {
      auto& analyzer = meta._fields.begin().value()->_analyzers[1];
      EXPECT_EQ(std::string("_system::empty"), analyzer._pool->name());
      EXPECT_EQ(std::string("empty"), analyzer._pool->type());
      EXPECT_EQUAL_SLICES(VPackParser::fromJson("{\"args\" : \"ru\"}")->slice(),
                          analyzer._pool->properties());
      EXPECT_EQ(1, analyzer._pool->features().size());
      EXPECT_TRUE(analyzer._pool->features().check(irs::type<irs::frequency>::id()));
      EXPECT_EQ("::empty", analyzer._shortName);
      EXPECT_EQ((*meta._analyzerDefinitions.find(analyzer._pool->name())), analyzer._pool);
    }
  }

  // complex definition on coordinator
  {
    auto before = arangodb::ServerState::instance()->getRole();
    arangodb::ServerState::instance()->setRole(arangodb::ServerState::ROLE_COORDINATOR);
    auto restore = irs::make_finally([&before]() -> void {
      arangodb::ServerState::instance()->setRole(before);
    });

    auto json = VPackParser::fromJson(
    "{ \
      \"analyzerDefinitions\": [ \
         { \"name\": \"::empty\", \"type\": \"empty\", \"properties\": {\"args\":\"ru\"}, \"features\": [ \"frequency\" ] } \
      ], \
      \"fields\" : { \"field\": { \"analyzers\": [ \"testVocbase::empty\", \"empty\", \"_system::empty\", \"::empty\" ] } } \
    }");
    arangodb::iresearch::IResearchLinkMeta meta;
    std::string errorField;
    EXPECT_TRUE(meta.init(server.server(), json->slice(), true, errorField, vocbase.name()));
    EXPECT_EQ(2, meta._analyzerDefinitions.size());
    EXPECT_EQ(1, meta._analyzers.size());
    {
      auto& analyzer = meta._analyzers[0];
      EXPECT_EQ(arangodb::iresearch::IResearchAnalyzerFeature::identity(), analyzer._pool);
    }

    ASSERT_EQ(1, meta._fields.size());
    ASSERT_EQ(2, meta._fields.begin().value()->_analyzers.size());
    {
      auto& analyzer = meta._fields.begin().value()->_analyzers[0];
      EXPECT_EQ("testVocbase::empty", analyzer._pool->name());
      EXPECT_EQ("empty", analyzer._pool->type());
      // definition from cache since it's not presented "analyzerDefinitions"
      EXPECT_EQUAL_SLICES(VPackParser::fromJson("{\"args\" : \"de\"}")->slice(),
                          analyzer._pool->properties());
      EXPECT_EQ(1, analyzer._pool->features().size());
      EXPECT_TRUE(analyzer._pool->features().check(irs::type<irs::frequency>::id()));
      EXPECT_EQ("empty", analyzer._shortName);
      EXPECT_EQ((*meta._analyzerDefinitions.find(analyzer._pool->name())), analyzer._pool);
    }
    {
      auto& analyzer = meta._fields.begin().value()->_analyzers[1];
      EXPECT_EQ("_system::empty", analyzer._pool->name());
      EXPECT_EQ("empty", analyzer._pool->type());
      EXPECT_EQUAL_SLICES(VPackParser::fromJson("{\"args\" : \"ru\"}")->slice(),
                          analyzer._pool->properties());
      EXPECT_EQ(1, analyzer._pool->features().size());
      EXPECT_TRUE(analyzer._pool->features().check(irs::type<irs::frequency>::id()));
      EXPECT_EQ("::empty", analyzer._shortName);
      EXPECT_EQ((*meta._analyzerDefinitions.find(analyzer._pool->name())), analyzer._pool);
    }
  }

  // missing analyzer (full) db-server
  {
    auto before = arangodb::ServerState::instance()->getRole();
    arangodb::ServerState::instance()->setRole(arangodb::ServerState::ROLE_DBSERVER);
    auto restore = irs::make_finally([&before]() -> void {
      arangodb::ServerState::instance()->setRole(before);
    });

    auto json = VPackParser::fromJson(
        "{ \
      \"analyzerDefinitions\": [ { \"name\": \"missing2\", \"type\": \"empty\", \"properties\": { \"args\": \"ru\" }, \"features\": [ \"frequency\" ] } ], \
      \"analyzers\": [ \"missing2\" ] \
    }");
    arangodb::iresearch::IResearchLinkMeta meta;
    std::string errorField;
    EXPECT_TRUE(meta.init(server.server(), json->slice(), true, errorField, vocbase.name()));
    EXPECT_EQ(1, meta._analyzers.size());
    EXPECT_EQ("testVocbase::missing2", meta._analyzers[0]._pool->name());
    EXPECT_EQ("empty", meta._analyzers[0]._pool->type());
    EXPECT_EQ(std::string("empty"), meta._analyzers[0]._pool->type());
    EXPECT_EQUAL_SLICES(VPackParser::fromJson("{\"args\" : \"ru\"}")->slice(),
                                      meta._analyzers[0]._pool->properties());
    EXPECT_EQ(1, meta._analyzers[0]._pool->features().size());
    EXPECT_TRUE(meta._analyzers[0]._pool->features().check(irs::type<irs::frequency>::id()));
    EXPECT_EQ("missing2", meta._analyzers[0]._shortName);
  }

  // missing analyzer (full) db-server (ignore analyzer definition)
  {
    auto before = arangodb::ServerState::instance()->getRole();
    arangodb::ServerState::instance()->setRole(arangodb::ServerState::ROLE_DBSERVER);
    auto restore = irs::make_finally([&before]() -> void {
      arangodb::ServerState::instance()->setRole(before);
    });

    auto json = VPackParser::fromJson(
        "{ \
      \"analyzerDefinitions\": [ { \"name\": \"missing2\", \"type\": \"empty\", \"properties\": { \"args\": \"ru\" }, \"features\": [ \"frequency\" ] } ], \
      \"analyzers\": [ \"missing2\" ] \
    }");
    arangodb::iresearch::IResearchLinkMeta meta;
    std::string errorField;
    EXPECT_FALSE(meta.init(server.server(), json->slice(), false, errorField, vocbase.name()));
    EXPECT_EQ("analyzers.missing2", errorField);
  }

  // missing analyzer definition db-server
  {
    auto before = arangodb::ServerState::instance()->getRole();
    arangodb::ServerState::instance()->setRole(arangodb::ServerState::ROLE_DBSERVER);
    auto restore = irs::make_finally([&before]() -> void {
      arangodb::ServerState::instance()->setRole(before);
    });

    auto json = VPackParser::fromJson(
        "{ \
      \"analyzers\": [ \"missing2\" ] \
    }");
    arangodb::iresearch::IResearchLinkMeta meta;
    std::string errorField;
    EXPECT_FALSE(meta.init(server.server(), json->slice(), true, errorField, vocbase.name()));
    EXPECT_EQ("analyzers.missing2", errorField);
  }

  // complex definition on db-server
  {
    auto before = arangodb::ServerState::instance()->getRole();
    arangodb::ServerState::instance()->setRole(arangodb::ServerState::ROLE_DBSERVER);
    auto restore = irs::make_finally([&before]() -> void {
      arangodb::ServerState::instance()->setRole(before);
    });

    auto json = VPackParser::fromJson(
    "{ \
      \"analyzerDefinitions\": [ \
         { \"name\": \"empty\", \"type\": \"empty\", \"properties\": {\"args\":\"ru\"}, \"features\": [ \"frequency\" ] }, \
         { \"name\": \"::empty\", \"type\": \"empty\", \"properties\": {\"args\":\"ru\"}, \"features\": [ \"frequency\" ] } \
      ], \
      \"fields\" : { \"field\": { \"analyzers\": [ \"testVocbase::empty\", \"empty\", \"_system::empty\", \"::empty\" ] } } \
    }");
    arangodb::iresearch::IResearchLinkMeta meta;
    std::string errorField;
    EXPECT_TRUE(meta.init(server.server(), json->slice(), true, errorField, vocbase.name()));
    EXPECT_EQ(2, meta._analyzerDefinitions.size());
    EXPECT_EQ(1, meta._analyzers.size());
    {
      auto& analyzer = meta._analyzers[0];
      EXPECT_EQ(arangodb::iresearch::IResearchAnalyzerFeature::identity(), analyzer._pool);
    }

    ASSERT_EQ(1, meta._fields.size());
    ASSERT_EQ(2, meta._fields.begin().value()->_analyzers.size());
    {
      auto& analyzer = meta._fields.begin().value()->_analyzers[0];
      EXPECT_EQ(std::string("testVocbase::empty"), analyzer._pool->name());
      EXPECT_EQ(std::string("empty"), analyzer._pool->type());
      EXPECT_EQUAL_SLICES(VPackParser::fromJson("{\"args\" : \"ru\"}")->slice(),
                          analyzer._pool->properties());
      EXPECT_EQ(1, analyzer._pool->features().size());
      EXPECT_TRUE(analyzer._pool->features().check(irs::type<irs::frequency>::id()));
      EXPECT_EQ("empty", analyzer._shortName);
      EXPECT_EQ((*meta._analyzerDefinitions.find(analyzer._pool->name())), analyzer._pool);
    }
    {
      auto& analyzer = meta._fields.begin().value()->_analyzers[1];
      EXPECT_EQ(std::string("_system::empty"), analyzer._pool->name());
      EXPECT_EQ(std::string("empty"), analyzer._pool->type());
      EXPECT_EQUAL_SLICES(VPackParser::fromJson("{\"args\" : \"ru\"}")->slice(),
                          analyzer._pool->properties());
      EXPECT_EQ(1, analyzer._pool->features().size());
      EXPECT_TRUE(analyzer._pool->features().check(irs::type<irs::frequency>::id()));
      EXPECT_EQ("::empty", analyzer._shortName);
      EXPECT_EQ((*meta._analyzerDefinitions.find(analyzer._pool->name())), analyzer._pool);
    }
  }

  // complex definition on db-server
  {
    auto before = arangodb::ServerState::instance()->getRole();
    arangodb::ServerState::instance()->setRole(arangodb::ServerState::ROLE_DBSERVER);
    auto restore = irs::make_finally([&before]() -> void {
      arangodb::ServerState::instance()->setRole(before);
    });

    auto json = VPackParser::fromJson(
    "{ \
      \"analyzerDefinitions\": [ \
         { \"name\": \"::empty\", \"type\": \"empty\", \"properties\": {\"args\":\"ru\"}, \"features\": [ \"frequency\" ] } \
      ], \
      \"fields\" : { \"field\": { \"analyzers\": [ \"testVocbase::empty\", \"empty\", \"_system::empty\", \"::empty\" ] } } \
    }");
    arangodb::iresearch::IResearchLinkMeta meta;
    std::string errorField;
    EXPECT_TRUE(meta.init(server.server(), json->slice(), true, errorField, vocbase.name()));
    EXPECT_EQ(2, meta._analyzerDefinitions.size());
    EXPECT_EQ(1, meta._analyzers.size());
    {
      auto& analyzer = meta._analyzers[0];
      EXPECT_EQ(arangodb::iresearch::IResearchAnalyzerFeature::identity(), analyzer._pool);
    }

    ASSERT_EQ(1, meta._fields.size());
    ASSERT_EQ(2, meta._fields.begin().value()->_analyzers.size());
    {
      auto& analyzer = meta._fields.begin().value()->_analyzers[0];
      EXPECT_EQ("testVocbase::empty", analyzer._pool->name());
      EXPECT_EQ("empty", analyzer._pool->type());
      // definition from cache since it's not presented "analyzerDefinitions"
      EXPECT_EQUAL_SLICES(VPackParser::fromJson("{\"args\" : \"de\"}")->slice(),
                          analyzer._pool->properties());
      EXPECT_EQ(1, analyzer._pool->features().size());
      EXPECT_TRUE(analyzer._pool->features().check(irs::type<irs::frequency>::id()));
      EXPECT_EQ("empty", analyzer._shortName);
      EXPECT_EQ((*meta._analyzerDefinitions.find(analyzer._pool->name())), analyzer._pool);
    }
    {
      auto& analyzer = meta._fields.begin().value()->_analyzers[1];
      EXPECT_EQ("_system::empty", analyzer._pool->name());
      EXPECT_EQ("empty", analyzer._pool->type());
      EXPECT_EQUAL_SLICES(VPackParser::fromJson("{\"args\" : \"ru\"}")->slice(),
                          analyzer._pool->properties());
      EXPECT_EQ(1, analyzer._pool->features().size());
      EXPECT_TRUE(analyzer._pool->features().check(irs::type<irs::frequency>::id()));
      EXPECT_EQ("::empty", analyzer._shortName);
      EXPECT_EQ((*meta._analyzerDefinitions.find(analyzer._pool->name())), analyzer._pool);
    }
  }

  // missing analyzer (full) inRecovery
  {
    auto json = VPackParser::fromJson(
        "{ \
      \"analyzerDefinitions\": [ { \"name\": \"missing3\", \"type\": \"empty\", \"properties\": {\"args\":\"ru\"}, \"features\": [ \"frequency\" ] } ], \
      \"analyzers\": [ \"missing3\" ] \
    }");
    auto before = StorageEngineMock::recoveryStateResult;
    StorageEngineMock::recoveryStateResult = arangodb::RecoveryState::IN_PROGRESS;
    auto restore = irs::make_finally(
        [&before]() -> void { StorageEngineMock::recoveryStateResult = before; });
    arangodb::iresearch::IResearchLinkMeta meta;
    std::string errorField;
    EXPECT_TRUE(meta.init(server.server(), json->slice(), true, errorField, vocbase.name()));
    EXPECT_EQ(1, meta._analyzers.size());
    EXPECT_EQ("testVocbase::missing3", meta._analyzers[0]._pool->name());
    EXPECT_EQ("empty", meta._analyzers[0]._pool->type());
    EXPECT_EQUAL_SLICES(VPackParser::fromJson("{\"args\" : \"ru\"}")->slice(),
                                      meta._analyzers[0]._pool->properties());
    EXPECT_EQ(1, meta._analyzers[0]._pool->features().size());
    EXPECT_TRUE(meta._analyzers[0]._pool->features().check(irs::type<irs::frequency>::id()));
    EXPECT_EQ("missing3", meta._analyzers[0]._shortName);
  }

  // missing analyzer (full) inRecovery (ignore analyzer definition)
  {
    auto json = VPackParser::fromJson(
        "{ \
      \"analyzerDefinitions\": [ { \"name\": \"missing3\", \"type\": \"empty\", \"properties\": {\"args\":\"ru\"}, \"features\": [ \"frequency\" ] } ], \
      \"analyzers\": [ \"missing3\" ] \
    }");
    auto before = StorageEngineMock::recoveryStateResult;
    StorageEngineMock::recoveryStateResult = arangodb::RecoveryState::IN_PROGRESS;
    auto restore = irs::make_finally(
        [&before]() -> void { StorageEngineMock::recoveryStateResult = before; });
    arangodb::iresearch::IResearchLinkMeta meta;
    std::string errorField;
    EXPECT_FALSE(meta.init(server.server(), json->slice(), false, errorField, vocbase.name()));
    EXPECT_EQ("analyzers.missing3", errorField);
  }

  // missing analyzer definition inRecovery
  {
    auto json = VPackParser::fromJson(
    "{ \
      \"analyzers\": [ \"missing3\" ] \
    }");
    auto before = StorageEngineMock::recoveryStateResult;
    StorageEngineMock::recoveryStateResult = arangodb::RecoveryState::IN_PROGRESS;
    auto restore = irs::make_finally(
        [&before]() -> void { StorageEngineMock::recoveryStateResult = before; });
    arangodb::iresearch::IResearchLinkMeta meta;
    std::string errorField;
    EXPECT_FALSE(meta.init(server.server(), json->slice(), true, errorField, vocbase.name()));
    EXPECT_EQ("analyzers.missing3", errorField);
  }

  // existing analyzer (name only)
  {
    auto json = VPackParser::fromJson(
        "{ \
      \"analyzers\": [ \"empty\" ] \
    }");
    arangodb::iresearch::IResearchLinkMeta meta;
    std::string errorField;
    EXPECT_TRUE(meta.init(server.server(), json->slice(), true, errorField, vocbase.name()));
    EXPECT_EQ(1, meta._analyzers.size());
    EXPECT_EQ(std::string("testVocbase::empty"), meta._analyzers[0]._pool->name());
    EXPECT_EQ(std::string("empty"), meta._analyzers[0]._pool->type());
    EXPECT_EQUAL_SLICES(
        VPackParser::fromJson("{\"args\" : \"de\"}")->slice(),
        meta._analyzers[0]._pool->properties());
    EXPECT_EQ(1, meta._analyzers[0]._pool->features().size());
    EXPECT_TRUE(meta._analyzers[0]._pool->features().check(irs::type<irs::frequency>::id()));
    EXPECT_EQ(std::string("empty"), meta._analyzers[0]._shortName);
  }

  // existing analyzer (name only) inRecovery
  {
    auto json = VPackParser::fromJson(
        "{ \
      \"analyzers\": [ \"empty\" ] \
    }");
    auto before = StorageEngineMock::recoveryStateResult;
    StorageEngineMock::recoveryStateResult = arangodb::RecoveryState::IN_PROGRESS;
    auto restore = irs::make_finally(
        [&before]() -> void { StorageEngineMock::recoveryStateResult = before; });
    arangodb::iresearch::IResearchLinkMeta meta;
    std::string errorField;
    EXPECT_TRUE(meta.init(server.server(), json->slice(), true, errorField, vocbase.name()));
    EXPECT_EQ(1, meta._analyzers.size());
    EXPECT_EQ(std::string("testVocbase::empty"), meta._analyzers[0]._pool->name());
    EXPECT_EQ(std::string("empty"), meta._analyzers[0]._pool->type());
    EXPECT_EQUAL_SLICES(
        VPackParser::fromJson("{\"args\" : \"de\"}")->slice(),
        meta._analyzers[0]._pool->properties());
    EXPECT_EQ(1, meta._analyzers[0]._pool->features().size());
    EXPECT_TRUE(meta._analyzers[0]._pool->features().check(irs::type<irs::frequency>::id()));
    EXPECT_EQ(std::string("empty"), meta._analyzers[0]._shortName);
  }

  // complex definition inRecovery
  {
    auto before = StorageEngineMock::recoveryStateResult;
    StorageEngineMock::recoveryStateResult = arangodb::RecoveryState::IN_PROGRESS;
    auto restore = irs::make_finally(
        [&before]() -> void { StorageEngineMock::recoveryStateResult = before; });

    auto json = VPackParser::fromJson(
    "{ \
      \"analyzerDefinitions\": [ \
         { \"name\": \"empty\", \"type\": \"empty\", \"properties\": {\"args\":\"ru\"}, \"features\": [ \"frequency\" ] }, \
         { \"name\": \"::empty\", \"type\": \"empty\", \"properties\": {\"args\":\"ru\"}, \"features\": [ \"frequency\" ] } \
      ], \
      \"fields\" : { \"field\": { \"analyzers\": [ \"testVocbase::empty\", \"empty\", \"_system::empty\", \"::empty\" ] } } \
    }");
    arangodb::iresearch::IResearchLinkMeta meta;
    std::string errorField;
    EXPECT_TRUE(meta.init(server.server(), json->slice(), true, errorField, vocbase.name()));
    EXPECT_EQ(2, meta._analyzerDefinitions.size());
    EXPECT_EQ(1, meta._analyzers.size());
    {
      auto& analyzer = meta._analyzers[0];
      EXPECT_EQ(arangodb::iresearch::IResearchAnalyzerFeature::identity(), analyzer._pool);
    }

    ASSERT_EQ(1, meta._fields.size());
    ASSERT_EQ(2, meta._fields.begin().value()->_analyzers.size());
    {
      auto& analyzer = meta._fields.begin().value()->_analyzers[0];
      EXPECT_EQ(std::string("testVocbase::empty"), analyzer._pool->name());
      EXPECT_EQ(std::string("empty"), analyzer._pool->type());
      EXPECT_EQUAL_SLICES(VPackParser::fromJson("{\"args\" : \"ru\"}")->slice(),
                          analyzer._pool->properties());
      EXPECT_EQ(1, analyzer._pool->features().size());
      EXPECT_TRUE(analyzer._pool->features().check(irs::type<irs::frequency>::id()));
      EXPECT_EQ("empty", analyzer._shortName);
      EXPECT_EQ((*meta._analyzerDefinitions.find(analyzer._pool->name())), analyzer._pool);
    }
    {
      auto& analyzer = meta._fields.begin().value()->_analyzers[1];
      EXPECT_EQ(std::string("_system::empty"), analyzer._pool->name());
      EXPECT_EQ(std::string("empty"), analyzer._pool->type());
      EXPECT_EQUAL_SLICES(VPackParser::fromJson("{\"args\" : \"ru\"}")->slice(),
                          analyzer._pool->properties());
      EXPECT_EQ(1, analyzer._pool->features().size());
      EXPECT_TRUE(analyzer._pool->features().check(irs::type<irs::frequency>::id()));
      EXPECT_EQ("::empty", analyzer._shortName);
      EXPECT_EQ((*meta._analyzerDefinitions.find(analyzer._pool->name())), analyzer._pool);
    }
  }

  // complex definition inRecovery
  {
    auto before = StorageEngineMock::recoveryStateResult;
    StorageEngineMock::recoveryStateResult = arangodb::RecoveryState::IN_PROGRESS;
    auto restore = irs::make_finally(
        [&before]() -> void { StorageEngineMock::recoveryStateResult = before; });

    auto json = VPackParser::fromJson(
    "{ \
      \"analyzerDefinitions\": [ \
         { \"name\": \"::empty\", \"type\": \"empty\", \"properties\": {\"args\":\"ru\"}, \"features\": [ \"frequency\" ] } \
      ], \
      \"fields\" : { \"field\": { \"analyzers\": [ \"testVocbase::empty\", \"empty\", \"_system::empty\", \"::empty\" ] } } \
    }");
    arangodb::iresearch::IResearchLinkMeta meta;
    std::string errorField;
    EXPECT_TRUE(meta.init(server.server(), json->slice(), true, errorField, vocbase.name()));
    EXPECT_EQ(2, meta._analyzerDefinitions.size());
    EXPECT_EQ(1, meta._analyzers.size());
    {
      auto& analyzer = meta._analyzers[0];
      EXPECT_EQ(arangodb::iresearch::IResearchAnalyzerFeature::identity(), analyzer._pool);
    }

    ASSERT_EQ(1, meta._fields.size());
    ASSERT_EQ(2, meta._fields.begin().value()->_analyzers.size());
    {
      auto& analyzer = meta._fields.begin().value()->_analyzers[0];
      EXPECT_EQ("testVocbase::empty", analyzer._pool->name());
      EXPECT_EQ("empty", analyzer._pool->type());
      // definition from cache since it's not presented "analyzerDefinitions"
      EXPECT_EQUAL_SLICES(VPackParser::fromJson("{\"args\" : \"de\"}")->slice(),
                          analyzer._pool->properties());
      EXPECT_EQ(1, analyzer._pool->features().size());
      EXPECT_TRUE(analyzer._pool->features().check(irs::type<irs::frequency>::id()));
      EXPECT_EQ("empty", analyzer._shortName);
      EXPECT_EQ((*meta._analyzerDefinitions.find(analyzer._pool->name())), analyzer._pool);
    }
    {
      auto& analyzer = meta._fields.begin().value()->_analyzers[1];
      EXPECT_EQ("_system::empty", analyzer._pool->name());
      EXPECT_EQ("empty", analyzer._pool->type());
      EXPECT_EQUAL_SLICES(VPackParser::fromJson("{\"args\" : \"ru\"}")->slice(),
                          analyzer._pool->properties());
      EXPECT_EQ(1, analyzer._pool->features().size());
      EXPECT_TRUE(analyzer._pool->features().check(irs::type<irs::frequency>::id()));
      EXPECT_EQ("::empty", analyzer._shortName);
      EXPECT_EQ((*meta._analyzerDefinitions.find(analyzer._pool->name())), analyzer._pool);
    }
  }

  // existing analyzer (full) analyzer creation not allowed (pass)
  {
    auto json = VPackParser::fromJson(
        "{ \
      \"analyzerDefinitions\": [ { \"name\": \"empty\", \"type\": \"empty\", \"properties\": {\"args\":\"de\"}, \"features\": [ \"frequency\" ] } ], \
      \"analyzers\": [ \"empty\" ] \
    }");
    arangodb::iresearch::IResearchLinkMeta meta;
    std::string errorField;
    EXPECT_TRUE(meta.init(server.server(), json->slice(), true, errorField, vocbase.name()));
    EXPECT_EQ(1, meta._analyzers.size());
    EXPECT_EQ(std::string("testVocbase::empty"), meta._analyzers[0]._pool->name());
    EXPECT_EQ(std::string("empty"), meta._analyzers[0]._pool->type());
    EXPECT_EQUAL_SLICES(
        VPackParser::fromJson("{\"args\" : \"de\"}")->slice(),
        meta._analyzers[0]._pool->properties());
    EXPECT_EQ(1, meta._analyzers[0]._pool->features().size());
    EXPECT_TRUE(meta._analyzers[0]._pool->features().check(irs::type<irs::frequency>::id()));
    EXPECT_EQ(std::string("empty"), meta._analyzers[0]._shortName);
  }

  // existing analyzer (full) analyzer definition not allowed
  {
    auto json = VPackParser::fromJson(
        "{ \
      \"analyzers\": [ { \"name\": \"empty\", \"type\": \"empty\", \"properties\": {\"args\":\"de\"}, \"features\": [ \"frequency\" ] } ], \
      \"analyzers\": [ \"empty\" ] \
    }");
    arangodb::iresearch::IResearchLinkMeta meta;
    std::string errorField;
    EXPECT_FALSE(meta.init(server.server(), json->slice(), false, errorField, vocbase.name()));
    EXPECT_EQ(std::string("analyzers[0]"), errorField);
  }

  // existing analyzer (full)
  {
    auto json = VPackParser::fromJson(
        "{ \
      \"analyzerDefinitions\": [ { \"name\": \"empty\", \"type\": \"empty\", \"properties\": {\"args\":\"de\"}, \"features\": [ \"frequency\" ] } ], \
      \"analyzers\": [ \"empty\" ] \
    }");
    arangodb::iresearch::IResearchLinkMeta meta;
    std::string errorField;
    EXPECT_TRUE(meta.init(server.server(), json->slice(), true, errorField, vocbase.name()));
    EXPECT_EQ(1, meta._analyzers.size());
    EXPECT_EQ(std::string("testVocbase::empty"), meta._analyzers[0]._pool->name());
    EXPECT_EQ(std::string("empty"), meta._analyzers[0]._pool->type());
    EXPECT_EQUAL_SLICES(
        VPackParser::fromJson("{\"args\" : \"de\"}")->slice(),
        meta._analyzers[0]._pool->properties());
    EXPECT_EQ(1, meta._analyzers[0]._pool->features().size());
    EXPECT_TRUE(meta._analyzers[0]._pool->features().check(irs::type<irs::frequency>::id()));
    EXPECT_EQ(std::string("empty"), meta._analyzers[0]._shortName);
  }

  // existing analyzer (full) inRecovery
  {
    auto json = VPackParser::fromJson(
        "{ \
      \"analyzerDefinitions\": [ { \"name\": \"empty\", \"type\": \"empty\", \"properties\": {\"args\":\"de\"}, \"features\": [ \"frequency\" ] } ], \
      \"analyzers\": [ \"empty\" ] \
    }");
    auto before = StorageEngineMock::recoveryStateResult;
    StorageEngineMock::recoveryStateResult = arangodb::RecoveryState::IN_PROGRESS;
    auto restore = irs::make_finally(
        [&before]() -> void { StorageEngineMock::recoveryStateResult = before; });
    arangodb::iresearch::IResearchLinkMeta meta;
    std::string errorField;
    EXPECT_TRUE(meta.init(server.server(), json->slice(), true, errorField, vocbase.name()));
    EXPECT_EQ(1, meta._analyzers.size());
    EXPECT_EQ("testVocbase::empty", meta._analyzers[0]._pool->name());
    EXPECT_EQ("empty", meta._analyzers[0]._pool->type());
    EXPECT_EQUAL_SLICES(
        VPackParser::fromJson("{\"args\" : \"de\"}")->slice(),
        meta._analyzers[0]._pool->properties());
    EXPECT_EQ(1, meta._analyzers[0]._pool->features().size());
    EXPECT_TRUE(meta._analyzers[0]._pool->features().check(irs::type<irs::frequency>::id()));
    EXPECT_EQ("empty", meta._analyzers[0]._shortName);
  }

  // existing analyzer (definition mismatch)
  {
    auto json = VPackParser::fromJson(
        "{ \
      \"analyzerDefinitions\": [ { \"name\": \"empty\", \"type\": \"empty\", \"properties\": {\"args\":\"ru\"}, \"features\": [ \"frequency\" ] } ], \
      \"analyzers\": [ \"empty\" ] \
    }");
    arangodb::iresearch::IResearchLinkMeta meta;
    std::string errorField;
    EXPECT_TRUE(meta.init(server.server(), json->slice(), true, errorField, vocbase.name()));
    EXPECT_EQ(1, meta._analyzers.size());
    EXPECT_EQ("testVocbase::empty", meta._analyzers[0]._pool->name());
    EXPECT_EQ("empty", meta._analyzers[0]._pool->type());
    EXPECT_EQUAL_SLICES(
        VPackParser::fromJson("{\"args\" : \"ru\"}")->slice(),
        meta._analyzers[0]._pool->properties());
    EXPECT_EQ(1, meta._analyzers[0]._pool->features().size());
    EXPECT_TRUE(meta._analyzers[0]._pool->features().check(irs::type<irs::frequency>::id()));
    EXPECT_EQ("empty", meta._analyzers[0]._shortName);
  }

  // existing analyzer (definition mismatch), don't read definitions
  {
    auto json = VPackParser::fromJson(
        "{ \
      \"analyzerDefinitions\": [ { \"name\": \"empty\", \"type\": \"empty\", \"properties\": {\"args\":\"ru\"}, \"features\": [ \"frequency\" ] } ], \
      \"analyzers\": [ \"empty\" ] \
    }");
    arangodb::iresearch::IResearchLinkMeta meta;
    std::string errorField;
    EXPECT_TRUE(meta.init(server.server(), json->slice(), false, errorField, vocbase.name()));
    EXPECT_EQ(1, meta._analyzers.size());
    EXPECT_EQ("testVocbase::empty", meta._analyzers[0]._pool->name());
    EXPECT_EQ("empty", meta._analyzers[0]._pool->type());
    // read definition from cache since we ignore "analyzerDefinitions"
    EXPECT_EQUAL_SLICES(
        VPackParser::fromJson("{\"args\" : \"de\"}")->slice(),
        meta._analyzers[0]._pool->properties());
    EXPECT_EQ(1, meta._analyzers[0]._pool->features().size());
    EXPECT_TRUE(meta._analyzers[0]._pool->features().check(irs::type<irs::frequency>::id()));
    EXPECT_EQ("empty", meta._analyzers[0]._shortName);
  }

  // existing analyzer (definition mismatch) inRecovery
  {
    auto json = VPackParser::fromJson(
        "{ \
      \"analyzerDefinitions\": [ { \"name\": \"empty\", \"type\": \"empty\", \"properties\": {\"args\":\"ru\"}, \"features\": [ \"frequency\" ] } ], \
      \"analyzers\": [ \"empty\" ] \
    }");
    auto before = StorageEngineMock::recoveryStateResult;
    StorageEngineMock::recoveryStateResult = arangodb::RecoveryState::IN_PROGRESS;
    auto restore = irs::make_finally(
        [&before]() -> void { StorageEngineMock::recoveryStateResult = before; });
    arangodb::iresearch::IResearchLinkMeta meta;
    std::string errorField;
    EXPECT_TRUE(meta.init(server.server(), json->slice(), true, errorField, vocbase.name()));
    EXPECT_EQ(1, meta._analyzers.size());
    EXPECT_EQ("testVocbase::empty", meta._analyzers[0]._pool->name());
    EXPECT_EQ("empty", meta._analyzers[0]._pool->type());
    // read definition from cache since we ignore "analyzerDefinitions"
    EXPECT_EQUAL_SLICES(
        VPackParser::fromJson("{\"args\" : \"ru\"}")->slice(),
        meta._analyzers[0]._pool->properties());
    EXPECT_EQ(1, meta._analyzers[0]._pool->features().size());
    EXPECT_TRUE(meta._analyzers[0]._pool->features().check(irs::type<irs::frequency>::id()));
    EXPECT_EQ("empty", meta._analyzers[0]._shortName);
  }

  // existing analyzer (definition mismatch) inRecovery, don't read definitions
  {
    auto json = VPackParser::fromJson(
        "{ \
      \"analyzerDefinitions\": [ { \"name\": \"empty\", \"type\": \"empty\", \"properties\": {\"args\":\"ru\"}, \"features\": [ \"frequency\" ] } ], \
      \"analyzers\": [ \"empty\" ] \
    }");
    auto before = StorageEngineMock::recoveryStateResult;
    StorageEngineMock::recoveryStateResult = arangodb::RecoveryState::IN_PROGRESS;
    auto restore = irs::make_finally(
        [&before]() -> void { StorageEngineMock::recoveryStateResult = before; });
    arangodb::iresearch::IResearchLinkMeta meta;
    std::string errorField;
    EXPECT_TRUE(meta.init(server.server(), json->slice(), false, errorField, vocbase.name()));
    EXPECT_EQ(1, meta._analyzers.size());
    EXPECT_EQ("testVocbase::empty", meta._analyzers[0]._pool->name());
    EXPECT_EQ("empty", meta._analyzers[0]._pool->type());
    // read definition from cache since we ignore "analyzerDefinitions"
    EXPECT_EQUAL_SLICES(
        VPackParser::fromJson("{\"args\" : \"de\"}")->slice(),
        meta._analyzers[0]._pool->properties());
    EXPECT_EQ(1, meta._analyzers[0]._pool->features().size());
    EXPECT_TRUE(meta._analyzers[0]._pool->features().check(irs::type<irs::frequency>::id()));
    EXPECT_EQ("empty", meta._analyzers[0]._shortName);
  }

  // test analyzers with definitions and many fields
  {
    auto lhs = arangodb::velocypack::Parser::fromJson(
      "{ \"view\": \"a/\", \"includeAllFields\": false, \"analyzers\":[\"testAnalyzer\", \"testAnalyzer2\"],"
      "  \"fields\":{ \"owner\": {}, \"departments\": {}, \"id\": {}, \"permission\": {}, \"status\": {}, \"type\": {} },"
      "  \"analyzerDefinitions\":[ "
      "    {\"name\":\"testAnalyzer\", \"type\":\"ngram\", \"properties\":{\"min\":2, \"max\":2, \"preserveOriginal\": false}}, "
      "    {\"name\":\"testAnalyzer2\", \"type\":\"ngram\", \"properties\":{\"min\":3, \"max\":3, \"preserveOriginal\": false}} "
      "  ]}");
    arangodb::iresearch::IResearchLinkMeta meta;
    std::string errorField;
    ASSERT_TRUE(meta.init(server.server(), lhs->slice(), true, errorField, vocbase.name()));
    arangodb::iresearch::IResearchLinkMeta meta2;
    ASSERT_TRUE(meta2.init(server.server(), lhs->slice(), true, errorField, vocbase.name()));
    ASSERT_EQ(meta, meta2);
    arangodb::iresearch::IResearchLinkMeta meta_assigned;
    meta_assigned = meta;
    ASSERT_EQ(meta, meta_assigned);
    arangodb::iresearch::IResearchLinkMeta meta_copied(meta);
    ASSERT_EQ(meta_copied, meta);
  }
}

// https://github.com/arangodb/backlog/issues/581
// (ArangoSearch view doesn't validate uniqueness of analyzers)
TEST_F(IResearchLinkMetaTest, test_addNonUniqueAnalyzers) {
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
  auto& analyzers = server.getFeature<arangodb::iresearch::IResearchAnalyzerFeature>();

  const std::string analyzerCustomName = "test_addNonUniqueAnalyzersMyIdentity";
  const std::string analyzerCustomInSystem =
      arangodb::StaticStrings::SystemDatabase + "::" + analyzerCustomName;
  const std::string analyzerCustomInTestVocbase = vocbase.name() + "::" + analyzerCustomName;

  // this is for test cleanup
  auto testCleanup = irs::make_finally([&analyzerCustomInSystem, &analyzers,
                                        &analyzerCustomInTestVocbase]() -> void {
    analyzers.remove(analyzerCustomInSystem);
    analyzers.remove(analyzerCustomInTestVocbase);
  });

  {
    arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult emplaceResult;
    // we need custom analyzer in _SYSTEM database and other will be in testVocbase with same name (it is ok to add both!).
    analyzers.emplace(emplaceResult, analyzerCustomInSystem, "identity",
                      VPackParser::fromJson("{ \"args\": \"en\" }")->slice(),
                      {irs::type<irs::frequency>::get()});
  }

  {
    arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult emplaceResult;
    analyzers.emplace(emplaceResult, analyzerCustomInTestVocbase, "identity",
                      VPackParser::fromJson("{ \"args\": \"en\" }")->slice(),
                      {irs::type<irs::frequency>::get()});
  }

  {
    const std::string testJson = std::string(
        "{ \
          \"includeAllFields\": true, \
          \"trackListPositions\": true, \
          \"storeValues\": \"value\",\
          \"analyzers\": [ \"identity\",\"identity\""  // two built-in analyzers
        ", \"" +
        analyzerCustomInTestVocbase + "\"" +    // local analyzer by full name
        ", \"" + analyzerCustomName + "\"" +    // local analyzer by short name
        ", \"::" + analyzerCustomName + "\"" +  // _system analyzer by short name
        ", \"" + analyzerCustomInSystem + "\"" +  // _system analyzer by full name
        " ] \
        }");

    arangodb::iresearch::IResearchLinkMeta meta;

    std::unordered_set<std::string> expectedAnalyzers;
    expectedAnalyzers.insert(analyzers.get("identity", arangodb::QueryAnalyzerRevisions::QUERY_LATEST)->name());
    expectedAnalyzers.insert(analyzers.get(analyzerCustomInTestVocbase, arangodb::QueryAnalyzerRevisions::QUERY_LATEST)->name());
    expectedAnalyzers.insert(analyzers.get(analyzerCustomInSystem, arangodb::QueryAnalyzerRevisions::QUERY_LATEST)->name());

    arangodb::iresearch::IResearchLinkMeta::Mask mask(false);
    auto json = VPackParser::fromJson(testJson);
    std::string errorField;
    EXPECT_TRUE(true ==
                meta.init(server.server(), json->slice(), false, errorField, vocbase.name(),
                          arangodb::iresearch::IResearchLinkMeta::DEFAULT(), &mask));
    EXPECT_TRUE(mask._analyzers);
    EXPECT_TRUE(errorField.empty());
    EXPECT_EQ(expectedAnalyzers.size(), meta._analyzers.size());

    for (decltype(meta._analyzers)::const_iterator analyzersItr =
             meta._analyzers.begin();
         analyzersItr != meta._analyzers.end(); ++analyzersItr) {
      EXPECT_EQ(1, expectedAnalyzers.erase(analyzersItr->_pool->name()));
    }
    EXPECT_TRUE(expectedAnalyzers.empty());
  }
}

// https://arangodb.atlassian.net/browse/ES-700
// Arango View Definition Partial Edit leads to View Rebuild (on Web UI)
// Reason: During server start SystemDatabaseFeature is not available
// while initing links so analyzer names are not properly normalized
class IResearchLinkMetaTestNoSystem
  : public ::testing::Test,
  public arangodb::tests::LogSuppressor<arangodb::Logger::AGENCYCOMM, arangodb::LogLevel::FATAL>,
  public arangodb::tests::LogSuppressor<arangodb::Logger::AUTHENTICATION, arangodb::LogLevel::ERR> {
protected:
  arangodb::tests::mocks::MockAqlServer server;

  IResearchLinkMetaTestNoSystem() {
    arangodb::tests::init();

    auto& dbFeature = server.getFeature<arangodb::DatabaseFeature>();
    auto sysvocbase = dbFeature.useDatabase(arangodb::StaticStrings::SystemDatabase);
    std::shared_ptr<arangodb::LogicalCollection> unused;
    arangodb::OperationOptions options(arangodb::ExecContext::current());
    arangodb::methods::Collections::createSystem(*sysvocbase, options,
      arangodb::tests::AnalyzerCollectionName,
      false, unused);
    unused = nullptr;

    TRI_vocbase_t* vocbase;
    dbFeature.createDatabase(testDBInfo(server.server()), vocbase);  // required for IResearchAnalyzerFeature::emplace(...)
    arangodb::methods::Collections::createSystem(*vocbase, options,
      arangodb::tests::AnalyzerCollectionName,
      false, unused);

    auto& analyzers = server.getFeature<arangodb::iresearch::IResearchAnalyzerFeature>();
    arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;

    analyzers.emplace(result, "testVocbase::empty", "empty",
      VPackParser::fromJson("{ \"args\": \"de\" }")->slice(),
      irs::flags{irs::type<irs::frequency>::get()});  // cache the 'empty' analyzer for 'testVocbase'

    // intentionally unprepare database feature to simulate server loading case where
    // DatabaseFeature is starting and loading Links but SystemDatabaseFeature cannot provide system database
    server.getFeature<arangodb::SystemDatabaseFeature>().unprepare();
  }
};

TEST_F(IResearchLinkMetaTestNoSystem, test_readAnalyzerDefinitions) {
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));

  ASSERT_EQ(nullptr, server.getFeature<arangodb::SystemDatabaseFeature>().use());
  // this is copy of IResearchLinkMetaTest::test_readAnalyzerDefinitions
  // main point here is name normalization checks!

  // missing analyzer (name only)
  {
    auto json = VPackParser::fromJson(
      "{ \
      \"analyzers\": [ \"empty1\" ] \
    }");
    arangodb::iresearch::IResearchLinkMeta meta;
    std::string errorField;
    EXPECT_FALSE(meta.init(server.server(), json->slice(), true, errorField, vocbase.name()));
    EXPECT_EQ(std::string("analyzers.empty1"), errorField);
  }

  // missing analyzer (name only) inRecovery
  {
    auto json = VPackParser::fromJson(
      "{ \
      \"analyzers\": [ \"empty1\" ] \
    }");
    auto before = StorageEngineMock::recoveryStateResult;
    StorageEngineMock::recoveryStateResult = arangodb::RecoveryState::IN_PROGRESS;
    auto restore = irs::make_finally(
      [&before]() -> void { StorageEngineMock::recoveryStateResult = before; });
    arangodb::iresearch::IResearchLinkMeta meta;
    std::string errorField;
    EXPECT_FALSE(meta.init(server.server(), json->slice(), true, errorField, vocbase.name()));
    EXPECT_EQ(std::string("analyzers.empty1"), errorField);
  }

  // missing analyzer (full) single-server
  {
    auto json = VPackParser::fromJson(
      "{ \
      \"analyzerDefinitions\": [ { \"name\": \"missing0\", \"type\": \"empty\", \"properties\": {\"args\":\"ru\"}, \"features\": [ \"frequency\" ] } ], \
      \"analyzers\": [ \"missing0\" ] \
    }");
    arangodb::iresearch::IResearchLinkMeta meta;
    std::string errorField;
    EXPECT_TRUE(meta.init(server.server(), json->slice(), true, errorField, vocbase.name()));
    EXPECT_EQ(1, meta._analyzers.size());
    // Name should be properly normalized!
    EXPECT_EQ(std::string("testVocbase::missing0"), meta._analyzers[0]._pool->name());
    EXPECT_EQ(std::string("empty"), meta._analyzers[0]._pool->type());
    EXPECT_EQUAL_SLICES(VPackParser::fromJson("{\"args\" : \"ru\"}")->slice(),
      meta._analyzers[0]._pool->properties());
    EXPECT_EQ(1, meta._analyzers[0]._pool->features().size());
    EXPECT_TRUE(meta._analyzers[0]._pool->features().check(irs::type<irs::frequency>::id()));
    EXPECT_EQ("missing0", meta._analyzers[0]._shortName);
  }

  // complex definition on single-server
  {
    auto json = VPackParser::fromJson(
      "{ \
      \"analyzerDefinitions\": [ \
         { \"name\": \"empty\", \"type\": \"empty\", \"properties\": {\"args\":\"ru\"}, \"features\": [ \"frequency\" ] }, \
         { \"name\": \"::empty\", \"type\": \"empty\", \"properties\": {\"args\":\"ru\"}, \"features\": [ \"frequency\" ] } \
      ], \
      \"fields\" : { \"field\": { \"analyzers\": [ \"testVocbase::empty\", \"empty\", \"_system::empty\", \"::empty\" ] } } \
    }");
    arangodb::iresearch::IResearchLinkMeta meta;
    std::string errorField;
    EXPECT_TRUE(meta.init(server.server(), json->slice(), true, errorField, vocbase.name()));
    EXPECT_EQ(2, meta._analyzerDefinitions.size());
    EXPECT_EQ(1, meta._analyzers.size());
    {
      auto& analyzer = meta._analyzers[0];
      EXPECT_EQ(arangodb::iresearch::IResearchAnalyzerFeature::identity(), analyzer._pool);
    }
    ASSERT_EQ(1, meta._fields.size());
    ASSERT_EQ(2, meta._fields.begin().value()->_analyzers.size());
    {
      auto& analyzer = meta._fields.begin().value()->_analyzers[0];
      EXPECT_EQ("testVocbase::empty", analyzer._pool->name());
      EXPECT_EQ("empty", analyzer._pool->type());
      EXPECT_EQUAL_SLICES(VPackParser::fromJson("{\"args\" : \"ru\"}")->slice(),
        analyzer._pool->properties());
      EXPECT_EQ(1, analyzer._pool->features().size());
      EXPECT_TRUE(analyzer._pool->features().check(irs::type<irs::frequency>::id()));
      EXPECT_EQ("empty", analyzer._shortName);
      EXPECT_EQ((*meta._analyzerDefinitions.find(analyzer._pool->name())), analyzer._pool);
    }
    {
      auto& analyzer = meta._fields.begin().value()->_analyzers[1];
      EXPECT_EQ("_system::empty", analyzer._pool->name());
      EXPECT_EQ("empty", analyzer._pool->type());
      EXPECT_EQUAL_SLICES(VPackParser::fromJson("{\"args\" : \"ru\"}")->slice(),
        analyzer._pool->properties());
      EXPECT_EQ(1, analyzer._pool->features().size());
      EXPECT_TRUE(analyzer._pool->features().check(irs::type<irs::frequency>::id()));
      EXPECT_EQ("::empty", analyzer._shortName);
      EXPECT_EQ((*meta._analyzerDefinitions.find(analyzer._pool->name())), analyzer._pool);
    }
  }

  // complex definition on single-server
  {
    auto json = VPackParser::fromJson(
      "{ \
      \"analyzerDefinitions\": [ \
         { \"name\": \"::empty\", \"type\": \"empty\", \"properties\": {\"args\":\"ru\"}, \"features\": [ \"frequency\" ] } \
      ], \
      \"fields\" : { \"field\": { \"analyzers\": [ \"testVocbase::empty\", \"empty\", \"_system::empty\", \"::empty\" ] } } \
    }");
    arangodb::iresearch::IResearchLinkMeta meta;
    std::string errorField;
    ASSERT_TRUE(meta.init(server.server(), json->slice(), true, errorField, vocbase.name()));
    EXPECT_EQ(2, meta._analyzerDefinitions.size());
    EXPECT_EQ(1, meta._analyzers.size());
    {
      auto& analyzer = meta._analyzers[0];
      EXPECT_EQ(arangodb::iresearch::IResearchAnalyzerFeature::identity(), analyzer._pool);
    }
    ASSERT_EQ(1, meta._fields.size());
    ASSERT_EQ(2, meta._fields.begin().value()->_analyzers.size());
    {
      auto& analyzer = meta._fields.begin().value()->_analyzers[0];
      EXPECT_EQ("testVocbase::empty", analyzer._pool->name());
      EXPECT_EQ("empty", analyzer._pool->type());
      // definition from cache since it's not presented "analyzerDefinitions"
      EXPECT_EQUAL_SLICES(VPackParser::fromJson("{\"args\" : \"de\"}")->slice(),
        analyzer._pool->properties());
      EXPECT_EQ(1, analyzer._pool->features().size());
      EXPECT_TRUE(analyzer._pool->features().check(irs::type<irs::frequency>::id()));
      EXPECT_EQ("empty", analyzer._shortName);
      EXPECT_EQ((*meta._analyzerDefinitions.find(analyzer._pool->name())), analyzer._pool);
    }
    {
      auto& analyzer = meta._fields.begin().value()->_analyzers[1];
      EXPECT_EQ("_system::empty", analyzer._pool->name());
      EXPECT_EQ("empty", analyzer._pool->type());
      EXPECT_EQUAL_SLICES(VPackParser::fromJson("{\"args\" : \"ru\"}")->slice(),
        analyzer._pool->properties());
      EXPECT_EQ(1, analyzer._pool->features().size());
      EXPECT_TRUE(analyzer._pool->features().check(irs::type<irs::frequency>::id()));
      EXPECT_EQ("::empty", analyzer._shortName);
      EXPECT_EQ((*meta._analyzerDefinitions.find(analyzer._pool->name())), analyzer._pool);
    }
  }

  // missing analyzer definition coordinator
  {
    auto before = arangodb::ServerState::instance()->getRole();
    arangodb::ServerState::instance()->setRole(arangodb::ServerState::ROLE_COORDINATOR);
    auto restore = irs::make_finally([&before]() -> void {
      arangodb::ServerState::instance()->setRole(before);
      });

    auto json = VPackParser::fromJson(
      "{ \
      \"analyzers\": [ \"missing1\" ] \
    }");
    arangodb::iresearch::IResearchLinkMeta meta;
    std::string errorField;
    EXPECT_FALSE(meta.init(server.server(), json->slice(), true, errorField, vocbase.name()));
    EXPECT_EQ("analyzers.missing1", errorField);
  }

  // missing analyzer definition coordinator
  {
    auto before = arangodb::ServerState::instance()->getRole();
    arangodb::ServerState::instance()->setRole(arangodb::ServerState::ROLE_COORDINATOR);
    auto restore = irs::make_finally([&before]() -> void {
      arangodb::ServerState::instance()->setRole(before);
      });

    auto json = VPackParser::fromJson(
      "{ \
      \"fields\" : { \"field\": { \"analyzers\": [ \"missing1\" ] } } \
    }");
    arangodb::iresearch::IResearchLinkMeta meta;
    std::string errorField;
    EXPECT_FALSE(meta.init(server.server(), json->slice(), true, errorField, vocbase.name()));
    EXPECT_EQ("fields.field.analyzers.missing1", errorField);
  }

  // missing analyzer (full) coordinator
  {
    auto before = arangodb::ServerState::instance()->getRole();
    arangodb::ServerState::instance()->setRole(arangodb::ServerState::ROLE_COORDINATOR);
    auto restore = irs::make_finally([&before]() -> void {
      arangodb::ServerState::instance()->setRole(before);
      });

    auto json = VPackParser::fromJson(
      "{ \
      \"analyzerDefinitions\": [ { \"name\": \"missing1\", \"type\": \"empty\", \"properties\": {\"args\":\"ru\"}, \"features\": [ \"frequency\" ] } ], \
      \"analyzers\": [ \"missing1\" ] \
    }");
    arangodb::iresearch::IResearchLinkMeta meta;
    std::string errorField;
    EXPECT_TRUE(meta.init(server.server(), json->slice(), true, errorField, vocbase.name()));
    EXPECT_EQ(1, meta._analyzers.size());
    EXPECT_EQ("testVocbase::missing1", meta._analyzers[0]._pool->name());
    EXPECT_EQ("empty", meta._analyzers[0]._pool->type());
    EXPECT_EQUAL_SLICES(VPackParser::fromJson("{\"args\" : \"ru\"}")->slice(),
      meta._analyzers[0]._pool->properties());
    EXPECT_EQ(1, meta._analyzers[0]._pool->features().size());
    EXPECT_TRUE(meta._analyzers[0]._pool->features().check(irs::type<irs::frequency>::id()));
    EXPECT_EQ("missing1", meta._analyzers[0]._shortName);
  }

  // missing analyzer (full) coordinator (ignore analyzer definition)
  {
    auto before = arangodb::ServerState::instance()->getRole();
    arangodb::ServerState::instance()->setRole(arangodb::ServerState::ROLE_COORDINATOR);
    auto restore = irs::make_finally([&before]() -> void {
      arangodb::ServerState::instance()->setRole(before);
      });

    auto json = VPackParser::fromJson(
      "{ \
      \"analyzerDefinitions\": [ { \"name\": \"missing1\", \"type\": \"empty\", \"properties\": {\"args\":\"ru\"}, \"features\": [ \"frequency\" ] } ], \
      \"analyzers\": [ \"missing1\" ] \
    }");
    arangodb::iresearch::IResearchLinkMeta meta;
    std::string errorField;
    EXPECT_FALSE(meta.init(server.server(), json->slice(), false, errorField, vocbase.name()));
    EXPECT_EQ("analyzers.missing1", errorField);
  }

  // complex definition on coordinator
  {
    auto before = arangodb::ServerState::instance()->getRole();
    arangodb::ServerState::instance()->setRole(arangodb::ServerState::ROLE_COORDINATOR);
    auto restore = irs::make_finally([&before]() -> void {
      arangodb::ServerState::instance()->setRole(before);
      });

    auto json = VPackParser::fromJson(
      "{ \
      \"analyzerDefinitions\": [ \
         { \"name\": \"empty\", \"type\": \"empty\", \"properties\": {\"args\":\"ru\"}, \"features\": [ \"frequency\" ] }, \
         { \"name\": \"::empty\", \"type\": \"empty\", \"properties\": {\"args\":\"ru\"}, \"features\": [ \"frequency\" ] } \
      ], \
      \"fields\" : { \"field\": { \"analyzers\": [ \"testVocbase::empty\", \"empty\", \"_system::empty\", \"::empty\" ] } } \
    }");
    arangodb::iresearch::IResearchLinkMeta meta;
    std::string errorField;
    EXPECT_TRUE(meta.init(server.server(), json->slice(), true, errorField, vocbase.name()));
    EXPECT_EQ(2, meta._analyzerDefinitions.size());
    EXPECT_EQ(1, meta._analyzers.size());
    {
      auto& analyzer = meta._analyzers[0];
      EXPECT_EQ(arangodb::iresearch::IResearchAnalyzerFeature::identity(), analyzer._pool);
    }

    ASSERT_EQ(1, meta._fields.size());
    ASSERT_EQ(2, meta._fields.begin().value()->_analyzers.size());
    {
      auto& analyzer = meta._fields.begin().value()->_analyzers[0];
      EXPECT_EQ(std::string("testVocbase::empty"), analyzer._pool->name());
      EXPECT_EQ(std::string("empty"), analyzer._pool->type());
      EXPECT_EQUAL_SLICES(VPackParser::fromJson("{\"args\" : \"ru\"}")->slice(),
        analyzer._pool->properties());
      EXPECT_EQ(1, analyzer._pool->features().size());
      EXPECT_TRUE(analyzer._pool->features().check(irs::type<irs::frequency>::id()));
      EXPECT_EQ("empty", analyzer._shortName);
      EXPECT_EQ((*meta._analyzerDefinitions.find(analyzer._pool->name())), analyzer._pool);
    }
    {
      auto& analyzer = meta._fields.begin().value()->_analyzers[1];
      EXPECT_EQ(std::string("_system::empty"), analyzer._pool->name());
      EXPECT_EQ(std::string("empty"), analyzer._pool->type());
      EXPECT_EQUAL_SLICES(VPackParser::fromJson("{\"args\" : \"ru\"}")->slice(),
        analyzer._pool->properties());
      EXPECT_EQ(1, analyzer._pool->features().size());
      EXPECT_TRUE(analyzer._pool->features().check(irs::type<irs::frequency>::id()));
      EXPECT_EQ("::empty", analyzer._shortName);
      EXPECT_EQ((*meta._analyzerDefinitions.find(analyzer._pool->name())), analyzer._pool);
    }
  }

  // complex definition on coordinator
  {
    auto before = arangodb::ServerState::instance()->getRole();
    arangodb::ServerState::instance()->setRole(arangodb::ServerState::ROLE_COORDINATOR);
    auto restore = irs::make_finally([&before]() -> void {
      arangodb::ServerState::instance()->setRole(before);
      });

    auto json = VPackParser::fromJson(
      "{ \
      \"analyzerDefinitions\": [ \
         { \"name\": \"::empty\", \"type\": \"empty\", \"properties\": {\"args\":\"ru\"}, \"features\": [ \"frequency\" ] } \
      ], \
      \"fields\" : { \"field\": { \"analyzers\": [ \"testVocbase::empty\", \"empty\", \"_system::empty\", \"::empty\" ] } } \
    }");
    arangodb::iresearch::IResearchLinkMeta meta;
    std::string errorField;
    EXPECT_TRUE(meta.init(server.server(), json->slice(), true, errorField, vocbase.name()));
    EXPECT_EQ(2, meta._analyzerDefinitions.size());
    EXPECT_EQ(1, meta._analyzers.size());
    {
      auto& analyzer = meta._analyzers[0];
      EXPECT_EQ(arangodb::iresearch::IResearchAnalyzerFeature::identity(), analyzer._pool);
    }

    ASSERT_EQ(1, meta._fields.size());
    ASSERT_EQ(2, meta._fields.begin().value()->_analyzers.size());
    {
      auto& analyzer = meta._fields.begin().value()->_analyzers[0];
      EXPECT_EQ("testVocbase::empty", analyzer._pool->name());
      EXPECT_EQ("empty", analyzer._pool->type());
      // definition from cache since it's not presented "analyzerDefinitions"
      EXPECT_EQUAL_SLICES(VPackParser::fromJson("{\"args\" : \"de\"}")->slice(),
        analyzer._pool->properties());
      EXPECT_EQ(1, analyzer._pool->features().size());
      EXPECT_TRUE(analyzer._pool->features().check(irs::type<irs::frequency>::id()));
      EXPECT_EQ("empty", analyzer._shortName);
      EXPECT_EQ((*meta._analyzerDefinitions.find(analyzer._pool->name())), analyzer._pool);
    }
    {
      auto& analyzer = meta._fields.begin().value()->_analyzers[1];
      EXPECT_EQ("_system::empty", analyzer._pool->name());
      EXPECT_EQ("empty", analyzer._pool->type());
      EXPECT_EQUAL_SLICES(VPackParser::fromJson("{\"args\" : \"ru\"}")->slice(),
        analyzer._pool->properties());
      EXPECT_EQ(1, analyzer._pool->features().size());
      EXPECT_TRUE(analyzer._pool->features().check(irs::type<irs::frequency>::id()));
      EXPECT_EQ("::empty", analyzer._shortName);
      EXPECT_EQ((*meta._analyzerDefinitions.find(analyzer._pool->name())), analyzer._pool);
    }
  }

  // missing analyzer (full) db-server
  {
    auto before = arangodb::ServerState::instance()->getRole();
    arangodb::ServerState::instance()->setRole(arangodb::ServerState::ROLE_DBSERVER);
    auto restore = irs::make_finally([&before]() -> void {
      arangodb::ServerState::instance()->setRole(before);
      });

    auto json = VPackParser::fromJson(
      "{ \
      \"analyzerDefinitions\": [ { \"name\": \"missing2\", \"type\": \"empty\", \"properties\": { \"args\": \"ru\" }, \"features\": [ \"frequency\" ] } ], \
      \"analyzers\": [ \"missing2\" ] \
    }");
    arangodb::iresearch::IResearchLinkMeta meta;
    std::string errorField;
    EXPECT_TRUE(meta.init(server.server(), json->slice(), true, errorField, vocbase.name()));
    EXPECT_EQ(1, meta._analyzers.size());
    EXPECT_EQ("testVocbase::missing2", meta._analyzers[0]._pool->name());
    EXPECT_EQ("empty", meta._analyzers[0]._pool->type());
    EXPECT_EQ(std::string("empty"), meta._analyzers[0]._pool->type());
    EXPECT_EQUAL_SLICES(VPackParser::fromJson("{\"args\" : \"ru\"}")->slice(),
      meta._analyzers[0]._pool->properties());
    EXPECT_EQ(1, meta._analyzers[0]._pool->features().size());
    EXPECT_TRUE(meta._analyzers[0]._pool->features().check(irs::type<irs::frequency>::id()));
    EXPECT_EQ("missing2", meta._analyzers[0]._shortName);
  }

  // missing analyzer (full) db-server (ignore analyzer definition)
  {
    auto before = arangodb::ServerState::instance()->getRole();
    arangodb::ServerState::instance()->setRole(arangodb::ServerState::ROLE_DBSERVER);
    auto restore = irs::make_finally([&before]() -> void {
      arangodb::ServerState::instance()->setRole(before);
      });

    auto json = VPackParser::fromJson(
      "{ \
      \"analyzerDefinitions\": [ { \"name\": \"missing2\", \"type\": \"empty\", \"properties\": { \"args\": \"ru\" }, \"features\": [ \"frequency\" ] } ], \
      \"analyzers\": [ \"missing2\" ] \
    }");
    arangodb::iresearch::IResearchLinkMeta meta;
    std::string errorField;
    EXPECT_FALSE(meta.init(server.server(), json->slice(), false, errorField, vocbase.name()));
    EXPECT_EQ("analyzers.missing2", errorField);
  }

  // missing analyzer definition db-server
  {
    auto before = arangodb::ServerState::instance()->getRole();
    arangodb::ServerState::instance()->setRole(arangodb::ServerState::ROLE_DBSERVER);
    auto restore = irs::make_finally([&before]() -> void {
      arangodb::ServerState::instance()->setRole(before);
      });

    auto json = VPackParser::fromJson(
      "{ \
      \"analyzers\": [ \"missing2\" ] \
    }");
    arangodb::iresearch::IResearchLinkMeta meta;
    std::string errorField;
    EXPECT_FALSE(meta.init(server.server(), json->slice(), true, errorField, vocbase.name()));
    EXPECT_EQ("analyzers.missing2", errorField);
  }

  // complex definition on db-server
  {
    auto before = arangodb::ServerState::instance()->getRole();
    arangodb::ServerState::instance()->setRole(arangodb::ServerState::ROLE_DBSERVER);
    auto restore = irs::make_finally([&before]() -> void {
      arangodb::ServerState::instance()->setRole(before);
      });

    auto json = VPackParser::fromJson(
      "{ \
      \"analyzerDefinitions\": [ \
         { \"name\": \"empty\", \"type\": \"empty\", \"properties\": {\"args\":\"ru\"}, \"features\": [ \"frequency\" ] }, \
         { \"name\": \"::empty\", \"type\": \"empty\", \"properties\": {\"args\":\"ru\"}, \"features\": [ \"frequency\" ] } \
      ], \
      \"fields\" : { \"field\": { \"analyzers\": [ \"testVocbase::empty\", \"empty\", \"_system::empty\", \"::empty\" ] } } \
    }");
    arangodb::iresearch::IResearchLinkMeta meta;
    std::string errorField;
    EXPECT_TRUE(meta.init(server.server(), json->slice(), true, errorField, vocbase.name()));
    EXPECT_EQ(2, meta._analyzerDefinitions.size());
    EXPECT_EQ(1, meta._analyzers.size());
    {
      auto& analyzer = meta._analyzers[0];
      EXPECT_EQ(arangodb::iresearch::IResearchAnalyzerFeature::identity(), analyzer._pool);
    }

    ASSERT_EQ(1, meta._fields.size());
    ASSERT_EQ(2, meta._fields.begin().value()->_analyzers.size());
    {
      auto& analyzer = meta._fields.begin().value()->_analyzers[0];
      EXPECT_EQ(std::string("testVocbase::empty"), analyzer._pool->name());
      EXPECT_EQ(std::string("empty"), analyzer._pool->type());
      EXPECT_EQUAL_SLICES(VPackParser::fromJson("{\"args\" : \"ru\"}")->slice(),
        analyzer._pool->properties());
      EXPECT_EQ(1, analyzer._pool->features().size());
      EXPECT_TRUE(analyzer._pool->features().check(irs::type<irs::frequency>::id()));
      EXPECT_EQ("empty", analyzer._shortName);
      EXPECT_EQ((*meta._analyzerDefinitions.find(analyzer._pool->name())), analyzer._pool);
    }
    {
      auto& analyzer = meta._fields.begin().value()->_analyzers[1];
      EXPECT_EQ(std::string("_system::empty"), analyzer._pool->name());
      EXPECT_EQ(std::string("empty"), analyzer._pool->type());
      EXPECT_EQUAL_SLICES(VPackParser::fromJson("{\"args\" : \"ru\"}")->slice(),
        analyzer._pool->properties());
      EXPECT_EQ(1, analyzer._pool->features().size());
      EXPECT_TRUE(analyzer._pool->features().check(irs::type<irs::frequency>::id()));
      EXPECT_EQ("::empty", analyzer._shortName);
      EXPECT_EQ((*meta._analyzerDefinitions.find(analyzer._pool->name())), analyzer._pool);
    }
  }

  // complex definition on db-server
  {
    auto before = arangodb::ServerState::instance()->getRole();
    arangodb::ServerState::instance()->setRole(arangodb::ServerState::ROLE_DBSERVER);
    auto restore = irs::make_finally([&before]() -> void {
      arangodb::ServerState::instance()->setRole(before);
      });

    auto json = VPackParser::fromJson(
      "{ \
      \"analyzerDefinitions\": [ \
         { \"name\": \"::empty\", \"type\": \"empty\", \"properties\": {\"args\":\"ru\"}, \"features\": [ \"frequency\" ] } \
      ], \
      \"fields\" : { \"field\": { \"analyzers\": [ \"testVocbase::empty\", \"empty\", \"_system::empty\", \"::empty\" ] } } \
    }");
    arangodb::iresearch::IResearchLinkMeta meta;
    std::string errorField;
    EXPECT_TRUE(meta.init(server.server(), json->slice(), true, errorField, vocbase.name()));
    EXPECT_EQ(2, meta._analyzerDefinitions.size());
    EXPECT_EQ(1, meta._analyzers.size());
    {
      auto& analyzer = meta._analyzers[0];
      EXPECT_EQ(arangodb::iresearch::IResearchAnalyzerFeature::identity(), analyzer._pool);
    }

    ASSERT_EQ(1, meta._fields.size());
    ASSERT_EQ(2, meta._fields.begin().value()->_analyzers.size());
    {
      auto& analyzer = meta._fields.begin().value()->_analyzers[0];
      EXPECT_EQ("testVocbase::empty", analyzer._pool->name());
      EXPECT_EQ("empty", analyzer._pool->type());
      // definition from cache since it's not presented "analyzerDefinitions"
      EXPECT_EQUAL_SLICES(VPackParser::fromJson("{\"args\" : \"de\"}")->slice(),
        analyzer._pool->properties());
      EXPECT_EQ(1, analyzer._pool->features().size());
      EXPECT_TRUE(analyzer._pool->features().check(irs::type<irs::frequency>::id()));
      EXPECT_EQ("empty", analyzer._shortName);
      EXPECT_EQ((*meta._analyzerDefinitions.find(analyzer._pool->name())), analyzer._pool);
    }
    {
      auto& analyzer = meta._fields.begin().value()->_analyzers[1];
      EXPECT_EQ("_system::empty", analyzer._pool->name());
      EXPECT_EQ("empty", analyzer._pool->type());
      EXPECT_EQUAL_SLICES(VPackParser::fromJson("{\"args\" : \"ru\"}")->slice(),
        analyzer._pool->properties());
      EXPECT_EQ(1, analyzer._pool->features().size());
      EXPECT_TRUE(analyzer._pool->features().check(irs::type<irs::frequency>::id()));
      EXPECT_EQ("::empty", analyzer._shortName);
      EXPECT_EQ((*meta._analyzerDefinitions.find(analyzer._pool->name())), analyzer._pool);
    }
  }

  // missing analyzer (full) inRecovery
  {
    auto json = VPackParser::fromJson(
      "{ \
      \"analyzerDefinitions\": [ { \"name\": \"missing3\", \"type\": \"empty\", \"properties\": {\"args\":\"ru\"}, \"features\": [ \"frequency\" ] } ], \
      \"analyzers\": [ \"missing3\" ] \
    }");
    auto before = StorageEngineMock::recoveryStateResult;
    StorageEngineMock::recoveryStateResult = arangodb::RecoveryState::IN_PROGRESS;
    auto restore = irs::make_finally(
      [&before]() -> void { StorageEngineMock::recoveryStateResult = before; });
    arangodb::iresearch::IResearchLinkMeta meta;
    std::string errorField;
    EXPECT_TRUE(meta.init(server.server(), json->slice(), true, errorField, vocbase.name()));
    EXPECT_EQ(1, meta._analyzers.size());
    EXPECT_EQ("testVocbase::missing3", meta._analyzers[0]._pool->name());
    EXPECT_EQ("empty", meta._analyzers[0]._pool->type());
    EXPECT_EQUAL_SLICES(VPackParser::fromJson("{\"args\" : \"ru\"}")->slice(),
      meta._analyzers[0]._pool->properties());
    EXPECT_EQ(1, meta._analyzers[0]._pool->features().size());
    EXPECT_TRUE(meta._analyzers[0]._pool->features().check(irs::type<irs::frequency>::id()));
    EXPECT_EQ("missing3", meta._analyzers[0]._shortName);
  }

  // missing analyzer (full) inRecovery (ignore analyzer definition)
  {
    auto json = VPackParser::fromJson(
      "{ \
      \"analyzerDefinitions\": [ { \"name\": \"missing3\", \"type\": \"empty\", \"properties\": {\"args\":\"ru\"}, \"features\": [ \"frequency\" ] } ], \
      \"analyzers\": [ \"missing3\" ] \
    }");
    auto before = StorageEngineMock::recoveryStateResult;
    StorageEngineMock::recoveryStateResult = arangodb::RecoveryState::IN_PROGRESS;
    auto restore = irs::make_finally(
      [&before]() -> void { StorageEngineMock::recoveryStateResult = before; });
    arangodb::iresearch::IResearchLinkMeta meta;
    std::string errorField;
    EXPECT_FALSE(meta.init(server.server(), json->slice(), false, errorField, vocbase.name()));
    EXPECT_EQ("analyzers.missing3", errorField);
  }

  // missing analyzer definition inRecovery
  {
    auto json = VPackParser::fromJson(
      "{ \
      \"analyzers\": [ \"missing3\" ] \
    }");
    auto before = StorageEngineMock::recoveryStateResult;
    StorageEngineMock::recoveryStateResult = arangodb::RecoveryState::IN_PROGRESS;
    auto restore = irs::make_finally(
      [&before]() -> void { StorageEngineMock::recoveryStateResult = before; });
    arangodb::iresearch::IResearchLinkMeta meta;
    std::string errorField;
    EXPECT_FALSE(meta.init(server.server(), json->slice(), true, errorField, vocbase.name()));
    EXPECT_EQ("analyzers.missing3", errorField);
  }

  // existing analyzer (name only)
  {
    auto json = VPackParser::fromJson(
      "{ \
      \"analyzers\": [ \"empty\" ] \
    }");
    arangodb::iresearch::IResearchLinkMeta meta;
    std::string errorField;
    EXPECT_TRUE(meta.init(server.server(), json->slice(), true, errorField, vocbase.name()));
    EXPECT_EQ(1, meta._analyzers.size());
    EXPECT_EQ(std::string("testVocbase::empty"), meta._analyzers[0]._pool->name());
    EXPECT_EQ(std::string("empty"), meta._analyzers[0]._pool->type());
    EXPECT_EQUAL_SLICES(
      VPackParser::fromJson("{\"args\" : \"de\"}")->slice(),
      meta._analyzers[0]._pool->properties());
    EXPECT_EQ(1, meta._analyzers[0]._pool->features().size());
    EXPECT_TRUE(meta._analyzers[0]._pool->features().check(irs::type<irs::frequency>::id()));
    EXPECT_EQ(std::string("empty"), meta._analyzers[0]._shortName);
  }

  // existing analyzer (name only) inRecovery
  {
    auto json = VPackParser::fromJson(
      "{ \
      \"analyzers\": [ \"empty\" ] \
    }");
    auto before = StorageEngineMock::recoveryStateResult;
    StorageEngineMock::recoveryStateResult = arangodb::RecoveryState::IN_PROGRESS;
    auto restore = irs::make_finally(
      [&before]() -> void { StorageEngineMock::recoveryStateResult = before; });
    arangodb::iresearch::IResearchLinkMeta meta;
    std::string errorField;
    EXPECT_TRUE(meta.init(server.server(), json->slice(), true, errorField, vocbase.name()));
    EXPECT_EQ(1, meta._analyzers.size());
    EXPECT_EQ(std::string("testVocbase::empty"), meta._analyzers[0]._pool->name());
    EXPECT_EQ(std::string("empty"), meta._analyzers[0]._pool->type());
    EXPECT_EQUAL_SLICES(
      VPackParser::fromJson("{\"args\" : \"de\"}")->slice(),
      meta._analyzers[0]._pool->properties());
    EXPECT_EQ(1, meta._analyzers[0]._pool->features().size());
    EXPECT_TRUE(meta._analyzers[0]._pool->features().check(irs::type<irs::frequency>::id()));
    EXPECT_EQ(std::string("empty"), meta._analyzers[0]._shortName);
  }

  // complex definition inRecovery
  {
    auto before = StorageEngineMock::recoveryStateResult;
    StorageEngineMock::recoveryStateResult = arangodb::RecoveryState::IN_PROGRESS;
    auto restore = irs::make_finally(
      [&before]() -> void { StorageEngineMock::recoveryStateResult = before; });

    auto json = VPackParser::fromJson(
      "{ \
      \"analyzerDefinitions\": [ \
         { \"name\": \"empty\", \"type\": \"empty\", \"properties\": {\"args\":\"ru\"}, \"features\": [ \"frequency\" ] }, \
         { \"name\": \"::empty\", \"type\": \"empty\", \"properties\": {\"args\":\"ru\"}, \"features\": [ \"frequency\" ] } \
      ], \
      \"fields\" : { \"field\": { \"analyzers\": [ \"testVocbase::empty\", \"empty\", \"_system::empty\", \"::empty\" ] } } \
    }");
    arangodb::iresearch::IResearchLinkMeta meta;
    std::string errorField;
    EXPECT_TRUE(meta.init(server.server(), json->slice(), true, errorField, vocbase.name()));
    EXPECT_EQ(2, meta._analyzerDefinitions.size());
    EXPECT_EQ(1, meta._analyzers.size());
    {
      auto& analyzer = meta._analyzers[0];
      EXPECT_EQ(arangodb::iresearch::IResearchAnalyzerFeature::identity(), analyzer._pool);
    }

    ASSERT_EQ(1, meta._fields.size());
    ASSERT_EQ(2, meta._fields.begin().value()->_analyzers.size());
    {
      auto& analyzer = meta._fields.begin().value()->_analyzers[0];
      EXPECT_EQ(std::string("testVocbase::empty"), analyzer._pool->name());
      EXPECT_EQ(std::string("empty"), analyzer._pool->type());
      EXPECT_EQUAL_SLICES(VPackParser::fromJson("{\"args\" : \"ru\"}")->slice(),
        analyzer._pool->properties());
      EXPECT_EQ(1, analyzer._pool->features().size());
      EXPECT_TRUE(analyzer._pool->features().check(irs::type<irs::frequency>::id()));
      EXPECT_EQ("empty", analyzer._shortName);
      EXPECT_EQ((*meta._analyzerDefinitions.find(analyzer._pool->name())), analyzer._pool);
    }
    {
      auto& analyzer = meta._fields.begin().value()->_analyzers[1];
      EXPECT_EQ(std::string("_system::empty"), analyzer._pool->name());
      EXPECT_EQ(std::string("empty"), analyzer._pool->type());
      EXPECT_EQUAL_SLICES(VPackParser::fromJson("{\"args\" : \"ru\"}")->slice(),
        analyzer._pool->properties());
      EXPECT_EQ(1, analyzer._pool->features().size());
      EXPECT_TRUE(analyzer._pool->features().check(irs::type<irs::frequency>::id()));
      EXPECT_EQ("::empty", analyzer._shortName);
      EXPECT_EQ((*meta._analyzerDefinitions.find(analyzer._pool->name())), analyzer._pool);
    }
  }

  // complex definition inRecovery
  {
    auto before = StorageEngineMock::recoveryStateResult;
    StorageEngineMock::recoveryStateResult = arangodb::RecoveryState::IN_PROGRESS;
    auto restore = irs::make_finally(
      [&before]() -> void { StorageEngineMock::recoveryStateResult = before; });

    auto json = VPackParser::fromJson(
      "{ \
      \"analyzerDefinitions\": [ \
         { \"name\": \"::empty\", \"type\": \"empty\", \"properties\": {\"args\":\"ru\"}, \"features\": [ \"frequency\" ] } \
      ], \
      \"fields\" : { \"field\": { \"analyzers\": [ \"testVocbase::empty\", \"empty\", \"_system::empty\", \"::empty\" ] } } \
    }");
    arangodb::iresearch::IResearchLinkMeta meta;
    std::string errorField;
    EXPECT_TRUE(meta.init(server.server(), json->slice(), true, errorField, vocbase.name()));
    EXPECT_EQ(2, meta._analyzerDefinitions.size());
    EXPECT_EQ(1, meta._analyzers.size());
    {
      auto& analyzer = meta._analyzers[0];
      EXPECT_EQ(arangodb::iresearch::IResearchAnalyzerFeature::identity(), analyzer._pool);
    }

    ASSERT_EQ(1, meta._fields.size());
    ASSERT_EQ(2, meta._fields.begin().value()->_analyzers.size());
    {
      auto& analyzer = meta._fields.begin().value()->_analyzers[0];
      EXPECT_EQ("testVocbase::empty", analyzer._pool->name());
      EXPECT_EQ("empty", analyzer._pool->type());
      // definition from cache since it's not presented "analyzerDefinitions"
      EXPECT_EQUAL_SLICES(VPackParser::fromJson("{\"args\" : \"de\"}")->slice(),
        analyzer._pool->properties());
      EXPECT_EQ(1, analyzer._pool->features().size());
      EXPECT_TRUE(analyzer._pool->features().check(irs::type<irs::frequency>::id()));
      EXPECT_EQ("empty", analyzer._shortName);
      EXPECT_EQ((*meta._analyzerDefinitions.find(analyzer._pool->name())), analyzer._pool);
    }
    {
      auto& analyzer = meta._fields.begin().value()->_analyzers[1];
      EXPECT_EQ("_system::empty", analyzer._pool->name());
      EXPECT_EQ("empty", analyzer._pool->type());
      EXPECT_EQUAL_SLICES(VPackParser::fromJson("{\"args\" : \"ru\"}")->slice(),
        analyzer._pool->properties());
      EXPECT_EQ(1, analyzer._pool->features().size());
      EXPECT_TRUE(analyzer._pool->features().check(irs::type<irs::frequency>::id()));
      EXPECT_EQ("::empty", analyzer._shortName);
      EXPECT_EQ((*meta._analyzerDefinitions.find(analyzer._pool->name())), analyzer._pool);
    }
  }

  // existing analyzer (full) analyzer creation not allowed (pass)
  {
    auto json = VPackParser::fromJson(
      "{ \
      \"analyzerDefinitions\": [ { \"name\": \"empty\", \"type\": \"empty\", \"properties\": {\"args\":\"de\"}, \"features\": [ \"frequency\" ] } ], \
      \"analyzers\": [ \"empty\" ] \
    }");
    arangodb::iresearch::IResearchLinkMeta meta;
    std::string errorField;
    EXPECT_TRUE(meta.init(server.server(), json->slice(), true, errorField, vocbase.name()));
    EXPECT_EQ(1, meta._analyzers.size());
    EXPECT_EQ(std::string("testVocbase::empty"), meta._analyzers[0]._pool->name());
    EXPECT_EQ(std::string("empty"), meta._analyzers[0]._pool->type());
    EXPECT_EQUAL_SLICES(
      VPackParser::fromJson("{\"args\" : \"de\"}")->slice(),
      meta._analyzers[0]._pool->properties());
    EXPECT_EQ(1, meta._analyzers[0]._pool->features().size());
    EXPECT_TRUE(meta._analyzers[0]._pool->features().check(irs::type<irs::frequency>::id()));
    EXPECT_EQ(std::string("empty"), meta._analyzers[0]._shortName);
  }

  // existing analyzer (full) analyzer definition not allowed
  {
    auto json = VPackParser::fromJson(
      "{ \
      \"analyzers\": [ { \"name\": \"empty\", \"type\": \"empty\", \"properties\": {\"args\":\"de\"}, \"features\": [ \"frequency\" ] } ], \
      \"analyzers\": [ \"empty\" ] \
    }");
    arangodb::iresearch::IResearchLinkMeta meta;
    std::string errorField;
    EXPECT_FALSE(meta.init(server.server(), json->slice(), false, errorField, vocbase.name()));
    EXPECT_EQ(std::string("analyzers[0]"), errorField);
  }

  // existing analyzer (full)
  {
    auto json = VPackParser::fromJson(
      "{ \
      \"analyzerDefinitions\": [ { \"name\": \"empty\", \"type\": \"empty\", \"properties\": {\"args\":\"de\"}, \"features\": [ \"frequency\" ] } ], \
      \"analyzers\": [ \"empty\" ] \
    }");
    arangodb::iresearch::IResearchLinkMeta meta;
    std::string errorField;
    EXPECT_TRUE(meta.init(server.server(), json->slice(), true, errorField, vocbase.name()));
    EXPECT_EQ(1, meta._analyzers.size());
    EXPECT_EQ(std::string("testVocbase::empty"), meta._analyzers[0]._pool->name());
    EXPECT_EQ(std::string("empty"), meta._analyzers[0]._pool->type());
    EXPECT_EQUAL_SLICES(
      VPackParser::fromJson("{\"args\" : \"de\"}")->slice(),
      meta._analyzers[0]._pool->properties());
    EXPECT_EQ(1, meta._analyzers[0]._pool->features().size());
    EXPECT_TRUE(meta._analyzers[0]._pool->features().check(irs::type<irs::frequency>::id()));
    EXPECT_EQ(std::string("empty"), meta._analyzers[0]._shortName);
  }

  // existing analyzer (full) inRecovery
  {
    auto json = VPackParser::fromJson(
      "{ \
      \"analyzerDefinitions\": [ { \"name\": \"empty\", \"type\": \"empty\", \"properties\": {\"args\":\"de\"}, \"features\": [ \"frequency\" ] } ], \
      \"analyzers\": [ \"empty\" ] \
    }");
    auto before = StorageEngineMock::recoveryStateResult;
    StorageEngineMock::recoveryStateResult = arangodb::RecoveryState::IN_PROGRESS;
    auto restore = irs::make_finally(
      [&before]() -> void { StorageEngineMock::recoveryStateResult = before; });
    arangodb::iresearch::IResearchLinkMeta meta;
    std::string errorField;
    EXPECT_TRUE(meta.init(server.server(), json->slice(), true, errorField, vocbase.name()));
    EXPECT_EQ(1, meta._analyzers.size());
    EXPECT_EQ("testVocbase::empty", meta._analyzers[0]._pool->name());
    EXPECT_EQ("empty", meta._analyzers[0]._pool->type());
    EXPECT_EQUAL_SLICES(
      VPackParser::fromJson("{\"args\" : \"de\"}")->slice(),
      meta._analyzers[0]._pool->properties());
    EXPECT_EQ(1, meta._analyzers[0]._pool->features().size());
    EXPECT_TRUE(meta._analyzers[0]._pool->features().check(irs::type<irs::frequency>::id()));
    EXPECT_EQ("empty", meta._analyzers[0]._shortName);
  }

  // existing analyzer (definition mismatch)
  {
    auto json = VPackParser::fromJson(
      "{ \
      \"analyzerDefinitions\": [ { \"name\": \"empty\", \"type\": \"empty\", \"properties\": {\"args\":\"ru\"}, \"features\": [ \"frequency\" ] } ], \
      \"analyzers\": [ \"empty\" ] \
    }");
    arangodb::iresearch::IResearchLinkMeta meta;
    std::string errorField;
    EXPECT_TRUE(meta.init(server.server(), json->slice(), true, errorField, vocbase.name()));
    EXPECT_EQ(1, meta._analyzers.size());
    EXPECT_EQ("testVocbase::empty", meta._analyzers[0]._pool->name());
    EXPECT_EQ("empty", meta._analyzers[0]._pool->type());
    EXPECT_EQUAL_SLICES(
      VPackParser::fromJson("{\"args\" : \"ru\"}")->slice(),
      meta._analyzers[0]._pool->properties());
    EXPECT_EQ(1, meta._analyzers[0]._pool->features().size());
    EXPECT_TRUE(meta._analyzers[0]._pool->features().check(irs::type<irs::frequency>::id()));
    EXPECT_EQ("empty", meta._analyzers[0]._shortName);
  }

  // existing analyzer (definition mismatch), don't read definitions
  {
    auto json = VPackParser::fromJson(
      "{ \
      \"analyzerDefinitions\": [ { \"name\": \"empty\", \"type\": \"empty\", \"properties\": {\"args\":\"ru\"}, \"features\": [ \"frequency\" ] } ], \
      \"analyzers\": [ \"empty\" ] \
    }");
    arangodb::iresearch::IResearchLinkMeta meta;
    std::string errorField;
    EXPECT_TRUE(meta.init(server.server(), json->slice(), false, errorField, vocbase.name()));
    EXPECT_EQ(1, meta._analyzers.size());
    EXPECT_EQ("testVocbase::empty", meta._analyzers[0]._pool->name());
    EXPECT_EQ("empty", meta._analyzers[0]._pool->type());
    // read definition from cache since we ignore "analyzerDefinitions"
    EXPECT_EQUAL_SLICES(
      VPackParser::fromJson("{\"args\" : \"de\"}")->slice(),
      meta._analyzers[0]._pool->properties());
    EXPECT_EQ(1, meta._analyzers[0]._pool->features().size());
    EXPECT_TRUE(meta._analyzers[0]._pool->features().check(irs::type<irs::frequency>::id()));
    EXPECT_EQ("empty", meta._analyzers[0]._shortName);
  }

  // existing analyzer (definition mismatch) inRecovery
  {
    auto json = VPackParser::fromJson(
      "{ \
      \"analyzerDefinitions\": [ { \"name\": \"empty\", \"type\": \"empty\", \"properties\": {\"args\":\"ru\"}, \"features\": [ \"frequency\" ] } ], \
      \"analyzers\": [ \"empty\" ] \
    }");
    auto before = StorageEngineMock::recoveryStateResult;
    StorageEngineMock::recoveryStateResult = arangodb::RecoveryState::IN_PROGRESS;
    auto restore = irs::make_finally(
      [&before]() -> void { StorageEngineMock::recoveryStateResult = before; });
    arangodb::iresearch::IResearchLinkMeta meta;
    std::string errorField;
    EXPECT_TRUE(meta.init(server.server(), json->slice(), true, errorField, vocbase.name()));
    EXPECT_EQ(1, meta._analyzers.size());
    EXPECT_EQ("testVocbase::empty", meta._analyzers[0]._pool->name());
    EXPECT_EQ("empty", meta._analyzers[0]._pool->type());
    // read definition from cache since we ignore "analyzerDefinitions"
    EXPECT_EQUAL_SLICES(
      VPackParser::fromJson("{\"args\" : \"ru\"}")->slice(),
      meta._analyzers[0]._pool->properties());
    EXPECT_EQ(1, meta._analyzers[0]._pool->features().size());
    EXPECT_TRUE(meta._analyzers[0]._pool->features().check(irs::type<irs::frequency>::id()));
    EXPECT_EQ("empty", meta._analyzers[0]._shortName);
  }

  // existing analyzer (definition mismatch) inRecovery, don't read definitions
  {
    auto json = VPackParser::fromJson(
      "{ \
      \"analyzerDefinitions\": [ { \"name\": \"empty\", \"type\": \"empty\", \"properties\": {\"args\":\"ru\"}, \"features\": [ \"frequency\" ] } ], \
      \"analyzers\": [ \"empty\" ] \
    }");
    auto before = StorageEngineMock::recoveryStateResult;
    StorageEngineMock::recoveryStateResult = arangodb::RecoveryState::IN_PROGRESS;
    auto restore = irs::make_finally(
      [&before]() -> void { StorageEngineMock::recoveryStateResult = before; });
    arangodb::iresearch::IResearchLinkMeta meta;
    std::string errorField;
    EXPECT_TRUE(meta.init(server.server(), json->slice(), false, errorField, vocbase.name()));
    EXPECT_EQ(1, meta._analyzers.size());
    EXPECT_EQ("testVocbase::empty", meta._analyzers[0]._pool->name());
    EXPECT_EQ("empty", meta._analyzers[0]._pool->type());
    // read definition from cache since we ignore "analyzerDefinitions"
    EXPECT_EQUAL_SLICES(
      VPackParser::fromJson("{\"args\" : \"de\"}")->slice(),
      meta._analyzers[0]._pool->properties());
    EXPECT_EQ(1, meta._analyzers[0]._pool->features().size());
    EXPECT_TRUE(meta._analyzers[0]._pool->features().check(irs::type<irs::frequency>::id()));
    EXPECT_EQ("empty", meta._analyzers[0]._shortName);
  }
}

TEST_F(IResearchLinkMetaTest, test_collectioNameComparison) {
  arangodb::iresearch::IResearchLinkMeta meta;
  arangodb::iresearch::IResearchLinkMeta meta1;
  // while upgrading in cluster missing collectionName
  // should not trigger link recreation!
  meta._collectionName = "";
  meta1._collectionName = "2";
  ASSERT_EQ(meta, meta1);
  ASSERT_FALSE(meta != meta1);
}

