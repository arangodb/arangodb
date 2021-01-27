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

#include "gtest/gtest.h"

#include "store/fs_directory.hpp"
#include "utils/log.hpp"
#include "utils/utf8_path.hpp"

#include <velocypack/Parser.h>

#include "IResearch/common.h"
#include "Mocks/IResearchLinkMock.h"
#include "Mocks/Servers.h"
#include "Mocks/StorageEngineMock.h"

#include "Aql/AqlFunctionFeature.h"
#include "Aql/QueryRegistry.h"
#include "Basics/files.h"
#include "Cluster/ClusterFeature.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "IResearch/IResearchAnalyzerFeature.h"
#include "IResearch/IResearchCommon.h"
#include "IResearch/IResearchFeature.h"
#include "IResearch/IResearchLink.h"
#include "IResearch/IResearchView.h"
#include "Logger/LogTopic.h"
#include "Logger/Logger.h"
#include "RestServer/AqlFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/DatabasePathFeature.h"
#include "RestServer/FlushFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "RestServer/ServerIdFeature.h"
#include "RestServer/SystemDatabaseFeature.h"
#include "RestServer/ViewTypesFeature.h"
#include "Sharding/ShardingFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "Transaction/Methods.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/ExecContext.h"
#include "V8Server/V8DealerFeature.h"
#include "VocBase/KeyGenerator.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/LogicalView.h"
#include "IResearchTestCompressor.h"
#include "Mocks/IResearchLinkMock.h"

using namespace std::chrono_literals;

#if USE_ENTERPRISE
#include "Enterprise/Ldap/LdapFeature.h"
#endif

REGISTER_COMPRESSION(iresearch::compression::mock::test_compressor,
                     &iresearch::compression::mock::test_compressor::compressor,
                     &iresearch::compression::mock::test_compressor::decompressor);

static const VPackBuilder systemDatabaseBuilder = dbArgsBuilder();
static const VPackSlice   systemDatabaseArgs = systemDatabaseBuilder.slice();
// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

class IResearchLinkTest
    : public ::testing::Test,
      public arangodb::tests::LogSuppressor<arangodb::Logger::AUTHENTICATION, arangodb::LogLevel::ERR>,
      public arangodb::tests::LogSuppressor<arangodb::Logger::ENGINES, arangodb::LogLevel::FATAL>,
      public arangodb::tests::LogSuppressor<arangodb::Logger::FIXME, arangodb::LogLevel::FATAL> {
 protected:
  static constexpr size_t enc_block_size{ 13 };
  arangodb::tests::mocks::MockAqlServer server;
  std::string testFilesystemPath;

  IResearchLinkTest() : server(false) {
    arangodb::tests::init();

    // ensure ArangoSearch start 1 maintenance for each group
    auto opts = server.server().options();
    auto& ars = server.getFeature<arangodb::iresearch::IResearchFeature>();
    ars.collectOptions(opts);
    auto* commitThreads = opts->get<arangodb::options::UInt32Parameter>("--arangosearch.commit-threads");
    opts->processingResult().touch("arangosearch.commit-threads");
    EXPECT_NE(nullptr, commitThreads);
    *commitThreads->ptr = 1;
    auto* consolidationThreads = opts->get<arangodb::options::UInt32Parameter>("--arangosearch.consolidation-threads");
    opts->processingResult().touch("arangosearch.consolidation-threads");
    EXPECT_NE(nullptr, consolidationThreads);
    *consolidationThreads->ptr = 1;
    ars.validateOptions(opts);

    server.addFeature<arangodb::FlushFeature>(false);
    server.startFeatures();
    EXPECT_EQ(
      (std::pair<size_t, size_t>{1, 1}),
      ars.limits(arangodb::iresearch::ThreadGroup::_0));
    EXPECT_EQ(
      (std::pair<size_t, size_t>{1, 1}),
      ars.limits(arangodb::iresearch::ThreadGroup::_1));

    TransactionStateMock::abortTransactionCount = 0;
    TransactionStateMock::beginTransactionCount = 0;
    TransactionStateMock::commitTransactionCount = 0;

    auto& dbPathFeature = server.getFeature<arangodb::DatabasePathFeature>();
    arangodb::tests::setDatabasePath(dbPathFeature);  // ensure test data is stored in a unique directory
    testFilesystemPath = dbPathFeature.directory();

    long systemError;
    std::string systemErrorStr;
    TRI_CreateDirectory(testFilesystemPath.c_str(), systemError, systemErrorStr);
  }

  ~IResearchLinkTest() {
    TRI_RemoveDirectory(testFilesystemPath.c_str());
  }
};

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------
TEST_F(IResearchLinkTest, test1) {
}

