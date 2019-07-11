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

#include "../Mocks/StorageEngineMock.h"
#include "AgencyMock.h"
#include "common.h"
#include "gtest/gtest.h"

#include "utils/locale_utils.hpp"
#include "utils/log.hpp"
#include "utils/utf8_path.hpp"

#include "Agency/Store.h"
#include "ApplicationFeatures/BasicPhase.h"
#include "ApplicationFeatures/ClusterPhase.h"
#include "ApplicationFeatures/CommunicationPhase.h"
#include "ApplicationFeatures/DatabasePhase.h"
#include "ApplicationFeatures/GreetingsPhase.h"
#include "ApplicationFeatures/V8Phase.h"
#include "Aql/AqlFunctionFeature.h"
#include "Aql/AstNode.h"
#include "Aql/ExecutionBlockImpl.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Function.h"
#include "Aql/IResearchViewNode.h"
#include "Aql/NoResultsExecutor.h"
#include "Aql/OptimizerRulesFeature.h"
#include "Aql/QueryRegistry.h"
#include "Aql/SortCondition.h"
#include "Basics/ArangoGlobalContext.h"
#include "Basics/files.h"
#include "Basics/VelocyPackHelper.h"
#include "Utils/ExecContext.h"
#include "VocBase/Methods/Indexes.h"

#if USE_ENTERPRISE
#include "Enterprise/Ldap/LdapFeature.h"
#endif

#include "Agency/AgencyFeature.h"
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
#include "IResearch/IResearchLinkMeta.h"
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
#include "Sharding/ShardingFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "V8Server/V8DealerFeature.h"
#include "VocBase/KeyGenerator.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ManagedDocumentResult.h"
#include "velocypack/Iterator.h"
#include "velocypack/Parser.h"


// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

class IResearchViewCoordinatorTest : public ::testing::Test {
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

  IResearchViewCoordinatorTest() : engine(server), server(nullptr, nullptr) {
    // need 2 connections or Agency callbacks will fail
    auto* agencyCommManager = new AgencyCommManagerMock("arango");
    agency = agencyCommManager->addConnection<GeneralClientConnectionAgencyMock>(_agencyStore);
    agency = agencyCommManager->addConnection<GeneralClientConnectionAgencyMock>(_agencyStore);
    arangodb::AgencyCommManager::MANAGER.reset(agencyCommManager);

    arangodb::EngineSelectorFeature::ENGINE = &engine;
    // register factories & normalizers
    auto& indexFactory = const_cast<arangodb::IndexFactory&>(engine.indexFactory());
    indexFactory.emplace(arangodb::iresearch::DATA_SOURCE_TYPE.name(),
                         arangodb::iresearch::IResearchLinkCoordinator::factory());

    arangodb::tests::init();

    // suppress INFO {authentication} Authentication is turned on (system only), authentication for unix sockets is turned on
    arangodb::LogTopic::setLogLevel(arangodb::Logger::AUTHENTICATION.name(),
                                    arangodb::LogLevel::WARN);

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
                                                     0, TRI_VOC_SYSTEM_DATABASE);
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
    buildFeatureEntry(new arangodb::aql::OptimizerRulesFeature(server), true);
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
    arangodb::LogTopic::setLogLevel(arangodb::Logger::ENGINES.name(), arangodb::LogLevel::FATAL);  // suppress ERROR {engines} failed to instantiate index, error: ...
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

    auto* authFeature =
        arangodb::application_features::ApplicationServer::getFeature<arangodb::AuthenticationFeature>(
            "Authentication");
    authFeature->enable();  // required for authentication tests

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

  ~IResearchViewCoordinatorTest() {
    system.reset();  // destroy before reseting the 'ENGINE'
    TRI_RemoveDirectory(testFilesystemPath.c_str());
    arangodb::LogTopic::setLogLevel(arangodb::iresearch::TOPIC.name(),
                                    arangodb::LogLevel::DEFAULT);
    arangodb::LogTopic::setLogLevel(arangodb::Logger::CLUSTER.name(),
                                    arangodb::LogLevel::DEFAULT);
    arangodb::LogTopic::setLogLevel(arangodb::Logger::FIXME.name(),
                                    arangodb::LogLevel::DEFAULT);
    arangodb::LogTopic::setLogLevel(arangodb::Logger::ENGINES.name(),
                                    arangodb::LogLevel::DEFAULT);
    arangodb::LogTopic::setLogLevel(arangodb::Logger::AGENCY.name(),
                                    arangodb::LogLevel::DEFAULT);
    arangodb::ClusterInfo::cleanup();  // reset ClusterInfo::instance() before DatabaseFeature::unprepare()
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

TEST_F(IResearchViewCoordinatorTest, test_type) {
  EXPECT_TRUE((arangodb::LogicalDataSource::Type::emplace(arangodb::velocypack::StringRef(
                   "arangosearch")) == arangodb::iresearch::DATA_SOURCE_TYPE));
}

TEST_F(IResearchViewCoordinatorTest, test_rename) {
  auto json = arangodb::velocypack::Parser::fromJson(
      "{ \"name\": \"testView\", \"type\": \"arangosearch\", \"id\": \"1\", "
      "\"collections\": [1,2,3] }");

  Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_COORDINATOR, 1,
                  "testVocbase");
  arangodb::LogicalView::ptr view;
  ASSERT_TRUE(
      (arangodb::LogicalView::instantiate(view, vocbase, json->slice(), 0).ok()));
  EXPECT_TRUE(nullptr != view);
  EXPECT_TRUE(nullptr !=
              std::dynamic_pointer_cast<arangodb::iresearch::IResearchViewCoordinator>(view));
  EXPECT_TRUE(0 == view->planVersion());
  EXPECT_TRUE("testView" == view->name());
  EXPECT_TRUE(false == view->deleted());
  EXPECT_TRUE(1 == view->id());
  EXPECT_TRUE(arangodb::iresearch::DATA_SOURCE_TYPE == view->type());
  EXPECT_TRUE(arangodb::LogicalView::category() == view->category());
  EXPECT_TRUE(&vocbase == &view->vocbase());

  auto const res = view->rename("otherName");
  EXPECT_TRUE(res.fail());
  EXPECT_TRUE(TRI_ERROR_CLUSTER_UNSUPPORTED == res.errorNumber());
}

TEST_F(IResearchViewCoordinatorTest, visit_collections) {
  auto* ci = arangodb::ClusterInfo::instance();
  ASSERT_TRUE((nullptr != ci));
  TRI_vocbase_t* vocbase;  // will be owned by DatabaseFeature

  {
    auto* database = arangodb::DatabaseFeature::DATABASE;
    ASSERT_TRUE((nullptr != database));
    ASSERT_TRUE((TRI_ERROR_NO_ERROR == database->createDatabase(1, "testVocbase", vocbase)));
    ASSERT_TRUE((nullptr != vocbase));
    ASSERT_TRUE((ci->createDatabaseCoordinator(vocbase->name(),
                                               arangodb::velocypack::Slice::emptyObjectSlice(), 0.0)
                     .ok()));
  }

  std::string collectionId0("100");
  std::string collectionId1("101");
  std::string collectionId2("102");
  std::string viewId("1");
  auto collectionJson0 = arangodb::velocypack::Parser::fromJson(
      "{ \"name\": \"testCollection0\", \"shards\":{} }");
  auto collectionJson1 = arangodb::velocypack::Parser::fromJson(
      "{ \"name\": \"testCollection1\", \"shards\":{} }");
  auto collectionJson2 = arangodb::velocypack::Parser::fromJson(
      "{ \"name\": \"testCollection2\", \"shards\":{} }");
  auto linkJson = arangodb::velocypack::Parser::fromJson("{ \"view\": \"1\" }");
  auto json = arangodb::velocypack::Parser::fromJson(
      "{ \"name\": \"testView\", \"type\": \"arangosearch\", \"id\": \"1\" "
      "}");
  ASSERT_TRUE((ci->createCollectionCoordinator(vocbase->name(), collectionId0, 0, 1,
                                               1, false, collectionJson0->slice(), 0.0)
                   .ok()));
  auto logicalCollection0 = ci->getCollection(vocbase->name(), collectionId0);
  ASSERT_TRUE((false == !logicalCollection0));
  ASSERT_TRUE((ci->createCollectionCoordinator(vocbase->name(), collectionId1, 0, 1,
                                               1, false, collectionJson1->slice(), 0.0)
                   .ok()));
  auto logicalCollection1 = ci->getCollection(vocbase->name(), collectionId1);
  ASSERT_TRUE((false == !logicalCollection1));
  ASSERT_TRUE((ci->createCollectionCoordinator(vocbase->name(), collectionId2, 0, 1,
                                               1, false, collectionJson2->slice(), 0.0)
                   .ok()));
  auto logicalCollection2 = ci->getCollection(vocbase->name(), collectionId2);
  ASSERT_TRUE((false == !logicalCollection2));
  ASSERT_TRUE((ci->createViewCoordinator(vocbase->name(), viewId, json->slice()).ok()));
  auto logicalView = ci->getView(vocbase->name(), viewId);
  ASSERT_TRUE((false == !logicalView));
  auto* view =
      dynamic_cast<arangodb::iresearch::IResearchViewCoordinator*>(logicalView.get());

  EXPECT_TRUE(nullptr != view);
  EXPECT_EQ(9, view->planVersion());
  EXPECT_TRUE("testView" == view->name());
  EXPECT_TRUE(false == view->deleted());
  EXPECT_TRUE(1 == view->id());
  EXPECT_TRUE(arangodb::iresearch::DATA_SOURCE_TYPE == view->type());
  EXPECT_TRUE(arangodb::LogicalView::category() == view->category());
  EXPECT_TRUE(vocbase == &view->vocbase());

  std::shared_ptr<arangodb::Index> link;
  EXPECT_NE(nullptr, arangodb::iresearch::IResearchLinkCoordinator::factory().instantiate(
                         *logicalCollection0, linkJson->slice(), 1, false));
  EXPECT_NE(nullptr, arangodb::iresearch::IResearchLinkCoordinator::factory().instantiate(
                         *logicalCollection1, linkJson->slice(), 2, false));
  EXPECT_NE(nullptr, arangodb::iresearch::IResearchLinkCoordinator::factory().instantiate(
                         *logicalCollection2, linkJson->slice(), 3, false));

  // visit view
  TRI_voc_cid_t expectedCollections[] = {1, 2, 3};
  auto* begin = expectedCollections;
  EXPECT_TRUE(true == view->visitCollections([&begin](TRI_voc_cid_t cid) {
    return *begin++ = cid;
  }));
  EXPECT_TRUE(3 == (begin - expectedCollections));
}

TEST_F(IResearchViewCoordinatorTest, test_defaults) {
  auto* database = arangodb::DatabaseFeature::DATABASE;
  ASSERT_TRUE(nullptr != database);
  auto* ci = arangodb::ClusterInfo::instance();
  ASSERT_TRUE((nullptr != ci));
  TRI_vocbase_t* vocbase;  // will be owned by DatabaseFeature

  // create database
  {
    // simulate heartbeat thread
    ASSERT_TRUE((TRI_ERROR_NO_ERROR ==
                 database->createDatabase(1, "testDatabase", vocbase)));

    ASSERT_TRUE((nullptr != vocbase));
    EXPECT_TRUE(("testDatabase" == vocbase->name()));
    EXPECT_TRUE((TRI_vocbase_type_e::TRI_VOCBASE_TYPE_COORDINATOR == vocbase->type()));
    EXPECT_TRUE((1 == vocbase->id()));

    EXPECT_TRUE((ci->createDatabaseCoordinator(vocbase->name(), VPackSlice::emptyObjectSlice(), 0.0)
                     .ok()));
  }

  // view definition with LogicalView (for persistence)
  {
    auto json = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testView\", \"type\": \"arangosearch\", \"id\": \"1\" "
        "}");
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_COORDINATOR, 1,
                          "testVocbase");
    arangodb::LogicalView::ptr view;
    ASSERT_TRUE(
        (arangodb::LogicalView::instantiate(view, vocbase, json->slice(), 0).ok()));
    EXPECT_TRUE((nullptr != view));
    EXPECT_TRUE((nullptr != std::dynamic_pointer_cast<arangodb::iresearch::IResearchViewCoordinator>(
                                view)));
    EXPECT_TRUE((0 == view->planVersion()));
    EXPECT_TRUE(("testView" == view->name()));
    EXPECT_TRUE((false == view->deleted()));
    EXPECT_TRUE((1 == view->id()));
    EXPECT_TRUE((arangodb::iresearch::DATA_SOURCE_TYPE == view->type()));
    EXPECT_TRUE((arangodb::LogicalView::category() == view->category()));
    EXPECT_TRUE((&vocbase == &view->vocbase()));

    // visit default view
    EXPECT_TRUE(
        (true == view->visitCollections([](TRI_voc_cid_t) { return false; })));

    // +system, +properties
    {
      arangodb::iresearch::IResearchViewMeta expectedMeta;
      arangodb::velocypack::Builder builder;
      builder.openObject();
      view->properties(builder, arangodb::LogicalDataSource::makeFlags(
                                    arangodb::LogicalDataSource::Serialize::Detailed,
                                    arangodb::LogicalDataSource::Serialize::ForPersistence));
      builder.close();
      auto slice = builder.slice();
      arangodb::iresearch::IResearchViewMeta meta;
      std::string error;

      EXPECT_TRUE((16U == slice.length()));
      EXPECT_TRUE((slice.hasKey("globallyUniqueId") &&
                   slice.get("globallyUniqueId").isString() &&
                   false == slice.get("globallyUniqueId").copyString().empty()));
      EXPECT_TRUE((slice.get("id").copyString() == "1"));
      EXPECT_TRUE((slice.hasKey("isSystem") && slice.get("isSystem").isBoolean() &&
                   false == slice.get("isSystem").getBoolean()));
      EXPECT_TRUE((slice.get("name").copyString() == "testView"));
      EXPECT_TRUE((slice.get("type").copyString() ==
                   arangodb::iresearch::DATA_SOURCE_TYPE.name()));
      EXPECT_TRUE((slice.hasKey("planId")));
      EXPECT_TRUE((false == slice.get("deleted").getBool()));
      EXPECT_TRUE((!slice.hasKey("links")));  // for persistence so no links
      EXPECT_TRUE((meta.init(slice, error) && expectedMeta == meta));
    }

    // -system, +properties
    {
      arangodb::iresearch::IResearchViewMeta expectedMeta;
      arangodb::velocypack::Builder builder;
      builder.openObject();
      view->properties(builder, arangodb::LogicalDataSource::makeFlags(
                                    arangodb::LogicalDataSource::Serialize::Detailed));
      builder.close();
      auto slice = builder.slice();
      arangodb::iresearch::IResearchViewMeta meta;
      std::string error;

      EXPECT_TRUE((13U == slice.length()));
      EXPECT_TRUE((slice.hasKey("globallyUniqueId") &&
                   slice.get("globallyUniqueId").isString() &&
                   false == slice.get("globallyUniqueId").copyString().empty()));
      EXPECT_TRUE((slice.get("id").copyString() == "1"));
      EXPECT_TRUE((slice.get("name").copyString() == "testView"));
      EXPECT_TRUE((slice.get("type").copyString() ==
                   arangodb::iresearch::DATA_SOURCE_TYPE.name()));
      EXPECT_TRUE((!slice.hasKey("planId")));
      EXPECT_TRUE((!slice.hasKey("deleted")));
      EXPECT_TRUE((slice.hasKey("links") && slice.get("links").isObject() &&
                   0 == slice.get("links").length()));
      EXPECT_TRUE((meta.init(slice, error) && expectedMeta == meta));
    }

    // -system, -properties
    {
      arangodb::velocypack::Builder builder;
      builder.openObject();
      view->properties(builder, arangodb::LogicalDataSource::makeFlags());
      builder.close();
      auto slice = builder.slice();
      arangodb::iresearch::IResearchViewMeta meta;
      std::string error;

      EXPECT_TRUE((4U == slice.length()));
      EXPECT_TRUE((slice.hasKey("globallyUniqueId") &&
                   slice.get("globallyUniqueId").isString() &&
                   false == slice.get("globallyUniqueId").copyString().empty()));
      EXPECT_TRUE((slice.get("id").copyString() == "1"));
      EXPECT_TRUE((slice.get("name").copyString() == "testView"));
      EXPECT_TRUE((slice.get("type").copyString() ==
                   arangodb::iresearch::DATA_SOURCE_TYPE.name()));
      EXPECT_TRUE((!slice.hasKey("planId")));
      EXPECT_TRUE((!slice.hasKey("deleted")));
      EXPECT_TRUE((!slice.hasKey("properties")));
    }

    // +system, -properties
    {
      arangodb::velocypack::Builder builder;
      builder.openObject();
      view->properties(builder, arangodb::LogicalDataSource::makeFlags(
                                    arangodb::LogicalDataSource::Serialize::ForPersistence));
      builder.close();
      auto slice = builder.slice();

      EXPECT_TRUE((7 == slice.length()));
      EXPECT_TRUE((slice.hasKey("globallyUniqueId") &&
                   slice.get("globallyUniqueId").isString() &&
                   false == slice.get("globallyUniqueId").copyString().empty()));
      EXPECT_TRUE((slice.get("id").copyString() == "1"));
      EXPECT_TRUE((slice.hasKey("isSystem") && slice.get("isSystem").isBoolean() &&
                   false == slice.get("isSystem").getBoolean()));
      EXPECT_TRUE((slice.get("name").copyString() == "testView"));
      EXPECT_TRUE((slice.get("type").copyString() ==
                   arangodb::iresearch::DATA_SOURCE_TYPE.name()));
      EXPECT_TRUE((false == slice.get("deleted").getBool()));
      EXPECT_TRUE((slice.hasKey("planId")));
      EXPECT_TRUE((!slice.hasKey("properties")));
    }
  }

  // new view definition with links to missing collections
  {
    auto viewCreateJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testView\", \"id\": \"42\", \"type\": "
        "\"arangosearch\", \"links\": { \"testCollection\": {} } }");
    auto viewId = "testView";

    arangodb::LogicalView::ptr logicalView;
    auto res = arangodb::iresearch::IResearchViewCoordinator::factory().create(
        logicalView, *vocbase, viewCreateJson->slice());
    EXPECT_TRUE((TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND == res.errorNumber()));
    logicalView = ci->getView(vocbase->name(), viewId);
    ASSERT_TRUE((true == !logicalView));
  }

  // new view definition with links with invalid definition
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson(
        "{ \"id\": \"1\", \"planId\": \"1\", \"name\": \"testCollection\", "
        "\"replicationFactor\": 1, \"type\": 1, \"shards\":{} }");
    auto viewCreateJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testView\", \"id\": \"42\", \"type\": "
        "\"arangosearch\", \"links\": { \"testCollection\": 42 } }");
    auto collectionId = std::to_string(1);
    auto viewId = "testView";

    EXPECT_TRUE((ci->createCollectionCoordinator(vocbase->name(), collectionId, 0, 1,
                                                 1, false, collectionJson->slice(), 0.0)
                     .ok()));
    auto logicalCollection = ci->getCollection(vocbase->name(), collectionId);
    ASSERT_TRUE((false == !logicalCollection));
    auto dropLogicalCollection = std::shared_ptr<arangodb::ClusterInfo>(
        ci, [vocbase, &collectionId](arangodb::ClusterInfo* ci) -> void {
          ci->dropCollectionCoordinator(vocbase->name(), collectionId, 0);
        });
    arangodb::LogicalView::ptr logicalView;
    auto res = arangodb::iresearch::IResearchViewCoordinator::factory().create(
        logicalView, *vocbase, viewCreateJson->slice());
    EXPECT_TRUE((TRI_ERROR_BAD_PARAMETER == res.errorNumber()));

    logicalCollection = ci->getCollection(vocbase->name(), collectionId);
    ASSERT_TRUE((false == !logicalCollection));
    logicalView = ci->getView(vocbase->name(), viewId);
    ASSERT_TRUE((true == !logicalView));
    EXPECT_TRUE((true == logicalCollection->getIndexes().empty()));
  }

  // new view definition with links (collection not authorized)
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson(
        "{ \"id\": \"1\", \"planId\": \"1\", \"name\": \"testCollection\", "
        "\"replicationFactor\": 1, \"type\": 1, \"shards\":{} }");
    auto viewCreateJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testView\", \"id\": \"42\", \"type\": "
        "\"arangosearch\", \"links\": { \"testCollection\": {} } }");
    auto collectionId = std::to_string(1);
    auto viewId = "testView";

    EXPECT_TRUE((ci->createCollectionCoordinator(vocbase->name(), collectionId, 0, 1,
                                                 1, false, collectionJson->slice(), 0.0)
                     .ok()));
    auto logicalCollection = ci->getCollection(vocbase->name(), collectionId);
    ASSERT_TRUE((false == !logicalCollection));
    auto dropLogicalCollection = std::shared_ptr<arangodb::ClusterInfo>(
        ci, [vocbase, &collectionId](arangodb::ClusterInfo* ci) -> void {
          ci->dropCollectionCoordinator(vocbase->name(), collectionId, 0);
        });

    struct ExecContext : public arangodb::ExecContext {
      ExecContext()
          : arangodb::ExecContext(arangodb::ExecContext::Type::Default, "", "",
                                  arangodb::auth::Level::NONE,
                                  arangodb::auth::Level::NONE) {}
    } execContext;
    arangodb::ExecContextScope execContextScope(&execContext);
    auto* authFeature = arangodb::AuthenticationFeature::instance();
    auto* userManager = authFeature->userManager();
    arangodb::aql::QueryRegistry queryRegistry(0);  // required for UserManager::loadFromDB()
    userManager->setQueryRegistry(&queryRegistry);
    auto resetUserManager = std::shared_ptr<arangodb::auth::UserManager>(
        userManager,
        [](arangodb::auth::UserManager* ptr) -> void { ptr->removeAllUsers(); });

    arangodb::LogicalView::ptr logicalView;
    auto res = arangodb::iresearch::IResearchViewCoordinator::factory().create(
        logicalView, *vocbase, viewCreateJson->slice());
    EXPECT_TRUE((TRI_ERROR_FORBIDDEN == res.errorNumber()));

    logicalCollection = ci->getCollection(vocbase->name(), collectionId);
    ASSERT_TRUE((false == !logicalCollection));
    logicalView = ci->getView(vocbase->name(), viewId);
    ASSERT_TRUE((true == !logicalView));
    EXPECT_TRUE((true == logicalCollection->getIndexes().empty()));
  }

  // new view definition with links
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson(
        "{ \"id\": \"1\", \"planId\": \"1\", \"name\": \"testCollection\", "
        "\"replicationFactor\": 1, \"type\": 1, \"shards\":{} }");
    auto viewCreateJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testView\", \"id\": \"42\", \"type\": "
        "\"arangosearch\", \"links\": { \"testCollection\": {} } }");
    auto collectionId = std::to_string(1);
    auto viewId = "testView";

    EXPECT_TRUE((ci->createCollectionCoordinator(vocbase->name(), collectionId, 0, 1,
                                                 1, false, collectionJson->slice(), 0.0)
                     .ok()));
    auto logicalCollection = ci->getCollection(vocbase->name(), collectionId);
    ASSERT_TRUE((false == !logicalCollection));
    auto dropLogicalCollection = std::shared_ptr<arangodb::ClusterInfo>(
        ci, [vocbase, &collectionId](arangodb::ClusterInfo* ci) -> void {
          ci->dropCollectionCoordinator(vocbase->name(), collectionId, 0);
        });

    // simulate heartbeat thread (create index in current)
    {
      auto const path = "/Current/Collections/" + vocbase->name() + "/" +
                        std::to_string(logicalCollection->id());
      auto const value = arangodb::velocypack::Parser::fromJson(
          "{ \"shard-id-does-not-matter\": { \"indexes\" : [ { \"id\": \"1\" "
          "} ] } }");
      EXPECT_TRUE((arangodb::AgencyComm().setValue(path, value->slice(), 0.0).successful()));
    }

    arangodb::LogicalView::ptr logicalView;
    EXPECT_TRUE((arangodb::iresearch::IResearchViewCoordinator::factory()
                     .create(logicalView, *vocbase, viewCreateJson->slice())
                     .ok()));

    logicalCollection = ci->getCollection(vocbase->name(), collectionId);
    ASSERT_TRUE((false == !logicalCollection));
    logicalView = ci->getView(vocbase->name(), viewId);
    ASSERT_TRUE((false == !logicalView));
    EXPECT_TRUE((false == logicalCollection->getIndexes().empty()));
    EXPECT_TRUE((false == logicalView->visitCollections(
                              [](TRI_voc_cid_t) -> bool { return false; })));
  }
}

TEST_F(IResearchViewCoordinatorTest, test_create_drop_view) {
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

    EXPECT_TRUE((ci->createDatabaseCoordinator(vocbase->name(), VPackSlice::emptyObjectSlice(), 0.0)
                     .ok()));
  }

  // no name specified
  {
    auto json = arangodb::velocypack::Parser::fromJson(
        "{ \"type\": \"arangosearch\" }");
    auto viewId = std::to_string(ci->uniqid());
    EXPECT_TRUE(
        (TRI_ERROR_BAD_PARAMETER ==
         ci->createViewCoordinator(vocbase->name(), viewId, json->slice()).errorNumber()));
  }

  // empty name
  {
    auto json = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"\", \"type\": \"arangosearch\" }");
    auto viewId = std::to_string(ci->uniqid());
    EXPECT_TRUE(
        (TRI_ERROR_BAD_PARAMETER ==
         ci->createViewCoordinator(vocbase->name(), viewId, json->slice()).errorNumber()));
  }

  // wrong name
  {
    auto json = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": 5, \"type\": \"arangosearch\" }");
    auto viewId = std::to_string(ci->uniqid());
    EXPECT_TRUE(
        (TRI_ERROR_BAD_PARAMETER ==
         ci->createViewCoordinator(vocbase->name(), viewId, json->slice()).errorNumber()));
  }

  // no type specified
  {
    auto json =
        arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\" }");
    auto viewId = std::to_string(ci->uniqid());
    EXPECT_TRUE(
        (TRI_ERROR_BAD_PARAMETER ==
         ci->createViewCoordinator(vocbase->name(), viewId, json->slice()).errorNumber()));
  }

  // create and drop view (no id specified)
  {
    auto json = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
    auto viewId = std::to_string(ci->uniqid() + 1);  // +1 because LogicalView creation will generate a new ID

    EXPECT_TRUE(
        (ci->createViewCoordinator(vocbase->name(), viewId, json->slice()).ok()));

    // get current plan version
    auto planVersion = arangodb::tests::getCurrentPlanVersion();

    auto view = ci->getView(vocbase->name(), viewId);
    EXPECT_TRUE(nullptr != view);
    EXPECT_TRUE(nullptr !=
                std::dynamic_pointer_cast<arangodb::iresearch::IResearchViewCoordinator>(view));
    EXPECT_TRUE(planVersion == view->planVersion());
    EXPECT_TRUE("testView" == view->name());
    EXPECT_TRUE(false == view->deleted());
    EXPECT_TRUE(viewId == std::to_string(view->id()));
    EXPECT_TRUE(arangodb::iresearch::DATA_SOURCE_TYPE == view->type());
    EXPECT_TRUE(arangodb::LogicalView::category() == view->category());
    EXPECT_TRUE(vocbase == &view->vocbase());

    // create duplicate view
    EXPECT_TRUE(
        (TRI_ERROR_ARANGO_DUPLICATE_NAME ==
         ci->createViewCoordinator(vocbase->name(), viewId, json->slice()).errorNumber()));
    EXPECT_TRUE(planVersion == arangodb::tests::getCurrentPlanVersion());
    EXPECT_TRUE(view == ci->getView(vocbase->name(), view->name()));

    // drop view
    EXPECT_TRUE(view->drop().ok());
    EXPECT_TRUE(planVersion < arangodb::tests::getCurrentPlanVersion());

    // check there is no more view
    EXPECT_TRUE(nullptr == ci->getView(vocbase->name(), view->name()));

    // drop already dropped view
    EXPECT_TRUE((view->drop().ok()));
  }

  // create and drop view
  {
    auto json = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testView\", \"id\": \"42\", \"type\": "
        "\"arangosearch\" }");
    auto viewId = std::to_string(42);

    EXPECT_TRUE(
        (ci->createViewCoordinator(vocbase->name(), viewId, json->slice()).ok()));
    EXPECT_TRUE("42" == viewId);

    // get current plan version
    auto planVersion = arangodb::tests::getCurrentPlanVersion();

    auto view = ci->getView(vocbase->name(), viewId);
    EXPECT_TRUE(nullptr != view);
    EXPECT_TRUE(nullptr !=
                std::dynamic_pointer_cast<arangodb::iresearch::IResearchViewCoordinator>(view));
    EXPECT_TRUE(planVersion == view->planVersion());
    EXPECT_TRUE("testView" == view->name());
    EXPECT_TRUE(false == view->deleted());
    EXPECT_TRUE(42 == view->id());
    EXPECT_TRUE(arangodb::iresearch::DATA_SOURCE_TYPE == view->type());
    EXPECT_TRUE(arangodb::LogicalView::category() == view->category());
    EXPECT_TRUE(vocbase == &view->vocbase());

    // create duplicate view
    EXPECT_TRUE(
        (TRI_ERROR_ARANGO_DUPLICATE_NAME ==
         ci->createViewCoordinator(vocbase->name(), viewId, json->slice()).errorNumber()));
    EXPECT_TRUE(planVersion == arangodb::tests::getCurrentPlanVersion());
    EXPECT_TRUE(view == ci->getView(vocbase->name(), view->name()));

    // drop view
    EXPECT_TRUE(view->drop().ok());
    EXPECT_TRUE(planVersion < arangodb::tests::getCurrentPlanVersion());

    // check there is no more view
    EXPECT_TRUE(nullptr == ci->getView(vocbase->name(), view->name()));

    // drop already dropped view
    EXPECT_TRUE((view->drop().ok()));
  }
}

