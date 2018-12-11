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
#include "Aql/QueryRegistry.h"
#include "Aql/AstNode.h"
#include "Aql/Function.h"
#include "Aql/SortCondition.h"
#include "Basics/ArangoGlobalContext.h"
#include "Basics/error.h"
#include "Basics/files.h"
#include "Basics/LocalTaskQueue.h"
#include "Utils/ExecContext.h"

#if USE_ENTERPRISE
  #include "Enterprise/Ldap/LdapFeature.h"
#endif

#include "ApplicationFeatures/BasicPhase.h"
#include "ApplicationFeatures/CommunicationPhase.h"
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
#include "IResearch/IResearchLinkHelper.h"
#include "IResearch/IResearchLinkMeta.h"
#include "IResearch/IResearchMMFilesLink.h"
#include "IResearch/IResearchView.h"
#include "Logger/Logger.h"
#include "Logger/LogTopic.h"
#include "Random/RandomFeature.h"
#include "RestServer/AqlFeature.h"
#include "RestServer/TraverserEngineRegistryFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/FlushFeature.h"
#include "RestServer/DatabasePathFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "RestServer/SystemDatabaseFeature.h"
#include "RestServer/ViewTypesFeature.h"
#include "Sharding/ShardingFeature.h"
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
    virtual void add(irs::byte_type* dst, const irs::byte_type* src) const override { score_cast(dst) = score_cast(src); }
    virtual irs::flags const& features() const override { return irs::flags::empty_instance(); }
    virtual bool less(const irs::byte_type* lhs, const irs::byte_type* rhs) const override { return score_cast(lhs) < score_cast(rhs); }
    virtual irs::sort::collector::ptr prepare_collector() const override { return nullptr; }
    virtual void prepare_score(irs::byte_type* score) const override { }
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

  IResearchViewSetup(): engine(server), server(nullptr, nullptr) {
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
    arangodb::application_features::ApplicationFeature* tmpFeature;

    buildFeatureEntry(new arangodb::application_features::BasicFeaturePhase(server, false), false);
    buildFeatureEntry(new arangodb::application_features::CommunicationFeaturePhase(server), false);
    buildFeatureEntry(new arangodb::application_features::ClusterFeaturePhase(server), false);
    buildFeatureEntry(new arangodb::application_features::DatabaseFeaturePhase(server), false);
    buildFeatureEntry(new arangodb::application_features::GreetingsFeaturePhase(server, false), false);
    buildFeatureEntry(new arangodb::application_features::V8FeaturePhase(server), false);
    // setup required application features
    buildFeatureEntry(new arangodb::V8DealerFeature(server), false);
    buildFeatureEntry(new arangodb::ViewTypesFeature(server), true);
    buildFeatureEntry(tmpFeature = new arangodb::QueryRegistryFeature(server), false);
    arangodb::application_features::ApplicationServer::server->addFeature(tmpFeature); // need QueryRegistryFeature feature to be added now in order to create the system database
    system = irs::memory::make_unique<TRI_vocbase_t>(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 0, TRI_VOC_SYSTEM_DATABASE);
    buildFeatureEntry(new arangodb::SystemDatabaseFeature(server, system.get()), false); // required for IResearchAnalyzerFeature
    buildFeatureEntry(new arangodb::RandomFeature(server), false); // required by AuthenticationFeature
    buildFeatureEntry(new arangodb::AuthenticationFeature(server), true);
    buildFeatureEntry(new arangodb::ClusterFeature(server), false);
    buildFeatureEntry(new arangodb::DatabaseFeature(server), false);
    buildFeatureEntry(new arangodb::DatabasePathFeature(server), false);
    buildFeatureEntry(new arangodb::TraverserEngineRegistryFeature(server), false);
    buildFeatureEntry(new arangodb::AqlFeature(server), true);
    buildFeatureEntry(new arangodb::aql::AqlFunctionFeature(server), true); // required for IResearchAnalyzerFeature
    buildFeatureEntry(new arangodb::iresearch::IResearchAnalyzerFeature(server), true);
    buildFeatureEntry(new arangodb::iresearch::IResearchFeature(server), true);
    buildFeatureEntry(new arangodb::ShardingFeature(server), false); 
    buildFeatureEntry(new arangodb::FlushFeature(server), false); // do not start the thread

    #if USE_ENTERPRISE
    buildFeatureEntry(new arangodb::LdapFeature(server), false); // required for AuthenticationFeature with USE_ENTERPRISE
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

    auto* dbFeature = arangodb::application_features::ApplicationServer::getFeature<arangodb::DatabaseFeature>("Database");
    arangodb::DatabaseFeature::DATABASE = dbFeature; // arangodb::auth::UserManager::collectionAuthLevel(...) dereferences a nullptr

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

  ~IResearchViewSetup() {
    system.reset(); // destroy before reseting the 'ENGINE'
    TRI_RemoveDirectory(testFilesystemPath.c_str());
    arangodb::LogTopic::setLogLevel(arangodb::iresearch::TOPIC.name(), arangodb::LogLevel::DEFAULT);
    arangodb::LogTopic::setLogLevel(arangodb::Logger::FIXME.name(), arangodb::LogLevel::DEFAULT);
    arangodb::LogTopic::setLogLevel(arangodb::Logger::CLUSTER.name(), arangodb::LogLevel::DEFAULT); // suppress ERROR recovery failure due to error from callback
    arangodb::application_features::ApplicationServer::server = nullptr;

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

SECTION("test_type") {
  CHECK((arangodb::LogicalDataSource::Type::emplace(arangodb::velocypack::StringRef("arangosearch")) == arangodb::iresearch::DATA_SOURCE_TYPE));
}

SECTION("test_defaults") {
  auto json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\" }");

  // view definition with LogicalView (for persistence)
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    arangodb::LogicalView::ptr view;
    CHECK((arangodb::iresearch::IResearchView::factory().create(view, vocbase, json->slice()).ok()));
    CHECK((false == !view));

    arangodb::iresearch::IResearchViewMeta expectedMeta;
    arangodb::iresearch::IResearchViewMetaState expectedMetaState;
    arangodb::velocypack::Builder builder;

    builder.openObject();
    view->properties(builder, true, true);
    builder.close();

    auto slice = builder.slice();
    arangodb::iresearch::IResearchViewMeta meta;
    arangodb::iresearch::IResearchViewMetaState metaState;
    std::string error;

    CHECK((15U == slice.length()));
    CHECK((slice.hasKey("globallyUniqueId") && slice.get("globallyUniqueId").isString() && false == slice.get("globallyUniqueId").copyString().empty()));
    CHECK(slice.get("name").copyString() == "testView");
    CHECK(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
    CHECK(false == slice.get("deleted").getBool());
    CHECK(false == slice.get("isSystem").getBool());
    CHECK((!slice.hasKey("links"))); // for persistence so no links
    CHECK((meta.init(slice, error) && expectedMeta == meta));
    CHECK((true == metaState.init(slice, error) && expectedMetaState == metaState));
  }

  // view definition with LogicalView
  {
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    arangodb::LogicalView::ptr view;
    CHECK((arangodb::iresearch::IResearchView::factory().create(view, vocbase, json->slice()).ok()));
    CHECK((false == !view));

    arangodb::iresearch::IResearchViewMeta expectedMeta;
    arangodb::velocypack::Builder builder;

    builder.openObject();
    view->properties(builder, true, false);
    builder.close();

    auto slice = builder.slice();
    arangodb::iresearch::IResearchViewMeta meta;
    std::string error;

    CHECK((slice.isObject()));
    CHECK((11U == slice.length()));
    CHECK((slice.hasKey("globallyUniqueId") && slice.get("globallyUniqueId").isString() && false == slice.get("globallyUniqueId").copyString().empty()));
    CHECK(slice.get("name").copyString() == "testView");
    CHECK(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
    CHECK((false == slice.hasKey("deleted")));
    CHECK((meta.init(slice, error) && expectedMeta == meta));

    auto tmpSlice = slice.get("links");
    CHECK((true == tmpSlice.isObject() && 0 == tmpSlice.length()));
  }

  // new view definition with links to missing collections
  {
    auto viewCreateJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\", \"id\": 101, \"links\": { \"testCollection\": {} } }");

    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    CHECK((true == !vocbase.lookupView("testView")));
    arangodb::LogicalView::ptr view;
    auto res = arangodb::iresearch::IResearchView::factory().create(view, vocbase, viewCreateJson->slice());
    CHECK((TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND == res.errorNumber()));
    CHECK((true == !vocbase.lookupView("testView")));
  }

  // new view definition with links with invalid definition
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\", \"id\": 100 }");
    auto viewCreateJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\", \"id\": 101, \"links\": { \"testCollection\": 42 } }");

    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    CHECK((nullptr != logicalCollection));
    CHECK((true == !vocbase.lookupView("testView")));
    CHECK((true == logicalCollection->getIndexes().empty()));
    arangodb::LogicalView::ptr view;
    auto res = arangodb::iresearch::IResearchView::factory().create(view, vocbase, viewCreateJson->slice());
    CHECK((TRI_ERROR_BAD_PARAMETER == res.errorNumber()));
    CHECK((true == !vocbase.lookupView("testView")));
  }

  // new view definition with links (collection not authorized)
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\", \"id\": 100 }");
    auto viewCreateJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\", \"links\": { \"testCollection\": {} } }");

    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    REQUIRE((nullptr != logicalCollection));

    struct ExecContext: public arangodb::ExecContext {
      ExecContext(): arangodb::ExecContext(arangodb::ExecContext::Type::Default, "", "",
                                           arangodb::auth::Level::NONE, arangodb::auth::Level::NONE) {}
    } execContext;
    auto* origExecContext = ExecContext::CURRENT;
    auto resetExecContext = irs::make_finally([origExecContext]()->void{ ExecContext::CURRENT = origExecContext; });
    ExecContext::CURRENT = &execContext;
    auto* authFeature = arangodb::AuthenticationFeature::instance();
    auto* userManager = authFeature->userManager();
    arangodb::auth::UserMap userMap; // empty map, no user -> no permissions
    userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database
    auto resetUserManager = irs::make_finally([userManager]()->void{ userManager->removeAllUsers(); });

    CHECK((true == !vocbase.lookupView("testView")));
    arangodb::LogicalView::ptr view;
    auto res = arangodb::iresearch::IResearchView::factory().create(view, vocbase, viewCreateJson->slice());
    CHECK((TRI_ERROR_FORBIDDEN == res.errorNumber()));
    CHECK((true == !vocbase.lookupView("testView")));
  }

  // new view definition with links
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\", \"id\": 100 }");
    auto viewCreateJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\", \"id\": 101, \"links\": { \"testCollection\": {} } }");

    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    CHECK((nullptr != logicalCollection));
    CHECK((true == !vocbase.lookupView("testView")));
    CHECK((true == logicalCollection->getIndexes().empty()));
    arangodb::LogicalView::ptr logicalView;
    CHECK((arangodb::iresearch::IResearchView::factory().create(logicalView, vocbase, viewCreateJson->slice()).ok()));
    REQUIRE((false == !logicalView));
    std::set<TRI_voc_cid_t> cids;
    logicalView->visitCollections([&cids](TRI_voc_cid_t cid)->bool { cids.emplace(cid); return true; });
    CHECK((1 == cids.size()));
    CHECK((false == logicalCollection->getIndexes().empty()));

    arangodb::iresearch::IResearchViewMeta expectedMeta;
    arangodb::velocypack::Builder builder;

    builder.openObject();
    logicalView->properties(builder, true, false);
    builder.close();

    auto slice = builder.slice();
    arangodb::iresearch::IResearchViewMeta meta;
    std::string error;
    CHECK((slice.isObject()));
    CHECK((11U == slice.length()));
    CHECK((slice.hasKey("globallyUniqueId") && slice.get("globallyUniqueId").isString() && false == slice.get("globallyUniqueId").copyString().empty()));
    CHECK((slice.get("name").copyString() == "testView"));
    CHECK((slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name()));
    CHECK((false == slice.hasKey("deleted")));
    CHECK((meta.init(slice, error) && expectedMeta == meta));

    auto tmpSlice = slice.get("links");
    CHECK((true == tmpSlice.isObject() && 1 == tmpSlice.length()));
    CHECK((true == tmpSlice.hasKey("testCollection")));
  }
}

SECTION("test_cleanup") {
  auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
  auto linkJson = arangodb::velocypack::Parser::fromJson("{ \"view\": \"testView\", \"includeAllFields\": true }");
  auto json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\":\"arangosearch\", \"cleanupIntervalStep\":1, \"consolidationIntervalMsec\": 1000 }");
  Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
  auto logicalCollection = vocbase.createCollection(collectionJson->slice());
  REQUIRE((false == !logicalCollection));
  auto logicalView = vocbase.createView(json->slice());
  REQUIRE((false == !logicalView));
  auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
  REQUIRE((false == !view));
  std::shared_ptr<arangodb::Index> index;
  REQUIRE((arangodb::iresearch::IResearchMMFilesLink::factory().instantiate(index, *logicalCollection, linkJson->slice(), 42, false).ok()));
  REQUIRE((false == !index));
  auto link = std::dynamic_pointer_cast<arangodb::iresearch::IResearchLink>(index);
  REQUIRE((false == !link));

  std::vector<std::string> const EMPTY;

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
    CHECK((link->insert(trx, arangodb::LocalDocumentId(0), doc->slice(), arangodb::Index::OperationMode::normal).ok()));
    CHECK((trx.commit().ok()));
    CHECK(view->commit().ok());
  }

  auto const memory = view->memory();

  // remove the data
  {
    arangodb::iresearch::IResearchLinkMeta meta;
    arangodb::transaction::Methods trx(
      arangodb::transaction::StandaloneContext::Create(vocbase),
      EMPTY,
      EMPTY,
      EMPTY,
      arangodb::transaction::Options()
    );
    CHECK((trx.begin().ok()));
    CHECK((link->remove(trx, arangodb::LocalDocumentId(0), arangodb::velocypack::Slice::emptyObjectSlice(), arangodb::Index::OperationMode::normal).ok()));
    CHECK((trx.commit().ok()));
    CHECK(view->commit().ok());
  }

  // wait for commit thread
  size_t const MAX_ATTEMPTS = 200;
  size_t attempt = 0;

  while (memory <= view->memory() && attempt < MAX_ATTEMPTS) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    ++attempt;
  }

  // ensure memory was freed
  CHECK(view->memory() <= memory);
}

SECTION("test_consolidate") {
  auto viewCreateJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\":\"arangosearch\", \"consolidationIntervalMsec\": 1000 }");
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
  auto logicalView = vocbase.createView(viewCreateJson->slice());
  REQUIRE((false == !logicalView));
  // FIXME TODO write test to check that long-running consolidation aborts on view drop
  // 1. create view with policy that blocks
  // 2. start policy
  // 3. drop view
  // 4. unblock policy
  // 5. ensure view drops immediately
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
  auto logicalCollection = vocbase.createCollection(collectionJson->slice());
  CHECK((nullptr != logicalCollection));
  CHECK((true == !vocbase.lookupView("testView")));
  CHECK((true == logicalCollection->getIndexes().empty()));
  CHECK((false == TRI_IsDirectory(dataPath.c_str()))); // createView(...) will call open()
  auto view = vocbase.createView(json->slice());
  REQUIRE((false == !view));

  CHECK((true == logicalCollection->getIndexes().empty()));
  CHECK((false == !vocbase.lookupView("testView")));
  CHECK((false == TRI_IsDirectory(dataPath.c_str())));
  CHECK((true == view->drop().ok()));
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
  auto logicalCollection = vocbase.createCollection(collectionJson->slice());
  CHECK((nullptr != logicalCollection));
  CHECK((true == !vocbase.lookupView("testView")));
  CHECK((true == logicalCollection->getIndexes().empty()));
  CHECK((false == TRI_IsDirectory(dataPath.c_str()))); // createView(...) will call open()
  auto view = vocbase.createView(json->slice());
  REQUIRE((false == !view));

  CHECK((true == logicalCollection->getIndexes().empty()));
  CHECK((false == !vocbase.lookupView("testView")));
  CHECK((false == TRI_IsDirectory(dataPath.c_str())));

  auto links = arangodb::velocypack::Parser::fromJson("{ \
    \"links\": { \"testCollection\": {} } \
  }");

  arangodb::Result res = view->properties(links->slice(), true);
  CHECK(true == res.ok());
  CHECK((false == logicalCollection->getIndexes().empty()));
  dataPath = ((((irs::utf8_path()/=s.testFilesystemPath)/=std::string("databases"))/=(std::string("database-") + std::to_string(vocbase.id())))/=(std::string("arangosearch-") + std::to_string(logicalCollection->id()) + "_" + std::to_string(view->planId()))).utf8();
  CHECK((true == TRI_IsDirectory(dataPath.c_str())));

  {
    struct ExecContext: public arangodb::ExecContext {
      ExecContext(): arangodb::ExecContext(arangodb::ExecContext::Type::Default, "", "",
                                           arangodb::auth::Level::NONE, arangodb::auth::Level::NONE) {}
    } execContext;
    arangodb::ExecContextScope execContextScope(&execContext);
    auto* authFeature = arangodb::AuthenticationFeature::instance();
    auto* userManager = authFeature->userManager();
    arangodb::aql::QueryRegistry queryRegistry(0); // required for UserManager::loadFromDB()
    userManager->setQueryRegistry(&queryRegistry);
    auto resetUserManager = std::shared_ptr<arangodb::auth::UserManager>(userManager, [](arangodb::auth::UserManager* ptr)->void { ptr->removeAllUsers(); });

    // not authorised (NONE collection) as per https://github.com/arangodb/backlog/issues/459
    {
      arangodb::auth::UserMap userMap;
      auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
      user.grantCollection(vocbase.name(), "testCollection", arangodb::auth::Level::NONE); // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

      CHECK((TRI_ERROR_FORBIDDEN == view->drop().errorNumber()));
      CHECK((false == logicalCollection->getIndexes().empty()));
      CHECK((false == !vocbase.lookupView("testView")));
      CHECK((true == TRI_IsDirectory(dataPath.c_str())));
    }

    // authorised (RO collection)
    {
      arangodb::auth::UserMap userMap;
      auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
      user.grantCollection(vocbase.name(), "testCollection", arangodb::auth::Level::RO); // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

      CHECK((true == view->drop().ok()));
      CHECK((true == logicalCollection->getIndexes().empty()));
      CHECK((true == !vocbase.lookupView("testView")));
      CHECK((false == TRI_IsDirectory(dataPath.c_str())));
    }
  }
}

SECTION("test_drop_collection") {
  auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
  auto viewCreateJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
  auto viewUpdateJson = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection\": { \"includeAllFields\": true } } }");
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
  auto logicalCollection = vocbase.createCollection(collectionJson->slice());
  REQUIRE((false == !logicalCollection));
  auto logicalView = vocbase.createView(viewCreateJson->slice());
  REQUIRE((false == !logicalView));
  auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
  REQUIRE((false == !view));

  CHECK((true == logicalView->properties(viewUpdateJson->slice(), true).ok()));
  CHECK((false == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));

  CHECK((true == logicalCollection->drop().ok()));
  CHECK((true == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));

  CHECK((true == logicalView->drop().ok()));
}

