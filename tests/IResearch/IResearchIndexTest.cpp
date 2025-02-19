////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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

#include "../3rdParty/iresearch/tests/tests_config.hpp"
#include "analysis/analyzers.hpp"
#include "analysis/token_attributes.hpp"
#include <filesystem>

#include "velocypack/Iterator.h"
#include "velocypack/Parser.h"

#include "Mocks/LogLevels.h"
#include "Mocks/Servers.h"
#include "Mocks/StorageEngineMock.h"

#include "Aql/AqlFunctionFeature.h"
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
#ifdef USE_V8
#include "V8Server/V8DealerFeature.h"
#endif
#include "VocBase/LogicalCollection.h"
#include "VocBase/LogicalView.h"
#include "VocBase/Methods/Collections.h"

namespace {
static const VPackBuilder systemDatabaseBuilder = dbArgsBuilder();
static const VPackSlice systemDatabaseArgs = systemDatabaseBuilder.slice();

struct TestAttributeX : public irs::attribute {
  static constexpr std::string_view type_name() noexcept {
    return "TestAttributeX";
  }
};

REGISTER_ATTRIBUTE(TestAttributeX);  // required to open reader on segments with
                                     // analized fields

struct TestAttributeY : public irs::attribute {
  static constexpr std::string_view type_name() noexcept {
    return "TestAttributeY";
  }
};

REGISTER_ATTRIBUTE(TestAttributeY);  // required to open reader on segments with
                                     // analized fields

class TestAnalyzer final : public irs::analysis::TypedAnalyzer<TestAnalyzer> {
 public:
  static constexpr std::string_view type_name() noexcept {
    return "TestInsertAnalyzer";
  }

  static ptr make(std::string_view args) {
    return std::make_unique<TestAnalyzer>(args);
  }

  static bool normalize(std::string_view args, std::string& out) {
    auto slice = arangodb::iresearch::slice(args);
    if (slice.isNull()) throw std::exception();
    if (slice.isNone()) return false;
    arangodb::velocypack::Builder builder;
    if (slice.isString()) {
      VPackObjectBuilder scope(&builder);
      arangodb::iresearch::addStringRef(
          builder, "args", arangodb::iresearch::getStringRef(slice));
    } else if (slice.isObject() && slice.hasKey("args") &&
               slice.get("args").isString()) {
      VPackObjectBuilder scope(&builder);
      arangodb::iresearch::addStringRef(
          builder, "args",
          arangodb::iresearch::getStringRef(slice.get("args")));
    } else {
      return false;
    }
    out = builder.buffer()->toString();
    return true;
  }

  TestAnalyzer(std::string_view value) {
    auto slice = arangodb::iresearch::slice(value);
    auto arg = slice.get("args").copyString();

    if (arg == "X") {
      _px = &_x;
    } else if (arg == "Y") {
      _py = &_y;
    }
  }

  irs::attribute* get_mutable(irs::type_info::type_id type) noexcept final {
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

  bool next() final {
    _term.value = _data;
    _data = irs::bytes_view{};

    return !irs::IsNull(_term.value);
  }

  bool reset(std::string_view data) final {
    _data = irs::ViewCast<irs::byte_type>(data);
    _term.value = irs::bytes_view{};

    return true;
  }

 private:
  irs::bytes_view _data;
  irs::increment _inc;
  irs::term_attribute _term;
  TestAttributeX _x;
  TestAttributeY _y;
  irs::attribute* _px{};
  irs::attribute* _py{};
};

REGISTER_ANALYZER_VPACK(TestAnalyzer, TestAnalyzer::make,
                        TestAnalyzer::normalize);

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

class IResearchIndexTest
    : public ::testing::Test,
      public arangodb::tests::LogSuppressor<arangodb::Logger::AUTHENTICATION,
                                            arangodb::LogLevel::ERR>,
      public arangodb::tests::LogSuppressor<arangodb::Logger::AQL,
                                            arangodb::LogLevel::ERR> {
 protected:
  arangodb::tests::mocks::MockAqlServer server;

 private:
  TRI_vocbase_t* _vocbase;

 protected:
  IResearchIndexTest() : server(false) {
    arangodb::tests::init(true);

    server.addFeature<arangodb::FlushFeature>(false);
    server.startFeatures();

    auto& analyzers =
        server.getFeature<arangodb::iresearch::IResearchAnalyzerFeature>();
    arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;

    auto& dbFeature = server.getFeature<arangodb::DatabaseFeature>();
    dbFeature.createDatabase(
        testDBInfo(server.server()),
        _vocbase);  // required for IResearchAnalyzerFeature::emplace(...)
    std::shared_ptr<arangodb::LogicalCollection> unused;
    arangodb::OperationOptions options(arangodb::ExecContext::current());
    arangodb::methods::Collections::createSystem(
        *_vocbase, options, arangodb::tests::AnalyzerCollectionName, false,
        unused);
    analyzers.emplace(
        result, "testVocbase::test_A", "TestInsertAnalyzer",
        arangodb::velocypack::Parser::fromJson("{ \"args\": \"X\" }")->slice(),
        arangodb::transaction::OperationOriginTestCase{});
    analyzers.emplace(
        result, "testVocbase::test_B", "TestInsertAnalyzer",
        arangodb::velocypack::Parser::fromJson("{ \"args\": \"Y\" }")->slice(),
        arangodb::transaction::OperationOriginTestCase{});

#ifdef USE_ENTERPRISE
    analyzers.emplace(
        result, "testVocbase::geojson", "geojson",
        arangodb::velocypack::Parser::fromJson(
            " { \"type\": \"shape\", \"options\":{\"maxCells\":20,\
                              \"minLevel\":4, \"maxLevel\":23}}")
            ->slice(),
        arangodb::transaction::OperationOriginTestCase{});
#endif

    auto& dbPathFeature = server.getFeature<arangodb::DatabasePathFeature>();
    arangodb::tests::setDatabasePath(
        dbPathFeature);  // ensure test data is stored in a unique directory
  }

  TRI_vocbase_t& vocbase() { return *_vocbase; }
};

}  // namespace

// test indexing with multiple analyzers (on different collections) will return
// results only for matching analyzer
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
  auto viewImpl = vocbase().createView(createView->slice(), false);
  ASSERT_NE(nullptr, viewImpl);

