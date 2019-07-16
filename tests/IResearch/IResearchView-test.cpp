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

#include "gtest/gtest.h"

#include "common.h"
#include "../Mocks/StorageEngineMock.h"
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

namespace {

struct DocIdScorer: public irs::sort {
  DECLARE_SORT_TYPE() { static irs::sort::type_id type("test_doc_id"); return type; }
  static ptr make(const irs::string_ref&) { PTR_NAMED(DocIdScorer, ptr); return ptr; }
  DocIdScorer(): irs::sort(DocIdScorer::type()) { }
  virtual sort::prepared::ptr prepare() const override { PTR_NAMED(Prepared, ptr); return ptr; }

  struct Prepared: public irs::sort::prepared_base<uint64_t, void> {
    virtual void add(irs::byte_type* dst, const irs::byte_type* src) const override { score_cast(dst) = score_cast(src); }
    virtual void collect(irs::byte_type*, const irs::index_reader& index, const irs::sort::field_collector* field, const irs::sort::term_collector* term) const override {}
    virtual irs::flags const& features() const override { return irs::flags::empty_instance(); }
    virtual bool less(const irs::byte_type* lhs, const irs::byte_type* rhs) const override { return score_cast(lhs) < score_cast(rhs); }
    virtual irs::sort::field_collector::ptr prepare_field_collector() const override { return nullptr; }
    virtual void prepare_score(irs::byte_type* score) const override { }
    virtual irs::sort::term_collector::ptr prepare_term_collector() const override { return nullptr; }
    virtual std::pair<score_ctx::ptr, irs::score_f> prepare_scorer(
      irs::sub_reader const& segment,
      irs::term_reader const& field,
      irs::byte_type const*,
      irs::attribute_view const& doc_attrs,
      irs::boost_t
    ) const override {
      return {
        std::make_unique<ScoreCtx>(doc_attrs.get<irs::document>()),
        [](const void* ctx, irs::byte_type* score_buf) {
          reinterpret_cast<uint64_t&>(*score_buf) = reinterpret_cast<const ScoreCtx*>(ctx)->_doc.get()->value;
        }
      };
    }
  };

  struct ScoreCtx: public irs::sort::score_ctx {
    ScoreCtx(irs::attribute_view::ref<irs::document>::type const& doc): _doc(doc) { }
    irs::attribute_view::ref<irs::document>::type const& _doc;
  };
};

REGISTER_SCORER_TEXT(DocIdScorer, DocIdScorer::make);

}

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

class IResearchViewTest : public ::testing::Test {
protected:
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

  IResearchViewTest(): engine(server), server(nullptr, nullptr) {
    arangodb::EngineSelectorFeature::ENGINE = &engine;

    arangodb::tests::init();

    // suppress INFO {authentication} Authentication is turned on (system only), authentication for unix sockets is turned on
    // suppress WARNING {authentication} --server.jwt-secret is insecure. Use --server.jwt-secret-keyfile instead
    arangodb::LogTopic::setLogLevel(arangodb::Logger::AUTHENTICATION.name(), arangodb::LogLevel::ERR);

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

  ~IResearchViewTest() {
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

TEST_F(IResearchViewTest, test_type) {
  EXPECT_TRUE((arangodb::LogicalDataSource::Type::emplace(arangodb::velocypack::StringRef("arangosearch")) == arangodb::iresearch::DATA_SOURCE_TYPE));
}

TEST_F(IResearchViewTest, test_defaults) {
  auto json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\" }");

  // view definition with LogicalView (for persistence)
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    arangodb::LogicalView::ptr view;
    EXPECT_TRUE((arangodb::iresearch::IResearchView::factory().create(view, vocbase, json->slice()).ok()));
    EXPECT_TRUE((false == !view));

    arangodb::iresearch::IResearchViewMeta expectedMeta;
    arangodb::iresearch::IResearchViewMetaState expectedMetaState;
    arangodb::velocypack::Builder builder;

    builder.openObject();
    view->properties(builder, arangodb::LogicalDataSource::makeFlags(
                                  arangodb::LogicalDataSource::Serialize::Detailed,
                                  arangodb::LogicalDataSource::Serialize::ForPersistence));
    builder.close();

    auto slice = builder.slice();
    arangodb::iresearch::IResearchViewMeta meta;
    arangodb::iresearch::IResearchViewMetaState metaState;
    std::string error;

    EXPECT_TRUE((17U == slice.length()));
    EXPECT_TRUE((slice.hasKey("globallyUniqueId") && slice.get("globallyUniqueId").isString() && false == slice.get("globallyUniqueId").copyString().empty()));
    EXPECT_TRUE(slice.get("name").copyString() == "testView");
    EXPECT_TRUE(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
    EXPECT_TRUE(false == slice.get("deleted").getBool());
    EXPECT_TRUE(false == slice.get("isSystem").getBool());
    EXPECT_TRUE((!slice.hasKey("links"))); // for persistence so no links
    EXPECT_TRUE((meta.init(slice, error) && expectedMeta == meta));
    EXPECT_TRUE((true == metaState.init(slice, error) && expectedMetaState == metaState));
  }

  // view definition with LogicalView
  {
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    arangodb::LogicalView::ptr view;
    EXPECT_TRUE((arangodb::iresearch::IResearchView::factory().create(view, vocbase, json->slice()).ok()));
    EXPECT_TRUE((false == !view));

    arangodb::iresearch::IResearchViewMeta expectedMeta;
    arangodb::velocypack::Builder builder;

    builder.openObject();
    view->properties(builder, arangodb::LogicalDataSource::makeFlags(
                                  arangodb::LogicalDataSource::Serialize::Detailed));
    builder.close();

    auto slice = builder.slice();
    arangodb::iresearch::IResearchViewMeta meta;
    std::string error;

    EXPECT_TRUE((slice.isObject()));
    EXPECT_TRUE((13U == slice.length()));
    EXPECT_TRUE((slice.hasKey("globallyUniqueId") && slice.get("globallyUniqueId").isString() && false == slice.get("globallyUniqueId").copyString().empty()));
    EXPECT_TRUE(slice.get("name").copyString() == "testView");
    EXPECT_TRUE(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
    EXPECT_TRUE((false == slice.hasKey("deleted")));
    EXPECT_TRUE((meta.init(slice, error) && expectedMeta == meta));

    auto tmpSlice = slice.get("links");
    EXPECT_TRUE((true == tmpSlice.isObject() && 0 == tmpSlice.length()));
  }

  // new view definition with links to missing collections
  {
    auto viewCreateJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\", \"id\": 101, \"links\": { \"testCollection\": {} } }");

    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    EXPECT_TRUE((true == !vocbase.lookupView("testView")));
    arangodb::LogicalView::ptr view;
    auto res = arangodb::iresearch::IResearchView::factory().create(view, vocbase, viewCreateJson->slice());
    EXPECT_TRUE((TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND == res.errorNumber()));
    EXPECT_TRUE((true == !vocbase.lookupView("testView")));
  }

  // new view definition with links with invalid definition
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\", \"id\": 100 }");
    auto viewCreateJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\", \"id\": 101, \"links\": { \"testCollection\": 42 } }");

    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    EXPECT_TRUE((nullptr != logicalCollection));
    EXPECT_TRUE((true == !vocbase.lookupView("testView")));
    EXPECT_TRUE((true == logicalCollection->getIndexes().empty()));
    arangodb::LogicalView::ptr view;
    auto res = arangodb::iresearch::IResearchView::factory().create(view, vocbase, viewCreateJson->slice());
    EXPECT_TRUE((TRI_ERROR_BAD_PARAMETER == res.errorNumber()));
    EXPECT_TRUE((true == !vocbase.lookupView("testView")));
  }

  // new view definition with links (collection not authorized)
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\", \"id\": 100 }");
    auto viewCreateJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\", \"links\": { \"testCollection\": {} } }");

    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    ASSERT_TRUE((nullptr != logicalCollection));

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

    EXPECT_TRUE((true == !vocbase.lookupView("testView")));
    arangodb::LogicalView::ptr view;
    auto res = arangodb::iresearch::IResearchView::factory().create(view, vocbase, viewCreateJson->slice());
    EXPECT_TRUE((TRI_ERROR_FORBIDDEN == res.errorNumber()));
    EXPECT_TRUE((true == !vocbase.lookupView("testView")));
  }

  // new view definition with links
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\", \"id\": 100 }");
    auto viewCreateJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\", \"id\": 101, \"links\": { \"testCollection\": {} } }");

    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    EXPECT_TRUE((nullptr != logicalCollection));
    EXPECT_TRUE((true == !vocbase.lookupView("testView")));
    EXPECT_TRUE((true == logicalCollection->getIndexes().empty()));
    arangodb::LogicalView::ptr logicalView;
    EXPECT_TRUE((arangodb::iresearch::IResearchView::factory().create(logicalView, vocbase, viewCreateJson->slice()).ok()));
    ASSERT_TRUE((false == !logicalView));
    std::set<TRI_voc_cid_t> cids;
    logicalView->visitCollections([&cids](TRI_voc_cid_t cid)->bool { cids.emplace(cid); return true; });
    EXPECT_TRUE((1 == cids.size()));
    EXPECT_TRUE((false == logicalCollection->getIndexes().empty()));

    arangodb::iresearch::IResearchViewMeta expectedMeta;
    arangodb::velocypack::Builder builder;

    builder.openObject();
    logicalView->properties(builder, arangodb::LogicalDataSource::makeFlags(
                                         arangodb::LogicalDataSource::Serialize::Detailed));
    builder.close();

    auto slice = builder.slice();
    arangodb::iresearch::IResearchViewMeta meta;
    std::string error;
    EXPECT_TRUE((slice.isObject()));
    EXPECT_TRUE((13U == slice.length()));
    EXPECT_TRUE((slice.hasKey("globallyUniqueId") && slice.get("globallyUniqueId").isString() && false == slice.get("globallyUniqueId").copyString().empty()));
    EXPECT_TRUE((slice.get("name").copyString() == "testView"));
    EXPECT_TRUE((slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name()));
    EXPECT_TRUE((false == slice.hasKey("deleted")));
    EXPECT_TRUE((meta.init(slice, error) && expectedMeta == meta));

    auto tmpSlice = slice.get("links");
    EXPECT_TRUE((true == tmpSlice.isObject() && 1 == tmpSlice.length()));
    EXPECT_TRUE((true == tmpSlice.hasKey("testCollection")));
  }
}

TEST_F(IResearchViewTest, test_cleanup) {
  auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
  auto linkJson = arangodb::velocypack::Parser::fromJson("{ \"view\": \"testView\", \"includeAllFields\": true }");
  auto json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\":\"arangosearch\", \"cleanupIntervalStep\":1, \"consolidationIntervalMsec\": 1000 }");
  Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
  auto logicalCollection = vocbase.createCollection(collectionJson->slice());
  ASSERT_TRUE((false == !logicalCollection));
  auto logicalView = vocbase.createView(json->slice());
  ASSERT_TRUE((false == !logicalView));
  auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
  ASSERT_TRUE((false == !view));
  std::shared_ptr<arangodb::Index> index = 
    arangodb::iresearch::IResearchMMFilesLink::factory().instantiate(*logicalCollection, linkJson->slice(), 42, false);
  ASSERT_TRUE((false == !index));
  auto link = std::dynamic_pointer_cast<arangodb::iresearch::IResearchLink>(index);
  ASSERT_TRUE((false == !link));

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
    EXPECT_TRUE((trx.begin().ok()));
    EXPECT_TRUE((link->insert(trx, arangodb::LocalDocumentId(0), doc->slice(), arangodb::Index::OperationMode::normal).ok()));
    EXPECT_TRUE((trx.commit().ok()));
    EXPECT_TRUE(link->commit().ok());
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
    EXPECT_TRUE((trx.begin().ok()));
    EXPECT_TRUE((link->remove(trx, arangodb::LocalDocumentId(0), arangodb::velocypack::Slice::emptyObjectSlice(), arangodb::Index::OperationMode::normal).ok()));
    EXPECT_TRUE((trx.commit().ok()));
    EXPECT_TRUE(link->commit().ok());
  }

  // wait for commit thread
  size_t const MAX_ATTEMPTS = 200;
  size_t attempt = 0;

  while (memory <= view->memory() && attempt < MAX_ATTEMPTS) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    ++attempt;
  }

  // ensure memory was freed
  EXPECT_TRUE(view->memory() <= memory);
}

TEST_F(IResearchViewTest, test_consolidate) {
  auto viewCreateJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\":\"arangosearch\", \"consolidationIntervalMsec\": 1000 }");
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
  auto logicalView = vocbase.createView(viewCreateJson->slice());
  ASSERT_TRUE((false == !logicalView));
  // FIXME TODO write test to check that long-running consolidation aborts on view drop
  // 1. create view with policy that blocks
  // 2. start policy
  // 3. drop view
  // 4. unblock policy
  // 5. ensure view drops immediately
}

TEST_F(IResearchViewTest, test_drop) {
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
  std::string dataPath = ((((irs::utf8_path()/=testFilesystemPath)/=std::string("databases"))/=(std::string("database-") + std::to_string(vocbase.id())))/=std::string("arangosearch-123")).utf8();
  auto json = arangodb::velocypack::Parser::fromJson("{ \
    \"id\": 123, \
    \"name\": \"testView\", \
    \"type\": \"arangosearch\" \
  }");

  EXPECT_TRUE((false == TRI_IsDirectory(dataPath.c_str())));

  auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
  auto logicalCollection = vocbase.createCollection(collectionJson->slice());
  EXPECT_TRUE((nullptr != logicalCollection));
  EXPECT_TRUE((true == !vocbase.lookupView("testView")));
  EXPECT_TRUE((true == logicalCollection->getIndexes().empty()));
  EXPECT_TRUE((false == TRI_IsDirectory(dataPath.c_str()))); // createView(...) will call open()
  auto view = vocbase.createView(json->slice());
  ASSERT_TRUE((false == !view));

  EXPECT_TRUE((true == logicalCollection->getIndexes().empty()));
  EXPECT_TRUE((false == !vocbase.lookupView("testView")));
  EXPECT_TRUE((false == TRI_IsDirectory(dataPath.c_str())));
  EXPECT_TRUE((true == view->drop().ok()));
  EXPECT_TRUE((true == logicalCollection->getIndexes().empty()));
  EXPECT_TRUE((true == !vocbase.lookupView("testView")));
  EXPECT_TRUE((false == TRI_IsDirectory(dataPath.c_str())));
}

TEST_F(IResearchViewTest, test_drop_with_link) {
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
  std::string dataPath = ((((irs::utf8_path()/=testFilesystemPath)/=std::string("databases"))/=(std::string("database-") + std::to_string(vocbase.id())))/=std::string("arangosearch-123")).utf8();
  auto json = arangodb::velocypack::Parser::fromJson("{ \
    \"id\": 123, \
    \"name\": \"testView\", \
    \"type\": \"arangosearch\" \
  }");

  EXPECT_TRUE((false == TRI_IsDirectory(dataPath.c_str())));

  auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
  auto logicalCollection = vocbase.createCollection(collectionJson->slice());
  EXPECT_TRUE((nullptr != logicalCollection));
  EXPECT_TRUE((true == !vocbase.lookupView("testView")));
  EXPECT_TRUE((true == logicalCollection->getIndexes().empty()));
  EXPECT_TRUE((false == TRI_IsDirectory(dataPath.c_str()))); // createView(...) will call open()
  auto view = vocbase.createView(json->slice());
  ASSERT_TRUE((false == !view));

  EXPECT_TRUE((true == logicalCollection->getIndexes().empty()));
  EXPECT_TRUE((false == !vocbase.lookupView("testView")));
  EXPECT_TRUE((false == TRI_IsDirectory(dataPath.c_str())));

  auto links = arangodb::velocypack::Parser::fromJson("{ \
    \"links\": { \"testCollection\": {} } \
  }");

  arangodb::Result res = view->properties(links->slice(), true);
  EXPECT_TRUE(true == res.ok());
  EXPECT_TRUE((false == logicalCollection->getIndexes().empty()));
  dataPath = ((((irs::utf8_path()/=testFilesystemPath)/=std::string("databases"))/=(std::string("database-") + std::to_string(vocbase.id())))/=(std::string("arangosearch-") + std::to_string(logicalCollection->id()) + "_" + std::to_string(arangodb::iresearch::IResearchLinkHelper::find(*logicalCollection, *view)->id()))).utf8();
  EXPECT_TRUE((true == TRI_IsDirectory(dataPath.c_str())));

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

      EXPECT_TRUE((TRI_ERROR_FORBIDDEN == view->drop().errorNumber()));
      EXPECT_TRUE((false == logicalCollection->getIndexes().empty()));
      EXPECT_TRUE((false == !vocbase.lookupView("testView")));
      EXPECT_TRUE((true == TRI_IsDirectory(dataPath.c_str())));
    }

    // authorised (RO collection)
    {
      arangodb::auth::UserMap userMap;
      auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
      user.grantCollection(vocbase.name(), "testCollection", arangodb::auth::Level::RO); // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

      EXPECT_TRUE((true == view->drop().ok()));
      EXPECT_TRUE((true == logicalCollection->getIndexes().empty()));
      EXPECT_TRUE((true == !vocbase.lookupView("testView")));
      EXPECT_TRUE((false == TRI_IsDirectory(dataPath.c_str())));
    }
  }
}

TEST_F(IResearchViewTest, test_drop_collection) {
  auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
  auto viewCreateJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
  auto viewUpdateJson = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection\": { \"includeAllFields\": true } } }");
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
  auto logicalCollection = vocbase.createCollection(collectionJson->slice());
  ASSERT_TRUE((false == !logicalCollection));
  auto logicalView = vocbase.createView(viewCreateJson->slice());
  ASSERT_TRUE((false == !logicalView));
  auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
  ASSERT_TRUE((false == !view));

  EXPECT_TRUE((true == logicalView->properties(viewUpdateJson->slice(), true).ok()));
  EXPECT_TRUE((false == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));

  EXPECT_TRUE((true == logicalCollection->drop().ok()));
  EXPECT_TRUE((true == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));

  EXPECT_TRUE((true == logicalView->drop().ok()));
}

TEST_F(IResearchViewTest, test_drop_cid) {
  static std::vector<std::string> const EMPTY;

  // cid not in list of collections for snapshot (view definition not updated, not persisted)
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
    auto linkJson = arangodb::velocypack::Parser::fromJson("{ \"view\": \"testView\", \"includeAllFields\": true }");
    auto json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\":\"arangosearch\" }");
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    ASSERT_TRUE((false == !logicalCollection));
    auto logicalView = vocbase.createView(json->slice());
    ASSERT_TRUE((false == !logicalView));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
    ASSERT_TRUE((false == !view));
    std::shared_ptr<arangodb::Index> index = 
      arangodb::iresearch::IResearchMMFilesLink::factory().instantiate(*logicalCollection, linkJson->slice(), 42, false);
    ASSERT_TRUE((false == !index));
    auto link = std::dynamic_pointer_cast<arangodb::iresearch::IResearchLink>(index);
    ASSERT_TRUE((false == !link));

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
      EXPECT_TRUE((trx.begin().ok()));
      EXPECT_TRUE((link->insert(trx, arangodb::LocalDocumentId(0), doc->slice(), arangodb::Index::OperationMode::normal).ok()));
      EXPECT_TRUE((trx.commit().ok()));
      EXPECT_TRUE(link->commit().ok());
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
      auto* snapshot = view->snapshot(trx, arangodb::iresearch::IResearchView::SnapshotMode::FindOrCreate);
      EXPECT_TRUE(1 == snapshot->live_docs_count());
    }

    // drop cid 42
    {
      bool persisted = false;
      auto before = StorageEngineMock::before;
      auto restore = irs::make_finally([&before]()->void { StorageEngineMock::before = before; });
      StorageEngineMock::before = [&persisted]()->void { persisted = true; };

      EXPECT_TRUE((true == view->unlink(logicalCollection->id()).ok()));
      EXPECT_TRUE((persisted)); // drop() modifies view meta if cid existed previously
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
      auto* snapshot = view->snapshot(trx, arangodb::iresearch::IResearchView::SnapshotMode::FindOrCreate);
      EXPECT_TRUE(0 == snapshot->live_docs_count());
    }
  }

  // cid in list of collections for snapshot (view definition updated+persisted)
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"id\": 42, \"name\": \"testCollection\" }");
    auto linkJson = arangodb::velocypack::Parser::fromJson("{ \"view\": \"testView\", \"includeAllFields\": true }");
    auto json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\":\"arangosearch\", \"collections\": [ 42 ] }");
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    ASSERT_TRUE((false == !logicalCollection));
    auto logicalView = vocbase.createView(json->slice());
    ASSERT_TRUE((false == !logicalView));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
    ASSERT_TRUE((false == !view));
    std::shared_ptr<arangodb::Index> index = 
      arangodb::iresearch::IResearchMMFilesLink::factory().instantiate(*logicalCollection, linkJson->slice(), 42, false);
    ASSERT_TRUE((false == !index));
    auto link = std::dynamic_pointer_cast<arangodb::iresearch::IResearchLink>(index);
    ASSERT_TRUE((false == !link));

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
      EXPECT_TRUE((trx.begin().ok()));
      EXPECT_TRUE((link->insert(trx, arangodb::LocalDocumentId(0), doc->slice(), arangodb::Index::OperationMode::normal).ok()));
      EXPECT_TRUE((trx.commit().ok()));
      EXPECT_TRUE(link->commit().ok());
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
      auto* snapshot = view->snapshot(trx, arangodb::iresearch::IResearchView::SnapshotMode::FindOrCreate);
      EXPECT_TRUE(1 == snapshot->live_docs_count());
    }

    // drop cid 42
    {
      bool persisted = false;
      auto before = StorageEngineMock::before;
      auto restore = irs::make_finally([&before]()->void { StorageEngineMock::before = before; });
      StorageEngineMock::before = [&persisted]()->void { persisted = true; };

      EXPECT_TRUE((true == view->unlink(logicalCollection->id()).ok()));
      EXPECT_TRUE((persisted)); // drop() modifies view meta if cid existed previously
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
      auto* snapshot = view->snapshot(trx, arangodb::iresearch::IResearchView::SnapshotMode::FindOrCreate);
      EXPECT_TRUE(0 == snapshot->live_docs_count());
    }
  }

  // cid in list of collections for snapshot (view definition updated, not persisted until recovery is complete)
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"id\": 42, \"name\": \"testCollection\" }");
    auto linkJson = arangodb::velocypack::Parser::fromJson("{ \"view\": \"testView\", \"includeAllFields\": true }");
    auto json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\":\"arangosearch\", \"collections\": [ 42 ] }");
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    ASSERT_TRUE((false == !logicalCollection));
    auto logicalView = vocbase.createView(json->slice());
    ASSERT_TRUE((false == !logicalView));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
    ASSERT_TRUE((false == !view));
    std::shared_ptr<arangodb::Index> index = 
      arangodb::iresearch::IResearchMMFilesLink::factory().instantiate(*logicalCollection, linkJson->slice(), __LINE__, false);
    ASSERT_TRUE((false == !index));
    auto link = std::dynamic_pointer_cast<arangodb::iresearch::IResearchLink>(index);
    ASSERT_TRUE((false == !link));

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
      EXPECT_TRUE((trx.begin().ok()));
      EXPECT_TRUE((link->insert(trx, arangodb::LocalDocumentId(0), doc->slice(), arangodb::Index::OperationMode::normal).ok()));
      EXPECT_TRUE((trx.commit().ok()));
      EXPECT_TRUE(link->commit().ok());
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
      auto* snapshot = view->snapshot(trx, arangodb::iresearch::IResearchView::SnapshotMode::FindOrCreate);
      EXPECT_TRUE(1 == snapshot->live_docs_count());
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

      EXPECT_TRUE((true == view->unlink(logicalCollection->id()).ok()));
      EXPECT_TRUE((!persisted)); // drop() modifies view meta if cid existed previously (but not persisted until after recovery)
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
      auto* snapshot = view->snapshot(trx, arangodb::iresearch::IResearchView::SnapshotMode::FindOrCreate);
      EXPECT_TRUE(0 == snapshot->live_docs_count());
    }

    // collection not in view after drop (in recovery)
    {
      std::unordered_set<TRI_voc_cid_t> expected;
      std::set<TRI_voc_cid_t> actual;

      EXPECT_TRUE((view->visitCollections([&actual](TRI_voc_cid_t cid)->bool { actual.emplace(cid); return true; })));

      for (auto& cid: expected) {
        EXPECT_TRUE((1 == actual.erase(cid)));
      }

      EXPECT_TRUE((actual.empty()));
    }
  }

  // cid in list of collections for snapshot (view definition persist failure)
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"id\": 42, \"name\": \"testCollection\" }");
    auto linkJson = arangodb::velocypack::Parser::fromJson("{ \"view\": \"testView\", \"includeAllFields\": true }");
    auto json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\":\"arangosearch\", \"collections\": [ 42 ] }");
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    ASSERT_TRUE((false == !logicalCollection));
    auto logicalView = vocbase.createView(json->slice());
    ASSERT_TRUE((false == !logicalView));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
    ASSERT_TRUE((false == !view));
    std::shared_ptr<arangodb::Index> index = 
      arangodb::iresearch::IResearchMMFilesLink::factory().instantiate(*logicalCollection, linkJson->slice(), __LINE__, false);
    ASSERT_TRUE((false == !index));
    auto link = std::dynamic_pointer_cast<arangodb::iresearch::IResearchLink>(index);
    ASSERT_TRUE((false == !link));

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
      EXPECT_TRUE((trx.begin().ok()));
      EXPECT_TRUE((link->insert(trx, arangodb::LocalDocumentId(0), doc->slice(), arangodb::Index::OperationMode::normal).ok()));
      EXPECT_TRUE((trx.commit().ok()));
      EXPECT_TRUE(link->commit().ok());
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
      auto* snapshot = view->snapshot(trx, arangodb::iresearch::IResearchView::SnapshotMode::FindOrCreate);
      EXPECT_TRUE(1 == snapshot->live_docs_count());
    }

    // drop cid 42
    {
      auto before = StorageEngineMock::before;
      auto restore = irs::make_finally([&before]()->void { StorageEngineMock::before = before; });
      StorageEngineMock::before = []()->void { throw std::exception(); };

      EXPECT_TRUE((true != view->unlink(logicalCollection->id()).ok()));
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
      auto* snapshot = view->snapshot(trx, arangodb::iresearch::IResearchView::SnapshotMode::FindOrCreate);
      EXPECT_TRUE(1 == snapshot->live_docs_count());
    }

    // collection in view after drop failure
    {
      std::unordered_set<TRI_voc_cid_t> expected = { logicalCollection->id() };
      std::set<TRI_voc_cid_t> actual;

      EXPECT_TRUE((view->visitCollections([&actual](TRI_voc_cid_t cid)->bool { actual.emplace(cid); return true; })));

      for (auto& cid: expected) {
        EXPECT_TRUE((1 == actual.erase(cid)));
      }

      EXPECT_TRUE((actual.empty()));
    }
  }

  // cid in list of collections for snapshot (view definition persist failure on recovery completion)
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"id\": 42, \"name\": \"testCollection\" }");
    auto linkJson = arangodb::velocypack::Parser::fromJson("{ \"view\": \"testView\", \"includeAllFields\": true }");
    auto json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\":\"arangosearch\", \"collections\": [ 42 ] }");
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    ASSERT_TRUE((false == !logicalCollection));
    auto logicalView = vocbase.createView(json->slice());
    ASSERT_TRUE((false == !logicalView));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
    ASSERT_TRUE((false == !view));
    std::shared_ptr<arangodb::Index> index = 
      arangodb::iresearch::IResearchMMFilesLink::factory().instantiate(*logicalCollection, linkJson->slice(), __LINE__, false);
    ASSERT_TRUE((false == !index));
    auto link = std::dynamic_pointer_cast<arangodb::iresearch::IResearchLink>(index);
    ASSERT_TRUE((false == !link));

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
      EXPECT_TRUE((trx.begin().ok()));
      EXPECT_TRUE((link->insert(trx, arangodb::LocalDocumentId(0), doc->slice(), arangodb::Index::OperationMode::normal).ok()));
      EXPECT_TRUE((trx.commit().ok()));
      EXPECT_TRUE(link->commit().ok());
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
      auto* snapshot = view->snapshot(trx, arangodb::iresearch::IResearchView::SnapshotMode::FindOrCreate);
      EXPECT_TRUE(1 == snapshot->live_docs_count());
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

      EXPECT_TRUE((true == view->unlink(logicalCollection->id()).ok()));
      EXPECT_TRUE((!persisted)); // drop() modifies view meta if cid existed previously (but not persisted until after recovery)
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
      auto* snapshot = view->snapshot(trx, arangodb::iresearch::IResearchView::SnapshotMode::FindOrCreate);
      EXPECT_TRUE(0 == snapshot->live_docs_count());
    }

    // collection in view after drop failure
    {
      std::unordered_set<TRI_voc_cid_t> expected;
      std::set<TRI_voc_cid_t> actual;

      EXPECT_TRUE((view->visitCollections([&actual](TRI_voc_cid_t cid)->bool { actual.emplace(cid); return true; })));

      for (auto& cid: expected) {
        EXPECT_TRUE((1 == actual.erase(cid)));
      }

      EXPECT_TRUE((actual.empty()));
    }

    // persistence fails during execution of callback
    {
      auto before = StorageEngineMock::before;
      auto restore = irs::make_finally([&before]()->void { StorageEngineMock::before = before; });
      StorageEngineMock::before = []()->void { throw std::exception(); };
      auto* feature = arangodb::application_features::ApplicationServer::lookupFeature<
        arangodb::DatabaseFeature
      >("Database");

      EXPECT_NO_THROW((feature->recoveryDone()));
    }
  }
}

