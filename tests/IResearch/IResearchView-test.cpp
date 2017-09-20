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

#include "analysis/token_attributes.hpp"
#include "search/scorers.hpp"
#include "utils/locale_utils.hpp"
#include "utils/log.hpp"
#include "utils/utf8_path.hpp"

#include "ApplicationFeatures/JemallocFeature.h"
#include "Aql/AqlFunctionFeature.h"
#include "Aql/AstNode.h"
#include "Aql/Function.h"
#include "Aql/SortCondition.h"
#include "Basics/ArangoGlobalContext.h"
#include "Basics/files.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "IResearch/IResearchFeature.h"
#include "IResearch/IResearchLinkMeta.h"
#include "IResearch/IResearchMMFilesLink.h"
#include "IResearch/IResearchView.h"
#include "IResearch/SystemDatabaseFeature.h"
#include "Logger/Logger.h"
#include "Logger/LogTopic.h"
#include "Random/RandomFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/FlushFeature.h"
#include "RestServer/DatabasePathFeature.h"
#include "RestServer/FeatureCacheFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "RestServer/ViewTypesFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "Transaction/StandaloneContext.h"
#include "Transaction/UserTransaction.h"
#include "Utils/OperationOptions.h"
#include "velocypack/Iterator.h"
#include "velocypack/Parser.h"
#include "V8Server/V8DealerFeature.h"
#include "Views/ViewIterator.h"
#include "VocBase/KeyGenerator.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/LogicalView.h"

NS_LOCAL

struct DocIdScorer: public irs::sort {
  DECLARE_SORT_TYPE() { static irs::sort::type_id type("test_doc_id"); return type; }
  static ptr make(const irs::string_ref&) { PTR_NAMED(DocIdScorer, ptr); return ptr; }
  DocIdScorer(): irs::sort(DocIdScorer::type()) { }
  virtual sort::prepared::ptr prepare() const override { PTR_NAMED(Prepared, ptr); return ptr; }

  struct Prepared: public irs::sort::prepared_base<uint64_t> {
    virtual void add(score_t& dst, const score_t& src) const override { dst = src; }
    virtual irs::flags const& features() const override { return irs::flags::empty_instance(); }
    virtual bool less(const score_t& lhs, const score_t& rhs) const override { return lhs < rhs; }
    virtual irs::sort::collector::ptr prepare_collector() const override { return nullptr; }
    virtual void prepare_score(score_t& score) const override { }
    virtual irs::sort::scorer::ptr prepare_scorer(
      irs::sub_reader const& segment,
      irs::term_reader const& field,
      irs::attribute_store const& query_attrs,
      irs::attribute_view const& doc_attrs
    ) const override {
      return irs::sort::scorer::make<Scorer>(doc_attrs.get<irs::document>());
    }
  };

  struct Scorer: public irs::sort::scorer {
    irs::attribute_view::ref<irs::document> const& _doc;
    Scorer(irs::attribute_view::ref<irs::document> const& doc): _doc(doc) { }
    virtual void score(irs::byte_type* score_buf) override {
      reinterpret_cast<uint64_t&>(*score_buf) = _doc.get()->value;
    }
  };
};

REGISTER_SCORER(DocIdScorer);

NS_END

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

struct IResearchViewSetup {
  StorageEngineMock engine;
  arangodb::application_features::ApplicationServer server;
  std::unique_ptr<TRI_vocbase_t> system;
  std::vector<std::pair<arangodb::application_features::ApplicationFeature*, bool>> features;
  std::string testFilesystemPath;

  IResearchViewSetup(): server(nullptr, nullptr) {
    arangodb::EngineSelectorFeature::ENGINE = &engine;

    arangodb::tests::init();

    // setup required application features
    features.emplace_back(new arangodb::V8DealerFeature(&server), false);
    features.emplace_back(new arangodb::ViewTypesFeature(&server), true);
    features.emplace_back(new arangodb::QueryRegistryFeature(&server), false);
    arangodb::application_features::ApplicationServer::server->addFeature(features.back().first);
    system = irs::memory::make_unique<TRI_vocbase_t>(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 0, TRI_VOC_SYSTEM_DATABASE);
    features.emplace_back(new arangodb::FeatureCacheFeature(&server), true);
    features.emplace_back(new arangodb::RandomFeature(&server), false); // required by AuthenticationFeature
    features.emplace_back(new arangodb::AuthenticationFeature(&server), true);
    features.emplace_back(new arangodb::DatabaseFeature(&server), false);
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

  ~IResearchViewSetup() {
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

TEST_CASE("IResearchViewTest", "[iresearch][iresearch-view]") {
  IResearchViewSetup s;
  UNUSED(s);


SECTION("test_defaults") {
  auto namedJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\" }");
  auto json = arangodb::velocypack::Parser::fromJson("{}");

  // existing view definition
  {
    auto view = arangodb::iresearch::IResearchView::make(nullptr, json->slice(), false);
    CHECK((true == !view));
  }

  // existing view definition with LogicalView (for persistence)
  {
    arangodb::LogicalView logicalView(nullptr, namedJson->slice());
    auto view = arangodb::iresearch::IResearchView::make(&logicalView, json->slice(), false);
    CHECK((false == !view));

    arangodb::iresearch::IResearchViewMeta expectedMeta;

    expectedMeta._dataPath = std::string("-") + std::to_string(logicalView.id());

    arangodb::velocypack::Builder builder;

    builder.openObject();
    view->getPropertiesVPack(builder, true);
    builder.close();

    auto slice = builder.slice();
    arangodb::iresearch::IResearchViewMeta meta;
    std::string error;

    CHECK((7U == slice.length()));
    CHECK((!slice.hasKey("links"))); // for persistence so no links
    CHECK((meta.init(slice, error, logicalView) && expectedMeta == meta));
  }

  // existing view definition with LogicalView
  {
    arangodb::LogicalView logicalView(nullptr, namedJson->slice());
    auto view = arangodb::iresearch::IResearchView::make(&logicalView, json->slice(), false);
    CHECK((false == !view));

    arangodb::iresearch::IResearchViewMeta expectedMeta;

    expectedMeta._dataPath = std::string("-") + std::to_string(logicalView.id());

    arangodb::velocypack::Builder builder;

    builder.openObject();
    view->getPropertiesVPack(builder, false);
    builder.close();

    auto slice = builder.slice();
    arangodb::iresearch::IResearchViewMeta meta;
    std::string error;

    CHECK((8U == slice.length()));
    CHECK((slice.hasKey("links")));
    CHECK((meta.init(slice, error, logicalView) && expectedMeta == meta));
  }

  // new view definition
  {
    auto view = arangodb::iresearch::IResearchView::make(nullptr, json->slice(), true);
    CHECK((true == !view));
  }

  // new view definition with LogicalView (for persistence)
  {
    arangodb::LogicalView logicalView(nullptr, namedJson->slice());
    auto view = arangodb::iresearch::IResearchView::make(&logicalView, json->slice(), true);
    CHECK((false == !view));

    arangodb::iresearch::IResearchViewMeta expectedMeta;

    expectedMeta._dataPath = std::string("-") + std::to_string(logicalView.id());

    arangodb::velocypack::Builder builder;

    builder.openObject();
    view->getPropertiesVPack(builder, true);
    builder.close();

    auto slice = builder.slice();
    arangodb::iresearch::IResearchViewMeta meta;
    std::string error;

    CHECK((7U == slice.length()));
    CHECK((!slice.hasKey("links"))); // for persistence so no links
    CHECK((meta.init(slice, error, logicalView) && expectedMeta == meta));
  }

  // new view definition with LogicalView
  {
    arangodb::LogicalView logicalView(nullptr, namedJson->slice());
    auto view = arangodb::iresearch::IResearchView::make(&logicalView, json->slice(), true);
    CHECK((false == !view));

    arangodb::iresearch::IResearchViewMeta expectedMeta;

    expectedMeta._dataPath = std::string("-") + std::to_string(logicalView.id());

    arangodb::velocypack::Builder builder;

    builder.openObject();
    view->getPropertiesVPack(builder, false);
    builder.close();

    auto slice = builder.slice();
    arangodb::iresearch::IResearchViewMeta meta;
    std::string error;

    CHECK((8U == slice.length()));
    CHECK((meta.init(slice, error, logicalView) && expectedMeta == meta));

    auto tmpSlice = slice.get("links");
    CHECK((true == tmpSlice.isObject() && 0 == tmpSlice.length()));
  }

  // new view definition with links (not supported for link creation)
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\", \"id\": 100 }");
    auto viewJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"iresearch\", \"id\": 101, \"properties\": { \"links\": { \"testCollection\": {} } } }"); 

    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto* logicalCollection = vocbase.createCollection(collectionJson->slice());CHECK((nullptr != logicalCollection));
    CHECK((true == !vocbase.lookupView("testView")));
    CHECK((true == logicalCollection->getIndexes().empty()));
    auto logicalView = vocbase.createView(viewJson->slice(), 0);
    REQUIRE((false == !logicalView));
    auto* view = logicalView->getImplementation();
    REQUIRE((false == !view));
    auto* viewImpl = dynamic_cast<arangodb::iresearch::IResearchView*>(view);
    REQUIRE((nullptr != viewImpl));
    CHECK((0 == viewImpl->linkCount()));
    CHECK((true == logicalCollection->getIndexes().empty()));
  }
}

