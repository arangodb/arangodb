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
#include "StorageEngineMock.h"

#include "store/fs_directory.hpp"
#include "utils/log.hpp"
#include "utils/utf8_path.hpp"

#include "Basics/files.h"
#include "IResearch/IResearchLink.h"
#include "Logger/Logger.h"
#include "Logger/LogTopic.h"
#include "RestServer/ViewTypesFeature.h"
#include "RestServer/QueryRegistryFeature.h"
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
  std::string testFilesystemPath;

  IResearchLinkSetup(): server(nullptr, nullptr) {
    arangodb::EngineSelectorFeature::ENGINE = &engine;
    arangodb::application_features::ApplicationFeature* feature;

    // ViewTypesFeature
    arangodb::application_features::ApplicationServer::server->addFeature(
      feature = new arangodb::ViewTypesFeature(arangodb::application_features::ApplicationServer::server)
    );
    feature->start();
    feature->prepare();

    // QueryRegistryFeature
    arangodb::application_features::ApplicationServer::server->addFeature(
      feature = new arangodb::QueryRegistryFeature(&server)
    );
    feature->start();
    feature->prepare();

    arangodb::ViewTypesFeature::registerViewImplementation(
      arangodb::iresearch::IResearchView::type(),
      arangodb::iresearch::IResearchView::make
    );

    PhysicalViewMock::persistPropertiesResult = TRI_ERROR_NO_ERROR;
    TransactionStateMock::abortTransactionCount = 0;
    TransactionStateMock::beginTransactionCount = 0;
    TransactionStateMock::commitTransactionCount = 0;
    testFilesystemPath = (
      irs::utf8_path()/
      TRI_GetTempPath()/
      (std::string("arangodb_tests.") + std::to_string(TRI_microtime()))
      ).utf8();

    long systemError;
    std::string systemErrorStr;
    TRI_CreateDirectory(testFilesystemPath.c_str(), systemError, systemErrorStr);

    // suppress log messages since tests check error conditions
    arangodb::LogTopic::setLogLevel(arangodb::Logger::FIXME.name(), arangodb::LogLevel::FATAL);
    irs::logger::output_le(iresearch::logger::IRL_FATAL, stderr);
  }

  ~IResearchLinkSetup() {
    TRI_RemoveDirectory(testFilesystemPath.c_str());
    arangodb::LogTopic::setLogLevel(arangodb::Logger::FIXME.name(), arangodb::LogLevel::DEFAULT);
    arangodb::application_features::ApplicationServer::server = nullptr;
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
    auto json = arangodb::velocypack::Parser::fromJson("{}");
    auto link = arangodb::iresearch::IResearchLink::make(1, nullptr, json->slice());
    CHECK((true == !link));
  }

  // no view can be found
  {
    auto json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\" }");
    auto link = arangodb::iresearch::IResearchLink::make(1, nullptr, json->slice());
    CHECK((true == !link));
  }

  // view specified without registration
  {
    // 'skipViewRegistration' attribute as defined in IResearchLink.cpp SKIP_VIEW_REGISTRATION_FIELD
    auto json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"skipViewRegistration\": null }");
    auto link = arangodb::iresearch::IResearchLink::make(1, nullptr, json->slice());
    CHECK((false == !link));
    auto builder = link->toVelocyPack(false);
    auto slice = builder->slice();
    CHECK((
      slice.hasKey("name")
      && slice.get("name").isString()
      && std::string("testView") == slice.get("name").copyString()
    ));
  }

  // valid link creation
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto linkJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\" }");
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
    auto viewJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"iresearch\", \"properties\": { \"name\" : \"testView\" } }");
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
    auto builder = link->toVelocyPack(true);
    std::string error;

    CHECK((actualMeta.init(builder->slice(), error) && expectedMeta == actualMeta));
    auto slice = builder->slice();
    CHECK((
      slice.hasKey("name")
      && slice.get("name").isString()
      && std::string("testView") == slice.get("name").copyString()
      && slice.hasKey("figures")
      && slice.get("figures").isObject()
      && slice.get("figures").hasKey("memory")
      && slice.get("figures").get("memory").isNumber()
      && 0 < slice.get("figures").get("memory").getUInt()
    ));
    CHECK((logicalCollection->dropIndex(link->id()) && logicalCollection->getIndexes().empty()));
  }
}

