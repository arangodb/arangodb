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
#include "common.h"

#include "../Mocks/StorageEngineMock.h"

#include "analysis/analyzers.hpp"
#include "analysis/token_attributes.hpp"
#include "utils/locale_utils.hpp"

#include "ApplicationFeatures/BasicPhase.h"
#include "ApplicationFeatures/CommunicationPhase.h"
#include "ApplicationFeatures/ClusterPhase.h"
#include "ApplicationFeatures/DatabasePhase.h"
#include "ApplicationFeatures/GreetingsPhase.h"
#include "ApplicationFeatures/V8Phase.h"
#include "ApplicationFeatures/ClusterPhase.h"
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
#include "IResearch/IResearchLinkMeta.h"
#include "IResearch/IResearchFeature.h"
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

struct TestAttributeZ: public irs::attribute {
  DECLARE_ATTRIBUTE_TYPE();
};

DEFINE_ATTRIBUTE_TYPE(TestAttributeZ);
REGISTER_ATTRIBUTE(TestAttributeZ);

class EmptyAnalyzer: public irs::analysis::analyzer {
public:
  DECLARE_ANALYZER_TYPE();
  EmptyAnalyzer() : irs::analysis::analyzer(EmptyAnalyzer::type()) {
    _attrs.emplace(_attr);
  }
  virtual irs::attribute_view const& attributes() const NOEXCEPT override { return _attrs; }
  static ptr make(irs::string_ref const&) { PTR_NAMED(EmptyAnalyzer, ptr); return ptr; }
  virtual bool next() override { return false; }
  virtual bool reset(irs::string_ref const& data) override { return true; }

private:
  irs::attribute_view _attrs;
  TestAttributeZ _attr;
};

DEFINE_ANALYZER_TYPE_NAMED(EmptyAnalyzer, "empty");
REGISTER_ANALYZER_JSON(EmptyAnalyzer, EmptyAnalyzer::make);

}

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

struct IResearchLinkMetaSetup {
  StorageEngineMock engine;
  arangodb::application_features::ApplicationServer server;
  std::unique_ptr<TRI_vocbase_t> system;
  std::vector<std::pair<arangodb::application_features::ApplicationFeature*, bool>> features;
  std::vector<arangodb::application_features::ApplicationFeature*> orderedFeatures;

  IResearchLinkMetaSetup(): engine(server), server(nullptr, nullptr) {
    arangodb::EngineSelectorFeature::ENGINE = &engine;

    arangodb::tests::init();

    // suppress INFO {authentication} Authentication is turned on (system only), authentication for unix sockets is turned on
    arangodb::LogTopic::setLogLevel(arangodb::Logger::AUTHENTICATION.name(), arangodb::LogLevel::WARN);

    // setup required application features
    features.emplace_back(new arangodb::AuthenticationFeature(server), true);
    features.emplace_back(new arangodb::DatabaseFeature(server), false);
    features.emplace_back(new arangodb::ShardingFeature(server), false);
    features.emplace_back(new arangodb::QueryRegistryFeature(server), false);
    arangodb::application_features::ApplicationServer::server->addFeature(features.back().first); // need QueryRegistryFeature feature to be added now in order to create the system database
    system = irs::memory::make_unique<TRI_vocbase_t>(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 0, TRI_VOC_SYSTEM_DATABASE);
    features.emplace_back(new arangodb::SystemDatabaseFeature(server, system.get()), false); // required for IResearchAnalyzerFeature
    features.emplace_back(new arangodb::V8DealerFeature(server), false); // required for DatabaseFeature::createDatabase(...)
    features.emplace_back(new arangodb::aql::AqlFunctionFeature(server), true); // required for IResearchAnalyzerFeature
    features.emplace_back(new arangodb::iresearch::IResearchAnalyzerFeature(server), true);

    #if USE_ENTERPRISE
      features.emplace_back(new arangodb::LdapFeature(server), false); // required for AuthenticationFeature with USE_ENTERPRISE
    #endif

    // required for V8DealerFeature::prepare(), ClusterFeature::prepare() not required
    arangodb::application_features::ApplicationServer::server->addFeature(
      new arangodb::ClusterFeature(server)
    );

    for (auto& f: features) {
      arangodb::application_features::ApplicationServer::server->addFeature(f.first);
    }

    for (auto& f: features) {
      f.first->prepare();
    }

    for (auto& f: features) {
      if (f.second) {
        f.first->start();
      }
    }

    auto* dbFeature = arangodb::application_features::ApplicationServer::lookupFeature<
      arangodb::DatabaseFeature
    >("Database");
    TRI_vocbase_t* vocbase;
    dbFeature->createDatabase(1, "testVocbase", vocbase); // required for IResearchAnalyzerFeature::emplace(...)

    auto* analyzers = arangodb::application_features::ApplicationServer::lookupFeature<
      arangodb::iresearch::IResearchAnalyzerFeature
    >();
    arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;

    analyzers->emplace(result, "empty", "empty", "en", irs::flags{ TestAttributeZ::type() }); // cache the 'empty' analyzer
    analyzers->emplace(result, "testVocbase::empty", "empty", "de", irs::flags{ TestAttributeZ::type() }); // cache the 'empty' analyzer for 'testVocbase'

    // suppress log messages since tests check error conditions
    arangodb::LogTopic::setLogLevel(arangodb::iresearch::TOPIC.name(), arangodb::LogLevel::FATAL);
    irs::logger::output_le(iresearch::logger::IRL_FATAL, stderr);
  }

  ~IResearchLinkMetaSetup() {
    system.reset(); // destroy before reseting the 'ENGINE'
    arangodb::LogTopic::setLogLevel(arangodb::iresearch::TOPIC.name(), arangodb::LogLevel::DEFAULT);
    arangodb::application_features::ApplicationServer::server = nullptr;

    // destroy application features
    for (auto& f: features) {
      if (f.second) {
        f.first->stop();
      }
    }

    for (auto& f: features) {
      f.first->unprepare();
    }

    arangodb::LogTopic::setLogLevel(arangodb::Logger::AUTHENTICATION.name(), arangodb::LogLevel::DEFAULT);
    arangodb::EngineSelectorFeature::ENGINE = nullptr;
  }
};

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief setup
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("IResearchLinkMetaTest", "[iresearch][iresearch-linkmeta]") {
  IResearchLinkMetaSetup s;
  UNUSED(s);

SECTION("test_defaults") {
  arangodb::iresearch::IResearchLinkMeta meta;

  CHECK(true == meta._fields.empty());
  CHECK(false == meta._includeAllFields);
  CHECK(false == meta._trackListPositions);
  CHECK((arangodb::iresearch::ValueStorage::NONE == meta._storeValues));
  CHECK(1U == meta._analyzers.size());
  CHECK((*(meta._analyzers.begin())));
  CHECK(("identity" == (*(meta._analyzers.begin()))->name()));
  CHECK((irs::flags({irs::norm::type(), irs::frequency::type() }) == (*(meta._analyzers.begin()))->features()));
  CHECK(false == !meta._analyzers.begin()->get());
}

SECTION("test_inheritDefaults") {
  arangodb::iresearch::IResearchAnalyzerFeature analyzers(s.server);
  arangodb::iresearch::IResearchLinkMeta defaults;
  arangodb::iresearch::IResearchLinkMeta meta;
  std::unordered_set<std::string> expectedFields = { "abc" };
  std::unordered_set<std::string> expectedOverrides = { "xyz" };
  std::string tmpString;

  analyzers.start();

  defaults._fields["abc"] = arangodb::iresearch::IResearchLinkMeta();
  defaults._includeAllFields = true;
  defaults._trackListPositions = true;
  defaults._storeValues = arangodb::iresearch::ValueStorage::FULL;
  defaults._analyzers.clear();
  defaults._analyzers.emplace_back(analyzers.ensure("empty"));
  defaults._fields["abc"]->_fields["xyz"] = arangodb::iresearch::IResearchLinkMeta();

  auto json = arangodb::velocypack::Parser::fromJson("{}");
  CHECK(true == meta.init(json->slice(), tmpString, nullptr, defaults));
  CHECK(1U == meta._fields.size());

  for (auto& field: meta._fields) {
    CHECK(1U == expectedFields.erase(field.key()));
    CHECK(1U == field.value()->_fields.size());

    for (auto& fieldOverride: field.value()->_fields) {
      auto& actual = *(fieldOverride.value());
      CHECK(1U == expectedOverrides.erase(fieldOverride.key()));

      if ("xyz" == fieldOverride.key()) {
        CHECK(true == actual._fields.empty());
        CHECK(false == actual._includeAllFields);
        CHECK(false == actual._trackListPositions);
        CHECK((arangodb::iresearch::ValueStorage::NONE == actual._storeValues));
        CHECK(1U == actual._analyzers.size());
        CHECK((*(actual._analyzers.begin())));
        CHECK(("identity" == (*(actual._analyzers.begin()))->name()));
        CHECK((irs::flags({irs::norm::type(), irs::frequency::type()}) == (*(actual._analyzers.begin()))->features()));
        CHECK(false == !actual._analyzers.begin()->get());
      }
    }
  }

  CHECK(true == expectedOverrides.empty());
  CHECK(true == expectedFields.empty());
  CHECK(true == meta._includeAllFields);
  CHECK(true == meta._trackListPositions);
  CHECK((arangodb::iresearch::ValueStorage::FULL == meta._storeValues));

  CHECK(1U == meta._analyzers.size());
  CHECK((*(meta._analyzers.begin())));
  CHECK(("empty" == (*(meta._analyzers.begin()))->name()));
  CHECK((irs::flags() == (*(meta._analyzers.begin()))->features()));
  CHECK(false == !meta._analyzers.begin()->get());
}

SECTION("test_readDefaults") {
  auto json = arangodb::velocypack::Parser::fromJson("{}");

  // without active vobcase
  {
    arangodb::iresearch::IResearchLinkMeta meta;
    std::string tmpString;
    CHECK((true == meta.init(json->slice(), tmpString)));
    CHECK((true == meta._fields.empty()));
    CHECK((false == meta._includeAllFields));
    CHECK((false == meta._trackListPositions));
    CHECK((arangodb::iresearch::ValueStorage::NONE == meta._storeValues));
    CHECK((1U == meta._analyzers.size()));
    CHECK((*(meta._analyzers.begin())));
    CHECK(("identity" == (*(meta._analyzers.begin()))->name()));
    CHECK((irs::flags({irs::norm::type(), irs::frequency::type()}) == (*(meta._analyzers.begin()))->features()));
    CHECK((false == !meta._analyzers.begin()->get()));
  }

  // with active vocbase
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    arangodb::iresearch::IResearchLinkMeta meta;
    std::string tmpString;
    CHECK((true == meta.init(json->slice(), tmpString, &vocbase)));
    CHECK((true == meta._fields.empty()));
    CHECK((false == meta._includeAllFields));
    CHECK((false == meta._trackListPositions));
    CHECK((arangodb::iresearch::ValueStorage::NONE == meta._storeValues));
    CHECK((1U == meta._analyzers.size()));
    CHECK((*(meta._analyzers.begin())));
    CHECK(("identity" == (*(meta._analyzers.begin()))->name()));
    CHECK((irs::flags({irs::norm::type(), irs::frequency::type()}) == (*(meta._analyzers.begin()))->features()));
    CHECK((false == !meta._analyzers.begin()->get()));
  }
}

