////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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

#include "utils/misc.hpp"

#include "gtest/gtest.h"

#include "../Mocks/StorageEngineMock.h"
#include "../Mocks/Servers.h"
#include "Agency/Store.h"
#include "AgencyMock.h"
#include "Aql/AstNode.h"
#include "Aql/Variable.h"
#include "Basics/files.h"
#include "Cluster/ClusterComm.h"
#include "Cluster/ClusterInfo.h"
#include "IResearch/IResearchCommon.h"
#include "IResearch/IResearchFeature.h"
#include "IResearch/IResearchLinkHelper.h"
#include "IResearch/IResearchLinkMeta.h"
#include "IResearch/IResearchMMFilesLink.h"
#include "IResearch/IResearchView.h"
#include "RestServer/ViewTypesFeature.h"
#include "Transaction/Methods.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/OperationOptions.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ManagedDocumentResult.h"
#include "common.h"
#include "utils/utf8_path.hpp"
#include "velocypack/Parser.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

class IResearchViewDBServerTest : public ::testing::Test {
 protected:
  arangodb::tests::mocks::MockDBServer server;
  arangodb::consensus::Store& _agencyStore;

  IResearchViewDBServerTest() : server(), _agencyStore(server.getAgencyStore()) {
  }

  void createTestDatabase(TRI_vocbase_t*& vocbase, std::string const name = "testDatabase") {
    vocbase = server.createDatabase(name);
    ASSERT_NE(nullptr, vocbase);
    ASSERT_EQ(name, vocbase->name());
    ASSERT_EQ(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, vocbase->type());
  }

  ~IResearchViewDBServerTest() {
  }
};

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

TEST_F(IResearchViewDBServerTest, test_drop) {
  auto* ci = arangodb::ClusterInfo::instance();
  ASSERT_TRUE(nullptr != ci);
  TRI_vocbase_t* vocbase;  // will be owned by DatabaseFeature
  createTestDatabase(vocbase);

  // drop empty
  {
    auto json = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
    arangodb::LogicalView::ptr wiew;
    ASSERT_TRUE((arangodb::iresearch::IResearchView::factory()
                     .create(wiew, *vocbase, json->slice())
                     .ok()));
    EXPECT_TRUE((false == !wiew));
    auto* impl = dynamic_cast<arangodb::iresearch::IResearchView*>(wiew.get());
    EXPECT_TRUE((nullptr != impl));

    EXPECT_TRUE((true == impl->drop().ok()));
  }

  // drop non-empty
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testCollection0\" }");
    auto linkJson = arangodb::velocypack::Parser::fromJson(
        "{ \"view\": \"testView0\", \"type\": \"arangosearch\", "
        "\"includeAllFields\": true }");
    auto viewJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testView0\", \"type\": \"arangosearch\" }");
    auto const wiewId = std::to_string(ci->uniqid() + 1);  // +1 because LogicalView creation will generate a new ID
    auto logicalCollection = vocbase->createCollection(collectionJson->slice());
    ASSERT_TRUE((false == !logicalCollection));
    arangodb::LogicalView::ptr wiew;
    ASSERT_TRUE((arangodb::iresearch::IResearchView::factory()
                     .create(wiew, *vocbase, viewJson->slice())
                     .ok()));
    EXPECT_TRUE((false == !wiew));
    auto* impl = dynamic_cast<arangodb::iresearch::IResearchView*>(wiew.get());
    EXPECT_TRUE((nullptr != impl));

    // ensure we have shard view in vocbase
    bool created;
    auto index = logicalCollection->createIndex(linkJson->slice(), created);
    ASSERT_TRUE((false == !index));
    auto link = std::dynamic_pointer_cast<arangodb::iresearch::IResearchLink>(index);
    ASSERT_TRUE((false == !link));

    static auto visitor = [](TRI_voc_cid_t) -> bool { return false; };
    EXPECT_TRUE((false == impl->visitCollections(visitor)));
    EXPECT_TRUE((false == !arangodb::iresearch::IResearchLinkHelper::find(*logicalCollection,
                                                                          *wiew)));
    EXPECT_TRUE((true == impl->drop().ok()));
    EXPECT_TRUE((true == !arangodb::iresearch::IResearchLinkHelper::find(*logicalCollection,
                                                                         *wiew)));
    EXPECT_TRUE((false == impl->visitCollections(visitor)));  // list of links is not modified after link drop
  }

  // drop non-empty (drop failure)
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testCollection1\" }");
    auto linkJson = arangodb::velocypack::Parser::fromJson(
        "{ \"view\": \"testView1\", \"type\": \"arangosearch\", "
        "\"includeAllFields\": true }");
    auto viewJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testView1\", \"type\": \"arangosearch\" }");
    auto const wiewId = std::to_string(ci->uniqid() + 1);  // +1 because LogicalView creation will generate a new ID
    auto logicalCollection = vocbase->createCollection(collectionJson->slice());
    ASSERT_TRUE((false == !logicalCollection));
    arangodb::LogicalView::ptr wiew;
    ASSERT_TRUE((arangodb::iresearch::IResearchView::factory()
                     .create(wiew, *vocbase, viewJson->slice())
                     .ok()));
    EXPECT_TRUE((false == !wiew));
    auto* impl = dynamic_cast<arangodb::iresearch::IResearchView*>(wiew.get());
    EXPECT_TRUE((nullptr != impl));

    // ensure we have shard view in vocbase
    bool created;
    auto index = logicalCollection->createIndex(linkJson->slice(), created);
    ASSERT_TRUE((false == !index));
    auto link = std::dynamic_pointer_cast<arangodb::iresearch::IResearchLink>(index);
    ASSERT_TRUE((false == !link));

    static auto visitor = [](TRI_voc_cid_t) -> bool { return false; };
    EXPECT_TRUE((false == impl->visitCollections(visitor)));
    EXPECT_TRUE((false == !arangodb::iresearch::IResearchLinkHelper::find(*logicalCollection,
                                                                          *wiew)));

    auto before = PhysicalCollectionMock::before;
    auto restore = irs::make_finally(
        [&before]() -> void { PhysicalCollectionMock::before = before; });
    PhysicalCollectionMock::before = []() -> void { throw std::exception(); };

    EXPECT_TRUE((!impl->drop().ok()));
    EXPECT_TRUE((false == !arangodb::iresearch::IResearchLinkHelper::find(*logicalCollection,
                                                                          *wiew)));
    EXPECT_TRUE((false == impl->visitCollections(visitor)));
  }
}

TEST_F(IResearchViewDBServerTest, test_drop_cid) {
  TRI_vocbase_t* vocbase;  // will be owned by DatabaseFeature
  createTestDatabase(vocbase);

  auto collectionJson = arangodb::velocypack::Parser::fromJson(
      "{ \"name\": \"testCollection\" }");
  auto linkJson = arangodb::velocypack::Parser::fromJson(
      "{ \"view\": \"testView\", \"type\": \"arangosearch\", "
      "\"includeAllFields\": true }");
  auto viewJson = arangodb::velocypack::Parser::fromJson(
      "{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
  auto logicalCollection = vocbase->createCollection(collectionJson->slice());
  ASSERT_TRUE((false == !logicalCollection));
  arangodb::LogicalView::ptr wiew;
  ASSERT_TRUE((arangodb::iresearch::IResearchView::factory()
                   .create(wiew, *vocbase, viewJson->slice())
                   .ok()));
  EXPECT_TRUE((false == !wiew));
  auto* impl = dynamic_cast<arangodb::iresearch::IResearchView*>(wiew.get());
  EXPECT_TRUE((nullptr != impl));

  // ensure we have shard view in vocbase
  bool created;
  auto index = logicalCollection->createIndex(linkJson->slice(), created);
  ASSERT_TRUE((false == !index));
  auto link = std::dynamic_pointer_cast<arangodb::iresearch::IResearchLink>(index);
  ASSERT_TRUE((false == !link));

  static auto visitor = [](TRI_voc_cid_t) -> bool { return false; };
  EXPECT_TRUE((false == impl->visitCollections(visitor)));
  EXPECT_TRUE((false == !arangodb::iresearch::IResearchLinkHelper::find(*logicalCollection,
                                                                        *wiew)));
  EXPECT_TRUE((impl->unlink(logicalCollection->id()).ok()));
  EXPECT_TRUE((false == !arangodb::iresearch::IResearchLinkHelper::find(*logicalCollection,
                                                                        *wiew)));
  EXPECT_TRUE((true == impl->visitCollections(visitor)));
  EXPECT_TRUE((impl->unlink(logicalCollection->id()).ok()));
}

TEST_F(IResearchViewDBServerTest, test_drop_database) {
  auto* ci = arangodb::ClusterInfo::instance();
  ASSERT_TRUE((nullptr != ci));

  auto collectionJson = arangodb::velocypack::Parser::fromJson(
      "{ \"name\": \"testCollection\" }");
  auto viewCreateJson = arangodb::velocypack::Parser::fromJson(
      "{ \"id\": \"42\", \"name\": \"testView\", \"type\": \"arangosearch\" }");
  auto viewUpdateJson = arangodb::velocypack::Parser::fromJson(
      "{ \"links\": { \"testCollection\": { \"includeAllFields\": true } } }");

  size_t beforeCount = 0;
  auto before = PhysicalCollectionMock::before;
  auto restore = irs::make_finally(
      [&before]() -> void { PhysicalCollectionMock::before = before; });
  PhysicalCollectionMock::before = [&beforeCount]() -> void { ++beforeCount; };

  TRI_vocbase_t* vocbase;  // will be owned by DatabaseFeature
  createTestDatabase(vocbase, "testDatabase" TOSTRING(__LINE__));

  auto logicalCollection = vocbase->createCollection(collectionJson->slice());
  ASSERT_TRUE((false == !logicalCollection));
  EXPECT_TRUE(
      (ci->createViewCoordinator(vocbase->name(), "42", viewCreateJson->slice()).ok()));
  auto logicalWiew = ci->getView(vocbase->name(), "42");  // link creation requires cluster-view to be in ClusterInfo instead of TRI_vocbase_t
  ASSERT_TRUE((false == !logicalWiew));
  auto* wiewImpl = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalWiew.get());
  ASSERT_TRUE((false == !wiewImpl));

  beforeCount = 0;  // reset before call to StorageEngine::createView(...)
  auto res = logicalWiew->properties(viewUpdateJson->slice(), true);
  ASSERT_TRUE(true == res.ok());
  EXPECT_TRUE((17 == beforeCount));  // +1 for StorageEngineMock::createIndex(...) and then for various other activities
}

