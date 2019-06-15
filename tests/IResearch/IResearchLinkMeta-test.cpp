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

#include "common.h"
#include "gtest/gtest.h"

#include "../Mocks/StorageEngineMock.h"

#include "analysis/analyzers.hpp"
#include "analysis/token_attributes.hpp"
#include "utils/locale_utils.hpp"

#include "ApplicationFeatures/BasicPhase.h"
#include "ApplicationFeatures/ClusterPhase.h"
#include "ApplicationFeatures/CommunicationPhase.h"
#include "ApplicationFeatures/DatabasePhase.h"
#include "ApplicationFeatures/GreetingsPhase.h"
#include "ApplicationFeatures/V8Phase.h"
#include "Aql/AqlFunctionFeature.h"
#include "Cluster/ClusterFeature.h"

#if USE_ENTERPRISE
#include "Enterprise/Ldap/LdapFeature.h"
#endif

#include "GeneralServer/AuthenticationFeature.h"
#include "IResearch/IResearchCommon.h"
#include "IResearch/IResearchFeature.h"
#include "IResearch/IResearchLinkMeta.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "RestServer/SystemDatabaseFeature.h"
#include "Sharding/ShardingFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "V8Server/V8DealerFeature.h"
#include "velocypack/Builder.h"
#include "velocypack/Iterator.h"
#include "velocypack/Parser.h"

namespace {

struct TestAttributeZ : public irs::attribute {
  DECLARE_ATTRIBUTE_TYPE();
};

DEFINE_ATTRIBUTE_TYPE(TestAttributeZ);
REGISTER_ATTRIBUTE(TestAttributeZ);

class EmptyAnalyzer : public irs::analysis::analyzer {
 public:
  DECLARE_ANALYZER_TYPE();

  static ptr make(irs::string_ref const&) {
    PTR_NAMED(EmptyAnalyzer, ptr);
    return ptr;
  }

  static bool normalize(
      irs::string_ref const& args,
      std::string& out) {
    out = args;
    return true;
  }

  EmptyAnalyzer() : irs::analysis::analyzer(EmptyAnalyzer::type()) {
    _attrs.emplace(_attr);
  }
  virtual irs::attribute_view const& attributes() const NOEXCEPT override {
    return _attrs;
  }
  virtual bool next() override { return false; }
  virtual bool reset(irs::string_ref const& data) override { return true; }

 private:
  irs::attribute_view _attrs;
  TestAttributeZ _attr;
};

DEFINE_ANALYZER_TYPE_NAMED(EmptyAnalyzer, "empty");
REGISTER_ANALYZER_JSON(EmptyAnalyzer, EmptyAnalyzer::make, EmptyAnalyzer::normalize);

}  // namespace

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

class IResearchLinkMetaTest : public ::testing::Test {
 protected:
  StorageEngineMock engine;
  arangodb::application_features::ApplicationServer server;
  std::vector<std::pair<arangodb::application_features::ApplicationFeature*, bool>> features;
  std::vector<arangodb::application_features::ApplicationFeature*> orderedFeatures;

  IResearchLinkMetaTest() : engine(server), server(nullptr, nullptr) {
    arangodb::EngineSelectorFeature::ENGINE = &engine;

    arangodb::tests::init();

    // suppress INFO {authentication} Authentication is turned on (system only), authentication for unix sockets is turned on
    // suppress WARNING {authentication} --server.jwt-secret is insecure. Use --server.jwt-secret-keyfile instead
    arangodb::LogTopic::setLogLevel(arangodb::Logger::AUTHENTICATION.name(),
                                    arangodb::LogLevel::ERR);

    // setup required application features
    features.emplace_back(new arangodb::AuthenticationFeature(server), true);
    features.emplace_back(new arangodb::DatabaseFeature(server), false);
    features.emplace_back(new arangodb::ShardingFeature(server), false);
    features.emplace_back(new arangodb::QueryRegistryFeature(server), false);
    features.emplace_back(new arangodb::SystemDatabaseFeature(server), true);  // required for IResearchAnalyzerFeature
    features.emplace_back(new arangodb::V8DealerFeature(server),
                          false);  // required for DatabaseFeature::createDatabase(...)
    features.emplace_back(new arangodb::aql::AqlFunctionFeature(server), true);  // required for IResearchAnalyzerFeature
    features.emplace_back(new arangodb::iresearch::IResearchAnalyzerFeature(server), true);

#if USE_ENTERPRISE
    features.emplace_back(new arangodb::LdapFeature(server),
                          false);  // required for AuthenticationFeature with USE_ENTERPRISE
#endif

    // required for V8DealerFeature::prepare(), ClusterFeature::prepare() not required
    arangodb::application_features::ApplicationServer::server->addFeature(
        new arangodb::ClusterFeature(server));

    for (auto& f : features) {
      arangodb::application_features::ApplicationServer::server->addFeature(f.first);
    }

    for (auto& f : features) {
      f.first->prepare();
    }

    auto const databases = arangodb::velocypack::Parser::fromJson(
        std::string("[ { \"name\": \"") +
        arangodb::StaticStrings::SystemDatabase + "\" } ]");
    auto* dbFeature =
        arangodb::application_features::ApplicationServer::lookupFeature<arangodb::DatabaseFeature>(
            "Database");
    dbFeature->loadDatabases(databases->slice());

    for (auto& f : features) {
      if (f.second) {
        f.first->start();
      }
    }

    TRI_vocbase_t* vocbase;
    dbFeature->createDatabase(1, "testVocbase", vocbase);  // required for IResearchAnalyzerFeature::emplace(...)

    auto* analyzers =
        arangodb::application_features::ApplicationServer::lookupFeature<arangodb::iresearch::IResearchAnalyzerFeature>();
    arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;

    analyzers->emplace(result, "testVocbase::empty",
                       "empty", "de", irs::flags{irs::frequency::type()});  // cache the 'empty' analyzer for 'testVocbase'

    // suppress log messages since tests check error conditions
    arangodb::LogTopic::setLogLevel(arangodb::Logger::AGENCYCOMM.name(),
                                    arangodb::LogLevel::FATAL);
    arangodb::LogTopic::setLogLevel(arangodb::iresearch::TOPIC.name(),
                                    arangodb::LogLevel::FATAL);
    irs::logger::output_le(iresearch::logger::IRL_FATAL, stderr);
  }

  ~IResearchLinkMetaTest() {
    arangodb::LogTopic::setLogLevel(arangodb::iresearch::TOPIC.name(),
                                    arangodb::LogLevel::DEFAULT);
    arangodb::LogTopic::setLogLevel(arangodb::Logger::AGENCYCOMM.name(),
                                    arangodb::LogLevel::DEFAULT);
    arangodb::application_features::ApplicationServer::server = nullptr;

    // destroy application features
    for (auto& f : features) {
      if (f.second) {
        f.first->stop();
      }
    }

    for (auto& f : features) {
      f.first->unprepare();
    }

    arangodb::LogTopic::setLogLevel(arangodb::Logger::AUTHENTICATION.name(),
                                    arangodb::LogLevel::DEFAULT);
    arangodb::EngineSelectorFeature::ENGINE = nullptr;
  }
};

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

TEST_F(IResearchLinkMetaTest, test_defaults) {
  arangodb::iresearch::IResearchLinkMeta meta;

  EXPECT_TRUE(true == meta._fields.empty());
  EXPECT_TRUE(false == meta._includeAllFields);
  EXPECT_TRUE(false == meta._trackListPositions);
  EXPECT_TRUE((arangodb::iresearch::ValueStorage::NONE == meta._storeValues));
  EXPECT_TRUE(1U == meta._analyzers.size());
  EXPECT_TRUE((*(meta._analyzers.begin())));
  EXPECT_TRUE(("identity" == meta._analyzers.begin()->_pool->name()));
  EXPECT_TRUE(("identity" == meta._analyzers.begin()->_shortName));
  EXPECT_TRUE((irs::flags({irs::norm::type(), irs::frequency::type()}) ==
               meta._analyzers.begin()->_pool->features()));
  EXPECT_TRUE(false == !meta._analyzers.begin()->_pool->get());
}

