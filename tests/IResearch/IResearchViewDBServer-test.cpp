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
#include "../IResearch/AgencyCommManagerMock.h"
#include "../IResearch/StorageEngineMock.h"
#include "Aql/AstNode.h"
#include "Aql/Variable.h"
#include "Basics/files.h"
#include "Cluster/ClusterInfo.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "IResearch/IResearchCommon.h"
#include "IResearch/IResearchFeature.h"
#include "IResearch/IResearchLinkMeta.h"
#include "IResearch/IResearchView.h"
#include "IResearch/IResearchViewDBServer.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/DatabasePathFeature.h"
#include "RestServer/FlushFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "RestServer/ViewTypesFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "Transaction/Methods.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/OperationOptions.h"
#include "velocypack/Parser.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ManagedDocumentResult.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

struct IResearchViewDBServerSetup {
  GeneralClientConnectionMapMock* agency;
  StorageEngineMock engine;
  arangodb::application_features::ApplicationServer server;
  std::vector<std::pair<arangodb::application_features::ApplicationFeature*, bool>> features;
  std::string testFilesystemPath;

  IResearchViewDBServerSetup(): server(nullptr, nullptr) {
    auto* agencyCommManager = new AgencyCommManagerMock();

    agency = agencyCommManager->addConnection<GeneralClientConnectionMapMock>();
    arangodb::ServerState::instance()->setRole(arangodb::ServerState::RoleEnum::ROLE_PRIMARY);
    arangodb::EngineSelectorFeature::ENGINE = &engine;
    arangodb::AgencyCommManager::MANAGER.reset(agencyCommManager);

    // suppress INFO {authentication} Authentication is turned on (system only), authentication for unix sockets is turned on
    arangodb::LogTopic::setLogLevel(arangodb::Logger::AUTHENTICATION.name(), arangodb::LogLevel::WARN);

    // suppress log messages since tests check error conditions
    arangodb::LogTopic::setLogLevel(arangodb::iresearch::TOPIC.name(), arangodb::LogLevel::FATAL);
    irs::logger::output_le(iresearch::logger::IRL_FATAL, stderr);

    // setup required application features
    features.emplace_back(new arangodb::AuthenticationFeature(&server), false); // required for AgencyComm::send(...)
    features.emplace_back(new arangodb::DatabaseFeature(&server), false); // required for TRI_vocbase_t::renameView(...)
    features.emplace_back(new arangodb::DatabasePathFeature(&server), false);
    features.emplace_back(new arangodb::FlushFeature(&server), false); // do not start the thread
    features.emplace_back(new arangodb::QueryRegistryFeature(&server), false); // required for TRI_vocbase_t instantiation
    features.emplace_back(new arangodb::ViewTypesFeature(&server), false); // required for TRI_vocbase_t::createView(...)
    features.emplace_back(new arangodb::iresearch::IResearchFeature(&server), false); // required for instantiating IResearchView*

    for (auto& f: features) {
      arangodb::application_features::ApplicationServer::server->addFeature(f.first);
    }

    for (auto& f: features) {
      f.first->prepare();
    }

    for (auto& f: features) {
      if (f.second) {
        f.first->start();
      }
    }

    arangodb::ClusterInfo::createInstance(nullptr); // required for generating view id

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
  }

  ~IResearchViewDBServerSetup() {
    TRI_RemoveDirectory(testFilesystemPath.c_str());
    arangodb::LogTopic::setLogLevel(arangodb::iresearch::TOPIC.name(), arangodb::LogLevel::DEFAULT);
    arangodb::EngineSelectorFeature::ENGINE = nullptr;
    arangodb::application_features::ApplicationServer::server = nullptr;
    arangodb::AgencyCommManager::MANAGER.reset();
    arangodb::ServerState::instance()->setRole(arangodb::ServerState::RoleEnum::ROLE_SINGLE);

    // destroy application features
    for (auto& f: features) {
      if (f.second) {
        f.first->stop();
      }
    }

    for (auto& f: features) {
      f.first->unprepare();
    }

    arangodb::LogTopic::setLogLevel(arangodb::Logger::AUTHENTICATION.name(), arangodb::LogLevel::DEFAULT);
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
  // drop empty
  {
    s.agency->responses.clear();
    s.agency->responses["POST /_api/agency/read HTTP/1.1\r\n\r\n[[\"/Sync/LatestID\"]]"] = "http/1.0 200\n\n[ { \"\": { \"Sync\": { \"LatestID\" : 1 } } } ]";
    s.agency->responses["POST /_api/agency/write HTTP/1.1"] = "http/1.0 200\n\n{\"results\": []}";
    auto json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto wiew = arangodb::iresearch::IResearchViewDBServer::make(vocbase, json->slice(), true, 42);
    CHECK((false == !wiew));
    auto* impl = dynamic_cast<arangodb::iresearch::IResearchViewDBServer*>(wiew.get());
    CHECK((nullptr != impl));

    CHECK((true == impl->drop().ok()));
  }

  // drop non-empty
  {
    s.agency->responses.clear();
    s.agency->responses["POST /_api/agency/read HTTP/1.1\r\n\r\n[[\"/Sync/LatestID\"]]"] = "http/1.0 200\n\n[ { \"\": { \"Sync\": { \"LatestID\" : 1 } } } ]";
    s.agency->responses["POST /_api/agency/write HTTP/1.1"] = "http/1.0 200\n\n{\"results\": []}";
    auto json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto wiew = arangodb::iresearch::IResearchViewDBServer::make(vocbase, json->slice(), true, 42);
    CHECK((false == !wiew));
    auto* impl = dynamic_cast<arangodb::iresearch::IResearchViewDBServer*>(wiew.get());
    CHECK((nullptr != impl));

    auto view = impl->ensure(123);
    auto const viewId = view->id();
    CHECK((false == !view));
    static auto visitor = [](TRI_voc_cid_t)->bool { return false; };
    CHECK((false == impl->visitCollections(visitor)));
    CHECK((false == !vocbase.lookupView(view->id())));
    CHECK((true == impl->drop().ok()));
    CHECK((true == !vocbase.lookupView(viewId)));
    CHECK((true == impl->visitCollections(visitor)));
  }

  // drop non-empty (drop failure)
  {
    s.agency->responses.clear();
    s.agency->responses["POST /_api/agency/read HTTP/1.1\r\n\r\n[[\"/Sync/LatestID\"]]"] = "http/1.0 200\n\n[ { \"\": { \"Sync\": { \"LatestID\" : 1 } } } ]";
    s.agency->responses["POST /_api/agency/write HTTP/1.1"] = "http/1.0 200\n\n{\"results\": []}";
    auto json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto wiew = arangodb::iresearch::IResearchViewDBServer::make(vocbase, json->slice(), true, 42);
    CHECK((false == !wiew));
    auto* impl = dynamic_cast<arangodb::iresearch::IResearchViewDBServer*>(wiew.get());
    CHECK((nullptr != impl));

    auto view = impl->ensure(123);
    CHECK((false == !view));
    static auto visitor = [](TRI_voc_cid_t)->bool { return false; };
    CHECK((false == impl->visitCollections(visitor)));
    CHECK((false == !vocbase.lookupView(view->id())));

    auto before = StorageEngineMock::before;
    auto restore = irs::make_finally([&before]()->void { StorageEngineMock::before = before; });
    StorageEngineMock::before = []()->void { throw std::exception(); };

    CHECK_THROWS((impl->drop()));
    CHECK((false == !vocbase.lookupView(view->id())));
    CHECK((false == impl->visitCollections(visitor)));
  }
}

SECTION("test_drop_cid") {
  s.agency->responses.clear();
  s.agency->responses["POST /_api/agency/read HTTP/1.1\r\n\r\n[[\"/Sync/LatestID\"]]"] = "http/1.0 200\n\n[ { \"\": { \"Sync\": { \"LatestID\" : 1 } } } ]";
  s.agency->responses["POST /_api/agency/write HTTP/1.1"] = "http/1.0 200\n\n{\"results\": []}";
  auto json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
  auto wiew = arangodb::iresearch::IResearchViewDBServer::make(vocbase, json->slice(), true, 42);
  CHECK((false == !wiew));
  auto* impl = dynamic_cast<arangodb::iresearch::IResearchViewDBServer*>(wiew.get());
  CHECK((nullptr != impl));

  auto view = impl->ensure(123);
  auto const viewId = view->id();
  CHECK((false == !view));
  static auto visitor = [](TRI_voc_cid_t)->bool { return false; };
  CHECK((false == impl->visitCollections(visitor)));
  CHECK((false == !vocbase.lookupView(view->id())));
  CHECK((TRI_ERROR_NO_ERROR == impl->drop(123).errorNumber()));
  CHECK((true == !vocbase.lookupView(viewId)));
  CHECK((true == impl->visitCollections(visitor)));
  CHECK((TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND == impl->drop(123).errorNumber())); // no longer present
}

SECTION("test_ensure") {
  s.agency->responses.clear();
  s.agency->responses["POST /_api/agency/read HTTP/1.1\r\n\r\n[[\"/Sync/LatestID\"]]"] = "http/1.0 200\n\n[ { \"\": { \"Sync\": { \"LatestID\" : 1 } } } ]";
  s.agency->responses["POST /_api/agency/write HTTP/1.1"] = "http/1.0 200\n\n{\"results\": []}";
  auto json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\", \"collections\": [ 3, 4, 5 ] }");
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
  auto wiew = arangodb::iresearch::IResearchViewDBServer::make(vocbase, json->slice(), true, 42);
  CHECK((false == !wiew));
  auto* impl = dynamic_cast<arangodb::iresearch::IResearchViewDBServer*>(wiew.get());
  CHECK((nullptr != impl));

