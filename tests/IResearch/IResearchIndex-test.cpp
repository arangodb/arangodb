////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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

#include "catch.hpp"
#include "common.h"

#include "StorageEngineMock.h"

#include "3rdParty/iresearch/tests/tests_config.hpp"
#include "analysis/analyzers.hpp"
#include "analysis/token_attributes.hpp"
#include "Aql/AqlFunctionFeature.h"
#include "Aql/OptimizerRulesFeature.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/files.h"
#include "Sharding/ShardingFeature.h"
#include "IResearch/IResearchAnalyzerFeature.h"
#include "IResearch/IResearchCommon.h"
#include "IResearch/IResearchFeature.h"
#include "Logger/Logger.h"
#include "RestServer/AqlFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/DatabasePathFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "RestServer/SystemDatabaseFeature.h"
#include "RestServer/TraverserEngineRegistryFeature.h"
#include "RestServer/ViewTypesFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/OperationOptions.h"
#include "Utils/SingleCollectionTransaction.h"
#include "utils/utf8_path.hpp"
#include "velocypack/Iterator.h"
#include "velocypack/Parser.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/LogicalView.h"

NS_LOCAL

struct TestAttributeX: public irs::attribute {
  DECLARE_ATTRIBUTE_TYPE();
};

DEFINE_ATTRIBUTE_TYPE(TestAttributeX);
REGISTER_ATTRIBUTE(TestAttributeX); // required to open reader on segments with analized fields

struct TestAttributeY: public irs::attribute {
  DECLARE_ATTRIBUTE_TYPE();
};

DEFINE_ATTRIBUTE_TYPE(TestAttributeY);
REGISTER_ATTRIBUTE(TestAttributeY); // required to open reader on segments with analized fields

struct TestTermAttribute: public irs::term_attribute {
 public:
  void value(irs::bytes_ref const& value) { value_ = value; }
  irs::bytes_ref const& value() const { return value_; }
};

class TestAnalyzer: public irs::analysis::analyzer {
 public:
  DECLARE_ANALYZER_TYPE();

  static ptr make(irs::string_ref const& args) {
    PTR_NAMED(TestAnalyzer, ptr, args);
    return ptr;
  }

  TestAnalyzer(irs::string_ref const& value)
    : irs::analysis::analyzer(TestAnalyzer::type()) {
    _attrs.emplace(_inc); // required by field_data::invert(...)
    _attrs.emplace(_term);

    if (value == "X") {
      _attrs.emplace(_x);
    } else if (value == "Y") {
      _attrs.emplace(_y);
    }
  }

  virtual irs::attribute_view const& attributes() const NOEXCEPT override { return _attrs; }

  virtual bool next() override {
    _term.value(_data);
    _data = irs::bytes_ref::NIL;

    return !_term.value().null();
  }

  virtual bool reset(irs::string_ref const& data) override {
    _data = irs::ref_cast<irs::byte_type>(data);
    _term.value(irs::bytes_ref::NIL);

    return true;
  }

 private:
  irs::attribute_view _attrs;
  irs::bytes_ref _data;
  irs::increment _inc;
  TestTermAttribute _term;
  TestAttributeX _x;
  TestAttributeY _y;
};

DEFINE_ANALYZER_TYPE_NAMED(TestAnalyzer, "TestInsertAnalyzer");
REGISTER_ANALYZER_JSON(TestAnalyzer, TestAnalyzer::make);

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

struct IResearchIndexSetup {
  StorageEngineMock engine;
  arangodb::application_features::ApplicationServer server;
  std::unique_ptr<TRI_vocbase_t> system;
  std::vector<std::pair<arangodb::application_features::ApplicationFeature*, bool>> features;