TEST_F(IResearchLinkMetaTest, test_inheritDefaults) {
  arangodb::iresearch::IResearchAnalyzerFeature analyzers(server);
  arangodb::iresearch::IResearchLinkMeta defaults;
  arangodb::iresearch::IResearchLinkMeta meta;
  std::unordered_set<std::string> expectedFields = {"abc"};
  std::unordered_set<std::string> expectedOverrides = {"xyz"};
  std::string tmpString;

  analyzers.start();

  defaults._fields["abc"] = arangodb::iresearch::IResearchLinkMeta();
  defaults._includeAllFields = true;
  defaults._trackListPositions = true;
  defaults._storeValues = arangodb::iresearch::ValueStorage::FULL;
  defaults._analyzers.clear();
  defaults._analyzers.emplace_back(arangodb::iresearch::IResearchLinkMeta::Analyzer(
      analyzers.get("testVocbase::empty"), "empty"));
  defaults._fields["abc"]->_fields["xyz"] = arangodb::iresearch::IResearchLinkMeta();

  auto json = arangodb::velocypack::Parser::fromJson("{}");
  EXPECT_TRUE(true == meta.init(json->slice(), false, tmpString, nullptr, defaults));
  EXPECT_TRUE(1U == meta._fields.size());

  for (auto& field : meta._fields) {
    EXPECT_TRUE(1U == expectedFields.erase(field.key()));
    EXPECT_TRUE(1U == field.value()->_fields.size());

    for (auto& fieldOverride : field.value()->_fields) {
      auto& actual = *(fieldOverride.value());
      EXPECT_TRUE(1U == expectedOverrides.erase(fieldOverride.key()));

      if ("xyz" == fieldOverride.key()) {
        EXPECT_TRUE(true == actual._fields.empty());
        EXPECT_TRUE(false == actual._includeAllFields);
        EXPECT_TRUE(false == actual._trackListPositions);
        EXPECT_TRUE((arangodb::iresearch::ValueStorage::NONE == actual._storeValues));
        EXPECT_TRUE(1U == actual._analyzers.size());
        EXPECT_TRUE((*(actual._analyzers.begin())));
        EXPECT_TRUE(("identity" == actual._analyzers.begin()->_pool->name()));
        EXPECT_TRUE(("identity" == actual._analyzers.begin()->_shortName));
        EXPECT_TRUE((irs::flags({irs::norm::type(), irs::frequency::type()}) ==
                     actual._analyzers.begin()->_pool->features()));
        EXPECT_TRUE(false == !actual._analyzers.begin()->_pool->get());
      }
    }
  }

  EXPECT_TRUE(true == expectedOverrides.empty());
  EXPECT_TRUE(true == expectedFields.empty());
  EXPECT_TRUE(true == meta._includeAllFields);
  EXPECT_TRUE(true == meta._trackListPositions);
  EXPECT_TRUE((arangodb::iresearch::ValueStorage::FULL == meta._storeValues));

  EXPECT_TRUE(1U == meta._analyzers.size());
  EXPECT_TRUE((*(meta._analyzers.begin())));
  EXPECT_TRUE(("testVocbase::empty" == meta._analyzers.begin()->_pool->name()));
  EXPECT_TRUE(("empty" == meta._analyzers.begin()->_shortName));
  EXPECT_TRUE((irs::flags({irs::frequency::type()}) ==
               meta._analyzers.begin()->_pool->features()));
  EXPECT_TRUE(false == !meta._analyzers.begin()->_pool->get());
}

TEST_F(IResearchLinkMetaTest, test_readDefaults) {
  auto json = arangodb::velocypack::Parser::fromJson("{}");

  // without active vobcase
  {
    arangodb::iresearch::IResearchLinkMeta meta;
    std::string tmpString;
    EXPECT_TRUE((true == meta.init(json->slice(), false, tmpString)));
    EXPECT_TRUE((true == meta._fields.empty()));
    EXPECT_TRUE((false == meta._includeAllFields));
    EXPECT_TRUE((false == meta._trackListPositions));
    EXPECT_TRUE((arangodb::iresearch::ValueStorage::NONE == meta._storeValues));
    EXPECT_TRUE((1U == meta._analyzers.size()));
    EXPECT_TRUE((*(meta._analyzers.begin())));
    EXPECT_TRUE(("identity" == meta._analyzers.begin()->_pool->name()));
    EXPECT_TRUE(("identity" == meta._analyzers.begin()->_shortName));
    EXPECT_TRUE((irs::flags({irs::norm::type(), irs::frequency::type()}) ==
                 meta._analyzers.begin()->_pool->features()));
    EXPECT_TRUE((false == !meta._analyzers.begin()->_pool->get()));
  }

  // with active vocbase
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                          "testVocbase");
    arangodb::iresearch::IResearchLinkMeta meta;
    std::string tmpString;
    EXPECT_TRUE((true == meta.init(json->slice(), false, tmpString, &vocbase)));
    EXPECT_TRUE((true == meta._fields.empty()));
    EXPECT_TRUE((false == meta._includeAllFields));
    EXPECT_TRUE((false == meta._trackListPositions));
    EXPECT_TRUE((arangodb::iresearch::ValueStorage::NONE == meta._storeValues));
    EXPECT_TRUE((1U == meta._analyzers.size()));
    EXPECT_TRUE((*(meta._analyzers.begin())));
    EXPECT_TRUE(("identity" == meta._analyzers.begin()->_pool->name()));
    EXPECT_TRUE(("identity" == meta._analyzers.begin()->_shortName));
    EXPECT_TRUE((irs::flags({irs::norm::type(), irs::frequency::type()}) ==
                 meta._analyzers.begin()->_pool->features()));
    EXPECT_TRUE((false == !meta._analyzers.begin()->_pool->get()));
  }
}

TEST_F(IResearchLinkMetaTest, test_readCustomizedValues) {
  auto json = arangodb::velocypack::Parser::fromJson(
      "{ \
    \"fields\": { \
      \"a\": {}, \
      \"b\": {}, \
      \"c\": { \
        \"fields\": { \
          \"default\": { \"fields\": {}, \"includeAllFields\": false, \"trackListPositions\": false, \"storeValues\": \"none\", \"analyzers\": [ \"identity\" ] }, \
          \"all\": { \"fields\": {\"d\": {}, \"e\": {}}, \"includeAllFields\": true, \"trackListPositions\": true, \"storeValues\": \"full\", \"analyzers\": [ \"empty\" ] }, \
          \"some\": { \"trackListPositions\": true, \"storeValues\": \"id\" }, \
          \"none\": {} \
        } \
      } \
    }, \
    \"includeAllFields\": true, \
    \"trackListPositions\": true, \
    \"storeValues\": \"full\", \
    \"analyzers\": [ \"empty\", \"identity\" ] \
  }");

  // without active vocbase
  {
    arangodb::iresearch::IResearchLinkMeta meta;
    std::string tmpString;
    EXPECT_TRUE(false == meta.init(json->slice(), false, tmpString));
  }

  // with active vocbase
  {
    std::unordered_set<std::string> expectedFields = {"a", "b", "c"};
    std::unordered_set<std::string> expectedOverrides = {"default", "all",
                                                         "some", "none"};
    std::unordered_set<std::string> expectedAnalyzers = {"empty", "identity"};
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                          "testVocbase");
    arangodb::iresearch::IResearchLinkMeta meta;
    std::string tmpString;
    EXPECT_TRUE((true == meta.init(json->slice(), false, tmpString, &vocbase)));
    EXPECT_TRUE((3U == meta._fields.size()));

    for (auto& field : meta._fields) {
      EXPECT_TRUE((1U == expectedFields.erase(field.key())));

      for (auto& fieldOverride : field.value()->_fields) {
        auto& actual = *(fieldOverride.value());

        EXPECT_TRUE((1U == expectedOverrides.erase(fieldOverride.key())));

        if ("default" == fieldOverride.key()) {
          EXPECT_TRUE((true == actual._fields.empty()));
          EXPECT_TRUE((false == actual._includeAllFields));
          EXPECT_TRUE((false == actual._trackListPositions));
          EXPECT_TRUE((arangodb::iresearch::ValueStorage::NONE == actual._storeValues));
          EXPECT_TRUE((1U == actual._analyzers.size()));
          EXPECT_TRUE((*(actual._analyzers.begin())));
          EXPECT_TRUE(("identity" == actual._analyzers.begin()->_pool->name()));
          EXPECT_TRUE(("identity" == actual._analyzers.begin()->_shortName));
          EXPECT_TRUE((irs::flags({irs::norm::type(), irs::frequency::type()}) ==
                       actual._analyzers.begin()->_pool->features()));
          EXPECT_TRUE((false == !actual._analyzers.begin()->_pool->get()));
        } else if ("all" == fieldOverride.key()) {
          EXPECT_TRUE((2U == actual._fields.size()));
          EXPECT_TRUE((true == (actual._fields.find("d") != actual._fields.end())));
          EXPECT_TRUE((true == (actual._fields.find("e") != actual._fields.end())));
          EXPECT_TRUE((true == actual._includeAllFields));
          EXPECT_TRUE((true == actual._trackListPositions));
          EXPECT_TRUE((arangodb::iresearch::ValueStorage::FULL == actual._storeValues));
          EXPECT_TRUE((1U == actual._analyzers.size()));
          EXPECT_TRUE((*(actual._analyzers.begin())));
          EXPECT_TRUE(("testVocbase::empty" == actual._analyzers.begin()->_pool->name()));
          EXPECT_TRUE(("empty" == actual._analyzers.begin()->_shortName));
          EXPECT_TRUE((irs::flags({irs::frequency::type()}) ==
                       actual._analyzers.begin()->_pool->features()));
          EXPECT_TRUE((false == !actual._analyzers.begin()->_pool->get()));
        } else if ("some" == fieldOverride.key()) {
          EXPECT_TRUE((true == actual._fields.empty()));    // not inherited
          EXPECT_TRUE((true == actual._includeAllFields));  // inherited
          EXPECT_TRUE((true == actual._trackListPositions));
          EXPECT_TRUE((arangodb::iresearch::ValueStorage::ID == actual._storeValues));
          EXPECT_TRUE((2U == actual._analyzers.size()));
          auto itr = actual._analyzers.begin();
          EXPECT_TRUE((*itr));
          EXPECT_TRUE(("testVocbase::empty" == itr->_pool->name()));
          EXPECT_TRUE(("empty" == itr->_shortName));
          EXPECT_TRUE((irs::flags({irs::frequency::type()}) == itr->_pool->features()));
          EXPECT_TRUE((false == !itr->_pool->get()));
          ++itr;
          EXPECT_TRUE((*itr));
          EXPECT_TRUE(("identity" == itr->_pool->name()));
          EXPECT_TRUE(("identity" == itr->_shortName));
          EXPECT_TRUE((irs::flags({irs::norm::type(), irs::frequency::type()}) ==
                       itr->_pool->features()));
          EXPECT_TRUE((false == !itr->_pool->get()));
        } else if ("none" == fieldOverride.key()) {
          EXPECT_TRUE((true == actual._fields.empty()));      // not inherited
          EXPECT_TRUE((true == actual._includeAllFields));    // inherited
          EXPECT_TRUE((true == actual._trackListPositions));  // inherited
          EXPECT_TRUE((arangodb::iresearch::ValueStorage::FULL == actual._storeValues));
          auto itr = actual._analyzers.begin();
          EXPECT_TRUE((*itr));
          EXPECT_TRUE(("testVocbase::empty" == itr->_pool->name()));
          EXPECT_TRUE(("empty" == itr->_shortName));
          EXPECT_TRUE((irs::flags({irs::frequency::type()}) == itr->_pool->features()));
          EXPECT_TRUE((false == !itr->_pool->get()));
          ++itr;
          EXPECT_TRUE((*itr));
          EXPECT_TRUE(("identity" == itr->_pool->name()));
          EXPECT_TRUE(("identity" == itr->_shortName));
          EXPECT_TRUE((irs::flags({irs::norm::type(), irs::frequency::type()}) ==
                       itr->_pool->features()));
          EXPECT_TRUE((false == !itr->_pool->get()));
        }
      }
    }

    EXPECT_TRUE((true == expectedOverrides.empty()));
    EXPECT_TRUE((true == expectedFields.empty()));
    EXPECT_TRUE((true == meta._includeAllFields));
    EXPECT_TRUE((true == meta._trackListPositions));
    EXPECT_TRUE((arangodb::iresearch::ValueStorage::FULL == meta._storeValues));
    auto itr = meta._analyzers.begin();
    EXPECT_TRUE((*itr));
    EXPECT_TRUE(("testVocbase::empty" == itr->_pool->name()));
    EXPECT_TRUE(("empty" == itr->_shortName));
    EXPECT_TRUE((irs::flags({irs::frequency::type()}) == itr->_pool->features()));
    EXPECT_TRUE((false == !itr->_pool->get()));
    ++itr;
    EXPECT_TRUE((*itr));
    EXPECT_TRUE(("identity" == itr->_pool->name()));
    EXPECT_TRUE(("identity" == itr->_shortName));
    EXPECT_TRUE((irs::flags({irs::norm::type(), irs::frequency::type()}) ==
                 itr->_pool->features()));
    EXPECT_TRUE((false == !itr->_pool->get()));
  }
}