  CHECK((true == !vocbase.lookupView("_iresearch_123_1_testView")));
  auto view = impl->ensure(123);
  CHECK((false == !view));
  CHECK((std::string("_iresearch_123_1_testView") == view->name()));
  CHECK((false == view->deleted()));
  CHECK((wiew->id() != view->id())); // must have unique ID
  CHECK((view->id() == view->planId())); // same as view ID
  CHECK((0 == view->planVersion()));
  CHECK((arangodb::iresearch::DATA_SOURCE_TYPE == view->type()));
  CHECK((&vocbase == &(view->vocbase())));
  static auto visitor = [](TRI_voc_cid_t)->bool { return false; };
  CHECK((true == view->visitCollections(visitor))); // no collections in view
  CHECK((false == !vocbase.lookupView("_iresearch_123_1_testView")));
}

SECTION("test_make") {
  // make DBServer view
  {
    s.agency->responses.clear();
    s.agency->responses["POST /_api/agency/read HTTP/1.1\r\n\r\n[[\"/Sync/LatestID\"]]"] = "http/1.0 200\n\n[ { \"\": { \"Sync\": { \"LatestID\" : 1 } } } ]";
    s.agency->responses["POST /_api/agency/write HTTP/1.1"] = "http/1.0 200\n\n{\"results\": []}";
    auto json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto wiew = arangodb::iresearch::IResearchViewDBServer::make(vocbase, json->slice(), true, 42);
    CHECK((false == !wiew));
    auto* impl = dynamic_cast<arangodb::iresearch::IResearchViewDBServer*>(wiew.get());
    CHECK((nullptr != impl));

    CHECK((std::string("testView") == wiew->name()));
    CHECK((false == wiew->deleted()));
    CHECK((1 == wiew->id()));
    CHECK((impl->id() == wiew->planId())); // same as view ID
    CHECK((0 == wiew->planVersion())); // when creating via vocbase planVersion is always 0
    CHECK((arangodb::iresearch::DATA_SOURCE_TYPE == wiew->type()));
    CHECK((&vocbase == &(wiew->vocbase())));
  }

  // make IResearchView (DBServer view also created)
  {
    s.agency->responses.clear();
    s.agency->responses["POST /_api/agency/read HTTP/1.1\r\n\r\n[[\"/Sync/LatestID\"]]"] = "http/1.0 200\n\n[ { \"\": { \"Sync\": { \"LatestID\" : 1 } } } ]";
    s.agency->responses["POST /_api/agency/write HTTP/1.1"] = "http/1.0 200\n\n{\"results\": []}";
    auto json = arangodb::velocypack::Parser::fromJson("{ \"id\": 100, \"name\": \"_iresearch_123_456_testView\", \"type\": \"arangosearch\", \"isSystem\": true }");
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    CHECK((true == !vocbase.lookupView("testView")));
    auto view = arangodb::iresearch::IResearchViewDBServer::make(vocbase, json->slice(), true, 42);
    CHECK((false == !view));
    auto* impl = dynamic_cast<arangodb::iresearch::IResearchView*>(view.get());
    CHECK((nullptr != impl));

    CHECK((std::string("_iresearch_123_456_testView") == view->name()));
    CHECK((false == view->deleted()));
    CHECK((100 == view->id()));
    CHECK((view->id() == view->planId())); // same as view ID
    CHECK((42 == view->planVersion()));
    CHECK((arangodb::iresearch::DATA_SOURCE_TYPE == view->type()));
    CHECK((&vocbase == &(view->vocbase())));

    auto wiew = vocbase.lookupView("testView");
    CHECK((false == !wiew));
    auto* wmpl = dynamic_cast<arangodb::iresearch::IResearchViewDBServer*>(wiew.get());
    CHECK((nullptr != wmpl));

    CHECK((std::string("testView") == wiew->name()));
    CHECK((false == wiew->deleted()));
    CHECK((view->id() != wiew->id()));
    CHECK((456 == wiew->id()));
    CHECK((wiew->id() == wiew->planId())); // same as view ID
    CHECK((0 == wiew->planVersion())); // when creating via vocbase planVersion is always 0
    CHECK((arangodb::iresearch::DATA_SOURCE_TYPE == wiew->type()));
    CHECK((&vocbase == &(wiew->vocbase())));
  }
}

