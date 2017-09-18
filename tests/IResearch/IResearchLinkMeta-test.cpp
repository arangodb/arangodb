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

#include "Aql/AqlFunctionFeature.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "IResearch/ApplicationServerHelper.h"
#include "IResearch/IResearchLinkMeta.h"
#include "IResearch/IResearchFeature.h"
#include "IResearch/SystemDatabaseFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/FeatureCacheFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "velocypack/Builder.h"
#include "velocypack/Iterator.h"
#include "velocypack/Parser.h"

NS_LOCAL

struct TestAttribute: public irs::attribute {
  DECLARE_ATTRIBUTE_TYPE();
};

DEFINE_ATTRIBUTE_TYPE(TestAttribute);

class EmptyTokenizer: public irs::analysis::analyzer {
public:
  DECLARE_ANALYZER_TYPE();
  EmptyTokenizer() : irs::analysis::analyzer(EmptyTokenizer::type()) {
    _attrs.emplace(_attr);
  }
  virtual irs::attribute_view const& attributes() const NOEXCEPT override { return _attrs; }
  static ptr make(irs::string_ref const&) { PTR_NAMED(EmptyTokenizer, ptr); return ptr; }
  virtual bool next() override { return false; }
  virtual bool reset(irs::string_ref const& data) override { return true; }

private:
  irs::attribute_view _attrs;
  TestAttribute _attr;
};

DEFINE_ANALYZER_TYPE_NAMED(EmptyTokenizer, "empty");
REGISTER_ANALYZER(EmptyTokenizer);

NS_END

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

struct IResearchLinkMetaSetup {
  StorageEngineMock engine;
  arangodb::application_features::ApplicationServer server;
  std::unique_ptr<TRI_vocbase_t> system;
  std::vector<std::pair<arangodb::application_features::ApplicationFeature*, bool>> features;

  IResearchLinkMetaSetup(): server(nullptr, nullptr) {
    arangodb::EngineSelectorFeature::ENGINE = &engine;

    arangodb::tests::init();

    // setup required application features
    features.emplace_back(new arangodb::AuthenticationFeature(&server), true); // required for FeatureCacheFeature
    features.emplace_back(new arangodb::DatabaseFeature(&server), false); // required for FeatureCacheFeature
    features.emplace_back(new arangodb::FeatureCacheFeature(&server), true); // required for IResearchAnalyzerFeature
    features.emplace_back(new arangodb::QueryRegistryFeature(&server), false); // required for constructing TRI_vocbase_t
    arangodb::application_features::ApplicationServer::server->addFeature(features.back().first);
    system = irs::memory::make_unique<TRI_vocbase_t>(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 0, TRI_VOC_SYSTEM_DATABASE);
    features.emplace_back(new arangodb::aql::AqlFunctionFeature(&server), true); // required for IResearchAnalyzerFeature
    features.emplace_back(new arangodb::iresearch::IResearchAnalyzerFeature(&server), true);
    features.emplace_back(new arangodb::iresearch::SystemDatabaseFeature(&server, system.get()), false); // required for IResearchAnalyzerFeature

    for (auto& f : features) {
      arangodb::application_features::ApplicationServer::server->addFeature(f.first);
    }

    for (auto& f : features) {
      f.first->prepare();
    }

    for (auto& f : features) {
      if (f.second) {
        f.first->start();
      }
    }

    auto* analyzers = arangodb::iresearch::getFeature<arangodb::iresearch::IResearchAnalyzerFeature>();

    analyzers->emplace("empty", "empty", "en"); // cache the 'empty' analyzer

    // suppress log messages since tests check error conditions
    arangodb::LogTopic::setLogLevel(arangodb::iresearch::IResearchFeature::IRESEARCH.name(), arangodb::LogLevel::FATAL);
    irs::logger::output_le(iresearch::logger::IRL_FATAL, stderr);
  }