SECTION("test_drop") {
  std::string dataPath = ((irs::utf8_path()/=s.testFilesystemPath)/=std::string("deleteme")).utf8();
  auto json = arangodb::velocypack::Parser::fromJson("{ \
    \"name\": \"testView\", \
    \"type\": \"iresearch\", \
    \"properties\": { \
      \"dataPath\": \"" + arangodb::basics::StringUtils::replace(dataPath, "\\", "/") + "\" \
    } \
  }");

  CHECK((false == TRI_IsDirectory(dataPath.c_str())));

  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
  auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
  auto* logicalCollection = vocbase.createCollection(collectionJson->slice());
  CHECK((nullptr != logicalCollection));
  CHECK((true == !vocbase.lookupView("testView")));
  CHECK((true == logicalCollection->getIndexes().empty()));
  CHECK((false == TRI_IsDirectory(dataPath.c_str()))); // createView(...) will call open()
  auto logicalView = vocbase.createView(json->slice(), 0);
  REQUIRE((false == !logicalView));
  auto view = logicalView->getImplementation();
  REQUIRE((false == !view));

  CHECK((true == logicalCollection->getIndexes().empty()));
  CHECK((false == !vocbase.lookupView("testView")));
  CHECK((true == TRI_IsDirectory(dataPath.c_str())));
  CHECK((TRI_ERROR_NO_ERROR == vocbase.dropView("testView")));
  CHECK((true == logicalCollection->getIndexes().empty()));
  CHECK((true == !vocbase.lookupView("testView")));
  CHECK((false == TRI_IsDirectory(dataPath.c_str())));
}

SECTION("test_drop_with_link") {
  std::string dataPath = ((irs::utf8_path()/=s.testFilesystemPath)/=std::string("deleteme")).utf8();
  auto json = arangodb::velocypack::Parser::fromJson("{ \
    \"name\": \"testView\", \
    \"type\": \"iresearch\", \
    \"properties\": { \
      \"dataPath\": \"" + arangodb::basics::StringUtils::replace(dataPath, "\\", "/") + "\" \
    } \
  }");

  CHECK((false == TRI_IsDirectory(dataPath.c_str())));

  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
  auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
  auto* logicalCollection = vocbase.createCollection(collectionJson->slice());
  CHECK((nullptr != logicalCollection));
  CHECK((true == !vocbase.lookupView("testView")));
  CHECK((true == logicalCollection->getIndexes().empty()));
  CHECK((false == TRI_IsDirectory(dataPath.c_str()))); // createView(...) will call open()
  auto logicalView = vocbase.createView(json->slice(), 0);
  REQUIRE((false == !logicalView));
  auto view = logicalView->getImplementation();
  REQUIRE((false == !view));

  CHECK((true == logicalCollection->getIndexes().empty()));
  CHECK((false == !vocbase.lookupView("testView")));
  CHECK((true == TRI_IsDirectory(dataPath.c_str())));


  auto links = arangodb::velocypack::Parser::fromJson("{ \
    \"links\": { \"testCollection\": {} } \
  }");

  arangodb::Result res = logicalView->updateProperties(links->slice(), true, false);
  CHECK(true == res.ok());
  CHECK((false == logicalCollection->getIndexes().empty()));

  CHECK((TRI_ERROR_NO_ERROR == vocbase.dropView("testView")));
  CHECK((true == logicalCollection->getIndexes().empty()));
  CHECK((true == !vocbase.lookupView("testView")));
  CHECK((false == TRI_IsDirectory(dataPath.c_str())));
}

SECTION("test_insert") {
  static std::vector<std::string> const EMPTY;
  auto json = arangodb::velocypack::Parser::fromJson("{}");
  auto namedJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\" }");
  arangodb::aql::AstNode noop(arangodb::aql::AstNodeType::NODE_TYPE_FILTER);
  arangodb::aql::AstNode noopChild(true, arangodb::aql::AstNodeValueType::VALUE_TYPE_BOOL); // all

  noop.addMember(&noopChild);

  // in recovery (removes cid+rid before insert)
  {
    StorageEngineMock::inRecoveryResult = true;
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    arangodb::LogicalView logicalView(nullptr, namedJson->slice());
    auto viewImpl = arangodb::iresearch::IResearchView::make(&logicalView, json->slice(), false);
    CHECK((false == !viewImpl));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(viewImpl.get());
    CHECK((nullptr != view));

    {
      auto docJson = arangodb::velocypack::Parser::fromJson("{\"abc\": \"def\"}");
      arangodb::iresearch::IResearchLinkMeta linkMeta;
      arangodb::transaction::UserTransaction trx(arangodb::transaction::StandaloneContext::Create(&vocbase), EMPTY, EMPTY, EMPTY, arangodb::transaction::Options());

      linkMeta._includeAllFields = true;
      CHECK((trx.begin().ok()));
      CHECK((TRI_ERROR_NO_ERROR == view->insert(trx, 1, 1, docJson->slice(), linkMeta)));
      CHECK((TRI_ERROR_NO_ERROR == view->insert(trx, 1, 2, docJson->slice(), linkMeta)));
      CHECK((TRI_ERROR_NO_ERROR == view->insert(trx, 1, 1, docJson->slice(), linkMeta))); // 2nd time
      CHECK((TRI_ERROR_NO_ERROR == view->insert(trx, 1, 2, docJson->slice(), linkMeta))); // 2nd time
      CHECK((trx.commit().ok()));
      CHECK((view->sync()));
    }

    arangodb::transaction::UserTransaction trx(arangodb::transaction::StandaloneContext::Create(&vocbase), EMPTY, EMPTY, EMPTY, arangodb::transaction::Options());
    CHECK((trx.begin().ok()));
    std::unique_ptr<arangodb::ViewIterator> itr(view->iteratorForCondition(&trx, &noop, nullptr, nullptr));
    CHECK((false == !itr));
    size_t count = 0;
    CHECK((!itr->next([&count](arangodb::DocumentIdentifierToken const&)->void{ ++count; }, 10)));
    CHECK((2 == count));
  }

  // in recovery batch (removes cid+rid before insert)
  {
    StorageEngineMock::inRecoveryResult = true;
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    arangodb::LogicalView logicalView(nullptr, namedJson->slice());
    auto viewImpl = arangodb::iresearch::IResearchView::make(&logicalView, json->slice(), false);
    CHECK((false == !viewImpl));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(viewImpl.get());
    CHECK((nullptr != view));

    {
      auto docJson = arangodb::velocypack::Parser::fromJson("{\"abc\": \"def\"}");
      arangodb::iresearch::IResearchLinkMeta linkMeta;
      arangodb::transaction::UserTransaction trx(arangodb::transaction::StandaloneContext::Create(&vocbase), EMPTY, EMPTY, EMPTY, arangodb::transaction::Options());
      std::vector<std::pair<TRI_voc_rid_t, arangodb::velocypack::Slice>> batch = {
        { 1, docJson->slice() },
        { 2, docJson->slice() },
      };

      linkMeta._includeAllFields = true;
      CHECK((trx.begin().ok()));
      CHECK((TRI_ERROR_NO_ERROR == view->insert(trx, 1, batch, linkMeta)));
      CHECK((TRI_ERROR_NO_ERROR == view->insert(trx, 1, batch, linkMeta))); // 2nd time
      CHECK((trx.commit().ok()));
      CHECK((view->sync()));
    }

    arangodb::transaction::UserTransaction trx(arangodb::transaction::StandaloneContext::Create(&vocbase), EMPTY, EMPTY, EMPTY, arangodb::transaction::Options());
    CHECK((trx.begin().ok()));
    std::unique_ptr<arangodb::ViewIterator> itr(view->iteratorForCondition(&trx, &noop, nullptr, nullptr));
    CHECK((false == !itr));
    size_t count = 0;
    CHECK((!itr->next([&count](arangodb::DocumentIdentifierToken const&)->void{ ++count; }, 10)));
    CHECK((2 == count));
  }

  // not in recovery
  {
    StorageEngineMock::inRecoveryResult = false;
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    arangodb::LogicalView logicalView(nullptr, namedJson->slice());
    auto viewImpl = arangodb::iresearch::IResearchView::make(&logicalView, json->slice(), false);
    CHECK((false == !viewImpl));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(viewImpl.get());
    CHECK((nullptr != view));

    {
      auto docJson = arangodb::velocypack::Parser::fromJson("{\"abc\": \"def\"}");
      arangodb::iresearch::IResearchLinkMeta linkMeta;
      arangodb::transaction::UserTransaction trx(arangodb::transaction::StandaloneContext::Create(&vocbase), EMPTY, EMPTY, EMPTY, arangodb::transaction::Options());

      linkMeta._includeAllFields = true;
      CHECK((trx.begin().ok()));
      CHECK((TRI_ERROR_NO_ERROR == view->insert(trx, 1, 1, docJson->slice(), linkMeta)));
      CHECK((TRI_ERROR_NO_ERROR == view->insert(trx, 1, 2, docJson->slice(), linkMeta)));
      CHECK((TRI_ERROR_NO_ERROR == view->insert(trx, 1, 1, docJson->slice(), linkMeta))); // 2nd time
      CHECK((TRI_ERROR_NO_ERROR == view->insert(trx, 1, 2, docJson->slice(), linkMeta))); // 2nd time
      CHECK((trx.commit().ok()));
      CHECK((view->sync()));
    }

    arangodb::transaction::UserTransaction trx(arangodb::transaction::StandaloneContext::Create(&vocbase), EMPTY, EMPTY, EMPTY, arangodb::transaction::Options());
    CHECK((trx.begin().ok()));
    std::unique_ptr<arangodb::ViewIterator> itr(view->iteratorForCondition(&trx, &noop, nullptr, nullptr));
    CHECK((false == !itr));
    size_t count = 0;
    CHECK((!itr->next([&count](arangodb::DocumentIdentifierToken const&)->void{ ++count; }, 10)));
    CHECK((4 == count));
  }

  // not in recovery (with waitForSync)
  {
    StorageEngineMock::inRecoveryResult = false;
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    arangodb::LogicalView logicalView(nullptr, namedJson->slice());
    auto viewImpl = arangodb::iresearch::IResearchView::make(&logicalView, json->slice(), false);
    CHECK((false == !viewImpl));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(viewImpl.get());
    CHECK((nullptr != view));

    {
      auto docJson = arangodb::velocypack::Parser::fromJson("{\"abc\": \"def\"}");
      arangodb::iresearch::IResearchLinkMeta linkMeta;
      arangodb::transaction::Options options;
      options.waitForSync = true;
      arangodb::transaction::UserTransaction trx(arangodb::transaction::StandaloneContext::Create(&vocbase), EMPTY, EMPTY, EMPTY, options);

      linkMeta._includeAllFields = true;
      CHECK((trx.begin().ok()));
      CHECK((TRI_ERROR_NO_ERROR == view->insert(trx, 1, 1, docJson->slice(), linkMeta)));
      CHECK((TRI_ERROR_NO_ERROR == view->insert(trx, 1, 2, docJson->slice(), linkMeta)));
      CHECK((TRI_ERROR_NO_ERROR == view->insert(trx, 1, 1, docJson->slice(), linkMeta))); // 2nd time
      CHECK((TRI_ERROR_NO_ERROR == view->insert(trx, 1, 2, docJson->slice(), linkMeta))); // 2nd time
      CHECK((trx.commit().ok()));
    }

    arangodb::transaction::UserTransaction trx(arangodb::transaction::StandaloneContext::Create(&vocbase), EMPTY, EMPTY, EMPTY, arangodb::transaction::Options());
    CHECK((trx.begin().ok()));
    std::unique_ptr<arangodb::ViewIterator> itr(view->iteratorForCondition(&trx, &noop, nullptr, nullptr));
    CHECK((false == !itr));
    size_t count = 0;
    CHECK((!itr->next([&count](arangodb::DocumentIdentifierToken const&)->void{ ++count; }, 10)));
    CHECK((4 == count));
  }

  // not in recovery batch
  {
    StorageEngineMock::inRecoveryResult = false;
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    arangodb::LogicalView logicalView(nullptr, namedJson->slice());
    auto viewImpl = arangodb::iresearch::IResearchView::make(&logicalView, json->slice(), false);
    CHECK((false == !viewImpl));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(viewImpl.get());
    CHECK((nullptr != view));

    {
      auto docJson = arangodb::velocypack::Parser::fromJson("{\"abc\": \"def\"}");
      arangodb::iresearch::IResearchLinkMeta linkMeta;
      arangodb::transaction::UserTransaction trx(arangodb::transaction::StandaloneContext::Create(&vocbase), EMPTY, EMPTY, EMPTY, arangodb::transaction::Options());
      std::vector<std::pair<TRI_voc_rid_t, arangodb::velocypack::Slice>> batch = {
        { 1, docJson->slice() },
        { 2, docJson->slice() },
      };

      linkMeta._includeAllFields = true;
      CHECK((trx.begin().ok()));
      CHECK((TRI_ERROR_NO_ERROR == view->insert(trx, 1, batch, linkMeta)));
      CHECK((TRI_ERROR_NO_ERROR == view->insert(trx, 1, batch, linkMeta))); // 2nd time
      CHECK((trx.commit().ok()));
      CHECK((view->sync()));
    }

    arangodb::transaction::UserTransaction trx(arangodb::transaction::StandaloneContext::Create(&vocbase), EMPTY, EMPTY, EMPTY, arangodb::transaction::Options());
    CHECK((trx.begin().ok()));
    std::unique_ptr<arangodb::ViewIterator> itr(view->iteratorForCondition(&trx, &noop, nullptr, nullptr));
    CHECK((false == !itr));
    size_t count = 0;
    CHECK((!itr->next([&count](arangodb::DocumentIdentifierToken const&)->void{ ++count; }, 10)));
    CHECK((4 == count));
  }

  // not in recovery batch (waitForSync)
  {
    StorageEngineMock::inRecoveryResult = false;
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    arangodb::LogicalView logicalView(nullptr, namedJson->slice());
    auto viewImpl = arangodb::iresearch::IResearchView::make(&logicalView, json->slice(), false);
    CHECK((false == !viewImpl));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(viewImpl.get());
    CHECK((nullptr != view));

    {
      auto docJson = arangodb::velocypack::Parser::fromJson("{\"abc\": \"def\"}");
      arangodb::iresearch::IResearchLinkMeta linkMeta;
      arangodb::transaction::Options options;
      options.waitForSync = true;
      arangodb::transaction::UserTransaction trx(arangodb::transaction::StandaloneContext::Create(&vocbase), EMPTY, EMPTY, EMPTY, options);
      std::vector<std::pair<TRI_voc_rid_t, arangodb::velocypack::Slice>> batch = {
        { 1, docJson->slice() },
        { 2, docJson->slice() },
      };

      linkMeta._includeAllFields = true;
      CHECK((trx.begin().ok()));
      CHECK((TRI_ERROR_NO_ERROR == view->insert(trx, 1, batch, linkMeta)));
      CHECK((TRI_ERROR_NO_ERROR == view->insert(trx, 1, batch, linkMeta))); // 2nd time
      CHECK((trx.commit().ok()));
    }

    arangodb::transaction::UserTransaction trx(arangodb::transaction::StandaloneContext::Create(&vocbase), EMPTY, EMPTY, EMPTY, arangodb::transaction::Options());
    CHECK((trx.begin().ok()));
    std::unique_ptr<arangodb::ViewIterator> itr(view->iteratorForCondition(&trx, &noop, nullptr, nullptr));
    CHECK((false == !itr));
    size_t count = 0;
    CHECK((!itr->next([&count](arangodb::DocumentIdentifierToken const&)->void{ ++count; }, 10)));
    CHECK((4 == count));
  }
}