TEST_F(IResearchLinkMetaTest, test_writeDefaults) {
  // without active vobcase (not fullAnalyzerDefinition)
  {
    arangodb::iresearch::IResearchLinkMeta meta;
    arangodb::velocypack::Builder builder;
    arangodb::velocypack::Slice tmpSlice;

    builder.openObject();
    EXPECT_TRUE((true == meta.json(builder, false)));
    builder.close();

    auto slice = builder.slice();

    EXPECT_TRUE((5U == slice.length()));
    tmpSlice = slice.get("fields");
    EXPECT_TRUE((true == tmpSlice.isObject() && 0 == tmpSlice.length()));
    tmpSlice = slice.get("includeAllFields");
    EXPECT_TRUE((true == tmpSlice.isBool() && false == tmpSlice.getBool()));
    tmpSlice = slice.get("trackListPositions");
    EXPECT_TRUE((true == tmpSlice.isBool() && false == tmpSlice.getBool()));
    tmpSlice = slice.get("storeValues");
    EXPECT_TRUE((true == tmpSlice.isString() && std::string("none") == tmpSlice.copyString()));
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
    EXPECT_TRUE((true == meta.json(builder, true)));
    builder.close();

    auto slice = builder.slice();

    EXPECT_TRUE((7U == slice.length()));
    tmpSlice = slice.get("fields");
    EXPECT_TRUE((true == tmpSlice.isObject() && 0 == tmpSlice.length()));
    tmpSlice = slice.get("includeAllFields");
    EXPECT_TRUE((true == tmpSlice.isBool() && false == tmpSlice.getBool()));
    tmpSlice = slice.get("trackListPositions");
    EXPECT_TRUE((true == tmpSlice.isBool() && false == tmpSlice.getBool()));
    tmpSlice = slice.get("storeValues");
    EXPECT_TRUE((true == tmpSlice.isString() && std::string("none") == tmpSlice.copyString()));
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
                 tmpSlice.at(0).get("properties").isString() && tmpSlice.at(0).get("properties").copyString() == "{}" &&
                 tmpSlice.at(0).get("features").isArray() &&
                 2 == tmpSlice.at(0).get("features").length()  // frequency+norm
                 ));
    tmpSlice = slice.get("primarySort");
    EXPECT_TRUE((true == tmpSlice.isArray() && 0 == tmpSlice.length()));
  }

  // with active vocbase (not fullAnalyzerDefinition)
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                          "testVocbase");
    arangodb::iresearch::IResearchLinkMeta meta;
    arangodb::velocypack::Builder builder;
    arangodb::velocypack::Slice tmpSlice;

    builder.openObject();
    EXPECT_TRUE((true == meta.json(builder, false, nullptr, &vocbase)));
    builder.close();

    auto slice = builder.slice();

    EXPECT_TRUE((5U == slice.length()));
    tmpSlice = slice.get("fields");
    EXPECT_TRUE((true == tmpSlice.isObject() && 0 == tmpSlice.length()));
    tmpSlice = slice.get("includeAllFields");
    EXPECT_TRUE((true == tmpSlice.isBool() && false == tmpSlice.getBool()));
    tmpSlice = slice.get("trackListPositions");
    EXPECT_TRUE((true == tmpSlice.isBool() && false == tmpSlice.getBool()));
    tmpSlice = slice.get("storeValues");
    EXPECT_TRUE((true == tmpSlice.isString() && std::string("none") == tmpSlice.copyString()));
    tmpSlice = slice.get("analyzers");
    EXPECT_TRUE((true == tmpSlice.isArray() && 1 == tmpSlice.length() &&
                 tmpSlice.at(0).isString() &&
                 std::string("identity") == tmpSlice.at(0).copyString()));
  }

  // with active vocbase (with fullAnalyzerDefinition)
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                          "testVocbase");
    arangodb::iresearch::IResearchLinkMeta meta;
    arangodb::velocypack::Builder builder;
    arangodb::velocypack::Slice tmpSlice;

    builder.openObject();
    EXPECT_TRUE((true == meta.json(builder, true, nullptr, &vocbase)));
    builder.close();

    auto slice = builder.slice();

    EXPECT_TRUE((7U == slice.length()));
    tmpSlice = slice.get("fields");
    EXPECT_TRUE((true == tmpSlice.isObject() && 0 == tmpSlice.length()));
    tmpSlice = slice.get("includeAllFields");
    EXPECT_TRUE((true == tmpSlice.isBool() && false == tmpSlice.getBool()));
    tmpSlice = slice.get("trackListPositions");
    EXPECT_TRUE((true == tmpSlice.isBool() && false == tmpSlice.getBool()));
    tmpSlice = slice.get("storeValues");
    EXPECT_TRUE((true == tmpSlice.isString() && std::string("none") == tmpSlice.copyString()));
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
                 tmpSlice.at(0).get("properties").isString() && tmpSlice.at(0).get("properties").copyString() == "{}" &&
                 tmpSlice.at(0).get("features").isArray() &&
                 2 == tmpSlice.at(0).get("features").length()  // frequency+norm
                 ));
    tmpSlice = slice.get("primarySort");
    EXPECT_TRUE((true == tmpSlice.isArray() && 0 == tmpSlice.length()));
  }
}