TEST_F(IResearchViewDBServerTest, test_ensure) {
  auto* ci = arangodb::ClusterInfo::instance();
  ASSERT_TRUE((nullptr != ci));
  TRI_vocbase_t* vocbase;  // will be owned by DatabaseFeature
  createTestDatabase(vocbase);

  auto collectionJson = arangodb::velocypack::Parser::fromJson(
      "{ \"name\": \"testCollection\" }");
  auto linkJson = arangodb::velocypack::Parser::fromJson(
      "{ \"view\": \"testView\", \"type\": \"arangosearch\", "
      "\"includeAllFields\": true }");
  auto viewJson = arangodb::velocypack::Parser::fromJson(
      "{ \"name\": \"testView\", \"type\": \"arangosearch\", \"collections\": "
      "[ 3, 4, 5 ] }");
  auto logicalCollection = vocbase->createCollection(collectionJson->slice());
  ASSERT_TRUE((false == !logicalCollection));
  arangodb::LogicalView::ptr wiew;
  ASSERT_TRUE((arangodb::iresearch::IResearchView::factory()
                   .create(wiew, *vocbase, viewJson->slice())
                   .ok()));
  EXPECT_TRUE((false == !wiew));
  auto* impl = dynamic_cast<arangodb::iresearch::IResearchView*>(wiew.get());
  EXPECT_TRUE((nullptr != impl));

  bool created;
  auto index = logicalCollection->createIndex(linkJson->slice(), created);
  ASSERT_TRUE((false == !index));
  auto link = std::dynamic_pointer_cast<arangodb::iresearch::IResearchLink>(index);
  ASSERT_TRUE((false == !link));

  static auto visitor = [](TRI_voc_cid_t) -> bool { return false; };
  EXPECT_TRUE((false == wiew->visitCollections(visitor)));  // no collections in view
  EXPECT_TRUE((false == !arangodb::iresearch::IResearchLinkHelper::find(*logicalCollection,
                                                                        *wiew)));
}

TEST_F(IResearchViewDBServerTest, test_make) {
  auto* ci = arangodb::ClusterInfo::instance();
  ASSERT_TRUE(nullptr != ci);

  // make DBServer view
  {
    auto const wiewId = ci->uniqid() + 1;  // +1 because LogicalView creation will generate a new ID
    auto json = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                          "testVocbase");
    arangodb::LogicalView::ptr wiew;
    ASSERT_TRUE((arangodb::iresearch::IResearchView::factory()
                     .instantiate(wiew, vocbase, json->slice(), 42)
                     .ok()));
    EXPECT_TRUE((false == !wiew));
    auto* impl = dynamic_cast<arangodb::iresearch::IResearchView*>(wiew.get());
    EXPECT_TRUE((nullptr != impl));

    EXPECT_TRUE((std::string("testView") == wiew->name()));
    EXPECT_TRUE((false == wiew->deleted()));
    EXPECT_TRUE((wiewId == wiew->id()));
    EXPECT_TRUE((impl->id() == wiew->planId()));  // same as view ID
    EXPECT_TRUE((42 == wiew->planVersion()));  // when creating via vocbase planVersion is always 0
    EXPECT_TRUE((arangodb::iresearch::DATA_SOURCE_TYPE == wiew->type()));
    EXPECT_TRUE((&vocbase == &(wiew->vocbase())));
  }
}

TEST_F(IResearchViewDBServerTest, test_open) {
  auto* ci = arangodb::ClusterInfo::instance();
  ASSERT_TRUE(nullptr != ci);

  // open empty
  {
    auto json = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                          "testVocbase");
    arangodb::LogicalView::ptr wiew;
    ASSERT_TRUE((arangodb::iresearch::IResearchView::factory()
                     .instantiate(wiew, vocbase, json->slice(), 42)
                     .ok()));
    EXPECT_TRUE((false == !wiew));
    auto* impl = dynamic_cast<arangodb::iresearch::IResearchView*>(wiew.get());
    EXPECT_TRUE((nullptr != impl));

    static auto visitor = [](TRI_voc_cid_t) -> bool { return false; };
    EXPECT_TRUE((true == impl->visitCollections(visitor)));
    wiew->open();
  }

  // open non-empty
  {
    auto const wiewId = std::to_string(ci->uniqid() + 1);  // +1 because LogicalView creation will generate a new ID
    std::string dataPath =
        (((irs::utf8_path() /= server.testFilesystemPath()) /=
          std::string("databases")) /= std::string("arangosearch-123"))
            .utf8();
    auto collectionJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testCollection\" }");
    auto json = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                          "testVocbase");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    ASSERT_TRUE((false == !logicalCollection));
    arangodb::LogicalView::ptr wiew;
    ASSERT_TRUE((arangodb::iresearch::IResearchView::factory()
                     .instantiate(wiew, vocbase, json->slice(), 42)
                     .ok()));
    EXPECT_TRUE((false == !wiew));
    auto* impl = dynamic_cast<arangodb::iresearch::IResearchView*>(wiew.get());
    EXPECT_TRUE((nullptr != impl));

    // ensure we have shard view in vocbase
    struct Link : public arangodb::iresearch::IResearchLink {
      Link(TRI_idx_iid_t id, arangodb::LogicalCollection& col)
          : IResearchLink(id, col) {}
    } link(42, *logicalCollection);
    auto asyncLinkPtr =
        std::make_shared<arangodb::iresearch::IResearchLink::AsyncLinkPtr::element_type>(&link);

    static auto visitor = [](TRI_voc_cid_t) -> bool { return false; };
    EXPECT_TRUE((true == impl->visitCollections(visitor)));
    EXPECT_TRUE((true == impl->link(asyncLinkPtr).ok()));
    EXPECT_TRUE((false == impl->visitCollections(visitor)));
    wiew->open();
  }
}