TEST_F(IResearchViewCoordinatorTest, test_create_link_in_background) {
  auto* database = arangodb::DatabaseFeature::DATABASE;
  ASSERT_NE(nullptr, database);
  auto* ci = arangodb::ClusterInfo::instance();
  ASSERT_NE(nullptr, ci);
  TRI_vocbase_t* vocbase;  // will be owned by DatabaseFeature

  // create database
  {
    // simulate heartbeat thread
    ASSERT_EQ(TRI_ERROR_NO_ERROR,
      database->createDatabase(1, "testDatabase", vocbase));

    ASSERT_NE(nullptr, vocbase);
    ASSERT_EQ("testDatabase", vocbase->name());
    ASSERT_EQ(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_COORDINATOR, vocbase->type());
    ASSERT_EQ(1, vocbase->id());

    ASSERT_TRUE((ci->createDatabaseCoordinator(vocbase->name(), VPackSlice::emptyObjectSlice(), 0.0)
      .ok()));
  }

  auto collectionJson = arangodb::velocypack::Parser::fromJson(
    "{ \"id\": \"1\", \"planId\": \"1\",  \"name\": \"testCollection\", "
    "\"replicationFactor\": 1, \"type\": 1, \"shards\":{} }");
  auto viewCreateJson = arangodb::velocypack::Parser::fromJson(
    "{ \"name\": \"testView\", \"id\": \"42\", \"type\": \"arangosearch\" "
    "}");
  auto viewUpdateJson = arangodb::velocypack::Parser::fromJson(
    "{ \"links\": { \"testCollection\": { \"includeAllFields\":true, \"inBackground\":true } } }");
  auto collectionId = std::to_string(1);
  auto viewId = std::to_string(42);

  ASSERT_TRUE((ci->createCollectionCoordinator(vocbase->name(), collectionId, 0, 1,
    false, collectionJson->slice(), 0.0)
    .ok()));
  auto logicalCollection = ci->getCollection(vocbase->name(), collectionId);
  ASSERT_NE(nullptr, logicalCollection);
  ASSERT_TRUE((ci->createViewCoordinator(vocbase->name(), viewId, viewCreateJson->slice())
    .ok()));
  auto logicalView = ci->getView(vocbase->name(), viewId);
  ASSERT_NE(nullptr, logicalView);

  ASSERT_TRUE(logicalCollection->getIndexes().empty());
  ASSERT_NE(nullptr, ci->getView(vocbase->name(), viewId));

  //  link creation
  {
    // simulate heartbeat thread (create index in current)
    {
      auto const path = "/Current/Collections/" + vocbase->name() + "/" +
        std::to_string(logicalCollection->id());
      auto const value = arangodb::velocypack::Parser::fromJson(
        "{ \"shard-id-does-not-matter\": { \"indexes\" : [ { \"id\": \"1\" "
        "} ] } }");
      EXPECT_TRUE(arangodb::AgencyComm().setValue(path, value->slice(), 0.0).successful());
    }
    ASSERT_TRUE((logicalView->properties(viewUpdateJson->slice(), true).ok()));
  }
  // check agency record
  {
    VPackBuilder agencyRecord;
    agencyRecord.openArray();
    _agencyStore.read(
      arangodb::velocypack::Parser::fromJson(
        "[\"arango/Plan/Collections/testDatabase/" + collectionId + "\"]")->slice(),
      agencyRecord);
    agencyRecord.close();

    ASSERT_TRUE(agencyRecord.slice().isArray());
    auto collectionInfoSlice = agencyRecord.slice().at(0);
    auto indexesSlice = collectionInfoSlice.get("arango").
      get("Plan").get("Collections").
      get("testDatabase").get(collectionId).
      get("indexes");
    ASSERT_TRUE(indexesSlice.isArray());
    auto linkSlice = indexesSlice.at(0);
    ASSERT_TRUE(linkSlice.hasKey("inBackground"));
    ASSERT_TRUE(linkSlice.get("inBackground").isBool());
    ASSERT_TRUE(linkSlice.get("inBackground").getBool());
  }
  // check index definition in collection
  {
    logicalCollection = ci->getCollection(vocbase->name(), collectionId);
    ASSERT_NE(nullptr, logicalCollection);
    auto indexes = logicalCollection->getIndexes();
    ASSERT_EQ(1, indexes.size()); // arangosearch should be there
    auto index = indexes[0];
    ASSERT_EQ(arangodb::Index::TRI_IDX_TYPE_IRESEARCH_LINK,  index->type());
    VPackBuilder builder;
    index->toVelocyPack(builder,
                        arangodb::Index::makeFlags(arangodb::Index::Serialize::Internals));
    // temporary property should not be returned
    ASSERT_FALSE(builder.slice().hasKey("inBackground")); 
  }
    
  // Check link definition in view
  {
    logicalView = ci->getView(vocbase->name(), viewId);
    ASSERT_NE(nullptr, logicalView);
    VPackBuilder builder;
    builder.openObject();
    logicalView->properties(builder, arangodb::LogicalDataSource::makeFlags(
      arangodb::LogicalDataSource::Serialize::Detailed));
    builder.close();
    ASSERT_TRUE(builder.slice().hasKey("links"));
    auto links = builder.slice().get("links");
    ASSERT_TRUE(links.isObject());
    ASSERT_TRUE(links.hasKey("testCollection"));
    auto testCollectionSlice = links.get("testCollection");
    // temporary property should not be returned
    ASSERT_FALSE(testCollectionSlice.hasKey("inBackground")); 
  }
}  

TEST_F(IResearchViewCoordinatorTest, test_drop_with_link) {
  auto* database = arangodb::DatabaseFeature::DATABASE;
  ASSERT_TRUE(nullptr != database);
  auto* ci = arangodb::ClusterInfo::instance();
  ASSERT_TRUE((nullptr != ci));
  TRI_vocbase_t* vocbase;  // will be owned by DatabaseFeature

  // create database
  {
    // simulate heartbeat thread
    ASSERT_TRUE((TRI_ERROR_NO_ERROR ==
                 database->createDatabase(1, "testDatabase", vocbase)));

    ASSERT_TRUE((nullptr != vocbase));
    EXPECT_TRUE(("testDatabase" == vocbase->name()));
    EXPECT_TRUE((TRI_vocbase_type_e::TRI_VOCBASE_TYPE_COORDINATOR == vocbase->type()));
    EXPECT_TRUE((1 == vocbase->id()));

    EXPECT_TRUE((ci->createDatabaseCoordinator(vocbase->name(), VPackSlice::emptyObjectSlice(), 0.0)
                     .ok()));
  }

  auto collectionJson = arangodb::velocypack::Parser::fromJson(
      "{ \"id\": \"1\", \"planId\": \"1\",  \"name\": \"testCollection\", "
      "\"replicationFactor\": 1, \"type\": 1, \"shards\":{} }");
  auto viewCreateJson = arangodb::velocypack::Parser::fromJson(
      "{ \"name\": \"testView\", \"id\": \"42\", \"type\": \"arangosearch\" "
      "}");
  auto viewUpdateJson = arangodb::velocypack::Parser::fromJson(
      "{ \"links\": { \"testCollection\": {} } }");
  auto collectionId = std::to_string(1);
  auto viewId = std::to_string(42);

  EXPECT_TRUE((ci->createCollectionCoordinator(vocbase->name(), collectionId, 0, 1,
                                               1, false, collectionJson->slice(), 0.0)
                   .ok()));
  auto logicalCollection = ci->getCollection(vocbase->name(), collectionId);
  ASSERT_TRUE((false == !logicalCollection));
  EXPECT_TRUE((ci->createViewCoordinator(vocbase->name(), viewId, viewCreateJson->slice())
                   .ok()));
  auto logicalView = ci->getView(vocbase->name(), viewId);
  ASSERT_TRUE((false == !logicalView));

  EXPECT_TRUE((true == logicalCollection->getIndexes().empty()));
  EXPECT_TRUE((false == !ci->getView(vocbase->name(), viewId)));

  // initial link creation
  {
    // simulate heartbeat thread (create index in current)
    {
      auto const path = "/Current/Collections/" + vocbase->name() + "/" +
                        std::to_string(logicalCollection->id());
      auto const value = arangodb::velocypack::Parser::fromJson(
          "{ \"shard-id-does-not-matter\": { \"indexes\" : [ { \"id\": \"1\" "
          "} ] } }");
      EXPECT_TRUE(arangodb::AgencyComm().setValue(path, value->slice(), 0.0).successful());
    }

    EXPECT_TRUE((logicalView->properties(viewUpdateJson->slice(), true).ok()));
    logicalCollection = ci->getCollection(vocbase->name(), collectionId);
    ASSERT_TRUE((false == !logicalCollection));
    logicalView = ci->getView(vocbase->name(), viewId);
    ASSERT_TRUE((false == !logicalView));
    EXPECT_TRUE((false == logicalCollection->getIndexes().empty()));
    EXPECT_TRUE((false == logicalView->visitCollections(
                              [](TRI_voc_cid_t) -> bool { return false; })));

    // simulate heartbeat thread (remove index from current)
    {
      auto const path = "/Current/Collections/" + vocbase->name() + "/" +
                        std::to_string(logicalCollection->id()) +
                        "/shard-id-does-not-matter/indexes";
      EXPECT_TRUE(arangodb::AgencyComm().removeValues(path, false).successful());
    }
  }

  {
    struct ExecContext : public arangodb::ExecContext {
      ExecContext()
          : arangodb::ExecContext(arangodb::ExecContext::Type::Default, "", "",
                                  arangodb::auth::Level::NONE,
                                  arangodb::auth::Level::NONE) {}
    } execContext;
    arangodb::ExecContextScope execContextScope(&execContext);
    auto* authFeature = arangodb::AuthenticationFeature::instance();
    auto* userManager = authFeature->userManager();
    arangodb::aql::QueryRegistry queryRegistry(0);  // required for UserManager::loadFromDB()
    userManager->setQueryRegistry(&queryRegistry);
    auto resetUserManager = std::shared_ptr<arangodb::auth::UserManager>(
        userManager,
        [](arangodb::auth::UserManager* ptr) -> void { ptr->removeAllUsers(); });

    // not authorised (NONE collection) as per https://github.com/arangodb/backlog/issues/459
    {
      arangodb::auth::UserMap userMap;
      auto& user =
          userMap
              .emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP))
              .first->second;
      user.grantCollection(vocbase->name(), "testCollection", arangodb::auth::Level::NONE);  // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database

      EXPECT_TRUE((TRI_ERROR_FORBIDDEN == logicalView->drop().errorNumber()));
      logicalCollection = ci->getCollection(vocbase->name(), collectionId);
      ASSERT_TRUE((false == !logicalCollection));
      EXPECT_TRUE((false == logicalCollection->getIndexes().empty()));
      EXPECT_TRUE((false == !ci->getView(vocbase->name(), viewId)));
    }

    // authorised (RO collection)
    {
      arangodb::auth::UserMap userMap;
      auto& user =
          userMap
              .emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP))
              .first->second;
      user.grantCollection(vocbase->name(), "testCollection", arangodb::auth::Level::RO);  // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database

      EXPECT_TRUE((true == logicalView->drop().ok()));
      logicalCollection = ci->getCollection(vocbase->name(), collectionId);
      ASSERT_TRUE((false == !logicalCollection));
      EXPECT_TRUE((true == logicalCollection->getIndexes().empty()));
      EXPECT_TRUE((true == !ci->getView(vocbase->name(), viewId)));
    }
  }
}

TEST_F(IResearchViewCoordinatorTest, test_update_properties) {
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

    EXPECT_TRUE((ci->createDatabaseCoordinator(vocbase->name(), VPackSlice::emptyObjectSlice(), 0.0)
                     .ok()));
  }

  // create view
  {
    auto json = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
    auto viewId = std::to_string(ci->uniqid() + 1);  // +1 because LogicalView creation will generate a new ID

    EXPECT_TRUE(
        (ci->createViewCoordinator(vocbase->name(), viewId, json->slice()).ok()));

    // get current plan version
    auto planVersion = arangodb::tests::getCurrentPlanVersion();

    auto view = ci->getView(vocbase->name(), viewId);
    EXPECT_TRUE(nullptr != view);
    EXPECT_TRUE(nullptr !=
                std::dynamic_pointer_cast<arangodb::iresearch::IResearchViewCoordinator>(view));
    EXPECT_TRUE(planVersion == view->planVersion());
    EXPECT_TRUE("testView" == view->name());
    EXPECT_TRUE(false == view->deleted());
    EXPECT_TRUE(viewId == std::to_string(view->id()));
    EXPECT_TRUE(arangodb::iresearch::DATA_SOURCE_TYPE == view->type());
    EXPECT_TRUE(arangodb::LogicalView::category() == view->category());
    EXPECT_TRUE(vocbase == &view->vocbase());

    // check default properties
    {
      VPackBuilder builder;
      builder.openObject();
      view->properties(builder, arangodb::LogicalDataSource::makeFlags(
                                    arangodb::LogicalDataSource::Serialize::Detailed));
      builder.close();

      arangodb::iresearch::IResearchViewMeta meta;
      std::string error;
      EXPECT_TRUE(meta.init(builder.slice(), error));
      EXPECT_TRUE(error.empty());
      EXPECT_TRUE(meta == arangodb::iresearch::IResearchViewMeta::DEFAULT());
    }

    decltype(view) fullyUpdatedView;

    // update properties - full update
    {
      auto props = arangodb::velocypack::Parser::fromJson(
          "{ \"cleanupIntervalStep\": 42, \"consolidationIntervalMsec\": 50 "
          "}");
      EXPECT_TRUE(view->properties(props->slice(), false).ok());
      EXPECT_TRUE(planVersion < arangodb::tests::getCurrentPlanVersion());  // plan version changed
      planVersion = arangodb::tests::getCurrentPlanVersion();

      fullyUpdatedView = ci->getView(vocbase->name(), viewId);
      EXPECT_TRUE(fullyUpdatedView != view);  // different objects
      EXPECT_TRUE(nullptr != fullyUpdatedView);
      EXPECT_TRUE(nullptr != std::dynamic_pointer_cast<arangodb::iresearch::IResearchViewCoordinator>(
                                 fullyUpdatedView));
      EXPECT_TRUE(planVersion == fullyUpdatedView->planVersion());
      EXPECT_TRUE("testView" == fullyUpdatedView->name());
      EXPECT_TRUE(false == fullyUpdatedView->deleted());
      EXPECT_TRUE(viewId == std::to_string(fullyUpdatedView->id()));
      EXPECT_TRUE(arangodb::iresearch::DATA_SOURCE_TYPE == fullyUpdatedView->type());
      EXPECT_TRUE(arangodb::LogicalView::category() == fullyUpdatedView->category());
      EXPECT_TRUE(vocbase == &fullyUpdatedView->vocbase());

      // check recently updated properties
      {
        VPackBuilder builder;
        builder.openObject();
        fullyUpdatedView->properties(builder,
                                     arangodb::LogicalDataSource::makeFlags(
                                         arangodb::LogicalDataSource::Serialize::Detailed));
        builder.close();

        arangodb::iresearch::IResearchViewMeta meta;
        arangodb::iresearch::IResearchViewMeta expected;
        expected._cleanupIntervalStep = 42;
        expected._consolidationIntervalMsec = 50;
        std::string error;
        EXPECT_TRUE(meta.init(builder.slice(), error));
        EXPECT_TRUE(error.empty());
        EXPECT_TRUE(expected == meta);
      }

      // old object remains the same
      {
        VPackBuilder builder;
        builder.openObject();
        view->properties(builder, arangodb::LogicalDataSource::makeFlags(
                                      arangodb::LogicalDataSource::Serialize::Detailed));
        builder.close();

        arangodb::iresearch::IResearchViewMeta meta;
        std::string error;
        EXPECT_TRUE(meta.init(builder.slice(), error));
        EXPECT_TRUE(error.empty());
        EXPECT_TRUE(meta == arangodb::iresearch::IResearchViewMeta::DEFAULT());
      }
    }

    // partially update properties
    {
      auto props = arangodb::velocypack::Parser::fromJson(
          "{ \"consolidationIntervalMsec\": 42 }");
      EXPECT_TRUE(fullyUpdatedView->properties(props->slice(), true).ok());
      EXPECT_TRUE(planVersion < arangodb::tests::getCurrentPlanVersion());  // plan version changed
      planVersion = arangodb::tests::getCurrentPlanVersion();

      auto partiallyUpdatedView = ci->getView(vocbase->name(), viewId);
      EXPECT_TRUE(partiallyUpdatedView != view);  // different objects
      EXPECT_TRUE(nullptr != partiallyUpdatedView);
      EXPECT_TRUE(nullptr != std::dynamic_pointer_cast<arangodb::iresearch::IResearchViewCoordinator>(
                                 partiallyUpdatedView));
      EXPECT_TRUE(planVersion == partiallyUpdatedView->planVersion());
      EXPECT_TRUE("testView" == partiallyUpdatedView->name());
      EXPECT_TRUE(false == partiallyUpdatedView->deleted());
      EXPECT_TRUE(viewId == std::to_string(partiallyUpdatedView->id()));
      EXPECT_TRUE(arangodb::iresearch::DATA_SOURCE_TYPE == partiallyUpdatedView->type());
      EXPECT_TRUE(arangodb::LogicalView::category() == partiallyUpdatedView->category());
      EXPECT_TRUE(vocbase == &partiallyUpdatedView->vocbase());

      // check recently updated properties
      {
        VPackBuilder builder;
        builder.openObject();
        partiallyUpdatedView->properties(builder,
                                         arangodb::LogicalDataSource::makeFlags(
                                             arangodb::LogicalDataSource::Serialize::Detailed));
        builder.close();

        arangodb::iresearch::IResearchViewMeta meta;
        arangodb::iresearch::IResearchViewMeta expected;
        expected._cleanupIntervalStep = 42;
        expected._consolidationIntervalMsec = 42;
        std::string error;
        EXPECT_TRUE(meta.init(builder.slice(), error));
        EXPECT_TRUE(error.empty());
        EXPECT_TRUE(expected == meta);
      }
    }

    // drop view
    EXPECT_TRUE(view->drop().ok());
    EXPECT_TRUE(planVersion < arangodb::tests::getCurrentPlanVersion());

    // there is no more view
    EXPECT_TRUE(nullptr == ci->getView(vocbase->name(), view->name()));
  }
}

TEST_F(IResearchViewCoordinatorTest, test_overwrite_immutable_properties) {
  auto* database = arangodb::DatabaseFeature::DATABASE;
  ASSERT_NE(nullptr, database);

  auto* ci = arangodb::ClusterInfo::instance();
  ASSERT_NE(nullptr, ci);

  TRI_vocbase_t* vocbase;  // will be owned by DatabaseFeature

  // create database
  {
    // simulate heartbeat thread
    ASSERT_EQ(TRI_ERROR_NO_ERROR, database->createDatabase(1, "testDatabase", vocbase));

    ASSERT_NE(nullptr, vocbase);
    EXPECT_EQ("testDatabase", vocbase->name());
    EXPECT_EQ(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_COORDINATOR, vocbase->type());
    EXPECT_EQ(1, vocbase->id());

    EXPECT_TRUE(ci->createDatabaseCoordinator(vocbase->name(),
                                              VPackSlice::emptyObjectSlice(), 0.0)
                    .ok());
  }

  // create view
  auto json = arangodb::velocypack::Parser::fromJson(
      "{ \"name\": \"testView\", "
      "\"type\": \"arangosearch\", "
      "\"writebufferActive\": 25, "
      "\"writebufferIdle\": 12, "
      "\"writebufferSizeMax\": 44040192, "
      "\"locale\": \"C\", "
      "\"version\": 1, "
      "\"primarySort\": [ "
      "{ \"field\": \"my.Nested.field\", \"direction\": \"asc\" }, "
      "{ \"field\": \"another.field\", \"asc\": false } "
      "]"
      "}");

  auto viewId = std::to_string(ci->uniqid() + 1);  // +1 because LogicalView creation will generate a new ID

  EXPECT_TRUE(ci->createViewCoordinator(vocbase->name(), viewId, json->slice()).ok());

  // get current plan version
  auto planVersion = arangodb::tests::getCurrentPlanVersion();

  auto view = ci->getView(vocbase->name(), viewId);
  EXPECT_NE(nullptr, view);
  EXPECT_NE(nullptr,
            std::dynamic_pointer_cast<arangodb::iresearch::IResearchViewCoordinator>(view));
  EXPECT_EQ(planVersion, view->planVersion());
  EXPECT_EQ("testView", view->name());
  EXPECT_FALSE(view->deleted());
  EXPECT_EQ(viewId, std::to_string(view->id()));
  EXPECT_EQ(arangodb::iresearch::DATA_SOURCE_TYPE, view->type());
  EXPECT_EQ(arangodb::LogicalView::category(), view->category());
  EXPECT_EQ(vocbase, &view->vocbase());

  // check immutable properties after creation
  {
    arangodb::iresearch::IResearchViewMeta meta;
    std::string tmpString;
    VPackBuilder builder;

    builder.openObject();
    EXPECT_TRUE(view->properties(builder, arangodb::LogicalDataSource::makeFlags(
                                              arangodb::LogicalDataSource::Serialize::Detailed))
                    .ok());
    builder.close();
    EXPECT_TRUE(true == meta.init(builder.slice(), tmpString));
    EXPECT_TRUE(std::string("C") == irs::locale_utils::name(meta._locale));
    EXPECT_TRUE(1 == meta._version);
    EXPECT_TRUE(25 == meta._writebufferActive);
    EXPECT_TRUE(12 == meta._writebufferIdle);
    EXPECT_TRUE(42 * (size_t(1) << 20) == meta._writebufferSizeMax);
    EXPECT_TRUE(2 == meta._primarySort.size());
    {
      auto& field = meta._primarySort.field(0);
      EXPECT_TRUE(3 == field.size());
      EXPECT_TRUE("my" == field[0].name);
      EXPECT_TRUE(false == field[0].shouldExpand);
      EXPECT_TRUE("Nested" == field[1].name);
      EXPECT_TRUE(false == field[1].shouldExpand);
      EXPECT_TRUE("field" == field[2].name);
      EXPECT_TRUE(false == field[2].shouldExpand);
      EXPECT_TRUE(true == meta._primarySort.direction(0));
    }
    {
      auto& field = meta._primarySort.field(1);
      EXPECT_TRUE(2 == field.size());
      EXPECT_TRUE("another" == field[0].name);
      EXPECT_TRUE(false == field[0].shouldExpand);
      EXPECT_TRUE("field" == field[1].name);
      EXPECT_TRUE(false == field[1].shouldExpand);
      EXPECT_TRUE(false == meta._primarySort.direction(1));
    }
  }

  {
    auto newProperties = arangodb::velocypack::Parser::fromJson(
        "{"
        "\"writeBufferActive\": 125, "
        "\"writeBufferIdle\": 112, "
        "\"writeBufferSizeMax\": 142, "
        "\"locale\": \"en\", "
        "\"version\": 1, "
        "\"primarySort\": [ "
        "{ \"field\": \"field\", \"asc\": true } "
        "]"
        "}");

    decltype(view) fullyUpdatedView;

    // update properties
    EXPECT_TRUE(view->properties(newProperties->slice(), false).ok());
    EXPECT_EQ(planVersion, arangodb::tests::getCurrentPlanVersion());  // plan version hasn't been changed as nothing to update
    planVersion = arangodb::tests::getCurrentPlanVersion();

    fullyUpdatedView = ci->getView(vocbase->name(), viewId);
    EXPECT_EQ(fullyUpdatedView, view);  // same objects as nothing to update
  }

  {
    auto newProperties = arangodb::velocypack::Parser::fromJson(
        "{"
        "\"consolidationIntervalMsec\": 42, "
        "\"writeBufferActive\": 125, "
        "\"writeBufferIdle\": 112, "
        "\"writeBufferSizeMax\": 142, "
        "\"locale\": \"en\", "
        "\"version\": 1, "
        "\"primarySort\": [ "
        "{ \"field\": \"field\", \"asc\": true } "
        "]"
        "}");

    decltype(view) fullyUpdatedView;

    // update properties
    EXPECT_TRUE(view->properties(newProperties->slice(), false).ok());
    EXPECT_LT(planVersion, arangodb::tests::getCurrentPlanVersion());  // plan version changed
    planVersion = arangodb::tests::getCurrentPlanVersion();

    fullyUpdatedView = ci->getView(vocbase->name(), viewId);
    EXPECT_NE(fullyUpdatedView, view);  // different objects
    EXPECT_NE(nullptr, fullyUpdatedView);
    EXPECT_NE(nullptr, std::dynamic_pointer_cast<arangodb::iresearch::IResearchViewCoordinator>(
                           fullyUpdatedView));
    EXPECT_EQ(planVersion, fullyUpdatedView->planVersion());
    EXPECT_EQ("testView", fullyUpdatedView->name());
    EXPECT_FALSE(fullyUpdatedView->deleted());
    EXPECT_EQ(viewId, std::to_string(fullyUpdatedView->id()));
    EXPECT_EQ(arangodb::iresearch::DATA_SOURCE_TYPE, fullyUpdatedView->type());
    EXPECT_EQ(arangodb::LogicalView::category(), fullyUpdatedView->category());
    EXPECT_EQ(vocbase, &fullyUpdatedView->vocbase());

    // check immutable properties after update
    {
      arangodb::iresearch::IResearchViewMeta meta;
      std::string tmpString;
      VPackBuilder builder;

      builder.clear();
      builder.openObject();
      EXPECT_TRUE(fullyUpdatedView
                      ->properties(builder, arangodb::LogicalDataSource::makeFlags(
                                                arangodb::LogicalDataSource::Serialize::Detailed))
                      .ok());
      builder.close();
      EXPECT_TRUE(true == meta.init(builder.slice(), tmpString));
      EXPECT_TRUE(std::string("C") == irs::locale_utils::name(meta._locale));
      EXPECT_TRUE(1 == meta._version);
      EXPECT_TRUE(25 == meta._writebufferActive);
      EXPECT_TRUE(12 == meta._writebufferIdle);
      EXPECT_TRUE(42 * (size_t(1) << 20) == meta._writebufferSizeMax);
      EXPECT_TRUE(2 == meta._primarySort.size());
      {
        auto& field = meta._primarySort.field(0);
        EXPECT_TRUE(3 == field.size());
        EXPECT_TRUE("my" == field[0].name);
        EXPECT_TRUE(false == field[0].shouldExpand);
        EXPECT_TRUE("Nested" == field[1].name);
        EXPECT_TRUE(false == field[1].shouldExpand);
        EXPECT_TRUE("field" == field[2].name);
        EXPECT_TRUE(false == field[2].shouldExpand);
        EXPECT_TRUE(true == meta._primarySort.direction(0));
      }
      {
        auto& field = meta._primarySort.field(1);
        EXPECT_TRUE(2 == field.size());
        EXPECT_TRUE("another" == field[0].name);
        EXPECT_TRUE(false == field[0].shouldExpand);
        EXPECT_TRUE("field" == field[1].name);
        EXPECT_TRUE(false == field[1].shouldExpand);
        EXPECT_TRUE(false == meta._primarySort.direction(1));
      }
    }
  }
}