TEST_F(IResearchViewTest, test_drop_database) {
  auto viewCreateJson = arangodb::velocypack::Parser::fromJson("{ \"id\": \"42\", \"name\": \"testView\", \"type\": \"arangosearch\" }");
  auto* databaseFeature = arangodb::application_features::ApplicationServer::getFeature<arangodb::DatabaseFeature>("Database");
  ASSERT_TRUE((nullptr != databaseFeature));

  size_t beforeCount = 0;
  auto before = StorageEngineMock::before;
  auto restore = irs::make_finally([&before]()->void { StorageEngineMock::before = before; });
  StorageEngineMock::before = [&beforeCount]()->void { ++beforeCount; };

  TRI_vocbase_t* vocbase; // will be owned by DatabaseFeature
  ASSERT_TRUE((TRI_ERROR_NO_ERROR == databaseFeature->createDatabase(0, "testDatabase" TOSTRING(__LINE__), vocbase)));
  ASSERT_TRUE((nullptr != vocbase));

  beforeCount = 0; // reset before call to StorageEngine::createView(...)
  auto logicalView = vocbase->createView(viewCreateJson->slice());
  ASSERT_TRUE((false == !logicalView));
  EXPECT_TRUE((1 + 1 == beforeCount)); // +1 for StorageEngineMock::createView(...), +1 for StorageEngineMock::getViewProperties(...)

  beforeCount = 0; // reset before call to StorageEngine::dropView(...)
  EXPECT_TRUE((TRI_ERROR_NO_ERROR == databaseFeature->dropDatabase(vocbase->id(), true, true)));
  EXPECT_TRUE((1 == beforeCount));
}

TEST_F(IResearchViewTest, test_instantiate) {
  // valid version
  {
    auto json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\", \"version\": 1 }");
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    arangodb::LogicalView::ptr view;
    EXPECT_TRUE((arangodb::iresearch::IResearchView::factory().instantiate(view, vocbase, json->slice(), 0).ok()));
    EXPECT_TRUE((false == !view));
  }

  // intantiate view from old version
  {
    auto json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\", \"version\": 0 }");
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    arangodb::LogicalView::ptr view;
    EXPECT_TRUE(arangodb::iresearch::IResearchView::factory().instantiate(view, vocbase, json->slice(), 0).ok());
    EXPECT_TRUE(nullptr != view);
  }

  // unsupported version
  {
    auto json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\", \"version\": 123456789 }");
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    arangodb::LogicalView::ptr view;
    EXPECT_TRUE((!arangodb::iresearch::IResearchView::factory().instantiate(view, vocbase, json->slice(), 0).ok()));
    EXPECT_TRUE((true == !view));
  }
}

TEST_F(IResearchViewTest, test_truncate_cid) {
  static std::vector<std::string> const EMPTY;

  // cid not in list of collections for snapshot (view definition not updated, not persisted)
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
    auto linkJson = arangodb::velocypack::Parser::fromJson("{ \"view\": \"testView\", \"includeAllFields\": true }");
    auto json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\":\"arangosearch\" }");
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    ASSERT_TRUE((false == !logicalCollection));
    auto logicalView = vocbase.createView(json->slice());
    ASSERT_TRUE((false == !logicalView));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
    ASSERT_TRUE((false == !view));
    std::shared_ptr<arangodb::Index> index = 
      arangodb::iresearch::IResearchMMFilesLink::factory().instantiate(*logicalCollection, linkJson->slice(), 42, false);
    ASSERT_TRUE((false == !index));
    auto link = std::dynamic_pointer_cast<arangodb::iresearch::IResearchLink>(index);
    ASSERT_TRUE((false == !link));

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
      EXPECT_TRUE((trx.begin().ok()));
      EXPECT_TRUE((link->insert(trx, arangodb::LocalDocumentId(0), doc->slice(), arangodb::Index::OperationMode::normal).ok()));
      EXPECT_TRUE((trx.commit().ok()));
      EXPECT_TRUE(link->commit().ok());
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
      auto* snapshot = view->snapshot(trx, arangodb::iresearch::IResearchView::SnapshotMode::FindOrCreate);
      EXPECT_TRUE(1 == snapshot->live_docs_count());
    }

    // truncate cid 42
    {
      bool persisted = false;
      auto before = StorageEngineMock::before;
      auto restore = irs::make_finally([&before]()->void { StorageEngineMock::before = before; });
      StorageEngineMock::before = [&persisted]()->void { persisted = true; };

      EXPECT_TRUE((true == view->unlink(logicalCollection->id()).ok()));
      EXPECT_TRUE((persisted)); // truncate() modifies view meta if cid existed previously
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
      auto* snapshot = view->snapshot(trx, arangodb::iresearch::IResearchView::SnapshotMode::FindOrCreate);
      EXPECT_TRUE(0 == snapshot->live_docs_count());
    }
  }

  // cid in list of collections for snapshot (view definition not updated+persisted)
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"id\": 42, \"name\": \"testCollection\" }");
    auto linkJson = arangodb::velocypack::Parser::fromJson("{ \"view\": \"testView\", \"includeAllFields\": true }");
    auto json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\":\"arangosearch\", \"collections\": [ 42 ] }");
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    ASSERT_TRUE((false == !logicalCollection));
    auto logicalView = vocbase.createView(json->slice());
    ASSERT_TRUE((false == !logicalView));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
    ASSERT_TRUE((false == !view));
    std::shared_ptr<arangodb::Index> index = 
      arangodb::iresearch::IResearchMMFilesLink::factory().instantiate(*logicalCollection, linkJson->slice(), 42, false);
    ASSERT_TRUE((false == !index));
    auto link = std::dynamic_pointer_cast<arangodb::iresearch::IResearchLink>(index);
    ASSERT_TRUE((false == !link));

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
      EXPECT_TRUE((trx.begin().ok()));
      EXPECT_TRUE((link->insert(trx, arangodb::LocalDocumentId(0), doc->slice(), arangodb::Index::OperationMode::normal).ok()));
      EXPECT_TRUE((trx.commit().ok()));
      EXPECT_TRUE(link->commit().ok());
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
      auto* snapshot = view->snapshot(trx, arangodb::iresearch::IResearchView::SnapshotMode::FindOrCreate);
      EXPECT_TRUE(1 == snapshot->live_docs_count());
    }

    // truncate cid 42
    {
      bool persisted = false;
      auto before = StorageEngineMock::before;
      auto restore = irs::make_finally([&before]()->void { StorageEngineMock::before = before; });
      StorageEngineMock::before = [&persisted]()->void { persisted = true; };

      EXPECT_TRUE((true == view->unlink(logicalCollection->id()).ok()));
      EXPECT_TRUE((persisted)); // truncate() modifies view meta if cid existed previously
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
      auto* snapshot = view->snapshot(trx, arangodb::iresearch::IResearchView::SnapshotMode::FindOrCreate);
      EXPECT_TRUE(0 == snapshot->live_docs_count());
    }
  }
}

TEST_F(IResearchViewTest, test_emplace_cid) {
  // emplace (already in list)
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"id\": 42, \"name\": \"testCollection\" }");
    auto linkJson = arangodb::velocypack::Parser::fromJson("{ \"view\": \"testView\", \"includeAllFields\": true }");
    auto json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\":\"arangosearch\", \"collections\": [ 42 ] }");
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    ASSERT_TRUE((false == !logicalCollection));
    auto logicalView = vocbase.createView(json->slice());
    ASSERT_TRUE((false == !logicalView));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
    ASSERT_TRUE((false == !view));
    std::shared_ptr<arangodb::Index> index = 
      arangodb::iresearch::IResearchMMFilesLink::factory().instantiate(*logicalCollection, linkJson->slice(), 42, false);
    ASSERT_TRUE((false == !index));
    auto link = std::dynamic_pointer_cast<arangodb::iresearch::IResearchLink>(index);
    ASSERT_TRUE((false == !link));

    // collection in view before
    {
      std::unordered_set<TRI_voc_cid_t> expected = { 42 };
      std::set<TRI_voc_cid_t> actual;

      EXPECT_TRUE((view->visitCollections([&actual](TRI_voc_cid_t cid)->bool { actual.emplace(cid); return true; })));

      for (auto& cid: expected) {
        EXPECT_TRUE((1 == actual.erase(cid)));
      }

      EXPECT_TRUE((actual.empty()));
    }

    // emplace cid 42
    {
      bool persisted = false;
      auto before = StorageEngineMock::before;
      auto restore = irs::make_finally([&before]()->void { StorageEngineMock::before = before; });
      StorageEngineMock::before = [&persisted]()->void { persisted = true; };

      EXPECT_TRUE((false == view->link(link->self()).ok()));
      EXPECT_TRUE((!persisted)); // emplace() does not modify view meta if cid existed previously
    }

    // collection in view after
    {
      std::unordered_set<TRI_voc_cid_t> expected = { 42 };
      std::set<TRI_voc_cid_t> actual;

      EXPECT_TRUE((view->visitCollections([&actual](TRI_voc_cid_t cid)->bool { actual.emplace(cid); return true; })));

      for (auto& cid: expected) {
        EXPECT_TRUE((1 == actual.erase(cid)));
      }

      EXPECT_TRUE((actual.empty()));
    }
  }

  // emplace (not in list)
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"id\": 42, \"name\": \"testCollection\" }");
    auto json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\":\"arangosearch\" }");
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    ASSERT_TRUE((false == !logicalCollection));
    auto logicalView = vocbase.createView(json->slice());
    ASSERT_TRUE((false == !logicalView));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
    ASSERT_TRUE((false == !view));

    // collection in view before
    {
      std::unordered_set<TRI_voc_cid_t> expected;
      std::set<TRI_voc_cid_t> actual;

      EXPECT_TRUE((view->visitCollections([&actual](TRI_voc_cid_t cid)->bool { actual.emplace(cid); return true; })));

      for (auto& cid: expected) {
        EXPECT_TRUE((1 == actual.erase(cid)));
      }

      EXPECT_TRUE((actual.empty()));
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

      EXPECT_TRUE((true == view->link(asyncLinkPtr).ok()));
      EXPECT_TRUE((persisted)); // emplace() modifies view meta if cid did not exist previously
    }

    // collection in view after
    {
      std::unordered_set<TRI_voc_cid_t> expected = { 42 };
      std::set<TRI_voc_cid_t> actual;

      EXPECT_TRUE((view->visitCollections([&actual](TRI_voc_cid_t cid)->bool { actual.emplace(cid); return true; })));

      for (auto& cid: expected) {
        EXPECT_TRUE((1 == actual.erase(cid)));
      }

      EXPECT_TRUE((actual.empty()));
    }
  }

  // emplace (not in list, not persisted until recovery is complete)
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"id\": 42, \"name\": \"testCollection\" }");
    auto json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\":\"arangosearch\"  }");
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    ASSERT_TRUE((false == !logicalCollection));
    auto logicalView = vocbase.createView(json->slice());
    ASSERT_TRUE((false == !logicalView));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
    ASSERT_TRUE((false == !view));

    // collection in view before
    {
      std::unordered_set<TRI_voc_cid_t> expected;
      std::set<TRI_voc_cid_t> actual;

      EXPECT_TRUE((view->visitCollections([&actual](TRI_voc_cid_t cid)->bool { actual.emplace(cid); return true; })));

      for (auto& cid: expected) {
        EXPECT_TRUE((1 == actual.erase(cid)));
      }

      EXPECT_TRUE((actual.empty()));
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

      EXPECT_TRUE((true == view->link(asyncLinkPtr).ok()));
      EXPECT_TRUE((!persisted)); // emplace() modifies view meta if cid did not exist previously (but not persisted until after recovery)
    }

    // collection in view after
    {
      std::unordered_set<TRI_voc_cid_t> expected = { 42 };
      std::set<TRI_voc_cid_t> actual;

      EXPECT_TRUE((view->visitCollections([&actual](TRI_voc_cid_t cid)->bool { actual.emplace(cid); return true; })));

      for (auto& cid: expected) {
        EXPECT_TRUE((1 == actual.erase(cid)));
      }

      EXPECT_TRUE((actual.empty()));
    }
  }

  // emplace (not in list, view definition persist failure)
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"id\": 42, \"name\": \"testCollection\" }");
    auto json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\":\"arangosearch\" }");
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    ASSERT_TRUE((false == !logicalCollection));
    auto logicalView = vocbase.createView(json->slice());
    ASSERT_TRUE((false == !logicalView));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
    ASSERT_TRUE((false == !view));

    // collection in view before
    {
      std::unordered_set<TRI_voc_cid_t> expected;
      std::set<TRI_voc_cid_t> actual;

      EXPECT_TRUE((view->visitCollections([&actual](TRI_voc_cid_t cid)->bool { actual.emplace(cid); return true; })));

      for (auto& cid: expected) {
        EXPECT_TRUE((1 == actual.erase(cid)));
      }

      EXPECT_TRUE((actual.empty()));
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

      EXPECT_TRUE((false == view->link(asyncLinkPtr).ok()));
    }

    // collection in view after
    {
      std::unordered_set<TRI_voc_cid_t> expected;
      std::set<TRI_voc_cid_t> actual;

      EXPECT_TRUE((view->visitCollections([&actual](TRI_voc_cid_t cid)->bool { actual.emplace(cid); return true; })));

      for (auto& cid: expected) {
        EXPECT_TRUE((1 == actual.erase(cid)));
      }

      EXPECT_TRUE((actual.empty()));
    }
  }

  // emplace (not in list, view definition persist failure on recovery completion)
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"id\": 42, \"name\": \"testCollection\" }");
    auto json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\":\"arangosearch\" }");
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    ASSERT_TRUE((false == !logicalCollection));
    auto logicalView = vocbase.createView(json->slice());
    ASSERT_TRUE((false == !logicalView));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
    ASSERT_TRUE((false == !view));

    // collection in view before
    {
      std::unordered_set<TRI_voc_cid_t> expected;
      std::set<TRI_voc_cid_t> actual;

      EXPECT_TRUE((view->visitCollections([&actual](TRI_voc_cid_t cid)->bool { actual.emplace(cid); return true; })));

      for (auto& cid: expected) {
        EXPECT_TRUE((1 == actual.erase(cid)));
      }

      EXPECT_TRUE((actual.empty()));
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

      EXPECT_TRUE((true == view->link(asyncLinkPtr).ok()));
      EXPECT_TRUE((!persisted)); // emplace() modifies view meta if cid did not exist previously (but not persisted until after recovery)
    }

    // collection in view after
    {
      std::unordered_set<TRI_voc_cid_t> expected = { 42 };
      std::set<TRI_voc_cid_t> actual;

      EXPECT_TRUE((view->visitCollections([&actual](TRI_voc_cid_t cid)->bool { actual.emplace(cid); return true; })));

      for (auto& cid: expected) {
        EXPECT_TRUE((1 == actual.erase(cid)));
      }

      EXPECT_TRUE((actual.empty()));
    }

    // persistence fails during execution of callback
    {
      auto before = StorageEngineMock::before;
      auto restore = irs::make_finally([&before]()->void { StorageEngineMock::before = before; });
      StorageEngineMock::before = []()->void { throw std::exception(); };
      auto* feature = arangodb::application_features::ApplicationServer::lookupFeature<
        arangodb::DatabaseFeature
      >("Database");

      EXPECT_NO_THROW((feature->recoveryDone()));
    }
  }
}

TEST_F(IResearchViewTest, test_insert) {
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
    ASSERT_TRUE((false == !logicalCollection));
    auto viewImpl = vocbase.createView(viewJson->slice());
    EXPECT_TRUE((false == !viewImpl));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(viewImpl.get());
    EXPECT_TRUE((nullptr != view));
    {
      // ensure the data store directory exists with data
      auto before = StorageEngineMock::inRecoveryResult;
      StorageEngineMock::inRecoveryResult = false;
      auto restore = irs::make_finally([&before]()->void { StorageEngineMock::inRecoveryResult = before; });
      std::shared_ptr<arangodb::Index> index = 
        arangodb::iresearch::IResearchMMFilesLink::factory().instantiate(*logicalCollection, linkJson->slice(), 42, false);
      auto link = std::dynamic_pointer_cast<arangodb::iresearch::IResearchLink>(index);
      ASSERT_TRUE((false == !link));
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
      EXPECT_TRUE((trx.begin().ok()));
      EXPECT_TRUE((link->insert(trx, arangodb::LocalDocumentId(1), docJson->slice(), arangodb::Index::OperationMode::normal).ok()));
      EXPECT_TRUE((trx.commit().ok()));
      EXPECT_TRUE((link->commit().ok()));
    }
    std::shared_ptr<arangodb::Index> index = 
      arangodb::iresearch::IResearchMMFilesLink::factory().instantiate(*logicalCollection, linkJson->slice(), 42, false);
    ASSERT_TRUE((false == !index));
    auto link = std::dynamic_pointer_cast<arangodb::iresearch::IResearchLink>(index);
    ASSERT_TRUE((false == !link));

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
      EXPECT_TRUE((trx.begin().ok()));
      EXPECT_TRUE((link->insert(trx, arangodb::LocalDocumentId(1), docJson->slice(), arangodb::Index::OperationMode::normal).ok()));
      EXPECT_TRUE((link->insert(trx, arangodb::LocalDocumentId(2), docJson->slice(), arangodb::Index::OperationMode::normal).ok()));
      EXPECT_TRUE((link->insert(trx, arangodb::LocalDocumentId(1), docJson->slice(), arangodb::Index::OperationMode::normal).ok())); // 2nd time
      EXPECT_TRUE((link->insert(trx, arangodb::LocalDocumentId(2), docJson->slice(), arangodb::Index::OperationMode::normal).ok())); // 2nd time
      EXPECT_TRUE((trx.commit().ok()));
      EXPECT_TRUE(link->commit().ok());
    }

    arangodb::transaction::Methods trx(
      arangodb::transaction::StandaloneContext::Create(vocbase),
      EMPTY,
      EMPTY,
      EMPTY,
      arangodb::transaction::Options()
    );
    auto* snapshot = view->snapshot(trx, arangodb::iresearch::IResearchView::SnapshotMode::FindOrCreate);
    EXPECT_TRUE(2 == snapshot->live_docs_count());
  }

  // in recovery batch (removes cid+rid before insert)
  {
    auto before = StorageEngineMock::inRecoveryResult;
    StorageEngineMock::inRecoveryResult = true;
    auto restore = irs::make_finally([&before]()->void { StorageEngineMock::inRecoveryResult = before; });
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    ASSERT_TRUE((false == !logicalCollection));
    auto viewImpl = vocbase.createView(viewJson->slice());
    EXPECT_TRUE((false == !viewImpl));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(viewImpl.get());
    EXPECT_TRUE((nullptr != view));
    {
      // ensure the data store directory exists with data
      auto before = StorageEngineMock::inRecoveryResult;
      StorageEngineMock::inRecoveryResult = false;
      auto restore = irs::make_finally([&before]()->void { StorageEngineMock::inRecoveryResult = before; });
      std::shared_ptr<arangodb::Index> index = 
        arangodb::iresearch::IResearchMMFilesLink::factory().instantiate(*logicalCollection, linkJson->slice(), 42, false);
      auto link = std::dynamic_pointer_cast<arangodb::iresearch::IResearchLink>(index);
      ASSERT_TRUE((false == !link));
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
      EXPECT_TRUE((trx.begin().ok()));
      EXPECT_TRUE((link->insert(trx, arangodb::LocalDocumentId(1), docJson->slice(), arangodb::Index::OperationMode::normal).ok()));
      EXPECT_TRUE((trx.commit().ok()));
      EXPECT_TRUE((link->commit().ok()));
    }
    std::shared_ptr<arangodb::Index> index = 
      arangodb::iresearch::IResearchMMFilesLink::factory().instantiate(*logicalCollection, linkJson->slice(), 42, false);
    ASSERT_TRUE((false == !index));
    auto link = std::dynamic_pointer_cast<arangodb::iresearch::IResearchLink>(index);
    ASSERT_TRUE((false == !link));

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
      EXPECT_TRUE((trx.begin().ok()));
      link->batchInsert(trx, batch, taskQueuePtr);
      link->batchInsert(trx, batch, taskQueuePtr); // 2nd time
      EXPECT_TRUE((TRI_ERROR_NO_ERROR == taskQueue.status()));
      EXPECT_TRUE((trx.commit().ok()));
      EXPECT_TRUE(link->commit().ok());
    }

      arangodb::transaction::Methods trx(
        arangodb::transaction::StandaloneContext::Create(vocbase),
        EMPTY,
        EMPTY,
        EMPTY,
        arangodb::transaction::Options()
      );
    auto* snapshot = view->snapshot(trx, arangodb::iresearch::IResearchView::SnapshotMode::FindOrCreate);
    EXPECT_TRUE((2 == snapshot->live_docs_count()));
  }

  // not in recovery (FindOrCreate)
  {
    StorageEngineMock::inRecoveryResult = false;
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    ASSERT_TRUE((false == !logicalCollection));
    auto viewImpl = vocbase.createView(viewJson->slice());
    EXPECT_TRUE((false == !viewImpl));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(viewImpl.get());
    EXPECT_TRUE((nullptr != view));
    std::shared_ptr<arangodb::Index> index = 
      arangodb::iresearch::IResearchMMFilesLink::factory().instantiate(*logicalCollection, linkJson->slice(), 42, false);
    ASSERT_TRUE((false == !index));
    auto link = std::dynamic_pointer_cast<arangodb::iresearch::IResearchLink>(index);
    ASSERT_TRUE((false == !link));

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
      EXPECT_TRUE((trx.begin().ok()));
      EXPECT_TRUE((link->insert(trx, arangodb::LocalDocumentId(1), docJson->slice(), arangodb::Index::OperationMode::normal).ok()));
      EXPECT_TRUE((link->insert(trx, arangodb::LocalDocumentId(2), docJson->slice(), arangodb::Index::OperationMode::normal).ok()));
      EXPECT_TRUE((link->insert(trx, arangodb::LocalDocumentId(1), docJson->slice(), arangodb::Index::OperationMode::normal).ok())); // 2nd time
      EXPECT_TRUE((link->insert(trx, arangodb::LocalDocumentId(2), docJson->slice(), arangodb::Index::OperationMode::normal).ok())); // 2nd time
      EXPECT_TRUE((trx.commit().ok()));
      EXPECT_TRUE(link->commit().ok());
    }

    arangodb::transaction::Methods trx(
      arangodb::transaction::StandaloneContext::Create(vocbase),
      EMPTY,
      EMPTY,
      EMPTY,
      arangodb::transaction::Options()
    );
    auto* snapshot = view->snapshot(trx, arangodb::iresearch::IResearchView::SnapshotMode::FindOrCreate);
    EXPECT_TRUE((4 == snapshot->docs_count()));
  }

  // not in recovery (SyncAndReplace)
  {
    StorageEngineMock::inRecoveryResult = false;
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    ASSERT_TRUE((false == !logicalCollection));
    auto viewImpl = vocbase.createView(viewJson->slice());
    EXPECT_TRUE((false == !viewImpl));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(viewImpl.get());
    EXPECT_TRUE((nullptr != view));
    EXPECT_TRUE(view->category() == arangodb::LogicalView::category());
    std::shared_ptr<arangodb::Index> index = 
      arangodb::iresearch::IResearchMMFilesLink::factory().instantiate(*logicalCollection, linkJson->slice(), 42, false);
    ASSERT_TRUE((false == !index));
    auto link = std::dynamic_pointer_cast<arangodb::iresearch::IResearchLink>(index);
    ASSERT_TRUE((false == !link));

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
      EXPECT_TRUE((trx.begin().ok()));
      EXPECT_TRUE((link->insert(trx, arangodb::LocalDocumentId(1), docJson->slice(), arangodb::Index::OperationMode::normal).ok()));
      EXPECT_TRUE((link->insert(trx, arangodb::LocalDocumentId(2), docJson->slice(), arangodb::Index::OperationMode::normal).ok()));
      EXPECT_TRUE((link->insert(trx, arangodb::LocalDocumentId(1), docJson->slice(), arangodb::Index::OperationMode::normal).ok())); // 2nd time
      EXPECT_TRUE((link->insert(trx, arangodb::LocalDocumentId(2), docJson->slice(), arangodb::Index::OperationMode::normal).ok())); // 2nd time
      EXPECT_TRUE((trx.commit().ok()));
    }

    arangodb::transaction::Methods trx(
      arangodb::transaction::StandaloneContext::Create(vocbase),
      EMPTY,
      EMPTY,
      EMPTY,
      arangodb::transaction::Options()
    );
    auto* snapshot = view->snapshot(trx, arangodb::iresearch::IResearchView::SnapshotMode::SyncAndReplace);
    EXPECT_TRUE((4 == snapshot->docs_count()));
  }

  // not in recovery : single operation transaction
  {
    StorageEngineMock::inRecoveryResult = false;
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    ASSERT_TRUE((false == !logicalCollection));
    auto viewImpl = vocbase.createView(viewJson->slice());
    EXPECT_TRUE((false == !viewImpl));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(viewImpl.get());
    EXPECT_TRUE((nullptr != view));
    EXPECT_TRUE(view->category() == arangodb::LogicalView::category());
    std::shared_ptr<arangodb::Index> index = 
      arangodb::iresearch::IResearchMMFilesLink::factory().instantiate(*logicalCollection, linkJson->slice(), 42, false);
    ASSERT_TRUE((false == !index));
    auto link = std::dynamic_pointer_cast<arangodb::iresearch::IResearchLink>(index);
    ASSERT_TRUE((false == !link));

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
      EXPECT_TRUE((trx.begin().ok()));
      EXPECT_TRUE((link->insert(trx, arangodb::LocalDocumentId(1), docJson->slice(), arangodb::Index::OperationMode::normal).ok()));
      EXPECT_TRUE((trx.commit().ok()));
    }

    arangodb::transaction::Methods trx(
      arangodb::transaction::StandaloneContext::Create(vocbase),
      EMPTY,
      EMPTY,
      EMPTY,
      arangodb::transaction::Options()
    );
    auto* snapshot = view->snapshot(trx, arangodb::iresearch::IResearchView::SnapshotMode::SyncAndReplace);
    EXPECT_TRUE((1 == snapshot->docs_count()));
  }

  // not in recovery batch (FindOrCreate)
  {
    StorageEngineMock::inRecoveryResult = false;
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    ASSERT_TRUE((false == !logicalCollection));
    auto viewImpl = vocbase.createView(viewJson->slice());
    EXPECT_TRUE((false == !viewImpl));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(viewImpl.get());
    EXPECT_TRUE((nullptr != view));
    std::shared_ptr<arangodb::Index> index = 
      arangodb::iresearch::IResearchMMFilesLink::factory().instantiate(*logicalCollection, linkJson->slice(), 42, false);
    ASSERT_TRUE((false == !index));
    auto link = std::dynamic_pointer_cast<arangodb::iresearch::IResearchLink>(index);
    ASSERT_TRUE((false == !link));

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
      EXPECT_TRUE((trx.begin().ok()));
      link->batchInsert(trx, batch, taskQueuePtr);
      link->batchInsert(trx, batch, taskQueuePtr); // 2nd time
      EXPECT_TRUE((TRI_ERROR_NO_ERROR == taskQueue.status()));
      EXPECT_TRUE((trx.commit().ok()));
      EXPECT_TRUE(link->commit().ok());
    }

    arangodb::transaction::Methods trx(
      arangodb::transaction::StandaloneContext::Create(vocbase),
      EMPTY,
      EMPTY,
      EMPTY,
      arangodb::transaction::Options()
    );
    auto* snapshot = view->snapshot(trx, arangodb::iresearch::IResearchView::SnapshotMode::FindOrCreate);
    EXPECT_TRUE((4 == snapshot->docs_count()));
  }

  // not in recovery batch (SyncAndReplace)
  {
    StorageEngineMock::inRecoveryResult = false;
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    ASSERT_TRUE((false == !logicalCollection));
    auto viewImpl = vocbase.createView(viewJson->slice());
    EXPECT_TRUE((false == !viewImpl));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(viewImpl.get());
    EXPECT_TRUE((nullptr != view));
    std::shared_ptr<arangodb::Index> index = 
      arangodb::iresearch::IResearchMMFilesLink::factory().instantiate(*logicalCollection, linkJson->slice(), 42, false);
    ASSERT_TRUE((false == !index));
    auto link = std::dynamic_pointer_cast<arangodb::iresearch::IResearchLink>(index);
    ASSERT_TRUE((false == !link));

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
      EXPECT_TRUE((trx.begin().ok()));
      link->batchInsert(trx, batch, taskQueuePtr);
      link->batchInsert(trx, batch, taskQueuePtr); // 2nd time
      EXPECT_TRUE((TRI_ERROR_NO_ERROR == taskQueue.status()));
      EXPECT_TRUE((trx.commit().ok()));
    }

    arangodb::transaction::Methods trx(
      arangodb::transaction::StandaloneContext::Create(vocbase),
      EMPTY,
      EMPTY,
      EMPTY,
      arangodb::transaction::Options()
    );
    auto* snapshot = view->snapshot(trx, arangodb::iresearch::IResearchView::SnapshotMode::SyncAndReplace);
    EXPECT_TRUE((4 == snapshot->docs_count()));
  }
}