  IResearchIndexSetup(): engine(server), server(nullptr, nullptr) {
    arangodb::EngineSelectorFeature::ENGINE = &engine;

    arangodb::tests::init(true);

    // suppress INFO {authentication} Authentication is turned on (system only), authentication for unix sockets is turned on
    arangodb::LogTopic::setLogLevel(arangodb::Logger::AUTHENTICATION.name(), arangodb::LogLevel::WARN);

    // suppress log messages since tests check error conditions
    arangodb::LogTopic::setLogLevel(arangodb::iresearch::TOPIC.name(), arangodb::LogLevel::FATAL);
    irs::logger::output_le(iresearch::logger::IRL_FATAL, stderr);

    // setup required application features
    features.emplace_back(new arangodb::AqlFeature(server), true); // required for arangodb::aql::Query(...)
    features.emplace_back(new arangodb::DatabaseFeature(server), false); // required for LogicalViewStorageEngine::modify(...)
    features.emplace_back(new arangodb::DatabasePathFeature(server), false); // requires for IResearchView::open()
    features.emplace_back(new arangodb::ShardingFeature(server), false);
    features.emplace_back(new arangodb::ViewTypesFeature(server), true); // required by TRI_vocbase_t::createView(...)
    features.emplace_back(new arangodb::QueryRegistryFeature(server), false); // required by TRI_vocbase_t(...)
    arangodb::application_features::ApplicationServer::server->addFeature(features.back().first); // need QueryRegistryFeature feature to be added now in order to create the system database
    system = irs::memory::make_unique<TRI_vocbase_t>(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 0, TRI_VOC_SYSTEM_DATABASE);
    features.emplace_back(new arangodb::SystemDatabaseFeature(server, system.get()), false); // required for IResearchAnalyzerFeature
    features.emplace_back(new arangodb::TraverserEngineRegistryFeature(server), false); // required for AQLFeature
    features.emplace_back(new arangodb::aql::AqlFunctionFeature(server), true); // required for IResearchAnalyzerFeature
    features.emplace_back(new arangodb::aql::OptimizerRulesFeature(server), true); // required for arangodb::aql::Query::execute(...)
    features.emplace_back(new arangodb::iresearch::IResearchAnalyzerFeature(server), true); // required for use of iresearch analyzers
    features.emplace_back(new arangodb::iresearch::IResearchFeature(server), true); // required for creating views of type 'iresearch'

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

    auto* analyzers = arangodb::application_features::ApplicationServer::lookupFeature<
      arangodb::iresearch::IResearchAnalyzerFeature
    >();

    analyzers->emplace("test_A", "TestInsertAnalyzer", "X"); // cache analyzer
    analyzers->emplace("test_B", "TestInsertAnalyzer", "Y"); // cache analyzer

    auto* dbPathFeature = arangodb::application_features::ApplicationServer::getFeature<arangodb::DatabasePathFeature>("DatabasePath");
    arangodb::tests::setDatabasePath(*dbPathFeature); // ensure test data is stored in a unique directory
  }

  ~IResearchIndexSetup() {
    system.reset(); // destroy before reseting the 'ENGINE'
    arangodb::LogTopic::setLogLevel(arangodb::iresearch::TOPIC.name(), arangodb::LogLevel::DEFAULT);
    arangodb::application_features::ApplicationServer::server = nullptr;
    arangodb::EngineSelectorFeature::ENGINE = nullptr;

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
  }
};