SECTION("test_open") {
  // open empty
  {
    s.agency->responses.clear();
    s.agency->responses["POST /_api/agency/read HTTP/1.1\r\n\r\n[[\"/Sync/LatestID\"]]"] = "http/1.0 200\n\n[ { \"\": { \"Sync\": { \"LatestID\" : 1 } } } ]";
    s.agency->responses["POST /_api/agency/write HTTP/1.1"] = "http/1.0 200\n\n{\"results\": []}";
    auto json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto wiew = arangodb::iresearch::IResearchViewDBServer::make(vocbase, json->slice(), true, 42);
    CHECK((false == !wiew));
    auto* impl = dynamic_cast<arangodb::iresearch::IResearchViewDBServer*>(wiew.get());
    CHECK((nullptr != impl));

    static auto visitor = [](TRI_voc_cid_t)->bool { return false; };
    CHECK((true == impl->visitCollections(visitor)));
    wiew->open();
  }

  // open non-empty
  {
    s.agency->responses.clear();
    s.agency->responses["POST /_api/agency/read HTTP/1.1\r\n\r\n[[\"/Sync/LatestID\"]]"] = "http/1.0 200\n\n[ { \"\": { \"Sync\": { \"LatestID\" : 1 } } } ]";
    s.agency->responses["POST /_api/agency/write HTTP/1.1"] = "http/1.0 200\n\n{\"results\": []}";
    std::string dataPath = (((irs::utf8_path()/=s.testFilesystemPath)/=std::string("databases"))/=std::string("arangosearch-123")).utf8();
    auto json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto wiew = arangodb::iresearch::IResearchViewDBServer::make(vocbase, json->slice(), true, 42);
    CHECK((false == !wiew));
    auto* impl = dynamic_cast<arangodb::iresearch::IResearchViewDBServer*>(wiew.get());
    CHECK((nullptr != impl));

    static auto visitor = [](TRI_voc_cid_t)->bool { return false; };
    CHECK((true == impl->visitCollections(visitor)));
    auto view = impl->ensure(123);
    CHECK((false == !view));
    CHECK((false == impl->visitCollections(visitor)));
    wiew->open();
  }
}

SECTION("test_query") {
  s.agency->responses.clear();
  s.agency->responses["POST /_api/agency/read HTTP/1.1\r\n\r\n[[\"/Sync/LatestID\"]]"] = "http/1.0 200\n\n[ { \"\": { \"Sync\": { \"LatestID\" : 1 } } } ]";
  s.agency->responses["POST /_api/agency/write HTTP/1.1"] = "http/1.0 200\n\n{\"results\": []}";
  auto createJson = arangodb::velocypack::Parser::fromJson("{ \
    \"name\": \"testView\", \
    \"type\": \"arangosearch\" \
  }");
  static std::vector<std::string> const EMPTY;
  arangodb::aql::AstNode noop(arangodb::aql::AstNodeType::NODE_TYPE_FILTER);
  arangodb::aql::AstNode noopChild(true, arangodb::aql::AstNodeValueType::VALUE_TYPE_BOOL); // all

  noop.addMember(&noopChild);

  // no filter/order provided, means "RETURN *"
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalWiew = vocbase.createView(createJson->slice());
    REQUIRE((false == !logicalWiew));
    auto* wiewImpl = dynamic_cast<arangodb::iresearch::IResearchViewDBServer*>(logicalWiew.get());
    REQUIRE((false == !wiewImpl));
    auto logicalView = wiewImpl->ensure(42);
    REQUIRE((false == !logicalView));
    auto* viewImpl = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
    REQUIRE((false == !viewImpl));

    auto state = s.engine.createTransactionState(vocbase, arangodb::transaction::Options());
    auto* snapshot = wiewImpl->snapshot(*state, true);
    CHECK(0 == snapshot->docs_count());
  }

  // ordered iterator
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalWiew = vocbase.createView(createJson->slice());
    REQUIRE((false == !logicalWiew));
    auto* wiewImpl = dynamic_cast<arangodb::iresearch::IResearchViewDBServer*>(logicalWiew.get());
    REQUIRE((false == !wiewImpl));
    auto logicalView = wiewImpl->ensure(42);
    REQUIRE((false == !logicalView));
    auto* viewImpl = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());

    // fill with test data
    {
      auto doc = arangodb::velocypack::Parser::fromJson("{ \"key\": 1 }");
      arangodb::iresearch::IResearchLinkMeta meta;
      meta._includeAllFields = true;
      arangodb::transaction::Methods trx(
        arangodb::transaction::StandaloneContext::Create(vocbase),
        EMPTY,
        EMPTY,
        EMPTY,
        arangodb::transaction::Options()
      );
      CHECK((trx.begin().ok()));

      for (size_t i = 0; i < 12; ++i) {
        viewImpl->insert(trx, 1, arangodb::LocalDocumentId(i), doc->slice(), meta);
      }

      CHECK((trx.commit().ok()));
      viewImpl->sync();
    }

    auto state = s.engine.createTransactionState(vocbase, arangodb::transaction::Options());
    auto* snapshot = wiewImpl->snapshot(*state, true);
    CHECK(12 == snapshot->docs_count());
  }

  // snapshot isolation
  {
    auto links = arangodb::velocypack::Parser::fromJson("{ \
      \"links\": { \"testCollection\": { \"includeAllFields\" : true } } \
    }");
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");

    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto* logicalCollection = vocbase.createCollection(collectionJson->slice());
    std::vector<std::string> collections{ logicalCollection->name() };
    auto logicalWiew = vocbase.createView(createJson->slice());
    CHECK((false == !logicalWiew));
    auto* wiewImpl = dynamic_cast<arangodb::iresearch::IResearchViewDBServer*>(logicalWiew.get());
    CHECK((false == !wiewImpl));
    arangodb::Result res = logicalWiew->updateProperties(links->slice(), true, false);
    CHECK(true == res.ok());
    CHECK((false == logicalCollection->getIndexes().empty()));

    // fill with test data
    {
      arangodb::transaction::Methods trx(
        arangodb::transaction::StandaloneContext::Create(vocbase),
        EMPTY,
        collections,
        EMPTY,
        arangodb::transaction::Options()
      );
      CHECK((trx.begin().ok()));

      arangodb::ManagedDocumentResult inserted;
      TRI_voc_tick_t tick;
      arangodb::OperationOptions options;
      for (size_t i = 1; i <= 12; ++i) {
        auto doc = arangodb::velocypack::Parser::fromJson(std::string("{ \"key\": ") + std::to_string(i) + " }");
        logicalCollection->insert(&trx, doc->slice(), inserted, options, tick, false);
      }

      CHECK((trx.commit().ok()));
    }

    arangodb::transaction::Options trxOptions;
    trxOptions.waitForSync = true;
    auto state0 = s.engine.createTransactionState(vocbase, trxOptions);
    auto* snapshot0 = wiewImpl->snapshot(*state0, true);
    CHECK(12 == snapshot0->docs_count());

    // add more data
    {
      arangodb::transaction::Methods trx(
        arangodb::transaction::StandaloneContext::Create(vocbase),
        EMPTY,
        collections,
        EMPTY,
        arangodb::transaction::Options()
      );
      CHECK((trx.begin().ok()));

      arangodb::ManagedDocumentResult inserted;
      TRI_voc_tick_t tick;
      arangodb::OperationOptions options;
      for (size_t i = 13; i <= 24; ++i) {
        auto doc = arangodb::velocypack::Parser::fromJson(std::string("{ \"key\": ") + std::to_string(i) + " }");
        logicalCollection->insert(&trx, doc->slice(), inserted, options, tick, false);
      }

      CHECK(trx.commit().ok());
    }

    // old reader sees same data as before
    CHECK(12 == snapshot0->docs_count());
    // new reader sees new data
    auto state1 = s.engine.createTransactionState(vocbase, trxOptions);
    auto* snapshot1 = wiewImpl->snapshot(*state1, true);
    CHECK(24 == snapshot1->docs_count());
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
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto* logicalCollection = vocbase.createCollection(collectionJson->slice());
    auto logicalWiew = vocbase.createView(viewCreateJson->slice());
    REQUIRE((false == !logicalWiew));
    auto* wiewImpl = dynamic_cast<arangodb::iresearch::IResearchViewDBServer*>(logicalWiew.get());
    REQUIRE((false == !wiewImpl));
    arangodb::Result res = logicalWiew->updateProperties(viewUpdateJson->slice(), true, false);
    REQUIRE(true == res.ok());

    // start flush thread
    auto flush = std::make_shared<std::atomic<bool>>(true);
    std::thread flushThread([feature, flush]()->void{
      while (flush->load()) {
        feature->executeCallbacks();
      }
    });
    auto flushStop = irs::make_finally([flush, &flushThread]()->void{
      flush->store(false);
      flushThread.join();
    });

    static std::vector<std::string> const EMPTY;
    arangodb::transaction::Options options;

    options.waitForSync = true;

    arangodb::aql::Variable variable("testVariable", 0);

    // test insert + query
    for (size_t i = 1; i < 200; ++i) {
      // insert
      {
        auto doc = arangodb::velocypack::Parser::fromJson(std::string("{ \"seq\": ") + std::to_string(i) + " }");
        arangodb::transaction::Methods trx(
          arangodb::transaction::StandaloneContext::Create(vocbase),
          EMPTY,
          EMPTY,
          EMPTY,
          options
        );

        CHECK((trx.begin().ok()));
        CHECK((trx.insert(logicalCollection->name(), doc->slice(), arangodb::OperationOptions()).ok()));
        CHECK((trx.commit().ok()));
      }

      // query
      {
        auto state = s.engine.createTransactionState(vocbase, arangodb::transaction::Options());
        auto* snapshot = wiewImpl->snapshot(*state, true);
        CHECK(i == snapshot->docs_count());
      }
    }
  }
}

