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

#include "gtest/gtest.h"

#include "common.h"
#include "AgencyMock.h"
#include "../Mocks/StorageEngineMock.h"

#include "utils/misc.hpp"
#include "utils/string.hpp"
#include "utils/thread_utils.hpp"
#include "utils/utf8_path.hpp"
#include "utils/version_defines.hpp"

#include "Agency/Store.h"
#include "ApplicationFeatures/CommunicationPhase.h"
#include "Aql/AqlFunctionFeature.h"
#include "Cluster/ClusterComm.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"

#if USE_ENTERPRISE
  #include "Enterprise/Ldap/LdapFeature.h"
#endif

#include "GeneralServer/AuthenticationFeature.h"
#include "IResearch/ApplicationServerHelper.h"
#include "IResearch/Containers.h"
#include "IResearch/IResearchCommon.h"
#include "IResearch/IResearchAnalyzerFeature.h"
#include "IResearch/IResearchFeature.h"
#include "IResearch/IResearchLinkCoordinator.h"
#include "IResearch/IResearchLinkHelper.h"
#include "IResearch/IResearchMMFilesLink.h"
#include "IResearch/IResearchView.h"
#include "Rest/Version.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/DatabasePathFeature.h"
#include "RestServer/FlushFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "RestServer/SystemDatabaseFeature.h"
#include "RestServer/UpgradeFeature.h"
#include "RestServer/ViewTypesFeature.h"
#include "Sharding/ShardingFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/Methods/Indexes.h"
#include "VocBase/Methods/Upgrade.h"

static const VPackBuilder systemDatabaseBuilder = dbArgsBuilder();
static const VPackSlice   systemDatabaseArgs = systemDatabaseBuilder.slice();
static const VPackBuilder testDatabaseBuilder = dbArgsBuilder("testVocbase");
static const VPackSlice   testDatabaseArgs = testDatabaseBuilder.slice();
// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

class IResearchFeatureTest : public ::testing::Test {
 protected:
  struct ClusterCommControl : arangodb::ClusterComm {
    static void reset() {
      arangodb::ClusterComm::_theInstanceInit.store(0);
    }
  };

  arangodb::consensus::Store _agencyStore{nullptr, "arango"};
  GeneralClientConnectionAgencyMock* agency;
  StorageEngineMock engine;
  arangodb::application_features::ApplicationServer server;

  IResearchFeatureTest(): engine(server), server(nullptr, nullptr) {
    auto* agencyCommManager = new AgencyCommManagerMock("arango");
    agency = agencyCommManager->addConnection<GeneralClientConnectionAgencyMock>(_agencyStore);
    agency = agencyCommManager->addConnection<GeneralClientConnectionAgencyMock>(_agencyStore); // need 2 connections or Agency callbacks will fail
    arangodb::AgencyCommManager::MANAGER.reset(agencyCommManager); // required for Coordinator tests

    arangodb::EngineSelectorFeature::ENGINE = &engine;

    arangodb::tests::init();

    // suppress INFO {authentication} Authentication is turned on (system only), authentication for unix sockets is turned on
    // suppress WARNING {authentication} --server.jwt-secret is insecure. Use --server.jwt-secret-keyfile instead
    arangodb::LogTopic::setLogLevel(arangodb::Logger::AUTHENTICATION.name(), arangodb::LogLevel::ERR);

    // suppress log messages since tests check error conditions
    arangodb::LogTopic::setLogLevel(arangodb::Logger::AGENCY.name(), arangodb::LogLevel::FATAL);
    arangodb::LogTopic::setLogLevel(arangodb::Logger::CLUSTER.name(), arangodb::LogLevel::FATAL);
    arangodb::LogTopic::setLogLevel(arangodb::iresearch::TOPIC.name(), arangodb::LogLevel::FATAL);
  }

  ~IResearchFeatureTest() {
    ClusterCommControl::reset();
    arangodb::LogTopic::setLogLevel(arangodb::iresearch::TOPIC.name(), arangodb::LogLevel::DEFAULT);
    arangodb::LogTopic::setLogLevel(arangodb::Logger::CLUSTER.name(), arangodb::LogLevel::DEFAULT);
    arangodb::LogTopic::setLogLevel(arangodb::Logger::AUTHENTICATION.name(), arangodb::LogLevel::DEFAULT);
    arangodb::LogTopic::setLogLevel(arangodb::Logger::AGENCY.name(), arangodb::LogLevel::DEFAULT);
    arangodb::application_features::ApplicationServer::server = nullptr;
    arangodb::EngineSelectorFeature::ENGINE = nullptr;
    arangodb::AgencyCommManager::MANAGER.reset();
  }
};

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

TEST_F(IResearchFeatureTest, test_start) {
  // create a new instance of an ApplicationServer and fill it with the required features
  // cannot use the existing server since its features already have some state
  std::shared_ptr<arangodb::application_features::ApplicationServer> originalServer(
    arangodb::application_features::ApplicationServer::server,
    [](arangodb::application_features::ApplicationServer* ptr)->void {
      arangodb::application_features::ApplicationServer::server = ptr;
    }
  );
  arangodb::application_features::ApplicationServer::server = nullptr; // avoid "ApplicationServer initialized twice"
  arangodb::application_features::ApplicationServer server(nullptr, nullptr);
  auto* functions = new arangodb::aql::AqlFunctionFeature(server);
  arangodb::iresearch::IResearchFeature iresearch(server);
  auto cleanup = irs::make_finally([functions]()->void{ functions->unprepare(); });

  enum class FunctionType {
    FILTER = 0,
    SCORER
  };

  std::map<irs::string_ref, std::pair<irs::string_ref, FunctionType>> expected = {
    // filter functions
    { "EXISTS", { ".|.,.", FunctionType::FILTER } },
    { "PHRASE", { ".,.|.+", FunctionType::FILTER } },
    { "STARTS_WITH", { ".,.|.", FunctionType::FILTER } },
    { "MIN_MATCH", { ".,.|.+", FunctionType::FILTER } },

    // context functions
    { "ANALYZER", { ".,.", FunctionType::FILTER } },
    { "BOOST", { ".,.", FunctionType::FILTER } },

    // scorer functions
    { "BM25", { ".|+", FunctionType::SCORER } },
    { "TFIDF", { ".|+", FunctionType::SCORER } },
  };

  server.addFeature(functions);
  functions->prepare();

  for(auto& entry: expected) {
    auto* function = arangodb::iresearch::getFunction(*functions, entry.first);
    EXPECT_TRUE((nullptr == function));
  };

  iresearch.start();

  for(auto& entry: expected) {
    auto* function = arangodb::iresearch::getFunction(*functions, entry.first);
    EXPECT_TRUE((nullptr != function));
    EXPECT_TRUE((entry.second.first == function->arguments));
    EXPECT_TRUE((
            (entry.second.second == FunctionType::FILTER && arangodb::iresearch::isFilter(*function))
            || (entry.second.second == FunctionType::SCORER && arangodb::iresearch::isScorer(*function))
    ));
  };
}