TEST_F(IResearchViewDBServerTest, test_query) {
  auto* ci = arangodb::ClusterInfo::instance();
  ASSERT_TRUE((nullptr != ci));

  auto createJson = arangodb::velocypack::Parser::fromJson(
      "{ \
    \"id\": \"42\", \
    \"name\": \"testView\", \
    \"type\": \"arangosearch\" \
  }");
  static std::vector<std::string> const EMPTY;
  arangodb::aql::AstNode noop(arangodb::aql::AstNodeType::NODE_TYPE_FILTER);
  arangodb::aql::AstNode noopChild(arangodb::aql::AstNodeValue(true));

  noop.addMember(&noopChild);

  // no filter/order provided, means "RETURN *"
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testCollection\" }");
    auto linkJson = arangodb::velocypack::Parser::fromJson(
        "{ \"view\": \"testView\", \"type\": \"arangosearch\", "
        "\"includeAllFields\": true }");
    TRI_vocbase_t* vocbase;  // will be owned by DatabaseFeature
    createTestDatabase(vocbase, "testDatabase0");
    auto logicalCollection = vocbase->createCollection(collectionJson->slice());
    ASSERT_TRUE(nullptr != logicalCollection);
    arangodb::LogicalView::ptr logicalWiew;
    ASSERT_TRUE((arangodb::iresearch::IResearchView::factory()
                     .create(logicalWiew, *vocbase, createJson->slice())
                     .ok()));
    ASSERT_TRUE((false == !logicalWiew));
    auto* wiewImpl =
        dynamic_cast<arangodb::iresearch::IResearchView*>(logicalWiew.get());
    ASSERT_TRUE((false == !wiewImpl));

    bool created;
    auto index = logicalCollection->createIndex(linkJson->slice(), created);
    ASSERT_TRUE((false == !index));
    auto link = std::dynamic_pointer_cast<arangodb::iresearch::IResearchLink>(index);
    ASSERT_TRUE((false == !link));

    arangodb::transaction::Methods trx(
        arangodb::transaction::StandaloneContext::Create(*vocbase),
        std::vector<std::string>{logicalCollection->name()}, EMPTY, EMPTY,
        arangodb::transaction::Options());
    EXPECT_TRUE((trx.begin().ok()));
    arangodb::HashSet<TRI_voc_cid_t> collections;
    collections.insert(logicalCollection->id());
    auto* snapshot =
        wiewImpl->snapshot(trx, arangodb::iresearch::IResearchView::SnapshotMode::FindOrCreate,
                           &collections);
    EXPECT_TRUE(0 == snapshot->docs_count());
    EXPECT_TRUE((trx.commit().ok()));
  }

  // ordered iterator
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testCollection\" }");
    auto linkJson = arangodb::velocypack::Parser::fromJson(
        "{ \"view\": \"testView\", \"type\": \"arangosearch\", "
        "\"includeAllFields\": true }");
    TRI_vocbase_t* vocbase;  // will be owned by DatabaseFeature
    createTestDatabase(vocbase, "testDatabase1");
    auto logicalCollection = vocbase->createCollection(collectionJson->slice());
    ASSERT_TRUE(nullptr != logicalCollection);
    arangodb::LogicalView::ptr logicalWiew;
    ASSERT_TRUE((arangodb::iresearch::IResearchView::factory()
                     .create(logicalWiew, *vocbase, createJson->slice())
                     .ok()));
    ASSERT_TRUE((false == !logicalWiew));
    auto* wiewImpl =
        dynamic_cast<arangodb::iresearch::IResearchView*>(logicalWiew.get());
    ASSERT_TRUE((false == !wiewImpl));

    bool created;
    auto index = logicalCollection->createIndex(linkJson->slice(), created);
    ASSERT_TRUE((false == !index));
    auto link = std::dynamic_pointer_cast<arangodb::iresearch::IResearchLink>(index);
    ASSERT_TRUE((false == !link));

    // fill with test data
    {
      auto doc = arangodb::velocypack::Parser::fromJson("{ \"key\": 1 }");
      arangodb::iresearch::IResearchLinkMeta meta;
      meta._includeAllFields = true;
      arangodb::transaction::Methods trx(
          arangodb::transaction::StandaloneContext::Create(*vocbase), EMPTY,
          std::vector<std::string>{logicalCollection->name()}, EMPTY,
          arangodb::transaction::Options());
      EXPECT_TRUE((trx.begin().ok()));

      for (size_t i = 0; i < 12; ++i) {
        EXPECT_TRUE((link->insert(trx, arangodb::LocalDocumentId(i), doc->slice(),
                                  arangodb::Index::OperationMode::normal)
                         .ok()));
      }

      EXPECT_TRUE((trx.commit().ok()));
      EXPECT_TRUE(link->commit().ok());
    }

    arangodb::transaction::Methods trx(
        arangodb::transaction::StandaloneContext::Create(*vocbase),
        std::vector<std::string>{logicalCollection->name()}, EMPTY, EMPTY,
        arangodb::transaction::Options());
    EXPECT_TRUE((trx.begin().ok()));
    arangodb::HashSet<TRI_voc_cid_t> collections;
    collections.insert(logicalCollection->id());
    auto* snapshot =
        wiewImpl->snapshot(trx, arangodb::iresearch::IResearchView::SnapshotMode::FindOrCreate,
                           &collections);
    EXPECT_TRUE(12 == snapshot->docs_count());
    EXPECT_TRUE((trx.commit().ok()));
  }

  // snapshot isolation
  {
    auto links = arangodb::velocypack::Parser::fromJson(
        "{ \
      \"links\": { \"testCollection\": { \"includeAllFields\" : true } } \
    }");
    auto collectionJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testCollection\", \"id\":442 }");

    TRI_vocbase_t* vocbase;  // will be owned by DatabaseFeature
    createTestDatabase(vocbase, "testDatabase" TOSTRING(__LINE__));
    auto logicalCollection = vocbase->createCollection(collectionJson->slice());
    std::vector<std::string> collections{logicalCollection->name()};
    EXPECT_TRUE(
        (ci->createViewCoordinator(vocbase->name(), "42", createJson->slice()).ok()));
    auto logicalWiew = ci->getView(vocbase->name(), "42");  // link creation requires cluster-view to be in ClusterInfo instead of TRI_vocbase_t
    EXPECT_TRUE((false == !logicalWiew));
    auto* wiewImpl =
        dynamic_cast<arangodb::iresearch::IResearchView*>(logicalWiew.get());
    EXPECT_TRUE((false == !wiewImpl));
    arangodb::Result res = logicalWiew->properties(links->slice(), true);
    EXPECT_TRUE(true == res.ok());
    EXPECT_TRUE((false == logicalCollection->getIndexes().empty()));

    // fill with test data
    {
      arangodb::transaction::Methods trx(arangodb::transaction::StandaloneContext::Create(*vocbase),
                                         EMPTY, collections, EMPTY,
                                         arangodb::transaction::Options());
      EXPECT_TRUE((trx.begin().ok()));

      arangodb::ManagedDocumentResult inserted;
      arangodb::OperationOptions options;
      for (size_t i = 1; i <= 12; ++i) {
        auto doc = arangodb::velocypack::Parser::fromJson(
            std::string("{ \"key\": ") + std::to_string(i) + " }");
        logicalCollection->insert(&trx, doc->slice(), inserted, options, false);
      }

      EXPECT_TRUE((trx.commit().ok()));
    }

    arangodb::transaction::Options trxOptions;

    arangodb::transaction::Methods trx0(arangodb::transaction::StandaloneContext::Create(*vocbase),
                                        collections, EMPTY, EMPTY, trxOptions);
    EXPECT_TRUE((trx0.begin().ok()));
    arangodb::HashSet<TRI_voc_cid_t> collectionIds;
    collectionIds.insert(logicalCollection->id());
    EXPECT_TRUE((nullptr == wiewImpl->snapshot(trx0, arangodb::iresearch::IResearchView::SnapshotMode::Find,
                                               &collectionIds)));
    auto* snapshot0 =
        wiewImpl->snapshot(trx0, arangodb::iresearch::IResearchView::SnapshotMode::SyncAndReplace,
                           &collectionIds);
    EXPECT_TRUE((snapshot0 == wiewImpl->snapshot(trx0, arangodb::iresearch::IResearchView::SnapshotMode::Find,
                                                 &collectionIds)));
    EXPECT_TRUE(12 == snapshot0->docs_count());
    EXPECT_TRUE((trx0.commit().ok()));

    // add more data
    {
      arangodb::transaction::Methods trx(arangodb::transaction::StandaloneContext::Create(*vocbase),
                                         EMPTY, collections, EMPTY,
                                         arangodb::transaction::Options());
      EXPECT_TRUE((trx.begin().ok()));

      arangodb::ManagedDocumentResult inserted;
      arangodb::OperationOptions options;
      for (size_t i = 13; i <= 24; ++i) {
        auto doc = arangodb::velocypack::Parser::fromJson(
            std::string("{ \"key\": ") + std::to_string(i) + " }");
        logicalCollection->insert(&trx, doc->slice(), inserted, options, false);
      }

      EXPECT_TRUE(trx.commit().ok());
    }

    // old reader sees same data as before
    EXPECT_TRUE(12 == snapshot0->docs_count());

    // new reader sees new data
    arangodb::transaction::Methods trx1(arangodb::transaction::StandaloneContext::Create(*vocbase),
                                        collections, EMPTY, EMPTY, trxOptions);
    EXPECT_TRUE((trx1.begin().ok()));
    auto* snapshot1 =
        wiewImpl->snapshot(trx1, arangodb::iresearch::IResearchView::SnapshotMode::SyncAndReplace,
                           &collectionIds);
    EXPECT_TRUE(24 == snapshot1->docs_count());
    EXPECT_TRUE((trx1.commit().ok()));
  }

  // query while running FlushThread
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testCollection\" }");
    auto viewCreateJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
    auto viewUpdateJson = arangodb::velocypack::Parser::fromJson(
        "{ \"links\": { \"testCollection\": { \"includeAllFields\": true } } "
        "}");
    TRI_vocbase_t* vocbase;  // will be owned by DatabaseFeature
    createTestDatabase(vocbase, "testDatabase" TOSTRING(__LINE__));
    ASSERT_TRUE((nullptr != vocbase));
    auto logicalCollection = vocbase->createCollection(collectionJson->slice());
    EXPECT_TRUE(
        (ci->createViewCoordinator(vocbase->name(), "42", createJson->slice()).ok()));
    auto logicalWiew = ci->getView(vocbase->name(), "42");  // link creation requires cluster-view to be in ClusterInfo instead of TRI_vocbase_t
    ASSERT_TRUE((false == !logicalWiew));
    auto* wiewImpl =
        dynamic_cast<arangodb::iresearch::IResearchView*>(logicalWiew.get());
    ASSERT_TRUE((false == !wiewImpl));
    arangodb::Result res = logicalWiew->properties(viewUpdateJson->slice(), true);
    ASSERT_TRUE(true == res.ok());

    static std::vector<std::string> const EMPTY;
    arangodb::transaction::Options options;

    arangodb::aql::Variable variable("testVariable", 0);

    // test insert + query
    for (size_t i = 1; i < 200; ++i) {
      // insert
      {
        auto doc = arangodb::velocypack::Parser::fromJson(
            std::string("{ \"seq\": ") + std::to_string(i) + " }");
        arangodb::transaction::Methods trx(
            arangodb::transaction::StandaloneContext::Create(*vocbase), EMPTY,
            std::vector<std::string>{logicalCollection->name()}, EMPTY, options);

        EXPECT_TRUE((trx.begin().ok()));
        EXPECT_TRUE((trx.insert(logicalCollection->name(), doc->slice(), arangodb::OperationOptions())
                         .ok()));
        EXPECT_TRUE((trx.commit().ok()));
      }

      // query
      {
        arangodb::transaction::Methods trx(
            arangodb::transaction::StandaloneContext::Create(*vocbase),
            std::vector<std::string>{logicalCollection->name()}, EMPTY, EMPTY,
            arangodb::transaction::Options{});
        EXPECT_TRUE((trx.begin().ok()));
        arangodb::HashSet<TRI_voc_cid_t> collections;
        collections.insert(logicalCollection->id());
        auto* snapshot =
            wiewImpl->snapshot(trx, arangodb::iresearch::IResearchView::SnapshotMode::SyncAndReplace,
                               &collections);
        EXPECT_TRUE(i == snapshot->docs_count());
        EXPECT_TRUE((trx.commit().ok()));
      }
    }
  }
}