  // populate collections
  {
    auto doc0 = arangodb::velocypack::Parser::fromJson(
        "{ \"seq\": 0, \"X\": \"abc\", \"Y\": \"def\" }");
    auto doc1 = arangodb::velocypack::Parser::fromJson(
        "{ \"seq\": 1, \"X\": \"abc\", \"Y\": \"def\" }");
    static std::vector<std::string> const EMPTY;
    std::vector<std::string> collections{collection0->name(),
                                         collection1->name()};
    arangodb::transaction::Methods trx(
        arangodb::transaction::StandaloneContext::create(
            vocbase(), arangodb::transaction::OperationOriginTestCase{}),
        EMPTY, collections, EMPTY, arangodb::transaction::Options());
    EXPECT_TRUE(trx.begin().ok());
    EXPECT_TRUE(trx.insert(collection0->name(), doc0->slice(),
                           arangodb::OperationOptions())
                    .ok());
    EXPECT_TRUE(trx.insert(collection1->name(), doc1->slice(),
                           arangodb::OperationOptions())
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

    EXPECT_TRUE(viewImpl->properties(updateJson->slice(), true, false).ok());

    auto nestedIndex = arangodb::velocypack::Parser::fromJson(
        R"({"type":"inverted", "name":"nest1",
            "fields":[{"name":"name", "nested":[{"name":"nested", "nested":[{"name":"color1"}]}]}]})");
    bool createdIndex;

#ifdef USE_ENTERPRISE
    auto index = collection0->createIndex(nestedIndex->slice(), createdIndex)
                     .waitAndGet();
    //    collection->createIndex(nestedIndex->slice(), result);
    //    ASSERT_TRUE(createdIndex);

    //    VPackBuilder outputDefinition;
    //    ASSERT_TRUE(arangodb::methods::Indexes::ensureIndex(collection,
    //    nestedIndex->slice(), createdIndex, outputDefinition).ok());
#else
    ASSERT_THROW(collection0->createIndex(nestedIndex->slice(), createdIndex)
                     .waitAndGet(),
                 arangodb::basics::Exception);
#endif
  }

  // docs match from both collections (2 analyzers used for collection0, 1
  // analyzer used for collection 1)
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

  // docs match from both collections (2 analyzers used for collection0, 1
  // analyzer used for collection 1)
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

  // docs match from both collections (2 analyzers used for collection0, 1
  // analyzer used for collection 1)
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
  TRI_vocbase_t vocbase(testDBInfo(server.server()));
  auto collection0 = vocbase.createCollection(createCollection0->slice());
  ASSERT_NE(nullptr, collection0);
  auto collection1 = vocbase.createCollection(createCollection1->slice());
  ASSERT_NE(nullptr, collection1);
  auto viewImpl = vocbase.createView(createView->slice(), false);
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

