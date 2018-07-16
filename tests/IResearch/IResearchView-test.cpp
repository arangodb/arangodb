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
#include "ExpressionContextMock.h"

#include "analysis/token_attributes.hpp"
#include "search/scorers.hpp"
#include "utils/locale_utils.hpp"
#include "utils/log.hpp"
#include "utils/utf8_path.hpp"

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

#include "ApplicationFeatures/BasicPhase.h"
#include "ApplicationFeatures/ClusterPhase.h"
#include "ApplicationFeatures/DatabasePhase.h"
#include "ApplicationFeatures/GreetingsPhase.h"
#include "ApplicationFeatures/V8Phase.h"
#include "Cluster/ClusterComm.h"
#include "Cluster/ClusterFeature.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "IResearch/IResearchCommon.h"
#include "IResearch/IResearchDocument.h"
#include "IResearch/IResearchFeature.h"
#include "IResearch/IResearchLinkMeta.h"
#include "IResearch/IResearchMMFilesLink.h"
#include "IResearch/IResearchView.h"
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
#include "Transaction/Methods.h"
#include "Utils/OperationOptions.h"
#include "Utils/SingleCollectionTransaction.h"
#include "velocypack/Iterator.h"
#include "velocypack/Parser.h"
#include "V8Server/V8DealerFeature.h"
#include "VocBase/KeyGenerator.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/LogicalView.h"
#include "VocBase/ManagedDocumentResult.h"

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
    irs::attribute_view::ref<irs::document>::type const& _doc;
    Scorer(irs::attribute_view::ref<irs::document>::type const& doc): _doc(doc) { }
    virtual void score(irs::byte_type* score_buf) override {
      reinterpret_cast<uint64_t&>(*score_buf) = _doc.get()->value;
    }
  };
};

REGISTER_SCORER_TEXT(DocIdScorer, DocIdScorer::make);

NS_END

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

struct IResearchViewSetup {
  struct ClusterCommControl : arangodb::ClusterComm {
    static void reset() {
      arangodb::ClusterComm::_theInstanceInit.store(0);
    }
  };

  StorageEngineMock engine;
  arangodb::application_features::ApplicationServer server;
  std::unique_ptr<TRI_vocbase_t> system;
  std::map<std::string, std::pair<arangodb::application_features::ApplicationFeature*, bool>> features;
  std::vector<arangodb::application_features::ApplicationFeature*> orderedFeatures;
  std::string testFilesystemPath;

  IResearchViewSetup(): server(nullptr, nullptr) {
    arangodb::EngineSelectorFeature::ENGINE = &engine;

    arangodb::tests::init();

    // suppress INFO {authentication} Authentication is turned on (system only), authentication for unix sockets is turned on
    arangodb::LogTopic::setLogLevel(arangodb::Logger::AUTHENTICATION.name(), arangodb::LogLevel::WARN);

    // suppress log messages since tests check error conditions
    arangodb::LogTopic::setLogLevel(arangodb::Logger::FIXME.name(), arangodb::LogLevel::FATAL); // suppress ERROR recovery failure due to error from callback
    arangodb::LogTopic::setLogLevel(arangodb::Logger::CLUSTER.name(), arangodb::LogLevel::FATAL); // suppress ERROR recovery failure due to error from callback
    arangodb::LogTopic::setLogLevel(arangodb::iresearch::TOPIC.name(), arangodb::LogLevel::FATAL);
    irs::logger::output_le(iresearch::logger::IRL_FATAL, stderr);

    auto buildFeatureEntry = [&] (arangodb::application_features::ApplicationFeature* ftr, bool start) -> void {
      std::string name = ftr->name();
      features.emplace(name, std::make_pair(ftr, start));
    };

    buildFeatureEntry(new arangodb::application_features::BasicFeaturePhase(&server, false), false);
    buildFeatureEntry(new arangodb::application_features::ClusterFeaturePhase(&server), false);
    buildFeatureEntry(new arangodb::application_features::DatabaseFeaturePhase(&server), false);
    buildFeatureEntry(new arangodb::application_features::GreetingsFeaturePhase(&server, false), false);
    buildFeatureEntry(new arangodb::application_features::V8FeaturePhase(&server), false);
    // setup required application features
    buildFeatureEntry(new arangodb::V8DealerFeature(&server), false);
    buildFeatureEntry(new arangodb::ViewTypesFeature(&server), true);
    buildFeatureEntry(new arangodb::QueryRegistryFeature(&server), false);
    buildFeatureEntry(new arangodb::RandomFeature(&server), false); // required by AuthenticationFeature
    buildFeatureEntry(new arangodb::AuthenticationFeature(&server), true);
    buildFeatureEntry(new arangodb::ClusterFeature(&server), false);
    buildFeatureEntry(new arangodb::DatabaseFeature(&server), false);
    buildFeatureEntry(new arangodb::DatabasePathFeature(&server), false);
    buildFeatureEntry(new arangodb::TraverserEngineRegistryFeature(&server), false);
    buildFeatureEntry(new arangodb::AqlFeature(&server), true);
    buildFeatureEntry(new arangodb::aql::AqlFunctionFeature(&server), true); // required for IResearchAnalyzerFeature
    buildFeatureEntry(new arangodb::iresearch::IResearchAnalyzerFeature(&server), true);
    buildFeatureEntry(new arangodb::iresearch::IResearchFeature(&server), true);

    // We need this feature to be added now in order to create the system database
    arangodb::application_features::ApplicationServer::server->addFeature(features.at("QueryRegistry").first);

    system = irs::memory::make_unique<TRI_vocbase_t>(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 0, TRI_VOC_SYSTEM_DATABASE);
 
    buildFeatureEntry(new arangodb::iresearch::SystemDatabaseFeature(&server, system.get()), false); // required for IResearchAnalyzerFeature

    buildFeatureEntry(new arangodb::FlushFeature(&server), false); // do not start the thread

    #if USE_ENTERPRISE
      buildFeatureEntry(new arangodb::LdapFeature(&server), false); // required for AuthenticationFeature with USE_ENTERPRISE
    #endif

    for (auto& f : features) {
      arangodb::application_features::ApplicationServer::server->addFeature(f.second.first);
    }
    arangodb::application_features::ApplicationServer::server->setupDependencies(false);
    orderedFeatures = arangodb::application_features::ApplicationServer::server->getOrderedFeatures();

    for (auto& f : orderedFeatures) {
      f->prepare();
    }

    for (auto& f : orderedFeatures) {
      if (features.at(f->name()).second) {
        f->start();
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
  }

  ~IResearchViewSetup() {
    system.reset(); // destroy before reseting the 'ENGINE'
    TRI_RemoveDirectory(testFilesystemPath.c_str());
    arangodb::LogTopic::setLogLevel(arangodb::iresearch::TOPIC.name(), arangodb::LogLevel::DEFAULT);
    arangodb::LogTopic::setLogLevel(arangodb::Logger::FIXME.name(), arangodb::LogLevel::DEFAULT);
    arangodb::LogTopic::setLogLevel(arangodb::Logger::CLUSTER.name(), arangodb::LogLevel::DEFAULT); // suppress ERROR recovery failure due to error from callback
    arangodb::application_features::ApplicationServer::server = nullptr;
    arangodb::EngineSelectorFeature::ENGINE = nullptr;

    for (auto f = orderedFeatures.rbegin() ; f != orderedFeatures.rend(); ++f) { 
      if (features.at((*f)->name()).second) {
        (*f)->stop();
      }
    }

    for (auto f = orderedFeatures.rbegin() ; f != orderedFeatures.rend(); ++f) { 
      (*f)->unprepare();
    }

    ClusterCommControl::reset();
    arangodb::LogTopic::setLogLevel(arangodb::Logger::AUTHENTICATION.name(), arangodb::LogLevel::DEFAULT);
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

SECTION("test_type") {
  CHECK((arangodb::LogicalDataSource::Type::emplace(arangodb::velocypack::StringRef("arangosearch")) == arangodb::iresearch::DATA_SOURCE_TYPE));
}

SECTION("test_defaults") {
  auto json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\" }");

  // view definition with LogicalView (for persistence)
  {
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto view = arangodb::iresearch::IResearchView::make(vocbase, json->slice(), true, 0);
    CHECK((false == !view));

    arangodb::iresearch::IResearchViewMeta expectedMeta;
    arangodb::iresearch::IResearchViewMetaState expectedMetaState;
    arangodb::velocypack::Builder builder;

    builder.openObject();
    view->toVelocyPack(builder, true, true);
    builder.close();

    auto slice = builder.slice();
    arangodb::iresearch::IResearchViewMeta meta;
    arangodb::iresearch::IResearchViewMetaState metaState;
    std::string error;

    CHECK((8 == slice.length()));
    CHECK((slice.hasKey("globallyUniqueId") && slice.get("globallyUniqueId").isString() && false == slice.get("globallyUniqueId").copyString().empty()));
    CHECK(slice.get("name").copyString() == "testView");
    CHECK(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
    CHECK(false == slice.get("deleted").getBool());
    CHECK(false == slice.get("isSystem").getBool());
    slice = slice.get("properties");
    CHECK(slice.isObject());
    CHECK((3U == slice.length()));
    CHECK((!slice.hasKey("links"))); // for persistence so no links
    CHECK((meta.init(slice, error) && expectedMeta == meta));
    CHECK((true == metaState.init(slice, error) && expectedMetaState == metaState));
  }

  // view definition with LogicalView
  {
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto view = arangodb::iresearch::IResearchView::make(vocbase, json->slice(), true, 0);
    CHECK((false == !view));

    arangodb::iresearch::IResearchViewMeta expectedMeta;
    arangodb::iresearch::IResearchViewMetaState expectedMetaState;
    arangodb::velocypack::Builder builder;

    builder.openObject();
    view->toVelocyPack(builder, true, false);
    builder.close();

    auto slice = builder.slice();
    arangodb::iresearch::IResearchViewMeta meta;
    arangodb::iresearch::IResearchViewMetaState metaState;
    std::string error;

    CHECK(4 == slice.length());
    CHECK(slice.get("name").copyString() == "testView");
    CHECK(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
    CHECK((false == slice.hasKey("deleted")));
    slice = slice.get("properties");
    CHECK(slice.isObject());
    CHECK((4U == slice.length()));
    CHECK((meta.init(slice, error) && expectedMeta == meta));
    CHECK((true == metaState.init(slice, error) && expectedMetaState == metaState));

    auto tmpSlice = slice.get("links");
    CHECK((true == tmpSlice.isObject() && 0 == tmpSlice.length()));
  }

  // new view definition with links (not supported for link creation)
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\", \"id\": 100 }");
    auto viewJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\", \"id\": 101, \"properties\": { \"links\": { \"testCollection\": {} } } }");

    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto* logicalCollection = vocbase.createCollection(collectionJson->slice());
    CHECK((nullptr != logicalCollection));
    CHECK((true == !vocbase.lookupView("testView")));
    CHECK((true == logicalCollection->getIndexes().empty()));
    auto logicalView = vocbase.createView(viewJson->slice());
    REQUIRE((false == !logicalView));
    std::set<TRI_voc_cid_t> cids;
    logicalView->visitCollections([&cids](TRI_voc_cid_t cid)->bool { cids.emplace(cid); return true; });
    CHECK((0 == cids.size()));
    CHECK((true == logicalCollection->getIndexes().empty()));
  }
}

SECTION("test_drop") {
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
  std::string dataPath = ((((irs::utf8_path()/=s.testFilesystemPath)/=std::string("databases"))/=(std::string("database-") + std::to_string(vocbase.id())))/=std::string("arangosearch-123")).utf8();
  auto json = arangodb::velocypack::Parser::fromJson("{ \
    \"id\": 123, \
    \"name\": \"testView\", \
    \"type\": \"arangosearch\" \
  }");

  CHECK((false == TRI_IsDirectory(dataPath.c_str())));

  auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
  auto* logicalCollection = vocbase.createCollection(collectionJson->slice());
  CHECK((nullptr != logicalCollection));
  CHECK((true == !vocbase.lookupView("testView")));
  CHECK((true == logicalCollection->getIndexes().empty()));
  CHECK((false == TRI_IsDirectory(dataPath.c_str()))); // createView(...) will call open()
  auto view = vocbase.createView(json->slice());
  REQUIRE((false == !view));

  CHECK((true == logicalCollection->getIndexes().empty()));
  CHECK((false == !vocbase.lookupView("testView")));
  CHECK((true == TRI_IsDirectory(dataPath.c_str())));
  CHECK((true == vocbase.dropView(view->id(), false).ok()));
  CHECK((true == logicalCollection->getIndexes().empty()));
  CHECK((true == !vocbase.lookupView("testView")));
  CHECK((false == TRI_IsDirectory(dataPath.c_str())));
}

SECTION("test_drop_with_link") {
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
  std::string dataPath = ((((irs::utf8_path()/=s.testFilesystemPath)/=std::string("databases"))/=(std::string("database-") + std::to_string(vocbase.id())))/=std::string("arangosearch-123")).utf8();
  auto json = arangodb::velocypack::Parser::fromJson("{ \
    \"id\": 123, \
    \"name\": \"testView\", \
    \"type\": \"arangosearch\" \
  }");

  CHECK((false == TRI_IsDirectory(dataPath.c_str())));

  auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
  auto* logicalCollection = vocbase.createCollection(collectionJson->slice());
  CHECK((nullptr != logicalCollection));
  CHECK((true == !vocbase.lookupView("testView")));
  CHECK((true == logicalCollection->getIndexes().empty()));
  CHECK((false == TRI_IsDirectory(dataPath.c_str()))); // createView(...) will call open()
  auto view = vocbase.createView(json->slice());
  REQUIRE((false == !view));

  CHECK((true == logicalCollection->getIndexes().empty()));
  CHECK((false == !vocbase.lookupView("testView")));
  CHECK((true == TRI_IsDirectory(dataPath.c_str())));

  auto links = arangodb::velocypack::Parser::fromJson("{ \
    \"links\": { \"testCollection\": {} } \
  }");

  arangodb::Result res = view->updateProperties(links->slice(), true, false);
  CHECK(true == res.ok());
  CHECK((false == logicalCollection->getIndexes().empty()));

  CHECK((true == vocbase.dropView(view->id(), false).ok()));
  CHECK((true == logicalCollection->getIndexes().empty()));
  CHECK((true == !vocbase.lookupView("testView")));
  CHECK((false == TRI_IsDirectory(dataPath.c_str())));
}

SECTION("test_drop_cid") {
  static std::vector<std::string> const EMPTY;

  // cid not in list of collections for snapshot (view definition not updated, not persisted)
  {
    auto json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\" }");
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto viewImpl = arangodb::iresearch::IResearchView::make(vocbase, json->slice(), true, 0);
    CHECK((false == !viewImpl));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(viewImpl.get());
    CHECK((nullptr != view));

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
      view->insert(trx, 42, arangodb::LocalDocumentId(0), doc->slice(), meta);
      CHECK((trx.commit().ok()));
      view->sync();
    }

    // query
    {
      arangodb::transaction::Methods trx(
        arangodb::transaction::StandaloneContext::Create(vocbase),
        EMPTY,
        EMPTY,
        EMPTY,
        arangodb::transaction::Options()
      );
      auto* snapshot = view->snapshot(trx, true);
      CHECK(1 == snapshot->live_docs_count());
    }

    // drop cid 42
    {
      bool persisted = false;
      auto before = StorageEngineMock::before;
      auto restore = irs::make_finally([&before]()->void { StorageEngineMock::before = before; });
      StorageEngineMock::before = [&persisted]()->void { persisted = true; };

      CHECK((TRI_ERROR_NO_ERROR == view->drop(42)));
      CHECK((!persisted)); // drop() does not modify view meta if cid did not exist previously
      view->sync();
    }

    // query
    {
      arangodb::transaction::Methods trx(
        arangodb::transaction::StandaloneContext::Create(vocbase),
        EMPTY,
        EMPTY,
        EMPTY,
        arangodb::transaction::Options()
      );
      auto* snapshot = view->snapshot(trx, true);
      CHECK(0 == snapshot->live_docs_count());
    }
  }

