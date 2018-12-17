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

#include "store/fs_directory.hpp"
#include "utils/log.hpp"
#include "utils/utf8_path.hpp"

#include "Aql/AqlFunctionFeature.h"

#if USE_ENTERPRISE
  #include "Enterprise/Ldap/LdapFeature.h"
#endif

#include "Basics/files.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "IResearch/IResearchAnalyzerFeature.h"
#include "IResearch/IResearchCommon.h"
#include "IResearch/IResearchFeature.h"
#include "IResearch/IResearchMMFilesLink.h"
#include "IResearch/IResearchView.h"
#include "Logger/Logger.h"
#include "Logger/LogTopic.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/DatabasePathFeature.h"
#include "RestServer/FlushFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "RestServer/ServerIdFeature.h"
#include "RestServer/SystemDatabaseFeature.h"
#include "RestServer/ViewTypesFeature.h"
#include "Sharding/ShardingFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "Transaction/Methods.h"
#include "Transaction/StandaloneContext.h"
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
  std::unique_ptr<TRI_vocbase_t> system;
  std::vector<std::pair<arangodb::application_features::ApplicationFeature*, bool>> features;
  std::string testFilesystemPath;

  IResearchLinkSetup(): engine(server), server(nullptr, nullptr) {
    arangodb::EngineSelectorFeature::ENGINE = &engine;

    arangodb::tests::init();

    // suppress INFO {authentication} Authentication is turned on (system only), authentication for unix sockets is turned on
    arangodb::LogTopic::setLogLevel(arangodb::Logger::AUTHENTICATION.name(), arangodb::LogLevel::WARN);

    // suppress log messages since tests check error conditions
    arangodb::LogTopic::setLogLevel(arangodb::iresearch::TOPIC.name(), arangodb::LogLevel::FATAL);
    irs::logger::output_le(iresearch::logger::IRL_FATAL, stderr);

    // setup required application features
    features.emplace_back(new arangodb::AuthenticationFeature(server), true);
    features.emplace_back(new arangodb::DatabaseFeature(server), false);
    features.emplace_back(new arangodb::ShardingFeature(server), false);
    features.emplace_back(new arangodb::ViewTypesFeature(server), true);
    features.emplace_back(new arangodb::QueryRegistryFeature(server), false);
    arangodb::application_features::ApplicationServer::server->addFeature(features.back().first); // need QueryRegistryFeature feature to be added now in order to create the system database
    system = irs::memory::make_unique<TRI_vocbase_t>(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 0, TRI_VOC_SYSTEM_DATABASE);
    features.emplace_back(new arangodb::SystemDatabaseFeature(server, system.get()), false); // required for IResearchAnalyzerFeature
    features.emplace_back(new arangodb::DatabasePathFeature(server), false);
    features.emplace_back(new arangodb::aql::AqlFunctionFeature(server), true); // required for IResearchAnalyzerFeature
    features.emplace_back(new arangodb::iresearch::IResearchAnalyzerFeature(server), true);
    features.emplace_back(new arangodb::iresearch::IResearchFeature(server), true);
    features.emplace_back(new arangodb::FlushFeature(server), false); // do not start the thread

    #if USE_ENTERPRISE
      features.emplace_back(new arangodb::LdapFeature(server), false); // required for AuthenticationFeature with USE_ENTERPRISE
    #endif

    for (auto& f : features) {
      arangodb::application_features::ApplicationServer::server->addFeature(f.first);
    }

    for (auto& f : features) {
      f.first->prepare();
    }

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
    system.reset(); // destroy before reseting the 'ENGINE'
    TRI_RemoveDirectory(testFilesystemPath.c_str());
    arangodb::LogTopic::setLogLevel(arangodb::iresearch::TOPIC.name(), arangodb::LogLevel::DEFAULT);
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

    arangodb::LogTopic::setLogLevel(arangodb::Logger::AUTHENTICATION.name(), arangodb::LogLevel::DEFAULT);
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

  // no view can be found
  {
    s.engine.views.clear();
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    REQUIRE((nullptr != logicalCollection));
    auto json = arangodb::velocypack::Parser::fromJson("{ \"view\": \"42\" }");
    std::shared_ptr<arangodb::Index> link;
    CHECK((TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND == arangodb::iresearch::IResearchMMFilesLink::factory().instantiate(link, *logicalCollection, json->slice(), 1, false).errorNumber()));
    CHECK((true == !link));
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
    CHECK((true == link->isPersistent()));
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
    CHECK((link->insert(trx, arangodb::LocalDocumentId(1), doc0->slice(), arangodb::Index::OperationMode::normal).ok()));
    CHECK((trx.commit().ok()));
  }

  flush->executeCallbacks(); // prepare memory store to be flushed to persisted storage
  CHECK((view->commit().ok()));
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
    CHECK((link->insert(trx, arangodb::LocalDocumentId(2), doc1->slice(), arangodb::Index::OperationMode::normal).ok()));
    CHECK((trx.commit().ok()));
  }

  flush->executeCallbacks(); // prepare memory store to be flushed to persisted storage
  CHECK((view->commit().ok()));
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
    CHECK((link->remove(trx, arangodb::LocalDocumentId(2), doc1->slice(), arangodb::Index::OperationMode::normal).ok()));
    CHECK((trx.commit().ok()));
  }

  flush->executeCallbacks(); // prepare memory store to be flushed to persisted storage
  CHECK((view->commit().ok()));
  CHECK((1 == reader.reopen().live_docs_count()));
  logicalCollection->dropIndex(link->id());
  CHECK((view->commit().ok()));
  CHECK_THROWS((reader.reopen()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate tests
////////////////////////////////////////////////////////////////////////////////

}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------