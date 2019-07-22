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

#include "gtest/gtest.h"

#include "../Mocks/StorageEngineMock.h"
#include "AgencyMock.h"
#include "common.h"

#include "utils/log.hpp"
#include "utils/utf8_path.hpp"

#include "ApplicationFeatures/BasicPhase.h"
#include "ApplicationFeatures/ClusterPhase.h"
#include "ApplicationFeatures/CommunicationPhase.h"
#include "ApplicationFeatures/DatabasePhase.h"
#include "ApplicationFeatures/GreetingsPhase.h"
#include "ApplicationFeatures/V8Phase.h"
#include "Aql/AqlFunctionFeature.h"
#include "Aql/AstNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Function.h"
#include "Aql/SortCondition.h"
#include "Basics/ArangoGlobalContext.h"
#include "Basics/files.h"

#if USE_ENTERPRISE
#include "Enterprise/Ldap/LdapFeature.h"
#endif

#include "Agency/Store.h"
#include "Cluster/ClusterComm.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
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
#include "Random/RandomFeature.h"
#include "RestServer/AqlFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/DatabasePathFeature.h"
#include "RestServer/FlushFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "RestServer/SystemDatabaseFeature.h"
#include "RestServer/TraverserEngineRegistryFeature.h"
#include "RestServer/ViewTypesFeature.h"
#include "Scheduler/SchedulerFeature.h"
#include "Sharding/ShardingFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "Utils/OperationOptions.h"
#include "Utils/SingleCollectionTransaction.h"
#include "V8Server/V8DealerFeature.h"
#include "VocBase/KeyGenerator.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ManagedDocumentResult.h"
#include "VocBase/Methods/Collections.h"
#include "VocBase/Methods/Indexes.h"
#include "velocypack/Iterator.h"
#include "velocypack/Parser.h"

static const VPackBuilder systemDatabaseBuilder = dbArgsBuilder();
static const VPackSlice   systemDatabaseArgs = systemDatabaseBuilder.slice();
// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

class IResearchLinkCoordinatorTest : public ::testing::Test {
 protected:
  struct ClusterCommControl : arangodb::ClusterComm {
    static void reset() { arangodb::ClusterComm::_theInstanceInit.store(0); }
  };

  arangodb::consensus::Store _agencyStore{nullptr, "arango"};
  GeneralClientConnectionAgencyMock* agency;
  StorageEngineMock engine;
  arangodb::application_features::ApplicationServer server;
  std::unique_ptr<TRI_vocbase_t> system;
  std::map<std::string, std::pair<arangodb::application_features::ApplicationFeature*, bool>> features;
  std::vector<arangodb::application_features::ApplicationFeature*> orderedFeatures;
  std::string testFilesystemPath;

