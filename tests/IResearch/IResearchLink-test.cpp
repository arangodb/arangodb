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
#include "../Mocks/StorageEngineMock.h"

#include "store/fs_directory.hpp"
#include "utils/log.hpp"
#include "utils/utf8_path.hpp"

#include "Aql/AqlFunctionFeature.h"
#include "Aql/QueryRegistry.h"

#if USE_ENTERPRISE
  #include "Enterprise/Ldap/LdapFeature.h"
#endif

#include "Basics/files.h"
#include "Cluster/ClusterFeature.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "IResearch/IResearchAnalyzerFeature.h"
#include "IResearch/IResearchCommon.h"
#include "IResearch/IResearchFeature.h"
#include "IResearch/IResearchLink.h"
#include "IResearch/IResearchMMFilesLink.h"
#include "IResearch/IResearchView.h"
#include "Logger/Logger.h"
#include "Logger/LogTopic.h"
#include "MMFiles/MMFilesWalRecoverState.h"
#include "RestServer/AqlFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/DatabasePathFeature.h"
#include "RestServer/FlushFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "RestServer/ServerIdFeature.h"
#include "RestServer/SystemDatabaseFeature.h"
#include "RestServer/TraverserEngineRegistryFeature.h"
#include "RestServer/ViewTypesFeature.h"
#include "Sharding/ShardingFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "Transaction/Methods.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/ExecContext.h"
#include "V8Server/V8DealerFeature.h"
#include "VocBase/KeyGenerator.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/LogicalView.h"

#include <velocypack/Parser.h>

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

struct IResearchLinkSetup {
  StorageEngineMock engine;
  arangodb::application_features::ApplicationServer server;
  std::vector<std::pair<arangodb::application_features::ApplicationFeature*, bool>> features;
  std::string testFilesystemPath;

  IResearchLinkSetup(): engine(server), server(nullptr, nullptr) {
    arangodb::EngineSelectorFeature::ENGINE = &engine;

    arangodb::tests::init();

    // suppress INFO {authentication} Authentication is turned on (system only), authentication for unix sockets is turned on
    arangodb::LogTopic::setLogLevel(arangodb::Logger::AUTHENTICATION.name(), arangodb::LogLevel::WARN);

    // suppress log messages since tests check error conditions
    arangodb::LogTopic::setLogLevel(arangodb::Logger::ENGINES.name(), arangodb::LogLevel::FATAL);
    arangodb::LogTopic::setLogLevel(arangodb::Logger::FIXME.name(), arangodb::LogLevel::FATAL);
    arangodb::LogTopic::setLogLevel(arangodb::iresearch::TOPIC.name(), arangodb::LogLevel::FATAL);
    irs::logger::output_le(iresearch::logger::IRL_FATAL, stderr);

    // setup required application features
    features.emplace_back(new arangodb::AqlFeature(server), true); // required for UserManager::loadFromDB()
    features.emplace_back(new arangodb::AuthenticationFeature(server), true);
    features.emplace_back(new arangodb::DatabaseFeature(server), false);
    features.emplace_back(new arangodb::ShardingFeature(server), false);
    features.emplace_back(new arangodb::ViewTypesFeature(server), true);
    features.emplace_back(new arangodb::QueryRegistryFeature(server), false);
    arangodb::application_features::ApplicationServer::server->addFeature(features.back().first); // need QueryRegistryFeature feature to be added now in order to create the system database
    features.emplace_back(new arangodb::SystemDatabaseFeature(server), true); // required for IResearchAnalyzerFeature
    features.emplace_back(new arangodb::TraverserEngineRegistryFeature(server), false); // required for AqlFeature::stop()
    features.emplace_back(new arangodb::DatabasePathFeature(server), false);
    features.emplace_back(new arangodb::aql::AqlFunctionFeature(server), true); // required for IResearchAnalyzerFeature
    features.emplace_back(new arangodb::iresearch::IResearchAnalyzerFeature(server), true);
    features.emplace_back(new arangodb::iresearch::IResearchFeature(server), true);
    features.emplace_back(new arangodb::FlushFeature(server), false); // do not start the thread
    features.emplace_back(new arangodb::V8DealerFeature(server), false); // required for DatabaseFeature::createDatabase(...)

    #if USE_ENTERPRISE
      features.emplace_back(new arangodb::LdapFeature(server), false); // required for AuthenticationFeature with USE_ENTERPRISE
    #endif

    // required for V8DealerFeature::prepare(), ClusterFeature::prepare() not required
    arangodb::application_features::ApplicationServer::server->addFeature(
      new arangodb::ClusterFeature(server)
    );

    for (auto& f : features) {
      arangodb::application_features::ApplicationServer::server->addFeature(f.first);
    }

    for (auto& f : features) {
      f.first->prepare();
    }

    auto const databases = arangodb::velocypack::Parser::fromJson(std::string("[ { \"name\": \"") + arangodb::StaticStrings::SystemDatabase + "\" } ]");
    auto* dbFeature = arangodb::application_features::ApplicationServer::lookupFeature<
      arangodb::DatabaseFeature
    >("Database");
    dbFeature->loadDatabases(databases->slice());

    for (auto& f : features) {
      if (f.second) {
        f.first->start();
      }
    }

    TransactionStateMock::abortTransactionCount = 0;
    TransactionStateMock::beginTransactionCount = 0;
    TransactionStateMock::commitTransactionCount = 0;

    auto* dbPathFeature = arangodb::application_features::ApplicationServer::getFeature<arangodb::DatabasePathFeature>("DatabasePath");
    arangodb::tests::setDatabasePath(*dbPathFeature); // ensure test data is stored in a unique directory
    testFilesystemPath = dbPathFeature->directory();

    long systemError;
    std::string systemErrorStr;
    TRI_CreateDirectory(testFilesystemPath.c_str(), systemError, systemErrorStr);
  }