SECTION("test_readCustomizedValues") {
  auto json = arangodb::velocypack::Parser::fromJson("{ \
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
    std::unordered_set<std::string> expectedFields = { "a", "b", "c" };
    std::unordered_set<std::string> expectedOverrides = { "default", "all", "some", "none" };
    std::unordered_set<std::string> expectedAnalyzers = { "empty", "identity" };
    arangodb::iresearch::IResearchLinkMeta meta;
    std::string tmpString;
    CHECK(true == meta.init(json->slice(), tmpString));
    CHECK(3U == meta._fields.size());

    for (auto& field: meta._fields) {
      CHECK(1U == expectedFields.erase(field.key()));

      for (auto& fieldOverride: field.value()->_fields) {
        auto& actual = *(fieldOverride.value());

        CHECK(1U == expectedOverrides.erase(fieldOverride.key()));

        if ("default" == fieldOverride.key()) {
          CHECK(true == actual._fields.empty());
          CHECK(false == actual._includeAllFields);
          CHECK(false == actual._trackListPositions);
          CHECK((arangodb::iresearch::ValueStorage::NONE == actual._storeValues));
          CHECK(1U == actual._analyzers.size());
          CHECK((*(actual._analyzers.begin())));
          CHECK(("identity" == (*(actual._analyzers.begin()))->name()));
          CHECK((irs::flags({irs::norm::type(), irs::frequency::type()}) == (*(actual._analyzers.begin()))->features()));
          CHECK(false == !actual._analyzers.begin()->get());
        } else if ("all" == fieldOverride.key()) {
          CHECK(2U == actual._fields.size());
          CHECK(true == (actual._fields.find("d") != actual._fields.end()));
          CHECK(true == (actual._fields.find("e") != actual._fields.end()));
          CHECK(true == actual._includeAllFields);
          CHECK(true == actual._trackListPositions);
          CHECK((arangodb::iresearch::ValueStorage::FULL == actual._storeValues));
          CHECK(1U == actual._analyzers.size());
          CHECK((*(actual._analyzers.begin())));
          CHECK(("empty" == (*(actual._analyzers.begin()))->name()));
          CHECK((irs::flags({TestAttributeZ::type()}) == (*(actual._analyzers.begin()))->features()));
          CHECK(false == !actual._analyzers.begin()->get());
        } else if ("some" == fieldOverride.key()) {
          CHECK(true == actual._fields.empty()); // not inherited
          CHECK(true == actual._includeAllFields); // inherited
          CHECK(true == actual._trackListPositions);
          CHECK((arangodb::iresearch::ValueStorage::ID == actual._storeValues));
          CHECK(2U == actual._analyzers.size());
          auto itr = actual._analyzers.begin();
          CHECK((*itr));
          CHECK(("empty" == (*itr)->name()));
          CHECK((irs::flags({TestAttributeZ::type()}) == (*itr)->features()));
          CHECK(false == !itr->get());
          ++itr;
          CHECK((*itr));
          CHECK(("identity" == (*itr)->name()));
          CHECK((irs::flags({irs::norm::type(), irs::frequency::type()}) == (*itr)->features()));
          CHECK(false == !itr->get());
        } else if ("none" == fieldOverride.key()) {
          CHECK(true == actual._fields.empty()); // not inherited
          CHECK(true == actual._includeAllFields); // inherited
          CHECK(true == actual._trackListPositions); // inherited
          CHECK((arangodb::iresearch::ValueStorage::FULL == actual._storeValues));
          auto itr = actual._analyzers.begin();
          CHECK((*itr));
          CHECK(("empty" == (*itr)->name()));
          CHECK((irs::flags({TestAttributeZ::type()}) == (*itr)->features()));
          CHECK(false == !itr->get());
          ++itr;
          CHECK((*itr));
          CHECK(("identity" == (*itr)->name()));
          CHECK((irs::flags({irs::norm::type(), irs::frequency::type()}) == (*itr)->features()));
          CHECK(false == !itr->get());
        }
      }
    }

    CHECK(true == expectedOverrides.empty());
    CHECK(true == expectedFields.empty());
    CHECK(true == meta._includeAllFields);
    CHECK(true == meta._trackListPositions);
    CHECK((arangodb::iresearch::ValueStorage::FULL == meta._storeValues));
    auto itr = meta._analyzers.begin();
    CHECK((*itr));
    CHECK(("empty" == (*itr)->name()));
    CHECK((irs::flags({TestAttributeZ::type()}) == (*itr)->features()));
    CHECK(false == !itr->get());
    ++itr;
    CHECK((*itr));
    CHECK(("identity" == (*itr)->name()));
    CHECK((irs::flags({irs::norm::type(), irs::frequency::type()}) == (*itr)->features()));
    CHECK(false == !itr->get());
  }

  // with active vocbase
  {
    std::unordered_set<std::string> expectedFields = { "a", "b", "c" };
    std::unordered_set<std::string> expectedOverrides = { "default", "all", "some", "none" };
    std::unordered_set<std::string> expectedAnalyzers = { "empty", "identity" };
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    arangodb::iresearch::IResearchLinkMeta meta;
    std::string tmpString;
    CHECK((true == meta.init(json->slice(), tmpString, &vocbase)));
    CHECK((3U == meta._fields.size()));

    for (auto& field: meta._fields) {
      CHECK((1U == expectedFields.erase(field.key())));

      for (auto& fieldOverride: field.value()->_fields) {
        auto& actual = *(fieldOverride.value());

        CHECK((1U == expectedOverrides.erase(fieldOverride.key())));

        if ("default" == fieldOverride.key()) {
          CHECK((true == actual._fields.empty()));
          CHECK((false == actual._includeAllFields));
          CHECK((false == actual._trackListPositions));
          CHECK((arangodb::iresearch::ValueStorage::NONE == actual._storeValues));
          CHECK((1U == actual._analyzers.size()));
          CHECK((*(actual._analyzers.begin())));
          CHECK(("identity" == (*(actual._analyzers.begin()))->name()));
          CHECK((irs::flags({irs::norm::type(), irs::frequency::type()}) == (*(actual._analyzers.begin()))->features()));
          CHECK((false == !actual._analyzers.begin()->get()));
        } else if ("all" == fieldOverride.key()) {
          CHECK((2U == actual._fields.size()));
          CHECK((true == (actual._fields.find("d") != actual._fields.end())));
          CHECK((true == (actual._fields.find("e") != actual._fields.end())));
          CHECK((true == actual._includeAllFields));
          CHECK((true == actual._trackListPositions));
          CHECK((arangodb::iresearch::ValueStorage::FULL == actual._storeValues));
          CHECK((1U == actual._analyzers.size()));
          CHECK((*(actual._analyzers.begin())));
          /* FIXME TODO uncomment once emplace(...) and all tests are updated
          CHECK(("testVocbase::empty" == (*(actual._analyzers.begin()))->name()));
          */
          CHECK((irs::flags({TestAttributeZ::type()}) == (*(actual._analyzers.begin()))->features()));
          CHECK((false == !actual._analyzers.begin()->get()));
        } else if ("some" == fieldOverride.key()) {
          CHECK((true == actual._fields.empty())); // not inherited
          CHECK((true == actual._includeAllFields)); // inherited
          CHECK((true == actual._trackListPositions));
          CHECK((arangodb::iresearch::ValueStorage::ID == actual._storeValues));
          CHECK((2U == actual._analyzers.size()));
          auto itr = actual._analyzers.begin();
          CHECK((*itr));
          /* FIXME TODO uncomment once emplace(...) and all tests are updated
          CHECK(("testVocbase::empty" == (*itr)->name()));
          */
          CHECK((irs::flags({TestAttributeZ::type()}) == (*itr)->features()));
          CHECK((false == !itr->get()));
          ++itr;
          CHECK((*itr));
          CHECK(("identity" == (*itr)->name()));
          CHECK((irs::flags({irs::norm::type(), irs::frequency::type()}) == (*itr)->features()));
          CHECK((false == !itr->get()));
        } else if ("none" == fieldOverride.key()) {
          CHECK((true == actual._fields.empty())); // not inherited
          CHECK((true == actual._includeAllFields)); // inherited
          CHECK((true == actual._trackListPositions)); // inherited
          CHECK((arangodb::iresearch::ValueStorage::FULL == actual._storeValues));
          auto itr = actual._analyzers.begin();
          CHECK((*itr));
          /* FIXME TODO uncomment once emplace(...) and all tests are updated
          CHECK(("testVocbase::empty" == (*itr)->name()));
          */
          CHECK((irs::flags({TestAttributeZ::type()}) == (*itr)->features()));
          CHECK((false == !itr->get()));
          ++itr;
          CHECK((*itr));
          CHECK(("identity" == (*itr)->name()));
          CHECK((irs::flags({irs::norm::type(), irs::frequency::type()}) == (*itr)->features()));
          CHECK((false == !itr->get()));
        }
      }
    }

    CHECK((true == expectedOverrides.empty()));
    CHECK((true == expectedFields.empty()));
    CHECK((true == meta._includeAllFields));
    CHECK((true == meta._trackListPositions));
    CHECK((arangodb::iresearch::ValueStorage::FULL == meta._storeValues));
    auto itr = meta._analyzers.begin();
    CHECK((*itr));
    /* FIXME TODO uncomment once emplace(...) and all tests are updated
    CHECK(("testVocbase::empty" == (*itr)->name()));
    */
    CHECK((irs::flags({TestAttributeZ::type()}) == (*itr)->features()));
    CHECK((false == !itr->get()));
    ++itr;
    CHECK((*itr));
    CHECK(("identity" == (*itr)->name()));
    CHECK((irs::flags({irs::norm::type(), irs::frequency::type()}) == (*itr)->features()));
    CHECK((false == !itr->get()));
  }
}

