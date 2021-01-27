////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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

#include "gtest/gtest.h"

#include "analysis/token_attributes.hpp"
#include "analysis/analyzers.hpp"
#include "search/scorers.hpp"
#include "utils/locale_utils.hpp"
#include "utils/log.hpp"
#include "utils/utf8_path.hpp"
#include "utils/lz4compression.hpp"

#include "velocypack/Iterator.h"
#include "velocypack/Parser.h"

#include "IResearch/ExpressionContextMock.h"
#include "IResearch/common.h"
#include "Mocks/IResearchLinkMock.h"
#include "Mocks/LogLevels.h"
#include "Mocks/Servers.h"
#include "Mocks/StorageEngineMock.h"

#include "ApplicationFeatures/CommunicationFeaturePhase.h"
#include "ApplicationFeatures/GreetingsFeaturePhase.h"
#include "Aql/AqlFunctionFeature.h"
#include "Aql/AstNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Function.h"
#include "Aql/QueryRegistry.h"
#include "Aql/SortCondition.h"
#include "Aql/OptimizerRulesFeature.h"
#include "Basics/ArangoGlobalContext.h"
#include "Basics/error.h"
#include "Basics/files.h"
#include "Cluster/ClusterFeature.h"
#include "FeaturePhases/BasicFeaturePhaseServer.h"
#include "FeaturePhases/ClusterFeaturePhase.h"
#include "FeaturePhases/DatabaseFeaturePhase.h"
#include "FeaturePhases/V8FeaturePhase.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "IResearch/IResearchCommon.h"
#include "IResearch/IResearchDocument.h"
#include "IResearch/IResearchFeature.h"
#include "IResearch/IResearchLinkHelper.h"
#include "IResearch/IResearchLinkMeta.h"
#include "IResearch/IResearchLink.h"
#include "IResearch/IResearchView.h"
#include "Logger/LogTopic.h"
#include "Logger/Logger.h"
#include "Random/RandomFeature.h"
#include "RestServer/AqlFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/DatabasePathFeature.h"
#include "RestServer/FlushFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "RestServer/SystemDatabaseFeature.h"
#include "RestServer/ViewTypesFeature.h"
#include "Sharding/ShardingFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "Transaction/Methods.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/ExecContext.h"
#include "Utils/OperationOptions.h"
#include "Utils/SingleCollectionTransaction.h"
#include "V8Server/V8DealerFeature.h"
#include "VocBase/KeyGenerator.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/LogicalView.h"
#include "VocBase/ManagedDocumentResult.h"

#if USE_ENTERPRISE
#include "Enterprise/Ldap/LdapFeature.h"
#endif

namespace {

struct DocIdScorer: public irs::sort {
  static constexpr irs::string_ref type_name() noexcept {
    return "test_doc_id";
  }

  static ptr make(const irs::string_ref&) { PTR_NAMED(DocIdScorer, ptr); return ptr; }
  DocIdScorer(): irs::sort(irs::type<DocIdScorer>::get()) { }
  virtual sort::prepared::ptr prepare() const override { PTR_NAMED(Prepared, ptr); return ptr; }

  struct Prepared: public irs::prepared_sort_base<uint64_t, void> {
    virtual void collect(irs::byte_type*, const irs::index_reader& index, const irs::sort::field_collector* field, const irs::sort::term_collector* term) const override {}
    virtual irs::flags const& features() const override { return irs::flags::empty_instance(); }
    virtual bool less(const irs::byte_type* lhs, const irs::byte_type* rhs) const override { return traits_t::score_cast(lhs) < traits_t::score_cast(rhs); }
    virtual irs::sort::field_collector::ptr prepare_field_collector() const override { return nullptr; }
    virtual irs::sort::term_collector::ptr prepare_term_collector() const override { return nullptr; }
    virtual irs::score_function prepare_scorer(
        irs::sub_reader const& segment,
        irs::term_reader const& field,
        irs::byte_type const*,
        irs::byte_type* score_buf,
        irs::attribute_provider const& doc_attrs,
        irs::boost_t) const override {
      auto* doc = irs::get<irs::document>(doc_attrs);
      EXPECT_NE(nullptr, doc);

      return {
        std::make_unique<ScoreCtx>(doc, score_buf),
        [](irs::score_ctx* ctx) -> irs::byte_type const* {
          auto* state = static_cast<ScoreCtx*>(ctx);
          reinterpret_cast<uint64_t&>(*state->_score_buf) = state->_doc->value;
          return state->_score_buf;
        }
      };
    }
  };

  struct ScoreCtx: public irs::score_ctx {
    ScoreCtx(irs::document const* doc, irs::byte_type* score_buf) noexcept
      : _doc(doc), _score_buf(score_buf) { }
    irs::document const* _doc;
    irs::byte_type* _score_buf;
  };
};

REGISTER_SCORER_TEXT(DocIdScorer, DocIdScorer::make);

struct TestAttribute : public irs::attribute {
  static constexpr irs::string_ref type_name() noexcept {
    return "TestAttribute";
  }
};

REGISTER_ATTRIBUTE(TestAttribute);  // required to open reader on segments with analized fields

class TestAnalyzer : public irs::analysis::analyzer {
 public:
  static constexpr irs::string_ref type_name() noexcept {
    return "TestAnalyzer";
  }

  TestAnalyzer() : irs::analysis::analyzer(irs::type<TestAnalyzer>::get()) { }

  virtual irs::attribute* get_mutable(irs::type_info::type_id type) noexcept override {
    if (type == irs::type<TestAttribute>::id()) {
      return &_attr;
    }
    if (type == irs::type<irs::increment>::id()) {
      return &_increment;
    }
    if (type == irs::type<irs::term_attribute>::id()) {
      return &_term;
    }
    return nullptr;
  }

  static ptr make(irs::string_ref const& args) {
    auto slice = arangodb::iresearch::slice(args);
    if (slice.isNull()) throw std::exception();
    if (slice.isNone()) return nullptr;
    PTR_NAMED(TestAnalyzer, ptr);
    return ptr;
  }

  static bool normalize(irs::string_ref const& args, std::string& definition) {
    // same validation as for make,
    // as normalize usually called to sanitize data before make
    auto slice = arangodb::iresearch::slice(args);
    if (slice.isNull()) throw std::exception();
    if (slice.isNone()) return false;

    arangodb::velocypack::Builder builder;
    if (slice.isString()) {
      VPackObjectBuilder scope(&builder);
      arangodb::iresearch::addStringRef(builder, "args",
                                        arangodb::iresearch::getStringRef(slice));
    } else if (slice.isObject() && slice.hasKey("args") && slice.get("args").isString()) {
      VPackObjectBuilder scope(&builder);
      arangodb::iresearch::addStringRef(builder, "args",
                                        arangodb::iresearch::getStringRef(slice.get("args")));
    } else {
      return false;
    }

    definition = builder.buffer()->toString();

    return true;
  }

  virtual bool next() override {
    if (_data.empty()) return false;

    _term.value = irs::bytes_ref(_data.c_str(), 1);
    _data = irs::bytes_ref(_data.c_str() + 1, _data.size() - 1);
    return true;
  }

  virtual bool reset(irs::string_ref const& data) override {
    _data = irs::ref_cast<irs::byte_type>(data);
    return true;
  }

 private:
  irs::bytes_ref _data;
  irs::increment _increment;
  irs::term_attribute _term;
  TestAttribute _attr;
};

REGISTER_ANALYZER_VPACK(TestAnalyzer, TestAnalyzer::make, TestAnalyzer::normalize);

}

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

class IResearchViewTest
    : public ::testing::Test,
      public arangodb::tests::LogSuppressor<arangodb::Logger::AUTHENTICATION, arangodb::LogLevel::ERR>,
      public arangodb::tests::LogSuppressor<arangodb::Logger::CLUSTER, arangodb::LogLevel::FATAL>,
      public arangodb::tests::LogSuppressor<arangodb::Logger::FIXME, arangodb::LogLevel::FATAL> {
 protected:

  arangodb::tests::mocks::MockAqlServer server;
  std::unique_ptr<TRI_vocbase_t> system;
  std::string testFilesystemPath;

  IResearchViewTest() : server(false) {
    arangodb::tests::init();

    server.addFeature<arangodb::FlushFeature>(false);
    server.startFeatures();

    TransactionStateMock::abortTransactionCount = 0;
    TransactionStateMock::beginTransactionCount = 0;
    TransactionStateMock::commitTransactionCount = 0;

    auto& dbPathFeature = server.getFeature<arangodb::DatabasePathFeature>();
    arangodb::tests::setDatabasePath(dbPathFeature);  // ensure test data is stored in a unique directory
    testFilesystemPath = dbPathFeature.directory();

    long systemError;
    std::string systemErrorStr;
    TRI_CreateDirectory(testFilesystemPath.c_str(), systemError, systemErrorStr);
  }

  ~IResearchViewTest() {
    TRI_RemoveDirectory(testFilesystemPath.c_str());
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
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
    arangodb::LogicalView::ptr view;
    EXPECT_TRUE((arangodb::iresearch::IResearchView::factory().create(view, vocbase, json->slice()).ok()));
    EXPECT_TRUE((false == !view));

    arangodb::iresearch::IResearchViewMeta expectedMeta;
    arangodb::iresearch::IResearchViewMetaState expectedMetaState;
    arangodb::velocypack::Builder builder;

    builder.openObject();
    view->properties(builder, arangodb::LogicalDataSource::Serialization::Persistence);
    builder.close();

    auto slice = builder.slice();
    arangodb::iresearch::IResearchViewMeta meta;
    arangodb::iresearch::IResearchViewMetaState metaState;
    std::string error;

    EXPECT_EQ(19, slice.length());
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
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
    arangodb::LogicalView::ptr view;
    EXPECT_TRUE((arangodb::iresearch::IResearchView::factory().create(view, vocbase, json->slice()).ok()));
    EXPECT_TRUE((false == !view));

    arangodb::iresearch::IResearchViewMeta expectedMeta;
    arangodb::velocypack::Builder builder;

    builder.openObject();
    view->properties(builder, arangodb::LogicalDataSource::Serialization::Properties);
    builder.close();

    auto slice = builder.slice();
    arangodb::iresearch::IResearchViewMeta meta;
    std::string error;

    EXPECT_TRUE((slice.isObject()));
    EXPECT_EQ(15, slice.length());
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

    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
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

    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
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

    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    ASSERT_TRUE((nullptr != logicalCollection));

    struct ExecContext: public arangodb::ExecContext {
      ExecContext(): arangodb::ExecContext(arangodb::ExecContext::Type::Default, "", "",
                                           arangodb::auth::Level::NONE, arangodb::auth::Level::NONE, false) {}
    } execContext;
    arangodb::ExecContextScope scope(&execContext);
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

    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    EXPECT_TRUE((nullptr != logicalCollection));
    EXPECT_TRUE((true == !vocbase.lookupView("testView")));
    EXPECT_TRUE((true == logicalCollection->getIndexes().empty()));
    arangodb::LogicalView::ptr logicalView;
    EXPECT_TRUE((arangodb::iresearch::IResearchView::factory().create(logicalView, vocbase, viewCreateJson->slice()).ok()));
    ASSERT_TRUE((false == !logicalView));
    std::set<arangodb::DataSourceId> cids;
    logicalView->visitCollections([&cids](arangodb::DataSourceId cid) -> bool {
      cids.emplace(cid);
      return true;
    });
    EXPECT_TRUE((1 == cids.size()));
    EXPECT_TRUE((false == logicalCollection->getIndexes().empty()));

    arangodb::iresearch::IResearchViewMeta expectedMeta;
    arangodb::velocypack::Builder builder;

    builder.openObject();
    logicalView->properties(builder, arangodb::LogicalDataSource::Serialization::Properties);
    builder.close();

    auto slice = builder.slice();
    arangodb::iresearch::IResearchViewMeta meta;
    std::string error;
    EXPECT_TRUE((slice.isObject()));
    EXPECT_EQ(15, slice.length());
    EXPECT_TRUE((slice.hasKey("globallyUniqueId") && slice.get("globallyUniqueId").isString() && !slice.get("globallyUniqueId").copyString().empty()));
    EXPECT_TRUE((slice.get("name").copyString() == "testView"));
    EXPECT_TRUE((slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name()));
    EXPECT_TRUE((false == slice.hasKey("deleted")));
    EXPECT_TRUE((meta.init(slice, error) && expectedMeta == meta));

    auto tmpSlice = slice.get("links");
    EXPECT_TRUE((true == tmpSlice.isObject() && 1 == tmpSlice.length()));
    EXPECT_TRUE((true == tmpSlice.hasKey("testCollection")));
  }
}

TEST_F(IResearchViewTest, test_properties) {
  // new view definition with links
  auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\", \"id\": 100 }");
  auto viewCreateJson = arangodb::velocypack::Parser::fromJson(
    "{ \"name\": \"testView\", \"type\": \"arangosearch\", \"id\": 101, "
    "  \"links\": { "
    "    \"testCollection\": { "
    "      \"includeAllFields\":true, "
    "      \"analyzers\": [\"inPlace\"], "
    "      \"analyzerDefinitions\": [ { \"name\" : \"inPlace\", \"type\":\"identity\", \"properties\":{}, \"features\":[] } ]"
    "    } "
    "  } "
    "}"
  );

  Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
  auto logicalCollection = vocbase.createCollection(collectionJson->slice());
  EXPECT_NE(nullptr, logicalCollection);
  EXPECT_EQ(nullptr, vocbase.lookupView("testView"));
  EXPECT_TRUE(logicalCollection->getIndexes().empty());
  arangodb::LogicalView::ptr logicalView;
  EXPECT_TRUE(arangodb::iresearch::IResearchView::factory().create(logicalView, vocbase, viewCreateJson->slice()).ok());
  ASSERT_TRUE(logicalView);
  std::set<arangodb::DataSourceId> cids;
  logicalView->visitCollections([&cids](arangodb::DataSourceId cid) -> bool {
    cids.emplace(cid);
    return true;
  });
  EXPECT_EQ(1, cids.size());
  EXPECT_FALSE(logicalCollection->getIndexes().empty());

  // check serialization for listing
  {
    arangodb::velocypack::Builder builder;
    builder.openObject();
    logicalView->properties(builder, arangodb::LogicalDataSource::Serialization::List);
    builder.close();

    auto slice = builder.slice();
    EXPECT_TRUE(slice.isObject());
    EXPECT_EQ(4, slice.length());
    EXPECT_TRUE(slice.get("name").isString() && "testView" == slice.get("name").copyString());
    EXPECT_TRUE(slice.get("type").isString() && "arangosearch" == slice.get("type").copyString());
    EXPECT_TRUE(slice.get("id").isString() && "101" == slice.get("id").copyString());
    EXPECT_TRUE(slice.get("globallyUniqueId").isString() && !slice.get("globallyUniqueId").copyString().empty());
  }

  // check serialization for properties
  {
    VPackSlice tmpSlice, tmpSlice2;

    VPackBuilder builder;
    builder.openObject();
    logicalView->properties(builder, arangodb::LogicalDataSource::Serialization::Properties);
    builder.close();

    auto slice = builder.slice();
    EXPECT_TRUE(slice.isObject());
    EXPECT_EQ(15, slice.length());
    EXPECT_TRUE(slice.get("name").isString() && "testView" == slice.get("name").copyString());
    EXPECT_TRUE(slice.get("type").isString() && "arangosearch" == slice.get("type").copyString());
    EXPECT_TRUE(slice.get("id").isString() && "101" == slice.get("id").copyString());
    EXPECT_TRUE(slice.get("globallyUniqueId").isString() && !slice.get("globallyUniqueId").copyString().empty());
    EXPECT_TRUE(slice.get("consolidationIntervalMsec").isNumber() && 1000 == slice.get("consolidationIntervalMsec").getNumber<size_t>());
    EXPECT_TRUE(slice.get("cleanupIntervalStep").isNumber() && 2 == slice.get("cleanupIntervalStep").getNumber<size_t>());
    EXPECT_TRUE(slice.get("commitIntervalMsec").isNumber() && 1000 == slice.get("commitIntervalMsec").getNumber<size_t>());
    { // consolidation policy
      tmpSlice = slice.get("consolidationPolicy");
      EXPECT_TRUE(tmpSlice.isObject() && 6 == tmpSlice.length());
      tmpSlice2 = tmpSlice.get("type");
      EXPECT_TRUE(tmpSlice2.isString() && std::string("tier") == tmpSlice2.copyString());
      tmpSlice2 = tmpSlice.get("segmentsMin");
      EXPECT_TRUE(tmpSlice2.isNumber() && 1 == tmpSlice2.getNumber<size_t>());
      tmpSlice2 = tmpSlice.get("segmentsMax");
      EXPECT_TRUE(tmpSlice2.isNumber() && 10 == tmpSlice2.getNumber<size_t>());
      tmpSlice2 = tmpSlice.get("segmentsBytesFloor");
      EXPECT_TRUE(tmpSlice2.isNumber() && (size_t(2) * (1 << 20)) == tmpSlice2.getNumber<size_t>());
      tmpSlice2 = tmpSlice.get("segmentsBytesMax");
      EXPECT_TRUE(tmpSlice2.isNumber() && (size_t(5) * (1 << 30)) == tmpSlice2.getNumber<size_t>());
      tmpSlice2 = tmpSlice.get("minScore");
      EXPECT_TRUE(tmpSlice2.isNumber() && (0. == tmpSlice2.getNumber<double>()));
    }
    tmpSlice = slice.get("writebufferActive");
    EXPECT_TRUE(tmpSlice.isNumber<size_t>() && 0 == tmpSlice.getNumber<size_t>());
    tmpSlice = slice.get("writebufferIdle");
    EXPECT_TRUE(tmpSlice.isNumber<size_t>() && 64 == tmpSlice.getNumber<size_t>());
    tmpSlice = slice.get("writebufferSizeMax");
    EXPECT_TRUE(tmpSlice.isNumber<size_t>() && 32 * (size_t(1) << 20) == tmpSlice.getNumber<size_t>());
    tmpSlice = slice.get("primarySort");
    EXPECT_TRUE(tmpSlice.isArray());
    EXPECT_EQ(0, tmpSlice.length());
    tmpSlice = slice.get("primarySortCompression");
    EXPECT_TRUE(tmpSlice.isString());
    tmpSlice = slice.get("storedValues");
    EXPECT_TRUE(tmpSlice.isArray());
    EXPECT_EQ(0, tmpSlice.length());
    { // links
      tmpSlice = slice.get("links");
      EXPECT_TRUE(tmpSlice.isObject());
      EXPECT_EQ(1, tmpSlice.length());
      tmpSlice2 = tmpSlice.get("testCollection");
      EXPECT_TRUE(tmpSlice2.isObject());
      EXPECT_EQ(5, tmpSlice2.length());
      EXPECT_TRUE(tmpSlice2.get("analyzers").isArray() &&
                  1 == tmpSlice2.get("analyzers").length() &&
                  "inPlace" == tmpSlice2.get("analyzers").at(0).copyString());
      EXPECT_TRUE(tmpSlice2.get("fields").isObject() && 0 == tmpSlice2.get("fields").length());
      EXPECT_TRUE(tmpSlice2.get("includeAllFields").isBool() && tmpSlice2.get("includeAllFields").getBool());
      EXPECT_TRUE(tmpSlice2.get("trackListPositions").isBool() && !tmpSlice2.get("trackListPositions").getBool());
      EXPECT_TRUE(tmpSlice2.get("storeValues").isString() && "none" == tmpSlice2.get("storeValues").copyString());
    }
  }

  // check serialization for persistence
  {
    VPackSlice tmpSlice, tmpSlice2;

    arangodb::velocypack::Builder builder;
    builder.openObject();
    logicalView->properties(builder, arangodb::LogicalDataSource::Serialization::Persistence);
    builder.close();

    auto slice = builder.slice();
    EXPECT_TRUE(slice.isObject());
    EXPECT_EQ(19, slice.length());
    EXPECT_TRUE(slice.get("name").isString() && "testView" == slice.get("name").copyString());
    EXPECT_TRUE(slice.get("type").isString() && "arangosearch" == slice.get("type").copyString());
    EXPECT_TRUE(slice.get("id").isString() && "101" == slice.get("id").copyString());
    EXPECT_TRUE(slice.get("planId").isString() && "101" == slice.get("planId").copyString());
    EXPECT_TRUE(slice.get("globallyUniqueId").isString() && !slice.get("globallyUniqueId").copyString().empty());
    EXPECT_TRUE(slice.get("consolidationIntervalMsec").isNumber() && 1000 == slice.get("consolidationIntervalMsec").getNumber<size_t>());
    EXPECT_TRUE(slice.get("cleanupIntervalStep").isNumber() && 2 == slice.get("cleanupIntervalStep").getNumber<size_t>());
    EXPECT_TRUE(slice.get("commitIntervalMsec").isNumber() && 1000 == slice.get("commitIntervalMsec").getNumber<size_t>());
    EXPECT_TRUE(slice.get("deleted").isBool() && !slice.get("deleted").getBool());
    EXPECT_TRUE(slice.get("isSystem").isBool() && !slice.get("isSystem").getBool());
    EXPECT_TRUE(slice.get("collections").isArray() &&
                1 == slice.get("collections").length() &&
                100 == slice.get("collections").at(0).getNumber<size_t>());

    { // consolidation policy
      tmpSlice = slice.get("consolidationPolicy");
      EXPECT_TRUE(tmpSlice.isObject() && 6 == tmpSlice.length());
      tmpSlice2 = tmpSlice.get("type");
      EXPECT_TRUE(tmpSlice2.isString() && std::string("tier") == tmpSlice2.copyString());
      tmpSlice2 = tmpSlice.get("segmentsMin");
      EXPECT_TRUE(tmpSlice2.isNumber() && 1 == tmpSlice2.getNumber<size_t>());
      tmpSlice2 = tmpSlice.get("segmentsMax");
      EXPECT_TRUE(tmpSlice2.isNumber() && 10 == tmpSlice2.getNumber<size_t>());
      tmpSlice2 = tmpSlice.get("segmentsBytesFloor");
      EXPECT_TRUE(tmpSlice2.isNumber() && (size_t(2) * (1 << 20)) == tmpSlice2.getNumber<size_t>());
      tmpSlice2 = tmpSlice.get("segmentsBytesMax");
      EXPECT_TRUE(tmpSlice2.isNumber() && (size_t(5) * (1 << 30)) == tmpSlice2.getNumber<size_t>());
      tmpSlice2 = tmpSlice.get("minScore");
      EXPECT_TRUE(tmpSlice2.isNumber() && (0. == tmpSlice2.getNumber<double>()));
    }
    tmpSlice = slice.get("writebufferActive");
    EXPECT_TRUE(tmpSlice.isNumber<size_t>() && 0 == tmpSlice.getNumber<size_t>());
    tmpSlice = slice.get("writebufferIdle");
    EXPECT_TRUE(tmpSlice.isNumber<size_t>() && 64 == tmpSlice.getNumber<size_t>());
    tmpSlice = slice.get("writebufferSizeMax");
    EXPECT_TRUE(tmpSlice.isNumber<size_t>() && 32 * (size_t(1) << 20) == tmpSlice.getNumber<size_t>());
    tmpSlice = slice.get("primarySort");
    EXPECT_TRUE(tmpSlice.isArray());
    EXPECT_EQ(0, tmpSlice.length());
    tmpSlice = slice.get("primarySortCompression");
    EXPECT_TRUE(tmpSlice.isString());
    tmpSlice = slice.get("storedValues");
    EXPECT_TRUE(tmpSlice.isArray());
    EXPECT_EQ(0, tmpSlice.length());
    tmpSlice = slice.get("version");
    EXPECT_TRUE(tmpSlice.isNumber<uint32_t>() && 1 == tmpSlice.getNumber<uint32_t>());
  }

  // check serialization for inventory
  {
    VPackSlice tmpSlice, tmpSlice2;

    arangodb::velocypack::Builder builder;
    builder.openObject();
    logicalView->properties(builder, arangodb::LogicalDataSource::Serialization::Inventory);
    builder.close();

    auto slice = builder.slice();
    EXPECT_TRUE(slice.isObject());
    EXPECT_EQ(15, slice.length());
    EXPECT_TRUE(slice.get("name").isString() && "testView" == slice.get("name").copyString());
    EXPECT_TRUE(slice.get("type").isString() && "arangosearch" == slice.get("type").copyString());
    EXPECT_TRUE(slice.get("id").isString() && "101" == slice.get("id").copyString());
    EXPECT_TRUE(slice.get("globallyUniqueId").isString() && !slice.get("globallyUniqueId").copyString().empty());
    EXPECT_TRUE(slice.get("consolidationIntervalMsec").isNumber() && 1000 == slice.get("consolidationIntervalMsec").getNumber<size_t>());
    EXPECT_TRUE(slice.get("cleanupIntervalStep").isNumber() && 2 == slice.get("cleanupIntervalStep").getNumber<size_t>());
    EXPECT_TRUE(slice.get("commitIntervalMsec").isNumber() && 1000 == slice.get("commitIntervalMsec").getNumber<size_t>());
    { // consolidation policy
      tmpSlice = slice.get("consolidationPolicy");
      EXPECT_TRUE(tmpSlice.isObject() && 6 == tmpSlice.length());
      tmpSlice2 = tmpSlice.get("type");
      EXPECT_TRUE(tmpSlice2.isString() && std::string("tier") == tmpSlice2.copyString());
      tmpSlice2 = tmpSlice.get("segmentsMin");
      EXPECT_TRUE(tmpSlice2.isNumber() && 1 == tmpSlice2.getNumber<size_t>());
      tmpSlice2 = tmpSlice.get("segmentsMax");
      EXPECT_TRUE(tmpSlice2.isNumber() && 10 == tmpSlice2.getNumber<size_t>());
      tmpSlice2 = tmpSlice.get("segmentsBytesFloor");
      EXPECT_TRUE(tmpSlice2.isNumber() && (size_t(2) * (1 << 20)) == tmpSlice2.getNumber<size_t>());
      tmpSlice2 = tmpSlice.get("segmentsBytesMax");
      EXPECT_TRUE(tmpSlice2.isNumber() && (size_t(5) * (1 << 30)) == tmpSlice2.getNumber<size_t>());
      tmpSlice2 = tmpSlice.get("minScore");
      EXPECT_TRUE(tmpSlice2.isNumber() && (0. == tmpSlice2.getNumber<double>()));
    }
    tmpSlice = slice.get("writebufferActive");
    EXPECT_TRUE(tmpSlice.isNumber<size_t>() && 0 == tmpSlice.getNumber<size_t>());
    tmpSlice = slice.get("writebufferIdle");
    EXPECT_TRUE(tmpSlice.isNumber<size_t>() && 64 == tmpSlice.getNumber<size_t>());
    tmpSlice = slice.get("writebufferSizeMax");
    EXPECT_TRUE(tmpSlice.isNumber<size_t>() && 32 * (size_t(1) << 20) == tmpSlice.getNumber<size_t>());
    tmpSlice = slice.get("primarySort");
    EXPECT_TRUE(tmpSlice.isArray());
    EXPECT_EQ(0, tmpSlice.length());
    tmpSlice = slice.get("primarySortCompression");
    EXPECT_TRUE(tmpSlice.isString());
    tmpSlice = slice.get("storedValues");
    EXPECT_TRUE(tmpSlice.isArray());
    EXPECT_EQ(0, tmpSlice.length());
    { // links
      tmpSlice = slice.get("links");
      EXPECT_TRUE(tmpSlice.isObject());
      EXPECT_EQ(1, tmpSlice.length());
      tmpSlice2 = tmpSlice.get("testCollection");
      EXPECT_TRUE(tmpSlice2.isObject());
      EXPECT_EQ(9, tmpSlice2.length());
      EXPECT_TRUE(tmpSlice2.get("analyzers").isArray() &&
                  1 == tmpSlice2.get("analyzers").length() &&
                  "inPlace" == tmpSlice2.get("analyzers").at(0).copyString());
      EXPECT_TRUE(tmpSlice2.get("fields").isObject() && 0 == tmpSlice2.get("fields").length());
      EXPECT_TRUE(tmpSlice2.get("includeAllFields").isBool() && tmpSlice2.get("includeAllFields").getBool());
      EXPECT_TRUE(tmpSlice2.get("trackListPositions").isBool() && !tmpSlice2.get("trackListPositions").getBool());
      EXPECT_TRUE(tmpSlice2.get("storeValues").isString() && "none" == tmpSlice2.get("storeValues").copyString());

      tmpSlice2 = tmpSlice2.get("analyzerDefinitions");
      ASSERT_TRUE(tmpSlice2.isArray());
      ASSERT_EQ(1, tmpSlice2.length());
      tmpSlice2 = tmpSlice2.at(0);
      ASSERT_TRUE(tmpSlice2.isObject());
      EXPECT_EQ(4, tmpSlice2.length());
      EXPECT_TRUE(tmpSlice2.get("name").isString() && "inPlace" == tmpSlice2.get("name").copyString());
      EXPECT_TRUE(tmpSlice2.get("type").isString() && "identity" == tmpSlice2.get("type").copyString());
      EXPECT_TRUE(tmpSlice2.get("properties").isObject() && 0 == tmpSlice2.get("properties").length());
      EXPECT_TRUE(tmpSlice2.get("features").isArray() && 0 == tmpSlice2.get("features").length());
    }
  }
}

