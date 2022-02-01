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
#include "IResearch/IResearchIdentityAnalyzer.h"
#include "IResearch/IResearchKludge.h"
#include "IResearch/IResearchLinkMeta.h"
#include "IResearch/IResearchPrimaryKeyFilter.h"
#include "IResearch/IResearchVPackTermAttribute.h"
#include "IResearch/GeoAnalyzer.h"
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
static const VPackSlice systemDatabaseArgs = systemDatabaseBuilder.slice();

using AssertFieldFunc = void(arangodb::tests::mocks::MockAqlServer&,
                             arangodb::iresearch::FieldIterator const&);

using AssertInvertedIndexFieldFunc =
    void(arangodb::tests::mocks::MockAqlServer&,
         arangodb::iresearch::InvertedIndexFieldIterator const&);

using IdentityAnalyzer = arangodb::iresearch::IdentityAnalyzer;

template<typename Analyzer, bool inverted = false>
void assertField(arangodb::tests::mocks::MockAqlServer& server,
                 arangodb::iresearch::Field const& value,
                 std::string const& expectedName,
                 irs::string_ref explicitAnalyzerName = irs::string_ref::NIL) {
  SCOPED_TRACE(testing::Message("Expected name: ") << expectedName);

  auto* analyzer = dynamic_cast<Analyzer*>(&value.get_tokens());
  ASSERT_NE(nullptr, analyzer);
  if constexpr (std::is_same_v<irs::null_token_stream, Analyzer> ||
                std::is_same_v<irs::string_token_stream, Analyzer> ||
                std::is_same_v<irs::numeric_token_stream, Analyzer> ||
                std::is_same_v<irs::boolean_token_stream, Analyzer>) {
    ASSERT_EQ(expectedName, value.name());
  } else if (std::is_base_of_v<irs::analysis::analyzer, Analyzer>) {
    auto analyzerName = explicitAnalyzerName.null()
                            ? std::string(irs::type<Analyzer>::name())
                            : std::string(explicitAnalyzerName);
    if constexpr (inverted) {
      ASSERT_EQ(expectedName, value.name());
    } else {
      ASSERT_EQ(mangleString(expectedName, analyzerName), value.name());
    }
    auto& analyzers = server.template getFeature<
        arangodb::iresearch::IResearchAnalyzerFeature>();
    const auto expectedAnalyzerPtr = irs::analysis::analyzers::get(
        irs::type<Analyzer>::name(), irs::type<irs::text_format::vpack>::get(),
        arangodb::iresearch::ref<char>(VPackSlice::emptyObjectSlice()));
    ASSERT_NE(nullptr, expectedAnalyzerPtr);

    const auto prefix =
        !std::is_same_v<arangodb::iresearch::IdentityAnalyzer, Analyzer>
            ? arangodb::StaticStrings::SystemDatabase + "::"
            : "";

    auto const expectedArangoSearchAnalyzerPtr =
        analyzers.get(prefix + analyzerName,
                      arangodb::QueryAnalyzerRevisions::QUERY_LATEST, true);
    ASSERT_NE(nullptr, expectedArangoSearchAnalyzerPtr);

    ASSERT_EQ(expectedAnalyzerPtr->type(), analyzer->type());
    ASSERT_EQ(expectedArangoSearchAnalyzerPtr->features().fieldFeatures(
                  inverted ? arangodb::iresearch::LinkVersion::MAX
                           : arangodb::iresearch::LinkVersion::MIN),
              value.features());
    ASSERT_EQ(expectedArangoSearchAnalyzerPtr->features().indexFeatures(),
              value.index_features());
  } else if constexpr (std::is_base_of_v<irs::token_stream, Analyzer>) {
    ASSERT_EQ(expectedName, value.name());
  }
}

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

  EmptyAnalyzer() : irs::analysis::analyzer(irs::type<EmptyAnalyzer>::get()) {}
  virtual irs::attribute* get_mutable(
      irs::type_info::type_id type) noexcept override {
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

REGISTER_ANALYZER_VPACK(EmptyAnalyzer, EmptyAnalyzer::make,
                        EmptyAnalyzer::normalize);

class VPackAnalyzer : public irs::analysis::analyzer {
 public:
  static constexpr irs::string_ref type_name() noexcept {
    return "iresearch-vpack-analyzer";
  }
  static ptr make(irs::string_ref const&) {
    PTR_NAMED(VPackAnalyzer, ptr);
    return ptr;
  }
  static bool normalize(irs::string_ref const&, std::string& out) {
    out.resize(VPackSlice::emptyObjectSlice().byteSize());
    std::memcpy(&out[0], VPackSlice::emptyObjectSlice().begin(), out.size());
    return true;
  }

  VPackAnalyzer() : irs::analysis::analyzer(irs::type<VPackAnalyzer>::get()) {}
  virtual irs::attribute* get_mutable(
      irs::type_info::type_id type) noexcept override {
    if (type == irs::type<irs::term_attribute>::id()) {
      return &_term;
    }
    if (type == irs::type<irs::increment>::id()) {
      return &_inc;
    }
    return nullptr;
  }
  virtual bool next() override {
    if (!_term.value.null()) {
      return false;
    }
    _term.value = irs::ref_cast<irs::byte_type>(_buf);
    return true;
  }
  virtual bool reset(irs::string_ref const& data) override {
    _buf = arangodb::iresearch::slice(data).toString();
    _term.value = irs::bytes_ref::NIL;
    return true;
  }

 private:
  std::string _buf;
  irs::term_attribute _term;
  irs::increment _inc;
};

REGISTER_ANALYZER_VPACK(VPackAnalyzer, VPackAnalyzer::make,
                        VPackAnalyzer::normalize);

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

  InvalidAnalyzer()
      : irs::analysis::analyzer(irs::type<InvalidAnalyzer>::get()) {}
  virtual irs::attribute* get_mutable(
      irs::type_info::type_id type) noexcept override {
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

REGISTER_ANALYZER_VPACK(InvalidAnalyzer, InvalidAnalyzer::make,
                        InvalidAnalyzer::normalize);

class TypedAnalyzer : public irs::analysis::analyzer {
 public:
  static constexpr irs::string_ref type_name() noexcept {
    return "iresearch-document-typed";
  }

  static ptr make(irs::string_ref const& args) {
    PTR_NAMED(TypedAnalyzer, ptr, args);
    return ptr;
  }

  static bool normalize(irs::string_ref const& args, std::string& out) {
    out.assign(args.c_str(), args.size());
    return true;
  }

  explicit TypedAnalyzer(irs::string_ref const& args)
      : irs::analysis::analyzer(irs::type<TypedAnalyzer>::get()) {
    VPackSlice slice(irs::ref_cast<irs::byte_type>(args).c_str());
    if (slice.hasKey("type")) {
      auto type = slice.get("type").stringView();
      if (type == "number") {
        _returnType.value = arangodb::iresearch::AnalyzerValueType::Number;
        _typedValue =
            arangodb::aql::AqlValue(arangodb::aql::AqlValueHintDouble(1));
        _vpackTerm.value = _typedValue.slice();
      } else if (type == "bool") {
        _returnType.value = arangodb::iresearch::AnalyzerValueType::Bool;
        _typedValue =
            arangodb::aql::AqlValue(arangodb::aql::AqlValueHintBool(true));
        _vpackTerm.value = _typedValue.slice();
      } else if (type == "string") {
        _returnType.value = arangodb::iresearch::AnalyzerValueType::String;
        _term.value = irs::ref_cast<irs::byte_type>(_strVal);
      } else {
        // Failure here means we have unexpected type
        EXPECT_TRUE(false);
      }
    }
  }

  virtual bool reset(irs::string_ref const& data) override {
    _resetted = true;
    return true;
  }

  virtual bool next() override {
    if (_resetted) {
      _resetted = false;
      return true;
    } else {
      return false;
    }
  }

  virtual irs::attribute* get_mutable(
      irs::type_info::type_id type) noexcept override {
    if (type == irs::type<irs::term_attribute>::id()) {
      return &_term;
    }
    if (type == irs::type<irs::increment>::id()) {
      return &_inc;
    }
    if (type ==
        irs::type<arangodb::iresearch::AnalyzerValueTypeAttribute>::id()) {
      return &_returnType;
    }
    if (type == irs::type<arangodb::iresearch::VPackTermAttribute>::id()) {
      return &_vpackTerm;
    }
    return nullptr;
  }

 private:
  std::string _strVal;
  irs::term_attribute _term;
  arangodb::iresearch::VPackTermAttribute _vpackTerm;
  irs::increment _inc;
  bool _resetted{false};
  arangodb::iresearch::AnalyzerValueTypeAttribute _returnType;
  arangodb::aql::AqlValue _typedValue;
};

REGISTER_ANALYZER_VPACK(TypedAnalyzer, TypedAnalyzer::make,
                        TypedAnalyzer::normalize);

class TypedArrayAnalyzer : public irs::analysis::analyzer {
 public:
  static constexpr irs::string_ref type_name() noexcept {
    return "iresearch-document-typed-array";
  }

  static ptr make(irs::string_ref const& args) {
    PTR_NAMED(TypedArrayAnalyzer, ptr, args);
    return ptr;
  }

  static bool normalize(irs::string_ref const& args, std::string& out) {
    out.assign(args.c_str(), args.size());
    return true;
  }

  explicit TypedArrayAnalyzer(irs::string_ref const&)
      : irs::analysis::analyzer(irs::type<TypedArrayAnalyzer>::get()) {
    _returnType.value = arangodb::iresearch::AnalyzerValueType::Number;
  }

  virtual bool reset(irs::string_ref const& data) override {
    auto value = std::string(data);
    _values.clear();
    _current = 0;
    for (double d = 1; d < std::atoi(value.c_str()); ++d) {
      _values.push_back(d);
    }
    return true;
  }

  virtual bool next() override {
    if (_current < _values.size()) {
      _typedValue = arangodb::aql::AqlValue(
          arangodb::aql::AqlValueHintDouble(_values[_current++]));
      _vpackTerm.value = _typedValue.slice();
      return true;
    } else {
      return false;
    }
  }

  virtual irs::attribute* get_mutable(
      irs::type_info::type_id type) noexcept override {
    if (type == irs::type<irs::increment>::id()) {
      return &_inc;
    }
    if (type ==
        irs::type<arangodb::iresearch::AnalyzerValueTypeAttribute>::id()) {
      return &_returnType;
    }
    if (type == irs::type<arangodb::iresearch::VPackTermAttribute>::id()) {
      return &_vpackTerm;
    }
    return nullptr;
  }

 private:
  arangodb::iresearch::VPackTermAttribute _vpackTerm;
  irs::increment _inc;
  std::vector<double> _values;
  size_t _current{0};
  arangodb::iresearch::AnalyzerValueTypeAttribute _returnType;
  arangodb::aql::AqlValue _typedValue;
};

REGISTER_ANALYZER_VPACK(TypedArrayAnalyzer, TypedArrayAnalyzer::make,
                        TypedArrayAnalyzer::normalize);

}  // namespace

namespace std {
// helper for checking features transfer from analyzer to the field
bool operator==(std::vector<irs::type_info::type_id> const& analyzer,
                irs::features_t const& field) {
  if (field.size() != analyzer.size()) {
    return false;
  }
  for (auto t : field) {
    if (find(analyzer.begin(), analyzer.end(), t().id()) == analyzer.end()) {
      return false;
    }
  }
  return true;
}
}  // namespace std
// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

class IResearchDocumentTest
    : public ::testing::Test,
      public arangodb::tests::LogSuppressor<arangodb::Logger::AUTHENTICATION,
                                            arangodb::LogLevel::ERR> {
 protected:
  arangodb::tests::mocks::MockAqlServer server;

  IResearchDocumentTest() {
    arangodb::tests::init();

    {
      auto& sysDatabase = server.getFeature<arangodb::SystemDatabaseFeature>();
      auto vocbase = sysDatabase.use();
      std::shared_ptr<arangodb::LogicalCollection> unused;
      arangodb::OperationOptions options(arangodb::ExecContext::current());
      arangodb::methods::Collections::createSystem(
          *vocbase, options, arangodb::tests::AnalyzerCollectionName, false,
          unused);
    }

    auto& analyzers =
        server.getFeature<arangodb::iresearch::IResearchAnalyzerFeature>();
    arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;

    // ensure that there will be no exception on 'emplace'
    InvalidAnalyzer::returnNullFromMake = false;
    InvalidAnalyzer::returnFalseFromToString = false;

    auto res = analyzers.emplace(
        result,
        arangodb::StaticStrings::SystemDatabase + "::iresearch-document-empty",
        "iresearch-document-empty",
        arangodb::velocypack::Parser::fromJson("{ \"args\": \"en\" }")->slice(),
        arangodb::iresearch::Features(irs::IndexFeatures::FREQ));
    EXPECT_TRUE(res.ok());

    res = analyzers.emplace(
        result,
        arangodb::StaticStrings::SystemDatabase +
            "::iresearch-document-invalid",
        "iresearch-document-invalid",
        arangodb::velocypack::Parser::fromJson("{ \"args\": \"en\" }")->slice(),
        arangodb::iresearch::Features(irs::IndexFeatures::FREQ));
    EXPECT_TRUE(res.ok());

    res = analyzers.emplace(
        result,
        arangodb::StaticStrings::SystemDatabase + "::iresearch-vpack-analyzer",
        "iresearch-vpack-analyzer", VPackSlice::emptyObjectSlice(),
        arangodb::iresearch::Features{});
    EXPECT_TRUE(res.ok());

    res = analyzers.emplace(
        result,
        arangodb::StaticStrings::SystemDatabase + "::iresearch-document-number",
        "iresearch-document-typed",
        arangodb::velocypack::Parser::fromJson("{ \"type\": \"number\" }")
            ->slice(),
        arangodb::iresearch::Features{});
    EXPECT_TRUE(res.ok());
    res = analyzers.emplace(
        result,
        arangodb::StaticStrings::SystemDatabase + "::iresearch-document-bool",
        "iresearch-document-typed",
        arangodb::velocypack::Parser::fromJson("{ \"type\": \"bool\" }")
            ->slice(),
        arangodb::iresearch::Features{});
    EXPECT_TRUE(res.ok());
    res = analyzers.emplace(
        result,
        arangodb::StaticStrings::SystemDatabase + "::iresearch-document-string",
        "iresearch-document-typed",
        arangodb::velocypack::Parser::fromJson("{ \"type\": \"string\" }")
            ->slice(),
        arangodb::iresearch::Features{});
    EXPECT_TRUE(res.ok());

    res = analyzers.emplace(
        result,
        arangodb::StaticStrings::SystemDatabase +
            "::iresearch-document-number-array",
        "iresearch-document-typed-array",
        arangodb::velocypack::Parser::fromJson("{ \"type\": \"number\" }")
            ->slice(),
        arangodb::iresearch::Features{});
    EXPECT_TRUE(res.ok());

    res = analyzers.emplace(
        result, arangodb::StaticStrings::SystemDatabase + "::my_geo", "geojson",
        arangodb::velocypack::Parser::fromJson("{\"type\": \"point\"}")
            ->slice(),
        arangodb::iresearch::Features{});
    EXPECT_TRUE(res.ok());
  }
};

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

TEST_F(IResearchDocumentTest, FieldIterator_construct) {
  auto& sysDatabase = server.getFeature<arangodb::SystemDatabaseFeature>();
  auto sysVocbase = sysDatabase.use();

  std::vector<std::string> EMPTY;
  arangodb::transaction::Methods trx(
      arangodb::transaction::StandaloneContext::Create(*sysVocbase), EMPTY,
      EMPTY, EMPTY, arangodb::transaction::Options());

  arangodb::iresearch::FieldIterator it(trx, irs::string_ref::EMPTY,
                                        arangodb::IndexId(0));
  EXPECT_FALSE(it.valid());
}

TEST_F(IResearchDocumentTest, FieldIterator_empty_object) {
  auto& sysDatabase = server.getFeature<arangodb::SystemDatabaseFeature>();
  auto sysVocbase = sysDatabase.use();

  std::vector<std::string> EMPTY;
  arangodb::transaction::Methods trx(
      arangodb::transaction::StandaloneContext::Create(*sysVocbase), EMPTY,
      EMPTY, EMPTY, arangodb::transaction::Options());

  arangodb::iresearch::IResearchLinkMeta linkMeta;
  linkMeta._includeAllFields = true;  // include all fields

  arangodb::iresearch::FieldIterator it(trx, irs::string_ref::EMPTY,
                                        arangodb::IndexId(0));
  it.reset(VPackSlice::emptyObjectSlice(), linkMeta);
  EXPECT_FALSE(it.valid());
}

TEST_F(IResearchDocumentTest,
       FieldIterator_traverse_complex_object_custom_nested_delimiter) {
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
  arangodb::transaction::Methods trx(
      arangodb::transaction::StandaloneContext::Create(*sysVocbase), EMPTY,
      EMPTY, EMPTY, arangodb::transaction::Options());

  arangodb::iresearch::FieldIterator it(trx, irs::string_ref::EMPTY,
                                        arangodb::IndexId(0));
  it.reset(slice, linkMeta);

  // default analyzer
  auto const expected_analyzer = irs::analysis::analyzers::get(
      "identity", irs::type<irs::text_format::vpack>::get(),
      arangodb::iresearch::ref<char>(VPackSlice::emptyObjectSlice()));
  auto& analyzers =
      server.getFeature<arangodb::iresearch::IResearchAnalyzerFeature>();
  auto const expected_features =
      analyzers.get("identity", arangodb::QueryAnalyzerRevisions::QUERY_LATEST)
          ->features();

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
    SCOPED_TRACE(field._name);
    EXPECT_EQ(
        expected_features.fieldFeatures(arangodb::iresearch::LinkVersion::MIN),
        field.features());
    EXPECT_EQ(expected_features.indexFeatures(), field.index_features());
    EXPECT_EQ(expected_analyzer->type(), analyzer.type());

    ++it;
  }

  EXPECT_TRUE(expectedValues.empty());
}

