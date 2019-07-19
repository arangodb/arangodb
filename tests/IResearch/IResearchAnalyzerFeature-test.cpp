////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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

#include "../Mocks/StorageEngineMock.h"
#include "ClusterCommMock.h"
#include "RestHandlerMock.h"

#include "common.h"

#include "analysis/analyzers.hpp"
#include "analysis/token_attributes.hpp"
#include "utils/utf8_path.hpp"

#include "Agency/Store.h"
#include "ApplicationFeatures/BasicPhase.h"
#include "ApplicationFeatures/ClusterPhase.h"
#include "ApplicationFeatures/CommunicationPhase.h"
#include "ApplicationFeatures/DatabasePhase.h"
#include "ApplicationFeatures/GreetingsPhase.h"
#include "ApplicationFeatures/V8Phase.h"
#include "Aql/AqlFunctionFeature.h"
#include "Aql/OptimizerRulesFeature.h"
#include "Aql/QueryRegistry.h"
#include "Basics/files.h"
#include "Cluster/ClusterComm.h"
#include "Cluster/ClusterFeature.h"

#if USE_ENTERPRISE
#include "Enterprise/Ldap/LdapFeature.h"
#endif

#include "GeneralServer/AuthenticationFeature.h"
#include "IResearch/AgencyMock.h"
#include "IResearch/IResearchAnalyzerFeature.h"
#include "IResearch/IResearchCommon.h"
#include "IResearch/IResearchFeature.h"
#include "IResearch/VelocyPackHelper.h"
#include "Indexes/IndexFactory.h"
#include "Random/RandomFeature.h"
#include "RestServer/AqlFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/DatabasePathFeature.h"
#include "RestServer/FlushFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "RestServer/SystemDatabaseFeature.h"
#include "RestServer/TraverserEngineRegistryFeature.h"
#include "RestServer/UpgradeFeature.h"
#include "RestServer/ViewTypesFeature.h"
#include "RestServer/VocbaseContext.h"
#include "Scheduler/SchedulerFeature.h"
#include "Sharding/ShardingFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/ExecContext.h"
#include "Utils/OperationOptions.h"
#include "Utils/SingleCollectionTransaction.h"
#include "V8Server/V8DealerFeature.h"
#include "VocBase/KeyGenerator.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ManagedDocumentResult.h"
#include "VocBase/Methods/Collections.h"
#include "VocBase/Methods/Indexes.h"
#include "velocypack/Slice.h"

namespace {

struct TestIndex : public arangodb::Index {
  TestIndex(TRI_idx_iid_t id, arangodb::LogicalCollection& collection,
            arangodb::velocypack::Slice const& definition)
      : arangodb::Index(id, collection, definition) {}
  bool canBeDropped() const override { return false; }
  bool hasSelectivityEstimate() const override { return false; }
  bool isHidden() const override { return false; }
  bool isPersistent() const override { return false; }
  bool isSorted() const override { return false; }
  std::unique_ptr<arangodb::IndexIterator> iteratorForCondition(
      arangodb::transaction::Methods* /* trx */, arangodb::aql::AstNode const* /* node */,
      arangodb::aql::Variable const* /* reference */,
      arangodb::IndexIteratorOptions const& /* opts */) override {
    return nullptr;
  }
  void load() override {}
  size_t memory() const override { return sizeof(Index); }
  arangodb::Index::IndexType type() const override {
    return arangodb::Index::TRI_IDX_TYPE_UNKNOWN;
  }
  char const* typeName() const override { return "testType"; }
  void unload() override {}
};

struct TestAttribute : public irs::attribute {
  DECLARE_ATTRIBUTE_TYPE();
};

DEFINE_ATTRIBUTE_TYPE(TestAttribute);
REGISTER_ATTRIBUTE(TestAttribute);  // required to open reader on segments with analized fields

struct TestTermAttribute : public irs::term_attribute {
 public:
  void value(irs::bytes_ref const& value) { value_ = value; }
};

class TestAnalyzer : public irs::analysis::analyzer {
 public:
  DECLARE_ANALYZER_TYPE();
  TestAnalyzer() : irs::analysis::analyzer(TestAnalyzer::type()) {
    _attrs.emplace(_term);
    _attrs.emplace(_attr);
    _attrs.emplace(_increment);  // required by field_data::invert(...)
  }

  virtual irs::attribute_view const& attributes() const NOEXCEPT override {
    return _attrs;
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

    _term.value(irs::bytes_ref(_data.c_str(), 1));
    _data = irs::bytes_ref(_data.c_str() + 1, _data.size() - 1);
    return true;
  }

  virtual bool reset(irs::string_ref const& data) override {
    _data = irs::ref_cast<irs::byte_type>(data);
    return true;
  }

 private:
  irs::attribute_view _attrs;
  irs::bytes_ref _data;
  irs::increment _increment;
  TestTermAttribute _term;
  TestAttribute _attr;
};

DEFINE_ANALYZER_TYPE_NAMED(TestAnalyzer, "TestAnalyzer");
REGISTER_ANALYZER_VPACK(TestAnalyzer, TestAnalyzer::make, TestAnalyzer::normalize);

struct Analyzer {
  irs::string_ref type;
  VPackSlice properties;
  irs::flags features;

  Analyzer() = default;
  Analyzer(irs::string_ref const& t, irs::string_ref const& p,
           irs::flags const& f = irs::flags::empty_instance())
      : type(t), features(f) {
    if (p.null()) {
      properties = VPackSlice::nullSlice();
    } else {
      propBuilder = VPackParser::fromJson(p);
      properties = propBuilder->slice();
    }
  }

 private:
  // internal VPack storage. Not to be used outside
  std::shared_ptr<VPackBuilder> propBuilder;
};

std::map<irs::string_ref, Analyzer> const& staticAnalyzers() {
  static const std::map<irs::string_ref, Analyzer> analyzers = {
      {"identity",
       {"identity", irs::string_ref::NIL, {irs::frequency::type(), irs::norm::type()}}},
      {"text_de",
       {"text",
        "{ \"locale\": \"de.UTF-8\", \"stopwords\": [ ] "
        "}",
        {irs::frequency::type(), irs::norm::type(), irs::position::type()}}},
      {"text_en",
       {"text",
        "{ \"locale\": \"en.UTF-8\", \"stopwords\": [ ] "
        "}",
        {irs::frequency::type(), irs::norm::type(), irs::position::type()}}},
      {"text_es",
       {"text",
        "{ \"locale\": \"es.UTF-8\", \"stopwords\": [ ] "
        "}",
        {irs::frequency::type(), irs::norm::type(), irs::position::type()}}},
      {"text_fi",
       {"text",
        "{ \"locale\": \"fi.UTF-8\", \"stopwords\": [ ] "
        "}",
        {irs::frequency::type(), irs::norm::type(), irs::position::type()}}},
      {"text_fr",
       {"text",
        "{ \"locale\": \"fr.UTF-8\", \"stopwords\": [ ] "
        "}",
        {irs::frequency::type(), irs::norm::type(), irs::position::type()}}},
      {"text_it",
       {"text",
        "{ \"locale\": \"it.UTF-8\", \"stopwords\": [ ] "
        "}",
        {irs::frequency::type(), irs::norm::type(), irs::position::type()}}},
      {"text_nl",
       {"text",
        "{ \"locale\": \"nl.UTF-8\", \"stopwords\": [ ] "
        "}",
        {irs::frequency::type(), irs::norm::type(), irs::position::type()}}},
      {"text_no",
       {"text",
        "{ \"locale\": \"no.UTF-8\", \"stopwords\": [ ] "
        "}",
        {irs::frequency::type(), irs::norm::type(), irs::position::type()}}},
      {"text_pt",
       {"text",
        "{ \"locale\": \"pt.UTF-8\", \"stopwords\": [ ] "
        "}",
        {irs::frequency::type(), irs::norm::type(), irs::position::type()}}},
      {"text_ru",
       {"text",
        "{ \"locale\": \"ru.UTF-8\", \"stopwords\": [ ] "
        "}",
        {irs::frequency::type(), irs::norm::type(), irs::position::type()}}},
      {"text_sv",
       {"text",
        "{ \"locale\": \"sv.UTF-8\", \"stopwords\": [ ] "
        "}",
        {irs::frequency::type(), irs::norm::type(), irs::position::type()}}},
      {"text_zh",
       {"text",
        "{ \"locale\": \"zh.UTF-8\", \"stopwords\": [ ] "
        "}",
        {irs::frequency::type(), irs::norm::type(), irs::position::type()}}},
  };

  return analyzers;
}

// AqlValue entries must be explicitly deallocated
struct VPackFunctionParametersWrapper {
  arangodb::SmallVector<arangodb::aql::AqlValue>::allocator_type::arena_type arena;
  arangodb::aql::VPackFunctionParameters instance;
  VPackFunctionParametersWrapper() : instance(arena) {}
  ~VPackFunctionParametersWrapper() {
    for (auto& entry : instance) {
      entry.destroy();
    }
  }
  arangodb::aql::VPackFunctionParameters* operator->() { return &instance; }
  arangodb::aql::VPackFunctionParameters& operator*() { return instance; }
};

// AqlValue entrys must be explicitly deallocated
struct AqlValueWrapper {
  arangodb::aql::AqlValue instance;
  AqlValueWrapper(arangodb::aql::AqlValue&& other)
      : instance(std::move(other)) {}
  ~AqlValueWrapper() { instance.destroy(); }
  arangodb::aql::AqlValue* operator->() { return &instance; }
  arangodb::aql::AqlValue& operator*() { return instance; }
};

// a way to set EngineSelectorFeature::ENGINE and nullify it via destructor,
// i.e. only after all TRI_vocbase_t and ApplicationServer have been destroyed
struct StorageEngineWrapper {
  StorageEngineMock instance;
  StorageEngineWrapper(arangodb::application_features::ApplicationServer& server)
      : instance(server) {
    arangodb::EngineSelectorFeature::ENGINE = &instance;
  }
  ~StorageEngineWrapper() { arangodb::EngineSelectorFeature::ENGINE = nullptr; }
  StorageEngineMock* operator->() { return &instance; }
  StorageEngineMock& operator*() { return instance; }
};

}  // namespace

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

class IResearchAnalyzerFeatureTest : public ::testing::Test {
 protected:
  struct ClusterCommControl : arangodb::ClusterComm {
    static void reset() { arangodb::ClusterComm::_theInstanceInit.store(0); }
  };

  arangodb::consensus::Store _agencyStore{nullptr, "arango"};
  GeneralClientConnectionAgencyMock* agency;
  StorageEngineWrapper engine;  // can only nullify 'ENGINE' after all TRI_vocbase_t and ApplicationServer have been destroyed
  arangodb::application_features::ApplicationServer server;
  std::vector<std::pair<arangodb::application_features::ApplicationFeature*, bool>> features;
  arangodb::SystemDatabaseFeature* sysDatabaseFeature{};

  IResearchAnalyzerFeatureTest() : engine(server), server(nullptr, nullptr) {
    auto* agencyCommManager = new AgencyCommManagerMock("arango");
    agency = agencyCommManager->addConnection<GeneralClientConnectionAgencyMock>(_agencyStore);
    agency = agencyCommManager->addConnection<GeneralClientConnectionAgencyMock>(
        _agencyStore);  // need 2 connections or Agency callbacks will fail
    arangodb::AgencyCommManager::MANAGER.reset(agencyCommManager);  // required for Coordinator tests

    arangodb::tests::init();

    // suppress INFO {authentication} Authentication is turned on (system only), authentication for unix sockets is turned on
    // suppress WARNING {authentication} --server.jwt-secret is insecure. Use --server.jwt-secret-keyfile instead
    arangodb::LogTopic::setLogLevel(arangodb::Logger::AUTHENTICATION.name(),
                                    arangodb::LogLevel::ERR);

    // setup required application features
    features.emplace_back(new arangodb::AuthenticationFeature(server), true);
    features.emplace_back(new arangodb::DatabaseFeature(server), false);
    features.emplace_back(new arangodb::ShardingFeature(server), false);
    features.emplace_back(new arangodb::QueryRegistryFeature(server), false);  // required for constructing TRI_vocbase_t
    arangodb::application_features::ApplicationServer::server->addFeature(
        features.back().first);  // need QueryRegistryFeature feature to be added now in order to create the system database
    features.emplace_back(sysDatabaseFeature = new arangodb::SystemDatabaseFeature(server),
                          true);  // required for IResearchAnalyzerFeature
    features.emplace_back(new arangodb::V8DealerFeature(server),
                          false);  // required for DatabaseFeature::createDatabase(...)
    features.emplace_back(new arangodb::aql::AqlFunctionFeature(server), true);  // required for IResearchAnalyzerFeature

#if USE_ENTERPRISE
    features.emplace_back(new arangodb::LdapFeature(server),
                          false);  // required for AuthenticationFeature with USE_ENTERPRISE
#endif

    // required for V8DealerFeature::prepare(), ClusterFeature::prepare() not required
    arangodb::application_features::ApplicationServer::server->addFeature(
        new arangodb::ClusterFeature(server));

    for (auto& f : features) {
      arangodb::application_features::ApplicationServer::server->addFeature(f.first);
    }

    for (auto& f : features) {
      f.first->prepare();
    }

    auto const databases = VPackParser::fromJson(
        std::string("[ { \"name\": \"" + arangodb::StaticStrings::SystemDatabase + "\" } ]"));
    auto* dbFeature =
        arangodb::application_features::ApplicationServer::lookupFeature<arangodb::DatabaseFeature>(
            "Database");
    dbFeature->loadDatabases(databases->slice());

    for (auto& f : features) {
      if (f.second) {
        f.first->start();
      }
    }

    // suppress log messages since tests check error conditions
    arangodb::LogTopic::setLogLevel(arangodb::Logger::AGENCY.name(),
                                    arangodb::LogLevel::FATAL);
    arangodb::LogTopic::setLogLevel(arangodb::Logger::CLUSTER.name(),
                                    arangodb::LogLevel::FATAL);
    arangodb::LogTopic::setLogLevel(arangodb::iresearch::TOPIC.name(),
                                    arangodb::LogLevel::FATAL);
    irs::logger::output_le(iresearch::logger::IRL_FATAL, stderr);

    // Add the authentication user:
    auto* authFeature = arangodb::AuthenticationFeature::instance();
    auto* userManager = authFeature->userManager();
    arangodb::aql::QueryRegistry queryRegistry(0);  // required for UserManager::loadFromDB()
    userManager->setQueryRegistry(&queryRegistry);

    auto vocbase = dbFeature->useDatabase(arangodb::StaticStrings::SystemDatabase);
    arangodb::methods::Collections::createSystem(*vocbase, arangodb::tests::AnalyzerCollectionName);
  }

  ~IResearchAnalyzerFeatureTest() {
    // Clear the authentication user:
    auto* authFeature = arangodb::AuthenticationFeature::instance();
    if (authFeature != nullptr) {
      auto* userManager = authFeature->userManager();
      if (userManager != nullptr) {
        userManager->removeAllUsers();
      }
    }

    arangodb::LogTopic::setLogLevel(arangodb::iresearch::TOPIC.name(),
                                    arangodb::LogLevel::DEFAULT);
    arangodb::LogTopic::setLogLevel(arangodb::Logger::CLUSTER.name(),
                                    arangodb::LogLevel::DEFAULT);
    arangodb::LogTopic::setLogLevel(arangodb::Logger::AGENCY.name(),
                                    arangodb::LogLevel::DEFAULT);
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

    ClusterCommControl::reset();
    arangodb::LogTopic::setLogLevel(arangodb::Logger::AUTHENTICATION.name(),
                                    arangodb::LogLevel::DEFAULT);
    arangodb::AgencyCommManager::MANAGER.reset();
  }

  void userSetAccessLevel(arangodb::auth::Level db, arangodb::auth::Level col) {
    auto* authFeature = arangodb::AuthenticationFeature::instance();
    ASSERT_TRUE(authFeature != nullptr);
    auto* userManager = authFeature->userManager();
    ASSERT_TRUE(userManager != nullptr);
    arangodb::auth::UserMap userMap;
    auto user = arangodb::auth::User::newUser("testUser", "testPW",
                                              arangodb::auth::Source::LDAP);
    user.grantDatabase("testVocbase", db);
    user.grantCollection("testVocbase", "*", col);
    userMap.emplace("testUser", std::move(user));
    userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database
  }

  std::unique_ptr<arangodb::ExecContext> getLoggedInContext() const {
    std::unique_ptr<arangodb::ExecContext> res;
    res.reset(arangodb::ExecContext::create("testUser", "testVocbase"));
    return res;
  }

  std::string analyzerName() const {
    return arangodb::StaticStrings::SystemDatabase + "::test_analyzer";
  }
};

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                         authentication test suite
// -----------------------------------------------------------------------------

TEST_F(IResearchAnalyzerFeatureTest, test_auth_no_auth) {
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                        "testVocbase");
  EXPECT_TRUE(arangodb::iresearch::IResearchAnalyzerFeature::canUse(vocbase,
                                                                    arangodb::auth::Level::RW));
}
TEST_F(IResearchAnalyzerFeatureTest, test_auth_no_vocbase_read) {
  // no vocbase read access
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                        "testVocbase");
  userSetAccessLevel(arangodb::auth::Level::NONE, arangodb::auth::Level::NONE);
  auto ctxt = getLoggedInContext();
  arangodb::ExecContextScope execContextScope(ctxt.get());
  EXPECT_FALSE(
      arangodb::iresearch::IResearchAnalyzerFeature::canUse(vocbase, arangodb::auth::Level::RO));
}

// no collection read access (vocbase read access, no user)
TEST_F(IResearchAnalyzerFeatureTest, test_auth_vocbase_none_collection_read_no_user) {
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                        "testVocbase");
  userSetAccessLevel(arangodb::auth::Level::NONE, arangodb::auth::Level::RO);
  auto ctxt = getLoggedInContext();
  arangodb::ExecContextScope execContextScope(ctxt.get());
  EXPECT_FALSE(
      arangodb::iresearch::IResearchAnalyzerFeature::canUse(vocbase, arangodb::auth::Level::RO));
}

// no collection read access (vocbase read access)
TEST_F(IResearchAnalyzerFeatureTest, test_auth_vocbase_ro_collection_none) {
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                        "testVocbase");
  arangodb::iresearch::IResearchAnalyzerFeature feature(server);
  userSetAccessLevel(arangodb::auth::Level::RO, arangodb::auth::Level::NONE);
  auto ctxt = getLoggedInContext();
  arangodb::ExecContextScope execContextScope(ctxt.get());
  // implicit RO access to collection _analyzers collection granted due to RO access to db
  EXPECT_TRUE(arangodb::iresearch::IResearchAnalyzerFeature::canUse(vocbase,
                                                                    arangodb::auth::Level::RO));

  EXPECT_FALSE(
      arangodb::iresearch::IResearchAnalyzerFeature::canUse(vocbase, arangodb::auth::Level::RW));
}

TEST_F(IResearchAnalyzerFeatureTest, test_auth_vocbase_ro_collection_ro) {
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                        "testVocbase");
  userSetAccessLevel(arangodb::auth::Level::RO, arangodb::auth::Level::RO);
  auto ctxt = getLoggedInContext();
  arangodb::ExecContextScope execContextScope(ctxt.get());
  EXPECT_TRUE(arangodb::iresearch::IResearchAnalyzerFeature::canUse(vocbase,
                                                                    arangodb::auth::Level::RO));
  EXPECT_FALSE(
      arangodb::iresearch::IResearchAnalyzerFeature::canUse(vocbase, arangodb::auth::Level::RW));
}

TEST_F(IResearchAnalyzerFeatureTest, test_auth_vocbase_ro_collection_rw) {
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                        "testVocbase");
  userSetAccessLevel(arangodb::auth::Level::RO, arangodb::auth::Level::RW);
  auto ctxt = getLoggedInContext();
  arangodb::ExecContextScope execContextScope(ctxt.get());
  EXPECT_TRUE(arangodb::iresearch::IResearchAnalyzerFeature::canUse(vocbase,
                                                                    arangodb::auth::Level::RO));
  EXPECT_FALSE(
      arangodb::iresearch::IResearchAnalyzerFeature::canUse(vocbase, arangodb::auth::Level::RW));
}

TEST_F(IResearchAnalyzerFeatureTest, test_auth_vocbase_rw_collection_ro) {
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                        "testVocbase");
  userSetAccessLevel(arangodb::auth::Level::RW, arangodb::auth::Level::RO);
  auto ctxt = getLoggedInContext();
  arangodb::ExecContextScope execContextScope(ctxt.get());
  EXPECT_TRUE(arangodb::iresearch::IResearchAnalyzerFeature::canUse(vocbase,
                                                                    arangodb::auth::Level::RO));
  // implicit access for system analyzers collection granted due to RW access to database
  EXPECT_TRUE(arangodb::iresearch::IResearchAnalyzerFeature::canUse(vocbase,
                                                                    arangodb::auth::Level::RW));
}

TEST_F(IResearchAnalyzerFeatureTest, test_auth_vocbase_rw_collection_rw) {
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                        "testVocbase");
  userSetAccessLevel(arangodb::auth::Level::RW, arangodb::auth::Level::RW);
  auto ctxt = getLoggedInContext();
  arangodb::ExecContextScope execContextScope(ctxt.get());
  EXPECT_TRUE(arangodb::iresearch::IResearchAnalyzerFeature::canUse(vocbase,
                                                                    arangodb::auth::Level::RO));
  EXPECT_TRUE(arangodb::iresearch::IResearchAnalyzerFeature::canUse(vocbase,
                                                                    arangodb::auth::Level::RW));
}

// -----------------------------------------------------------------------------
// --SECTION--                                                emplace test suite
// -----------------------------------------------------------------------------

TEST_F(IResearchAnalyzerFeatureTest, test_emplace_valid) {
  arangodb::iresearch::IResearchAnalyzerFeature feature(server);
  {
    arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;
    EXPECT_TRUE(feature
                    .emplace(result, analyzerName(), "TestAnalyzer",
                             VPackParser::fromJson("\"abc\"")->slice())
                    .ok());
    EXPECT_NE(result.first, nullptr);
  }
  auto pool = feature.get(analyzerName());
  ASSERT_NE(pool, nullptr);
  EXPECT_EQ(irs::flags(), pool->features());
}

TEST_F(IResearchAnalyzerFeatureTest, test_emplace_duplicate_valid) {
  // add duplicate valid (same name+type+properties)
  arangodb::iresearch::IResearchAnalyzerFeature feature(server);
  {
    arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;
    EXPECT_TRUE(feature
                    .emplace(result, analyzerName(), "TestAnalyzer",
                             VPackParser::fromJson("\"abc\"")->slice(),
                             irs::flags{irs::frequency::type()})
                    .ok());
    EXPECT_NE(result.first, nullptr);
  }
  auto pool = feature.get(analyzerName());
  ASSERT_NE(pool, nullptr);
  EXPECT_EQ(irs::flags({irs::frequency::type()}), pool->features());
  {
    arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;
    EXPECT_TRUE(feature
                    .emplace(result, analyzerName(), "TestAnalyzer",
                             VPackParser::fromJson("\"abc\"")->slice(),
                             irs::flags{irs::frequency::type()})
                    .ok());
    EXPECT_NE(result.first, nullptr);
  }
  auto poolOther = feature.get(analyzerName());
  ASSERT_NE(poolOther, nullptr);
  EXPECT_EQ(pool, poolOther);
}

TEST_F(IResearchAnalyzerFeatureTest, test_emplace_duplicate_invalid_properties) {
  // add duplicate invalid (same name+type different properties)
  arangodb::iresearch::IResearchAnalyzerFeature feature(server);
  {
    arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;
    EXPECT_TRUE(feature
                    .emplace(result, analyzerName(), "TestAnalyzer",
                             VPackParser::fromJson("\"abc\"")->slice())
                    .ok());
    EXPECT_NE(result.first, nullptr);
  }
  auto pool = feature.get(analyzerName());
  ASSERT_NE(pool, nullptr);
  EXPECT_EQ(irs::flags(), pool->features());
  // Emplace should fail
  {
    arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;
    EXPECT_FALSE(feature
                     .emplace(result, analyzerName(), "TestAnalyzer",
                              VPackParser::fromJson("\"abcd\"")->slice())
                     .ok());
    EXPECT_EQ(result.first, nullptr);
  }
  // The formerly stored feature should still be available
  auto poolOther = feature.get(analyzerName());
  ASSERT_NE(poolOther, nullptr);
  EXPECT_EQ(pool, poolOther);
}