SECTION("test_rename") {
  // rename empty
  {
    s.agency->responses.clear();
    s.agency->responses["POST /_api/agency/read HTTP/1.1\r\n\r\n[[\"/Sync/LatestID\"]]"] = "http/1.0 200\n\n[ { \"\": { \"Sync\": { \"LatestID\" : 1 } } } ]";
    s.agency->responses["POST /_api/agency/write HTTP/1.1"] = "http/1.0 200\n\n{\"results\": []}";
    auto json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto wiew = arangodb::iresearch::IResearchViewDBServer::make(vocbase, json->slice(), true, 42);
    CHECK((false == !wiew));
    auto* impl = dynamic_cast<arangodb::iresearch::IResearchViewDBServer*>(wiew.get());
    CHECK((nullptr != impl));

    CHECK((std::string("testView") == wiew->name()));

    {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      wiew->toVelocyPack(builder, false, false);
      builder.close();
      CHECK((builder.slice().hasKey("name")));
      CHECK((std::string("testView") == builder.slice().get("name").copyString()));
    }

    CHECK((true == wiew->rename("newName", true).ok()));

    {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      wiew->toVelocyPack(builder, false, false);
      builder.close();
      CHECK((builder.slice().hasKey("name")));
      CHECK((std::string("newName") == builder.slice().get("name").copyString()));
    }

    auto view = impl->ensure(123);
    CHECK((false == !view));
    CHECK((std::string("_iresearch_123_1_newName") == view->name()));
  }

  // rename non-empty
  {
    s.agency->responses.clear();
    s.agency->responses["POST /_api/agency/read HTTP/1.1\r\n\r\n[[\"/Sync/LatestID\"]]"] = "http/1.0 200\n\n[ { \"\": { \"Sync\": { \"LatestID\" : 1 } } } ]";
    s.agency->responses["POST /_api/agency/write HTTP/1.1"] = "http/1.0 200\n\n{\"results\": []}";
    auto json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto wiew = arangodb::iresearch::IResearchViewDBServer::make(vocbase, json->slice(), true, 42);
    CHECK((false == !wiew));
    auto* impl = dynamic_cast<arangodb::iresearch::IResearchViewDBServer*>(wiew.get());
    CHECK((nullptr != impl));

    CHECK((std::string("testView") == wiew->name()));
    auto view = impl->ensure(123);
    CHECK((false == !view));
    CHECK((std::string("_iresearch_123_3_testView") == view->name()));

    {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      wiew->toVelocyPack(builder, false, false);
      builder.close();
      CHECK((builder.slice().hasKey("name")));
      CHECK((std::string("testView") == builder.slice().get("name").copyString()));
    }

    CHECK((true == wiew->rename("newName", true).ok()));

    {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      wiew->toVelocyPack(builder, false, false);
      builder.close();
      CHECK((builder.slice().hasKey("name")));
      CHECK((std::string("newName") == builder.slice().get("name").copyString()));
    }

    CHECK((std::string("_iresearch_123_3_newName") == view->name()));
    wiew->rename("testView", true); // rename back or vocbase will be out of sync
  }
}