  ~IResearchLinkMetaSetup() {
    system.reset(); // destroy before reseting the 'ENGINE'
    arangodb::LogTopic::setLogLevel(arangodb::iresearch::IResearchFeature::IRESEARCH.name(), arangodb::LogLevel::DEFAULT);
    arangodb::application_features::ApplicationServer::server = nullptr;
    arangodb::EngineSelectorFeature::ENGINE = nullptr;

    // destroy application features
    for (auto& f : features) {
      if (f.second) {
        f.first->stop();
      }
    }

    for (auto& f : features) {
      f.first->unprepare();
    }

    arangodb::FeatureCacheFeature::reset();
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

  CHECK(1. == meta._boost);
  CHECK(true == meta._fields.empty());
  CHECK(false == meta._includeAllFields);
  CHECK(false == meta._nestListValues);
  CHECK(1U == meta._tokenizers.size());
  CHECK((*(meta._tokenizers.begin())));
  CHECK(("identity" == (*(meta._tokenizers.begin()))->name()));
  CHECK((irs::flags({irs::norm::type(), irs::frequency::type(), irs::term_attribute::type(), irs::increment::type()}) == (*(meta._tokenizers.begin()))->features())); // FIXME remove increment, term_attribute
  CHECK(false == !meta._tokenizers.begin()->get());
}

SECTION("test_inheritDefaults") {
  arangodb::iresearch::IResearchAnalyzerFeature analyzers(nullptr);
  arangodb::iresearch::IResearchLinkMeta defaults;
  arangodb::iresearch::IResearchLinkMeta meta;
  std::unordered_set<std::string> expectedFields = { "abc" };
  std::unordered_set<std::string> expectedOverrides = { "xyz" };
  std::string tmpString;

  analyzers.start();

  defaults._boost = 3.14f;
  defaults._fields["abc"] = std::move(arangodb::iresearch::IResearchLinkMeta());
  defaults._includeAllFields = true;
  defaults._nestListValues = true;
  defaults._tokenizers.clear();
  defaults._tokenizers.emplace_back(analyzers.ensure("empty"));
  defaults._fields["abc"]->_fields["xyz"] = std::move(arangodb::iresearch::IResearchLinkMeta());

  auto json = arangodb::velocypack::Parser::fromJson("{}");
  CHECK(true == meta.init(json->slice(), tmpString, defaults));
  CHECK(3.14f == meta._boost);
  CHECK(1U == meta._fields.size());

  for (auto& field: meta._fields) {
    CHECK(1U == expectedFields.erase(field.key()));
    CHECK(1U == field.value()->_fields.size());

    for (auto& fieldOverride: field.value()->_fields) {
      auto& actual = *(fieldOverride.value());
      CHECK(1U == expectedOverrides.erase(fieldOverride.key()));

      if ("xyz" == fieldOverride.key()) {
        CHECK(1.f == actual._boost);
        CHECK(true == actual._fields.empty());
        CHECK(false == actual._includeAllFields);
        CHECK(false == actual._nestListValues);
        CHECK(1U == actual._tokenizers.size());
        CHECK((*(actual._tokenizers.begin())));
        CHECK(("identity" == (*(actual._tokenizers.begin()))->name()));
        CHECK((irs::flags({irs::norm::type(), irs::frequency::type(), irs::term_attribute::type(), irs::increment::type()}) == (*(actual._tokenizers.begin()))->features())); // FIXME remove increment, term_attribute
        CHECK(false == !actual._tokenizers.begin()->get());
      }
    }
  }

  CHECK(true == expectedOverrides.empty());
  CHECK(true == expectedFields.empty());
  CHECK(true == meta._includeAllFields);
  CHECK(true == meta._nestListValues);

  CHECK(1U == meta._tokenizers.size());
  CHECK((*(meta._tokenizers.begin())));
  CHECK(("empty" == (*(meta._tokenizers.begin()))->name()));
  CHECK((irs::flags({TestAttribute::type()}) == (*(meta._tokenizers.begin()))->features()));
  CHECK(false == !meta._tokenizers.begin()->get());
}

SECTION("test_readDefaults") {
  arangodb::iresearch::IResearchLinkMeta meta;
  auto json = arangodb::velocypack::Parser::fromJson("{}");
  std::string tmpString;

  CHECK(true == meta.init(json->slice(), tmpString));
  CHECK(1.f == meta._boost);
  CHECK(true == meta._fields.empty());
  CHECK(false == meta._includeAllFields);
  CHECK(false == meta._nestListValues);
  CHECK(1U == meta._tokenizers.size());
  CHECK((*(meta._tokenizers.begin())));
  CHECK(("identity" == (*(meta._tokenizers.begin()))->name()));
  CHECK((irs::flags({irs::norm::type(), irs::frequency::type(), irs::term_attribute::type(), irs::increment::type()}) == (*(meta._tokenizers.begin()))->features())); // FIXME remove increment, term_attribute

  CHECK(false == !meta._tokenizers.begin()->get());
}

SECTION("test_readCustomizedValues") {
  std::unordered_set<std::string> expectedFields = { "a", "b", "c" };
  std::unordered_set<std::string> expectedOverrides = { "default", "all", "some", "none" };
  std::unordered_set<std::string> expectedTokenizers = { "empty", "identity" };
  arangodb::iresearch::IResearchLinkMeta meta;
  std::string tmpString;

  {
    auto json = arangodb::velocypack::Parser::fromJson("{ \
      \"boost\": 10, \
      \"fields\": { \
        \"a\": {}, \
        \"b\": {}, \
        \"c\": { \
          \"fields\": { \
            \"default\": { \"boost\": 1, \"fields\": {}, \"includeAllFields\": false, \"nestListValues\": false, \"tokenizers\": [ \"identity\" ] }, \
            \"all\": { \"boost\": 11, \"fields\": {\"d\": {}, \"e\": {}}, \"includeAllFields\": true, \"nestListValues\": true, \"tokenizers\": [ \"empty\" ] }, \
            \"some\": { \"boost\": 12, \"nestListValues\": true }, \
            \"none\": {} \
          } \
        } \
      }, \
      \"includeAllFields\": true, \
      \"nestListValues\": true, \
      \"tokenizers\": [ \"empty\", \"identity\" ] \
    }");
    CHECK(true == meta.init(json->slice(), tmpString));
    CHECK(10.f == meta._boost);
    CHECK(3U == meta._fields.size());

    for (auto& field: meta._fields) {
      CHECK(1U == expectedFields.erase(field.key()));

      for (auto& fieldOverride: field.value()->_fields) {
        auto& actual = *(fieldOverride.value());

        CHECK(1U == expectedOverrides.erase(fieldOverride.key()));

        if ("default" == fieldOverride.key()) {
          CHECK(1.f == actual._boost);
          CHECK(true == actual._fields.empty());
          CHECK(false == actual._includeAllFields);
          CHECK(false == actual._nestListValues);
          CHECK(1U == actual._tokenizers.size());
          CHECK((*(actual._tokenizers.begin())));
          CHECK(("identity" == (*(actual._tokenizers.begin()))->name()));
          CHECK((irs::flags({irs::norm::type(), irs::frequency::type(), irs::term_attribute::type(), irs::increment::type()}) == (*(actual._tokenizers.begin()))->features())); // FIXME remove increment, term_attribute
          CHECK(false == !actual._tokenizers.begin()->get());
        } else if ("all" == fieldOverride.key()) {
          CHECK(11. == actual._boost);
          CHECK(2U == actual._fields.size());
          CHECK(true == (actual._fields.find("d") != actual._fields.end()));
          CHECK(true == (actual._fields.find("e") != actual._fields.end()));
          CHECK(true == actual._includeAllFields);
          CHECK(true == actual._nestListValues);
          CHECK(1U == actual._tokenizers.size());
          CHECK((*(actual._tokenizers.begin())));
          CHECK(("empty" == (*(actual._tokenizers.begin()))->name()));
          CHECK((irs::flags({TestAttribute::type()}) == (*(actual._tokenizers.begin()))->features()));
          CHECK(false == !actual._tokenizers.begin()->get());
        } else if ("some" == fieldOverride.key()) {
          CHECK(12. == actual._boost);
          CHECK(true == actual._fields.empty()); // not inherited
          CHECK(true == actual._includeAllFields); // inherited
          CHECK(true == actual._nestListValues);
          CHECK(2U == actual._tokenizers.size());
          auto itr = actual._tokenizers.begin();
          CHECK((*itr));
          CHECK(("empty" == (*itr)->name()));
          CHECK((irs::flags({TestAttribute::type()}) == (*itr)->features()));
          CHECK(false == !itr->get());
          ++itr;
          CHECK((*itr));
          CHECK(("identity" == (*itr)->name()));
          CHECK((irs::flags({irs::norm::type(), irs::frequency::type(), irs::term_attribute::type(), irs::increment::type()}) == (*itr)->features())); // FIXME remove increment, term_attribute
          CHECK(false == !itr->get());
        } else if ("none" == fieldOverride.key()) {
          CHECK(10. == actual._boost); // inherited
          CHECK(true == actual._fields.empty()); // not inherited
          CHECK(true == actual._includeAllFields); // inherited
          CHECK(true == actual._nestListValues); // inherited
          auto itr = actual._tokenizers.begin();
          CHECK((*itr));
          CHECK(("empty" == (*itr)->name()));
          CHECK((irs::flags({TestAttribute::type()}) == (*itr)->features()));
          CHECK(false == !itr->get());
          ++itr;
          CHECK((*itr));
          CHECK(("identity" == (*itr)->name()));
          CHECK((irs::flags({irs::norm::type(), irs::frequency::type(), irs::term_attribute::type(), irs::increment::type()}) == (*itr)->features())); // FIXME remove increment, term_attribute
          CHECK(false == !itr->get());
        }
      }
    }

    CHECK(true == expectedOverrides.empty());
    CHECK(true == expectedFields.empty());
    CHECK(true == meta._includeAllFields);
    CHECK(true == meta._nestListValues);
    auto itr = meta._tokenizers.begin();
    CHECK((*itr));
    CHECK(("empty" == (*itr)->name()));
    CHECK((irs::flags({TestAttribute::type()}) == (*itr)->features()));
    CHECK(false == !itr->get());
    ++itr;
    CHECK((*itr));
    CHECK(("identity" == (*itr)->name()));
    CHECK((irs::flags({irs::norm::type(), irs::frequency::type(), irs::term_attribute::type(), irs::increment::type()}) == (*itr)->features()));  // FIXME remove increment, term_attribute
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
  tmpSlice = slice.get("boost");
  CHECK((true == tmpSlice.isNumber() && 1. == tmpSlice.getDouble()));
  tmpSlice = slice.get("fields");
  CHECK((true == tmpSlice.isObject() && 0 == tmpSlice.length()));
  tmpSlice = slice.get("includeAllFields");
  CHECK((true == tmpSlice.isBool() && false == tmpSlice.getBool()));
  tmpSlice = slice.get("nestListValues");
  CHECK((true == tmpSlice.isBool() && false == tmpSlice.getBool()));
  tmpSlice = slice.get("tokenizers");
  CHECK((
    true ==
    tmpSlice.isArray() &&
    1 == tmpSlice.length() &&
    tmpSlice.at(0).isString() &&
    std::string("identity") == tmpSlice.at(0).copyString()
  ));
}

SECTION("test_writeCustomizedValues") {
  arangodb::iresearch::IResearchAnalyzerFeature analyzers(nullptr);
  arangodb::iresearch::IResearchLinkMeta meta;

  analyzers.emplace("identity", "identity", "");
  analyzers.emplace("empty", "empty", "en");

  meta._boost = 10.;
  meta._includeAllFields = true;
  meta._nestListValues = true;
  meta._tokenizers.clear();
  meta._tokenizers.emplace_back(analyzers.ensure("identity"));
  meta._tokenizers.emplace_back(analyzers.ensure("empty"));
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

  auto& overrideDefault = *(meta._fields["c"]->_fields["default"]);
  auto& overrideAll = *(meta._fields["c"]->_fields["all"]);
  auto& overrideSome = *(meta._fields["c"]->_fields["some"]);
  auto& overrideNone = *(meta._fields["c"]->_fields["none"]);

  overrideAll._boost = 11.;
  overrideAll._fields.clear(); // do not inherit fields to match jSon inheritance
  overrideAll._fields["x"] = std::move(arangodb::iresearch::IResearchLinkMeta());
  overrideAll._fields["y"] = std::move(arangodb::iresearch::IResearchLinkMeta());
  overrideAll._includeAllFields = false;
  overrideAll._nestListValues = false;
  overrideAll._tokenizers.clear();
  overrideAll._tokenizers.emplace_back(analyzers.ensure("empty"));
  overrideSome._boost = 12;
  overrideSome._fields.clear(); // do not inherit fields to match jSon inheritance
  overrideSome._nestListValues = false;
  overrideNone._fields.clear(); // do not inherit fields to match jSon inheritance

  std::unordered_set<std::string> expectedFields = { "a", "b", "c" };
  std::unordered_set<std::string> expectedOverrides = { "default", "all", "some", "none" };
  std::unordered_set<std::string> expectedTokenizers = { "empty", "identity" };
  arangodb::velocypack::Builder builder;
  arangodb::velocypack::Slice tmpSlice;

  CHECK(true == meta.json(arangodb::velocypack::ObjectBuilder(&builder)));

  auto slice = builder.slice();

  CHECK((5U == slice.length()));
  tmpSlice = slice.get("boost");
  CHECK((true == tmpSlice.isNumber() && 10. == tmpSlice.getDouble()));
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
        tmpSlice = sliceOverride.get("boost");
        CHECK((true == tmpSlice.isNumber() && 1. == tmpSlice.getDouble()));
        tmpSlice = sliceOverride.get("includeAllFields");
        CHECK(true == (false == tmpSlice.getBool()));
        tmpSlice = sliceOverride.get("nestListValues");
        CHECK(true == (false == tmpSlice.getBool()));
        tmpSlice = sliceOverride.get("tokenizers");
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
        tmpSlice = sliceOverride.get("boost");
        CHECK((true == tmpSlice.isNumber() && 11. == tmpSlice.getDouble()));
        tmpSlice = sliceOverride.get("fields");
        CHECK((true == tmpSlice.isObject() && 2 == tmpSlice.length()));
        for (arangodb::velocypack::ObjectIterator overrideFieldItr(tmpSlice); overrideFieldItr.valid(); ++overrideFieldItr) {
          CHECK((true == overrideFieldItr.key().isString() && 1 == expectedFields.erase(overrideFieldItr.key().copyString())));
        }
        CHECK(true == expectedFields.empty());
        tmpSlice = sliceOverride.get("includeAllFields");
        CHECK((true == tmpSlice.isBool() && false == tmpSlice.getBool()));
        tmpSlice = sliceOverride.get("nestListValues");
        CHECK((true == tmpSlice.isBool() && false == tmpSlice.getBool()));
        tmpSlice = sliceOverride.get("tokenizers");
        CHECK((
          true ==
          tmpSlice.isArray() &&
          1 == tmpSlice.length() &&
          tmpSlice.at(0).isString() &&
          std::string("empty") == tmpSlice.at(0).copyString()
        ));
      } else if ("some" == fieldOverride.copyString()) {
        CHECK(2U == sliceOverride.length());
        tmpSlice = sliceOverride.get("boost");
        CHECK((true == tmpSlice.isNumber() && 12. == tmpSlice.getDouble()));
        tmpSlice = sliceOverride.get("nestListValues");
        CHECK((true == tmpSlice.isBool() && false == tmpSlice.getBool()));
      } else if ("none" == fieldOverride.copyString()) {
        CHECK(0U == sliceOverride.length());
      }
    }
  }

  CHECK(true == expectedOverrides.empty());
  CHECK(true == expectedFields.empty());
  tmpSlice = slice.get("includeAllFields");
  CHECK((true == tmpSlice.isBool() && true == tmpSlice.getBool()));
  tmpSlice = slice.get("nestListValues");
  CHECK((true == tmpSlice.isBool() && true == tmpSlice.getBool()));
  tmpSlice = slice.get("tokenizers");
  CHECK((true == tmpSlice.isArray() && 2 == tmpSlice.length()));

  for (arangodb::velocypack::ArrayIterator tokenizersItr(tmpSlice); tokenizersItr.valid(); ++tokenizersItr) {
    auto key = *tokenizersItr;
    CHECK((true == key.isString() && 1 == expectedTokenizers.erase(key.copyString())));
  }

  CHECK(true == expectedTokenizers.empty());
}

