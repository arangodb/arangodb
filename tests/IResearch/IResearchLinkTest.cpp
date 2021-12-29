///////////////////////////////////////////////////////////////////////////////
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

#include "gtest/gtest.h"

#include "store/fs_directory.hpp"
#include "utils/file_utils.hpp"
#include "utils/log.hpp"
#include "utils/utf8_path.hpp"

#include <velocypack/Parser.h>

#include <filesystem>
#include <regex>

#include "Aql/QueryRegistry.h"
#include "Basics/files.h"
#include "IResearch/IResearchCommon.h"
#include "IResearch/IResearchFeature.h"
#include "IResearch/IResearchLink.h"
#include "IResearch/IResearchView.h"
#include "IResearch/common.h"
#include "IResearchTestCompressor.h"
#include "Logger/LogTopic.h"
#include "Logger/Logger.h"
#include "Metrics/MetricKey.h"
#include "Mocks/IResearchLinkMock.h"
#include "Mocks/Servers.h"
#include "Mocks/StorageEngineMock.h"
#include "RestServer/DatabasePathFeature.h"
#include "RestServer/FlushFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "Transaction/Methods.h"
#include "Transaction/StandaloneContext.h"
#include "VocBase/KeyGenerator.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/LogicalView.h"

using namespace std::chrono_literals;
namespace fs = std::filesystem;

#if USE_ENTERPRISE
#include "Enterprise/Ldap/LdapFeature.h"
#endif

REGISTER_COMPRESSION(
    iresearch::compression::mock::test_compressor,
    &iresearch::compression::mock::test_compressor::compressor,
    &iresearch::compression::mock::test_compressor::decompressor);

static const VPackBuilder systemDatabaseBuilder = dbArgsBuilder();
static const VPackSlice systemDatabaseArgs = systemDatabaseBuilder.slice();
// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

class IResearchLinkTest
    : public ::testing::Test,
      public arangodb::tests::LogSuppressor<arangodb::Logger::AUTHENTICATION,
                                            arangodb::LogLevel::ERR>,
      public arangodb::tests::LogSuppressor<arangodb::Logger::ENGINES,
                                            arangodb::LogLevel::FATAL>,
      public arangodb::tests::LogSuppressor<arangodb::Logger::FIXME,
                                            arangodb::LogLevel::FATAL> {
 protected:
  static constexpr size_t kEncBlockSize{13};
  arangodb::tests::mocks::MockAqlServer server;
  std::string testFilesystemPath;

  IResearchLinkTest() : server(false) {
    arangodb::tests::init();

    // ensure ArangoSearch start 1 maintenance for each group
    auto opts = server.server().options();
    auto& ars = server.getFeature<arangodb::iresearch::IResearchFeature>();
    ars.collectOptions(opts);
    auto* commitThreads = opts->get<arangodb::options::UInt32Parameter>(
        "--arangosearch.commit-threads");
    opts->processingResult().touch("arangosearch.commit-threads");
    EXPECT_NE(nullptr, commitThreads);
    *commitThreads->ptr = 1;
    auto* consolidationThreads = opts->get<arangodb::options::UInt32Parameter>(
        "--arangosearch.consolidation-threads");
    opts->processingResult().touch("arangosearch.consolidation-threads");
    EXPECT_NE(nullptr, consolidationThreads);
    *consolidationThreads->ptr = 1;
    ars.validateOptions(opts);

    server.addFeature<arangodb::FlushFeature>(false);
    server.startFeatures();
    EXPECT_EQ((std::pair<size_t, size_t>{1, 1}),
              ars.limits(arangodb::iresearch::ThreadGroup::_0));
    EXPECT_EQ((std::pair<size_t, size_t>{1, 1}),
              ars.limits(arangodb::iresearch::ThreadGroup::_1));

    TransactionStateMock::abortTransactionCount = 0;
    TransactionStateMock::beginTransactionCount = 0;
    TransactionStateMock::commitTransactionCount = 0;

    auto& dbPathFeature = server.getFeature<arangodb::DatabasePathFeature>();
    arangodb::tests::setDatabasePath(
        dbPathFeature);  // ensure test data is stored in a unique directory
    testFilesystemPath = dbPathFeature.directory();

    long systemError;
    std::string systemErrorStr;
    TRI_CreateDirectory(testFilesystemPath.c_str(), systemError,
                        systemErrorStr);
  }

  ~IResearchLinkTest() override {
    TRI_RemoveDirectory(testFilesystemPath.c_str());
  }
};

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

TEST_F(IResearchLinkTest, test_format_id) {
  using namespace arangodb::iresearch;
  static_assert("1_3simd" == getFormat(LinkVersion::MIN));
  static_assert("1_4simd" == getFormat(LinkVersion::MAX));
}

TEST_F(IResearchLinkTest, test_defaults) {
  // no view specified
  {
    auto& engine = *static_cast<StorageEngineMock*>(
        &server.getFeature<arangodb::EngineSelectorFeature>().engine());
    engine.views.clear();
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL,
                          testDBInfo(server.server()));
    auto collectionJson = arangodb::velocypack::Parser::fromJson(
        R"({ "name": "testCollection" })");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    ASSERT_NE(nullptr, logicalCollection);
    auto json = arangodb::velocypack::Parser::fromJson("{}");
    try {
      StorageEngineMock::buildLinkMock(arangodb::IndexId{1}, *logicalCollection,
                                       json->slice());
      EXPECT_TRUE(false);
    } catch (arangodb::basics::Exception const& ex) {
      EXPECT_EQ(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, ex.code());
    }
  }

  // no view can be found (e.g. db-server coming up with view not available from
  // Agency yet)
  {
    auto& engine = *static_cast<StorageEngineMock*>(
        &server.getFeature<arangodb::EngineSelectorFeature>().engine());
    engine.views.clear();
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL,
                          testDBInfo(server.server()));
    auto collectionJson = arangodb::velocypack::Parser::fromJson(
        R"({ "name": "testCollection" })");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    ASSERT_NE(nullptr, logicalCollection);
    auto json = arangodb::velocypack::Parser::fromJson(R"({ "view": "42" })");
    auto link = StorageEngineMock::buildLinkMock(
        arangodb::IndexId{1}, *logicalCollection, json->slice());
    EXPECT_NE(nullptr, link.get());
  }

  // valid link creation
  {
    auto& engine = *static_cast<StorageEngineMock*>(
        &server.getFeature<arangodb::EngineSelectorFeature>().engine());
    engine.views.clear();
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL,
                          testDBInfo(server.server()));
    auto linkJson = arangodb::velocypack::Parser::fromJson(
        R"({ "type": "arangosearch", "view": "42" })");
    auto collectionJson = arangodb::velocypack::Parser::fromJson(
        R"({ "name": "testCollection" })");
    auto viewJson = arangodb::velocypack::Parser::fromJson(
        R"({ "name": "testView", "id": 42, "type": "arangosearch" })");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    ASSERT_NE(nullptr, logicalCollection);
    auto logicalView = vocbase.createView(viewJson->slice());
    ASSERT_NE(nullptr, logicalView);

    bool created;
    auto link = logicalCollection->createIndex(linkJson->slice(), created);
    ASSERT_TRUE(nullptr != link && created);
    EXPECT_TRUE(link->canBeDropped());
    EXPECT_EQ(logicalCollection.get(), &(link->collection()));
    EXPECT_TRUE(link->fieldNames().empty());
    EXPECT_TRUE(link->fields().empty());
    EXPECT_FALSE(link->hasExpansion());
    EXPECT_FALSE(link->hasSelectivityEstimate());
    EXPECT_FALSE(link->implicitlyUnique());
    EXPECT_FALSE(link->isSorted());
    EXPECT_EQ(0, link->memory());
    EXPECT_TRUE(link->sparse());
    EXPECT_TRUE(arangodb::Index::IndexType::TRI_IDX_TYPE_IRESEARCH_LINK ==
                link->type());
    EXPECT_TRUE(arangodb::iresearch::DATA_SOURCE_TYPE.name() ==
                link->typeName());
    EXPECT_FALSE(link->unique());
    auto* impl = dynamic_cast<arangodb::iresearch::IResearchLink*>(link.get());
    ASSERT_NE(nullptr, impl);
    ASSERT_EQ("1_3simd", impl->format());

    arangodb::iresearch::IResearchLinkMeta actualMeta;
    arangodb::iresearch::IResearchLinkMeta expectedMeta;
    auto builder = link->toVelocyPack(
        arangodb::Index::makeFlags(arangodb::Index::Serialize::Figures));
    std::string error;

    EXPECT_TRUE(
        actualMeta.init(server.server(), builder->slice(), false, error));
    EXPECT_EQ(expectedMeta, actualMeta);
    auto slice = builder->slice();
    EXPECT_TRUE(slice.hasKey("view"));
    EXPECT_TRUE(slice.get("view").isString());
    EXPECT_TRUE(logicalView->guid() == slice.get("view").copyString());
    EXPECT_TRUE(slice.hasKey("figures"));
    auto figuresSlice = slice.get("figures");
    EXPECT_TRUE(figuresSlice.isObject());
    EXPECT_TRUE(figuresSlice.hasKey("indexSize"));
    EXPECT_TRUE(figuresSlice.get("indexSize").isNumber());
    EXPECT_EQ(0, figuresSlice.get("indexSize").getNumber<size_t>());
    EXPECT_TRUE(figuresSlice.hasKey("numFiles"));
    EXPECT_TRUE(figuresSlice.get("numFiles").isNumber());
    EXPECT_EQ(1, figuresSlice.get("numFiles").getNumber<size_t>());
    EXPECT_TRUE(figuresSlice.hasKey("numDocs"));
    EXPECT_TRUE(figuresSlice.get("numDocs").isNumber());
    EXPECT_EQ(0, figuresSlice.get("numDocs").getNumber<size_t>());
    EXPECT_TRUE(figuresSlice.hasKey("numLiveDocs"));
    EXPECT_TRUE(figuresSlice.get("numLiveDocs").isNumber());
    EXPECT_EQ(0, figuresSlice.get("numLiveDocs").getNumber<size_t>());
    EXPECT_TRUE(figuresSlice.hasKey("numBufferedDocs"));
    EXPECT_TRUE(figuresSlice.get("numBufferedDocs").isNumber());
    EXPECT_EQ(0, figuresSlice.get("numBufferedDocs").getNumber<size_t>());
    EXPECT_TRUE(figuresSlice.hasKey("numSegments"));
    EXPECT_TRUE(figuresSlice.get("numSegments").isNumber());
    EXPECT_EQ(0, figuresSlice.get("numSegments").getNumber<size_t>());
    EXPECT_TRUE((logicalCollection->dropIndex(link->id()) &&
                 logicalCollection->getIndexes().empty()));
  }

  // valid link creation (explicit version)
  {
    auto& engine = *static_cast<StorageEngineMock*>(
        &server.getFeature<arangodb::EngineSelectorFeature>().engine());
    engine.views.clear();
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL,
                          testDBInfo(server.server()));
    auto linkJson = arangodb::velocypack::Parser::fromJson(
        R"({ "type": "arangosearch", "view": "42", "version":1 })");
    auto collectionJson = arangodb::velocypack::Parser::fromJson(
        R"({ "name": "testCollection" })");
    auto viewJson = arangodb::velocypack::Parser::fromJson(
        R"({ "name": "testView", "id": 42, "type": "arangosearch" })");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    ASSERT_NE(nullptr, logicalCollection);
    auto logicalView = vocbase.createView(viewJson->slice());
    ASSERT_NE(nullptr, logicalView);

    bool created;
    auto link = logicalCollection->createIndex(linkJson->slice(), created);
    ASSERT_TRUE(nullptr != link && created);
    EXPECT_TRUE(link->canBeDropped());
    EXPECT_EQ(logicalCollection.get(), &(link->collection()));
    EXPECT_TRUE(link->fieldNames().empty());
    EXPECT_TRUE(link->fields().empty());
    EXPECT_FALSE(link->hasExpansion());
    EXPECT_FALSE(link->hasSelectivityEstimate());
    EXPECT_FALSE(link->implicitlyUnique());
    EXPECT_FALSE(link->isSorted());
    EXPECT_EQ(0, link->memory());
    EXPECT_TRUE(link->sparse());
    EXPECT_TRUE(arangodb::Index::IndexType::TRI_IDX_TYPE_IRESEARCH_LINK ==
                link->type());
    EXPECT_TRUE(arangodb::iresearch::DATA_SOURCE_TYPE.name() ==
                link->typeName());
    EXPECT_FALSE(link->unique());
    auto* impl = dynamic_cast<arangodb::iresearch::IResearchLink*>(link.get());
    ASSERT_NE(nullptr, impl);
    ASSERT_EQ("1_4simd", impl->format());

    arangodb::iresearch::IResearchLinkMeta actualMeta;
    arangodb::iresearch::IResearchLinkMeta expectedMeta;
    auto builder = link->toVelocyPack(
        arangodb::Index::makeFlags(arangodb::Index::Serialize::Figures));
    std::string error;

    EXPECT_TRUE(
        actualMeta.init(server.server(), builder->slice(), false, error));
    EXPECT_EQ(expectedMeta, actualMeta);
    auto slice = builder->slice();
    EXPECT_TRUE(slice.hasKey("view"));
    EXPECT_TRUE(slice.get("view").isString());
    EXPECT_TRUE(logicalView->guid() == slice.get("view").copyString());
    EXPECT_TRUE(slice.hasKey("figures"));
    auto figuresSlice = slice.get("figures");
    EXPECT_TRUE(figuresSlice.isObject());
    EXPECT_TRUE(figuresSlice.hasKey("indexSize"));
    EXPECT_TRUE(figuresSlice.get("indexSize").isNumber());
    EXPECT_EQ(0, figuresSlice.get("indexSize").getNumber<size_t>());
    EXPECT_TRUE(figuresSlice.hasKey("numFiles"));
    EXPECT_TRUE(figuresSlice.get("numFiles").isNumber());
    EXPECT_EQ(1, figuresSlice.get("numFiles").getNumber<size_t>());
    EXPECT_TRUE(figuresSlice.hasKey("numDocs"));
    EXPECT_TRUE(figuresSlice.get("numDocs").isNumber());
    EXPECT_EQ(0, figuresSlice.get("numDocs").getNumber<size_t>());
    EXPECT_TRUE(figuresSlice.hasKey("numLiveDocs"));
    EXPECT_TRUE(figuresSlice.get("numLiveDocs").isNumber());
    EXPECT_EQ(0, figuresSlice.get("numLiveDocs").getNumber<size_t>());
    EXPECT_TRUE(figuresSlice.hasKey("numBufferedDocs"));
    EXPECT_TRUE(figuresSlice.get("numBufferedDocs").isNumber());
    EXPECT_EQ(0, figuresSlice.get("numBufferedDocs").getNumber<size_t>());
    EXPECT_TRUE(figuresSlice.hasKey("numSegments"));
    EXPECT_TRUE(figuresSlice.get("numSegments").isNumber());
    EXPECT_EQ(0, figuresSlice.get("numSegments").getNumber<size_t>());
    EXPECT_TRUE((logicalCollection->dropIndex(link->id()) &&
                 logicalCollection->getIndexes().empty()));
  }

  // ensure jSON is still valid after unload()
  {
    auto& engine = *static_cast<StorageEngineMock*>(
        &server.getFeature<arangodb::EngineSelectorFeature>().engine());
    engine.views.clear();
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL,
                          testDBInfo(server.server()));
    auto linkJson = arangodb::velocypack::Parser::fromJson(
        R"({ "type": "arangosearch", "view": "42" })");
    auto collectionJson = arangodb::velocypack::Parser::fromJson(
        R"({ "name": "testCollection" })");
    auto viewJson = arangodb::velocypack::Parser::fromJson(
        R"({ "name": "testView", "id": 42, "type": "arangosearch" })");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    ASSERT_TRUE((nullptr != logicalCollection));
    auto logicalView = vocbase.createView(viewJson->slice());
    ASSERT_TRUE((false == !logicalView));

    bool created;
    auto link = logicalCollection->createIndex(linkJson->slice(), created);
    ASSERT_TRUE(nullptr != link && created);
    EXPECT_TRUE(link->canBeDropped());
    EXPECT_EQ(logicalCollection.get(), &(link->collection()));
    EXPECT_TRUE(link->fieldNames().empty());
    EXPECT_TRUE(link->fields().empty());
    EXPECT_FALSE(link->hasExpansion());
    EXPECT_FALSE(link->hasSelectivityEstimate());
    EXPECT_FALSE(link->implicitlyUnique());
    EXPECT_FALSE(link->isSorted());
    EXPECT_EQ(0, link->memory());
    EXPECT_TRUE(link->sparse());
    EXPECT_EQ(arangodb::Index::IndexType::TRI_IDX_TYPE_IRESEARCH_LINK,
              link->type());
    EXPECT_EQ(arangodb::iresearch::DATA_SOURCE_TYPE.name(), link->typeName());
    EXPECT_FALSE(link->unique());

    {
      arangodb::iresearch::IResearchLinkMeta actualMeta;
      arangodb::iresearch::IResearchLinkMeta expectedMeta;
      auto builder = link->toVelocyPack(
          arangodb::Index::makeFlags(arangodb::Index::Serialize::Figures));
      std::string error;

      EXPECT_TRUE(
          (actualMeta.init(server.server(), builder->slice(), false, error) &&
           expectedMeta == actualMeta));
      auto slice = builder->slice();
      EXPECT_TRUE(slice.hasKey("view") && slice.get("view").isString() &&
                  logicalView->guid() == slice.get("view").copyString());
      EXPECT_TRUE(slice.hasKey("figures"));
      auto figuresSlice = slice.get("figures");
      EXPECT_TRUE(figuresSlice.isObject());
      EXPECT_TRUE(figuresSlice.hasKey("indexSize"));
      EXPECT_TRUE(figuresSlice.get("indexSize").isNumber());
      EXPECT_EQ(0, figuresSlice.get("indexSize").getNumber<size_t>());
      EXPECT_TRUE(figuresSlice.hasKey("numFiles"));
      EXPECT_TRUE(figuresSlice.get("numFiles").isNumber());
      EXPECT_EQ(1, figuresSlice.get("numFiles").getNumber<size_t>());
      EXPECT_TRUE(figuresSlice.hasKey("numDocs"));
      EXPECT_TRUE(figuresSlice.get("numDocs").isNumber());
      EXPECT_EQ(0, figuresSlice.get("numDocs").getNumber<size_t>());
      EXPECT_TRUE(figuresSlice.hasKey("numLiveDocs"));
      EXPECT_TRUE(figuresSlice.get("numLiveDocs").isNumber());
      EXPECT_EQ(0, figuresSlice.get("numLiveDocs").getNumber<size_t>());
      EXPECT_TRUE(figuresSlice.hasKey("numBufferedDocs"));
      EXPECT_TRUE(figuresSlice.get("numBufferedDocs").isNumber());
      EXPECT_EQ(0, figuresSlice.get("numBufferedDocs").getNumber<size_t>());
      EXPECT_TRUE(figuresSlice.hasKey("numSegments"));
      EXPECT_TRUE(figuresSlice.get("numSegments").isNumber());
      EXPECT_EQ(0, figuresSlice.get("numSegments").getNumber<size_t>());
    }

    // ensure jSON is still valid after unload()
    {
      link->unload();
      auto builder = link->toVelocyPack(
          arangodb::Index::makeFlags(arangodb::Index::Serialize::Figures));
      auto slice = builder->slice();
      EXPECT_TRUE(slice.hasKey("view") && slice.get("view").isString() &&
                  logicalView->guid() == slice.get("view").copyString());
      EXPECT_TRUE(slice.hasKey("figures"));
      auto figuresSlice = slice.get("figures");
      EXPECT_TRUE(figuresSlice.isObject());
      EXPECT_TRUE(figuresSlice.hasKey("indexSize"));
      EXPECT_TRUE(figuresSlice.get("indexSize").isNumber());
      EXPECT_EQ(0, figuresSlice.get("indexSize").getNumber<size_t>());
      EXPECT_TRUE(figuresSlice.hasKey("numFiles"));
      EXPECT_TRUE(figuresSlice.get("numFiles").isNumber());
      EXPECT_EQ(0, figuresSlice.get("numFiles").getNumber<size_t>());
      EXPECT_TRUE(figuresSlice.hasKey("numDocs"));
      EXPECT_TRUE(figuresSlice.get("numDocs").isNumber());
      EXPECT_EQ(0, figuresSlice.get("numDocs").getNumber<size_t>());
      EXPECT_TRUE(figuresSlice.hasKey("numLiveDocs"));
      EXPECT_TRUE(figuresSlice.get("numLiveDocs").isNumber());
      EXPECT_EQ(0, figuresSlice.get("numLiveDocs").getNumber<size_t>());
      EXPECT_TRUE(figuresSlice.hasKey("numBufferedDocs"));
      EXPECT_TRUE(figuresSlice.get("numBufferedDocs").isNumber());
      EXPECT_EQ(0, figuresSlice.get("numBufferedDocs").getNumber<size_t>());
      EXPECT_TRUE(figuresSlice.hasKey("numSegments"));
      EXPECT_TRUE(figuresSlice.get("numSegments").isNumber());
      EXPECT_EQ(0, figuresSlice.get("numSegments").getNumber<size_t>());
    }
  }
}

