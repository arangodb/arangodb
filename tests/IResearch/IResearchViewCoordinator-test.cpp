//////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 EMC Corporation
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
/// Copyright holder is EMC Corporation
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

struct IResearchViewCoordinatorSetup {

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

  IResearchViewCoordinatorSetup(): server(nullptr, nullptr) {
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
    features.emplace_back(new arangodb::DatabaseFeature(&server), false);
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

  ~IResearchViewCoordinatorSetup() {
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

////////////////////////////////////////////////////////////////////////////////
/// @brief setup
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("IResearchViewCoordinatorTest", "[iresearch][iresearch-view-coordinator]") {
  IResearchViewCoordinatorSetup s;
  UNUSED(s);

SECTION("test_type") {
  CHECK((arangodb::LogicalDataSource::Type::emplace(arangodb::velocypack::StringRef("arangosearch")) == arangodb::iresearch::DATA_SOURCE_TYPE));
}

SECTION("visit_collections") {
  auto json = arangodb::velocypack::Parser::fromJson(
    "{ \"name\": \"testView\", \"type\": \"arangosearch\", \"id\": \"1\", \"properties\": { \"collections\": [1,2,3] } }");

  Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_COORDINATOR, 1, "testVocbase");

  auto view = arangodb::LogicalView::create(vocbase, json->slice());

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

  auto view = arangodb::LogicalView::create(vocbase, json->slice());

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

    CHECK(6 == slice.length());
    CHECK(slice.get("id").copyString() == "1");
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

    CHECK(5 == slice.length());
    CHECK(slice.get("id").copyString() == "1");
    CHECK(slice.get("name").copyString() == "testView");
    CHECK(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
    CHECK(false == slice.get("deleted").getBool());
    CHECK(slice.hasKey("planId"));
    CHECK(!slice.hasKey("properties"));
  }
}

SECTION("test_drop") {
  // FIXME implement
}

SECTION("modify_view") {
  // FIXME implement
}

}
