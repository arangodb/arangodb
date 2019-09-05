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
#include "Cluster/ClusterComm.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
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

#if USE_ENTERPRISE
#include "Enterprise/Ldap/LdapFeature.h"
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

class IResearchLinkCoordinatorTest : public ::testing::Test {
 protected:
  struct ClusterCommControl : arangodb::ClusterComm {
    static void reset() { arangodb::ClusterComm::_theInstanceInit.store(0); }
  };

  arangodb::application_features::ApplicationServer server;
  arangodb::consensus::Store _agencyStore;
  GeneralClientConnectionAgencyMock* agency;
  StorageEngineMock engine;
  std::unique_ptr<TRI_vocbase_t> system;
  std::map<std::string, std::pair<arangodb::application_features::ApplicationFeature*, bool>> features;
  std::vector<std::reference_wrapper<arangodb::application_features::ApplicationFeature>> orderedFeatures;
  std::string testFilesystemPath;

  IResearchLinkCoordinatorTest()
      : server(nullptr, nullptr), _agencyStore{server, nullptr, "arango"}, engine(server) {
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

    server.addFeature<arangodb::application_features::BasicFeaturePhaseServer>(
        std::make_unique<arangodb::application_features::BasicFeaturePhaseServer>(server));
    features.emplace(server
                         .getFeature<arangodb::application_features::BasicFeaturePhaseServer>()
                         .name(),
                     std::make_pair(&server.getFeature<arangodb::application_features::BasicFeaturePhaseServer>(),
                                    false));

    server.addFeature<arangodb::application_features::CommunicationFeaturePhase>(
        std::make_unique<arangodb::application_features::CommunicationFeaturePhase>(server));
    features.emplace(server
                         .getFeature<arangodb::application_features::CommunicationFeaturePhase>()
                         .name(),
                     std::make_pair(&server.getFeature<arangodb::application_features::CommunicationFeaturePhase>(),
                                    false));

    server.addFeature<arangodb::application_features::ClusterFeaturePhase>(
        std::make_unique<arangodb::application_features::ClusterFeaturePhase>(server));
    features.emplace(
        server.getFeature<arangodb::application_features::ClusterFeaturePhase>().name(),
        std::make_pair(&server.getFeature<arangodb::application_features::ClusterFeaturePhase>(),
                       false));

    server.addFeature<arangodb::application_features::DatabaseFeaturePhase>(
        std::make_unique<arangodb::application_features::DatabaseFeaturePhase>(server));
    features.emplace(server
                         .getFeature<arangodb::application_features::DatabaseFeaturePhase>()
                         .name(),
                     std::make_pair(&server.getFeature<arangodb::application_features::DatabaseFeaturePhase>(),
                                    false));

    server.addFeature<arangodb::application_features::GreetingsFeaturePhase>(
        std::make_unique<arangodb::application_features::GreetingsFeaturePhase>(server, false));
    features.emplace(server
                         .getFeature<arangodb::application_features::GreetingsFeaturePhase>()
                         .name(),
                     std::make_pair(&server.getFeature<arangodb::application_features::GreetingsFeaturePhase>(),
                                    false));

    server.addFeature<arangodb::application_features::V8FeaturePhase>(
        std::make_unique<arangodb::application_features::V8FeaturePhase>(server));
    features.emplace(
        server.getFeature<arangodb::application_features::V8FeaturePhase>().name(),
        std::make_pair(&server.getFeature<arangodb::application_features::V8FeaturePhase>(),
                       false));

    // setup required application features
    server.addFeature<arangodb::V8DealerFeature>(
        std::make_unique<arangodb::V8DealerFeature>(server));
    features.emplace(server.getFeature<arangodb::V8DealerFeature>().name(),
                     std::make_pair(&server.getFeature<arangodb::V8DealerFeature>(), false));

    server.addFeature<arangodb::ViewTypesFeature>(
        std::make_unique<arangodb::ViewTypesFeature>(server));
    features.emplace(server.getFeature<arangodb::ViewTypesFeature>().name(),
                     std::make_pair(&server.getFeature<arangodb::ViewTypesFeature>(), true));

    server.addFeature<arangodb::QueryRegistryFeature>(
        std::make_unique<arangodb::QueryRegistryFeature>(server));
    features.emplace(server.getFeature<arangodb::QueryRegistryFeature>().name(),
                     std::make_pair(&server.getFeature<arangodb::QueryRegistryFeature>(), false));

    system = irs::memory::make_unique<TRI_vocbase_t>(server, TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL,
                                                     0, TRI_VOC_SYSTEM_DATABASE);
    server.addFeature<arangodb::SystemDatabaseFeature>(
        std::make_unique<arangodb::SystemDatabaseFeature>(server, system.get()));
    features.emplace(server.getFeature<arangodb::SystemDatabaseFeature>().name(),
                     std::make_pair(&server.getFeature<arangodb::SystemDatabaseFeature>(),
                                    false));  // required for IResearchAnalyzerFeature

    server.addFeature<arangodb::RandomFeature>(
        std::make_unique<arangodb::RandomFeature>(server));
    features.emplace(server.getFeature<arangodb::RandomFeature>().name(),
                     std::make_pair(&server.getFeature<arangodb::RandomFeature>(),
                                    false));  // required by AuthenticationFeature

    server.addFeature<arangodb::AuthenticationFeature>(
        std::make_unique<arangodb::AuthenticationFeature>(server));
    features.emplace(server.getFeature<arangodb::AuthenticationFeature>().name(),
                     std::make_pair(&server.getFeature<arangodb::AuthenticationFeature>(), false));

    server.addFeature<arangodb::DatabaseFeature>(
        std::make_unique<arangodb::DatabaseFeature>(server));
    features.emplace(server.getFeature<arangodb::DatabaseFeature>().name(),
                     std::make_pair(&server.getFeature<arangodb::DatabaseFeature>(), false));

    server.addFeature<arangodb::DatabasePathFeature>(
        std::make_unique<arangodb::DatabasePathFeature>(server));
    features.emplace(server.getFeature<arangodb::DatabasePathFeature>().name(),
                     std::make_pair(&server.getFeature<arangodb::DatabasePathFeature>(), false));

    server.addFeature<arangodb::TraverserEngineRegistryFeature>(
        std::make_unique<arangodb::TraverserEngineRegistryFeature>(server));
    features.emplace(
        server.getFeature<arangodb::TraverserEngineRegistryFeature>().name(),
        std::make_pair(&server.getFeature<arangodb::TraverserEngineRegistryFeature>(),
                       false));  // must be before AqlFeature

    server.addFeature<arangodb::AqlFeature>(std::make_unique<arangodb::AqlFeature>(server));
    features.emplace(server.getFeature<arangodb::AqlFeature>().name(),
                     std::make_pair(&server.getFeature<arangodb::AqlFeature>(), true));

    server.addFeature<arangodb::aql::AqlFunctionFeature>(
        std::make_unique<arangodb::aql::AqlFunctionFeature>(server));
    features.emplace(server.getFeature<arangodb::aql::AqlFunctionFeature>().name(),
                     std::make_pair(&server.getFeature<arangodb::aql::AqlFunctionFeature>(),
                                    true));  // required for IResearchAnalyzerFeature

    server.addFeature<arangodb::iresearch::IResearchFeature>(
        std::make_unique<arangodb::iresearch::IResearchFeature>(server));
    features.emplace(server.getFeature<arangodb::iresearch::IResearchFeature>().name(),
                     std::make_pair(&server.getFeature<arangodb::iresearch::IResearchFeature>(),
                                    true));

    server.addFeature<arangodb::FlushFeature>(
        std::make_unique<arangodb::FlushFeature>(server));
    features.emplace(server.getFeature<arangodb::FlushFeature>().name(),
                     std::make_pair(&server.getFeature<arangodb::FlushFeature>(), false));  // do not start the thread

    server.addFeature<arangodb::ClusterFeature>(
        std::make_unique<arangodb::ClusterFeature>(server));
    features.emplace(server.getFeature<arangodb::ClusterFeature>().name(),
                     std::make_pair(&server.getFeature<arangodb::ClusterFeature>(), false));

    server.addFeature<arangodb::ShardingFeature>(
        std::make_unique<arangodb::ShardingFeature>(server));
    features.emplace(server.getFeature<arangodb::ShardingFeature>().name(),
                     std::make_pair(&server.getFeature<arangodb::ShardingFeature>(), false));

    server.addFeature<arangodb::iresearch::IResearchAnalyzerFeature>(
        std::make_unique<arangodb::iresearch::IResearchAnalyzerFeature>(server));
    features.emplace(
        server.getFeature<arangodb::iresearch::IResearchAnalyzerFeature>().name(),
        std::make_pair(&server.getFeature<arangodb::iresearch::IResearchAnalyzerFeature>(), true));

#if USE_ENTERPRISE
    server.addFeature<arangodb::LdapFeature>(std::make_unique<arangodb::LdapFeature>(server));
    features.emplace(server.getFeature<arangodb::LdapFeature>().name(),
                     std::make_pair(&server.getFeature<arangodb::LdapFeature>(), false));  // required for AuthenticationFeature with USE_ENTERPRISE
#endif

    server.setupDependencies(false);
    orderedFeatures = server.getOrderedFeatures();

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
      f.get().prepare();

      if (f.get().name() == "Authentication") {
        f.get().forceDisable();
      }
    }

