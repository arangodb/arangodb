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

#include "catch.hpp"
#include "common.h"
#include "AgencyMock.h"
#include "../Mocks/StorageEngineMock.h"
#include "ApplicationFeatures/BasicPhase.h"
#include "ApplicationFeatures/CommunicationPhase.h"
#include "ApplicationFeatures/ClusterPhase.h"
#include "ApplicationFeatures/DatabasePhase.h"
#include "ApplicationFeatures/GreetingsPhase.h"
#include "ApplicationFeatures/V8Phase.h"
#include "Agency/Store.h"
#include "Aql/AstNode.h"
#include "Aql/Variable.h"
#include "Basics/files.h"
#include "Cluster/ClusterComm.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "Sharding/ShardingFeature.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "IResearch/IResearchCommon.h"
#include "IResearch/IResearchFeature.h"
#include "IResearch/IResearchLinkHelper.h"
#include "IResearch/IResearchLinkMeta.h"
#include "IResearch/IResearchMMFilesLink.h"
#include "IResearch/IResearchView.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/DatabasePathFeature.h"
#include "RestServer/FlushFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "RestServer/SystemDatabaseFeature.h"
#include "RestServer/ViewTypesFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "Transaction/Methods.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/OperationOptions.h"
#include "utils/utf8_path.hpp"
#include "velocypack/Parser.h"
#include "V8Server/V8DealerFeature.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ManagedDocumentResult.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

struct IResearchViewDBServerSetup {
  struct ClusterCommControl : arangodb::ClusterComm {
    static void reset() {
      arangodb::ClusterComm::_theInstanceInit.store(0);
    }
  };

  arangodb::consensus::Store _agencyStore{nullptr, "arango"};
  GeneralClientConnectionAgencyMock* agency;
  StorageEngineMock engine;
  arangodb::application_features::ApplicationServer server;
  std::map<std::string, std::pair<arangodb::application_features::ApplicationFeature*, bool>> features;
  std::vector<arangodb::application_features::ApplicationFeature*> orderedFeatures;
  std::string testFilesystemPath;

  IResearchViewDBServerSetup(): engine(server), server(nullptr, nullptr) {
    auto* agencyCommManager = new AgencyCommManagerMock("arango");
    agency = agencyCommManager->addConnection<GeneralClientConnectionAgencyMock>(_agencyStore);
    agency = agencyCommManager->addConnection<GeneralClientConnectionAgencyMock>(_agencyStore); // need 2 connections or Agency callbacks will fail
    arangodb::AgencyCommManager::MANAGER.reset(agencyCommManager);

    arangodb::ServerState::instance()->setRole(arangodb::ServerState::RoleEnum::ROLE_DBSERVER);
    arangodb::EngineSelectorFeature::ENGINE = &engine;

    // suppress INFO {authentication} Authentication is turned on (system only), authentication for unix sockets is turned on
    // suppress WARNING {authentication} --server.jwt-secret is insecure. Use --server.jwt-secret-keyfile instead
    arangodb::LogTopic::setLogLevel(arangodb::Logger::AUTHENTICATION.name(), arangodb::LogLevel::ERR);

    // suppress INFO {cluster} Starting up with role PRIMARY
    arangodb::LogTopic::setLogLevel(arangodb::Logger::CLUSTER.name(), arangodb::LogLevel::WARN);

    // suppress log messages since tests check error conditions
    arangodb::LogTopic::setLogLevel(arangodb::Logger::AGENCY.name(), arangodb::LogLevel::FATAL);
    arangodb::LogTopic::setLogLevel(arangodb::iresearch::TOPIC.name(), arangodb::LogLevel::FATAL);
    irs::logger::output_le(iresearch::logger::IRL_FATAL, stderr);

    auto buildFeatureEntry = [&] (arangodb::application_features::ApplicationFeature* ftr, bool start) -> void {
      std::string name = ftr->name();
      features.emplace(name, std::make_pair(ftr, start));
    };

    buildFeatureEntry(new arangodb::application_features::BasicFeaturePhase(server, false), false);
    buildFeatureEntry(new arangodb::application_features::CommunicationFeaturePhase(server), false);
    buildFeatureEntry(new arangodb::application_features::ClusterFeaturePhase(server), false);
    buildFeatureEntry(new arangodb::application_features::DatabaseFeaturePhase(server), false);
    buildFeatureEntry(new arangodb::application_features::GreetingsFeaturePhase(server, false), false);
    buildFeatureEntry(new arangodb::application_features::V8FeaturePhase(server), false);

    // setup required application features
    buildFeatureEntry(new arangodb::AuthenticationFeature(server), false); // required for AgencyComm::send(...)
    buildFeatureEntry(arangodb::DatabaseFeature::DATABASE = new arangodb::DatabaseFeature(server), false); // required for TRI_vocbase_t::renameView(...)
    buildFeatureEntry(new arangodb::DatabasePathFeature(server), false);
    buildFeatureEntry(new arangodb::FlushFeature(server), false); // do not start the thread
    buildFeatureEntry(new arangodb::QueryRegistryFeature(server), false); // required for TRI_vocbase_t instantiation
    buildFeatureEntry(new arangodb::ShardingFeature(server), false); // required for TRI_vocbase_t instantiation
    buildFeatureEntry(new arangodb::SystemDatabaseFeature(server), true); // required for IResearchAnalyzerFeature
    buildFeatureEntry(new arangodb::ViewTypesFeature(server), false); // required for TRI_vocbase_t::createView(...)
    buildFeatureEntry(new arangodb::iresearch::IResearchAnalyzerFeature(server), false); // required for IResearchLinkMeta::init(...)
    buildFeatureEntry(new arangodb::iresearch::IResearchFeature(server), false); // required for instantiating IResearchView*
    buildFeatureEntry(new arangodb::ClusterFeature(server), false);
    buildFeatureEntry(new arangodb::V8DealerFeature(server), false);

    for (auto& f : features) {
      arangodb::application_features::ApplicationServer::server->addFeature(f.second.first);
    }

    arangodb::application_features::ApplicationServer::server->setupDependencies(false);
    orderedFeatures = arangodb::application_features::ApplicationServer::server->getOrderedFeatures();

    for (auto& f : orderedFeatures) {
      if (f->name() == "Endpoint") {
        // We need this feature to be there but do not use it.
        continue;
      }
      f->prepare();
      if (f->name() == "Authentication") {
        f->forceDisable();
      }
    }

    auto const databases = arangodb::velocypack::Parser::fromJson(std::string("[ { \"name\": \"") + arangodb::StaticStrings::SystemDatabase + "\" } ]");
    auto* dbFeature = arangodb::application_features::ApplicationServer::lookupFeature<
      arangodb::DatabaseFeature
    >("Database");
    dbFeature->loadDatabases(databases->slice());

    for (auto& f : orderedFeatures) {
      if (features.at(f->name()).second) {
        f->start();
      }
    }

    auto* dbPathFeature = arangodb::application_features::ApplicationServer::getFeature<arangodb::DatabasePathFeature>("DatabasePath");
    arangodb::tests::setDatabasePath(*dbPathFeature); // ensure test data is stored in a unique directory
    testFilesystemPath = dbPathFeature->directory();

    long systemError;
    std::string systemErrorStr;
    TRI_CreateDirectory(testFilesystemPath.c_str(), systemError, systemErrorStr);

    agencyCommManager->start(); // initialize agency
  }