TEST_F(IResearchViewTest, test_open) {
  // default data path
  {
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    std::string dataPath = ((((irs::utf8_path()/=testFilesystemPath)/=std::string("databases"))/=(std::string("database-") + std::to_string(vocbase.id())))/=std::string("arangosearch-123")).utf8();
    auto json = arangodb::velocypack::Parser::fromJson("{ \"id\": 123, \"name\": \"testView\", \"type\": \"testType\" }");

    EXPECT_TRUE((false == TRI_IsDirectory(dataPath.c_str())));
    arangodb::LogicalView::ptr view;
    ASSERT_TRUE((arangodb::iresearch::IResearchView::factory().instantiate(view, vocbase, json->slice(), 0).ok()));
    EXPECT_TRUE((false == !view));
    EXPECT_TRUE((false == TRI_IsDirectory(dataPath.c_str())));
    view->open();
    EXPECT_TRUE((false == TRI_IsDirectory(dataPath.c_str())));
  }
}

TEST_F(IResearchViewTest, test_query) {
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
    ASSERT_TRUE((false == !logicalView));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
    ASSERT_TRUE((false == !view));

    arangodb::transaction::Methods trx(
      arangodb::transaction::StandaloneContext::Create(vocbase),
      EMPTY,
      EMPTY,
      EMPTY,
      arangodb::transaction::Options()
    );
    auto* snapshot = view->snapshot(trx, arangodb::iresearch::IResearchView::SnapshotMode::FindOrCreate);
    EXPECT_TRUE(0 == snapshot->docs_count());
  }

  // ordered iterator
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
    auto linkJson = arangodb::velocypack::Parser::fromJson("{ \"view\": \"testView\", \"includeAllFields\": true }");
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    ASSERT_TRUE((false == !logicalCollection));
    auto logicalView = vocbase.createView(createJson->slice());
    EXPECT_TRUE((false == !logicalView));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
    EXPECT_TRUE((false == !view));
    std::shared_ptr<arangodb::Index> index = 
      arangodb::iresearch::IResearchMMFilesLink::factory().instantiate(*logicalCollection, linkJson->slice(), 42, false);
    ASSERT_TRUE((false == !index));
    auto link = std::dynamic_pointer_cast<arangodb::iresearch::IResearchLink>(index);
    ASSERT_TRUE((false == !link));

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
      EXPECT_TRUE((trx.begin().ok()));

      for (size_t i = 0; i < 12; ++i) {
        EXPECT_TRUE((link->insert(trx, arangodb::LocalDocumentId(i), doc->slice(), arangodb::Index::OperationMode::normal).ok()));
      }

      EXPECT_TRUE((trx.commit().ok()));
      EXPECT_TRUE(link->commit().ok());
    }

    arangodb::transaction::Methods trx(
      arangodb::transaction::StandaloneContext::Create(vocbase),
      EMPTY,
      EMPTY,
      EMPTY,
      arangodb::transaction::Options()
    );
    auto* snapshot = view->snapshot(trx, arangodb::iresearch::IResearchView::SnapshotMode::FindOrCreate);
    EXPECT_TRUE(12 == snapshot->docs_count());
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
    EXPECT_TRUE((false == !logicalView));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
    EXPECT_TRUE((false == !view));
    arangodb::Result res = logicalView->properties(links->slice(), true);
    EXPECT_TRUE(true == res.ok());
    EXPECT_TRUE((false == logicalCollection->getIndexes().empty()));
    auto index = logicalCollection->getIndexes()[0];
    ASSERT_TRUE((false == !index));
    auto link = std::dynamic_pointer_cast<arangodb::iresearch::IResearchLink>(index);
    ASSERT_TRUE((false == !link));

    // fill with test data
    {
      arangodb::transaction::Methods trx(
        arangodb::transaction::StandaloneContext::Create(vocbase),
        EMPTY,
        collections,
        EMPTY,
        arangodb::transaction::Options()
      );
      EXPECT_TRUE((trx.begin().ok()));

      arangodb::ManagedDocumentResult inserted;
      arangodb::OperationOptions options;
      for (size_t i = 1; i <= 12; ++i) {
        auto doc = arangodb::velocypack::Parser::fromJson(std::string("{ \"key\": ") + std::to_string(i) + " }");
        logicalCollection->insert(&trx, doc->slice(), inserted, options, false);
      }

      EXPECT_TRUE((trx.commit().ok()));
      EXPECT_TRUE(link->commit().ok());
    }

    arangodb::transaction::Methods trx0(
      arangodb::transaction::StandaloneContext::Create(vocbase),
      EMPTY,
      EMPTY,
      EMPTY,
      arangodb::transaction::Options()
    );
    auto* snapshot0 = view->snapshot(trx0, arangodb::iresearch::IResearchView::SnapshotMode::FindOrCreate);
    EXPECT_TRUE(12 == snapshot0->docs_count());

    // add more data
    {
      arangodb::transaction::Methods trx(
        arangodb::transaction::StandaloneContext::Create(vocbase),
        EMPTY,
        collections,
        EMPTY,
        arangodb::transaction::Options()
      );
      EXPECT_TRUE((trx.begin().ok()));

      arangodb::ManagedDocumentResult inserted;
      arangodb::OperationOptions options;
      for (size_t i = 13; i <= 24; ++i) {
        auto doc = arangodb::velocypack::Parser::fromJson(std::string("{ \"key\": ") + std::to_string(i) + " }");
        logicalCollection->insert(&trx, doc->slice(), inserted, options, false);
      }

      EXPECT_TRUE(trx.commit().ok());
      EXPECT_TRUE(link->commit().ok());
    }

    // old reader sees same data as before
    EXPECT_TRUE(12 == snapshot0->docs_count());
    // new reader sees new data
    arangodb::transaction::Methods trx1(
      arangodb::transaction::StandaloneContext::Create(vocbase),
      EMPTY,
      EMPTY,
      EMPTY,
      arangodb::transaction::Options()
    );
    auto* snapshot1 = view->snapshot(trx1, arangodb::iresearch::IResearchView::SnapshotMode::FindOrCreate);
    EXPECT_TRUE(24 == snapshot1->docs_count());
  }

  // query while running FlushThread
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
    auto viewCreateJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
    auto viewUpdateJson = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection\": { \"includeAllFields\": true } } }");
    auto* feature = arangodb::application_features::ApplicationServer::lookupFeature<
      arangodb::FlushFeature
    >("Flush");
    ASSERT_TRUE(feature);
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    auto logicalView = vocbase.createView(viewCreateJson->slice());
    ASSERT_TRUE((false == !logicalView));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
    ASSERT_TRUE((false == !view));
    arangodb::Result res = logicalView->properties(viewUpdateJson->slice(), true);
    ASSERT_TRUE(true == res.ok());

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

        EXPECT_TRUE((trx.begin().ok()));
        EXPECT_TRUE((trx.insert(logicalCollection->name(), doc->slice(), arangodb::OperationOptions()).ok()));
        EXPECT_TRUE((trx.commit().ok()));
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
        auto* snapshot = view->snapshot(trx, arangodb::iresearch::IResearchView::SnapshotMode::SyncAndReplace);
        EXPECT_TRUE(i == snapshot->docs_count());
      }
    }
  }
}

TEST_F(IResearchViewTest, test_register_link) {
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
    engine.views.clear();
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    ASSERT_TRUE((false == !logicalCollection));
    auto logicalView = vocbase.createView(viewJson0->slice());
    ASSERT_TRUE((false == !logicalView));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
    ASSERT_TRUE((false == !view));

    {
      arangodb::velocypack::Builder builder;
      builder.openObject();
      view->properties(builder, arangodb::LogicalDataSource::makeFlags());
      builder.close();

      auto slice = builder.slice();
      EXPECT_TRUE(slice.isObject());
      EXPECT_TRUE((4U == slice.length()));
      EXPECT_TRUE((slice.hasKey("globallyUniqueId") && slice.get("globallyUniqueId").isString() && false == slice.get("globallyUniqueId").copyString().empty()));
      EXPECT_TRUE(slice.get("id").copyString() == "101");
      EXPECT_TRUE(slice.get("name").copyString() == "testView");
      EXPECT_TRUE(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
      EXPECT_TRUE(slice.get("deleted").isNone()); // no system properties
    }

    {
      std::set<TRI_voc_cid_t> cids;
      view->visitCollections([&cids](TRI_voc_cid_t cid)->bool { cids.emplace(cid); return true; });
      EXPECT_TRUE((0 == cids.size()));
    }

    auto before = StorageEngineMock::inRecoveryResult;
    StorageEngineMock::inRecoveryResult = true;
    auto restore = irs::make_finally([&before]()->void { StorageEngineMock::inRecoveryResult = before; });
    persisted = false;
    std::shared_ptr<arangodb::Index> link =
      arangodb::iresearch::IResearchMMFilesLink::factory().instantiate(*logicalCollection, linkJson->slice(), 1, false);
    EXPECT_TRUE((false == persisted));
    EXPECT_TRUE((false == !link));

    // link addition does modify view meta
    {
      std::unordered_set<TRI_voc_cid_t> expected = { 100 };
      std::set<TRI_voc_cid_t> actual;
      view->visitCollections([&actual](TRI_voc_cid_t cid)->bool { actual.emplace(cid); return true; });

      for (auto& cid: expected) {
        EXPECT_TRUE((1 == actual.erase(cid)));
      }

      EXPECT_TRUE((actual.empty()));
    }
  }

  std::vector<std::string> EMPTY;

  // new link
  {
    engine.views.clear();
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    ASSERT_TRUE((false == !logicalCollection));
    auto logicalView = vocbase.createView(viewJson0->slice());
    ASSERT_TRUE((false == !logicalView));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
    ASSERT_TRUE((false == !view));

    {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      view->properties(builder, arangodb::LogicalDataSource::makeFlags());
      builder.close();

      auto slice = builder.slice();
      EXPECT_TRUE(slice.isObject());
      EXPECT_TRUE((4U == slice.length()));
      EXPECT_TRUE((slice.hasKey("globallyUniqueId") && slice.get("globallyUniqueId").isString() && false == slice.get("globallyUniqueId").copyString().empty()));
      EXPECT_TRUE(slice.get("id").copyString() == "101");
      EXPECT_TRUE(slice.get("name").copyString() == "testView");
      EXPECT_TRUE(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
      EXPECT_TRUE(slice.get("deleted").isNone()); // no system properties
    }

    {
      std::unordered_set<TRI_voc_cid_t> cids;
      arangodb::transaction::Methods trx(
        arangodb::transaction::StandaloneContext::Create(vocbase),
        EMPTY,
        EMPTY,
        EMPTY,
        arangodb::transaction::Options()
      );
      auto* snapshot = view->snapshot(trx, arangodb::iresearch::IResearchView::SnapshotMode::FindOrCreate);
      EXPECT_TRUE((0 == snapshot->docs_count()));
    }

    {
      std::set<TRI_voc_cid_t> actual;
      view->visitCollections([&actual](TRI_voc_cid_t cid)->bool { actual.emplace(cid); return true; });
      EXPECT_TRUE((actual.empty()));
    }

    persisted = false;
    std::shared_ptr<arangodb::Index> link =
      arangodb::iresearch::IResearchMMFilesLink::factory().instantiate(*logicalCollection, linkJson->slice(), 1, false);
    EXPECT_TRUE((true == persisted)); // link instantiation does modify and persist view meta
    EXPECT_TRUE((false == !link));
    std::unordered_set<TRI_voc_cid_t> cids;
    arangodb::transaction::Methods trx(
      arangodb::transaction::StandaloneContext::Create(vocbase),
      EMPTY,
      EMPTY,
      EMPTY,
      arangodb::transaction::Options()
    );
    auto* snapshot = view->snapshot(trx, arangodb::iresearch::IResearchView::SnapshotMode::FindOrCreate);
    EXPECT_TRUE((0 == snapshot->docs_count())); // link addition does trigger collection load

    // link addition does modify view meta
    {
      std::unordered_set<TRI_voc_cid_t> expected = { 100 };
      std::set<TRI_voc_cid_t> actual;
      view->visitCollections([&actual](TRI_voc_cid_t cid)->bool { actual.emplace(cid); return true; });

      for (auto& cid: expected) {
        EXPECT_TRUE((1 == actual.erase(cid)));
      }

      EXPECT_TRUE((actual.empty()));
    }
  }

  // known link
  {
    engine.views.clear();
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    ASSERT_TRUE((false == !logicalCollection));
    auto logicalView = vocbase.createView(viewJson1->slice());
    ASSERT_TRUE((false == !logicalView));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
    ASSERT_TRUE((false == !view));

    {
      std::unordered_set<TRI_voc_cid_t> cids;
      arangodb::transaction::Methods trx(
        arangodb::transaction::StandaloneContext::Create(vocbase),
        EMPTY,
        EMPTY,
        EMPTY,
        arangodb::transaction::Options()
      );
      auto* snapshot = view->snapshot(trx, arangodb::iresearch::IResearchView::SnapshotMode::FindOrCreate);
      EXPECT_TRUE((nullptr == snapshot));
    }

    {
      std::unordered_set<TRI_voc_cid_t> expected = { 100, 123 };
      std::set<TRI_voc_cid_t> actual = { 123 };
      view->visitCollections([&actual](TRI_voc_cid_t cid)->bool { actual.emplace(cid); return true; });

      for (auto& cid: expected) {
        EXPECT_TRUE((1 == actual.erase(cid)));
      }

      EXPECT_TRUE((actual.empty()));
    }

    persisted = false;
    std::shared_ptr<arangodb::Index> link0 =
      arangodb::iresearch::IResearchMMFilesLink::factory().instantiate(*logicalCollection, linkJson->slice(), 1, false);
    EXPECT_TRUE((false == persisted));
    EXPECT_TRUE((false == !link0));

    {
      std::unordered_set<TRI_voc_cid_t> cids;
      arangodb::transaction::Methods trx(
        arangodb::transaction::StandaloneContext::Create(vocbase),
        EMPTY,
        EMPTY,
        EMPTY,
        arangodb::transaction::Options()
      );
      auto* snapshot = view->snapshot(trx, arangodb::iresearch::IResearchView::SnapshotMode::FindOrCreate);
      EXPECT_TRUE((0 == snapshot->docs_count())); // link addition does trigger collection load
    }

    {
      std::unordered_set<TRI_voc_cid_t> expected = { 100, 123 };
      std::set<TRI_voc_cid_t> actual = { 123 };
      view->visitCollections([&actual](TRI_voc_cid_t cid)->bool { actual.emplace(cid); return true; });

      for (auto& cid: expected) {
        EXPECT_TRUE((1 == actual.erase(cid)));
      }

      EXPECT_TRUE((actual.empty()));
    }

    persisted = false;
    std::shared_ptr<arangodb::Index> link1;
    try {
      link1 = arangodb::iresearch::IResearchMMFilesLink::factory().instantiate(*logicalCollection, linkJson->slice(), 1, false);
      EXPECT_EQ(nullptr, link1);
    } catch (std::exception const&) {
      // ignore any errors here
    }
    link0.reset(); // unload link before creating a new link instance
    link1 = arangodb::iresearch::IResearchMMFilesLink::factory().instantiate(*logicalCollection, linkJson->slice(), 1, false);
    EXPECT_TRUE((false == persisted));
    EXPECT_TRUE((false == !link1)); // duplicate link creation is allowed
    std::unordered_set<TRI_voc_cid_t> cids;
    arangodb::transaction::Methods trx(
      arangodb::transaction::StandaloneContext::Create(vocbase),
      EMPTY,
      EMPTY,
      EMPTY,
      arangodb::transaction::Options()
    );
    auto* snapshot = view->snapshot(trx, arangodb::iresearch::IResearchView::SnapshotMode::FindOrCreate);
    EXPECT_TRUE((0 == snapshot->docs_count())); // link addition does trigger collection load

    {
      std::unordered_set<TRI_voc_cid_t> expected = { 100, 123 };
      std::set<TRI_voc_cid_t> actual = { 123 };
      view->visitCollections([&actual](TRI_voc_cid_t cid)->bool { actual.emplace(cid); return true; });

      for (auto& cid: expected) {
        EXPECT_TRUE((1 == actual.erase(cid)));
      }

      EXPECT_TRUE((actual.empty()));
    }
  }
}

TEST_F(IResearchViewTest, test_unregister_link) {
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
    engine.views.clear();
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    ASSERT_TRUE((false == !logicalCollection));
    auto logicalView = vocbase.createView(viewJson->slice());
    ASSERT_TRUE((false == !logicalView));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
    ASSERT_TRUE((false == !view));
    std::shared_ptr<arangodb::Index> index = 
      arangodb::iresearch::IResearchMMFilesLink::factory().instantiate(*logicalCollection, linkJson->slice(), __LINE__, false);
    ASSERT_TRUE((false == !index));
    auto link = std::dynamic_pointer_cast<arangodb::iresearch::IResearchLink>(index);
    ASSERT_TRUE((false == !link));

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
      EXPECT_TRUE((trx.begin().ok()));
      EXPECT_TRUE((link->insert(trx, arangodb::LocalDocumentId(0), doc->slice(), arangodb::Index::OperationMode::normal).ok()));
      EXPECT_TRUE((trx.commit().ok()));
      EXPECT_TRUE((link->commit().ok()));
    }

    auto links = arangodb::velocypack::Parser::fromJson(
      "{ \"links\": { \"testCollection\": { \"id\": " + std::to_string(link->id()) + " } } }" // same link ID
    );

    link->unload(); // unload link before creating a new link instance
    arangodb::Result res = logicalView->properties(links->slice(), true);
    EXPECT_TRUE(true == res.ok());
    EXPECT_TRUE((false == logicalCollection->getIndexes().empty()));

    {
      std::unordered_set<TRI_voc_cid_t> cids;
      arangodb::transaction::Methods trx(
        arangodb::transaction::StandaloneContext::Create(vocbase),
        EMPTY,
        EMPTY,
        EMPTY,
        arangodb::transaction::Options()
      );
      auto* snapshot = view->snapshot(trx, arangodb::iresearch::IResearchView::SnapshotMode::FindOrCreate);
      EXPECT_TRUE((1 == snapshot->docs_count()));
    }

    {
      std::unordered_set<TRI_voc_cid_t> expected = { 100 };
      std::set<TRI_voc_cid_t> actual = { };
      view->visitCollections([&actual](TRI_voc_cid_t cid)->bool { actual.emplace(cid); return true; });

      for (auto& cid: expected) {
        EXPECT_TRUE((1 == actual.erase(cid)));
      }

      EXPECT_TRUE((actual.empty()));
    }

    EXPECT_TRUE((nullptr != vocbase.lookupCollection("testCollection")));

    auto before = StorageEngineMock::inRecoveryResult;
    StorageEngineMock::inRecoveryResult = true;
    auto restore = irs::make_finally([&before]()->void { StorageEngineMock::inRecoveryResult = before; });
    persisted = false;
    EXPECT_TRUE((true == vocbase.dropCollection(logicalCollection->id(), true, -1).ok()));
    EXPECT_TRUE((false == persisted)); // link removal does not persist view meta
    EXPECT_TRUE((nullptr == vocbase.lookupCollection("testCollection")));

    {
      std::unordered_set<TRI_voc_cid_t> cids;
      arangodb::transaction::Methods trx(
        arangodb::transaction::StandaloneContext::Create(vocbase),
        EMPTY,
        EMPTY,
        EMPTY,
        arangodb::transaction::Options()
      );
      auto* snapshot = view->snapshot(trx, arangodb::iresearch::IResearchView::SnapshotMode::FindOrCreate);
      EXPECT_TRUE((0 == snapshot->docs_count()));
    }

    {
      std::set<TRI_voc_cid_t> actual;
      view->visitCollections([&actual](TRI_voc_cid_t cid)->bool { actual.emplace(cid); return true; });
      EXPECT_TRUE((actual.empty())); // collection removal does modify view meta
    }

    EXPECT_TRUE((false == !vocbase.lookupView("testView")));
    EXPECT_TRUE((true == vocbase.dropView(view->id(), false).ok()));
    EXPECT_TRUE((true == !vocbase.lookupView("testView")));
  }

  // link removed before view
  {
    engine.views.clear();
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    ASSERT_TRUE((false == !logicalCollection));
    auto logicalView = vocbase.createView(viewJson->slice());
    ASSERT_TRUE((false == !logicalView));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
    ASSERT_TRUE((false == !view));
    std::shared_ptr<arangodb::Index> index = 
      arangodb::iresearch::IResearchMMFilesLink::factory().instantiate(*logicalCollection, linkJson->slice(), __LINE__, false);
    ASSERT_TRUE((false == !index));
    auto link = std::dynamic_pointer_cast<arangodb::iresearch::IResearchLink>(index);
    ASSERT_TRUE((false == !link));

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
      EXPECT_TRUE((trx.begin().ok()));
      EXPECT_TRUE((link->insert(trx, arangodb::LocalDocumentId(0), doc->slice(), arangodb::Index::OperationMode::normal).ok()));
      EXPECT_TRUE((trx.commit().ok()));
      EXPECT_TRUE((link->commit().ok()));
    }

    auto links = arangodb::velocypack::Parser::fromJson(
      "{ \"links\": { \"testCollection\": {\"id\": " + std::to_string(link->id()) + " } } }" // same link ID
    );

    link->unload(); // unload link before creating a new link instance
    arangodb::Result res = logicalView->properties(links->slice(), true);
    EXPECT_TRUE(true == res.ok());
    EXPECT_TRUE((false == logicalCollection->getIndexes().empty()));

    {
      std::unordered_set<TRI_voc_cid_t> cids;
      arangodb::transaction::Methods trx(
        arangodb::transaction::StandaloneContext::Create(vocbase),
        EMPTY,
        EMPTY,
        EMPTY,
        arangodb::transaction::Options()
      );
      auto* snapshot = view->snapshot(trx, arangodb::iresearch::IResearchView::SnapshotMode::FindOrCreate);
      EXPECT_TRUE((1 == snapshot->docs_count()));
    }

    {
      std::unordered_set<TRI_voc_cid_t> expected = { 100 };
      std::set<TRI_voc_cid_t> actual;
      view->visitCollections([&actual](TRI_voc_cid_t cid)->bool { actual.emplace(cid); return true; });

      for (auto& cid: expected) {
        EXPECT_TRUE((1 == actual.erase(cid)));
      }

      EXPECT_TRUE((actual.empty()));
    }

    EXPECT_TRUE((nullptr != vocbase.lookupCollection("testCollection")));
    persisted = false;
    EXPECT_TRUE((true == vocbase.dropCollection(logicalCollection->id(), true, -1).ok()));
    EXPECT_TRUE((true == persisted)); // collection removal persists view meta
    EXPECT_TRUE((nullptr == vocbase.lookupCollection("testCollection")));

    {
      std::unordered_set<TRI_voc_cid_t> cids;
      arangodb::transaction::Methods trx(
        arangodb::transaction::StandaloneContext::Create(vocbase),
        EMPTY,
        EMPTY,
        EMPTY,
        arangodb::transaction::Options()
      );
      auto* snapshot = view->snapshot(trx, arangodb::iresearch::IResearchView::SnapshotMode::FindOrCreate);
      EXPECT_TRUE((0 == snapshot->docs_count()));
    }

    {
      std::set<TRI_voc_cid_t> actual;
      view->visitCollections([&actual](TRI_voc_cid_t cid)->bool { actual.emplace(cid); return true; });
      EXPECT_TRUE((actual.empty())); // collection removal does modify view meta
    }

    EXPECT_TRUE((false == !vocbase.lookupView("testView")));
    EXPECT_TRUE((true == vocbase.dropView(view->id(), false).ok()));
    EXPECT_TRUE((true == !vocbase.lookupView("testView")));
  }

  // view removed before link
  {
    engine.views.clear();
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    auto logicalView = vocbase.createView(viewJson->slice());
    ASSERT_TRUE((false == !logicalView));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
    ASSERT_TRUE((false == !view));

    auto links = arangodb::velocypack::Parser::fromJson("{ \
      \"links\": { \"testCollection\": {} } \
    }");

    arangodb::Result res = logicalView->properties(links->slice(), true);
    EXPECT_TRUE(true == res.ok());
    EXPECT_TRUE((false == logicalCollection->getIndexes().empty()));

    std::set<TRI_voc_cid_t> cids;
    view->visitCollections([&cids](TRI_voc_cid_t cid)->bool { cids.emplace(cid); return true; });
    EXPECT_TRUE((1 == cids.size()));
    EXPECT_TRUE((false == !vocbase.lookupView("testView")));
    EXPECT_TRUE((true == view->drop().ok()));
    EXPECT_TRUE((true == !vocbase.lookupView("testView")));
    EXPECT_TRUE((nullptr != vocbase.lookupCollection("testCollection")));
    EXPECT_TRUE((true == vocbase.dropCollection(logicalCollection->id(), true, -1).ok()));
    EXPECT_TRUE((nullptr == vocbase.lookupCollection("testCollection")));
  }

  // view deallocated before link removed
  {
    engine.views.clear();
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());

    {
      auto createJson = arangodb::velocypack::Parser::fromJson("{}");
      auto updateJson = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection\": {} } }");
      auto logicalView = vocbase.createView(viewJson->slice());
      ASSERT_TRUE((false == !logicalView));
      auto* viewImpl = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
      ASSERT_TRUE((nullptr != viewImpl));
      EXPECT_TRUE((viewImpl->properties(updateJson->slice(), true).ok()));
      EXPECT_TRUE((false == logicalCollection->getIndexes().empty()));
      std::set<TRI_voc_cid_t> cids;
      viewImpl->visitCollections([&cids](TRI_voc_cid_t cid)->bool { cids.emplace(cid); return true; });
      EXPECT_TRUE((1 == cids.size()));
      logicalCollection->getIndexes()[0]->unload(); // release view reference to prevent deadlock due to ~IResearchView() waiting for IResearchLink::unload()
      EXPECT_TRUE((true == vocbase.dropView(logicalView->id(), false).ok()));
      EXPECT_TRUE((1 == logicalView.use_count())); // ensure destructor for ViewImplementation is called
      EXPECT_TRUE((false == logicalCollection->getIndexes().empty()));
    }

    // create a new view with same ID to validate links
    {
      auto json = arangodb::velocypack::Parser::fromJson("{}");
      arangodb::LogicalView::ptr view;
      ASSERT_TRUE((arangodb::iresearch::IResearchView::factory().instantiate(view, vocbase, viewJson->slice(), 0).ok()));
      ASSERT_TRUE((false == !view));
      auto* viewImpl = dynamic_cast<arangodb::iresearch::IResearchView*>(view.get());
      ASSERT_TRUE((nullptr != viewImpl));
      std::set<TRI_voc_cid_t> cids;
      viewImpl->visitCollections([&cids](TRI_voc_cid_t cid)->bool { cids.emplace(cid); return true; });
      EXPECT_TRUE((0 == cids.size()));

      for (auto& index: logicalCollection->getIndexes()) {
        auto* link = dynamic_cast<arangodb::iresearch::IResearchLink*>(index.get());
        ASSERT_TRUE((!link->self()->get())); // check that link is unregistred from view
      }
    }
  }
}