  // cid in list of collections for snapshot (view definition updated+persisted)
  {
    auto json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"properties\": { \"collections\": [ 42 ] } }");
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto viewImpl = arangodb::iresearch::IResearchView::make(vocbase, json->slice(), true, 0);
    CHECK((false == !viewImpl));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(viewImpl.get());
    CHECK((nullptr != view));

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
      view->insert(trx, 42, arangodb::LocalDocumentId(0), doc->slice(), meta);
      CHECK((trx.commit().ok()));
      view->sync();
    }

    // query
    {
      arangodb::transaction::Methods trx(
        arangodb::transaction::StandaloneContext::Create(vocbase),
        EMPTY,
        EMPTY,
        EMPTY,
        arangodb::transaction::Options()
      );
      auto* snapshot = view->snapshot(trx, true);
      CHECK(1 == snapshot->live_docs_count());
    }

    // drop cid 42
    {
      bool persisted = false;
      auto before = StorageEngineMock::before;
      auto restore = irs::make_finally([&before]()->void { StorageEngineMock::before = before; });
      StorageEngineMock::before = [&persisted]()->void { persisted = true; };

      CHECK((TRI_ERROR_NO_ERROR == view->drop(42)));
      CHECK((persisted)); // drop() modifies view meta if cid existed previously
      view->sync();
    }

    // query
    {
      arangodb::transaction::Methods trx(
        arangodb::transaction::StandaloneContext::Create(vocbase),
        EMPTY,
        EMPTY,
        EMPTY,
        arangodb::transaction::Options()
      );
      auto* snapshot = view->snapshot(trx, true);
      CHECK(0 == snapshot->live_docs_count());
    }
  }

  // cid in list of collections for snapshot (view definition updated, not persisted until recovery is complete)
  {
    auto json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"collections\": [ 42 ] }");
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto viewImpl = arangodb::iresearch::IResearchView::make(vocbase, json->slice(), true, 0);
    CHECK((false == !viewImpl));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(viewImpl.get());
    CHECK((nullptr != view));

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
      view->insert(trx, 42, arangodb::LocalDocumentId(0), doc->slice(), meta);
      CHECK((trx.commit().ok()));
      view->sync();
    }

    // query
    {
      arangodb::transaction::Methods trx(
        arangodb::transaction::StandaloneContext::Create(vocbase),
        EMPTY,
        EMPTY,
        EMPTY,
        arangodb::transaction::Options()
      );
      auto* snapshot = view->snapshot(trx, true);
      CHECK(1 == snapshot->live_docs_count());
    }

    // drop cid 42
    {
      bool persisted = false;
      auto before = StorageEngineMock::before;
      auto restore = irs::make_finally([&before]()->void { StorageEngineMock::before = before; });
      StorageEngineMock::before = [&persisted]()->void { persisted = true; };
      auto beforeRecovery = StorageEngineMock::inRecoveryResult;
      StorageEngineMock::inRecoveryResult = true;
      auto restoreRecovery = irs::make_finally([&beforeRecovery]()->void { StorageEngineMock::inRecoveryResult = beforeRecovery; });

      CHECK((TRI_ERROR_NO_ERROR == view->drop(42)));
      CHECK((!persisted)); // drop() modifies view meta if cid existed previously (but not persisted until after recovery)
      view->sync();
    }

    // query
    {
      arangodb::transaction::Methods trx(
        arangodb::transaction::StandaloneContext::Create(vocbase),
        EMPTY,
        EMPTY,
        EMPTY,
        arangodb::transaction::Options()
      );
      auto* snapshot = view->snapshot(trx, true);
      CHECK(0 == snapshot->live_docs_count());
    }

    // collection not in view after drop (in recovery)
    {
      std::unordered_set<TRI_voc_cid_t> expected;
      std::set<TRI_voc_cid_t> actual;

      CHECK((view->visitCollections([&actual](TRI_voc_cid_t cid)->bool { actual.emplace(cid); return true; })));

      for (auto& cid: expected) {
        CHECK((1 == actual.erase(cid)));
      }

      CHECK((actual.empty()));
    }
  }

  // cid in list of collections for snapshot (view definition persist failure)
  {
    auto json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"properties\": { \"collections\": [ 42 ] } }");
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto viewImpl = arangodb::iresearch::IResearchView::make(vocbase, json->slice(), true, 0);
    CHECK((false == !viewImpl));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(viewImpl.get());
    CHECK((nullptr != view));

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
      view->insert(trx, 42, arangodb::LocalDocumentId(0), doc->slice(), meta);
      CHECK((trx.commit().ok()));
      view->sync();
    }

    // query
    {
      arangodb::transaction::Methods trx(
        arangodb::transaction::StandaloneContext::Create(vocbase),
        EMPTY,
        EMPTY,
        EMPTY,
        arangodb::transaction::Options()
      );
      auto* snapshot = view->snapshot(trx, true);
      CHECK(1 == snapshot->live_docs_count());
    }

    // drop cid 42
    {
      auto before = StorageEngineMock::before;
      auto restore = irs::make_finally([&before]()->void { StorageEngineMock::before = before; });
      StorageEngineMock::before = []()->void { throw std::exception(); };

      CHECK((TRI_ERROR_NO_ERROR != view->drop(42)));
      view->sync();
    }

    // query
    {
      arangodb::transaction::Methods trx(
        arangodb::transaction::StandaloneContext::Create(vocbase),
        EMPTY,
        EMPTY,
        EMPTY,
        arangodb::transaction::Options()
      );
      auto* snapshot = view->snapshot(trx, true);
      CHECK(1 == snapshot->live_docs_count());
    }

    // collection in view after drop failure
    {
      std::unordered_set<TRI_voc_cid_t> expected = { 42 };
      std::set<TRI_voc_cid_t> actual;

      CHECK((view->visitCollections([&actual](TRI_voc_cid_t cid)->bool { actual.emplace(cid); return true; })));

      for (auto& cid: expected) {
        CHECK((1 == actual.erase(cid)));
      }

      CHECK((actual.empty()));
    }
  }

  // cid in list of collections for snapshot (view definition persist failure on recovery completion)
  {
    auto json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"properties\": { \"collections\": [ 42 ] } }");
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto viewImpl = arangodb::iresearch::IResearchView::make(vocbase, json->slice(), true, 0);
    CHECK((false == !viewImpl));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(viewImpl.get());
    CHECK((nullptr != view));

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
      view->insert(trx, 42, arangodb::LocalDocumentId(0), doc->slice(), meta);
      CHECK((trx.commit().ok()));
      view->sync();
    }

    // query
    {
      arangodb::transaction::Methods trx(
        arangodb::transaction::StandaloneContext::Create(vocbase),
        EMPTY,
        EMPTY,
        EMPTY,
        arangodb::transaction::Options()
      );
      auto* snapshot = view->snapshot(trx, true);
      CHECK(1 == snapshot->live_docs_count());
    }

    // drop cid 42
    {
      bool persisted = false;
      auto before = StorageEngineMock::before;
      auto restore = irs::make_finally([&before]()->void { StorageEngineMock::before = before; });
      StorageEngineMock::before = [&persisted]()->void { persisted = true; };
      auto beforeRecovery = StorageEngineMock::inRecoveryResult;
      StorageEngineMock::inRecoveryResult = true;
      auto restoreRecovery = irs::make_finally([&beforeRecovery]()->void { StorageEngineMock::inRecoveryResult = beforeRecovery; });

      CHECK((TRI_ERROR_NO_ERROR == view->drop(42)));
      CHECK((!persisted)); // drop() modifies view meta if cid existed previously (but not persisted until after recovery)
      view->sync();
    }

    // query
    {
      arangodb::transaction::Methods trx(
        arangodb::transaction::StandaloneContext::Create(vocbase),
        EMPTY,
        EMPTY,
        EMPTY,
        arangodb::transaction::Options()
      );
      auto* snapshot = view->snapshot(trx, true);
      CHECK(0 == snapshot->live_docs_count());
    }

    // collection in view after drop failure
    {
      std::unordered_set<TRI_voc_cid_t> expected;
      std::set<TRI_voc_cid_t> actual;

      CHECK((view->visitCollections([&actual](TRI_voc_cid_t cid)->bool { actual.emplace(cid); return true; })));

      for (auto& cid: expected) {
        CHECK((1 == actual.erase(cid)));
      }

      CHECK((actual.empty()));
    }

    // persistence fails during execution of callback
    {
      auto before = StorageEngineMock::before;
      auto restore = irs::make_finally([&before]()->void { StorageEngineMock::before = before; });
      StorageEngineMock::before = []()->void { throw std::exception(); };
      auto* feature = arangodb::application_features::ApplicationServer::lookupFeature<
        arangodb::DatabaseFeature
      >("Database");

      CHECK_THROWS((feature->recoveryDone()));
    }
  }
}

SECTION("test_emplace_cid") {
  // emplace (already in list)
  {
    auto json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"properties\": { \"collections\": [ 42 ] } }");
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto viewImpl = arangodb::iresearch::IResearchView::make(vocbase, json->slice(), true, 0);
    CHECK((false == !viewImpl));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(viewImpl.get());
    CHECK((nullptr != view));

    // collection in view before
    {
      std::unordered_set<TRI_voc_cid_t> expected = { 42 };
      std::set<TRI_voc_cid_t> actual;

      CHECK((view->visitCollections([&actual](TRI_voc_cid_t cid)->bool { actual.emplace(cid); return true; })));

      for (auto& cid: expected) {
        CHECK((1 == actual.erase(cid)));
      }

      CHECK((actual.empty()));
    }

    // emplace cid 42
    {
      bool persisted = false;
      auto before = StorageEngineMock::before;
      auto restore = irs::make_finally([&before]()->void { StorageEngineMock::before = before; });
      StorageEngineMock::before = [&persisted]()->void { persisted = true; };

      CHECK((false == view->emplace(42)));
      CHECK((!persisted)); // emplace() does not modify view meta if cid existed previously
    }

    // collection in view after
    {
      std::unordered_set<TRI_voc_cid_t> expected = { 42 };
      std::set<TRI_voc_cid_t> actual;

      CHECK((view->visitCollections([&actual](TRI_voc_cid_t cid)->bool { actual.emplace(cid); return true; })));

      for (auto& cid: expected) {
        CHECK((1 == actual.erase(cid)));
      }

      CHECK((actual.empty()));
    }
  }

  // emplace (not in list)
  {
    auto json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\" }");
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto viewImpl = arangodb::iresearch::IResearchView::make(vocbase, json->slice(), true, 0);
    CHECK((false == !viewImpl));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(viewImpl.get());
    CHECK((nullptr != view));

    // collection in view before
    {
      std::unordered_set<TRI_voc_cid_t> expected;
      std::set<TRI_voc_cid_t> actual;

      CHECK((view->visitCollections([&actual](TRI_voc_cid_t cid)->bool { actual.emplace(cid); return true; })));

      for (auto& cid: expected) {
        CHECK((1 == actual.erase(cid)));
      }

      CHECK((actual.empty()));
    }

    // emplace cid 42
    {
      bool persisted = false;
      auto before = StorageEngineMock::before;
      auto restore = irs::make_finally([&before]()->void { StorageEngineMock::before = before; });
      StorageEngineMock::before = [&persisted]()->void { persisted = true; };

      CHECK((true == view->emplace(42)));
      CHECK((persisted)); // emplace() modifies view meta if cid did not exist previously
    }

    // collection in view after
    {
      std::unordered_set<TRI_voc_cid_t> expected = { 42 };
      std::set<TRI_voc_cid_t> actual;

      CHECK((view->visitCollections([&actual](TRI_voc_cid_t cid)->bool { actual.emplace(cid); return true; })));

      for (auto& cid: expected) {
        CHECK((1 == actual.erase(cid)));
      }

      CHECK((actual.empty()));
    }
  }

  // emplace (not in list, not persisted until recovery is complete)
  {
    auto json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\" }");
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto viewImpl = arangodb::iresearch::IResearchView::make(vocbase, json->slice(), true, 0);
    CHECK((false == !viewImpl));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(viewImpl.get());
    CHECK((nullptr != view));

    // collection in view before
    {
      std::unordered_set<TRI_voc_cid_t> expected;
      std::set<TRI_voc_cid_t> actual;

      CHECK((view->visitCollections([&actual](TRI_voc_cid_t cid)->bool { actual.emplace(cid); return true; })));

      for (auto& cid: expected) {
        CHECK((1 == actual.erase(cid)));
      }

      CHECK((actual.empty()));
    }

    // emplace cid 42
    {
      bool persisted = false;
      auto before = StorageEngineMock::before;
      auto restore = irs::make_finally([&before]()->void { StorageEngineMock::before = before; });
      StorageEngineMock::before = [&persisted]()->void { persisted = true; };
      auto beforeRecovery = StorageEngineMock::inRecoveryResult;
      StorageEngineMock::inRecoveryResult = true;
      auto restoreRecovery = irs::make_finally([&beforeRecovery]()->void { StorageEngineMock::inRecoveryResult = beforeRecovery; });

      CHECK((true == view->emplace(42)));
      CHECK((!persisted)); // emplace() modifies view meta if cid did not exist previously (but not persisted until after recovery)
    }

    // collection in view after
    {
      std::unordered_set<TRI_voc_cid_t> expected = { 42 };
      std::set<TRI_voc_cid_t> actual;

      CHECK((view->visitCollections([&actual](TRI_voc_cid_t cid)->bool { actual.emplace(cid); return true; })));

      for (auto& cid: expected) {
        CHECK((1 == actual.erase(cid)));
      }

      CHECK((actual.empty()));
    }
  }

  // emplace (not in list, view definition persist failure)
  {
    auto json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\" }");
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto viewImpl = arangodb::iresearch::IResearchView::make(vocbase, json->slice(), true, 0);
    CHECK((false == !viewImpl));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(viewImpl.get());
    CHECK((nullptr != view));

    // collection in view before
    {
      std::unordered_set<TRI_voc_cid_t> expected;
      std::set<TRI_voc_cid_t> actual;

      CHECK((view->visitCollections([&actual](TRI_voc_cid_t cid)->bool { actual.emplace(cid); return true; })));

      for (auto& cid: expected) {
        CHECK((1 == actual.erase(cid)));
      }

      CHECK((actual.empty()));
    }

    // emplace cid 42
    {
      auto before = StorageEngineMock::before;
      auto restore = irs::make_finally([&before]()->void { StorageEngineMock::before = before; });
      StorageEngineMock::before = []()->void { throw std::exception(); };

      CHECK((false == view->emplace(42)));
    }

    // collection in view after
    {
      std::unordered_set<TRI_voc_cid_t> expected;
      std::set<TRI_voc_cid_t> actual;

      CHECK((view->visitCollections([&actual](TRI_voc_cid_t cid)->bool { actual.emplace(cid); return true; })));

      for (auto& cid: expected) {
        CHECK((1 == actual.erase(cid)));
      }

      CHECK((actual.empty()));
    }
  }

  // emplace (not in list, view definition persist failure on recovery completion)
  {
    auto json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\" }");
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto viewImpl = arangodb::iresearch::IResearchView::make(vocbase, json->slice(), true, 0);
    CHECK((false == !viewImpl));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(viewImpl.get());
    CHECK((nullptr != view));

    // collection in view before
    {
      std::unordered_set<TRI_voc_cid_t> expected;
      std::set<TRI_voc_cid_t> actual;

      CHECK((view->visitCollections([&actual](TRI_voc_cid_t cid)->bool { actual.emplace(cid); return true; })));

      for (auto& cid: expected) {
        CHECK((1 == actual.erase(cid)));
      }

      CHECK((actual.empty()));
    }

    // emplace cid 42
    {
      bool persisted = false;
      auto before = StorageEngineMock::before;
      auto restore = irs::make_finally([&before]()->void { StorageEngineMock::before = before; });
      StorageEngineMock::before = [&persisted]()->void { persisted = true; };
      auto beforeRecovery = StorageEngineMock::inRecoveryResult;
      StorageEngineMock::inRecoveryResult = true;
      auto restoreRecovery = irs::make_finally([&beforeRecovery]()->void { StorageEngineMock::inRecoveryResult = beforeRecovery; });

      CHECK((true == view->emplace(42)));
      CHECK((!persisted)); // emplace() modifies view meta if cid did not exist previously (but not persisted until after recovery)
    }

    // collection in view after
    {
      std::unordered_set<TRI_voc_cid_t> expected = { 42 };
      std::set<TRI_voc_cid_t> actual;

      CHECK((view->visitCollections([&actual](TRI_voc_cid_t cid)->bool { actual.emplace(cid); return true; })));

      for (auto& cid: expected) {
        CHECK((1 == actual.erase(cid)));
      }

      CHECK((actual.empty()));
    }

    // persistence fails during execution of callback
    {
      auto before = StorageEngineMock::before;
      auto restore = irs::make_finally([&before]()->void { StorageEngineMock::before = before; });
      StorageEngineMock::before = []()->void { throw std::exception(); };
      auto* feature = arangodb::application_features::ApplicationServer::lookupFeature<
        arangodb::DatabaseFeature
      >("Database");

      CHECK_THROWS((feature->recoveryDone()));
    }
  }
}