    EXPECT_TRUE(viewImpl->properties(updateJson->slice(), true, false).ok());
  }

  // `catch` doesn't support cuncurrent checks
  bool resThread0 = false;
  bool resThread1 = false;

  // populate collections asynchronously
  {
    std::thread thread0([collection0, &resThread0]() -> void {
      arangodb::velocypack::Builder builder;

      try {
        std::filesystem::path resource;

        resource /= std::string_view(arangodb::tests::testResourceDir);
        resource /= std::string_view("simple_sequential.json");
        builder = arangodb::basics::VelocyPackHelper::velocyPackFromFile(
            resource.string());
      } catch (...) {
        return;  // velocyPackFromFile(...) may throw exception
      }

      auto doc = arangodb::velocypack::Parser::fromJson(
          "{ \"seq\": 40, \"same\": \"xyz\", \"duplicated\": \"abcd\" }");
      auto slice = builder.slice();
      resThread0 = slice.isArray();
      if (!resThread0) return;

      arangodb::SingleCollectionTransaction trx(
          arangodb::transaction::StandaloneContext::create(
              collection0->vocbase(),
              arangodb::transaction::OperationOriginTestCase{}),
          *collection0, arangodb::AccessMode::Type::WRITE);
      resThread0 = trx.begin().ok();
      if (!resThread0) return;

      resThread0 = trx.insert(collection0->name(), doc->slice(),
                              arangodb::OperationOptions())
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
        std::filesystem::path resource;

        resource /= std::string_view(arangodb::tests::testResourceDir);
        resource /= std::string_view("simple_sequential.json");
        builder = arangodb::basics::VelocyPackHelper::velocyPackFromFile(
            resource.string());
      } catch (...) {
        return;  // velocyPackFromFile(...) may throw exception
      }

      auto doc = arangodb::velocypack::Parser::fromJson(
          "{ \"seq\": 50, \"same\": \"xyz\", \"duplicated\": \"abcd\" }");
      auto slice = builder.slice();
      resThread1 = slice.isArray();
      if (!resThread1) return;

      arangodb::SingleCollectionTransaction trx(
          arangodb::transaction::StandaloneContext::create(
              collection1->vocbase(),
              arangodb::transaction::OperationOriginTestCase{}),
          *collection1, arangodb::AccessMode::Type::WRITE);
      resThread1 = trx.begin().ok();
      if (!resThread1) return;

      resThread1 = trx.insert(collection1->name(), doc->slice(),
                              arangodb::OperationOptions())
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

  // docs match from both collections (2 analyzers used for collectio0, 1
  // analyzer used for collection 1)
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH ANALYZER(PHRASE(d.same, 'xyz', 'test_A'), "
        "'test_B') OPTIONS { waitForSync: true } SORT d.seq RETURN d",
        nullptr);
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    EXPECT_TRUE(slice.isArray());
    std::vector<size_t> expected{
        0,  0,  1,  1,  2,  2,  3,  3,  4,  4,  5,  5,  6,  6,  7,  7,  8,
        8,  9,  9,  10, 10, 11, 11, 12, 12, 13, 13, 14, 14, 15, 15, 16, 16,
        17, 17, 18, 18, 19, 19, 20, 20, 21, 21, 22, 22, 23, 23, 24, 24, 25,
        25, 26, 26, 27, 27, 28, 28, 29, 29, 30, 30, 31, 31, 40, 50};
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      auto key = resolved.get("seq");
      EXPECT_TRUE(i < expected.size());
      EXPECT_EQ(expected[i++], key.getNumber<size_t>());
    }

    EXPECT_EQ(i, expected.size());
  }

  // docs match from both collections (2 analyzers used for collectio0, 1
  // analyzer used for collection 1)
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH PHRASE(d.same, 'xyz', 'test_A') OPTIONS { "
        "waitForSync : true } SORT d.seq RETURN d",
        nullptr);
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    EXPECT_TRUE(slice.isArray());
    std::vector<size_t> expected{
        0,  0,  1,  1,  2,  2,  3,  3,  4,  4,  5,  5,  6,  6,  7,  7,  8,
        8,  9,  9,  10, 10, 11, 11, 12, 12, 13, 13, 14, 14, 15, 15, 16, 16,
        17, 17, 18, 18, 19, 19, 20, 20, 21, 21, 22, 22, 23, 23, 24, 24, 25,
        25, 26, 26, 27, 27, 28, 28, 29, 29, 30, 30, 31, 31, 40, 50};
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      auto key = resolved.get("seq");
      EXPECT_TRUE(i < expected.size());
      EXPECT_EQ(expected[i++], key.getNumber<size_t>());
    }

    EXPECT_EQ(i, expected.size());
  }

  // docs match from both collections (2 analyzers used for collectio0, 1
  // analyzer used for collection 1)
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH ANALYZER(PHRASE(d.same, 'xyz'), 'test_A') "
        "OPTIONS { waitForSync : true } SORT d.seq RETURN d",
        nullptr);
    ASSERT_TRUE(result.result.ok());
    auto slice = result.data->slice();
    EXPECT_TRUE(slice.isArray());
    std::vector<size_t> expected{
        0,  0,  1,  1,  2,  2,  3,  3,  4,  4,  5,  5,  6,  6,  7,  7,  8,
        8,  9,  9,  10, 10, 11, 11, 12, 12, 13, 13, 14, 14, 15, 15, 16, 16,
        17, 17, 18, 18, 19, 19, 20, 20, 21, 21, 22, 22, 23, 23, 24, 24, 25,
        25, 26, 26, 27, 27, 28, 28, 29, 29, 30, 30, 31, 31, 40, 50};
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
  TRI_vocbase_t vocbase(testDBInfo(server.server()));
  auto collection0 = vocbase.createCollection(createCollection0->slice());
  ASSERT_NE(nullptr, collection0);
  auto collection1 = vocbase.createCollection(createCollection1->slice());
  ASSERT_NE(nullptr, collection1);
  auto viewImpl = vocbase.createView(createView->slice(), false);
  ASSERT_NE(nullptr, viewImpl);

  // populate collections
  {
    auto doc0 = arangodb::velocypack::Parser::fromJson(
        "{ \"seq\": 0, \"X\": \"abc\", \"Y\": \"def\" }");
    auto doc1 = arangodb::velocypack::Parser::fromJson(
        "{ \"seq\": 1, \"X\": \"abc\", \"Y\": \"def\" }");
    static std::vector<std::string> const EMPTY;
    std::vector<std::string> collections{collection0->name(),
                                         collection1->name()};
    arangodb::transaction::Methods trx(
        arangodb::transaction::StandaloneContext::create(
            vocbase, arangodb::transaction::OperationOriginTestCase{}),
        EMPTY, collections, EMPTY, arangodb::transaction::Options());
    EXPECT_TRUE(trx.begin().ok());
    EXPECT_TRUE(trx.insert(collection0->name(), doc0->slice(),
                           arangodb::OperationOptions())
                    .ok());
    EXPECT_TRUE(trx.insert(collection1->name(), doc1->slice(),
                           arangodb::OperationOptions())
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

    EXPECT_TRUE(viewImpl->properties(updateJson->slice(), true, false).ok());
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

#ifdef USE_ENTERPRISE
TEST_F(IResearchIndexTest, test_pkCached) {
  auto createCollection0 = arangodb::velocypack::Parser::fromJson(
      "{ \"name\": \"testCollection0\" }");
  auto createCollection1 = arangodb::velocypack::Parser::fromJson(
      "{ \"name\": \"testCollection1\" }");
  auto createView = arangodb::velocypack::Parser::fromJson(
      "{ \"name\": \"testView\", \"type\": \"arangosearch\", "
      "\"primaryKeyCache\":true }");
  TRI_vocbase_t vocbase(testDBInfo(server.server()));
  auto& feature = server.getFeature<arangodb::iresearch::IResearchFeature>();
  feature.setCacheUsageLimit(10000000);
  auto collection0 = vocbase.createCollection(createCollection0->slice());
  ASSERT_NE(nullptr, collection0);
  auto collection1 = vocbase.createCollection(createCollection1->slice());
  ASSERT_NE(nullptr, collection1);
  auto viewImpl = vocbase.createView(createView->slice(), false);
  ASSERT_NE(nullptr, viewImpl);

  // populate collections
  {
    auto doc0 = arangodb::velocypack::Parser::fromJson(
        "{ \"seq\": 0, \"X\": \"abc\", \"Y\": \"def\" }");
    auto doc1 = arangodb::velocypack::Parser::fromJson(
        "{ \"seq\": 1, \"X\": \"abc\", \"Y\": \"def\" }");
    static std::vector<std::string> const EMPTY;
    std::vector<std::string> collections{collection0->name(),
                                         collection1->name()};
    arangodb::transaction::Methods trx(
        arangodb::transaction::StandaloneContext::create(
            vocbase, arangodb::transaction::OperationOriginTestCase{}),
        EMPTY, collections, EMPTY, arangodb::transaction::Options());
    EXPECT_TRUE(trx.begin().ok());
    EXPECT_TRUE(trx.insert(collection0->name(), doc0->slice(),
                           arangodb::OperationOptions())
                    .ok());
    EXPECT_TRUE(trx.insert(collection1->name(), doc1->slice(),
                           arangodb::OperationOptions())
                    .ok());
    EXPECT_TRUE(trx.commit().ok());
  }

  // link collections with view
  {
    auto updateJson = arangodb::velocypack::Parser::fromJson(
        "{\"links\": { \
      \"testCollection0\": { \"fields\": { \
        \"X\": { }, \
        \"Y\": { } \
      } }, \
      \"testCollection1\": { \"fields\": { \
        \"X\": { } \
      } } \
    } }");

    EXPECT_TRUE(viewImpl->properties(updateJson->slice(), true, false).ok());
  }
  // running query to force sync
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH d.X == 'abc' OPTIONS { waitForSync: true } "
        "SORT d.seq RETURN d",
        nullptr);
    ASSERT_TRUE(result.result.ok());
  }
  ASSERT_GT(feature.columnsCacheUsage(), 0);
}