TEST_F(IResearchViewTest, test_vocbase_inventory) {
  // new view definition with links
  auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\", \"id\": 100 }");
  auto viewCreateJson = arangodb::velocypack::Parser::fromJson(
    "{ \"name\": \"testView\", \"type\": \"arangosearch\", \"id\": 101, "
    "  \"links\": { "
    "    \"testCollection\": { "
    "      \"incudeAllFields\":true, "
    "      \"analyzers\": [\"inPlace\"], "
    "      \"analyzerDefinitions\": [ { \"name\" : \"inPlace\", \"type\":\"identity\", \"properties\":{}, \"features\":[] } ]"
    "    } "
    "  } "
    "}"
  );

  Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
  auto logicalCollection = vocbase.createCollection(collectionJson->slice());
  EXPECT_NE(nullptr, logicalCollection);
  EXPECT_EQ(nullptr, vocbase.lookupView("testView"));
  EXPECT_TRUE(logicalCollection->getIndexes().empty());
  arangodb::LogicalView::ptr logicalView;
  EXPECT_TRUE(arangodb::iresearch::IResearchView::factory().create(logicalView, vocbase, viewCreateJson->slice()).ok());
  ASSERT_TRUE(logicalView);
  std::set<arangodb::DataSourceId> cids;
  logicalView->visitCollections([&cids](arangodb::DataSourceId cid) -> bool {
    cids.emplace(cid);
    return true;
  });
  EXPECT_EQ(1, cids.size());
  EXPECT_FALSE(logicalCollection->getIndexes().empty());

  // check vocbase inventory
  {
    arangodb::velocypack::Builder builder;
    builder.openObject();
    vocbase.inventory(builder, std::numeric_limits<TRI_voc_tick_t>::max(), [](arangodb::LogicalCollection const*) { return true; });

    auto slice = builder.close().slice();
    ASSERT_TRUE(slice.isObject());

    // ensure links are not exposed as indices
    auto collectionsSlice = slice.get("collections");
    ASSERT_TRUE(collectionsSlice.isArray());
    for (auto collectionSlice : VPackArrayIterator(collectionsSlice)) {
      ASSERT_TRUE(collectionSlice.isObject());
      auto indexesSlice = collectionSlice.get("indexes");
      ASSERT_TRUE(indexesSlice.isArray());
      for (auto indexSlice : VPackArrayIterator(indexesSlice)) {
        ASSERT_TRUE(indexSlice.isObject());
        ASSERT_TRUE(indexSlice.hasKey("type"));
        ASSERT_TRUE(indexSlice.get("type").isString());
        ASSERT_NE("arangosearch", indexSlice.get("type").copyString());
      }
    }

    // check views
    auto viewsSlice = slice.get("views");
    ASSERT_TRUE(viewsSlice.isArray());
    ASSERT_EQ(1, viewsSlice.length());
    auto viewSlice = viewsSlice.at(0);
    ASSERT_TRUE(viewSlice.isObject());

    VPackBuilder viewDefinition;
    viewDefinition.openObject();
    ASSERT_TRUE(logicalView->properties(viewDefinition,
                                        arangodb::LogicalDataSource::Serialization::Inventory).ok());
    viewDefinition.close();

    EXPECT_EQUAL_SLICES(viewDefinition.slice(), viewSlice);
  }
}

TEST_F(IResearchViewTest, test_cleanup) {
  auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
  auto linkJson = arangodb::velocypack::Parser::fromJson("{ \"view\": \"testView\", \"includeAllFields\": true }");
  auto json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\":\"arangosearch\", \"cleanupIntervalStep\":1, \"consolidationIntervalMsec\": 1000 }");
  Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
  auto logicalCollection = vocbase.createCollection(collectionJson->slice());
  ASSERT_TRUE((false == !logicalCollection));
  auto logicalView = vocbase.createView(json->slice());
  ASSERT_TRUE((false == !logicalView));
  auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
  ASSERT_TRUE((false == !view));
  auto index = StorageEngineMock::buildLinkMock(arangodb::IndexId{42}, *logicalCollection, linkJson->slice());
  ASSERT_NE(nullptr, index.get());
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
    EXPECT_TRUE((link->insert(trx, arangodb::LocalDocumentId(0), doc->slice()).ok()));
    EXPECT_TRUE((trx.commit().ok()));
    EXPECT_TRUE(link->commit().ok());
  }

  auto const memory = index->memory();

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
    EXPECT_TRUE((link->remove(trx, arangodb::LocalDocumentId(0), arangodb::velocypack::Slice::emptyObjectSlice()).ok()));
    EXPECT_TRUE((trx.commit().ok()));
    EXPECT_TRUE(link->commit().ok());
  }

  // wait for commit thread
  size_t const MAX_ATTEMPTS = 200;
  size_t attempt = 0;

  while (memory <= index->memory() && attempt < MAX_ATTEMPTS) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    ++attempt;
  }

  // ensure memory was freed
  EXPECT_TRUE(index->memory() <= memory);
}

TEST_F(IResearchViewTest, test_consolidate) {
  auto viewCreateJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\":\"arangosearch\", \"consolidationIntervalMsec\": 1000 }");
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
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
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
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
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
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
  dataPath =
      ((((irs::utf8_path() /= testFilesystemPath) /=
         std::string("databases")) /=
        (std::string("database-") + std::to_string(vocbase.id()))) /=
       (std::string("arangosearch-") + std::to_string(logicalCollection->id().id()) + "_" +
        std::to_string(arangodb::iresearch::IResearchLinkHelper::find(*logicalCollection, *view)
                           ->id()
                           .id())))
          .utf8();
  EXPECT_TRUE((true == TRI_IsDirectory(dataPath.c_str())));

  {
    struct ExecContext: public arangodb::ExecContext {
      ExecContext(): arangodb::ExecContext(arangodb::ExecContext::Type::Default, "", "",
                                           arangodb::auth::Level::NONE, arangodb::auth::Level::NONE, false) {}
    } execContext;
    arangodb::ExecContextScope execContextScope(&execContext);
    auto* authFeature = arangodb::AuthenticationFeature::instance();
    auto* userManager = authFeature->userManager();
    
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
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
  auto logicalCollection = vocbase.createCollection(collectionJson->slice());
  ASSERT_TRUE((false == !logicalCollection));
  auto logicalView = vocbase.createView(viewCreateJson->slice());
  ASSERT_TRUE((false == !logicalView));
  auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
  ASSERT_TRUE((false == !view));

  EXPECT_TRUE((true == logicalView->properties(viewUpdateJson->slice(), true).ok()));
  EXPECT_TRUE((false == logicalView->visitCollections(
                            [](arangodb::DataSourceId) -> bool { return false; })));

  EXPECT_TRUE((true == logicalCollection->drop().ok()));
  EXPECT_TRUE((true == logicalView->visitCollections(
                           [](arangodb::DataSourceId) -> bool { return false; })));

  EXPECT_TRUE((true == logicalView->drop().ok()));
}

TEST_F(IResearchViewTest, test_drop_cid) {
  static std::vector<std::string> const EMPTY;

  // cid not in list of collections for snapshot (view definition not updated, not persisted)
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
    auto linkJson = arangodb::velocypack::Parser::fromJson("{ \"view\": \"testView\", \"includeAllFields\": true }");
    auto json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\":\"arangosearch\" }");
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    ASSERT_TRUE((false == !logicalCollection));
    auto logicalView = vocbase.createView(json->slice());
    ASSERT_TRUE((false == !logicalView));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
    ASSERT_TRUE((false == !view));
  
    auto index = StorageEngineMock::buildLinkMock(arangodb::IndexId{42}, *logicalCollection, linkJson->slice());
    ASSERT_NE(nullptr, index);
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
      EXPECT_TRUE((link->insert(trx, arangodb::LocalDocumentId(0), doc->slice()).ok()));
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
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    ASSERT_TRUE((false == !logicalCollection));
    auto logicalView = vocbase.createView(json->slice());
    ASSERT_TRUE((false == !logicalView));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
    ASSERT_TRUE((false == !view));
    auto index = StorageEngineMock::buildLinkMock(arangodb::IndexId{42}, *logicalCollection, linkJson->slice());
    ASSERT_NE(nullptr, index);
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
      EXPECT_TRUE((link->insert(trx, arangodb::LocalDocumentId(0), doc->slice()).ok()));
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
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    ASSERT_TRUE((false == !logicalCollection));
    auto logicalView = vocbase.createView(json->slice());
    ASSERT_TRUE((false == !logicalView));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
    ASSERT_TRUE((false == !view));
    auto index = StorageEngineMock::buildLinkMock(arangodb::IndexId{__LINE__}, *logicalCollection, linkJson->slice());
    ASSERT_NE(nullptr, index);
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
      EXPECT_TRUE((link->insert(trx, arangodb::LocalDocumentId(0), doc->slice()).ok()));
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
      auto beforeRecovery = StorageEngineMock::recoveryStateResult;
      StorageEngineMock::recoveryStateResult = arangodb::RecoveryState::IN_PROGRESS;
      auto restoreRecovery = irs::make_finally([&beforeRecovery]()->void { StorageEngineMock::recoveryStateResult = beforeRecovery; });

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
      std::unordered_set<arangodb::DataSourceId> expected;
      std::set<arangodb::DataSourceId> actual;

      EXPECT_TRUE((view->visitCollections([&actual](arangodb::DataSourceId cid) -> bool {
        actual.emplace(cid);
        return true;
      })));

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
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    ASSERT_TRUE((false == !logicalCollection));
    auto logicalView = vocbase.createView(json->slice());
    ASSERT_TRUE((false == !logicalView));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
    ASSERT_TRUE((false == !view));
    auto index = StorageEngineMock::buildLinkMock(arangodb::IndexId{__LINE__}, *logicalCollection, linkJson->slice());
    ASSERT_NE(nullptr, index);
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
      EXPECT_TRUE((link->insert(trx, arangodb::LocalDocumentId(0), doc->slice()).ok()));
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
      std::unordered_set<arangodb::DataSourceId> expected = {logicalCollection->id()};
      std::set<arangodb::DataSourceId> actual;

      EXPECT_TRUE((view->visitCollections([&actual](arangodb::DataSourceId cid) -> bool {
        actual.emplace(cid);
        return true;
      })));

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
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    ASSERT_TRUE((false == !logicalCollection));
    auto logicalView = vocbase.createView(json->slice());
    ASSERT_TRUE((false == !logicalView));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
    ASSERT_TRUE((false == !view));
    auto index = StorageEngineMock::buildLinkMock(arangodb::IndexId{__LINE__}, *logicalCollection, linkJson->slice());
    ASSERT_NE(nullptr, index);
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
      EXPECT_TRUE((link->insert(trx, arangodb::LocalDocumentId(0), doc->slice()).ok()));
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
      auto beforeRecovery = StorageEngineMock::recoveryStateResult;
      StorageEngineMock::recoveryStateResult = arangodb::RecoveryState::IN_PROGRESS;
      auto restoreRecovery = irs::make_finally([&beforeRecovery]()->void { StorageEngineMock::recoveryStateResult = beforeRecovery; });

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
      std::unordered_set<arangodb::DataSourceId> expected;
      std::set<arangodb::DataSourceId> actual;

      EXPECT_TRUE((view->visitCollections([&actual](arangodb::DataSourceId cid) -> bool {
        actual.emplace(cid);
        return true;
      })));

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
      auto& feature = server.getFeature<arangodb::DatabaseFeature>();

      EXPECT_NO_THROW((feature.recoveryDone()));
    }
  }
}

TEST_F(IResearchViewTest, test_drop_database) {
  auto viewCreateJson = arangodb::velocypack::Parser::fromJson("{ \"id\": \"42\", \"name\": \"testView\", \"type\": \"arangosearch\" }");
  auto& databaseFeature = server.getFeature<arangodb::DatabaseFeature>();

  size_t beforeCount = 0;
  auto before = StorageEngineMock::before;
  auto restore = irs::make_finally([&before]()->void { StorageEngineMock::before = before; });
  StorageEngineMock::before = [&beforeCount]()->void { ++beforeCount; };

  TRI_vocbase_t* vocbase; // will be owned by DatabaseFeature
  arangodb::CreateDatabaseInfo testDBInfo(server.server(),
                                          arangodb::ExecContext::current());
  testDBInfo.load("testDatabase" TOSTRING(__LINE__), 3);
  ASSERT_TRUE(databaseFeature.createDatabase(std::move(testDBInfo), vocbase).ok());
  ASSERT_TRUE((nullptr != vocbase));

  beforeCount = 0; // reset before call to StorageEngine::createView(...)
  auto logicalView = vocbase->createView(viewCreateJson->slice());
  ASSERT_TRUE((false == !logicalView));
  EXPECT_TRUE((1 == beforeCount)); // +1 for StorageEngineMock::createView(...)

  beforeCount = 0; // reset before call to StorageEngine::dropView(...)
  EXPECT_TRUE((TRI_ERROR_NO_ERROR == databaseFeature.dropDatabase(vocbase->id(), true)));
  EXPECT_TRUE((1 == beforeCount));
}