TEST_F(IResearchLinkMetaTest, test_writeCustomizedValues) {
  arangodb::iresearch::IResearchAnalyzerFeature analyzers(server);
  analyzers.prepare();  // add static analyzers
  arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult emplaceResult;
  arangodb::iresearch::IResearchLinkMeta meta;

  analyzers.emplace(emplaceResult,
                    arangodb::StaticStrings::SystemDatabase + "::empty",
                    "empty", "en", {irs::frequency::type()});

  meta._includeAllFields = true;
  meta._trackListPositions = true;
  meta._storeValues = arangodb::iresearch::ValueStorage::FULL;
  meta._analyzers.clear();
  meta._analyzers.emplace_back(arangodb::iresearch::IResearchLinkMeta::Analyzer(
      analyzers.get("identity"), "identity"));
  meta._analyzers.emplace_back(arangodb::iresearch::IResearchLinkMeta::Analyzer(
      analyzers.get(arangodb::StaticStrings::SystemDatabase + "::empty"),
      "enmpty"));
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
      analyzers.get(arangodb::StaticStrings::SystemDatabase + "::empty"),
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
    EXPECT_TRUE((true == meta.json(builder, false)));
    builder.close();

    auto slice = builder.slice();

    EXPECT_TRUE((5U == slice.length()));
    tmpSlice = slice.get("fields");
    EXPECT_TRUE((true == tmpSlice.isObject() && 3 == tmpSlice.length()));

    for (arangodb::velocypack::ObjectIterator itr(tmpSlice); itr.valid(); ++itr) {
      auto key = itr.key();
      auto value = itr.value();
      EXPECT_TRUE((true == key.isString() && 1 == expectedFields.erase(key.copyString())));
      EXPECT_TRUE((true == value.isObject()));

      if (!value.hasKey("fields")) {
        continue;
      }

      tmpSlice = value.get("fields");

      for (arangodb::velocypack::ObjectIterator overrideItr(tmpSlice);
           overrideItr.valid(); ++overrideItr) {
        auto fieldOverride = overrideItr.key();
        auto sliceOverride = overrideItr.value();
        EXPECT_TRUE((true == fieldOverride.isString() && sliceOverride.isObject()));
        EXPECT_TRUE((1U == expectedOverrides.erase(fieldOverride.copyString())));

        if ("default" == fieldOverride.copyString()) {
          EXPECT_TRUE((4U == sliceOverride.length()));
          tmpSlice = sliceOverride.get("includeAllFields");
          EXPECT_TRUE((true == (false == tmpSlice.getBool())));
          tmpSlice = sliceOverride.get("trackListPositions");
          EXPECT_TRUE((true == (false == tmpSlice.getBool())));
          tmpSlice = sliceOverride.get("storeValues");
          EXPECT_TRUE((true == tmpSlice.isString() &&
                       std::string("none") == tmpSlice.copyString()));
          tmpSlice = sliceOverride.get("analyzers");
          EXPECT_TRUE((true == tmpSlice.isArray() && 1 == tmpSlice.length() &&
                       tmpSlice.at(0).isString() &&
                       std::string("identity") == tmpSlice.at(0).copyString()));
        } else if ("all" == fieldOverride.copyString()) {
          std::unordered_set<std::string> expectedFields = {"x", "y"};
          EXPECT_TRUE((5U == sliceOverride.length()));
          tmpSlice = sliceOverride.get("fields");
          EXPECT_TRUE((true == tmpSlice.isObject() && 2 == tmpSlice.length()));
          for (arangodb::velocypack::ObjectIterator overrideFieldItr(tmpSlice);
               overrideFieldItr.valid(); ++overrideFieldItr) {
            EXPECT_TRUE((true == overrideFieldItr.key().isString() &&
                         1 == expectedFields.erase(overrideFieldItr.key().copyString())));
          }
          EXPECT_TRUE((true == expectedFields.empty()));
          tmpSlice = sliceOverride.get("includeAllFields");
          EXPECT_TRUE((true == tmpSlice.isBool() && false == tmpSlice.getBool()));
          tmpSlice = sliceOverride.get("trackListPositions");
          EXPECT_TRUE((true == tmpSlice.isBool() && false == tmpSlice.getBool()));
          tmpSlice = sliceOverride.get("storeValues");
          EXPECT_TRUE((true == tmpSlice.isString() &&
                       std::string("none") == tmpSlice.copyString()));
          tmpSlice = sliceOverride.get("analyzers");
          EXPECT_TRUE((true == tmpSlice.isArray() && 1 == tmpSlice.length() &&
                       tmpSlice.at(0).isString() &&
                       std::string(arangodb::StaticStrings::SystemDatabase +
                                   "::empty") == tmpSlice.at(0).copyString()));
        } else if ("some" == fieldOverride.copyString()) {
          EXPECT_TRUE((2U == sliceOverride.length()));
          tmpSlice = sliceOverride.get("trackListPositions");
          EXPECT_TRUE((true == tmpSlice.isBool() && false == tmpSlice.getBool()));
          tmpSlice = sliceOverride.get("storeValues");
          EXPECT_TRUE((true == tmpSlice.isString() &&
                       std::string("id") == tmpSlice.copyString()));
        } else if ("none" == fieldOverride.copyString()) {
          EXPECT_TRUE((0U == sliceOverride.length()));
        }
      }
    }

    EXPECT_TRUE((true == expectedOverrides.empty()));
    EXPECT_TRUE((true == expectedFields.empty()));
    tmpSlice = slice.get("includeAllFields");
    EXPECT_TRUE((true == tmpSlice.isBool() && true == tmpSlice.getBool()));
    tmpSlice = slice.get("trackListPositions");
    EXPECT_TRUE((true == tmpSlice.isBool() && true == tmpSlice.getBool()));
    tmpSlice = slice.get("storeValues");
    EXPECT_TRUE((true == tmpSlice.isString() && std::string("full") == tmpSlice.copyString()));
    tmpSlice = slice.get("analyzers");
    EXPECT_TRUE((true == tmpSlice.isArray() && 2 == tmpSlice.length()));

    for (arangodb::velocypack::ArrayIterator analyzersItr(tmpSlice);
         analyzersItr.valid(); ++analyzersItr) {
      auto key = *analyzersItr;
      EXPECT_TRUE((true == key.isString() && 1 == expectedAnalyzers.erase(key.copyString())));
    }

    EXPECT_TRUE((true == expectedAnalyzers.empty()));
  }

  // without active vobcase (with fullAnalyzerDefinition)
  {
    std::unordered_set<std::string> expectedFields = {"a", "b", "c"};
    std::unordered_set<std::string> expectedOverrides = {"default", "all",
                                                         "some", "none"};
    std::unordered_set<std::string> expectedAnalyzers = {
        arangodb::StaticStrings::SystemDatabase + "::empty", "identity"};
    std::set<std::pair<std::string, std::string>> expectedAnalyzerDefinitions = {
        {arangodb::StaticStrings::SystemDatabase + "::empty", "en"},
        {"identity", "{}"},
    };
    arangodb::velocypack::Builder builder;
    arangodb::velocypack::Slice tmpSlice;

    builder.openObject();
    EXPECT_TRUE((true == meta.json(builder, true)));
    builder.close();

    auto slice = builder.slice();

    EXPECT_TRUE((7U == slice.length()));
    tmpSlice = slice.get("fields");
    EXPECT_TRUE((true == tmpSlice.isObject() && 3 == tmpSlice.length()));

    for (arangodb::velocypack::ObjectIterator itr(tmpSlice); itr.valid(); ++itr) {
      auto key = itr.key();
      auto value = itr.value();
      EXPECT_TRUE((true == key.isString() && 1 == expectedFields.erase(key.copyString())));
      EXPECT_TRUE((true == value.isObject()));

      if (!value.hasKey("fields")) {
        continue;
      }

      tmpSlice = value.get("fields");

      for (arangodb::velocypack::ObjectIterator overrideItr(tmpSlice);
           overrideItr.valid(); ++overrideItr) {
        auto fieldOverride = overrideItr.key();
        auto sliceOverride = overrideItr.value();
        EXPECT_TRUE((true == fieldOverride.isString() && sliceOverride.isObject()));
        EXPECT_TRUE((1U == expectedOverrides.erase(fieldOverride.copyString())));

        if ("default" == fieldOverride.copyString()) {
          EXPECT_TRUE((4U == sliceOverride.length()));
          tmpSlice = sliceOverride.get("includeAllFields");
          EXPECT_TRUE((true == (false == tmpSlice.getBool())));
          tmpSlice = sliceOverride.get("trackListPositions");
          EXPECT_TRUE((true == (false == tmpSlice.getBool())));
          tmpSlice = sliceOverride.get("storeValues");
          EXPECT_TRUE((true == tmpSlice.isString() &&
                       std::string("none") == tmpSlice.copyString()));
          tmpSlice = sliceOverride.get("analyzers");
          EXPECT_TRUE((true == tmpSlice.isArray() && 1 == tmpSlice.length() &&
                       tmpSlice.at(0).isString() &&
                       std::string("identity") == tmpSlice.at(0).copyString()));
        } else if ("all" == fieldOverride.copyString()) {
          std::unordered_set<std::string> expectedFields = {"x", "y"};
          EXPECT_TRUE((5U == sliceOverride.length()));
          tmpSlice = sliceOverride.get("fields");
          EXPECT_TRUE((true == tmpSlice.isObject() && 2 == tmpSlice.length()));
          for (arangodb::velocypack::ObjectIterator overrideFieldItr(tmpSlice);
               overrideFieldItr.valid(); ++overrideFieldItr) {
            EXPECT_TRUE((true == overrideFieldItr.key().isString() &&
                         1 == expectedFields.erase(overrideFieldItr.key().copyString())));
          }
          EXPECT_TRUE((true == expectedFields.empty()));
          tmpSlice = sliceOverride.get("includeAllFields");
          EXPECT_TRUE((true == tmpSlice.isBool() && false == tmpSlice.getBool()));
          tmpSlice = sliceOverride.get("trackListPositions");
          EXPECT_TRUE((true == tmpSlice.isBool() && false == tmpSlice.getBool()));
          tmpSlice = sliceOverride.get("storeValues");
          EXPECT_TRUE((true == tmpSlice.isString() &&
                       std::string("none") == tmpSlice.copyString()));
          tmpSlice = sliceOverride.get("analyzers");
          EXPECT_TRUE((true == tmpSlice.isArray() && 1 == tmpSlice.length() &&
                       tmpSlice.at(0).isString() &&
                       arangodb::StaticStrings::SystemDatabase + "::empty" ==
                           tmpSlice.at(0).copyString()));
        } else if ("some" == fieldOverride.copyString()) {
          EXPECT_TRUE((2U == sliceOverride.length()));
          tmpSlice = sliceOverride.get("trackListPositions");
          EXPECT_TRUE((true == tmpSlice.isBool() && false == tmpSlice.getBool()));
          tmpSlice = sliceOverride.get("storeValues");
          EXPECT_TRUE((true == tmpSlice.isString() &&
                       std::string("id") == tmpSlice.copyString()));
        } else if ("none" == fieldOverride.copyString()) {
          EXPECT_TRUE((0U == sliceOverride.length()));
        }
      }
    }

    EXPECT_TRUE((true == expectedOverrides.empty()));
    EXPECT_TRUE((true == expectedFields.empty()));
    tmpSlice = slice.get("includeAllFields");
    EXPECT_TRUE((true == tmpSlice.isBool() && true == tmpSlice.getBool()));
    tmpSlice = slice.get("trackListPositions");
    EXPECT_TRUE((true == tmpSlice.isBool() && true == tmpSlice.getBool()));
    tmpSlice = slice.get("storeValues");
    EXPECT_TRUE((true == tmpSlice.isString() && std::string("full") == tmpSlice.copyString()));
    tmpSlice = slice.get("analyzers");
    EXPECT_TRUE((true == tmpSlice.isArray() && 2 == tmpSlice.length()));

    for (arangodb::velocypack::ArrayIterator analyzersItr(tmpSlice);
         analyzersItr.valid(); ++analyzersItr) {
      auto key = *analyzersItr;
      EXPECT_TRUE((true == key.isString() && 1 == expectedAnalyzers.erase(key.copyString())));
    }

    EXPECT_TRUE((true == expectedAnalyzers.empty()));
    tmpSlice = slice.get("analyzerDefinitions");
    EXPECT_TRUE((true == tmpSlice.isArray() && 2 == tmpSlice.length()));

    for (arangodb::velocypack::ArrayIterator analyzersItr(tmpSlice);
         analyzersItr.valid(); ++analyzersItr) {
      auto value = *analyzersItr;
      EXPECT_TRUE(
          (true == value.isObject() && value.hasKey("name") &&
           value.get("name").isString() && value.hasKey("type") &&
           value.get("type").isString() && value.hasKey("properties") &&
           (value.get("properties").isString() || value.get("properties").isNull()) &&
           value.hasKey("features") && value.get("features").isArray() &&
           (1 == value.get("features").length() || 2 == value.get("features").length())  // empty/identity 1/2
           && 1 == expectedAnalyzerDefinitions.erase(
                       std::make_pair(value.get("name").copyString(),
                                      value.get("properties").isNull()
                                          ? ""
                                          : value.get("properties").copyString()))));
    }

    EXPECT_TRUE((true == expectedAnalyzerDefinitions.empty()));

    std::string errorField;
    tmpSlice = slice.get("primarySort");
    EXPECT_TRUE(tmpSlice.isArray());
    arangodb::iresearch::IResearchViewSort sort;
    EXPECT_TRUE(sort.fromVelocyPack(tmpSlice, errorField));
    EXPECT_TRUE(2 == sort.size());
    EXPECT_TRUE(true == sort.direction(0));
    EXPECT_TRUE((std::vector<arangodb::basics::AttributeName>{{"_key", false}} ==
                sort.field(0)));
    EXPECT_TRUE(false == sort.direction(1));
    EXPECT_TRUE((std::vector<arangodb::basics::AttributeName>{{"_id", false}} ==
                sort.field(1)));
  }

  // with active vocbase (not fullAnalyzerDefinition)
  {
    std::unordered_set<std::string> expectedFields = {"a", "b", "c"};
    std::unordered_set<std::string> expectedOverrides = {"default", "all",
                                                         "some", "none"};
    std::unordered_set<std::string> expectedAnalyzers = {"::empty", "identity"};
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                          "testVocbase");
    arangodb::velocypack::Builder builder;
    arangodb::velocypack::Slice tmpSlice;

    builder.openObject();
    EXPECT_TRUE((true == meta.json(builder, false, nullptr, &vocbase)));
    builder.close();

    auto slice = builder.slice();

    EXPECT_TRUE((5U == slice.length()));
    tmpSlice = slice.get("fields");
    EXPECT_TRUE((true == tmpSlice.isObject() && 3 == tmpSlice.length()));

    for (arangodb::velocypack::ObjectIterator itr(tmpSlice); itr.valid(); ++itr) {
      auto key = itr.key();
      auto value = itr.value();
      EXPECT_TRUE((true == key.isString() && 1 == expectedFields.erase(key.copyString())));
      EXPECT_TRUE((true == value.isObject()));

      if (!value.hasKey("fields")) {
        continue;
      }

      tmpSlice = value.get("fields");

      for (arangodb::velocypack::ObjectIterator overrideItr(tmpSlice);
           overrideItr.valid(); ++overrideItr) {
        auto fieldOverride = overrideItr.key();
        auto sliceOverride = overrideItr.value();
        EXPECT_TRUE((true == fieldOverride.isString() && sliceOverride.isObject()));
        EXPECT_TRUE((1U == expectedOverrides.erase(fieldOverride.copyString())));

        if ("default" == fieldOverride.copyString()) {
          EXPECT_TRUE((4U == sliceOverride.length()));
          tmpSlice = sliceOverride.get("includeAllFields");
          EXPECT_TRUE((true == (false == tmpSlice.getBool())));
          tmpSlice = sliceOverride.get("trackListPositions");
          EXPECT_TRUE((true == (false == tmpSlice.getBool())));
          tmpSlice = sliceOverride.get("storeValues");
          EXPECT_TRUE((true == tmpSlice.isString() &&
                       std::string("none") == tmpSlice.copyString()));
          tmpSlice = sliceOverride.get("analyzers");
          EXPECT_TRUE((true == tmpSlice.isArray() && 1 == tmpSlice.length() &&
                       tmpSlice.at(0).isString() &&
                       std::string("identity") == tmpSlice.at(0).copyString()));
        } else if ("all" == fieldOverride.copyString()) {
          std::unordered_set<std::string> expectedFields = {"x", "y"};
          EXPECT_TRUE((5U == sliceOverride.length()));
          tmpSlice = sliceOverride.get("fields");
          EXPECT_TRUE((true == tmpSlice.isObject() && 2 == tmpSlice.length()));
          for (arangodb::velocypack::ObjectIterator overrideFieldItr(tmpSlice);
               overrideFieldItr.valid(); ++overrideFieldItr) {
            EXPECT_TRUE((true == overrideFieldItr.key().isString() &&
                         1 == expectedFields.erase(overrideFieldItr.key().copyString())));
          }
          EXPECT_TRUE((true == expectedFields.empty()));
          tmpSlice = sliceOverride.get("includeAllFields");
          EXPECT_TRUE((true == tmpSlice.isBool() && false == tmpSlice.getBool()));
          tmpSlice = sliceOverride.get("trackListPositions");
          EXPECT_TRUE((true == tmpSlice.isBool() && false == tmpSlice.getBool()));
          tmpSlice = sliceOverride.get("storeValues");
          EXPECT_TRUE((true == tmpSlice.isString() &&
                       std::string("none") == tmpSlice.copyString()));
          tmpSlice = sliceOverride.get("analyzers");
          EXPECT_TRUE((true == tmpSlice.isArray() && 1 == tmpSlice.length() &&
                       tmpSlice.at(0).isString() &&
                       std::string("::empty") == tmpSlice.at(0).copyString()));
        } else if ("some" == fieldOverride.copyString()) {
          EXPECT_TRUE((2U == sliceOverride.length()));
          tmpSlice = sliceOverride.get("trackListPositions");
          EXPECT_TRUE((true == tmpSlice.isBool() && false == tmpSlice.getBool()));
          tmpSlice = sliceOverride.get("storeValues");
          EXPECT_TRUE((true == tmpSlice.isString() &&
                       std::string("id") == tmpSlice.copyString()));
        } else if ("none" == fieldOverride.copyString()) {
          EXPECT_TRUE((0U == sliceOverride.length()));
        }
      }
    }

    EXPECT_TRUE((true == expectedOverrides.empty()));
    EXPECT_TRUE((true == expectedFields.empty()));
    tmpSlice = slice.get("includeAllFields");
    EXPECT_TRUE((true == tmpSlice.isBool() && true == tmpSlice.getBool()));
    tmpSlice = slice.get("trackListPositions");
    EXPECT_TRUE((true == tmpSlice.isBool() && true == tmpSlice.getBool()));
    tmpSlice = slice.get("storeValues");
    EXPECT_TRUE((true == tmpSlice.isString() && std::string("full") == tmpSlice.copyString()));
    tmpSlice = slice.get("analyzers");
    EXPECT_TRUE((true == tmpSlice.isArray() && 2 == tmpSlice.length()));

    for (arangodb::velocypack::ArrayIterator analyzersItr(tmpSlice);
         analyzersItr.valid(); ++analyzersItr) {
      auto key = *analyzersItr;
      EXPECT_TRUE((true == key.isString() && 1 == expectedAnalyzers.erase(key.copyString())));
    }

    EXPECT_TRUE((true == expectedAnalyzers.empty()));
  }

  // with active vocbase (with fullAnalyzerDefinition)
  {
    std::unordered_set<std::string> expectedFields = {"a", "b", "c"};
    std::unordered_set<std::string> expectedOverrides = {"default", "all",
                                                         "some", "none"};
    std::unordered_set<std::string> expectedAnalyzers = {
        arangodb::StaticStrings::SystemDatabase + "::empty", "identity"};
    std::set<std::pair<std::string, std::string>> expectedAnalyzerDefinitions = {
        {arangodb::StaticStrings::SystemDatabase + "::empty", "en"},
        {"identity", "{}"},
    };
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                          "testVocbase");
    arangodb::velocypack::Builder builder;
    arangodb::velocypack::Slice tmpSlice;

    builder.openObject();
    EXPECT_TRUE((true == meta.json(builder, true, nullptr, &vocbase)));
    builder.close();

    auto slice = builder.slice();

    EXPECT_TRUE((7U == slice.length()));
    tmpSlice = slice.get("fields");
    EXPECT_TRUE((true == tmpSlice.isObject() && 3 == tmpSlice.length()));

    for (arangodb::velocypack::ObjectIterator itr(tmpSlice); itr.valid(); ++itr) {
      auto key = itr.key();
      auto value = itr.value();
      EXPECT_TRUE((true == key.isString() && 1 == expectedFields.erase(key.copyString())));
      EXPECT_TRUE((true == value.isObject()));

      if (!value.hasKey("fields")) {
        continue;
      }

      tmpSlice = value.get("fields");

      for (arangodb::velocypack::ObjectIterator overrideItr(tmpSlice);
           overrideItr.valid(); ++overrideItr) {
        auto fieldOverride = overrideItr.key();
        auto sliceOverride = overrideItr.value();
        EXPECT_TRUE((true == fieldOverride.isString() && sliceOverride.isObject()));
        EXPECT_TRUE((1U == expectedOverrides.erase(fieldOverride.copyString())));

        if ("default" == fieldOverride.copyString()) {
          EXPECT_TRUE((4U == sliceOverride.length()));
          tmpSlice = sliceOverride.get("includeAllFields");
          EXPECT_TRUE((true == (false == tmpSlice.getBool())));
          tmpSlice = sliceOverride.get("trackListPositions");
          EXPECT_TRUE((true == (false == tmpSlice.getBool())));
          tmpSlice = sliceOverride.get("storeValues");
          EXPECT_TRUE((true == tmpSlice.isString() &&
                       std::string("none") == tmpSlice.copyString()));
          tmpSlice = sliceOverride.get("analyzers");
          EXPECT_TRUE((true == tmpSlice.isArray() && 1 == tmpSlice.length() &&
                       tmpSlice.at(0).isString() &&
                       std::string("identity") == tmpSlice.at(0).copyString()));
        } else if ("all" == fieldOverride.copyString()) {
          std::unordered_set<std::string> expectedFields = {"x", "y"};
          EXPECT_TRUE((5U == sliceOverride.length()));
          tmpSlice = sliceOverride.get("fields");
          EXPECT_TRUE((true == tmpSlice.isObject() && 2 == tmpSlice.length()));
          for (arangodb::velocypack::ObjectIterator overrideFieldItr(tmpSlice);
               overrideFieldItr.valid(); ++overrideFieldItr) {
            EXPECT_TRUE((true == overrideFieldItr.key().isString() &&
                         1 == expectedFields.erase(overrideFieldItr.key().copyString())));
          }
          EXPECT_TRUE((true == expectedFields.empty()));
          tmpSlice = sliceOverride.get("includeAllFields");
          EXPECT_TRUE((true == tmpSlice.isBool() && false == tmpSlice.getBool()));
          tmpSlice = sliceOverride.get("trackListPositions");
          EXPECT_TRUE((true == tmpSlice.isBool() && false == tmpSlice.getBool()));
          tmpSlice = sliceOverride.get("storeValues");
          EXPECT_TRUE((true == tmpSlice.isString() &&
                       std::string("none") == tmpSlice.copyString()));
          tmpSlice = sliceOverride.get("analyzers");
          EXPECT_TRUE((true == tmpSlice.isArray() && 1 == tmpSlice.length() &&
                       tmpSlice.at(0).isString() &&
                       arangodb::StaticStrings::SystemDatabase + "::empty" ==
                           tmpSlice.at(0).copyString()));
        } else if ("some" == fieldOverride.copyString()) {
          EXPECT_TRUE((2U == sliceOverride.length()));
          tmpSlice = sliceOverride.get("trackListPositions");
          EXPECT_TRUE((true == tmpSlice.isBool() && false == tmpSlice.getBool()));
          tmpSlice = sliceOverride.get("storeValues");
          EXPECT_TRUE((true == tmpSlice.isString() &&
                       std::string("id") == tmpSlice.copyString()));
        } else if ("none" == fieldOverride.copyString()) {
          EXPECT_TRUE((0U == sliceOverride.length()));
        }
      }
    }

    EXPECT_TRUE((true == expectedOverrides.empty()));
    EXPECT_TRUE((true == expectedFields.empty()));
    tmpSlice = slice.get("includeAllFields");
    EXPECT_TRUE((true == tmpSlice.isBool() && true == tmpSlice.getBool()));
    tmpSlice = slice.get("trackListPositions");
    EXPECT_TRUE((true == tmpSlice.isBool() && true == tmpSlice.getBool()));
    tmpSlice = slice.get("storeValues");
    EXPECT_TRUE((true == tmpSlice.isString() && std::string("full") == tmpSlice.copyString()));
    tmpSlice = slice.get("analyzers");
    EXPECT_TRUE((true == tmpSlice.isArray() && 2 == tmpSlice.length()));

    for (arangodb::velocypack::ArrayIterator analyzersItr(tmpSlice);
         analyzersItr.valid(); ++analyzersItr) {
      auto key = *analyzersItr;
      EXPECT_TRUE((true == key.isString() && 1 == expectedAnalyzers.erase(key.copyString())));
    }

    EXPECT_TRUE((true == expectedAnalyzers.empty()));
    tmpSlice = slice.get("analyzerDefinitions");
    EXPECT_TRUE((true == tmpSlice.isArray() && 2 == tmpSlice.length()));

    for (arangodb::velocypack::ArrayIterator analyzersItr(tmpSlice);
         analyzersItr.valid(); ++analyzersItr) {
      auto value = *analyzersItr;
      EXPECT_TRUE(
          (true == value.isObject() && value.hasKey("name") &&
           value.get("name").isString() && value.hasKey("type") &&
           value.get("type").isString() && value.hasKey("properties") &&
           (value.get("properties").isString() || value.get("properties").isNull()) &&
           value.hasKey("features") && value.get("features").isArray() &&
           (1 == value.get("features").length() || 2 == value.get("features").length())  // empty/identity 1/2
           && 1 == expectedAnalyzerDefinitions.erase(
                       std::make_pair(value.get("name").copyString(),
                                      value.get("properties").isNull()
                                          ? ""
                                          : value.get("properties").copyString()))));
    }

    EXPECT_TRUE((true == expectedAnalyzerDefinitions.empty()));

    std::string errorField;
    tmpSlice = slice.get("primarySort");
    EXPECT_TRUE(tmpSlice.isArray());
    EXPECT_TRUE(2 == tmpSlice.length());

    {
      auto valueSlice = tmpSlice.at(0);
      EXPECT_TRUE(valueSlice.isObject());
      EXPECT_TRUE(2 == valueSlice.length());
      EXPECT_TRUE(valueSlice.get("field").isString());
      EXPECT_TRUE("_key" == valueSlice.get("field").copyString());
      EXPECT_TRUE(valueSlice.get("asc").isBool());
      EXPECT_TRUE(valueSlice.get("asc").getBool());
    }

    {
      auto valueSlice = tmpSlice.at(1);
      EXPECT_TRUE(valueSlice.isObject());
      EXPECT_TRUE(2 == valueSlice.length());
      EXPECT_TRUE(valueSlice.get("field").isString());
      EXPECT_TRUE("_id" == valueSlice.get("field").copyString());
      EXPECT_TRUE(valueSlice.get("asc").isBool());
      EXPECT_TRUE(!valueSlice.get("asc").getBool());
    }
  }
}