  ~IResearchLinkSetup() {
    arangodb::application_features::ApplicationServer::lookupFeature<arangodb::SystemDatabaseFeature>()->unprepare(); // release system database before reseting the 'ENGINE'
    TRI_RemoveDirectory(testFilesystemPath.c_str());
    arangodb::LogTopic::setLogLevel(arangodb::iresearch::TOPIC.name(), arangodb::LogLevel::DEFAULT);
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

    arangodb::LogTopic::setLogLevel(arangodb::Logger::FIXME.name(), arangodb::LogLevel::DEFAULT);
    arangodb::LogTopic::setLogLevel(arangodb::Logger::ENGINES.name(), arangodb::LogLevel::DEFAULT);
    arangodb::LogTopic::setLogLevel(arangodb::Logger::AUTHENTICATION.name(), arangodb::LogLevel::DEFAULT);
    arangodb::EngineSelectorFeature::ENGINE = nullptr;
  }
};

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief setup
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("IResearchLinkTest", "[iresearch][iresearch-link]") {
  IResearchLinkSetup s;
  UNUSED(s);

SECTION("test_defaults") {
  // no view specified
  {
    s.engine.views.clear();
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    REQUIRE((nullptr != logicalCollection));
    auto json = arangodb::velocypack::Parser::fromJson("{}");
    std::shared_ptr<arangodb::Index> link;
    CHECK((TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND == arangodb::iresearch::IResearchMMFilesLink::factory().instantiate(link, *logicalCollection, json->slice(), 1, false).errorNumber()));
    CHECK((true == !link));
  }

  // no view can be found (e.g. db-server coming up with view not available from Agency yet)
  {
    s.engine.views.clear();
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    REQUIRE((nullptr != logicalCollection));
    auto json = arangodb::velocypack::Parser::fromJson("{ \"view\": \"42\" }");
    std::shared_ptr<arangodb::Index> link;
    CHECK((arangodb::iresearch::IResearchMMFilesLink::factory().instantiate(link, *logicalCollection, json->slice(), 1, false).ok()));
    CHECK((false == !link));
  }

  // valid link creation
  {
    s.engine.views.clear();
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto linkJson = arangodb::velocypack::Parser::fromJson("{ \"type\": \"arangosearch\", \"view\": \"42\" }");
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
    auto viewJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"id\": 42, \"type\": \"arangosearch\" }");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    REQUIRE((nullptr != logicalCollection));
    auto logicalView = vocbase.createView(viewJson->slice());
    REQUIRE((false == !logicalView));

    bool created;
    auto link = logicalCollection->createIndex(linkJson->slice(), created);
    REQUIRE((false == !link && created));
    CHECK((true == link->canBeDropped()));
    CHECK((logicalCollection.get() == &(link->collection())));
    CHECK((link->fieldNames().empty()));
    CHECK((link->fields().empty()));
    CHECK((true == link->hasBatchInsert()));
    CHECK((false == link->hasExpansion()));
    CHECK((false == link->hasSelectivityEstimate()));
    CHECK((false == link->implicitlyUnique()));
    CHECK((false == link->isSorted()));
    CHECK((0 < link->memory()));
    CHECK((true == link->sparse()));
    CHECK((arangodb::Index::IndexType::TRI_IDX_TYPE_IRESEARCH_LINK == link->type()));
    CHECK((arangodb::iresearch::DATA_SOURCE_TYPE.name() == link->typeName()));
    CHECK((false == link->unique()));

    arangodb::iresearch::IResearchLinkMeta actualMeta;
    arangodb::iresearch::IResearchLinkMeta expectedMeta;
    auto builder = link->toVelocyPack(arangodb::Index::makeFlags(arangodb::Index::Serialize::Figures));
    std::string error;

    CHECK((actualMeta.init(builder->slice(), error) && expectedMeta == actualMeta));
    auto slice = builder->slice();
    CHECK((
      slice.hasKey("view")
      && slice.get("view").isString()
      && logicalView->guid() == slice.get("view").copyString()
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
    auto linkJson = arangodb::velocypack::Parser::fromJson("{ \"type\": \"arangosearch\", \"view\": \"42\" }");
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
    auto viewJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"id\": 42, \"type\": \"arangosearch\" }");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    REQUIRE((nullptr != logicalCollection));
    auto logicalView = vocbase.createView(viewJson->slice());
    REQUIRE((false == !logicalView));

    bool created;
    auto link = logicalCollection->createIndex(linkJson->slice(), created);
    REQUIRE((false == !link && created));
    CHECK((true == link->canBeDropped()));
    CHECK((logicalCollection.get() == &(link->collection())));
    CHECK((link->fieldNames().empty()));
    CHECK((link->fields().empty()));
    CHECK((true == link->hasBatchInsert()));
    CHECK((false == link->hasExpansion()));
    CHECK((false == link->hasSelectivityEstimate()));
    CHECK((false == link->implicitlyUnique()));
    CHECK((false == link->isSorted()));
    CHECK((0 < link->memory()));
    CHECK((true == link->sparse()));
    CHECK((arangodb::Index::IndexType::TRI_IDX_TYPE_IRESEARCH_LINK == link->type()));
    CHECK((arangodb::iresearch::DATA_SOURCE_TYPE.name() == link->typeName()));
    CHECK((false == link->unique()));

    {
      arangodb::iresearch::IResearchLinkMeta actualMeta;
      arangodb::iresearch::IResearchLinkMeta expectedMeta;
      auto builder = link->toVelocyPack(arangodb::Index::makeFlags(arangodb::Index::Serialize::Figures));
      std::string error;

      CHECK((actualMeta.init(builder->slice(), error) && expectedMeta == actualMeta));
      auto slice = builder->slice();
      CHECK((
        slice.hasKey("view")
        && slice.get("view").isString()
        && logicalView->guid() == slice.get("view").copyString()
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
      auto builder = link->toVelocyPack(arangodb::Index::makeFlags(arangodb::Index::Serialize::Figures));
      auto slice = builder->slice();
      CHECK((
        slice.hasKey("view")
        && slice.get("view").isString()
        && logicalView->guid() == slice.get("view").copyString()
        && slice.hasKey("figures")
        && slice.get("figures").isObject()
        && slice.get("figures").hasKey("memory")
        && slice.get("figures").get("memory").isNumber()
        && 0 < slice.get("figures").get("memory").getUInt()
      ));
    }
  }
}

SECTION("test_flush_marker") {
  static std::vector<std::string> const EMPTY;
  auto doc0 = arangodb::velocypack::Parser::fromJson("{ \"abc\": \"def\" }");
  auto doc1 = arangodb::velocypack::Parser::fromJson("{ \"ghi\": \"jkl\" }");
  auto doc2 = arangodb::velocypack::Parser::fromJson("{ \"mno\": \"pqr\" }");

  auto before = StorageEngineMock::inRecoveryResult;
  StorageEngineMock::inRecoveryResult = true;
  auto restore = irs::make_finally([&before]()->void { StorageEngineMock::inRecoveryResult = before; });

  auto* dbFeature = arangodb::application_features::ApplicationServer::lookupFeature<arangodb::DatabaseFeature>("Database");
  REQUIRE((dbFeature));
  TRI_vocbase_t* vocbase;
  REQUIRE((TRI_ERROR_NO_ERROR == dbFeature->createDatabase(1, "testDatabase", vocbase)));
  auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\", \"id\": 100 }");
  auto linkJson = arangodb::velocypack::Parser::fromJson("{ \"id\": 42, \"includeAllFields\": true, \"type\": \"arangosearch\", \"view\": \"testView\" }");
  auto logicalCollection = vocbase->createCollection(collectionJson->slice());
  REQUIRE((false == !logicalCollection));
  bool created;
  auto index = logicalCollection->createIndex(linkJson->slice(), created);
  REQUIRE((false == !index && created));
  auto link = std::dynamic_pointer_cast<arangodb::iresearch::IResearchLink>(index);
  REQUIRE((false == !link));

  // recovery non-object
  {
    auto json = arangodb::velocypack::Parser::fromJson("{ \"type\": \"arangosearch\", \"data\": [] }");
    std::basic_string<uint8_t> buf;
    buf.resize(sizeof(::MMFilesMarker) + sizeof(TRI_voc_tick_t)); // reserve space for header
    arangodb::encoding::storeNumber(&buf[sizeof(::MMFilesMarker)], TRI_voc_tick_t(1), sizeof(TRI_voc_tick_t));
    buf.append(json->slice().begin(), json->slice().byteSize());
    auto* marker = reinterpret_cast<MMFilesMarker*>(&buf[0]);
    marker->setSize(buf.size());
    marker->setType(::MMFilesMarkerType::TRI_DF_MARKER_VPACK_FLUSH_SYNC);
    arangodb::MMFilesWalRecoverState state(false);
    CHECK((0 == state.errorCount));
    CHECK((arangodb::MMFilesWalRecoverState::ReplayMarker(marker, &state, nullptr)));
    CHECK((1 == state.errorCount));
  }

  // recovery no collection
  {
    auto json = arangodb::velocypack::Parser::fromJson("{ \"type\": \"arangosearch\", \"data\": { \"iid\": 52 } }");
    std::basic_string<uint8_t> buf;
    buf.resize(sizeof(::MMFilesMarker) + sizeof(TRI_voc_tick_t)); // reserve space for header
    arangodb::encoding::storeNumber(&buf[sizeof(::MMFilesMarker)], TRI_voc_tick_t(1), sizeof(TRI_voc_tick_t));
    buf.append(json->slice().begin(), json->slice().byteSize());
    auto* marker = reinterpret_cast<MMFilesMarker*>(&buf[0]);
    marker->setSize(buf.size());
    marker->setType(::MMFilesMarkerType::TRI_DF_MARKER_VPACK_FLUSH_SYNC);
    arangodb::MMFilesWalRecoverState state(false);
    CHECK((0 == state.errorCount));
    CHECK((arangodb::MMFilesWalRecoverState::ReplayMarker(marker, &state, nullptr)));
    CHECK((1 == state.errorCount));
  }

  // recovery no index
  {
    auto json = arangodb::velocypack::Parser::fromJson("{ \"type\": \"arangosearch\", \"data\": { \"cid\": 42 } }");
    std::basic_string<uint8_t> buf;
    buf.resize(sizeof(::MMFilesMarker) + sizeof(TRI_voc_tick_t)); // reserve space for header
    arangodb::encoding::storeNumber(&buf[sizeof(::MMFilesMarker)], TRI_voc_tick_t(1), sizeof(TRI_voc_tick_t));
    buf.append(json->slice().begin(), json->slice().byteSize());
    auto* marker = reinterpret_cast<MMFilesMarker*>(&buf[0]);
    marker->setSize(buf.size());
    marker->setType(::MMFilesMarkerType::TRI_DF_MARKER_VPACK_FLUSH_SYNC);
    arangodb::MMFilesWalRecoverState state(false);
    CHECK((0 == state.errorCount));
    CHECK((arangodb::MMFilesWalRecoverState::ReplayMarker(marker, &state, nullptr)));
    CHECK((1 == state.errorCount));
  }

  // recovery missing collection
  {
    auto json = arangodb::velocypack::Parser::fromJson("{ \"type\": \"arangosearch\", \"data\": { \"cid\": 42, \"iid\": 52 } }");
    std::basic_string<uint8_t> buf;
    buf.resize(sizeof(::MMFilesMarker) + sizeof(TRI_voc_tick_t)); // reserve space for header
    arangodb::encoding::storeNumber(&buf[sizeof(::MMFilesMarker)], TRI_voc_tick_t(1), sizeof(TRI_voc_tick_t));
    buf.append(json->slice().begin(), json->slice().byteSize());
    auto* marker = reinterpret_cast<MMFilesMarker*>(&buf[0]);
    marker->setSize(buf.size());
    marker->setType(::MMFilesMarkerType::TRI_DF_MARKER_VPACK_FLUSH_SYNC);
    arangodb::MMFilesWalRecoverState state(false);
    CHECK((0 == state.errorCount));
    CHECK((arangodb::MMFilesWalRecoverState::ReplayMarker(marker, &state, nullptr)));
    CHECK((0 == state.errorCount)); // missing collection treated as a removed collection (after the WAL marker)
  }

  // recovery missing link
  {
    auto json = arangodb::velocypack::Parser::fromJson("{ \"type\": \"arangosearch\", \"data\": { \"cid\": 100, \"iid\": 24 } }");
    std::basic_string<uint8_t> buf;
    buf.resize(sizeof(::MMFilesMarker) + sizeof(TRI_voc_tick_t)); // reserve space for header
    arangodb::encoding::storeNumber(&buf[sizeof(::MMFilesMarker)], TRI_voc_tick_t(1), sizeof(TRI_voc_tick_t));
    buf.append(json->slice().begin(), json->slice().byteSize());
    auto* marker = reinterpret_cast<MMFilesMarker*>(&buf[0]);
    marker->setSize(buf.size());
    marker->setType(::MMFilesMarkerType::TRI_DF_MARKER_VPACK_FLUSH_SYNC);
    arangodb::MMFilesWalRecoverState state(false);
    CHECK((0 == state.errorCount));
    CHECK((arangodb::MMFilesWalRecoverState::ReplayMarker(marker, &state, nullptr)));
    CHECK((0 == state.errorCount)); // missing link treated as a removed index (after the WAL marker)
  }

  // recovery non-string value
  {
    auto json = arangodb::velocypack::Parser::fromJson("{ \"type\": \"arangosearch\", \"data\": { \"cid\": 100, \"iid\": 42, \"value\": [] } }");
    std::basic_string<uint8_t> buf;
    buf.resize(sizeof(::MMFilesMarker) + sizeof(TRI_voc_tick_t)); // reserve space for header
    arangodb::encoding::storeNumber(&buf[sizeof(::MMFilesMarker)], TRI_voc_tick_t(1), sizeof(TRI_voc_tick_t));
    buf.append(json->slice().begin(), json->slice().byteSize());
    auto* marker = reinterpret_cast<MMFilesMarker*>(&buf[0]);
    marker->setSize(buf.size());
    marker->setType(::MMFilesMarkerType::TRI_DF_MARKER_VPACK_FLUSH_SYNC);
    arangodb::MMFilesWalRecoverState state(false);
    CHECK((0 == state.errorCount));
    CHECK((arangodb::MMFilesWalRecoverState::ReplayMarker(marker, &state, nullptr)));
    CHECK((1 == state.errorCount));
  }

  // recovery non-recovery state (i.e. recovery marker after end of recovery)
  {
    auto before = StorageEngineMock::inRecoveryResult;
    StorageEngineMock::inRecoveryResult = false;
    auto restore = irs::make_finally([&before]()->void { StorageEngineMock::inRecoveryResult = before; });
    auto linkJson0 = arangodb::velocypack::Parser::fromJson("{ \"id\": 41, \"includeAllFields\": true, \"type\": \"arangosearch\", \"view\": \"testView\" }");
    bool created;
    auto index0 = logicalCollection->createIndex(linkJson0->slice(), created);
    CHECK((false == !index0 && created));
    auto link0 = std::dynamic_pointer_cast<arangodb::iresearch::IResearchLink>(index0);
    CHECK((false == !link0));

    auto json = arangodb::velocypack::Parser::fromJson("{ \"type\": \"arangosearch\", \"data\": { \"cid\": 100, \"iid\": 41, \"value\": \"segments_41\" } }");
    std::basic_string<uint8_t> buf;
    buf.resize(sizeof(::MMFilesMarker) + sizeof(TRI_voc_tick_t)); // reserve space for header
    arangodb::encoding::storeNumber(&buf[sizeof(::MMFilesMarker)], TRI_voc_tick_t(1), sizeof(TRI_voc_tick_t));
    buf.append(json->slice().begin(), json->slice().byteSize());
    auto* marker = reinterpret_cast<MMFilesMarker*>(&buf[0]);
    marker->setSize(buf.size());
    marker->setType(::MMFilesMarkerType::TRI_DF_MARKER_VPACK_FLUSH_SYNC);
    arangodb::MMFilesWalRecoverState state(false);
    CHECK((0 == state.errorCount));
    CHECK((arangodb::MMFilesWalRecoverState::ReplayMarker(marker, &state, nullptr)));
    CHECK((1 == state.errorCount));
  }

  // recovery success
  {
    auto json = arangodb::velocypack::Parser::fromJson("{ \"type\": \"arangosearch\", \"data\": { \"cid\": 100, \"iid\": 42, \"value\": \"segments_42\" } }");
    std::basic_string<uint8_t> buf;
    buf.resize(sizeof(::MMFilesMarker) + sizeof(TRI_voc_tick_t)); // reserve space for header
    arangodb::encoding::storeNumber(&buf[sizeof(::MMFilesMarker)], TRI_voc_tick_t(1), sizeof(TRI_voc_tick_t));
    buf.append(json->slice().begin(), json->slice().byteSize());
    auto* marker = reinterpret_cast<MMFilesMarker*>(&buf[0]);
    marker->setSize(buf.size());
    marker->setType(::MMFilesMarkerType::TRI_DF_MARKER_VPACK_FLUSH_SYNC);
    arangodb::MMFilesWalRecoverState state(false);
    CHECK((0 == state.errorCount));
    CHECK((arangodb::MMFilesWalRecoverState::ReplayMarker(marker, &state, nullptr)));
    CHECK((0 == state.errorCount));
  }

  // open existing without checkpoint file
  {
    auto linkJson1 = arangodb::velocypack::Parser::fromJson("{ \"id\": 43, \"includeAllFields\": true, \"type\": \"arangosearch\", \"view\": \"testView\" }");

    // initial population of link
    {
      std::shared_ptr<arangodb::Index> index1;
      CHECK((arangodb::iresearch::IResearchMMFilesLink::factory().instantiate(index1, *logicalCollection, linkJson1->slice(), 43, false).ok()));
      CHECK((false == !index1));
      auto link1 = std::dynamic_pointer_cast<arangodb::iresearch::IResearchLink>(index1);
      CHECK((false == !link1));

      arangodb::transaction::Methods trx(
        arangodb::transaction::StandaloneContext::Create(*vocbase),
        EMPTY,
        EMPTY,
        EMPTY,
        arangodb::transaction::Options()
      );
      CHECK((trx.begin().ok()));
      CHECK((link1->insert(trx, arangodb::LocalDocumentId(1), doc0->slice(), arangodb::Index::OperationMode::normal).ok()));
      CHECK((trx.commit().ok()));
      auto before = StorageEngineMock::inRecoveryResult;
      StorageEngineMock::inRecoveryResult = false;
      auto restore = irs::make_finally([&before]()->void { StorageEngineMock::inRecoveryResult = before; });
      dbFeature->recoveryDone(); // will commit 'link1' (it will also commit 'link' and set RecoveryState to DONE)
    }

    irs::utf8_path path;
    path /= s.testFilesystemPath;
    path /= "databases";
    path /= std::string("database-") + std::to_string(vocbase->id());
    path /= std::string("arangosearch-") + std::to_string(logicalCollection->id()) + "_43";
    irs::fs_directory dir(path.utf8());
    auto reader = irs::directory_reader::open(dir);
    path /= reader.meta().filename  + ".checkpoint";
    bool exists;
    CHECK((path.exists_file(exists) && exists));
    CHECK((path.remove()));
    auto index1 = logicalCollection->createIndex(linkJson1->slice(), created);
    CHECK((true == !index1));
  }

  // open exisitng with checkpoint file unmatched by marker (missing WAL tail)
  {
    auto linkJson1 = arangodb::velocypack::Parser::fromJson("{ \"id\": 44, \"includeAllFields\": true, \"type\": \"arangosearch\", \"view\": \"testView\" }");

    // initial population of link
    {
      auto before = StorageEngineMock::inRecoveryResult;
      StorageEngineMock::inRecoveryResult = false;
      auto restore = irs::make_finally([&before]()->void { StorageEngineMock::inRecoveryResult = before; });
      std::shared_ptr<arangodb::Index> index1;
      CHECK((arangodb::iresearch::IResearchMMFilesLink::factory().instantiate(index1, *logicalCollection, linkJson1->slice(), 44, false).ok()));
      CHECK((false == !index1));
      auto link1 = std::dynamic_pointer_cast<arangodb::iresearch::IResearchLink>(index1);
      CHECK((false == !link1));

      // insert doc0
      {
        arangodb::transaction::Methods trx(
          arangodb::transaction::StandaloneContext::Create(*vocbase),
          EMPTY,
          EMPTY,
          EMPTY,
          arangodb::transaction::Options()
        );
        CHECK((trx.begin().ok()));
        CHECK((link1->insert(trx, arangodb::LocalDocumentId(1), doc0->slice(), arangodb::Index::OperationMode::normal).ok()));
        CHECK((trx.commit().ok()));
        CHECK((link1->commit().ok()));
      }
    }

    auto index1 = logicalCollection->createIndex(linkJson1->slice(), created);
    CHECK((false == !index1)); // link creation success in recovery
    auto before = StorageEngineMock::inRecoveryResult;
    StorageEngineMock::inRecoveryResult = false;
    auto restore = irs::make_finally([&before]()->void { StorageEngineMock::inRecoveryResult = before; });
    CHECK_THROWS((dbFeature->recoveryDone())); // but recovery will fail to finish
  }

  // commit failed write WAL
  {
    auto before = StorageEngineMock::flushSubscriptionResult;
    auto restore = irs::make_finally([&before]()->void { StorageEngineMock::flushSubscriptionResult = before; });
    StorageEngineMock::flushSubscriptionResult = arangodb::Result(TRI_ERROR_INTERNAL);

    arangodb::transaction::Methods trx(
      arangodb::transaction::StandaloneContext::Create(*vocbase),
      EMPTY,
      EMPTY,
      EMPTY,
      arangodb::transaction::Options()
    );
    CHECK((trx.begin().ok()));
    CHECK((link->insert(trx, arangodb::LocalDocumentId(1), doc0->slice(), arangodb::Index::OperationMode::normal).ok()));
    CHECK((trx.commit().ok()));
    CHECK((!link->commit().ok()));
  }

  // commit failed write checkpoint
  {
    arangodb::transaction::Methods trx(
      arangodb::transaction::StandaloneContext::Create(*vocbase),
      EMPTY,
      EMPTY,
      EMPTY,
      arangodb::transaction::Options()
    );
    CHECK((trx.begin().ok()));
    CHECK((link->insert(trx, arangodb::LocalDocumentId(2), doc1->slice(), arangodb::Index::OperationMode::normal).ok()));
    CHECK((trx.commit().ok()));
    irs::utf8_path path;
    path /= s.testFilesystemPath;
    path /= "databases";
    path /= std::string("database-") + std::to_string(vocbase->id());
    path /= std::string("arangosearch-") + std::to_string(logicalCollection->id()) + "_" + std::to_string(link->id());
    path /= "segments_3.checkpoint";
    CHECK((path.mkdir())); //create a directory by same name as the checkpoint file to force error
    CHECK((!link->commit().ok()));
  }

  // commit success
  {
    arangodb::transaction::Methods trx(
      arangodb::transaction::StandaloneContext::Create(*vocbase),
      EMPTY,
      EMPTY,
      EMPTY,
      arangodb::transaction::Options()
    );
    CHECK((trx.begin().ok()));
    CHECK((link->insert(trx, arangodb::LocalDocumentId(3), doc2->slice(), arangodb::Index::OperationMode::normal).ok()));
    CHECK((trx.commit().ok()));
    CHECK((link->commit().ok()));
  }
}

SECTION("test_init") {
  // collection registered with view (collection initially not in view)
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\", \"id\": 100 }");
    auto linkJson = arangodb::velocypack::Parser::fromJson("{ \"type\": \"arangosearch\", \"view\": \"42\" }");
    auto viewJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"id\": 42, \"type\": \"arangosearch\" }");
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    REQUIRE((nullptr != logicalCollection));
    auto logicalView = vocbase.createView(viewJson->slice());
    REQUIRE((false == !logicalView));

    // no collections in view before
    {
      std::unordered_set<TRI_voc_cid_t> expected;
      std::set<TRI_voc_cid_t> actual;

      CHECK((logicalView->visitCollections([&actual](TRI_voc_cid_t cid)->bool { actual.emplace(cid); return true; })));

      for (auto& cid: expected) {
        CHECK((1 == actual.erase(cid)));
      }

      CHECK((actual.empty()));
    }

    std::shared_ptr<arangodb::Index> link;
    CHECK((arangodb::iresearch::IResearchMMFilesLink::factory().instantiate(link, *logicalCollection, linkJson->slice(), 1, false).ok()));
    CHECK((false == !link));

    // collection in view after
    {
      std::unordered_set<TRI_voc_cid_t> expected = { 100 };
      std::set<TRI_voc_cid_t> actual;

      CHECK((logicalView->visitCollections([&actual](TRI_voc_cid_t cid)->bool { actual.emplace(cid); return true; })));

      for (auto& cid: expected) {
        CHECK((1 == actual.erase(cid)));
      }

      CHECK((actual.empty()));
    }

    link.reset();

    // collection in view on destruct
    {
      std::unordered_set<TRI_voc_cid_t> expected = { 100 };
      std::set<TRI_voc_cid_t> actual;

      CHECK((logicalView->visitCollections([&actual](TRI_voc_cid_t cid)->bool { actual.emplace(cid); return true; })));

      for (auto& cid: expected) {
        CHECK((1 == actual.erase(cid)));
      }

      CHECK((actual.empty()));
    }
  }

  // collection registered with view (collection initially is in view)
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\", \"id\": 101 }");
    auto linkJson = arangodb::velocypack::Parser::fromJson("{ \"type\": \"arangosearch\", \"view\": \"43\" }");
    auto viewJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"id\": 43, \"type\": \"arangosearch\", \"collections\": [ 101 ] }");
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    REQUIRE((nullptr != logicalCollection));
    auto logicalView = vocbase.createView(viewJson->slice());
    REQUIRE((false == !logicalView));

    // collection in view before
    {
      std::unordered_set<TRI_voc_cid_t> expected = { 101 };
      std::set<TRI_voc_cid_t> actual;

      CHECK((logicalView->visitCollections([&actual](TRI_voc_cid_t cid)->bool { actual.emplace(cid); return true; })));

      for (auto& cid: expected) {
        CHECK((1 == actual.erase(cid)));
      }

      CHECK((actual.empty()));
    }

    std::shared_ptr<arangodb::Index> link;
    CHECK((arangodb::iresearch::IResearchMMFilesLink::factory().instantiate(link, *logicalCollection, linkJson->slice(), 1, false).ok()));
    CHECK((false == !link));

    // collection in view after
    {
      std::unordered_set<TRI_voc_cid_t> expected = { 101 };
      std::set<TRI_voc_cid_t> actual;

      CHECK((logicalView->visitCollections([&actual](TRI_voc_cid_t cid)->bool { actual.emplace(cid); return true; })));

      for (auto& cid: expected) {
        CHECK((1 == actual.erase(cid)));
      }

      CHECK((actual.empty()));
    }

    link.reset();

    // collection still in view on destruct
    {
      std::unordered_set<TRI_voc_cid_t> expected = { 101 };
      std::set<TRI_voc_cid_t> actual;

      CHECK((logicalView->visitCollections([&actual](TRI_voc_cid_t cid)->bool { actual.emplace(cid); return true; })));

      for (auto& cid: expected) {
        CHECK((1 == actual.erase(cid)));
      }

      CHECK((actual.empty()));
    }
  }
}