TEST_F(IResearchViewCoordinatorTest, test_update_links_partial_remove) {
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

    EXPECT_TRUE((ci->createDatabaseCoordinator(vocbase->name(), VPackSlice::emptyObjectSlice(), 0.0)
                     .ok()));
  }

  // create collections
  std::shared_ptr<arangodb::LogicalCollection> logicalCollection1;
  {
    auto const collectionId = "1";

    auto collectionJson = arangodb::velocypack::Parser::fromJson(
        "{ \"id\": \"1\", \"planId\": \"1\",  \"name\": \"testCollection1\", "
        "\"replicationFactor\": 1, \"type\": 1, \"shards\":{} }");

    EXPECT_TRUE((ci->createCollectionCoordinator(vocbase->name(), collectionId, 0, 1,
                                                 1, false, collectionJson->slice(), 0.0)
                     .ok()));

    logicalCollection1 = ci->getCollection(vocbase->name(), collectionId);
    ASSERT_TRUE(nullptr != logicalCollection1);
  }

  std::shared_ptr<arangodb::LogicalCollection> logicalCollection2;
  {
    auto const collectionId = "2";

    auto collectionJson = arangodb::velocypack::Parser::fromJson(
        "{ \"id\": \"2\", \"planId\": \"2\",  \"name\": \"testCollection2\", "
        "\"replicationFactor\": 1, \"type\": 1, \"shards\":{} }");

    EXPECT_TRUE((ci->createCollectionCoordinator(vocbase->name(), collectionId, 0, 1,
                                                 1, false, collectionJson->slice(), 0.0)
                     .ok()));

    logicalCollection2 = ci->getCollection(vocbase->name(), collectionId);
    ASSERT_TRUE(nullptr != logicalCollection2);
  }

  std::shared_ptr<arangodb::LogicalCollection> logicalCollection3;
  {
    auto const collectionId = "3";

    auto collectionJson = arangodb::velocypack::Parser::fromJson(
        "{ \"id\": \"3\", \"planId\": \"3\",  \"name\": \"testCollection3\", "
        "\"replicationFactor\": 1, \"type\": 1, \"shards\":{} }");

    EXPECT_TRUE((ci->createCollectionCoordinator(vocbase->name(), collectionId, 0, 1,
                                                 1, false, collectionJson->slice(), 0.0)
                     .ok()));

    logicalCollection3 = ci->getCollection(vocbase->name(), collectionId);
    ASSERT_TRUE(nullptr != logicalCollection3);
  }

  auto const currentCollection1Path = "/Current/Collections/" + vocbase->name() +
                                      "/" + std::to_string(logicalCollection1->id());
  auto const currentCollection2Path = "/Current/Collections/" + vocbase->name() +
                                      "/" + std::to_string(logicalCollection2->id());
  auto const currentCollection3Path = "/Current/Collections/" + vocbase->name() +
                                      "/" + std::to_string(logicalCollection3->id());

  // get current plan version
  auto planVersion = arangodb::tests::getCurrentPlanVersion();

  ci->loadCurrent();

  // create view
  auto viewJson = arangodb::velocypack::Parser::fromJson(
      "{ \"name\": \"testView\", \"id\": \"42\", \"type\": \"arangosearch\" "
      "}");
  arangodb::LogicalView::ptr view;
  ASSERT_TRUE((arangodb::LogicalView::create(view, *vocbase, viewJson->slice()).ok()));
  ASSERT_TRUE(view);
  auto const viewId = std::to_string(view->planId());
  EXPECT_TRUE("42" == viewId);

  // simulate heartbeat thread (create index in current)
  {
    auto const value = arangodb::velocypack::Parser::fromJson(
        "{ \"shard-id-does-not-matter\": { \"indexes\" : [ { \"id\": \"1\" } "
        "] } }");
    EXPECT_TRUE(arangodb::AgencyComm()
                    .setValue(currentCollection1Path, value->slice(), 0.0)
                    .successful());
  }
  {
    auto const value = arangodb::velocypack::Parser::fromJson(
        "{ \"shard-id-does-not-matter\": { \"indexes\" : [ { \"id\": \"2\" } "
        "] } }");
    EXPECT_TRUE(arangodb::AgencyComm()
                    .setValue(currentCollection2Path, value->slice(), 0.0)
                    .successful());
  }
  {
    auto const value = arangodb::velocypack::Parser::fromJson(
        "{ \"shard-id-does-not-matter\": { \"indexes\" : [ { \"id\": \"3\" } "
        "] } }");
    EXPECT_TRUE(arangodb::AgencyComm()
                    .setValue(currentCollection3Path, value->slice(), 0.0)
                    .successful());
  }

  // explicitly specify id for the sake of tests
  auto linksJson = arangodb::velocypack::Parser::fromJson(
      "{ \"links\": {"
      "  \"testCollection1\" : { \"id\": \"1\", \"includeAllFields\" : true "
      "}, "
      "  \"2\" : { \"id\": \"2\", \"trackListPositions\" : true }, "
      "  \"testCollection3\" : { \"id\": \"3\" } "
      "} }");
  EXPECT_TRUE(view->properties(linksJson->slice(), true).ok());  // add links
  EXPECT_TRUE(planVersion < arangodb::tests::getCurrentPlanVersion());  // plan version changed
  planVersion = arangodb::tests::getCurrentPlanVersion();
  auto oldView = view;
  view = ci->getView(vocbase->name(), viewId);  // get new version of the view
  EXPECT_TRUE(view != oldView);                 // different objects
  EXPECT_TRUE(nullptr != view);
  EXPECT_TRUE(nullptr !=
              std::dynamic_pointer_cast<arangodb::iresearch::IResearchViewCoordinator>(view));
  EXPECT_TRUE(planVersion == view->planVersion());
  EXPECT_TRUE("testView" == view->name());
  EXPECT_TRUE(false == view->deleted());
  EXPECT_TRUE(42 == view->id());
  EXPECT_TRUE(arangodb::iresearch::DATA_SOURCE_TYPE == view->type());
  EXPECT_TRUE(arangodb::LogicalView::category() == view->category());
  EXPECT_TRUE(vocbase == &view->vocbase());

  // visit collections
  {
    std::set<TRI_voc_cid_t> expectedLinks{logicalCollection1->id(),
                                          logicalCollection2->id(),
                                          logicalCollection3->id()};
    EXPECT_TRUE(view->visitCollections([&expectedLinks](TRI_voc_cid_t cid) mutable {
      return 1 == expectedLinks.erase(cid);
    }));
    EXPECT_TRUE(expectedLinks.empty());
  }

  // check properties
  {
    VPackBuilder info;
    info.openObject();
    view->properties(info, arangodb::LogicalDataSource::makeFlags(
                               arangodb::LogicalDataSource::Serialize::Detailed));
    info.close();

    auto const properties = info.slice();
    EXPECT_TRUE(properties.hasKey(arangodb::iresearch::StaticStrings::LinksField));
    auto const linksSlice = properties.get(arangodb::iresearch::StaticStrings::LinksField);
    EXPECT_TRUE(linksSlice.isObject());

    VPackObjectIterator it(linksSlice);
    EXPECT_TRUE(it.valid());
    EXPECT_TRUE(3 == it.size());

    // testCollection1
    {
      auto const value = linksSlice.get(logicalCollection1->name());
      EXPECT_TRUE(value.isObject());

      arangodb::iresearch::IResearchLinkMeta expectedMeta;
      expectedMeta._includeAllFields = true;
      arangodb::iresearch::IResearchLinkMeta actualMeta;
      std::string error;
      EXPECT_TRUE(actualMeta.init(value, false, error));
      EXPECT_TRUE(error.empty());
      EXPECT_TRUE(expectedMeta == actualMeta);
    }

    // testCollection2
    {
      auto const value = linksSlice.get(logicalCollection2->name());
      EXPECT_TRUE(value.isObject());

      arangodb::iresearch::IResearchLinkMeta expectedMeta;
      expectedMeta._trackListPositions = true;
      arangodb::iresearch::IResearchLinkMeta actualMeta;
      std::string error;
      EXPECT_TRUE(actualMeta.init(value, false, error));
      EXPECT_TRUE(error.empty());
      EXPECT_TRUE(expectedMeta == actualMeta);
    }

    // testCollection3
    {
      auto const value = linksSlice.get(logicalCollection3->name());
      EXPECT_TRUE(value.isObject());

      arangodb::iresearch::IResearchLinkMeta expectedMeta;
      arangodb::iresearch::IResearchLinkMeta actualMeta;
      std::string error;
      EXPECT_TRUE(actualMeta.init(value, false, error));
      EXPECT_TRUE(error.empty());
      EXPECT_TRUE(expectedMeta == actualMeta);
    }
  }

  // test index in testCollection1
  {
    // get new version from plan
    auto updatedCollection =
        ci->getCollection(vocbase->name(), std::to_string(logicalCollection1->id()));
    ASSERT_TRUE(updatedCollection);
    auto link = arangodb::iresearch::IResearchLinkHelper::find(*updatedCollection, *view);
    EXPECT_TRUE(link);

    auto index = std::dynamic_pointer_cast<arangodb::Index>(link);
    ASSERT_TRUE((false == !index));
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

    arangodb::iresearch::IResearchLinkMeta expectedMeta;
    expectedMeta._includeAllFields = true;
    arangodb::iresearch::IResearchLinkMeta actualMeta;
    auto builder = index->toVelocyPack(
        arangodb::Index::makeFlags(arangodb::Index::Serialize::Figures));

    std::string error;
    EXPECT_TRUE(actualMeta.init(builder->slice(), false, error));
    EXPECT_TRUE(error.empty());
    EXPECT_TRUE(expectedMeta == actualMeta);
    auto const slice = builder->slice();
    EXPECT_TRUE(slice.hasKey("view"));
    EXPECT_TRUE(slice.get("view").isString());
    EXPECT_TRUE(view->guid() == slice.get("view").copyString());
    EXPECT_TRUE(slice.hasKey("figures"));
    EXPECT_TRUE(slice.get("figures").isObject());
    EXPECT_TRUE(slice.get("figures").hasKey("memory"));
    EXPECT_TRUE(slice.get("figures").get("memory").isNumber());
    EXPECT_TRUE(0 < slice.get("figures").get("memory").getUInt());
  }

  // test index in testCollection2
  {
    // get new version from plan
    auto updatedCollection =
        ci->getCollection(vocbase->name(), std::to_string(logicalCollection2->id()));
    ASSERT_TRUE(updatedCollection);
    auto link = arangodb::iresearch::IResearchLinkHelper::find(*updatedCollection, *view);
    EXPECT_TRUE(link);

    auto index = std::dynamic_pointer_cast<arangodb::Index>(link);
    ASSERT_TRUE((false == !index));
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

    arangodb::iresearch::IResearchLinkMeta expectedMeta;
    expectedMeta._trackListPositions = true;
    arangodb::iresearch::IResearchLinkMeta actualMeta;
    auto builder = index->toVelocyPack(
        arangodb::Index::makeFlags(arangodb::Index::Serialize::Figures));

    std::string error;
    EXPECT_TRUE(actualMeta.init(builder->slice(), false, error));
    EXPECT_TRUE(error.empty());
    EXPECT_TRUE(expectedMeta == actualMeta);
    auto const slice = builder->slice();
    EXPECT_TRUE(slice.hasKey("view"));
    EXPECT_TRUE(slice.get("view").isString());
    EXPECT_TRUE(view->id() == 42);
    EXPECT_TRUE(view->guid() == slice.get("view").copyString());
    EXPECT_TRUE(slice.hasKey("figures"));
    EXPECT_TRUE(slice.get("figures").isObject());
    EXPECT_TRUE(slice.get("figures").hasKey("memory"));
    EXPECT_TRUE(slice.get("figures").get("memory").isNumber());
    EXPECT_TRUE(0 < slice.get("figures").get("memory").getUInt());
  }

  // test index in testCollection3
  {
    auto updatedCollection =
        ci->getCollection(vocbase->name(), std::to_string(logicalCollection3->id()));
    ASSERT_TRUE(updatedCollection);
    auto link = arangodb::iresearch::IResearchLinkHelper::find(*updatedCollection, *view);
    EXPECT_TRUE(link);

    auto index = std::dynamic_pointer_cast<arangodb::Index>(link);
    ASSERT_TRUE((false == !index));
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

    arangodb::iresearch::IResearchLinkMeta expectedMeta;
    arangodb::iresearch::IResearchLinkMeta actualMeta;
    auto builder = index->toVelocyPack(
        arangodb::Index::makeFlags(arangodb::Index::Serialize::Figures));

    std::string error;
    EXPECT_TRUE(actualMeta.init(builder->slice(), false, error));
    EXPECT_TRUE(error.empty());
    EXPECT_TRUE(expectedMeta == actualMeta);
    auto const slice = builder->slice();
    EXPECT_TRUE(slice.hasKey("view"));
    EXPECT_TRUE(slice.get("view").isString());
    EXPECT_TRUE(view->id() == 42);
    EXPECT_TRUE(view->guid() == slice.get("view").copyString());
    EXPECT_TRUE(slice.hasKey("figures"));
    EXPECT_TRUE(slice.get("figures").isObject());
    EXPECT_TRUE(slice.get("figures").hasKey("memory"));
    EXPECT_TRUE(slice.get("figures").get("memory").isNumber());
    EXPECT_TRUE(0 < slice.get("figures").get("memory").getUInt());
  }

  EXPECT_TRUE(view->properties(linksJson->slice(), true).ok());  // same properties -> should not affect plan version
  EXPECT_TRUE(planVersion == arangodb::tests::getCurrentPlanVersion());  // plan did't change version

  // remove testCollection2 link
  // simulate heartbeat thread (create index in current)
  {
    auto const value = arangodb::velocypack::Parser::fromJson(
        "{ \"shard-id-does-not-matter\": { \"indexes\" : [ ] } }");
    EXPECT_TRUE(arangodb::AgencyComm()
                    .setValue(currentCollection2Path, value->slice(), 0.0)
                    .successful());
  }

  auto const updateJson = arangodb::velocypack::Parser::fromJson(
      "{ \"links\": {"
      "  \"2\" : null "
      "} }");
  EXPECT_TRUE(view->properties(updateJson->slice(), true).ok());
  EXPECT_TRUE(planVersion < arangodb::tests::getCurrentPlanVersion());  // plan version changed
  planVersion = arangodb::tests::getCurrentPlanVersion();
  oldView = view;
  view = ci->getView(vocbase->name(), viewId);  // get new version of the view
  EXPECT_TRUE(view != oldView);                 // different objects
  EXPECT_TRUE(nullptr != view);
  EXPECT_TRUE(nullptr !=
              std::dynamic_pointer_cast<arangodb::iresearch::IResearchViewCoordinator>(view));
  EXPECT_TRUE(planVersion == view->planVersion());
  EXPECT_TRUE("testView" == view->name());
  EXPECT_TRUE(false == view->deleted());
  EXPECT_TRUE(42 == view->id());
  EXPECT_TRUE(arangodb::iresearch::DATA_SOURCE_TYPE == view->type());
  EXPECT_TRUE(arangodb::LogicalView::category() == view->category());
  EXPECT_TRUE(vocbase == &view->vocbase());

  // visit collections
  {
    std::set<TRI_voc_cid_t> expectedLinks{logicalCollection1->id(),
                                          logicalCollection3->id()};
    EXPECT_TRUE(view->visitCollections([&expectedLinks](TRI_voc_cid_t cid) mutable {
      return 1 == expectedLinks.erase(cid);
    }));
    EXPECT_TRUE(expectedLinks.empty());
  }

  // check properties
  {
    VPackBuilder info;
    info.openObject();
    view->properties(info, arangodb::LogicalDataSource::makeFlags(
                               arangodb::LogicalDataSource::Serialize::Detailed));
    info.close();

    auto const properties = info.slice();
    EXPECT_TRUE(properties.hasKey(arangodb::iresearch::StaticStrings::LinksField));
    auto const linksSlice = properties.get(arangodb::iresearch::StaticStrings::LinksField);
    EXPECT_TRUE(linksSlice.isObject());

    VPackObjectIterator it(linksSlice);
    EXPECT_TRUE(it.valid());
    EXPECT_TRUE(2 == it.size());

    // testCollection1
    {
      auto const value = linksSlice.get(logicalCollection1->name());
      EXPECT_TRUE(value.isObject());

      arangodb::iresearch::IResearchLinkMeta expectedMeta;
      expectedMeta._includeAllFields = true;
      arangodb::iresearch::IResearchLinkMeta actualMeta;
      std::string error;
      EXPECT_TRUE(actualMeta.init(value, false, error));
      EXPECT_TRUE(error.empty());
      EXPECT_TRUE(expectedMeta == actualMeta);
    }

    // testCollection3
    {
      auto const value = linksSlice.get(logicalCollection3->name());
      EXPECT_TRUE(value.isObject());

      arangodb::iresearch::IResearchLinkMeta expectedMeta;
      arangodb::iresearch::IResearchLinkMeta actualMeta;
      std::string error;
      EXPECT_TRUE(actualMeta.init(value, false, error));
      EXPECT_TRUE(error.empty());
      EXPECT_TRUE(expectedMeta == actualMeta);
    }
  }

  // test index in testCollection1
  {
    // get new version from plan
    auto updatedCollection =
        ci->getCollection(vocbase->name(), std::to_string(logicalCollection1->id()));
    ASSERT_TRUE(updatedCollection);
    auto link = arangodb::iresearch::IResearchLinkHelper::find(*updatedCollection, *view);
    EXPECT_TRUE(link);

    auto index = std::dynamic_pointer_cast<arangodb::Index>(link);
    ASSERT_TRUE((false == !index));
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

    arangodb::iresearch::IResearchLinkMeta expectedMeta;
    expectedMeta._includeAllFields = true;
    arangodb::iresearch::IResearchLinkMeta actualMeta;
    auto builder = index->toVelocyPack(
        arangodb::Index::makeFlags(arangodb::Index::Serialize::Figures));

    std::string error;
    EXPECT_TRUE(actualMeta.init(builder->slice(), false, error));
    EXPECT_TRUE(error.empty());
    EXPECT_TRUE(expectedMeta == actualMeta);
    auto const slice = builder->slice();
    EXPECT_TRUE(slice.hasKey("view"));
    EXPECT_TRUE(slice.get("view").isString());
    EXPECT_TRUE(view->id() == 42);
    EXPECT_TRUE(view->guid() == slice.get("view").copyString());
    EXPECT_TRUE(slice.hasKey("figures"));
    EXPECT_TRUE(slice.get("figures").isObject());
    EXPECT_TRUE(slice.get("figures").hasKey("memory"));
    EXPECT_TRUE(slice.get("figures").get("memory").isNumber());
    EXPECT_TRUE(0 < slice.get("figures").get("memory").getUInt());
  }

  // test index in testCollection3
  {
    // get new version from plan
    auto updatedCollection =
        ci->getCollection(vocbase->name(), std::to_string(logicalCollection3->id()));
    ASSERT_TRUE(updatedCollection);
    auto link = arangodb::iresearch::IResearchLinkHelper::find(*updatedCollection, *view);
    EXPECT_TRUE(link);

    auto index = std::dynamic_pointer_cast<arangodb::Index>(link);
    ASSERT_TRUE((false == !index));
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

    arangodb::iresearch::IResearchLinkMeta expectedMeta;
    arangodb::iresearch::IResearchLinkMeta actualMeta;
    auto builder = index->toVelocyPack(
        arangodb::Index::makeFlags(arangodb::Index::Serialize::Figures));

    std::string error;
    EXPECT_TRUE(actualMeta.init(builder->slice(), false, error));
    EXPECT_TRUE(error.empty());
    EXPECT_TRUE(expectedMeta == actualMeta);
    auto const slice = builder->slice();
    EXPECT_TRUE(slice.hasKey("view"));
    EXPECT_TRUE(slice.get("view").isString());
    EXPECT_TRUE(view->id() == 42);
    EXPECT_TRUE(view->guid() == slice.get("view").copyString());
    EXPECT_TRUE(slice.hasKey("figures"));
    EXPECT_TRUE(slice.get("figures").isObject());
    EXPECT_TRUE(slice.get("figures").hasKey("memory"));
    EXPECT_TRUE(slice.get("figures").get("memory").isNumber());
    EXPECT_TRUE(0 < slice.get("figures").get("memory").getUInt());
  }

  // drop view
  // simulate heartbeat thread (drop index in current)
  {
    auto const value = arangodb::velocypack::Parser::fromJson(
        "{ \"shard-id-does-not-matter\": { \"indexes\" : [ ] } }");
    EXPECT_TRUE(arangodb::AgencyComm()
                    .setValue(currentCollection1Path, value->slice(), 0.0)
                    .successful());
  }
  {
    auto const value = arangodb::velocypack::Parser::fromJson(
        "{ \"shard-id-does-not-matter\": { \"indexes\" : [ ] } }");
    EXPECT_TRUE(arangodb::AgencyComm()
                    .setValue(currentCollection3Path, value->slice(), 0.0)
                    .successful());
  }

  EXPECT_TRUE(view->drop().ok());
  EXPECT_TRUE(planVersion < arangodb::tests::getCurrentPlanVersion());

  // there is no more view
  EXPECT_TRUE(nullptr == ci->getView(vocbase->name(), view->name()));

  // there are no more links
  {
    // get new version from plan
    auto updatedCollection =
        ci->getCollection(vocbase->name(), std::to_string(logicalCollection1->id()));
    ASSERT_TRUE(updatedCollection);
    auto link = arangodb::iresearch::IResearchLinkHelper::find(*updatedCollection, *view);
    EXPECT_TRUE(!link);
  }

  {
    // get new version from plan
    auto updatedCollection =
        ci->getCollection(vocbase->name(), std::to_string(logicalCollection2->id()));
    ASSERT_TRUE(updatedCollection);
    auto link = arangodb::iresearch::IResearchLinkHelper::find(*updatedCollection, *view);
    EXPECT_TRUE(!link);
  }

  {
    // get new version from plan
    auto updatedCollection =
        ci->getCollection(vocbase->name(), std::to_string(logicalCollection3->id()));
    ASSERT_TRUE(updatedCollection);
    auto link = arangodb::iresearch::IResearchLinkHelper::find(*updatedCollection, *view);
    EXPECT_TRUE(!link);
  }
}

