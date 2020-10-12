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

#include "common.h"
#include "gtest/gtest.h"

#include "3rdParty/iresearch/tests/tests_config.hpp"
#include "analysis/analyzers.hpp"
#include "analysis/token_attributes.hpp"
#include "utils/utf8_path.hpp"

#include "velocypack/Iterator.h"
#include "velocypack/Parser.h"

#include "Mocks/LogLevels.h"
#include "Mocks/Servers.h"
#include "Mocks/StorageEngineMock.h"

#include "Aql/AqlFunctionFeature.h"
#include "Aql/OptimizerRulesFeature.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/files.h"
#include "Cluster/ClusterFeature.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "IResearch/IResearchAnalyzerFeature.h"
#include "IResearch/VelocyPackHelper.h"
#include "IResearch/IResearchCommon.h"
#include "IResearch/IResearchFeature.h"
#include "Logger/Logger.h"
#include "RestServer/AqlFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/DatabasePathFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "RestServer/FlushFeature.h"
#include "RestServer/SystemDatabaseFeature.h"
#include "RestServer/ViewTypesFeature.h"
#include "Sharding/ShardingFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/OperationOptions.h"
#include "Utils/SingleCollectionTransaction.h"
#include "V8Server/V8DealerFeature.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/LogicalView.h"
#include "VocBase/Methods/Collections.h"

#if USE_ENTERPRISE
#include "Enterprise/Ldap/LdapFeature.h"
#endif

namespace {
static const VPackBuilder systemDatabaseBuilder = dbArgsBuilder();
static const VPackSlice   systemDatabaseArgs = systemDatabaseBuilder.slice();

struct TestAttributeX : public irs::attribute {
  static constexpr irs::string_ref type_name() noexcept {
    return "TestAttributeX";
  }
};

REGISTER_ATTRIBUTE(TestAttributeX);  // required to open reader on segments with analized fields

struct TestAttributeY : public irs::attribute {
  static constexpr irs::string_ref type_name() noexcept {
    return "TestAttributeY";
  }
};

REGISTER_ATTRIBUTE(TestAttributeY);  // required to open reader on segments with analized fields

class TestAnalyzer : public irs::analysis::analyzer {
 public:
  static constexpr irs::string_ref type_name() noexcept {
    return "TestInsertAnalyzer";
  }

  static ptr make(irs::string_ref const& args) {
    PTR_NAMED(TestAnalyzer, ptr, args);
    return ptr;
  }