SECTION("test_self_token") {
  // test empty token
  {
    arangodb::iresearch::IResearchLink::AsyncLinkPtr::element_type empty(nullptr);
    CHECK((nullptr == empty.get()));
  }

  arangodb::iresearch::IResearchLink::AsyncLinkPtr self;

  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
    auto linkJson = arangodb::velocypack::Parser::fromJson("{ \"view\": \"testView\" }");
    auto viewJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\":\"arangosearch\" }");
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    REQUIRE((false == !logicalCollection));
    auto logicalView = vocbase.createView(viewJson->slice());
    CHECK((false == !logicalView));
    std::shared_ptr<arangodb::Index> index;
    REQUIRE((arangodb::iresearch::IResearchMMFilesLink::factory().instantiate(index, *logicalCollection, linkJson->slice(), 42, false).ok()));
    REQUIRE((false == !index));
    auto link = std::dynamic_pointer_cast<arangodb::iresearch::IResearchLink>(index);
    REQUIRE((false == !link));
    self = link->self();
    CHECK((false == !self));
    CHECK((link.get() == self->get()));
  }

  CHECK((false == !self));
  CHECK((nullptr == self->get()));
}

SECTION("test_drop") {
  // collection drop (removes collection from view) subsequent destroy does not touch view
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\", \"id\": 100 }");
    auto linkJson = arangodb::velocypack::Parser::fromJson("{ \"type\": \"arangosearch\", \"view\": \"42\" }");
    auto viewJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"id\": 42, \"type\": \"arangosearch\" }");
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    REQUIRE((nullptr != logicalCollection));
    auto logicalView = vocbase.createView(viewJson->slice());
    REQUIRE((false == !logicalView));

    std::shared_ptr<arangodb::Index> link0;
    CHECK((arangodb::iresearch::IResearchMMFilesLink::factory().instantiate(link0, *logicalCollection, linkJson->slice(), 1, false).ok()));
    CHECK((false == !link0));

    // collection in view before
    {
      std::unordered_set<TRI_voc_cid_t> expected = { 100 };
      std::set<TRI_voc_cid_t> actual;

      CHECK((logicalView->visitCollections([&actual](TRI_voc_cid_t cid)->bool { actual.emplace(cid); return true; })));

      for (auto& cid: expected) {
        CHECK((1 == actual.erase(cid)));
      }

      CHECK((actual.empty()));
    }

    CHECK((true == (*dynamic_cast<arangodb::iresearch::IResearchLink*>(link0.get()) == *logicalView)));
    CHECK((link0->drop().ok()));
    CHECK((true == (*dynamic_cast<arangodb::iresearch::IResearchLink*>(link0.get()) == *logicalView)));

    // collection not in view after
    {
      std::unordered_set<TRI_voc_cid_t> expected;
      std::set<TRI_voc_cid_t> actual;

      CHECK((logicalView->visitCollections([&actual](TRI_voc_cid_t cid)->bool { actual.emplace(cid); return true; })));

      for (auto& cid: expected) {
        CHECK((1 == actual.erase(cid)));
      }

      CHECK((actual.empty()));
    }

    std::shared_ptr<arangodb::Index> link1;
    CHECK((arangodb::iresearch::IResearchMMFilesLink::factory().instantiate(link1, *logicalCollection, linkJson->slice(), 1, false).ok()));
    CHECK((false == !link1));

    // collection in view before (new link)
    {
      std::unordered_set<TRI_voc_cid_t> expected = { 100 };
      std::set<TRI_voc_cid_t> actual;

      CHECK((logicalView->visitCollections([&actual](TRI_voc_cid_t cid)->bool { actual.emplace(cid); return true; })));

      for (auto& cid: expected) {
        CHECK((1 == actual.erase(cid)));
      }

      CHECK((actual.empty()));
    }

    link0.reset();

    // collection in view after (new link) subsequent destroy does not touch view
    {
      std::unordered_set<TRI_voc_cid_t> expected = { 100 };
      std::set<TRI_voc_cid_t> actual;

      CHECK((logicalView->visitCollections([&actual](TRI_voc_cid_t cid)->bool { actual.emplace(cid); return true; })));

      for (auto& cid: expected) {
        CHECK((1 == actual.erase(cid)));
      }

      CHECK((actual.empty()));
    }
  }
}

