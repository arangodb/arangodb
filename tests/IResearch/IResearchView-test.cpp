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

#include "utils/locale_utils.hpp"
#include "utils/log.hpp"
#include "utils/utf8_path.hpp"

#include "Aql/AstNode.h"
#include "Aql/SortCondition.h"
#include "Basics/files.h"
#include "IResearch/IResearchLinkMeta.h"
#include "IResearch/IResearchView.h"
#include "Logger/Logger.h"
#include "Logger/LogTopic.h"
#include "RestServer/ViewTypesFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "Transaction/StandaloneContext.h"
#include "Transaction/UserTransaction.h"
#include "velocypack/Iterator.h"
#include "velocypack/Parser.h"
#include "Views/ViewIterator.h"
#include "VocBase/KeyGenerator.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/LogicalView.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

struct IResearchViewSetup {
  StorageEngineMock engine;
  arangodb::application_features::ApplicationServer server;
  std::string testFilesystemPath;

  IResearchViewSetup(): server(nullptr, nullptr) {
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

  ~IResearchViewSetup() {
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

TEST_CASE("IResearchViewTest", "[iresearch][iresearch-view]") {
  IResearchViewSetup s;
  UNUSED(s);

SECTION("test_defaults") {
  auto namedJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\" }");
  auto json = arangodb::velocypack::Parser::fromJson("{ \
    \"name\": \"testView\" \
  }");
  arangodb::iresearch::IResearchViewMeta expectedMeta;

  expectedMeta._name = "testView";

  // existing view definition
  {
    auto view = arangodb::iresearch::IResearchView::make(nullptr, json->slice(), false);
    CHECK(false == (!view));

    arangodb::velocypack::Builder builder;

    builder.openObject();
    view->getPropertiesVPack(builder);
    builder.close();

    auto slice = builder.slice();
    arangodb::iresearch::IResearchViewMeta meta;
    std::string error;

    CHECK((9U == slice.length()));
    CHECK((meta.init(slice, error) && expectedMeta == meta));
  }

  // existing view definition with LogicalView
  {
    arangodb::LogicalView logicalView(nullptr, namedJson->slice());
    auto view = arangodb::iresearch::IResearchView::make(&logicalView, json->slice(), false);
    CHECK(false == (!view));

    arangodb::velocypack::Builder builder;

    builder.openObject();
    view->getPropertiesVPack(builder);
    builder.close();

    auto slice = builder.slice();
    arangodb::iresearch::IResearchViewMeta meta;
    std::string error;

    CHECK((10U == slice.length()));
    CHECK((meta.init(slice, error) && expectedMeta == meta));
  }

  // new view definition
  {
    auto view = arangodb::iresearch::IResearchView::make(nullptr, json->slice(), true);
    CHECK(false == (!view));

    arangodb::velocypack::Builder builder;

    builder.openObject();
    view->getPropertiesVPack(builder);
    builder.close();

    auto slice = builder.slice();
    arangodb::iresearch::IResearchViewMeta meta;
    std::string error;

    CHECK((9U == slice.length()));
    CHECK((meta.init(slice, error) && expectedMeta == meta));
  }

  // new view definition with LogicalView
  {
    arangodb::LogicalView logicalView(nullptr, namedJson->slice());
    auto view = arangodb::iresearch::IResearchView::make(&logicalView, json->slice(), true);
    CHECK(false == (!view));

    arangodb::velocypack::Builder builder;

    builder.openObject();
    view->getPropertiesVPack(builder);
    builder.close();

    auto slice = builder.slice();
    arangodb::iresearch::IResearchViewMeta meta;
    std::string error;

    CHECK((10U == slice.length()));
    CHECK((meta.init(slice, error) && expectedMeta == meta));

    auto tmpSlice = slice.get("links");
    CHECK((true == tmpSlice.isObject() && 0 == tmpSlice.length()));
  }
}

SECTION("test_drop") {
  std::string dataPath = (irs::utf8_path()/s.testFilesystemPath/std::string("deleteme")).utf8();
  auto json = arangodb::velocypack::Parser::fromJson("{ \
    \"name\": \"testView\", \
    \"type\": \"iresearch\", \
    \"links\": { \"testCollection\": {} }, \
    \"dataPath\": \"" + arangodb::basics::StringUtils::replace(dataPath, "\\", "/") + "\" \
  }");

  CHECK((false == TRI_IsDirectory(dataPath.c_str())));

  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
  auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
  auto* logicalCollection = vocbase.createCollection(collectionJson->slice());
  CHECK((nullptr != logicalCollection));
  CHECK((true == !vocbase.lookupView("testView")));
  CHECK((true == logicalCollection->getIndexes().empty()));
  auto logicalView = vocbase.createView(json->slice(), 0);
  CHECK((false == !logicalView));
  auto view = logicalView->getImplementation();
  CHECK((false == !view));

  CHECK((false == logicalCollection->getIndexes().empty()));
  CHECK((false == !vocbase.lookupView("testView")));
  CHECK((false == TRI_IsDirectory(dataPath.c_str())));
  view->open();
  CHECK((true == TRI_IsDirectory(dataPath.c_str())));
  CHECK((TRI_ERROR_NO_ERROR == vocbase.dropView("testView")));
  CHECK((true == logicalCollection->getIndexes().empty()));
  CHECK((true == !vocbase.lookupView("testView")));
  CHECK((false == TRI_IsDirectory(dataPath.c_str())));
}

SECTION("test_move_datapath") {
  std::string createDataPath = (irs::utf8_path()/s.testFilesystemPath/std::string("deleteme0")).utf8();
  std::string updateDataPath = (irs::utf8_path()/s.testFilesystemPath/std::string("deleteme1")).utf8();
  auto namedJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\" }");
  auto createJson = arangodb::velocypack::Parser::fromJson("{ \
    \"name\": \"testView\", \
    \"type\": \"iresearch\", \
    \"dataPath\": \"" + arangodb::basics::StringUtils::replace(createDataPath, "\\", "/") + "\" \
  }");
  auto updateJson = arangodb::velocypack::Parser::fromJson("{ \
    \"dataPath\": \"" + arangodb::basics::StringUtils::replace(updateDataPath, "\\", "/") + "\" \
  }");

  CHECK((false == TRI_IsDirectory(createDataPath.c_str())));
  CHECK((false == TRI_IsDirectory(updateDataPath.c_str())));

  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
  auto logicalView = vocbase.createView(createJson->slice(), 0);
  CHECK((false == !logicalView));
  auto view = logicalView->getImplementation();
  CHECK((false == !view));

  CHECK((false == TRI_IsDirectory(createDataPath.c_str())));
  view->open();
  CHECK((true == TRI_IsDirectory(createDataPath.c_str())));
  CHECK((view->updateProperties(updateJson->slice(), true, false).ok()));
  CHECK((false == TRI_IsDirectory(createDataPath.c_str())));
  CHECK((true == TRI_IsDirectory(updateDataPath.c_str())));
}

SECTION("test_open") {
  std::string dataPath = (irs::utf8_path()/s.testFilesystemPath/std::string("deleteme")).utf8();
  auto namedJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\" }");
  auto json = arangodb::velocypack::Parser::fromJson("{ \
    \"name\": \"testView\", \
    \"dataPath\": \"" + arangodb::basics::StringUtils::replace(dataPath, "\\", "/") + "\" \
  }");

  CHECK((false == TRI_IsDirectory(dataPath.c_str())));
  auto view = arangodb::iresearch::IResearchView::make(nullptr, json->slice(), false);
  CHECK(false == (!view));
  CHECK((false == TRI_IsDirectory(dataPath.c_str())));
  view->open();
  CHECK((true == TRI_IsDirectory(dataPath.c_str())));
}

SECTION("test_query") {
  auto createJson = arangodb::velocypack::Parser::fromJson("{ \
    \"name\": \"testView\", \
    \"type\": \"iresearch\" \
  }");
  static std::vector<std::string> const EMPTY;
  arangodb::aql::AstNode noop(arangodb::aql::AstNodeType::NODE_TYPE_FILTER);
  arangodb::aql::AstNode noopChild(arangodb::aql::AstNodeType::NODE_TYPE_OPERATOR_BINARY_OR);

  noop.addMember(&noopChild);

  // no transaction provided
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalView = vocbase.createView(createJson->slice(), 0);
    CHECK((false == !logicalView));
    auto view = logicalView->getImplementation();
    CHECK((false == !view));

    CHECK((nullptr == dynamic_cast<arangodb::iresearch::IResearchView*>(view)->iteratorForCondition(nullptr, &noop, nullptr, nullptr)));
  }