TEST_F(IResearchLinkTest, test_init) {
  // collection registered with view (collection initially not in view)
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson(
        R"({ "name": "testCollection", "id": 100 })");
    auto linkJson = arangodb::velocypack::Parser::fromJson(
        R"({ "type": "arangosearch", "view": "42" })");
    auto viewJson = arangodb::velocypack::Parser::fromJson(
        R"({ "name": "testView", "id": 42, "type": "arangosearch" })");
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL,
                          testDBInfo(server.server()));
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    ASSERT_TRUE((nullptr != logicalCollection));
    auto logicalView = vocbase.createView(viewJson->slice());
    ASSERT_TRUE((false == !logicalView));

    // no collections in view before
    {
      std::unordered_set<arangodb::DataSourceId> expected;
      std::set<arangodb::DataSourceId> actual;

      EXPECT_TRUE((logicalView->visitCollections(
          [&actual](arangodb::DataSourceId cid) -> bool {
            actual.emplace(cid);
            return true;
          })));

      for (auto& cid : expected) {
        EXPECT_EQ(1, actual.erase(cid));
      }

      EXPECT_TRUE((actual.empty()));
    }

    auto link = StorageEngineMock::buildLinkMock(
        arangodb::IndexId{1}, *logicalCollection, linkJson->slice());
    EXPECT_NE(nullptr, link.get());
    // EXPECT_TRUE((false == !link));

    // collection in view after
    {
      std::unordered_set<arangodb::DataSourceId> expected = {
          arangodb::DataSourceId{100}};
      std::set<arangodb::DataSourceId> actual;

      EXPECT_TRUE((logicalView->visitCollections(
          [&actual](arangodb::DataSourceId cid) -> bool {
            actual.emplace(cid);
            return true;
          })));

      for (auto& cid : expected) {
        EXPECT_EQ(1, actual.erase(cid));
      }

      EXPECT_TRUE((actual.empty()));
    }

    link.reset();

    // collection in view on destruct
    {
      std::unordered_set<arangodb::DataSourceId> expected = {
          arangodb::DataSourceId{100}};
      std::set<arangodb::DataSourceId> actual;

      EXPECT_TRUE((logicalView->visitCollections(
          [&actual](arangodb::DataSourceId cid) -> bool {
            actual.emplace(cid);
            return true;
          })));

      for (auto& cid : expected) {
        EXPECT_EQ(1, actual.erase(cid));
      }

      EXPECT_TRUE((actual.empty()));
    }
  }

  // collection registered with view (collection initially is in view)
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson(
        R"({ "name": "testCollection", "id": 101 })");
    auto linkJson = arangodb::velocypack::Parser::fromJson(
        R"({ "type": "arangosearch", "view": "43" })");
    auto viewJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testView\", \"id\": 43, \"type\": \"arangosearch\", "
        "\"collections\": [ 101 ] }");
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL,
                          testDBInfo(server.server()));
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    ASSERT_NE(nullptr, logicalCollection);
    auto logicalView = vocbase.createView(viewJson->slice());
    ASSERT_NE(nullptr, logicalView);

    // collection in view before
    {
      std::unordered_set<arangodb::DataSourceId> expected = {
          arangodb::DataSourceId{101}};
      std::set<arangodb::DataSourceId> actual;

      EXPECT_TRUE((logicalView->visitCollections(
          [&actual](arangodb::DataSourceId cid) -> bool {
            actual.emplace(cid);
            return true;
          })));

      for (auto& cid : expected) {
        EXPECT_EQ(1, actual.erase(cid));
      }

      EXPECT_TRUE((actual.empty()));
    }

    auto link = StorageEngineMock::buildLinkMock(
        arangodb::IndexId{1}, *logicalCollection, linkJson->slice());
    EXPECT_NE(nullptr, link.get());

    // collection in view after
    {
      std::unordered_set<arangodb::DataSourceId> expected = {
          arangodb::DataSourceId{101}};
      std::set<arangodb::DataSourceId> actual;

      EXPECT_TRUE((logicalView->visitCollections(
          [&actual](arangodb::DataSourceId cid) -> bool {
            actual.emplace(cid);
            return true;
          })));

      for (auto& cid : expected) {
        EXPECT_EQ(1, actual.erase(cid));
      }

      EXPECT_TRUE((actual.empty()));
    }

    link.reset();

    // collection still in view on destruct
    {
      std::unordered_set<arangodb::DataSourceId> expected = {
          arangodb::DataSourceId{101}};
      std::set<arangodb::DataSourceId> actual;

      EXPECT_TRUE((logicalView->visitCollections(
          [&actual](arangodb::DataSourceId cid) -> bool {
            actual.emplace(cid);
            return true;
          })));

      for (auto& cid : expected) {
        EXPECT_EQ(1, actual.erase(cid));
      }

      EXPECT_TRUE((actual.empty()));
    }
  }
}

TEST_F(IResearchLinkTest, test_self_token) {
  // test empty token
  {
    arangodb::iresearch::AsyncLinkHandle empty(nullptr);
    auto lock = empty.lock();
    EXPECT_EQ(nullptr, lock.get());
  }

  arangodb::iresearch::IResearchLink::AsyncLinkPtr self;

  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson(
        R"({ "name": "testCollection" })");
    auto linkJson =
        arangodb::velocypack::Parser::fromJson(R"({ "view": "testView" })");
    auto viewJson = arangodb::velocypack::Parser::fromJson(
        R"({ "name": "testView", "type":"arangosearch" })");
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL,
                    testDBInfo(server.server()));
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    ASSERT_NE(nullptr, logicalCollection);
    auto logicalView = vocbase.createView(viewJson->slice());
    EXPECT_NE(nullptr, logicalView);
    auto index = StorageEngineMock::buildLinkMock(
        arangodb::IndexId{42}, *logicalCollection, linkJson->slice());
    ASSERT_NE(nullptr, index.get());
    // ASSERT_TRUE((false == !index));
    auto link =
        std::dynamic_pointer_cast<arangodb::iresearch::IResearchLink>(index);
    ASSERT_NE(nullptr, link);
    ASSERT_EQ(
        arangodb::iresearch::getFormat(arangodb::iresearch::LinkVersion::MIN),
        link->format());
    self = link->self();
    EXPECT_NE(nullptr, self);
    auto lock = self->lock();
    EXPECT_EQ(link.get(), lock.get());
  }

  EXPECT_TRUE(self);
  auto lock = self->lock();
  EXPECT_EQ(nullptr, lock.get());
}

