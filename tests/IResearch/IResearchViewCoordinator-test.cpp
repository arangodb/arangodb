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

#include "Aql/AqlFunctionFeature.h"
#include "Aql/AstNode.h"
#include "Aql/BasicBlocks.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Function.h"
#include "Aql/OptimizerRulesFeature.h"
#include "Aql/SortCondition.h"
#include "Agency/Store.h"
#include "Basics/ArangoGlobalContext.h"
#include "Basics/files.h"

#if USE_ENTERPRISE
  #include "Enterprise/Ldap/LdapFeature.h"
#endif

#include "Agency/AgencyFeature.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterComm.h"
#include "Cluster/ShardingFeature.h"
#include "IResearch/ApplicationServerHelper.h"
#include "IResearch/IResearchCommon.h"
#include "IResearch/IResearchFeature.h"
#include "IResearch/IResearchLinkCoordinator.h"
#include "IResearch/IResearchLinkHelper.h"
#include "IResearch/IResearchLinkMeta.h"
#include "IResearch/IResearchViewCoordinator.h"
#include "IResearch/IResearchViewNode.h"
#include "IResearch/SystemDatabaseFeature.h"
#include "Logger/Logger.h"
#include "Logger/LogTopic.h"
#include "Random/RandomFeature.h"
#include "RestServer/AqlFeature.h"
#include "RestServer/TraverserEngineRegistryFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/FlushFeature.h"
#include "RestServer/DatabasePathFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "RestServer/ViewTypesFeature.h"
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
  std::vector<std::pair<arangodb::application_features::ApplicationFeature*, bool>> features;
  std::string testFilesystemPath;

  IResearchViewCoordinatorSetup(): server(nullptr, nullptr) {
    auto* agencyCommManager = new AgencyCommManagerMock("arango");
    agency = agencyCommManager->addConnection<GeneralClientConnectionAgencyMock>(_agencyStore);
    arangodb::AgencyCommManager::MANAGER.reset(agencyCommManager);

    arangodb::EngineSelectorFeature::ENGINE = &engine;
    // register factories & normalizers
    auto& indexFactory = const_cast<arangodb::IndexFactory&>(engine.indexFactory());
    indexFactory.emplaceFactory(
      arangodb::iresearch::DATA_SOURCE_TYPE.name(),
      arangodb::iresearch::IResearchLinkCoordinator::make
    );
    indexFactory.emplaceNormalizer(
      arangodb::iresearch::DATA_SOURCE_TYPE.name(),
      arangodb::iresearch::IResearchLinkHelper::normalize
    );

    arangodb::tests::init();

    // suppress INFO {authentication} Authentication is turned on (system only), authentication for unix sockets is turned on
    arangodb::LogTopic::setLogLevel(arangodb::Logger::AUTHENTICATION.name(), arangodb::LogLevel::WARN);

    arangodb::AuthenticationFeature* auth{};

    // pretend we're on coordinator
    serverRoleBeforeSetup = arangodb::ServerState::instance()->getRole();
    arangodb::ServerState::instance()->setRole(arangodb::ServerState::ROLE_COORDINATOR);

    // setup required application features
    features.emplace_back(new arangodb::V8DealerFeature(&server), false);
    features.emplace_back(new arangodb::ViewTypesFeature(&server), true);
    features.emplace_back(new arangodb::QueryRegistryFeature(&server), false);
    arangodb::application_features::ApplicationServer::server->addFeature(features.back().first);
    system = std::make_unique<TRI_vocbase_t>(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_COORDINATOR, 0, TRI_VOC_SYSTEM_DATABASE);
    features.emplace_back(new arangodb::RandomFeature(&server), false); // required by AuthenticationFeature
    features.emplace_back(auth = new arangodb::AuthenticationFeature(&server), false);
    features.emplace_back(arangodb::DatabaseFeature::DATABASE = new arangodb::DatabaseFeature(&server), false);
    features.emplace_back(new arangodb::DatabasePathFeature(&server), false);
    features.emplace_back(new arangodb::TraverserEngineRegistryFeature(&server), false); // must be before AqlFeature
    features.emplace_back(new arangodb::AqlFeature(&server), true);
    features.emplace_back(new arangodb::ShardingFeature(&server), true);
    features.emplace_back(new arangodb::aql::AqlFunctionFeature(&server), true); // required for IResearchAnalyzerFeature
    features.emplace_back(new arangodb::iresearch::IResearchFeature(&server), true);
    features.emplace_back(new arangodb::iresearch::SystemDatabaseFeature(&server, system.get()), false); // required for IResearchAnalyzerFeature
    features.emplace_back(new arangodb::aql::OptimizerRulesFeature(&server), true);
    features.emplace_back(new arangodb::FlushFeature(&server), false); // do not start the thread
    features.emplace_back(new arangodb::ClusterFeature(&server), false);
    features.emplace_back(new arangodb::AgencyFeature(&server), false);
    features.emplace_back(new arangodb::iresearch::IResearchAnalyzerFeature(&server), true);

    #if USE_ENTERPRISE
      features.emplace_back(new arangodb::LdapFeature(&server), false); // required for AuthenticationFeature with USE_ENTERPRISE
    #endif

    for (auto& f : features) {
      arangodb::application_features::ApplicationServer::server->addFeature(f.first);
    }

    // suppress log messages since tests check error conditions
    arangodb::LogTopic::setLogLevel(arangodb::Logger::FIXME.name(), arangodb::LogLevel::ERR); // suppress ERROR recovery failure due to error from callback
    arangodb::LogTopic::setLogLevel(arangodb::Logger::CLUSTER.name(), arangodb::LogLevel::ERR);
    arangodb::LogTopic::setLogLevel(arangodb::iresearch::TOPIC.name(), arangodb::LogLevel::FATAL);
    irs::logger::output_le(iresearch::logger::IRL_FATAL, stderr);

    for (auto& f : features) {
      f.first->prepare();

      if (f.first->name() == "Authentication") {
        f.first->forceDisable();
      }
    }

    for (auto& f : features) {
      if (f.second) {
        f.first->start();
      }
    }

    TransactionStateMock::abortTransactionCount = 0;
    TransactionStateMock::beginTransactionCount = 0;
    TransactionStateMock::commitTransactionCount = 0;
    testFilesystemPath = (
      (irs::utf8_path()/=
      TRI_GetTempPath())/=
      (std::string("arangodb_tests.") + std::to_string(TRI_microtime()))
    ).utf8();
    auto* dbPathFeature = arangodb::application_features::ApplicationServer::getFeature<arangodb::DatabasePathFeature>("DatabasePath");
    const_cast<std::string&>(dbPathFeature->directory()) = testFilesystemPath;

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
    arangodb::ClusterInfo::cleanup(); // reset ClusterInfo::instance() before DatabaseFeature::unprepare()
    arangodb::application_features::ApplicationServer::server = nullptr;

    // destroy application features
    for (auto& f : features) {
      if (f.second) {
        f.first->stop();
      }
    }

    for (auto& f : features) {
      f.first->unprepare();
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
    "{ \"name\": \"testView\", \"type\": \"arangosearch\", \"id\": \"1\", \"properties\": { \"collections\": [1,2,3] } }");

  Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_COORDINATOR, 1, "testVocbase");

  auto view = arangodb::LogicalView::create(vocbase, json->slice(), false); // false == do not persist
  CHECK(nullptr != view);
  CHECK(nullptr != std::dynamic_pointer_cast<arangodb::iresearch::IResearchViewCoordinator>(view));
  CHECK(0 == view->planVersion());
  CHECK("testView" == view->name());
  CHECK(false == view->deleted());
  CHECK(1 == view->id());
  CHECK(arangodb::iresearch::DATA_SOURCE_TYPE == view->type());
  CHECK(arangodb::LogicalView::category() == view->category());
  CHECK(&vocbase == &view->vocbase());

  auto const res = view->rename("otherName", true);
  CHECK(res.fail());
  CHECK(TRI_ERROR_NOT_IMPLEMENTED == res.errorNumber());
}