SECTION("test_insert") {
  static std::vector<std::string> const EMPTY;
  auto json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\" }");
  arangodb::aql::AstNode noop(arangodb::aql::AstNodeType::NODE_TYPE_FILTER);
  arangodb::aql::AstNode noopChild(true, arangodb::aql::AstNodeValueType::VALUE_TYPE_BOOL); // all

  noop.addMember(&noopChild);

  // in recovery (removes cid+rid before insert)
  {
    auto before = StorageEngineMock::inRecoveryResult;
    StorageEngineMock::inRecoveryResult = true;
    auto restore = irs::make_finally([&before]()->void { StorageEngineMock::inRecoveryResult = before; });
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto viewImpl = arangodb::iresearch::IResearchView::make(vocbase, json->slice(), true, 0);
    CHECK((false == !viewImpl));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(viewImpl.get());
    CHECK((nullptr != view));
    view->open();

    {
      auto docJson = arangodb::velocypack::Parser::fromJson("{\"abc\": \"def\"}");
      arangodb::iresearch::IResearchLinkMeta linkMeta;
      arangodb::transaction::Methods trx(
        arangodb::transaction::StandaloneContext::Create(vocbase),
        EMPTY,
        EMPTY,
        EMPTY,
        arangodb::transaction::Options()
      );

      linkMeta._includeAllFields = true;
      CHECK((trx.begin().ok()));
      CHECK((TRI_ERROR_NO_ERROR == view->insert(trx, 1, arangodb::LocalDocumentId(1), docJson->slice(), linkMeta)));
      CHECK((TRI_ERROR_NO_ERROR == view->insert(trx, 1, arangodb::LocalDocumentId(2), docJson->slice(), linkMeta)));
      CHECK((TRI_ERROR_NO_ERROR == view->insert(trx, 1, arangodb::LocalDocumentId(1), docJson->slice(), linkMeta))); // 2nd time
      CHECK((TRI_ERROR_NO_ERROR == view->insert(trx, 1, arangodb::LocalDocumentId(2), docJson->slice(), linkMeta))); // 2nd time
      CHECK((trx.commit().ok()));
      CHECK((view->sync()));
    }

    arangodb::transaction::Methods trx(
      arangodb::transaction::StandaloneContext::Create(vocbase),
      EMPTY,
      EMPTY,
      EMPTY,
      arangodb::transaction::Options()
    );
    auto* snapshot = view->snapshot(trx, true);
    CHECK(2 == snapshot->live_docs_count());
  }

  // in recovery batch (removes cid+rid before insert)
  {
    auto before = StorageEngineMock::inRecoveryResult;
    StorageEngineMock::inRecoveryResult = true;
    auto restore = irs::make_finally([&before]()->void { StorageEngineMock::inRecoveryResult = before; });
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto viewImpl = arangodb::iresearch::IResearchView::make(vocbase, json->slice(), true, 0);
    CHECK((false == !viewImpl));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(viewImpl.get());
    CHECK((nullptr != view));
    view->open();

    {
      auto docJson = arangodb::velocypack::Parser::fromJson("{\"abc\": \"def\"}");
      arangodb::iresearch::IResearchLinkMeta linkMeta;
      arangodb::transaction::Methods trx(
        arangodb::transaction::StandaloneContext::Create(vocbase),
        EMPTY,
        EMPTY,
        EMPTY,
        arangodb::transaction::Options()
      );
      std::vector<std::pair<arangodb::LocalDocumentId, arangodb::velocypack::Slice>> batch = {
        { arangodb::LocalDocumentId(1), docJson->slice() },
        { arangodb::LocalDocumentId(2), docJson->slice() },
      };

      linkMeta._includeAllFields = true;
      CHECK((trx.begin().ok()));
      CHECK((TRI_ERROR_NO_ERROR == view->insert(trx, 1, batch, linkMeta)));
      CHECK((TRI_ERROR_NO_ERROR == view->insert(trx, 1, batch, linkMeta))); // 2nd time
      CHECK((trx.commit().ok()));
      CHECK((view->sync()));
    }

      arangodb::transaction::Methods trx(
        arangodb::transaction::StandaloneContext::Create(vocbase),
        EMPTY,
        EMPTY,
        EMPTY,
        arangodb::transaction::Options()
      );
    auto* snapshot = view->snapshot(trx, true);
    CHECK((2 == snapshot->docs_count()));
  }

  // not in recovery
  {
    StorageEngineMock::inRecoveryResult = false;
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto viewImpl = arangodb::iresearch::IResearchView::make(vocbase, json->slice(), true, 0);
    CHECK((false == !viewImpl));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(viewImpl.get());
    CHECK((nullptr != view));

    // validate cid count
    {
      std::set<TRI_voc_cid_t> cids;
      view->visitCollections([&cids](TRI_voc_cid_t cid)->bool { cids.emplace(cid); return true; });
      CHECK((0 == cids.size()));
      std::unordered_set<TRI_voc_cid_t> actual;
      arangodb::transaction::Methods trx(
        arangodb::transaction::StandaloneContext::Create(vocbase),
        EMPTY,
        EMPTY,
        EMPTY,
        arangodb::transaction::Options()
      );
      auto* snapshot = view->snapshot(trx, true);
      arangodb::iresearch::appendKnownCollections(actual, *snapshot);
      CHECK((actual.empty()));
    }

    {
      auto docJson = arangodb::velocypack::Parser::fromJson("{\"abc\": \"def\"}");
      arangodb::iresearch::IResearchLinkMeta linkMeta;
      arangodb::transaction::Methods trx(
        arangodb::transaction::StandaloneContext::Create(vocbase),
        EMPTY,
        EMPTY,
        EMPTY,
        arangodb::transaction::Options()
      );

      linkMeta._includeAllFields = true;
      CHECK((trx.begin().ok()));
      CHECK((TRI_ERROR_NO_ERROR == view->insert(trx, 1, arangodb::LocalDocumentId(1), docJson->slice(), linkMeta)));
      CHECK((TRI_ERROR_NO_ERROR == view->insert(trx, 1, arangodb::LocalDocumentId(2), docJson->slice(), linkMeta)));
      CHECK((TRI_ERROR_NO_ERROR == view->insert(trx, 1, arangodb::LocalDocumentId(1), docJson->slice(), linkMeta))); // 2nd time
      CHECK((TRI_ERROR_NO_ERROR == view->insert(trx, 1, arangodb::LocalDocumentId(2), docJson->slice(), linkMeta))); // 2nd time
      CHECK((trx.commit().ok()));
      CHECK((view->sync()));
    }

    arangodb::transaction::Methods trx(
      arangodb::transaction::StandaloneContext::Create(vocbase),
      EMPTY,
      EMPTY,
      EMPTY,
      arangodb::transaction::Options()
    );
    auto* snapshot = view->snapshot(trx, true);
    CHECK((4 == snapshot->docs_count()));

    // validate cid count
    {
      std::set<TRI_voc_cid_t> cids;
      view->visitCollections([&cids](TRI_voc_cid_t cid)->bool { cids.emplace(cid); return true; });
      CHECK((0 == cids.size()));
      std::unordered_set<TRI_voc_cid_t> expected = { 1 };
      std::unordered_set<TRI_voc_cid_t> actual;
      arangodb::transaction::Methods trx(
        arangodb::transaction::StandaloneContext::Create(vocbase),
        EMPTY,
        EMPTY,
        EMPTY,
        arangodb::transaction::Options()
      );
      auto* snapshot = view->snapshot(trx, true);
      arangodb::iresearch::appendKnownCollections(actual, *snapshot);

      for (auto& cid: expected) {
        CHECK((1 == actual.erase(cid)));
      }

      CHECK((actual.empty()));
    }
  }

  // not in recovery (with waitForSync)
  {
    StorageEngineMock::inRecoveryResult = false;
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto viewImpl = arangodb::iresearch::IResearchView::make(vocbase, json->slice(), true, 0);
    CHECK((false == !viewImpl));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(viewImpl.get());
    CHECK((nullptr != view));
    CHECK(view->category() == arangodb::LogicalView::category());

    {
      auto docJson = arangodb::velocypack::Parser::fromJson("{\"abc\": \"def\"}");
      arangodb::iresearch::IResearchLinkMeta linkMeta;
      arangodb::transaction::Options options;
      options.waitForSync = true;
      arangodb::transaction::Methods trx(
        arangodb::transaction::StandaloneContext::Create(vocbase),
        EMPTY,
        EMPTY,
        EMPTY,
        options
      );

      linkMeta._includeAllFields = true;
      CHECK((trx.begin().ok()));
      CHECK((TRI_ERROR_NO_ERROR == view->insert(trx, 1, arangodb::LocalDocumentId(1), docJson->slice(), linkMeta)));
      CHECK((TRI_ERROR_NO_ERROR == view->insert(trx, 1, arangodb::LocalDocumentId(2), docJson->slice(), linkMeta)));
      CHECK((TRI_ERROR_NO_ERROR == view->insert(trx, 1, arangodb::LocalDocumentId(1), docJson->slice(), linkMeta))); // 2nd time
      CHECK((TRI_ERROR_NO_ERROR == view->insert(trx, 1, arangodb::LocalDocumentId(2), docJson->slice(), linkMeta))); // 2nd time
      CHECK((trx.commit().ok()));
    }

    arangodb::transaction::Methods trx(
      arangodb::transaction::StandaloneContext::Create(vocbase),
      EMPTY,
      EMPTY,
      EMPTY,
      arangodb::transaction::Options()
    );
    auto* snapshot = view->snapshot(trx, true);
    CHECK((4 == snapshot->docs_count()));
  }

  // not in recovery batch
  {
    StorageEngineMock::inRecoveryResult = false;
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto viewImpl = arangodb::iresearch::IResearchView::make(vocbase, json->slice(), true, 0);
    CHECK((false == !viewImpl));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(viewImpl.get());
    CHECK((nullptr != view));

    {
      auto docJson = arangodb::velocypack::Parser::fromJson("{\"abc\": \"def\"}");
      arangodb::iresearch::IResearchLinkMeta linkMeta;
      arangodb::transaction::Methods trx(
        arangodb::transaction::StandaloneContext::Create(vocbase),
        EMPTY,
        EMPTY,
        EMPTY,
        arangodb::transaction::Options()
      );
      std::vector<std::pair<arangodb::LocalDocumentId, arangodb::velocypack::Slice>> batch = {
        { arangodb::LocalDocumentId(1), docJson->slice() },
        { arangodb::LocalDocumentId(2), docJson->slice() },
      };

      linkMeta._includeAllFields = true;
      CHECK((trx.begin().ok()));
      CHECK((TRI_ERROR_NO_ERROR == view->insert(trx, 1, batch, linkMeta)));
      CHECK((TRI_ERROR_NO_ERROR == view->insert(trx, 1, batch, linkMeta))); // 2nd time
      CHECK((trx.commit().ok()));
      CHECK((view->sync()));
    }

    arangodb::transaction::Methods trx(
      arangodb::transaction::StandaloneContext::Create(vocbase),
      EMPTY,
      EMPTY,
      EMPTY,
      arangodb::transaction::Options()
    );
    auto* snapshot = view->snapshot(trx, true);
    CHECK((4 == snapshot->docs_count()));
  }

  // not in recovery batch (waitForSync)
  {
    StorageEngineMock::inRecoveryResult = false;
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto viewImpl = arangodb::iresearch::IResearchView::make(vocbase, json->slice(), true, 0);
    CHECK((false == !viewImpl));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(viewImpl.get());
    CHECK((nullptr != view));

    {
      auto docJson = arangodb::velocypack::Parser::fromJson("{\"abc\": \"def\"}");
      arangodb::iresearch::IResearchLinkMeta linkMeta;
      arangodb::transaction::Options options;
      options.waitForSync = true;
      arangodb::transaction::Methods trx(
        arangodb::transaction::StandaloneContext::Create(vocbase),
        EMPTY,
        EMPTY,
        EMPTY,
        options
      );
      std::vector<std::pair<arangodb::LocalDocumentId, arangodb::velocypack::Slice>> batch = {
        { arangodb::LocalDocumentId(1), docJson->slice() },
        { arangodb::LocalDocumentId(2), docJson->slice() },
      };

      linkMeta._includeAllFields = true;
      CHECK((trx.begin().ok()));
      CHECK((TRI_ERROR_NO_ERROR == view->insert(trx, 1, batch, linkMeta)));
      CHECK((TRI_ERROR_NO_ERROR == view->insert(trx, 1, batch, linkMeta))); // 2nd time
      CHECK((trx.commit().ok()));
    }

    arangodb::transaction::Methods trx(
      arangodb::transaction::StandaloneContext::Create(vocbase),
      EMPTY,
      EMPTY,
      EMPTY,
      arangodb::transaction::Options()
    );
    auto* snapshot = view->snapshot(trx, true);
    CHECK((4 == snapshot->docs_count()));
  }
}