SECTION("test_readMaskAll") {
  arangodb::iresearch::IResearchLinkMeta meta;
  arangodb::iresearch::IResearchLinkMeta::Mask mask;
  std::string tmpString;

  auto json = arangodb::velocypack::Parser::fromJson("{ \
    \"boost\": 10, \
    \"fields\": { \"a\": {} }, \
    \"includeAllFields\": true, \
    \"nestListValues\": true, \
    \"tokenizers\": [] \
  }");
  CHECK(true == meta.init(json->slice(), tmpString, arangodb::iresearch::IResearchLinkMeta::DEFAULT(), &mask));
  CHECK(true == mask._boost);
  CHECK(true == mask._fields);
  CHECK(true == mask._includeAllFields);
  CHECK(true == mask._nestListValues);
  CHECK(true == mask._tokenizers);
}

SECTION("test_readMaskNone") {
  arangodb::iresearch::IResearchLinkMeta meta;
  arangodb::iresearch::IResearchLinkMeta::Mask mask;
  std::string tmpString;

  auto json = arangodb::velocypack::Parser::fromJson("{}");
  CHECK(true == meta.init(json->slice(), tmpString, arangodb::iresearch::IResearchLinkMeta::DEFAULT(), &mask));
  CHECK(false == mask._boost);
  CHECK(false == mask._fields);
  CHECK(false == mask._includeAllFields);
  CHECK(false == mask._nestListValues);
  CHECK(false == mask._tokenizers);
}

SECTION("test_writeMaskAll") {
  arangodb::iresearch::IResearchLinkMeta meta;
  arangodb::iresearch::IResearchLinkMeta::Mask mask(true);
  arangodb::velocypack::Builder builder;

  CHECK(true == meta.json(arangodb::velocypack::ObjectBuilder(&builder), nullptr, &mask));

  auto slice = builder.slice();

  CHECK((5U == slice.length()));
  CHECK(true == slice.hasKey("boost"));
  CHECK(true == slice.hasKey("fields"));
  CHECK(true == slice.hasKey("includeAllFields"));
  CHECK(true == slice.hasKey("nestListValues"));
  CHECK(true == slice.hasKey("tokenizers"));
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
