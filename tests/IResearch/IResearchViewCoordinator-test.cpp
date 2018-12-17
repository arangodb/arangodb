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

#include "catch.hpp"
#include "common.h"
#include "AgencyMock.h"
#include "StorageEngineMock.h"

#include "utils/utf8_path.hpp"
#include "utils/log.hpp"

#include "ApplicationFeatures/BasicPhase.h"
#include "ApplicationFeatures/CommunicationPhase.h"
#include "ApplicationFeatures/ClusterPhase.h"
#include "ApplicationFeatures/DatabasePhase.h"
#include "ApplicationFeatures/GreetingsPhase.h"
#include "ApplicationFeatures/V8Phase.h"
#include "Aql/AqlFunctionFeature.h"
#include "Aql/AstNode.h"
#include "Aql/BasicBlocks.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Function.h"
#include "Aql/OptimizerRulesFeature.h"
#include "Aql/QueryRegistry.h"
#include "Aql/SortCondition.h"
#include "Agency/Store.h"
#include "Basics/ArangoGlobalContext.h"
#include "Basics/files.h"
#include "Utils/ExecContext.h"
#include "VocBase/Methods/Indexes.h"

#if USE_ENTERPRISE
  #include "Enterprise/Ldap/LdapFeature.h"
#endif

#include "Agency/AgencyFeature.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterComm.h"
#include "IResearch/ApplicationServerHelper.h"
#include "IResearch/IResearchCommon.h"
#include "IResearch/IResearchFeature.h"
#include "IResearch/IResearchLink.h"
#include "IResearch/IResearchLinkCoordinator.h"
#include "IResearch/IResearchLinkHelper.h"
#include "IResearch/IResearchLinkMeta.h"
#include "IResearch/IResearchViewCoordinator.h"
#include "IResearch/IResearchViewNode.h"
#include "Logger/Logger.h"
#include "Logger/LogTopic.h"
#include "Random/RandomFeature.h"
#include "RestServer/AqlFeature.h"
#include "RestServer/TraverserEngineRegistryFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/FlushFeature.h"
#include "RestServer/DatabasePathFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "RestServer/SystemDatabaseFeature.h"
#include "RestServer/ViewTypesFeature.h"
#include "Sharding/ShardingFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "velocypack/Iterator.h"
#include "velocypack/Parser.h"
#include "V8Server/V8DealerFeature.h"
#include "VocBase/KeyGenerator.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ManagedDocumentResult.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

struct IResearchViewCoordinatorSetup {

  struct ClusterCommControl : arangodb::ClusterComm {
    static void reset() {
      arangodb::ClusterComm::_theInstanceInit.store(0);
    }
  };

  arangodb::consensus::Store _agencyStore{nullptr, "arango"};
  GeneralClientConnectionAgencyMock* agency;
  StorageEngineMock engine;
  arangodb::application_features::ApplicationServer server;
  std::unique_ptr<TRI_vocbase_t> system;
  std::map<std::string, std::pair<arangodb::application_features::ApplicationFeature*, bool>> features;
  std::vector<arangodb::application_features::ApplicationFeature*> orderedFeatures;
  std::string testFilesystemPath;

  IResearchViewCoordinatorSetup(): engine(server), server(nullptr, nullptr) {
    auto* agencyCommManager = new AgencyCommManagerMock("arango");
    agency = agencyCommManager->addConnection<GeneralClientConnectionAgencyMock>(_agencyStore);
    agency = agencyCommManager->addConnection<GeneralClientConnectionAgencyMock>(_agencyStore); // need 2 connections or Agency callbacks will fail
    arangodb::AgencyCommManager::MANAGER.reset(agencyCommManager);

    arangodb::EngineSelectorFeature::ENGINE = &engine;
    // register factories & normalizers
    auto& indexFactory = const_cast<arangodb::IndexFactory&>(engine.indexFactory());
    indexFactory.emplace(
      arangodb::iresearch::DATA_SOURCE_TYPE.name(),
      arangodb::iresearch::IResearchLinkCoordinator::factory()
    );

    arangodb::tests::init();

    // suppress INFO {authentication} Authentication is turned on (system only), authentication for unix sockets is turned on
    arangodb::LogTopic::setLogLevel(arangodb::Logger::AUTHENTICATION.name(), arangodb::LogLevel::WARN);

    // pretend we're on coordinator
    serverRoleBeforeSetup = arangodb::ServerState::instance()->getRole();
    arangodb::ServerState::instance()->setRole(arangodb::ServerState::ROLE_COORDINATOR);

    auto buildFeatureEntry = [&] (arangodb::application_features::ApplicationFeature* ftr, bool start) -> void {
      std::string name = ftr->name();
      features.emplace(name, std::make_pair(ftr, start));
    };
    arangodb::application_features::ApplicationFeature* tmpFeature;

    buildFeatureEntry(new arangodb::application_features::BasicFeaturePhase(server, false), false);
    buildFeatureEntry(new arangodb::application_features::CommunicationFeaturePhase(server), false);
    buildFeatureEntry(new arangodb::application_features::ClusterFeaturePhase(server), false);
    buildFeatureEntry(new arangodb::application_features::DatabaseFeaturePhase(server), false);
    buildFeatureEntry(new arangodb::application_features::GreetingsFeaturePhase(server, false), false);
    buildFeatureEntry(new arangodb::application_features::V8FeaturePhase(server), false);

    // setup required application features
    buildFeatureEntry(new arangodb::V8DealerFeature(server), false);
    buildFeatureEntry(new arangodb::ViewTypesFeature(server), true);
    buildFeatureEntry(tmpFeature = new arangodb::QueryRegistryFeature(server), false);
    arangodb::application_features::ApplicationServer::server->addFeature(tmpFeature); // need QueryRegistryFeature feature to be added now in order to create the system database
    system = irs::memory::make_unique<TRI_vocbase_t>(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 0, TRI_VOC_SYSTEM_DATABASE);
    buildFeatureEntry(new arangodb::SystemDatabaseFeature(server, system.get()), false); // required for IResearchAnalyzerFeature
    buildFeatureEntry(new arangodb::RandomFeature(server), false); // required by AuthenticationFeature
    buildFeatureEntry(new arangodb::AuthenticationFeature(server), false);
    buildFeatureEntry(arangodb::DatabaseFeature::DATABASE = new arangodb::DatabaseFeature(server), false);
    buildFeatureEntry(new arangodb::DatabasePathFeature(server), false);
    buildFeatureEntry(new arangodb::TraverserEngineRegistryFeature(server), false); // must be before AqlFeature
    buildFeatureEntry(new arangodb::AqlFeature(server), true);
    buildFeatureEntry(new arangodb::aql::AqlFunctionFeature(server), true); // required for IResearchAnalyzerFeature
    buildFeatureEntry(new arangodb::iresearch::IResearchFeature(server), true);
    buildFeatureEntry(new arangodb::aql::OptimizerRulesFeature(server), true);
    buildFeatureEntry(new arangodb::FlushFeature(server), false); // do not start the thread
    buildFeatureEntry(new arangodb::ClusterFeature(server), false);
    buildFeatureEntry(new arangodb::ShardingFeature(server), false);
    buildFeatureEntry(new arangodb::iresearch::IResearchAnalyzerFeature(server), true);

    #if USE_ENTERPRISE
      buildFeatureEntry(new arangodb::LdapFeature(server), false); // required for AuthenticationFeature with USE_ENTERPRISE
    #endif

    for (auto& f : features) {
      arangodb::application_features::ApplicationServer::server->addFeature(f.second.first);
    }
    arangodb::application_features::ApplicationServer::server->setupDependencies(false);
    orderedFeatures = arangodb::application_features::ApplicationServer::server->getOrderedFeatures();

    // suppress log messages since tests check error conditions
    arangodb::LogTopic::setLogLevel(arangodb::Logger::AGENCY.name(), arangodb::LogLevel::FATAL);
    arangodb::LogTopic::setLogLevel(arangodb::Logger::ENGINES.name(), arangodb::LogLevel::FATAL); // suppress ERROR {engines} failed to instantiate index, error: ...
    arangodb::LogTopic::setLogLevel(arangodb::Logger::FIXME.name(), arangodb::LogLevel::ERR); // suppress ERROR recovery failure due to error from callback
    arangodb::LogTopic::setLogLevel(arangodb::Logger::CLUSTER.name(), arangodb::LogLevel::FATAL);
    arangodb::LogTopic::setLogLevel(arangodb::iresearch::TOPIC.name(), arangodb::LogLevel::FATAL);
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

    auto* authFeature = arangodb::application_features::ApplicationServer::getFeature<arangodb::AuthenticationFeature>("Authentication");
    authFeature->enable(); // required for authentication tests

    TransactionStateMock::abortTransactionCount = 0;
    TransactionStateMock::beginTransactionCount = 0;
    TransactionStateMock::commitTransactionCount = 0;

    auto* dbPathFeature = arangodb::application_features::ApplicationServer::getFeature<arangodb::DatabasePathFeature>("DatabasePath");
    arangodb::tests::setDatabasePath(*dbPathFeature); // ensure test data is stored in a unique directory
    testFilesystemPath = dbPathFeature->directory();

    long systemError;
    std::string systemErrorStr;
    TRI_CreateDirectory(testFilesystemPath.c_str(), systemError, systemErrorStr);

    agencyCommManager->start(); // initialize agency
  }

  ~IResearchViewCoordinatorSetup() {
    system.reset(); // destroy before reseting the 'ENGINE'
    TRI_RemoveDirectory(testFilesystemPath.c_str());
    arangodb::LogTopic::setLogLevel(arangodb::iresearch::TOPIC.name(), arangodb::LogLevel::DEFAULT);
    arangodb::LogTopic::setLogLevel(arangodb::Logger::CLUSTER.name(), arangodb::LogLevel::DEFAULT);
    arangodb::LogTopic::setLogLevel(arangodb::Logger::FIXME.name(), arangodb::LogLevel::DEFAULT);
    arangodb::LogTopic::setLogLevel(arangodb::Logger::ENGINES.name(), arangodb::LogLevel::DEFAULT);
    arangodb::LogTopic::setLogLevel(arangodb::Logger::AGENCY.name(), arangodb::LogLevel::DEFAULT);
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
    arangodb::ServerState::instance()->setRole(serverRoleBeforeSetup);
    arangodb::LogTopic::setLogLevel(arangodb::Logger::AUTHENTICATION.name(), arangodb::LogLevel::DEFAULT);
    arangodb::EngineSelectorFeature::ENGINE = nullptr;
  }

  arangodb::ServerState::RoleEnum serverRoleBeforeSetup;
};

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

TEST_CASE("IResearchViewCoordinatorTest", "[iresearch][iresearch-view-coordinator]") {
  IResearchViewCoordinatorSetup s;
  UNUSED(s);

SECTION("test_type") {
  CHECK((arangodb::LogicalDataSource::Type::emplace(arangodb::velocypack::StringRef("arangosearch")) == arangodb::iresearch::DATA_SOURCE_TYPE));
}

SECTION("test_rename") {
  auto json = arangodb::velocypack::Parser::fromJson(
    "{ \"name\": \"testView\", \"type\": \"arangosearch\", \"id\": \"1\", \"collections\": [1,2,3] }");

  Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_COORDINATOR, 1, "testVocbase");
  arangodb::LogicalView::ptr view;
  REQUIRE((arangodb::LogicalView::instantiate(view, vocbase, json->slice(), 0).ok()));
  CHECK(nullptr != view);
  CHECK(nullptr != std::dynamic_pointer_cast<arangodb::iresearch::IResearchViewCoordinator>(view));
  CHECK(0 == view->planVersion());
  CHECK("testView" == view->name());
  CHECK(false == view->deleted());
  CHECK(1 == view->id());
  CHECK(arangodb::iresearch::DATA_SOURCE_TYPE == view->type());
  CHECK(arangodb::LogicalView::category() == view->category());
  CHECK(&vocbase == &view->vocbase());

  auto const res = view->rename("otherName");
  CHECK(res.fail());
  CHECK(TRI_ERROR_CLUSTER_UNSUPPORTED == res.errorNumber());
}

SECTION("visit_collections") {
  auto json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\", \"id\": \"1\" }");
  Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_COORDINATOR, 1, "testVocbase");
  arangodb::LogicalView::ptr logicalView;
  REQUIRE((arangodb::LogicalView::instantiate(logicalView, vocbase, json->slice(), 0).ok()));
  auto* view = dynamic_cast<arangodb::iresearch::IResearchViewCoordinator*>(logicalView.get());

  CHECK(nullptr != view);
  CHECK(0 == view->planVersion());
  CHECK("testView" == view->name());
  CHECK(false == view->deleted());
  CHECK(1 == view->id());
  CHECK(arangodb::iresearch::DATA_SOURCE_TYPE == view->type());
  CHECK(arangodb::LogicalView::category() == view->category());
  CHECK(&vocbase == &view->vocbase());

  CHECK((true == view->emplace(1, "1", arangodb::velocypack::Slice::emptyObjectSlice())));
  CHECK((true == view->emplace(2, "2", arangodb::velocypack::Slice::emptyObjectSlice())));
  CHECK((true == view->emplace(3, "3", arangodb::velocypack::Slice::emptyObjectSlice())));

  // visit view
  TRI_voc_cid_t expectedCollections[] = {1,2,3};
  auto* begin = expectedCollections;
  CHECK(true == view->visitCollections([&begin](TRI_voc_cid_t cid) {
    return *begin++ = cid; })
  );
  CHECK(3 == (begin-expectedCollections));
}

SECTION("test_defaults") {
  auto* database = arangodb::DatabaseFeature::DATABASE;
  REQUIRE(nullptr != database);
  auto* ci = arangodb::ClusterInfo::instance();
  REQUIRE((nullptr != ci));
  TRI_vocbase_t* vocbase; // will be owned by DatabaseFeature
  std::string error;

  // create database
  {
    // simulate heartbeat thread
    REQUIRE((TRI_ERROR_NO_ERROR == database->createDatabase(1, "testDatabase", vocbase)));

    REQUIRE((nullptr != vocbase));
    CHECK(("testDatabase" == vocbase->name()));
    CHECK((TRI_vocbase_type_e::TRI_VOCBASE_TYPE_COORDINATOR == vocbase->type()));
    CHECK((1 == vocbase->id()));

    CHECK((TRI_ERROR_NO_ERROR == ci->createDatabaseCoordinator(
      vocbase->name(), VPackSlice::emptyObjectSlice(), error, 0.0
    )));
    CHECK(("no error" == error));
  }

  // view definition with LogicalView (for persistence)
  {
    auto json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\", \"id\": \"1\" }");
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_COORDINATOR, 1, "testVocbase");
    arangodb::LogicalView::ptr view;
    REQUIRE((arangodb::LogicalView::instantiate(view, vocbase, json->slice(), 0).ok()));

    CHECK((nullptr != view));
    CHECK((nullptr != std::dynamic_pointer_cast<arangodb::iresearch::IResearchViewCoordinator>(view)));
    CHECK((0 == view->planVersion()));
    CHECK(("testView" == view->name()));
    CHECK((false == view->deleted()));
    CHECK((1 == view->id()));
    CHECK((arangodb::iresearch::DATA_SOURCE_TYPE == view->type()));
    CHECK((arangodb::LogicalView::category() == view->category()));
    CHECK((&vocbase == &view->vocbase()));

    // visit default view
    CHECK((true == view->visitCollections([](TRI_voc_cid_t) { return false; })));

    // +system, +properties
    {
      arangodb::iresearch::IResearchViewMeta expectedMeta;
      arangodb::velocypack::Builder builder;
      builder.openObject();
      view->properties(builder, true, true);
      builder.close();
      auto slice = builder.slice();
      arangodb::iresearch::IResearchViewMeta meta;
      std::string error;

      CHECK((14U == slice.length()));
      CHECK((slice.hasKey("globallyUniqueId") && slice.get("globallyUniqueId").isString() && false == slice.get("globallyUniqueId").copyString().empty()));
      CHECK((slice.get("id").copyString() == "1"));
      CHECK((slice.hasKey("isSystem") && slice.get("isSystem").isBoolean() && false == slice.get("isSystem").getBoolean()));
      CHECK((slice.get("name").copyString() == "testView"));
      CHECK((slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name()));
      CHECK((slice.hasKey("planId")));
      CHECK((false == slice.get("deleted").getBool()));
      CHECK((!slice.hasKey("links"))); // for persistence so no links
      CHECK((meta.init(slice, error) && expectedMeta == meta));
    }

    // -system, +properties
    {
      arangodb::iresearch::IResearchViewMeta expectedMeta;
      arangodb::velocypack::Builder builder;
      builder.openObject();
      view->properties(builder, true, false);
      builder.close();
      auto slice = builder.slice();
      arangodb::iresearch::IResearchViewMeta meta;
      std::string error;

      CHECK((11U == slice.length()));
      CHECK((slice.hasKey("globallyUniqueId") && slice.get("globallyUniqueId").isString() && false == slice.get("globallyUniqueId").copyString().empty()));
      CHECK((slice.get("id").copyString() == "1"));
      CHECK((slice.get("name").copyString() == "testView"));
      CHECK((slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name()));
      CHECK((!slice.hasKey("planId")));
      CHECK((!slice.hasKey("deleted")));
      CHECK((slice.hasKey("links") && slice.get("links").isObject() && 0 == slice.get("links").length()));
      CHECK((meta.init(slice, error) && expectedMeta == meta));
    }

    // -system, -properties
    {
      arangodb::velocypack::Builder builder;
      builder.openObject();
      view->properties(builder, false, false);
      builder.close();
      auto slice = builder.slice();
      arangodb::iresearch::IResearchViewMeta meta;
      std::string error;

      CHECK((4U == slice.length()));
      CHECK((slice.hasKey("globallyUniqueId") && slice.get("globallyUniqueId").isString() && false == slice.get("globallyUniqueId").copyString().empty()));
      CHECK((slice.get("id").copyString() == "1"));
      CHECK((slice.get("name").copyString() == "testView"));
      CHECK((slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name()));
      CHECK((!slice.hasKey("planId")));
      CHECK((!slice.hasKey("deleted")));
      CHECK((!slice.hasKey("properties")));
    }

    // +system, -properties
    {
      arangodb::velocypack::Builder builder;
      builder.openObject();
      view->properties(builder, false, true);
      builder.close();
      auto slice = builder.slice();

      CHECK((7 == slice.length()));
      CHECK((slice.hasKey("globallyUniqueId") && slice.get("globallyUniqueId").isString() && false == slice.get("globallyUniqueId").copyString().empty()));
      CHECK((slice.get("id").copyString() == "1"));
      CHECK((slice.hasKey("isSystem") && slice.get("isSystem").isBoolean() && false == slice.get("isSystem").getBoolean()));
      CHECK((slice.get("name").copyString() == "testView"));
      CHECK((slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name()));
      CHECK((false == slice.get("deleted").getBool()));
      CHECK((slice.hasKey("planId")));
      CHECK((!slice.hasKey("properties")));
    }
  }

  // new view definition with links to missing collections
  {
    auto viewCreateJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"id\": \"42\", \"type\": \"arangosearch\", \"links\": { \"testCollection\": {} } }");
    auto viewId = "testView";

    arangodb::LogicalView::ptr logicalView;
    auto res = arangodb::iresearch::IResearchViewCoordinator::factory().create(logicalView, *vocbase, viewCreateJson->slice());
    CHECK((TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND == res.errorNumber()));
    logicalView = ci->getView(vocbase->name(), viewId);
    REQUIRE((true == !logicalView));
  }

  // new view definition with links with invalid definition
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"id\": \"1\", \"planId\": \"1\", \"name\": \"testCollection\", \"replicationFactor\": 1, \"type\": 1, \"shards\":{} }");
    auto viewCreateJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"id\": \"42\", \"type\": \"arangosearch\", \"links\": { \"testCollection\": 42 } }");
    auto collectionId = std::to_string(1);
    auto viewId = "testView";

    error.clear();
    CHECK((TRI_ERROR_NO_ERROR == ci->createCollectionCoordinator(vocbase->name(), collectionId, 0, 1, false, collectionJson->slice(), error, 0.0)));
    CHECK((error.empty()));
    auto logicalCollection = ci->getCollection(vocbase->name(), collectionId);
    REQUIRE((false == !logicalCollection));
    auto dropLogicalCollection = std::shared_ptr<arangodb::ClusterInfo>(ci, [vocbase, &collectionId](arangodb::ClusterInfo* ci)->void { std::string error; ci->dropCollectionCoordinator(vocbase->name(), collectionId, error, 0); });
    arangodb::LogicalView::ptr logicalView;
    auto res = arangodb::iresearch::IResearchViewCoordinator::factory().create(logicalView, *vocbase, viewCreateJson->slice());
    CHECK((TRI_ERROR_BAD_PARAMETER == res.errorNumber()));

    logicalCollection = ci->getCollection(vocbase->name(), collectionId);
    REQUIRE((false == !logicalCollection));
    logicalView = ci->getView(vocbase->name(), viewId);
    REQUIRE((true == !logicalView));
    CHECK((true == logicalCollection->getIndexes().empty()));
  }

  // new view definition with links (collection not authorized)
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"id\": \"1\", \"planId\": \"1\", \"name\": \"testCollection\", \"replicationFactor\": 1, \"type\": 1, \"shards\":{} }");
    auto viewCreateJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"id\": \"42\", \"type\": \"arangosearch\", \"links\": { \"testCollection\": {} } }");
    auto collectionId = std::to_string(1);
    auto viewId = "testView";

    error.clear();
    CHECK((TRI_ERROR_NO_ERROR == ci->createCollectionCoordinator(vocbase->name(), collectionId, 0, 1, false, collectionJson->slice(), error, 0.0)));
    CHECK((error.empty()));
    auto logicalCollection = ci->getCollection(vocbase->name(), collectionId);
    REQUIRE((false == !logicalCollection));
    auto dropLogicalCollection = std::shared_ptr<arangodb::ClusterInfo>(ci, [vocbase, &collectionId](arangodb::ClusterInfo* ci)->void { std::string error; ci->dropCollectionCoordinator(vocbase->name(), collectionId, error, 0); });

    struct ExecContext: public arangodb::ExecContext {
      ExecContext(): arangodb::ExecContext(arangodb::ExecContext::Type::Default, "", "",
                                           arangodb::auth::Level::NONE, arangodb::auth::Level::NONE) {}
    } execContext;
    arangodb::ExecContextScope execContextScope(&execContext);
    auto* authFeature = arangodb::AuthenticationFeature::instance();
    auto* userManager = authFeature->userManager();
    arangodb::aql::QueryRegistry queryRegistry(0); // required for UserManager::loadFromDB()
    userManager->setQueryRegistry(&queryRegistry);
    auto resetUserManager = std::shared_ptr<arangodb::auth::UserManager>(userManager, [](arangodb::auth::UserManager* ptr)->void { ptr->removeAllUsers(); });

    arangodb::LogicalView::ptr logicalView;
    auto res = arangodb::iresearch::IResearchViewCoordinator::factory().create(logicalView, *vocbase, viewCreateJson->slice());
    CHECK((TRI_ERROR_FORBIDDEN == res.errorNumber()));

    logicalCollection = ci->getCollection(vocbase->name(), collectionId);
    REQUIRE((false == !logicalCollection));
    logicalView = ci->getView(vocbase->name(), viewId);
    REQUIRE((true == !logicalView));
    CHECK((true == logicalCollection->getIndexes().empty()));
  }

  // new view definition with links
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"id\": \"1\", \"planId\": \"1\", \"name\": \"testCollection\", \"replicationFactor\": 1, \"type\": 1, \"shards\":{} }");
    auto viewCreateJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"id\": \"42\", \"type\": \"arangosearch\", \"links\": { \"testCollection\": {} } }");
    auto collectionId = std::to_string(1);
    auto viewId = "testView";

    error.clear();
    CHECK((TRI_ERROR_NO_ERROR == ci->createCollectionCoordinator(vocbase->name(), collectionId, 0, 1, false, collectionJson->slice(), error, 0.0)));
    CHECK((error.empty()));
    auto logicalCollection = ci->getCollection(vocbase->name(), collectionId);
    REQUIRE((false == !logicalCollection));
    auto dropLogicalCollection = std::shared_ptr<arangodb::ClusterInfo>(ci, [vocbase, &collectionId](arangodb::ClusterInfo* ci)->void { std::string error; ci->dropCollectionCoordinator(vocbase->name(), collectionId, error, 0); });

    // simulate heartbeat thread (create index in current)
    {
      auto const path = "/Current/Collections/" + vocbase->name() + "/" + std::to_string(logicalCollection->id());
      auto const value = arangodb::velocypack::Parser::fromJson("{ \"shard-id-does-not-matter\": { \"indexes\" : [ { \"id\": \"1\" } ] } }");
      CHECK((arangodb::AgencyComm().setValue(path, value->slice(), 0.0).successful()));
    }

    arangodb::LogicalView::ptr logicalView;
    CHECK((arangodb::iresearch::IResearchViewCoordinator::factory().create(logicalView, *vocbase, viewCreateJson->slice()).ok()));

    logicalCollection = ci->getCollection(vocbase->name(), collectionId);
    REQUIRE((false == !logicalCollection));
    logicalView = ci->getView(vocbase->name(), viewId);
    REQUIRE((false == !logicalView));
    CHECK((false == logicalCollection->getIndexes().empty()));
    CHECK((false == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));
  }
}