NS_END

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief setup
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("IResearchIndexTestValue", "[iresearch][iresearch-index]") {
  IResearchIndexSetup s;
  UNUSED(s);

// test indexing with multiple analyzers (on different collections) will return results only for matching analyzer
SECTION("test_analyzer") {
  auto createCollection0 = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection0\" }");
  auto createCollection1 = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection1\" }");
  auto createView = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
  auto collection0 = vocbase.createCollection(createCollection0->slice());
  REQUIRE((nullptr != collection0));
  auto collection1 = vocbase.createCollection(createCollection1->slice());
  REQUIRE((nullptr != collection1));
  auto viewImpl = vocbase.createView(createView->slice());
  REQUIRE((nullptr != viewImpl));

  // populate collections
  {
    auto doc0 = arangodb::velocypack::Parser::fromJson("{ \"seq\": 0, \"X\": \"abc\", \"Y\": \"def\" }");
    auto doc1 = arangodb::velocypack::Parser::fromJson("{ \"seq\": 1, \"X\": \"abc\", \"Y\": \"def\" }");
    static std::vector<std::string> const EMPTY;
    std::vector<std::string> collections{ collection0->name(), collection1->name() };
    arangodb::transaction::Methods trx(
      arangodb::transaction::StandaloneContext::Create(vocbase),
      EMPTY,
      collections,
      EMPTY,
      arangodb::transaction::Options()
    );
    CHECK((trx.begin().ok()));
    CHECK((trx.insert(collection0->name(), doc0->slice(), arangodb::OperationOptions()).ok()));
    CHECK((trx.insert(collection1->name(), doc1->slice(), arangodb::OperationOptions()).ok()));
    CHECK((trx.commit().ok()));
  }

  // link collections with view
  {
    auto updateJson = arangodb::velocypack::Parser::fromJson("{ \"links\": { \
      \"testCollection0\": { \"fields\": { \
        \"X\": { \"analyzers\": [ \"test_A\", \"test_B\" ] }, \
        \"Y\": { \"analyzers\": [ \"test_B\" ] } \
      } }, \
      \"testCollection1\": { \"fields\": { \
        \"X\": { \"analyzers\": [ \"test_A\" ] }, \
        \"Y\": { \"analyzers\": [ \"test_A\" ] } \
      } } \
    } }");

    CHECK((viewImpl->properties(updateJson->slice(), false).ok()));
  }

  // docs match from both collections (2 analyzers used for collection0, 1 analyzer used for collection 1)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH ANALYZER(PHRASE(d.X, 'abc', 'test_A'), 'test_B') OPTIONS { waitForSync: true } SORT d.seq RETURN d",
      nullptr
    );
    REQUIRE(TRI_ERROR_NO_ERROR == result.code);
    auto slice = result.result->slice();
    CHECK(slice.isArray());
    std::vector<size_t> expected { 0, 1 };
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      auto key = resolved.get("seq");
      CHECK((i < expected.size()));
      CHECK((expected[i++] == key.getNumber<size_t>()));
    }

    CHECK((i == expected.size()));
  }

  // docs match from both collections (2 analyzers used for collection0, 1 analyzer used for collection 1)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d.X, 'abc', 'test_A') OPTIONS { waitForSync: true } SORT d.seq RETURN d",
      nullptr
    );
    REQUIRE(TRI_ERROR_NO_ERROR == result.code);
    auto slice = result.result->slice();
    CHECK(slice.isArray());
    std::vector<size_t> expected { 0, 1 };
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      auto key = resolved.get("seq");
      CHECK((i < expected.size()));
      CHECK((expected[i++] == key.getNumber<size_t>()));
    }

    CHECK((i == expected.size()));
  }

    // docs match from both collections (2 analyzers used for collection0, 1 analyzer used for collection 1)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH ANALYZER(PHRASE(d.X, 'abc'), 'test_A') OPTIONS { waitForSync: true } SORT d.seq RETURN d",
      nullptr
    );
    REQUIRE(TRI_ERROR_NO_ERROR == result.code);
    auto slice = result.result->slice();
    CHECK(slice.isArray());
    std::vector<size_t> expected { 0, 1 };
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      auto key = resolved.get("seq");
      CHECK((i < expected.size()));
      CHECK((expected[i++] == key.getNumber<size_t>()));
    }

    CHECK((i == expected.size()));
  }

  // docs match from collection0 (2 analyzers used)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d.X, 'abc', 'test_B') SORT d.seq RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == result.code);
    auto slice = result.result->slice();
    CHECK(slice.isArray());
    std::vector<size_t> expected { 0 };
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      auto key = resolved.get("seq");
      CHECK((i < expected.size()));
      CHECK((expected[i++] == key.getNumber<size_t>()));
    }

    CHECK((i == expected.size()));
  }

  // docs match from collection0 (2 analyzers used)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH analyzer(PHRASE(d.X, 'abc'), 'test_B') SORT d.seq RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == result.code);
    auto slice = result.result->slice();
    CHECK(slice.isArray());
    std::vector<size_t> expected { 0 };
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      auto key = resolved.get("seq");
      CHECK((i < expected.size()));
      CHECK((expected[i++] == key.getNumber<size_t>()));
    }

    CHECK((i == expected.size()));
  }

  // docs match from collection1 (1 analyzer used)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d.Y, 'def', 'test_A') SORT d.seq RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == result.code);
    auto slice = result.result->slice();
    CHECK(slice.isArray());
    std::vector<size_t> expected { 1 };
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      auto key = resolved.get("seq");
      CHECK((i < expected.size()));
      CHECK((expected[i++] == key.getNumber<size_t>()));
    }

    CHECK((i == expected.size()));
  }

  // docs match from collection1 (1 analyzer used)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH ANALYZER(PHRASE(d.Y, 'def', 'test_A'), 'test_B') SORT d.seq RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == result.code);
    auto slice = result.result->slice();
    CHECK(slice.isArray());
    std::vector<size_t> expected { 1 };
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      auto key = resolved.get("seq");
      CHECK((i < expected.size()));
      CHECK((expected[i++] == key.getNumber<size_t>()));
    }

    CHECK((i == expected.size()));
  }

  // docs match from collection1 (1 analyzer used)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d.Y, 'def', 'test_A') SORT d.seq RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == result.code);
    auto slice = result.result->slice();
    CHECK(slice.isArray());
    std::vector<size_t> expected { 1 };
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      auto key = resolved.get("seq");
      CHECK((i < expected.size()));
      CHECK((expected[i++] == key.getNumber<size_t>()));
    }

    CHECK((i == expected.size()));
  }

  // docs match from collection1 (1 analyzer used)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH ANALYZER(PHRASE(d.Y, 'def'), 'test_A') SORT d.seq RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == result.code);
    auto slice = result.result->slice();
    CHECK(slice.isArray());
    std::vector<size_t> expected { 1 };
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      auto key = resolved.get("seq");
      CHECK((i < expected.size()));
      CHECK((expected[i++] == key.getNumber<size_t>()));
    }

    CHECK((i == expected.size()));
  }
}