SECTION("test_move_datapath") {
  std::string createDataPath = ((irs::utf8_path()/=s.testFilesystemPath)/=std::string("deleteme0")).utf8();
  std::string updateDataPath = ((irs::utf8_path()/=s.testFilesystemPath)/=std::string("deleteme1")).utf8();
  auto namedJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\" }");
  auto createJson = arangodb::velocypack::Parser::fromJson("{ \
    \"name\": \"testView\", \
    \"type\": \"iresearch\", \
    \"properties\": { \
      \"dataPath\": \"" + arangodb::basics::StringUtils::replace(createDataPath, "\\", "/") + "\" \
    } \
  }");
  auto updateJson = arangodb::velocypack::Parser::fromJson("{ \
    \"dataPath\": \"" + arangodb::basics::StringUtils::replace(updateDataPath, "\\", "/") + "\" \
  }");

  CHECK((false == TRI_IsDirectory(createDataPath.c_str())));
  CHECK((false == TRI_IsDirectory(updateDataPath.c_str())));

  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
  CHECK((false == TRI_IsDirectory(createDataPath.c_str()))); // createView(...) will call open()
  auto logicalView = vocbase.createView(createJson->slice(), 0);
  REQUIRE((false == !logicalView));
  auto view = logicalView->getImplementation();
  REQUIRE((false == !view));

  CHECK((true == TRI_IsDirectory(createDataPath.c_str())));
  CHECK((view->updateProperties(updateJson->slice(), true, false).ok()));
  CHECK((false == TRI_IsDirectory(createDataPath.c_str())));
  CHECK((true == TRI_IsDirectory(updateDataPath.c_str())));
}

SECTION("test_open") {
  // absolute data path
  {
    std::string dataPath = ((irs::utf8_path()/=s.testFilesystemPath)/=std::string("deleteme")).utf8();
    auto namedJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\" }");
    auto json = arangodb::velocypack::Parser::fromJson("{ \
      \"dataPath\": \"" + arangodb::basics::StringUtils::replace(dataPath, "\\", "/") + "\" \
    }");

    CHECK((false == TRI_IsDirectory(dataPath.c_str())));
    arangodb::LogicalView logicalView(nullptr, namedJson->slice());
    auto view = arangodb::iresearch::IResearchView::make(&logicalView, json->slice(), false);

    REQUIRE((false == !view));
    CHECK((false == TRI_IsDirectory(dataPath.c_str())));
    view->open();
    CHECK((true == TRI_IsDirectory(dataPath.c_str())));
  }

  auto* dbPathFeature = arangodb::application_features::ApplicationServer::getFeature<arangodb::DatabasePathFeature>("DatabasePath");
  auto& origDirectory = dbPathFeature->directory();
  std::shared_ptr<arangodb::DatabasePathFeature> originalDirectory(
    dbPathFeature,
    [origDirectory](arangodb::DatabasePathFeature* ptr)->void {
      const_cast<std::string&>(ptr->directory()) = origDirectory;
    }
  );

  // relative data path
  {
    arangodb::options::ProgramOptions options("", "", "", nullptr);

    options.addPositional((irs::utf8_path()/=s.testFilesystemPath).utf8());
    dbPathFeature->validateOptions(std::shared_ptr<decltype(options)>(&options, [](decltype(options)*){})); // set data directory

    std::string dataPath = (((irs::utf8_path()/=s.testFilesystemPath)/=std::string("databases"))/=std::string("deleteme")).utf8();
    auto namedJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\" }");
    auto json = arangodb::velocypack::Parser::fromJson("{ \
      \"dataPath\": \"deleteme\" \
    }");

    CHECK((false == TRI_IsDirectory(dataPath.c_str())));
    arangodb::LogicalView logicalView(nullptr, namedJson->slice());
    auto view = arangodb::iresearch::IResearchView::make(&logicalView, json->slice(), false);
    CHECK((false == !view));
    CHECK((false == TRI_IsDirectory(dataPath.c_str())));
    view->open();
    CHECK((true == TRI_IsDirectory(dataPath.c_str())));
  }

  // default data path
  {
    arangodb::options::ProgramOptions options("", "", "", nullptr);

    options.addPositional((irs::utf8_path()/=s.testFilesystemPath).utf8());
    dbPathFeature->validateOptions(std::shared_ptr<decltype(options)>(&options, [](decltype(options)*){})); // set data directory

    std::string dataPath = (((irs::utf8_path()/=s.testFilesystemPath)/=std::string("databases"))/=std::string("testType-123")).utf8();
    auto namedJson = arangodb::velocypack::Parser::fromJson("{ \"id\": 123, \"name\": \"testView\", \"type\": \"testType\" }");
    auto json = arangodb::velocypack::Parser::fromJson("{}");

    CHECK((false == TRI_IsDirectory(dataPath.c_str())));
    arangodb::LogicalView logicalView(nullptr, namedJson->slice());
    auto view = arangodb::iresearch::IResearchView::make(&logicalView, json->slice(), false);
    CHECK((false == !view));
    CHECK((false == TRI_IsDirectory(dataPath.c_str())));
    view->open();
    CHECK((true == TRI_IsDirectory(dataPath.c_str())));
  }
}