    for (auto& f : orderedFeatures) {
      if (features.at(f.get().name()).second) {
        f.get().start();
      }
    }

    TransactionStateMock::abortTransactionCount = 0;
    TransactionStateMock::beginTransactionCount = 0;
    TransactionStateMock::commitTransactionCount = 0;

    auto& dbPathFeature = server.getFeature<arangodb::DatabasePathFeature>();
    arangodb::tests::setDatabasePath(dbPathFeature);  // ensure test data is stored in a unique directory
    testFilesystemPath = dbPathFeature.directory();

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

    // destroy application features
    for (auto f = orderedFeatures.rbegin(); f != orderedFeatures.rend(); ++f) {
      if (features.at(f->get().name()).second) {
        f->get().stop();
      }
    }

    for (auto f = orderedFeatures.rbegin(); f != orderedFeatures.rend(); ++f) {
      f->get().unprepare();
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
    ASSERT_TRUE(TRI_ERROR_NO_ERROR == database->createDatabase(1, "testDatabase", vocbase));

    ASSERT_TRUE(nullptr != vocbase);
    EXPECT_TRUE("testDatabase" == vocbase->name());
    EXPECT_TRUE(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_COORDINATOR == vocbase->type());
    EXPECT_TRUE(1 == vocbase->id());

    EXPECT_TRUE(arangodb::AgencyComm().setValue(std::string("Current/Databases/") + vocbase->name(), arangodb::velocypack::Slice::emptyObjectSlice(), 0.0).successful());
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
