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

#if USE_ENTERPRISE
  #include "Enterprise/Ldap/LdapFeature.h"
#endif

#include "GeneralServer/AuthenticationFeature.h"
#include "IResearch/IResearchAnalyzerFeature.h"
#include "IResearch/IResearchCommon.h"
#include "IResearch/IResearchDocument.h"
#include "IResearch/IResearchFilterFactory.h"
#include "IResearch/IResearchLinkMeta.h"
#include "IResearch/IResearchPrimaryKeyFilter.h"
#include "IResearch/IResearchKludge.h"
#include "Logger/Logger.h"
#include "Logger/LogTopic.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "RestServer/SystemDatabaseFeature.h"
#include "Sharding/ShardingFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"

#include "velocypack/Iterator.h"
#include "velocypack/Builder.h"
#include "velocypack/Parser.h"
#include "velocypack/velocypack-aliases.h"

#include "analysis/analyzers.hpp"
#include "analysis/token_streams.hpp"
#include "index/directory_reader.hpp"
#include "index/index_writer.hpp"
#include "store/memory_directory.hpp"

#include <memory>

namespace {

struct TestAttribute: public irs::attribute {
  DECLARE_ATTRIBUTE_TYPE();
};

DEFINE_ATTRIBUTE_TYPE(TestAttribute);

class EmptyAnalyzer: public irs::analysis::analyzer {
 public:
  DECLARE_ANALYZER_TYPE();
  EmptyAnalyzer(): irs::analysis::analyzer(EmptyAnalyzer::type()) {
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

DEFINE_ANALYZER_TYPE_NAMED(EmptyAnalyzer, "iresearch-document-empty");
REGISTER_ANALYZER_JSON(EmptyAnalyzer, EmptyAnalyzer::make);

class InvalidAnalyzer: public irs::analysis::analyzer {
 public:
  static bool returnNullFromMake;

  DECLARE_ANALYZER_TYPE();
  InvalidAnalyzer(): irs::analysis::analyzer(InvalidAnalyzer::type()) {
    _attrs.emplace(_attr);
  }
  virtual irs::attribute_view const& attributes() const NOEXCEPT override { return _attrs; }
  static ptr make(irs::string_ref const&) {
    if (!returnNullFromMake) {
      PTR_NAMED(InvalidAnalyzer, ptr);
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

bool InvalidAnalyzer::returnNullFromMake = false;

DEFINE_ANALYZER_TYPE_NAMED(InvalidAnalyzer, "iresearch-document-invalid");
REGISTER_ANALYZER_JSON(InvalidAnalyzer, InvalidAnalyzer::make);

}

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

struct IResearchDocumentSetup {
  StorageEngineMock engine;
  arangodb::application_features::ApplicationServer server;
  std::unique_ptr<TRI_vocbase_t> system;
  std::vector<std::pair<arangodb::application_features::ApplicationFeature*, bool>> features;

  IResearchDocumentSetup(): engine(server), server(nullptr, nullptr) {
    arangodb::EngineSelectorFeature::ENGINE = &engine;

    arangodb::tests::init();

    // suppress INFO {authentication} Authentication is turned on (system only), authentication for unix sockets is turned on
    arangodb::LogTopic::setLogLevel(arangodb::Logger::AUTHENTICATION.name(), arangodb::LogLevel::WARN);

    // setup required application features
    features.emplace_back(new arangodb::AuthenticationFeature(server), true);
    features.emplace_back(new arangodb::DatabaseFeature(server), false);
    features.emplace_back(new arangodb::QueryRegistryFeature(server), false); // required for constructing TRI_vocbase_t
    arangodb::application_features::ApplicationServer::server->addFeature(features.back().first); // need QueryRegistryFeature feature to be added now in order to create the system database
    system = irs::memory::make_unique<TRI_vocbase_t>(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 0, TRI_VOC_SYSTEM_DATABASE);
    features.emplace_back(new arangodb::SystemDatabaseFeature(server, system.get()), false); // required for IResearchAnalyzerFeature
    features.emplace_back(new arangodb::aql::AqlFunctionFeature(server), true); // required for IResearchAnalyzerFeature
    features.emplace_back(new arangodb::ShardingFeature(server), true);
    features.emplace_back(new arangodb::iresearch::IResearchAnalyzerFeature(server), true);

    #if USE_ENTERPRISE
      features.emplace_back(new arangodb::LdapFeature(server), false); // required for AuthenticationFeature with USE_ENTERPRISE
    #endif

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

    auto* analyzers = arangodb::application_features::ApplicationServer::lookupFeature<
      arangodb::iresearch::IResearchAnalyzerFeature
    >();

    // ensure that there will be no exception on 'emplace'
    InvalidAnalyzer::returnNullFromMake = false;

    analyzers->emplace("iresearch-document-empty", "iresearch-document-empty", "en", irs::flags{ TestAttribute::type() }); // cache analyzer
    analyzers->emplace("iresearch-document-invalid", "iresearch-document-invalid", "en", irs::flags{ TestAttribute::type() }); // cache analyzer

    // suppress log messages since tests check error conditions
    arangodb::LogTopic::setLogLevel(arangodb::iresearch::TOPIC.name(), arangodb::LogLevel::FATAL);
    irs::logger::output_le(iresearch::logger::IRL_FATAL, stderr);
  }

  ~IResearchDocumentSetup() {
    system.reset(); // destroy before reseting the 'ENGINE'
    arangodb::LogTopic::setLogLevel(arangodb::iresearch::TOPIC.name(), arangodb::LogLevel::DEFAULT);
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

    arangodb::LogTopic::setLogLevel(arangodb::Logger::AUTHENTICATION.name(), arangodb::LogLevel::DEFAULT);
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
  field._features = &features;
  field._analyzer = nullptr;

  // check CID value
  {
    TRI_voc_cid_t cid = 10;
    arangodb::iresearch::Field::setCidValue(field, cid, arangodb::iresearch::Field::init_stream_t());
    CHECK(arangodb::iresearch::DocumentPrimaryKey::CID() == field._name);
    CHECK(&irs::flags::empty_instance() == field._features);

    auto* stream = dynamic_cast<irs::string_token_stream*>(field._analyzer.get());
    REQUIRE(nullptr != stream);
    CHECK(stream->next());
    CHECK(!stream->next());

    arangodb::iresearch::Field::setCidValue(field, cid);
    CHECK(arangodb::iresearch::DocumentPrimaryKey::CID() == field._name);
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
    \"analyzers\": [], \
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
  auto const expected_analyzer =  irs::analysis::analyzers::get("identity", irs::text_format::json, "");
  auto* analyzers = arangodb::application_features::ApplicationServer::lookupFeature<
    arangodb::iresearch::IResearchAnalyzerFeature
  >();
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

    ++it;
  }

  CHECK(expectedValues.empty());
  CHECK(it == arangodb::iresearch::FieldIterator::END);
}

SECTION("FieldIterator_traverse_complex_object_all_fields") {
  auto json = arangodb::velocypack::Parser::fromJson("{ \
    \"nested\": { \"foo\": \"str\" }, \
    \"keys\": [ \"1\",\"2\",\"3\",\"4\" ], \
    \"analyzers\": [], \
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
  auto const expected_analyzer = irs::analysis::analyzers::get("identity", irs::text_format::json, "");
  auto* analyzers = arangodb::application_features::ApplicationServer::lookupFeature<
    arangodb::iresearch::IResearchAnalyzerFeature
  >();
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

    ++it;
  }

  CHECK(expectedValues.empty());
  CHECK(it == arangodb::iresearch::FieldIterator::END);
}

SECTION("FieldIterator_traverse_complex_object_ordered_all_fields") {
  auto json = arangodb::velocypack::Parser::fromJson("{ \
    \"nested\": { \"foo\": \"str\" }, \
    \"keys\": [ \"1\",\"2\",\"3\",\"4\" ], \
    \"analyzers\": [], \
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
  linkMeta._trackListPositions = true; // allow indexes in field names

  // default analyzer
  auto const expected_analyzer = irs::analysis::analyzers::get("identity", irs::text_format::json, "");
  auto* analyzers = arangodb::application_features::ApplicationServer::lookupFeature<
    arangodb::iresearch::IResearchAnalyzerFeature
  >();
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
  }

  CHECK(expectedValues.empty());
}

SECTION("FieldIterator_traverse_complex_object_ordered_filtered") {
  auto json = arangodb::velocypack::Parser::fromJson("{ \
    \"nested\": { \"foo\": \"str\" }, \
    \"keys\": [ \"1\",\"2\",\"3\",\"4\" ], \
    \"analyzers\": [], \
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
    \"includeAllFields\" : false, \
    \"trackListPositions\" : true, \
    \"fields\" : { \"boost\" : { } }, \
    \"analyzers\": [ \"identity\" ] \
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
  const auto expected_analyzer = irs::analysis::analyzers::get("identity", irs::text_format::json, "");
  auto* analyzers = arangodb::application_features::ApplicationServer::lookupFeature<
    arangodb::iresearch::IResearchAnalyzerFeature
  >();
  CHECK(nullptr != analyzers);
  auto const expected_features = analyzers->get("identity")->features();
  auto& analyzer = dynamic_cast<irs::analysis::analyzer&>(value.get_tokens());
  CHECK(expected_features == value.features());
  CHECK(&expected_analyzer->type() == &analyzer.type());

  ++it;
  CHECK(!it.valid());
  CHECK(it == arangodb::iresearch::FieldIterator::END);
}

SECTION("FieldIterator_traverse_complex_object_ordered_filtered") {
  auto json = arangodb::velocypack::Parser::fromJson("{ \
    \"nested\": { \"foo\": \"str\" }, \
    \"keys\": [ \"1\",\"2\",\"3\",\"4\" ], \
    \"analyzers\": [], \
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
  linkMeta._trackListPositions = true; // allow indexes in field names

  arangodb::iresearch::FieldIterator it(slice, linkMeta);
  CHECK(!it.valid());
  CHECK(it == arangodb::iresearch::FieldIterator::END);
}

SECTION("FieldIterator_traverse_complex_object_ordered_empty_analyzers") {
  auto json = arangodb::velocypack::Parser::fromJson("{ \
    \"nested\": { \"foo\": \"str\" }, \
    \"keys\": [ \"1\",\"2\",\"3\",\"4\" ], \
    \"analyzers\": [], \
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
  linkMeta._analyzers.clear(); // clear all analyzers
  linkMeta._includeAllFields = true; // include all fields

  arangodb::iresearch::FieldIterator it(slice, linkMeta);
  CHECK(!it.valid());
  CHECK(it == arangodb::iresearch::FieldIterator::END);
}

SECTION("FieldIterator_traverse_complex_object_ordered_check_value_types") {
  auto* analyzers = arangodb::application_features::ApplicationServer::lookupFeature<
    arangodb::iresearch::IResearchAnalyzerFeature
  >();
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
  linkMeta._analyzers.emplace_back(analyzers->get("iresearch-document-empty")); // add analyzer
  linkMeta._includeAllFields = true; // include all fields

  arangodb::iresearch::FieldIterator it(slice, linkMeta);
  CHECK(it != arangodb::iresearch::FieldIterator::END);

  // stringValue (with IdentityAnalyzer)
  {
    auto& field = *it;
    CHECK(mangleStringIdentity("stringValue") == field.name());
    auto const expected_analyzer = irs::analysis::analyzers::get("identity", irs::text_format::json, "");
    auto* analyzers = arangodb::application_features::ApplicationServer::lookupFeature<
      arangodb::iresearch::IResearchAnalyzerFeature
    >();
    CHECK(nullptr != analyzers);
    auto const expected_features = analyzers->get("identity")->features();
    auto& analyzer = dynamic_cast<irs::analysis::analyzer&>(field.get_tokens());
    CHECK(&expected_analyzer->type() == &analyzer.type());
    CHECK(expected_features == field.features());
  }

  ++it;
  REQUIRE(it.valid());
  REQUIRE(it != arangodb::iresearch::FieldIterator::END);

  // stringValue (with EmptyAnalyzer)
  {
    auto& field = *it;
    CHECK(mangleString("stringValue", "iresearch-document-empty") == field.name());
    auto const expected_analyzer = irs::analysis::analyzers::get("iresearch-document-empty", irs::text_format::json, "en");
    auto& analyzer = dynamic_cast<EmptyAnalyzer&>(field.get_tokens());
    CHECK(&expected_analyzer->type() == &analyzer.type());
    CHECK(irs::flags({TestAttribute::type()}) == field.features());
  }

  ++it;
  REQUIRE(it.valid());
  REQUIRE(it != arangodb::iresearch::FieldIterator::END);

  // nullValue
  {
    auto& field = *it;
    CHECK(mangleNull("nullValue") == field.name());
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
    const auto expected_analyzer = irs::analysis::analyzers::get("identity", irs::text_format::json, "");
    auto* analyzers = arangodb::application_features::ApplicationServer::lookupFeature<
      arangodb::iresearch::IResearchAnalyzerFeature
    >();
    CHECK(nullptr != analyzers);
    auto const expected_features = analyzers->get("identity")->features();
    auto& analyzer = dynamic_cast<irs::analysis::analyzer&>(value.get_tokens());
    CHECK(expected_features == value.features());
    CHECK(&expected_analyzer->type() == &analyzer.type());
  }

  ++it;
  REQUIRE(it.valid());

  // depth (with IdentityAnalyzer)
  {
    auto& value = *it;
    CHECK(mangleStringIdentity("depth") == value.name());
    const auto expected_analyzer = irs::analysis::analyzers::get("identity", irs::text_format::json, "");
    auto* analyzers = arangodb::application_features::ApplicationServer::lookupFeature<
      arangodb::iresearch::IResearchAnalyzerFeature
    >();
    CHECK(nullptr != analyzers);
    auto const expected_features = analyzers->get("identity")->features();
    auto& analyzer = dynamic_cast<irs::analysis::analyzer&>(value.get_tokens());
    CHECK(expected_features == value.features());
    CHECK(&expected_analyzer->type() == &analyzer.type());
  }

  ++it;
  REQUIRE(!it.valid());

  it.reset(json1->slice(), linkMeta);
  REQUIRE(it.valid());

  {
    auto& value = *it;
    CHECK(mangleStringIdentity("name") == value.name());
    const auto expected_analyzer = irs::analysis::analyzers::get("identity", irs::text_format::json, "");
    auto* analyzers = arangodb::application_features::ApplicationServer::lookupFeature<
      arangodb::iresearch::IResearchAnalyzerFeature
    >();
    CHECK(nullptr != analyzers);
    auto const expected_features = analyzers->get("identity")->features();
    auto& analyzer = dynamic_cast<irs::analysis::analyzer&>(value.get_tokens());
    CHECK(expected_features == value.features());
    CHECK(&expected_analyzer->type() == &analyzer.type());
  }

  ++it;
  REQUIRE(!it.valid());
}

SECTION("FieldIterator_traverse_complex_object_ordered_all_fields_custom_list_offset_prefix_suffix") {
  auto json = arangodb::velocypack::Parser::fromJson("{ \
    \"nested\": { \"foo\": \"str\" }, \
    \"keys\": [ \"1\",\"2\",\"3\",\"4\" ], \
    \"analyzers\": [], \
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
  linkMeta._trackListPositions = true; // allow indexes in field names

  arangodb::iresearch::FieldIterator it(slice, linkMeta);
  CHECK(it != arangodb::iresearch::FieldIterator::END);

  // default analyzer
  auto const expected_analyzer = irs::analysis::analyzers::get("identity", irs::text_format::json, "");
  auto* analyzers = arangodb::application_features::ApplicationServer::lookupFeature<
    arangodb::iresearch::IResearchAnalyzerFeature
  >();
  CHECK(nullptr != analyzers);
  auto const expected_features = analyzers->get("identity")->features();

  for ( ; it != arangodb::iresearch::FieldIterator::END; ++it) {
    auto& field = *it;
    std::string const actualName = std::string(field.name());
    CHECK(1 == expectedValues.erase(actualName));

    auto& analyzer = dynamic_cast<irs::analysis::analyzer&>(field.get_tokens());
    CHECK(expected_features == field.features());
    CHECK(&expected_analyzer->type() == &analyzer.type());
  }

  CHECK(expectedValues.empty());
  CHECK(it == arangodb::iresearch::FieldIterator::END);
}

SECTION("FieldIterator_traverse_complex_object_check_meta_inheritance") {
  auto json = arangodb::velocypack::Parser::fromJson("{ \
    \"nested\": { \"foo\": \"str\" }, \
    \"keys\": [ \"1\",\"2\",\"3\",\"4\" ], \
    \"analyzers\": [], \
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
    \"includeAllFields\" : true, \
    \"trackListPositions\" : true, \
    \"fields\" : { \
       \"boost\" : { \"analyzers\": [ \"identity\" ] }, \
       \"keys\" : { \"trackListPositions\" : false, \"analyzers\": [ \"identity\" ] }, \
       \"depth\" : { \"trackListPositions\" : true }, \
       \"fields\" : { \"includeAllFields\" : false, \"fields\" : { \"fieldA\" : { \"includeAllFields\" : true } } }, \
       \"listValuation\" : { \"includeAllFields\" : false }, \
       \"array\" : { \
         \"fields\" : { \"subarr\" : { \"trackListPositions\" : false }, \"subobj\": { \"includeAllFields\" : false }, \"id\" : { } } \
       } \
     }, \
    \"analyzers\": [ \"identity\", \"iresearch-document-empty\" ] \
  }");

  arangodb::iresearch::IResearchLinkMeta linkMeta;

  std::string error;
  REQUIRE(linkMeta.init(linkMetaJson->slice(), error));

  arangodb::iresearch::FieldIterator it(slice, linkMeta);
  REQUIRE(it.valid());
  REQUIRE(it != arangodb::iresearch::FieldIterator::END);

  // nested.foo (with IdentityAnalyzer)
  {
    auto& value = *it;
    CHECK(mangleStringIdentity("nested.foo") == value.name());
    const auto expected_analyzer = irs::analysis::analyzers::get("identity", irs::text_format::json, "");
    auto& analyzer = dynamic_cast<irs::analysis::analyzer&>(value.get_tokens());
    auto* analyzers = arangodb::application_features::ApplicationServer::lookupFeature<
      arangodb::iresearch::IResearchAnalyzerFeature
    >();
    CHECK(nullptr != analyzers);
    auto const expected_features = analyzers->get("identity")->features();
    CHECK(expected_features == value.features());
    CHECK(&expected_analyzer->type() == &analyzer.type());
  }

  ++it;
  REQUIRE(it.valid());
  REQUIRE(it != arangodb::iresearch::FieldIterator::END);

  // nested.foo (with EmptyAnalyzer)
  {
    auto& value = *it;
    CHECK(mangleString("nested.foo", "iresearch-document-empty") == value.name());
    auto& analyzer = dynamic_cast<EmptyAnalyzer&>(value.get_tokens());
    CHECK(!analyzer.next());
  }

  // keys[]
  for (size_t i = 0; i < 4; ++i) {
    ++it;
    REQUIRE(it.valid());
    REQUIRE(it != arangodb::iresearch::FieldIterator::END);

    auto& value = *it;
    CHECK(mangleStringIdentity("keys") == value.name());
    const auto expected_analyzer = irs::analysis::analyzers::get("identity", irs::text_format::json, "");
    auto* analyzers = arangodb::application_features::ApplicationServer::lookupFeature<
      arangodb::iresearch::IResearchAnalyzerFeature
    >();
    CHECK(nullptr != analyzers);
    auto const expected_features = analyzers->get("identity")->features();
    auto& analyzer = dynamic_cast<irs::analysis::analyzer&>(value.get_tokens());
    CHECK(expected_features == value.features());
    CHECK(&expected_analyzer->type() == &analyzer.type());
  }

  ++it;
  REQUIRE(it.valid());
  REQUIRE(it != arangodb::iresearch::FieldIterator::END);

  // boost
  {
    auto& value = *it;
    CHECK(mangleStringIdentity("boost") == value.name());
    const auto expected_analyzer = irs::analysis::analyzers::get("identity", irs::text_format::json, "");
    auto* analyzers = arangodb::application_features::ApplicationServer::lookupFeature<
      arangodb::iresearch::IResearchAnalyzerFeature
    >();
    CHECK(nullptr != analyzers);
    auto const expected_features = analyzers->get("identity")->features();
    auto& analyzer = dynamic_cast<irs::analysis::analyzer&>(value.get_tokens());
    CHECK(expected_features == value.features());
    CHECK(&expected_analyzer->type() == &analyzer.type());
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
  }

  ++it;
  REQUIRE(it.valid());
  REQUIRE(it != arangodb::iresearch::FieldIterator::END);

  // fields.fieldA (with IdenityAnalyzer)
  {
    auto& value = *it;
    CHECK(mangleStringIdentity("fields.fieldA.name") == value.name());
    const auto expected_analyzer = irs::analysis::analyzers::get("identity", irs::text_format::json, "");
    auto* analyzers = arangodb::application_features::ApplicationServer::lookupFeature<
      arangodb::iresearch::IResearchAnalyzerFeature
    >();
    CHECK(nullptr != analyzers);
    auto const expected_features = analyzers->get("identity")->features();
    auto& analyzer = dynamic_cast<irs::analysis::analyzer&>(value.get_tokens());
    CHECK(expected_features == value.features());
    CHECK(&expected_analyzer->type() == &analyzer.type());
  }

  ++it;
  REQUIRE(it.valid());
  REQUIRE(it != arangodb::iresearch::FieldIterator::END);

  // fields.fieldA (with EmptyAnalyzer)
  {
    auto& value = *it;
    CHECK(mangleString("fields.fieldA.name", "iresearch-document-empty") == value.name());
    auto& analyzer = dynamic_cast<EmptyAnalyzer&>(value.get_tokens());
    CHECK(!analyzer.next());
  }

  ++it;
  REQUIRE(it.valid());
  REQUIRE(it != arangodb::iresearch::FieldIterator::END);

  // listValuation (with IdenityAnalyzer)
  {
    auto& value = *it;
    CHECK(mangleStringIdentity("listValuation") == value.name());
    const auto expected_analyzer = irs::analysis::analyzers::get("identity", irs::text_format::json, "");
    auto* analyzers = arangodb::application_features::ApplicationServer::lookupFeature<
      arangodb::iresearch::IResearchAnalyzerFeature
    >();
    CHECK(nullptr != analyzers);
    auto const expected_features = analyzers->get("identity")->features();
    auto& analyzer = dynamic_cast<irs::analysis::analyzer&>(value.get_tokens());
    CHECK(expected_features == value.features());
    CHECK(&expected_analyzer->type() == &analyzer.type());
  }

  ++it;
  REQUIRE(it.valid());
  REQUIRE(it != arangodb::iresearch::FieldIterator::END);

  // listValuation (with EmptyAnalyzer)
  {
    auto& value = *it;
    CHECK(mangleString("listValuation", "iresearch-document-empty") == value.name());
    auto& analyzer = dynamic_cast<EmptyAnalyzer&>(value.get_tokens());
    CHECK(!analyzer.next());
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
  }


  // array[0].subarr[0-2]
  for (size_t i = 0; i < 3; ++i) {
    ++it;
    REQUIRE(it.valid());
    REQUIRE(it != arangodb::iresearch::FieldIterator::END);

    // IdentityAnalyzer
    {
      auto& value = *it;
      CHECK(mangleStringIdentity("array[0].subarr") == value.name());
      const auto expected_analyzer = irs::analysis::analyzers::get("identity", irs::text_format::json, "");
      auto* analyzers = arangodb::application_features::ApplicationServer::lookupFeature<
        arangodb::iresearch::IResearchAnalyzerFeature
      >();
      CHECK(nullptr != analyzers);
      auto const expected_features = analyzers->get("identity")->features();
      auto& analyzer = dynamic_cast<irs::analysis::analyzer&>(value.get_tokens());
      CHECK(expected_features == value.features());
      CHECK(&expected_analyzer->type() == &analyzer.type());
    }

    ++it;
    REQUIRE(it.valid());
    REQUIRE(it != arangodb::iresearch::FieldIterator::END);

    // EmptyAnalyzer
    {
      auto& value = *it;
      CHECK(mangleString("array[0].subarr", "iresearch-document-empty") == value.name());
      auto& analyzer = dynamic_cast<EmptyAnalyzer&>(value.get_tokens());
      CHECK(!analyzer.next());
    }
  }

   // array[1].subarr[0-2]
  for (size_t i = 0; i < 3; ++i) {
    ++it;
    REQUIRE(it.valid());
    REQUIRE(it != arangodb::iresearch::FieldIterator::END);

    // IdentityAnalyzer
    {
      auto& value = *it;
      CHECK(mangleStringIdentity("array[1].subarr") == value.name());
      const auto expected_analyzer = irs::analysis::analyzers::get("identity", irs::text_format::json, "");
      auto* analyzers = arangodb::application_features::ApplicationServer::lookupFeature<
        arangodb::iresearch::IResearchAnalyzerFeature
      >();
      CHECK(nullptr != analyzers);
      auto const expected_features = analyzers->get("identity")->features();
      auto& analyzer = dynamic_cast<irs::analysis::analyzer&>(value.get_tokens());
      CHECK(expected_features == value.features());
      CHECK(&expected_analyzer->type() == &analyzer.type());
    }

    ++it;
    REQUIRE(it.valid());
    REQUIRE(it != arangodb::iresearch::FieldIterator::END);

    // EmptyAnalyzer
    {
      auto& value = *it;
      CHECK(mangleString("array[1].subarr", "iresearch-document-empty") == value.name());
      auto& analyzer = dynamic_cast<EmptyAnalyzer&>(value.get_tokens());
      CHECK(!analyzer.next());
    }
  }

  ++it;
  REQUIRE(it.valid());
  REQUIRE(it != arangodb::iresearch::FieldIterator::END);

  // array[1].id (IdentityAnalyzer)
  {
    auto& value = *it;
    CHECK(mangleStringIdentity("array[1].id") == value.name());
    const auto expected_analyzer = irs::analysis::analyzers::get("identity", irs::text_format::json, "");
    auto* analyzers = arangodb::application_features::ApplicationServer::lookupFeature<
      arangodb::iresearch::IResearchAnalyzerFeature
    >();
    CHECK(nullptr != analyzers);
    auto const expected_features = analyzers->get("identity")->features();
    auto& analyzer = dynamic_cast<irs::analysis::analyzer&>(value.get_tokens());
    CHECK(expected_features == value.features());
    CHECK(&expected_analyzer->type() == &analyzer.type());
  }

  ++it;
  REQUIRE(it.valid());
  REQUIRE(it != arangodb::iresearch::FieldIterator::END);

  // array[1].id (EmptyAnalyzer)
  {
    auto& value = *it;
    CHECK(mangleString("array[1].id", "iresearch-document-empty") == value.name());
    auto& analyzer = dynamic_cast<EmptyAnalyzer&>(value.get_tokens());
    CHECK(!analyzer.next());
  }

  ++it;
  REQUIRE(it.valid());
  REQUIRE(it != arangodb::iresearch::FieldIterator::END);

  // array[2].id (IdentityAnalyzer)
  {
    auto& value = *it;
    CHECK(mangleNumeric("array[2].id") == value.name());
    auto& analyzer = dynamic_cast<irs::numeric_token_stream&>(value.get_tokens());
    CHECK(analyzer.next());
  }

  // array[2].subarr[0-2]
  for (size_t i = 0; i < 3; ++i) {
    ++it;
    REQUIRE(it.valid());
    REQUIRE(it != arangodb::iresearch::FieldIterator::END);

    // IdentityAnalyzer
    {
      auto& value = *it;
      CHECK(mangleStringIdentity("array[2].subarr") == value.name());
      const auto expected_analyzer = irs::analysis::analyzers::get("identity", irs::text_format::json, "");
      auto* analyzers = arangodb::application_features::ApplicationServer::lookupFeature<
        arangodb::iresearch::IResearchAnalyzerFeature
      >();
      CHECK(nullptr != analyzers);
      auto const expected_features = analyzers->get("identity")->features();
      auto& analyzer = dynamic_cast<irs::analysis::analyzer&>(value.get_tokens());
      CHECK(expected_features == value.features());
      CHECK(&expected_analyzer->type() == &analyzer.type());
    }

    ++it;
    REQUIRE(it.valid());
    REQUIRE(it != arangodb::iresearch::FieldIterator::END);

    // EmptyAnalyzer
    {
      auto& value = *it;
      CHECK(mangleString("array[2].subarr", "iresearch-document-empty") == value.name());
      auto& analyzer = dynamic_cast<EmptyAnalyzer&>(value.get_tokens());
      CHECK(!analyzer.next());
    }
  }

  ++it;
  CHECK(!it.valid());
  CHECK(it == arangodb::iresearch::FieldIterator::END);
}

SECTION("FieldIterator_nullptr_analyzer") {
  arangodb::iresearch::IResearchAnalyzerFeature analyzers(s.server);
  auto json = arangodb::velocypack::Parser::fromJson("{ \
    \"stringValue\": \"string\" \
  }");
  auto const slice = json->slice();

  // register analizers with feature
  {
    // ensure that there will be no exception on 'start'
    InvalidAnalyzer::returnNullFromMake = false;

    analyzers.start();
    analyzers.erase("empty");
    analyzers.erase("invalid");

    // ensure that there will be no exception on 'emplace'
    InvalidAnalyzer::returnNullFromMake = false;

    analyzers.emplace("empty", "iresearch-document-empty", "en", irs::flags{TestAttribute::type()});
    analyzers.emplace("invalid", "iresearch-document-invalid", "en", irs::flags{TestAttribute::type()});
  }

  // last analyzer invalid
  {
    arangodb::iresearch::IResearchLinkMeta linkMeta;
    linkMeta._analyzers.emplace_back(analyzers.get("empty")); // add analyzer
    linkMeta._analyzers.emplace_back(analyzers.get("invalid")); // add analyzer
    linkMeta._includeAllFields = true; // include all fields

    // acquire analyzer, another one should be created
    auto analyzer = linkMeta._analyzers.back()->get(); // cached instance should have been acquired

    arangodb::iresearch::FieldIterator it(slice, linkMeta);
    REQUIRE(it.valid());
    REQUIRE(it != arangodb::iresearch::FieldIterator::END);

    // stringValue (with IdentityAnalyzer)
    {
      auto& field = *it;
      CHECK(mangleStringIdentity("stringValue") == field.name());
      auto const expected_analyzer = irs::analysis::analyzers::get("identity", irs::text_format::json, "");
      auto* analyzers = arangodb::application_features::ApplicationServer::lookupFeature<
        arangodb::iresearch::IResearchAnalyzerFeature
      >();
      CHECK(nullptr != analyzers);
      auto const expected_features = analyzers->get("identity")->features();
      auto& analyzer = dynamic_cast<irs::analysis::analyzer&>(field.get_tokens());
      CHECK(&expected_analyzer->type() == &analyzer.type());
      CHECK(expected_features == field.features());
    }

    ++it;
    REQUIRE(it.valid());
    REQUIRE(arangodb::iresearch::FieldIterator::END != it);

    // stringValue (with EmptyAnalyzer)
    {
      auto& field = *it;
      CHECK(mangleString("stringValue", "empty") == field.name());
      auto const expected_analyzer = irs::analysis::analyzers::get("iresearch-document-empty", irs::text_format::json, "en");
      auto& analyzer = dynamic_cast<EmptyAnalyzer&>(field.get_tokens());
      CHECK(&expected_analyzer->type() == &analyzer.type());
      CHECK(expected_analyzer->attributes().features()== field.features());
    }

    ++it;
    REQUIRE(!it.valid());
    REQUIRE(arangodb::iresearch::FieldIterator::END == it);

    analyzer->reset(irs::string_ref::NIL); // ensure that acquired 'analyzer' will not be optimized out
  }

  // first analyzer is invalid
  {
    arangodb::iresearch::IResearchLinkMeta linkMeta;
    linkMeta._analyzers.clear();
    linkMeta._analyzers.emplace_back(analyzers.get("invalid")); // add analyzer
    linkMeta._analyzers.emplace_back(analyzers.get("empty")); // add analyzer
    linkMeta._includeAllFields = true; // include all fields

    // acquire analyzer, another one should be created
    auto analyzer = linkMeta._analyzers.front()->get(); // cached instance should have been acquired

    arangodb::iresearch::FieldIterator it(slice, linkMeta);
    REQUIRE(it.valid());
    REQUIRE(it != arangodb::iresearch::FieldIterator::END);

    // stringValue (with EmptyAnalyzer)
    {
      auto& field = *it;
      CHECK(mangleString("stringValue", "empty") == field.name());
      auto const expected_analyzer = irs::analysis::analyzers::get("iresearch-document-empty", irs::text_format::json, "en");
      auto& analyzer = dynamic_cast<EmptyAnalyzer&>(field.get_tokens());
      CHECK(&expected_analyzer->type() == &analyzer.type());
      CHECK(expected_analyzer->attributes().features() == field.features());
    }

    ++it;
    REQUIRE(!it.valid());
    REQUIRE(arangodb::iresearch::FieldIterator::END == it);

    analyzer->reset(irs::string_ref::NIL); // ensure that acquired 'analyzer' will not be optimized out
  }
}

SECTION("test_cid_rid_encoding") {
  auto data = arangodb::velocypack::Parser::fromJson(
    "[{ \"cid\": 62, \"rid\": 1605879230128717824},"
    "{ \"cid\": 62, \"rid\": 1605879230128717826},"
    "{ \"cid\": 62, \"rid\": 1605879230129766400},"
    "{ \"cid\": 62, \"rid\": 1605879230130814976},"
    "{ \"cid\": 62, \"rid\": 1605879230130814978},"
    "{ \"cid\": 62, \"rid\": 1605879230131863552},"
    "{ \"cid\": 62, \"rid\": 1605879230131863554},"
    "{ \"cid\": 62, \"rid\": 1605879230132912128},"
    "{ \"cid\": 62, \"rid\": 1605879230133960704},"
    "{ \"cid\": 62, \"rid\": 1605879230133960706},"
    "{ \"cid\": 62, \"rid\": 1605879230135009280},"
    "{ \"cid\": 62, \"rid\": 1605879230136057856},"
    "{ \"cid\": 62, \"rid\": 1605879230136057858},"
    "{ \"cid\": 62, \"rid\": 1605879230137106432},"
    "{ \"cid\": 62, \"rid\": 1605879230137106434},"
    "{ \"cid\": 62, \"rid\": 1605879230138155008},"
    "{ \"cid\": 62, \"rid\": 1605879230138155010},"
    "{ \"cid\": 62, \"rid\": 1605879230139203584},"
    "{ \"cid\": 62, \"rid\": 1605879230139203586},"
    "{ \"cid\": 62, \"rid\": 1605879230140252160},"
    "{ \"cid\": 62, \"rid\": 1605879230140252162},"
    "{ \"cid\": 62, \"rid\": 1605879230141300736},"
    "{ \"cid\": 62, \"rid\": 1605879230142349312},"
    "{ \"cid\": 62, \"rid\": 1605879230142349314},"
    "{ \"cid\": 62, \"rid\": 1605879230142349316},"
    "{ \"cid\": 62, \"rid\": 1605879230143397888},"
    "{ \"cid\": 62, \"rid\": 1605879230143397890},"
    "{ \"cid\": 62, \"rid\": 1605879230144446464},"
    "{ \"cid\": 62, \"rid\": 1605879230144446466},"
    "{ \"cid\": 62, \"rid\": 1605879230144446468},"
    "{ \"cid\": 62, \"rid\": 1605879230145495040},"
    "{ \"cid\": 62, \"rid\": 1605879230145495042},"
    "{ \"cid\": 62, \"rid\": 1605879230145495044},"
    "{ \"cid\": 62, \"rid\": 1605879230146543616},"
    "{ \"cid\": 62, \"rid\": 1605879230146543618},"
    "{ \"cid\": 62, \"rid\": 1605879230146543620},"
    "{ \"cid\": 62, \"rid\": 1605879230147592192}]"
  );

  struct DataStore {
    irs::memory_directory dir;
    irs::directory_reader reader;
    irs::index_writer::ptr writer;

    DataStore() {
      writer = irs::index_writer::make(dir, irs::formats::get("1_0"), irs::OM_CREATE);
      REQUIRE(writer);
      writer->commit();

      reader = irs::directory_reader::open(dir);
    }
  };

  DataStore store0, store1;

  auto const dataSlice = data->slice();

  arangodb::iresearch::Field field;
  TRI_voc_cid_t cid;
  uint64_t rid;

  size_t size = 0;
  for (auto const docSlice : arangodb::velocypack::ArrayIterator(dataSlice)) {
    auto const cidSlice = docSlice.get("cid");
    CHECK(cidSlice.isNumber());
    auto const ridSlice = docSlice.get("rid");
    CHECK(ridSlice.isNumber());

    cid = cidSlice.getNumber<TRI_voc_cid_t>();
    rid = ridSlice.getNumber<uint64_t>();

    arangodb::iresearch::DocumentPrimaryKey const pk(cid, rid);

    auto& writer = store0.writer;

    // insert document
    {
      auto doc = writer->documents().insert();
      arangodb::iresearch::Field::setCidValue(field, pk.first, arangodb::iresearch::Field::init_stream_t());
      CHECK((doc.insert(irs::action::index, field)));
      arangodb::iresearch::Field::setPkValue(field, pk);
      CHECK(doc.insert(irs::action::index_store, field));
      CHECK(doc);
    }
    writer->commit();

    ++size;
  }

  store0.reader = store0.reader->reopen();
  CHECK((size == store0.reader->size()));
  CHECK((size == store0.reader->docs_count()));

  store1.writer->import(*store0.reader);
  store1.writer->commit();

  auto reader = store1.reader->reopen();
  REQUIRE((reader));
  CHECK((1 == reader->size()));
  CHECK((size == reader->docs_count()));

  size_t found = 0;
  for (auto const docSlice : arangodb::velocypack::ArrayIterator(dataSlice)) {
    auto const cidSlice = docSlice.get("cid");
    CHECK(cidSlice.isNumber());
    auto const ridSlice = docSlice.get("rid");
    CHECK(ridSlice.isNumber());

    cid = cidSlice.getNumber<TRI_voc_cid_t>();
    rid = ridSlice.getNumber<uint64_t>();

    auto& segment = (*reader)[0];
    auto* cidField = segment.field(arangodb::iresearch::DocumentPrimaryKey::CID());
    CHECK(cidField);
    CHECK(size == cidField->docs_count());

    auto* pkField = segment.field(arangodb::iresearch::DocumentPrimaryKey::PK());
    CHECK(pkField);
    CHECK(size == pkField->docs_count());

    arangodb::iresearch::PrimaryKeyFilterContainer filters;
    CHECK(filters.empty());
    auto& filter = filters.emplace(cid, rid);
    REQUIRE(filter.type() == arangodb::iresearch::PrimaryKeyFilter::type());
    CHECK(!filters.empty());

    // first execution
    {
      auto prepared = filter.prepare(*reader);
      REQUIRE(prepared);
      CHECK(prepared == filter.prepare(*reader)); // same object
      CHECK(&filter == dynamic_cast<arangodb::iresearch::PrimaryKeyFilter const*>(prepared.get())); // same object

      for (auto& segment : *reader) {
        auto docs = prepared->execute(segment);
        REQUIRE(docs);
        //CHECK((nullptr == prepared->execute(segment))); // unusable filter TRI_ASSERT(...) check
        CHECK((irs::filter::prepared::empty() == filter.prepare(*reader))); // unusable filter (after execute)

        CHECK(docs->next());
        auto const id = docs->value();
        ++found;
        CHECK(!docs->next());
        CHECK(irs::type_limits<irs::type_t::doc_id_t>::eof(docs->value()));
        CHECK(!docs->next());
        CHECK(irs::type_limits<irs::type_t::doc_id_t>::eof(docs->value()));

        auto column = segment.column_reader(arangodb::iresearch::DocumentPrimaryKey::PK());
        REQUIRE(column);

        auto values = column->values();
        REQUIRE(values);

        irs::bytes_ref pkValue;
        CHECK(values(id, pkValue));

        arangodb::iresearch::DocumentPrimaryKey::type pk;
        CHECK(arangodb::iresearch::DocumentPrimaryKey::read(pk, pkValue));
        CHECK(cid == pk.first);
        CHECK(rid == pk.second);
      }
    }

    // FIXME uncomment after fix
    //// can't prepare twice
    //{
    //  auto prepared = filter.prepare(*reader);
    //  REQUIRE(prepared);
    //  CHECK(prepared == filter.prepare(*reader)); // same object

    //  for (auto& segment : *reader) {
    //    auto docs = prepared->execute(segment);
    //    REQUIRE(docs);
    //    CHECK(docs == prepared->execute(segment)); // same object
    //    CHECK(!docs->next());
    //    CHECK(irs::type_limits<irs::type_t::doc_id_t>::eof(docs->value()));
    //  }
    //}
  }

  CHECK(found == size);
}

SECTION("test_cid_rid_filter") {
  auto data = arangodb::velocypack::Parser::fromJson(
    "[{ \"cid\": 62, \"rid\": 1605879230128717824},"
    "{ \"cid\": 62, \"rid\": 1605879230128717826},"
    "{ \"cid\": 62, \"rid\": 1605879230129766400},"
    "{ \"cid\": 62, \"rid\": 1605879230130814976},"
    "{ \"cid\": 62, \"rid\": 1605879230130814978},"
    "{ \"cid\": 62, \"rid\": 1605879230131863552},"
    "{ \"cid\": 62, \"rid\": 1605879230131863554},"
    "{ \"cid\": 62, \"rid\": 1605879230132912128},"
    "{ \"cid\": 62, \"rid\": 1605879230133960704},"
    "{ \"cid\": 62, \"rid\": 1605879230133960706},"
    "{ \"cid\": 62, \"rid\": 1605879230135009280},"
    "{ \"cid\": 62, \"rid\": 1605879230136057856},"
    "{ \"cid\": 62, \"rid\": 1605879230136057858},"
    "{ \"cid\": 62, \"rid\": 1605879230137106432},"
    "{ \"cid\": 62, \"rid\": 1605879230137106434},"
    "{ \"cid\": 62, \"rid\": 1605879230138155008},"
    "{ \"cid\": 62, \"rid\": 1605879230138155010},"
    "{ \"cid\": 62, \"rid\": 1605879230139203584},"
    "{ \"cid\": 62, \"rid\": 1605879230139203586},"
    "{ \"cid\": 62, \"rid\": 1605879230140252160},"
    "{ \"cid\": 62, \"rid\": 1605879230140252162},"
    "{ \"cid\": 62, \"rid\": 1605879230141300736},"
    "{ \"cid\": 62, \"rid\": 1605879230142349312},"
    "{ \"cid\": 62, \"rid\": 1605879230142349314},"
    "{ \"cid\": 62, \"rid\": 1605879230142349316},"
    "{ \"cid\": 62, \"rid\": 1605879230143397888},"
    "{ \"cid\": 62, \"rid\": 1605879230143397890},"
    "{ \"cid\": 62, \"rid\": 1605879230144446464},"
    "{ \"cid\": 62, \"rid\": 1605879230144446466},"
    "{ \"cid\": 62, \"rid\": 1605879230144446468},"
    "{ \"cid\": 62, \"rid\": 1605879230145495040},"
    "{ \"cid\": 62, \"rid\": 1605879230145495042},"
    "{ \"cid\": 62, \"rid\": 1605879230145495044},"
    "{ \"cid\": 62, \"rid\": 1605879230146543616},"
    "{ \"cid\": 62, \"rid\": 1605879230146543618},"
    "{ \"cid\": 62, \"rid\": 1605879230146543620},"
    "{ \"cid\": 62, \"rid\": 1605879230147592192}]"
  );
  auto data1 = arangodb::velocypack::Parser::fromJson("{ \"cid\": 62, \"rid\": 2605879230128717824}");

  struct DataStore {
    irs::memory_directory dir;
    irs::directory_reader reader;
    irs::index_writer::ptr writer;

    DataStore() {
      writer = irs::index_writer::make(dir, irs::formats::get("1_0"), irs::OM_CREATE);
      REQUIRE(writer);
      writer->commit();

      reader = irs::directory_reader::open(dir);
    }
  };

  auto const dataSlice = data->slice();
  size_t expectedDocs = 0;
  size_t expectedLiveDocs = 0;
  DataStore store;

  // initial population
  for (auto const docSlice: arangodb::velocypack::ArrayIterator(dataSlice)) {
    auto const cidSlice = docSlice.get("cid");
    CHECK((cidSlice.isNumber<TRI_voc_cid_t>()));
    auto const ridSlice = docSlice.get("rid");
    CHECK((ridSlice.isNumber<uint64_t>()));

    auto cid = cidSlice.getNumber<TRI_voc_cid_t>();
    auto rid = ridSlice.getNumber<uint64_t>();
    arangodb::iresearch::Field field;
    arangodb::iresearch::DocumentPrimaryKey const pk(cid, rid);

    // insert document
    {
      auto ctx = store.writer->documents();
      auto doc = ctx.insert();
      arangodb::iresearch::Field::setCidValue(field, pk.first, arangodb::iresearch::Field::init_stream_t());
      CHECK((doc.insert(irs::action::index, field)));
      arangodb::iresearch::Field::setPkValue(field, pk);
      CHECK((doc.insert(irs::action::index_store, field)));
      CHECK((doc));
      ++expectedDocs;
      ++expectedLiveDocs;
    }
  }

  // add extra doc to hold segment after others are removed
  {
    arangodb::iresearch::Field field;
    arangodb::iresearch::DocumentPrimaryKey const pk(42, 12345);
    auto ctx = store.writer->documents();
    auto doc = ctx.insert();
    arangodb::iresearch::Field::setCidValue(field, pk.first, arangodb::iresearch::Field::init_stream_t());
    CHECK((doc.insert(irs::action::index, field)));
    arangodb::iresearch::Field::setPkValue(field, pk);
    CHECK((doc.insert(irs::action::index_store, field)));
    CHECK((doc));
  }

  store.writer->commit();
  store.reader = store.reader->reopen();
  CHECK((1 == store.reader->size()));
  CHECK((expectedDocs + 1 == store.reader->docs_count())); // +1 for keep-alive doc
  CHECK((expectedLiveDocs + 1 == store.reader->live_docs_count())); // +1 for keep-alive doc

  // check regular filter case (unique cid+rid)
  {
    size_t actualDocs = 0;

    for (auto const docSlice: arangodb::velocypack::ArrayIterator(dataSlice)) {
      auto const cidSlice = docSlice.get("cid");
      CHECK((cidSlice.isNumber<TRI_voc_cid_t>()));
      auto const ridSlice = docSlice.get("rid");
      CHECK(ridSlice.isNumber<uint64_t>());

      auto cid = cidSlice.getNumber<TRI_voc_cid_t>();
      auto rid = ridSlice.getNumber<uint64_t>();
      arangodb::iresearch::PrimaryKeyFilterContainer filters;
      CHECK((filters.empty()));
      auto& filter = filters.emplace(cid, rid);
      REQUIRE((filter.type() == arangodb::iresearch::PrimaryKeyFilter::type()));
      CHECK((!filters.empty()));

      auto prepared = filter.prepare(*store.reader);
      REQUIRE((prepared));
      CHECK((prepared == filter.prepare(*store.reader))); // same object
      CHECK((&filter == dynamic_cast<arangodb::iresearch::PrimaryKeyFilter const*>(prepared.get()))); // same object

      for (auto& segment: *store.reader) {
        auto docs = prepared->execute(segment);
        REQUIRE((docs));
        //CHECK((nullptr == prepared->execute(segment))); // unusable filter TRI_ASSERT(...) check
        CHECK((irs::filter::prepared::empty() == filter.prepare(*store.reader))); // unusable filter (after execute)

        CHECK((docs->next()));
        auto const id = docs->value();
        ++actualDocs;
        CHECK((!docs->next()));
        CHECK((irs::type_limits<irs::type_t::doc_id_t>::eof(docs->value())));
        CHECK((!docs->next()));
        CHECK((irs::type_limits<irs::type_t::doc_id_t>::eof(docs->value())));

        auto column = segment.column_reader(arangodb::iresearch::DocumentPrimaryKey::PK());
        REQUIRE((column));

        auto values = column->values();
        REQUIRE((values));

        irs::bytes_ref pkValue;
        CHECK((values(id, pkValue)));

        arangodb::iresearch::DocumentPrimaryKey::type pk;
        CHECK((arangodb::iresearch::DocumentPrimaryKey::read(pk, pkValue)));
        CHECK((cid == pk.first));
        CHECK((rid == pk.second));
      }
    }

    CHECK((expectedDocs == actualDocs));
  }

  // remove + insert (simulate recovery)
  for (auto const docSlice: arangodb::velocypack::ArrayIterator(dataSlice)) {
    auto const cidSlice = docSlice.get("cid");
    CHECK((cidSlice.isNumber<TRI_voc_cid_t>()));
    auto const ridSlice = docSlice.get("rid");
    CHECK((ridSlice.isNumber<uint64_t>()));

    auto cid = cidSlice.getNumber<TRI_voc_cid_t>();
    auto rid = ridSlice.getNumber<uint64_t>();
    arangodb::iresearch::Field field;
    arangodb::iresearch::DocumentPrimaryKey const pk(cid, rid);

    // remove + insert document
    {
      auto ctx = store.writer->documents();
      ctx.remove(std::make_shared<arangodb::iresearch::PrimaryKeyFilter>(cid, rid));
      auto doc = ctx.insert();
      arangodb::iresearch::Field::setCidValue(field, pk.first, arangodb::iresearch::Field::init_stream_t());
      CHECK((doc.insert(irs::action::index, field)));
      arangodb::iresearch::Field::setPkValue(field, pk);
      CHECK((doc.insert(irs::action::index_store, field)));
      CHECK((doc));
      ++expectedDocs;
    }
  }

  // add extra doc to hold segment after others are removed
  {
    arangodb::iresearch::Field field;
    arangodb::iresearch::DocumentPrimaryKey const pk(43, 123456);
    auto ctx = store.writer->documents();
    auto doc = ctx.insert();
    arangodb::iresearch::Field::setCidValue(field, pk.first, arangodb::iresearch::Field::init_stream_t());
    CHECK((doc.insert(irs::action::index, field)));
    arangodb::iresearch::Field::setPkValue(field, pk);
    CHECK((doc.insert(irs::action::index_store, field)));
    CHECK((doc));
  }

  store.writer->commit();
  store.reader = store.reader->reopen();
  CHECK((2 == store.reader->size()));
  CHECK((expectedDocs + 2 == store.reader->docs_count())); // +2 for keep-alive doc
  CHECK((expectedLiveDocs + 2 == store.reader->live_docs_count())); // +2 for keep-alive doc

  // check 1st recovery case
  {
    size_t actualDocs = 0;

    auto beforeRecovery = StorageEngineMock::inRecoveryResult;
    StorageEngineMock::inRecoveryResult = true;
    auto restoreRecovery = irs::make_finally([&beforeRecovery]()->void { StorageEngineMock::inRecoveryResult = beforeRecovery; });

    for (auto const docSlice: arangodb::velocypack::ArrayIterator(dataSlice)) {
      auto const cidSlice = docSlice.get("cid");
      CHECK((cidSlice.isNumber<TRI_voc_cid_t>()));
      auto const ridSlice = docSlice.get("rid");
      CHECK(ridSlice.isNumber<uint64_t>());

      auto cid = cidSlice.getNumber<TRI_voc_cid_t>();
      auto rid = ridSlice.getNumber<uint64_t>();
      arangodb::iresearch::PrimaryKeyFilterContainer filters;
      CHECK((filters.empty()));
      auto& filter = filters.emplace(cid, rid);
      REQUIRE((filter.type() == arangodb::iresearch::PrimaryKeyFilter::type()));
      CHECK((!filters.empty()));

      auto prepared = filter.prepare(*store.reader);
      REQUIRE((prepared));
      CHECK((prepared == filter.prepare(*store.reader))); // same object
      CHECK((&filter == dynamic_cast<arangodb::iresearch::PrimaryKeyFilter const*>(prepared.get()))); // same object

      for (auto& segment: *store.reader) {
        auto docs = prepared->execute(segment);
        REQUIRE((docs));
        CHECK((nullptr != prepared->execute(segment))); // usable filter
        CHECK((nullptr != filter.prepare(*store.reader))); // usable filter (after execute)

        if (docs->next()) { // old segments will not have any matching docs
          auto const id = docs->value();
          ++actualDocs;
          CHECK((!docs->next()));
          CHECK((irs::type_limits<irs::type_t::doc_id_t>::eof(docs->value())));
          CHECK((!docs->next()));
          CHECK((irs::type_limits<irs::type_t::doc_id_t>::eof(docs->value())));

          auto column = segment.column_reader(arangodb::iresearch::DocumentPrimaryKey::PK());
          REQUIRE((column));

          auto values = column->values();
          REQUIRE((values));

          irs::bytes_ref pkValue;
          CHECK((values(id, pkValue)));

          arangodb::iresearch::DocumentPrimaryKey::type pk;
          CHECK((arangodb::iresearch::DocumentPrimaryKey::read(pk, pkValue)));
          CHECK((cid == pk.first));
          CHECK((rid == pk.second));
        }
      }
    }

    CHECK((expectedLiveDocs == actualDocs));
  }

  // remove + insert (simulate recovery) 2nd time
  for (auto const docSlice: arangodb::velocypack::ArrayIterator(dataSlice)) {
    auto const cidSlice = docSlice.get("cid");
    CHECK((cidSlice.isNumber<TRI_voc_cid_t>()));
    auto const ridSlice = docSlice.get("rid");
    CHECK((ridSlice.isNumber<uint64_t>()));

    auto cid = cidSlice.getNumber<TRI_voc_cid_t>();
    auto rid = ridSlice.getNumber<uint64_t>();
    arangodb::iresearch::Field field;
    arangodb::iresearch::DocumentPrimaryKey const pk(cid, rid);

    // remove + insert document
    {
      auto ctx = store.writer->documents();
      ctx.remove(std::make_shared<arangodb::iresearch::PrimaryKeyFilter>(cid, rid));
      auto doc = ctx.insert();
      arangodb::iresearch::Field::setCidValue(field, pk.first, arangodb::iresearch::Field::init_stream_t());
      CHECK((doc.insert(irs::action::index, field)));
      arangodb::iresearch::Field::setPkValue(field, pk);
      CHECK((doc.insert(irs::action::index_store, field)));
      CHECK((doc));
      ++expectedDocs;
    }
  }

  // add extra doc to hold segment after others are removed
  {
    arangodb::iresearch::Field field;
    arangodb::iresearch::DocumentPrimaryKey const pk(44, 1234567);
    auto ctx = store.writer->documents();
    auto doc = ctx.insert();
    arangodb::iresearch::Field::setCidValue(field, pk.first, arangodb::iresearch::Field::init_stream_t());
    CHECK((doc.insert(irs::action::index, field)));
    arangodb::iresearch::Field::setPkValue(field, pk);
    CHECK((doc.insert(irs::action::index_store, field)));
    CHECK((doc));
  }

  store.writer->commit();
  store.reader = store.reader->reopen();
  CHECK((3 == store.reader->size()));
  CHECK((expectedDocs + 3 == store.reader->docs_count())); // +3 for keep-alive doc
  CHECK((expectedLiveDocs + 3 == store.reader->live_docs_count())); // +3 for keep-alive doc

  // check 2nd recovery case
  {
    size_t actualDocs = 0;

    auto beforeRecovery = StorageEngineMock::inRecoveryResult;
    StorageEngineMock::inRecoveryResult = true;
    auto restoreRecovery = irs::make_finally([&beforeRecovery]()->void { StorageEngineMock::inRecoveryResult = beforeRecovery; });

    for (auto const docSlice: arangodb::velocypack::ArrayIterator(dataSlice)) {
      auto const cidSlice = docSlice.get("cid");
      CHECK((cidSlice.isNumber<TRI_voc_cid_t>()));
      auto const ridSlice = docSlice.get("rid");
      CHECK(ridSlice.isNumber<uint64_t>());

      auto cid = cidSlice.getNumber<TRI_voc_cid_t>();
      auto rid = ridSlice.getNumber<uint64_t>();
      arangodb::iresearch::PrimaryKeyFilterContainer filters;
      CHECK((filters.empty()));
      auto& filter = filters.emplace(cid, rid);
      REQUIRE((filter.type() == arangodb::iresearch::PrimaryKeyFilter::type()));
      CHECK((!filters.empty()));

      auto prepared = filter.prepare(*store.reader);
      REQUIRE((prepared));
      CHECK((prepared == filter.prepare(*store.reader))); // same object
      CHECK((&filter == dynamic_cast<arangodb::iresearch::PrimaryKeyFilter const*>(prepared.get()))); // same object

      for (auto& segment: *store.reader) {
        auto docs = prepared->execute(segment);
        REQUIRE((docs));
        CHECK((nullptr != prepared->execute(segment))); // usable filter
        CHECK((nullptr != filter.prepare(*store.reader))); // usable filter (after execute)

        if (docs->next()) { // old segments will not have any matching docs
          auto const id = docs->value();
          ++actualDocs;
          CHECK((!docs->next()));
          CHECK((irs::type_limits<irs::type_t::doc_id_t>::eof(docs->value())));
          CHECK((!docs->next()));
          CHECK((irs::type_limits<irs::type_t::doc_id_t>::eof(docs->value())));

          auto column = segment.column_reader(arangodb::iresearch::DocumentPrimaryKey::PK());
          REQUIRE((column));

          auto values = column->values();
          REQUIRE((values));

          irs::bytes_ref pkValue;
          CHECK((values(id, pkValue)));

          arangodb::iresearch::DocumentPrimaryKey::type pk;
          CHECK((arangodb::iresearch::DocumentPrimaryKey::read(pk, pkValue)));
          CHECK((cid == pk.first));
          CHECK((rid == pk.second));
        }
      }
    }

    CHECK((expectedLiveDocs == actualDocs));
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate tests
////////////////////////////////////////////////////////////////////////////////

}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------