  ~IResearchViewDBServerSetup() {
    TRI_RemoveDirectory(testFilesystemPath.c_str());
    arangodb::LogTopic::setLogLevel(arangodb::Logger::CLUSTER.name(), arangodb::LogLevel::DEFAULT);
    arangodb::ClusterInfo::cleanup(); // reset ClusterInfo::instance() before DatabaseFeature::unprepare()
    arangodb::application_features::ApplicationServer::server = nullptr;

    // destroy application features
    for (auto f = orderedFeatures.rbegin() ; f != orderedFeatures.rend(); ++f) { 
      if (features.at((*f)->name()).second) {
        (*f)->stop();
      }
    }

    for (auto f = orderedFeatures.rbegin() ; f != orderedFeatures.rend(); ++f) { 
      (*f)->unprepare();
    }

    ClusterCommControl::reset();
    arangodb::ServerState::instance()->setRole(arangodb::ServerState::RoleEnum::ROLE_SINGLE);
    arangodb::LogTopic::setLogLevel(arangodb::iresearch::TOPIC.name(), arangodb::LogLevel::DEFAULT);
    arangodb::LogTopic::setLogLevel(arangodb::Logger::AUTHENTICATION.name(), arangodb::LogLevel::DEFAULT);
    arangodb::LogTopic::setLogLevel(arangodb::Logger::AGENCY.name(), arangodb::LogLevel::DEFAULT);
    arangodb::EngineSelectorFeature::ENGINE = nullptr;
    arangodb::AgencyCommManager::MANAGER.reset();
  }
};

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief setup
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("IResearchViewDBServerTest", "[cluster][iresearch][iresearch-view]") {
  IResearchViewDBServerSetup s;
  (void)(s);

SECTION("test_drop") {
  auto* database = arangodb::DatabaseFeature::DATABASE;
  REQUIRE(nullptr != database);
  auto* ci = arangodb::ClusterInfo::instance();
  REQUIRE(nullptr != ci);
  TRI_vocbase_t* vocbase; // will be owned by DatabaseFeature

  // create database
  {
    // simulate heartbeat thread
    REQUIRE(TRI_ERROR_NO_ERROR == database->createDatabase(1, "testDatabase", vocbase));

    REQUIRE(nullptr != vocbase);
    CHECK("testDatabase" == vocbase->name());
    CHECK(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL == vocbase->type());
    CHECK(1 == vocbase->id());

    CHECK((ci->createDatabaseCoordinator(vocbase->name(), VPackSlice::emptyObjectSlice(), 0.0).ok()));
  }

  // drop empty
  {
    auto json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
    arangodb::LogicalView::ptr wiew;
    REQUIRE((arangodb::iresearch::IResearchView::factory().create(wiew, *vocbase, json->slice()).ok()));
    CHECK((false == !wiew));
    auto* impl = dynamic_cast<arangodb::iresearch::IResearchView*>(wiew.get());
    CHECK((nullptr != impl));

    CHECK((true == impl->drop().ok()));
  }

  // drop non-empty
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection0\" }");
    auto linkJson = arangodb::velocypack::Parser::fromJson("{ \"view\": \"testView0\", \"type\": \"arangosearch\", \"includeAllFields\": true }");
    auto viewJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView0\", \"type\": \"arangosearch\" }");
    auto const wiewId = std::to_string(ci->uniqid() + 1); // +1 because LogicalView creation will generate a new ID
    auto logicalCollection = vocbase->createCollection(collectionJson->slice());
    REQUIRE((false == !logicalCollection));
    arangodb::LogicalView::ptr wiew;
    REQUIRE((arangodb::iresearch::IResearchView::factory().create(wiew, *vocbase, viewJson->slice()).ok()));
    CHECK((false == !wiew));
    auto* impl = dynamic_cast<arangodb::iresearch::IResearchView*>(wiew.get());
    CHECK((nullptr != impl));

    // ensure we have shard view in vocbase
    bool created;
    auto index = logicalCollection->createIndex(linkJson->slice(), created);
    REQUIRE((false == !index));
    auto link = std::dynamic_pointer_cast<arangodb::iresearch::IResearchLink>(index);
    REQUIRE((false == !link));

    static auto visitor = [](TRI_voc_cid_t)->bool { return false; };
    CHECK((false == impl->visitCollections(visitor)));
    CHECK((false == !arangodb::iresearch::IResearchLinkHelper::find(*logicalCollection, *wiew)));
    CHECK((true == impl->drop().ok()));
    CHECK((true == !arangodb::iresearch::IResearchLinkHelper::find(*logicalCollection, *wiew)));
    CHECK((false == impl->visitCollections(visitor))); // list of links is not modified after link drop
  }

  // drop non-empty (drop failure)
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection1\" }");
    auto linkJson = arangodb::velocypack::Parser::fromJson("{ \"view\": \"testView1\", \"type\": \"arangosearch\", \"includeAllFields\": true }");
    auto viewJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView1\", \"type\": \"arangosearch\" }");
    auto const wiewId = std::to_string(ci->uniqid() + 1); // +1 because LogicalView creation will generate a new ID
    auto logicalCollection = vocbase->createCollection(collectionJson->slice());
    REQUIRE((false == !logicalCollection));
    arangodb::LogicalView::ptr wiew;
    REQUIRE((arangodb::iresearch::IResearchView::factory().create(wiew, *vocbase, viewJson->slice()).ok()));
    CHECK((false == !wiew));
    auto* impl = dynamic_cast<arangodb::iresearch::IResearchView*>(wiew.get());
    CHECK((nullptr != impl));

    // ensure we have shard view in vocbase
    bool created;
    auto index = logicalCollection->createIndex(linkJson->slice(), created);
    REQUIRE((false == !index));
    auto link = std::dynamic_pointer_cast<arangodb::iresearch::IResearchLink>(index);
    REQUIRE((false == !link));

    static auto visitor = [](TRI_voc_cid_t)->bool { return false; };
    CHECK((false == impl->visitCollections(visitor)));
    CHECK((false == !arangodb::iresearch::IResearchLinkHelper::find(*logicalCollection, *wiew)));

    auto before = PhysicalCollectionMock::before;
    auto restore = irs::make_finally([&before]()->void { PhysicalCollectionMock::before = before; });
    PhysicalCollectionMock::before = []()->void { throw std::exception(); };

    CHECK((!impl->drop().ok()));
    CHECK((false == !arangodb::iresearch::IResearchLinkHelper::find(*logicalCollection, *wiew)));
    CHECK((false == impl->visitCollections(visitor)));
  }
}

SECTION("test_drop_cid") {
  auto* database = arangodb::DatabaseFeature::DATABASE;
  REQUIRE((nullptr != database));
  auto* ci = arangodb::ClusterInfo::instance();
  REQUIRE((nullptr != ci));
  TRI_vocbase_t* vocbase; // will be owned by DatabaseFeature

  // create database
  {
    // simulate heartbeat thread
    REQUIRE(TRI_ERROR_NO_ERROR == database->createDatabase(1, "testDatabase", vocbase));

    REQUIRE(nullptr != vocbase);
    CHECK("testDatabase" == vocbase->name());
    CHECK(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL == vocbase->type());
    CHECK(1 == vocbase->id());

    CHECK((ci->createDatabaseCoordinator(vocbase->name(), VPackSlice::emptyObjectSlice(), 0.0).ok()));
  }

  auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
  auto linkJson = arangodb::velocypack::Parser::fromJson("{ \"view\": \"testView\", \"type\": \"arangosearch\", \"includeAllFields\": true }");
  auto viewJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
  auto logicalCollection = vocbase->createCollection(collectionJson->slice());
  REQUIRE((false == !logicalCollection));
  arangodb::LogicalView::ptr wiew;
  REQUIRE((arangodb::iresearch::IResearchView::factory().create(wiew, *vocbase, viewJson->slice()).ok()));
  CHECK((false == !wiew));
  auto* impl = dynamic_cast<arangodb::iresearch::IResearchView*>(wiew.get());
  CHECK((nullptr != impl));

  // ensure we have shard view in vocbase
  bool created;
  auto index = logicalCollection->createIndex(linkJson->slice(), created);
  REQUIRE((false == !index));
  auto link = std::dynamic_pointer_cast<arangodb::iresearch::IResearchLink>(index);
  REQUIRE((false == !link));

  static auto visitor = [](TRI_voc_cid_t)->bool { return false; };
  CHECK((false == impl->visitCollections(visitor)));
  CHECK((false == !arangodb::iresearch::IResearchLinkHelper::find(*logicalCollection, *wiew)));
  CHECK((impl->unlink(logicalCollection->id()).ok()));
  CHECK((false == !arangodb::iresearch::IResearchLinkHelper::find(*logicalCollection, *wiew)));
  CHECK((true == impl->visitCollections(visitor)));
  CHECK((impl->unlink(logicalCollection->id()).ok()));
}

SECTION("test_drop_database") {
  auto* ci = arangodb::ClusterInfo::instance();
  REQUIRE((nullptr != ci));
  auto* databaseFeature = arangodb::application_features::ApplicationServer::getFeature<arangodb::DatabaseFeature>("Database");
  REQUIRE((nullptr != databaseFeature));

  auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
  auto viewCreateJson = arangodb::velocypack::Parser::fromJson("{ \"id\": \"42\", \"name\": \"testView\", \"type\": \"arangosearch\" }");
  auto viewUpdateJson = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection\": { \"includeAllFields\": true } } }");

  size_t beforeCount = 0;
  auto before = PhysicalCollectionMock::before;
  auto restore = irs::make_finally([&before]()->void { PhysicalCollectionMock::before = before; });
  PhysicalCollectionMock::before = [&beforeCount]()->void { ++beforeCount; };

  TRI_vocbase_t* vocbase; // will be owned by DatabaseFeature
  REQUIRE((TRI_ERROR_NO_ERROR == databaseFeature->createDatabase(0, "testDatabase" TOSTRING(__LINE__), vocbase)));
  REQUIRE((nullptr != vocbase));
  REQUIRE((ci->createDatabaseCoordinator(vocbase->name(), arangodb::velocypack::Slice::emptyObjectSlice(), 0.).ok()));
  auto logicalCollection = vocbase->createCollection(collectionJson->slice());
  REQUIRE((false == !logicalCollection));
  CHECK((ci->createViewCoordinator(vocbase->name(), "42", viewCreateJson->slice()).ok()));
  auto logicalWiew = ci->getView(vocbase->name(), "42"); // link creation requires cluster-view to be in ClusterInfo instead of TRI_vocbase_t
  REQUIRE((false == !logicalWiew));
  auto* wiewImpl = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalWiew.get());
  REQUIRE((false == !wiewImpl));

  beforeCount = 0; // reset before call to StorageEngine::createView(...)
  auto res = logicalWiew->properties(viewUpdateJson->slice(), true);
  REQUIRE(true == res.ok());
  CHECK((1 == beforeCount)); // +1 for StorageEngineMock::createIndex(...)

  beforeCount = 0; // reset before call to StorageEngine::dropView(...)
  CHECK((TRI_ERROR_NO_ERROR == databaseFeature->dropDatabase(vocbase->id(), true, true)));
  CHECK((0 == beforeCount));
}