TEST_F(IResearchViewTest, test_instantiate) {
  // valid version
  {
    auto json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\", \"version\": 1 }");
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
    arangodb::LogicalView::ptr view;
    EXPECT_TRUE((arangodb::iresearch::IResearchView::factory().instantiate(view, vocbase, json->slice()).ok()));
    EXPECT_TRUE((false == !view));
  }

  // intantiate view from old version
  {
    auto json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\", \"version\": 0 }");
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
    arangodb::LogicalView::ptr view;
    EXPECT_TRUE(arangodb::iresearch::IResearchView::factory().instantiate(view, vocbase, json->slice()).ok());
    EXPECT_TRUE(nullptr != view);
  }

  // unsupported version
  {
    auto json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\", \"version\": 123456789 }");
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
    arangodb::LogicalView::ptr view;
    EXPECT_TRUE((!arangodb::iresearch::IResearchView::factory().instantiate(view, vocbase, json->slice()).ok()));
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
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    ASSERT_TRUE((false == !logicalCollection));
    auto logicalView = vocbase.createView(json->slice());
    ASSERT_TRUE((false == !logicalView));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
    ASSERT_TRUE((false == !view));
    auto index = StorageEngineMock::buildLinkMock(arangodb::IndexId{42}, *logicalCollection, linkJson->slice());
    ASSERT_NE(nullptr, index);
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
      EXPECT_TRUE((link->insert(trx, arangodb::LocalDocumentId(0), doc->slice()).ok()));
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
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    ASSERT_TRUE((false == !logicalCollection));
    auto logicalView = vocbase.createView(json->slice());
    ASSERT_TRUE((false == !logicalView));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
    ASSERT_TRUE((false == !view));
    auto index = StorageEngineMock::buildLinkMock(arangodb::IndexId{42}, *logicalCollection, linkJson->slice());
    ASSERT_NE(nullptr, index);
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
      EXPECT_TRUE((link->insert(trx, arangodb::LocalDocumentId(0), doc->slice()).ok()));
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
  struct Link : public arangodb::iresearch::IResearchLink {
    Link(arangodb::IndexId id, arangodb::LogicalCollection& col)
        : IResearchLink(id, col) {
      auto json = VPackParser::fromJson(R"({ "view": "42" })");
      EXPECT_TRUE(init(json->slice()).ok());
    }
  };

  // emplace (already in list)
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"id\": 42, \"name\": \"testCollection\" }");
    auto linkJson = arangodb::velocypack::Parser::fromJson("{ \"view\": \"testView\", \"includeAllFields\": true }");
    auto json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\":\"arangosearch\", \"collections\": [ 42 ] }");
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    ASSERT_TRUE((false == !logicalCollection));
    auto logicalView = vocbase.createView(json->slice());
    ASSERT_TRUE((false == !logicalView));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
    ASSERT_TRUE((false == !view));
    auto index = StorageEngineMock::buildLinkMock(arangodb::IndexId{42}, *logicalCollection, linkJson->slice());
    ASSERT_NE(nullptr, index);
    auto link = std::dynamic_pointer_cast<arangodb::iresearch::IResearchLink>(index);
    ASSERT_TRUE((false == !link));

    // collection in view before
    {
      std::unordered_set<arangodb::DataSourceId> expected = {arangodb::DataSourceId{42}};
      std::set<arangodb::DataSourceId> actual;

      EXPECT_TRUE((view->visitCollections([&actual](arangodb::DataSourceId cid) -> bool {
        actual.emplace(cid);
        return true;
      })));

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

      auto lock = link->self()->lock();
      EXPECT_FALSE(view->link(link->self()).ok());
      EXPECT_FALSE(persisted); // emplace() does not modify view meta if cid existed previously
    }

    // collection in view after
    {
      std::unordered_set<arangodb::DataSourceId> expected = {arangodb::DataSourceId{42}};
      std::set<arangodb::DataSourceId> actual;

      EXPECT_TRUE((view->visitCollections([&actual](arangodb::DataSourceId cid) -> bool {
        actual.emplace(cid);
        return true;
      })));

      for (auto& cid: expected) {
        EXPECT_EQ(1, actual.erase(cid));
      }

      EXPECT_TRUE(actual.empty());
    }
  }

  // emplace (not in list)
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"id\": 42, \"name\": \"testCollection\" }");
    auto json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\":\"arangosearch\" }");
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    ASSERT_TRUE((false == !logicalCollection));
    auto logicalView = vocbase.createView(json->slice());
    ASSERT_TRUE((false == !logicalView));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
    ASSERT_TRUE((false == !view));

    // collection in view before
    {
      std::unordered_set<arangodb::DataSourceId> expected;
      std::set<arangodb::DataSourceId> actual;

      EXPECT_TRUE((view->visitCollections([&actual](arangodb::DataSourceId cid) -> bool {
        actual.emplace(cid);
        return true;
      })));

      for (auto& cid: expected) {
        EXPECT_TRUE((1 == actual.erase(cid)));
      }

      EXPECT_TRUE((actual.empty()));
    }

    // emplace cid 42
    {
      Link link(arangodb::IndexId{42}, *logicalCollection);

      bool persisted = false;
      auto before = StorageEngineMock::before;
      auto restore = irs::make_finally([&before]()->void { StorageEngineMock::before = before; });
      StorageEngineMock::before = [&persisted]()->void { persisted = true; };
      auto asyncLinkPtr = std::make_shared<arangodb::iresearch::IResearchLink::AsyncLinkPtr::element_type>(&link);

      EXPECT_TRUE(view->link(asyncLinkPtr).ok());
      EXPECT_TRUE(persisted); // emplace() modifies view meta if cid did not exist previously
    }

    // collection in view after
    {
      std::unordered_set<arangodb::DataSourceId> expected = {arangodb::DataSourceId{42}};
      std::set<arangodb::DataSourceId> actual;

      EXPECT_TRUE((view->visitCollections([&actual](arangodb::DataSourceId cid) -> bool {
        actual.emplace(cid);
        return true;
      })));

      for (auto& cid: expected) {
        EXPECT_EQ(1, actual.erase(cid));
      }

      EXPECT_TRUE(actual.empty());
    }
  }

  // emplace (not in list, not persisted until recovery is complete)
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"id\": 42, \"name\": \"testCollection\" }");
    auto json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\":\"arangosearch\"  }");
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    ASSERT_TRUE((false == !logicalCollection));
    auto logicalView = vocbase.createView(json->slice());
    ASSERT_TRUE((false == !logicalView));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
    ASSERT_TRUE((false == !view));

    // collection in view before
    {
      std::unordered_set<arangodb::DataSourceId> expected;
      std::set<arangodb::DataSourceId> actual;

      EXPECT_TRUE((view->visitCollections([&actual](arangodb::DataSourceId cid) -> bool {
        actual.emplace(cid);
        return true;
      })));

      for (auto& cid: expected) {
        EXPECT_EQ(1, actual.erase(cid));
      }

      EXPECT_TRUE(actual.empty());
    }

    // emplace cid 42
    {
      Link link(arangodb::IndexId{42}, *logicalCollection);

      bool persisted = false;
      auto before = StorageEngineMock::before;
      auto restore = irs::make_finally([&before]()->void { StorageEngineMock::before = before; });
      StorageEngineMock::before = [&persisted]()->void { persisted = true; };
      auto beforeRecovery = StorageEngineMock::recoveryStateResult;
      StorageEngineMock::recoveryStateResult = arangodb::RecoveryState::IN_PROGRESS;
      auto restoreRecovery = irs::make_finally([&beforeRecovery]()->void { StorageEngineMock::recoveryStateResult = beforeRecovery; });
      auto asyncLinkPtr = std::make_shared<arangodb::iresearch::IResearchLink::AsyncLinkPtr::element_type>(&link);

      EXPECT_TRUE(view->link(asyncLinkPtr).ok());
      EXPECT_FALSE(persisted); // emplace() modifies view meta if cid did not exist previously (but not persisted until after recovery)
    }

    // collection in view after
    {
      std::unordered_set<arangodb::DataSourceId> expected = {arangodb::DataSourceId{42}};
      std::set<arangodb::DataSourceId> actual;

      EXPECT_TRUE((view->visitCollections([&actual](arangodb::DataSourceId cid) -> bool {
        actual.emplace(cid);
        return true;
      })));

      for (auto& cid: expected) {
        EXPECT_EQ(1, actual.erase(cid));
      }

      EXPECT_TRUE(actual.empty());
    }
  }

  // emplace (not in list, view definition persist failure)
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"id\": 42, \"name\": \"testCollection\" }");
    auto json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\":\"arangosearch\" }");
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    ASSERT_TRUE((false == !logicalCollection));
    auto logicalView = vocbase.createView(json->slice());
    ASSERT_TRUE((false == !logicalView));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
    ASSERT_TRUE((false == !view));

    // collection in view before
    {
      std::unordered_set<arangodb::DataSourceId> expected;
      std::set<arangodb::DataSourceId> actual;

      EXPECT_TRUE((view->visitCollections([&actual](arangodb::DataSourceId cid) -> bool {
        actual.emplace(cid);
        return true;
      })));

      for (auto& cid: expected) {
        EXPECT_EQ(1, actual.erase(cid));
      }

      EXPECT_TRUE(actual.empty());
    }

    // emplace cid 42
    {
      auto before = StorageEngineMock::before;
      auto restore = irs::make_finally([&before]()->void { StorageEngineMock::before = before; });
      StorageEngineMock::before = []()->void { throw std::exception(); };
      Link link(arangodb::IndexId{42}, *logicalCollection);
      auto asyncLinkPtr = std::make_shared<arangodb::iresearch::IResearchLink::AsyncLinkPtr::element_type>(&link);

      EXPECT_TRUE((false == view->link(asyncLinkPtr).ok()));
    }

    // collection in view after
    {
      std::unordered_set<arangodb::DataSourceId> expected;
      std::set<arangodb::DataSourceId> actual;

      EXPECT_TRUE((view->visitCollections([&actual](arangodb::DataSourceId cid) -> bool {
        actual.emplace(cid);
        return true;
      })));

      for (auto& cid: expected) {
        EXPECT_EQ(1, actual.erase(cid));
      }

      EXPECT_TRUE(actual.empty());
    }
  }

  // emplace (not in list, view definition persist failure on recovery completion)
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"id\": 42, \"name\": \"testCollection\" }");
    auto json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\":\"arangosearch\" }");
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    ASSERT_TRUE((false == !logicalCollection));
    auto logicalView = vocbase.createView(json->slice());
    ASSERT_TRUE((false == !logicalView));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
    ASSERT_TRUE((false == !view));

    // collection in view before
    {
      std::unordered_set<arangodb::DataSourceId> expected;
      std::set<arangodb::DataSourceId> actual;

      EXPECT_TRUE((view->visitCollections([&actual](arangodb::DataSourceId cid) -> bool {
        actual.emplace(cid);
        return true;
      })));

      for (auto& cid: expected) {
        EXPECT_EQ(1, actual.erase(cid));
      }

      EXPECT_TRUE(actual.empty());
    }

    // emplace cid 42
    {
      Link link(arangodb::IndexId{42}, *logicalCollection);

      bool persisted = false;
      auto before = StorageEngineMock::before;
      auto restore = irs::make_finally([&before]()->void { StorageEngineMock::before = before; });
      StorageEngineMock::before = [&persisted]()->void { persisted = true; };
      auto beforeRecovery = StorageEngineMock::recoveryStateResult;
      StorageEngineMock::recoveryStateResult = arangodb::RecoveryState::IN_PROGRESS;
      auto restoreRecovery = irs::make_finally([&beforeRecovery]()->void { StorageEngineMock::recoveryStateResult = beforeRecovery; });
      auto asyncLinkPtr = std::make_shared<arangodb::iresearch::IResearchLink::AsyncLinkPtr::element_type>(&link);

      EXPECT_TRUE(view->link(asyncLinkPtr).ok());
      EXPECT_FALSE(persisted); // emplace() modifies view meta if cid did not exist previously (but not persisted until after recovery)
    }

    // collection in view after
    {
      std::unordered_set<arangodb::DataSourceId> expected = {arangodb::DataSourceId{42}};
      std::set<arangodb::DataSourceId> actual;

      EXPECT_TRUE(view->visitCollections([&actual](arangodb::DataSourceId cid) -> bool {
        actual.emplace(cid);
        return true;
      }));

      for (auto& cid: expected) {
        EXPECT_EQ(1, actual.erase(cid));
      }

      EXPECT_TRUE(actual.empty());
    }

    // persistence fails during execution of callback
    {
      auto before = StorageEngineMock::before;
      auto restore = irs::make_finally([&before]()->void { StorageEngineMock::before = before; });
      StorageEngineMock::before = []()->void { throw std::exception(); };
      auto& feature = server.getFeature<arangodb::DatabaseFeature>();

      EXPECT_NO_THROW(feature.recoveryDone());
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

  // in recovery (skip operations before or at recovery tick)
  {
    auto before = StorageEngineMock::recoveryStateResult;
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    ASSERT_NE(nullptr, logicalCollection);
    auto viewImpl = vocbase.createView(viewJson->slice());
    ASSERT_NE(nullptr, viewImpl);
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(viewImpl.get());
    ASSERT_NE(nullptr, view);
    StorageEngineMock::recoveryTickResult = 42;
    StorageEngineMock::recoveryStateResult = arangodb::RecoveryState::DONE;
    StorageEngineMock::recoveryTickCallback = []() {
      StorageEngineMock::recoveryStateResult = arangodb::RecoveryState::IN_PROGRESS;
    };
    auto index = StorageEngineMock::buildLinkMock(arangodb::IndexId{42}, *logicalCollection, linkJson->slice());
    StorageEngineMock::recoveryTickCallback = []() {};
    auto restore = irs::make_finally([&before]()->void {
      StorageEngineMock::recoveryStateResult = before;
      StorageEngineMock::recoveryTickResult = 0;
    });

    ASSERT_NE(nullptr, index);
    auto link = std::dynamic_pointer_cast<arangodb::iresearch::IResearchLink>(index);
    ASSERT_NE(nullptr, link);

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

      // skip tick operations before recovery tick
      StorageEngineMock::recoveryTickResult = 41;
      EXPECT_TRUE(link->insert(trx, arangodb::LocalDocumentId(1), docJson->slice()).ok());
      StorageEngineMock::recoveryTickResult = 42;
      EXPECT_TRUE(link->insert(trx, arangodb::LocalDocumentId(2), docJson->slice()).ok());

      // insert operations after recovery tick
      StorageEngineMock::recoveryTickResult = 43;
      EXPECT_TRUE(link->insert(trx, arangodb::LocalDocumentId(1), docJson->slice()).ok());
      EXPECT_TRUE(link->insert(trx, arangodb::LocalDocumentId(2), docJson->slice()).ok());

      EXPECT_TRUE(trx.commit().ok());
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
    EXPECT_EQ(2, snapshot->live_docs_count());
  }

  // in recovery batch (skip operations before or at recovery tick)
  {
    auto before = StorageEngineMock::recoveryStateResult;
    StorageEngineMock::recoveryStateResult = arangodb::RecoveryState::IN_PROGRESS;
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));

    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    ASSERT_TRUE((false == !logicalCollection));
    auto viewImpl = vocbase.createView(viewJson->slice());
    EXPECT_TRUE((false == !viewImpl));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(viewImpl.get());
    EXPECT_TRUE((nullptr != view));

    StorageEngineMock::recoveryTickResult = 42;
    StorageEngineMock::recoveryStateResult = arangodb::RecoveryState::DONE;
    StorageEngineMock::recoveryTickCallback = []() {
      StorageEngineMock::recoveryStateResult = arangodb::RecoveryState::IN_PROGRESS;
    };
    auto index = StorageEngineMock::buildLinkMock(arangodb::IndexId{42}, *logicalCollection, linkJson->slice());
    StorageEngineMock::recoveryTickCallback = []() {};
    auto restore = irs::make_finally([&before]()->void {
      StorageEngineMock::recoveryStateResult = before;
      StorageEngineMock::recoveryTickResult = 0;
    });

    ASSERT_NE(nullptr, index);
    auto link = std::dynamic_pointer_cast<arangodb::iresearch::IResearchLink>(index);
    ASSERT_NE(nullptr, link);

    {
      auto docJson = arangodb::velocypack::Parser::fromJson("{\"abc\": \"def\"}");
      arangodb::iresearch::IResearchLinkMeta linkMeta;
      arangodb::transaction::Methods trx(
        arangodb::transaction::StandaloneContext::Create(vocbase),
        EMPTY, EMPTY, EMPTY,
        arangodb::transaction::Options());

      std::vector<std::pair<arangodb::LocalDocumentId, arangodb::velocypack::Slice>> batch = {
        { arangodb::LocalDocumentId(1), docJson->slice() },
        { arangodb::LocalDocumentId(2), docJson->slice() },
      };

      linkMeta._includeAllFields = true;
      EXPECT_TRUE((trx.begin().ok()));
      // insert operations before recovery tick
      StorageEngineMock::recoveryTickResult = 41;
      for (auto const& pair : batch) {
        link->insert(trx, pair.first, pair.second);
      }
      StorageEngineMock::recoveryTickResult = 42;
      for (auto const& pair : batch) {
        link->insert(trx, pair.first, pair.second);
      }
      // insert operations after recovery tick
      StorageEngineMock::recoveryTickResult = 43;
      for (auto const& pair : batch) {
        link->insert(trx, pair.first, pair.second);
      }
      EXPECT_TRUE((trx.commit().ok()));
      EXPECT_TRUE(link->commit().ok());
    }

    arangodb::transaction::Methods trx(
      arangodb::transaction::StandaloneContext::Create(vocbase),
      EMPTY, EMPTY, EMPTY,
      arangodb::transaction::Options());

    auto* snapshot = view->snapshot(trx, arangodb::iresearch::IResearchView::SnapshotMode::FindOrCreate);
    EXPECT_EQ(2, snapshot->live_docs_count());
  }

  // not in recovery (FindOrCreate)
  {
    StorageEngineMock::recoveryStateResult = arangodb::RecoveryState::DONE;
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    ASSERT_TRUE((false == !logicalCollection));
    auto viewImpl = vocbase.createView(viewJson->slice());
    EXPECT_TRUE((false == !viewImpl));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(viewImpl.get());
    EXPECT_TRUE((nullptr != view));
    auto index = StorageEngineMock::buildLinkMock(arangodb::IndexId{42}, *logicalCollection, linkJson->slice());
    ASSERT_NE(nullptr, index);
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
      EXPECT_TRUE((link->insert(trx, arangodb::LocalDocumentId(1), docJson->slice()).ok()));
      EXPECT_TRUE((link->insert(trx, arangodb::LocalDocumentId(2), docJson->slice()).ok()));
      EXPECT_TRUE((link->insert(trx, arangodb::LocalDocumentId(1), docJson->slice()).ok())); // 2nd time
      EXPECT_TRUE((link->insert(trx, arangodb::LocalDocumentId(2), docJson->slice()).ok())); // 2nd time
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
    StorageEngineMock::recoveryStateResult = arangodb::RecoveryState::DONE;
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    ASSERT_TRUE((false == !logicalCollection));
    auto viewImpl = vocbase.createView(viewJson->slice());
    EXPECT_TRUE((false == !viewImpl));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(viewImpl.get());
    EXPECT_TRUE((nullptr != view));
    EXPECT_TRUE(view->category() == arangodb::LogicalView::category());
    auto index = StorageEngineMock::buildLinkMock(arangodb::IndexId{42}, *logicalCollection, linkJson->slice());
    ASSERT_NE(nullptr, index);
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
      EXPECT_TRUE((link->insert(trx, arangodb::LocalDocumentId(1), docJson->slice()).ok()));
      EXPECT_TRUE((link->insert(trx, arangodb::LocalDocumentId(2), docJson->slice()).ok()));
      EXPECT_TRUE((link->insert(trx, arangodb::LocalDocumentId(1), docJson->slice()).ok())); // 2nd time
      EXPECT_TRUE((link->insert(trx, arangodb::LocalDocumentId(2), docJson->slice()).ok())); // 2nd time
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
    StorageEngineMock::recoveryStateResult = arangodb::RecoveryState::DONE;
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    ASSERT_TRUE((false == !logicalCollection));
    auto viewImpl = vocbase.createView(viewJson->slice());
    EXPECT_TRUE((false == !viewImpl));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(viewImpl.get());
    EXPECT_TRUE((nullptr != view));
    EXPECT_TRUE(view->category() == arangodb::LogicalView::category());
    auto index = StorageEngineMock::buildLinkMock(arangodb::IndexId{42}, *logicalCollection, linkJson->slice());
    ASSERT_NE(nullptr, index);
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
      EXPECT_TRUE((link->insert(trx, arangodb::LocalDocumentId(1), docJson->slice()).ok()));
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
    StorageEngineMock::recoveryStateResult = arangodb::RecoveryState::DONE;
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    ASSERT_TRUE((false == !logicalCollection));
    auto viewImpl = vocbase.createView(viewJson->slice());
    EXPECT_TRUE((false == !viewImpl));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(viewImpl.get());
    EXPECT_TRUE((nullptr != view));
    auto index = StorageEngineMock::buildLinkMock(arangodb::IndexId{42}, *logicalCollection, linkJson->slice());
    ASSERT_NE(nullptr, index);
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
      std::vector<std::pair<arangodb::LocalDocumentId, arangodb::velocypack::Slice>> batch = {
        { arangodb::LocalDocumentId(1), docJson->slice() },
        { arangodb::LocalDocumentId(2), docJson->slice() },
      };

      linkMeta._includeAllFields = true;
      EXPECT_TRUE((trx.begin().ok()));
      for (auto const& pair : batch) {
        link->insert(trx, pair.first, pair.second);
      }
      for (auto const& pair : batch) {
        link->insert(trx, pair.first, pair.second);
      }  // 2nd time
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
    StorageEngineMock::recoveryStateResult = arangodb::RecoveryState::DONE;
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    ASSERT_TRUE((false == !logicalCollection));
    auto viewImpl = vocbase.createView(viewJson->slice());
    EXPECT_TRUE((false == !viewImpl));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(viewImpl.get());
    EXPECT_TRUE((nullptr != view));
    auto index = StorageEngineMock::buildLinkMock(arangodb::IndexId{42}, *logicalCollection, linkJson->slice());
    ASSERT_NE(nullptr, index);
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
      std::vector<std::pair<arangodb::LocalDocumentId, arangodb::velocypack::Slice>> batch = {
        { arangodb::LocalDocumentId(1), docJson->slice() },
        { arangodb::LocalDocumentId(2), docJson->slice() },
      };

      linkMeta._includeAllFields = true;
      EXPECT_TRUE((trx.begin().ok()));
      for (auto const& pair : batch) {
        link->insert(trx, pair.first, pair.second);
      }
      for (auto const& pair : batch) {
        link->insert(trx, pair.first, pair.second);
      }  // 2nd time
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

TEST_F(IResearchViewTest, test_remove) {
  static std::vector<std::string> const EMPTY;
  auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
  auto linkJson = arangodb::velocypack::Parser::fromJson("{ \"view\": \"testView\", \"includeAllFields\": true }");
  auto viewJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\":\"arangosearch\" }");
  arangodb::aql::AstNode noop(arangodb::aql::AstNodeType::NODE_TYPE_FILTER);
  arangodb::aql::AstNode noopChild(arangodb::aql::AstNodeValue(true));

  noop.addMember(&noopChild);

  // in recovery (skip operations before or at recovery tick)
  {
    auto before = StorageEngineMock::recoveryStateResult;
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    ASSERT_NE(nullptr, logicalCollection);
    auto viewImpl = vocbase.createView(viewJson->slice());
    ASSERT_NE(nullptr, viewImpl);
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(viewImpl.get());
    ASSERT_NE(nullptr, view);
    StorageEngineMock::recoveryTickResult = 42;
    StorageEngineMock::recoveryStateResult = arangodb::RecoveryState::DONE;
    StorageEngineMock::recoveryTickCallback = []() {
      StorageEngineMock::recoveryStateResult = arangodb::RecoveryState::IN_PROGRESS;
    };
    auto index = StorageEngineMock::buildLinkMock(arangodb::IndexId{42}, *logicalCollection, linkJson->slice());
    StorageEngineMock::recoveryTickCallback = []() {};
    auto restore = irs::make_finally([&before]()->void {
      StorageEngineMock::recoveryStateResult = before;
      StorageEngineMock::recoveryTickResult = 0;
    });

    ASSERT_NE(nullptr, index);
    auto link = std::dynamic_pointer_cast<arangodb::iresearch::IResearchLink>(index);
    ASSERT_NE(nullptr, link);

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

      // insert operations after recovery tick
      StorageEngineMock::recoveryTickResult = 43;
      EXPECT_TRUE(link->insert(trx, arangodb::LocalDocumentId(1), docJson->slice()).ok());
      EXPECT_TRUE(link->insert(trx, arangodb::LocalDocumentId(2), docJson->slice()).ok());
      EXPECT_TRUE(link->insert(trx, arangodb::LocalDocumentId(3), docJson->slice()).ok());

      // skip tick operations before recovery tick
      StorageEngineMock::recoveryTickResult = 41;
      EXPECT_TRUE(link->remove(trx, arangodb::LocalDocumentId(1), VPackSlice::noneSlice()).ok());
      StorageEngineMock::recoveryTickResult = 42;
      EXPECT_TRUE(link->insert(trx, arangodb::LocalDocumentId(2), VPackSlice::noneSlice()).ok());

      // apply remove after recovery tick
      StorageEngineMock::recoveryTickResult = 43;
      EXPECT_TRUE(link->remove(trx, arangodb::LocalDocumentId(3), VPackSlice::noneSlice()).ok());

      EXPECT_TRUE(trx.commit().ok());
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
    EXPECT_EQ(2, snapshot->live_docs_count());
  }

  // in recovery batch (skip operations before or at recovery tick)
  {
    auto before = StorageEngineMock::recoveryStateResult;
    StorageEngineMock::recoveryStateResult = arangodb::RecoveryState::IN_PROGRESS;
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    ASSERT_TRUE((false == !logicalCollection));
    auto viewImpl = vocbase.createView(viewJson->slice());
    EXPECT_TRUE((false == !viewImpl));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(viewImpl.get());
    EXPECT_TRUE((nullptr != view));

    StorageEngineMock::recoveryTickResult = 42;
    StorageEngineMock::recoveryStateResult = arangodb::RecoveryState::DONE;
    StorageEngineMock::recoveryTickCallback = []() {
      StorageEngineMock::recoveryStateResult = arangodb::RecoveryState::IN_PROGRESS;
    };
    auto index = StorageEngineMock::buildLinkMock(arangodb::IndexId{42}, *logicalCollection, linkJson->slice());
    StorageEngineMock::recoveryTickCallback = []() {};
    auto restore = irs::make_finally([&before]()->void {
      StorageEngineMock::recoveryStateResult = before;
      StorageEngineMock::recoveryTickResult = 0;
    });

    ASSERT_NE(nullptr, index);
    auto link = std::dynamic_pointer_cast<arangodb::iresearch::IResearchLink>(index);
    ASSERT_NE(nullptr, link);

    {
      auto docJson = arangodb::velocypack::Parser::fromJson("{\"abc\": \"def\"}");
      arangodb::iresearch::IResearchLinkMeta linkMeta;
      arangodb::transaction::Methods trx(
        arangodb::transaction::StandaloneContext::Create(vocbase),
        EMPTY, EMPTY, EMPTY,
        arangodb::transaction::Options());

      std::vector<std::pair<arangodb::LocalDocumentId, arangodb::velocypack::Slice>> batch = {
        { arangodb::LocalDocumentId(1), docJson->slice() },
        { arangodb::LocalDocumentId(2), docJson->slice() },
      };

      linkMeta._includeAllFields = true;
      EXPECT_TRUE((trx.begin().ok()));
      // insert operations before recovery tick
      StorageEngineMock::recoveryTickResult = 41;
      for (auto const& pair : batch) {
        link->insert(trx, pair.first, pair.second);
      }
      StorageEngineMock::recoveryTickResult = 42;
      for (auto const& pair : batch) {
        link->insert(trx, pair.first, pair.second);
      }
      // insert operations after recovery tick
      StorageEngineMock::recoveryTickResult = 43;
      for (auto const& pair : batch) {
        link->insert(trx, pair.first, pair.second);
      }
      EXPECT_TRUE((trx.commit().ok()));
      EXPECT_TRUE(link->commit().ok());
    }

    arangodb::transaction::Methods trx(
      arangodb::transaction::StandaloneContext::Create(vocbase),
      EMPTY, EMPTY, EMPTY,
      arangodb::transaction::Options());

    auto* snapshot = view->snapshot(trx, arangodb::iresearch::IResearchView::SnapshotMode::FindOrCreate);
    EXPECT_EQ(2, snapshot->live_docs_count());
  }

  // not in recovery (FindOrCreate)
  {
    StorageEngineMock::recoveryStateResult = arangodb::RecoveryState::DONE;
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    ASSERT_TRUE((false == !logicalCollection));
    auto viewImpl = vocbase.createView(viewJson->slice());
    EXPECT_TRUE((false == !viewImpl));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(viewImpl.get());
    EXPECT_TRUE((nullptr != view));
    auto index = StorageEngineMock::buildLinkMock(arangodb::IndexId{42}, *logicalCollection, linkJson->slice());
    ASSERT_NE(nullptr, index);
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
      EXPECT_TRUE((link->insert(trx, arangodb::LocalDocumentId(1), docJson->slice()).ok()));
      EXPECT_TRUE((link->insert(trx, arangodb::LocalDocumentId(2), docJson->slice()).ok()));
      EXPECT_TRUE((link->insert(trx, arangodb::LocalDocumentId(1), docJson->slice()).ok())); // 2nd time
      EXPECT_TRUE((link->insert(trx, arangodb::LocalDocumentId(2), docJson->slice()).ok())); // 2nd time
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
    StorageEngineMock::recoveryStateResult = arangodb::RecoveryState::DONE;
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    ASSERT_TRUE((false == !logicalCollection));
    auto viewImpl = vocbase.createView(viewJson->slice());
    EXPECT_TRUE((false == !viewImpl));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(viewImpl.get());
    EXPECT_TRUE((nullptr != view));
    EXPECT_TRUE(view->category() == arangodb::LogicalView::category());
    auto index = StorageEngineMock::buildLinkMock(arangodb::IndexId{42}, *logicalCollection, linkJson->slice());
    ASSERT_NE(nullptr, index);
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
      EXPECT_TRUE((link->insert(trx, arangodb::LocalDocumentId(1), docJson->slice()).ok()));
      EXPECT_TRUE((link->insert(trx, arangodb::LocalDocumentId(2), docJson->slice()).ok()));
      EXPECT_TRUE((link->insert(trx, arangodb::LocalDocumentId(1), docJson->slice()).ok())); // 2nd time
      EXPECT_TRUE((link->insert(trx, arangodb::LocalDocumentId(2), docJson->slice()).ok())); // 2nd time
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
    StorageEngineMock::recoveryStateResult = arangodb::RecoveryState::DONE;
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    ASSERT_TRUE((false == !logicalCollection));
    auto viewImpl = vocbase.createView(viewJson->slice());
    EXPECT_TRUE((false == !viewImpl));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(viewImpl.get());
    EXPECT_TRUE((nullptr != view));
    EXPECT_TRUE(view->category() == arangodb::LogicalView::category());
    auto index = StorageEngineMock::buildLinkMock(arangodb::IndexId{42}, *logicalCollection, linkJson->slice());
    ASSERT_NE(nullptr, index);
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
      EXPECT_TRUE((link->insert(trx, arangodb::LocalDocumentId(1), docJson->slice()).ok()));
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
    StorageEngineMock::recoveryStateResult = arangodb::RecoveryState::DONE;
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    ASSERT_TRUE((false == !logicalCollection));
    auto viewImpl = vocbase.createView(viewJson->slice());
    EXPECT_TRUE((false == !viewImpl));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(viewImpl.get());
    EXPECT_TRUE((nullptr != view));
    auto index = StorageEngineMock::buildLinkMock(arangodb::IndexId{42}, *logicalCollection, linkJson->slice());
    ASSERT_NE(nullptr, index);
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
      std::vector<std::pair<arangodb::LocalDocumentId, arangodb::velocypack::Slice>> batch = {
        { arangodb::LocalDocumentId(1), docJson->slice() },
        { arangodb::LocalDocumentId(2), docJson->slice() },
      };

      linkMeta._includeAllFields = true;
      EXPECT_TRUE((trx.begin().ok()));
      for (auto const& pair : batch) {
        link->insert(trx, pair.first, pair.second);
      }
      for (auto const& pair : batch) {
        link->insert(trx, pair.first, pair.second);
      }  // 2nd time
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
    StorageEngineMock::recoveryStateResult = arangodb::RecoveryState::DONE;
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    ASSERT_TRUE((false == !logicalCollection));
    auto viewImpl = vocbase.createView(viewJson->slice());
    EXPECT_TRUE((false == !viewImpl));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(viewImpl.get());
    EXPECT_TRUE((nullptr != view));
    auto index = StorageEngineMock::buildLinkMock(arangodb::IndexId{42}, *logicalCollection, linkJson->slice());
    ASSERT_NE(nullptr, index);
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
      std::vector<std::pair<arangodb::LocalDocumentId, arangodb::velocypack::Slice>> batch = {
        { arangodb::LocalDocumentId(1), docJson->slice() },
        { arangodb::LocalDocumentId(2), docJson->slice() },
      };

      linkMeta._includeAllFields = true;
      EXPECT_TRUE((trx.begin().ok()));
      for (auto const& pair : batch) {
        link->insert(trx, pair.first, pair.second);
      }
      for (auto const& pair : batch) {
        link->insert(trx, pair.first, pair.second);
      }  // 2nd time
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
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
    std::string dataPath = ((((irs::utf8_path()/=testFilesystemPath)/=std::string("databases"))/=(std::string("database-") + std::to_string(vocbase.id())))/=std::string("arangosearch-123")).utf8();
    auto json = arangodb::velocypack::Parser::fromJson("{ \"id\": 123, \"name\": \"testView\", \"type\": \"testType\" }");

    EXPECT_TRUE((false == TRI_IsDirectory(dataPath.c_str())));
    arangodb::LogicalView::ptr view;
    ASSERT_TRUE((arangodb::iresearch::IResearchView::factory().instantiate(view, vocbase, json->slice()).ok()));
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
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
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
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    ASSERT_TRUE((false == !logicalCollection));
    auto logicalView = vocbase.createView(createJson->slice());
    EXPECT_TRUE((false == !logicalView));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
    EXPECT_TRUE((false == !view));
    auto index = StorageEngineMock::buildLinkMock(arangodb::IndexId{42}, *logicalCollection, linkJson->slice());
    ASSERT_NE(nullptr, index);
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
        EXPECT_TRUE((link->insert(trx, arangodb::LocalDocumentId(i), doc->slice()).ok()));
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

    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
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
        logicalCollection->insert(&trx, doc->slice(), inserted, options);
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
        logicalCollection->insert(&trx, doc->slice(), inserted, options);
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
    ASSERT_TRUE(server.server().hasFeature<arangodb::FlushFeature>());
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    auto logicalView = vocbase.createView(viewCreateJson->slice());
    ASSERT_TRUE((false == !logicalView));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
    ASSERT_TRUE((false == !view));
    arangodb::Result res = logicalView->properties(viewUpdateJson->slice(), true);
    ASSERT_TRUE(true == res.ok());

    static std::vector<std::string> const EMPTY;
    arangodb::transaction::Options options;

    arangodb::aql::Variable variable("testVariable", 0, false);

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
    auto& engine = *static_cast<StorageEngineMock*>(
        &server.getFeature<arangodb::EngineSelectorFeature>().engine());
    engine.views.clear();
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    ASSERT_TRUE((false == !logicalCollection));
    auto logicalView = vocbase.createView(viewJson0->slice());
    ASSERT_TRUE((false == !logicalView));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
    ASSERT_TRUE((false == !view));

    {
      arangodb::velocypack::Builder builder;
      builder.openObject();
      view->properties(builder, arangodb::LogicalDataSource::Serialization::List);
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
      std::set<arangodb::DataSourceId> cids;
      view->visitCollections([&cids](arangodb::DataSourceId cid) -> bool {
        cids.emplace(cid);
        return true;
      });
      EXPECT_TRUE((0 == cids.size()));
    }

    auto before = StorageEngineMock::recoveryStateResult;
    StorageEngineMock::recoveryStateResult = arangodb::RecoveryState::IN_PROGRESS;
    auto restore = irs::make_finally([&before]()->void { StorageEngineMock::recoveryStateResult = before; });
    persisted = false;
    
    auto link = StorageEngineMock::buildLinkMock(arangodb::IndexId{1}, *logicalCollection, linkJson->slice());
    ASSERT_NE(nullptr, link);
    EXPECT_TRUE(persisted);
    EXPECT_NE(nullptr, link);

    // link addition does modify view meta
    {
      std::unordered_set<arangodb::DataSourceId> expected = {arangodb::DataSourceId{100}};
      std::set<arangodb::DataSourceId> actual;
      view->visitCollections([&actual](arangodb::DataSourceId cid) -> bool {
        actual.emplace(cid);
        return true;
      });

      for (auto& cid: expected) {
        EXPECT_TRUE((1 == actual.erase(cid)));
      }

      EXPECT_TRUE((actual.empty()));
    }
  }

  std::vector<std::string> EMPTY;

  // new link
  {
    auto& engine = *static_cast<StorageEngineMock*>(
        &server.getFeature<arangodb::EngineSelectorFeature>().engine());
    engine.views.clear();
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    ASSERT_TRUE((false == !logicalCollection));
    auto logicalView = vocbase.createView(viewJson0->slice());
    ASSERT_TRUE((false == !logicalView));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
    ASSERT_TRUE((false == !view));

    {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      view->properties(builder, arangodb::LogicalDataSource::Serialization::List);
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
      std::unordered_set<arangodb::DataSourceId> cids;
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
      std::set<arangodb::DataSourceId> actual;
      view->visitCollections([&actual](arangodb::DataSourceId cid) -> bool {
        actual.emplace(cid);
        return true;
      });
      EXPECT_TRUE((actual.empty()));
    }

    persisted = false;
    auto link = StorageEngineMock::buildLinkMock(arangodb::IndexId{1}, *logicalCollection, linkJson->slice());
    ASSERT_NE(nullptr, link);
    EXPECT_TRUE(persisted); // link instantiation does modify and persist view meta
    std::unordered_set<arangodb::DataSourceId> cids;
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
      std::unordered_set<arangodb::DataSourceId> expected = {arangodb::DataSourceId{100}};
      std::set<arangodb::DataSourceId> actual;
      view->visitCollections([&actual](arangodb::DataSourceId cid) -> bool {
        actual.emplace(cid);
        return true;
      });

      for (auto& cid: expected) {
        EXPECT_TRUE((1 == actual.erase(cid)));
      }

      EXPECT_TRUE((actual.empty()));
    }
  }

  // known link
  {
    auto& engine = *static_cast<StorageEngineMock*>(
        &server.getFeature<arangodb::EngineSelectorFeature>().engine());
    engine.views.clear();
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    ASSERT_TRUE((false == !logicalCollection));
    auto logicalView = vocbase.createView(viewJson1->slice());
    ASSERT_TRUE((false == !logicalView));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
    ASSERT_TRUE((false == !view));

    {
      std::unordered_set<arangodb::DataSourceId> cids;
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
      std::unordered_set<arangodb::DataSourceId> expected = {arangodb::DataSourceId{100},
                                                             arangodb::DataSourceId{123}};
      std::set<arangodb::DataSourceId> actual = {arangodb::DataSourceId{123}};
      view->visitCollections([&actual](arangodb::DataSourceId cid) -> bool {
        actual.emplace(cid);
        return true;
      });

      for (auto& cid: expected) {
        EXPECT_TRUE((1 == actual.erase(cid)));
      }

      EXPECT_TRUE((actual.empty()));
    }

    persisted = false;
    auto link0 = StorageEngineMock::buildLinkMock(arangodb::IndexId{1}, *logicalCollection, linkJson->slice());
    EXPECT_FALSE(persisted);
    EXPECT_NE(nullptr, link0);

    {
      std::unordered_set<arangodb::DataSourceId> cids;
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
      std::unordered_set<arangodb::DataSourceId> expected = {arangodb::DataSourceId{100},
                                                             arangodb::DataSourceId{123}};
      std::set<arangodb::DataSourceId> actual = {arangodb::DataSourceId{123}};
      view->visitCollections([&actual](arangodb::DataSourceId cid) -> bool {
        actual.emplace(cid);
        return true;
      });

      for (auto& cid: expected) {
        EXPECT_TRUE((1 == actual.erase(cid)));
      }

      EXPECT_TRUE((actual.empty()));
    }

    persisted = false;
    std::shared_ptr<arangodb::Index> link1;
    try {
      link1 = StorageEngineMock::buildLinkMock(arangodb::IndexId{1}, *logicalCollection, linkJson->slice());
      EXPECT_EQ(nullptr, link1);
    } catch (std::exception const&) {
      // ignore any errors here
    }
    link0.reset(); // unload link before creating a new link instance
    link1 = StorageEngineMock::buildLinkMock(arangodb::IndexId{1}, *logicalCollection, linkJson->slice());
    EXPECT_FALSE(persisted);
    EXPECT_NE(nullptr, link1); // duplicate link creation is allowed
    std::unordered_set<arangodb::DataSourceId> cids;
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
      std::unordered_set<arangodb::DataSourceId> expected = {arangodb::DataSourceId{100},
                                                             arangodb::DataSourceId{123}};
      std::set<arangodb::DataSourceId> actual = {arangodb::DataSourceId{123}};
      view->visitCollections([&actual](arangodb::DataSourceId cid) -> bool {
        actual.emplace(cid);
        return true;
      });

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
    auto& engine = *static_cast<StorageEngineMock*>(
        &server.getFeature<arangodb::EngineSelectorFeature>().engine());
    engine.views.clear();
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    ASSERT_TRUE((false == !logicalCollection));
    auto logicalView = vocbase.createView(viewJson->slice());
    ASSERT_TRUE((false == !logicalView));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
    ASSERT_TRUE((false == !view));
    auto index = StorageEngineMock::buildLinkMock(arangodb::IndexId{__LINE__}, *logicalCollection, linkJson->slice());
    ASSERT_NE(nullptr, index);
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
      EXPECT_TRUE((link->insert(trx, arangodb::LocalDocumentId(0), doc->slice()).ok()));
      EXPECT_TRUE((trx.commit().ok()));
      EXPECT_TRUE((link->commit().ok()));
    }

    auto links = arangodb::velocypack::Parser::fromJson(
        "{ \"links\": { \"testCollection\": { \"id\": " +
        std::to_string(link->id().id()) + " } } }"  // same link ID
    );

    link->unload(); // unload link before creating a new link instance
    arangodb::Result res = logicalView->properties(links->slice(), true);
    EXPECT_TRUE(true == res.ok());
    EXPECT_TRUE((false == logicalCollection->getIndexes().empty()));

    {
      std::unordered_set<arangodb::DataSourceId> cids;
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
      std::unordered_set<arangodb::DataSourceId> expected = {arangodb::DataSourceId{100}};
      std::set<arangodb::DataSourceId> actual = {};
      view->visitCollections([&actual](arangodb::DataSourceId cid) -> bool {
        actual.emplace(cid);
        return true;
      });

      for (auto& cid: expected) {
        EXPECT_TRUE((1 == actual.erase(cid)));
      }

      EXPECT_TRUE((actual.empty()));
    }

    EXPECT_TRUE((nullptr != vocbase.lookupCollection("testCollection")));

    auto before = StorageEngineMock::recoveryStateResult;
    StorageEngineMock::recoveryStateResult = arangodb::RecoveryState::IN_PROGRESS;
    auto restore = irs::make_finally([&before]()->void { StorageEngineMock::recoveryStateResult = before; });
    persisted = false;
    EXPECT_TRUE((true == vocbase.dropCollection(logicalCollection->id(), true, -1).ok()));
    EXPECT_TRUE((false == persisted)); // link removal does not persist view meta
    EXPECT_TRUE((nullptr == vocbase.lookupCollection("testCollection")));

    {
      std::unordered_set<arangodb::DataSourceId> cids;
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
      std::set<arangodb::DataSourceId> actual;
      view->visitCollections([&actual](arangodb::DataSourceId cid) -> bool {
        actual.emplace(cid);
        return true;
      });
      EXPECT_TRUE((actual.empty())); // collection removal does modify view meta
    }

    EXPECT_TRUE((false == !vocbase.lookupView("testView")));
    EXPECT_TRUE((true == vocbase.dropView(view->id(), false).ok()));
    EXPECT_TRUE((true == !vocbase.lookupView("testView")));
  }

  // link removed before view
  {
    auto& engine = *static_cast<StorageEngineMock*>(
        &server.getFeature<arangodb::EngineSelectorFeature>().engine());
    engine.views.clear();
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    ASSERT_TRUE((false == !logicalCollection));
    auto logicalView = vocbase.createView(viewJson->slice());
    ASSERT_TRUE((false == !logicalView));
    auto* view = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
    ASSERT_TRUE((false == !view));
    auto index = StorageEngineMock::buildLinkMock(arangodb::IndexId{__LINE__}, *logicalCollection, linkJson->slice());
    ASSERT_NE(nullptr, index);
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
      EXPECT_TRUE((link->insert(trx, arangodb::LocalDocumentId(0), doc->slice()).ok()));
      EXPECT_TRUE((trx.commit().ok()));
      EXPECT_TRUE((link->commit().ok()));
    }

    auto links = arangodb::velocypack::Parser::fromJson(
        "{ \"links\": { \"testCollection\": {\"id\": " +
        std::to_string(link->id().id()) + " } } }"  // same link ID
    );

    link->unload(); // unload link before creating a new link instance
    arangodb::Result res = logicalView->properties(links->slice(), true);
    EXPECT_TRUE(true == res.ok());
    EXPECT_TRUE((false == logicalCollection->getIndexes().empty()));

    {
      std::unordered_set<arangodb::DataSourceId> cids;
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
      std::unordered_set<arangodb::DataSourceId> expected = {arangodb::DataSourceId{100}};
      std::set<arangodb::DataSourceId> actual;
      view->visitCollections([&actual](arangodb::DataSourceId cid) -> bool {
        actual.emplace(cid);
        return true;
      });

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
      std::unordered_set<arangodb::DataSourceId> cids;
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
      std::set<arangodb::DataSourceId> actual;
      view->visitCollections([&actual](arangodb::DataSourceId cid) -> bool {
        actual.emplace(cid);
        return true;
      });
      EXPECT_TRUE((actual.empty())); // collection removal does modify view meta
    }

    EXPECT_TRUE((false == !vocbase.lookupView("testView")));
    EXPECT_TRUE((true == vocbase.dropView(view->id(), false).ok()));
    EXPECT_TRUE((true == !vocbase.lookupView("testView")));
  }

  // view removed before link
  {
    auto& engine = *static_cast<StorageEngineMock*>(
        &server.getFeature<arangodb::EngineSelectorFeature>().engine());
    engine.views.clear();
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
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

    std::set<arangodb::DataSourceId> cids;
    view->visitCollections([&cids](arangodb::DataSourceId cid) -> bool {
      cids.emplace(cid);
      return true;
    });
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
    auto& engine = *static_cast<StorageEngineMock*>(
        &server.getFeature<arangodb::EngineSelectorFeature>().engine());
    engine.views.clear();
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
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
      std::set<arangodb::DataSourceId> cids;
      viewImpl->visitCollections([&cids](arangodb::DataSourceId cid) -> bool {
        cids.emplace(cid);
        return true;
      });
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
      ASSERT_TRUE((arangodb::iresearch::IResearchView::factory().instantiate(view, vocbase, viewJson->slice()).ok()));
      ASSERT_TRUE((false == !view));
      auto* viewImpl = dynamic_cast<arangodb::iresearch::IResearchView*>(view.get());
      ASSERT_TRUE((nullptr != viewImpl));
      std::set<arangodb::DataSourceId> cids;
      viewImpl->visitCollections([&cids](arangodb::DataSourceId cid) -> bool {
        cids.emplace(cid);
        return true;
      });
      EXPECT_TRUE((0 == cids.size()));

      for (auto& index: logicalCollection->getIndexes()) {
        auto* link = dynamic_cast<arangodb::iresearch::IResearchLink*>(index.get());
        ASSERT_NE(nullptr, link);
        auto lock = link->self()->lock();
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
    auto& engine = *static_cast<StorageEngineMock*>(
        &server.getFeature<arangodb::EngineSelectorFeature>().engine());
    engine.views.clear();
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
    arangodb::LogicalView::ptr view;
    EXPECT_TRUE((arangodb::iresearch::IResearchView::factory().create(view, vocbase, viewJson->slice()).ok()));
    EXPECT_TRUE((nullptr != view));
    auto* viewImpl = dynamic_cast<arangodb::iresearch::IResearchView*>(view.get());
    ASSERT_TRUE((nullptr != viewImpl));

    std::set<arangodb::DataSourceId> actual;
    viewImpl->visitCollections([&actual](arangodb::DataSourceId cid) -> bool {
      actual.emplace(cid);
      return true;
    });
    EXPECT_TRUE((actual.empty()));
  }

  // test add via link before open (TRI_vocbase_t::createView(...) will call open())
  {
    auto& engine = *static_cast<StorageEngineMock*>(
        &server.getFeature<arangodb::EngineSelectorFeature>().engine());
    engine.views.clear();
    auto updateJson = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection\": { } } }");
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    ASSERT_TRUE((nullptr != logicalCollection));
    arangodb::LogicalView::ptr logicalView;
    ASSERT_TRUE((arangodb::iresearch::IResearchView::factory().instantiate(logicalView, vocbase, viewJson->slice()).ok()));
    ASSERT_TRUE((false == !logicalView));
    engine.createView(vocbase, logicalView->id(), *logicalView); // ensure link can find view
    StorageEngineMock(server.server()).registerView(vocbase, logicalView);  // ensure link can find view
    auto* viewImpl = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
    ASSERT_TRUE((nullptr != viewImpl));

    EXPECT_TRUE((viewImpl->properties(updateJson->slice(), false).ok()));

    std::set<arangodb::DataSourceId> actual;
    std::set<arangodb::DataSourceId> expected = {arangodb::DataSourceId{100}};
    viewImpl->visitCollections([&actual](arangodb::DataSourceId cid) -> bool {
      actual.emplace(cid);
      return true;
    });

    for (auto& cid: actual) {
      EXPECT_TRUE((1 == expected.erase(cid)));
    }

    EXPECT_TRUE((expected.empty()));
    logicalCollection->getIndexes()[0]->unload(); // release view reference to prevent deadlock due to ~IResearchView() waiting for IResearchLink::unload()
  }

  // test drop via link before open (TRI_vocbase_t::createView(...) will call open())
  {
    auto& engine = *static_cast<StorageEngineMock*>(
        &server.getFeature<arangodb::EngineSelectorFeature>().engine());
    engine.views.clear();
    auto updateJson0 = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection\": { } } }");
    auto updateJson1 = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection\": null } }");
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    ASSERT_TRUE((nullptr != logicalCollection));
    arangodb::LogicalView::ptr logicalView;
    ASSERT_TRUE((arangodb::iresearch::IResearchView::factory().instantiate(logicalView, vocbase, viewJson->slice()).ok()));
    ASSERT_TRUE((false == !logicalView));
    engine.createView(vocbase, logicalView->id(), *logicalView); // ensure link can find view
    StorageEngineMock(server.server()).registerView(vocbase, logicalView);  // ensure link can find view
    auto* viewImpl = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
    ASSERT_TRUE((nullptr != viewImpl));

    // create link
    {
      EXPECT_TRUE((viewImpl->properties(updateJson0->slice(), false).ok()));

      std::set<arangodb::DataSourceId> actual;
      std::set<arangodb::DataSourceId> expected = {arangodb::DataSourceId{100}};
      viewImpl->visitCollections([&actual](arangodb::DataSourceId cid) -> bool {
        actual.emplace(cid);
        return true;
      });

      for (auto& cid: actual) {
        EXPECT_TRUE((1 == expected.erase(cid)));
      }

      EXPECT_TRUE((expected.empty()));
    }

    // drop link
    {
      EXPECT_TRUE((viewImpl->properties(updateJson1->slice(), false).ok()));

      std::set<arangodb::DataSourceId> actual;
      viewImpl->visitCollections([&actual](arangodb::DataSourceId cid) -> bool {
        actual.emplace(cid);
        return true;
      });
      EXPECT_TRUE((actual.empty()));
    }
  }

  // test load persisted CIDs on open (TRI_vocbase_t::createView(...) will call open())
  // use separate view ID for this test since doing open from persisted store
  {// initial populate persisted view
   {auto& engine = *static_cast<StorageEngineMock*>(
        &server.getFeature<arangodb::EngineSelectorFeature>().engine());
  engine.views.clear();
  auto collectionJson = arangodb::velocypack::Parser::fromJson(
      "{ \"name\": \"testCollection\" }");
  auto linkJson =
      arangodb::velocypack::Parser::fromJson("{ \"view\": \"testView\" }");
  auto createJson = arangodb::velocypack::Parser::fromJson(
      "{ \"name\": \"testView\", \"type\": \"arangosearch\", \"id\": 102 }");
  ASSERT_TRUE(server.server().hasFeature<arangodb::FlushFeature>());
  Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
  auto logicalCollection = vocbase.createCollection(collectionJson->slice());
  ASSERT_TRUE((false == !logicalCollection));
  auto logicalView = vocbase.createView(createJson->slice());
  ASSERT_TRUE((false == !logicalView));
  auto* viewImpl = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
  ASSERT_TRUE((nullptr != viewImpl));
  auto index = StorageEngineMock::buildLinkMock(arangodb::IndexId{42}, *logicalCollection, linkJson->slice());
  ASSERT_NE(nullptr, index);
  auto link = std::dynamic_pointer_cast<arangodb::iresearch::IResearchLink>(index);
  ASSERT_TRUE((false == !link));

  static std::vector<std::string> const EMPTY;
  auto doc = arangodb::velocypack::Parser::fromJson("{ \"key\": 1 }");
  arangodb::iresearch::IResearchLinkMeta meta;
  meta._includeAllFields = true;
  arangodb::transaction::Methods trx(arangodb::transaction::StandaloneContext::Create(vocbase),
                                     EMPTY, EMPTY, EMPTY,
                                     arangodb::transaction::Options());
  EXPECT_TRUE((trx.begin().ok()));
  EXPECT_TRUE((link->insert(trx, arangodb::LocalDocumentId(0), doc->slice()).ok()));
  EXPECT_TRUE((trx.commit().ok()));
  EXPECT_TRUE((link->commit().ok()));  // commit to persisted store
    }

    // test persisted CIDs on open
    {
      auto& engine = *static_cast<StorageEngineMock*>(
          &server.getFeature<arangodb::EngineSelectorFeature>().engine());
      engine.views.clear();
      auto createJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\", \"id\": 102 }");
      Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
      auto logicalView = vocbase.createView(createJson->slice());
      ASSERT_TRUE((false == !logicalView));
      auto* viewImpl = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
      ASSERT_TRUE((nullptr != viewImpl));

      std::set<arangodb::DataSourceId> actual;
      viewImpl->visitCollections([&actual](arangodb::DataSourceId cid) -> bool {
        actual.emplace(cid);
        return true;
      });
      EXPECT_TRUE((actual.empty())); // persisted cids do not modify view meta
    }
  }

  // test add via link after open (TRI_vocbase_t::createView(...) will call open())
  {
    auto& engine = *static_cast<StorageEngineMock*>(
        &server.getFeature<arangodb::EngineSelectorFeature>().engine());
    engine.views.clear();
    auto updateJson = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection\": { } } }");
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    ASSERT_TRUE((nullptr != logicalCollection));
    auto logicalView = vocbase.createView(viewJson->slice());
    ASSERT_TRUE((false == !logicalView));
    auto* viewImpl = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
    ASSERT_TRUE((nullptr != viewImpl));

    EXPECT_TRUE((viewImpl->properties(updateJson->slice(), false).ok()));

    std::set<arangodb::DataSourceId> actual;
    std::set<arangodb::DataSourceId> expected = {arangodb::DataSourceId{100}};
    viewImpl->visitCollections([&actual](arangodb::DataSourceId cid) -> bool {
      actual.emplace(cid);
      return true;
    });

    for (auto& cid: actual) {
      EXPECT_TRUE((1 == expected.erase(cid)));
    }

    EXPECT_TRUE((expected.empty()));
  }

  // test drop via link after open (TRI_vocbase_t::createView(...) will call open())
  {
    auto& engine = *static_cast<StorageEngineMock*>(
        &server.getFeature<arangodb::EngineSelectorFeature>().engine());
    engine.views.clear();
    auto updateJson0 = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection\": { } } }");
    auto updateJson1 = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection\": null } }");
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    ASSERT_TRUE((nullptr != logicalCollection));
    auto logicalView = vocbase.createView(viewJson->slice());
    ASSERT_TRUE((false == !logicalView));
    auto* viewImpl = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
    ASSERT_TRUE((nullptr != viewImpl));

    // create link
    {
      EXPECT_TRUE((viewImpl->properties(updateJson0->slice(), false).ok()));

      std::set<arangodb::DataSourceId> actual;
      std::set<arangodb::DataSourceId> expected = {arangodb::DataSourceId{100}};
      viewImpl->visitCollections([&actual](arangodb::DataSourceId cid) -> bool {
        actual.emplace(cid);
        return true;
      });

      for (auto& cid: actual) {
        EXPECT_TRUE((1 == expected.erase(cid)));
      }

      EXPECT_TRUE((expected.empty()));
    }

    // drop link
    {
      EXPECT_TRUE((viewImpl->properties(updateJson1->slice(), false).ok()));

      std::set<arangodb::DataSourceId> actual;
      viewImpl->visitCollections([&actual](arangodb::DataSourceId cid) -> bool {
        actual.emplace(cid);
        return true;
      });
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

  Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
  auto logicalView = vocbase.createView(viewJson->slice()); // create view
  ASSERT_TRUE(nullptr != logicalView);

  VPackBuilder builder;

  // check immutable properties after creation
  {
    builder.openObject();

    EXPECT_TRUE(logicalView->properties(builder, arangodb::LogicalDataSource::Serialization::Properties).ok());
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
    EXPECT_EQ(irs::type<irs::compression::lz4>::id(), meta._primarySortCompression);
  }

  auto newProperties = arangodb::velocypack::Parser::fromJson(
    "{"
      "\"writeBufferActive\": 125, "
      "\"writeBufferIdle\": 112, "
      "\"writeBufferSizeMax\": 142, "
      "\"locale\": \"en\", "
      "\"version\": 1, "
      "\"primarySortCompression\":\"none\","
      "\"primarySort\": [ "
        "{ \"field\": \"field\", \"asc\": true } "
      "]"
  "}");

  EXPECT_TRUE(logicalView->properties(newProperties->slice(), false).ok()); // update immutable properties

  // check immutable properties after update
  {
    builder.clear();
    builder.openObject();
    EXPECT_TRUE(logicalView->properties(builder, arangodb::LogicalDataSource::Serialization::Properties).ok());
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
    EXPECT_EQ(irs::type<irs::compression::lz4>::id(), meta._primarySortCompression);
  }
}