TEST_F(IResearchDocumentTest,
       FieldIterator_traverse_complex_object_all_fields) {
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
      {mangleStringIdentity("array.subobj.name"), 1}};

  auto const slice = json->slice();

  arangodb::iresearch::IResearchLinkMeta linkMeta;
  linkMeta._includeAllFields = true;

  std::vector<std::string> EMPTY;
  arangodb::transaction::Methods trx(
      arangodb::transaction::StandaloneContext::Create(*sysVocbase), EMPTY,
      EMPTY, EMPTY, arangodb::transaction::Options());

  arangodb::iresearch::FieldIterator it(trx, irs::string_ref::EMPTY,
                                        arangodb::IndexId(0));
  it.reset(slice, linkMeta);

  // default analyzer
  auto const expected_analyzer = irs::analysis::analyzers::get(
      "identity", irs::type<irs::text_format::vpack>::get(),
      arangodb::iresearch::ref<char>(VPackSlice::emptyObjectSlice()));
  auto& analyzers =
      server.getFeature<arangodb::iresearch::IResearchAnalyzerFeature>();
  auto const expected_features =
      analyzers.get("identity", arangodb::QueryAnalyzerRevisions::QUERY_LATEST)
          ->features();

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
    EXPECT_EQ(
        expected_features.fieldFeatures(arangodb::iresearch::LinkVersion::MIN),
        field.features());
    EXPECT_EQ(expected_features.indexFeatures(), field.index_features());
    EXPECT_EQ(expected_analyzer->type(), analyzer.type());

    ++it;
  }

  EXPECT_TRUE(expectedValues.empty());
}

TEST_F(IResearchDocumentTest,
       FieldIterator_traverse_complex_object_ordered_all_fields) {
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
  auto const expected_analyzer = irs::analysis::analyzers::get(
      "identity", irs::type<irs::text_format::vpack>::get(),
      arangodb::iresearch::ref<char>(VPackSlice::emptyObjectSlice()));
  auto& analyzers =
      server.getFeature<arangodb::iresearch::IResearchAnalyzerFeature>();
  auto const expected_features =
      analyzers.get("identity", arangodb::QueryAnalyzerRevisions::QUERY_LATEST)
          ->features();

  std::vector<std::string> EMPTY;
  arangodb::transaction::Methods trx(
      arangodb::transaction::StandaloneContext::Create(*sysVocbase), EMPTY,
      EMPTY, EMPTY, arangodb::transaction::Options());

  arangodb::iresearch::FieldIterator doc(trx, irs::string_ref::EMPTY,
                                         arangodb::IndexId(0));
  doc.reset(slice, linkMeta);
  for (; doc.valid(); ++doc) {
    auto& field = *doc;
    std::string const actualName = std::string(field.name());
    EXPECT_EQ(1, expectedValues.erase(actualName));

    auto& analyzer = dynamic_cast<irs::analysis::analyzer&>(field.get_tokens());
    EXPECT_EQ(
        expected_features.fieldFeatures(arangodb::iresearch::LinkVersion::MIN),
        field.features());
    EXPECT_EQ(expected_features.indexFeatures(), field.index_features());
    EXPECT_EQ(expected_analyzer->type(), analyzer.type());
  }

  EXPECT_TRUE(expectedValues.empty());
}

TEST_F(IResearchDocumentTest,
       FieldIterator_traverse_complex_object_ordered_filtered) {
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
  ASSERT_TRUE(linkMeta.init(server.server(), linkMetaJson->slice(), error));

  std::vector<std::string> EMPTY;
  arangodb::transaction::Methods trx(
      arangodb::transaction::StandaloneContext::Create(*sysVocbase), EMPTY,
      EMPTY, EMPTY, arangodb::transaction::Options());

  arangodb::iresearch::FieldIterator it(trx, irs::string_ref::EMPTY,
                                        arangodb::IndexId(0));
  it.reset(slice, linkMeta);
  ASSERT_TRUE(it.valid());

  auto& value = *it;
  EXPECT_EQ(mangleStringIdentity("boost"), value.name());
  const auto expected_analyzer = irs::analysis::analyzers::get(
      "identity", irs::type<irs::text_format::vpack>::get(),
      arangodb::iresearch::ref<char>(VPackSlice::emptyObjectSlice()));
  auto& analyzers =
      server.getFeature<arangodb::iresearch::IResearchAnalyzerFeature>();
  auto const expected_features =
      analyzers.get("identity", arangodb::QueryAnalyzerRevisions::QUERY_LATEST)
          ->features();
  auto& analyzer = dynamic_cast<irs::analysis::analyzer&>(value.get_tokens());
  EXPECT_EQ(
      expected_features.fieldFeatures(arangodb::iresearch::LinkVersion::MIN),
      value.features());
  EXPECT_EQ(expected_features.indexFeatures(), value.index_features());
  EXPECT_EQ(expected_analyzer->type(), analyzer.type());

  ++it;
  EXPECT_FALSE(it.valid());
}

TEST_F(IResearchDocumentTest,
       FieldIterator_traverse_complex_object_ordered_filtered_2) {
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
  arangodb::transaction::Methods trx(
      arangodb::transaction::StandaloneContext::Create(*sysVocbase), EMPTY,
      EMPTY, EMPTY, arangodb::transaction::Options());

  arangodb::iresearch::FieldIterator it(trx, irs::string_ref::EMPTY,
                                        arangodb::IndexId(0));
  it.reset(slice, linkMeta);
  EXPECT_FALSE(it.valid());
}

TEST_F(IResearchDocumentTest,
       FieldIterator_traverse_complex_object_ordered_empty_analyzers) {
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
  linkMeta._analyzers.clear();  // clear all analyzers
  linkMeta._primitiveOffset = 0;
  linkMeta._includeAllFields = true;  // include all fields

  std::vector<std::string> EMPTY;
  arangodb::transaction::Methods trx(
      arangodb::transaction::StandaloneContext::Create(*sysVocbase), EMPTY,
      EMPTY, EMPTY, arangodb::transaction::Options());

  arangodb::iresearch::FieldIterator it(trx, irs::string_ref::EMPTY,
                                        arangodb::IndexId(0));
  it.reset(slice, linkMeta);
  EXPECT_FALSE(it.valid());
}

