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
#include "Aql/ExecutionPlan.h"
#include "Aql/AstNode.h"
#include "Aql/Function.h"
#include "Aql/SortCondition.h"
#include "Basics/ArangoGlobalContext.h"
#include "Basics/files.h"

#if USE_ENTERPRISE
  #include "Enterprise/Ldap/LdapFeature.h"
#endif

#include "GeneralServer/AuthenticationFeature.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterComm.h"
#include "Agency/AgencyFeature.h"
#include "Agency/Store.h"
#include "IResearch/ApplicationServerHelper.h"
#include "IResearch/IResearchCommon.h"
#include "IResearch/IResearchFeature.h"
#include "IResearch/IResearchViewCoordinator.h"
#include "IResearch/IResearchLinkCoordinator.h"
#include "IResearch/IResearchLinkHelper.h"
#include "IResearch/SystemDatabaseFeature.h"
#include "Logger/Logger.h"
#include "Logger/LogTopic.h"
#include "Random/RandomFeature.h"
#include "RestServer/AqlFeature.h"
#include "RestServer/EndpointFeature.h"
#include "Scheduler/SchedulerFeature.h"
#include "RestServer/TraverserEngineRegistryFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/FlushFeature.h"
#include "RestServer/DatabasePathFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "RestServer/ViewTypesFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "Utils/OperationOptions.h"
#include "Utils/SingleCollectionTransaction.h"
#include "velocypack/Iterator.h"
#include "velocypack/Parser.h"
#include "V8Server/V8DealerFeature.h"
#include "VocBase/KeyGenerator.h"
#include "VocBase/Methods/Collections.h"
#include "VocBase/Methods/Indexes.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ManagedDocumentResult.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

struct IResearchLinkCoordinatorSetup {
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