TEST_F(IResearchLinkMetaTest, test_readMaskAll) {
  arangodb::iresearch::IResearchLinkMeta meta;
  arangodb::iresearch::IResearchLinkMeta::Mask mask;
  std::string tmpString;

  auto json = arangodb::velocypack::Parser::fromJson(
      "{ \
    \"fields\": { \"a\": {} }, \
    \"includeAllFields\": true, \
    \"trackListPositions\": true, \
    \"storeValues\": \"full\", \
    \"analyzers\": [] \
  }");
  EXPECT_TRUE(true == meta.init(json->slice(), false, tmpString, nullptr,
                                arangodb::iresearch::IResearchLinkMeta::DEFAULT(), &mask));
  EXPECT_TRUE(true == mask._fields);
  EXPECT_TRUE(true == mask._includeAllFields);
  EXPECT_TRUE(true == mask._trackListPositions);
  EXPECT_TRUE((true == mask._storeValues));
  EXPECT_TRUE(true == mask._analyzers);
}

TEST_F(IResearchLinkMetaTest, test_readMaskNone) {
  arangodb::iresearch::IResearchLinkMeta meta;
  arangodb::iresearch::IResearchLinkMeta::Mask mask;
  std::string tmpString;

  auto json = arangodb::velocypack::Parser::fromJson("{}");
  EXPECT_TRUE(true == meta.init(json->slice(), false, tmpString, nullptr,
                                arangodb::iresearch::IResearchLinkMeta::DEFAULT(), &mask));
  EXPECT_TRUE(false == mask._fields);
  EXPECT_TRUE(false == mask._includeAllFields);
  EXPECT_TRUE(false == mask._trackListPositions);
  EXPECT_TRUE((false == mask._storeValues));
  EXPECT_TRUE(false == mask._analyzers);
}

