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

#include "StorageEngineMock.h"

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
#include "velocypack/Builder.h"
#include "velocypack/Iterator.h"
#include "velocypack/Parser.h"

NS_LOCAL

struct TestAttribute: public irs::attribute {
  DECLARE_ATTRIBUTE_TYPE();
};

DEFINE_ATTRIBUTE_TYPE(TestAttribute);

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
  TestAttribute _attr;
};

DEFINE_ANALYZER_TYPE_NAMED(EmptyAnalyzer, "empty");
REGISTER_ANALYZER_JSON(EmptyAnalyzer, EmptyAnalyzer::make);

NS_END

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

struct IResearchLinkMetaSetup {
  StorageEngineMock engine;
  arangodb::application_features::ApplicationServer server;
  std::unique_ptr<TRI_vocbase_t> system;
  std::map<std::string, std::pair<arangodb::application_features::ApplicationFeature*, bool>> features;
  std::vector<arangodb::application_features::ApplicationFeature*> orderedFeatures;

  IResearchLinkMetaSetup(): engine(server), server(nullptr, nullptr) {
    arangodb::EngineSelectorFeature::ENGINE = &engine;

    arangodb::tests::init();

    // suppress INFO {authentication} Authentication is turned on (system only), authentication for unix sockets is turned on
    arangodb::LogTopic::setLogLevel(arangodb::Logger::AUTHENTICATION.name(), arangodb::LogLevel::WARN);

    auto buildFeatureEntry = [&] (arangodb::application_features::ApplicationFeature* ftr, bool start) -> void {
      std::string name = ftr->name();
      features.emplace(name, std::make_pair(ftr, start));
    };
    arangodb::application_features::ApplicationFeature* tmpFeature;

    buildFeatureEntry(new arangodb::application_features::BasicFeaturePhase(server, false), false);
    buildFeatureEntry(new arangodb::application_features::CommunicationFeaturePhase(server), false);
    buildFeatureEntry(new arangodb::application_features::ClusterFeaturePhase(server), false);
    buildFeatureEntry(new arangodb::application_features::DatabaseFeaturePhase(server), false);
    buildFeatureEntry(new arangodb::application_features::GreetingsFeaturePhase(server, false), false);
    buildFeatureEntry(new arangodb::application_features::V8FeaturePhase(server), false);


    // setup required application features
    buildFeatureEntry(new arangodb::AuthenticationFeature(server), true);
    buildFeatureEntry(new arangodb::DatabaseFeature(server), false);
    buildFeatureEntry(new arangodb::ShardingFeature(server), false);
    buildFeatureEntry(tmpFeature = new arangodb::QueryRegistryFeature(server), false);
    arangodb::application_features::ApplicationServer::server->addFeature(tmpFeature); // need QueryRegistryFeature feature to be added now in order to create the system database
    system = irs::memory::make_unique<TRI_vocbase_t>(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 0, TRI_VOC_SYSTEM_DATABASE);
    buildFeatureEntry(new arangodb::SystemDatabaseFeature(server, system.get()), false); // required for IResearchAnalyzerFeature
    buildFeatureEntry(new arangodb::aql::AqlFunctionFeature(server), true); // required for IResearchAnalyzerFeature
    buildFeatureEntry(new arangodb::iresearch::IResearchAnalyzerFeature(server), true);

    #if USE_ENTERPRISE
      buildFeatureEntry(new arangodb::LdapFeature(server), false); // required for AuthenticationFeature with USE_ENTERPRISE
    #endif

    for (auto& f : features) {
      arangodb::application_features::ApplicationServer::server->addFeature(f.second.first);
    }
    arangodb::application_features::ApplicationServer::server->setupDependencies(false);
    orderedFeatures = arangodb::application_features::ApplicationServer::server->getOrderedFeatures();

    for (auto& f : orderedFeatures) {
      f->prepare();
    }

    for (auto& f : orderedFeatures) {
      if (features.at(f->name()).second) {
        f->start();
      }
    }

    auto* analyzers = arangodb::application_features::ApplicationServer::lookupFeature<
      arangodb::iresearch::IResearchAnalyzerFeature
    >();

    analyzers->emplace("empty", "empty", "en", irs::flags{ TestAttribute::type() }); // cache the 'empty' analyzer

    // suppress log messages since tests check error conditions
    arangodb::LogTopic::setLogLevel(arangodb::iresearch::TOPIC.name(), arangodb::LogLevel::FATAL);
    irs::logger::output_le(iresearch::logger::IRL_FATAL, stderr);
  }