SECTION("test_query") {
  auto createJson = arangodb::velocypack::Parser::fromJson("{ \
    \"name\": \"testView\", \
    \"type\": \"iresearch\" \
  }");
  static std::vector<std::string> const EMPTY;
  arangodb::aql::AstNode noop(arangodb::aql::AstNodeType::NODE_TYPE_FILTER);
  arangodb::aql::AstNode noopChild(true, arangodb::aql::AstNodeValueType::VALUE_TYPE_BOOL); // all

  noop.addMember(&noopChild);

  // no transaction provided
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalView = vocbase.createView(createJson->slice(), 0);
    REQUIRE((false == !logicalView));
    auto view = logicalView->getImplementation();
    REQUIRE((false == !view));

    CHECK((nullptr == view->iteratorForCondition(nullptr, &noop, nullptr, nullptr)));
  }

  // no filter/order provided, means "RETURN *"
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalView = vocbase.createView(createJson->slice(), 0);
    REQUIRE((false == !logicalView));
    auto view = logicalView->getImplementation();
    REQUIRE((false == !view));

    arangodb::transaction::UserTransaction trx(arangodb::transaction::StandaloneContext::Create(&vocbase), EMPTY, EMPTY, EMPTY, arangodb::transaction::Options());
    CHECK((trx.begin().ok()));
    std::unique_ptr<arangodb::ViewIterator> itr(view->iteratorForCondition(&trx, nullptr, nullptr, nullptr));

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

  // empty ordered iterator
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalView = vocbase.createView(createJson->slice(), 0);
    REQUIRE((false == !logicalView));
    auto* view = logicalView->getImplementation();
    REQUIRE((false == !view));

    std::vector<std::pair<arangodb::aql::Variable const*, bool>> sorts;
    std::vector<std::vector<arangodb::basics::AttributeName>> constAttributes;
    std::unordered_map<arangodb::aql::VariableId, arangodb::aql::AstNode const*> variableDefinitions;

    irs::string_ref attribute("testAttribute");
    arangodb::aql::AstNode nodeExpression(attribute.c_str(), attribute.size(), arangodb::aql::AstNodeValueType::VALUE_TYPE_STRING);
    arangodb::aql::Variable variable("testVariable", 0);

    variableDefinitions.emplace(variable.id, &nodeExpression); // add node for condition
    sorts.emplace_back(std::make_pair(&variable, true)); // add one condition

    arangodb::aql::SortCondition order(nullptr, sorts, constAttributes, variableDefinitions);
    arangodb::transaction::UserTransaction trx(arangodb::transaction::StandaloneContext::Create(&vocbase), EMPTY, EMPTY, EMPTY, arangodb::transaction::Options());
    CHECK((trx.begin().ok()));
    std::unique_ptr<arangodb::ViewIterator> itr(view->iteratorForCondition(&trx, &noop, nullptr, &order));

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
    REQUIRE((false == !logicalView));
    auto* view = logicalView->getImplementation();
    REQUIRE((false == !view));

    arangodb::transaction::UserTransaction trx(arangodb::transaction::StandaloneContext::Create(&vocbase), EMPTY, EMPTY, EMPTY, arangodb::transaction::Options());

    CHECK((trx.begin().ok()));
    std::unique_ptr<arangodb::ViewIterator> itr(view->iteratorForCondition(&trx, &noop, nullptr, nullptr));

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
    REQUIRE((false == !logicalView));
    auto* view = logicalView->getImplementation();
    REQUIRE((false == !view));

    arangodb::aql::SortCondition order;
    arangodb::transaction::UserTransaction trx(arangodb::transaction::StandaloneContext::Create(&vocbase), EMPTY, EMPTY, EMPTY, arangodb::transaction::Options());
    CHECK((trx.begin().ok()));
    std::unique_ptr<arangodb::ViewIterator> itr(view->iteratorForCondition(&trx, &noop, nullptr, &order));

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
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalView = vocbase.createView(createJson->slice(), 0);
    CHECK((false == !logicalView));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView->getImplementation());
    CHECK((false == !view));

    // fill with test data
    {
      auto doc = arangodb::velocypack::Parser::fromJson("{ \"key\": 1 }");
      arangodb::iresearch::IResearchLinkMeta meta;
      meta._includeAllFields = true;
      arangodb::transaction::UserTransaction trx(arangodb::transaction::StandaloneContext::Create(&vocbase), EMPTY, EMPTY, EMPTY, arangodb::transaction::Options());
      CHECK((trx.begin().ok()));

      for (size_t i = 0; i < 12; ++i) {
        view->insert(trx, 1, i, doc->slice(), meta);
      }

      CHECK((trx.commit().ok()));
      view->sync();
    }

    arangodb::aql::AstNode filter(arangodb::aql::AstNodeType::NODE_TYPE_FILTER);
    arangodb::aql::AstNode filterGe(arangodb::aql::AstNodeType::NODE_TYPE_OPERATOR_BINARY_GE);
    arangodb::aql::AstNode filterAttr(arangodb::aql::AstNodeType::NODE_TYPE_ATTRIBUTE_ACCESS);
    arangodb::aql::AstNode filterReference(arangodb::aql::AstNodeType::NODE_TYPE_REFERENCE);
    arangodb::aql::AstNode filterValue(int64_t(1), arangodb::aql::AstNodeValueType::VALUE_TYPE_INT);
    irs::string_ref attr("key");

    filterAttr.setStringValue(attr.c_str(), attr.size());
    filterAttr.addMember(&filterReference);
    filterGe.addMember(&filterAttr);
    filterGe.addMember(&filterValue);
    filter.addMember(&filterGe);

    std::vector<std::pair<arangodb::aql::Variable const*, bool>> sorts;
    std::vector<std::vector<arangodb::basics::AttributeName>> constAttributes;
    std::unordered_map<arangodb::aql::VariableId, arangodb::aql::AstNode const*> variableDefinitions;

    irs::string_ref attribute("testAttribute");
    arangodb::aql::AstNode nodeArgs(arangodb::aql::AstNodeType::NODE_TYPE_ARRAY);
    arangodb::aql::AstNode nodeOutVar(arangodb::aql::AstNodeType::NODE_TYPE_REFERENCE);
    arangodb::aql::AstNode nodeExpression(arangodb::aql::AstNodeType::NODE_TYPE_FCALL);
    arangodb::aql::Function nodeFunction("test_doc_id", "", false, true, true, false);
    arangodb::aql::Variable variable("testVariable", 0);

    nodeArgs.addMember(&nodeOutVar);
    nodeExpression.addMember(&nodeArgs);
    nodeExpression.setData(&nodeFunction);
    variableDefinitions.emplace(variable.id, &nodeExpression); // add node for condition
    sorts.emplace_back(std::make_pair(&variable, true)); // add one condition

    arangodb::aql::SortCondition order(nullptr, sorts, constAttributes, variableDefinitions);
    arangodb::transaction::UserTransaction trx(arangodb::transaction::StandaloneContext::Create(&vocbase), EMPTY, EMPTY, EMPTY, arangodb::transaction::Options());
    CHECK((trx.begin().ok()));
    std::unique_ptr<arangodb::ViewIterator> itr(view->iteratorForCondition(&trx, &filter, nullptr, &order));

    CHECK((false == !itr));
    CHECK((std::string("iresearch-ordered-iterator") == itr->typeName()));
    CHECK((&trx == itr->transaction()));
    CHECK((view == itr->view()));
    CHECK((false == itr->hasExtra()));
    CHECK_THROWS(itr->nextExtra([](arangodb::DocumentIdentifierToken const&, arangodb::velocypack::Slice)->void{}, 42));

    {
      std::vector<size_t> const expected = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
      size_t next = 0;
      CHECK((true == itr->next([&expected, &next](arangodb::DocumentIdentifierToken const& token)->void{ CHECK((token._data == expected[next++])); }, 10)));
      CHECK((expected.size() == next));
    }

    CHECK_NOTHROW((itr->reset()));
    uint64_t skipped = 0;
    CHECK_NOTHROW((itr->skip(5, skipped)));
    CHECK((5 == skipped));

    {
      std::vector<size_t> const expected = { 6, 7, 8, 9, 10, 11, 12 };
      size_t next = 0;
      CHECK((false == itr->next([&expected, &next](arangodb::DocumentIdentifierToken const& token)->void{ CHECK((token._data == expected[next++])); }, 10)));
      CHECK((expected.size() == next));
    }

    CHECK_NOTHROW((itr->skip(5, skipped)));
    CHECK((0 == skipped));

    CHECK_NOTHROW((itr->reset()));
  }

  // unordered iterator (nullptr sort condition)
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalView = vocbase.createView(createJson->slice(), 0);
    CHECK((false == !logicalView));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView->getImplementation());
    CHECK((false == !view));

    // fill with test data
    {
      auto doc = arangodb::velocypack::Parser::fromJson("{ \"key\": 1 }");
      arangodb::iresearch::IResearchLinkMeta meta;
      meta._includeAllFields = true;
      arangodb::transaction::UserTransaction trx(arangodb::transaction::StandaloneContext::Create(&vocbase), EMPTY, EMPTY, EMPTY, arangodb::transaction::Options());
      CHECK((trx.begin().ok()));

      for (size_t i = 0; i < 12; ++i) {
        view->insert(trx, 1, i, doc->slice(), meta);
      }

      CHECK((trx.commit().ok()));
      view->sync();
    }

    arangodb::transaction::UserTransaction trx(arangodb::transaction::StandaloneContext::Create(&vocbase), EMPTY, EMPTY, EMPTY, arangodb::transaction::Options());
    CHECK((trx.begin().ok()));
    std::unique_ptr<arangodb::ViewIterator> itr(view->iteratorForCondition(&trx, &noop, nullptr, nullptr));

    CHECK((false == !itr));
    CHECK((std::string("iresearch-unordered-iterator") == itr->typeName()));
    CHECK((&trx == itr->transaction()));
    CHECK((view == itr->view()));
    CHECK((false == itr->hasExtra()));
    CHECK_THROWS(itr->nextExtra([](arangodb::DocumentIdentifierToken const&, arangodb::velocypack::Slice)->void{}, 42));

    std::vector<size_t> actual;

    {
      std::set<size_t> expected = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12 };
      CHECK((true == itr->next([&expected, &actual](arangodb::DocumentIdentifierToken const& token)->void{ CHECK((1 == expected.erase(token._data))); actual.emplace_back(token._data); }, 10)));
      CHECK((10 == actual.size()));
    }

    CHECK_NOTHROW((itr->reset()));
    uint64_t skipped = 0;
    CHECK_NOTHROW((itr->skip(5, skipped)));
    CHECK((5 == skipped));

    {
      std::vector<size_t> expected0(actual.begin() += 5, actual.end()); // skip first 5, order must be same for rest
      std::unordered_set<size_t> expected1 = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12 };
      for (auto& entry: actual) expected1.erase(entry);
      size_t next = 0;
      CHECK((false == itr->next([&expected0, &expected1, &next](arangodb::DocumentIdentifierToken const& token)->void{ CHECK(((next < expected0.size() && expected0[next++] == token._data) || (next >= expected0.size() && 1 == expected1.erase(token._data)))); }, 10)));
      CHECK((expected0.size() == next && expected1.empty()));
    }

    CHECK_NOTHROW((itr->skip(5, skipped)));
    CHECK((0 == skipped));

    CHECK_NOTHROW((itr->reset()));
  }

  // unordered iterator (empty sort condition)
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalView = vocbase.createView(createJson->slice(), 0);
    CHECK((false == !logicalView));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView->getImplementation());
    CHECK((false == !view));

    // fill with test data
    {
      auto doc = arangodb::velocypack::Parser::fromJson("{ \"key\": 1 }");
      arangodb::iresearch::IResearchLinkMeta meta;
      meta._includeAllFields = true;
      arangodb::transaction::UserTransaction trx(arangodb::transaction::StandaloneContext::Create(&vocbase), EMPTY, EMPTY, EMPTY, arangodb::transaction::Options());
      CHECK((trx.begin().ok()));

      for (size_t i = 0; i < 12; ++i) {
        view->insert(trx, 1, i, doc->slice(), meta);
      }

      CHECK((trx.commit().ok()));
      view->sync();
    }

    arangodb::aql::SortCondition order;
    arangodb::transaction::UserTransaction trx(arangodb::transaction::StandaloneContext::Create(&vocbase), EMPTY, EMPTY, EMPTY, arangodb::transaction::Options());
    CHECK((trx.begin().ok()));
    std::unique_ptr<arangodb::ViewIterator> itr(view->iteratorForCondition(&trx, &noop, nullptr, &order));

    CHECK((false == !itr));
    CHECK((std::string("iresearch-unordered-iterator") == itr->typeName()));
    CHECK((&trx == itr->transaction()));
    CHECK((view == itr->view()));
    CHECK((false == itr->hasExtra()));
    CHECK_THROWS(itr->nextExtra([](arangodb::DocumentIdentifierToken const&, arangodb::velocypack::Slice)->void{}, 42));

    std::vector<size_t> actual;

    {
      std::set<size_t> expected = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12 };
      CHECK((true == itr->next([&expected, &actual](arangodb::DocumentIdentifierToken const& token)->void{ CHECK((1 == expected.erase(token._data))); actual.emplace_back(token._data); }, 10)));
      CHECK((10 == actual.size()));
    }

    CHECK_NOTHROW((itr->reset()));
    uint64_t skipped = 0;
    CHECK_NOTHROW((itr->skip(5, skipped)));
    CHECK((5 == skipped));

    {
      std::vector<size_t> expected0(actual.begin() += 5, actual.end()); // skip first 5, order must be same for rest
      std::unordered_set<size_t> expected1 = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12 };
      for (auto& entry: actual) expected1.erase(entry);
      size_t next = 0;
      CHECK((false == itr->next([&expected0, &expected1, &next](arangodb::DocumentIdentifierToken const& token)->void{ CHECK(((next < expected0.size() && expected0[next++] == token._data) || (next >= expected0.size() && 1 == expected1.erase(token._data)))); }, 10)));
      CHECK((expected0.size() == next && expected1.empty()));
    }

    CHECK_NOTHROW((itr->skip(5, skipped)));
    CHECK((0 == skipped));

    CHECK_NOTHROW((itr->reset()));
  }

  // FIXME TODO implement
  // add tests for filter
}