SECTION("test_ensure") {
  auto* database = arangodb::DatabaseFeature::DATABASE;
  REQUIRE((nullptr != database));
  auto* ci = arangodb::ClusterInfo::instance();
  REQUIRE((nullptr != ci));
  TRI_vocbase_t* vocbase; // will be owned by DatabaseFeature

  // create database
  {
    // simulate heartbeat thread
    REQUIRE(TRI_ERROR_NO_ERROR == database->createDatabase(1, "testDatabase", vocbase));

    REQUIRE(nullptr != vocbase);
    CHECK("testDatabase" == vocbase->name());
    CHECK(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL == vocbase->type());
    CHECK(1 == vocbase->id());

    CHECK((ci->createDatabaseCoordinator(vocbase->name(), VPackSlice::emptyObjectSlice(), 0.0).ok()));
  }

  auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
  auto linkJson = arangodb::velocypack::Parser::fromJson("{ \"view\": \"testView\", \"type\": \"arangosearch\", \"includeAllFields\": true }");
  auto viewJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\", \"collections\": [ 3, 4, 5 ] }");
  auto logicalCollection = vocbase->createCollection(collectionJson->slice());
  REQUIRE((false == !logicalCollection));
  arangodb::LogicalView::ptr wiew;
  REQUIRE((arangodb::iresearch::IResearchView::factory().create(wiew, *vocbase, viewJson->slice()).ok()));
  CHECK((false == !wiew));
  auto* impl = dynamic_cast<arangodb::iresearch::IResearchView*>(wiew.get());
  CHECK((nullptr != impl));

  bool created;
  auto index = logicalCollection->createIndex(linkJson->slice(), created);
  REQUIRE((false == !index));
  auto link = std::dynamic_pointer_cast<arangodb::iresearch::IResearchLink>(index);
  REQUIRE((false == !link));

  static auto visitor = [](TRI_voc_cid_t)->bool { return false; };
  CHECK((false == wiew->visitCollections(visitor))); // no collections in view
  CHECK((false == !arangodb::iresearch::IResearchLinkHelper::find(*logicalCollection, *wiew)));
}

SECTION("test_make") {
  auto* ci = arangodb::ClusterInfo::instance();
  REQUIRE(nullptr != ci);

  // make DBServer view
  {
    auto const wiewId = ci->uniqid() + 1; // +1 because LogicalView creation will generate a new ID
    auto json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,  "testVocbase");
    arangodb::LogicalView::ptr wiew;
    REQUIRE((arangodb::iresearch::IResearchView::factory().instantiate(wiew, vocbase, json->slice(), 42).ok()));
    CHECK((false == !wiew));
    auto* impl = dynamic_cast<arangodb::iresearch::IResearchView*>(wiew.get());
    CHECK((nullptr != impl));

    CHECK((std::string("testView") == wiew->name()));
    CHECK((false == wiew->deleted()));
    CHECK((wiewId == wiew->id()));
    CHECK((impl->id() == wiew->planId())); // same as view ID
    CHECK((42 == wiew->planVersion())); // when creating via vocbase planVersion is always 0
    CHECK((arangodb::iresearch::DATA_SOURCE_TYPE == wiew->type()));
    CHECK((&vocbase == &(wiew->vocbase())));
  }
}

SECTION("test_open") {
  auto* ci = arangodb::ClusterInfo::instance();
  REQUIRE(nullptr != ci);

  // open empty
  {
    auto json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    arangodb::LogicalView::ptr wiew;
    REQUIRE((arangodb::iresearch::IResearchView::factory().instantiate(wiew, vocbase, json->slice(), 42).ok()));
    CHECK((false == !wiew));
    auto* impl = dynamic_cast<arangodb::iresearch::IResearchView*>(wiew.get());
    CHECK((nullptr != impl));

    static auto visitor = [](TRI_voc_cid_t)->bool { return false; };
    CHECK((true == impl->visitCollections(visitor)));
    wiew->open();
  }

  // open non-empty
  {
    auto const wiewId = std::to_string(ci->uniqid() + 1); // +1 because LogicalView creation will generate a new ID
    std::string dataPath = (((irs::utf8_path()/=s.testFilesystemPath)/=std::string("databases"))/=std::string("arangosearch-123")).utf8();
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
    auto json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    REQUIRE((false == !logicalCollection));
    arangodb::LogicalView::ptr wiew;
    REQUIRE((arangodb::iresearch::IResearchView::factory().instantiate(wiew, vocbase, json->slice(), 42).ok()));
    CHECK((false == !wiew));
    auto* impl = dynamic_cast<arangodb::iresearch::IResearchView*>(wiew.get());
    CHECK((nullptr != impl));

    // ensure we have shard view in vocbase
    struct Link: public arangodb::iresearch::IResearchLink {
      Link(TRI_idx_iid_t id, arangodb::LogicalCollection& col): IResearchLink(id, col) {}
    } link(42, *logicalCollection);
    auto asyncLinkPtr = std::make_shared<arangodb::iresearch::IResearchLink::AsyncLinkPtr::element_type>(&link);

    static auto visitor = [](TRI_voc_cid_t)->bool { return false; };
    CHECK((true == impl->visitCollections(visitor)));
    CHECK((true == impl->link(asyncLinkPtr).ok()));
    CHECK((false == impl->visitCollections(visitor)));
    wiew->open();
  }
}