// test concurrent indexing with analyzers into view
SECTION("test_async_index") {
  auto createCollection0 = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection0\" }");
  auto createCollection1 = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection1\" }");
  auto createView = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
  auto collection0 = vocbase.createCollection(createCollection0->slice());
  REQUIRE((nullptr != collection0));
  auto collection1 = vocbase.createCollection(createCollection1->slice());
  REQUIRE((nullptr != collection1));
  auto viewImpl = vocbase.createView(createView->slice());
  REQUIRE((nullptr != viewImpl));

  // link collections with view
  {
    auto updateJson = arangodb::velocypack::Parser::fromJson("{ \"links\": { \
      \"testCollection0\": { \"fields\": { \
        \"same\": { \"analyzers\": [ \"test_A\", \"test_B\" ] }, \
        \"duplicated\": { \"analyzers\": [ \"test_B\" ] } \
      } }, \
      \"testCollection1\": { \"fields\": { \
        \"same\": { \"analyzers\": [ \"test_A\" ] }, \
        \"duplicated\": { \"analyzers\": [ \"test_A\" ] } \
      } } \
    } }");

    CHECK((viewImpl->properties(updateJson->slice(), false).ok()));
  }

  // `catch` doesn't support cuncurrent checks
  bool resThread0 = false;
  bool resThread1 = false;

  // populate collections asynchronously
  {
    std::thread thread0([collection0, &resThread0]()->void {
      arangodb::velocypack::Builder builder;

      try {
        irs::utf8_path resource;

        resource/=irs::string_ref(IResearch_test_resource_dir);
        resource/=irs::string_ref("simple_sequential.json");
        builder = arangodb::basics::VelocyPackHelper::velocyPackFromFile(resource.utf8());
      } catch (...) {
        return; // velocyPackFromFile(...) may throw exception
      }

      auto doc = arangodb::velocypack::Parser::fromJson("{ \"seq\": 40, \"same\": \"xyz\", \"duplicated\": \"abcd\" }");
      auto slice = builder.slice();
      resThread0 = slice.isArray();
      if (!resThread0) return;

      arangodb::SingleCollectionTransaction trx(
        arangodb::transaction::StandaloneContext::Create(collection0->vocbase()),
        *collection0,
        arangodb::AccessMode::Type::WRITE
      );
      resThread0 = trx.begin().ok();
      if (!resThread0) return;

      resThread0 = trx.insert(collection0->name(), doc->slice(), arangodb::OperationOptions()).ok();
      if (!resThread0) return;

      for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto res = trx.insert(collection0->name(), itr.value(), arangodb::OperationOptions());
        resThread0 = res.ok();
        if (!resThread0) return;
      }

      resThread0 = trx.commit().ok();
    });

    std::thread thread1([collection1, &resThread1]()->void {
      arangodb::velocypack::Builder builder;

      try {
        irs::utf8_path resource;

        resource/=irs::string_ref(IResearch_test_resource_dir);
        resource/=irs::string_ref("simple_sequential.json");
        builder = arangodb::basics::VelocyPackHelper::velocyPackFromFile(resource.utf8());
      } catch (...) {
        return; // velocyPackFromFile(...) may throw exception
      }

      auto doc = arangodb::velocypack::Parser::fromJson("{ \"seq\": 50, \"same\": \"xyz\", \"duplicated\": \"abcd\" }");
      auto slice = builder.slice();
      resThread1 = slice.isArray();
      if (!resThread1) return;

      arangodb::SingleCollectionTransaction trx(
        arangodb::transaction::StandaloneContext::Create(collection1->vocbase()),
        *collection1,
        arangodb::AccessMode::Type::WRITE
      );
      resThread1 = trx.begin().ok();
      if (!resThread1) return;

      resThread1 = trx.insert(collection1->name(), doc->slice(), arangodb::OperationOptions()).ok();
      if (!resThread1) return;

      for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
        auto res = trx.insert(collection1->name(), itr.value(), arangodb::OperationOptions());
        resThread1 = res.ok();
        if (!resThread1) return;
      }

      resThread1 = trx.commit().ok();
    });

    thread0.join();
    thread1.join();
  }

  CHECK(resThread0);
  CHECK(resThread1);

  // docs match from both collections (2 analyzers used for collectio0, 1 analyzer used for collection 1)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH ANALYZER(PHRASE(d.same, 'xyz', 'test_A'), 'test_B') OPTIONS { waitForSync: true } SORT d.seq RETURN d",
      nullptr
    );
    REQUIRE(TRI_ERROR_NO_ERROR == result.code);
    auto slice = result.result->slice();
    CHECK(slice.isArray());
    std::vector<size_t> expected {
      0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9,
      10, 10, 11, 11, 12, 12, 13, 13, 14, 14, 15, 15, 16, 16, 17, 17, 18, 18, 19, 19,
      20, 20, 21, 21, 22, 22, 23, 23, 24, 24, 25, 25, 26, 26, 27, 27, 28, 28, 29, 29,
      30, 30, 31, 31, 40, 50
    };
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      auto key = resolved.get("seq");
      CHECK((i < expected.size()));
      CHECK((expected[i++] == key.getNumber<size_t>()));
    }

    CHECK((i == expected.size()));
  }

  // docs match from both collections (2 analyzers used for collectio0, 1 analyzer used for collection 1)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d.same, 'xyz', 'test_A') OPTIONS { waitForSync : true } SORT d.seq RETURN d",
      nullptr
    );
    REQUIRE(TRI_ERROR_NO_ERROR == result.code);
    auto slice = result.result->slice();
    CHECK(slice.isArray());
    std::vector<size_t> expected {
      0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9,
      10, 10, 11, 11, 12, 12, 13, 13, 14, 14, 15, 15, 16, 16, 17, 17, 18, 18, 19, 19,
      20, 20, 21, 21, 22, 22, 23, 23, 24, 24, 25, 25, 26, 26, 27, 27, 28, 28, 29, 29,
      30, 30, 31, 31, 40, 50
    };
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      auto key = resolved.get("seq");
      CHECK((i < expected.size()));
      CHECK((expected[i++] == key.getNumber<size_t>()));
    }

    CHECK((i == expected.size()));
  }

  // docs match from both collections (2 analyzers used for collectio0, 1 analyzer used for collection 1)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH ANALYZER(PHRASE(d.same, 'xyz'), 'test_A') OPTIONS { waitForSync : true } SORT d.seq RETURN d",
      nullptr
    );
    REQUIRE(TRI_ERROR_NO_ERROR == result.code);
    auto slice = result.result->slice();
    CHECK(slice.isArray());
    std::vector<size_t> expected {
      0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9,
      10, 10, 11, 11, 12, 12, 13, 13, 14, 14, 15, 15, 16, 16, 17, 17, 18, 18, 19, 19,
      20, 20, 21, 21, 22, 22, 23, 23, 24, 24, 25, 25, 26, 26, 27, 27, 28, 28, 29, 29,
      30, 30, 31, 31, 40, 50
    };
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      auto key = resolved.get("seq");
      CHECK((i < expected.size()));
      CHECK((expected[i++] == key.getNumber<size_t>()));
    }

    CHECK((i == expected.size()));
  }

  // docs match from collection0 (2 analyzers used)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH ANALYZER(PHRASE(d.same, 'xyz', 'test_B'), 'identity') SORT d.seq RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == result.code);
    auto slice = result.result->slice();
    CHECK(slice.isArray());
    std::vector<size_t> expected {
      0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
      10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
      20, 21, 22, 23, 24, 25, 26, 27, 28, 29,
      30, 31, 40
    };
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      auto key = resolved.get("seq");
      CHECK((i < expected.size()));
      CHECK((expected[i++] == key.getNumber<size_t>()));
    }

    CHECK((i == expected.size()));
  }

  // docs match from collection0 (2 analyzers used)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d.same, 'xyz', 'test_B') SORT d.seq RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == result.code);
    auto slice = result.result->slice();
    CHECK(slice.isArray());
    std::vector<size_t> expected {
      0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
      10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
      20, 21, 22, 23, 24, 25, 26, 27, 28, 29,
      30, 31, 40
    };
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      auto key = resolved.get("seq");
      CHECK((i < expected.size()));
      CHECK((expected[i++] == key.getNumber<size_t>()));
    }

    CHECK((i == expected.size()));
  }

  // docs match from collection0 (2 analyzers used)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH ANALYZER(PHRASE(d.same, 'xyz'), 'test_B') SORT d.seq RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == result.code);
    auto slice = result.result->slice();
    CHECK(slice.isArray());
    std::vector<size_t> expected {
      0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
      10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
      20, 21, 22, 23, 24, 25, 26, 27, 28, 29,
      30, 31, 40
    };
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      auto key = resolved.get("seq");
      CHECK((i < expected.size()));
      CHECK((expected[i++] == key.getNumber<size_t>()));
    }

    CHECK((i == expected.size()));
  }

  // docs match from collection1 (1 analyzer used)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH ANALYZER(PHRASE(d.duplicated, 'abcd', 'test_A'), 'test_B') SORT d.seq RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == result.code);
    auto slice = result.result->slice();
    CHECK(slice.isArray());
    std::vector<size_t> expected {
      0, 4, 10, 20, 26, 30, 50
    };
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      auto key = resolved.get("seq");
      CHECK((i < expected.size()));
      CHECK((expected[i++] == key.getNumber<size_t>()));
    }

    CHECK((i == expected.size()));
  }

  // docs match from collection1 (1 analyzer used)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH PHRASE(d.duplicated, 'abcd', 'test_A') SORT d.seq RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == result.code);
    auto slice = result.result->slice();
    CHECK(slice.isArray());
    std::vector<size_t> expected {
      0, 4, 10, 20, 26, 30, 50
    };
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      auto key = resolved.get("seq");
      CHECK((i < expected.size()));
      CHECK((expected[i++] == key.getNumber<size_t>()));
    }

    CHECK((i == expected.size()));
  }

  // docs match from collection1 (1 analyzer used)
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH ANALYZER(PHRASE(d.duplicated, 'abcd'), 'test_A') SORT d.seq RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == result.code);
    auto slice = result.result->slice();
    CHECK(slice.isArray());
    std::vector<size_t> expected {
      0, 4, 10, 20, 26, 30, 50
    };
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      auto key = resolved.get("seq");
      CHECK((i < expected.size()));
      CHECK((expected[i++] == key.getNumber<size_t>()));
    }

    CHECK((i == expected.size()));
  }
}