  static bool normalize(irs::string_ref const& args, std::string& out) {
    auto slice = arangodb::iresearch::slice(args);
    if (slice.isNull()) throw std::exception();
    if (slice.isNone()) return false;
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

  TestAnalyzer(irs::string_ref const& value)
      : irs::analysis::analyzer(irs::type<TestAnalyzer>::get()) {

    auto slice = arangodb::iresearch::slice(value);
    auto arg = slice.get("args").copyString();

    if (arg == "X") {
      _px = &_x;
    } else if (arg == "Y") {
      _py = &_y;
    }
  }

  virtual irs::attribute* get_mutable(irs::type_info::type_id type) noexcept override {
    if (type == irs::type<TestAttributeX>::id()) {
      return _px;
    }
    if (type == irs::type<TestAttributeY>::id()) {
      return _py;
    }
    if (type == irs::type<irs::increment>::id()) {
      return &_inc;
    }
    if (type == irs::type<irs::term_attribute>::id()) {
      return &_term;
    }
    return nullptr;
  }

  virtual bool next() override {
    _term.value = _data;
    _data = irs::bytes_ref::NIL;

    return !_term.value.null();
  }

  virtual bool reset(irs::string_ref const& data) override {
    _data = irs::ref_cast<irs::byte_type>(data);
    _term.value = irs::bytes_ref::NIL;

    return true;
  }

 private:
  irs::bytes_ref _data;
  irs::increment _inc;
  irs::term_attribute _term;
  TestAttributeX _x;
  TestAttributeY _y;
  irs::attribute* _px{};
  irs::attribute* _py{};
};

REGISTER_ANALYZER_VPACK(TestAnalyzer, TestAnalyzer::make, TestAnalyzer::normalize);

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

class IResearchIndexTest
    : public ::testing::Test,
      public arangodb::tests::LogSuppressor<arangodb::Logger::AUTHENTICATION, arangodb::LogLevel::ERR>,
      public arangodb::tests::LogSuppressor<arangodb::Logger::AQL, arangodb::LogLevel::ERR> {
 protected:
  arangodb::tests::mocks::MockAqlServer server;

 private:
  TRI_vocbase_t* _vocbase;

 protected:
  IResearchIndexTest() : server(false) {
    arangodb::tests::init(true);

    server.addFeature<arangodb::FlushFeature>(false);
    server.startFeatures();

    auto& analyzers = server.getFeature<arangodb::iresearch::IResearchAnalyzerFeature>();
    arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;

    auto& dbFeature = server.getFeature<arangodb::DatabaseFeature>();
    dbFeature.createDatabase(testDBInfo(server.server()), _vocbase);  // required for IResearchAnalyzerFeature::emplace(...)
    std::shared_ptr<arangodb::LogicalCollection> unused;
    arangodb::OperationOptions options(arangodb::ExecContext::current());
    arangodb::methods::Collections::createSystem(*_vocbase, options,
                                                 arangodb::tests::AnalyzerCollectionName,
                                                 false, unused);
    analyzers.emplace(
        result, "testVocbase::test_A", "TestInsertAnalyzer",
        arangodb::velocypack::Parser::fromJson("{ \"args\": \"X\" }")->slice());
    analyzers.emplace(
        result, "testVocbase::test_B", "TestInsertAnalyzer",
        arangodb::velocypack::Parser::fromJson("{ \"args\": \"Y\" }")->slice());

    auto& dbPathFeature = server.getFeature<arangodb::DatabasePathFeature>();
    arangodb::tests::setDatabasePath(dbPathFeature);  // ensure test data is stored in a unique directory
  }

  TRI_vocbase_t& vocbase() { return *_vocbase; }
};

}  // namespace

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

// test indexing with multiple analyzers (on different collections) will return results only for matching analyzer
TEST_F(IResearchIndexTest, test_analyzer) {
  auto createCollection0 = arangodb::velocypack::Parser::fromJson(
      "{ \"name\": \"testCollection0\" }");
  auto createCollection1 = arangodb::velocypack::Parser::fromJson(
      "{ \"name\": \"testCollection1\" }");
  auto createView = arangodb::velocypack::Parser::fromJson(
      "{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
  auto collection0 = vocbase().createCollection(createCollection0->slice());
  ASSERT_NE(nullptr, collection0);
  auto collection1 = vocbase().createCollection(createCollection1->slice());
  ASSERT_NE(nullptr, collection1);
  auto viewImpl = vocbase().createView(createView->slice());
  ASSERT_NE(nullptr, viewImpl);

  // populate collections
  {
    auto doc0 = arangodb::velocypack::Parser::fromJson(
        "{ \"seq\": 0, \"X\": \"abc\", \"Y\": \"def\" }");
    auto doc1 = arangodb::velocypack::Parser::fromJson(
        "{ \"seq\": 1, \"X\": \"abc\", \"Y\": \"def\" }");
    static std::vector<std::string> const EMPTY;
    std::vector<std::string> collections{collection0->name(), collection1->name()};
    arangodb::transaction::Methods trx(arangodb::transaction::StandaloneContext::Create(vocbase()),
                                       EMPTY, collections, EMPTY,
                                       arangodb::transaction::Options());
    EXPECT_TRUE(trx.begin().ok());
    EXPECT_TRUE(trx.insert(collection0->name(), doc0->slice(), arangodb::OperationOptions())
                    .ok());
    EXPECT_TRUE(trx.insert(collection1->name(), doc1->slice(), arangodb::OperationOptions())
                    .ok());
    EXPECT_TRUE(trx.commit().ok());
  }

  // link collections with view
  {
    auto updateJson = arangodb::velocypack::Parser::fromJson(
        "{ \"links\": { \
      \"testCollection0\": { \"fields\": { \
        \"X\": { \"analyzers\": [ \"test_A\", \"test_B\" ] }, \
        \"Y\": { \"analyzers\": [ \"test_B\" ] } \
      } }, \
      \"testCollection1\": { \"fields\": { \
        \"X\": { \"analyzers\": [ \"test_A\" ] }, \
        \"Y\": { \"analyzers\": [ \"test_A\" ] } \
      } } \
    } }");

    EXPECT_TRUE(viewImpl->properties(updateJson->slice(), false).ok());
  }

  // docs match from both collections (2 analyzers used for collection0, 1 analyzer used for collection 1)
  {
    auto result = arangodb::tests::executeQuery(
        vocbase(),
        "FOR d IN testView SEARCH ANALYZER(PHRASE(d.X, 'abc', 'test_A'), "
        "'test_B') OPTIONS { waitForSync: true } SORT d.seq RETURN d",
        nullptr);
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    EXPECT_TRUE(slice.isArray());
    std::vector<size_t> expected{0, 1};
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      auto key = resolved.get("seq");
      EXPECT_TRUE(i < expected.size());
      EXPECT_EQ(expected[i++], key.getNumber<size_t>());
    }

    EXPECT_EQ(i, expected.size());
  }

  // docs match from both collections (2 analyzers used for collection0, 1 analyzer used for collection 1)
  {
    auto result = arangodb::tests::executeQuery(
        vocbase(),
        "FOR d IN testView SEARCH PHRASE(d.X, 'abc', 'test_A') OPTIONS { "
        "waitForSync: true } SORT d.seq RETURN d",
        nullptr);
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    EXPECT_TRUE(slice.isArray());
    std::vector<size_t> expected{0, 1};
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      auto key = resolved.get("seq");
      EXPECT_TRUE(i < expected.size());
      EXPECT_EQ(expected[i++], key.getNumber<size_t>());
    }

    EXPECT_EQ(i, expected.size());
  }

