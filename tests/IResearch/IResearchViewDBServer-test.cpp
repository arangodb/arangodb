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
#include "Basics/files.h"
#include "Cluster/ClusterInfo.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "IResearch/IResearchCommon.h"
#include "IResearch/IResearchFeature.h"
#include "IResearch/IResearchView.h"
#include "IResearch/IResearchViewDBServer.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/DatabasePathFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "RestServer/ViewTypesFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "velocypack/Parser.h"

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

SECTION("test_updateProperties") {
  // update empty (partial)
  {
    s.agency->responses.clear();
    s.agency->responses["POST /_api/agency/read HTTP/1.1\r\n\r\n[[\"/Sync/LatestID\"]]"] = "http/1.0 200\n\n[ { \"\": { \"Sync\": { \"LatestID\" : 1 } } } ]";
    s.agency->responses["POST /_api/agency/write HTTP/1.1"] = "http/1.0 200\n\n{\"results\": []}";
    auto json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\", \"properties\": { \"collections\": [ 3, 4, 5 ], \"threadsMaxIdle\": 24, \"threadsMaxTotal\": 42 } }");
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
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
      CHECK((slice.hasKey("collections") && slice.get("collections").isArray() && 0 == slice.get("collections").length()));
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
    auto json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\", \"properties\": { \"collections\": [ 3, 4, 5 ], \"threadsMaxIdle\": 24, \"threadsMaxTotal\": 42 } }");
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
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
      CHECK((slice.hasKey("collections") && slice.get("collections").isArray() && 0 == slice.get("collections").length()));
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
    auto json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\", \"properties\": { \"collections\": [ 3, 4, 5 ], \"threadsMaxIdle\": 24, \"threadsMaxTotal\": 42 } }");
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
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
      CHECK((slice.hasKey("collections") && slice.get("collections").isArray() && 1 == slice.get("collections").length()));
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
    auto json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\", \"properties\": { \"collections\": [ 3, 4, 5 ], \"threadsMaxIdle\": 24, \"threadsMaxTotal\": 42 } }");
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
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