TEST_F(IResearchViewTest, test_transaction_registration) {
  auto collectionJson0 = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection0\" }");
  auto collectionJson1 = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection1\" }");
  auto viewJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
  Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
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
  Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
  auto logicalCollection = vocbase.createCollection(collectionJson->slice());
  ASSERT_TRUE((false == !logicalCollection));
  auto logicalView = vocbase.createView(viewJson->slice());
  ASSERT_TRUE((false == !logicalView));
  auto* viewImpl = dynamic_cast<arangodb::iresearch::IResearchView*>(logicalView.get());
  ASSERT_TRUE((nullptr != viewImpl));
  auto index = StorageEngineMock::buildLinkMock(arangodb::IndexId{42}, *logicalCollection, linkJson->slice());
  ASSERT_NE(nullptr, index);
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
    EXPECT_TRUE((link->insert(trx, arangodb::LocalDocumentId(0), doc->slice()).ok()));
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
    EXPECT_TRUE((link->insert(trx, arangodb::LocalDocumentId(1), doc->slice()).ok()));
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
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
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
        EXPECT_TRUE(view->properties(builder, arangodb::LogicalDataSource::Serialization::Properties).ok());
        builder.close();

        auto slice = builder.slice();
        arangodb::iresearch::IResearchViewMeta meta;
        std::string error;

        EXPECT_TRUE(slice.isObject());
        EXPECT_EQ(15, slice.length());
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
        view->properties(builder, arangodb::LogicalDataSource::Serialization::Persistence);
        builder.close();

        auto slice = builder.slice();
        arangodb::iresearch::IResearchViewMeta meta;
        arangodb::iresearch::IResearchViewMetaState metaState;
        std::string error;

        EXPECT_TRUE(slice.isObject());
        EXPECT_EQ(19, slice.length());
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
        view->properties(builder, arangodb::LogicalDataSource::Serialization::Properties);
        builder.close();

        auto slice = builder.slice();
        arangodb::iresearch::IResearchViewMeta meta;
        arangodb::iresearch::IResearchViewMetaState metaState;
        std::string error;

        EXPECT_TRUE(slice.isObject());
        EXPECT_EQ(15, slice.length());
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
        view->properties(builder, arangodb::LogicalDataSource::Serialization::Persistence);
        builder.close();

        auto slice = builder.slice();
        arangodb::iresearch::IResearchViewMeta meta;
        arangodb::iresearch::IResearchViewMetaState metaState;
        std::string error;

        EXPECT_TRUE(slice.isObject());
        EXPECT_EQ(19, slice.length());
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
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
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
      EXPECT_TRUE(logicalView->properties(builder, arangodb::LogicalDataSource::Serialization::Properties).ok());
      builder.close();

      auto slice = builder.slice();
      arangodb::iresearch::IResearchViewMeta meta;
      arangodb::iresearch::IResearchViewMetaState metaState;
      std::string error;

      EXPECT_TRUE((slice.isObject()));
      EXPECT_EQ(15, slice.length());
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
      logicalView->properties(builder, arangodb::LogicalDataSource::Serialization::Persistence);
      builder.close();

      auto slice = builder.slice();
      arangodb::iresearch::IResearchViewMeta meta;
      arangodb::iresearch::IResearchViewMetaState metaState;
      std::string error;

      EXPECT_TRUE((slice.isObject()));
      EXPECT_EQ(19, slice.length());
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
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
    auto logicalView = vocbase.createView(createJson->slice());
    ASSERT_TRUE((false == !logicalView));
    ASSERT_TRUE((logicalView->category() == arangodb::LogicalView::category()));

    arangodb::iresearch::IResearchViewMeta expectedMeta;
    arangodb::iresearch::IResearchViewMetaState expectedMetaState;
    auto updateJson = arangodb::velocypack::Parser::fromJson("{ \"cleanupIntervalStep\": 62, \"links\": { \"testCollection\": {} } }");

    expectedMeta._cleanupIntervalStep = 52;
    EXPECT_TRUE((TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND == logicalView->properties(updateJson->slice(), false).errorNumber()));
    EXPECT_TRUE((true == logicalView->visitCollections(
                             [](arangodb::DataSourceId) -> bool { return false; })));

    // not for persistence
    {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      EXPECT_TRUE(logicalView->properties(builder, arangodb::LogicalDataSource::Serialization::Properties).ok());
      builder.close();

      auto slice = builder.slice();
      arangodb::iresearch::IResearchViewMeta meta;
      arangodb::iresearch::IResearchViewMetaState metaState;
      std::string error;

      EXPECT_TRUE((slice.isObject()));
      EXPECT_EQ(15, slice.length());
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
      logicalView->properties(builder, arangodb::LogicalDataSource::Serialization::Persistence);
      builder.close();

      auto slice = builder.slice();
      arangodb::iresearch::IResearchViewMeta meta;
      arangodb::iresearch::IResearchViewMetaState metaState;
      std::string error;

      EXPECT_TRUE((slice.isObject()));
      EXPECT_EQ(19, slice.length());
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
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
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
    EXPECT_TRUE((true == logicalView->visitCollections(
                             [](arangodb::DataSourceId) -> bool { return false; })));

    // not for persistence
    {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      logicalView->properties(builder, arangodb::LogicalDataSource::Serialization::Properties);
      builder.close();

      auto slice = builder.slice();
      arangodb::iresearch::IResearchViewMeta meta;
      arangodb::iresearch::IResearchViewMetaState metaState;
      std::string error;

      EXPECT_TRUE((slice.isObject()));
      EXPECT_EQ(15, slice.length());
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
      EXPECT_TRUE(logicalView->properties(builder, arangodb::LogicalDataSource::Serialization::Persistence).ok());
      builder.close();

      auto slice = builder.slice();
      arangodb::iresearch::IResearchViewMeta meta;
      arangodb::iresearch::IResearchViewMetaState metaState;
      std::string error;

      EXPECT_TRUE((slice.isObject()));
      EXPECT_EQ(19, slice.length());
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
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
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
        logicalView->properties(builder, arangodb::LogicalDataSource::Serialization::Properties);
        builder.close();

        auto slice = builder.slice();
        arangodb::iresearch::IResearchViewMeta meta;
        arangodb::iresearch::IResearchViewMetaState metaState;
        std::string error;

        EXPECT_TRUE((slice.isObject()));
        EXPECT_EQ(15, slice.length());
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
          EXPECT_TRUE((true == value.isObject() &&
                       expectedItr != expectedLinkMeta.end() &&
                       linkMeta.init(server.server(), value, false, error) &&
                       expectedItr->second == linkMeta));
          expectedLinkMeta.erase(expectedItr);
        }
      }

      // for persistence
      {
        arangodb::velocypack::Builder builder;

        builder.openObject();
        logicalView->properties(builder, arangodb::LogicalDataSource::Serialization::Persistence);
        builder.close();

        auto slice = builder.slice();
        arangodb::iresearch::IResearchViewMeta meta;
        arangodb::iresearch::IResearchViewMetaState metaState;
        std::string error;

        EXPECT_TRUE((slice.isObject()));
        EXPECT_EQ(19, slice.length());
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
        logicalView->properties(builder, arangodb::LogicalDataSource::Serialization::Properties);
        builder.close();

        auto slice = builder.slice();
        arangodb::iresearch::IResearchViewMeta meta;
        arangodb::iresearch::IResearchViewMetaState metaState;
        std::string error;

        EXPECT_TRUE((slice.isObject()));
        EXPECT_EQ(15, slice.length());
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
        logicalView->properties(builder, arangodb::LogicalDataSource::Serialization::Persistence);
        builder.close();

        auto slice = builder.slice();
        arangodb::iresearch::IResearchViewMeta meta;
        arangodb::iresearch::IResearchViewMetaState metaState;
        std::string error;

        EXPECT_TRUE((slice.isObject()));
        EXPECT_EQ(19, slice.length());
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
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
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
        view->properties(builder, arangodb::LogicalDataSource::Serialization::Properties);
        builder.close();

        auto slice = builder.slice();
        arangodb::iresearch::IResearchViewMeta meta;
        arangodb::iresearch::IResearchViewMetaState metaState;
        std::string error;

        EXPECT_TRUE(slice.isObject());
        EXPECT_EQ(15, slice.length());
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
          EXPECT_TRUE((true == value.isObject() &&
                       expectedItr != expectedLinkMeta.end() &&
                       linkMeta.init(server.server(), value, false, error) &&
                       expectedItr->second == linkMeta));
          expectedLinkMeta.erase(expectedItr);
        }
      }

      // for persistence
      {
        arangodb::velocypack::Builder builder;

        builder.openObject();
        view->properties(builder, arangodb::LogicalDataSource::Serialization::Persistence);
        builder.close();

        auto slice = builder.slice();
        arangodb::iresearch::IResearchViewMeta meta;
        arangodb::iresearch::IResearchViewMetaState metaState;
        std::string error;

        EXPECT_TRUE(slice.isObject());
        EXPECT_EQ(19, slice.length());
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
        view->properties(builder, arangodb::LogicalDataSource::Serialization::Properties);
        builder.close();

        auto slice = builder.slice();
        arangodb::iresearch::IResearchViewMeta meta;
        arangodb::iresearch::IResearchViewMetaState metaState;
        std::string error;

        EXPECT_TRUE(slice.isObject());
        EXPECT_EQ(15, slice.length());
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
          EXPECT_TRUE((true == value.isObject() &&
                       expectedItr != expectedLinkMeta.end() &&
                       linkMeta.init(server.server(), value, false, error) &&
                       expectedItr->second == linkMeta));
          expectedLinkMeta.erase(expectedItr);
        }
      }

      // for persistence
      {
        arangodb::velocypack::Builder builder;

        builder.openObject();
        view->properties(builder, arangodb::LogicalDataSource::Serialization::Persistence);
        builder.close();

        auto slice = builder.slice();
        arangodb::iresearch::IResearchViewMeta meta;
        arangodb::iresearch::IResearchViewMetaState metaState;
        std::string error;

        EXPECT_TRUE(slice.isObject());
        EXPECT_EQ(19, slice.length());
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
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
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
        view->properties(builder, arangodb::LogicalDataSource::Serialization::Properties);
        builder.close();

        auto slice = builder.slice();
        EXPECT_TRUE(slice.isObject());
        EXPECT_EQ(15, slice.length());
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
        view->properties(builder, arangodb::LogicalDataSource::Serialization::Persistence);
        builder.close();

        auto slice = builder.slice();
        EXPECT_TRUE(slice.isObject());
        EXPECT_EQ(19, slice.length());
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
        view->properties(builder, arangodb::LogicalDataSource::Serialization::Properties);
        builder.close();

        auto slice = builder.slice();
        EXPECT_EQ(15, slice.length());
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
        view->properties(builder, arangodb::LogicalDataSource::Serialization::Persistence);
        builder.close();

        auto slice = builder.slice();
        EXPECT_EQ(19, slice.length());
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

    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    ASSERT_TRUE((nullptr != logicalCollection));
    auto logicalView = vocbase.createView(viewCreateJson->slice());
    ASSERT_TRUE((false == !logicalView));

    EXPECT_TRUE((true == logicalCollection->getIndexes().empty()));
    EXPECT_TRUE((true == logicalView->visitCollections(
                             [](arangodb::DataSourceId) -> bool { return false; })));

    // initial link creation
    {
      auto updateJson = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection\": {} } }");
      EXPECT_TRUE((logicalView->properties(updateJson->slice(), true).ok()));
      EXPECT_TRUE((false == logicalCollection->getIndexes().empty()));
      EXPECT_TRUE((false == logicalView->visitCollections([](arangodb::DataSourceId) -> bool {
        return false;
      })));
    }

    struct ExecContext: public arangodb::ExecContext {
      ExecContext(): arangodb::ExecContext(arangodb::ExecContext::Type::Default, "", "",
                                           arangodb::auth::Level::NONE, arangodb::auth::Level::NONE, false) {}
    } execContext;
    arangodb::ExecContextScope execContextScope(&execContext);
    auto* authFeature = arangodb::AuthenticationFeature::instance();
    auto* userManager = authFeature->userManager();
    
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
      logicalView->properties(builder, arangodb::LogicalDataSource::Serialization::Persistence); // 'forPersistence' to avoid auth check
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
      logicalView->properties(builder, arangodb::LogicalDataSource::Serialization::Persistence); // 'forPersistence' to avoid auth check
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

    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    ASSERT_TRUE((nullptr != logicalCollection));
    auto logicalView = vocbase.createView(viewCreateJson->slice());
    ASSERT_TRUE((false == !logicalView));

    EXPECT_TRUE((true == logicalCollection->getIndexes().empty()));
    EXPECT_TRUE((true == logicalView->visitCollections(
                             [](arangodb::DataSourceId) -> bool { return false; })));

    struct ExecContext: public arangodb::ExecContext {
      ExecContext(): arangodb::ExecContext(arangodb::ExecContext::Type::Default, "", "",
                                           arangodb::auth::Level::NONE, arangodb::auth::Level::NONE, false) {}
    } execContext;
    arangodb::ExecContextScope scope(&execContext);
    auto* authFeature = arangodb::AuthenticationFeature::instance();
    auto* userManager = authFeature->userManager();
    arangodb::auth::UserMap userMap; // empty map, no user -> no permissions
    userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database
    auto resetUserManager = irs::make_finally([userManager]()->void{ userManager->removeAllUsers(); });

    EXPECT_TRUE((TRI_ERROR_FORBIDDEN == logicalView->properties(viewUpdateJson->slice(), false).errorNumber()));
    EXPECT_TRUE((true == logicalCollection->getIndexes().empty()));
    EXPECT_TRUE((true == logicalView->visitCollections(
                             [](arangodb::DataSourceId) -> bool { return false; })));
  }

  // drop link (collection not authorized)
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\", \"id\": 100 }");
    auto viewCreateJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
    auto viewUpdateJson = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection\": null } }");

    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    ASSERT_TRUE((nullptr != logicalCollection));
    auto logicalView = vocbase.createView(viewCreateJson->slice());
    ASSERT_TRUE((false == !logicalView));

    EXPECT_TRUE((true == logicalCollection->getIndexes().empty()));
    EXPECT_TRUE((true == logicalView->visitCollections(
                             [](arangodb::DataSourceId) -> bool { return false; })));

    // initial link creation
    {
      auto updateJson = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection\": {} } }");
      EXPECT_TRUE((logicalView->properties(updateJson->slice(), true).ok()));
      EXPECT_TRUE((false == logicalCollection->getIndexes().empty()));
      EXPECT_TRUE((false == logicalView->visitCollections([](arangodb::DataSourceId) -> bool {
        return false;
      })));
    }

    struct ExecContext: public arangodb::ExecContext {
      ExecContext(): arangodb::ExecContext(arangodb::ExecContext::Type::Default, "", "",
                                           arangodb::auth::Level::NONE, arangodb::auth::Level::NONE, false) {}
    } execContext;
    arangodb::ExecContextScope execContextScope(&execContext);
    auto* authFeature = arangodb::AuthenticationFeature::instance();
    auto* userManager = authFeature->userManager();
    
    auto resetUserManager = std::shared_ptr<arangodb::auth::UserManager>(userManager, [](arangodb::auth::UserManager* ptr)->void { ptr->removeAllUsers(); });

    // subsequent update (overwrite) not authorised (NONE collection)
    {
      arangodb::auth::UserMap userMap;
      auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
      user.grantCollection(vocbase.name(), "testCollection", arangodb::auth::Level::NONE); // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

      EXPECT_TRUE((TRI_ERROR_FORBIDDEN == logicalView->properties(viewUpdateJson->slice(), false).errorNumber()));
      EXPECT_TRUE((false == logicalCollection->getIndexes().empty()));
      EXPECT_TRUE((false == logicalView->visitCollections([](arangodb::DataSourceId) -> bool {
        return false;
      })));
    }

    // subsequent update (overwrite) authorised (RO collection)
    {
      arangodb::auth::UserMap userMap;
      auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
      user.grantCollection(vocbase.name(), "testCollection", arangodb::auth::Level::RO); // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

      EXPECT_TRUE((logicalView->properties(viewUpdateJson->slice(), false).ok()));
      EXPECT_TRUE((true == logicalCollection->getIndexes().empty()));
      EXPECT_TRUE((true == logicalView->visitCollections([](arangodb::DataSourceId) -> bool {
        return false;
      })));
    }
  }

  // add authorised link (existing collection not authorized)
  {
    auto collection0Json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection0\", \"id\": 100 }");
    auto collection1Json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection1\", \"id\": 101 }");
    auto viewCreateJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
    auto viewUpdateJson = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection0\": {}, \"testCollection1\": {} } }");

    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
    auto logicalCollection0 = vocbase.createCollection(collection0Json->slice());
    ASSERT_TRUE((nullptr != logicalCollection0));
    auto logicalCollection1 = vocbase.createCollection(collection1Json->slice());
    ASSERT_TRUE((nullptr != logicalCollection1));
    auto logicalView = vocbase.createView(viewCreateJson->slice());
    ASSERT_TRUE((false == !logicalView));

    EXPECT_TRUE((true == logicalCollection0->getIndexes().empty()));
    EXPECT_TRUE((true == logicalCollection1->getIndexes().empty()));
    EXPECT_TRUE((true == logicalView->visitCollections(
                             [](arangodb::DataSourceId) -> bool { return false; })));

    // initial link creation
    {
      auto updateJson = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection0\": {} } }");
      EXPECT_TRUE((logicalView->properties(updateJson->slice(), true).ok()));
      EXPECT_TRUE((false == logicalCollection0->getIndexes().empty()));
      EXPECT_TRUE((true == logicalCollection1->getIndexes().empty()));
      EXPECT_TRUE((false == logicalView->visitCollections([](arangodb::DataSourceId) -> bool {
        return false;
      })));
    }

    struct ExecContext: public arangodb::ExecContext {
      ExecContext(): arangodb::ExecContext(arangodb::ExecContext::Type::Default, "", "",
                                           arangodb::auth::Level::NONE, arangodb::auth::Level::NONE, false) {}
    } execContext;
    arangodb::ExecContextScope execContextScope(&execContext);
    auto* authFeature = arangodb::AuthenticationFeature::instance();
    auto* userManager = authFeature->userManager();
    
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
      EXPECT_TRUE((false == logicalView->visitCollections([](arangodb::DataSourceId) -> bool {
        return false;
      })));
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
      EXPECT_TRUE((false == logicalView->visitCollections([](arangodb::DataSourceId) -> bool {
        return false;
      })));
    }
  }

  // drop authorised link (existing collection not authorized)
  {
    auto collection0Json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection0\", \"id\": 100 }");
    auto collection1Json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection1\", \"id\": 101 }");
    auto viewCreateJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
    auto viewUpdateJson = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection0\": {} } }");

    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
    auto logicalCollection0 = vocbase.createCollection(collection0Json->slice());
    ASSERT_TRUE((nullptr != logicalCollection0));
    auto logicalCollection1 = vocbase.createCollection(collection1Json->slice());
    ASSERT_TRUE((nullptr != logicalCollection1));
    auto logicalView = vocbase.createView(viewCreateJson->slice());
    ASSERT_TRUE((false == !logicalView));

    EXPECT_TRUE((true == logicalCollection0->getIndexes().empty()));
    EXPECT_TRUE((true == logicalCollection1->getIndexes().empty()));
    EXPECT_TRUE((true == logicalView->visitCollections(
                             [](arangodb::DataSourceId) -> bool { return false; })));

    // initial link creation
    {
      auto updateJson = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection0\": {}, \"testCollection1\": {} } }");
      EXPECT_TRUE((logicalView->properties(updateJson->slice(), true).ok()));
      EXPECT_TRUE((false == logicalCollection0->getIndexes().empty()));
      EXPECT_TRUE((false == logicalCollection1->getIndexes().empty()));
      EXPECT_TRUE((false == logicalView->visitCollections([](arangodb::DataSourceId) -> bool {
        return false;
      })));
    }

    struct ExecContext: public arangodb::ExecContext {
      ExecContext(): arangodb::ExecContext(arangodb::ExecContext::Type::Default, "", "",
                                           arangodb::auth::Level::NONE, arangodb::auth::Level::NONE, false) {}
    } execContext;
    arangodb::ExecContextScope execContextScope(&execContext);
    auto* authFeature = arangodb::AuthenticationFeature::instance();
    auto* userManager = authFeature->userManager();
    
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
      EXPECT_TRUE((false == logicalView->visitCollections([](arangodb::DataSourceId) -> bool {
        return false;
      })));
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
      EXPECT_TRUE((false == logicalView->visitCollections([](arangodb::DataSourceId) -> bool {
        return false;
      })));
    }
  }

  // drop link (collection not authorized)
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\", \"id\": 100 }");
    auto viewCreateJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
    auto viewUpdateJson = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection\": null } }");

    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    ASSERT_TRUE((nullptr != logicalCollection));
    auto logicalView = vocbase.createView(viewCreateJson->slice());
    ASSERT_TRUE((false == !logicalView));

    EXPECT_TRUE((true == logicalCollection->getIndexes().empty()));
    EXPECT_TRUE((true == logicalView->visitCollections(
                             [](arangodb::DataSourceId) -> bool { return false; })));

    // initial link creation
    {
      auto updateJson = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection\": {} } }");
      EXPECT_TRUE((logicalView->properties(updateJson->slice(), true).ok()));
      EXPECT_TRUE((false == logicalCollection->getIndexes().empty()));
      EXPECT_TRUE((false == logicalView->visitCollections([](arangodb::DataSourceId) -> bool {
        return false;
      })));
    }

    struct ExecContext: public arangodb::ExecContext {
      ExecContext(): arangodb::ExecContext(arangodb::ExecContext::Type::Default, "", "",
                                           arangodb::auth::Level::NONE, arangodb::auth::Level::NONE, false) {}
    } execContext;
    arangodb::ExecContextScope scopedExecContext(&execContext);
    auto* authFeature = arangodb::AuthenticationFeature::instance();
    auto* userManager = authFeature->userManager();
    
    auto resetUserManager = std::shared_ptr<arangodb::auth::UserManager>(userManager, [](arangodb::auth::UserManager* ptr)->void { ptr->removeAllUsers(); });

    // subsequent update (overwrite) not authorised (NONE collection)
    {
      arangodb::auth::UserMap userMap;
      auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
      user.grantCollection(vocbase.name(), "testCollection", arangodb::auth::Level::NONE); // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

      EXPECT_TRUE((TRI_ERROR_FORBIDDEN == logicalView->properties(viewUpdateJson->slice(), false).errorNumber()));
      EXPECT_TRUE((false == logicalCollection->getIndexes().empty()));
      EXPECT_TRUE((false == logicalView->visitCollections([](arangodb::DataSourceId) -> bool {
        return false;
      })));
    }

    // subsequent update (overwrite) authorised (RO collection)
    {
      arangodb::auth::UserMap userMap;
      auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
      user.grantCollection(vocbase.name(), "testCollection", arangodb::auth::Level::RO); // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

      EXPECT_TRUE((logicalView->properties(viewUpdateJson->slice(), false).ok()));
      EXPECT_TRUE((true == logicalCollection->getIndexes().empty()));
      EXPECT_TRUE((true == logicalView->visitCollections([](arangodb::DataSourceId) -> bool {
        return false;
      })));
    }
  }

  // add authorised link (existing collection not authorized)
  {
    auto collection0Json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection0\", \"id\": 100 }");
    auto collection1Json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection1\", \"id\": 101 }");
    auto viewCreateJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
    auto viewUpdateJson = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection0\": {}, \"testCollection1\": {} } }");

    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
    auto logicalCollection0 = vocbase.createCollection(collection0Json->slice());
    ASSERT_TRUE((nullptr != logicalCollection0));
    auto logicalCollection1 = vocbase.createCollection(collection1Json->slice());
    ASSERT_TRUE((nullptr != logicalCollection1));
    auto logicalView = vocbase.createView(viewCreateJson->slice());
    ASSERT_TRUE((false == !logicalView));

    EXPECT_TRUE((true == logicalCollection0->getIndexes().empty()));
    EXPECT_TRUE((true == logicalCollection1->getIndexes().empty()));
    EXPECT_TRUE((true == logicalView->visitCollections(
                             [](arangodb::DataSourceId) -> bool { return false; })));

    // initial link creation
    {
      auto updateJson = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection0\": {} } }");
      EXPECT_TRUE((logicalView->properties(updateJson->slice(), true).ok()));
      EXPECT_TRUE((false == logicalCollection0->getIndexes().empty()));
      EXPECT_TRUE((true == logicalCollection1->getIndexes().empty()));
      EXPECT_TRUE((false == logicalView->visitCollections([](arangodb::DataSourceId) -> bool {
        return false;
      })));
    }

    struct ExecContext: public arangodb::ExecContext {
      ExecContext(): arangodb::ExecContext(arangodb::ExecContext::Type::Default, "", "",
                                           arangodb::auth::Level::NONE, arangodb::auth::Level::NONE, false) {}
    } execContext;
    arangodb::ExecContextScope scopedExecContext(&execContext);
    auto* authFeature = arangodb::AuthenticationFeature::instance();
    auto* userManager = authFeature->userManager();
    
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
      EXPECT_TRUE((false == logicalView->visitCollections([](arangodb::DataSourceId) -> bool {
        return false;
      })));
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
      EXPECT_TRUE((false == logicalView->visitCollections([](arangodb::DataSourceId) -> bool {
        return false;
      })));
    }
  }

  // drop authorised link (existing collection not authorized)
  {
    auto collection0Json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection0\", \"id\": 100 }");
    auto collection1Json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection1\", \"id\": 101 }");
    auto viewCreateJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
    auto viewUpdateJson = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection0\": {} } }");

    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
    auto logicalCollection0 = vocbase.createCollection(collection0Json->slice());
    ASSERT_TRUE((nullptr != logicalCollection0));
    auto logicalCollection1 = vocbase.createCollection(collection1Json->slice());
    ASSERT_TRUE((nullptr != logicalCollection1));
    auto logicalView = vocbase.createView(viewCreateJson->slice());
    ASSERT_TRUE((false == !logicalView));

    EXPECT_TRUE((true == logicalCollection0->getIndexes().empty()));
    EXPECT_TRUE((true == logicalCollection1->getIndexes().empty()));
    EXPECT_TRUE((true == logicalView->visitCollections(
                             [](arangodb::DataSourceId) -> bool { return false; })));

    // initial link creation
    {
      auto updateJson = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection0\": {}, \"testCollection1\": {} } }");
      EXPECT_TRUE((logicalView->properties(updateJson->slice(), true).ok()));
      EXPECT_TRUE((false == logicalCollection0->getIndexes().empty()));
      EXPECT_TRUE((false == logicalCollection1->getIndexes().empty()));
      EXPECT_TRUE((false == logicalView->visitCollections([](arangodb::DataSourceId) -> bool {
        return false;
      })));
    }

    struct ExecContext: public arangodb::ExecContext {
      ExecContext(): arangodb::ExecContext(arangodb::ExecContext::Type::Default, "", "",
                                           arangodb::auth::Level::NONE, arangodb::auth::Level::NONE, false) {}
    } execContext;
    arangodb::ExecContextScope scopedExecContext(&execContext);
    auto* authFeature = arangodb::AuthenticationFeature::instance();
    auto* userManager = authFeature->userManager();
    
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
      EXPECT_TRUE((false == logicalView->visitCollections([](arangodb::DataSourceId) -> bool {
        return false;
      })));
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
      EXPECT_TRUE((false == logicalView->visitCollections([](arangodb::DataSourceId) -> bool {
        return false;
      })));
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
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
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
      view->properties(builder, arangodb::LogicalDataSource::Serialization::Properties);
      builder.close();

      auto slice = builder.slice();
      arangodb::iresearch::IResearchViewMeta meta;
      arangodb::iresearch::IResearchViewMetaState metaState;
      std::string error;

      EXPECT_TRUE(slice.isObject());
      EXPECT_EQ(15, slice.length());
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
      view->properties(builder, arangodb::LogicalDataSource::Serialization::Persistence);
      builder.close();

      auto slice = builder.slice();
      arangodb::iresearch::IResearchViewMeta meta;
      arangodb::iresearch::IResearchViewMetaState metaState;
      std::string error;

      EXPECT_TRUE(slice.isObject());
      EXPECT_EQ(19, slice.length());
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
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
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
      logicalView->properties(builder, arangodb::LogicalDataSource::Serialization::Properties);
      builder.close();

      auto slice = builder.slice();
      arangodb::iresearch::IResearchViewMeta meta;
      arangodb::iresearch::IResearchViewMetaState metaState;
      std::string error;

      EXPECT_TRUE((slice.isObject()));
      EXPECT_EQ(15, slice.length());
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
      logicalView->properties(builder, arangodb::LogicalDataSource::Serialization::Persistence);
      builder.close();

      auto slice = builder.slice();
      arangodb::iresearch::IResearchViewMeta meta;
      arangodb::iresearch::IResearchViewMetaState metaState;
      std::string error;

      EXPECT_TRUE((slice.isObject()));
      EXPECT_EQ(19, slice.length());
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
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
    auto logicalView = vocbase.createView(createJson->slice());
    ASSERT_TRUE((false == !logicalView));
    ASSERT_TRUE((logicalView->category() == arangodb::LogicalView::category()));

    arangodb::iresearch::IResearchViewMeta expectedMeta;
    arangodb::iresearch::IResearchViewMetaState expectedMetaState;
    auto updateJson = arangodb::velocypack::Parser::fromJson("{ \"cleanupIntervalStep\": 62, \"links\": { \"testCollection\": {} } }");

    expectedMeta._cleanupIntervalStep = 52;
    EXPECT_TRUE((TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND == logicalView->properties(updateJson->slice(), true).errorNumber()));
    EXPECT_TRUE((true == logicalView->visitCollections(
                             [](arangodb::DataSourceId) -> bool { return false; })));

    // not for persistence
    {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      logicalView->properties(builder, arangodb::LogicalDataSource::Serialization::Properties);
      builder.close();

      auto slice = builder.slice();
      arangodb::iresearch::IResearchViewMeta meta;
      arangodb::iresearch::IResearchViewMetaState metaState;
      std::string error;

      EXPECT_TRUE((slice.isObject()));
      EXPECT_EQ(15, slice.length());
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
      logicalView->properties(builder, arangodb::LogicalDataSource::Serialization::Persistence);
      builder.close();

      auto slice = builder.slice();
      arangodb::iresearch::IResearchViewMeta meta;
      arangodb::iresearch::IResearchViewMetaState metaState;
      std::string error;

      EXPECT_TRUE((slice.isObject()));
      EXPECT_EQ(19, slice.length());
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
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
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
    EXPECT_TRUE((true == logicalView->visitCollections(
                             [](arangodb::DataSourceId) -> bool { return false; })));

    // not for persistence
    {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      logicalView->properties(builder, arangodb::LogicalDataSource::Serialization::Properties);
      builder.close();

      auto slice = builder.slice();
      arangodb::iresearch::IResearchViewMeta meta;
      arangodb::iresearch::IResearchViewMetaState metaState;
      std::string error;

      EXPECT_TRUE((slice.isObject()));
      EXPECT_EQ(15, slice.length());
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
      logicalView->properties(builder, arangodb::LogicalDataSource::Serialization::Persistence);
      builder.close();

      auto slice = builder.slice();
      arangodb::iresearch::IResearchViewMeta meta;
      arangodb::iresearch::IResearchViewMetaState metaState;
      std::string error;

      EXPECT_TRUE((slice.isObject()));
      EXPECT_EQ(19, slice.length());
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
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
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
        logicalView->properties(builder, arangodb::LogicalDataSource::Serialization::Properties);
        builder.close();

        auto slice = builder.slice();
        arangodb::iresearch::IResearchViewMeta meta;
        arangodb::iresearch::IResearchViewMetaState metaState;
        std::string error;

        EXPECT_TRUE((slice.isObject()));
        EXPECT_EQ(15, slice.length());
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
          EXPECT_TRUE((true == value.isObject() &&
                       expectedItr != expectedLinkMeta.end() &&
                       linkMeta.init(server.server(), value, false, error) &&
                       expectedItr->second == linkMeta));
          expectedLinkMeta.erase(expectedItr);
        }
      }

      // for persistence
      {
        arangodb::velocypack::Builder builder;

        builder.openObject();
        logicalView->properties(builder, arangodb::LogicalDataSource::Serialization::Persistence);
        builder.close();

        auto slice = builder.slice();
        arangodb::iresearch::IResearchViewMeta meta;
        arangodb::iresearch::IResearchViewMetaState metaState;
        std::string error;

        EXPECT_TRUE((slice.isObject()));
        EXPECT_EQ(19, slice.length());
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
        logicalView->properties(builder, arangodb::LogicalDataSource::Serialization::Properties);
        builder.close();

        auto slice = builder.slice();
        arangodb::iresearch::IResearchViewMeta meta;
        arangodb::iresearch::IResearchViewMetaState metaState;
        std::string error;

        EXPECT_TRUE((slice.isObject()));
        EXPECT_EQ(15, slice.length());
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
          EXPECT_TRUE((true == value.isObject() &&
                       expectedItr != expectedLinkMeta.end() &&
                       linkMeta.init(server.server(), value, false, error) &&
                       expectedItr->second == linkMeta));
          expectedLinkMeta.erase(expectedItr);
        }
      }

      // for persistence
      {
        arangodb::velocypack::Builder builder;

        builder.openObject();
        logicalView->properties(builder, arangodb::LogicalDataSource::Serialization::Persistence);
        builder.close();

        auto slice = builder.slice();
        arangodb::iresearch::IResearchViewMeta meta;
        arangodb::iresearch::IResearchViewMetaState metaState;
        std::string error;

        EXPECT_TRUE((slice.isObject()));
        EXPECT_EQ(19, slice.length());
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
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    ASSERT_TRUE((nullptr != logicalCollection));
    auto view = vocbase.createView(createJson->slice());
    ASSERT_TRUE((false == !view));
    ASSERT_TRUE(view->category() == arangodb::LogicalView::category());

    auto updateJson = arangodb::velocypack::Parser::fromJson(
      "{ \"links\": { \"testCollection\": {} } }"
    );

    auto beforeRec = StorageEngineMock::recoveryStateResult;
    StorageEngineMock::recoveryStateResult = arangodb::RecoveryState::IN_PROGRESS;
    auto restore = irs::make_finally([&beforeRec]()->void { StorageEngineMock::recoveryStateResult = beforeRec; });
    persisted = false;
    EXPECT_TRUE((view->properties(updateJson->slice(), true).ok()));
    EXPECT_TRUE((true == persisted));

    // not for persistence
    {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      view->properties(builder, arangodb::LogicalDataSource::Serialization::Properties);
      builder.close();

      auto slice = builder.slice();
      EXPECT_TRUE(slice.isObject());
      EXPECT_EQ(15, slice.length());
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
      view->properties(builder, arangodb::LogicalDataSource::Serialization::Persistence);
      builder.close();

      auto slice = builder.slice();
      EXPECT_TRUE(slice.isObject());
      EXPECT_EQ(19, slice.length());
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
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
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
      view->properties(builder, arangodb::LogicalDataSource::Serialization::Properties);
      builder.close();

      auto slice = builder.slice();
      arangodb::iresearch::IResearchViewMeta meta;
      arangodb::iresearch::IResearchViewMetaState metaState;
      std::string error;

      EXPECT_TRUE(slice.isObject());
      EXPECT_EQ(15, slice.length());
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
        EXPECT_TRUE((true == value.isObject() && expectedItr != expectedLinkMeta.end() &&
                     linkMeta.init(server.server(), value, false, error) &&
                     expectedItr->second == linkMeta));
        expectedLinkMeta.erase(expectedItr);
      }
    }

    // for persistence
    {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      view->properties(builder, arangodb::LogicalDataSource::Serialization::Persistence);
      builder.close();

      auto slice = builder.slice();
      arangodb::iresearch::IResearchViewMeta meta;
      arangodb::iresearch::IResearchViewMetaState metaState;
      std::string error;

      EXPECT_TRUE(slice.isObject());
      EXPECT_EQ(19, slice.length());
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
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
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
      view->properties(builder, arangodb::LogicalDataSource::Serialization::Properties);
      builder.close();

      auto slice = builder.slice();
      arangodb::iresearch::IResearchViewMeta meta;
      arangodb::iresearch::IResearchViewMetaState metaState;
      std::string error;

      EXPECT_TRUE(slice.isObject());
      EXPECT_EQ(15, slice.length());
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
        EXPECT_TRUE((true == value.isObject() && expectedItr != expectedLinkMeta.end() &&
                     linkMeta.init(server.server(), value, false, error) &&
                     expectedItr->second == linkMeta));
        expectedLinkMeta.erase(expectedItr);
      }
    }

    // for persistence
    {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      view->properties(builder, arangodb::LogicalDataSource::Serialization::Persistence);
      builder.close();

      auto slice = builder.slice();
      arangodb::iresearch::IResearchViewMeta meta;
      arangodb::iresearch::IResearchViewMetaState metaState;
      std::string error;

      EXPECT_TRUE(slice.isObject());
      EXPECT_EQ(19, slice.length());
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
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
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
      view->properties(builder, arangodb::LogicalDataSource::Serialization::Properties);
      builder.close();

      auto slice = builder.slice();
      arangodb::iresearch::IResearchViewMeta meta;
      arangodb::iresearch::IResearchViewMetaState metaState;
      std::string error;

      EXPECT_TRUE(slice.isObject());
      EXPECT_EQ(15, slice.length());
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
      view->properties(builder, arangodb::LogicalDataSource::Serialization::Persistence);
      builder.close();

      auto slice = builder.slice();
      arangodb::iresearch::IResearchViewMeta meta;
      arangodb::iresearch::IResearchViewMetaState metaState;
      std::string error;

      EXPECT_TRUE(slice.isObject());
      EXPECT_EQ(19, slice.length());
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
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
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
      auto beforeRecovery = StorageEngineMock::recoveryStateResult;
      StorageEngineMock::recoveryStateResult = arangodb::RecoveryState::IN_PROGRESS;
      auto restoreRecovery = irs::make_finally([&beforeRecovery]()->void { StorageEngineMock::recoveryStateResult = beforeRecovery; });
      EXPECT_TRUE((view->properties(updateJson->slice(), true).ok()));
      EXPECT_TRUE((true == persisted)); // link addition does not persist view meta

      arangodb::velocypack::Builder builder;

      builder.openObject();
      view->properties(builder, arangodb::LogicalDataSource::Serialization::Properties);
      builder.close();

      auto slice = builder.slice();
      EXPECT_TRUE(slice.isObject());
      EXPECT_EQ(15, slice.length());
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

      auto before = StorageEngineMock::recoveryStateResult;
      StorageEngineMock::recoveryStateResult = arangodb::RecoveryState::IN_PROGRESS;
      auto restore = irs::make_finally([&before]()->void { StorageEngineMock::recoveryStateResult = before; });
      persisted = false;
      EXPECT_TRUE((view->properties(updateJson->slice(), true).ok()));
      EXPECT_TRUE((false == persisted));

      arangodb::velocypack::Builder builder;

      builder.openObject();
      view->properties(builder, arangodb::LogicalDataSource::Serialization::Properties);
      builder.close();

      auto slice = builder.slice();
      EXPECT_TRUE(slice.isObject());
      EXPECT_EQ(15, slice.length());
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
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
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
        view->properties(builder, arangodb::LogicalDataSource::Serialization::Properties);
        builder.close();

        auto slice = builder.slice();
        arangodb::iresearch::IResearchViewMeta meta;
        arangodb::iresearch::IResearchViewMetaState metaState;
        std::string error;

        EXPECT_TRUE(slice.isObject());
        EXPECT_EQ(15, slice.length());
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
        view->properties(builder, arangodb::LogicalDataSource::Serialization::Persistence);
        builder.close();

        auto slice = builder.slice();
        arangodb::iresearch::IResearchViewMeta meta;
        arangodb::iresearch::IResearchViewMetaState metaState;
        std::string error;

        EXPECT_TRUE(slice.isObject());
        EXPECT_EQ(19, slice.length());
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
        view->properties(builder, arangodb::LogicalDataSource::Serialization::Properties);
        builder.close();

        auto slice = builder.slice();
        arangodb::iresearch::IResearchViewMeta meta;
        arangodb::iresearch::IResearchViewMetaState metaState;
        std::string error;

        EXPECT_TRUE(slice.isObject());
        EXPECT_EQ(15, slice.length());
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
        view->properties(builder, arangodb::LogicalDataSource::Serialization::Persistence);
        builder.close();

        auto slice = builder.slice();
        arangodb::iresearch::IResearchViewMeta meta;
        arangodb::iresearch::IResearchViewMetaState metaState;
        std::string error;

        EXPECT_TRUE(slice.isObject());
        EXPECT_EQ(19, slice.length());
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
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
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
      view->properties(builder, arangodb::LogicalDataSource::Serialization::Properties);
      builder.close();

      auto slice = builder.slice();
      arangodb::iresearch::IResearchViewMeta meta;
      arangodb::iresearch::IResearchViewMetaState metaState;
      std::string error;

      EXPECT_TRUE(slice.isObject());
      EXPECT_EQ(15, slice.length());
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
      view->properties(builder, arangodb::LogicalDataSource::Serialization::Persistence);
      builder.close();

      auto slice = builder.slice();
      arangodb::iresearch::IResearchViewMeta meta;
      arangodb::iresearch::IResearchViewMetaState metaState;
      std::string error;

      EXPECT_TRUE(slice.isObject());
      EXPECT_EQ(19, slice.length());
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
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
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
      view->properties(builder, arangodb::LogicalDataSource::Serialization::Properties);
      builder.close();

      auto slice = builder.slice();
      arangodb::iresearch::IResearchViewMeta meta;
      arangodb::iresearch::IResearchViewMetaState metaState;
      std::string error;

      EXPECT_TRUE(slice.isObject());
      EXPECT_EQ(15, slice.length());
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
      view->properties(builder, arangodb::LogicalDataSource::Serialization::Persistence);
      builder.close();

      auto slice = builder.slice();
      arangodb::iresearch::IResearchViewMeta meta;
      arangodb::iresearch::IResearchViewMetaState metaState;
      std::string error;

      EXPECT_TRUE(slice.isObject());
      EXPECT_EQ(19, slice.length());
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
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
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
        view->properties(builder, arangodb::LogicalDataSource::Serialization::Properties);
        builder.close();

        auto slice = builder.slice();
        EXPECT_TRUE(slice.isObject());
        EXPECT_EQ(15, slice.length());
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
        view->properties(builder, arangodb::LogicalDataSource::Serialization::Persistence);
        builder.close();

        auto slice = builder.slice();
        EXPECT_TRUE(slice.isObject());
        EXPECT_EQ(19, slice.length());
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
      std::unordered_set<arangodb::IndexId> initial;

      for (auto& idx: logicalCollection->getIndexes()) {
        initial.emplace(idx->id());
      }

      EXPECT_TRUE((!initial.empty()));
      EXPECT_TRUE((view->properties(updateJson->slice(), true).ok()));

      // not for persistence
      {
        arangodb::velocypack::Builder builder;

        builder.openObject();
        view->properties(builder, arangodb::LogicalDataSource::Serialization::Properties);
        builder.close();

        auto slice = builder.slice();
        EXPECT_TRUE(slice.isObject());
        EXPECT_EQ(15, slice.length());
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
        view->properties(builder, arangodb::LogicalDataSource::Serialization::Persistence);
        builder.close();

        auto slice = builder.slice();
        EXPECT_TRUE(slice.isObject());
        EXPECT_EQ(19, slice.length());
        EXPECT_TRUE(slice.get("name").copyString() == "testView");
        EXPECT_TRUE(slice.get("type").copyString() == arangodb::iresearch::DATA_SOURCE_TYPE.name());
        EXPECT_TRUE((slice.hasKey("deleted") && slice.get("deleted").isBoolean() && false == slice.get("deleted").getBoolean())); // has system properties
        auto tmpSlice = slice.get("collections");
        EXPECT_TRUE((true == tmpSlice.isArray() && 1 == tmpSlice.length()));
        EXPECT_TRUE((false == slice.hasKey("links")));
      }

      std::unordered_set<arangodb::IndexId> actual;

      for (auto& index: logicalCollection->getIndexes()) {
        actual.emplace(index->id());
      }

      EXPECT_TRUE((initial != actual)); // a reindexing took place (link recreated)
    }
  }

  // update existing link (partial update)
  {
    Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
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
        view->properties(builder, arangodb::LogicalDataSource::Serialization::Properties);
        builder.close();

        auto slice = builder.slice();
        EXPECT_TRUE(slice.isObject());
        EXPECT_EQ(15, slice.length());
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
        view->properties(builder, arangodb::LogicalDataSource::Serialization::Persistence);
        builder.close();

        auto slice = builder.slice();
        EXPECT_TRUE(slice.isObject());
        EXPECT_EQ(19, slice.length());
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
        view->properties(builder, arangodb::LogicalDataSource::Serialization::Properties);
        builder.close();

        auto slice = builder.slice();
        EXPECT_TRUE(slice.isObject());
        EXPECT_EQ(15, slice.length());
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
        view->properties(builder, arangodb::LogicalDataSource::Serialization::Persistence);
        builder.close();

        auto slice = builder.slice();
        EXPECT_TRUE(slice.isObject());
        EXPECT_EQ(19, slice.length());
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

    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    ASSERT_TRUE((nullptr != logicalCollection));
    auto logicalView = vocbase.createView(viewCreateJson->slice());
    ASSERT_TRUE((false == !logicalView));

    EXPECT_TRUE((true == logicalCollection->getIndexes().empty()));
    EXPECT_TRUE((true == logicalView->visitCollections(
                             [](arangodb::DataSourceId) -> bool { return false; })));

    // initial link creation
    {
      auto updateJson = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection\": {} } }");
      EXPECT_TRUE((logicalView->properties(updateJson->slice(), true).ok()));
      EXPECT_TRUE((false == logicalCollection->getIndexes().empty()));
      EXPECT_TRUE((false == logicalView->visitCollections([](arangodb::DataSourceId) -> bool {
        return false;
      })));
    }

    struct ExecContext: public arangodb::ExecContext {
      ExecContext(): arangodb::ExecContext(arangodb::ExecContext::Type::Default, "", "",
                                           arangodb::auth::Level::NONE, arangodb::auth::Level::NONE, false) {}
    } execContext;
    arangodb::ExecContextScope execContextScope(&execContext);
    auto* authFeature = arangodb::AuthenticationFeature::instance();
    auto* userManager = authFeature->userManager();
    
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
      logicalView->properties(builder, arangodb::LogicalDataSource::Serialization::Persistence); // 'forPersistence' to avoid auth check
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
      logicalView->properties(builder, arangodb::LogicalDataSource::Serialization::Persistence); // 'forPersistence' to avoid auth check
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

    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    ASSERT_TRUE((nullptr != logicalCollection));
    auto logicalView = vocbase.createView(viewCreateJson->slice());
    ASSERT_TRUE((false == !logicalView));

    EXPECT_TRUE((true == logicalCollection->getIndexes().empty()));
    EXPECT_TRUE((true == logicalView->visitCollections(
                             [](arangodb::DataSourceId) -> bool { return false; })));

    struct ExecContext: public arangodb::ExecContext {
      ExecContext(): arangodb::ExecContext(arangodb::ExecContext::Type::Default, "", "",
                                           arangodb::auth::Level::NONE, arangodb::auth::Level::NONE, false) {}
    } execContext;
    arangodb::ExecContextScope scope(&execContext);
    auto* authFeature = arangodb::AuthenticationFeature::instance();
    auto* userManager = authFeature->userManager();
    arangodb::auth::UserMap userMap; // empty map, no user -> no permissions
    userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database
    auto resetUserManager = irs::make_finally([userManager]()->void{ userManager->removeAllUsers(); });

    EXPECT_TRUE((TRI_ERROR_FORBIDDEN == logicalView->properties(viewUpdateJson->slice(), false).errorNumber()));
    EXPECT_TRUE((true == logicalCollection->getIndexes().empty()));
    EXPECT_TRUE((true == logicalView->visitCollections(
                             [](arangodb::DataSourceId) -> bool { return false; })));
  }

  // drop link (collection not authorized)
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\", \"id\": 100 }");
    auto viewCreateJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
    auto viewUpdateJson = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection\": null } }");

    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
    auto logicalCollection = vocbase.createCollection(collectionJson->slice());
    ASSERT_TRUE((nullptr != logicalCollection));
    auto logicalView = vocbase.createView(viewCreateJson->slice());
    ASSERT_TRUE((false == !logicalView));

    EXPECT_TRUE((true == logicalCollection->getIndexes().empty()));
    EXPECT_TRUE((true == logicalView->visitCollections(
                             [](arangodb::DataSourceId) -> bool { return false; })));

    // initial link creation
    {
      auto updateJson = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection\": {} } }");
      EXPECT_TRUE((logicalView->properties(updateJson->slice(), true).ok()));
      EXPECT_TRUE((false == logicalCollection->getIndexes().empty()));
      EXPECT_TRUE((false == logicalView->visitCollections([](arangodb::DataSourceId) -> bool {
        return false;
      })));
    }

    struct ExecContext: public arangodb::ExecContext {
      ExecContext(): arangodb::ExecContext(arangodb::ExecContext::Type::Default, "", "",
                                           arangodb::auth::Level::NONE, arangodb::auth::Level::NONE, false) {}
    } execContext;
    arangodb::ExecContextScope execContextScope(&execContext);
    auto* authFeature = arangodb::AuthenticationFeature::instance();
    auto* userManager = authFeature->userManager();
    
    auto resetUserManager = std::shared_ptr<arangodb::auth::UserManager>(userManager, [](arangodb::auth::UserManager* ptr)->void { ptr->removeAllUsers(); });

    // subsequent update (overwrite) not authorised (NONE collection)
    {
      arangodb::auth::UserMap userMap;
      auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
      user.grantCollection(vocbase.name(), "testCollection", arangodb::auth::Level::NONE); // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

      EXPECT_TRUE((TRI_ERROR_FORBIDDEN == logicalView->properties(viewUpdateJson->slice(), true).errorNumber()));
      EXPECT_TRUE((false == logicalCollection->getIndexes().empty()));
      EXPECT_TRUE((false == logicalView->visitCollections([](arangodb::DataSourceId) -> bool {
        return false;
      })));
    }

    // subsequent update (overwrite) authorised (RO collection)
    {
      arangodb::auth::UserMap userMap;
      auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
      user.grantCollection(vocbase.name(), "testCollection", arangodb::auth::Level::RO); // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

      EXPECT_TRUE((logicalView->properties(viewUpdateJson->slice(), true).ok()));
      EXPECT_TRUE((true == logicalCollection->getIndexes().empty()));
      EXPECT_TRUE((true == logicalView->visitCollections([](arangodb::DataSourceId) -> bool {
        return false;
      })));
    }
  }

  // add authorised link (existing collection not authorized)
  {
    auto collection0Json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection0\", \"id\": 100 }");
    auto collection1Json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection1\", \"id\": 101 }");
    auto viewCreateJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
    auto viewUpdateJson = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection1\": {} } }");

    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
    auto logicalCollection0 = vocbase.createCollection(collection0Json->slice());
    ASSERT_TRUE((nullptr != logicalCollection0));
    auto logicalCollection1 = vocbase.createCollection(collection1Json->slice());
    ASSERT_TRUE((nullptr != logicalCollection1));
    auto logicalView = vocbase.createView(viewCreateJson->slice());
    ASSERT_TRUE((false == !logicalView));

    EXPECT_TRUE((true == logicalCollection0->getIndexes().empty()));
    EXPECT_TRUE((true == logicalCollection1->getIndexes().empty()));
    EXPECT_TRUE((true == logicalView->visitCollections(
                             [](arangodb::DataSourceId) -> bool { return false; })));

    // initial link creation
    {
      auto updateJson = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection0\": {} } }");
      EXPECT_TRUE((logicalView->properties(updateJson->slice(), true).ok()));
      EXPECT_TRUE((false == logicalCollection0->getIndexes().empty()));
      EXPECT_TRUE((true == logicalCollection1->getIndexes().empty()));
      EXPECT_TRUE((false == logicalView->visitCollections([](arangodb::DataSourceId) -> bool {
        return false;
      })));
    }

    struct ExecContext: public arangodb::ExecContext {
      ExecContext(): arangodb::ExecContext(arangodb::ExecContext::Type::Default, "", "",
                                           arangodb::auth::Level::NONE, arangodb::auth::Level::NONE, false) {}
    } execContext;
    arangodb::ExecContextScope execContextScope(&execContext);
    auto* authFeature = arangodb::AuthenticationFeature::instance();
    auto* userManager = authFeature->userManager();
    
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
      EXPECT_TRUE((false == logicalView->visitCollections([](arangodb::DataSourceId) -> bool {
        return false;
      })));
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
      EXPECT_TRUE((false == logicalView->visitCollections([](arangodb::DataSourceId) -> bool {
        return false;
      })));
    }
  }

  // drop authorised link (existing collection not authorized)
  {
    auto collection0Json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection0\", \"id\": 100 }");
    auto collection1Json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection1\", \"id\": 101 }");
    auto viewCreateJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
    auto viewUpdateJson = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection1\": null } }");

    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
    auto logicalCollection0 = vocbase.createCollection(collection0Json->slice());
    ASSERT_TRUE((nullptr != logicalCollection0));
    auto logicalCollection1 = vocbase.createCollection(collection1Json->slice());
    ASSERT_TRUE((nullptr != logicalCollection1));
    auto logicalView = vocbase.createView(viewCreateJson->slice());
    ASSERT_TRUE((false == !logicalView));

    EXPECT_TRUE((true == logicalCollection0->getIndexes().empty()));
    EXPECT_TRUE((true == logicalCollection1->getIndexes().empty()));
    EXPECT_TRUE((true == logicalView->visitCollections(
                             [](arangodb::DataSourceId) -> bool { return false; })));

    // initial link creation
    {
      auto updateJson = arangodb::velocypack::Parser::fromJson("{ \"links\": { \"testCollection0\": {}, \"testCollection1\": {} } }");
      EXPECT_TRUE((logicalView->properties(updateJson->slice(), true).ok()));
      EXPECT_TRUE((false == logicalCollection0->getIndexes().empty()));
      EXPECT_TRUE((false == logicalCollection1->getIndexes().empty()));
      EXPECT_TRUE((false == logicalView->visitCollections([](arangodb::DataSourceId) -> bool {
        return false;
      })));
    }

    struct ExecContext: public arangodb::ExecContext {
      ExecContext(): arangodb::ExecContext(arangodb::ExecContext::Type::Default, "", "",
                                           arangodb::auth::Level::NONE, arangodb::auth::Level::NONE, false) {}
    } execContext;
    arangodb::ExecContextScope execContextScope(&execContext);
    auto* authFeature = arangodb::AuthenticationFeature::instance();
    auto* userManager = authFeature->userManager();
    
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
      EXPECT_TRUE((false == logicalView->visitCollections([](arangodb::DataSourceId) -> bool {
        return false;
      })));
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
      EXPECT_TRUE((false == logicalView->visitCollections([](arangodb::DataSourceId) -> bool {
        return false;
      })));
    }
  }
}