TEST_F(IResearchIndexTest, test_pkCachedInverted) {
  auto createCollection0 = arangodb::velocypack::Parser::fromJson(
      "{ \"name\": \"testCollection0\" }");
  TRI_vocbase_t vocbase(testDBInfo(server.server()));
  auto& feature = server.getFeature<arangodb::iresearch::IResearchFeature>();
  feature.setCacheUsageLimit(10000000);
  auto collection0 = vocbase.createCollection(createCollection0->slice());
  ASSERT_NE(nullptr, collection0);

  {
    auto doc0 = arangodb::velocypack::Parser::fromJson(
        "{ \"seq\": 0, \"X\": \"abc\", \"Y\": \"def\" }");
    auto doc1 = arangodb::velocypack::Parser::fromJson(
        "{ \"seq\": 1, \"X\": \"abc\", \"Y\": \"def\" }");
    static std::vector<std::string> const EMPTY;
    std::vector<std::string> collections{collection0->name()};
    arangodb::transaction::Methods trx(
        arangodb::transaction::StandaloneContext::create(
            vocbase, arangodb::transaction::OperationOriginTestCase{}),
        EMPTY, collections, EMPTY, arangodb::transaction::Options());
    EXPECT_TRUE(trx.begin().ok());
    EXPECT_TRUE(trx.insert(collection0->name(), doc0->slice(),
                           arangodb::OperationOptions())
                    .ok());
    EXPECT_TRUE(trx.insert(collection0->name(), doc1->slice(),
                           arangodb::OperationOptions())
                    .ok());
    EXPECT_TRUE(trx.commit().ok());
  }

  // link collections with view
  {
    auto updateJson = arangodb::velocypack::Parser::fromJson(R"({
      "name": "inverted",
      "type": "inverted",
      "primaryKeyCache": true,
      "fields":  ["X", "Y"]})");

    bool created{false};
    ASSERT_TRUE(
        collection0->createIndex(updateJson->slice(), created).waitAndGet());
    ASSERT_TRUE(created);
  }
  // running query to force sync
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testCollection0 OPTIONS { waitForSync: true, indexHint: "
        "'inverted', forceIndexHint: true} FILTER d.X == 'abc' "
        "SORT d.seq RETURN d",
        nullptr);
    ASSERT_TRUE(result.result.ok());
  }
  ASSERT_GT(feature.columnsCacheUsage(), 0);
}

