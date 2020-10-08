//////////////////////////////////////////////////////////////////////////////
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

#include <memory>

#include "gtest/gtest.h"

#include "velocypack/Builder.h"
#include "velocypack/Iterator.h"
#include "velocypack/Parser.h"
#include "velocypack/velocypack-aliases.h"

#include "analysis/analyzers.hpp"
#include "analysis/token_streams.hpp"
#include "index/directory_reader.hpp"
#include "index/index_writer.hpp"
#include "store/memory_directory.hpp"

#include "IResearch/common.h"
#include "Mocks/LogLevels.h"
#include "Mocks/Servers.h"

#include "Aql/AqlFunctionFeature.h"
#include "Cluster/ClusterFeature.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "IResearch/IResearchAnalyzerFeature.h"
#include "IResearch/IResearchCommon.h"
#include "IResearch/IResearchDocument.h"
#include "IResearch/IResearchFilterFactory.h"
#include "IResearch/IResearchKludge.h"
#include "IResearch/IResearchLinkMeta.h"
#include "IResearch/IResearchPrimaryKeyFilter.h"
#include "Logger/LogTopic.h"
#include "Logger/Logger.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "RestServer/SystemDatabaseFeature.h"
#include "RestServer/AqlFeature.h"
#include "Aql/OptimizerRulesFeature.h"
#include "Sharding/ShardingFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "Transaction/Methods.h"
#include "Transaction/StandaloneContext.h"
#include "V8Server/V8DealerFeature.h"
#include "VocBase/Methods/Collections.h"

#if USE_ENTERPRISE
#include "Enterprise/Ldap/LdapFeature.h"
#endif

namespace {
static const VPackBuilder systemDatabaseBuilder = dbArgsBuilder();
static const VPackSlice   systemDatabaseArgs = systemDatabaseBuilder.slice();

struct TestAttribute : public irs::attribute {
  static constexpr irs::string_ref type_name() noexcept {
    return "TestAttribute";
  }
};

class EmptyAnalyzer : public irs::analysis::analyzer {
 public:
  static constexpr irs::string_ref type_name() noexcept {
    return "iresearch-document-empty";
  }
  static ptr make(irs::string_ref const&) {
    PTR_NAMED(EmptyAnalyzer, ptr);
    return ptr;
  }
  static bool normalize(irs::string_ref const&, std::string& out) {
    out.resize(VPackSlice::emptyObjectSlice().byteSize());
    std::memcpy(&out[0], VPackSlice::emptyObjectSlice().begin(), out.size());
    return true;
  }

  EmptyAnalyzer() : irs::analysis::analyzer(irs::type<EmptyAnalyzer>::get()) { }
  virtual irs::attribute* get_mutable(irs::type_info::type_id type) noexcept override {
    if (type == irs::type<irs::frequency>::id()) {
      return &_attr;
    }
    return nullptr;
  }
  virtual bool next() override { return false; }
  virtual bool reset(irs::string_ref const& data) override { return true; }

 private:
  irs::frequency _attr;
};

REGISTER_ANALYZER_VPACK(EmptyAnalyzer, EmptyAnalyzer::make, EmptyAnalyzer::normalize);

class InvalidAnalyzer : public irs::analysis::analyzer {
 public:
  static bool returnNullFromMake;
  static bool returnFalseFromToString;

  static constexpr irs::string_ref type_name() noexcept {
    return "iresearch-document-invalid";
  }

  static ptr make(irs::string_ref const&) {
    if (returnNullFromMake) {
      return nullptr;
    }

    PTR_NAMED(InvalidAnalyzer, ptr);
    return ptr;
  }

  static bool normalize(irs::string_ref const&, std::string& out) {
    out.resize(VPackSlice::emptyObjectSlice().byteSize());
    std::memcpy(&out[0], VPackSlice::emptyObjectSlice().begin(), out.size());
    return !returnFalseFromToString;
  }

  InvalidAnalyzer() : irs::analysis::analyzer(irs::type<InvalidAnalyzer>::get()) { }
  virtual irs::attribute* get_mutable(irs::type_info::type_id type) noexcept override {
    if (type == irs::type<TestAttribute>::id()) {
      return &_attr;
    }
    return nullptr;
  }
  virtual bool next() override { return false; }
  virtual bool reset(irs::string_ref const& data) override { return true; }

 private:
  TestAttribute _attr;
};

bool InvalidAnalyzer::returnNullFromMake = false;
bool InvalidAnalyzer::returnFalseFromToString = false;

REGISTER_ANALYZER_VPACK(InvalidAnalyzer, InvalidAnalyzer::make, InvalidAnalyzer::normalize);

}  // namespace

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

class IResearchDocumentTest
    : public ::testing::Test,
      public arangodb::tests::LogSuppressor<arangodb::Logger::AUTHENTICATION, arangodb::LogLevel::ERR> {
 protected:
  arangodb::tests::mocks::MockAqlServer server;

  IResearchDocumentTest() {
    arangodb::tests::init();

    {
      auto& sysDatabase = server.getFeature<arangodb::SystemDatabaseFeature>();
      auto vocbase = sysDatabase.use();
      std::shared_ptr<arangodb::LogicalCollection> unused;
      arangodb::methods::Collections::createSystem(*vocbase, arangodb::tests::AnalyzerCollectionName,
                                                   false, unused);
    }

    auto& analyzers = server.getFeature<arangodb::iresearch::IResearchAnalyzerFeature>();
    arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;

    // ensure that there will be no exception on 'emplace'
    InvalidAnalyzer::returnNullFromMake = false;
    InvalidAnalyzer::returnFalseFromToString = false;

    auto res = analyzers.emplace(
        result,
        arangodb::StaticStrings::SystemDatabase + "::iresearch-document-empty",
        "iresearch-document-empty",
        arangodb::velocypack::Parser::fromJson("{ \"args\": \"en\" }")->slice(),
        irs::flags{irs::type<irs::frequency>::get()});  // cache analyzer
    EXPECT_TRUE(res.ok());

    res = analyzers.emplace(
        result,
        arangodb::StaticStrings::SystemDatabase +
            "::iresearch-document-invalid",
        "iresearch-document-invalid",
        arangodb::velocypack::Parser::fromJson("{ \"args\": \"en\" }")->slice(),
        irs::flags{irs::type<irs::frequency>::get()});  // cache analyzer
    EXPECT_TRUE(res.ok());
  }
};

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

TEST_F(IResearchDocumentTest, FieldIterator_static_checks) {
  static_assert(std::is_same<std::forward_iterator_tag, arangodb::iresearch::FieldIterator::iterator_category>::value,
                "Invalid iterator category");

  static_assert(std::is_same<arangodb::iresearch::Field const, arangodb::iresearch::FieldIterator::value_type>::value,
                "Invalid iterator value type");

  static_assert(std::is_same<arangodb::iresearch::Field const&, arangodb::iresearch::FieldIterator::reference>::value,
                "Invalid iterator reference type");

  static_assert(std::is_same<arangodb::iresearch::Field const*, arangodb::iresearch::FieldIterator::pointer>::value,
                "Invalid iterator pointer type");

  static_assert(std::is_same<std::ptrdiff_t, arangodb::iresearch::FieldIterator::difference_type>::value,
                "Invalid iterator difference type");
}

TEST_F(IResearchDocumentTest, FieldIterator_construct) {
  auto& sysDatabase = server.getFeature<arangodb::SystemDatabaseFeature>();
  auto sysVocbase = sysDatabase.use();

  std::vector<std::string> EMPTY;
  arangodb::transaction::Methods trx(arangodb::transaction::StandaloneContext::Create(*sysVocbase),
                                     EMPTY, EMPTY, EMPTY,
                                     arangodb::transaction::Options());

  arangodb::iresearch::FieldIterator it(trx);
  EXPECT_FALSE(it.valid());
  EXPECT_EQ(it, arangodb::iresearch::FieldIterator(trx));
}