  IResearchLinkCoordinatorTest() : engine(server), server(nullptr, nullptr) {
    auto* agencyCommManager = new AgencyCommManagerMock("arango");
    agency = agencyCommManager->addConnection<GeneralClientConnectionAgencyMock>(_agencyStore);
    agency = agencyCommManager->addConnection<GeneralClientConnectionAgencyMock>(
        _agencyStore);  // need 2 connections or Agency callbacks will fail
    arangodb::AgencyCommManager::MANAGER.reset(agencyCommManager);

    arangodb::EngineSelectorFeature::ENGINE = &engine;

    // register factories & normalizers
    auto& indexFactory = const_cast<arangodb::IndexFactory&>(engine.indexFactory());
    indexFactory.emplace(arangodb::iresearch::DATA_SOURCE_TYPE.name(),
                         arangodb::iresearch::IResearchLinkCoordinator::factory());

    arangodb::tests::init();

    // suppress INFO {authentication} Authentication is turned on (system only), authentication for unix sockets is turned on
    // suppress WARNING {authentication} --server.jwt-secret is insecure. Use --server.jwt-secret-keyfile instead
    arangodb::LogTopic::setLogLevel(arangodb::Logger::AUTHENTICATION.name(),
                                    arangodb::LogLevel::ERR);

    // pretend we're on coordinator
    serverRoleBeforeSetup = arangodb::ServerState::instance()->getRole();
    arangodb::ServerState::instance()->setRole(arangodb::ServerState::ROLE_COORDINATOR);

    auto buildFeatureEntry = [&](arangodb::application_features::ApplicationFeature* ftr,
                                 bool start) -> void {
      std::string name = ftr->name();
      features.emplace(name, std::make_pair(ftr, start));
    };
    arangodb::application_features::ApplicationFeature* tmpFeature;

    buildFeatureEntry(new arangodb::application_features::BasicFeaturePhase(server, false), false);
    buildFeatureEntry(new arangodb::application_features::CommunicationFeaturePhase(server),
                      false);
    buildFeatureEntry(new arangodb::application_features::ClusterFeaturePhase(server), false);
    buildFeatureEntry(new arangodb::application_features::DatabaseFeaturePhase(server), false);
    buildFeatureEntry(new arangodb::application_features::GreetingsFeaturePhase(server, false),
                      false);
    buildFeatureEntry(new arangodb::application_features::V8FeaturePhase(server), false);

    // setup required application features
    buildFeatureEntry(new arangodb::V8DealerFeature(server), false);
    buildFeatureEntry(new arangodb::ViewTypesFeature(server), true);
    buildFeatureEntry(tmpFeature = new arangodb::QueryRegistryFeature(server), false);
    arangodb::application_features::ApplicationServer::server->addFeature(tmpFeature);  // need QueryRegistryFeature feature to be added now in order to create the system database
    system = irs::memory::make_unique<TRI_vocbase_t>(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL,
                                                     0, systemDatabaseArgs);
    buildFeatureEntry(new arangodb::SystemDatabaseFeature(server, system.get()),
                      false);  // required for IResearchAnalyzerFeature
    buildFeatureEntry(new arangodb::RandomFeature(server), false);  // required by AuthenticationFeature
    buildFeatureEntry(new arangodb::AuthenticationFeature(server), false);
    buildFeatureEntry(arangodb::DatabaseFeature::DATABASE =
                          new arangodb::DatabaseFeature(server),
                      false);
    buildFeatureEntry(new arangodb::DatabasePathFeature(server), false);
    buildFeatureEntry(new arangodb::TraverserEngineRegistryFeature(server), false);  // must be before AqlFeature
    buildFeatureEntry(new arangodb::AqlFeature(server), true);
    buildFeatureEntry(new arangodb::aql::AqlFunctionFeature(server), true);  // required for IResearchAnalyzerFeature
    buildFeatureEntry(new arangodb::iresearch::IResearchFeature(server), true);
    buildFeatureEntry(new arangodb::FlushFeature(server), false);  // do not start the thread
    buildFeatureEntry(new arangodb::ClusterFeature(server), false);
    buildFeatureEntry(new arangodb::ShardingFeature(server), false);
    buildFeatureEntry(new arangodb::iresearch::IResearchAnalyzerFeature(server), true);

#if USE_ENTERPRISE
    buildFeatureEntry(new arangodb::LdapFeature(server),
                      false);  // required for AuthenticationFeature with USE_ENTERPRISE
#endif

    for (auto& f : features) {
      arangodb::application_features::ApplicationServer::server->addFeature(
          f.second.first);
    }
    arangodb::application_features::ApplicationServer::server->setupDependencies(false);
    orderedFeatures =
        arangodb::application_features::ApplicationServer::server->getOrderedFeatures();

    // suppress log messages since tests check error conditions
    arangodb::LogTopic::setLogLevel(arangodb::Logger::AGENCY.name(),
                                    arangodb::LogLevel::FATAL);
    arangodb::LogTopic::setLogLevel(arangodb::Logger::FIXME.name(), arangodb::LogLevel::ERR);  // suppress ERROR recovery failure due to error from callback
    arangodb::LogTopic::setLogLevel(arangodb::Logger::CLUSTER.name(),
                                    arangodb::LogLevel::FATAL);
    arangodb::LogTopic::setLogLevel(arangodb::iresearch::TOPIC.name(),
                                    arangodb::LogLevel::FATAL);
    irs::logger::output_le(iresearch::logger::IRL_FATAL, stderr);

    for (auto& f : orderedFeatures) {
      f->prepare();

      if (f->name() == "Authentication") {
        f->forceDisable();
      }
    }

    for (auto& f : orderedFeatures) {
      if (features.at(f->name()).second) {
        f->start();
      }
    }

    TransactionStateMock::abortTransactionCount = 0;
    TransactionStateMock::beginTransactionCount = 0;
    TransactionStateMock::commitTransactionCount = 0;

    auto* dbPathFeature =
        arangodb::application_features::ApplicationServer::getFeature<arangodb::DatabasePathFeature>(
            "DatabasePath");
    arangodb::tests::setDatabasePath(*dbPathFeature);  // ensure test data is stored in a unique directory
    testFilesystemPath = dbPathFeature->directory();

    long systemError;
    std::string systemErrorStr;
    TRI_CreateDirectory(testFilesystemPath.c_str(), systemError, systemErrorStr);

    agencyCommManager->start();  // initialize agency
  }