SECTION("test_create_drop_view") {
  auto* database = arangodb::DatabaseFeature::DATABASE;
  REQUIRE(nullptr != database);

  auto* ci = arangodb::ClusterInfo::instance();
  REQUIRE(nullptr != ci);

  std::string error;
  TRI_vocbase_t* vocbase; // will be owned by DatabaseFeature

  // create database
  {
    // simulate heartbeat thread
    REQUIRE(TRI_ERROR_NO_ERROR == database->createDatabase(1, "testDatabase", vocbase));

    REQUIRE(nullptr != vocbase);
    CHECK("testDatabase" == vocbase->name());
    CHECK(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_COORDINATOR == vocbase->type());
    CHECK(1 == vocbase->id());

    CHECK(TRI_ERROR_NO_ERROR == ci->createDatabaseCoordinator(
      vocbase->name(), VPackSlice::emptyObjectSlice(), error, 0.0
    ));
    CHECK("no error" == error);
  }

  // no name specified
  {
    auto json = arangodb::velocypack::Parser::fromJson("{ \"type\": \"arangosearch\" }");
    auto viewId = std::to_string(ci->uniqid());
    CHECK(TRI_ERROR_BAD_PARAMETER == ci->createViewCoordinator(
      vocbase->name(), viewId, json->slice(), error
    ));
  }

  // empty name
  {
    auto json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"\", \"type\": \"arangosearch\" }");
    auto viewId = std::to_string(ci->uniqid());
    CHECK(TRI_ERROR_BAD_PARAMETER == ci->createViewCoordinator(
      vocbase->name(), viewId, json->slice(), error
    ));
  }

  // wrong name
  {
    auto json = arangodb::velocypack::Parser::fromJson("{ \"name\": 5, \"type\": \"arangosearch\" }");
    auto viewId = std::to_string(ci->uniqid());
    CHECK(TRI_ERROR_BAD_PARAMETER == ci->createViewCoordinator(
      vocbase->name(), viewId, json->slice(), error
    ));
  }

  // no type specified
  {
    auto json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\" }");
    auto viewId = std::to_string(ci->uniqid());
    CHECK(TRI_ERROR_BAD_PARAMETER == ci->createViewCoordinator(
      vocbase->name(), viewId, json->slice(), error
    ));
  }

  // create and drop view (no id specified)
  {
    auto json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
    auto viewId = std::to_string(ci->uniqid() + 1); // +1 because LogicalView creation will generate a new ID
    error.clear(); // clear error message

    CHECK(TRI_ERROR_NO_ERROR == ci->createViewCoordinator(
      vocbase->name(), viewId, json->slice(), error
    ));
    CHECK(error.empty());

    // get current plan version
    auto planVersion = arangodb::tests::getCurrentPlanVersion();

    auto view = ci->getView(vocbase->name(), viewId);
    CHECK(nullptr != view);
    CHECK(nullptr != std::dynamic_pointer_cast<arangodb::iresearch::IResearchViewCoordinator>(view));
    CHECK(planVersion == view->planVersion());
    CHECK("testView" == view->name());
    CHECK(false == view->deleted());
    CHECK(viewId == std::to_string(view->id()));
    CHECK(arangodb::iresearch::DATA_SOURCE_TYPE == view->type());
    CHECK(arangodb::LogicalView::category() == view->category());
    CHECK(vocbase == &view->vocbase());

    // create duplicate view
    CHECK(TRI_ERROR_ARANGO_DUPLICATE_NAME == ci->createViewCoordinator(
      vocbase->name(), viewId, json->slice(), error
    ));
    CHECK(error.empty());
    CHECK(planVersion == arangodb::tests::getCurrentPlanVersion());
    CHECK(view == ci->getView(vocbase->name(), view->name()));

    // drop view
    CHECK(view->drop().ok());
    CHECK(planVersion < arangodb::tests::getCurrentPlanVersion());

    // check there is no more view
    CHECK(nullptr == ci->getView(vocbase->name(), view->name()));

    // drop already dropped view
    CHECK((view->drop().ok()));
  }

  // create and drop view
  {
    auto json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"id\": \"42\", \"type\": \"arangosearch\" }");
    auto viewId = std::to_string(42);
    error.clear(); // clear error message

    CHECK(TRI_ERROR_NO_ERROR == ci->createViewCoordinator(
      vocbase->name(), viewId, json->slice(), error
    ));
    CHECK(error.empty());
    CHECK("42" == viewId);

    // get current plan version
    auto planVersion = arangodb::tests::getCurrentPlanVersion();

    auto view = ci->getView(vocbase->name(), viewId);
    CHECK(nullptr != view);
    CHECK(nullptr != std::dynamic_pointer_cast<arangodb::iresearch::IResearchViewCoordinator>(view));
    CHECK(planVersion == view->planVersion());
    CHECK("testView" == view->name());
    CHECK(false == view->deleted());
    CHECK(42 == view->id());
    CHECK(arangodb::iresearch::DATA_SOURCE_TYPE == view->type());
    CHECK(arangodb::LogicalView::category() == view->category());
    CHECK(vocbase == &view->vocbase());

    // create duplicate view
    CHECK(TRI_ERROR_ARANGO_DUPLICATE_NAME == ci->createViewCoordinator(
      vocbase->name(), viewId, json->slice(), error
    ));
    CHECK(error.empty());
    CHECK(planVersion == arangodb::tests::getCurrentPlanVersion());
    CHECK(view == ci->getView(vocbase->name(), view->name()));

    // drop view
    CHECK(view->drop().ok());
    CHECK(planVersion < arangodb::tests::getCurrentPlanVersion());

    // check there is no more view
    CHECK(nullptr == ci->getView(vocbase->name(), view->name()));

    // drop already dropped view
    CHECK((view->drop().ok()));
  }
}

SECTION("test_drop_with_link") {
  auto* database = arangodb::DatabaseFeature::DATABASE;
  REQUIRE(nullptr != database);
  auto* ci = arangodb::ClusterInfo::instance();
  REQUIRE((nullptr != ci));
  TRI_vocbase_t* vocbase; // will be owned by DatabaseFeature
  std::string error;

  // create database
  {
    // simulate heartbeat thread
    REQUIRE((TRI_ERROR_NO_ERROR == database->createDatabase(1, "testDatabase", vocbase)));

    REQUIRE((nullptr != vocbase));
    CHECK(("testDatabase" == vocbase->name()));
    CHECK((TRI_vocbase_type_e::TRI_VOCBASE_TYPE_COORDINATOR == vocbase->type()));
    CHECK((1 == vocbase->id()));

    CHECK((TRI_ERROR_NO_ERROR == ci->createDatabaseCoordinator(
      vocbase->name(), VPackSlice::emptyObjectSlice(), error, 0.0
    )));
    CHECK(("no error" == error));
  }

  auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"id\": \"1\", \"planId\": \"1\",  \"name\": \"testCollection\", \"replicationFactor\": 1, \"type\": 1, \"shards\":{} }");
  auto viewCreateJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"id\": \"42\", \"type\": \"arangosearch\" }");
  auto viewUpdateJson = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection\": {} } }");
  auto collectionId = std::to_string(1);
  auto viewId = std::to_string(42);

  error.clear();
  CHECK((TRI_ERROR_NO_ERROR == ci->createCollectionCoordinator(vocbase->name(), collectionId, 0, 1, false, collectionJson->slice(), error, 0.0)));
  CHECK((error.empty()));
  auto logicalCollection = ci->getCollection(vocbase->name(), collectionId);
  REQUIRE((false == !logicalCollection));
  error.clear();
  CHECK((TRI_ERROR_NO_ERROR == ci->createViewCoordinator(vocbase->name(), viewId, viewCreateJson->slice(), error)));
  CHECK((error.empty()));
  auto logicalView = ci->getView(vocbase->name(), viewId);
  REQUIRE((false == !logicalView));

  CHECK((true == logicalCollection->getIndexes().empty()));
  CHECK((false == !ci->getView(vocbase->name(), viewId)));

  // initial link creation
  {
    // simulate heartbeat thread (create index in current)
    {
      auto const path = "/Current/Collections/" + vocbase->name() + "/" + std::to_string(logicalCollection->id());
      auto const value = arangodb::velocypack::Parser::fromJson("{ \"shard-id-does-not-matter\": { \"indexes\" : [ { \"id\": \"1\" } ] } }");
      CHECK(arangodb::AgencyComm().setValue(path, value->slice(), 0.0).successful());
    }

    CHECK((logicalView->properties(viewUpdateJson->slice(), true).ok()));
    logicalCollection = ci->getCollection(vocbase->name(), collectionId);
    REQUIRE((false == !logicalCollection));
    logicalView = ci->getView(vocbase->name(), viewId);
    REQUIRE((false == !logicalView));
    CHECK((false == logicalCollection->getIndexes().empty()));
    CHECK((false == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));

    // simulate heartbeat thread (remove index from current)
    {
      auto const path = "/Current/Collections/" + vocbase->name() + "/" + std::to_string(logicalCollection->id()) + "/shard-id-does-not-matter/indexes";
      CHECK(arangodb::AgencyComm().removeValues(path, false).successful());
    }
  }

  {
    struct ExecContext: public arangodb::ExecContext {
      ExecContext(): arangodb::ExecContext(arangodb::ExecContext::Type::Default, "", "",
                                           arangodb::auth::Level::NONE, arangodb::auth::Level::NONE) {}
    } execContext;
    arangodb::ExecContextScope execContextScope(&execContext);
    auto* authFeature = arangodb::AuthenticationFeature::instance();
    auto* userManager = authFeature->userManager();
    arangodb::aql::QueryRegistry queryRegistry(0); // required for UserManager::loadFromDB()
    userManager->setQueryRegistry(&queryRegistry);
    auto resetUserManager = std::shared_ptr<arangodb::auth::UserManager>(userManager, [](arangodb::auth::UserManager* ptr)->void { ptr->removeAllUsers(); });

    // not authorised (NONE collection) as per https://github.com/arangodb/backlog/issues/459
    {
      arangodb::auth::UserMap userMap;
      auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
      user.grantCollection(vocbase->name(), "testCollection", arangodb::auth::Level::NONE); // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

      CHECK((TRI_ERROR_FORBIDDEN == logicalView->drop().errorNumber()));
      logicalCollection = ci->getCollection(vocbase->name(), collectionId);
      REQUIRE((false == !logicalCollection));
      CHECK((false == logicalCollection->getIndexes().empty()));
      CHECK((false == !ci->getView(vocbase->name(), viewId)));
    }

    // authorised (RO collection)
    {
      arangodb::auth::UserMap userMap;
      auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
      user.grantCollection(vocbase->name(), "testCollection", arangodb::auth::Level::RO); // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

      CHECK((true == logicalView->drop().ok()));
      logicalCollection = ci->getCollection(vocbase->name(), collectionId);
      REQUIRE((false == !logicalCollection));
      CHECK((true == logicalCollection->getIndexes().empty()));
      CHECK((true == !ci->getView(vocbase->name(), viewId)));
    }
  }
}

SECTION("test_update_properties") {
  auto* database = arangodb::DatabaseFeature::DATABASE;
  REQUIRE(nullptr != database);

  auto* ci = arangodb::ClusterInfo::instance();
  REQUIRE(nullptr != ci);

  std::string error;
  TRI_vocbase_t* vocbase; // will be owned by DatabaseFeature

  // create database
  {
    // simulate heartbeat thread
    REQUIRE(TRI_ERROR_NO_ERROR == database->createDatabase(1, "testDatabase", vocbase));

    REQUIRE(nullptr != vocbase);
    CHECK("testDatabase" == vocbase->name());
    CHECK(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_COORDINATOR == vocbase->type());
    CHECK(1 == vocbase->id());

    CHECK(TRI_ERROR_NO_ERROR == ci->createDatabaseCoordinator(
      vocbase->name(), VPackSlice::emptyObjectSlice(), error, 0.0
    ));
    CHECK("no error" == error);
  }

  // create view
  {
    auto json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
    auto viewId = std::to_string(ci->uniqid() + 1); // +1 because LogicalView creation will generate a new ID
    error.clear(); // clear error message

    CHECK(TRI_ERROR_NO_ERROR == ci->createViewCoordinator(
      vocbase->name(), viewId, json->slice(), error
    ));
    CHECK(error.empty());

    // get current plan version
    auto planVersion = arangodb::tests::getCurrentPlanVersion();

    auto view = ci->getView(vocbase->name(), viewId);
    CHECK(nullptr != view);
    CHECK(nullptr != std::dynamic_pointer_cast<arangodb::iresearch::IResearchViewCoordinator>(view));
    CHECK(planVersion == view->planVersion());
    CHECK("testView" == view->name());
    CHECK(false == view->deleted());
    CHECK(viewId == std::to_string(view->id()));
    CHECK(arangodb::iresearch::DATA_SOURCE_TYPE == view->type());
    CHECK(arangodb::LogicalView::category() == view->category());
    CHECK(vocbase == &view->vocbase());

    // check default properties
    {
      VPackBuilder builder;
      builder.openObject();
      view->properties(builder, true, false);
      builder.close();

      arangodb::iresearch::IResearchViewMeta meta;
      error.clear(); // clear error
      CHECK(meta.init(builder.slice(), error));
      CHECK(error.empty());
      CHECK(meta == arangodb::iresearch::IResearchViewMeta::DEFAULT());
    }

    decltype(view) fullyUpdatedView;

    // update properties - full update
    {
      auto props = arangodb::velocypack::Parser::fromJson("{ \"cleanupIntervalStep\": 42, \"consolidationIntervalMsec\": 50 }");
      CHECK(view->properties(props->slice(), false).ok());
      CHECK(planVersion < arangodb::tests::getCurrentPlanVersion()); // plan version changed
      planVersion = arangodb::tests::getCurrentPlanVersion();

      fullyUpdatedView = ci->getView(vocbase->name(), viewId);
      CHECK(fullyUpdatedView != view); // different objects
      CHECK(nullptr != fullyUpdatedView);
      CHECK(nullptr != std::dynamic_pointer_cast<arangodb::iresearch::IResearchViewCoordinator>(fullyUpdatedView));
      CHECK(planVersion == fullyUpdatedView->planVersion());
      CHECK("testView" == fullyUpdatedView->name());
      CHECK(false == fullyUpdatedView->deleted());
      CHECK(viewId == std::to_string(fullyUpdatedView->id()));
      CHECK(arangodb::iresearch::DATA_SOURCE_TYPE == fullyUpdatedView->type());
      CHECK(arangodb::LogicalView::category() == fullyUpdatedView->category());
      CHECK(vocbase == &fullyUpdatedView->vocbase());

      // check recently updated properties
      {
        VPackBuilder builder;
        builder.openObject();
        fullyUpdatedView->properties(builder, true, false);
        builder.close();

        arangodb::iresearch::IResearchViewMeta meta;
        arangodb::iresearch::IResearchViewMeta expected;
        expected._cleanupIntervalStep = 42;
        expected._consolidationIntervalMsec = 50;
        error.clear(); // clear error
        CHECK(meta.init(builder.slice(), error));
        CHECK(error.empty());
        CHECK(expected == meta);
      }

      // old object remains the same
      {
        VPackBuilder builder;
        builder.openObject();
        view->properties(builder, true, false);
        builder.close();

        arangodb::iresearch::IResearchViewMeta meta;
        error.clear(); // clear error
        CHECK(meta.init(builder.slice(), error));
        CHECK(error.empty());
        CHECK(meta == arangodb::iresearch::IResearchViewMeta::DEFAULT());
      }
    }

    // partially update properties
    {
      auto props = arangodb::velocypack::Parser::fromJson("{ \"consolidationIntervalMsec\": 42 }");
      CHECK(fullyUpdatedView->properties(props->slice(), true).ok());
      CHECK(planVersion < arangodb::tests::getCurrentPlanVersion()); // plan version changed
      planVersion = arangodb::tests::getCurrentPlanVersion();

      auto partiallyUpdatedView = ci->getView(vocbase->name(), viewId);
      CHECK(partiallyUpdatedView != view); // different objects
      CHECK(nullptr != partiallyUpdatedView);
      CHECK(nullptr != std::dynamic_pointer_cast<arangodb::iresearch::IResearchViewCoordinator>(partiallyUpdatedView));
      CHECK(planVersion == partiallyUpdatedView->planVersion());
      CHECK("testView" == partiallyUpdatedView->name());
      CHECK(false == partiallyUpdatedView->deleted());
      CHECK(viewId == std::to_string(partiallyUpdatedView->id()));
      CHECK(arangodb::iresearch::DATA_SOURCE_TYPE == partiallyUpdatedView->type());
      CHECK(arangodb::LogicalView::category() == partiallyUpdatedView->category());
      CHECK(vocbase == &partiallyUpdatedView->vocbase());

      // check recently updated properties
      {
        VPackBuilder builder;
        builder.openObject();
        partiallyUpdatedView->properties(builder, true, false);
        builder.close();

        arangodb::iresearch::IResearchViewMeta meta;
        arangodb::iresearch::IResearchViewMeta expected;
        expected._cleanupIntervalStep = 42;
        expected._consolidationIntervalMsec = 42;
        error.clear(); // clear error
        CHECK(meta.init(builder.slice(), error));
        CHECK(error.empty());
        CHECK(expected == meta);
      }
    }

    // drop view
    CHECK(view->drop().ok());
    CHECK(planVersion < arangodb::tests::getCurrentPlanVersion());

    // there is no more view
    CHECK(nullptr == ci->getView(vocbase->name(), view->name()));
  }
}