SECTION("test_toVelocyPack") {
  // base
  {
    s.agency->responses.clear();
    s.agency->responses["POST /_api/agency/read HTTP/1.1\r\n\r\n[[\"/Sync/LatestID\"]]"] = "http/1.0 200\n\n[ { \"\": { \"Sync\": { \"LatestID\" : 1 } } } ]";
    s.agency->responses["POST /_api/agency/write HTTP/1.1"] = "http/1.0 200\n\n{\"results\": []}";
    auto json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\", \"unusedKey\": \"unusedValue\" }");
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto wiew = arangodb::iresearch::IResearchViewDBServer::make(vocbase, json->slice(), true, 42);
    CHECK((false == !wiew));
    auto* impl = dynamic_cast<arangodb::iresearch::IResearchViewDBServer*>(wiew.get());
    CHECK((nullptr != impl));

    arangodb::velocypack::Builder builder;

    builder.openObject();
    wiew->toVelocyPack(builder,false, false);
    builder.close();
    auto slice = builder.slice();
    CHECK((3 == slice.length()));
    CHECK((slice.hasKey("id") && slice.get("id").isString() && std::string("1") == slice.get("id").copyString()));
    CHECK((slice.hasKey("name") && slice.get("name").isString() && std::string("testView") == slice.get("name").copyString()));
    CHECK((slice.hasKey("type") && slice.get("type").isString() && arangodb::iresearch::DATA_SOURCE_TYPE.name() == slice.get("type").copyString()));
  }

  // includeProperties
  {
    s.agency->responses.clear();
    s.agency->responses["POST /_api/agency/read HTTP/1.1\r\n\r\n[[\"/Sync/LatestID\"]]"] = "http/1.0 200\n\n[ { \"\": { \"Sync\": { \"LatestID\" : 1 } } } ]";
    s.agency->responses["POST /_api/agency/write HTTP/1.1"] = "http/1.0 200\n\n{\"results\": []}";
    auto json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\", \"unusedKey\": \"unusedValue\" }");
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto wiew = arangodb::iresearch::IResearchViewDBServer::make(vocbase, json->slice(), true, 42);
    CHECK((false == !wiew));
    auto* impl = dynamic_cast<arangodb::iresearch::IResearchViewDBServer*>(wiew.get());
    CHECK((nullptr != impl));

    arangodb::velocypack::Builder builder;

    builder.openObject();
    wiew->toVelocyPack(builder, true, false);
    builder.close();
    auto slice = builder.slice();
    CHECK((4 == slice.length()));
    CHECK((slice.hasKey("id") && slice.get("id").isString() && std::string("2") == slice.get("id").copyString()));
    CHECK((slice.hasKey("name") && slice.get("name").isString() && std::string("testView") == slice.get("name").copyString()));
    CHECK((slice.hasKey("type") && slice.get("type").isString() && arangodb::iresearch::DATA_SOURCE_TYPE.name() == slice.get("type").copyString()));
    CHECK((slice.hasKey("properties")));
    auto props = slice.get("properties");
    CHECK((props.isObject()));
    CHECK((1 == props.length()));
    CHECK((props.hasKey("collections") && props.get("collections").isArray() && 0 == props.get("collections").length()));
  }

  // includeSystem
  {
    s.agency->responses.clear();
    s.agency->responses["POST /_api/agency/read HTTP/1.1\r\n\r\n[[\"/Sync/LatestID\"]]"] = "http/1.0 200\n\n[ { \"\": { \"Sync\": { \"LatestID\" : 1 } } } ]";
    s.agency->responses["POST /_api/agency/write HTTP/1.1"] = "http/1.0 200\n\n{\"results\": []}";
    auto json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\", \"unusedKey\": \"unusedValue\" }");
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto wiew = arangodb::iresearch::IResearchViewDBServer::make(vocbase, json->slice(), true, 42);
    CHECK((false == !wiew));
    auto* impl = dynamic_cast<arangodb::iresearch::IResearchViewDBServer*>(wiew.get());
    CHECK((nullptr != impl));

    arangodb::velocypack::Builder builder;

    builder.openObject();
    wiew->toVelocyPack(builder, false, true);
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
  static std::vector<std::string> const EMPTY;
  s.agency->responses.clear();
  s.agency->responses["POST /_api/agency/read HTTP/1.1\r\n\r\n[[\"/Sync/LatestID\"]]"] = "http/1.0 200\n\n[ { \"\": { \"Sync\": { \"LatestID\" : 1 } } } ]";
  s.agency->responses["POST /_api/agency/write HTTP/1.1"] = "http/1.0 200\n\n{\"results\": []}";
  auto viewJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\", \"commit\": { \"commitIntervalMsec\": 0 } }");
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
  auto logicalWiew = vocbase.createView(viewJson->slice());
  REQUIRE((false == !logicalWiew));
  auto* wiewImpl = dynamic_cast<arangodb::iresearch::IResearchViewDBServer*>(logicalWiew.get());
  REQUIRE((nullptr != wiewImpl));
  auto logicalView = wiewImpl->ensure(42);
  REQUIRE((false == !logicalView));
  auto* viewImpl = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());

  // add a single document to view (do not sync)
  {
    auto doc = arangodb::velocypack::Parser::fromJson("{ \"key\": 1 }");
    arangodb::iresearch::IResearchLinkMeta meta;
    meta._includeAllFields = true;
    arangodb::transaction::Methods trx(
      arangodb::transaction::StandaloneContext::Create(vocbase),
      EMPTY,
      EMPTY,
      EMPTY,
      arangodb::transaction::Options()
    );
    CHECK((trx.begin().ok()));
    viewImpl->insert(trx, 42, arangodb::LocalDocumentId(0), doc->slice(), meta);
    CHECK((trx.commit().ok()));
  }

  // no snapshot in TransactionState (force == false, waitForSync = false)
  {
    auto state = s.engine.createTransactionState(vocbase, arangodb::transaction::Options());
    auto* snapshot = wiewImpl->snapshot(*state);
    CHECK((nullptr == snapshot));
  }

  // no snapshot in TransactionState (force == true, waitForSync = false)
  {
    auto state = s.engine.createTransactionState(vocbase, arangodb::transaction::Options());
    auto* snapshot = wiewImpl->snapshot(*state, true);
    CHECK((nullptr != snapshot));
    CHECK((0 == snapshot->live_docs_count()));
  }

  // no snapshot in TransactionState (force == false, waitForSync = true)
  {
    auto state = s.engine.createTransactionState(vocbase, arangodb::transaction::Options());
    state->waitForSync(true);
    auto* snapshot = wiewImpl->snapshot(*state);
    CHECK((nullptr == snapshot));
  }

  // no snapshot in TransactionState (force == true, waitForSync = true)
  {
    auto state = s.engine.createTransactionState(vocbase, arangodb::transaction::Options());
    state->waitForSync(true);
    auto* snapshot = wiewImpl->snapshot(*state, true);
    CHECK((nullptr != snapshot));
    CHECK((1 == snapshot->live_docs_count()));
  }

  // add another single document to view (do not sync)
  {
    auto doc = arangodb::velocypack::Parser::fromJson("{ \"key\": 2 }");
    arangodb::iresearch::IResearchLinkMeta meta;
    meta._includeAllFields = true;
    arangodb::transaction::Methods trx(
      arangodb::transaction::StandaloneContext::Create(vocbase),
      EMPTY,
      EMPTY,
      EMPTY,
      arangodb::transaction::Options()
    );
    CHECK((trx.begin().ok()));
    viewImpl->insert(trx, 42, arangodb::LocalDocumentId(1), doc->slice(), meta);
    CHECK((trx.commit().ok()));
  }

  // old snapshot in TransactionState (force == false, waitForSync = false)
  {
    arangodb::transaction::Methods trx(
      arangodb::transaction::StandaloneContext::Create(vocbase),
      EMPTY,
      EMPTY,
      EMPTY,
      arangodb::transaction::Options()
    );
    auto* state = trx.state();
    wiewImpl->apply(trx);
    state->updateStatus(arangodb::transaction::Status::RUNNING);
    auto* snapshot = wiewImpl->snapshot(*state);
    CHECK((nullptr != snapshot));
    CHECK((1 == snapshot->live_docs_count()));
    state->updateStatus(arangodb::transaction::Status::ABORTED); // prevent assertion ind destructor
  }

  // old snapshot in TransactionState (force == true, waitForSync = false)
  {
    arangodb::transaction::Methods trx(
      arangodb::transaction::StandaloneContext::Create(vocbase),
      EMPTY,
      EMPTY,
      EMPTY,
      arangodb::transaction::Options()
    );
    auto* state = trx.state();
    wiewImpl->apply(trx);
    state->updateStatus(arangodb::transaction::Status::RUNNING);
    auto* snapshot = wiewImpl->snapshot(*state, true);
    CHECK((nullptr != snapshot));
    CHECK((1 == snapshot->live_docs_count()));
    state->updateStatus(arangodb::transaction::Status::ABORTED); // prevent assertion ind destructor
  }

  // old snapshot in TransactionState (force == true, waitForSync = false during updateStatus(), true during snapshot())
  {
    arangodb::transaction::Methods trx(
      arangodb::transaction::StandaloneContext::Create(vocbase),
      EMPTY,
      EMPTY,
      EMPTY,
      arangodb::transaction::Options()
    );
    auto* state = trx.state();
    wiewImpl->apply(trx);
    state->updateStatus(arangodb::transaction::Status::RUNNING);
    state->waitForSync(true);
    auto* snapshot = wiewImpl->snapshot(*state, true);
    CHECK((nullptr != snapshot));
    CHECK((1 == snapshot->live_docs_count()));
    state->updateStatus(arangodb::transaction::Status::ABORTED); // prevent assertion ind destructor
  }

  // old snapshot in TransactionState (force == true, waitForSync = true during updateStatus(), false during snapshot())
  {
    arangodb::transaction::Methods trx(
      arangodb::transaction::StandaloneContext::Create(vocbase),
      EMPTY,
      EMPTY,
      EMPTY,
      arangodb::transaction::Options()
    );
    auto* state = trx.state();
    state->waitForSync(true);
    wiewImpl->apply(trx);
    state->updateStatus(arangodb::transaction::Status::RUNNING);
    state->waitForSync(false);
    auto* snapshot = wiewImpl->snapshot(*state, true);
    CHECK((nullptr != snapshot));
    CHECK((2 == snapshot->live_docs_count()));
    state->updateStatus(arangodb::transaction::Status::ABORTED); // prevent assertion ind destructor
  }
}