TEST_F(IResearchLinkTest, test_drop) {
  // collection drop (removes collection from view) subsequent destroy does not
  // touch view
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson(
        R"({ "name": "testCollection", "id": 100 })");
    auto linkJson = arangodb::velocypack::Parser::fromJson(
        R"({ "type": "arangosearch", "view": "42" })");
    auto viewJson = arangodb::velocypack::Parser::fromJson(
        R"({ "name": "testView", "id": 42, "type": "arangosearch" })");
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL,
                          testDBInfo(server.server()));
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    ASSERT_TRUE((nullptr != logicalCollection));
    auto logicalView = vocbase.createView(viewJson->slice());
    ASSERT_TRUE((false == !logicalView));

    auto link0 = StorageEngineMock::buildLinkMock(
        arangodb::IndexId{1}, *logicalCollection, linkJson->slice());
    EXPECT_NE(nullptr, link0.get());

    // collection in view before
    {
      std::unordered_set<arangodb::DataSourceId> expected = {
          arangodb::DataSourceId{100}};
      std::set<arangodb::DataSourceId> actual;

      EXPECT_TRUE((logicalView->visitCollections(
          [&actual](arangodb::DataSourceId cid) -> bool {
            actual.emplace(cid);
            return true;
          })));

      for (auto& cid : expected) {
        EXPECT_EQ(1, actual.erase(cid));
      }

      EXPECT_TRUE((actual.empty()));
    }

    EXPECT_TRUE((true == (*dynamic_cast<arangodb::iresearch::IResearchLink*>(
                              link0.get()) == *logicalView)));
    EXPECT_TRUE((link0->drop().ok()));
    EXPECT_TRUE((true == (*dynamic_cast<arangodb::iresearch::IResearchLink*>(
                              link0.get()) == *logicalView)));

    // collection not in view after
    {
      std::unordered_set<arangodb::DataSourceId> expected;
      std::set<arangodb::DataSourceId> actual;

      EXPECT_TRUE((logicalView->visitCollections(
          [&actual](arangodb::DataSourceId cid) -> bool {
            actual.emplace(cid);
            return true;
          })));

      for (auto& cid : expected) {
        EXPECT_EQ(1, actual.erase(cid));
      }

      EXPECT_TRUE((actual.empty()));
    }

    auto link1 = StorageEngineMock::buildLinkMock(
        arangodb::IndexId{1}, *logicalCollection, linkJson->slice());
    EXPECT_NE(nullptr, link1);

    // collection in view before (new link)
    {
      std::unordered_set<arangodb::DataSourceId> expected = {
          arangodb::DataSourceId{100}};
      std::set<arangodb::DataSourceId> actual;

      EXPECT_TRUE((logicalView->visitCollections(
          [&actual](arangodb::DataSourceId cid) -> bool {
            actual.emplace(cid);
            return true;
          })));

      for (auto& cid : expected) {
        EXPECT_EQ(1, actual.erase(cid));
      }

      EXPECT_TRUE((actual.empty()));
    }

    link0.reset();

    // collection in view after (new link) subsequent destroy does not touch
    // view
    {
      std::unordered_set<arangodb::DataSourceId> expected = {
          arangodb::DataSourceId{100}};
      std::set<arangodb::DataSourceId> actual;

      EXPECT_TRUE((logicalView->visitCollections(
          [&actual](arangodb::DataSourceId cid) -> bool {
            actual.emplace(cid);
            return true;
          })));

      for (auto& cid : expected) {
        EXPECT_EQ(1, actual.erase(cid));
      }

      EXPECT_TRUE((actual.empty()));
    }
  }
}

TEST_F(IResearchLinkTest, test_unload) {
  // index unload does not touch the view (subsequent destroy)
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson(
        R"({ "name": "testCollection", "id": 100 })");
    auto linkJson = arangodb::velocypack::Parser::fromJson(
        R"({ "type": "arangosearch", "view": "42" })");
    auto viewJson = arangodb::velocypack::Parser::fromJson(
        R"({ "name": "testView", "id": 42, "type": "arangosearch" })");
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL,
                          testDBInfo(server.server()));
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    ASSERT_TRUE((nullptr != logicalCollection));
    auto logicalView = vocbase.createView(viewJson->slice());
    ASSERT_TRUE((false == !logicalView));

    auto link = StorageEngineMock::buildLinkMock(
        arangodb::IndexId{1}, *logicalCollection, linkJson->slice());
    EXPECT_NE(nullptr, link.get());

    // collection in view before
    {
      std::unordered_set<arangodb::DataSourceId> expected = {
          arangodb::DataSourceId{100}};
      std::set<arangodb::DataSourceId> actual;

      EXPECT_TRUE((logicalView->visitCollections(
          [&actual](arangodb::DataSourceId cid) -> bool {
            actual.emplace(cid);
            return true;
          })));

      for (auto& cid : expected) {
        EXPECT_EQ(1, actual.erase(cid));
      }

      EXPECT_TRUE((actual.empty()));
    }

    EXPECT_TRUE((true == (*dynamic_cast<arangodb::iresearch::IResearchLink*>(
                              link.get()) == *logicalView)));
    link->unload();
    EXPECT_TRUE((true == (*dynamic_cast<arangodb::iresearch::IResearchLink*>(
                              link.get()) == *logicalView)));

    // collection in view after unload
    {
      std::unordered_set<arangodb::DataSourceId> expected = {
          arangodb::DataSourceId{100}};
      std::set<arangodb::DataSourceId> actual;

      EXPECT_TRUE((logicalView->visitCollections(
          [&actual](arangodb::DataSourceId cid) -> bool {
            actual.emplace(cid);
            return true;
          })));

      for (auto& cid : expected) {
        EXPECT_EQ(1, actual.erase(cid));
      }

      EXPECT_TRUE((actual.empty()));
    }

    link.reset();

    // collection in view after destruct, subsequent destroy does not touch view
    {
      std::unordered_set<arangodb::DataSourceId> expected = {
          arangodb::DataSourceId{100}};
      std::set<arangodb::DataSourceId> actual;

      EXPECT_TRUE((logicalView->visitCollections(
          [&actual](arangodb::DataSourceId cid) -> bool {
            actual.emplace(cid);
            return true;
          })));

      for (auto& cid : expected) {
        EXPECT_EQ(1, actual.erase(cid));
      }

      EXPECT_TRUE((actual.empty()));
    }
  }
}

TEST_F(IResearchLinkTest, test_write_index_creation_version_0) {
  static std::vector<std::string> const kEmpty;
  auto doc0 = arangodb::velocypack::Parser::fromJson(R"({ "abc": "def" })");
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL,
                        testDBInfo(server.server()));
  std::string dataPath =
      ((((irs::utf8_path() /= testFilesystemPath) /=
         std::string("databases")) /=
        (std::string("database-") + std::to_string(vocbase.id()))) /=
       std::string("arangosearch-42"))
          .string();
  auto linkJson = arangodb::velocypack::Parser::fromJson(
      "{ \"id\": 42, \"type\": \"arangosearch\", \"view\": \"42\", "
      "\"includeAllFields\": true }");
  auto collectionJson =
      arangodb::velocypack::Parser::fromJson(R"({ "name": "testCollection" })");
  auto viewJson = arangodb::velocypack::Parser::fromJson(
      "{ \
    \"id\": 42, \
    \"name\": \"testView\", \
    \"type\": \"arangosearch\" \
  }");
  auto logicalCollection = vocbase.createCollection(collectionJson->slice());
  ASSERT_TRUE((nullptr != logicalCollection));
  auto view = std::dynamic_pointer_cast<arangodb::iresearch::IResearchView>(
      vocbase.createView(viewJson->slice()));
  ASSERT_TRUE((false == !view));
  view->open();
  ASSERT_TRUE(server.server().hasFeature<arangodb::FlushFeature>());

  dataPath = ((((irs::utf8_path() /= testFilesystemPath) /=
                std::string("databases")) /=
               (std::string("database-") + std::to_string(vocbase.id()))) /=
              (std::string("arangosearch-") +
               std::to_string(logicalCollection->id().id()) + "_42"))
                 .string();
  irs::fs_directory directory(dataPath);
  bool created;
  auto link = logicalCollection->createIndex(linkJson->slice(), created);
  ASSERT_TRUE((false == !link && created));
  auto reader = irs::directory_reader::open(directory);
  EXPECT_EQ(0, reader.reopen().live_docs_count());
  {
    arangodb::transaction::Methods trx(
        arangodb::transaction::StandaloneContext::Create(vocbase), kEmpty,
        kEmpty, kEmpty, arangodb::transaction::Options());
    trx.addHint(arangodb::transaction::Hints::Hint::INDEX_CREATION);
    EXPECT_TRUE((trx.begin().ok()));
    auto* l = dynamic_cast<arangodb::iresearch::IResearchLink*>(link.get());
    ASSERT_EQ(
        arangodb::iresearch::getFormat(arangodb::iresearch::LinkVersion::MIN),
        l->format());
    ASSERT_TRUE(l != nullptr);
    EXPECT_TRUE(
        (l->insert(trx, arangodb::LocalDocumentId(1), doc0->slice()).ok()));
    l->commit(true);
    EXPECT_EQ(
        1, reader.reopen().live_docs_count());  // should see this immediately

    EXPECT_TRUE((trx.commit().ok()));
    EXPECT_TRUE((l->commit().ok()));
  }

  EXPECT_EQ(1, reader.reopen().live_docs_count());
  logicalCollection->dropIndex(link->id());
  EXPECT_ANY_THROW((reader.reopen()));
}

TEST_F(IResearchLinkTest, test_write_index_creation_version_1) {
  static std::vector<std::string> const kEmpty;
  auto doc0 = arangodb::velocypack::Parser::fromJson(R"({ "abc": "def" })");
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL,
                        testDBInfo(server.server()));
  std::string dataPath =
      ((((irs::utf8_path() /= testFilesystemPath) /=
         std::string("databases")) /=
        (std::string("database-") + std::to_string(vocbase.id()))) /=
       std::string("arangosearch-42"))
          .string();
  auto linkJson = arangodb::velocypack::Parser::fromJson(
      R"({ "id": 42, "type": "arangosearch", "view": "42", "includeAllFields": true, "version":1 })");
  auto collectionJson =
      arangodb::velocypack::Parser::fromJson(R"({ "name": "testCollection" })");
  auto viewJson = arangodb::velocypack::Parser::fromJson(
      "{ \
    \"id\": 42, \
    \"name\": \"testView\", \
    \"type\": \"arangosearch\" \
  }");
  auto logicalCollection = vocbase.createCollection(collectionJson->slice());
  ASSERT_TRUE((nullptr != logicalCollection));
  auto view = std::dynamic_pointer_cast<arangodb::iresearch::IResearchView>(
      vocbase.createView(viewJson->slice()));
  ASSERT_TRUE((false == !view));
  view->open();
  ASSERT_TRUE(server.server().hasFeature<arangodb::FlushFeature>());

  dataPath = ((((irs::utf8_path() /= testFilesystemPath) /=
                std::string("databases")) /=
               (std::string("database-") + std::to_string(vocbase.id()))) /=
              (std::string("arangosearch-") +
               std::to_string(logicalCollection->id().id()) + "_42"))
                 .string();
  irs::fs_directory directory(dataPath);
  bool created;
  auto link = logicalCollection->createIndex(linkJson->slice(), created);
  ASSERT_TRUE((false == !link && created));
  auto reader = irs::directory_reader::open(directory);
  EXPECT_EQ(0, reader.reopen().live_docs_count());
  {
    arangodb::transaction::Methods trx(
        arangodb::transaction::StandaloneContext::Create(vocbase), kEmpty,
        kEmpty, kEmpty, arangodb::transaction::Options());
    trx.addHint(arangodb::transaction::Hints::Hint::INDEX_CREATION);
    EXPECT_TRUE((trx.begin().ok()));
    auto* l = dynamic_cast<arangodb::iresearch::IResearchLink*>(link.get());
    ASSERT_EQ(
        arangodb::iresearch::getFormat(arangodb::iresearch::LinkVersion::MAX),
        l->format());
    ASSERT_TRUE(l != nullptr);
    EXPECT_TRUE(
        (l->insert(trx, arangodb::LocalDocumentId(1), doc0->slice()).ok()));
    l->commit(true);
    EXPECT_EQ(
        1, reader.reopen().live_docs_count());  // should see this immediately

    EXPECT_TRUE((trx.commit().ok()));
    EXPECT_TRUE((l->commit().ok()));
  }

  EXPECT_EQ(1, reader.reopen().live_docs_count());
  logicalCollection->dropIndex(link->id());
  EXPECT_ANY_THROW((reader.reopen()));
}

TEST_F(IResearchLinkTest, test_write) {
  static std::vector<std::string> const kEmpty;
  auto doc0 = arangodb::velocypack::Parser::fromJson(R"({ "abc": "def" })");
  auto doc1 = arangodb::velocypack::Parser::fromJson(R"({ "ghi": "jkl" })");
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL,
                        testDBInfo(server.server()));
  std::string dataPath =
      ((((irs::utf8_path() /= testFilesystemPath) /=
         std::string("databases")) /=
        (std::string("database-") + std::to_string(vocbase.id()))) /=
       std::string("arangosearch-42"))
          .string();
  auto linkJson = arangodb::velocypack::Parser::fromJson(
      "{ \"id\": 42, \"type\": \"arangosearch\", \"view\": \"42\", "
      "\"includeAllFields\": true }");
  auto collectionJson =
      arangodb::velocypack::Parser::fromJson(R"({ "name": "testCollection" })");
  auto viewJson = arangodb::velocypack::Parser::fromJson(
      "{ \
    \"id\": 42, \
    \"name\": \"testView\", \
    \"type\": \"arangosearch\" \
  }");
  auto logicalCollection = vocbase.createCollection(collectionJson->slice());
  ASSERT_TRUE((nullptr != logicalCollection));
  auto view = std::dynamic_pointer_cast<arangodb::iresearch::IResearchView>(
      vocbase.createView(viewJson->slice()));
  ASSERT_TRUE((false == !view));
  view->open();
  ASSERT_TRUE(server.server().hasFeature<arangodb::FlushFeature>());

  dataPath = ((((irs::utf8_path() /= testFilesystemPath) /=
                std::string("databases")) /=
               (std::string("database-") + std::to_string(vocbase.id()))) /=
              (std::string("arangosearch-") +
               std::to_string(logicalCollection->id().id()) + "_42"))
                 .string();
  irs::fs_directory directory(dataPath);
  bool created;
  auto link = logicalCollection->createIndex(linkJson->slice(), created);
  ASSERT_TRUE((false == !link && created));
  auto reader = irs::directory_reader::open(directory);
  EXPECT_EQ(0, reader.reopen().live_docs_count());
  {
    arangodb::transaction::Methods trx(
        arangodb::transaction::StandaloneContext::Create(vocbase), kEmpty,
        kEmpty, kEmpty, arangodb::transaction::Options());
    EXPECT_TRUE((trx.begin().ok()));
    auto* l = dynamic_cast<arangodb::iresearch::IResearchLink*>(link.get());
    ASSERT_TRUE(l != nullptr);
    EXPECT_TRUE(
        (l->insert(trx, arangodb::LocalDocumentId(1), doc0->slice()).ok()));
    EXPECT_TRUE((l->commit().ok()));
    EXPECT_EQ(0, reader.reopen().live_docs_count());

    EXPECT_TRUE((trx.commit().ok()));
    EXPECT_TRUE((l->commit().ok()));
  }

  EXPECT_EQ(1, reader.reopen().live_docs_count());

  {
    arangodb::transaction::Methods trx(
        arangodb::transaction::StandaloneContext::Create(vocbase), kEmpty,
        kEmpty, kEmpty, arangodb::transaction::Options());
    EXPECT_TRUE((trx.begin().ok()));
    auto* l = dynamic_cast<arangodb::iresearch::IResearchLink*>(link.get());
    ASSERT_TRUE(l != nullptr);
    EXPECT_TRUE(
        (l->insert(trx, arangodb::LocalDocumentId(2), doc1->slice()).ok()));
    EXPECT_TRUE((trx.commit().ok()));
    EXPECT_TRUE((l->commit().ok()));
  }

  EXPECT_EQ(2, reader.reopen().live_docs_count());

  {
    arangodb::transaction::Methods trx(
        arangodb::transaction::StandaloneContext::Create(vocbase), kEmpty,
        kEmpty, kEmpty, arangodb::transaction::Options());
    EXPECT_TRUE((trx.begin().ok()));
    auto* l = dynamic_cast<arangodb::iresearch::IResearchLink*>(link.get());
    ASSERT_TRUE(l != nullptr);
    EXPECT_TRUE(
        (l->remove(trx, arangodb::LocalDocumentId(2), doc1->slice()).ok()));
    EXPECT_TRUE((trx.commit().ok()));
    EXPECT_TRUE((l->commit().ok()));
  }

  EXPECT_EQ(1, reader.reopen().live_docs_count());
  logicalCollection->dropIndex(link->id());
  EXPECT_ANY_THROW((reader.reopen()));
}