SECTION("test_update_links_partial_remove") {
  auto* database = arangodb::DatabaseFeature::DATABASE;
  REQUIRE(nullptr != database);

  auto* ci = arangodb::ClusterInfo::instance();
  REQUIRE(nullptr != ci);

  std::string error;
  TRI_vocbase_t* vocbase; // will be owned by DatabaseFeature

  // create database
  {
    // simulate heartbeat thread
    REQUIRE(TRI_ERROR_NO_ERROR == database->createDatabase(1, "testDatabase", vocbase));

    REQUIRE(nullptr != vocbase);
    CHECK("testDatabase" == vocbase->name());
    CHECK(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_COORDINATOR == vocbase->type());
    CHECK(1 == vocbase->id());

    CHECK(TRI_ERROR_NO_ERROR == ci->createDatabaseCoordinator(
      vocbase->name(), VPackSlice::emptyObjectSlice(), error, 0.0
    ));
    CHECK("no error" == error);
  }

  // create collections
  std::shared_ptr<arangodb::LogicalCollection> logicalCollection1;
  {
    auto const collectionId = "1";

    auto collectionJson = arangodb::velocypack::Parser::fromJson(
      "{ \"id\": \"1\", \"planId\": \"1\",  \"name\": \"testCollection1\", \"replicationFactor\": 1, \"type\": 1, \"shards\":{} }"
    );

    CHECK(TRI_ERROR_NO_ERROR == ci->createCollectionCoordinator(
      vocbase->name(), collectionId, 0, 1, false, collectionJson->slice(), error, 0.0
    ));

    logicalCollection1 = ci->getCollection(vocbase->name(), collectionId);
    REQUIRE(nullptr != logicalCollection1);
  }

  std::shared_ptr<arangodb::LogicalCollection> logicalCollection2;
  {
    auto const collectionId = "2";

    auto collectionJson = arangodb::velocypack::Parser::fromJson(
      "{ \"id\": \"2\", \"planId\": \"2\",  \"name\": \"testCollection2\", \"replicationFactor\": 1, \"type\": 1, \"shards\":{} }"
    );

    CHECK(TRI_ERROR_NO_ERROR == ci->createCollectionCoordinator(
      vocbase->name(), collectionId, 0, 1, false, collectionJson->slice(), error, 0.0
    ));

    logicalCollection2 = ci->getCollection(vocbase->name(), collectionId);
    REQUIRE(nullptr != logicalCollection2);
  }

  std::shared_ptr<arangodb::LogicalCollection> logicalCollection3;
  {
    auto const collectionId = "3";

    auto collectionJson = arangodb::velocypack::Parser::fromJson(
      "{ \"id\": \"3\", \"planId\": \"3\",  \"name\": \"testCollection3\", \"replicationFactor\": 1, \"type\": 1, \"shards\":{} }"
    );

    CHECK(TRI_ERROR_NO_ERROR == ci->createCollectionCoordinator(
      vocbase->name(), collectionId, 0, 1, false, collectionJson->slice(), error, 0.0
    ));

    logicalCollection3 = ci->getCollection(vocbase->name(), collectionId);
    REQUIRE(nullptr != logicalCollection3);
  }

  auto const currentCollection1Path =
      "/Current/Collections/" + vocbase->name() + "/" + std::to_string(logicalCollection1->id());
  auto const currentCollection2Path =
      "/Current/Collections/" + vocbase->name() + "/" + std::to_string(logicalCollection2->id());
  auto const currentCollection3Path =
      "/Current/Collections/" + vocbase->name() + "/" + std::to_string(logicalCollection3->id());

  // get current plan version
  auto planVersion = arangodb::tests::getCurrentPlanVersion();

  ci->loadCurrent();

  // create view
  auto viewJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"id\": \"42\", \"type\": \"arangosearch\" }");
  arangodb::LogicalView::ptr view;
  REQUIRE((arangodb::LogicalView::create(view, *vocbase, viewJson->slice()).ok()));
  REQUIRE(view);
  auto const viewId = std::to_string(view->planId());
  CHECK("42" == viewId);

  // simulate heartbeat thread (create index in current)
  {
    auto const value = arangodb::velocypack::Parser::fromJson("{ \"shard-id-does-not-matter\": { \"indexes\" : [ { \"id\": \"1\" } ] } }");
    CHECK(arangodb::AgencyComm().setValue(currentCollection1Path, value->slice(), 0.0).successful());
  }
  {
    auto const value = arangodb::velocypack::Parser::fromJson("{ \"shard-id-does-not-matter\": { \"indexes\" : [ { \"id\": \"2\" } ] } }");
    CHECK(arangodb::AgencyComm().setValue(currentCollection2Path, value->slice(), 0.0).successful());
  }
  {
    auto const value = arangodb::velocypack::Parser::fromJson("{ \"shard-id-does-not-matter\": { \"indexes\" : [ { \"id\": \"3\" } ] } }");
    CHECK(arangodb::AgencyComm().setValue(currentCollection3Path, value->slice(), 0.0).successful());
  }

  // explicitly specify id for the sake of tests
  auto linksJson = arangodb::velocypack::Parser::fromJson(
    "{ \"links\": {"
    "  \"testCollection1\" : { \"id\": \"1\", \"includeAllFields\" : true }, "
    "  \"2\" : { \"id\": \"2\", \"trackListPositions\" : true }, "
    "  \"testCollection3\" : { \"id\": \"3\" } "
    "} }"
  );
  CHECK(view->properties(linksJson->slice(), true).ok()); // add links
  CHECK(planVersion < arangodb::tests::getCurrentPlanVersion()); // plan version changed
  planVersion = arangodb::tests::getCurrentPlanVersion();
  auto oldView = view;
  view = ci->getView(vocbase->name(), viewId); // get new version of the view
  CHECK(view != oldView); // different objects
  CHECK(nullptr != view);
  CHECK(nullptr != std::dynamic_pointer_cast<arangodb::iresearch::IResearchViewCoordinator>(view));
  CHECK(planVersion == view->planVersion());
  CHECK("testView" == view->name());
  CHECK(false == view->deleted());
  CHECK(42 == view->id());
  CHECK(arangodb::iresearch::DATA_SOURCE_TYPE == view->type());
  CHECK(arangodb::LogicalView::category() == view->category());
  CHECK(vocbase == &view->vocbase());

  // visit collections
  {
    std::set<TRI_voc_cid_t> expectedLinks{
      logicalCollection1->id(),
      logicalCollection2->id(),
      logicalCollection3->id()
    };
    CHECK(view->visitCollections([&expectedLinks](TRI_voc_cid_t cid) mutable {
      return 1 == expectedLinks.erase(cid);
    }));
    CHECK(expectedLinks.empty());
  }

  // check properties
  {
    VPackBuilder info;
    info.openObject();
    view->properties(info, true, false);
    info.close();

    auto const properties = info.slice();
    CHECK(properties.hasKey(arangodb::iresearch::StaticStrings::LinksField));
    auto const linksSlice = properties.get(arangodb::iresearch::StaticStrings::LinksField);
    CHECK(linksSlice.isObject());

    VPackObjectIterator it(linksSlice);
    CHECK(it.valid());
    CHECK(3 == it.size());

    // testCollection1
    {
      auto const value = linksSlice.get(logicalCollection1->name());
      CHECK(value.isObject());

      arangodb::iresearch::IResearchLinkMeta expectedMeta;
      expectedMeta._includeAllFields = true;
      arangodb::iresearch::IResearchLinkMeta actualMeta;
      error.clear();
      CHECK(actualMeta.init(value, error));
      CHECK(error.empty());
      CHECK(expectedMeta == actualMeta);
    }

    // testCollection2
    {
      auto const value = linksSlice.get(logicalCollection2->name());
      CHECK(value.isObject());

      arangodb::iresearch::IResearchLinkMeta expectedMeta;
      expectedMeta._trackListPositions = true;
      arangodb::iresearch::IResearchLinkMeta actualMeta;
      error.clear();
      CHECK(actualMeta.init(value, error));
      CHECK(error.empty());
      CHECK(expectedMeta == actualMeta);
    }

    // testCollection3
    {
      auto const value = linksSlice.get(logicalCollection3->name());
      CHECK(value.isObject());

      arangodb::iresearch::IResearchLinkMeta expectedMeta;
      arangodb::iresearch::IResearchLinkMeta actualMeta;
      error.clear();
      CHECK(actualMeta.init(value, error));
      CHECK(error.empty());
      CHECK(expectedMeta == actualMeta);
    }
  }

  // test index in testCollection1
  {
    // get new version from plan
    auto updatedCollection = ci->getCollection(vocbase->name(), std::to_string(logicalCollection1->id()));
    REQUIRE(updatedCollection);
    auto link = arangodb::iresearch::IResearchLinkHelper::find(*updatedCollection, *view);
    CHECK(link);

    auto index = std::dynamic_pointer_cast<arangodb::Index>(link);
    REQUIRE((false == !index));
    CHECK((true == index->canBeDropped()));
    CHECK((updatedCollection.get() == &(index->collection())));
    CHECK((index->fieldNames().empty()));
    CHECK((index->fields().empty()));
    CHECK((true == index->hasBatchInsert()));
    CHECK((false == index->hasExpansion()));
    CHECK((false == index->hasSelectivityEstimate()));
    CHECK((false == index->implicitlyUnique()));
    CHECK((true == index->isPersistent()));
    CHECK((false == index->isSorted()));
    CHECK((0 < index->memory()));
    CHECK((true == index->sparse()));
    CHECK((arangodb::Index::IndexType::TRI_IDX_TYPE_IRESEARCH_LINK == index->type()));
    CHECK((arangodb::iresearch::DATA_SOURCE_TYPE.name() == index->typeName()));
    CHECK((false == index->unique()));

    arangodb::iresearch::IResearchLinkMeta expectedMeta;
    expectedMeta._includeAllFields = true;
    arangodb::iresearch::IResearchLinkMeta actualMeta;
    auto builder = index->toVelocyPack(arangodb::Index::makeFlags(arangodb::Index::Serialize::Figures));

    error.clear();
    CHECK(actualMeta.init(builder->slice(), error));
    CHECK(error.empty());
    CHECK(expectedMeta == actualMeta);
    auto const slice = builder->slice();
    CHECK(slice.hasKey("view"));
    CHECK(slice.get("view").isString());
    CHECK(view->guid() == slice.get("view").copyString());
    CHECK(slice.hasKey("figures"));
    CHECK(slice.get("figures").isObject());
    CHECK(slice.get("figures").hasKey("memory"));
    CHECK(slice.get("figures").get("memory").isNumber());
    CHECK(0 < slice.get("figures").get("memory").getUInt());
  }

  // test index in testCollection2
  {
    // get new version from plan
    auto updatedCollection = ci->getCollection(vocbase->name(), std::to_string(logicalCollection2->id()));
    REQUIRE(updatedCollection);
    auto link = arangodb::iresearch::IResearchLinkHelper::find(*updatedCollection, *view);
    CHECK(link);

    auto index = std::dynamic_pointer_cast<arangodb::Index>(link);
    REQUIRE((false == !index));
    CHECK((true == index->canBeDropped()));
    CHECK((updatedCollection.get() == &(index->collection())));
    CHECK((index->fieldNames().empty()));
    CHECK((index->fields().empty()));
    CHECK((true == index->hasBatchInsert()));
    CHECK((false == index->hasExpansion()));
    CHECK((false == index->hasSelectivityEstimate()));
    CHECK((false == index->implicitlyUnique()));
    CHECK((true == index->isPersistent()));
    CHECK((false == index->isSorted()));
    CHECK((0 < index->memory()));
    CHECK((true == index->sparse()));
    CHECK((arangodb::Index::IndexType::TRI_IDX_TYPE_IRESEARCH_LINK == index->type()));
    CHECK((arangodb::iresearch::DATA_SOURCE_TYPE.name() == index->typeName()));
    CHECK((false == index->unique()));

    arangodb::iresearch::IResearchLinkMeta expectedMeta;
    expectedMeta._trackListPositions = true;
    arangodb::iresearch::IResearchLinkMeta actualMeta;
    auto builder = index->toVelocyPack(arangodb::Index::makeFlags(arangodb::Index::Serialize::Figures));

    error.clear();
    CHECK(actualMeta.init(builder->slice(), error));
    CHECK(error.empty());
    CHECK(expectedMeta == actualMeta);
    auto const slice = builder->slice();
    CHECK(slice.hasKey("view"));
    CHECK(slice.get("view").isString());
    CHECK(view->id() == 42);
    CHECK(view->guid() == slice.get("view").copyString());
    CHECK(slice.hasKey("figures"));
    CHECK(slice.get("figures").isObject());
    CHECK(slice.get("figures").hasKey("memory"));
    CHECK(slice.get("figures").get("memory").isNumber());
    CHECK(0 < slice.get("figures").get("memory").getUInt());
  }

  // test index in testCollection3
  {
    auto updatedCollection = ci->getCollection(vocbase->name(), std::to_string(logicalCollection3->id()));
    REQUIRE(updatedCollection);
    auto link = arangodb::iresearch::IResearchLinkHelper::find(*updatedCollection, *view);
    CHECK(link);

    auto index = std::dynamic_pointer_cast<arangodb::Index>(link);
    REQUIRE((false == !index));
    CHECK((true == index->canBeDropped()));
    CHECK((updatedCollection.get() == &(index->collection())));
    CHECK((index->fieldNames().empty()));
    CHECK((index->fields().empty()));
    CHECK((true == index->hasBatchInsert()));
    CHECK((false == index->hasExpansion()));
    CHECK((false == index->hasSelectivityEstimate()));
    CHECK((false == index->implicitlyUnique()));
    CHECK((true == index->isPersistent()));
    CHECK((false == index->isSorted()));
    CHECK((0 < index->memory()));
    CHECK((true == index->sparse()));
    CHECK((arangodb::Index::IndexType::TRI_IDX_TYPE_IRESEARCH_LINK == index->type()));
    CHECK((arangodb::iresearch::DATA_SOURCE_TYPE.name() == index->typeName()));
    CHECK((false == index->unique()));

    arangodb::iresearch::IResearchLinkMeta expectedMeta;
    arangodb::iresearch::IResearchLinkMeta actualMeta;
    auto builder = index->toVelocyPack(arangodb::Index::makeFlags(arangodb::Index::Serialize::Figures));

    error.clear();
    CHECK(actualMeta.init(builder->slice(), error));
    CHECK(error.empty());
    CHECK(expectedMeta == actualMeta);
    auto const slice = builder->slice();
    CHECK(slice.hasKey("view"));
    CHECK(slice.get("view").isString());
    CHECK(view->id() == 42);
    CHECK(view->guid() == slice.get("view").copyString());
    CHECK(slice.hasKey("figures"));
    CHECK(slice.get("figures").isObject());
    CHECK(slice.get("figures").hasKey("memory"));
    CHECK(slice.get("figures").get("memory").isNumber());
    CHECK(0 < slice.get("figures").get("memory").getUInt());
  }

  CHECK(view->properties(linksJson->slice(), true).ok()); // same properties -> should not affect plan version
  CHECK(planVersion == arangodb::tests::getCurrentPlanVersion()); // plan did't change version

  // remove testCollection2 link
  // simulate heartbeat thread (create index in current)
  {
    auto const value = arangodb::velocypack::Parser::fromJson("{ \"shard-id-does-not-matter\": { \"indexes\" : [ ] } }");
    CHECK(arangodb::AgencyComm().setValue(currentCollection2Path, value->slice(), 0.0).successful());
  }

  auto const updateJson = arangodb::velocypack::Parser::fromJson(
    "{ \"links\": {"
    "  \"2\" : null "
    "} }"
  );
  CHECK(view->properties(updateJson->slice(), true).ok());
  CHECK(planVersion < arangodb::tests::getCurrentPlanVersion()); // plan version changed
  planVersion = arangodb::tests::getCurrentPlanVersion();
  oldView = view;
  view = ci->getView(vocbase->name(), viewId); // get new version of the view
  CHECK(view != oldView); // different objects
  CHECK(nullptr != view);
  CHECK(nullptr != std::dynamic_pointer_cast<arangodb::iresearch::IResearchViewCoordinator>(view));
  CHECK(planVersion == view->planVersion());
  CHECK("testView" == view->name());
  CHECK(false == view->deleted());
  CHECK(42 == view->id());
  CHECK(arangodb::iresearch::DATA_SOURCE_TYPE == view->type());
  CHECK(arangodb::LogicalView::category() == view->category());
  CHECK(vocbase == &view->vocbase());

  // visit collections
  {
    std::set<TRI_voc_cid_t> expectedLinks{
      logicalCollection1->id(),
      logicalCollection3->id()
    };
    CHECK(view->visitCollections([&expectedLinks](TRI_voc_cid_t cid) mutable {
      return 1 == expectedLinks.erase(cid);
    }));
    CHECK(expectedLinks.empty());
  }

  // check properties
  {
    VPackBuilder info;
    info.openObject();
    view->properties(info, true, false);
    info.close();

    auto const properties = info.slice();
    CHECK(properties.hasKey(arangodb::iresearch::StaticStrings::LinksField));
    auto const linksSlice = properties.get(arangodb::iresearch::StaticStrings::LinksField);
    CHECK(linksSlice.isObject());

    VPackObjectIterator it(linksSlice);
    CHECK(it.valid());
    CHECK(2 == it.size());

    // testCollection1
    {
      auto const value = linksSlice.get(logicalCollection1->name());
      CHECK(value.isObject());

      arangodb::iresearch::IResearchLinkMeta expectedMeta;
      expectedMeta._includeAllFields = true;
      arangodb::iresearch::IResearchLinkMeta actualMeta;
      error.clear();
      CHECK(actualMeta.init(value, error));
      CHECK(error.empty());
      CHECK(expectedMeta == actualMeta);
    }

    // testCollection3
    {
      auto const value = linksSlice.get(logicalCollection3->name());
      CHECK(value.isObject());

      arangodb::iresearch::IResearchLinkMeta expectedMeta;
      arangodb::iresearch::IResearchLinkMeta actualMeta;
      error.clear();
      CHECK(actualMeta.init(value, error));
      CHECK(error.empty());
      CHECK(expectedMeta == actualMeta);
    }
  }

  // test index in testCollection1
  {
    // get new version from plan
    auto updatedCollection = ci->getCollection(vocbase->name(), std::to_string(logicalCollection1->id()));
    REQUIRE(updatedCollection);
    auto link = arangodb::iresearch::IResearchLinkHelper::find(*updatedCollection, *view);
    CHECK(link);

    auto index = std::dynamic_pointer_cast<arangodb::Index>(link);
    REQUIRE((false == !index));
    CHECK((true == index->canBeDropped()));
    CHECK((updatedCollection.get() == &(index->collection())));
    CHECK((index->fieldNames().empty()));
    CHECK((index->fields().empty()));
    CHECK((true == index->hasBatchInsert()));
    CHECK((false == index->hasExpansion()));
    CHECK((false == index->hasSelectivityEstimate()));
    CHECK((false == index->implicitlyUnique()));
    CHECK((true == index->isPersistent()));
    CHECK((false == index->isSorted()));
    CHECK((0 < index->memory()));
    CHECK((true == index->sparse()));
    CHECK((arangodb::Index::IndexType::TRI_IDX_TYPE_IRESEARCH_LINK == index->type()));
    CHECK((arangodb::iresearch::DATA_SOURCE_TYPE.name() == index->typeName()));
    CHECK((false == index->unique()));

    arangodb::iresearch::IResearchLinkMeta expectedMeta;
    expectedMeta._includeAllFields = true;
    arangodb::iresearch::IResearchLinkMeta actualMeta;
    auto builder = index->toVelocyPack(arangodb::Index::makeFlags(arangodb::Index::Serialize::Figures));

    error.clear();
    CHECK(actualMeta.init(builder->slice(), error));
    CHECK(error.empty());
    CHECK(expectedMeta == actualMeta);
    auto const slice = builder->slice();
    CHECK(slice.hasKey("view"));
    CHECK(slice.get("view").isString());
    CHECK(view->id() == 42);
    CHECK(view->guid() == slice.get("view").copyString());
    CHECK(slice.hasKey("figures"));
    CHECK(slice.get("figures").isObject());
    CHECK(slice.get("figures").hasKey("memory"));
    CHECK(slice.get("figures").get("memory").isNumber());
    CHECK(0 < slice.get("figures").get("memory").getUInt());
  }

  // test index in testCollection3
  {
    // get new version from plan
    auto updatedCollection = ci->getCollection(vocbase->name(), std::to_string(logicalCollection3->id()));
    REQUIRE(updatedCollection);
    auto link = arangodb::iresearch::IResearchLinkHelper::find(*updatedCollection, *view);
    CHECK(link);

    auto index = std::dynamic_pointer_cast<arangodb::Index>(link);
    REQUIRE((false == !index));
    CHECK((true == index->canBeDropped()));
    CHECK((updatedCollection.get() == &(index->collection())));
    CHECK((index->fieldNames().empty()));
    CHECK((index->fields().empty()));
    CHECK((true == index->hasBatchInsert()));
    CHECK((false == index->hasExpansion()));
    CHECK((false == index->hasSelectivityEstimate()));
    CHECK((false == index->implicitlyUnique()));
    CHECK((true == index->isPersistent()));
    CHECK((false == index->isSorted()));
    CHECK((0 < index->memory()));
    CHECK((true == index->sparse()));
    CHECK((arangodb::Index::IndexType::TRI_IDX_TYPE_IRESEARCH_LINK == index->type()));
    CHECK((arangodb::iresearch::DATA_SOURCE_TYPE.name() == index->typeName()));
    CHECK((false == index->unique()));

    arangodb::iresearch::IResearchLinkMeta expectedMeta;
    arangodb::iresearch::IResearchLinkMeta actualMeta;
    auto builder = index->toVelocyPack(arangodb::Index::makeFlags(arangodb::Index::Serialize::Figures));

    error.clear();
    CHECK(actualMeta.init(builder->slice(), error));
    CHECK(error.empty());
    CHECK(expectedMeta == actualMeta);
    auto const slice = builder->slice();
    CHECK(slice.hasKey("view"));
    CHECK(slice.get("view").isString());
    CHECK(view->id() == 42);
    CHECK(view->guid() == slice.get("view").copyString());
    CHECK(slice.hasKey("figures"));
    CHECK(slice.get("figures").isObject());
    CHECK(slice.get("figures").hasKey("memory"));
    CHECK(slice.get("figures").get("memory").isNumber());
    CHECK(0 < slice.get("figures").get("memory").getUInt());
  }

  // drop view
  // simulate heartbeat thread (drop index in current)
  {
    auto const value = arangodb::velocypack::Parser::fromJson("{ \"shard-id-does-not-matter\": { \"indexes\" : [ ] } }");
    CHECK(arangodb::AgencyComm().setValue(currentCollection1Path, value->slice(), 0.0).successful());
  }
  {
    auto const value = arangodb::velocypack::Parser::fromJson("{ \"shard-id-does-not-matter\": { \"indexes\" : [ ] } }");
    CHECK(arangodb::AgencyComm().setValue(currentCollection3Path, value->slice(), 0.0).successful());
  }

  CHECK(view->drop().ok());
  CHECK(planVersion < arangodb::tests::getCurrentPlanVersion());

  // there is no more view
  CHECK(nullptr == ci->getView(vocbase->name(), view->name()));

  // there are no more links
  {
    // get new version from plan
    auto updatedCollection = ci->getCollection(vocbase->name(), std::to_string(logicalCollection1->id()));
    REQUIRE(updatedCollection);
    auto link = arangodb::iresearch::IResearchLinkHelper::find(*updatedCollection, *view);
    CHECK(!link);
  }

  {
    // get new version from plan
    auto updatedCollection = ci->getCollection(vocbase->name(), std::to_string(logicalCollection2->id()));
    REQUIRE(updatedCollection);
    auto link = arangodb::iresearch::IResearchLinkHelper::find(*updatedCollection, *view);
    CHECK(!link);
  }

  {
    // get new version from plan
    auto updatedCollection = ci->getCollection(vocbase->name(), std::to_string(logicalCollection3->id()));
    REQUIRE(updatedCollection);
    auto link = arangodb::iresearch::IResearchLinkHelper::find(*updatedCollection, *view);
    CHECK(!link);
  }
}

SECTION("test_update_links_partial_add") {
  auto* database = arangodb::DatabaseFeature::DATABASE;
  REQUIRE(nullptr != database);

  auto* ci = arangodb::ClusterInfo::instance();
  REQUIRE(nullptr != ci);

  std::string error;
  TRI_vocbase_t* vocbase; // will be owned by DatabaseFeature

  // create database
  {
    // simulate heartbeat thread
    REQUIRE(TRI_ERROR_NO_ERROR == database->createDatabase(1, "testDatabase", vocbase));

    REQUIRE(nullptr != vocbase);
    CHECK("testDatabase" == vocbase->name());
    CHECK(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_COORDINATOR == vocbase->type());
    CHECK(1 == vocbase->id());

    CHECK(TRI_ERROR_NO_ERROR == ci->createDatabaseCoordinator(
      vocbase->name(), VPackSlice::emptyObjectSlice(), error, 0.0
    ));
    CHECK("no error" == error);
  }

  // create collections
  std::shared_ptr<arangodb::LogicalCollection> logicalCollection1;
  {
    auto const collectionId = "1";

    auto collectionJson = arangodb::velocypack::Parser::fromJson(
      "{ \"id\": \"1\", \"planId\": \"1\",  \"name\": \"testCollection1\", \"replicationFactor\": 1, \"type\": 1, \"shards\":{} }"
    );

    CHECK(TRI_ERROR_NO_ERROR == ci->createCollectionCoordinator(
      vocbase->name(), collectionId, 0, 1, false, collectionJson->slice(), error, 0.0
    ));

    logicalCollection1 = ci->getCollection(vocbase->name(), collectionId);
    REQUIRE(nullptr != logicalCollection1);
  }

  std::shared_ptr<arangodb::LogicalCollection> logicalCollection2;
  {
    auto const collectionId = "2";

    auto collectionJson = arangodb::velocypack::Parser::fromJson(
      "{ \"id\": \"2\", \"planId\": \"2\",  \"name\": \"testCollection2\", \"replicationFactor\": 1, \"type\": 1, \"shards\":{} }"
    );

    CHECK(TRI_ERROR_NO_ERROR == ci->createCollectionCoordinator(
      vocbase->name(), collectionId, 0, 1, false, collectionJson->slice(), error, 0.0
    ));

    logicalCollection2 = ci->getCollection(vocbase->name(), collectionId);
    REQUIRE(nullptr != logicalCollection2);
  }

  std::shared_ptr<arangodb::LogicalCollection> logicalCollection3;
  {
    auto const collectionId = "3";

    auto collectionJson = arangodb::velocypack::Parser::fromJson(
      "{ \"id\": \"3\", \"planId\": \"3\",  \"name\": \"testCollection3\", \"replicationFactor\": 1, \"type\": 1, \"shards\":{} }"
    );

    CHECK(TRI_ERROR_NO_ERROR == ci->createCollectionCoordinator(
      vocbase->name(), collectionId, 0, 1, false, collectionJson->slice(), error, 0.0
    ));

    logicalCollection3 = ci->getCollection(vocbase->name(), collectionId);
    REQUIRE(nullptr != logicalCollection3);
  }

  auto const currentCollection1Path =
      "/Current/Collections/" + vocbase->name() + "/" + std::to_string(logicalCollection1->id());
  auto const currentCollection2Path =
      "/Current/Collections/" + vocbase->name() + "/" + std::to_string(logicalCollection2->id());
  auto const currentCollection3Path =
      "/Current/Collections/" + vocbase->name() + "/" + std::to_string(logicalCollection3->id());

  // get current plan version
  auto planVersion = arangodb::tests::getCurrentPlanVersion();

  ci->loadCurrent();

  // create view
  auto viewJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"id\": \"42\", \"type\": \"arangosearch\" }");
  arangodb::LogicalView::ptr view;
  REQUIRE((arangodb::LogicalView::create(view, *vocbase, viewJson->slice()).ok()));
  REQUIRE(view);
  auto const viewId = std::to_string(view->planId());
  CHECK("42" == viewId);

  // simulate heartbeat thread (create index in current)
  {
    auto const value = arangodb::velocypack::Parser::fromJson("{ \"shard-id-does-not-matter\": { \"indexes\" : [ { \"id\": \"1\" } ] } }");
    CHECK(arangodb::AgencyComm().setValue(currentCollection1Path, value->slice(), 0.0).successful());
  }
  {
    auto const value = arangodb::velocypack::Parser::fromJson("{ \"shard-id-does-not-matter\": { \"indexes\" : [ { \"id\": \"3\" } ] } }");
    CHECK(arangodb::AgencyComm().setValue(currentCollection3Path, value->slice(), 0.0).successful());
  }

  // explicitly specify id for the sake of tests
  auto linksJson = arangodb::velocypack::Parser::fromJson(
    "{ \"links\": {"
    "  \"testCollection1\" : { \"id\": \"1\", \"includeAllFields\" : true }, "
    "  \"testCollection3\" : { \"id\": \"3\" } "
    "} }"
  );
  CHECK(view->properties(linksJson->slice(), true).ok()); // add links
  CHECK(planVersion < arangodb::tests::getCurrentPlanVersion()); // plan version changed
  planVersion = arangodb::tests::getCurrentPlanVersion();
  auto oldView = view;
  view = ci->getView(vocbase->name(), viewId); // get new version of the view
  CHECK(view != oldView); // different objects
  CHECK(nullptr != view);
  CHECK(nullptr != std::dynamic_pointer_cast<arangodb::iresearch::IResearchViewCoordinator>(view));
  CHECK(planVersion == view->planVersion());
  CHECK("testView" == view->name());
  CHECK(false == view->deleted());
  CHECK(42 == view->id());
  CHECK(arangodb::iresearch::DATA_SOURCE_TYPE == view->type());
  CHECK(arangodb::LogicalView::category() == view->category());
  CHECK(vocbase == &view->vocbase());

  // visit collections
  {
    std::set<TRI_voc_cid_t> expectedLinks{
      logicalCollection1->id(),
      logicalCollection3->id()
    };
    CHECK(view->visitCollections([&expectedLinks](TRI_voc_cid_t cid) mutable {
      return 1 == expectedLinks.erase(cid);
    }));
    CHECK(expectedLinks.empty());
  }

  // check properties
  {
    VPackBuilder info;
    info.openObject();
    view->properties(info, true, false);
    info.close();

    auto const properties = info.slice();
    CHECK(properties.hasKey(arangodb::iresearch::StaticStrings::LinksField));
    auto const linksSlice = properties.get(arangodb::iresearch::StaticStrings::LinksField);
    CHECK(linksSlice.isObject());

    VPackObjectIterator it(linksSlice);
    CHECK(it.valid());
    CHECK(2 == it.size());

    // testCollection1
    {
      auto const value = linksSlice.get(logicalCollection1->name());
      CHECK(value.isObject());

      arangodb::iresearch::IResearchLinkMeta expectedMeta;
      expectedMeta._includeAllFields = true;
      arangodb::iresearch::IResearchLinkMeta actualMeta;
      error.clear();
      CHECK(actualMeta.init(value, error));
      CHECK(error.empty());
      CHECK(expectedMeta == actualMeta);
    }

    // testCollection3
    {
      auto const value = linksSlice.get(logicalCollection3->name());
      CHECK(value.isObject());

      arangodb::iresearch::IResearchLinkMeta expectedMeta;
      arangodb::iresearch::IResearchLinkMeta actualMeta;
      error.clear();
      CHECK(actualMeta.init(value, error));
      CHECK(error.empty());
      CHECK(expectedMeta == actualMeta);
    }
  }

  // test index in testCollection1
  {
    // get new version from plan
    auto updatedCollection = ci->getCollection(vocbase->name(), std::to_string(logicalCollection1->id()));
    REQUIRE(updatedCollection);
    auto link = arangodb::iresearch::IResearchLinkHelper::find(*updatedCollection, *view);
    CHECK(link);

    auto index = std::dynamic_pointer_cast<arangodb::Index>(link);
    REQUIRE((false == !index));
    CHECK((true == index->canBeDropped()));
    CHECK((updatedCollection.get() == &(index->collection())));
    CHECK((index->fieldNames().empty()));
    CHECK((index->fields().empty()));
    CHECK((true == index->hasBatchInsert()));
    CHECK((false == index->hasExpansion()));
    CHECK((false == index->hasSelectivityEstimate()));
    CHECK((false == index->implicitlyUnique()));
    CHECK((true == index->isPersistent()));
    CHECK((false == index->isSorted()));
    CHECK((0 < index->memory()));
    CHECK((true == index->sparse()));
    CHECK((arangodb::Index::IndexType::TRI_IDX_TYPE_IRESEARCH_LINK == index->type()));
    CHECK((arangodb::iresearch::DATA_SOURCE_TYPE.name() == index->typeName()));
    CHECK((false == index->unique()));

    arangodb::iresearch::IResearchLinkMeta expectedMeta;
    expectedMeta._includeAllFields = true;
    arangodb::iresearch::IResearchLinkMeta actualMeta;
    auto builder = index->toVelocyPack(arangodb::Index::makeFlags(arangodb::Index::Serialize::Figures));

    error.clear();
    CHECK(actualMeta.init(builder->slice(), error));
    CHECK(error.empty());
    CHECK(expectedMeta == actualMeta);
    auto const slice = builder->slice();
    CHECK(slice.hasKey("view"));
    CHECK(slice.get("view").isString());
    CHECK(view->id() == 42);
    CHECK(view->guid() == slice.get("view").copyString());
    CHECK(slice.hasKey("figures"));
    CHECK(slice.get("figures").isObject());
    CHECK(slice.get("figures").hasKey("memory"));
    CHECK(slice.get("figures").get("memory").isNumber());
    CHECK(0 < slice.get("figures").get("memory").getUInt());
  }

  // test index in testCollection3
  {
    auto updatedCollection = ci->getCollection(vocbase->name(), std::to_string(logicalCollection3->id()));
    REQUIRE(updatedCollection);
    auto link = arangodb::iresearch::IResearchLinkHelper::find(*updatedCollection, *view);
    CHECK(link);

    auto index = std::dynamic_pointer_cast<arangodb::Index>(link);
    REQUIRE((false == !index));
    CHECK((true == index->canBeDropped()));
    CHECK((updatedCollection.get() == &(index->collection())));
    CHECK((index->fieldNames().empty()));
    CHECK((index->fields().empty()));
    CHECK((true == index->hasBatchInsert()));
    CHECK((false == index->hasExpansion()));
    CHECK((false == index->hasSelectivityEstimate()));
    CHECK((false == index->implicitlyUnique()));
    CHECK((true == index->isPersistent()));
    CHECK((false == index->isSorted()));
    CHECK((0 < index->memory()));
    CHECK((true == index->sparse()));
    CHECK((arangodb::Index::IndexType::TRI_IDX_TYPE_IRESEARCH_LINK == index->type()));
    CHECK((arangodb::iresearch::DATA_SOURCE_TYPE.name() == index->typeName()));
    CHECK((false == index->unique()));

    arangodb::iresearch::IResearchLinkMeta expectedMeta;
    arangodb::iresearch::IResearchLinkMeta actualMeta;
    auto builder = index->toVelocyPack(arangodb::Index::makeFlags(arangodb::Index::Serialize::Figures));

    error.clear();
    CHECK(actualMeta.init(builder->slice(), error));
    CHECK(error.empty());
    CHECK(expectedMeta == actualMeta);
    auto const slice = builder->slice();
    CHECK(slice.hasKey("view"));
    CHECK(slice.get("view").isString());
    CHECK(view->id() == 42);
    CHECK(view->guid() == slice.get("view").copyString());
    CHECK(slice.hasKey("figures"));
    CHECK(slice.get("figures").isObject());
    CHECK(slice.get("figures").hasKey("memory"));
    CHECK(slice.get("figures").get("memory").isNumber());
    CHECK(0 < slice.get("figures").get("memory").getUInt());
  }

  CHECK(view->properties(linksJson->slice(), true).ok()); // same properties -> should not affect plan version
  CHECK(planVersion == arangodb::tests::getCurrentPlanVersion()); // plan did't change version

  // remove testCollection2 link
  // simulate heartbeat thread (create index in current)
  {
    auto const value = arangodb::velocypack::Parser::fromJson("{ \"shard-id-does-not-matter\": { \"indexes\" : [ { \"id\": \"2\" } ] } }");
    CHECK(arangodb::AgencyComm().setValue(currentCollection2Path, value->slice(), 0.0).successful());
  }

  auto const updateJson = arangodb::velocypack::Parser::fromJson(
    "{ \"links\": {"
    "  \"2\" : { \"id\": \"2\", \"trackListPositions\" : true } "
    "} }"
  );
  CHECK(view->properties(updateJson->slice(), true).ok());
  CHECK(planVersion < arangodb::tests::getCurrentPlanVersion()); // plan version changed
  planVersion = arangodb::tests::getCurrentPlanVersion();
  oldView = view;
  view = ci->getView(vocbase->name(), viewId); // get new version of the view
  CHECK(view != oldView); // different objects
  CHECK(nullptr != view);
  CHECK(nullptr != std::dynamic_pointer_cast<arangodb::iresearch::IResearchViewCoordinator>(view));
  CHECK(planVersion == view->planVersion());
  CHECK("testView" == view->name());
  CHECK(false == view->deleted());
  CHECK(42 == view->id());
  CHECK(arangodb::iresearch::DATA_SOURCE_TYPE == view->type());
  CHECK(arangodb::LogicalView::category() == view->category());
  CHECK(vocbase == &view->vocbase());

  // visit collections
  {
    std::set<TRI_voc_cid_t> expectedLinks{
      logicalCollection1->id(),
      logicalCollection2->id(),
      logicalCollection3->id()
    };
    CHECK(view->visitCollections([&expectedLinks](TRI_voc_cid_t cid) mutable {
      return 1 == expectedLinks.erase(cid);
    }));
    CHECK(expectedLinks.empty());
  }

  // check properties
  {
    VPackBuilder info;
    info.openObject();
    view->properties(info, true, false);
    info.close();

    auto const properties = info.slice();
    CHECK(properties.hasKey(arangodb::iresearch::StaticStrings::LinksField));
    auto const linksSlice = properties.get(arangodb::iresearch::StaticStrings::LinksField);
    CHECK(linksSlice.isObject());

    VPackObjectIterator it(linksSlice);
    CHECK(it.valid());
    CHECK(3 == it.size());

    // testCollection1
    {
      auto const value = linksSlice.get(logicalCollection1->name());
      CHECK(value.isObject());

      arangodb::iresearch::IResearchLinkMeta expectedMeta;
      expectedMeta._includeAllFields = true;
      arangodb::iresearch::IResearchLinkMeta actualMeta;
      error.clear();
      CHECK(actualMeta.init(value, error));
      CHECK(error.empty());
      CHECK(expectedMeta == actualMeta);
    }

    // testCollection2
    {
      auto const value = linksSlice.get(logicalCollection2->name());
      CHECK(value.isObject());

      arangodb::iresearch::IResearchLinkMeta expectedMeta;
      expectedMeta._trackListPositions = true;
      arangodb::iresearch::IResearchLinkMeta actualMeta;
      error.clear();
      CHECK(actualMeta.init(value, error));
      CHECK(error.empty());
      CHECK(expectedMeta == actualMeta);
    }

    // testCollection3
    {
      auto const value = linksSlice.get(logicalCollection3->name());
      CHECK(value.isObject());

      arangodb::iresearch::IResearchLinkMeta expectedMeta;
      arangodb::iresearch::IResearchLinkMeta actualMeta;
      error.clear();
      CHECK(actualMeta.init(value, error));
      CHECK(error.empty());
      CHECK(expectedMeta == actualMeta);
    }
  }

  // test index in testCollection1
  {
    // get new version from plan
    auto updatedCollection = ci->getCollection(vocbase->name(), std::to_string(logicalCollection1->id()));
    REQUIRE(updatedCollection);
    auto link = arangodb::iresearch::IResearchLinkHelper::find(*updatedCollection, *view);
    CHECK(link);

    auto index = std::dynamic_pointer_cast<arangodb::Index>(link);
    REQUIRE((false == !index));
    CHECK((true == index->canBeDropped()));
    CHECK((updatedCollection.get() == &(index->collection())));
    CHECK((index->fieldNames().empty()));
    CHECK((index->fields().empty()));
    CHECK((true == index->hasBatchInsert()));
    CHECK((false == index->hasExpansion()));
    CHECK((false == index->hasSelectivityEstimate()));
    CHECK((false == index->implicitlyUnique()));
    CHECK((true == index->isPersistent()));
    CHECK((false == index->isSorted()));
    CHECK((0 < index->memory()));
    CHECK((true == index->sparse()));
    CHECK((arangodb::Index::IndexType::TRI_IDX_TYPE_IRESEARCH_LINK == index->type()));
    CHECK((arangodb::iresearch::DATA_SOURCE_TYPE.name() == index->typeName()));
    CHECK((false == index->unique()));

    arangodb::iresearch::IResearchLinkMeta expectedMeta;
    expectedMeta._includeAllFields = true;
    arangodb::iresearch::IResearchLinkMeta actualMeta;
    auto builder = index->toVelocyPack(arangodb::Index::makeFlags(arangodb::Index::Serialize::Figures));

    error.clear();
    CHECK(actualMeta.init(builder->slice(), error));
    CHECK(error.empty());
    CHECK(expectedMeta == actualMeta);
    auto const slice = builder->slice();
    CHECK(slice.hasKey("view"));
    CHECK(slice.get("view").isString());
    CHECK(view->id() == 42);
    CHECK(view->guid() == slice.get("view").copyString());
    CHECK(slice.hasKey("figures"));
    CHECK(slice.get("figures").isObject());
    CHECK(slice.get("figures").hasKey("memory"));
    CHECK(slice.get("figures").get("memory").isNumber());
    CHECK(0 < slice.get("figures").get("memory").getUInt());
  }

  // test index in testCollection2
  {
    // get new version from plan
    auto updatedCollection = ci->getCollection(vocbase->name(), std::to_string(logicalCollection2->id()));
    REQUIRE(updatedCollection);
    auto link = arangodb::iresearch::IResearchLinkHelper::find(*updatedCollection, *view);
    CHECK(link);

    auto index = std::dynamic_pointer_cast<arangodb::Index>(link);
    REQUIRE((false == !index));
    CHECK((true == index->canBeDropped()));
    CHECK((updatedCollection.get() == &(index->collection())));
    CHECK((index->fieldNames().empty()));
    CHECK((index->fields().empty()));
    CHECK((true == index->hasBatchInsert()));
    CHECK((false == index->hasExpansion()));
    CHECK((false == index->hasSelectivityEstimate()));
    CHECK((false == index->implicitlyUnique()));
    CHECK((true == index->isPersistent()));
    CHECK((false == index->isSorted()));
    CHECK((0 < index->memory()));
    CHECK((true == index->sparse()));
    CHECK((arangodb::Index::IndexType::TRI_IDX_TYPE_IRESEARCH_LINK == index->type()));
    CHECK((arangodb::iresearch::DATA_SOURCE_TYPE.name() == index->typeName()));
    CHECK((false == index->unique()));

    arangodb::iresearch::IResearchLinkMeta expectedMeta;
    expectedMeta._trackListPositions = true;
    arangodb::iresearch::IResearchLinkMeta actualMeta;
    auto builder = index->toVelocyPack(arangodb::Index::makeFlags(arangodb::Index::Serialize::Figures));

    error.clear();
    CHECK(actualMeta.init(builder->slice(), error));
    CHECK(error.empty());
    CHECK(expectedMeta == actualMeta);
    auto const slice = builder->slice();
    CHECK(slice.hasKey("view"));
    CHECK(slice.get("view").isString());
    CHECK(view->id() == 42);
    CHECK(view->guid() == slice.get("view").copyString());
    CHECK(slice.hasKey("figures"));
    CHECK(slice.get("figures").isObject());
    CHECK(slice.get("figures").hasKey("memory"));
    CHECK(slice.get("figures").get("memory").isNumber());
    CHECK(0 < slice.get("figures").get("memory").getUInt());
  }

  // test index in testCollection3
  {
    // get new version from plan
    auto updatedCollection = ci->getCollection(vocbase->name(), std::to_string(logicalCollection3->id()));
    REQUIRE(updatedCollection);
    auto link = arangodb::iresearch::IResearchLinkHelper::find(*updatedCollection, *view);
    CHECK(link);

    auto index = std::dynamic_pointer_cast<arangodb::Index>(link);
    REQUIRE((false == !index));
    CHECK((true == index->canBeDropped()));
    CHECK((updatedCollection.get() == &(index->collection())));
    CHECK((index->fieldNames().empty()));
    CHECK((index->fields().empty()));
    CHECK((true == index->hasBatchInsert()));
    CHECK((false == index->hasExpansion()));
    CHECK((false == index->hasSelectivityEstimate()));
    CHECK((false == index->implicitlyUnique()));
    CHECK((true == index->isPersistent()));
    CHECK((false == index->isSorted()));
    CHECK((0 < index->memory()));
    CHECK((true == index->sparse()));
    CHECK((arangodb::Index::IndexType::TRI_IDX_TYPE_IRESEARCH_LINK == index->type()));
    CHECK((arangodb::iresearch::DATA_SOURCE_TYPE.name() == index->typeName()));
    CHECK((false == index->unique()));

    arangodb::iresearch::IResearchLinkMeta expectedMeta;
    arangodb::iresearch::IResearchLinkMeta actualMeta;
    auto builder = index->toVelocyPack(arangodb::Index::makeFlags(arangodb::Index::Serialize::Figures));

    error.clear();
    CHECK(actualMeta.init(builder->slice(), error));
    CHECK(error.empty());
    CHECK(expectedMeta == actualMeta);
    auto const slice = builder->slice();
    CHECK(slice.hasKey("view"));
    CHECK(slice.get("view").isString());
    CHECK(view->id() == 42);
    CHECK(view->guid() == slice.get("view").copyString());
    CHECK(slice.hasKey("figures"));
    CHECK(slice.get("figures").isObject());
    CHECK(slice.get("figures").hasKey("memory"));
    CHECK(slice.get("figures").get("memory").isNumber());
    CHECK(0 < slice.get("figures").get("memory").getUInt());
  }

  // partial update - empty delta
  {
    auto const updateJson = arangodb::velocypack::Parser::fromJson("{ }");
    CHECK(view->properties(updateJson->slice(), true).ok()); // empty properties -> should not affect plan version
    CHECK(planVersion == arangodb::tests::getCurrentPlanVersion()); // plan did't change version
  }

  // drop view
  // simulate heartbeat thread (drop index in current)
  {
    auto const value = arangodb::velocypack::Parser::fromJson("{ \"shard-id-does-not-matter\": { \"indexes\" : [ ] } }");
    CHECK(arangodb::AgencyComm().setValue(currentCollection1Path, value->slice(), 0.0).successful());
  }
  {
    auto const value = arangodb::velocypack::Parser::fromJson("{ \"shard-id-does-not-matter\": { \"indexes\" : [ ] } }");
    CHECK(arangodb::AgencyComm().setValue(currentCollection2Path, value->slice(), 0.0).successful());
  }
  {
    auto const value = arangodb::velocypack::Parser::fromJson("{ \"shard-id-does-not-matter\": { \"indexes\" : [ ] } }");
    CHECK(arangodb::AgencyComm().setValue(currentCollection3Path, value->slice(), 0.0).successful());
  }

  CHECK(view->drop().ok());
  CHECK(planVersion < arangodb::tests::getCurrentPlanVersion());

  // there is no more view
  CHECK(nullptr == ci->getView(vocbase->name(), view->name()));

  // there are no more links
  {
    // get new version from plan
    auto updatedCollection = ci->getCollection(vocbase->name(), std::to_string(logicalCollection1->id()));
    REQUIRE(updatedCollection);
    auto link = arangodb::iresearch::IResearchLinkHelper::find(*updatedCollection, *view);
    CHECK(!link);
  }

  {
    // get new version from plan
    auto updatedCollection = ci->getCollection(vocbase->name(), std::to_string(logicalCollection2->id()));
    REQUIRE(updatedCollection);
    auto link = arangodb::iresearch::IResearchLinkHelper::find(*updatedCollection, *view);
    CHECK(!link);
  }

  {
    // get new version from plan
    auto updatedCollection = ci->getCollection(vocbase->name(), std::to_string(logicalCollection3->id()));
    REQUIRE(updatedCollection);
    auto link = arangodb::iresearch::IResearchLinkHelper::find(*updatedCollection, *view);
    CHECK(!link);
  }

  // add link (collection not authorized)
  {
    auto const collectionId = "1";
    logicalCollection1 = ci->getCollection(vocbase->name(), collectionId);
    REQUIRE((false == !logicalCollection1));
    arangodb::LogicalView::ptr logicalView;
    REQUIRE((arangodb::LogicalView::create(logicalView, *vocbase, viewJson->slice()).ok()));
    REQUIRE((false == !logicalView));

    CHECK((true == logicalCollection1->getIndexes().empty()));
    CHECK((true == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));

    struct ExecContext: public arangodb::ExecContext {
      ExecContext(): arangodb::ExecContext(arangodb::ExecContext::Type::Default, "", "",
                                           arangodb::auth::Level::NONE, arangodb::auth::Level::NONE) {}
    } execContext;
    auto* origExecContext = ExecContext::CURRENT;
    auto resetExecContext = irs::make_finally([origExecContext]()->void{ ExecContext::CURRENT = origExecContext; });
    ExecContext::CURRENT = &execContext;
    auto* authFeature = arangodb::AuthenticationFeature::instance();
    auto* userManager = authFeature->userManager();
    arangodb::aql::QueryRegistry queryRegistry(0); // required for UserManager::removeAllUsers()
    arangodb::auth::UserMap userMap; // empty map, no user -> no permissions
    userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database
    userManager->setQueryRegistry(&queryRegistry);
    auto resetUserManager = irs::make_finally([userManager]()->void{ userManager->removeAllUsers(); });

    CHECK((TRI_ERROR_FORBIDDEN == logicalView->properties(linksJson->slice(), true).errorNumber()));
    logicalCollection1 = ci->getCollection(vocbase->name(), collectionId);
    REQUIRE((false == !logicalCollection1));
    logicalView = ci->getView(vocbase->name(), viewId); // get new version of the view
    REQUIRE((false == !logicalView));
    CHECK((true == logicalCollection1->getIndexes().empty()));
    CHECK((true == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));
  }
}