  ~IResearchLinkMetaSetup() {
    system.reset(); // destroy before reseting the 'ENGINE'
    arangodb::LogTopic::setLogLevel(arangodb::iresearch::TOPIC.name(), arangodb::LogLevel::DEFAULT);
    arangodb::application_features::ApplicationServer::server = nullptr;
    arangodb::EngineSelectorFeature::ENGINE = nullptr;

    // destroy application features
    for (auto f = orderedFeatures.rbegin() ; f != orderedFeatures.rend(); ++f) { 
      if (features.at((*f)->name()).second) {
        (*f)->stop();
      }
    }

    for (auto f = orderedFeatures.rbegin() ; f != orderedFeatures.rend(); ++f) { 
      (*f)->unprepare();
    }

    arangodb::LogTopic::setLogLevel(arangodb::Logger::AUTHENTICATION.name(), arangodb::LogLevel::DEFAULT);
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
  CHECK(true == meta.init(json->slice(), tmpString, defaults));
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
  arangodb::iresearch::IResearchLinkMeta meta;
  auto json = arangodb::velocypack::Parser::fromJson("{}");
  std::string tmpString;

  CHECK(true == meta.init(json->slice(), tmpString));
  CHECK(true == meta._fields.empty());
  CHECK(false == meta._includeAllFields);
  CHECK(false == meta._trackListPositions);
  CHECK((arangodb::iresearch::ValueStorage::NONE == meta._storeValues));
  CHECK(1U == meta._analyzers.size());
  CHECK((*(meta._analyzers.begin())));
  CHECK(("identity" == (*(meta._analyzers.begin()))->name()));
  CHECK((irs::flags({irs::norm::type(), irs::frequency::type()}) == (*(meta._analyzers.begin()))->features()));

  CHECK(false == !meta._analyzers.begin()->get());
}

SECTION("test_readCustomizedValues") {
  std::unordered_set<std::string> expectedFields = { "a", "b", "c" };
  std::unordered_set<std::string> expectedOverrides = { "default", "all", "some", "none" };
  std::unordered_set<std::string> expectedAnalyzers = { "empty", "identity" };
  arangodb::iresearch::IResearchLinkMeta meta;
  std::string tmpString;

  {
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
          CHECK((irs::flags({TestAttribute::type()}) == (*(actual._analyzers.begin()))->features()));
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
          CHECK((irs::flags({TestAttribute::type()}) == (*itr)->features()));
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
          CHECK((irs::flags({TestAttribute::type()}) == (*itr)->features()));
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
    CHECK((irs::flags({TestAttribute::type()}) == (*itr)->features()));
    CHECK(false == !itr->get());
    ++itr;
    CHECK((*itr));
    CHECK(("identity" == (*itr)->name()));
    CHECK((irs::flags({irs::norm::type(), irs::frequency::type()}) == (*itr)->features()));
    CHECK(false == !itr->get());
  }
}

SECTION("test_writeDefaults") {
  arangodb::iresearch::IResearchLinkMeta meta;
  arangodb::velocypack::Builder builder;
  arangodb::velocypack::Slice tmpSlice;

  CHECK(true == meta.json(arangodb::velocypack::ObjectBuilder(&builder)));

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

SECTION("test_writeCustomizedValues") {
  arangodb::iresearch::IResearchAnalyzerFeature analyzers(s.server);
  arangodb::iresearch::IResearchLinkMeta meta;

  analyzers.emplace("identity", "identity", "");
  analyzers.emplace("empty", "empty", "en");

  meta._includeAllFields = true;
  meta._trackListPositions = true;
  meta._storeValues = arangodb::iresearch::ValueStorage::FULL;
  meta._analyzers.clear();
  meta._analyzers.emplace_back(analyzers.ensure("identity"));
  meta._analyzers.emplace_back(analyzers.ensure("empty"));
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
  overrideAll._analyzers.emplace_back(analyzers.ensure("empty"));
  overrideSome._fields.clear(); // do not inherit fields to match jSon inheritance
  overrideSome._trackListPositions = false;
  overrideSome._storeValues = arangodb::iresearch::ValueStorage::ID;
  overrideNone._fields.clear(); // do not inherit fields to match jSon inheritance

  std::unordered_set<std::string> expectedFields = { "a", "b", "c" };
  std::unordered_set<std::string> expectedOverrides = { "default", "all", "some", "none" };
  std::unordered_set<std::string> expectedAnalyzers = { "empty", "identity" };
  arangodb::velocypack::Builder builder;
  arangodb::velocypack::Slice tmpSlice;

  CHECK(true == meta.json(arangodb::velocypack::ObjectBuilder(&builder)));

  auto slice = builder.slice();

  CHECK((5U == slice.length()));
  tmpSlice = slice.get("fields");
  CHECK((true == tmpSlice.isObject() && 3 == tmpSlice.length()));

  for (arangodb::velocypack::ObjectIterator itr(tmpSlice); itr.valid(); ++itr) {
    auto key = itr.key();
    auto value = itr.value();
    CHECK((true == key.isString() && 1 == expectedFields.erase(key.copyString())));
    CHECK(true == value.isObject());

    if (!value.hasKey("fields")) {
      continue;
    }

    tmpSlice = value.get("fields");

    for (arangodb::velocypack::ObjectIterator overrideItr(tmpSlice); overrideItr.valid(); ++overrideItr) {
      auto fieldOverride = overrideItr.key();
      auto sliceOverride = overrideItr.value();
      CHECK((true == fieldOverride.isString() && sliceOverride.isObject()));
      CHECK(1U == expectedOverrides.erase(fieldOverride.copyString()));

      if ("default" == fieldOverride.copyString()) {
        CHECK((4U == sliceOverride.length()));
        tmpSlice = sliceOverride.get("includeAllFields");
        CHECK(true == (false == tmpSlice.getBool()));
        tmpSlice = sliceOverride.get("trackListPositions");
        CHECK(true == (false == tmpSlice.getBool()));
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
        CHECK(true == expectedFields.empty());
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
        CHECK(2U == sliceOverride.length());
        tmpSlice = sliceOverride.get("trackListPositions");
        CHECK((true == tmpSlice.isBool() && false == tmpSlice.getBool()));
        tmpSlice = sliceOverride.get("storeValues");
        CHECK((true == tmpSlice.isString() && std::string("id") == tmpSlice.copyString()));
      } else if ("none" == fieldOverride.copyString()) {
        CHECK(0U == sliceOverride.length());
      }
    }
  }

  CHECK(true == expectedOverrides.empty());
  CHECK(true == expectedFields.empty());
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

  CHECK(true == expectedAnalyzers.empty());
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
  CHECK(true == meta.init(json->slice(), tmpString, arangodb::iresearch::IResearchLinkMeta::DEFAULT(), &mask));
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
  CHECK(true == meta.init(json->slice(), tmpString, arangodb::iresearch::IResearchLinkMeta::DEFAULT(), &mask));
  CHECK(false == mask._fields);
  CHECK(false == mask._includeAllFields);
  CHECK(false == mask._trackListPositions);
  CHECK((false == mask._storeValues));
  CHECK(false == mask._analyzers);
}

SECTION("test_writeMaskAll") {
  arangodb::iresearch::IResearchLinkMeta meta;
  arangodb::iresearch::IResearchLinkMeta::Mask mask(true);
  arangodb::velocypack::Builder builder;

  CHECK(true == meta.json(arangodb::velocypack::ObjectBuilder(&builder), nullptr, &mask));

  auto slice = builder.slice();

  CHECK((5U == slice.length()));
  CHECK(true == slice.hasKey("fields"));
  CHECK(true == slice.hasKey("includeAllFields"));
  CHECK(true == slice.hasKey("trackListPositions"));
  CHECK(true == slice.hasKey("storeValues"));
  CHECK(true == slice.hasKey("analyzers"));
}

SECTION("test_writeMaskNone") {
  arangodb::iresearch::IResearchLinkMeta meta;
  arangodb::iresearch::IResearchLinkMeta::Mask mask(false);
  arangodb::velocypack::Builder builder;

  CHECK(true == meta.json(arangodb::velocypack::ObjectBuilder(&builder), nullptr, &mask));

  auto slice = builder.slice();

  CHECK(0U == slice.length());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate tests
////////////////////////////////////////////////////////////////////////////////

}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