SECTION("test_writeDefaults") {
  // without active vobcase (not fullAnalyzerDefinition)
  {
    arangodb::iresearch::IResearchLinkMeta meta;
    arangodb::velocypack::Builder builder;
    arangodb::velocypack::Slice tmpSlice;

    builder.openObject();
    CHECK((true == meta.json(builder, false)));
    builder.close();

    auto slice = builder.slice();

    CHECK((5U == slice.length()));
    tmpSlice = slice.get("fields");
    CHECK((true == tmpSlice.isObject() && 0 == tmpSlice.length()));
    tmpSlice = slice.get("includeAllFields");
    CHECK((true == tmpSlice.isBool() && false == tmpSlice.getBool()));
    tmpSlice = slice.get("trackListPositions");
    CHECK((true == tmpSlice.isBool() && false == tmpSlice.getBool()));
    tmpSlice = slice.get("storeValues");
    CHECK((true == tmpSlice.isString() && std::string("none") == tmpSlice.copyString()));
    tmpSlice = slice.get("analyzers");
    CHECK((
      true ==
      tmpSlice.isArray() &&
      1 == tmpSlice.length() &&
      tmpSlice.at(0).isString() &&
      std::string("identity") == tmpSlice.at(0).copyString()
    ));
  }

  // without active vobcase (with fullAnalyzerDefinition)
  {
    arangodb::iresearch::IResearchLinkMeta meta;
    arangodb::velocypack::Builder builder;
    arangodb::velocypack::Slice tmpSlice;

    builder.openObject();
    CHECK((true == meta.json(builder, true)));
    builder.close();

    auto slice = builder.slice();

    CHECK((5U == slice.length()));
    tmpSlice = slice.get("fields");
    CHECK((true == tmpSlice.isObject() && 0 == tmpSlice.length()));
    tmpSlice = slice.get("includeAllFields");
    CHECK((true == tmpSlice.isBool() && false == tmpSlice.getBool()));
    tmpSlice = slice.get("trackListPositions");
    CHECK((true == tmpSlice.isBool() && false == tmpSlice.getBool()));
    tmpSlice = slice.get("storeValues");
    CHECK((true == tmpSlice.isString() && std::string("none") == tmpSlice.copyString()));
    tmpSlice = slice.get("analyzers");
    CHECK((
      true == tmpSlice.isArray()
      && 1 == tmpSlice.length()
      && tmpSlice.at(0).isObject()
      && tmpSlice.at(0).get("name").isString() && std::string("identity") == tmpSlice.at(0).get("name").copyString()
      && tmpSlice.at(0).get("type").isString() && std::string("identity") == tmpSlice.at(0).get("type").copyString()
      && tmpSlice.at(0).get("properties").isString() && std::string("") == tmpSlice.at(0).get("properties").copyString()
      && tmpSlice.at(0).get("features").isArray() && 2 == tmpSlice.at(0).get("features").length() // frequency+norm
    ));
  }

  // with active vocbase (not fullAnalyzerDefinition)
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    arangodb::iresearch::IResearchLinkMeta meta;
    arangodb::velocypack::Builder builder;
    arangodb::velocypack::Slice tmpSlice;

    builder.openObject();
    CHECK((true == meta.json(builder, false, nullptr, &vocbase)));
    builder.close();

    auto slice = builder.slice();

    CHECK((5U == slice.length()));
    tmpSlice = slice.get("fields");
    CHECK((true == tmpSlice.isObject() && 0 == tmpSlice.length()));
    tmpSlice = slice.get("includeAllFields");
    CHECK((true == tmpSlice.isBool() && false == tmpSlice.getBool()));
    tmpSlice = slice.get("trackListPositions");
    CHECK((true == tmpSlice.isBool() && false == tmpSlice.getBool()));
    tmpSlice = slice.get("storeValues");
    CHECK((true == tmpSlice.isString() && std::string("none") == tmpSlice.copyString()));
    tmpSlice = slice.get("analyzers");
    CHECK((
      true ==
      tmpSlice.isArray() &&
      1 == tmpSlice.length() &&
      tmpSlice.at(0).isString() &&
      std::string("identity") == tmpSlice.at(0).copyString()
    ));
  }

  // with active vocbase (with fullAnalyzerDefinition)
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    arangodb::iresearch::IResearchLinkMeta meta;
    arangodb::velocypack::Builder builder;
    arangodb::velocypack::Slice tmpSlice;

    builder.openObject();
    CHECK((true == meta.json(builder, true, nullptr, &vocbase)));
    builder.close();

    auto slice = builder.slice();

    CHECK((5U == slice.length()));
    tmpSlice = slice.get("fields");
    CHECK((true == tmpSlice.isObject() && 0 == tmpSlice.length()));
    tmpSlice = slice.get("includeAllFields");
    CHECK((true == tmpSlice.isBool() && false == tmpSlice.getBool()));
    tmpSlice = slice.get("trackListPositions");
    CHECK((true == tmpSlice.isBool() && false == tmpSlice.getBool()));
    tmpSlice = slice.get("storeValues");
    CHECK((true == tmpSlice.isString() && std::string("none") == tmpSlice.copyString()));
    tmpSlice = slice.get("analyzers");
    CHECK((
      true == tmpSlice.isArray()
      && 1 == tmpSlice.length()
      && tmpSlice.at(0).isObject()
      && tmpSlice.at(0).get("name").isString() && std::string("identity") == tmpSlice.at(0).get("name").copyString()
      && tmpSlice.at(0).get("type").isString() && std::string("identity") == tmpSlice.at(0).get("type").copyString()
      && tmpSlice.at(0).get("properties").isString() && std::string("") == tmpSlice.at(0).get("properties").copyString()
      && tmpSlice.at(0).get("features").isArray() && 2 == tmpSlice.at(0).get("features").length() // frequency+norm
    ));
  }
}