SECTION("test_drop_cid") {
  static std::vector<std::string> const EMPTY;

  // cid not in list of collections for snapshot (view definition not updated, not persisted)
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
    auto linkJson = arangodb::velocypack::Parser::fromJson("{ \"view\": \"testView\", \"includeAllFields\": true }");
    auto json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\":\"arangosearch\" }");
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    REQUIRE((false == !logicalCollection));
    auto logicalView = vocbase.createView(json->slice());
    REQUIRE((false == !logicalView));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
    REQUIRE((false == !view));
    std::shared_ptr<arangodb::Index> index;
    REQUIRE((arangodb::iresearch::IResearchMMFilesLink::factory().instantiate(index, *logicalCollection, linkJson->slice(), 42, false).ok()));
    REQUIRE((false == !index));
    auto link = std::dynamic_pointer_cast<arangodb::iresearch::IResearchLink>(index);
    REQUIRE((false == !link));

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
      CHECK((link->insert(trx, arangodb::LocalDocumentId(0), doc->slice(), arangodb::Index::OperationMode::normal).ok()));
      CHECK((trx.commit().ok()));
      CHECK(view->commit().ok());
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
      auto* snapshot = view->snapshot(trx, arangodb::iresearch::IResearchView::Snapshot::FindOrCreate);
      CHECK(1 == snapshot->live_docs_count());
    }

    // drop cid 42
    {
      bool persisted = false;
      auto before = StorageEngineMock::before;
      auto restore = irs::make_finally([&before]()->void { StorageEngineMock::before = before; });
      StorageEngineMock::before = [&persisted]()->void { persisted = true; };

      CHECK((true == view->unlink(logicalCollection->id()).ok()));
      CHECK((persisted)); // drop() modifies view meta if cid existed previously
      CHECK(view->commit().ok());
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
      auto* snapshot = view->snapshot(trx, arangodb::iresearch::IResearchView::Snapshot::FindOrCreate);
      CHECK(0 == snapshot->live_docs_count());
    }
  }

  // cid in list of collections for snapshot (view definition updated+persisted)
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"id\": 42, \"name\": \"testCollection\" }");
    auto linkJson = arangodb::velocypack::Parser::fromJson("{ \"view\": \"testView\", \"includeAllFields\": true }");
    auto json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\":\"arangosearch\", \"collections\": [ 42 ] }");
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    REQUIRE((false == !logicalCollection));
    auto logicalView = vocbase.createView(json->slice());
    REQUIRE((false == !logicalView));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
    REQUIRE((false == !view));
    std::shared_ptr<arangodb::Index> index;
    REQUIRE((arangodb::iresearch::IResearchMMFilesLink::factory().instantiate(index, *logicalCollection, linkJson->slice(), 42, false).ok()));
    REQUIRE((false == !index));
    auto link = std::dynamic_pointer_cast<arangodb::iresearch::IResearchLink>(index);
    REQUIRE((false == !link));

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
      CHECK((link->insert(trx, arangodb::LocalDocumentId(0), doc->slice(), arangodb::Index::OperationMode::normal).ok()));
      CHECK((trx.commit().ok()));
      CHECK(view->commit().ok());
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
      auto* snapshot = view->snapshot(trx, arangodb::iresearch::IResearchView::Snapshot::FindOrCreate);
      CHECK(1 == snapshot->live_docs_count());
    }

    // drop cid 42
    {
      bool persisted = false;
      auto before = StorageEngineMock::before;
      auto restore = irs::make_finally([&before]()->void { StorageEngineMock::before = before; });
      StorageEngineMock::before = [&persisted]()->void { persisted = true; };

      CHECK((true == view->unlink(logicalCollection->id()).ok()));
      CHECK((persisted)); // drop() modifies view meta if cid existed previously
      CHECK(view->commit().ok());
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
      auto* snapshot = view->snapshot(trx, arangodb::iresearch::IResearchView::Snapshot::FindOrCreate);
      CHECK(0 == snapshot->live_docs_count());
    }
  }

  // cid in list of collections for snapshot (view definition updated, not persisted until recovery is complete)
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"id\": 42, \"name\": \"testCollection\" }");
    auto linkJson = arangodb::velocypack::Parser::fromJson("{ \"view\": \"testView\", \"includeAllFields\": true }");
    auto json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\":\"arangosearch\", \"collections\": [ 42 ] }");
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    REQUIRE((false == !logicalCollection));
    auto logicalView = vocbase.createView(json->slice());
    REQUIRE((false == !logicalView));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
    REQUIRE((false == !view));
    std::shared_ptr<arangodb::Index> index;
    REQUIRE((arangodb::iresearch::IResearchMMFilesLink::factory().instantiate(index, *logicalCollection, linkJson->slice(), 42, false).ok()));
    REQUIRE((false == !index));
    auto link = std::dynamic_pointer_cast<arangodb::iresearch::IResearchLink>(index);
    REQUIRE((false == !link));

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
      CHECK((link->insert(trx, arangodb::LocalDocumentId(0), doc->slice(), arangodb::Index::OperationMode::normal).ok()));
      CHECK((trx.commit().ok()));
      CHECK(view->commit().ok());
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
      auto* snapshot = view->snapshot(trx, arangodb::iresearch::IResearchView::Snapshot::FindOrCreate);
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

      CHECK((true == view->unlink(logicalCollection->id()).ok()));
      CHECK((!persisted)); // drop() modifies view meta if cid existed previously (but not persisted until after recovery)
      CHECK(view->commit().ok());
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
      auto* snapshot = view->snapshot(trx, arangodb::iresearch::IResearchView::Snapshot::FindOrCreate);
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
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"id\": 42, \"name\": \"testCollection\" }");
    auto linkJson = arangodb::velocypack::Parser::fromJson("{ \"view\": \"testView\", \"includeAllFields\": true }");
    auto json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\":\"arangosearch\", \"collections\": [ 42 ] }");
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    REQUIRE((false == !logicalCollection));
    auto logicalView = vocbase.createView(json->slice());
    REQUIRE((false == !logicalView));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
    REQUIRE((false == !view));
    std::shared_ptr<arangodb::Index> index;
    REQUIRE((arangodb::iresearch::IResearchMMFilesLink::factory().instantiate(index, *logicalCollection, linkJson->slice(), 42, false).ok()));
    REQUIRE((false == !index));
    auto link = std::dynamic_pointer_cast<arangodb::iresearch::IResearchLink>(index);
    REQUIRE((false == !link));

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
      CHECK((link->insert(trx, arangodb::LocalDocumentId(0), doc->slice(), arangodb::Index::OperationMode::normal).ok()));
      CHECK((trx.commit().ok()));
      CHECK(view->commit().ok());
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
      auto* snapshot = view->snapshot(trx, arangodb::iresearch::IResearchView::Snapshot::FindOrCreate);
      CHECK(1 == snapshot->live_docs_count());
    }

    // drop cid 42
    {
      auto before = StorageEngineMock::before;
      auto restore = irs::make_finally([&before]()->void { StorageEngineMock::before = before; });
      StorageEngineMock::before = []()->void { throw std::exception(); };

      CHECK((true != view->unlink(logicalCollection->id()).ok()));
      CHECK(view->commit().ok());
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
      auto* snapshot = view->snapshot(trx, arangodb::iresearch::IResearchView::Snapshot::FindOrCreate);
      CHECK(1 == snapshot->live_docs_count());
    }

    // collection in view after drop failure
    {
      std::unordered_set<TRI_voc_cid_t> expected = { logicalCollection->id() };
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
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"id\": 42, \"name\": \"testCollection\" }");
    auto linkJson = arangodb::velocypack::Parser::fromJson("{ \"view\": \"testView\", \"includeAllFields\": true }");
    auto json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\":\"arangosearch\", \"collections\": [ 42 ] }");
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    REQUIRE((false == !logicalCollection));
    auto logicalView = vocbase.createView(json->slice());
    REQUIRE((false == !logicalView));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
    REQUIRE((false == !view));
    std::shared_ptr<arangodb::Index> index;
    REQUIRE((arangodb::iresearch::IResearchMMFilesLink::factory().instantiate(index, *logicalCollection, linkJson->slice(), 42, false).ok()));
    REQUIRE((false == !index));
    auto link = std::dynamic_pointer_cast<arangodb::iresearch::IResearchLink>(index);
    REQUIRE((false == !link));

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
      CHECK((link->insert(trx, arangodb::LocalDocumentId(0), doc->slice(), arangodb::Index::OperationMode::normal).ok()));
      CHECK((trx.commit().ok()));
      CHECK(view->commit().ok());
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
      auto* snapshot = view->snapshot(trx, arangodb::iresearch::IResearchView::Snapshot::FindOrCreate);
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

      CHECK((true == view->unlink(logicalCollection->id()).ok()));
      CHECK((!persisted)); // drop() modifies view meta if cid existed previously (but not persisted until after recovery)
      CHECK(view->commit().ok());
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
      auto* snapshot = view->snapshot(trx, arangodb::iresearch::IResearchView::Snapshot::FindOrCreate);
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

SECTION("test_drop_database") {
  auto viewCreateJson = arangodb::velocypack::Parser::fromJson("{ \"id\": \"42\", \"name\": \"testView\", \"type\": \"arangosearch\" }");
  auto* databaseFeature = arangodb::application_features::ApplicationServer::getFeature<arangodb::DatabaseFeature>("Database");
  REQUIRE((nullptr != databaseFeature));

  size_t beforeCount = 0;
  auto before = StorageEngineMock::before;
  auto restore = irs::make_finally([&before]()->void { StorageEngineMock::before = before; });
  StorageEngineMock::before = [&beforeCount]()->void { ++beforeCount; };

  TRI_vocbase_t* vocbase; // will be owned by DatabaseFeature
  REQUIRE((TRI_ERROR_NO_ERROR == databaseFeature->createDatabase(0, "testDatabase" TOSTRING(__LINE__), vocbase)));
  REQUIRE((nullptr != vocbase));

  beforeCount = 0; // reset before call to StorageEngine::createView(...)
  auto logicalView = vocbase->createView(viewCreateJson->slice());
  REQUIRE((false == !logicalView));
  CHECK((1 + 1 == beforeCount)); // +1 for StorageEngineMock::createView(...), +1 for StorageEngineMock::getViewProperties(...)

  beforeCount = 0; // reset before call to StorageEngine::dropView(...)
  CHECK((TRI_ERROR_NO_ERROR == databaseFeature->dropDatabase(vocbase->id(), true, true)));
  CHECK((1 == beforeCount));
}

SECTION("test_instantiate") {
  // valid version
  {
    auto json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\", \"version\": 1 }");
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    arangodb::LogicalView::ptr view;
    CHECK((arangodb::iresearch::IResearchView::factory().instantiate(view, vocbase, json->slice(), 0).ok()));
    CHECK((false == !view));
  }

  // no-longer supported version version
  {
    auto json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\", \"version\": 0 }");
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    arangodb::LogicalView::ptr view;
    CHECK((!arangodb::iresearch::IResearchView::factory().instantiate(view, vocbase, json->slice(), 0).ok()));
    CHECK((true == !view));
  }

  // unsupported version
  {
    auto json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\", \"version\": 123456789 }");
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    arangodb::LogicalView::ptr view;
    CHECK((!arangodb::iresearch::IResearchView::factory().instantiate(view, vocbase, json->slice(), 0).ok()));
    CHECK((true == !view));
  }
}

SECTION("test_truncate_cid") {
  static std::vector<std::string> const EMPTY;

  // cid not in list of collections for snapshot (view definition not updated, not persisted)
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
    auto linkJson = arangodb::velocypack::Parser::fromJson("{ \"view\": \"testView\", \"includeAllFields\": true }");
    auto json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\":\"arangosearch\" }");
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    REQUIRE((false == !logicalCollection));
    auto logicalView = vocbase.createView(json->slice());
    REQUIRE((false == !logicalView));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
    REQUIRE((false == !view));
    std::shared_ptr<arangodb::Index> index;
    REQUIRE((arangodb::iresearch::IResearchMMFilesLink::factory().instantiate(index, *logicalCollection, linkJson->slice(), 42, false).ok()));
    REQUIRE((false == !index));
    auto link = std::dynamic_pointer_cast<arangodb::iresearch::IResearchLink>(index);
    REQUIRE((false == !link));

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
      CHECK((link->insert(trx, arangodb::LocalDocumentId(0), doc->slice(), arangodb::Index::OperationMode::normal).ok()));
      CHECK((trx.commit().ok()));
      CHECK(view->commit().ok());
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
      auto* snapshot = view->snapshot(trx, arangodb::iresearch::IResearchView::Snapshot::FindOrCreate);
      CHECK(1 == snapshot->live_docs_count());
    }

    // truncate cid 42
    {
      bool persisted = false;
      auto before = StorageEngineMock::before;
      auto restore = irs::make_finally([&before]()->void { StorageEngineMock::before = before; });
      StorageEngineMock::before = [&persisted]()->void { persisted = true; };

      CHECK((true == view->unlink(logicalCollection->id()).ok()));
      CHECK((persisted)); // truncate() modifies view meta if cid existed previously
      CHECK(view->commit().ok());
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
      auto* snapshot = view->snapshot(trx, arangodb::iresearch::IResearchView::Snapshot::FindOrCreate);
      CHECK(0 == snapshot->live_docs_count());
    }
  }

  // cid in list of collections for snapshot (view definition not updated+persisted)
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"id\": 42, \"name\": \"testCollection\" }");
    auto linkJson = arangodb::velocypack::Parser::fromJson("{ \"view\": \"testView\", \"includeAllFields\": true }");
    auto json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\":\"arangosearch\", \"collections\": [ 42 ] }");
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    REQUIRE((false == !logicalCollection));
    auto logicalView = vocbase.createView(json->slice());
    REQUIRE((false == !logicalView));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
    REQUIRE((false == !view));
    std::shared_ptr<arangodb::Index> index;
    REQUIRE((arangodb::iresearch::IResearchMMFilesLink::factory().instantiate(index, *logicalCollection, linkJson->slice(), 42, false).ok()));
    REQUIRE((false == !index));
    auto link = std::dynamic_pointer_cast<arangodb::iresearch::IResearchLink>(index);
    REQUIRE((false == !link));

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
      CHECK((link->insert(trx, arangodb::LocalDocumentId(0), doc->slice(), arangodb::Index::OperationMode::normal).ok()));
      CHECK((trx.commit().ok()));
      CHECK(view->commit().ok());
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
      auto* snapshot = view->snapshot(trx, arangodb::iresearch::IResearchView::Snapshot::FindOrCreate);
      CHECK(1 == snapshot->live_docs_count());
    }

    // truncate cid 42
    {
      bool persisted = false;
      auto before = StorageEngineMock::before;
      auto restore = irs::make_finally([&before]()->void { StorageEngineMock::before = before; });
      StorageEngineMock::before = [&persisted]()->void { persisted = true; };

      CHECK((true == view->unlink(logicalCollection->id()).ok()));
      CHECK((persisted)); // truncate() modifies view meta if cid existed previously
      CHECK(view->commit().ok());
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
      auto* snapshot = view->snapshot(trx, arangodb::iresearch::IResearchView::Snapshot::FindOrCreate);
      CHECK(0 == snapshot->live_docs_count());
    }
  }
}

SECTION("test_emplace_cid") {
  // emplace (already in list)
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"id\": 42, \"name\": \"testCollection\" }");
    auto linkJson = arangodb::velocypack::Parser::fromJson("{ \"view\": \"testView\", \"includeAllFields\": true }");
    auto json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\":\"arangosearch\", \"collections\": [ 42 ] }");
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    REQUIRE((false == !logicalCollection));
    auto logicalView = vocbase.createView(json->slice());
    REQUIRE((false == !logicalView));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
    REQUIRE((false == !view));
    std::shared_ptr<arangodb::Index> index;
    REQUIRE((arangodb::iresearch::IResearchMMFilesLink::factory().instantiate(index, *logicalCollection, linkJson->slice(), 42, false).ok()));
    REQUIRE((false == !index));
    auto link = std::dynamic_pointer_cast<arangodb::iresearch::IResearchLink>(index);
    REQUIRE((false == !link));

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

      CHECK((false == view->link(link->self())));
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
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"id\": 42, \"name\": \"testCollection\" }");
    auto json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\":\"arangosearch\" }");
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    REQUIRE((false == !logicalCollection));
    auto logicalView = vocbase.createView(json->slice());
    REQUIRE((false == !logicalView));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
    REQUIRE((false == !view));

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
      struct Link: public arangodb::iresearch::IResearchLink {
        Link(TRI_idx_iid_t id, arangodb::LogicalCollection& col): IResearchLink(id, col) {}
      } link(42, *logicalCollection);
      auto asyncLinkPtr = std::make_shared<arangodb::iresearch::IResearchLink::AsyncLinkPtr::element_type>(&link);

      CHECK((true == view->link(asyncLinkPtr)));
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
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"id\": 42, \"name\": \"testCollection\" }");
    auto json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\":\"arangosearch\"  }");
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    REQUIRE((false == !logicalCollection));
    auto logicalView = vocbase.createView(json->slice());
    REQUIRE((false == !logicalView));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
    REQUIRE((false == !view));

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
      struct Link: public arangodb::iresearch::IResearchLink {
        Link(TRI_idx_iid_t id, arangodb::LogicalCollection& col): IResearchLink(id, col) {}
      } link(42, *logicalCollection);
      auto asyncLinkPtr = std::make_shared<arangodb::iresearch::IResearchLink::AsyncLinkPtr::element_type>(&link);

      CHECK((true == view->link(asyncLinkPtr)));
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
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"id\": 42, \"name\": \"testCollection\" }");
    auto json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\":\"arangosearch\" }");
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    REQUIRE((false == !logicalCollection));
    auto logicalView = vocbase.createView(json->slice());
    REQUIRE((false == !logicalView));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
    REQUIRE((false == !view));

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
      struct Link: public arangodb::iresearch::IResearchLink {
        Link(TRI_idx_iid_t id, arangodb::LogicalCollection& col): IResearchLink(id, col) {}
      } link(42, *logicalCollection);
      auto asyncLinkPtr = std::make_shared<arangodb::iresearch::IResearchLink::AsyncLinkPtr::element_type>(&link);

      CHECK((false == view->link(asyncLinkPtr)));
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
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"id\": 42, \"name\": \"testCollection\" }");
    auto json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\":\"arangosearch\" }");
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    REQUIRE((false == !logicalCollection));
    auto logicalView = vocbase.createView(json->slice());
    REQUIRE((false == !logicalView));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
    REQUIRE((false == !view));

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
      struct Link: public arangodb::iresearch::IResearchLink {
        Link(TRI_idx_iid_t id, arangodb::LogicalCollection& col): IResearchLink(id, col) {}
      } link(42, *logicalCollection);
      auto asyncLinkPtr = std::make_shared<arangodb::iresearch::IResearchLink::AsyncLinkPtr::element_type>(&link);

      CHECK((true == view->link(asyncLinkPtr)));
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
  auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
  auto linkJson = arangodb::velocypack::Parser::fromJson("{ \"view\": \"testView\", \"includeAllFields\": true }");
  auto viewJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\":\"arangosearch\" }");
  arangodb::aql::AstNode noop(arangodb::aql::AstNodeType::NODE_TYPE_FILTER);
  arangodb::aql::AstNode noopChild(arangodb::aql::AstNodeValue(true));

  noop.addMember(&noopChild);

  // in recovery (removes cid+rid before insert)
  {
    auto before = StorageEngineMock::inRecoveryResult;
    StorageEngineMock::inRecoveryResult = true;
    auto restore = irs::make_finally([&before]()->void { StorageEngineMock::inRecoveryResult = before; });
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    REQUIRE((false == !logicalCollection));
    auto viewImpl = vocbase.createView(viewJson->slice());
    CHECK((false == !viewImpl));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(viewImpl.get());
    CHECK((nullptr != view));
    std::shared_ptr<arangodb::Index> index;
    REQUIRE((arangodb::iresearch::IResearchMMFilesLink::factory().instantiate(index, *logicalCollection, linkJson->slice(), 42, false).ok()));
    REQUIRE((false == !index));
    auto link = std::dynamic_pointer_cast<arangodb::iresearch::IResearchLink>(index);
    REQUIRE((false == !link));

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
      CHECK((link->insert(trx, arangodb::LocalDocumentId(1), docJson->slice(), arangodb::Index::OperationMode::normal).ok()));
      CHECK((link->insert(trx, arangodb::LocalDocumentId(2), docJson->slice(), arangodb::Index::OperationMode::normal).ok()));
      CHECK((link->insert(trx, arangodb::LocalDocumentId(1), docJson->slice(), arangodb::Index::OperationMode::normal).ok())); // 2nd time
      CHECK((link->insert(trx, arangodb::LocalDocumentId(2), docJson->slice(), arangodb::Index::OperationMode::normal).ok())); // 2nd time
      CHECK((trx.commit().ok()));
      CHECK(view->commit().ok());
    }

    arangodb::transaction::Methods trx(
      arangodb::transaction::StandaloneContext::Create(vocbase),
      EMPTY,
      EMPTY,
      EMPTY,
      arangodb::transaction::Options()
    );
    auto* snapshot = view->snapshot(trx, arangodb::iresearch::IResearchView::Snapshot::FindOrCreate);
    CHECK(2 == snapshot->live_docs_count());
  }

  // in recovery batch (removes cid+rid before insert)
  {
    auto before = StorageEngineMock::inRecoveryResult;
    StorageEngineMock::inRecoveryResult = true;
    auto restore = irs::make_finally([&before]()->void { StorageEngineMock::inRecoveryResult = before; });
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    REQUIRE((false == !logicalCollection));
    auto viewImpl = vocbase.createView(viewJson->slice());
    CHECK((false == !viewImpl));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(viewImpl.get());
    CHECK((nullptr != view));
    std::shared_ptr<arangodb::Index> index;
    REQUIRE((arangodb::iresearch::IResearchMMFilesLink::factory().instantiate(index, *logicalCollection, linkJson->slice(), 42, false).ok()));
    REQUIRE((false == !index));
    auto link = std::dynamic_pointer_cast<arangodb::iresearch::IResearchLink>(index);
    REQUIRE((false == !link));

    {
      auto docJson = arangodb::velocypack::Parser::fromJson("{\"abc\": \"def\"}");
      arangodb::basics::LocalTaskQueue taskQueue(nullptr);
      auto taskQueuePtr = std::shared_ptr<arangodb::basics::LocalTaskQueue>(&taskQueue, [](arangodb::basics::LocalTaskQueue*)->void{});
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
      link->batchInsert(trx, batch, taskQueuePtr);
      link->batchInsert(trx, batch, taskQueuePtr); // 2nd time
      CHECK((TRI_ERROR_NO_ERROR == taskQueue.status()));
      CHECK((trx.commit().ok()));
      CHECK(view->commit().ok());
    }

      arangodb::transaction::Methods trx(
        arangodb::transaction::StandaloneContext::Create(vocbase),
        EMPTY,
        EMPTY,
        EMPTY,
        arangodb::transaction::Options()
      );
    auto* snapshot = view->snapshot(trx, arangodb::iresearch::IResearchView::Snapshot::FindOrCreate);
    CHECK((2 == snapshot->live_docs_count()));
  }

  // not in recovery (FindOrCreate)
  {
    StorageEngineMock::inRecoveryResult = false;
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    REQUIRE((false == !logicalCollection));
    auto viewImpl = vocbase.createView(viewJson->slice());
    CHECK((false == !viewImpl));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(viewImpl.get());
    CHECK((nullptr != view));
    std::shared_ptr<arangodb::Index> index;
    REQUIRE((arangodb::iresearch::IResearchMMFilesLink::factory().instantiate(index, *logicalCollection, linkJson->slice(), 42, false).ok()));
    REQUIRE((false == !index));
    auto link = std::dynamic_pointer_cast<arangodb::iresearch::IResearchLink>(index);
    REQUIRE((false == !link));

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
      CHECK((link->insert(trx, arangodb::LocalDocumentId(1), docJson->slice(), arangodb::Index::OperationMode::normal).ok()));
      CHECK((link->insert(trx, arangodb::LocalDocumentId(2), docJson->slice(), arangodb::Index::OperationMode::normal).ok()));
      CHECK((link->insert(trx, arangodb::LocalDocumentId(1), docJson->slice(), arangodb::Index::OperationMode::normal).ok())); // 2nd time
      CHECK((link->insert(trx, arangodb::LocalDocumentId(2), docJson->slice(), arangodb::Index::OperationMode::normal).ok())); // 2nd time
      CHECK((trx.commit().ok()));
      CHECK(view->commit().ok());
    }

    arangodb::transaction::Methods trx(
      arangodb::transaction::StandaloneContext::Create(vocbase),
      EMPTY,
      EMPTY,
      EMPTY,
      arangodb::transaction::Options()
    );
    auto* snapshot = view->snapshot(trx, arangodb::iresearch::IResearchView::Snapshot::FindOrCreate);
    CHECK((4 == snapshot->docs_count()));
  }

  // not in recovery (SyncAndReplace)
  {
    StorageEngineMock::inRecoveryResult = false;
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    REQUIRE((false == !logicalCollection));
    auto viewImpl = vocbase.createView(viewJson->slice());
    CHECK((false == !viewImpl));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(viewImpl.get());
    CHECK((nullptr != view));
    CHECK(view->category() == arangodb::LogicalView::category());
    std::shared_ptr<arangodb::Index> index;
    REQUIRE((arangodb::iresearch::IResearchMMFilesLink::factory().instantiate(index, *logicalCollection, linkJson->slice(), 42, false).ok()));
    REQUIRE((false == !index));
    auto link = std::dynamic_pointer_cast<arangodb::iresearch::IResearchLink>(index);
    REQUIRE((false == !link));

    {
      auto docJson = arangodb::velocypack::Parser::fromJson("{\"abc\": \"def\"}");
      arangodb::iresearch::IResearchLinkMeta linkMeta;
      arangodb::transaction::Options options;
      arangodb::transaction::Methods trx(
        arangodb::transaction::StandaloneContext::Create(vocbase),
        EMPTY,
        EMPTY,
        EMPTY,
        options
      );

      linkMeta._includeAllFields = true;
      CHECK((trx.begin().ok()));
      CHECK((link->insert(trx, arangodb::LocalDocumentId(1), docJson->slice(), arangodb::Index::OperationMode::normal).ok()));
      CHECK((link->insert(trx, arangodb::LocalDocumentId(2), docJson->slice(), arangodb::Index::OperationMode::normal).ok()));
      CHECK((link->insert(trx, arangodb::LocalDocumentId(1), docJson->slice(), arangodb::Index::OperationMode::normal).ok())); // 2nd time
      CHECK((link->insert(trx, arangodb::LocalDocumentId(2), docJson->slice(), arangodb::Index::OperationMode::normal).ok())); // 2nd time
      CHECK((trx.commit().ok()));
    }

    arangodb::transaction::Methods trx(
      arangodb::transaction::StandaloneContext::Create(vocbase),
      EMPTY,
      EMPTY,
      EMPTY,
      arangodb::transaction::Options()
    );
    auto* snapshot = view->snapshot(trx, arangodb::iresearch::IResearchView::Snapshot::SyncAndReplace);
    CHECK((4 == snapshot->docs_count()));
  }

  // not in recovery : single operation transaction
  {
    StorageEngineMock::inRecoveryResult = false;
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    REQUIRE((false == !logicalCollection));
    auto viewImpl = vocbase.createView(viewJson->slice());
    CHECK((false == !viewImpl));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(viewImpl.get());
    CHECK((nullptr != view));
    CHECK(view->category() == arangodb::LogicalView::category());
    std::shared_ptr<arangodb::Index> index;
    REQUIRE((arangodb::iresearch::IResearchMMFilesLink::factory().instantiate(index, *logicalCollection, linkJson->slice(), 42, false).ok()));
    REQUIRE((false == !index));
    auto link = std::dynamic_pointer_cast<arangodb::iresearch::IResearchLink>(index);
    REQUIRE((false == !link));

    {
      auto docJson = arangodb::velocypack::Parser::fromJson("{\"abc\": \"def\"}");
      arangodb::iresearch::IResearchLinkMeta linkMeta;
      arangodb::transaction::Options options;
      arangodb::transaction::Methods trx(
        arangodb::transaction::StandaloneContext::Create(vocbase),
        EMPTY,
        EMPTY,
        EMPTY,
        options
      );
      trx.addHint(arangodb::transaction::Hints::Hint::SINGLE_OPERATION);

      linkMeta._includeAllFields = true;
      CHECK((trx.begin().ok()));
      CHECK((link->insert(trx, arangodb::LocalDocumentId(1), docJson->slice(), arangodb::Index::OperationMode::normal).ok()));
      CHECK((trx.commit().ok()));
    }

    arangodb::transaction::Methods trx(
      arangodb::transaction::StandaloneContext::Create(vocbase),
      EMPTY,
      EMPTY,
      EMPTY,
      arangodb::transaction::Options()
    );
    auto* snapshot = view->snapshot(trx, arangodb::iresearch::IResearchView::Snapshot::SyncAndReplace);
    CHECK((1 == snapshot->docs_count()));
  }

  // not in recovery batch (FindOrCreate)
  {
    StorageEngineMock::inRecoveryResult = false;
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    REQUIRE((false == !logicalCollection));
    auto viewImpl = vocbase.createView(viewJson->slice());
    CHECK((false == !viewImpl));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(viewImpl.get());
    CHECK((nullptr != view));
    std::shared_ptr<arangodb::Index> index;
    REQUIRE((arangodb::iresearch::IResearchMMFilesLink::factory().instantiate(index, *logicalCollection, linkJson->slice(), 42, false).ok()));
    REQUIRE((false == !index));
    auto link = std::dynamic_pointer_cast<arangodb::iresearch::IResearchLink>(index);
    REQUIRE((false == !link));

    {
      auto docJson = arangodb::velocypack::Parser::fromJson("{\"abc\": \"def\"}");
      arangodb::basics::LocalTaskQueue taskQueue(nullptr);
      auto taskQueuePtr = std::shared_ptr<arangodb::basics::LocalTaskQueue>(&taskQueue, [](arangodb::basics::LocalTaskQueue*)->void{});
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
      link->batchInsert(trx, batch, taskQueuePtr);
      link->batchInsert(trx, batch, taskQueuePtr); // 2nd time
      CHECK((TRI_ERROR_NO_ERROR == taskQueue.status()));
      CHECK((trx.commit().ok()));
      CHECK(view->commit().ok());
    }

    arangodb::transaction::Methods trx(
      arangodb::transaction::StandaloneContext::Create(vocbase),
      EMPTY,
      EMPTY,
      EMPTY,
      arangodb::transaction::Options()
    );
    auto* snapshot = view->snapshot(trx, arangodb::iresearch::IResearchView::Snapshot::FindOrCreate);
    CHECK((4 == snapshot->docs_count()));
  }

  // not in recovery batch (SyncAndReplace)
  {
    StorageEngineMock::inRecoveryResult = false;
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    REQUIRE((false == !logicalCollection));
    auto viewImpl = vocbase.createView(viewJson->slice());
    CHECK((false == !viewImpl));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(viewImpl.get());
    CHECK((nullptr != view));
    std::shared_ptr<arangodb::Index> index;
    REQUIRE((arangodb::iresearch::IResearchMMFilesLink::factory().instantiate(index, *logicalCollection, linkJson->slice(), 42, false).ok()));
    REQUIRE((false == !index));
    auto link = std::dynamic_pointer_cast<arangodb::iresearch::IResearchLink>(index);
    REQUIRE((false == !link));

    {
      auto docJson = arangodb::velocypack::Parser::fromJson("{\"abc\": \"def\"}");
      arangodb::basics::LocalTaskQueue taskQueue(nullptr);
      auto taskQueuePtr = std::shared_ptr<arangodb::basics::LocalTaskQueue>(&taskQueue, [](arangodb::basics::LocalTaskQueue*)->void{});
      arangodb::iresearch::IResearchLinkMeta linkMeta;
      arangodb::transaction::Options options;
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
      link->batchInsert(trx, batch, taskQueuePtr);
      link->batchInsert(trx, batch, taskQueuePtr); // 2nd time
      CHECK((TRI_ERROR_NO_ERROR == taskQueue.status()));
      CHECK((trx.commit().ok()));
    }

    arangodb::transaction::Methods trx(
      arangodb::transaction::StandaloneContext::Create(vocbase),
      EMPTY,
      EMPTY,
      EMPTY,
      arangodb::transaction::Options()
    );
    auto* snapshot = view->snapshot(trx, arangodb::iresearch::IResearchView::Snapshot::SyncAndReplace);
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
    arangodb::LogicalView::ptr view;
    REQUIRE((arangodb::iresearch::IResearchView::factory().instantiate(view, vocbase, json->slice(), 0).ok()));
    CHECK((false == !view));
    CHECK((false == TRI_IsDirectory(dataPath.c_str())));
    view->open();
    CHECK((false == TRI_IsDirectory(dataPath.c_str())));
  }
}