SECTION("test_open") {
  // default data path
  {
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    std::string dataPath = ((((irs::utf8_path()/=s.testFilesystemPath)/=std::string("databases"))/=(std::string("database-") + std::to_string(vocbase.id())))/=std::string("arangosearch-123")).utf8();
    auto json = arangodb::velocypack::Parser::fromJson("{ \"id\": 123, \"name\": \"testView\", \"type\": \"testType\" }");

    CHECK((false == TRI_IsDirectory(dataPath.c_str())));
    auto view = arangodb::iresearch::IResearchView::make(vocbase, json->slice(), true, 0);
    CHECK((false == !view));
    CHECK((false == TRI_IsDirectory(dataPath.c_str())));
    view->open();
    CHECK((true == TRI_IsDirectory(dataPath.c_str())));
  }
}

SECTION("test_query") {
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
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalView = vocbase.createView(createJson->slice());
    REQUIRE((false == !logicalView));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
    REQUIRE((false == !view));

    arangodb::transaction::Methods trx(
      arangodb::transaction::StandaloneContext::Create(vocbase),
      EMPTY,
      EMPTY,
      EMPTY,
      arangodb::transaction::Options()
    );
    auto* snapshot = view->snapshot(trx, true);
    CHECK(0 == snapshot->docs_count());
  }

  // ordered iterator
  {
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalView = vocbase.createView(createJson->slice());
    CHECK((false == !logicalView));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
    CHECK((false == !view));

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
        view->insert(trx, 1, arangodb::LocalDocumentId(i), doc->slice(), meta);
      }

      CHECK((trx.commit().ok()));
      view->sync();
    }

    arangodb::transaction::Methods trx(
      arangodb::transaction::StandaloneContext::Create(vocbase),
      EMPTY,
      EMPTY,
      EMPTY,
      arangodb::transaction::Options()
    );
    auto* snapshot = view->snapshot(trx, true);
    CHECK(12 == snapshot->docs_count());
  }

  // snapshot isolation
  {
    auto links = arangodb::velocypack::Parser::fromJson("{ \
      \"links\": { \"testCollection\": { \"includeAllFields\" : true } } \
    }");
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");

    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto* logicalCollection = vocbase.createCollection(collectionJson->slice());
    std::vector<std::string> collections{ logicalCollection->name() };
    auto logicalView = vocbase.createView(createJson->slice());
    CHECK((false == !logicalView));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
    CHECK((false == !view));
    arangodb::Result res = logicalView->updateProperties(links->slice(), true, false);
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
      view->sync();
    }

    arangodb::transaction::Methods trx0(
      arangodb::transaction::StandaloneContext::Create(vocbase),
      EMPTY,
      EMPTY,
      EMPTY,
      arangodb::transaction::Options()
    );
    auto* snapshot0 = view->snapshot(trx0, true);
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
      CHECK(view->sync());
    }

    // old reader sees same data as before
    CHECK(12 == snapshot0->docs_count());
    // new reader sees new data
    arangodb::transaction::Methods trx1(
      arangodb::transaction::StandaloneContext::Create(vocbase),
      EMPTY,
      EMPTY,
      EMPTY,
      arangodb::transaction::Options()
    );
    auto* snapshot1 = view->snapshot(trx1, true);
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
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto* logicalCollection = vocbase.createCollection(collectionJson->slice());
    auto logicalView = vocbase.createView(viewCreateJson->slice());
    REQUIRE((false == !logicalView));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
    REQUIRE((false == !view));
    arangodb::Result res = logicalView->updateProperties(viewUpdateJson->slice(), true, false);
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
        arangodb::transaction::Methods trx(
          arangodb::transaction::StandaloneContext::Create(vocbase),
          EMPTY,
          EMPTY,
          EMPTY,
          options
        );
        auto* snapshot = view->snapshot(trx, true);
        CHECK(i == snapshot->docs_count());
      }
    }
  }
}

SECTION("test_register_link") {
  bool persisted = false;
  auto before = StorageEngineMock::before;
  auto restore = irs::make_finally([&before]()->void { StorageEngineMock::before = before; });
  StorageEngineMock::before = [&persisted]()->void { persisted = true; };

  auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\", \"id\": 100 }");
  auto viewJson0 = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\", \"id\": 101 }");
  auto viewJson1 = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\", \"id\": 101, \"properties\": { \"collections\": [ 100 ] } }");
  auto linkJson  = arangodb::velocypack::Parser::fromJson("{ \"view\": 101 }");

  // new link in recovery
  {
    s.engine.views.clear();
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto* logicalCollection = vocbase.createCollection(collectionJson->slice());
    auto logicalView = vocbase.createView(viewJson0->slice());
    REQUIRE((false == !logicalView));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
    REQUIRE((false == !view));

    {
      arangodb::velocypack::Builder builder;
      builder.openObject();
      view->toVelocyPack(builder, false, false);
      builder.close();

      auto slice = builder.slice();
      CHECK(slice.isObject());
      CHECK(slice.get("id").copyString() == "101");
      CHECK(slice.get("name").copyString() == "testView");
      CHECK(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
      CHECK(slice.get("deleted").isNone()); // no system properties
      CHECK(3 == slice.length());
    }

    {
      std::set<TRI_voc_cid_t> cids;
      view->visitCollections([&cids](TRI_voc_cid_t cid)->bool { cids.emplace(cid); return true; });
      CHECK((0 == cids.size()));
    }

    auto before = StorageEngineMock::inRecoveryResult;
    StorageEngineMock::inRecoveryResult = true;
    auto restore = irs::make_finally([&before]()->void { StorageEngineMock::inRecoveryResult = before; });
    persisted = false;
    auto link = arangodb::iresearch::IResearchMMFilesLink::make(logicalCollection, linkJson->slice(), 1, false);
    CHECK((false == persisted));
    CHECK((false == !link));

    // link addition does modify view meta
    {
      std::unordered_set<TRI_voc_cid_t> expected = { 100 };
      std::set<TRI_voc_cid_t> actual;
      view->visitCollections([&actual](TRI_voc_cid_t cid)->bool { actual.emplace(cid); return true; });

      for (auto& cid: expected) {
        CHECK((1 == actual.erase(cid)));
      }

      CHECK((actual.empty()));
    }
  }

  std::vector<std::string> EMPTY;

  // new link
  {
    s.engine.views.clear();
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto* logicalCollection = vocbase.createCollection(collectionJson->slice());
    auto logicalView = vocbase.createView(viewJson0->slice());
    REQUIRE((false == !logicalView));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
    REQUIRE((false == !view));

    {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      view->toVelocyPack(builder, false, false);
      builder.close();

      auto slice = builder.slice();
      CHECK(slice.isObject());
      CHECK(slice.get("id").copyString() == "101");
      CHECK(slice.get("name").copyString() == "testView");
      CHECK(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
      CHECK(slice.get("deleted").isNone()); // no system properties
      CHECK(3 == slice.length());
    }

    {
      std::unordered_set<TRI_voc_cid_t> cids;
      view->sync();
      arangodb::transaction::Methods trx(
        arangodb::transaction::StandaloneContext::Create(vocbase),
        EMPTY,
        EMPTY,
        EMPTY,
        arangodb::transaction::Options()
      );
      auto* snapshot = view->snapshot(trx, true);
      arangodb::iresearch::appendKnownCollections(cids, *snapshot);
      CHECK((0 == cids.size()));
    }

    {
      std::set<TRI_voc_cid_t> actual;
      view->visitCollections([&actual](TRI_voc_cid_t cid)->bool { actual.emplace(cid); return true; });
      CHECK((actual.empty()));
    }

    persisted = false;
    auto link = arangodb::iresearch::IResearchMMFilesLink::make(logicalCollection, linkJson->slice(), 1, false);
    CHECK((true == persisted)); // link instantiation does modify and persist view meta
    CHECK((false == !link));
    std::unordered_set<TRI_voc_cid_t> cids;
    view->sync();
    arangodb::transaction::Methods trx(
      arangodb::transaction::StandaloneContext::Create(vocbase),
      EMPTY,
      EMPTY,
      EMPTY,
      arangodb::transaction::Options()
    );
    auto* snapshot = view->snapshot(trx, true);
    arangodb::iresearch::appendKnownCollections(cids, *snapshot);
    CHECK((0 == cids.size())); // link addition does trigger collection load

    // link addition does modify view meta
    {
      std::unordered_set<TRI_voc_cid_t> expected = { 100 };
      std::set<TRI_voc_cid_t> actual;
      view->visitCollections([&actual](TRI_voc_cid_t cid)->bool { actual.emplace(cid); return true; });

      for (auto& cid: expected) {
        CHECK((1 == actual.erase(cid)));
      }

      CHECK((actual.empty()));
    }
  }

  // known link
  {
    s.engine.views.clear();
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto* logicalCollection = vocbase.createCollection(collectionJson->slice());
    auto logicalView = vocbase.createView(viewJson1->slice());
    REQUIRE((false == !logicalView));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
    REQUIRE((false == !view));

    {
      std::unordered_set<TRI_voc_cid_t> cids;
      view->sync();
      arangodb::transaction::Methods trx(
        arangodb::transaction::StandaloneContext::Create(vocbase),
        EMPTY,
        EMPTY,
        EMPTY,
        arangodb::transaction::Options()
      );
      auto* snapshot = view->snapshot(trx, true);
      arangodb::iresearch::appendKnownCollections(cids, *snapshot);
      CHECK((0 == cids.size()));
    }

    {
      std::unordered_set<TRI_voc_cid_t> expected = { 100, 123 };
      std::set<TRI_voc_cid_t> actual = { 123 };
      view->visitCollections([&actual](TRI_voc_cid_t cid)->bool { actual.emplace(cid); return true; });

      for (auto& cid: expected) {
        CHECK((1 == actual.erase(cid)));
      }

      CHECK((actual.empty()));
    }

    persisted = false;
    auto link0 = arangodb::iresearch::IResearchMMFilesLink::make(logicalCollection, linkJson->slice(), 1, false);
    CHECK((false == persisted));
    CHECK((false == !link0));

    {
      std::unordered_set<TRI_voc_cid_t> cids;
      view->sync();
      arangodb::transaction::Methods trx(
        arangodb::transaction::StandaloneContext::Create(vocbase),
        EMPTY,
        EMPTY,
        EMPTY,
        arangodb::transaction::Options()
      );
      auto* snapshot = view->snapshot(trx, true);
      arangodb::iresearch::appendKnownCollections(cids, *snapshot);
      CHECK((0 == cids.size())); // link addition does trigger collection load
    }

    {
      std::unordered_set<TRI_voc_cid_t> expected = { 100, 123 };
      std::set<TRI_voc_cid_t> actual = { 123 };
      view->visitCollections([&actual](TRI_voc_cid_t cid)->bool { actual.emplace(cid); return true; });

      for (auto& cid: expected) {
        CHECK((1 == actual.erase(cid)));
      }

      CHECK((actual.empty()));
    }

    persisted = false;
    auto link1 = arangodb::iresearch::IResearchMMFilesLink::make(logicalCollection, linkJson->slice(), 1, false);
    CHECK((false == persisted));
    CHECK((false == !link1)); // duplicate link creation is allowed
    std::unordered_set<TRI_voc_cid_t> cids;
    view->sync();
    arangodb::transaction::Methods trx(
      arangodb::transaction::StandaloneContext::Create(vocbase),
      EMPTY,
      EMPTY,
      EMPTY,
      arangodb::transaction::Options()
    );
    auto* snapshot = view->snapshot(trx, true);
    arangodb::iresearch::appendKnownCollections(cids, *snapshot);
    CHECK((0 == cids.size())); // link addition does trigger collection load

    {
      std::unordered_set<TRI_voc_cid_t> expected = { 100, 123 };
      std::set<TRI_voc_cid_t> actual = { 123 };
      view->visitCollections([&actual](TRI_voc_cid_t cid)->bool { actual.emplace(cid); return true; });

      for (auto& cid: expected) {
        CHECK((1 == actual.erase(cid)));
      }

      CHECK((actual.empty()));
    }
  }
}

SECTION("test_unregister_link") {
  std::vector<std::string> const EMPTY;
  bool persisted = false;
  auto before = StorageEngineMock::before;
  auto restore = irs::make_finally([&before]()->void { StorageEngineMock::before = before; });
  StorageEngineMock::before = [&persisted]()->void { persisted = true; };

  auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\", \"id\": 100 }");
  auto viewJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\", \"id\": 101, \"properties\": { } }");

  // link removed before view (in recovery)
  {
    s.engine.views.clear();
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto* logicalCollection = vocbase.createCollection(collectionJson->slice());
    auto logicalView = vocbase.createView(viewJson->slice());
    REQUIRE((false == !logicalView));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
    REQUIRE((false == !view));

    // add a document to the view
    {
      static std::vector<std::string> const EMPTY;
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
      view->insert(trx, logicalCollection->id(), arangodb::LocalDocumentId(0), doc->slice(), meta);
      CHECK((trx.commit().ok()));
    }

    auto links = arangodb::velocypack::Parser::fromJson("{ \
      \"links\": { \"testCollection\": {} } \
    }");

    arangodb::Result res = logicalView->updateProperties(links->slice(), true, false);
    CHECK(true == res.ok());
    CHECK((false == logicalCollection->getIndexes().empty()));

    {
      std::unordered_set<TRI_voc_cid_t> cids;
      view->sync();
      arangodb::transaction::Methods trx(
        arangodb::transaction::StandaloneContext::Create(vocbase),
        EMPTY,
        EMPTY,
        EMPTY,
        arangodb::transaction::Options()
      );
      auto* snapshot = view->snapshot(trx, true);
      arangodb::iresearch::appendKnownCollections(cids, *snapshot);
      CHECK((1 == cids.size()));
    }

    {
      std::unordered_set<TRI_voc_cid_t> expected = { 100 };
      std::set<TRI_voc_cid_t> actual = { };
      view->visitCollections([&actual](TRI_voc_cid_t cid)->bool { actual.emplace(cid); return true; });

      for (auto& cid: expected) {
        CHECK((1 == actual.erase(cid)));
      }

      CHECK((actual.empty()));
    }

    CHECK((nullptr != vocbase.lookupCollection("testCollection")));

    auto before = StorageEngineMock::inRecoveryResult;
    StorageEngineMock::inRecoveryResult = true;
    auto restore = irs::make_finally([&before]()->void { StorageEngineMock::inRecoveryResult = before; });
    persisted = false;
    CHECK((true == vocbase.dropCollection(logicalCollection->id(), true, -1).ok()));
    CHECK((false == persisted)); // link removal does not persist view meta
    CHECK((nullptr == vocbase.lookupCollection("testCollection")));

    {
      std::unordered_set<TRI_voc_cid_t> cids;
      view->sync();
      arangodb::transaction::Methods trx(
        arangodb::transaction::StandaloneContext::Create(vocbase),
        EMPTY,
        EMPTY,
        EMPTY,
        arangodb::transaction::Options()
      );
      auto* snapshot = view->snapshot(trx, true);
      arangodb::iresearch::appendKnownCollections(cids, *snapshot);
      CHECK((0 == cids.size()));
    }

    {
      std::set<TRI_voc_cid_t> actual;
      view->visitCollections([&actual](TRI_voc_cid_t cid)->bool { actual.emplace(cid); return true; });
      CHECK((actual.empty())); // collection removal does modify view meta
    }

    CHECK((false == !vocbase.lookupView("testView")));
    CHECK((true == vocbase.dropView(view->id(), false).ok()));
    CHECK((true == !vocbase.lookupView("testView")));
  }

  // link removed before view
  {
    s.engine.views.clear();
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto* logicalCollection = vocbase.createCollection(collectionJson->slice());
    auto logicalView = vocbase.createView(viewJson->slice());
    REQUIRE((false == !logicalView));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
    REQUIRE((false == !view));

    // add a document to the view
    {
      static std::vector<std::string> const EMPTY;
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
      view->insert(trx, logicalCollection->id(), arangodb::LocalDocumentId(0), doc->slice(), meta);
      CHECK((trx.commit().ok()));
    }

    auto links = arangodb::velocypack::Parser::fromJson("{ \
      \"links\": { \"testCollection\": {} } \
    }");

    arangodb::Result res = logicalView->updateProperties(links->slice(), true, false);
    CHECK(true == res.ok());
    CHECK((false == logicalCollection->getIndexes().empty()));

    {
      std::unordered_set<TRI_voc_cid_t> cids;
      view->sync();
      arangodb::transaction::Methods trx(
        arangodb::transaction::StandaloneContext::Create(vocbase),
        EMPTY,
        EMPTY,
        EMPTY,
        arangodb::transaction::Options()
      );
      auto* snapshot = view->snapshot(trx, true);
      arangodb::iresearch::appendKnownCollections(cids, *snapshot);
      CHECK((1 == cids.size()));
    }

    {
      std::unordered_set<TRI_voc_cid_t> expected = { 100 };
      std::set<TRI_voc_cid_t> actual;
      view->visitCollections([&actual](TRI_voc_cid_t cid)->bool { actual.emplace(cid); return true; });

      for (auto& cid: expected) {
        CHECK((1 == actual.erase(cid)));
      }

      CHECK((actual.empty()));
    }

    CHECK((nullptr != vocbase.lookupCollection("testCollection")));
    persisted = false;
    CHECK((true == vocbase.dropCollection(logicalCollection->id(), true, -1).ok()));
    CHECK((true == persisted)); // collection removal persists view meta
    CHECK((nullptr == vocbase.lookupCollection("testCollection")));

    {
      std::unordered_set<TRI_voc_cid_t> cids;
      view->sync();
      arangodb::transaction::Methods trx(
        arangodb::transaction::StandaloneContext::Create(vocbase),
        EMPTY,
        EMPTY,
        EMPTY,
        arangodb::transaction::Options()
      );
      auto* snapshot = view->snapshot(trx, true);
      arangodb::iresearch::appendKnownCollections(cids, *snapshot);
      CHECK((0 == cids.size()));
    }

    {
      std::set<TRI_voc_cid_t> actual;
      view->visitCollections([&actual](TRI_voc_cid_t cid)->bool { actual.emplace(cid); return true; });
      CHECK((actual.empty())); // collection removal does modify view meta
    }

    CHECK((false == !vocbase.lookupView("testView")));
    CHECK((true == vocbase.dropView(view->id(), false).ok()));
    CHECK((true == !vocbase.lookupView("testView")));
  }

  // view removed before link
  {
    s.engine.views.clear();
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto* logicalCollection = vocbase.createCollection(collectionJson->slice());
    auto logicalView = vocbase.createView(viewJson->slice());
    REQUIRE((false == !logicalView));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
    REQUIRE((false == !view));

    auto links = arangodb::velocypack::Parser::fromJson("{ \
      \"links\": { \"testCollection\": {} } \
    }");

    arangodb::Result res = logicalView->updateProperties(links->slice(), true, false);
    CHECK(true == res.ok());
    CHECK((false == logicalCollection->getIndexes().empty()));

    std::set<TRI_voc_cid_t> cids;
    view->visitCollections([&cids](TRI_voc_cid_t cid)->bool { cids.emplace(cid); return true; });
    CHECK((1 == cids.size()));
    CHECK((false == !vocbase.lookupView("testView")));
    CHECK((true == vocbase.dropView(view->id(), false).ok()));
    CHECK((true == !vocbase.lookupView("testView")));
    CHECK((nullptr != vocbase.lookupCollection("testCollection")));
    CHECK((true == vocbase.dropCollection(logicalCollection->id(), true, -1).ok()));
    CHECK((nullptr == vocbase.lookupCollection("testCollection")));
  }

  // view deallocated before link removed
  {
    s.engine.views.clear();
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto* logicalCollection = vocbase.createCollection(collectionJson->slice());

    {
      auto createJson = arangodb::velocypack::Parser::fromJson("{}");
      auto updateJson = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection\": {} } }");
      auto logicalView = vocbase.createView(viewJson->slice());
      REQUIRE((false == !logicalView));
      auto* viewImpl = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
      REQUIRE((nullptr != viewImpl));
      CHECK((viewImpl->updateProperties(updateJson->slice(), true, false).ok()));
      CHECK((false == logicalCollection->getIndexes().empty()));
      std::set<TRI_voc_cid_t> cids;
      viewImpl->visitCollections([&cids](TRI_voc_cid_t cid)->bool { cids.emplace(cid); return true; });
      CHECK((1 == cids.size()));
      logicalCollection->getIndexes()[0]->unload(); // release view reference to prevent deadlock due to ~IResearchView() waiting for IResearchLink::unload()
      CHECK((true == vocbase.dropView(logicalView->id(), false).ok()));
      CHECK((1 == logicalView.use_count())); // ensure destructor for ViewImplementation is called
      CHECK((false == logicalCollection->getIndexes().empty()));
    }

    // create a new view with same ID to validate links
    {
      auto json = arangodb::velocypack::Parser::fromJson("{}");
      auto view = arangodb::iresearch::IResearchView::make(vocbase, viewJson->slice(), true, 0);
      REQUIRE((false == !view));
      auto* viewImpl = dynamic_cast<arangodb::iresearch::IResearchView*>(view.get());
      REQUIRE((nullptr != viewImpl));
      std::set<TRI_voc_cid_t> cids;
      viewImpl->visitCollections([&cids](TRI_voc_cid_t cid)->bool { cids.emplace(cid); return true; });
      CHECK((0 == cids.size()));

      for (auto& index: logicalCollection->getIndexes()) {
        auto* link = dynamic_cast<arangodb::iresearch::IResearchLink*>(index.get());
        REQUIRE((*link != *viewImpl)); // check that link is unregistred from view
      }
    }
  }
}