SECTION("test_query") {
  auto* database = arangodb::DatabaseFeature::DATABASE;
  REQUIRE((nullptr != database));
  auto* ci = arangodb::ClusterInfo::instance();
  REQUIRE((nullptr != ci));
  auto* databaseFeature = arangodb::application_features::ApplicationServer::getFeature<arangodb::DatabaseFeature>("Database");
  REQUIRE((nullptr != databaseFeature));

  auto createJson = arangodb::velocypack::Parser::fromJson("{ \
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
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
    auto linkJson = arangodb::velocypack::Parser::fromJson("{ \"view\": \"testView\", \"type\": \"arangosearch\", \"includeAllFields\": true }");
    TRI_vocbase_t* vocbase; // will be owned by DatabaseFeature
    REQUIRE((TRI_ERROR_NO_ERROR == database->createDatabase(1, "testDatabase0", vocbase)));
    CHECK((ci->createDatabaseCoordinator(vocbase->name(), VPackSlice::emptyObjectSlice(), 0.0).ok()));
    auto logicalCollection = vocbase->createCollection(collectionJson->slice());
    REQUIRE(nullptr != logicalCollection);
    arangodb::LogicalView::ptr logicalWiew;
    REQUIRE((arangodb::iresearch::IResearchView::factory().create(logicalWiew, *vocbase, createJson->slice()).ok()));
    REQUIRE((false == !logicalWiew));
    auto* wiewImpl = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalWiew.get());
    REQUIRE((false == !wiewImpl));

    bool created;
    auto index = logicalCollection->createIndex(linkJson->slice(), created);
    REQUIRE((false == !index));
    auto link = std::dynamic_pointer_cast<arangodb::iresearch::IResearchLink>(index);
    REQUIRE((false == !link));

    arangodb::transaction::Methods trx(
      arangodb::transaction::StandaloneContext::Create(*vocbase),
      std::vector<std::string>{logicalCollection->name()},
      EMPTY,
      EMPTY,
      arangodb::transaction::Options()
    );
    CHECK((trx.begin().ok()));
    arangodb::HashSet<TRI_voc_cid_t> collections;
    collections.insert(logicalCollection->id());
    auto* snapshot = wiewImpl->snapshot(trx, arangodb::iresearch::IResearchView::SnapshotMode::FindOrCreate, &collections);
    CHECK(0 == snapshot->docs_count());
    CHECK((trx.commit().ok()));
  }

  // ordered iterator
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
    auto linkJson = arangodb::velocypack::Parser::fromJson("{ \"view\": \"testView\", \"type\": \"arangosearch\", \"includeAllFields\": true }");
    TRI_vocbase_t* vocbase; // will be owned by DatabaseFeature
    REQUIRE((TRI_ERROR_NO_ERROR == database->createDatabase(1, "testDatabase1", vocbase)));
    CHECK((ci->createDatabaseCoordinator(vocbase->name(), VPackSlice::emptyObjectSlice(), 0.0).ok()));
    auto logicalCollection = vocbase->createCollection(collectionJson->slice());
    REQUIRE(nullptr != logicalCollection);
    arangodb::LogicalView::ptr logicalWiew;
    REQUIRE((arangodb::iresearch::IResearchView::factory().create(logicalWiew, *vocbase, createJson->slice()).ok()));
    REQUIRE((false == !logicalWiew));
    auto* wiewImpl = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalWiew.get());
    REQUIRE((false == !wiewImpl));

    bool created;
    auto index = logicalCollection->createIndex(linkJson->slice(), created);
    REQUIRE((false == !index));
    auto link = std::dynamic_pointer_cast<arangodb::iresearch::IResearchLink>(index);
    REQUIRE((false == !link));

    // fill with test data
    {
      auto doc = arangodb::velocypack::Parser::fromJson("{ \"key\": 1 }");
      arangodb::iresearch::IResearchLinkMeta meta;
      meta._includeAllFields = true;
      arangodb::transaction::Methods trx(
        arangodb::transaction::StandaloneContext::Create(*vocbase),
        EMPTY,
        std::vector<std::string>{logicalCollection->name()},
        EMPTY,
        arangodb::transaction::Options()
      );
      CHECK((trx.begin().ok()));

      for (size_t i = 0; i < 12; ++i) {
        CHECK((link->insert(trx, arangodb::LocalDocumentId(i), doc->slice(), arangodb::Index::OperationMode::normal).ok()));
      }

      CHECK((trx.commit().ok()));
      CHECK(link->commit().ok());
    }

    arangodb::transaction::Methods trx(
      arangodb::transaction::StandaloneContext::Create(*vocbase),
      std::vector<std::string>{logicalCollection->name()},
      EMPTY,
      EMPTY,
      arangodb::transaction::Options()
    );
    CHECK((trx.begin().ok()));
    arangodb::HashSet<TRI_voc_cid_t> collections;
    collections.insert(logicalCollection->id());
    auto* snapshot = wiewImpl->snapshot(trx, arangodb::iresearch::IResearchView::SnapshotMode::FindOrCreate, &collections);
    CHECK(12 == snapshot->docs_count());
    CHECK((trx.commit().ok()));
  }

  // snapshot isolation
  {
    auto links = arangodb::velocypack::Parser::fromJson("{ \
      \"links\": { \"testCollection\": { \"includeAllFields\" : true } } \
    }");
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\", \"id\":442 }");

    TRI_vocbase_t* vocbase; // will be owned by DatabaseFeature
    REQUIRE((TRI_ERROR_NO_ERROR == databaseFeature->createDatabase(0, "testDatabase" TOSTRING(__LINE__), vocbase)));
    REQUIRE((nullptr != vocbase));
    REQUIRE((ci->createDatabaseCoordinator(vocbase->name(), arangodb::velocypack::Slice::emptyObjectSlice(), 0.).ok()));
    auto logicalCollection = vocbase->createCollection(collectionJson->slice());
    std::vector<std::string> collections{ logicalCollection->name() };
    CHECK((ci->createViewCoordinator(vocbase->name(), "42", createJson->slice()).ok()));
    auto logicalWiew = ci->getView(vocbase->name(), "42"); // link creation requires cluster-view to be in ClusterInfo instead of TRI_vocbase_t
    CHECK((false == !logicalWiew));
    auto* wiewImpl = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalWiew.get());
    CHECK((false == !wiewImpl));
    arangodb::Result res = logicalWiew->properties(links->slice(), true);
    CHECK(true == res.ok());
    CHECK((false == logicalCollection->getIndexes().empty()));

    // fill with test data
    {
      arangodb::transaction::Methods trx(
        arangodb::transaction::StandaloneContext::Create(*vocbase),
        EMPTY,
        collections,
        EMPTY,
        arangodb::transaction::Options()
      );
      CHECK((trx.begin().ok()));

      arangodb::ManagedDocumentResult inserted;
      arangodb::OperationOptions options;
      for (size_t i = 1; i <= 12; ++i) {
        auto doc = arangodb::velocypack::Parser::fromJson(std::string("{ \"key\": ") + std::to_string(i) + " }");
        logicalCollection->insert(&trx, doc->slice(), inserted, options, false);
      }

      CHECK((trx.commit().ok()));
    }

    arangodb::transaction::Options trxOptions;

    arangodb::transaction::Methods trx0(
      arangodb::transaction::StandaloneContext::Create(*vocbase),
      collections,
      EMPTY,
      EMPTY,
      trxOptions
    );
    CHECK((trx0.begin().ok()));
    arangodb::HashSet<TRI_voc_cid_t> collectionIds;
    collectionIds.insert(logicalCollection->id());
    CHECK((nullptr == wiewImpl->snapshot(trx0, arangodb::iresearch::IResearchView::SnapshotMode::Find, &collectionIds)));
    auto* snapshot0 = wiewImpl->snapshot(trx0, arangodb::iresearch::IResearchView::SnapshotMode::SyncAndReplace, &collectionIds);
    CHECK((snapshot0 == wiewImpl->snapshot(trx0, arangodb::iresearch::IResearchView::SnapshotMode::Find, &collectionIds)));
    CHECK(12 == snapshot0->docs_count());
    CHECK((trx0.commit().ok()));

    // add more data
    {
      arangodb::transaction::Methods trx(
        arangodb::transaction::StandaloneContext::Create(*vocbase),
        EMPTY,
        collections,
        EMPTY,
        arangodb::transaction::Options()
      );
      CHECK((trx.begin().ok()));

      arangodb::ManagedDocumentResult inserted;
      arangodb::OperationOptions options;
      for (size_t i = 13; i <= 24; ++i) {
        auto doc = arangodb::velocypack::Parser::fromJson(std::string("{ \"key\": ") + std::to_string(i) + " }");
        logicalCollection->insert(&trx, doc->slice(), inserted, options, false);
      }

      CHECK(trx.commit().ok());
    }

    // old reader sees same data as before
    CHECK(12 == snapshot0->docs_count());

    // new reader sees new data
    arangodb::transaction::Methods trx1(
      arangodb::transaction::StandaloneContext::Create(*vocbase),
      collections,
      EMPTY,
      EMPTY,
      trxOptions
    );
    CHECK((trx1.begin().ok()));
    auto* snapshot1 = wiewImpl->snapshot(trx1, arangodb::iresearch::IResearchView::SnapshotMode::SyncAndReplace, &collectionIds);
    CHECK(24 == snapshot1->docs_count());
    CHECK((trx1.commit().ok()));
  }

  // query while running FlushThread
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
    auto viewCreateJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
    auto viewUpdateJson = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection\": { \"includeAllFields\": true } } }");
    auto* feature = arangodb::application_features::ApplicationServer::lookupFeature<
      arangodb::FlushFeature
    >("Flush");
    REQUIRE(feature);
    TRI_vocbase_t* vocbase; // will be owned by DatabaseFeature
    REQUIRE((TRI_ERROR_NO_ERROR == databaseFeature->createDatabase(0, "testDatabase" TOSTRING(__LINE__), vocbase)));
    REQUIRE((nullptr != vocbase));
    REQUIRE((ci->createDatabaseCoordinator(vocbase->name(), arangodb::velocypack::Slice::emptyObjectSlice(), 0.).ok()));
    auto logicalCollection = vocbase->createCollection(collectionJson->slice());
    CHECK((ci->createViewCoordinator(vocbase->name(), "42", createJson->slice()).ok()));
    auto logicalWiew = ci->getView(vocbase->name(), "42"); // link creation requires cluster-view to be in ClusterInfo instead of TRI_vocbase_t
    REQUIRE((false == !logicalWiew));
    auto* wiewImpl = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalWiew.get());
    REQUIRE((false == !wiewImpl));
    arangodb::Result res = logicalWiew->properties(viewUpdateJson->slice(), true);
    REQUIRE(true == res.ok());

    static std::vector<std::string> const EMPTY;
    arangodb::transaction::Options options;

    arangodb::aql::Variable variable("testVariable", 0);

    // test insert + query
    for (size_t i = 1; i < 200; ++i) {
      // insert
      {
        auto doc = arangodb::velocypack::Parser::fromJson(std::string("{ \"seq\": ") + std::to_string(i) + " }");
        arangodb::transaction::Methods trx(
          arangodb::transaction::StandaloneContext::Create(*vocbase),
          EMPTY,
          std::vector<std::string>{logicalCollection->name()},
          EMPTY,
          options
        );

        CHECK((trx.begin().ok()));
        CHECK((trx.insert(logicalCollection->name(), doc->slice(), arangodb::OperationOptions()).ok()));
        CHECK((trx.commit().ok()));
      }

      // query
      {
        arangodb::transaction::Methods trx(
          arangodb::transaction::StandaloneContext::Create(*vocbase),
          std::vector<std::string>{logicalCollection->name()},
          EMPTY,
          EMPTY,
          arangodb::transaction::Options{}
        );
        CHECK((trx.begin().ok()));
        arangodb::HashSet<TRI_voc_cid_t> collections;
        collections.insert(logicalCollection->id());
        auto* snapshot = wiewImpl->snapshot(trx, arangodb::iresearch::IResearchView::SnapshotMode::SyncAndReplace, &collections);
        CHECK(i == snapshot->docs_count());
        CHECK((trx.commit().ok()));
      }
    }
  }
}