TEST_F(IResearchViewTest, test_tracked_cids) {
  auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\", \"id\": 100 }");
  auto viewJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\", \"id\": 101 }");

  // test empty before open (TRI_vocbase_t::createView(...) will call open())
  {
    engine.views.clear();
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    arangodb::LogicalView::ptr view;
    EXPECT_TRUE((arangodb::iresearch::IResearchView::factory().create(view, vocbase, viewJson->slice()).ok()));
    EXPECT_TRUE((nullptr != view));
    auto* viewImpl = dynamic_cast<arangodb::iresearch::IResearchView*>(view.get());
    ASSERT_TRUE((nullptr != viewImpl));

    std::set<TRI_voc_cid_t> actual;
    viewImpl->visitCollections([&actual](TRI_voc_cid_t cid)->bool { actual.emplace(cid); return true; });
    EXPECT_TRUE((actual.empty()));
  }

  // test add via link before open (TRI_vocbase_t::createView(...) will call open())
  {
    engine.views.clear();
    auto updateJson = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection\": { } } }");
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    ASSERT_TRUE((nullptr != logicalCollection));
    arangodb::LogicalView::ptr logicalView;
    ASSERT_TRUE((arangodb::iresearch::IResearchView::factory().instantiate(logicalView, vocbase, viewJson->slice(), 0).ok()));
    ASSERT_TRUE((false == !logicalView));
    engine.createView(vocbase, logicalView->id(), *logicalView); // ensure link can find view
    StorageEngineMock(server).registerView(vocbase, logicalView); // ensure link can find view
    auto* viewImpl = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
    ASSERT_TRUE((nullptr != viewImpl));

    EXPECT_TRUE((viewImpl->properties(updateJson->slice(), false).ok()));

    std::set<TRI_voc_cid_t> actual;
    std::set<TRI_voc_cid_t> expected = { 100 };
    viewImpl->visitCollections([&actual](TRI_voc_cid_t cid)->bool { actual.emplace(cid); return true; });

    for (auto& cid: actual) {
      EXPECT_TRUE((1 == expected.erase(cid)));
    }

    EXPECT_TRUE((expected.empty()));
    logicalCollection->getIndexes()[0]->unload(); // release view reference to prevent deadlock due to ~IResearchView() waiting for IResearchLink::unload()
  }

  // test drop via link before open (TRI_vocbase_t::createView(...) will call open())
  {
    engine.views.clear();
    auto updateJson0 = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection\": { } } }");
    auto updateJson1 = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection\": null } }");
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    ASSERT_TRUE((nullptr != logicalCollection));
    arangodb::LogicalView::ptr logicalView;
    ASSERT_TRUE((arangodb::iresearch::IResearchView::factory().instantiate(logicalView, vocbase, viewJson->slice(), 0).ok()));
    ASSERT_TRUE((false == !logicalView));
    engine.createView(vocbase, logicalView->id(), *logicalView); // ensure link can find view
    StorageEngineMock(server).registerView(vocbase, logicalView); // ensure link can find view
    auto* viewImpl = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
    ASSERT_TRUE((nullptr != viewImpl));

    // create link
    {
      EXPECT_TRUE((viewImpl->properties(updateJson0->slice(), false).ok()));

      std::set<TRI_voc_cid_t> actual;
      std::set<TRI_voc_cid_t> expected = { 100 };
      viewImpl->visitCollections([&actual](TRI_voc_cid_t cid)->bool { actual.emplace(cid); return true; });

      for (auto& cid: actual) {
        EXPECT_TRUE((1 == expected.erase(cid)));
      }

      EXPECT_TRUE((expected.empty()));
    }

    // drop link
    {
      EXPECT_TRUE((viewImpl->properties(updateJson1->slice(), false).ok()));

      std::set<TRI_voc_cid_t> actual;
      viewImpl->visitCollections([&actual](TRI_voc_cid_t cid)->bool { actual.emplace(cid); return true; });
      EXPECT_TRUE((actual.empty()));
    }
  }

  // test load persisted CIDs on open (TRI_vocbase_t::createView(...) will call open())
  // use separate view ID for this test since doing open from persisted store
  {
    // initial populate persisted view
    {
      engine.views.clear();
      auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
      auto linkJson = arangodb::velocypack::Parser::fromJson("{ \"view\": \"testView\" }");
      auto createJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\", \"id\": 102 }");
      auto* feature = arangodb::application_features::ApplicationServer::lookupFeature<
        arangodb::FlushFeature
      >("Flush");
      ASSERT_TRUE(feature);
      Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
      auto logicalCollection = vocbase.createCollection(collectionJson->slice());
      ASSERT_TRUE((false == !logicalCollection));
      auto logicalView = vocbase.createView(createJson->slice());
      ASSERT_TRUE((false == !logicalView));
      auto* viewImpl = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
      ASSERT_TRUE((nullptr != viewImpl));
      std::shared_ptr<arangodb::Index> index = 
        arangodb::iresearch::IResearchMMFilesLink::factory().instantiate(*logicalCollection, linkJson->slice(), 42, false);
      ASSERT_TRUE((false == !index));
      auto link = std::dynamic_pointer_cast<arangodb::iresearch::IResearchLink>(index);
      ASSERT_TRUE((false == !link));

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
      EXPECT_TRUE((trx.begin().ok()));
      EXPECT_TRUE((link->insert(trx, arangodb::LocalDocumentId(0), doc->slice(), arangodb::Index::OperationMode::normal).ok()));
      EXPECT_TRUE((trx.commit().ok()));
      EXPECT_TRUE((link->commit().ok())); // commit to persisted store
    }

    // test persisted CIDs on open
    {
      engine.views.clear();
      auto createJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\", \"id\": 102 }");
      Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
      auto logicalView = vocbase.createView(createJson->slice());
      ASSERT_TRUE((false == !logicalView));
      auto* viewImpl = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
      ASSERT_TRUE((nullptr != viewImpl));

      std::set<TRI_voc_cid_t> actual;
      viewImpl->visitCollections([&actual](TRI_voc_cid_t cid)->bool { actual.emplace(cid); return true; });
      EXPECT_TRUE((actual.empty())); // persisted cids do not modify view meta
    }
  }

  // test add via link after open (TRI_vocbase_t::createView(...) will call open())
  {
    engine.views.clear();
    auto updateJson = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection\": { } } }");
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    ASSERT_TRUE((nullptr != logicalCollection));
    auto logicalView = vocbase.createView(viewJson->slice());
    ASSERT_TRUE((false == !logicalView));
    auto* viewImpl = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
    ASSERT_TRUE((nullptr != viewImpl));

    EXPECT_TRUE((viewImpl->properties(updateJson->slice(), false).ok()));

    std::set<TRI_voc_cid_t> actual;
    std::set<TRI_voc_cid_t> expected = { 100 };
    viewImpl->visitCollections([&actual](TRI_voc_cid_t cid)->bool { actual.emplace(cid); return true; });

    for (auto& cid: actual) {
      EXPECT_TRUE((1 == expected.erase(cid)));
    }

    EXPECT_TRUE((expected.empty()));
  }

  // test drop via link after open (TRI_vocbase_t::createView(...) will call open())
  {
    engine.views.clear();
    auto updateJson0 = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection\": { } } }");
    auto updateJson1 = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection\": null } }");
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    ASSERT_TRUE((nullptr != logicalCollection));
    auto logicalView = vocbase.createView(viewJson->slice());
    ASSERT_TRUE((false == !logicalView));
    auto* viewImpl = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
    ASSERT_TRUE((nullptr != viewImpl));

    // create link
    {
      EXPECT_TRUE((viewImpl->properties(updateJson0->slice(), false).ok()));

      std::set<TRI_voc_cid_t> actual;
      std::set<TRI_voc_cid_t> expected = { 100 };
      viewImpl->visitCollections([&actual](TRI_voc_cid_t cid)->bool { actual.emplace(cid); return true; });

      for (auto& cid: actual) {
        EXPECT_TRUE((1 == expected.erase(cid)));
      }

      EXPECT_TRUE((expected.empty()));
    }

    // drop link
    {
      EXPECT_TRUE((viewImpl->properties(updateJson1->slice(), false).ok()));

      std::set<TRI_voc_cid_t> actual;
      viewImpl->visitCollections([&actual](TRI_voc_cid_t cid)->bool { actual.emplace(cid); return true; });
      EXPECT_TRUE((actual.empty()));
    }
  }
}

TEST_F(IResearchViewTest, test_overwrite_immutable_properties) {
  arangodb::iresearch::IResearchViewMeta meta;
  arangodb::iresearch::IResearchViewMetaState metaState;
  std::string tmpString;

  auto viewJson = arangodb::velocypack::Parser::fromJson(
    "{ \"id\": 123, "
      "\"name\": \"testView\", "
      "\"type\": \"arangosearch\", "
      "\"writebufferActive\": 25, "
      "\"writebufferIdle\": 12, "
      "\"writebufferSizeMax\": 44040192, "
      "\"locale\": \"C\", "
      "\"version\": 1, "
      "\"primarySort\": [ "
        "{ \"field\": \"my.Nested.field\", \"direction\": \"asc\" }, "
        "{ \"field\": \"another.field\", \"asc\": false } "
      "]"
  "}");

  Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
  auto logicalView = vocbase.createView(viewJson->slice()); // create view
  ASSERT_TRUE(nullptr != logicalView);

  VPackBuilder builder;

  // check immutable properties after creation
  {
    builder.openObject();
    EXPECT_TRUE(logicalView
                    ->properties(builder, arangodb::LogicalDataSource::makeFlags(
                                              arangodb::LogicalDataSource::Serialize::Detailed))
                    .ok());
    builder.close();
    EXPECT_TRUE(true == meta.init(builder.slice(), tmpString));
    EXPECT_TRUE(std::string("C") == irs::locale_utils::name(meta._locale));
    EXPECT_TRUE(1 == meta._version);
    EXPECT_TRUE(25 == meta._writebufferActive);
    EXPECT_TRUE(12 == meta._writebufferIdle);
    EXPECT_TRUE(42*(size_t(1)<<20) == meta._writebufferSizeMax);
    EXPECT_TRUE(2 == meta._primarySort.size());
    {
      auto& field = meta._primarySort.field(0);
      EXPECT_TRUE(3 == field.size());
      EXPECT_TRUE("my" == field[0].name);
      EXPECT_TRUE(false == field[0].shouldExpand);
      EXPECT_TRUE("Nested" == field[1].name);
      EXPECT_TRUE(false == field[1].shouldExpand);
      EXPECT_TRUE("field" == field[2].name);
      EXPECT_TRUE(false == field[2].shouldExpand);
      EXPECT_TRUE(true == meta._primarySort.direction(0));
    }
    {
      auto& field = meta._primarySort.field(1);
      EXPECT_TRUE(2 == field.size());
      EXPECT_TRUE("another" == field[0].name);
      EXPECT_TRUE(false == field[0].shouldExpand);
      EXPECT_TRUE("field" == field[1].name);
      EXPECT_TRUE(false == field[1].shouldExpand);
      EXPECT_TRUE(false == meta._primarySort.direction(1));
    }
  }

  auto newProperties = arangodb::velocypack::Parser::fromJson(
    "{"
      "\"writeBufferActive\": 125, "
      "\"writeBufferIdle\": 112, "
      "\"writeBufferSizeMax\": 142, "
      "\"locale\": \"en\", "
      "\"version\": 1, "
      "\"primarySort\": [ "
        "{ \"field\": \"field\", \"asc\": true } "
      "]"
  "}");

  EXPECT_TRUE(logicalView->properties(newProperties->slice(), false).ok()); // update immutable properties

  // check immutable properties after update
  {
    builder.clear();
    builder.openObject();
    EXPECT_TRUE(logicalView
                    ->properties(builder, arangodb::LogicalDataSource::makeFlags(
                                              arangodb::LogicalDataSource::Serialize::Detailed))
                    .ok());
    builder.close();
    EXPECT_TRUE(true == meta.init(builder.slice(), tmpString));
    EXPECT_TRUE(std::string("C") == irs::locale_utils::name(meta._locale));
    EXPECT_TRUE(1 == meta._version);
    EXPECT_TRUE(25 == meta._writebufferActive);
    EXPECT_TRUE(12 == meta._writebufferIdle);
    EXPECT_TRUE(42*(size_t(1)<<20) == meta._writebufferSizeMax);
    EXPECT_TRUE(2 == meta._primarySort.size());
    {
      auto& field = meta._primarySort.field(0);
      EXPECT_TRUE(3 == field.size());
      EXPECT_TRUE("my" == field[0].name);
      EXPECT_TRUE(false == field[0].shouldExpand);
      EXPECT_TRUE("Nested" == field[1].name);
      EXPECT_TRUE(false == field[1].shouldExpand);
      EXPECT_TRUE("field" == field[2].name);
      EXPECT_TRUE(false == field[2].shouldExpand);
      EXPECT_TRUE(true == meta._primarySort.direction(0));
    }
    {
      auto& field = meta._primarySort.field(1);
      EXPECT_TRUE(2 == field.size());
      EXPECT_TRUE("another" == field[0].name);
      EXPECT_TRUE(false == field[0].shouldExpand);
      EXPECT_TRUE("field" == field[1].name);
      EXPECT_TRUE(false == field[1].shouldExpand);
      EXPECT_TRUE(false == meta._primarySort.direction(1));
    }
  }
}

TEST_F(IResearchViewTest, test_transaction_registration) {
  auto collectionJson0 = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection0\" }");
  auto collectionJson1 = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection1\" }");
  auto viewJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
  Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
  auto logicalCollection0 = vocbase.createCollection(collectionJson0->slice());
  ASSERT_TRUE((nullptr != logicalCollection0));
  auto logicalCollection1 = vocbase.createCollection(collectionJson1->slice());
  ASSERT_TRUE((nullptr != logicalCollection1));
  auto logicalView = vocbase.createView(viewJson->slice());
  ASSERT_TRUE((false == !logicalView));
  auto* viewImpl = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
  ASSERT_TRUE((nullptr != viewImpl));

  // link collection to view
  {
    auto updateJson = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection0\": {}, \"testCollection1\": {} } }");
    EXPECT_TRUE((viewImpl->properties(updateJson->slice(), false).ok()));
  }

  // read transaction (by id)
  {
    arangodb::SingleCollectionTransaction trx(
      arangodb::transaction::StandaloneContext::Create(vocbase),
      *logicalView,
      arangodb::AccessMode::Type::READ
    );
    EXPECT_TRUE((trx.begin().ok()));
    EXPECT_TRUE((2 == trx.state()->numCollections()));
    EXPECT_TRUE((nullptr != trx.state()->findCollection(logicalCollection0->id())));
    EXPECT_TRUE((nullptr != trx.state()->findCollection(logicalCollection1->id())));
    std::unordered_set<std::string> expectedNames = { "testCollection0", "testCollection1" };
    std::unordered_set<std::string> actualNames;
    trx.state()->allCollections([&actualNames](arangodb::TransactionCollection& col)->bool {
      actualNames.emplace(col.collection()->name());
      return true;
    });

    for(auto& entry: actualNames) {
      EXPECT_TRUE((1 == expectedNames.erase(entry)));
    }

    EXPECT_TRUE((expectedNames.empty()));
    EXPECT_TRUE((trx.commit().ok()));
  }

  // read transaction (by name)
  {
    arangodb::SingleCollectionTransaction trx(
      arangodb::transaction::StandaloneContext::Create(vocbase),
      logicalView->name(),
      arangodb::AccessMode::Type::READ
    );
    EXPECT_TRUE((trx.begin().ok()));
    EXPECT_TRUE((2 == trx.state()->numCollections()));
    EXPECT_TRUE((nullptr != trx.state()->findCollection(logicalCollection0->id())));
    EXPECT_TRUE((nullptr != trx.state()->findCollection(logicalCollection1->id())));
    std::unordered_set<std::string> expectedNames = { "testCollection0", "testCollection1" };
    std::unordered_set<std::string> actualNames;
    trx.state()->allCollections([&actualNames](arangodb::TransactionCollection& col)->bool {
      actualNames.emplace(col.collection()->name());
      return true;
    });

    for(auto& entry: actualNames) {
      EXPECT_TRUE((1 == expectedNames.erase(entry)));
    }

    EXPECT_TRUE((expectedNames.empty()));
    EXPECT_TRUE((trx.commit().ok()));
  }

  // write transaction (by id)
  {
    arangodb::SingleCollectionTransaction trx(
      arangodb::transaction::StandaloneContext::Create(vocbase),
      *logicalView,
      arangodb::AccessMode::Type::WRITE
    );
    EXPECT_TRUE((trx.begin().ok()));
    EXPECT_TRUE((2 == trx.state()->numCollections()));
    EXPECT_TRUE((nullptr != trx.state()->findCollection(logicalCollection0->id())));
    EXPECT_TRUE((nullptr != trx.state()->findCollection(logicalCollection1->id())));
    std::unordered_set<std::string> expectedNames = { "testCollection0", "testCollection1" };
    std::unordered_set<std::string> actualNames;
    trx.state()->allCollections([&actualNames](arangodb::TransactionCollection& col)->bool {
      actualNames.emplace(col.collection()->name());
      return true;
    });

    for(auto& entry: actualNames) {
      EXPECT_TRUE((1 == expectedNames.erase(entry)));
    }

    EXPECT_TRUE((expectedNames.empty()));
    EXPECT_TRUE((trx.commit().ok()));
  }

  // write transaction (by name)
  {
    arangodb::SingleCollectionTransaction trx(
      arangodb::transaction::StandaloneContext::Create(vocbase),
      logicalView->name(),
      arangodb::AccessMode::Type::WRITE
    );
    EXPECT_TRUE((trx.begin().ok()));
    EXPECT_TRUE((2 == trx.state()->numCollections()));
    EXPECT_TRUE((nullptr != trx.state()->findCollection(logicalCollection0->id())));
    EXPECT_TRUE((nullptr != trx.state()->findCollection(logicalCollection1->id())));
    std::unordered_set<std::string> expectedNames = { "testCollection0", "testCollection1" };
    std::unordered_set<std::string> actualNames;
    trx.state()->allCollections([&actualNames](arangodb::TransactionCollection& col)->bool {
      actualNames.emplace(col.collection()->name());
      return true;
    });

    for(auto& entry: actualNames) {
      EXPECT_TRUE((1 == expectedNames.erase(entry)));
    }

    EXPECT_TRUE((expectedNames.empty()));
    EXPECT_TRUE((trx.commit().ok()));
  }

  // exclusive transaction (by id)
  {
    arangodb::SingleCollectionTransaction trx(
      arangodb::transaction::StandaloneContext::Create(vocbase),
      *logicalView,
      arangodb::AccessMode::Type::READ
    );
    EXPECT_TRUE((trx.begin().ok()));
    EXPECT_TRUE((2 == trx.state()->numCollections()));
    EXPECT_TRUE((nullptr != trx.state()->findCollection(logicalCollection0->id())));
    EXPECT_TRUE((nullptr != trx.state()->findCollection(logicalCollection1->id())));
    std::unordered_set<std::string> expectedNames = { "testCollection0", "testCollection1" };
    std::unordered_set<std::string> actualNames;
    trx.state()->allCollections([&actualNames](arangodb::TransactionCollection& col)->bool {
      actualNames.emplace(col.collection()->name());
      return true;
    });

    for(auto& entry: actualNames) {
      EXPECT_TRUE((1 == expectedNames.erase(entry)));
    }

    EXPECT_TRUE((expectedNames.empty()));
    EXPECT_TRUE((trx.commit().ok()));
  }

  // exclusive transaction (by name)
  {
    arangodb::SingleCollectionTransaction trx(
      arangodb::transaction::StandaloneContext::Create(vocbase),
      logicalView->name(),
      arangodb::AccessMode::Type::READ
    );
    EXPECT_TRUE((trx.begin().ok()));
    EXPECT_TRUE((2 == trx.state()->numCollections()));
    EXPECT_TRUE((nullptr != trx.state()->findCollection(logicalCollection0->id())));
    EXPECT_TRUE((nullptr != trx.state()->findCollection(logicalCollection1->id())));
    std::unordered_set<std::string> expectedNames = { "testCollection0", "testCollection1" };
    std::unordered_set<std::string> actualNames;
    trx.state()->allCollections([&actualNames](arangodb::TransactionCollection& col)->bool {
      actualNames.emplace(col.collection()->name());
      return true;
    });

    for(auto& entry: actualNames) {
      EXPECT_TRUE((1 == expectedNames.erase(entry)));
    }

    EXPECT_TRUE((expectedNames.empty()));
    EXPECT_TRUE((trx.commit().ok()));
  }

  // drop collection from vocbase
  EXPECT_TRUE((true == vocbase.dropCollection(logicalCollection1->id(), true, 0).ok()));

  // read transaction (by id) (one collection dropped)
  {
    arangodb::SingleCollectionTransaction trx(
      arangodb::transaction::StandaloneContext::Create(vocbase),
      *logicalView,
      arangodb::AccessMode::Type::READ
    );
    EXPECT_TRUE((trx.begin().ok()));
    EXPECT_TRUE((1 == trx.state()->numCollections()));
    EXPECT_TRUE((nullptr != trx.state()->findCollection(logicalCollection0->id())));
    std::unordered_set<std::string> expectedNames = { "testCollection0" };
    std::unordered_set<std::string> actualNames;
    trx.state()->allCollections([&actualNames](arangodb::TransactionCollection& col)->bool {
      actualNames.emplace(col.collection()->name());
      return true;
    });

    for(auto& entry: actualNames) {
      EXPECT_TRUE((1 == expectedNames.erase(entry)));
    }

    EXPECT_TRUE((expectedNames.empty()));
    EXPECT_TRUE((trx.commit().ok()));
  }

  // read transaction (by name) (one collection dropped)
  {
    arangodb::SingleCollectionTransaction trx(
      arangodb::transaction::StandaloneContext::Create(vocbase),
      logicalView->name(),
      arangodb::AccessMode::Type::READ
    );
    EXPECT_TRUE((trx.begin().ok()));
    EXPECT_TRUE((1 == trx.state()->numCollections()));
    EXPECT_TRUE((nullptr != trx.state()->findCollection(logicalCollection0->id())));
    std::unordered_set<std::string> expectedNames = { "testCollection0" };
    std::unordered_set<std::string> actualNames;
    trx.state()->allCollections([&actualNames](arangodb::TransactionCollection& col)->bool {
      actualNames.emplace(col.collection()->name());
      return true;
    });

    for(auto& entry: actualNames) {
      EXPECT_TRUE((1 == expectedNames.erase(entry)));
    }

    EXPECT_TRUE((expectedNames.empty()));
    EXPECT_TRUE((trx.commit().ok()));
  }

  // write transaction (by id) (one collection dropped)
  {
    arangodb::SingleCollectionTransaction trx(
      arangodb::transaction::StandaloneContext::Create(vocbase),
      *logicalView,
      arangodb::AccessMode::Type::WRITE
    );
    EXPECT_TRUE((trx.begin().ok()));
    EXPECT_TRUE((1 == trx.state()->numCollections()));
    EXPECT_TRUE((nullptr != trx.state()->findCollection(logicalCollection0->id())));
    std::unordered_set<std::string> expectedNames = { "testCollection0" };
    std::unordered_set<std::string> actualNames;
    trx.state()->allCollections([&actualNames](arangodb::TransactionCollection& col)->bool {
      actualNames.emplace(col.collection()->name());
      return true;
    });

    for(auto& entry: actualNames) {
      EXPECT_TRUE((1 == expectedNames.erase(entry)));
    }

    EXPECT_TRUE((expectedNames.empty()));
    EXPECT_TRUE((trx.commit().ok()));
  }

  // write transaction (by name) (one collection dropped)
  {
    arangodb::SingleCollectionTransaction trx(
      arangodb::transaction::StandaloneContext::Create(vocbase),
      logicalView->name(),
      arangodb::AccessMode::Type::WRITE
    );
    EXPECT_TRUE((trx.begin().ok()));
    EXPECT_TRUE((1 == trx.state()->numCollections()));
    EXPECT_TRUE((nullptr != trx.state()->findCollection(logicalCollection0->id())));
    std::unordered_set<std::string> expectedNames = { "testCollection0" };
    std::unordered_set<std::string> actualNames;
    trx.state()->allCollections([&actualNames](arangodb::TransactionCollection& col)->bool {
      actualNames.emplace(col.collection()->name());
      return true;
    });

    for(auto& entry: actualNames) {
      EXPECT_TRUE((1 == expectedNames.erase(entry)));
    }

    EXPECT_TRUE((expectedNames.empty()));
    EXPECT_TRUE((trx.commit().ok()));
  }

  // exclusive transaction (by id) (one collection dropped)
  {
    arangodb::SingleCollectionTransaction trx(
      arangodb::transaction::StandaloneContext::Create(vocbase),
      *logicalView,
      arangodb::AccessMode::Type::READ
    );
    EXPECT_TRUE((trx.begin().ok()));
    EXPECT_TRUE((1 == trx.state()->numCollections()));
    EXPECT_TRUE((nullptr != trx.state()->findCollection(logicalCollection0->id())));
    std::unordered_set<std::string> expectedNames = { "testCollection0" };
    std::unordered_set<std::string> actualNames;
    trx.state()->allCollections([&actualNames](arangodb::TransactionCollection& col)->bool {
      actualNames.emplace(col.collection()->name());
      return true;
    });

    for(auto& entry: actualNames) {
      EXPECT_TRUE((1 == expectedNames.erase(entry)));
    }

    EXPECT_TRUE((expectedNames.empty()));
    EXPECT_TRUE((trx.commit().ok()));
  }

  // exclusive transaction (by name) (one collection dropped)
  {
    arangodb::SingleCollectionTransaction trx(
      arangodb::transaction::StandaloneContext::Create(vocbase),
      logicalView->name(),
      arangodb::AccessMode::Type::READ
    );
    EXPECT_TRUE((trx.begin().ok()));
    EXPECT_TRUE((1 == trx.state()->numCollections()));
    EXPECT_TRUE((nullptr != trx.state()->findCollection(logicalCollection0->id())));
    std::unordered_set<std::string> expectedNames = { "testCollection0" };
    std::unordered_set<std::string> actualNames;
    trx.state()->allCollections([&actualNames](arangodb::TransactionCollection& col)->bool {
      actualNames.emplace(col.collection()->name());
      return true;
    });

    for(auto& entry: actualNames) {
      EXPECT_TRUE((1 == expectedNames.erase(entry)));
    }

    EXPECT_TRUE((expectedNames.empty()));
    EXPECT_TRUE((trx.commit().ok()));
  }
}