TEST_F(IResearchDocumentTest,
       FieldIterator_traverse_complex_object_ordered_check_value_types) {
  auto& analyzers =
      server.getFeature<arangodb::iresearch::IResearchAnalyzerFeature>();
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
                        "::iresearch-document-empty",
                    arangodb::QueryAnalyzerRevisions::QUERY_LATEST),
      "iresearch-document-empty"));   // add analyzer
  linkMeta._includeAllFields = true;  // include all fields
  linkMeta._primitiveOffset = linkMeta._analyzers.size();

  std::vector<std::string> EMPTY;
  arangodb::transaction::Methods trx(
      arangodb::transaction::StandaloneContext::Create(*sysVocbase), EMPTY,
      EMPTY, EMPTY, arangodb::transaction::Options());

  arangodb::iresearch::FieldIterator it(trx, irs::string_ref::EMPTY,
                                        arangodb::IndexId(0));
  it.reset(slice, linkMeta);

  // stringValue (with IdentityAnalyzer)
  {
    auto& field = *it;
    EXPECT_EQ(mangleStringIdentity("stringValue"), field.name());
    auto const expected_analyzer = irs::analysis::analyzers::get(
        "identity", irs::type<irs::text_format::vpack>::get(),
        arangodb::iresearch::ref<char>(VPackSlice::emptyObjectSlice()));
    auto& analyzers =
        server.getFeature<arangodb::iresearch::IResearchAnalyzerFeature>();
    auto const expected_features =
        analyzers
            .get("identity", arangodb::QueryAnalyzerRevisions::QUERY_LATEST)
            ->features();
    auto& analyzer = dynamic_cast<irs::analysis::analyzer&>(field.get_tokens());
    EXPECT_EQ(expected_analyzer->type(), analyzer.type());
    EXPECT_EQ(
        expected_features.fieldFeatures(arangodb::iresearch::LinkVersion::MIN),
        field.features());
    EXPECT_EQ(expected_features.indexFeatures(), field.index_features());
  }

  ++it;
  ASSERT_TRUE(it.valid());

  // stringValue (with EmptyAnalyzer)
  {
    auto& field = *it;
    EXPECT_EQ(mangleString("stringValue", "iresearch-document-empty"),
              field.name());
    auto const expected_analyzer = irs::analysis::analyzers::get(
        "iresearch-document-empty", irs::type<irs::text_format::vpack>::get(),
        arangodb::iresearch::ref<char>(VPackSlice::emptyObjectSlice()));
    auto& analyzer = dynamic_cast<EmptyAnalyzer&>(field.get_tokens());
    EXPECT_EQ(expected_analyzer->type(), analyzer.type());
    auto expectedFeatures =
        arangodb::iresearch::Features(irs::IndexFeatures::FREQ);
    EXPECT_EQ(
        expectedFeatures.fieldFeatures(arangodb::iresearch::LinkVersion::MIN),
        field.features());
    EXPECT_EQ(expectedFeatures.indexFeatures(), field.index_features());
  }

  ++it;
  ASSERT_TRUE(it.valid());

  // nullValue
  {
    auto& field = *it;
    EXPECT_EQ(mangleNull("nullValue"), field.name());
    auto& analyzer = dynamic_cast<irs::null_token_stream&>(field.get_tokens());
    auto expectedFeatures =
        arangodb::iresearch::Features({}, irs::IndexFeatures::NONE);
    EXPECT_EQ(
        expectedFeatures.fieldFeatures(arangodb::iresearch::LinkVersion::MIN),
        field.features());
    EXPECT_EQ(expectedFeatures.indexFeatures(), field.index_features());
    EXPECT_TRUE(analyzer.next());
  }

  ++it;
  ASSERT_TRUE(it.valid());

  // trueValue
  {
    auto& field = *it;
    EXPECT_EQ(mangleBool("trueValue"), field.name());
    auto& analyzer =
        dynamic_cast<irs::boolean_token_stream&>(field.get_tokens());
    auto expectedFeatures =
        arangodb::iresearch::Features({}, irs::IndexFeatures::NONE);
    EXPECT_EQ(
        expectedFeatures.fieldFeatures(arangodb::iresearch::LinkVersion::MIN),
        field.features());
    EXPECT_EQ(expectedFeatures.indexFeatures(), field.index_features());
    EXPECT_TRUE(analyzer.next());
  }

  ++it;
  ASSERT_TRUE(it.valid());

  // falseValue
  {
    auto& field = *it;
    EXPECT_EQ(mangleBool("falseValue"), field.name());
    auto& analyzer =
        dynamic_cast<irs::boolean_token_stream&>(field.get_tokens());
    auto expectedFeatures =
        arangodb::iresearch::Features({}, irs::IndexFeatures::NONE);
    EXPECT_EQ(
        expectedFeatures.fieldFeatures(arangodb::iresearch::LinkVersion::MIN),
        field.features());
    EXPECT_EQ(expectedFeatures.indexFeatures(), field.index_features());
    EXPECT_TRUE(analyzer.next());
  }

  ++it;
  ASSERT_TRUE(it.valid());

  // smallIntValue
  {
    auto& field = *it;
    EXPECT_EQ(mangleNumeric("smallIntValue"), field.name());
    auto& analyzer =
        dynamic_cast<irs::numeric_token_stream&>(field.get_tokens());
    irs::type_info::type_id const fieldFeatures[]{
        irs::type<irs::granularity_prefix>::id()};
    EXPECT_EQ((irs::features_t{fieldFeatures, 1}), field.features());
    EXPECT_EQ(irs::IndexFeatures::NONE, field.index_features());
    EXPECT_TRUE(analyzer.next());
  }

  ++it;
  ASSERT_TRUE(it.valid());

  // smallNegativeIntValue
  {
    auto& field = *it;
    EXPECT_EQ(mangleNumeric("smallNegativeIntValue"), field.name());
    auto& analyzer =
        dynamic_cast<irs::numeric_token_stream&>(field.get_tokens());
    irs::type_info::type_id const fieldFeatures[]{
        irs::type<irs::granularity_prefix>::id()};
    EXPECT_EQ((irs::features_t{fieldFeatures, 1}), field.features());
    EXPECT_EQ(irs::IndexFeatures::NONE, field.index_features());
    EXPECT_TRUE(analyzer.next());
  }

  ++it;
  ASSERT_TRUE(it.valid());

  // bigIntValue
  {
    auto& field = *it;
    EXPECT_EQ(mangleNumeric("bigIntValue"), field.name());
    auto& analyzer =
        dynamic_cast<irs::numeric_token_stream&>(field.get_tokens());
    irs::type_info::type_id const fieldFeatures[]{
        irs::type<irs::granularity_prefix>::id()};
    EXPECT_EQ((irs::features_t{fieldFeatures, 1}), field.features());
    EXPECT_EQ(irs::IndexFeatures::NONE, field.index_features());
    EXPECT_TRUE(analyzer.next());
  }

  ++it;
  ASSERT_TRUE(it.valid());

  // bigNegativeIntValue
  {
    auto& field = *it;
    EXPECT_EQ(mangleNumeric("bigNegativeIntValue"), field.name());
    auto& analyzer =
        dynamic_cast<irs::numeric_token_stream&>(field.get_tokens());
    irs::type_info::type_id const fieldFeatures[]{
        irs::type<irs::granularity_prefix>::id()};
    EXPECT_EQ((irs::features_t{fieldFeatures, 1}), field.features());
    EXPECT_EQ(irs::IndexFeatures::NONE, field.index_features());
    EXPECT_TRUE(analyzer.next());
  }

  ++it;
  ASSERT_TRUE(it.valid());

  // smallDoubleValue
  {
    auto& field = *it;
    EXPECT_EQ(mangleNumeric("smallDoubleValue"), field.name());
    auto& analyzer =
        dynamic_cast<irs::numeric_token_stream&>(field.get_tokens());
    irs::type_info::type_id const fieldFeatures[]{
        irs::type<irs::granularity_prefix>::id()};
    EXPECT_EQ((irs::features_t{fieldFeatures, 1}), field.features());
    EXPECT_EQ(irs::IndexFeatures::NONE, field.index_features());
    EXPECT_TRUE(analyzer.next());
  }

  ++it;
  ASSERT_TRUE(it.valid());

  // bigDoubleValue
  {
    auto& field = *it;
    EXPECT_EQ(mangleNumeric("bigDoubleValue"), field.name());
    auto& analyzer =
        dynamic_cast<irs::numeric_token_stream&>(field.get_tokens());
    irs::type_info::type_id const fieldFeatures[]{
        irs::type<irs::granularity_prefix>::id()};
    EXPECT_EQ((irs::features_t{fieldFeatures, 1}), field.features());
    EXPECT_EQ(irs::IndexFeatures::NONE, field.index_features());
    EXPECT_TRUE(analyzer.next());
  }

  ++it;
  ASSERT_TRUE(it.valid());

  // bigNegativeDoubleValue
  {
    auto& field = *it;
    EXPECT_EQ(mangleNumeric("bigNegativeDoubleValue"), field.name());
    auto& analyzer =
        dynamic_cast<irs::numeric_token_stream&>(field.get_tokens());
    irs::type_info::type_id const fieldFeatures[]{
        irs::type<irs::granularity_prefix>::id()};
    EXPECT_EQ((irs::features_t{fieldFeatures, 1}), field.features());
    EXPECT_EQ(irs::IndexFeatures::NONE, field.index_features());
    EXPECT_TRUE(analyzer.next());
  }

  ++it;
  EXPECT_FALSE(it.valid());
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
  arangodb::transaction::Methods trx(
      arangodb::transaction::StandaloneContext::Create(*sysVocbase), EMPTY,
      EMPTY, EMPTY, arangodb::transaction::Options());

  arangodb::iresearch::FieldIterator it(trx, irs::string_ref::EMPTY,
                                        arangodb::IndexId(0));
  it.reset(json0->slice(), linkMeta);
  ASSERT_TRUE(it.valid());

  {
    auto& value = *it;
    EXPECT_EQ(mangleStringIdentity("boost"), value.name());
    const auto expected_analyzer = irs::analysis::analyzers::get(
        "identity", irs::type<irs::text_format::vpack>::get(),
        arangodb::iresearch::ref<char>(VPackSlice::emptyObjectSlice()));
    auto& analyzers =
        server.getFeature<arangodb::iresearch::IResearchAnalyzerFeature>();
    auto const expected_features =
        analyzers
            .get("identity", arangodb::QueryAnalyzerRevisions::QUERY_LATEST)
            ->features();
    auto& analyzer = dynamic_cast<irs::analysis::analyzer&>(value.get_tokens());
    EXPECT_EQ(
        expected_features.fieldFeatures(arangodb::iresearch::LinkVersion::MIN),
        value.features());
    EXPECT_EQ(expected_features.indexFeatures(), value.index_features());
    EXPECT_EQ(expected_analyzer->type(), analyzer.type());
  }

  ++it;
  ASSERT_TRUE(it.valid());

  // depth (with IdentityAnalyzer)
  {
    auto& value = *it;
    EXPECT_EQ(mangleStringIdentity("depth"), value.name());
    const auto expected_analyzer = irs::analysis::analyzers::get(
        "identity", irs::type<irs::text_format::vpack>::get(),
        arangodb::iresearch::ref<char>(VPackSlice::emptyObjectSlice()));
    auto& analyzers =
        server.getFeature<arangodb::iresearch::IResearchAnalyzerFeature>();
    auto const expected_features =
        analyzers
            .get("identity", arangodb::QueryAnalyzerRevisions::QUERY_LATEST)
            ->features();
    auto& analyzer = dynamic_cast<irs::analysis::analyzer&>(value.get_tokens());
    EXPECT_EQ(
        expected_features.fieldFeatures(arangodb::iresearch::LinkVersion::MIN),
        value.features());
    EXPECT_EQ(expected_features.indexFeatures(), value.index_features());
    EXPECT_EQ(expected_analyzer->type(), analyzer.type());
  }

  ++it;
  ASSERT_FALSE(it.valid());

  it.reset(json1->slice(), linkMeta);
  ASSERT_TRUE(it.valid());

  {
    auto& value = *it;
    EXPECT_EQ(mangleStringIdentity("name"), value.name());
    const auto expected_analyzer = irs::analysis::analyzers::get(
        "identity", irs::type<irs::text_format::vpack>::get(),
        arangodb::iresearch::ref<char>(VPackSlice::emptyObjectSlice()));
    auto& analyzers =
        server.getFeature<arangodb::iresearch::IResearchAnalyzerFeature>();
    auto const expected_features =
        analyzers
            .get("identity", arangodb::QueryAnalyzerRevisions::QUERY_LATEST)
            ->features();
    auto& analyzer = dynamic_cast<irs::analysis::analyzer&>(value.get_tokens());
    EXPECT_EQ(
        expected_features.fieldFeatures(arangodb::iresearch::LinkVersion::MIN),
        value.features());
    EXPECT_EQ(expected_features.indexFeatures(), value.index_features());
    EXPECT_EQ(expected_analyzer->type(), analyzer.type());
  }

  ++it;
  ASSERT_FALSE(it.valid());
}

TEST_F(
    IResearchDocumentTest,
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
  arangodb::transaction::Methods trx(
      arangodb::transaction::StandaloneContext::Create(*sysVocbase), EMPTY,
      EMPTY, EMPTY, arangodb::transaction::Options());

  arangodb::iresearch::FieldIterator it(trx, irs::string_ref::EMPTY,
                                        arangodb::IndexId(0));
  it.reset(slice, linkMeta);

  // default analyzer
  auto const expected_analyzer = irs::analysis::analyzers::get(
      "identity", irs::type<irs::text_format::vpack>::get(),
      arangodb::iresearch::ref<char>(VPackSlice::emptyObjectSlice()));
  auto& analyzers =
      server.getFeature<arangodb::iresearch::IResearchAnalyzerFeature>();
  auto const expected_features =
      analyzers.get("identity", arangodb::QueryAnalyzerRevisions::QUERY_LATEST)
          ->features();

  for (; it.valid(); ++it) {
    auto& field = *it;
    std::string const actualName = std::string(field.name());
    EXPECT_EQ(1, expectedValues.erase(actualName));

    auto& analyzer = dynamic_cast<irs::analysis::analyzer&>(field.get_tokens());
    EXPECT_EQ(
        expected_features.fieldFeatures(arangodb::iresearch::LinkVersion::MIN),
        field.features());
    EXPECT_EQ(expected_features.indexFeatures(), field.index_features());
    EXPECT_EQ(expected_analyzer->type(), analyzer.type());
  }

  EXPECT_TRUE(expectedValues.empty());
}