TEST_F(IResearchLinkMetaTest, test_writeMaskAll) {
  // not fullAnalyzerDefinition
  {
    arangodb::iresearch::IResearchLinkMeta meta;
    arangodb::iresearch::IResearchLinkMeta::Mask mask(true);
    arangodb::velocypack::Builder builder;

    builder.openObject();
    EXPECT_TRUE((true == meta.json(builder, false, nullptr, nullptr, &mask)));
    builder.close();

    auto slice = builder.slice();

    EXPECT_TRUE((5U == slice.length()));
    EXPECT_TRUE(true == slice.hasKey("fields"));
    EXPECT_TRUE(true == slice.hasKey("includeAllFields"));
    EXPECT_TRUE(true == slice.hasKey("trackListPositions"));
    EXPECT_TRUE(true == slice.hasKey("storeValues"));
    EXPECT_TRUE(true == slice.hasKey("analyzers"));
  }

  // with fullAnalyzerDefinition
  {
    arangodb::iresearch::IResearchLinkMeta meta;
    arangodb::iresearch::IResearchLinkMeta::Mask mask(true);
    arangodb::velocypack::Builder builder;

    builder.openObject();
    EXPECT_TRUE((true == meta.json(builder, true, nullptr, nullptr, &mask)));
    builder.close();

    auto slice = builder.slice();

    EXPECT_TRUE((7U == slice.length()));
    EXPECT_TRUE(true == slice.hasKey("fields"));
    EXPECT_TRUE(true == slice.hasKey("includeAllFields"));
    EXPECT_TRUE(true == slice.hasKey("trackListPositions"));
    EXPECT_TRUE(true == slice.hasKey("storeValues"));
    EXPECT_TRUE(true == slice.hasKey("analyzers"));
    EXPECT_TRUE(true == slice.hasKey("analyzerDefinitions"));
    EXPECT_TRUE(true == slice.hasKey("primarySort"));
  }
}