  // docs match from both collections (2 analyzers used for collection0, 1 analyzer used for collection 1)
  {
    auto result = arangodb::tests::executeQuery(
        vocbase(),
        "FOR d IN testView SEARCH ANALYZER(PHRASE(d.X, 'abc'), 'test_A') "
        "OPTIONS { waitForSync: true } SORT d.seq RETURN d",
        nullptr);
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    EXPECT_TRUE(slice.isArray());
    std::vector<size_t> expected{0, 1};
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      auto key = resolved.get("seq");
      EXPECT_TRUE(i < expected.size());
      EXPECT_EQ(expected[i++], key.getNumber<size_t>());
    }

    EXPECT_EQ(i, expected.size());
  }

  // docs match from collection0 (2 analyzers used)
  {
    auto result =
        arangodb::tests::executeQuery(vocbase(),
                                      "FOR d IN testView SEARCH PHRASE(d.X, "
                                      "'abc', 'test_B') SORT d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    EXPECT_TRUE(slice.isArray());
    std::vector<size_t> expected{0};
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      auto key = resolved.get("seq");
      EXPECT_TRUE(i < expected.size());
      EXPECT_EQ(expected[i++], key.getNumber<size_t>());
    }

    EXPECT_EQ(i, expected.size());
  }

  // docs match from collection0 (2 analyzers used)
  {
    auto result = arangodb::tests::executeQuery(
        vocbase(),
        "FOR d IN testView SEARCH analyzer(PHRASE(d.X, 'abc'), 'test_B') SORT "
        "d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    EXPECT_TRUE(slice.isArray());
    std::vector<size_t> expected{0};
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      auto key = resolved.get("seq");
      EXPECT_TRUE(i < expected.size());
      EXPECT_EQ(expected[i++], key.getNumber<size_t>());
    }

    EXPECT_EQ(i, expected.size());
  }

  // docs match from collection1 (1 analyzer used)
  {
    auto result =
        arangodb::tests::executeQuery(vocbase(),
                                      "FOR d IN testView SEARCH PHRASE(d.Y, "
                                      "'def', 'test_A') SORT d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    EXPECT_TRUE(slice.isArray());
    std::vector<size_t> expected{1};
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      auto key = resolved.get("seq");
      EXPECT_TRUE(i < expected.size());
      EXPECT_EQ(expected[i++], key.getNumber<size_t>());
    }

    EXPECT_EQ(i, expected.size());
  }

  // docs match from collection1 (1 analyzer used)
  {
    auto result = arangodb::tests::executeQuery(
        vocbase(),
        "FOR d IN testView SEARCH ANALYZER(PHRASE(d.Y, 'def', 'test_A'), "
        "'test_B') SORT d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    EXPECT_TRUE(slice.isArray());
    std::vector<size_t> expected{1};
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      auto key = resolved.get("seq");
      EXPECT_TRUE(i < expected.size());
      EXPECT_EQ(expected[i++], key.getNumber<size_t>());
    }

    EXPECT_EQ(i, expected.size());
  }

  // docs match from collection1 (1 analyzer used)
  {
    auto result =
        arangodb::tests::executeQuery(vocbase(),
                                      "FOR d IN testView SEARCH PHRASE(d.Y, "
                                      "'def', 'test_A') SORT d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    EXPECT_TRUE(slice.isArray());
    std::vector<size_t> expected{1};
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      auto key = resolved.get("seq");
      EXPECT_TRUE(i < expected.size());
      EXPECT_EQ(expected[i++], key.getNumber<size_t>());
    }

    EXPECT_EQ(i, expected.size());
  }

  // docs match from collection1 (1 analyzer used)
  {
    auto result = arangodb::tests::executeQuery(
        vocbase(),
        "FOR d IN testView SEARCH ANALYZER(PHRASE(d.Y, 'def'), 'test_A') SORT "
        "d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    EXPECT_TRUE(slice.isArray());
    std::vector<size_t> expected{1};
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      auto key = resolved.get("seq");
      EXPECT_TRUE(i < expected.size());
      EXPECT_EQ(expected[i++], key.getNumber<size_t>());
    }

    EXPECT_EQ(i, expected.size());
  }
}