  IResearchLinkCoordinatorSetup(): server(nullptr, nullptr) {
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
    features.emplace_back(new arangodb::aql::AqlFunctionFeature(&server), true); // required for IResearchAnalyzerFeature
    features.emplace_back(new arangodb::iresearch::IResearchFeature(&server), true);
    features.emplace_back(new arangodb::iresearch::SystemDatabaseFeature(&server, system.get()), false); // required for IResearchAnalyzerFeature
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
    arangodb::LogTopic::setLogLevel(arangodb::Logger::CLUSTER.name(), arangodb::LogLevel::FATAL);
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

  ~IResearchLinkCoordinatorSetup() {
    system.reset(); // destroy before reseting the 'ENGINE'
    TRI_RemoveDirectory(testFilesystemPath.c_str());
    arangodb::LogTopic::setLogLevel(arangodb::iresearch::TOPIC.name(), arangodb::LogLevel::DEFAULT);
    arangodb::LogTopic::setLogLevel(arangodb::Logger::CLUSTER.name(), arangodb::LogLevel::DEFAULT);
    arangodb::LogTopic::setLogLevel(arangodb::Logger::FIXME.name(), arangodb::LogLevel::DEFAULT);
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

TEST_CASE("IResearchLinkCoordinatorTest", "[iresearch][iresearch-link]") {
  IResearchLinkCoordinatorSetup s;
  UNUSED(s);

SECTION("test_create_drop") {
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
      "{ \"name\": \"testCollection\", \"replicationFactor\":1 }"
    );

    CHECK(TRI_ERROR_NO_ERROR == ci->createCollectionCoordinator(
      vocbase->name(), collectionId, 0, 1, false, collectionJson->slice(), error, 0.0
    ));

    logicalCollection = ci->getCollection(vocbase->name(), collectionId);
    REQUIRE((nullptr != logicalCollection));
  }

  ci->loadCurrent();

  // no view specified
  {
    auto json = arangodb::velocypack::Parser::fromJson("{}");
    auto link = arangodb::iresearch::IResearchLinkCoordinator::make(
      logicalCollection.get(), json->slice(), 1, true
    );
    CHECK(!link);
  }

  // no view can be found
  {
    auto json = arangodb::velocypack::Parser::fromJson("{ \"view\": 42 }");
    auto link = arangodb::iresearch::IResearchLinkCoordinator::make(
      logicalCollection.get(), json->slice(), 1, true
    );
    CHECK(!link);
  }

  auto const currentCollectionPath =
      "/Current/Collections/" + vocbase->name() + "/" + std::to_string(logicalCollection->id());

  // valid link creation
  {
    auto linkJson = arangodb::velocypack::Parser::fromJson("{ \"id\" : \"42\", \"type\": \"arangosearch\", \"view\": 42 }");
    auto viewJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"id\": \"42\", \"type\": \"arangosearch\" }");
    auto logicalView = vocbase->createView(viewJson->slice());
    REQUIRE(logicalView);
    auto const viewId = std::to_string(logicalView->planId());
    CHECK("42" == viewId);

    // simulate heartbeat thread (create index in current)
    {
      auto const value = arangodb::velocypack::Parser::fromJson("{ \"shard-id\": { \"indexes\" : [ { \"id\": \"42\" } ] } }");
      CHECK(arangodb::AgencyComm().setValue(currentCollectionPath, value->slice(), 0.0).successful());
    }

    // unable to create index without timeout
    VPackBuilder outputDefinition;
    CHECK(arangodb::methods::Indexes::ensureIndex(
      logicalCollection.get(), linkJson->slice(), true, outputDefinition
    ).ok());

    // get new version from plan
    auto updatedCollection = ci->getCollection(vocbase->name(), std::to_string(logicalCollection->id()));
    REQUIRE(updatedCollection);
    auto link = arangodb::iresearch::IResearchLinkCoordinator::find(*updatedCollection, *logicalView);
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

    arangodb::iresearch::IResearchLinkMeta actualMeta;
    arangodb::iresearch::IResearchLinkMeta expectedMeta;
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

    // simulate heartbeat thread (drop index from current)
    {
      auto const value = arangodb::velocypack::Parser::fromJson("{ \"shard-id\": { \"indexes\" : [ ] } }");
      CHECK(arangodb::AgencyComm().setValue(currentCollectionPath, value->slice(), 0.0).successful());
    }

    auto const indexArg = arangodb::velocypack::Parser::fromJson("{\"id\": \"42\"}");
    CHECK(arangodb::methods::Indexes::drop(logicalCollection.get(), indexArg->slice()).ok());

    // get new version from plan
    updatedCollection = ci->getCollection(vocbase->name(), std::to_string(logicalCollection->id()));
    REQUIRE(updatedCollection);
    CHECK(!arangodb::iresearch::IResearchLinkCoordinator::find(*updatedCollection, *logicalView));

    // drop view
    CHECK(vocbase->dropView(logicalView->planId(), false).ok());
    CHECK(nullptr == ci->getView(vocbase->name(), viewId));

    // old index remains valid
    {
      arangodb::iresearch::IResearchLinkMeta actualMeta;
      arangodb::iresearch::IResearchLinkMeta expectedMeta;
      auto builder = index->toVelocyPack(true, false);
      std::string error;

      CHECK((actualMeta.init(builder->slice(), error) && expectedMeta == actualMeta));
      auto slice = builder->slice();
      CHECK(error.empty());
      CHECK((
        slice.hasKey("view")
        && slice.get("view").isNumber()
        && TRI_voc_cid_t(42) == slice.get("view").getNumber<TRI_voc_cid_t>()
        && slice.hasKey("figures")
        && slice.get("figures").isObject()
        && slice.get("figures").hasKey("memory")
        && slice.get("figures").get("memory").isNumber()
        && 0 < slice.get("figures").get("memory").getUInt()
      ));
    }
  }

  // ensure jSON is still valid after unload()
  {
    auto linkJson = arangodb::velocypack::Parser::fromJson("{ \"id\":\"42\", \"type\": \"arangosearch\", \"view\": 42 }");
    auto viewJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"id\": \"42\", \"type\": \"arangosearch\" }");
    auto logicalView = vocbase->createView(viewJson->slice());
    REQUIRE(logicalView);
    auto const viewId = std::to_string(logicalView->planId());
    CHECK("42" == viewId);

    // simulate heartbeat thread (create index in current)
    {
      auto const value = arangodb::velocypack::Parser::fromJson("{ \"shard-id\": { \"indexes\" : [ { \"id\": \"42\" } ] } }");
      CHECK(arangodb::AgencyComm().setValue(currentCollectionPath, value->slice(), 0.0).successful());
    }

    // unable to create index without timeout
    VPackBuilder outputDefinition;
    CHECK(arangodb::methods::Indexes::ensureIndex(
      logicalCollection.get(),
      linkJson->slice(),
      true,
      outputDefinition
    ).ok());

    // get new version from plan
    auto updatedCollection = ci->getCollection(vocbase->name(), std::to_string(logicalCollection->id()));
    REQUIRE(updatedCollection);
    auto link = arangodb::iresearch::IResearchLinkCoordinator::find(*updatedCollection, *logicalView);
    CHECK(link);

    auto index = std::dynamic_pointer_cast<arangodb::Index>(link);
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

    {
      arangodb::iresearch::IResearchLinkMeta actualMeta;
      arangodb::iresearch::IResearchLinkMeta expectedMeta;
      auto builder = index->toVelocyPack(true, false);
      std::string error;

      CHECK((actualMeta.init(builder->slice(), error) && expectedMeta == actualMeta));
      auto slice = builder->slice();
      CHECK((
        slice.hasKey("view")
        && slice.get("view").isNumber()
        && TRI_voc_cid_t(42) == slice.get("view").getNumber<TRI_voc_cid_t>()
        && slice.hasKey("figures")
        && slice.get("figures").isObject()
        && slice.get("figures").hasKey("memory")
        && slice.get("figures").get("memory").isNumber()
        && 0 < slice.get("figures").get("memory").getUInt()
      ));
    }

    // ensure jSON is still valid after unload()
    {
      index->unload();
      auto builder = index->toVelocyPack(true, false);
      auto slice = builder->slice();
      CHECK((
        slice.hasKey("view")
        && slice.get("view").isNumber()
        && TRI_voc_cid_t(42) == slice.get("view").getNumber<TRI_voc_cid_t>()
        && slice.hasKey("figures")
        && slice.get("figures").isObject()
        && slice.get("figures").hasKey("memory")
        && slice.get("figures").get("memory").isNumber()
        && 0 < slice.get("figures").get("memory").getUInt()
      ));
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate tests
////////////////////////////////////////////////////////////////////////////////

}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