SECTION("test_update_links_replace") {
  auto* database = arangodb::DatabaseFeature::DATABASE;
  REQUIRE(nullptr != database);

  auto* ci = arangodb::ClusterInfo::instance();
  REQUIRE(nullptr != ci);

  std::string error;
  TRI_vocbase_t* vocbase; // will be owned by DatabaseFeature

  // create database
  {
    // simulate heartbeat thread
    REQUIRE(TRI_ERROR_NO_ERROR == database->createDatabase(1, "testDatabase", vocbase));

    REQUIRE(nullptr != vocbase);
    CHECK("testDatabase" == vocbase->name());
    CHECK(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_COORDINATOR == vocbase->type());
    CHECK(1 == vocbase->id());

    CHECK(TRI_ERROR_NO_ERROR == ci->createDatabaseCoordinator(
      vocbase->name(), VPackSlice::emptyObjectSlice(), error, 0.0
    ));
    CHECK("no error" == error);
  }

  // create collections
  std::shared_ptr<arangodb::LogicalCollection> logicalCollection1;
  {
    auto const collectionId = "1";

    auto collectionJson = arangodb::velocypack::Parser::fromJson(
      "{ \"id\": \"1\", \"planId\": \"1\",  \"name\": \"testCollection1\", \"replicationFactor\": 1, \"type\": 1, \"shards\":{} }"
    );

    CHECK(TRI_ERROR_NO_ERROR == ci->createCollectionCoordinator(
      vocbase->name(), collectionId, 0, 1, false, collectionJson->slice(), error, 0.0
    ));

    logicalCollection1 = ci->getCollection(vocbase->name(), collectionId);
    REQUIRE(nullptr != logicalCollection1);
  }

  std::shared_ptr<arangodb::LogicalCollection> logicalCollection2;
  {
    auto const collectionId = "2";

    auto collectionJson = arangodb::velocypack::Parser::fromJson(
      "{ \"id\": \"2\", \"planId\": \"2\",  \"name\": \"testCollection2\", \"replicationFactor\": 1, \"type\": 1, \"shards\":{} }"
    );

    CHECK(TRI_ERROR_NO_ERROR == ci->createCollectionCoordinator(
      vocbase->name(), collectionId, 0, 1, false, collectionJson->slice(), error, 0.0
    ));

    logicalCollection2 = ci->getCollection(vocbase->name(), collectionId);
    REQUIRE(nullptr != logicalCollection2);
  }

  std::shared_ptr<arangodb::LogicalCollection> logicalCollection3;
  {
    auto const collectionId = "3";

    auto collectionJson = arangodb::velocypack::Parser::fromJson(
      "{ \"id\": \"3\", \"planId\": \"3\",  \"name\": \"testCollection3\", \"replicationFactor\": 1, \"type\": 1, \"shards\":{} }"
    );

    CHECK(TRI_ERROR_NO_ERROR == ci->createCollectionCoordinator(
      vocbase->name(), collectionId, 0, 1, false, collectionJson->slice(), error, 0.0
    ));

    logicalCollection3 = ci->getCollection(vocbase->name(), collectionId);
    REQUIRE(nullptr != logicalCollection3);
  }

  auto const currentCollection1Path =
      "/Current/Collections/" + vocbase->name() + "/" + std::to_string(logicalCollection1->id());
  auto const currentCollection2Path =
      "/Current/Collections/" + vocbase->name() + "/" + std::to_string(logicalCollection2->id());
  auto const currentCollection3Path =
      "/Current/Collections/" + vocbase->name() + "/" + std::to_string(logicalCollection3->id());

  // get current plan version
  auto planVersion = arangodb::tests::getCurrentPlanVersion();

  ci->loadCurrent();

  // create view
  auto viewJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"id\": \"42\", \"type\": \"arangosearch\" }");
  arangodb::LogicalView::ptr view;
  REQUIRE((arangodb::LogicalView::create(view, *vocbase, viewJson->slice()).ok()));
  REQUIRE(view);
  auto const viewId = std::to_string(view->planId());
  CHECK("42" == viewId);

  // simulate heartbeat thread (create index in current)
  {
    auto const value = arangodb::velocypack::Parser::fromJson("{ \"shard-id-does-not-matter\": { \"indexes\" : [ { \"id\": \"1\" } ] } }");
    CHECK(arangodb::AgencyComm().setValue(currentCollection1Path, value->slice(), 0.0).successful());
  }
  {
    auto const value = arangodb::velocypack::Parser::fromJson("{ \"shard-id-does-not-matter\": { \"indexes\" : [ { \"id\": \"3\" } ] } }");
    CHECK(arangodb::AgencyComm().setValue(currentCollection3Path, value->slice(), 0.0).successful());
  }

  // explicitly specify id for the sake of tests
  auto linksJson = arangodb::velocypack::Parser::fromJson(
    "{ \"links\": {"
    "  \"testCollection1\" : { \"id\": \"1\", \"includeAllFields\" : true }, "
    "  \"testCollection3\" : { \"id\": \"3\" } "
    "} }"
  );
  CHECK(view->properties(linksJson->slice(), false).ok()); // add link
  CHECK(planVersion < arangodb::tests::getCurrentPlanVersion()); // plan version changed
  planVersion = arangodb::tests::getCurrentPlanVersion();
  auto oldView = view;
  view = ci->getView(vocbase->name(), viewId); // get new version of the view
  CHECK(view != oldView); // different objects
  CHECK(nullptr != view);
  CHECK(nullptr != std::dynamic_pointer_cast<arangodb::iresearch::IResearchViewCoordinator>(view));
  CHECK(planVersion == view->planVersion());
  CHECK("testView" == view->name());
  CHECK(false == view->deleted());
  CHECK(42 == view->id());
  CHECK(arangodb::iresearch::DATA_SOURCE_TYPE == view->type());
  CHECK(arangodb::LogicalView::category() == view->category());
  CHECK(vocbase == &view->vocbase());

  // visit collections
  {
    std::set<TRI_voc_cid_t> expectedLinks{
      logicalCollection1->id(),
      logicalCollection3->id()
    };
    CHECK(view->visitCollections([&expectedLinks](TRI_voc_cid_t cid) mutable {
      return 1 == expectedLinks.erase(cid);
    }));
    CHECK(expectedLinks.empty());
  }

  // check properties
  {
    VPackBuilder info;
    info.openObject();
    view->properties(info, true, false);
    info.close();

    auto const properties = info.slice();
    CHECK(properties.hasKey(arangodb::iresearch::StaticStrings::LinksField));
    auto const linksSlice = properties.get(arangodb::iresearch::StaticStrings::LinksField);
    CHECK(linksSlice.isObject());

    VPackObjectIterator it(linksSlice);
    CHECK(it.valid());
    CHECK(2 == it.size());

    // testCollection1
    {
      auto const value = linksSlice.get(logicalCollection1->name());
      CHECK(value.isObject());

      arangodb::iresearch::IResearchLinkMeta expectedMeta;
      expectedMeta._includeAllFields = true;
      arangodb::iresearch::IResearchLinkMeta actualMeta;
      error.clear();
      CHECK(actualMeta.init(value, error));
      CHECK(error.empty());
      CHECK(expectedMeta == actualMeta);
    }

    // testCollection3
    {
      auto const value = linksSlice.get(logicalCollection3->name());
      CHECK(value.isObject());

      arangodb::iresearch::IResearchLinkMeta expectedMeta;
      arangodb::iresearch::IResearchLinkMeta actualMeta;
      error.clear();
      CHECK(actualMeta.init(value, error));
      CHECK(error.empty());
      CHECK(expectedMeta == actualMeta);
    }
  }

  // test index in testCollection1
  {
    // get new version from plan
    auto updatedCollection = ci->getCollection(vocbase->name(), std::to_string(logicalCollection1->id()));
    REQUIRE(updatedCollection);
    auto link = arangodb::iresearch::IResearchLinkHelper::find(*updatedCollection, *view);
    CHECK(link);

    auto index = std::dynamic_pointer_cast<arangodb::Index>(link);
    REQUIRE((false == !index));
    CHECK((true == index->canBeDropped()));
    CHECK((updatedCollection.get() == &(index->collection())));
    CHECK((index->fieldNames().empty()));
    CHECK((index->fields().empty()));
    CHECK((true == index->hasBatchInsert()));
    CHECK((false == index->hasExpansion()));
    CHECK((false == index->hasSelectivityEstimate()));
    CHECK((false == index->implicitlyUnique()));
    CHECK((true == index->isPersistent()));
    CHECK((false == index->isSorted()));
    CHECK((0 < index->memory()));
    CHECK((true == index->sparse()));
    CHECK((arangodb::Index::IndexType::TRI_IDX_TYPE_IRESEARCH_LINK == index->type()));
    CHECK((arangodb::iresearch::DATA_SOURCE_TYPE.name() == index->typeName()));
    CHECK((false == index->unique()));

    arangodb::iresearch::IResearchLinkMeta expectedMeta;
    expectedMeta._includeAllFields = true;
    arangodb::iresearch::IResearchLinkMeta actualMeta;
    auto builder = index->toVelocyPack(arangodb::Index::makeFlags(arangodb::Index::Serialize::Figures));

    error.clear();
    CHECK(actualMeta.init(builder->slice(), error));
    CHECK(error.empty());
    CHECK(expectedMeta == actualMeta);
    auto const slice = builder->slice();
    CHECK(slice.hasKey("view"));
    CHECK(slice.get("view").isString());
    CHECK(view->id() == 42);
    CHECK(view->guid() == slice.get("view").copyString());
    CHECK(slice.hasKey("figures"));
    CHECK(slice.get("figures").isObject());
    CHECK(slice.get("figures").hasKey("memory"));
    CHECK(slice.get("figures").get("memory").isNumber());
    CHECK(0 < slice.get("figures").get("memory").getUInt());
  }

  // test index in testCollection3
  {
    auto updatedCollection = ci->getCollection(vocbase->name(), std::to_string(logicalCollection3->id()));
    REQUIRE(updatedCollection);
    auto link = arangodb::iresearch::IResearchLinkHelper::find(*updatedCollection, *view);
    CHECK(link);

    auto index = std::dynamic_pointer_cast<arangodb::Index>(link);
    REQUIRE((false == !index));
    CHECK((true == index->canBeDropped()));
    CHECK((updatedCollection.get() == &(index->collection())));
    CHECK((index->fieldNames().empty()));
    CHECK((index->fields().empty()));
    CHECK((true == index->hasBatchInsert()));
    CHECK((false == index->hasExpansion()));
    CHECK((false == index->hasSelectivityEstimate()));
    CHECK((false == index->implicitlyUnique()));
    CHECK((true == index->isPersistent()));
    CHECK((false == index->isSorted()));
    CHECK((0 < index->memory()));
    CHECK((true == index->sparse()));
    CHECK((arangodb::Index::IndexType::TRI_IDX_TYPE_IRESEARCH_LINK == index->type()));
    CHECK((arangodb::iresearch::DATA_SOURCE_TYPE.name() == index->typeName()));
    CHECK((false == index->unique()));

    arangodb::iresearch::IResearchLinkMeta expectedMeta;
    arangodb::iresearch::IResearchLinkMeta actualMeta;
    auto builder = index->toVelocyPack(arangodb::Index::makeFlags(arangodb::Index::Serialize::Figures));

    error.clear();
    CHECK(actualMeta.init(builder->slice(), error));
    CHECK(error.empty());
    CHECK(expectedMeta == actualMeta);
    auto const slice = builder->slice();
    CHECK(slice.hasKey("view"));
    CHECK(slice.get("view").isString());
    CHECK(view->id() == 42);
    CHECK(view->guid() == slice.get("view").copyString());
    CHECK(slice.hasKey("figures"));
    CHECK(slice.get("figures").isObject());
    CHECK(slice.get("figures").hasKey("memory"));
    CHECK(slice.get("figures").get("memory").isNumber());
    CHECK(0 < slice.get("figures").get("memory").getUInt());
  }

  CHECK(view->properties(linksJson->slice(), false).ok()); // same properties -> should not affect plan version
  CHECK(planVersion == arangodb::tests::getCurrentPlanVersion()); // plan did't change version

  CHECK(view->properties(linksJson->slice(), true).ok()); // same properties -> should not affect plan version
  CHECK(planVersion == arangodb::tests::getCurrentPlanVersion()); // plan did't change version

  // replace links with testCollection2 link
  // simulate heartbeat thread (create index in current)
  {
    auto const value = arangodb::velocypack::Parser::fromJson("{ \"shard-id-does-not-matter\": { \"indexes\" : [ ] } }");
    CHECK(arangodb::AgencyComm().setValue(currentCollection1Path, value->slice(), 0.0).successful());
  }
  {
    auto const value = arangodb::velocypack::Parser::fromJson("{ \"shard-id-does-not-matter\": { \"indexes\" : [ { \"id\": \"2\" } ] } }");
    CHECK(arangodb::AgencyComm().setValue(currentCollection2Path, value->slice(), 0.0).successful());
  }
  {
    auto const value = arangodb::velocypack::Parser::fromJson("{ \"shard-id-does-not-matter\": { \"indexes\" : [ ] } }");
    CHECK(arangodb::AgencyComm().setValue(currentCollection3Path, value->slice(), 0.0).successful());
  }

  auto updateJson = arangodb::velocypack::Parser::fromJson(
    "{ \"links\": {"
    "  \"2\" : { \"id\": \"2\", \"trackListPositions\" : true } "
    "} }"
  );
  CHECK(view->properties(updateJson->slice(), false).ok());
  CHECK(planVersion < arangodb::tests::getCurrentPlanVersion()); // plan version changed
  planVersion = arangodb::tests::getCurrentPlanVersion();
  oldView = view;
  view = ci->getView(vocbase->name(), viewId); // get new version of the view
  CHECK(view != oldView); // different objects
  CHECK(nullptr != view);
  CHECK(nullptr != std::dynamic_pointer_cast<arangodb::iresearch::IResearchViewCoordinator>(view));
  CHECK(planVersion == view->planVersion());
  CHECK("testView" == view->name());
  CHECK(false == view->deleted());
  CHECK(42 == view->id());
  CHECK(arangodb::iresearch::DATA_SOURCE_TYPE == view->type());
  CHECK(arangodb::LogicalView::category() == view->category());
  CHECK(vocbase == &view->vocbase());

  // visit collections
  {
    std::set<TRI_voc_cid_t> expectedLinks{
      logicalCollection2->id()
    };
    CHECK(view->visitCollections([&expectedLinks](TRI_voc_cid_t cid) mutable {
      return 1 == expectedLinks.erase(cid);
    }));
    CHECK(expectedLinks.empty());
  }

  // check properties
  {
    VPackBuilder info;
    info.openObject();
    view->properties(info, true, false);
    info.close();

    auto const properties = info.slice();
    CHECK(properties.hasKey(arangodb::iresearch::StaticStrings::LinksField));
    auto const linksSlice = properties.get(arangodb::iresearch::StaticStrings::LinksField);
    CHECK(linksSlice.isObject());

    VPackObjectIterator it(linksSlice);
    CHECK(it.valid());
    CHECK(1 == it.size());

    // testCollection2
    {
      auto const value = linksSlice.get(logicalCollection2->name());
      CHECK(value.isObject());

      arangodb::iresearch::IResearchLinkMeta expectedMeta;
      expectedMeta._trackListPositions = true;
      arangodb::iresearch::IResearchLinkMeta actualMeta;
      error.clear();
      CHECK(actualMeta.init(value, error));
      CHECK(error.empty());
      CHECK(expectedMeta == actualMeta);
    }
  }

  // test index in testCollection2
  {
    // get new version from plan
    auto updatedCollection = ci->getCollection(vocbase->name(), std::to_string(logicalCollection2->id()));
    REQUIRE(updatedCollection);
    auto link = arangodb::iresearch::IResearchLinkHelper::find(*updatedCollection, *view);
    CHECK(link);

    auto index = std::dynamic_pointer_cast<arangodb::Index>(link);
    REQUIRE((false == !index));
    CHECK((true == index->canBeDropped()));
    CHECK((updatedCollection.get() == &(index->collection())));
    CHECK((index->fieldNames().empty()));
    CHECK((index->fields().empty()));
    CHECK((true == index->hasBatchInsert()));
    CHECK((false == index->hasExpansion()));
    CHECK((false == index->hasSelectivityEstimate()));
    CHECK((false == index->implicitlyUnique()));
    CHECK((true == index->isPersistent()));
    CHECK((false == index->isSorted()));
    CHECK((0 < index->memory()));
    CHECK((true == index->sparse()));
    CHECK((arangodb::Index::IndexType::TRI_IDX_TYPE_IRESEARCH_LINK == index->type()));
    CHECK((arangodb::iresearch::DATA_SOURCE_TYPE.name() == index->typeName()));
    CHECK((false == index->unique()));

    arangodb::iresearch::IResearchLinkMeta expectedMeta;
    expectedMeta._trackListPositions = true;
    arangodb::iresearch::IResearchLinkMeta actualMeta;
    auto builder = index->toVelocyPack(arangodb::Index::makeFlags(arangodb::Index::Serialize::Figures));

    error.clear();
    CHECK(actualMeta.init(builder->slice(), error));
    CHECK(error.empty());
    CHECK(expectedMeta == actualMeta);
    auto const slice = builder->slice();
    CHECK(slice.hasKey("view"));
    CHECK(slice.get("view").isString());
    CHECK(view->id() == 42);
    CHECK(view->guid() == slice.get("view").copyString());
    CHECK(slice.hasKey("figures"));
    CHECK(slice.get("figures").isObject());
    CHECK(slice.get("figures").hasKey("memory"));
    CHECK(slice.get("figures").get("memory").isNumber());
    CHECK(0 < slice.get("figures").get("memory").getUInt());
  }

  // replace links with testCollection1 link
  // simulate heartbeat thread (create index in current)
  {
    auto const value = arangodb::velocypack::Parser::fromJson("{ \"shard-id-does-not-matter\": { \"indexes\" : [ { \"id\" : \"1\" } ] } }");
    CHECK(arangodb::AgencyComm().setValue(currentCollection1Path, value->slice(), 0.0).successful());
  }
  {
    auto const value = arangodb::velocypack::Parser::fromJson("{ \"shard-id-does-not-matter\": { \"indexes\" : [ ] } }");
    CHECK(arangodb::AgencyComm().setValue(currentCollection2Path, value->slice(), 0.0).successful());
  }

  updateJson = arangodb::velocypack::Parser::fromJson(
    "{ \"links\": {"
    "  \"testCollection1\" : { \"id\": \"1\", \"includeAllFields\" : true }, "
    "  \"2\" : null "
    "} }"
  );
  CHECK(view->properties(updateJson->slice(), false).ok());
  CHECK(planVersion < arangodb::tests::getCurrentPlanVersion()); // plan version changed
  planVersion = arangodb::tests::getCurrentPlanVersion();
  oldView = view;
  view = ci->getView(vocbase->name(), viewId); // get new version of the view
  CHECK(view != oldView); // different objects
  CHECK(nullptr != view);
  CHECK(nullptr != std::dynamic_pointer_cast<arangodb::iresearch::IResearchViewCoordinator>(view));
  CHECK(planVersion == view->planVersion());
  CHECK("testView" == view->name());
  CHECK(false == view->deleted());
  CHECK(42 == view->id());
  CHECK(arangodb::iresearch::DATA_SOURCE_TYPE == view->type());
  CHECK(arangodb::LogicalView::category() == view->category());
  CHECK(vocbase == &view->vocbase());

  // visit collections
  {
    std::set<TRI_voc_cid_t> expectedLinks{
      logicalCollection1->id()
    };
    CHECK(view->visitCollections([&expectedLinks](TRI_voc_cid_t cid) mutable {
      return 1 == expectedLinks.erase(cid);
    }));
    CHECK(expectedLinks.empty());
  }

  // check properties
  {
    VPackBuilder info;
    info.openObject();
    view->properties(info, true, false);
    info.close();

    auto const properties = info.slice();
    CHECK(properties.hasKey(arangodb::iresearch::StaticStrings::LinksField));
    auto const linksSlice = properties.get(arangodb::iresearch::StaticStrings::LinksField);
    CHECK(linksSlice.isObject());

    VPackObjectIterator it(linksSlice);
    CHECK(it.valid());
    CHECK(1 == it.size());

    // testCollection2
    {
      auto const value = linksSlice.get(logicalCollection1->name());
      CHECK(value.isObject());

      arangodb::iresearch::IResearchLinkMeta expectedMeta;
      expectedMeta._includeAllFields = true;
      arangodb::iresearch::IResearchLinkMeta actualMeta;
      error.clear();
      CHECK(actualMeta.init(value, error));
      CHECK(error.empty());
      CHECK(expectedMeta == actualMeta);
    }
  }

  // test index in testCollection1
  {
    // get new version from plan
    auto updatedCollection = ci->getCollection(vocbase->name(), std::to_string(logicalCollection1->id()));
    REQUIRE(updatedCollection);
    auto link = arangodb::iresearch::IResearchLinkHelper::find(*updatedCollection, *view);
    CHECK(link);

    auto index = std::dynamic_pointer_cast<arangodb::Index>(link);
    REQUIRE((false == !index));
    CHECK((true == index->canBeDropped()));
    CHECK((updatedCollection.get() == &(index->collection())));
    CHECK((index->fieldNames().empty()));
    CHECK((index->fields().empty()));
    CHECK((true == index->hasBatchInsert()));
    CHECK((false == index->hasExpansion()));
    CHECK((false == index->hasSelectivityEstimate()));
    CHECK((false == index->implicitlyUnique()));
    CHECK((true == index->isPersistent()));
    CHECK((false == index->isSorted()));
    CHECK((0 < index->memory()));
    CHECK((true == index->sparse()));
    CHECK((arangodb::Index::IndexType::TRI_IDX_TYPE_IRESEARCH_LINK == index->type()));
    CHECK((arangodb::iresearch::DATA_SOURCE_TYPE.name() == index->typeName()));
    CHECK((false == index->unique()));

    arangodb::iresearch::IResearchLinkMeta expectedMeta;
    expectedMeta._includeAllFields = true;
    arangodb::iresearch::IResearchLinkMeta actualMeta;
    auto builder = index->toVelocyPack(arangodb::Index::makeFlags(arangodb::Index::Serialize::Figures));

    error.clear();
    CHECK(actualMeta.init(builder->slice(), error));
    CHECK(error.empty());
    CHECK(expectedMeta == actualMeta);
    auto const slice = builder->slice();
    CHECK(slice.hasKey("view"));
    CHECK(slice.get("view").isString());
    CHECK(view->id() == 42);
    CHECK(view->guid() == slice.get("view").copyString());
    CHECK(slice.hasKey("figures"));
    CHECK(slice.get("figures").isObject());
    CHECK(slice.get("figures").hasKey("memory"));
    CHECK(slice.get("figures").get("memory").isNumber());
    CHECK(0 < slice.get("figures").get("memory").getUInt());
  }

  // drop view
  // simulate heartbeat thread (drop index in current)
  {
    auto const value = arangodb::velocypack::Parser::fromJson("{ \"shard-id-does-not-matter\": { \"indexes\" : [ ] } }");
    CHECK(arangodb::AgencyComm().setValue(currentCollection1Path, value->slice(), 0.0).successful());
  }

  CHECK(view->drop().ok());
  CHECK(planVersion < arangodb::tests::getCurrentPlanVersion());

  // there is no more view
  CHECK(nullptr == ci->getView(vocbase->name(), view->name()));

  // there are no more links
  {
    // get new version from plan
    auto updatedCollection = ci->getCollection(vocbase->name(), std::to_string(logicalCollection1->id()));
    REQUIRE(updatedCollection);
    auto link = arangodb::iresearch::IResearchLinkHelper::find(*updatedCollection, *view);
    CHECK(!link);
  }

  {
    // get new version from plan
    auto updatedCollection = ci->getCollection(vocbase->name(), std::to_string(logicalCollection2->id()));
    REQUIRE(updatedCollection);
    auto link = arangodb::iresearch::IResearchLinkHelper::find(*updatedCollection, *view);
    CHECK(!link);
  }

  {
    // get new version from plan
    auto updatedCollection = ci->getCollection(vocbase->name(), std::to_string(logicalCollection3->id()));
    REQUIRE(updatedCollection);
    auto link = arangodb::iresearch::IResearchLinkHelper::find(*updatedCollection, *view);
    CHECK(!link);
  }
}