SECTION("test_writeCustomizedValues") {
  arangodb::iresearch::IResearchAnalyzerFeature analyzers(s.server);
  arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult emplaceResult;
  arangodb::iresearch::IResearchLinkMeta meta;

  analyzers.emplace(emplaceResult, "empty", "empty", "en", { irs::attribute::type_id::get("position") });

  meta._includeAllFields = true;
  meta._trackListPositions = true;
  meta._storeValues = arangodb::iresearch::ValueStorage::FULL;
  meta._analyzers.clear();
  meta._analyzers.emplace_back(analyzers.get("identity"));
  meta._analyzers.emplace_back(analyzers.get("empty"));
  meta._fields["a"] = meta; // copy from meta
  meta._fields["a"]->_fields.clear(); // do not inherit fields to match jSon inheritance
  meta._fields["b"] = meta; // copy from meta
  meta._fields["b"]->_fields.clear(); // do not inherit fields to match jSon inheritance
  meta._fields["c"] = meta; // copy from meta
  meta._fields["c"]->_fields.clear(); // do not inherit fields to match jSon inheritance
  meta._fields["c"]->_fields["default"]; // default values
  meta._fields["c"]->_fields["all"]; // will override values below
  meta._fields["c"]->_fields["some"] = meta._fields["c"]; // initialize with parent, override below
  meta._fields["c"]->_fields["none"] = meta._fields["c"]; // initialize with parent

  auto& overrideAll = *(meta._fields["c"]->_fields["all"]);
  auto& overrideSome = *(meta._fields["c"]->_fields["some"]);
  auto& overrideNone = *(meta._fields["c"]->_fields["none"]);

  overrideAll._fields.clear(); // do not inherit fields to match jSon inheritance
  overrideAll._fields["x"] = arangodb::iresearch::IResearchLinkMeta();
  overrideAll._fields["y"] = arangodb::iresearch::IResearchLinkMeta();
  overrideAll._includeAllFields = false;
  overrideAll._trackListPositions = false;
  overrideAll._storeValues = arangodb::iresearch::ValueStorage::NONE;
  overrideAll._analyzers.clear();
  overrideAll._analyzers.emplace_back(analyzers.get("empty"));
  overrideSome._fields.clear(); // do not inherit fields to match jSon inheritance
  overrideSome._trackListPositions = false;
  overrideSome._storeValues = arangodb::iresearch::ValueStorage::ID;
  overrideNone._fields.clear(); // do not inherit fields to match jSon inheritance

  // without active vobcase (not fullAnalyzerDefinition)
  {
    std::unordered_set<std::string> expectedFields = { "a", "b", "c" };
    std::unordered_set<std::string> expectedOverrides = { "default", "all", "some", "none" };
    std::unordered_set<std::string> expectedAnalyzers = { "empty", "identity" };
    arangodb::velocypack::Builder builder;
    arangodb::velocypack::Slice tmpSlice;

    builder.openObject();
    CHECK((true == meta.json(builder, false)));
    builder.close();

    auto slice = builder.slice();

    CHECK((5U == slice.length()));
    tmpSlice = slice.get("fields");
    CHECK((true == tmpSlice.isObject() && 3 == tmpSlice.length()));

    for (arangodb::velocypack::ObjectIterator itr(tmpSlice); itr.valid(); ++itr) {
      auto key = itr.key();
      auto value = itr.value();
      CHECK((true == key.isString() && 1 == expectedFields.erase(key.copyString())));
      CHECK((true == value.isObject()));

      if (!value.hasKey("fields")) {
        continue;
      }

      tmpSlice = value.get("fields");

      for (arangodb::velocypack::ObjectIterator overrideItr(tmpSlice); overrideItr.valid(); ++overrideItr) {
        auto fieldOverride = overrideItr.key();
        auto sliceOverride = overrideItr.value();
        CHECK((true == fieldOverride.isString() && sliceOverride.isObject()));
        CHECK((1U == expectedOverrides.erase(fieldOverride.copyString())));

        if ("default" == fieldOverride.copyString()) {
          CHECK((4U == sliceOverride.length()));
          tmpSlice = sliceOverride.get("includeAllFields");
          CHECK((true == (false == tmpSlice.getBool())));
          tmpSlice = sliceOverride.get("trackListPositions");
          CHECK((true == (false == tmpSlice.getBool())));
          tmpSlice = sliceOverride.get("storeValues");
          CHECK((true == tmpSlice.isString() && std::string("none") == tmpSlice.copyString()));
          tmpSlice = sliceOverride.get("analyzers");
          CHECK((
            true ==
            tmpSlice.isArray() &&
            1 == tmpSlice.length() &&
            tmpSlice.at(0).isString() &&
            std::string("identity") == tmpSlice.at(0).copyString()
          ));
        } else if ("all" == fieldOverride.copyString()) {
          std::unordered_set<std::string> expectedFields = { "x", "y" };
          CHECK((5U == sliceOverride.length()));
          tmpSlice = sliceOverride.get("fields");
          CHECK((true == tmpSlice.isObject() && 2 == tmpSlice.length()));
          for (arangodb::velocypack::ObjectIterator overrideFieldItr(tmpSlice); overrideFieldItr.valid(); ++overrideFieldItr) {
            CHECK((true == overrideFieldItr.key().isString() && 1 == expectedFields.erase(overrideFieldItr.key().copyString())));
          }
          CHECK((true == expectedFields.empty()));
          tmpSlice = sliceOverride.get("includeAllFields");
          CHECK((true == tmpSlice.isBool() && false == tmpSlice.getBool()));
          tmpSlice = sliceOverride.get("trackListPositions");
          CHECK((true == tmpSlice.isBool() && false == tmpSlice.getBool()));
          tmpSlice = sliceOverride.get("storeValues");
          CHECK((true == tmpSlice.isString() && std::string("none") == tmpSlice.copyString()));
          tmpSlice = sliceOverride.get("analyzers");
          CHECK((
            true ==
            tmpSlice.isArray() &&
            1 == tmpSlice.length() &&
            tmpSlice.at(0).isString() &&
            std::string("empty") == tmpSlice.at(0).copyString()
          ));
        } else if ("some" == fieldOverride.copyString()) {
          CHECK((2U == sliceOverride.length()));
          tmpSlice = sliceOverride.get("trackListPositions");
          CHECK((true == tmpSlice.isBool() && false == tmpSlice.getBool()));
          tmpSlice = sliceOverride.get("storeValues");
          CHECK((true == tmpSlice.isString() && std::string("id") == tmpSlice.copyString()));
        } else if ("none" == fieldOverride.copyString()) {
          CHECK((0U == sliceOverride.length()));
        }
      }
    }

    CHECK((true == expectedOverrides.empty()));
    CHECK((true == expectedFields.empty()));
    tmpSlice = slice.get("includeAllFields");
    CHECK((true == tmpSlice.isBool() && true == tmpSlice.getBool()));
    tmpSlice = slice.get("trackListPositions");
    CHECK((true == tmpSlice.isBool() && true == tmpSlice.getBool()));
    tmpSlice = slice.get("storeValues");
    CHECK((true == tmpSlice.isString() && std::string("full") == tmpSlice.copyString()));
    tmpSlice = slice.get("analyzers");
    CHECK((true == tmpSlice.isArray() && 2 == tmpSlice.length()));

    for (arangodb::velocypack::ArrayIterator analyzersItr(tmpSlice); analyzersItr.valid(); ++analyzersItr) {
      auto key = *analyzersItr;
      CHECK((true == key.isString() && 1 == expectedAnalyzers.erase(key.copyString())));
    }

    CHECK((true == expectedAnalyzers.empty()));
  }

  // without active vobcase (with fullAnalyzerDefinition)
  {
    std::unordered_set<std::string> expectedFields = { "a", "b", "c" };
    std::unordered_set<std::string> expectedOverrides = { "default", "all", "some", "none" };
    std::set<std::pair<std::string, std::string>> expectedAnalyzers = {
      { "empty", "en" },
      { "identity", "" },
    };
    arangodb::velocypack::Builder builder;
    arangodb::velocypack::Slice tmpSlice;

    builder.openObject();
    CHECK((true == meta.json(builder, true)));
    builder.close();

    auto slice = builder.slice();

    CHECK((5U == slice.length()));
    tmpSlice = slice.get("fields");
    CHECK((true == tmpSlice.isObject() && 3 == tmpSlice.length()));

    for (arangodb::velocypack::ObjectIterator itr(tmpSlice); itr.valid(); ++itr) {
      auto key = itr.key();
      auto value = itr.value();
      CHECK((true == key.isString() && 1 == expectedFields.erase(key.copyString())));
      CHECK((true == value.isObject()));

      if (!value.hasKey("fields")) {
        continue;
      }

      tmpSlice = value.get("fields");

      for (arangodb::velocypack::ObjectIterator overrideItr(tmpSlice); overrideItr.valid(); ++overrideItr) {
        auto fieldOverride = overrideItr.key();
        auto sliceOverride = overrideItr.value();
        CHECK((true == fieldOverride.isString() && sliceOverride.isObject()));
        CHECK((1U == expectedOverrides.erase(fieldOverride.copyString())));

        if ("default" == fieldOverride.copyString()) {
          CHECK((4U == sliceOverride.length()));
          tmpSlice = sliceOverride.get("includeAllFields");
          CHECK((true == (false == tmpSlice.getBool())));
          tmpSlice = sliceOverride.get("trackListPositions");
          CHECK((true == (false == tmpSlice.getBool())));
          tmpSlice = sliceOverride.get("storeValues");
          CHECK((true == tmpSlice.isString() && std::string("none") == tmpSlice.copyString()));
          tmpSlice = sliceOverride.get("analyzers");
          CHECK((
            true == tmpSlice.isArray()
            && 1 == tmpSlice.length()
            && tmpSlice.at(0).isObject()
            && tmpSlice.at(0).get("name").isString() && std::string("identity") == tmpSlice.at(0).get("name").copyString()
            && tmpSlice.at(0).get("type").isString() && std::string("identity") == tmpSlice.at(0).get("type").copyString()
            && tmpSlice.at(0).get("properties").isString() && std::string("") == tmpSlice.at(0).get("properties").copyString()
            && tmpSlice.at(0).get("features").isArray() && 2 == tmpSlice.at(0).get("features").length() // frequency+norm
          ));
        } else if ("all" == fieldOverride.copyString()) {
          std::unordered_set<std::string> expectedFields = { "x", "y" };
          CHECK((5U == sliceOverride.length()));
          tmpSlice = sliceOverride.get("fields");
          CHECK((true == tmpSlice.isObject() && 2 == tmpSlice.length()));
          for (arangodb::velocypack::ObjectIterator overrideFieldItr(tmpSlice); overrideFieldItr.valid(); ++overrideFieldItr) {
            CHECK((true == overrideFieldItr.key().isString() && 1 == expectedFields.erase(overrideFieldItr.key().copyString())));
          }
          CHECK((true == expectedFields.empty()));
          tmpSlice = sliceOverride.get("includeAllFields");
          CHECK((true == tmpSlice.isBool() && false == tmpSlice.getBool()));
          tmpSlice = sliceOverride.get("trackListPositions");
          CHECK((true == tmpSlice.isBool() && false == tmpSlice.getBool()));
          tmpSlice = sliceOverride.get("storeValues");
          CHECK((true == tmpSlice.isString() && std::string("none") == tmpSlice.copyString()));
          tmpSlice = sliceOverride.get("analyzers");
          CHECK((
            true == tmpSlice.isArray()
            && 1 == tmpSlice.length()
            && tmpSlice.at(0).isObject()
            && tmpSlice.at(0).get("name").isString() && std::string("empty") == tmpSlice.at(0).get("name").copyString()
            && tmpSlice.at(0).get("type").isString() && std::string("empty") == tmpSlice.at(0).get("type").copyString()
            && tmpSlice.at(0).get("properties").isString() && std::string("en") == tmpSlice.at(0).get("properties").copyString()
            && tmpSlice.at(0).get("features").isArray() && 1 == tmpSlice.at(0).get("features").length()
            && tmpSlice.at(0).get("features").at(0).isString() && std::string("position") == tmpSlice.at(0).get("features").at(0).copyString()
          ));
        } else if ("some" == fieldOverride.copyString()) {
          CHECK((2U == sliceOverride.length()));
          tmpSlice = sliceOverride.get("trackListPositions");
          CHECK((true == tmpSlice.isBool() && false == tmpSlice.getBool()));
          tmpSlice = sliceOverride.get("storeValues");
          CHECK((true == tmpSlice.isString() && std::string("id") == tmpSlice.copyString()));
        } else if ("none" == fieldOverride.copyString()) {
          CHECK((0U == sliceOverride.length()));
        }
      }
    }

    CHECK((true == expectedOverrides.empty()));
    CHECK((true == expectedFields.empty()));
    tmpSlice = slice.get("includeAllFields");
    CHECK((true == tmpSlice.isBool() && true == tmpSlice.getBool()));
    tmpSlice = slice.get("trackListPositions");
    CHECK((true == tmpSlice.isBool() && true == tmpSlice.getBool()));
    tmpSlice = slice.get("storeValues");
    CHECK((true == tmpSlice.isString() && std::string("full") == tmpSlice.copyString()));
    tmpSlice = slice.get("analyzers");
    CHECK((true == tmpSlice.isArray() && 2 == tmpSlice.length()));

    for (arangodb::velocypack::ArrayIterator analyzersItr(tmpSlice); analyzersItr.valid(); ++analyzersItr) {
      auto value = *analyzersItr;
      CHECK((
        true == value.isObject()
        && value.hasKey("name") && value.get("name").isString()
        && value.hasKey("type") && value.get("type").isString()
        && value.hasKey("properties") && value.get("properties").isString()
        && value.hasKey("features") && value.get("features").isArray() && (1 == value.get("features").length() || 2 == value.get("features").length()) // empty/identity 1/2
        && 1 == expectedAnalyzers.erase(std::make_pair(value.get("name").copyString(), value.get("properties").copyString()))
     ));
    }

    CHECK((true == expectedAnalyzers.empty()));
  }

  // with active vocbase (no fullAnalyzerDefinition)
  {
    std::unordered_set<std::string> expectedFields = { "a", "b", "c" };
    std::unordered_set<std::string> expectedOverrides = { "default", "all", "some", "none" };
    std::unordered_set<std::string> expectedAnalyzers = { "empty", "identity" };
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    arangodb::velocypack::Builder builder;
    arangodb::velocypack::Slice tmpSlice;

    builder.openObject();
    CHECK((true == meta.json(builder, false, nullptr, &vocbase)));
    builder.close();

    auto slice = builder.slice();

    CHECK((5U == slice.length()));
    tmpSlice = slice.get("fields");
    CHECK((true == tmpSlice.isObject() && 3 == tmpSlice.length()));

    for (arangodb::velocypack::ObjectIterator itr(tmpSlice); itr.valid(); ++itr) {
      auto key = itr.key();
      auto value = itr.value();
      CHECK((true == key.isString() && 1 == expectedFields.erase(key.copyString())));
      CHECK((true == value.isObject()));

      if (!value.hasKey("fields")) {
        continue;
      }

      tmpSlice = value.get("fields");

      for (arangodb::velocypack::ObjectIterator overrideItr(tmpSlice); overrideItr.valid(); ++overrideItr) {
        auto fieldOverride = overrideItr.key();
        auto sliceOverride = overrideItr.value();
        CHECK((true == fieldOverride.isString() && sliceOverride.isObject()));
        CHECK((1U == expectedOverrides.erase(fieldOverride.copyString())));

        if ("default" == fieldOverride.copyString()) {
          CHECK((4U == sliceOverride.length()));
          tmpSlice = sliceOverride.get("includeAllFields");
          CHECK((true == (false == tmpSlice.getBool())));
          tmpSlice = sliceOverride.get("trackListPositions");
          CHECK((true == (false == tmpSlice.getBool())));
          tmpSlice = sliceOverride.get("storeValues");
          CHECK((true == tmpSlice.isString() && std::string("none") == tmpSlice.copyString()));
          tmpSlice = sliceOverride.get("analyzers");
          CHECK((
            true ==
            tmpSlice.isArray() &&
            1 == tmpSlice.length() &&
            tmpSlice.at(0).isString() &&
            std::string("identity") == tmpSlice.at(0).copyString()
          ));
        } else if ("all" == fieldOverride.copyString()) {
          std::unordered_set<std::string> expectedFields = { "x", "y" };
          CHECK((5U == sliceOverride.length()));
          tmpSlice = sliceOverride.get("fields");
          CHECK((true == tmpSlice.isObject() && 2 == tmpSlice.length()));
          for (arangodb::velocypack::ObjectIterator overrideFieldItr(tmpSlice); overrideFieldItr.valid(); ++overrideFieldItr) {
            CHECK((true == overrideFieldItr.key().isString() && 1 == expectedFields.erase(overrideFieldItr.key().copyString())));
          }
          CHECK((true == expectedFields.empty()));
          tmpSlice = sliceOverride.get("includeAllFields");
          CHECK((true == tmpSlice.isBool() && false == tmpSlice.getBool()));
          tmpSlice = sliceOverride.get("trackListPositions");
          CHECK((true == tmpSlice.isBool() && false == tmpSlice.getBool()));
          tmpSlice = sliceOverride.get("storeValues");
          CHECK((true == tmpSlice.isString() && std::string("none") == tmpSlice.copyString()));
          tmpSlice = sliceOverride.get("analyzers");
          CHECK((
            true ==
            tmpSlice.isArray() &&
            1 == tmpSlice.length() &&
            tmpSlice.at(0).isString() &&
            std::string("empty") == tmpSlice.at(0).copyString()
          ));
        } else if ("some" == fieldOverride.copyString()) {
          CHECK((2U == sliceOverride.length()));
          tmpSlice = sliceOverride.get("trackListPositions");
          CHECK((true == tmpSlice.isBool() && false == tmpSlice.getBool()));
          tmpSlice = sliceOverride.get("storeValues");
          CHECK((true == tmpSlice.isString() && std::string("id") == tmpSlice.copyString()));
        } else if ("none" == fieldOverride.copyString()) {
          CHECK((0U == sliceOverride.length()));
        }
      }
    }

    CHECK((true == expectedOverrides.empty()));
    CHECK((true == expectedFields.empty()));
    tmpSlice = slice.get("includeAllFields");
    CHECK((true == tmpSlice.isBool() && true == tmpSlice.getBool()));
    tmpSlice = slice.get("trackListPositions");
    CHECK((true == tmpSlice.isBool() && true == tmpSlice.getBool()));
    tmpSlice = slice.get("storeValues");
    CHECK((true == tmpSlice.isString() && std::string("full") == tmpSlice.copyString()));
    tmpSlice = slice.get("analyzers");
    CHECK((true == tmpSlice.isArray() && 2 == tmpSlice.length()));

    for (arangodb::velocypack::ArrayIterator analyzersItr(tmpSlice); analyzersItr.valid(); ++analyzersItr) {
      auto key = *analyzersItr;
      CHECK((true == key.isString() && 1 == expectedAnalyzers.erase(key.copyString())));
    }

    CHECK((true == expectedAnalyzers.empty()));
  }

  // with active vocbase (with fullAnalyzerDefinition)
  {
    std::unordered_set<std::string> expectedFields = { "a", "b", "c" };
    std::unordered_set<std::string> expectedOverrides = { "default", "all", "some", "none" };
    std::set<std::pair<std::string, std::string>> expectedAnalyzers = {
      { "empty", "en" },
      { "identity", "" },
    };
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    arangodb::velocypack::Builder builder;
    arangodb::velocypack::Slice tmpSlice;

    builder.openObject();
    CHECK((true == meta.json(builder, true, nullptr, &vocbase)));
    builder.close();

    auto slice = builder.slice();

    CHECK((5U == slice.length()));
    tmpSlice = slice.get("fields");
    CHECK((true == tmpSlice.isObject() && 3 == tmpSlice.length()));

    for (arangodb::velocypack::ObjectIterator itr(tmpSlice); itr.valid(); ++itr) {
      auto key = itr.key();
      auto value = itr.value();
      CHECK((true == key.isString() && 1 == expectedFields.erase(key.copyString())));
      CHECK((true == value.isObject()));

      if (!value.hasKey("fields")) {
        continue;
      }

      tmpSlice = value.get("fields");

      for (arangodb::velocypack::ObjectIterator overrideItr(tmpSlice); overrideItr.valid(); ++overrideItr) {
        auto fieldOverride = overrideItr.key();
        auto sliceOverride = overrideItr.value();
        CHECK((true == fieldOverride.isString() && sliceOverride.isObject()));
        CHECK((1U == expectedOverrides.erase(fieldOverride.copyString())));

        if ("default" == fieldOverride.copyString()) {
          CHECK((4U == sliceOverride.length()));
          tmpSlice = sliceOverride.get("includeAllFields");
          CHECK((true == (false == tmpSlice.getBool())));
          tmpSlice = sliceOverride.get("trackListPositions");
          CHECK((true == (false == tmpSlice.getBool())));
          tmpSlice = sliceOverride.get("storeValues");
          CHECK((true == tmpSlice.isString() && std::string("none") == tmpSlice.copyString()));
          tmpSlice = sliceOverride.get("analyzers");
          CHECK((
            true == tmpSlice.isArray()
            && 1 == tmpSlice.length()
            && tmpSlice.at(0).isObject()
            && tmpSlice.at(0).get("name").isString() && std::string("identity") == tmpSlice.at(0).get("name").copyString()
            && tmpSlice.at(0).get("type").isString() && std::string("identity") == tmpSlice.at(0).get("type").copyString()
            && tmpSlice.at(0).get("properties").isString() && std::string("") == tmpSlice.at(0).get("properties").copyString()
            && tmpSlice.at(0).get("features").isArray() && 2 == tmpSlice.at(0).get("features").length()  // frequency+norm
          ));
        } else if ("all" == fieldOverride.copyString()) {
          std::unordered_set<std::string> expectedFields = { "x", "y" };
          CHECK((5U == sliceOverride.length()));
          tmpSlice = sliceOverride.get("fields");
          CHECK((true == tmpSlice.isObject() && 2 == tmpSlice.length()));
          for (arangodb::velocypack::ObjectIterator overrideFieldItr(tmpSlice); overrideFieldItr.valid(); ++overrideFieldItr) {
            CHECK((true == overrideFieldItr.key().isString() && 1 == expectedFields.erase(overrideFieldItr.key().copyString())));
          }
          CHECK((true == expectedFields.empty()));
          tmpSlice = sliceOverride.get("includeAllFields");
          CHECK((true == tmpSlice.isBool() && false == tmpSlice.getBool()));
          tmpSlice = sliceOverride.get("trackListPositions");
          CHECK((true == tmpSlice.isBool() && false == tmpSlice.getBool()));
          tmpSlice = sliceOverride.get("storeValues");
          CHECK((true == tmpSlice.isString() && std::string("none") == tmpSlice.copyString()));
          tmpSlice = sliceOverride.get("analyzers");
          CHECK((
            true == tmpSlice.isArray()
            && 1 == tmpSlice.length()
            && tmpSlice.at(0).isObject()
            && tmpSlice.at(0).get("name").isString() && std::string("empty") == tmpSlice.at(0).get("name").copyString()
            && tmpSlice.at(0).get("type").isString() && std::string("empty") == tmpSlice.at(0).get("type").copyString()
            && tmpSlice.at(0).get("properties").isString() && std::string("en") == tmpSlice.at(0).get("properties").copyString()
            && tmpSlice.at(0).get("features").isArray() && 1 == tmpSlice.at(0).get("features").length()
            && tmpSlice.at(0).get("features").at(0).isString() && std::string("position") == tmpSlice.at(0).get("features").at(0).copyString()
          ));
        } else if ("some" == fieldOverride.copyString()) {
          CHECK((2U == sliceOverride.length()));
          tmpSlice = sliceOverride.get("trackListPositions");
          CHECK((true == tmpSlice.isBool() && false == tmpSlice.getBool()));
          tmpSlice = sliceOverride.get("storeValues");
          CHECK((true == tmpSlice.isString() && std::string("id") == tmpSlice.copyString()));
        } else if ("none" == fieldOverride.copyString()) {
          CHECK((0U == sliceOverride.length()));
        }
      }
    }

    CHECK((true == expectedOverrides.empty()));
    CHECK((true == expectedFields.empty()));
    tmpSlice = slice.get("includeAllFields");
    CHECK((true == tmpSlice.isBool() && true == tmpSlice.getBool()));
    tmpSlice = slice.get("trackListPositions");
    CHECK((true == tmpSlice.isBool() && true == tmpSlice.getBool()));
    tmpSlice = slice.get("storeValues");
    CHECK((true == tmpSlice.isString() && std::string("full") == tmpSlice.copyString()));
    tmpSlice = slice.get("analyzers");
    CHECK((true == tmpSlice.isArray() && 2 == tmpSlice.length()));

    for (arangodb::velocypack::ArrayIterator analyzersItr(tmpSlice); analyzersItr.valid(); ++analyzersItr) {
      auto value = *analyzersItr;
      CHECK((
        true == value.isObject()
        && value.hasKey("name") && value.get("name").isString()
        && value.hasKey("type") && value.get("type").isString()
        && value.hasKey("properties") && value.get("properties").isString()
        && value.hasKey("features") && value.get("features").isArray() && (1 == value.get("features").length() || 2 == value.get("features").length()) // empty/identity 1/2
        && 1 == expectedAnalyzers.erase(std::make_pair(value.get("name").copyString(), value.get("properties").copyString()))
     ));
    }

    CHECK((true == expectedAnalyzers.empty()));
  }
}