TEST_F(IResearchDocumentTest,
       FieldIterator_traverse_complex_object_check_meta_inheritance) {
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
  ASSERT_TRUE(linkMeta.init(server.server(), linkMetaJson->slice(), error,
                            sysVocbase.get()->name()));

  std::vector<std::string> EMPTY;
  arangodb::transaction::Methods trx(
      arangodb::transaction::StandaloneContext::Create(*sysVocbase), EMPTY,
      EMPTY, EMPTY, arangodb::transaction::Options());

  arangodb::iresearch::FieldIterator it(trx, irs::string_ref::EMPTY,
                                        arangodb::IndexId(0));
  it.reset(slice, linkMeta);
  ASSERT_TRUE(it.valid());

  // nested.foo (with IdentityAnalyzer)
  {
    auto& value = *it;
    EXPECT_EQ(mangleStringIdentity("nested.foo"), value.name());
    const auto expected_analyzer = irs::analysis::analyzers::get(
        "identity", irs::type<irs::text_format::vpack>::get(),
        arangodb::iresearch::ref<char>(VPackSlice::emptyObjectSlice()));
    auto& analyzer = dynamic_cast<irs::analysis::analyzer&>(value.get_tokens());
    auto& analyzers =
        server.getFeature<arangodb::iresearch::IResearchAnalyzerFeature>();
    auto const expected_features =
        analyzers
            .get("identity", arangodb::QueryAnalyzerRevisions::QUERY_LATEST)
            ->features();
    EXPECT_EQ(
        expected_features.fieldFeatures(arangodb::iresearch::LinkVersion::MIN),
        value.features());
    EXPECT_EQ(expected_features.indexFeatures(), value.index_features());
    EXPECT_EQ(expected_analyzer->type(), analyzer.type());
  }

  ++it;
  ASSERT_TRUE(it.valid());

  // nested.foo (with EmptyAnalyzer)
  {
    auto& value = *it;
    EXPECT_EQ(mangleString("nested.foo", "iresearch-document-empty"),
              value.name());
    auto& analyzer = dynamic_cast<EmptyAnalyzer&>(value.get_tokens());
    EXPECT_FALSE(analyzer.next());
  }

  // keys[]
  for (size_t i = 0; i < 4; ++i) {
    ++it;
    ASSERT_TRUE(it.valid());

    auto& value = *it;
    EXPECT_EQ(mangleStringIdentity("keys"), value.name());
    const auto expected_analyzer = irs::analysis::analyzers::get(
        "identity", irs::type<irs::text_format::vpack>::get(),
        arangodb::iresearch::ref<char>(VPackSlice::emptyObjectSlice()));
    auto& analyzers =
        server.getFeature<arangodb::iresearch::IResearchAnalyzerFeature>();
    auto const expected_features =
        analyzers
            .get("identity", arangodb::QueryAnalyzerRevisions::QUERY_LATEST)
            ->features();
    auto& analyzer = dynamic_cast<irs::analysis::analyzer&>(value.get_tokens());
    EXPECT_EQ(
        expected_features.fieldFeatures(arangodb::iresearch::LinkVersion::MIN),
        value.features());
    EXPECT_EQ(expected_features.indexFeatures(), value.index_features());
    EXPECT_EQ(expected_analyzer->type(), analyzer.type());
  }

  ++it;
  ASSERT_TRUE(it.valid());

  // boost
  {
    auto& value = *it;
    EXPECT_EQ(mangleStringIdentity("boost"), value.name());
    const auto expected_analyzer = irs::analysis::analyzers::get(
        "identity", irs::type<irs::text_format::vpack>::get(),
        arangodb::iresearch::ref<char>(VPackSlice::emptyObjectSlice()));
    auto& analyzers =
        server.getFeature<arangodb::iresearch::IResearchAnalyzerFeature>();
    auto const expected_features =
        analyzers
            .get("identity", arangodb::QueryAnalyzerRevisions::QUERY_LATEST)
            ->features();
    auto& analyzer = dynamic_cast<irs::analysis::analyzer&>(value.get_tokens());
    EXPECT_EQ(
        expected_features.fieldFeatures(arangodb::iresearch::LinkVersion::MIN),
        value.features());
    EXPECT_EQ(expected_features.indexFeatures(), value.index_features());
    EXPECT_EQ(expected_analyzer->type(), analyzer.type());
  }

  ++it;
  ASSERT_TRUE(it.valid());

  // depth
  {
    auto& value = *it;
    EXPECT_EQ(mangleNumeric("depth"), value.name());
    auto& analyzer =
        dynamic_cast<irs::numeric_token_stream&>(value.get_tokens());
    EXPECT_TRUE(analyzer.next());
  }

  ++it;
  ASSERT_TRUE(it.valid());

  // fields.fieldA (with IdenityAnalyzer)
  {
    auto& value = *it;
    EXPECT_EQ(mangleStringIdentity("fields.fieldA.name"), value.name());
    const auto expected_analyzer = irs::analysis::analyzers::get(
        "identity", irs::type<irs::text_format::vpack>::get(),
        arangodb::iresearch::ref<char>(VPackSlice::emptyObjectSlice()));
    auto& analyzers =
        server.getFeature<arangodb::iresearch::IResearchAnalyzerFeature>();
    auto const expected_features =
        analyzers
            .get("identity", arangodb::QueryAnalyzerRevisions::QUERY_LATEST)
            ->features();
    auto& analyzer = dynamic_cast<irs::analysis::analyzer&>(value.get_tokens());
    EXPECT_EQ(
        expected_features.fieldFeatures(arangodb::iresearch::LinkVersion::MIN),
        value.features());
    EXPECT_EQ(expected_features.indexFeatures(), value.index_features());
    EXPECT_EQ(expected_analyzer->type(), analyzer.type());
  }

  ++it;
  ASSERT_TRUE(it.valid());

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

  // listValuation (with IdenityAnalyzer)
  {
    auto& value = *it;
    EXPECT_EQ(mangleStringIdentity("listValuation"), value.name());
    const auto expected_analyzer = irs::analysis::analyzers::get(
        "identity", irs::type<irs::text_format::vpack>::get(),
        arangodb::iresearch::ref<char>(VPackSlice::emptyObjectSlice()));
    auto& analyzers =
        server.getFeature<arangodb::iresearch::IResearchAnalyzerFeature>();
    auto const expected_features =
        analyzers
            .get("identity", arangodb::QueryAnalyzerRevisions::QUERY_LATEST)
            ->features();
    auto& analyzer = dynamic_cast<irs::analysis::analyzer&>(value.get_tokens());
    EXPECT_EQ(
        expected_features.fieldFeatures(arangodb::iresearch::LinkVersion::MIN),
        value.features());
    EXPECT_EQ(expected_features.indexFeatures(), value.index_features());
    EXPECT_EQ(expected_analyzer->type(), analyzer.type());
  }

  ++it;
  ASSERT_TRUE(it.valid());

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

  // locale
  {
    auto& value = *it;
    EXPECT_EQ(mangleNull("locale"), value.name());
    auto& analyzer = dynamic_cast<irs::null_token_stream&>(value.get_tokens());
    EXPECT_TRUE(analyzer.next());
  }

  ++it;
  ASSERT_TRUE(it.valid());

  // array[0].id
  {
    auto& value = *it;
    EXPECT_EQ(mangleNumeric("array[0].id"), value.name());
    auto& analyzer =
        dynamic_cast<irs::numeric_token_stream&>(value.get_tokens());
    EXPECT_TRUE(analyzer.next());
  }

  // array[0].subarr[0-2]
  for (size_t i = 0; i < 3; ++i) {
    ++it;
    ASSERT_TRUE(it.valid());

    // IdentityAnalyzer
    {
      auto& value = *it;
      EXPECT_EQ(mangleStringIdentity("array[0].subarr"), value.name());
      const auto expected_analyzer = irs::analysis::analyzers::get(
          "identity", irs::type<irs::text_format::vpack>::get(),
          arangodb::iresearch::ref<char>(VPackSlice::emptyObjectSlice()));
      auto& analyzers =
          server.getFeature<arangodb::iresearch::IResearchAnalyzerFeature>();
      auto const expected_features =
          analyzers
              .get("identity", arangodb::QueryAnalyzerRevisions::QUERY_LATEST)
              ->features();
      auto& analyzer =
          dynamic_cast<irs::analysis::analyzer&>(value.get_tokens());
      EXPECT_EQ(expected_features.fieldFeatures(
                    arangodb::iresearch::LinkVersion::MIN),
                value.features());
      EXPECT_EQ(expected_features.indexFeatures(), value.index_features());
      EXPECT_EQ(expected_analyzer->type(), analyzer.type());
    }

    ++it;
    ASSERT_TRUE(it.valid());

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

    // IdentityAnalyzer
    {
      auto& value = *it;
      EXPECT_EQ(mangleStringIdentity("array[1].subarr"), value.name());
      const auto expected_analyzer = irs::analysis::analyzers::get(
          "identity", irs::type<irs::text_format::vpack>::get(),
          arangodb::iresearch::ref<char>(VPackSlice::emptyObjectSlice()));
      auto& analyzers =
          server.getFeature<arangodb::iresearch::IResearchAnalyzerFeature>();
      auto const expected_features =
          analyzers
              .get("identity", arangodb::QueryAnalyzerRevisions::QUERY_LATEST)
              ->features();
      auto& analyzer =
          dynamic_cast<irs::analysis::analyzer&>(value.get_tokens());
      EXPECT_EQ(expected_features.fieldFeatures(
                    arangodb::iresearch::LinkVersion::MIN),
                value.features());
      EXPECT_EQ(expected_features.indexFeatures(), value.index_features());
      EXPECT_EQ(expected_analyzer->type(), analyzer.type());
    }

    ++it;
    ASSERT_TRUE(it.valid());

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

  // array[1].id (IdentityAnalyzer)
  {
    auto& value = *it;
    EXPECT_EQ(mangleStringIdentity("array[1].id"), value.name());
    const auto expected_analyzer = irs::analysis::analyzers::get(
        "identity", irs::type<irs::text_format::vpack>::get(),
        arangodb::iresearch::ref<char>(VPackSlice::emptyObjectSlice()));
    auto& analyzers =
        server.getFeature<arangodb::iresearch::IResearchAnalyzerFeature>();
    auto const expected_features =
        analyzers
            .get("identity", arangodb::QueryAnalyzerRevisions::QUERY_LATEST)
            ->features();
    auto& analyzer = dynamic_cast<irs::analysis::analyzer&>(value.get_tokens());
    EXPECT_EQ(
        expected_features.fieldFeatures(arangodb::iresearch::LinkVersion::MIN),
        value.features());
    EXPECT_EQ(expected_features.indexFeatures(), value.index_features());
    EXPECT_EQ(expected_analyzer->type(), analyzer.type());
  }

  ++it;
  ASSERT_TRUE(it.valid());

  // array[1].id (EmptyAnalyzer)
  {
    auto& value = *it;
    EXPECT_EQ(mangleString("array[1].id", "iresearch-document-empty"),
              value.name());
    auto& analyzer = dynamic_cast<EmptyAnalyzer&>(value.get_tokens());
    EXPECT_FALSE(analyzer.next());
  }

  ++it;
  ASSERT_TRUE(it.valid());

  // array[2].id (IdentityAnalyzer)
  {
    auto& value = *it;
    EXPECT_EQ(mangleNumeric("array[2].id"), value.name());
    auto& analyzer =
        dynamic_cast<irs::numeric_token_stream&>(value.get_tokens());
    EXPECT_TRUE(analyzer.next());
  }

  // array[2].subarr[0-2]
  for (size_t i = 0; i < 3; ++i) {
    ++it;
    ASSERT_TRUE(it.valid());

    // IdentityAnalyzer
    {
      auto& value = *it;
      EXPECT_EQ(mangleStringIdentity("array[2].subarr"), value.name());
      const auto expected_analyzer = irs::analysis::analyzers::get(
          "identity", irs::type<irs::text_format::vpack>::get(),
          arangodb::iresearch::ref<char>(VPackSlice::emptyObjectSlice()));
      auto& analyzers =
          server.getFeature<arangodb::iresearch::IResearchAnalyzerFeature>();
      auto const expected_features =
          analyzers
              .get("identity", arangodb::QueryAnalyzerRevisions::QUERY_LATEST)
              ->features();
      auto& analyzer =
          dynamic_cast<irs::analysis::analyzer&>(value.get_tokens());
      EXPECT_EQ(expected_features.fieldFeatures(
                    arangodb::iresearch::LinkVersion::MIN),
                value.features());
      EXPECT_EQ(expected_features.indexFeatures(), value.index_features());
      EXPECT_EQ(expected_analyzer->type(), analyzer.type());
    }

    ++it;
    ASSERT_TRUE(it.valid());

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
}

TEST_F(
    IResearchDocumentTest,
    FieldIterator_traverse_complex_object_check_meta_inheritance_object_analyzer) {
  auto& sysDatabase = server.getFeature<arangodb::SystemDatabaseFeature>();
  auto sysVocbase = sysDatabase.use();

  auto const json = arangodb::velocypack::Parser::fromJson(
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

  auto const linkMetaJson = arangodb::velocypack::Parser::fromJson(
      "{ \
    \"includeAllFields\" : true, \
    \"trackListPositions\" : true, \
    \"fields\" : { \
       \"boost\" : { \"analyzers\": [ \"identity\" ] }, \
       \"keys\" : { \"trackListPositions\" : false, \"analyzers\": [ \"identity\", \"iresearch-vpack-analyzer\" ] }, \
       \"depth\" : { \"trackListPositions\" : true }, \
       \"fields\" : { \"includeAllFields\" : false, \"fields\" : { \"fieldA\" : { \"includeAllFields\" : true } } }, \
       \"listValuation\" : { \"includeAllFields\" : false }, \
       \"array\" : { \
         \"fields\" : { \"subarr\" : { \"trackListPositions\" : false }, \"subobj\": { \"includeAllFields\" : false }, \"id\" : { } } \
       } \
     }, \
    \"analyzers\": [ \"identity\", \"iresearch-document-empty\", \"iresearch-vpack-analyzer\" ] \
  }");

  arangodb::iresearch::IResearchLinkMeta linkMeta;
  std::string error;
  ASSERT_TRUE(linkMeta.init(server.server(), linkMetaJson->slice(), error,
                            sysVocbase.get()->name()));

  std::vector<std::string> EMPTY;
  arangodb::transaction::Methods trx(
      arangodb::transaction::StandaloneContext::Create(*sysVocbase), EMPTY,
      EMPTY, EMPTY, arangodb::transaction::Options());

  std::function<AssertFieldFunc> const assertFields[] = {
      [](auto& server, auto const& it) {
        assertField<VPackAnalyzer>(server, *it, "nested");
      },
      [](auto& server, auto const& it) {
        assertField<IdentityAnalyzer>(server, *it, "nested.foo");
      },
      [](auto& server, auto const& it) {
        assertField<EmptyAnalyzer>(server, *it, "nested.foo");
      },
      [](auto& server, auto const& it) {
        assertField<VPackAnalyzer>(server, *it, "keys");
      },
      [](auto& server, auto const& it) {
        assertField<IdentityAnalyzer>(server, *it, "keys");
      },
      [](auto& server, auto const& it) {
        assertField<IdentityAnalyzer>(server, *it, "keys");
      },
      [](auto& server, auto const& it) {
        assertField<IdentityAnalyzer>(server, *it, "keys");
      },
      [](auto& server, auto const& it) {
        assertField<IdentityAnalyzer>(server, *it, "keys");
      },
      [](auto& server, auto const& it) {
        assertField<VPackAnalyzer>(server, *it, "analyzers");
      },
      [](auto& server, auto const& it) {
        assertField<IdentityAnalyzer>(server, *it, "boost");
      },
      [](auto& server, auto const& it) {
        assertField<irs::numeric_token_stream>(server, *it,
                                               mangleNumeric("depth"));
      },
      [](auto& server, auto const& it) {
        assertField<VPackAnalyzer>(server, *it, "fields");
      },
      [](auto& server, auto const& it) {
        assertField<VPackAnalyzer>(server, *it, "fields.fieldA");
      },
      [](auto& server, auto const& it) {
        assertField<IdentityAnalyzer>(server, *it, "fields.fieldA.name");
      },
      [](auto& server, auto const& it) {
        assertField<EmptyAnalyzer>(server, *it, "fields.fieldA.name");
      },
      [](auto& server, auto const& it) {
        assertField<IdentityAnalyzer>(server, *it, "listValuation");
      },
      [](auto& server, auto const& it) {
        assertField<EmptyAnalyzer>(server, *it, "listValuation");
      },
      [](auto& server, auto const& it) {
        assertField<irs::null_token_stream>(server, *it, mangleNull("locale"));
      },
      [](auto& server, auto const& it) {
        assertField<VPackAnalyzer>(server, *it, "array");
      },
      [](auto& server, auto const& it) {
        assertField<VPackAnalyzer>(server, *it, "array[0]");
      },
      [](auto& server, auto const& it) {
        assertField<irs::numeric_token_stream>(server, *it,
                                               mangleNumeric("array[0].id"));
      },
      [](auto& server, auto const& it) {
        assertField<VPackAnalyzer>(server, *it, "array[0].subarr");
      },
      [](auto& server, auto const& it) {
        assertField<IdentityAnalyzer>(server, *it, "array[0].subarr");
      },
      [](auto& server, auto const& it) {
        assertField<EmptyAnalyzer>(server, *it, "array[0].subarr");
      },
      [](auto& server, auto const& it) {
        assertField<IdentityAnalyzer>(server, *it, "array[0].subarr");
      },
      [](auto& server, auto const& it) {
        assertField<EmptyAnalyzer>(server, *it, "array[0].subarr");
      },
      [](auto& server, auto const& it) {
        assertField<IdentityAnalyzer>(server, *it, "array[0].subarr");
      },
      [](auto& server, auto const& it) {
        assertField<EmptyAnalyzer>(server, *it, "array[0].subarr");
      },
      [](auto& server, auto const& it) {
        assertField<VPackAnalyzer>(server, *it, "array[0].subobj");
      },
      [](auto& server, auto const& it) {
        assertField<VPackAnalyzer>(server, *it, "array[1]");
      },
      [](auto& server, auto const& it) {
        assertField<VPackAnalyzer>(server, *it, "array[1].subarr");
      },
      [](auto& server, auto const& it) {
        assertField<IdentityAnalyzer>(server, *it, "array[1].subarr");
      },
      [](auto& server, auto const& it) {
        assertField<EmptyAnalyzer>(server, *it, "array[1].subarr");
      },
      [](auto& server, auto const& it) {
        assertField<IdentityAnalyzer>(server, *it, "array[1].subarr");
      },
      [](auto& server, auto const& it) {
        assertField<EmptyAnalyzer>(server, *it, "array[1].subarr");
      },
      [](auto& server, auto const& it) {
        assertField<IdentityAnalyzer>(server, *it, "array[1].subarr");
      },
      [](auto& server, auto const& it) {
        assertField<EmptyAnalyzer>(server, *it, "array[1].subarr");
      },
      [](auto& server, auto const& it) {
        assertField<VPackAnalyzer>(server, *it, "array[1].subobj");
      },
      [](auto& server, auto const& it) {
        assertField<IdentityAnalyzer>(server, *it, "array[1].id");
      },
      [](auto& server, auto const& it) {
        assertField<EmptyAnalyzer>(server, *it, "array[1].id");
      },
      [](auto& server, auto const& it) {
        assertField<VPackAnalyzer>(server, *it, "array[2]");
      },
      [](auto& server, auto const& it) {
        assertField<irs::numeric_token_stream>(server, *it,
                                               mangleNumeric("array[2].id"));
      },
      [](auto& server, auto const& it) {
        assertField<VPackAnalyzer>(server, *it, "array[2].subarr");
      },
      [](auto& server, auto const& it) {
        assertField<IdentityAnalyzer>(server, *it, "array[2].subarr");
      },
      [](auto& server, auto const& it) {
        assertField<EmptyAnalyzer>(server, *it, "array[2].subarr");
      },
      [](auto& server, auto const& it) {
        assertField<IdentityAnalyzer>(server, *it, "array[2].subarr");
      },
      [](auto& server, auto const& it) {
        assertField<EmptyAnalyzer>(server, *it, "array[2].subarr");
      },
      [](auto& server, auto const& it) {
        assertField<IdentityAnalyzer>(server, *it, "array[2].subarr");
      },
      [](auto& server, auto const& it) {
        assertField<EmptyAnalyzer>(server, *it, "array[2].subarr");
      },
      [](auto& server, auto const& it) {
        assertField<VPackAnalyzer>(server, *it, "array[2].subobj");
      },
  };

  arangodb::iresearch::FieldIterator it(trx, irs::string_ref::EMPTY,
                                        arangodb::IndexId(0));
  it.reset(json->slice(), linkMeta);

  for (auto& assertField : assertFields) {
    ASSERT_TRUE(it.valid());
    assertField(server, it);
    ++it;
  }

  ASSERT_FALSE(it.valid());
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
                arangodb::velocypack::Parser::fromJson("{ \"args\":\"en\" }")
                    ->slice(),
                arangodb::iresearch::Features(irs::IndexFeatures::FREQ))
            .ok());

    // valid duplicate (same properties)
    ASSERT_TRUE(
        analyzers
            .emplace(
                result, arangodb::StaticStrings::SystemDatabase + "::empty",
                "iresearch-document-empty",
                arangodb::velocypack::Parser::fromJson("{ \"args\":\"en\" }")
                    ->slice(),
                arangodb::iresearch::Features(irs::IndexFeatures::FREQ))
            .ok());

    // ensure there will be no exception on 'emplace'
    InvalidAnalyzer::returnFalseFromToString = true;
    ASSERT_FALSE(
        analyzers
            .emplace(
                result, arangodb::StaticStrings::SystemDatabase + "::invalid",
                "iresearch-document-invalid",
                arangodb::velocypack::Parser::fromJson("{ \"args\":\"en\" }")
                    ->slice(),
                arangodb::iresearch::Features(irs::IndexFeatures::FREQ))
            .ok());
    InvalidAnalyzer::returnFalseFromToString = false;

    // ensure that there will be no exception on 'emplace'
    InvalidAnalyzer::returnNullFromMake = true;
    ASSERT_FALSE(
        analyzers
            .emplace(
                result, arangodb::StaticStrings::SystemDatabase + "::invalid",
                "iresearch-document-invalid",
                arangodb::velocypack::Parser::fromJson("{ \"args\":\"en\" }")
                    ->slice(),
                arangodb::iresearch::Features(irs::IndexFeatures::FREQ))
            .ok());
    InvalidAnalyzer::returnNullFromMake = false;

    ASSERT_TRUE(
        analyzers
            .emplace(
                result, arangodb::StaticStrings::SystemDatabase + "::invalid",
                "iresearch-document-invalid",
                arangodb::velocypack::Parser::fromJson("{ \"args\":\"en\" }")
                    ->slice(),
                arangodb::iresearch::Features(irs::IndexFeatures::FREQ))
            .ok());
  }

  // last analyzer invalid
  {
    arangodb::iresearch::IResearchLinkMeta linkMeta;
    linkMeta._analyzers.emplace_back(arangodb::iresearch::FieldMeta::Analyzer(
        analyzers.get(arangodb::StaticStrings::SystemDatabase + "::empty",
                      arangodb::QueryAnalyzerRevisions::QUERY_LATEST),
        "empty"));  // add analyzer

    InvalidAnalyzer::returnNullFromMake = false;
    linkMeta._analyzers.emplace_back(arangodb::iresearch::FieldMeta::Analyzer(
        analyzers.get(arangodb::StaticStrings::SystemDatabase + "::invalid",
                      arangodb::QueryAnalyzerRevisions::QUERY_LATEST),
        "invalid"));                    // add analyzer
    linkMeta._includeAllFields = true;  // include all fields
    linkMeta._primitiveOffset = linkMeta._analyzers.size();

    // acquire analyzer, another one should be created
    auto analyzer =
        linkMeta._analyzers.back()
            ._pool->get();  // cached instance should have been acquired

    InvalidAnalyzer::returnNullFromMake = true;

    std::vector<std::string> EMPTY;
    arangodb::transaction::Methods trx(
        arangodb::transaction::StandaloneContext::Create(*sysVocbase), EMPTY,
        EMPTY, EMPTY, arangodb::transaction::Options());

    arangodb::iresearch::FieldIterator it(trx, irs::string_ref::EMPTY,
                                          arangodb::IndexId(0));
    it.reset(slice, linkMeta);
    ASSERT_TRUE(it.valid());

    // stringValue (with IdentityAnalyzer)
    {
      auto& field = *it;
      EXPECT_EQ(mangleStringIdentity("stringValue"), field.name());
      auto const expected_analyzer = irs::analysis::analyzers::get(
          "identity", irs::type<irs::text_format::vpack>::get(),
          arangodb::iresearch::ref<char>(VPackSlice::emptyObjectSlice()));
      auto& analyzers =
          server.getFeature<arangodb::iresearch::IResearchAnalyzerFeature>();
      auto const expected_features =
          analyzers
              .get("identity", arangodb::QueryAnalyzerRevisions::QUERY_LATEST)
              ->features();
      auto& analyzer =
          dynamic_cast<irs::analysis::analyzer&>(field.get_tokens());
      EXPECT_EQ(expected_analyzer->type(), analyzer.type());
      EXPECT_EQ(expected_features.fieldFeatures(
                    arangodb::iresearch::LinkVersion::MIN),
                field.features());
      EXPECT_EQ(expected_features.indexFeatures(), field.index_features());
    }

    ++it;
    ASSERT_TRUE(it.valid());

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

    analyzer->reset(irs::string_ref::NIL);  // ensure that acquired 'analyzer'
                                            // will not be optimized out
  }

  // first analyzer is invalid
  {
    arangodb::iresearch::IResearchLinkMeta linkMeta;
    linkMeta._analyzers.clear();

    InvalidAnalyzer::returnNullFromMake = false;
    linkMeta._analyzers.emplace_back(arangodb::iresearch::FieldMeta::Analyzer(
        analyzers.get(arangodb::StaticStrings::SystemDatabase + "::invalid",
                      arangodb::QueryAnalyzerRevisions::QUERY_LATEST),
        "invalid"));  // add analyzer
    linkMeta._analyzers.emplace_back(arangodb::iresearch::FieldMeta::Analyzer(
        analyzers.get(arangodb::StaticStrings::SystemDatabase + "::empty",
                      arangodb::QueryAnalyzerRevisions::QUERY_LATEST),
        "empty"));                      // add analyzer
    linkMeta._includeAllFields = true;  // include all fields
    linkMeta._primitiveOffset = linkMeta._analyzers.size();

    // acquire analyzer, another one should be created
    auto analyzer =
        linkMeta._analyzers.front()
            ._pool->get();  // cached instance should have been acquired
    InvalidAnalyzer::returnNullFromMake = true;

    std::vector<std::string> EMPTY;
    arangodb::transaction::Methods trx(
        arangodb::transaction::StandaloneContext::Create(*sysVocbase), EMPTY,
        EMPTY, EMPTY, arangodb::transaction::Options());

    arangodb::iresearch::FieldIterator it(trx, irs::string_ref::EMPTY,
                                          arangodb::IndexId(0));
    it.reset(slice, linkMeta);
    ASSERT_TRUE(it.valid());

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

    analyzer->reset(irs::string_ref::NIL);  // ensure that acquired 'analyzer'
                                            // will not be optimized out
  }
}