SECTION("test_unload") {
  // index unload does not touch the view (subsequent destroy)
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\", \"id\": 100 }");
    auto linkJson = arangodb::velocypack::Parser::fromJson("{ \"type\": \"arangosearch\", \"view\": \"42\" }");
    auto viewJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"id\": 42, \"type\": \"arangosearch\" }");
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    REQUIRE((nullptr != logicalCollection));
    auto logicalView = vocbase.createView(viewJson->slice());
    REQUIRE((false == !logicalView));

    std::shared_ptr<arangodb::Index> link;
    CHECK((arangodb::iresearch::IResearchMMFilesLink::factory().instantiate(link, *logicalCollection, linkJson->slice(), 1, false).ok()));
    CHECK((false == !link));

    // collection in view before
    {
      std::unordered_set<TRI_voc_cid_t> expected = { 100 };
      std::set<TRI_voc_cid_t> actual;

      CHECK((logicalView->visitCollections([&actual](TRI_voc_cid_t cid)->bool { actual.emplace(cid); return true; })));

      for (auto& cid: expected) {
        CHECK((1 == actual.erase(cid)));
      }

      CHECK((actual.empty()));
    }

    CHECK((true == (*dynamic_cast<arangodb::iresearch::IResearchLink*>(link.get()) == *logicalView)));
    link->unload();
    CHECK((true == (*dynamic_cast<arangodb::iresearch::IResearchLink*>(link.get()) == *logicalView)));

    // collection in view after unload
    {
      std::unordered_set<TRI_voc_cid_t> expected = { 100 };
      std::set<TRI_voc_cid_t> actual;

      CHECK((logicalView->visitCollections([&actual](TRI_voc_cid_t cid)->bool { actual.emplace(cid); return true; })));

      for (auto& cid: expected) {
        CHECK((1 == actual.erase(cid)));
      }

      CHECK((actual.empty()));
    }

    link.reset();

    // collection in view after destruct, subsequent destroy does not touch view
    {
      std::unordered_set<TRI_voc_cid_t> expected = { 100 };
      std::set<TRI_voc_cid_t> actual;

      CHECK((logicalView->visitCollections([&actual](TRI_voc_cid_t cid)->bool { actual.emplace(cid); return true; })));

      for (auto& cid: expected) {
        CHECK((1 == actual.erase(cid)));
      }

      CHECK((actual.empty()));
    }
  }
}