TEST_F(IResearchLinkTest, test_write_with_custom_compression_nondefault_sole) {
  static std::vector<std::string> const kEmpty;
  auto doc0 = arangodb::velocypack::Parser::fromJson(
      R"({ "abc": "def", "abc2":"aaa", "sort":"ps"  })");
  auto doc1 = arangodb::velocypack::Parser::fromJson(R"({ "ghi": "jkl" })");
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL,
                        testDBInfo(server.server()));
  std::string dataPath =
      ((((irs::utf8_path() /= testFilesystemPath) /=
         std::string("databases")) /=
        (std::string("database-") + std::to_string(vocbase.id()))) /=
       std::string("arangosearch-42"))
          .string();
  auto linkJson = arangodb::velocypack::Parser::fromJson(
      "{ \"id\": 42, \"type\": \"arangosearch\", \"view\": \"42\", "
      "\"includeAllFields\": true,\
    \"primarySort\":[{\"field\":\"sort\", \"direction\":\"asc\"}],\
    \"storedValues\":[{\"fields\":[\"abc\"], \"compression\":\"test\"}, {\"fields\":[\"abc2\"], \"compression\":\"test\"}]\
    }");
  auto collectionJson =
      arangodb::velocypack::Parser::fromJson(R"({ "name": "testCollection" })");
  auto viewJson = arangodb::velocypack::Parser::fromJson(
      "{ \
    \"id\": 42, \
    \"name\": \"testView\", \
    \"type\": \"arangosearch\", \
    \"primarySort\":[{\"field\":\"sort\", \"direction\":\"asc\"}],\
    \"storedValues\":[{\"fields\":[\"abc\"], \"compression\":\"test\"}, {\"fields\":[\"abc2\"], \"compression\":\"test\"}]\
  }");
  std::set<std::string> compressedValues;
  irs::compression::mock::test_compressor::functions().compress_mock =
      [&compressedValues](irs::byte_type* src, size_t size,
                          irs::bstring& out) -> irs::bytes_ref {
    out.append(src, size);
    compressedValues.emplace(reinterpret_cast<const char*>(src), size);
    return {reinterpret_cast<const irs::byte_type*>(out.data()), size};
  };
  auto compressorRemover = irs::make_finally([]() {
    irs::compression::mock::test_compressor::functions().compress_mock =
        nullptr;
  });
  auto logicalCollection = vocbase.createCollection(collectionJson->slice());
  ASSERT_TRUE((nullptr != logicalCollection));
  auto view = std::dynamic_pointer_cast<arangodb::iresearch::IResearchView>(
      vocbase.createView(viewJson->slice()));
  ASSERT_TRUE((false == !view));
  view->open();
  ASSERT_TRUE(server.server().hasFeature<arangodb::FlushFeature>());

  dataPath = ((((irs::utf8_path() /= testFilesystemPath) /=
                std::string("databases")) /=
               (std::string("database-") + std::to_string(vocbase.id()))) /=
              (std::string("arangosearch-") +
               std::to_string(logicalCollection->id().id()) + "_42"))
                 .string();
  irs::fs_directory directory(dataPath);
  bool created;
  auto link = logicalCollection->createIndex(linkJson->slice(), created);
  ASSERT_TRUE((false == !link && created));
  auto reader = irs::directory_reader::open(directory);
  EXPECT_EQ(0, reader.reopen().live_docs_count());
  {
    arangodb::transaction::Methods trx(
        arangodb::transaction::StandaloneContext::Create(vocbase), kEmpty,
        kEmpty, kEmpty, arangodb::transaction::Options());
    EXPECT_TRUE((trx.begin().ok()));
    auto* l = dynamic_cast<arangodb::iresearch::IResearchLink*>(link.get());
    ASSERT_TRUE(l != nullptr);
    EXPECT_TRUE(
        (l->insert(trx, arangodb::LocalDocumentId(1), doc0->slice()).ok()));
    EXPECT_TRUE((l->commit().ok()));
    EXPECT_EQ(0, reader.reopen().live_docs_count());

    EXPECT_TRUE((trx.commit().ok()));
    EXPECT_TRUE((l->commit().ok()));
  }

  EXPECT_EQ(1, reader.reopen().live_docs_count());

  {
    arangodb::transaction::Methods trx(
        arangodb::transaction::StandaloneContext::Create(vocbase), kEmpty,
        kEmpty, kEmpty, arangodb::transaction::Options());
    EXPECT_TRUE((trx.begin().ok()));
    auto* l = dynamic_cast<arangodb::iresearch::IResearchLink*>(link.get());
    ASSERT_TRUE(l != nullptr);
    EXPECT_TRUE(
        (l->insert(trx, arangodb::LocalDocumentId(2), doc1->slice()).ok()));
    EXPECT_TRUE((trx.commit().ok()));
    EXPECT_TRUE((l->commit().ok()));
  }

  EXPECT_EQ(2, reader.reopen().live_docs_count());
  std::set<std::string> expected;
  auto abcSlice = doc0->slice().get("abc");
  expected.emplace(reinterpret_cast<const char*>(abcSlice.start()),
                   abcSlice.byteSize());
  auto abc2Slice = doc0->slice().get("abc2");
  expected.emplace(reinterpret_cast<const char*>(abc2Slice.start()),
                   abc2Slice.byteSize());
  EXPECT_EQ(expected, compressedValues);
}

TEST_F(IResearchLinkTest,
       test_write_with_custom_compression_nondefault_sole_with_sort) {
  static std::vector<std::string> const kEmpty;
  auto doc0 = arangodb::velocypack::Parser::fromJson(
      R"({ "abc": "def", "abc2":"aaa", "sort":"ps"  })");
  auto doc1 = arangodb::velocypack::Parser::fromJson(
      R"({ "ghi": "jkl", "sort":"pp" })");
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL,
                        testDBInfo(server.server()));
  std::string dataPath =
      ((((irs::utf8_path() /= testFilesystemPath) /=
         std::string("databases")) /=
        (std::string("database-") + std::to_string(vocbase.id()))) /=
       std::string("arangosearch-42"))
          .string();
  auto linkJson = arangodb::velocypack::Parser::fromJson(
      "{ \"id\": 42, \"type\": \"arangosearch\", \"view\": \"42\", "
      "\"includeAllFields\": true,\
    \"primarySort\":[{\"field\":\"sort\", \"direction\":\"asc\"}],\
    \"primarySortCompression\":\"test\",\
    \"storedValues\":[{\"fields\":[\"abc\"], \"compression\":\"test\"}, {\"fields\":[\"abc2\"], \"compression\":\"test\"}]\
    }");
  auto collectionJson =
      arangodb::velocypack::Parser::fromJson(R"({ "name": "testCollection" })");
  auto viewJson = arangodb::velocypack::Parser::fromJson(
      "{ \
    \"id\": 42, \
    \"name\": \"testView\", \
    \"type\": \"arangosearch\", \
    \"primarySort\":[{\"field\":\"sort\", \"direction\":\"asc\"}],\
    \"storedValues\":[{\"fields\":[\"abc\"], \"compression\":\"test\"}, {\"fields\":[\"abc2\"], \"compression\":\"test\"}]\
  }");
  std::set<std::string> compressedValues;
  irs::compression::mock::test_compressor::functions().compress_mock =
      [&compressedValues](irs::byte_type* src, size_t size,
                          irs::bstring& out) -> irs::bytes_ref {
    out.append(src, size);
    compressedValues.emplace(reinterpret_cast<const char*>(src), size);
    return {reinterpret_cast<const irs::byte_type*>(out.data()), size};
  };
  auto compressorRemover = irs::make_finally([]() {
    irs::compression::mock::test_compressor::functions().compress_mock =
        nullptr;
  });
  auto logicalCollection = vocbase.createCollection(collectionJson->slice());
  ASSERT_TRUE((nullptr != logicalCollection));
  auto view = std::dynamic_pointer_cast<arangodb::iresearch::IResearchView>(
      vocbase.createView(viewJson->slice()));
  ASSERT_TRUE((false == !view));
  view->open();
  ASSERT_TRUE(server.server().hasFeature<arangodb::FlushFeature>());

  dataPath = ((((irs::utf8_path() /= testFilesystemPath) /=
                std::string("databases")) /=
               (std::string("database-") + std::to_string(vocbase.id()))) /=
              (std::string("arangosearch-") +
               std::to_string(logicalCollection->id().id()) + "_42"))
                 .string();
  irs::fs_directory directory(dataPath);
  bool created;
  auto link = logicalCollection->createIndex(linkJson->slice(), created);
  ASSERT_TRUE((false == !link && created));
  auto reader = irs::directory_reader::open(directory);
  EXPECT_EQ(0, reader.reopen().live_docs_count());
  {
    arangodb::transaction::Methods trx(
        arangodb::transaction::StandaloneContext::Create(vocbase), kEmpty,
        kEmpty, kEmpty, arangodb::transaction::Options());
    EXPECT_TRUE((trx.begin().ok()));
    auto* l = dynamic_cast<arangodb::iresearch::IResearchLink*>(link.get());
    ASSERT_TRUE(l != nullptr);
    EXPECT_TRUE(
        (l->insert(trx, arangodb::LocalDocumentId(1), doc0->slice()).ok()));
    EXPECT_TRUE((l->commit().ok()));
    EXPECT_EQ(0, reader.reopen().live_docs_count());

    EXPECT_TRUE((trx.commit().ok()));
    EXPECT_TRUE((l->commit().ok()));
  }

  EXPECT_EQ(1, reader.reopen().live_docs_count());

  {
    arangodb::transaction::Methods trx(
        arangodb::transaction::StandaloneContext::Create(vocbase), kEmpty,
        kEmpty, kEmpty, arangodb::transaction::Options());
    EXPECT_TRUE((trx.begin().ok()));
    auto* l = dynamic_cast<arangodb::iresearch::IResearchLink*>(link.get());
    ASSERT_TRUE(l != nullptr);
    EXPECT_TRUE(
        (l->insert(trx, arangodb::LocalDocumentId(2), doc1->slice()).ok()));
    EXPECT_TRUE((trx.commit().ok()));
    EXPECT_TRUE((l->commit().ok()));
  }

  EXPECT_EQ(2, reader.reopen().live_docs_count());
  std::set<std::string> expected;
  auto sortSlice = doc0->slice().get("sort");
  expected.emplace(reinterpret_cast<const char*>(sortSlice.start()),
                   sortSlice.byteSize());
  auto abcSlice = doc0->slice().get("abc");
  expected.emplace(reinterpret_cast<const char*>(abcSlice.start()),
                   abcSlice.byteSize());
  auto abc2Slice = doc0->slice().get("abc2");
  expected.emplace(reinterpret_cast<const char*>(abc2Slice.start()),
                   abc2Slice.byteSize());
  auto sortSlice1 = doc1->slice().get("sort");
  expected.emplace(reinterpret_cast<const char*>(sortSlice1.start()),
                   sortSlice1.byteSize());
  EXPECT_EQ(expected, compressedValues);
}

TEST_F(IResearchLinkTest, test_write_with_custom_compression_nondefault_mixed) {
  static std::vector<std::string> const kEmpty;
  auto doc0 = arangodb::velocypack::Parser::fromJson(
      R"({ "abc": "def", "abc2":"aaa", "sort":"ps" })");
  auto doc1 = arangodb::velocypack::Parser::fromJson(R"({ "ghi": "jkl" })");
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL,
                        testDBInfo(server.server()));
  std::string dataPath =
      ((((irs::utf8_path() /= testFilesystemPath) /=
         std::string("databases")) /=
        (std::string("database-") + std::to_string(vocbase.id()))) /=
       std::string("arangosearch-42"))
          .string();
  auto linkJson = arangodb::velocypack::Parser::fromJson(
      "{ \"id\": 42, \"type\": \"arangosearch\", \"view\": \"42\", "
      "\"includeAllFields\": true,\
    \"primarySort\":[{\"field\":\"sort\", \"direction\":\"asc\"}],\
    \"storedValues\":[{\"fields\":[\"abc\"], \"compression\":\"test\"},\
                      {\"fields\":[\"abc2\"], \"compression\":\"lz4\"},\
                      {\"fields\":[\"ghi\"], \"compression\":\"test\"}]\
    }");
  auto collectionJson =
      arangodb::velocypack::Parser::fromJson(R"({ "name": "testCollection" })");
  auto viewJson = arangodb::velocypack::Parser::fromJson(
      "{ \
    \"id\": 42, \
    \"name\": \"testView\", \
    \"type\": \"arangosearch\", \
    \"primarySort\":[{\"field\":\"sort\", \"direction\":\"asc\"}],\
    \"storedValues\":[{\"fields\":[\"abc\"], \"compression\":\"test\"},\
                      {\"fields\":[\"abc2\"], \"compression\":\"lz4\"},\
                      {\"fields\":[\"ghi\"], \"compression\":\"test\"}]\
  }");
  std::set<std::string> compressedValues;
  irs::compression::mock::test_compressor::functions().compress_mock =
      [&compressedValues](irs::byte_type* src, size_t size,
                          irs::bstring& out) -> irs::bytes_ref {
    out.append(src, size);
    compressedValues.emplace(reinterpret_cast<const char*>(src), size);
    return {reinterpret_cast<const irs::byte_type*>(out.data()), size};
  };
  auto compressorRemover = irs::make_finally([]() {
    irs::compression::mock::test_compressor::functions().compress_mock =
        nullptr;
  });
  auto logicalCollection = vocbase.createCollection(collectionJson->slice());
  ASSERT_TRUE((nullptr != logicalCollection));
  auto view = std::dynamic_pointer_cast<arangodb::iresearch::IResearchView>(
      vocbase.createView(viewJson->slice()));
  ASSERT_TRUE((false == !view));
  view->open();
  ASSERT_TRUE(server.server().hasFeature<arangodb::FlushFeature>());

  dataPath = ((((irs::utf8_path() /= testFilesystemPath) /=
                std::string("databases")) /=
               (std::string("database-") + std::to_string(vocbase.id()))) /=
              (std::string("arangosearch-") +
               std::to_string(logicalCollection->id().id()) + "_42"))
                 .string();
  irs::fs_directory directory(dataPath);
  bool created;
  auto link = logicalCollection->createIndex(linkJson->slice(), created);
  ASSERT_TRUE((false == !link && created));
  auto reader = irs::directory_reader::open(directory);
  EXPECT_EQ(0, reader.reopen().live_docs_count());
  {
    arangodb::transaction::Methods trx(
        arangodb::transaction::StandaloneContext::Create(vocbase), kEmpty,
        kEmpty, kEmpty, arangodb::transaction::Options());
    EXPECT_TRUE((trx.begin().ok()));
    auto* l = dynamic_cast<arangodb::iresearch::IResearchLink*>(link.get());
    ASSERT_TRUE(l != nullptr);
    EXPECT_TRUE(
        (l->insert(trx, arangodb::LocalDocumentId(1), doc0->slice()).ok()));
    EXPECT_TRUE((l->commit().ok()));
    EXPECT_EQ(0, reader.reopen().live_docs_count());

    EXPECT_TRUE((trx.commit().ok()));
    EXPECT_TRUE((l->commit().ok()));
  }

  EXPECT_EQ(1, reader.reopen().live_docs_count());

  {
    arangodb::transaction::Methods trx(
        arangodb::transaction::StandaloneContext::Create(vocbase), kEmpty,
        kEmpty, kEmpty, arangodb::transaction::Options());
    EXPECT_TRUE((trx.begin().ok()));
    auto* l = dynamic_cast<arangodb::iresearch::IResearchLink*>(link.get());
    ASSERT_TRUE(l != nullptr);
    EXPECT_TRUE(
        (l->insert(trx, arangodb::LocalDocumentId(2), doc1->slice()).ok()));
    EXPECT_TRUE((trx.commit().ok()));
    EXPECT_TRUE((l->commit().ok()));
  }

  EXPECT_EQ(2, reader.reopen().live_docs_count());
  std::set<std::string> expected;
  auto abcSlice = doc0->slice().get("abc");
  expected.emplace(reinterpret_cast<const char*>(abcSlice.start()),
                   abcSlice.byteSize());
  auto abc2Slice = doc1->slice().get("ghi");
  expected.emplace(reinterpret_cast<const char*>(abc2Slice.start()),
                   abc2Slice.byteSize());
  EXPECT_EQ(expected, compressedValues);
}