TEST_F(IResearchDocumentTest,
       FieldIterator_traverse_complex_object_primitive_type_sub_analyzers) {
  auto const linkMetaJson = arangodb::velocypack::Parser::fromJson(
      "{ \
    \"includeAllFields\" : true, \
    \"trackListPositions\" : true, \
    \"fields\" : { \
       \"boost\" : { \"analyzers\": [ \"iresearch-document-number\" ] }, \
       \"keys\" : { \"trackListPositions\" : false, \"analyzers\": [ \"iresearch-document-number\", \"iresearch-document-bool\" ] }, \
       \"depth\" : { \"trackListPositions\" : true, \"analyzers\": [ \"iresearch-document-string\"] }, \
       \"fields\" : { \"analyzers\": [ \"iresearch-document-number\", \"iresearch-document-bool\" ],\
                      \"includeAllFields\" : false, \"fields\" :\
                        { \"fieldA\" : { \"includeAllFields\" : true } } }, \
       \"listValuation\" : { \"includeAllFields\" : false }, \
       \"array\" : { \
         \"analyzers\": [ \"iresearch-document-number\", \"iresearch-document-bool\" ],\
         \"trackListPositions\":true,\
         \"fields\" : { \"subarr\" : { \"trackListPositions\" : false }, \"subobj\": { \"includeAllFields\" : false, \"fields\":{\"id\" : {}} }, \"id\" : { } } \
       } \
     }, \
    \"analyzers\": [ \"identity\"] \
  }");

  auto& sysDatabase = server.getFeature<arangodb::SystemDatabaseFeature>();
  auto sysVocbase = sysDatabase.use();
  arangodb::iresearch::IResearchLinkMeta linkMeta;
  std::string error;
  ASSERT_TRUE(linkMeta.init(server.server(), linkMetaJson->slice(), error,
                            sysVocbase.get()->name()));

  std::vector<std::string> EMPTY;
  arangodb::transaction::Methods trx(
      arangodb::transaction::StandaloneContext::Create(*sysVocbase), EMPTY,
      EMPTY, EMPTY, arangodb::transaction::Options());

  auto json = arangodb::velocypack::Parser::fromJson(
      "{ \
    \"nested\": { \"foo\": \"str\" }, \
    \"keys\": [ \"1\",\"2\"], \
    \"analyzers\": [], \
    \"boost\": \"10\", \
    \"depth\": \"20\", \
    \"fields\": { \"fieldA\" : { \"name\" : \"a\" }, \"fieldB\" : { \"name\" : \"b\" } }, \
    \"listValuation\": \"ignored\", \
    \"array\" : [ \
      { \"id\" : \"1\", \"subarr\" : [ \"1\", \"2\", \"3\" ], \"subobj\" : { \"id\" : \"1\" } }, \
      { \"subarr\" : [ \"4\", \"5\", \"6\" ], \"subobj\" : { \"name\" : \"foo\" }, \"id\" : \"2\" }, \
      { \"id\" : \"3\", \"subarr\" : [ \"7\", \"8\", \"9\" ], \"subobj\" : { \"id\" : \"2\" } } \
    ] \
  }");

  std::function<AssertFieldFunc> const assertFields[] = {
      [](auto& server, auto const& it) {
        assertField<IdentityAnalyzer>(server, *it, "nested.foo");
      },
      [](auto& server, auto const& it) {
        assertField<irs::numeric_token_stream>(server, *it,
                                               mangleNumeric("keys"));
      },
      [](auto& server, auto const& it) {
        assertField<irs::boolean_token_stream>(server, *it, mangleBool("keys"));
      },
      [](auto& server, auto const& it) {
        assertField<irs::numeric_token_stream>(server, *it,
                                               mangleNumeric("keys"));
      },
      [](auto& server, auto const& it) {
        assertField<irs::boolean_token_stream>(server, *it, mangleBool("keys"));
      },
      [](auto& server, auto const& it) {
        assertField<irs::numeric_token_stream>(server, *it,
                                               mangleNumeric("boost"));
      },
      [](auto& server, auto const& it) {
        assertField<TypedAnalyzer>(server, *it, "depth",
                                   "iresearch-document-string");
      },
      [](auto& server, auto const& it) {
        assertField<irs::numeric_token_stream>(
            server, *it, mangleNumeric("fields.fieldA.name"));
      },
      [](auto& server, auto const& it) {
        assertField<irs::boolean_token_stream>(
            server, *it, mangleBool("fields.fieldA.name"));
      },
      [](auto& server, auto const& it) {
        assertField<IdentityAnalyzer>(server, *it, "listValuation");
      },
      [](auto& server, auto const& it) {
        assertField<irs::numeric_token_stream>(server, *it,
                                               mangleNumeric("array[0].id"));
      },
      [](auto& server, auto const& it) {
        assertField<irs::boolean_token_stream>(server, *it,
                                               mangleBool("array[0].id"));
      },
      [](auto& server, auto const& it) {
        assertField<irs::numeric_token_stream>(
            server, *it, mangleNumeric("array[0].subarr"));
      },
      [](auto& server, auto const& it) {
        assertField<irs::boolean_token_stream>(server, *it,
                                               mangleBool("array[0].subarr"));
      },
      [](auto& server, auto const& it) {
        assertField<irs::numeric_token_stream>(
            server, *it, mangleNumeric("array[0].subarr"));
      },
      [](auto& server, auto const& it) {
        assertField<irs::boolean_token_stream>(server, *it,
                                               mangleBool("array[0].subarr"));
      },
      [](auto& server, auto const& it) {
        assertField<irs::numeric_token_stream>(
            server, *it, mangleNumeric("array[0].subarr"));
      },
      [](auto& server, auto const& it) {
        assertField<irs::boolean_token_stream>(server, *it,
                                               mangleBool("array[0].subarr"));
      },
      [](auto& server, auto const& it) {
        assertField<irs::numeric_token_stream>(
            server, *it, mangleNumeric("array[0].subobj.id"));
      },
      [](auto& server, auto const& it) {
        assertField<irs::boolean_token_stream>(
            server, *it, mangleBool("array[0].subobj.id"));
      },
      [](auto& server, auto const& it) {
        assertField<irs::numeric_token_stream>(
            server, *it, mangleNumeric("array[1].subarr"));
      },
      [](auto& server, auto const& it) {
        assertField<irs::boolean_token_stream>(server, *it,
                                               mangleBool("array[1].subarr"));
      },
      [](auto& server, auto const& it) {
        assertField<irs::numeric_token_stream>(
            server, *it, mangleNumeric("array[1].subarr"));
      },
      [](auto& server, auto const& it) {
        assertField<irs::boolean_token_stream>(server, *it,
                                               mangleBool("array[1].subarr"));
      },
      [](auto& server, auto const& it) {
        assertField<irs::numeric_token_stream>(
            server, *it, mangleNumeric("array[1].subarr"));
      },
      [](auto& server, auto const& it) {
        assertField<irs::boolean_token_stream>(server, *it,
                                               mangleBool("array[1].subarr"));
      },
      [](auto& server, auto const& it) {
        assertField<irs::numeric_token_stream>(server, *it,
                                               mangleNumeric("array[1].id"));
      },
      [](auto& server, auto const& it) {
        assertField<irs::boolean_token_stream>(server, *it,
                                               mangleBool("array[1].id"));
      },
      [](auto& server, auto const& it) {
        assertField<irs::numeric_token_stream>(server, *it,
                                               mangleNumeric("array[2].id"));
      },
      [](auto& server, auto const& it) {
        assertField<irs::boolean_token_stream>(server, *it,
                                               mangleBool("array[2].id"));
      },
      [](auto& server, auto const& it) {
        assertField<irs::numeric_token_stream>(
            server, *it, mangleNumeric("array[2].subarr"));
      },
      [](auto& server, auto const& it) {
        assertField<irs::boolean_token_stream>(server, *it,
                                               mangleBool("array[2].subarr"));
      },
      [](auto& server, auto const& it) {
        assertField<irs::numeric_token_stream>(
            server, *it, mangleNumeric("array[2].subarr"));
      },
      [](auto& server, auto const& it) {
        assertField<irs::boolean_token_stream>(server, *it,
                                               mangleBool("array[2].subarr"));
      },
      [](auto& server, auto const& it) {
        assertField<irs::numeric_token_stream>(
            server, *it, mangleNumeric("array[2].subarr"));
      },
      [](auto& server, auto const& it) {
        assertField<irs::boolean_token_stream>(server, *it,
                                               mangleBool("array[2].subarr"));
      },
      [](auto& server, auto const& it) {
        assertField<irs::numeric_token_stream>(
            server, *it, mangleNumeric("array[2].subobj.id"));
      },
      [](auto& server, auto const& it) {
        assertField<irs::boolean_token_stream>(
            server, *it, mangleBool("array[2].subobj.id"));
      },
  };

  arangodb::iresearch::FieldIterator it(trx, irs::string_ref::EMPTY,
                                        arangodb::IndexId(0));
  it.reset(json->slice(), linkMeta);

  for (auto& assertField : assertFields) {
    ASSERT_TRUE(it.valid());
    assertField(server, it);
    ++it;
  }

  ASSERT_FALSE(it.valid());
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
      writer = irs::index_writer::make(dir, irs::formats::get("1_0"),
                                       irs::OM_CREATE);
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

    auto pk = arangodb::iresearch::DocumentPrimaryKey::encode(
        arangodb::LocalDocumentId(rid));
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

  auto& engine = server.getFeature<arangodb::EngineSelectorFeature>().engine();

  size_t found = 0;
  for (auto const docSlice : arangodb::velocypack::ArrayIterator(dataSlice)) {
    auto const ridSlice = docSlice.get("rid");
    EXPECT_TRUE(ridSlice.isNumber());

    rid = ridSlice.getNumber<uint64_t>();

    auto& segment = (*reader)[0];

    auto* pkField =
        segment.field(arangodb::iresearch::DocumentPrimaryKey::PK());
    EXPECT_TRUE(pkField);
    EXPECT_EQ(size, pkField->docs_count());

    arangodb::iresearch::PrimaryKeyFilterContainer filters;
    EXPECT_TRUE(filters.empty());
    auto& filter = filters.emplace(engine, arangodb::LocalDocumentId(rid));
    ASSERT_EQ(filter.type(engine),
              arangodb::iresearch::PrimaryKeyFilter::type(engine));
    EXPECT_FALSE(filters.empty());

    // first execution
    {
      auto prepared = filter.prepare(*reader);
      ASSERT_TRUE(prepared);
      EXPECT_EQ(prepared, filter.prepare(*reader));  // same object
      EXPECT_TRUE(&filter ==
                  dynamic_cast<arangodb::iresearch::PrimaryKeyFilter const*>(
                      prepared.get()));  // same object

      for (auto& segment : *reader) {
        auto docs = prepared->execute(segment);
        ASSERT_TRUE(docs);
        // EXPECT_EQ(nullptr, prepared->execute(segment)); // unusable filter
        // TRI_ASSERT(...) check
        EXPECT_EQ(irs::filter::prepared::empty(),
                  filter.prepare(*reader));  // unusable filter (after execute)

        EXPECT_TRUE(docs->next());
        auto const id = docs->value();
        ++found;
        EXPECT_FALSE(docs->next());
        EXPECT_TRUE(irs::doc_limits::eof(docs->value()));
        EXPECT_FALSE(docs->next());
        EXPECT_TRUE(irs::doc_limits::eof(docs->value()));

        auto column =
            segment.column(arangodb::iresearch::DocumentPrimaryKey::PK());
        ASSERT_TRUE(column);

        auto values = column->iterator(false);
        ASSERT_NE(nullptr, values);
        auto* value = irs::get<irs::payload>(*values);
        ASSERT_NE(nullptr, value);

        EXPECT_EQ(id, values->seek(id));

        arangodb::LocalDocumentId pk;
        EXPECT_TRUE(
            arangodb::iresearch::DocumentPrimaryKey::read(pk, value->value));
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
      writer = irs::index_writer::make(dir, irs::formats::get("1_0"),
                                       irs::OM_CREATE);
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
    auto pk = arangodb::iresearch::DocumentPrimaryKey::encode(
        arangodb::LocalDocumentId(rid));

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
    auto pk = arangodb::iresearch::DocumentPrimaryKey::encode(
        arangodb::LocalDocumentId(12345));
    auto ctx = store.writer->documents();
    auto doc = ctx.insert();
    arangodb::iresearch::Field::setPkValue(field, pk);
    EXPECT_TRUE(doc.insert<irs::Action::INDEX | irs::Action::STORE>(field));
    EXPECT_TRUE(doc);
  }

  store.writer->commit();
  store.reader = store.reader->reopen();
  EXPECT_EQ(1, store.reader->size());
  EXPECT_EQ(expectedDocs + 1,
            store.reader->docs_count());  // +1 for keep-alive doc
  EXPECT_EQ(expectedLiveDocs + 1,
            store.reader->live_docs_count());  // +1 for keep-alive doc

  auto& engine = server.getFeature<arangodb::EngineSelectorFeature>().engine();

  // check regular filter case (unique rid)
  {
    size_t actualDocs = 0;

    for (auto const docSlice : arangodb::velocypack::ArrayIterator(dataSlice)) {
      auto const ridSlice = docSlice.get("rid");
      EXPECT_TRUE(ridSlice.isNumber<uint64_t>());

      auto rid = ridSlice.getNumber<uint64_t>();
      arangodb::iresearch::PrimaryKeyFilterContainer filters;
      EXPECT_TRUE(filters.empty());
      auto& filter = filters.emplace(engine, arangodb::LocalDocumentId(rid));
      ASSERT_EQ(filter.type(engine),
                arangodb::iresearch::PrimaryKeyFilter::type(engine));
      EXPECT_FALSE(filters.empty());

      auto prepared = filter.prepare(*store.reader);
      ASSERT_TRUE(prepared);
      EXPECT_EQ(prepared, filter.prepare(*store.reader));  // same object
      EXPECT_TRUE((&filter ==
                   dynamic_cast<arangodb::iresearch::PrimaryKeyFilter const*>(
                       prepared.get())));  // same object

      for (auto& segment : *store.reader) {
        auto docs = prepared->execute(segment);
        ASSERT_TRUE(docs);
        // EXPECT_EQ(nullptr, prepared->execute(segment)); // unusable filter
        // TRI_ASSERT(...) check
        EXPECT_EQ(
            irs::filter::prepared::empty(),
            filter.prepare(*store.reader));  // unusable filter (after execute)

        EXPECT_TRUE(docs->next());
        auto const id = docs->value();
        ++actualDocs;
        EXPECT_FALSE(docs->next());
        EXPECT_TRUE(irs::doc_limits::eof(docs->value()));
        EXPECT_FALSE(docs->next());
        EXPECT_TRUE(irs::doc_limits::eof(docs->value()));

        auto column =
            segment.column(arangodb::iresearch::DocumentPrimaryKey::PK());
        ASSERT_TRUE(column);

        auto values = column->iterator(false);
        ASSERT_NE(nullptr, values);
        auto* value = irs::get<irs::payload>(*values);
        ASSERT_NE(nullptr, value);

        EXPECT_EQ(id, values->seek(id));

        arangodb::LocalDocumentId pk;
        EXPECT_TRUE(
            arangodb::iresearch::DocumentPrimaryKey::read(pk, value->value));
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
    auto pk = arangodb::iresearch::DocumentPrimaryKey::encode(
        arangodb::LocalDocumentId(rid));

    // remove + insert document
    {
      auto ctx = store.writer->documents();
      ctx.remove(std::make_shared<arangodb::iresearch::PrimaryKeyFilter>(
          engine, arangodb::LocalDocumentId(rid)));
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
  EXPECT_EQ(expectedDocs + 2,
            store.reader->docs_count());  // +2 for keep-alive doc
  EXPECT_EQ(expectedLiveDocs + 2,
            store.reader->live_docs_count());  // +2 for keep-alive doc

  // check 1st recovery case
  {
    size_t actualDocs = 0;

    auto beforeRecovery = StorageEngineMock::recoveryStateResult;
    StorageEngineMock::recoveryStateResult =
        arangodb::RecoveryState::IN_PROGRESS;
    auto restoreRecovery = irs::make_finally([&beforeRecovery]() -> void {
      StorageEngineMock::recoveryStateResult = beforeRecovery;
    });

    for (auto const docSlice : arangodb::velocypack::ArrayIterator(dataSlice)) {
      auto const ridSlice = docSlice.get("rid");
      EXPECT_TRUE(ridSlice.isNumber<uint64_t>());

      auto rid = ridSlice.getNumber<uint64_t>();
      arangodb::iresearch::PrimaryKeyFilterContainer filters;
      EXPECT_TRUE(filters.empty());
      auto& filter = filters.emplace(engine, arangodb::LocalDocumentId(rid));
      ASSERT_EQ(filter.type(engine),
                arangodb::iresearch::PrimaryKeyFilter::type(engine));
      EXPECT_FALSE(filters.empty());

      auto prepared = filter.prepare(*store.reader);
      ASSERT_TRUE(prepared);
      EXPECT_EQ(prepared, filter.prepare(*store.reader));  // same object
      EXPECT_TRUE((&filter ==
                   dynamic_cast<arangodb::iresearch::PrimaryKeyFilter const*>(
                       prepared.get())));  // same object

      for (auto& segment : *store.reader) {
        auto docs = prepared->execute(segment);
        ASSERT_TRUE(docs);
        EXPECT_NE(nullptr, prepared->execute(segment));  // usable filter
        EXPECT_NE(
            nullptr,
            filter.prepare(*store.reader));  // usable filter (after execute)

        if (docs->next()) {  // old segments will not have any matching docs
          auto const id = docs->value();
          ++actualDocs;
          EXPECT_FALSE(docs->next());
          EXPECT_TRUE(irs::doc_limits::eof(docs->value()));
          EXPECT_FALSE(docs->next());
          EXPECT_TRUE(irs::doc_limits::eof(docs->value()));

          auto column =
              segment.column(arangodb::iresearch::DocumentPrimaryKey::PK());
          ASSERT_TRUE(column);

          auto values = column->iterator(false);
          ASSERT_NE(nullptr, values);
          auto* value = irs::get<irs::payload>(*values);
          ASSERT_NE(nullptr, value);

          EXPECT_EQ(id, values->seek(id));

          arangodb::LocalDocumentId pk;
          EXPECT_TRUE(
              arangodb::iresearch::DocumentPrimaryKey::read(pk, value->value));
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
    auto pk = arangodb::iresearch::DocumentPrimaryKey::encode(
        arangodb::LocalDocumentId(rid));

    // remove + insert document
    {
      auto ctx = store.writer->documents();
      ctx.remove(std::make_shared<arangodb::iresearch::PrimaryKeyFilter>(
          engine, arangodb::LocalDocumentId(rid)));
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
  EXPECT_EQ(expectedDocs + 3,
            store.reader->docs_count());  // +3 for keep-alive doc
  EXPECT_EQ(expectedLiveDocs + 3,
            store.reader->live_docs_count());  // +3 for keep-alive doc

  // check 2nd recovery case
  {
    size_t actualDocs = 0;

    auto beforeRecovery = StorageEngineMock::recoveryStateResult;
    StorageEngineMock::recoveryStateResult =
        arangodb::RecoveryState::IN_PROGRESS;
    auto restoreRecovery = irs::make_finally([&beforeRecovery]() -> void {
      StorageEngineMock::recoveryStateResult = beforeRecovery;
    });

    for (auto const docSlice : arangodb::velocypack::ArrayIterator(dataSlice)) {
      auto const ridSlice = docSlice.get("rid");
      EXPECT_TRUE(ridSlice.isNumber<uint64_t>());

      auto rid = ridSlice.getNumber<uint64_t>();
      arangodb::iresearch::PrimaryKeyFilterContainer filters;
      EXPECT_TRUE(filters.empty());
      auto& filter = filters.emplace(engine, arangodb::LocalDocumentId(rid));
      ASSERT_EQ(filter.type(engine),
                arangodb::iresearch::PrimaryKeyFilter::type(engine));
      EXPECT_FALSE(filters.empty());

      auto prepared = filter.prepare(*store.reader);
      ASSERT_TRUE(prepared);
      EXPECT_EQ(prepared, filter.prepare(*store.reader));  // same object
      EXPECT_TRUE((&filter ==
                   dynamic_cast<arangodb::iresearch::PrimaryKeyFilter const*>(
                       prepared.get())));  // same object

      for (auto& segment : *store.reader) {
        auto docs = prepared->execute(segment);
        ASSERT_TRUE(docs);
        EXPECT_NE(nullptr, prepared->execute(segment));  // usable filter
        EXPECT_NE(
            nullptr,
            filter.prepare(*store.reader));  // usable filter (after execute)

        if (docs->next()) {  // old segments will not have any matching docs
          auto const id = docs->value();
          ++actualDocs;
          EXPECT_FALSE(docs->next());
          EXPECT_TRUE(irs::doc_limits::eof(docs->value()));
          EXPECT_FALSE(docs->next());
          EXPECT_TRUE(irs::doc_limits::eof(docs->value()));

          auto column =
              segment.column(arangodb::iresearch::DocumentPrimaryKey::PK());
          ASSERT_TRUE(column);

          auto values = column->iterator(false);
          ASSERT_NE(nullptr, values);
          auto* value = irs::get<irs::payload>(*values);
          ASSERT_NE(nullptr, value);

          EXPECT_EQ(id, values->seek(id));

          arangodb::LocalDocumentId pk;
          EXPECT_TRUE(
              arangodb::iresearch::DocumentPrimaryKey::read(pk, value->value));
          EXPECT_EQ(rid, pk.id());
        }
      }
    }

    EXPECT_EQ(expectedLiveDocs, actualDocs);
  }
}

TEST_F(IResearchDocumentTest, FieldIterator_index_id_attr) {
  auto& sysDatabase = server.getFeature<arangodb::SystemDatabaseFeature>();
  auto sysVocbase = sysDatabase.use();

  std::vector<std::string> EMPTY;
  arangodb::transaction::Methods trx(
      arangodb::transaction::StandaloneContext::Create(*sysVocbase), EMPTY,
      EMPTY, EMPTY, arangodb::transaction::Options());

  arangodb::iresearch::IResearchLinkMeta linkMeta;
  linkMeta._includeAllFields = true;  // include all fields

  arangodb::RevisionId rev;
  VPackBuilder document;
  {
    auto analyzersCollection = sysVocbase->useCollection(
        arangodb::StaticStrings::AnalyzersCollection, false);
    ASSERT_TRUE(analyzersCollection);
    VPackBuilder builder;
    builder.openObject();
    builder.add(arangodb::StaticStrings::KeyString, VPackValue("test"));
    builder.close();
    ASSERT_TRUE(analyzersCollection->getPhysical()
                    ->newObjectForInsert(&trx, builder.slice(), false, document,
                                         false, rev)
                    .ok());
    sysVocbase->releaseCollection(analyzersCollection.get());
  }

  auto mangledId = mangleStringIdentity(arangodb::StaticStrings::IdString);
  {
    arangodb::iresearch::FieldIterator it_no_name(trx, irs::string_ref::EMPTY,
                                                  arangodb::IndexId(0));
    EXPECT_FALSE(it_no_name.valid());
    it_no_name.reset(document.slice(), linkMeta);
    ASSERT_TRUE(it_no_name.valid());
    bool id_indexed{false};
    while (it_no_name.valid()) {
      auto& field = *it_no_name;
      std::string const actualName = std::string(field.name());
      id_indexed = actualName == mangledId;
      if (id_indexed) {
        break;
      }
      ++it_no_name;
    }
    ASSERT_TRUE(
        id_indexed);  // for single server collection name is not necessary
  }
  {
    arangodb::iresearch::FieldIterator it_name(trx, "test",
                                               arangodb::IndexId(1));
    EXPECT_FALSE(it_name.valid());
    it_name.reset(document.slice(), linkMeta);
    ASSERT_TRUE(it_name.valid());
    bool id_indexed{false};
    while (it_name.valid()) {
      auto& field = *it_name;
      std::string const actualName = std::string(field.name());
      id_indexed = actualName == mangledId;
      if (id_indexed) {
        break;
      }
      ++it_name;
    }
    ASSERT_TRUE(id_indexed);
  }
}

TEST_F(IResearchDocumentTest, FieldIterator_dbServer_index_id_attr) {
  auto oldRole = arangodb::ServerState::instance()->getRole();
  arangodb::ServerState::instance()->setRole(
      arangodb::ServerState::RoleEnum::ROLE_DBSERVER);
  auto roleRestorer = irs::make_finally(
      [oldRole]() { arangodb::ServerState::instance()->setRole(oldRole); });
  auto& sysDatabase = server.getFeature<arangodb::SystemDatabaseFeature>();
  auto sysVocbase = sysDatabase.use();

  std::vector<std::string> EMPTY;
  arangodb::transaction::Methods trx(
      arangodb::transaction::StandaloneContext::Create(*sysVocbase), EMPTY,
      EMPTY, EMPTY, arangodb::transaction::Options());

  arangodb::iresearch::IResearchLinkMeta linkMeta;
  linkMeta._includeAllFields = true;  // include all fields

  arangodb::RevisionId rev;
  VPackBuilder document;
  {
    auto analyzersCollection = sysVocbase->useCollection(
        arangodb::StaticStrings::AnalyzersCollection, false);
    ASSERT_TRUE(analyzersCollection);
    VPackBuilder builder;
    builder.openObject();
    builder.add(arangodb::StaticStrings::KeyString, VPackValue("test"));
    builder.close();
    ASSERT_TRUE(analyzersCollection->getPhysical()
                    ->newObjectForInsert(&trx, builder.slice(), false, document,
                                         false, rev)
                    .ok());
    sysVocbase->releaseCollection(analyzersCollection.get());
  }

  auto mangledId = mangleStringIdentity(arangodb::StaticStrings::IdString);
  {
    arangodb::iresearch::FieldIterator it_no_name(trx, irs::string_ref::EMPTY,
                                                  arangodb::IndexId(0));
    EXPECT_FALSE(it_no_name.valid());
    it_no_name.reset(document.slice(), linkMeta);
    ASSERT_TRUE(it_no_name.valid());
    bool id_indexed{false};
    while (it_no_name.valid()) {
      auto& field = *it_no_name;
      std::string const actualName = std::string(field.name());
      id_indexed = actualName == mangledId;
      if (id_indexed) {
        break;
      }
      ++it_no_name;
    }
    ASSERT_FALSE(id_indexed);
  }
  {
    arangodb::iresearch::FieldIterator it_name(trx, "test",
                                               arangodb::IndexId(1));
    EXPECT_FALSE(it_name.valid());
    it_name.reset(document.slice(), linkMeta);
    ASSERT_TRUE(it_name.valid());
    bool id_indexed{false};
    while (it_name.valid()) {
      auto& field = *it_name;
      std::string const actualName = std::string(field.name());
      id_indexed = actualName == mangledId;
      if (id_indexed) {
        break;
      }
      ++it_name;
    }
    ASSERT_TRUE(id_indexed);
  }
}

TEST_F(IResearchDocumentTest, FieldIterator_concurrent_use_typed_analyzer) {
  auto& sysDatabase = server.getFeature<arangodb::SystemDatabaseFeature>();
  auto sysVocbase = sysDatabase.use();
  auto& analyzers =
      server
          .template getFeature<arangodb::iresearch::IResearchAnalyzerFeature>();
  arangodb::iresearch::IResearchLinkMeta linkMeta;
  linkMeta._analyzers.clear();
  linkMeta._analyzers.emplace_back(arangodb::iresearch::FieldMeta::Analyzer(
      analyzers.get(arangodb::StaticStrings::SystemDatabase +
                        "::iresearch-document-number-array",
                    arangodb::QueryAnalyzerRevisions::QUERY_LATEST),
      "iresearch-document-number-array"));  // add analyzer
  ASSERT_TRUE(linkMeta._analyzers.front()._pool);
  linkMeta._includeAllFields = true;  // include all fields
  linkMeta._primitiveOffset = linkMeta._analyzers.size();
  std::string error;
  std::vector<std::string> EMPTY;
  arangodb::transaction::Methods trx(
      arangodb::transaction::StandaloneContext::Create(*sysVocbase), EMPTY,
      EMPTY, EMPTY, arangodb::transaction::Options());
  arangodb::iresearch::FieldIterator it1(trx, irs::string_ref::EMPTY,
                                         arangodb::IndexId(0));

  auto json = arangodb::velocypack::Parser::fromJson("{\"value\":\"3\"}");
  auto json2 = arangodb::velocypack::Parser::fromJson("{\"value\":\"4\"}");

  it1.reset(json->slice(), linkMeta);
  ASSERT_TRUE(it1.valid());

  arangodb::iresearch::FieldIterator it2(trx, irs::string_ref::EMPTY,
                                         arangodb::IndexId(0));
  it2.reset(json2->slice(), linkMeta);
  ASSERT_TRUE(it2.valid());

  // exhaust first array member (1) of it1
  {
    irs::numeric_token_stream expected_tokens;
    expected_tokens.reset(1.);
    auto& actual_tokens = (*it1).get_tokens();
    auto actual_value = irs::get<irs::term_attribute>(actual_tokens);
    auto expected_value = irs::get<irs::term_attribute>(expected_tokens);
    while (actual_tokens.next()) {
      ASSERT_TRUE(expected_tokens.next());
      ASSERT_EQ(actual_value->value, expected_value->value);
    }
    ASSERT_FALSE(expected_tokens.next());
  }
  // exhaust first array member (1) of it2
  {
    irs::numeric_token_stream expected_tokens;
    expected_tokens.reset(1.);
    auto& actual_tokens = (*it2).get_tokens();
    auto actual_value = irs::get<irs::term_attribute>(actual_tokens);
    auto expected_value = irs::get<irs::term_attribute>(expected_tokens);
    while (actual_tokens.next()) {
      ASSERT_TRUE(expected_tokens.next());
      ASSERT_EQ(actual_value->value, expected_value->value);
    }
    ASSERT_FALSE(expected_tokens.next());
  }
  ++it1;  // now it1 should have it`s typed iterator pointing to 2
  ++it2;  // now it2 should have it`s typed iterator pointing to 2  (not to 3!)
  ASSERT_TRUE(it1.valid());
  {
    irs::numeric_token_stream expected_tokens;
    expected_tokens.reset(2.);
    auto& actual_tokens = (*it1).get_tokens();
    auto actual_value = irs::get<irs::term_attribute>(actual_tokens);
    auto expected_value = irs::get<irs::term_attribute>(expected_tokens);
    while (actual_tokens.next()) {
      ASSERT_TRUE(expected_tokens.next());
      ASSERT_EQ(actual_value->value, expected_value->value);
    }
    ASSERT_FALSE(expected_tokens.next());
  }
  ASSERT_TRUE(it2.valid());
  {
    irs::numeric_token_stream expected_tokens;
    expected_tokens.reset(2.);
    auto& actual_tokens = (*it2).get_tokens();
    auto actual_value = irs::get<irs::term_attribute>(actual_tokens);
    auto expected_value = irs::get<irs::term_attribute>(expected_tokens);
    while (actual_tokens.next()) {
      ASSERT_TRUE(expected_tokens.next());
      ASSERT_EQ(actual_value->value, expected_value->value);
    }
    ASSERT_FALSE(expected_tokens.next());
  }
}

TEST_F(
    IResearchDocumentTest,
    InvertedFieldIterator_traverse_simple_object_primitive_type_sub_analyzers) {
  auto const indexMetaJson = arangodb::velocypack::Parser::fromJson(
      R"({"fields" : [{"name": "boost"},
                  {"name": "keys"},
                  {"name": "keys2", "analyzer": "iresearch-document-number"}]})");

  auto& sysDatabase = server.getFeature<arangodb::SystemDatabaseFeature>();
  auto sysVocbase = sysDatabase.use();
  arangodb::iresearch::IResearchInvertedIndexMeta indexMeta;
  std::string error;
  ASSERT_TRUE(indexMeta.init(server.server(), indexMetaJson->slice(), false,
                             error, sysVocbase.get()->name()));

  std::vector<std::string> EMPTY;
  arangodb::transaction::Methods trx(
      arangodb::transaction::StandaloneContext::Create(*sysVocbase), EMPTY,
      EMPTY, EMPTY, arangodb::transaction::Options());

  auto json = arangodb::velocypack::Parser::fromJson(
      R"({
    "nested": {"foo": "str"},
    "keys": "1",
    "keys2": "2",
    "boost": 10,
    "depth": [20, 30, 40],
    "fields": {"fieldA" : {"name" : 1}, "fieldB" : {"name" : "b"}},
    "listValuation": "ignored",
    "array": [
      {"id": 1, "subobj": {"id": "10" }},
      {"subobj": {"name": "foo" }, "id": "2"},
      {"id": "3", "subobj": {"id": "22" }}],
    "last_not_present": "ignored"})");

  std::function<AssertInvertedIndexFieldFunc> const assertFields[] = {
      [](auto& server, auto const& it) {
        assertField<irs::numeric_token_stream, true>(server, *it,
                                                     mangleNumeric("boost"));
      },
      [](auto& server, auto const& it) {
        assertField<IdentityAnalyzer, true>(
            server, *it, mangleInvertedIndexStringIdentity("keys"));
      },
      [](auto& server, auto const& it) {
        assertField<irs::numeric_token_stream, true>(server, *it,
                                                     mangleNumeric("keys2"));
      },
  };

  arangodb::iresearch::InvertedIndexFieldIterator it(
      trx, irs::string_ref::EMPTY, arangodb::IndexId(0));
  it.reset(json->slice(), indexMeta);

  for (auto& assertField : assertFields) {
    ASSERT_TRUE(it.valid());
    assertField(server, it);
    ++it;
  }

  ASSERT_FALSE(it.valid());
}

TEST_F(
    IResearchDocumentTest,
    InvertedFieldIterator_traverse_complex_object_primitive_type_sub_analyzers) {
  auto const indexMetaJson = arangodb::velocypack::Parser::fromJson(
      R"({"fields" : [{"name": "boost"},
                  {"name": "never_present"},
                  {"name": "keys[*]"},
                  {"name": "keys2[*]", "analyzer": "iresearch-document-number"},
                  {"name": "depth[*]"},
                  {"name": "fields.fieldA.name", "analyzer": "iresearch-document-number"},
                  {"name": "array[*].id"},
                  {"name": "array[*].subobj.id"}]})");

  auto& sysDatabase = server.getFeature<arangodb::SystemDatabaseFeature>();
  auto sysVocbase = sysDatabase.use();
  arangodb::iresearch::IResearchInvertedIndexMeta indexMeta;
  std::string error;
  ASSERT_TRUE(indexMeta.init(server.server(), indexMetaJson->slice(), false,
                             error, sysVocbase.get()->name()));

  std::vector<std::string> EMPTY;
  arangodb::transaction::Methods trx(
      arangodb::transaction::StandaloneContext::Create(*sysVocbase), EMPTY,
      EMPTY, EMPTY, arangodb::transaction::Options());

  auto json = arangodb::velocypack::Parser::fromJson(
      R"({
    "nested": {"foo": "str"},
    "keys": ["1", "2"],
    "keys2": ["1", "2"],
    "boost": 10,
    "depth": [20, 30, 40],
    "fields": {"fieldA" : {"name" : "1"}, "fieldB" : {"name" : "b"}},
    "listValuation": "ignored",
    "array": [
      {"id": 1, "subobj": {"id": "10" }},
      {"subobj": {"name": "foo" }, "id": "2"},
      {"id": "3", "subobj": {"id": "22" }},
      {"No_id": "3", "subobj": {"id": 22 }}],
    "last_not_present": "ignored"})");

  std::function<AssertInvertedIndexFieldFunc> const assertFields[] = {
      [](auto& server, auto const& it) {
        assertField<irs::numeric_token_stream, true>(server, *it,
                                                     mangleNumeric("boost"));
      },
      [](auto& server, auto const& it) {
        assertField<IdentityAnalyzer, true>(
            server, *it, mangleInvertedIndexStringIdentity("keys"));
      },
      [](auto& server, auto const& it) {
        assertField<IdentityAnalyzer, true>(
            server, *it, mangleInvertedIndexStringIdentity("keys"));
      },
      [](auto& server, auto const& it) {
        assertField<irs::numeric_token_stream, true>(server, *it,
                                                     mangleNumeric("keys2"));
      },
      [](auto& server, auto const& it) {
        assertField<irs::numeric_token_stream, true>(server, *it,
                                                     mangleNumeric("keys2"));
      },
      [](auto& server, auto const& it) {
        assertField<irs::numeric_token_stream, true>(server, *it,
                                                     mangleNumeric("depth"));
      },
      [](auto& server, auto const& it) {
        assertField<irs::numeric_token_stream, true>(server, *it,
                                                     mangleNumeric("depth"));
      },
      [](auto& server, auto const& it) {
        assertField<irs::numeric_token_stream, true>(server, *it,
                                                     mangleNumeric("depth"));
      },
      [](auto& server, auto const& it) {
        assertField<irs::numeric_token_stream, true>(
            server, *it, mangleNumeric("fields.fieldA.name"));
      },
      [](auto& server, auto const& it) {
        assertField<irs::numeric_token_stream, true>(
            server, *it, mangleNumeric("array[*].id"));
      },
      [](auto& server, auto const& it) {
        assertField<IdentityAnalyzer, true>(
            server, *it, mangleInvertedIndexStringIdentity("array[*].id"));
      },
      [](auto& server, auto const& it) {
        assertField<IdentityAnalyzer, true>(
            server, *it, mangleInvertedIndexStringIdentity("array[*].id"));
      },
      [](auto& server, auto const& it) {
        assertField<irs::null_token_stream>(server, *it,
                                            mangleNull("array[*].id"));
      },
      [](auto& server, auto const& it) {
        assertField<IdentityAnalyzer, true>(
            server, *it,
            mangleInvertedIndexStringIdentity("array[*].subobj.id"));
      },
      [](auto& server, auto const& it) {
        assertField<irs::null_token_stream>(server, *it,
                                            mangleNull("array[*].subobj.id"));
      },
      [](auto& server, auto const& it) {
        assertField<IdentityAnalyzer, true>(
            server, *it,
            mangleInvertedIndexStringIdentity("array[*].subobj.id"));
      },
      [](auto& server, auto const& it) {
        assertField<irs::numeric_token_stream, true>(
            server, *it, mangleNumeric("array[*].subobj.id"));
      },
  };

  arangodb::iresearch::InvertedIndexFieldIterator it(
      trx, irs::string_ref::EMPTY, arangodb::IndexId(0));
  it.reset(json->slice(), indexMeta);

  size_t fieldIdx{};
  for (auto& assertField : assertFields) {
    SCOPED_TRACE(testing::Message("Field Idx: ") << (fieldIdx++));
    ASSERT_TRUE(it.valid());
    assertField(server, it);
    ++it;
  }

  ASSERT_FALSE(it.valid());
}

TEST_F(IResearchDocumentTest, InvertedFieldIterator_traverse_complex_with_geo) {
  auto const indexMetaJson = arangodb::velocypack::Parser::fromJson(
      R"({"fields" : [{"name": "boost"},
                  {"name": "never_present", "analyzer": "my_geo"},
                  {"name": "keys[*]"},
                  {"name": "geo_field", "analyzer": "my_geo"},
                  {"name": "keys2[*]", "analyzer": "my_geo"},
                  {"name": "depth[*]"},
                  {"name": "fields.fieldA.name", "analyzer": "iresearch-document-number"},
                  {"name": "array[*].id"},
                  {"name": "array[*].subobj.id", "analyzer": "my_geo"}]})");

  auto& sysDatabase = server.getFeature<arangodb::SystemDatabaseFeature>();
  auto sysVocbase = sysDatabase.use();
  arangodb::iresearch::IResearchInvertedIndexMeta indexMeta;
  std::string error;
  ASSERT_TRUE(indexMeta.init(server.server(), indexMetaJson->slice(), false,
                             error, sysVocbase.get()->name()));

  std::vector<std::string> EMPTY;
  arangodb::transaction::Methods trx(
      arangodb::transaction::StandaloneContext::Create(*sysVocbase), EMPTY,
      EMPTY, EMPTY, arangodb::transaction::Options());

  auto json = arangodb::velocypack::Parser::fromJson(
      R"({
    "nested": {"foo": "str"},
    "keys": ["1", "2"],
    "geo_field": { "type": "Point", "coordinates": [ 37.615895, 55.7039]},
    "keys2": [{ "type": "Point", "coordinates": [37.615895, 55.7039]},
              { "type": "Point", "coordinates": [37.615895, 55.7039]}],
    "boost": 10,
    "depth": [20, 30, 40],
    "fields": {"fieldA" : {"name" : "1"}, "fieldB" : {"name" : "b"}},
    "listValuation": "ignored",
    "array": [
      {"id": 1, "subobj": {"id": { "type": "Point", "coordinates": [ 37.615895, 55.7039]} }},
      {"subobj": {"name": "foo" }, "id": "2"},
      {"id": "3", "subobj": {"id": { "type": "Point", "coordinates": [ 37.615895, 55.7039]} }},
      {"No_id": "3", "subobj": {"id": { "type": "Point", "coordinates": [ 37.615895, 55.7039]} }}],
    "last_not_present": "ignored"})");

  std::function<AssertInvertedIndexFieldFunc> const assertFields[] = {
      [](auto& server, auto const& it) {
        assertField<irs::numeric_token_stream, true>(server, *it,
                                                     mangleNumeric("boost"));
      },
      [](auto& server, auto const& it) {
        assertField<IdentityAnalyzer, true>(
            server, *it, mangleInvertedIndexStringIdentity("keys"));
      },
      [](auto& server, auto const& it) {
        assertField<IdentityAnalyzer, true>(
            server, *it, mangleInvertedIndexStringIdentity("keys"));
      },
      [](auto& server, auto const& it) {
        assertField<arangodb::iresearch::GeoJSONAnalyzer, false>(
            server, *it, "geo_field", "my_geo");
      },
      [](auto& server, auto const& it) {
        assertField<arangodb::iresearch::GeoJSONAnalyzer, false>(
            server, *it, "keys2", "my_geo");
      },
      [](auto& server, auto const& it) {
        assertField<arangodb::iresearch::GeoJSONAnalyzer, false>(
            server, *it, "keys2", "my_geo");
      },
      [](auto& server, auto const& it) {
        assertField<irs::numeric_token_stream, true>(server, *it,
                                                     mangleNumeric("depth"));
      },
      [](auto& server, auto const& it) {
        assertField<irs::numeric_token_stream, true>(server, *it,
                                                     mangleNumeric("depth"));
      },
      [](auto& server, auto const& it) {
        assertField<irs::numeric_token_stream, true>(server, *it,
                                                     mangleNumeric("depth"));
      },
      [](auto& server, auto const& it) {
        assertField<irs::numeric_token_stream, true>(
            server, *it, mangleNumeric("fields.fieldA.name"));
      },
      [](auto& server, auto const& it) {
        assertField<irs::numeric_token_stream, true>(
            server, *it, mangleNumeric("array[*].id"));
      },
      [](auto& server, auto const& it) {
        assertField<IdentityAnalyzer, true>(
            server, *it, mangleInvertedIndexStringIdentity("array[*].id"));
      },
      [](auto& server, auto const& it) {
        assertField<IdentityAnalyzer, true>(
            server, *it, mangleInvertedIndexStringIdentity("array[*].id"));
      },
      [](auto& server, auto const& it) {
        assertField<irs::null_token_stream>(server, *it,
                                            mangleNull("array[*].id"));
      },
      [](auto& server, auto const& it) {
        assertField<arangodb::iresearch::GeoJSONAnalyzer>(
            server, *it, "array[*].subobj.id", "my_geo");
      },
      [](auto& server, auto const& it) {
        assertField<irs::null_token_stream>(server, *it,
                                            mangleNull("array[*].subobj.id"));
      },
      [](auto& server, auto const& it) {
        assertField<arangodb::iresearch::GeoJSONAnalyzer>(
            server, *it, "array[*].subobj.id", "my_geo");
      },
      [](auto& server, auto const& it) {
        assertField<arangodb::iresearch::GeoJSONAnalyzer>(
            server, *it, "array[*].subobj.id", "my_geo");
      },
  };

  arangodb::iresearch::InvertedIndexFieldIterator it(
      trx, irs::string_ref::EMPTY, arangodb::IndexId(0));
  it.reset(json->slice(), indexMeta);

  size_t fieldIdx{};
  for (auto& assertField : assertFields) {
    SCOPED_TRACE(testing::Message("Field Idx: ") << (fieldIdx++));
    ASSERT_TRUE(it.valid());
    assertField(server, it);
    ++it;
  }

  ASSERT_FALSE(it.valid());
}

TEST_F(IResearchDocumentTest, InvertedFieldIterator_not_array_expansion) {
  auto const indexMetaJson = arangodb::velocypack::Parser::fromJson(
      R"({"fields" : [{"name": "boost"},
                      {"name": "keys[*]"}]})");

  auto& sysDatabase = server.getFeature<arangodb::SystemDatabaseFeature>();
  auto sysVocbase = sysDatabase.use();
  arangodb::iresearch::IResearchInvertedIndexMeta indexMeta;
  std::string error;
  ASSERT_TRUE(indexMeta.init(server.server(), indexMetaJson->slice(), false,
                             error, sysVocbase.get()->name()));

  std::vector<std::string> EMPTY;
  arangodb::transaction::Methods trx(
      arangodb::transaction::StandaloneContext::Create(*sysVocbase), EMPTY,
      EMPTY, EMPTY, arangodb::transaction::Options());

  auto json = arangodb::velocypack::Parser::fromJson(
      R"({"keys": "not_an_array", "boost": 10})");

  arangodb::iresearch::InvertedIndexFieldIterator it(
      trx, irs::string_ref::EMPTY, arangodb::IndexId(0));
  it.reset(json->slice(), indexMeta);
  ASSERT_TRUE(it.valid());
  assertField<irs::numeric_token_stream, true>(server, *it,
                                               mangleNumeric("boost"));
  ++it;  // "keys" is just ignored
  ASSERT_FALSE(it.valid());
}