SECTION("test_register_link") {
  bool persisted = false;
  auto before = PhysicalViewMock::before;
  auto restore = irs::make_finally([&before]()->void { PhysicalViewMock::before = before; });
  PhysicalViewMock::before = [&persisted]()->void { persisted = true; };

  auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\", \"id\": 100 }");
  auto viewJson0 = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"iresearch\", \"id\": 101 }");
  auto viewJson1 = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"iresearch\", \"id\": 101, \"properties\": { \"collections\": [ 100 ] } }");
  auto linkJson  = arangodb::velocypack::Parser::fromJson("{ \"view\": 101 }");

  // new link in recovery
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto* logicalCollection = vocbase.createCollection(collectionJson->slice());
    auto logicalView = vocbase.createView(viewJson0->slice(), 0);
    REQUIRE((false == !logicalView));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView->getImplementation());
    REQUIRE((false == !view));

    CHECK((0 == view->linkCount()));

    StorageEngineMock::inRecoveryResult = true;
    auto restore = irs::make_finally([]()->void { StorageEngineMock::inRecoveryResult = false; });
    persisted = false;
    auto link = arangodb::iresearch::IResearchMMFilesLink::make(1, logicalCollection, linkJson->slice());
    CHECK((false == persisted));
    CHECK((false == !link));
    CHECK((1 == view->linkCount()));
  }

  // new link
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto* logicalCollection = vocbase.createCollection(collectionJson->slice());
    auto logicalView = vocbase.createView(viewJson0->slice(), 0);
    REQUIRE((false == !logicalView));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView->getImplementation());
    REQUIRE((false == !view));

    CHECK((0 == view->linkCount()));
    persisted = false;
    auto link = arangodb::iresearch::IResearchMMFilesLink::make(1, logicalCollection, linkJson->slice());
    CHECK((true == persisted));
    CHECK((false == !link));
    CHECK((1 == view->linkCount()));
  }

  // known link
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto* logicalCollection = vocbase.createCollection(collectionJson->slice());
    auto logicalView = vocbase.createView(viewJson1->slice(), 0);
    REQUIRE((false == !logicalView));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView->getImplementation());
    REQUIRE((false == !view));

    CHECK((1 == view->linkCount()));
    persisted = false;
    auto link0 = arangodb::iresearch::IResearchMMFilesLink::make(1, logicalCollection, linkJson->slice());
    CHECK((false == persisted));
    CHECK((false == !link0));
    CHECK((1 == view->linkCount()));
    persisted = false;
    auto link1 = arangodb::iresearch::IResearchMMFilesLink::make(1, logicalCollection, linkJson->slice());
    CHECK((false == persisted));
    CHECK((true == !link1));
    CHECK((1 == view->linkCount()));
  }
}