SECTION("test_write") {
  static std::vector<std::string> const EMPTY;
  auto doc0 = arangodb::velocypack::Parser::fromJson("{ \"abc\": \"def\" }");
  auto doc1 = arangodb::velocypack::Parser::fromJson("{ \"ghi\": \"jkl\" }");
  std::string dataPath = (irs::utf8_path()/s.testFilesystemPath/std::string("test_write")).utf8();
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
  auto linkJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"includeAllFields\": true }");
  auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
  auto viewJson = arangodb::velocypack::Parser::fromJson("{ \
    \"name\": \"testView\", \
    \"type\": \"iresearch\", \
    \"properties\": { \"name\": \"testView\", \
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
  CHECK((false == !link && created));
  CHECK((0 == reader.reopen().live_docs_count()));
// FIXME TODO for use by iresearch::kludge, remove once checkpoint ids are implemented
#if 1
  CHECK((TRI_ERROR_BAD_PARAMETER == static_cast<arangodb::iresearch::IResearchLink*>(link.get())->insert(nullptr, 0, 1, doc0->slice()).errorNumber()));
#else
  CHECK((TRI_ERROR_BAD_PARAMETER == link->insert(nullptr, 1, doc0->slice(), false).errorNumber()));
#endif
  {
    arangodb::transaction::UserTransaction trx(arangodb::transaction::StandaloneContext::Create(&vocbase), EMPTY, EMPTY, EMPTY, arangodb::transaction::Options());
    CHECK((trx.begin().ok()));
// FIXME TODO for use by iresearch::kludge, remove once checkpoint ids are implemented
#if 1
    CHECK((static_cast<arangodb::iresearch::IResearchLink*>(link.get())->insert(&trx, 0, 1, doc0->slice()).ok()));
#else
    CHECK((link->insert(&trx, 1, doc0->slice(), false).ok()));
#endif
    CHECK((trx.commit().ok()));
  }

  CHECK((TRI_ERROR_NO_ERROR == view->finish(0))); // TODO FIXME this should be a proper FID once IResearchLink supports fid resolution
  CHECK((true == view->sync()));
  CHECK((1 == reader.reopen().live_docs_count()));

  {
    arangodb::transaction::UserTransaction trx(arangodb::transaction::StandaloneContext::Create(&vocbase), EMPTY, EMPTY, EMPTY, arangodb::transaction::Options());
    CHECK((trx.begin().ok()));
// FIXME TODO for use by iresearch::kludge, remove once checkpoint ids are implemented
#if 1
    CHECK((static_cast<arangodb::iresearch::IResearchLink*>(link.get())->insert(&trx, 0, 2, doc1->slice()).ok()));
#else
    CHECK((link->insert(&trx, 2, doc1->slice(), false).ok()));
#endif
    CHECK((trx.commit().ok()));
  }

  CHECK((TRI_ERROR_NO_ERROR == view->finish(0))); // TODO FIXME this should be a proper FID once IResearchLink supports fid resolution
  CHECK((true == view->sync()));
  CHECK((2 == reader.reopen().live_docs_count()));

  {
    arangodb::transaction::UserTransaction trx(arangodb::transaction::StandaloneContext::Create(&vocbase), EMPTY, EMPTY, EMPTY, arangodb::transaction::Options());
    CHECK((trx.begin().ok()));
// FIXME TODO for use by iresearch::kludge, remove once checkpoint ids are implemented
#if 1
    CHECK((static_cast<arangodb::iresearch::IResearchLink*>(link.get())->remove(&trx, 2).ok()));
#else
    CHECK((link->remove(&trx, 2, doc1->slice(), false).ok()));
#endif
    CHECK((trx.commit().ok()));
  }

  CHECK((TRI_ERROR_NO_ERROR == view->finish(0))); // TODO FIXME this should be a proper FID once IResearchLink supports fid resolution
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