TEST_F(IResearchDocumentTest, InvertedFieldIterator_array_no_expansion) {
  auto const indexMetaJson = arangodb::velocypack::Parser::fromJson(
      R"({"fields" : [{"name": "boost"},
                      {"name": "keys"}]})");

  auto& sysDatabase = server.getFeature<arangodb::SystemDatabaseFeature>();
  auto sysVocbase = sysDatabase.use();
  arangodb::iresearch::IResearchInvertedIndexMeta indexMeta;
  std::string error;
  ASSERT_TRUE(indexMeta.init(server.server(), indexMetaJson->slice(), false,
                             error, sysVocbase.get()->name()));

  std::vector<std::string> EMPTY;
  arangodb::transaction::Methods trx(
      arangodb::transaction::StandaloneContext::Create(*sysVocbase), EMPTY,
      EMPTY, EMPTY, arangodb::transaction::Options());

  auto json = arangodb::velocypack::Parser::fromJson(
      R"({"keys": [1,2,3], "boost": 10})");

  arangodb::iresearch::InvertedIndexFieldIterator it(
      trx, irs::string_ref::EMPTY, arangodb::IndexId(0));
  it.reset(json->slice(), indexMeta);
  ASSERT_TRUE(it.valid());
  assertField<irs::numeric_token_stream, true>(server, *it,
                                               mangleNumeric("boost"));
  ASSERT_THROW(++it, arangodb::basics::Exception);
}