SECTION("test_update_links_clear") {
  auto* database = arangodb::DatabaseFeature::DATABASE;
  REQUIRE(nullptr != database);

  auto* ci = arangodb::ClusterInfo::instance();
  REQUIRE(nullptr != ci);

  std::string error;
  TRI_vocbase_t* vocbase; // will be owned by DatabaseFeature

  // create database
  {
    // simulate heartbeat thread
    REQUIRE(TRI_ERROR_NO_ERROR == database->createDatabase(1, "testDatabase", vocbase));

    REQUIRE(nullptr != vocbase);
    CHECK("testDatabase" == vocbase->name());
    CHECK(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_COORDINATOR == vocbase->type());
    CHECK(1 == vocbase->id());

    CHECK(TRI_ERROR_NO_ERROR == ci->createDatabaseCoordinator(
      vocbase->name(), VPackSlice::emptyObjectSlice(), error, 0.0
    ));
    CHECK("no error" == error);
  }

  // create collections
  std::shared_ptr<arangodb::LogicalCollection> logicalCollection1;
  {
    auto const collectionId = "1";

    auto collectionJson = arangodb::velocypack::Parser::fromJson(
      "{ \"id\": \"1\", \"planId\": \"1\",  \"name\": \"testCollection1\", \"replicationFactor\": 1, \"type\": 1, \"shards\":{} }"
    );

    CHECK(TRI_ERROR_NO_ERROR == ci->createCollectionCoordinator(
      vocbase->name(), collectionId, 0, 1, false, collectionJson->slice(), error, 0.0
    ));

    logicalCollection1 = ci->getCollection(vocbase->name(), collectionId);
    REQUIRE(nullptr != logicalCollection1);
  }

  std::shared_ptr<arangodb::LogicalCollection> logicalCollection2;
  {
    auto const collectionId = "2";

    auto collectionJson = arangodb::velocypack::Parser::fromJson(
      "{ \"id\": \"2\", \"planId\": \"2\",  \"name\": \"testCollection2\", \"replicationFactor\": 1, \"type\": 1, \"shards\":{} }"
    );

    CHECK(TRI_ERROR_NO_ERROR == ci->createCollectionCoordinator(
      vocbase->name(), collectionId, 0, 1, false, collectionJson->slice(), error, 0.0
    ));

    logicalCollection2 = ci->getCollection(vocbase->name(), collectionId);
    REQUIRE(nullptr != logicalCollection2);
  }

  std::shared_ptr<arangodb::LogicalCollection> logicalCollection3;
  {
    auto const collectionId = "3";

    auto collectionJson = arangodb::velocypack::Parser::fromJson(
      "{ \"id\": \"3\", \"planId\": \"3\",  \"name\": \"testCollection3\", \"replicationFactor\": 1, \"type\": 1, \"shards\":{} }"
    );

    CHECK(TRI_ERROR_NO_ERROR == ci->createCollectionCoordinator(
      vocbase->name(), collectionId, 0, 1, false, collectionJson->slice(), error, 0.0
    ));

    logicalCollection3 = ci->getCollection(vocbase->name(), collectionId);
    REQUIRE(nullptr != logicalCollection3);
  }

  auto const currentCollection1Path =
      "/Current/Collections/" + vocbase->name() + "/" + std::to_string(logicalCollection1->id());
  auto const currentCollection2Path =
      "/Current/Collections/" + vocbase->name() + "/" + std::to_string(logicalCollection2->id());
  auto const currentCollection3Path =
      "/Current/Collections/" + vocbase->name() + "/" + std::to_string(logicalCollection3->id());

  // get current plan version
  auto planVersion = arangodb::tests::getCurrentPlanVersion();

  ci->loadCurrent();

  // create view
  auto viewJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"id\": \"42\", \"type\": \"arangosearch\" }");
  arangodb::LogicalView::ptr view;
  REQUIRE((arangodb::LogicalView::create(view, *vocbase, viewJson->slice()).ok()));
  REQUIRE(view);
  auto const viewId = std::to_string(view->planId());
  CHECK("42" == viewId);

  // simulate heartbeat thread (create index in current)
  {
    auto const value = arangodb::velocypack::Parser::fromJson("{ \"shard-id-does-not-matter\": { \"indexes\" : [ { \"id\": \"1\" } ] } }");
    CHECK(arangodb::AgencyComm().setValue(currentCollection1Path, value->slice(), 0.0).successful());
  }
  {
    auto const value = arangodb::velocypack::Parser::fromJson("{ \"shard-id-does-not-matter\": { \"indexes\" : [ { \"id\": \"2\" } ] } }");
    CHECK(arangodb::AgencyComm().setValue(currentCollection2Path, value->slice(), 0.0).successful());
  }
  {
    auto const value = arangodb::velocypack::Parser::fromJson("{ \"shard-id-does-not-matter\": { \"indexes\" : [ { \"id\": \"3\" } ] } }");
    CHECK(arangodb::AgencyComm().setValue(currentCollection3Path, value->slice(), 0.0).successful());
  }

  // explicitly specify id for the sake of tests
  auto linksJson = arangodb::velocypack::Parser::fromJson(
    "{ \"locale\": \"en\", \"links\": {"
    "  \"testCollection1\" : { \"id\": \"1\", \"includeAllFields\" : true }, "
    "  \"2\" : { \"id\": \"2\", \"trackListPositions\" : true }, "
    "  \"testCollection3\" : { \"id\": \"3\" } "
    "} }"
  );
  CHECK(view->properties(linksJson->slice(), false).ok()); // add link
  CHECK(planVersion < arangodb::tests::getCurrentPlanVersion()); // plan version changed
  planVersion = arangodb::tests::getCurrentPlanVersion();
  auto oldView = view;
  view = ci->getView(vocbase->name(), viewId); // get new version of the view
  CHECK(view != oldView); // different objects
  CHECK(nullptr != view);
  CHECK(nullptr != std::dynamic_pointer_cast<arangodb::iresearch::IResearchViewCoordinator>(view));
  CHECK(planVersion == view->planVersion());
  CHECK("testView" == view->name());
  CHECK(false == view->deleted());
  CHECK(42 == view->id());
  CHECK(arangodb::iresearch::DATA_SOURCE_TYPE == view->type());
  CHECK(arangodb::LogicalView::category() == view->category());
  CHECK(vocbase == &view->vocbase());

  // visit collections
  {
    std::set<TRI_voc_cid_t> expectedLinks{
      logicalCollection1->id(),
      logicalCollection2->id(),
      logicalCollection3->id()
    };
    CHECK(view->visitCollections([&expectedLinks](TRI_voc_cid_t cid) mutable {
      return 1 == expectedLinks.erase(cid);
    }));
    CHECK(expectedLinks.empty());
  }

  // check properties
  {
    VPackBuilder info;
    info.openObject();
    view->properties(info, true, false);
    info.close();

    auto const properties = info.slice();
    CHECK(properties.hasKey(arangodb::iresearch::StaticStrings::LinksField));
    auto const linksSlice = properties.get(arangodb::iresearch::StaticStrings::LinksField);
    CHECK(linksSlice.isObject());

    VPackObjectIterator it(linksSlice);
    CHECK(it.valid());
    CHECK(3 == it.size());

    // testCollection1
    {
      auto const value = linksSlice.get(logicalCollection1->name());
      CHECK(value.isObject());

      arangodb::iresearch::IResearchLinkMeta expectedMeta;
      expectedMeta._includeAllFields = true;
      arangodb::iresearch::IResearchLinkMeta actualMeta;
      error.clear();
      CHECK(actualMeta.init(value, error));
      CHECK(error.empty());
      CHECK(expectedMeta == actualMeta);
    }

    // testCollection2
    {
      auto const value = linksSlice.get(logicalCollection2->name());
      CHECK(value.isObject());

      arangodb::iresearch::IResearchLinkMeta expectedMeta;
      expectedMeta._trackListPositions = true;
      arangodb::iresearch::IResearchLinkMeta actualMeta;
      error.clear();
      CHECK(actualMeta.init(value, error));
      CHECK(error.empty());
      CHECK(expectedMeta == actualMeta);
    }

    // testCollection3
    {
      auto const value = linksSlice.get(logicalCollection3->name());
      CHECK(value.isObject());

      arangodb::iresearch::IResearchLinkMeta expectedMeta;
      arangodb::iresearch::IResearchLinkMeta actualMeta;
      error.clear();
      CHECK(actualMeta.init(value, error));
      CHECK(error.empty());
      CHECK(expectedMeta == actualMeta);
    }
  }

  // test index in testCollection1
  {
    // get new version from plan
    auto updatedCollection = ci->getCollection(vocbase->name(), std::to_string(logicalCollection1->id()));
    REQUIRE(updatedCollection);
    auto link = arangodb::iresearch::IResearchLinkHelper::find(*updatedCollection, *view);
    CHECK(link);

    auto index = std::dynamic_pointer_cast<arangodb::Index>(link);
    REQUIRE((false == !index));
    CHECK((true == index->canBeDropped()));
    CHECK((updatedCollection.get() == &(index->collection())));
    CHECK((index->fieldNames().empty()));
    CHECK((index->fields().empty()));
    CHECK((true == index->hasBatchInsert()));
    CHECK((false == index->hasExpansion()));
    CHECK((false == index->hasSelectivityEstimate()));
    CHECK((false == index->implicitlyUnique()));
    CHECK((true == index->isPersistent()));
    CHECK((false == index->isSorted()));
    CHECK((0 < index->memory()));
    CHECK((true == index->sparse()));
    CHECK((arangodb::Index::IndexType::TRI_IDX_TYPE_IRESEARCH_LINK == index->type()));
    CHECK((arangodb::iresearch::DATA_SOURCE_TYPE.name() == index->typeName()));
    CHECK((false == index->unique()));

    arangodb::iresearch::IResearchLinkMeta expectedMeta;
    expectedMeta._includeAllFields = true;
    arangodb::iresearch::IResearchLinkMeta actualMeta;
    auto builder = index->toVelocyPack(arangodb::Index::makeFlags(arangodb::Index::Serialize::Figures));

    error.clear();
    CHECK(actualMeta.init(builder->slice(), error));
    CHECK(error.empty());
    CHECK(expectedMeta == actualMeta);
    auto const slice = builder->slice();
    CHECK(slice.hasKey("view"));
    CHECK(slice.get("view").isString());
    CHECK(view->id() == 42);
    CHECK(view->guid() == slice.get("view").copyString());
    CHECK(slice.hasKey("figures"));
    CHECK(slice.get("figures").isObject());
    CHECK(slice.get("figures").hasKey("memory"));
    CHECK(slice.get("figures").get("memory").isNumber());
    CHECK(0 < slice.get("figures").get("memory").getUInt());
  }

  // test index in testCollection2
  {
    // get new version from plan
    auto updatedCollection = ci->getCollection(vocbase->name(), std::to_string(logicalCollection2->id()));
    REQUIRE(updatedCollection);
    auto link = arangodb::iresearch::IResearchLinkHelper::find(*updatedCollection, *view);
    CHECK(link);

    auto index = std::dynamic_pointer_cast<arangodb::Index>(link);
    REQUIRE((false == !index));
    CHECK((true == index->canBeDropped()));
    CHECK((updatedCollection.get() == &(index->collection())));
    CHECK((index->fieldNames().empty()));
    CHECK((index->fields().empty()));
    CHECK((true == index->hasBatchInsert()));
    CHECK((false == index->hasExpansion()));
    CHECK((false == index->hasSelectivityEstimate()));
    CHECK((false == index->implicitlyUnique()));
    CHECK((true == index->isPersistent()));
    CHECK((false == index->isSorted()));
    CHECK((0 < index->memory()));
    CHECK((true == index->sparse()));
    CHECK((arangodb::Index::IndexType::TRI_IDX_TYPE_IRESEARCH_LINK == index->type()));
    CHECK((arangodb::iresearch::DATA_SOURCE_TYPE.name() == index->typeName()));
    CHECK((false == index->unique()));

    arangodb::iresearch::IResearchLinkMeta expectedMeta;
    expectedMeta._trackListPositions = true;
    arangodb::iresearch::IResearchLinkMeta actualMeta;
    auto builder = index->toVelocyPack(arangodb::Index::makeFlags(arangodb::Index::Serialize::Figures));

    error.clear();
    CHECK(actualMeta.init(builder->slice(), error));
    CHECK(error.empty());
    CHECK(expectedMeta == actualMeta);
    auto const slice = builder->slice();
    CHECK(slice.hasKey("view"));
    CHECK(slice.get("view").isString());
    CHECK(view->id() == 42);
    CHECK(view->guid() == slice.get("view").copyString());
    CHECK(slice.hasKey("figures"));
    CHECK(slice.get("figures").isObject());
    CHECK(slice.get("figures").hasKey("memory"));
    CHECK(slice.get("figures").get("memory").isNumber());
    CHECK(0 < slice.get("figures").get("memory").getUInt());
  }

  // test index in testCollection3
  {
    auto updatedCollection = ci->getCollection(vocbase->name(), std::to_string(logicalCollection3->id()));
    REQUIRE(updatedCollection);
    auto link = arangodb::iresearch::IResearchLinkHelper::find(*updatedCollection, *view);
    CHECK(link);

    auto index = std::dynamic_pointer_cast<arangodb::Index>(link);
    REQUIRE((false == !index));
    CHECK((true == index->canBeDropped()));
    CHECK((updatedCollection.get() == &(index->collection())));
    CHECK((index->fieldNames().empty()));
    CHECK((index->fields().empty()));
    CHECK((true == index->hasBatchInsert()));
    CHECK((false == index->hasExpansion()));
    CHECK((false == index->hasSelectivityEstimate()));
    CHECK((false == index->implicitlyUnique()));
    CHECK((true == index->isPersistent()));
    CHECK((false == index->isSorted()));
    CHECK((0 < index->memory()));
    CHECK((true == index->sparse()));
    CHECK((arangodb::Index::IndexType::TRI_IDX_TYPE_IRESEARCH_LINK == index->type()));
    CHECK((arangodb::iresearch::DATA_SOURCE_TYPE.name() == index->typeName()));
    CHECK((false == index->unique()));

    arangodb::iresearch::IResearchLinkMeta expectedMeta;
    arangodb::iresearch::IResearchLinkMeta actualMeta;
    auto builder = index->toVelocyPack(arangodb::Index::makeFlags(arangodb::Index::Serialize::Figures));

    error.clear();
    CHECK(actualMeta.init(builder->slice(), error));
    CHECK(error.empty());
    CHECK(expectedMeta == actualMeta);
    auto const slice = builder->slice();
    CHECK(slice.hasKey("view"));
    CHECK(slice.get("view").isString());
    CHECK(view->id() == 42);
    CHECK(view->guid() == slice.get("view").copyString());
    CHECK(slice.hasKey("figures"));
    CHECK(slice.get("figures").isObject());
    CHECK(slice.get("figures").hasKey("memory"));
    CHECK(slice.get("figures").get("memory").isNumber());
    CHECK(0 < slice.get("figures").get("memory").getUInt());
  }

  CHECK(view->properties(linksJson->slice(), false).ok()); // same properties -> should not affect plan version
  CHECK(planVersion == arangodb::tests::getCurrentPlanVersion()); // plan did't change version

  CHECK(view->properties(linksJson->slice(), true).ok()); // same properties -> should not affect plan version
  CHECK(planVersion == arangodb::tests::getCurrentPlanVersion()); // plan did't change version

  // remove all links
  // simulate heartbeat thread (create index in current)
  {
    auto const value = arangodb::velocypack::Parser::fromJson("{ \"shard-id-does-not-matter\": { \"indexes\" : [ ] } }");
    CHECK(arangodb::AgencyComm().setValue(currentCollection1Path, value->slice(), 0.0).successful());
  }
  {
    auto const value = arangodb::velocypack::Parser::fromJson("{ \"shard-id-does-not-matter\": { \"indexes\" : [ ] } }");
    CHECK(arangodb::AgencyComm().setValue(currentCollection2Path, value->slice(), 0.0).successful());
  }
  {
    auto const value = arangodb::velocypack::Parser::fromJson("{ \"shard-id-does-not-matter\": { \"indexes\" : [ ] } }");
    CHECK(arangodb::AgencyComm().setValue(currentCollection3Path, value->slice(), 0.0).successful());
  }

  auto const updateJson = arangodb::velocypack::Parser::fromJson("{ \"links\": {} }");
  CHECK(view->properties(updateJson->slice(), false).ok());
  CHECK(planVersion < arangodb::tests::getCurrentPlanVersion()); // plan version changed
  planVersion = arangodb::tests::getCurrentPlanVersion();
  oldView = view;
  view = ci->getView(vocbase->name(), viewId); // get new version of the view
  CHECK(view != oldView); // different objects
  CHECK(nullptr != view);
  CHECK(nullptr != std::dynamic_pointer_cast<arangodb::iresearch::IResearchViewCoordinator>(view));
  CHECK(planVersion == view->planVersion());
  CHECK("testView" == view->name());
  CHECK(false == view->deleted());
  CHECK(42 == view->id());
  CHECK(arangodb::iresearch::DATA_SOURCE_TYPE == view->type());
  CHECK(arangodb::LogicalView::category() == view->category());
  CHECK(vocbase == &view->vocbase());

  // visit collections
  {
    CHECK(view->visitCollections([](TRI_voc_cid_t cid) mutable {
      return false;
    }));
  }

  // check properties
  {
    VPackBuilder info;
    info.openObject();
    view->properties(info, true, false);
    info.close();

    auto const properties = info.slice();
    CHECK((properties.hasKey(arangodb::iresearch::StaticStrings::LinksField)));
    auto links = properties.get(arangodb::iresearch::StaticStrings::LinksField);
    CHECK((links.isObject()));
    CHECK((0 == links.length()));
  }

  // test index in testCollection1
  {
    // get new version from plan
    auto updatedCollection = ci->getCollection(vocbase->name(), std::to_string(logicalCollection1->id()));
    REQUIRE(updatedCollection);
    auto link = arangodb::iresearch::IResearchLinkHelper::find(*updatedCollection, *view);
    CHECK(!link);
  }

  // test index in testCollection2
  {
    // get new version from plan
    auto updatedCollection = ci->getCollection(vocbase->name(), std::to_string(logicalCollection2->id()));
    REQUIRE(updatedCollection);
    auto link = arangodb::iresearch::IResearchLinkHelper::find(*updatedCollection, *view);
    CHECK(!link);
  }

  // test index in testCollection3
  {
    // get new version from plan
    auto updatedCollection = ci->getCollection(vocbase->name(), std::to_string(logicalCollection3->id()));
    REQUIRE(updatedCollection);
    auto link = arangodb::iresearch::IResearchLinkHelper::find(*updatedCollection, *view);
    CHECK(!link);
  }

  // drop view
  CHECK(view->drop().ok());
  CHECK(planVersion < arangodb::tests::getCurrentPlanVersion());

  // there is no more view
  CHECK(nullptr == ci->getView(vocbase->name(), view->name()));
}