SECTION("test_readMaskAll") {
  arangodb::iresearch::IResearchLinkMeta meta;
  arangodb::iresearch::IResearchLinkMeta::Mask mask;
  std::string tmpString;

  auto json = arangodb::velocypack::Parser::fromJson("{ \
    \"fields\": { \"a\": {} }, \
    \"includeAllFields\": true, \
    \"trackListPositions\": true, \
    \"storeValues\": \"full\", \
    \"analyzers\": [] \
  }");
  CHECK(true == meta.init(json->slice(), tmpString, nullptr, arangodb::iresearch::IResearchLinkMeta::DEFAULT(), &mask));
  CHECK(true == mask._fields);
  CHECK(true == mask._includeAllFields);
  CHECK(true == mask._trackListPositions);
  CHECK((true == mask._storeValues));
  CHECK(true == mask._analyzers);
}

SECTION("test_readMaskNone") {
  arangodb::iresearch::IResearchLinkMeta meta;
  arangodb::iresearch::IResearchLinkMeta::Mask mask;
  std::string tmpString;

  auto json = arangodb::velocypack::Parser::fromJson("{}");
  CHECK(true == meta.init(json->slice(), tmpString, nullptr, arangodb::iresearch::IResearchLinkMeta::DEFAULT(), &mask));
  CHECK(false == mask._fields);
  CHECK(false == mask._includeAllFields);
  CHECK(false == mask._trackListPositions);
  CHECK((false == mask._storeValues));
  CHECK(false == mask._analyzers);
}