TEST_F(IResearchViewCoordinatorTest, test_update_links_partial_add) {
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

    EXPECT_TRUE((ci->createDatabaseCoordinator(vocbase->name(), VPackSlice::emptyObjectSlice(), 0.0)
                     .ok()));
  }

  // create collections
  std::shared_ptr<arangodb::LogicalCollection> logicalCollection1;
  {
    auto const collectionId = "1";

    auto collectionJson = arangodb::velocypack::Parser::fromJson(
        "{ \"id\": \"1\", \"planId\": \"1\",  \"name\": \"testCollection1\", "
        "\"replicationFactor\": 1, \"type\": 1, \"shards\":{} }");

    EXPECT_TRUE((ci->createCollectionCoordinator(vocbase->name(), collectionId, 0, 1,
                                                 1, false, collectionJson->slice(), 0.0)
                     .ok()));

    logicalCollection1 = ci->getCollection(vocbase->name(), collectionId);
    ASSERT_TRUE(nullptr != logicalCollection1);
  }

  std::shared_ptr<arangodb::LogicalCollection> logicalCollection2;
  {
    auto const collectionId = "2";

    auto collectionJson = arangodb::velocypack::Parser::fromJson(
        "{ \"id\": \"2\", \"planId\": \"2\",  \"name\": \"testCollection2\", "
        "\"replicationFactor\": 1, \"type\": 1, \"shards\":{} }");

    EXPECT_TRUE((ci->createCollectionCoordinator(vocbase->name(), collectionId, 0, 1,
                                                 1, false, collectionJson->slice(), 0.0)
                     .ok()));

    logicalCollection2 = ci->getCollection(vocbase->name(), collectionId);
    ASSERT_TRUE(nullptr != logicalCollection2);
  }

  std::shared_ptr<arangodb::LogicalCollection> logicalCollection3;
  {
    auto const collectionId = "3";

    auto collectionJson = arangodb::velocypack::Parser::fromJson(
        "{ \"id\": \"3\", \"planId\": \"3\",  \"name\": \"testCollection3\", "
        "\"replicationFactor\": 1, \"type\": 1, \"shards\":{} }");

    EXPECT_TRUE((ci->createCollectionCoordinator(vocbase->name(), collectionId, 0, 1,
                                                 1, false, collectionJson->slice(), 0.0)
                     .ok()));

    logicalCollection3 = ci->getCollection(vocbase->name(), collectionId);
    ASSERT_TRUE(nullptr != logicalCollection3);
  }

  auto const currentCollection1Path = "/Current/Collections/" + vocbase->name() +
                                      "/" + std::to_string(logicalCollection1->id());
  auto const currentCollection2Path = "/Current/Collections/" + vocbase->name() +
                                      "/" + std::to_string(logicalCollection2->id());
  auto const currentCollection3Path = "/Current/Collections/" + vocbase->name() +
                                      "/" + std::to_string(logicalCollection3->id());

  // get current plan version
  auto planVersion = arangodb::tests::getCurrentPlanVersion();

  ci->loadCurrent();

  // create view
  auto viewJson = arangodb::velocypack::Parser::fromJson(
      "{ \"name\": \"testView\", \"id\": \"42\", \"type\": \"arangosearch\" "
      "}");
  arangodb::LogicalView::ptr view;
  ASSERT_TRUE((arangodb::LogicalView::create(view, *vocbase, viewJson->slice()).ok()));
  ASSERT_TRUE(view);
  auto const viewId = std::to_string(view->planId());
  EXPECT_TRUE("42" == viewId);

  // simulate heartbeat thread (create index in current)
  {
    auto const value = arangodb::velocypack::Parser::fromJson(
        "{ \"shard-id-does-not-matter\": { \"indexes\" : [ { \"id\": \"1\" } "
        "] } }");
    EXPECT_TRUE(arangodb::AgencyComm()
                    .setValue(currentCollection1Path, value->slice(), 0.0)
                    .successful());
  }
  {
    auto const value = arangodb::velocypack::Parser::fromJson(
        "{ \"shard-id-does-not-matter\": { \"indexes\" : [ { \"id\": \"3\" } "
        "] } }");
    EXPECT_TRUE(arangodb::AgencyComm()
                    .setValue(currentCollection3Path, value->slice(), 0.0)
                    .successful());
  }

  // explicitly specify id for the sake of tests
  auto linksJson = arangodb::velocypack::Parser::fromJson(
      "{ \"links\": {"
      "  \"testCollection1\" : { \"id\": \"1\", \"includeAllFields\" : true "
      "}, "
      "  \"testCollection3\" : { \"id\": \"3\" } "
      "} }");
  EXPECT_TRUE(view->properties(linksJson->slice(), true).ok());  // add links
  EXPECT_TRUE(planVersion < arangodb::tests::getCurrentPlanVersion());  // plan version changed
  planVersion = arangodb::tests::getCurrentPlanVersion();
  auto oldView = view;
  view = ci->getView(vocbase->name(), viewId);  // get new version of the view
  EXPECT_TRUE(view != oldView);                 // different objects
  EXPECT_TRUE(nullptr != view);
  EXPECT_TRUE(nullptr !=
              std::dynamic_pointer_cast<arangodb::iresearch::IResearchViewCoordinator>(view));
  EXPECT_TRUE(planVersion == view->planVersion());
  EXPECT_TRUE("testView" == view->name());
  EXPECT_TRUE(false == view->deleted());
  EXPECT_TRUE(42 == view->id());
  EXPECT_TRUE(arangodb::iresearch::DATA_SOURCE_TYPE == view->type());
  EXPECT_TRUE(arangodb::LogicalView::category() == view->category());
  EXPECT_TRUE(vocbase == &view->vocbase());

  // visit collections
  {
    std::set<TRI_voc_cid_t> expectedLinks{logicalCollection1->id(),
                                          logicalCollection3->id()};
    EXPECT_TRUE(view->visitCollections([&expectedLinks](TRI_voc_cid_t cid) mutable {
      return 1 == expectedLinks.erase(cid);
    }));
    EXPECT_TRUE(expectedLinks.empty());
  }

  // check properties
  {
    VPackBuilder info;
    info.openObject();
    view->properties(info, arangodb::LogicalDataSource::makeFlags(
                               arangodb::LogicalDataSource::Serialize::Detailed));
    info.close();

    auto const properties = info.slice();
    EXPECT_TRUE(properties.hasKey(arangodb::iresearch::StaticStrings::LinksField));
    auto const linksSlice = properties.get(arangodb::iresearch::StaticStrings::LinksField);
    EXPECT_TRUE(linksSlice.isObject());

    VPackObjectIterator it(linksSlice);
    EXPECT_TRUE(it.valid());
    EXPECT_TRUE(2 == it.size());

    // testCollection1
    {
      auto const value = linksSlice.get(logicalCollection1->name());
      EXPECT_TRUE(value.isObject());

      arangodb::iresearch::IResearchLinkMeta expectedMeta;
      expectedMeta._includeAllFields = true;
      arangodb::iresearch::IResearchLinkMeta actualMeta;
      std::string error;
      EXPECT_TRUE(actualMeta.init(value, false, error));
      EXPECT_TRUE(error.empty());
      EXPECT_TRUE(expectedMeta == actualMeta);
    }

    // testCollection3
    {
      auto const value = linksSlice.get(logicalCollection3->name());
      EXPECT_TRUE(value.isObject());

      arangodb::iresearch::IResearchLinkMeta expectedMeta;
      arangodb::iresearch::IResearchLinkMeta actualMeta;
      std::string error;
      EXPECT_TRUE(actualMeta.init(value, false, error));
      EXPECT_TRUE(error.empty());
      EXPECT_TRUE(expectedMeta == actualMeta);
    }
  }

  // test index in testCollection1
  {
    // get new version from plan
    auto updatedCollection =
        ci->getCollection(vocbase->name(), std::to_string(logicalCollection1->id()));
    ASSERT_TRUE(updatedCollection);
    auto link = arangodb::iresearch::IResearchLinkHelper::find(*updatedCollection, *view);
    EXPECT_TRUE(link);

    auto index = std::dynamic_pointer_cast<arangodb::Index>(link);
    ASSERT_TRUE((false == !index));
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

    arangodb::iresearch::IResearchLinkMeta expectedMeta;
    expectedMeta._includeAllFields = true;
    arangodb::iresearch::IResearchLinkMeta actualMeta;
    auto builder = index->toVelocyPack(
        arangodb::Index::makeFlags(arangodb::Index::Serialize::Figures));

    std::string error;
    EXPECT_TRUE(actualMeta.init(builder->slice(), false, error));
    EXPECT_TRUE(error.empty());
    EXPECT_TRUE(expectedMeta == actualMeta);
    auto const slice = builder->slice();
    EXPECT_TRUE(slice.hasKey("view"));
    EXPECT_TRUE(slice.get("view").isString());
    EXPECT_TRUE(view->id() == 42);
    EXPECT_TRUE(view->guid() == slice.get("view").copyString());
    EXPECT_TRUE(slice.hasKey("figures"));
    EXPECT_TRUE(slice.get("figures").isObject());
    EXPECT_TRUE(slice.get("figures").hasKey("memory"));
    EXPECT_TRUE(slice.get("figures").get("memory").isNumber());
    EXPECT_TRUE(0 < slice.get("figures").get("memory").getUInt());
  }

  // test index in testCollection3
  {
    auto updatedCollection =
        ci->getCollection(vocbase->name(), std::to_string(logicalCollection3->id()));
    ASSERT_TRUE(updatedCollection);
    auto link = arangodb::iresearch::IResearchLinkHelper::find(*updatedCollection, *view);
    EXPECT_TRUE(link);

    auto index = std::dynamic_pointer_cast<arangodb::Index>(link);
    ASSERT_TRUE((false == !index));
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

    arangodb::iresearch::IResearchLinkMeta expectedMeta;
    arangodb::iresearch::IResearchLinkMeta actualMeta;
    auto builder = index->toVelocyPack(
        arangodb::Index::makeFlags(arangodb::Index::Serialize::Figures));

    std::string error;
    EXPECT_TRUE(actualMeta.init(builder->slice(), false, error));
    EXPECT_TRUE(error.empty());
    EXPECT_TRUE(expectedMeta == actualMeta);
    auto const slice = builder->slice();
    EXPECT_TRUE(slice.hasKey("view"));
    EXPECT_TRUE(slice.get("view").isString());
    EXPECT_TRUE(view->id() == 42);
    EXPECT_TRUE(view->guid() == slice.get("view").copyString());
    EXPECT_TRUE(slice.hasKey("figures"));
    EXPECT_TRUE(slice.get("figures").isObject());
    EXPECT_TRUE(slice.get("figures").hasKey("memory"));
    EXPECT_TRUE(slice.get("figures").get("memory").isNumber());
    EXPECT_TRUE(0 < slice.get("figures").get("memory").getUInt());
  }

  EXPECT_TRUE(view->properties(linksJson->slice(), true).ok());  // same properties -> should not affect plan version
  EXPECT_TRUE(planVersion == arangodb::tests::getCurrentPlanVersion());  // plan did't change version

  // remove testCollection2 link
  // simulate heartbeat thread (create index in current)
  {
    auto const value = arangodb::velocypack::Parser::fromJson(
        "{ \"shard-id-does-not-matter\": { \"indexes\" : [ { \"id\": \"2\" } "
        "] } }");
    EXPECT_TRUE(arangodb::AgencyComm()
                    .setValue(currentCollection2Path, value->slice(), 0.0)
                    .successful());
  }

  auto const updateJson = arangodb::velocypack::Parser::fromJson(
      "{ \"links\": {"
      "  \"2\" : { \"id\": \"2\", \"trackListPositions\" : true } "
      "} }");
  EXPECT_TRUE(view->properties(updateJson->slice(), true).ok());
  EXPECT_TRUE(planVersion < arangodb::tests::getCurrentPlanVersion());  // plan version changed
  planVersion = arangodb::tests::getCurrentPlanVersion();
  oldView = view;
  view = ci->getView(vocbase->name(), viewId);  // get new version of the view
  EXPECT_TRUE(view != oldView);                 // different objects
  EXPECT_TRUE(nullptr != view);
  EXPECT_TRUE(nullptr !=
              std::dynamic_pointer_cast<arangodb::iresearch::IResearchViewCoordinator>(view));
  EXPECT_TRUE(planVersion == view->planVersion());
  EXPECT_TRUE("testView" == view->name());
  EXPECT_TRUE(false == view->deleted());
  EXPECT_TRUE(42 == view->id());
  EXPECT_TRUE(arangodb::iresearch::DATA_SOURCE_TYPE == view->type());
  EXPECT_TRUE(arangodb::LogicalView::category() == view->category());
  EXPECT_TRUE(vocbase == &view->vocbase());

  // visit collections
  {
    std::set<TRI_voc_cid_t> expectedLinks{logicalCollection1->id(),
                                          logicalCollection2->id(),
                                          logicalCollection3->id()};
    EXPECT_TRUE(view->visitCollections([&expectedLinks](TRI_voc_cid_t cid) mutable {
      return 1 == expectedLinks.erase(cid);
    }));
    EXPECT_TRUE(expectedLinks.empty());
  }

  // check properties
  {
    VPackBuilder info;
    info.openObject();
    view->properties(info, arangodb::LogicalDataSource::makeFlags(
                               arangodb::LogicalDataSource::Serialize::Detailed));
    info.close();

    auto const properties = info.slice();
    EXPECT_TRUE(properties.hasKey(arangodb::iresearch::StaticStrings::LinksField));
    auto const linksSlice = properties.get(arangodb::iresearch::StaticStrings::LinksField);
    EXPECT_TRUE(linksSlice.isObject());

    VPackObjectIterator it(linksSlice);
    EXPECT_TRUE(it.valid());
    EXPECT_TRUE(3 == it.size());

    // testCollection1
    {
      auto const value = linksSlice.get(logicalCollection1->name());
      EXPECT_TRUE(value.isObject());

      arangodb::iresearch::IResearchLinkMeta expectedMeta;
      expectedMeta._includeAllFields = true;
      arangodb::iresearch::IResearchLinkMeta actualMeta;
      std::string error;
      EXPECT_TRUE(actualMeta.init(value, false, error));
      EXPECT_TRUE(error.empty());
      EXPECT_TRUE(expectedMeta == actualMeta);
    }

    // testCollection2
    {
      auto const value = linksSlice.get(logicalCollection2->name());
      EXPECT_TRUE(value.isObject());

      arangodb::iresearch::IResearchLinkMeta expectedMeta;
      expectedMeta._trackListPositions = true;
      arangodb::iresearch::IResearchLinkMeta actualMeta;
      std::string error;
      EXPECT_TRUE(actualMeta.init(value, false, error));
      EXPECT_TRUE(error.empty());
      EXPECT_TRUE(expectedMeta == actualMeta);
    }

    // testCollection3
    {
      auto const value = linksSlice.get(logicalCollection3->name());
      EXPECT_TRUE(value.isObject());

      arangodb::iresearch::IResearchLinkMeta expectedMeta;
      arangodb::iresearch::IResearchLinkMeta actualMeta;
      std::string error;
      EXPECT_TRUE(actualMeta.init(value, false, error));
      EXPECT_TRUE(error.empty());
      EXPECT_TRUE(expectedMeta == actualMeta);
    }
  }

  // test index in testCollection1
  {
    // get new version from plan
    auto updatedCollection =
        ci->getCollection(vocbase->name(), std::to_string(logicalCollection1->id()));
    ASSERT_TRUE(updatedCollection);
    auto link = arangodb::iresearch::IResearchLinkHelper::find(*updatedCollection, *view);
    EXPECT_TRUE(link);

    auto index = std::dynamic_pointer_cast<arangodb::Index>(link);
    ASSERT_TRUE((false == !index));
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

    arangodb::iresearch::IResearchLinkMeta expectedMeta;
    expectedMeta._includeAllFields = true;
    arangodb::iresearch::IResearchLinkMeta actualMeta;
    auto builder = index->toVelocyPack(
        arangodb::Index::makeFlags(arangodb::Index::Serialize::Figures));

    std::string error;
    EXPECT_TRUE(actualMeta.init(builder->slice(), false, error));
    EXPECT_TRUE(error.empty());
    EXPECT_TRUE(expectedMeta == actualMeta);
    auto const slice = builder->slice();
    EXPECT_TRUE(slice.hasKey("view"));
    EXPECT_TRUE(slice.get("view").isString());
    EXPECT_TRUE(view->id() == 42);
    EXPECT_TRUE(view->guid() == slice.get("view").copyString());
    EXPECT_TRUE(slice.hasKey("figures"));
    EXPECT_TRUE(slice.get("figures").isObject());
    EXPECT_TRUE(slice.get("figures").hasKey("memory"));
    EXPECT_TRUE(slice.get("figures").get("memory").isNumber());
    EXPECT_TRUE(0 < slice.get("figures").get("memory").getUInt());
  }

  // test index in testCollection2
  {
    // get new version from plan
    auto updatedCollection =
        ci->getCollection(vocbase->name(), std::to_string(logicalCollection2->id()));
    ASSERT_TRUE(updatedCollection);
    auto link = arangodb::iresearch::IResearchLinkHelper::find(*updatedCollection, *view);
    EXPECT_TRUE(link);

    auto index = std::dynamic_pointer_cast<arangodb::Index>(link);
    ASSERT_TRUE((false == !index));
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

    arangodb::iresearch::IResearchLinkMeta expectedMeta;
    expectedMeta._trackListPositions = true;
    arangodb::iresearch::IResearchLinkMeta actualMeta;
    auto builder = index->toVelocyPack(
        arangodb::Index::makeFlags(arangodb::Index::Serialize::Figures));

    std::string error;
    EXPECT_TRUE(actualMeta.init(builder->slice(), false, error));
    EXPECT_TRUE(error.empty());
    EXPECT_TRUE(expectedMeta == actualMeta);
    auto const slice = builder->slice();
    EXPECT_TRUE(slice.hasKey("view"));
    EXPECT_TRUE(slice.get("view").isString());
    EXPECT_TRUE(view->id() == 42);
    EXPECT_TRUE(view->guid() == slice.get("view").copyString());
    EXPECT_TRUE(slice.hasKey("figures"));
    EXPECT_TRUE(slice.get("figures").isObject());
    EXPECT_TRUE(slice.get("figures").hasKey("memory"));
    EXPECT_TRUE(slice.get("figures").get("memory").isNumber());
    EXPECT_TRUE(0 < slice.get("figures").get("memory").getUInt());
  }

  // test index in testCollection3
  {
    // get new version from plan
    auto updatedCollection =
        ci->getCollection(vocbase->name(), std::to_string(logicalCollection3->id()));
    ASSERT_TRUE(updatedCollection);
    auto link = arangodb::iresearch::IResearchLinkHelper::find(*updatedCollection, *view);
    EXPECT_TRUE(link);

    auto index = std::dynamic_pointer_cast<arangodb::Index>(link);
    ASSERT_TRUE((false == !index));
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

    arangodb::iresearch::IResearchLinkMeta expectedMeta;
    arangodb::iresearch::IResearchLinkMeta actualMeta;
    auto builder = index->toVelocyPack(
        arangodb::Index::makeFlags(arangodb::Index::Serialize::Figures));

    std::string error;
    EXPECT_TRUE(actualMeta.init(builder->slice(), false, error));
    EXPECT_TRUE(error.empty());
    EXPECT_TRUE(expectedMeta == actualMeta);
    auto const slice = builder->slice();
    EXPECT_TRUE(slice.hasKey("view"));
    EXPECT_TRUE(slice.get("view").isString());
    EXPECT_TRUE(view->id() == 42);
    EXPECT_TRUE(view->guid() == slice.get("view").copyString());
    EXPECT_TRUE(slice.hasKey("figures"));
    EXPECT_TRUE(slice.get("figures").isObject());
    EXPECT_TRUE(slice.get("figures").hasKey("memory"));
    EXPECT_TRUE(slice.get("figures").get("memory").isNumber());
    EXPECT_TRUE(0 < slice.get("figures").get("memory").getUInt());
  }

  // partial update - empty delta
  {
    auto const updateJson = arangodb::velocypack::Parser::fromJson("{ }");
    EXPECT_TRUE(view->properties(updateJson->slice(), true).ok());  // empty properties -> should not affect plan version
    EXPECT_TRUE(planVersion == arangodb::tests::getCurrentPlanVersion());  // plan did't change version
  }

  // drop view
  // simulate heartbeat thread (drop index in current)
  {
    auto const value = arangodb::velocypack::Parser::fromJson(
        "{ \"shard-id-does-not-matter\": { \"indexes\" : [ ] } }");
    EXPECT_TRUE(arangodb::AgencyComm()
                    .setValue(currentCollection1Path, value->slice(), 0.0)
                    .successful());
  }
  {
    auto const value = arangodb::velocypack::Parser::fromJson(
        "{ \"shard-id-does-not-matter\": { \"indexes\" : [ ] } }");
    EXPECT_TRUE(arangodb::AgencyComm()
                    .setValue(currentCollection2Path, value->slice(), 0.0)
                    .successful());
  }
  {
    auto const value = arangodb::velocypack::Parser::fromJson(
        "{ \"shard-id-does-not-matter\": { \"indexes\" : [ ] } }");
    EXPECT_TRUE(arangodb::AgencyComm()
                    .setValue(currentCollection3Path, value->slice(), 0.0)
                    .successful());
  }

  EXPECT_TRUE(view->drop().ok());
  EXPECT_TRUE(planVersion < arangodb::tests::getCurrentPlanVersion());

  // there is no more view
  EXPECT_TRUE(nullptr == ci->getView(vocbase->name(), view->name()));

  // there are no more links
  {
    // get new version from plan
    auto updatedCollection =
        ci->getCollection(vocbase->name(), std::to_string(logicalCollection1->id()));
    ASSERT_TRUE(updatedCollection);
    auto link = arangodb::iresearch::IResearchLinkHelper::find(*updatedCollection, *view);
    EXPECT_TRUE(!link);
  }

  {
    // get new version from plan
    auto updatedCollection =
        ci->getCollection(vocbase->name(), std::to_string(logicalCollection2->id()));
    ASSERT_TRUE(updatedCollection);
    auto link = arangodb::iresearch::IResearchLinkHelper::find(*updatedCollection, *view);
    EXPECT_TRUE(!link);
  }

  {
    // get new version from plan
    auto updatedCollection =
        ci->getCollection(vocbase->name(), std::to_string(logicalCollection3->id()));
    ASSERT_TRUE(updatedCollection);
    auto link = arangodb::iresearch::IResearchLinkHelper::find(*updatedCollection, *view);
    EXPECT_TRUE(!link);
  }

  // add link (collection not authorized)
  {
    auto const collectionId = "1";
    logicalCollection1 = ci->getCollection(vocbase->name(), collectionId);
    ASSERT_TRUE((false == !logicalCollection1));
    arangodb::LogicalView::ptr logicalView;
    ASSERT_TRUE(
        (arangodb::LogicalView::create(logicalView, *vocbase, viewJson->slice()).ok()));
    ASSERT_TRUE((false == !logicalView));

    EXPECT_TRUE((true == logicalCollection1->getIndexes().empty()));
    EXPECT_TRUE((true == logicalView->visitCollections(
                             [](TRI_voc_cid_t) -> bool { return false; })));

    struct ExecContext : public arangodb::ExecContext {
      ExecContext()
          : arangodb::ExecContext(arangodb::ExecContext::Type::Default, "", "",
                                  arangodb::auth::Level::NONE,
                                  arangodb::auth::Level::NONE) {}
    } execContext;
    auto* origExecContext = ExecContext::CURRENT;
    auto resetExecContext = irs::make_finally(
        [origExecContext]() -> void { ExecContext::CURRENT = origExecContext; });
    ExecContext::CURRENT = &execContext;
    auto* authFeature = arangodb::AuthenticationFeature::instance();
    auto* userManager = authFeature->userManager();
    arangodb::aql::QueryRegistry queryRegistry(0);  // required for UserManager::removeAllUsers()
    arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
    userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database
    userManager->setQueryRegistry(&queryRegistry);
    auto resetUserManager = irs::make_finally(
        [userManager]() -> void { userManager->removeAllUsers(); });

    EXPECT_TRUE((TRI_ERROR_FORBIDDEN ==
                 logicalView->properties(linksJson->slice(), true).errorNumber()));
    logicalCollection1 = ci->getCollection(vocbase->name(), collectionId);
    ASSERT_TRUE((false == !logicalCollection1));
    logicalView = ci->getView(vocbase->name(), viewId);  // get new version of the view
    ASSERT_TRUE((false == !logicalView));
    EXPECT_TRUE((true == logicalCollection1->getIndexes().empty()));
    EXPECT_TRUE((true == logicalView->visitCollections(
                             [](TRI_voc_cid_t) -> bool { return false; })));
  }
}

TEST_F(IResearchViewCoordinatorTest, test_update_links_replace) {
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

    EXPECT_TRUE((ci->createDatabaseCoordinator(vocbase->name(), VPackSlice::emptyObjectSlice(), 0.0)
                     .ok()));
  }

  // create collections
  std::shared_ptr<arangodb::LogicalCollection> logicalCollection1;
  {
    auto const collectionId = "1";

    auto collectionJson = arangodb::velocypack::Parser::fromJson(
        "{ \"id\": \"1\", \"planId\": \"1\",  \"name\": \"testCollection1\", "
        "\"replicationFactor\": 1, \"type\": 1, \"shards\":{} }");

    EXPECT_TRUE((ci->createCollectionCoordinator(vocbase->name(), collectionId, 0, 1,
                                                 1, false, collectionJson->slice(), 0.0)
                     .ok()));

    logicalCollection1 = ci->getCollection(vocbase->name(), collectionId);
    ASSERT_TRUE(nullptr != logicalCollection1);
  }

  std::shared_ptr<arangodb::LogicalCollection> logicalCollection2;
  {
    auto const collectionId = "2";

    auto collectionJson = arangodb::velocypack::Parser::fromJson(
        "{ \"id\": \"2\", \"planId\": \"2\",  \"name\": \"testCollection2\", "
        "\"replicationFactor\": 1, \"type\": 1, \"shards\":{} }");

    EXPECT_TRUE((ci->createCollectionCoordinator(vocbase->name(), collectionId, 0, 1,
                                                 1, false, collectionJson->slice(), 0.0)
                     .ok()));

    logicalCollection2 = ci->getCollection(vocbase->name(), collectionId);
    ASSERT_TRUE(nullptr != logicalCollection2);
  }

  std::shared_ptr<arangodb::LogicalCollection> logicalCollection3;
  {
    auto const collectionId = "3";

    auto collectionJson = arangodb::velocypack::Parser::fromJson(
        "{ \"id\": \"3\", \"planId\": \"3\",  \"name\": \"testCollection3\", "
        "\"replicationFactor\": 1, \"type\": 1, \"shards\":{} }");

    EXPECT_TRUE((ci->createCollectionCoordinator(vocbase->name(), collectionId, 0, 1,
                                                 1, false, collectionJson->slice(), 0.0)
                     .ok()));

    logicalCollection3 = ci->getCollection(vocbase->name(), collectionId);
    ASSERT_TRUE(nullptr != logicalCollection3);
  }

  auto const currentCollection1Path = "/Current/Collections/" + vocbase->name() +
                                      "/" + std::to_string(logicalCollection1->id());
  auto const currentCollection2Path = "/Current/Collections/" + vocbase->name() +
                                      "/" + std::to_string(logicalCollection2->id());
  auto const currentCollection3Path = "/Current/Collections/" + vocbase->name() +
                                      "/" + std::to_string(logicalCollection3->id());

  // get current plan version
  auto planVersion = arangodb::tests::getCurrentPlanVersion();

  ci->loadCurrent();

  // create view
  auto viewJson = arangodb::velocypack::Parser::fromJson(
      "{ \"name\": \"testView\", \"id\": \"42\", \"type\": \"arangosearch\" "
      "}");
  arangodb::LogicalView::ptr view;
  ASSERT_TRUE((arangodb::LogicalView::create(view, *vocbase, viewJson->slice()).ok()));
  ASSERT_TRUE(view);
  auto const viewId = std::to_string(view->planId());
  EXPECT_TRUE("42" == viewId);

  // simulate heartbeat thread (create index in current)
  {
    auto const value = arangodb::velocypack::Parser::fromJson(
        "{ \"shard-id-does-not-matter\": { \"indexes\" : [ { \"id\": \"1\" } "
        "] } }");
    EXPECT_TRUE(arangodb::AgencyComm()
                    .setValue(currentCollection1Path, value->slice(), 0.0)
                    .successful());
  }
  {
    auto const value = arangodb::velocypack::Parser::fromJson(
        "{ \"shard-id-does-not-matter\": { \"indexes\" : [ { \"id\": \"3\" } "
        "] } }");
    EXPECT_TRUE(arangodb::AgencyComm()
                    .setValue(currentCollection3Path, value->slice(), 0.0)
                    .successful());
  }

  // explicitly specify id for the sake of tests
  auto linksJson = arangodb::velocypack::Parser::fromJson(
      "{ \"links\": {"
      "  \"testCollection1\" : { \"id\": \"1\", \"includeAllFields\" : true "
      "}, "
      "  \"testCollection3\" : { \"id\": \"3\" } "
      "} }");
  EXPECT_TRUE(view->properties(linksJson->slice(), false).ok());  // add link
  EXPECT_TRUE(planVersion < arangodb::tests::getCurrentPlanVersion());  // plan version changed
  planVersion = arangodb::tests::getCurrentPlanVersion();
  auto oldView = view;
  view = ci->getView(vocbase->name(), viewId);  // get new version of the view
  EXPECT_TRUE(view != oldView);                 // different objects
  EXPECT_TRUE(nullptr != view);
  EXPECT_TRUE(nullptr !=
              std::dynamic_pointer_cast<arangodb::iresearch::IResearchViewCoordinator>(view));
  EXPECT_TRUE(planVersion == view->planVersion());
  EXPECT_TRUE("testView" == view->name());
  EXPECT_TRUE(false == view->deleted());
  EXPECT_TRUE(42 == view->id());
  EXPECT_TRUE(arangodb::iresearch::DATA_SOURCE_TYPE == view->type());
  EXPECT_TRUE(arangodb::LogicalView::category() == view->category());
  EXPECT_TRUE(vocbase == &view->vocbase());

  // visit collections
  {
    std::set<TRI_voc_cid_t> expectedLinks{logicalCollection1->id(),
                                          logicalCollection3->id()};
    EXPECT_TRUE(view->visitCollections([&expectedLinks](TRI_voc_cid_t cid) mutable {
      return 1 == expectedLinks.erase(cid);
    }));
    EXPECT_TRUE(expectedLinks.empty());
  }

  // check properties
  {
    VPackBuilder info;
    info.openObject();
    view->properties(info, arangodb::LogicalDataSource::makeFlags(
                               arangodb::LogicalDataSource::Serialize::Detailed));
    info.close();

    auto const properties = info.slice();
    EXPECT_TRUE(properties.hasKey(arangodb::iresearch::StaticStrings::LinksField));
    auto const linksSlice = properties.get(arangodb::iresearch::StaticStrings::LinksField);
    EXPECT_TRUE(linksSlice.isObject());

    VPackObjectIterator it(linksSlice);
    EXPECT_TRUE(it.valid());
    EXPECT_TRUE(2 == it.size());

    // testCollection1
    {
      auto const value = linksSlice.get(logicalCollection1->name());
      EXPECT_TRUE(value.isObject());

      arangodb::iresearch::IResearchLinkMeta expectedMeta;
      expectedMeta._includeAllFields = true;
      arangodb::iresearch::IResearchLinkMeta actualMeta;
      std::string error;
      EXPECT_TRUE(actualMeta.init(value, false, error));
      EXPECT_TRUE(error.empty());
      EXPECT_TRUE(expectedMeta == actualMeta);
    }

    // testCollection3
    {
      auto const value = linksSlice.get(logicalCollection3->name());
      EXPECT_TRUE(value.isObject());

      arangodb::iresearch::IResearchLinkMeta expectedMeta;
      arangodb::iresearch::IResearchLinkMeta actualMeta;
      std::string error;
      EXPECT_TRUE(actualMeta.init(value, false, error));
      EXPECT_TRUE(error.empty());
      EXPECT_TRUE(expectedMeta == actualMeta);
    }
  }

  // test index in testCollection1
  {
    // get new version from plan
    auto updatedCollection =
        ci->getCollection(vocbase->name(), std::to_string(logicalCollection1->id()));
    ASSERT_TRUE(updatedCollection);
    auto link = arangodb::iresearch::IResearchLinkHelper::find(*updatedCollection, *view);
    EXPECT_TRUE(link);

    auto index = std::dynamic_pointer_cast<arangodb::Index>(link);
    ASSERT_TRUE((false == !index));
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

    arangodb::iresearch::IResearchLinkMeta expectedMeta;
    expectedMeta._includeAllFields = true;
    arangodb::iresearch::IResearchLinkMeta actualMeta;
    auto builder = index->toVelocyPack(
        arangodb::Index::makeFlags(arangodb::Index::Serialize::Figures));

    std::string error;
    EXPECT_TRUE(actualMeta.init(builder->slice(), false, error));
    EXPECT_TRUE(error.empty());
    EXPECT_TRUE(expectedMeta == actualMeta);
    auto const slice = builder->slice();
    EXPECT_TRUE(slice.hasKey("view"));
    EXPECT_TRUE(slice.get("view").isString());
    EXPECT_TRUE(view->id() == 42);
    EXPECT_TRUE(view->guid() == slice.get("view").copyString());
    EXPECT_TRUE(slice.hasKey("figures"));
    EXPECT_TRUE(slice.get("figures").isObject());
    EXPECT_TRUE(slice.get("figures").hasKey("memory"));
    EXPECT_TRUE(slice.get("figures").get("memory").isNumber());
    EXPECT_TRUE(0 < slice.get("figures").get("memory").getUInt());
  }

  // test index in testCollection3
  {
    auto updatedCollection =
        ci->getCollection(vocbase->name(), std::to_string(logicalCollection3->id()));
    ASSERT_TRUE(updatedCollection);
    auto link = arangodb::iresearch::IResearchLinkHelper::find(*updatedCollection, *view);
    EXPECT_TRUE(link);

    auto index = std::dynamic_pointer_cast<arangodb::Index>(link);
    ASSERT_TRUE((false == !index));
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

    arangodb::iresearch::IResearchLinkMeta expectedMeta;
    arangodb::iresearch::IResearchLinkMeta actualMeta;
    auto builder = index->toVelocyPack(
        arangodb::Index::makeFlags(arangodb::Index::Serialize::Figures));

    std::string error;
    EXPECT_TRUE(actualMeta.init(builder->slice(), false, error));
    EXPECT_TRUE(error.empty());
    EXPECT_TRUE(expectedMeta == actualMeta);
    auto const slice = builder->slice();
    EXPECT_TRUE(slice.hasKey("view"));
    EXPECT_TRUE(slice.get("view").isString());
    EXPECT_TRUE(view->id() == 42);
    EXPECT_TRUE(view->guid() == slice.get("view").copyString());
    EXPECT_TRUE(slice.hasKey("figures"));
    EXPECT_TRUE(slice.get("figures").isObject());
    EXPECT_TRUE(slice.get("figures").hasKey("memory"));
    EXPECT_TRUE(slice.get("figures").get("memory").isNumber());
    EXPECT_TRUE(0 < slice.get("figures").get("memory").getUInt());
  }

  EXPECT_TRUE(view->properties(linksJson->slice(), false).ok());  // same properties -> should not affect plan version
  EXPECT_TRUE(planVersion == arangodb::tests::getCurrentPlanVersion());  // plan did't change version

  EXPECT_TRUE(view->properties(linksJson->slice(), true).ok());  // same properties -> should not affect plan version
  EXPECT_TRUE(planVersion == arangodb::tests::getCurrentPlanVersion());  // plan did't change version

  // replace links with testCollection2 link
  // simulate heartbeat thread (create index in current)
  {
    auto const value = arangodb::velocypack::Parser::fromJson(
        "{ \"shard-id-does-not-matter\": { \"indexes\" : [ ] } }");
    EXPECT_TRUE(arangodb::AgencyComm()
                    .setValue(currentCollection1Path, value->slice(), 0.0)
                    .successful());
  }
  {
    auto const value = arangodb::velocypack::Parser::fromJson(
        "{ \"shard-id-does-not-matter\": { \"indexes\" : [ { \"id\": \"2\" } "
        "] } }");
    EXPECT_TRUE(arangodb::AgencyComm()
                    .setValue(currentCollection2Path, value->slice(), 0.0)
                    .successful());
  }
  {
    auto const value = arangodb::velocypack::Parser::fromJson(
        "{ \"shard-id-does-not-matter\": { \"indexes\" : [ ] } }");
    EXPECT_TRUE(arangodb::AgencyComm()
                    .setValue(currentCollection3Path, value->slice(), 0.0)
                    .successful());
  }

  auto updateJson = arangodb::velocypack::Parser::fromJson(
      "{ \"links\": {"
      "  \"2\" : { \"id\": \"2\", \"trackListPositions\" : true } "
      "} }");
  EXPECT_TRUE(view->properties(updateJson->slice(), false).ok());
  EXPECT_TRUE(planVersion < arangodb::tests::getCurrentPlanVersion());  // plan version changed
  planVersion = arangodb::tests::getCurrentPlanVersion();
  oldView = view;
  view = ci->getView(vocbase->name(), viewId);  // get new version of the view
  EXPECT_TRUE(view != oldView);                 // different objects
  EXPECT_TRUE(nullptr != view);
  EXPECT_TRUE(nullptr !=
              std::dynamic_pointer_cast<arangodb::iresearch::IResearchViewCoordinator>(view));
  EXPECT_TRUE(planVersion == view->planVersion());
  EXPECT_TRUE("testView" == view->name());
  EXPECT_TRUE(false == view->deleted());
  EXPECT_TRUE(42 == view->id());
  EXPECT_TRUE(arangodb::iresearch::DATA_SOURCE_TYPE == view->type());
  EXPECT_TRUE(arangodb::LogicalView::category() == view->category());
  EXPECT_TRUE(vocbase == &view->vocbase());

  // visit collections
  {
    std::set<TRI_voc_cid_t> expectedLinks{logicalCollection2->id()};
    EXPECT_TRUE(view->visitCollections([&expectedLinks](TRI_voc_cid_t cid) mutable {
      return 1 == expectedLinks.erase(cid);
    }));
    EXPECT_TRUE(expectedLinks.empty());
  }

  // check properties
  {
    VPackBuilder info;
    info.openObject();
    view->properties(info, arangodb::LogicalDataSource::makeFlags(
                               arangodb::LogicalDataSource::Serialize::Detailed));
    info.close();

    auto const properties = info.slice();
    EXPECT_TRUE(properties.hasKey(arangodb::iresearch::StaticStrings::LinksField));
    auto const linksSlice = properties.get(arangodb::iresearch::StaticStrings::LinksField);
    EXPECT_TRUE(linksSlice.isObject());

    VPackObjectIterator it(linksSlice);
    EXPECT_TRUE(it.valid());
    EXPECT_TRUE(1 == it.size());

    // testCollection2
    {
      auto const value = linksSlice.get(logicalCollection2->name());
      EXPECT_TRUE(value.isObject());

      arangodb::iresearch::IResearchLinkMeta expectedMeta;
      expectedMeta._trackListPositions = true;
      arangodb::iresearch::IResearchLinkMeta actualMeta;
      std::string error;
      EXPECT_TRUE(actualMeta.init(value, false, error));
      EXPECT_TRUE(error.empty());
      EXPECT_TRUE(expectedMeta == actualMeta);
    }
  }

  // test index in testCollection2
  {
    // get new version from plan
    auto updatedCollection =
        ci->getCollection(vocbase->name(), std::to_string(logicalCollection2->id()));
    ASSERT_TRUE(updatedCollection);
    auto link = arangodb::iresearch::IResearchLinkHelper::find(*updatedCollection, *view);
    EXPECT_TRUE(link);

    auto index = std::dynamic_pointer_cast<arangodb::Index>(link);
    ASSERT_TRUE((false == !index));
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

    arangodb::iresearch::IResearchLinkMeta expectedMeta;
    expectedMeta._trackListPositions = true;
    arangodb::iresearch::IResearchLinkMeta actualMeta;
    auto builder = index->toVelocyPack(
        arangodb::Index::makeFlags(arangodb::Index::Serialize::Figures));

    std::string error;
    EXPECT_TRUE(actualMeta.init(builder->slice(), false, error));
    EXPECT_TRUE(error.empty());
    EXPECT_TRUE(expectedMeta == actualMeta);
    auto const slice = builder->slice();
    EXPECT_TRUE(slice.hasKey("view"));
    EXPECT_TRUE(slice.get("view").isString());
    EXPECT_TRUE(view->id() == 42);
    EXPECT_TRUE(view->guid() == slice.get("view").copyString());
    EXPECT_TRUE(slice.hasKey("figures"));
    EXPECT_TRUE(slice.get("figures").isObject());
    EXPECT_TRUE(slice.get("figures").hasKey("memory"));
    EXPECT_TRUE(slice.get("figures").get("memory").isNumber());
    EXPECT_TRUE(0 < slice.get("figures").get("memory").getUInt());
  }

  // replace links with testCollection1 link
  // simulate heartbeat thread (create index in current)
  {
    auto const value = arangodb::velocypack::Parser::fromJson(
        "{ \"shard-id-does-not-matter\": { \"indexes\" : [ { \"id\" : \"1\" "
        "} ] } }");
    EXPECT_TRUE(arangodb::AgencyComm()
                    .setValue(currentCollection1Path, value->slice(), 0.0)
                    .successful());
  }
  {
    auto const value = arangodb::velocypack::Parser::fromJson(
        "{ \"shard-id-does-not-matter\": { \"indexes\" : [ ] } }");
    EXPECT_TRUE(arangodb::AgencyComm()
                    .setValue(currentCollection2Path, value->slice(), 0.0)
                    .successful());
  }

  updateJson = arangodb::velocypack::Parser::fromJson(
      "{ \"links\": {"
      "  \"testCollection1\" : { \"id\": \"1\", \"includeAllFields\" : true "
      "}, "
      "  \"2\" : null "
      "} }");
  EXPECT_TRUE(view->properties(updateJson->slice(), false).ok());
  EXPECT_TRUE(planVersion < arangodb::tests::getCurrentPlanVersion());  // plan version changed
  planVersion = arangodb::tests::getCurrentPlanVersion();
  oldView = view;
  view = ci->getView(vocbase->name(), viewId);  // get new version of the view
  EXPECT_TRUE(view != oldView);                 // different objects
  EXPECT_TRUE(nullptr != view);
  EXPECT_TRUE(nullptr !=
              std::dynamic_pointer_cast<arangodb::iresearch::IResearchViewCoordinator>(view));
  EXPECT_TRUE(planVersion == view->planVersion());
  EXPECT_TRUE("testView" == view->name());
  EXPECT_TRUE(false == view->deleted());
  EXPECT_TRUE(42 == view->id());
  EXPECT_TRUE(arangodb::iresearch::DATA_SOURCE_TYPE == view->type());
  EXPECT_TRUE(arangodb::LogicalView::category() == view->category());
  EXPECT_TRUE(vocbase == &view->vocbase());

  // visit collections
  {
    std::set<TRI_voc_cid_t> expectedLinks{logicalCollection1->id()};
    EXPECT_TRUE(view->visitCollections([&expectedLinks](TRI_voc_cid_t cid) mutable {
      return 1 == expectedLinks.erase(cid);
    }));
    EXPECT_TRUE(expectedLinks.empty());
  }

  // check properties
  {
    VPackBuilder info;
    info.openObject();
    view->properties(info, arangodb::LogicalDataSource::makeFlags(
                               arangodb::LogicalDataSource::Serialize::Detailed));
    info.close();

    auto const properties = info.slice();
    EXPECT_TRUE(properties.hasKey(arangodb::iresearch::StaticStrings::LinksField));
    auto const linksSlice = properties.get(arangodb::iresearch::StaticStrings::LinksField);
    EXPECT_TRUE(linksSlice.isObject());

    VPackObjectIterator it(linksSlice);
    EXPECT_TRUE(it.valid());
    EXPECT_TRUE(1 == it.size());

    // testCollection2
    {
      auto const value = linksSlice.get(logicalCollection1->name());
      EXPECT_TRUE(value.isObject());

      arangodb::iresearch::IResearchLinkMeta expectedMeta;
      expectedMeta._includeAllFields = true;
      arangodb::iresearch::IResearchLinkMeta actualMeta;
      std::string error;
      EXPECT_TRUE(actualMeta.init(value, false, error));
      EXPECT_TRUE(error.empty());
      EXPECT_TRUE(expectedMeta == actualMeta);
    }
  }

  // test index in testCollection1
  {
    // get new version from plan
    auto updatedCollection =
        ci->getCollection(vocbase->name(), std::to_string(logicalCollection1->id()));
    ASSERT_TRUE(updatedCollection);
    auto link = arangodb::iresearch::IResearchLinkHelper::find(*updatedCollection, *view);
    EXPECT_TRUE(link);

    auto index = std::dynamic_pointer_cast<arangodb::Index>(link);
    ASSERT_TRUE((false == !index));
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

    arangodb::iresearch::IResearchLinkMeta expectedMeta;
    expectedMeta._includeAllFields = true;
    arangodb::iresearch::IResearchLinkMeta actualMeta;
    auto builder = index->toVelocyPack(
        arangodb::Index::makeFlags(arangodb::Index::Serialize::Figures));

    std::string error;
    EXPECT_TRUE(actualMeta.init(builder->slice(), false, error));
    EXPECT_TRUE(error.empty());
    EXPECT_TRUE(expectedMeta == actualMeta);
    auto const slice = builder->slice();
    EXPECT_TRUE(slice.hasKey("view"));
    EXPECT_TRUE(slice.get("view").isString());
    EXPECT_TRUE(view->id() == 42);
    EXPECT_TRUE(view->guid() == slice.get("view").copyString());
    EXPECT_TRUE(slice.hasKey("figures"));
    EXPECT_TRUE(slice.get("figures").isObject());
    EXPECT_TRUE(slice.get("figures").hasKey("memory"));
    EXPECT_TRUE(slice.get("figures").get("memory").isNumber());
    EXPECT_TRUE(0 < slice.get("figures").get("memory").getUInt());
  }

  // drop view
  // simulate heartbeat thread (drop index in current)
  {
    auto const value = arangodb::velocypack::Parser::fromJson(
        "{ \"shard-id-does-not-matter\": { \"indexes\" : [ ] } }");
    EXPECT_TRUE(arangodb::AgencyComm()
                    .setValue(currentCollection1Path, value->slice(), 0.0)
                    .successful());
  }

  EXPECT_TRUE(view->drop().ok());
  EXPECT_TRUE(planVersion < arangodb::tests::getCurrentPlanVersion());

  // there is no more view
  EXPECT_TRUE(nullptr == ci->getView(vocbase->name(), view->name()));

  // there are no more links
  {
    // get new version from plan
    auto updatedCollection =
        ci->getCollection(vocbase->name(), std::to_string(logicalCollection1->id()));
    ASSERT_TRUE(updatedCollection);
    auto link = arangodb::iresearch::IResearchLinkHelper::find(*updatedCollection, *view);
    EXPECT_TRUE(!link);
  }

  {
    // get new version from plan
    auto updatedCollection =
        ci->getCollection(vocbase->name(), std::to_string(logicalCollection2->id()));
    ASSERT_TRUE(updatedCollection);
    auto link = arangodb::iresearch::IResearchLinkHelper::find(*updatedCollection, *view);
    EXPECT_TRUE(!link);
  }

  {
    // get new version from plan
    auto updatedCollection =
        ci->getCollection(vocbase->name(), std::to_string(logicalCollection3->id()));
    ASSERT_TRUE(updatedCollection);
    auto link = arangodb::iresearch::IResearchLinkHelper::find(*updatedCollection, *view);
    EXPECT_TRUE(!link);
  }
}