TEST_F(IResearchViewDBServerTest, test_rename) {
  auto* ci = arangodb::ClusterInfo::instance();
  ASSERT_TRUE(nullptr != ci);

  // rename empty
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testCollection\" }");
    auto json = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                          "testVocbase");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    ASSERT_TRUE((false == !logicalCollection));
    arangodb::LogicalView::ptr wiew;
    ASSERT_TRUE((arangodb::iresearch::IResearchView::factory()
                     .instantiate(wiew, vocbase, json->slice(), 42)
                     .ok()));
    EXPECT_TRUE((false == !wiew));
    auto* impl = dynamic_cast<arangodb::iresearch::IResearchView*>(wiew.get());
    EXPECT_TRUE((nullptr != impl));

    EXPECT_TRUE((std::string("testView") == wiew->name()));

    {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      wiew->properties(builder, arangodb::LogicalDataSource::makeFlags());
      builder.close();
      EXPECT_TRUE((builder.slice().hasKey("name")));
      EXPECT_TRUE((std::string("testView") == builder.slice().get("name").copyString()));
    }

    EXPECT_TRUE((TRI_ERROR_CLUSTER_UNSUPPORTED == wiew->rename("newName").errorNumber()));

    {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      wiew->properties(builder, arangodb::LogicalDataSource::makeFlags());
      builder.close();
      EXPECT_TRUE((builder.slice().hasKey("name")));
      EXPECT_TRUE((std::string("testView") == builder.slice().get("name").copyString()));
    }

    struct Link : public arangodb::iresearch::IResearchLink {
      Link(TRI_idx_iid_t id, arangodb::LogicalCollection& col)
          : IResearchLink(id, col) {}
    } link(42, *logicalCollection);
    auto asyncLinkPtr =
        std::make_shared<arangodb::iresearch::IResearchLink::AsyncLinkPtr::element_type>(&link);
    EXPECT_TRUE((true == impl->link(asyncLinkPtr).ok()));
  }

  // rename non-empty
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testCollection\" }");
    auto const wiewId = std::to_string(ci->uniqid() + 1);  // +1 because LogicalView creation will generate a new ID
    auto json = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                          "testVocbase");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    ASSERT_TRUE((false == !logicalCollection));
    arangodb::LogicalView::ptr wiew;
    ASSERT_TRUE((arangodb::iresearch::IResearchView::factory()
                     .instantiate(wiew, vocbase, json->slice(), 42)
                     .ok()));
    EXPECT_TRUE((false == !wiew));
    auto* impl = dynamic_cast<arangodb::iresearch::IResearchView*>(wiew.get());
    EXPECT_TRUE((nullptr != impl));

    // ensure we have shard view in vocbase
    struct Link : public arangodb::iresearch::IResearchLink {
      Link(TRI_idx_iid_t id, arangodb::LogicalCollection& col)
          : IResearchLink(id, col) {}
    } link(42, *logicalCollection);
    auto asyncLinkPtr =
        std::make_shared<arangodb::iresearch::IResearchLink::AsyncLinkPtr::element_type>(&link);
    EXPECT_TRUE((true == impl->link(asyncLinkPtr).ok()));

    {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      wiew->properties(builder, arangodb::LogicalDataSource::makeFlags());
      builder.close();
      EXPECT_TRUE((builder.slice().hasKey("name")));
      EXPECT_TRUE((std::string("testView") == builder.slice().get("name").copyString()));
    }

    EXPECT_TRUE((TRI_ERROR_CLUSTER_UNSUPPORTED == wiew->rename("newName").errorNumber()));

    {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      wiew->properties(builder, arangodb::LogicalDataSource::makeFlags());
      builder.close();
      EXPECT_TRUE((builder.slice().hasKey("name")));
      EXPECT_TRUE((std::string("testView") == builder.slice().get("name").copyString()));
    }

    wiew->rename("testView");  // rename back or vocbase will be out of sync
  }
}

TEST_F(IResearchViewDBServerTest, test_toVelocyPack) {
  // base
  {
    auto json = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testView\", \"type\": \"arangosearch\", \"unusedKey\": "
        "\"unusedValue\" }");
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                          "testVocbase");
    arangodb::LogicalView::ptr wiew;
    ASSERT_TRUE((arangodb::iresearch::IResearchView::factory()
                     .instantiate(wiew, vocbase, json->slice(), 42)
                     .ok()));
    EXPECT_TRUE((false == !wiew));
    auto* impl = dynamic_cast<arangodb::iresearch::IResearchView*>(wiew.get());
    EXPECT_TRUE((nullptr != impl));

    arangodb::velocypack::Builder builder;

    builder.openObject();
    wiew->properties(builder, arangodb::LogicalDataSource::makeFlags());
    builder.close();
    auto slice = builder.slice();
    EXPECT_TRUE((4U == slice.length()));
    EXPECT_TRUE((slice.hasKey("globallyUniqueId") &&
                 slice.get("globallyUniqueId").isString() &&
                 false == slice.get("globallyUniqueId").copyString().empty()));
    EXPECT_TRUE((slice.hasKey("id") && slice.get("id").isString() &&
                 std::string("1") == slice.get("id").copyString()));
    EXPECT_TRUE((slice.hasKey("name") && slice.get("name").isString() &&
                 std::string("testView") == slice.get("name").copyString()));
    EXPECT_TRUE((slice.hasKey("type") && slice.get("type").isString() &&
                 arangodb::iresearch::DATA_SOURCE_TYPE.name() ==
                     slice.get("type").copyString()));
  }

  // includeProperties
  {
    auto json = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testView\", \"type\": \"arangosearch\", \"unusedKey\": "
        "\"unusedValue\" }");
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                          "testVocbase");
    arangodb::LogicalView::ptr wiew;
    ASSERT_TRUE((arangodb::iresearch::IResearchView::factory()
                     .instantiate(wiew, vocbase, json->slice(), 42)
                     .ok()));
    EXPECT_TRUE((false == !wiew));
    auto* impl = dynamic_cast<arangodb::iresearch::IResearchView*>(wiew.get());
    EXPECT_TRUE((nullptr != impl));

    arangodb::velocypack::Builder builder;

    builder.openObject();
    wiew->properties(builder, arangodb::LogicalDataSource::makeFlags(
                                  arangodb::LogicalDataSource::Serialize::Detailed));
    builder.close();
    auto slice = builder.slice();
    EXPECT_TRUE((13U == slice.length()));
    EXPECT_TRUE((slice.hasKey("globallyUniqueId") &&
                 slice.get("globallyUniqueId").isString() &&
                 false == slice.get("globallyUniqueId").copyString().empty()));
    EXPECT_TRUE((slice.hasKey("id") && slice.get("id").isString() &&
                 std::string("2") == slice.get("id").copyString()));
    EXPECT_TRUE((slice.hasKey("name") && slice.get("name").isString() &&
                 std::string("testView") == slice.get("name").copyString()));
    EXPECT_TRUE((slice.hasKey("type") && slice.get("type").isString() &&
                 arangodb::iresearch::DATA_SOURCE_TYPE.name() ==
                     slice.get("type").copyString()));
  }

  // includeSystem
  {
    auto json = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testView\", \"type\": \"arangosearch\", \"unusedKey\": "
        "\"unusedValue\" }");
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                          "testVocbase");
    arangodb::LogicalView::ptr wiew;
    ASSERT_TRUE((arangodb::iresearch::IResearchView::factory()
                     .instantiate(wiew, vocbase, json->slice(), 42)
                     .ok()));
    EXPECT_TRUE((false == !wiew));
    auto* impl = dynamic_cast<arangodb::iresearch::IResearchView*>(wiew.get());
    EXPECT_TRUE((nullptr != impl));

    arangodb::velocypack::Builder builder;

    builder.openObject();
    wiew->properties(builder, arangodb::LogicalDataSource::makeFlags(
                                  arangodb::LogicalDataSource::Serialize::ForPersistence));
    builder.close();
    auto slice = builder.slice();
    EXPECT_TRUE((7 == slice.length()));
    EXPECT_TRUE((slice.hasKey("deleted") && slice.get("deleted").isBoolean() &&
                 false == slice.get("deleted").getBoolean()));
    EXPECT_TRUE((slice.hasKey("globallyUniqueId") &&
                 slice.get("globallyUniqueId").isString() &&
                 false == slice.get("globallyUniqueId").copyString().empty()));
    EXPECT_TRUE((slice.hasKey("id") && slice.get("id").isString() &&
                 std::string("3") == slice.get("id").copyString()));
    EXPECT_TRUE((slice.hasKey("isSystem") && slice.get("isSystem").isBoolean() &&
                 false == slice.get("isSystem").getBoolean()));
    EXPECT_TRUE((slice.hasKey("name") && slice.get("name").isString() &&
                 std::string("testView") == slice.get("name").copyString()));
    EXPECT_TRUE((slice.hasKey("planId") && slice.get("planId").isString() &&
                 std::string("3") == slice.get("planId").copyString()));
    EXPECT_TRUE((slice.hasKey("type") && slice.get("type").isString() &&
                 arangodb::iresearch::DATA_SOURCE_TYPE.name() ==
                     slice.get("type").copyString()));
  }
}