SECTION("test_self_token") {
  // test empty token
  {
    arangodb::iresearch::IResearchView::AsyncSelf empty(nullptr);
    CHECK((nullptr == empty.get()));
  }

  arangodb::iresearch::IResearchView::AsyncSelf::ptr self;

  {
    auto json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\" }");
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto view = arangodb::iresearch::IResearchView::make(vocbase, json->slice(), true, 0);
    CHECK((false == !view));
    auto* viewImpl = dynamic_cast<arangodb::iresearch::IResearchView*>(view.get());
    REQUIRE((nullptr != viewImpl));
    self = viewImpl->self();
    CHECK((false == !self));
    CHECK((viewImpl == self->get()));
  }

  CHECK((false == !self));
  CHECK((nullptr == self->get()));
}

SECTION("test_tracked_cids") {
  auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\", \"id\": 100 }");
  auto viewJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\", \"id\": 101, \"properties\": { } }");

  // test empty before open (TRI_vocbase_t::createView(...) will call open())
  {
    s.engine.views.clear();
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto view = arangodb::iresearch::IResearchView::make(vocbase, viewJson->slice(), true, 0);
    CHECK((nullptr != view));
    auto* viewImpl = dynamic_cast<arangodb::iresearch::IResearchView*>(view.get());
    REQUIRE((nullptr != viewImpl));

    std::set<TRI_voc_cid_t> actual;
    viewImpl->visitCollections([&actual](TRI_voc_cid_t cid)->bool { actual.emplace(cid); return true; });
    CHECK((actual.empty()));
  }

  // test add via link before open (TRI_vocbase_t::createView(...) will call open())
  {
    s.engine.views.clear();
    auto updateJson = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection\": { } } }");
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto* logicalCollection = vocbase.createCollection(collectionJson->slice());
    REQUIRE((nullptr != logicalCollection));
    auto logicalView = arangodb::iresearch::IResearchView::make(vocbase, viewJson->slice(), true, 0);
    REQUIRE((false == !logicalView));
    StorageEngineMock().registerView(vocbase, logicalView); // ensure link can find view
    auto* viewImpl = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
    REQUIRE((nullptr != viewImpl));

    CHECK((viewImpl->updateProperties(updateJson->slice(), false, false).ok()));

    std::set<TRI_voc_cid_t> actual;
    std::set<TRI_voc_cid_t> expected = { 100 };
    viewImpl->visitCollections([&actual](TRI_voc_cid_t cid)->bool { actual.emplace(cid); return true; });

    for (auto& cid: actual) {
      CHECK((1 == expected.erase(cid)));
    }

    CHECK((expected.empty()));
    logicalCollection->getIndexes()[0]->unload(); // release view reference to prevent deadlock due to ~IResearchView() waiting for IResearchLink::unload()
  }

  // test drop via link before open (TRI_vocbase_t::createView(...) will call open())
  {
    s.engine.views.clear();
    auto updateJson0 = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection\": { } } }");
    auto updateJson1 = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection\": null } }");
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto* logicalCollection = vocbase.createCollection(collectionJson->slice());
    REQUIRE((nullptr != logicalCollection));
    auto logicalView = arangodb::iresearch::IResearchView::make(vocbase, viewJson->slice(), true, 0);
    REQUIRE((false == !logicalView));
    StorageEngineMock().registerView(vocbase, logicalView); // ensure link can find view
    auto* viewImpl = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
    REQUIRE((nullptr != viewImpl));

    // create link
    {
      CHECK((viewImpl->updateProperties(updateJson0->slice(), false, false).ok()));

      std::set<TRI_voc_cid_t> actual;
      std::set<TRI_voc_cid_t> expected = { 100 };
      viewImpl->visitCollections([&actual](TRI_voc_cid_t cid)->bool { actual.emplace(cid); return true; });

      for (auto& cid: actual) {
        CHECK((1 == expected.erase(cid)));
      }

      CHECK((expected.empty()));
    }

    // drop link
    {
      CHECK((viewImpl->updateProperties(updateJson1->slice(), false, false).ok()));

      std::set<TRI_voc_cid_t> actual;
      viewImpl->visitCollections([&actual](TRI_voc_cid_t cid)->bool { actual.emplace(cid); return true; });
      CHECK((actual.empty()));
    }
  }

  // test load persisted CIDs on open (TRI_vocbase_t::createView(...) will call open())
  // use separate view ID for this test since doing open from persisted store
  {
    // initial populate persisted view
    {
      s.engine.views.clear();
      auto createJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\", \"id\": 102, \"properties\": { } }");
      auto* feature = arangodb::application_features::ApplicationServer::lookupFeature<
        arangodb::FlushFeature
      >("Flush");
      REQUIRE(feature);
      Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
      auto logicalView = vocbase.createView(createJson->slice());
      REQUIRE((false == !logicalView));
      auto* viewImpl = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
      REQUIRE((nullptr != viewImpl));

      static std::vector<std::string> const EMPTY;
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
      feature->executeCallbacks(); // commit to persisted store
    }

    // test persisted CIDs on open
    {
      s.engine.views.clear();
      auto createJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\", \"id\": 102, \"properties\": { } }");
      Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
      auto logicalView = vocbase.createView(createJson->slice());
      REQUIRE((false == !logicalView));
      auto* viewImpl = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
      REQUIRE((nullptr != viewImpl));

      std::set<TRI_voc_cid_t> actual;
      viewImpl->visitCollections([&actual](TRI_voc_cid_t cid)->bool { actual.emplace(cid); return true; });
      CHECK((actual.empty())); // persisted cids do not modify view meta
    }
  }

  // test add via link after open (TRI_vocbase_t::createView(...) will call open())
  {
    s.engine.views.clear();
    auto updateJson = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection\": { } } }");
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto* logicalCollection = vocbase.createCollection(collectionJson->slice());
    REQUIRE((nullptr != logicalCollection));
    auto logicalView = vocbase.createView(viewJson->slice());
    REQUIRE((false == !logicalView));
    auto* viewImpl = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
    REQUIRE((nullptr != viewImpl));

    CHECK((viewImpl->updateProperties(updateJson->slice(), false, false).ok()));

    std::set<TRI_voc_cid_t> actual;
    std::set<TRI_voc_cid_t> expected = { 100 };
    viewImpl->visitCollections([&actual](TRI_voc_cid_t cid)->bool { actual.emplace(cid); return true; });

    for (auto& cid: actual) {
      CHECK((1 == expected.erase(cid)));
    }

    CHECK((expected.empty()));
  }

  // test drop via link after open (TRI_vocbase_t::createView(...) will call open())
  {
    s.engine.views.clear();
    auto updateJson0 = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection\": { } } }");
    auto updateJson1 = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection\": null } }");
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto* logicalCollection = vocbase.createCollection(collectionJson->slice());
    REQUIRE((nullptr != logicalCollection));
    auto logicalView = vocbase.createView(viewJson->slice());
    REQUIRE((false == !logicalView));
    auto* viewImpl = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
    REQUIRE((nullptr != viewImpl));

    // create link
    {
      CHECK((viewImpl->updateProperties(updateJson0->slice(), false, false).ok()));

      std::set<TRI_voc_cid_t> actual;
      std::set<TRI_voc_cid_t> expected = { 100 };
      viewImpl->visitCollections([&actual](TRI_voc_cid_t cid)->bool { actual.emplace(cid); return true; });

      for (auto& cid: actual) {
        CHECK((1 == expected.erase(cid)));
      }

      CHECK((expected.empty()));
    }

    // drop link
    {
      CHECK((viewImpl->updateProperties(updateJson1->slice(), false, false).ok()));

      std::set<TRI_voc_cid_t> actual;
      viewImpl->visitCollections([&actual](TRI_voc_cid_t cid)->bool { actual.emplace(cid); return true; });
      CHECK((actual.empty()));
    }
  }
}