TEST_F(IResearchDocumentTest, FieldIterator_traverse_complex_object_custom_nested_delimiter) {
  auto& sysDatabase = server.getFeature<arangodb::SystemDatabaseFeature>();
  auto sysVocbase = sysDatabase.use();

  auto json = arangodb::velocypack::Parser::fromJson(
      "{ \
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

  std::unordered_map<std::string, size_t> expectedValues{
      {mangleStringIdentity("nested.foo"), 1},
      {mangleStringIdentity("keys"), 4},
      {mangleStringIdentity("boost"), 1},
      {mangleStringIdentity("depth"), 1},
      {mangleStringIdentity("fields.fieldA.name"), 1},
      {mangleStringIdentity("fields.fieldB.name"), 1},
      {mangleStringIdentity("listValuation"), 1},
      {mangleStringIdentity("locale"), 1},
      {mangleStringIdentity("array.id"), 3},
      {mangleStringIdentity("array.subarr"), 9},
      {mangleStringIdentity("array.subobj.id"), 2},
      {mangleStringIdentity("array.subobj.name"), 1},
      {mangleStringIdentity("array.id"), 2}};

  auto const slice = json->slice();

  arangodb::iresearch::IResearchLinkMeta linkMeta;
  linkMeta._includeAllFields = true;  // include all fields

  std::vector<std::string> EMPTY;
  arangodb::transaction::Methods trx(arangodb::transaction::StandaloneContext::Create(*sysVocbase),
                                     EMPTY, EMPTY, EMPTY,
                                     arangodb::transaction::Options());

  arangodb::iresearch::FieldIterator it(trx);
  it.reset(slice, linkMeta);
  EXPECT_NE(it, arangodb::iresearch::FieldIterator(trx));

  // default analyzer
  auto const expected_analyzer =
      irs::analysis::analyzers::get("identity", irs::type<irs::text_format::vpack>::get(),
                                     arangodb::iresearch::ref<char>(VPackSlice::emptyObjectSlice()));
  auto& analyzers = server.getFeature<arangodb::iresearch::IResearchAnalyzerFeature>();
  auto const expected_features = analyzers.get("identity", arangodb::QueryAnalyzerRevisions::QUERY_LATEST)->features();

  while (it.valid()) {
    auto& field = *it;
    std::string const actualName = std::string(field.name());
    auto const expectedValue = expectedValues.find(actualName);
    ASSERT_NE(expectedValues.end(), expectedValue);

    auto& refs = expectedValue->second;
    if (!--refs) {
      expectedValues.erase(expectedValue);
    }

    auto& analyzer = dynamic_cast<irs::analysis::analyzer&>(field.get_tokens());
    EXPECT_EQ(expected_features, field.features());
    EXPECT_EQ(expected_analyzer->type(), analyzer.type());

    ++it;
  }

  EXPECT_TRUE(expectedValues.empty());
  EXPECT_EQ(it, arangodb::iresearch::FieldIterator(trx));
}

TEST_F(IResearchDocumentTest, FieldIterator_traverse_complex_object_all_fields) {
  auto& sysDatabase = server.getFeature<arangodb::SystemDatabaseFeature>();
  auto sysVocbase = sysDatabase.use();

  auto json = arangodb::velocypack::Parser::fromJson(
      "{ \
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

  std::unordered_map<std::string, size_t> expectedValues{
      {mangleStringIdentity("nested.foo"), 1},
      {mangleStringIdentity("keys"), 4},
      {mangleStringIdentity("boost"), 1},
      {mangleStringIdentity("depth"), 1},
      {mangleStringIdentity("fields.fieldA.name"), 1},
      {mangleStringIdentity("fields.fieldB.name"), 1},
      {mangleStringIdentity("listValuation"), 1},
      {mangleStringIdentity("locale"), 1},
      {mangleStringIdentity("array.id"), 3},
      {mangleStringIdentity("array.subarr"), 9},
      {mangleStringIdentity("array.subobj.id"), 2},
      {mangleStringIdentity("array.subobj.name"), 1},
      {mangleStringIdentity("array.id"), 2}};

  auto const slice = json->slice();

  arangodb::iresearch::IResearchLinkMeta linkMeta;
  linkMeta._includeAllFields = true;

  std::vector<std::string> EMPTY;
  arangodb::transaction::Methods trx(arangodb::transaction::StandaloneContext::Create(*sysVocbase),
                                     EMPTY, EMPTY, EMPTY,
                                     arangodb::transaction::Options());

  arangodb::iresearch::FieldIterator it(trx);
  it.reset(slice, linkMeta);
  EXPECT_NE(it, arangodb::iresearch::FieldIterator(trx));

  // default analyzer
  auto const expected_analyzer =
      irs::analysis::analyzers::get("identity", irs::type<irs::text_format::vpack>::get(), 
                                    arangodb::iresearch::ref<char>(VPackSlice::emptyObjectSlice()));
  auto& analyzers = server.getFeature<arangodb::iresearch::IResearchAnalyzerFeature>();
  auto const expected_features = analyzers.get("identity", arangodb::QueryAnalyzerRevisions::QUERY_LATEST)->features();

  while (it.valid()) {
    auto& field = *it;
    std::string const actualName = std::string(field.name());
    auto const expectedValue = expectedValues.find(actualName);
    ASSERT_NE(expectedValues.end(), expectedValue);

    auto& refs = expectedValue->second;
    if (!--refs) {
      expectedValues.erase(expectedValue);
    }

    auto& analyzer = dynamic_cast<irs::analysis::analyzer&>(field.get_tokens());
    EXPECT_EQ(expected_features, field.features());
    EXPECT_EQ(expected_analyzer->type(), analyzer.type());

    ++it;
  }

  EXPECT_TRUE(expectedValues.empty());
  EXPECT_EQ(it, arangodb::iresearch::FieldIterator(trx));
}

TEST_F(IResearchDocumentTest, FieldIterator_traverse_complex_object_ordered_all_fields) {
  auto& sysDatabase = server.getFeature<arangodb::SystemDatabaseFeature>();
  auto sysVocbase = sysDatabase.use();

  auto json = arangodb::velocypack::Parser::fromJson(
      "{ \
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

  std::unordered_multiset<std::string> expectedValues{
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
      mangleStringIdentity("array[2].subobj.id")};

  auto const slice = json->slice();

  arangodb::iresearch::IResearchLinkMeta linkMeta;
  linkMeta._includeAllFields = true;    // include all fields
  linkMeta._trackListPositions = true;  // allow indexes in field names

  // default analyzer
  auto const expected_analyzer =
      irs::analysis::analyzers::get("identity", irs::type<irs::text_format::vpack>::get(), 
                                     arangodb::iresearch::ref<char>(VPackSlice::emptyObjectSlice()));
  auto& analyzers = server.getFeature<arangodb::iresearch::IResearchAnalyzerFeature>();
  auto const expected_features = analyzers.get("identity", arangodb::QueryAnalyzerRevisions::QUERY_LATEST)->features();

  std::vector<std::string> EMPTY;
  arangodb::transaction::Methods trx(arangodb::transaction::StandaloneContext::Create(*sysVocbase),
                                     EMPTY, EMPTY, EMPTY,
                                     arangodb::transaction::Options());

  arangodb::iresearch::FieldIterator doc(trx);
  doc.reset(slice, linkMeta);
  for (; doc.valid(); ++doc) {
    auto& field = *doc;
    std::string const actualName = std::string(field.name());
    EXPECT_EQ(1, expectedValues.erase(actualName));

    auto& analyzer = dynamic_cast<irs::analysis::analyzer&>(field.get_tokens());
    EXPECT_EQ(expected_features, field.features());
    EXPECT_EQ(expected_analyzer->type(), analyzer.type());
  }

  EXPECT_TRUE(expectedValues.empty());
}

TEST_F(IResearchDocumentTest, FieldIterator_traverse_complex_object_ordered_filtered) {
  auto& sysDatabase = server.getFeature<arangodb::SystemDatabaseFeature>();
  auto sysVocbase = sysDatabase.use();

  auto json = arangodb::velocypack::Parser::fromJson(
      "{ \
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

  auto linkMetaJson = arangodb::velocypack::Parser::fromJson(
      "{ \
    \"includeAllFields\" : false, \
    \"trackListPositions\" : true, \
    \"fields\" : { \"boost\" : { } }, \
    \"analyzers\": [ \"identity\" ] \
  }");

  auto const slice = json->slice();

  arangodb::iresearch::IResearchLinkMeta linkMeta;

  std::string error;
  ASSERT_TRUE(linkMeta.init(server.server(), linkMetaJson->slice(), false, error));

  std::vector<std::string> EMPTY;
  arangodb::transaction::Methods trx(arangodb::transaction::StandaloneContext::Create(*sysVocbase),
                                     EMPTY, EMPTY, EMPTY,
                                     arangodb::transaction::Options());

  arangodb::iresearch::FieldIterator it(trx);
  it.reset(slice, linkMeta);
  ASSERT_TRUE(it.valid());
  ASSERT_NE(it, arangodb::iresearch::FieldIterator(trx));

  auto& value = *it;
  EXPECT_EQ(mangleStringIdentity("boost"), value.name());
  const auto expected_analyzer =
      irs::analysis::analyzers::get("identity", irs::type<irs::text_format::vpack>::get(), 
                                    arangodb::iresearch::ref<char>(VPackSlice::emptyObjectSlice()));
  auto& analyzers = server.getFeature<arangodb::iresearch::IResearchAnalyzerFeature>();
  auto const expected_features = analyzers.get("identity", arangodb::QueryAnalyzerRevisions::QUERY_LATEST)->features();
  auto& analyzer = dynamic_cast<irs::analysis::analyzer&>(value.get_tokens());
  EXPECT_EQ(expected_features, value.features());
  EXPECT_EQ(expected_analyzer->type(), analyzer.type());

  ++it;
  EXPECT_FALSE(it.valid());
  EXPECT_EQ(it, arangodb::iresearch::FieldIterator(trx));
}

TEST_F(IResearchDocumentTest, FieldIterator_traverse_complex_object_ordered_filtered_2) {
  auto& sysDatabase = server.getFeature<arangodb::SystemDatabaseFeature>();
  auto sysVocbase = sysDatabase.use();

  auto json = arangodb::velocypack::Parser::fromJson(
      "{ \
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
  linkMeta._includeAllFields = false;   // ignore all fields
  linkMeta._trackListPositions = true;  // allow indexes in field names

  std::vector<std::string> EMPTY;
  arangodb::transaction::Methods trx(arangodb::transaction::StandaloneContext::Create(*sysVocbase),
                                     EMPTY, EMPTY, EMPTY,
                                     arangodb::transaction::Options());

  arangodb::iresearch::FieldIterator it(trx);
  it.reset(slice, linkMeta);
  EXPECT_FALSE(it.valid());
  EXPECT_EQ(it, arangodb::iresearch::FieldIterator(trx));
}

TEST_F(IResearchDocumentTest, FieldIterator_traverse_complex_object_ordered_empty_analyzers) {
  auto& sysDatabase = server.getFeature<arangodb::SystemDatabaseFeature>();
  auto sysVocbase = sysDatabase.use();

  auto json = arangodb::velocypack::Parser::fromJson(
      "{ \
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
  linkMeta._analyzers.clear();        // clear all analyzers
  linkMeta._includeAllFields = true;  // include all fields

  std::vector<std::string> EMPTY;
  arangodb::transaction::Methods trx(arangodb::transaction::StandaloneContext::Create(*sysVocbase),
                                     EMPTY, EMPTY, EMPTY,
                                     arangodb::transaction::Options());

  arangodb::iresearch::FieldIterator it(trx);
  it.reset(slice, linkMeta);
  EXPECT_FALSE(it.valid());
  EXPECT_EQ(it, arangodb::iresearch::FieldIterator(trx));
}

TEST_F(IResearchDocumentTest, FieldIterator_traverse_complex_object_ordered_check_value_types) {
  auto& analyzers = server.getFeature<arangodb::iresearch::IResearchAnalyzerFeature>();
  auto& sysDatabase = server.getFeature<arangodb::SystemDatabaseFeature>();
  auto sysVocbase = sysDatabase.use();

  auto json = arangodb::velocypack::Parser::fromJson(
      "{ \
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
  linkMeta._analyzers.emplace_back(arangodb::iresearch::FieldMeta::Analyzer(
      analyzers.get(arangodb::StaticStrings::SystemDatabase +
                    "::iresearch-document-empty", arangodb::QueryAnalyzerRevisions::QUERY_LATEST),
      "iresearch-document-empty"));   // add analyzer
  linkMeta._includeAllFields = true;  // include all fields

  std::vector<std::string> EMPTY;
  arangodb::transaction::Methods trx(arangodb::transaction::StandaloneContext::Create(*sysVocbase),
                                     EMPTY, EMPTY, EMPTY,
                                     arangodb::transaction::Options());

  arangodb::iresearch::FieldIterator it(trx);
  it.reset(slice, linkMeta);
  EXPECT_NE(it, arangodb::iresearch::FieldIterator(trx));

  // stringValue (with IdentityAnalyzer)
  {
    auto& field = *it;
    EXPECT_EQ(mangleStringIdentity("stringValue"), field.name());
    auto const expected_analyzer =
        irs::analysis::analyzers::get("identity", irs::type<irs::text_format::vpack>::get(), 
                                      arangodb::iresearch::ref<char>(VPackSlice::emptyObjectSlice()));
    auto& analyzers = server.getFeature<arangodb::iresearch::IResearchAnalyzerFeature>();
    auto const expected_features = analyzers.get("identity", arangodb::QueryAnalyzerRevisions::QUERY_LATEST)->features();
    auto& analyzer = dynamic_cast<irs::analysis::analyzer&>(field.get_tokens());
    EXPECT_EQ(expected_analyzer->type(), analyzer.type());
    EXPECT_EQ(expected_features, field.features());
  }

  ++it;
  ASSERT_TRUE(it.valid());
  ASSERT_NE(it, arangodb::iresearch::FieldIterator(trx));

  // stringValue (with EmptyAnalyzer)
  {
    auto& field = *it;
    EXPECT_EQ(mangleString("stringValue", "iresearch-document-empty"), field.name());
    auto const expected_analyzer = irs::analysis::analyzers::get(
        "iresearch-document-empty", irs::type<irs::text_format::vpack>::get(),
                                      arangodb::iresearch::ref<char>(VPackSlice::emptyObjectSlice()));
    auto& analyzer = dynamic_cast<EmptyAnalyzer&>(field.get_tokens());
    EXPECT_EQ(expected_analyzer->type(), analyzer.type());
    EXPECT_EQ(irs::flags({irs::type<irs::frequency>::get()}), field.features());
  }

  ++it;
  ASSERT_TRUE(it.valid());
  ASSERT_NE(it, arangodb::iresearch::FieldIterator(trx));

  // nullValue
  {
    auto& field = *it;
    EXPECT_EQ(mangleNull("nullValue"), field.name());
    auto& analyzer = dynamic_cast<irs::null_token_stream&>(field.get_tokens());
    EXPECT_TRUE(analyzer.next());
  }

  ++it;
  ASSERT_TRUE(it.valid());
  ASSERT_NE(it, arangodb::iresearch::FieldIterator(trx));

  // trueValue
  {
    auto& field = *it;
    EXPECT_EQ(mangleBool("trueValue"), field.name());
    auto& analyzer = dynamic_cast<irs::boolean_token_stream&>(field.get_tokens());
    EXPECT_TRUE(analyzer.next());
  }

  ++it;
  ASSERT_TRUE(it.valid());
  ASSERT_NE(it, arangodb::iresearch::FieldIterator(trx));

  // falseValue
  {
    auto& field = *it;
    EXPECT_EQ(mangleBool("falseValue"), field.name());
    auto& analyzer = dynamic_cast<irs::boolean_token_stream&>(field.get_tokens());
    EXPECT_TRUE(analyzer.next());
  }

  ++it;
  ASSERT_TRUE(it.valid());
  ASSERT_NE(it, arangodb::iresearch::FieldIterator(trx));

  // smallIntValue
  {
    auto& field = *it;
    EXPECT_EQ(mangleNumeric("smallIntValue"), field.name());
    auto& analyzer = dynamic_cast<irs::numeric_token_stream&>(field.get_tokens());
    EXPECT_TRUE(analyzer.next());
  }

  ++it;
  ASSERT_TRUE(it.valid());
  ASSERT_NE(it, arangodb::iresearch::FieldIterator(trx));

  // smallNegativeIntValue
  {
    auto& field = *it;
    EXPECT_EQ(mangleNumeric("smallNegativeIntValue"), field.name());
    auto& analyzer = dynamic_cast<irs::numeric_token_stream&>(field.get_tokens());
    EXPECT_TRUE(analyzer.next());
  }

  ++it;
  ASSERT_TRUE(it.valid());
  ASSERT_NE(it, arangodb::iresearch::FieldIterator(trx));

  // bigIntValue
  {
    auto& field = *it;
    EXPECT_EQ(mangleNumeric("bigIntValue"), field.name());
    auto& analyzer = dynamic_cast<irs::numeric_token_stream&>(field.get_tokens());
    EXPECT_TRUE(analyzer.next());
  }

  ++it;
  ASSERT_TRUE(it.valid());
  ASSERT_NE(it, arangodb::iresearch::FieldIterator(trx));

  // bigNegativeIntValue
  {
    auto& field = *it;
    EXPECT_EQ(mangleNumeric("bigNegativeIntValue"), field.name());
    auto& analyzer = dynamic_cast<irs::numeric_token_stream&>(field.get_tokens());
    EXPECT_TRUE(analyzer.next());
  }

  ++it;
  ASSERT_TRUE(it.valid());
  ASSERT_NE(it, arangodb::iresearch::FieldIterator(trx));

  // smallDoubleValue
  {
    auto& field = *it;
    EXPECT_EQ(mangleNumeric("smallDoubleValue"), field.name());
    auto& analyzer = dynamic_cast<irs::numeric_token_stream&>(field.get_tokens());
    EXPECT_TRUE(analyzer.next());
  }

  ++it;
  ASSERT_TRUE(it.valid());
  ASSERT_NE(it, arangodb::iresearch::FieldIterator(trx));

  // bigDoubleValue
  {
    auto& field = *it;
    EXPECT_EQ(mangleNumeric("bigDoubleValue"), field.name());
    auto& analyzer = dynamic_cast<irs::numeric_token_stream&>(field.get_tokens());
    EXPECT_TRUE(analyzer.next());
  }

  ++it;
  ASSERT_TRUE(it.valid());
  ASSERT_NE(it, arangodb::iresearch::FieldIterator(trx));

  // bigNegativeDoubleValue
  {
    auto& field = *it;
    EXPECT_EQ(mangleNumeric("bigNegativeDoubleValue"), field.name());
    auto& analyzer = dynamic_cast<irs::numeric_token_stream&>(field.get_tokens());
    EXPECT_TRUE(analyzer.next());
  }

  ++it;
  EXPECT_FALSE(it.valid());
  EXPECT_EQ(it, arangodb::iresearch::FieldIterator(trx));
}

TEST_F(IResearchDocumentTest, FieldIterator_reset) {
  auto& sysDatabase = server.getFeature<arangodb::SystemDatabaseFeature>();
  auto sysVocbase = sysDatabase.use();

  auto json0 = arangodb::velocypack::Parser::fromJson(
      "{ \
    \"boost\": \"10\", \
    \"depth\": \"20\" \
  }");

  auto json1 = arangodb::velocypack::Parser::fromJson(
      "{ \
    \"name\": \"foo\" \
  }");

  arangodb::iresearch::IResearchLinkMeta linkMeta;
  linkMeta._includeAllFields = true;  // include all fields

  std::vector<std::string> EMPTY;
  arangodb::transaction::Methods trx(arangodb::transaction::StandaloneContext::Create(*sysVocbase),
                                     EMPTY, EMPTY, EMPTY,
                                     arangodb::transaction::Options());

  arangodb::iresearch::FieldIterator it(trx);
  it.reset(json0->slice(), linkMeta);
  ASSERT_TRUE(it.valid());

  {
    auto& value = *it;
    EXPECT_EQ(mangleStringIdentity("boost"), value.name());
    const auto expected_analyzer =
        irs::analysis::analyzers::get("identity", irs::type<irs::text_format::vpack>::get(), 
                                      arangodb::iresearch::ref<char>(VPackSlice::emptyObjectSlice()));
    auto& analyzers = server.getFeature<arangodb::iresearch::IResearchAnalyzerFeature>();
    auto const expected_features = analyzers.get("identity", arangodb::QueryAnalyzerRevisions::QUERY_LATEST)->features();
    auto& analyzer = dynamic_cast<irs::analysis::analyzer&>(value.get_tokens());
    EXPECT_EQ(expected_features, value.features());
    EXPECT_EQ(expected_analyzer->type(), analyzer.type());
  }

  ++it;
  ASSERT_TRUE(it.valid());

  // depth (with IdentityAnalyzer)
  {
    auto& value = *it;
    EXPECT_EQ(mangleStringIdentity("depth"), value.name());
    const auto expected_analyzer =
        irs::analysis::analyzers::get("identity", irs::type<irs::text_format::vpack>::get(), 
                                      arangodb::iresearch::ref<char>(VPackSlice::emptyObjectSlice()));
    auto& analyzers = server.getFeature<arangodb::iresearch::IResearchAnalyzerFeature>();
    auto const expected_features = analyzers.get("identity", arangodb::QueryAnalyzerRevisions::QUERY_LATEST)->features();
    auto& analyzer = dynamic_cast<irs::analysis::analyzer&>(value.get_tokens());
    EXPECT_EQ(expected_features, value.features());
    EXPECT_EQ(expected_analyzer->type(), analyzer.type());
  }

  ++it;
  ASSERT_FALSE(it.valid());

  it.reset(json1->slice(), linkMeta);
  ASSERT_TRUE(it.valid());

  {
    auto& value = *it;
    EXPECT_EQ(mangleStringIdentity("name"), value.name());
    const auto expected_analyzer =
        irs::analysis::analyzers::get("identity", irs::type<irs::text_format::vpack>::get(), 
                                      arangodb::iresearch::ref<char>(VPackSlice::emptyObjectSlice()));
    auto& analyzers = server.getFeature<arangodb::iresearch::IResearchAnalyzerFeature>();
    auto const expected_features = analyzers.get("identity", arangodb::QueryAnalyzerRevisions::QUERY_LATEST)->features();
    auto& analyzer = dynamic_cast<irs::analysis::analyzer&>(value.get_tokens());
    EXPECT_EQ(expected_features, value.features());
    EXPECT_EQ(expected_analyzer->type(), analyzer.type());
  }

  ++it;
  ASSERT_FALSE(it.valid());
}

TEST_F(IResearchDocumentTest,
     FieldIterator_traverse_complex_object_ordered_all_fields_custom_list_offset_prefix_suffix) {
  auto& sysDatabase = server.getFeature<arangodb::SystemDatabaseFeature>();
  auto sysVocbase = sysDatabase.use();

  auto json = arangodb::velocypack::Parser::fromJson(
      "{ \
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

  std::unordered_multiset<std::string> expectedValues{
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
      mangleStringIdentity("array[2].subobj.id")};

  auto const slice = json->slice();

  arangodb::iresearch::IResearchLinkMeta linkMeta;
  linkMeta._includeAllFields = true;    // include all fields
  linkMeta._trackListPositions = true;  // allow indexes in field names

  std::vector<std::string> EMPTY;
  arangodb::transaction::Methods trx(arangodb::transaction::StandaloneContext::Create(*sysVocbase),
                                     EMPTY, EMPTY, EMPTY,
                                     arangodb::transaction::Options());

  arangodb::iresearch::FieldIterator it(trx);
  it.reset(slice, linkMeta);
  EXPECT_NE(it, arangodb::iresearch::FieldIterator(trx));

  // default analyzer
  auto const expected_analyzer =
      irs::analysis::analyzers::get("identity", irs::type<irs::text_format::vpack>::get(), 
                                    arangodb::iresearch::ref<char>(VPackSlice::emptyObjectSlice()));
  auto& analyzers = server.getFeature<arangodb::iresearch::IResearchAnalyzerFeature>();
  auto const expected_features = analyzers.get("identity", arangodb::QueryAnalyzerRevisions::QUERY_LATEST)->features();

  for (; it != arangodb::iresearch::FieldIterator(trx); ++it) {
    auto& field = *it;
    std::string const actualName = std::string(field.name());
    EXPECT_EQ(1, expectedValues.erase(actualName));

    auto& analyzer = dynamic_cast<irs::analysis::analyzer&>(field.get_tokens());
    EXPECT_EQ(expected_features, field.features());
    EXPECT_EQ(expected_analyzer->type(), analyzer.type());
  }

  EXPECT_TRUE(expectedValues.empty());
  EXPECT_EQ(it, arangodb::iresearch::FieldIterator(trx));
}

TEST_F(IResearchDocumentTest, FieldIterator_traverse_complex_object_check_meta_inheritance) {
  auto& sysDatabase = server.getFeature<arangodb::SystemDatabaseFeature>();
  auto sysVocbase = sysDatabase.use();

  auto json = arangodb::velocypack::Parser::fromJson(
      "{ \
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

  auto linkMetaJson = arangodb::velocypack::Parser::fromJson(
      "{ \
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
  ASSERT_TRUE(linkMeta.init(server.server(), linkMetaJson->slice(), false,
                            error, sysVocbase.get()->name()));

  std::vector<std::string> EMPTY;
  arangodb::transaction::Methods trx(arangodb::transaction::StandaloneContext::Create(*sysVocbase),
                                     EMPTY, EMPTY, EMPTY,
                                     arangodb::transaction::Options());

  arangodb::iresearch::FieldIterator it(trx);
  it.reset(slice, linkMeta);
  ASSERT_TRUE(it.valid());
  ASSERT_NE(it, arangodb::iresearch::FieldIterator(trx));

  // nested.foo (with IdentityAnalyzer)
  {
    auto& value = *it;
    EXPECT_EQ(mangleStringIdentity("nested.foo"), value.name());
    const auto expected_analyzer =
        irs::analysis::analyzers::get("identity", irs::type<irs::text_format::vpack>::get(),
                                      arangodb::iresearch::ref<char>(
                                          VPackSlice::emptyObjectSlice()));
    auto& analyzer = dynamic_cast<irs::analysis::analyzer&>(value.get_tokens());
    auto& analyzers = server.getFeature<arangodb::iresearch::IResearchAnalyzerFeature>();
    auto const expected_features = analyzers.get("identity", arangodb::QueryAnalyzerRevisions::QUERY_LATEST)->features();
    EXPECT_EQ(expected_features, value.features());
    EXPECT_EQ(expected_analyzer->type(), analyzer.type());
  }

  ++it;
  ASSERT_TRUE(it.valid());
  ASSERT_NE(it, arangodb::iresearch::FieldIterator(trx));

  // nested.foo (with EmptyAnalyzer)
  {
    auto& value = *it;
    EXPECT_EQ(mangleString("nested.foo", "iresearch-document-empty"), value.name());
    auto& analyzer = dynamic_cast<EmptyAnalyzer&>(value.get_tokens());
    EXPECT_FALSE(analyzer.next());
  }

  // keys[]
  for (size_t i = 0; i < 4; ++i) {
    ++it;
    ASSERT_TRUE(it.valid());
    ASSERT_NE(it, arangodb::iresearch::FieldIterator(trx));

    auto& value = *it;
    EXPECT_EQ(mangleStringIdentity("keys"), value.name());
    const auto expected_analyzer =
        irs::analysis::analyzers::get("identity", irs::type<irs::text_format::vpack>::get(),
                                      arangodb::iresearch::ref<char>(VPackSlice::emptyObjectSlice()));
    auto& analyzers = server.getFeature<arangodb::iresearch::IResearchAnalyzerFeature>();
    auto const expected_features = analyzers.get("identity", arangodb::QueryAnalyzerRevisions::QUERY_LATEST)->features();
    auto& analyzer = dynamic_cast<irs::analysis::analyzer&>(value.get_tokens());
    EXPECT_EQ(expected_features, value.features());
    EXPECT_EQ(expected_analyzer->type(), analyzer.type());
  }

  ++it;
  ASSERT_TRUE(it.valid());
  ASSERT_NE(it, arangodb::iresearch::FieldIterator(trx));

  // boost
  {
    auto& value = *it;
    EXPECT_EQ(mangleStringIdentity("boost"), value.name());
    const auto expected_analyzer =
        irs::analysis::analyzers::get("identity", irs::type<irs::text_format::vpack>::get(), 
                                      arangodb::iresearch::ref<char>(VPackSlice::emptyObjectSlice()));
    auto& analyzers = server.getFeature<arangodb::iresearch::IResearchAnalyzerFeature>();
    auto const expected_features = analyzers.get("identity", arangodb::QueryAnalyzerRevisions::QUERY_LATEST)->features();
    auto& analyzer = dynamic_cast<irs::analysis::analyzer&>(value.get_tokens());
    EXPECT_EQ(expected_features, value.features());
    EXPECT_EQ(expected_analyzer->type(), analyzer.type());
  }

  ++it;
  ASSERT_TRUE(it.valid());
  ASSERT_NE(it, arangodb::iresearch::FieldIterator(trx));

  // depth
  {
    auto& value = *it;
    EXPECT_EQ(mangleNumeric("depth"), value.name());
    auto& analyzer = dynamic_cast<irs::numeric_token_stream&>(value.get_tokens());
    EXPECT_TRUE(analyzer.next());
  }

  ++it;
  ASSERT_TRUE(it.valid());
  ASSERT_NE(it, arangodb::iresearch::FieldIterator(trx));

  // fields.fieldA (with IdenityAnalyzer)
  {
    auto& value = *it;
    EXPECT_EQ(mangleStringIdentity("fields.fieldA.name"), value.name());
    const auto expected_analyzer =
        irs::analysis::analyzers::get("identity", irs::type<irs::text_format::vpack>::get(), 
                                      arangodb::iresearch::ref<char>(VPackSlice::emptyObjectSlice()));
    auto& analyzers = server.getFeature<arangodb::iresearch::IResearchAnalyzerFeature>();
    auto const expected_features = analyzers.get("identity", arangodb::QueryAnalyzerRevisions::QUERY_LATEST)->features();
    auto& analyzer = dynamic_cast<irs::analysis::analyzer&>(value.get_tokens());
    EXPECT_EQ(expected_features, value.features());
    EXPECT_EQ(expected_analyzer->type(), analyzer.type());
  }

  ++it;
  ASSERT_TRUE(it.valid());
  ASSERT_NE(it, arangodb::iresearch::FieldIterator(trx));

  // fields.fieldA (with EmptyAnalyzer)
  {
    auto& value = *it;
    EXPECT_TRUE(mangleString("fields.fieldA.name",
                             "iresearch-document-empty") == value.name());
    auto& analyzer = dynamic_cast<EmptyAnalyzer&>(value.get_tokens());
    EXPECT_FALSE(analyzer.next());
  }

  ++it;
  ASSERT_TRUE(it.valid());
  ASSERT_NE(it, arangodb::iresearch::FieldIterator(trx));

  // listValuation (with IdenityAnalyzer)
  {
    auto& value = *it;
    EXPECT_EQ(mangleStringIdentity("listValuation"), value.name());
    const auto expected_analyzer =
        irs::analysis::analyzers::get("identity", irs::type<irs::text_format::vpack>::get(),
                                      arangodb::iresearch::ref<char>(VPackSlice::emptyObjectSlice()));
    auto& analyzers = server.getFeature<arangodb::iresearch::IResearchAnalyzerFeature>();
    auto const expected_features = analyzers.get("identity", arangodb::QueryAnalyzerRevisions::QUERY_LATEST)->features();
    auto& analyzer = dynamic_cast<irs::analysis::analyzer&>(value.get_tokens());
    EXPECT_EQ(expected_features, value.features());
    EXPECT_EQ(expected_analyzer->type(), analyzer.type());
  }

  ++it;
  ASSERT_TRUE(it.valid());
  ASSERT_NE(it, arangodb::iresearch::FieldIterator(trx));

  // listValuation (with EmptyAnalyzer)
  {
    auto& value = *it;
    EXPECT_TRUE(mangleString("listValuation", "iresearch-document-empty") ==
                value.name());
    auto& analyzer = dynamic_cast<EmptyAnalyzer&>(value.get_tokens());
    EXPECT_FALSE(analyzer.next());
  }

  ++it;
  ASSERT_TRUE(it.valid());
  ASSERT_NE(it, arangodb::iresearch::FieldIterator(trx));

  // locale
  {
    auto& value = *it;
    EXPECT_EQ(mangleNull("locale"), value.name());
    auto& analyzer = dynamic_cast<irs::null_token_stream&>(value.get_tokens());
    EXPECT_TRUE(analyzer.next());
  }

  ++it;
  ASSERT_TRUE(it.valid());
  ASSERT_NE(it, arangodb::iresearch::FieldIterator(trx));

  // array[0].id
  {
    auto& value = *it;
    EXPECT_EQ(mangleNumeric("array[0].id"), value.name());
    auto& analyzer = dynamic_cast<irs::numeric_token_stream&>(value.get_tokens());
    EXPECT_TRUE(analyzer.next());
  }

  // array[0].subarr[0-2]
  for (size_t i = 0; i < 3; ++i) {
    ++it;
    ASSERT_TRUE(it.valid());
    ASSERT_NE(it, arangodb::iresearch::FieldIterator(trx));

    // IdentityAnalyzer
    {
      auto& value = *it;
      EXPECT_EQ(mangleStringIdentity("array[0].subarr"), value.name());
      const auto expected_analyzer =
          irs::analysis::analyzers::get("identity", irs::type<irs::text_format::vpack>::get(), 
                                        arangodb::iresearch::ref<char>(VPackSlice::emptyObjectSlice()));
      auto& analyzers =
          server.getFeature<arangodb::iresearch::IResearchAnalyzerFeature>();
      auto const expected_features = analyzers.get("identity", arangodb::QueryAnalyzerRevisions::QUERY_LATEST)->features();
      auto& analyzer = dynamic_cast<irs::analysis::analyzer&>(value.get_tokens());
      EXPECT_EQ(expected_features, value.features());
      EXPECT_EQ(expected_analyzer->type(), analyzer.type());
    }

    ++it;
    ASSERT_TRUE(it.valid());
    ASSERT_NE(it, arangodb::iresearch::FieldIterator(trx));

    // EmptyAnalyzer
    {
      auto& value = *it;
      EXPECT_TRUE(mangleString("array[0].subarr", "iresearch-document-empty") ==
                  value.name());
      auto& analyzer = dynamic_cast<EmptyAnalyzer&>(value.get_tokens());
      EXPECT_FALSE(analyzer.next());
    }
  }

  // array[1].subarr[0-2]
  for (size_t i = 0; i < 3; ++i) {
    ++it;
    ASSERT_TRUE(it.valid());
    ASSERT_NE(it, arangodb::iresearch::FieldIterator(trx));

    // IdentityAnalyzer
    {
      auto& value = *it;
      EXPECT_EQ(mangleStringIdentity("array[1].subarr"), value.name());
      const auto expected_analyzer =
          irs::analysis::analyzers::get("identity", irs::type<irs::text_format::vpack>::get(),
                                        arangodb::iresearch::ref<char>(VPackSlice::emptyObjectSlice()));
      auto& analyzers =
          server.getFeature<arangodb::iresearch::IResearchAnalyzerFeature>();
      auto const expected_features = analyzers.get("identity", arangodb::QueryAnalyzerRevisions::QUERY_LATEST)->features();
      auto& analyzer = dynamic_cast<irs::analysis::analyzer&>(value.get_tokens());
      EXPECT_EQ(expected_features, value.features());
      EXPECT_EQ(expected_analyzer->type(), analyzer.type());
    }

    ++it;
    ASSERT_TRUE(it.valid());
    ASSERT_NE(it, arangodb::iresearch::FieldIterator(trx));

    // EmptyAnalyzer
    {
      auto& value = *it;
      EXPECT_TRUE(mangleString("array[1].subarr", "iresearch-document-empty") ==
                  value.name());
      auto& analyzer = dynamic_cast<EmptyAnalyzer&>(value.get_tokens());
      EXPECT_FALSE(analyzer.next());
    }
  }

  ++it;
  ASSERT_TRUE(it.valid());
  ASSERT_NE(it, arangodb::iresearch::FieldIterator(trx));

  // array[1].id (IdentityAnalyzer)
  {
    auto& value = *it;
    EXPECT_EQ(mangleStringIdentity("array[1].id"), value.name());
    const auto expected_analyzer =
        irs::analysis::analyzers::get("identity", irs::type<irs::text_format::vpack>::get(), 
                                      arangodb::iresearch::ref<char>(VPackSlice::emptyObjectSlice()));
    auto& analyzers = server.getFeature<arangodb::iresearch::IResearchAnalyzerFeature>();
    auto const expected_features = analyzers.get("identity", arangodb::QueryAnalyzerRevisions::QUERY_LATEST)->features();
    auto& analyzer = dynamic_cast<irs::analysis::analyzer&>(value.get_tokens());
    EXPECT_EQ(expected_features, value.features());
    EXPECT_EQ(expected_analyzer->type(), analyzer.type());
  }

  ++it;
  ASSERT_TRUE(it.valid());
  ASSERT_NE(it, arangodb::iresearch::FieldIterator(trx));

  // array[1].id (EmptyAnalyzer)
  {
    auto& value = *it;
    EXPECT_EQ(mangleString("array[1].id", "iresearch-document-empty"), value.name());
    auto& analyzer = dynamic_cast<EmptyAnalyzer&>(value.get_tokens());
    EXPECT_FALSE(analyzer.next());
  }

  ++it;
  ASSERT_TRUE(it.valid());
  ASSERT_NE(it, arangodb::iresearch::FieldIterator(trx));

  // array[2].id (IdentityAnalyzer)
  {
    auto& value = *it;
    EXPECT_EQ(mangleNumeric("array[2].id"), value.name());
    auto& analyzer = dynamic_cast<irs::numeric_token_stream&>(value.get_tokens());
    EXPECT_TRUE(analyzer.next());
  }

  // array[2].subarr[0-2]
  for (size_t i = 0; i < 3; ++i) {
    ++it;
    ASSERT_TRUE(it.valid());
    ASSERT_NE(it, arangodb::iresearch::FieldIterator(trx));

    // IdentityAnalyzer
    {
      auto& value = *it;
      EXPECT_EQ(mangleStringIdentity("array[2].subarr"), value.name());
      const auto expected_analyzer =
          irs::analysis::analyzers::get("identity", irs::type<irs::text_format::vpack>::get(),
                                        arangodb::iresearch::ref<char>(VPackSlice::emptyObjectSlice()));
      auto& analyzers =
          server.getFeature<arangodb::iresearch::IResearchAnalyzerFeature>();
      auto const expected_features = analyzers.get("identity", arangodb::QueryAnalyzerRevisions::QUERY_LATEST)->features();
      auto& analyzer = dynamic_cast<irs::analysis::analyzer&>(value.get_tokens());
      EXPECT_EQ(expected_features, value.features());
      EXPECT_EQ(expected_analyzer->type(), analyzer.type());
    }

    ++it;
    ASSERT_TRUE(it.valid());
    ASSERT_NE(it, arangodb::iresearch::FieldIterator(trx));

    // EmptyAnalyzer
    {
      auto& value = *it;
      EXPECT_TRUE(mangleString("array[2].subarr", "iresearch-document-empty") ==
                  value.name());
      auto& analyzer = dynamic_cast<EmptyAnalyzer&>(value.get_tokens());
      EXPECT_FALSE(analyzer.next());
    }
  }

  ++it;
  EXPECT_FALSE(it.valid());
  EXPECT_EQ(it, arangodb::iresearch::FieldIterator(trx));
}

TEST_F(IResearchDocumentTest, FieldIterator_nullptr_analyzer) {
  auto& sysDatabase = server.getFeature<arangodb::SystemDatabaseFeature>();
  auto sysVocbase = sysDatabase.use();

  arangodb::iresearch::IResearchAnalyzerFeature analyzers(server.server());
  auto json = arangodb::velocypack::Parser::fromJson(
      "{ \
    \"stringValue\": \"string\" \
  }");
  auto const slice = json->slice();

  // register analizers with feature
  {
     // ensure there will be no exception on 'start'
    InvalidAnalyzer::returnNullFromMake = false;
    InvalidAnalyzer::returnFalseFromToString = false;
    analyzers.start();

    analyzers.remove("empty");
    analyzers.remove("invalid");

    arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;
    ASSERT_TRUE(
        analyzers
            .emplace(
                result, arangodb::StaticStrings::SystemDatabase + "::empty",
                  "iresearch-document-empty",
                  arangodb::velocypack::Parser::fromJson("{ \"args\":\"en\" }")->slice(),
                irs::flags{irs::type<irs::frequency>::get()})
            .ok());

    // valid duplicate (same properties)
    ASSERT_TRUE(
        analyzers
            .emplace(
                result, arangodb::StaticStrings::SystemDatabase + "::empty",
                  "iresearch-document-empty",
                  arangodb::velocypack::Parser::fromJson("{ \"args\":\"en\" }")->slice(),
                irs::flags{irs::type<irs::frequency>::get()})
            .ok());

    // ensure there will be no exception on 'emplace'
    InvalidAnalyzer::returnFalseFromToString = true;
    ASSERT_FALSE(
        analyzers
            .emplace(
                result, arangodb::StaticStrings::SystemDatabase + "::invalid",
                 "iresearch-document-invalid",
                 arangodb::velocypack::Parser::fromJson("{ \"args\":\"en\" }")->slice(),
                irs::flags{irs::type<irs::frequency>::get()})
            .ok());
    InvalidAnalyzer::returnFalseFromToString = false;

    // ensure that there will be no exception on 'emplace'
    InvalidAnalyzer::returnNullFromMake = true;
    ASSERT_FALSE(
        analyzers
            .emplace(
                result, arangodb::StaticStrings::SystemDatabase + "::invalid",
                 "iresearch-document-invalid",
                 arangodb::velocypack::Parser::fromJson("{ \"args\":\"en\" }")->slice(),
                irs::flags{irs::type<irs::frequency>::get()})
            .ok());
    InvalidAnalyzer::returnNullFromMake = false;

    ASSERT_TRUE(
        analyzers
            .emplace(
                result, arangodb::StaticStrings::SystemDatabase + "::invalid",
                 "iresearch-document-invalid",
                 arangodb::velocypack::Parser::fromJson("{ \"args\":\"en\" }")->slice(),
                irs::flags{irs::type<irs::frequency>::get()})
            .ok());
  }

  // last analyzer invalid
  {
    arangodb::iresearch::IResearchLinkMeta linkMeta;
    linkMeta._analyzers.emplace_back(arangodb::iresearch::FieldMeta::Analyzer(
        analyzers.get(arangodb::StaticStrings::SystemDatabase + "::empty", arangodb::QueryAnalyzerRevisions::QUERY_LATEST),
        "empty"));  // add analyzer

    InvalidAnalyzer::returnNullFromMake = false;
    linkMeta._analyzers.emplace_back(arangodb::iresearch::FieldMeta::Analyzer(
        analyzers.get(arangodb::StaticStrings::SystemDatabase + "::invalid", arangodb::QueryAnalyzerRevisions::QUERY_LATEST),
        "invalid"));                    // add analyzer
    linkMeta._includeAllFields = true;  // include all fields

    // acquire analyzer, another one should be created
    auto analyzer = linkMeta._analyzers.back()._pool->get();  // cached instance should have been acquired

    InvalidAnalyzer::returnNullFromMake = true;

    std::vector<std::string> EMPTY;
    arangodb::transaction::Methods trx(arangodb::transaction::StandaloneContext::Create(*sysVocbase),
                                       EMPTY, EMPTY, EMPTY,
                                       arangodb::transaction::Options());

    arangodb::iresearch::FieldIterator it(trx);
    it.reset(slice, linkMeta);
    ASSERT_TRUE(it.valid());
    ASSERT_NE(it, arangodb::iresearch::FieldIterator(trx));

    // stringValue (with IdentityAnalyzer)
    {
      auto& field = *it;
      EXPECT_EQ(mangleStringIdentity("stringValue"), field.name());
      auto const expected_analyzer =
          irs::analysis::analyzers::get("identity", irs::type<irs::text_format::vpack>::get(), 
                                        arangodb::iresearch::ref<char>(VPackSlice::emptyObjectSlice()));
      auto& analyzers =
          server.getFeature<arangodb::iresearch::IResearchAnalyzerFeature>();
      auto const expected_features = analyzers.get("identity", arangodb::QueryAnalyzerRevisions::QUERY_LATEST)->features();
      auto& analyzer = dynamic_cast<irs::analysis::analyzer&>(field.get_tokens());
      EXPECT_EQ(expected_analyzer->type(), analyzer.type());
      EXPECT_EQ(expected_features, field.features());
    }

    ++it;
    ASSERT_TRUE(it.valid());
    ASSERT_NE(arangodb::iresearch::FieldIterator(trx), it);

    // stringValue (with EmptyAnalyzer)
    {
      auto& field = *it;
      EXPECT_EQ(mangleString("stringValue", "empty"), field.name());
      auto const expected_analyzer = irs::analysis::analyzers::get(
          "iresearch-document-empty", irs::type<irs::text_format::vpack>::get(),
              arangodb::iresearch::ref<char>(VPackSlice::emptyObjectSlice()));
      auto& analyzer = dynamic_cast<EmptyAnalyzer&>(field.get_tokens());
      EXPECT_EQ(expected_analyzer->type(), analyzer.type());
    }

    ++it;
    ASSERT_FALSE(it.valid());
    ASSERT_EQ(arangodb::iresearch::FieldIterator(trx), it);

    analyzer->reset(irs::string_ref::NIL);  // ensure that acquired 'analyzer' will not be optimized out
  }

  // first analyzer is invalid
  {
    arangodb::iresearch::IResearchLinkMeta linkMeta;
    linkMeta._analyzers.clear();

    InvalidAnalyzer::returnNullFromMake = false;
    linkMeta._analyzers.emplace_back(arangodb::iresearch::FieldMeta::Analyzer(
        analyzers.get(arangodb::StaticStrings::SystemDatabase + "::invalid", arangodb::QueryAnalyzerRevisions::QUERY_LATEST),
        "invalid"));  // add analyzer
    linkMeta._analyzers.emplace_back(arangodb::iresearch::FieldMeta::Analyzer(
        analyzers.get(arangodb::StaticStrings::SystemDatabase + "::empty", arangodb::QueryAnalyzerRevisions::QUERY_LATEST),
        "empty"));                      // add analyzer
    linkMeta._includeAllFields = true;  // include all fields

    // acquire analyzer, another one should be created
    auto analyzer = linkMeta._analyzers.front()._pool->get();  // cached instance should have been acquired
    InvalidAnalyzer::returnNullFromMake = true;

    std::vector<std::string> EMPTY;
    arangodb::transaction::Methods trx(arangodb::transaction::StandaloneContext::Create(*sysVocbase),
                                       EMPTY, EMPTY, EMPTY,
                                       arangodb::transaction::Options());

    arangodb::iresearch::FieldIterator it(trx);
    it.reset(slice, linkMeta);
    ASSERT_TRUE(it.valid());
    ASSERT_NE(it, arangodb::iresearch::FieldIterator(trx));

    // stringValue (with EmptyAnalyzer)
    {
      auto& field = *it;
      EXPECT_EQ(mangleString("stringValue", "empty"), field.name());
      auto const expected_analyzer = irs::analysis::analyzers::get(
          "iresearch-document-empty", irs::type<irs::text_format::vpack>::get(),
            arangodb::iresearch::ref<char>(VPackSlice::emptyObjectSlice()));
      auto& analyzer = dynamic_cast<EmptyAnalyzer&>(field.get_tokens());
      EXPECT_EQ(expected_analyzer->type(), analyzer.type());
    }

    ++it;
    ASSERT_FALSE(it.valid());
    ASSERT_EQ(arangodb::iresearch::FieldIterator(trx), it);

    analyzer->reset(irs::string_ref::NIL);  // ensure that acquired 'analyzer' will not be optimized out
  }
}

TEST_F(IResearchDocumentTest, test_rid_encoding) {
  auto data = arangodb::velocypack::Parser::fromJson(
      "[{ \"rid\": 1605879230128717824},"
      "{  \"rid\": 1605879230128717826},"
      "{  \"rid\": 1605879230129766400},"
      "{  \"rid\": 1605879230130814976},"
      "{  \"rid\": 1605879230130814978},"
      "{  \"rid\": 1605879230131863552},"
      "{  \"rid\": 1605879230131863554},"
      "{  \"rid\": 1605879230132912128},"
      "{  \"rid\": 1605879230133960704},"
      "{  \"rid\": 1605879230133960706},"
      "{  \"rid\": 1605879230135009280},"
      "{  \"rid\": 1605879230136057856},"
      "{  \"rid\": 1605879230136057858},"
      "{  \"rid\": 1605879230137106432},"
      "{  \"rid\": 1605879230137106434},"
      "{  \"rid\": 1605879230138155008},"
      "{  \"rid\": 1605879230138155010},"
      "{  \"rid\": 1605879230139203584},"
      "{  \"rid\": 1605879230139203586},"
      "{  \"rid\": 1605879230140252160},"
      "{  \"rid\": 1605879230140252162},"
      "{  \"rid\": 1605879230141300736},"
      "{  \"rid\": 1605879230142349312},"
      "{  \"rid\": 1605879230142349314},"
      "{  \"rid\": 1605879230142349316},"
      "{  \"rid\": 1605879230143397888},"
      "{  \"rid\": 1605879230143397890},"
      "{  \"rid\": 1605879230144446464},"
      "{  \"rid\": 1605879230144446466},"
      "{  \"rid\": 1605879230144446468},"
      "{  \"rid\": 1605879230145495040},"
      "{  \"rid\": 1605879230145495042},"
      "{  \"rid\": 1605879230145495044},"
      "{  \"rid\": 1605879230146543616},"
      "{  \"rid\": 1605879230146543618},"
      "{  \"rid\": 1605879230146543620},"
      "{  \"rid\": 1605879230147592192}]");

  struct DataStore {
    irs::memory_directory dir;
    irs::directory_reader reader;
    irs::index_writer::ptr writer;

    DataStore() {
      writer = irs::index_writer::make(dir, irs::formats::get("1_0"), irs::OM_CREATE);
      EXPECT_TRUE(writer);
      writer->commit();

      reader = irs::directory_reader::open(dir);
    }
  };

  DataStore store0, store1;

  auto const dataSlice = data->slice();

  arangodb::iresearch::Field field;
  uint64_t rid;

  size_t size = 0;
  for (auto const docSlice : arangodb::velocypack::ArrayIterator(dataSlice)) {
    auto const ridSlice = docSlice.get("rid");
    EXPECT_TRUE(ridSlice.isNumber());

    rid = ridSlice.getNumber<uint64_t>();

    auto pk = arangodb::iresearch::DocumentPrimaryKey::encode(arangodb::LocalDocumentId(rid));
    auto& writer = store0.writer;

    // insert document
    {
      auto doc = writer->documents().insert();
      arangodb::iresearch::Field::setPkValue(field, pk);
      EXPECT_TRUE(doc.insert<irs::Action::INDEX | irs::Action::STORE>(field));
      EXPECT_TRUE(doc);
    }
    writer->commit();

    ++size;
  }

  store0.reader = store0.reader->reopen();
  EXPECT_EQ(size, store0.reader->size());
  EXPECT_EQ(size, store0.reader->docs_count());

  store1.writer->import(*store0.reader);
  store1.writer->commit();

  auto reader = store1.reader->reopen();
  ASSERT_TRUE(reader);
  EXPECT_EQ(1, reader->size());
  EXPECT_EQ(size, reader->docs_count());

  size_t found = 0;
  for (auto const docSlice : arangodb::velocypack::ArrayIterator(dataSlice)) {
    auto const ridSlice = docSlice.get("rid");
    EXPECT_TRUE(ridSlice.isNumber());

    rid = ridSlice.getNumber<uint64_t>();

    auto& segment = (*reader)[0];

    auto* pkField = segment.field(arangodb::iresearch::DocumentPrimaryKey::PK());
    EXPECT_TRUE(pkField);
    EXPECT_EQ(size, pkField->docs_count());

    arangodb::iresearch::PrimaryKeyFilterContainer filters;
    EXPECT_TRUE(filters.empty());
    auto& filter = filters.emplace(arangodb::LocalDocumentId(rid));
    ASSERT_EQ(filter.type(), arangodb::iresearch::PrimaryKeyFilter::type());
    EXPECT_FALSE(filters.empty());

    // first execution
    {
      auto prepared = filter.prepare(*reader);
      ASSERT_TRUE(prepared);
      EXPECT_EQ(prepared, filter.prepare(*reader));  // same object
      EXPECT_TRUE(&filter == dynamic_cast<arangodb::iresearch::PrimaryKeyFilter const*>(
                                 prepared.get()));  // same object

      for (auto& segment : *reader) {
        auto docs = prepared->execute(segment);
        ASSERT_TRUE(docs);
        // EXPECT_EQ(nullptr, prepared->execute(segment)); // unusable filter TRI_ASSERT(...) check
        EXPECT_EQ(irs::filter::prepared::empty(), filter.prepare(*reader));  // unusable filter (after execute)

        EXPECT_TRUE(docs->next());
        auto const id = docs->value();
        ++found;
        EXPECT_FALSE(docs->next());
        EXPECT_TRUE(irs::doc_limits::eof(docs->value()));
        EXPECT_FALSE(docs->next());
        EXPECT_TRUE(irs::doc_limits::eof(docs->value()));

        auto column =
            segment.column_reader(arangodb::iresearch::DocumentPrimaryKey::PK());
        ASSERT_TRUE(column);

        auto values = column->values();
        ASSERT_TRUE(values);

        irs::bytes_ref pkValue;
        EXPECT_TRUE(values(id, pkValue));

        arangodb::LocalDocumentId pk;
        EXPECT_TRUE(arangodb::iresearch::DocumentPrimaryKey::read(pk, pkValue));
        EXPECT_EQ(rid, pk.id());
      }
    }

    // FIXME uncomment after fix
    //// can't prepare twice
    //{
    //  auto prepared = filter.prepare(*reader);
    //  ASSERT_TRUE(prepared);
    //  EXPECT_EQ(prepared, filter.prepare(*reader)); // same object

    //  for (auto& segment : *reader) {
    //    auto docs = prepared->execute(segment);
    //    ASSERT_TRUE(docs);
    //    EXPECT_EQ(docs, prepared->execute(segment)); // same object
    //    EXPECT_FALSE(docs->next());
    //    EXPECT_TRUE(irs::doc_limits::eof(docs->value()));
    //  }
    //}
  }

  EXPECT_EQ(found, size);
}

TEST_F(IResearchDocumentTest, test_rid_filter) {
  auto data = arangodb::velocypack::Parser::fromJson(
      "[{ \"rid\": 1605879230128717824},"
      "{  \"rid\": 1605879230128717826},"
      "{  \"rid\": 1605879230129766400},"
      "{  \"rid\": 1605879230130814976},"
      "{  \"rid\": 1605879230130814978},"
      "{  \"rid\": 1605879230131863552},"
      "{  \"rid\": 1605879230131863554},"
      "{  \"rid\": 1605879230132912128},"
      "{  \"rid\": 1605879230133960704},"
      "{  \"rid\": 1605879230133960706},"
      "{  \"rid\": 1605879230135009280},"
      "{  \"rid\": 1605879230136057856},"
      "{  \"rid\": 1605879230136057858},"
      "{  \"rid\": 1605879230137106432},"
      "{  \"rid\": 1605879230137106434},"
      "{  \"rid\": 1605879230138155008},"
      "{  \"rid\": 1605879230138155010},"
      "{  \"rid\": 1605879230139203584},"
      "{  \"rid\": 1605879230139203586},"
      "{  \"rid\": 1605879230140252160},"
      "{  \"rid\": 1605879230140252162},"
      "{  \"rid\": 1605879230141300736},"
      "{  \"rid\": 1605879230142349312},"
      "{  \"rid\": 1605879230142349314},"
      "{  \"rid\": 1605879230142349316},"
      "{  \"rid\": 1605879230143397888},"
      "{  \"rid\": 1605879230143397890},"
      "{  \"rid\": 1605879230144446464},"
      "{  \"rid\": 1605879230144446466},"
      "{  \"rid\": 1605879230144446468},"
      "{  \"rid\": 1605879230145495040},"
      "{  \"rid\": 1605879230145495042},"
      "{  \"rid\": 1605879230145495044},"
      "{  \"rid\": 1605879230146543616},"
      "{  \"rid\": 1605879230146543618},"
      "{  \"rid\": 1605879230146543620},"
      "{  \"rid\": 1605879230147592192}]");
  auto data1 =
      arangodb::velocypack::Parser::fromJson("{ \"rid\": 2605879230128717824}");

  struct DataStore {
    irs::memory_directory dir;
    irs::directory_reader reader;
    irs::index_writer::ptr writer;

    DataStore() {
      writer = irs::index_writer::make(dir, irs::formats::get("1_0"), irs::OM_CREATE);
      EXPECT_TRUE(writer);
      writer->commit();

      reader = irs::directory_reader::open(dir);
    }
  };

  auto const dataSlice = data->slice();
  size_t expectedDocs = 0;
  size_t expectedLiveDocs = 0;
  DataStore store;

  // initial population
  for (auto const docSlice : arangodb::velocypack::ArrayIterator(dataSlice)) {
    auto const ridSlice = docSlice.get("rid");
    EXPECT_TRUE(ridSlice.isNumber<uint64_t>());

    auto rid = ridSlice.getNumber<uint64_t>();
    arangodb::iresearch::Field field;
    auto pk = arangodb::iresearch::DocumentPrimaryKey::encode(arangodb::LocalDocumentId(rid));

    // insert document
    {
      auto ctx = store.writer->documents();
      auto doc = ctx.insert();
      arangodb::iresearch::Field::setPkValue(field, pk);
      EXPECT_TRUE(doc.insert<irs::Action::INDEX | irs::Action::STORE>(field));
      EXPECT_TRUE(doc);
      ++expectedDocs;
      ++expectedLiveDocs;
    }
  }

  // add extra doc to hold segment after others are removed
  {
    arangodb::iresearch::Field field;
    auto pk = arangodb::iresearch::DocumentPrimaryKey::encode(arangodb::LocalDocumentId(12345));
    auto ctx = store.writer->documents();
    auto doc = ctx.insert();
    arangodb::iresearch::Field::setPkValue(field, pk);
    EXPECT_TRUE(doc.insert<irs::Action::INDEX | irs::Action::STORE>(field));
    EXPECT_TRUE(doc);
  }

  store.writer->commit();
  store.reader = store.reader->reopen();
  EXPECT_EQ(1, store.reader->size());
  EXPECT_EQ(expectedDocs + 1, store.reader->docs_count());  // +1 for keep-alive doc
  EXPECT_EQ(expectedLiveDocs + 1, store.reader->live_docs_count());  // +1 for keep-alive doc

  // check regular filter case (unique rid)
  {
    size_t actualDocs = 0;

    for (auto const docSlice : arangodb::velocypack::ArrayIterator(dataSlice)) {
      auto const ridSlice = docSlice.get("rid");
      EXPECT_TRUE(ridSlice.isNumber<uint64_t>());

      auto rid = ridSlice.getNumber<uint64_t>();
      arangodb::iresearch::PrimaryKeyFilterContainer filters;
      EXPECT_TRUE(filters.empty());
      auto& filter = filters.emplace(arangodb::LocalDocumentId(rid));
      ASSERT_EQ(filter.type(), arangodb::iresearch::PrimaryKeyFilter::type());
      EXPECT_FALSE(filters.empty());

      auto prepared = filter.prepare(*store.reader);
      ASSERT_TRUE(prepared);
      EXPECT_EQ(prepared, filter.prepare(*store.reader));  // same object
      EXPECT_TRUE((&filter == dynamic_cast<arangodb::iresearch::PrimaryKeyFilter const*>(
                                  prepared.get())));  // same object

      for (auto& segment : *store.reader) {
        auto docs = prepared->execute(segment);
        ASSERT_TRUE(docs);
        // EXPECT_EQ(nullptr, prepared->execute(segment)); // unusable filter TRI_ASSERT(...) check
        EXPECT_EQ(irs::filter::prepared::empty(), filter.prepare(*store.reader));  // unusable filter (after execute)

        EXPECT_TRUE(docs->next());
        auto const id = docs->value();
        ++actualDocs;
        EXPECT_FALSE(docs->next());
        EXPECT_TRUE(irs::doc_limits::eof(docs->value()));
        EXPECT_FALSE(docs->next());
        EXPECT_TRUE(irs::doc_limits::eof(docs->value()));

        auto column =
            segment.column_reader(arangodb::iresearch::DocumentPrimaryKey::PK());
        ASSERT_TRUE(column);

        auto values = column->values();
        ASSERT_TRUE(values);

        irs::bytes_ref pkValue;
        EXPECT_TRUE(values(id, pkValue));

        arangodb::LocalDocumentId pk;
        EXPECT_TRUE(arangodb::iresearch::DocumentPrimaryKey::read(pk, pkValue));
        EXPECT_EQ(rid, pk.id());
      }
    }

    EXPECT_EQ(expectedDocs, actualDocs);
  }

  // remove + insert (simulate recovery)
  for (auto const docSlice : arangodb::velocypack::ArrayIterator(dataSlice)) {
    auto const ridSlice = docSlice.get("rid");
    EXPECT_TRUE(ridSlice.isNumber<uint64_t>());

    auto rid = ridSlice.getNumber<uint64_t>();
    arangodb::iresearch::Field field;
    auto pk = arangodb::iresearch::DocumentPrimaryKey::encode(arangodb::LocalDocumentId(rid));

    // remove + insert document
    {
      auto ctx = store.writer->documents();
      ctx.remove(std::make_shared<arangodb::iresearch::PrimaryKeyFilter>(
          arangodb::LocalDocumentId(rid)));
      auto doc = ctx.insert();
      arangodb::iresearch::Field::setPkValue(field, pk);
      EXPECT_TRUE(doc.insert<irs::Action::INDEX | irs::Action::STORE>(field));
      EXPECT_TRUE(doc);
      ++expectedDocs;
    }
  }

  // add extra doc to hold segment after others are removed
  {
    arangodb::iresearch::Field field;
    auto pk = arangodb::iresearch::DocumentPrimaryKey::encode(
        arangodb::LocalDocumentId(123456));
    auto ctx = store.writer->documents();
    auto doc = ctx.insert();
    arangodb::iresearch::Field::setPkValue(field, pk);
    EXPECT_TRUE(doc.insert<irs::Action::INDEX | irs::Action::STORE>(field));
    EXPECT_TRUE(doc);
  }

  store.writer->commit();
  store.reader = store.reader->reopen();
  EXPECT_EQ(2, store.reader->size());
  EXPECT_EQ(expectedDocs + 2, store.reader->docs_count());  // +2 for keep-alive doc
  EXPECT_EQ(expectedLiveDocs + 2, store.reader->live_docs_count());  // +2 for keep-alive doc

  // check 1st recovery case
  {
    size_t actualDocs = 0;

    auto beforeRecovery = StorageEngineMock::recoveryStateResult;
    StorageEngineMock::recoveryStateResult = arangodb::RecoveryState::IN_PROGRESS;
    auto restoreRecovery = irs::make_finally([&beforeRecovery]() -> void {
      StorageEngineMock::recoveryStateResult = beforeRecovery;
    });

    for (auto const docSlice : arangodb::velocypack::ArrayIterator(dataSlice)) {
      auto const ridSlice = docSlice.get("rid");
      EXPECT_TRUE(ridSlice.isNumber<uint64_t>());

      auto rid = ridSlice.getNumber<uint64_t>();
      arangodb::iresearch::PrimaryKeyFilterContainer filters;
      EXPECT_TRUE(filters.empty());
      auto& filter = filters.emplace(arangodb::LocalDocumentId(rid));
      ASSERT_EQ(filter.type(), arangodb::iresearch::PrimaryKeyFilter::type());
      EXPECT_FALSE(filters.empty());

      auto prepared = filter.prepare(*store.reader);
      ASSERT_TRUE(prepared);
      EXPECT_EQ(prepared, filter.prepare(*store.reader));  // same object
      EXPECT_TRUE((&filter == dynamic_cast<arangodb::iresearch::PrimaryKeyFilter const*>(
                                  prepared.get())));  // same object

      for (auto& segment : *store.reader) {
        auto docs = prepared->execute(segment);
        ASSERT_TRUE(docs);
        EXPECT_NE(nullptr, prepared->execute(segment));  // usable filter
        EXPECT_NE(nullptr, filter.prepare(*store.reader));  // usable filter (after execute)

        if (docs->next()) {  // old segments will not have any matching docs
          auto const id = docs->value();
          ++actualDocs;
          EXPECT_FALSE(docs->next());
          EXPECT_TRUE(irs::doc_limits::eof(docs->value()));
          EXPECT_FALSE(docs->next());
          EXPECT_TRUE(irs::doc_limits::eof(docs->value()));

          auto column =
              segment.column_reader(arangodb::iresearch::DocumentPrimaryKey::PK());
          ASSERT_TRUE(column);

          auto values = column->values();
          ASSERT_TRUE(values);

          irs::bytes_ref pkValue;
          EXPECT_TRUE(values(id, pkValue));

          arangodb::LocalDocumentId pk;
          EXPECT_TRUE(arangodb::iresearch::DocumentPrimaryKey::read(pk, pkValue));
          EXPECT_EQ(rid, pk.id());
        }
      }
    }

    EXPECT_EQ(expectedLiveDocs, actualDocs);
  }

  // remove + insert (simulate recovery) 2nd time
  for (auto const docSlice : arangodb::velocypack::ArrayIterator(dataSlice)) {
    auto const ridSlice = docSlice.get("rid");
    EXPECT_TRUE(ridSlice.isNumber<uint64_t>());

    auto rid = ridSlice.getNumber<uint64_t>();
    arangodb::iresearch::Field field;
    auto pk = arangodb::iresearch::DocumentPrimaryKey::encode(arangodb::LocalDocumentId(rid));

    // remove + insert document
    {
      auto ctx = store.writer->documents();
      ctx.remove(std::make_shared<arangodb::iresearch::PrimaryKeyFilter>(
          arangodb::LocalDocumentId(rid)));
      auto doc = ctx.insert();
      arangodb::iresearch::Field::setPkValue(field, pk);
      EXPECT_TRUE(doc.insert<irs::Action::INDEX | irs::Action::STORE>(field));
      EXPECT_TRUE(doc);
      ++expectedDocs;
    }
  }

  // add extra doc to hold segment after others are removed
  {
    arangodb::iresearch::Field field;
    auto pk = arangodb::iresearch::DocumentPrimaryKey::encode(
        arangodb::LocalDocumentId(1234567));
    auto ctx = store.writer->documents();
    auto doc = ctx.insert();
    arangodb::iresearch::Field::setPkValue(field, pk);
    EXPECT_TRUE(doc.insert<irs::Action::INDEX | irs::Action::STORE>(field));
    EXPECT_TRUE(doc);
  }

  store.writer->commit();
  store.reader = store.reader->reopen();
  EXPECT_EQ(3, store.reader->size());
  EXPECT_EQ(expectedDocs + 3, store.reader->docs_count());  // +3 for keep-alive doc
  EXPECT_EQ(expectedLiveDocs + 3, store.reader->live_docs_count());  // +3 for keep-alive doc

  // check 2nd recovery case
  {
    size_t actualDocs = 0;

    auto beforeRecovery = StorageEngineMock::recoveryStateResult;
    StorageEngineMock::recoveryStateResult = arangodb::RecoveryState::IN_PROGRESS;
    auto restoreRecovery = irs::make_finally([&beforeRecovery]() -> void {
      StorageEngineMock::recoveryStateResult = beforeRecovery;
    });

    for (auto const docSlice : arangodb::velocypack::ArrayIterator(dataSlice)) {
      auto const ridSlice = docSlice.get("rid");
      EXPECT_TRUE(ridSlice.isNumber<uint64_t>());

      auto rid = ridSlice.getNumber<uint64_t>();
      arangodb::iresearch::PrimaryKeyFilterContainer filters;
      EXPECT_TRUE(filters.empty());
      auto& filter = filters.emplace(arangodb::LocalDocumentId(rid));
      ASSERT_EQ(filter.type(), arangodb::iresearch::PrimaryKeyFilter::type());
      EXPECT_FALSE(filters.empty());

      auto prepared = filter.prepare(*store.reader);
      ASSERT_TRUE(prepared);
      EXPECT_EQ(prepared, filter.prepare(*store.reader));  // same object
      EXPECT_TRUE((&filter == dynamic_cast<arangodb::iresearch::PrimaryKeyFilter const*>(
                                  prepared.get())));  // same object

      for (auto& segment : *store.reader) {
        auto docs = prepared->execute(segment);
        ASSERT_TRUE(docs);
        EXPECT_NE(nullptr, prepared->execute(segment));  // usable filter
        EXPECT_NE(nullptr, filter.prepare(*store.reader));  // usable filter (after execute)

        if (docs->next()) {  // old segments will not have any matching docs
          auto const id = docs->value();
          ++actualDocs;
          EXPECT_FALSE(docs->next());
          EXPECT_TRUE(irs::doc_limits::eof(docs->value()));
          EXPECT_FALSE(docs->next());
          EXPECT_TRUE(irs::doc_limits::eof(docs->value()));

          auto column =
              segment.column_reader(arangodb::iresearch::DocumentPrimaryKey::PK());
          ASSERT_TRUE(column);

          auto values = column->values();
          ASSERT_TRUE(values);

          irs::bytes_ref pkValue;
          EXPECT_TRUE(values(id, pkValue));

          arangodb::LocalDocumentId pk;
          EXPECT_TRUE(arangodb::iresearch::DocumentPrimaryKey::read(pk, pkValue));
          EXPECT_EQ(rid, pk.id());
        }
      }
    }

    EXPECT_EQ(expectedLiveDocs, actualDocs);
  }
}