TEST_F(IResearchViewDBServerTest, test_transaction_snapshot) {
  auto* ci = arangodb::ClusterInfo::instance();
  ASSERT_TRUE((nullptr != ci));
  TRI_vocbase_t* vocbase;  // will be owned by DatabaseFeature
  createTestDatabase(vocbase);

  static std::vector<std::string> const EMPTY;
  auto viewJson = arangodb::velocypack::Parser::fromJson(
      "{ \"name\": \"testView\", \"type\": \"arangosearch\", "
      "\"consolidationIntervalMsec\": 0 }");
  auto collectionJson = arangodb::velocypack::Parser::fromJson(
      "{ \"name\": \"testCollection\" }");
  auto linkJson = arangodb::velocypack::Parser::fromJson(
      "{ \"view\": \"testView\", \"type\": \"arangosearch\", "
      "\"includeAllFields\": true }");
  auto logicalCollection = vocbase->createCollection(collectionJson->slice());
  ASSERT_TRUE(nullptr != logicalCollection);
  arangodb::LogicalView::ptr logicalWiew;
  ASSERT_TRUE((arangodb::iresearch::IResearchView::factory()
                   .create(logicalWiew, *vocbase, viewJson->slice())
                   .ok()));
  ASSERT_TRUE((false == !logicalWiew));
  auto* wiewImpl = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalWiew.get());
  ASSERT_TRUE((nullptr != wiewImpl));

  bool created;
  auto index = logicalCollection->createIndex(linkJson->slice(), created);
  ASSERT_TRUE((false == !index));
  auto link = std::dynamic_pointer_cast<arangodb::iresearch::IResearchLink>(index);
  ASSERT_TRUE((false == !link));

  // add a single document to view (do not sync)
  {
    auto doc = arangodb::velocypack::Parser::fromJson("{ \"key\": 1 }");
    arangodb::iresearch::IResearchLinkMeta meta;
    meta._includeAllFields = true;
    arangodb::transaction::Methods trx(
        arangodb::transaction::StandaloneContext::Create(*vocbase), EMPTY,
        std::vector<std::string>{logicalCollection->name()}, EMPTY,
        arangodb::transaction::Options());
    EXPECT_TRUE((trx.begin().ok()));
    EXPECT_TRUE((link->insert(trx, arangodb::LocalDocumentId(0), doc->slice(),
                              arangodb::Index::OperationMode::normal)
                     .ok()));
    EXPECT_TRUE((trx.commit().ok()));
  }

  // no snapshot in TransactionState (force == false, waitForSync = false)
  {
    arangodb::transaction::Methods trx(
        arangodb::transaction::StandaloneContext::Create(*vocbase),
        std::vector<std::string>{logicalCollection->name()}, EMPTY, EMPTY,
        arangodb::transaction::Options());
    EXPECT_TRUE((trx.begin().ok()));
    arangodb::HashSet<TRI_voc_cid_t> collections;
    collections.insert(logicalCollection->id());
    auto* snapshot =
        wiewImpl->snapshot(trx, arangodb::iresearch::IResearchView::SnapshotMode::Find,
                           &collections);
    EXPECT_TRUE((nullptr == snapshot));
    EXPECT_TRUE((trx.commit().ok()));
  }

  // no snapshot in TransactionState (force == true, waitForSync = false)
  {
    arangodb::transaction::Methods trx(
        arangodb::transaction::StandaloneContext::Create(*vocbase),
        std::vector<std::string>{logicalCollection->name()}, EMPTY, EMPTY,
        arangodb::transaction::Options());
    EXPECT_TRUE((trx.begin().ok()));
    arangodb::HashSet<TRI_voc_cid_t> collections;
    collections.insert(logicalCollection->id());
    EXPECT_TRUE((nullptr == wiewImpl->snapshot(trx, arangodb::iresearch::IResearchView::SnapshotMode::Find,
                                               &collections)));
    auto* snapshot =
        wiewImpl->snapshot(trx, arangodb::iresearch::IResearchView::SnapshotMode::FindOrCreate,
                           &collections);
    EXPECT_TRUE((snapshot == wiewImpl->snapshot(trx, arangodb::iresearch::IResearchView::SnapshotMode::FindOrCreate,
                                                &collections)));
    EXPECT_TRUE((0 == snapshot->live_docs_count()));
    EXPECT_TRUE((trx.commit().ok()));
  }

  // no snapshot in TransactionState (force == false, waitForSync = true)
  {
    arangodb::transaction::Options opts;
    opts.waitForSync = true;
    arangodb::transaction::Methods trx(
        arangodb::transaction::StandaloneContext::Create(*vocbase),
        std::vector<std::string>{logicalCollection->name()}, EMPTY, EMPTY, opts);
    EXPECT_TRUE((trx.begin().ok()));
    arangodb::HashSet<TRI_voc_cid_t> collections;
    collections.insert(logicalCollection->id());
    auto* snapshot =
        wiewImpl->snapshot(trx, arangodb::iresearch::IResearchView::SnapshotMode::Find,
                           &collections);
    EXPECT_TRUE((nullptr == snapshot));
    EXPECT_TRUE((trx.commit().ok()));
  }

  // no snapshot in TransactionState (force == true, waitForSync = true)
  {
    arangodb::transaction::Options opts;
    arangodb::transaction::Methods trx(
        arangodb::transaction::StandaloneContext::Create(*vocbase),
        std::vector<std::string>{logicalCollection->name()}, EMPTY, EMPTY, opts);
    EXPECT_TRUE((trx.begin().ok()));
    arangodb::HashSet<TRI_voc_cid_t> collections;
    collections.insert(logicalCollection->id());
    EXPECT_TRUE((nullptr == wiewImpl->snapshot(trx, arangodb::iresearch::IResearchView::SnapshotMode::Find,
                                               &collections)));
    auto* snapshot =
        wiewImpl->snapshot(trx, arangodb::iresearch::IResearchView::SnapshotMode::SyncAndReplace,
                           &collections);
    EXPECT_TRUE((snapshot == wiewImpl->snapshot(trx, arangodb::iresearch::IResearchView::SnapshotMode::Find,
                                                &collections)));
    EXPECT_TRUE((snapshot == wiewImpl->snapshot(trx, arangodb::iresearch::IResearchView::SnapshotMode::FindOrCreate,
                                                &collections)));
    EXPECT_TRUE((nullptr != snapshot));
    EXPECT_TRUE((1 == snapshot->live_docs_count()));
    EXPECT_TRUE((trx.commit().ok()));
  }
}