SECTION("test_rename") {
  auto* ci = arangodb::ClusterInfo::instance();
  REQUIRE(nullptr != ci);

  // rename empty
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
    auto json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    REQUIRE((false == !logicalCollection));
    arangodb::LogicalView::ptr wiew;
    REQUIRE((arangodb::iresearch::IResearchView::factory().instantiate(wiew, vocbase, json->slice(), 42).ok()));
    CHECK((false == !wiew));
    auto* impl = dynamic_cast<arangodb::iresearch::IResearchView*>(wiew.get());
    CHECK((nullptr != impl));

    CHECK((std::string("testView") == wiew->name()));

    {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      wiew->properties(builder, false, false);
      builder.close();
      CHECK((builder.slice().hasKey("name")));
      CHECK((std::string("testView") == builder.slice().get("name").copyString()));
    }

    CHECK((TRI_ERROR_CLUSTER_UNSUPPORTED == wiew->rename("newName").errorNumber()));

    {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      wiew->properties(builder, false, false);
      builder.close();
      CHECK((builder.slice().hasKey("name")));
      CHECK((std::string("testView") == builder.slice().get("name").copyString()));
    }

    struct Link: public arangodb::iresearch::IResearchLink {
      Link(TRI_idx_iid_t id, arangodb::LogicalCollection& col): IResearchLink(id, col) {}
    } link(42, *logicalCollection);
    auto asyncLinkPtr = std::make_shared<arangodb::iresearch::IResearchLink::AsyncLinkPtr::element_type>(&link);
    CHECK((true == impl->link(asyncLinkPtr).ok()));
  }

  // rename non-empty
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
    auto const wiewId = std::to_string(ci->uniqid() + 1); // +1 because LogicalView creation will generate a new ID
    auto json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    REQUIRE((false == !logicalCollection));
    arangodb::LogicalView::ptr wiew;
    REQUIRE((arangodb::iresearch::IResearchView::factory().instantiate(wiew, vocbase, json->slice(), 42).ok()));
    CHECK((false == !wiew));
    auto* impl = dynamic_cast<arangodb::iresearch::IResearchView*>(wiew.get());
    CHECK((nullptr != impl));

    // ensure we have shard view in vocbase
    struct Link: public arangodb::iresearch::IResearchLink {
      Link(TRI_idx_iid_t id, arangodb::LogicalCollection& col): IResearchLink(id, col) {}
    } link(42, *logicalCollection);
    auto asyncLinkPtr = std::make_shared<arangodb::iresearch::IResearchLink::AsyncLinkPtr::element_type>(&link);
    CHECK((true == impl->link(asyncLinkPtr).ok()));

    {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      wiew->properties(builder, false, false);
      builder.close();
      CHECK((builder.slice().hasKey("name")));
      CHECK((std::string("testView") == builder.slice().get("name").copyString()));
    }

    CHECK((TRI_ERROR_CLUSTER_UNSUPPORTED == wiew->rename("newName").errorNumber()));

    {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      wiew->properties(builder, false, false);
      builder.close();
      CHECK((builder.slice().hasKey("name")));
      CHECK((std::string("testView") == builder.slice().get("name").copyString()));
    }

    wiew->rename("testView"); // rename back or vocbase will be out of sync
  }
}

SECTION("test_toVelocyPack") {
  // base
  {
    auto json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\", \"unusedKey\": \"unusedValue\" }");
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    arangodb::LogicalView::ptr wiew;
    REQUIRE((arangodb::iresearch::IResearchView::factory().instantiate(wiew, vocbase, json->slice(), 42).ok()));
    CHECK((false == !wiew));
    auto* impl = dynamic_cast<arangodb::iresearch::IResearchView*>(wiew.get());
    CHECK((nullptr != impl));

    arangodb::velocypack::Builder builder;

    builder.openObject();
    wiew->properties(builder,false, false);
    builder.close();
    auto slice = builder.slice();
    CHECK((4U == slice.length()));
    CHECK((slice.hasKey("globallyUniqueId") && slice.get("globallyUniqueId").isString() && false == slice.get("globallyUniqueId").copyString().empty()));
    CHECK((slice.hasKey("id") && slice.get("id").isString() && std::string("1") == slice.get("id").copyString()));
    CHECK((slice.hasKey("name") && slice.get("name").isString() && std::string("testView") == slice.get("name").copyString()));
    CHECK((slice.hasKey("type") && slice.get("type").isString() && arangodb::iresearch::DATA_SOURCE_TYPE.name() == slice.get("type").copyString()));
  }

  // includeProperties
  {
    auto json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\", \"unusedKey\": \"unusedValue\" }");
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    arangodb::LogicalView::ptr wiew;
    REQUIRE((arangodb::iresearch::IResearchView::factory().instantiate(wiew, vocbase, json->slice(), 42).ok()));
    CHECK((false == !wiew));
    auto* impl = dynamic_cast<arangodb::iresearch::IResearchView*>(wiew.get());
    CHECK((nullptr != impl));

    arangodb::velocypack::Builder builder;

    builder.openObject();
    wiew->properties(builder, true, false);
    builder.close();
    auto slice = builder.slice();
    CHECK((13U == slice.length()));
    CHECK((slice.hasKey("globallyUniqueId") && slice.get("globallyUniqueId").isString() && false == slice.get("globallyUniqueId").copyString().empty()));
    CHECK((slice.hasKey("id") && slice.get("id").isString() && std::string("2") == slice.get("id").copyString()));
    CHECK((slice.hasKey("name") && slice.get("name").isString() && std::string("testView") == slice.get("name").copyString()));
    CHECK((slice.hasKey("type") && slice.get("type").isString() && arangodb::iresearch::DATA_SOURCE_TYPE.name() == slice.get("type").copyString()));
  }

  // includeSystem
  {
    auto json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\", \"unusedKey\": \"unusedValue\" }");
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    arangodb::LogicalView::ptr wiew;
    REQUIRE((arangodb::iresearch::IResearchView::factory().instantiate(wiew, vocbase, json->slice(), 42).ok()));
    CHECK((false == !wiew));
    auto* impl = dynamic_cast<arangodb::iresearch::IResearchView*>(wiew.get());
    CHECK((nullptr != impl));

    arangodb::velocypack::Builder builder;

    builder.openObject();
    wiew->properties(builder, false, true);
    builder.close();
    auto slice = builder.slice();
    CHECK((7 == slice.length()));
    CHECK((slice.hasKey("deleted") && slice.get("deleted").isBoolean() && false == slice.get("deleted").getBoolean()));
    CHECK((slice.hasKey("globallyUniqueId") && slice.get("globallyUniqueId").isString() && false == slice.get("globallyUniqueId").copyString().empty()));
    CHECK((slice.hasKey("id") && slice.get("id").isString() && std::string("3") == slice.get("id").copyString()));
    CHECK((slice.hasKey("isSystem") && slice.get("isSystem").isBoolean() && false == slice.get("isSystem").getBoolean()));
    CHECK((slice.hasKey("name") && slice.get("name").isString() && std::string("testView") == slice.get("name").copyString()));
    CHECK((slice.hasKey("planId") && slice.get("planId").isString() && std::string("3") == slice.get("planId").copyString()));
    CHECK((slice.hasKey("type") && slice.get("type").isString() && arangodb::iresearch::DATA_SOURCE_TYPE.name() == slice.get("type").copyString()));
  }
}