TEST_F(IResearchViewCoordinatorTest, test_update_links_clear) {
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

    EXPECT_TRUE((ci->createDatabaseCoordinator(vocbase->name(), VPackSlice::emptyObjectSlice(), 0.0)
                     .ok()));
  }

  // create collections
  std::shared_ptr<arangodb::LogicalCollection> logicalCollection1;
  {
    auto const collectionId = "1";

    auto collectionJson = arangodb::velocypack::Parser::fromJson(
        "{ \"id\": \"1\", \"planId\": \"1\",  \"name\": \"testCollection1\", "
        "\"replicationFactor\": 1, \"type\": 1, \"shards\":{} }");

    EXPECT_TRUE((ci->createCollectionCoordinator(vocbase->name(), collectionId, 0, 1,
                                                 1, false, collectionJson->slice(), 0.0)
                     .ok()));

    logicalCollection1 = ci->getCollection(vocbase->name(), collectionId);
    ASSERT_TRUE(nullptr != logicalCollection1);
  }

  std::shared_ptr<arangodb::LogicalCollection> logicalCollection2;
  {
    auto const collectionId = "2";

    auto collectionJson = arangodb::velocypack::Parser::fromJson(
        "{ \"id\": \"2\", \"planId\": \"2\",  \"name\": \"testCollection2\", "
        "\"replicationFactor\": 1, \"type\": 1, \"shards\":{} }");

    EXPECT_TRUE((ci->createCollectionCoordinator(vocbase->name(), collectionId, 0, 1,
                                                 1, false, collectionJson->slice(), 0.0)
                     .ok()));

    logicalCollection2 = ci->getCollection(vocbase->name(), collectionId);
    ASSERT_TRUE(nullptr != logicalCollection2);
  }

  std::shared_ptr<arangodb::LogicalCollection> logicalCollection3;
  {
    auto const collectionId = "3";

    auto collectionJson = arangodb::velocypack::Parser::fromJson(
        "{ \"id\": \"3\", \"planId\": \"3\",  \"name\": \"testCollection3\", "
        "\"replicationFactor\": 1, \"type\": 1, \"shards\":{} }");

    EXPECT_TRUE((ci->createCollectionCoordinator(vocbase->name(), collectionId, 0, 1,
                                                 1, false, collectionJson->slice(), 0.0)
                     .ok()));

    logicalCollection3 = ci->getCollection(vocbase->name(), collectionId);
    ASSERT_TRUE(nullptr != logicalCollection3);
  }

  auto const currentCollection1Path = "/Current/Collections/" + vocbase->name() +
                                      "/" + std::to_string(logicalCollection1->id());
  auto const currentCollection2Path = "/Current/Collections/" + vocbase->name() +
                                      "/" + std::to_string(logicalCollection2->id());
  auto const currentCollection3Path = "/Current/Collections/" + vocbase->name() +
                                      "/" + std::to_string(logicalCollection3->id());

  // get current plan version
  auto planVersion = arangodb::tests::getCurrentPlanVersion();

  ci->loadCurrent();

  // create view
  auto viewJson = arangodb::velocypack::Parser::fromJson(
      "{ \"name\": \"testView\", \"id\": \"42\", \"type\": \"arangosearch\" "
      "}");
  arangodb::LogicalView::ptr view;
  ASSERT_TRUE((arangodb::LogicalView::create(view, *vocbase, viewJson->slice()).ok()));
  ASSERT_TRUE(view);
  auto const viewId = std::to_string(view->planId());
  EXPECT_TRUE("42" == viewId);

  // simulate heartbeat thread (create index in current)
  {
    auto const value = arangodb::velocypack::Parser::fromJson(
        "{ \"shard-id-does-not-matter\": { \"indexes\" : [ { \"id\": \"1\" } "
        "] } }");
    EXPECT_TRUE(arangodb::AgencyComm()
                    .setValue(currentCollection1Path, value->slice(), 0.0)
                    .successful());
  }
  {
    auto const value = arangodb::velocypack::Parser::fromJson(
        "{ \"shard-id-does-not-matter\": { \"indexes\" : [ { \"id\": \"2\" } "
        "] } }");
    EXPECT_TRUE(arangodb::AgencyComm()
                    .setValue(currentCollection2Path, value->slice(), 0.0)
                    .successful());
  }
  {
    auto const value = arangodb::velocypack::Parser::fromJson(
        "{ \"shard-id-does-not-matter\": { \"indexes\" : [ { \"id\": \"3\" } "
        "] } }");
    EXPECT_TRUE(arangodb::AgencyComm()
                    .setValue(currentCollection3Path, value->slice(), 0.0)
                    .successful());
  }

  // explicitly specify id for the sake of tests
  auto linksJson = arangodb::velocypack::Parser::fromJson(
      "{ \"locale\": \"en\", \"links\": {"
      "  \"testCollection1\" : { \"id\": \"1\", \"includeAllFields\" : true "
      "}, "
      "  \"2\" : { \"id\": \"2\", \"trackListPositions\" : true }, "
      "  \"testCollection3\" : { \"id\": \"3\" } "
      "} }");
  EXPECT_TRUE(view->properties(linksJson->slice(), false).ok());  // add link
  EXPECT_TRUE(planVersion < arangodb::tests::getCurrentPlanVersion());  // plan version changed
  planVersion = arangodb::tests::getCurrentPlanVersion();
  auto oldView = view;
  view = ci->getView(vocbase->name(), viewId);  // get new version of the view
  EXPECT_TRUE(view != oldView);                 // different objects
  EXPECT_TRUE(nullptr != view);
  EXPECT_TRUE(nullptr !=
              std::dynamic_pointer_cast<arangodb::iresearch::IResearchViewCoordinator>(view));
  EXPECT_TRUE(planVersion == view->planVersion());
  EXPECT_TRUE("testView" == view->name());
  EXPECT_TRUE(false == view->deleted());
  EXPECT_TRUE(42 == view->id());
  EXPECT_TRUE(arangodb::iresearch::DATA_SOURCE_TYPE == view->type());
  EXPECT_TRUE(arangodb::LogicalView::category() == view->category());
  EXPECT_TRUE(vocbase == &view->vocbase());

  // visit collections
  {
    std::set<TRI_voc_cid_t> expectedLinks{logicalCollection1->id(),
                                          logicalCollection2->id(),
                                          logicalCollection3->id()};
    EXPECT_TRUE(view->visitCollections([&expectedLinks](TRI_voc_cid_t cid) mutable {
      return 1 == expectedLinks.erase(cid);
    }));
    EXPECT_TRUE(expectedLinks.empty());
  }

  // check properties
  {
    VPackBuilder info;
    info.openObject();
    view->properties(info, arangodb::LogicalDataSource::makeFlags(
                               arangodb::LogicalDataSource::Serialize::Detailed));
    info.close();

    auto const properties = info.slice();
    EXPECT_TRUE(properties.hasKey(arangodb::iresearch::StaticStrings::LinksField));
    auto const linksSlice = properties.get(arangodb::iresearch::StaticStrings::LinksField);
    EXPECT_TRUE(linksSlice.isObject());

    VPackObjectIterator it(linksSlice);
    EXPECT_TRUE(it.valid());
    EXPECT_TRUE(3 == it.size());

    // testCollection1
    {
      auto const value = linksSlice.get(logicalCollection1->name());
      EXPECT_TRUE(value.isObject());

      arangodb::iresearch::IResearchLinkMeta expectedMeta;
      expectedMeta._includeAllFields = true;
      arangodb::iresearch::IResearchLinkMeta actualMeta;
      std::string error;
      EXPECT_TRUE(actualMeta.init(value, false, error));
      EXPECT_TRUE(error.empty());
      EXPECT_TRUE(expectedMeta == actualMeta);
    }

    // testCollection2
    {
      auto const value = linksSlice.get(logicalCollection2->name());
      EXPECT_TRUE(value.isObject());

      arangodb::iresearch::IResearchLinkMeta expectedMeta;
      expectedMeta._trackListPositions = true;
      arangodb::iresearch::IResearchLinkMeta actualMeta;
      std::string error;
      EXPECT_TRUE(actualMeta.init(value, false, error));
      EXPECT_TRUE(error.empty());
      EXPECT_TRUE(expectedMeta == actualMeta);
    }

    // testCollection3
    {
      auto const value = linksSlice.get(logicalCollection3->name());
      EXPECT_TRUE(value.isObject());

      arangodb::iresearch::IResearchLinkMeta expectedMeta;
      arangodb::iresearch::IResearchLinkMeta actualMeta;
      std::string error;
      EXPECT_TRUE(actualMeta.init(value, false, error));
      EXPECT_TRUE(error.empty());
      EXPECT_TRUE(expectedMeta == actualMeta);
    }
  }

  // test index in testCollection1
  {
    // get new version from plan
    auto updatedCollection =
        ci->getCollection(vocbase->name(), std::to_string(logicalCollection1->id()));
    ASSERT_TRUE(updatedCollection);
    auto link = arangodb::iresearch::IResearchLinkHelper::find(*updatedCollection, *view);
    EXPECT_TRUE(link);

    auto index = std::dynamic_pointer_cast<arangodb::Index>(link);
    ASSERT_TRUE((false == !index));
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

    arangodb::iresearch::IResearchLinkMeta expectedMeta;
    expectedMeta._includeAllFields = true;
    arangodb::iresearch::IResearchLinkMeta actualMeta;
    auto builder = index->toVelocyPack(
        arangodb::Index::makeFlags(arangodb::Index::Serialize::Figures));

    std::string error;
    EXPECT_TRUE(actualMeta.init(builder->slice(), false, error));
    EXPECT_TRUE(error.empty());
    EXPECT_TRUE(expectedMeta == actualMeta);
    auto const slice = builder->slice();
    EXPECT_TRUE(slice.hasKey("view"));
    EXPECT_TRUE(slice.get("view").isString());
    EXPECT_TRUE(view->id() == 42);
    EXPECT_TRUE(view->guid() == slice.get("view").copyString());
    EXPECT_TRUE(slice.hasKey("figures"));
    EXPECT_TRUE(slice.get("figures").isObject());
    EXPECT_TRUE(slice.get("figures").hasKey("memory"));
    EXPECT_TRUE(slice.get("figures").get("memory").isNumber());
    EXPECT_TRUE(0 < slice.get("figures").get("memory").getUInt());
  }

  // test index in testCollection2
  {
    // get new version from plan
    auto updatedCollection =
        ci->getCollection(vocbase->name(), std::to_string(logicalCollection2->id()));
    ASSERT_TRUE(updatedCollection);
    auto link = arangodb::iresearch::IResearchLinkHelper::find(*updatedCollection, *view);
    EXPECT_TRUE(link);

    auto index = std::dynamic_pointer_cast<arangodb::Index>(link);
    ASSERT_TRUE((false == !index));
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

    arangodb::iresearch::IResearchLinkMeta expectedMeta;
    expectedMeta._trackListPositions = true;
    arangodb::iresearch::IResearchLinkMeta actualMeta;
    auto builder = index->toVelocyPack(
        arangodb::Index::makeFlags(arangodb::Index::Serialize::Figures));

    std::string error;
    EXPECT_TRUE(actualMeta.init(builder->slice(), false, error));
    EXPECT_TRUE(error.empty());
    EXPECT_TRUE(expectedMeta == actualMeta);
    auto const slice = builder->slice();
    EXPECT_TRUE(slice.hasKey("view"));
    EXPECT_TRUE(slice.get("view").isString());
    EXPECT_TRUE(view->id() == 42);
    EXPECT_TRUE(view->guid() == slice.get("view").copyString());
    EXPECT_TRUE(slice.hasKey("figures"));
    EXPECT_TRUE(slice.get("figures").isObject());
    EXPECT_TRUE(slice.get("figures").hasKey("memory"));
    EXPECT_TRUE(slice.get("figures").get("memory").isNumber());
    EXPECT_TRUE(0 < slice.get("figures").get("memory").getUInt());
  }

  // test index in testCollection3
  {
    auto updatedCollection =
        ci->getCollection(vocbase->name(), std::to_string(logicalCollection3->id()));
    ASSERT_TRUE(updatedCollection);
    auto link = arangodb::iresearch::IResearchLinkHelper::find(*updatedCollection, *view);
    EXPECT_TRUE(link);

    auto index = std::dynamic_pointer_cast<arangodb::Index>(link);
    ASSERT_TRUE((false == !index));
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

    arangodb::iresearch::IResearchLinkMeta expectedMeta;
    arangodb::iresearch::IResearchLinkMeta actualMeta;
    auto builder = index->toVelocyPack(
        arangodb::Index::makeFlags(arangodb::Index::Serialize::Figures));

    std::string error;
    EXPECT_TRUE(actualMeta.init(builder->slice(), false, error));
    EXPECT_TRUE(error.empty());
    EXPECT_TRUE(expectedMeta == actualMeta);
    auto const slice = builder->slice();
    EXPECT_TRUE(slice.hasKey("view"));
    EXPECT_TRUE(slice.get("view").isString());
    EXPECT_TRUE(view->id() == 42);
    EXPECT_TRUE(view->guid() == slice.get("view").copyString());
    EXPECT_TRUE(slice.hasKey("figures"));
    EXPECT_TRUE(slice.get("figures").isObject());
    EXPECT_TRUE(slice.get("figures").hasKey("memory"));
    EXPECT_TRUE(slice.get("figures").get("memory").isNumber());
    EXPECT_TRUE(0 < slice.get("figures").get("memory").getUInt());
  }

  EXPECT_TRUE(view->properties(linksJson->slice(), false).ok());  // same properties -> should not affect plan version
  EXPECT_TRUE(planVersion == arangodb::tests::getCurrentPlanVersion());  // plan did't change version

  EXPECT_TRUE(view->properties(linksJson->slice(), true).ok());  // same properties -> should not affect plan version
  EXPECT_TRUE(planVersion == arangodb::tests::getCurrentPlanVersion());  // plan did't change version

  // remove all links
  // simulate heartbeat thread (create index in current)
  {
    auto const value = arangodb::velocypack::Parser::fromJson(
        "{ \"shard-id-does-not-matter\": { \"indexes\" : [ ] } }");
    EXPECT_TRUE(arangodb::AgencyComm()
                    .setValue(currentCollection1Path, value->slice(), 0.0)
                    .successful());
  }
  {
    auto const value = arangodb::velocypack::Parser::fromJson(
        "{ \"shard-id-does-not-matter\": { \"indexes\" : [ ] } }");
    EXPECT_TRUE(arangodb::AgencyComm()
                    .setValue(currentCollection2Path, value->slice(), 0.0)
                    .successful());
  }
  {
    auto const value = arangodb::velocypack::Parser::fromJson(
        "{ \"shard-id-does-not-matter\": { \"indexes\" : [ ] } }");
    EXPECT_TRUE(arangodb::AgencyComm()
                    .setValue(currentCollection3Path, value->slice(), 0.0)
                    .successful());
  }

  auto const updateJson =
      arangodb::velocypack::Parser::fromJson("{ \"links\": {} }");
  EXPECT_TRUE(view->properties(updateJson->slice(), false).ok());
  EXPECT_TRUE(planVersion < arangodb::tests::getCurrentPlanVersion());  // plan version changed
  planVersion = arangodb::tests::getCurrentPlanVersion();
  oldView = view;
  view = ci->getView(vocbase->name(), viewId);  // get new version of the view
  EXPECT_TRUE(view != oldView);                 // different objects
  EXPECT_TRUE(nullptr != view);
  EXPECT_TRUE(nullptr !=
              std::dynamic_pointer_cast<arangodb::iresearch::IResearchViewCoordinator>(view));
  EXPECT_TRUE(planVersion == view->planVersion());
  EXPECT_TRUE("testView" == view->name());
  EXPECT_TRUE(false == view->deleted());
  EXPECT_TRUE(42 == view->id());
  EXPECT_TRUE(arangodb::iresearch::DATA_SOURCE_TYPE == view->type());
  EXPECT_TRUE(arangodb::LogicalView::category() == view->category());
  EXPECT_TRUE(vocbase == &view->vocbase());

  // visit collections
  {
    EXPECT_TRUE(
        view->visitCollections([](TRI_voc_cid_t cid) mutable { return false; }));
  }

  // check properties
  {
    VPackBuilder info;
    info.openObject();
    view->properties(info, arangodb::LogicalDataSource::makeFlags(
                               arangodb::LogicalDataSource::Serialize::Detailed));
    info.close();

    auto const properties = info.slice();
    EXPECT_TRUE((properties.hasKey(arangodb::iresearch::StaticStrings::LinksField)));
    auto links = properties.get(arangodb::iresearch::StaticStrings::LinksField);
    EXPECT_TRUE((links.isObject()));
    EXPECT_TRUE((0 == links.length()));
  }

  // test index in testCollection1
  {
    // get new version from plan
    auto updatedCollection =
        ci->getCollection(vocbase->name(), std::to_string(logicalCollection1->id()));
    ASSERT_TRUE(updatedCollection);
    auto link = arangodb::iresearch::IResearchLinkHelper::find(*updatedCollection, *view);
    EXPECT_TRUE(!link);
  }

  // test index in testCollection2
  {
    // get new version from plan
    auto updatedCollection =
        ci->getCollection(vocbase->name(), std::to_string(logicalCollection2->id()));
    ASSERT_TRUE(updatedCollection);
    auto link = arangodb::iresearch::IResearchLinkHelper::find(*updatedCollection, *view);
    EXPECT_TRUE(!link);
  }

  // test index in testCollection3
  {
    // get new version from plan
    auto updatedCollection =
        ci->getCollection(vocbase->name(), std::to_string(logicalCollection3->id()));
    ASSERT_TRUE(updatedCollection);
    auto link = arangodb::iresearch::IResearchLinkHelper::find(*updatedCollection, *view);
    EXPECT_TRUE(!link);
  }

  // drop view
  EXPECT_TRUE(view->drop().ok());
  EXPECT_TRUE(planVersion < arangodb::tests::getCurrentPlanVersion());

  // there is no more view
  EXPECT_TRUE(nullptr == ci->getView(vocbase->name(), view->name()));
}