SECTION("test_drop_link") {
  auto* database = arangodb::DatabaseFeature::DATABASE;
  REQUIRE(nullptr != database);

  auto* ci = arangodb::ClusterInfo::instance();
  REQUIRE(nullptr != ci);

  std::string error;
  TRI_vocbase_t* vocbase; // will be owned by DatabaseFeature

  // create database
  {
    // simulate heartbeat thread
    REQUIRE(TRI_ERROR_NO_ERROR == database->createDatabase(1, "testDatabase", vocbase));

    REQUIRE(nullptr != vocbase);
    CHECK("testDatabase" == vocbase->name());
    CHECK(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_COORDINATOR == vocbase->type());
    CHECK(1 == vocbase->id());

    CHECK(TRI_ERROR_NO_ERROR == ci->createDatabaseCoordinator(
      vocbase->name(), VPackSlice::emptyObjectSlice(), error, 0.0
    ));
    CHECK("no error" == error);
  }

  // create collection
  std::shared_ptr<arangodb::LogicalCollection> logicalCollection;
  {
    auto const collectionId = "1";

    auto collectionJson = arangodb::velocypack::Parser::fromJson(
      "{ \"id\": \"1\", \"planId\": \"1\",  \"name\": \"testCollection\", \"replicationFactor\": 1, \"type\": 1, \"shards\":{} }"
    );

    CHECK(TRI_ERROR_NO_ERROR == ci->createCollectionCoordinator(
      vocbase->name(), collectionId, 0, 1, false, collectionJson->slice(), error, 0.0
    ));

    logicalCollection = ci->getCollection(vocbase->name(), collectionId);
    REQUIRE((nullptr != logicalCollection));
  }

  ci->loadCurrent();

  auto const currentCollectionPath =
      "/Current/Collections/" + vocbase->name() + "/" + std::to_string(logicalCollection->id());

  // get current plan version
  auto planVersion = arangodb::tests::getCurrentPlanVersion();

  // update link
  {
    auto viewJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"id\": \"42\", \"type\": \"arangosearch\" }");
    arangodb::LogicalView::ptr logicalView;
    REQUIRE((arangodb::LogicalView::create(logicalView, *vocbase, viewJson->slice()).ok()));
    auto view = std::dynamic_pointer_cast<arangodb::iresearch::IResearchViewCoordinator>(logicalView);
    REQUIRE(view);
    auto const viewId = std::to_string(view->planId());
    CHECK("42" == viewId);

    // simulate heartbeat thread (create index in current)
    {
      auto const value = arangodb::velocypack::Parser::fromJson("{ \"shard-id-does-not-matter\": { \"indexes\" : [ { \"id\": \"1\" } ] } }");
      CHECK(arangodb::AgencyComm().setValue(currentCollectionPath, value->slice(), 0.0).successful());
    }

    auto linksJson = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection\" : { \"includeAllFields\" : true } } }");
    CHECK(view->properties(linksJson->slice(), true).ok()); // add link
    CHECK(planVersion < arangodb::tests::getCurrentPlanVersion()); // plan version changed
    planVersion = arangodb::tests::getCurrentPlanVersion();

    auto oldView = view;
    view = std::dynamic_pointer_cast<arangodb::iresearch::IResearchViewCoordinator>(ci->getView(vocbase->name(), viewId)); // get new version of the view
    CHECK(view != oldView); // different objects
    CHECK(nullptr != view);
    CHECK(nullptr != std::dynamic_pointer_cast<arangodb::iresearch::IResearchViewCoordinator>(view));
    CHECK(planVersion == view->planVersion());
    CHECK("testView" == view->name());
    CHECK(false == view->deleted());
    CHECK(42 == view->id());
    CHECK(arangodb::iresearch::DATA_SOURCE_TYPE == view->type());
    CHECK(arangodb::LogicalView::category() == view->category());
    CHECK(vocbase == &view->vocbase());

    // visit collections
    {
      std::set<TRI_voc_cid_t> expectedLinks{ logicalCollection->id() };
      CHECK(view->visitCollections([&expectedLinks](TRI_voc_cid_t cid) mutable {
        return 1 == expectedLinks.erase(cid);
      }));
      CHECK(expectedLinks.empty());
    }

    // check properties
    {
      VPackBuilder info;
      info.openObject();
      view->properties(info, true, false);
      info.close();

      auto const properties = info.slice();
      CHECK(properties.hasKey(arangodb::iresearch::StaticStrings::LinksField));
      auto const linksSlice = properties.get(arangodb::iresearch::StaticStrings::LinksField);
      CHECK(linksSlice.isObject());

      VPackObjectIterator it(linksSlice);
      CHECK(it.valid());
      CHECK(1 == it.size());
      auto const& valuePair = *it;
      auto const key = valuePair.key;
      CHECK(key.isString());
      CHECK("testCollection" == key.copyString());
      auto const value = valuePair.value;
      CHECK(value.isObject());

      arangodb::iresearch::IResearchLinkMeta expectedMeta;
      expectedMeta._includeAllFields = true;
      arangodb::iresearch::IResearchLinkMeta actualMeta;
      error.clear();
      CHECK(actualMeta.init(value, error));
      CHECK(error.empty());
      CHECK(expectedMeta == actualMeta);
    }

    TRI_idx_iid_t linkId;

    // check indexes
    {
      // get new version from plan
      auto updatedCollection = ci->getCollection(vocbase->name(), std::to_string(logicalCollection->id()));
      REQUIRE(updatedCollection);
      auto link = arangodb::iresearch::IResearchLinkHelper::find(*updatedCollection, *view);
      REQUIRE((link));
      linkId = link->id();

      auto index = std::dynamic_pointer_cast<arangodb::Index>(link);
      REQUIRE((false == !index));
      CHECK((true == index->canBeDropped()));
      CHECK((updatedCollection.get() == &(index->collection())));
      CHECK((index->fieldNames().empty()));
      CHECK((index->fields().empty()));
      CHECK((true == index->hasBatchInsert()));
      CHECK((false == index->hasExpansion()));
      CHECK((false == index->hasSelectivityEstimate()));
      CHECK((false == index->implicitlyUnique()));
      CHECK((true == index->isPersistent()));
      CHECK((false == index->isSorted()));
      CHECK((0 < index->memory()));
      CHECK((true == index->sparse()));
      CHECK((arangodb::Index::IndexType::TRI_IDX_TYPE_IRESEARCH_LINK == index->type()));
      CHECK((arangodb::iresearch::DATA_SOURCE_TYPE.name() == index->typeName()));
      CHECK((false == index->unique()));

      arangodb::iresearch::IResearchLinkMeta expectedMeta;
      expectedMeta._includeAllFields = true;
      arangodb::iresearch::IResearchLinkMeta actualMeta;
      auto builder = index->toVelocyPack(arangodb::Index::makeFlags(arangodb::Index::Serialize::Figures));

      error.clear();
      CHECK(actualMeta.init(builder->slice(), error));
      CHECK(error.empty());
      CHECK(expectedMeta == actualMeta);
      auto const slice = builder->slice();
      CHECK(slice.hasKey("view"));
      CHECK(slice.get("view").isString());
      CHECK(view->id() == 42);
      CHECK(view->guid() == slice.get("view").copyString());
      CHECK(slice.hasKey("figures"));
      CHECK(slice.get("figures").isObject());
      CHECK(slice.get("figures").hasKey("memory"));
      CHECK(slice.get("figures").get("memory").isNumber());
      CHECK(0 < slice.get("figures").get("memory").getUInt());
    }

    CHECK(view->properties(linksJson->slice(), true).ok()); // same properties -> should not affect plan version
    CHECK(planVersion == arangodb::tests::getCurrentPlanVersion()); // plan did't change version

    // simulate heartbeat thread (drop index from current)
    {
      auto const value = arangodb::velocypack::Parser::fromJson("{ \"shard-id-does-not-matter\": { \"indexes\" : [ ] } }");
      CHECK(arangodb::AgencyComm().setValue(currentCollectionPath, value->slice(), 0.0).successful());
    }

    // drop link
    CHECK((arangodb::methods::Indexes::drop(logicalCollection.get(), arangodb::velocypack::Parser::fromJson(std::to_string(linkId))->slice()).ok()));
    CHECK(planVersion < arangodb::tests::getCurrentPlanVersion()); // plan version changed
    planVersion = arangodb::tests::getCurrentPlanVersion();
    oldView = view;
    view = std::dynamic_pointer_cast<arangodb::iresearch::IResearchViewCoordinator>(ci->getView(vocbase->name(), viewId)); // get new version of the view
    CHECK(view != oldView); // different objects
    CHECK(nullptr != view);
    CHECK(nullptr != std::dynamic_pointer_cast<arangodb::iresearch::IResearchViewCoordinator>(view));
    CHECK(planVersion == view->planVersion());
    CHECK("testView" == view->name());
    CHECK(false == view->deleted());
    CHECK(42 == view->id());
    CHECK(arangodb::iresearch::DATA_SOURCE_TYPE == view->type());
    CHECK(arangodb::LogicalView::category() == view->category());
    CHECK(vocbase == &view->vocbase());

    // visit collections
    CHECK(view->visitCollections([](TRI_voc_cid_t) {
      return false;
    }));

    // check properties
    {
      VPackBuilder info;
      info.openObject();
      view->properties(info, true, false);
      info.close();

      auto const properties = info.slice();
      CHECK((properties.hasKey(arangodb::iresearch::StaticStrings::LinksField)));
      auto links = properties.get(arangodb::iresearch::StaticStrings::LinksField);
      CHECK((links.isObject()));
      CHECK((0 == links.length()));
    }

    // check indexes
    {
      // get new version from plan
      auto updatedCollection = ci->getCollection(vocbase->name(), std::to_string(logicalCollection->id()));
      REQUIRE(updatedCollection);
      auto link = arangodb::iresearch::IResearchLinkHelper::find(*updatedCollection, *view);
      CHECK(!link);
    }

    // drop view
    CHECK(view->drop().ok());
    CHECK(planVersion < arangodb::tests::getCurrentPlanVersion());

    // there is no more view
    CHECK(nullptr == ci->getView(vocbase->name(), view->name()));
  }

  // add link (collection not authorized)
  {
    auto viewCreateJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"id\": \"42\", \"type\": \"arangosearch\" }");
    auto viewUpdateJson = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection\" : { \"includeAllFields\" : true } } }");
    auto const collectionId = "1";
    auto logicalCollection1 = ci->getCollection(vocbase->name(), collectionId);
    REQUIRE((false == !logicalCollection1));
    arangodb::LogicalView::ptr logicalView;
    REQUIRE((arangodb::LogicalView::create(logicalView, *vocbase, viewCreateJson->slice()).ok()));
    REQUIRE((false == !logicalView));
    auto const viewId = std::to_string(logicalView->planId());

    CHECK((true == logicalCollection1->getIndexes().empty()));
    CHECK((true == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));

    struct ExecContext: public arangodb::ExecContext {
      ExecContext(): arangodb::ExecContext(arangodb::ExecContext::Type::Default, "", "",
                                           arangodb::auth::Level::NONE, arangodb::auth::Level::NONE) {}
    } execContext;
    auto* origExecContext = ExecContext::CURRENT;
    auto resetExecContext = irs::make_finally([origExecContext]()->void{ ExecContext::CURRENT = origExecContext; });
    ExecContext::CURRENT = &execContext;
    auto* authFeature = arangodb::AuthenticationFeature::instance();
    auto* userManager = authFeature->userManager();
    arangodb::aql::QueryRegistry queryRegistry(0); // required for UserManager::removeAllUsers()
    arangodb::auth::UserMap userMap; // empty map, no user -> no permissions
    userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database
    userManager->setQueryRegistry(&queryRegistry);
    auto resetUserManager = irs::make_finally([userManager]()->void{ userManager->removeAllUsers(); });

    CHECK((TRI_ERROR_FORBIDDEN == logicalView->properties(viewUpdateJson->slice(), false).errorNumber()));
    logicalCollection1 = ci->getCollection(vocbase->name(), collectionId);
    REQUIRE((false == !logicalCollection1));
    logicalView = ci->getView(vocbase->name(), viewId); // get new version of the view
    REQUIRE((false == !logicalView));
    CHECK((true == logicalCollection->getIndexes().empty()));
    CHECK((true == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));
  }
}

