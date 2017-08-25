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

#include "ApplicationFeatures/JemallocFeature.h"
#include "Aql/AqlFunctionFeature.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "Basics/files.h"
#include "IResearch/IResearchAnalyzerFeature.h"
#include "IResearch/IResearchFeature.h"
#include "IResearch/IResearchMMFilesLink.h"
#include "IResearch/SystemDatabaseFeature.h"
#include "Logger/Logger.h"
#include "Logger/LogTopic.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/DatabasePathFeature.h"
#include "RestServer/FeatureCacheFeature.h"
#include "RestServer/FlushFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "RestServer/ViewTypesFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "Transaction/StandaloneContext.h"
#include "Transaction/UserTransaction.h"
#include "velocypack/Parser.h"
#include "VocBase/KeyGenerator.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/LogicalView.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

struct IResearchLinkSetup {
  StorageEngineMock engine;
  arangodb::application_features::ApplicationServer server;
  std::unique_ptr<TRI_vocbase_t> system;
  std::vector<std::pair<arangodb::application_features::ApplicationFeature*, bool>> features;
  std::string testFilesystemPath;

  IResearchLinkSetup(): server(nullptr, nullptr) {
    arangodb::EngineSelectorFeature::ENGINE = &engine;

    arangodb::tests::init();

    // setup required application features
    features.emplace_back(new arangodb::AuthenticationFeature(&server), true); // required for FeatureCacheFeature
    features.emplace_back(new arangodb::DatabaseFeature(&server), false); // required for FeatureCacheFeature
    features.emplace_back(new arangodb::FeatureCacheFeature(&server), true); // required for IResearchAnalyzerFeature
    features.emplace_back(new arangodb::ViewTypesFeature(&server), true);
    features.emplace_back(new arangodb::QueryRegistryFeature(&server), false);
    arangodb::application_features::ApplicationServer::server->addFeature(features.back().first);
    system = irs::memory::make_unique<TRI_vocbase_t>(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 0, TRI_VOC_SYSTEM_DATABASE);
    features.emplace_back(new arangodb::DatabasePathFeature(&server), false);
    features.emplace_back(new arangodb::JemallocFeature(&server), false); // required for DatabasePathFeature
    features.emplace_back(new arangodb::aql::AqlFunctionFeature(&server), true); // required for IResearchAnalyzerFeature
    features.emplace_back(new arangodb::iresearch::IResearchAnalyzerFeature(&server), true);
    features.emplace_back(new arangodb::iresearch::IResearchFeature(&server), true);
    features.emplace_back(new arangodb::iresearch::SystemDatabaseFeature(&server, system.get()), false); // required for IResearchAnalyzerFeature
    features.emplace_back(new arangodb::FlushFeature(&server), false); // do not start the thread

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

    PhysicalViewMock::persistPropertiesResult = TRI_ERROR_NO_ERROR;
    TransactionStateMock::abortTransactionCount = 0;
    TransactionStateMock::beginTransactionCount = 0;
    TransactionStateMock::commitTransactionCount = 0;
    testFilesystemPath = (
      (irs::utf8_path()/=
      TRI_GetTempPath())/=
      (std::string("arangodb_tests.") + std::to_string(TRI_microtime()))
      ).utf8();

    long systemError;
    std::string systemErrorStr;
    TRI_CreateDirectory(testFilesystemPath.c_str(), systemError, systemErrorStr);

    // suppress log messages since tests check error conditions
    arangodb::LogTopic::setLogLevel(arangodb::iresearch::IResearchFeature::IRESEARCH.name(), arangodb::LogLevel::FATAL);
    irs::logger::output_le(iresearch::logger::IRL_FATAL, stderr);
  }

  ~IResearchLinkSetup() {
    system.reset(); // destroy before reseting the 'ENGINE'
    TRI_RemoveDirectory(testFilesystemPath.c_str());
    arangodb::LogTopic::setLogLevel(arangodb::iresearch::IResearchFeature::IRESEARCH.name(), arangodb::LogLevel::DEFAULT);
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

    arangodb::FeatureCacheFeature::reset();
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
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
    auto* logicalCollection = vocbase.createCollection(collectionJson->slice());
    REQUIRE((nullptr != logicalCollection));
    auto json = arangodb::velocypack::Parser::fromJson("{}");
    auto link = arangodb::iresearch::IResearchMMFilesLink::make(1, logicalCollection, json->slice());
    CHECK((true == !link));
  }

