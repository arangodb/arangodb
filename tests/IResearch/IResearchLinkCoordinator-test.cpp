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

#include "../Mocks/StorageEngineMock.h"
#include "../Mocks/Servers.h"
#include "AgencyMock.h"
#include "common.h"

#include "utils/log.hpp"
#include "utils/utf8_path.hpp"

#include "Agency/Store.h"
#include "ApplicationFeatures/CommunicationFeaturePhase.h"
#include "ApplicationFeatures/GreetingsFeaturePhase.h"
#include "Aql/AqlFunctionFeature.h"
#include "Aql/AstNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Function.h"
#include "Aql/SortCondition.h"
#include "Basics/ArangoGlobalContext.h"
#include "Basics/files.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "ClusterEngine/ClusterEngine.h"
#include "FeaturePhases/BasicFeaturePhaseServer.h"
#include "FeaturePhases/ClusterFeaturePhase.h"
#include "FeaturePhases/DatabaseFeaturePhase.h"
#include "FeaturePhases/V8FeaturePhase.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "IResearch/ApplicationServerHelper.h"
#include "IResearch/IResearchCommon.h"
#include "IResearch/IResearchFeature.h"
#include "IResearch/IResearchLink.h"
#include "IResearch/IResearchLinkCoordinator.h"
#include "IResearch/IResearchLinkHelper.h"
#include "IResearch/IResearchViewCoordinator.h"
#include "Logger/LogTopic.h"
#include "Logger/Logger.h"
#include "Utils/OperationOptions.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/KeyGenerator.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ManagedDocumentResult.h"
#include "VocBase/Methods/Collections.h"
#include "VocBase/Methods/Indexes.h"
#include "velocypack/Iterator.h"
#include "velocypack/Parser.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

class IResearchLinkCoordinatorTest : public ::testing::Test {
 protected:
  arangodb::tests::mocks::MockCoordinator server;

  IResearchLinkCoordinatorTest() : server() {
    arangodb::tests::init();
    TransactionStateMock::abortTransactionCount = 0;
    TransactionStateMock::beginTransactionCount = 0;
    TransactionStateMock::commitTransactionCount = 0;
  }

  void createTestDatabase(TRI_vocbase_t*& vocbase) {
    vocbase = server.createDatabase("testDatabase");
    ASSERT_NE(nullptr, vocbase);
    ASSERT_EQ("testDatabase", vocbase->name());
    ASSERT_EQ(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_COORDINATOR, vocbase->type());
  }

  ~IResearchLinkCoordinatorTest() = default;

};

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