TEST_F(IResearchViewDBServerTest, test_updateProperties) {
  auto* ci = arangodb::ClusterInfo::instance();
  ASSERT_TRUE((nullptr != ci));

  // update empty (partial)
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testCollection\" }");
    auto viewJson = arangodb::velocypack::Parser::fromJson(
        "{ \"id\": \"42\", \"name\": \"testView\", \"type\": \"arangosearch\", "
        "\"collections\": [ 3, 4, 5 ], \"cleanupIntervalStep\": 24, "
        "\"consolidationIntervalMsec\": 42 }");
    TRI_vocbase_t* vocbase;  // will be owned by DatabaseFeature
    createTestDatabase(vocbase, "testDatabase" TOSTRING(__LINE__));
    auto logicalCollection = vocbase->createCollection(collectionJson->slice());
    EXPECT_TRUE((nullptr != logicalCollection));
    EXPECT_TRUE(
        (ci->createViewCoordinator(vocbase->name(), "42", viewJson->slice()).ok()));
    auto wiew = ci->getView(vocbase->name(), "42");  // link creation requires cluster-view to be in ClusterInfo instead of TRI_vocbase_t
    EXPECT_TRUE((false == !wiew));
    auto* impl = dynamic_cast<arangodb::iresearch::IResearchView*>(wiew.get());
    EXPECT_TRUE((nullptr != impl));

    {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      wiew->properties(builder, arangodb::LogicalDataSource::makeFlags(
                                    arangodb::LogicalDataSource::Serialize::Detailed));
      builder.close();

      auto slice = builder.slice();
      EXPECT_TRUE((slice.isObject()));
      EXPECT_TRUE((13U == slice.length()));
      EXPECT_TRUE((slice.hasKey("cleanupIntervalStep") &&
                   slice.get("cleanupIntervalStep").isNumber<size_t>() &&
                   24 == slice.get("cleanupIntervalStep").getNumber<size_t>()));
      EXPECT_TRUE((slice.hasKey("consolidationIntervalMsec") &&
                   slice.get("consolidationIntervalMsec").isNumber<size_t>() &&
                   42 == slice.get("consolidationIntervalMsec").getNumber<size_t>()));
      EXPECT_TRUE((slice.hasKey("links") && slice.get("links").isObject() &&
                   0 == slice.get("links").length()));
    }

    {
      auto update = arangodb::velocypack::Parser::fromJson(
          "{ \"collections\": [ 6, 7, 8, 9 ], \"consolidationIntervalMsec\": "
          "52, \"links\": { \"testCollection\": {} } }");
      EXPECT_TRUE((true == wiew->properties(update->slice(), true).ok()));
    }

    {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      wiew->properties(builder, arangodb::LogicalDataSource::makeFlags(
                                    arangodb::LogicalDataSource::Serialize::Detailed));
      builder.close();

      auto slice = builder.slice();
      EXPECT_TRUE((slice.isObject()));
      EXPECT_TRUE((13U == slice.length()));
      EXPECT_TRUE((slice.hasKey("cleanupIntervalStep") &&
                   slice.get("cleanupIntervalStep").isNumber<size_t>() &&
                   24 == slice.get("cleanupIntervalStep").getNumber<size_t>()));
      EXPECT_TRUE((slice.hasKey("consolidationIntervalMsec") &&
                   slice.get("consolidationIntervalMsec").isNumber<size_t>() &&
                   52 == slice.get("consolidationIntervalMsec").getNumber<size_t>()));
      EXPECT_TRUE((slice.hasKey("links") && slice.get("links").isObject() &&
                   1 == slice.get("links").length()));
    }

    EXPECT_TRUE((false == !arangodb::iresearch::IResearchLinkHelper::find(*logicalCollection,
                                                                          *wiew)));
    static auto visitor = [](TRI_voc_cid_t) -> bool { return false; };
    EXPECT_TRUE((false == wiew->visitCollections(visitor)));  // no collections in view

    // not for persistence
    {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      wiew->properties(builder, arangodb::LogicalDataSource::makeFlags(
                                    arangodb::LogicalDataSource::Serialize::Detailed));
      builder.close();

      auto slice = builder.slice();
      EXPECT_TRUE((slice.isObject()));
      EXPECT_TRUE((13U == slice.length()));
      EXPECT_TRUE((slice.hasKey("cleanupIntervalStep") &&
                   slice.get("cleanupIntervalStep").isNumber<size_t>() &&
                   24 == slice.get("cleanupIntervalStep").getNumber<size_t>()));
      EXPECT_TRUE((slice.hasKey("consolidationIntervalMsec") &&
                   slice.get("consolidationIntervalMsec").isNumber<size_t>() &&
                   52 == slice.get("consolidationIntervalMsec").getNumber<size_t>()));
      EXPECT_TRUE((slice.hasKey("links") && slice.get("links").isObject() &&
                   1 == slice.get("links").length()));
    }

    // for persistence
    {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      wiew->properties(builder, arangodb::LogicalDataSource::makeFlags(
                                    arangodb::LogicalDataSource::Serialize::Detailed,
                                    arangodb::LogicalDataSource::Serialize::ForPersistence));
      builder.close();

      auto slice = builder.slice();
      EXPECT_TRUE((slice.isObject()));
      EXPECT_TRUE((17U == slice.length()));
      EXPECT_TRUE((slice.hasKey("collections") && slice.get("collections").isArray() &&
                   1 == slice.get("collections").length()));
      EXPECT_TRUE((slice.hasKey("cleanupIntervalStep") &&
                   slice.get("cleanupIntervalStep").isNumber<size_t>() &&
                   24 == slice.get("cleanupIntervalStep").getNumber<size_t>()));
      EXPECT_TRUE((slice.hasKey("consolidationIntervalMsec") &&
                   slice.get("consolidationIntervalMsec").isNumber<size_t>() &&
                   52 == slice.get("consolidationIntervalMsec").getNumber<size_t>()));
      EXPECT_TRUE((false == slice.hasKey("links")));
    }
  }

  // update empty (full)
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testCollection\" }");
    auto viewJson = arangodb::velocypack::Parser::fromJson(
        "{ \"id\": \"42\", \"name\": \"testView\", \"type\": \"arangosearch\", "
        "\"collections\": [ 3, 4, 5 ], \"cleanupIntervalStep\": 24, "
        "\"consolidationIntervalMsec\": 42 }");
    TRI_vocbase_t* vocbase;  // will be owned by DatabaseFeature
    createTestDatabase(vocbase, "testDatabase" TOSTRING(__LINE__));
    ASSERT_TRUE((nullptr != vocbase));
    auto logicalCollection = vocbase->createCollection(collectionJson->slice());
    EXPECT_TRUE((nullptr != logicalCollection));
    EXPECT_TRUE(
        (ci->createViewCoordinator(vocbase->name(), "42", viewJson->slice()).ok()));
    auto wiew = ci->getView(vocbase->name(), "42");  // link creation requires cluster-view to be in ClusterInfo instead of TRI_vocbase_t
    EXPECT_TRUE((false == !wiew));
    auto* impl = dynamic_cast<arangodb::iresearch::IResearchView*>(wiew.get());
    EXPECT_TRUE((nullptr != impl));

    {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      wiew->properties(builder, arangodb::LogicalDataSource::makeFlags(
                                    arangodb::LogicalDataSource::Serialize::Detailed));
      builder.close();

      auto slice = builder.slice();
      EXPECT_TRUE((slice.isObject()));
      EXPECT_TRUE((13U == slice.length()));
      EXPECT_TRUE((slice.hasKey("cleanupIntervalStep") &&
                   slice.get("cleanupIntervalStep").isNumber<size_t>() &&
                   24 == slice.get("cleanupIntervalStep").getNumber<size_t>()));
      EXPECT_TRUE((slice.hasKey("consolidationIntervalMsec") &&
                   slice.get("consolidationIntervalMsec").isNumber<size_t>() &&
                   42 == slice.get("consolidationIntervalMsec").getNumber<size_t>()));
      EXPECT_TRUE((slice.hasKey("links") && slice.get("links").isObject() &&
                   0 == slice.get("links").length()));
    }

    {
      auto update = arangodb::velocypack::Parser::fromJson(
          "{ \"collections\": [ 6, 7, 8, 9 ], \"links\": { \"testCollection\": "
          "{} }, \"consolidationIntervalMsec\": 52 }");
      EXPECT_TRUE((true == wiew->properties(update->slice(), false).ok()));
    }

    {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      wiew->properties(builder, arangodb::LogicalDataSource::makeFlags(
                                    arangodb::LogicalDataSource::Serialize::Detailed));
      builder.close();

      auto slice = builder.slice();
      EXPECT_TRUE((slice.isObject()));
      EXPECT_TRUE((13U == slice.length()));
      EXPECT_TRUE((slice.hasKey("cleanupIntervalStep") &&
                   slice.get("cleanupIntervalStep").isNumber<size_t>() &&
                   2 == slice.get("cleanupIntervalStep").getNumber<size_t>()));
      EXPECT_TRUE((slice.hasKey("consolidationIntervalMsec") &&
                   slice.get("consolidationIntervalMsec").isNumber<size_t>() &&
                   52 == slice.get("consolidationIntervalMsec").getNumber<size_t>()));
      EXPECT_TRUE((slice.hasKey("links") && slice.get("links").isObject() &&
                   1 == slice.get("links").length()));
    }

    EXPECT_TRUE((false == !arangodb::iresearch::IResearchLinkHelper::find(*logicalCollection,
                                                                          *wiew)));
    static auto visitor = [](TRI_voc_cid_t) -> bool { return false; };
    EXPECT_TRUE((false == wiew->visitCollections(visitor)));  // no collections in view

    // not for persistence
    {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      wiew->properties(builder, arangodb::LogicalDataSource::makeFlags(
                                    arangodb::LogicalDataSource::Serialize::Detailed));
      builder.close();

      auto slice = builder.slice();
      EXPECT_TRUE((slice.isObject()));
      EXPECT_TRUE((13U == slice.length()));
      EXPECT_TRUE((slice.hasKey("cleanupIntervalStep") &&
                   slice.get("cleanupIntervalStep").isNumber<size_t>() &&
                   2 == slice.get("cleanupIntervalStep").getNumber<size_t>()));
      EXPECT_TRUE((slice.hasKey("consolidationIntervalMsec") &&
                   slice.get("consolidationIntervalMsec").isNumber<size_t>() &&
                   52 == slice.get("consolidationIntervalMsec").getNumber<size_t>()));
      EXPECT_TRUE((slice.hasKey("links") && slice.get("links").isObject() &&
                   1 == slice.get("links").length()));
    }

    // for persistence
    {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      wiew->properties(builder, arangodb::LogicalDataSource::makeFlags(
                                    arangodb::LogicalDataSource::Serialize::Detailed,
                                    arangodb::LogicalDataSource::Serialize::ForPersistence));
      builder.close();

      auto slice = builder.slice();
      EXPECT_TRUE((slice.isObject()));
      EXPECT_TRUE((17U == slice.length()));
      EXPECT_TRUE((slice.hasKey("collections") && slice.get("collections").isArray() &&
                   1 == slice.get("collections").length()));
      EXPECT_TRUE((slice.hasKey("cleanupIntervalStep") &&
                   slice.get("cleanupIntervalStep").isNumber<size_t>() &&
                   2 == slice.get("cleanupIntervalStep").getNumber<size_t>()));
      EXPECT_TRUE((slice.hasKey("consolidationIntervalMsec") &&
                   slice.get("consolidationIntervalMsec").isNumber<size_t>() &&
                   52 == slice.get("consolidationIntervalMsec").getNumber<size_t>()));
      EXPECT_TRUE((false == slice.hasKey("links")));
    }
  }

  // update non-empty (partial)
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testCollection\" }");
    auto linkJson = arangodb::velocypack::Parser::fromJson(
        "{ \"view\": \"testView\", \"type\": \"arangosearch\", "
        "\"includeAllFields\": true }");
    auto viewJson = arangodb::velocypack::Parser::fromJson(
        "{ \"id\": \"42\", \"name\": \"testView\", \"type\": \"arangosearch\", "
        "\"collections\": [ 3, 4, 5 ], \"cleanupIntervalStep\": 24, "
        "\"consolidationIntervalMsec\": 42 }");
    TRI_vocbase_t* vocbase;  // will be owned by DatabaseFeature
    createTestDatabase(vocbase, "testDatabase" TOSTRING(__LINE__));
    ASSERT_TRUE((nullptr != vocbase));
    auto logicalCollection = vocbase->createCollection(collectionJson->slice());
    EXPECT_TRUE((nullptr != logicalCollection));
    EXPECT_TRUE(
        (ci->createViewCoordinator(vocbase->name(), "42", viewJson->slice()).ok()));
    auto wiew = ci->getView(vocbase->name(), "42");  // link creation requires cluster-view to be in ClusterInfo instead of TRI_vocbase_t
    EXPECT_TRUE((false == !wiew));
    auto* impl = dynamic_cast<arangodb::iresearch::IResearchView*>(wiew.get());
    EXPECT_TRUE((nullptr != impl));

    bool created;
    auto index = logicalCollection->createIndex(linkJson->slice(), created);
    ASSERT_TRUE((false == !index));
    auto link = std::dynamic_pointer_cast<arangodb::iresearch::IResearchLink>(index);
    ASSERT_TRUE((false == !link));
    static auto visitor = [](TRI_voc_cid_t) -> bool { return false; };
    EXPECT_TRUE((false == wiew->visitCollections(visitor)));  // 1 collection in view

    {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      wiew->properties(builder, arangodb::LogicalDataSource::makeFlags(
                                    arangodb::LogicalDataSource::Serialize::Detailed));
      builder.close();

      auto slice = builder.slice();
      EXPECT_TRUE((slice.isObject()));
      EXPECT_TRUE((13U == slice.length()));
      EXPECT_TRUE((slice.hasKey("cleanupIntervalStep") &&
                   slice.get("cleanupIntervalStep").isNumber<size_t>() &&
                   24 == slice.get("cleanupIntervalStep").getNumber<size_t>()));
      EXPECT_TRUE((slice.hasKey("consolidationIntervalMsec") &&
                   slice.get("consolidationIntervalMsec").isNumber<size_t>() &&
                   42 == slice.get("consolidationIntervalMsec").getNumber<size_t>()));
      EXPECT_TRUE((slice.hasKey("links") && slice.get("links").isObject() &&
                   1 == slice.get("links").length()));
    }

    {
      auto update = arangodb::velocypack::Parser::fromJson(
          "{ \"collections\": [ 6, 7, 8 ], \"links\": { \"testCollection\": {} "
          "}, \"consolidationIntervalMsec\": 52 }");
      EXPECT_TRUE((true == wiew->properties(update->slice(), true).ok()));
    }

    {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      wiew->properties(builder, arangodb::LogicalDataSource::makeFlags(
                                    arangodb::LogicalDataSource::Serialize::Detailed));
      builder.close();

      auto slice = builder.slice();
      EXPECT_TRUE((slice.isObject()));
      EXPECT_TRUE((13U == slice.length()));
      EXPECT_TRUE((slice.hasKey("cleanupIntervalStep") &&
                   slice.get("cleanupIntervalStep").isNumber<size_t>() &&
                   24 == slice.get("cleanupIntervalStep").getNumber<size_t>()));
      EXPECT_TRUE((slice.hasKey("consolidationIntervalMsec") &&
                   slice.get("consolidationIntervalMsec").isNumber<size_t>() &&
                   52 == slice.get("consolidationIntervalMsec").getNumber<size_t>()));
      EXPECT_TRUE((slice.hasKey("links") && slice.get("links").isObject() &&
                   1 == slice.get("links").length()));
    }

    // not for persistence
    {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      wiew->properties(builder, arangodb::LogicalDataSource::makeFlags(
                                    arangodb::LogicalDataSource::Serialize::Detailed));
      builder.close();

      auto slice = builder.slice();
      EXPECT_TRUE((slice.isObject()));
      EXPECT_TRUE((13U == slice.length()));
      EXPECT_TRUE((slice.hasKey("cleanupIntervalStep") &&
                   slice.get("cleanupIntervalStep").isNumber<size_t>() &&
                   24 == slice.get("cleanupIntervalStep").getNumber<size_t>()));
      EXPECT_TRUE((slice.hasKey("consolidationIntervalMsec") &&
                   slice.get("consolidationIntervalMsec").isNumber<size_t>() &&
                   52 == slice.get("consolidationIntervalMsec").getNumber<size_t>()));
      EXPECT_TRUE((slice.hasKey("links") && slice.get("links").isObject() &&
                   1 == slice.get("links").length()));
    }

    // for persistence
    {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      wiew->properties(builder, arangodb::LogicalDataSource::makeFlags(
                                    arangodb::LogicalDataSource::Serialize::Detailed,
                                    arangodb::LogicalDataSource::Serialize::ForPersistence));
      builder.close();

      auto slice = builder.slice();
      EXPECT_TRUE((slice.isObject()));
      EXPECT_TRUE((17U == slice.length()));
      EXPECT_TRUE((slice.hasKey("collections") && slice.get("collections").isArray() &&
                   1 == slice.get("collections").length()));
      EXPECT_TRUE((slice.hasKey("cleanupIntervalStep") &&
                   slice.get("cleanupIntervalStep").isNumber<size_t>() &&
                   24 == slice.get("cleanupIntervalStep").getNumber<size_t>()));
      EXPECT_TRUE((slice.hasKey("consolidationIntervalMsec") &&
                   slice.get("consolidationIntervalMsec").isNumber<size_t>() &&
                   52 == slice.get("consolidationIntervalMsec").getNumber<size_t>()));
      EXPECT_TRUE((false == slice.hasKey("links")));
    }
  }

  // update non-empty (full)
  {
    auto collection0Json = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testCollection\" }");
    auto collection1Json = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testCollection1\", \"id\": \"123\" }");
    auto linkJson = arangodb::velocypack::Parser::fromJson(
        "{ \"view\": \"testView\", \"type\": \"arangosearch\", "
        "\"includeAllFields\": true }");
    auto viewJson = arangodb::velocypack::Parser::fromJson(
        "{ \"id\": \"42\", \"name\": \"testView\", \"type\": \"arangosearch\", "
        "\"collections\": [ 3, 4, 5 ], \"cleanupIntervalStep\": 24, "
        "\"consolidationIntervalMsec\": 42 }");
    TRI_vocbase_t* vocbase;  // will be owned by DatabaseFeature
    createTestDatabase(vocbase, "testDatabase" TOSTRING(__LINE__));
    ASSERT_TRUE((nullptr != vocbase));
    auto logicalCollection0 = vocbase->createCollection(collection0Json->slice());
    EXPECT_TRUE((nullptr != logicalCollection0));
    auto logicalCollection1 = vocbase->createCollection(collection1Json->slice());
    EXPECT_TRUE((nullptr != logicalCollection1));
    EXPECT_TRUE(
        (ci->createViewCoordinator(vocbase->name(), "42", viewJson->slice()).ok()));
    auto wiew = ci->getView(vocbase->name(), "42");  // link creation requires cluster-view to be in ClusterInfo instead of TRI_vocbase_t
    EXPECT_TRUE((false == !wiew));
    auto* impl = dynamic_cast<arangodb::iresearch::IResearchView*>(wiew.get());
    EXPECT_TRUE((nullptr != impl));

    bool created;
    auto index = logicalCollection1->createIndex(linkJson->slice(), created);
    ASSERT_TRUE((false == !index));
    auto link = std::dynamic_pointer_cast<arangodb::iresearch::IResearchLink>(index);
    ASSERT_TRUE((false == !link));
    static auto visitor = [](TRI_voc_cid_t) -> bool { return false; };
    EXPECT_TRUE((false == wiew->visitCollections(visitor)));  // 1 collection in view

    {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      wiew->properties(builder, arangodb::LogicalDataSource::makeFlags(
                                    arangodb::LogicalDataSource::Serialize::Detailed));
      builder.close();

      auto slice = builder.slice();
      EXPECT_TRUE((slice.isObject()));
      EXPECT_TRUE((13U == slice.length()));
      EXPECT_TRUE((slice.hasKey("cleanupIntervalStep") &&
                   slice.get("cleanupIntervalStep").isNumber<size_t>() &&
                   24 == slice.get("cleanupIntervalStep").getNumber<size_t>()));
      EXPECT_TRUE((slice.hasKey("consolidationIntervalMsec") &&
                   slice.get("consolidationIntervalMsec").isNumber<size_t>() &&
                   42 == slice.get("consolidationIntervalMsec").getNumber<size_t>()));
      EXPECT_TRUE((slice.hasKey("links") && slice.get("links").isObject() &&
                   1 == slice.get("links").length()));
    }

    {
      auto update = arangodb::velocypack::Parser::fromJson(
          "{ \"collections\": [ 6, 7, 8 ], \"links\": { \"testCollection\": {} "
          "}, \"consolidationIntervalMsec\": 52 }");
      EXPECT_TRUE((true == wiew->properties(update->slice(), false).ok()));
    }

    {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      wiew->properties(builder, arangodb::LogicalDataSource::makeFlags(
                                    arangodb::LogicalDataSource::Serialize::Detailed));
      builder.close();

      auto slice = builder.slice();
      EXPECT_TRUE((slice.isObject()));
      EXPECT_TRUE((13U == slice.length()));
      EXPECT_TRUE((slice.hasKey("cleanupIntervalStep") &&
                   slice.get("cleanupIntervalStep").isNumber<size_t>() &&
                   2 == slice.get("cleanupIntervalStep").getNumber<size_t>()));
      EXPECT_TRUE((slice.hasKey("consolidationIntervalMsec") &&
                   slice.get("consolidationIntervalMsec").isNumber<size_t>() &&
                   52 == slice.get("consolidationIntervalMsec").getNumber<size_t>()));
      EXPECT_TRUE((slice.hasKey("links") && slice.get("links").isObject() &&
                   1 == slice.get("links").length()));
    }

    // not for persistence
    {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      wiew->properties(builder, arangodb::LogicalDataSource::makeFlags(
                                    arangodb::LogicalDataSource::Serialize::Detailed));
      builder.close();

      auto slice = builder.slice();
      EXPECT_TRUE((slice.isObject()));
      EXPECT_TRUE((13U == slice.length()));
      EXPECT_TRUE((slice.hasKey("cleanupIntervalStep") &&
                   slice.get("cleanupIntervalStep").isNumber<size_t>() &&
                   2 == slice.get("cleanupIntervalStep").getNumber<size_t>()));
      EXPECT_TRUE((slice.hasKey("consolidationIntervalMsec") &&
                   slice.get("consolidationIntervalMsec").isNumber<size_t>() &&
                   52 == slice.get("consolidationIntervalMsec").getNumber<size_t>()));
      EXPECT_TRUE((slice.hasKey("links") && slice.get("links").isObject() &&
                   1 == slice.get("links").length()));
    }

    // for persistence
    {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      wiew->properties(builder, arangodb::LogicalDataSource::makeFlags(
                                    arangodb::LogicalDataSource::Serialize::Detailed,
                                    arangodb::LogicalDataSource::Serialize::ForPersistence));
      builder.close();

      auto slice = builder.slice();
      EXPECT_TRUE((slice.isObject()));
      EXPECT_TRUE((17U == slice.length()));
      EXPECT_TRUE((slice.hasKey("collections") && slice.get("collections").isArray() &&
                   2 == slice.get("collections").length()));  // list of links is not modified after link drop
      EXPECT_TRUE((slice.hasKey("cleanupIntervalStep") &&
                   slice.get("cleanupIntervalStep").isNumber<size_t>() &&
                   2 == slice.get("cleanupIntervalStep").getNumber<size_t>()));
      EXPECT_TRUE((slice.hasKey("consolidationIntervalMsec") &&
                   slice.get("consolidationIntervalMsec").isNumber<size_t>() &&
                   52 == slice.get("consolidationIntervalMsec").getNumber<size_t>()));
      EXPECT_TRUE((false == slice.hasKey("links")));
    }
  }
}