  // no filter provided
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalView = vocbase.createView(createJson->slice(), 0);
    CHECK((false == !logicalView));
    auto view = logicalView->getImplementation();
    CHECK((false == !view));

    arangodb::transaction::Options options;
    options.waitForSync = false;
    options.allowImplicitCollections = false;

    arangodb::transaction::UserTransaction trx(arangodb::transaction::StandaloneContext::Create(&vocbase), EMPTY, EMPTY, EMPTY, options);
    CHECK((nullptr == dynamic_cast<arangodb::iresearch::IResearchView*>(view)->iteratorForCondition(&trx, nullptr, nullptr, nullptr)));
  }

  // empty ordered iterator
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalView = vocbase.createView(createJson->slice(), 0);
    CHECK((false == !logicalView));
    auto* view = logicalView->getImplementation();
    CHECK((false == !view));

    std::vector<std::pair<arangodb::aql::Variable const*, bool>> sorts;
    std::vector<std::vector<arangodb::basics::AttributeName>> constAttributes;
    std::unordered_map<arangodb::aql::VariableId, arangodb::aql::AstNode const*> variableDefinitions;

    sorts.emplace_back(std::make_pair(nullptr, true)); // add one condition

    arangodb::aql::SortCondition order(nullptr, sorts, constAttributes, variableDefinitions);

    arangodb::transaction::Options options;
    options.waitForSync = false;
    options.allowImplicitCollections = false;

    arangodb::transaction::UserTransaction trx(arangodb::transaction::StandaloneContext::Create(&vocbase), EMPTY, EMPTY, EMPTY, options);
    CHECK((trx.begin().ok()));
    std::unique_ptr<arangodb::ViewIterator> itr(dynamic_cast<arangodb::iresearch::IResearchView*>(view)->iteratorForCondition(&trx, &noop, nullptr, &order));

    CHECK((false == !itr));
    CHECK((std::string("iresearch-ordered-iterator") == itr->typeName()));
    CHECK((&trx == itr->transaction()));
    CHECK((view == itr->view()));
    CHECK((false == itr->hasExtra()));
    CHECK_THROWS(itr->nextExtra([](arangodb::DocumentIdentifierToken const&, arangodb::velocypack::Slice)->void{}, 42));
    size_t count = 0;
    CHECK((false == itr->next([&count](arangodb::DocumentIdentifierToken const&)->void{ ++count; }, 42)));
    CHECK((0 == count));
    uint64_t skipped = 0;
    CHECK_NOTHROW((itr->skip(5, skipped)));
    CHECK((0 == skipped));
    CHECK_NOTHROW((itr->reset()));
  }

  // empty unordered iterator (nullptr sort condition)
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalView = vocbase.createView(createJson->slice(), 0);
    CHECK((false == !logicalView));
    auto* view = logicalView->getImplementation();
    CHECK((false == !view));

    arangodb::transaction::Options options;
    options.waitForSync = false;
    options.allowImplicitCollections = false;

    arangodb::transaction::UserTransaction trx(arangodb::transaction::StandaloneContext::Create(&vocbase), EMPTY, EMPTY, EMPTY, options);

    CHECK((trx.begin().ok()));
    std::unique_ptr<arangodb::ViewIterator> itr(dynamic_cast<arangodb::iresearch::IResearchView*>(view)->iteratorForCondition(&trx, &noop, nullptr, nullptr));

    CHECK((false == !itr));
    CHECK((std::string("iresearch-unordered-iterator") == itr->typeName()));
    CHECK((&trx == itr->transaction()));
    CHECK((view == itr->view()));
    CHECK((false == itr->hasExtra()));
    CHECK_THROWS(itr->nextExtra([](arangodb::DocumentIdentifierToken const&, arangodb::velocypack::Slice)->void{}, 42));
    size_t count = 0;
    CHECK((false == itr->next([&count](arangodb::DocumentIdentifierToken const&)->void{ ++count; }, 42)));
    CHECK((0 == count));
    uint64_t skipped = 0;
    CHECK_NOTHROW((itr->skip(5, skipped)));
    CHECK((0 == skipped));
    CHECK_NOTHROW((itr->reset()));
  }

  // empty unordered iterator (empty sort condition)
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalView = vocbase.createView(createJson->slice(), 0);
    CHECK((false == !logicalView));
    auto* view = logicalView->getImplementation();
    CHECK((false == !view));

    arangodb::transaction::Options options;
    options.waitForSync = false;
    options.allowImplicitCollections = false;

    arangodb::aql::SortCondition order;
    arangodb::transaction::UserTransaction trx(arangodb::transaction::StandaloneContext::Create(&vocbase), EMPTY, EMPTY, EMPTY, options);
    CHECK((trx.begin().ok()));
    std::unique_ptr<arangodb::ViewIterator> itr(dynamic_cast<arangodb::iresearch::IResearchView*>(view)->iteratorForCondition(&trx, &noop, nullptr, &order));

    CHECK((false == !itr));
    CHECK((std::string("iresearch-unordered-iterator") == itr->typeName()));
    CHECK((&trx == itr->transaction()));
    CHECK((view == itr->view()));
    CHECK((false == itr->hasExtra()));
    CHECK_THROWS(itr->nextExtra([](arangodb::DocumentIdentifierToken const&, arangodb::velocypack::Slice)->void{}, 42));
    size_t count = 0;
    CHECK((false == itr->next([&count](arangodb::DocumentIdentifierToken const&)->void{ ++count; }, 42)));
    CHECK((0 == count));
    uint64_t skipped = 0;
    CHECK_NOTHROW((itr->skip(5, skipped)));
    CHECK((0 == skipped));
    CHECK_NOTHROW((itr->reset()));
  }

  // ordered iterator
  {
    // FIXME TODO implement
  }

  // unordered iterator (nullptr sort condition)
  {
    // FIXME TODO implement
  }

  // unordered iterator (empty sort condition)
  {
    // FIXME TODO implement
  }

  // FIXME TODO implement
}