TEST_F(IResearchViewTest, test_transaction_snapshot) {
  static std::vector<std::string> const EMPTY;
  auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
  auto linkJson = arangodb::velocypack::Parser::fromJson("{ \"view\": \"testView\", \"includeAllFields\": true }");
  auto viewJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\", \"commitIntervalMsec\": 0 }");
  Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
  auto logicalCollection = vocbase.createCollection(collectionJson->slice());
  ASSERT_TRUE((false == !logicalCollection));
  auto logicalView = vocbase.createView(viewJson->slice());
  ASSERT_TRUE((false == !logicalView));
  auto* viewImpl = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
  ASSERT_TRUE((nullptr != viewImpl));
  std::shared_ptr<arangodb::Index> index = 
    arangodb::iresearch::IResearchMMFilesLink::factory().instantiate(*logicalCollection, linkJson->slice(), 42, false);
  ASSERT_TRUE((false == !index));
  auto link = std::dynamic_pointer_cast<arangodb::iresearch::IResearchLink>(index);
  ASSERT_TRUE((false == !link));

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
    EXPECT_TRUE((trx.begin().ok()));
    EXPECT_TRUE((link->insert(trx, arangodb::LocalDocumentId(0), doc->slice(), arangodb::Index::OperationMode::normal).ok()));
    EXPECT_TRUE((trx.commit().ok()));
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
    EXPECT_TRUE((nullptr == snapshot));
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
    EXPECT_TRUE(nullptr == viewImpl->snapshot(trx, arangodb::iresearch::IResearchView::SnapshotMode::Find));
    auto* snapshot = viewImpl->snapshot(trx, arangodb::iresearch::IResearchView::SnapshotMode::FindOrCreate);
    EXPECT_TRUE(snapshot == viewImpl->snapshot(trx, arangodb::iresearch::IResearchView::SnapshotMode::Find));
    EXPECT_TRUE(snapshot == viewImpl->snapshot(trx, arangodb::iresearch::IResearchView::SnapshotMode::FindOrCreate));
    EXPECT_TRUE(nullptr != snapshot);
    EXPECT_TRUE((0 == snapshot->live_docs_count()));
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
    EXPECT_TRUE((nullptr == snapshot));
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
    EXPECT_TRUE(nullptr == viewImpl->snapshot(trx, arangodb::iresearch::IResearchView::SnapshotMode::Find));
    auto* snapshot = viewImpl->snapshot(trx, arangodb::iresearch::IResearchView::SnapshotMode::SyncAndReplace);
    EXPECT_TRUE(snapshot == viewImpl->snapshot(trx, arangodb::iresearch::IResearchView::SnapshotMode::Find));
    EXPECT_TRUE(snapshot == viewImpl->snapshot(trx, arangodb::iresearch::IResearchView::SnapshotMode::FindOrCreate));
    EXPECT_TRUE((nullptr != snapshot));
    EXPECT_TRUE((1 == snapshot->live_docs_count()));
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
    EXPECT_TRUE((trx.begin().ok()));
    EXPECT_TRUE((link->insert(trx, arangodb::LocalDocumentId(1), doc->slice(), arangodb::Index::OperationMode::normal).ok()));
    EXPECT_TRUE((trx.commit().ok()));
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
    EXPECT_TRUE((true == viewImpl->apply(trx)));
    EXPECT_TRUE((true == trx.begin().ok()));
    auto* snapshot = viewImpl->snapshot(trx);
    EXPECT_TRUE((nullptr != snapshot));
    EXPECT_TRUE((1 == snapshot->live_docs_count()));
    EXPECT_TRUE(true == trx.abort().ok()); // prevent assertion in destructor
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
    EXPECT_TRUE((true == viewImpl->apply(trx)));
    EXPECT_TRUE((true == trx.begin().ok()));
    auto* snapshot = viewImpl->snapshot(trx, arangodb::iresearch::IResearchView::SnapshotMode::FindOrCreate);
    EXPECT_TRUE(snapshot == viewImpl->snapshot(trx, arangodb::iresearch::IResearchView::SnapshotMode::Find));
    EXPECT_TRUE(snapshot == viewImpl->snapshot(trx, arangodb::iresearch::IResearchView::SnapshotMode::FindOrCreate));
    EXPECT_TRUE((nullptr != snapshot));
    EXPECT_TRUE((1 == snapshot->live_docs_count()));
    EXPECT_TRUE(true == trx.abort().ok()); // prevent assertion in destructor
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
    ASSERT_TRUE(state);
    EXPECT_TRUE((true == viewImpl->apply(trx)));
    EXPECT_TRUE((true == trx.begin().ok()));
    state->waitForSync(true);
    auto* snapshot = viewImpl->snapshot(trx, arangodb::iresearch::IResearchView::SnapshotMode::FindOrCreate);
    EXPECT_TRUE(snapshot == viewImpl->snapshot(trx, arangodb::iresearch::IResearchView::SnapshotMode::Find));
    EXPECT_TRUE((nullptr != snapshot));
    EXPECT_TRUE((1 == snapshot->live_docs_count()));
    EXPECT_TRUE(true == trx.abort().ok()); // prevent assertion in destructor
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
    ASSERT_TRUE(state);
    EXPECT_TRUE((true == viewImpl->apply(trx)));
    EXPECT_TRUE((true == trx.begin().ok()));
    auto* snapshot = viewImpl->snapshot(trx, arangodb::iresearch::IResearchView::SnapshotMode::SyncAndReplace);
    EXPECT_TRUE(snapshot == viewImpl->snapshot(trx, arangodb::iresearch::IResearchView::SnapshotMode::Find));
    EXPECT_TRUE((nullptr != snapshot));
    EXPECT_TRUE((2 == snapshot->live_docs_count()));
    EXPECT_TRUE(true == trx.abort().ok()); // prevent assertion in destructor
  }
}