  // no view can be found
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
    auto* logicalCollection = vocbase.createCollection(collectionJson->slice());
    REQUIRE((nullptr != logicalCollection));
    auto json = arangodb::velocypack::Parser::fromJson("{ \"view\": 42 }");
    auto link = arangodb::iresearch::IResearchMMFilesLink::make(1, logicalCollection, json->slice());
    CHECK((true == !link));
  }

  // valid link creation
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto linkJson = arangodb::velocypack::Parser::fromJson("{ \"view\": 42 }");
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
    auto viewJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"id\": 42, \"type\": \"iresearch\" }");
    auto* logicalCollection = vocbase.createCollection(collectionJson->slice());
    REQUIRE((nullptr != logicalCollection));
    auto logicalView = vocbase.createView(viewJson->slice(), 0);
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
    CHECK((std::string("iresearch") == link->typeName()));
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
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto linkJson = arangodb::velocypack::Parser::fromJson("{ \"view\": 42 }");
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
    auto viewJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"id\": 42, \"type\": \"iresearch\" }");
    auto* logicalCollection = vocbase.createCollection(collectionJson->slice());
    REQUIRE((nullptr != logicalCollection));
    auto logicalView = vocbase.createView(viewJson->slice(), 0);
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
    CHECK((std::string("iresearch") == link->typeName()));
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

SECTION("test_write") {
  static std::vector<std::string> const EMPTY;
  auto doc0 = arangodb::velocypack::Parser::fromJson("{ \"abc\": \"def\" }");
  auto doc1 = arangodb::velocypack::Parser::fromJson("{ \"ghi\": \"jkl\" }");
  std::string dataPath = ((irs::utf8_path()/=s.testFilesystemPath)/=std::string("test_write")).utf8();
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
  auto linkJson = arangodb::velocypack::Parser::fromJson("{ \"view\": 42, \"includeAllFields\": true }");
  auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
  auto viewJson = arangodb::velocypack::Parser::fromJson("{ \
    \"id\": 42, \
    \"name\": \"testView\", \
    \"type\": \"iresearch\", \
    \"properties\": { \
      \"dataPath\": \"" + arangodb::basics::StringUtils::replace(dataPath, "\\", "/") + "\" \
    } \
  }");
  auto* logicalCollection = vocbase.createCollection(collectionJson->slice());
  REQUIRE((nullptr != logicalCollection));
  auto logicalView = vocbase.createView(viewJson->slice(), 0);
  REQUIRE((false == !logicalView));
  auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView->getImplementation());
  REQUIRE((false == !view));
  view->open();

  irs::fs_directory directory(dataPath);
  auto reader = irs::directory_reader::open(directory);
  bool created;
  auto link = logicalCollection->createIndex(nullptr, linkJson->slice(), created);
  REQUIRE((false == !link && created));
  CHECK((0 == reader.reopen().live_docs_count()));
  CHECK((TRI_ERROR_BAD_PARAMETER == link->insert(nullptr, 1, doc0->slice(), false).errorNumber()));
  {
    arangodb::transaction::UserTransaction trx(arangodb::transaction::StandaloneContext::Create(&vocbase), EMPTY, EMPTY, EMPTY, arangodb::transaction::Options());
    CHECK((trx.begin().ok()));
    CHECK((link->insert(&trx, 1, doc0->slice(), false).ok()));
    CHECK((trx.commit().ok()));
  }

  CHECK((TRI_ERROR_NO_ERROR == view->finish()));
  CHECK((true == view->sync()));
  CHECK((1 == reader.reopen().live_docs_count()));

  {
    arangodb::transaction::UserTransaction trx(arangodb::transaction::StandaloneContext::Create(&vocbase), EMPTY, EMPTY, EMPTY, arangodb::transaction::Options());
    CHECK((trx.begin().ok()));
    CHECK((link->insert(&trx, 2, doc1->slice(), false).ok()));
    CHECK((trx.commit().ok()));
  }

  CHECK((TRI_ERROR_NO_ERROR == view->finish()));
  CHECK((true == view->sync()));
  CHECK((2 == reader.reopen().live_docs_count()));

  {
    arangodb::transaction::UserTransaction trx(arangodb::transaction::StandaloneContext::Create(&vocbase), EMPTY, EMPTY, EMPTY, arangodb::transaction::Options());
    CHECK((trx.begin().ok()));
    CHECK((link->remove(&trx, 2, doc1->slice(), false).ok()));
    CHECK((trx.commit().ok()));
  }

  CHECK((TRI_ERROR_NO_ERROR == view->finish()));
  CHECK((true == view->sync()));
  CHECK((1 == reader.reopen().live_docs_count()));
  logicalCollection->dropIndex(link->id());
  CHECK((true == view->sync()));
  CHECK((0 == reader.reopen().live_docs_count()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate tests
////////////////////////////////////////////////////////////////////////////////

}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------