SECTION("test_transaction_snapshot") {
  auto* database = arangodb::DatabaseFeature::DATABASE;
  REQUIRE((nullptr != database));
  auto* ci = arangodb::ClusterInfo::instance();
  REQUIRE((nullptr != ci));
  TRI_vocbase_t* vocbase; // will be owned by DatabaseFeature

  // create database
  {
    // simulate heartbeat thread
    REQUIRE(TRI_ERROR_NO_ERROR == database->createDatabase(1, "testDatabase", vocbase));

    REQUIRE(nullptr != vocbase);
    CHECK("testDatabase" == vocbase->name());
    CHECK(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL == vocbase->type());
    CHECK(1 == vocbase->id());

    CHECK((ci->createDatabaseCoordinator(vocbase->name(), VPackSlice::emptyObjectSlice(), 0.0).ok()));
  }

  static std::vector<std::string> const EMPTY;
  auto viewJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\", \"consolidationIntervalMsec\": 0 }");
  auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
  auto linkJson = arangodb::velocypack::Parser::fromJson("{ \"view\": \"testView\", \"type\": \"arangosearch\", \"includeAllFields\": true }");
  auto logicalCollection = vocbase->createCollection(collectionJson->slice());
  REQUIRE(nullptr != logicalCollection);
  arangodb::LogicalView::ptr logicalWiew;
  REQUIRE((arangodb::iresearch::IResearchView::factory().create(logicalWiew, *vocbase, viewJson->slice()).ok()));
  REQUIRE((false == !logicalWiew));
  auto* wiewImpl = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalWiew.get());
  REQUIRE((nullptr != wiewImpl));

  bool created;
  auto index = logicalCollection->createIndex(linkJson->slice(), created);
  REQUIRE((false == !index));
  auto link = std::dynamic_pointer_cast<arangodb::iresearch::IResearchLink>(index);
  REQUIRE((false == !link));

  // add a single document to view (do not sync)
  {
    auto doc = arangodb::velocypack::Parser::fromJson("{ \"key\": 1 }");
    arangodb::iresearch::IResearchLinkMeta meta;
    meta._includeAllFields = true;
    arangodb::transaction::Methods trx(
      arangodb::transaction::StandaloneContext::Create(*vocbase),
      EMPTY,
      std::vector<std::string>{logicalCollection->name()},
      EMPTY,
      arangodb::transaction::Options()
    );
    CHECK((trx.begin().ok()));
    CHECK((link->insert(trx, arangodb::LocalDocumentId(0), doc->slice(), arangodb::Index::OperationMode::normal).ok()));
    CHECK((trx.commit().ok()));
  }

  // no snapshot in TransactionState (force == false, waitForSync = false)
  {
    arangodb::transaction::Methods trx(
      arangodb::transaction::StandaloneContext::Create(*vocbase),
      std::vector<std::string>{logicalCollection->name()},
      EMPTY,
      EMPTY,
      arangodb::transaction::Options()
    );
    CHECK((trx.begin().ok()));
    arangodb::HashSet<TRI_voc_cid_t> collections;
    collections.insert(logicalCollection->id());
    auto* snapshot = wiewImpl->snapshot(trx, arangodb::iresearch::IResearchView::SnapshotMode::Find, &collections);
    CHECK((nullptr == snapshot));
    CHECK((trx.commit().ok()));
  }

  // no snapshot in TransactionState (force == true, waitForSync = false)
  {
    arangodb::transaction::Methods trx(
      arangodb::transaction::StandaloneContext::Create(*vocbase),
      std::vector<std::string>{logicalCollection->name()},
      EMPTY,
      EMPTY,
      arangodb::transaction::Options()
    );
    CHECK((trx.begin().ok()));
    arangodb::HashSet<TRI_voc_cid_t> collections;
    collections.insert(logicalCollection->id());
    CHECK((nullptr == wiewImpl->snapshot(trx, arangodb::iresearch::IResearchView::SnapshotMode::Find, &collections)));
    auto* snapshot = wiewImpl->snapshot(trx, arangodb::iresearch::IResearchView::SnapshotMode::FindOrCreate, &collections);
    CHECK((snapshot == wiewImpl->snapshot(trx, arangodb::iresearch::IResearchView::SnapshotMode::FindOrCreate, &collections)));
    CHECK((0 == snapshot->live_docs_count()));
    CHECK((trx.commit().ok()));
  }

  // no snapshot in TransactionState (force == false, waitForSync = true)
  {
    arangodb::transaction::Options opts;
    opts.waitForSync = true;
    arangodb::transaction::Methods trx(
      arangodb::transaction::StandaloneContext::Create(*vocbase),
      std::vector<std::string>{logicalCollection->name()},
      EMPTY,
      EMPTY,
      opts
    );
    CHECK((trx.begin().ok()));
    arangodb::HashSet<TRI_voc_cid_t> collections;
    collections.insert(logicalCollection->id());
    auto* snapshot = wiewImpl->snapshot(trx, arangodb::iresearch::IResearchView::SnapshotMode::Find, &collections);
    CHECK((nullptr == snapshot));
    CHECK((trx.commit().ok()));
  }

  // no snapshot in TransactionState (force == true, waitForSync = true)
  {
    arangodb::transaction::Options opts;
    arangodb::transaction::Methods trx(
      arangodb::transaction::StandaloneContext::Create(*vocbase),
      std::vector<std::string>{logicalCollection->name()},
      EMPTY,
      EMPTY,
      opts
    );
    CHECK((trx.begin().ok()));
    arangodb::HashSet<TRI_voc_cid_t> collections;
    collections.insert(logicalCollection->id());
    CHECK((nullptr == wiewImpl->snapshot(trx, arangodb::iresearch::IResearchView::SnapshotMode::Find, &collections)));
    auto* snapshot = wiewImpl->snapshot(trx, arangodb::iresearch::IResearchView::SnapshotMode::SyncAndReplace, &collections);
    CHECK((snapshot == wiewImpl->snapshot(trx, arangodb::iresearch::IResearchView::SnapshotMode::Find, &collections)));
    CHECK((snapshot == wiewImpl->snapshot(trx, arangodb::iresearch::IResearchView::SnapshotMode::FindOrCreate, &collections)));
    CHECK((nullptr != snapshot));
    CHECK((1 == snapshot->live_docs_count()));
    CHECK((trx.commit().ok()));
  }
}