SECTION("test_update_overwrite") {
  auto createJson = arangodb::velocypack::Parser::fromJson("{ \
    \"name\": \"testView\", \
    \"type\": \"iresearch\" \
  }");

  // modify meta params
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalView = vocbase.createView(createJson->slice(), 0);
    CHECK((false == !logicalView));
    auto view = logicalView->getImplementation();
    CHECK((false == !view));

    // initial update (overwrite)
    {
      arangodb::iresearch::IResearchViewMeta expectedMeta;
      auto updateJson = arangodb::velocypack::Parser::fromJson("{ \
        \"locale\": \"en\", \
        \"name\": \"<invalid and ignored>\", \
        \"threadsMaxIdle\": 10, \
        \"threadsMaxTotal\": 20 \
      }");

      expectedMeta._name = "testView";
      expectedMeta._locale = irs::locale_utils::locale("en", true);
      expectedMeta._threadsMaxIdle = 10;
      expectedMeta._threadsMaxTotal = 20;
      CHECK((view->updateProperties(updateJson->slice(), false, false).ok()));

      arangodb::velocypack::Builder builder;

      builder.openObject();
      view->getPropertiesVPack(builder);
      builder.close();

      auto slice = builder.slice();
      arangodb::iresearch::IResearchViewMeta meta;
      std::string error;

      CHECK((10U == slice.length()));
      CHECK((meta.init(slice, error) && expectedMeta == meta));

      auto tmpSlice = slice.get("links");
      CHECK((true == tmpSlice.isObject() && 0 == tmpSlice.length()));
    }

    // subsequent update (overwrite)
    {
      arangodb::iresearch::IResearchViewMeta expectedMeta;
      auto updateJson = arangodb::velocypack::Parser::fromJson("{ \
        \"locale\": \"ru\", \
        \"name\": \"<invalid and ignored>\" \
      }");

      expectedMeta._name = "testView";
      expectedMeta._locale = irs::locale_utils::locale("ru", true);
      CHECK((view->updateProperties(updateJson->slice(), false, false).ok()));

      arangodb::velocypack::Builder builder;

      builder.openObject();
      view->getPropertiesVPack(builder);
      builder.close();

      auto slice = builder.slice();
      arangodb::iresearch::IResearchViewMeta meta;
      std::string error;

      CHECK((10U == slice.length()));
      CHECK((meta.init(slice, error) && expectedMeta == meta));

      auto tmpSlice = slice.get("links");
      CHECK((true == tmpSlice.isObject() && 0 == tmpSlice.length()));
    }
  }
}