SECTION("test_query") {
  auto createJson = arangodb::velocypack::Parser::fromJson("{ \
    \"name\": \"testView\", \
    \"type\": \"arangosearch\" \
  }");
  static std::vector<std::string> const EMPTY;
  arangodb::aql::AstNode noop(arangodb::aql::AstNodeType::NODE_TYPE_FILTER);
  arangodb::aql::AstNode noopChild(arangodb::aql::AstNodeValue(true)); // all

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
    auto* snapshot = view->snapshot(trx, arangodb::iresearch::IResearchView::Snapshot::FindOrCreate);
    CHECK(0 == snapshot->docs_count());
  }

  // ordered iterator
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
    auto linkJson = arangodb::velocypack::Parser::fromJson("{ \"view\": \"testView\", \"includeAllFields\": true }");
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    REQUIRE((false == !logicalCollection));
    auto logicalView = vocbase.createView(createJson->slice());
    CHECK((false == !logicalView));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
    CHECK((false == !view));
    std::shared_ptr<arangodb::Index> index;
    REQUIRE((arangodb::iresearch::IResearchMMFilesLink::factory().instantiate(index, *logicalCollection, linkJson->slice(), 42, false).ok()));
    REQUIRE((false == !index));
    auto link = std::dynamic_pointer_cast<arangodb::iresearch::IResearchLink>(index);
    REQUIRE((false == !link));

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
        CHECK((link->insert(trx, arangodb::LocalDocumentId(i), doc->slice(), arangodb::Index::OperationMode::normal).ok()));
      }

      CHECK((trx.commit().ok()));
      CHECK(view->commit().ok());
    }

    arangodb::transaction::Methods trx(
      arangodb::transaction::StandaloneContext::Create(vocbase),
      EMPTY,
      EMPTY,
      EMPTY,
      arangodb::transaction::Options()
    );
    auto* snapshot = view->snapshot(trx, arangodb::iresearch::IResearchView::Snapshot::FindOrCreate);
    CHECK(12 == snapshot->docs_count());
  }

  // snapshot isolation
  {
    auto links = arangodb::velocypack::Parser::fromJson("{ \
      \"links\": { \"testCollection\": { \"includeAllFields\" : true } } \
    }");
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");

    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    std::vector<std::string> collections{ logicalCollection->name() };
    auto logicalView = vocbase.createView(createJson->slice());
    CHECK((false == !logicalView));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
    CHECK((false == !view));
    arangodb::Result res = logicalView->properties(links->slice(), true);
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
      CHECK(view->commit().ok());
    }

    arangodb::transaction::Methods trx0(
      arangodb::transaction::StandaloneContext::Create(vocbase),
      EMPTY,
      EMPTY,
      EMPTY,
      arangodb::transaction::Options()
    );
    auto* snapshot0 = view->snapshot(trx0, arangodb::iresearch::IResearchView::Snapshot::FindOrCreate);
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
      CHECK(view->commit().ok());
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
    auto* snapshot1 = view->snapshot(trx1, arangodb::iresearch::IResearchView::Snapshot::FindOrCreate);
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
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    auto logicalView = vocbase.createView(viewCreateJson->slice());
    REQUIRE((false == !logicalView));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
    REQUIRE((false == !view));
    arangodb::Result res = logicalView->properties(viewUpdateJson->slice(), true);
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

    arangodb::aql::Variable variable("testVariable", 0);

    // test insert + query
    for (size_t i = 1; i < 200; ++i) {
      // insert
      {
        auto doc = arangodb::velocypack::Parser::fromJson(std::string("{ \"seq\": ") + std::to_string(i) + " }");
        arangodb::transaction::Methods trx(
          arangodb::transaction::StandaloneContext::Create(vocbase),
          EMPTY,
          std::vector<std::string>{logicalCollection->name()},
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
        auto* snapshot = view->snapshot(trx, arangodb::iresearch::IResearchView::Snapshot::SyncAndReplace);
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
  auto viewJson1 = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\", \"id\": 101, \"collections\": [ 100 ] }");
  auto linkJson  = arangodb::velocypack::Parser::fromJson("{ \"view\": \"101\" }");

  // new link in recovery
  {
    s.engine.views.clear();
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    REQUIRE((false == !logicalCollection));
    auto logicalView = vocbase.createView(viewJson0->slice());
    REQUIRE((false == !logicalView));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
    REQUIRE((false == !view));

    {
      arangodb::velocypack::Builder builder;
      builder.openObject();
      view->properties(builder, false, false);
      builder.close();

      auto slice = builder.slice();
      CHECK(slice.isObject());
      CHECK((4U == slice.length()));
      CHECK((slice.hasKey("globallyUniqueId") && slice.get("globallyUniqueId").isString() && false == slice.get("globallyUniqueId").copyString().empty()));
      CHECK(slice.get("id").copyString() == "101");
      CHECK(slice.get("name").copyString() == "testView");
      CHECK(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
      CHECK(slice.get("deleted").isNone()); // no system properties
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
    std::shared_ptr<arangodb::Index> link;
    CHECK((arangodb::iresearch::IResearchMMFilesLink::factory().instantiate(link, *logicalCollection, linkJson->slice(), 1, false).ok()));
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
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    REQUIRE((false == !logicalCollection));
    auto logicalView = vocbase.createView(viewJson0->slice());
    REQUIRE((false == !logicalView));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
    REQUIRE((false == !view));

    {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      view->properties(builder, false, false);
      builder.close();

      auto slice = builder.slice();
      CHECK(slice.isObject());
      CHECK((4U == slice.length()));
      CHECK((slice.hasKey("globallyUniqueId") && slice.get("globallyUniqueId").isString() && false == slice.get("globallyUniqueId").copyString().empty()));
      CHECK(slice.get("id").copyString() == "101");
      CHECK(slice.get("name").copyString() == "testView");
      CHECK(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
      CHECK(slice.get("deleted").isNone()); // no system properties
    }

    {
      std::unordered_set<TRI_voc_cid_t> cids;
      CHECK(view->commit().ok());
      arangodb::transaction::Methods trx(
        arangodb::transaction::StandaloneContext::Create(vocbase),
        EMPTY,
        EMPTY,
        EMPTY,
        arangodb::transaction::Options()
      );
      auto* snapshot = view->snapshot(trx, arangodb::iresearch::IResearchView::Snapshot::FindOrCreate);
      CHECK((0 == snapshot->docs_count()));
    }

    {
      std::set<TRI_voc_cid_t> actual;
      view->visitCollections([&actual](TRI_voc_cid_t cid)->bool { actual.emplace(cid); return true; });
      CHECK((actual.empty()));
    }

    persisted = false;
    std::shared_ptr<arangodb::Index> link;
    CHECK((arangodb::iresearch::IResearchMMFilesLink::factory().instantiate(link, *logicalCollection, linkJson->slice(), 1, false).ok()));
    CHECK((true == persisted)); // link instantiation does modify and persist view meta
    CHECK((false == !link));
    std::unordered_set<TRI_voc_cid_t> cids;
    CHECK(view->commit().ok());
    arangodb::transaction::Methods trx(
      arangodb::transaction::StandaloneContext::Create(vocbase),
      EMPTY,
      EMPTY,
      EMPTY,
      arangodb::transaction::Options()
    );
    auto* snapshot = view->snapshot(trx, arangodb::iresearch::IResearchView::Snapshot::FindOrCreate);
    CHECK((0 == snapshot->docs_count())); // link addition does trigger collection load

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
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    REQUIRE((false == !logicalCollection));
    auto logicalView = vocbase.createView(viewJson1->slice());
    REQUIRE((false == !logicalView));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
    REQUIRE((false == !view));

    {
      std::unordered_set<TRI_voc_cid_t> cids;
      CHECK((TRI_ERROR_ARANGO_INDEX_HANDLE_BAD == view->commit().errorNumber()));
      arangodb::transaction::Methods trx(
        arangodb::transaction::StandaloneContext::Create(vocbase),
        EMPTY,
        EMPTY,
        EMPTY,
        arangodb::transaction::Options()
      );
      auto* snapshot = view->snapshot(trx, arangodb::iresearch::IResearchView::Snapshot::FindOrCreate);
      CHECK((nullptr == snapshot));
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
    std::shared_ptr<arangodb::Index> link0;
    CHECK((arangodb::iresearch::IResearchMMFilesLink::factory().instantiate(link0, *logicalCollection, linkJson->slice(), 1, false).ok()));
    CHECK((false == persisted));
    CHECK((false == !link0));

    {
      std::unordered_set<TRI_voc_cid_t> cids;
      CHECK(view->commit().ok());
      arangodb::transaction::Methods trx(
        arangodb::transaction::StandaloneContext::Create(vocbase),
        EMPTY,
        EMPTY,
        EMPTY,
        arangodb::transaction::Options()
      );
      auto* snapshot = view->snapshot(trx, arangodb::iresearch::IResearchView::Snapshot::FindOrCreate);
      CHECK((0 == snapshot->docs_count())); // link addition does trigger collection load
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
    std::shared_ptr<arangodb::Index> link1;
    CHECK((!arangodb::iresearch::IResearchMMFilesLink::factory().instantiate(link1, *logicalCollection, linkJson->slice(), 1, false).ok()));
    link0.reset(); // unload link before creating a new link instance
    CHECK((arangodb::iresearch::IResearchMMFilesLink::factory().instantiate(link1, *logicalCollection, linkJson->slice(), 1, false).ok()));
    CHECK((false == persisted));
    CHECK((false == !link1)); // duplicate link creation is allowed
    std::unordered_set<TRI_voc_cid_t> cids;
    CHECK(view->commit().ok());
    arangodb::transaction::Methods trx(
      arangodb::transaction::StandaloneContext::Create(vocbase),
      EMPTY,
      EMPTY,
      EMPTY,
      arangodb::transaction::Options()
    );
    auto* snapshot = view->snapshot(trx, arangodb::iresearch::IResearchView::Snapshot::FindOrCreate);
    CHECK((0 == snapshot->docs_count())); // link addition does trigger collection load

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
  auto linkJson = arangodb::velocypack::Parser::fromJson("{ \"view\": \"testView\", \"includeAllFields\": true }");
  auto viewJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\", \"id\": 101 }");

  // link removed before view (in recovery)
  {
    s.engine.views.clear();
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    REQUIRE((false == !logicalCollection));
    auto logicalView = vocbase.createView(viewJson->slice());
    REQUIRE((false == !logicalView));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
    REQUIRE((false == !view));
    std::shared_ptr<arangodb::Index> index;
    REQUIRE((arangodb::iresearch::IResearchMMFilesLink::factory().instantiate(index, *logicalCollection, linkJson->slice(), 42, false).ok()));
    REQUIRE((false == !index));
    auto link = std::dynamic_pointer_cast<arangodb::iresearch::IResearchLink>(index);
    REQUIRE((false == !link));

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
      CHECK((link->insert(trx, arangodb::LocalDocumentId(0), doc->slice(), arangodb::Index::OperationMode::normal).ok()));
      CHECK((trx.commit().ok()));
      CHECK((view->commit().ok()));
    }

    auto links = arangodb::velocypack::Parser::fromJson("{ \
      \"links\": { \"testCollection\": {} } \
    }");

    link->unload(); // unload link before creating a new link instance
    arangodb::Result res = logicalView->properties(links->slice(), true);
    CHECK(true == res.ok());
    CHECK((false == logicalCollection->getIndexes().empty()));

    {
      std::unordered_set<TRI_voc_cid_t> cids;
      CHECK(view->commit().ok());
      arangodb::transaction::Methods trx(
        arangodb::transaction::StandaloneContext::Create(vocbase),
        EMPTY,
        EMPTY,
        EMPTY,
        arangodb::transaction::Options()
      );
      auto* snapshot = view->snapshot(trx, arangodb::iresearch::IResearchView::Snapshot::FindOrCreate);
      CHECK((1 == snapshot->docs_count()));
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
      CHECK(view->commit().ok());
      arangodb::transaction::Methods trx(
        arangodb::transaction::StandaloneContext::Create(vocbase),
        EMPTY,
        EMPTY,
        EMPTY,
        arangodb::transaction::Options()
      );
      auto* snapshot = view->snapshot(trx, arangodb::iresearch::IResearchView::Snapshot::FindOrCreate);
      CHECK((0 == snapshot->docs_count()));
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
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    REQUIRE((false == !logicalCollection));
    auto logicalView = vocbase.createView(viewJson->slice());
    REQUIRE((false == !logicalView));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
    REQUIRE((false == !view));
    std::shared_ptr<arangodb::Index> index;
    REQUIRE((arangodb::iresearch::IResearchMMFilesLink::factory().instantiate(index, *logicalCollection, linkJson->slice(), 42, false).ok()));
    REQUIRE((false == !index));
    auto link = std::dynamic_pointer_cast<arangodb::iresearch::IResearchLink>(index);
    REQUIRE((false == !link));

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
      CHECK((link->insert(trx, arangodb::LocalDocumentId(0), doc->slice(), arangodb::Index::OperationMode::normal).ok()));
      CHECK((trx.commit().ok()));
      CHECK((view->commit().ok()));
    }

    auto links = arangodb::velocypack::Parser::fromJson("{ \
      \"links\": { \"testCollection\": {} } \
    }");

    link->unload(); // unload link before creating a new link instance
    arangodb::Result res = logicalView->properties(links->slice(), true);
    CHECK(true == res.ok());
    CHECK((false == logicalCollection->getIndexes().empty()));

    {
      std::unordered_set<TRI_voc_cid_t> cids;
      CHECK(view->commit().ok());
      arangodb::transaction::Methods trx(
        arangodb::transaction::StandaloneContext::Create(vocbase),
        EMPTY,
        EMPTY,
        EMPTY,
        arangodb::transaction::Options()
      );
      auto* snapshot = view->snapshot(trx, arangodb::iresearch::IResearchView::Snapshot::FindOrCreate);
      CHECK((1 == snapshot->docs_count()));
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
      CHECK(view->commit().ok());
      arangodb::transaction::Methods trx(
        arangodb::transaction::StandaloneContext::Create(vocbase),
        EMPTY,
        EMPTY,
        EMPTY,
        arangodb::transaction::Options()
      );
      auto* snapshot = view->snapshot(trx, arangodb::iresearch::IResearchView::Snapshot::FindOrCreate);
      CHECK((0 == snapshot->docs_count()));
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
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    auto logicalView = vocbase.createView(viewJson->slice());
    REQUIRE((false == !logicalView));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
    REQUIRE((false == !view));

    auto links = arangodb::velocypack::Parser::fromJson("{ \
      \"links\": { \"testCollection\": {} } \
    }");

    arangodb::Result res = logicalView->properties(links->slice(), true);
    CHECK(true == res.ok());
    CHECK((false == logicalCollection->getIndexes().empty()));

    std::set<TRI_voc_cid_t> cids;
    view->visitCollections([&cids](TRI_voc_cid_t cid)->bool { cids.emplace(cid); return true; });
    CHECK((1 == cids.size()));
    CHECK((false == !vocbase.lookupView("testView")));
    CHECK((true == view->drop().ok()));
    CHECK((true == !vocbase.lookupView("testView")));
    CHECK((nullptr != vocbase.lookupCollection("testCollection")));
    CHECK((true == vocbase.dropCollection(logicalCollection->id(), true, -1).ok()));
    CHECK((nullptr == vocbase.lookupCollection("testCollection")));
  }

  // view deallocated before link removed
  {
    s.engine.views.clear();
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());

    {
      auto createJson = arangodb::velocypack::Parser::fromJson("{}");
      auto updateJson = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection\": {} } }");
      auto logicalView = vocbase.createView(viewJson->slice());
      REQUIRE((false == !logicalView));
      auto* viewImpl = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
      REQUIRE((nullptr != viewImpl));
      CHECK((viewImpl->properties(updateJson->slice(), true).ok()));
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
      arangodb::LogicalView::ptr view;
      REQUIRE((arangodb::iresearch::IResearchView::factory().instantiate(view, vocbase, viewJson->slice(), 0).ok()));
      REQUIRE((false == !view));
      auto* viewImpl = dynamic_cast<arangodb::iresearch::IResearchView*>(view.get());
      REQUIRE((nullptr != viewImpl));
      std::set<TRI_voc_cid_t> cids;
      viewImpl->visitCollections([&cids](TRI_voc_cid_t cid)->bool { cids.emplace(cid); return true; });
      CHECK((0 == cids.size()));

      for (auto& index: logicalCollection->getIndexes()) {
        auto* link = dynamic_cast<arangodb::iresearch::IResearchLink*>(index.get());
        REQUIRE((!link->self()->get())); // check that link is unregistred from view
      }
    }
  }
}

SECTION("test_self_token") {
  // test empty token
  {
    arangodb::iresearch::IResearchView::AsyncViewPtr::element_type empty(nullptr);
    CHECK((nullptr == empty.get()));
  }
}

SECTION("test_tracked_cids") {
  auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\", \"id\": 100 }");
  auto viewJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\", \"id\": 101 }");

  // test empty before open (TRI_vocbase_t::createView(...) will call open())
  {
    s.engine.views.clear();
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    arangodb::LogicalView::ptr view;
    CHECK((arangodb::iresearch::IResearchView::factory().create(view, vocbase, viewJson->slice()).ok()));
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
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    REQUIRE((nullptr != logicalCollection));
    arangodb::LogicalView::ptr logicalView;
    REQUIRE((arangodb::iresearch::IResearchView::factory().instantiate(logicalView, vocbase, viewJson->slice(), 0).ok()));
    REQUIRE((false == !logicalView));
    s.engine.createView(vocbase, logicalView->id(), *logicalView); // ensure link can find view
    StorageEngineMock(s.server).registerView(vocbase, logicalView); // ensure link can find view
    auto* viewImpl = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
    REQUIRE((nullptr != viewImpl));

    CHECK((viewImpl->properties(updateJson->slice(), false).ok()));

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
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    REQUIRE((nullptr != logicalCollection));
    arangodb::LogicalView::ptr logicalView;
    REQUIRE((arangodb::iresearch::IResearchView::factory().instantiate(logicalView, vocbase, viewJson->slice(), 0).ok()));
    REQUIRE((false == !logicalView));
    s.engine.createView(vocbase, logicalView->id(), *logicalView); // ensure link can find view
    StorageEngineMock(s.server).registerView(vocbase, logicalView); // ensure link can find view
    auto* viewImpl = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
    REQUIRE((nullptr != viewImpl));

    // create link
    {
      CHECK((viewImpl->properties(updateJson0->slice(), false).ok()));

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
      CHECK((viewImpl->properties(updateJson1->slice(), false).ok()));

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
      auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
      auto linkJson = arangodb::velocypack::Parser::fromJson("{ \"view\": \"testView\" }");
      auto createJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\", \"id\": 102 }");
      auto* feature = arangodb::application_features::ApplicationServer::lookupFeature<
        arangodb::FlushFeature
      >("Flush");
      REQUIRE(feature);
      Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
      auto logicalCollection = vocbase.createCollection(collectionJson->slice());
      REQUIRE((false == !logicalCollection));
      auto logicalView = vocbase.createView(createJson->slice());
      REQUIRE((false == !logicalView));
      auto* viewImpl = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
      REQUIRE((nullptr != viewImpl));
      std::shared_ptr<arangodb::Index> index;
      REQUIRE((arangodb::iresearch::IResearchMMFilesLink::factory().instantiate(index, *logicalCollection, linkJson->slice(), 42, false).ok()));
      REQUIRE((false == !index));
      auto link = std::dynamic_pointer_cast<arangodb::iresearch::IResearchLink>(index);
      REQUIRE((false == !link));

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
      CHECK((link->insert(trx, arangodb::LocalDocumentId(0), doc->slice(), arangodb::Index::OperationMode::normal).ok()));
      CHECK((trx.commit().ok()));
      feature->executeCallbacks(); // commit to persisted store
    }

    // test persisted CIDs on open
    {
      s.engine.views.clear();
      auto createJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\", \"id\": 102 }");
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
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    REQUIRE((nullptr != logicalCollection));
    auto logicalView = vocbase.createView(viewJson->slice());
    REQUIRE((false == !logicalView));
    auto* viewImpl = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
    REQUIRE((nullptr != viewImpl));

    CHECK((viewImpl->properties(updateJson->slice(), false).ok()));

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
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    REQUIRE((nullptr != logicalCollection));
    auto logicalView = vocbase.createView(viewJson->slice());
    REQUIRE((false == !logicalView));
    auto* viewImpl = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
    REQUIRE((nullptr != viewImpl));

    // create link
    {
      CHECK((viewImpl->properties(updateJson0->slice(), false).ok()));

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
      CHECK((viewImpl->properties(updateJson1->slice(), false).ok()));

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
  auto logicalCollection0 = vocbase.createCollection(collectionJson0->slice());
  REQUIRE((nullptr != logicalCollection0));
  auto logicalCollection1 = vocbase.createCollection(collectionJson1->slice());
  REQUIRE((nullptr != logicalCollection1));
  auto logicalView = vocbase.createView(viewJson->slice());
  REQUIRE((false == !logicalView));
  auto* viewImpl = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
  REQUIRE((nullptr != viewImpl));

  // link collection to view
  {
    auto updateJson = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection0\": {}, \"testCollection1\": {} } }");
    CHECK((viewImpl->properties(updateJson->slice(), false).ok()));
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
  auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
  auto linkJson = arangodb::velocypack::Parser::fromJson("{ \"view\": \"testView\", \"includeAllFields\": true }");
  auto viewJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\", \"commitIntervalMsec\": 0 }");
  Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
  auto logicalCollection = vocbase.createCollection(collectionJson->slice());
  REQUIRE((false == !logicalCollection));
  auto logicalView = vocbase.createView(viewJson->slice());
  REQUIRE((false == !logicalView));
  auto* viewImpl = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
  REQUIRE((nullptr != viewImpl));
  std::shared_ptr<arangodb::Index> index;
  REQUIRE((arangodb::iresearch::IResearchMMFilesLink::factory().instantiate(index, *logicalCollection, linkJson->slice(), 42, false).ok()));
  REQUIRE((false == !index));
  auto link = std::dynamic_pointer_cast<arangodb::iresearch::IResearchLink>(index);
  REQUIRE((false == !link));

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
    CHECK((link->insert(trx, arangodb::LocalDocumentId(0), doc->slice(), arangodb::Index::OperationMode::normal).ok()));
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
    CHECK(nullptr == viewImpl->snapshot(trx, arangodb::iresearch::IResearchView::Snapshot::Find));
    auto* snapshot = viewImpl->snapshot(trx, arangodb::iresearch::IResearchView::Snapshot::FindOrCreate);
    CHECK(snapshot == viewImpl->snapshot(trx, arangodb::iresearch::IResearchView::Snapshot::Find));
    CHECK(snapshot == viewImpl->snapshot(trx, arangodb::iresearch::IResearchView::Snapshot::FindOrCreate));
    CHECK(nullptr != snapshot);
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
    arangodb::transaction::Methods trx(
      arangodb::transaction::StandaloneContext::Create(vocbase),
      EMPTY,
      EMPTY,
      EMPTY,
      opts
    );
    CHECK(nullptr == viewImpl->snapshot(trx, arangodb::iresearch::IResearchView::Snapshot::Find));
    auto* snapshot = viewImpl->snapshot(trx, arangodb::iresearch::IResearchView::Snapshot::SyncAndReplace);
    CHECK(snapshot == viewImpl->snapshot(trx, arangodb::iresearch::IResearchView::Snapshot::Find));
    CHECK(snapshot == viewImpl->snapshot(trx, arangodb::iresearch::IResearchView::Snapshot::FindOrCreate));
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
    CHECK((link->insert(trx, arangodb::LocalDocumentId(1), doc->slice(), arangodb::Index::OperationMode::normal).ok()));
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
    auto* snapshot = viewImpl->snapshot(trx, arangodb::iresearch::IResearchView::Snapshot::FindOrCreate);
    CHECK(snapshot == viewImpl->snapshot(trx, arangodb::iresearch::IResearchView::Snapshot::Find));
    CHECK(snapshot == viewImpl->snapshot(trx, arangodb::iresearch::IResearchView::Snapshot::FindOrCreate));
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
    auto* snapshot = viewImpl->snapshot(trx, arangodb::iresearch::IResearchView::Snapshot::FindOrCreate);
    CHECK(snapshot == viewImpl->snapshot(trx, arangodb::iresearch::IResearchView::Snapshot::Find));
    CHECK((nullptr != snapshot));
    CHECK((1 == snapshot->live_docs_count()));
    CHECK(true == trx.abort().ok()); // prevent assertion in destructor
  }

  // old snapshot in TransactionState (force == true, waitForSync = true during updateStatus(), false during snapshot())
  {
    arangodb::transaction::Options opts;
    arangodb::transaction::Methods trx(
      arangodb::transaction::StandaloneContext::Create(vocbase),
      EMPTY,
      EMPTY,
      EMPTY,
      opts
    );
    auto state = trx.state();
    REQUIRE(state);
    CHECK((true == viewImpl->apply(trx)));
    CHECK((true == trx.begin().ok()));
    auto* snapshot = viewImpl->snapshot(trx, arangodb::iresearch::IResearchView::Snapshot::SyncAndReplace);
    CHECK(snapshot == viewImpl->snapshot(trx, arangodb::iresearch::IResearchView::Snapshot::Find));
    CHECK((nullptr != snapshot));
    CHECK((2 == snapshot->live_docs_count()));
    CHECK(true == trx.abort().ok()); // prevent assertion in destructor
  }
}

SECTION("test_update_overwrite") {
  auto createJson = arangodb::velocypack::Parser::fromJson("{ \
    \"name\": \"testView\", \
    \"type\": \"arangosearch\", \
    \"cleanupIntervalStep\": 52 \
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
        \"cleanupIntervalStep\": 42 \
      }");

      expectedMeta._cleanupIntervalStep = 42;
      CHECK((view->properties(updateJson->slice(), false).ok()));

      // not for persistence
      {
        arangodb::velocypack::Builder builder;

        builder.openObject();
        view->properties(builder, true, false);
        builder.close();

        auto slice = builder.slice();
        arangodb::iresearch::IResearchViewMeta meta;
        std::string error;

        CHECK(slice.isObject());
        CHECK((11U == slice.length()));
        CHECK((slice.hasKey("globallyUniqueId") && slice.get("globallyUniqueId").isString() && false == slice.get("globallyUniqueId").copyString().empty()));
        CHECK(slice.get("name").copyString() == "testView");
        CHECK(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
        CHECK(slice.get("deleted").isNone()); // no system properties
        CHECK((meta.init(slice, error) && expectedMeta == meta));
        auto tmpSlice = slice.get("links");
        CHECK((true == tmpSlice.isObject() && 0 == tmpSlice.length()));
      }

      // for persistence
      {
        arangodb::velocypack::Builder builder;

        builder.openObject();
        view->properties(builder, true, true);
        builder.close();

        auto slice = builder.slice();
        arangodb::iresearch::IResearchViewMeta meta;
        arangodb::iresearch::IResearchViewMetaState metaState;
        std::string error;

        CHECK(slice.isObject());
        CHECK((15U == slice.length()));
        CHECK(slice.get("name").copyString() == "testView");
        CHECK(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
        CHECK((slice.hasKey("deleted") && slice.get("deleted").isBoolean() && false == slice.get("deleted").getBoolean())); // has system properties
        CHECK((meta.init(slice, error) && expectedMeta == meta));
        CHECK((true == metaState.init(slice, error) && expectedMetaState == metaState));
        CHECK((false == slice.hasKey("links")));
      }
    }

    // subsequent update (overwrite)
    {
      arangodb::iresearch::IResearchViewMeta expectedMeta;
      arangodb::iresearch::IResearchViewMetaState expectedMetaState;
      auto updateJson = arangodb::velocypack::Parser::fromJson("{ \
        \"cleanupIntervalStep\": 62 \
      }");

      expectedMeta._cleanupIntervalStep = 62;
      CHECK((view->properties(updateJson->slice(), false).ok()));

      // not for persistence
      {
        arangodb::velocypack::Builder builder;

        builder.openObject();
        view->properties(builder, true, false);
        builder.close();

        auto slice = builder.slice();
        arangodb::iresearch::IResearchViewMeta meta;
        arangodb::iresearch::IResearchViewMetaState metaState;
        std::string error;

        CHECK(slice.isObject());
        CHECK((11U == slice.length()));
        CHECK((slice.hasKey("globallyUniqueId") && slice.get("globallyUniqueId").isString() && false == slice.get("globallyUniqueId").copyString().empty()));
        CHECK(slice.get("name").copyString() == "testView");
        CHECK(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
        CHECK(slice.get("deleted").isNone()); // no system properties
        CHECK((meta.init(slice, error) && expectedMeta == meta));
        auto tmpSlice = slice.get("links");
        CHECK((true == tmpSlice.isObject() && 0 == tmpSlice.length()));
      }

      // for persistence
      {
        arangodb::velocypack::Builder builder;

        builder.openObject();
        view->properties(builder, true, true);
        builder.close();

        auto slice = builder.slice();
        arangodb::iresearch::IResearchViewMeta meta;
        arangodb::iresearch::IResearchViewMetaState metaState;
        std::string error;

        CHECK(slice.isObject());
        CHECK((15U == slice.length()));
        CHECK(slice.get("name").copyString() == "testView");
        CHECK(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
        CHECK((slice.hasKey("deleted") && slice.get("deleted").isBoolean() && false == slice.get("deleted").getBoolean())); // has system properties
        CHECK((meta.init(slice, error) && expectedMeta == meta));
        CHECK((true == metaState.init(slice, error) && expectedMetaState == metaState));
        CHECK((false == slice.hasKey("links")));
      }
    }
  }

  // test rollback on meta modification failure (as an example invalid value for 'cleanupIntervalStep')
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalView = vocbase.createView(createJson->slice());
    REQUIRE((false == !logicalView));
    REQUIRE((logicalView->category() == arangodb::LogicalView::category()));

    arangodb::iresearch::IResearchViewMeta expectedMeta;
    arangodb::iresearch::IResearchViewMetaState expectedMetaState;
    auto updateJson = arangodb::velocypack::Parser::fromJson("{ \"cleanupIntervalStep\": 0.123 }");

    expectedMeta._cleanupIntervalStep = 52;
    CHECK((TRI_ERROR_BAD_PARAMETER == logicalView->properties(updateJson->slice(), false).errorNumber()));

    // not for persistence
    {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      logicalView->properties(builder, true, false);
      builder.close();

      auto slice = builder.slice();
      arangodb::iresearch::IResearchViewMeta meta;
      arangodb::iresearch::IResearchViewMetaState metaState;
      std::string error;

      CHECK((slice.isObject()));
      CHECK((11U == slice.length()));
      CHECK((slice.hasKey("globallyUniqueId") && slice.get("globallyUniqueId").isString() && false == slice.get("globallyUniqueId").copyString().empty()));
      CHECK((slice.get("name").copyString() == "testView"));
      CHECK((slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name()));
      CHECK((slice.get("deleted").isNone())); // no system properties
      CHECK((meta.init(slice, error) && expectedMeta == meta));
      auto tmpSlice = slice.get("links");
      CHECK((true == tmpSlice.isObject() && 0 == tmpSlice.length()));
    }

    // for persistence
    {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      logicalView->properties(builder, true, true);
      builder.close();

      auto slice = builder.slice();
      arangodb::iresearch::IResearchViewMeta meta;
      arangodb::iresearch::IResearchViewMetaState metaState;
      std::string error;

      CHECK((slice.isObject()));
      CHECK((15U == slice.length()));
      CHECK((slice.get("name").copyString() == "testView"));
      CHECK((slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name()));
      CHECK((slice.hasKey("deleted") && slice.get("deleted").isBoolean() && false == slice.get("deleted").getBoolean())); // has system properties
      CHECK((meta.init(slice, error) && expectedMeta == meta));
      CHECK((true == metaState.init(slice, error) && expectedMetaState == metaState));
      CHECK((false == slice.hasKey("links")));
    }
  }

  // modify meta params with links to missing collections
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalView = vocbase.createView(createJson->slice());
    REQUIRE((false == !logicalView));
    REQUIRE((logicalView->category() == arangodb::LogicalView::category()));

    arangodb::iresearch::IResearchViewMeta expectedMeta;
    arangodb::iresearch::IResearchViewMetaState expectedMetaState;
    auto updateJson = arangodb::velocypack::Parser::fromJson("{ \"cleanupIntervalStep\": 62, \"links\": { \"testCollection\": {} } }");

    expectedMeta._cleanupIntervalStep = 52;
    CHECK((TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND == logicalView->properties(updateJson->slice(), false).errorNumber()));
    CHECK((true == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));

    // not for persistence
    {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      logicalView->properties(builder, true, false);
      builder.close();

      auto slice = builder.slice();
      arangodb::iresearch::IResearchViewMeta meta;
      arangodb::iresearch::IResearchViewMetaState metaState;
      std::string error;

      CHECK((slice.isObject()));
      CHECK((11U == slice.length()));
      CHECK((slice.hasKey("globallyUniqueId") && slice.get("globallyUniqueId").isString() && false == slice.get("globallyUniqueId").copyString().empty()));
      CHECK((slice.get("name").copyString() == "testView"));
      CHECK((slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name()));
      CHECK((slice.get("deleted").isNone())); // no system properties
      CHECK((meta.init(slice, error) && expectedMeta == meta));
      auto tmpSlice = slice.get("links");
      CHECK((true == tmpSlice.isObject() && 0 == tmpSlice.length()));
    }

    // for persistence
    {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      logicalView->properties(builder, true, true);
      builder.close();

      auto slice = builder.slice();
      arangodb::iresearch::IResearchViewMeta meta;
      arangodb::iresearch::IResearchViewMetaState metaState;
      std::string error;

      CHECK((slice.isObject()));
      CHECK((15U == slice.length()));
      CHECK((slice.get("name").copyString() == "testView"));
      CHECK((slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name()));
      CHECK((slice.hasKey("deleted") && slice.get("deleted").isBoolean() && false == slice.get("deleted").getBoolean())); // has system properties
      CHECK((meta.init(slice, error) && expectedMeta == meta));
      CHECK((true == metaState.init(slice, error) && expectedMetaState == metaState));
      CHECK((false == slice.hasKey("links")));
    }
  }

  // modify meta params with links with invalid definition
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    REQUIRE((nullptr != logicalCollection));
    auto logicalView = vocbase.createView(createJson->slice());
    REQUIRE((false == !logicalView));
    REQUIRE((logicalView->category() == arangodb::LogicalView::category()));
    CHECK((true == logicalCollection->getIndexes().empty()));

    arangodb::iresearch::IResearchViewMeta expectedMeta;
    arangodb::iresearch::IResearchViewMetaState expectedMetaState;
    auto updateJson = arangodb::velocypack::Parser::fromJson("{ \"cleanupIntervalStep\": 62, \"links\": { \"testCollection\": 42 } }");

    expectedMeta._cleanupIntervalStep = 52;
    CHECK((TRI_ERROR_BAD_PARAMETER == logicalView->properties(updateJson->slice(), false).errorNumber()));
    CHECK((true == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));

    // not for persistence
    {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      logicalView->properties(builder, true, false);
      builder.close();

      auto slice = builder.slice();
      arangodb::iresearch::IResearchViewMeta meta;
      arangodb::iresearch::IResearchViewMetaState metaState;
      std::string error;

      CHECK((slice.isObject()));
      CHECK((11U == slice.length()));
      CHECK((slice.hasKey("globallyUniqueId") && slice.get("globallyUniqueId").isString() && false == slice.get("globallyUniqueId").copyString().empty()));
      CHECK((slice.get("name").copyString() == "testView"));
      CHECK((slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name()));
      CHECK((slice.get("deleted").isNone())); // no system properties
      CHECK((meta.init(slice, error) && expectedMeta == meta));
      auto tmpSlice = slice.get("links");
      CHECK((true == tmpSlice.isObject() && 0 == tmpSlice.length()));
    }

    // for persistence
    {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      logicalView->properties(builder, true, true);
      builder.close();

      auto slice = builder.slice();
      arangodb::iresearch::IResearchViewMeta meta;
      arangodb::iresearch::IResearchViewMetaState metaState;
      std::string error;

      CHECK((slice.isObject()));
      CHECK((15U == slice.length()));
      CHECK((slice.get("name").copyString() == "testView"));
      CHECK((slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name()));
      CHECK((slice.hasKey("deleted") && slice.get("deleted").isBoolean() && false == slice.get("deleted").getBoolean())); // has system properties
      CHECK((meta.init(slice, error) && expectedMeta == meta));
      CHECK((true == metaState.init(slice, error) && expectedMetaState == metaState));
      CHECK((false == slice.hasKey("links")));
    }
  }

  // modify meta params with links
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    REQUIRE((nullptr != logicalCollection));
    auto logicalView = vocbase.createView(createJson->slice());
    REQUIRE((false == !logicalView));
    REQUIRE((logicalView->category() == arangodb::LogicalView::category()));
    CHECK((true == logicalCollection->getIndexes().empty()));

    // initial link creation
    {
      auto updateJson = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection\": {} } }");
      arangodb::iresearch::IResearchViewMeta expectedMeta;
      arangodb::iresearch::IResearchViewMetaState expectedMetaState;
      std::unordered_map<std::string, arangodb::iresearch::IResearchLinkMeta> expectedLinkMeta;

      expectedMeta._cleanupIntervalStep = 52;
      expectedMetaState._collections.insert(logicalCollection->id());
      expectedLinkMeta["testCollection"]; // use defaults
      CHECK((logicalView->properties(updateJson->slice(), true).ok()));

      // not for persistence
      {
        arangodb::velocypack::Builder builder;

        builder.openObject();
        logicalView->properties(builder, true, false);
        builder.close();

        auto slice = builder.slice();
        arangodb::iresearch::IResearchViewMeta meta;
        arangodb::iresearch::IResearchViewMetaState metaState;
        std::string error;

        CHECK((slice.isObject()));
        CHECK((11U == slice.length()));
        CHECK((slice.hasKey("globallyUniqueId") && slice.get("globallyUniqueId").isString() && false == slice.get("globallyUniqueId").copyString().empty()));
        CHECK((slice.get("name").copyString() == "testView"));
        CHECK((slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name()));
        CHECK((slice.get("deleted").isNone())); // no system properties
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
      }

      // for persistence
      {
        arangodb::velocypack::Builder builder;

        builder.openObject();
        logicalView->properties(builder, true, true);
        builder.close();

        auto slice = builder.slice();
        arangodb::iresearch::IResearchViewMeta meta;
        arangodb::iresearch::IResearchViewMetaState metaState;
        std::string error;

        CHECK((slice.isObject()));
        CHECK((15U == slice.length()));
        CHECK((slice.get("name").copyString() == "testView"));
        CHECK((slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name()));
        CHECK((slice.hasKey("deleted") && slice.get("deleted").isBoolean() && false == slice.get("deleted").getBoolean())); // has system properties
        CHECK((meta.init(slice, error) && expectedMeta == meta));
        CHECK((true == metaState.init(slice, error) && expectedMetaState == metaState));
        CHECK((false == slice.hasKey("links")));
      }

      CHECK((true == expectedLinkMeta.empty()));
      CHECK((false == logicalCollection->getIndexes().empty()));
    }

    // subsequent update (overwrite)
    {
      arangodb::iresearch::IResearchViewMeta expectedMeta;
      arangodb::iresearch::IResearchViewMetaState expectedMetaState;
      auto updateJson = arangodb::velocypack::Parser::fromJson("{ \
        \"cleanupIntervalStep\": 62 \
      }");

      expectedMeta._cleanupIntervalStep = 62;
      CHECK((logicalView->properties(updateJson->slice(), false).ok()));

      // not for persistence
      {
        arangodb::velocypack::Builder builder;

        builder.openObject();
        logicalView->properties(builder, true, false);
        builder.close();

        auto slice = builder.slice();
        arangodb::iresearch::IResearchViewMeta meta;
        arangodb::iresearch::IResearchViewMetaState metaState;
        std::string error;

        CHECK((slice.isObject()));
        CHECK((11U == slice.length()));
        CHECK((slice.hasKey("globallyUniqueId") && slice.get("globallyUniqueId").isString() && false == slice.get("globallyUniqueId").copyString().empty()));
        CHECK((slice.get("name").copyString() == "testView"));
        CHECK((slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name()));
        CHECK((slice.get("deleted").isNone())); // no system properties
        CHECK((meta.init(slice, error) && expectedMeta == meta));
        auto tmpSlice = slice.get("links");
        CHECK((true == tmpSlice.isObject() && 0 == tmpSlice.length()));
      }

      // for persistence
      {
        arangodb::velocypack::Builder builder;

        builder.openObject();
        logicalView->properties(builder, true, true);
        builder.close();

        auto slice = builder.slice();
        arangodb::iresearch::IResearchViewMeta meta;
        arangodb::iresearch::IResearchViewMetaState metaState;
        std::string error;

        CHECK((slice.isObject()));
        CHECK((15U == slice.length()));
        CHECK((slice.get("name").copyString() == "testView"));
        CHECK((slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name()));
        CHECK((slice.hasKey("deleted") && slice.get("deleted").isBoolean() && false == slice.get("deleted").getBoolean())); // has system properties
        CHECK((meta.init(slice, error) && expectedMeta == meta));
        CHECK((true == metaState.init(slice, error) && expectedMetaState == metaState));
        CHECK((false == slice.hasKey("links")));
      }
    }
  }

  // overwrite links
  {
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto collectionJson0 = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection0\" }");
    auto collectionJson1 = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection1\" }");
    auto logicalCollection0 = vocbase.createCollection(collectionJson0->slice());
    REQUIRE((nullptr != logicalCollection0));
    auto logicalCollection1 = vocbase.createCollection(collectionJson1->slice());
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

      expectedMeta._cleanupIntervalStep = 52;
      expectedMetaState._collections.insert(logicalCollection0->id());
      expectedLinkMeta["testCollection0"]; // use defaults
      CHECK((view->properties(updateJson->slice(), true).ok()));

      // not for persistence
      {
        arangodb::velocypack::Builder builder;

        builder.openObject();
        view->properties(builder, true, false);
        builder.close();

        auto slice = builder.slice();
        arangodb::iresearch::IResearchViewMeta meta;
        arangodb::iresearch::IResearchViewMetaState metaState;
        std::string error;

        CHECK(slice.isObject());
        CHECK((11U == slice.length()));
        CHECK((slice.hasKey("globallyUniqueId") && slice.get("globallyUniqueId").isString() && false == slice.get("globallyUniqueId").copyString().empty()));
        CHECK(slice.get("name").copyString() == "testView");
        CHECK(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
        CHECK(slice.get("deleted").isNone()); // no system properties
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
      }

      // for persistence
      {
        arangodb::velocypack::Builder builder;

        builder.openObject();
        view->properties(builder, true, true);
        builder.close();

        auto slice = builder.slice();
        arangodb::iresearch::IResearchViewMeta meta;
        arangodb::iresearch::IResearchViewMetaState metaState;
        std::string error;

        CHECK(slice.isObject());
        CHECK((15U == slice.length()));
        CHECK(slice.get("name").copyString() == "testView");
        CHECK(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
        CHECK((slice.hasKey("deleted") && slice.get("deleted").isBoolean() && false == slice.get("deleted").getBoolean())); // has system properties
        CHECK((meta.init(slice, error) && expectedMeta == meta));
        CHECK((true == metaState.init(slice, error) && expectedMetaState == metaState));
        CHECK((false == slice.hasKey("links")));
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

      expectedMeta._cleanupIntervalStep = 10;
      expectedMetaState._collections.insert(logicalCollection1->id());
      expectedLinkMeta["testCollection1"]; // use defaults
      CHECK((view->properties(updateJson->slice(), false).ok()));

      // not for persistence
      {
        arangodb::velocypack::Builder builder;

        builder.openObject();
        view->properties(builder, true, false);
        builder.close();

        auto slice = builder.slice();
        arangodb::iresearch::IResearchViewMeta meta;
        arangodb::iresearch::IResearchViewMetaState metaState;
        std::string error;

        CHECK(slice.isObject());
        CHECK((11U == slice.length()));
        CHECK((slice.hasKey("globallyUniqueId") && slice.get("globallyUniqueId").isString() && false == slice.get("globallyUniqueId").copyString().empty()));
        CHECK(slice.get("name").copyString() == "testView");
        CHECK(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
        CHECK(slice.get("deleted").isNone()); // no system properties
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
      }

      // for persistence
      {
        arangodb::velocypack::Builder builder;

        builder.openObject();
        view->properties(builder, true, true);
        builder.close();

        auto slice = builder.slice();
        arangodb::iresearch::IResearchViewMeta meta;
        arangodb::iresearch::IResearchViewMetaState metaState;
        std::string error;

        CHECK(slice.isObject());
        CHECK((15U == slice.length()));
        CHECK(slice.get("name").copyString() == "testView");
        CHECK(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
        CHECK((slice.hasKey("deleted") && slice.get("deleted").isBoolean() && false == slice.get("deleted").getBoolean())); // has system properties
        CHECK((meta.init(slice, error) && expectedMeta == meta));
        CHECK((true == metaState.init(slice, error) && expectedMetaState == metaState));
        CHECK((false == slice.hasKey("links")));
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
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    REQUIRE((nullptr != logicalCollection));
    auto view = vocbase.createView(createJson->slice());
    REQUIRE((false == !view));
    REQUIRE(view->category() == arangodb::LogicalView::category());

    // initial add of link
    {
      auto updateJson = arangodb::velocypack::Parser::fromJson(
        "{ \"links\": { \"testCollection\": { \"includeAllFields\": true } } }"
      );
      CHECK((view->properties(updateJson->slice(), true).ok()));

      // not for persistence
      {
        arangodb::velocypack::Builder builder;

        builder.openObject();
        view->properties(builder, true, false);
        builder.close();

        auto slice = builder.slice();
        CHECK(slice.isObject());
        CHECK((11U == slice.length()));
        CHECK((slice.hasKey("globallyUniqueId") && slice.get("globallyUniqueId").isString() && false == slice.get("globallyUniqueId").copyString().empty()));
        CHECK(slice.get("name").copyString() == "testView");
        CHECK(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
        CHECK(slice.get("deleted").isNone()); // no system properties
        auto tmpSlice = slice.get("links");
        CHECK((true == tmpSlice.isObject() && 1 == tmpSlice.length()));
        tmpSlice = tmpSlice.get("testCollection");
        CHECK((true == tmpSlice.isObject()));
        tmpSlice = tmpSlice.get("includeAllFields");
        CHECK((true == tmpSlice.isBoolean() && true == tmpSlice.getBoolean()));
      }

      // for persistence
      {
        arangodb::velocypack::Builder builder;

        builder.openObject();
        view->properties(builder, true, true);
        builder.close();

        auto slice = builder.slice();
        CHECK(slice.isObject());
        CHECK((15U == slice.length()));
        CHECK(slice.get("name").copyString() == "testView");
        CHECK(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
        CHECK((slice.hasKey("deleted") && slice.get("deleted").isBoolean() && false == slice.get("deleted").getBoolean())); // has system properties
        auto tmpSlice = slice.get("collections");
        CHECK((true == tmpSlice.isArray() && 1 == tmpSlice.length()));
        CHECK((false == slice.hasKey("links")));
      }
    }

    // update link
    {
      auto updateJson = arangodb::velocypack::Parser::fromJson(
        "{ \"links\": { \"testCollection\": { } } }"
      );
      CHECK((view->properties(updateJson->slice(), false).ok()));

      // not for persistence
      {
        arangodb::velocypack::Builder builder;

        builder.openObject();
        view->properties(builder, true, false);
        builder.close();

        auto slice = builder.slice();
        CHECK((11U == slice.length()));
        CHECK((slice.hasKey("globallyUniqueId") && slice.get("globallyUniqueId").isString() && false == slice.get("globallyUniqueId").copyString().empty()));
        CHECK(slice.get("name").copyString() == "testView");
        CHECK(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
        CHECK(slice.get("deleted").isNone()); // no system properties
        auto tmpSlice = slice.get("links");
        CHECK((true == tmpSlice.isObject() && 1 == tmpSlice.length()));
        tmpSlice = tmpSlice.get("testCollection");
        CHECK((true == tmpSlice.isObject()));
        tmpSlice = tmpSlice.get("includeAllFields");
        CHECK((true == tmpSlice.isBoolean() && false == tmpSlice.getBoolean()));
      }

      // for persistence
      {
        arangodb::velocypack::Builder builder;

        builder.openObject();
        view->properties(builder, true, true);
        builder.close();

        auto slice = builder.slice();
        CHECK((15U == slice.length()));
        CHECK(slice.get("name").copyString() == "testView");
        CHECK(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
        CHECK((slice.hasKey("deleted") && slice.get("deleted").isBoolean() && false == slice.get("deleted").getBoolean())); // has system properties
        auto tmpSlice = slice.get("collections");
        CHECK((true == tmpSlice.isArray() && 1 == tmpSlice.length()));
        CHECK((false == slice.hasKey("links")));
      }
    }
  }

  // modify meta params with links (collection not authorized)
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\", \"id\": 100 }");
    auto viewCreateJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
    auto viewUpdateJson = arangodb::velocypack::Parser::fromJson("{ \"cleanupIntervalStep\": 62, \"links\": { \"testCollection\": {} } }");

    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    REQUIRE((nullptr != logicalCollection));
    auto logicalView = vocbase.createView(viewCreateJson->slice());
    REQUIRE((false == !logicalView));

    CHECK((true == logicalCollection->getIndexes().empty()));
    CHECK((true == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));

    // initial link creation
    {
      auto updateJson = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection\": {} } }");
      CHECK((logicalView->properties(updateJson->slice(), true).ok()));
      CHECK((false == logicalCollection->getIndexes().empty()));
      CHECK((false == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));
    }

    struct ExecContext: public arangodb::ExecContext {
      ExecContext(): arangodb::ExecContext(arangodb::ExecContext::Type::Default, "", "",
                                           arangodb::auth::Level::NONE, arangodb::auth::Level::NONE) {}
    } execContext;
    arangodb::ExecContextScope execContextScope(&execContext);
    auto* authFeature = arangodb::AuthenticationFeature::instance();
    auto* userManager = authFeature->userManager();
    arangodb::aql::QueryRegistry queryRegistry(0); // required for UserManager::loadFromDB()
    userManager->setQueryRegistry(&queryRegistry);
    auto resetUserManager = std::shared_ptr<arangodb::auth::UserManager>(userManager, [](arangodb::auth::UserManager* ptr)->void { ptr->removeAllUsers(); });

    // subsequent update (overwrite) not authorised (NONE collection)
    {
      arangodb::auth::UserMap userMap;
      auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
      user.grantCollection(vocbase.name(), "testCollection", arangodb::auth::Level::NONE); // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

      arangodb::iresearch::IResearchViewMeta expectedMeta;
      expectedMeta._cleanupIntervalStep = 10;

      CHECK((TRI_ERROR_FORBIDDEN == logicalView->properties(viewUpdateJson->slice(), false).errorNumber()));

      arangodb::velocypack::Builder builder;
      builder.openObject();
      logicalView->properties(builder, true, true); // 'forPersistence' to avoid auth check
      builder.close();

      auto slice = builder.slice();
      arangodb::iresearch::IResearchViewMeta meta;
      std::string error;
      CHECK((meta.init(slice, error) && expectedMeta == meta));
    }

    // subsequent update (overwrite) authorised (RO collection)
    {
      arangodb::auth::UserMap userMap;
      auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
      user.grantCollection(vocbase.name(), "testCollection", arangodb::auth::Level::RO); // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

      arangodb::iresearch::IResearchViewMeta expectedMeta;
      expectedMeta._cleanupIntervalStep = 62;

      CHECK((logicalView->properties(viewUpdateJson->slice(), false).ok()));

      arangodb::velocypack::Builder builder;
      builder.openObject();
      logicalView->properties(builder, true, true); // 'forPersistence' to avoid auth check
      builder.close();

      auto slice = builder.slice();
      arangodb::iresearch::IResearchViewMeta meta;
      std::string error;
      CHECK((meta.init(slice, error) && expectedMeta == meta));
    }
  }

  // add link (collection not authorized)
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\", \"id\": 100 }");
    auto viewCreateJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
    auto viewUpdateJson = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection\": {} } }");

    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    REQUIRE((nullptr != logicalCollection));
    auto logicalView = vocbase.createView(viewCreateJson->slice());
    REQUIRE((false == !logicalView));

    CHECK((true == logicalCollection->getIndexes().empty()));
    CHECK((true == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));

    struct ExecContext: public arangodb::ExecContext {
      ExecContext(): arangodb::ExecContext(arangodb::ExecContext::Type::Default, "", "",
                                           arangodb::auth::Level::NONE, arangodb::auth::Level::NONE) {}
    } execContext;
    auto* origExecContext = ExecContext::CURRENT;
    auto resetExecContext = irs::make_finally([origExecContext]()->void{ ExecContext::CURRENT = origExecContext; });
    ExecContext::CURRENT = &execContext;
    auto* authFeature = arangodb::AuthenticationFeature::instance();
    auto* userManager = authFeature->userManager();
    arangodb::auth::UserMap userMap; // empty map, no user -> no permissions
    userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database
    auto resetUserManager = irs::make_finally([userManager]()->void{ userManager->removeAllUsers(); });

    CHECK((TRI_ERROR_FORBIDDEN == logicalView->properties(viewUpdateJson->slice(), false).errorNumber()));
    CHECK((true == logicalCollection->getIndexes().empty()));
    CHECK((true == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));
  }

  // drop link (collection not authorized)
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\", \"id\": 100 }");
    auto viewCreateJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
    auto viewUpdateJson = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection\": null } }");

    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    REQUIRE((nullptr != logicalCollection));
    auto logicalView = vocbase.createView(viewCreateJson->slice());
    REQUIRE((false == !logicalView));

    CHECK((true == logicalCollection->getIndexes().empty()));
    CHECK((true == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));

    // initial link creation
    {
      auto updateJson = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection\": {} } }");
      CHECK((logicalView->properties(updateJson->slice(), true).ok()));
      CHECK((false == logicalCollection->getIndexes().empty()));
      CHECK((false == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));
    }

    struct ExecContext: public arangodb::ExecContext {
      ExecContext(): arangodb::ExecContext(arangodb::ExecContext::Type::Default, "", "",
                                           arangodb::auth::Level::NONE, arangodb::auth::Level::NONE) {}
    } execContext;
    arangodb::ExecContextScope execContextScope(&execContext);
    auto* authFeature = arangodb::AuthenticationFeature::instance();
    auto* userManager = authFeature->userManager();
    arangodb::aql::QueryRegistry queryRegistry(0); // required for UserManager::loadFromDB()
    userManager->setQueryRegistry(&queryRegistry);
    auto resetUserManager = std::shared_ptr<arangodb::auth::UserManager>(userManager, [](arangodb::auth::UserManager* ptr)->void { ptr->removeAllUsers(); });

    // subsequent update (overwrite) not authorised (NONE collection)
    {
      arangodb::auth::UserMap userMap;
      auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
      user.grantCollection(vocbase.name(), "testCollection", arangodb::auth::Level::NONE); // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

      CHECK((TRI_ERROR_FORBIDDEN == logicalView->properties(viewUpdateJson->slice(), false).errorNumber()));
      CHECK((false == logicalCollection->getIndexes().empty()));
      CHECK((false == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));
    }

    // subsequent update (overwrite) authorised (RO collection)
    {
      arangodb::auth::UserMap userMap;
      auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
      user.grantCollection(vocbase.name(), "testCollection", arangodb::auth::Level::RO); // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

      CHECK((logicalView->properties(viewUpdateJson->slice(), false).ok()));
      CHECK((true == logicalCollection->getIndexes().empty()));
      CHECK((true == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));
    }
  }

  // add authorised link (existing collection not authorized)
  {
    auto collection0Json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection0\", \"id\": 100 }");
    auto collection1Json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection1\", \"id\": 101 }");
    auto viewCreateJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
    auto viewUpdateJson = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection0\": {}, \"testCollection1\": {} } }");

    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalCollection0 = vocbase.createCollection(collection0Json->slice());
    REQUIRE((nullptr != logicalCollection0));
    auto logicalCollection1 = vocbase.createCollection(collection1Json->slice());
    REQUIRE((nullptr != logicalCollection1));
    auto logicalView = vocbase.createView(viewCreateJson->slice());
    REQUIRE((false == !logicalView));

    CHECK((true == logicalCollection0->getIndexes().empty()));
    CHECK((true == logicalCollection1->getIndexes().empty()));
    CHECK((true == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));

    // initial link creation
    {
      auto updateJson = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection0\": {} } }");
      CHECK((logicalView->properties(updateJson->slice(), true).ok()));
      CHECK((false == logicalCollection0->getIndexes().empty()));
      CHECK((true == logicalCollection1->getIndexes().empty()));
      CHECK((false == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));
    }

    struct ExecContext: public arangodb::ExecContext {
      ExecContext(): arangodb::ExecContext(arangodb::ExecContext::Type::Default, "", "",
                                           arangodb::auth::Level::NONE, arangodb::auth::Level::NONE) {}
    } execContext;
    arangodb::ExecContextScope execContextScope(&execContext);
    auto* authFeature = arangodb::AuthenticationFeature::instance();
    auto* userManager = authFeature->userManager();
    arangodb::aql::QueryRegistry queryRegistry(0); // required for UserManager::loadFromDB()
    userManager->setQueryRegistry(&queryRegistry);
    auto resetUserManager = std::shared_ptr<arangodb::auth::UserManager>(userManager, [](arangodb::auth::UserManager* ptr)->void { ptr->removeAllUsers(); });

    // subsequent update (overwrite) not authorised (NONE collection)
    {
      arangodb::auth::UserMap userMap;
      auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
      user.grantCollection(vocbase.name(), "testCollection0", arangodb::auth::Level::NONE); // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      user.grantCollection(vocbase.name(), "testCollection1", arangodb::auth::Level::RO); // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

      CHECK((TRI_ERROR_FORBIDDEN == logicalView->properties(viewUpdateJson->slice(), false).errorNumber()));
      CHECK((false == logicalCollection0->getIndexes().empty()));
      CHECK((true == logicalCollection1->getIndexes().empty()));
      CHECK((false == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));
    }

    // subsequent update (overwrite) authorised (RO collection)
    {
      arangodb::auth::UserMap userMap;
      auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
      user.grantCollection(vocbase.name(), "testCollection0", arangodb::auth::Level::RO); // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      user.grantCollection(vocbase.name(), "testCollection1", arangodb::auth::Level::RO); // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

      CHECK((logicalView->properties(viewUpdateJson->slice(), false).ok()));
      CHECK((false == logicalCollection0->getIndexes().empty()));
      CHECK((false == logicalCollection1->getIndexes().empty()));
      CHECK((false == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));
    }
  }

  // drop authorised link (existing collection not authorized)
  {
    auto collection0Json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection0\", \"id\": 100 }");
    auto collection1Json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection1\", \"id\": 101 }");
    auto viewCreateJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
    auto viewUpdateJson = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection0\": {} } }");

    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalCollection0 = vocbase.createCollection(collection0Json->slice());
    REQUIRE((nullptr != logicalCollection0));
    auto logicalCollection1 = vocbase.createCollection(collection1Json->slice());
    REQUIRE((nullptr != logicalCollection1));
    auto logicalView = vocbase.createView(viewCreateJson->slice());
    REQUIRE((false == !logicalView));

    CHECK((true == logicalCollection0->getIndexes().empty()));
    CHECK((true == logicalCollection1->getIndexes().empty()));
    CHECK((true == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));

    // initial link creation
    {
      auto updateJson = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection0\": {}, \"testCollection1\": {} } }");
      CHECK((logicalView->properties(updateJson->slice(), true).ok()));
      CHECK((false == logicalCollection0->getIndexes().empty()));
      CHECK((false == logicalCollection1->getIndexes().empty()));
      CHECK((false == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));
    }

    struct ExecContext: public arangodb::ExecContext {
      ExecContext(): arangodb::ExecContext(arangodb::ExecContext::Type::Default, "", "",
                                           arangodb::auth::Level::NONE, arangodb::auth::Level::NONE) {}
    } execContext;
    arangodb::ExecContextScope execContextScope(&execContext);
    auto* authFeature = arangodb::AuthenticationFeature::instance();
    auto* userManager = authFeature->userManager();
    arangodb::aql::QueryRegistry queryRegistry(0); // required for UserManager::loadFromDB()
    userManager->setQueryRegistry(&queryRegistry);
    auto resetUserManager = std::shared_ptr<arangodb::auth::UserManager>(userManager, [](arangodb::auth::UserManager* ptr)->void { ptr->removeAllUsers(); });

    // subsequent update (overwrite) not authorised (NONE collection)
    {
      arangodb::auth::UserMap userMap;
      auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
      user.grantCollection(vocbase.name(), "testCollection0", arangodb::auth::Level::NONE); // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      user.grantCollection(vocbase.name(), "testCollection1", arangodb::auth::Level::RO); // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

      CHECK((TRI_ERROR_FORBIDDEN == logicalView->properties(viewUpdateJson->slice(), false).errorNumber()));
      CHECK((false == logicalCollection0->getIndexes().empty()));
      CHECK((false == logicalCollection1->getIndexes().empty()));
      CHECK((false == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));
    }

    // subsequent update (overwrite) authorised (RO collection)
    {
      arangodb::auth::UserMap userMap;
      auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
      user.grantCollection(vocbase.name(), "testCollection0", arangodb::auth::Level::RO); // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      user.grantCollection(vocbase.name(), "testCollection1", arangodb::auth::Level::RO); // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

      CHECK((logicalView->properties(viewUpdateJson->slice(), false).ok()));
      CHECK((false == logicalCollection0->getIndexes().empty()));
      CHECK((true == logicalCollection1->getIndexes().empty()));
      CHECK((false == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));
    }
  }

  // drop link (collection not authorized)
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\", \"id\": 100 }");
    auto viewCreateJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
    auto viewUpdateJson = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection\": null } }");

    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    REQUIRE((nullptr != logicalCollection));
    auto logicalView = vocbase.createView(viewCreateJson->slice());
    REQUIRE((false == !logicalView));

    CHECK((true == logicalCollection->getIndexes().empty()));
    CHECK((true == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));

    // initial link creation
    {
      auto updateJson = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection\": {} } }");
      CHECK((logicalView->properties(updateJson->slice(), true).ok()));
      CHECK((false == logicalCollection->getIndexes().empty()));
      CHECK((false == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));
    }

    struct ExecContext: public arangodb::ExecContext {
      ExecContext(): arangodb::ExecContext(arangodb::ExecContext::Type::Default, "", "",
                                           arangodb::auth::Level::NONE, arangodb::auth::Level::NONE) {}
    } execContext;
    arangodb::ExecContextScope scopedExecContext(&execContext);
    auto* authFeature = arangodb::AuthenticationFeature::instance();
    auto* userManager = authFeature->userManager();
    arangodb::aql::QueryRegistry queryRegistry(0); // required for UserManager::loadFromDB()
    userManager->setQueryRegistry(&queryRegistry);
    auto resetUserManager = std::shared_ptr<arangodb::auth::UserManager>(userManager, [](arangodb::auth::UserManager* ptr)->void { ptr->removeAllUsers(); });

    // subsequent update (overwrite) not authorised (NONE collection)
    {
      arangodb::auth::UserMap userMap;
      auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
      user.grantCollection(vocbase.name(), "testCollection", arangodb::auth::Level::NONE); // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

      CHECK((TRI_ERROR_FORBIDDEN == logicalView->properties(viewUpdateJson->slice(), false).errorNumber()));
      CHECK((false == logicalCollection->getIndexes().empty()));
      CHECK((false == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));
    }

    // subsequent update (overwrite) authorised (RO collection)
    {
      arangodb::auth::UserMap userMap;
      auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
      user.grantCollection(vocbase.name(), "testCollection", arangodb::auth::Level::RO); // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

      CHECK((logicalView->properties(viewUpdateJson->slice(), false).ok()));
      CHECK((true == logicalCollection->getIndexes().empty()));
      CHECK((true == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));
    }
  }

  // add authorised link (existing collection not authorized)
  {
    auto collection0Json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection0\", \"id\": 100 }");
    auto collection1Json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection1\", \"id\": 101 }");
    auto viewCreateJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
    auto viewUpdateJson = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection0\": {}, \"testCollection1\": {} } }");

    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalCollection0 = vocbase.createCollection(collection0Json->slice());
    REQUIRE((nullptr != logicalCollection0));
    auto logicalCollection1 = vocbase.createCollection(collection1Json->slice());
    REQUIRE((nullptr != logicalCollection1));
    auto logicalView = vocbase.createView(viewCreateJson->slice());
    REQUIRE((false == !logicalView));

    CHECK((true == logicalCollection0->getIndexes().empty()));
    CHECK((true == logicalCollection1->getIndexes().empty()));
    CHECK((true == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));

    // initial link creation
    {
      auto updateJson = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection0\": {} } }");
      CHECK((logicalView->properties(updateJson->slice(), true).ok()));
      CHECK((false == logicalCollection0->getIndexes().empty()));
      CHECK((true == logicalCollection1->getIndexes().empty()));
      CHECK((false == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));
    }

    struct ExecContext: public arangodb::ExecContext {
      ExecContext(): arangodb::ExecContext(arangodb::ExecContext::Type::Default, "", "",
                                           arangodb::auth::Level::NONE, arangodb::auth::Level::NONE) {}
    } execContext;
    arangodb::ExecContextScope scopedExecContext(&execContext);
    auto* authFeature = arangodb::AuthenticationFeature::instance();
    auto* userManager = authFeature->userManager();
    arangodb::aql::QueryRegistry queryRegistry(0); // required for UserManager::loadFromDB()
    userManager->setQueryRegistry(&queryRegistry);
    auto resetUserManager = std::shared_ptr<arangodb::auth::UserManager>(userManager, [](arangodb::auth::UserManager* ptr)->void { ptr->removeAllUsers(); });

    // subsequent update (overwrite) not authorised (NONE collection)
    {
      arangodb::auth::UserMap userMap;
      auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
      user.grantCollection(vocbase.name(), "testCollection0", arangodb::auth::Level::NONE); // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      user.grantCollection(vocbase.name(), "testCollection1", arangodb::auth::Level::RO); // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

      CHECK((TRI_ERROR_FORBIDDEN == logicalView->properties(viewUpdateJson->slice(), false).errorNumber()));
      CHECK((false == logicalCollection0->getIndexes().empty()));
      CHECK((true == logicalCollection1->getIndexes().empty()));
      CHECK((false == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));
    }

    // subsequent update (overwrite) authorised (RO collection)
    {
      arangodb::auth::UserMap userMap;
      auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
      user.grantCollection(vocbase.name(), "testCollection0", arangodb::auth::Level::RO); // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      user.grantCollection(vocbase.name(), "testCollection1", arangodb::auth::Level::RO); // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

      CHECK((logicalView->properties(viewUpdateJson->slice(), false).ok()));
      CHECK((false == logicalCollection0->getIndexes().empty()));
      CHECK((false == logicalCollection1->getIndexes().empty()));
      CHECK((false == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));
    }
  }

  // drop authorised link (existing collection not authorized)
  {
    auto collection0Json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection0\", \"id\": 100 }");
    auto collection1Json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection1\", \"id\": 101 }");
    auto viewCreateJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
    auto viewUpdateJson = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection0\": {} } }");

    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalCollection0 = vocbase.createCollection(collection0Json->slice());
    REQUIRE((nullptr != logicalCollection0));
    auto logicalCollection1 = vocbase.createCollection(collection1Json->slice());
    REQUIRE((nullptr != logicalCollection1));
    auto logicalView = vocbase.createView(viewCreateJson->slice());
    REQUIRE((false == !logicalView));

    CHECK((true == logicalCollection0->getIndexes().empty()));
    CHECK((true == logicalCollection1->getIndexes().empty()));
    CHECK((true == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));

    // initial link creation
    {
      auto updateJson = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection0\": {}, \"testCollection1\": {} } }");
      CHECK((logicalView->properties(updateJson->slice(), true).ok()));
      CHECK((false == logicalCollection0->getIndexes().empty()));
      CHECK((false == logicalCollection1->getIndexes().empty()));
      CHECK((false == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));
    }

    struct ExecContext: public arangodb::ExecContext {
      ExecContext(): arangodb::ExecContext(arangodb::ExecContext::Type::Default, "", "",
                                           arangodb::auth::Level::NONE, arangodb::auth::Level::NONE) {}
    } execContext;
    arangodb::ExecContextScope scopedExecContext(&execContext);
    auto* authFeature = arangodb::AuthenticationFeature::instance();
    auto* userManager = authFeature->userManager();
    arangodb::aql::QueryRegistry queryRegistry(0); // required for UserManager::loadFromDB()
    userManager->setQueryRegistry(&queryRegistry);
    auto resetUserManager = std::shared_ptr<arangodb::auth::UserManager>(userManager, [](arangodb::auth::UserManager* ptr)->void { ptr->removeAllUsers(); });

    // subsequent update (overwrite) not authorised (NONE collection)
    {
      arangodb::auth::UserMap userMap;
      auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
      user.grantCollection(vocbase.name(), "testCollection0", arangodb::auth::Level::NONE); // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      user.grantCollection(vocbase.name(), "testCollection1", arangodb::auth::Level::RO); // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

      CHECK((TRI_ERROR_FORBIDDEN == logicalView->properties(viewUpdateJson->slice(), false).errorNumber()));
      CHECK((false == logicalCollection0->getIndexes().empty()));
      CHECK((false == logicalCollection1->getIndexes().empty()));
      CHECK((false == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));
    }

    // subsequent update (overwrite) authorised (RO collection)
    {
      arangodb::auth::UserMap userMap;
      auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
      user.grantCollection(vocbase.name(), "testCollection0", arangodb::auth::Level::RO); // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      user.grantCollection(vocbase.name(), "testCollection1", arangodb::auth::Level::RO); // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

      CHECK((logicalView->properties(viewUpdateJson->slice(), false).ok()));
      CHECK((false == logicalCollection0->getIndexes().empty()));
      CHECK((true == logicalCollection1->getIndexes().empty()));
      CHECK((false == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));
    }
  }
}

