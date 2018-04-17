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
#include "StorageEngineMock.h"

#include "utils/utf8_path.hpp"
#include "utils/log.hpp"

#include "ApplicationFeatures/JemallocFeature.h"
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
#include "IResearch/ApplicationServerHelper.h"
#include "IResearch/IResearchCommon.h"
#include "IResearch/IResearchFeature.h"
#include "IResearch/IResearchViewCoordinator.h"
#include "IResearch/IResearchLinkCoordinator.h"
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
#include "Transaction/StandaloneContext.h"
#include "Transaction/UserTransaction.h"
#include "Utils/OperationOptions.h"
#include "Utils/SingleCollectionTransaction.h"
#include "velocypack/Iterator.h"
#include "velocypack/Parser.h"
#include "V8Server/V8DealerFeature.h"
#include "VocBase/KeyGenerator.h"
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

  StorageEngineMock engine;
  arangodb::application_features::ApplicationServer server;
  std::unique_ptr<TRI_vocbase_t> system;
  std::vector<std::pair<arangodb::application_features::ApplicationFeature*, bool>> features;
  std::string testFilesystemPath;

  IResearchLinkCoordinatorSetup(): server(nullptr, nullptr) {
    arangodb::EngineSelectorFeature::ENGINE = &engine;

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
    features.emplace_back(new arangodb::DatabaseFeature(&server), true);
    features.emplace_back(new arangodb::DatabasePathFeature(&server), false);
    features.emplace_back(new arangodb::JemallocFeature(&server), false); // required for DatabasePathFeature
    features.emplace_back(new arangodb::TraverserEngineRegistryFeature(&server), false); // must be before AqlFeature
    features.emplace_back(new arangodb::AqlFeature(&server), true);
    features.emplace_back(new arangodb::aql::AqlFunctionFeature(&server), true); // required for IResearchAnalyzerFeature
    features.emplace_back(new arangodb::iresearch::IResearchFeature(&server), true);
    features.emplace_back(new arangodb::iresearch::SystemDatabaseFeature(&server, system.get()), false); // required for IResearchAnalyzerFeature
    features.emplace_back(new arangodb::FlushFeature(&server), false); // do not start the thread
    features.emplace_back(new arangodb::ClusterFeature(&server), false);
    features.emplace_back(new arangodb::AgencyFeature(&server), false);

    #if USE_ENTERPRISE
      features.emplace_back(new arangodb::LdapFeature(&server), false); // required for AuthenticationFeature with USE_ENTERPRISE
    #endif

    for (auto& f : features) {
      arangodb::application_features::ApplicationServer::server->addFeature(f.first);
    }

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

    // suppress log messages since tests check error conditions
    arangodb::LogTopic::setLogLevel(arangodb::Logger::FIXME.name(), arangodb::LogLevel::ERR); // suppress ERROR recovery failure due to error from callback
    arangodb::LogTopic::setLogLevel(arangodb::iresearch::TOPIC.name(), arangodb::LogLevel::FATAL);
    irs::logger::output_le(iresearch::logger::IRL_FATAL, stderr);
  }

  ~IResearchLinkCoordinatorSetup() {
    system.reset(); // destroy before reseting the 'ENGINE'
    TRI_RemoveDirectory(testFilesystemPath.c_str());
    arangodb::LogTopic::setLogLevel(arangodb::iresearch::TOPIC.name(), arangodb::LogLevel::DEFAULT);
    arangodb::LogTopic::setLogLevel(arangodb::Logger::FIXME.name(), arangodb::LogLevel::DEFAULT);
    arangodb::application_features::ApplicationServer::server = nullptr;
    arangodb::EngineSelectorFeature::ENGINE = nullptr;

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
  }

  arangodb::ServerState::RoleEnum serverRoleBeforeSetup;
};

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