SECTION("test_write") {
  static std::vector<std::string> const EMPTY;
  auto doc0 = arangodb::velocypack::Parser::fromJson("{ \"abc\": \"def\" }");
  auto doc1 = arangodb::velocypack::Parser::fromJson("{ \"ghi\": \"jkl\" }");
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
  std::string dataPath = ((((irs::utf8_path()/=s.testFilesystemPath)/=std::string("databases"))/=(std::string("database-") + std::to_string(vocbase.id())))/=std::string("arangosearch-42")).utf8();
  auto linkJson = arangodb::velocypack::Parser::fromJson("{ \"id\": 42, \"type\": \"arangosearch\", \"view\": \"42\", \"includeAllFields\": true }");
  auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
  auto viewJson = arangodb::velocypack::Parser::fromJson("{ \
    \"id\": 42, \
    \"name\": \"testView\", \
    \"type\": \"arangosearch\" \
  }");
  auto logicalCollection = vocbase.createCollection(collectionJson->slice());
  REQUIRE((nullptr != logicalCollection));
  auto view = std::dynamic_pointer_cast<arangodb::iresearch::IResearchView>(
    vocbase.createView(viewJson->slice())
  );
  REQUIRE((false == !view));
  view->open();
  auto* flush = arangodb::application_features::ApplicationServer::lookupFeature<
    arangodb::FlushFeature
  >("Flush");
  REQUIRE((flush));

  dataPath = ((((irs::utf8_path()/=s.testFilesystemPath)/=std::string("databases"))/=(std::string("database-") + std::to_string(vocbase.id())))/=(std::string("arangosearch-") + std::to_string(logicalCollection->id()) + "_42")).utf8();
  irs::fs_directory directory(dataPath);
  bool created;
  auto link = logicalCollection->createIndex(linkJson->slice(), created);
  REQUIRE((false == !link && created));
  auto reader = irs::directory_reader::open(directory);
  CHECK((0 == reader.reopen().live_docs_count()));
  {
    arangodb::transaction::Methods trx(
      arangodb::transaction::StandaloneContext::Create(vocbase),
      EMPTY,
      EMPTY,
      EMPTY,
      arangodb::transaction::Options()
    );
    CHECK((trx.begin().ok()));
    auto* l = dynamic_cast<arangodb::iresearch::IResearchLink*>(link.get());
    REQUIRE(l != nullptr);
    CHECK((l->insert(trx, arangodb::LocalDocumentId(1), doc0->slice(), arangodb::Index::OperationMode::normal).ok()));
    CHECK((trx.commit().ok()));
    CHECK((l->commit().ok()));
  }

  CHECK((1 == reader.reopen().live_docs_count()));

  {
    arangodb::transaction::Methods trx(
      arangodb::transaction::StandaloneContext::Create(vocbase),
      EMPTY,
      EMPTY,
      EMPTY,
      arangodb::transaction::Options()
    );
    CHECK((trx.begin().ok()));
    auto* l = dynamic_cast<arangodb::iresearch::IResearchLink*>(link.get());
    REQUIRE(l != nullptr);
    CHECK((l->insert(trx, arangodb::LocalDocumentId(2), doc1->slice(), arangodb::Index::OperationMode::normal).ok()));
    CHECK((trx.commit().ok()));
    CHECK((l->commit().ok()));
  }

  CHECK((2 == reader.reopen().live_docs_count()));

  {
    arangodb::transaction::Methods trx(
      arangodb::transaction::StandaloneContext::Create(vocbase),
      EMPTY,
      EMPTY,
      EMPTY,
      arangodb::transaction::Options()
    );
    CHECK((trx.begin().ok()));
    auto* l = dynamic_cast<arangodb::iresearch::IResearchLink*>(link.get());
    REQUIRE(l != nullptr);
    CHECK((l->remove(trx, arangodb::LocalDocumentId(2), doc1->slice(), arangodb::Index::OperationMode::normal).ok()));
    CHECK((trx.commit().ok()));
    CHECK((l->commit().ok()));
  }

  CHECK((1 == reader.reopen().live_docs_count()));
  logicalCollection->dropIndex(link->id());
  CHECK_THROWS((reader.reopen()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate tests
////////////////////////////////////////////////////////////////////////////////

}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------