TEST_F(IResearchIndexTest, test_pkCachedRestricted) {
  auto createCollection0 = arangodb::velocypack::Parser::fromJson(
      "{ \"name\": \"testCollection0\" }");
  auto createCollection1 = arangodb::velocypack::Parser::fromJson(
      "{ \"name\": \"testCollection1\" }");
  auto createView = arangodb::velocypack::Parser::fromJson(
      "{ \"name\": \"testView\", \"type\": \"arangosearch\", "
      "\"primaryKeyCache\":true }");
  TRI_vocbase_t vocbase(testDBInfo(server.server()));
  auto& feature = server.getFeature<arangodb::iresearch::IResearchFeature>();
  feature.setCacheUsageLimit(10);
  auto collection0 = vocbase.createCollection(createCollection0->slice());
  ASSERT_NE(nullptr, collection0);
  auto collection1 = vocbase.createCollection(createCollection1->slice());
  ASSERT_NE(nullptr, collection1);
  auto viewImpl = vocbase.createView(createView->slice(), false);
  ASSERT_NE(nullptr, viewImpl);

  // populate collections
  {
    auto doc0 = arangodb::velocypack::Parser::fromJson(
        "{ \"seq\": 0, \"X\": \"abc\", \"Y\": \"def\" }");
    auto doc1 = arangodb::velocypack::Parser::fromJson(
        "{ \"seq\": 1, \"X\": \"abc\", \"Y\": \"def\" }");
    static std::vector<std::string> const EMPTY;
    std::vector<std::string> collections{collection0->name(),
                                         collection1->name()};
    arangodb::transaction::Methods trx(
        arangodb::transaction::StandaloneContext::create(
            vocbase, arangodb::transaction::OperationOriginTestCase{}),
        EMPTY, collections, EMPTY, arangodb::transaction::Options());
    EXPECT_TRUE(trx.begin().ok());
    EXPECT_TRUE(trx.insert(collection0->name(), doc0->slice(),
                           arangodb::OperationOptions())
                    .ok());
    EXPECT_TRUE(trx.insert(collection1->name(), doc1->slice(),
                           arangodb::OperationOptions())
                    .ok());
    EXPECT_TRUE(trx.commit().ok());
  }

  // link collections with view
  {
    auto updateJson = arangodb::velocypack::Parser::fromJson(
        "{\"links\": { \
      \"testCollection0\": { \"fields\": { \
        \"X\": { }, \
        \"Y\": { } \
      } }, \
      \"testCollection1\": { \"fields\": { \
        \"X\": { } \
      } } \
    } }");

    EXPECT_TRUE(viewImpl->properties(updateJson->slice(), true, false).ok());
  }
  // running query to force sync
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH d.X == 'abc' OPTIONS { waitForSync: true } "
        "SORT d.seq RETURN d",
        nullptr);
    ASSERT_TRUE(result.result.ok());
  }
  ASSERT_GT(feature.columnsCacheUsage(), 0);
  ASSERT_LE(feature.columnsCacheUsage(), 10);
}

TEST_F(IResearchIndexTest, test_sortCached) {
  auto createCollection0 = arangodb::velocypack::Parser::fromJson(
      "{ \"name\": \"testCollection0\" }");
  auto createCollection1 = arangodb::velocypack::Parser::fromJson(
      "{ \"name\": \"testCollection1\" }");
  auto createView = arangodb::velocypack::Parser::fromJson(
      "{ \"name\": \"testView\", \"type\": \"arangosearch\",\
        \"primarySortCache\":true,\
        \"primarySort\":[{\"field\":\"X\", \"direction\":\"asc\" }]}");
  auto& feature = server.getFeature<arangodb::iresearch::IResearchFeature>();
  feature.setCacheUsageLimit(10000000);
  TRI_vocbase_t vocbase(testDBInfo(server.server()));
  auto collection0 = vocbase.createCollection(createCollection0->slice());
  ASSERT_NE(nullptr, collection0);
  auto collection1 = vocbase.createCollection(createCollection1->slice());
  ASSERT_NE(nullptr, collection1);
  auto viewImpl = vocbase.createView(createView->slice(), false);
  ASSERT_NE(nullptr, viewImpl);

  // populate collections
  {
    auto doc0 = arangodb::velocypack::Parser::fromJson(
        "{ \"seq\": 0, \"X\": \"abc\", \"Y\": \"def\" }");
    auto doc1 = arangodb::velocypack::Parser::fromJson(
        "{ \"seq\": 1, \"X\": \"abc\", \"Y\": \"def\" }");
    static std::vector<std::string> const EMPTY;
    std::vector<std::string> collections{collection0->name(),
                                         collection1->name()};
    arangodb::transaction::Methods trx(
        arangodb::transaction::StandaloneContext::create(
            vocbase, arangodb::transaction::OperationOriginTestCase{}),
        EMPTY, collections, EMPTY, arangodb::transaction::Options());
    EXPECT_TRUE(trx.begin().ok());
    EXPECT_TRUE(trx.insert(collection0->name(), doc0->slice(),
                           arangodb::OperationOptions())
                    .ok());
    EXPECT_TRUE(trx.insert(collection1->name(), doc1->slice(),
                           arangodb::OperationOptions())
                    .ok());
    EXPECT_TRUE(trx.commit().ok());
  }

  // link collections with view
  {
    auto updateJson = arangodb::velocypack::Parser::fromJson(
        "{\"links\": { \
      \"testCollection0\": { \"fields\": { \
        \"X\": { }, \
        \"Y\": { } \
      } }, \
      \"testCollection1\": { \"fields\": { \
        \"X\": { } \
      } } \
    } }");

    EXPECT_TRUE(viewImpl->properties(updateJson->slice(), true, false).ok());
  }
  // running query to force sync
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH d.X == 'abc' OPTIONS { waitForSync: true } "
        "SORT d.seq RETURN d",
        nullptr);
    ASSERT_TRUE(result.result.ok());
  }
  ASSERT_GT(feature.columnsCacheUsage(), 0);
}