// test concurrent indexing with analyzers into view
TEST_F(IResearchIndexTest, test_async_index) {
  auto createCollection0 = arangodb::velocypack::Parser::fromJson(
      "{ \"name\": \"testCollection0\" }");
  auto createCollection1 = arangodb::velocypack::Parser::fromJson(
      "{ \"name\": \"testCollection1\" }");
  auto createView = arangodb::velocypack::Parser::fromJson(
      "{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
  auto collection0 = vocbase.createCollection(createCollection0->slice());
  ASSERT_NE(nullptr, collection0);
  auto collection1 = vocbase.createCollection(createCollection1->slice());
  ASSERT_NE(nullptr, collection1);
  auto viewImpl = vocbase.createView(createView->slice());
  ASSERT_NE(nullptr, viewImpl);

  // link collections with view
  {
    auto updateJson = arangodb::velocypack::Parser::fromJson(
        "{ \"links\": { \
      \"testCollection0\": { \"fields\": { \
        \"same\": { \"analyzers\": [ \"test_A\", \"test_B\" ] }, \
        \"duplicated\": { \"analyzers\": [ \"test_B\" ] } \
      } }, \
      \"testCollection1\": { \"fields\": { \
        \"same\": { \"analyzers\": [ \"test_A\" ] }, \
        \"duplicated\": { \"analyzers\": [ \"test_A\" ] } \
      } } \
    } }");

    EXPECT_TRUE(viewImpl->properties(updateJson->slice(), false).ok());
  }

  // `catch` doesn't support cuncurrent checks
  bool resThread0 = false;
  bool resThread1 = false;

  // populate collections asynchronously
  {
    std::thread thread0([collection0, &resThread0]() -> void {
      arangodb::velocypack::Builder builder;

      try {
        irs::utf8_path resource;

        resource /= irs::string_ref(arangodb::tests::testResourceDir);
        resource /= irs::string_ref("simple_sequential.json");
        builder =
            arangodb::basics::VelocyPackHelper::velocyPackFromFile(resource.utf8());
      } catch (...) {
        return;  // velocyPackFromFile(...) may throw exception
      }

      auto doc = arangodb::velocypack::Parser::fromJson(
          "{ \"seq\": 40, \"same\": \"xyz\", \"duplicated\": \"abcd\" }");
      auto slice = builder.slice();
      resThread0 = slice.isArray();
      if (!resThread0) return;

      arangodb::SingleCollectionTransaction trx(
          arangodb::transaction::StandaloneContext::Create(collection0->vocbase()),
          *collection0, arangodb::AccessMode::Type::WRITE);
      resThread0 = trx.begin().ok();
      if (!resThread0) return;

      resThread0 = trx.insert(collection0->name(), doc->slice(), arangodb::OperationOptions())
                       .ok();
      if (!resThread0) return;

      for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto res = trx.insert(collection0->name(), itr.value(),
                              arangodb::OperationOptions());
        resThread0 = res.ok();
        if (!resThread0) return;
      }

      resThread0 = trx.commit().ok();
    });

    std::thread thread1([collection1, &resThread1]() -> void {
      arangodb::velocypack::Builder builder;

      try {
        irs::utf8_path resource;

        resource /= irs::string_ref(arangodb::tests::testResourceDir);
        resource /= irs::string_ref("simple_sequential.json");
        builder =
            arangodb::basics::VelocyPackHelper::velocyPackFromFile(resource.utf8());
      } catch (...) {
        return;  // velocyPackFromFile(...) may throw exception
      }

      auto doc = arangodb::velocypack::Parser::fromJson(
          "{ \"seq\": 50, \"same\": \"xyz\", \"duplicated\": \"abcd\" }");
      auto slice = builder.slice();
      resThread1 = slice.isArray();
      if (!resThread1) return;

      arangodb::SingleCollectionTransaction trx(
          arangodb::transaction::StandaloneContext::Create(collection1->vocbase()),
          *collection1, arangodb::AccessMode::Type::WRITE);
      resThread1 = trx.begin().ok();
      if (!resThread1) return;

      resThread1 = trx.insert(collection1->name(), doc->slice(), arangodb::OperationOptions())
                       .ok();
      if (!resThread1) return;

      for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto res = trx.insert(collection1->name(), itr.value(),
                              arangodb::OperationOptions());
        resThread1 = res.ok();
        if (!resThread1) return;
      }

      resThread1 = trx.commit().ok();
    });

    thread0.join();
    thread1.join();
  }

  EXPECT_TRUE(resThread0);
  EXPECT_TRUE(resThread1);

  // docs match from both collections (2 analyzers used for collectio0, 1 analyzer used for collection 1)
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH ANALYZER(PHRASE(d.same, 'xyz', 'test_A'), "
        "'test_B') OPTIONS { waitForSync: true } SORT d.seq RETURN d",
        nullptr);
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    EXPECT_TRUE(slice.isArray());
    std::vector<size_t> expected{0,  0,  1,  1,  2,  2,  3,  3,  4,  4,  5,
                                 5,  6,  6,  7,  7,  8,  8,  9,  9,  10, 10,
                                 11, 11, 12, 12, 13, 13, 14, 14, 15, 15, 16,
                                 16, 17, 17, 18, 18, 19, 19, 20, 20, 21, 21,
                                 22, 22, 23, 23, 24, 24, 25, 25, 26, 26, 27,
                                 27, 28, 28, 29, 29, 30, 30, 31, 31, 40, 50};
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      auto key = resolved.get("seq");
      EXPECT_TRUE(i < expected.size());
      EXPECT_EQ(expected[i++], key.getNumber<size_t>());
    }

    EXPECT_EQ(i, expected.size());
  }

  // docs match from both collections (2 analyzers used for collectio0, 1 analyzer used for collection 1)
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH PHRASE(d.same, 'xyz', 'test_A') OPTIONS { "
        "waitForSync : true } SORT d.seq RETURN d",
        nullptr);
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    EXPECT_TRUE(slice.isArray());
    std::vector<size_t> expected{0,  0,  1,  1,  2,  2,  3,  3,  4,  4,  5,
                                 5,  6,  6,  7,  7,  8,  8,  9,  9,  10, 10,
                                 11, 11, 12, 12, 13, 13, 14, 14, 15, 15, 16,
                                 16, 17, 17, 18, 18, 19, 19, 20, 20, 21, 21,
                                 22, 22, 23, 23, 24, 24, 25, 25, 26, 26, 27,
                                 27, 28, 28, 29, 29, 30, 30, 31, 31, 40, 50};
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      auto key = resolved.get("seq");
      EXPECT_TRUE(i < expected.size());
      EXPECT_EQ(expected[i++], key.getNumber<size_t>());
    }

    EXPECT_EQ(i, expected.size());
  }

  // docs match from both collections (2 analyzers used for collectio0, 1 analyzer used for collection 1)
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH ANALYZER(PHRASE(d.same, 'xyz'), 'test_A') "
        "OPTIONS { waitForSync : true } SORT d.seq RETURN d",
        nullptr);
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    EXPECT_TRUE(slice.isArray());
    std::vector<size_t> expected{0,  0,  1,  1,  2,  2,  3,  3,  4,  4,  5,
                                 5,  6,  6,  7,  7,  8,  8,  9,  9,  10, 10,
                                 11, 11, 12, 12, 13, 13, 14, 14, 15, 15, 16,
                                 16, 17, 17, 18, 18, 19, 19, 20, 20, 21, 21,
                                 22, 22, 23, 23, 24, 24, 25, 25, 26, 26, 27,
                                 27, 28, 28, 29, 29, 30, 30, 31, 31, 40, 50};
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      auto key = resolved.get("seq");
      EXPECT_TRUE(i < expected.size());
      EXPECT_EQ(expected[i++], key.getNumber<size_t>());
    }

    EXPECT_EQ(i, expected.size());
  }

  // docs match from collection0 (2 analyzers used)
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH ANALYZER(PHRASE(d.same, 'xyz', 'test_B'), "
        "'identity') SORT d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    EXPECT_TRUE(slice.isArray());
    std::vector<size_t> expected{0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10,
                                 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
                                 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 40};
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      auto key = resolved.get("seq");
      EXPECT_TRUE(i < expected.size());
      EXPECT_EQ(expected[i++], key.getNumber<size_t>());
    }

    EXPECT_EQ(i, expected.size());
  }

  // docs match from collection0 (2 analyzers used)
  {
    auto result =
        arangodb::tests::executeQuery(vocbase,
                                      "FOR d IN testView SEARCH PHRASE(d.same, "
                                      "'xyz', 'test_B') SORT d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    EXPECT_TRUE(slice.isArray());
    std::vector<size_t> expected{0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10,
                                 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
                                 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 40};
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      auto key = resolved.get("seq");
      EXPECT_TRUE(i < expected.size());
      EXPECT_EQ(expected[i++], key.getNumber<size_t>());
    }

    EXPECT_EQ(i, expected.size());
  }

  // docs match from collection0 (2 analyzers used)
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH ANALYZER(PHRASE(d.same, 'xyz'), 'test_B') "
        "SORT d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    EXPECT_TRUE(slice.isArray());
    std::vector<size_t> expected{0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10,
                                 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
                                 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 40};
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      auto key = resolved.get("seq");
      EXPECT_TRUE(i < expected.size());
      EXPECT_EQ(expected[i++], key.getNumber<size_t>());
    }

    EXPECT_EQ(i, expected.size());
  }

  // docs match from collection1 (1 analyzer used)
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH ANALYZER(PHRASE(d.duplicated, 'abcd', "
        "'test_A'), 'test_B') SORT d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    EXPECT_TRUE(slice.isArray());
    std::vector<size_t> expected{0, 4, 10, 20, 26, 30, 50};
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      auto key = resolved.get("seq");
      EXPECT_TRUE(i < expected.size());
      EXPECT_EQ(expected[i++], key.getNumber<size_t>());
    }

    EXPECT_EQ(i, expected.size());
  }

  // docs match from collection1 (1 analyzer used)
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH PHRASE(d.duplicated, 'abcd', 'test_A') SORT "
        "d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    EXPECT_TRUE(slice.isArray());
    std::vector<size_t> expected{0, 4, 10, 20, 26, 30, 50};
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      auto key = resolved.get("seq");
      EXPECT_TRUE(i < expected.size());
      EXPECT_EQ(expected[i++], key.getNumber<size_t>());
    }

    EXPECT_EQ(i, expected.size());
  }

  // docs match from collection1 (1 analyzer used)
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH ANALYZER(PHRASE(d.duplicated, 'abcd'), "
        "'test_A') SORT d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    EXPECT_TRUE(slice.isArray());
    std::vector<size_t> expected{0, 4, 10, 20, 26, 30, 50};
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      auto key = resolved.get("seq");
      EXPECT_TRUE(i < expected.size());
      EXPECT_EQ(expected[i++], key.getNumber<size_t>());
    }

    EXPECT_EQ(i, expected.size());
  }
}