TEST_F(IResearchAnalyzerFeatureTest, test_emplace_duplicate_invalid_features) {
  // add duplicate invalid (same name+type different properties)
  arangodb::iresearch::IResearchAnalyzerFeature feature(server);
  {
    arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;
    EXPECT_TRUE(feature
                    .emplace(result, analyzerName(), "TestAnalyzer",
                             VPackParser::fromJson("\"abc\"")->slice())
                    .ok());
    EXPECT_NE(result.first, nullptr);
  }
  auto pool = feature.get(analyzerName());
  ASSERT_NE(pool, nullptr);
  EXPECT_EQ(irs::flags(), pool->features());
  {
    // Emplace should fail
    arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;
    EXPECT_FALSE(feature
                     .emplace(result, analyzerName(), "TestAnalyzer",
                              VPackParser::fromJson("\"abc\"")->slice(),
                              irs::flags{irs::frequency::type()})
                     .ok());
    EXPECT_EQ(result.first, nullptr);
  }
  // The formerly stored feature should still be available
  auto poolOther = feature.get(analyzerName());
  ASSERT_NE(poolOther, nullptr);
  EXPECT_EQ(pool, poolOther);
}

TEST_F(IResearchAnalyzerFeatureTest, test_emplace_duplicate_invalid_type) {
  // add duplicate invalid (same name+type different properties)
  arangodb::iresearch::IResearchAnalyzerFeature feature(server);
  {
    arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;
    EXPECT_TRUE(feature
                    .emplace(result, analyzerName(), "TestAnalyzer",
                             VPackParser::fromJson("\"abc\"")->slice())
                    .ok());
    EXPECT_NE(result.first, nullptr);
  }
  auto pool = feature.get(analyzerName());
  ASSERT_NE(pool, nullptr);
  EXPECT_EQ(irs::flags(), pool->features());
  {
    // Emplace should fail
    arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;
    EXPECT_FALSE(feature
                     .emplace(result, analyzerName(), "invalid",
                              VPackParser::fromJson("\"abc\"")->slice(),
                              irs::flags{irs::frequency::type()})
                     .ok());
    EXPECT_EQ(result.first, nullptr);
  }
  // The formerly stored feature should still be available
  auto poolOther = feature.get(analyzerName());
  ASSERT_NE(poolOther, nullptr);
  EXPECT_EQ(pool, poolOther);
}

TEST_F(IResearchAnalyzerFeatureTest, test_emplace_creation_failure_properties) {
  // add invalid (instance creation failure)
  arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;
  arangodb::iresearch::IResearchAnalyzerFeature feature(server);
  auto res = feature.emplace(result, analyzerName(), "TestAnalyzer",
                             VPackSlice::noneSlice());
  EXPECT_FALSE(res.ok());
  EXPECT_EQ(TRI_ERROR_BAD_PARAMETER, res.errorNumber());
  EXPECT_EQ(feature.get(analyzerName()), nullptr);
}

TEST_F(IResearchAnalyzerFeatureTest, test_emplace_creation_failure__properties_nil) {
  // add invalid (instance creation exception)
  arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;
  arangodb::iresearch::IResearchAnalyzerFeature feature(server);
  auto res = feature.emplace(result, analyzerName(), "TestAnalyzer",
                             VPackSlice::nullSlice());
  EXPECT_FALSE(res.ok());
  EXPECT_EQ(TRI_ERROR_BAD_PARAMETER, res.errorNumber());
  EXPECT_EQ(feature.get(analyzerName()), nullptr);
}
TEST_F(IResearchAnalyzerFeatureTest, test_emplace_creation_failure_invalid_type) {
  // add invalid (not registred)
  arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;
  arangodb::iresearch::IResearchAnalyzerFeature feature(server);
  auto res = feature.emplace(result, analyzerName(), "invalid",
                             VPackParser::fromJson("\"abc\"")->slice());
  EXPECT_FALSE(res.ok());
  EXPECT_EQ(TRI_ERROR_NOT_IMPLEMENTED, res.errorNumber());
  EXPECT_EQ(feature.get(analyzerName()), nullptr);
}
TEST_F(IResearchAnalyzerFeatureTest, test_emplace_creation_during_recovery) {
  // add valid inRecovery (failure)
  arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;
  arangodb::iresearch::IResearchAnalyzerFeature feature(server);
  auto before = StorageEngineMock::recoveryStateResult;
  StorageEngineMock::recoveryStateResult = arangodb::RecoveryState::IN_PROGRESS;
  auto restore = irs::make_finally(
      [&before]() -> void { StorageEngineMock::recoveryStateResult = before; });
  auto res = feature.emplace(result, analyzerName(), "TestAnalyzer",
                             VPackParser::fromJson("\"abc\"")->slice());
  // emplace should return OK for the sake of recovery
  EXPECT_TRUE(res.ok());
  auto ptr = feature.get(analyzerName());
  // but nothing should be stored
  EXPECT_EQ(nullptr, ptr);
}

TEST_F(IResearchAnalyzerFeatureTest, test_emplace_creation_unsupported_type) {
  // add invalid (unsupported feature)
  arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;
  arangodb::iresearch::IResearchAnalyzerFeature feature(server);
  auto res = feature.emplace(result, analyzerName(), "TestAnalyzer",
                             VPackParser::fromJson("\"abc\"")->slice(),
                             {irs::document::type()});
  EXPECT_FALSE(res.ok());
  EXPECT_EQ(TRI_ERROR_BAD_PARAMETER, res.errorNumber());
  EXPECT_EQ(feature.get(analyzerName()), nullptr);
}

TEST_F(IResearchAnalyzerFeatureTest, test_emplace_creation_position_without_frequency) {
  // add invalid ('position' without 'frequency')
  arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;
  arangodb::iresearch::IResearchAnalyzerFeature feature(server);
  auto res = feature.emplace(result, analyzerName(), "TestAnalyzer",
                             VPackParser::fromJson("\"abc\"")->slice(),
                             {irs::position::type()});
  EXPECT_FALSE(res.ok());
  EXPECT_EQ(TRI_ERROR_BAD_PARAMETER, res.errorNumber());
  EXPECT_EQ(feature.get(analyzerName()), nullptr);
}

TEST_F(IResearchAnalyzerFeatureTest, test_emplace_creation_properties_too_large) {
  arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;
  arangodb::iresearch::IResearchAnalyzerFeature feature(server);
  std::string properties(1024 * 1024 + 1, 'x');  // +1 char longer then limit
  auto res = feature.emplace(result, analyzerName(), "TestAnalyzer",
                             VPackParser::fromJson("\"abc\"")->slice(),
                             {irs::position::type()});
  EXPECT_FALSE(res.ok());
  EXPECT_EQ(TRI_ERROR_BAD_PARAMETER, res.errorNumber());
  EXPECT_EQ(feature.get(analyzerName()), nullptr);
}

TEST_F(IResearchAnalyzerFeatureTest, test_emplace_creation_name_invalid_character) {
  arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;
  arangodb::iresearch::IResearchAnalyzerFeature feature(server);
  std::string invalidName = analyzerName() + "+";  // '+' is invalid
  auto res = feature.emplace(result, invalidName, "TestAnalyzer",
                             VPackParser::fromJson("\"abc\"")->slice());
  EXPECT_FALSE(res.ok());
  EXPECT_EQ(TRI_ERROR_BAD_PARAMETER, res.errorNumber());
  EXPECT_EQ(feature.get(invalidName), nullptr);
}

TEST_F(IResearchAnalyzerFeatureTest, test_emplace_add_static_analyzer) {
  arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;
  arangodb::iresearch::IResearchAnalyzerFeature feature(server);
  feature.prepare();  // add static analyzers
  auto res = feature.emplace(result, "identity", "identity", VPackSlice::noneSlice(),
                             irs::flags{irs::frequency::type(), irs::norm::type()});
  EXPECT_TRUE(res.ok());
  EXPECT_NE(result.first, nullptr);
  auto pool = feature.get("identity");
  ASSERT_NE(pool, nullptr);
  EXPECT_EQ(irs::flags({irs::norm::type(), irs::frequency::type()}), pool->features());
  auto analyzer = pool->get();
  ASSERT_NE(analyzer, nullptr);
}

TEST_F(IResearchAnalyzerFeatureTest, test_get_parameter_match) {
  arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;
  arangodb::iresearch::IResearchAnalyzerFeature feature(server);
  auto res = feature.emplace(result, analyzerName(), "TestAnalyzer",
                             VPackParser::fromJson("\"abc\"")->slice(),
                             {irs::frequency::type()});
  EXPECT_TRUE(res.ok());
  auto read = feature.get(analyzerName(), "identity",
                          VPackParser::fromJson("\"abc\"")->slice(),
                          {irs::frequency::type()});
  ASSERT_EQ(read, nullptr);
}

TEST_F(IResearchAnalyzerFeatureTest, test_get_properties_mismatch) {
  arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;
  arangodb::iresearch::IResearchAnalyzerFeature feature(server);
  auto res = feature.emplace(result, analyzerName(), "TestAnalyzer",
                             VPackParser::fromJson("\"abc\"")->slice(),
                             {irs::frequency::type()});
  EXPECT_TRUE(res.ok());
  auto read = feature.get(analyzerName(), "TestAnalyzer",
                          VPackParser::fromJson("\"abcd\"")->slice(),
                          {irs::frequency::type()});
  ASSERT_EQ(read, nullptr);
}