TEST_F(IResearchLinkTest,
       test_write_with_custom_compression_nondefault_mixed_with_sort) {
  static std::vector<std::string> const kEmpty;
  auto doc0 = arangodb::velocypack::Parser::fromJson(
      R"({ "abc": "def", "abc2":"aaa", "sort":"ps" })");
  auto doc1 = arangodb::velocypack::Parser::fromJson(
      R"({ "ghi": "jkl", "sort":"pp" })");
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL,
                        testDBInfo(server.server()));
  std::string dataPath =
      ((((irs::utf8_path() /= testFilesystemPath) /=
         std::string("databases")) /=
        (std::string("database-") + std::to_string(vocbase.id()))) /=
       std::string("arangosearch-42"))
          .string();
  auto linkJson = arangodb::velocypack::Parser::fromJson(
      "{ \"id\": 42, \"type\": \"arangosearch\", \"view\": \"42\", "
      "\"includeAllFields\": true,\
    \"primarySort\":[{\"field\":\"sort\", \"direction\":\"asc\"}],\
    \"primarySortCompression\":\"test\",\
    \"storedValues\":[{\"fields\":[\"abc\"], \"compression\":\"test\"},\
                      {\"fields\":[\"abc2\"], \"compression\":\"lz4\"},\
                      {\"fields\":[\"ghi\"], \"compression\":\"test\"}]\
    }");
  auto collectionJson =
      arangodb::velocypack::Parser::fromJson(R"({ "name": "testCollection" })");
  auto viewJson = arangodb::velocypack::Parser::fromJson(
      "{ \
    \"id\": 42, \
    \"name\": \"testView\", \
    \"type\": \"arangosearch\", \
    \"primarySort\":[{\"field\":\"sort\", \"direction\":\"asc\"}],\
    \"primarySortCompression\":\"test\",\
    \"storedValues\":[{\"fields\":[\"abc\"], \"compression\":\"test\"},\
                      {\"fields\":[\"abc2\"], \"compression\":\"lz4\"},\
                      {\"fields\":[\"ghi\"], \"compression\":\"test\"}]\
  }");
  std::set<std::string> compressedValues;
  irs::compression::mock::test_compressor::functions().compress_mock =
      [&compressedValues](irs::byte_type* src, size_t size,
                          irs::bstring& out) -> irs::bytes_ref {
    out.append(src, size);
    compressedValues.emplace(reinterpret_cast<const char*>(src), size);
    return {reinterpret_cast<const irs::byte_type*>(out.data()), size};
  };
  auto compressorRemover = irs::make_finally([]() {
    irs::compression::mock::test_compressor::functions().compress_mock =
        nullptr;
  });
  auto logicalCollection = vocbase.createCollection(collectionJson->slice());
  ASSERT_TRUE((nullptr != logicalCollection));
  auto view = std::dynamic_pointer_cast<arangodb::iresearch::IResearchView>(
      vocbase.createView(viewJson->slice()));
  ASSERT_TRUE((false == !view));
  view->open();
  ASSERT_TRUE(server.server().hasFeature<arangodb::FlushFeature>());

  dataPath = ((((irs::utf8_path() /= testFilesystemPath) /=
                std::string("databases")) /=
               (std::string("database-") + std::to_string(vocbase.id()))) /=
              (std::string("arangosearch-") +
               std::to_string(logicalCollection->id().id()) + "_42"))
                 .string();
  irs::fs_directory directory(dataPath);
  bool created;
  auto link = logicalCollection->createIndex(linkJson->slice(), created);
  ASSERT_TRUE((false == !link && created));
  auto reader = irs::directory_reader::open(directory);
  EXPECT_EQ(0, reader.reopen().live_docs_count());
  {
    arangodb::transaction::Methods trx(
        arangodb::transaction::StandaloneContext::Create(vocbase), kEmpty,
        kEmpty, kEmpty, arangodb::transaction::Options());
    EXPECT_TRUE((trx.begin().ok()));
    auto* l = dynamic_cast<arangodb::iresearch::IResearchLink*>(link.get());
    ASSERT_TRUE(l != nullptr);
    EXPECT_TRUE(
        (l->insert(trx, arangodb::LocalDocumentId(1), doc0->slice()).ok()));
    EXPECT_TRUE((l->commit().ok()));
    EXPECT_EQ(0, reader.reopen().live_docs_count());

    EXPECT_TRUE((trx.commit().ok()));
    EXPECT_TRUE((l->commit().ok()));
  }

  EXPECT_EQ(1, reader.reopen().live_docs_count());

  {
    arangodb::transaction::Methods trx(
        arangodb::transaction::StandaloneContext::Create(vocbase), kEmpty,
        kEmpty, kEmpty, arangodb::transaction::Options());
    EXPECT_TRUE((trx.begin().ok()));
    auto* l = dynamic_cast<arangodb::iresearch::IResearchLink*>(link.get());
    ASSERT_TRUE(l != nullptr);
    EXPECT_TRUE(
        (l->insert(trx, arangodb::LocalDocumentId(2), doc1->slice()).ok()));
    EXPECT_TRUE((trx.commit().ok()));
    EXPECT_TRUE((l->commit().ok()));
  }

  EXPECT_EQ(2, reader.reopen().live_docs_count());
  std::set<std::string> expected;
  auto sortSlice = doc0->slice().get("sort");
  expected.emplace(reinterpret_cast<const char*>(sortSlice.start()),
                   sortSlice.byteSize());
  auto abcSlice = doc0->slice().get("abc");
  expected.emplace(reinterpret_cast<const char*>(abcSlice.start()),
                   abcSlice.byteSize());
  auto sortSlice1 = doc1->slice().get("sort");
  expected.emplace(reinterpret_cast<const char*>(sortSlice1.start()),
                   sortSlice1.byteSize());
  auto abc2Slice = doc1->slice().get("ghi");
  expected.emplace(reinterpret_cast<const char*>(abc2Slice.start()),
                   abc2Slice.byteSize());
  EXPECT_EQ(expected, compressedValues);
}

TEST_F(
    IResearchLinkTest,
    test_write_with_custom_compression_nondefault_mixed_with_sort_encrypted) {
  auto linkCallbackRemover =
      arangodb::iresearch::IResearchLinkMock::setCallbakForScope([]() {
        return irs::directory_attributes{
            0,
            std::make_unique<iresearch::mock::test_encryption>(kEncBlockSize)};
      });
  static std::vector<std::string> const kEmpty;
  auto doc0 = arangodb::velocypack::Parser::fromJson(
      R"({ "abc": "def", "abc2":"aaa", "sort":"ps" })");
  auto doc1 = arangodb::velocypack::Parser::fromJson(
      R"({ "ghi": "jkl", "sort":"pp" })");
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL,
                        testDBInfo(server.server()));
  std::string dataPath =
      ((((irs::utf8_path() /= testFilesystemPath) /=
         std::string("databases")) /=
        (std::string("database-") + std::to_string(vocbase.id()))) /=
       std::string("arangosearch-42"))
          .string();
  auto linkJson = arangodb::velocypack::Parser::fromJson(
      "{ \"id\": 42, \"type\": \"arangosearch\", \"view\": \"42\", "
      "\"includeAllFields\": true,\
    \"primarySort\":[{\"field\":\"sort\", \"direction\":\"asc\"}],\
    \"primarySortCompression\":\"test\",\
    \"storedValues\":[{\"fields\":[\"abc\"], \"compression\":\"test\"},\
                      {\"fields\":[\"abc2\"], \"compression\":\"lz4\"},\
                      {\"fields\":[\"ghi\"], \"compression\":\"test\"}]\
    }");
  auto collectionJson =
      arangodb::velocypack::Parser::fromJson(R"({ "name": "testCollection" })");
  auto viewJson = arangodb::velocypack::Parser::fromJson(
      "{ \
    \"id\": 42, \
    \"name\": \"testView\", \
    \"type\": \"arangosearch\", \
    \"primarySort\":[{\"field\":\"sort\", \"direction\":\"asc\"}],\
    \"primarySortCompression\":\"test\",\
    \"storedValues\":[{\"fields\":[\"abc\"], \"compression\":\"test\"},\
                      {\"fields\":[\"abc2\"], \"compression\":\"lz4\"},\
                      {\"fields\":[\"ghi\"], \"compression\":\"test\"}]\
  }");
  std::set<std::string> compressedValues;
  irs::compression::mock::test_compressor::functions().compress_mock =
      [&compressedValues](irs::byte_type* src, size_t size,
                          irs::bstring& out) -> irs::bytes_ref {
    out.append(src, size);
    compressedValues.emplace(reinterpret_cast<const char*>(src), size);
    return {reinterpret_cast<const irs::byte_type*>(out.data()), size};
  };
  auto compressorRemover = irs::make_finally([]() {
    irs::compression::mock::test_compressor::functions().compress_mock =
        nullptr;
  });
  auto logicalCollection = vocbase.createCollection(collectionJson->slice());
  ASSERT_TRUE((nullptr != logicalCollection));
  auto view = std::dynamic_pointer_cast<arangodb::iresearch::IResearchView>(
      vocbase.createView(viewJson->slice()));
  ASSERT_TRUE((false == !view));
  view->open();
  ASSERT_TRUE(server.server().hasFeature<arangodb::FlushFeature>());

  dataPath = ((((irs::utf8_path() /= testFilesystemPath) /=
                std::string("databases")) /=
               (std::string("database-") + std::to_string(vocbase.id()))) /=
              (std::string("arangosearch-") +
               std::to_string(logicalCollection->id().id()) + "_42"))
                 .string();
  irs::fs_directory directory(
      dataPath, irs::directory_attributes(
                    0, std::make_unique<iresearch::mock::test_encryption>(
                           kEncBlockSize)));

  bool created;
  auto link = logicalCollection->createIndex(linkJson->slice(), created);
  ASSERT_TRUE((false == !link && created));
  auto reader = irs::directory_reader::open(directory);
  EXPECT_EQ(0, reader.reopen().live_docs_count());
  {
    arangodb::transaction::Methods trx(
        arangodb::transaction::StandaloneContext::Create(vocbase), kEmpty,
        kEmpty, kEmpty, arangodb::transaction::Options());
    EXPECT_TRUE((trx.begin().ok()));
    auto* l = dynamic_cast<arangodb::iresearch::IResearchLink*>(link.get());
    ASSERT_TRUE(l != nullptr);
    EXPECT_TRUE(
        (l->insert(trx, arangodb::LocalDocumentId(1), doc0->slice()).ok()));
    EXPECT_TRUE((l->commit().ok()));
    EXPECT_EQ(0, reader.reopen().live_docs_count());

    EXPECT_TRUE((trx.commit().ok()));
    EXPECT_TRUE((l->commit().ok()));
  }

  EXPECT_EQ(1, reader.reopen().live_docs_count());

  {
    arangodb::transaction::Methods trx(
        arangodb::transaction::StandaloneContext::Create(vocbase), kEmpty,
        kEmpty, kEmpty, arangodb::transaction::Options());
    EXPECT_TRUE((trx.begin().ok()));
    auto* l = dynamic_cast<arangodb::iresearch::IResearchLink*>(link.get());
    ASSERT_TRUE(l != nullptr);
    EXPECT_TRUE(
        (l->insert(trx, arangodb::LocalDocumentId(2), doc1->slice()).ok()));
    EXPECT_TRUE((trx.commit().ok()));
    EXPECT_TRUE((l->commit().ok()));
  }

  EXPECT_EQ(2, reader.reopen().live_docs_count());
  std::set<std::string> expected;
  auto sortSlice = doc0->slice().get("sort");
  expected.emplace(reinterpret_cast<const char*>(sortSlice.start()),
                   sortSlice.byteSize());
  auto abcSlice = doc0->slice().get("abc");
  expected.emplace(reinterpret_cast<const char*>(abcSlice.start()),
                   abcSlice.byteSize());
  auto sortSlice1 = doc1->slice().get("sort");
  expected.emplace(reinterpret_cast<const char*>(sortSlice1.start()),
                   sortSlice1.byteSize());
  auto abc2Slice = doc1->slice().get("ghi");
  expected.emplace(reinterpret_cast<const char*>(abc2Slice.start()),
                   abc2Slice.byteSize());
  EXPECT_EQ(expected, compressedValues);
}