TEST_F(IResearchViewCoordinatorTest, test_drop_link) {
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

    EXPECT_TRUE((ci->createDatabaseCoordinator(vocbase->name(), VPackSlice::emptyObjectSlice(), 0.0)
                     .ok()));
  }

  // create collection
  std::shared_ptr<arangodb::LogicalCollection> logicalCollection;
  {
    auto const collectionId = "1";

    auto collectionJson = arangodb::velocypack::Parser::fromJson(
        "{ \"id\": \"1\", \"planId\": \"1\",  \"name\": \"testCollection\", "
        "\"replicationFactor\": 1, \"type\": 1, \"shards\":{} }");

    EXPECT_TRUE((ci->createCollectionCoordinator(vocbase->name(), collectionId, 0, 1,
                                                 1, false, collectionJson->slice(), 0.0)
                     .ok()));

    logicalCollection = ci->getCollection(vocbase->name(), collectionId);
    ASSERT_TRUE((nullptr != logicalCollection));
  }

  ci->loadCurrent();

  auto const currentCollectionPath = "/Current/Collections/" + vocbase->name() +
                                     "/" + std::to_string(logicalCollection->id());

  // get current plan version
  auto planVersion = arangodb::tests::getCurrentPlanVersion();

  // update link
  {
    auto viewJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testView\", \"id\": \"42\", \"type\": "
        "\"arangosearch\" }");
    arangodb::LogicalView::ptr logicalView;
    ASSERT_TRUE(
        (arangodb::LogicalView::create(logicalView, *vocbase, viewJson->slice()).ok()));
    auto view =
        std::dynamic_pointer_cast<arangodb::iresearch::IResearchViewCoordinator>(logicalView);
    ASSERT_TRUE(view);
    auto const viewId = std::to_string(view->planId());
    EXPECT_TRUE("42" == viewId);

    // simulate heartbeat thread (create index in current)
    {
      auto const value = arangodb::velocypack::Parser::fromJson(
          "{ \"shard-id-does-not-matter\": { \"indexes\" : [ { \"id\": \"1\" "
          "} ] } }");
      EXPECT_TRUE(arangodb::AgencyComm()
                      .setValue(currentCollectionPath, value->slice(), 0.0)
                      .successful());
    }

    auto linksJson = arangodb::velocypack::Parser::fromJson(
        "{ \"links\": { \"testCollection\" : { \"includeAllFields\" : true } "
        "} }");
    EXPECT_TRUE(view->properties(linksJson->slice(), true).ok());  // add link
    EXPECT_TRUE(planVersion < arangodb::tests::getCurrentPlanVersion());  // plan version changed
    planVersion = arangodb::tests::getCurrentPlanVersion();

    auto oldView = view;
    view = std::dynamic_pointer_cast<arangodb::iresearch::IResearchViewCoordinator>(
        ci->getView(vocbase->name(), viewId));  // get new version of the view
    EXPECT_TRUE(view != oldView);               // different objects
    EXPECT_TRUE(nullptr != view);
    EXPECT_TRUE(nullptr !=
                std::dynamic_pointer_cast<arangodb::iresearch::IResearchViewCoordinator>(view));
    EXPECT_TRUE(planVersion == view->planVersion());
    EXPECT_TRUE("testView" == view->name());
    EXPECT_TRUE(false == view->deleted());
    EXPECT_TRUE(42 == view->id());
    EXPECT_TRUE(arangodb::iresearch::DATA_SOURCE_TYPE == view->type());
    EXPECT_TRUE(arangodb::LogicalView::category() == view->category());
    EXPECT_TRUE(vocbase == &view->vocbase());

    // visit collections
    {
      std::set<TRI_voc_cid_t> expectedLinks{logicalCollection->id()};
      EXPECT_TRUE(view->visitCollections([&expectedLinks](TRI_voc_cid_t cid) mutable {
        return 1 == expectedLinks.erase(cid);
      }));
      EXPECT_TRUE(expectedLinks.empty());
    }

    // check properties
    {
      VPackBuilder info;
      info.openObject();
      view->properties(info, arangodb::LogicalDataSource::makeFlags(
                                 arangodb::LogicalDataSource::Serialize::Detailed));
      info.close();

      auto const properties = info.slice();
      EXPECT_TRUE(properties.hasKey(arangodb::iresearch::StaticStrings::LinksField));
      auto const linksSlice = properties.get(arangodb::iresearch::StaticStrings::LinksField);
      EXPECT_TRUE(linksSlice.isObject());

      VPackObjectIterator it(linksSlice);
      EXPECT_TRUE(it.valid());
      EXPECT_TRUE(1 == it.size());
      auto const& valuePair = *it;
      auto const key = valuePair.key;
      EXPECT_TRUE(key.isString());
      EXPECT_TRUE("testCollection" == key.copyString());
      auto const value = valuePair.value;
      EXPECT_TRUE(value.isObject());

      arangodb::iresearch::IResearchLinkMeta expectedMeta;
      expectedMeta._includeAllFields = true;
      arangodb::iresearch::IResearchLinkMeta actualMeta;
      std::string error;
      EXPECT_TRUE(actualMeta.init(value, false, error));
      EXPECT_TRUE(error.empty());
      EXPECT_TRUE(expectedMeta == actualMeta);
    }

    TRI_idx_iid_t linkId;

    // check indexes
    {
      // get new version from plan
      auto updatedCollection =
          ci->getCollection(vocbase->name(), std::to_string(logicalCollection->id()));
      ASSERT_TRUE(updatedCollection);
      auto link = arangodb::iresearch::IResearchLinkHelper::find(*updatedCollection, *view);
      ASSERT_TRUE((link));
      linkId = link->id();

      auto index = std::dynamic_pointer_cast<arangodb::Index>(link);
      ASSERT_TRUE((false == !index));
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
      EXPECT_TRUE((arangodb::Index::IndexType::TRI_IDX_TYPE_IRESEARCH_LINK ==
                   index->type()));
      EXPECT_TRUE((arangodb::iresearch::DATA_SOURCE_TYPE.name() == index->typeName()));
      EXPECT_TRUE((false == index->unique()));

      arangodb::iresearch::IResearchLinkMeta expectedMeta;
      expectedMeta._includeAllFields = true;
      arangodb::iresearch::IResearchLinkMeta actualMeta;
      auto builder = index->toVelocyPack(
          arangodb::Index::makeFlags(arangodb::Index::Serialize::Figures));

      std::string error;
      EXPECT_TRUE(actualMeta.init(builder->slice(), false, error));
      EXPECT_TRUE(error.empty());
      EXPECT_TRUE(expectedMeta == actualMeta);
      auto const slice = builder->slice();
      EXPECT_TRUE(slice.hasKey("view"));
      EXPECT_TRUE(slice.get("view").isString());
      EXPECT_TRUE(view->id() == 42);
      EXPECT_TRUE(view->guid() == slice.get("view").copyString());
      EXPECT_TRUE(slice.hasKey("figures"));
      EXPECT_TRUE(slice.get("figures").isObject());
      EXPECT_TRUE(slice.get("figures").hasKey("memory"));
      EXPECT_TRUE(slice.get("figures").get("memory").isNumber());
      EXPECT_TRUE(0 < slice.get("figures").get("memory").getUInt());
    }

    EXPECT_TRUE(view->properties(linksJson->slice(), true).ok());  // same properties -> should not affect plan version
    EXPECT_TRUE(planVersion == arangodb::tests::getCurrentPlanVersion());  // plan did't change version

    // simulate heartbeat thread (drop index from current)
    {
      auto const value = arangodb::velocypack::Parser::fromJson(
          "{ \"shard-id-does-not-matter\": { \"indexes\" : [ ] } }");
      EXPECT_TRUE(arangodb::AgencyComm()
                      .setValue(currentCollectionPath, value->slice(), 0.0)
                      .successful());
    }

    // drop link
    EXPECT_TRUE(
        (arangodb::methods::Indexes::drop(
             logicalCollection.get(),
             arangodb::velocypack::Parser::fromJson(std::to_string(linkId))->slice())
             .ok()));
    EXPECT_TRUE(planVersion < arangodb::tests::getCurrentPlanVersion());  // plan version changed
    planVersion = arangodb::tests::getCurrentPlanVersion();
    oldView = view;
    view = std::dynamic_pointer_cast<arangodb::iresearch::IResearchViewCoordinator>(
        ci->getView(vocbase->name(), viewId));  // get new version of the view
    EXPECT_TRUE(view != oldView);               // different objects
    EXPECT_TRUE(nullptr != view);
    EXPECT_TRUE(nullptr !=
                std::dynamic_pointer_cast<arangodb::iresearch::IResearchViewCoordinator>(view));
    EXPECT_TRUE(planVersion == view->planVersion());
    EXPECT_TRUE("testView" == view->name());
    EXPECT_TRUE(false == view->deleted());
    EXPECT_TRUE(42 == view->id());
    EXPECT_TRUE(arangodb::iresearch::DATA_SOURCE_TYPE == view->type());
    EXPECT_TRUE(arangodb::LogicalView::category() == view->category());
    EXPECT_TRUE(vocbase == &view->vocbase());

    // visit collections
    EXPECT_TRUE(view->visitCollections([](TRI_voc_cid_t) { return false; }));

    // check properties
    {
      VPackBuilder info;
      info.openObject();
      view->properties(info, arangodb::LogicalDataSource::makeFlags(
                                 arangodb::LogicalDataSource::Serialize::Detailed));
      info.close();

      auto const properties = info.slice();
      EXPECT_TRUE((properties.hasKey(arangodb::iresearch::StaticStrings::LinksField)));
      auto links = properties.get(arangodb::iresearch::StaticStrings::LinksField);
      EXPECT_TRUE((links.isObject()));
      EXPECT_TRUE((0 == links.length()));
    }

    // check indexes
    {
      // get new version from plan
      auto updatedCollection =
          ci->getCollection(vocbase->name(), std::to_string(logicalCollection->id()));
      ASSERT_TRUE(updatedCollection);
      auto link = arangodb::iresearch::IResearchLinkHelper::find(*updatedCollection, *view);
      EXPECT_TRUE(!link);
    }

    // drop view
    EXPECT_TRUE(view->drop().ok());
    EXPECT_TRUE(planVersion < arangodb::tests::getCurrentPlanVersion());

    // there is no more view
    EXPECT_TRUE(nullptr == ci->getView(vocbase->name(), view->name()));
  }

  // add link (collection not authorized)
  {
    auto viewCreateJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testView\", \"id\": \"42\", \"type\": "
        "\"arangosearch\" }");
    auto viewUpdateJson = arangodb::velocypack::Parser::fromJson(
        "{ \"links\": { \"testCollection\" : { \"includeAllFields\" : true } "
        "} }");
    auto const collectionId = "1";
    auto logicalCollection1 = ci->getCollection(vocbase->name(), collectionId);
    ASSERT_TRUE((false == !logicalCollection1));
    arangodb::LogicalView::ptr logicalView;
    ASSERT_TRUE(
        (arangodb::LogicalView::create(logicalView, *vocbase, viewCreateJson->slice())
             .ok()));
    ASSERT_TRUE((false == !logicalView));
    auto const viewId = std::to_string(logicalView->planId());

    EXPECT_TRUE((true == logicalCollection1->getIndexes().empty()));
    EXPECT_TRUE((true == logicalView->visitCollections(
                             [](TRI_voc_cid_t) -> bool { return false; })));

    struct ExecContext : public arangodb::ExecContext {
      ExecContext()
          : arangodb::ExecContext(arangodb::ExecContext::Type::Default, "", "",
                                  arangodb::auth::Level::NONE,
                                  arangodb::auth::Level::NONE) {}
    } execContext;
    auto* origExecContext = ExecContext::CURRENT;
    auto resetExecContext = irs::make_finally(
        [origExecContext]() -> void { ExecContext::CURRENT = origExecContext; });
    ExecContext::CURRENT = &execContext;
    auto* authFeature = arangodb::AuthenticationFeature::instance();
    auto* userManager = authFeature->userManager();
    arangodb::aql::QueryRegistry queryRegistry(0);  // required for UserManager::removeAllUsers()
    arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
    userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database
    userManager->setQueryRegistry(&queryRegistry);
    auto resetUserManager = irs::make_finally(
        [userManager]() -> void { userManager->removeAllUsers(); });

    EXPECT_TRUE((TRI_ERROR_FORBIDDEN ==
                 logicalView->properties(viewUpdateJson->slice(), false).errorNumber()));
    logicalCollection1 = ci->getCollection(vocbase->name(), collectionId);
    ASSERT_TRUE((false == !logicalCollection1));
    logicalView = ci->getView(vocbase->name(), viewId);  // get new version of the view
    ASSERT_TRUE((false == !logicalView));
    EXPECT_TRUE((true == logicalCollection->getIndexes().empty()));
    EXPECT_TRUE((true == logicalView->visitCollections(
                             [](TRI_voc_cid_t) -> bool { return false; })));
  }
}

TEST_F(IResearchViewCoordinatorTest, test_update_overwrite) {
  auto* database = arangodb::DatabaseFeature::DATABASE;
  ASSERT_TRUE(nullptr != database);
  auto* ci = arangodb::ClusterInfo::instance();
  ASSERT_TRUE((nullptr != ci));
  TRI_vocbase_t* vocbase;  // will be owned by DatabaseFeature

  // create database
  {
    // simulate heartbeat thread
    ASSERT_TRUE((TRI_ERROR_NO_ERROR ==
                 database->createDatabase(1, "testDatabase", vocbase)));

    ASSERT_TRUE((nullptr != vocbase));
    EXPECT_TRUE(("testDatabase" == vocbase->name()));
    EXPECT_TRUE((TRI_vocbase_type_e::TRI_VOCBASE_TYPE_COORDINATOR == vocbase->type()));
    EXPECT_TRUE((1 == vocbase->id()));

    EXPECT_TRUE((ci->createDatabaseCoordinator(vocbase->name(), VPackSlice::emptyObjectSlice(), 0.0)
                     .ok()));
  }

  // modify meta params with links to missing collections
  {
    auto viewCreateJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testView\", \"id\": \"42\", \"type\": "
        "\"arangosearch\" }");
    auto viewUpdateJson = arangodb::velocypack::Parser::fromJson(
        "{ \"links\": { \"testCollection\": {} } }");
    auto viewId = std::to_string(42);

    EXPECT_TRUE(
        (ci->createViewCoordinator(vocbase->name(), viewId, viewCreateJson->slice())
             .ok()));
    auto logicalView = ci->getView(vocbase->name(), viewId);
    ASSERT_TRUE((false == !logicalView));
    auto dropLogicalView = std::shared_ptr<arangodb::ClusterInfo>(
        ci, [vocbase, &viewId](arangodb::ClusterInfo* ci) -> void {
          ci->dropViewCoordinator(vocbase->name(), viewId);
        });

    EXPECT_TRUE((true == logicalView->visitCollections(
                             [](TRI_voc_cid_t) -> bool { return false; })));

    arangodb::iresearch::IResearchViewMeta expectedMeta;
    expectedMeta._cleanupIntervalStep = 10;

    EXPECT_TRUE((TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND ==
                 logicalView->properties(viewUpdateJson->slice(), false).errorNumber()));
    logicalView = ci->getView(vocbase->name(), viewId);
    ASSERT_TRUE((false == !logicalView));
    EXPECT_TRUE((true == logicalView->visitCollections(
                             [](TRI_voc_cid_t) -> bool { return false; })));

    arangodb::velocypack::Builder builder;
    builder.openObject();
    logicalView->properties(builder,
                            arangodb::LogicalDataSource::makeFlags(
                                arangodb::LogicalDataSource::Serialize::Detailed,
                                arangodb::LogicalDataSource::Serialize::ForPersistence));  // 'forPersistence' to avoid auth check
    builder.close();

    auto slice = builder.slice();
    arangodb::iresearch::IResearchViewMeta meta;
    std::string error;
    EXPECT_TRUE((meta.init(slice, error) && expectedMeta == meta));
  }

  // modify meta params with links with invalid definition
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson(
        "{ \"id\": \"1\", \"planId\": \"1\",  \"name\": \"testCollection\", "
        "\"replicationFactor\": 1, \"type\": 1, \"shards\":{} }");
    auto viewCreateJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testView\", \"id\": \"42\", \"type\": "
        "\"arangosearch\" }");
    auto viewUpdateJson = arangodb::velocypack::Parser::fromJson(
        "{ \"links\": { \"testCollection\": 42 } }");
    auto collectionId = std::to_string(1);
    auto viewId = std::to_string(42);

    EXPECT_TRUE((ci->createCollectionCoordinator(vocbase->name(), collectionId, 0, 1,
                                                 1, false, collectionJson->slice(), 0.0)
                     .ok()));
    auto logicalCollection = ci->getCollection(vocbase->name(), collectionId);
    ASSERT_TRUE((false == !logicalCollection));
    auto dropLogicalCollection = std::shared_ptr<arangodb::ClusterInfo>(
        ci, [vocbase, &collectionId](arangodb::ClusterInfo* ci) -> void {
          ci->dropCollectionCoordinator(vocbase->name(), collectionId, 0);
        });
    EXPECT_TRUE(
        (ci->createViewCoordinator(vocbase->name(), viewId, viewCreateJson->slice())
             .ok()));
    auto logicalView = ci->getView(vocbase->name(), viewId);
    ASSERT_TRUE((false == !logicalView));
    auto dropLogicalView = std::shared_ptr<arangodb::ClusterInfo>(
        ci, [vocbase, &viewId](arangodb::ClusterInfo* ci) -> void {
          ci->dropViewCoordinator(vocbase->name(), viewId);
        });

    EXPECT_TRUE((true == logicalCollection->getIndexes().empty()));
    EXPECT_TRUE((true == logicalView->visitCollections(
                             [](TRI_voc_cid_t) -> bool { return false; })));

    arangodb::iresearch::IResearchViewMeta expectedMeta;
    expectedMeta._cleanupIntervalStep = 10;

    EXPECT_TRUE((TRI_ERROR_BAD_PARAMETER ==
                 logicalView->properties(viewUpdateJson->slice(), false).errorNumber()));
    logicalView = ci->getView(vocbase->name(), viewId);
    ASSERT_TRUE((false == !logicalView));
    EXPECT_TRUE((true == logicalView->visitCollections(
                             [](TRI_voc_cid_t) -> bool { return false; })));

    arangodb::velocypack::Builder builder;
    builder.openObject();
    logicalView->properties(builder,
                            arangodb::LogicalDataSource::makeFlags(
                                arangodb::LogicalDataSource::Serialize::Detailed,
                                arangodb::LogicalDataSource::Serialize::ForPersistence));  // 'forPersistence' to avoid auth check
    builder.close();

    auto slice = builder.slice();
    arangodb::iresearch::IResearchViewMeta meta;
    std::string error;
    EXPECT_TRUE((meta.init(slice, error) && expectedMeta == meta));
  }

  // modify meta params with links (collection not authorized)
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson(
        "{ \"id\": \"1\", \"planId\": \"1\",  \"name\": \"testCollection\", "
        "\"replicationFactor\": 1, \"type\": 1, \"shards\":{} }");
    auto viewCreateJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testView\", \"id\": \"42\", \"type\": "
        "\"arangosearch\" }");
    auto viewUpdateJson = arangodb::velocypack::Parser::fromJson(
        "{ \"cleanupIntervalStep\": 62, \"links\": { \"testCollection\": {} "
        "} }");
    auto collectionId = std::to_string(1);
    auto viewId = std::to_string(42);

    EXPECT_TRUE((ci->createCollectionCoordinator(vocbase->name(), collectionId, 0, 1,
                                                 1, false, collectionJson->slice(), 0.0)
                     .ok()));
    auto logicalCollection = ci->getCollection(vocbase->name(), collectionId);
    ASSERT_TRUE((false == !logicalCollection));
    auto dropLogicalCollection = std::shared_ptr<arangodb::ClusterInfo>(
        ci, [vocbase, &collectionId](arangodb::ClusterInfo* ci) -> void {
          ci->dropCollectionCoordinator(vocbase->name(), collectionId, 0);
        });
    EXPECT_TRUE(
        (ci->createViewCoordinator(vocbase->name(), viewId, viewCreateJson->slice())
             .ok()));
    auto logicalView = ci->getView(vocbase->name(), viewId);
    ASSERT_TRUE((false == !logicalView));
    auto dropLogicalView = std::shared_ptr<arangodb::ClusterInfo>(
        ci, [vocbase, &viewId](arangodb::ClusterInfo* ci) -> void {
          ci->dropViewCoordinator(vocbase->name(), viewId);
        });

    EXPECT_TRUE((true == logicalCollection->getIndexes().empty()));
    EXPECT_TRUE((true == logicalView->visitCollections(
                             [](TRI_voc_cid_t) -> bool { return false; })));

    // initial link creation
    {
      // simulate heartbeat thread (create index in current)
      {
        auto const path = "/Current/Collections/" + vocbase->name() + "/" +
                          std::to_string(logicalCollection->id());
        auto const value = arangodb::velocypack::Parser::fromJson(
            "{ \"shard-id-does-not-matter\": { \"indexes\" : [ { \"id\": "
            "\"1\" } ] } }");
        EXPECT_TRUE(arangodb::AgencyComm().setValue(path, value->slice(), 0.0).successful());
      }

      auto updateJson = arangodb::velocypack::Parser::fromJson(
          "{ \"links\": { \"testCollection\": {} } }");
      EXPECT_TRUE((logicalView->properties(updateJson->slice(), true).ok()));
      logicalCollection = ci->getCollection(vocbase->name(), collectionId);
      ASSERT_TRUE((false == !logicalCollection));
      logicalView = ci->getView(vocbase->name(), viewId);
      ASSERT_TRUE((false == !logicalView));
      EXPECT_TRUE((false == logicalCollection->getIndexes().empty()));
      EXPECT_TRUE((false == logicalView->visitCollections(
                                [](TRI_voc_cid_t) -> bool { return false; })));
    }

    struct ExecContext : public arangodb::ExecContext {
      ExecContext()
          : arangodb::ExecContext(arangodb::ExecContext::Type::Default, "", "",
                                  arangodb::auth::Level::NONE,
                                  arangodb::auth::Level::NONE) {}
    } execContext;
    arangodb::ExecContextScope execContextScope(&execContext);
    auto* authFeature = arangodb::AuthenticationFeature::instance();
    auto* userManager = authFeature->userManager();
    arangodb::aql::QueryRegistry queryRegistry(0);  // required for UserManager::loadFromDB()
    userManager->setQueryRegistry(&queryRegistry);
    auto resetUserManager = std::shared_ptr<arangodb::auth::UserManager>(
        userManager,
        [](arangodb::auth::UserManager* ptr) -> void { ptr->removeAllUsers(); });

    // subsequent update (overwrite) not authorised (NONE collection)
    {
      arangodb::auth::UserMap userMap;
      auto& user =
          userMap
              .emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP))
              .first->second;
      user.grantCollection(vocbase->name(), "testCollection", arangodb::auth::Level::NONE);  // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database

      arangodb::iresearch::IResearchViewMeta expectedMeta;
      expectedMeta._cleanupIntervalStep = 10;

      EXPECT_TRUE((TRI_ERROR_FORBIDDEN ==
                   logicalView->properties(viewUpdateJson->slice(), false).errorNumber()));
      logicalView = ci->getView(vocbase->name(), viewId);
      ASSERT_TRUE((false == !logicalView));

      arangodb::velocypack::Builder builder;
      builder.openObject();
      logicalView->properties(builder,
                              arangodb::LogicalDataSource::makeFlags(
                                  arangodb::LogicalDataSource::Serialize::Detailed,
                                  arangodb::LogicalDataSource::Serialize::ForPersistence));  // 'forPersistence' to avoid auth check
      builder.close();

      auto slice = builder.slice();
      arangodb::iresearch::IResearchViewMeta meta;
      std::string error;
      EXPECT_TRUE((meta.init(slice, error) && expectedMeta == meta));
    }

    // subsequent update (overwrite) authorised (RO collection)
    {
      arangodb::auth::UserMap userMap;
      auto& user =
          userMap
              .emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP))
              .first->second;
      user.grantCollection(vocbase->name(), "testCollection", arangodb::auth::Level::RO);  // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database

      arangodb::iresearch::IResearchViewMeta expectedMeta;
      expectedMeta._cleanupIntervalStep = 62;

      EXPECT_TRUE((logicalView->properties(viewUpdateJson->slice(), false).ok()));
      logicalView = ci->getView(vocbase->name(), viewId);
      ASSERT_TRUE((false == !logicalView));

      arangodb::velocypack::Builder builder;
      builder.openObject();
      logicalView->properties(builder,
                              arangodb::LogicalDataSource::makeFlags(
                                  arangodb::LogicalDataSource::Serialize::Detailed,
                                  arangodb::LogicalDataSource::Serialize::ForPersistence));  // 'forPersistence' to avoid auth check
      builder.close();

      auto slice = builder.slice();
      arangodb::iresearch::IResearchViewMeta meta;
      std::string error;
      EXPECT_TRUE((meta.init(slice, error) && expectedMeta == meta));
    }
  }

  // add link (collection not authorized)
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson(
        "{ \"id\": \"1\", \"planId\": \"1\",  \"name\": \"testCollection\", "
        "\"replicationFactor\": 1, \"shards\":{} }");
    auto viewCreateJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testView\", \"id\": \"42\", \"type\": "
        "\"arangosearch\" }");
    auto viewUpdateJson = arangodb::velocypack::Parser::fromJson(
        "{ \"links\": { \"testCollection\": {} } }");
    auto collectionId = std::to_string(1);
    auto viewId = std::to_string(42);

    EXPECT_TRUE((ci->createCollectionCoordinator(vocbase->name(), collectionId, 0, 1,
                                                 1, false, collectionJson->slice(), 0.0)
                     .ok()));
    auto logicalCollection = ci->getCollection(vocbase->name(), collectionId);
    ASSERT_TRUE((false == !logicalCollection));
    auto dropLogicalCollection = std::shared_ptr<arangodb::ClusterInfo>(
        ci, [vocbase, &collectionId](arangodb::ClusterInfo* ci) -> void {
          ci->dropCollectionCoordinator(vocbase->name(), collectionId, 0);
        });
    EXPECT_TRUE(
        (ci->createViewCoordinator(vocbase->name(), viewId, viewCreateJson->slice())
             .ok()));
    auto logicalView = ci->getView(vocbase->name(), viewId);
    ASSERT_TRUE((false == !logicalView));
    auto dropLogicalView = std::shared_ptr<arangodb::ClusterInfo>(
        ci, [vocbase, &viewId](arangodb::ClusterInfo* ci) -> void {
          ci->dropViewCoordinator(vocbase->name(), viewId);
        });

    EXPECT_TRUE((true == logicalCollection->getIndexes().empty()));
    EXPECT_TRUE((true == logicalView->visitCollections(
                             [](TRI_voc_cid_t) -> bool { return false; })));

    struct ExecContext : public arangodb::ExecContext {
      ExecContext()
          : arangodb::ExecContext(arangodb::ExecContext::Type::Default, "", "",
                                  arangodb::auth::Level::NONE,
                                  arangodb::auth::Level::NONE) {}
    } execContext;
    auto* origExecContext = ExecContext::CURRENT;
    auto resetExecContext = irs::make_finally(
        [origExecContext]() -> void { ExecContext::CURRENT = origExecContext; });
    ExecContext::CURRENT = &execContext;
    auto* authFeature = arangodb::AuthenticationFeature::instance();
    auto* userManager = authFeature->userManager();
    arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
    userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database
    auto resetUserManager = irs::make_finally(
        [userManager]() -> void { userManager->removeAllUsers(); });

    EXPECT_TRUE((TRI_ERROR_FORBIDDEN ==
                 logicalView->properties(viewUpdateJson->slice(), false).errorNumber()));
    logicalCollection = ci->getCollection(vocbase->name(), collectionId);
    ASSERT_TRUE((false == !logicalCollection));
    logicalView = ci->getView(vocbase->name(), viewId);
    ASSERT_TRUE((false == !logicalView));
    EXPECT_TRUE((true == logicalCollection->getIndexes().empty()));
    EXPECT_TRUE((true == logicalView->visitCollections(
                             [](TRI_voc_cid_t) -> bool { return false; })));
  }

  // drop link (collection not authorized)
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson(
        "{ \"id\": \"1\", \"planId\": \"1\",  \"name\": \"testCollection\", "
        "\"replicationFactor\": 1, \"type\": 1, \"shards\":{} }");
    auto viewCreateJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testView\", \"id\": \"42\", \"type\": "
        "\"arangosearch\" }");
    auto viewUpdateJson = arangodb::velocypack::Parser::fromJson(
        "{ \"links\": { \"testCollection\": null } }");
    auto collectionId = std::to_string(1);
    auto viewId = std::to_string(42);

    EXPECT_TRUE((ci->createCollectionCoordinator(vocbase->name(), collectionId, 0, 1,
                                                 1, false, collectionJson->slice(), 0.0)
                     .ok()));
    auto logicalCollection = ci->getCollection(vocbase->name(), collectionId);
    ASSERT_TRUE((false == !logicalCollection));
    auto dropLogicalCollection = std::shared_ptr<arangodb::ClusterInfo>(
        ci, [vocbase, &collectionId](arangodb::ClusterInfo* ci) -> void {
          ci->dropCollectionCoordinator(vocbase->name(), collectionId, 0);
        });
    EXPECT_TRUE(
        (ci->createViewCoordinator(vocbase->name(), viewId, viewCreateJson->slice())
             .ok()));
    auto logicalView = ci->getView(vocbase->name(), viewId);
    ASSERT_TRUE((false == !logicalView));
    auto dropLogicalView = std::shared_ptr<arangodb::ClusterInfo>(
        ci, [vocbase, &viewId](arangodb::ClusterInfo* ci) -> void {
          ci->dropViewCoordinator(vocbase->name(), viewId);
        });

    EXPECT_TRUE((true == logicalCollection->getIndexes().empty()));
    EXPECT_TRUE((true == logicalView->visitCollections(
                             [](TRI_voc_cid_t) -> bool { return false; })));

    // initial link creation
    {
      // simulate heartbeat thread (create index in current)
      {
        auto const path = "/Current/Collections/" + vocbase->name() + "/" +
                          std::to_string(logicalCollection->id());
        auto const value = arangodb::velocypack::Parser::fromJson(
            "{ \"shard-id-does-not-matter\": { \"indexes\" : [ { \"id\": "
            "\"2\" } ] } }");
        EXPECT_TRUE(arangodb::AgencyComm().setValue(path, value->slice(), 0.0).successful());
      }

      auto updateJson = arangodb::velocypack::Parser::fromJson(
          "{ \"links\": { \"testCollection\": {} } }");
      EXPECT_TRUE((logicalView->properties(updateJson->slice(), true).ok()));
      logicalCollection = ci->getCollection(vocbase->name(), collectionId);
      ASSERT_TRUE((false == !logicalCollection));
      logicalView = ci->getView(vocbase->name(), viewId);
      ASSERT_TRUE((false == !logicalView));
      EXPECT_TRUE((false == logicalCollection->getIndexes().empty()));
      EXPECT_TRUE((false == logicalView->visitCollections(
                                [](TRI_voc_cid_t) -> bool { return false; })));

      // simulate heartbeat thread (update index in current)
      {
        auto const path = "/Current/Collections/" + vocbase->name() + "/" +
                          std::to_string(logicalCollection->id());
        auto const value = arangodb::velocypack::Parser::fromJson(
            "{ \"shard-id-does-not-matter\": { \"indexes\" : [ { \"id\": "
            "\"3\" } ] } }");
        EXPECT_TRUE(arangodb::AgencyComm().setValue(path, value->slice(), 0.0).successful());
      }
    }

    struct ExecContext : public arangodb::ExecContext {
      ExecContext()
          : arangodb::ExecContext(arangodb::ExecContext::Type::Default, "", "",
                                  arangodb::auth::Level::NONE,
                                  arangodb::auth::Level::NONE) {}
    } execContext;
    arangodb::ExecContextScope execContextScope(&execContext);
    auto* authFeature = arangodb::AuthenticationFeature::instance();
    auto* userManager = authFeature->userManager();
    arangodb::aql::QueryRegistry queryRegistry(0);  // required for UserManager::loadFromDB()
    userManager->setQueryRegistry(&queryRegistry);
    auto resetUserManager = std::shared_ptr<arangodb::auth::UserManager>(
        userManager,
        [](arangodb::auth::UserManager* ptr) -> void { ptr->removeAllUsers(); });

    // subsequent update (overwrite) not authorised (NONE collection)
    {
      arangodb::auth::UserMap userMap;
      auto& user =
          userMap
              .emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP))
              .first->second;
      user.grantCollection(vocbase->name(), "testCollection", arangodb::auth::Level::NONE);  // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database

      EXPECT_TRUE((TRI_ERROR_FORBIDDEN ==
                   logicalView->properties(viewUpdateJson->slice(), false).errorNumber()));
      logicalCollection = ci->getCollection(vocbase->name(), collectionId);
      ASSERT_TRUE((false == !logicalCollection));
      logicalView = ci->getView(vocbase->name(), viewId);
      ASSERT_TRUE((false == !logicalView));
      EXPECT_TRUE((false == logicalCollection->getIndexes().empty()));
      EXPECT_TRUE((false == logicalView->visitCollections(
                                [](TRI_voc_cid_t) -> bool { return false; })));
    }

    // subsequent update (overwrite) authorised (RO collection)
    {
      arangodb::auth::UserMap userMap;
      auto& user =
          userMap
              .emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP))
              .first->second;
      user.grantCollection(vocbase->name(), "testCollection", arangodb::auth::Level::RO);  // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database

      EXPECT_TRUE((logicalView->properties(viewUpdateJson->slice(), false).ok()));
      logicalCollection = ci->getCollection(vocbase->name(), collectionId);
      ASSERT_TRUE((false == !logicalCollection));
      logicalView = ci->getView(vocbase->name(), viewId);
      ASSERT_TRUE((false == !logicalView));
      EXPECT_TRUE((true == logicalCollection->getIndexes().empty()));
      EXPECT_TRUE((true == logicalView->visitCollections(
                               [](TRI_voc_cid_t) -> bool { return false; })));
    }
  }

  // add authorised link (existing collection not authorized)
  {
    auto collection0Json = arangodb::velocypack::Parser::fromJson(
        "{ \"id\": \"1\", \"planId\": \"1\",  \"name\": \"testCollection0\", "
        "\"replicationFactor\": 1, \"type\": 1, \"shards\":{} }");
    auto collection1Json = arangodb::velocypack::Parser::fromJson(
        "{ \"id\": \"2\", \"planId\": \"2\",  \"name\": \"testCollection1\", "
        "\"replicationFactor\": 1, \"type\": 1, \"shards\":{} }");
    auto viewCreateJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testView\", \"id\": \"42\", \"type\": "
        "\"arangosearch\" }");
    auto viewUpdateJson = arangodb::velocypack::Parser::fromJson(
        "{ \"links\": { \"testCollection0\": {}, \"testCollection1\": {} } "
        "}");
    auto collectionId0 = std::to_string(1);
    auto collectionId1 = std::to_string(2);
    auto viewId = std::to_string(42);

    EXPECT_TRUE((ci->createCollectionCoordinator(vocbase->name(), collectionId0, 0, 1,
                                                 1, false, collection0Json->slice(), 0.0)
                     .ok()));
    auto logicalCollection0 = ci->getCollection(vocbase->name(), collectionId0);
    ASSERT_TRUE((false == !logicalCollection0));
    auto dropLogicalCollection0 = std::shared_ptr<arangodb::ClusterInfo>(
        ci, [vocbase, &collectionId0](arangodb::ClusterInfo* ci) -> void {
          ci->dropCollectionCoordinator(vocbase->name(), collectionId0, 0);
        });
    EXPECT_TRUE((ci->createCollectionCoordinator(vocbase->name(), collectionId1, 0, 1,
                                                 1, false, collection1Json->slice(), 0.0)
                     .ok()));
    auto logicalCollection1 = ci->getCollection(vocbase->name(), collectionId1);
    ASSERT_TRUE((false == !logicalCollection1));
    auto dropLogicalCollection1 = std::shared_ptr<arangodb::ClusterInfo>(
        ci, [vocbase, &collectionId1](arangodb::ClusterInfo* ci) -> void {
          ci->dropCollectionCoordinator(vocbase->name(), collectionId1, 0);
        });
    EXPECT_TRUE(
        (ci->createViewCoordinator(vocbase->name(), viewId, viewCreateJson->slice())
             .ok()));
    auto logicalView = ci->getView(vocbase->name(), viewId);
    ASSERT_TRUE((false == !logicalView));
    auto dropLogicalView = std::shared_ptr<arangodb::ClusterInfo>(
        ci, [vocbase, &viewId](arangodb::ClusterInfo* ci) -> void {
          ci->dropViewCoordinator(vocbase->name(), viewId);
        });

    EXPECT_TRUE((true == logicalCollection0->getIndexes().empty()));
    EXPECT_TRUE((true == logicalCollection1->getIndexes().empty()));
    EXPECT_TRUE((true == logicalView->visitCollections(
                             [](TRI_voc_cid_t) -> bool { return false; })));

    // initial link creation
    {
      // simulate heartbeat thread (create index in current)
      {
        auto const path0 = "/Current/Collections/" + vocbase->name() + "/" +
                           std::to_string(logicalCollection0->id());
        auto const path1 = "/Current/Collections/" + vocbase->name() + "/" +
                           std::to_string(logicalCollection1->id());
        auto const value0 = arangodb::velocypack::Parser::fromJson(
            "{ \"shard-id-does-not-matter\": { \"indexes\" : [ { \"id\": "
            "\"3\" } ] } }");
        auto const value1 = arangodb::velocypack::Parser::fromJson(
            "{ \"shard-id-does-not-matter\": { \"indexes\" : [ { \"id\": "
            "\"4\" } ] } }");
        EXPECT_TRUE(arangodb::AgencyComm().setValue(path0, value0->slice(), 0.0).successful());
        EXPECT_TRUE(arangodb::AgencyComm().setValue(path1, value1->slice(), 0.0).successful());
      }

      auto updateJson = arangodb::velocypack::Parser::fromJson(
          "{ \"links\": { \"testCollection0\": {} } }");
      EXPECT_TRUE((logicalView->properties(updateJson->slice(), true).ok()));
      logicalCollection0 = ci->getCollection(vocbase->name(), collectionId0);
      ASSERT_TRUE((false == !logicalCollection0));
      logicalCollection1 = ci->getCollection(vocbase->name(), collectionId1);
      ASSERT_TRUE((false == !logicalCollection1));
      logicalView = ci->getView(vocbase->name(), viewId);
      ASSERT_TRUE((false == !logicalView));
      EXPECT_TRUE((false == logicalCollection0->getIndexes().empty()));
      EXPECT_TRUE((true == logicalCollection1->getIndexes().empty()));
      EXPECT_TRUE((false == logicalView->visitCollections(
                                [](TRI_voc_cid_t) -> bool { return false; })));
    }

    struct ExecContext : public arangodb::ExecContext {
      ExecContext()
          : arangodb::ExecContext(arangodb::ExecContext::Type::Default, "", "",
                                  arangodb::auth::Level::NONE,
                                  arangodb::auth::Level::NONE) {}
    } execContext;
    arangodb::ExecContextScope execContextScope(&execContext);
    auto* authFeature = arangodb::AuthenticationFeature::instance();
    auto* userManager = authFeature->userManager();
    arangodb::aql::QueryRegistry queryRegistry(0);  // required for UserManager::loadFromDB()
    userManager->setQueryRegistry(&queryRegistry);
    auto resetUserManager = std::shared_ptr<arangodb::auth::UserManager>(
        userManager,
        [](arangodb::auth::UserManager* ptr) -> void { ptr->removeAllUsers(); });

    // subsequent update (overwrite) not authorised (NONE collection)
    {
      arangodb::auth::UserMap userMap;
      auto& user =
          userMap
              .emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP))
              .first->second;
      user.grantCollection(vocbase->name(),
                           "testCollection0", arangodb::auth::Level::NONE);  // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      user.grantCollection(vocbase->name(), "testCollection1", arangodb::auth::Level::RO);  // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database

      EXPECT_TRUE((TRI_ERROR_FORBIDDEN ==
                   logicalView->properties(viewUpdateJson->slice(), false).errorNumber()));
      logicalCollection0 = ci->getCollection(vocbase->name(), collectionId0);
      ASSERT_TRUE((false == !logicalCollection0));
      logicalCollection1 = ci->getCollection(vocbase->name(), collectionId1);
      ASSERT_TRUE((false == !logicalCollection1));
      logicalView = ci->getView(vocbase->name(), viewId);
      ASSERT_TRUE((false == !logicalView));
      EXPECT_TRUE((false == logicalCollection0->getIndexes().empty()));
      EXPECT_TRUE((true == logicalCollection1->getIndexes().empty()));
      EXPECT_TRUE((false == logicalView->visitCollections(
                                [](TRI_voc_cid_t) -> bool { return false; })));
    }

    // subsequent update (overwrite) authorised (RO collection)
    {
      arangodb::auth::UserMap userMap;
      auto& user =
          userMap
              .emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP))
              .first->second;
      user.grantCollection(vocbase->name(), "testCollection0", arangodb::auth::Level::RO);  // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      user.grantCollection(vocbase->name(), "testCollection1", arangodb::auth::Level::RO);  // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database

      EXPECT_TRUE((logicalView->properties(viewUpdateJson->slice(), false).ok()));
      logicalCollection0 = ci->getCollection(vocbase->name(), collectionId0);
      ASSERT_TRUE((false == !logicalCollection0));
      logicalCollection1 = ci->getCollection(vocbase->name(), collectionId1);
      ASSERT_TRUE((false == !logicalCollection1));
      logicalView = ci->getView(vocbase->name(), viewId);
      ASSERT_TRUE((false == !logicalView));
      EXPECT_TRUE((false == logicalCollection0->getIndexes().empty()));
      EXPECT_TRUE((false == logicalCollection1->getIndexes().empty()));
      EXPECT_TRUE((false == logicalView->visitCollections(
                                [](TRI_voc_cid_t) -> bool { return false; })));
    }
  }

  // drop authorised link (existing collection not authorized)
  {
    auto collection0Json = arangodb::velocypack::Parser::fromJson(
        "{ \"id\": \"1\", \"planId\": \"1\",  \"name\": \"testCollection0\", "
        "\"replicationFactor\": 1, \"type\": 1, \"shards\":{} }");
    auto collection1Json = arangodb::velocypack::Parser::fromJson(
        "{ \"id\": \"2\", \"planId\": \"2\",  \"name\": \"testCollection1\", "
        "\"replicationFactor\": 1, \"type\": 1, \"shards\":{} }");
    auto viewCreateJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testView\", \"id\": \"42\", \"type\": "
        "\"arangosearch\" }");
    auto viewUpdateJson = arangodb::velocypack::Parser::fromJson(
        "{ \"links\": { \"testCollection0\": {} } }");
    auto collectionId0 = std::to_string(1);
    auto collectionId1 = std::to_string(2);
    auto viewId = std::to_string(42);

    EXPECT_TRUE((ci->createCollectionCoordinator(vocbase->name(), collectionId0, 0, 1,
                                                 1, false, collection0Json->slice(), 0.0)
                     .ok()));
    auto logicalCollection0 = ci->getCollection(vocbase->name(), collectionId0);
    ASSERT_TRUE((false == !logicalCollection0));
    auto dropLogicalCollection0 = std::shared_ptr<arangodb::ClusterInfo>(
        ci, [vocbase, &collectionId0](arangodb::ClusterInfo* ci) -> void {
          ci->dropCollectionCoordinator(vocbase->name(), collectionId0, 0);
        });
    EXPECT_TRUE((ci->createCollectionCoordinator(vocbase->name(), collectionId1, 0, 1,
                                                 1, false, collection1Json->slice(), 0.0)
                     .ok()));
    auto logicalCollection1 = ci->getCollection(vocbase->name(), collectionId1);
    ASSERT_TRUE((false == !logicalCollection1));
    auto dropLogicalCollection1 = std::shared_ptr<arangodb::ClusterInfo>(
        ci, [vocbase, &collectionId1](arangodb::ClusterInfo* ci) -> void {
          ci->dropCollectionCoordinator(vocbase->name(), collectionId1, 0);
        });
    EXPECT_TRUE(
        (ci->createViewCoordinator(vocbase->name(), viewId, viewCreateJson->slice())
             .ok()));
    auto logicalView = ci->getView(vocbase->name(), viewId);
    ASSERT_TRUE((false == !logicalView));
    auto dropLogicalView = std::shared_ptr<arangodb::ClusterInfo>(
        ci, [vocbase, &viewId](arangodb::ClusterInfo* ci) -> void {
          ci->dropViewCoordinator(vocbase->name(), viewId);
        });

    EXPECT_TRUE((true == logicalCollection0->getIndexes().empty()));
    EXPECT_TRUE((true == logicalCollection1->getIndexes().empty()));
    EXPECT_TRUE((true == logicalView->visitCollections(
                             [](TRI_voc_cid_t) -> bool { return false; })));

    // initial link creation
    {
      // simulate heartbeat thread (create index in current)
      {
        auto const path0 = "/Current/Collections/" + vocbase->name() + "/" +
                           std::to_string(logicalCollection0->id());
        auto const path1 = "/Current/Collections/" + vocbase->name() + "/" +
                           std::to_string(logicalCollection1->id());
        auto const value0 = arangodb::velocypack::Parser::fromJson(
            "{ \"shard-id-does-not-matter\": { \"indexes\" : [ { \"id\": "
            "\"5\" } ] } }");
        auto const value1 = arangodb::velocypack::Parser::fromJson(
            "{ \"shard-id-does-not-matter\": { \"indexes\" : [ { \"id\": "
            "\"6\" } ] } }");
        EXPECT_TRUE(arangodb::AgencyComm().setValue(path0, value0->slice(), 0.0).successful());
        EXPECT_TRUE(arangodb::AgencyComm().setValue(path1, value1->slice(), 0.0).successful());
      }

      auto updateJson = arangodb::velocypack::Parser::fromJson(
          "{ \"links\": { \"testCollection0\": {}, \"testCollection1\": {} } "
          "}");
      EXPECT_TRUE((logicalView->properties(updateJson->slice(), true).ok()));
      logicalCollection0 = ci->getCollection(vocbase->name(), collectionId0);
      ASSERT_TRUE((false == !logicalCollection0));
      logicalCollection1 = ci->getCollection(vocbase->name(), collectionId1);
      ASSERT_TRUE((false == !logicalCollection1));
      logicalView = ci->getView(vocbase->name(), viewId);
      ASSERT_TRUE((false == !logicalView));
      EXPECT_TRUE((false == logicalCollection0->getIndexes().empty()));
      EXPECT_TRUE((false == logicalCollection1->getIndexes().empty()));
      EXPECT_TRUE((false == logicalView->visitCollections(
                                [](TRI_voc_cid_t) -> bool { return false; })));

      // simulate heartbeat thread (remove index from current)
      {
        auto const path = "/Current/Collections/" + vocbase->name() + "/" +
                          std::to_string(logicalCollection1->id()) +
                          "/shard-id-does-not-matter/indexes";
        EXPECT_TRUE(arangodb::AgencyComm().removeValues(path, false).successful());
      }
    }

    struct ExecContext : public arangodb::ExecContext {
      ExecContext()
          : arangodb::ExecContext(arangodb::ExecContext::Type::Default, "", "",
                                  arangodb::auth::Level::NONE,
                                  arangodb::auth::Level::NONE) {}
    } execContext;
    arangodb::ExecContextScope execContextScope(&execContext);
    auto* authFeature = arangodb::AuthenticationFeature::instance();
    auto* userManager = authFeature->userManager();
    arangodb::aql::QueryRegistry queryRegistry(0);  // required for UserManager::loadFromDB()
    userManager->setQueryRegistry(&queryRegistry);
    auto resetUserManager = std::shared_ptr<arangodb::auth::UserManager>(
        userManager,
        [](arangodb::auth::UserManager* ptr) -> void { ptr->removeAllUsers(); });

    // subsequent update (overwrite) not authorised (NONE collection)
    {
      arangodb::auth::UserMap userMap;
      auto& user =
          userMap
              .emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP))
              .first->second;
      user.grantCollection(vocbase->name(),
                           "testCollection0", arangodb::auth::Level::NONE);  // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      user.grantCollection(vocbase->name(), "testCollection1", arangodb::auth::Level::RO);  // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database

      EXPECT_TRUE((TRI_ERROR_FORBIDDEN ==
                   logicalView->properties(viewUpdateJson->slice(), false).errorNumber()));
      logicalCollection0 = ci->getCollection(vocbase->name(), collectionId0);
      ASSERT_TRUE((false == !logicalCollection0));
      logicalCollection1 = ci->getCollection(vocbase->name(), collectionId1);
      ASSERT_TRUE((false == !logicalCollection1));
      logicalView = ci->getView(vocbase->name(), viewId);
      ASSERT_TRUE((false == !logicalView));
      EXPECT_TRUE((false == logicalCollection0->getIndexes().empty()));
      EXPECT_TRUE((false == logicalCollection1->getIndexes().empty()));
      EXPECT_TRUE((false == logicalView->visitCollections(
                                [](TRI_voc_cid_t) -> bool { return false; })));
    }

    // subsequent update (overwrite) authorised (RO collection)
    {
      arangodb::auth::UserMap userMap;
      auto& user =
          userMap
              .emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP))
              .first->second;
      user.grantCollection(vocbase->name(), "testCollection0", arangodb::auth::Level::RO);  // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      user.grantCollection(vocbase->name(), "testCollection1", arangodb::auth::Level::RO);  // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database

      EXPECT_TRUE((logicalView->properties(viewUpdateJson->slice(), false).ok()));
      logicalCollection0 = ci->getCollection(vocbase->name(), collectionId0);
      ASSERT_TRUE((false == !logicalCollection0));
      logicalCollection1 = ci->getCollection(vocbase->name(), collectionId1);
      ASSERT_TRUE((false == !logicalCollection1));
      logicalView = ci->getView(vocbase->name(), viewId);
      ASSERT_TRUE((false == !logicalView));
      EXPECT_TRUE((false == logicalCollection0->getIndexes().empty()));
      EXPECT_TRUE((true == logicalCollection1->getIndexes().empty()));
      EXPECT_TRUE((false == logicalView->visitCollections(
                                [](TRI_voc_cid_t) -> bool { return false; })));
    }
  }
}