  ~IResearchLinkCoordinatorTest() {
    system.reset();  // destroy before reseting the 'ENGINE'
    TRI_RemoveDirectory(testFilesystemPath.c_str());
    arangodb::LogTopic::setLogLevel(arangodb::iresearch::TOPIC.name(),
                                    arangodb::LogLevel::DEFAULT);
    arangodb::LogTopic::setLogLevel(arangodb::Logger::CLUSTER.name(),
                                    arangodb::LogLevel::DEFAULT);
    arangodb::LogTopic::setLogLevel(arangodb::Logger::FIXME.name(),
                                    arangodb::LogLevel::DEFAULT);
    arangodb::LogTopic::setLogLevel(arangodb::Logger::AGENCY.name(),
                                    arangodb::LogLevel::DEFAULT);
    arangodb::application_features::ApplicationServer::server = nullptr;

    // destroy application features
    for (auto f = orderedFeatures.rbegin(); f != orderedFeatures.rend(); ++f) {
      if (features.at((*f)->name()).second) {
        (*f)->stop();
      }
    }

    for (auto f = orderedFeatures.rbegin(); f != orderedFeatures.rend(); ++f) {
      (*f)->unprepare();
    }

    ClusterCommControl::reset();
    arangodb::ServerState::instance()->setRole(serverRoleBeforeSetup);
    arangodb::LogTopic::setLogLevel(arangodb::Logger::AUTHENTICATION.name(),
                                    arangodb::LogLevel::DEFAULT);
    arangodb::EngineSelectorFeature::ENGINE = nullptr;
  }

  arangodb::ServerState::RoleEnum serverRoleBeforeSetup;
};

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