TEST_F(IResearchIndexTest, test_sortCachedInverted) {
  auto createCollection0 = arangodb::velocypack::Parser::fromJson(
      "{ \"name\": \"testCollection0\" }");
  TRI_vocbase_t vocbase(testDBInfo(server.server()));
  auto& feature = server.getFeature<arangodb::iresearch::IResearchFeature>();
  feature.setCacheUsageLimit(10000000);
  auto collection0 = vocbase.createCollection(createCollection0->slice());
  ASSERT_NE(nullptr, collection0);

  {
    auto doc0 = arangodb::velocypack::Parser::fromJson(
        "{ \"seq\": 0, \"X\": \"abc\", \"Y\": \"def\" }");
    auto doc1 = arangodb::velocypack::Parser::fromJson(
        "{ \"seq\": 1, \"X\": \"abc\", \"Y\": \"def\" }");
    static std::vector<std::string> const EMPTY;
    std::vector<std::string> collections{collection0->name()};
    arangodb::transaction::Methods trx(
        arangodb::transaction::StandaloneContext::create(
            vocbase, arangodb::transaction::OperationOriginTestCase{}),
        EMPTY, collections, EMPTY, arangodb::transaction::Options());
    EXPECT_TRUE(trx.begin().ok());
    EXPECT_TRUE(trx.insert(collection0->name(), doc0->slice(),
                           arangodb::OperationOptions())
                    .ok());
    EXPECT_TRUE(trx.insert(collection0->name(), doc1->slice(),
                           arangodb::OperationOptions())
                    .ok());
    EXPECT_TRUE(trx.commit().ok());
  }

  // link collections with view
  {
    auto updateJson = arangodb::velocypack::Parser::fromJson(R"({
      "name": "inverted",
      "type": "inverted",
      "primaryKeyCache": false,
      "primarySort": {"cache":true, "fields":[{"field": "X", "direction": "asc"}]},
      "fields":  ["X", "Y"]})");

    bool created{false};
    ASSERT_TRUE(
        collection0->createIndex(updateJson->slice(), created).waitAndGet());
    ASSERT_TRUE(created);
  }
  // running query to force sync
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testCollection0 OPTIONS { waitForSync: true, indexHint: "
        "'inverted', forceIndexHint: true} FILTER d.X == 'abc' "
        "SORT d.seq RETURN d",
        nullptr);
    ASSERT_TRUE(result.result.ok());
  }
  ASSERT_GT(feature.columnsCacheUsage(), 0);
}

TEST_F(IResearchIndexTest, test_sortCachedRestricted) {
  auto createCollection0 = arangodb::velocypack::Parser::fromJson(
      "{ \"name\": \"testCollection0\" }");
  auto createCollection1 = arangodb::velocypack::Parser::fromJson(
      "{ \"name\": \"testCollection1\" }");
  auto createView = arangodb::velocypack::Parser::fromJson(
      "{ \"name\": \"testView\", \"type\": \"arangosearch\",\
        \"primarySortCache\":true,\
        \"primarySort\":[{\"field\":\"X\", \"direction\":\"asc\" }]}");
  auto& feature = server.getFeature<arangodb::iresearch::IResearchFeature>();
  feature.setCacheUsageLimit(10);
  TRI_vocbase_t vocbase(testDBInfo(server.server()));
  auto collection0 = vocbase.createCollection(createCollection0->slice());
  ASSERT_NE(nullptr, collection0);
  auto collection1 = vocbase.createCollection(createCollection1->slice());
  ASSERT_NE(nullptr, collection1);
  auto viewImpl = vocbase.createView(createView->slice(), false);
  ASSERT_NE(nullptr, viewImpl);

  // populate collections
  {
    auto doc0 = arangodb::velocypack::Parser::fromJson(
        "{ \"seq\": 0, \"X\": \"abc\", \"Y\": \"def\" }");
    auto doc1 = arangodb::velocypack::Parser::fromJson(
        "{ \"seq\": 1, \"X\": \"abc\", \"Y\": \"def\" }");
    static std::vector<std::string> const EMPTY;
    std::vector<std::string> collections{collection0->name(),
                                         collection1->name()};
    arangodb::transaction::Methods trx(
        arangodb::transaction::StandaloneContext::create(
            vocbase, arangodb::transaction::OperationOriginTestCase{}),
        EMPTY, collections, EMPTY, arangodb::transaction::Options());
    EXPECT_TRUE(trx.begin().ok());
    EXPECT_TRUE(trx.insert(collection0->name(), doc0->slice(),
                           arangodb::OperationOptions())
                    .ok());
    EXPECT_TRUE(trx.insert(collection1->name(), doc1->slice(),
                           arangodb::OperationOptions())
                    .ok());
    EXPECT_TRUE(trx.commit().ok());
  }

  // link collections with view
  {
    auto updateJson = arangodb::velocypack::Parser::fromJson(
        "{\"links\": { \
      \"testCollection0\": { \"fields\": { \
        \"X\": { }, \
        \"Y\": { } \
      } }, \
      \"testCollection1\": { \"fields\": { \
        \"X\": { } \
      } } \
    } }");

    EXPECT_TRUE(viewImpl->properties(updateJson->slice(), true, false).ok());
  }
  // running query to force sync
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH d.X == 'abc' OPTIONS { waitForSync: true } "
        "SORT d.seq RETURN d",
        nullptr);
    ASSERT_TRUE(result.result.ok());
  }
  ASSERT_GT(feature.columnsCacheUsage(), 0);
  ASSERT_LE(feature.columnsCacheUsage(), 10);
}