// test indexing selected fields will ommit non-indexed fields during query
SECTION("test_fields") {
  auto createCollection0 = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection0\" }");
  auto createCollection1 = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection1\" }");
  auto createView = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
  auto collection0 = vocbase.createCollection(createCollection0->slice());
  REQUIRE((nullptr != collection0));
  auto collection1 = vocbase.createCollection(createCollection1->slice());
  REQUIRE((nullptr != collection1));
  auto viewImpl = vocbase.createView(createView->slice());
  REQUIRE((nullptr != viewImpl));

  // populate collections
  {
    auto doc0 = arangodb::velocypack::Parser::fromJson("{ \"seq\": 0, \"X\": \"abc\", \"Y\": \"def\" }");
    auto doc1 = arangodb::velocypack::Parser::fromJson("{ \"seq\": 1, \"X\": \"abc\", \"Y\": \"def\" }");
    static std::vector<std::string> const EMPTY;
    std::vector<std::string> collections{ collection0->name(), collection1->name() };
    arangodb::transaction::Methods trx(
      arangodb::transaction::StandaloneContext::Create(vocbase),
      EMPTY,
      collections,
      EMPTY,
      arangodb::transaction::Options()
    );
    CHECK((trx.begin().ok()));
    CHECK((trx.insert(collection0->name(), doc0->slice(), arangodb::OperationOptions()).ok()));
    CHECK((trx.insert(collection1->name(), doc1->slice(), arangodb::OperationOptions()).ok()));
    CHECK((trx.commit().ok()));
  }

  // link collections with view
  {
    auto updateJson = arangodb::velocypack::Parser::fromJson("{ \"links\": { \
      \"testCollection0\": { \"fields\": { \
        \"X\": { }, \
        \"Y\": { } \
      } }, \
      \"testCollection1\": { \"fields\": { \
        \"X\": { } \
      } } \
    } }");

    CHECK((viewImpl->properties(updateJson->slice(), false).ok()));
  }

  // docs match from both collections
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH d.X == 'abc' OPTIONS { waitForSync: true } SORT d.seq RETURN d",
      nullptr
    );
    REQUIRE(TRI_ERROR_NO_ERROR == result.code);
    auto slice = result.result->slice();
    CHECK(slice.isArray());
    std::vector<size_t> expected { 0, 1 };
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      auto key = resolved.get("seq");
      CHECK((i < expected.size()));
      CHECK((expected[i++] == key.getNumber<size_t>()));
    }

    CHECK((i == expected.size()));
  }

  // docs match from collection0
  {
    auto result = arangodb::tests::executeQuery(
      vocbase,
      "FOR d IN testView SEARCH d.Y == 'def' SORT d.seq RETURN d"
    );
    REQUIRE(TRI_ERROR_NO_ERROR == result.code);
    auto slice = result.result->slice();
    CHECK(slice.isArray());
    std::vector<size_t> expected { 0 };
    size_t i = 0;

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto const resolved = itr.value().resolveExternals();
      auto key = resolved.get("seq");
      CHECK((i < expected.size()));
      CHECK((expected[i++] == key.getNumber<size_t>()));
    }

    CHECK((i == expected.size()));
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate tests
////////////////////////////////////////////////////////////////////////////////

}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------