TEST_F(IResearchLinkTest, test_maintenance_disabled_at_creation) {
  using namespace arangodb;
  using namespace arangodb::iresearch;

  std::mutex mtx;
  std::condition_variable cv;
  auto& feature = server.getFeature<IResearchFeature>();

  auto blockQueue = [&]() {
    { auto lock = irs::make_lock_guard(mtx); }
    cv.notify_one();
  };

  {
    // assume 10s is more than enough to finish initialization tasks
    auto const end = std::chrono::steady_clock::now() + 10s;
    while (std::get<0>(feature.stats(ThreadGroup::_0)) ||
           std::get<0>(feature.stats(ThreadGroup::_1))) {
      std::this_thread::sleep_for(10ms);
      ASSERT_LE(std::chrono::steady_clock::now(), end);
    }
  }

  ASSERT_EQ(std::make_tuple(size_t(0), size_t(0), size_t(1)),
            feature.stats(ThreadGroup::_0));
  ASSERT_EQ(std::make_tuple(size_t(0), size_t(0), size_t(1)),
            feature.stats(ThreadGroup::_1));

  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL,
                        testDBInfo(server.server()));
  auto collectionJson = VPackParser::fromJson(R"({
    "name": "testCollection" })");
  auto linkJson = VPackParser::fromJson(R"({
    "id": 42, "view": "42",
    "type": "arangosearch" })");
  auto viewJson = VPackParser::fromJson(R"({
    "id": 42, "name": "testView",
    "type": "arangosearch",
    "consolidationIntervalMsec": 0,
    "commitIntervalMsec": 0})");

  std::shared_ptr<arangodb::Index> link;
  auto logicalCollection = vocbase.createCollection(collectionJson->slice());
  ASSERT_NE(nullptr, logicalCollection);
  auto view = std::dynamic_pointer_cast<IResearchView>(
      vocbase.createView(viewJson->slice()));
  ASSERT_NE(nullptr, view);
  view->open();
  ASSERT_TRUE(server.server().hasFeature<FlushFeature>());

  ASSERT_EQ(std::make_tuple(size_t(0), size_t(0), size_t(1)),
            feature.stats(ThreadGroup::_0));
  ASSERT_EQ(std::make_tuple(size_t(0), size_t(0), size_t(1)),
            feature.stats(ThreadGroup::_1));

  // block queues
  {
    auto lock = irs::make_unique_lock(mtx);
    ASSERT_TRUE(feature.queue(ThreadGroup::_0, 0ms, blockQueue));
    ASSERT_TRUE(feature.queue(ThreadGroup::_1, 0ms, blockQueue));

    bool created;
    link = logicalCollection->createIndex(linkJson->slice(), created);
    ASSERT_TRUE(created);
    ASSERT_NE(nullptr, link);

    ASSERT_EQ(std::make_tuple(size_t(1), size_t(0), size_t(1)),
              feature.stats(ThreadGroup::_0));
    ASSERT_EQ(std::make_tuple(size_t(1), size_t(0), size_t(1)),
              feature.stats(ThreadGroup::_1));
  }

  ASSERT_TRUE(link->drop().ok());
  ASSERT_TRUE(view->drop().ok());
  ASSERT_TRUE(logicalCollection->drop().ok());

  auto const end = std::chrono::steady_clock::now() + 10s;
  while (std::get<0>(feature.stats(ThreadGroup::_0)) ||
         std::get<0>(feature.stats(ThreadGroup::_1))) {
    std::this_thread::sleep_for(10ms);
    ASSERT_LE(std::chrono::steady_clock::now(), end);
  }

  ASSERT_EQ(std::make_tuple(size_t(0), size_t(0), size_t(1)),
            feature.stats(ThreadGroup::_0));
  ASSERT_EQ(std::make_tuple(size_t(0), size_t(0), size_t(1)),
            feature.stats(ThreadGroup::_1));
}

TEST_F(IResearchLinkTest, test_maintenance_consolidation) {
  using namespace arangodb;
  using namespace arangodb::iresearch;

  std::mutex mtx;
  std::condition_variable cv;
  auto& feature = server.getFeature<IResearchFeature>();

  std::atomic<size_t> step{0};
  auto blockQueue = [&]() {
    ++step;
    { auto lock = irs::make_lock_guard(mtx); }
    cv.notify_one();
  };

  size_t expectedStep = 0;
  auto waitForBlocker = [&](std::chrono::steady_clock::duration timeout = 10s) {
    ++expectedStep;

    auto const end = std::chrono::steady_clock::now() + timeout;
    while (expectedStep != step) {
      std::this_thread::sleep_for(10ms);
      ASSERT_LE(std::chrono::steady_clock::now(), end);
      ASSERT_LE(step, expectedStep);
    }
  };

  {
    // assume 10s is more than enough to finish initialization tasks
    auto const end = std::chrono::steady_clock::now() + 10s;
    while (std::get<0>(feature.stats(ThreadGroup::_0)) ||
           std::get<0>(feature.stats(ThreadGroup::_1))) {
      std::this_thread::sleep_for(10ms);
      ASSERT_LE(std::chrono::steady_clock::now(), end);
    }
  }

  ASSERT_EQ(std::make_tuple(size_t(0), size_t(0), size_t(1)),
            feature.stats(ThreadGroup::_0));
  ASSERT_EQ(std::make_tuple(size_t(0), size_t(0), size_t(1)),
            feature.stats(ThreadGroup::_1));

  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL,
                        testDBInfo(server.server()));
  auto collectionJson = VPackParser::fromJson(R"({
    "name": "testCollection" })");
  auto linkJson = VPackParser::fromJson(R"({
    "id": 42, "view": "42",
    "type": "arangosearch" })");
  auto viewJson = VPackParser::fromJson(R"({
    "id": 42, "name": "testView",
    "type": "arangosearch",
    "consolidationIntervalMsec": 50,
    "commitIntervalMsec": 0 })");

  auto logicalCollection = vocbase.createCollection(collectionJson->slice());
  ASSERT_NE(nullptr, logicalCollection);
  auto view = std::dynamic_pointer_cast<IResearchView>(
      vocbase.createView(viewJson->slice()));
  ASSERT_NE(nullptr, view);
  view->open();
  ASSERT_TRUE(server.server().hasFeature<FlushFeature>());

  ASSERT_EQ(std::make_tuple(size_t(0), size_t(0), size_t(1)),
            feature.stats(ThreadGroup::_0));
  ASSERT_EQ(std::make_tuple(size_t(0), size_t(0), size_t(1)),
            feature.stats(ThreadGroup::_1));

  // block queue
  {
    auto lock = irs::make_unique_lock(mtx);
    ASSERT_TRUE(feature.queue(ThreadGroup::_1, 0ms, blockQueue));
    waitForBlocker();

    bool created;
    auto link = logicalCollection->createIndex(linkJson->slice(), created);
    ASSERT_TRUE(created);
    ASSERT_NE(nullptr, link);
    auto linkImpl = std::dynamic_pointer_cast<IResearchLink>(link);
    ASSERT_NE(nullptr, linkImpl);
    auto asyncSelf = linkImpl->self();
    ASSERT_NE(nullptr, asyncSelf);

    // ensure consolidation is scheduled upon link creation
    {
      ASSERT_EQ(std::make_tuple(size_t(1), size_t(1), size_t(1)),
                feature.stats(ThreadGroup::_1));
      ASSERT_TRUE(feature.queue(ThreadGroup::_1, 500ms, blockQueue));
      ASSERT_EQ(std::make_tuple(size_t(1), size_t(2), size_t(1)),
                feature.stats(ThreadGroup::_1));

      cv.wait(lock);     // release current blocker
      waitForBlocker();  // wait for the next blocker
      ASSERT_EQ(std::make_tuple(size_t(1), size_t(1), size_t(1)),
                feature.stats(ThreadGroup::_1));
    }

    // disable/enable consolidation via interval
    {
      IResearchViewMeta meta;
      meta._consolidationIntervalMsec = 0;
      ASSERT_TRUE(linkImpl->properties(meta).ok());

      // don't schedule new task as commitIntervalMsec is set to 0
      ASSERT_EQ(std::make_tuple(size_t(1), size_t(1), size_t(1)),
                feature.stats(ThreadGroup::_1));
      ASSERT_TRUE(feature.queue(ThreadGroup::_1, 500ms, blockQueue));
      ASSERT_EQ(std::make_tuple(size_t(1), size_t(2), size_t(1)),
                feature.stats(ThreadGroup::_1));

      cv.wait(lock);
      waitForBlocker();

      // ensure nothing is scheduled as commit is turned off
      ASSERT_EQ(std::make_tuple(size_t(1), size_t(0), size_t(1)),
                feature.stats(ThreadGroup::_1));

      // reschedule task
      meta._consolidationIntervalMsec = 50;
      ASSERT_TRUE(linkImpl->properties(meta).ok());

      ASSERT_EQ(std::make_tuple(size_t(1), size_t(1), size_t(1)),
                feature.stats(ThreadGroup::_1));
      ASSERT_TRUE(feature.queue(ThreadGroup::_1, 500ms, blockQueue));
      ASSERT_EQ(std::make_tuple(size_t(1), size_t(2), size_t(1)),
                feature.stats(ThreadGroup::_1));

      cv.wait(lock);
      waitForBlocker();  // wait for the next blocker

      ASSERT_EQ(std::make_tuple(size_t(1), size_t(1), size_t(1)),
                feature.stats(ThreadGroup::_1));
    }

    // disable/enable consolidation via policy
    {
      IResearchViewMeta meta;
      meta._consolidationPolicy = {};
      meta._commitIntervalMsec = 0;
      ASSERT_TRUE(linkImpl->properties(meta).ok());

      // don't schedule new task as commitIntervalMsec is set to 0
      ASSERT_EQ(std::make_tuple(size_t(1), size_t(1), size_t(1)),
                feature.stats(ThreadGroup::_1));
      ASSERT_TRUE(feature.queue(ThreadGroup::_1, 500ms, blockQueue));
      ASSERT_EQ(std::make_tuple(size_t(1), size_t(2), size_t(1)),
                feature.stats(ThreadGroup::_1));

      cv.wait(lock);
      waitForBlocker();

      // ensure nothing is scheduled as commit is turned off
      ASSERT_EQ(std::make_tuple(size_t(1), size_t(0), size_t(1)),
                feature.stats(ThreadGroup::_1));

      // reschedule task
      meta._consolidationPolicy =
          IResearchViewMeta::DEFAULT()._consolidationPolicy;
      meta._commitIntervalMsec = 0;
      ASSERT_TRUE(linkImpl->properties(meta).ok());

      ASSERT_EQ(std::make_tuple(size_t(1), size_t(1), size_t(1)),
                feature.stats(ThreadGroup::_1));
      ASSERT_TRUE(feature.queue(ThreadGroup::_1, 500ms, blockQueue));
      ASSERT_EQ(std::make_tuple(size_t(1), size_t(2), size_t(1)),
                feature.stats(ThreadGroup::_1));

      cv.wait(lock);
      waitForBlocker();  // wait for the next blocker

      ASSERT_EQ(std::make_tuple(size_t(1), size_t(1), size_t(1)),
                feature.stats(ThreadGroup::_1));
    }

#ifdef ARANGODB_ENABLE_FAILURE_TESTS
    // ensure consolidation is rescheduled after exception
    {
      auto clearFailurePoints =
          arangodb::scopeGuard(TRI_ClearFailurePointsDebugging);
      TRI_AddFailurePointDebugging("IResearchConsolidationTask::lockDataStore");

      ASSERT_EQ(std::make_tuple(size_t(1), size_t(1), size_t(1)),
                feature.stats(ThreadGroup::_1));
      ASSERT_TRUE(feature.queue(ThreadGroup::_1, 500ms, blockQueue));
      ASSERT_EQ(std::make_tuple(size_t(1), size_t(2), size_t(1)),
                feature.stats(ThreadGroup::_1));

      cv.wait(lock);
      waitForBlocker();  // wait for the next blocker

      ASSERT_EQ(std::make_tuple(size_t(1), size_t(1), size_t(1)),
                feature.stats(ThreadGroup::_1));
    }

    // ensure consolidation is rescheduled after exception
    {
      auto clearFailurePoints =
          arangodb::scopeGuard(TRI_ClearFailurePointsDebugging);
      TRI_AddFailurePointDebugging(
          "IResearchConsolidationTask::consolidateUnsafe");

      ASSERT_EQ(std::make_tuple(size_t(1), size_t(1), size_t(1)),
                feature.stats(ThreadGroup::_1));
      ASSERT_TRUE(feature.queue(ThreadGroup::_1, 500ms, blockQueue));
      ASSERT_EQ(std::make_tuple(size_t(1), size_t(2), size_t(1)),
                feature.stats(ThreadGroup::_1));

      cv.wait(lock);
      waitForBlocker();  // wait for the next blocker

      ASSERT_EQ(std::make_tuple(size_t(1), size_t(1), size_t(1)),
                feature.stats(ThreadGroup::_1));
    }
#endif

    // ensure no commit is scheduled after dropping a link
    {
      ASSERT_TRUE(link->drop().ok());
      ASSERT_TRUE(asyncSelf->terminationRequested());

      ASSERT_TRUE(cv.wait_for(lock, 10s, [&feature]() {
        return std::make_tuple(size_t(0), size_t(0), size_t(1)) ==
               feature.stats(ThreadGroup::_1);
      }));
    }
  }

  ASSERT_TRUE(view->drop().ok());
  ASSERT_TRUE(logicalCollection->drop().ok());
}