TEST_F(IResearchIndexTest, test_geoCached) {
  auto createCollection0 = arangodb::velocypack::Parser::fromJson(
      "{ \"name\": \"testCollection0\" }");
  auto createView = arangodb::velocypack::Parser::fromJson(
      "{ \"name\": \"testView\", \"type\": \"arangosearch\", "
      "\"primaryKeyCache\":false }");
  TRI_vocbase_t vocbase(testDBInfo(server.server()));
  auto& feature = server.getFeature<arangodb::iresearch::IResearchFeature>();
  feature.setCacheUsageLimit(10000000);
  auto collection0 = vocbase.createCollection(createCollection0->slice());
  ASSERT_NE(nullptr, collection0);
  auto viewImpl = vocbase.createView(createView->slice(), true);
  ASSERT_NE(nullptr, viewImpl);

  // populate collections
  {
    auto doc0 = arangodb::velocypack::Parser::fromJson(
        "{\
       \"geofield\" : {\
        \"type\" : \"Polygon\",\
        \"coordinates\" : [[\
          [ 100.318391, 13.535502 ], [ 100.318391, 14.214848 ],\
          [ 101.407575, 14.214848 ], [ 101.407575, 13.535502 ],\
          [ 100.318391, 13.535502 ]\
        ]]\
      }\
    }");
    static std::vector<std::string> const EMPTY;
    std::vector<std::string> collections{collection0->name()};
    arangodb::transaction::Methods trx(
        arangodb::transaction::StandaloneContext::create(
            vocbase, arangodb::transaction::OperationOriginTestCase{}),
        EMPTY, collections, EMPTY, arangodb::transaction::Options());
    EXPECT_TRUE(trx.begin().ok());
    EXPECT_TRUE(trx.insert(collection0->name(), doc0->slice(),
                           arangodb::OperationOptions())
                    .ok());
    EXPECT_TRUE(trx.commit().ok());
  }

  // link collection with view
  {
    auto updateJson = arangodb::velocypack::Parser::fromJson(
        "{\"links\": { \
      \"testCollection0\": { \"fields\": { \
        \"geofield\": { \"cache\":true, \"analyzers\":[\"geojson\"] }, \
        \"Y\": { } \
      } }\
    } }");

    EXPECT_TRUE(viewImpl->properties(updateJson->slice(), true, false).ok());
  }
  // running query to force sync
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testView SEARCH d.X == 'abc' OPTIONS { waitForSync: true } "
        "SORT d.seq RETURN d",
        nullptr);
    ASSERT_TRUE(result.result.ok());
  }
  ASSERT_GT(feature.columnsCacheUsage(), 0);
}

TEST_F(IResearchIndexTest, test_geoCachedInverted) {
  auto createCollection0 = arangodb::velocypack::Parser::fromJson(
      "{ \"name\": \"testCollection0\" }");
  auto createView = arangodb::velocypack::Parser::fromJson(
      "{ \"name\": \"testView\", \"type\": \"arangosearch\", "
      "\"primaryKeyCache\":false }");
  TRI_vocbase_t vocbase(testDBInfo(server.server()));
  auto& feature = server.getFeature<arangodb::iresearch::IResearchFeature>();
  feature.setCacheUsageLimit(10000000);
  auto collection0 = vocbase.createCollection(createCollection0->slice());
  ASSERT_NE(nullptr, collection0);
  auto viewImpl = vocbase.createView(createView->slice(), true);
  ASSERT_NE(nullptr, viewImpl);

  // populate collections
  {
    auto doc0 = arangodb::velocypack::Parser::fromJson(
        "{\
       \"geofield\" : {\
        \"type\" : \"Polygon\",\
        \"coordinates\" : [[\
          [ 100.318391, 13.535502 ], [ 100.318391, 14.214848 ],\
          [ 101.407575, 14.214848 ], [ 101.407575, 13.535502 ],\
          [ 100.318391, 13.535502 ]\
        ]]\
      }\
    }");
    static std::vector<std::string> const EMPTY;
    std::vector<std::string> collections{collection0->name()};
    arangodb::transaction::Methods trx(
        arangodb::transaction::StandaloneContext::create(
            vocbase, arangodb::transaction::OperationOriginTestCase{}),
        EMPTY, collections, EMPTY, arangodb::transaction::Options());
    EXPECT_TRUE(trx.begin().ok());
    EXPECT_TRUE(trx.insert(collection0->name(), doc0->slice(),
                           arangodb::OperationOptions())
                    .ok());
    EXPECT_TRUE(trx.commit().ok());
  }

  {
    auto updateJson = arangodb::velocypack::Parser::fromJson(R"({
      "name": "inverted",
      "type": "inverted",
      "primaryKeyCache": false,
      "fields": ["X", {"name":"geofield", "cache":true, "analyzer":"geojson"}]})");

    bool created{false};
    ASSERT_TRUE(
        collection0->createIndex(updateJson->slice(), created).waitAndGet());
    ASSERT_TRUE(created);
  }
  // running query to force sync
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN testCollection0 OPTIONS { waitForSync: true, indexHint: "
        "'inverted', forceIndexHint: true} FILTER d.X == 'abc' "
        "SORT d.seq RETURN d",
        nullptr);
    ASSERT_TRUE(result.result.ok());
  }
  ASSERT_GT(feature.columnsCacheUsage(), 0);
}

class IResearchCacheOnlyFollowersTest : public ::testing::Test {
 protected:
  arangodb::tests::mocks::MockDBServer server;

  IResearchCacheOnlyFollowersTest() : server("PRMR_0001") {
    auto& feature = server.getFeature<arangodb::iresearch::IResearchFeature>();
    feature.setColumnsCacheOnlyOnLeader(true);
    feature.setCacheUsageLimit(10000000);
  }
};