SECTION("visit_collections") {
  auto json = arangodb::velocypack::Parser::fromJson(
    "{ \"name\": \"testView\", \"type\": \"arangosearch\", \"id\": \"1\", \"properties\": { \"collections\": [1,2,3] } }");

  Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_COORDINATOR, 1, "testVocbase");

  auto view = arangodb::LogicalView::create(vocbase, json->slice(), false); // false == do not persist

  CHECK(nullptr != view);
  CHECK(nullptr != std::dynamic_pointer_cast<arangodb::iresearch::IResearchViewCoordinator>(view));
  CHECK(0 == view->planVersion());
  CHECK("testView" == view->name());
  CHECK(false == view->deleted());
  CHECK(1 == view->id());
  CHECK(arangodb::iresearch::DATA_SOURCE_TYPE == view->type());
  CHECK(arangodb::LogicalView::category() == view->category());
  CHECK(&vocbase == &view->vocbase());

  // visit view
  TRI_voc_cid_t expectedCollections[] = {1,2,3};
  auto* begin = expectedCollections;
  CHECK(true == view->visitCollections([&begin](TRI_voc_cid_t cid) {
    return *begin++ = cid; })
  );
  CHECK(3 == (begin-expectedCollections));
}

SECTION("test_defaults") {
  auto json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\", \"id\": \"1\" }");

  // view definition with LogicalView (for persistence)
  Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_COORDINATOR, 1, "testVocbase");

  auto view = arangodb::LogicalView::create(vocbase, json->slice(), false); // false == do not persist

  CHECK(nullptr != view);
  CHECK(nullptr != std::dynamic_pointer_cast<arangodb::iresearch::IResearchViewCoordinator>(view));
  CHECK(0 == view->planVersion());
  CHECK("testView" == view->name());
  CHECK(false == view->deleted());
  CHECK(1 == view->id());
  CHECK(arangodb::iresearch::DATA_SOURCE_TYPE == view->type());
  CHECK(arangodb::LogicalView::category() == view->category());
  CHECK(&vocbase == &view->vocbase());

  // visit default view
  CHECK(true == view->visitCollections([](TRI_voc_cid_t) { return false; }));

  // +system, +properties
  {
    arangodb::iresearch::IResearchViewMeta expectedMeta;
    arangodb::velocypack::Builder builder;
    builder.openObject();
    view->toVelocyPack(builder, true, true);
    builder.close();
    auto slice = builder.slice();
    arangodb::iresearch::IResearchViewMeta meta;
    std::string error;

    CHECK(8 == slice.length());
    CHECK((slice.hasKey("globallyUniqueId") && slice.get("globallyUniqueId").isString() && false == slice.get("globallyUniqueId").copyString().empty()));
    CHECK(slice.get("id").copyString() == "1");
    CHECK((slice.hasKey("isSystem") && slice.get("isSystem").isBoolean() && false == slice.get("isSystem").getBoolean()));
    CHECK(slice.get("name").copyString() == "testView");
    CHECK(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
    CHECK(slice.hasKey("planId"));
    CHECK(false == slice.get("deleted").getBool());
    slice = slice.get("properties");
    CHECK(slice.isObject());
    CHECK((5 == slice.length()));
    CHECK((!slice.hasKey("links"))); // for persistence so no links
    CHECK((meta.init(slice, error) && expectedMeta == meta));
  }

  // -system, +properties
  {
    arangodb::iresearch::IResearchViewMeta expectedMeta;
    arangodb::velocypack::Builder builder;
    builder.openObject();
    view->toVelocyPack(builder, true, false);
    builder.close();
    auto slice = builder.slice();
    arangodb::iresearch::IResearchViewMeta meta;
    std::string error;

    CHECK(4 == slice.length());
    CHECK(slice.get("id").copyString() == "1");
    CHECK(slice.get("name").copyString() == "testView");
    CHECK(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
    CHECK(!slice.hasKey("planId"));
    CHECK(!slice.hasKey("deleted"));
    slice = slice.get("properties");
    CHECK(slice.isObject());
    CHECK((5 == slice.length()));
    CHECK((!slice.hasKey("links")));
    CHECK((meta.init(slice, error) && expectedMeta == meta));
  }

  // -system, -properties
  {
    arangodb::velocypack::Builder builder;
    builder.openObject();
    view->toVelocyPack(builder, false, false);
    builder.close();
    auto slice = builder.slice();
    arangodb::iresearch::IResearchViewMeta meta;
    std::string error;

    CHECK(3 == slice.length());
    CHECK(slice.get("id").copyString() == "1");
    CHECK(slice.get("name").copyString() == "testView");
    CHECK(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
    CHECK(!slice.hasKey("planId"));
    CHECK(!slice.hasKey("deleted"));
    CHECK(!slice.hasKey("properties"));
  }

  // +system, -properties
  {
    arangodb::velocypack::Builder builder;
    builder.openObject();
    view->toVelocyPack(builder, false, true);
    builder.close();
    auto slice = builder.slice();

    CHECK(7 == slice.length());
    CHECK((slice.hasKey("globallyUniqueId") && slice.get("globallyUniqueId").isString() && false == slice.get("globallyUniqueId").copyString().empty()));
    CHECK(slice.get("id").copyString() == "1");
    CHECK((slice.hasKey("isSystem") && slice.get("isSystem").isBoolean() && false == slice.get("isSystem").getBoolean()));
    CHECK(slice.get("name").copyString() == "testView");
    CHECK(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
    CHECK(false == slice.get("deleted").getBool());
    CHECK(slice.hasKey("planId"));
    CHECK(!slice.hasKey("properties"));
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
    REQUIRE(TRI_ERROR_NO_ERROR == database->createDatabaseCoordinator(1, "testDatabase", vocbase));

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
    CHECK(view != ci->getView(vocbase->name(), view->name())); // FIXME by some reason???

    // drop view
    CHECK(view->drop().ok());
    CHECK(planVersion < arangodb::tests::getCurrentPlanVersion());

    // check there is no more view
    CHECK(nullptr == ci->getView(vocbase->name(), view->name()));

    // drop already dropped view
    {
      auto const res = view->drop();
      CHECK(res.fail());
      //CHECK(TRI_ERROR_... == res.errorNumber()) FIXME
    }
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
    CHECK(view != ci->getView(vocbase->name(), view->name())); // FIXME by some reason???

    // drop view
    CHECK(view->drop().ok());
    CHECK(planVersion < arangodb::tests::getCurrentPlanVersion());

    // check there is no more view
    CHECK(nullptr == ci->getView(vocbase->name(), view->name()));

    // drop already dropped view
    {
      auto const res = view->drop();
      CHECK(res.fail());
      //CHECK(TRI_ERROR_... == res.errorNumber()) FIXME
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
    REQUIRE(TRI_ERROR_NO_ERROR == database->createDatabaseCoordinator(1, "testDatabase", vocbase));

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
      view->toVelocyPack(builder, true, false);
      builder.close();

      arangodb::iresearch::IResearchViewMeta meta;
      error.clear(); // clear error
      CHECK(meta.init(builder.slice().get("properties"), error));
      CHECK(error.empty());
      CHECK(meta == arangodb::iresearch::IResearchViewMeta::DEFAULT());
    }

    decltype(view) fullyUpdatedView;

    // update properties - full update
    {
      auto props = arangodb::velocypack::Parser::fromJson("{ \"threadsMaxIdle\" : 42, \"threadsMaxTotal\" : 50 }");
      CHECK(view->updateProperties(props->slice(), false, true).ok());
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
        fullyUpdatedView->toVelocyPack(builder, true, false);
        builder.close();

        arangodb::iresearch::IResearchViewMeta meta;
        arangodb::iresearch::IResearchViewMeta expected;
        expected._threadsMaxIdle = 42;
        expected._threadsMaxTotal = 50;
        error.clear(); // clear error
        CHECK(meta.init(builder.slice().get("properties"), error));
        CHECK(error.empty());
        CHECK(expected == meta);
      }

      // old object remains the same
      {
        VPackBuilder builder;
        builder.openObject();
        view->toVelocyPack(builder, true, false);
        builder.close();

        arangodb::iresearch::IResearchViewMeta meta;
        error.clear(); // clear error
        CHECK(meta.init(builder.slice().get("properties"), error));
        CHECK(error.empty());
        CHECK(meta == arangodb::iresearch::IResearchViewMeta::DEFAULT());
      }
    }

    // partially update properties
    {
      auto props = arangodb::velocypack::Parser::fromJson("{ \"threadsMaxTotal\" : 42 }");
      CHECK(fullyUpdatedView->updateProperties(props->slice(), true, true).ok());
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
        partiallyUpdatedView->toVelocyPack(builder, true, false);
        builder.close();

        arangodb::iresearch::IResearchViewMeta meta;
        arangodb::iresearch::IResearchViewMeta expected;
        expected._threadsMaxIdle = 42;
        expected._threadsMaxTotal = 42;
        error.clear(); // clear error
        CHECK(meta.init(builder.slice().get("properties"), error));
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
    REQUIRE(TRI_ERROR_NO_ERROR == database->createDatabaseCoordinator(1, "testDatabase", vocbase));

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
      "{ \"id\": \"1\", \"planId\": \"1\",  \"name\": \"testCollection1\", \"replicationFactor\": 1 }"
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
      "{ \"id\": \"2\", \"planId\": \"2\",  \"name\": \"testCollection2\", \"replicationFactor\": 1 }"
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
      "{ \"id\": \"3\", \"planId\": \"3\",  \"name\": \"testCollection3\", \"replicationFactor\": 1 }"
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
  auto view = vocbase->createView(viewJson->slice());
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
    "{ \"links\" : {"
    "  \"testCollection1\" : { \"id\": \"1\", \"includeAllFields\" : true }, "
    "  \"2\" : { \"id\": \"2\", \"trackListPositions\" : true }, "
    "  \"testCollection3\" : { \"id\": \"3\" } "
    "} }"
  );
  CHECK(view->updateProperties(linksJson->slice(), true, true).ok()); // add links
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
    view->toVelocyPack(info, true, false);
    info.close();

    auto const properties = info.slice().get("properties");
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
      auto const value = linksSlice.get(std::to_string(logicalCollection2->id()));
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
    auto link = arangodb::iresearch::IResearchLinkCoordinator::find(*updatedCollection, *view);
    CHECK(link);

    auto index = std::dynamic_pointer_cast<arangodb::Index>(link);
    REQUIRE((false == !index));
    CHECK((true == index->canBeDropped()));
    CHECK((updatedCollection.get() == index->collection()));
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
    auto builder = index->toVelocyPack(true, false);

    error.clear();
    CHECK(actualMeta.init(builder->slice(), error));
    CHECK(error.empty());
    CHECK(expectedMeta == actualMeta);
    auto const slice = builder->slice();
    CHECK(slice.hasKey("view"));
    CHECK(slice.get("view").isNumber());
    CHECK(TRI_voc_cid_t(42) == slice.get("view").getNumber<TRI_voc_cid_t>());
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
    auto link = arangodb::iresearch::IResearchLinkCoordinator::find(*updatedCollection, *view);
    CHECK(link);

    auto index = std::dynamic_pointer_cast<arangodb::Index>(link);
    REQUIRE((false == !index));
    CHECK((true == index->canBeDropped()));
    CHECK((updatedCollection.get() == index->collection()));
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
    auto builder = index->toVelocyPack(true, false);

    error.clear();
    CHECK(actualMeta.init(builder->slice(), error));
    CHECK(error.empty());
    CHECK(expectedMeta == actualMeta);
    auto const slice = builder->slice();
    CHECK(slice.hasKey("view"));
    CHECK(slice.get("view").isNumber());
    CHECK(TRI_voc_cid_t(42) == slice.get("view").getNumber<TRI_voc_cid_t>());
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
    auto link = arangodb::iresearch::IResearchLinkCoordinator::find(*updatedCollection, *view);
    CHECK(link);

    auto index = std::dynamic_pointer_cast<arangodb::Index>(link);
    REQUIRE((false == !index));
    CHECK((true == index->canBeDropped()));
    CHECK((updatedCollection.get() == index->collection()));
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
    auto builder = index->toVelocyPack(true, false);

    error.clear();
    CHECK(actualMeta.init(builder->slice(), error));
    CHECK(error.empty());
    CHECK(expectedMeta == actualMeta);
    auto const slice = builder->slice();
    CHECK(slice.hasKey("view"));
    CHECK(slice.get("view").isNumber());
    CHECK(TRI_voc_cid_t(42) == slice.get("view").getNumber<TRI_voc_cid_t>());
    CHECK(slice.hasKey("figures"));
    CHECK(slice.get("figures").isObject());
    CHECK(slice.get("figures").hasKey("memory"));
    CHECK(slice.get("figures").get("memory").isNumber());
    CHECK(0 < slice.get("figures").get("memory").getUInt());
  }

  CHECK(view->updateProperties(linksJson->slice(), true, true).ok()); // same properties -> should not affect plan version
  CHECK(planVersion == arangodb::tests::getCurrentPlanVersion()); // plan did't change version

  // remove testCollection2 link
  // simulate heartbeat thread (create index in current)
  {
    auto const value = arangodb::velocypack::Parser::fromJson("{ \"shard-id-does-not-matter\": { \"indexes\" : [ ] } }");
    CHECK(arangodb::AgencyComm().setValue(currentCollection2Path, value->slice(), 0.0).successful());
  }

  auto const updateJson = arangodb::velocypack::Parser::fromJson(
    "{ \"links\" : {"
    "  \"2\" : null "
    "} }"
  );
  CHECK(view->updateProperties(updateJson->slice(), true, true).ok());
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
    view->toVelocyPack(info, true, false);
    info.close();

    auto const properties = info.slice().get("properties");
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
    auto link = arangodb::iresearch::IResearchLinkCoordinator::find(*updatedCollection, *view);
    CHECK(link);

    auto index = std::dynamic_pointer_cast<arangodb::Index>(link);
    REQUIRE((false == !index));
    CHECK((true == index->canBeDropped()));
    CHECK((updatedCollection.get() == index->collection()));
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
    auto builder = index->toVelocyPack(true, false);

    error.clear();
    CHECK(actualMeta.init(builder->slice(), error));
    CHECK(error.empty());
    CHECK(expectedMeta == actualMeta);
    auto const slice = builder->slice();
    CHECK(slice.hasKey("view"));
    CHECK(slice.get("view").isNumber());
    CHECK(TRI_voc_cid_t(42) == slice.get("view").getNumber<TRI_voc_cid_t>());
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
    auto link = arangodb::iresearch::IResearchLinkCoordinator::find(*updatedCollection, *view);
    CHECK(link);

    auto index = std::dynamic_pointer_cast<arangodb::Index>(link);
    REQUIRE((false == !index));
    CHECK((true == index->canBeDropped()));
    CHECK((updatedCollection.get() == index->collection()));
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
    auto builder = index->toVelocyPack(true, false);

    error.clear();
    CHECK(actualMeta.init(builder->slice(), error));
    CHECK(error.empty());
    CHECK(expectedMeta == actualMeta);
    auto const slice = builder->slice();
    CHECK(slice.hasKey("view"));
    CHECK(slice.get("view").isNumber());
    CHECK(TRI_voc_cid_t(42) == slice.get("view").getNumber<TRI_voc_cid_t>());
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
    auto link = arangodb::iresearch::IResearchLinkCoordinator::find(*updatedCollection, *view);
    CHECK(!link);
  }

  {
    // get new version from plan
    auto updatedCollection = ci->getCollection(vocbase->name(), std::to_string(logicalCollection2->id()));
    REQUIRE(updatedCollection);
    auto link = arangodb::iresearch::IResearchLinkCoordinator::find(*updatedCollection, *view);
    CHECK(!link);
  }

  {
    // get new version from plan
    auto updatedCollection = ci->getCollection(vocbase->name(), std::to_string(logicalCollection3->id()));
    REQUIRE(updatedCollection);
    auto link = arangodb::iresearch::IResearchLinkCoordinator::find(*updatedCollection, *view);
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
    REQUIRE(TRI_ERROR_NO_ERROR == database->createDatabaseCoordinator(1, "testDatabase", vocbase));

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
      "{ \"id\": \"1\", \"planId\": \"1\",  \"name\": \"testCollection1\", \"replicationFactor\": 1 }"
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
      "{ \"id\": \"2\", \"planId\": \"2\",  \"name\": \"testCollection2\", \"replicationFactor\": 1 }"
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
      "{ \"id\": \"3\", \"planId\": \"3\",  \"name\": \"testCollection3\", \"replicationFactor\": 1 }"
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
  auto view = vocbase->createView(viewJson->slice());
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
    "{ \"links\" : {"
    "  \"testCollection1\" : { \"id\": \"1\", \"includeAllFields\" : true }, "
    "  \"testCollection3\" : { \"id\": \"3\" } "
    "} }"
  );
  CHECK(view->updateProperties(linksJson->slice(), true, true).ok()); // add links
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
    view->toVelocyPack(info, true, false);
    info.close();

    auto const properties = info.slice().get("properties");
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
    auto link = arangodb::iresearch::IResearchLinkCoordinator::find(*updatedCollection, *view);
    CHECK(link);

    auto index = std::dynamic_pointer_cast<arangodb::Index>(link);
    REQUIRE((false == !index));
    CHECK((true == index->canBeDropped()));
    CHECK((updatedCollection.get() == index->collection()));
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
    auto builder = index->toVelocyPack(true, false);

    error.clear();
    CHECK(actualMeta.init(builder->slice(), error));
    CHECK(error.empty());
    CHECK(expectedMeta == actualMeta);
    auto const slice = builder->slice();
    CHECK(slice.hasKey("view"));
    CHECK(slice.get("view").isNumber());
    CHECK(TRI_voc_cid_t(42) == slice.get("view").getNumber<TRI_voc_cid_t>());
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
    auto link = arangodb::iresearch::IResearchLinkCoordinator::find(*updatedCollection, *view);
    CHECK(link);

    auto index = std::dynamic_pointer_cast<arangodb::Index>(link);
    REQUIRE((false == !index));
    CHECK((true == index->canBeDropped()));
    CHECK((updatedCollection.get() == index->collection()));
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
    auto builder = index->toVelocyPack(true, false);

    error.clear();
    CHECK(actualMeta.init(builder->slice(), error));
    CHECK(error.empty());
    CHECK(expectedMeta == actualMeta);
    auto const slice = builder->slice();
    CHECK(slice.hasKey("view"));
    CHECK(slice.get("view").isNumber());
    CHECK(TRI_voc_cid_t(42) == slice.get("view").getNumber<TRI_voc_cid_t>());
    CHECK(slice.hasKey("figures"));
    CHECK(slice.get("figures").isObject());
    CHECK(slice.get("figures").hasKey("memory"));
    CHECK(slice.get("figures").get("memory").isNumber());
    CHECK(0 < slice.get("figures").get("memory").getUInt());
  }

  CHECK(view->updateProperties(linksJson->slice(), true, true).ok()); // same properties -> should not affect plan version
  CHECK(planVersion == arangodb::tests::getCurrentPlanVersion()); // plan did't change version

  // remove testCollection2 link
  // simulate heartbeat thread (create index in current)
  {
    auto const value = arangodb::velocypack::Parser::fromJson("{ \"shard-id-does-not-matter\": { \"indexes\" : [ { \"id\": \"2\" } ] } }");
    CHECK(arangodb::AgencyComm().setValue(currentCollection2Path, value->slice(), 0.0).successful());
  }

  auto const updateJson = arangodb::velocypack::Parser::fromJson(
    "{ \"links\" : {"
    "  \"2\" : { \"id\": \"2\", \"trackListPositions\" : true } "
    "} }"
  );
  CHECK(view->updateProperties(updateJson->slice(), true, true).ok());
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
    view->toVelocyPack(info, true, false);
    info.close();

    auto const properties = info.slice().get("properties");
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
      auto const value = linksSlice.get(std::to_string(logicalCollection2->id()));
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
    auto link = arangodb::iresearch::IResearchLinkCoordinator::find(*updatedCollection, *view);
    CHECK(link);

    auto index = std::dynamic_pointer_cast<arangodb::Index>(link);
    REQUIRE((false == !index));
    CHECK((true == index->canBeDropped()));
    CHECK((updatedCollection.get() == index->collection()));
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
    auto builder = index->toVelocyPack(true, false);

    error.clear();
    CHECK(actualMeta.init(builder->slice(), error));
    CHECK(error.empty());
    CHECK(expectedMeta == actualMeta);
    auto const slice = builder->slice();
    CHECK(slice.hasKey("view"));
    CHECK(slice.get("view").isNumber());
    CHECK(TRI_voc_cid_t(42) == slice.get("view").getNumber<TRI_voc_cid_t>());
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
    auto link = arangodb::iresearch::IResearchLinkCoordinator::find(*updatedCollection, *view);
    CHECK(link);

    auto index = std::dynamic_pointer_cast<arangodb::Index>(link);
    REQUIRE((false == !index));
    CHECK((true == index->canBeDropped()));
    CHECK((updatedCollection.get() == index->collection()));
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
    auto builder = index->toVelocyPack(true, false);

    error.clear();
    CHECK(actualMeta.init(builder->slice(), error));
    CHECK(error.empty());
    CHECK(expectedMeta == actualMeta);
    auto const slice = builder->slice();
    CHECK(slice.hasKey("view"));
    CHECK(slice.get("view").isNumber());
    CHECK(TRI_voc_cid_t(42) == slice.get("view").getNumber<TRI_voc_cid_t>());
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
    auto link = arangodb::iresearch::IResearchLinkCoordinator::find(*updatedCollection, *view);
    CHECK(link);

    auto index = std::dynamic_pointer_cast<arangodb::Index>(link);
    REQUIRE((false == !index));
    CHECK((true == index->canBeDropped()));
    CHECK((updatedCollection.get() == index->collection()));
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
    auto builder = index->toVelocyPack(true, false);

    error.clear();
    CHECK(actualMeta.init(builder->slice(), error));
    CHECK(error.empty());
    CHECK(expectedMeta == actualMeta);
    auto const slice = builder->slice();
    CHECK(slice.hasKey("view"));
    CHECK(slice.get("view").isNumber());
    CHECK(TRI_voc_cid_t(42) == slice.get("view").getNumber<TRI_voc_cid_t>());
    CHECK(slice.hasKey("figures"));
    CHECK(slice.get("figures").isObject());
    CHECK(slice.get("figures").hasKey("memory"));
    CHECK(slice.get("figures").get("memory").isNumber());
    CHECK(0 < slice.get("figures").get("memory").getUInt());
  }

  // partial update - empty delta
  {
    auto const updateJson = arangodb::velocypack::Parser::fromJson("{ }");
    CHECK(view->updateProperties(updateJson->slice(), true, true).ok()); // empty properties -> should not affect plan version
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
    auto link = arangodb::iresearch::IResearchLinkCoordinator::find(*updatedCollection, *view);
    CHECK(!link);
  }

  {
    // get new version from plan
    auto updatedCollection = ci->getCollection(vocbase->name(), std::to_string(logicalCollection2->id()));
    REQUIRE(updatedCollection);
    auto link = arangodb::iresearch::IResearchLinkCoordinator::find(*updatedCollection, *view);
    CHECK(!link);
  }

  {
    // get new version from plan
    auto updatedCollection = ci->getCollection(vocbase->name(), std::to_string(logicalCollection3->id()));
    REQUIRE(updatedCollection);
    auto link = arangodb::iresearch::IResearchLinkCoordinator::find(*updatedCollection, *view);
    CHECK(!link);
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
    REQUIRE(TRI_ERROR_NO_ERROR == database->createDatabaseCoordinator(1, "testDatabase", vocbase));

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
      "{ \"id\": \"1\", \"planId\": \"1\",  \"name\": \"testCollection1\", \"replicationFactor\": 1 }"
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
      "{ \"id\": \"2\", \"planId\": \"2\",  \"name\": \"testCollection2\", \"replicationFactor\": 1 }"
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
      "{ \"id\": \"3\", \"planId\": \"3\",  \"name\": \"testCollection3\", \"replicationFactor\": 1 }"
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
  auto view = vocbase->createView(viewJson->slice());
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
    "{ \"links\" : {"
    "  \"testCollection1\" : { \"id\": \"1\", \"includeAllFields\" : true }, "
    "  \"testCollection3\" : { \"id\": \"3\" } "
    "} }"
  );
  CHECK(view->updateProperties(linksJson->slice(), false, true).ok()); // add link
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
    view->toVelocyPack(info, true, false);
    info.close();

    auto const properties = info.slice().get("properties");
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
    auto link = arangodb::iresearch::IResearchLinkCoordinator::find(*updatedCollection, *view);
    CHECK(link);

    auto index = std::dynamic_pointer_cast<arangodb::Index>(link);
    REQUIRE((false == !index));
    CHECK((true == index->canBeDropped()));
    CHECK((updatedCollection.get() == index->collection()));
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
    auto builder = index->toVelocyPack(true, false);

    error.clear();
    CHECK(actualMeta.init(builder->slice(), error));
    CHECK(error.empty());
    CHECK(expectedMeta == actualMeta);
    auto const slice = builder->slice();
    CHECK(slice.hasKey("view"));
    CHECK(slice.get("view").isNumber());
    CHECK(TRI_voc_cid_t(42) == slice.get("view").getNumber<TRI_voc_cid_t>());
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
    auto link = arangodb::iresearch::IResearchLinkCoordinator::find(*updatedCollection, *view);
    CHECK(link);

    auto index = std::dynamic_pointer_cast<arangodb::Index>(link);
    REQUIRE((false == !index));
    CHECK((true == index->canBeDropped()));
    CHECK((updatedCollection.get() == index->collection()));
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
    auto builder = index->toVelocyPack(true, false);

    error.clear();
    CHECK(actualMeta.init(builder->slice(), error));
    CHECK(error.empty());
    CHECK(expectedMeta == actualMeta);
    auto const slice = builder->slice();
    CHECK(slice.hasKey("view"));
    CHECK(slice.get("view").isNumber());
    CHECK(TRI_voc_cid_t(42) == slice.get("view").getNumber<TRI_voc_cid_t>());
    CHECK(slice.hasKey("figures"));
    CHECK(slice.get("figures").isObject());
    CHECK(slice.get("figures").hasKey("memory"));
    CHECK(slice.get("figures").get("memory").isNumber());
    CHECK(0 < slice.get("figures").get("memory").getUInt());
  }

  CHECK(view->updateProperties(linksJson->slice(), false, true).ok()); // same properties -> should not affect plan version
  CHECK(planVersion == arangodb::tests::getCurrentPlanVersion()); // plan did't change version

  CHECK(view->updateProperties(linksJson->slice(), true, true).ok()); // same properties -> should not affect plan version
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
    "{ \"links\" : {"
    "  \"2\" : { \"id\": \"2\", \"trackListPositions\" : true } "
    "} }"
  );
  CHECK(view->updateProperties(updateJson->slice(), false, true).ok());
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
    view->toVelocyPack(info, true, false);
    info.close();

    auto const properties = info.slice().get("properties");
    CHECK(properties.hasKey(arangodb::iresearch::StaticStrings::LinksField));
    auto const linksSlice = properties.get(arangodb::iresearch::StaticStrings::LinksField);
    CHECK(linksSlice.isObject());

    VPackObjectIterator it(linksSlice);
    CHECK(it.valid());
    CHECK(1 == it.size());

    // testCollection2
    {
      auto const value = linksSlice.get(std::to_string(logicalCollection2->id()));
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
    auto link = arangodb::iresearch::IResearchLinkCoordinator::find(*updatedCollection, *view);
    CHECK(link);

    auto index = std::dynamic_pointer_cast<arangodb::Index>(link);
    REQUIRE((false == !index));
    CHECK((true == index->canBeDropped()));
    CHECK((updatedCollection.get() == index->collection()));
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
    auto builder = index->toVelocyPack(true, false);

    error.clear();
    CHECK(actualMeta.init(builder->slice(), error));
    CHECK(error.empty());
    CHECK(expectedMeta == actualMeta);
    auto const slice = builder->slice();
    CHECK(slice.hasKey("view"));
    CHECK(slice.get("view").isNumber());
    CHECK(TRI_voc_cid_t(42) == slice.get("view").getNumber<TRI_voc_cid_t>());
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
    "{ \"links\" : {"
    "  \"testCollection1\" : { \"id\": \"1\", \"includeAllFields\" : true }, "
    "  \"2\" : null "
    "} }"
  );
  CHECK(view->updateProperties(updateJson->slice(), false, true).ok());
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
    view->toVelocyPack(info, true, false);
    info.close();

    auto const properties = info.slice().get("properties");
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
    auto link = arangodb::iresearch::IResearchLinkCoordinator::find(*updatedCollection, *view);
    CHECK(link);

    auto index = std::dynamic_pointer_cast<arangodb::Index>(link);
    REQUIRE((false == !index));
    CHECK((true == index->canBeDropped()));
    CHECK((updatedCollection.get() == index->collection()));
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
    auto builder = index->toVelocyPack(true, false);

    error.clear();
    CHECK(actualMeta.init(builder->slice(), error));
    CHECK(error.empty());
    CHECK(expectedMeta == actualMeta);
    auto const slice = builder->slice();
    CHECK(slice.hasKey("view"));
    CHECK(slice.get("view").isNumber());
    CHECK(TRI_voc_cid_t(42) == slice.get("view").getNumber<TRI_voc_cid_t>());
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
    auto link = arangodb::iresearch::IResearchLinkCoordinator::find(*updatedCollection, *view);
    CHECK(!link);
  }

  {
    // get new version from plan
    auto updatedCollection = ci->getCollection(vocbase->name(), std::to_string(logicalCollection2->id()));
    REQUIRE(updatedCollection);
    auto link = arangodb::iresearch::IResearchLinkCoordinator::find(*updatedCollection, *view);
    CHECK(!link);
  }

  {
    // get new version from plan
    auto updatedCollection = ci->getCollection(vocbase->name(), std::to_string(logicalCollection3->id()));
    REQUIRE(updatedCollection);
    auto link = arangodb::iresearch::IResearchLinkCoordinator::find(*updatedCollection, *view);
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
    REQUIRE(TRI_ERROR_NO_ERROR == database->createDatabaseCoordinator(1, "testDatabase", vocbase));

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
      "{ \"id\": \"1\", \"planId\": \"1\",  \"name\": \"testCollection1\", \"replicationFactor\": 1 }"
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
      "{ \"id\": \"2\", \"planId\": \"2\",  \"name\": \"testCollection2\", \"replicationFactor\": 1 }"
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
      "{ \"id\": \"3\", \"planId\": \"3\",  \"name\": \"testCollection3\", \"replicationFactor\": 1 }"
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
  auto view = vocbase->createView(viewJson->slice());
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
    "{ \"links\" : {"
    "  \"testCollection1\" : { \"id\": \"1\", \"includeAllFields\" : true }, "
    "  \"2\" : { \"id\": \"2\", \"trackListPositions\" : true }, "
    "  \"testCollection3\" : { \"id\": \"3\" } "
    "} }"
  );
  CHECK(view->updateProperties(linksJson->slice(), false, true).ok()); // add link
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
    view->toVelocyPack(info, true, false);
    info.close();

    auto const properties = info.slice().get("properties");
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
      auto const value = linksSlice.get(std::to_string(logicalCollection2->id()));
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
    auto link = arangodb::iresearch::IResearchLinkCoordinator::find(*updatedCollection, *view);
    CHECK(link);

    auto index = std::dynamic_pointer_cast<arangodb::Index>(link);
    REQUIRE((false == !index));
    CHECK((true == index->canBeDropped()));
    CHECK((updatedCollection.get() == index->collection()));
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
    auto builder = index->toVelocyPack(true, false);

    error.clear();
    CHECK(actualMeta.init(builder->slice(), error));
    CHECK(error.empty());
    CHECK(expectedMeta == actualMeta);
    auto const slice = builder->slice();
    CHECK(slice.hasKey("view"));
    CHECK(slice.get("view").isNumber());
    CHECK(TRI_voc_cid_t(42) == slice.get("view").getNumber<TRI_voc_cid_t>());
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
    auto link = arangodb::iresearch::IResearchLinkCoordinator::find(*updatedCollection, *view);
    CHECK(link);

    auto index = std::dynamic_pointer_cast<arangodb::Index>(link);
    REQUIRE((false == !index));
    CHECK((true == index->canBeDropped()));
    CHECK((updatedCollection.get() == index->collection()));
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
    auto builder = index->toVelocyPack(true, false);

    error.clear();
    CHECK(actualMeta.init(builder->slice(), error));
    CHECK(error.empty());
    CHECK(expectedMeta == actualMeta);
    auto const slice = builder->slice();
    CHECK(slice.hasKey("view"));
    CHECK(slice.get("view").isNumber());
    CHECK(TRI_voc_cid_t(42) == slice.get("view").getNumber<TRI_voc_cid_t>());
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
    auto link = arangodb::iresearch::IResearchLinkCoordinator::find(*updatedCollection, *view);
    CHECK(link);

    auto index = std::dynamic_pointer_cast<arangodb::Index>(link);
    REQUIRE((false == !index));
    CHECK((true == index->canBeDropped()));
    CHECK((updatedCollection.get() == index->collection()));
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
    auto builder = index->toVelocyPack(true, false);

    error.clear();
    CHECK(actualMeta.init(builder->slice(), error));
    CHECK(error.empty());
    CHECK(expectedMeta == actualMeta);
    auto const slice = builder->slice();
    CHECK(slice.hasKey("view"));
    CHECK(slice.get("view").isNumber());
    CHECK(TRI_voc_cid_t(42) == slice.get("view").getNumber<TRI_voc_cid_t>());
    CHECK(slice.hasKey("figures"));
    CHECK(slice.get("figures").isObject());
    CHECK(slice.get("figures").hasKey("memory"));
    CHECK(slice.get("figures").get("memory").isNumber());
    CHECK(0 < slice.get("figures").get("memory").getUInt());
  }

  CHECK(view->updateProperties(linksJson->slice(), false, true).ok()); // same properties -> should not affect plan version
  CHECK(planVersion == arangodb::tests::getCurrentPlanVersion()); // plan did't change version

  CHECK(view->updateProperties(linksJson->slice(), true, true).ok()); // same properties -> should not affect plan version
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

  auto const updateJson = arangodb::velocypack::Parser::fromJson("{}");
  CHECK(view->updateProperties(updateJson->slice(), false, true).ok());
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
    view->toVelocyPack(info, true, false);
    info.close();

    auto const properties = info.slice().get("properties");
    CHECK(!properties.hasKey(arangodb::iresearch::StaticStrings::LinksField));
  }

  // test index in testCollection1
  {
    // get new version from plan
    auto updatedCollection = ci->getCollection(vocbase->name(), std::to_string(logicalCollection1->id()));
    REQUIRE(updatedCollection);
    auto link = arangodb::iresearch::IResearchLinkCoordinator::find(*updatedCollection, *view);
    CHECK(!link);
  }

  // test index in testCollection2
  {
    // get new version from plan
    auto updatedCollection = ci->getCollection(vocbase->name(), std::to_string(logicalCollection2->id()));
    REQUIRE(updatedCollection);
    auto link = arangodb::iresearch::IResearchLinkCoordinator::find(*updatedCollection, *view);
    CHECK(!link);
  }

  // test index in testCollection3
  {
    // get new version from plan
    auto updatedCollection = ci->getCollection(vocbase->name(), std::to_string(logicalCollection3->id()));
    REQUIRE(updatedCollection);
    auto link = arangodb::iresearch::IResearchLinkCoordinator::find(*updatedCollection, *view);
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
    REQUIRE(TRI_ERROR_NO_ERROR == database->createDatabaseCoordinator(1, "testDatabase", vocbase));

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
      "{ \"id\": \"1\", \"planId\": \"1\",  \"name\": \"testCollection\", \"replicationFactor\": 1 }"
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
    auto view = std::dynamic_pointer_cast<arangodb::iresearch::IResearchViewCoordinator>(vocbase->createView(viewJson->slice()));
    REQUIRE(view);
    auto const viewId = std::to_string(view->planId());
    CHECK("42" == viewId);
    CHECK(TRI_ERROR_BAD_PARAMETER == view->drop(logicalCollection->id()).errorNumber()); // unable to drop nonexistent link

    // simulate heartbeat thread (create index in current)
    {
      auto const value = arangodb::velocypack::Parser::fromJson("{ \"shard-id-does-not-matter\": { \"indexes\" : [ { \"id\": \"1\" } ] } }");
      CHECK(arangodb::AgencyComm().setValue(currentCollectionPath, value->slice(), 0.0).successful());
    }

    auto linksJson = arangodb::velocypack::Parser::fromJson("{ \"links\" : { \"testCollection\" : { \"includeAllFields\" : true } } }");
    CHECK(view->updateProperties(linksJson->slice(), true, true).ok()); // add link
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
      view->toVelocyPack(info, true, false);
      info.close();

      auto const properties = info.slice().get("properties");
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

    // check indexes
    {
      // get new version from plan
      auto updatedCollection = ci->getCollection(vocbase->name(), std::to_string(logicalCollection->id()));
      REQUIRE(updatedCollection);
      auto link = arangodb::iresearch::IResearchLinkCoordinator::find(*updatedCollection, *view);
      CHECK(link);

      auto index = std::dynamic_pointer_cast<arangodb::Index>(link);
      REQUIRE((false == !index));
      CHECK((true == index->canBeDropped()));
      CHECK((updatedCollection.get() == index->collection()));
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
      auto builder = index->toVelocyPack(true, false);

      error.clear();
      CHECK(actualMeta.init(builder->slice(), error));
      CHECK(error.empty());
      CHECK(expectedMeta == actualMeta);
      auto const slice = builder->slice();
      CHECK(slice.hasKey("view"));
      CHECK(slice.get("view").isNumber());
      CHECK(TRI_voc_cid_t(42) == slice.get("view").getNumber<TRI_voc_cid_t>());
      CHECK(slice.hasKey("figures"));
      CHECK(slice.get("figures").isObject());
      CHECK(slice.get("figures").hasKey("memory"));
      CHECK(slice.get("figures").get("memory").isNumber());
      CHECK(0 < slice.get("figures").get("memory").getUInt());
    }

    CHECK(view->updateProperties(linksJson->slice(), true, true).ok()); // same properties -> should not affect plan version
    CHECK(planVersion == arangodb::tests::getCurrentPlanVersion()); // plan did't change version

    // simulate heartbeat thread (drop index from current)
    {
      auto const value = arangodb::velocypack::Parser::fromJson("{ \"shard-id-does-not-matter\": { \"indexes\" : [ ] } }");
      CHECK(arangodb::AgencyComm().setValue(currentCollectionPath, value->slice(), 0.0).successful());
    }

    // drop link
    CHECK(view->drop(logicalCollection->id()).ok());
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
      view->toVelocyPack(info, true, false);
      info.close();

      auto const properties = info.slice().get("properties");
      CHECK(!properties.hasKey(arangodb::iresearch::StaticStrings::LinksField));
    }

    // check indexes
    {
      // get new version from plan
      auto updatedCollection = ci->getCollection(vocbase->name(), std::to_string(logicalCollection->id()));
      REQUIRE(updatedCollection);
      auto link = arangodb::iresearch::IResearchLinkCoordinator::find(*updatedCollection, *view);
      CHECK(!link);
    }

    // drop view
    CHECK(view->drop().ok());
    CHECK(planVersion < arangodb::tests::getCurrentPlanVersion());

    // there is no more view
    CHECK(nullptr == ci->getView(vocbase->name(), view->name()));
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
    REQUIRE(TRI_ERROR_NO_ERROR == database->createDatabaseCoordinator(1, "testDatabase", vocbase));

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
    query.prepare(arangodb::QueryRegistryFeature::QUERY_REGISTRY, 42);

    arangodb::aql::Variable const outVariable("variable", 0);

    arangodb::iresearch::IResearchViewNode node(
      *query.plan(),
      42, // id
      *vocbase, // database
      view, // view
      outVariable,
      nullptr, // no filter condition
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