SECTION("test_updateProperties") {
  // update empty (partial)
  {
    s.agency->responses.clear();
    s.agency->responses["POST /_api/agency/read HTTP/1.1\r\n\r\n[[\"/Sync/LatestID\"]]"] = "http/1.0 200\n\n[ { \"\": { \"Sync\": { \"LatestID\" : 1 } } } ]";
    s.agency->responses["POST /_api/agency/write HTTP/1.1"] = "http/1.0 200\n\n{\"results\": []}";
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
    auto json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\", \"properties\": { \"collections\": [ 3, 4, 5 ], \"threadsMaxIdle\": 24, \"threadsMaxTotal\": 42 } }");
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto* logicalCollection = vocbase.createCollection(collectionJson->slice());
    CHECK((nullptr != logicalCollection));
    auto wiew = arangodb::iresearch::IResearchViewDBServer::make(vocbase, json->slice(), true, 42);
    CHECK((false == !wiew));
    auto* impl = dynamic_cast<arangodb::iresearch::IResearchViewDBServer*>(wiew.get());
    CHECK((nullptr != impl));

    {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      wiew->toVelocyPack(builder, true, false);
      builder.close();
      auto slice = builder.slice().get("properties");
      CHECK((slice.hasKey("collections") && slice.get("collections").isArray() && 0 == slice.get("collections").length()));
      CHECK((slice.hasKey("threadsMaxIdle") && slice.get("threadsMaxIdle").isNumber<size_t>() && 24 == slice.get("threadsMaxIdle").getNumber<size_t>()));
      CHECK((slice.hasKey("threadsMaxTotal") && slice.get("threadsMaxTotal").isNumber<size_t>() && 42 == slice.get("threadsMaxTotal").getNumber<size_t>()));
    }

    {
      auto update = arangodb::velocypack::Parser::fromJson("{ \"collections\": [ 6, 7, 8, 9 ], \"links\": { \"testCollection\": {} }, \"threadsMaxTotal\": 52 }");
      CHECK((true == wiew->updateProperties(update->slice(), true, true).ok()));
    }

    {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      wiew->toVelocyPack(builder, true, false);
      builder.close();
      auto slice = builder.slice().get("properties");
      CHECK((slice.hasKey("collections") && slice.get("collections").isArray() && 1 == slice.get("collections").length()));
      CHECK((!slice.hasKey("links")));
      CHECK((slice.hasKey("threadsMaxIdle") && slice.get("threadsMaxIdle").isNumber<size_t>() && 24 == slice.get("threadsMaxIdle").getNumber<size_t>()));
      CHECK((slice.hasKey("threadsMaxTotal") && slice.get("threadsMaxTotal").isNumber<size_t>() && 52 == slice.get("threadsMaxTotal").getNumber<size_t>()));
    }

    auto view = impl->ensure(123);
    CHECK((false == !view));
    static auto visitor = [](TRI_voc_cid_t)->bool { return false; };
    CHECK((true == view->visitCollections(visitor))); // no collections in view

    arangodb::velocypack::Builder builder;

    builder.openObject();
    view->toVelocyPack(builder, true, false);
    builder.close();
    auto slice = builder.slice().get("properties");
    CHECK((slice.hasKey("collections") && slice.get("collections").isArray() && 0 == slice.get("collections").length()));
    CHECK((slice.hasKey("links") && slice.get("links").isObject() && 0 == slice.get("links").length()));
    CHECK((slice.hasKey("threadsMaxIdle") && slice.get("threadsMaxIdle").isNumber<size_t>() && 24 == slice.get("threadsMaxIdle").getNumber<size_t>()));
    CHECK((slice.hasKey("threadsMaxTotal") && slice.get("threadsMaxTotal").isNumber<size_t>() && 52 == slice.get("threadsMaxTotal").getNumber<size_t>()));
  }

  // update empty (full)
  {
    s.agency->responses.clear();
    s.agency->responses["POST /_api/agency/read HTTP/1.1\r\n\r\n[[\"/Sync/LatestID\"]]"] = "http/1.0 200\n\n[ { \"\": { \"Sync\": { \"LatestID\" : 1 } } } ]";
    s.agency->responses["POST /_api/agency/write HTTP/1.1"] = "http/1.0 200\n\n{\"results\": []}";
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
    auto json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\", \"properties\": { \"collections\": [ 3, 4, 5 ], \"threadsMaxIdle\": 24, \"threadsMaxTotal\": 42 } }");
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto* logicalCollection = vocbase.createCollection(collectionJson->slice());
    CHECK((nullptr != logicalCollection));
    auto wiew = arangodb::iresearch::IResearchViewDBServer::make(vocbase, json->slice(), true, 42);
    CHECK((false == !wiew));
    auto* impl = dynamic_cast<arangodb::iresearch::IResearchViewDBServer*>(wiew.get());
    CHECK((nullptr != impl));

    {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      wiew->toVelocyPack(builder, true, false);
      builder.close();
      auto slice = builder.slice().get("properties");
      CHECK((slice.hasKey("collections") && slice.get("collections").isArray() && 0 == slice.get("collections").length()));
      CHECK((slice.hasKey("threadsMaxIdle") && slice.get("threadsMaxIdle").isNumber<size_t>() && 24 == slice.get("threadsMaxIdle").getNumber<size_t>()));
      CHECK((slice.hasKey("threadsMaxTotal") && slice.get("threadsMaxTotal").isNumber<size_t>() && 42 == slice.get("threadsMaxTotal").getNumber<size_t>()));
    }

    {
      auto update = arangodb::velocypack::Parser::fromJson("{ \"collections\": [ 6, 7, 8, 9 ], \"links\": { \"testCollection\": {} }, \"threadsMaxTotal\": 52 }");
      CHECK((true == wiew->updateProperties(update->slice(), false, true).ok()));
    }

    {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      wiew->toVelocyPack(builder, true, false);
      builder.close();
      auto slice = builder.slice().get("properties");
      CHECK((slice.hasKey("collections") && slice.get("collections").isArray() && 1 == slice.get("collections").length()));
      CHECK((!slice.hasKey("links")));
      CHECK((slice.hasKey("threadsMaxIdle") && slice.get("threadsMaxIdle").isNumber<size_t>() && 5 == slice.get("threadsMaxIdle").getNumber<size_t>()));
      CHECK((slice.hasKey("threadsMaxTotal") && slice.get("threadsMaxTotal").isNumber<size_t>() && 52 == slice.get("threadsMaxTotal").getNumber<size_t>()));
    }

    auto view = impl->ensure(123);
    CHECK((false == !view));
    static auto visitor = [](TRI_voc_cid_t)->bool { return false; };
    CHECK((true == view->visitCollections(visitor))); // no collections in view

    arangodb::velocypack::Builder builder;

    builder.openObject();
    view->toVelocyPack(builder, true, false);
    builder.close();
    auto slice = builder.slice().get("properties");
    CHECK((slice.hasKey("collections") && slice.get("collections").isArray() && 0 == slice.get("collections").length()));
    CHECK((slice.hasKey("links") && slice.get("links").isObject() && 0 == slice.get("links").length()));
    CHECK((slice.hasKey("threadsMaxIdle") && slice.get("threadsMaxIdle").isNumber<size_t>() && 5 == slice.get("threadsMaxIdle").getNumber<size_t>()));
    CHECK((slice.hasKey("threadsMaxTotal") && slice.get("threadsMaxTotal").isNumber<size_t>() && 52 == slice.get("threadsMaxTotal").getNumber<size_t>()));
  }

  // update non-empty (partial)
  {
    s.agency->responses.clear();
    s.agency->responses["POST /_api/agency/read HTTP/1.1\r\n\r\n[[\"/Sync/LatestID\"]]"] = "http/1.0 200\n\n[ { \"\": { \"Sync\": { \"LatestID\" : 1 } } } ]";
    s.agency->responses["POST /_api/agency/write HTTP/1.1"] = "http/1.0 200\n\n{\"results\": []}";
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
    auto json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\", \"properties\": { \"collections\": [ 3, 4, 5 ], \"threadsMaxIdle\": 24, \"threadsMaxTotal\": 42 } }");
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto* logicalCollection = vocbase.createCollection(collectionJson->slice());
    CHECK((nullptr != logicalCollection));
    auto wiew = arangodb::iresearch::IResearchViewDBServer::make(vocbase, json->slice(), true, 42);
    CHECK((false == !wiew));
    auto* impl = dynamic_cast<arangodb::iresearch::IResearchViewDBServer*>(wiew.get());
    CHECK((nullptr != impl));

    auto view = impl->ensure(123);
    CHECK((false == !view));
    static auto visitor = [](TRI_voc_cid_t)->bool { return false; };
    CHECK((true == view->visitCollections(visitor))); // no collections in view

    {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      wiew->toVelocyPack(builder, true, false);
      builder.close();
      auto slice = builder.slice().get("properties");
      CHECK((slice.hasKey("collections") && slice.get("collections").isArray() && 1 == slice.get("collections").length()));
      CHECK((slice.hasKey("threadsMaxIdle") && slice.get("threadsMaxIdle").isNumber<size_t>() && 24 == slice.get("threadsMaxIdle").getNumber<size_t>()));
      CHECK((slice.hasKey("threadsMaxTotal") && slice.get("threadsMaxTotal").isNumber<size_t>() && 42 == slice.get("threadsMaxTotal").getNumber<size_t>()));
    }

    {
      auto update = arangodb::velocypack::Parser::fromJson("{ \"collections\": [ 6, 7, 8 ], \"links\": { \"testCollection\": {} }, \"threadsMaxTotal\": 52 }");
      CHECK((true == wiew->updateProperties(update->slice(), true, true).ok()));
    }

    {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      wiew->toVelocyPack(builder, true, false);
      builder.close();
      auto slice = builder.slice().get("properties");
      CHECK((slice.hasKey("collections") && slice.get("collections").isArray() && 2 == slice.get("collections").length()));
      CHECK((!slice.hasKey("links")));
      CHECK((slice.hasKey("threadsMaxIdle") && slice.get("threadsMaxIdle").isNumber<size_t>() && 24 == slice.get("threadsMaxIdle").getNumber<size_t>()));
      CHECK((slice.hasKey("threadsMaxTotal") && slice.get("threadsMaxTotal").isNumber<size_t>() && 52 == slice.get("threadsMaxTotal").getNumber<size_t>()));
    }

    CHECK((true == view->visitCollections(visitor))); // no collections in view

    arangodb::velocypack::Builder builder;

    builder.openObject();
    view->toVelocyPack(builder, true, false);
    builder.close();
    auto slice = builder.slice().get("properties");
    CHECK((slice.hasKey("collections") && slice.get("collections").isArray() && 0 == slice.get("collections").length()));
    CHECK((slice.hasKey("links") && slice.get("links").isObject() && 0 == slice.get("links").length()));
    CHECK((slice.hasKey("threadsMaxIdle") && slice.get("threadsMaxIdle").isNumber<size_t>() && 24 == slice.get("threadsMaxIdle").getNumber<size_t>()));
    CHECK((slice.hasKey("threadsMaxTotal") && slice.get("threadsMaxTotal").isNumber<size_t>() && 52 == slice.get("threadsMaxTotal").getNumber<size_t>()));
  }

  // update non-empty (full)
  {
    s.agency->responses.clear();
    s.agency->responses["POST /_api/agency/read HTTP/1.1\r\n\r\n[[\"/Sync/LatestID\"]]"] = "http/1.0 200\n\n[ { \"\": { \"Sync\": { \"LatestID\" : 1 } } } ]";
    s.agency->responses["POST /_api/agency/write HTTP/1.1"] = "http/1.0 200\n\n{\"results\": []}";
    auto collection0Json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
    auto collection1Json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection1\", \"id\": \"123\" }");
    auto json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\", \"properties\": { \"collections\": [ 3, 4, 5 ], \"threadsMaxIdle\": 24, \"threadsMaxTotal\": 42 } }");
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto* logicalCollection0 = vocbase.createCollection(collection0Json->slice());
    CHECK((nullptr != logicalCollection0));
    auto* logicalCollection1 = vocbase.createCollection(collection1Json->slice());
    CHECK((nullptr != logicalCollection1));
    auto wiew = arangodb::iresearch::IResearchViewDBServer::make(vocbase, json->slice(), true, 42);
    CHECK((false == !wiew));
    auto* impl = dynamic_cast<arangodb::iresearch::IResearchViewDBServer*>(wiew.get());
    CHECK((nullptr != impl));

    auto view = impl->ensure(123);
    CHECK((false == !view));
    static auto visitor = [](TRI_voc_cid_t)->bool { return false; };
    CHECK((true == view->visitCollections(visitor))); // no collections in view

    {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      wiew->toVelocyPack(builder, true, false);
      builder.close();
      auto slice = builder.slice().get("properties");
      CHECK((slice.hasKey("collections") && slice.get("collections").isArray() && 1 == slice.get("collections").length()));
      CHECK((slice.hasKey("threadsMaxIdle") && slice.get("threadsMaxIdle").isNumber<size_t>() && 24 == slice.get("threadsMaxIdle").getNumber<size_t>()));
      CHECK((slice.hasKey("threadsMaxTotal") && slice.get("threadsMaxTotal").isNumber<size_t>() && 42 == slice.get("threadsMaxTotal").getNumber<size_t>()));
    }

    {
      auto update = arangodb::velocypack::Parser::fromJson("{ \"collections\": [ 6, 7, 8 ], \"links\": { \"testCollection\": {} }, \"threadsMaxTotal\": 52 }");
      CHECK((true == wiew->updateProperties(update->slice(), false, true).ok()));
    }

    {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      wiew->toVelocyPack(builder, true, false);
      builder.close();
      auto slice = builder.slice().get("properties");
      CHECK((slice.hasKey("collections") && slice.get("collections").isArray() && 1 == slice.get("collections").length()));
      CHECK((!slice.hasKey("links")));
      CHECK((slice.hasKey("threadsMaxIdle") && slice.get("threadsMaxIdle").isNumber<size_t>() && 5 == slice.get("threadsMaxIdle").getNumber<size_t>()));
      CHECK((slice.hasKey("threadsMaxTotal") && slice.get("threadsMaxTotal").isNumber<size_t>() && 52 == slice.get("threadsMaxTotal").getNumber<size_t>()));
    }

    CHECK((true == view->visitCollections(visitor))); // no collections in view

    arangodb::velocypack::Builder builder;

    builder.openObject();
    view->toVelocyPack(builder, true, false);
    builder.close();
    auto slice = builder.slice().get("properties");
    CHECK((slice.hasKey("collections") && slice.get("collections").isArray() && 0 == slice.get("collections").length()));
    CHECK((slice.hasKey("links") && slice.get("links").isObject() && 0 == slice.get("links").length()));
    CHECK((slice.hasKey("threadsMaxIdle") && slice.get("threadsMaxIdle").isNumber<size_t>() && 5 == slice.get("threadsMaxIdle").getNumber<size_t>()));
    CHECK((slice.hasKey("threadsMaxTotal") && slice.get("threadsMaxTotal").isNumber<size_t>() && 52 == slice.get("threadsMaxTotal").getNumber<size_t>()));
  }
}