TEST_F(IResearchViewTest, test_remove_referenced_analyzer) {
  auto& databaseFeature = server.server().getFeature<arangodb::DatabaseFeature>();

  TRI_vocbase_t* vocbase; // will be owned by DatabaseFeature
  arangodb::CreateDatabaseInfo testDBInfo(server.server(),
                                          arangodb::ExecContext::current());
  testDBInfo.load("testDatabase" TOSTRING(__LINE__), 3);
  ASSERT_TRUE(databaseFeature.createDatabase(std::move(testDBInfo), vocbase).ok());
  ASSERT_NE(nullptr, vocbase);

  // create _analyzers collection
  {
    auto createJson = arangodb::velocypack::Parser::fromJson(
                        "{ \"name\": \"" +  arangodb::StaticStrings::AnalyzersCollection + "\", \"isSystem\":true }");
    ASSERT_NE(nullptr, vocbase->createCollection(createJson->slice()));
  }

  auto& analyzers = server.server().getFeature<arangodb::iresearch::IResearchAnalyzerFeature>();

  std::shared_ptr<arangodb::LogicalView> view;
  std::shared_ptr<arangodb::LogicalCollection> collection;

  // remove existing (used by link)
  {
    // add analyzer
    {
      arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;
      ASSERT_TRUE(analyzers.emplace(result, vocbase->name() + "::test_analyzer3",
                                    "TestAnalyzer", VPackParser::fromJson("\"abc\"")->slice()).ok());
      ASSERT_NE(nullptr, analyzers.get(vocbase->name() + "::test_analyzer3", arangodb::QueryAnalyzerRevisions::QUERY_LATEST));
    }

    // create collection
    {
      auto createJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection1\" }");
      collection = vocbase->createCollection(createJson->slice());
      ASSERT_NE(nullptr, collection);
    }

    // create view
    {
      auto createJson = arangodb::velocypack::Parser::fromJson(
            "{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
      view = vocbase->createView(createJson->slice());
      ASSERT_NE(nullptr, view);

      auto updateJson = arangodb::velocypack::Parser::fromJson(
        "{ \"links\": { \"testCollection1\": { \"includeAllFields\": true, \"analyzers\":[\"test_analyzer3\"] }}}");
      EXPECT_TRUE(view->properties(updateJson->slice(), true).ok());
    }

    EXPECT_FALSE(analyzers.remove(vocbase->name() + "::test_analyzer3", false).ok()); // used by link
    EXPECT_NE(nullptr, analyzers.get(vocbase->name() + "::test_analyzer3", arangodb::QueryAnalyzerRevisions::QUERY_LATEST));
    EXPECT_TRUE(analyzers.remove(vocbase->name() + "::test_analyzer3", true).ok());
    EXPECT_EQ(nullptr, analyzers.get(vocbase->name() + "::test_analyzer3", arangodb::QueryAnalyzerRevisions::QUERY_LATEST));

    auto cleanup = arangodb::scopeGuard([vocbase, &view, &collection]() {
      if (view) {
        EXPECT_TRUE(vocbase->dropView(view->id(), false).ok());
        view = nullptr;
      }

      if (collection) {
        EXPECT_TRUE(vocbase->dropCollection(collection->id(), false, 1.0).ok());
        collection = nullptr;
      }
    });
  }

  // remove existing (used by link)
  {
    // add analyzer
    {
      arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;
      ASSERT_TRUE(analyzers.emplace(result, vocbase->name() + "::test_analyzer3",
                                    "TestAnalyzer", VPackParser::fromJson("\"abc\"")->slice()).ok());
      ASSERT_NE(nullptr, analyzers.get(vocbase->name() + "::test_analyzer3", arangodb::QueryAnalyzerRevisions::QUERY_LATEST));
    }

    // create collection
    {
      auto createJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection1\" }");
      collection = vocbase->createCollection(createJson->slice());
      ASSERT_NE(nullptr, collection);
    }

    // create view
    {
      auto createJson = arangodb::velocypack::Parser::fromJson(
            "{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
      view = vocbase->createView(createJson->slice());
      ASSERT_NE(nullptr, view);

      auto updateJson = arangodb::velocypack::Parser::fromJson(
        "{ \"analyzerDefinitions\" : { \
             \"name\":\"test_analyzer3\", \"features\":[], \
             \"type\":\"TestAnalyzer\", \"properties\": {\"args\":\"abc\"} \
           }, \
           \"links\": { \"testCollection1\": { \"includeAllFields\": true, \"analyzers\":[\"test_analyzer3\"] }} \
        }");
      EXPECT_TRUE(view->properties(updateJson->slice(), true).ok());
    }

    EXPECT_FALSE(analyzers.remove(vocbase->name() + "::test_analyzer3", false).ok()); // used by link
    EXPECT_NE(nullptr, analyzers.get(vocbase->name() + "::test_analyzer3", arangodb::QueryAnalyzerRevisions::QUERY_LATEST));
    EXPECT_TRUE(analyzers.remove(vocbase->name() + "::test_analyzer3", true).ok());
    EXPECT_EQ(nullptr, analyzers.get(vocbase->name() + "::test_analyzer3", arangodb::QueryAnalyzerRevisions::QUERY_LATEST));

    auto cleanup = arangodb::scopeGuard([vocbase, &view, &collection]() {
      if (view) {
        EXPECT_TRUE(vocbase->dropView(view->id(), false).ok());
        view = nullptr;
      }

      if (collection) {
        EXPECT_TRUE(vocbase->dropCollection(collection->id(), false, 1.0).ok());
        collection = nullptr;
      }
    });
  }

  // remove existing (properties don't match
  {
    // add analyzer
    {
      arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;
      ASSERT_TRUE(analyzers.emplace(result, vocbase->name() + "::test_analyzer3",
                                    "TestAnalyzer", VPackParser::fromJson("\"abcd\"")->slice()).ok());
      ASSERT_NE(nullptr, analyzers.get(vocbase->name() + "::test_analyzer3",
                                       arangodb::QueryAnalyzerRevisions::QUERY_LATEST));
    }

    // create collection
    {
      auto createJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection1\" }");
      collection = vocbase->createCollection(createJson->slice());
      ASSERT_NE(nullptr, collection);
    }

    // create view
    {
      auto createJson = arangodb::velocypack::Parser::fromJson(
            "{ \"name\": \"testView\", \"type\": \"arangosearch\" }");
      view = vocbase->createView(createJson->slice());
      ASSERT_NE(nullptr, view);

      auto updateJson = arangodb::velocypack::Parser::fromJson(
        "{ \"analyzerDefinitions\" : { \
             \"name\":\"test_analyzer3\", \"features\":[], \
             \"type\":\"TestAnalyzer\", \"properties\": \"abc\" \
           }, \
           \"links\": { \"testCollection1\": { \"includeAllFields\": true, \"analyzers\":[\"test_analyzer3\"] }} \
        }");
      EXPECT_TRUE(view->properties(updateJson->slice(), true).ok());
    }

    EXPECT_FALSE(analyzers.remove(vocbase->name() + "::test_analyzer3", false).ok()); // used by link (we ignore analyzerDefinitions on single-server)
    EXPECT_NE(nullptr, analyzers.get(vocbase->name() + "::test_analyzer3", arangodb::QueryAnalyzerRevisions::QUERY_LATEST));
    EXPECT_TRUE(analyzers.remove(vocbase->name() + "::test_analyzer3", true).ok());
    EXPECT_EQ(nullptr, analyzers.get(vocbase->name() + "::test_analyzer3", arangodb::QueryAnalyzerRevisions::QUERY_LATEST));

    auto cleanup = arangodb::scopeGuard([vocbase, &view, &collection]() {
      if (view) {
        EXPECT_TRUE(vocbase->dropView(view->id(), false).ok());
        view = nullptr;
      }

      if (collection) {
        EXPECT_TRUE(vocbase->dropCollection(collection->id(), false, 1.0).ok());
        collection = nullptr;
      }
    });
  }
}