SECTION("test_writeMaskAll") {
  // not fullAnalyzerDefinition
  {
    arangodb::iresearch::IResearchLinkMeta meta;
    arangodb::iresearch::IResearchLinkMeta::Mask mask(true);
    arangodb::velocypack::Builder builder;

    builder.openObject();
    CHECK((true == meta.json(builder, false, nullptr, nullptr, &mask)));
    builder.close();

    auto slice = builder.slice();

    CHECK((5U == slice.length()));
    CHECK(true == slice.hasKey("fields"));
    CHECK(true == slice.hasKey("includeAllFields"));
    CHECK(true == slice.hasKey("trackListPositions"));
    CHECK(true == slice.hasKey("storeValues"));
    CHECK(true == slice.hasKey("analyzers"));
  }

  // with fullAnalyzerDefinition
  {
    arangodb::iresearch::IResearchLinkMeta meta;
    arangodb::iresearch::IResearchLinkMeta::Mask mask(true);
    arangodb::velocypack::Builder builder;

    builder.openObject();
    CHECK((true == meta.json(builder, true, nullptr, nullptr, &mask)));
    builder.close();

    auto slice = builder.slice();

    CHECK((5U == slice.length()));
    CHECK(true == slice.hasKey("fields"));
    CHECK(true == slice.hasKey("includeAllFields"));
    CHECK(true == slice.hasKey("trackListPositions"));
    CHECK(true == slice.hasKey("storeValues"));
    CHECK(true == slice.hasKey("analyzers"));
  }
}