TEST_F(IResearchLinkMetaTest, test_writeMaskNone) {
  // not fullAnalyzerDefinition
  {
    arangodb::iresearch::IResearchLinkMeta meta;
    arangodb::iresearch::IResearchLinkMeta::Mask mask(false);
    arangodb::velocypack::Builder builder;

    builder.openObject();
    EXPECT_TRUE((true == meta.json(builder, false, nullptr, nullptr, &mask)));
    builder.close();

    auto slice = builder.slice();

    EXPECT_TRUE(0U == slice.length());
  }

  // with fullAnalyzerDefinition
  {
    arangodb::iresearch::IResearchLinkMeta meta;
    arangodb::iresearch::IResearchLinkMeta::Mask mask(false);
    arangodb::velocypack::Builder builder;

    builder.openObject();
    EXPECT_TRUE((true == meta.json(builder, true, nullptr, nullptr, &mask)));
    builder.close();

    auto slice = builder.slice();

    EXPECT_TRUE(0U == slice.length());
  }
}

TEST_F(IResearchLinkMetaTest, test_readAnalyzerDefinitions) {
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                        "testVocbase");

  // missing analyzer (name only)
  {
    auto json = arangodb::velocypack::Parser::fromJson(
        "{ \
      \"analyzers\": [ \"empty1\" ] \
    }");
    arangodb::iresearch::IResearchLinkMeta meta;
    std::string errorField;
    EXPECT_TRUE((false == meta.init(json->slice(), true, errorField, &vocbase)));
    EXPECT_TRUE((std::string("analyzers=>empty1") == errorField));
  }

  // missing analyzer (name only) inRecovery
  {
    auto json = arangodb::velocypack::Parser::fromJson(
        "{ \
      \"analyzers\": [ \"empty1\" ] \
    }");
    auto before = StorageEngineMock::inRecoveryResult;
    StorageEngineMock::inRecoveryResult = true;
    auto restore = irs::make_finally(
        [&before]() -> void { StorageEngineMock::inRecoveryResult = before; });
    arangodb::iresearch::IResearchLinkMeta meta;
    std::string errorField;
    EXPECT_TRUE((false == meta.init(json->slice(), true, errorField, &vocbase)));
    EXPECT_TRUE((std::string("analyzers=>empty1") == errorField));
  }

  // missing analyzer (full) no name (fail) required
  {
    auto json = arangodb::velocypack::Parser::fromJson(
        "{ \
      \"analyzerDefinitions\": [ { \"type\": \"empty\", \"properties\": \"ru\", \"features\": [ \"frequency\" ] } ], \
      \"analyzers\": [ \"empty\" ] \
    }");
    arangodb::iresearch::IResearchLinkMeta meta;
    std::string errorField;
    EXPECT_TRUE((false == meta.init(json->slice(), true, errorField, &vocbase)));
    EXPECT_TRUE((std::string("analyzerDefinitions=>[0]=>name") == errorField));
  }

  // missing analyzer (full) no type (fail) required
  {
    auto json = arangodb::velocypack::Parser::fromJson(
        "{ \
      \"analyzerDefinitions\": [ { \"name\": \"missing0\", \"properties\": \"ru\", \"features\": [ \"frequency\" ] } ], \
      \"analyzers\": [ \"missing0\" ] \
    }");
    arangodb::iresearch::IResearchLinkMeta meta;
    std::string errorField;
    EXPECT_TRUE((false == meta.init(json->slice(), true, errorField, &vocbase)));
    EXPECT_TRUE((std::string("analyzerDefinitions=>[0]=>type") == errorField));
  }

  // missing analyzer (full) analyzer creation not allowed (fail)
  {
    auto json = arangodb::velocypack::Parser::fromJson(
        "{ \
      \"analyzerDefinitions\": [ { \"name\": \"missing0\", \"type\": \"empty\", \"properties\": \"ru\", \"features\": [ \"frequency\" ] } ], \
      \"analyzers\": [ \"missing0\" ] \
    }");
    arangodb::iresearch::IResearchLinkMeta meta;
    std::string errorField;
    EXPECT_TRUE((false == meta.init(json->slice(), true, errorField, &vocbase)));
    EXPECT_TRUE((std::string("analyzerDefinitions=>[0]") == errorField));
  }

  // missing analyzer (full) single-server
  {
    auto json = arangodb::velocypack::Parser::fromJson(
        "{ \
      \"analyzerDefinitions\": [ { \"name\": \"missing0\", \"type\": \"empty\", \"properties\": \"ru\", \"features\": [ \"frequency\" ] } ], \
      \"analyzers\": [ \"missing0\" ] \
    }");
    arangodb::iresearch::IResearchLinkMeta meta;
    std::string errorField;
    EXPECT_TRUE((false == meta.init(json->slice(), true, errorField, &vocbase)));
    EXPECT_TRUE((std::string("analyzerDefinitions=>[0]") == errorField));
  }

  // missing analyzer (full) coordinator
  {
    auto before = arangodb::ServerState::instance()->getRole();
    arangodb::ServerState::instance()->setRole(arangodb::ServerState::ROLE_COORDINATOR);
    auto restore = irs::make_finally([&before]() -> void {
      arangodb::ServerState::instance()->setRole(before);
    });

    auto json = arangodb::velocypack::Parser::fromJson(
        "{ \
      \"analyzerDefinitions\": [ { \"name\": \"missing1\", \"type\": \"empty\", \"properties\": \"ru\", \"features\": [ \"frequency\" ] } ], \
      \"analyzers\": [ \"missing1\" ] \
    }");
    arangodb::iresearch::IResearchLinkMeta meta;
    std::string errorField;
    EXPECT_TRUE((false == meta.init(json->slice(), true, errorField, &vocbase)));
    EXPECT_TRUE((std::string("analyzerDefinitions=>[0]") == errorField));
  }

  // missing analyzer (full) db-server
  {
    auto before = arangodb::ServerState::instance()->getRole();
    arangodb::ServerState::instance()->setRole(arangodb::ServerState::ROLE_DBSERVER);
    auto restore = irs::make_finally([&before]() -> void {
      arangodb::ServerState::instance()->setRole(before);
    });

    auto json = arangodb::velocypack::Parser::fromJson(
        "{ \
      \"analyzerDefinitions\": [ { \"name\": \"missing2\", \"type\": \"empty\", \"properties\": \"ru\", \"features\": [ \"frequency\" ] } ], \
      \"analyzers\": [ \"missing2\" ] \
    }");
    arangodb::iresearch::IResearchLinkMeta meta;
    std::string errorField;
    EXPECT_TRUE((true == meta.init(json->slice(), true, errorField, &vocbase)));
    EXPECT_TRUE((1 == meta._analyzers.size()));
    EXPECT_TRUE(
        (std::string("testVocbase::missing2") == meta._analyzers[0]._pool->name()));
    EXPECT_TRUE((std::string("empty") == meta._analyzers[0]._pool->type()));
    EXPECT_TRUE((std::string("ru") == meta._analyzers[0]._pool->properties()));
    EXPECT_TRUE((1 == meta._analyzers[0]._pool->features().size()));
    EXPECT_TRUE((true == meta._analyzers[0]._pool->features().check(irs::frequency::type())));
    EXPECT_TRUE((std::string("missing2") == meta._analyzers[0]._shortName));
  }

  // missing analyzer (full) inRecovery
  {
    auto json = arangodb::velocypack::Parser::fromJson(
        "{ \
      \"analyzerDefinitions\": [ { \"name\": \"missing3\", \"type\": \"empty\", \"properties\": \"ru\", \"features\": [ \"frequency\" ] } ], \
      \"analyzers\": [ \"missing3\" ] \
    }");
    auto before = StorageEngineMock::inRecoveryResult;
    StorageEngineMock::inRecoveryResult = true;
    auto restore = irs::make_finally(
        [&before]() -> void { StorageEngineMock::inRecoveryResult = before; });
    arangodb::iresearch::IResearchLinkMeta meta;
    std::string errorField;
    EXPECT_TRUE((false == meta.init(json->slice(), true, errorField, &vocbase)));
    EXPECT_TRUE((std::string("analyzers=>missing3") == errorField));  // not in the persisted collection
  }

  // existing analyzer (name only)
  {
    auto json = arangodb::velocypack::Parser::fromJson(
        "{ \
      \"analyzers\": [ \"empty\" ] \
    }");
    arangodb::iresearch::IResearchLinkMeta meta;
    std::string errorField;
    EXPECT_TRUE((true == meta.init(json->slice(), true, errorField, &vocbase)));
    EXPECT_TRUE((1 == meta._analyzers.size()));
    EXPECT_TRUE((std::string("testVocbase::empty") == meta._analyzers[0]._pool->name()));
    EXPECT_TRUE((std::string("empty") == meta._analyzers[0]._pool->type()));
    EXPECT_TRUE((std::string("de") == meta._analyzers[0]._pool->properties()));
    EXPECT_TRUE((1 == meta._analyzers[0]._pool->features().size()));
    EXPECT_TRUE((true == meta._analyzers[0]._pool->features().check(irs::frequency::type())));
    EXPECT_TRUE((std::string("empty") == meta._analyzers[0]._shortName));
  }

  // existing analyzer (name only) inRecovery
  {
    auto json = arangodb::velocypack::Parser::fromJson(
        "{ \
      \"analyzers\": [ \"empty\" ] \
    }");
    auto before = StorageEngineMock::inRecoveryResult;
    StorageEngineMock::inRecoveryResult = true;
    auto restore = irs::make_finally(
        [&before]() -> void { StorageEngineMock::inRecoveryResult = before; });
    arangodb::iresearch::IResearchLinkMeta meta;
    std::string errorField;
    EXPECT_TRUE((true == meta.init(json->slice(), true, errorField, &vocbase)));
    EXPECT_TRUE((1 == meta._analyzers.size()));
    EXPECT_TRUE((std::string("testVocbase::empty") == meta._analyzers[0]._pool->name()));
    EXPECT_TRUE((std::string("empty") == meta._analyzers[0]._pool->type()));
    EXPECT_TRUE((std::string("de") == meta._analyzers[0]._pool->properties()));
    EXPECT_TRUE((1 == meta._analyzers[0]._pool->features().size()));
    EXPECT_TRUE((true == meta._analyzers[0]._pool->features().check(irs::frequency::type())));
    EXPECT_TRUE((std::string("empty") == meta._analyzers[0]._shortName));
  }

  // existing analyzer (full) analyzer creation not allowed (pass)
  {
    auto json = arangodb::velocypack::Parser::fromJson(
        "{ \
      \"analyzerDefinitions\": [ { \"name\": \"empty\", \"type\": \"empty\", \"properties\": \"de\", \"features\": [ \"frequency\" ] } ], \
      \"analyzers\": [ \"empty\" ] \
    }");
    arangodb::iresearch::IResearchLinkMeta meta;
    std::string errorField;
    EXPECT_TRUE((true == meta.init(json->slice(), true, errorField, &vocbase)));
    EXPECT_TRUE((1 == meta._analyzers.size()));
    EXPECT_TRUE((std::string("testVocbase::empty") == meta._analyzers[0]._pool->name()));
    EXPECT_TRUE((std::string("empty") == meta._analyzers[0]._pool->type()));
    EXPECT_TRUE((std::string("de") == meta._analyzers[0]._pool->properties()));
    EXPECT_TRUE((1 == meta._analyzers[0]._pool->features().size()));
    EXPECT_TRUE((true == meta._analyzers[0]._pool->features().check(irs::frequency::type())));
    EXPECT_TRUE((std::string("empty") == meta._analyzers[0]._shortName));
  }

  // existing analyzer (full) analyzer definition not allowed
  {
    auto json = arangodb::velocypack::Parser::fromJson(
        "{ \
      \"analyzers\": [ { \"name\": \"empty\", \"type\": \"empty\", \"properties\": \"de\", \"features\": [ \"frequency\" ] } ], \
      \"analyzers\": [ \"empty\" ] \
    }");
    arangodb::iresearch::IResearchLinkMeta meta;
    std::string errorField;
    EXPECT_TRUE((false == meta.init(json->slice(), false, errorField, &vocbase)));
    EXPECT_TRUE((std::string("analyzers=>[0]") == errorField));
  }

  // existing analyzer (full)
  {
    auto json = arangodb::velocypack::Parser::fromJson(
        "{ \
      \"analyzerDefinitions\": [ { \"name\": \"empty\", \"type\": \"empty\", \"properties\": \"de\", \"features\": [ \"frequency\" ] } ], \
      \"analyzers\": [ \"empty\" ] \
    }");
    arangodb::iresearch::IResearchLinkMeta meta;
    std::string errorField;
    EXPECT_TRUE((true == meta.init(json->slice(), true, errorField, &vocbase)));
    EXPECT_TRUE((1 == meta._analyzers.size()));
    EXPECT_TRUE((std::string("testVocbase::empty") == meta._analyzers[0]._pool->name()));
    EXPECT_TRUE((std::string("empty") == meta._analyzers[0]._pool->type()));
    EXPECT_TRUE((std::string("de") == meta._analyzers[0]._pool->properties()));
    EXPECT_TRUE((1 == meta._analyzers[0]._pool->features().size()));
    EXPECT_TRUE((true == meta._analyzers[0]._pool->features().check(irs::frequency::type())));
    EXPECT_TRUE((std::string("empty") == meta._analyzers[0]._shortName));
  }

  // existing analyzer (full) inRecovery
  {
    auto json = arangodb::velocypack::Parser::fromJson(
        "{ \
      \"analyzerDefinitions\": [ { \"name\": \"empty\", \"type\": \"empty\", \"properties\": \"de\", \"features\": [ \"frequency\" ] } ], \
      \"analyzers\": [ \"empty\" ] \
    }");
    auto before = StorageEngineMock::inRecoveryResult;
    StorageEngineMock::inRecoveryResult = true;
    auto restore = irs::make_finally(
        [&before]() -> void { StorageEngineMock::inRecoveryResult = before; });
    arangodb::iresearch::IResearchLinkMeta meta;
    std::string errorField;
    EXPECT_TRUE((true == meta.init(json->slice(), true, errorField, &vocbase)));
    EXPECT_TRUE((1 == meta._analyzers.size()));
    EXPECT_TRUE((std::string("testVocbase::empty") == meta._analyzers[0]._pool->name()));
    EXPECT_TRUE((std::string("empty") == meta._analyzers[0]._pool->type()));
    EXPECT_TRUE((std::string("de") == meta._analyzers[0]._pool->properties()));
    EXPECT_TRUE((1 == meta._analyzers[0]._pool->features().size()));
    EXPECT_TRUE((true == meta._analyzers[0]._pool->features().check(irs::frequency::type())));
    EXPECT_TRUE((std::string("empty") == meta._analyzers[0]._shortName));
  }

  // existing analyzer (definition mismatch)
  {
    auto json = arangodb::velocypack::Parser::fromJson(
        "{ \
      \"analyzerDefinitions\": [ { \"name\": \"empty\", \"type\": \"empty\", \"properties\": \"ru\", \"features\": [ \"frequency\" ] } ], \
      \"analyzers\": [ \"empty\" ] \
    }");
    arangodb::iresearch::IResearchLinkMeta meta;
    std::string errorField;
    EXPECT_TRUE((false == meta.init(json->slice(), true, errorField, &vocbase)));
    EXPECT_TRUE((std::string("analyzerDefinitions=>[0]") == errorField));
  }

  // existing analyzer (definition mismatch) inRecovery
  {
    auto json = arangodb::velocypack::Parser::fromJson(
        "{ \
      \"analyzerDefinitions\": [ { \"name\": \"empty\", \"type\": \"empty\", \"properties\": \"ru\", \"features\": [ \"frequency\" ] } ], \
      \"analyzers\": [ \"empty\" ] \
    }");
    auto before = StorageEngineMock::inRecoveryResult;
    StorageEngineMock::inRecoveryResult = true;
    auto restore = irs::make_finally(
        [&before]() -> void { StorageEngineMock::inRecoveryResult = before; });
    arangodb::iresearch::IResearchLinkMeta meta;
    std::string errorField;
    EXPECT_TRUE((false == meta.init(json->slice(), true, errorField, &vocbase)));
    EXPECT_TRUE((std::string("analyzerDefinitions=>[0]") == errorField));
  }
}