SECTION("test_update_partial") {
  auto createJson = arangodb::velocypack::Parser::fromJson("{ \
    \"name\": \"testView\", \
    \"type\": \"arangosearch\", \
    \"cleanupIntervalStep\": 52 \
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
      \"cleanupIntervalStep\": 42 \
    }");

    expectedMeta._cleanupIntervalStep = 42;
    CHECK((view->properties(updateJson->slice(), true).ok()));

    // not for persistence
    {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      view->properties(builder, true, false);
      builder.close();

      auto slice = builder.slice();
      arangodb::iresearch::IResearchViewMeta meta;
      arangodb::iresearch::IResearchViewMetaState metaState;
      std::string error;

      CHECK(slice.isObject());
      CHECK((11U == slice.length()));
      CHECK((slice.hasKey("globallyUniqueId") && slice.get("globallyUniqueId").isString() && false == slice.get("globallyUniqueId").copyString().empty()));
      CHECK(slice.get("name").copyString() == "testView");
      CHECK(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
      CHECK(slice.get("deleted").isNone()); // no system properties
      CHECK((meta.init(slice, error) && expectedMeta == meta));
      auto tmpSlice = slice.get("links");
      CHECK((true == tmpSlice.isObject() && 0 == tmpSlice.length()));
    }

    // for persistence
    {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      view->properties(builder, true, true);
      builder.close();

      auto slice = builder.slice();
      arangodb::iresearch::IResearchViewMeta meta;
      arangodb::iresearch::IResearchViewMetaState metaState;
      std::string error;

      CHECK(slice.isObject());
      CHECK((15U == slice.length()));
      CHECK(slice.get("name").copyString() == "testView");
      CHECK(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
      CHECK((slice.hasKey("deleted") && slice.get("deleted").isBoolean() && false == slice.get("deleted").getBoolean())); // has system properties
      CHECK((meta.init(slice, error) && expectedMeta == meta));
      CHECK((true == metaState.init(slice, error) && expectedMetaState == metaState));
      CHECK((false == slice.hasKey("links")));
    }
  }

  // test rollback on meta modification failure (as an example invalid value for 'cleanupIntervalStep')
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalView = vocbase.createView(createJson->slice());
    REQUIRE((false == !logicalView));
    REQUIRE((logicalView->category() == arangodb::LogicalView::category()));

    arangodb::iresearch::IResearchViewMeta expectedMeta;
    arangodb::iresearch::IResearchViewMetaState expectedMetaState;
    auto updateJson = arangodb::velocypack::Parser::fromJson("{ \"cleanupIntervalStep\": 0.123 }");

    expectedMeta._cleanupIntervalStep = 52;
    CHECK((TRI_ERROR_BAD_PARAMETER == logicalView->properties(updateJson->slice(), true).errorNumber()));

    // not for persistence
    {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      logicalView->properties(builder, true, false);
      builder.close();

      auto slice = builder.slice();
      arangodb::iresearch::IResearchViewMeta meta;
      arangodb::iresearch::IResearchViewMetaState metaState;
      std::string error;

      CHECK((slice.isObject()));
      CHECK((11U == slice.length()));
      CHECK((slice.hasKey("globallyUniqueId") && slice.get("globallyUniqueId").isString() && false == slice.get("globallyUniqueId").copyString().empty()));
      CHECK((slice.get("name").copyString() == "testView"));
      CHECK((slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name()));
      CHECK((slice.get("deleted").isNone())); // no system properties
      CHECK((meta.init(slice, error) && expectedMeta == meta));
      auto tmpSlice = slice.get("links");
      CHECK((true == tmpSlice.isObject() && 0 == tmpSlice.length()));
    }

    // for persistence
    {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      logicalView->properties(builder, true, true);
      builder.close();

      auto slice = builder.slice();
      arangodb::iresearch::IResearchViewMeta meta;
      arangodb::iresearch::IResearchViewMetaState metaState;
      std::string error;

      CHECK((slice.isObject()));
      CHECK((15U == slice.length()));
      CHECK((slice.get("name").copyString() == "testView"));
      CHECK((slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name()));
      CHECK((slice.hasKey("deleted") && slice.get("deleted").isBoolean() && false == slice.get("deleted").getBoolean())); // has system properties
      CHECK((meta.init(slice, error) && expectedMeta == meta));
      CHECK((true == metaState.init(slice, error) && expectedMetaState == metaState));
      CHECK((false == slice.hasKey("links")));
    }
  }

  // modify meta params with links to missing collections
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalView = vocbase.createView(createJson->slice());
    REQUIRE((false == !logicalView));
    REQUIRE((logicalView->category() == arangodb::LogicalView::category()));

    arangodb::iresearch::IResearchViewMeta expectedMeta;
    arangodb::iresearch::IResearchViewMetaState expectedMetaState;
    auto updateJson = arangodb::velocypack::Parser::fromJson("{ \"cleanupIntervalStep\": 62, \"links\": { \"testCollection\": {} } }");

    expectedMeta._cleanupIntervalStep = 52;
    CHECK((TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND == logicalView->properties(updateJson->slice(), true).errorNumber()));
    CHECK((true == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));

    // not for persistence
    {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      logicalView->properties(builder, true, false);
      builder.close();

      auto slice = builder.slice();
      arangodb::iresearch::IResearchViewMeta meta;
      arangodb::iresearch::IResearchViewMetaState metaState;
      std::string error;

      CHECK((slice.isObject()));
      CHECK((11U == slice.length()));
      CHECK((slice.hasKey("globallyUniqueId") && slice.get("globallyUniqueId").isString() && false == slice.get("globallyUniqueId").copyString().empty()));
      CHECK((slice.get("name").copyString() == "testView"));
      CHECK((slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name()));
      CHECK((slice.get("deleted").isNone())); // no system properties
      CHECK((meta.init(slice, error) && expectedMeta == meta));
      auto tmpSlice = slice.get("links");
      CHECK((true == tmpSlice.isObject() && 0 == tmpSlice.length()));
    }

    // for persistence
    {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      logicalView->properties(builder, true, true);
      builder.close();

      auto slice = builder.slice();
      arangodb::iresearch::IResearchViewMeta meta;
      arangodb::iresearch::IResearchViewMetaState metaState;
      std::string error;

      CHECK((slice.isObject()));
      CHECK((15U == slice.length()));
      CHECK((slice.get("name").copyString() == "testView"));
      CHECK((slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name()));
      CHECK((slice.hasKey("deleted") && slice.get("deleted").isBoolean() && false == slice.get("deleted").getBoolean())); // has system properties
      CHECK((meta.init(slice, error) && expectedMeta == meta));
      CHECK((true == metaState.init(slice, error) && expectedMetaState == metaState));
      CHECK((false == slice.hasKey("links")));
    }
  }

  // modify meta params with links with invalid definition
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    REQUIRE((nullptr != logicalCollection));
    auto logicalView = vocbase.createView(createJson->slice());
    REQUIRE((false == !logicalView));
    REQUIRE((logicalView->category() == arangodb::LogicalView::category()));
    CHECK((true == logicalCollection->getIndexes().empty()));

    arangodb::iresearch::IResearchViewMeta expectedMeta;
    arangodb::iresearch::IResearchViewMetaState expectedMetaState;
    auto updateJson = arangodb::velocypack::Parser::fromJson("{ \"cleanupIntervalStep\": 62, \"links\": { \"testCollection\": 42 } }");

    expectedMeta._cleanupIntervalStep = 52;
    CHECK((TRI_ERROR_BAD_PARAMETER == logicalView->properties(updateJson->slice(), true).errorNumber()));
    CHECK((true == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));

    // not for persistence
    {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      logicalView->properties(builder, true, false);
      builder.close();

      auto slice = builder.slice();
      arangodb::iresearch::IResearchViewMeta meta;
      arangodb::iresearch::IResearchViewMetaState metaState;
      std::string error;

      CHECK((slice.isObject()));
      CHECK((11U == slice.length()));
      CHECK((slice.hasKey("globallyUniqueId") && slice.get("globallyUniqueId").isString() && false == slice.get("globallyUniqueId").copyString().empty()));
      CHECK((slice.get("name").copyString() == "testView"));
      CHECK((slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name()));
      CHECK((slice.get("deleted").isNone())); // no system properties
      CHECK((meta.init(slice, error) && expectedMeta == meta));
      auto tmpSlice = slice.get("links");
      CHECK((true == tmpSlice.isObject() && 0 == tmpSlice.length()));
    }

    // for persistence
    {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      logicalView->properties(builder, true, true);
      builder.close();

      auto slice = builder.slice();
      arangodb::iresearch::IResearchViewMeta meta;
      arangodb::iresearch::IResearchViewMetaState metaState;
      std::string error;

      CHECK((slice.isObject()));
      CHECK((15U == slice.length()));
      CHECK((slice.get("name").copyString() == "testView"));
      CHECK((slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name()));
      CHECK((slice.hasKey("deleted") && slice.get("deleted").isBoolean() && false == slice.get("deleted").getBoolean())); // has system properties
      CHECK((meta.init(slice, error) && expectedMeta == meta));
      CHECK((true == metaState.init(slice, error) && expectedMetaState == metaState));
      CHECK((false == slice.hasKey("links")));
    }
  }

  // modify meta params with links
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    REQUIRE((nullptr != logicalCollection));
    auto logicalView = vocbase.createView(createJson->slice());
    REQUIRE((false == !logicalView));
    REQUIRE((logicalView->category() == arangodb::LogicalView::category()));
    CHECK((true == logicalCollection->getIndexes().empty()));

    // initial link creation
    {
      auto updateJson = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection\": {} } }");
      arangodb::iresearch::IResearchViewMeta expectedMeta;
      arangodb::iresearch::IResearchViewMetaState expectedMetaState;
      std::unordered_map<std::string, arangodb::iresearch::IResearchLinkMeta> expectedLinkMeta;

      expectedMeta._cleanupIntervalStep = 52;
      expectedMetaState._collections.insert(logicalCollection->id());
      expectedLinkMeta["testCollection"]; // use defaults
      CHECK((logicalView->properties(updateJson->slice(), true).ok()));

      // not for persistence
      {
        arangodb::velocypack::Builder builder;

        builder.openObject();
        logicalView->properties(builder, true, false);
        builder.close();

        auto slice = builder.slice();
        arangodb::iresearch::IResearchViewMeta meta;
        arangodb::iresearch::IResearchViewMetaState metaState;
        std::string error;

        CHECK((slice.isObject()));
        CHECK((11U == slice.length()));
        CHECK((slice.hasKey("globallyUniqueId") && slice.get("globallyUniqueId").isString() && false == slice.get("globallyUniqueId").copyString().empty()));
        CHECK((slice.get("name").copyString() == "testView"));
        CHECK((slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name()));
        CHECK((slice.get("deleted").isNone())); // no system properties
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
      }

      // for persistence
      {
        arangodb::velocypack::Builder builder;

        builder.openObject();
        logicalView->properties(builder, true, true);
        builder.close();

        auto slice = builder.slice();
        arangodb::iresearch::IResearchViewMeta meta;
        arangodb::iresearch::IResearchViewMetaState metaState;
        std::string error;

        CHECK((slice.isObject()));
        CHECK((15U == slice.length()));
        CHECK((slice.get("name").copyString() == "testView"));
        CHECK((slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name()));
        CHECK((slice.hasKey("deleted") && slice.get("deleted").isBoolean() && false == slice.get("deleted").getBoolean())); // has system properties
        CHECK((meta.init(slice, error) && expectedMeta == meta));
        CHECK((true == metaState.init(slice, error) && expectedMetaState == metaState));
        CHECK((false == slice.hasKey("links")));
      }

      CHECK((true == expectedLinkMeta.empty()));
      CHECK((false == logicalCollection->getIndexes().empty()));
    }

    // subsequent update (partial update)
    {
      arangodb::iresearch::IResearchViewMeta expectedMeta;
      arangodb::iresearch::IResearchViewMetaState expectedMetaState;
      std::unordered_map<std::string, arangodb::iresearch::IResearchLinkMeta> expectedLinkMeta;
      auto updateJson = arangodb::velocypack::Parser::fromJson("{ \
        \"cleanupIntervalStep\": 62 \
      }");

      expectedMeta._cleanupIntervalStep = 62;
      expectedMetaState._collections.insert(logicalCollection->id());
      expectedLinkMeta["testCollection"]; // use defaults
      CHECK((logicalView->properties(updateJson->slice(), true).ok()));

      // not for persistence
      {
        arangodb::velocypack::Builder builder;

        builder.openObject();
        logicalView->properties(builder, true, false);
        builder.close();

        auto slice = builder.slice();
        arangodb::iresearch::IResearchViewMeta meta;
        arangodb::iresearch::IResearchViewMetaState metaState;
        std::string error;

        CHECK((slice.isObject()));
        CHECK((11U == slice.length()));
        CHECK((slice.hasKey("globallyUniqueId") && slice.get("globallyUniqueId").isString() && false == slice.get("globallyUniqueId").copyString().empty()));
        CHECK((slice.get("name").copyString() == "testView"));
        CHECK((slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name()));
        CHECK((slice.get("deleted").isNone())); // no system properties
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
      }

      // for persistence
      {
        arangodb::velocypack::Builder builder;

        builder.openObject();
        logicalView->properties(builder, true, true);
        builder.close();

        auto slice = builder.slice();
        arangodb::iresearch::IResearchViewMeta meta;
        arangodb::iresearch::IResearchViewMetaState metaState;
        std::string error;

        CHECK((slice.isObject()));
        CHECK((15U == slice.length()));
        CHECK((slice.get("name").copyString() == "testView"));
        CHECK((slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name()));
        CHECK((slice.hasKey("deleted") && slice.get("deleted").isBoolean() && false == slice.get("deleted").getBoolean())); // has system properties
        CHECK((meta.init(slice, error) && expectedMeta == meta));
        CHECK((true == metaState.init(slice, error) && expectedMetaState == metaState));
        CHECK((false == slice.hasKey("links")));
      }

      CHECK((true == expectedLinkMeta.empty()));
      CHECK((false == logicalCollection->getIndexes().empty()));
    }
  }

  // add a new link (in recovery)
  {
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
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
    CHECK((view->properties(updateJson->slice(), true).ok()));
    CHECK((false == persisted));

    // not for persistence
    {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      view->properties(builder, true, false);
      builder.close();

      auto slice = builder.slice();
      CHECK(slice.isObject());
      CHECK((11U == slice.length()));
      CHECK((slice.hasKey("globallyUniqueId") && slice.get("globallyUniqueId").isString() && false == slice.get("globallyUniqueId").copyString().empty()));
      CHECK(slice.get("name").copyString() == "testView");
      CHECK(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
      CHECK(slice.get("deleted").isNone()); // no system properties
      CHECK(slice.isObject());
      CHECK((
        true == slice.hasKey("links")
        && slice.get("links").isObject()
        && 1 == slice.get("links").length()
      ));
    }

    // for persistence
    {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      view->properties(builder, true, true);
      builder.close();

      auto slice = builder.slice();
      CHECK(slice.isObject());
      CHECK((15U == slice.length()));
      CHECK(slice.get("name").copyString() == "testView");
      CHECK(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
      CHECK((slice.hasKey("deleted") && slice.get("deleted").isBoolean() && false == slice.get("deleted").getBoolean())); // has system properties
      auto tmpSlice = slice.get("collections");
      CHECK((true == tmpSlice.isArray() && 1 == tmpSlice.length()));
      CHECK((false == slice.hasKey("links")));
    }
  }

  // add a new link
  {
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
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

    expectedMeta._cleanupIntervalStep = 52;
    expectedMetaState._collections.insert(logicalCollection->id());
    expectedLinkMeta["testCollection"]; // use defaults
    persisted = false;
    CHECK((view->properties(updateJson->slice(), true).ok()));
    CHECK((true == persisted)); // link addition does modify and persist view meta

    // not for persistence
    {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      view->properties(builder, true, false);
      builder.close();

      auto slice = builder.slice();
      arangodb::iresearch::IResearchViewMeta meta;
      arangodb::iresearch::IResearchViewMetaState metaState;
      std::string error;

      CHECK(slice.isObject());
      CHECK((11U == slice.length()));
      CHECK((slice.hasKey("globallyUniqueId") && slice.get("globallyUniqueId").isString() && false == slice.get("globallyUniqueId").copyString().empty()));
      CHECK(slice.get("name").copyString() == "testView");
      CHECK(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
      CHECK(slice.get("deleted").isNone()); // no system properties
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
    }

    // for persistence
    {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      view->properties(builder, true, true);
      builder.close();

      auto slice = builder.slice();
      arangodb::iresearch::IResearchViewMeta meta;
      arangodb::iresearch::IResearchViewMetaState metaState;
      std::string error;

      CHECK(slice.isObject());
      CHECK((15U == slice.length()));
      CHECK(slice.get("name").copyString() == "testView");
      CHECK(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
      CHECK((slice.hasKey("deleted") && slice.get("deleted").isBoolean() && false == slice.get("deleted").getBoolean())); // has system properties
      CHECK((meta.init(slice, error) && expectedMeta == meta));
      CHECK((true == metaState.init(slice, error) && expectedMetaState == metaState));
      CHECK((false == slice.hasKey("links")));
    }

    CHECK((true == expectedLinkMeta.empty()));
  }

  // add a new link to a collection with documents
  {
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
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
        std::vector<std::string>{logicalCollection->name()},
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

    expectedMeta._cleanupIntervalStep = 52;
    expectedMetaState._collections.insert(logicalCollection->id());
    expectedLinkMeta["testCollection"]; // use defaults
    persisted = false;
    CHECK((view->properties(updateJson->slice(), true).ok()));
    CHECK((true == persisted)); // link addition does modify and persist view meta

    // not for persistence
    {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      view->properties(builder, true, false);
      builder.close();

      auto slice = builder.slice();
      arangodb::iresearch::IResearchViewMeta meta;
      arangodb::iresearch::IResearchViewMetaState metaState;
      std::string error;

      CHECK(slice.isObject());
      CHECK((11U == slice.length()));
      CHECK((slice.hasKey("globallyUniqueId") && slice.get("globallyUniqueId").isString() && false == slice.get("globallyUniqueId").copyString().empty()));
      CHECK(slice.get("name").copyString() == "testView");
      CHECK(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
      CHECK(slice.get("deleted").isNone()); // no system properties
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
    }

    // for persistence
    {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      view->properties(builder, true, true);
      builder.close();

      auto slice = builder.slice();
      arangodb::iresearch::IResearchViewMeta meta;
      arangodb::iresearch::IResearchViewMetaState metaState;
      std::string error;

      CHECK(slice.isObject());
      CHECK((15U == slice.length()));
      CHECK(slice.get("name").copyString() == "testView");
      CHECK(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
      CHECK((slice.hasKey("deleted") && slice.get("deleted").isBoolean() && false == slice.get("deleted").getBoolean())); // has system properties
      CHECK((meta.init(slice, error) && expectedMeta == meta));
      CHECK((true == metaState.init(slice, error) && expectedMetaState == metaState));
      CHECK((false == slice.hasKey("links")));
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

    expectedMeta._cleanupIntervalStep = 52;
    CHECK((TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND == view->properties(updateJson->slice(), true).errorNumber()));

    // not for persistence
    {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      view->properties(builder, true, false);
      builder.close();

      auto slice = builder.slice();
      arangodb::iresearch::IResearchViewMeta meta;
      arangodb::iresearch::IResearchViewMetaState metaState;
      std::string error;

      CHECK(slice.isObject());
      CHECK((11U == slice.length()));
      CHECK((slice.hasKey("globallyUniqueId") && slice.get("globallyUniqueId").isString() && false == slice.get("globallyUniqueId").copyString().empty()));
      CHECK(slice.get("name").copyString() == "testView");
      CHECK(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
      CHECK(slice.get("deleted").isNone()); // no system properties
      CHECK((meta.init(slice, error) && expectedMeta == meta));
      auto tmpSlice = slice.get("links");
      CHECK((true == tmpSlice.isObject() && 0 == tmpSlice.length()));
    }

    // for persistence
    {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      view->properties(builder, true, true);
      builder.close();

      auto slice = builder.slice();
      arangodb::iresearch::IResearchViewMeta meta;
      arangodb::iresearch::IResearchViewMetaState metaState;
      std::string error;

      CHECK(slice.isObject());
      CHECK((15U == slice.length()));
      CHECK(slice.get("name").copyString() == "testView");
      CHECK(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
      CHECK((slice.hasKey("deleted") && slice.get("deleted").isBoolean() && false == slice.get("deleted").getBoolean())); // has system properties
      CHECK((meta.init(slice, error) && expectedMeta == meta));
      CHECK((metaState.init(slice, error) && expectedMetaState == metaState));
      CHECK((false == slice.hasKey("links")));
    }
  }

  // remove link (in recovery)
  {
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
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
      CHECK((view->properties(updateJson->slice(), true).ok()));
      CHECK((false == persisted)); // link addition does not persist view meta

      arangodb::velocypack::Builder builder;

      builder.openObject();
      view->properties(builder, true, false);
      builder.close();

      auto slice = builder.slice();
      CHECK(slice.isObject());
      CHECK((11U == slice.length()));
      CHECK((slice.hasKey("globallyUniqueId") && slice.get("globallyUniqueId").isString() && false == slice.get("globallyUniqueId").copyString().empty()));
      CHECK(slice.get("name").copyString() == "testView");
      CHECK(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
      CHECK(slice.get("deleted").isNone()); // no system properties
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
      CHECK((view->properties(updateJson->slice(), true).ok()));
      CHECK((false == persisted));

      arangodb::velocypack::Builder builder;

      builder.openObject();
      view->properties(builder, true, false);
      builder.close();

      auto slice = builder.slice();
      CHECK(slice.isObject());
      CHECK((11U == slice.length()));
      CHECK((slice.hasKey("globallyUniqueId") && slice.get("globallyUniqueId").isString() && false == slice.get("globallyUniqueId").copyString().empty()));
      CHECK(slice.get("name").copyString() == "testView");
      CHECK(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
      CHECK(slice.get("deleted").isNone()); // no system properties
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
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    REQUIRE((nullptr != logicalCollection));
    auto view = vocbase.createView(createJson->slice());
    REQUIRE((false == !view));

    arangodb::iresearch::IResearchViewMeta expectedMeta;
    arangodb::iresearch::IResearchViewMetaState expectedMetaState;

    expectedMeta._cleanupIntervalStep = 52;
    expectedMetaState._collections.insert(logicalCollection->id());

    {
      auto updateJson = arangodb::velocypack::Parser::fromJson("{ \
        \"links\": { \
          \"testCollection\": {} \
      }}");

      CHECK((view->properties(updateJson->slice(), true).ok()));

      // not for persistence
      {
        arangodb::velocypack::Builder builder;

        builder.openObject();
        view->properties(builder, true, false);
        builder.close();

        auto slice = builder.slice();
        arangodb::iresearch::IResearchViewMeta meta;
        arangodb::iresearch::IResearchViewMetaState metaState;
        std::string error;

        CHECK(slice.isObject());
        CHECK((11U == slice.length()));
        CHECK((slice.hasKey("globallyUniqueId") && slice.get("globallyUniqueId").isString() && false == slice.get("globallyUniqueId").copyString().empty()));
        CHECK(slice.get("name").copyString() == "testView");
        CHECK(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
        CHECK(slice.get("deleted").isNone()); // no system properties
        CHECK((meta.init(slice, error) && expectedMeta == meta));
        auto tmpSlice = slice.get("links");
        CHECK((true == tmpSlice.isObject() && 1 == tmpSlice.length()));
      }

      // for persistence
      {
        arangodb::velocypack::Builder builder;

        builder.openObject();
        view->properties(builder, true, true);
        builder.close();

        auto slice = builder.slice();
        arangodb::iresearch::IResearchViewMeta meta;
        arangodb::iresearch::IResearchViewMetaState metaState;
        std::string error;

        CHECK(slice.isObject());
        CHECK((15U == slice.length()));
        CHECK(slice.get("name").copyString() == "testView");
        CHECK(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
        CHECK((slice.hasKey("deleted") && slice.get("deleted").isBoolean() && false == slice.get("deleted").getBoolean())); // has system properties
        CHECK((meta.init(slice, error) && expectedMeta == meta));
        CHECK((true == metaState.init(slice, error) && expectedMetaState == metaState));
        CHECK((false == slice.hasKey("links")));
      }
    }

    {
      auto updateJson = arangodb::velocypack::Parser::fromJson("{ \
        \"links\": { \
          \"testCollection\": null \
      }}");

      expectedMeta._cleanupIntervalStep = 52;
      expectedMetaState._collections.clear();
      CHECK((view->properties(updateJson->slice(), true).ok()));

      // not for persistence
      {
        arangodb::velocypack::Builder builder;

        builder.openObject();
        view->properties(builder, true, false);
        builder.close();

        auto slice = builder.slice();
        arangodb::iresearch::IResearchViewMeta meta;
        arangodb::iresearch::IResearchViewMetaState metaState;
        std::string error;

        CHECK(slice.isObject());
        CHECK((11U == slice.length()));
        CHECK((slice.hasKey("globallyUniqueId") && slice.get("globallyUniqueId").isString() && false == slice.get("globallyUniqueId").copyString().empty()));
        CHECK(slice.get("name").copyString() == "testView");
        CHECK(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
        CHECK(slice.get("deleted").isNone()); // no system properties
        CHECK((meta.init(slice, error) && expectedMeta == meta));
        auto tmpSlice = slice.get("links");
        CHECK((true == tmpSlice.isObject() && 0 == tmpSlice.length()));
      }

      // for persistence
      {
        arangodb::velocypack::Builder builder;

        builder.openObject();
        view->properties(builder, true, true);
        builder.close();

        auto slice = builder.slice();
        arangodb::iresearch::IResearchViewMeta meta;
        arangodb::iresearch::IResearchViewMetaState metaState;
        std::string error;

        CHECK(slice.isObject());
        CHECK((15U == slice.length()));
        CHECK(slice.get("name").copyString() == "testView");
        CHECK(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
        CHECK((slice.hasKey("deleted") && slice.get("deleted").isBoolean() && false == slice.get("deleted").getBoolean())); // has system properties
        CHECK((meta.init(slice, error) && expectedMeta == meta));
        CHECK((true == metaState.init(slice, error) && expectedMetaState == metaState));
        CHECK((false == slice.hasKey("links")));
      }
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

    expectedMeta._cleanupIntervalStep = 52;
    CHECK((TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND == view->properties(updateJson->slice(), true).errorNumber()));

    // not for persistence
    {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      view->properties(builder, true, false);
      builder.close();

      auto slice = builder.slice();
      arangodb::iresearch::IResearchViewMeta meta;
      arangodb::iresearch::IResearchViewMetaState metaState;
      std::string error;

      CHECK(slice.isObject());
      CHECK((11U == slice.length()));
      CHECK((slice.hasKey("globallyUniqueId") && slice.get("globallyUniqueId").isString() && false == slice.get("globallyUniqueId").copyString().empty()));
      CHECK(slice.get("name").copyString() == "testView");
      CHECK(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
      CHECK(slice.get("deleted").isNone()); // no system properties
      CHECK((meta.init(slice, error) && expectedMeta == meta));
      auto tmpSlice = slice.get("links");
      CHECK((true == tmpSlice.isObject() && 0 == tmpSlice.length()));
    }

    // for persistence
    {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      view->properties(builder, true, true);
      builder.close();

      auto slice = builder.slice();
      arangodb::iresearch::IResearchViewMeta meta;
      arangodb::iresearch::IResearchViewMetaState metaState;
      std::string error;

      CHECK(slice.isObject());
      CHECK((15U == slice.length()));
      CHECK(slice.get("name").copyString() == "testView");
      CHECK(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
      CHECK((slice.hasKey("deleted") && slice.get("deleted").isBoolean() && false == slice.get("deleted").getBoolean())); // has system properties
      CHECK((meta.init(slice, error) && expectedMeta == meta));
      CHECK((true == metaState.init(slice, error) && expectedMetaState == metaState));
      CHECK((false == slice.hasKey("links")));
    }
  }

  // remove non-existant link
  {
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    REQUIRE((nullptr != logicalCollection));
    auto view = vocbase.createView(createJson->slice());
    REQUIRE((false == !view));

    arangodb::iresearch::IResearchViewMeta expectedMeta;
    arangodb::iresearch::IResearchViewMetaState expectedMetaState;
    auto updateJson = arangodb::velocypack::Parser::fromJson("{ \
      \"links\": { \
        \"testCollection\": null \
    }}");

    expectedMeta._cleanupIntervalStep = 52;
    CHECK((view->properties(updateJson->slice(), true).ok()));

    // not for persistence
    {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      view->properties(builder, true, false);
      builder.close();

      auto slice = builder.slice();
      arangodb::iresearch::IResearchViewMeta meta;
      arangodb::iresearch::IResearchViewMetaState metaState;
      std::string error;

      CHECK(slice.isObject());
      CHECK((11U == slice.length()));
      CHECK((slice.hasKey("globallyUniqueId") && slice.get("globallyUniqueId").isString() && false == slice.get("globallyUniqueId").copyString().empty()));
      CHECK(slice.get("name").copyString() == "testView");
      CHECK(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
      CHECK(slice.get("deleted").isNone()); // no system properties
      CHECK((meta.init(slice, error) && expectedMeta == meta));
      auto tmpSlice = slice.get("links");
      CHECK((true == tmpSlice.isObject() && 0 == tmpSlice.length()));
    }

    // for persistence
    {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      view->properties(builder, true, true);
      builder.close();

      auto slice = builder.slice();
      arangodb::iresearch::IResearchViewMeta meta;
      arangodb::iresearch::IResearchViewMetaState metaState;
      std::string error;

      CHECK(slice.isObject());
      CHECK((15U == slice.length()));
      CHECK(slice.get("name").copyString() == "testView");
      CHECK(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
      CHECK((slice.hasKey("deleted") && slice.get("deleted").isBoolean() && false == slice.get("deleted").getBoolean())); // has system properties
      CHECK((meta.init(slice, error) && expectedMeta == meta));
      CHECK((true == metaState.init(slice, error) && expectedMetaState == metaState));
      CHECK((false == slice.hasKey("links")));
    }
  }

  // remove + add link to same collection (reindex)
  {
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    REQUIRE((nullptr != logicalCollection));
    auto view = vocbase.createView(createJson->slice());
    REQUIRE((false == !view));

    // initial add of link
    {
      auto updateJson = arangodb::velocypack::Parser::fromJson(
        "{ \"links\": { \"testCollection\": {} } }"
      );
      CHECK((view->properties(updateJson->slice(), true).ok()));

      // not for persistence
      {
        arangodb::velocypack::Builder builder;

        builder.openObject();
        view->properties(builder, true, false);
        builder.close();

        auto slice = builder.slice();
        CHECK(slice.isObject());
        CHECK((11U == slice.length()));
        CHECK((slice.hasKey("globallyUniqueId") && slice.get("globallyUniqueId").isString() && false == slice.get("globallyUniqueId").copyString().empty()));
        CHECK(slice.get("name").copyString() == "testView");
        CHECK(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
        CHECK(slice.get("deleted").isNone()); // no system properties
        auto tmpSlice = slice.get("links");
        CHECK((true == tmpSlice.isObject() && 1 == tmpSlice.length()));
      }

      // for persistence
      {
        arangodb::velocypack::Builder builder;

        builder.openObject();
        view->properties(builder, true, true);
        builder.close();

        auto slice = builder.slice();
        CHECK(slice.isObject());
        CHECK((15U == slice.length()));
        CHECK(slice.get("name").copyString() == "testView");
        CHECK(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
        CHECK((slice.hasKey("deleted") && slice.get("deleted").isBoolean() && false == slice.get("deleted").getBoolean())); // has system properties
        auto tmpSlice = slice.get("collections");
        CHECK((true == tmpSlice.isArray() && 1 == tmpSlice.length()));
        CHECK((false == slice.hasKey("links")));
      }
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
      CHECK((view->properties(updateJson->slice(), true).ok()));

      // not for persistence
      {
        arangodb::velocypack::Builder builder;

        builder.openObject();
        view->properties(builder, true, false);
        builder.close();

        auto slice = builder.slice();
        CHECK(slice.isObject());
        CHECK((11U == slice.length()));
        CHECK((slice.hasKey("globallyUniqueId") && slice.get("globallyUniqueId").isString() && false == slice.get("globallyUniqueId").copyString().empty()));
        CHECK(slice.get("name").copyString() == "testView");
        CHECK(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
        CHECK(slice.get("deleted").isNone()); // no system properties
        auto tmpSlice = slice.get("links");
        CHECK((true == tmpSlice.isObject() && 1 == tmpSlice.length()));
      }

      // for persistence
      {
        arangodb::velocypack::Builder builder;

        builder.openObject();
        view->properties(builder, true, true);
        builder.close();

        auto slice = builder.slice();
        CHECK(slice.isObject());
        CHECK((15U == slice.length()));
        CHECK(slice.get("name").copyString() == "testView");
        CHECK(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
        CHECK((slice.hasKey("deleted") && slice.get("deleted").isBoolean() && false == slice.get("deleted").getBoolean())); // has system properties
        auto tmpSlice = slice.get("collections");
        CHECK((true == tmpSlice.isArray() && 1 == tmpSlice.length()));
        CHECK((false == slice.hasKey("links")));
      }

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
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    REQUIRE((nullptr != logicalCollection));
    auto view = vocbase.createView(createJson->slice());
    REQUIRE((false == !view));

    // initial add of link
    {
      auto updateJson = arangodb::velocypack::Parser::fromJson(
        "{ \"links\": { \"testCollection\": { \"includeAllFields\": true } } }"
      );
      CHECK((view->properties(updateJson->slice(), true).ok()));

      // not for persistence
      {
        arangodb::velocypack::Builder builder;

        builder.openObject();
        view->properties(builder, true, false);
        builder.close();

        auto slice = builder.slice();
        CHECK(slice.isObject());
        CHECK((11U == slice.length()));
        CHECK((slice.hasKey("globallyUniqueId") && slice.get("globallyUniqueId").isString() && false == slice.get("globallyUniqueId").copyString().empty()));
        CHECK(slice.get("name").copyString() == "testView");
        CHECK(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
        CHECK(slice.get("deleted").isNone()); // no system properties
        auto tmpSlice = slice.get("links");
        CHECK((true == tmpSlice.isObject() && 1 == tmpSlice.length()));
        tmpSlice = tmpSlice.get("testCollection");
        CHECK((true == tmpSlice.isObject()));
        tmpSlice = tmpSlice.get("includeAllFields");
        CHECK((true == tmpSlice.isBoolean() && true == tmpSlice.getBoolean()));
      }

      // for persistence
      {
        arangodb::velocypack::Builder builder;

        builder.openObject();
        view->properties(builder, true, true);
        builder.close();

        auto slice = builder.slice();
        CHECK(slice.isObject());
        CHECK((15U == slice.length()));
        CHECK(slice.get("name").copyString() == "testView");
        CHECK(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
        CHECK((slice.hasKey("deleted") && slice.get("deleted").isBoolean() && false == slice.get("deleted").getBoolean())); // has system properties
        auto tmpSlice = slice.get("collections");
        CHECK((true == tmpSlice.isArray() && 1 == tmpSlice.length()));
        CHECK((false == slice.hasKey("links")));
      }
    }

    // update link
    {
      auto updateJson = arangodb::velocypack::Parser::fromJson(
        "{ \"links\": { \"testCollection\": { } } }"
      );
      CHECK((view->properties(updateJson->slice(), true).ok()));

      // not for persistence
      {
        arangodb::velocypack::Builder builder;

        builder.openObject();
        view->properties(builder, true, false);
        builder.close();

        auto slice = builder.slice();
        CHECK(slice.isObject());
        CHECK((11U == slice.length()));
        CHECK((slice.hasKey("globallyUniqueId") && slice.get("globallyUniqueId").isString() && false == slice.get("globallyUniqueId").copyString().empty()));
        CHECK(slice.get("name").copyString() == "testView");
        CHECK(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
        CHECK(slice.get("deleted").isNone()); // no system properties
        auto tmpSlice = slice.get("links");
        CHECK((true == tmpSlice.isObject() && 1 == tmpSlice.length()));
        tmpSlice = tmpSlice.get("testCollection");
        CHECK((true == tmpSlice.isObject()));
        tmpSlice = tmpSlice.get("includeAllFields");
        CHECK((true == tmpSlice.isBoolean() && false == tmpSlice.getBoolean()));
      }

      // for persistence
      {
        arangodb::velocypack::Builder builder;

        builder.openObject();
        view->properties(builder, true, true);
        builder.close();

        auto slice = builder.slice();
        CHECK(slice.isObject());
        CHECK((15U == slice.length()));
        CHECK(slice.get("name").copyString() == "testView");
        CHECK(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
        CHECK((slice.hasKey("deleted") && slice.get("deleted").isBoolean() && false == slice.get("deleted").getBoolean())); // has system properties
        auto tmpSlice = slice.get("collections");
        CHECK((true == tmpSlice.isArray() && 1 == tmpSlice.length()));
        CHECK((false == slice.hasKey("links")));
      }
    }
  }

  // modify meta params with links (collection not authorized)
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\", \"id\": 100 }");
    auto viewCreateJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
    auto viewUpdateJson = arangodb::velocypack::Parser::fromJson("{ \"cleanupIntervalStep\": 62 }");

    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    REQUIRE((nullptr != logicalCollection));
    auto logicalView = vocbase.createView(viewCreateJson->slice());
    REQUIRE((false == !logicalView));

    CHECK((true == logicalCollection->getIndexes().empty()));
    CHECK((true == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));

    // initial link creation
    {
      auto updateJson = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection\": {} } }");
      CHECK((logicalView->properties(updateJson->slice(), true).ok()));
      CHECK((false == logicalCollection->getIndexes().empty()));
      CHECK((false == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));
    }

    struct ExecContext: public arangodb::ExecContext {
      ExecContext(): arangodb::ExecContext(arangodb::ExecContext::Type::Default, "", "",
                                           arangodb::auth::Level::NONE, arangodb::auth::Level::NONE) {}
    } execContext;
    arangodb::ExecContextScope execContextScope(&execContext);
    auto* authFeature = arangodb::AuthenticationFeature::instance();
    auto* userManager = authFeature->userManager();
    arangodb::aql::QueryRegistry queryRegistry(0); // required for UserManager::loadFromDB()
    userManager->setQueryRegistry(&queryRegistry);
    auto resetUserManager = std::shared_ptr<arangodb::auth::UserManager>(userManager, [](arangodb::auth::UserManager* ptr)->void { ptr->removeAllUsers(); });

    // subsequent update (overwrite) not authorised (NONE collection)
    {
      arangodb::auth::UserMap userMap;
      auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
      user.grantCollection(vocbase.name(), "testCollection", arangodb::auth::Level::NONE); // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

      arangodb::iresearch::IResearchViewMeta expectedMeta;
      expectedMeta._cleanupIntervalStep = 10;

      CHECK((TRI_ERROR_FORBIDDEN == logicalView->properties(viewUpdateJson->slice(), true).errorNumber()));

      arangodb::velocypack::Builder builder;
      builder.openObject();
      logicalView->properties(builder, true, true); // 'forPersistence' to avoid auth check
      builder.close();

      auto slice = builder.slice();
      arangodb::iresearch::IResearchViewMeta meta;
      std::string error;
      CHECK((meta.init(slice, error) && expectedMeta == meta));
    }

    // subsequent update (overwrite) authorised (RO collection)
    {
      arangodb::auth::UserMap userMap;
      auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
      user.grantCollection(vocbase.name(), "testCollection", arangodb::auth::Level::RO); // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

      arangodb::iresearch::IResearchViewMeta expectedMeta;
      expectedMeta._cleanupIntervalStep = 62;

      CHECK((logicalView->properties(viewUpdateJson->slice(), true).ok()));

      arangodb::velocypack::Builder builder;
      builder.openObject();
      logicalView->properties(builder, true, true); // 'forPersistence' to avoid auth check
      builder.close();

      auto slice = builder.slice();
      arangodb::iresearch::IResearchViewMeta meta;
      std::string error;
      CHECK((meta.init(slice, error) && expectedMeta == meta));
    }
  }

  // add link (collection not authorized)
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\", \"id\": 100 }");
    auto viewCreateJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
    auto viewUpdateJson = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection\": {} } }");

    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    REQUIRE((nullptr != logicalCollection));
    auto logicalView = vocbase.createView(viewCreateJson->slice());
    REQUIRE((false == !logicalView));

    CHECK((true == logicalCollection->getIndexes().empty()));
    CHECK((true == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));

    struct ExecContext: public arangodb::ExecContext {
      ExecContext(): arangodb::ExecContext(arangodb::ExecContext::Type::Default, "", "",
                                           arangodb::auth::Level::NONE, arangodb::auth::Level::NONE) {}
    } execContext;
    auto* origExecContext = ExecContext::CURRENT;
    auto resetExecContext = irs::make_finally([origExecContext]()->void{ ExecContext::CURRENT = origExecContext; });
    ExecContext::CURRENT = &execContext;
    auto* authFeature = arangodb::AuthenticationFeature::instance();
    auto* userManager = authFeature->userManager();
    arangodb::auth::UserMap userMap; // empty map, no user -> no permissions
    userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database
    auto resetUserManager = irs::make_finally([userManager]()->void{ userManager->removeAllUsers(); });

    CHECK((TRI_ERROR_FORBIDDEN == logicalView->properties(viewUpdateJson->slice(), false).errorNumber()));
    CHECK((true == logicalCollection->getIndexes().empty()));
    CHECK((true == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));
  }

  // drop link (collection not authorized)
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\", \"id\": 100 }");
    auto viewCreateJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
    auto viewUpdateJson = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection\": null } }");

    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    REQUIRE((nullptr != logicalCollection));
    auto logicalView = vocbase.createView(viewCreateJson->slice());
    REQUIRE((false == !logicalView));

    CHECK((true == logicalCollection->getIndexes().empty()));
    CHECK((true == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));

    // initial link creation
    {
      auto updateJson = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection\": {} } }");
      CHECK((logicalView->properties(updateJson->slice(), true).ok()));
      CHECK((false == logicalCollection->getIndexes().empty()));
      CHECK((false == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));
    }

    struct ExecContext: public arangodb::ExecContext {
      ExecContext(): arangodb::ExecContext(arangodb::ExecContext::Type::Default, "", "",
                                           arangodb::auth::Level::NONE, arangodb::auth::Level::NONE) {}
    } execContext;
    arangodb::ExecContextScope execContextScope(&execContext);
    auto* authFeature = arangodb::AuthenticationFeature::instance();
    auto* userManager = authFeature->userManager();
    arangodb::aql::QueryRegistry queryRegistry(0); // required for UserManager::loadFromDB()
    userManager->setQueryRegistry(&queryRegistry);
    auto resetUserManager = std::shared_ptr<arangodb::auth::UserManager>(userManager, [](arangodb::auth::UserManager* ptr)->void { ptr->removeAllUsers(); });

    // subsequent update (overwrite) not authorised (NONE collection)
    {
      arangodb::auth::UserMap userMap;
      auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
      user.grantCollection(vocbase.name(), "testCollection", arangodb::auth::Level::NONE); // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

      CHECK((TRI_ERROR_FORBIDDEN == logicalView->properties(viewUpdateJson->slice(), true).errorNumber()));
      CHECK((false == logicalCollection->getIndexes().empty()));
      CHECK((false == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));
    }

    // subsequent update (overwrite) authorised (RO collection)
    {
      arangodb::auth::UserMap userMap;
      auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
      user.grantCollection(vocbase.name(), "testCollection", arangodb::auth::Level::RO); // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

      CHECK((logicalView->properties(viewUpdateJson->slice(), true).ok()));
      CHECK((true == logicalCollection->getIndexes().empty()));
      CHECK((true == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));
    }
  }

  // add authorised link (existing collection not authorized)
  {
    auto collection0Json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection0\", \"id\": 100 }");
    auto collection1Json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection1\", \"id\": 101 }");
    auto viewCreateJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
    auto viewUpdateJson = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection1\": {} } }");

    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalCollection0 = vocbase.createCollection(collection0Json->slice());
    REQUIRE((nullptr != logicalCollection0));
    auto logicalCollection1 = vocbase.createCollection(collection1Json->slice());
    REQUIRE((nullptr != logicalCollection1));
    auto logicalView = vocbase.createView(viewCreateJson->slice());
    REQUIRE((false == !logicalView));

    CHECK((true == logicalCollection0->getIndexes().empty()));
    CHECK((true == logicalCollection1->getIndexes().empty()));
    CHECK((true == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));

    // initial link creation
    {
      auto updateJson = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection0\": {} } }");
      CHECK((logicalView->properties(updateJson->slice(), true).ok()));
      CHECK((false == logicalCollection0->getIndexes().empty()));
      CHECK((true == logicalCollection1->getIndexes().empty()));
      CHECK((false == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));
    }

    struct ExecContext: public arangodb::ExecContext {
      ExecContext(): arangodb::ExecContext(arangodb::ExecContext::Type::Default, "", "",
                                           arangodb::auth::Level::NONE, arangodb::auth::Level::NONE) {}
    } execContext;
    arangodb::ExecContextScope execContextScope(&execContext);
    auto* authFeature = arangodb::AuthenticationFeature::instance();
    auto* userManager = authFeature->userManager();
    arangodb::aql::QueryRegistry queryRegistry(0); // required for UserManager::loadFromDB()
    userManager->setQueryRegistry(&queryRegistry);
    auto resetUserManager = std::shared_ptr<arangodb::auth::UserManager>(userManager, [](arangodb::auth::UserManager* ptr)->void { ptr->removeAllUsers(); });

    // subsequent update (overwrite) not authorised (NONE collection)
    {
      arangodb::auth::UserMap userMap;
      auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
      user.grantCollection(vocbase.name(), "testCollection0", arangodb::auth::Level::NONE); // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      user.grantCollection(vocbase.name(), "testCollection1", arangodb::auth::Level::RO); // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

      CHECK((TRI_ERROR_FORBIDDEN == logicalView->properties(viewUpdateJson->slice(), true).errorNumber()));
      CHECK((false == logicalCollection0->getIndexes().empty()));
      CHECK((true == logicalCollection1->getIndexes().empty()));
      CHECK((false == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));
    }

    // subsequent update (overwrite) authorised (RO collection)
    {
      arangodb::auth::UserMap userMap;
      auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
      user.grantCollection(vocbase.name(), "testCollection0", arangodb::auth::Level::RO); // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      user.grantCollection(vocbase.name(), "testCollection1", arangodb::auth::Level::RO); // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

      CHECK((logicalView->properties(viewUpdateJson->slice(), true).ok()));
      CHECK((false == logicalCollection0->getIndexes().empty()));
      CHECK((false == logicalCollection1->getIndexes().empty()));
      CHECK((false == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));
    }
  }

  // drop authorised link (existing collection not authorized)
  {
    auto collection0Json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection0\", \"id\": 100 }");
    auto collection1Json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection1\", \"id\": 101 }");
    auto viewCreateJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
    auto viewUpdateJson = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection1\": null } }");

    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalCollection0 = vocbase.createCollection(collection0Json->slice());
    REQUIRE((nullptr != logicalCollection0));
    auto logicalCollection1 = vocbase.createCollection(collection1Json->slice());
    REQUIRE((nullptr != logicalCollection1));
    auto logicalView = vocbase.createView(viewCreateJson->slice());
    REQUIRE((false == !logicalView));

    CHECK((true == logicalCollection0->getIndexes().empty()));
    CHECK((true == logicalCollection1->getIndexes().empty()));
    CHECK((true == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));

    // initial link creation
    {
      auto updateJson = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection0\": {}, \"testCollection1\": {} } }");
      CHECK((logicalView->properties(updateJson->slice(), true).ok()));
      CHECK((false == logicalCollection0->getIndexes().empty()));
      CHECK((false == logicalCollection1->getIndexes().empty()));
      CHECK((false == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));
    }

    struct ExecContext: public arangodb::ExecContext {
      ExecContext(): arangodb::ExecContext(arangodb::ExecContext::Type::Default, "", "",
                                           arangodb::auth::Level::NONE, arangodb::auth::Level::NONE) {}
    } execContext;
    arangodb::ExecContextScope execContextScope(&execContext);
    auto* authFeature = arangodb::AuthenticationFeature::instance();
    auto* userManager = authFeature->userManager();
    arangodb::aql::QueryRegistry queryRegistry(0); // required for UserManager::loadFromDB()
    userManager->setQueryRegistry(&queryRegistry);
    auto resetUserManager = std::shared_ptr<arangodb::auth::UserManager>(userManager, [](arangodb::auth::UserManager* ptr)->void { ptr->removeAllUsers(); });

    // subsequent update (overwrite) not authorised (NONE collection)
    {
      arangodb::auth::UserMap userMap;
      auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
      user.grantCollection(vocbase.name(), "testCollection0", arangodb::auth::Level::NONE); // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      user.grantCollection(vocbase.name(), "testCollection1", arangodb::auth::Level::RO); // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

      CHECK((TRI_ERROR_FORBIDDEN == logicalView->properties(viewUpdateJson->slice(), true).errorNumber()));
      CHECK((false == logicalCollection0->getIndexes().empty()));
      CHECK((false == logicalCollection1->getIndexes().empty()));
      CHECK((false == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));
    }

    // subsequent update (overwrite) authorised (RO collection)
    {
      arangodb::auth::UserMap userMap;
      auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
      user.grantCollection(vocbase.name(), "testCollection0", arangodb::auth::Level::RO); // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      user.grantCollection(vocbase.name(), "testCollection1", arangodb::auth::Level::RO); // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

      CHECK((logicalView->properties(viewUpdateJson->slice(), true).ok()));
      CHECK((false == logicalCollection0->getIndexes().empty()));
      CHECK((true == logicalCollection1->getIndexes().empty()));
      CHECK((false == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));
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