SECTION("test_updateProperties") {
  auto* ci = arangodb::ClusterInfo::instance();
  REQUIRE((nullptr != ci));
  auto* databaseFeature = arangodb::application_features::ApplicationServer::getFeature<arangodb::DatabaseFeature>("Database");
  REQUIRE((nullptr != databaseFeature));

  // update empty (partial)
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
    auto viewJson = arangodb::velocypack::Parser::fromJson("{ \"id\": \"42\", \"name\": \"testView\", \"type\": \"arangosearch\", \"collections\": [ 3, 4, 5 ], \"cleanupIntervalStep\": 24, \"consolidationIntervalMsec\": 42 }");
    TRI_vocbase_t* vocbase; // will be owned by DatabaseFeature
    REQUIRE((TRI_ERROR_NO_ERROR == databaseFeature->createDatabase(0, "testDatabase" TOSTRING(__LINE__), vocbase)));
    REQUIRE((nullptr != vocbase));
    REQUIRE((ci->createDatabaseCoordinator(vocbase->name(), arangodb::velocypack::Slice::emptyObjectSlice(), 0.).ok()));
    auto logicalCollection = vocbase->createCollection(collectionJson->slice());
    CHECK((nullptr != logicalCollection));
    CHECK((ci->createViewCoordinator(vocbase->name(), "42", viewJson->slice()).ok()));
    auto wiew = ci->getView(vocbase->name(), "42"); // link creation requires cluster-view to be in ClusterInfo instead of TRI_vocbase_t
    CHECK((false == !wiew));
    auto* impl = dynamic_cast<arangodb::iresearch::IResearchView*>(wiew.get());
    CHECK((nullptr != impl));

    {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      wiew->properties(builder, true, false);
      builder.close();

      auto slice = builder.slice();
      CHECK((slice.isObject()));
      CHECK((13U == slice.length()));
      CHECK((slice.hasKey("cleanupIntervalStep") && slice.get("cleanupIntervalStep").isNumber<size_t>() && 24 == slice.get("cleanupIntervalStep").getNumber<size_t>()));
      CHECK((slice.hasKey("consolidationIntervalMsec") && slice.get("consolidationIntervalMsec").isNumber<size_t>() && 42 == slice.get("consolidationIntervalMsec").getNumber<size_t>()));
      CHECK((slice.hasKey("links") && slice.get("links").isObject() && 0 == slice.get("links").length()));
    }

    {
      auto update = arangodb::velocypack::Parser::fromJson("{ \"collections\": [ 6, 7, 8, 9 ], \"consolidationIntervalMsec\": 52, \"links\": { \"testCollection\": {} } }");
      CHECK((true == wiew->properties(update->slice(), true).ok()));
    }

    {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      wiew->properties(builder, true, false);
      builder.close();

      auto slice = builder.slice();
      CHECK((slice.isObject()));
      CHECK((13U == slice.length()));
      CHECK((slice.hasKey("cleanupIntervalStep") && slice.get("cleanupIntervalStep").isNumber<size_t>() && 24 == slice.get("cleanupIntervalStep").getNumber<size_t>()));
      CHECK((slice.hasKey("consolidationIntervalMsec") && slice.get("consolidationIntervalMsec").isNumber<size_t>() && 52 == slice.get("consolidationIntervalMsec").getNumber<size_t>()));
      CHECK((slice.hasKey("links") && slice.get("links").isObject() && 1 == slice.get("links").length()));
    }


    CHECK((false == !arangodb::iresearch::IResearchLinkHelper::find(*logicalCollection, *wiew)));
    static auto visitor = [](TRI_voc_cid_t)->bool { return false; };
    CHECK((false == wiew->visitCollections(visitor))); // no collections in view

    // not for persistence
    {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      wiew->properties(builder, true, false);
      builder.close();

      auto slice = builder.slice();
      CHECK((slice.isObject()));
      CHECK((13U == slice.length()));
      CHECK((slice.hasKey("cleanupIntervalStep") && slice.get("cleanupIntervalStep").isNumber<size_t>() && 24 == slice.get("cleanupIntervalStep").getNumber<size_t>()));
      CHECK((slice.hasKey("consolidationIntervalMsec") && slice.get("consolidationIntervalMsec").isNumber<size_t>() && 52 == slice.get("consolidationIntervalMsec").getNumber<size_t>()));
      CHECK((slice.hasKey("links") && slice.get("links").isObject() && 1 == slice.get("links").length()));
    }

    // for persistence
    {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      wiew->properties(builder, true, true);
      builder.close();

      auto slice = builder.slice();
      CHECK((slice.isObject()));
      CHECK((17U == slice.length()));
      CHECK((slice.hasKey("collections") && slice.get("collections").isArray() && 1 == slice.get("collections").length()));
      CHECK((slice.hasKey("cleanupIntervalStep") && slice.get("cleanupIntervalStep").isNumber<size_t>() && 24 == slice.get("cleanupIntervalStep").getNumber<size_t>()));
      CHECK((slice.hasKey("consolidationIntervalMsec") && slice.get("consolidationIntervalMsec").isNumber<size_t>() && 52 == slice.get("consolidationIntervalMsec").getNumber<size_t>()));
      CHECK((false == slice.hasKey("links")));
    }
  }

  // update empty (full)
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
    auto viewJson = arangodb::velocypack::Parser::fromJson("{ \"id\": \"42\", \"name\": \"testView\", \"type\": \"arangosearch\", \"collections\": [ 3, 4, 5 ], \"cleanupIntervalStep\": 24, \"consolidationIntervalMsec\": 42 }");
    TRI_vocbase_t* vocbase; // will be owned by DatabaseFeature
    REQUIRE((TRI_ERROR_NO_ERROR == databaseFeature->createDatabase(0, "testDatabase" TOSTRING(__LINE__), vocbase)));
    REQUIRE((nullptr != vocbase));
    REQUIRE((ci->createDatabaseCoordinator(vocbase->name(), arangodb::velocypack::Slice::emptyObjectSlice(), 0.).ok()));
    auto logicalCollection = vocbase->createCollection(collectionJson->slice());
    CHECK((nullptr != logicalCollection));
    CHECK((ci->createViewCoordinator(vocbase->name(), "42", viewJson->slice()).ok()));
    auto wiew = ci->getView(vocbase->name(), "42"); // link creation requires cluster-view to be in ClusterInfo instead of TRI_vocbase_t
    CHECK((false == !wiew));
    auto* impl = dynamic_cast<arangodb::iresearch::IResearchView*>(wiew.get());
    CHECK((nullptr != impl));

    {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      wiew->properties(builder, true, false);
      builder.close();

      auto slice = builder.slice();
      CHECK((slice.isObject()));
      CHECK((13U == slice.length()));
      CHECK((slice.hasKey("cleanupIntervalStep") && slice.get("cleanupIntervalStep").isNumber<size_t>() && 24 == slice.get("cleanupIntervalStep").getNumber<size_t>()));
      CHECK((slice.hasKey("consolidationIntervalMsec") && slice.get("consolidationIntervalMsec").isNumber<size_t>() && 42 == slice.get("consolidationIntervalMsec").getNumber<size_t>()));
      CHECK((slice.hasKey("links") && slice.get("links").isObject() && 0 == slice.get("links").length()));
    }

    {
      auto update = arangodb::velocypack::Parser::fromJson("{ \"collections\": [ 6, 7, 8, 9 ], \"links\": { \"testCollection\": {} }, \"consolidationIntervalMsec\": 52 }");
      CHECK((true == wiew->properties(update->slice(), false).ok()));
    }

    {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      wiew->properties(builder, true, false);
      builder.close();

      auto slice = builder.slice();
      CHECK((slice.isObject()));
      CHECK((13U == slice.length()));
      CHECK((slice.hasKey("cleanupIntervalStep") && slice.get("cleanupIntervalStep").isNumber<size_t>() && 10 == slice.get("cleanupIntervalStep").getNumber<size_t>()));
      CHECK((slice.hasKey("consolidationIntervalMsec") && slice.get("consolidationIntervalMsec").isNumber<size_t>() && 52 == slice.get("consolidationIntervalMsec").getNumber<size_t>()));
      CHECK((slice.hasKey("links") && slice.get("links").isObject() && 1 == slice.get("links").length()));
    }

    CHECK((false == !arangodb::iresearch::IResearchLinkHelper::find(*logicalCollection, *wiew)));
    static auto visitor = [](TRI_voc_cid_t)->bool { return false; };
    CHECK((false == wiew->visitCollections(visitor))); // no collections in view

    // not for persistence
    {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      wiew->properties(builder, true, false);
      builder.close();

      auto slice = builder.slice();
      CHECK((slice.isObject()));
      CHECK((13U == slice.length()));
      CHECK((slice.hasKey("cleanupIntervalStep") && slice.get("cleanupIntervalStep").isNumber<size_t>() && 10 == slice.get("cleanupIntervalStep").getNumber<size_t>()));
      CHECK((slice.hasKey("consolidationIntervalMsec") && slice.get("consolidationIntervalMsec").isNumber<size_t>() && 52 == slice.get("consolidationIntervalMsec").getNumber<size_t>()));
      CHECK((slice.hasKey("links") && slice.get("links").isObject() && 1 == slice.get("links").length()));
    }

    // for persistence
    {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      wiew->properties(builder, true, true);
      builder.close();

      auto slice = builder.slice();
      CHECK((slice.isObject()));
      CHECK((17U == slice.length()));
      CHECK((slice.hasKey("collections") && slice.get("collections").isArray() && 1 == slice.get("collections").length()));
      CHECK((slice.hasKey("cleanupIntervalStep") && slice.get("cleanupIntervalStep").isNumber<size_t>() && 10 == slice.get("cleanupIntervalStep").getNumber<size_t>()));
      CHECK((slice.hasKey("consolidationIntervalMsec") && slice.get("consolidationIntervalMsec").isNumber<size_t>() && 52 == slice.get("consolidationIntervalMsec").getNumber<size_t>()));
      CHECK((false == slice.hasKey("links")));
    }
  }

  // update non-empty (partial)
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
    auto linkJson = arangodb::velocypack::Parser::fromJson("{ \"view\": \"testView\", \"type\": \"arangosearch\", \"includeAllFields\": true }");
    auto viewJson = arangodb::velocypack::Parser::fromJson("{ \"id\": \"42\", \"name\": \"testView\", \"type\": \"arangosearch\", \"collections\": [ 3, 4, 5 ], \"cleanupIntervalStep\": 24, \"consolidationIntervalMsec\": 42 }");
    TRI_vocbase_t* vocbase; // will be owned by DatabaseFeature
    REQUIRE((TRI_ERROR_NO_ERROR == databaseFeature->createDatabase(0, "testDatabase" TOSTRING(__LINE__), vocbase)));
    REQUIRE((nullptr != vocbase));
    REQUIRE((ci->createDatabaseCoordinator(vocbase->name(), arangodb::velocypack::Slice::emptyObjectSlice(), 0.).ok()));
    auto logicalCollection = vocbase->createCollection(collectionJson->slice());
    CHECK((nullptr != logicalCollection));
    CHECK((ci->createViewCoordinator(vocbase->name(), "42", viewJson->slice()).ok()));
    auto wiew = ci->getView(vocbase->name(), "42"); // link creation requires cluster-view to be in ClusterInfo instead of TRI_vocbase_t
    CHECK((false == !wiew));
    auto* impl = dynamic_cast<arangodb::iresearch::IResearchView*>(wiew.get());
    CHECK((nullptr != impl));

    bool created;
    auto index = logicalCollection->createIndex(linkJson->slice(), created);
    REQUIRE((false == !index));
    auto link = std::dynamic_pointer_cast<arangodb::iresearch::IResearchLink>(index);
    REQUIRE((false == !link));
    static auto visitor = [](TRI_voc_cid_t)->bool { return false; };
    CHECK((false == wiew->visitCollections(visitor))); // 1 collection in view

    {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      wiew->properties(builder, true, false);
      builder.close();

      auto slice = builder.slice();
      CHECK((slice.isObject()));
      CHECK((13U == slice.length()));
      CHECK((slice.hasKey("cleanupIntervalStep") && slice.get("cleanupIntervalStep").isNumber<size_t>() && 24 == slice.get("cleanupIntervalStep").getNumber<size_t>()));
      CHECK((slice.hasKey("consolidationIntervalMsec") && slice.get("consolidationIntervalMsec").isNumber<size_t>() && 42 == slice.get("consolidationIntervalMsec").getNumber<size_t>()));
      CHECK((slice.hasKey("links") && slice.get("links").isObject() && 1 == slice.get("links").length()));
    }

    {
      auto update = arangodb::velocypack::Parser::fromJson("{ \"collections\": [ 6, 7, 8 ], \"links\": { \"testCollection\": {} }, \"consolidationIntervalMsec\": 52 }");
      CHECK((true == wiew->properties(update->slice(), true).ok()));
    }

    {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      wiew->properties(builder, true, false);
      builder.close();

      auto slice = builder.slice();
      CHECK((slice.isObject()));
      CHECK((13U == slice.length()));
      CHECK((slice.hasKey("cleanupIntervalStep") && slice.get("cleanupIntervalStep").isNumber<size_t>() && 24 == slice.get("cleanupIntervalStep").getNumber<size_t>()));
      CHECK((slice.hasKey("consolidationIntervalMsec") && slice.get("consolidationIntervalMsec").isNumber<size_t>() && 52 == slice.get("consolidationIntervalMsec").getNumber<size_t>()));
      CHECK((slice.hasKey("links") && slice.get("links").isObject() && 1 == slice.get("links").length()));
    }

    // not for persistence
    {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      wiew->properties(builder, true, false);
      builder.close();

      auto slice = builder.slice();
      CHECK((slice.isObject()));
      CHECK((13U == slice.length()));
      CHECK((slice.hasKey("cleanupIntervalStep") && slice.get("cleanupIntervalStep").isNumber<size_t>() && 24 == slice.get("cleanupIntervalStep").getNumber<size_t>()));
      CHECK((slice.hasKey("consolidationIntervalMsec") && slice.get("consolidationIntervalMsec").isNumber<size_t>() && 52 == slice.get("consolidationIntervalMsec").getNumber<size_t>()));
      CHECK((slice.hasKey("links") && slice.get("links").isObject() && 1 == slice.get("links").length()));
    }

    // for persistence
    {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      wiew->properties(builder, true, true);
      builder.close();

      auto slice = builder.slice();
      CHECK((slice.isObject()));
      CHECK((17U == slice.length()));
      CHECK((slice.hasKey("collections") && slice.get("collections").isArray() && 1 == slice.get("collections").length()));
      CHECK((slice.hasKey("cleanupIntervalStep") && slice.get("cleanupIntervalStep").isNumber<size_t>() && 24 == slice.get("cleanupIntervalStep").getNumber<size_t>()));
      CHECK((slice.hasKey("consolidationIntervalMsec") && slice.get("consolidationIntervalMsec").isNumber<size_t>() && 52 == slice.get("consolidationIntervalMsec").getNumber<size_t>()));
      CHECK((false == slice.hasKey("links")));
    }
  }

  // update non-empty (full)
  {
    auto collection0Json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
    auto collection1Json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection1\", \"id\": \"123\" }");
    auto linkJson = arangodb::velocypack::Parser::fromJson("{ \"view\": \"testView\", \"type\": \"arangosearch\", \"includeAllFields\": true }");
    auto viewJson = arangodb::velocypack::Parser::fromJson("{ \"id\": \"42\", \"name\": \"testView\", \"type\": \"arangosearch\", \"collections\": [ 3, 4, 5 ], \"cleanupIntervalStep\": 24, \"consolidationIntervalMsec\": 42 }");
    TRI_vocbase_t* vocbase; // will be owned by DatabaseFeature
    REQUIRE((TRI_ERROR_NO_ERROR == databaseFeature->createDatabase(0, "testDatabase" TOSTRING(__LINE__), vocbase)));
    REQUIRE((nullptr != vocbase));
    REQUIRE((ci->createDatabaseCoordinator(vocbase->name(), arangodb::velocypack::Slice::emptyObjectSlice(), 0.).ok()));
    auto logicalCollection0 = vocbase->createCollection(collection0Json->slice());
    CHECK((nullptr != logicalCollection0));
    auto logicalCollection1 = vocbase->createCollection(collection1Json->slice());
    CHECK((nullptr != logicalCollection1));
    CHECK((ci->createViewCoordinator(vocbase->name(), "42", viewJson->slice()).ok()));
    auto wiew = ci->getView(vocbase->name(), "42"); // link creation requires cluster-view to be in ClusterInfo instead of TRI_vocbase_t
    CHECK((false == !wiew));
    auto* impl = dynamic_cast<arangodb::iresearch::IResearchView*>(wiew.get());
    CHECK((nullptr != impl));

    bool created;
    auto index = logicalCollection1->createIndex(linkJson->slice(), created);
    REQUIRE((false == !index));
    auto link = std::dynamic_pointer_cast<arangodb::iresearch::IResearchLink>(index);
    REQUIRE((false == !link));
    static auto visitor = [](TRI_voc_cid_t)->bool { return false; };
    CHECK((false == wiew->visitCollections(visitor))); // 1 collection in view

    {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      wiew->properties(builder, true, false);
      builder.close();

      auto slice = builder.slice();
      CHECK((slice.isObject()));
      CHECK((13U == slice.length()));
      CHECK((slice.hasKey("cleanupIntervalStep") && slice.get("cleanupIntervalStep").isNumber<size_t>() && 24 == slice.get("cleanupIntervalStep").getNumber<size_t>()));
      CHECK((slice.hasKey("consolidationIntervalMsec") && slice.get("consolidationIntervalMsec").isNumber<size_t>() && 42 == slice.get("consolidationIntervalMsec").getNumber<size_t>()));
      CHECK((slice.hasKey("links") && slice.get("links").isObject() && 1 == slice.get("links").length()));
    }

    {
      auto update = arangodb::velocypack::Parser::fromJson("{ \"collections\": [ 6, 7, 8 ], \"links\": { \"testCollection\": {} }, \"consolidationIntervalMsec\": 52 }");
      CHECK((true == wiew->properties(update->slice(), false).ok()));
    }

    {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      wiew->properties(builder, true, false);
      builder.close();

      auto slice = builder.slice();
      CHECK((slice.isObject()));
      CHECK((13U == slice.length()));
      CHECK((slice.hasKey("cleanupIntervalStep") && slice.get("cleanupIntervalStep").isNumber<size_t>() && 10 == slice.get("cleanupIntervalStep").getNumber<size_t>()));
      CHECK((slice.hasKey("consolidationIntervalMsec") && slice.get("consolidationIntervalMsec").isNumber<size_t>() && 52 == slice.get("consolidationIntervalMsec").getNumber<size_t>()));
      CHECK((slice.hasKey("links") && slice.get("links").isObject() && 1 == slice.get("links").length()));
    }

    // not for persistence
    {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      wiew->properties(builder, true, false);
      builder.close();

      auto slice = builder.slice();
      CHECK((slice.isObject()));
      CHECK((13U == slice.length()));
      CHECK((slice.hasKey("cleanupIntervalStep") && slice.get("cleanupIntervalStep").isNumber<size_t>() && 10 == slice.get("cleanupIntervalStep").getNumber<size_t>()));
      CHECK((slice.hasKey("consolidationIntervalMsec") && slice.get("consolidationIntervalMsec").isNumber<size_t>() && 52 == slice.get("consolidationIntervalMsec").getNumber<size_t>()));
      CHECK((slice.hasKey("links") && slice.get("links").isObject() && 1 == slice.get("links").length()));
    }

    // for persistence
    {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      wiew->properties(builder, true, true);
      builder.close();

      auto slice = builder.slice();
      CHECK((slice.isObject()));
      CHECK((17U == slice.length()));
      CHECK((slice.hasKey("collections") && slice.get("collections").isArray() && 2 == slice.get("collections").length())); // list of links is not modified after link drop
      CHECK((slice.hasKey("cleanupIntervalStep") && slice.get("cleanupIntervalStep").isNumber<size_t>() && 10 == slice.get("cleanupIntervalStep").getNumber<size_t>()));
      CHECK((slice.hasKey("consolidationIntervalMsec") && slice.get("consolidationIntervalMsec").isNumber<size_t>() && 52 == slice.get("consolidationIntervalMsec").getNumber<size_t>()));
      CHECK((false == slice.hasKey("links")));
    }
  }
}