SECTION("test_update_overwrite") {
  auto* database = arangodb::DatabaseFeature::DATABASE;
  REQUIRE(nullptr != database);
  auto* ci = arangodb::ClusterInfo::instance();
  REQUIRE((nullptr != ci));
  TRI_vocbase_t* vocbase; // will be owned by DatabaseFeature
  std::string error;

  // create database
  {
    // simulate heartbeat thread
    REQUIRE((TRI_ERROR_NO_ERROR == database->createDatabase(1, "testDatabase", vocbase)));

    REQUIRE((nullptr != vocbase));
    CHECK(("testDatabase" == vocbase->name()));
    CHECK((TRI_vocbase_type_e::TRI_VOCBASE_TYPE_COORDINATOR == vocbase->type()));
    CHECK((1 == vocbase->id()));

    CHECK((TRI_ERROR_NO_ERROR == ci->createDatabaseCoordinator(
      vocbase->name(), VPackSlice::emptyObjectSlice(), error, 0.0
    )));
    CHECK(("no error" == error));
  }

  // modify meta params with links to missing collections
  {
    auto viewCreateJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"id\": \"42\", \"type\": \"arangosearch\" }");
    auto viewUpdateJson = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection\": {} } }");
    auto viewId = std::to_string(42);

    error.clear();
    CHECK((TRI_ERROR_NO_ERROR == ci->createViewCoordinator(vocbase->name(), viewId, viewCreateJson->slice(), error)));
    CHECK((error.empty()));
    auto logicalView = ci->getView(vocbase->name(), viewId);
    REQUIRE((false == !logicalView));
    auto dropLogicalView = std::shared_ptr<arangodb::ClusterInfo>(ci, [vocbase, &viewId](arangodb::ClusterInfo* ci)->void { std::string error; ci->dropViewCoordinator(vocbase->name(), viewId, error); });

    CHECK((true == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));

    arangodb::iresearch::IResearchViewMeta expectedMeta;
    expectedMeta._cleanupIntervalStep = 10;

    CHECK((TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND == logicalView->properties(viewUpdateJson->slice(), false).errorNumber()));
    logicalView = ci->getView(vocbase->name(), viewId);
    REQUIRE((false == !logicalView));
    CHECK((true == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));

    arangodb::velocypack::Builder builder;
    builder.openObject();
    logicalView->properties(builder, true, true); // 'forPersistence' to avoid auth check
    builder.close();

    auto slice = builder.slice();
    arangodb::iresearch::IResearchViewMeta meta;
    error.clear();
    CHECK((meta.init(slice, error) && expectedMeta == meta));
  }

  // modify meta params with links with invalid definition
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"id\": \"1\", \"planId\": \"1\",  \"name\": \"testCollection\", \"replicationFactor\": 1, \"type\": 1, \"shards\":{} }");
    auto viewCreateJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"id\": \"42\", \"type\": \"arangosearch\" }");
    auto viewUpdateJson = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection\": 42 } }");
    auto collectionId = std::to_string(1);
    auto viewId = std::to_string(42);

    error.clear();
    CHECK((TRI_ERROR_NO_ERROR == ci->createCollectionCoordinator(vocbase->name(), collectionId, 0, 1, false, collectionJson->slice(), error, 0.0)));
    CHECK((error.empty()));
    auto logicalCollection = ci->getCollection(vocbase->name(), collectionId);
    REQUIRE((false == !logicalCollection));
    auto dropLogicalCollection = std::shared_ptr<arangodb::ClusterInfo>(ci, [vocbase, &collectionId](arangodb::ClusterInfo* ci)->void { std::string error; ci->dropCollectionCoordinator(vocbase->name(), collectionId, error, 0); });
    error.clear();
    CHECK((TRI_ERROR_NO_ERROR == ci->createViewCoordinator(vocbase->name(), viewId, viewCreateJson->slice(), error)));
    CHECK((error.empty()));
    auto logicalView = ci->getView(vocbase->name(), viewId);
    REQUIRE((false == !logicalView));
    auto dropLogicalView = std::shared_ptr<arangodb::ClusterInfo>(ci, [vocbase, &viewId](arangodb::ClusterInfo* ci)->void { std::string error; ci->dropViewCoordinator(vocbase->name(), viewId, error); });

    CHECK((true == logicalCollection->getIndexes().empty()));
    CHECK((true == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));

    arangodb::iresearch::IResearchViewMeta expectedMeta;
    expectedMeta._cleanupIntervalStep = 10;

    CHECK((TRI_ERROR_BAD_PARAMETER == logicalView->properties(viewUpdateJson->slice(), false).errorNumber()));
    logicalView = ci->getView(vocbase->name(), viewId);
    REQUIRE((false == !logicalView));
    CHECK((true == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));

    arangodb::velocypack::Builder builder;
    builder.openObject();
    logicalView->properties(builder, true, true); // 'forPersistence' to avoid auth check
    builder.close();

    auto slice = builder.slice();
    arangodb::iresearch::IResearchViewMeta meta;
    error.clear();
    CHECK((meta.init(slice, error) && expectedMeta == meta));
  }

  // modify meta params with links (collection not authorized)
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"id\": \"1\", \"planId\": \"1\",  \"name\": \"testCollection\", \"replicationFactor\": 1, \"type\": 1, \"shards\":{} }");
    auto viewCreateJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"id\": \"42\", \"type\": \"arangosearch\" }");
    auto viewUpdateJson = arangodb::velocypack::Parser::fromJson("{ \"cleanupIntervalStep\": 62, \"links\": { \"testCollection\": {} } }");
    auto collectionId = std::to_string(1);
    auto viewId = std::to_string(42);

    error.clear();
    CHECK((TRI_ERROR_NO_ERROR == ci->createCollectionCoordinator(vocbase->name(), collectionId, 0, 1, false, collectionJson->slice(), error, 0.0)));
    CHECK((error.empty()));
    auto logicalCollection = ci->getCollection(vocbase->name(), collectionId);
    REQUIRE((false == !logicalCollection));
    auto dropLogicalCollection = std::shared_ptr<arangodb::ClusterInfo>(ci, [vocbase, &collectionId](arangodb::ClusterInfo* ci)->void { std::string error; ci->dropCollectionCoordinator(vocbase->name(), collectionId, error, 0); });
    error.clear();
    CHECK((TRI_ERROR_NO_ERROR == ci->createViewCoordinator(vocbase->name(), viewId, viewCreateJson->slice(), error)));
    CHECK((error.empty()));
    auto logicalView = ci->getView(vocbase->name(), viewId);
    REQUIRE((false == !logicalView));
    auto dropLogicalView = std::shared_ptr<arangodb::ClusterInfo>(ci, [vocbase, &viewId](arangodb::ClusterInfo* ci)->void { std::string error; ci->dropViewCoordinator(vocbase->name(), viewId, error); });

    CHECK((true == logicalCollection->getIndexes().empty()));
    CHECK((true == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));

    // initial link creation
    {
      // simulate heartbeat thread (create index in current)
      {
        auto const path = "/Current/Collections/" + vocbase->name() + "/" + std::to_string(logicalCollection->id());
        auto const value = arangodb::velocypack::Parser::fromJson("{ \"shard-id-does-not-matter\": { \"indexes\" : [ { \"id\": \"1\" } ] } }");
        CHECK(arangodb::AgencyComm().setValue(path, value->slice(), 0.0).successful());
      }

      auto updateJson = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection\": {} } }");
      CHECK((logicalView->properties(updateJson->slice(), true).ok()));
      logicalCollection = ci->getCollection(vocbase->name(), collectionId);
      REQUIRE((false == !logicalCollection));
      logicalView = ci->getView(vocbase->name(), viewId);
      REQUIRE((false == !logicalView));
      CHECK((false == logicalCollection->getIndexes().empty()));
      CHECK((false == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));
    }

    struct ExecContext: public arangodb::ExecContext {
      ExecContext(): arangodb::ExecContext(arangodb::ExecContext::Type::Default, "", "",
                                           arangodb::auth::Level::NONE, arangodb::auth::Level::NONE) {}
    } execContext;
    arangodb::ExecContextScope execContextScope(&execContext);
    auto* authFeature = arangodb::AuthenticationFeature::instance();
    auto* userManager = authFeature->userManager();
    arangodb::aql::QueryRegistry queryRegistry(0); // required for UserManager::loadFromDB()
    userManager->setQueryRegistry(&queryRegistry);
    auto resetUserManager = std::shared_ptr<arangodb::auth::UserManager>(userManager, [](arangodb::auth::UserManager* ptr)->void { ptr->removeAllUsers(); });

    // subsequent update (overwrite) not authorised (NONE collection)
    {
      arangodb::auth::UserMap userMap;
      auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
      user.grantCollection(vocbase->name(), "testCollection", arangodb::auth::Level::NONE); // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

      arangodb::iresearch::IResearchViewMeta expectedMeta;
      expectedMeta._cleanupIntervalStep = 10;

      CHECK((TRI_ERROR_FORBIDDEN == logicalView->properties(viewUpdateJson->slice(), false).errorNumber()));
      logicalView = ci->getView(vocbase->name(), viewId);
      REQUIRE((false == !logicalView));

      arangodb::velocypack::Builder builder;
      builder.openObject();
      logicalView->properties(builder, true, true); // 'forPersistence' to avoid auth check
      builder.close();

      auto slice = builder.slice();
      arangodb::iresearch::IResearchViewMeta meta;
      std::string error;
      CHECK((meta.init(slice, error) && expectedMeta == meta));
    }

    // subsequent update (overwrite) authorised (RO collection)
    {
      arangodb::auth::UserMap userMap;
      auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
      user.grantCollection(vocbase->name(), "testCollection", arangodb::auth::Level::RO); // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

      arangodb::iresearch::IResearchViewMeta expectedMeta;
      expectedMeta._cleanupIntervalStep = 62;

      CHECK((logicalView->properties(viewUpdateJson->slice(), false).ok()));
      logicalView = ci->getView(vocbase->name(), viewId);
      REQUIRE((false == !logicalView));

      arangodb::velocypack::Builder builder;
      builder.openObject();
      logicalView->properties(builder, true, true); // 'forPersistence' to avoid auth check
      builder.close();

      auto slice = builder.slice();
      arangodb::iresearch::IResearchViewMeta meta;
      std::string error;
      CHECK((meta.init(slice, error) && expectedMeta == meta));
    }
  }

  // add link (collection not authorized)
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"id\": \"1\", \"planId\": \"1\",  \"name\": \"testCollection\", \"replicationFactor\": 1, \"shards\":{} }");
    auto viewCreateJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"id\": \"42\", \"type\": \"arangosearch\" }");
    auto viewUpdateJson = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection\": {} } }");
    auto collectionId = std::to_string(1);
    auto viewId = std::to_string(42);
    std::string error;

    error.clear();
    CHECK((TRI_ERROR_NO_ERROR == ci->createCollectionCoordinator(vocbase->name(), collectionId, 0, 1, false, collectionJson->slice(), error, 0.0)));
    CHECK((error.empty()));
    auto logicalCollection = ci->getCollection(vocbase->name(), collectionId);
    REQUIRE((false == !logicalCollection));
    auto dropLogicalCollection = std::shared_ptr<arangodb::ClusterInfo>(ci, [vocbase, &collectionId](arangodb::ClusterInfo* ci)->void { std::string error; ci->dropCollectionCoordinator(vocbase->name(), collectionId, error, 0); });
    error.clear();
    CHECK((TRI_ERROR_NO_ERROR == ci->createViewCoordinator(vocbase->name(), viewId, viewCreateJson->slice(), error)));
    CHECK((error.empty()));
    auto logicalView = ci->getView(vocbase->name(), viewId);
    REQUIRE((false == !logicalView));
    auto dropLogicalView = std::shared_ptr<arangodb::ClusterInfo>(ci, [vocbase, &viewId](arangodb::ClusterInfo* ci)->void { std::string error; ci->dropViewCoordinator(vocbase->name(), viewId, error); });

    CHECK((true == logicalCollection->getIndexes().empty()));
    CHECK((true == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));

    struct ExecContext: public arangodb::ExecContext {
      ExecContext(): arangodb::ExecContext(arangodb::ExecContext::Type::Default, "", "",
                                           arangodb::auth::Level::NONE, arangodb::auth::Level::NONE) {}
    } execContext;
    auto* origExecContext = ExecContext::CURRENT;
    auto resetExecContext = irs::make_finally([origExecContext]()->void{ ExecContext::CURRENT = origExecContext; });
    ExecContext::CURRENT = &execContext;
    auto* authFeature = arangodb::AuthenticationFeature::instance();
    auto* userManager = authFeature->userManager();
    arangodb::auth::UserMap userMap; // empty map, no user -> no permissions
    userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database
    auto resetUserManager = irs::make_finally([userManager]()->void{ userManager->removeAllUsers(); });

    CHECK((TRI_ERROR_FORBIDDEN == logicalView->properties(viewUpdateJson->slice(), false).errorNumber()));
    logicalCollection = ci->getCollection(vocbase->name(), collectionId);
    REQUIRE((false == !logicalCollection));
    logicalView = ci->getView(vocbase->name(), viewId);
    REQUIRE((false == !logicalView));
    CHECK((true == logicalCollection->getIndexes().empty()));
    CHECK((true == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));
  }

  // drop link (collection not authorized)
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"id\": \"1\", \"planId\": \"1\",  \"name\": \"testCollection\", \"replicationFactor\": 1, \"type\": 1, \"shards\":{} }");
    auto viewCreateJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"id\": \"42\", \"type\": \"arangosearch\" }");
    auto viewUpdateJson = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection\": null } }");
    auto collectionId = std::to_string(1);
    auto viewId = std::to_string(42);
    std::string error;

    error.clear();
    CHECK((TRI_ERROR_NO_ERROR == ci->createCollectionCoordinator(vocbase->name(), collectionId, 0, 1, false, collectionJson->slice(), error, 0.0)));
    CHECK((error.empty()));
    auto logicalCollection = ci->getCollection(vocbase->name(), collectionId);
    REQUIRE((false == !logicalCollection));
    auto dropLogicalCollection = std::shared_ptr<arangodb::ClusterInfo>(ci, [vocbase, &collectionId](arangodb::ClusterInfo* ci)->void { std::string error; ci->dropCollectionCoordinator(vocbase->name(), collectionId, error, 0); });
    error.clear();
    CHECK((TRI_ERROR_NO_ERROR == ci->createViewCoordinator(vocbase->name(), viewId, viewCreateJson->slice(), error)));
    CHECK((error.empty()));
    auto logicalView = ci->getView(vocbase->name(), viewId);
    REQUIRE((false == !logicalView));
    auto dropLogicalView = std::shared_ptr<arangodb::ClusterInfo>(ci, [vocbase, &viewId](arangodb::ClusterInfo* ci)->void { std::string error; ci->dropViewCoordinator(vocbase->name(), viewId, error); });

    CHECK((true == logicalCollection->getIndexes().empty()));
    CHECK((true == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));

    // initial link creation
    {
      // simulate heartbeat thread (create index in current)
      {
        auto const path = "/Current/Collections/" + vocbase->name() + "/" + std::to_string(logicalCollection->id());
        auto const value = arangodb::velocypack::Parser::fromJson("{ \"shard-id-does-not-matter\": { \"indexes\" : [ { \"id\": \"2\" } ] } }");
        CHECK(arangodb::AgencyComm().setValue(path, value->slice(), 0.0).successful());
      }

      auto updateJson = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection\": {} } }");
      CHECK((logicalView->properties(updateJson->slice(), true).ok()));
      logicalCollection = ci->getCollection(vocbase->name(), collectionId);
      REQUIRE((false == !logicalCollection));
      logicalView = ci->getView(vocbase->name(), viewId);
      REQUIRE((false == !logicalView));
      CHECK((false == logicalCollection->getIndexes().empty()));
      CHECK((false == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));

      // simulate heartbeat thread (update index in current)
      {
        auto const path = "/Current/Collections/" + vocbase->name() + "/" + std::to_string(logicalCollection->id());
        auto const value = arangodb::velocypack::Parser::fromJson("{ \"shard-id-does-not-matter\": { \"indexes\" : [ { \"id\": \"3\" } ] } }");
        CHECK(arangodb::AgencyComm().setValue(path, value->slice(), 0.0).successful());
      }
    }

    struct ExecContext: public arangodb::ExecContext {
      ExecContext(): arangodb::ExecContext(arangodb::ExecContext::Type::Default, "", "",
                                           arangodb::auth::Level::NONE, arangodb::auth::Level::NONE) {}
    } execContext;
    arangodb::ExecContextScope execContextScope(&execContext);
    auto* authFeature = arangodb::AuthenticationFeature::instance();
    auto* userManager = authFeature->userManager();
    arangodb::aql::QueryRegistry queryRegistry(0); // required for UserManager::loadFromDB()
    userManager->setQueryRegistry(&queryRegistry);
    auto resetUserManager = std::shared_ptr<arangodb::auth::UserManager>(userManager, [](arangodb::auth::UserManager* ptr)->void { ptr->removeAllUsers(); });

    // subsequent update (overwrite) not authorised (NONE collection)
    {
      arangodb::auth::UserMap userMap;
      auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
      user.grantCollection(vocbase->name(), "testCollection", arangodb::auth::Level::NONE); // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

      CHECK((TRI_ERROR_FORBIDDEN == logicalView->properties(viewUpdateJson->slice(), false).errorNumber()));
      logicalCollection = ci->getCollection(vocbase->name(), collectionId);
      REQUIRE((false == !logicalCollection));
      logicalView = ci->getView(vocbase->name(), viewId);
      REQUIRE((false == !logicalView));
      CHECK((false == logicalCollection->getIndexes().empty()));
      CHECK((false == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));
    }

    // subsequent update (overwrite) authorised (RO collection)
    {
      arangodb::auth::UserMap userMap;
      auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
      user.grantCollection(vocbase->name(), "testCollection", arangodb::auth::Level::RO); // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

      CHECK((logicalView->properties(viewUpdateJson->slice(), false).ok()));
      logicalCollection = ci->getCollection(vocbase->name(), collectionId);
      REQUIRE((false == !logicalCollection));
      logicalView = ci->getView(vocbase->name(), viewId);
      REQUIRE((false == !logicalView));
      CHECK((true == logicalCollection->getIndexes().empty()));
      CHECK((true == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));
    }
  }

  // add authorised link (existing collection not authorized)
  {
    auto collection0Json = arangodb::velocypack::Parser::fromJson("{ \"id\": \"1\", \"planId\": \"1\",  \"name\": \"testCollection0\", \"replicationFactor\": 1, \"type\": 1, \"shards\":{} }");
    auto collection1Json = arangodb::velocypack::Parser::fromJson("{ \"id\": \"2\", \"planId\": \"2\",  \"name\": \"testCollection1\", \"replicationFactor\": 1, \"type\": 1, \"shards\":{} }");
    auto viewCreateJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"id\": \"42\", \"type\": \"arangosearch\" }");
    auto viewUpdateJson = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection0\": {}, \"testCollection1\": {} } }");
    auto collectionId0 = std::to_string(1);
    auto collectionId1 = std::to_string(2);
    auto viewId = std::to_string(42);
    std::string error;

    error.clear();
    CHECK((TRI_ERROR_NO_ERROR == ci->createCollectionCoordinator(vocbase->name(), collectionId0, 0, 1, false, collection0Json->slice(), error, 0.0)));
    CHECK((error.empty()));
    auto logicalCollection0 = ci->getCollection(vocbase->name(), collectionId0);
    REQUIRE((false == !logicalCollection0));
    auto dropLogicalCollection0 = std::shared_ptr<arangodb::ClusterInfo>(ci, [vocbase, &collectionId0](arangodb::ClusterInfo* ci)->void { std::string error; ci->dropCollectionCoordinator(vocbase->name(), collectionId0, error, 0); });
    error.clear();
    CHECK((TRI_ERROR_NO_ERROR == ci->createCollectionCoordinator(vocbase->name(), collectionId1, 0, 1, false, collection1Json->slice(), error, 0.0)));
    CHECK((error.empty()));
    auto logicalCollection1 = ci->getCollection(vocbase->name(), collectionId1);
    REQUIRE((false == !logicalCollection1));
    auto dropLogicalCollection1 = std::shared_ptr<arangodb::ClusterInfo>(ci, [vocbase, &collectionId1](arangodb::ClusterInfo* ci)->void { std::string error; ci->dropCollectionCoordinator(vocbase->name(), collectionId1, error, 0); });
    error.clear();
    CHECK((TRI_ERROR_NO_ERROR == ci->createViewCoordinator(vocbase->name(), viewId, viewCreateJson->slice(), error)));
    CHECK((error.empty()));
    auto logicalView = ci->getView(vocbase->name(), viewId);
    REQUIRE((false == !logicalView));
    auto dropLogicalView = std::shared_ptr<arangodb::ClusterInfo>(ci, [vocbase, &viewId](arangodb::ClusterInfo* ci)->void { std::string error; ci->dropViewCoordinator(vocbase->name(), viewId, error); });

    CHECK((true == logicalCollection0->getIndexes().empty()));
    CHECK((true == logicalCollection1->getIndexes().empty()));
    CHECK((true == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));

    // initial link creation
    {
      // simulate heartbeat thread (create index in current)
      {
        auto const path0 = "/Current/Collections/" + vocbase->name() + "/" + std::to_string(logicalCollection0->id());
        auto const path1 = "/Current/Collections/" + vocbase->name() + "/" + std::to_string(logicalCollection1->id());
        auto const value0 = arangodb::velocypack::Parser::fromJson("{ \"shard-id-does-not-matter\": { \"indexes\" : [ { \"id\": \"3\" } ] } }");
        auto const value1 = arangodb::velocypack::Parser::fromJson("{ \"shard-id-does-not-matter\": { \"indexes\" : [ { \"id\": \"4\" } ] } }");
        CHECK(arangodb::AgencyComm().setValue(path0, value0->slice(), 0.0).successful());
        CHECK(arangodb::AgencyComm().setValue(path1, value1->slice(), 0.0).successful());
      }

      auto updateJson = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection0\": {} } }");
      CHECK((logicalView->properties(updateJson->slice(), true).ok()));
      logicalCollection0 = ci->getCollection(vocbase->name(), collectionId0);
      REQUIRE((false == !logicalCollection0));
      logicalCollection1 = ci->getCollection(vocbase->name(), collectionId1);
      REQUIRE((false == !logicalCollection1));
      logicalView = ci->getView(vocbase->name(), viewId);
      REQUIRE((false == !logicalView));
      CHECK((false == logicalCollection0->getIndexes().empty()));
      CHECK((true == logicalCollection1->getIndexes().empty()));
      CHECK((false == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));
    }

    struct ExecContext: public arangodb::ExecContext {
      ExecContext(): arangodb::ExecContext(arangodb::ExecContext::Type::Default, "", "",
                                           arangodb::auth::Level::NONE, arangodb::auth::Level::NONE) {}
    } execContext;
    arangodb::ExecContextScope execContextScope(&execContext);
    auto* authFeature = arangodb::AuthenticationFeature::instance();
    auto* userManager = authFeature->userManager();
    arangodb::aql::QueryRegistry queryRegistry(0); // required for UserManager::loadFromDB()
    userManager->setQueryRegistry(&queryRegistry);
    auto resetUserManager = std::shared_ptr<arangodb::auth::UserManager>(userManager, [](arangodb::auth::UserManager* ptr)->void { ptr->removeAllUsers(); });

    // subsequent update (overwrite) not authorised (NONE collection)
    {
      arangodb::auth::UserMap userMap;
      auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
      user.grantCollection(vocbase->name(), "testCollection0", arangodb::auth::Level::NONE); // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      user.grantCollection(vocbase->name(), "testCollection1", arangodb::auth::Level::RO); // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

      CHECK((TRI_ERROR_FORBIDDEN == logicalView->properties(viewUpdateJson->slice(), false).errorNumber()));
      logicalCollection0 = ci->getCollection(vocbase->name(), collectionId0);
      REQUIRE((false == !logicalCollection0));
      logicalCollection1 = ci->getCollection(vocbase->name(), collectionId1);
      REQUIRE((false == !logicalCollection1));
      logicalView = ci->getView(vocbase->name(), viewId);
      REQUIRE((false == !logicalView));
      CHECK((false == logicalCollection0->getIndexes().empty()));
      CHECK((true == logicalCollection1->getIndexes().empty()));
      CHECK((false == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));
    }

    // subsequent update (overwrite) authorised (RO collection)
    {
      arangodb::auth::UserMap userMap;
      auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
      user.grantCollection(vocbase->name(), "testCollection0", arangodb::auth::Level::RO); // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      user.grantCollection(vocbase->name(), "testCollection1", arangodb::auth::Level::RO); // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

      CHECK((logicalView->properties(viewUpdateJson->slice(), false).ok()));
      logicalCollection0 = ci->getCollection(vocbase->name(), collectionId0);
      REQUIRE((false == !logicalCollection0));
      logicalCollection1 = ci->getCollection(vocbase->name(), collectionId1);
      REQUIRE((false == !logicalCollection1));
      logicalView = ci->getView(vocbase->name(), viewId);
      REQUIRE((false == !logicalView));
      CHECK((false == logicalCollection0->getIndexes().empty()));
      CHECK((false == logicalCollection1->getIndexes().empty()));
      CHECK((false == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));
    }
  }

  // drop authorised link (existing collection not authorized)
  {
    auto collection0Json = arangodb::velocypack::Parser::fromJson("{ \"id\": \"1\", \"planId\": \"1\",  \"name\": \"testCollection0\", \"replicationFactor\": 1, \"type\": 1, \"shards\":{} }");
    auto collection1Json = arangodb::velocypack::Parser::fromJson("{ \"id\": \"2\", \"planId\": \"2\",  \"name\": \"testCollection1\", \"replicationFactor\": 1, \"type\": 1, \"shards\":{} }");
    auto viewCreateJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"id\": \"42\", \"type\": \"arangosearch\" }");
    auto viewUpdateJson = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection0\": {} } }");
    auto collectionId0 = std::to_string(1);
    auto collectionId1 = std::to_string(2);
    auto viewId = std::to_string(42);
    std::string error;

    error.clear();
    CHECK((TRI_ERROR_NO_ERROR == ci->createCollectionCoordinator(vocbase->name(), collectionId0, 0, 1, false, collection0Json->slice(), error, 0.0)));
    CHECK((error.empty()));
    auto logicalCollection0 = ci->getCollection(vocbase->name(), collectionId0);
    REQUIRE((false == !logicalCollection0));
    auto dropLogicalCollection0 = std::shared_ptr<arangodb::ClusterInfo>(ci, [vocbase, &collectionId0](arangodb::ClusterInfo* ci)->void { std::string error; ci->dropCollectionCoordinator(vocbase->name(), collectionId0, error, 0); });
    error.clear();
    CHECK((TRI_ERROR_NO_ERROR == ci->createCollectionCoordinator(vocbase->name(), collectionId1, 0, 1, false, collection1Json->slice(), error, 0.0)));
    CHECK((error.empty()));
    auto logicalCollection1 = ci->getCollection(vocbase->name(), collectionId1);
    REQUIRE((false == !logicalCollection1));
    auto dropLogicalCollection1 = std::shared_ptr<arangodb::ClusterInfo>(ci, [vocbase, &collectionId1](arangodb::ClusterInfo* ci)->void { std::string error; ci->dropCollectionCoordinator(vocbase->name(), collectionId1, error, 0); });
    error.clear();
    CHECK((TRI_ERROR_NO_ERROR == ci->createViewCoordinator(vocbase->name(), viewId, viewCreateJson->slice(), error)));
    CHECK((error.empty()));
    auto logicalView = ci->getView(vocbase->name(), viewId);
    REQUIRE((false == !logicalView));
    auto dropLogicalView = std::shared_ptr<arangodb::ClusterInfo>(ci, [vocbase, &viewId](arangodb::ClusterInfo* ci)->void { std::string error; ci->dropViewCoordinator(vocbase->name(), viewId, error); });

    CHECK((true == logicalCollection0->getIndexes().empty()));
    CHECK((true == logicalCollection1->getIndexes().empty()));
    CHECK((true == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));

    // initial link creation
    {
      // simulate heartbeat thread (create index in current)
      {
        auto const path0 = "/Current/Collections/" + vocbase->name() + "/" + std::to_string(logicalCollection0->id());
        auto const path1 = "/Current/Collections/" + vocbase->name() + "/" + std::to_string(logicalCollection1->id());
        auto const value0 = arangodb::velocypack::Parser::fromJson("{ \"shard-id-does-not-matter\": { \"indexes\" : [ { \"id\": \"5\" } ] } }");
        auto const value1 = arangodb::velocypack::Parser::fromJson("{ \"shard-id-does-not-matter\": { \"indexes\" : [ { \"id\": \"6\" } ] } }");
        CHECK(arangodb::AgencyComm().setValue(path0, value0->slice(), 0.0).successful());
        CHECK(arangodb::AgencyComm().setValue(path1, value1->slice(), 0.0).successful());
      }

      auto updateJson = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection0\": {}, \"testCollection1\": {} } }");
      CHECK((logicalView->properties(updateJson->slice(), true).ok()));
      logicalCollection0 = ci->getCollection(vocbase->name(), collectionId0);
      REQUIRE((false == !logicalCollection0));
      logicalCollection1 = ci->getCollection(vocbase->name(), collectionId1);
      REQUIRE((false == !logicalCollection1));
      logicalView = ci->getView(vocbase->name(), viewId);
      REQUIRE((false == !logicalView));
      CHECK((false == logicalCollection0->getIndexes().empty()));
      CHECK((false == logicalCollection1->getIndexes().empty()));
      CHECK((false == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));

      // simulate heartbeat thread (remove index from current)
      {
        auto const path = "/Current/Collections/" + vocbase->name() + "/" + std::to_string(logicalCollection1->id()) + "/shard-id-does-not-matter/indexes";
        CHECK(arangodb::AgencyComm().removeValues(path, false).successful());
      }
    }

    struct ExecContext: public arangodb::ExecContext {
      ExecContext(): arangodb::ExecContext(arangodb::ExecContext::Type::Default, "", "",
                                           arangodb::auth::Level::NONE, arangodb::auth::Level::NONE) {}
    } execContext;
    arangodb::ExecContextScope execContextScope(&execContext);
    auto* authFeature = arangodb::AuthenticationFeature::instance();
    auto* userManager = authFeature->userManager();
    arangodb::aql::QueryRegistry queryRegistry(0); // required for UserManager::loadFromDB()
    userManager->setQueryRegistry(&queryRegistry);
    auto resetUserManager = std::shared_ptr<arangodb::auth::UserManager>(userManager, [](arangodb::auth::UserManager* ptr)->void { ptr->removeAllUsers(); });

    // subsequent update (overwrite) not authorised (NONE collection)
    {
      arangodb::auth::UserMap userMap;
      auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
      user.grantCollection(vocbase->name(), "testCollection0", arangodb::auth::Level::NONE); // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      user.grantCollection(vocbase->name(), "testCollection1", arangodb::auth::Level::RO); // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

      CHECK((TRI_ERROR_FORBIDDEN == logicalView->properties(viewUpdateJson->slice(), false).errorNumber()));
      logicalCollection0 = ci->getCollection(vocbase->name(), collectionId0);
      REQUIRE((false == !logicalCollection0));
      logicalCollection1 = ci->getCollection(vocbase->name(), collectionId1);
      REQUIRE((false == !logicalCollection1));
      logicalView = ci->getView(vocbase->name(), viewId);
      REQUIRE((false == !logicalView));
      CHECK((false == logicalCollection0->getIndexes().empty()));
      CHECK((false == logicalCollection1->getIndexes().empty()));
      CHECK((false == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));
    }

    // subsequent update (overwrite) authorised (RO collection)
    {
      arangodb::auth::UserMap userMap;
      auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
      user.grantCollection(vocbase->name(), "testCollection0", arangodb::auth::Level::RO); // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      user.grantCollection(vocbase->name(), "testCollection1", arangodb::auth::Level::RO); // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

      CHECK((logicalView->properties(viewUpdateJson->slice(), false).ok()));
      logicalCollection0 = ci->getCollection(vocbase->name(), collectionId0);
      REQUIRE((false == !logicalCollection0));
      logicalCollection1 = ci->getCollection(vocbase->name(), collectionId1);
      REQUIRE((false == !logicalCollection1));
      logicalView = ci->getView(vocbase->name(), viewId);
      REQUIRE((false == !logicalView));
      CHECK((false == logicalCollection0->getIndexes().empty()));
      CHECK((true == logicalCollection1->getIndexes().empty()));
      CHECK((false == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));
    }
  }
}