TEST_F(IResearchViewTest, create_view_with_stored_value) {
  // default
  {
    auto json = arangodb::velocypack::Parser::fromJson(
          "{ "
          "  \"name\": \"testView\", "
          "  \"type\": \"arangosearch\", "
          "  \"storedValues\": [ "
          "    [\"obj.a\"], [\"obj.b.b1\"], [\"\"], [], [\"\"], "
          "    [\"obj.c\", \"\", \"obj.d\"], [\"obj.e\", \"obj.f.f1\", \"obj.g\"] ] "
          "} ");
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
    arangodb::LogicalView::ptr view;
    EXPECT_TRUE((arangodb::iresearch::IResearchView::factory().create(view, vocbase, json->slice()).ok()));
    EXPECT_FALSE(!view);

    arangodb::velocypack::Builder builder;
    builder.openObject();
    view->properties(builder, arangodb::LogicalDataSource::Serialization::Persistence);
    builder.close();
    auto slice = builder.slice();
    arangodb::iresearch::IResearchViewMeta meta;
    std::string error;
    EXPECT_EQ(19, slice.length());
    EXPECT_EQ("testView", slice.get("name").copyString());
    EXPECT_TRUE(meta.init(slice, error));
    ASSERT_EQ(4, meta._storedValues.columns().size());
    EXPECT_EQ(1, meta._storedValues.columns()[0].fields.size());
    EXPECT_EQ(irs::type<irs::compression::lz4>::id(), meta._storedValues.columns()[0].compression);
    EXPECT_EQ(arangodb::iresearch::IResearchViewStoredValues::FIELDS_DELIMITER + std::string("obj.a"), meta._storedValues.columns()[0].name);
    EXPECT_EQ(1, meta._storedValues.columns()[1].fields.size());
    EXPECT_EQ(irs::type<irs::compression::lz4>::id(), meta._storedValues.columns()[1].compression);
    EXPECT_EQ(arangodb::iresearch::IResearchViewStoredValues::FIELDS_DELIMITER + std::string("obj.b.b1"), meta._storedValues.columns()[1].name);
    EXPECT_EQ(2, meta._storedValues.columns()[2].fields.size());
    EXPECT_EQ(irs::type<irs::compression::lz4>::id(), meta._storedValues.columns()[2].compression);
    EXPECT_EQ(arangodb::iresearch::IResearchViewStoredValues::FIELDS_DELIMITER + std::string("obj.c") +
              arangodb::iresearch::IResearchViewStoredValues::FIELDS_DELIMITER + "obj.d",
              meta._storedValues.columns()[2].name);
    EXPECT_EQ(3, meta._storedValues.columns()[3].fields.size());
    EXPECT_EQ(irs::type<irs::compression::lz4>::id(), meta._storedValues.columns()[3].compression);
    EXPECT_EQ(arangodb::iresearch::IResearchViewStoredValues::FIELDS_DELIMITER + std::string("obj.e") +
              arangodb::iresearch::IResearchViewStoredValues::FIELDS_DELIMITER + "obj.f.f1" +
              arangodb::iresearch::IResearchViewStoredValues::FIELDS_DELIMITER + "obj.g", meta._storedValues.columns()[3].name);
  }

  // repeated fields and columns
  {
    auto json = arangodb::velocypack::Parser::fromJson(
          "{ "
          "  \"name\": \"testView\", "
          "  \"type\": \"arangosearch\", "
          "  \"storedValues\": [ "
          "    [\"obj.a\"], [\"obj.a\"], [\"obj.b\"], [\"obj.c\"], [\"obj.d\"], "
          "    [\"obj.d\"], [\"obj.c.c1\", \"obj.c\", \"obj.c\", \"obj.d\", \"obj.c.c2\"], [\"obj.b\", \"obj.b\"] ] "
          "} ");
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
    arangodb::LogicalView::ptr view;
    EXPECT_TRUE((arangodb::iresearch::IResearchView::factory().create(view, vocbase, json->slice()).ok()));
    EXPECT_FALSE(!view);

    arangodb::velocypack::Builder builder;
    builder.openObject();
    view->properties(builder, arangodb::LogicalDataSource::Serialization::Persistence);
    builder.close();
    auto slice = builder.slice();
    arangodb::iresearch::IResearchViewMeta meta;
    std::string error;
    EXPECT_EQ(19, slice.length());
    EXPECT_EQ("testView", slice.get("name").copyString());
    EXPECT_TRUE(meta.init(slice, error));
    ASSERT_EQ(5, meta._storedValues.columns().size());
    EXPECT_EQ(1, meta._storedValues.columns()[0].fields.size());
    EXPECT_EQ(irs::type<irs::compression::lz4>::id(), meta._storedValues.columns()[0].compression);
    EXPECT_EQ(arangodb::iresearch::IResearchViewStoredValues::FIELDS_DELIMITER + std::string("obj.a"), meta._storedValues.columns()[0].name);
    EXPECT_EQ(1, meta._storedValues.columns()[1].fields.size());
    EXPECT_EQ(irs::type<irs::compression::lz4>::id(), meta._storedValues.columns()[1].compression);
    EXPECT_EQ(arangodb::iresearch::IResearchViewStoredValues::FIELDS_DELIMITER + std::string("obj.b"), meta._storedValues.columns()[1].name);
    EXPECT_EQ(1, meta._storedValues.columns()[2].fields.size());
    EXPECT_EQ(irs::type<irs::compression::lz4>::id(), meta._storedValues.columns()[2].compression);
    EXPECT_EQ(arangodb::iresearch::IResearchViewStoredValues::FIELDS_DELIMITER + std::string("obj.c"), meta._storedValues.columns()[2].name);
    EXPECT_EQ(1, meta._storedValues.columns()[3].fields.size());
    EXPECT_EQ(irs::type<irs::compression::lz4>::id(), meta._storedValues.columns()[3].compression);
    EXPECT_EQ(arangodb::iresearch::IResearchViewStoredValues::FIELDS_DELIMITER + std::string("obj.d"), meta._storedValues.columns()[3].name);
    EXPECT_EQ(2, meta._storedValues.columns()[4].fields.size());
    EXPECT_EQ(irs::type<irs::compression::lz4>::id(), meta._storedValues.columns()[4].compression);
    EXPECT_EQ(arangodb::iresearch::IResearchViewStoredValues::FIELDS_DELIMITER + std::string("obj.c") +
              arangodb::iresearch::IResearchViewStoredValues::FIELDS_DELIMITER + "obj.d",
              meta._storedValues.columns()[4].name);
  }
}