TEST_F(IResearchViewTest, test_update_overwrite) {
  auto createJson = arangodb::velocypack::Parser::fromJson("{ \
    \"name\": \"testView\", \
    \"type\": \"arangosearch\", \
    \"cleanupIntervalStep\": 52 \
  }");

  // modify meta params
  {
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto view = vocbase.createView(createJson->slice());
    ASSERT_TRUE((false == !view));

    // initial update (overwrite)
    {
      arangodb::iresearch::IResearchViewMeta expectedMeta;
      arangodb::iresearch::IResearchViewMetaState expectedMetaState;
      auto updateJson = arangodb::velocypack::Parser::fromJson("{ \
        \"cleanupIntervalStep\": 42 \
      }");

      expectedMeta._cleanupIntervalStep = 42;
      EXPECT_TRUE((view->properties(updateJson->slice(), false).ok()));

      // not for persistence
      {
        arangodb::velocypack::Builder builder;

        builder.openObject();
        view->properties(builder, arangodb::LogicalDataSource::makeFlags(
                                      arangodb::LogicalDataSource::Serialize::Detailed));
        builder.close();

        auto slice = builder.slice();
        arangodb::iresearch::IResearchViewMeta meta;
        std::string error;

        EXPECT_TRUE(slice.isObject());
        EXPECT_TRUE((13U == slice.length()));
        EXPECT_TRUE((slice.hasKey("globallyUniqueId") && slice.get("globallyUniqueId").isString() && false == slice.get("globallyUniqueId").copyString().empty()));
        EXPECT_TRUE(slice.get("name").copyString() == "testView");
        EXPECT_TRUE(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
        EXPECT_TRUE(slice.get("deleted").isNone()); // no system properties
        EXPECT_TRUE((meta.init(slice, error) && expectedMeta == meta));
        auto tmpSlice = slice.get("links");
        EXPECT_TRUE((true == tmpSlice.isObject() && 0 == tmpSlice.length()));
      }

      // for persistence
      {
        arangodb::velocypack::Builder builder;

        builder.openObject();
        view->properties(builder, arangodb::LogicalDataSource::makeFlags(
                                      arangodb::LogicalDataSource::Serialize::Detailed,
                                      arangodb::LogicalDataSource::Serialize::ForPersistence));
        builder.close();

        auto slice = builder.slice();
        arangodb::iresearch::IResearchViewMeta meta;
        arangodb::iresearch::IResearchViewMetaState metaState;
        std::string error;

        EXPECT_TRUE(slice.isObject());
        EXPECT_TRUE((17U == slice.length()));
        EXPECT_TRUE(slice.get("name").copyString() == "testView");
        EXPECT_TRUE(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
        EXPECT_TRUE((slice.hasKey("deleted") && slice.get("deleted").isBoolean() && false == slice.get("deleted").getBoolean())); // has system properties
        EXPECT_TRUE((meta.init(slice, error) && expectedMeta == meta));
        EXPECT_TRUE((true == metaState.init(slice, error) && expectedMetaState == metaState));
        EXPECT_TRUE((false == slice.hasKey("links")));
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
      EXPECT_TRUE((view->properties(updateJson->slice(), false).ok()));

      // not for persistence
      {
        arangodb::velocypack::Builder builder;

        builder.openObject();
        view->properties(builder, arangodb::LogicalDataSource::makeFlags(
                                      arangodb::LogicalDataSource::Serialize::Detailed));
        builder.close();

        auto slice = builder.slice();
        arangodb::iresearch::IResearchViewMeta meta;
        arangodb::iresearch::IResearchViewMetaState metaState;
        std::string error;

        EXPECT_TRUE(slice.isObject());
        EXPECT_TRUE((13U == slice.length()));
        EXPECT_TRUE((slice.hasKey("globallyUniqueId") && slice.get("globallyUniqueId").isString() && false == slice.get("globallyUniqueId").copyString().empty()));
        EXPECT_TRUE(slice.get("name").copyString() == "testView");
        EXPECT_TRUE(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
        EXPECT_TRUE(slice.get("deleted").isNone()); // no system properties
        EXPECT_TRUE((meta.init(slice, error) && expectedMeta == meta));
        auto tmpSlice = slice.get("links");
        EXPECT_TRUE((true == tmpSlice.isObject() && 0 == tmpSlice.length()));
      }

      // for persistence
      {
        arangodb::velocypack::Builder builder;

        builder.openObject();
        view->properties(builder, arangodb::LogicalDataSource::makeFlags(
                                      arangodb::LogicalDataSource::Serialize::Detailed,
                                      arangodb::LogicalDataSource::Serialize::ForPersistence));
        builder.close();

        auto slice = builder.slice();
        arangodb::iresearch::IResearchViewMeta meta;
        arangodb::iresearch::IResearchViewMetaState metaState;
        std::string error;

        EXPECT_TRUE(slice.isObject());
        EXPECT_TRUE((17U == slice.length()));
        EXPECT_TRUE(slice.get("name").copyString() == "testView");
        EXPECT_TRUE(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
        EXPECT_TRUE((slice.hasKey("deleted") && slice.get("deleted").isBoolean() && false == slice.get("deleted").getBoolean())); // has system properties
        EXPECT_TRUE((meta.init(slice, error) && expectedMeta == meta));
        EXPECT_TRUE((true == metaState.init(slice, error) && expectedMetaState == metaState));
        EXPECT_TRUE((false == slice.hasKey("links")));
      }
    }
  }

  // test rollback on meta modification failure (as an example invalid value for 'cleanupIntervalStep')
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalView = vocbase.createView(createJson->slice());
    ASSERT_TRUE((false == !logicalView));
    ASSERT_TRUE((logicalView->category() == arangodb::LogicalView::category()));

    arangodb::iresearch::IResearchViewMeta expectedMeta;
    arangodb::iresearch::IResearchViewMetaState expectedMetaState;
    auto updateJson = arangodb::velocypack::Parser::fromJson("{ \"cleanupIntervalStep\": 0.123 }");

    expectedMeta._cleanupIntervalStep = 52;
    EXPECT_TRUE((TRI_ERROR_BAD_PARAMETER == logicalView->properties(updateJson->slice(), false).errorNumber()));

    // not for persistence
    {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      logicalView->properties(builder, arangodb::LogicalDataSource::makeFlags(
                                           arangodb::LogicalDataSource::Serialize::Detailed));
      builder.close();

      auto slice = builder.slice();
      arangodb::iresearch::IResearchViewMeta meta;
      arangodb::iresearch::IResearchViewMetaState metaState;
      std::string error;

      EXPECT_TRUE((slice.isObject()));
      EXPECT_TRUE((13U == slice.length()));
      EXPECT_TRUE((slice.hasKey("globallyUniqueId") && slice.get("globallyUniqueId").isString() && false == slice.get("globallyUniqueId").copyString().empty()));
      EXPECT_TRUE((slice.get("name").copyString() == "testView"));
      EXPECT_TRUE((slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name()));
      EXPECT_TRUE((slice.get("deleted").isNone())); // no system properties
      EXPECT_TRUE((meta.init(slice, error) && expectedMeta == meta));
      auto tmpSlice = slice.get("links");
      EXPECT_TRUE((true == tmpSlice.isObject() && 0 == tmpSlice.length()));
    }

    // for persistence
    {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      logicalView->properties(builder,
                              arangodb::LogicalDataSource::makeFlags(
                                  arangodb::LogicalDataSource::Serialize::Detailed,
                                  arangodb::LogicalDataSource::Serialize::ForPersistence));
      builder.close();

      auto slice = builder.slice();
      arangodb::iresearch::IResearchViewMeta meta;
      arangodb::iresearch::IResearchViewMetaState metaState;
      std::string error;

      EXPECT_TRUE((slice.isObject()));
      EXPECT_TRUE((17U == slice.length()));
      EXPECT_TRUE((slice.get("name").copyString() == "testView"));
      EXPECT_TRUE((slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name()));
      EXPECT_TRUE((slice.hasKey("deleted") && slice.get("deleted").isBoolean() && false == slice.get("deleted").getBoolean())); // has system properties
      EXPECT_TRUE((meta.init(slice, error) && expectedMeta == meta));
      EXPECT_TRUE((true == metaState.init(slice, error) && expectedMetaState == metaState));
      EXPECT_TRUE((false == slice.hasKey("links")));
    }
  }

  // modify meta params with links to missing collections
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalView = vocbase.createView(createJson->slice());
    ASSERT_TRUE((false == !logicalView));
    ASSERT_TRUE((logicalView->category() == arangodb::LogicalView::category()));

    arangodb::iresearch::IResearchViewMeta expectedMeta;
    arangodb::iresearch::IResearchViewMetaState expectedMetaState;
    auto updateJson = arangodb::velocypack::Parser::fromJson("{ \"cleanupIntervalStep\": 62, \"links\": { \"testCollection\": {} } }");

    expectedMeta._cleanupIntervalStep = 52;
    EXPECT_TRUE((TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND == logicalView->properties(updateJson->slice(), false).errorNumber()));
    EXPECT_TRUE((true == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));

    // not for persistence
    {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      logicalView->properties(builder, arangodb::LogicalDataSource::makeFlags(
                                           arangodb::LogicalDataSource::Serialize::Detailed));
      builder.close();

      auto slice = builder.slice();
      arangodb::iresearch::IResearchViewMeta meta;
      arangodb::iresearch::IResearchViewMetaState metaState;
      std::string error;

      EXPECT_TRUE((slice.isObject()));
      EXPECT_TRUE((13U == slice.length()));
      EXPECT_TRUE((slice.hasKey("globallyUniqueId") && slice.get("globallyUniqueId").isString() && false == slice.get("globallyUniqueId").copyString().empty()));
      EXPECT_TRUE((slice.get("name").copyString() == "testView"));
      EXPECT_TRUE((slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name()));
      EXPECT_TRUE((slice.get("deleted").isNone())); // no system properties
      EXPECT_TRUE((meta.init(slice, error) && expectedMeta == meta));
      auto tmpSlice = slice.get("links");
      EXPECT_TRUE((true == tmpSlice.isObject() && 0 == tmpSlice.length()));
    }

    // for persistence
    {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      logicalView->properties(builder,
                              arangodb::LogicalDataSource::makeFlags(
                                  arangodb::LogicalDataSource::Serialize::Detailed,
                                  arangodb::LogicalDataSource::Serialize::ForPersistence));
      builder.close();

      auto slice = builder.slice();
      arangodb::iresearch::IResearchViewMeta meta;
      arangodb::iresearch::IResearchViewMetaState metaState;
      std::string error;

      EXPECT_TRUE((slice.isObject()));
      EXPECT_TRUE((17U == slice.length()));
      EXPECT_TRUE((slice.get("name").copyString() == "testView"));
      EXPECT_TRUE((slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name()));
      EXPECT_TRUE((slice.hasKey("deleted") && slice.get("deleted").isBoolean() && false == slice.get("deleted").getBoolean())); // has system properties
      EXPECT_TRUE((meta.init(slice, error) && expectedMeta == meta));
      EXPECT_TRUE((true == metaState.init(slice, error) && expectedMetaState == metaState));
      EXPECT_TRUE((false == slice.hasKey("links")));
    }
  }

  // modify meta params with links with invalid definition
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    ASSERT_TRUE((nullptr != logicalCollection));
    auto logicalView = vocbase.createView(createJson->slice());
    ASSERT_TRUE((false == !logicalView));
    ASSERT_TRUE((logicalView->category() == arangodb::LogicalView::category()));
    EXPECT_TRUE((true == logicalCollection->getIndexes().empty()));

    arangodb::iresearch::IResearchViewMeta expectedMeta;
    arangodb::iresearch::IResearchViewMetaState expectedMetaState;
    auto updateJson = arangodb::velocypack::Parser::fromJson("{ \"cleanupIntervalStep\": 62, \"links\": { \"testCollection\": 42 } }");

    expectedMeta._cleanupIntervalStep = 52;
    EXPECT_TRUE((TRI_ERROR_BAD_PARAMETER == logicalView->properties(updateJson->slice(), false).errorNumber()));
    EXPECT_TRUE((true == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));

    // not for persistence
    {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      logicalView->properties(builder, arangodb::LogicalDataSource::makeFlags(
                                           arangodb::LogicalDataSource::Serialize::Detailed));
      builder.close();

      auto slice = builder.slice();
      arangodb::iresearch::IResearchViewMeta meta;
      arangodb::iresearch::IResearchViewMetaState metaState;
      std::string error;

      EXPECT_TRUE((slice.isObject()));
      EXPECT_TRUE((13U == slice.length()));
      EXPECT_TRUE((slice.hasKey("globallyUniqueId") && slice.get("globallyUniqueId").isString() && false == slice.get("globallyUniqueId").copyString().empty()));
      EXPECT_TRUE((slice.get("name").copyString() == "testView"));
      EXPECT_TRUE((slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name()));
      EXPECT_TRUE((slice.get("deleted").isNone())); // no system properties
      EXPECT_TRUE((meta.init(slice, error) && expectedMeta == meta));
      auto tmpSlice = slice.get("links");
      EXPECT_TRUE((true == tmpSlice.isObject() && 0 == tmpSlice.length()));
    }

    // for persistence
    {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      logicalView->properties(builder,
                              arangodb::LogicalDataSource::makeFlags(
                                  arangodb::LogicalDataSource::Serialize::Detailed,
                                  arangodb::LogicalDataSource::Serialize::ForPersistence));
      builder.close();

      auto slice = builder.slice();
      arangodb::iresearch::IResearchViewMeta meta;
      arangodb::iresearch::IResearchViewMetaState metaState;
      std::string error;

      EXPECT_TRUE((slice.isObject()));
      EXPECT_TRUE((17U == slice.length()));
      EXPECT_TRUE((slice.get("name").copyString() == "testView"));
      EXPECT_TRUE((slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name()));
      EXPECT_TRUE((slice.hasKey("deleted") && slice.get("deleted").isBoolean() && false == slice.get("deleted").getBoolean())); // has system properties
      EXPECT_TRUE((meta.init(slice, error) && expectedMeta == meta));
      EXPECT_TRUE((true == metaState.init(slice, error) && expectedMetaState == metaState));
      EXPECT_TRUE((false == slice.hasKey("links")));
    }
  }

  // modify meta params with links
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    ASSERT_TRUE((nullptr != logicalCollection));
    auto logicalView = vocbase.createView(createJson->slice());
    ASSERT_TRUE((false == !logicalView));
    ASSERT_TRUE((logicalView->category() == arangodb::LogicalView::category()));
    EXPECT_TRUE((true == logicalCollection->getIndexes().empty()));

    // initial link creation
    {
      auto updateJson = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection\": {} } }");
      arangodb::iresearch::IResearchViewMeta expectedMeta;
      arangodb::iresearch::IResearchViewMetaState expectedMetaState;
      std::unordered_map<std::string, arangodb::iresearch::IResearchLinkMeta> expectedLinkMeta;

      expectedMeta._cleanupIntervalStep = 52;
      expectedMetaState._collections.insert(logicalCollection->id());
      expectedLinkMeta["testCollection"]; // use defaults
      EXPECT_TRUE((logicalView->properties(updateJson->slice(), true).ok()));

      // not for persistence
      {
        arangodb::velocypack::Builder builder;

        builder.openObject();
        logicalView->properties(builder, arangodb::LogicalDataSource::makeFlags(
                                             arangodb::LogicalDataSource::Serialize::Detailed));
        builder.close();

        auto slice = builder.slice();
        arangodb::iresearch::IResearchViewMeta meta;
        arangodb::iresearch::IResearchViewMetaState metaState;
        std::string error;

        EXPECT_TRUE((slice.isObject()));
        EXPECT_TRUE((13U == slice.length()));
        EXPECT_TRUE((slice.hasKey("globallyUniqueId") && slice.get("globallyUniqueId").isString() && false == slice.get("globallyUniqueId").copyString().empty()));
        EXPECT_TRUE((slice.get("name").copyString() == "testView"));
        EXPECT_TRUE((slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name()));
        EXPECT_TRUE((slice.get("deleted").isNone())); // no system properties
        EXPECT_TRUE((meta.init(slice, error) && expectedMeta == meta));

        auto tmpSlice = slice.get("links");
        EXPECT_TRUE((true == tmpSlice.isObject() && 1 == tmpSlice.length()));

        for (arangodb::velocypack::ObjectIterator itr(tmpSlice); itr.valid(); ++itr) {
          arangodb::iresearch::IResearchLinkMeta linkMeta;
          auto key = itr.key();
          auto value = itr.value();
          EXPECT_TRUE((true == key.isString()));

          auto expectedItr = expectedLinkMeta.find(key.copyString());
          EXPECT_TRUE((
            true == value.isObject()
            && expectedItr != expectedLinkMeta.end()
            && linkMeta.init(value, false, error)
            && expectedItr->second == linkMeta
          ));
          expectedLinkMeta.erase(expectedItr);
        }
      }

      // for persistence
      {
        arangodb::velocypack::Builder builder;

        builder.openObject();
        logicalView->properties(builder,
                                arangodb::LogicalDataSource::makeFlags(
                                    arangodb::LogicalDataSource::Serialize::Detailed,
                                    arangodb::LogicalDataSource::Serialize::ForPersistence));
        builder.close();

        auto slice = builder.slice();
        arangodb::iresearch::IResearchViewMeta meta;
        arangodb::iresearch::IResearchViewMetaState metaState;
        std::string error;

        EXPECT_TRUE((slice.isObject()));
        EXPECT_TRUE((17U == slice.length()));
        EXPECT_TRUE((slice.get("name").copyString() == "testView"));
        EXPECT_TRUE((slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name()));
        EXPECT_TRUE((slice.hasKey("deleted") && slice.get("deleted").isBoolean() && false == slice.get("deleted").getBoolean())); // has system properties
        EXPECT_TRUE((meta.init(slice, error) && expectedMeta == meta));
        EXPECT_TRUE((true == metaState.init(slice, error) && expectedMetaState == metaState));
        EXPECT_TRUE((false == slice.hasKey("links")));
      }

      EXPECT_TRUE((true == expectedLinkMeta.empty()));
      EXPECT_TRUE((false == logicalCollection->getIndexes().empty()));
    }

    // subsequent update (overwrite)
    {
      arangodb::iresearch::IResearchViewMeta expectedMeta;
      arangodb::iresearch::IResearchViewMetaState expectedMetaState;
      auto updateJson = arangodb::velocypack::Parser::fromJson("{ \
        \"cleanupIntervalStep\": 62 \
      }");

      expectedMeta._cleanupIntervalStep = 62;
      EXPECT_TRUE((logicalView->properties(updateJson->slice(), false).ok()));

      // not for persistence
      {
        arangodb::velocypack::Builder builder;

        builder.openObject();
        logicalView->properties(builder, arangodb::LogicalDataSource::makeFlags(
                                             arangodb::LogicalDataSource::Serialize::Detailed));
        builder.close();

        auto slice = builder.slice();
        arangodb::iresearch::IResearchViewMeta meta;
        arangodb::iresearch::IResearchViewMetaState metaState;
        std::string error;

        EXPECT_TRUE((slice.isObject()));
        EXPECT_TRUE((13U == slice.length()));
        EXPECT_TRUE((slice.hasKey("globallyUniqueId") && slice.get("globallyUniqueId").isString() && false == slice.get("globallyUniqueId").copyString().empty()));
        EXPECT_TRUE((slice.get("name").copyString() == "testView"));
        EXPECT_TRUE((slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name()));
        EXPECT_TRUE((slice.get("deleted").isNone())); // no system properties
        EXPECT_TRUE((meta.init(slice, error) && expectedMeta == meta));
        auto tmpSlice = slice.get("links");
        EXPECT_TRUE((true == tmpSlice.isObject() && 0 == tmpSlice.length()));
      }

      // for persistence
      {
        arangodb::velocypack::Builder builder;

        builder.openObject();
        logicalView->properties(builder,
                                arangodb::LogicalDataSource::makeFlags(
                                    arangodb::LogicalDataSource::Serialize::Detailed,
                                    arangodb::LogicalDataSource::Serialize::ForPersistence));
        builder.close();

        auto slice = builder.slice();
        arangodb::iresearch::IResearchViewMeta meta;
        arangodb::iresearch::IResearchViewMetaState metaState;
        std::string error;

        EXPECT_TRUE((slice.isObject()));
        EXPECT_TRUE((17U == slice.length()));
        EXPECT_TRUE((slice.get("name").copyString() == "testView"));
        EXPECT_TRUE((slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name()));
        EXPECT_TRUE((slice.hasKey("deleted") && slice.get("deleted").isBoolean() && false == slice.get("deleted").getBoolean())); // has system properties
        EXPECT_TRUE((meta.init(slice, error) && expectedMeta == meta));
        EXPECT_TRUE((true == metaState.init(slice, error) && expectedMetaState == metaState));
        EXPECT_TRUE((false == slice.hasKey("links")));
      }
    }
  }

  // overwrite links
  {
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto collectionJson0 = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection0\" }");
    auto collectionJson1 = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection1\" }");
    auto logicalCollection0 = vocbase.createCollection(collectionJson0->slice());
    ASSERT_TRUE((nullptr != logicalCollection0));
    auto logicalCollection1 = vocbase.createCollection(collectionJson1->slice());
    ASSERT_TRUE((nullptr != logicalCollection1));
    auto view = vocbase.createView(createJson->slice());
    ASSERT_TRUE((false == !view));
    ASSERT_TRUE(view->category() == arangodb::LogicalView::category());
    EXPECT_TRUE((true == logicalCollection0->getIndexes().empty()));
    EXPECT_TRUE((true == logicalCollection1->getIndexes().empty()));

    // initial creation
    {
      auto updateJson = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection0\": {} } }");
      arangodb::iresearch::IResearchViewMeta expectedMeta;
      arangodb::iresearch::IResearchViewMetaState expectedMetaState;
      std::unordered_map<std::string, arangodb::iresearch::IResearchLinkMeta> expectedLinkMeta;

      expectedMeta._cleanupIntervalStep = 52;
      expectedMetaState._collections.insert(logicalCollection0->id());
      expectedLinkMeta["testCollection0"]; // use defaults
      EXPECT_TRUE((view->properties(updateJson->slice(), true).ok()));

      // not for persistence
      {
        arangodb::velocypack::Builder builder;

        builder.openObject();
        view->properties(builder, arangodb::LogicalDataSource::makeFlags(
                                      arangodb::LogicalDataSource::Serialize::Detailed));
        builder.close();

        auto slice = builder.slice();
        arangodb::iresearch::IResearchViewMeta meta;
        arangodb::iresearch::IResearchViewMetaState metaState;
        std::string error;

        EXPECT_TRUE(slice.isObject());
        EXPECT_TRUE((13U == slice.length()));
        EXPECT_TRUE((slice.hasKey("globallyUniqueId") && slice.get("globallyUniqueId").isString() && false == slice.get("globallyUniqueId").copyString().empty()));
        EXPECT_TRUE(slice.get("name").copyString() == "testView");
        EXPECT_TRUE(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
        EXPECT_TRUE(slice.get("deleted").isNone()); // no system properties
        EXPECT_TRUE((meta.init(slice, error) && expectedMeta == meta));

        auto tmpSlice = slice.get("links");
        EXPECT_TRUE((true == tmpSlice.isObject() && 1 == tmpSlice.length()));

        for (arangodb::velocypack::ObjectIterator itr(tmpSlice); itr.valid(); ++itr) {
          arangodb::iresearch::IResearchLinkMeta linkMeta;
          auto key = itr.key();
          auto value = itr.value();
          EXPECT_TRUE((true == key.isString()));

          auto expectedItr = expectedLinkMeta.find(key.copyString());
          EXPECT_TRUE((
            true == value.isObject()
            && expectedItr != expectedLinkMeta.end()
            && linkMeta.init(value, false, error)
            && expectedItr->second == linkMeta
          ));
          expectedLinkMeta.erase(expectedItr);
        }
      }

      // for persistence
      {
        arangodb::velocypack::Builder builder;

        builder.openObject();
        view->properties(builder, arangodb::LogicalDataSource::makeFlags(
                                      arangodb::LogicalDataSource::Serialize::Detailed,
                                      arangodb::LogicalDataSource::Serialize::ForPersistence));
        builder.close();

        auto slice = builder.slice();
        arangodb::iresearch::IResearchViewMeta meta;
        arangodb::iresearch::IResearchViewMetaState metaState;
        std::string error;

        EXPECT_TRUE(slice.isObject());
        EXPECT_TRUE((17U == slice.length()));
        EXPECT_TRUE(slice.get("name").copyString() == "testView");
        EXPECT_TRUE(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
        EXPECT_TRUE((slice.hasKey("deleted") && slice.get("deleted").isBoolean() && false == slice.get("deleted").getBoolean())); // has system properties
        EXPECT_TRUE((meta.init(slice, error) && expectedMeta == meta));
        EXPECT_TRUE((true == metaState.init(slice, error) && expectedMetaState == metaState));
        EXPECT_TRUE((false == slice.hasKey("links")));
      }

      EXPECT_TRUE((true == expectedLinkMeta.empty()));
      EXPECT_TRUE((false == logicalCollection0->getIndexes().empty()));
      EXPECT_TRUE((true == logicalCollection1->getIndexes().empty()));
    }

    // update overwrite links
    {
      auto updateJson = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection1\": {} } }");
      arangodb::iresearch::IResearchViewMeta expectedMeta;
      arangodb::iresearch::IResearchViewMetaState expectedMetaState;
      std::unordered_map<std::string, arangodb::iresearch::IResearchLinkMeta> expectedLinkMeta;

      expectedMetaState._collections.insert(logicalCollection1->id());
      expectedLinkMeta["testCollection1"]; // use defaults
      EXPECT_TRUE((view->properties(updateJson->slice(), false).ok()));

      // not for persistence
      {
        arangodb::velocypack::Builder builder;

        builder.openObject();
        view->properties(builder, arangodb::LogicalDataSource::makeFlags(
                                      arangodb::LogicalDataSource::Serialize::Detailed));
        builder.close();

        auto slice = builder.slice();
        arangodb::iresearch::IResearchViewMeta meta;
        arangodb::iresearch::IResearchViewMetaState metaState;
        std::string error;

        EXPECT_TRUE(slice.isObject());
        EXPECT_TRUE((13U == slice.length()));
        EXPECT_TRUE((slice.hasKey("globallyUniqueId") && slice.get("globallyUniqueId").isString() && false == slice.get("globallyUniqueId").copyString().empty()));
        EXPECT_TRUE(slice.get("name").copyString() == "testView");
        EXPECT_TRUE(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
        EXPECT_TRUE(slice.get("deleted").isNone()); // no system properties
        EXPECT_TRUE((meta.init(slice, error) && expectedMeta == meta));

        auto tmpSlice = slice.get("links");
        EXPECT_TRUE((true == tmpSlice.isObject() && 1 == tmpSlice.length()));

        for (arangodb::velocypack::ObjectIterator itr(tmpSlice); itr.valid(); ++itr) {
          arangodb::iresearch::IResearchLinkMeta linkMeta;
          auto key = itr.key();
          auto value = itr.value();
          EXPECT_TRUE((true == key.isString()));

          auto expectedItr = expectedLinkMeta.find(key.copyString());
          EXPECT_TRUE((
            true == value.isObject()
            && expectedItr != expectedLinkMeta.end()
            && linkMeta.init(value, false, error)
            && expectedItr->second == linkMeta
          ));
          expectedLinkMeta.erase(expectedItr);
        }
      }

      // for persistence
      {
        arangodb::velocypack::Builder builder;

        builder.openObject();
        view->properties(builder, arangodb::LogicalDataSource::makeFlags(
                                      arangodb::LogicalDataSource::Serialize::Detailed,
                                      arangodb::LogicalDataSource::Serialize::ForPersistence));
        builder.close();

        auto slice = builder.slice();
        arangodb::iresearch::IResearchViewMeta meta;
        arangodb::iresearch::IResearchViewMetaState metaState;
        std::string error;

        EXPECT_TRUE(slice.isObject());
        EXPECT_TRUE((17U == slice.length()));
        EXPECT_TRUE(slice.get("name").copyString() == "testView");
        EXPECT_TRUE(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
        EXPECT_TRUE((slice.hasKey("deleted") && slice.get("deleted").isBoolean() && false == slice.get("deleted").getBoolean())); // has system properties
        EXPECT_TRUE((meta.init(slice, error) && expectedMeta == meta));
        EXPECT_TRUE((true == metaState.init(slice, error) && expectedMetaState == metaState));
        EXPECT_TRUE((false == slice.hasKey("links")));
      }

      EXPECT_TRUE((true == expectedLinkMeta.empty()));
      EXPECT_TRUE((true == logicalCollection0->getIndexes().empty()));
      EXPECT_TRUE((false == logicalCollection1->getIndexes().empty()));
    }
  }

  // update existing link (full update)
  {
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    ASSERT_TRUE((nullptr != logicalCollection));
    auto view = vocbase.createView(createJson->slice());
    ASSERT_TRUE((false == !view));
    ASSERT_TRUE(view->category() == arangodb::LogicalView::category());

    // initial add of link
    {
      auto updateJson = arangodb::velocypack::Parser::fromJson(
        "{ \"links\": { \"testCollection\": { \"includeAllFields\": true } } }"
      );
      EXPECT_TRUE((view->properties(updateJson->slice(), true).ok()));

      // not for persistence
      {
        arangodb::velocypack::Builder builder;

        builder.openObject();
        view->properties(builder, arangodb::LogicalDataSource::makeFlags(
                                      arangodb::LogicalDataSource::Serialize::Detailed));
        builder.close();

        auto slice = builder.slice();
        EXPECT_TRUE(slice.isObject());
        EXPECT_TRUE((13U == slice.length()));
        EXPECT_TRUE((slice.hasKey("globallyUniqueId") && slice.get("globallyUniqueId").isString() && false == slice.get("globallyUniqueId").copyString().empty()));
        EXPECT_TRUE(slice.get("name").copyString() == "testView");
        EXPECT_TRUE(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
        EXPECT_TRUE(slice.get("deleted").isNone()); // no system properties
        auto tmpSlice = slice.get("links");
        EXPECT_TRUE((true == tmpSlice.isObject() && 1 == tmpSlice.length()));
        tmpSlice = tmpSlice.get("testCollection");
        EXPECT_TRUE((true == tmpSlice.isObject()));
        tmpSlice = tmpSlice.get("includeAllFields");
        EXPECT_TRUE((true == tmpSlice.isBoolean() && true == tmpSlice.getBoolean()));
      }

      // for persistence
      {
        arangodb::velocypack::Builder builder;

        builder.openObject();
        view->properties(builder, arangodb::LogicalDataSource::makeFlags(
                                      arangodb::LogicalDataSource::Serialize::Detailed,
                                      arangodb::LogicalDataSource::Serialize::ForPersistence));
        builder.close();

        auto slice = builder.slice();
        EXPECT_TRUE(slice.isObject());
        EXPECT_TRUE((17U == slice.length()));
        EXPECT_TRUE(slice.get("name").copyString() == "testView");
        EXPECT_TRUE(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
        EXPECT_TRUE((slice.hasKey("deleted") && slice.get("deleted").isBoolean() && false == slice.get("deleted").getBoolean())); // has system properties
        auto tmpSlice = slice.get("collections");
        EXPECT_TRUE((true == tmpSlice.isArray() && 1 == tmpSlice.length()));
        EXPECT_TRUE((false == slice.hasKey("links")));
      }
    }

    // update link
    {
      auto updateJson = arangodb::velocypack::Parser::fromJson(
        "{ \"links\": { \"testCollection\": { } } }"
      );
      EXPECT_TRUE((view->properties(updateJson->slice(), false).ok()));

      // not for persistence
      {
        arangodb::velocypack::Builder builder;

        builder.openObject();
        view->properties(builder, arangodb::LogicalDataSource::makeFlags(
                                      arangodb::LogicalDataSource::Serialize::Detailed));
        builder.close();

        auto slice = builder.slice();
        EXPECT_TRUE((13U == slice.length()));
        EXPECT_TRUE((slice.hasKey("globallyUniqueId") && slice.get("globallyUniqueId").isString() && false == slice.get("globallyUniqueId").copyString().empty()));
        EXPECT_TRUE(slice.get("name").copyString() == "testView");
        EXPECT_TRUE(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
        EXPECT_TRUE(slice.get("deleted").isNone()); // no system properties
        auto tmpSlice = slice.get("links");
        EXPECT_TRUE((true == tmpSlice.isObject() && 1 == tmpSlice.length()));
        tmpSlice = tmpSlice.get("testCollection");
        EXPECT_TRUE((true == tmpSlice.isObject()));
        tmpSlice = tmpSlice.get("includeAllFields");
        EXPECT_TRUE((true == tmpSlice.isBoolean() && false == tmpSlice.getBoolean()));
      }

      // for persistence
      {
        arangodb::velocypack::Builder builder;

        builder.openObject();
        view->properties(builder, arangodb::LogicalDataSource::makeFlags(
                                      arangodb::LogicalDataSource::Serialize::Detailed,
                                      arangodb::LogicalDataSource::Serialize::ForPersistence));
        builder.close();

        auto slice = builder.slice();
        EXPECT_TRUE((17U == slice.length()));
        EXPECT_TRUE(slice.get("name").copyString() == "testView");
        EXPECT_TRUE(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
        EXPECT_TRUE((slice.hasKey("deleted") && slice.get("deleted").isBoolean() && false == slice.get("deleted").getBoolean())); // has system properties
        auto tmpSlice = slice.get("collections");
        EXPECT_TRUE((true == tmpSlice.isArray() && 1 == tmpSlice.length()));
        EXPECT_TRUE((false == slice.hasKey("links")));
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
    ASSERT_TRUE((nullptr != logicalCollection));
    auto logicalView = vocbase.createView(viewCreateJson->slice());
    ASSERT_TRUE((false == !logicalView));

    EXPECT_TRUE((true == logicalCollection->getIndexes().empty()));
    EXPECT_TRUE((true == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));

    // initial link creation
    {
      auto updateJson = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection\": {} } }");
      EXPECT_TRUE((logicalView->properties(updateJson->slice(), true).ok()));
      EXPECT_TRUE((false == logicalCollection->getIndexes().empty()));
      EXPECT_TRUE((false == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));
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

      EXPECT_TRUE((TRI_ERROR_FORBIDDEN == logicalView->properties(viewUpdateJson->slice(), false).errorNumber()));

      arangodb::velocypack::Builder builder;
      builder.openObject();
      logicalView->properties(builder,
                              arangodb::LogicalDataSource::makeFlags(
                                  arangodb::LogicalDataSource::Serialize::Detailed,
                                  arangodb::LogicalDataSource::Serialize::ForPersistence));  // 'forPersistence' to avoid auth check
      builder.close();

      auto slice = builder.slice();
      arangodb::iresearch::IResearchViewMeta meta;
      std::string error;
      EXPECT_TRUE((meta.init(slice, error) && expectedMeta == meta));
    }

    // subsequent update (overwrite) authorised (RO collection)
    {
      arangodb::auth::UserMap userMap;
      auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
      user.grantCollection(vocbase.name(), "testCollection", arangodb::auth::Level::RO); // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

      arangodb::iresearch::IResearchViewMeta expectedMeta;
      expectedMeta._cleanupIntervalStep = 62;

      EXPECT_TRUE((logicalView->properties(viewUpdateJson->slice(), false).ok()));

      arangodb::velocypack::Builder builder;
      builder.openObject();
      logicalView->properties(builder,
                              arangodb::LogicalDataSource::makeFlags(
                                  arangodb::LogicalDataSource::Serialize::Detailed,
                                  arangodb::LogicalDataSource::Serialize::ForPersistence));  // 'forPersistence' to avoid auth check
      builder.close();

      auto slice = builder.slice();
      arangodb::iresearch::IResearchViewMeta meta;
      std::string error;
      EXPECT_TRUE((meta.init(slice, error) && expectedMeta == meta));
    }
  }

  // add link (collection not authorized)
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\", \"id\": 100 }");
    auto viewCreateJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
    auto viewUpdateJson = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection\": {} } }");

    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    ASSERT_TRUE((nullptr != logicalCollection));
    auto logicalView = vocbase.createView(viewCreateJson->slice());
    ASSERT_TRUE((false == !logicalView));

    EXPECT_TRUE((true == logicalCollection->getIndexes().empty()));
    EXPECT_TRUE((true == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));

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

    EXPECT_TRUE((TRI_ERROR_FORBIDDEN == logicalView->properties(viewUpdateJson->slice(), false).errorNumber()));
    EXPECT_TRUE((true == logicalCollection->getIndexes().empty()));
    EXPECT_TRUE((true == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));
  }

  // drop link (collection not authorized)
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\", \"id\": 100 }");
    auto viewCreateJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
    auto viewUpdateJson = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection\": null } }");

    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    ASSERT_TRUE((nullptr != logicalCollection));
    auto logicalView = vocbase.createView(viewCreateJson->slice());
    ASSERT_TRUE((false == !logicalView));

    EXPECT_TRUE((true == logicalCollection->getIndexes().empty()));
    EXPECT_TRUE((true == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));

    // initial link creation
    {
      auto updateJson = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection\": {} } }");
      EXPECT_TRUE((logicalView->properties(updateJson->slice(), true).ok()));
      EXPECT_TRUE((false == logicalCollection->getIndexes().empty()));
      EXPECT_TRUE((false == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));
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

      EXPECT_TRUE((TRI_ERROR_FORBIDDEN == logicalView->properties(viewUpdateJson->slice(), false).errorNumber()));
      EXPECT_TRUE((false == logicalCollection->getIndexes().empty()));
      EXPECT_TRUE((false == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));
    }

    // subsequent update (overwrite) authorised (RO collection)
    {
      arangodb::auth::UserMap userMap;
      auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
      user.grantCollection(vocbase.name(), "testCollection", arangodb::auth::Level::RO); // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

      EXPECT_TRUE((logicalView->properties(viewUpdateJson->slice(), false).ok()));
      EXPECT_TRUE((true == logicalCollection->getIndexes().empty()));
      EXPECT_TRUE((true == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));
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
    ASSERT_TRUE((nullptr != logicalCollection0));
    auto logicalCollection1 = vocbase.createCollection(collection1Json->slice());
    ASSERT_TRUE((nullptr != logicalCollection1));
    auto logicalView = vocbase.createView(viewCreateJson->slice());
    ASSERT_TRUE((false == !logicalView));

    EXPECT_TRUE((true == logicalCollection0->getIndexes().empty()));
    EXPECT_TRUE((true == logicalCollection1->getIndexes().empty()));
    EXPECT_TRUE((true == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));

    // initial link creation
    {
      auto updateJson = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection0\": {} } }");
      EXPECT_TRUE((logicalView->properties(updateJson->slice(), true).ok()));
      EXPECT_TRUE((false == logicalCollection0->getIndexes().empty()));
      EXPECT_TRUE((true == logicalCollection1->getIndexes().empty()));
      EXPECT_TRUE((false == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));
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

      EXPECT_TRUE((TRI_ERROR_FORBIDDEN == logicalView->properties(viewUpdateJson->slice(), false).errorNumber()));
      EXPECT_TRUE((false == logicalCollection0->getIndexes().empty()));
      EXPECT_TRUE((true == logicalCollection1->getIndexes().empty()));
      EXPECT_TRUE((false == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));
    }

    // subsequent update (overwrite) authorised (RO collection)
    {
      arangodb::auth::UserMap userMap;
      auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
      user.grantCollection(vocbase.name(), "testCollection0", arangodb::auth::Level::RO); // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      user.grantCollection(vocbase.name(), "testCollection1", arangodb::auth::Level::RO); // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

      EXPECT_TRUE((logicalView->properties(viewUpdateJson->slice(), false).ok()));
      EXPECT_TRUE((false == logicalCollection0->getIndexes().empty()));
      EXPECT_TRUE((false == logicalCollection1->getIndexes().empty()));
      EXPECT_TRUE((false == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));
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
    ASSERT_TRUE((nullptr != logicalCollection0));
    auto logicalCollection1 = vocbase.createCollection(collection1Json->slice());
    ASSERT_TRUE((nullptr != logicalCollection1));
    auto logicalView = vocbase.createView(viewCreateJson->slice());
    ASSERT_TRUE((false == !logicalView));

    EXPECT_TRUE((true == logicalCollection0->getIndexes().empty()));
    EXPECT_TRUE((true == logicalCollection1->getIndexes().empty()));
    EXPECT_TRUE((true == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));

    // initial link creation
    {
      auto updateJson = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection0\": {}, \"testCollection1\": {} } }");
      EXPECT_TRUE((logicalView->properties(updateJson->slice(), true).ok()));
      EXPECT_TRUE((false == logicalCollection0->getIndexes().empty()));
      EXPECT_TRUE((false == logicalCollection1->getIndexes().empty()));
      EXPECT_TRUE((false == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));
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

      EXPECT_TRUE((TRI_ERROR_FORBIDDEN == logicalView->properties(viewUpdateJson->slice(), false).errorNumber()));
      EXPECT_TRUE((false == logicalCollection0->getIndexes().empty()));
      EXPECT_TRUE((false == logicalCollection1->getIndexes().empty()));
      EXPECT_TRUE((false == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));
    }

    // subsequent update (overwrite) authorised (RO collection)
    {
      arangodb::auth::UserMap userMap;
      auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
      user.grantCollection(vocbase.name(), "testCollection0", arangodb::auth::Level::RO); // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      user.grantCollection(vocbase.name(), "testCollection1", arangodb::auth::Level::RO); // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

      EXPECT_TRUE((logicalView->properties(viewUpdateJson->slice(), false).ok()));
      EXPECT_TRUE((false == logicalCollection0->getIndexes().empty()));
      EXPECT_TRUE((true == logicalCollection1->getIndexes().empty()));
      EXPECT_TRUE((false == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));
    }
  }

  // drop link (collection not authorized)
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\", \"id\": 100 }");
    auto viewCreateJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
    auto viewUpdateJson = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection\": null } }");

    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    ASSERT_TRUE((nullptr != logicalCollection));
    auto logicalView = vocbase.createView(viewCreateJson->slice());
    ASSERT_TRUE((false == !logicalView));

    EXPECT_TRUE((true == logicalCollection->getIndexes().empty()));
    EXPECT_TRUE((true == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));

    // initial link creation
    {
      auto updateJson = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection\": {} } }");
      EXPECT_TRUE((logicalView->properties(updateJson->slice(), true).ok()));
      EXPECT_TRUE((false == logicalCollection->getIndexes().empty()));
      EXPECT_TRUE((false == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));
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

      EXPECT_TRUE((TRI_ERROR_FORBIDDEN == logicalView->properties(viewUpdateJson->slice(), false).errorNumber()));
      EXPECT_TRUE((false == logicalCollection->getIndexes().empty()));
      EXPECT_TRUE((false == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));
    }

    // subsequent update (overwrite) authorised (RO collection)
    {
      arangodb::auth::UserMap userMap;
      auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
      user.grantCollection(vocbase.name(), "testCollection", arangodb::auth::Level::RO); // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

      EXPECT_TRUE((logicalView->properties(viewUpdateJson->slice(), false).ok()));
      EXPECT_TRUE((true == logicalCollection->getIndexes().empty()));
      EXPECT_TRUE((true == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));
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
    ASSERT_TRUE((nullptr != logicalCollection0));
    auto logicalCollection1 = vocbase.createCollection(collection1Json->slice());
    ASSERT_TRUE((nullptr != logicalCollection1));
    auto logicalView = vocbase.createView(viewCreateJson->slice());
    ASSERT_TRUE((false == !logicalView));

    EXPECT_TRUE((true == logicalCollection0->getIndexes().empty()));
    EXPECT_TRUE((true == logicalCollection1->getIndexes().empty()));
    EXPECT_TRUE((true == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));

    // initial link creation
    {
      auto updateJson = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection0\": {} } }");
      EXPECT_TRUE((logicalView->properties(updateJson->slice(), true).ok()));
      EXPECT_TRUE((false == logicalCollection0->getIndexes().empty()));
      EXPECT_TRUE((true == logicalCollection1->getIndexes().empty()));
      EXPECT_TRUE((false == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));
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

      EXPECT_TRUE((TRI_ERROR_FORBIDDEN == logicalView->properties(viewUpdateJson->slice(), false).errorNumber()));
      EXPECT_TRUE((false == logicalCollection0->getIndexes().empty()));
      EXPECT_TRUE((true == logicalCollection1->getIndexes().empty()));
      EXPECT_TRUE((false == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));
    }

    // subsequent update (overwrite) authorised (RO collection)
    {
      arangodb::auth::UserMap userMap;
      auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
      user.grantCollection(vocbase.name(), "testCollection0", arangodb::auth::Level::RO); // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      user.grantCollection(vocbase.name(), "testCollection1", arangodb::auth::Level::RO); // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

      EXPECT_TRUE((logicalView->properties(viewUpdateJson->slice(), false).ok()));
      EXPECT_TRUE((false == logicalCollection0->getIndexes().empty()));
      EXPECT_TRUE((false == logicalCollection1->getIndexes().empty()));
      EXPECT_TRUE((false == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));
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
    ASSERT_TRUE((nullptr != logicalCollection0));
    auto logicalCollection1 = vocbase.createCollection(collection1Json->slice());
    ASSERT_TRUE((nullptr != logicalCollection1));
    auto logicalView = vocbase.createView(viewCreateJson->slice());
    ASSERT_TRUE((false == !logicalView));

    EXPECT_TRUE((true == logicalCollection0->getIndexes().empty()));
    EXPECT_TRUE((true == logicalCollection1->getIndexes().empty()));
    EXPECT_TRUE((true == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));

    // initial link creation
    {
      auto updateJson = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection0\": {}, \"testCollection1\": {} } }");
      EXPECT_TRUE((logicalView->properties(updateJson->slice(), true).ok()));
      EXPECT_TRUE((false == logicalCollection0->getIndexes().empty()));
      EXPECT_TRUE((false == logicalCollection1->getIndexes().empty()));
      EXPECT_TRUE((false == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));
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

      EXPECT_TRUE((TRI_ERROR_FORBIDDEN == logicalView->properties(viewUpdateJson->slice(), false).errorNumber()));
      EXPECT_TRUE((false == logicalCollection0->getIndexes().empty()));
      EXPECT_TRUE((false == logicalCollection1->getIndexes().empty()));
      EXPECT_TRUE((false == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));
    }

    // subsequent update (overwrite) authorised (RO collection)
    {
      arangodb::auth::UserMap userMap;
      auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
      user.grantCollection(vocbase.name(), "testCollection0", arangodb::auth::Level::RO); // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      user.grantCollection(vocbase.name(), "testCollection1", arangodb::auth::Level::RO); // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

      EXPECT_TRUE((logicalView->properties(viewUpdateJson->slice(), false).ok()));
      EXPECT_TRUE((false == logicalCollection0->getIndexes().empty()));
      EXPECT_TRUE((true == logicalCollection1->getIndexes().empty()));
      EXPECT_TRUE((false == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));
    }
  }
}