TEST_CASE("IResearchLinkCoordinatorTest", "[iresearch][iresearch-link]") {
  IResearchLinkCoordinatorSetup s;
  UNUSED(s);

SECTION("test_defaults") {
  // no view specified
  {
    s.engine.views.clear();
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
    auto* logicalCollection = vocbase.createCollection(collectionJson->slice());
    REQUIRE((nullptr != logicalCollection));
    auto json = arangodb::velocypack::Parser::fromJson("{}");
    auto link = arangodb::iresearch::IResearchLinkCoordinator::createLinkMMFiles(logicalCollection, json->slice(), 1, true);
    CHECK((true == !link));
  }

  // no view can be found
  {
    s.engine.views.clear();
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
    auto* logicalCollection = vocbase.createCollection(collectionJson->slice());
    REQUIRE((nullptr != logicalCollection));
    auto json = arangodb::velocypack::Parser::fromJson("{ \"view\": 42 }");
    auto link = arangodb::iresearch::IResearchLinkCoordinator::createLinkMMFiles(logicalCollection, json->slice(), 1, true);
    CHECK((true == !link));
  }

  // valid link creation
  {
    s.engine.views.clear();
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto linkJson = arangodb::velocypack::Parser::fromJson("{ \"type\": \"arangosearch\", \"view\": 42 }");
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
    auto viewJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"id\": 42, \"type\": \"arangosearch\" }");
    auto* logicalCollection = vocbase.createCollection(collectionJson->slice());
    REQUIRE((nullptr != logicalCollection));
    auto logicalView = vocbase.createView(viewJson->slice());
    REQUIRE((false == !logicalView));

    bool created;
    auto link = logicalCollection->createIndex(nullptr, linkJson->slice(), created);
    REQUIRE((false == !link && created));
    CHECK((true == link->allowExpansion()));
    CHECK((true == link->canBeDropped()));
    CHECK((logicalCollection == link->collection()));
    CHECK((link->fieldNames().empty()));
    CHECK((link->fields().empty()));
    CHECK((true == link->hasBatchInsert()));
    CHECK((false == link->hasExpansion()));
    CHECK((false == link->hasSelectivityEstimate()));
    CHECK((false == link->implicitlyUnique()));
    CHECK((true == link->isPersistent()));
    CHECK((false == link->isSorted()));
    CHECK((0 < link->memory()));
    CHECK((true == link->sparse()));
    CHECK((arangodb::Index::IndexType::TRI_IDX_TYPE_IRESEARCH_LINK == link->type()));
    CHECK((arangodb::iresearch::DATA_SOURCE_TYPE.name() == link->typeName()));
    CHECK((false == link->unique()));

    arangodb::iresearch::IResearchLinkMeta actualMeta;
    arangodb::iresearch::IResearchLinkMeta expectedMeta;
    auto builder = link->toVelocyPack(true, false);
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

    CHECK((logicalCollection->dropIndex(link->id()) && logicalCollection->getIndexes().empty()));
  }

  // ensure jSON is still valid after unload()
  {
    s.engine.views.clear();
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto linkJson = arangodb::velocypack::Parser::fromJson("{ \"type\": \"arangosearch\", \"view\": 42 }");
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
    auto viewJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"id\": 42, \"type\": \"arangosearch\" }");
    auto* logicalCollection = vocbase.createCollection(collectionJson->slice());
    REQUIRE((nullptr != logicalCollection));
    auto logicalView = vocbase.createView(viewJson->slice());
    REQUIRE((false == !logicalView));

    bool created;
    auto link = logicalCollection->createIndex(nullptr, linkJson->slice(), created);
    REQUIRE((false == !link && created));
    CHECK((true == link->allowExpansion()));
    CHECK((true == link->canBeDropped()));
    CHECK((logicalCollection == link->collection()));
    CHECK((link->fieldNames().empty()));
    CHECK((link->fields().empty()));
    CHECK((true == link->hasBatchInsert()));
    CHECK((false == link->hasExpansion()));
    CHECK((false == link->hasSelectivityEstimate()));
    CHECK((false == link->implicitlyUnique()));
    CHECK((true == link->isPersistent()));
    CHECK((false == link->isSorted()));
    CHECK((0 < link->memory()));
    CHECK((true == link->sparse()));
    CHECK((arangodb::Index::IndexType::TRI_IDX_TYPE_IRESEARCH_LINK == link->type()));
    CHECK((arangodb::iresearch::DATA_SOURCE_TYPE.name() == link->typeName()));
    CHECK((false == link->unique()));

    {
      arangodb::iresearch::IResearchLinkMeta actualMeta;
      arangodb::iresearch::IResearchLinkMeta expectedMeta;
      auto builder = link->toVelocyPack(true, false);
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
      link->unload();
      auto builder = link->toVelocyPack(true, false);
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

}