SECTION("test_update_partial") {
  auto createJson = arangodb::velocypack::Parser::fromJson("{ \
    \"name\": \"testView\", \
    \"type\": \"iresearch\" \
  }");

  // modify meta params
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalView = vocbase.createView(createJson->slice(), 0);
    CHECK((false == !logicalView));
    auto view = logicalView->getImplementation();
    CHECK((false == !view));

    arangodb::iresearch::IResearchViewMeta expectedMeta;
    auto updateJson = arangodb::velocypack::Parser::fromJson("{ \
      \"locale\": \"en\", \
      \"name\": \"<invalid and ignored>\", \
      \"threadsMaxIdle\": 10, \
      \"threadsMaxTotal\": 20 \
    }");

    expectedMeta._name = "testView";
    expectedMeta._locale = irs::locale_utils::locale("en", true);
    expectedMeta._threadsMaxIdle = 10;
    expectedMeta._threadsMaxTotal = 20;
    CHECK((view->updateProperties(updateJson->slice(), true, false).ok()));

    arangodb::velocypack::Builder builder;

    builder.openObject();
    view->getPropertiesVPack(builder);
    builder.close();

    auto slice = builder.slice();
    arangodb::iresearch::IResearchViewMeta meta;
    std::string error;

    CHECK((10U == slice.length()));
    CHECK((meta.init(slice, error) && expectedMeta == meta));

    auto tmpSlice = slice.get("links");
    CHECK((true == tmpSlice.isObject() && 0 == tmpSlice.length()));
  }

  // test rollback on meta modification failure
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalView = vocbase.createView(createJson->slice(), 0);
    CHECK((false == !logicalView));
    auto view = logicalView->getImplementation();
    CHECK((false == !view));

    std::string dataPath = (irs::utf8_path()/s.testFilesystemPath/std::string("deleteme")).utf8();
    auto res = TRI_CreateDatafile(dataPath, 1); // create a file where the data path directory should be
    arangodb::iresearch::IResearchViewMeta expectedMeta;
    auto updateJson = arangodb::velocypack::Parser::fromJson(std::string() + "{ \
      \"dataPath\": \"" + arangodb::basics::StringUtils::replace(dataPath, "\\", "/") + "\", \
      \"locale\": \"en\", \
      \"threadsMaxIdle\": 10, \
      \"threadsMaxTotal\": 20 \
    }");

    expectedMeta._name = "testView";
    CHECK((TRI_ERROR_BAD_PARAMETER == view->updateProperties(updateJson->slice(), true, false).errorNumber()));

    arangodb::velocypack::Builder builder;

    builder.openObject();
    view->getPropertiesVPack(builder);
    builder.close();

    auto slice = builder.slice();
    arangodb::iresearch::IResearchViewMeta meta;
    std::string error;

    CHECK((10U == slice.length()));
    CHECK((meta.init(slice, error) && expectedMeta == meta));

    auto tmpSlice = slice.get("links");
    CHECK((true == tmpSlice.isObject() && 0 == tmpSlice.length()));
  }

  // test rollback on persist failure
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalView = vocbase.createView(createJson->slice(), 0);
    CHECK((false == !logicalView));
    auto view = logicalView->getImplementation();
    CHECK((false == !view));

    arangodb::iresearch::IResearchViewMeta expectedMeta;
    auto updateJson = arangodb::velocypack::Parser::fromJson("{ \
      \"locale\": \"en\", \
      \"threadsMaxIdle\": 10, \
      \"threadsMaxTotal\": 20 \
    }");

    expectedMeta._name = "testView";
    PhysicalViewMock::persistPropertiesResult = TRI_ERROR_INTERNAL; // test fail
    CHECK((TRI_ERROR_INTERNAL == view->updateProperties(updateJson->slice(), true, false).errorNumber()));
    PhysicalViewMock::persistPropertiesResult = TRI_ERROR_NO_ERROR; // revert to valid

    arangodb::velocypack::Builder builder;

    builder.openObject();
    view->getPropertiesVPack(builder);
    builder.close();

    auto slice = builder.slice();
    arangodb::iresearch::IResearchViewMeta meta;
    std::string error;

    CHECK((10U == slice.length()));
    CHECK((meta.init(slice, error) && expectedMeta == meta));

    auto tmpSlice = slice.get("links");
    CHECK((true == tmpSlice.isObject() && 0 == tmpSlice.length()));
  }

  // add a new link
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
    auto* logicalCollection = vocbase.createCollection(collectionJson->slice());
    CHECK((nullptr != logicalCollection));
    auto logicalView = vocbase.createView(createJson->slice(), 0);
    CHECK((false == !logicalView));
    auto view = logicalView->getImplementation();
    CHECK((false == !view));

    arangodb::iresearch::IResearchViewMeta expectedMeta;
    std::unordered_map<std::string, arangodb::iresearch::IResearchLinkMeta> expectedLinkMeta;
    auto updateJson = arangodb::velocypack::Parser::fromJson("{ \
      \"links\": { \
        \"testCollection\": {} \
      }}");

    expectedMeta._collections.insert(logicalCollection->cid());
    expectedMeta._name = "testView";
    expectedLinkMeta["testCollection"]; // use defaults
    CHECK((view->updateProperties(updateJson->slice(), true, false).ok()));

    arangodb::velocypack::Builder builder;

    builder.openObject();
    view->getPropertiesVPack(builder);
    builder.close();

    auto slice = builder.slice();
    arangodb::iresearch::IResearchViewMeta meta;
    std::string error;

    CHECK((10U == slice.length()));
    CHECK((meta.init(slice, error) && expectedMeta == meta));

    auto tmpSlice = slice.get("links");
    CHECK((true == tmpSlice.isObject() && 1 == tmpSlice.length()));

    for (arangodb::velocypack::ObjectIterator itr(tmpSlice); itr.valid(); ++itr) {
      arangodb::iresearch::IResearchLinkMeta linkMeta;
      auto key = itr.key();
      auto value = itr.value();
      CHECK((true == key.isString()));

      auto expectedItr = expectedLinkMeta.find(key.copyString());
      CHECK((
        true == value.isObject()
        && expectedItr != expectedLinkMeta.end()
        && linkMeta.init(value, error)
        && expectedItr->second == linkMeta
      ));
      expectedLinkMeta.erase(expectedItr);
    }

    CHECK((true == expectedLinkMeta.empty()));
  }

  // add new link to non-existant collection
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalView = vocbase.createView(createJson->slice(), 0);
    CHECK((false == !logicalView));
    auto view = logicalView->getImplementation();
    CHECK((false == !view));

    arangodb::iresearch::IResearchViewMeta expectedMeta;
    auto updateJson = arangodb::velocypack::Parser::fromJson("{ \
      \"links\": { \
        \"testCollection\": {} \
      }}");

    expectedMeta._name = "testView";
    CHECK((TRI_ERROR_BAD_PARAMETER == view->updateProperties(updateJson->slice(), true, false).errorNumber()));

    arangodb::velocypack::Builder builder;

    builder.openObject();
    view->getPropertiesVPack(builder);
    builder.close();

    auto slice = builder.slice();
    arangodb::iresearch::IResearchViewMeta meta;
    std::string error;

    CHECK((10U == slice.length()));
    CHECK((meta.init(slice, error) && expectedMeta == meta));

    auto tmpSlice = slice.get("links");
    CHECK((true == tmpSlice.isObject() && 0 == tmpSlice.length()));
  }

  // remove link
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
    auto* logicalCollection = vocbase.createCollection(collectionJson->slice());
    CHECK((nullptr != logicalCollection));
    auto logicalView = vocbase.createView(createJson->slice(), 0);
    CHECK((false == !logicalView));
    auto view = logicalView->getImplementation();
    CHECK((false == !view));

    arangodb::iresearch::IResearchViewMeta expectedMeta;

    expectedMeta._collections.insert(logicalCollection->cid());
    expectedMeta._name = "testView";

    {
      auto updateJson = arangodb::velocypack::Parser::fromJson("{ \
        \"links\": { \
          \"testCollection\": {} \
      }}");

      CHECK((view->updateProperties(updateJson->slice(), true, false).ok()));

      arangodb::velocypack::Builder builder;

      builder.openObject();
      view->getPropertiesVPack(builder);
      builder.close();

      auto slice = builder.slice();
      arangodb::iresearch::IResearchViewMeta meta;
      std::string error;

      CHECK((10U == slice.length()));
      CHECK((meta.init(slice, error) && expectedMeta == meta));

      auto tmpSlice = slice.get("links");
      CHECK((true == tmpSlice.isObject() && 1 == tmpSlice.length()));
    }

    {
      auto updateJson = arangodb::velocypack::Parser::fromJson("{ \
        \"links\": { \
          \"testCollection\": null \
      }}");

      expectedMeta._collections.clear();
      CHECK((view->updateProperties(updateJson->slice(), true, false).ok()));

      arangodb::velocypack::Builder builder;

      builder.openObject();
      view->getPropertiesVPack(builder);
      builder.close();

      auto slice = builder.slice();
      arangodb::iresearch::IResearchViewMeta meta;
      std::string error;

      CHECK((10U == slice.length()));
      CHECK((meta.init(slice, error) && expectedMeta == meta));

      auto tmpSlice = slice.get("links");
      CHECK((true == tmpSlice.isObject() && 0 == tmpSlice.length()));
    }
  }

  // remove link from non-existant collection
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalView = vocbase.createView(createJson->slice(), 0);
    CHECK((false == !logicalView));
    auto view = logicalView->getImplementation();
    CHECK((false == !view));

    arangodb::iresearch::IResearchViewMeta expectedMeta;
    auto updateJson = arangodb::velocypack::Parser::fromJson("{ \
      \"links\": { \
        \"testCollection\": null \
      }}");

    expectedMeta._name = "testView";
    CHECK((TRI_ERROR_BAD_PARAMETER == view->updateProperties(updateJson->slice(), true, false).errorNumber()));

    arangodb::velocypack::Builder builder;

    builder.openObject();
    view->getPropertiesVPack(builder);
    builder.close();

    auto slice = builder.slice();
    arangodb::iresearch::IResearchViewMeta meta;
    std::string error;

    CHECK((10U == slice.length()));
    CHECK((meta.init(slice, error) && expectedMeta == meta));

    auto tmpSlice = slice.get("links");
    CHECK((true == tmpSlice.isObject() && 0 == tmpSlice.length()));
  }

  // remove non-existant link
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
    auto* logicalCollection = vocbase.createCollection(collectionJson->slice());
    CHECK((nullptr != logicalCollection));
    auto logicalView = vocbase.createView(createJson->slice(), 0);
    CHECK((false == !logicalView));
    auto view = logicalView->getImplementation();
    CHECK((false == !view));

    auto updateJson = arangodb::velocypack::Parser::fromJson("{ \
      \"links\": { \
        \"testCollection\": null \
    }}");
    arangodb::iresearch::IResearchViewMeta expectedMeta;

    expectedMeta._name = "testView";

    CHECK((view->updateProperties(updateJson->slice(), true, false).ok()));

    arangodb::velocypack::Builder builder;

    builder.openObject();
    view->getPropertiesVPack(builder);
    builder.close();

    auto slice = builder.slice();
    arangodb::iresearch::IResearchViewMeta meta;
    std::string error;

    CHECK((10U == slice.length()));
    CHECK((meta.init(slice, error) && expectedMeta == meta));

    auto tmpSlice = slice.get("links");
    CHECK((true == tmpSlice.isObject() && 0 == tmpSlice.length()));
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate tests
////////////////////////////////////////////////////////////////////////////////

}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