TEST_F(IResearchLinkTest, test_maintenance_commit) {
  using namespace arangodb;
  using namespace arangodb::iresearch;

  std::mutex mtx;
  std::condition_variable cv;
  auto& feature = server.getFeature<IResearchFeature>();

  std::atomic<size_t> step{0};
  auto blockQueue = [&]() {
    ++step;
    { auto lock = irs::make_lock_guard(mtx); }
    cv.notify_one();
  };

  size_t expectedStep = 0;
  auto waitForBlocker = [&](std::chrono::steady_clock::duration timeout = 10s) {
    ++expectedStep;

    auto const end = std::chrono::steady_clock::now() + timeout;
    while (expectedStep != step) {
      std::this_thread::sleep_for(10ms);
      ASSERT_LE(std::chrono::steady_clock::now(), end);
      ASSERT_LE(step, expectedStep);
    }
  };

  {
    // assume 10s is more than enough to finish initialization tasks
    auto const end = std::chrono::steady_clock::now() + 10s;
    while (std::get<0>(feature.stats(ThreadGroup::_0)) ||
           std::get<0>(feature.stats(ThreadGroup::_1))) {
      std::this_thread::sleep_for(10ms);
      ASSERT_LE(std::chrono::steady_clock::now(), end);
    }
  }

  ASSERT_EQ(std::make_tuple(size_t(0), size_t(0), size_t(1)),
            feature.stats(ThreadGroup::_0));
  ASSERT_EQ(std::make_tuple(size_t(0), size_t(0), size_t(1)),
            feature.stats(ThreadGroup::_1));

  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL,
                        testDBInfo(server.server()));
  auto collectionJson = VPackParser::fromJson(R"({
    "name": "testCollection" })");
  auto linkJson = VPackParser::fromJson(R"({
    "id": 42, "view": "42",
    "type": "arangosearch" })");
  auto viewJson = VPackParser::fromJson(R"({
    "id": 42, "name": "testView",
    "type": "arangosearch",
    "consolidationIntervalMsec": 0,
    "commitIntervalMsec": 50 })");

  auto logicalCollection = vocbase.createCollection(collectionJson->slice());
  ASSERT_NE(nullptr, logicalCollection);
  auto view = std::dynamic_pointer_cast<IResearchView>(
      vocbase.createView(viewJson->slice()));
  ASSERT_NE(nullptr, view);
  view->open();
  ASSERT_TRUE(server.server().hasFeature<FlushFeature>());

  ASSERT_EQ(std::make_tuple(size_t(0), size_t(0), size_t(1)),
            feature.stats(ThreadGroup::_0));
  ASSERT_EQ(std::make_tuple(size_t(0), size_t(0), size_t(1)),
            feature.stats(ThreadGroup::_1));

  // block queue
  {
    auto lock = irs::make_unique_lock(mtx);
    ASSERT_TRUE(feature.queue(ThreadGroup::_0, 0ms, blockQueue));
    waitForBlocker();

    bool created;
    auto link = logicalCollection->createIndex(linkJson->slice(), created);
    ASSERT_TRUE(created);
    ASSERT_NE(nullptr, link);
    auto linkImpl = std::dynamic_pointer_cast<IResearchLink>(link);
    ASSERT_NE(nullptr, linkImpl);
    auto asyncSelf = linkImpl->self();
    ASSERT_NE(nullptr, asyncSelf);

    // ensure commit is scheduled upon link creation
    {
      ASSERT_EQ(std::make_tuple(size_t(1), size_t(1), size_t(1)),
                feature.stats(ThreadGroup::_0));
      ASSERT_TRUE(feature.queue(ThreadGroup::_0, 500ms, blockQueue));
      ASSERT_EQ(std::make_tuple(size_t(1), size_t(2), size_t(1)),
                feature.stats(ThreadGroup::_0));

      cv.wait(lock);     // release current blocker
      waitForBlocker();  // wait for the next blocker
      ASSERT_EQ(std::make_tuple(size_t(1), size_t(1), size_t(1)),
                feature.stats(ThreadGroup::_0));
    }

    // disable/enable commit
    {
      IResearchViewMeta meta;
      meta._commitIntervalMsec = 0;
      ASSERT_TRUE(linkImpl->properties(meta).ok());

      // don't schedule new task as commitIntervalMsec is set to 0
      ASSERT_EQ(std::make_tuple(size_t(1), size_t(1), size_t(1)),
                feature.stats(ThreadGroup::_0));
      ASSERT_TRUE(feature.queue(ThreadGroup::_0, 500ms, blockQueue));
      ASSERT_EQ(std::make_tuple(size_t(1), size_t(2), size_t(1)),
                feature.stats(ThreadGroup::_0));

      cv.wait(lock);
      waitForBlocker();

      // ensure nothing is scheduled as commit is turned off
      ASSERT_EQ(std::make_tuple(size_t(1), size_t(0), size_t(1)),
                feature.stats(ThreadGroup::_0));

      // reschedule task
      meta._commitIntervalMsec = 50;
      ASSERT_TRUE(linkImpl->properties(meta).ok());

      ASSERT_EQ(std::make_tuple(size_t(1), size_t(1), size_t(1)),
                feature.stats(ThreadGroup::_0));
      ASSERT_TRUE(feature.queue(ThreadGroup::_0, 500ms, blockQueue));
      ASSERT_EQ(std::make_tuple(size_t(1), size_t(2), size_t(1)),
                feature.stats(ThreadGroup::_0));

      cv.wait(lock);
      waitForBlocker();  // wait for the next blocker

      ASSERT_EQ(std::make_tuple(size_t(1), size_t(1), size_t(1)),
                feature.stats(ThreadGroup::_0));
    }

#ifdef ARANGODB_ENABLE_FAILURE_TESTS
    // ensure commit is rescheduled after exception
    {
      auto clearFailurePoints =
          arangodb::scopeGuard(TRI_ClearFailurePointsDebugging);
      TRI_AddFailurePointDebugging("IResearchCommitTask::lockDataStore");

      ASSERT_EQ(std::make_tuple(size_t(1), size_t(1), size_t(1)),
                feature.stats(ThreadGroup::_0));
      ASSERT_TRUE(feature.queue(ThreadGroup::_0, 500ms, blockQueue));
      ASSERT_EQ(std::make_tuple(size_t(1), size_t(2), size_t(1)),
                feature.stats(ThreadGroup::_0));

      cv.wait(lock);
      waitForBlocker();  // wait for the next blocker

      ASSERT_EQ(std::make_tuple(size_t(1), size_t(1), size_t(1)),
                feature.stats(ThreadGroup::_0));
    }

    // ensure commit is rescheduled after exception
    {
      auto clearFailurePoints =
          arangodb::scopeGuard(TRI_ClearFailurePointsDebugging);
      TRI_AddFailurePointDebugging("IResearchCommitTask::commitUnsafe");

      ASSERT_EQ(std::make_tuple(size_t(1), size_t(1), size_t(1)),
                feature.stats(ThreadGroup::_0));
      ASSERT_TRUE(feature.queue(ThreadGroup::_0, 500ms, blockQueue));
      ASSERT_EQ(std::make_tuple(size_t(1), size_t(2), size_t(1)),
                feature.stats(ThreadGroup::_0));

      cv.wait(lock);
      waitForBlocker();  // wait for the next blocker

      ASSERT_EQ(std::make_tuple(size_t(1), size_t(1), size_t(1)),
                feature.stats(ThreadGroup::_0));
    }

    // ensure commit is rescheduled after exception
    {
      auto clearFailurePoints =
          arangodb::scopeGuard(TRI_ClearFailurePointsDebugging);
      TRI_AddFailurePointDebugging("IResearchCommitTask::cleanupUnsafe");

      ASSERT_EQ(std::make_tuple(size_t(1), size_t(1), size_t(1)),
                feature.stats(ThreadGroup::_0));
      ASSERT_TRUE(feature.queue(ThreadGroup::_0, 500ms, blockQueue));
      ASSERT_EQ(std::make_tuple(size_t(1), size_t(2), size_t(1)),
                feature.stats(ThreadGroup::_0));

      cv.wait(lock);
      waitForBlocker();  // wait for the next blocker

      ASSERT_EQ(std::make_tuple(size_t(1), size_t(1), size_t(1)),
                feature.stats(ThreadGroup::_0));
    }
#endif

    // ensure no commit is scheduled after dropping a link
    {
      ASSERT_TRUE(link->drop().ok());
      ASSERT_TRUE(asyncSelf->terminationRequested());

      ASSERT_TRUE(cv.wait_for(lock, 10s, [&feature]() {
        return std::make_tuple(size_t(0), size_t(0), size_t(1)) ==
               feature.stats(ThreadGroup::_0);
      }));
    }
  }

  ASSERT_TRUE(view->drop().ok());
  ASSERT_TRUE(logicalCollection->drop().ok());
}

void getStatsFromFolder(std::string_view path, uint64_t& indexSize,
                        uint64_t& numFiles) {
  irs::utf8_path utf8Path{path};
  auto visitor = [&indexSize, &numFiles,
                  &utf8Path](const path_char_t* filename) -> bool {
    auto pathParts = irs::file_utils::path_parts(filename);
    std::match_results<typename std::basic_string<path_char_t>::const_iterator>
        match;
    std::basic_regex<path_char_t> regex([] {
      if constexpr (std::is_same_v<path_char_t, wchar_t>) {
        return L"^_\\d+$";
      } else {
        return "^_\\d+$";
      }
    }());
    std::basic_string<path_char_t> name(pathParts.stem.c_str(),
                                        pathParts.stem.size());

    if (std::regex_match(name, match, regex)) {
      // creating abs path to current file
      irs::utf8_path absPath = utf8Path;
      absPath /= pathParts.basename.c_str();
      uint64_t currSize = 0;
      irs::file_utils::byte_size(currSize, absPath.c_str());
      indexSize += currSize;
      ++numFiles;
    }
    return true;
  };
  iresearch::file_utils::visit_directory(utf8Path.c_str(), visitor, false);
}

using LinkStats = arangodb::iresearch::IResearchLink::LinkStats;

bool operator==(const LinkStats& lhs, const LinkStats& rhs) noexcept {
  // ignore numBufferedDocs
  return lhs.numDocs == rhs.numDocs && lhs.numLiveDocs == rhs.numLiveDocs &&
         lhs.numSegments == rhs.numSegments && lhs.numFiles == rhs.numFiles &&
         lhs.indexSize == rhs.indexSize;
}

static std::vector<std::string> const kEmpty;

class IResearchLinkMetricsTest : public IResearchLinkTest {
 protected:
  TRI_vocbase_t _vocbase{TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL,
                         testDBInfo(server.server())};
  std::array<std::shared_ptr<arangodb::velocypack::Builder>, 3> _docs;
  std::shared_ptr<arangodb::LogicalCollection> _logicalCollection;
  uint64_t _cleanupIntervalStep = 0;
  uint64_t _commitIntervalMs = 0;
  uint64_t _consolidationIntervalMs = 0;
  std::shared_ptr<arangodb::LogicalView> _view;
  irs::utf8_path _dirPath = testFilesystemPath;
  std::shared_ptr<arangodb::Index> _link;

  IResearchLinkMetricsTest() {
    _docs[0] = arangodb::velocypack::Parser::fromJson(R"({ "abc": "def" })");
    _docs[1] = arangodb::velocypack::Parser::fromJson(R"({ "ghia": "jkla" })");
    _docs[2] = arangodb::velocypack::Parser::fromJson(R"({ "1234": "56789" })");

    auto collectionJson = arangodb::velocypack::Parser::fromJson(
        R"({ "name": "testCollection" })");
    _logicalCollection = _vocbase.createCollection(collectionJson->slice());

    EXPECT_NE(_logicalCollection, nullptr);

    _dirPath /= "databases";
    _dirPath /= "database-" + std::to_string(_vocbase.id());
    _dirPath /=
        "arangosearch-" + std::to_string(_logicalCollection->id().id()) + "_42";
  }

  ~IResearchLinkMetricsTest() override { resetLink(); }

  bool checkMetricExist(std::string_view name, std::string_view label) const {
    arangodb::metrics::MetricKey key(name, label);
    auto* metric =
        _vocbase.server().getFeature<arangodb::metrics::MetricsFeature>().get(
            key);
    return metric != nullptr;
  }

  void setLink() {
    char temp[1000];
    sprintf(temp, R"({
      "id": 42,
      "name": "testView",
      "cleanupIntervalStep": %llu,
      "commitIntervalMsec": %llu,
      "consolidationIntervalMsec": %llu,
      "type": "arangosearch"
    })",
            static_cast<long long unsigned>(_cleanupIntervalStep),
            static_cast<long long unsigned>(_commitIntervalMs),
            static_cast<long long unsigned>(_consolidationIntervalMs));
    auto viewJson = arangodb::velocypack::Parser::fromJson(temp);

    _view = std::dynamic_pointer_cast<arangodb::iresearch::IResearchView>(
        _vocbase.createView(viewJson->slice()));

    EXPECT_NE(_view, nullptr);
    _view->open();

    EXPECT_TRUE(server.server().hasFeature<arangodb::FlushFeature>());

    bool created;
    auto linkJson = arangodb::velocypack::Parser::fromJson(
        R"({
      "id": 42,
      "type": "arangosearch",
      "view": "42",
      "includeAllFields": true
    })");
    _link = _logicalCollection->createIndex(linkJson->slice(), created);
    EXPECT_TRUE(created);
    EXPECT_NE(_link, nullptr);
    auto label = getLinkMetricLabel();
    EXPECT_TRUE(checkMetricExist("arangosearch_link_stats", label));
    EXPECT_TRUE(checkMetricExist("arangosearch_num_failed_commits", label));
    EXPECT_TRUE(checkMetricExist("arangosearch_num_failed_cleanups", label));
    EXPECT_TRUE(
        checkMetricExist("arangosearch_num_failed_consolidations", label));
    EXPECT_TRUE(checkMetricExist("arangosearch_commit_time", label));
    EXPECT_TRUE(checkMetricExist("arangosearch_cleanup_time", label));
    EXPECT_TRUE(checkMetricExist("arangosearch_consolidation_time", label));
  }

  void resetLink() {
    if (_link) {
      _logicalCollection->dropIndex(_link->id());
      _link.reset();
    }
    _view.reset();
  }

  arangodb::iresearch::IResearchLink* getLink() {
    auto* l = dynamic_cast<arangodb::iresearch::IResearchLink*>(_link.get());
    EXPECT_NE(l, nullptr);
    return l;
  }

  std::string getLinkMetricLabel() {
    auto* l = getLink();
    std::string label;
    label += "view=\"" + l->getViewId() + "\",";
    label += "collection=\"" + l->getCollectionName() + "\",";
    label += "shard=\"" + l->getShardName() + "\",";
    label += "db=\"" + l->getDbName() + "\"";
    return label;
  }

  void getPrometheusStr(std::string& result) {
    auto label = getLinkMetricLabel();
    arangodb::metrics::MetricKey key("arangosearch_link_stats", label);
    auto* metric =
        _vocbase.server().getFeature<arangodb::metrics::MetricsFeature>().get(
            key);
    if (metric != nullptr) {
      metric->toPrometheus(result, "", "");
    }
  }

  double insert(uint64_t begin, uint64_t end, size_t docId,
                bool commit = true) {
    auto* l = getLink();
    arangodb::transaction::Methods trx(
        arangodb::transaction::StandaloneContext::Create(_vocbase), kEmpty,
        kEmpty, kEmpty, arangodb::transaction::Options());
    EXPECT_TRUE(trx.begin().ok());
    for (; begin != end; ++begin) {
      EXPECT_TRUE(l->insert(trx, arangodb::LocalDocumentId(begin),
                            _docs[docId]->slice())
                      .ok());
    }
    EXPECT_TRUE(trx.commit().ok());
    if (commit) {
      auto start = std::chrono::steady_clock::now();
      EXPECT_TRUE(l->commit().ok());
      return std::chrono::duration<double, std::milli>(
                 std::chrono::steady_clock::now() - start)
          .count();
    }
    return NAN;
  }

  double remove(uint64_t begin, uint64_t end) {
    auto* l = getLink();
    arangodb::transaction::Methods trx(
        arangodb::transaction::StandaloneContext::Create(_vocbase), kEmpty,
        kEmpty, kEmpty, arangodb::transaction::Options());
    EXPECT_TRUE(trx.begin().ok());
    for (; begin != end; ++begin) {
      EXPECT_TRUE(
          l->remove(trx, arangodb::LocalDocumentId(begin), _docs[0]->slice())
              .ok());
    }

    EXPECT_TRUE(trx.commit().ok());
    auto start = std::chrono::steady_clock::now();
    EXPECT_TRUE(l->commit().ok());
    return std::chrono::duration<double, std::milli>(
               std::chrono::steady_clock::now() - start)
        .count();
  }

  std::tuple<uint64_t, uint64_t> numFiles() {
    uint64_t numFiles{0};
    uint64_t indexSize{0};
    auto visitor = [&](const path_char_t* filename) -> bool {
      ++numFiles;
      auto pathParts = irs::file_utils::path_parts(filename);
      irs::utf8_path absPath = _dirPath;
      absPath /= pathParts.basename.c_str();
      uint64_t currSize = 0;
      irs::file_utils::byte_size(currSize, absPath.c_str());
      indexSize += currSize;
      return true;
    };
    iresearch::file_utils::visit_directory(_dirPath.c_str(), visitor, false);
    return {numFiles, indexSize};
  }
};