SECTION("test_update_partial") {
  auto* database = arangodb::DatabaseFeature::DATABASE;
  REQUIRE(nullptr != database);
  auto* ci = arangodb::ClusterInfo::instance();
  REQUIRE((nullptr != ci));
  TRI_vocbase_t* vocbase; // will be owned by DatabaseFeature
  std::string error;

  // create database
  {
    // simulate heartbeat thread
    REQUIRE((TRI_ERROR_NO_ERROR == database->createDatabase(1, "testDatabase", vocbase)));

    REQUIRE((nullptr != vocbase));
    CHECK(("testDatabase" == vocbase->name()));
    CHECK((TRI_vocbase_type_e::TRI_VOCBASE_TYPE_COORDINATOR == vocbase->type()));
    CHECK((1 == vocbase->id()));

    CHECK((TRI_ERROR_NO_ERROR == ci->createDatabaseCoordinator(
      vocbase->name(), VPackSlice::emptyObjectSlice(), error, 0.0
    )));
    CHECK(("no error" == error));
  }

  // modify meta params with links to missing collections
  {
    auto viewCreateJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"id\": \"42\", \"type\": \"arangosearch\" }");
    auto viewUpdateJson = arangodb::velocypack::Parser::fromJson("{ \"cleanupIntervalStep\": 62, \"links\": { \"testCollection\": {} } }");
    auto viewId = std::to_string(42);

    error.clear();
    CHECK((TRI_ERROR_NO_ERROR == ci->createViewCoordinator(vocbase->name(), viewId, viewCreateJson->slice(), error)));
    CHECK((error.empty()));
    auto logicalView = ci->getView(vocbase->name(), viewId);
    REQUIRE((false == !logicalView));
    auto dropLogicalView = std::shared_ptr<arangodb::ClusterInfo>(ci, [vocbase, &viewId](arangodb::ClusterInfo* ci)->void { std::string error; ci->dropViewCoordinator(vocbase->name(), viewId, error); });

    CHECK((true == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));

    arangodb::iresearch::IResearchViewMeta expectedMeta;
    expectedMeta._cleanupIntervalStep = 10;

    CHECK((TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND == logicalView->properties(viewUpdateJson->slice(), true).errorNumber()));
    logicalView = ci->getView(vocbase->name(), viewId);
    REQUIRE((false == !logicalView));
    CHECK((true == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));

    arangodb::velocypack::Builder builder;
    builder.openObject();
    logicalView->properties(builder, true, true); // 'forPersistence' to avoid auth check
    builder.close();

    auto slice = builder.slice();
    arangodb::iresearch::IResearchViewMeta meta;
    error.clear();
    CHECK((meta.init(slice, error) && expectedMeta == meta));
  }

  // modify meta params with links with invalid definition
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"id\": \"1\", \"planId\": \"1\",  \"name\": \"testCollection\", \"replicationFactor\": 1, \"type\": 1, \"shards\":{} }");
    auto viewCreateJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"id\": \"42\", \"type\": \"arangosearch\" }");
    auto viewUpdateJson = arangodb::velocypack::Parser::fromJson("{ \"cleanupIntervalStep\": 62, \"links\": { \"testCollection\": 42 } }");
    auto collectionId = std::to_string(1);
    auto viewId = std::to_string(42);

    error.clear();
    CHECK((TRI_ERROR_NO_ERROR == ci->createCollectionCoordinator(vocbase->name(), collectionId, 0, 1, false, collectionJson->slice(), error, 0.0)));
    CHECK((error.empty()));
    auto logicalCollection = ci->getCollection(vocbase->name(), collectionId);
    REQUIRE((false == !logicalCollection));
    auto dropLogicalCollection = std::shared_ptr<arangodb::ClusterInfo>(ci, [vocbase, &collectionId](arangodb::ClusterInfo* ci)->void { std::string error; ci->dropCollectionCoordinator(vocbase->name(), collectionId, error, 0); });
    error.clear();
    CHECK((TRI_ERROR_NO_ERROR == ci->createViewCoordinator(vocbase->name(), viewId, viewCreateJson->slice(), error)));
    CHECK((error.empty()));
    auto logicalView = ci->getView(vocbase->name(), viewId);
    REQUIRE((false == !logicalView));
    auto dropLogicalView = std::shared_ptr<arangodb::ClusterInfo>(ci, [vocbase, &viewId](arangodb::ClusterInfo* ci)->void { std::string error; ci->dropViewCoordinator(vocbase->name(), viewId, error); });

    CHECK((true == logicalCollection->getIndexes().empty()));
    CHECK((true == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));

    arangodb::iresearch::IResearchViewMeta expectedMeta;
    expectedMeta._cleanupIntervalStep = 10;

    CHECK((TRI_ERROR_BAD_PARAMETER == logicalView->properties(viewUpdateJson->slice(), true).errorNumber()));
    logicalView = ci->getView(vocbase->name(), viewId);
    REQUIRE((false == !logicalView));
    CHECK((true == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));

    arangodb::velocypack::Builder builder;
    builder.openObject();
    logicalView->properties(builder, true, true); // 'forPersistence' to avoid auth check
    builder.close();

    auto slice = builder.slice();
    arangodb::iresearch::IResearchViewMeta meta;
    error.clear();
    CHECK((meta.init(slice, error) && expectedMeta == meta));
  }

  // modify meta params with links (collection not authorized)
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"id\": \"1\", \"planId\": \"1\",  \"name\": \"testCollection\", \"replicationFactor\": 1, \"shards\":{} }");
    auto viewCreateJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"id\": \"42\", \"type\": \"arangosearch\" }");
    auto viewUpdateJson = arangodb::velocypack::Parser::fromJson("{ \"cleanupIntervalStep\": 62 }");
    auto collectionId = std::to_string(1);
    auto viewId = std::to_string(42);

    error.clear();
    CHECK((TRI_ERROR_NO_ERROR == ci->createCollectionCoordinator(vocbase->name(), collectionId, 0, 1, false, collectionJson->slice(), error, 0.0)));
    CHECK((error.empty()));
    auto logicalCollection = ci->getCollection(vocbase->name(), collectionId);
    REQUIRE((false == !logicalCollection));
    auto dropLogicalCollection = std::shared_ptr<arangodb::ClusterInfo>(ci, [vocbase, &collectionId](arangodb::ClusterInfo* ci)->void { std::string error; ci->dropCollectionCoordinator(vocbase->name(), collectionId, error, 0); });
    error.clear();
    CHECK((TRI_ERROR_NO_ERROR == ci->createViewCoordinator(vocbase->name(), viewId, viewCreateJson->slice(), error)));
    CHECK((error.empty()));
    auto logicalView = ci->getView(vocbase->name(), viewId);
    REQUIRE((false == !logicalView));
    auto dropLogicalView = std::shared_ptr<arangodb::ClusterInfo>(ci, [vocbase, &viewId](arangodb::ClusterInfo* ci)->void { std::string error; ci->dropViewCoordinator(vocbase->name(), viewId, error); });

    CHECK((true == logicalCollection->getIndexes().empty()));
    CHECK((true == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));

    // initial link creation
    {
      // simulate heartbeat thread (create index in current)
      {
        auto const path = "/Current/Collections/" + vocbase->name() + "/" + std::to_string(logicalCollection->id());
        auto const value = arangodb::velocypack::Parser::fromJson("{ \"shard-id-does-not-matter\": { \"indexes\" : [ { \"id\": \"1\" } ] } }");
        CHECK(arangodb::AgencyComm().setValue(path, value->slice(), 0.0).successful());
      }

      auto updateJson = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection\": {} } }");
      CHECK((logicalView->properties(updateJson->slice(), true).ok()));
      logicalCollection = ci->getCollection(vocbase->name(), collectionId);
      REQUIRE((false == !logicalCollection));
      logicalView = ci->getView(vocbase->name(), viewId);
      REQUIRE((false == !logicalView));
      CHECK((false == logicalCollection->getIndexes().empty()));
      CHECK((false == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));
    }

    struct ExecContext: public arangodb::ExecContext {
      ExecContext(): arangodb::ExecContext(arangodb::ExecContext::Type::Default, "", "",
                                           arangodb::auth::Level::NONE, arangodb::auth::Level::NONE) {}
    } execContext;
    arangodb::ExecContextScope execContextScope(&execContext);
    auto* authFeature = arangodb::AuthenticationFeature::instance();
    auto* userManager = authFeature->userManager();
    arangodb::aql::QueryRegistry queryRegistry(0); // required for UserManager::loadFromDB()
    userManager->setQueryRegistry(&queryRegistry);
    auto resetUserManager = std::shared_ptr<arangodb::auth::UserManager>(userManager, [](arangodb::auth::UserManager* ptr)->void { ptr->removeAllUsers(); });

    // subsequent update (overwrite) not authorised (NONE collection)
    {
      arangodb::auth::UserMap userMap;
      auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
      user.grantCollection(vocbase->name(), "testCollection", arangodb::auth::Level::NONE); // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

      arangodb::iresearch::IResearchViewMeta expectedMeta;
      expectedMeta._cleanupIntervalStep = 10;

      CHECK((TRI_ERROR_FORBIDDEN == logicalView->properties(viewUpdateJson->slice(), true).errorNumber()));
      logicalView = ci->getView(vocbase->name(), viewId);
      REQUIRE((false == !logicalView));

      arangodb::velocypack::Builder builder;
      builder.openObject();
      logicalView->properties(builder, true, true); // 'forPersistence' to avoid auth check
      builder.close();

      auto slice = builder.slice();
      arangodb::iresearch::IResearchViewMeta meta;
      std::string error;
      CHECK((meta.init(slice, error) && expectedMeta == meta));
    }

    // subsequent update (overwrite) authorised (RO collection)
    {
      arangodb::auth::UserMap userMap;
      auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
      user.grantCollection(vocbase->name(), "testCollection", arangodb::auth::Level::RO); // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

      arangodb::iresearch::IResearchViewMeta expectedMeta;
      expectedMeta._cleanupIntervalStep = 62;

      CHECK((logicalView->properties(viewUpdateJson->slice(), true).ok()));
      logicalView = ci->getView(vocbase->name(), viewId);
      REQUIRE((false == !logicalView));

      arangodb::velocypack::Builder builder;
      builder.openObject();
      logicalView->properties(builder, true, true); // 'forPersistence' to avoid auth check
      builder.close();

      auto slice = builder.slice();
      arangodb::iresearch::IResearchViewMeta meta;
      std::string error;
      CHECK((meta.init(slice, error) && expectedMeta == meta));
    }
  }

  // add link (collection not authorized)
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"id\": \"1\", \"planId\": \"1\",  \"name\": \"testCollection\", \"replicationFactor\": 1, \"shards\":{} }");
    auto viewCreateJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"id\": \"42\", \"type\": \"arangosearch\" }");
    auto viewUpdateJson = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection\": {} } }");
    auto collectionId = std::to_string(1);
    auto viewId = std::to_string(42);
    std::string error;

    error.clear();
    CHECK((TRI_ERROR_NO_ERROR == ci->createCollectionCoordinator(vocbase->name(), collectionId, 0, 1, false, collectionJson->slice(), error, 0.0)));
    CHECK((error.empty()));
    auto logicalCollection = ci->getCollection(vocbase->name(), collectionId);
    REQUIRE((false == !logicalCollection));
    auto dropLogicalCollection = std::shared_ptr<arangodb::ClusterInfo>(ci, [vocbase, &collectionId](arangodb::ClusterInfo* ci)->void { std::string error; ci->dropCollectionCoordinator(vocbase->name(), collectionId, error, 0); });
    error.clear();
    CHECK((TRI_ERROR_NO_ERROR == ci->createViewCoordinator(vocbase->name(), viewId, viewCreateJson->slice(), error)));
    CHECK((error.empty()));
    auto logicalView = ci->getView(vocbase->name(), viewId);
    REQUIRE((false == !logicalView));
    auto dropLogicalView = std::shared_ptr<arangodb::ClusterInfo>(ci, [vocbase, &viewId](arangodb::ClusterInfo* ci)->void { std::string error; ci->dropViewCoordinator(vocbase->name(), viewId, error); });

    CHECK((true == logicalCollection->getIndexes().empty()));
    CHECK((true == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));

    struct ExecContext: public arangodb::ExecContext {
      ExecContext(): arangodb::ExecContext(arangodb::ExecContext::Type::Default, "", "",
                                           arangodb::auth::Level::NONE, arangodb::auth::Level::NONE) {}
    } execContext;
    auto* origExecContext = ExecContext::CURRENT;
    auto resetExecContext = irs::make_finally([origExecContext]()->void{ ExecContext::CURRENT = origExecContext; });
    ExecContext::CURRENT = &execContext;
    auto* authFeature = arangodb::AuthenticationFeature::instance();
    auto* userManager = authFeature->userManager();
    arangodb::auth::UserMap userMap; // empty map, no user -> no permissions
    userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database
    auto resetUserManager = irs::make_finally([userManager]()->void{ userManager->removeAllUsers(); });

    CHECK((TRI_ERROR_FORBIDDEN == logicalView->properties(viewUpdateJson->slice(), true).errorNumber()));
    logicalCollection = ci->getCollection(vocbase->name(), collectionId);
    REQUIRE((false == !logicalCollection));
    logicalView = ci->getView(vocbase->name(), viewId);
    REQUIRE((false == !logicalView));
    CHECK((true == logicalCollection->getIndexes().empty()));
    CHECK((true == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));
  }

  // drop link (collection not authorized)
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"id\": \"1\", \"planId\": \"1\",  \"name\": \"testCollection\", \"replicationFactor\": 1, \"shards\":{} }");
    auto viewCreateJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"id\": \"42\", \"type\": \"arangosearch\" }");
    auto viewUpdateJson = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection\": null } }");
    auto collectionId = std::to_string(1);
    auto viewId = std::to_string(42);
    std::string error;

    error.clear();
    CHECK((TRI_ERROR_NO_ERROR == ci->createCollectionCoordinator(vocbase->name(), collectionId, 0, 1, false, collectionJson->slice(), error, 0.0)));
    CHECK((error.empty()));
    auto logicalCollection = ci->getCollection(vocbase->name(), collectionId);
    REQUIRE((false == !logicalCollection));
    auto dropLogicalCollection = std::shared_ptr<arangodb::ClusterInfo>(ci, [vocbase, &collectionId](arangodb::ClusterInfo* ci)->void { std::string error; ci->dropCollectionCoordinator(vocbase->name(), collectionId, error, 0); });
    error.clear();
    CHECK((TRI_ERROR_NO_ERROR == ci->createViewCoordinator(vocbase->name(), viewId, viewCreateJson->slice(), error)));
    CHECK((error.empty()));
    auto logicalView = ci->getView(vocbase->name(), viewId);
    REQUIRE((false == !logicalView));
    auto dropLogicalView = std::shared_ptr<arangodb::ClusterInfo>(ci, [vocbase, &viewId](arangodb::ClusterInfo* ci)->void { std::string error; ci->dropViewCoordinator(vocbase->name(), viewId, error); });

    CHECK((true == logicalCollection->getIndexes().empty()));
    CHECK((true == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));

    // initial link creation
    {
      // simulate heartbeat thread (create index in current)
      {
        auto const path = "/Current/Collections/" + vocbase->name() + "/" + std::to_string(logicalCollection->id());
        auto const value = arangodb::velocypack::Parser::fromJson("{ \"shard-id-does-not-matter\": { \"indexes\" : [ { \"id\": \"2\" } ] } }");
        CHECK(arangodb::AgencyComm().setValue(path, value->slice(), 0.0).successful());
      }

      auto updateJson = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection\": {} } }");
      CHECK((logicalView->properties(updateJson->slice(), true).ok()));
      logicalCollection = ci->getCollection(vocbase->name(), collectionId);
      REQUIRE((false == !logicalCollection));
      logicalView = ci->getView(vocbase->name(), viewId);
      REQUIRE((false == !logicalView));
      CHECK((false == logicalCollection->getIndexes().empty()));
      CHECK((false == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));

      // simulate heartbeat thread (remove index from current)
      {
        auto const path = "/Current/Collections/" + vocbase->name() + "/" + std::to_string(logicalCollection->id()) + "/shard-id-does-not-matter/indexes";
        CHECK(arangodb::AgencyComm().removeValues(path, false).successful());
      }
    }

    struct ExecContext: public arangodb::ExecContext {
      ExecContext(): arangodb::ExecContext(arangodb::ExecContext::Type::Default, "", "",
                                           arangodb::auth::Level::NONE, arangodb::auth::Level::NONE) {}
    } execContext;
    arangodb::ExecContextScope execContextScope(&execContext);
    auto* authFeature = arangodb::AuthenticationFeature::instance();
    auto* userManager = authFeature->userManager();
    arangodb::aql::QueryRegistry queryRegistry(0); // required for UserManager::loadFromDB()
    userManager->setQueryRegistry(&queryRegistry);
    auto resetUserManager = std::shared_ptr<arangodb::auth::UserManager>(userManager, [](arangodb::auth::UserManager* ptr)->void { ptr->removeAllUsers(); });

    // subsequent update (overwrite) not authorised (NONE collection)
    {
      arangodb::auth::UserMap userMap;
      auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
      user.grantCollection(vocbase->name(), "testCollection", arangodb::auth::Level::NONE); // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

      CHECK((TRI_ERROR_FORBIDDEN == logicalView->properties(viewUpdateJson->slice(), true).errorNumber()));
      logicalCollection = ci->getCollection(vocbase->name(), collectionId);
      REQUIRE((false == !logicalCollection));
      logicalView = ci->getView(vocbase->name(), viewId);
      REQUIRE((false == !logicalView));
      CHECK((false == logicalCollection->getIndexes().empty()));
      CHECK((false == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));
    }

    // subsequent update (overwrite) authorised (RO collection)
    {
      arangodb::auth::UserMap userMap;
      auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
      user.grantCollection(vocbase->name(), "testCollection", arangodb::auth::Level::RO); // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

      CHECK((logicalView->properties(viewUpdateJson->slice(), true).ok()));
      logicalCollection = ci->getCollection(vocbase->name(), collectionId);
      REQUIRE((false == !logicalCollection));
      logicalView = ci->getView(vocbase->name(), viewId);
      REQUIRE((false == !logicalView));
      CHECK((true == logicalCollection->getIndexes().empty()));
      CHECK((true == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));
    }
  }

  // add authorised link (existing collection not authorized)
  {
    auto collection0Json = arangodb::velocypack::Parser::fromJson("{ \"id\": \"1\", \"planId\": \"1\",  \"name\": \"testCollection0\", \"replicationFactor\": 1, \"shards\":{} }");
    auto collection1Json = arangodb::velocypack::Parser::fromJson("{ \"id\": \"2\", \"planId\": \"2\",  \"name\": \"testCollection1\", \"replicationFactor\": 1, \"shards\":{} }");
    auto viewCreateJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"id\": \"42\", \"type\": \"arangosearch\" }");
    auto viewUpdateJson = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection1\": {} } }");
    auto collectionId0 = std::to_string(1);
    auto collectionId1 = std::to_string(2);
    auto viewId = std::to_string(42);
    std::string error;

    error.clear();
    CHECK((TRI_ERROR_NO_ERROR == ci->createCollectionCoordinator(vocbase->name(), collectionId0, 0, 1, false, collection0Json->slice(), error, 0.0)));
    CHECK((error.empty()));
    auto logicalCollection0 = ci->getCollection(vocbase->name(), collectionId0);
    REQUIRE((false == !logicalCollection0));
    auto dropLogicalCollection0 = std::shared_ptr<arangodb::ClusterInfo>(ci, [vocbase, &collectionId0](arangodb::ClusterInfo* ci)->void { std::string error; ci->dropCollectionCoordinator(vocbase->name(), collectionId0, error, 0); });
    error.clear();
    CHECK((TRI_ERROR_NO_ERROR == ci->createCollectionCoordinator(vocbase->name(), collectionId1, 0, 1, false, collection1Json->slice(), error, 0.0)));
    CHECK((error.empty()));
    auto logicalCollection1 = ci->getCollection(vocbase->name(), collectionId1);
    REQUIRE((false == !logicalCollection1));
    auto dropLogicalCollection1 = std::shared_ptr<arangodb::ClusterInfo>(ci, [vocbase, &collectionId1](arangodb::ClusterInfo* ci)->void { std::string error; ci->dropCollectionCoordinator(vocbase->name(), collectionId1, error, 0); });
    error.clear();
    CHECK((TRI_ERROR_NO_ERROR == ci->createViewCoordinator(vocbase->name(), viewId, viewCreateJson->slice(), error)));
    CHECK((error.empty()));
    auto logicalView = ci->getView(vocbase->name(), viewId);
    REQUIRE((false == !logicalView));
    auto dropLogicalView = std::shared_ptr<arangodb::ClusterInfo>(ci, [vocbase, &viewId](arangodb::ClusterInfo* ci)->void { std::string error; ci->dropViewCoordinator(vocbase->name(), viewId, error); });

    CHECK((true == logicalCollection0->getIndexes().empty()));
    CHECK((true == logicalCollection1->getIndexes().empty()));
    CHECK((true == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));

    // initial link creation
    {
      // simulate heartbeat thread (create index in current)
      {
        auto const path = "/Current/Collections/" + vocbase->name() + "/" + std::to_string(logicalCollection0->id());
        auto const value = arangodb::velocypack::Parser::fromJson("{ \"shard-id-does-not-matter\": { \"indexes\" : [ { \"id\": \"3\" } ] } }");
        CHECK(arangodb::AgencyComm().setValue(path, value->slice(), 0.0).successful());
      }

      auto updateJson = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection0\": {} } }");
      CHECK((logicalView->properties(updateJson->slice(), true).ok()));
      logicalCollection0 = ci->getCollection(vocbase->name(), collectionId0);
      REQUIRE((false == !logicalCollection0));
      logicalCollection1 = ci->getCollection(vocbase->name(), collectionId1);
      REQUIRE((false == !logicalCollection1));
      logicalView = ci->getView(vocbase->name(), viewId);
      REQUIRE((false == !logicalView));
      CHECK((false == logicalCollection0->getIndexes().empty()));
      CHECK((true == logicalCollection1->getIndexes().empty()));
      CHECK((false == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));

      // simulate heartbeat thread (update index in current)
      {
        auto const path0 = "/Current/Collections/" + vocbase->name() + "/" + std::to_string(logicalCollection0->id());
        auto const path1 = "/Current/Collections/" + vocbase->name() + "/" + std::to_string(logicalCollection1->id());
        auto const value = arangodb::velocypack::Parser::fromJson("{ \"shard-id-does-not-matter\": { \"indexes\" : [ { \"id\": \"4\" } ] } }");
        CHECK(arangodb::AgencyComm().removeValues(path0, false).successful());
        CHECK(arangodb::AgencyComm().setValue(path1, value->slice(), 0.0).successful());
      }
    }

    struct ExecContext: public arangodb::ExecContext {
      ExecContext(): arangodb::ExecContext(arangodb::ExecContext::Type::Default, "", "",
                                           arangodb::auth::Level::NONE, arangodb::auth::Level::NONE) {}
    } execContext;
    arangodb::ExecContextScope execContextScope(&execContext);
    auto* authFeature = arangodb::AuthenticationFeature::instance();
    auto* userManager = authFeature->userManager();
    arangodb::aql::QueryRegistry queryRegistry(0); // required for UserManager::loadFromDB()
    userManager->setQueryRegistry(&queryRegistry);
    auto resetUserManager = std::shared_ptr<arangodb::auth::UserManager>(userManager, [](arangodb::auth::UserManager* ptr)->void { ptr->removeAllUsers(); });

    // subsequent update (overwrite) not authorised (NONE collection)
    {
      arangodb::auth::UserMap userMap;
      auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
      user.grantCollection(vocbase->name(), "testCollection0", arangodb::auth::Level::NONE); // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      user.grantCollection(vocbase->name(), "testCollection1", arangodb::auth::Level::RO); // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

      CHECK((TRI_ERROR_FORBIDDEN == logicalView->properties(viewUpdateJson->slice(), true).errorNumber()));
      logicalCollection0 = ci->getCollection(vocbase->name(), collectionId0);
      REQUIRE((false == !logicalCollection0));
      logicalCollection1 = ci->getCollection(vocbase->name(), collectionId1);
      REQUIRE((false == !logicalCollection1));
      logicalView = ci->getView(vocbase->name(), viewId);
      REQUIRE((false == !logicalView));
      CHECK((false == logicalCollection0->getIndexes().empty()));
      CHECK((true == logicalCollection1->getIndexes().empty()));
      CHECK((false == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));
    }

    // subsequent update (overwrite) authorised (RO collection)
    {
      arangodb::auth::UserMap userMap;
      auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
      user.grantCollection(vocbase->name(), "testCollection0", arangodb::auth::Level::RO); // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      user.grantCollection(vocbase->name(), "testCollection1", arangodb::auth::Level::RO); // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

      CHECK((logicalView->properties(viewUpdateJson->slice(), true).ok()));
      logicalCollection0 = ci->getCollection(vocbase->name(), collectionId0);
      REQUIRE((false == !logicalCollection0));
      logicalCollection1 = ci->getCollection(vocbase->name(), collectionId1);
      REQUIRE((false == !logicalCollection1));
      logicalView = ci->getView(vocbase->name(), viewId);
      REQUIRE((false == !logicalView));
      CHECK((false == logicalCollection0->getIndexes().empty()));
      CHECK((false == logicalCollection1->getIndexes().empty()));
      CHECK((false == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));
    }
  }

  // drop authorised link (existing collection not authorized)
  {
    auto collection0Json = arangodb::velocypack::Parser::fromJson("{ \"id\": \"1\", \"planId\": \"1\",  \"name\": \"testCollection0\", \"replicationFactor\": 1, \"shards\":{} }");
    auto collection1Json = arangodb::velocypack::Parser::fromJson("{ \"id\": \"2\", \"planId\": \"2\",  \"name\": \"testCollection1\", \"replicationFactor\": 1, \"shards\":{} }");
    auto viewCreateJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"id\": \"42\", \"type\": \"arangosearch\" }");
    auto viewUpdateJson = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection1\": null } }");
    auto collectionId0 = std::to_string(1);
    auto collectionId1 = std::to_string(2);
    auto viewId = std::to_string(42);
    std::string error;

    error.clear();
    CHECK((TRI_ERROR_NO_ERROR == ci->createCollectionCoordinator(vocbase->name(), collectionId0, 0, 1, false, collection0Json->slice(), error, 0.0)));
    CHECK((error.empty()));
    auto logicalCollection0 = ci->getCollection(vocbase->name(), collectionId0);
    REQUIRE((false == !logicalCollection0));
    auto dropLogicalCollection0 = std::shared_ptr<arangodb::ClusterInfo>(ci, [vocbase, &collectionId0](arangodb::ClusterInfo* ci)->void { std::string error; ci->dropCollectionCoordinator(vocbase->name(), collectionId0, error, 0); });
    error.clear();
    CHECK((TRI_ERROR_NO_ERROR == ci->createCollectionCoordinator(vocbase->name(), collectionId1, 0, 1, false, collection1Json->slice(), error, 0.0)));
    CHECK((error.empty()));
    auto logicalCollection1 = ci->getCollection(vocbase->name(), collectionId1);
    REQUIRE((false == !logicalCollection1));
    auto dropLogicalCollection1 = std::shared_ptr<arangodb::ClusterInfo>(ci, [vocbase, &collectionId1](arangodb::ClusterInfo* ci)->void { std::string error; ci->dropCollectionCoordinator(vocbase->name(), collectionId1, error, 0); });
    error.clear();
    CHECK((TRI_ERROR_NO_ERROR == ci->createViewCoordinator(vocbase->name(), viewId, viewCreateJson->slice(), error)));
    CHECK((error.empty()));
    auto logicalView = ci->getView(vocbase->name(), viewId);
    REQUIRE((false == !logicalView));
    auto dropLogicalView = std::shared_ptr<arangodb::ClusterInfo>(ci, [vocbase, &viewId](arangodb::ClusterInfo* ci)->void { std::string error; ci->dropViewCoordinator(vocbase->name(), viewId, error); });

    CHECK((true == logicalCollection0->getIndexes().empty()));
    CHECK((true == logicalCollection1->getIndexes().empty()));
    CHECK((true == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));

    // initial link creation
    {
      // simulate heartbeat thread (create index in current)
      {
        auto const path0 = "/Current/Collections/" + vocbase->name() + "/" + std::to_string(logicalCollection0->id());
        auto const path1 = "/Current/Collections/" + vocbase->name() + "/" + std::to_string(logicalCollection1->id());
        auto const value0 = arangodb::velocypack::Parser::fromJson("{ \"shard-id-does-not-matter\": { \"indexes\" : [ { \"id\": \"5\" } ] } }");
        auto const value1 = arangodb::velocypack::Parser::fromJson("{ \"shard-id-does-not-matter\": { \"indexes\" : [ { \"id\": \"6\" } ] } }");
        CHECK(arangodb::AgencyComm().setValue(path0, value0->slice(), 0.0).successful());
        CHECK(arangodb::AgencyComm().setValue(path1, value1->slice(), 0.0).successful());
      }

      auto updateJson = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection0\": {}, \"testCollection1\": {} } }");
      CHECK((logicalView->properties(updateJson->slice(), true).ok()));
      logicalCollection0 = ci->getCollection(vocbase->name(), collectionId0);
      REQUIRE((false == !logicalCollection0));
      logicalCollection1 = ci->getCollection(vocbase->name(), collectionId1);
      REQUIRE((false == !logicalCollection1));
      logicalView = ci->getView(vocbase->name(), viewId);
      REQUIRE((false == !logicalView));
      CHECK((false == logicalCollection0->getIndexes().empty()));
      CHECK((false == logicalCollection1->getIndexes().empty()));
      CHECK((false == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));

      // simulate heartbeat thread (remove index from current)
      {
        auto const path = "/Current/Collections/" + vocbase->name() + "/" + std::to_string(logicalCollection1->id()) + "/shard-id-does-not-matter/indexes";
        CHECK(arangodb::AgencyComm().removeValues(path, false).successful());
      }
    }

    struct ExecContext: public arangodb::ExecContext {
      ExecContext(): arangodb::ExecContext(arangodb::ExecContext::Type::Default, "", "",
                                           arangodb::auth::Level::NONE, arangodb::auth::Level::NONE) {}
    } execContext;
    arangodb::ExecContextScope execContextScope(&execContext);
    auto* authFeature = arangodb::AuthenticationFeature::instance();
    auto* userManager = authFeature->userManager();
    arangodb::aql::QueryRegistry queryRegistry(0); // required for UserManager::loadFromDB()
    userManager->setQueryRegistry(&queryRegistry);
    auto resetUserManager = std::shared_ptr<arangodb::auth::UserManager>(userManager, [](arangodb::auth::UserManager* ptr)->void { ptr->removeAllUsers(); });

    // subsequent update (overwrite) not authorised (NONE collection)
    {
      arangodb::auth::UserMap userMap;
      auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
      user.grantCollection(vocbase->name(), "testCollection0", arangodb::auth::Level::NONE); // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      user.grantCollection(vocbase->name(), "testCollection1", arangodb::auth::Level::RO); // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

      CHECK((TRI_ERROR_FORBIDDEN == logicalView->properties(viewUpdateJson->slice(), true).errorNumber()));
      logicalCollection0 = ci->getCollection(vocbase->name(), collectionId0);
      REQUIRE((false == !logicalCollection0));
      logicalCollection1 = ci->getCollection(vocbase->name(), collectionId1);
      REQUIRE((false == !logicalCollection1));
      logicalView = ci->getView(vocbase->name(), viewId);
      REQUIRE((false == !logicalView));
      CHECK((false == logicalCollection0->getIndexes().empty()));
      CHECK((false == logicalCollection1->getIndexes().empty()));
      CHECK((false == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));
    }

    // subsequent update (overwrite) authorised (RO collection)
    {
      arangodb::auth::UserMap userMap;
      auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
      user.grantCollection(vocbase->name(), "testCollection0", arangodb::auth::Level::RO); // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      user.grantCollection(vocbase->name(), "testCollection1", arangodb::auth::Level::RO); // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

      CHECK((logicalView->properties(viewUpdateJson->slice(), true).ok()));
      logicalCollection0 = ci->getCollection(vocbase->name(), collectionId0);
      REQUIRE((false == !logicalCollection0));
      logicalCollection1 = ci->getCollection(vocbase->name(), collectionId1);
      REQUIRE((false == !logicalCollection1));
      logicalView = ci->getView(vocbase->name(), viewId);
      REQUIRE((false == !logicalView));
      CHECK((false == logicalCollection0->getIndexes().empty()));
      CHECK((true == logicalCollection1->getIndexes().empty()));
      CHECK((false == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));
    }
  }
}

SECTION("IResearchViewNode::createBlock") {
  auto* database = arangodb::DatabaseFeature::DATABASE;
  REQUIRE(nullptr != database);

  auto* ci = arangodb::ClusterInfo::instance();
  REQUIRE(nullptr != ci);

  std::string error;
  TRI_vocbase_t* vocbase; // will be owned by DatabaseFeature

  // create database
  {
    // simulate heartbeat thread
    REQUIRE(TRI_ERROR_NO_ERROR == database->createDatabase(1, "testDatabase", vocbase));

    REQUIRE(nullptr != vocbase);
    CHECK("testDatabase" == vocbase->name());
    CHECK(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_COORDINATOR == vocbase->type());
    CHECK(1 == vocbase->id());

    CHECK(TRI_ERROR_NO_ERROR == ci->createDatabaseCoordinator(
      vocbase->name(), VPackSlice::emptyObjectSlice(), error, 0.0
    ));
    CHECK("no error" == error);
  }

  // create and drop view (no id specified)
  {
    auto json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
    auto viewId = std::to_string(ci->uniqid() + 1); // +1 because LogicalView creation will generate a new ID
    error.clear(); // clear error message

    CHECK(TRI_ERROR_NO_ERROR == ci->createViewCoordinator(
      vocbase->name(), viewId, json->slice(), error
    ));
    CHECK(error.empty());

    // get current plan version
    auto planVersion = arangodb::tests::getCurrentPlanVersion();

    auto view = ci->getView(vocbase->name(), viewId);
    CHECK(nullptr != view);
    CHECK(nullptr != std::dynamic_pointer_cast<arangodb::iresearch::IResearchViewCoordinator>(view));
    CHECK(planVersion == view->planVersion());
    CHECK("testView" == view->name());
    CHECK(false == view->deleted());
    CHECK(viewId == std::to_string(view->id()));
    CHECK(arangodb::iresearch::DATA_SOURCE_TYPE == view->type());
    CHECK(arangodb::LogicalView::category() == view->category());
    CHECK(vocbase == &view->vocbase());

    // dummy query
    arangodb::aql::Query query(
      false, *vocbase, arangodb::aql::QueryString("RETURN 1"),
      nullptr, arangodb::velocypack::Parser::fromJson("{}"),
      arangodb::aql::PART_MAIN
    );
    query.prepare(arangodb::QueryRegistryFeature::registry());

    arangodb::aql::Variable const outVariable("variable", 0);

    arangodb::iresearch::IResearchViewNode node(
      *query.plan(),
      42, // id
      *vocbase, // database
      view, // view
      outVariable,
      nullptr, // no filter condition
      nullptr, // no options
      {} // no sort condition
    );

    arangodb::aql::ExecutionEngine engine(&query);
    std::unordered_map<arangodb::aql::ExecutionNode*, arangodb::aql::ExecutionBlock*> cache;
    auto execBlock = node.createBlock(engine, cache);
    CHECK(nullptr != execBlock);
    CHECK(nullptr != dynamic_cast<arangodb::aql::NoResultsBlock*>(execBlock.get()));

    // drop view
    CHECK(view->drop().ok());
    CHECK(planVersion < arangodb::tests::getCurrentPlanVersion());

    // check there is no more view
    CHECK(nullptr == ci->getView(vocbase->name(), view->name()));
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate tests
////////////////////////////////////////////////////////////////////////////////

}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