SECTION("test_unregister_link") {
  bool persisted = false;
  auto before = PhysicalViewMock::before;
  auto restore = irs::make_finally([&before]()->void { PhysicalViewMock::before = before; });
  PhysicalViewMock::before = [&persisted]()->void { persisted = true; };

  auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\", \"id\": 100 }");
  auto viewJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"iresearch\", \"id\": 101, \"properties\": { } }"); 

  // link removed before view (in recovery)
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto* logicalCollection = vocbase.createCollection(collectionJson->slice());
    auto logicalView = vocbase.createView(viewJson->slice(), 0);
    REQUIRE((false == !logicalView));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView->getImplementation());
    REQUIRE((false == !view));

    auto links = arangodb::velocypack::Parser::fromJson("{ \
      \"links\": { \"testCollection\": {} } \
    }");

    arangodb::Result res = logicalView->updateProperties(links->slice(), true, false);
    CHECK(true == res.ok());
    CHECK((false == logicalCollection->getIndexes().empty()));

    CHECK((1 == view->linkCount()));
    CHECK((nullptr != vocbase.lookupCollection("testCollection")));

    StorageEngineMock::inRecoveryResult = true;
    auto restore = irs::make_finally([]()->void { StorageEngineMock::inRecoveryResult = false; });
    persisted = false;
    CHECK((TRI_ERROR_NO_ERROR == vocbase.dropCollection(logicalCollection, true, -1)));
    CHECK((false == persisted));
    CHECK((nullptr == vocbase.lookupCollection("testCollection")));
    CHECK((0 == view->linkCount()));
    CHECK((false == !vocbase.lookupView("testView")));
    CHECK((TRI_ERROR_NO_ERROR == vocbase.dropView("testView")));
    CHECK((true == !vocbase.lookupView("testView")));
  }

  // link removed before view
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto* logicalCollection = vocbase.createCollection(collectionJson->slice());
    auto logicalView = vocbase.createView(viewJson->slice(), 0);
    REQUIRE((false == !logicalView));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView->getImplementation());
    REQUIRE((false == !view));

    auto links = arangodb::velocypack::Parser::fromJson("{ \
      \"links\": { \"testCollection\": {} } \
    }");

    arangodb::Result res = logicalView->updateProperties(links->slice(), true, false);
    CHECK(true == res.ok());
    CHECK((false == logicalCollection->getIndexes().empty()));

    CHECK((1 == view->linkCount()));
    CHECK((nullptr != vocbase.lookupCollection("testCollection")));
    persisted = false;
    CHECK((TRI_ERROR_NO_ERROR == vocbase.dropCollection(logicalCollection, true, -1)));
    CHECK((true == persisted));
    CHECK((nullptr == vocbase.lookupCollection("testCollection")));
    CHECK((0 == view->linkCount()));
    CHECK((false == !vocbase.lookupView("testView")));
    CHECK((TRI_ERROR_NO_ERROR == vocbase.dropView("testView")));
    CHECK((true == !vocbase.lookupView("testView")));
  }

  // view removed before link
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto* logicalCollection = vocbase.createCollection(collectionJson->slice());
    auto logicalView = vocbase.createView(viewJson->slice(), 0);
    REQUIRE((false == !logicalView));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView->getImplementation());
    REQUIRE((false == !view));

    auto links = arangodb::velocypack::Parser::fromJson("{ \
      \"links\": { \"testCollection\": {} } \
    }");

    arangodb::Result res = logicalView->updateProperties(links->slice(), true, false);
    CHECK(true == res.ok());
    CHECK((false == logicalCollection->getIndexes().empty()));

    CHECK((1 == view->linkCount()));
    CHECK((false == !vocbase.lookupView("testView")));
    CHECK((TRI_ERROR_NO_ERROR == vocbase.dropView("testView")));
    CHECK((true == !vocbase.lookupView("testView")));
    CHECK((nullptr != vocbase.lookupCollection("testCollection")));
    CHECK((TRI_ERROR_NO_ERROR == vocbase.dropCollection(logicalCollection, true, -1)));
    CHECK((nullptr == vocbase.lookupCollection("testCollection")));
  }

  // view deallocated before link removed
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto* logicalCollection = vocbase.createCollection(collectionJson->slice());

    {
      auto createJson = arangodb::velocypack::Parser::fromJson("{}");
      auto updateJson = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection\": {} } }");
      auto logicalView = vocbase.createView(viewJson->slice(), 0);
      REQUIRE((false == !logicalView));
      auto* viewImpl = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView->getImplementation());
      REQUIRE((nullptr != viewImpl));
      CHECK((viewImpl->updateProperties(updateJson->slice(), true, false).ok()));
      CHECK((false == logicalCollection->getIndexes().empty()));
      CHECK((1 == viewImpl->linkCount()));

      auto factory = [](arangodb::LogicalView*, arangodb::velocypack::Slice const&, bool isNew)->std::unique_ptr<arangodb::ViewImplementation>{ return nullptr; };
      logicalView->spawnImplementation(factory, createJson->slice(), true); // ensure destructor for ViewImplementation is called
      CHECK((false == logicalCollection->getIndexes().empty()));
    }

    // create a new view with same ID to validate links
    {
      auto json = arangodb::velocypack::Parser::fromJson("{}");
      arangodb::LogicalView logicalView(&vocbase, viewJson->slice());
      auto view = arangodb::iresearch::IResearchView::make(&logicalView, json->slice(), true);
      REQUIRE((false == !view));
      auto* viewImpl = dynamic_cast<arangodb::iresearch::IResearchView*>(view.get());
      REQUIRE((nullptr != viewImpl));
      CHECK((0 == viewImpl->linkCount()));

      for (auto& index: logicalCollection->getIndexes()) {
        auto* link = dynamic_cast<arangodb::iresearch::IResearchLink*>(index.get());
        REQUIRE((*link != *viewImpl)); // check that link is unregistred from view
      }
    }
  }
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
    REQUIRE((false == !logicalView));
    auto view = logicalView->getImplementation();
    REQUIRE((false == !view));

    // initial update (overwrite)
    {
      arangodb::iresearch::IResearchViewMeta expectedMeta;
      auto updateJson = arangodb::velocypack::Parser::fromJson("{ \
        \"locale\": \"en\", \
        \"threadsMaxIdle\": 10, \
        \"threadsMaxTotal\": 20 \
      }");

      expectedMeta._dataPath = std::string("iresearch-") + std::to_string(logicalView->id());
      expectedMeta._locale = irs::locale_utils::locale("en", true);
      expectedMeta._threadsMaxIdle = 10;
      expectedMeta._threadsMaxTotal = 20;
      CHECK((view->updateProperties(updateJson->slice(), false, false).ok()));

      arangodb::velocypack::Builder builder;

      builder.openObject();
      view->getPropertiesVPack(builder, false);
      builder.close();

      auto slice = builder.slice();
      arangodb::iresearch::IResearchViewMeta meta;
      std::string error;

      CHECK((8U == slice.length()));
      CHECK((meta.init(slice, error, *logicalView) && expectedMeta == meta));

      auto tmpSlice = slice.get("links");
      CHECK((true == tmpSlice.isObject() && 0 == tmpSlice.length()));
    }

    // subsequent update (overwrite)
    {
      arangodb::iresearch::IResearchViewMeta expectedMeta;
      auto updateJson = arangodb::velocypack::Parser::fromJson("{ \
        \"locale\": \"ru\" \
      }");

      expectedMeta._dataPath = std::string("iresearch-") + std::to_string(logicalView->id());
      expectedMeta._locale = irs::locale_utils::locale("ru", true);
      CHECK((view->updateProperties(updateJson->slice(), false, false).ok()));

      arangodb::velocypack::Builder builder;

      builder.openObject();
      view->getPropertiesVPack(builder, false);
      builder.close();

      auto slice = builder.slice();
      arangodb::iresearch::IResearchViewMeta meta;
      std::string error;

      CHECK((8U == slice.length()));
      CHECK((meta.init(slice, error, *logicalView) && expectedMeta == meta));

      auto tmpSlice = slice.get("links");
      CHECK((true == tmpSlice.isObject() && 0 == tmpSlice.length()));
    }
  }

  // overwrite links
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto collectionJson0 = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection0\" }");
    auto collectionJson1 = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection1\" }");
    auto* logicalCollection0 = vocbase.createCollection(collectionJson0->slice());
    REQUIRE((nullptr != logicalCollection0));
    auto* logicalCollection1 = vocbase.createCollection(collectionJson1->slice());
    REQUIRE((nullptr != logicalCollection1));
    auto logicalView = vocbase.createView(createJson->slice(), 0);
    REQUIRE((false == !logicalView));
    auto view = logicalView->getImplementation();
    REQUIRE((false == !view));
    CHECK((true == logicalCollection0->getIndexes().empty()));
    CHECK((true == logicalCollection1->getIndexes().empty()));

    // initial creation
    {
      auto updateJson = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection0\": {} } }");
      arangodb::iresearch::IResearchViewMeta expectedMeta;
      std::unordered_map<std::string, arangodb::iresearch::IResearchLinkMeta> expectedLinkMeta;

      expectedMeta._collections.insert(logicalCollection0->cid());
      expectedMeta._dataPath = std::string("iresearch-") + std::to_string(logicalView->id());
      expectedLinkMeta["testCollection0"]; // use defaults
      CHECK((view->updateProperties(updateJson->slice(), true, false).ok()));

      arangodb::velocypack::Builder builder;

      builder.openObject();
      view->getPropertiesVPack(builder, false);
      builder.close();

      auto slice = builder.slice();
      arangodb::iresearch::IResearchViewMeta meta;
      std::string error;

      CHECK((8U == slice.length()));
      CHECK((meta.init(slice, error, *logicalView) && expectedMeta == meta));

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
      CHECK((false == logicalCollection0->getIndexes().empty()));
      CHECK((true == logicalCollection1->getIndexes().empty()));
    }

    // update overwrite links
    {
      auto updateJson = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection1\": {} } }");
      arangodb::iresearch::IResearchViewMeta expectedMeta;
      std::unordered_map<std::string, arangodb::iresearch::IResearchLinkMeta> expectedLinkMeta;

      expectedMeta._collections.insert(logicalCollection1->cid());
      expectedMeta._dataPath = std::string("iresearch-") + std::to_string(logicalView->id());
      expectedLinkMeta["testCollection1"]; // use defaults
      CHECK((view->updateProperties(updateJson->slice(), false, false).ok()));

      arangodb::velocypack::Builder builder;

      builder.openObject();
      view->getPropertiesVPack(builder, false);
      builder.close();

      auto slice = builder.slice();
      arangodb::iresearch::IResearchViewMeta meta;
      std::string error;

      CHECK((8U == slice.length()));
      CHECK((meta.init(slice, error, *logicalView) && expectedMeta == meta));

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
      CHECK((true == logicalCollection0->getIndexes().empty()));
      CHECK((false == logicalCollection1->getIndexes().empty()));
    }
  }
}