TEST_F(IResearchDocumentTest, InvertedFieldIterator_object) {
  auto const indexMetaJson = arangodb::velocypack::Parser::fromJson(
      R"({"fields" : [{"name": "boost"},
                      {"name": "keys"}]})");

  auto& sysDatabase = server.getFeature<arangodb::SystemDatabaseFeature>();
  auto sysVocbase = sysDatabase.use();
  arangodb::iresearch::IResearchInvertedIndexMeta indexMeta;
  std::string error;
  ASSERT_TRUE(indexMeta.init(server.server(), indexMetaJson->slice(), false,
                             error, sysVocbase.get()->name()));

  std::vector<std::string> EMPTY;
  arangodb::transaction::Methods trx(
      arangodb::transaction::StandaloneContext::Create(*sysVocbase), EMPTY,
      EMPTY, EMPTY, arangodb::transaction::Options());

  auto json = arangodb::velocypack::Parser::fromJson(
      R"({"keys": { "a":1, "b":2, "c":3}, "boost": 10})");

  arangodb::iresearch::InvertedIndexFieldIterator it(
      trx, irs::string_ref::EMPTY, arangodb::IndexId(0));
  it.reset(json->slice(), indexMeta);
  ASSERT_TRUE(it.valid());
  assertField<irs::numeric_token_stream, true>(server, *it,
                                               mangleNumeric("boost"));
  ASSERT_THROW(++it, arangodb::basics::Exception);
}

TEST_F(IResearchDocumentTest, InvertedFieldIterator_empty) {
  auto const indexMetaJson = arangodb::velocypack::Parser::fromJson(
      R"({"fields" : [{"name": "boost"},
                      {"name": "keys"}]})");

  auto& sysDatabase = server.getFeature<arangodb::SystemDatabaseFeature>();
  auto sysVocbase = sysDatabase.use();
  arangodb::iresearch::IResearchInvertedIndexMeta indexMeta;
  std::string error;
  ASSERT_TRUE(indexMeta.init(server.server(), indexMetaJson->slice(), false,
                             error, sysVocbase.get()->name()));

  std::vector<std::string> EMPTY;
  arangodb::transaction::Methods trx(
      arangodb::transaction::StandaloneContext::Create(*sysVocbase), EMPTY,
      EMPTY, EMPTY, arangodb::transaction::Options());

  auto json = arangodb::velocypack::Parser::fromJson(
      R"({"not_keys": { "a":1, "b":2, "c":3}, "some_boost": 10})");

  arangodb::iresearch::InvertedIndexFieldIterator it(
      trx, irs::string_ref::EMPTY, arangodb::IndexId(0));
  it.reset(json->slice(), indexMeta);
  ASSERT_FALSE(it.valid());
}