TEST_F(IResearchViewTest, create_view_with_stored_value_with_compression) {
    auto json = arangodb::velocypack::Parser::fromJson(
      "{ "
      "  \"name\": \"testView\", "
      "  \"type\": \"arangosearch\", "
      "  \"storedValues\": [ "
      "    {\"fields\":[\"obj.a\"], \"compression\":\"none\"} , {\"fields\":[\"obj.b.b1\"], \"compression\":\"lz4\"} ] "
      "} ");
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
    arangodb::LogicalView::ptr view;
    EXPECT_TRUE((arangodb::iresearch::IResearchView::factory().create(view, vocbase, json->slice()).ok()));
    EXPECT_FALSE(!view);

    arangodb::velocypack::Builder builder;
    builder.openObject();
    view->properties(builder, arangodb::LogicalDataSource::Serialization::Persistence);
    builder.close();
    auto slice = builder.slice();
    arangodb::iresearch::IResearchViewMeta meta;
    std::string error;
    EXPECT_EQ(19, slice.length());
    EXPECT_EQ("testView", slice.get("name").copyString());
    EXPECT_TRUE(meta.init(slice, error));
    ASSERT_EQ(2, meta._storedValues.columns().size());
    EXPECT_EQ(1, meta._storedValues.columns()[0].fields.size());
    EXPECT_EQ(irs::type<irs::compression::none>::id(), meta._storedValues.columns()[0].compression);
    EXPECT_EQ(arangodb::iresearch::IResearchViewStoredValues::FIELDS_DELIMITER + std::string("obj.a"), meta._storedValues.columns()[0].name);
    EXPECT_EQ(1, meta._storedValues.columns()[1].fields.size());
    EXPECT_EQ(irs::type<irs::compression::lz4>::id(), meta._storedValues.columns()[1].compression);
    EXPECT_EQ(arangodb::iresearch::IResearchViewStoredValues::FIELDS_DELIMITER + std::string("obj.b.b1"), meta._storedValues.columns()[1].name);
}