// test indexing selected fields will omit non-indexed fields during query
TEST_F(IResearchIndexTest, test_fields) {
  auto createCollection0 = arangodb::velocypack::Parser::fromJson(
      "{ \"name\": \"testCollection0\" }");
  auto createCollection1 = arangodb::velocypack::Parser::fromJson(
      "{ \"name\": \"testCollection1\" }");
  auto createView = arangodb::velocypack::Parser::fromJson(
      "{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
  auto collection0 = vocbase.createCollection(createCollection0->slice());
  ASSERT_NE(nullptr, collection0);
  auto collection1 = vocbase.createCollection(createCollection1->slice());
  ASSERT_NE(nullptr, collection1);
  auto viewImpl = vocbase.createView(createView->slice());
  ASSERT_NE(nullptr, viewImpl);

  // populate collections
  {
    auto doc0 = arangodb::velocypack::Parser::fromJson(
        "{ \"seq\": 0, \"X\": \"abc\", \"Y\": \"def\" }");
    auto doc1 = arangodb::velocypack::Parser::fromJson(
        "{ \"seq\": 1, \"X\": \"abc\", \"Y\": \"def\" }");
    static std::vector<std::string> const EMPTY;
    std::vector<std::string> collections{collection0->name(), collection1->name()};
    arangodb::transaction::Methods trx(arangodb::transaction::StandaloneContext::Create(vocbase),
                                       EMPTY, collections, EMPTY,
                                       arangodb::transaction::Options());
    EXPECT_TRUE(trx.begin().ok());
    EXPECT_TRUE(trx.insert(collection0->name(), doc0->slice(), arangodb::OperationOptions())
                    .ok());
    EXPECT_TRUE(trx.insert(collection1->name(), doc1->slice(), arangodb::OperationOptions())
                    .ok());
    EXPECT_TRUE(trx.commit().ok());
  }

  // link collections with view
  {
    auto updateJson = arangodb::velocypack::Parser::fromJson(
        "{ \"links\": { \
      \"testCollection0\": { \"fields\": { \
        \"X\": { }, \
        \"Y\": { } \
      } }, \
      \"testCollection1\": { \"fields\": { \
        \"X\": { } \
      } } \
    } }");

    EXPECT_TRUE(viewImpl->properties(updateJson->slice(), false).ok());
  }

  // docs match from both collections
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH d.X == 'abc' OPTIONS { waitForSync: true } "
        "SORT d.seq RETURN d",
        nullptr);
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    EXPECT_TRUE(slice.isArray());
    std::vector<size_t> expected{0, 1};
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      auto key = resolved.get("seq");
      EXPECT_TRUE(i < expected.size());
      EXPECT_EQ(expected[i++], key.getNumber<size_t>());
    }

    EXPECT_EQ(i, expected.size());
  }

  // docs match from collection0
  {
    auto result = arangodb::tests::executeQuery(
        vocbase, "FOR d IN testView SEARCH d.Y == 'def' SORT d.seq RETURN d");
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    EXPECT_TRUE(slice.isArray());
    std::vector<size_t> expected{0};
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      auto key = resolved.get("seq");
      EXPECT_TRUE(i < expected.size());
      EXPECT_EQ(expected[i++], key.getNumber<size_t>());
    }

    EXPECT_EQ(i, expected.size());
  }
}