TEST_F(IResearchViewTest, test_update_partial) {
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
    ASSERT_TRUE((false == !view));
    ASSERT_TRUE(view->category() == arangodb::LogicalView::category());

    arangodb::iresearch::IResearchViewMeta expectedMeta;
    arangodb::iresearch::IResearchViewMetaState expectedMetaState;
    auto updateJson = arangodb::velocypack::Parser::fromJson("{ \
      \"cleanupIntervalStep\": 42 \
    }");

    expectedMeta._cleanupIntervalStep = 42;
    EXPECT_TRUE((view->properties(updateJson->slice(), true).ok()));

    // not for persistence
    {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      view->properties(builder, arangodb::LogicalDataSource::makeFlags(
                                    arangodb::LogicalDataSource::Serialize::Detailed));
      builder.close();

      auto slice = builder.slice();
      arangodb::iresearch::IResearchViewMeta meta;
      arangodb::iresearch::IResearchViewMetaState metaState;
      std::string error;

      EXPECT_TRUE(slice.isObject());
      EXPECT_TRUE((13U == slice.length()));
      EXPECT_TRUE((slice.hasKey("globallyUniqueId") && slice.get("globallyUniqueId").isString() && false == slice.get("globallyUniqueId").copyString().empty()));
      EXPECT_TRUE(slice.get("name").copyString() == "testView");
      EXPECT_TRUE(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
      EXPECT_TRUE(slice.get("deleted").isNone()); // no system properties
      EXPECT_TRUE((meta.init(slice, error) && expectedMeta == meta));
      auto tmpSlice = slice.get("links");
      EXPECT_TRUE((true == tmpSlice.isObject() && 0 == tmpSlice.length()));
    }

    // for persistence
    {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      view->properties(builder, arangodb::LogicalDataSource::makeFlags(
                                    arangodb::LogicalDataSource::Serialize::Detailed,
                                    arangodb::LogicalDataSource::Serialize::ForPersistence));
      builder.close();

      auto slice = builder.slice();
      arangodb::iresearch::IResearchViewMeta meta;
      arangodb::iresearch::IResearchViewMetaState metaState;
      std::string error;

      EXPECT_TRUE(slice.isObject());
      EXPECT_TRUE((17U == slice.length()));
      EXPECT_TRUE(slice.get("name").copyString() == "testView");
      EXPECT_TRUE(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
      EXPECT_TRUE((slice.hasKey("deleted") && slice.get("deleted").isBoolean() && false == slice.get("deleted").getBoolean())); // has system properties
      EXPECT_TRUE((meta.init(slice, error) && expectedMeta == meta));
      EXPECT_TRUE((true == metaState.init(slice, error) && expectedMetaState == metaState));
      EXPECT_TRUE((false == slice.hasKey("links")));
    }
  }

  // test rollback on meta modification failure (as an example invalid value for 'cleanupIntervalStep')
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalView = vocbase.createView(createJson->slice());
    ASSERT_TRUE((false == !logicalView));
    ASSERT_TRUE((logicalView->category() == arangodb::LogicalView::category()));

    arangodb::iresearch::IResearchViewMeta expectedMeta;
    arangodb::iresearch::IResearchViewMetaState expectedMetaState;
    auto updateJson = arangodb::velocypack::Parser::fromJson("{ \"cleanupIntervalStep\": 0.123 }");

    expectedMeta._cleanupIntervalStep = 52;
    EXPECT_TRUE((TRI_ERROR_BAD_PARAMETER == logicalView->properties(updateJson->slice(), true).errorNumber()));

    // not for persistence
    {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      logicalView->properties(builder, arangodb::LogicalDataSource::makeFlags(
                                           arangodb::LogicalDataSource::Serialize::Detailed));
      builder.close();

      auto slice = builder.slice();
      arangodb::iresearch::IResearchViewMeta meta;
      arangodb::iresearch::IResearchViewMetaState metaState;
      std::string error;

      EXPECT_TRUE((slice.isObject()));
      EXPECT_TRUE((13U == slice.length()));
      EXPECT_TRUE((slice.hasKey("globallyUniqueId") && slice.get("globallyUniqueId").isString() && false == slice.get("globallyUniqueId").copyString().empty()));
      EXPECT_TRUE((slice.get("name").copyString() == "testView"));
      EXPECT_TRUE((slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name()));
      EXPECT_TRUE((slice.get("deleted").isNone())); // no system properties
      EXPECT_TRUE((meta.init(slice, error) && expectedMeta == meta));
      auto tmpSlice = slice.get("links");
      EXPECT_TRUE((true == tmpSlice.isObject() && 0 == tmpSlice.length()));
    }

    // for persistence
    {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      logicalView->properties(builder,
                              arangodb::LogicalDataSource::makeFlags(
                                  arangodb::LogicalDataSource::Serialize::Detailed,
                                  arangodb::LogicalDataSource::Serialize::ForPersistence));
      builder.close();

      auto slice = builder.slice();
      arangodb::iresearch::IResearchViewMeta meta;
      arangodb::iresearch::IResearchViewMetaState metaState;
      std::string error;

      EXPECT_TRUE((slice.isObject()));
      EXPECT_TRUE((17U == slice.length()));
      EXPECT_TRUE((slice.get("name").copyString() == "testView"));
      EXPECT_TRUE((slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name()));
      EXPECT_TRUE((slice.hasKey("deleted") && slice.get("deleted").isBoolean() && false == slice.get("deleted").getBoolean())); // has system properties
      EXPECT_TRUE((meta.init(slice, error) && expectedMeta == meta));
      EXPECT_TRUE((true == metaState.init(slice, error) && expectedMetaState == metaState));
      EXPECT_TRUE((false == slice.hasKey("links")));
    }
  }

  // modify meta params with links to missing collections
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalView = vocbase.createView(createJson->slice());
    ASSERT_TRUE((false == !logicalView));
    ASSERT_TRUE((logicalView->category() == arangodb::LogicalView::category()));

    arangodb::iresearch::IResearchViewMeta expectedMeta;
    arangodb::iresearch::IResearchViewMetaState expectedMetaState;
    auto updateJson = arangodb::velocypack::Parser::fromJson("{ \"cleanupIntervalStep\": 62, \"links\": { \"testCollection\": {} } }");

    expectedMeta._cleanupIntervalStep = 52;
    EXPECT_TRUE((TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND == logicalView->properties(updateJson->slice(), true).errorNumber()));
    EXPECT_TRUE((true == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));

    // not for persistence
    {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      logicalView->properties(builder, arangodb::LogicalDataSource::makeFlags(
                                           arangodb::LogicalDataSource::Serialize::Detailed));
      builder.close();

      auto slice = builder.slice();
      arangodb::iresearch::IResearchViewMeta meta;
      arangodb::iresearch::IResearchViewMetaState metaState;
      std::string error;

      EXPECT_TRUE((slice.isObject()));
      EXPECT_TRUE((13U == slice.length()));
      EXPECT_TRUE((slice.hasKey("globallyUniqueId") && slice.get("globallyUniqueId").isString() && false == slice.get("globallyUniqueId").copyString().empty()));
      EXPECT_TRUE((slice.get("name").copyString() == "testView"));
      EXPECT_TRUE((slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name()));
      EXPECT_TRUE((slice.get("deleted").isNone())); // no system properties
      EXPECT_TRUE((meta.init(slice, error) && expectedMeta == meta));
      auto tmpSlice = slice.get("links");
      EXPECT_TRUE((true == tmpSlice.isObject() && 0 == tmpSlice.length()));
    }

    // for persistence
    {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      logicalView->properties(builder,
                              arangodb::LogicalDataSource::makeFlags(
                                  arangodb::LogicalDataSource::Serialize::Detailed,
                                  arangodb::LogicalDataSource::Serialize::ForPersistence));
      builder.close();

      auto slice = builder.slice();
      arangodb::iresearch::IResearchViewMeta meta;
      arangodb::iresearch::IResearchViewMetaState metaState;
      std::string error;

      EXPECT_TRUE((slice.isObject()));
      EXPECT_TRUE((17U == slice.length()));
      EXPECT_TRUE((slice.get("name").copyString() == "testView"));
      EXPECT_TRUE((slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name()));
      EXPECT_TRUE((slice.hasKey("deleted") && slice.get("deleted").isBoolean() && false == slice.get("deleted").getBoolean())); // has system properties
      EXPECT_TRUE((meta.init(slice, error) && expectedMeta == meta));
      EXPECT_TRUE((true == metaState.init(slice, error) && expectedMetaState == metaState));
      EXPECT_TRUE((false == slice.hasKey("links")));
    }
  }

  // modify meta params with links with invalid definition
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    ASSERT_TRUE((nullptr != logicalCollection));
    auto logicalView = vocbase.createView(createJson->slice());
    ASSERT_TRUE((false == !logicalView));
    ASSERT_TRUE((logicalView->category() == arangodb::LogicalView::category()));
    EXPECT_TRUE((true == logicalCollection->getIndexes().empty()));

    arangodb::iresearch::IResearchViewMeta expectedMeta;
    arangodb::iresearch::IResearchViewMetaState expectedMetaState;
    auto updateJson = arangodb::velocypack::Parser::fromJson("{ \"cleanupIntervalStep\": 62, \"links\": { \"testCollection\": 42 } }");

    expectedMeta._cleanupIntervalStep = 52;
    EXPECT_TRUE((TRI_ERROR_BAD_PARAMETER == logicalView->properties(updateJson->slice(), true).errorNumber()));
    EXPECT_TRUE((true == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));

    // not for persistence
    {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      logicalView->properties(builder, arangodb::LogicalDataSource::makeFlags(
                                           arangodb::LogicalDataSource::Serialize::Detailed));
      builder.close();

      auto slice = builder.slice();
      arangodb::iresearch::IResearchViewMeta meta;
      arangodb::iresearch::IResearchViewMetaState metaState;
      std::string error;

      EXPECT_TRUE((slice.isObject()));
      EXPECT_TRUE((13U == slice.length()));
      EXPECT_TRUE((slice.hasKey("globallyUniqueId") && slice.get("globallyUniqueId").isString() && false == slice.get("globallyUniqueId").copyString().empty()));
      EXPECT_TRUE((slice.get("name").copyString() == "testView"));
      EXPECT_TRUE((slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name()));
      EXPECT_TRUE((slice.get("deleted").isNone())); // no system properties
      EXPECT_TRUE((meta.init(slice, error) && expectedMeta == meta));
      auto tmpSlice = slice.get("links");
      EXPECT_TRUE((true == tmpSlice.isObject() && 0 == tmpSlice.length()));
    }

    // for persistence
    {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      logicalView->properties(builder,
                              arangodb::LogicalDataSource::makeFlags(
                                  arangodb::LogicalDataSource::Serialize::Detailed,
                                  arangodb::LogicalDataSource::Serialize::ForPersistence));
      builder.close();

      auto slice = builder.slice();
      arangodb::iresearch::IResearchViewMeta meta;
      arangodb::iresearch::IResearchViewMetaState metaState;
      std::string error;

      EXPECT_TRUE((slice.isObject()));
      EXPECT_TRUE((17U == slice.length()));
      EXPECT_TRUE((slice.get("name").copyString() == "testView"));
      EXPECT_TRUE((slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name()));
      EXPECT_TRUE((slice.hasKey("deleted") && slice.get("deleted").isBoolean() && false == slice.get("deleted").getBoolean())); // has system properties
      EXPECT_TRUE((meta.init(slice, error) && expectedMeta == meta));
      EXPECT_TRUE((true == metaState.init(slice, error) && expectedMetaState == metaState));
      EXPECT_TRUE((false == slice.hasKey("links")));
    }
  }

  // modify meta params with links
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    ASSERT_TRUE((nullptr != logicalCollection));
    auto logicalView = vocbase.createView(createJson->slice());
    ASSERT_TRUE((false == !logicalView));
    ASSERT_TRUE((logicalView->category() == arangodb::LogicalView::category()));
    EXPECT_TRUE((true == logicalCollection->getIndexes().empty()));

    // initial link creation
    {
      auto updateJson = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection\": {} } }");
      arangodb::iresearch::IResearchViewMeta expectedMeta;
      arangodb::iresearch::IResearchViewMetaState expectedMetaState;
      std::unordered_map<std::string, arangodb::iresearch::IResearchLinkMeta> expectedLinkMeta;

      expectedMeta._cleanupIntervalStep = 52;
      expectedMetaState._collections.insert(logicalCollection->id());
      expectedLinkMeta["testCollection"]; // use defaults
      EXPECT_TRUE((logicalView->properties(updateJson->slice(), true).ok()));

      // not for persistence
      {
        arangodb::velocypack::Builder builder;

        builder.openObject();
        logicalView->properties(builder, arangodb::LogicalDataSource::makeFlags(
                                             arangodb::LogicalDataSource::Serialize::Detailed));
        builder.close();

        auto slice = builder.slice();
        arangodb::iresearch::IResearchViewMeta meta;
        arangodb::iresearch::IResearchViewMetaState metaState;
        std::string error;

        EXPECT_TRUE((slice.isObject()));
        EXPECT_TRUE((13U == slice.length()));
        EXPECT_TRUE((slice.hasKey("globallyUniqueId") && slice.get("globallyUniqueId").isString() && false == slice.get("globallyUniqueId").copyString().empty()));
        EXPECT_TRUE((slice.get("name").copyString() == "testView"));
        EXPECT_TRUE((slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name()));
        EXPECT_TRUE((slice.get("deleted").isNone())); // no system properties
        EXPECT_TRUE((meta.init(slice, error) && expectedMeta == meta));

        auto tmpSlice = slice.get("links");
        EXPECT_TRUE((true == tmpSlice.isObject() && 1 == tmpSlice.length()));

        for (arangodb::velocypack::ObjectIterator itr(tmpSlice); itr.valid(); ++itr) {
          arangodb::iresearch::IResearchLinkMeta linkMeta;
          auto key = itr.key();
          auto value = itr.value();
          EXPECT_TRUE((true == key.isString()));

          auto expectedItr = expectedLinkMeta.find(key.copyString());
          EXPECT_TRUE((
            true == value.isObject()
            && expectedItr != expectedLinkMeta.end()
            && linkMeta.init(value, false, error)
            && expectedItr->second == linkMeta
          ));
          expectedLinkMeta.erase(expectedItr);
        }
      }

      // for persistence
      {
        arangodb::velocypack::Builder builder;

        builder.openObject();
        logicalView->properties(builder,
                                arangodb::LogicalDataSource::makeFlags(
                                    arangodb::LogicalDataSource::Serialize::Detailed,
                                    arangodb::LogicalDataSource::Serialize::ForPersistence));
        builder.close();

        auto slice = builder.slice();
        arangodb::iresearch::IResearchViewMeta meta;
        arangodb::iresearch::IResearchViewMetaState metaState;
        std::string error;

        EXPECT_TRUE((slice.isObject()));
        EXPECT_TRUE((17U == slice.length()));
        EXPECT_TRUE((slice.get("name").copyString() == "testView"));
        EXPECT_TRUE((slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name()));
        EXPECT_TRUE((slice.hasKey("deleted") && slice.get("deleted").isBoolean() && false == slice.get("deleted").getBoolean())); // has system properties
        EXPECT_TRUE((meta.init(slice, error) && expectedMeta == meta));
        EXPECT_TRUE((true == metaState.init(slice, error) && expectedMetaState == metaState));
        EXPECT_TRUE((false == slice.hasKey("links")));
      }

      EXPECT_TRUE((true == expectedLinkMeta.empty()));
      EXPECT_TRUE((false == logicalCollection->getIndexes().empty()));
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
      EXPECT_TRUE((logicalView->properties(updateJson->slice(), true).ok()));

      // not for persistence
      {
        arangodb::velocypack::Builder builder;

        builder.openObject();
        logicalView->properties(builder, arangodb::LogicalDataSource::makeFlags(
                                             arangodb::LogicalDataSource::Serialize::Detailed));
        builder.close();

        auto slice = builder.slice();
        arangodb::iresearch::IResearchViewMeta meta;
        arangodb::iresearch::IResearchViewMetaState metaState;
        std::string error;

        EXPECT_TRUE((slice.isObject()));
        EXPECT_TRUE((13U == slice.length()));
        EXPECT_TRUE((slice.hasKey("globallyUniqueId") && slice.get("globallyUniqueId").isString() && false == slice.get("globallyUniqueId").copyString().empty()));
        EXPECT_TRUE((slice.get("name").copyString() == "testView"));
        EXPECT_TRUE((slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name()));
        EXPECT_TRUE((slice.get("deleted").isNone())); // no system properties
        EXPECT_TRUE((meta.init(slice, error) && expectedMeta == meta));

        auto tmpSlice = slice.get("links");
        EXPECT_TRUE((true == tmpSlice.isObject() && 1 == tmpSlice.length()));

        for (arangodb::velocypack::ObjectIterator itr(tmpSlice); itr.valid(); ++itr) {
          arangodb::iresearch::IResearchLinkMeta linkMeta;
          auto key = itr.key();
          auto value = itr.value();
          EXPECT_TRUE((true == key.isString()));

          auto expectedItr = expectedLinkMeta.find(key.copyString());
          EXPECT_TRUE((
            true == value.isObject()
            && expectedItr != expectedLinkMeta.end()
            && linkMeta.init(value, false, error)
            && expectedItr->second == linkMeta
          ));
          expectedLinkMeta.erase(expectedItr);
        }
      }

      // for persistence
      {
        arangodb::velocypack::Builder builder;

        builder.openObject();
        logicalView->properties(builder,
                                arangodb::LogicalDataSource::makeFlags(
                                    arangodb::LogicalDataSource::Serialize::Detailed,
                                    arangodb::LogicalDataSource::Serialize::ForPersistence));
        builder.close();

        auto slice = builder.slice();
        arangodb::iresearch::IResearchViewMeta meta;
        arangodb::iresearch::IResearchViewMetaState metaState;
        std::string error;

        EXPECT_TRUE((slice.isObject()));
        EXPECT_TRUE((17U == slice.length()));
        EXPECT_TRUE((slice.get("name").copyString() == "testView"));
        EXPECT_TRUE((slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name()));
        EXPECT_TRUE((slice.hasKey("deleted") && slice.get("deleted").isBoolean() && false == slice.get("deleted").getBoolean())); // has system properties
        EXPECT_TRUE((meta.init(slice, error) && expectedMeta == meta));
        EXPECT_TRUE((true == metaState.init(slice, error) && expectedMetaState == metaState));
        EXPECT_TRUE((false == slice.hasKey("links")));
      }

      EXPECT_TRUE((true == expectedLinkMeta.empty()));
      EXPECT_TRUE((false == logicalCollection->getIndexes().empty()));
    }
  }

  // add a new link (in recovery)
  {
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    ASSERT_TRUE((nullptr != logicalCollection));
    auto view = vocbase.createView(createJson->slice());
    ASSERT_TRUE((false == !view));
    ASSERT_TRUE(view->category() == arangodb::LogicalView::category());

    auto updateJson = arangodb::velocypack::Parser::fromJson(
      "{ \"links\": { \"testCollection\": {} } }"
    );

    auto beforeRec = StorageEngineMock::inRecoveryResult;
    StorageEngineMock::inRecoveryResult = true;
    auto restore = irs::make_finally([&beforeRec]()->void { StorageEngineMock::inRecoveryResult = beforeRec; });
    persisted = false;
    EXPECT_TRUE((view->properties(updateJson->slice(), true).ok()));
    EXPECT_TRUE((false == persisted));

    // not for persistence
    {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      view->properties(builder, arangodb::LogicalDataSource::makeFlags(
                                    arangodb::LogicalDataSource::Serialize::Detailed));
      builder.close();

      auto slice = builder.slice();
      EXPECT_TRUE(slice.isObject());
      EXPECT_TRUE((13U == slice.length()));
      EXPECT_TRUE((slice.hasKey("globallyUniqueId") && slice.get("globallyUniqueId").isString() && false == slice.get("globallyUniqueId").copyString().empty()));
      EXPECT_TRUE(slice.get("name").copyString() == "testView");
      EXPECT_TRUE(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
      EXPECT_TRUE(slice.get("deleted").isNone()); // no system properties
      EXPECT_TRUE(slice.isObject());
      EXPECT_TRUE((
        true == slice.hasKey("links")
        && slice.get("links").isObject()
        && 1 == slice.get("links").length()
      ));
    }

    // for persistence
    {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      view->properties(builder, arangodb::LogicalDataSource::makeFlags(
                                    arangodb::LogicalDataSource::Serialize::Detailed,
                                    arangodb::LogicalDataSource::Serialize::ForPersistence));
      builder.close();

      auto slice = builder.slice();
      EXPECT_TRUE(slice.isObject());
      EXPECT_TRUE((17U == slice.length()));
      EXPECT_TRUE(slice.get("name").copyString() == "testView");
      EXPECT_TRUE(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
      EXPECT_TRUE((slice.hasKey("deleted") && slice.get("deleted").isBoolean() && false == slice.get("deleted").getBoolean())); // has system properties
      auto tmpSlice = slice.get("collections");
      EXPECT_TRUE((true == tmpSlice.isArray() && 1 == tmpSlice.length()));
      EXPECT_TRUE((false == slice.hasKey("links")));
    }
  }

  // add a new link
  {
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    ASSERT_TRUE((nullptr != logicalCollection));
    auto view = vocbase.createView(createJson->slice());
    ASSERT_TRUE((false == !view));

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
    EXPECT_TRUE((view->properties(updateJson->slice(), true).ok()));
    EXPECT_TRUE((true == persisted)); // link addition does modify and persist view meta

    // not for persistence
    {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      view->properties(builder, arangodb::LogicalDataSource::makeFlags(
                                    arangodb::LogicalDataSource::Serialize::Detailed));
      builder.close();

      auto slice = builder.slice();
      arangodb::iresearch::IResearchViewMeta meta;
      arangodb::iresearch::IResearchViewMetaState metaState;
      std::string error;

      EXPECT_TRUE(slice.isObject());
      EXPECT_TRUE((13U == slice.length()));
      EXPECT_TRUE((slice.hasKey("globallyUniqueId") && slice.get("globallyUniqueId").isString() && false == slice.get("globallyUniqueId").copyString().empty()));
      EXPECT_TRUE(slice.get("name").copyString() == "testView");
      EXPECT_TRUE(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
      EXPECT_TRUE(slice.get("deleted").isNone()); // no system properties
      EXPECT_TRUE((meta.init(slice, error) && expectedMeta == meta));

      auto tmpSlice = slice.get("links");
      EXPECT_TRUE((true == tmpSlice.isObject() && 1 == tmpSlice.length()));

      for (arangodb::velocypack::ObjectIterator itr(tmpSlice); itr.valid(); ++itr) {
        arangodb::iresearch::IResearchLinkMeta linkMeta;
        auto key = itr.key();
        auto value = itr.value();
        EXPECT_TRUE((true == key.isString()));

        auto expectedItr = expectedLinkMeta.find(key.copyString());
        EXPECT_TRUE((
          true == value.isObject()
          && expectedItr != expectedLinkMeta.end()
          && linkMeta.init(value, false, error)
          && expectedItr->second == linkMeta
        ));
        expectedLinkMeta.erase(expectedItr);
      }
    }

    // for persistence
    {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      view->properties(builder, arangodb::LogicalDataSource::makeFlags(
                                    arangodb::LogicalDataSource::Serialize::Detailed,
                                    arangodb::LogicalDataSource::Serialize::ForPersistence));
      builder.close();

      auto slice = builder.slice();
      arangodb::iresearch::IResearchViewMeta meta;
      arangodb::iresearch::IResearchViewMetaState metaState;
      std::string error;

      EXPECT_TRUE(slice.isObject());
      EXPECT_TRUE((17U == slice.length()));
      EXPECT_TRUE(slice.get("name").copyString() == "testView");
      EXPECT_TRUE(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
      EXPECT_TRUE((slice.hasKey("deleted") && slice.get("deleted").isBoolean() && false == slice.get("deleted").getBoolean())); // has system properties
      EXPECT_TRUE((meta.init(slice, error) && expectedMeta == meta));
      EXPECT_TRUE((true == metaState.init(slice, error) && expectedMetaState == metaState));
      EXPECT_TRUE((false == slice.hasKey("links")));
    }

    EXPECT_TRUE((true == expectedLinkMeta.empty()));
  }

  // add a new link to a collection with documents
  {
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    ASSERT_TRUE((nullptr != logicalCollection));
    auto view = vocbase.createView(createJson->slice());
    ASSERT_TRUE((false == !view));
    ASSERT_TRUE(view->category() == arangodb::LogicalView::category());

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

      EXPECT_TRUE((trx.begin().ok()));
      EXPECT_TRUE((trx.insert(logicalCollection->name(), doc->slice(), arangodb::OperationOptions()).ok()));
      EXPECT_TRUE((trx.commit().ok()));
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
    EXPECT_TRUE((view->properties(updateJson->slice(), true).ok()));
    EXPECT_TRUE((true == persisted)); // link addition does modify and persist view meta

    // not for persistence
    {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      view->properties(builder, arangodb::LogicalDataSource::makeFlags(
                                    arangodb::LogicalDataSource::Serialize::Detailed));
      builder.close();

      auto slice = builder.slice();
      arangodb::iresearch::IResearchViewMeta meta;
      arangodb::iresearch::IResearchViewMetaState metaState;
      std::string error;

      EXPECT_TRUE(slice.isObject());
      EXPECT_TRUE((13U == slice.length()));
      EXPECT_TRUE((slice.hasKey("globallyUniqueId") && slice.get("globallyUniqueId").isString() && false == slice.get("globallyUniqueId").copyString().empty()));
      EXPECT_TRUE(slice.get("name").copyString() == "testView");
      EXPECT_TRUE(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
      EXPECT_TRUE(slice.get("deleted").isNone()); // no system properties
      EXPECT_TRUE((meta.init(slice, error) && expectedMeta == meta));

      auto tmpSlice = slice.get("links");
      EXPECT_TRUE((true == tmpSlice.isObject() && 1 == tmpSlice.length()));

      for (arangodb::velocypack::ObjectIterator itr(tmpSlice); itr.valid(); ++itr) {
        arangodb::iresearch::IResearchLinkMeta linkMeta;
        auto key = itr.key();
        auto value = itr.value();
        EXPECT_TRUE((true == key.isString()));

        auto expectedItr = expectedLinkMeta.find(key.copyString());
        EXPECT_TRUE((
          true == value.isObject()
          && expectedItr != expectedLinkMeta.end()
          && linkMeta.init(value, false, error)
          && expectedItr->second == linkMeta
        ));
        expectedLinkMeta.erase(expectedItr);
      }
    }

    // for persistence
    {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      view->properties(builder, arangodb::LogicalDataSource::makeFlags(
                                    arangodb::LogicalDataSource::Serialize::Detailed,
                                    arangodb::LogicalDataSource::Serialize::ForPersistence));
      builder.close();

      auto slice = builder.slice();
      arangodb::iresearch::IResearchViewMeta meta;
      arangodb::iresearch::IResearchViewMetaState metaState;
      std::string error;

      EXPECT_TRUE(slice.isObject());
      EXPECT_TRUE((17U == slice.length()));
      EXPECT_TRUE(slice.get("name").copyString() == "testView");
      EXPECT_TRUE(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
      EXPECT_TRUE((slice.hasKey("deleted") && slice.get("deleted").isBoolean() && false == slice.get("deleted").getBoolean())); // has system properties
      EXPECT_TRUE((meta.init(slice, error) && expectedMeta == meta));
      EXPECT_TRUE((true == metaState.init(slice, error) && expectedMetaState == metaState));
      EXPECT_TRUE((false == slice.hasKey("links")));
    }

    EXPECT_TRUE((true == expectedLinkMeta.empty()));
  }

  // add new link to non-existant collection
  {
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto view = vocbase.createView(createJson->slice());
    ASSERT_TRUE((false == !view));
    ASSERT_TRUE(view->category() == arangodb::LogicalView::category());

    arangodb::iresearch::IResearchViewMeta expectedMeta;
    arangodb::iresearch::IResearchViewMetaState expectedMetaState;
    auto updateJson = arangodb::velocypack::Parser::fromJson("{ \
      \"links\": { \
        \"testCollection\": {} \
      }}");

    expectedMeta._cleanupIntervalStep = 52;
    EXPECT_TRUE((TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND == view->properties(updateJson->slice(), true).errorNumber()));

    // not for persistence
    {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      view->properties(builder, arangodb::LogicalDataSource::makeFlags(
                                    arangodb::LogicalDataSource::Serialize::Detailed));
      builder.close();

      auto slice = builder.slice();
      arangodb::iresearch::IResearchViewMeta meta;
      arangodb::iresearch::IResearchViewMetaState metaState;
      std::string error;

      EXPECT_TRUE(slice.isObject());
      EXPECT_TRUE((13U == slice.length()));
      EXPECT_TRUE((slice.hasKey("globallyUniqueId") && slice.get("globallyUniqueId").isString() && false == slice.get("globallyUniqueId").copyString().empty()));
      EXPECT_TRUE(slice.get("name").copyString() == "testView");
      EXPECT_TRUE(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
      EXPECT_TRUE(slice.get("deleted").isNone()); // no system properties
      EXPECT_TRUE((meta.init(slice, error) && expectedMeta == meta));
      auto tmpSlice = slice.get("links");
      EXPECT_TRUE((true == tmpSlice.isObject() && 0 == tmpSlice.length()));
    }

    // for persistence
    {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      view->properties(builder, arangodb::LogicalDataSource::makeFlags(
                                    arangodb::LogicalDataSource::Serialize::Detailed,
                                    arangodb::LogicalDataSource::Serialize::ForPersistence));
      builder.close();

      auto slice = builder.slice();
      arangodb::iresearch::IResearchViewMeta meta;
      arangodb::iresearch::IResearchViewMetaState metaState;
      std::string error;

      EXPECT_TRUE(slice.isObject());
      EXPECT_TRUE((17U == slice.length()));
      EXPECT_TRUE(slice.get("name").copyString() == "testView");
      EXPECT_TRUE(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
      EXPECT_TRUE((slice.hasKey("deleted") && slice.get("deleted").isBoolean() && false == slice.get("deleted").getBoolean())); // has system properties
      EXPECT_TRUE((meta.init(slice, error) && expectedMeta == meta));
      EXPECT_TRUE((metaState.init(slice, error) && expectedMetaState == metaState));
      EXPECT_TRUE((false == slice.hasKey("links")));
    }
  }

  // remove link (in recovery)
  {
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    ASSERT_TRUE((nullptr != logicalCollection));
    auto view = vocbase.createView(createJson->slice());
    ASSERT_TRUE((false == !view));
    ASSERT_TRUE(view->category() == arangodb::LogicalView::category());

    {
      auto updateJson = arangodb::velocypack::Parser::fromJson(
        "{ \"links\": { \"testCollection\": {} } }"
      );
      persisted = false;
      auto beforeRecovery = StorageEngineMock::inRecoveryResult;
      StorageEngineMock::inRecoveryResult = true;
      auto restoreRecovery = irs::make_finally([&beforeRecovery]()->void { StorageEngineMock::inRecoveryResult = beforeRecovery; });
      EXPECT_TRUE((view->properties(updateJson->slice(), true).ok()));
      EXPECT_TRUE((false == persisted)); // link addition does not persist view meta

      arangodb::velocypack::Builder builder;

      builder.openObject();
      view->properties(builder, arangodb::LogicalDataSource::makeFlags(
                                    arangodb::LogicalDataSource::Serialize::Detailed));
      builder.close();

      auto slice = builder.slice();
      EXPECT_TRUE(slice.isObject());
      EXPECT_TRUE((13U == slice.length()));
      EXPECT_TRUE((slice.hasKey("globallyUniqueId") && slice.get("globallyUniqueId").isString() && false == slice.get("globallyUniqueId").copyString().empty()));
      EXPECT_TRUE(slice.get("name").copyString() == "testView");
      EXPECT_TRUE(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
      EXPECT_TRUE(slice.get("deleted").isNone()); // no system properties
      EXPECT_TRUE((
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
      EXPECT_TRUE((view->properties(updateJson->slice(), true).ok()));
      EXPECT_TRUE((false == persisted));

      arangodb::velocypack::Builder builder;

      builder.openObject();
      view->properties(builder, arangodb::LogicalDataSource::makeFlags(
                                    arangodb::LogicalDataSource::Serialize::Detailed));
      builder.close();

      auto slice = builder.slice();
      EXPECT_TRUE(slice.isObject());
      EXPECT_TRUE((13U == slice.length()));
      EXPECT_TRUE((slice.hasKey("globallyUniqueId") && slice.get("globallyUniqueId").isString() && false == slice.get("globallyUniqueId").copyString().empty()));
      EXPECT_TRUE(slice.get("name").copyString() == "testView");
      EXPECT_TRUE(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
      EXPECT_TRUE(slice.get("deleted").isNone()); // no system properties
      EXPECT_TRUE((
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
    ASSERT_TRUE((nullptr != logicalCollection));
    auto view = vocbase.createView(createJson->slice());
    ASSERT_TRUE((false == !view));

    arangodb::iresearch::IResearchViewMeta expectedMeta;
    arangodb::iresearch::IResearchViewMetaState expectedMetaState;

    expectedMeta._cleanupIntervalStep = 52;
    expectedMetaState._collections.insert(logicalCollection->id());

    {
      auto updateJson = arangodb::velocypack::Parser::fromJson("{ \
        \"links\": { \
          \"testCollection\": {} \
      }}");

      EXPECT_TRUE((view->properties(updateJson->slice(), true).ok()));

      // not for persistence
      {
        arangodb::velocypack::Builder builder;

        builder.openObject();
        view->properties(builder, arangodb::LogicalDataSource::makeFlags(
                                      arangodb::LogicalDataSource::Serialize::Detailed));
        builder.close();

        auto slice = builder.slice();
        arangodb::iresearch::IResearchViewMeta meta;
        arangodb::iresearch::IResearchViewMetaState metaState;
        std::string error;

        EXPECT_TRUE(slice.isObject());
        EXPECT_TRUE((13U == slice.length()));
        EXPECT_TRUE((slice.hasKey("globallyUniqueId") && slice.get("globallyUniqueId").isString() && false == slice.get("globallyUniqueId").copyString().empty()));
        EXPECT_TRUE(slice.get("name").copyString() == "testView");
        EXPECT_TRUE(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
        EXPECT_TRUE(slice.get("deleted").isNone()); // no system properties
        EXPECT_TRUE((meta.init(slice, error) && expectedMeta == meta));
        auto tmpSlice = slice.get("links");
        EXPECT_TRUE((true == tmpSlice.isObject() && 1 == tmpSlice.length()));
      }

      // for persistence
      {
        arangodb::velocypack::Builder builder;

        builder.openObject();
        view->properties(builder, arangodb::LogicalDataSource::makeFlags(
                                      arangodb::LogicalDataSource::Serialize::Detailed,
                                      arangodb::LogicalDataSource::Serialize::ForPersistence));
        builder.close();

        auto slice = builder.slice();
        arangodb::iresearch::IResearchViewMeta meta;
        arangodb::iresearch::IResearchViewMetaState metaState;
        std::string error;

        EXPECT_TRUE(slice.isObject());
        EXPECT_TRUE((17U == slice.length()));
        EXPECT_TRUE(slice.get("name").copyString() == "testView");
        EXPECT_TRUE(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
        EXPECT_TRUE((slice.hasKey("deleted") && slice.get("deleted").isBoolean() && false == slice.get("deleted").getBoolean())); // has system properties
        EXPECT_TRUE((meta.init(slice, error) && expectedMeta == meta));
        EXPECT_TRUE((true == metaState.init(slice, error) && expectedMetaState == metaState));
        EXPECT_TRUE((false == slice.hasKey("links")));
      }
    }

    {
      auto updateJson = arangodb::velocypack::Parser::fromJson("{ \
        \"links\": { \
          \"testCollection\": null \
      }}");

      expectedMeta._cleanupIntervalStep = 52;
      expectedMetaState._collections.clear();
      EXPECT_TRUE((view->properties(updateJson->slice(), true).ok()));

      // not for persistence
      {
        arangodb::velocypack::Builder builder;

        builder.openObject();
        view->properties(builder, arangodb::LogicalDataSource::makeFlags(
                                      arangodb::LogicalDataSource::Serialize::Detailed));
        builder.close();

        auto slice = builder.slice();
        arangodb::iresearch::IResearchViewMeta meta;
        arangodb::iresearch::IResearchViewMetaState metaState;
        std::string error;

        EXPECT_TRUE(slice.isObject());
        EXPECT_TRUE((13U == slice.length()));
        EXPECT_TRUE((slice.hasKey("globallyUniqueId") && slice.get("globallyUniqueId").isString() && false == slice.get("globallyUniqueId").copyString().empty()));
        EXPECT_TRUE(slice.get("name").copyString() == "testView");
        EXPECT_TRUE(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
        EXPECT_TRUE(slice.get("deleted").isNone()); // no system properties
        EXPECT_TRUE((meta.init(slice, error) && expectedMeta == meta));
        auto tmpSlice = slice.get("links");
        EXPECT_TRUE((true == tmpSlice.isObject() && 0 == tmpSlice.length()));
      }

      // for persistence
      {
        arangodb::velocypack::Builder builder;

        builder.openObject();
        view->properties(builder, arangodb::LogicalDataSource::makeFlags(
                                      arangodb::LogicalDataSource::Serialize::Detailed,
                                      arangodb::LogicalDataSource::Serialize::ForPersistence));
        builder.close();

        auto slice = builder.slice();
        arangodb::iresearch::IResearchViewMeta meta;
        arangodb::iresearch::IResearchViewMetaState metaState;
        std::string error;

        EXPECT_TRUE(slice.isObject());
        EXPECT_TRUE((17U == slice.length()));
        EXPECT_TRUE(slice.get("name").copyString() == "testView");
        EXPECT_TRUE(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
        EXPECT_TRUE((slice.hasKey("deleted") && slice.get("deleted").isBoolean() && false == slice.get("deleted").getBoolean())); // has system properties
        EXPECT_TRUE((meta.init(slice, error) && expectedMeta == meta));
        EXPECT_TRUE((true == metaState.init(slice, error) && expectedMetaState == metaState));
        EXPECT_TRUE((false == slice.hasKey("links")));
      }
    }
  }

  // remove link from non-existant collection
  {
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto view = vocbase.createView(createJson->slice());
    ASSERT_TRUE((false == !view));

    arangodb::iresearch::IResearchViewMeta expectedMeta;
    arangodb::iresearch::IResearchViewMetaState expectedMetaState;
    auto updateJson = arangodb::velocypack::Parser::fromJson("{ \
      \"links\": { \
        \"testCollection\": null \
      }}");

    expectedMeta._cleanupIntervalStep = 52;
    EXPECT_TRUE((TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND == view->properties(updateJson->slice(), true).errorNumber()));

    // not for persistence
    {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      view->properties(builder, arangodb::LogicalDataSource::makeFlags(
                                    arangodb::LogicalDataSource::Serialize::Detailed));
      builder.close();

      auto slice = builder.slice();
      arangodb::iresearch::IResearchViewMeta meta;
      arangodb::iresearch::IResearchViewMetaState metaState;
      std::string error;

      EXPECT_TRUE(slice.isObject());
      EXPECT_TRUE((13U == slice.length()));
      EXPECT_TRUE((slice.hasKey("globallyUniqueId") && slice.get("globallyUniqueId").isString() && false == slice.get("globallyUniqueId").copyString().empty()));
      EXPECT_TRUE(slice.get("name").copyString() == "testView");
      EXPECT_TRUE(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
      EXPECT_TRUE(slice.get("deleted").isNone()); // no system properties
      EXPECT_TRUE((meta.init(slice, error) && expectedMeta == meta));
      auto tmpSlice = slice.get("links");
      EXPECT_TRUE((true == tmpSlice.isObject() && 0 == tmpSlice.length()));
    }

    // for persistence
    {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      view->properties(builder, arangodb::LogicalDataSource::makeFlags(
                                    arangodb::LogicalDataSource::Serialize::Detailed,
                                    arangodb::LogicalDataSource::Serialize::ForPersistence));
      builder.close();

      auto slice = builder.slice();
      arangodb::iresearch::IResearchViewMeta meta;
      arangodb::iresearch::IResearchViewMetaState metaState;
      std::string error;

      EXPECT_TRUE(slice.isObject());
      EXPECT_TRUE((17U == slice.length()));
      EXPECT_TRUE(slice.get("name").copyString() == "testView");
      EXPECT_TRUE(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
      EXPECT_TRUE((slice.hasKey("deleted") && slice.get("deleted").isBoolean() && false == slice.get("deleted").getBoolean())); // has system properties
      EXPECT_TRUE((meta.init(slice, error) && expectedMeta == meta));
      EXPECT_TRUE((true == metaState.init(slice, error) && expectedMetaState == metaState));
      EXPECT_TRUE((false == slice.hasKey("links")));
    }
  }

  // remove non-existant link
  {
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    ASSERT_TRUE((nullptr != logicalCollection));
    auto view = vocbase.createView(createJson->slice());
    ASSERT_TRUE((false == !view));

    arangodb::iresearch::IResearchViewMeta expectedMeta;
    arangodb::iresearch::IResearchViewMetaState expectedMetaState;
    auto updateJson = arangodb::velocypack::Parser::fromJson("{ \
      \"links\": { \
        \"testCollection\": null \
    }}");

    expectedMeta._cleanupIntervalStep = 52;
    EXPECT_TRUE((view->properties(updateJson->slice(), true).ok()));

    // not for persistence
    {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      view->properties(builder, arangodb::LogicalDataSource::makeFlags(
                                    arangodb::LogicalDataSource::Serialize::Detailed));
      builder.close();

      auto slice = builder.slice();
      arangodb::iresearch::IResearchViewMeta meta;
      arangodb::iresearch::IResearchViewMetaState metaState;
      std::string error;

      EXPECT_TRUE(slice.isObject());
      EXPECT_TRUE((13U == slice.length()));
      EXPECT_TRUE((slice.hasKey("globallyUniqueId") && slice.get("globallyUniqueId").isString() && false == slice.get("globallyUniqueId").copyString().empty()));
      EXPECT_TRUE(slice.get("name").copyString() == "testView");
      EXPECT_TRUE(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
      EXPECT_TRUE(slice.get("deleted").isNone()); // no system properties
      EXPECT_TRUE((meta.init(slice, error) && expectedMeta == meta));
      auto tmpSlice = slice.get("links");
      EXPECT_TRUE((true == tmpSlice.isObject() && 0 == tmpSlice.length()));
    }

    // for persistence
    {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      view->properties(builder, arangodb::LogicalDataSource::makeFlags(
                                    arangodb::LogicalDataSource::Serialize::Detailed,
                                    arangodb::LogicalDataSource::Serialize::ForPersistence));
      builder.close();

      auto slice = builder.slice();
      arangodb::iresearch::IResearchViewMeta meta;
      arangodb::iresearch::IResearchViewMetaState metaState;
      std::string error;

      EXPECT_TRUE(slice.isObject());
      EXPECT_TRUE((17U == slice.length()));
      EXPECT_TRUE(slice.get("name").copyString() == "testView");
      EXPECT_TRUE(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
      EXPECT_TRUE((slice.hasKey("deleted") && slice.get("deleted").isBoolean() && false == slice.get("deleted").getBoolean())); // has system properties
      EXPECT_TRUE((meta.init(slice, error) && expectedMeta == meta));
      EXPECT_TRUE((true == metaState.init(slice, error) && expectedMetaState == metaState));
      EXPECT_TRUE((false == slice.hasKey("links")));
    }
  }

  // remove + add link to same collection (reindex)
  {
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    ASSERT_TRUE((nullptr != logicalCollection));
    auto view = vocbase.createView(createJson->slice());
    ASSERT_TRUE((false == !view));

    // initial add of link
    {
      auto updateJson = arangodb::velocypack::Parser::fromJson(
        "{ \"links\": { \"testCollection\": {} } }"
      );
      EXPECT_TRUE((view->properties(updateJson->slice(), true).ok()));

      // not for persistence
      {
        arangodb::velocypack::Builder builder;

        builder.openObject();
        view->properties(builder, arangodb::LogicalDataSource::makeFlags(
                                      arangodb::LogicalDataSource::Serialize::Detailed));
        builder.close();

        auto slice = builder.slice();
        EXPECT_TRUE(slice.isObject());
        EXPECT_TRUE((13U == slice.length()));
        EXPECT_TRUE((slice.hasKey("globallyUniqueId") && slice.get("globallyUniqueId").isString() && false == slice.get("globallyUniqueId").copyString().empty()));
        EXPECT_TRUE(slice.get("name").copyString() == "testView");
        EXPECT_TRUE(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
        EXPECT_TRUE(slice.get("deleted").isNone()); // no system properties
        auto tmpSlice = slice.get("links");
        EXPECT_TRUE((true == tmpSlice.isObject() && 1 == tmpSlice.length()));
      }

      // for persistence
      {
        arangodb::velocypack::Builder builder;

        builder.openObject();
        view->properties(builder, arangodb::LogicalDataSource::makeFlags(
                                      arangodb::LogicalDataSource::Serialize::Detailed,
                                      arangodb::LogicalDataSource::Serialize::ForPersistence));
        builder.close();

        auto slice = builder.slice();
        EXPECT_TRUE(slice.isObject());
        EXPECT_TRUE((17U == slice.length()));
        EXPECT_TRUE(slice.get("name").copyString() == "testView");
        EXPECT_TRUE(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
        EXPECT_TRUE((slice.hasKey("deleted") && slice.get("deleted").isBoolean() && false == slice.get("deleted").getBoolean())); // has system properties
        auto tmpSlice = slice.get("collections");
        EXPECT_TRUE((true == tmpSlice.isArray() && 1 == tmpSlice.length()));
        EXPECT_TRUE((false == slice.hasKey("links")));
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

      EXPECT_TRUE((!initial.empty()));
      EXPECT_TRUE((view->properties(updateJson->slice(), true).ok()));

      // not for persistence
      {
        arangodb::velocypack::Builder builder;

        builder.openObject();
        view->properties(builder, arangodb::LogicalDataSource::makeFlags(
                                      arangodb::LogicalDataSource::Serialize::Detailed));
        builder.close();

        auto slice = builder.slice();
        EXPECT_TRUE(slice.isObject());
        EXPECT_TRUE((13U == slice.length()));
        EXPECT_TRUE((slice.hasKey("globallyUniqueId") && slice.get("globallyUniqueId").isString() && false == slice.get("globallyUniqueId").copyString().empty()));
        EXPECT_TRUE(slice.get("name").copyString() == "testView");
        EXPECT_TRUE(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
        EXPECT_TRUE(slice.get("deleted").isNone()); // no system properties
        auto tmpSlice = slice.get("links");
        EXPECT_TRUE((true == tmpSlice.isObject() && 1 == tmpSlice.length()));
      }

      // for persistence
      {
        arangodb::velocypack::Builder builder;

        builder.openObject();
        view->properties(builder, arangodb::LogicalDataSource::makeFlags(
                                      arangodb::LogicalDataSource::Serialize::Detailed,
                                      arangodb::LogicalDataSource::Serialize::ForPersistence));
        builder.close();

        auto slice = builder.slice();
        EXPECT_TRUE(slice.isObject());
        EXPECT_TRUE((17U == slice.length()));
        EXPECT_TRUE(slice.get("name").copyString() == "testView");
        EXPECT_TRUE(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
        EXPECT_TRUE((slice.hasKey("deleted") && slice.get("deleted").isBoolean() && false == slice.get("deleted").getBoolean())); // has system properties
        auto tmpSlice = slice.get("collections");
        EXPECT_TRUE((true == tmpSlice.isArray() && 1 == tmpSlice.length()));
        EXPECT_TRUE((false == slice.hasKey("links")));
      }

      std::unordered_set<TRI_idx_iid_t> actual;

      for (auto& index: logicalCollection->getIndexes()) {
        actual.emplace(index->id());
      }

      EXPECT_TRUE((initial != actual)); // a reindexing took place (link recreated)
    }
  }

  // update existing link (partial update)
  {
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    ASSERT_TRUE((nullptr != logicalCollection));
    auto view = vocbase.createView(createJson->slice());
    ASSERT_TRUE((false == !view));

    // initial add of link
    {
      auto updateJson = arangodb::velocypack::Parser::fromJson(
        "{ \"links\": { \"testCollection\": { \"includeAllFields\": true } } }"
      );
      EXPECT_TRUE((view->properties(updateJson->slice(), true).ok()));

      // not for persistence
      {
        arangodb::velocypack::Builder builder;

        builder.openObject();
        view->properties(builder, arangodb::LogicalDataSource::makeFlags(
                                      arangodb::LogicalDataSource::Serialize::Detailed));
        builder.close();

        auto slice = builder.slice();
        EXPECT_TRUE(slice.isObject());
        EXPECT_TRUE((13U == slice.length()));
        EXPECT_TRUE((slice.hasKey("globallyUniqueId") && slice.get("globallyUniqueId").isString() && false == slice.get("globallyUniqueId").copyString().empty()));
        EXPECT_TRUE(slice.get("name").copyString() == "testView");
        EXPECT_TRUE(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
        EXPECT_TRUE(slice.get("deleted").isNone()); // no system properties
        auto tmpSlice = slice.get("links");
        EXPECT_TRUE((true == tmpSlice.isObject() && 1 == tmpSlice.length()));
        tmpSlice = tmpSlice.get("testCollection");
        EXPECT_TRUE((true == tmpSlice.isObject()));
        tmpSlice = tmpSlice.get("includeAllFields");
        EXPECT_TRUE((true == tmpSlice.isBoolean() && true == tmpSlice.getBoolean()));
      }

      // for persistence
      {
        arangodb::velocypack::Builder builder;

        builder.openObject();
        view->properties(builder, arangodb::LogicalDataSource::makeFlags(
                                      arangodb::LogicalDataSource::Serialize::Detailed,
                                      arangodb::LogicalDataSource::Serialize::ForPersistence));
        builder.close();

        auto slice = builder.slice();
        EXPECT_TRUE(slice.isObject());
        EXPECT_TRUE((17U == slice.length()));
        EXPECT_TRUE(slice.get("name").copyString() == "testView");
        EXPECT_TRUE(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
        EXPECT_TRUE((slice.hasKey("deleted") && slice.get("deleted").isBoolean() && false == slice.get("deleted").getBoolean())); // has system properties
        auto tmpSlice = slice.get("collections");
        EXPECT_TRUE((true == tmpSlice.isArray() && 1 == tmpSlice.length()));
        EXPECT_TRUE((false == slice.hasKey("links")));
      }
    }

    // update link
    {
      auto updateJson = arangodb::velocypack::Parser::fromJson(
        "{ \"links\": { \"testCollection\": { } } }"
      );
      EXPECT_TRUE((view->properties(updateJson->slice(), true).ok()));

      // not for persistence
      {
        arangodb::velocypack::Builder builder;

        builder.openObject();
        view->properties(builder, arangodb::LogicalDataSource::makeFlags(
                                      arangodb::LogicalDataSource::Serialize::Detailed));
        builder.close();

        auto slice = builder.slice();
        EXPECT_TRUE(slice.isObject());
        EXPECT_TRUE((13U == slice.length()));
        EXPECT_TRUE((slice.hasKey("globallyUniqueId") && slice.get("globallyUniqueId").isString() && false == slice.get("globallyUniqueId").copyString().empty()));
        EXPECT_TRUE(slice.get("name").copyString() == "testView");
        EXPECT_TRUE(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
        EXPECT_TRUE(slice.get("deleted").isNone()); // no system properties
        auto tmpSlice = slice.get("links");
        EXPECT_TRUE((true == tmpSlice.isObject() && 1 == tmpSlice.length()));
        tmpSlice = tmpSlice.get("testCollection");
        EXPECT_TRUE((true == tmpSlice.isObject()));
        tmpSlice = tmpSlice.get("includeAllFields");
        EXPECT_TRUE((true == tmpSlice.isBoolean() && false == tmpSlice.getBoolean()));
      }

      // for persistence
      {
        arangodb::velocypack::Builder builder;

        builder.openObject();
        view->properties(builder, arangodb::LogicalDataSource::makeFlags(
                                      arangodb::LogicalDataSource::Serialize::Detailed,
                                      arangodb::LogicalDataSource::Serialize::ForPersistence));
        builder.close();

        auto slice = builder.slice();
        EXPECT_TRUE(slice.isObject());
        EXPECT_TRUE((17U == slice.length()));
        EXPECT_TRUE(slice.get("name").copyString() == "testView");
        EXPECT_TRUE(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
        EXPECT_TRUE((slice.hasKey("deleted") && slice.get("deleted").isBoolean() && false == slice.get("deleted").getBoolean())); // has system properties
        auto tmpSlice = slice.get("collections");
        EXPECT_TRUE((true == tmpSlice.isArray() && 1 == tmpSlice.length()));
        EXPECT_TRUE((false == slice.hasKey("links")));
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
    ASSERT_TRUE((nullptr != logicalCollection));
    auto logicalView = vocbase.createView(viewCreateJson->slice());
    ASSERT_TRUE((false == !logicalView));

    EXPECT_TRUE((true == logicalCollection->getIndexes().empty()));
    EXPECT_TRUE((true == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));

    // initial link creation
    {
      auto updateJson = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection\": {} } }");
      EXPECT_TRUE((logicalView->properties(updateJson->slice(), true).ok()));
      EXPECT_TRUE((false == logicalCollection->getIndexes().empty()));
      EXPECT_TRUE((false == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));
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

      EXPECT_TRUE((TRI_ERROR_FORBIDDEN == logicalView->properties(viewUpdateJson->slice(), true).errorNumber()));

      arangodb::velocypack::Builder builder;
      builder.openObject();
      logicalView->properties(builder,
                              arangodb::LogicalDataSource::makeFlags(
                                  arangodb::LogicalDataSource::Serialize::Detailed,
                                  arangodb::LogicalDataSource::Serialize::ForPersistence));  // 'forPersistence' to avoid auth check
      builder.close();

      auto slice = builder.slice();
      arangodb::iresearch::IResearchViewMeta meta;
      std::string error;
      EXPECT_TRUE((meta.init(slice, error) && expectedMeta == meta));
    }

    // subsequent update (overwrite) authorised (RO collection)
    {
      arangodb::auth::UserMap userMap;
      auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
      user.grantCollection(vocbase.name(), "testCollection", arangodb::auth::Level::RO); // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

      arangodb::iresearch::IResearchViewMeta expectedMeta;
      expectedMeta._cleanupIntervalStep = 62;

      EXPECT_TRUE((logicalView->properties(viewUpdateJson->slice(), true).ok()));

      arangodb::velocypack::Builder builder;
      builder.openObject();
      logicalView->properties(builder,
                              arangodb::LogicalDataSource::makeFlags(
                                  arangodb::LogicalDataSource::Serialize::Detailed,
                                  arangodb::LogicalDataSource::Serialize::ForPersistence));  // 'forPersistence' to avoid auth check
      builder.close();

      auto slice = builder.slice();
      arangodb::iresearch::IResearchViewMeta meta;
      std::string error;
      EXPECT_TRUE((meta.init(slice, error) && expectedMeta == meta));
    }
  }

  // add link (collection not authorized)
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\", \"id\": 100 }");
    auto viewCreateJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
    auto viewUpdateJson = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection\": {} } }");

    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    ASSERT_TRUE((nullptr != logicalCollection));
    auto logicalView = vocbase.createView(viewCreateJson->slice());
    ASSERT_TRUE((false == !logicalView));

    EXPECT_TRUE((true == logicalCollection->getIndexes().empty()));
    EXPECT_TRUE((true == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));

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

    EXPECT_TRUE((TRI_ERROR_FORBIDDEN == logicalView->properties(viewUpdateJson->slice(), false).errorNumber()));
    EXPECT_TRUE((true == logicalCollection->getIndexes().empty()));
    EXPECT_TRUE((true == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));
  }

  // drop link (collection not authorized)
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\", \"id\": 100 }");
    auto viewCreateJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
    auto viewUpdateJson = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection\": null } }");

    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    ASSERT_TRUE((nullptr != logicalCollection));
    auto logicalView = vocbase.createView(viewCreateJson->slice());
    ASSERT_TRUE((false == !logicalView));

    EXPECT_TRUE((true == logicalCollection->getIndexes().empty()));
    EXPECT_TRUE((true == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));

    // initial link creation
    {
      auto updateJson = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection\": {} } }");
      EXPECT_TRUE((logicalView->properties(updateJson->slice(), true).ok()));
      EXPECT_TRUE((false == logicalCollection->getIndexes().empty()));
      EXPECT_TRUE((false == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));
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

      EXPECT_TRUE((TRI_ERROR_FORBIDDEN == logicalView->properties(viewUpdateJson->slice(), true).errorNumber()));
      EXPECT_TRUE((false == logicalCollection->getIndexes().empty()));
      EXPECT_TRUE((false == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));
    }

    // subsequent update (overwrite) authorised (RO collection)
    {
      arangodb::auth::UserMap userMap;
      auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
      user.grantCollection(vocbase.name(), "testCollection", arangodb::auth::Level::RO); // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

      EXPECT_TRUE((logicalView->properties(viewUpdateJson->slice(), true).ok()));
      EXPECT_TRUE((true == logicalCollection->getIndexes().empty()));
      EXPECT_TRUE((true == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));
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
    ASSERT_TRUE((nullptr != logicalCollection0));
    auto logicalCollection1 = vocbase.createCollection(collection1Json->slice());
    ASSERT_TRUE((nullptr != logicalCollection1));
    auto logicalView = vocbase.createView(viewCreateJson->slice());
    ASSERT_TRUE((false == !logicalView));

    EXPECT_TRUE((true == logicalCollection0->getIndexes().empty()));
    EXPECT_TRUE((true == logicalCollection1->getIndexes().empty()));
    EXPECT_TRUE((true == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));

    // initial link creation
    {
      auto updateJson = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection0\": {} } }");
      EXPECT_TRUE((logicalView->properties(updateJson->slice(), true).ok()));
      EXPECT_TRUE((false == logicalCollection0->getIndexes().empty()));
      EXPECT_TRUE((true == logicalCollection1->getIndexes().empty()));
      EXPECT_TRUE((false == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));
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

      EXPECT_TRUE((TRI_ERROR_FORBIDDEN == logicalView->properties(viewUpdateJson->slice(), true).errorNumber()));
      EXPECT_TRUE((false == logicalCollection0->getIndexes().empty()));
      EXPECT_TRUE((true == logicalCollection1->getIndexes().empty()));
      EXPECT_TRUE((false == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));
    }

    // subsequent update (overwrite) authorised (RO collection)
    {
      arangodb::auth::UserMap userMap;
      auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
      user.grantCollection(vocbase.name(), "testCollection0", arangodb::auth::Level::RO); // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      user.grantCollection(vocbase.name(), "testCollection1", arangodb::auth::Level::RO); // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

      EXPECT_TRUE((logicalView->properties(viewUpdateJson->slice(), true).ok()));
      EXPECT_TRUE((false == logicalCollection0->getIndexes().empty()));
      EXPECT_TRUE((false == logicalCollection1->getIndexes().empty()));
      EXPECT_TRUE((false == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));
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
    ASSERT_TRUE((nullptr != logicalCollection0));
    auto logicalCollection1 = vocbase.createCollection(collection1Json->slice());
    ASSERT_TRUE((nullptr != logicalCollection1));
    auto logicalView = vocbase.createView(viewCreateJson->slice());
    ASSERT_TRUE((false == !logicalView));

    EXPECT_TRUE((true == logicalCollection0->getIndexes().empty()));
    EXPECT_TRUE((true == logicalCollection1->getIndexes().empty()));
    EXPECT_TRUE((true == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));

    // initial link creation
    {
      auto updateJson = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection0\": {}, \"testCollection1\": {} } }");
      EXPECT_TRUE((logicalView->properties(updateJson->slice(), true).ok()));
      EXPECT_TRUE((false == logicalCollection0->getIndexes().empty()));
      EXPECT_TRUE((false == logicalCollection1->getIndexes().empty()));
      EXPECT_TRUE((false == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));
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

      EXPECT_TRUE((TRI_ERROR_FORBIDDEN == logicalView->properties(viewUpdateJson->slice(), true).errorNumber()));
      EXPECT_TRUE((false == logicalCollection0->getIndexes().empty()));
      EXPECT_TRUE((false == logicalCollection1->getIndexes().empty()));
      EXPECT_TRUE((false == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));
    }

    // subsequent update (overwrite) authorised (RO collection)
    {
      arangodb::auth::UserMap userMap;
      auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
      user.grantCollection(vocbase.name(), "testCollection0", arangodb::auth::Level::RO); // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      user.grantCollection(vocbase.name(), "testCollection1", arangodb::auth::Level::RO); // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

      EXPECT_TRUE((logicalView->properties(viewUpdateJson->slice(), true).ok()));
      EXPECT_TRUE((false == logicalCollection0->getIndexes().empty()));
      EXPECT_TRUE((true == logicalCollection1->getIndexes().empty()));
      EXPECT_TRUE((false == logicalView->visitCollections([](TRI_voc_cid_t)->bool { return false; })));
    }
  }
}