TEST_F(IResearchLinkMetricsTest, TimeCommit) {
  _cleanupIntervalStep = 1;
  setLink();
  auto* l = getLink();
  double timeMs;
  {
    timeMs = insert(1, 10000, 0);
    auto [commitTime1, cleanupTime1, consolidationTime1] = l->avgTime();
    EXPECT_LT(0, commitTime1);
    EXPECT_EQ(0, cleanupTime1);
    EXPECT_LE(commitTime1, timeMs);
    timeMs += insert(10000, 10100, 1);
    auto [commitTime2, cleanupTime2, consolidationTime2] = l->avgTime();
    EXPECT_LT(0, commitTime2);
    EXPECT_EQ(0, cleanupTime1);
    EXPECT_LE(commitTime2, timeMs / 2.0);
  }
  {
    auto [numFiles0, indexSize0] = numFiles();
    EXPECT_LT(0, numFiles0);
    EXPECT_LT(0, indexSize0);

    timeMs += remove(1, 10000);
    auto [commitTime1, cleanupTime1, consolidationTime1] = l->avgTime();
    EXPECT_LE(commitTime1, timeMs / 3.0);
    EXPECT_LE(cleanupTime1, timeMs / 3.0);
    EXPECT_LE(commitTime1 + cleanupTime1, timeMs / 3.0);
    auto [numFiles1, indexSize1] = numFiles();
    EXPECT_LT(0, numFiles1);
    EXPECT_LT(numFiles1, numFiles0);
    EXPECT_LT(0, indexSize1);
    EXPECT_LT(indexSize1, indexSize0);

    timeMs += remove(10000, 10100);
    auto [commitTime2, cleanupTime2, consolidationTime2] = l->avgTime();
    EXPECT_LE(commitTime2, timeMs / 4.0);
    EXPECT_LE(cleanupTime2, timeMs / 4.0);
    EXPECT_LE(commitTime2 + cleanupTime2, timeMs / 4.0);
    auto [numFiles2, indexSize2] = numFiles();
    EXPECT_LT(0, numFiles2);
    EXPECT_LT(numFiles2, numFiles1);
    EXPECT_LT(0, indexSize2);
    EXPECT_LT(indexSize2, indexSize1);
  }
}

TEST_F(IResearchLinkMetricsTest, TimeConsolidate) {
  _cleanupIntervalStep = 1;
  _consolidationIntervalMs = 300;
  setLink();
  auto* l = getLink();
  {
    insert(1, 10000, 0);
    auto [commitTime1, cleanupTime1, consolidationTime1] = l->avgTime();
    EXPECT_LT(0, commitTime1);
    insert(10000, 10100, 1);
    auto [commitTime2, cleanupTime2, consolidationTime2] = l->avgTime();
    EXPECT_LT(0, commitTime2);
  }
  {
    std::this_thread::sleep_for(600ms);
    auto [commitTime1, cleanupTime1, consolidationTime1] = l->avgTime();
    EXPECT_LT(0, consolidationTime1);
  }
}

TEST_F(IResearchLinkMetricsTest, CleanupWhenEmptyCommit) {
  _cleanupIntervalStep = 1;
  _commitIntervalMs = 1;
  setLink();
  auto dataPath = _dirPath / "abracadabra.txt";
  bool exist{false};
  ASSERT_TRUE(irs::file_utils::exists(exist, dataPath.c_str()));
  ASSERT_FALSE(exist);
  {  // It's necessary to close dataFile, otherwise test doesn't work on Windows
    std::ofstream dataFile{dataPath.c_str()};
    dataFile << "boom";
  }
  ASSERT_TRUE(irs::file_utils::exists(exist, dataPath.c_str()));
  int tryCount{1000};
  while(exist && (--tryCount) > 0) {
    std::this_thread::sleep_for(10ms);
    ASSERT_TRUE(irs::file_utils::exists(exist, dataPath.c_str()));
  }
  ASSERT_FALSE(exist);
}

TEST_F(IResearchLinkMetricsTest, RemoveMetrics) {
  setLink();
  auto label = getLinkMetricLabel();
  resetLink();
  EXPECT_FALSE(checkMetricExist("arangosearch_link_stats", label));
  EXPECT_FALSE(checkMetricExist("arangosearch_num_failed_commits", label));
  EXPECT_FALSE(checkMetricExist("arangosearch_num_failed_cleanups", label));
  EXPECT_FALSE(
      checkMetricExist("arangosearch_num_failed_consolidations", label));
  EXPECT_FALSE(checkMetricExist("arangosearch_commit_time", label));
  EXPECT_FALSE(checkMetricExist("arangosearch_cleanup_time", label));
  EXPECT_FALSE(checkMetricExist("arangosearch_consolidation_time", label));
}

TEST_F(IResearchLinkMetricsTest, CreateSameLink) {
  setLink();
  EXPECT_THROW(setLink(), std::exception);
}

TEST_F(IResearchLinkMetricsTest, WriteAndMetrics1) {
  setLink();
  auto* l = getLink();
  auto dataPath = _dirPath.string();
  LinkStats expectedStat;
  {
    insert(1, 2, 0);
    ++expectedStat.numDocs;
    ++expectedStat.numLiveDocs;
    insert(2, 3, 1);
    ++expectedStat.numDocs;
    ++expectedStat.numLiveDocs;
    insert(3, 4, 2);
    ++expectedStat.numDocs;
    ++expectedStat.numLiveDocs;
  }
  {
    LinkStats actualStat = l->stats();
    std::string realStr;
    l->stats().toPrometheus(realStr, "", "");
    std::string expectedStr;
    expectedStr.reserve(1024);

    expectedStr += "arangosearch_num_buffered_docs{}0\n";
    expectedStr += "arangosearch_num_docs{}3\n";
    expectedStr += "arangosearch_num_live_docs{}3\n";
    expectedStr += "arangosearch_num_segments{}3\n";
    expectedStr += "arangosearch_num_files{}16\n";
    expectedStr += "arangosearch_index_size{}2054\n";

    EXPECT_EQ(realStr, expectedStr);
    // get other stats
    getStatsFromFolder(dataPath, expectedStat.indexSize, expectedStat.numFiles);

    expectedStat.numSegments = 3;
    // should increase numFiles in expected stat
    ++expectedStat.numFiles;

    EXPECT_TRUE(expectedStat == actualStat);
  }
  {
    auto numFailed = l->numFailed();
    EXPECT_EQ(numFailed, (decltype(numFailed){0, 0, 0}));
    auto avgTime = l->avgTime();
    EXPECT_GE(avgTime, (decltype(avgTime){0, 0, 0}));
  }
}

TEST_F(IResearchLinkMetricsTest, WriteAndMetrics2) {
  setLink();
  auto* l = getLink();
  auto dataPath = _dirPath.string();
  {
    insert(1, 2, 0, false);  // insert doc in first segment, don't commit
    insert(2, 3, 1);         // insert another doc in first segment, now commit
    // check link metrics
    LinkStats expectedStats;
    expectedStats.numDocs = 2;
    expectedStats.numLiveDocs = 2;
    getStatsFromFolder(dataPath, expectedStats.indexSize,
                       expectedStats.numFiles);
    ++expectedStats.numFiles;
    expectedStats.numSegments = 1;
    LinkStats actualStats = l->stats();

    EXPECT_TRUE(expectedStats == actualStats);
  }
  {
    insert(3, 4, 2);  // insert third doc in new segment, commit
    // check link metrics
    LinkStats expectedStats;
    expectedStats.numDocs = 3;
    expectedStats.numLiveDocs = 3;
    getStatsFromFolder(dataPath, expectedStats.indexSize,
                       expectedStats.numFiles);

    ++expectedStats.numFiles;
    expectedStats.numSegments = 2;
    LinkStats actualStats = l->stats();

    EXPECT_TRUE(expectedStats == actualStats);
  }
  {
    std::string realStr;
    l->stats().toPrometheus(realStr, "", "");
    std::string expectedStr;
    expectedStr.reserve(1024);

    expectedStr += "arangosearch_num_buffered_docs{}0\n";
    expectedStr += "arangosearch_num_docs{}3\n";
    expectedStr += "arangosearch_num_live_docs{}3\n";
    expectedStr += "arangosearch_num_segments{}2\n";
    expectedStr += "arangosearch_num_files{}11\n";
    expectedStr += "arangosearch_index_size{}1513\n";

    EXPECT_EQ(realStr, expectedStr);
  }
  {
    remove(1, 2);  // delete second doc from second segment, commit
    LinkStats actualStats = l->stats();

    // check link metrics
    LinkStats expectedStats;
    expectedStats.numDocs = 3;
    expectedStats.numLiveDocs = 2;
    expectedStats.numFiles = 12;
    expectedStats.numSegments = 2;  // we have 2 segments
    expectedStats.indexSize = 1561;

    EXPECT_TRUE(expectedStats == actualStats);
  }
  {
    std::string realStr;
    l->stats().toPrometheus(realStr, "test",
                            R"(view="foo",collection="bar","shard"="s0001")");
    std::string expectedStr;
    expectedStr +=
        "arangosearch_num_buffered_docs{test,view=\"foo\","
        "collection=\"bar\",\"shard\"=\"s0001\"}0\n";
    expectedStr +=
        "arangosearch_num_docs{test,view=\"foo\","
        "collection=\"bar\",\"shard\"=\"s0001\"}3\n";
    expectedStr +=
        "arangosearch_num_live_docs{test,view=\"foo\","
        "collection=\"bar\",\"shard\"=\"s0001\"}2\n";
    expectedStr +=
        "arangosearch_num_segments{test,view=\"foo\","
        "collection=\"bar\",\"shard\"=\"s0001\"}2\n";
    expectedStr +=
        "arangosearch_num_files{test,view=\"foo\","
        "collection=\"bar\",\"shard\"=\"s0001\"}12\n";
    expectedStr +=
        "arangosearch_index_size{test,view=\"foo\","
        "collection=\"bar\",\"shard\"=\"s0001\"}1561\n";

    EXPECT_EQ(realStr, expectedStr);
  }
  {
    auto numFailed = l->numFailed();
    EXPECT_EQ(numFailed, (decltype(numFailed){0, 0, 0}));
    auto avgTime = l->avgTime();
    EXPECT_GE(avgTime, (decltype(avgTime){0, 0, 0}));
  }
}

TEST_F(IResearchLinkMetricsTest, LinkAndMetics) {
  setLink();
  auto* l = getLink();
  auto dataPath = _dirPath.string();
  {
    insert(1, 2, 0);
    std::string collection = l->getCollectionName();

    std::string expected;

    expected +=
        R"(arangosearch_num_buffered_docs{view="h3039/42",collection=")";
    expected += collection;
    expected += R"(",shard="",db="testVocbase"}0)";
    expected += "\n";

    expected += R"(arangosearch_num_docs{view="h3039/42",collection=")";
    expected += collection;
    expected += R"(",shard="",db="testVocbase"}1)";
    expected += "\n";

    expected += R"(arangosearch_num_live_docs{view="h3039/42",collection=")";
    expected += collection;
    expected += R"(",shard="",db="testVocbase"}1)";
    expected += "\n";

    expected += R"(arangosearch_num_segments{view="h3039/42",collection=")";
    expected += collection;
    expected += R"(",shard="",db="testVocbase"}1)";
    expected += "\n";

    expected += R"(arangosearch_num_files{view="h3039/42",collection=")";
    expected += collection;
    expected += R"(",shard="",db="testVocbase"}6)";
    expected += "\n";

    expected += R"(arangosearch_index_size{view="h3039/42",collection=")";
    expected += collection;
    expected += R"(",shard="",db="testVocbase"}681)";
    expected += "\n";

    std::string actual;
    getPrometheusStr(actual);

    EXPECT_EQ(actual, expected);
  }

  {
    insert(1, 2, 1, false);
    insert(2, 3, 2);

    std::string collection = l->getCollectionName();

    std::string expected;

    expected +=
        R"(arangosearch_num_buffered_docs{view="h3039/42",collection=")";
    expected += collection;
    expected += R"(",shard="",db="testVocbase"}0)";
    expected += "\n";

    expected += R"(arangosearch_num_docs{view="h3039/42",collection=")";
    expected += collection;
    expected += R"(",shard="",db="testVocbase"}3)";
    expected += "\n";

    expected += R"(arangosearch_num_live_docs{view="h3039/42",collection=")";
    expected += collection;
    expected += R"(",shard="",db="testVocbase"}3)";
    expected += "\n";

    expected += R"(arangosearch_num_segments{view="h3039/42",collection=")";
    expected += collection;
    expected += R"(",shard="",db="testVocbase"}2)";
    expected += "\n";

    expected += R"(arangosearch_num_files{view="h3039/42",collection=")";
    expected += collection;
    expected += R"(",shard="",db="testVocbase"}11)";
    expected += "\n";

    expected += R"(arangosearch_index_size{view="h3039/42",collection=")";
    expected += collection;
    expected += R"(",shard="",db="testVocbase"}1513)";
    expected += "\n";

    std::string actual;
    getPrometheusStr(actual);

    EXPECT_EQ(actual, expected);
  }
  {
    auto numFailed = l->numFailed();
    EXPECT_EQ(numFailed, (decltype(numFailed){0, 0, 0}));
    auto avgTime = l->avgTime();
    EXPECT_GE(avgTime, (decltype(avgTime){0, 0, 0}));
  }
}