TEST_F(IResearchViewCoordinatorTest, test_update_partial) {
  auto* database = arangodb::DatabaseFeature::DATABASE;
  ASSERT_TRUE(nullptr != database);
  auto* ci = arangodb::ClusterInfo::instance();
  ASSERT_TRUE((nullptr != ci));
  TRI_vocbase_t* vocbase;  // will be owned by DatabaseFeature

  // create database
  {
    // simulate heartbeat thread
    ASSERT_TRUE((TRI_ERROR_NO_ERROR ==
                 database->createDatabase(1, "testDatabase", vocbase)));

    ASSERT_TRUE((nullptr != vocbase));
    EXPECT_TRUE(("testDatabase" == vocbase->name()));
    EXPECT_TRUE((TRI_vocbase_type_e::TRI_VOCBASE_TYPE_COORDINATOR == vocbase->type()));
    EXPECT_TRUE((1 == vocbase->id()));

    EXPECT_TRUE((ci->createDatabaseCoordinator(vocbase->name(), VPackSlice::emptyObjectSlice(), 0.0)
                     .ok()));
  }

  // modify meta params with links to missing collections
  {
    auto viewCreateJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testView\", \"id\": \"42\", \"type\": "
        "\"arangosearch\" }");
    auto viewUpdateJson = arangodb::velocypack::Parser::fromJson(
        "{ \"cleanupIntervalStep\": 62, \"links\": { \"testCollection\": {} "
        "} }");
    auto viewId = std::to_string(42);

    EXPECT_TRUE(
        (ci->createViewCoordinator(vocbase->name(), viewId, viewCreateJson->slice())
             .ok()));
    auto logicalView = ci->getView(vocbase->name(), viewId);
    ASSERT_TRUE((false == !logicalView));
    auto dropLogicalView = std::shared_ptr<arangodb::ClusterInfo>(
        ci, [vocbase, &viewId](arangodb::ClusterInfo* ci) -> void {
          ci->dropViewCoordinator(vocbase->name(), viewId);
        });

    EXPECT_TRUE((true == logicalView->visitCollections(
                             [](TRI_voc_cid_t) -> bool { return false; })));

    arangodb::iresearch::IResearchViewMeta expectedMeta;
    expectedMeta._cleanupIntervalStep = 10;

    EXPECT_TRUE((TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND ==
                 logicalView->properties(viewUpdateJson->slice(), true).errorNumber()));
    logicalView = ci->getView(vocbase->name(), viewId);
    ASSERT_TRUE((false == !logicalView));
    EXPECT_TRUE((true == logicalView->visitCollections(
                             [](TRI_voc_cid_t) -> bool { return false; })));

    arangodb::velocypack::Builder builder;
    builder.openObject();
    logicalView->properties(builder,
                            arangodb::LogicalDataSource::makeFlags(
                                arangodb::LogicalDataSource::Serialize::Detailed,
                                arangodb::LogicalDataSource::Serialize::ForPersistence));  // 'forPersistence' to avoid auth check
    builder.close();

    auto slice = builder.slice();
    arangodb::iresearch::IResearchViewMeta meta;
    std::string error;
    EXPECT_TRUE((meta.init(slice, error) && expectedMeta == meta));
  }

  // modify meta params with links with invalid definition
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson(
        "{ \"id\": \"1\", \"planId\": \"1\",  \"name\": \"testCollection\", "
        "\"replicationFactor\": 1, \"type\": 1, \"shards\":{} }");
    auto viewCreateJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testView\", \"id\": \"42\", \"type\": "
        "\"arangosearch\" }");
    auto viewUpdateJson = arangodb::velocypack::Parser::fromJson(
        "{ \"cleanupIntervalStep\": 62, \"links\": { \"testCollection\": 42 "
        "} }");
    auto collectionId = std::to_string(1);
    auto viewId = std::to_string(42);

    EXPECT_TRUE((ci->createCollectionCoordinator(vocbase->name(), collectionId, 0, 1,
                                                 1, false, collectionJson->slice(), 0.0)
                     .ok()));
    auto logicalCollection = ci->getCollection(vocbase->name(), collectionId);
    ASSERT_TRUE((false == !logicalCollection));
    auto dropLogicalCollection = std::shared_ptr<arangodb::ClusterInfo>(
        ci, [vocbase, &collectionId](arangodb::ClusterInfo* ci) -> void {
          ci->dropCollectionCoordinator(vocbase->name(), collectionId, 0);
        });
    EXPECT_TRUE(
        (ci->createViewCoordinator(vocbase->name(), viewId, viewCreateJson->slice())
             .ok()));
    auto logicalView = ci->getView(vocbase->name(), viewId);
    ASSERT_TRUE((false == !logicalView));
    auto dropLogicalView = std::shared_ptr<arangodb::ClusterInfo>(
        ci, [vocbase, &viewId](arangodb::ClusterInfo* ci) -> void {
          ci->dropViewCoordinator(vocbase->name(), viewId);
        });

    EXPECT_TRUE((true == logicalCollection->getIndexes().empty()));
    EXPECT_TRUE((true == logicalView->visitCollections(
                             [](TRI_voc_cid_t) -> bool { return false; })));

    arangodb::iresearch::IResearchViewMeta expectedMeta;
    expectedMeta._cleanupIntervalStep = 10;

    EXPECT_TRUE((TRI_ERROR_BAD_PARAMETER ==
                 logicalView->properties(viewUpdateJson->slice(), true).errorNumber()));
    logicalView = ci->getView(vocbase->name(), viewId);
    ASSERT_TRUE((false == !logicalView));
    EXPECT_TRUE((true == logicalView->visitCollections(
                             [](TRI_voc_cid_t) -> bool { return false; })));

    arangodb::velocypack::Builder builder;
    builder.openObject();
    logicalView->properties(builder,
                            arangodb::LogicalDataSource::makeFlags(
                                arangodb::LogicalDataSource::Serialize::Detailed,
                                arangodb::LogicalDataSource::Serialize::ForPersistence));  // 'forPersistence' to avoid auth check
    builder.close();

    auto slice = builder.slice();
    arangodb::iresearch::IResearchViewMeta meta;
    std::string error;
    EXPECT_TRUE((meta.init(slice, error) && expectedMeta == meta));
  }

  // modify meta params with links (collection not authorized)
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson(
        "{ \"id\": \"1\", \"planId\": \"1\",  \"name\": \"testCollection\", "
        "\"replicationFactor\": 1, \"shards\":{} }");
    auto viewCreateJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testView\", \"id\": \"42\", \"type\": "
        "\"arangosearch\" }");
    auto viewUpdateJson = arangodb::velocypack::Parser::fromJson(
        "{ \"cleanupIntervalStep\": 62 }");
    auto collectionId = std::to_string(1);
    auto viewId = std::to_string(42);

    EXPECT_TRUE((ci->createCollectionCoordinator(vocbase->name(), collectionId, 0, 1,
                                                 1, false, collectionJson->slice(), 0.0)
                     .ok()));
    auto logicalCollection = ci->getCollection(vocbase->name(), collectionId);
    ASSERT_TRUE((false == !logicalCollection));
    auto dropLogicalCollection = std::shared_ptr<arangodb::ClusterInfo>(
        ci, [vocbase, &collectionId](arangodb::ClusterInfo* ci) -> void {
          ci->dropCollectionCoordinator(vocbase->name(), collectionId, 0);
        });
    EXPECT_TRUE(
        (ci->createViewCoordinator(vocbase->name(), viewId, viewCreateJson->slice())
             .ok()));
    auto logicalView = ci->getView(vocbase->name(), viewId);
    ASSERT_TRUE((false == !logicalView));
    auto dropLogicalView = std::shared_ptr<arangodb::ClusterInfo>(
        ci, [vocbase, &viewId](arangodb::ClusterInfo* ci) -> void {
          ci->dropViewCoordinator(vocbase->name(), viewId);
        });

    EXPECT_TRUE((true == logicalCollection->getIndexes().empty()));
    EXPECT_TRUE((true == logicalView->visitCollections(
                             [](TRI_voc_cid_t) -> bool { return false; })));

    // initial link creation
    {
      // simulate heartbeat thread (create index in current)
      {
        auto const path = "/Current/Collections/" + vocbase->name() + "/" +
                          std::to_string(logicalCollection->id());
        auto const value = arangodb::velocypack::Parser::fromJson(
            "{ \"shard-id-does-not-matter\": { \"indexes\" : [ { \"id\": "
            "\"1\" } ] } }");
        EXPECT_TRUE(arangodb::AgencyComm().setValue(path, value->slice(), 0.0).successful());
      }

      auto updateJson = arangodb::velocypack::Parser::fromJson(
          "{ \"links\": { \"testCollection\": {} } }");
      EXPECT_TRUE((logicalView->properties(updateJson->slice(), true).ok()));
      logicalCollection = ci->getCollection(vocbase->name(), collectionId);
      ASSERT_TRUE((false == !logicalCollection));
      logicalView = ci->getView(vocbase->name(), viewId);
      ASSERT_TRUE((false == !logicalView));
      EXPECT_TRUE((false == logicalCollection->getIndexes().empty()));
      EXPECT_TRUE((false == logicalView->visitCollections(
                                [](TRI_voc_cid_t) -> bool { return false; })));
    }

    struct ExecContext : public arangodb::ExecContext {
      ExecContext()
          : arangodb::ExecContext(arangodb::ExecContext::Type::Default, "", "",
                                  arangodb::auth::Level::NONE,
                                  arangodb::auth::Level::NONE) {}
    } execContext;
    arangodb::ExecContextScope execContextScope(&execContext);
    auto* authFeature = arangodb::AuthenticationFeature::instance();
    auto* userManager = authFeature->userManager();
    arangodb::aql::QueryRegistry queryRegistry(0);  // required for UserManager::loadFromDB()
    userManager->setQueryRegistry(&queryRegistry);
    auto resetUserManager = std::shared_ptr<arangodb::auth::UserManager>(
        userManager,
        [](arangodb::auth::UserManager* ptr) -> void { ptr->removeAllUsers(); });

    // subsequent update (overwrite) not authorised (NONE collection)
    {
      arangodb::auth::UserMap userMap;
      auto& user =
          userMap
              .emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP))
              .first->second;
      user.grantCollection(vocbase->name(), "testCollection", arangodb::auth::Level::NONE);  // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database

      arangodb::iresearch::IResearchViewMeta expectedMeta;
      expectedMeta._cleanupIntervalStep = 10;

      EXPECT_TRUE((TRI_ERROR_FORBIDDEN ==
                   logicalView->properties(viewUpdateJson->slice(), true).errorNumber()));
      logicalView = ci->getView(vocbase->name(), viewId);
      ASSERT_TRUE((false == !logicalView));

      arangodb::velocypack::Builder builder;
      builder.openObject();
      logicalView->properties(builder,
                              arangodb::LogicalDataSource::makeFlags(
                                  arangodb::LogicalDataSource::Serialize::Detailed,
                                  arangodb::LogicalDataSource::Serialize::ForPersistence));  // 'forPersistence' to avoid auth check
      builder.close();

      auto slice = builder.slice();
      arangodb::iresearch::IResearchViewMeta meta;
      std::string error;
      EXPECT_TRUE((meta.init(slice, error) && expectedMeta == meta));
    }

    // subsequent update (overwrite) authorised (RO collection)
    {
      arangodb::auth::UserMap userMap;
      auto& user =
          userMap
              .emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP))
              .first->second;
      user.grantCollection(vocbase->name(), "testCollection", arangodb::auth::Level::RO);  // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database

      arangodb::iresearch::IResearchViewMeta expectedMeta;
      expectedMeta._cleanupIntervalStep = 62;

      EXPECT_TRUE((logicalView->properties(viewUpdateJson->slice(), true).ok()));
      logicalView = ci->getView(vocbase->name(), viewId);
      ASSERT_TRUE((false == !logicalView));

      arangodb::velocypack::Builder builder;
      builder.openObject();
      logicalView->properties(builder,
                              arangodb::LogicalDataSource::makeFlags(
                                  arangodb::LogicalDataSource::Serialize::Detailed,
                                  arangodb::LogicalDataSource::Serialize::ForPersistence));  // 'forPersistence' to avoid auth check
      builder.close();

      auto slice = builder.slice();
      arangodb::iresearch::IResearchViewMeta meta;
      std::string error;
      EXPECT_TRUE((meta.init(slice, error) && expectedMeta == meta));
    }
  }

  // add link (collection not authorized)
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson(
        "{ \"id\": \"1\", \"planId\": \"1\",  \"name\": \"testCollection\", "
        "\"replicationFactor\": 1, \"shards\":{} }");
    auto viewCreateJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testView\", \"id\": \"42\", \"type\": "
        "\"arangosearch\" }");
    auto viewUpdateJson = arangodb::velocypack::Parser::fromJson(
        "{ \"links\": { \"testCollection\": {} } }");
    auto collectionId = std::to_string(1);
    auto viewId = std::to_string(42);

    EXPECT_TRUE((ci->createCollectionCoordinator(vocbase->name(), collectionId, 0, 1,
                                                 1, false, collectionJson->slice(), 0.0)
                     .ok()));
    auto logicalCollection = ci->getCollection(vocbase->name(), collectionId);
    ASSERT_TRUE((false == !logicalCollection));
    auto dropLogicalCollection = std::shared_ptr<arangodb::ClusterInfo>(
        ci, [vocbase, &collectionId](arangodb::ClusterInfo* ci) -> void {
          ci->dropCollectionCoordinator(vocbase->name(), collectionId, 0);
        });
    EXPECT_TRUE(
        (ci->createViewCoordinator(vocbase->name(), viewId, viewCreateJson->slice())
             .ok()));
    auto logicalView = ci->getView(vocbase->name(), viewId);
    ASSERT_TRUE((false == !logicalView));
    auto dropLogicalView = std::shared_ptr<arangodb::ClusterInfo>(
        ci, [vocbase, &viewId](arangodb::ClusterInfo* ci) -> void {
          ci->dropViewCoordinator(vocbase->name(), viewId);
        });

    EXPECT_TRUE((true == logicalCollection->getIndexes().empty()));
    EXPECT_TRUE((true == logicalView->visitCollections(
                             [](TRI_voc_cid_t) -> bool { return false; })));

    struct ExecContext : public arangodb::ExecContext {
      ExecContext()
          : arangodb::ExecContext(arangodb::ExecContext::Type::Default, "", "",
                                  arangodb::auth::Level::NONE,
                                  arangodb::auth::Level::NONE) {}
    } execContext;
    auto* origExecContext = ExecContext::CURRENT;
    auto resetExecContext = irs::make_finally(
        [origExecContext]() -> void { ExecContext::CURRENT = origExecContext; });
    ExecContext::CURRENT = &execContext;
    auto* authFeature = arangodb::AuthenticationFeature::instance();
    auto* userManager = authFeature->userManager();
    arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
    userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database
    auto resetUserManager = irs::make_finally(
        [userManager]() -> void { userManager->removeAllUsers(); });

    EXPECT_TRUE((TRI_ERROR_FORBIDDEN ==
                 logicalView->properties(viewUpdateJson->slice(), true).errorNumber()));
    logicalCollection = ci->getCollection(vocbase->name(), collectionId);
    ASSERT_TRUE((false == !logicalCollection));
    logicalView = ci->getView(vocbase->name(), viewId);
    ASSERT_TRUE((false == !logicalView));
    EXPECT_TRUE((true == logicalCollection->getIndexes().empty()));
    EXPECT_TRUE((true == logicalView->visitCollections(
                             [](TRI_voc_cid_t) -> bool { return false; })));
  }

  // drop link (collection not authorized)
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson(
        "{ \"id\": \"1\", \"planId\": \"1\",  \"name\": \"testCollection\", "
        "\"replicationFactor\": 1, \"shards\":{} }");
    auto viewCreateJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testView\", \"id\": \"42\", \"type\": "
        "\"arangosearch\" }");
    auto viewUpdateJson = arangodb::velocypack::Parser::fromJson(
        "{ \"links\": { \"testCollection\": null } }");
    auto collectionId = std::to_string(1);
    auto viewId = std::to_string(42);

    EXPECT_TRUE((ci->createCollectionCoordinator(vocbase->name(), collectionId, 0, 1,
                                                 1, false, collectionJson->slice(), 0.0)
                     .ok()));
    auto logicalCollection = ci->getCollection(vocbase->name(), collectionId);
    ASSERT_TRUE((false == !logicalCollection));
    auto dropLogicalCollection = std::shared_ptr<arangodb::ClusterInfo>(
        ci, [vocbase, &collectionId](arangodb::ClusterInfo* ci) -> void {
          ci->dropCollectionCoordinator(vocbase->name(), collectionId, 0);
        });
    EXPECT_TRUE(
        (ci->createViewCoordinator(vocbase->name(), viewId, viewCreateJson->slice())
             .ok()));
    auto logicalView = ci->getView(vocbase->name(), viewId);
    ASSERT_TRUE((false == !logicalView));
    auto dropLogicalView = std::shared_ptr<arangodb::ClusterInfo>(
        ci, [vocbase, &viewId](arangodb::ClusterInfo* ci) -> void {
          ci->dropViewCoordinator(vocbase->name(), viewId);
        });

    EXPECT_TRUE((true == logicalCollection->getIndexes().empty()));
    EXPECT_TRUE((true == logicalView->visitCollections(
                             [](TRI_voc_cid_t) -> bool { return false; })));

    // initial link creation
    {
      // simulate heartbeat thread (create index in current)
      {
        auto const path = "/Current/Collections/" + vocbase->name() + "/" +
                          std::to_string(logicalCollection->id());
        auto const value = arangodb::velocypack::Parser::fromJson(
            "{ \"shard-id-does-not-matter\": { \"indexes\" : [ { \"id\": "
            "\"2\" } ] } }");
        EXPECT_TRUE(arangodb::AgencyComm().setValue(path, value->slice(), 0.0).successful());
      }

      auto updateJson = arangodb::velocypack::Parser::fromJson(
          "{ \"links\": { \"testCollection\": {} } }");
      EXPECT_TRUE((logicalView->properties(updateJson->slice(), true).ok()));
      logicalCollection = ci->getCollection(vocbase->name(), collectionId);
      ASSERT_TRUE((false == !logicalCollection));
      logicalView = ci->getView(vocbase->name(), viewId);
      ASSERT_TRUE((false == !logicalView));
      EXPECT_TRUE((false == logicalCollection->getIndexes().empty()));
      EXPECT_TRUE((false == logicalView->visitCollections(
                                [](TRI_voc_cid_t) -> bool { return false; })));

      // simulate heartbeat thread (remove index from current)
      {
        auto const path = "/Current/Collections/" + vocbase->name() + "/" +
                          std::to_string(logicalCollection->id()) +
                          "/shard-id-does-not-matter/indexes";
        EXPECT_TRUE(arangodb::AgencyComm().removeValues(path, false).successful());
      }
    }

    struct ExecContext : public arangodb::ExecContext {
      ExecContext()
          : arangodb::ExecContext(arangodb::ExecContext::Type::Default, "", "",
                                  arangodb::auth::Level::NONE,
                                  arangodb::auth::Level::NONE) {}
    } execContext;
    arangodb::ExecContextScope execContextScope(&execContext);
    auto* authFeature = arangodb::AuthenticationFeature::instance();
    auto* userManager = authFeature->userManager();
    arangodb::aql::QueryRegistry queryRegistry(0);  // required for UserManager::loadFromDB()
    userManager->setQueryRegistry(&queryRegistry);
    auto resetUserManager = std::shared_ptr<arangodb::auth::UserManager>(
        userManager,
        [](arangodb::auth::UserManager* ptr) -> void { ptr->removeAllUsers(); });

    // subsequent update (overwrite) not authorised (NONE collection)
    {
      arangodb::auth::UserMap userMap;
      auto& user =
          userMap
              .emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP))
              .first->second;
      user.grantCollection(vocbase->name(), "testCollection", arangodb::auth::Level::NONE);  // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database

      EXPECT_TRUE((TRI_ERROR_FORBIDDEN ==
                   logicalView->properties(viewUpdateJson->slice(), true).errorNumber()));
      logicalCollection = ci->getCollection(vocbase->name(), collectionId);
      ASSERT_TRUE((false == !logicalCollection));
      logicalView = ci->getView(vocbase->name(), viewId);
      ASSERT_TRUE((false == !logicalView));
      EXPECT_TRUE((false == logicalCollection->getIndexes().empty()));
      EXPECT_TRUE((false == logicalView->visitCollections(
                                [](TRI_voc_cid_t) -> bool { return false; })));
    }

    // subsequent update (overwrite) authorised (RO collection)
    {
      arangodb::auth::UserMap userMap;
      auto& user =
          userMap
              .emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP))
              .first->second;
      user.grantCollection(vocbase->name(), "testCollection", arangodb::auth::Level::RO);  // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database

      EXPECT_TRUE((logicalView->properties(viewUpdateJson->slice(), true).ok()));
      logicalCollection = ci->getCollection(vocbase->name(), collectionId);
      ASSERT_TRUE((false == !logicalCollection));
      logicalView = ci->getView(vocbase->name(), viewId);
      ASSERT_TRUE((false == !logicalView));
      EXPECT_TRUE((true == logicalCollection->getIndexes().empty()));
      EXPECT_TRUE((true == logicalView->visitCollections(
                               [](TRI_voc_cid_t) -> bool { return false; })));
    }
  }

  // add authorised link (existing collection not authorized)
  {
    auto collection0Json = arangodb::velocypack::Parser::fromJson(
        "{ \"id\": \"1\", \"planId\": \"1\",  \"name\": \"testCollection0\", "
        "\"replicationFactor\": 1, \"shards\":{} }");
    auto collection1Json = arangodb::velocypack::Parser::fromJson(
        "{ \"id\": \"2\", \"planId\": \"2\",  \"name\": \"testCollection1\", "
        "\"replicationFactor\": 1, \"shards\":{} }");
    auto viewCreateJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testView\", \"id\": \"42\", \"type\": "
        "\"arangosearch\" }");
    auto viewUpdateJson = arangodb::velocypack::Parser::fromJson(
        "{ \"links\": { \"testCollection1\": {} } }");
    auto collectionId0 = std::to_string(1);
    auto collectionId1 = std::to_string(2);
    auto viewId = std::to_string(42);

    EXPECT_TRUE((ci->createCollectionCoordinator(vocbase->name(), collectionId0, 0, 1,
                                                 1, false, collection0Json->slice(), 0.0)
                     .ok()));
    auto logicalCollection0 = ci->getCollection(vocbase->name(), collectionId0);
    ASSERT_TRUE((false == !logicalCollection0));
    auto dropLogicalCollection0 = std::shared_ptr<arangodb::ClusterInfo>(
        ci, [vocbase, &collectionId0](arangodb::ClusterInfo* ci) -> void {
          ci->dropCollectionCoordinator(vocbase->name(), collectionId0, 0);
        });
    EXPECT_TRUE((ci->createCollectionCoordinator(vocbase->name(), collectionId1, 0, 1,
                                                 1, false, collection1Json->slice(), 0.0)
                     .ok()));
    auto logicalCollection1 = ci->getCollection(vocbase->name(), collectionId1);
    ASSERT_TRUE((false == !logicalCollection1));
    auto dropLogicalCollection1 = std::shared_ptr<arangodb::ClusterInfo>(
        ci, [vocbase, &collectionId1](arangodb::ClusterInfo* ci) -> void {
          ci->dropCollectionCoordinator(vocbase->name(), collectionId1, 0);
        });
    EXPECT_TRUE(
        (ci->createViewCoordinator(vocbase->name(), viewId, viewCreateJson->slice())
             .ok()));
    auto logicalView = ci->getView(vocbase->name(), viewId);
    ASSERT_TRUE((false == !logicalView));
    auto dropLogicalView = std::shared_ptr<arangodb::ClusterInfo>(
        ci, [vocbase, &viewId](arangodb::ClusterInfo* ci) -> void {
          ci->dropViewCoordinator(vocbase->name(), viewId);
        });

    EXPECT_TRUE((true == logicalCollection0->getIndexes().empty()));
    EXPECT_TRUE((true == logicalCollection1->getIndexes().empty()));
    EXPECT_TRUE((true == logicalView->visitCollections(
                             [](TRI_voc_cid_t) -> bool { return false; })));

    // initial link creation
    {
      // simulate heartbeat thread (create index in current)
      {
        auto const path = "/Current/Collections/" + vocbase->name() + "/" +
                          std::to_string(logicalCollection0->id());
        auto const value = arangodb::velocypack::Parser::fromJson(
            "{ \"shard-id-does-not-matter\": { \"indexes\" : [ { \"id\": "
            "\"3\" } ] } }");
        EXPECT_TRUE(arangodb::AgencyComm().setValue(path, value->slice(), 0.0).successful());
      }

      auto updateJson = arangodb::velocypack::Parser::fromJson(
          "{ \"links\": { \"testCollection0\": {} } }");
      EXPECT_TRUE((logicalView->properties(updateJson->slice(), true).ok()));
      logicalCollection0 = ci->getCollection(vocbase->name(), collectionId0);
      ASSERT_TRUE((false == !logicalCollection0));
      logicalCollection1 = ci->getCollection(vocbase->name(), collectionId1);
      ASSERT_TRUE((false == !logicalCollection1));
      logicalView = ci->getView(vocbase->name(), viewId);
      ASSERT_TRUE((false == !logicalView));
      EXPECT_TRUE((false == logicalCollection0->getIndexes().empty()));
      EXPECT_TRUE((true == logicalCollection1->getIndexes().empty()));
      EXPECT_TRUE((false == logicalView->visitCollections(
                                [](TRI_voc_cid_t) -> bool { return false; })));

      // simulate heartbeat thread (update index in current)
      {
        auto const path0 = "/Current/Collections/" + vocbase->name() + "/" +
                           std::to_string(logicalCollection0->id());
        auto const path1 = "/Current/Collections/" + vocbase->name() + "/" +
                           std::to_string(logicalCollection1->id());
        auto const value = arangodb::velocypack::Parser::fromJson(
            "{ \"shard-id-does-not-matter\": { \"indexes\" : [ { \"id\": "
            "\"4\" } ] } }");
        EXPECT_TRUE(arangodb::AgencyComm().removeValues(path0, false).successful());
        EXPECT_TRUE(arangodb::AgencyComm().setValue(path1, value->slice(), 0.0).successful());
      }
    }

    struct ExecContext : public arangodb::ExecContext {
      ExecContext()
          : arangodb::ExecContext(arangodb::ExecContext::Type::Default, "", "",
                                  arangodb::auth::Level::NONE,
                                  arangodb::auth::Level::NONE) {}
    } execContext;
    arangodb::ExecContextScope execContextScope(&execContext);
    auto* authFeature = arangodb::AuthenticationFeature::instance();
    auto* userManager = authFeature->userManager();
    arangodb::aql::QueryRegistry queryRegistry(0);  // required for UserManager::loadFromDB()
    userManager->setQueryRegistry(&queryRegistry);
    auto resetUserManager = std::shared_ptr<arangodb::auth::UserManager>(
        userManager,
        [](arangodb::auth::UserManager* ptr) -> void { ptr->removeAllUsers(); });

    // subsequent update (overwrite) not authorised (NONE collection)
    {
      arangodb::auth::UserMap userMap;
      auto& user =
          userMap
              .emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP))
              .first->second;
      user.grantCollection(vocbase->name(),
                           "testCollection0", arangodb::auth::Level::NONE);  // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      user.grantCollection(vocbase->name(), "testCollection1", arangodb::auth::Level::RO);  // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database

      EXPECT_TRUE((TRI_ERROR_FORBIDDEN ==
                   logicalView->properties(viewUpdateJson->slice(), true).errorNumber()));
      logicalCollection0 = ci->getCollection(vocbase->name(), collectionId0);
      ASSERT_TRUE((false == !logicalCollection0));
      logicalCollection1 = ci->getCollection(vocbase->name(), collectionId1);
      ASSERT_TRUE((false == !logicalCollection1));
      logicalView = ci->getView(vocbase->name(), viewId);
      ASSERT_TRUE((false == !logicalView));
      EXPECT_TRUE((false == logicalCollection0->getIndexes().empty()));
      EXPECT_TRUE((true == logicalCollection1->getIndexes().empty()));
      EXPECT_TRUE((false == logicalView->visitCollections(
                                [](TRI_voc_cid_t) -> bool { return false; })));
    }

    // subsequent update (overwrite) authorised (RO collection)
    {
      arangodb::auth::UserMap userMap;
      auto& user =
          userMap
              .emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP))
              .first->second;
      user.grantCollection(vocbase->name(), "testCollection0", arangodb::auth::Level::RO);  // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      user.grantCollection(vocbase->name(), "testCollection1", arangodb::auth::Level::RO);  // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database

      EXPECT_TRUE((logicalView->properties(viewUpdateJson->slice(), true).ok()));
      logicalCollection0 = ci->getCollection(vocbase->name(), collectionId0);
      ASSERT_TRUE((false == !logicalCollection0));
      logicalCollection1 = ci->getCollection(vocbase->name(), collectionId1);
      ASSERT_TRUE((false == !logicalCollection1));
      logicalView = ci->getView(vocbase->name(), viewId);
      ASSERT_TRUE((false == !logicalView));
      EXPECT_TRUE((false == logicalCollection0->getIndexes().empty()));
      EXPECT_TRUE((false == logicalCollection1->getIndexes().empty()));
      EXPECT_TRUE((false == logicalView->visitCollections(
                                [](TRI_voc_cid_t) -> bool { return false; })));
    }
  }

  // drop authorised link (existing collection not authorized)
  {
    auto collection0Json = arangodb::velocypack::Parser::fromJson(
        "{ \"id\": \"1\", \"planId\": \"1\",  \"name\": \"testCollection0\", "
        "\"replicationFactor\": 1, \"shards\":{} }");
    auto collection1Json = arangodb::velocypack::Parser::fromJson(
        "{ \"id\": \"2\", \"planId\": \"2\",  \"name\": \"testCollection1\", "
        "\"replicationFactor\": 1, \"shards\":{} }");
    auto viewCreateJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testView\", \"id\": \"42\", \"type\": "
        "\"arangosearch\" }");
    auto viewUpdateJson = arangodb::velocypack::Parser::fromJson(
        "{ \"links\": { \"testCollection1\": null } }");
    auto collectionId0 = std::to_string(1);
    auto collectionId1 = std::to_string(2);
    auto viewId = std::to_string(42);

    EXPECT_TRUE((ci->createCollectionCoordinator(vocbase->name(), collectionId0, 0, 1,
                                                 1, false, collection0Json->slice(), 0.0)
                     .ok()));
    auto logicalCollection0 = ci->getCollection(vocbase->name(), collectionId0);
    ASSERT_TRUE((false == !logicalCollection0));
    auto dropLogicalCollection0 = std::shared_ptr<arangodb::ClusterInfo>(
        ci, [vocbase, &collectionId0](arangodb::ClusterInfo* ci) -> void {
          ci->dropCollectionCoordinator(vocbase->name(), collectionId0, 0);
        });
    EXPECT_TRUE((ci->createCollectionCoordinator(vocbase->name(), collectionId1, 0, 1,
                                                 1, false, collection1Json->slice(), 0.0)
                     .ok()));
    auto logicalCollection1 = ci->getCollection(vocbase->name(), collectionId1);
    ASSERT_TRUE((false == !logicalCollection1));
    auto dropLogicalCollection1 = std::shared_ptr<arangodb::ClusterInfo>(
        ci, [vocbase, &collectionId1](arangodb::ClusterInfo* ci) -> void {
          ci->dropCollectionCoordinator(vocbase->name(), collectionId1, 0);
        });
    EXPECT_TRUE(
        (ci->createViewCoordinator(vocbase->name(), viewId, viewCreateJson->slice())
             .ok()));
    auto logicalView = ci->getView(vocbase->name(), viewId);
    ASSERT_TRUE((false == !logicalView));
    auto dropLogicalView = std::shared_ptr<arangodb::ClusterInfo>(
        ci, [vocbase, &viewId](arangodb::ClusterInfo* ci) -> void {
          ci->dropViewCoordinator(vocbase->name(), viewId);
        });

    EXPECT_TRUE((true == logicalCollection0->getIndexes().empty()));
    EXPECT_TRUE((true == logicalCollection1->getIndexes().empty()));
    EXPECT_TRUE((true == logicalView->visitCollections(
                             [](TRI_voc_cid_t) -> bool { return false; })));

    // initial link creation
    {
      // simulate heartbeat thread (create index in current)
      {
        auto const path0 = "/Current/Collections/" + vocbase->name() + "/" +
                           std::to_string(logicalCollection0->id());
        auto const path1 = "/Current/Collections/" + vocbase->name() + "/" +
                           std::to_string(logicalCollection1->id());
        auto const value0 = arangodb::velocypack::Parser::fromJson(
            "{ \"shard-id-does-not-matter\": { \"indexes\" : [ { \"id\": "
            "\"5\" } ] } }");
        auto const value1 = arangodb::velocypack::Parser::fromJson(
            "{ \"shard-id-does-not-matter\": { \"indexes\" : [ { \"id\": "
            "\"6\" } ] } }");
        EXPECT_TRUE(arangodb::AgencyComm().setValue(path0, value0->slice(), 0.0).successful());
        EXPECT_TRUE(arangodb::AgencyComm().setValue(path1, value1->slice(), 0.0).successful());
      }

      auto updateJson = arangodb::velocypack::Parser::fromJson(
          "{ \"links\": { \"testCollection0\": {}, \"testCollection1\": {} } "
          "}");
      EXPECT_TRUE((logicalView->properties(updateJson->slice(), true).ok()));
      logicalCollection0 = ci->getCollection(vocbase->name(), collectionId0);
      ASSERT_TRUE((false == !logicalCollection0));
      logicalCollection1 = ci->getCollection(vocbase->name(), collectionId1);
      ASSERT_TRUE((false == !logicalCollection1));
      logicalView = ci->getView(vocbase->name(), viewId);
      ASSERT_TRUE((false == !logicalView));
      EXPECT_TRUE((false == logicalCollection0->getIndexes().empty()));
      EXPECT_TRUE((false == logicalCollection1->getIndexes().empty()));
      EXPECT_TRUE((false == logicalView->visitCollections(
                                [](TRI_voc_cid_t) -> bool { return false; })));

      // simulate heartbeat thread (remove index from current)
      {
        auto const path = "/Current/Collections/" + vocbase->name() + "/" +
                          std::to_string(logicalCollection1->id()) +
                          "/shard-id-does-not-matter/indexes";
        EXPECT_TRUE(arangodb::AgencyComm().removeValues(path, false).successful());
      }
    }

    struct ExecContext : public arangodb::ExecContext {
      ExecContext()
          : arangodb::ExecContext(arangodb::ExecContext::Type::Default, "", "",
                                  arangodb::auth::Level::NONE,
                                  arangodb::auth::Level::NONE) {}
    } execContext;
    arangodb::ExecContextScope execContextScope(&execContext);
    auto* authFeature = arangodb::AuthenticationFeature::instance();
    auto* userManager = authFeature->userManager();
    arangodb::aql::QueryRegistry queryRegistry(0);  // required for UserManager::loadFromDB()
    userManager->setQueryRegistry(&queryRegistry);
    auto resetUserManager = std::shared_ptr<arangodb::auth::UserManager>(
        userManager,
        [](arangodb::auth::UserManager* ptr) -> void { ptr->removeAllUsers(); });

    // subsequent update (overwrite) not authorised (NONE collection)
    {
      arangodb::auth::UserMap userMap;
      auto& user =
          userMap
              .emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP))
              .first->second;
      user.grantCollection(vocbase->name(),
                           "testCollection0", arangodb::auth::Level::NONE);  // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      user.grantCollection(vocbase->name(), "testCollection1", arangodb::auth::Level::RO);  // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database

      EXPECT_TRUE((TRI_ERROR_FORBIDDEN ==
                   logicalView->properties(viewUpdateJson->slice(), true).errorNumber()));
      logicalCollection0 = ci->getCollection(vocbase->name(), collectionId0);
      ASSERT_TRUE((false == !logicalCollection0));
      logicalCollection1 = ci->getCollection(vocbase->name(), collectionId1);
      ASSERT_TRUE((false == !logicalCollection1));
      logicalView = ci->getView(vocbase->name(), viewId);
      ASSERT_TRUE((false == !logicalView));
      EXPECT_TRUE((false == logicalCollection0->getIndexes().empty()));
      EXPECT_TRUE((false == logicalCollection1->getIndexes().empty()));
      EXPECT_TRUE((false == logicalView->visitCollections(
                                [](TRI_voc_cid_t) -> bool { return false; })));
    }

    // subsequent update (overwrite) authorised (RO collection)
    {
      arangodb::auth::UserMap userMap;
      auto& user =
          userMap
              .emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP))
              .first->second;
      user.grantCollection(vocbase->name(), "testCollection0", arangodb::auth::Level::RO);  // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      user.grantCollection(vocbase->name(), "testCollection1", arangodb::auth::Level::RO);  // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database

      EXPECT_TRUE((logicalView->properties(viewUpdateJson->slice(), true).ok()));
      logicalCollection0 = ci->getCollection(vocbase->name(), collectionId0);
      ASSERT_TRUE((false == !logicalCollection0));
      logicalCollection1 = ci->getCollection(vocbase->name(), collectionId1);
      ASSERT_TRUE((false == !logicalCollection1));
      logicalView = ci->getView(vocbase->name(), viewId);
      ASSERT_TRUE((false == !logicalView));
      EXPECT_TRUE((false == logicalCollection0->getIndexes().empty()));
      EXPECT_TRUE((true == logicalCollection1->getIndexes().empty()));
      EXPECT_TRUE((false == logicalView->visitCollections(
                                [](TRI_voc_cid_t) -> bool { return false; })));
    }
  }
}