SECTION("test_transaction_registration") {
  auto collectionJson0 = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection0\" }");
  auto collectionJson1 = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection1\" }");
  auto viewJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
  Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
  auto* logicalCollection0 = vocbase.createCollection(collectionJson0->slice());
  REQUIRE((nullptr != logicalCollection0));
  auto* logicalCollection1 = vocbase.createCollection(collectionJson1->slice());
  REQUIRE((nullptr != logicalCollection1));
  auto logicalView = vocbase.createView(viewJson->slice());
  REQUIRE((false == !logicalView));
  auto* viewImpl = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
  REQUIRE((nullptr != viewImpl));

  // link collection to view
  {
    auto updateJson = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection0\": {}, \"testCollection1\": {} } }");
    CHECK((viewImpl->updateProperties(updateJson->slice(), false, false).ok()));
  }

  // read transaction (by id)
  {
    arangodb::SingleCollectionTransaction trx(
      arangodb::transaction::StandaloneContext::Create(vocbase),
      *logicalView,
      arangodb::AccessMode::Type::READ
    );
    CHECK((trx.begin().ok()));
    CHECK((2 == trx.state()->numCollections()));
    CHECK((nullptr != trx.state()->findCollection(logicalCollection0->id())));
    CHECK((nullptr != trx.state()->findCollection(logicalCollection1->id())));
    std::unordered_set<std::string> expectedNames = { "testCollection0", "testCollection1" };
    auto actualNames = trx.state()->collectionNames();

    for(auto& entry: actualNames) {
      CHECK((1 == expectedNames.erase(entry)));
    }

    CHECK((expectedNames.empty()));
    CHECK((trx.commit().ok()));
  }

  // read transaction (by name)
  {
    arangodb::SingleCollectionTransaction trx(
      arangodb::transaction::StandaloneContext::Create(vocbase),
      logicalView->name(),
      arangodb::AccessMode::Type::READ
    );
    CHECK((trx.begin().ok()));
    CHECK((2 == trx.state()->numCollections()));
    CHECK((nullptr != trx.state()->findCollection(logicalCollection0->id())));
    CHECK((nullptr != trx.state()->findCollection(logicalCollection1->id())));
    std::unordered_set<std::string> expectedNames = { "testCollection0", "testCollection1" };
    auto actualNames = trx.state()->collectionNames();

    for(auto& entry: actualNames) {
      CHECK((1 == expectedNames.erase(entry)));
    }

    CHECK((expectedNames.empty()));
    CHECK((trx.commit().ok()));
  }

  // write transaction (by id)
  {
    arangodb::SingleCollectionTransaction trx(
      arangodb::transaction::StandaloneContext::Create(vocbase),
      *logicalView,
      arangodb::AccessMode::Type::WRITE
    );
    CHECK((trx.begin().ok()));
    CHECK((2 == trx.state()->numCollections()));
    CHECK((nullptr != trx.state()->findCollection(logicalCollection0->id())));
    CHECK((nullptr != trx.state()->findCollection(logicalCollection1->id())));
    std::unordered_set<std::string> expectedNames = { "testCollection0", "testCollection1" };
    auto actualNames = trx.state()->collectionNames();

    for(auto& entry: actualNames) {
      CHECK((1 == expectedNames.erase(entry)));
    }

    CHECK((expectedNames.empty()));
    CHECK((trx.commit().ok()));
  }

  // write transaction (by name)
  {
    arangodb::SingleCollectionTransaction trx(
      arangodb::transaction::StandaloneContext::Create(vocbase),
      logicalView->name(),
      arangodb::AccessMode::Type::WRITE
    );
    CHECK((trx.begin().ok()));
    CHECK((2 == trx.state()->numCollections()));
    CHECK((nullptr != trx.state()->findCollection(logicalCollection0->id())));
    CHECK((nullptr != trx.state()->findCollection(logicalCollection1->id())));
    std::unordered_set<std::string> expectedNames = { "testCollection0", "testCollection1" };
    auto actualNames = trx.state()->collectionNames();

    for(auto& entry: actualNames) {
      CHECK((1 == expectedNames.erase(entry)));
    }

    CHECK((expectedNames.empty()));
    CHECK((trx.commit().ok()));
  }

  // exclusive transaction (by id)
  {
    arangodb::SingleCollectionTransaction trx(
      arangodb::transaction::StandaloneContext::Create(vocbase),
      *logicalView,
      arangodb::AccessMode::Type::READ
    );
    CHECK((trx.begin().ok()));
    CHECK((2 == trx.state()->numCollections()));
    CHECK((nullptr != trx.state()->findCollection(logicalCollection0->id())));
    CHECK((nullptr != trx.state()->findCollection(logicalCollection1->id())));
    std::unordered_set<std::string> expectedNames = { "testCollection0", "testCollection1" };
    auto actualNames = trx.state()->collectionNames();

    for(auto& entry: actualNames) {
      CHECK((1 == expectedNames.erase(entry)));
    }

    CHECK((expectedNames.empty()));
    CHECK((trx.commit().ok()));
  }

  // exclusive transaction (by name)
  {
    arangodb::SingleCollectionTransaction trx(
      arangodb::transaction::StandaloneContext::Create(vocbase),
      logicalView->name(),
      arangodb::AccessMode::Type::READ
    );
    CHECK((trx.begin().ok()));
    CHECK((2 == trx.state()->numCollections()));
    CHECK((nullptr != trx.state()->findCollection(logicalCollection0->id())));
    CHECK((nullptr != trx.state()->findCollection(logicalCollection1->id())));
    std::unordered_set<std::string> expectedNames = { "testCollection0", "testCollection1" };
    auto actualNames = trx.state()->collectionNames();

    for(auto& entry: actualNames) {
      CHECK((1 == expectedNames.erase(entry)));
    }

    CHECK((expectedNames.empty()));
    CHECK((trx.commit().ok()));
  }

  // drop collection from vocbase
  CHECK((true == vocbase.dropCollection(logicalCollection1->id(), true, 0).ok()));

  // read transaction (by id) (one collection dropped)
  {
    arangodb::SingleCollectionTransaction trx(
      arangodb::transaction::StandaloneContext::Create(vocbase),
      *logicalView,
      arangodb::AccessMode::Type::READ
    );
    CHECK((trx.begin().ok()));
    CHECK((1 == trx.state()->numCollections()));
    CHECK((nullptr != trx.state()->findCollection(logicalCollection0->id())));
    std::unordered_set<std::string> expectedNames = { "testCollection0" };
    auto actualNames = trx.state()->collectionNames();

    for(auto& entry: actualNames) {
      CHECK((1 == expectedNames.erase(entry)));
    }

    CHECK((expectedNames.empty()));
    CHECK((trx.commit().ok()));
  }

  // read transaction (by name) (one collection dropped)
  {
    arangodb::SingleCollectionTransaction trx(
      arangodb::transaction::StandaloneContext::Create(vocbase),
      logicalView->name(),
      arangodb::AccessMode::Type::READ
    );
    CHECK((trx.begin().ok()));
    CHECK((1 == trx.state()->numCollections()));
    CHECK((nullptr != trx.state()->findCollection(logicalCollection0->id())));
    std::unordered_set<std::string> expectedNames = { "testCollection0" };
    auto actualNames = trx.state()->collectionNames();

    for(auto& entry: actualNames) {
      CHECK((1 == expectedNames.erase(entry)));
    }

    CHECK((expectedNames.empty()));
    CHECK((trx.commit().ok()));
  }

  // write transaction (by id) (one collection dropped)
  {
    arangodb::SingleCollectionTransaction trx(
      arangodb::transaction::StandaloneContext::Create(vocbase),
      *logicalView,
      arangodb::AccessMode::Type::WRITE
    );
    CHECK((trx.begin().ok()));
    CHECK((1 == trx.state()->numCollections()));
    CHECK((nullptr != trx.state()->findCollection(logicalCollection0->id())));
    std::unordered_set<std::string> expectedNames = { "testCollection0" };
    auto actualNames = trx.state()->collectionNames();

    for(auto& entry: actualNames) {
      CHECK((1 == expectedNames.erase(entry)));
    }

    CHECK((expectedNames.empty()));
    CHECK((trx.commit().ok()));
  }

  // write transaction (by name) (one collection dropped)
  {
    arangodb::SingleCollectionTransaction trx(
      arangodb::transaction::StandaloneContext::Create(vocbase),
      logicalView->name(),
      arangodb::AccessMode::Type::WRITE
    );
    CHECK((trx.begin().ok()));
    CHECK((1 == trx.state()->numCollections()));
    CHECK((nullptr != trx.state()->findCollection(logicalCollection0->id())));
    std::unordered_set<std::string> expectedNames = { "testCollection0" };
    auto actualNames = trx.state()->collectionNames();

    for(auto& entry: actualNames) {
      CHECK((1 == expectedNames.erase(entry)));
    }

    CHECK((expectedNames.empty()));
    CHECK((trx.commit().ok()));
  }

  // exclusive transaction (by id) (one collection dropped)
  {
    arangodb::SingleCollectionTransaction trx(
      arangodb::transaction::StandaloneContext::Create(vocbase),
      *logicalView,
      arangodb::AccessMode::Type::READ
    );
    CHECK((trx.begin().ok()));
    CHECK((1 == trx.state()->numCollections()));
    CHECK((nullptr != trx.state()->findCollection(logicalCollection0->id())));
    std::unordered_set<std::string> expectedNames = { "testCollection0" };
    auto actualNames = trx.state()->collectionNames();

    for(auto& entry: actualNames) {
      CHECK((1 == expectedNames.erase(entry)));
    }

    CHECK((expectedNames.empty()));
    CHECK((trx.commit().ok()));
  }

  // exclusive transaction (by name) (one collection dropped)
  {
    arangodb::SingleCollectionTransaction trx(
      arangodb::transaction::StandaloneContext::Create(vocbase),
      logicalView->name(),
      arangodb::AccessMode::Type::READ
    );
    CHECK((trx.begin().ok()));
    CHECK((1 == trx.state()->numCollections()));
    CHECK((nullptr != trx.state()->findCollection(logicalCollection0->id())));
    std::unordered_set<std::string> expectedNames = { "testCollection0" };
    auto actualNames = trx.state()->collectionNames();

    for(auto& entry: actualNames) {
      CHECK((1 == expectedNames.erase(entry)));
    }

    CHECK((expectedNames.empty()));
    CHECK((trx.commit().ok()));
  }
}

SECTION("test_transaction_snapshot") {
  static std::vector<std::string> const EMPTY;
  auto viewJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\", \"commit\": { \"commitIntervalMsec\": 0 } }");
  Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
  auto logicalView = vocbase.createView(viewJson->slice());
  REQUIRE((false == !logicalView));
  auto* viewImpl = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
  REQUIRE((nullptr != viewImpl));

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
    arangodb::transaction::Methods trx(
      arangodb::transaction::StandaloneContext::Create(vocbase),
      EMPTY,
      EMPTY,
      EMPTY,
      arangodb::transaction::Options()
    );
    auto* snapshot = viewImpl->snapshot(trx);
    CHECK((nullptr == snapshot));
  }

  // no snapshot in TransactionState (force == true, waitForSync = false)
  {
    arangodb::transaction::Methods trx(
      arangodb::transaction::StandaloneContext::Create(vocbase),
      EMPTY,
      EMPTY,
      EMPTY,
      arangodb::transaction::Options()
    );
    auto* snapshot = viewImpl->snapshot(trx, true);
    CHECK((nullptr != snapshot));
    CHECK((0 == snapshot->live_docs_count()));
  }

  // no snapshot in TransactionState (force == false, waitForSync = true)
  {
    arangodb::transaction::Options opts;
    opts.waitForSync = true;
    arangodb::transaction::Methods trx(
      arangodb::transaction::StandaloneContext::Create(vocbase),
      EMPTY,
      EMPTY,
      EMPTY,
      opts
    );
    auto* snapshot = viewImpl->snapshot(trx);
    CHECK((nullptr == snapshot));
  }

  // no snapshot in TransactionState (force == true, waitForSync = true)
  {
    arangodb::transaction::Options opts;
    opts.waitForSync = true;
    arangodb::transaction::Methods trx(
      arangodb::transaction::StandaloneContext::Create(vocbase),
      EMPTY,
      EMPTY,
      EMPTY,
      opts
    );
    auto* snapshot = viewImpl->snapshot(trx, true);
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
    CHECK((true == viewImpl->apply(trx)));
    CHECK((true == trx.begin().ok()));
    auto* snapshot = viewImpl->snapshot(trx);
    CHECK((nullptr != snapshot));
    CHECK((1 == snapshot->live_docs_count()));
    CHECK(true == trx.abort().ok()); // prevent assertion in destructor
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
    CHECK((true == viewImpl->apply(trx)));
    CHECK((true == trx.begin().ok()));
    auto* snapshot = viewImpl->snapshot(trx, true);
    CHECK((nullptr != snapshot));
    CHECK((1 == snapshot->live_docs_count()));
    CHECK(true == trx.abort().ok()); // prevent assertion in destructor
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
    auto state = trx.state();
    REQUIRE(state);
    CHECK((true == viewImpl->apply(trx)));
    CHECK((true == trx.begin().ok()));
    state->waitForSync(true);
    auto* snapshot = viewImpl->snapshot(trx, true);
    CHECK((nullptr != snapshot));
    CHECK((1 == snapshot->live_docs_count()));
    CHECK(true == trx.abort().ok()); // prevent assertion in destructor
  }

  // old snapshot in TransactionState (force == true, waitForSync = true during updateStatus(), false during snapshot())
  {
    arangodb::transaction::Options opts;
    opts.waitForSync = true;
    arangodb::transaction::Methods trx(
      arangodb::transaction::StandaloneContext::Create(vocbase),
      EMPTY,
      EMPTY,
      EMPTY,
      opts
    );
    auto state = trx.state();
    REQUIRE(state);
    state->waitForSync(true);
    CHECK((true == viewImpl->apply(trx)));
    CHECK((true == trx.begin().ok()));
    state->waitForSync(false);
    auto* snapshot = viewImpl->snapshot(trx, true);
    CHECK((nullptr != snapshot));
    CHECK((2 == snapshot->live_docs_count()));
    CHECK(true == trx.abort().ok()); // prevent assertion in destructor
  }
}

