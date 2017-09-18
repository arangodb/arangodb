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

#include "Aql/AqlFunctionFeature.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "IResearch/ApplicationServerHelper.h"
#include "IResearch/IResearchAnalyzerFeature.h"
#include "IResearch/IResearchFeature.h"
#include "IResearch/IResearchDocument.h"
#include "IResearch/IResearchLinkMeta.h"
#include "IResearch/IResearchKludge.h"
#include "IResearch/SystemDatabaseFeature.h"
#include "Logger/Logger.h"
#include "Logger/LogTopic.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/FeatureCacheFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"

#include "velocypack/Iterator.h"
#include "velocypack/Builder.h"
#include "velocypack/Parser.h"
#include "velocypack/velocypack-aliases.h"

#include "analysis/analyzers.hpp"
#include "analysis/token_streams.hpp"

NS_LOCAL

struct TestAttribute: public irs::attribute {
  DECLARE_ATTRIBUTE_TYPE();
};

DEFINE_ATTRIBUTE_TYPE(TestAttribute);

class EmptyTokenizer: public irs::analysis::analyzer {
 public:
  DECLARE_ANALYZER_TYPE();
  EmptyTokenizer(): irs::analysis::analyzer(EmptyTokenizer::type()) {
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

DEFINE_ANALYZER_TYPE_NAMED(EmptyTokenizer, "iresearch-document-empty");
REGISTER_ANALYZER(EmptyTokenizer);

class InvalidTokenizer: public irs::analysis::analyzer {
 public:
  static bool returnNullFromMake;

  DECLARE_ANALYZER_TYPE();
  InvalidTokenizer(): irs::analysis::analyzer(InvalidTokenizer::type()) {
    _attrs.emplace(_attr);
  }
  virtual irs::attribute_view const& attributes() const NOEXCEPT override { return _attrs; }
  static ptr make(irs::string_ref const&) {
    if (!returnNullFromMake) {
      PTR_NAMED(InvalidTokenizer, ptr);
      returnNullFromMake = true;
      return ptr;
    }
    return nullptr;
  }
  virtual bool next() override { return false; }
  virtual bool reset(irs::string_ref const& data) override { return true; }

 private:
  irs::attribute_view _attrs;
  TestAttribute _attr;
};

bool InvalidTokenizer::returnNullFromMake = false;

DEFINE_ANALYZER_TYPE_NAMED(InvalidTokenizer, "iresearch-document-invalid");
REGISTER_ANALYZER(InvalidTokenizer);

std::string mangleBool(std::string name) {
  arangodb::iresearch::kludge::mangleBool(name);
  return name;
}

std::string mangleNull(std::string name) {
  arangodb::iresearch::kludge::mangleNull(name);
  return name;
}

std::string mangleNumeric(std::string name) {
  arangodb::iresearch::kludge::mangleNumeric(name);
  return name;
}

std::string mangleString(std::string name, std::string suffix) {
  arangodb::iresearch::kludge::mangleAnalyzer(name);
  name += suffix;
  return name;
}

std::string mangleStringIdentity(std::string name) {
  arangodb::iresearch::kludge::mangleStringField(
    name,
    arangodb::iresearch::IResearchAnalyzerFeature::identity()
  );
  return name;
}

NS_END

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

struct IResearchDocumentSetup {
  StorageEngineMock engine;
  arangodb::application_features::ApplicationServer server;
  std::unique_ptr<TRI_vocbase_t> system;
  std::vector<std::pair<arangodb::application_features::ApplicationFeature*, bool>> features;

  IResearchDocumentSetup(): server(nullptr, nullptr) {
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

    // ensure that there will be no exception on 'emplace'
    InvalidTokenizer::returnNullFromMake = false;

    analyzers->emplace("iresearch-document-empty", "iresearch-document-empty", "en"); // cache analyzer
    analyzers->emplace("iresearch-document-invalid", "iresearch-document-invalid", "en"); // cache analyzer

    // suppress log messages since tests check error conditions
    arangodb::LogTopic::setLogLevel(arangodb::iresearch::IResearchFeature::IRESEARCH.name(), arangodb::LogLevel::FATAL);
    irs::logger::output_le(iresearch::logger::IRL_FATAL, stderr);
  }

  ~IResearchDocumentSetup() {
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

TEST_CASE("IResearchDocumentTest", "[iresearch][iresearch-document]") {
  IResearchDocumentSetup s;
  UNUSED(s);

SECTION("Field_setCid") {
  irs::flags features;
  features.add<TestAttribute>();

  arangodb::iresearch::Field field;

  // reset field
  field._boost = 25;
  field._features = &features;
  field._analyzer = nullptr;

  // check CID value
  {
    TRI_voc_cid_t cid = 10;
    arangodb::iresearch::Field::setCidValue(field, cid, arangodb::iresearch::Field::init_stream_t());
    CHECK(1.f == field._boost);
    CHECK(arangodb::iresearch::DocumentPrimaryKey::CID() == field._name);
    CHECK(&irs::flags::empty_instance() == field._features);

    auto* stream = dynamic_cast<irs::string_token_stream*>(field._analyzer.get());
    REQUIRE(nullptr != stream);
    CHECK(stream->next());
    CHECK(!stream->next());

    arangodb::iresearch::Field::setCidValue(field, cid);
    CHECK(1.f == field._boost);
    CHECK(arangodb::iresearch::DocumentPrimaryKey::CID() == field._name);
    CHECK(&irs::flags::empty_instance() == field._features);
    CHECK(stream == field._analyzer.get());
    CHECK(stream->next());
    CHECK(!stream->next());
  }

  // reset field
  field._boost = 25;
  field._features = &features;
  field._analyzer = nullptr;

  // check RID value
  {
    TRI_voc_rid_t rid = 10;
    arangodb::iresearch::Field::setRidValue(field, rid, arangodb::iresearch::Field::init_stream_t());
    CHECK(1.f == field._boost);
    CHECK(arangodb::iresearch::DocumentPrimaryKey::RID() == field._name);
    CHECK(&irs::flags::empty_instance() == field._features);

    auto* stream = dynamic_cast<irs::string_token_stream*>(field._analyzer.get());
    REQUIRE(nullptr != stream);
    CHECK(stream->next());
    CHECK(!stream->next());

    arangodb::iresearch::Field::setRidValue(field, rid);
    CHECK(1.f == field._boost);
    CHECK(arangodb::iresearch::DocumentPrimaryKey::RID() == field._name);
    CHECK(&irs::flags::empty_instance() == field._features);
    CHECK(stream == field._analyzer.get());
    CHECK(stream->next());
    CHECK(!stream->next());
  }
}

SECTION("FieldIterator_static_checks") {
  static_assert(
    std::is_same<
      std::forward_iterator_tag,
      arangodb::iresearch::FieldIterator::iterator_category
    >::value,
    "Invalid iterator category"
  );

  static_assert(
    std::is_same<
      arangodb::iresearch::Field const,
      arangodb::iresearch::FieldIterator::value_type
    >::value,
    "Invalid iterator value type"
  );

  static_assert(
    std::is_same<
      arangodb::iresearch::Field const&,
      arangodb::iresearch::FieldIterator::reference
    >::value,
    "Invalid iterator reference type"
  );

  static_assert(
    std::is_same<
      arangodb::iresearch::Field const*,
      arangodb::iresearch::FieldIterator::pointer
    >::value,
    "Invalid iterator pointer type"
  );

  static_assert(
    std::is_same<
      std::ptrdiff_t,
      arangodb::iresearch::FieldIterator::difference_type
    >::value,
    "Invalid iterator difference type"
  );
}

SECTION("FieldIterator_construct") {
  arangodb::iresearch::FieldIterator it;
  CHECK(!it.valid());
  CHECK(it == arangodb::iresearch::FieldIterator());
  CHECK(it == arangodb::iresearch::FieldIterator::END);
}

SECTION("FieldIterator_traverse_complex_object_custom_nested_delimiter") {
  auto json = arangodb::velocypack::Parser::fromJson("{ \
    \"nested\": { \"foo\": \"str\" }, \
    \"keys\": [ \"1\",\"2\",\"3\",\"4\" ], \
    \"tokenizers\": [], \
    \"boost\": \"10\", \
    \"depth\": \"20\", \
    \"fields\": { \"fieldA\" : { \"name\" : \"a\" }, \"fieldB\" : { \"name\" : \"b\" } }, \
    \"listValuation\": \"ignored\", \
    \"locale\": \"ru_RU.KOI8-R\", \
    \"array\" : [ \
      { \"id\" : \"1\", \"subarr\" : [ \"1\", \"2\", \"3\" ], \"subobj\" : { \"id\" : \"1\" } }, \
      { \"subarr\" : [ \"4\", \"5\", \"6\" ], \"subobj\" : { \"name\" : \"foo\" }, \"id\" : \"2\" }, \
      { \"id\" : \"3\", \"subarr\" : [ \"7\", \"8\", \"9\" ], \"subobj\" : { \"id\" : \"2\" } } \
    ] \
  }");

  std::unordered_map<std::string, size_t> expectedValues {
    { mangleStringIdentity("nested.foo"), 1 },
    { mangleStringIdentity("keys"), 4 },
    { mangleStringIdentity("boost"), 1 },
    { mangleStringIdentity("depth"), 1 },
    { mangleStringIdentity("fields.fieldA.name"), 1 },
    { mangleStringIdentity("fields.fieldB.name"), 1 },
    { mangleStringIdentity("listValuation"), 1 },
    { mangleStringIdentity("locale"), 1 },
    { mangleStringIdentity("array.id"), 3 },
    { mangleStringIdentity("array.subarr"), 9 },
    { mangleStringIdentity("array.subobj.id"), 2 },
    { mangleStringIdentity("array.subobj.name"), 1 },
    { mangleStringIdentity("array.id"), 2 }
  };

  auto const slice = json->slice();

  arangodb::iresearch::IResearchLinkMeta linkMeta;
  linkMeta._includeAllFields = true; // include all fields

  arangodb::iresearch::FieldIterator it(slice, linkMeta);
  CHECK(it != arangodb::iresearch::FieldIterator::END);

  // default analyzer
  auto const expected_analyzer =  irs::analysis::analyzers::get("identity", "");
  auto analyzers = arangodb::iresearch::getFeature<arangodb::iresearch::IResearchAnalyzerFeature>();
  CHECK(nullptr != analyzers);
  auto const expected_features = analyzers->get("identity")->features();

  while (it.valid()) {
    auto& field = *it;
    std::string const actualName = std::string(field.name());
    auto const expectedValue = expectedValues.find(actualName);
    REQUIRE(expectedValues.end() != expectedValue);

    auto& refs = expectedValue->second;
    if (!--refs) {
      expectedValues.erase(expectedValue);
    }

    auto& analyzer = dynamic_cast<irs::analysis::analyzer&>(field.get_tokens());
    CHECK(expected_features == field.features());
    CHECK(&expected_analyzer->type() == &analyzer.type());
    CHECK(linkMeta._boost == field.boost());

    ++it;
  }

  CHECK(expectedValues.empty());
  CHECK(it == arangodb::iresearch::FieldIterator::END);
}

SECTION("FieldIterator_traverse_complex_object_all_fields") {
  auto json = arangodb::velocypack::Parser::fromJson("{ \
    \"nested\": { \"foo\": \"str\" }, \
    \"keys\": [ \"1\",\"2\",\"3\",\"4\" ], \
    \"tokenizers\": [], \
    \"boost\": \"10\", \
    \"depth\": \"20\", \
    \"fields\": { \"fieldA\" : { \"name\" : \"a\" }, \"fieldB\" : { \"name\" : \"b\" } }, \
    \"listValuation\": \"ignored\", \
    \"locale\": \"ru_RU.KOI8-R\", \
    \"array\" : [ \
      { \"id\" : \"1\", \"subarr\" : [ \"1\", \"2\", \"3\" ], \"subobj\" : { \"id\" : \"1\" } }, \
      { \"subarr\" : [ \"4\", \"5\", \"6\" ], \"subobj\" : { \"name\" : \"foo\" }, \"id\" : \"2\" }, \
      { \"id\" : \"3\", \"subarr\" : [ \"7\", \"8\", \"9\" ], \"subobj\" : { \"id\" : \"2\" } } \
    ] \
  }");

  std::unordered_map<std::string, size_t> expectedValues {
    { mangleStringIdentity("nested.foo"), 1 },
    { mangleStringIdentity("keys"), 4 },
    { mangleStringIdentity("boost"), 1 },
    { mangleStringIdentity("depth"), 1 },
    { mangleStringIdentity("fields.fieldA.name"), 1 },
    { mangleStringIdentity("fields.fieldB.name"), 1 },
    { mangleStringIdentity("listValuation"), 1 },
    { mangleStringIdentity("locale"), 1 },
    { mangleStringIdentity("array.id"), 3 },
    { mangleStringIdentity("array.subarr"), 9 },
    { mangleStringIdentity("array.subobj.id"), 2 },
    { mangleStringIdentity("array.subobj.name"), 1 },
    { mangleStringIdentity("array.id"), 2 }
  };

  auto const slice = json->slice();

  arangodb::iresearch::IResearchLinkMeta linkMeta;
  linkMeta._includeAllFields = true;

  arangodb::iresearch::FieldIterator it(slice, linkMeta);
  CHECK(it != arangodb::iresearch::FieldIterator::END);

  // default analyzer
  auto const expected_analyzer = irs::analysis::analyzers::get("identity", "");
  auto analyzers = arangodb::iresearch::getFeature<arangodb::iresearch::IResearchAnalyzerFeature>();
  CHECK(nullptr != analyzers);
  auto const expected_features = analyzers->get("identity")->features();

  while (it.valid()) {
    auto& field = *it;
    std::string const actualName = std::string(field.name());
    auto const expectedValue = expectedValues.find(actualName);
    REQUIRE(expectedValues.end() != expectedValue);

    auto& refs = expectedValue->second;
    if (!--refs) {
      expectedValues.erase(expectedValue);
    }

    auto& analyzer = dynamic_cast<irs::analysis::analyzer&>(field.get_tokens());
    CHECK(expected_features == field.features());
    CHECK(&expected_analyzer->type() == &analyzer.type());
    CHECK(linkMeta._boost == field.boost());

    ++it;
  }

  CHECK(expectedValues.empty());
  CHECK(it == arangodb::iresearch::FieldIterator::END);
}

SECTION("FieldIterator_traverse_complex_object_ordered_all_fields") {
  auto json = arangodb::velocypack::Parser::fromJson("{ \
    \"nested\": { \"foo\": \"str\" }, \
    \"keys\": [ \"1\",\"2\",\"3\",\"4\" ], \
    \"tokenizers\": [], \
    \"boost\": \"10\", \
    \"depth\": \"20\", \
    \"fields\": { \"fieldA\" : { \"name\" : \"a\" }, \"fieldB\" : { \"name\" : \"b\" } }, \
    \"listValuation\": \"ignored\", \
    \"locale\": \"ru_RU.KOI8-R\", \
    \"array\" : [ \
      { \"id\" : \"1\", \"subarr\" : [ \"1\", \"2\", \"3\" ], \"subobj\" : { \"id\" : \"1\" } }, \
      { \"subarr\" : [ \"4\", \"5\", \"6\" ], \"subobj\" : { \"name\" : \"foo\" }, \"id\" : \"2\" }, \
      { \"id\" : \"3\", \"subarr\" : [ \"7\", \"8\", \"9\" ], \"subobj\" : { \"id\" : \"2\" } } \
    ] \
  }");

  std::unordered_multiset<std::string> expectedValues {
    mangleStringIdentity("nested.foo"),
    mangleStringIdentity("keys[0]"),
    mangleStringIdentity("keys[1]"),
    mangleStringIdentity("keys[2]"),
    mangleStringIdentity("keys[3]"),
    mangleStringIdentity("boost"),
    mangleStringIdentity("depth"),
    mangleStringIdentity("fields.fieldA.name"),
    mangleStringIdentity("fields.fieldB.name"),
    mangleStringIdentity("listValuation"),
    mangleStringIdentity("locale"),

    mangleStringIdentity("array[0].id"),
    mangleStringIdentity("array[0].subarr[0]"),
    mangleStringIdentity("array[0].subarr[1]"),
    mangleStringIdentity("array[0].subarr[2]"),
    mangleStringIdentity("array[0].subobj.id"),

    mangleStringIdentity("array[1].subarr[0]"),
    mangleStringIdentity("array[1].subarr[1]"),
    mangleStringIdentity("array[1].subarr[2]"),
    mangleStringIdentity("array[1].subobj.name"),
    mangleStringIdentity("array[1].id"),

    mangleStringIdentity("array[2].id"),
    mangleStringIdentity("array[2].subarr[0]"),
    mangleStringIdentity("array[2].subarr[1]"),
    mangleStringIdentity("array[2].subarr[2]"),
    mangleStringIdentity("array[2].subobj.id")
  };

  auto const slice = json->slice();

  arangodb::iresearch::IResearchLinkMeta linkMeta;
  linkMeta._includeAllFields = true; // include all fields
  linkMeta._nestListValues = true; // allow indexes in field names

  // default analyzer
  auto const expected_analyzer = irs::analysis::analyzers::get("identity", "");
  auto analyzers = arangodb::iresearch::getFeature<arangodb::iresearch::IResearchAnalyzerFeature>();
  CHECK(nullptr != analyzers);
  auto const expected_features = analyzers->get("identity")->features();

  arangodb::iresearch::FieldIterator doc(slice, linkMeta);
  for (;doc.valid(); ++doc) {
    auto& field = *doc;
    std::string const actualName = std::string(field.name());
    CHECK(1 == expectedValues.erase(actualName));

    auto& analyzer = dynamic_cast<irs::analysis::analyzer&>(field.get_tokens());
    CHECK(expected_features == field.features());
    CHECK(&expected_analyzer->type() == &analyzer.type());
    CHECK(linkMeta._boost == field.boost());
  }

  CHECK(expectedValues.empty());
}

SECTION("FieldIterator_traverse_complex_object_ordered_filtered") {
  auto json = arangodb::velocypack::Parser::fromJson("{ \
    \"nested\": { \"foo\": \"str\" }, \
    \"keys\": [ \"1\",\"2\",\"3\",\"4\" ], \
    \"tokenizers\": [], \
    \"boost\": \"10\", \
    \"depth\": \"20\", \
    \"fields\": { \"fieldA\" : { \"name\" : \"a\" }, \"fieldB\" : { \"name\" : \"b\" } }, \
    \"listValuation\": \"ignored\", \
    \"locale\": \"ru_RU.KOI8-R\", \
    \"array\" : [ \
      { \"id\" : \"1\", \"subarr\" : [ \"1\", \"2\", \"3\" ], \"subobj\" : { \"id\" : \"1\" } }, \
      { \"subarr\" : [ \"4\", \"5\", \"6\" ], \"subobj\" : { \"name\" : \"foo\" }, \"id\" : \"2\" }, \
      { \"id\" : \"3\", \"subarr\" : [ \"7\", \"8\", \"9\" ], \"subobj\" : { \"id\" : \"2\" } } \
    ] \
  }");

  auto linkMetaJson = arangodb::velocypack::Parser::fromJson("{ \
    \"boost\" : 1, \
    \"includeAllFields\" : false, \
    \"nestListValues\" : true, \
    \"fields\" : { \"boost\" : { \"boost\" : 10 } }, \
    \"tokenizers\": [ \"identity\" ] \
  }");

  auto const slice = json->slice();

  arangodb::iresearch::IResearchLinkMeta linkMeta;

  std::string error;
  REQUIRE(linkMeta.init(linkMetaJson->slice(), error));

  arangodb::iresearch::FieldIterator it(slice, linkMeta);
  REQUIRE(it.valid());
  REQUIRE(it != arangodb::iresearch::FieldIterator::END);

  auto& value = *it;
  CHECK(mangleStringIdentity("boost") == value.name());
  const auto expected_analyzer = irs::analysis::analyzers::get("identity", "");
  auto analyzers = arangodb::iresearch::getFeature<arangodb::iresearch::IResearchAnalyzerFeature>();
  CHECK(nullptr != analyzers);
  auto const expected_features = analyzers->get("identity")->features();
  auto& analyzer = dynamic_cast<irs::analysis::analyzer&>(value.get_tokens());
  CHECK(expected_features == value.features());
  CHECK(&expected_analyzer->type() == &analyzer.type());
  CHECK(10.f == value.boost());

  ++it;
  CHECK(!it.valid());
  CHECK(it == arangodb::iresearch::FieldIterator::END);
}

SECTION("FieldIterator_traverse_complex_object_ordered_filtered") {
  auto json = arangodb::velocypack::Parser::fromJson("{ \
    \"nested\": { \"foo\": \"str\" }, \
    \"keys\": [ \"1\",\"2\",\"3\",\"4\" ], \
    \"tokenizers\": [], \
    \"boost\": \"10\", \
    \"depth\": \"20\", \
    \"fields\": { \"fieldA\" : { \"name\" : \"a\" }, \"fieldB\" : { \"name\" : \"b\" } }, \
    \"listValuation\": \"ignored\", \
    \"locale\": \"ru_RU.KOI8-R\", \
    \"array\" : [ \
      { \"id\" : \"1\", \"subarr\" : [ \"1\", \"2\", \"3\" ], \"subobj\" : { \"id\" : \"1\" } }, \
      { \"subarr\" : [ \"4\", \"5\", \"6\" ], \"subobj\" : { \"name\" : \"foo\" }, \"id\" : \"2\" }, \
      { \"id\" : \"3\", \"subarr\" : [ \"7\", \"8\", \"9\" ], \"subobj\" : { \"id\" : \"2\" } } \
    ] \
  }");

  auto const slice = json->slice();

  arangodb::iresearch::IResearchLinkMeta linkMeta;
  linkMeta._includeAllFields = false; // ignore all fields
  linkMeta._nestListValues = true; // allow indexes in field names

  arangodb::iresearch::FieldIterator it(slice, linkMeta);
  CHECK(!it.valid());
  CHECK(it == arangodb::iresearch::FieldIterator::END);
}

SECTION("FieldIterator_traverse_complex_object_ordered_empty_tokenizers") {
  auto json = arangodb::velocypack::Parser::fromJson("{ \
    \"nested\": { \"foo\": \"str\" }, \
    \"keys\": [ \"1\",\"2\",\"3\",\"4\" ], \
    \"tokenizers\": [], \
    \"boost\": \"10\", \
    \"depth\": \"20\", \
    \"fields\": { \"fieldA\" : { \"name\" : \"a\" }, \"fieldB\" : { \"name\" : \"b\" } }, \
    \"listValuation\": \"ignored\", \
    \"locale\": \"ru_RU.KOI8-R\", \
    \"array\" : [ \
      { \"id\" : \"1\", \"subarr\" : [ \"1\", \"2\", \"3\" ], \"subobj\" : { \"id\" : \"1\" } }, \
      { \"subarr\" : [ \"4\", \"5\", \"6\" ], \"subobj\" : { \"name\" : \"foo\" }, \"id\" : \"2\" }, \
      { \"id\" : \"3\", \"subarr\" : [ \"7\", \"8\", \"9\" ], \"subobj\" : { \"id\" : \"2\" } } \
    ] \
  }");

  auto const slice = json->slice();

  arangodb::iresearch::IResearchLinkMeta linkMeta;
  linkMeta._tokenizers.clear(); // clear all tokenizers
  linkMeta._includeAllFields = true; // include all fields

  arangodb::iresearch::FieldIterator it(slice, linkMeta);
  CHECK(!it.valid());
  CHECK(it == arangodb::iresearch::FieldIterator::END);
}

SECTION("FieldIterator_traverse_complex_object_ordered_check_value_types") {
  auto* analyzers = arangodb::iresearch::getFeature<arangodb::iresearch::IResearchAnalyzerFeature>();
  auto json = arangodb::velocypack::Parser::fromJson("{ \
    \"mustBeSkipped\" : {}, \
    \"stringValue\": \"string\", \
    \"nullValue\": null, \
    \"trueValue\": true, \
    \"falseValue\": false, \
    \"mustBeSkipped2\" : {}, \
    \"smallIntValue\": 10, \
    \"smallNegativeIntValue\": -5, \
    \"bigIntValue\": 2147483647, \
    \"bigNegativeIntValue\": -2147483648, \
    \"smallDoubleValue\": 20.123, \
    \"bigDoubleValue\": 1.79769e+308, \
    \"bigNegativeDoubleValue\": -1.79769e+308 \
  }");
  auto const slice = json->slice();

  arangodb::iresearch::IResearchLinkMeta linkMeta;
  linkMeta._tokenizers.emplace_back(analyzers->get("iresearch-document-empty")); // add tokenizer
  linkMeta._includeAllFields = true; // include all fields

  arangodb::iresearch::FieldIterator it(slice, linkMeta);
  CHECK(it != arangodb::iresearch::FieldIterator::END);

  // stringValue (with IdentityTokenizer)
  {
    auto& field = *it;
    CHECK(mangleStringIdentity("stringValue") == field.name());
    CHECK(1.f == field.boost());

    auto const expected_analyzer = irs::analysis::analyzers::get("identity", "");
    auto analyzers = arangodb::iresearch::getFeature<arangodb::iresearch::IResearchAnalyzerFeature>();
    CHECK(nullptr != analyzers);
    auto const expected_features = analyzers->get("identity")->features();
    auto& analyzer = dynamic_cast<irs::analysis::analyzer&>(field.get_tokens());
    CHECK(&expected_analyzer->type() == &analyzer.type());
    CHECK(expected_features == field.features());
  }

  ++it;
  REQUIRE(it.valid());
  REQUIRE(it != arangodb::iresearch::FieldIterator::END);

  // stringValue (with EmptyTokenizer)
  {
    auto& field = *it;
    CHECK(mangleString("stringValue", "iresearch-document-empty") == field.name());
    CHECK(1.f == field.boost());

    auto const expected_analyzer = irs::analysis::analyzers::get("iresearch-document-empty", "en");
    auto& analyzer = dynamic_cast<EmptyTokenizer&>(field.get_tokens());
    CHECK(&expected_analyzer->type() == &analyzer.type());
    CHECK(expected_analyzer->attributes().features() == field.features());
  }

  ++it;
  REQUIRE(it.valid());
  REQUIRE(it != arangodb::iresearch::FieldIterator::END);

  // nullValue
  {
    auto& field = *it;
    CHECK(mangleNull("nullValue") == field.name());
    CHECK(1.f == field.boost());

    auto& analyzer = dynamic_cast<irs::null_token_stream&>(field.get_tokens());
    CHECK(analyzer.next());
  }

  ++it;
  REQUIRE(it.valid());
  REQUIRE(it != arangodb::iresearch::FieldIterator::END);

  // trueValue
  {
    auto& field = *it;
    CHECK(mangleBool("trueValue") == field.name());
    CHECK(1.f == field.boost());

    auto& analyzer = dynamic_cast<irs::boolean_token_stream&>(field.get_tokens());
    CHECK(analyzer.next());
  }

  ++it;
  REQUIRE(it.valid());
  REQUIRE(it != arangodb::iresearch::FieldIterator::END);

  // falseValue
  {
    auto& field = *it;
    CHECK(mangleBool("falseValue") == field.name());
    CHECK(1.f == field.boost());

    auto& analyzer = dynamic_cast<irs::boolean_token_stream&>(field.get_tokens());
    CHECK(analyzer.next());
  }

  ++it;
  REQUIRE(it.valid());
  REQUIRE(it != arangodb::iresearch::FieldIterator::END);

  // smallIntValue
  {
    auto& field = *it;
    CHECK(mangleNumeric("smallIntValue") == field.name());
    CHECK(1.f == field.boost());

    auto& analyzer = dynamic_cast<irs::numeric_token_stream&>(field.get_tokens());
    CHECK(analyzer.next());
  }

  ++it;
  REQUIRE(it.valid());
  REQUIRE(it != arangodb::iresearch::FieldIterator::END);

  // smallNegativeIntValue
  {
    auto& field = *it;
    CHECK(mangleNumeric("smallNegativeIntValue") == field.name());
    CHECK(1.f == field.boost());

    auto& analyzer = dynamic_cast<irs::numeric_token_stream&>(field.get_tokens());
    CHECK(analyzer.next());
  }

  ++it;
  REQUIRE(it.valid());
  REQUIRE(it != arangodb::iresearch::FieldIterator::END);

  // bigIntValue
  {
    auto& field = *it;
    CHECK(mangleNumeric("bigIntValue") == field.name());
    CHECK(1.f == field.boost());

    auto& analyzer = dynamic_cast<irs::numeric_token_stream&>(field.get_tokens());
    CHECK(analyzer.next());
  }

  ++it;
  REQUIRE(it.valid());
  REQUIRE(it != arangodb::iresearch::FieldIterator::END);

  // bigNegativeIntValue
  {
    auto& field = *it;
    CHECK(mangleNumeric("bigNegativeIntValue") == field.name());
    CHECK(1.f == field.boost());

    auto& analyzer = dynamic_cast<irs::numeric_token_stream&>(field.get_tokens());
    CHECK(analyzer.next());
  }

  ++it;
  REQUIRE(it.valid());
  REQUIRE(it != arangodb::iresearch::FieldIterator::END);

  // smallDoubleValue
  {
    auto& field = *it;
    CHECK(mangleNumeric("smallDoubleValue") == field.name());
    CHECK(1.f == field.boost());

    auto& analyzer = dynamic_cast<irs::numeric_token_stream&>(field.get_tokens());
    CHECK(analyzer.next());
  }

  ++it;
  REQUIRE(it.valid());
  REQUIRE(it != arangodb::iresearch::FieldIterator::END);

  // bigDoubleValue
  {
    auto& field = *it;
    CHECK(mangleNumeric("bigDoubleValue") == field.name());
    CHECK(1.f == field.boost());

    auto& analyzer = dynamic_cast<irs::numeric_token_stream&>(field.get_tokens());
    CHECK(analyzer.next());
  }

  ++it;
  REQUIRE(it.valid());
  REQUIRE(it != arangodb::iresearch::FieldIterator::END);

  // bigNegativeDoubleValue
  {
    auto& field = *it;
    CHECK(mangleNumeric("bigNegativeDoubleValue") == field.name());
    CHECK(1.f == field.boost());

    auto& analyzer = dynamic_cast<irs::numeric_token_stream&>(field.get_tokens());
    CHECK(analyzer.next());
  }

  ++it;
  CHECK(!it.valid());
  CHECK(it == arangodb::iresearch::FieldIterator::END);
}

SECTION("FieldIterator_reset") {
  auto json0 = arangodb::velocypack::Parser::fromJson("{ \
    \"boost\": \"10\", \
    \"depth\": \"20\" \
  }");

  auto json1 = arangodb::velocypack::Parser::fromJson("{ \
    \"name\": \"foo\" \
  }");

  arangodb::iresearch::IResearchLinkMeta linkMeta;
  linkMeta._includeAllFields = true; // include all fields

  arangodb::iresearch::FieldIterator it(json0->slice(), linkMeta);
  REQUIRE(it.valid());

  {
    auto& value = *it;
    CHECK(mangleStringIdentity("boost") == value.name());
    const auto expected_analyzer = irs::analysis::analyzers::get("identity", "");
    auto analyzers = arangodb::iresearch::getFeature<arangodb::iresearch::IResearchAnalyzerFeature>();
    CHECK(nullptr != analyzers);
    auto const expected_features = analyzers->get("identity")->features();
    auto& analyzer = dynamic_cast<irs::analysis::analyzer&>(value.get_tokens());
    CHECK(expected_features == value.features());
    CHECK(&expected_analyzer->type() == &analyzer.type());
    CHECK(1.f == value.boost());
  }

  ++it;
  REQUIRE(it.valid());

  // depth (with IdentityTokenizer)
  {
    auto& value = *it;
    CHECK(mangleStringIdentity("depth") == value.name());
    const auto expected_analyzer = irs::analysis::analyzers::get("identity", "");
    auto analyzers = arangodb::iresearch::getFeature<arangodb::iresearch::IResearchAnalyzerFeature>();
    CHECK(nullptr != analyzers);
    auto const expected_features = analyzers->get("identity")->features();
    auto& analyzer = dynamic_cast<irs::analysis::analyzer&>(value.get_tokens());
    CHECK(expected_features == value.features());
    CHECK(&expected_analyzer->type() == &analyzer.type());
    CHECK(1.f == value.boost());
  }

  ++it;
  REQUIRE(!it.valid());

  it.reset(json1->slice(), linkMeta);
  REQUIRE(it.valid());

  {
    auto& value = *it;
    CHECK(mangleStringIdentity("name") == value.name());
    const auto expected_analyzer = irs::analysis::analyzers::get("identity", "");
    auto analyzers = arangodb::iresearch::getFeature<arangodb::iresearch::IResearchAnalyzerFeature>();
    CHECK(nullptr != analyzers);
    auto const expected_features = analyzers->get("identity")->features();
    auto& analyzer = dynamic_cast<irs::analysis::analyzer&>(value.get_tokens());
    CHECK(expected_features == value.features());
    CHECK(&expected_analyzer->type() == &analyzer.type());
    CHECK(1.f == value.boost());
  }

  ++it;
  REQUIRE(!it.valid());
}

SECTION("FieldIterator_traverse_complex_object_ordered_all_fields_custom_list_offset_prefix_suffix") {
  auto json = arangodb::velocypack::Parser::fromJson("{ \
    \"nested\": { \"foo\": \"str\" }, \
    \"keys\": [ \"1\",\"2\",\"3\",\"4\" ], \
    \"tokenizers\": [], \
    \"boost\": \"10\", \
    \"depth\": \"20\", \
    \"fields\": { \"fieldA\" : { \"name\" : \"a\" }, \"fieldB\" : { \"name\" : \"b\" } }, \
    \"listValuation\": \"ignored\", \
    \"locale\": \"ru_RU.KOI8-R\", \
    \"array\" : [ \
      { \"id\" : \"1\", \"subarr\" : [ \"1\", \"2\", \"3\" ], \"subobj\" : { \"id\" : \"1\" } }, \
      { \"subarr\" : [ \"4\", \"5\", \"6\" ], \"subobj\" : { \"name\" : \"foo\" }, \"id\" : \"2\" }, \
      { \"id\" : \"3\", \"subarr\" : [ \"7\", \"8\", \"9\" ], \"subobj\" : { \"id\" : \"2\" } } \
    ] \
  }");

  std::unordered_multiset<std::string> expectedValues {
    mangleStringIdentity("nested.foo"),
    mangleStringIdentity("keys[0]"),
    mangleStringIdentity("keys[1]"),
    mangleStringIdentity("keys[2]"),
    mangleStringIdentity("keys[3]"),
    mangleStringIdentity("boost"),
    mangleStringIdentity("depth"),
    mangleStringIdentity("fields.fieldA.name"),
    mangleStringIdentity("fields.fieldB.name"),
    mangleStringIdentity("listValuation"),
    mangleStringIdentity("locale"),

    mangleStringIdentity("array[0].id"),
    mangleStringIdentity("array[0].subarr[0]"),
    mangleStringIdentity("array[0].subarr[1]"),
    mangleStringIdentity("array[0].subarr[2]"),
    mangleStringIdentity("array[0].subobj.id"),

    mangleStringIdentity("array[1].subarr[0]"),
    mangleStringIdentity("array[1].subarr[1]"),
    mangleStringIdentity("array[1].subarr[2]"),
    mangleStringIdentity("array[1].subobj.name"),
    mangleStringIdentity("array[1].id"),

    mangleStringIdentity("array[2].id"),
    mangleStringIdentity("array[2].subarr[0]"),
    mangleStringIdentity("array[2].subarr[1]"),
    mangleStringIdentity("array[2].subarr[2]"),
    mangleStringIdentity("array[2].subobj.id")
  };

  auto const slice = json->slice();

  arangodb::iresearch::IResearchLinkMeta linkMeta;
  linkMeta._includeAllFields = true; // include all fields
  linkMeta._nestListValues = true; // allow indexes in field names

  arangodb::iresearch::FieldIterator it(slice, linkMeta);
  CHECK(it != arangodb::iresearch::FieldIterator::END);

  // default analyzer
  auto const expected_analyzer = irs::analysis::analyzers::get("identity", "");
  auto analyzers = arangodb::iresearch::getFeature<arangodb::iresearch::IResearchAnalyzerFeature>();
  CHECK(nullptr != analyzers);
  auto const expected_features = analyzers->get("identity")->features();

  for ( ; it != arangodb::iresearch::FieldIterator::END; ++it) {
    auto& field = *it;
    std::string const actualName = std::string(field.name());
    CHECK(1 == expectedValues.erase(actualName));

    auto& analyzer = dynamic_cast<irs::analysis::analyzer&>(field.get_tokens());
    CHECK(expected_features == field.features());
    CHECK(&expected_analyzer->type() == &analyzer.type());
    CHECK(linkMeta._boost == field.boost());
  }

  CHECK(expectedValues.empty());
  CHECK(it == arangodb::iresearch::FieldIterator::END);
}

SECTION("FieldIterator_traverse_complex_object_check_meta_inheritance") {
  auto json = arangodb::velocypack::Parser::fromJson("{ \
    \"nested\": { \"foo\": \"str\" }, \
    \"keys\": [ \"1\",\"2\",\"3\",\"4\" ], \
    \"tokenizers\": [], \
    \"boost\": \"10\", \
    \"depth\": 20, \
    \"fields\": { \"fieldA\" : { \"name\" : \"a\" }, \"fieldB\" : { \"name\" : \"b\" } }, \
    \"listValuation\": \"ignored\", \
    \"locale\": null, \
    \"array\" : [ \
      { \"id\" : 1, \"subarr\" : [ \"1\", \"2\", \"3\" ], \"subobj\" : { \"id\" : 1 } }, \
      { \"subarr\" : [ \"4\", \"5\", \"6\" ], \"subobj\" : { \"name\" : \"foo\" }, \"id\" : \"2\" }, \
      { \"id\" : 3, \"subarr\" : [ \"7\", \"8\", \"9\" ], \"subobj\" : { \"id\" : 2 } } \
    ] \
  }");

  auto const slice = json->slice();

  auto linkMetaJson = arangodb::velocypack::Parser::fromJson("{ \
    \"boost\" : 1, \
    \"includeAllFields\" : true, \
    \"nestListValues\" : true, \
    \"fields\" : { \
       \"boost\" : { \"boost\" : 10, \"tokenizers\": [ \"identity\" ] }, \
       \"keys\" : { \"nestListValues\" : false, \"tokenizers\": [ \"identity\" ] }, \
       \"depth\" : { \"boost\" : 5, \"nestListValues\" : true }, \
       \"fields\" : { \"includeAllFields\" : false, \"boost\" : 3, \"fields\" : { \"fieldA\" : { \"includeAllFields\" : true } } }, \
       \"listValuation\" : { \"includeAllFields\" : false }, \
       \"array\" : { \
         \"fields\" : { \"subarr\" : { \"nestListValues\" : false }, \"subobj\": { \"includeAllFields\" : false }, \"id\" : { \"boost\" : 2 } } \
       } \
     }, \
    \"tokenizers\": [ \"identity\", \"iresearch-document-empty\" ] \
  }");

  arangodb::iresearch::IResearchLinkMeta linkMeta;

  std::string error;
  REQUIRE(linkMeta.init(linkMetaJson->slice(), error));

  arangodb::iresearch::FieldIterator it(slice, linkMeta);
  REQUIRE(it.valid());
  REQUIRE(it != arangodb::iresearch::FieldIterator::END);

  // nested.foo (with IdentityTokenizer)
  {
    auto& value = *it;
    CHECK(mangleStringIdentity("nested.foo") == value.name());
    const auto expected_analyzer = irs::analysis::analyzers::get("identity", "");
    auto& analyzer = dynamic_cast<irs::analysis::analyzer&>(value.get_tokens());
    auto analyzers = arangodb::iresearch::getFeature<arangodb::iresearch::IResearchAnalyzerFeature>();
    CHECK(nullptr != analyzers);
    auto const expected_features = analyzers->get("identity")->features();
    CHECK(expected_features == value.features());
    CHECK(&expected_analyzer->type() == &analyzer.type());
    CHECK(1.f == value.boost());
  }

  ++it;
  REQUIRE(it.valid());
  REQUIRE(it != arangodb::iresearch::FieldIterator::END);

  // nested.foo (with EmptyTokenizer)
  {
    auto& value = *it;
    CHECK(mangleString("nested.foo", "iresearch-document-empty") == value.name());
    auto& analyzer = dynamic_cast<EmptyTokenizer&>(value.get_tokens());
    CHECK(!analyzer.next());
    CHECK(1.f == value.boost());
  }

  // keys[]
  for (size_t i = 0; i < 4; ++i) {
    ++it;
    REQUIRE(it.valid());
    REQUIRE(it != arangodb::iresearch::FieldIterator::END);

    auto& value = *it;
    CHECK(mangleStringIdentity("keys") == value.name());
    const auto expected_analyzer = irs::analysis::analyzers::get("identity", "");
    auto analyzers = arangodb::iresearch::getFeature<arangodb::iresearch::IResearchAnalyzerFeature>();
    CHECK(nullptr != analyzers);
    auto const expected_features = analyzers->get("identity")->features();
    auto& analyzer = dynamic_cast<irs::analysis::analyzer&>(value.get_tokens());
    CHECK(expected_features == value.features());
    CHECK(&expected_analyzer->type() == &analyzer.type());
    CHECK(1.f == value.boost());

  }

  ++it;
  REQUIRE(it.valid());
  REQUIRE(it != arangodb::iresearch::FieldIterator::END);

  // boost
  {
    auto& value = *it;
    CHECK(mangleStringIdentity("boost") == value.name());
    const auto expected_analyzer = irs::analysis::analyzers::get("identity", "");
    auto analyzers = arangodb::iresearch::getFeature<arangodb::iresearch::IResearchAnalyzerFeature>();
    CHECK(nullptr != analyzers);
    auto const expected_features = analyzers->get("identity")->features();
    auto& analyzer = dynamic_cast<irs::analysis::analyzer&>(value.get_tokens());
    CHECK(expected_features == value.features());
    CHECK(&expected_analyzer->type() == &analyzer.type());
    CHECK(10.f == value.boost());
  }

  ++it;
  REQUIRE(it.valid());
  REQUIRE(it != arangodb::iresearch::FieldIterator::END);

  // depth
  {
    auto& value = *it;
    CHECK(mangleNumeric("depth") == value.name());
    auto& analyzer = dynamic_cast<irs::numeric_token_stream&>(value.get_tokens());
    CHECK(analyzer.next());
    CHECK(5.f == value.boost());
  }

  ++it;
  REQUIRE(it.valid());
  REQUIRE(it != arangodb::iresearch::FieldIterator::END);

  // fields.fieldA (with IdenitytTokenizer)
  {
    auto& value = *it;
    CHECK(mangleStringIdentity("fields.fieldA.name") == value.name());
    const auto expected_analyzer = irs::analysis::analyzers::get("identity", "");
    auto analyzers = arangodb::iresearch::getFeature<arangodb::iresearch::IResearchAnalyzerFeature>();
    CHECK(nullptr != analyzers);
    auto const expected_features = analyzers->get("identity")->features();
    auto& analyzer = dynamic_cast<irs::analysis::analyzer&>(value.get_tokens());
    CHECK(expected_features == value.features());
    CHECK(&expected_analyzer->type() == &analyzer.type());
    CHECK(3.f == value.boost());
  }

  ++it;
  REQUIRE(it.valid());
  REQUIRE(it != arangodb::iresearch::FieldIterator::END);

  // fields.fieldA (with EmptyTokenizer)
  {
    auto& value = *it;
    CHECK(mangleString("fields.fieldA.name", "iresearch-document-empty") == value.name());
    auto& analyzer = dynamic_cast<EmptyTokenizer&>(value.get_tokens());
    CHECK(!analyzer.next());
    CHECK(3.f == value.boost());
  }

  ++it;
  REQUIRE(it.valid());
  REQUIRE(it != arangodb::iresearch::FieldIterator::END);

  // listValuation (with IdenitytTokenizer)
  {
    auto& value = *it;
    CHECK(mangleStringIdentity("listValuation") == value.name());
    const auto expected_analyzer = irs::analysis::analyzers::get("identity", "");
    auto analyzers = arangodb::iresearch::getFeature<arangodb::iresearch::IResearchAnalyzerFeature>();
    CHECK(nullptr != analyzers);
    auto const expected_features = analyzers->get("identity")->features();
    auto& analyzer = dynamic_cast<irs::analysis::analyzer&>(value.get_tokens());
    CHECK(expected_features == value.features());
    CHECK(&expected_analyzer->type() == &analyzer.type());
    CHECK(1.f == value.boost());
  }

  ++it;
  REQUIRE(it.valid());
  REQUIRE(it != arangodb::iresearch::FieldIterator::END);

  // listValuation (with EmptyTokenizer)
  {
    auto& value = *it;
    CHECK(mangleString("listValuation", "iresearch-document-empty") == value.name());
    auto& analyzer = dynamic_cast<EmptyTokenizer&>(value.get_tokens());
    CHECK(!analyzer.next());
    CHECK(1.f == value.boost());
  }

  ++it;
  REQUIRE(it.valid());
  REQUIRE(it != arangodb::iresearch::FieldIterator::END);

  // locale
  {
    auto& value = *it;
    CHECK(mangleNull("locale") == value.name());
    auto& analyzer = dynamic_cast<irs::null_token_stream&>(value.get_tokens());
    CHECK(analyzer.next());
    CHECK(1.f == value.boost());
  }

  ++it;
  REQUIRE(it.valid());
  REQUIRE(it != arangodb::iresearch::FieldIterator::END);

  // array[0].id
  {
    auto& value = *it;
    CHECK(mangleNumeric("array[0].id") == value.name());
    auto& analyzer = dynamic_cast<irs::numeric_token_stream&>(value.get_tokens());
    CHECK(analyzer.next());
    CHECK(2.f == value.boost());
  }


  // array[0].subarr[0-2]
  for (size_t i = 0; i < 3; ++i) {
    ++it;
    REQUIRE(it.valid());
    REQUIRE(it != arangodb::iresearch::FieldIterator::END);

    // IdentityTokenizer
    {
      auto& value = *it;
      CHECK(mangleStringIdentity("array[0].subarr") == value.name());
      const auto expected_analyzer = irs::analysis::analyzers::get("identity", "");
      auto analyzers = arangodb::iresearch::getFeature<arangodb::iresearch::IResearchAnalyzerFeature>();
      CHECK(nullptr != analyzers);
      auto const expected_features = analyzers->get("identity")->features();
      auto& analyzer = dynamic_cast<irs::analysis::analyzer&>(value.get_tokens());
      CHECK(expected_features == value.features());
      CHECK(&expected_analyzer->type() == &analyzer.type());
      CHECK(1.f == value.boost());
    }

    ++it;
    REQUIRE(it.valid());
    REQUIRE(it != arangodb::iresearch::FieldIterator::END);

    // EmptyTokenizer
    {
      auto& value = *it;
      CHECK(mangleString("array[0].subarr", "iresearch-document-empty") == value.name());
      auto& analyzer = dynamic_cast<EmptyTokenizer&>(value.get_tokens());
      CHECK(!analyzer.next());
      CHECK(1.f == value.boost());
    }
  }

   // array[1].subarr[0-2]
  for (size_t i = 0; i < 3; ++i) {
    ++it;
    REQUIRE(it.valid());
    REQUIRE(it != arangodb::iresearch::FieldIterator::END);

    // IdentityTokenizer
    {
      auto& value = *it;
      CHECK(mangleStringIdentity("array[1].subarr") == value.name());
      const auto expected_analyzer = irs::analysis::analyzers::get("identity", "");
      auto analyzers = arangodb::iresearch::getFeature<arangodb::iresearch::IResearchAnalyzerFeature>();
      CHECK(nullptr != analyzers);
      auto const expected_features = analyzers->get("identity")->features();
      auto& analyzer = dynamic_cast<irs::analysis::analyzer&>(value.get_tokens());
      CHECK(expected_features == value.features());
      CHECK(&expected_analyzer->type() == &analyzer.type());
      CHECK(1.f == value.boost());
    }

    ++it;
    REQUIRE(it.valid());
    REQUIRE(it != arangodb::iresearch::FieldIterator::END);

    // EmptyTokenizer
    {
      auto& value = *it;
      CHECK(mangleString("array[1].subarr", "iresearch-document-empty") == value.name());
      auto& analyzer = dynamic_cast<EmptyTokenizer&>(value.get_tokens());
      CHECK(!analyzer.next());
      CHECK(1.f == value.boost());
    }
  }

  ++it;
  REQUIRE(it.valid());
  REQUIRE(it != arangodb::iresearch::FieldIterator::END);

  // array[1].id (IdentityTokenizer)
  {
    auto& value = *it;
    CHECK(mangleStringIdentity("array[1].id") == value.name());
    const auto expected_analyzer = irs::analysis::analyzers::get("identity", "");
    auto analyzers = arangodb::iresearch::getFeature<arangodb::iresearch::IResearchAnalyzerFeature>();
    CHECK(nullptr != analyzers);
    auto const expected_features = analyzers->get("identity")->features();
    auto& analyzer = dynamic_cast<irs::analysis::analyzer&>(value.get_tokens());
    CHECK(expected_features == value.features());
    CHECK(&expected_analyzer->type() == &analyzer.type());
    CHECK(2.f == value.boost());
  }

  ++it;
  REQUIRE(it.valid());
  REQUIRE(it != arangodb::iresearch::FieldIterator::END);

  // array[1].id (EmptyTokenizer)
  {
    auto& value = *it;
    CHECK(mangleString("array[1].id", "iresearch-document-empty") == value.name());
    auto& analyzer = dynamic_cast<EmptyTokenizer&>(value.get_tokens());
    CHECK(!analyzer.next());
    CHECK(2.f == value.boost());
  }

  ++it;
  REQUIRE(it.valid());
  REQUIRE(it != arangodb::iresearch::FieldIterator::END);

  // array[2].id (IdentityTokenizer)
  {
    auto& value = *it;
    CHECK(mangleNumeric("array[2].id") == value.name());
    auto& analyzer = dynamic_cast<irs::numeric_token_stream&>(value.get_tokens());
    CHECK(analyzer.next());
    CHECK(2.f == value.boost());
  }

  // array[2].subarr[0-2]
  for (size_t i = 0; i < 3; ++i) {
    ++it;
    REQUIRE(it.valid());
    REQUIRE(it != arangodb::iresearch::FieldIterator::END);

    // IdentityTokenizer
    {
      auto& value = *it;
      CHECK(mangleStringIdentity("array[2].subarr") == value.name());
      const auto expected_analyzer = irs::analysis::analyzers::get("identity", "");
      auto analyzers = arangodb::iresearch::getFeature<arangodb::iresearch::IResearchAnalyzerFeature>();
      CHECK(nullptr != analyzers);
      auto const expected_features = analyzers->get("identity")->features();
      auto& analyzer = dynamic_cast<irs::analysis::analyzer&>(value.get_tokens());
      CHECK(expected_features == value.features());
      CHECK(&expected_analyzer->type() == &analyzer.type());
      CHECK(1.f == value.boost());
    }

    ++it;
    REQUIRE(it.valid());
    REQUIRE(it != arangodb::iresearch::FieldIterator::END);

    // EmptyTokenizer
    {
      auto& value = *it;
      CHECK(mangleString("array[2].subarr", "iresearch-document-empty") == value.name());
      auto& analyzer = dynamic_cast<EmptyTokenizer&>(value.get_tokens());
      CHECK(!analyzer.next());
      CHECK(1.f == value.boost());
    }
  }

  ++it;
  CHECK(!it.valid());
  CHECK(it == arangodb::iresearch::FieldIterator::END);
}

SECTION("FieldIterator_nullptr_tokenizer") {
  arangodb::iresearch::IResearchAnalyzerFeature analyzers(nullptr);
  auto json = arangodb::velocypack::Parser::fromJson("{ \
    \"stringValue\": \"string\" \
  }");
  auto const slice = json->slice();

  // register analizers with feature
  {
    // ensure that there will be no exception on 'start'
    InvalidTokenizer::returnNullFromMake = false;

    analyzers.start();
    analyzers.erase("empty");
    analyzers.erase("invalid");

    // ensure that there will be no exception on 'emplace'
    InvalidTokenizer::returnNullFromMake = false;

    analyzers.emplace("empty", "iresearch-document-empty", "en");
    analyzers.emplace("invalid", "iresearch-document-invalid", "en");
  }

  // last tokenizer invalid
  {
    arangodb::iresearch::IResearchLinkMeta linkMeta;
    linkMeta._tokenizers.emplace_back(analyzers.get("empty")); // add tokenizer
    linkMeta._tokenizers.emplace_back(analyzers.get("invalid")); // add tokenizer
    linkMeta._includeAllFields = true; // include all fields

    // acquire tokenizer, another one should be created
    auto tokenizer = linkMeta._tokenizers.back()->get(); // cached instance should have been acquired

    arangodb::iresearch::FieldIterator it(slice, linkMeta);
    REQUIRE(it.valid());
    REQUIRE(it != arangodb::iresearch::FieldIterator::END);

    // stringValue (with IdentityTokenizer)
    {
      auto& field = *it;
      CHECK(mangleStringIdentity("stringValue") == field.name());
      CHECK(1.f == field.boost());

      auto const expected_analyzer = irs::analysis::analyzers::get("identity", "");
      auto analyzers = arangodb::iresearch::getFeature<arangodb::iresearch::IResearchAnalyzerFeature>();
      CHECK(nullptr != analyzers);
      auto const expected_features = analyzers->get("identity")->features();
      auto& analyzer = dynamic_cast<irs::analysis::analyzer&>(field.get_tokens());
      CHECK(&expected_analyzer->type() == &analyzer.type());
      CHECK(expected_features == field.features());
    }

    ++it;
    REQUIRE(it.valid());
    REQUIRE(arangodb::iresearch::FieldIterator::END != it);

    // stringValue (with EmptyTokenizer)
    {
      auto& field = *it;
      CHECK(mangleString("stringValue", "empty") == field.name());
      CHECK(1.f == field.boost());

      auto const expected_analyzer = irs::analysis::analyzers::get("iresearch-document-empty", "en");
      auto& analyzer = dynamic_cast<EmptyTokenizer&>(field.get_tokens());
      CHECK(&expected_analyzer->type() == &analyzer.type());
      CHECK(expected_analyzer->attributes().features()== field.features());
    }

    ++it;
    REQUIRE(!it.valid());
    REQUIRE(arangodb::iresearch::FieldIterator::END == it);

    tokenizer->reset(irs::string_ref::nil); // ensure that acquired 'tokenizer' will not be optimized out
  }

  // first tokenizer is invalid
  {
    arangodb::iresearch::IResearchLinkMeta linkMeta;
    linkMeta._tokenizers.clear();
    linkMeta._tokenizers.emplace_back(analyzers.get("invalid")); // add tokenizer
    linkMeta._tokenizers.emplace_back(analyzers.get("empty")); // add tokenizer
    linkMeta._includeAllFields = true; // include all fields

    // acquire tokenizer, another one should be created
    auto tokenizer = linkMeta._tokenizers.front()->get(); // cached instance should have been aquired

    arangodb::iresearch::FieldIterator it(slice, linkMeta);
    REQUIRE(it.valid());
    REQUIRE(it != arangodb::iresearch::FieldIterator::END);

    // stringValue (with EmptyTokenizer)
    {
      auto& field = *it;
      CHECK(mangleString("stringValue", "empty") == field.name());
      CHECK(1.f == field.boost());

      auto const expected_analyzer = irs::analysis::analyzers::get("iresearch-document-empty", "en");
      auto& analyzer = dynamic_cast<EmptyTokenizer&>(field.get_tokens());
      CHECK(&expected_analyzer->type() == &analyzer.type());
      CHECK(expected_analyzer->attributes().features() == field.features());
    }

    ++it;
    REQUIRE(!it.valid());
    REQUIRE(arangodb::iresearch::FieldIterator::END == it);

    tokenizer->reset(irs::string_ref::nil); // ensure that acquired 'tokenizer' will not be optimized out
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate tests
////////////////////////////////////////////////////////////////////////////////

}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