TEST_F(IResearchFeatureTest, test_upgrade0_1) {
  // version 0 data-source path
  auto getPersistedPath0 = [](arangodb::LogicalView const& view)->irs::utf8_path {
    auto* dbPathFeature = arangodb::application_features::ApplicationServer::lookupFeature<arangodb::DatabasePathFeature>("DatabasePath");
    EXPECT_TRUE(dbPathFeature);
    irs::utf8_path dataPath(dbPathFeature->directory());
    dataPath /= "databases";
    dataPath /= "database-";
    dataPath += std::to_string(view.vocbase().id());
    dataPath /= arangodb::iresearch::DATA_SOURCE_TYPE.name();
    dataPath += "-";
    dataPath += std::to_string(view.id());
    return dataPath;
  };

  // version 1 data-source path
  auto getPersistedPath1 = [](arangodb::iresearch::IResearchLink const& link)->irs::utf8_path {
    auto* dbPathFeature = arangodb::application_features::ApplicationServer::lookupFeature<arangodb::DatabasePathFeature>("DatabasePath");
    EXPECT_TRUE(dbPathFeature);
    irs::utf8_path dataPath(dbPathFeature->directory());
    dataPath /= "databases";
    dataPath /= "database-";
    dataPath += std::to_string(link.collection().vocbase().id());
    dataPath /= arangodb::iresearch::DATA_SOURCE_TYPE.name();
    dataPath += "-";
    dataPath += std::to_string(link.collection().id());
    dataPath += "_";
    dataPath += std::to_string(link.id());
    return dataPath;
  };

  // test single-server (no directory)
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
    auto linkJson = arangodb::velocypack::Parser::fromJson("{ \"view\": \"testView\", \"type\": \"arangosearch\", \"includeAllFields\": true }");
    auto viewJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\", \"version\": 0 }");
    auto versionJson = arangodb::velocypack::Parser::fromJson("{ \"version\": 0, \"tasks\": {} }");

    // create a new instance of an ApplicationServer and fill it with the required features
    // cannot use the existing server since its features already have some state
    std::shared_ptr<arangodb::application_features::ApplicationServer> originalServer(
      arangodb::application_features::ApplicationServer::server,
      [](arangodb::application_features::ApplicationServer* ptr)->void {
        arangodb::application_features::ApplicationServer::server = ptr;
      }
    );
    arangodb::application_features::ApplicationServer::server = nullptr; // avoid "ApplicationServer initialized twice"
    arangodb::application_features::ApplicationServer server(nullptr, nullptr);
    arangodb::iresearch::IResearchFeature feature(server);
    arangodb::iresearch::IResearchAnalyzerFeature* analyzerFeature{};
    arangodb::DatabasePathFeature* dbPathFeature;
    server.addFeature(new arangodb::FlushFeature(server)); // required to skip IResearchView validation
    server.addFeature(new arangodb::DatabaseFeature(server)); // required to skip IResearchView validation
    server.addFeature(dbPathFeature = new arangodb::DatabasePathFeature(server)); // required for IResearchLink::initDataStore()
    server.addFeature(analyzerFeature = new arangodb::iresearch::IResearchAnalyzerFeature(server)); // required for restoring link analyzers
    server.addFeature(new arangodb::QueryRegistryFeature(server)); // required for constructing TRI_vocbase_t
    TRI_vocbase_t system(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 0, systemDatabaseArgs);
    server.addFeature(new arangodb::SystemDatabaseFeature(server, &system)); // required for IResearchAnalyzerFeature::start()
    server.addFeature(new arangodb::UpgradeFeature(server, nullptr, {})); // required for upgrade tasks
    server.addFeature(new arangodb::ViewTypesFeature(server)); // required for IResearchFeature::prepare()
    analyzerFeature->prepare(); // add static analyzers
    feature.prepare(); // register iresearch view type
    feature.start(); // register upgrade tasks
    server.getFeature<arangodb::DatabaseFeature>("Database")->enableUpgrade(); // skip IResearchView validation

    arangodb::tests::setDatabasePath(*dbPathFeature); // ensure test data is stored in a unique directory
    auto versionFilename = StorageEngineMock::versionFilenameResult;
    auto versionFilenameRestore = irs::make_finally([&versionFilename]()->void { StorageEngineMock::versionFilenameResult = versionFilename; });
    StorageEngineMock::versionFilenameResult = (irs::utf8_path(dbPathFeature->directory()) /= "version").utf8();
    ASSERT_TRUE((irs::utf8_path(dbPathFeature->directory()).mkdir()));
    ASSERT_TRUE((arangodb::basics::VelocyPackHelper::velocyPackToFile(StorageEngineMock::versionFilenameResult, versionJson->slice(), false)));

    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, testDatabaseArgs);
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    ASSERT_TRUE((false == !logicalCollection));
    auto logicalView0 = vocbase.createView(viewJson->slice());
    ASSERT_TRUE((false == !logicalView0));
    bool created;
    auto index = logicalCollection->createIndex(linkJson->slice(), created);
    ASSERT_TRUE((created));
    ASSERT_TRUE((false == !index));
    auto link0 = std::dynamic_pointer_cast<arangodb::iresearch::IResearchLink>(index);
    ASSERT_TRUE((false == !link0));

    index->unload(); // release file handles
    bool result;
    auto linkDataPath = getPersistedPath1(*link0);
    EXPECT_TRUE((linkDataPath.remove())); // remove link directory
    auto viewDataPath = getPersistedPath0(*logicalView0);
    EXPECT_TRUE((viewDataPath.exists(result) && !result)); // ensure no view directory
    arangodb::velocypack::Builder builder;
    builder.openObject();
    EXPECT_TRUE((logicalView0
                     ->properties(builder, arangodb::LogicalDataSource::makeFlags(
                                               arangodb::LogicalDataSource::Serialize::Detailed,
                                               arangodb::LogicalDataSource::Serialize::ForPersistence))
                     .ok()));
    builder.close();
    EXPECT_TRUE((0 == builder.slice().get("version").getNumber<uint32_t>())); // ensure 'version == 0 before upgrade

    EXPECT_TRUE((arangodb::methods::Upgrade::startup(vocbase, true, false).ok())); // run upgrade
    auto logicalView1 = vocbase.lookupView(logicalView0->name());
    EXPECT_TRUE((false == !logicalView1)); // ensure view present after upgrade
    EXPECT_TRUE((logicalView0->id() == logicalView1->id())); // ensure same id for view
    auto link1 = arangodb::iresearch::IResearchLinkHelper::find(*logicalCollection, *logicalView1);
    EXPECT_TRUE((false == !link1)); // ensure link present after upgrade
    EXPECT_TRUE((link0->id() != link1->id())); // ensure new link
    linkDataPath = getPersistedPath1(*link1);
    EXPECT_TRUE((linkDataPath.exists(result) && result)); // ensure link directory created after upgrade
    EXPECT_TRUE((viewDataPath.exists(result) && !result)); // ensure view directory not present
    viewDataPath = getPersistedPath0(*logicalView1);
    EXPECT_TRUE((viewDataPath.exists(result) && !result)); // ensure view directory not created
    builder.clear();
    builder.openObject();
    EXPECT_TRUE((logicalView1
                     ->properties(builder, arangodb::LogicalDataSource::makeFlags(
                                               arangodb::LogicalDataSource::Serialize::Detailed,
                                               arangodb::LogicalDataSource::Serialize::ForPersistence))
                     .ok()));
    builder.close();
    EXPECT_TRUE((1 == builder.slice().get("version").getNumber<uint32_t>())); // ensure 'version == 1 after upgrade
  }

  // test single-server (with directory)
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
    auto linkJson = arangodb::velocypack::Parser::fromJson("{ \"view\": \"testView\", \"type\": \"arangosearch\", \"includeAllFields\": true }");
    auto viewJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\", \"version\": 0 }");
    auto versionJson = arangodb::velocypack::Parser::fromJson("{ \"version\": 0, \"tasks\": {} }");

    // create a new instance of an ApplicationServer and fill it with the required features
    // cannot use the existing server since its features already have some state
    std::shared_ptr<arangodb::application_features::ApplicationServer> originalServer(
      arangodb::application_features::ApplicationServer::server,
      [](arangodb::application_features::ApplicationServer* ptr)->void {
        arangodb::application_features::ApplicationServer::server = ptr;
      }
    );
    arangodb::application_features::ApplicationServer::server = nullptr; // avoid "ApplicationServer initialized twice"
    arangodb::application_features::ApplicationServer server(nullptr, nullptr);
    arangodb::iresearch::IResearchFeature feature(server);
    arangodb::iresearch::IResearchAnalyzerFeature* analyzerFeature{};
    arangodb::DatabasePathFeature* dbPathFeature;
    server.addFeature(new arangodb::FlushFeature(server)); // required to skip IResearchView validation
    server.addFeature(new arangodb::DatabaseFeature(server)); // required to skip IResearchView validation
    server.addFeature(dbPathFeature = new arangodb::DatabasePathFeature(server)); // required for IResearchLink::initDataStore()
    server.addFeature(analyzerFeature = new arangodb::iresearch::IResearchAnalyzerFeature(server)); // required for restoring link analyzers
    server.addFeature(new arangodb::QueryRegistryFeature(server)); // required for constructing TRI_vocbase_t
    TRI_vocbase_t system(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 0, systemDatabaseArgs);
    server.addFeature(new arangodb::SystemDatabaseFeature(server, &system)); // required for IResearchLinkHelper::normalize(...)
    server.addFeature(new arangodb::UpgradeFeature(server, nullptr, {})); // required for upgrade tasks
    server.addFeature(new arangodb::ViewTypesFeature(server)); // required for IResearchFeature::prepare()
    analyzerFeature->prepare(); // add static analyzers
    feature.prepare(); // register iresearch view type
    feature.start(); // register upgrade tasks
    server.getFeature<arangodb::DatabaseFeature>("Database")->enableUpgrade(); // skip IResearchView validation

    arangodb::tests::setDatabasePath(*dbPathFeature); // ensure test data is stored in a unique directory
    auto versionFilename = StorageEngineMock::versionFilenameResult;
    auto versionFilenameRestore = irs::make_finally([&versionFilename]()->void { StorageEngineMock::versionFilenameResult = versionFilename; });
    StorageEngineMock::versionFilenameResult = (irs::utf8_path(dbPathFeature->directory()) /= "version").utf8();
    ASSERT_TRUE((irs::utf8_path(dbPathFeature->directory()).mkdir()));
    ASSERT_TRUE((arangodb::basics::VelocyPackHelper::velocyPackToFile(StorageEngineMock::versionFilenameResult, versionJson->slice(), false)));

    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, testDatabaseArgs);
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    ASSERT_TRUE((false == !logicalCollection));
    auto logicalView0 = vocbase.createView(viewJson->slice());
    ASSERT_TRUE((false == !logicalView0));
    bool created;
    auto index = logicalCollection->createIndex(linkJson->slice(), created);
    ASSERT_TRUE((created));
    ASSERT_TRUE((false == !index));
    auto link0 = std::dynamic_pointer_cast<arangodb::iresearch::IResearchLink>(index);
    ASSERT_TRUE((false == !link0));

    index->unload(); // release file handles
    bool result;
    auto linkDataPath = getPersistedPath1(*link0);
    EXPECT_TRUE((linkDataPath.remove())); // remove link directory
    auto viewDataPath = getPersistedPath0(*logicalView0);
    EXPECT_TRUE((viewDataPath.exists(result) && !result));
    EXPECT_TRUE((viewDataPath.mkdir())); // create view directory
    EXPECT_TRUE((viewDataPath.exists(result) && result));
    arangodb::velocypack::Builder builder;
    builder.openObject();
    EXPECT_TRUE((logicalView0
                     ->properties(builder, arangodb::LogicalDataSource::makeFlags(
                                               arangodb::LogicalDataSource::Serialize::Detailed,
                                               arangodb::LogicalDataSource::Serialize::ForPersistence))
                     .ok()));
    builder.close();
    EXPECT_TRUE((0 == builder.slice().get("version").getNumber<uint32_t>())); // ensure 'version == 0 before upgrade

    EXPECT_TRUE((arangodb::methods::Upgrade::startup(vocbase, true, false).ok())); // run upgrade
    auto logicalView1 = vocbase.lookupView(logicalView0->name());
    EXPECT_TRUE((false == !logicalView1)); // ensure view present after upgrade
    EXPECT_TRUE((logicalView0->id() == logicalView1->id())); // ensure same id for view
    auto link1 = arangodb::iresearch::IResearchLinkHelper::find(*logicalCollection, *logicalView1);
    EXPECT_TRUE((false == !link1)); // ensure link present after upgrade
    EXPECT_TRUE((link0->id() != link1->id())); // ensure new link
    linkDataPath = getPersistedPath1(*link1);
    EXPECT_TRUE((linkDataPath.exists(result) && result)); // ensure link directory created after upgrade
    EXPECT_TRUE((viewDataPath.exists(result) && !result)); // ensure view directory removed after upgrade
    viewDataPath = getPersistedPath0(*logicalView1);
    EXPECT_TRUE((viewDataPath.exists(result) && !result)); // ensure view directory not created
    builder.clear();
    builder.openObject();
    EXPECT_TRUE((logicalView1
                     ->properties(builder, arangodb::LogicalDataSource::makeFlags(
                                               arangodb::LogicalDataSource::Serialize::Detailed,
                                               arangodb::LogicalDataSource::Serialize::ForPersistence))
                     .ok()));
    builder.close();
    EXPECT_TRUE((1 == builder.slice().get("version").getNumber<uint32_t>())); // ensure 'version == 1 after upgrade

    server.getFeature<arangodb::DatabaseFeature>("Database")->unprepare();
  }

  // test coordinator
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"id\": \"1\", \"name\": \"testCollection\", \"shards\":{} }");
    auto linkJson = arangodb::velocypack::Parser::fromJson("{ \"view\": \"testView\", \"type\": \"arangosearch\", \"includeAllFields\": true }");
    auto viewJson = arangodb::velocypack::Parser::fromJson("{ \"id\": 42, \"name\": \"testView\", \"type\": \"arangosearch\", \"version\": 0 }");
    auto versionJson = arangodb::velocypack::Parser::fromJson("{ \"version\": 0, \"tasks\": {} }");
    auto collectionId = std::to_string(1);
    auto viewId = std::to_string(42);

    auto serverRoleBefore = arangodb::ServerState::instance()->getRole();
    arangodb::ServerState::instance()->setRole(arangodb::ServerState::ROLE_COORDINATOR);
    auto serverRoleRestore = irs::make_finally([&serverRoleBefore]()->void { arangodb::ServerState::instance()->setRole(serverRoleBefore); });

    // create a new instance of an ApplicationServer and fill it with the required features
    // cannot use the existing server since its features already have some state
    std::shared_ptr<arangodb::application_features::ApplicationServer> originalServer(
      arangodb::application_features::ApplicationServer::server,
      [](arangodb::application_features::ApplicationServer* ptr)->void {
        arangodb::application_features::ApplicationServer::server = ptr;
      }
    );
    arangodb::application_features::ApplicationServer::server = nullptr; // avoid "ApplicationServer initialized twice"
    arangodb::application_features::ApplicationServer server(nullptr, nullptr);
    arangodb::DatabaseFeature* database;
    arangodb::iresearch::IResearchFeature feature(server);
    arangodb::iresearch::IResearchAnalyzerFeature* analyzerFeature{};
    server.addFeature(new arangodb::FlushFeature(server)); // required for constructing TRI_vocbase_t
    server.addFeature(new arangodb::QueryRegistryFeature(server)); // required for constructing TRI_vocbase_t
    TRI_vocbase_t system(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 0, systemDatabaseArgs);
    server.addFeature(new arangodb::AuthenticationFeature(server)); // required for ClusterComm::instance()
    server.addFeature(new arangodb::ClusterFeature(server)); // required to create ClusterInfo instance
    server.addFeature(new arangodb::application_features::CommunicationFeaturePhase(server)); // required for SimpleHttpClient::doRequest()
    server.addFeature(database = new arangodb::DatabaseFeature(server)); // required to skip IResearchView validation
    server.addFeature(analyzerFeature = new arangodb::iresearch::IResearchAnalyzerFeature(server)); // required for restoring link analyzers
    server.addFeature(new arangodb::ShardingFeature(server)); // required for LogicalCollection::LogicalCollection(...)
    server.addFeature(new arangodb::SystemDatabaseFeature(server, &system)); // required for IResearchLinkHelper::normalize(...)
    server.addFeature(new arangodb::UpgradeFeature(server, nullptr, {})); // required for upgrade tasks
    server.addFeature(new arangodb::ViewTypesFeature(server)); // required for IResearchFeature::prepare()

    #if USE_ENTERPRISE
      server.addFeature(new arangodb::LdapFeature(server)); // required for AuthenticationFeature with USE_ENTERPRISE
    #endif

    analyzerFeature->prepare(); // add static analyzers
    feature.prepare(); // register iresearch view type
    feature.start(); // register upgrade tasks
    server.getFeature<arangodb::AuthenticationFeature>("Authentication")->prepare(); // create AuthenticationFeature::INSTANCE
    server.getFeature<arangodb::ClusterFeature>("Cluster")->prepare(); // create ClusterInfo instance
    server.getFeature<arangodb::DatabaseFeature>("Database")->enableUpgrade(); // skip IResearchView validation
    server.getFeature<arangodb::ShardingFeature>("Sharding")->prepare(); // register sharding types
    arangodb::AgencyCommManager::MANAGER->start(); // initialize agency
    arangodb::DatabaseFeature::DATABASE = database; // required for ClusterInfo::createCollectionCoordinator(...)
    const_cast<arangodb::IndexFactory&>(engine.indexFactory()).emplace( // required for Indexes::ensureIndex(...)
      arangodb::iresearch::DATA_SOURCE_TYPE.name(),
      arangodb::iresearch::IResearchLinkCoordinator::factory()
    );
    auto* ci = arangodb::ClusterInfo::instance();
    ASSERT_TRUE((nullptr != ci));
    TRI_vocbase_t* vocbase; // will be owned by DatabaseFeature

    ASSERT_TRUE((TRI_ERROR_NO_ERROR == database->createDatabase(1, "testDatabase", arangodb::velocypack::Slice::emptyObjectSlice(), vocbase)));
    ASSERT_TRUE((ci->createDatabaseCoordinator(vocbase->name(), arangodb::velocypack::Slice::emptyObjectSlice(), 0.0).ok()));
    ASSERT_TRUE((ci->createCollectionCoordinator(vocbase->name(), collectionId, 0, 1, false, collectionJson->slice(), 0.0).ok()));
    auto logicalCollection = ci->getCollection(vocbase->name(), collectionId);
    ASSERT_TRUE((false == !logicalCollection));
    EXPECT_TRUE((ci->createViewCoordinator(vocbase->name(), viewId, viewJson->slice()).ok()));
    auto logicalView0 = ci->getView(vocbase->name(), viewId);
    ASSERT_TRUE((false == !logicalView0));

    // simulate heartbeat thread (create index in current)
    {
      auto const path = "/Current/Collections/" + vocbase->name() + "/" + std::to_string(logicalCollection->id());
      auto const value = arangodb::velocypack::Parser::fromJson("{ \"shard-id-does-not-matter\": { \"indexes\" : [ { \"id\": \"1\" } ] } }");
      EXPECT_TRUE(arangodb::AgencyComm().setValue(path, value->slice(), 0.0).successful());
    }
    arangodb::velocypack::Builder tmp;
    ASSERT_TRUE((arangodb::methods::Indexes::ensureIndex(logicalCollection.get(), linkJson->slice(), true, tmp).ok()));
    logicalCollection = ci->getCollection(vocbase->name(), collectionId);
    ASSERT_TRUE((false == !logicalCollection));
    auto link0 = arangodb::iresearch::IResearchLinkHelper::find(*logicalCollection, *logicalView0);
    ASSERT_TRUE((false == !link0));

    arangodb::velocypack::Builder builder;
    builder.openObject();
    EXPECT_TRUE((logicalView0
                     ->properties(builder, arangodb::LogicalDataSource::makeFlags(
                                               arangodb::LogicalDataSource::Serialize::Detailed,
                                               arangodb::LogicalDataSource::Serialize::ForPersistence))
                     .ok()));
    builder.close();
    EXPECT_TRUE((0 == builder.slice().get("version").getNumber<uint32_t>())); // ensure 'version == 0 before upgrade

    // ensure no upgrade on coordinator
    // simulate heartbeat thread (create index in current)
    {
      auto const path = "/Current/Collections/" + vocbase->name() + "/" + std::to_string(logicalCollection->id());
      auto const value = arangodb::velocypack::Parser::fromJson("{ \"shard-id-does-not-matter\": { \"indexes\" : [ { \"id\": \"2\" } ] } }");
      EXPECT_TRUE(arangodb::AgencyComm().setValue(path, value->slice(), 0.0).successful());
    }
    EXPECT_TRUE((arangodb::methods::Upgrade::clusterBootstrap(*vocbase).ok())); // run upgrade
    auto logicalCollection2 = ci->getCollection(vocbase->name(), collectionId);
    ASSERT_TRUE((false == !logicalCollection2));
    auto logicalView1 = ci->getView(vocbase->name(), viewId);
    EXPECT_TRUE((false == !logicalView1)); // ensure view present after upgrade
    EXPECT_TRUE((logicalView0->id() == logicalView1->id())); // ensure same id for view
    auto link1 = arangodb::iresearch::IResearchLinkHelper::find(*logicalCollection2, *logicalView1);
    EXPECT_TRUE((false == !link1)); // ensure link present after upgrade
    EXPECT_TRUE((link0->id() == link1->id())); // ensure new link
    builder.clear();
    builder.openObject();
    EXPECT_TRUE((logicalView1
                     ->properties(builder, arangodb::LogicalDataSource::makeFlags(
                                               arangodb::LogicalDataSource::Serialize::Detailed,
                                               arangodb::LogicalDataSource::Serialize::ForPersistence))
                     .ok()));
    builder.close();
    EXPECT_TRUE((0 == builder.slice().get("version").getNumber<uint32_t>())); // ensure 'version == 0 after upgrade

    server.getFeature<arangodb::DatabaseFeature>("Database")->unprepare();
  }

  // test db-server (no directory)
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
    auto linkJson = arangodb::velocypack::Parser::fromJson("{ \"view\": \"testView\", \"type\": \"arangosearch\", \"includeAllFields\": true }");
    auto viewJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\", \"version\": 0 }");
    auto versionJson = arangodb::velocypack::Parser::fromJson("{ \"version\": 0, \"tasks\": {} }");

    auto serverRoleBefore = arangodb::ServerState::instance()->getRole();
    arangodb::ServerState::instance()->setRole(arangodb::ServerState::ROLE_DBSERVER);
    auto serverRoleRestore = irs::make_finally([&serverRoleBefore]()->void { arangodb::ServerState::instance()->setRole(serverRoleBefore); });

    // create a new instance of an ApplicationServer and fill it with the required features
    // cannot use the existing server since its features already have some state
    std::shared_ptr<arangodb::application_features::ApplicationServer> originalServer(
      arangodb::application_features::ApplicationServer::server,
      [](arangodb::application_features::ApplicationServer* ptr)->void {
        arangodb::application_features::ApplicationServer::server = ptr;
      }
    );
    arangodb::application_features::ApplicationServer::server = nullptr; // avoid "ApplicationServer initialized twice"
    arangodb::application_features::ApplicationServer server(nullptr, nullptr);
    arangodb::iresearch::IResearchFeature feature(server);
    arangodb::DatabasePathFeature* dbPathFeature;
    arangodb::iresearch::IResearchAnalyzerFeature* analyzerFeature{};
    server.addFeature(new arangodb::FlushFeature(server)); // required to skip IResearchView validation
    server.addFeature(new arangodb::AuthenticationFeature(server)); // required for ClusterInfo::loadPlan()
    server.addFeature(new arangodb::application_features::CommunicationFeaturePhase(server)); // required for SimpleHttpClient::doRequest()
    server.addFeature(new arangodb::DatabaseFeature(server)); // required to skip IResearchView validation
    server.addFeature(dbPathFeature = new arangodb::DatabasePathFeature(server)); // required for IResearchLink::initDataStore()
    server.addFeature(analyzerFeature = new arangodb::iresearch::IResearchAnalyzerFeature(server)); // required for restoring link analyzers
    server.addFeature(new arangodb::QueryRegistryFeature(server)); // required for constructing TRI_vocbase_t
    server.addFeature(new arangodb::ShardingFeature(server)); // required for LogicalCollection::LogicalCollection(...)
    server.addFeature(new arangodb::UpgradeFeature(server, nullptr, {})); // required for upgrade tasks
    server.addFeature(new arangodb::ViewTypesFeature(server)); // required for IResearchFeature::prepare()
    analyzerFeature->prepare(); // add static analyzers
    feature.prepare(); // register iresearch view type
    feature.start(); // register upgrade tasks
    server.getFeature<arangodb::AuthenticationFeature>("Authentication")->prepare(); // create AuthenticationFeature::INSTANCE
    server.getFeature<arangodb::DatabaseFeature>("Database")->enableUpgrade(); // skip IResearchView validation
    server.getFeature<arangodb::ShardingFeature>("Sharding")->prepare(); // register sharding types

    arangodb::tests::setDatabasePath(*dbPathFeature); // ensure test data is stored in a unique directory
    auto versionFilename = StorageEngineMock::versionFilenameResult;
    auto versionFilenameRestore = irs::make_finally([&versionFilename]()->void { StorageEngineMock::versionFilenameResult = versionFilename; });
    StorageEngineMock::versionFilenameResult = (irs::utf8_path(dbPathFeature->directory()) /= "version").utf8();
    ASSERT_TRUE((irs::utf8_path(dbPathFeature->directory()).mkdir()));
    ASSERT_TRUE((arangodb::basics::VelocyPackHelper::velocyPackToFile(StorageEngineMock::versionFilenameResult, versionJson->slice(), false)));

    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, testDatabaseArgs);
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    ASSERT_TRUE((false == !logicalCollection));
    auto logicalView = vocbase.createView(viewJson->slice());
    ASSERT_TRUE((false == !logicalView));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
    ASSERT_TRUE((false == !view));
    bool created;
    auto index = logicalCollection->createIndex(linkJson->slice(), created);
    ASSERT_TRUE((created));
    ASSERT_TRUE((false == !index));
    auto link = std::dynamic_pointer_cast<arangodb::iresearch::IResearchLink>(index);
    ASSERT_TRUE((false == !link));
    ASSERT_TRUE((view->link(link->self()).ok())); // link will not notify view in 'vocbase', hence notify manually

    index->unload(); // release file handles
    bool result;
    auto linkDataPath = getPersistedPath1(*link);
    EXPECT_TRUE((linkDataPath.remove())); // remove link directory
    auto viewDataPath = getPersistedPath0(*logicalView);
    EXPECT_TRUE((viewDataPath.exists(result) && !result)); // ensure no view directory
    arangodb::velocypack::Builder builder;
    builder.openObject();
    EXPECT_TRUE((logicalView
                     ->properties(builder, arangodb::LogicalDataSource::makeFlags(
                                               arangodb::LogicalDataSource::Serialize::Detailed,
                                               arangodb::LogicalDataSource::Serialize::ForPersistence))
                     .ok()));
    builder.close();
    EXPECT_TRUE((0 == builder.slice().get("version").getNumber<uint32_t>())); // ensure 'version == 0 before upgrade

    EXPECT_TRUE((arangodb::methods::Upgrade::startup(vocbase, true, false).ok())); // run upgrade
    logicalView = vocbase.lookupView(logicalView->name());
    EXPECT_TRUE((true == !logicalView)); // ensure view removed after upgrade
    EXPECT_TRUE((viewDataPath.exists(result) && !result)); // ensure view directory not present

    server.getFeature<arangodb::DatabaseFeature>("Database")->unprepare();
  }

  // test db-server (with directory)
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
    auto linkJson = arangodb::velocypack::Parser::fromJson("{ \"view\": \"testView\", \"type\": \"arangosearch\", \"includeAllFields\": true }");
    auto viewJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\", \"version\": 0 }");
    auto versionJson = arangodb::velocypack::Parser::fromJson("{ \"version\": 0, \"tasks\": {} }");

    auto serverRoleBefore = arangodb::ServerState::instance()->getRole();
    arangodb::ServerState::instance()->setRole(arangodb::ServerState::ROLE_DBSERVER);
    auto serverRoleRestore = irs::make_finally([&serverRoleBefore]()->void { arangodb::ServerState::instance()->setRole(serverRoleBefore); });

    // create a new instance of an ApplicationServer and fill it with the required features
    // cannot use the existing server since its features already have some state
    std::shared_ptr<arangodb::application_features::ApplicationServer> originalServer(
      arangodb::application_features::ApplicationServer::server,
      [](arangodb::application_features::ApplicationServer* ptr)->void {
        arangodb::application_features::ApplicationServer::server = ptr;
      }
    );
    arangodb::application_features::ApplicationServer::server = nullptr; // avoid "ApplicationServer initialized twice"
    arangodb::application_features::ApplicationServer server(nullptr, nullptr);
    arangodb::iresearch::IResearchFeature feature(server);
    arangodb::DatabasePathFeature* dbPathFeature;
    arangodb::iresearch::IResearchAnalyzerFeature* analyzerFeature{};
    server.addFeature(new arangodb::FlushFeature(server)); // required to skip IResearchView validation
    server.addFeature(new arangodb::AuthenticationFeature(server)); // required for ClusterInfo::loadPlan()
    server.addFeature(new arangodb::application_features::CommunicationFeaturePhase(server)); // required for SimpleHttpClient::doRequest()
    server.addFeature(new arangodb::DatabaseFeature(server)); // required to skip IResearchView validation
    server.addFeature(dbPathFeature = new arangodb::DatabasePathFeature(server)); // required for IResearchLink::initDataStore()
    server.addFeature(analyzerFeature = new arangodb::iresearch::IResearchAnalyzerFeature(server)); // required for restoring link analyzers
    server.addFeature(new arangodb::QueryRegistryFeature(server)); // required for constructing TRI_vocbase_t
    server.addFeature(new arangodb::ShardingFeature(server)); // required for LogicalCollection::LogicalCollection(...)
    server.addFeature(new arangodb::UpgradeFeature(server, nullptr, {})); // required for upgrade tasks
    server.addFeature(new arangodb::ViewTypesFeature(server)); // required for IResearchFeature::prepare()
    analyzerFeature->prepare(); // add static analyzers
    feature.prepare(); // register iresearch view type
    feature.start(); // register upgrade tasks
    server.getFeature<arangodb::AuthenticationFeature>("Authentication")->prepare(); // create AuthenticationFeature::INSTANCE
    server.getFeature<arangodb::DatabaseFeature>("Database")->enableUpgrade(); // skip IResearchView validation
    server.getFeature<arangodb::ShardingFeature>("Sharding")->prepare(); // register sharding types

    arangodb::tests::setDatabasePath(*dbPathFeature); // ensure test data is stored in a unique directory
    auto versionFilename = StorageEngineMock::versionFilenameResult;
    auto versionFilenameRestore = irs::make_finally([&versionFilename]()->void { StorageEngineMock::versionFilenameResult = versionFilename; });
    StorageEngineMock::versionFilenameResult = (irs::utf8_path(dbPathFeature->directory()) /= "version").utf8();
    ASSERT_TRUE((irs::utf8_path(dbPathFeature->directory()).mkdir()));
    ASSERT_TRUE((arangodb::basics::VelocyPackHelper::velocyPackToFile(StorageEngineMock::versionFilenameResult, versionJson->slice(), false)));

    engine.views.clear();
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, testDatabaseArgs);
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    ASSERT_TRUE((false == !logicalCollection));
    auto logicalView = vocbase.createView(viewJson->slice());
    ASSERT_TRUE((false == !logicalView));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
    ASSERT_TRUE((false == !view));
    bool created;
    auto index = logicalCollection->createIndex(linkJson->slice(), created);
    ASSERT_TRUE((created));
    ASSERT_TRUE((false == !index));
    auto link = std::dynamic_pointer_cast<arangodb::iresearch::IResearchLink>(index);
    ASSERT_TRUE((false == !link));
    ASSERT_TRUE((view->link(link->self()).ok())); // link will not notify view in 'vocbase', hence notify manually

    index->unload(); // release file handles
    bool result;
    auto linkDataPath = getPersistedPath1(*link);
    EXPECT_TRUE((linkDataPath.remove())); // remove link directory
    auto viewDataPath = getPersistedPath0(*logicalView);
    EXPECT_TRUE((viewDataPath.exists(result) && !result));
    EXPECT_TRUE((viewDataPath.mkdir())); // create view directory
    EXPECT_TRUE((viewDataPath.exists(result) && result));
    arangodb::velocypack::Builder builder;
    builder.openObject();
    EXPECT_TRUE((logicalView
                     ->properties(builder, arangodb::LogicalDataSource::makeFlags(
                                               arangodb::LogicalDataSource::Serialize::Detailed,
                                               arangodb::LogicalDataSource::Serialize::ForPersistence))
                     .ok()));
    builder.close();
    EXPECT_TRUE((0 == builder.slice().get("version").getNumber<uint32_t>())); // ensure 'version == 0 before upgrade

    EXPECT_TRUE((arangodb::methods::Upgrade::startup(vocbase, true, false).ok())); // run upgrade