SECTION("test_update_overwrite") {
  auto createJson = arangodb::velocypack::Parser::fromJson("{ \
    \"name\": \"testView\", \
    \"type\": \"arangosearch\" \
  }");

  // modify meta params
  {
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto view = vocbase.createView(createJson->slice());
    REQUIRE((false == !view));

    // initial update (overwrite)
    {
      arangodb::iresearch::IResearchViewMeta expectedMeta;
      arangodb::iresearch::IResearchViewMetaState expectedMetaState;
      auto updateJson = arangodb::velocypack::Parser::fromJson("{ \
        \"locale\": \"en\" \
      }");

      expectedMeta._locale = irs::locale_utils::locale("en", true);
      CHECK((view->updateProperties(updateJson->slice(), false, false).ok()));

      arangodb::velocypack::Builder builder;

      builder.openObject();
      view->toVelocyPack(builder, true, false);
      builder.close();

      auto slice = builder.slice();
      arangodb::iresearch::IResearchViewMeta meta;
      arangodb::iresearch::IResearchViewMetaState metaState;
      std::string error;

      CHECK(slice.isObject());
      CHECK(4 == slice.length());
      CHECK(slice.get("name").copyString() == "testView");
      CHECK(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
      CHECK(slice.get("deleted").isNone()); // no system properties
      slice = slice.get("properties");
      CHECK(slice.isObject());
      CHECK((4U == slice.length()));
      CHECK((meta.init(slice, error) && expectedMeta == meta));
      CHECK((true == metaState.init(slice, error) && expectedMetaState == metaState));

      auto tmpSlice = slice.get("links");
      CHECK((true == tmpSlice.isObject() && 0 == tmpSlice.length()));
    }

    // subsequent update (overwrite)
    {
      arangodb::iresearch::IResearchViewMeta expectedMeta;
      arangodb::iresearch::IResearchViewMetaState expectedMetaState;
      auto updateJson = arangodb::velocypack::Parser::fromJson("{ \
        \"locale\": \"ru\" \
      }");

      expectedMeta._locale = irs::locale_utils::locale("ru", true);
      CHECK((view->updateProperties(updateJson->slice(), false, false).ok()));

      arangodb::velocypack::Builder builder;

      builder.openObject();
      view->toVelocyPack(builder, true, false);
      builder.close();

      auto slice = builder.slice();
      arangodb::iresearch::IResearchViewMeta meta;
      arangodb::iresearch::IResearchViewMetaState metaState;
      std::string error;

      CHECK(slice.isObject());
      CHECK(slice.get("name").copyString() == "testView");
      CHECK(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
      CHECK(slice.get("deleted").isNone()); // no system properties
      slice = slice.get("properties");
      CHECK(slice.isObject());
      CHECK((4U == slice.length()));
      CHECK((meta.init(slice, error) && expectedMeta == meta));
      CHECK((true == metaState.init(slice, error) && expectedMetaState == metaState));

      auto tmpSlice = slice.get("links");
      CHECK((true == tmpSlice.isObject() && 0 == tmpSlice.length()));
    }
  }

  // overwrite links
  {
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto collectionJson0 = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection0\" }");
    auto collectionJson1 = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection1\" }");
    auto* logicalCollection0 = vocbase.createCollection(collectionJson0->slice());
    REQUIRE((nullptr != logicalCollection0));
    auto* logicalCollection1 = vocbase.createCollection(collectionJson1->slice());
    REQUIRE((nullptr != logicalCollection1));
    auto view = vocbase.createView(createJson->slice());
    REQUIRE((false == !view));
    REQUIRE(view->category() == arangodb::LogicalView::category());
    CHECK((true == logicalCollection0->getIndexes().empty()));
    CHECK((true == logicalCollection1->getIndexes().empty()));

    // initial creation
    {
      auto updateJson = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection0\": {} } }");
      arangodb::iresearch::IResearchViewMeta expectedMeta;
      arangodb::iresearch::IResearchViewMetaState expectedMetaState;
      std::unordered_map<std::string, arangodb::iresearch::IResearchLinkMeta> expectedLinkMeta;

      expectedMetaState._collections.insert(logicalCollection0->id());
      expectedLinkMeta["testCollection0"]; // use defaults
      CHECK((view->updateProperties(updateJson->slice(), true, false).ok()));

      arangodb::velocypack::Builder builder;

      builder.openObject();
      view->toVelocyPack(builder, true, false);
      builder.close();

      auto slice = builder.slice();
      arangodb::iresearch::IResearchViewMeta meta;
      arangodb::iresearch::IResearchViewMetaState metaState;
      std::string error;

      CHECK(slice.isObject());
      CHECK(slice.get("name").copyString() == "testView");
      CHECK(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
      CHECK(slice.get("deleted").isNone()); // no system properties
      slice = slice.get("properties");
      CHECK(slice.isObject());
      CHECK((4U == slice.length()));
      CHECK((meta.init(slice, error) && expectedMeta == meta));
      CHECK((true == metaState.init(slice, error) && expectedMetaState == metaState));

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
      arangodb::iresearch::IResearchViewMetaState expectedMetaState;
      std::unordered_map<std::string, arangodb::iresearch::IResearchLinkMeta> expectedLinkMeta;

      expectedMetaState._collections.insert(logicalCollection1->id());
      expectedLinkMeta["testCollection1"]; // use defaults
      CHECK((view->updateProperties(updateJson->slice(), false, false).ok()));

      arangodb::velocypack::Builder builder;

      builder.openObject();
      view->toVelocyPack(builder, true, false);
      builder.close();

      auto slice = builder.slice();
      arangodb::iresearch::IResearchViewMeta meta;
      arangodb::iresearch::IResearchViewMetaState metaState;
      std::string error;

      CHECK(slice.isObject());
      CHECK(slice.get("name").copyString() == "testView");
      CHECK(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
      CHECK(slice.get("deleted").isNone()); // no system properties
      slice = slice.get("properties");
      CHECK(slice.isObject());
      CHECK((4U == slice.length()));
      CHECK((meta.init(slice, error) && expectedMeta == meta));
      CHECK((true == metaState.init(slice, error) && expectedMetaState == metaState));

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

  // update existing link (full update)
  {
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
    auto* logicalCollection = vocbase.createCollection(collectionJson->slice());
    REQUIRE((nullptr != logicalCollection));
    auto view = vocbase.createView(createJson->slice());
    REQUIRE((false == !view));
    REQUIRE(view->category() == arangodb::LogicalView::category());

    // initial add of link
    {
      auto updateJson = arangodb::velocypack::Parser::fromJson(
        "{ \"links\": { \"testCollection\": { \"includeAllFields\": true } } }"
      );
      CHECK((view->updateProperties(updateJson->slice(), true, false).ok()));

      arangodb::velocypack::Builder builder;

      builder.openObject();
      view->toVelocyPack(builder, true, false);
      builder.close();

      auto slice = builder.slice();
      CHECK(slice.isObject());
      CHECK(slice.get("name").copyString() == "testView");
      CHECK(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
      CHECK(slice.get("deleted").isNone()); // no system properties
      slice = slice.get("properties");
      auto tmpSlice = slice.get("collections");
      CHECK((true == tmpSlice.isArray() && 1 == tmpSlice.length()));
      tmpSlice = slice.get("links");
      CHECK((true == tmpSlice.isObject() && 1 == tmpSlice.length()));
      tmpSlice = tmpSlice.get("testCollection");
      CHECK((true == tmpSlice.isObject()));
      tmpSlice = tmpSlice.get("includeAllFields");
      CHECK((true == tmpSlice.isBoolean() && true == tmpSlice.getBoolean()));
    }

    // update link
    {
      auto updateJson = arangodb::velocypack::Parser::fromJson(
        "{ \"links\": { \"testCollection\": { } } }"
      );
      CHECK((view->updateProperties(updateJson->slice(), false, false).ok()));

      arangodb::velocypack::Builder builder;

      builder.openObject();
      view->toVelocyPack(builder, true, false);
      builder.close();

      auto slice = builder.slice();
      CHECK(slice.get("name").copyString() == "testView");
      CHECK(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
      CHECK(slice.get("deleted").isNone()); // no system properties
      slice = slice.get("properties");
      auto tmpSlice = slice.get("links");
      CHECK((true == tmpSlice.isObject() && 1 == tmpSlice.length()));
      tmpSlice = tmpSlice.get("testCollection");
      CHECK((true == tmpSlice.isObject()));
      tmpSlice = tmpSlice.get("includeAllFields");
      CHECK((true == tmpSlice.isBoolean() && false == tmpSlice.getBoolean()));
    }
  }
}

SECTION("test_update_partial") {
  auto createJson = arangodb::velocypack::Parser::fromJson("{ \
    \"name\": \"testView\", \
    \"type\": \"arangosearch\" \
  }");
  bool persisted = false;
  auto before = StorageEngineMock::before;
  auto restore = irs::make_finally([&before]()->void { StorageEngineMock::before = before; });
  StorageEngineMock::before = [&persisted]()->void { persisted = true; };

  // modify meta params
  {
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto view = vocbase.createView(createJson->slice());
    REQUIRE((false == !view));
    REQUIRE(view->category() == arangodb::LogicalView::category());

    arangodb::iresearch::IResearchViewMeta expectedMeta;
    arangodb::iresearch::IResearchViewMetaState expectedMetaState;
    auto updateJson = arangodb::velocypack::Parser::fromJson("{ \
      \"locale\": \"en\" \
    }");

    expectedMeta._locale = irs::locale_utils::locale("en", true);
    CHECK((view->updateProperties(updateJson->slice(), true, false).ok()));

    arangodb::velocypack::Builder builder;

    builder.openObject();
    view->toVelocyPack(builder, true, false);
    builder.close();

    auto slice = builder.slice();
    arangodb::iresearch::IResearchViewMeta meta;
    arangodb::iresearch::IResearchViewMetaState metaState;
    std::string error;

    CHECK(slice.isObject());
    CHECK(slice.get("name").copyString() == "testView");
    CHECK(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
    CHECK(slice.get("deleted").isNone()); // no system properties
    slice = slice.get("properties");
    CHECK(slice.isObject());
    CHECK((4U == slice.length()));
    CHECK((meta.init(slice, error) && expectedMeta == meta));
    CHECK((true == metaState.init(slice, error) && expectedMetaState == metaState));

    auto tmpSlice = slice.get("links");
    CHECK((true == tmpSlice.isObject() && 0 == tmpSlice.length()));
  }

  // test rollback on meta modification failure (as an example invalid value for 'locale')
  {
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto view = vocbase.createView(createJson->slice());
    REQUIRE((false == !view));
    REQUIRE(view->category() == arangodb::LogicalView::category());

    arangodb::iresearch::IResearchViewMeta expectedMeta;
    arangodb::iresearch::IResearchViewMetaState expectedMetaState;
    auto updateJson = arangodb::velocypack::Parser::fromJson(std::string() + "{ \
      \"locale\": 123 \
    }");

    CHECK((TRI_ERROR_BAD_PARAMETER == view->updateProperties(updateJson->slice(), true, false).errorNumber()));

    arangodb::velocypack::Builder builder;

    builder.openObject();
    view->toVelocyPack(builder, true, false);
    builder.close();

    auto slice = builder.slice();
    arangodb::iresearch::IResearchViewMeta meta;
    arangodb::iresearch::IResearchViewMetaState metaState;
    std::string error;

    CHECK(slice.isObject());
    CHECK(slice.get("name").copyString() == "testView");
    CHECK(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
    CHECK(slice.get("deleted").isNone()); // no system properties
    slice = slice.get("properties");
    CHECK(slice.isObject());
    CHECK((4U == slice.length()));
    CHECK((meta.init(slice, error) && expectedMeta == meta));
    CHECK((true == metaState.init(slice, error) && expectedMetaState == metaState));

    auto tmpSlice = slice.get("links");
    CHECK((true == tmpSlice.isObject() && 0 == tmpSlice.length()));
  }

  // add a new link (in recovery)
  {
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
    auto* logicalCollection = vocbase.createCollection(collectionJson->slice());
    REQUIRE((nullptr != logicalCollection));
    auto view = vocbase.createView(createJson->slice());
    REQUIRE((false == !view));
    REQUIRE(view->category() == arangodb::LogicalView::category());

    auto updateJson = arangodb::velocypack::Parser::fromJson(
      "{ \"links\": { \"testCollection\": {} } }"
    );

    auto before = StorageEngineMock::inRecoveryResult;
    StorageEngineMock::inRecoveryResult = true;
    auto restore = irs::make_finally([&before]()->void { StorageEngineMock::inRecoveryResult = before; });
    persisted = false;
    CHECK((view->updateProperties(updateJson->slice(), true, false).ok()));
    CHECK((false == persisted));

    arangodb::velocypack::Builder builder;

    builder.openObject();
    view->toVelocyPack(builder, true, false);
    builder.close();

    auto slice = builder.slice();
    CHECK(slice.isObject());
    CHECK(slice.get("name").copyString() == "testView");
    CHECK(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
    CHECK(slice.get("deleted").isNone()); // no system properties
    slice = slice.get("properties");
    CHECK(slice.isObject());
    CHECK((
      true == slice.hasKey("links")
      && slice.get("links").isObject()
      && 1 == slice.get("links").length()
    ));
  }

  // add a new link
  {
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
    auto* logicalCollection = vocbase.createCollection(collectionJson->slice());
    REQUIRE((nullptr != logicalCollection));
    auto view = vocbase.createView(createJson->slice());
    REQUIRE((false == !view));

    arangodb::iresearch::IResearchViewMeta expectedMeta;
    arangodb::iresearch::IResearchViewMetaState expectedMetaState;
    std::unordered_map<std::string, arangodb::iresearch::IResearchLinkMeta> expectedLinkMeta;
    auto updateJson = arangodb::velocypack::Parser::fromJson("{ \
      \"links\": { \
        \"testCollection\": {} \
      }}");

    expectedMetaState._collections.insert(logicalCollection->id());
    expectedLinkMeta["testCollection"]; // use defaults
    persisted = false;
    CHECK((view->updateProperties(updateJson->slice(), true, false).ok()));
    CHECK((true == persisted)); // link addition does modify and persist view meta

    arangodb::velocypack::Builder builder;

    builder.openObject();
    view->toVelocyPack(builder, true, false);
    builder.close();

    auto slice = builder.slice();
    arangodb::iresearch::IResearchViewMeta meta;
    arangodb::iresearch::IResearchViewMetaState metaState;
    std::string error;

    CHECK(slice.isObject());
    CHECK(slice.get("name").copyString() == "testView");
    CHECK(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
    CHECK(slice.get("deleted").isNone()); // no system properties
    slice = slice.get("properties");
    CHECK(slice.isObject());
    CHECK((4U == slice.length()));
    CHECK((meta.init(slice, error) && expectedMeta == meta));
    CHECK((true == metaState.init(slice, error) && expectedMetaState == metaState));

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
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
    auto* logicalCollection = vocbase.createCollection(collectionJson->slice());
    REQUIRE((nullptr != logicalCollection));
    auto view = vocbase.createView(createJson->slice());
    REQUIRE((false == !view));
    REQUIRE(view->category() == arangodb::LogicalView::category());

    {
      static std::vector<std::string> const EMPTY;
      auto doc = arangodb::velocypack::Parser::fromJson("{ \"abc\": \"def\" }");
      arangodb::transaction::Methods trx(
        arangodb::transaction::StandaloneContext::Create(vocbase),
        EMPTY,
        EMPTY,
        EMPTY,
        arangodb::transaction::Options()
      );

      CHECK((trx.begin().ok()));
      CHECK((trx.insert(logicalCollection->name(), doc->slice(), arangodb::OperationOptions()).ok()));
      CHECK((trx.commit().ok()));
    }

    arangodb::iresearch::IResearchViewMeta expectedMeta;
    arangodb::iresearch::IResearchViewMetaState expectedMetaState;
    std::unordered_map<std::string, arangodb::iresearch::IResearchLinkMeta> expectedLinkMeta;
    auto updateJson = arangodb::velocypack::Parser::fromJson("{ \
      \"links\": { \
        \"testCollection\": {} \
      }}");

    expectedMetaState._collections.insert(logicalCollection->id());
    expectedLinkMeta["testCollection"]; // use defaults
    persisted = false;
    CHECK((view->updateProperties(updateJson->slice(), true, false).ok()));
    CHECK((true == persisted)); // link addition does modify and persist view meta

    arangodb::velocypack::Builder builder;

    builder.openObject();
    view->toVelocyPack(builder, true, false);
    builder.close();

    auto slice = builder.slice();
    arangodb::iresearch::IResearchViewMeta meta;
    arangodb::iresearch::IResearchViewMetaState metaState;
    std::string error;

    CHECK(slice.isObject());
    CHECK(slice.get("name").copyString() == "testView");
    CHECK(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
    CHECK(slice.get("deleted").isNone()); // no system properties
    slice = slice.get("properties");
    CHECK((4U == slice.length()));
    CHECK((meta.init(slice, error) && expectedMeta == meta));
    CHECK((true == metaState.init(slice, error) && expectedMetaState == metaState));

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
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto view = vocbase.createView(createJson->slice());
    REQUIRE((false == !view));
    REQUIRE(view->category() == arangodb::LogicalView::category());

    arangodb::iresearch::IResearchViewMeta expectedMeta;
    arangodb::iresearch::IResearchViewMetaState expectedMetaState;
    auto updateJson = arangodb::velocypack::Parser::fromJson("{ \
      \"links\": { \
        \"testCollection\": {} \
      }}");

    CHECK((TRI_ERROR_BAD_PARAMETER == view->updateProperties(updateJson->slice(), true, false).errorNumber()));

    arangodb::velocypack::Builder builder;

    builder.openObject();
    view->toVelocyPack(builder, true, false);
    builder.close();

    auto slice = builder.slice();
    arangodb::iresearch::IResearchViewMeta meta;
    arangodb::iresearch::IResearchViewMetaState metaState;
    std::string error;

    CHECK(slice.isObject());
    CHECK(slice.get("name").copyString() == "testView");
    CHECK(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
    CHECK(slice.get("deleted").isNone()); // no system properties
    slice = slice.get("properties");
    CHECK((4U == slice.length()));
    CHECK((meta.init(slice, error) && expectedMeta == meta));
    CHECK((metaState.init(slice, error) && expectedMetaState == metaState));

    auto tmpSlice = slice.get("links");
    CHECK((true == tmpSlice.isObject() && 0 == tmpSlice.length()));
  }

  // remove link (in recovery)
  {
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
    auto* logicalCollection = vocbase.createCollection(collectionJson->slice());
    REQUIRE((nullptr != logicalCollection));
    auto view = vocbase.createView(createJson->slice());
    REQUIRE((false == !view));
    REQUIRE(view->category() == arangodb::LogicalView::category());

    {
      auto updateJson = arangodb::velocypack::Parser::fromJson(
        "{ \"links\": { \"testCollection\": {} } }"
      );
      persisted = false;
      auto beforeRecovery = StorageEngineMock::inRecoveryResult;
      StorageEngineMock::inRecoveryResult = true;
      auto restoreRecovery = irs::make_finally([&beforeRecovery]()->void { StorageEngineMock::inRecoveryResult = beforeRecovery; });
      CHECK((view->updateProperties(updateJson->slice(), true, false).ok()));
      CHECK((false == persisted)); // link addition does not persist view meta

      arangodb::velocypack::Builder builder;

      builder.openObject();
      view->toVelocyPack(builder, true, false);
      builder.close();

      auto slice = builder.slice();
      CHECK(slice.isObject());
      CHECK(slice.get("name").copyString() == "testView");
      CHECK(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
      CHECK(slice.get("deleted").isNone()); // no system properties
      slice = slice.get("properties");
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

      auto before = StorageEngineMock::inRecoveryResult;
      StorageEngineMock::inRecoveryResult = true;
      auto restore = irs::make_finally([&before]()->void { StorageEngineMock::inRecoveryResult = before; });
      persisted = false;
      CHECK((view->updateProperties(updateJson->slice(), true, false).ok()));
      CHECK((false == persisted));

      arangodb::velocypack::Builder builder;

      builder.openObject();
      view->toVelocyPack(builder, true, false);
      builder.close();

      auto slice = builder.slice();
      CHECK(slice.isObject());
      CHECK(slice.get("name").copyString() == "testView");
      CHECK(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
      CHECK(slice.get("deleted").isNone()); // no system properties
      slice = slice.get("properties");
      CHECK((
        true == slice.hasKey("links")
        && slice.get("links").isObject()
        && 0 == slice.get("links").length()
      ));
    }
  }

  // remove link
  {
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
    auto* logicalCollection = vocbase.createCollection(collectionJson->slice());
    REQUIRE((nullptr != logicalCollection));
    auto view = vocbase.createView(createJson->slice());
    REQUIRE((false == !view));

    arangodb::iresearch::IResearchViewMeta expectedMeta;
    arangodb::iresearch::IResearchViewMetaState expectedMetaState;

    expectedMetaState._collections.insert(logicalCollection->id());

    {
      auto updateJson = arangodb::velocypack::Parser::fromJson("{ \
        \"links\": { \
          \"testCollection\": {} \
      }}");

      CHECK((view->updateProperties(updateJson->slice(), true, false).ok()));

      arangodb::velocypack::Builder builder;

      builder.openObject();
      view->toVelocyPack(builder, true, false);
      builder.close();

      auto slice = builder.slice();
      arangodb::iresearch::IResearchViewMeta meta;
      arangodb::iresearch::IResearchViewMetaState metaState;
      std::string error;

      CHECK(slice.isObject());
      CHECK(slice.get("name").copyString() == "testView");
      CHECK(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
      CHECK(slice.get("deleted").isNone()); // no system properties
      slice = slice.get("properties");
      CHECK(slice.isObject());
      CHECK((4U == slice.length()));
      CHECK((meta.init(slice, error) && expectedMeta == meta));
      CHECK((true == metaState.init(slice, error) && expectedMetaState == metaState));

      auto tmpSlice = slice.get("links");
      CHECK((true == tmpSlice.isObject() && 1 == tmpSlice.length()));
    }

    {
      auto updateJson = arangodb::velocypack::Parser::fromJson("{ \
        \"links\": { \
          \"testCollection\": null \
      }}");

      expectedMetaState._collections.clear();
      CHECK((view->updateProperties(updateJson->slice(), true, false).ok()));

      arangodb::velocypack::Builder builder;

      builder.openObject();
      view->toVelocyPack(builder, true, false);
      builder.close();

      auto slice = builder.slice();
      arangodb::iresearch::IResearchViewMeta meta;
      arangodb::iresearch::IResearchViewMetaState metaState;
      std::string error;

      CHECK(slice.isObject());
      CHECK(slice.get("name").copyString() == "testView");
      CHECK(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
      CHECK(slice.get("deleted").isNone()); // no system properties
      slice = slice.get("properties");
      CHECK(slice.isObject());
      CHECK((4U == slice.length()));
      CHECK((meta.init(slice, error) && expectedMeta == meta));
      CHECK((true == metaState.init(slice, error) && expectedMetaState == metaState));

      auto tmpSlice = slice.get("links");
      CHECK((true == tmpSlice.isObject() && 0 == tmpSlice.length()));
    }
  }

  // remove link from non-existant collection
  {
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto view = vocbase.createView(createJson->slice());
    REQUIRE((false == !view));

    arangodb::iresearch::IResearchViewMeta expectedMeta;
    arangodb::iresearch::IResearchViewMetaState expectedMetaState;
    auto updateJson = arangodb::velocypack::Parser::fromJson("{ \
      \"links\": { \
        \"testCollection\": null \
      }}");

    CHECK((TRI_ERROR_BAD_PARAMETER == view->updateProperties(updateJson->slice(), true, false).errorNumber()));

    arangodb::velocypack::Builder builder;

    builder.openObject();
    view->toVelocyPack(builder, true, false);
    builder.close();

    auto slice = builder.slice();
    arangodb::iresearch::IResearchViewMeta meta;
    arangodb::iresearch::IResearchViewMetaState metaState;
    std::string error;

    CHECK(slice.isObject());
    CHECK(slice.get("name").copyString() == "testView");
    CHECK(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
    CHECK(slice.get("deleted").isNone()); // no system properties
    slice = slice.get("properties");
    CHECK(slice.isObject());
    CHECK((4U == slice.length()));
    CHECK((meta.init(slice, error) && expectedMeta == meta));
    CHECK((true == metaState.init(slice, error) && expectedMetaState == metaState));

    auto tmpSlice = slice.get("links");
    CHECK((true == tmpSlice.isObject() && 0 == tmpSlice.length()));
  }

  // remove non-existant link
  {
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
    auto* logicalCollection = vocbase.createCollection(collectionJson->slice());
    REQUIRE((nullptr != logicalCollection));
    auto view = vocbase.createView(createJson->slice());
    REQUIRE((false == !view));

    arangodb::iresearch::IResearchViewMeta expectedMeta;
    arangodb::iresearch::IResearchViewMetaState expectedMetaState;
    auto updateJson = arangodb::velocypack::Parser::fromJson("{ \
      \"links\": { \
        \"testCollection\": null \
    }}");

    CHECK((view->updateProperties(updateJson->slice(), true, false).ok()));

    arangodb::velocypack::Builder builder;

    builder.openObject();
    view->toVelocyPack(builder, true, false);
    builder.close();

    auto slice = builder.slice();
    arangodb::iresearch::IResearchViewMeta meta;
    arangodb::iresearch::IResearchViewMetaState metaState;
    std::string error;

    CHECK(slice.isObject());
    CHECK(slice.get("name").copyString() == "testView");
    CHECK(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
    CHECK(slice.get("deleted").isNone()); // no system properties
    slice = slice.get("properties");
    CHECK(slice.isObject());
    CHECK((4U == slice.length()));
    CHECK((meta.init(slice, error) && expectedMeta == meta));
    CHECK((true == metaState.init(slice, error) && expectedMetaState == metaState));

    auto tmpSlice = slice.get("links");
    CHECK((true == tmpSlice.isObject() && 0 == tmpSlice.length()));
  }

  // remove + add link to same collection (reindex)
  {
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
    auto* logicalCollection = vocbase.createCollection(collectionJson->slice());
    REQUIRE((nullptr != logicalCollection));
    auto view = vocbase.createView(createJson->slice());
    REQUIRE((false == !view));

    // initial add of link
    {
      auto updateJson = arangodb::velocypack::Parser::fromJson(
        "{ \"links\": { \"testCollection\": {} } }"
      );
      CHECK((view->updateProperties(updateJson->slice(), true, false).ok()));

      arangodb::velocypack::Builder builder;

      builder.openObject();
      view->toVelocyPack(builder, true, false);
      builder.close();

      auto slice = builder.slice();
      CHECK(slice.isObject());
      CHECK(slice.get("name").copyString() == "testView");
      CHECK(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
      CHECK(slice.get("deleted").isNone()); // no system properties
      slice = slice.get("properties");
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
      view->toVelocyPack(builder, true, false);
      builder.close();

      auto slice = builder.slice();
      CHECK(slice.isObject());
      CHECK(slice.get("name").copyString() == "testView");
      CHECK(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
      CHECK(slice.get("deleted").isNone()); // no system properties
      slice = slice.get("properties");
      auto tmpSlice = slice.get("links");
      CHECK((true == tmpSlice.isObject() && 1 == tmpSlice.length()));

      std::unordered_set<TRI_idx_iid_t> actual;

      for (auto& index: logicalCollection->getIndexes()) {
        actual.emplace(index->id());
      }

      CHECK((initial != actual)); // a reindexing took place (link recreated)
    }
  }

  // update existing link (partial update)
  {
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
    auto* logicalCollection = vocbase.createCollection(collectionJson->slice());
    REQUIRE((nullptr != logicalCollection));
    auto view = vocbase.createView(createJson->slice());
    REQUIRE((false == !view));

    // initial add of link
    {
      auto updateJson = arangodb::velocypack::Parser::fromJson(
        "{ \"links\": { \"testCollection\": { \"includeAllFields\": true } } }"
      );
      CHECK((view->updateProperties(updateJson->slice(), true, false).ok()));

      arangodb::velocypack::Builder builder;

      builder.openObject();
      view->toVelocyPack(builder, true, false);
      builder.close();

      auto slice = builder.slice();
      CHECK(slice.isObject());
      CHECK(slice.get("name").copyString() == "testView");
      CHECK(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
      CHECK(slice.get("deleted").isNone()); // no system properties
      slice = slice.get("properties");
      auto tmpSlice = slice.get("collections");
      CHECK((true == tmpSlice.isArray() && 1 == tmpSlice.length()));
      tmpSlice = slice.get("links");
      CHECK((true == tmpSlice.isObject() && 1 == tmpSlice.length()));
      tmpSlice = tmpSlice.get("testCollection");
      CHECK((true == tmpSlice.isObject()));
      tmpSlice = tmpSlice.get("includeAllFields");
      CHECK((true == tmpSlice.isBoolean() && true == tmpSlice.getBoolean()));
    }

    // update link
    {
      auto updateJson = arangodb::velocypack::Parser::fromJson(
        "{ \"links\": { \"testCollection\": { } } }"
      );
      CHECK((view->updateProperties(updateJson->slice(), true, false).ok()));

      arangodb::velocypack::Builder builder;

      builder.openObject();
      view->toVelocyPack(builder, true, false);
      builder.close();

      auto slice = builder.slice();
      CHECK(slice.isObject());
      CHECK(slice.get("name").copyString() == "testView");
      CHECK(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
      CHECK(slice.get("deleted").isNone()); // no system properties
      slice = slice.get("properties");
      auto tmpSlice = slice.get("links");
      CHECK((true == tmpSlice.isObject() && 1 == tmpSlice.length()));
      tmpSlice = tmpSlice.get("testCollection");
      CHECK((true == tmpSlice.isObject()));
      tmpSlice = tmpSlice.get("includeAllFields");
      CHECK((true == tmpSlice.isBoolean() && false == tmpSlice.getBoolean()));
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