SECTION("test_update_partial") {
  auto createJson = arangodb::velocypack::Parser::fromJson("{ \
    \"name\": \"testView\", \
    \"type\": \"iresearch\" \
  }");
  bool persisted = false;
  auto before = PhysicalViewMock::before;
  auto restore = irs::make_finally([&before]()->void { PhysicalViewMock::before = before; });
  PhysicalViewMock::before = [&persisted]()->void { persisted = true; };

  // modify meta params
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalView = vocbase.createView(createJson->slice(), 0);
    REQUIRE((false == !logicalView));
    auto view = logicalView->getImplementation();
    REQUIRE((false == !view));

    arangodb::iresearch::IResearchViewMeta expectedMeta;
    auto updateJson = arangodb::velocypack::Parser::fromJson("{ \
      \"locale\": \"en\", \
      \"threadsMaxIdle\": 10, \
      \"threadsMaxTotal\": 20 \
    }");

    expectedMeta._dataPath = std::string("iresearch-") + std::to_string(logicalView->id());
    expectedMeta._locale = irs::locale_utils::locale("en", true);
    expectedMeta._threadsMaxIdle = 10;
    expectedMeta._threadsMaxTotal = 20;
    CHECK((view->updateProperties(updateJson->slice(), true, false).ok()));

    arangodb::velocypack::Builder builder;

    builder.openObject();
    view->getPropertiesVPack(builder, false);
    builder.close();

    auto slice = builder.slice();
    arangodb::iresearch::IResearchViewMeta meta;
    std::string error;

    CHECK((8U == slice.length()));
    CHECK((meta.init(slice, error, *logicalView) && expectedMeta == meta));

    auto tmpSlice = slice.get("links");
    CHECK((true == tmpSlice.isObject() && 0 == tmpSlice.length()));
  }

  // test rollback on meta modification failure
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalView = vocbase.createView(createJson->slice(), 0);
    REQUIRE((false == !logicalView));
    auto view = logicalView->getImplementation();
    REQUIRE((false == !view));

    std::string dataPath = ((irs::utf8_path()/=s.testFilesystemPath)/=std::string("deleteme")).utf8();
    auto res = TRI_CreateDatafile(dataPath, 1); // create a file where the data path directory should be
    arangodb::iresearch::IResearchViewMeta expectedMeta;
    auto updateJson = arangodb::velocypack::Parser::fromJson(std::string() + "{ \
      \"dataPath\": \"" + arangodb::basics::StringUtils::replace(dataPath, "\\", "/") + "\", \
      \"locale\": \"en\", \
      \"threadsMaxIdle\": 10, \
      \"threadsMaxTotal\": 20 \
    }");

    expectedMeta._dataPath = std::string("iresearch-") + std::to_string(logicalView->id());
    CHECK((TRI_ERROR_BAD_PARAMETER == view->updateProperties(updateJson->slice(), true, false).errorNumber()));

    arangodb::velocypack::Builder builder;

    builder.openObject();
    view->getPropertiesVPack(builder, false);
    builder.close();

    auto slice = builder.slice();
    arangodb::iresearch::IResearchViewMeta meta;
    std::string error;

    CHECK((8U == slice.length()));
    CHECK((meta.init(slice, error, *logicalView) && expectedMeta == meta));

    auto tmpSlice = slice.get("links");
    CHECK((true == tmpSlice.isObject() && 0 == tmpSlice.length()));
  }

  // test rollback on persist failure
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalView = vocbase.createView(createJson->slice(), 0);
    REQUIRE((false == !logicalView));
    auto view = logicalView->getImplementation();
    REQUIRE((false == !view));

    arangodb::iresearch::IResearchViewMeta expectedMeta;
    auto updateJson = arangodb::velocypack::Parser::fromJson("{ \
      \"locale\": \"en\", \
      \"threadsMaxIdle\": 10, \
      \"threadsMaxTotal\": 20 \
    }");

    expectedMeta._dataPath = std::string("iresearch-") + std::to_string(logicalView->id());

    PhysicalViewMock::persistPropertiesResult = TRI_ERROR_INTERNAL; // test fail
    CHECK((TRI_ERROR_INTERNAL == view->updateProperties(updateJson->slice(), true, false).errorNumber()));
    PhysicalViewMock::persistPropertiesResult = TRI_ERROR_NO_ERROR; // revert to valid

    arangodb::velocypack::Builder builder;

    builder.openObject();
    view->getPropertiesVPack(builder, false);
    builder.close();

    auto slice = builder.slice();
    arangodb::iresearch::IResearchViewMeta meta;
    std::string error;

    CHECK((8U == slice.length()));
    CHECK((meta.init(slice, error, *logicalView) && expectedMeta == meta));

    auto tmpSlice = slice.get("links");
    CHECK((true == tmpSlice.isObject() && 0 == tmpSlice.length()));
  }

  // add a new link (in recovery)
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
    auto* logicalCollection = vocbase.createCollection(collectionJson->slice());
    REQUIRE((nullptr != logicalCollection));
    auto logicalView = vocbase.createView(createJson->slice(), 0);
    REQUIRE((false == !logicalView));
    auto view = logicalView->getImplementation();
    REQUIRE((false == !view));

    auto updateJson = arangodb::velocypack::Parser::fromJson(
      "{ \"links\": { \"testCollection\": {} } }"
    );

    StorageEngineMock::inRecoveryResult = true;
    auto restore = irs::make_finally([]()->void { StorageEngineMock::inRecoveryResult = false; });
    persisted = false;
    CHECK((view->updateProperties(updateJson->slice(), true, false).ok()));
    CHECK((false == persisted));

    arangodb::velocypack::Builder builder;

    builder.openObject();
    view->getPropertiesVPack(builder, false);
    builder.close();

    auto slice = builder.slice();
    CHECK((
      true == slice.hasKey("links")
      && slice.get("links").isObject()
      && 1 == slice.get("links").length()
    ));
  }

  // add a new link
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
    auto* logicalCollection = vocbase.createCollection(collectionJson->slice());
    REQUIRE((nullptr != logicalCollection));
    auto logicalView = vocbase.createView(createJson->slice(), 0);
    REQUIRE((false == !logicalView));
    auto view = logicalView->getImplementation();
    REQUIRE((false == !view));

    arangodb::iresearch::IResearchViewMeta expectedMeta;
    std::unordered_map<std::string, arangodb::iresearch::IResearchLinkMeta> expectedLinkMeta;
    auto updateJson = arangodb::velocypack::Parser::fromJson("{ \
      \"links\": { \
        \"testCollection\": {} \
      }}");

    expectedMeta._collections.insert(logicalCollection->cid());
    expectedMeta._dataPath = std::string("iresearch-") + std::to_string(logicalView->id());
    expectedLinkMeta["testCollection"]; // use defaults
    persisted = false;
    CHECK((view->updateProperties(updateJson->slice(), true, false).ok()));
    CHECK((true == persisted));

    arangodb::velocypack::Builder builder;

    builder.openObject();
    view->getPropertiesVPack(builder, false);
    builder.close();

    auto slice = builder.slice();
    arangodb::iresearch::IResearchViewMeta meta;
    std::string error;

    CHECK((8U == slice.length()));
    CHECK((meta.init(slice, error, *logicalView) && expectedMeta == meta));

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

  // add a new link to a collection with documents
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
    auto* logicalCollection = vocbase.createCollection(collectionJson->slice());
    REQUIRE((nullptr != logicalCollection));
    auto logicalView = vocbase.createView(createJson->slice(), 0);
    REQUIRE((false == !logicalView));
    auto view = logicalView->getImplementation();
    REQUIRE((false == !view));

    {
      static std::vector<std::string> const EMPTY;
      auto doc = arangodb::velocypack::Parser::fromJson("{ \"abc\": \"def\" }");
      arangodb::transaction::UserTransaction trx(arangodb::transaction::StandaloneContext::Create(&vocbase), EMPTY, EMPTY, EMPTY, arangodb::transaction::Options());

      CHECK((trx.begin().ok()));
      CHECK((trx.insert(logicalCollection->name(), doc->slice(), arangodb::OperationOptions()).successful()));
      CHECK((trx.commit().ok()));
    }

    arangodb::iresearch::IResearchViewMeta expectedMeta;
    std::unordered_map<std::string, arangodb::iresearch::IResearchLinkMeta> expectedLinkMeta;
    auto updateJson = arangodb::velocypack::Parser::fromJson("{ \
      \"links\": { \
        \"testCollection\": {} \
      }}");

    expectedMeta._collections.insert(logicalCollection->cid());
    expectedMeta._dataPath = std::string("iresearch-") + std::to_string(logicalView->id());
    expectedLinkMeta["testCollection"]; // use defaults
    persisted = false;
    CHECK((view->updateProperties(updateJson->slice(), true, false).ok()));
    CHECK((true == persisted));

    arangodb::velocypack::Builder builder;

    builder.openObject();
    view->getPropertiesVPack(builder, false);
    builder.close();

    auto slice = builder.slice();
    arangodb::iresearch::IResearchViewMeta meta;
    std::string error;

    CHECK((8U == slice.length()));
    CHECK((meta.init(slice, error, *logicalView) && expectedMeta == meta));

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
    REQUIRE((false == !logicalView));
    auto view = logicalView->getImplementation();
    REQUIRE((false == !view));

    arangodb::iresearch::IResearchViewMeta expectedMeta;

    expectedMeta._dataPath = std::string("iresearch-") + std::to_string(logicalView->id());

    auto updateJson = arangodb::velocypack::Parser::fromJson("{ \
      \"links\": { \
        \"testCollection\": {} \
      }}");

    CHECK((TRI_ERROR_BAD_PARAMETER == view->updateProperties(updateJson->slice(), true, false).errorNumber()));

    arangodb::velocypack::Builder builder;

    builder.openObject();
    view->getPropertiesVPack(builder, false);
    builder.close();

    auto slice = builder.slice();
    arangodb::iresearch::IResearchViewMeta meta;
    std::string error;

    CHECK((8U == slice.length()));
    CHECK((meta.init(slice, error, *logicalView) && expectedMeta == meta));

    auto tmpSlice = slice.get("links");
    CHECK((true == tmpSlice.isObject() && 0 == tmpSlice.length()));
  }

  // remove link (in recovery)
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
    auto* logicalCollection = vocbase.createCollection(collectionJson->slice());
    REQUIRE((nullptr != logicalCollection));
    auto logicalView = vocbase.createView(createJson->slice(), 0);
    REQUIRE((false == !logicalView));
    auto view = logicalView->getImplementation();
    REQUIRE((false == !view));

    {
      auto updateJson = arangodb::velocypack::Parser::fromJson(
        "{ \"links\": { \"testCollection\": {} } }"
      );
      persisted = false;
      CHECK((view->updateProperties(updateJson->slice(), true, false).ok()));
      CHECK((true == persisted));

      arangodb::velocypack::Builder builder;

      builder.openObject();
      view->getPropertiesVPack(builder, false);
      builder.close();

      auto slice = builder.slice();
      CHECK((
        true == slice.hasKey("links")
        && slice.get("links").isObject()
        && 1 == slice.get("links").length()
      ));
    }

    {
      auto updateJson = arangodb::velocypack::Parser::fromJson(
        "{ \"links\": { \"testCollection\": null } }"
      );

      StorageEngineMock::inRecoveryResult = true;
      auto restore = irs::make_finally([]()->void { StorageEngineMock::inRecoveryResult = false; });
      persisted = false;
      CHECK((view->updateProperties(updateJson->slice(), true, false).ok()));
      CHECK((false == persisted));

      arangodb::velocypack::Builder builder;

      builder.openObject();
      view->getPropertiesVPack(builder, false);
      builder.close();

      auto slice = builder.slice();
      CHECK((
        true == slice.hasKey("links")
        && slice.get("links").isObject()
        && 0 == slice.get("links").length()
      ));
    }
  }

  // remove link
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
    auto* logicalCollection = vocbase.createCollection(collectionJson->slice());
    REQUIRE((nullptr != logicalCollection));
    auto logicalView = vocbase.createView(createJson->slice(), 0);
    REQUIRE((false == !logicalView));
    auto view = logicalView->getImplementation();
    REQUIRE((false == !view));

    arangodb::iresearch::IResearchViewMeta expectedMeta;

    expectedMeta._collections.insert(logicalCollection->cid());
    expectedMeta._dataPath = std::string("iresearch-") + std::to_string(logicalView->id());

    {
      auto updateJson = arangodb::velocypack::Parser::fromJson("{ \
        \"links\": { \
          \"testCollection\": {} \
      }}");

      CHECK((view->updateProperties(updateJson->slice(), true, false).ok()));

      arangodb::velocypack::Builder builder;

      builder.openObject();
      view->getPropertiesVPack(builder, false);
      builder.close();

      auto slice = builder.slice();
      arangodb::iresearch::IResearchViewMeta meta;
      std::string error;

      CHECK((8U == slice.length()));
      CHECK((meta.init(slice, error, *logicalView) && expectedMeta == meta));

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
      view->getPropertiesVPack(builder, false);
      builder.close();

      auto slice = builder.slice();
      arangodb::iresearch::IResearchViewMeta meta;
      std::string error;

      CHECK((8U == slice.length()));
      CHECK((meta.init(slice, error, *logicalView) && expectedMeta == meta));

      auto tmpSlice = slice.get("links");
      CHECK((true == tmpSlice.isObject() && 0 == tmpSlice.length()));
    }
  }

  // remove link from non-existant collection
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalView = vocbase.createView(createJson->slice(), 0);
    REQUIRE((false == !logicalView));
    auto view = logicalView->getImplementation();
    REQUIRE((false == !view));

    arangodb::iresearch::IResearchViewMeta expectedMeta;

    expectedMeta._dataPath = std::string("iresearch-") + std::to_string(logicalView->id());

    auto updateJson = arangodb::velocypack::Parser::fromJson("{ \
      \"links\": { \
        \"testCollection\": null \
      }}");

    CHECK((TRI_ERROR_BAD_PARAMETER == view->updateProperties(updateJson->slice(), true, false).errorNumber()));

    arangodb::velocypack::Builder builder;

    builder.openObject();
    view->getPropertiesVPack(builder, false);
    builder.close();

    auto slice = builder.slice();
    arangodb::iresearch::IResearchViewMeta meta;
    std::string error;

    CHECK((8U == slice.length()));
    CHECK((meta.init(slice, error, *logicalView) && expectedMeta == meta));

    auto tmpSlice = slice.get("links");
    CHECK((true == tmpSlice.isObject() && 0 == tmpSlice.length()));
  }

  // remove non-existant link
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
    auto* logicalCollection = vocbase.createCollection(collectionJson->slice());
    REQUIRE((nullptr != logicalCollection));
    auto logicalView = vocbase.createView(createJson->slice(), 0);
    REQUIRE((false == !logicalView));
    auto view = logicalView->getImplementation();
    REQUIRE((false == !view));

    arangodb::iresearch::IResearchViewMeta expectedMeta;

    expectedMeta._dataPath = std::string("iresearch-") + std::to_string(logicalView->id());

    auto updateJson = arangodb::velocypack::Parser::fromJson("{ \
      \"links\": { \
        \"testCollection\": null \
    }}");

    CHECK((view->updateProperties(updateJson->slice(), true, false).ok()));

    arangodb::velocypack::Builder builder;

    builder.openObject();
    view->getPropertiesVPack(builder, false);
    builder.close();

    auto slice = builder.slice();
    arangodb::iresearch::IResearchViewMeta meta;
    std::string error;

    CHECK((8U == slice.length()));
    CHECK((meta.init(slice, error, *logicalView) && expectedMeta == meta));

    auto tmpSlice = slice.get("links");
    CHECK((true == tmpSlice.isObject() && 0 == tmpSlice.length()));
  }

  // remove + add link to same collection (reindex)
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
    auto* logicalCollection = vocbase.createCollection(collectionJson->slice());
    REQUIRE((nullptr != logicalCollection));
    auto logicalView = vocbase.createView(createJson->slice(), 0);
    REQUIRE((false == !logicalView));
    auto view = logicalView->getImplementation();
    REQUIRE((false == !view));

    // initial add of link
    {
      auto updateJson = arangodb::velocypack::Parser::fromJson(
        "{ \"links\": { \"testCollection\": {} } }"
      );
      CHECK((view->updateProperties(updateJson->slice(), true, false).ok()));

      arangodb::velocypack::Builder builder;

      builder.openObject();
      view->getPropertiesVPack(builder, false);
      builder.close();

      auto slice = builder.slice();
      auto tmpSlice = slice.get("links");
      CHECK((true == tmpSlice.isObject() && 1 == tmpSlice.length()));
    }

    // add + remove
    {
      auto updateJson = arangodb::velocypack::Parser::fromJson(
        "{ \"links\": { \"testCollection\": null, \"testCollection\": {} } }"
      );
      std::unordered_set<TRI_idx_iid_t> initial;

      for (auto& idx: logicalCollection->getIndexes()) {
        initial.emplace(idx->id());
      }

      CHECK((!initial.empty()));
      CHECK((view->updateProperties(updateJson->slice(), true, false).ok()));
      arangodb::velocypack::Builder builder;

      builder.openObject();
      view->getPropertiesVPack(builder, false);
      builder.close();

      auto slice = builder.slice();
      auto tmpSlice = slice.get("links");
      CHECK((true == tmpSlice.isObject() && 1 == tmpSlice.length()));

      std::unordered_set<TRI_idx_iid_t> actual;

      for (auto& index: logicalCollection->getIndexes()) {
        actual.emplace(index->id());
      }

      CHECK((initial != actual)); // a reindexing took place (link recreated)
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