TEST_F(IResearchLinkTest, test_defaults) {
  // no view specified
  {
    auto& engine = *static_cast<StorageEngineMock*>(
        &server.getFeature<arangodb::EngineSelectorFeature>().engine());
    engine.views.clear();
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
    auto collectionJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testCollection\" }");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    ASSERT_NE(nullptr, logicalCollection);
    auto json = arangodb::velocypack::Parser::fromJson("{}");
    try {
      StorageEngineMock::buildLinkMock(arangodb::IndexId{1}, *logicalCollection, json->slice());
      EXPECT_TRUE(false);
    } catch (arangodb::basics::Exception const& ex) {
      EXPECT_EQ(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, ex.code());
    }
  }

  // no view can be found (e.g. db-server coming up with view not available from Agency yet)
  {
    auto& engine = *static_cast<StorageEngineMock*>(
        &server.getFeature<arangodb::EngineSelectorFeature>().engine());
    engine.views.clear();
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
    auto collectionJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testCollection\" }");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    ASSERT_NE(nullptr, logicalCollection);
    auto json = arangodb::velocypack::Parser::fromJson("{ \"view\": \"42\" }");
    auto link = StorageEngineMock::buildLinkMock(arangodb::IndexId{1}, *logicalCollection, json->slice());
    EXPECT_NE(nullptr, link.get());
  }

  // valid link creation
  {
    auto& engine = *static_cast<StorageEngineMock*>(
        &server.getFeature<arangodb::EngineSelectorFeature>().engine());
    engine.views.clear();
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
    auto linkJson = arangodb::velocypack::Parser::fromJson(
        "{ \"type\": \"arangosearch\", \"view\": \"42\" }");
    auto collectionJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testCollection\" }");
    auto viewJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testView\", \"id\": 42, \"type\": \"arangosearch\" }");
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
    EXPECT_FALSE( link->isSorted());
    EXPECT_EQ(0, link->memory());
    EXPECT_TRUE(link->sparse());
    EXPECT_TRUE(arangodb::Index::IndexType::TRI_IDX_TYPE_IRESEARCH_LINK == link->type());
    EXPECT_TRUE(arangodb::iresearch::DATA_SOURCE_TYPE.name() == link->typeName());
    EXPECT_FALSE(link->unique());

    arangodb::iresearch::IResearchLinkMeta actualMeta;
    arangodb::iresearch::IResearchLinkMeta expectedMeta;
    auto builder = link->toVelocyPack(
        arangodb::Index::makeFlags(arangodb::Index::Serialize::Figures));
    std::string error;

    EXPECT_TRUE(actualMeta.init(server.server(), builder->slice(), false, error));
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
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
    auto linkJson = arangodb::velocypack::Parser::fromJson(
        "{ \"type\": \"arangosearch\", \"view\": \"42\" }");
    auto collectionJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testCollection\" }");
    auto viewJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testView\", \"id\": 42, \"type\": \"arangosearch\" }");
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
    EXPECT_EQ(arangodb::Index::IndexType::TRI_IDX_TYPE_IRESEARCH_LINK, link->type());
    EXPECT_EQ(arangodb::iresearch::DATA_SOURCE_TYPE.name(), link->typeName());
    EXPECT_FALSE(link->unique());

    {
      arangodb::iresearch::IResearchLinkMeta actualMeta;
      arangodb::iresearch::IResearchLinkMeta expectedMeta;
      auto builder = link->toVelocyPack(
          arangodb::Index::makeFlags(arangodb::Index::Serialize::Figures));
      std::string error;

      EXPECT_TRUE((actualMeta.init(server.server(), builder->slice(), false, error) &&
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
        "{ \"name\": \"testCollection\", \"id\": 100 }");
    auto linkJson = arangodb::velocypack::Parser::fromJson(
        "{ \"type\": \"arangosearch\", \"view\": \"42\" }");
    auto viewJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testView\", \"id\": 42, \"type\": \"arangosearch\" }");
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    ASSERT_TRUE((nullptr != logicalCollection));
    auto logicalView = vocbase.createView(viewJson->slice());
    ASSERT_TRUE((false == !logicalView));

    // no collections in view before
    {
      std::unordered_set<arangodb::DataSourceId> expected;
      std::set<arangodb::DataSourceId> actual;

      EXPECT_TRUE((logicalView->visitCollections([&actual](arangodb::DataSourceId cid) -> bool {
        actual.emplace(cid);
        return true;
      })));

      for (auto& cid : expected) {
        EXPECT_EQ(1, actual.erase(cid));
      }

      EXPECT_TRUE((actual.empty()));
    }

    auto link = StorageEngineMock::buildLinkMock(arangodb::IndexId{1}, *logicalCollection, linkJson->slice());
    EXPECT_NE(nullptr, link.get());
    //EXPECT_TRUE((false == !link));

    // collection in view after
    {
      std::unordered_set<arangodb::DataSourceId> expected = {arangodb::DataSourceId{100}};
      std::set<arangodb::DataSourceId> actual;

      EXPECT_TRUE((logicalView->visitCollections([&actual](arangodb::DataSourceId cid) -> bool {
        actual.emplace(cid);
        return true;
      })));

      for (auto& cid : expected) {
        EXPECT_EQ(1,  actual.erase(cid));
      }

      EXPECT_TRUE((actual.empty()));
    }

    link.reset();

    // collection in view on destruct
    {
      std::unordered_set<arangodb::DataSourceId> expected = {arangodb::DataSourceId{100}};
      std::set<arangodb::DataSourceId> actual;

      EXPECT_TRUE((logicalView->visitCollections([&actual](arangodb::DataSourceId cid) -> bool {
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
        "{ \"name\": \"testCollection\", \"id\": 101 }");
    auto linkJson = arangodb::velocypack::Parser::fromJson(
        "{ \"type\": \"arangosearch\", \"view\": \"43\" }");
    auto viewJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testView\", \"id\": 43, \"type\": \"arangosearch\", "
        "\"collections\": [ 101 ] }");
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    ASSERT_NE(nullptr, logicalCollection);
    auto logicalView = vocbase.createView(viewJson->slice());
    ASSERT_NE(nullptr, logicalView);

    // collection in view before
    {
      std::unordered_set<arangodb::DataSourceId> expected = {arangodb::DataSourceId{101}};
      std::set<arangodb::DataSourceId> actual;

      EXPECT_TRUE((logicalView->visitCollections([&actual](arangodb::DataSourceId cid) -> bool {
        actual.emplace(cid);
        return true;
      })));

      for (auto& cid : expected) {
        EXPECT_EQ(1, actual.erase(cid));
      }

      EXPECT_TRUE((actual.empty()));
    }

    auto link = StorageEngineMock::buildLinkMock(arangodb::IndexId{1}, *logicalCollection, linkJson->slice());
    EXPECT_NE(nullptr, link.get());

    // collection in view after
    {
      std::unordered_set<arangodb::DataSourceId> expected = {arangodb::DataSourceId{101}};
      std::set<arangodb::DataSourceId> actual;

      EXPECT_TRUE((logicalView->visitCollections([&actual](arangodb::DataSourceId cid) -> bool {
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
      std::unordered_set<arangodb::DataSourceId> expected = {arangodb::DataSourceId{101}};
      std::set<arangodb::DataSourceId> actual;

      EXPECT_TRUE((logicalView->visitCollections([&actual](arangodb::DataSourceId cid) -> bool {
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
    EXPECT_EQ(nullptr, empty.get());
  }

  arangodb::iresearch::IResearchLink::AsyncLinkPtr self;

  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testCollection\" }");
    auto linkJson =
        arangodb::velocypack::Parser::fromJson("{ \"view\": \"testView\" }");
    auto viewJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testView\", \"type\":\"arangosearch\" }");
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    ASSERT_NE(nullptr, logicalCollection);
    auto logicalView = vocbase.createView(viewJson->slice());
    EXPECT_NE(nullptr, logicalView);
    auto index = StorageEngineMock::buildLinkMock(arangodb::IndexId{42}, *logicalCollection, linkJson->slice());
    ASSERT_NE(nullptr, index.get());
    //ASSERT_TRUE((false == !index));
    auto link = std::dynamic_pointer_cast<arangodb::iresearch::IResearchLink>(index);
    ASSERT_NE(nullptr, link);
    self = link->self();
    EXPECT_NE(nullptr, self);
    auto lock = self->lock();
    EXPECT_EQ(link.get(), self->get());
  }

  EXPECT_TRUE(self);
  auto lock = self->lock();
  EXPECT_EQ(nullptr, self->get());
}

TEST_F(IResearchLinkTest, test_drop) {
  // collection drop (removes collection from view) subsequent destroy does not touch view
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testCollection\", \"id\": 100 }");
    auto linkJson = arangodb::velocypack::Parser::fromJson(
        "{ \"type\": \"arangosearch\", \"view\": \"42\" }");
    auto viewJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testView\", \"id\": 42, \"type\": \"arangosearch\" }");
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    ASSERT_TRUE((nullptr != logicalCollection));
    auto logicalView = vocbase.createView(viewJson->slice());
    ASSERT_TRUE((false == !logicalView));

    auto link0 = StorageEngineMock::buildLinkMock(arangodb::IndexId{1}, *logicalCollection, linkJson->slice());
    EXPECT_NE(nullptr, link0.get());

    // collection in view before
    {
      std::unordered_set<arangodb::DataSourceId> expected = {arangodb::DataSourceId{100}};
      std::set<arangodb::DataSourceId> actual;

      EXPECT_TRUE((logicalView->visitCollections([&actual](arangodb::DataSourceId cid) -> bool {
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

      EXPECT_TRUE((logicalView->visitCollections([&actual](arangodb::DataSourceId cid) -> bool {
        actual.emplace(cid);
        return true;
      })));

      for (auto& cid : expected) {
        EXPECT_EQ(1, actual.erase(cid));
      }

      EXPECT_TRUE((actual.empty()));
    }

    auto link1 = StorageEngineMock::buildLinkMock(arangodb::IndexId{1}, *logicalCollection, linkJson->slice());
    EXPECT_NE(nullptr, link1);

    // collection in view before (new link)
    {
      std::unordered_set<arangodb::DataSourceId> expected = {arangodb::DataSourceId{100}};
      std::set<arangodb::DataSourceId> actual;

      EXPECT_TRUE((logicalView->visitCollections([&actual](arangodb::DataSourceId cid) -> bool {
        actual.emplace(cid);
        return true;
      })));

      for (auto& cid : expected) {
        EXPECT_EQ(1, actual.erase(cid));
      }

      EXPECT_TRUE((actual.empty()));
    }

    link0.reset();

    // collection in view after (new link) subsequent destroy does not touch view
    {
      std::unordered_set<arangodb::DataSourceId> expected = {arangodb::DataSourceId{100}};
      std::set<arangodb::DataSourceId> actual;

      EXPECT_TRUE((logicalView->visitCollections([&actual](arangodb::DataSourceId cid) -> bool {
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
        "{ \"name\": \"testCollection\", \"id\": 100 }");
    auto linkJson = arangodb::velocypack::Parser::fromJson(
        "{ \"type\": \"arangosearch\", \"view\": \"42\" }");
    auto viewJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testView\", \"id\": 42, \"type\": \"arangosearch\" }");
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    ASSERT_TRUE((nullptr != logicalCollection));
    auto logicalView = vocbase.createView(viewJson->slice());
    ASSERT_TRUE((false == !logicalView));

    auto link = StorageEngineMock::buildLinkMock(arangodb::IndexId{1}, *logicalCollection, linkJson->slice());
    EXPECT_NE(nullptr, link.get());

    // collection in view before
    {
      std::unordered_set<arangodb::DataSourceId> expected = {arangodb::DataSourceId{100}};
      std::set<arangodb::DataSourceId> actual;

      EXPECT_TRUE((logicalView->visitCollections([&actual](arangodb::DataSourceId cid) -> bool {
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
      std::unordered_set<arangodb::DataSourceId> expected = {arangodb::DataSourceId{100}};
      std::set<arangodb::DataSourceId> actual;

      EXPECT_TRUE((logicalView->visitCollections([&actual](arangodb::DataSourceId cid) -> bool {
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
      std::unordered_set<arangodb::DataSourceId> expected = {arangodb::DataSourceId{100}};
      std::set<arangodb::DataSourceId> actual;

      EXPECT_TRUE((logicalView->visitCollections([&actual](arangodb::DataSourceId cid) -> bool {
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

TEST_F(IResearchLinkTest, test_write_index_creation) {
  static std::vector<std::string> const EMPTY;
  auto doc0 = arangodb::velocypack::Parser::fromJson("{ \"abc\": \"def\" }");
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
  std::string dataPath = ((((irs::utf8_path() /= testFilesystemPath) /= std::string("databases")) /=
    (std::string("database-") + std::to_string(vocbase.id()))) /=
    std::string("arangosearch-42"))
    .utf8();
  auto linkJson = arangodb::velocypack::Parser::fromJson(
    "{ \"id\": 42, \"type\": \"arangosearch\", \"view\": \"42\", "
    "\"includeAllFields\": true }");
  auto collectionJson = arangodb::velocypack::Parser::fromJson(
    "{ \"name\": \"testCollection\" }");
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

  dataPath =
      ((((irs::utf8_path() /= testFilesystemPath) /=
         std::string("databases")) /=
        (std::string("database-") + std::to_string(vocbase.id()))) /=
       (std::string("arangosearch-") + std::to_string(logicalCollection->id().id()) + "_42"))
          .utf8();
  irs::fs_directory directory(dataPath);
  bool created;
  auto link = logicalCollection->createIndex(linkJson->slice(), created);
  ASSERT_TRUE((false == !link && created));
  auto reader = irs::directory_reader::open(directory);
  EXPECT_EQ(0, reader.reopen().live_docs_count());
  {
    arangodb::transaction::Methods trx(arangodb::transaction::StandaloneContext::Create(vocbase),
      EMPTY, EMPTY, EMPTY,
      arangodb::transaction::Options());
    trx.addHint(arangodb::transaction::Hints::Hint::INDEX_CREATION);
    EXPECT_TRUE((trx.begin().ok()));
    auto* l = dynamic_cast<arangodb::iresearch::IResearchLink*>(link.get());
    ASSERT_TRUE(l != nullptr);
    EXPECT_TRUE((l->insert(trx, arangodb::LocalDocumentId(1), doc0->slice()).ok()));
    l->commit(true);
    EXPECT_EQ(1, reader.reopen().live_docs_count()); // should see this immediately

    EXPECT_TRUE((trx.commit().ok()));
    EXPECT_TRUE((l->commit().ok()));
  }

  EXPECT_EQ(1, reader.reopen().live_docs_count());
  logicalCollection->dropIndex(link->id());
  EXPECT_ANY_THROW((reader.reopen()));
}

TEST_F(IResearchLinkTest, test_write) {
  static std::vector<std::string> const EMPTY;
  auto doc0 = arangodb::velocypack::Parser::fromJson("{ \"abc\": \"def\" }");
  auto doc1 = arangodb::velocypack::Parser::fromJson("{ \"ghi\": \"jkl\" }");
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
  std::string dataPath = ((((irs::utf8_path() /= testFilesystemPath) /= std::string("databases")) /=
                           (std::string("database-") + std::to_string(vocbase.id()))) /=
                          std::string("arangosearch-42"))
                             .utf8();
  auto linkJson = arangodb::velocypack::Parser::fromJson(
      "{ \"id\": 42, \"type\": \"arangosearch\", \"view\": \"42\", "
      "\"includeAllFields\": true }");
  auto collectionJson = arangodb::velocypack::Parser::fromJson(
      "{ \"name\": \"testCollection\" }");
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

  dataPath =
      ((((irs::utf8_path() /= testFilesystemPath) /=
         std::string("databases")) /=
        (std::string("database-") + std::to_string(vocbase.id()))) /=
       (std::string("arangosearch-") + std::to_string(logicalCollection->id().id()) + "_42"))
          .utf8();
  irs::fs_directory directory(dataPath);
  bool created;
  auto link = logicalCollection->createIndex(linkJson->slice(), created);
  ASSERT_TRUE((false == !link && created));
  auto reader = irs::directory_reader::open(directory);
  EXPECT_EQ(0, reader.reopen().live_docs_count());
  {
    arangodb::transaction::Methods trx(arangodb::transaction::StandaloneContext::Create(vocbase),
                                       EMPTY, EMPTY, EMPTY,
                                       arangodb::transaction::Options());
    EXPECT_TRUE((trx.begin().ok()));
    auto* l = dynamic_cast<arangodb::iresearch::IResearchLink*>(link.get());
    ASSERT_TRUE(l != nullptr);
    EXPECT_TRUE((l->insert(trx, arangodb::LocalDocumentId(1), doc0->slice())
                     .ok()));
    EXPECT_TRUE((l->commit().ok()));
    EXPECT_EQ(0, reader.reopen().live_docs_count());

    EXPECT_TRUE((trx.commit().ok()));
    EXPECT_TRUE((l->commit().ok()));
  }

  EXPECT_EQ(1, reader.reopen().live_docs_count());

  {
    arangodb::transaction::Methods trx(arangodb::transaction::StandaloneContext::Create(vocbase),
                                       EMPTY, EMPTY, EMPTY,
                                       arangodb::transaction::Options());
    EXPECT_TRUE((trx.begin().ok()));
    auto* l = dynamic_cast<arangodb::iresearch::IResearchLink*>(link.get());
    ASSERT_TRUE(l != nullptr);
    EXPECT_TRUE((l->insert(trx, arangodb::LocalDocumentId(2), doc1->slice())
                     .ok()));
    EXPECT_TRUE((trx.commit().ok()));
    EXPECT_TRUE((l->commit().ok()));
  }

  EXPECT_EQ(2, reader.reopen().live_docs_count());

  {
    arangodb::transaction::Methods trx(arangodb::transaction::StandaloneContext::Create(vocbase),
                                       EMPTY, EMPTY, EMPTY,
                                       arangodb::transaction::Options());
    EXPECT_TRUE((trx.begin().ok()));
    auto* l = dynamic_cast<arangodb::iresearch::IResearchLink*>(link.get());
    ASSERT_TRUE(l != nullptr);
    EXPECT_TRUE((l->remove(trx, arangodb::LocalDocumentId(2), doc1->slice())
                     .ok()));
    EXPECT_TRUE((trx.commit().ok()));
    EXPECT_TRUE((l->commit().ok()));
  }

  EXPECT_EQ(1, reader.reopen().live_docs_count());
  logicalCollection->dropIndex(link->id());
  EXPECT_ANY_THROW((reader.reopen()));
}

TEST_F(IResearchLinkTest, test_write_with_custom_compression_nondefault_sole) {
  static std::vector<std::string> const EMPTY;
  auto doc0 = arangodb::velocypack::Parser::fromJson("{ \"abc\": \"def\", \"abc2\":\"aaa\", \"sort\":\"ps\"  }");
  auto doc1 = arangodb::velocypack::Parser::fromJson("{ \"ghi\": \"jkl\" }");
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
  std::string dataPath = ((((irs::utf8_path() /= testFilesystemPath) /= std::string("databases")) /=
    (std::string("database-") + std::to_string(vocbase.id()))) /=
    std::string("arangosearch-42"))
    .utf8();
  auto linkJson = arangodb::velocypack::Parser::fromJson(
    "{ \"id\": 42, \"type\": \"arangosearch\", \"view\": \"42\", "
    "\"includeAllFields\": true,\
    \"primarySort\":[{\"field\":\"sort\", \"direction\":\"asc\"}],\
    \"storedValues\":[{\"fields\":[\"abc\"], \"compression\":\"test\"}, {\"fields\":[\"abc2\"], \"compression\":\"test\"}]\
    }");
  auto collectionJson = arangodb::velocypack::Parser::fromJson(
    "{ \"name\": \"testCollection\" }");
  auto viewJson = arangodb::velocypack::Parser::fromJson(
    "{ \
    \"id\": 42, \
    \"name\": \"testView\", \
    \"type\": \"arangosearch\", \
    \"primarySort\":[{\"field\":\"sort\", \"direction\":\"asc\"}],\
    \"storedValues\":[{\"fields\":[\"abc\"], \"compression\":\"test\"}, {\"fields\":[\"abc2\"], \"compression\":\"test\"}]\
  }");
  std::set<std::string> compressed_values;
  irs::compression::mock::test_compressor::functions().compress_mock = [&compressed_values](irs::byte_type* src, size_t size, irs::bstring& out)->irs::bytes_ref {
    out.append(src, size);
    compressed_values.emplace(reinterpret_cast<const char*>(src), size);
    return irs::bytes_ref(reinterpret_cast<const irs::byte_type*>(out.data()),  size);
  };
  auto compressorRemover = irs::make_finally([]() {irs::compression::mock::test_compressor::functions().compress_mock = nullptr; });
  auto logicalCollection = vocbase.createCollection(collectionJson->slice());
  ASSERT_TRUE((nullptr != logicalCollection));
  auto view = std::dynamic_pointer_cast<arangodb::iresearch::IResearchView>(
    vocbase.createView(viewJson->slice()));
  ASSERT_TRUE((false == !view));
  view->open();
  ASSERT_TRUE(server.server().hasFeature<arangodb::FlushFeature>());

  dataPath =
      ((((irs::utf8_path() /= testFilesystemPath) /=
         std::string("databases")) /=
        (std::string("database-") + std::to_string(vocbase.id()))) /=
       (std::string("arangosearch-") + std::to_string(logicalCollection->id().id()) + "_42"))
          .utf8();
  irs::fs_directory directory(dataPath);
  bool created;
  auto link = logicalCollection->createIndex(linkJson->slice(), created);
  ASSERT_TRUE((false == !link && created));
  auto reader = irs::directory_reader::open(directory);
  EXPECT_EQ(0, reader.reopen().live_docs_count());
  {
    arangodb::transaction::Methods trx(arangodb::transaction::StandaloneContext::Create(vocbase),
      EMPTY, EMPTY, EMPTY,
      arangodb::transaction::Options());
    EXPECT_TRUE((trx.begin().ok()));
    auto* l = dynamic_cast<arangodb::iresearch::IResearchLink*>(link.get());
    ASSERT_TRUE(l != nullptr);
    EXPECT_TRUE((l->insert(trx, arangodb::LocalDocumentId(1), doc0->slice())
      .ok()));
    EXPECT_TRUE((l->commit().ok()));
    EXPECT_EQ(0, reader.reopen().live_docs_count());

    EXPECT_TRUE((trx.commit().ok()));
    EXPECT_TRUE((l->commit().ok()));
  }

  EXPECT_EQ(1, reader.reopen().live_docs_count());

  {
    arangodb::transaction::Methods trx(arangodb::transaction::StandaloneContext::Create(vocbase),
      EMPTY, EMPTY, EMPTY,
      arangodb::transaction::Options());
    EXPECT_TRUE((trx.begin().ok()));
    auto* l = dynamic_cast<arangodb::iresearch::IResearchLink*>(link.get());
    ASSERT_TRUE(l != nullptr);
    EXPECT_TRUE((l->insert(trx, arangodb::LocalDocumentId(2), doc1->slice())
      .ok()));
    EXPECT_TRUE((trx.commit().ok()));
    EXPECT_TRUE((l->commit().ok()));
  }

  EXPECT_EQ(2, reader.reopen().live_docs_count());
  std::set<std::string> expected;
  auto abcSlice = doc0->slice().get("abc");
  expected.emplace(reinterpret_cast<const char*>(abcSlice.start()), abcSlice.byteSize());
  auto abc2Slice = doc0->slice().get("abc2");
  expected.emplace(reinterpret_cast<const char*>(abc2Slice.start()), abc2Slice.byteSize());
  EXPECT_EQ(expected, compressed_values);
}

TEST_F(IResearchLinkTest, test_write_with_custom_compression_nondefault_sole_with_sort) {
  static std::vector<std::string> const EMPTY;
  auto doc0 = arangodb::velocypack::Parser::fromJson("{ \"abc\": \"def\", \"abc2\":\"aaa\", \"sort\":\"ps\"  }");
  auto doc1 = arangodb::velocypack::Parser::fromJson("{ \"ghi\": \"jkl\", \"sort\":\"pp\" }");
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
  std::string dataPath = ((((irs::utf8_path() /= testFilesystemPath) /= std::string("databases")) /=
    (std::string("database-") + std::to_string(vocbase.id()))) /=
    std::string("arangosearch-42"))
    .utf8();
  auto linkJson = arangodb::velocypack::Parser::fromJson(
    "{ \"id\": 42, \"type\": \"arangosearch\", \"view\": \"42\", "
    "\"includeAllFields\": true,\
    \"primarySort\":[{\"field\":\"sort\", \"direction\":\"asc\"}],\
    \"primarySortCompression\":\"test\",\
    \"storedValues\":[{\"fields\":[\"abc\"], \"compression\":\"test\"}, {\"fields\":[\"abc2\"], \"compression\":\"test\"}]\
    }");
  auto collectionJson = arangodb::velocypack::Parser::fromJson(
    "{ \"name\": \"testCollection\" }");
  auto viewJson = arangodb::velocypack::Parser::fromJson(
    "{ \
    \"id\": 42, \
    \"name\": \"testView\", \
    \"type\": \"arangosearch\", \
    \"primarySort\":[{\"field\":\"sort\", \"direction\":\"asc\"}],\
    \"storedValues\":[{\"fields\":[\"abc\"], \"compression\":\"test\"}, {\"fields\":[\"abc2\"], \"compression\":\"test\"}]\
  }");
  std::set<std::string> compressed_values;
  irs::compression::mock::test_compressor::functions().compress_mock = [&compressed_values](irs::byte_type* src, size_t size, irs::bstring& out)->irs::bytes_ref {
    out.append(src, size);
    compressed_values.emplace(reinterpret_cast<const char*>(src), size);
    return irs::bytes_ref(reinterpret_cast<const irs::byte_type*>(out.data()), size);
  };
  auto compressorRemover = irs::make_finally([]() {irs::compression::mock::test_compressor::functions().compress_mock = nullptr; });
  auto logicalCollection = vocbase.createCollection(collectionJson->slice());
  ASSERT_TRUE((nullptr != logicalCollection));
  auto view = std::dynamic_pointer_cast<arangodb::iresearch::IResearchView>(
    vocbase.createView(viewJson->slice()));
  ASSERT_TRUE((false == !view));
  view->open();
  ASSERT_TRUE(server.server().hasFeature<arangodb::FlushFeature>());

  dataPath =
      ((((irs::utf8_path() /= testFilesystemPath) /=
         std::string("databases")) /=
        (std::string("database-") + std::to_string(vocbase.id()))) /=
       (std::string("arangosearch-") + std::to_string(logicalCollection->id().id()) + "_42"))
          .utf8();
  irs::fs_directory directory(dataPath);
  bool created;
  auto link = logicalCollection->createIndex(linkJson->slice(), created);
  ASSERT_TRUE((false == !link && created));
  auto reader = irs::directory_reader::open(directory);
  EXPECT_EQ(0, reader.reopen().live_docs_count());
  {
    arangodb::transaction::Methods trx(arangodb::transaction::StandaloneContext::Create(vocbase),
      EMPTY, EMPTY, EMPTY,
      arangodb::transaction::Options());
    EXPECT_TRUE((trx.begin().ok()));
    auto* l = dynamic_cast<arangodb::iresearch::IResearchLink*>(link.get());
    ASSERT_TRUE(l != nullptr);
    EXPECT_TRUE((l->insert(trx, arangodb::LocalDocumentId(1), doc0->slice())
      .ok()));
    EXPECT_TRUE((l->commit().ok()));
    EXPECT_EQ(0, reader.reopen().live_docs_count());

    EXPECT_TRUE((trx.commit().ok()));
    EXPECT_TRUE((l->commit().ok()));
  }

  EXPECT_EQ(1, reader.reopen().live_docs_count());

  {
    arangodb::transaction::Methods trx(arangodb::transaction::StandaloneContext::Create(vocbase),
      EMPTY, EMPTY, EMPTY,
      arangodb::transaction::Options());
    EXPECT_TRUE((trx.begin().ok()));
    auto* l = dynamic_cast<arangodb::iresearch::IResearchLink*>(link.get());
    ASSERT_TRUE(l != nullptr);
    EXPECT_TRUE((l->insert(trx, arangodb::LocalDocumentId(2), doc1->slice())
      .ok()));
    EXPECT_TRUE((trx.commit().ok()));
    EXPECT_TRUE((l->commit().ok()));
  }

  EXPECT_EQ(2, reader.reopen().live_docs_count());
  std::set<std::string> expected;
  auto sortSlice = doc0->slice().get("sort");
  expected.emplace(reinterpret_cast<const char*>(sortSlice.start()), sortSlice.byteSize());
  auto abcSlice = doc0->slice().get("abc");
  expected.emplace(reinterpret_cast<const char*>(abcSlice.start()), abcSlice.byteSize());
  auto abc2Slice = doc0->slice().get("abc2");
  expected.emplace(reinterpret_cast<const char*>(abc2Slice.start()), abc2Slice.byteSize());
  auto sortSlice1 = doc1->slice().get("sort");
  expected.emplace(reinterpret_cast<const char*>(sortSlice1.start()), sortSlice1.byteSize());
  EXPECT_EQ(expected, compressed_values);
}

TEST_F(IResearchLinkTest, test_write_with_custom_compression_nondefault_mixed) {
  static std::vector<std::string> const EMPTY;
  auto doc0 = arangodb::velocypack::Parser::fromJson("{ \"abc\": \"def\", \"abc2\":\"aaa\", \"sort\":\"ps\" }");
  auto doc1 = arangodb::velocypack::Parser::fromJson("{ \"ghi\": \"jkl\" }");
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
  std::string dataPath = ((((irs::utf8_path() /= testFilesystemPath) /= std::string("databases")) /=
    (std::string("database-") + std::to_string(vocbase.id()))) /=
    std::string("arangosearch-42"))
    .utf8();
  auto linkJson = arangodb::velocypack::Parser::fromJson(
    "{ \"id\": 42, \"type\": \"arangosearch\", \"view\": \"42\", "
    "\"includeAllFields\": true,\
    \"primarySort\":[{\"field\":\"sort\", \"direction\":\"asc\"}],\
    \"storedValues\":[{\"fields\":[\"abc\"], \"compression\":\"test\"},\
                      {\"fields\":[\"abc2\"], \"compression\":\"lz4\"},\
                      {\"fields\":[\"ghi\"], \"compression\":\"test\"}]\
    }");
  auto collectionJson = arangodb::velocypack::Parser::fromJson(
    "{ \"name\": \"testCollection\" }");
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
  std::set<std::string> compressed_values;
  irs::compression::mock::test_compressor::functions().compress_mock = [&compressed_values](irs::byte_type* src, size_t size, irs::bstring& out)->irs::bytes_ref {
    out.append(src, size);
    compressed_values.emplace(reinterpret_cast<const char*>(src), size);
    return irs::bytes_ref(reinterpret_cast<const irs::byte_type*>(out.data()), size);
  };
  auto compressorRemover = irs::make_finally([]() {irs::compression::mock::test_compressor::functions().compress_mock = nullptr; });
  auto logicalCollection = vocbase.createCollection(collectionJson->slice());
  ASSERT_TRUE((nullptr != logicalCollection));
  auto view = std::dynamic_pointer_cast<arangodb::iresearch::IResearchView>(
    vocbase.createView(viewJson->slice()));
  ASSERT_TRUE((false == !view));
  view->open();
  ASSERT_TRUE(server.server().hasFeature<arangodb::FlushFeature>());

  dataPath =
      ((((irs::utf8_path() /= testFilesystemPath) /=
         std::string("databases")) /=
        (std::string("database-") + std::to_string(vocbase.id()))) /=
       (std::string("arangosearch-") + std::to_string(logicalCollection->id().id()) + "_42"))
          .utf8();
  irs::fs_directory directory(dataPath);
  bool created;
  auto link = logicalCollection->createIndex(linkJson->slice(), created);
  ASSERT_TRUE((false == !link && created));
  auto reader = irs::directory_reader::open(directory);
  EXPECT_EQ(0, reader.reopen().live_docs_count());
  {
    arangodb::transaction::Methods trx(arangodb::transaction::StandaloneContext::Create(vocbase),
      EMPTY, EMPTY, EMPTY,
      arangodb::transaction::Options());
    EXPECT_TRUE((trx.begin().ok()));
    auto* l = dynamic_cast<arangodb::iresearch::IResearchLink*>(link.get());
    ASSERT_TRUE(l != nullptr);
    EXPECT_TRUE((l->insert(trx, arangodb::LocalDocumentId(1), doc0->slice())
      .ok()));
    EXPECT_TRUE((l->commit().ok()));
    EXPECT_EQ(0, reader.reopen().live_docs_count());

    EXPECT_TRUE((trx.commit().ok()));
    EXPECT_TRUE((l->commit().ok()));
  }

  EXPECT_EQ(1, reader.reopen().live_docs_count());

  {
    arangodb::transaction::Methods trx(arangodb::transaction::StandaloneContext::Create(vocbase),
      EMPTY, EMPTY, EMPTY,
      arangodb::transaction::Options());
    EXPECT_TRUE((trx.begin().ok()));
    auto* l = dynamic_cast<arangodb::iresearch::IResearchLink*>(link.get());
    ASSERT_TRUE(l != nullptr);
    EXPECT_TRUE((l->insert(trx, arangodb::LocalDocumentId(2), doc1->slice())
      .ok()));
    EXPECT_TRUE((trx.commit().ok()));
    EXPECT_TRUE((l->commit().ok()));
  }

  EXPECT_EQ(2, reader.reopen().live_docs_count());
  std::set<std::string> expected;
  auto abcSlice = doc0->slice().get("abc");
  expected.emplace(reinterpret_cast<const char*>(abcSlice.start()), abcSlice.byteSize());
  auto abc2Slice = doc1->slice().get("ghi");
  expected.emplace(reinterpret_cast<const char*>(abc2Slice.start()), abc2Slice.byteSize());
  EXPECT_EQ(expected, compressed_values);
}

TEST_F(IResearchLinkTest, test_write_with_custom_compression_nondefault_mixed_with_sort) {
  static std::vector<std::string> const EMPTY;
  auto doc0 = arangodb::velocypack::Parser::fromJson("{ \"abc\": \"def\", \"abc2\":\"aaa\", \"sort\":\"ps\" }");
  auto doc1 = arangodb::velocypack::Parser::fromJson("{ \"ghi\": \"jkl\", \"sort\":\"pp\" }");
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
  std::string dataPath = ((((irs::utf8_path() /= testFilesystemPath) /= std::string("databases")) /=
    (std::string("database-") + std::to_string(vocbase.id()))) /=
    std::string("arangosearch-42"))
    .utf8();
  auto linkJson = arangodb::velocypack::Parser::fromJson(
    "{ \"id\": 42, \"type\": \"arangosearch\", \"view\": \"42\", "
    "\"includeAllFields\": true,\
    \"primarySort\":[{\"field\":\"sort\", \"direction\":\"asc\"}],\
    \"primarySortCompression\":\"test\",\
    \"storedValues\":[{\"fields\":[\"abc\"], \"compression\":\"test\"},\
                      {\"fields\":[\"abc2\"], \"compression\":\"lz4\"},\
                      {\"fields\":[\"ghi\"], \"compression\":\"test\"}]\
    }");
  auto collectionJson = arangodb::velocypack::Parser::fromJson(
    "{ \"name\": \"testCollection\" }");
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
  std::set<std::string> compressed_values;
  irs::compression::mock::test_compressor::functions().compress_mock = [&compressed_values](irs::byte_type* src, size_t size, irs::bstring& out)->irs::bytes_ref {
    out.append(src, size);
    compressed_values.emplace(reinterpret_cast<const char*>(src), size);
    return irs::bytes_ref(reinterpret_cast<const irs::byte_type*>(out.data()), size);
  };
  auto compressorRemover = irs::make_finally([]() {irs::compression::mock::test_compressor::functions().compress_mock = nullptr; });
  auto logicalCollection = vocbase.createCollection(collectionJson->slice());
  ASSERT_TRUE((nullptr != logicalCollection));
  auto view = std::dynamic_pointer_cast<arangodb::iresearch::IResearchView>(
    vocbase.createView(viewJson->slice()));
  ASSERT_TRUE((false == !view));
  view->open();
  ASSERT_TRUE(server.server().hasFeature<arangodb::FlushFeature>());

  dataPath =
      ((((irs::utf8_path() /= testFilesystemPath) /=
         std::string("databases")) /=
        (std::string("database-") + std::to_string(vocbase.id()))) /=
       (std::string("arangosearch-") + std::to_string(logicalCollection->id().id()) + "_42"))
          .utf8();
  irs::fs_directory directory(dataPath);
  bool created;
  auto link = logicalCollection->createIndex(linkJson->slice(), created);
  ASSERT_TRUE((false == !link && created));
  auto reader = irs::directory_reader::open(directory);
  EXPECT_EQ(0, reader.reopen().live_docs_count());
  {
    arangodb::transaction::Methods trx(arangodb::transaction::StandaloneContext::Create(vocbase),
      EMPTY, EMPTY, EMPTY,
      arangodb::transaction::Options());
    EXPECT_TRUE((trx.begin().ok()));
    auto* l = dynamic_cast<arangodb::iresearch::IResearchLink*>(link.get());
    ASSERT_TRUE(l != nullptr);
    EXPECT_TRUE((l->insert(trx, arangodb::LocalDocumentId(1), doc0->slice())
      .ok()));
    EXPECT_TRUE((l->commit().ok()));
    EXPECT_EQ(0, reader.reopen().live_docs_count());

    EXPECT_TRUE((trx.commit().ok()));
    EXPECT_TRUE((l->commit().ok()));
  }

  EXPECT_EQ(1, reader.reopen().live_docs_count());

  {
    arangodb::transaction::Methods trx(arangodb::transaction::StandaloneContext::Create(vocbase),
      EMPTY, EMPTY, EMPTY,
      arangodb::transaction::Options());
    EXPECT_TRUE((trx.begin().ok()));
    auto* l = dynamic_cast<arangodb::iresearch::IResearchLink*>(link.get());
    ASSERT_TRUE(l != nullptr);
    EXPECT_TRUE((l->insert(trx, arangodb::LocalDocumentId(2), doc1->slice())
      .ok()));
    EXPECT_TRUE((trx.commit().ok()));
    EXPECT_TRUE((l->commit().ok()));
  }

  EXPECT_EQ(2, reader.reopen().live_docs_count());
  std::set<std::string> expected;
  auto sortSlice = doc0->slice().get("sort");
  expected.emplace(reinterpret_cast<const char*>(sortSlice.start()), sortSlice.byteSize());
  auto abcSlice = doc0->slice().get("abc");
  expected.emplace(reinterpret_cast<const char*>(abcSlice.start()), abcSlice.byteSize());
  auto sortSlice1 = doc1->slice().get("sort");
  expected.emplace(reinterpret_cast<const char*>(sortSlice1.start()), sortSlice1.byteSize());
  auto abc2Slice = doc1->slice().get("ghi");
  expected.emplace(reinterpret_cast<const char*>(abc2Slice.start()), abc2Slice.byteSize());
  EXPECT_EQ(expected, compressed_values);
}

TEST_F(IResearchLinkTest, test_write_with_custom_compression_nondefault_mixed_with_sort_encrypted) {
  auto linkCallbackRemover = arangodb::iresearch::IResearchLinkMock::setCallbakForScope([](irs::directory& dir) {
    dir.attributes().emplace<iresearch::mock::test_encryption>(enc_block_size);
    });
  static std::vector<std::string> const EMPTY;
  auto doc0 = arangodb::velocypack::Parser::fromJson("{ \"abc\": \"def\", \"abc2\":\"aaa\", \"sort\":\"ps\" }");
  auto doc1 = arangodb::velocypack::Parser::fromJson("{ \"ghi\": \"jkl\", \"sort\":\"pp\" }");
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
  std::string dataPath = ((((irs::utf8_path() /= testFilesystemPath) /= std::string("databases")) /=
    (std::string("database-") + std::to_string(vocbase.id()))) /=
    std::string("arangosearch-42"))
    .utf8();
  auto linkJson = arangodb::velocypack::Parser::fromJson(
    "{ \"id\": 42, \"type\": \"arangosearch\", \"view\": \"42\", "
    "\"includeAllFields\": true,\
    \"primarySort\":[{\"field\":\"sort\", \"direction\":\"asc\"}],\
    \"primarySortCompression\":\"test\",\
    \"storedValues\":[{\"fields\":[\"abc\"], \"compression\":\"test\"},\
                      {\"fields\":[\"abc2\"], \"compression\":\"lz4\"},\
                      {\"fields\":[\"ghi\"], \"compression\":\"test\"}]\
    }");
  auto collectionJson = arangodb::velocypack::Parser::fromJson(
    "{ \"name\": \"testCollection\" }");
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
  std::set<std::string> compressed_values;
  irs::compression::mock::test_compressor::functions().compress_mock = [&compressed_values]
      (irs::byte_type* src, size_t size, irs::bstring& out)->irs::bytes_ref {
    out.append(src, size);
    compressed_values.emplace(reinterpret_cast<const char*>(src), size);
    return irs::bytes_ref(reinterpret_cast<const irs::byte_type*>(out.data()), size);
  };
  auto compressorRemover = irs::make_finally([]() {irs::compression::mock::test_compressor::functions().compress_mock = nullptr; });
  auto logicalCollection = vocbase.createCollection(collectionJson->slice());
  ASSERT_TRUE((nullptr != logicalCollection));
  auto view = std::dynamic_pointer_cast<arangodb::iresearch::IResearchView>(
    vocbase.createView(viewJson->slice()));
  ASSERT_TRUE((false == !view));
  view->open();
  ASSERT_TRUE(server.server().hasFeature<arangodb::FlushFeature>());

  dataPath =
      ((((irs::utf8_path() /= testFilesystemPath) /=
         std::string("databases")) /=
        (std::string("database-") + std::to_string(vocbase.id()))) /=
       (std::string("arangosearch-") + std::to_string(logicalCollection->id().id()) + "_42"))
          .utf8();
  irs::fs_directory directory(dataPath);
  directory.attributes().emplace<iresearch::mock::test_encryption>(enc_block_size);
  bool created;
  auto link = logicalCollection->createIndex(linkJson->slice(), created);
  ASSERT_TRUE((false == !link && created));
  auto reader = irs::directory_reader::open(directory);
  EXPECT_EQ(0, reader.reopen().live_docs_count());
  {
    arangodb::transaction::Methods trx(arangodb::transaction::StandaloneContext::Create(vocbase),
      EMPTY, EMPTY, EMPTY,
      arangodb::transaction::Options());
    EXPECT_TRUE((trx.begin().ok()));
    auto* l = dynamic_cast<arangodb::iresearch::IResearchLink*>(link.get());
    ASSERT_TRUE(l != nullptr);
    EXPECT_TRUE((l->insert(trx, arangodb::LocalDocumentId(1), doc0->slice())
      .ok()));
    EXPECT_TRUE((l->commit().ok()));
    EXPECT_EQ(0, reader.reopen().live_docs_count());

    EXPECT_TRUE((trx.commit().ok()));
    EXPECT_TRUE((l->commit().ok()));
  }

  EXPECT_EQ(1, reader.reopen().live_docs_count());

  {
    arangodb::transaction::Methods trx(arangodb::transaction::StandaloneContext::Create(vocbase),
      EMPTY, EMPTY, EMPTY,
      arangodb::transaction::Options());
    EXPECT_TRUE((trx.begin().ok()));
    auto* l = dynamic_cast<arangodb::iresearch::IResearchLink*>(link.get());
    ASSERT_TRUE(l != nullptr);
    EXPECT_TRUE((l->insert(trx, arangodb::LocalDocumentId(2), doc1->slice())
      .ok()));
    EXPECT_TRUE((trx.commit().ok()));
    EXPECT_TRUE((l->commit().ok()));
  }

  EXPECT_EQ(2, reader.reopen().live_docs_count());
  std::set<std::string> expected;
  auto sortSlice = doc0->slice().get("sort");
  expected.emplace(reinterpret_cast<const char*>(sortSlice.start()), sortSlice.byteSize());
  auto abcSlice = doc0->slice().get("abc");
  expected.emplace(reinterpret_cast<const char*>(abcSlice.start()), abcSlice.byteSize());
  auto sortSlice1 = doc1->slice().get("sort");
  expected.emplace(reinterpret_cast<const char*>(sortSlice1.start()), sortSlice1.byteSize());
  auto abc2Slice = doc1->slice().get("ghi");
  expected.emplace(reinterpret_cast<const char*>(abc2Slice.start()), abc2Slice.byteSize());
  EXPECT_EQ(expected, compressed_values);
}

TEST_F(IResearchLinkTest, test_maintenance_disabled_at_creation) {
  using namespace arangodb;
  using namespace arangodb::iresearch;

  std::mutex mtx;
  std::condition_variable cv;
  auto& feature = server.getFeature<IResearchFeature>();

  auto blockQueue = [&](){
    {
      auto lock = irs::make_lock_guard(mtx);
    }
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

  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
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
  auto view = std::dynamic_pointer_cast<IResearchView>(vocbase.createView(viewJson->slice()));
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
  auto blockQueue = [&](){
    ++step;
    {
      auto lock = irs::make_lock_guard(mtx);
    }
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

  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
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
  auto view = std::dynamic_pointer_cast<IResearchView>(vocbase.createView(viewJson->slice()));
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

      cv.wait(lock); // release current blocker
      waitForBlocker(); // wait for the next blocker
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
      waitForBlocker(); // wait for the next blocker

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
      meta._consolidationPolicy = IResearchViewMeta::DEFAULT()._consolidationPolicy;
      meta._commitIntervalMsec = 0;
      ASSERT_TRUE(linkImpl->properties(meta).ok());

      ASSERT_EQ(std::make_tuple(size_t(1), size_t(1), size_t(1)),
                feature.stats(ThreadGroup::_1));
      ASSERT_TRUE(feature.queue(ThreadGroup::_1, 500ms, blockQueue));
      ASSERT_EQ(std::make_tuple(size_t(1), size_t(2), size_t(1)),
                feature.stats(ThreadGroup::_1));

      cv.wait(lock);
      waitForBlocker(); // wait for the next blocker

      ASSERT_EQ(std::make_tuple(size_t(1), size_t(1), size_t(1)),
                feature.stats(ThreadGroup::_1));
    }

#ifdef ARANGODB_ENABLE_FAILURE_TESTS
    // ensure consolidation is rescheduled after exception
    {
      auto clearFailurePoints = arangodb::scopeGuard(TRI_ClearFailurePointsDebugging);
      TRI_AddFailurePointDebugging("IResearchConsolidationTask::lockDataStore");

      ASSERT_EQ(std::make_tuple(size_t(1), size_t(1), size_t(1)),
                feature.stats(ThreadGroup::_1));
      ASSERT_TRUE(feature.queue(ThreadGroup::_1, 500ms, blockQueue));
      ASSERT_EQ(std::make_tuple(size_t(1), size_t(2), size_t(1)),
                feature.stats(ThreadGroup::_1));

      cv.wait(lock);
      waitForBlocker(); // wait for the next blocker

      ASSERT_EQ(std::make_tuple(size_t(1), size_t(1), size_t(1)),
                feature.stats(ThreadGroup::_1));
    }

    // ensure consolidation is rescheduled after exception
    {
      auto clearFailurePoints = arangodb::scopeGuard(TRI_ClearFailurePointsDebugging);
      TRI_AddFailurePointDebugging("IResearchConsolidationTask::consolidateUnsafe");

      ASSERT_EQ(std::make_tuple(size_t(1), size_t(1), size_t(1)),
                feature.stats(ThreadGroup::_1));
      ASSERT_TRUE(feature.queue(ThreadGroup::_1, 500ms, blockQueue));
      ASSERT_EQ(std::make_tuple(size_t(1), size_t(2), size_t(1)),
                feature.stats(ThreadGroup::_1));

      cv.wait(lock);
      waitForBlocker(); // wait for the next blocker

      ASSERT_EQ(std::make_tuple(size_t(1), size_t(1), size_t(1)),
                feature.stats(ThreadGroup::_1));
    }
#endif

    // ensure no commit is scheduled after dropping a link
    {
      ASSERT_TRUE(link->drop().ok());
      ASSERT_TRUE(asyncSelf->terminationRequested());

      ASSERT_TRUE(cv.wait_for(lock, 10s, [&feature](){
        return std::make_tuple(size_t(0), size_t(0), size_t(1))
                 == feature.stats(ThreadGroup::_1);
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
  auto blockQueue = [&](){
    ++step;
    {
      auto lock = irs::make_lock_guard(mtx);
    }
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

  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
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
  auto view = std::dynamic_pointer_cast<IResearchView>(vocbase.createView(viewJson->slice()));
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

      cv.wait(lock); // release current blocker
      waitForBlocker(); // wait for the next blocker
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
      waitForBlocker(); // wait for the next blocker

      ASSERT_EQ(std::make_tuple(size_t(1), size_t(1), size_t(1)),
                feature.stats(ThreadGroup::_0));
    }

#ifdef ARANGODB_ENABLE_FAILURE_TESTS
    // ensure commit is rescheduled after exception
    {
      auto clearFailurePoints = arangodb::scopeGuard(TRI_ClearFailurePointsDebugging);
      TRI_AddFailurePointDebugging("IResearchCommitTask::lockDataStore");

      ASSERT_EQ(std::make_tuple(size_t(1), size_t(1), size_t(1)),
                feature.stats(ThreadGroup::_0));
      ASSERT_TRUE(feature.queue(ThreadGroup::_0, 500ms, blockQueue));
      ASSERT_EQ(std::make_tuple(size_t(1), size_t(2), size_t(1)),
                feature.stats(ThreadGroup::_0));

      cv.wait(lock);
      waitForBlocker(); // wait for the next blocker

      ASSERT_EQ(std::make_tuple(size_t(1), size_t(1), size_t(1)),
                feature.stats(ThreadGroup::_0));
    }

    // ensure commit is rescheduled after exception
    {
      auto clearFailurePoints = arangodb::scopeGuard(TRI_ClearFailurePointsDebugging);
      TRI_AddFailurePointDebugging("IResearchCommitTask::commitUnsafe");

      ASSERT_EQ(std::make_tuple(size_t(1), size_t(1), size_t(1)),
                feature.stats(ThreadGroup::_0));
      ASSERT_TRUE(feature.queue(ThreadGroup::_0, 500ms, blockQueue));
      ASSERT_EQ(std::make_tuple(size_t(1), size_t(2), size_t(1)),
                feature.stats(ThreadGroup::_0));

      cv.wait(lock);
      waitForBlocker(); // wait for the next blocker

      ASSERT_EQ(std::make_tuple(size_t(1), size_t(1), size_t(1)),
                feature.stats(ThreadGroup::_0));
    }

    // ensure commit is rescheduled after exception
    {
      auto clearFailurePoints = arangodb::scopeGuard(TRI_ClearFailurePointsDebugging);
      TRI_AddFailurePointDebugging("IResearchCommitTask::cleanupUnsafe");

      ASSERT_EQ(std::make_tuple(size_t(1), size_t(1), size_t(1)),
                feature.stats(ThreadGroup::_0));
      ASSERT_TRUE(feature.queue(ThreadGroup::_0, 500ms, blockQueue));
      ASSERT_EQ(std::make_tuple(size_t(1), size_t(2), size_t(1)),
                feature.stats(ThreadGroup::_0));

      cv.wait(lock);
      waitForBlocker(); // wait for the next blocker

      ASSERT_EQ(std::make_tuple(size_t(1), size_t(1), size_t(1)),
                feature.stats(ThreadGroup::_0));
    }
#endif

    // ensure no commit is scheduled after dropping a link
    {
      ASSERT_TRUE(link->drop().ok());
      ASSERT_TRUE(asyncSelf->terminationRequested());

      ASSERT_TRUE(cv.wait_for(lock, 10s, [&feature](){
        return std::make_tuple(size_t(0), size_t(0), size_t(1))
                 == feature.stats(ThreadGroup::_0);
      }));
    }
  }

  ASSERT_TRUE(view->drop().ok());
  ASSERT_TRUE(logicalCollection->drop().ok());
}