TEST_F(IResearchLinkCoordinatorTest, test_create_drop) {
  auto* database = arangodb::DatabaseFeature::DATABASE;
  ASSERT_TRUE(nullptr != database);

  auto* ci = arangodb::ClusterInfo::instance();
  ASSERT_TRUE(nullptr != ci);

  TRI_vocbase_t* vocbase;  // will be owned by DatabaseFeature

  // create database
  {
    // simulate heartbeat thread
    ASSERT_TRUE(TRI_ERROR_NO_ERROR == database->createDatabase(1, "testDatabase", arangodb::velocypack::Slice::emptyObjectSlice(), vocbase));

    ASSERT_TRUE(nullptr != vocbase);
    EXPECT_TRUE("testDatabase" == vocbase->name());
    EXPECT_TRUE(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_COORDINATOR == vocbase->type());
    EXPECT_TRUE(1 == vocbase->id());

    EXPECT_TRUE((ci->createDatabaseCoordinator(vocbase->name(), VPackSlice::emptyObjectSlice(), 0.0)
                     .ok()));
  }

  // create collection
  std::shared_ptr<arangodb::LogicalCollection> logicalCollection;
  {
    auto const collectionId = "1";

    auto collectionJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testCollection\", \"replicationFactor\":1, "
        "\"shards\":{} }");

    EXPECT_TRUE((ci->createCollectionCoordinator(vocbase->name(), collectionId, 0, 1,
                                                 1, false, collectionJson->slice(), 0.0)
                     .ok()));

    logicalCollection = ci->getCollection(vocbase->name(), collectionId);
    ASSERT_TRUE((nullptr != logicalCollection));
  }

  ci->loadCurrent();

  // no view specified
  {
    auto json = arangodb::velocypack::Parser::fromJson("{}");
    try {
      arangodb::iresearch::IResearchLinkCoordinator::factory().instantiate(
          *logicalCollection.get(), json->slice(), 1, true);
      EXPECT_TRUE(false);
    } catch (arangodb::basics::Exception const& ex) {
      EXPECT_EQ(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, ex.code());
    }
  }

  // no view can be found (e.g. db-server coming up with view not available from Agency yet)
  {
    auto json = arangodb::velocypack::Parser::fromJson("{ \"view\": \"42\" }");
    EXPECT_NE(nullptr, arangodb::iresearch::IResearchLinkCoordinator::factory().instantiate(
                           *logicalCollection.get(), json->slice(), 1, true));
  }

  auto const currentCollectionPath = "/Current/Collections/" + vocbase->name() +
                                     "/" + std::to_string(logicalCollection->id());

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
    auto const viewId = std::to_string(logicalView->planId());
    EXPECT_TRUE("42" == viewId);

    // simulate heartbeat thread (create index in current)
    {
      auto const value = arangodb::velocypack::Parser::fromJson(
          "{ \"shard-id\": { \"indexes\" : [ { \"id\": \"42\" } ] } }");
      EXPECT_TRUE(arangodb::AgencyComm()
                      .setValue(currentCollectionPath, value->slice(), 0.0)
                      .successful());
    }

    // unable to create index without timeout
    VPackBuilder outputDefinition;
    EXPECT_TRUE(arangodb::methods::Indexes::ensureIndex(logicalCollection.get(),
                                                        linkJson->slice(), true, outputDefinition)
                    .ok());

    // get new version from plan
    auto updatedCollection0 =
        ci->getCollection(vocbase->name(), std::to_string(logicalCollection->id()));
    ASSERT_TRUE((updatedCollection0));
    auto link = arangodb::iresearch::IResearchLinkHelper::find(*updatedCollection0, *logicalView);
    EXPECT_TRUE(link);

    auto index = std::dynamic_pointer_cast<arangodb::Index>(link);
    ASSERT_TRUE((false == !index));
    EXPECT_TRUE((true == index->canBeDropped()));
    EXPECT_TRUE((updatedCollection0.get() == &(index->collection())));
    EXPECT_TRUE((index->fieldNames().empty()));
    EXPECT_TRUE((index->fields().empty()));
    EXPECT_TRUE((false == index->hasExpansion()));
    EXPECT_TRUE((false == index->hasSelectivityEstimate()));
    EXPECT_TRUE((false == index->implicitlyUnique()));
    EXPECT_TRUE((false == index->isSorted()));
    EXPECT_TRUE((0 < index->memory()));
    EXPECT_TRUE((true == index->sparse()));
    EXPECT_TRUE(
        (arangodb::Index::IndexType::TRI_IDX_TYPE_IRESEARCH_LINK == index->type()));
    EXPECT_TRUE((arangodb::iresearch::DATA_SOURCE_TYPE.name() == index->typeName()));
    EXPECT_TRUE((false == index->unique()));

    arangodb::iresearch::IResearchLinkMeta actualMeta;
    arangodb::iresearch::IResearchLinkMeta expectedMeta;
    auto builder = index->toVelocyPack(
        arangodb::Index::makeFlags(arangodb::Index::Serialize::Figures));

    std::string error;
    EXPECT_TRUE(actualMeta.init(builder->slice(), false, error));
    EXPECT_TRUE(error.empty());
    EXPECT_TRUE(expectedMeta == actualMeta);
    auto const slice = builder->slice();
    EXPECT_TRUE(slice.hasKey("view"));
    EXPECT_TRUE(slice.get("view").isString());
    EXPECT_TRUE(logicalView->id() == 42);
    EXPECT_TRUE(logicalView->guid() == slice.get("view").copyString());
    EXPECT_TRUE(slice.hasKey("figures"));
    EXPECT_TRUE(slice.get("figures").isObject());
    EXPECT_TRUE(slice.get("figures").hasKey("memory"));
    EXPECT_TRUE(slice.get("figures").get("memory").isNumber());
    EXPECT_TRUE(0 < slice.get("figures").get("memory").getUInt());

    // simulate heartbeat thread (drop index from current)
    {
      auto const value = arangodb::velocypack::Parser::fromJson(
          "{ \"shard-id\": { \"indexes\" : [ ] } }");
      EXPECT_TRUE(arangodb::AgencyComm()
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
        ci->getCollection(vocbase->name(), std::to_string(logicalCollection->id()));
    ASSERT_TRUE((updatedCollection1));
    EXPECT_TRUE((!arangodb::iresearch::IResearchLinkHelper::find(*updatedCollection1,
                                                                 *logicalView)));

    // drop view
    EXPECT_TRUE(logicalView->drop().ok());
    EXPECT_TRUE(nullptr == ci->getView(vocbase->name(), viewId));

    // old index remains valid
    {
      arangodb::iresearch::IResearchLinkMeta actualMeta;
      arangodb::iresearch::IResearchLinkMeta expectedMeta;
      auto builder = index->toVelocyPack(
          arangodb::Index::makeFlags(arangodb::Index::Serialize::Figures));
      std::string error;

      EXPECT_TRUE((actualMeta.init(builder->slice(), false, error) && expectedMeta == actualMeta));
      auto slice = builder->slice();
      EXPECT_TRUE(error.empty());
      EXPECT_TRUE((slice.hasKey("view") && slice.get("view").isString() &&
                   logicalView->id() == 42 &&
                   logicalView->guid() == slice.get("view").copyString() &&
                   slice.hasKey("figures") && slice.get("figures").isObject() &&
                   slice.get("figures").hasKey("memory") &&
                   slice.get("figures").get("memory").isNumber() &&
                   0 < slice.get("figures").get("memory").getUInt()));
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
    auto const viewId = std::to_string(logicalView->planId());
    EXPECT_TRUE("42" == viewId);

    // simulate heartbeat thread (create index in current)
    {
      auto const value = arangodb::velocypack::Parser::fromJson(
          "{ \"shard-id\": { \"indexes\" : [ { \"id\": \"42\" } ] } }");
      EXPECT_TRUE(arangodb::AgencyComm()
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
        ci->getCollection(vocbase->name(), std::to_string(logicalCollection->id()));
    ASSERT_TRUE(updatedCollection);
    auto link = arangodb::iresearch::IResearchLinkHelper::find(*updatedCollection, *logicalView);
    EXPECT_TRUE(link);

    auto index = std::dynamic_pointer_cast<arangodb::Index>(link);
    EXPECT_TRUE((true == index->canBeDropped()));
    EXPECT_TRUE((updatedCollection.get() == &(index->collection())));
    EXPECT_TRUE((index->fieldNames().empty()));
    EXPECT_TRUE((index->fields().empty()));
    EXPECT_TRUE((false == index->hasExpansion()));
    EXPECT_TRUE((false == index->hasSelectivityEstimate()));
    EXPECT_TRUE((false == index->implicitlyUnique()));
    EXPECT_TRUE((false == index->isSorted()));
    EXPECT_TRUE((0 < index->memory()));
    EXPECT_TRUE((true == index->sparse()));
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

      EXPECT_TRUE((actualMeta.init(builder->slice(), false, error) && expectedMeta == actualMeta));
      auto slice = builder->slice();
      EXPECT_TRUE((slice.hasKey("view") && slice.get("view").isString() &&
                   logicalView->id() == 42 &&
                   logicalView->guid() == slice.get("view").copyString() &&
                   slice.hasKey("figures") && slice.get("figures").isObject() &&
                   slice.get("figures").hasKey("memory") &&
                   slice.get("figures").get("memory").isNumber() &&
                   0 < slice.get("figures").get("memory").getUInt()));
    }

    // ensure jSON is still valid after unload()
    {
      index->unload();
      auto builder = index->toVelocyPack(
          arangodb::Index::makeFlags(arangodb::Index::Serialize::Figures));
      auto slice = builder->slice();
      EXPECT_TRUE((slice.hasKey("view") && slice.get("view").isString() &&
                   logicalView->id() == 42 &&
                   logicalView->guid() == slice.get("view").copyString() &&
                   slice.hasKey("figures") && slice.get("figures").isObject() &&
                   slice.get("figures").hasKey("memory") &&
                   slice.get("figures").get("memory").isNumber() &&
                   0 < slice.get("figures").get("memory").getUInt()));
    }
  }
}