SECTION("test_writeMaskNone") {
  // not fullAnalyzerDefinition
  {
    arangodb::iresearch::IResearchLinkMeta meta;
    arangodb::iresearch::IResearchLinkMeta::Mask mask(false);
    arangodb::velocypack::Builder builder;

    builder.openObject();
    CHECK((true == meta.json(builder, false, nullptr, nullptr, &mask)));
    builder.close();

    auto slice = builder.slice();

    CHECK(0U == slice.length());
  }

  // with fullAnalyzerDefinition
  {
    arangodb::iresearch::IResearchLinkMeta meta;
    arangodb::iresearch::IResearchLinkMeta::Mask mask(false);
    arangodb::velocypack::Builder builder;

    builder.openObject();
    CHECK((true == meta.json(builder, true, nullptr, nullptr, &mask)));
    builder.close();

    auto slice = builder.slice();

    CHECK(0U == slice.length());
  }
}

SECTION("test_readAnalyzerDefinitions") {
  // missing analyzer (name only)
  {
    auto json = arangodb::velocypack::Parser::fromJson("{ \
      \"analyzers\": [ \"empty1\" ] \
    }");
    arangodb::iresearch::IResearchLinkMeta meta;
    std::string errorField;
    CHECK((false == meta.init(json->slice(), errorField)));
    CHECK((std::string("analyzers=>empty1") == errorField));
  }

  // missing analyzer (name only) inRecovery
  {
    auto json = arangodb::velocypack::Parser::fromJson("{ \
      \"analyzers\": [ \"empty1\" ] \
    }");
    auto before = StorageEngineMock::inRecoveryResult;
    StorageEngineMock::inRecoveryResult = true;
    auto restore = irs::make_finally([&before]()->void { StorageEngineMock::inRecoveryResult = before; });
    arangodb::iresearch::IResearchLinkMeta meta;
    std::string errorField;
    CHECK((false == meta.init(json->slice(), errorField)));
    CHECK((std::string("analyzers=>empty1") == errorField));
  }

  // missing analyzer (full) no name (fail) required
  {
    auto json = arangodb::velocypack::Parser::fromJson("{ \
      \"analyzers\": [ { \"type\": \"empty\", \"properties\": \"ru\", \"features\": [ \"frequency\" ] } ] \
    }");
    arangodb::iresearch::IResearchLinkMeta meta;
    std::string errorField;
    CHECK((false == meta.init(json->slice(), errorField)));
    CHECK((std::string("analyzers=>[0]=>name") == errorField));
  }

  // missing analyzer (full) no type (fail) required
  {
    auto json = arangodb::velocypack::Parser::fromJson("{ \
      \"analyzers\": [ { \"name\": \"missing0\", \"properties\": \"ru\", \"features\": [ \"frequency\" ] } ] \
    }");
    arangodb::iresearch::IResearchLinkMeta meta;
    std::string errorField;
    CHECK((false == meta.init(json->slice(), errorField)));
    CHECK((std::string("analyzers=>[0]=>type") == errorField));
  }

  // missing analyzer (full) analyzer creation not allowed (fail)
  {
    auto json = arangodb::velocypack::Parser::fromJson("{ \
      \"analyzers\": [ { \"name\": \"missing0\", \"type\": \"empty\", \"properties\": \"ru\", \"features\": [ \"frequency\" ] } ] \
    }");
    arangodb::iresearch::IResearchLinkMeta meta;
    std::string errorField;
    CHECK((false == meta.init(json->slice(), errorField)));
    CHECK((std::string("analyzers=>[0]") == errorField));
  }

  // missing analyzer (full) single-server
  {
    auto json = arangodb::velocypack::Parser::fromJson("{ \
      \"analyzers\": [ { \"name\": \"missing0\", \"type\": \"empty\", \"properties\": \"ru\", \"features\": [ \"frequency\" ] } ] \
    }");
    arangodb::iresearch::IResearchLinkMeta meta;
    std::string errorField;
    CHECK((false == meta.init(json->slice(), errorField)));
    CHECK((std::string("analyzers=>[0]") == errorField));
  }

  // missing analyzer (full) coordinator
  {
    auto before = arangodb::ServerState::instance()->getRole();
    arangodb::ServerState::instance()->setRole(arangodb::ServerState::ROLE_COORDINATOR);
    auto restore = irs::make_finally([&before]()->void { arangodb::ServerState::instance()->setRole(before); });

    auto json = arangodb::velocypack::Parser::fromJson("{ \
      \"analyzers\": [ { \"name\": \"missing1\", \"type\": \"empty\", \"properties\": \"ru\", \"features\": [ \"frequency\" ] } ] \
    }");
    arangodb::iresearch::IResearchLinkMeta meta;
    std::string errorField;
    CHECK((false == meta.init(json->slice(), errorField)));
    CHECK((std::string("analyzers=>[0]") == errorField));
  }

  // missing analyzer (full) db-server
  {
    auto before = arangodb::ServerState::instance()->getRole();
    arangodb::ServerState::instance()->setRole(arangodb::ServerState::ROLE_DBSERVER);
    auto restore = irs::make_finally([&before]()->void { arangodb::ServerState::instance()->setRole(before); });

    auto json = arangodb::velocypack::Parser::fromJson("{ \
      \"analyzers\": [ { \"name\": \"missing2\", \"type\": \"empty\", \"properties\": \"ru\", \"features\": [ \"frequency\" ] } ] \
    }");
    arangodb::iresearch::IResearchLinkMeta meta;
    std::string errorField;
    CHECK((true == meta.init(json->slice(), errorField)));
    CHECK((1 == meta._analyzers.size()));
    CHECK((std::string("missing2") == meta._analyzers[0]->name()));
    CHECK((std::string("empty") == meta._analyzers[0]->type()));
    CHECK((std::string("ru") == meta._analyzers[0]->properties()));
    CHECK((1 == meta._analyzers[0]->features().size()));
    CHECK((true == meta._analyzers[0]->features().check(irs::frequency::type())));
  }

  // missing analyzer (full) inRecovery
  {
    auto json = arangodb::velocypack::Parser::fromJson("{ \
      \"analyzers\": [ { \"name\": \"missing3\", \"type\": \"empty\", \"properties\": \"ru\", \"features\": [ \"frequency\" ] } ] \
    }");
    auto before = StorageEngineMock::inRecoveryResult;
    StorageEngineMock::inRecoveryResult = true;
    auto restore = irs::make_finally([&before]()->void { StorageEngineMock::inRecoveryResult = before; });
    arangodb::iresearch::IResearchLinkMeta meta;
    std::string errorField;
    CHECK((false == meta.init(json->slice(), errorField)));
    CHECK((std::string("analyzers=>[0]") == errorField));
  }

  // existing analyzer (name only)
  {
    auto json = arangodb::velocypack::Parser::fromJson("{ \
      \"analyzers\": [ \"empty\" ] \
    }");
    arangodb::iresearch::IResearchLinkMeta meta;
    std::string errorField;
    CHECK((true == meta.init(json->slice(), errorField)));
    CHECK((1 == meta._analyzers.size()));
    CHECK((std::string("empty") == meta._analyzers[0]->name()));
    CHECK((std::string("empty") == meta._analyzers[0]->type()));
    CHECK((std::string("en") == meta._analyzers[0]->properties()));
    CHECK((1 == meta._analyzers[0]->features().size()));
    CHECK((true == meta._analyzers[0]->features().check(TestAttributeZ::type())));
  }

  // existing analyzer (name only) inRecovery
  {
    auto json = arangodb::velocypack::Parser::fromJson("{ \
      \"analyzers\": [ \"empty\" ] \
    }");
    auto before = StorageEngineMock::inRecoveryResult;
    StorageEngineMock::inRecoveryResult = true;
    auto restore = irs::make_finally([&before]()->void { StorageEngineMock::inRecoveryResult = before; });
    arangodb::iresearch::IResearchLinkMeta meta;
    std::string errorField;
    CHECK((true == meta.init(json->slice(), errorField)));
    CHECK((1 == meta._analyzers.size()));
    CHECK((std::string("empty") == meta._analyzers[0]->name()));
    CHECK((std::string("empty") == meta._analyzers[0]->type()));
    CHECK((std::string("en") == meta._analyzers[0]->properties()));
    CHECK((1 == meta._analyzers[0]->features().size()));
    CHECK((true == meta._analyzers[0]->features().check(TestAttributeZ::type())));
  }

  // existing analyzer (full) analyzer creation not allowed (passs)
  {
    auto json = arangodb::velocypack::Parser::fromJson("{ \
      \"analyzers\": [ { \"name\": \"empty\", \"type\": \"empty\", \"properties\": \"en\", \"features\": [ \"TestAttributeZ\" ] } ] \
    }");
    arangodb::iresearch::IResearchLinkMeta meta;
    std::string errorField;
    CHECK((true == meta.init(json->slice(), errorField)));
    CHECK((1 == meta._analyzers.size()));
    CHECK((std::string("empty") == meta._analyzers[0]->name()));
    CHECK((std::string("empty") == meta._analyzers[0]->type()));
    CHECK((std::string("en") == meta._analyzers[0]->properties()));
    CHECK((1 == meta._analyzers[0]->features().size()));
    CHECK((true == meta._analyzers[0]->features().check(TestAttributeZ::type())));
  }

  // existing analyzer (full)
  {
    auto json = arangodb::velocypack::Parser::fromJson("{ \
      \"analyzers\": [ { \"name\": \"empty\", \"type\": \"empty\", \"properties\": \"en\", \"features\": [ \"TestAttributeZ\" ] } ] \
    }");
    arangodb::iresearch::IResearchLinkMeta meta;
    std::string errorField;
    CHECK((true == meta.init(json->slice(), errorField)));
    CHECK((1 == meta._analyzers.size()));
    CHECK((std::string("empty") == meta._analyzers[0]->name()));
    CHECK((std::string("empty") == meta._analyzers[0]->type()));
    CHECK((std::string("en") == meta._analyzers[0]->properties()));
    CHECK((1 == meta._analyzers[0]->features().size()));
    CHECK((true == meta._analyzers[0]->features().check(TestAttributeZ::type())));
  }

  // existing analyzer (full) inRecovery
  {
    auto json = arangodb::velocypack::Parser::fromJson("{ \
      \"analyzers\": [ { \"name\": \"empty\", \"type\": \"empty\", \"properties\": \"en\", \"features\": [ \"TestAttributeZ\" ] } ] \
    }");
    auto before = StorageEngineMock::inRecoveryResult;
    StorageEngineMock::inRecoveryResult = true;
    auto restore = irs::make_finally([&before]()->void { StorageEngineMock::inRecoveryResult = before; });
    arangodb::iresearch::IResearchLinkMeta meta;
    std::string errorField;
    CHECK((true == meta.init(json->slice(), errorField)));
    CHECK((1 == meta._analyzers.size()));
    CHECK((std::string("empty") == meta._analyzers[0]->name()));
    CHECK((std::string("empty") == meta._analyzers[0]->type()));
    CHECK((std::string("en") == meta._analyzers[0]->properties()));
    CHECK((1 == meta._analyzers[0]->features().size()));
    CHECK((true == meta._analyzers[0]->features().check(TestAttributeZ::type())));
  }

  // existing analyzer (definition mismatch)
  {
    auto json = arangodb::velocypack::Parser::fromJson("{ \
      \"analyzers\": [ { \"name\": \"empty\", \"type\": \"empty\", \"properties\": \"ru\", \"features\": [ \"TestAttributeZ\" ] } ] \
    }");
    arangodb::iresearch::IResearchLinkMeta meta;
    std::string errorField;
    CHECK((false == meta.init(json->slice(), errorField)));
    CHECK((std::string("analyzers=>[0]") == errorField));
  }

  // existing analyzer (definition mismatch) inRecovery
  {
    auto json = arangodb::velocypack::Parser::fromJson("{ \
      \"analyzers\": [ { \"name\": \"empty\", \"type\": \"empty\", \"properties\": \"ru\", \"features\": [ \"TestAttributeZ\" ] } ] \
    }");
    auto before = StorageEngineMock::inRecoveryResult;
    StorageEngineMock::inRecoveryResult = true;
    auto restore = irs::make_finally([&before]()->void { StorageEngineMock::inRecoveryResult = before; });
    arangodb::iresearch::IResearchLinkMeta meta;
    std::string errorField;
    CHECK((false == meta.init(json->slice(), errorField)));
    CHECK((std::string("analyzers=>[0]") == errorField));
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate tests
////////////////////////////////////////////////////////////////////////////////

}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------