// https://github.com/arangodb/backlog/issues/581
// (ArangoSearch view doesn't validate uniqueness of analyzers)
TEST_F(IResearchLinkMetaTest, test_addNonUniqueAnalyzers) {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                          "testVocbase");
    auto* analyzers =
        arangodb::application_features::ApplicationServer::lookupFeature<arangodb::iresearch::IResearchAnalyzerFeature>();

    const std::string analyzerCustomName =
        "test_addNonUniqueAnalyzersMyIdentity";
    const std::string analyzerCustomInSystem =
        arangodb::StaticStrings::SystemDatabase + "::" + analyzerCustomName;
    const std::string analyzerCustomInTestVocbase =
        vocbase.name() + "::" + analyzerCustomName;

    // this is for test cleanup
    auto testCleanup = irs::make_finally([&analyzerCustomInSystem, &analyzers,
                                      &analyzerCustomInTestVocbase]() -> void {
      analyzers->remove(analyzerCustomInSystem);
      analyzers->remove(analyzerCustomInTestVocbase);
    });

    {
      arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult emplaceResult;
      // we need custom analyzer in _SYSTEM database and other will be in testVocbase with same name (it is ok to add both!).
      analyzers->emplace(emplaceResult, analyzerCustomInSystem, "identity", "en",
                         {irs::frequency::type()});
    }

    {
      arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult emplaceResult;
      analyzers->emplace(emplaceResult, analyzerCustomInTestVocbase, "identity",
                         "en", {irs::frequency::type()});
    }

    {

      const std::string testJson = std::string(
          "{ \
          \"includeAllFields\": true, \
          \"trackListPositions\": true, \
          \"storeValues\": \"full\",\
          \"analyzers\": [ \"identity\",\"identity\""  // two built-in analyzers
          ", \"" + analyzerCustomInTestVocbase + "\"" +  // local analyzer by full name
          ", \"" + analyzerCustomName + "\"" +  // local analyzer by short name
          ", \"::" + analyzerCustomName + "\"" +  // _system analyzer by short name
          ", \"" + analyzerCustomInSystem + "\"" +  // _system analyzer by full name
          " ] \
        }");

      arangodb::iresearch::IResearchLinkMeta meta;

      std::unordered_set<std::string> expectedAnalyzers;
      expectedAnalyzers.insert(analyzers->get("identity")->name());
      expectedAnalyzers.insert(analyzers->get(analyzerCustomInTestVocbase)->name());
      expectedAnalyzers.insert(analyzers->get(analyzerCustomInSystem)->name());
      
      arangodb::iresearch::IResearchLinkMeta::Mask mask(false);
      auto json = arangodb::velocypack::Parser::fromJson(testJson);
      std::string errorField;
      EXPECT_TRUE(true == meta.init(json->slice(), false, errorField, &vocbase,
                                    arangodb::iresearch::IResearchLinkMeta::DEFAULT(),
                                    &mask));
      EXPECT_TRUE(true == mask._analyzers);
      EXPECT_TRUE(errorField.empty());
      EXPECT_EQ(expectedAnalyzers.size(), meta._analyzers.size()); 

      for (decltype(meta._analyzers)::const_iterator analyzersItr = meta._analyzers.begin();
           analyzersItr != meta._analyzers.end(); ++analyzersItr) {
        EXPECT_TRUE(1 == expectedAnalyzers.erase(analyzersItr->_pool->name()));
      }
      EXPECT_TRUE((true == expectedAnalyzers.empty()));
   }
 }