TEST_F(IResearchCacheOnlyFollowersTest, test_PkInverted) {
  auto createCollection0 = arangodb::velocypack::Parser::fromJson(
      "{\"id\":1, \"name\": \"s1337\" }");
  TRI_vocbase_t vocbase(testDBInfo(server.server()));
  auto& feature = server.getFeature<arangodb::iresearch::IResearchFeature>();
  auto collection0 = vocbase.createCollection(createCollection0->slice());
  ASSERT_NE(nullptr, collection0);
  std::vector<std::pair<std::string, std::string>> shards{
      {collection0->name(), "PRMR_0002"}};
  server.createDatabase(vocbase.name());
  server.createCollection(vocbase.name(), collection0->name(), shards,
                          TRI_COL_TYPE_DOCUMENT);

  {
    auto doc0 = arangodb::velocypack::Parser::fromJson(
        "{ \"seq\": 0, \"X\": \"abc\", \"Y\": \"def\" }");
    auto doc1 = arangodb::velocypack::Parser::fromJson(
        "{ \"seq\": 1, \"X\": \"abc\", \"Y\": \"def\" }");
    static std::vector<std::string> const EMPTY;
    std::vector<std::string> collections{collection0->name()};
    arangodb::transaction::Methods trx(
        arangodb::transaction::StandaloneContext::create(
            vocbase, arangodb::transaction::OperationOriginTestCase{}),
        EMPTY, collections, EMPTY, arangodb::transaction::Options());
    EXPECT_TRUE(trx.begin().ok());
    EXPECT_TRUE(trx.insert(collection0->name(), doc0->slice(),
                           arangodb::OperationOptions())
                    .ok());
    EXPECT_TRUE(trx.insert(collection0->name(), doc1->slice(),
                           arangodb::OperationOptions())
                    .ok());
    EXPECT_TRUE(trx.commit().ok());
  }

  // link collections with view
  {
    auto updateJson = arangodb::velocypack::Parser::fromJson(R"({
      "name": "inverted",
      "type": "inverted",
      "primaryKeyCache": true,
      "fields":  ["X", "Y"]})");

    bool created{false};
    ASSERT_TRUE(
        collection0->createIndex(updateJson->slice(), created).waitAndGet());
    ASSERT_TRUE(created);
  }
  // running query to force sync
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN s1337 OPTIONS { waitForSync: true, indexHint: "
        "'inverted', forceIndexHint: true} FILTER d.X == 'abc' "
        "SORT d.seq RETURN d",
        nullptr);
    ASSERT_TRUE(result.result.ok()) << result.result.errorMessage();
  }
  ASSERT_EQ(feature.columnsCacheUsage(), 0);
  // now make it leader (just recreate collection in plan with new shards
  // layout)
  std::vector<std::pair<std::string, std::string>> shards2{
      {collection0->name(), "PRMR_0001"}};
  server.createCollection(vocbase.name(), collection0->name(), shards2,
                          TRI_COL_TYPE_DOCUMENT);
  // running query to force sync
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN s1337 OPTIONS { waitForSync: true, indexHint: "
        "'inverted', forceIndexHint: true} FILTER d.X == 'abc' "
        "SORT d.seq RETURN d",
        nullptr);
    ASSERT_TRUE(result.result.ok());
  }
  ASSERT_GT(feature.columnsCacheUsage(), 0);
}

TEST_F(IResearchCacheOnlyFollowersTest, test_PkInverted_InitialLeader) {
  auto createCollection0 = arangodb::velocypack::Parser::fromJson(
      "{\"id\":1, \"name\": \"s1337\" }");
  TRI_vocbase_t vocbase(testDBInfo(server.server()));
  auto& feature = server.getFeature<arangodb::iresearch::IResearchFeature>();
  auto collection0 = vocbase.createCollection(createCollection0->slice());
  ASSERT_NE(nullptr, collection0);
  std::vector<std::pair<std::string, std::string>> shards{
      {collection0->name(), "PRMR_0001"}};
  server.createDatabase(vocbase.name());
  server.createCollection(vocbase.name(), collection0->name(), shards,
                          TRI_COL_TYPE_DOCUMENT);

  {
    auto doc0 = arangodb::velocypack::Parser::fromJson(
        "{ \"seq\": 0, \"X\": \"abc\", \"Y\": \"def\" }");
    auto doc1 = arangodb::velocypack::Parser::fromJson(
        "{ \"seq\": 1, \"X\": \"abc\", \"Y\": \"def\" }");
    static std::vector<std::string> const EMPTY;
    std::vector<std::string> collections{collection0->name()};
    arangodb::transaction::Methods trx(
        arangodb::transaction::StandaloneContext::create(
            vocbase, arangodb::transaction::OperationOriginTestCase{}),
        EMPTY, collections, EMPTY, arangodb::transaction::Options());
    EXPECT_TRUE(trx.begin().ok());
    EXPECT_TRUE(trx.insert(collection0->name(), doc0->slice(),
                           arangodb::OperationOptions())
                    .ok());
    EXPECT_TRUE(trx.insert(collection0->name(), doc1->slice(),
                           arangodb::OperationOptions())
                    .ok());
    EXPECT_TRUE(trx.commit().ok());
  }

  // link collections with view
  {
    auto updateJson = arangodb::velocypack::Parser::fromJson(R"({
      "name": "inverted",
      "type": "inverted",
      "primaryKeyCache": true,
      "fields":  ["X", "Y"]})");

    bool created{false};
    ASSERT_TRUE(
        collection0->createIndex(updateJson->slice(), created).waitAndGet());
    ASSERT_TRUE(created);
  }
  // running query to force sync
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN s1337 OPTIONS { waitForSync: true, indexHint: "
        "'inverted', forceIndexHint: true} FILTER d.X == 'abc' "
        "SORT d.seq RETURN d",
        nullptr);
    ASSERT_TRUE(result.result.ok());
  }
  ASSERT_GT(feature.columnsCacheUsage(), 0);
  // now make it follower (just recreate collection in plan with new shards
  // layout)
  std::vector<std::pair<std::string, std::string>> shards2{
      {collection0->name(), "PRMR_0002"}};
  server.createCollection(vocbase.name(), collection0->name(), shards2,
                          TRI_COL_TYPE_DOCUMENT);
  // running query to force sync
  {
    auto result = arangodb::tests::executeQuery(
        vocbase,
        "FOR d IN s1337 OPTIONS { waitForSync: true, indexHint: "
        "'inverted', forceIndexHint: true} FILTER d.X == 'abc' "
        "SORT d.seq RETURN d",
        nullptr);
    ASSERT_TRUE(result.result.ok());
  }
  ASSERT_EQ(feature.columnsCacheUsage(), 0);
}

#endif