SECTION("test_visitCollections") {
  // visit empty
  {
    s.agency->responses.clear();
    s.agency->responses["POST /_api/agency/read HTTP/1.1\r\n\r\n[[\"/Sync/LatestID\"]]"] = "http/1.0 200\n\n[ { \"\": { \"Sync\": { \"LatestID\" : 1 } } } ]";
    s.agency->responses["POST /_api/agency/write HTTP/1.1"] = "http/1.0 200\n\n{\"results\": []}";
    auto json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto wiew = arangodb::iresearch::IResearchViewDBServer::make(vocbase, json->slice(), true, 42);
    CHECK((false == !wiew));
    auto* impl = dynamic_cast<arangodb::iresearch::IResearchViewDBServer*>(wiew.get());
    CHECK((nullptr != impl));

    static auto visitor = [](TRI_voc_cid_t)->bool { return false; };
    CHECK((true == wiew->visitCollections(visitor))); // no collections in view
  }

  // visit non-empty
  {
    s.agency->responses.clear();
    s.agency->responses["POST /_api/agency/read HTTP/1.1\r\n\r\n[[\"/Sync/LatestID\"]]"] = "http/1.0 200\n\n[ { \"\": { \"Sync\": { \"LatestID\" : 1 } } } ]";
    s.agency->responses["POST /_api/agency/write HTTP/1.1"] = "http/1.0 200\n\n{\"results\": []}";
    auto json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto wiew = arangodb::iresearch::IResearchViewDBServer::make(vocbase, json->slice(), true, 42);
    CHECK((false == !wiew));
    auto* impl = dynamic_cast<arangodb::iresearch::IResearchViewDBServer*>(wiew.get());
    CHECK((nullptr != impl));

    auto view = impl->ensure(123);
    CHECK((false == !view));
    std::set<TRI_voc_cid_t> cids = { 123 };
    static auto visitor = [&cids](TRI_voc_cid_t cid)->bool { return 1 == cids.erase(cid); };
    CHECK((true == wiew->visitCollections(visitor))); // all collections expected
    CHECK((true == cids.empty()));
    CHECK((true == impl->drop(123).ok()));
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