TEST_F(IResearchViewCoordinatorTest, IResearchViewNode_createBlock) {
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

    EXPECT_TRUE((ci->createDatabaseCoordinator(vocbase->name(), VPackSlice::emptyObjectSlice(), 0.0)
                     .ok()));
  }

  // create and drop view (no id specified)
  {
    auto json = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
    auto viewId = std::to_string(ci->uniqid() + 1);  // +1 because LogicalView creation will generate a new ID

    EXPECT_TRUE(
        (ci->createViewCoordinator(vocbase->name(), viewId, json->slice()).ok()));

    // get current plan version
    auto planVersion = arangodb::tests::getCurrentPlanVersion();

    auto view = ci->getView(vocbase->name(), viewId);
    EXPECT_TRUE(nullptr != view);
    EXPECT_TRUE(nullptr !=
                std::dynamic_pointer_cast<arangodb::iresearch::IResearchViewCoordinator>(view));
    EXPECT_TRUE(planVersion == view->planVersion());
    EXPECT_TRUE("testView" == view->name());
    EXPECT_TRUE(false == view->deleted());
    EXPECT_TRUE(viewId == std::to_string(view->id()));
    EXPECT_TRUE(arangodb::iresearch::DATA_SOURCE_TYPE == view->type());
    EXPECT_TRUE(arangodb::LogicalView::category() == view->category());
    EXPECT_TRUE(vocbase == &view->vocbase());

    // dummy query
    arangodb::aql::Query query(false, *vocbase, arangodb::aql::QueryString("RETURN 1"),
                               nullptr, arangodb::velocypack::Parser::fromJson("{}"),
                               arangodb::aql::PART_MAIN);
    query.prepare(arangodb::QueryRegistryFeature::registry());

    arangodb::aql::SingletonNode singleton(query.plan(), 0);

    arangodb::aql::Variable const outVariable("variable", 0);

    arangodb::iresearch::IResearchViewNode node(*query.plan(),
                                                42,        // id
                                                *vocbase,  // database
                                                view,      // view
                                                outVariable,
                                                nullptr,  // no filter condition
                                                nullptr,  // no options
                                                {}        // no sort condition
    );
    node.addDependency(&singleton);

    arangodb::aql::ExecutionEngine engine(&query);
    std::unordered_map<arangodb::aql::ExecutionNode*, arangodb::aql::ExecutionBlock*> cache;
    singleton.setVarUsageValid();
    node.setVarUsageValid();
    singleton.planRegisters(nullptr);
    node.planRegisters(nullptr);
    auto singletonBlock = singleton.createBlock(engine, cache);
    auto execBlock = node.createBlock(engine, cache);
    EXPECT_TRUE(nullptr != execBlock);
    EXPECT_TRUE(nullptr !=
                dynamic_cast<arangodb::aql::ExecutionBlockImpl<arangodb::aql::NoResultsExecutor>*>(
                    execBlock.get()));

    // drop view
    EXPECT_TRUE(view->drop().ok());
    EXPECT_TRUE(planVersion < arangodb::tests::getCurrentPlanVersion());

    // check there is no more view
    EXPECT_TRUE(nullptr == ci->getView(vocbase->name(), view->name()));
  }
}