TEST_F(IResearchViewDBServerTest, test_visitCollections) {
  auto* ci = arangodb::ClusterInfo::instance();
  ASSERT_TRUE(nullptr != ci);

  // visit empty
  {
    auto json = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                          "testVocbase");
    arangodb::LogicalView::ptr wiew;
    ASSERT_TRUE((arangodb::iresearch::IResearchView::factory()
                     .instantiate(wiew, vocbase, json->slice(), 42)
                     .ok()));
    EXPECT_TRUE((false == !wiew));
    auto* impl = dynamic_cast<arangodb::iresearch::IResearchView*>(wiew.get());
    EXPECT_TRUE((nullptr != impl));

    static auto visitor = [](TRI_voc_cid_t) -> bool { return false; };
    EXPECT_TRUE((true == wiew->visitCollections(visitor)));  // no collections in view
  }

  // visit non-empty
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testCollection\" }");
    auto linkJson = arangodb::velocypack::Parser::fromJson(
        "{ \"view\": \"testView\", \"includeAllFields\": true }");
    auto const wiewId = std::to_string(ci->uniqid() + 1);  // +1 because LogicalView creation will generate a new ID
    auto json = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                          "testVocbase");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    ASSERT_TRUE((false == !logicalCollection));
    arangodb::LogicalView::ptr wiew;
    ASSERT_TRUE((arangodb::iresearch::IResearchView::factory()
                     .instantiate(wiew, vocbase, json->slice(), 42)
                     .ok()));
    EXPECT_TRUE((false == !wiew));
    auto* impl = dynamic_cast<arangodb::iresearch::IResearchView*>(wiew.get());
    EXPECT_TRUE((nullptr != impl));

    // ensure we have shard view in vocbase
    struct Link : public arangodb::iresearch::IResearchLink {
      Link(TRI_idx_iid_t id, arangodb::LogicalCollection& col)
          : IResearchLink(id, col) {}
    } link(42, *logicalCollection);
    auto asyncLinkPtr =
        std::make_shared<arangodb::iresearch::IResearchLink::AsyncLinkPtr::element_type>(&link);
    EXPECT_TRUE((true == impl->link(asyncLinkPtr).ok()));

    std::set<TRI_voc_cid_t> cids = {logicalCollection->id()};
    static auto visitor = [&cids](TRI_voc_cid_t cid) -> bool {
      return 1 == cids.erase(cid);
    };
    EXPECT_TRUE((true == wiew->visitCollections(visitor)));  // all collections expected
    EXPECT_TRUE((true == cids.empty()));
    EXPECT_TRUE((true == impl->unlink(logicalCollection->id()).ok()));
    EXPECT_TRUE((true == wiew->visitCollections(visitor)));  // no collections in view
  }
}