TEST_F(IResearchAnalyzerFeatureTest, test_get_feature_mismatch) {
  arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;
  arangodb::iresearch::IResearchAnalyzerFeature feature(server);
  auto res = feature.emplace(result, analyzerName(), "TestAnalyzer",
                             VPackParser::fromJson("\"abc\"")->slice(),
                             {irs::frequency::type()});
  EXPECT_TRUE(res.ok());
  auto read = feature.get(analyzerName(), "TestAnalyzer",
                          VPackParser::fromJson("\"abc\"")->slice(),
                          {irs::position::type()});
  ASSERT_EQ(read, nullptr);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    get test suite
// -----------------------------------------------------------------------------

class IResearchAnalyzerFeatureGetTest : public IResearchAnalyzerFeatureTest {
 protected:
  arangodb::AqlFeature aqlFeature;
  arangodb::iresearch::IResearchAnalyzerFeature analyzerFeature;
  std::string dbName;

 private:
  arangodb::SystemDatabaseFeature::ptr _sysVocbase;
  TRI_vocbase_t* _vocbase;
  arangodb::DatabaseFeature* _dbFeature;

 protected:
  IResearchAnalyzerFeatureGetTest()
      : IResearchAnalyzerFeatureTest(),
        aqlFeature(server),
        analyzerFeature(server),
        dbName("testVocbase") {}

  ~IResearchAnalyzerFeatureGetTest() {}

  // Need Setup inorder to alow ASSERTs
  void SetUp() override {
    // required for Query::Query(...), must not call ~AqlFeature() for the duration of the test
    aqlFeature.start();
    _dbFeature =
        arangodb::application_features::ApplicationServer::lookupFeature<arangodb::DatabaseFeature>(
            "Database");
    ASSERT_NE(_dbFeature, nullptr);

    // Prepare a database
    ASSERT_NE(sysDatabaseFeature, nullptr);
    _sysVocbase = sysDatabaseFeature->use();
    ASSERT_NE(_sysVocbase, nullptr);

    _vocbase = nullptr;
    auto res = _dbFeature->createDatabase(1, dbName, _vocbase);
    ASSERT_EQ(res, TRI_ERROR_NO_ERROR);
    ASSERT_NE(_vocbase, nullptr);
    arangodb::methods::Collections::createSystem(*_vocbase, arangodb::tests::AnalyzerCollectionName);
    // Prepare analyzers
    analyzerFeature.prepare();  // add static analyzers

    arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;
    ASSERT_TRUE(feature()
                    .emplace(result, sysName(), "TestAnalyzer",
                             VPackParser::fromJson("\"abc\"")->slice())
                    .ok());
    ASSERT_TRUE(feature()
                    .emplace(result, specificName(), "TestAnalyzer",
                             VPackParser::fromJson("\"def\"")->slice())
                    .ok());
  }

  void TearDown() override {
    // Not allowed to assert here
    if (_dbFeature != nullptr) {
      _dbFeature->dropDatabase(dbName, true, true);
      _vocbase = nullptr;
    }
  }

  arangodb::iresearch::IResearchAnalyzerFeature& feature() {
    return analyzerFeature;
  }

  std::string sysName() const {
    return arangodb::StaticStrings::SystemDatabase + shortName();
  }

  std::string specificName() const { return dbName + shortName(); }
  std::string shortName() const { return "::test_analyzer"; }

  TRI_vocbase_t* system() const { return _sysVocbase.get(); }

  TRI_vocbase_t* specificBase() const { return _vocbase; }

  arangodb::DatabaseFeature* databaseFeature() const { return _dbFeature; }
};

TEST_F(IResearchAnalyzerFeatureGetTest, test_get_valid) {
  auto pool = feature().get(analyzerName());
  ASSERT_NE(pool, nullptr);
  EXPECT_EQ(irs::flags(), pool->features());
  EXPECT_EQUAL_SLICES(VPackParser::fromJson("{\"args\":\"abc\"}")->slice(),
                      pool->properties());
  auto analyzer = pool.get();
  EXPECT_NE(analyzer, nullptr);
}

TEST_F(IResearchAnalyzerFeatureGetTest, test_get_global_system) {
  auto sysVocbase = system();
  ASSERT_NE(sysVocbase, nullptr);
  auto pool = feature().get(analyzerName(), *sysVocbase, *sysVocbase);
  ASSERT_NE(pool, nullptr);
  EXPECT_EQ(irs::flags(), pool->features());
  EXPECT_EQUAL_SLICES(VPackParser::fromJson("{\"args\":\"abc\"}")->slice(),
                      pool->properties());
  auto analyzer = pool.get();
  EXPECT_NE(analyzer, nullptr);
}

TEST_F(IResearchAnalyzerFeatureGetTest, test_get_global_specific) {
  auto sysVocbase = system();
  auto vocbase = specificBase();
  ASSERT_NE(sysVocbase, nullptr);
  ASSERT_NE(vocbase, nullptr);
  auto pool = feature().get(analyzerName(), *vocbase, *sysVocbase);
  ASSERT_NE(pool, nullptr);
  EXPECT_EQ(irs::flags(), pool->features());
  EXPECT_EQUAL_SLICES(VPackParser::fromJson("{\"args\":\"abc\"}")->slice(),
                      pool->properties());
  auto analyzer = pool.get();
  EXPECT_NE(analyzer, nullptr);
}

TEST_F(IResearchAnalyzerFeatureGetTest, test_get_global_specific_analyzer_name_only) {
  auto sysVocbase = system();
  auto vocbase = specificBase();
  ASSERT_NE(sysVocbase, nullptr);
  ASSERT_NE(vocbase, nullptr);
  auto pool = feature().get(shortName(), *vocbase, *sysVocbase);
  ASSERT_NE(pool, nullptr);
  EXPECT_EQ(irs::flags(), pool->features());
  EXPECT_EQUAL_SLICES(VPackParser::fromJson("{\"args\":\"abc\"}")->slice(),
                      pool->properties());
  auto analyzer = pool.get();
  EXPECT_NE(analyzer, nullptr);
}

TEST_F(IResearchAnalyzerFeatureGetTest, test_get_local_system_analyzer_no_colons) {
  auto sysVocbase = system();
  auto vocbase = specificBase();
  ASSERT_NE(sysVocbase, nullptr);
  ASSERT_NE(vocbase, nullptr);
  auto pool = feature().get("test_analyzer", *vocbase, *sysVocbase);
  ASSERT_NE(pool, nullptr);
  EXPECT_EQ(irs::flags(), pool->features());
  EXPECT_EQUAL_SLICES(VPackParser::fromJson("{\"args\":\"def\"}")->slice(),
                      pool->properties());
  auto analyzer = pool.get();
  EXPECT_NE(analyzer, nullptr);
}

TEST_F(IResearchAnalyzerFeatureGetTest, test_get_local_including_collection_name) {
  auto sysVocbase = system();
  auto vocbase = specificBase();
  ASSERT_NE(sysVocbase, nullptr);
  ASSERT_NE(vocbase, nullptr);
  auto pool = feature().get(specificName(), *vocbase, *sysVocbase);
  ASSERT_NE(pool, nullptr);
  EXPECT_EQ(irs::flags(), pool->features());
  EXPECT_EQUAL_SLICES(VPackParser::fromJson("{\"args\":\"def\"}")->slice(),
                      pool->properties());
  auto analyzer = pool.get();
  EXPECT_NE(analyzer, nullptr);
}

TEST_F(IResearchAnalyzerFeatureGetTest, test_get_failure_invalid_name) {
  auto pool =
      feature().get(arangodb::StaticStrings::SystemDatabase + "::invalid");
  EXPECT_EQ(pool, nullptr);
}

TEST_F(IResearchAnalyzerFeatureGetTest, test_get_failure_invalid_name_adding_vocbases) {
  auto sysVocbase = system();
  ASSERT_NE(sysVocbase, nullptr);
  auto pool =
      feature().get(arangodb::StaticStrings::SystemDatabase + "::invalid",
                    *sysVocbase, *sysVocbase);
  EXPECT_EQ(pool, nullptr);
}

TEST_F(IResearchAnalyzerFeatureGetTest, test_get_failure_invalid_short_name_adding_vocbases) {
  auto sysVocbase = system();
  ASSERT_NE(sysVocbase, nullptr);
  auto pool = feature().get("::invalid", *sysVocbase, *sysVocbase);
  EXPECT_EQ(pool, nullptr);
}

TEST_F(IResearchAnalyzerFeatureGetTest,
       test_get_failure_invalid_short_name_no_colons_adding_vocbases) {
  auto sysVocbase = system();
  ASSERT_NE(sysVocbase, nullptr);
  auto pool = feature().get("invalid", *sysVocbase, *sysVocbase);
  EXPECT_EQ(pool, nullptr);
}

TEST_F(IResearchAnalyzerFeatureGetTest, test_get_failure_invalid_type_adding_vocbases) {
  auto sysVocbase = system();
  ASSERT_NE(sysVocbase, nullptr);
  auto pool = feature().get("testAnalyzer", *sysVocbase, *sysVocbase);
  EXPECT_EQ(pool, nullptr);
}

TEST_F(IResearchAnalyzerFeatureGetTest, test_get_static_analyzer) {
  auto pool = feature().get("identity");
  ASSERT_NE(pool, nullptr);
  EXPECT_EQ(irs::flags({irs::norm::type(), irs::frequency::type()}), pool->features());
  auto analyzer = pool->get();
  ASSERT_NE(analyzer, nullptr);
}

TEST_F(IResearchAnalyzerFeatureGetTest, test_get_static_analyzer_adding_vocbases) {
  auto sysVocbase = system();
  ASSERT_NE(sysVocbase, nullptr);
  auto pool = feature().get("identity", *sysVocbase, *sysVocbase);
  ASSERT_NE(pool, nullptr);
  EXPECT_EQ(irs::flags({irs::norm::type(), irs::frequency::type()}), pool->features());
  auto analyzer = pool->get();
  ASSERT_NE(analyzer, nullptr);
}

TEST_F(IResearchAnalyzerFeatureGetTest, test_get_failure_specfic_type_and_properties_mismatch) {
  auto pool = feature().get(specificName(), "TestAnalyzer",
                            VPackParser::fromJson("{\"args\":\"abc\"}")->slice(),
                            {irs::frequency::type()});
  ASSERT_EQ(pool, nullptr);
}

// -----------------------------------------------------------------------------
// --SECTION--                                            coordinator test suite
// -----------------------------------------------------------------------------

class IResearchAnalyzerFeatureCoordinatorTest : public ::testing::Test {
 private:
  struct ClusterCommControl : arangodb::ClusterComm {
    static void reset() { arangodb::ClusterComm::_theInstanceInit.store(0); }
  };

  arangodb::consensus::Store _agencyStore{nullptr, "arango"};
  GeneralClientConnectionAgencyMock* agency;
  StorageEngineMock engine;
  arangodb::application_features::ApplicationServer server;
  std::string _dbName;
  std::unique_ptr<TRI_vocbase_t> _system;
  std::map<std::string, std::pair<arangodb::application_features::ApplicationFeature*, bool>> features;
  std::vector<arangodb::application_features::ApplicationFeature*> orderedFeatures;
  std::string testFilesystemPath;
  arangodb::ServerState::RoleEnum _serverRoleBeforeSetup;
  TRI_vocbase_t* _vocbase;
  arangodb::iresearch::IResearchAnalyzerFeature* _feature;

 protected:
  IResearchAnalyzerFeatureCoordinatorTest()
      : engine(server), server(nullptr, nullptr), _dbName("TestVocbase") {
    auto* agencyCommManager = new AgencyCommManagerMock("arango");
    agency = agencyCommManager->addConnection<GeneralClientConnectionAgencyMock>(_agencyStore);
    agency = agencyCommManager->addConnection<GeneralClientConnectionAgencyMock>(
        _agencyStore);  // need 2 connections or Agency callbacks will fail
    arangodb::AgencyCommManager::MANAGER.reset(agencyCommManager);

    arangodb::EngineSelectorFeature::ENGINE = &engine;
    /*
    // register factories & normalizers
    auto& indexFactory = const_cast<arangodb::IndexFactory&>(engine.indexFactory());
    indexFactory.emplace(arangodb::iresearch::DATA_SOURCE_TYPE.name(),
                         arangodb::iresearch::IResearchLinkCoordinator::factory());
                         */

    arangodb::tests::init();

    // suppress INFO {authentication} Authentication is turned on (system only), authentication for unix sockets is turned on
    arangodb::LogTopic::setLogLevel(arangodb::Logger::AUTHENTICATION.name(),
                                    arangodb::LogLevel::WARN);

    // pretend we're on coordinator
    _serverRoleBeforeSetup = arangodb::ServerState::instance()->getRole();
    arangodb::ServerState::instance()->setRole(arangodb::ServerState::ROLE_COORDINATOR);

    auto buildFeatureEntry = [&](arangodb::application_features::ApplicationFeature* ftr,
                                 bool start) -> void {
      std::string name = ftr->name();
      features.emplace(name, std::make_pair(ftr, start));
    };
    arangodb::application_features::ApplicationFeature* tmpFeature;

    buildFeatureEntry(new arangodb::application_features::BasicFeaturePhase(server, false), false);
    buildFeatureEntry(new arangodb::application_features::CommunicationFeaturePhase(server),
                      false);
    buildFeatureEntry(new arangodb::application_features::ClusterFeaturePhase(server), false);
    buildFeatureEntry(new arangodb::application_features::DatabaseFeaturePhase(server), false);
    buildFeatureEntry(new arangodb::application_features::GreetingsFeaturePhase(server, false),
                      false);
    buildFeatureEntry(new arangodb::application_features::V8FeaturePhase(server), false);

    // setup required application features
    buildFeatureEntry(new arangodb::V8DealerFeature(server), false);
    buildFeatureEntry(new arangodb::ViewTypesFeature(server), true);
    buildFeatureEntry(tmpFeature = new arangodb::QueryRegistryFeature(server), false);
    arangodb::application_features::ApplicationServer::server->addFeature(tmpFeature);  // need QueryRegistryFeature feature to be added now in order to create the system database
    _system = irs::memory::make_unique<TRI_vocbase_t>(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL,
                                                      0, TRI_VOC_SYSTEM_DATABASE);
    buildFeatureEntry(new arangodb::SystemDatabaseFeature(server, _system.get()),
                      false);  // required for IResearchAnalyzerFeature
    buildFeatureEntry(new arangodb::RandomFeature(server), false);  // required by AuthenticationFeature
    buildFeatureEntry(new arangodb::AuthenticationFeature(server), false);
    buildFeatureEntry(arangodb::DatabaseFeature::DATABASE =
                          new arangodb::DatabaseFeature(server),
                      false);
    buildFeatureEntry(new arangodb::DatabasePathFeature(server), false);
    buildFeatureEntry(new arangodb::TraverserEngineRegistryFeature(server), false);  // must be before AqlFeature
    buildFeatureEntry(new arangodb::AqlFeature(server), true);
    buildFeatureEntry(new arangodb::aql::AqlFunctionFeature(server), true);  // required for IResearchAnalyzerFeature
    buildFeatureEntry(new arangodb::iresearch::IResearchFeature(server), true);
    buildFeatureEntry(new arangodb::aql::OptimizerRulesFeature(server), true);
    buildFeatureEntry(new arangodb::FlushFeature(server), false);  // do not start the thread
    buildFeatureEntry(new arangodb::ClusterFeature(server), false);
    buildFeatureEntry(new arangodb::ShardingFeature(server), false);
    buildFeatureEntry(new arangodb::iresearch::IResearchAnalyzerFeature(server), true);

#if USE_ENTERPRISE
    buildFeatureEntry(new arangodb::LdapFeature(server),
                      false);  // required for AuthenticationFeature with USE_ENTERPRISE
#endif

    for (auto& f : features) {
      arangodb::application_features::ApplicationServer::server->addFeature(
          f.second.first);
    }
    arangodb::application_features::ApplicationServer::server->setupDependencies(false);
    orderedFeatures =
        arangodb::application_features::ApplicationServer::server->getOrderedFeatures();

    // suppress log messages since tests check error conditions
    arangodb::LogTopic::setLogLevel(arangodb::Logger::AGENCY.name(),
                                    arangodb::LogLevel::FATAL);
    arangodb::LogTopic::setLogLevel(arangodb::Logger::ENGINES.name(), arangodb::LogLevel::FATAL);  // suppress ERROR {engines} failed to instantiate index, error: ...
    arangodb::LogTopic::setLogLevel(arangodb::Logger::FIXME.name(), arangodb::LogLevel::ERR);  // suppress ERROR recovery failure due to error from callback
    arangodb::LogTopic::setLogLevel(arangodb::Logger::CLUSTER.name(),
                                    arangodb::LogLevel::FATAL);
    arangodb::LogTopic::setLogLevel(arangodb::iresearch::TOPIC.name(),
                                    arangodb::LogLevel::FATAL);
    irs::logger::output_le(iresearch::logger::IRL_FATAL, stderr);

    for (auto& f : orderedFeatures) {
      f->prepare();

      if (f->name() == "Authentication") {
        f->forceDisable();
      }
    }

    for (auto& f : orderedFeatures) {
      if (features.at(f->name()).second) {
        f->start();
      }
    }

    auto* authFeature =
        arangodb::application_features::ApplicationServer::getFeature<arangodb::AuthenticationFeature>(
            "Authentication");
    authFeature->enable();  // required for authentication tests
    // We have added the feature above, so saave to dereference here
    _feature =
        arangodb::application_features::ApplicationServer::lookupFeature<arangodb::iresearch::IResearchAnalyzerFeature>(
            "ArangoSearchAnalyzer");
    TransactionStateMock::abortTransactionCount = 0;
    TransactionStateMock::beginTransactionCount = 0;
    TransactionStateMock::commitTransactionCount = 0;

    auto* dbPathFeature =
        arangodb::application_features::ApplicationServer::getFeature<arangodb::DatabasePathFeature>(
            "DatabasePath");
    arangodb::tests::setDatabasePath(*dbPathFeature);  // ensure test data is stored in a unique directory
    testFilesystemPath = dbPathFeature->directory();

    long systemError;
    std::string systemErrorStr;
    TRI_CreateDirectory(testFilesystemPath.c_str(), systemError, systemErrorStr);

    agencyCommManager->start();  // initialize agency
  }
  ~IResearchAnalyzerFeatureCoordinatorTest() {
    _system.reset();  // destroy before reseting the 'ENGINE'
    TRI_RemoveDirectory(testFilesystemPath.c_str());
    arangodb::LogTopic::setLogLevel(arangodb::iresearch::TOPIC.name(),
                                    arangodb::LogLevel::DEFAULT);
    arangodb::LogTopic::setLogLevel(arangodb::Logger::CLUSTER.name(),
                                    arangodb::LogLevel::DEFAULT);
    arangodb::LogTopic::setLogLevel(arangodb::Logger::FIXME.name(),
                                    arangodb::LogLevel::DEFAULT);
    arangodb::LogTopic::setLogLevel(arangodb::Logger::ENGINES.name(),
                                    arangodb::LogLevel::DEFAULT);
    arangodb::LogTopic::setLogLevel(arangodb::Logger::AGENCY.name(),
                                    arangodb::LogLevel::DEFAULT);
    arangodb::ClusterInfo::cleanup();  // reset ClusterInfo::instance() before DatabaseFeature::unprepare()
    arangodb::application_features::ApplicationServer::server = nullptr;

    // destroy application features
    for (auto f = orderedFeatures.rbegin(); f != orderedFeatures.rend(); ++f) {
      if (features.at((*f)->name()).second) {
        (*f)->stop();
      }
    }

    for (auto f = orderedFeatures.rbegin(); f != orderedFeatures.rend(); ++f) {
      (*f)->unprepare();
    }

    ClusterCommControl::reset();
    arangodb::ServerState::instance()->setRole(_serverRoleBeforeSetup);
    arangodb::LogTopic::setLogLevel(arangodb::Logger::AUTHENTICATION.name(),
                                    arangodb::LogLevel::DEFAULT);
    arangodb::EngineSelectorFeature::ENGINE = nullptr;
  }

  void SetUp() override {
    auto dbFeature =
        arangodb::application_features::ApplicationServer::lookupFeature<arangodb::DatabaseFeature>(
            "Database");

    // DO we need this?
    // Is this in right position

    // create system vocbase (before feature start)
    {
      auto const databases = VPackParser::fromJson(
          std::string("[ { \"name\": \"") +
          arangodb::StaticStrings::SystemDatabase + "\" } ]");
      EXPECT_TRUE((TRI_ERROR_NO_ERROR == dbFeature->loadDatabases(databases->slice())));
      auto system =
          arangodb::application_features::ApplicationServer::lookupFeature<arangodb::SystemDatabaseFeature>(
              "SystemDatabase");
      system->start();  // get system database from DatabaseFeature
    }

    _vocbase = nullptr;
    auto res = dbFeature->createDatabase(1, _dbName, _vocbase);
    ASSERT_EQ(res, TRI_ERROR_NO_ERROR);
    ASSERT_NE(_vocbase, nullptr);

    // Prepare analyzers
    _feature->prepare();  // add static analyzers
  }

  void TearDown() override {
    // Not allowed to assert here
    auto dbFeature =
        arangodb::application_features::ApplicationServer::lookupFeature<arangodb::DatabaseFeature>(
            "Database");
    if (dbFeature != nullptr) {
      dbFeature->dropDatabase(_dbName, true, true);
      _vocbase = nullptr;
    }
  }

  arangodb::iresearch::IResearchAnalyzerFeature& feature() {
    // Cannot use TestAsserts here, only in void funtions
    TRI_ASSERT(_feature != nullptr);
    return *_feature;
  }

  std::string sysName() const {
    return arangodb::StaticStrings::SystemDatabase + shortName();
  }

  std::string specificName() const { return _dbName + shortName(); }
  std::string shortName() const { return "::test_analyzer"; }

  TRI_vocbase_t* system() const { return _system.get(); }

  TRI_vocbase_t* specificBase() const { return _vocbase; }
};

TEST_F(IResearchAnalyzerFeatureGetTest, test_get_db_server) {
  auto before = arangodb::ServerState::instance()->getRole();
  arangodb::ServerState::instance()->setRole(arangodb::ServerState::ROLE_DBSERVER);
  auto restore = irs::make_finally([&before]() -> void {
    arangodb::ServerState::instance()->setRole(before);
  });

  arangodb::iresearch::IResearchAnalyzerFeature feature(server);
  EXPECT_TRUE(
      (false == !feature.get("testVocbase::test_analyzer", "TestAnalyzer",
                             VPackParser::fromJson("\"abc\"")->slice(),
                             {irs::frequency::type()})));
}

TEST_F(IResearchAnalyzerFeatureCoordinatorTest, test_ensure_index) {
  // add index factory
  {
    struct IndexTypeFactory : public arangodb::IndexTypeFactory {
      virtual bool equal(arangodb::velocypack::Slice const& lhs,
                         arangodb::velocypack::Slice const& rhs) const override {
        return false;
      }

      std::shared_ptr<arangodb::Index> instantiate(arangodb::LogicalCollection& collection,
                                                   arangodb::velocypack::Slice const& definition,
                                                   TRI_idx_iid_t id,
                                                   bool isClusterConstructor) const override {
        auto* ci = arangodb::ClusterInfo::instance();
        EXPECT_NE(nullptr, ci);
        auto* feature =
            arangodb::application_features::ApplicationServer::lookupFeature<arangodb::iresearch::IResearchAnalyzerFeature>();
        EXPECT_TRUE((feature));
        ci->invalidatePlan();  // invalidate plan to test recursive lock aquisition in ClusterInfo::loadPlan()
        EXPECT_EQ(nullptr, feature->get(arangodb::StaticStrings::SystemDatabase + "::missing",
                                        "TestAnalyzer", VPackSlice::noneSlice(),
                                        irs::flags()));
        return std::make_shared<TestIndex>(id, collection, definition);
      }

      virtual arangodb::Result normalize(arangodb::velocypack::Builder& normalized,
                                         arangodb::velocypack::Slice definition, bool isCreation,
                                         TRI_vocbase_t const& vocbase) const override {
        EXPECT_TRUE((arangodb::iresearch::mergeSlice(normalized, definition)));
        return arangodb::Result();
      }
    };
    static const IndexTypeFactory indexTypeFactory;
    auto& indexFactory = const_cast<arangodb::IndexFactory&>(
        arangodb::EngineSelectorFeature::ENGINE->indexFactory());
    indexFactory.emplace("testType", indexTypeFactory);
  }

  // get missing via link creation (coordinator) ensure no recursive ClusterInfo::loadPlan() call
  {
    auto createCollectionJson = VPackParser::fromJson(
        std::string("{ \"id\": 42, \"name\": \"") + arangodb::tests::AnalyzerCollectionName +
        "\", \"isSystem\": true, \"shards\": { }, \"type\": 2 }");  // 'id' and 'shards' required for coordinator tests
    auto collectionId = std::to_string(42);

    ClusterCommMock clusterComm;
    auto scopedClusterComm = ClusterCommMock::setInstance(clusterComm);
    auto* ci = arangodb::ClusterInfo::instance();
    ASSERT_NE(nullptr, ci);

    ASSERT_TRUE((ci->createCollectionCoordinator(system()->name(), collectionId, 0, 1, false,
                                                 createCollectionJson->slice(), 0.0)
                     .ok()));
    auto logicalCollection = ci->getCollection(system()->name(), collectionId);
    ASSERT_NE(nullptr, logicalCollection);

    // simulate heartbeat thread
    // We need this call BEFORE creation of collection if at all
    {
      auto const colPath = "/Current/Collections/_system/42";
      auto const colValue =
          VPackParser::fromJson(
              "{ \"same-as-dummy-shard-id\": { \"indexes\": [ { \"id\": \"1\" "
              "} ], \"servers\": [ \"same-as-dummy-shard-server\" ] } }");  // '1' must match 'idString' in ClusterInfo::ensureIndexCoordinatorInner(...)
      EXPECT_TRUE(
          arangodb::AgencyComm().setValue(colPath, colValue->slice(), 0.0).successful());
      auto const dummyPath = "/Plan/Collections";
      auto const dummyValue = VPackParser::fromJson(
          "{ \"_system\": { \"42\": { \"name\": \"testCollection\", "
          "\"shards\": { \"same-as-dummy-shard-id\": [ "
          "\"same-as-dummy-shard-server\" ] } } } }");
      EXPECT_TRUE(
          arangodb::AgencyComm().setValue(dummyPath, dummyValue->slice(), 0.0).successful());
      auto const versionPath = "/Plan/Version";
      auto const versionValue =
          VPackParser::fromJson(std::to_string(ci->getPlanVersion() + 1));
      EXPECT_TRUE((arangodb::AgencyComm()
                       .setValue(versionPath, versionValue->slice(), 0.0)
                       .successful()));  // force loadPlan() update
      ci->invalidateCurrent();           // force reload of 'Current'
    }

    // insert response for expected analyzer lookup
    {
      arangodb::ClusterCommResult response;
      response.status = arangodb::ClusterCommOpStatus::CL_COMM_RECEIVED;
      response.result = std::make_shared<arangodb::httpclient::SimpleHttpResult>();
      response.result->getBody()
          .appendText(
              "{ \"result\": { \"snippets\": { \"6:shard-id-does-not-matter\": "
              "\"value-does-not-matter\" } } }")
          .ensureNullTerminated();  // '6' must match GATHER Node id in ExecutionEngine::createBlocks(...)
      clusterComm._responses.emplace_back(std::move(response));
    }

    // insert response for expected analyzer reload from collection
    {
      arangodb::ClusterCommResult response;
      response.status = arangodb::ClusterCommOpStatus::CL_COMM_SENT;
      response.result = std::make_shared<arangodb::httpclient::SimpleHttpResult>();
      response.result->getBody()
          .appendText(
              "{ \"done\": true, \"nrItems\": 1, \"nrRegs\": 1, \"data\": [ 1 "
              "], \"raw\": [ null, null, { \"_key\": \"key-does-not-matter\", "
              "\"name\": \"abc\", \"type\": \"TestAnalyzer\", \"properties\": "
              "\"abc\" } ] }")
          .ensureNullTerminated();  // 'data' value must be 1 as per AqlItemBlock::AqlItemBlock(...), first 2 'raw' values ignored, 'nrRegs' must be 1 or assertion failure in ExecutionBlockImpl<Executor>::requestWrappedBlock(...)
      clusterComm._responses.emplace_back(std::move(response));
    }

    arangodb::velocypack::Builder builder;
    arangodb::velocypack::Builder tmp;

    builder.openObject();
    builder.add(arangodb::StaticStrings::IndexType, arangodb::velocypack::Value("testType"));
    builder.add(arangodb::StaticStrings::IndexFields,
                arangodb::velocypack::Slice::emptyArraySlice());
    builder.close();
    EXPECT_TRUE((arangodb::methods::Indexes::ensureIndex(logicalCollection.get(),
                                                         builder.slice(), true, tmp)
                     .ok()));
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                               identity test suite
// -----------------------------------------------------------------------------
TEST_F(IResearchAnalyzerFeatureTest, test_identity_static) {
  auto pool = arangodb::iresearch::IResearchAnalyzerFeature::identity();
  ASSERT_NE(nullptr, pool);
  EXPECT_EQ(irs::flags({irs::norm::type(), irs::frequency::type()}), pool->features());
  EXPECT_EQ("identity", pool->name());
  auto analyzer = pool->get();
  ASSERT_NE(nullptr, analyzer);
  auto& term = analyzer->attributes().get<irs::term_attribute>();
  ASSERT_NE(nullptr, term);
  EXPECT_FALSE(analyzer->next());
  EXPECT_TRUE(analyzer->reset("abc def ghi"));
  EXPECT_TRUE(analyzer->next());
  EXPECT_EQ(irs::ref_cast<irs::byte_type>(irs::string_ref("abc def ghi")), term->value());
  EXPECT_FALSE(analyzer->next());
  EXPECT_TRUE(analyzer->reset("123 456"));
  EXPECT_TRUE(analyzer->next());
  EXPECT_EQ(irs::ref_cast<irs::byte_type>(irs::string_ref("123 456")), term->value());
  EXPECT_FALSE(analyzer->next());
}
TEST_F(IResearchAnalyzerFeatureTest, test_identity_registered) {
  arangodb::iresearch::IResearchAnalyzerFeature feature(server);
  feature.prepare();  // add static analyzers
  EXPECT_TRUE((false == !feature.get("identity")));
  auto pool = feature.get("identity");
  ASSERT_NE(nullptr, pool);
  EXPECT_EQ(irs::flags({irs::norm::type(), irs::frequency::type()}), pool->features());
  EXPECT_EQ("identity", pool->name());
  auto analyzer = pool->get();
  ASSERT_NE(nullptr, analyzer);
  auto& term = analyzer->attributes().get<irs::term_attribute>();
  ASSERT_NE(nullptr, term);
  EXPECT_FALSE(analyzer->next());
  EXPECT_TRUE(analyzer->reset("abc def ghi"));
  EXPECT_TRUE(analyzer->next());
  EXPECT_EQ(irs::ref_cast<irs::byte_type>(irs::string_ref("abc def ghi")), term->value());
  EXPECT_FALSE(analyzer->next());
  EXPECT_TRUE(analyzer->reset("123 456"));
  EXPECT_TRUE(analyzer->next());
  EXPECT_EQ(irs::ref_cast<irs::byte_type>(irs::string_ref("123 456")), term->value());
  EXPECT_FALSE(analyzer->next());
}

// -----------------------------------------------------------------------------
// --SECTION--                                              normalize test suite
// -----------------------------------------------------------------------------

TEST_F(IResearchAnalyzerFeatureTest, test_normalize) {
  TRI_vocbase_t active(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "active");
  TRI_vocbase_t system(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "system");

  // normalize 'identity' (with prefix)
  {
    irs::string_ref analyzer = "identity";
    auto normalized =
        arangodb::iresearch::IResearchAnalyzerFeature::normalize(analyzer, active,
                                                                 system, true);
    EXPECT_TRUE((std::string("identity") == normalized));
  }

  // normalize 'identity' (without prefix)
  {
    irs::string_ref analyzer = "identity";
    auto normalized =
        arangodb::iresearch::IResearchAnalyzerFeature::normalize(analyzer, active,
                                                                 system, true);
    EXPECT_TRUE((std::string("identity") == normalized));
  }

  // normalize NIL (with prefix)
  {
    irs::string_ref analyzer = irs::string_ref::NIL;
    auto normalized =
        arangodb::iresearch::IResearchAnalyzerFeature::normalize(analyzer, active,
                                                                 system, true);
    EXPECT_TRUE((std::string("active::") == normalized));
  }

  // normalize NIL (without prefix)
  {
    irs::string_ref analyzer = irs::string_ref::NIL;
    auto normalized =
        arangodb::iresearch::IResearchAnalyzerFeature::normalize(analyzer, active,
                                                                 system, false);
    EXPECT_TRUE((std::string("") == normalized));
  }

  // normalize EMPTY (with prefix)
  {
    irs::string_ref analyzer = irs::string_ref::EMPTY;
    auto normalized =
        arangodb::iresearch::IResearchAnalyzerFeature::normalize(analyzer, active,
                                                                 system, true);
    EXPECT_TRUE((std::string("active::") == normalized));
  }

  // normalize EMPTY (without prefix)
  {
    irs::string_ref analyzer = irs::string_ref::EMPTY;
    auto normalized =
        arangodb::iresearch::IResearchAnalyzerFeature::normalize(analyzer, active,
                                                                 system, false);
    EXPECT_TRUE((std::string("") == normalized));
  }

  // normalize delimiter (with prefix)
  {
    irs::string_ref analyzer = "::";
    auto normalized =
        arangodb::iresearch::IResearchAnalyzerFeature::normalize(analyzer, active,
                                                                 system, true);
    EXPECT_TRUE((std::string("system::") == normalized));
  }

  // normalize delimiter (without prefix)
  {
    irs::string_ref analyzer = "::";
    auto normalized =
        arangodb::iresearch::IResearchAnalyzerFeature::normalize(analyzer, active,
                                                                 system, false);
    EXPECT_TRUE((std::string("::") == normalized));
  }

  // normalize delimiter + name (with prefix)
  {
    irs::string_ref analyzer = "::name";
    auto normalized =
        arangodb::iresearch::IResearchAnalyzerFeature::normalize(analyzer, active,
                                                                 system, true);
    EXPECT_TRUE((std::string("system::name") == normalized));
  }

  // normalize delimiter + name (without prefix)
  {
    irs::string_ref analyzer = "::name";
    auto normalized =
        arangodb::iresearch::IResearchAnalyzerFeature::normalize(analyzer, active,
                                                                 system, false);
    EXPECT_TRUE((std::string("::name") == normalized));
  }

  // normalize no-delimiter + name (with prefix)
  {
    irs::string_ref analyzer = "name";
    auto normalized =
        arangodb::iresearch::IResearchAnalyzerFeature::normalize(analyzer, active,
                                                                 system, true);
    EXPECT_TRUE((std::string("active::name") == normalized));
  }

  // normalize no-delimiter + name (without prefix)
  {
    irs::string_ref analyzer = "name";
    auto normalized =
        arangodb::iresearch::IResearchAnalyzerFeature::normalize(analyzer, active,
                                                                 system, false);
    EXPECT_TRUE((std::string("name") == normalized));
  }

  // normalize system + delimiter (with prefix)
  {
    irs::string_ref analyzer = "system::";
    auto normalized =
        arangodb::iresearch::IResearchAnalyzerFeature::normalize(analyzer, active,
                                                                 system, true);
    EXPECT_TRUE((std::string("system::") == normalized));
  }

  // normalize system + delimiter (without prefix)
  {
    irs::string_ref analyzer = "system::";
    auto normalized =
        arangodb::iresearch::IResearchAnalyzerFeature::normalize(analyzer, active,
                                                                 system, false);
    EXPECT_TRUE((std::string("::") == normalized));
  }

  // normalize vocbase + delimiter (with prefix)
  {
    irs::string_ref analyzer = "active::";
    auto normalized =
        arangodb::iresearch::IResearchAnalyzerFeature::normalize(analyzer, active,
                                                                 system, true);
    EXPECT_TRUE((std::string("active::") == normalized));
  }

  // normalize vocbase + delimiter (without prefix)
  {
    irs::string_ref analyzer = "active::";
    auto normalized =
        arangodb::iresearch::IResearchAnalyzerFeature::normalize(analyzer, active,
                                                                 system, false);
    EXPECT_TRUE((std::string("") == normalized));
  }

  // normalize system + delimiter + name (with prefix)
  {
    irs::string_ref analyzer = "system::name";
    auto normalized =
        arangodb::iresearch::IResearchAnalyzerFeature::normalize(analyzer, active,
                                                                 system, true);
    EXPECT_TRUE((std::string("system::name") == normalized));
  }

  // normalize system + delimiter + name (without prefix)
  {
    irs::string_ref analyzer = "system::name";
    auto normalized =
        arangodb::iresearch::IResearchAnalyzerFeature::normalize(analyzer, active,
                                                                 system, false);
    EXPECT_TRUE((std::string("::name") == normalized));
  }

  // normalize system + delimiter + name (without prefix) in system
  {
    irs::string_ref analyzer = "system::name";
    auto normalized =
        arangodb::iresearch::IResearchAnalyzerFeature::normalize(analyzer, system,
                                                                 system, false);
    EXPECT_TRUE((std::string("name") == normalized));
  }

  // normalize vocbase + delimiter + name (with prefix)
  {
    irs::string_ref analyzer = "active::name";
    auto normalized =
        arangodb::iresearch::IResearchAnalyzerFeature::normalize(analyzer, active,
                                                                 system, true);
    EXPECT_TRUE((std::string("active::name") == normalized));
  }

  // normalize vocbase + delimiter + name (without prefix)
  {
    irs::string_ref analyzer = "active::name";
    auto normalized =
        arangodb::iresearch::IResearchAnalyzerFeature::normalize(analyzer, active,
                                                                 system, false);
    EXPECT_TRUE((std::string("name") == normalized));
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                        static_analyzer test suite
// -----------------------------------------------------------------------------

TEST_F(IResearchAnalyzerFeatureTest, test_static_analyzer_features) {
  // test registered 'identity'
  arangodb::iresearch::IResearchAnalyzerFeature feature(server);
  feature.prepare();  // add static analyzers
  for (auto& analyzerEntry : staticAnalyzers()) {
    EXPECT_TRUE((false == !feature.get(analyzerEntry.first)));
    auto pool = feature.get(analyzerEntry.first);
    ASSERT_TRUE((false == !pool));
    EXPECT_TRUE(analyzerEntry.second.features == pool->features());
    EXPECT_TRUE(analyzerEntry.first == pool->name());
    auto analyzer = pool->get();
    EXPECT_TRUE((false == !analyzer));
    auto& term = analyzer->attributes().get<irs::term_attribute>();
    EXPECT_TRUE((false == !term));
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                            persistence test suite
// -----------------------------------------------------------------------------

TEST_F(IResearchAnalyzerFeatureTest, test_persistence) {
  static std::vector<std::string> const EMPTY;
  auto* database =
      arangodb::application_features::ApplicationServer::lookupFeature<arangodb::SystemDatabaseFeature>();
  auto vocbase = database->use();

  // read invalid configuration (missing attributes)
  {
    {
      std::string collection(arangodb::tests::AnalyzerCollectionName);
      arangodb::OperationOptions options;
      arangodb::SingleCollectionTransaction trx(
          arangodb::transaction::StandaloneContext::Create(*vocbase),
          collection, arangodb::AccessMode::Type::WRITE);
      trx.begin();
      trx.truncate(collection, options);
      trx.insert(collection, VPackParser::fromJson("{}")->slice(), options);
      trx.insert(collection,
                 VPackParser::fromJson("{\"type\": \"identity\", "
                                       "\"properties\": null}")
                     ->slice(),
                 options);
      trx.insert(collection,
                 VPackParser::fromJson(
                     "{\"name\": 12345,        \"type\": \"identity\", "
                     "\"properties\": null}")
                     ->slice(),
                 options);
      trx.insert(collection,
                 VPackParser::fromJson(
                     "{\"name\": \"invalid1\",                         "
                     "\"properties\": null}")
                     ->slice(),
                 options);
      trx.insert(collection,
                 VPackParser::fromJson(
                     "{\"name\": \"invalid2\", \"type\": 12345,        "
                     "\"properties\": null}")
                     ->slice(),
                 options);
      trx.commit();
    }

    std::map<std::string, std::pair<irs::string_ref, irs::string_ref>> expected = {};
    arangodb::iresearch::IResearchAnalyzerFeature feature(server);

    feature.start();  // load persisted analyzers

    feature.visit(
        [&expected](arangodb::iresearch::IResearchAnalyzerFeature::AnalyzerPool::ptr const& analyzer) -> bool {
          if (staticAnalyzers().find(analyzer->name()) != staticAnalyzers().end()) {
            return true;  // skip static analyzers
          }

          auto itr = expected.find(analyzer->name());
          EXPECT_TRUE((itr != expected.end()));
          EXPECT_TRUE((itr->second.first == analyzer->type()));
          EXPECT_TRUE((itr->second.second == analyzer->properties().toString()));
          expected.erase(itr);
          return true;
        });
    EXPECT_TRUE((expected.empty()));
  }

  // read invalid configuration (duplicate non-identical records)
  {
    {
      std::string collection(arangodb::tests::AnalyzerCollectionName);
      arangodb::OperationOptions options;
      arangodb::SingleCollectionTransaction trx(
          arangodb::transaction::StandaloneContext::Create(*vocbase),
          collection, arangodb::AccessMode::Type::WRITE);
      trx.begin();
      trx.truncate(collection, options);
      trx.insert(collection,
                 VPackParser::fromJson(
                     "{\"name\": \"valid\", \"type\": \"TestAnalyzer\", "
                     "\"properties\": {\"args\":\"abcd\"} }")
                     ->slice(),
                 options);
      trx.insert(collection,
                 VPackParser::fromJson(
                     "{\"name\": \"valid\", \"type\": \"TestAnalyzer\", "
                     "\"properties\": {\"args\":\"abc\"} }")
                     ->slice(),
                 options);
      trx.commit();
    }

    arangodb::iresearch::IResearchAnalyzerFeature feature(server);
    EXPECT_ANY_THROW((feature.start()));
  }

  // read valid configuration (different parameter options)
  {
    {
      std::string collection(arangodb::tests::AnalyzerCollectionName);
      arangodb::OperationOptions options;
      arangodb::SingleCollectionTransaction trx(
          arangodb::transaction::StandaloneContext::Create(*vocbase),
          collection, arangodb::AccessMode::Type::WRITE);
      trx.begin();
      trx.truncate(collection, options);
      trx.insert(collection,
                 VPackParser::fromJson(
                     "{\"name\": \"valid0\", \"type\": \"identity\", "
                     "\"properties\": {}                      }")
                     ->slice(),
                 options);
      trx.insert(collection,
                 VPackParser::fromJson(
                     "{\"name\": \"valid1\", \"type\": \"identity\", "
                     "\"properties\": true                      }")
                     ->slice(),
                 options);
      trx.insert(collection,
                 VPackParser::fromJson(
                     "{\"name\": \"valid2\", \"type\": \"identity\", "
                     "\"properties\": {\"args\":\"abc\"}                  }")
                     ->slice(),
                 options);
      trx.insert(collection,
                 VPackParser::fromJson(
                     "{\"name\": \"valid3\", \"type\": \"identity\", "
                     "\"properties\": 3.14                      }")
                     ->slice(),
                 options);
      trx.insert(collection,
                 VPackParser::fromJson(
                     "{\"name\": \"valid4\", \"type\": \"identity\", "
                     "\"properties\": [ 1, \"abc\" ]            }")
                     ->slice(),
                 options);
      trx.insert(collection,
                 VPackParser::fromJson(
                     "{\"name\": \"valid5\", \"type\": \"identity\", "
                     "\"properties\": { \"a\": 7, \"b\": \"c\" }}")
                     ->slice(),
                 options);
      trx.commit();
    }

    std::map<std::string, std::pair<irs::string_ref, irs::string_ref>> expected = {
        {arangodb::StaticStrings::SystemDatabase + "::valid0",
         {"identity", "{\n}"}},
        {arangodb::StaticStrings::SystemDatabase + "::valid2",
         {"identity", "{\n}"}},
        {arangodb::StaticStrings::SystemDatabase + "::valid4",
         {"identity", "{\n}"}},
        {arangodb::StaticStrings::SystemDatabase + "::valid5",
         {"identity", "{\n}"}},
    };
    arangodb::iresearch::IResearchAnalyzerFeature feature(server);

    feature.start();  // load persisted analyzers

    feature.visit(
        [&expected](arangodb::iresearch::IResearchAnalyzerFeature::AnalyzerPool::ptr const& analyzer) -> bool {
          if (staticAnalyzers().find(analyzer->name()) != staticAnalyzers().end()) {
            return true;  // skip static analyzers
          }

          auto itr = expected.find(analyzer->name());
          EXPECT_TRUE((itr != expected.end()));
          EXPECT_EQ(itr->second.first, analyzer->type());
          EXPECT_EQ(itr->second.second, analyzer->properties().toString());
          expected.erase(itr);
          return true;
        });
    EXPECT_TRUE((expected.empty()));
  }

  // add new records
  {{arangodb::OperationOptions options;
  arangodb::ManagedDocumentResult result;
  auto collection = vocbase->lookupCollection(arangodb::tests::AnalyzerCollectionName);
  arangodb::transaction::Methods trx(arangodb::transaction::StandaloneContext::Create(*vocbase),
                                     EMPTY, EMPTY, EMPTY,
                                     arangodb::transaction::Options());
  EXPECT_TRUE((collection->truncate(trx, options).ok()));
}

{
  arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;
  arangodb::iresearch::IResearchAnalyzerFeature feature(server);

  EXPECT_TRUE(
      (feature
           .emplace(result, arangodb::StaticStrings::SystemDatabase + "::valid",
                    "identity", VPackParser::fromJson("{\"args\":\"abc\"}")->slice())
           .ok()));
  EXPECT_TRUE((result.first));
  EXPECT_TRUE((result.second));
}

{
  std::map<std::string, std::pair<irs::string_ref, irs::string_ref>> expected = {
      {arangodb::StaticStrings::SystemDatabase + "::valid",
       {"identity", "{\n}"}},
  };
  arangodb::iresearch::IResearchAnalyzerFeature feature(server);

  feature.start();  // load persisted analyzers

  feature.visit(
      [&expected](arangodb::iresearch::IResearchAnalyzerFeature::AnalyzerPool::ptr const& analyzer) -> bool {
        if (staticAnalyzers().find(analyzer->name()) != staticAnalyzers().end()) {
          return true;  // skip static analyzers
        }

        auto itr = expected.find(analyzer->name());
        EXPECT_TRUE((itr != expected.end()));
        EXPECT_EQ(itr->second.first, analyzer->type());
        EXPECT_EQ(itr->second.second, analyzer->properties().toString());
        expected.erase(itr);
        return true;
      });
  EXPECT_TRUE((expected.empty()));
}
}

// remove existing records
{{std::string collection(arangodb::tests::AnalyzerCollectionName);
arangodb::OperationOptions options;
arangodb::SingleCollectionTransaction trx(arangodb::transaction::StandaloneContext::Create(*vocbase),
                                          collection, arangodb::AccessMode::Type::WRITE);
trx.begin();
trx.truncate(collection, options);
trx.insert(
    collection,
    VPackParser::fromJson(
        "{\"name\": \"valid\", \"type\": \"identity\", \"properties\": {}}")
        ->slice(),
    options);
trx.commit();
}

{
  std::map<std::string, std::pair<irs::string_ref, irs::string_ref>> expected = {
      {"identity", {"identity", "{\n}"}},
      {"text_de",
       {"text",
        "{ \"locale\": \"de.UTF-8\", \"caseConvert\": \"lower\", "
        "\"stopwords\": [ ], \"noAccent\": true, \"noStrem\": false }"}},
      {"text_en",
       {"text",
        "{ \"locale\": \"en.UTF-8\", \"caseConvert\": \"lower\", "
        "\"stopwords\": [ ], \"noAccent\": true, \"noStrem\": false }"}},
      {"text_es",
       {"text",
        "{ \"locale\": \"es.UTF-8\", \"caseConvert\": \"lower\", "
        "\"stopwords\": [ ], \"noAccent\": true, \"noStrem\": false }"}},
      {"text_fi",
       {"text",
        "{ \"locale\": \"fi.UTF-8\", \"caseConvert\": \"lower\", "
        "\"stopwords\": [ ], \"noAccent\": true, \"noStrem\": false }"}},
      {"text_fr",
       {"text",
        "{ \"locale\": \"fr.UTF-8\", \"caseConvert\": \"lower\", "
        "\"stopwords\": [ ], \"noAccent\": true, \"noStrem\": false }"}},
      {"text_it",
       {"text",
        "{ \"locale\": \"it.UTF-8\", \"caseConvert\": \"lower\", "
        "\"stopwords\": [ ], \"noAccent\": true, \"noStrem\": false }"}},
      {"text_nl",
       {"text",
        "{ \"locale\": \"nl.UTF-8\", \"caseConvert\": \"lower\", "
        "\"stopwords\": [ ], \"noAccent\": true, \"noStrem\": false }"}},
      {"text_no",
       {"text",
        "{ \"locale\": \"no.UTF-8\", \"caseConvert\": \"lower\", "
        "\"stopwords\": [ ], \"noAccent\": true, \"noStrem\": false }"}},
      {"text_pt",
       {"text",
        "{ \"locale\": \"pt.UTF-8\", \"caseConvert\": \"lower\", "
        "\"stopwords\": [ ], \"noAccent\": true, \"noStrem\": false }"}},
      {"text_ru",
       {"text",
        "{ \"locale\": \"ru.UTF-8\", \"caseConvert\": \"lower\", "
        "\"stopwords\": [ ], \"noAccent\": true, \"noStrem\": false }"}},
      {"text_sv",
       {"text",
        "{ \"locale\": \"sv.UTF-8\", \"caseConvert\": \"lower\", "
        "\"stopwords\": [ ], \"noAccent\": true, \"noStrem\": false }"}},
      {"text_zh",
       {"text",
        "{ \"locale\": \"zh.UTF-8\", \"caseConvert\": \"lower\", "
        "\"stopwords\": [ ], \"noAccent\": true, \"noStrem\": false }"}},
      {arangodb::StaticStrings::SystemDatabase + "::valid", {"identity", "{}"}},
  };
  arangodb::iresearch::IResearchAnalyzerFeature feature(server);

  feature.prepare();  // load static analyzers
  feature.start();    // load persisted analyzers

  feature.visit([&expected](arangodb::iresearch::IResearchAnalyzerFeature::AnalyzerPool::ptr const& analyzer) -> bool {
    if ((analyzer->name() != "identity" &&
         !irs::starts_with(irs::string_ref(analyzer->name()), "text_")) &&
        staticAnalyzers().find(analyzer->name()) != staticAnalyzers().end()) {
      return true;  // skip static analyzers
    }

    auto itr = expected.find(analyzer->name());
    EXPECT_TRUE((itr != expected.end()));
    EXPECT_TRUE((itr->second.first == analyzer->type()));

    std::string expectedProperties;
    EXPECT_TRUE(irs::analysis::analyzers::normalize(
        expectedProperties, analyzer->type(), irs::text_format::vpack,
        arangodb::iresearch::ref<char>(VPackParser::fromJson(itr->second.second)->slice()),
        false));

    EXPECT_EQUAL_SLICES(arangodb::iresearch::slice(expectedProperties),
                        analyzer->properties());
    expected.erase(itr);
    return true;
  });

  EXPECT_TRUE((expected.empty()));
  EXPECT_TRUE(
      (true ==
       feature.remove(arangodb::StaticStrings::SystemDatabase + "::valid").ok()));
  EXPECT_TRUE((false == feature.remove("identity").ok()));
}

{
  std::map<std::string, std::pair<irs::string_ref, irs::string_ref>> expected = {};
  arangodb::iresearch::IResearchAnalyzerFeature feature(server);

  feature.start();  // load persisted analyzers

  feature.visit(
      [&expected](arangodb::iresearch::IResearchAnalyzerFeature::AnalyzerPool::ptr const& analyzer) -> bool {
        if (staticAnalyzers().find(analyzer->name()) != staticAnalyzers().end()) {
          return true;  // skip static analyzers
        }

        auto itr = expected.find(analyzer->name());
        EXPECT_TRUE((itr != expected.end()));
        EXPECT_TRUE((itr->second.first == analyzer->type()));
        EXPECT_TRUE((itr->second.second == analyzer->properties().toString()));
        expected.erase(itr);
        return true;
      });
  EXPECT_TRUE((expected.empty()));
}
}

// emplace on single-server (should persist)
{
  arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;
  arangodb::iresearch::IResearchAnalyzerFeature feature(server);
  EXPECT_TRUE(
      (true == feature
                   .emplace(result, arangodb::StaticStrings::SystemDatabase + "::test_analyzerA",
                            "TestAnalyzer", VPackParser::fromJson("\"abc\"")->slice(),
                            {irs::frequency::type()})
                   .ok()));
  EXPECT_TRUE((false == !result.first));
  EXPECT_TRUE((false == !feature.get(arangodb::StaticStrings::SystemDatabase + "::test_analyzerA")));
  EXPECT_TRUE((false == !vocbase->lookupCollection(arangodb::tests::AnalyzerCollectionName)));
  arangodb::OperationOptions options;
  arangodb::SingleCollectionTransaction trx(
      arangodb::transaction::StandaloneContext::Create(*vocbase),
      arangodb::tests::AnalyzerCollectionName, arangodb::AccessMode::Type::WRITE);
  EXPECT_TRUE((trx.begin().ok()));
  auto queryResult = trx.all(arangodb::tests::AnalyzerCollectionName, 0, 2, options);
  EXPECT_TRUE((true == queryResult.ok()));
  auto slice = arangodb::velocypack::Slice(queryResult.buffer->data());
  EXPECT_TRUE((slice.isArray() && 1 == slice.length()));
  slice = slice.at(0);
  EXPECT_TRUE((slice.isObject()));
  EXPECT_TRUE((slice.hasKey("name") && slice.get("name").isString() &&
               std::string("test_analyzerA") == slice.get("name").copyString()));
  EXPECT_TRUE((slice.hasKey("type") && slice.get("type").isString() &&
               std::string("TestAnalyzer") == slice.get("type").copyString()));
  EXPECT_TRUE((slice.hasKey("properties") && slice.get("properties").isObject() &&
               VPackParser::fromJson("{\"args\":\"abc\"}")->slice().toString() ==
                   slice.get("properties").toString()));
  EXPECT_TRUE((slice.hasKey("features") && slice.get("features").isArray() &&
               1 == slice.get("features").length() &&
               slice.get("features").at(0).isString() &&
               std::string("frequency") == slice.get("features").at(0).copyString()));
  EXPECT_TRUE((trx.truncate(arangodb::tests::AnalyzerCollectionName, options).ok()));
  EXPECT_TRUE((trx.commit().ok()));
}
}

TEST_F(IResearchAnalyzerFeatureTest, test_remove) {
  auto* dbFeature =
      arangodb::application_features::ApplicationServer::lookupFeature<arangodb::DatabaseFeature>(
          "Database");
  ASSERT_TRUE((false == !dbFeature));
  arangodb::AqlFeature aqlFeature(server);
  aqlFeature.start();  // required for Query::Query(...), must not call ~AqlFeature() for the duration of the test

  // remove existing
  {
    arangodb::iresearch::IResearchAnalyzerFeature feature(server);
    feature.prepare();  // add static analyzers

    // add analyzer
    {
      arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;
      ASSERT_TRUE(
          (true == feature
                       .emplace(result, arangodb::StaticStrings::SystemDatabase + "::test_analyzer0",
                                "TestAnalyzer", VPackParser::fromJson("\"abc\"")->slice())
                       .ok()));
      ASSERT_TRUE((false == !feature.get(arangodb::StaticStrings::SystemDatabase +
                                         "::test_analyzer0")));
    }

    EXPECT_TRUE((true == feature
                             .remove(arangodb::StaticStrings::SystemDatabase + "::test_analyzer0")
                             .ok()));
    EXPECT_TRUE((true == !feature.get(arangodb::StaticStrings::SystemDatabase + "::test_analyzer0")));
  }

  // remove existing (inRecovery) single-server
  {
    arangodb::iresearch::IResearchAnalyzerFeature feature(server);

    // add analyzer
    {
      arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;
      ASSERT_TRUE(
          (true == feature
                       .emplace(result, arangodb::StaticStrings::SystemDatabase + "::test_analyzer0",
                                "TestAnalyzer", VPackParser::fromJson("\"abc\"")->slice())
                       .ok()));
      ASSERT_TRUE((false == !feature.get(arangodb::StaticStrings::SystemDatabase +
                                         "::test_analyzer0")));
    }

    auto before = StorageEngineMock::recoveryStateResult;
    StorageEngineMock::recoveryStateResult = arangodb::RecoveryState::IN_PROGRESS;
    auto restore = irs::make_finally(
        [&before]() -> void { StorageEngineMock::recoveryStateResult = before; });

    EXPECT_TRUE((false == feature
                              .remove(arangodb::StaticStrings::SystemDatabase + "::test_analyzer0")
                              .ok()));
    EXPECT_TRUE((false == !feature.get(arangodb::StaticStrings::SystemDatabase + "::test_analyzer0")));
  }

  // remove existing (dbserver)
  {
    auto beforeRole = arangodb::ServerState::instance()->getRole();
    arangodb::ServerState::instance()->setRole(arangodb::ServerState::ROLE_DBSERVER);
    auto restoreRole = irs::make_finally([&beforeRole]() -> void {
      arangodb::ServerState::instance()->setRole(beforeRole);
    });

    // create a new instance of an ApplicationServer and fill it with the required features
    // cannot use the existing server since its features already have some state
    std::shared_ptr<arangodb::application_features::ApplicationServer> originalServer(
        arangodb::application_features::ApplicationServer::server,
        [](arangodb::application_features::ApplicationServer* ptr) -> void {
          arangodb::application_features::ApplicationServer::server = ptr;
        });
    arangodb::application_features::ApplicationServer::server =
        nullptr;  // avoid "ApplicationServer initialized twice"
    arangodb::application_features::ApplicationServer server(nullptr, nullptr);
    arangodb::iresearch::IResearchAnalyzerFeature* feature;
    arangodb::DatabaseFeature* dbFeature;
    arangodb::SystemDatabaseFeature* sysDatabase;
    server.addFeature(new arangodb::ClusterFeature(server));  // required to create ClusterInfo instance
    server.addFeature(dbFeature = new arangodb::DatabaseFeature(server));  // required for IResearchAnalyzerFeature::emplace(...)
    server.addFeature(new arangodb::QueryRegistryFeature(server));  // required for constructing TRI_vocbase_t
    server.addFeature(new arangodb::ShardingFeature(server));  // required for Collections::create(...)
    server.addFeature(sysDatabase = new arangodb::SystemDatabaseFeature(server));  // required for IResearchAnalyzerFeature::start()
    server.addFeature(new arangodb::V8DealerFeature(server));  // required for DatabaseFeature::createDatabase(...)
    server.addFeature(new arangodb::application_features::CommunicationFeaturePhase(
        server));  // required for SimpleHttpClient::doRequest()
    server.addFeature(feature = new arangodb::iresearch::IResearchAnalyzerFeature(server));  // required for running upgrade task
    arangodb::aql::OptimizerRulesFeature(this->server).prepare();  // required for Query::preparePlan(...)
    auto clearOptimizerRules = irs::make_finally([this]() -> void {
      arangodb::aql::OptimizerRulesFeature(this->server).unprepare();
    });

    auto cleanup = arangodb::scopeGuard([dbFeature]() { dbFeature->unprepare(); });

    // create system vocbase (before feature start)
    {
      auto const databases = VPackParser::fromJson(
          std::string("[ { \"name\": \"") +
          arangodb::StaticStrings::SystemDatabase + "\" } ]");
      EXPECT_TRUE((TRI_ERROR_NO_ERROR == dbFeature->loadDatabases(databases->slice())));
      sysDatabase->start();  // get system database from DatabaseFeature
    }

    ClusterCommMock clusterComm;
    auto scopedClusterComm = ClusterCommMock::setInstance(
        clusterComm);  // or get SIGFPE in ClusterComm::communicator() while call to ClusterInfo::createDocumentOnCoordinator(...)

    // insert response for expected empty initial analyzer list
    {
      arangodb::ClusterCommResult response;
      response.operationID = 1;  // sequential non-zero value
      response.status = arangodb::ClusterCommOpStatus::CL_COMM_RECEIVED;
      response.answer_code = arangodb::rest::ResponseCode::CREATED;
      response.answer = std::make_shared<GeneralRequestMock>(*(sysDatabase->use()));
      static_cast<GeneralRequestMock*>(response.answer.get())->_payload =
          *VPackParser::fromJson("{ \"result\": [] }");  // empty initial result
      clusterComm._responses.emplace_back(std::move(response));
    }

    // add analyzer
    {
      arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;
      ASSERT_TRUE((true == !feature->get(arangodb::StaticStrings::SystemDatabase +
                                         "::test_analyzer2")));
      ASSERT_TRUE((true == feature
                               ->emplace(result, arangodb::StaticStrings::SystemDatabase + "::test_analyzer2",
                                         "TestAnalyzer",
                                         VPackParser::fromJson("\"abc\"")->slice())
                               .ok()));
      ASSERT_TRUE((false == !feature->get(arangodb::StaticStrings::SystemDatabase +
                                          "::test_analyzer2")));
    }

    EXPECT_TRUE((true == feature
                             ->remove(arangodb::StaticStrings::SystemDatabase + "::test_analyzer2")
                             .ok()));
    EXPECT_TRUE((true == !feature->get(arangodb::StaticStrings::SystemDatabase + "::test_analyzer2")));
  }

  // remove existing (inRecovery) dbserver
  {
    auto beforeRole = arangodb::ServerState::instance()->getRole();
    arangodb::ServerState::instance()->setRole(arangodb::ServerState::ROLE_DBSERVER);
    auto restoreRole = irs::make_finally([&beforeRole]() -> void {
      arangodb::ServerState::instance()->setRole(beforeRole);
    });

    // create a new instance of an ApplicationServer and fill it with the required features
    // cannot use the existing server since its features already have some state
    std::shared_ptr<arangodb::application_features::ApplicationServer> originalServer(
        arangodb::application_features::ApplicationServer::server,
        [](arangodb::application_features::ApplicationServer* ptr) -> void {
          arangodb::application_features::ApplicationServer::server = ptr;
        });
    arangodb::application_features::ApplicationServer::server =
        nullptr;  // avoid "ApplicationServer initialized twice"
    arangodb::application_features::ApplicationServer server(nullptr, nullptr);
    arangodb::iresearch::IResearchAnalyzerFeature* feature;
    arangodb::DatabaseFeature* dbFeature;
    arangodb::SystemDatabaseFeature* sysDatabase;
    server.addFeature(new arangodb::ClusterFeature(server));  // required to create ClusterInfo instance
    server.addFeature(dbFeature = new arangodb::DatabaseFeature(server));  // required for IResearchAnalyzerFeature::emplace(...)
    server.addFeature(new arangodb::QueryRegistryFeature(server));  // required for constructing TRI_vocbase_t
    server.addFeature(new arangodb::ShardingFeature(server));  // required for Collections::create(...)
    server.addFeature(sysDatabase = new arangodb::SystemDatabaseFeature(server));  // required for IResearchAnalyzerFeature::start()
    server.addFeature(new arangodb::V8DealerFeature(server));  // required for DatabaseFeature::createDatabase(...)
    server.addFeature(new arangodb::application_features::CommunicationFeaturePhase(
        server));  // required for SimpleHttpClient::doRequest()
    server.addFeature(feature = new arangodb::iresearch::IResearchAnalyzerFeature(server));  // required for running upgrade task
    arangodb::aql::OptimizerRulesFeature(this->server).prepare();  // required for Query::preparePlan(...)
    auto clearOptimizerRules = irs::make_finally([this]() -> void {
      arangodb::aql::OptimizerRulesFeature(this->server).unprepare();
    });

    auto cleanup = arangodb::scopeGuard([dbFeature]() { dbFeature->unprepare(); });

    // create system vocbase (before feature start)
    {
      auto const databases = VPackParser::fromJson(
          std::string("[ { \"name\": \"") +
          arangodb::StaticStrings::SystemDatabase + "\" } ]");
      EXPECT_TRUE((TRI_ERROR_NO_ERROR == dbFeature->loadDatabases(databases->slice())));
      sysDatabase->start();  // get system database from DatabaseFeature
    }

    ClusterCommMock clusterComm;
    auto scopedClusterComm = ClusterCommMock::setInstance(
        clusterComm);  // or get SIGFPE in ClusterComm::communicator() while call to ClusterInfo::createDocumentOnCoordinator(...)

    // insert response for expected empty initial analyzer list
    {
      arangodb::ClusterCommResult response;
      response.operationID = 1;  // sequential non-zero value
      response.status = arangodb::ClusterCommOpStatus::CL_COMM_RECEIVED;
      response.answer_code = arangodb::rest::ResponseCode::CREATED;
      response.answer = std::make_shared<GeneralRequestMock>(*(sysDatabase->use()));
      static_cast<GeneralRequestMock*>(response.answer.get())->_payload =
          *VPackParser::fromJson("{ \"result\": [] }");  // empty initial result
      clusterComm._responses.emplace_back(std::move(response));
    }

    // add analyzer
    {
      arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;
      ASSERT_TRUE((true == !feature->get(arangodb::StaticStrings::SystemDatabase +
                                         "::test_analyzer2")));
      ASSERT_TRUE((true == feature
                               ->emplace(result, arangodb::StaticStrings::SystemDatabase + "::test_analyzer2",
                                         "TestAnalyzer",
                                         VPackParser::fromJson("\"abc\"")->slice())
                               .ok()));
      ASSERT_TRUE((false == !feature->get(arangodb::StaticStrings::SystemDatabase +
                                          "::test_analyzer2")));
    }

    auto before = StorageEngineMock::recoveryStateResult;
    StorageEngineMock::recoveryStateResult = arangodb::RecoveryState::IN_PROGRESS;
    auto restore = irs::make_finally(
        [&before]() -> void { StorageEngineMock::recoveryStateResult = before; });

    EXPECT_TRUE((true == feature
                             ->remove(arangodb::StaticStrings::SystemDatabase + "::test_analyzer2")
                             .ok()));
    EXPECT_TRUE((true == !feature->get(arangodb::StaticStrings::SystemDatabase + "::test_analyzer2")));
  }

  // remove existing (in-use)
  {
    arangodb::iresearch::IResearchAnalyzerFeature feature(server);
    arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;  // will keep reference
    ASSERT_TRUE(
        (true == feature
                     .emplace(result, arangodb::StaticStrings::SystemDatabase + "::test_analyzer3",
                              "TestAnalyzer", VPackParser::fromJson("\"abc\"")->slice())
                     .ok()));
    ASSERT_TRUE((false == !feature.get(arangodb::StaticStrings::SystemDatabase + "::test_analyzer3")));

    EXPECT_TRUE((false == feature
                              .remove(arangodb::StaticStrings::SystemDatabase + "::test_analyzer3", false)
                              .ok()));
    EXPECT_TRUE((false == !feature.get(arangodb::StaticStrings::SystemDatabase + "::test_analyzer3")));
    EXPECT_TRUE((true == feature
                             .remove(arangodb::StaticStrings::SystemDatabase + "::test_analyzer3", true)
                             .ok()));
    EXPECT_TRUE((true == !feature.get(arangodb::StaticStrings::SystemDatabase + "::test_analyzer3")));
  }

  // remove missing (no vocbase)
  {
    arangodb::iresearch::IResearchAnalyzerFeature feature(server);
    ASSERT_TRUE((nullptr == dbFeature->lookupDatabase("testVocbase")));

    EXPECT_TRUE((true == !feature.get("testVocbase::test_analyzer")));
    EXPECT_TRUE((false == feature.remove("testVocbase::test_analyzer").ok()));
  }

  // remove missing (no collection)
  {
    arangodb::iresearch::IResearchAnalyzerFeature feature(server);
    TRI_vocbase_t* vocbase;
    ASSERT_TRUE((TRI_ERROR_NO_ERROR ==
                 dbFeature->createDatabase(1, "testVocbase", vocbase)));
    ASSERT_TRUE((nullptr != dbFeature->lookupDatabase("testVocbase")));

    EXPECT_TRUE((true == !feature.get("testVocbase::test_analyzer")));
    EXPECT_TRUE((false == feature.remove("testVocbase::test_analyzer").ok()));
  }

  // remove invalid
  {
    arangodb::iresearch::IResearchAnalyzerFeature feature(server);
    EXPECT_TRUE((true == !feature.get(arangodb::StaticStrings::SystemDatabase + "::test_analyzer")));
    EXPECT_TRUE((false == feature
                              .remove(arangodb::StaticStrings::SystemDatabase + "::test_analyzer")
                              .ok()));
  }

  // remove static analyzer
  {
    arangodb::iresearch::IResearchAnalyzerFeature feature(server);
    feature.prepare();  // add static analyzers
    EXPECT_TRUE((false == !feature.get("identity")));
    EXPECT_TRUE((false == feature.remove("identity").ok()));
    EXPECT_TRUE((false == !feature.get("identity")));
  }
}

TEST_F(IResearchAnalyzerFeatureTest, test_prepare) {
  auto before = StorageEngineMock::recoveryStateResult;
  StorageEngineMock::recoveryStateResult = arangodb::RecoveryState::IN_PROGRESS;
  auto restore = irs::make_finally(
      [&before]() -> void { StorageEngineMock::recoveryStateResult = before; });
  arangodb::iresearch::IResearchAnalyzerFeature feature(server);
  EXPECT_TRUE(feature.visit([](auto) { return false; }));  // ensure feature is empty after creation
  feature.prepare();  // add static analyzers

  // check static analyzers
  auto expected = staticAnalyzers();
  feature.visit([&expected, &feature](
                    arangodb::iresearch::IResearchAnalyzerFeature::AnalyzerPool::ptr const& analyzer) -> bool {
    auto itr = expected.find(analyzer->name());
    EXPECT_TRUE((itr != expected.end()));
    EXPECT_TRUE((itr->second.type == analyzer->type()));

    std::string expectedProperties;
    EXPECT_TRUE(irs::analysis::analyzers::normalize(
        expectedProperties, analyzer->type(), irs::text_format::vpack,
        arangodb::iresearch::ref<char>(itr->second.properties), false));

    EXPECT_EQUAL_SLICES(arangodb::iresearch::slice(expectedProperties),
                        analyzer->properties());
    EXPECT_TRUE(
        (itr->second.features.is_subset_of(feature.get(analyzer->name())->features())));
    expected.erase(itr);
    return true;
  });
  EXPECT_TRUE((expected.empty()));
}

TEST_F(IResearchAnalyzerFeatureTest, test_start) {
  auto* database =
      arangodb::application_features::ApplicationServer::lookupFeature<arangodb::SystemDatabaseFeature>();
  auto vocbase = database->use();

  // test feature start load configuration (inRecovery, no configuration collection)
  {
    // ensure no configuration collection
    {
      auto collection = vocbase->lookupCollection(arangodb::tests::AnalyzerCollectionName);

      if (collection) {
        vocbase->dropCollection(collection->id(), true, -1);
      }

      collection = vocbase->lookupCollection(arangodb::tests::AnalyzerCollectionName);
      EXPECT_TRUE((nullptr == collection));
    }
==== BASE ====
    
    auto before = StorageEngineMock::recoveryStateResult;
    StorageEngineMock::recoveryStateResult = arangodb::RecoveryState::IN_PROGRESS;
    auto restore = irs::make_finally(
        [&before]() -> void { StorageEngineMock::recoveryStateResult = before; });
    arangodb::iresearch::IResearchAnalyzerFeature feature(server);
    feature.prepare();  // add static analyzers
    feature.start();    // load persisted analyzers
    EXPECT_TRUE((nullptr == vocbase->lookupCollection(arangodb::tests::AnalyzerCollectionName)));

    auto expected = staticAnalyzers();

    feature.visit(
        [&expected, &feature](
            arangodb::iresearch::IResearchAnalyzerFeature::AnalyzerPool::ptr const& analyzer) -> bool {
          auto itr = expected.find(analyzer->name());
          EXPECT_TRUE((itr != expected.end()));
          EXPECT_TRUE((itr->second.type == analyzer->type()));

          std::string expectedProperties;
          EXPECT_TRUE(irs::analysis::analyzers::normalize(
              expectedProperties, analyzer->type(), irs::text_format::vpack,
              arangodb::iresearch::ref<char>(itr->second.properties), false));

          EXPECT_EQUAL_SLICES(arangodb::iresearch::slice(expectedProperties),
                              analyzer->properties());
          EXPECT_TRUE((itr->second.features.is_subset_of(
              feature.get(analyzer->name())->features())));
          expected.erase(itr);
          return true;
        });
    EXPECT_TRUE((expected.empty()));
  }

  // test feature start load configuration (inRecovery, with configuration collection)
  {
    // ensure there is an empty configuration collection
    {
      auto collection = vocbase->lookupCollection(arangodb::tests::AnalyzerCollectionName);

      if (collection) {
        vocbase->dropCollection(collection->id(), true, -1);
      }

      collection = vocbase->lookupCollection(arangodb::tests::AnalyzerCollectionName);
      EXPECT_TRUE((nullptr == collection));
      arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;
      arangodb::iresearch::IResearchAnalyzerFeature feature(server);
      arangodb::methods::Collections::createSystem(*vocbase, arangodb::tests::AnalyzerCollectionName);
      EXPECT_TRUE(
          (true == feature
                       .emplace(result, arangodb::StaticStrings::SystemDatabase + "::test_analyzer",
                                "identity", VPackParser::fromJson("\"abc\"")->slice())
                       .ok()));
      EXPECT_TRUE((false == !result.first));
      collection = vocbase->lookupCollection(arangodb::tests::AnalyzerCollectionName);
      EXPECT_TRUE((nullptr != collection));
    }

    auto before = StorageEngineMock::recoveryStateResult;
    StorageEngineMock::recoveryStateResult = arangodb::RecoveryState::IN_PROGRESS;
    auto restore = irs::make_finally(
        [&before]() -> void { StorageEngineMock::recoveryStateResult = before; });
    arangodb::iresearch::IResearchAnalyzerFeature feature(server);
    feature.prepare();  // add static analyzers
    feature.start();    // load persisted analyzers
    EXPECT_TRUE((nullptr != vocbase->lookupCollection(arangodb::tests::AnalyzerCollectionName)));

    auto expected = staticAnalyzers();
    auto expectedAnalyzer =
        arangodb::StaticStrings::SystemDatabase + "::test_analyzer";

    expected.emplace(std::piecewise_construct, std::forward_as_tuple(expectedAnalyzer),
                     std::forward_as_tuple("identity", "\"abc\""));
    feature.visit(
        [&expected, &feature](
            arangodb::iresearch::IResearchAnalyzerFeature::AnalyzerPool::ptr const& analyzer) -> bool {
          auto itr = expected.find(analyzer->name());
          EXPECT_TRUE((itr != expected.end()));
          EXPECT_TRUE((itr->second.type == analyzer->type()));

          std::string expectedProperties;
          EXPECT_TRUE(irs::analysis::analyzers::normalize(
              expectedProperties, analyzer->type(), irs::text_format::vpack,
              arangodb::iresearch::ref<char>(itr->second.properties), false));

          EXPECT_EQUAL_SLICES(arangodb::iresearch::slice(expectedProperties),
                              analyzer->properties());
          EXPECT_TRUE((itr->second.features.is_subset_of(
              feature.get(analyzer->name())->features())));
          expected.erase(itr);
          return true;
        });
    EXPECT_TRUE((expected.empty()));
  }

  // test feature start load configuration (no configuration collection)
  {
    // ensure no configuration collection
    {
      auto collection = vocbase->lookupCollection(arangodb::tests::AnalyzerCollectionName);

      if (collection) {
        vocbase->dropCollection(collection->id(), true, -1);
      }

      collection = vocbase->lookupCollection(arangodb::tests::AnalyzerCollectionName);
      EXPECT_TRUE((nullptr == collection));
    }
    arangodb::iresearch::IResearchAnalyzerFeature feature(server);
    feature.prepare();  // add static analyzers
    feature.start();    // load persisted analyzers
    EXPECT_TRUE((nullptr == vocbase->lookupCollection(arangodb::tests::AnalyzerCollectionName)));

    auto expected = staticAnalyzers();

    feature.visit(
        [&expected, &feature](
            arangodb::iresearch::IResearchAnalyzerFeature::AnalyzerPool::ptr const& analyzer) -> bool {
          auto itr = expected.find(analyzer->name());
          EXPECT_TRUE((itr != expected.end()));
          EXPECT_TRUE((itr->second.type == analyzer->type()));

          std::string expectedProperties;
          EXPECT_TRUE(irs::analysis::analyzers::normalize(
              expectedProperties, analyzer->type(), irs::text_format::vpack,
              arangodb::iresearch::ref<char>(itr->second.properties), false));

          EXPECT_EQUAL_SLICES(arangodb::iresearch::slice(expectedProperties),
                              analyzer->properties());

          EXPECT_TRUE((itr->second.features.is_subset_of(
              feature.get(analyzer->name())->features())));
          expected.erase(itr);
          return true;
        });
    EXPECT_TRUE((expected.empty()));
  }

  // test feature start load configuration (with configuration collection)
  {
    // ensure there is an empty configuration collection
    {
      auto collection = vocbase->lookupCollection(arangodb::tests::AnalyzerCollectionName);

      if (collection) {
        vocbase->dropCollection(collection->id(), true, -1);
      }

      collection = vocbase->lookupCollection(arangodb::tests::AnalyzerCollectionName);
      EXPECT_TRUE((nullptr == collection));
      arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;
      arangodb::iresearch::IResearchAnalyzerFeature feature(server);
      arangodb::methods::Collections::createSystem(*vocbase, arangodb::tests::AnalyzerCollectionName);
      EXPECT_TRUE(
          (true == feature
                       .emplace(result, arangodb::StaticStrings::SystemDatabase + "::test_analyzer",
                                "identity", VPackParser::fromJson("\"abc\"")->slice())
                       .ok()));
      EXPECT_TRUE((false == !result.first));
      collection = vocbase->lookupCollection(arangodb::tests::AnalyzerCollectionName);
      EXPECT_TRUE((nullptr != collection));
    }

    arangodb::iresearch::IResearchAnalyzerFeature feature(server);
    feature.prepare();  // add static analyzers
    feature.start();    // load persisted analyzers
    EXPECT_TRUE((nullptr != vocbase->lookupCollection(arangodb::tests::AnalyzerCollectionName)));

    auto expected = staticAnalyzers();
    auto expectedAnalyzer =
        arangodb::StaticStrings::SystemDatabase + "::test_analyzer";

    expected.emplace(std::piecewise_construct, std::forward_as_tuple(expectedAnalyzer),
                     std::forward_as_tuple("identity", "{}"));
    feature.visit(
        [&expected, &feature](
            arangodb::iresearch::IResearchAnalyzerFeature::AnalyzerPool::ptr const& analyzer) -> bool {
          auto itr = expected.find(analyzer->name());
          EXPECT_TRUE((itr != expected.end()));
          EXPECT_TRUE((itr->second.type == analyzer->type()));

          std::string expectedproperties;
          EXPECT_TRUE(irs::analysis::analyzers::normalize(
              expectedproperties, analyzer->type(), irs::text_format::vpack,
              arangodb::iresearch::ref<char>(itr->second.properties), false));

          EXPECT_EQUAL_SLICES(arangodb::iresearch::slice(expectedproperties),
                              analyzer->properties());
          EXPECT_TRUE((itr->second.features.is_subset_of(
              feature.get(analyzer->name())->features())));
          expected.erase(itr);
          return true;
        });
    EXPECT_TRUE((expected.empty()));
  }
}

TEST_F(IResearchAnalyzerFeatureTest, test_tokens) {
  // create a new instance of an ApplicationServer and fill it with the required features
  // cannot use the existing server since its features already have some state
  std::shared_ptr<arangodb::application_features::ApplicationServer> originalServer(
      arangodb::application_features::ApplicationServer::server,
      [](arangodb::application_features::ApplicationServer* ptr) -> void {
        arangodb::application_features::ApplicationServer::server = ptr;
      });
  arangodb::application_features::ApplicationServer::server =
      nullptr;  // avoid "ApplicationServer initialized twice"
  arangodb::application_features::ApplicationServer server(nullptr, nullptr);
  auto* analyzers = new arangodb::iresearch::IResearchAnalyzerFeature(server);
  auto* functions = new arangodb::aql::AqlFunctionFeature(server);
  auto* dbfeature = new arangodb::DatabaseFeature(server);
  auto cleanup = arangodb::scopeGuard([dbfeature]() { dbfeature->unprepare(); });
  auto* sharding = new arangodb::ShardingFeature(server);
  auto* systemdb = new arangodb::SystemDatabaseFeature(server);

  arangodb::application_features::ApplicationServer::server->addFeature(analyzers);
  arangodb::application_features::ApplicationServer::server->addFeature(dbfeature);
  arangodb::application_features::ApplicationServer::server->addFeature(functions);
  server.addFeature(new arangodb::QueryRegistryFeature(server));  // required for constructing TRI_vocbase_t
  arangodb::application_features::ApplicationServer::server->addFeature(sharding);
  arangodb::application_features::ApplicationServer::server->addFeature(systemdb);
  server.addFeature(new arangodb::V8DealerFeature(server));  // required for DatabaseFeature::createDatabase(...)

  sharding->prepare();

  // create system vocbase (before feature start)
  {
    auto const databases = VPackParser::fromJson(
        std::string("[ { \"name\": \"") +
        arangodb::StaticStrings::SystemDatabase + "\" } ]");
    EXPECT_EQ(TRI_ERROR_NO_ERROR, dbfeature->loadDatabases(databases->slice()));
    systemdb->start();  // get system database from DatabaseFeature
  }

  auto vocbase = systemdb->use();
  // ensure there is no configuration collection
  {
    auto collection = vocbase->lookupCollection(arangodb::tests::AnalyzerCollectionName);

    if (collection) {
      vocbase->dropCollection(collection->id(), true, -1);
    }

    collection = vocbase->lookupCollection(arangodb::tests::AnalyzerCollectionName);
    EXPECT_EQ(nullptr, collection);
  }

  arangodb::methods::Collections::createSystem(*vocbase, arangodb::tests::AnalyzerCollectionName);

  // test function registration

  // AqlFunctionFeature::byName(..) throws exception instead of returning a nullptr
  EXPECT_ANY_THROW((functions->byName("TOKENS")));
  analyzers->prepare();
  analyzers->start();  // load AQL functions
  // if failed to register - other tests makes no sense
  auto* function = functions->byName("TOKENS");
  ASSERT_NE(nullptr, function);
  auto& impl = function->implementation;
  ASSERT_NE(nullptr, impl);

  arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;
  analyzers->start();  // load AQL functions
  ASSERT_TRUE(
      (true == analyzers
                   ->emplace(result, arangodb::StaticStrings::SystemDatabase + "::test_analyzer",
                             "TestAnalyzer", VPackParser::fromJson("\"abc\"")->slice())
                   .ok()));
  ASSERT_TRUE((false == !result.first));

  // test tokenization
  {
    std::string analyzer(arangodb::StaticStrings::SystemDatabase +
                         "::test_analyzer");
    irs::string_ref data("abcdefghijklmnopqrstuvwxyz");
    VPackFunctionParametersWrapper args;
    args->emplace_back(data.c_str(), data.size());
    args->emplace_back(analyzer.c_str(), analyzer.size());
    AqlValueWrapper result(impl(nullptr, nullptr, *args));
    EXPECT_TRUE(result->isArray());
    EXPECT_EQ(26, result->length());

    for (int64_t i = 0; i < 26; ++i) {
      bool mustDestroy;
      auto entry = result->at(i, mustDestroy, false);
      EXPECT_TRUE(entry.isString());
      auto value = arangodb::iresearch::getStringRef(entry.slice());
      EXPECT_EQ(1, value.size());
      EXPECT_EQ('a' + i, value.c_str()[0]);
    }
  }
  // test default analyzer
  {
    irs::string_ref data("abcdefghijklmnopqrstuvwxyz");
    VPackFunctionParametersWrapper args;
    args->emplace_back(data.c_str(), data.size());
    AqlValueWrapper result(impl(nullptr, nullptr, *args));
    EXPECT_TRUE(result->isArray());
    EXPECT_EQ(1, result->length());
    bool mustDestroy;
    auto entry = result->at(0, mustDestroy, false);
    EXPECT_TRUE(entry.isString());
    std::string value = arangodb::iresearch::getStringRef(entry.slice());
    EXPECT_EQ(data, value);
  }

  // test invalid arg count
  // Zero count (less than expected)
  {
    arangodb::SmallVector<arangodb::aql::AqlValue>::allocator_type::arena_type arena;
    arangodb::aql::VPackFunctionParameters args{arena};
    EXPECT_THROW(AqlValueWrapper(impl(nullptr, nullptr, args)), arangodb::basics::Exception);
  }
  // test invalid arg count
  // 3 parameters. More than expected
  {
    irs::string_ref data("abcdefghijklmnopqrstuvwxyz");
    irs::string_ref analyzer("identity");
    irs::string_ref unexpectedParameter("something");
    VPackFunctionParametersWrapper args;
    args->emplace_back(data.c_str(), data.size());
    args->emplace_back(analyzer.c_str(), analyzer.size());
    args->emplace_back(unexpectedParameter.c_str(), unexpectedParameter.size());
    EXPECT_THROW(AqlValueWrapper(impl(nullptr, nullptr, *args)), arangodb::basics::Exception);
  }

  // test values
  // 123.4
  std::string expected123P4[] = { "oMBe2ZmZmZma",
                                  "sMBe2ZmZmQ==",
                                  "wMBe2Zk=",
                                  "0MBe"};

  // 123
  std::string expected123[] = { "oMBewAAAAAAA",
                                "sMBewAAAAA==",
                                "wMBewAA=",
                                "0MBe"};

  // boolean true
  std::string expectedTrue("/w==");
  // boolean false
  std::string expectedFalse("AA==");

  // test double data type
  {
    VPackFunctionParametersWrapper args;
    args->emplace_back(arangodb::aql::AqlValueHintDouble(123.4));
    auto result = AqlValueWrapper(impl(nullptr, nullptr, *args));
    EXPECT_TRUE(result->isArray());
    EXPECT_EQ(IRESEARCH_COUNTOF(expected123P4), result->length());

    for (size_t i = 0; i < result->length(); ++i) {
      bool mustDestroy;
      auto entry = result->at(i, mustDestroy, false).slice();
      EXPECT_TRUE(entry.isString());
      EXPECT_EQ(expected123P4[i], arangodb::iresearch::getStringRef(entry));
   
    }
  }
  // test integer data type
  {
    auto expected = 123;
    VPackFunctionParametersWrapper args;
    args->emplace_back(arangodb::aql::AqlValueHintInt(expected));
    auto result = AqlValueWrapper(impl(nullptr, nullptr, *args));
    EXPECT_TRUE(result->isArray());
    EXPECT_EQ(IRESEARCH_COUNTOF(expected123), result->length());

    for (size_t i = 0; i < result->length(); ++i) {
      bool mustDestroy;
      auto entry = result->at(i, mustDestroy, false).slice();
      EXPECT_TRUE(entry.isString());
      EXPECT_EQ(expected123[i], arangodb::iresearch::getStringRef(entry));
    }
  }
  // test true bool
  {
    VPackFunctionParametersWrapper args;
    args->emplace_back(arangodb::aql::AqlValueHintBool(true));
    auto result = AqlValueWrapper(impl(nullptr, nullptr, *args));
    EXPECT_TRUE(result->isArray());
    EXPECT_EQ(1, result->length());
    bool mustDestroy;
    auto entry = result->at(0, mustDestroy, false).slice();
    EXPECT_TRUE(entry.isString());
    EXPECT_EQ(expectedTrue, arangodb::iresearch::getStringRef(entry));
  }
  // test false bool
  {
    VPackFunctionParametersWrapper args;
    args->emplace_back(arangodb::aql::AqlValueHintBool(false));
    auto result = AqlValueWrapper(impl(nullptr, nullptr, *args));
    EXPECT_TRUE(result->isArray());
    EXPECT_EQ(1, result->length());
    bool mustDestroy;
    auto entry = result->at(0, mustDestroy, false).slice();
    EXPECT_TRUE(entry.isString());
    EXPECT_EQ(expectedFalse, arangodb::iresearch::getStringRef(entry));
  }
  // test null data type
  {
    VPackFunctionParametersWrapper args;
    args->emplace_back(arangodb::aql::AqlValueHintNull());
    auto result = AqlValueWrapper(impl(nullptr, nullptr, *args));
    EXPECT_TRUE(result->isArray());
    EXPECT_EQ(1, result->length());
    bool mustDestroy;
    auto entry = result->at(0, mustDestroy, false).slice();
    EXPECT_TRUE(entry.isString());
    EXPECT_EQ("", arangodb::iresearch::getStringRef(entry));
  }

  // test double type with not needed analyzer
  {
    std::string analyzer(arangodb::StaticStrings::SystemDatabase +
                         "::test_analyzer");
    VPackFunctionParametersWrapper args;
    args->emplace_back(arangodb::aql::AqlValueHintDouble(123.4));
    args->emplace_back(analyzer.c_str(), analyzer.size());
    auto result = AqlValueWrapper(impl(nullptr, nullptr, *args));
    EXPECT_TRUE(result->isArray());
    EXPECT_EQ(IRESEARCH_COUNTOF(expected123P4), result->length());

    for (size_t i = 0; i < result->length(); ++i) {
      bool mustDestroy;
      auto entry = result->at(i, mustDestroy, false).slice();
      EXPECT_TRUE(entry.isString());
      EXPECT_EQ(expected123P4[i], arangodb::iresearch::getStringRef(entry));
    }
  }
  // test double type with not needed analyzer (invalid analyzer type)
  {
    irs::string_ref analyzer("invalid_analyzer");
    VPackFunctionParametersWrapper args;
    args->emplace_back(arangodb::aql::AqlValueHintDouble(123.4));
    args->emplace_back(analyzer.c_str(), analyzer.size());
    EXPECT_THROW(AqlValueWrapper(impl(nullptr, nullptr, *args)), arangodb::basics::Exception);
  }
  // test invalid analyzer (when analyzer needed for text)
  {
    irs::string_ref analyzer("invalid");
    irs::string_ref data("abcdefghijklmnopqrstuvwxyz");
    VPackFunctionParametersWrapper args;
    args->emplace_back(data.c_str(), data.size());
    args->emplace_back(analyzer.c_str(), analyzer.size());
    EXPECT_THROW(AqlValueWrapper(impl(nullptr, nullptr, *args)), arangodb::basics::Exception);
  }

  // empty array
  {
    VPackFunctionParametersWrapper args;
    args->emplace_back(arangodb::aql::AqlValueHintEmptyArray());
    auto result = AqlValueWrapper(impl(nullptr, nullptr, *args));
    EXPECT_TRUE(result->isArray());
    EXPECT_EQ(1, result->length());
    bool mustDestroy;
    auto entry = result->at(0, mustDestroy, false).slice();
    EXPECT_TRUE(entry.isEmptyArray());
  }
  // empty nested array
  {
    VPackFunctionParametersWrapper args;
    auto buffer = irs::memory::make_unique<arangodb::velocypack::Buffer<uint8_t>>();
    VPackBuilder builder(*buffer);
    builder.openArray();
    builder.openArray();
    builder.close();
    builder.close();
    auto bufOwner = true;
    auto aqlValue = arangodb::aql::AqlValue(buffer.get(), bufOwner);
    if (!bufOwner) {
      buffer.release();
    }
    args->push_back(std::move(aqlValue));
    auto result = AqlValueWrapper(impl(nullptr, nullptr, *args));
    EXPECT_TRUE(result->isArray());
    EXPECT_EQ(1, result->length());
    bool mustDestroy;
    auto entry = result->at(0, mustDestroy, false).slice();
    EXPECT_TRUE(entry.isArray());
    EXPECT_EQ(1, entry.length());
    auto entryNested = entry.at(0);
    EXPECT_TRUE(entryNested.isEmptyArray());
  }

  // non-empty nested array
  {
    VPackFunctionParametersWrapper args;
    auto buffer = irs::memory::make_unique<arangodb::velocypack::Buffer<uint8_t>>();
    VPackBuilder builder(*buffer);
    builder.openArray();
    builder.openArray();
    builder.openArray();
    builder.add(arangodb::velocypack::Value(true));
    builder.close();
    builder.close();
    builder.close();
    auto bufOwner = true;
    auto aqlValue = arangodb::aql::AqlValue(buffer.get(), bufOwner);
    if (!bufOwner) {
      buffer.release();
    }
    args->push_back(std::move(aqlValue));
    auto result = AqlValueWrapper(impl(nullptr, nullptr, *args));
    EXPECT_TRUE(result->isArray());
    EXPECT_EQ(1, result->length());
    bool mustDestroy;
    auto entry = result->at(0, mustDestroy, false).slice();
    EXPECT_TRUE(entry.isArray());
    EXPECT_EQ(1, entry.length());
    auto nested = entry.at(0);
    EXPECT_TRUE(nested.isArray());
    EXPECT_EQ(1, nested.length());
    auto nested2 = nested.at(0);
    EXPECT_TRUE(nested2.isArray());
    EXPECT_EQ(1, nested2.length());
    auto booleanValue = nested2.at(0);
    EXPECT_TRUE(booleanValue.isString());
    EXPECT_EQ(expectedTrue, arangodb::iresearch::getStringRef(booleanValue));
  }

  // array of bools
  {
    auto buffer = irs::memory::make_unique<arangodb::velocypack::Buffer<uint8_t>>();
    VPackBuilder builder(*buffer);
    builder.openArray();
    builder.add(arangodb::velocypack::Value(true));
    builder.add(arangodb::velocypack::Value(false));
    builder.add(arangodb::velocypack::Value(true));
    builder.close();
    auto bufOwner = true;
    auto aqlValue = arangodb::aql::AqlValue(buffer.get(), bufOwner);
    if (!bufOwner) {
      buffer.release();
    }
    VPackFunctionParametersWrapper args;
    args->push_back(std::move(aqlValue));
    irs::string_ref analyzer("text_en");
    args->emplace_back(analyzer.c_str(), analyzer.size());
    auto result = AqlValueWrapper(impl(nullptr, nullptr, *args));
    EXPECT_TRUE(result->isArray());
    EXPECT_EQ(3, result->length());
    {
      bool mustDestroy;
      auto entry = result->at(0, mustDestroy, false).slice();
      EXPECT_TRUE(entry.isArray());
      EXPECT_EQ(1, entry.length());
      auto booleanValue = entry.at(0);
      EXPECT_TRUE(booleanValue.isString());
      EXPECT_EQ(expectedTrue, arangodb::iresearch::getStringRef(booleanValue));
    }
    {
      bool mustDestroy;
      auto entry = result->at(1, mustDestroy, false).slice();
      EXPECT_TRUE(entry.isArray());
      EXPECT_EQ(1, entry.length());
      auto booleanValue = entry.at(0);
      EXPECT_TRUE(booleanValue.isString());
      EXPECT_EQ(expectedFalse, arangodb::iresearch::getStringRef(booleanValue));
    }
    {
      bool mustDestroy;
      auto entry = result->at(2, mustDestroy, false).slice();
      EXPECT_TRUE(entry.isArray());
      EXPECT_EQ(1, entry.length());
      auto booleanValue = entry.at(0);
      EXPECT_TRUE(booleanValue.isString());
      EXPECT_EQ(expectedTrue, arangodb::iresearch::getStringRef(booleanValue));
    }
  }

  // mixed values array
  // [ [[]], [['test', 123.4, true]], 123, 123.4, true, null, false, 'jumps', ['quick', 'dog'] ]
  {
    VPackFunctionParametersWrapper args;
    auto buffer = irs::memory::make_unique<arangodb::velocypack::Buffer<uint8_t>>();
    VPackBuilder builder(*buffer);
    builder.openArray();
    // [[]]
    builder.openArray();
    builder.openArray();
    builder.close();
    builder.close();

    //[['test', 123.4, true]]
    builder.openArray();
    builder.openArray();
    builder.add(arangodb::velocypack::Value("test"));
    builder.add(arangodb::velocypack::Value(123.4));
    builder.add(arangodb::velocypack::Value(true));
    builder.close();
    builder.close();

    builder.add(arangodb::velocypack::Value(123));
    builder.add(arangodb::velocypack::Value(123.4));
    builder.add(arangodb::velocypack::Value(true));
    builder.add(VPackSlice::nullSlice());
    builder.add(arangodb::velocypack::Value(false));
    builder.add(arangodb::velocypack::Value("jumps"));

    //[ 'quick', 'dog' ]
    builder.openArray();
    builder.add(arangodb::velocypack::Value("quick"));
    builder.add(arangodb::velocypack::Value("dog"));
    builder.close();

    builder.close();

    auto bufOwner = true;
    auto aqlValue = arangodb::aql::AqlValue(buffer.get(), bufOwner);
    if (!bufOwner) {
      buffer.release();
    }
    args->push_back(std::move(aqlValue));
    irs::string_ref analyzer("text_en");
    args->emplace_back(analyzer.c_str(), analyzer.size());
    auto result = AqlValueWrapper(impl(nullptr, nullptr, *args));
    EXPECT_TRUE(result->isArray());
    EXPECT_EQ(9, result->length());
    {
      bool mustDestroy;
      auto entry = result->at(0, mustDestroy, false).slice();
      EXPECT_TRUE(entry.isArray());
      EXPECT_EQ(1, entry.length());
      auto nested = entry.at(0);
      EXPECT_TRUE(nested.isArray());
      EXPECT_EQ(1, nested.length());
      auto nested2 = nested.at(0);
      EXPECT_TRUE(nested2.isEmptyArray());
    }
    {
      bool mustDestroy;
      auto entry = result->at(1, mustDestroy, false).slice();
      EXPECT_TRUE(entry.isArray());
      EXPECT_EQ(1, entry.length());
      auto nested = entry.at(0);
      EXPECT_TRUE(nested.isArray());
      EXPECT_EQ(3, nested.length());

      {
        auto textTokens = nested.at(0);
        EXPECT_TRUE(textTokens.isArray());
        EXPECT_EQ(1, textTokens.length());
        std::string value = arangodb::iresearch::getStringRef(textTokens.at(0));
        EXPECT_STREQ("test", value.c_str());
      }
      {
        auto numberTokens = nested.at(1);
        EXPECT_TRUE(numberTokens.isArray());
        EXPECT_EQ(IRESEARCH_COUNTOF(expected123P4), numberTokens.length());
        for (size_t i = 0; i < numberTokens.length(); ++i) {
          auto entry = numberTokens.at(i);
          EXPECT_TRUE(entry.isString());
          EXPECT_EQ(expected123P4[i],
                    arangodb::iresearch::getStringRef(entry));
        }
      }
      {
        auto booleanTokens = nested.at(2);
        EXPECT_TRUE(booleanTokens.isArray());
        EXPECT_EQ(1, booleanTokens.length());
        auto booleanValue = booleanTokens.at(0);
        EXPECT_TRUE(booleanValue.isString());
        EXPECT_EQ(expectedTrue, arangodb::iresearch::getStringRef(booleanValue));
      }
    }
    {
      bool mustDestroy;
      auto entry = result->at(2, mustDestroy, false).slice();
      EXPECT_TRUE(entry.isArray());
      EXPECT_EQ(IRESEARCH_COUNTOF(expected123), entry.length());
      for (size_t i = 0; i < entry.length(); ++i) {
        auto numberSlice = entry.at(i);
        EXPECT_TRUE(numberSlice.isString());
        EXPECT_EQ(expected123[i], arangodb::iresearch::getStringRef(numberSlice));
      }
    }
    {
      bool mustDestroy;
      auto entry = result->at(3, mustDestroy, false).slice();
      EXPECT_TRUE(entry.isArray());
      EXPECT_EQ(IRESEARCH_COUNTOF(expected123P4), entry.length());
      for (size_t i = 0; i < entry.length(); ++i) {
        auto numberSlice = entry.at(i);
        EXPECT_TRUE(numberSlice.isString());
        EXPECT_EQ(expected123P4[i], arangodb::iresearch::getStringRef(numberSlice));
      }
    }
    {
      bool mustDestroy;
      auto entry = result->at(4, mustDestroy, false).slice();
      EXPECT_TRUE(entry.isArray());
      EXPECT_EQ(1, entry.length());
      auto booleanValue = entry.at(0);
      EXPECT_TRUE(booleanValue.isString());
      EXPECT_EQ(expectedTrue, arangodb::iresearch::getStringRef(booleanValue));
    }
    {
      bool mustDestroy;
      auto entry = result->at(5, mustDestroy, false).slice();
      EXPECT_TRUE(entry.isArray());
      EXPECT_EQ(1, entry.length());
      auto nullSlice = entry.at(0);
      EXPECT_TRUE(nullSlice.isString());
      EXPECT_EQ("", arangodb::iresearch::getStringRef(nullSlice));
    }
    {
      bool mustDestroy;
      auto entry = result->at(6, mustDestroy, false).slice();
      EXPECT_TRUE(entry.isArray());
      EXPECT_EQ(1, entry.length());
      auto booleanValue = entry.at(0);
      EXPECT_TRUE(booleanValue.isString());
      EXPECT_EQ(expectedFalse, arangodb::iresearch::getStringRef(booleanValue));
    }
    {
      bool mustDestroy;
      auto entry = result->at(7, mustDestroy, false).slice();
      EXPECT_TRUE(entry.isArray());
      EXPECT_EQ(1, entry.length());
      auto textSlice = entry.at(0);
      EXPECT_TRUE(textSlice.isString());
      std::string value = arangodb::iresearch::getStringRef(textSlice);
      EXPECT_STREQ("jump", value.c_str());
    }
    {
      bool mustDestroy;
      auto entry = result->at(8, mustDestroy, false).slice();
      EXPECT_TRUE(entry.isArray());
      EXPECT_EQ(2, entry.length());
      {
        auto subArray = entry.at(0);
        EXPECT_TRUE(subArray.isArray());
        EXPECT_EQ(1, subArray.length());
        auto textSlice = subArray.at(0);
        EXPECT_TRUE(textSlice.isString());
        std::string value = arangodb::iresearch::getStringRef(textSlice);
        EXPECT_STREQ("quick", value.c_str());
      }
      {
        auto subArray = entry.at(1);
        EXPECT_TRUE(subArray.isArray());
        EXPECT_EQ(1, subArray.length());
        auto textSlice = subArray.at(0);
        EXPECT_TRUE(textSlice.isString());
        std::string value = arangodb::iresearch::getStringRef(textSlice);
        EXPECT_STREQ("dog", value.c_str());
      }
    }
  }
}

TEST_F(IResearchAnalyzerFeatureTest, test_upgrade_static_legacy) {
  static std::string const LEGACY_ANALYZER_COLLECTION_NAME(
      "_iresearch_analyzers");
  static std::string const ANALYZER_COLLECTION_QUERY =
      std::string("FOR d IN ") + arangodb::tests::AnalyzerCollectionName +
      " RETURN d";
  static std::unordered_set<std::string> const EXPECTED_LEGACY_ANALYZERS = {
      "text_de", "text_en", "text_es", "text_fi", "text_fr", "text_it",
      "text_nl", "text_no", "text_pt", "text_ru", "text_sv", "text_zh",
  };
  auto createCollectionJson = VPackParser::fromJson(
      std::string("{ \"id\": 42, \"name\": \"") + arangodb::tests::AnalyzerCollectionName +
      "\", \"isSystem\": true, \"shards\": { \"same-as-dummy-shard-id\": [ "
      "\"shard-server-does-not-matter\" ] }, \"type\": 2 }");  // 'id' and 'shards' required for coordinator tests
  auto createLegacyCollectionJson = VPackParser::fromJson(
      std::string("{ \"id\": 43, \"name\": \"") + LEGACY_ANALYZER_COLLECTION_NAME +
      "\", \"isSystem\": true, \"shards\": { \"shard-id-does-not-matter\": [ "
      "\"shard-server-does-not-matter\" ] }, \"type\": 2 }");  // 'id' and 'shards' required for coordinator tests
  auto collectionId = std::to_string(42);
  auto legacyCollectionId = std::to_string(43);
  auto versionJson = VPackParser::fromJson("{ \"version\": 0, \"tasks\": {} }");
  arangodb::AqlFeature aqlFeature(server);
  aqlFeature.start();  // required for Query::Query(...), must not call ~AqlFeature() for the duration of the test

  // test no system, no analyzer collection (single-server)
  {
    TRI_vocbase_t system(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 0,
                         TRI_VOC_SYSTEM_DATABASE);  // create befor reseting srver

    // create a new instance of an ApplicationServer and fill it with the required features
    // cannot use the existing server since its features already have some state
    std::shared_ptr<arangodb::application_features::ApplicationServer> originalServer(
        arangodb::application_features::ApplicationServer::server,
        [](arangodb::application_features::ApplicationServer* ptr) -> void {
          arangodb::application_features::ApplicationServer::server = ptr;
        });
    arangodb::application_features::ApplicationServer::server =
        nullptr;  // avoid "ApplicationServer initialized twice"
    arangodb::application_features::ApplicationServer server(nullptr, nullptr);
    arangodb::iresearch::IResearchAnalyzerFeature* feature;
    arangodb::DatabaseFeature* dbFeature;
    arangodb::SystemDatabaseFeature* sysDatabase;
    server.addFeature(dbFeature = new arangodb::DatabaseFeature(server));  // required for IResearchAnalyzerFeature::emplace(...)
    server.addFeature(new arangodb::QueryRegistryFeature(server));  // required for constructing TRI_vocbase_t
    server.addFeature(sysDatabase = new arangodb::SystemDatabaseFeature(server, &system));  // required for IResearchAnalyzerFeature::start()
    server.addFeature(new arangodb::UpgradeFeature(server, nullptr, {}));  // required for upgrade tasks
    server.addFeature(new arangodb::V8DealerFeature(server));  // required for DatabaseFeature::createDatabase(...)
    server.addFeature(feature = new arangodb::iresearch::IResearchAnalyzerFeature(server));  // required for running upgrade task
    arangodb::aql::OptimizerRulesFeature(this->server).prepare();  // required for Query::preparePlan(...)
    auto clearOptimizerRules = irs::make_finally([this]() -> void {
      arangodb::aql::OptimizerRulesFeature(this->server).unprepare();
    });

    auto cleanup = arangodb::scopeGuard([dbFeature]() { dbFeature->unprepare(); });
    feature->start();  // register upgrade tasks

    arangodb::DatabasePathFeature dbPathFeature(server);
    arangodb::tests::setDatabasePath(dbPathFeature);  // ensure test data is stored in a unique directory
    auto versionFilename = StorageEngineMock::versionFilenameResult;
    auto versionFilenameRestore = irs::make_finally([&versionFilename]() -> void {
      StorageEngineMock::versionFilenameResult = versionFilename;
    });
    StorageEngineMock::versionFilenameResult =
        (irs::utf8_path(dbPathFeature.directory()) /= "version").utf8();
    ASSERT_TRUE((irs::utf8_path(dbPathFeature.directory()).mkdir()));
    ASSERT_TRUE((arangodb::basics::VelocyPackHelper::velocyPackToFile(
        StorageEngineMock::versionFilenameResult, versionJson->slice(), false)));

    TRI_vocbase_t* vocbase;
    EXPECT_TRUE((TRI_ERROR_NO_ERROR ==
                 dbFeature->createDatabase(1, "testVocbase", vocbase)));
    sysDatabase->unprepare();  // unset system vocbase
    EXPECT_TRUE((arangodb::methods::Upgrade::startup(*vocbase, true, false).ok()));  // run upgrade
    EXPECT_TRUE((false == !vocbase->lookupCollection(arangodb::tests::AnalyzerCollectionName)));
    auto result = arangodb::tests::executeQuery(*vocbase, ANALYZER_COLLECTION_QUERY);
    EXPECT_TRUE((result.result.ok()));
    auto slice = result.data->slice();
    EXPECT_TRUE(slice.isArray());
    EXPECT_EQ(0, slice.length());
  }

  // test no system, with analyzer collection (single-server)
  {
    TRI_vocbase_t system(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 0,
                         TRI_VOC_SYSTEM_DATABASE);  // create befor reseting srver

    // create a new instance of an ApplicationServer and fill it with the required features
    // cannot use the existing server since its features already have some state
    std::shared_ptr<arangodb::application_features::ApplicationServer> originalServer(
        arangodb::application_features::ApplicationServer::server,
        [](arangodb::application_features::ApplicationServer* ptr) -> void {
          arangodb::application_features::ApplicationServer::server = ptr;
        });
    arangodb::application_features::ApplicationServer::server =
        nullptr;  // avoid "ApplicationServer initialized twice"
    arangodb::application_features::ApplicationServer server(nullptr, nullptr);
    arangodb::iresearch::IResearchAnalyzerFeature* feature;
    arangodb::DatabaseFeature* dbFeature;
    arangodb::SystemDatabaseFeature* sysDatabase;
    server.addFeature(dbFeature = new arangodb::DatabaseFeature(server));  // required for IResearchAnalyzerFeature::emplace(...)
    server.addFeature(new arangodb::QueryRegistryFeature(server));  // required for constructing TRI_vocbase_t
    server.addFeature(sysDatabase = new arangodb::SystemDatabaseFeature(server, &system));  // required for IResearchAnalyzerFeature::start()
    server.addFeature(new arangodb::UpgradeFeature(server, nullptr, {}));  // required for upgrade tasks
    server.addFeature(new arangodb::V8DealerFeature(server));  // required for DatabaseFeature::createDatabase(...)
    server.addFeature(feature = new arangodb::iresearch::IResearchAnalyzerFeature(server));  // required for running upgrade task
    arangodb::aql::OptimizerRulesFeature(this->server).prepare();  // required for Query::preparePlan(...)
    auto clearOptimizerRules = irs::make_finally([this]() -> void {
      arangodb::aql::OptimizerRulesFeature(this->server).unprepare();
    });

    auto cleanup = arangodb::scopeGuard([dbFeature]() { dbFeature->unprepare(); });
    feature->start();  // register upgrade tasks

    arangodb::DatabasePathFeature dbPathFeature(server);
    arangodb::tests::setDatabasePath(dbPathFeature);  // ensure test data is stored in a unique directory
    auto versionFilename = StorageEngineMock::versionFilenameResult;
    auto versionFilenameRestore = irs::make_finally([&versionFilename]() -> void {
      StorageEngineMock::versionFilenameResult = versionFilename;
    });
    StorageEngineMock::versionFilenameResult =
        (irs::utf8_path(dbPathFeature.directory()) /= "version").utf8();
    ASSERT_TRUE((irs::utf8_path(dbPathFeature.directory()).mkdir()));
    ASSERT_TRUE((arangodb::basics::VelocyPackHelper::velocyPackToFile(
        StorageEngineMock::versionFilenameResult, versionJson->slice(), false)));

    std::unordered_set<std::string> expected{"abc"};
    TRI_vocbase_t* vocbase;
    EXPECT_TRUE((TRI_ERROR_NO_ERROR ==
                 dbFeature->createDatabase(1, "testVocbase", vocbase)));
    EXPECT_TRUE((false == !vocbase->createCollection(createCollectionJson->slice())));

    // add document to collection
    {
      arangodb::OperationOptions options;
      arangodb::SingleCollectionTransaction trx(
          arangodb::transaction::StandaloneContext::Create(*vocbase),
          arangodb::tests::AnalyzerCollectionName, arangodb::AccessMode::Type::WRITE);
      EXPECT_TRUE((true == trx.begin().ok()));
      EXPECT_TRUE(
          (true == trx.insert(arangodb::tests::AnalyzerCollectionName,
                              VPackParser::fromJson("{\"name\": \"abc\"}")->slice(), options)
                       .ok()));
      EXPECT_TRUE((trx.commit().ok()));
    }

    sysDatabase->unprepare();  // unset system vocbase
    EXPECT_TRUE((arangodb::methods::Upgrade::startup(*vocbase, true, false).ok()));  // run upgrade
    EXPECT_TRUE((false == !vocbase->lookupCollection(arangodb::tests::AnalyzerCollectionName)));
    auto result = arangodb::tests::executeQuery(*vocbase, ANALYZER_COLLECTION_QUERY);
    EXPECT_TRUE((result.result.ok()));
    auto slice = result.data->slice();
    EXPECT_TRUE(slice.isArray());

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto resolved = itr.value().resolveExternals();
      EXPECT_TRUE((resolved.isObject()));
      EXPECT_TRUE((resolved.get("name").isString()));
      EXPECT_TRUE((1 == expected.erase(resolved.get("name").copyString())));
    }

    EXPECT_TRUE((true == expected.empty()));
  }

  // test system, no legacy collection, no analyzer collection (single-server)
  {
    TRI_vocbase_t system(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 0,
                         TRI_VOC_SYSTEM_DATABASE);  // create befor reseting srver

    // create a new instance of an ApplicationServer and fill it with the required features
    // cannot use the existing server since its features already have some state
    std::shared_ptr<arangodb::application_features::ApplicationServer> originalServer(
        arangodb::application_features::ApplicationServer::server,
        [](arangodb::application_features::ApplicationServer* ptr) -> void {
          arangodb::application_features::ApplicationServer::server = ptr;
        });
    arangodb::application_features::ApplicationServer::server =
        nullptr;  // avoid "ApplicationServer initialized twice"
    arangodb::application_features::ApplicationServer server(nullptr, nullptr);
    arangodb::iresearch::IResearchAnalyzerFeature* feature;
    arangodb::DatabaseFeature* dbFeature;
    arangodb::SystemDatabaseFeature* sysDatabase;
    server.addFeature(dbFeature = new arangodb::DatabaseFeature(server));  // required for IResearchAnalyzerFeature::emplace(...)
    server.addFeature(new arangodb::QueryRegistryFeature(server));  // required for constructing TRI_vocbase_t
    server.addFeature(sysDatabase = new arangodb::SystemDatabaseFeature(server, &system));  // required for IResearchAnalyzerFeature::start()
    server.addFeature(new arangodb::UpgradeFeature(server, nullptr, {}));  // required for upgrade tasks
    server.addFeature(new arangodb::V8DealerFeature(server));  // required for DatabaseFeature::createDatabase(...)
    server.addFeature(feature = new arangodb::iresearch::IResearchAnalyzerFeature(server));  // required for running upgrade task
    arangodb::aql::OptimizerRulesFeature(this->server).prepare();  // required for Query::preparePlan(...)
    auto clearOptimizerRules = irs::make_finally([this]() -> void {
      arangodb::aql::OptimizerRulesFeature(this->server).unprepare();
    });

    feature->start();  // register upgrade tasks
    auto cleanup = arangodb::scopeGuard([dbFeature]() { dbFeature->unprepare(); });

    // ensure no legacy collection after feature start
    {
      auto collection = system.lookupCollection(LEGACY_ANALYZER_COLLECTION_NAME);
      ASSERT_TRUE((true == !collection));
    }

    arangodb::DatabasePathFeature dbPathFeature(server);
    arangodb::tests::setDatabasePath(dbPathFeature);  // ensure test data is stored in a unique directory
    auto versionFilename = StorageEngineMock::versionFilenameResult;
    auto versionFilenameRestore = irs::make_finally([&versionFilename]() -> void {
      StorageEngineMock::versionFilenameResult = versionFilename;
    });
    StorageEngineMock::versionFilenameResult =
        (irs::utf8_path(dbPathFeature.directory()) /= "version").utf8();
    ASSERT_TRUE((irs::utf8_path(dbPathFeature.directory()).mkdir()));
    ASSERT_TRUE((arangodb::basics::VelocyPackHelper::velocyPackToFile(
        StorageEngineMock::versionFilenameResult, versionJson->slice(), false)));

    TRI_vocbase_t* vocbase;
    EXPECT_TRUE((TRI_ERROR_NO_ERROR ==
                 dbFeature->createDatabase(1, "testVocbase", vocbase)));
    EXPECT_TRUE((arangodb::methods::Upgrade::startup(*vocbase, true, false).ok()));  // run upgrade
    EXPECT_TRUE((false == !vocbase->lookupCollection(arangodb::tests::AnalyzerCollectionName)));
    auto result = arangodb::tests::executeQuery(*vocbase, ANALYZER_COLLECTION_QUERY);
    EXPECT_TRUE((result.result.ok()));
    auto slice = result.data->slice();
    EXPECT_TRUE(slice.isArray());
    EXPECT_EQ(0, slice.length());
  }

  // test system, no legacy collection, with analyzer collection (single-server)
  {
    TRI_vocbase_t system(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 0,
                         TRI_VOC_SYSTEM_DATABASE);  // create befor reseting srver

    // create a new instance of an ApplicationServer and fill it with the required features
    // cannot use the existing server since its features already have some state
    std::shared_ptr<arangodb::application_features::ApplicationServer> originalServer(
        arangodb::application_features::ApplicationServer::server,
        [](arangodb::application_features::ApplicationServer* ptr) -> void {
          arangodb::application_features::ApplicationServer::server = ptr;
        });
    arangodb::application_features::ApplicationServer::server =
        nullptr;  // avoid "ApplicationServer initialized twice"
    arangodb::application_features::ApplicationServer server(nullptr, nullptr);
    arangodb::iresearch::IResearchAnalyzerFeature* feature;
    arangodb::DatabaseFeature* dbFeature;
    arangodb::SystemDatabaseFeature* sysDatabase;
    server.addFeature(dbFeature = new arangodb::DatabaseFeature(server));  // required for IResearchAnalyzerFeature::emplace(...)
    server.addFeature(new arangodb::QueryRegistryFeature(server));  // required for constructing TRI_vocbase_t
    server.addFeature(sysDatabase = new arangodb::SystemDatabaseFeature(server, &system));  // required for IResearchAnalyzerFeature::start()
    server.addFeature(new arangodb::UpgradeFeature(server, nullptr, {}));  // required for upgrade tasks
    server.addFeature(new arangodb::V8DealerFeature(server));  // required for DatabaseFeature::createDatabase(...)
    server.addFeature(feature = new arangodb::iresearch::IResearchAnalyzerFeature(server));  // required for running upgrade task
    arangodb::aql::OptimizerRulesFeature(this->server).prepare();  // required for Query::preparePlan(...)
    auto clearOptimizerRules = irs::make_finally([this]() -> void {
      arangodb::aql::OptimizerRulesFeature(this->server).unprepare();
    });

    feature->start();  // register upgrade tasks
    auto cleanup = arangodb::scopeGuard([dbFeature]() { dbFeature->unprepare(); });

    // ensure no legacy collection after feature start
    {
      auto collection = system.lookupCollection(LEGACY_ANALYZER_COLLECTION_NAME);
      ASSERT_TRUE((true == !collection));
    }

    arangodb::DatabasePathFeature dbPathFeature(server);
    arangodb::tests::setDatabasePath(dbPathFeature);  // ensure test data is stored in a unique directory
    auto versionFilename = StorageEngineMock::versionFilenameResult;
    auto versionFilenameRestore = irs::make_finally([&versionFilename]() -> void {
      StorageEngineMock::versionFilenameResult = versionFilename;
    });
    StorageEngineMock::versionFilenameResult =
        (irs::utf8_path(dbPathFeature.directory()) /= "version").utf8();
    ASSERT_TRUE((irs::utf8_path(dbPathFeature.directory()).mkdir()));
    ASSERT_TRUE((arangodb::basics::VelocyPackHelper::velocyPackToFile(
        StorageEngineMock::versionFilenameResult, versionJson->slice(), false)));

    std::unordered_set<std::string> expected{"abc"};
    TRI_vocbase_t* vocbase;
    EXPECT_TRUE((TRI_ERROR_NO_ERROR ==
                 dbFeature->createDatabase(1, "testVocbase", vocbase)));
    EXPECT_TRUE((false == !vocbase->createCollection(createCollectionJson->slice())));

    // add document to collection
    {
      arangodb::OperationOptions options;
      arangodb::SingleCollectionTransaction trx(
          arangodb::transaction::StandaloneContext::Create(*vocbase),
          arangodb::tests::AnalyzerCollectionName, arangodb::AccessMode::Type::WRITE);
      EXPECT_TRUE((true == trx.begin().ok()));
      EXPECT_TRUE(
          (true == trx.insert(arangodb::tests::AnalyzerCollectionName,
                              VPackParser::fromJson("{\"name\": \"abc\"}")->slice(), options)
                       .ok()));
      EXPECT_TRUE((trx.commit().ok()));
    }

    EXPECT_TRUE((arangodb::methods::Upgrade::startup(*vocbase, true, false).ok()));  // run upgrade
    EXPECT_TRUE((false == !vocbase->lookupCollection(arangodb::tests::AnalyzerCollectionName)));
    auto result = arangodb::tests::executeQuery(*vocbase, ANALYZER_COLLECTION_QUERY);
    EXPECT_TRUE((result.result.ok()));
    auto slice = result.data->slice();
    EXPECT_TRUE(slice.isArray());

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto resolved = itr.value().resolveExternals();
      EXPECT_TRUE((resolved.isObject()));
      EXPECT_TRUE((resolved.get("name").isString()));
      EXPECT_TRUE((1 == expected.erase(resolved.get("name").copyString())));
    }

    EXPECT_TRUE((true == expected.empty()));
  }

  // test system, with legacy collection, no analyzer collection (single-server)
  {
    TRI_vocbase_t system(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 0,
                         TRI_VOC_SYSTEM_DATABASE);  // create befor reseting srver

    // create a new instance of an ApplicationServer and fill it with the required features
    // cannot use the existing server since its features already have some state
    std::shared_ptr<arangodb::application_features::ApplicationServer> originalServer(
        arangodb::application_features::ApplicationServer::server,
        [](arangodb::application_features::ApplicationServer* ptr) -> void {
          arangodb::application_features::ApplicationServer::server = ptr;
        });
    arangodb::application_features::ApplicationServer::server =
        nullptr;  // avoid "ApplicationServer initialized twice"
    arangodb::application_features::ApplicationServer server(nullptr, nullptr);
    arangodb::iresearch::IResearchAnalyzerFeature* feature;
    arangodb::DatabaseFeature* dbFeature;
    arangodb::SystemDatabaseFeature* sysDatabase;
    server.addFeature(dbFeature = new arangodb::DatabaseFeature(server));  // required for IResearchAnalyzerFeature::emplace(...)
    server.addFeature(new arangodb::QueryRegistryFeature(server));  // required for constructing TRI_vocbase_t
    server.addFeature(sysDatabase = new arangodb::SystemDatabaseFeature(server, &system));  // required for IResearchAnalyzerFeature::start()
    server.addFeature(new arangodb::UpgradeFeature(server, nullptr, {}));  // required for upgrade tasks
    server.addFeature(new arangodb::V8DealerFeature(server));  // required for DatabaseFeature::createDatabase(...)
    server.addFeature(feature = new arangodb::iresearch::IResearchAnalyzerFeature(server));  // required for running upgrade task
    arangodb::aql::OptimizerRulesFeature(this->server).prepare();  // required for Query::preparePlan(...)
    auto clearOptimizerRules = irs::make_finally([this]() -> void {
      arangodb::aql::OptimizerRulesFeature(this->server).unprepare();
    });

    feature->start();  // register upgrade tasks
    auto cleanup = arangodb::scopeGuard([dbFeature]() { dbFeature->unprepare(); });

    // ensure legacy collection after feature start
    {
      auto collection = system.lookupCollection(LEGACY_ANALYZER_COLLECTION_NAME);
      ASSERT_TRUE((true == !collection));
      ASSERT_TRUE((false == !system.createCollection(createLegacyCollectionJson->slice())));
    }

    // add document to legacy collection after feature start
    {
      arangodb::OperationOptions options;
      arangodb::SingleCollectionTransaction trx(
          arangodb::transaction::StandaloneContext::Create(system),
          LEGACY_ANALYZER_COLLECTION_NAME, arangodb::AccessMode::Type::WRITE);
      EXPECT_TRUE((true == trx.begin().ok()));
      EXPECT_TRUE(
          (true == trx.insert(arangodb::tests::AnalyzerCollectionName,
                              VPackParser::fromJson("{\"name\": \"legacy\"}")->slice(), options)
                       .ok()));
      EXPECT_TRUE((trx.commit().ok()));
    }

    arangodb::DatabasePathFeature dbPathFeature(server);
    arangodb::tests::setDatabasePath(dbPathFeature);  // ensure test data is stored in a unique directory
    auto versionFilename = StorageEngineMock::versionFilenameResult;
    auto versionFilenameRestore = irs::make_finally([&versionFilename]() -> void {
      StorageEngineMock::versionFilenameResult = versionFilename;
    });
    StorageEngineMock::versionFilenameResult =
        (irs::utf8_path(dbPathFeature.directory()) /= "version").utf8();
    ASSERT_TRUE((irs::utf8_path(dbPathFeature.directory()).mkdir()));
    ASSERT_TRUE((arangodb::basics::VelocyPackHelper::velocyPackToFile(
        StorageEngineMock::versionFilenameResult, versionJson->slice(), false)));

    TRI_vocbase_t* vocbase;
    EXPECT_TRUE((TRI_ERROR_NO_ERROR ==
                 dbFeature->createDatabase(1, "testVocbase", vocbase)));
    EXPECT_TRUE((arangodb::methods::Upgrade::startup(*vocbase, true, false).ok()));  // run upgrade
    EXPECT_TRUE((false == !vocbase->lookupCollection(arangodb::tests::AnalyzerCollectionName)));
    auto result = arangodb::tests::executeQuery(*vocbase, ANALYZER_COLLECTION_QUERY);
    EXPECT_TRUE((result.result.ok()));
    auto slice = result.data->slice();
    EXPECT_TRUE(slice.isArray());
    EXPECT_EQ(0, slice.length());
  }

  // test system, no legacy collection, with analyzer collection (single-server)
  {
    TRI_vocbase_t system(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 0,
                         TRI_VOC_SYSTEM_DATABASE);  // create befor reseting srver

    // create a new instance of an ApplicationServer and fill it with the required features
    // cannot use the existing server since its features already have some state
    std::shared_ptr<arangodb::application_features::ApplicationServer> originalServer(
        arangodb::application_features::ApplicationServer::server,
        [](arangodb::application_features::ApplicationServer* ptr) -> void {
          arangodb::application_features::ApplicationServer::server = ptr;
        });
    arangodb::application_features::ApplicationServer::server =
        nullptr;  // avoid "ApplicationServer initialized twice"
    arangodb::application_features::ApplicationServer server(nullptr, nullptr);
    arangodb::iresearch::IResearchAnalyzerFeature* feature;
    arangodb::DatabaseFeature* dbFeature;
    arangodb::SystemDatabaseFeature* sysDatabase;
    server.addFeature(dbFeature = new arangodb::DatabaseFeature(server));  // required for IResearchAnalyzerFeature::emplace(...)
    server.addFeature(new arangodb::QueryRegistryFeature(server));  // required for constructing TRI_vocbase_t
    server.addFeature(sysDatabase = new arangodb::SystemDatabaseFeature(server, &system));  // required for IResearchAnalyzerFeature::start()
    server.addFeature(new arangodb::UpgradeFeature(server, nullptr, {}));  // required for upgrade tasks
    server.addFeature(new arangodb::V8DealerFeature(server));  // required for DatabaseFeature::createDatabase(...)
    server.addFeature(feature = new arangodb::iresearch::IResearchAnalyzerFeature(server));  // required for running upgrade task
    arangodb::aql::OptimizerRulesFeature(this->server).prepare();  // required for Query::preparePlan(...)
    auto clearOptimizerRules = irs::make_finally([this]() -> void {
      arangodb::aql::OptimizerRulesFeature(this->server).unprepare();
    });

    feature->start();  // register upgrade tasks
    auto cleanup = arangodb::scopeGuard([dbFeature]() { dbFeature->unprepare(); });

    // ensure no legacy collection after feature start
    {
      auto collection = system.lookupCollection(LEGACY_ANALYZER_COLLECTION_NAME);
      ASSERT_TRUE((true == !collection));
    }

    arangodb::DatabasePathFeature dbPathFeature(server);
    arangodb::tests::setDatabasePath(dbPathFeature);  // ensure test data is stored in a unique directory
    auto versionFilename = StorageEngineMock::versionFilenameResult;
    auto versionFilenameRestore = irs::make_finally([&versionFilename]() -> void {
      StorageEngineMock::versionFilenameResult = versionFilename;
    });
    StorageEngineMock::versionFilenameResult =
        (irs::utf8_path(dbPathFeature.directory()) /= "version").utf8();
    ASSERT_TRUE((irs::utf8_path(dbPathFeature.directory()).mkdir()));
    ASSERT_TRUE((arangodb::basics::VelocyPackHelper::velocyPackToFile(
        StorageEngineMock::versionFilenameResult, versionJson->slice(), false)));

    std::set<std::string> expected{"abc"};
    TRI_vocbase_t* vocbase;
    EXPECT_TRUE((TRI_ERROR_NO_ERROR ==
                 dbFeature->createDatabase(1, "testVocbase", vocbase)));
    EXPECT_TRUE((false == !vocbase->createCollection(createCollectionJson->slice())));

    // add document to collection
    {
      arangodb::OperationOptions options;
      arangodb::SingleCollectionTransaction trx(
          arangodb::transaction::StandaloneContext::Create(*vocbase),
          arangodb::tests::AnalyzerCollectionName, arangodb::AccessMode::Type::WRITE);
      EXPECT_TRUE((true == trx.begin().ok()));
      EXPECT_TRUE(
          (true == trx.insert(arangodb::tests::AnalyzerCollectionName,
                              VPackParser::fromJson("{\"name\": \"abc\"}")->slice(), options)
                       .ok()));
      EXPECT_TRUE((trx.commit().ok()));
    }

    EXPECT_TRUE((arangodb::methods::Upgrade::startup(*vocbase, true, false).ok()));  // run upgrade
    EXPECT_TRUE((false == !vocbase->lookupCollection(arangodb::tests::AnalyzerCollectionName)));
    auto result = arangodb::tests::executeQuery(*vocbase, ANALYZER_COLLECTION_QUERY);
    EXPECT_TRUE((result.result.ok()));
    auto slice = result.data->slice();
    EXPECT_TRUE(slice.isArray());

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto resolved = itr.value().resolveExternals();
      EXPECT_TRUE((resolved.isObject()));
      EXPECT_TRUE((resolved.get("name").isString()));
      EXPECT_TRUE((1 == expected.erase(resolved.get("name").copyString())));
    }

    EXPECT_TRUE((true == expected.empty()));
  }
}

namespace {
// helper function for string->vpack properties represenation conversion
template <class Container>
std::set<typename Container::value_type> makeVPackPropExpectedSet(const Container& stringPropContainer) {
  std::set<typename Container::value_type> expectedSet;
  for (auto& expectedEntry : stringPropContainer) {
    std::string normalizedProperties;
    auto vpack = VPackParser::fromJson(expectedEntry._properties);
    EXPECT_TRUE(irs::analysis::analyzers::normalize(
        normalizedProperties, expectedEntry._type, irs::text_format::vpack,
        arangodb::iresearch::ref<char>(vpack->slice()), false));
    expectedSet.emplace(expectedEntry._name, normalizedProperties,
                        expectedEntry._features, expectedEntry._type);
  }
  return expectedSet;
}
}  // namespace

TEST_F(IResearchAnalyzerFeatureTest, test_visit) {
  struct ExpectedType {
    irs::flags _features;
    std::string _name;
    std::string _properties;
    std::string _type;
    ExpectedType(irs::string_ref const& name, irs::string_ref const& properties,
                 irs::flags const& features, irs::string_ref const& type)
        : _features(features), _name(name), _properties(properties), _type(type) {}

    bool operator<(ExpectedType const& other) const {
      if (_name < other._name) {
        return true;
      }

      if (_name > other._name) {
        return false;
      }

      if (_properties < other._properties) {
        return true;
      }

      if (_properties > other._properties) {
        return false;
      }

      if (_features.size() < other._features.size()) {
        return true;
      }

      if (_features.size() > other._features.size()) {
        return false;
      }

      if (_type < other._type) {
        return true;
      }

      if (_type > other._type) {
        return false;
      }

      return false;  // assume equal
    }
  };

  // create a new instance of an ApplicationServer and fill it with the required features
  // cannot use the existing server since its features already have some state
  std::shared_ptr<arangodb::application_features::ApplicationServer> originalServer(
      arangodb::application_features::ApplicationServer::server,
      [](arangodb::application_features::ApplicationServer* ptr) -> void {
        arangodb::application_features::ApplicationServer::server = ptr;
      });
  arangodb::application_features::ApplicationServer::server =
      nullptr;  // avoid "ApplicationServer initialized twice"
  arangodb::application_features::ApplicationServer server(nullptr, nullptr);
  arangodb::iresearch::IResearchAnalyzerFeature feature(server);
  arangodb::DatabaseFeature* dbFeature;
  arangodb::SystemDatabaseFeature* sysDatabase;
  server.addFeature(dbFeature = new arangodb::DatabaseFeature(server));  // required for IResearchAnalyzerFeature::emplace(...)
  server.addFeature(new arangodb::QueryRegistryFeature(server));  // required for constructing TRI_vocbase_t
  server.addFeature(sysDatabase = new arangodb::SystemDatabaseFeature(server));  // required for IResearchAnalyzerFeature::start()
  server.addFeature(new arangodb::V8DealerFeature(server));  // required for DatabaseFeature::createDatabase(...)

  // create system vocbase (before feature start)
  {
    auto const databases = VPackParser::fromJson(
        std::string("[ { \"name\": \"") +
        arangodb::StaticStrings::SystemDatabase + "\" } ]");
    EXPECT_TRUE((TRI_ERROR_NO_ERROR == dbFeature->loadDatabases(databases->slice())));
    sysDatabase->start();  // get system database from DatabaseFeature
    arangodb::methods::Collections::createSystem(*sysDatabase->use(),
                                                 arangodb::tests::AnalyzerCollectionName);
  }

  auto cleanup = arangodb::scopeGuard([dbFeature]() { dbFeature->unprepare(); });

  arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;
  EXPECT_TRUE(
      (true == feature
                   .emplace(result, arangodb::StaticStrings::SystemDatabase + "::test_analyzer0",
                            "TestAnalyzer", VPackParser::fromJson("\"abc0\"")->slice())
                   .ok()));
  EXPECT_TRUE((false == !result.first));
  EXPECT_TRUE(
      (true == feature
                   .emplace(result, arangodb::StaticStrings::SystemDatabase + "::test_analyzer1",
                            "TestAnalyzer", VPackParser::fromJson("\"abc1\"")->slice())
                   .ok()));
  EXPECT_TRUE((false == !result.first));
  EXPECT_TRUE(
      (true == feature
                   .emplace(result, arangodb::StaticStrings::SystemDatabase + "::test_analyzer2",
                            "TestAnalyzer", VPackParser::fromJson("\"abc2\"")->slice())
                   .ok()));
  EXPECT_TRUE((false == !result.first));

  // full visitation
  {
    std::set<ExpectedType> expected = {
        {arangodb::StaticStrings::SystemDatabase + "::test_analyzer0",
         "\"abc0\"",
         {},
         "TestAnalyzer"},
        {arangodb::StaticStrings::SystemDatabase + "::test_analyzer1",
         "\"abc1\"",
         {},
         "TestAnalyzer"},
        {arangodb::StaticStrings::SystemDatabase + "::test_analyzer2",
         "\"abc2\"",
         {},
         "TestAnalyzer"},
    };
    auto expectedSet = makeVPackPropExpectedSet(expected);
    auto result = feature.visit(
        [&expectedSet](arangodb::iresearch::IResearchAnalyzerFeature::AnalyzerPool::ptr const& analyzer) -> bool {
          if (staticAnalyzers().find(analyzer->name()) != staticAnalyzers().end()) {
            return true;  // skip static analyzers
          }

          EXPECT_EQ(analyzer->type(), "TestAnalyzer");
          EXPECT_EQ(1, expectedSet.erase(
                           ExpectedType(analyzer->name(),
                                        arangodb::iresearch::ref<char>(analyzer->properties()),
                                        analyzer->features(), analyzer->type())));
          return true;
        });
    EXPECT_TRUE((true == result));
    EXPECT_TRUE((expectedSet.empty()));
  }

  // partial visitation
  {
    std::set<ExpectedType> expected = {
        {arangodb::StaticStrings::SystemDatabase + "::test_analyzer0",
         "\"abc0\"",
         {},
         "TestAnalyzer"},
        {arangodb::StaticStrings::SystemDatabase + "::test_analyzer1",
         "\"abc1\"",
         {},
         "TestAnalyzer"},
        {arangodb::StaticStrings::SystemDatabase + "::test_analyzer2",
         "\"abc2\"",
         {},
         "TestAnalyzer"},
    };
    auto expectedSet = makeVPackPropExpectedSet(expected);
    auto result = feature.visit(
        [&expectedSet](arangodb::iresearch::IResearchAnalyzerFeature::AnalyzerPool::ptr const& analyzer) -> bool {
          if (staticAnalyzers().find(analyzer->name()) != staticAnalyzers().end()) {
            return true;  // skip static analyzers
          }

          EXPECT_EQ(analyzer->type(), "TestAnalyzer");
          EXPECT_EQ(1, expectedSet.erase(
                           ExpectedType(analyzer->name(),
                                        arangodb::iresearch::ref<char>(analyzer->properties()),
                                        analyzer->features(), analyzer->type())));
          return false;
        });
    EXPECT_TRUE((false == result));
    EXPECT_TRUE((2 == expectedSet.size()));
  }

  TRI_vocbase_t* vocbase0;
  TRI_vocbase_t* vocbase1;
  TRI_vocbase_t* vocbase2;
  EXPECT_TRUE((TRI_ERROR_NO_ERROR == dbFeature->createDatabase(1, "vocbase0", vocbase0)));
  EXPECT_TRUE((TRI_ERROR_NO_ERROR == dbFeature->createDatabase(1, "vocbase1", vocbase1)));
  EXPECT_TRUE((TRI_ERROR_NO_ERROR == dbFeature->createDatabase(1, "vocbase2", vocbase2)));
  arangodb::methods::Collections::createSystem(*vocbase0, arangodb::tests::AnalyzerCollectionName);
  arangodb::methods::Collections::createSystem(*vocbase1, arangodb::tests::AnalyzerCollectionName);
  arangodb::methods::Collections::createSystem(*vocbase2, arangodb::tests::AnalyzerCollectionName);
  // add database-prefixed analyzers
  {
    arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;
    EXPECT_TRUE(feature
                    .emplace(result, "vocbase2::test_analyzer3", "TestAnalyzer",
                             VPackParser::fromJson("\"abc3\"")->slice())
                    .ok());
    EXPECT_TRUE((false == !result.first));
    EXPECT_TRUE(feature
                    .emplace(result, "vocbase2::test_analyzer4", "TestAnalyzer",
                             VPackParser::fromJson("\"abc4\"")->slice())
                    .ok());
    EXPECT_TRUE((false == !result.first));
    EXPECT_TRUE(feature
                    .emplace(result, "vocbase1::test_analyzer5", "TestAnalyzer",
                             VPackParser::fromJson("\"abc5\"")->slice())
                    .ok());
    EXPECT_TRUE((false == !result.first));
  }

  // full visitation limited to a vocbase (empty)
  {
    std::set<ExpectedType> expected = {};
    auto result = feature.visit(
        [&expected](arangodb::iresearch::IResearchAnalyzerFeature::AnalyzerPool::ptr const& analyzer) -> bool {
          EXPECT_EQ(analyzer->type(), "TestAnalyzer");
          EXPECT_EQ(1, expected.erase(
                           ExpectedType(analyzer->name(),
                                        arangodb::iresearch::ref<char>(analyzer->properties()),
                                        analyzer->features(), analyzer->type())));
          return true;
        },
        vocbase0);
    EXPECT_TRUE((true == result));
    EXPECT_TRUE((expected.empty()));
  }

  // full visitation limited to a vocbase (non-empty)
  {
    std::set<ExpectedType> expected = {
        {"vocbase2::test_analyzer3", "\"abc3\"", {}, "TestAnalyzer"},
        {"vocbase2::test_analyzer4", "\"abc4\"", {}, "TestAnalyzer"},
    };
    auto expectedSet = makeVPackPropExpectedSet(expected);
    auto result = feature.visit(
        [&expectedSet](arangodb::iresearch::IResearchAnalyzerFeature::AnalyzerPool::ptr const& analyzer) -> bool {
          EXPECT_EQ(analyzer->type(), "TestAnalyzer");
          EXPECT_EQ(1, expectedSet.erase(
                           ExpectedType(analyzer->name(),
                                        arangodb::iresearch::ref<char>(analyzer->properties()),
                                        analyzer->features(), analyzer->type())));
          return true;
        },
        vocbase2);
    EXPECT_TRUE((true == result));
    EXPECT_TRUE((expectedSet.empty()));
  }

  // static analyzer visitation
  {
    std::vector<ExpectedType> expected = {
        {"identity",
         "{}",
         {irs::frequency::type(), irs::norm::type()},
         "identity"},
        {"text_de",
         "{ \"locale\": \"de.UTF-8\", \"stopwords\": [ ] "
         "}",
         {irs::frequency::type(), irs::norm::type(), irs::position::type()},
         "text"},
        {"text_en",
         "{ \"locale\": \"en.UTF-8\", \"stopwords\": [ ] "
         "}",
         {irs::frequency::type(), irs::norm::type(), irs::position::type()},
         "text"},
        {"text_es",
         "{ \"locale\": \"es.UTF-8\", \"stopwords\": [ ] "
         "}",
         {irs::frequency::type(), irs::norm::type(), irs::position::type()},
         "text"},
        {"text_fi",
         "{ \"locale\": \"fi.UTF-8\", \"stopwords\": [ ] "
         "}",
         {irs::frequency::type(), irs::norm::type(), irs::position::type()},
         "text"},
        {"text_fr",
         "{ \"locale\": \"fr.UTF-8\", \"stopwords\": [ ] "
         "}",
         {irs::frequency::type(), irs::norm::type(), irs::position::type()},
         "text"},
        {"text_it",
         "{ \"locale\": \"it.UTF-8\", \"stopwords\": [ ] "
         "}",
         {irs::frequency::type(), irs::norm::type(), irs::position::type()},
         "text"},
        {"text_nl",
         "{ \"locale\": \"nl.UTF-8\", \"stopwords\": [ ] "
         "}",
         {irs::frequency::type(), irs::norm::type(), irs::position::type()},
         "text"},
        {"text_no",
         "{ \"locale\": \"no.UTF-8\", \"stopwords\": [ ] "
         "}",
         {irs::frequency::type(), irs::norm::type(), irs::position::type()},
         "text"},
        {"text_pt",
         "{ \"locale\": \"pt.UTF-8\", \"stopwords\": [ ] "
         "}",
         {irs::frequency::type(), irs::norm::type(), irs::position::type()},
         "text"},
        {"text_ru",
         "{ \"locale\": \"ru.UTF-8\", \"stopwords\": [ ] "
         "}",
         {irs::frequency::type(), irs::norm::type(), irs::position::type()},
         "text"},
        {"text_sv",
         "{ \"locale\": \"sv.UTF-8\", \"stopwords\": [ ] "
         "}",
         {irs::frequency::type(), irs::norm::type(), irs::position::type()},
         "text"},
        {"text_zh",
         "{ \"locale\": \"zh.UTF-8\", \"stopwords\": [ ] "
         "}",
         {irs::frequency::type(), irs::norm::type(), irs::position::type()},
         "text"},
    };

    auto expectedSet = makeVPackPropExpectedSet(expected);
    ASSERT_EQ(expected.size(), expectedSet.size());

    auto result = feature.visit(
        [&expectedSet](arangodb::iresearch::IResearchAnalyzerFeature::AnalyzerPool::ptr const& analyzer) -> bool {
          EXPECT_EQ(1, expectedSet.erase(
                           ExpectedType(analyzer->name(),
                                        arangodb::iresearch::ref<char>(analyzer->properties()),
                                        analyzer->features(), analyzer->type())));
          return true;
        },
        nullptr);
    EXPECT_TRUE((true == result));
    EXPECT_TRUE((expectedSet.empty()));
  }
}

TEST_F(IResearchAnalyzerFeatureTest, custom_analyzers_vpack_create) {
  // create a new instance of an ApplicationServer and fill it with the required features
  // cannot use the existing server since its features already have some state
  std::shared_ptr<arangodb::application_features::ApplicationServer> originalServer(
      arangodb::application_features::ApplicationServer::server,
      [](arangodb::application_features::ApplicationServer* ptr) -> void {
        arangodb::application_features::ApplicationServer::server = ptr;
      });
  arangodb::application_features::ApplicationServer::server =
      nullptr;  // avoid "ApplicationServer initialized twice"
  arangodb::application_features::ApplicationServer server(nullptr, nullptr);
  arangodb::iresearch::IResearchAnalyzerFeature feature(server);
  arangodb::DatabaseFeature* dbFeature;
  arangodb::SystemDatabaseFeature* sysDatabase;
  server.addFeature(dbFeature = new arangodb::DatabaseFeature(server));  // required for IResearchAnalyzerFeature::emplace(...)
  server.addFeature(new arangodb::QueryRegistryFeature(server));  // required for constructing TRI_vocbase_t
  server.addFeature(sysDatabase = new arangodb::SystemDatabaseFeature(server));  // required for IResearchAnalyzerFeature::start()
  server.addFeature(new arangodb::V8DealerFeature(server));  // required for DatabaseFeature::createDatabase(...)
  auto cleanup = arangodb::scopeGuard([dbFeature]() { dbFeature->unprepare(); });

  // create system vocbase (before feature start)
  {
    auto const databases = VPackParser::fromJson(
        std::string("[ { \"name\": \"") +
        arangodb::StaticStrings::SystemDatabase + "\" } ]");
    EXPECT_TRUE((TRI_ERROR_NO_ERROR == dbFeature->loadDatabases(databases->slice())));
    sysDatabase->start();  // get system database from DatabaseFeature
    auto vocbase = dbFeature->useDatabase(arangodb::StaticStrings::SystemDatabase);
    arangodb::methods::Collections::createSystem(*vocbase, arangodb::tests::AnalyzerCollectionName);
  }

  // NGRAM ////////////////////////////////////////////////////////////////////
  {
    arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;
    // with unknown parameter
    EXPECT_TRUE(feature
                    .emplace(result, arangodb::StaticStrings::SystemDatabase + "::test_ngram_analyzer1",
                             "ngram",
                             VPackParser::fromJson(
                                 "{\"min\":1,\"max\":5,\"preserveOriginal\":"
                                 "false,\"invalid_parameter\":true}")
                                 ->slice())
                    .ok());
    EXPECT_TRUE(result.first);
    EXPECT_EQUAL_SLICES(VPackParser::fromJson(
                            "{\"min\":1,\"max\":5,\"preserveOriginal\":false}")
                            ->slice(),
                        result.first->properties());
  }
  {
    arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;
    // with changed parameters
    auto vpack = VPackParser::fromJson(
        "{\"min\":11,\"max\":22,\"preserveOriginal\":true}");
    EXPECT_TRUE(feature
                    .emplace(result, arangodb::StaticStrings::SystemDatabase + "::test_ngram_analyzer2",
                             "ngram", vpack->slice())
                    .ok());
    EXPECT_TRUE(result.first);
    EXPECT_EQUAL_SLICES(vpack->slice(), result.first->properties());
  }
  // DELIMITER ////////////////////////////////////////////////////////////////
  {
    arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;
    // with unknown parameter
    EXPECT_TRUE(
        feature
            .emplace(result, arangodb::StaticStrings::SystemDatabase + "::test_delimiter_analyzer1",
                     "delimiter",
                     VPackParser::fromJson(
                         "{\"delimiter\":\",\",\"invalid_parameter\":true}")
                         ->slice())
            .ok());
    EXPECT_TRUE(result.first);
    EXPECT_EQUAL_SLICES(VPackParser::fromJson("{\"delimiter\":\",\"}")->slice(),
                        result.first->properties());
  }
  {
    arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;
    // with unknown parameter
    auto vpack = VPackParser::fromJson("{\"delimiter\":\"|\"}");
    EXPECT_TRUE(feature
                    .emplace(result, arangodb::StaticStrings::SystemDatabase + "::test_delimiter_analyzer2",
                             "delimiter", vpack->slice())
                    .ok());
    EXPECT_TRUE(result.first);
    EXPECT_EQUAL_SLICES(vpack->slice(), result.first->properties());
  }
  // TEXT /////////////////////////////////////////////////////////////////////
  // with unknown parameter
  {
    arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;
    auto vpack = VPackParser::fromJson(
        "{\"locale\":\"ru_RU.UTF-8\",\"case\":\"lower\",\"invalid_"
        "parameter\":"
        "true,\"stopwords\":[],\"accent\":true,\"stemming\":false}");
    EXPECT_TRUE(feature
                    .emplace(result, arangodb::StaticStrings::SystemDatabase + "::test_text_analyzer1",
                             "text", vpack->slice())
                    .ok());
    EXPECT_TRUE(result.first);
    EXPECT_EQUAL_SLICES(
        VPackParser::fromJson(
            "{ "
            "\"locale\":\"ru_RU.utf-8\",\"case\":\"lower\",\"stopwords\":[],"
            "\"accent\":true,\"stemming\":false}")
            ->slice(),
        result.first->properties());
  }

  // no case convert in creation. Default value shown
  {
    arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;
    auto vpack = VPackParser::fromJson(
        "{\"locale\":\"ru_RU.UTF-8\",\"stopwords\":[],\"accent\":true,"
        "\"stemming\":false}");
    EXPECT_TRUE(feature
                    .emplace(result, arangodb::StaticStrings::SystemDatabase + "::test_text_analyzer2",
                             "text", vpack->slice())
                    .ok());
    EXPECT_TRUE(result.first);
    EXPECT_EQUAL_SLICES(
        VPackParser::fromJson(
            "{\"locale\":\"ru_RU.utf-8\",\"case\":\"lower\",\"stopwords\":[],"
            "\"accent\":true,\"stemming\":false}")
            ->slice(),
        result.first->properties());
  }

  // no accent in creation. Default value shown
  {
    arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;
    auto vpack = VPackParser::fromJson(
        "{\"locale\":\"ru_RU.UTF-8\",\"case\":\"lower\",\"stopwords\":[],"
        "\"stemming\":false}");
    EXPECT_TRUE(feature
                    .emplace(result, arangodb::StaticStrings::SystemDatabase + "::test_text_analyzer3",
                             "text", vpack->slice())
                    .ok());
    EXPECT_TRUE(result.first);
    EXPECT_EQUAL_SLICES(
        VPackParser::fromJson(
            "{\"locale\":\"ru_RU.utf-8\",\"case\":\"lower\",\"stopwords\":[],"
            "\"accent\":false,\"stemming\":false}")
            ->slice(),
        result.first->properties());
  }

  // no stem in creation. Default value shown
  {
    arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;
    auto vpack = VPackParser::fromJson(
        "{\"locale\":\"ru_RU.UTF-8\",\"case\":\"lower\",\"stopwords\":[],"
        "\"accent\":true}");
    EXPECT_TRUE(feature
                    .emplace(result, arangodb::StaticStrings::SystemDatabase + "::test_text_analyzer4",
                             "text", vpack->slice())
                    .ok());
    EXPECT_TRUE(result.first);
    EXPECT_EQUAL_SLICES(
        VPackParser::fromJson(
            "{\"locale\":\"ru_RU.utf-8\",\"case\":\"lower\",\"stopwords\":[],"
            "\"accent\":true,\"stemming\":true}")
            ->slice(),
        result.first->properties());
  }

  // non default values for stem, accent and case
  {
    arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;
    auto vpack = VPackParser::fromJson(
        "{\"locale\":\"ru_RU.utf-8\",\"case\":\"upper\",\"stopwords\":[],"
        "\"accent\":true,\"stemming\":false}");
    EXPECT_TRUE(feature
                    .emplace(result, arangodb::StaticStrings::SystemDatabase + "::test_text_analyzer5",
                             "text", vpack->slice())
                    .ok());
    EXPECT_TRUE(result.first);
    EXPECT_EQUAL_SLICES(vpack->slice(), result.first->properties());
  }

  // non-empty stopwords with duplicates
  {
    arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;
    auto vpack = VPackParser::fromJson(
        "{\"locale\":\"en_US.utf-8\",\"case\":\"upper\",\"stopwords\":[\"z\","
        "\"a\",\"b\",\"a\"],\"accent\":false,\"stemming\":true}");
    EXPECT_TRUE(feature
                    .emplace(result, arangodb::StaticStrings::SystemDatabase + "::test_text_analyzer6",
                             "text", vpack->slice())
                    .ok());
    EXPECT_TRUE(result.first);

    // stopwords order is not guaranteed. Need to deep check json
    auto propSlice = result.first->properties();
    ASSERT_TRUE(propSlice.hasKey("stopwords"));
    auto stopwords = propSlice.get("stopwords");
    ASSERT_TRUE(stopwords.isArray());

    std::unordered_set<std::string> expected_stopwords = {"z", "a", "b"};
    for (auto const& it : arangodb::velocypack::ArrayIterator(stopwords)) {
      ASSERT_TRUE(it.isString());
      expected_stopwords.erase(it.copyString());
    }
    ASSERT_TRUE(expected_stopwords.empty());
  }
  // with invalid locale
  {
    arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;
    auto vpack = VPackParser::fromJson("{\"locale\":\"invalid12345.UTF-8\"}");
    EXPECT_FALSE(feature
                     .emplace(result, arangodb::StaticStrings::SystemDatabase + "::test_text_analyzer7",
                              "text", vpack->slice())
                     .ok());
  }
  // STEM /////////////////////////////////////////////////////////////////////
  // with unknown parameter
  {
    arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;
    auto vpack = VPackParser::fromJson(
        "{\"locale\":\"ru_RU.UTF-8\",\"invalid_parameter\":true}");
    EXPECT_TRUE(feature
                    .emplace(result, arangodb::StaticStrings::SystemDatabase + "::test_stem_analyzer1",
                             "stem", vpack->slice())
                    .ok());
    EXPECT_TRUE(result.first);
    EXPECT_EQUAL_SLICES(VPackParser::fromJson("{\"locale\":\"ru_RU.utf-8\"}")->slice(),
                        result.first->properties());
  }
  // with invalid locale
  {
    arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;
    auto vpack = VPackParser::fromJson("{\"locale\":\"invalid12345.UTF-8\"}");
    EXPECT_FALSE(feature
                     .emplace(result, arangodb::StaticStrings::SystemDatabase + "::test_stem_analyzer2",
                              "stem", vpack->slice())
                     .ok());
  }
  // NORM /////////////////////////////////////////////////////////////////////
  // with unknown parameter
  {
    arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;
    auto vpack = VPackParser::fromJson(
        "{\"locale\":\"ru_RU.UTF-8\",\"case\":\"lower\",\"invalid_"
        "parameter\":"
        "true,\"accent\":true}");
    EXPECT_TRUE(feature
                    .emplace(result, arangodb::StaticStrings::SystemDatabase + "::test_norm_analyzer1",
                             "norm", vpack->slice())
                    .ok());
    EXPECT_TRUE(result.first);
    EXPECT_EQUAL_SLICES(
        VPackParser::fromJson(
            "{\"locale\":\"ru_RU.utf-8\",\"case\":\"lower\",\"accent\":true}")
            ->slice(),
        result.first->properties());
  }

  // no case convert in creation. Default value shown
  {
    arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;
    auto vpack =
        VPackParser::fromJson("{\"locale\":\"ru_RU.UTF-8\",\"accent\":true}");
    EXPECT_TRUE(feature
                    .emplace(result, arangodb::StaticStrings::SystemDatabase + "::test_norm_analyzer2",
                             "norm", vpack->slice())
                    .ok());
    EXPECT_TRUE(result.first);
    EXPECT_EQUAL_SLICES(
        VPackParser::fromJson(
            "{\"locale\":\"ru_RU.utf-8\",\"case\":\"none\",\"accent\":true}")
            ->slice(),
        result.first->properties());
  }

  // no accent in creation. Default value shown
  {
    arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;
    auto vpack = VPackParser::fromJson(
        "{\"locale\":\"ru_RU.UTF-8\",\"case\":\"lower\"}");
    EXPECT_TRUE(feature
                    .emplace(result, arangodb::StaticStrings::SystemDatabase + "::test_norm_analyzer3",
                             "norm", vpack->slice())
                    .ok());
    EXPECT_TRUE(result.first);
    EXPECT_EQUAL_SLICES(
        VPackParser::fromJson(
            "{\"locale\":\"ru_RU.utf-8\",\"case\":\"lower\",\"accent\":true}")
            ->slice(),
        result.first->properties());
  }
  // non default values for accent and case
  {
    arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;
    auto vpack = VPackParser::fromJson(
        "{\"locale\":\"ru_RU.utf-8\",\"case\":\"upper\",\"accent\":true}");
    EXPECT_TRUE(feature
                    .emplace(result, arangodb::StaticStrings::SystemDatabase + "::test_norm_analyzer4",
                             "norm", vpack->slice())
                    .ok());
    EXPECT_TRUE(result.first);
    EXPECT_EQUAL_SLICES(vpack->slice(), result.first->properties());
  }
  // with invalid locale
  {
    arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;
    auto vpack = VPackParser::fromJson("{\"locale\":\"invalid12345.UTF-8\"}");
    EXPECT_FALSE(feature
                     .emplace(result, arangodb::StaticStrings::SystemDatabase + "::test_norm_analyzer5",
                              "norm", vpack->slice())
                     .ok());
  }
}