SECTION("test_visitCollections") {
  auto* ci = arangodb::ClusterInfo::instance();
  REQUIRE(nullptr != ci);

  // visit empty
  {
    auto json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    arangodb::LogicalView::ptr wiew;
    REQUIRE((arangodb::iresearch::IResearchView::factory().instantiate(wiew, vocbase, json->slice(), 42).ok()));
    CHECK((false == !wiew));
    auto* impl = dynamic_cast<arangodb::iresearch::IResearchView*>(wiew.get());
    CHECK((nullptr != impl));

    static auto visitor = [](TRI_voc_cid_t)->bool { return false; };
    CHECK((true == wiew->visitCollections(visitor))); // no collections in view
  }

  // visit non-empty
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
    auto linkJson = arangodb::velocypack::Parser::fromJson("{ \"view\": \"testView\", \"includeAllFields\": true }");
    auto const wiewId = std::to_string(ci->uniqid() + 1); // +1 because LogicalView creation will generate a new ID
    auto json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    REQUIRE((false == !logicalCollection));
    arangodb::LogicalView::ptr wiew;
    REQUIRE((arangodb::iresearch::IResearchView::factory().instantiate(wiew, vocbase, json->slice(), 42).ok()));
    CHECK((false == !wiew));
    auto* impl = dynamic_cast<arangodb::iresearch::IResearchView*>(wiew.get());
    CHECK((nullptr != impl));

    // ensure we have shard view in vocbase
    struct Link: public arangodb::iresearch::IResearchLink {
      Link(TRI_idx_iid_t id, arangodb::LogicalCollection& col): IResearchLink(id, col) {}
    } link(42, *logicalCollection);
    auto asyncLinkPtr = std::make_shared<arangodb::iresearch::IResearchLink::AsyncLinkPtr::element_type>(&link);
    CHECK((true == impl->link(asyncLinkPtr).ok()));

    std::set<TRI_voc_cid_t> cids = { logicalCollection->id() };
    static auto visitor = [&cids](TRI_voc_cid_t cid)->bool { return 1 == cids.erase(cid); };
    CHECK((true == wiew->visitCollections(visitor))); // all collections expected
    CHECK((true == cids.empty()));
    CHECK((true == impl->unlink(logicalCollection->id()).ok()));
    CHECK((true == wiew->visitCollections(visitor))); // no collections in view
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate tests
////////////////////////////////////////////////////////////////////////////////

}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