TEST_F(IResearchLinkCoordinatorTest, test_create_drop) {
  arangodb::ServerState::instance()->setRebootId(arangodb::RebootId{1}); // Hack.
  auto& ci = server.getFeature<arangodb::ClusterFeature>().clusterInfo();
  TRI_vocbase_t* vocbase;  // will be owned by DatabaseFeature

  createTestDatabase(vocbase);

  // create collection
  std::shared_ptr<arangodb::LogicalCollection> logicalCollection;
  {
    auto const collectionId = "1";

    auto collectionJson = arangodb::velocypack::Parser::fromJson(
      "{ \"id\": \"1\", \"name\": \"testCollection\", \"replicationFactor\":1, \"shards\":{} }");

    EXPECT_TRUE(ci.createCollectionCoordinator(vocbase->name(), collectionId, 0, 1, 1, false,
                                               collectionJson->slice(), 0.0, false, nullptr).ok());

    logicalCollection = ci.getCollection(vocbase->name(), collectionId);
    ASSERT_TRUE((nullptr != logicalCollection));
  }

  // no view specified
  auto& factory =
      server.getFeature<arangodb::iresearch::IResearchFeature>().factory<arangodb::ClusterEngine>();
  {
    auto json = arangodb::velocypack::Parser::fromJson("{}");
    try {
      factory.instantiate(*logicalCollection.get(), json->slice(),
                          arangodb::IndexId::edgeFrom(), true);
      EXPECT_TRUE(false);
    } catch (arangodb::basics::Exception const& ex) {
      EXPECT_EQ(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, ex.code());
    }
  }

  // no view can be found (e.g. db-server coming up with view not available from Agency yet)
  {
    auto json = arangodb::velocypack::Parser::fromJson("{ \"view\": \"42\" }");
    EXPECT_NE(nullptr, factory.instantiate(*logicalCollection.get(), json->slice(),
                                           arangodb::IndexId{1}, true));
  }

  auto const currentCollectionPath = "/Current/Collections/" + vocbase->name() +
                                     "/" +
                                     std::to_string(logicalCollection->id().id());

  // valid link creation
  {
    auto linkJson = arangodb::velocypack::Parser::fromJson(
        "{ \"id\" : \"42\", \"type\": \"arangosearch\", \"view\": \"42\" }");
    auto viewJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testView\", \"id\": \"42\", \"type\": \"arangosearch\" "
        "}");
    arangodb::LogicalView::ptr logicalView;
    ASSERT_TRUE(
        (arangodb::LogicalView::create(logicalView, *vocbase, viewJson->slice()).ok()));

    ASSERT_TRUE(logicalView);
    auto const viewId = std::to_string(logicalView->planId().id());
    EXPECT_TRUE("42" == viewId);

    // simulate heartbeat thread (create index in current)
    {
      auto const value = arangodb::velocypack::Parser::fromJson(
          "{ \"shard-id\": { \"indexes\" : [ { \"id\": \"42\" } ] } }");
      EXPECT_TRUE(arangodb::AgencyComm(server.server())
                  .setValue(currentCollectionPath, value->slice(), 0.0)
                  .successful());
    }

    // unable to create index without timeout
    VPackBuilder outputDefinition;
    EXPECT_TRUE(
      arangodb::methods::Indexes::ensureIndex(
        logicalCollection.get(), linkJson->slice(), true, outputDefinition).ok());

    // get new version from plan
    auto updatedCollection0 =
        ci.getCollection(vocbase->name(), std::to_string(logicalCollection->id().id()));
    ASSERT_TRUE((updatedCollection0));
    auto link = arangodb::iresearch::IResearchLinkHelper::find(*updatedCollection0, *logicalView);
    EXPECT_TRUE(link);

    auto index = std::dynamic_pointer_cast<arangodb::Index>(link);
    ASSERT_TRUE(false == !index);
    EXPECT_TRUE(true == index->canBeDropped());
    EXPECT_TRUE(updatedCollection0.get() == &(index->collection()));
    EXPECT_TRUE(index->fieldNames().empty());
    EXPECT_TRUE(index->fields().empty());
    EXPECT_TRUE(false == index->hasExpansion());
    EXPECT_TRUE(false == index->hasSelectivityEstimate());
    EXPECT_TRUE(false == index->implicitlyUnique());
    EXPECT_TRUE(false == index->isSorted());
    EXPECT_EQ(0, index->memory());
    EXPECT_TRUE(true == index->sparse());
    EXPECT_TRUE(
        (arangodb::Index::IndexType::TRI_IDX_TYPE_IRESEARCH_LINK == index->type()));
    EXPECT_TRUE((arangodb::iresearch::DATA_SOURCE_TYPE.name() == index->typeName()));
    EXPECT_TRUE((false == index->unique()));

    arangodb::iresearch::IResearchLinkMeta actualMeta;
    arangodb::iresearch::IResearchLinkMeta expectedMeta;
    auto builder = index->toVelocyPack(
        arangodb::Index::makeFlags(arangodb::Index::Serialize::Figures));

    std::string error;
    EXPECT_TRUE(actualMeta.init(server.server(), builder->slice(), false, error));
    EXPECT_TRUE(error.empty());
    EXPECT_TRUE(expectedMeta == actualMeta);
    auto const slice = builder->slice();
    EXPECT_TRUE(slice.hasKey("view"));
    EXPECT_TRUE(slice.get("view").isString());
    EXPECT_TRUE(logicalView->id().id() == 42);
    EXPECT_TRUE(logicalView->guid() == slice.get("view").copyString());
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

    // simulate heartbeat thread (drop index from current)
    {
      auto const value = arangodb::velocypack::Parser::fromJson(
          "{ \"shard-id\": { \"indexes\" : [ ] } }");
      EXPECT_TRUE(arangodb::AgencyComm(server.server())
                      .setValue(currentCollectionPath, value->slice(), 0.0)
                      .successful());
    }

    auto const indexArg =
        arangodb::velocypack::Parser::fromJson("{\"id\": \"42\"}");
    EXPECT_TRUE(arangodb::methods::Indexes::drop(logicalCollection.get(),
                                                 indexArg->slice())
                    .ok());

    // get new version from plan
    auto updatedCollection1 =
        ci.getCollection(vocbase->name(), std::to_string(logicalCollection->id().id()));
    ASSERT_TRUE((updatedCollection1));
    EXPECT_TRUE((!arangodb::iresearch::IResearchLinkHelper::find(*updatedCollection1,
                                                                 *logicalView)));

    // drop view
    EXPECT_TRUE(logicalView->drop().ok());
    EXPECT_TRUE(nullptr == ci.getView(vocbase->name(), viewId));

    // old index remains valid
    {
      arangodb::iresearch::IResearchLinkMeta actualMeta;
      arangodb::iresearch::IResearchLinkMeta expectedMeta;
      auto builder = index->toVelocyPack(
          arangodb::Index::makeFlags(arangodb::Index::Serialize::Figures));
      std::string error;

      EXPECT_TRUE((actualMeta.init(server.server(), builder->slice(), false, error) &&
                   expectedMeta == actualMeta));
      auto slice = builder->slice();
      EXPECT_TRUE(error.empty());
      EXPECT_TRUE(slice.hasKey("view") && slice.get("view").isString() &&
                  logicalView->id().id() == 42 &&
                  logicalView->guid() == slice.get("view").copyString());
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

  // ensure jSON is still valid after unload()
  {
    auto linkJson = arangodb::velocypack::Parser::fromJson(
        "{ \"id\":\"42\", \"type\": \"arangosearch\", \"view\": \"42\" }");
    auto viewJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testView\", \"id\": \"42\", \"type\": \"arangosearch\" "
        "}");
    arangodb::LogicalView::ptr logicalView;
    ASSERT_TRUE(
        (arangodb::LogicalView::create(logicalView, *vocbase, viewJson->slice()).ok()));
    ASSERT_TRUE(logicalView);
    auto const viewId = std::to_string(logicalView->planId().id());
    EXPECT_TRUE("42" == viewId);

    // simulate heartbeat thread (create index in current)
    {
      auto const value = arangodb::velocypack::Parser::fromJson(
          "{ \"shard-id\": { \"indexes\" : [ { \"id\": \"42\" } ] } }");
      EXPECT_TRUE(arangodb::AgencyComm(server.server())
                      .setValue(currentCollectionPath, value->slice(), 0.0)
                      .successful());
    }

    // unable to create index without timeout
    VPackBuilder outputDefinition;
    EXPECT_TRUE(arangodb::methods::Indexes::ensureIndex(logicalCollection.get(),
                                                        linkJson->slice(), true, outputDefinition)
                    .ok());

    // get new version from plan
    auto updatedCollection =
        ci.getCollection(vocbase->name(), std::to_string(logicalCollection->id().id()));
    ASSERT_TRUE(updatedCollection);
    auto link = arangodb::iresearch::IResearchLinkHelper::find(*updatedCollection, *logicalView);
    EXPECT_TRUE(link);

    auto index = std::dynamic_pointer_cast<arangodb::Index>(link);
    EXPECT_TRUE(true == index->canBeDropped());
    EXPECT_TRUE(updatedCollection.get() == &(index->collection()));
    EXPECT_TRUE(index->fieldNames().empty());
    EXPECT_TRUE(index->fields().empty());
    EXPECT_TRUE(false == index->hasExpansion());
    EXPECT_TRUE(false == index->hasSelectivityEstimate());
    EXPECT_TRUE(false == index->implicitlyUnique());
    EXPECT_TRUE(false == index->isSorted());
    EXPECT_EQ(0, index->memory());
    EXPECT_TRUE(true == index->sparse());
    EXPECT_TRUE(
        (arangodb::Index::IndexType::TRI_IDX_TYPE_IRESEARCH_LINK == index->type()));
    EXPECT_TRUE((arangodb::iresearch::DATA_SOURCE_TYPE.name() == index->typeName()));
    EXPECT_TRUE((false == index->unique()));

    {
      arangodb::iresearch::IResearchLinkMeta actualMeta;
      arangodb::iresearch::IResearchLinkMeta expectedMeta;
      auto builder = index->toVelocyPack(
          arangodb::Index::makeFlags(arangodb::Index::Serialize::Figures));
      std::string error;

      EXPECT_TRUE((actualMeta.init(server.server(), builder->slice(), false, error) &&
                   expectedMeta == actualMeta));
      auto slice = builder->slice();
      EXPECT_TRUE(slice.hasKey("view") && slice.get("view").isString() &&
                  logicalView->id().id() == 42 &&
                  logicalView->guid() == slice.get("view").copyString());
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

    // ensure jSON is still valid after unload()
    {
      index->unload();
      auto builder = index->toVelocyPack(
          arangodb::Index::makeFlags(arangodb::Index::Serialize::Figures));
      auto slice = builder->slice();
      EXPECT_TRUE(slice.hasKey("view") && slice.get("view").isString() &&
                  logicalView->id().id() == 42 &&
                  logicalView->guid() == slice.get("view").copyString());
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