//    EXPECT_TRUE(arangodb::methods::Upgrade::clusterBootstrap(vocbase).ok()); // run upgrade
    logicalView = vocbase.lookupView(logicalView->name());
    EXPECT_TRUE((true == !logicalView)); // ensure view removed after upgrade
    EXPECT_TRUE((viewDataPath.exists(result) && !result)); // ensure view directory removed after upgrade
  }
}

TEST_F(IResearchFeatureTest, IResearch_version_test) {
  EXPECT_TRUE(IResearch_version == arangodb::rest::Version::getIResearchVersion());
  EXPECT_TRUE(IResearch_version == arangodb::rest::Version::Values["iresearch-version"]);
}

// Temporarily surpress for MSVC
#ifndef _MSC_VER
TEST_F(IResearchFeatureTest, test_async) {
  // schedule task (null resource mutex)
  {
    // create a new instance of an ApplicationServer and fill it with the required features
    // cannot use the existing server since its features already have some state
    std::shared_ptr<arangodb::application_features::ApplicationServer> originalServer(
      arangodb::application_features::ApplicationServer::server,
      [](arangodb::application_features::ApplicationServer* ptr)->void {
        arangodb::application_features::ApplicationServer::server = ptr;
      }
    );
    arangodb::application_features::ApplicationServer::server = nullptr; // avoid "ApplicationServer initialized twice"
    arangodb::application_features::ApplicationServer server(nullptr, nullptr);
    bool deallocated = false; // declare above 'feature' to ensure proper destruction order
    arangodb::iresearch::IResearchFeature feature(server);
    server.addFeature(new arangodb::ViewTypesFeature(server)); // required for IResearchFeature::prepare()
    feature.prepare(); // start thread pool
    std::condition_variable cond;
    std::mutex mutex;
    SCOPED_LOCK_NAMED(mutex, lock);

    {
      std::shared_ptr<bool> flag(&deallocated, [](bool* ptr)->void { *ptr = true; });
      feature.async(nullptr, [&cond, &mutex, flag](size_t&, bool)->bool { SCOPED_LOCK(mutex); cond.notify_all(); return false; });
    }
    EXPECT_TRUE((std::cv_status::timeout != cond.wait_for(lock, std::chrono::milliseconds(100))));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_TRUE((true == deallocated));
  }

  // schedule task (null resource mutex value)
  {
    // create a new instance of an ApplicationServer and fill it with the required features
    // cannot use the existing server since its features already have some state
    std::shared_ptr<arangodb::application_features::ApplicationServer> originalServer(
      arangodb::application_features::ApplicationServer::server,
      [](arangodb::application_features::ApplicationServer* ptr)->void {
        arangodb::application_features::ApplicationServer::server = ptr;
      }
    );
    arangodb::application_features::ApplicationServer::server = nullptr; // avoid "ApplicationServer initialized twice"
    arangodb::application_features::ApplicationServer server(nullptr, nullptr);
    bool deallocated = false; // declare above 'feature' to ensure proper destruction order
    arangodb::iresearch::IResearchFeature feature(server);
    server.addFeature(new arangodb::ViewTypesFeature(server)); // required for IResearchFeature::prepare()
    feature.prepare(); // start thread pool
    auto resourceMutex = std::make_shared<arangodb::iresearch::ResourceMutex>(nullptr);
    std::condition_variable cond;
    std::mutex mutex;
    SCOPED_LOCK_NAMED(mutex, lock);

    {
      std::shared_ptr<bool> flag(&deallocated, [](bool* ptr)->void { *ptr = true; });
      feature.async(resourceMutex, [&cond, &mutex, flag](size_t&, bool)->bool { SCOPED_LOCK(mutex); cond.notify_all(); return false; });
    }
    EXPECT_TRUE((std::cv_status::timeout == cond.wait_for(lock, std::chrono::milliseconds(100))));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_TRUE((true == deallocated));
  }

  // schedule task (null functr)
  {
    // create a new instance of an ApplicationServer and fill it with the required features
    // cannot use the existing server since its features already have some state
    std::shared_ptr<arangodb::application_features::ApplicationServer> originalServer(
      arangodb::application_features::ApplicationServer::server,
      [](arangodb::application_features::ApplicationServer* ptr)->void {
        arangodb::application_features::ApplicationServer::server = ptr;
      }
    );
    arangodb::application_features::ApplicationServer::server = nullptr; // avoid "ApplicationServer initialized twice"
    arangodb::application_features::ApplicationServer server(nullptr, nullptr);
    arangodb::iresearch::IResearchFeature feature(server);
    server.addFeature(new arangodb::ViewTypesFeature(server)); // required for IResearchFeature::prepare()
    feature.prepare(); // start thread pool
    auto resourceMutex = std::make_shared<arangodb::iresearch::ResourceMutex>(&server);
    std::condition_variable cond;
    std::mutex mutex;
    SCOPED_LOCK_NAMED(mutex, lock);

    feature.async(resourceMutex, {});
    EXPECT_TRUE((std::cv_status::timeout == cond.wait_for(lock, std::chrono::milliseconds(100))));
    resourceMutex->reset(); // should not deadlock if task released
  }

  // schedule task (wait indefinite)
  {
    // create a new instance of an ApplicationServer and fill it with the required features
    // cannot use the existing server since its features already have some state
    std::shared_ptr<arangodb::application_features::ApplicationServer> originalServer(
      arangodb::application_features::ApplicationServer::server,
      [](arangodb::application_features::ApplicationServer* ptr)->void {
        arangodb::application_features::ApplicationServer::server = ptr;
      }
    );
    arangodb::application_features::ApplicationServer::server = nullptr; // avoid "ApplicationServer initialized twice"
    arangodb::application_features::ApplicationServer server(nullptr, nullptr);
    bool deallocated = false; // declare above 'feature' to ensure proper destruction order
    arangodb::iresearch::IResearchFeature feature(server);
    server.addFeature(new arangodb::ViewTypesFeature(server)); // required for IResearchFeature::prepare()
    feature.prepare(); // start thread pool
    std::condition_variable cond;
    std::mutex mutex;
    size_t count = 0;
    SCOPED_LOCK_NAMED(mutex, lock);

    {
      std::shared_ptr<bool> flag(&deallocated, [](bool* ptr)->void { *ptr = true; });
      feature.async(nullptr, [&cond, &mutex, flag, &count](size_t&, bool)->bool { ++count; SCOPED_LOCK(mutex); cond.notify_all(); return true; });
    }
    EXPECT_TRUE((std::cv_status::timeout != cond.wait_for(lock, std::chrono::milliseconds(100)))); // first run invoked immediately
    EXPECT_TRUE((false == deallocated));
    EXPECT_TRUE((std::cv_status::timeout == cond.wait_for(lock, std::chrono::milliseconds(100))));
    EXPECT_TRUE((false == deallocated)); // still scheduled
    EXPECT_TRUE((1 == count));
  }

  // single-run task
  {
    // create a new instance of an ApplicationServer and fill it with the required features
    // cannot use the existing server since its features already have some state
    std::shared_ptr<arangodb::application_features::ApplicationServer> originalServer(
      arangodb::application_features::ApplicationServer::server,
      [](arangodb::application_features::ApplicationServer* ptr)->void {
        arangodb::application_features::ApplicationServer::server = ptr;
      }
    );
    arangodb::application_features::ApplicationServer::server = nullptr; // avoid "ApplicationServer initialized twice"
    arangodb::application_features::ApplicationServer server(nullptr, nullptr);
    bool deallocated = false; // declare above 'feature' to ensure proper destruction order
    arangodb::iresearch::IResearchFeature feature(server);
    server.addFeature(new arangodb::ViewTypesFeature(server)); // required for IResearchFeature::prepare()
    feature.prepare(); // start thread pool
    auto resourceMutex = std::make_shared<arangodb::iresearch::ResourceMutex>(&server);
    std::condition_variable cond;
    std::mutex mutex;
    SCOPED_LOCK_NAMED(mutex, lock);

    {
      std::shared_ptr<bool> flag(&deallocated, [](bool* ptr)->void { *ptr = true; });
      feature.async(resourceMutex, [&cond, &mutex, flag](size_t&, bool)->bool { SCOPED_LOCK(mutex); cond.notify_all(); return false; });
    }
    EXPECT_TRUE((std::cv_status::timeout != cond.wait_for(lock, std::chrono::milliseconds(100))));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_TRUE((true == deallocated));
  }

  // multi-run task
  {
    // create a new instance of an ApplicationServer and fill it with the required features
    // cannot use the existing server since its features already have some state
    std::shared_ptr<arangodb::application_features::ApplicationServer> originalServer(
      arangodb::application_features::ApplicationServer::server,
      [](arangodb::application_features::ApplicationServer* ptr)->void {
        arangodb::application_features::ApplicationServer::server = ptr;
      }
    );
    arangodb::application_features::ApplicationServer::server = nullptr; // avoid "ApplicationServer initialized twice"
    arangodb::application_features::ApplicationServer server(nullptr, nullptr);
    bool deallocated = false; // declare above 'feature' to ensure proper destruction order
    arangodb::iresearch::IResearchFeature feature(server);
    server.addFeature(new arangodb::ViewTypesFeature(server)); // required for IResearchFeature::prepare()
    feature.prepare(); // start thread pool
    auto resourceMutex = std::make_shared<arangodb::iresearch::ResourceMutex>(&server);
    std::condition_variable cond;
    std::mutex mutex;
    size_t count = 0;
    auto last = std::chrono::system_clock::now();
    std::chrono::system_clock::duration diff;
    SCOPED_LOCK_NAMED(mutex, lock);

    {
      std::shared_ptr<bool> flag(&deallocated, [](bool* ptr)->void { *ptr = true; });
      feature.async(resourceMutex, [&cond, &mutex, flag, &count, &last, &diff](size_t& timeoutMsec, bool)->bool {
        diff = std::chrono::system_clock::now() - last;
        last = std::chrono::system_clock::now();
        timeoutMsec = 100;
        if (++count <= 1) return true;
        SCOPED_LOCK(mutex);
        cond.notify_all();
        return false;
      });
    }
    EXPECT_TRUE((std::cv_status::timeout != cond.wait_for(lock, std::chrono::milliseconds(1000))));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_TRUE((true == deallocated));
    EXPECT_TRUE((2 == count));
    EXPECT_TRUE((std::chrono::milliseconds(100) < diff));
  }

  // trigger task by notify
  {
    // create a new instance of an ApplicationServer and fill it with the required features
    // cannot use the existing server since its features already have some state
    std::shared_ptr<arangodb::application_features::ApplicationServer> originalServer(
      arangodb::application_features::ApplicationServer::server,
      [](arangodb::application_features::ApplicationServer* ptr)->void {
        arangodb::application_features::ApplicationServer::server = ptr;
      }
    );
    arangodb::application_features::ApplicationServer::server = nullptr; // avoid "ApplicationServer initialized twice"
    arangodb::application_features::ApplicationServer server(nullptr, nullptr);
    bool deallocated = false; // declare above 'feature' to ensure proper destruction order
    arangodb::iresearch::IResearchFeature feature(server);
    server.addFeature(new arangodb::ViewTypesFeature(server)); // required for IResearchFeature::prepare()
    feature.prepare(); // start thread pool
    auto resourceMutex = std::make_shared<arangodb::iresearch::ResourceMutex>(&server);
    bool execVal = true;
    std::condition_variable cond;
    std::mutex mutex;
    size_t count = 0;
    auto last = std::chrono::system_clock::now();
    SCOPED_LOCK_NAMED(mutex, lock);

    {
      std::shared_ptr<bool> flag(&deallocated, [](bool* ptr)->void { *ptr = true; });
      feature.async(resourceMutex, [&cond, &mutex, flag, &execVal, &count](size_t&, bool exec)->bool { execVal = exec; SCOPED_LOCK(mutex); cond.notify_all(); return ++count < 2; });
    }
    EXPECT_TRUE((std::cv_status::timeout != cond.wait_for(lock, std::chrono::milliseconds(100)))); // first run invoked immediately
    EXPECT_TRUE((false == deallocated));
    EXPECT_TRUE((std::cv_status::timeout == cond.wait_for(lock, std::chrono::milliseconds(100))));
    EXPECT_TRUE((false == deallocated));
    feature.asyncNotify();
    EXPECT_TRUE((std::cv_status::timeout != cond.wait_for(lock, std::chrono::milliseconds(100))));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_TRUE((true == deallocated));
    EXPECT_TRUE((false == execVal));
    auto diff = std::chrono::system_clock::now() - last;
    EXPECT_TRUE((std::chrono::milliseconds(1000) > diff));
  }

  // trigger by timeout
  {
    // create a new instance of an ApplicationServer and fill it with the required features
    // cannot use the existing server since its features already have some state
    std::shared_ptr<arangodb::application_features::ApplicationServer> originalServer(
      arangodb::application_features::ApplicationServer::server,
      [](arangodb::application_features::ApplicationServer* ptr)->void {
        arangodb::application_features::ApplicationServer::server = ptr;
      }
    );
    arangodb::application_features::ApplicationServer::server = nullptr; // avoid "ApplicationServer initialized twice"
    arangodb::application_features::ApplicationServer server(nullptr, nullptr);
    bool deallocated = false; // declare above 'feature' to ensure proper destruction order
    arangodb::iresearch::IResearchFeature feature(server);
    server.addFeature(new arangodb::ViewTypesFeature(server)); // required for IResearchFeature::prepare()
    feature.prepare(); // start thread pool
    auto resourceMutex = std::make_shared<arangodb::iresearch::ResourceMutex>(&server);
    bool execVal = false;
    std::condition_variable cond;
    std::mutex mutex;
    size_t count = 0;
    auto last = std::chrono::system_clock::now();
    SCOPED_LOCK_NAMED(mutex, lock);

    {
      std::shared_ptr<bool> flag(&deallocated, [](bool* ptr)->void { *ptr = true; });
      feature.async(resourceMutex, [&cond, &mutex, flag, &execVal, &count](size_t& timeoutMsec, bool exec)->bool {
        execVal = exec;
        SCOPED_LOCK(mutex);
        cond.notify_all();
        timeoutMsec = 100;
        return ++count < 2;
      });
    }
    EXPECT_TRUE((std::cv_status::timeout != cond.wait_for(lock, std::chrono::milliseconds(100)))); // first run invoked immediately
    EXPECT_TRUE((false == deallocated));
    EXPECT_TRUE((std::cv_status::timeout != cond.wait_for(lock, std::chrono::milliseconds(1000))));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_TRUE((true == deallocated));
    EXPECT_TRUE((true == execVal));
    auto diff = std::chrono::system_clock::now() - last;
    EXPECT_TRUE((std::chrono::milliseconds(300) >= diff)); // could be a little more then 100ms+100ms
  }

  // deallocate empty
  {
    // create a new instance of an ApplicationServer and fill it with the required features
    // cannot use the existing server since its features already have some state
    std::shared_ptr<arangodb::application_features::ApplicationServer> originalServer(
      arangodb::application_features::ApplicationServer::server,
      [](arangodb::application_features::ApplicationServer* ptr)->void {
        arangodb::application_features::ApplicationServer::server = ptr;
      }
    );
    arangodb::application_features::ApplicationServer::server = nullptr; // avoid "ApplicationServer initialized twice"
    arangodb::application_features::ApplicationServer server(nullptr, nullptr);

    {
      arangodb::iresearch::IResearchFeature feature(server);
      server.addFeature(new arangodb::ViewTypesFeature(server)); // required for IResearchFeature::prepare()
      feature.prepare(); // start thread pool
    }
  }

  // deallocate with running tasks
  {
    // create a new instance of an ApplicationServer and fill it with the required features
    // cannot use the existing server since its features already have some state
    std::shared_ptr<arangodb::application_features::ApplicationServer> originalServer(
      arangodb::application_features::ApplicationServer::server,
      [](arangodb::application_features::ApplicationServer* ptr)->void {
        arangodb::application_features::ApplicationServer::server = ptr;
      }
    );
    arangodb::application_features::ApplicationServer::server = nullptr; // avoid "ApplicationServer initialized twice"
    arangodb::application_features::ApplicationServer server(nullptr, nullptr);
    auto resourceMutex = std::make_shared<arangodb::iresearch::ResourceMutex>(&server);
    bool deallocated = false;
    std::condition_variable cond;
    std::mutex mutex;
    SCOPED_LOCK_NAMED(mutex, lock);

    {
      arangodb::iresearch::IResearchFeature feature(server);
      server.addFeature(new arangodb::ViewTypesFeature(server)); // required for IResearchFeature::prepare()
      feature.prepare(); // start thread pool
      std::shared_ptr<bool> flag(&deallocated, [](bool* ptr)->void { *ptr = true; });

      feature.async(resourceMutex, [&cond, &mutex, flag](size_t& timeoutMsec, bool)->bool { SCOPED_LOCK(mutex); cond.notify_all(); timeoutMsec = 100; return true; });
      EXPECT_TRUE((std::cv_status::timeout != cond.wait_for(lock, std::chrono::milliseconds(100))));
    }

    EXPECT_TRUE((true == deallocated));
  }

  // multiple tasks with same resourceMutex + resourceMutex reset (sequential creation)
  {
    // create a new instance of an ApplicationServer and fill it with the required features
    // cannot use the existing server since its features already have some state
    std::shared_ptr<arangodb::application_features::ApplicationServer> originalServer(
      arangodb::application_features::ApplicationServer::server,
      [](arangodb::application_features::ApplicationServer* ptr)->void {
        arangodb::application_features::ApplicationServer::server = ptr;
      }
    );
    arangodb::application_features::ApplicationServer::server = nullptr; // avoid "ApplicationServer initialized twice"
    arangodb::application_features::ApplicationServer server(nullptr, nullptr);
    bool deallocated0 = false; // declare above 'feature' to ensure proper destruction order
    bool deallocated1 = false; // declare above 'feature' to ensure proper destruction order
    arangodb::iresearch::IResearchFeature feature(server);
    server.addFeature(new arangodb::ViewTypesFeature(server)); // required for IResearchFeature::prepare()
    feature.prepare(); // start thread pool
    auto resourceMutex = std::make_shared<arangodb::iresearch::ResourceMutex>(&server);
    std::condition_variable cond;
    std::mutex mutex;
    size_t count = 0;
    SCOPED_LOCK_NAMED(mutex, lock);

    {
      std::shared_ptr<bool> flag(&deallocated0, [](bool* ptr)->void { *ptr = true; });
      feature.async(resourceMutex, [&cond, &mutex, flag, &count](size_t& timeoutMsec, bool)->bool {
        if (++count > 1) return false;
        timeoutMsec = 100;
        SCOPED_LOCK_NAMED(mutex, lock);
        cond.notify_all();
        cond.wait(lock);
        return true;
      });
    }
    EXPECT_TRUE((std::cv_status::timeout != cond.wait_for(lock, std::chrono::milliseconds(1000)))); // wait for the first task to start

    std::thread thread([resourceMutex]()->void { resourceMutex->reset(); }); // try to aquire a write lock
    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // hopefully a write-lock aquisition attempt is in progress

    {
      TRY_SCOPED_LOCK_NAMED(resourceMutex->mutex(), resourceLock);
      EXPECT_TRUE((false == resourceLock.owns_lock())); // write-lock aquired successfully (read-locks blocked)
    }

    {
      std::shared_ptr<bool> flag(&deallocated1, [](bool* ptr)->void { *ptr = true; });
      feature.async(resourceMutex, [flag](size_t&, bool)->bool { return false; }); // will never get invoked because resourceMutex is reset
    }
    cond.notify_all(); // wake up first task after resourceMutex write-lock aquired (will process pending tasks)
    lock.unlock(); // allow first task to run
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_TRUE((true == deallocated0));
    EXPECT_TRUE((true == deallocated1));
    thread.join();
  }

  // schedule task (resize pool)
  {
    // create a new instance of an ApplicationServer and fill it with the required features
    // cannot use the existing server since its features already have some state
    std::shared_ptr<arangodb::application_features::ApplicationServer> originalServer(
      arangodb::application_features::ApplicationServer::server,
      [](arangodb::application_features::ApplicationServer* ptr)->void {
        arangodb::application_features::ApplicationServer::server = ptr;
      }
    );
    arangodb::application_features::ApplicationServer::server = nullptr; // avoid "ApplicationServer initialized twice"
    arangodb::application_features::ApplicationServer server(nullptr, nullptr);
    bool deallocated = false; // declare above 'feature' to ensure proper destruction order
    arangodb::iresearch::IResearchFeature feature(server);
    server.addFeature(new arangodb::ViewTypesFeature(server)); // required for IResearchFeature::prepare()
    arangodb::options::ProgramOptions options("", "", "", nullptr);
    auto optionsPtr = std::shared_ptr<arangodb::options::ProgramOptions>(&options, [](arangodb::options::ProgramOptions*)->void {});
    feature.collectOptions(optionsPtr);
    options.get<arangodb::options::UInt64Parameter>("arangosearch.threads")->set("8");
    auto resourceMutex = std::make_shared<arangodb::iresearch::ResourceMutex>(&server);
    std::condition_variable cond;
    std::mutex mutex;
    size_t count = 0;
    auto last = std::chrono::system_clock::now();
    std::chrono::system_clock::duration diff;
    SCOPED_LOCK_NAMED(mutex, lock);

    {
      std::shared_ptr<bool> flag(&deallocated, [](bool* ptr)->void { *ptr = true; });
      feature.async(resourceMutex, [&cond, &mutex, flag, &count, &last, &diff](size_t& timeoutMsec, bool)->bool {
        diff = std::chrono::system_clock::now() - last;
        last = std::chrono::system_clock::now();
        timeoutMsec = 100;
        if (++count <= 1) return true;
        SCOPED_LOCK(mutex);
        cond.notify_all();
        return false;
      });
    }
    feature.prepare(); // start thread pool after a task has been scheduled, to trigger resize with a task
    EXPECT_TRUE((std::cv_status::timeout != cond.wait_for(lock, std::chrono::milliseconds(1000))));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_TRUE((true == deallocated));
    EXPECT_TRUE((2 == count));
    EXPECT_TRUE((std::chrono::milliseconds(100) < diff));
  }
}
#endif
