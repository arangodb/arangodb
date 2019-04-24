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

#include "catch.hpp"
#include "ClusterCommMock.h"
#include "common.h"
#include "RestHandlerMock.h"
#include "../Mocks/StorageEngineMock.h"

#include "analysis/analyzers.hpp"
#include "analysis/token_attributes.hpp"
#include "utils/utf8_path.hpp"

#include "Agency/Store.h"
#include "ApplicationFeatures/CommunicationPhase.h"
#include "Aql/AqlFunctionFeature.h"
#include "Aql/OptimizerRulesFeature.h"
#include "Aql/QueryRegistry.h"
#include "Cluster/ClusterComm.h"
#include "Cluster/ClusterFeature.h"

#if USE_ENTERPRISE
  #include "Enterprise/Ldap/LdapFeature.h"
#endif

#include "GeneralServer/AuthenticationFeature.h"
#include "Indexes/IndexFactory.h"
#include "IResearch/AgencyMock.h"
#include "IResearch/IResearchAnalyzerFeature.h"
#include "IResearch/IResearchCommon.h"
#include "IResearch/VelocyPackHelper.h"
#include "RestServer/AqlFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/DatabasePathFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "RestServer/SystemDatabaseFeature.h"
#include "RestServer/UpgradeFeature.h"
#include "RestServer/VocbaseContext.h"
#include "Scheduler/SchedulerFeature.h"
#include "Sharding/ShardingFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/ExecContext.h"
#include "Utils/OperationOptions.h"
#include "Utils/SingleCollectionTransaction.h"
#include "V8Server/V8DealerFeature.h"
#include "velocypack/Slice.h"
#include "VocBase/KeyGenerator.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ManagedDocumentResult.h"
#include "VocBase/Methods/Indexes.h"

namespace {

std::string const ANALYZER_COLLECTION_NAME("_analyzers");

struct TestIndex: public arangodb::Index {
  TestIndex(
      TRI_idx_iid_t id,
      arangodb::LogicalCollection& collection,
      arangodb::velocypack::Slice const& definition
  ): arangodb::Index(id, collection, definition) {
  }
  bool canBeDropped() const override { return false; }
  bool hasSelectivityEstimate() const override { return false; }
  bool isHidden() const override { return false; }
  bool isPersistent() const override { return false; }
  bool isSorted() const override { return false; }
  arangodb::IndexIterator* iteratorForCondition(
      arangodb::transaction::Methods* trx,
      arangodb::aql::AstNode const* node,
      arangodb::aql::Variable const* variable,
      arangodb::IndexIteratorOptions const& operations
  ) override {
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

struct TestAttribute: public irs::attribute {
  DECLARE_ATTRIBUTE_TYPE();
};

DEFINE_ATTRIBUTE_TYPE(TestAttribute);
REGISTER_ATTRIBUTE(TestAttribute); // required to open reader on segments with analized fields

struct TestTermAttribute: public irs::term_attribute {
 public:
  void value(irs::bytes_ref const& value) {
    value_ = value;
  }
};

class TestAnalyzer: public irs::analysis::analyzer {
public:
  DECLARE_ANALYZER_TYPE();
  TestAnalyzer(): irs::analysis::analyzer(TestAnalyzer::type()) {
    _attrs.emplace(_term);
    _attrs.emplace(_attr);
    _attrs.emplace(_increment); // required by field_data::invert(...)
  }

  virtual irs::attribute_view const& attributes() const NOEXCEPT override { return _attrs; }

  static ptr make(irs::string_ref const& args) {
    if (args.null()) throw std::exception();
    if (args.empty()) return nullptr;
    PTR_NAMED(TestAnalyzer, ptr);
    return ptr;
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
REGISTER_ANALYZER_JSON(TestAnalyzer, TestAnalyzer::make);

struct Analyzer {
  irs::string_ref type;
  irs::string_ref properties;
  irs::flags features;

  Analyzer() = default;
  Analyzer(irs::string_ref const& t, irs::string_ref const& p, irs::flags const& f = irs::flags::empty_instance()): type(t), properties(p), features(f) {}
};

std::map<irs::string_ref, Analyzer>const& staticAnalyzers() {
  static const std::map<irs::string_ref, Analyzer> analyzers = {
    { "identity", { "identity", irs::string_ref::NIL, { irs::frequency::type(), irs::norm::type() } } },
  };

  return analyzers;
}

// AqlValue entries must be explicitly deallocated
struct VPackFunctionParametersWrapper {
  arangodb::SmallVector<arangodb::aql::AqlValue>::allocator_type::arena_type arena;
  arangodb::aql::VPackFunctionParameters instance;
  VPackFunctionParametersWrapper(): instance(arena) {}
  ~VPackFunctionParametersWrapper() {
    for(auto& entry: instance) {
      entry.destroy();
    }
  }
  arangodb::aql::VPackFunctionParameters* operator->() { return &instance; }
  arangodb::aql::VPackFunctionParameters& operator*() { return instance; }
};

// AqlValue entrys must be explicitly deallocated
struct AqlValueWrapper {
  arangodb::aql::AqlValue instance;
  AqlValueWrapper(arangodb::aql::AqlValue&& other): instance(std::move(other)) {}
  ~AqlValueWrapper() { instance.destroy(); }
  arangodb::aql::AqlValue* operator->() { return &instance; }
  arangodb::aql::AqlValue& operator*() { return instance; }
};

// a way to set EngineSelectorFeature::ENGINE and nullify it via destructor,
// i.e. only after all TRI_vocbase_t and ApplicationServer have been destroyed
struct StorageEngineWrapper {
  StorageEngineMock instance;
  StorageEngineWrapper(
    arangodb::application_features::ApplicationServer& server
  ): instance(server) { arangodb::EngineSelectorFeature::ENGINE = &instance; }
  ~StorageEngineWrapper() { arangodb::EngineSelectorFeature::ENGINE = nullptr; }
  StorageEngineMock* operator->() { return &instance; }
  StorageEngineMock& operator*() { return instance; }
};

}

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

struct IResearchAnalyzerFeatureSetup {
  struct ClusterCommControl: arangodb::ClusterComm {
    static void reset() {
      arangodb::ClusterComm::_theInstanceInit.store(0);
    }
  };

  arangodb::consensus::Store _agencyStore{nullptr, "arango"};
  GeneralClientConnectionAgencyMock* agency;
  StorageEngineWrapper engine; // can only nullify 'ENGINE' after all TRI_vocbase_t and ApplicationServer have been destroyed
  arangodb::application_features::ApplicationServer server;
  std::vector<std::pair<arangodb::application_features::ApplicationFeature*, bool>> features;

  IResearchAnalyzerFeatureSetup(): engine(server), server(nullptr, nullptr) {
    auto* agencyCommManager = new AgencyCommManagerMock("arango");
    agency = agencyCommManager->addConnection<GeneralClientConnectionAgencyMock>(_agencyStore);
    agency = agencyCommManager->addConnection<GeneralClientConnectionAgencyMock>(_agencyStore); // need 2 connections or Agency callbacks will fail
    arangodb::AgencyCommManager::MANAGER.reset(agencyCommManager); // required for Coordinator tests

    arangodb::tests::init();

    // suppress INFO {authentication} Authentication is turned on (system only), authentication for unix sockets is turned on
    // suppress WARNING {authentication} --server.jwt-secret is insecure. Use --server.jwt-secret-keyfile instead
    arangodb::LogTopic::setLogLevel(arangodb::Logger::AUTHENTICATION.name(), arangodb::LogLevel::ERR);

    // setup required application features
    features.emplace_back(new arangodb::AuthenticationFeature(server), true);
    features.emplace_back(new arangodb::DatabaseFeature(server), false);
    features.emplace_back(new arangodb::ShardingFeature(server), false);
    features.emplace_back(new arangodb::QueryRegistryFeature(server), false); // required for constructing TRI_vocbase_t
    arangodb::application_features::ApplicationServer::server->addFeature(features.back().first); // need QueryRegistryFeature feature to be added now in order to create the system database
    features.emplace_back(new arangodb::SystemDatabaseFeature(server), true); // required for IResearchAnalyzerFeature
    features.emplace_back(new arangodb::V8DealerFeature(server), false); // required for DatabaseFeature::createDatabase(...)
    features.emplace_back(new arangodb::aql::AqlFunctionFeature(server), true); // required for IResearchAnalyzerFeature

    #if USE_ENTERPRISE
      features.emplace_back(new arangodb::LdapFeature(server), false); // required for AuthenticationFeature with USE_ENTERPRISE
    #endif

    // required for V8DealerFeature::prepare(), ClusterFeature::prepare() not required
    arangodb::application_features::ApplicationServer::server->addFeature(
      new arangodb::ClusterFeature(server)
    );

    for (auto& f : features) {
      arangodb::application_features::ApplicationServer::server->addFeature(f.first);
    }

    for (auto& f : features) {
      f.first->prepare();
    }

    auto const databases = arangodb::velocypack::Parser::fromJson(std::string("[ { \"name\": \"") + arangodb::StaticStrings::SystemDatabase + "\" } ]");
    auto* dbFeature = arangodb::application_features::ApplicationServer::lookupFeature<
      arangodb::DatabaseFeature
    >("Database");
    dbFeature->loadDatabases(databases->slice());

    for (auto& f : features) {
      if (f.second) {
        f.first->start();
      }
    }

    // suppress log messages since tests check error conditions
    arangodb::LogTopic::setLogLevel(arangodb::Logger::AGENCY.name(), arangodb::LogLevel::FATAL);
    arangodb::LogTopic::setLogLevel(arangodb::Logger::CLUSTER.name(), arangodb::LogLevel::FATAL);
    arangodb::LogTopic::setLogLevel(arangodb::iresearch::TOPIC.name(), arangodb::LogLevel::FATAL);
    irs::logger::output_le(iresearch::logger::IRL_FATAL, stderr);
  }

  ~IResearchAnalyzerFeatureSetup() {
    arangodb::LogTopic::setLogLevel(arangodb::iresearch::TOPIC.name(), arangodb::LogLevel::DEFAULT);
    arangodb::LogTopic::setLogLevel(arangodb::Logger::CLUSTER.name(), arangodb::LogLevel::DEFAULT);
    arangodb::LogTopic::setLogLevel(arangodb::Logger::AGENCY.name(), arangodb::LogLevel::DEFAULT);
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
    arangodb::LogTopic::setLogLevel(arangodb::Logger::AUTHENTICATION.name(), arangodb::LogLevel::DEFAULT);
    arangodb::AgencyCommManager::MANAGER.reset();
  }
};

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief setup
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("IResearchAnalyzerFeatureTest", "[iresearch][iresearch-feature]") {
  IResearchAnalyzerFeatureSetup s;
  UNUSED(s);

SECTION("test_auth") {
  // no ExecContext
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    CHECK((true == arangodb::iresearch::IResearchAnalyzerFeature::canUse(vocbase, arangodb::auth::Level::RW)));
  }

  // no vocbase read access
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    arangodb::iresearch::IResearchAnalyzerFeature feature(s.server);
    struct ExecContext: public arangodb::ExecContext {
      ExecContext(): arangodb::ExecContext(
        arangodb::ExecContext::Type::Default, "", "testVocbase", arangodb::auth::Level::NONE, arangodb::auth::Level::NONE
      ) {}
    } execContext;
    arangodb::ExecContextScope execContextScope(&execContext);
    auto* authFeature = arangodb::AuthenticationFeature::instance();
    auto* userManager = authFeature->userManager();
    arangodb::aql::QueryRegistry queryRegistry(0); // required for UserManager::loadFromDB()
    userManager->setQueryRegistry(&queryRegistry);
    CHECK((false == arangodb::iresearch::IResearchAnalyzerFeature::canUse(vocbase, arangodb::auth::Level::RO)));
  }

  // no collection read access (vocbase read access, no user)
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    arangodb::iresearch::IResearchAnalyzerFeature feature(s.server);
    struct ExecContext: public arangodb::ExecContext {
      ExecContext(): arangodb::ExecContext(
        arangodb::ExecContext::Type::Default, "", "testVocbase", arangodb::auth::Level::NONE, arangodb::auth::Level::RO
      ) {}
    } execContext;
    arangodb::ExecContextScope execContextScope(&execContext);
    auto* authFeature = arangodb::AuthenticationFeature::instance();
    auto* userManager = authFeature->userManager();
    arangodb::aql::QueryRegistry queryRegistry(0); // required for UserManager::loadFromDB()
    userManager->setQueryRegistry(&queryRegistry);
    auto resetUserManager = std::shared_ptr<arangodb::auth::UserManager>(userManager, [](arangodb::auth::UserManager* ptr)->void { ptr->removeAllUsers(); });
    arangodb::auth::UserMap userMap; // empty map, no user -> no permissions
    userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database
    CHECK((false == arangodb::iresearch::IResearchAnalyzerFeature::canUse(vocbase, arangodb::auth::Level::RO)));
  }

  // no collection read access (vocbase read access)
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    arangodb::iresearch::IResearchAnalyzerFeature feature(s.server);
    struct ExecContext: public arangodb::ExecContext {
      ExecContext(): arangodb::ExecContext(
        arangodb::ExecContext::Type::Default, "", "testVocbase", arangodb::auth::Level::NONE, arangodb::auth::Level::RO
      ) {}
    } execContext;
    arangodb::ExecContextScope execContextScope(&execContext);
    auto* authFeature = arangodb::AuthenticationFeature::instance();
    auto* userManager = authFeature->userManager();
    arangodb::aql::QueryRegistry queryRegistry(0); // required for UserManager::loadFromDB()
    userManager->setQueryRegistry(&queryRegistry);
    auto resetUserManager = std::shared_ptr<arangodb::auth::UserManager>(userManager, [](arangodb::auth::UserManager* ptr)->void { ptr->removeAllUsers(); });
    arangodb::auth::UserMap userMap;
    auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
    user.grantDatabase(vocbase.name(), arangodb::auth::Level::NONE); // system collections use vocbase auth level
    userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database
    CHECK((false == arangodb::iresearch::IResearchAnalyzerFeature::canUse(vocbase, arangodb::auth::Level::RO)));
  }

  // no vocbase write access
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    arangodb::iresearch::IResearchAnalyzerFeature feature(s.server);
    struct ExecContext: public arangodb::ExecContext {
      ExecContext(): arangodb::ExecContext(
        arangodb::ExecContext::Type::Default, "", "testVocbase", arangodb::auth::Level::NONE, arangodb::auth::Level::RO
      ) {}
    } execContext;
    arangodb::ExecContextScope execContextScope(&execContext);
    auto* authFeature = arangodb::AuthenticationFeature::instance();
    auto* userManager = authFeature->userManager();
    arangodb::aql::QueryRegistry queryRegistry(0); // required for UserManager::loadFromDB()
    userManager->setQueryRegistry(&queryRegistry);
    auto resetUserManager = std::shared_ptr<arangodb::auth::UserManager>(userManager, [](arangodb::auth::UserManager* ptr)->void { ptr->removeAllUsers(); });
    arangodb::auth::UserMap userMap;
    auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
    user.grantDatabase(vocbase.name(), arangodb::auth::Level::RO); // system collections use vocbase auth level
    userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database
    CHECK((true == arangodb::iresearch::IResearchAnalyzerFeature::canUse(vocbase, arangodb::auth::Level::RO)));
    CHECK((false == arangodb::iresearch::IResearchAnalyzerFeature::canUse(vocbase, arangodb::auth::Level::RW)));
  }

  // no collection write access (vocbase write access)
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    arangodb::iresearch::IResearchAnalyzerFeature feature(s.server);
    struct ExecContext: public arangodb::ExecContext {
      ExecContext(): arangodb::ExecContext(
        arangodb::ExecContext::Type::Default, "", "testVocbase", arangodb::auth::Level::NONE, arangodb::auth::Level::RW
      ) {}
    } execContext;
    arangodb::ExecContextScope execContextScope(&execContext);
    auto* authFeature = arangodb::AuthenticationFeature::instance();
    auto* userManager = authFeature->userManager();
    arangodb::aql::QueryRegistry queryRegistry(0); // required for UserManager::loadFromDB()
    userManager->setQueryRegistry(&queryRegistry);
    auto resetUserManager = std::shared_ptr<arangodb::auth::UserManager>(userManager, [](arangodb::auth::UserManager* ptr)->void { ptr->removeAllUsers(); });
    arangodb::auth::UserMap userMap;
    auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
    user.grantDatabase(vocbase.name(), arangodb::auth::Level::RO); // system collections use vocbase auth level
    userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database
    CHECK((true == arangodb::iresearch::IResearchAnalyzerFeature::canUse(vocbase, arangodb::auth::Level::RO)));
    CHECK((false == arangodb::iresearch::IResearchAnalyzerFeature::canUse(vocbase, arangodb::auth::Level::RW)));
  }

  // collection write access
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    arangodb::iresearch::IResearchAnalyzerFeature feature(s.server);
    struct ExecContext: public arangodb::ExecContext {
      ExecContext(): arangodb::ExecContext(
        arangodb::ExecContext::Type::Default, "", "testVocbase", arangodb::auth::Level::NONE, arangodb::auth::Level::RW
      ) {}
    } execContext;
    arangodb::ExecContextScope execContextScope(&execContext);
    auto* authFeature = arangodb::AuthenticationFeature::instance();
    auto* userManager = authFeature->userManager();
    arangodb::aql::QueryRegistry queryRegistry(0); // required for UserManager::loadFromDB()
    userManager->setQueryRegistry(&queryRegistry);
    auto resetUserManager = std::shared_ptr<arangodb::auth::UserManager>(userManager, [](arangodb::auth::UserManager* ptr)->void { ptr->removeAllUsers(); });
    arangodb::auth::UserMap userMap;
    auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
    user.grantDatabase(vocbase.name(), arangodb::auth::Level::RW); // system collections use vocbase auth level
    userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database
    CHECK((true == arangodb::iresearch::IResearchAnalyzerFeature::canUse(vocbase, arangodb::auth::Level::RW)));
  }
}

SECTION("test_emplace") {
  // add valid
  {
    arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;
    arangodb::iresearch::IResearchAnalyzerFeature feature(s.server);
    CHECK((true == feature.emplace(result, arangodb::StaticStrings::SystemDatabase + "::test_analyzer0", "TestAnalyzer", "abc").ok()));
    CHECK((false == !result.first));
    auto pool = feature.get(arangodb::StaticStrings::SystemDatabase + "::test_analyzer0");
    CHECK((false == !pool));
    CHECK((irs::flags() == pool->features()));
  }

  // add duplicate valid (same name+type+properties)
  {
    arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;
    arangodb::iresearch::IResearchAnalyzerFeature feature(s.server);
    CHECK((true == feature.emplace(result, arangodb::StaticStrings::SystemDatabase + "::test_analyzer1", "TestAnalyzer", "abc", irs::flags{ irs::frequency::type() }).ok()));
    CHECK((false == !result.first));
    auto pool = feature.get(arangodb::StaticStrings::SystemDatabase + "::test_analyzer1");
    CHECK((false == !pool));
    CHECK((irs::flags({ irs::frequency::type() }) == pool->features()));
    CHECK((true == feature.emplace(result, arangodb::StaticStrings::SystemDatabase + "::test_analyzer1", "TestAnalyzer", "abc", irs::flags{ irs::frequency::type() }).ok()));
    CHECK((false == !result.first));
    CHECK((false == !feature.get(arangodb::StaticStrings::SystemDatabase + "::test_analyzer1")));
  }

  // add duplicate invalid (same name+type different properties)
  {
    arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;
    arangodb::iresearch::IResearchAnalyzerFeature feature(s.server);
    CHECK((true == feature.emplace(result, arangodb::StaticStrings::SystemDatabase + "::test_analyzer2", "TestAnalyzer", "abc").ok()));
    CHECK((false == !result.first));
    auto pool = feature.get(arangodb::StaticStrings::SystemDatabase + "::test_analyzer2");
    CHECK((false == !pool));
    CHECK((irs::flags() == pool->features()));
    CHECK((false == feature.emplace(result, arangodb::StaticStrings::SystemDatabase + "::test_analyzer2", "TestAnalyzer", "abcd").ok()));
    CHECK((false == !feature.get(arangodb::StaticStrings::SystemDatabase + "::test_analyzer2")));
  }

  // add duplicate invalid (same name+type+properties different features)
  {
    arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;
    arangodb::iresearch::IResearchAnalyzerFeature feature(s.server);
    CHECK((true == feature.emplace(result, arangodb::StaticStrings::SystemDatabase + "::test_analyzer2", "TestAnalyzer", "abc").ok()));
    CHECK((false == !result.first));
    auto pool = feature.get(arangodb::StaticStrings::SystemDatabase + "::test_analyzer2");
    CHECK((false == !pool));
    CHECK((irs::flags() == pool->features()));
    CHECK((false == feature.emplace(result, arangodb::StaticStrings::SystemDatabase + "::test_analyzer2", "TestAnalyzer", "abc", irs::flags{ irs::frequency::type() }).ok()));
    CHECK((false == !feature.get(arangodb::StaticStrings::SystemDatabase + "::test_analyzer2")));
  }

  // add duplicate invalid (same name+properties different type)
  {
    arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;
    arangodb::iresearch::IResearchAnalyzerFeature feature(s.server);
    CHECK((true == feature.emplace(result, arangodb::StaticStrings::SystemDatabase + "::test_analyzer3", "TestAnalyzer", "abc").ok()));
    CHECK((false == !result.first));
    auto pool = feature.get(arangodb::StaticStrings::SystemDatabase + "::test_analyzer3");
    CHECK((false == !pool));
    CHECK((irs::flags() == pool->features()));
    CHECK((false == feature.emplace(result, arangodb::StaticStrings::SystemDatabase + "::test_analyzer3", "invalid", "abc").ok()));
    CHECK((false == !feature.get(arangodb::StaticStrings::SystemDatabase + "::test_analyzer3")));
  }

  // add invalid (instance creation failure)
  {
    arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;
    arangodb::iresearch::IResearchAnalyzerFeature feature(s.server);
    CHECK((false == feature.emplace(result, arangodb::StaticStrings::SystemDatabase + "::test_analyzer4", "TestAnalyzer", "").ok()));
    CHECK((true == !feature.get(arangodb::StaticStrings::SystemDatabase + "::test_analyzer4")));
  }

  // add invalid (instance creation exception)
  {
    arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;
    arangodb::iresearch::IResearchAnalyzerFeature feature(s.server);
    CHECK((false == feature.emplace(result, arangodb::StaticStrings::SystemDatabase + "::test_analyzer5", "TestAnalyzer", irs::string_ref::NIL).ok()));
    CHECK((true == !feature.get(arangodb::StaticStrings::SystemDatabase + "::test_analyzer5")));
  }

  // add invalid (not registred)
  {
    arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;
    arangodb::iresearch::IResearchAnalyzerFeature feature(s.server);
    CHECK((false == feature.emplace(result, arangodb::StaticStrings::SystemDatabase + "::test_analyzer6", "invalid", irs::string_ref::NIL).ok()));
    CHECK((true == !feature.get(arangodb::StaticStrings::SystemDatabase + "::test_analyzer6")));
  }

  // add valid inRecovery (failure)
  {
    arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;
    arangodb::iresearch::IResearchAnalyzerFeature feature(s.server);
    auto before = StorageEngineMock::inRecoveryResult;
    StorageEngineMock::inRecoveryResult = true;
    auto restore = irs::make_finally([&before]()->void { StorageEngineMock::inRecoveryResult = before; });
    CHECK((true == feature.emplace(result, arangodb::StaticStrings::SystemDatabase + "::test_analyzer8", "TestAnalyzer", "abc").ok()));
    CHECK((true == !feature.get(arangodb::StaticStrings::SystemDatabase + "::test_analyzer8")));
  }

  // add invalid (unsupported feature)
  {
    arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;
    arangodb::iresearch::IResearchAnalyzerFeature feature(s.server);
    CHECK((false == feature.emplace(result, arangodb::StaticStrings::SystemDatabase + "::test_analyzer9", "TestAnalyzer", "abc", {irs::document::type()}).ok()));
    CHECK((true == !feature.get(arangodb::StaticStrings::SystemDatabase + "::test_analyzer9")));
  }

  // add invalid ('position' without 'frequency')
  {
    arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;
    arangodb::iresearch::IResearchAnalyzerFeature feature(s.server);
    CHECK((false == feature.emplace(result, arangodb::StaticStrings::SystemDatabase + "::test_analyzer10", "TestAnalyzer", "abc", {irs::position::type()}).ok()));
    CHECK((true == !feature.get(arangodb::StaticStrings::SystemDatabase + "::test_analyzer10")));
  }

  // add invalid (properties too large)
  {
    arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;
    arangodb::iresearch::IResearchAnalyzerFeature feature(s.server);
    std::string properties(1024*1024 + 1, 'x'); // +1 char longer then limit
    CHECK((false == feature.emplace(result, arangodb::StaticStrings::SystemDatabase + "::test_analyzer11", "TestAnalyzer", properties).ok()));
    CHECK((true == !feature.get(arangodb::StaticStrings::SystemDatabase + "::test_analyzer11")));
  }

  // add static analyzer
  {
    arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;
    arangodb::iresearch::IResearchAnalyzerFeature feature(s.server);
    CHECK((true == feature.emplace(result, "identity", "identity", irs::string_ref::NIL, irs::flags{ irs::frequency::type(), irs::norm::type() }).ok()));
    CHECK((false == !result.first));
    auto pool = feature.get("identity");
    CHECK((false == !pool));
    CHECK((irs::flags({irs::norm::type(), irs::frequency::type() }) == pool->features()));
    auto analyzer = pool->get();
    CHECK((false == !analyzer));
  }
}

SECTION("test_get") {
  auto* dbFeature = arangodb::application_features::ApplicationServer::lookupFeature<
    arangodb::DatabaseFeature
  >("Database");
  REQUIRE((false == !dbFeature));
  arangodb::AqlFeature aqlFeature(s.server);
  aqlFeature.start(); // required for Query::Query(...), must not call ~AqlFeature() for the duration of the test

  {
    arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;
    arangodb::iresearch::IResearchAnalyzerFeature feature(s.server);

    REQUIRE((feature.emplace(result, arangodb::StaticStrings::SystemDatabase + "::test_analyzer", "TestAnalyzer", "abc").ok()));

    // get valid
    {
      auto pool = feature.get(arangodb::StaticStrings::SystemDatabase + "::test_analyzer");
      REQUIRE((false == !pool));
      CHECK((irs::flags() == pool->features()));
      auto analyzer = pool.get();
      CHECK((false == !analyzer));
    }

    // get invalid
    {
      CHECK((true == !feature.get(arangodb::StaticStrings::SystemDatabase + "::invalid")));
    }

    // get static analyzer
    {
      auto pool = feature.get("identity");
      REQUIRE((false == !pool));
      CHECK((irs::flags({irs::norm::type(), irs::frequency::type()}) == pool->features()));
      auto analyzer = pool->get();
      CHECK((false == !analyzer));
    }
  }

  // get existing with parameter match
  {
    TRI_vocbase_t* vocbase;
    REQUIRE((TRI_ERROR_NO_ERROR == dbFeature->createDatabase(1, "testVocbase", vocbase)));
    auto dropDB = irs::make_finally([dbFeature]()->void { dbFeature->dropDatabase("testVocbase", true, true); });
    arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;
    arangodb::iresearch::IResearchAnalyzerFeature feature(s.server);
    REQUIRE((feature.emplace(result, "testVocbase::test_analyzer", "TestAnalyzer", "abc", { irs::frequency::type() } ).ok()));

    CHECK((false == !feature.get("testVocbase::test_analyzer", "TestAnalyzer", "abc", { irs::frequency::type() })));
  }

  // get exisitng with type mismatch
  {
    TRI_vocbase_t* vocbase;
    REQUIRE((TRI_ERROR_NO_ERROR == dbFeature->createDatabase(1, "testVocbase", vocbase)));
    auto dropDB = irs::make_finally([dbFeature]()->void { dbFeature->dropDatabase("testVocbase", true, true); });
    arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;
    arangodb::iresearch::IResearchAnalyzerFeature feature(s.server);
    REQUIRE((feature.emplace(result, "testVocbase::test_analyzer", "TestAnalyzer", "abc", { irs::frequency::type() } ).ok()));

    CHECK((true == !feature.get("testVocbase::test_analyzer", "identity", "abc", { irs::frequency::type() })));
  }

  // get existing with properties mismatch
  {
    TRI_vocbase_t* vocbase;
    REQUIRE((TRI_ERROR_NO_ERROR == dbFeature->createDatabase(1, "testVocbase", vocbase)));
    auto dropDB = irs::make_finally([dbFeature]()->void { dbFeature->dropDatabase("testVocbase", true, true); });
    arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;
    arangodb::iresearch::IResearchAnalyzerFeature feature(s.server);
    REQUIRE((feature.emplace(result, "testVocbase::test_analyzer", "TestAnalyzer", "abc", { irs::frequency::type() } ).ok()));

    CHECK((true == !feature.get("testVocbase::test_analyzer", "TestAnalyzer", "abcd", { irs::frequency::type() })));
  }

  // get existing with features mismatch
  {
    TRI_vocbase_t* vocbase;
    REQUIRE((TRI_ERROR_NO_ERROR == dbFeature->createDatabase(1, "testVocbase", vocbase)));
    auto dropDB = irs::make_finally([dbFeature]()->void { dbFeature->dropDatabase("testVocbase", true, true); });
    arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;
    arangodb::iresearch::IResearchAnalyzerFeature feature(s.server);
    REQUIRE((feature.emplace(result, "testVocbase::test_analyzer", "TestAnalyzer", "abc", { irs::frequency::type() } ).ok()));

    CHECK((true == !feature.get("testVocbase::test_analyzer", "TestAnalyzer", "abc", { irs::position::type() })));
  }

  // get missing (single-server)
  {
    TRI_vocbase_t* vocbase;
    REQUIRE((TRI_ERROR_NO_ERROR == dbFeature->createDatabase(1, "testVocbase", vocbase)));
    auto dropDB = irs::make_finally([dbFeature]()->void { dbFeature->dropDatabase("testVocbase", true, true); });

    arangodb::iresearch::IResearchAnalyzerFeature feature(s.server);
    CHECK((true == !feature.get("testVocbase::test_analyzer", "TestAnalyzer", "abc", { irs::frequency::type() })));
  }

  // get missing (coordinator)
  {
    auto before = arangodb::ServerState::instance()->getRole();
    arangodb::ServerState::instance()->setRole(arangodb::ServerState::ROLE_COORDINATOR);
    auto restore = irs::make_finally([&before]()->void { arangodb::ServerState::instance()->setRole(before); });

    arangodb::iresearch::IResearchAnalyzerFeature feature(s.server);
    CHECK((true == !feature.get("testVocbase::test_analyzer", "TestAnalyzer", "abc", { irs::frequency::type() })));
  }

  // get missing (db-server)
  {
    auto before = arangodb::ServerState::instance()->getRole();
    arangodb::ServerState::instance()->setRole(arangodb::ServerState::ROLE_DBSERVER);
    auto restore = irs::make_finally([&before]()->void { arangodb::ServerState::instance()->setRole(before); });

    arangodb::iresearch::IResearchAnalyzerFeature feature(s.server);
    CHECK((false == !feature.get("testVocbase::test_analyzer", "TestAnalyzer", "abc", { irs::frequency::type() })));
  }

  // add index factory
  {
    struct IndexTypeFactory: public arangodb::IndexTypeFactory {
      virtual bool equal(
          arangodb::velocypack::Slice const& lhs,
          arangodb::velocypack::Slice const& rhs
      ) const override {
        return false;
      }

      virtual arangodb::Result instantiate(
          std::shared_ptr<arangodb::Index>& index,
          arangodb::LogicalCollection& collection,
          arangodb::velocypack::Slice const& definition,
          TRI_idx_iid_t id,
          bool isClusterConstructor
      ) const override {
        auto* ci = arangodb::ClusterInfo::instance();
        REQUIRE((nullptr != ci));
        auto* feature = arangodb::application_features::ApplicationServer::lookupFeature<arangodb::iresearch::IResearchAnalyzerFeature>();
        REQUIRE((feature));
        ci->invalidatePlan(); // invalidate plan to test recursive lock aquisition in ClusterInfo::loadPlan()
        CHECK((true == !feature->get(arangodb::StaticStrings::SystemDatabase + "::missing", "TestAnalyzer", irs::string_ref::NIL, irs::flags())));
        index = std::make_shared<TestIndex>(id, collection, definition);
        return arangodb::Result();
      }

      virtual arangodb::Result normalize(
          arangodb::velocypack::Builder& normalized,
          arangodb::velocypack::Slice definition,
          bool isCreation,
          TRI_vocbase_t const& vocbase
      ) const override {
        CHECK((arangodb::iresearch::mergeSlice(normalized, definition)));
        return arangodb::Result();
      }
    };
    static const IndexTypeFactory indexTypeFactory;
    auto& indexFactory = const_cast<arangodb::IndexFactory&>(arangodb::EngineSelectorFeature::ENGINE->indexFactory());
    indexFactory.emplace("testType", indexTypeFactory);
  }

  // get missing via link creation (coordinator) ensure no recursive ClusterInfo::loadPlan() call
  {
    auto createCollectionJson = arangodb::velocypack::Parser::fromJson(std::string("{ \"id\": 42, \"name\": \"") + ANALYZER_COLLECTION_NAME + "\", \"isSystem\": true, \"shards\": { \"same-as-dummy-shard-id\": [ \"shard-server-does-not-matter\" ] }, \"type\": 2 }"); // 'id' and 'shards' required for coordinator tests
    auto collectionId = std::to_string(42);
    auto before = arangodb::ServerState::instance()->getRole();
    arangodb::ServerState::instance()->setRole(arangodb::ServerState::ROLE_COORDINATOR);
    auto restore = irs::make_finally([&before]()->void { arangodb::ServerState::instance()->setRole(before); });

    // create a new instance of an ApplicationServer and fill it with the required features
    // cannot use the existing server since its features already have some state
    std::shared_ptr<arangodb::application_features::ApplicationServer> originalServer(
      arangodb::application_features::ApplicationServer::server,
      [](arangodb::application_features::ApplicationServer* ptr)->void {
        arangodb::application_features::ApplicationServer::server = ptr;
      }
    );
    arangodb::application_features::ApplicationServer::server = nullptr; // avoid "ApplicationServer initialized twice"
    arangodb::application_features::ApplicationServer server(nullptr, nullptr);
    arangodb::iresearch::IResearchAnalyzerFeature* feature;
    arangodb::DatabaseFeature* dbFeature;
    arangodb::SystemDatabaseFeature* sysDatabase;
    server.addFeature(new arangodb::ClusterFeature(server)); // required to create ClusterInfo instance
    server.addFeature(dbFeature = new arangodb::DatabaseFeature(server)); // required for IResearchAnalyzerFeature::emplace(...)
    server.addFeature(new arangodb::QueryRegistryFeature(server)); // required for constructing TRI_vocbase_t
    server.addFeature(new arangodb::ShardingFeature(server)); // required for Collections::create(...)
    server.addFeature(sysDatabase = new arangodb::SystemDatabaseFeature(server)); // required for IResearchAnalyzerFeature::start()
    server.addFeature(new arangodb::V8DealerFeature(server)); // required for DatabaseFeature::createDatabase(...)
    server.addFeature(new arangodb::application_features::CommunicationFeaturePhase(server)); // required for SimpleHttpClient::doRequest()
    server.addFeature(feature = new arangodb::iresearch::IResearchAnalyzerFeature(server)); // required for running upgrade task
    arangodb::aql::OptimizerRulesFeature(server).prepare(); // required for Query::preparePlan(...)
    auto clearOptimizerRules = irs::make_finally([&s]()->void { arangodb::aql::OptimizerRulesFeature(s.server).unprepare(); });

    // create system vocbase (before feature start)
    {
      auto const databases = arangodb::velocypack::Parser::fromJson(std::string("[ { \"name\": \"") + arangodb::StaticStrings::SystemDatabase + "\" } ]");
      CHECK((TRI_ERROR_NO_ERROR == dbFeature->loadDatabases(databases->slice())));
      sysDatabase->start(); // get system database from DatabaseFeature
    }

    server.getFeature<arangodb::ClusterFeature>("Cluster")->prepare(); // create ClusterInfo instance
    server.getFeature<arangodb::ShardingFeature>("Sharding")->prepare(); // required for Collections::create(...), register sharding types
    arangodb::AgencyCommManager::MANAGER->start(); // initialize agency

    ClusterCommMock clusterComm;
    auto scopedClusterComm = ClusterCommMock::setInstance(clusterComm);
    auto* ci = arangodb::ClusterInfo::instance();
    REQUIRE((nullptr != ci));

    auto system = sysDatabase->use();
    REQUIRE((ci->createCollectionCoordinator(system->name(), collectionId, 0, 1, false, createCollectionJson->slice(), 0.0).ok()));
    auto logicalCollection = ci->getCollection(system->name(), collectionId);
    REQUIRE((false == !logicalCollection));

    // simulate heartbeat thread
    {
      auto const colPath = "/Current/Collections/_system/42";
      auto const colValue = arangodb::velocypack::Parser::fromJson("{ \"same-as-dummy-shard-id\": { \"indexes\": [ { \"id\": \"1\" } ], \"servers\": [ \"same-as-dummy-shard-server\" ] } }"); // '1' must match 'idString' in ClusterInfo::ensureIndexCoordinatorInner(...)
      CHECK(arangodb::AgencyComm().setValue(colPath, colValue->slice(), 0.0).successful());
      auto const dummyPath = "/Plan/Collections";
      auto const dummyValue = arangodb::velocypack::Parser::fromJson("{ \"_system\": { \"42\": { \"name\": \"testCollection\", \"shards\": { \"same-as-dummy-shard-id\": [ \"same-as-dummy-shard-server\" ] } } } }");
      CHECK(arangodb::AgencyComm().setValue(dummyPath, dummyValue->slice(), 0.0).successful());
      auto const versionPath = "/Plan/Version";
      auto const versionValue = arangodb::velocypack::Parser::fromJson(std::to_string(ci->getPlanVersion() + 1));
      CHECK((arangodb::AgencyComm().setValue(versionPath, versionValue->slice(), 0.0).successful())); // force loadPlan() update
      ci->invalidateCurrent(); // force reload of 'Current'
    }

    // insert response for expected analyzer lookup
    {
      arangodb::ClusterCommResult response;
      response.status = arangodb::ClusterCommOpStatus::CL_COMM_RECEIVED;
      response.result = std::make_shared<arangodb::httpclient::SimpleHttpResult>();
      response.result->getBody().appendText("{ \"result\": { \"snippets\": { \"6:shard-id-does-not-matter\": \"value-does-not-matter\" } } }").ensureNullTerminated(); // '6' must match GATHER Node id in ExecutionEngine::createBlocks(...)
      clusterComm._responses.emplace_back(std::move(response));
    }

    // insert response for expected analyzer reload from collection
    {
      arangodb::ClusterCommResult response;
      response.status = arangodb::ClusterCommOpStatus::CL_COMM_SENT;
      response.result = std::make_shared<arangodb::httpclient::SimpleHttpResult>();
      response.result->getBody().appendText("{ \"done\": true, \"nrItems\": 1, \"nrRegs\": 1, \"data\": [ 1 ], \"raw\": [ null, null, { \"_key\": \"key-does-not-matter\", \"name\": \"abc\", \"type\": \"TestAnalyzer\", \"properties\": \"abc\" } ] }").ensureNullTerminated(); // 'data' value must be 1 as per AqlItemBlock::AqlItemBlock(...), first 2 'raw' values ignored, 'nrRegs' must be 1 or assertion failure in ExecutionBlockImpl<Executor>::requestWrappedBlock(...)
      clusterComm._responses.emplace_back(std::move(response));
    }

    arangodb::velocypack::Builder builder;
    arangodb::velocypack::Builder tmp;

    builder.openObject();
    builder.add(arangodb::StaticStrings::IndexType, arangodb::velocypack::Value("testType"));
    builder.add(arangodb::StaticStrings::IndexFields, arangodb::velocypack::Slice::emptyArraySlice());
    builder.close();
    CHECK((arangodb::methods::Indexes::ensureIndex(logicalCollection.get(), builder.slice(), true, tmp).ok()));
  }
}

SECTION("test_identity") {
  // test static 'identity'
  {
    auto pool = arangodb::iresearch::IResearchAnalyzerFeature::identity();
    CHECK((false == !pool));
    CHECK((irs::flags({irs::norm::type(), irs::frequency::type()}) == pool->features()));
    CHECK(("identity" == pool->name()));
    auto analyzer = pool->get();
    CHECK((false == !analyzer));
    auto& term = analyzer->attributes().get<irs::term_attribute>();
    CHECK((false == !term));
    CHECK((!analyzer->next()));
    CHECK((analyzer->reset("abc def ghi")));
    CHECK((analyzer->next()));
    CHECK((irs::ref_cast<irs::byte_type>(irs::string_ref("abc def ghi")) == term->value()));
    CHECK((!analyzer->next()));
    CHECK((analyzer->reset("123 456")));
    CHECK((analyzer->next()));
    CHECK((irs::ref_cast<irs::byte_type>(irs::string_ref("123 456")) == term->value()));
    CHECK((!analyzer->next()));
  }

  // test registered 'identity'
  {
    arangodb::iresearch::IResearchAnalyzerFeature feature(s.server);
    CHECK((false == !feature.get("identity")));
    auto pool = feature.get("identity");
    REQUIRE((false == !pool));
    CHECK((irs::flags({irs::norm::type(), irs::frequency::type()}) == pool->features()));
    CHECK(("identity" == pool->name()));
    auto analyzer = pool->get();
    CHECK((false == !analyzer));
    auto& term = analyzer->attributes().get<irs::term_attribute>();
    CHECK((false == !term));
    CHECK((!analyzer->next()));
    CHECK((analyzer->reset("abc def ghi")));
    CHECK((analyzer->next()));
    CHECK((irs::ref_cast<irs::byte_type>(irs::string_ref("abc def ghi")) == term->value()));
    CHECK((!analyzer->next()));
    CHECK((analyzer->reset("123 456")));
    CHECK((analyzer->next()));
    CHECK((irs::ref_cast<irs::byte_type>(irs::string_ref("123 456")) == term->value()));
    CHECK((!analyzer->next()));
  }
}

SECTION("test_normalize") {
  TRI_vocbase_t active(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "active");
  TRI_vocbase_t system(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "system");

  // normalize 'identity' (with prefix)
  {
    irs::string_ref analyzer = "identity";
    auto normalized = arangodb::iresearch::IResearchAnalyzerFeature::normalize(analyzer, active, system, true);
    CHECK((std::string("identity") == normalized));
  }

  // normalize 'identity' (without prefix)
  {
    irs::string_ref analyzer = "identity";
    auto normalized = arangodb::iresearch::IResearchAnalyzerFeature::normalize(analyzer, active, system, true);
    CHECK((std::string("identity") == normalized));
  }

  // normalize NIL (with prefix)
  {
    irs::string_ref analyzer = irs::string_ref::NIL;
    auto normalized = arangodb::iresearch::IResearchAnalyzerFeature::normalize(analyzer, active, system, true);
    CHECK((std::string("active::") == normalized));
  }

  // normalize NIL (without prefix)
  {
    irs::string_ref analyzer = irs::string_ref::NIL;
    auto normalized = arangodb::iresearch::IResearchAnalyzerFeature::normalize(analyzer, active, system, false);
    CHECK((std::string("") == normalized));
  }

  // normalize EMPTY (with prefix)
  {
    irs::string_ref analyzer = irs::string_ref::EMPTY;
    auto normalized = arangodb::iresearch::IResearchAnalyzerFeature::normalize(analyzer, active, system, true);
    CHECK((std::string("active::") == normalized));
  }

  // normalize EMPTY (without prefix)
  {
    irs::string_ref analyzer = irs::string_ref::EMPTY;
    auto normalized = arangodb::iresearch::IResearchAnalyzerFeature::normalize(analyzer, active, system, false);
    CHECK((std::string("") == normalized));
  }

  // normalize delimiter (with prefix)
  {
    irs::string_ref analyzer = "::";
    auto normalized = arangodb::iresearch::IResearchAnalyzerFeature::normalize(analyzer, active, system, true);
    CHECK((std::string("system::") == normalized));
  }

  // normalize delimiter (without prefix)
  {
    irs::string_ref analyzer = "::";
    auto normalized = arangodb::iresearch::IResearchAnalyzerFeature::normalize(analyzer, active, system, false);
    CHECK((std::string("::") == normalized));
  }

  // normalize delimiter + name (with prefix)
  {
    irs::string_ref analyzer = "::name";
    auto normalized = arangodb::iresearch::IResearchAnalyzerFeature::normalize(analyzer, active, system, true);
    CHECK((std::string("system::name") == normalized));
  }

  // normalize delimiter + name (without prefix)
  {
    irs::string_ref analyzer = "::name";
    auto normalized = arangodb::iresearch::IResearchAnalyzerFeature::normalize(analyzer, active, system, false);
    CHECK((std::string("::name") == normalized));
  }

  // normalize no-delimiter + name (with prefix)
  {
    irs::string_ref analyzer = "name";
    auto normalized = arangodb::iresearch::IResearchAnalyzerFeature::normalize(analyzer, active, system, true);
    CHECK((std::string("active::name") == normalized));
  }

  // normalize no-delimiter + name (without prefix)
  {
    irs::string_ref analyzer = "name";
    auto normalized = arangodb::iresearch::IResearchAnalyzerFeature::normalize(analyzer, active, system, false);
    CHECK((std::string("name") == normalized));
  }

  // normalize system + delimiter (with prefix)
  {
    irs::string_ref analyzer = "system::";
    auto normalized = arangodb::iresearch::IResearchAnalyzerFeature::normalize(analyzer, active, system, true);
    CHECK((std::string("system::") == normalized));
  }

  // normalize system + delimiter (without prefix)
  {
    irs::string_ref analyzer = "system::";
    auto normalized = arangodb::iresearch::IResearchAnalyzerFeature::normalize(analyzer, active, system, false);
    CHECK((std::string("::") == normalized));
  }

  // normalize vocbase + delimiter (with prefix)
  {
    irs::string_ref analyzer = "active::";
    auto normalized = arangodb::iresearch::IResearchAnalyzerFeature::normalize(analyzer, active, system, true);
    CHECK((std::string("active::") == normalized));
  }

  // normalize vocbase + delimiter (without prefix)
  {
    irs::string_ref analyzer = "active::";
    auto normalized = arangodb::iresearch::IResearchAnalyzerFeature::normalize(analyzer, active, system, false);
    CHECK((std::string("") == normalized));
  }

  // normalize system + delimiter + name (with prefix)
  {
    irs::string_ref analyzer = "system::name";
    auto normalized = arangodb::iresearch::IResearchAnalyzerFeature::normalize(analyzer, active, system, true);
    CHECK((std::string("system::name") == normalized));
  }

  // normalize system + delimiter + name (without prefix)
  {
    irs::string_ref analyzer = "system::name";
    auto normalized = arangodb::iresearch::IResearchAnalyzerFeature::normalize(analyzer, active, system, false);
    CHECK((std::string("::name") == normalized));
  }

  // normalize vocbase + delimiter + name (with prefix)
  {
    irs::string_ref analyzer = "active::name";
    auto normalized = arangodb::iresearch::IResearchAnalyzerFeature::normalize(analyzer, active, system, true);
    CHECK((std::string("active::name") == normalized));
  }

  // normalize vocbase + delimiter + name (without prefix)
  {
    irs::string_ref analyzer = "active::name";
    auto normalized = arangodb::iresearch::IResearchAnalyzerFeature::normalize(analyzer, active, system, false);
    CHECK((std::string("name") == normalized));
  }
}

SECTION("test_static_analyzer_features") {
  // test registered 'identity'
  {
    arangodb::iresearch::IResearchAnalyzerFeature feature(s.server);
    for (auto& analyzerEntry : staticAnalyzers()) {
      CHECK((false == !feature.get(analyzerEntry.first)));
      auto pool = feature.get(analyzerEntry.first);
      REQUIRE((false == !pool));
      CHECK(analyzerEntry.second.features == pool->features());
      CHECK(analyzerEntry.first == pool->name());
      auto analyzer = pool->get();
      CHECK((false == !analyzer));
      auto& term = analyzer->attributes().get<irs::term_attribute>();
      CHECK((false == !term));
    }
  }
}

SECTION("test_persistence") {
  static std::vector<std::string> const EMPTY;
  auto* database = arangodb::application_features::ApplicationServer::lookupFeature<
    arangodb::SystemDatabaseFeature
  >();
  auto vocbase = database->use();

  // ensure there is an empty configuration collection
  {
    auto createCollectionJson = arangodb::velocypack::Parser::fromJson(std::string("{ \"name\": \"") + ANALYZER_COLLECTION_NAME + "\", \"isSystem\": true }");
    CHECK((false == !vocbase->createCollection(createCollectionJson->slice())));
  }

  // read invalid configuration (missing attributes)
  {
    {
      std::string collection(ANALYZER_COLLECTION_NAME);
      arangodb::OperationOptions options;
      arangodb::SingleCollectionTransaction trx(
        arangodb::transaction::StandaloneContext::Create(*vocbase),
        collection,
        arangodb::AccessMode::Type::WRITE
      );
      trx.begin();
      trx.truncate(collection, options);
      trx.insert(collection, arangodb::velocypack::Parser::fromJson("{}")->slice(), options);
      trx.insert(collection, arangodb::velocypack::Parser::fromJson("{                        \"type\": \"identity\", \"properties\": null}")->slice(), options);
      trx.insert(collection, arangodb::velocypack::Parser::fromJson("{\"name\": 12345,        \"type\": \"identity\", \"properties\": null}")->slice(), options);
      trx.insert(collection, arangodb::velocypack::Parser::fromJson("{\"name\": \"invalid1\",                         \"properties\": null}")->slice(), options);
      trx.insert(collection, arangodb::velocypack::Parser::fromJson("{\"name\": \"invalid2\", \"type\": 12345,        \"properties\": null}")->slice(), options);
      trx.commit();
    }

    std::map<std::string, std::pair<irs::string_ref, irs::string_ref>> expected = {};
    arangodb::iresearch::IResearchAnalyzerFeature feature(s.server);

    feature.start(); // load persisted analyzers

    feature.visit([&expected](
      arangodb::iresearch::IResearchAnalyzerFeature::AnalyzerPool::ptr const& analyzer
    )->bool {
      if (staticAnalyzers().find(analyzer->name()) != staticAnalyzers().end()) {
        return true; // skip static analyzers
      }

      auto itr = expected.find(analyzer->name());
      CHECK((itr != expected.end()));
      CHECK((itr->second.first == analyzer->type()));
      CHECK((itr->second.second == analyzer->properties()));
      expected.erase(itr);
      return true;
    });
    CHECK((expected.empty()));
  }

  // read invalid configuration (duplicate non-identical records)
  {
    {
      std::string collection(ANALYZER_COLLECTION_NAME);
      arangodb::OperationOptions options;
      arangodb::SingleCollectionTransaction trx(
        arangodb::transaction::StandaloneContext::Create(*vocbase),
        collection,
        arangodb::AccessMode::Type::WRITE
      );
      trx.begin();
      trx.truncate(collection, options);
      trx.insert(collection, arangodb::velocypack::Parser::fromJson("{\"name\": \"valid\", \"type\": \"identity\", \"properties\": null}")->slice(), options);
      trx.insert(collection, arangodb::velocypack::Parser::fromJson("{\"name\": \"valid\", \"type\": \"identity\", \"properties\": \"abc\"}")->slice(), options);
      trx.commit();
    }

    arangodb::iresearch::IResearchAnalyzerFeature feature(s.server);
    CHECK_THROWS((feature.start()));
  }

  // read valid configuration (different parameter options)
  {
    {
      std::string collection(ANALYZER_COLLECTION_NAME);
      arangodb::OperationOptions options;
      arangodb::SingleCollectionTransaction trx(
        arangodb::transaction::StandaloneContext::Create(*vocbase),
        collection,
        arangodb::AccessMode::Type::WRITE
      );
      trx.begin();
      trx.truncate(collection, options);
      trx.insert(collection, arangodb::velocypack::Parser::fromJson("{\"name\": \"valid0\", \"type\": \"identity\", \"properties\": null                      }")->slice(), options);
      trx.insert(collection, arangodb::velocypack::Parser::fromJson("{\"name\": \"valid1\", \"type\": \"identity\", \"properties\": true                      }")->slice(), options);
      trx.insert(collection, arangodb::velocypack::Parser::fromJson("{\"name\": \"valid2\", \"type\": \"identity\", \"properties\": \"abc\"                   }")->slice(), options);
      trx.insert(collection, arangodb::velocypack::Parser::fromJson("{\"name\": \"valid3\", \"type\": \"identity\", \"properties\": 3.14                      }")->slice(), options);
      trx.insert(collection, arangodb::velocypack::Parser::fromJson("{\"name\": \"valid4\", \"type\": \"identity\", \"properties\": [ 1, \"abc\" ]            }")->slice(), options);
      trx.insert(collection, arangodb::velocypack::Parser::fromJson("{\"name\": \"valid5\", \"type\": \"identity\", \"properties\": { \"a\": 7, \"b\": \"c\" }}")->slice(), options);
      trx.commit();
    }

    std::map<std::string, std::pair<irs::string_ref, irs::string_ref>> expected = {
      { arangodb::StaticStrings::SystemDatabase + "::valid0", { "identity", irs::string_ref::NIL } },
      { arangodb::StaticStrings::SystemDatabase + "::valid2", { "identity", "abc" } },
      { arangodb::StaticStrings::SystemDatabase + "::valid4", { "identity", "[1,\"abc\"]" } },
      { arangodb::StaticStrings::SystemDatabase + "::valid5", { "identity", "{\"a\":7,\"b\":\"c\"}" } },
    };
    arangodb::iresearch::IResearchAnalyzerFeature feature(s.server);

    feature.start(); // load persisted analyzers

    feature.visit([&expected](
      arangodb::iresearch::IResearchAnalyzerFeature::AnalyzerPool::ptr const& analyzer
    )->bool {
      if (staticAnalyzers().find(analyzer->name()) != staticAnalyzers().end()) {
        return true; // skip static analyzers
      }

      auto itr = expected.find(analyzer->name());
      CHECK((itr != expected.end()));
      CHECK((itr->second.first == analyzer->type()));
      CHECK((itr->second.second == analyzer->properties()));
      expected.erase(itr);
      return true;
    });
    CHECK((expected.empty()));
  }

  // add new records
  {
    {
      arangodb::OperationOptions options;
      arangodb::ManagedDocumentResult result;
      auto collection = vocbase->lookupCollection(ANALYZER_COLLECTION_NAME);
      arangodb::transaction::Methods trx(
        arangodb::transaction::StandaloneContext::Create(*vocbase),
        EMPTY,
        EMPTY,
        EMPTY,
        arangodb::transaction::Options()
      );
      CHECK((collection->truncate(trx, options).ok()));
    }

    {
      arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;
      arangodb::iresearch::IResearchAnalyzerFeature feature(s.server);

      CHECK((feature.emplace(result, arangodb::StaticStrings::SystemDatabase + "::valid", "identity", "abc").ok()));
      CHECK((result.first));
      CHECK((result.second));
    }

    {
      std::map<std::string, std::pair<irs::string_ref, irs::string_ref>> expected = {
        { arangodb::StaticStrings::SystemDatabase + "::valid", { "identity", "abc" } },
      };
      arangodb::iresearch::IResearchAnalyzerFeature feature(s.server);

      feature.start(); // load persisted analyzers

      feature.visit([&expected](
        arangodb::iresearch::IResearchAnalyzerFeature::AnalyzerPool::ptr const& analyzer
      )->bool {
        if (staticAnalyzers().find(analyzer->name()) != staticAnalyzers().end()) {
          return true; // skip static analyzers
        }

        auto itr = expected.find(analyzer->name());
        CHECK((itr != expected.end()));
        CHECK((itr->second.first == analyzer->type()));
        CHECK((itr->second.second == analyzer->properties()));
        expected.erase(itr);
        return true;
      });
      CHECK((expected.empty()));
    }
  }

  // remove existing records
  {
    {
      std::string collection(ANALYZER_COLLECTION_NAME);
      arangodb::OperationOptions options;
      arangodb::SingleCollectionTransaction trx(
        arangodb::transaction::StandaloneContext::Create(*vocbase),
        collection,
        arangodb::AccessMode::Type::WRITE
      );
      trx.begin();
      trx.truncate(collection, options);
      trx.insert(collection, arangodb::velocypack::Parser::fromJson("{\"name\": \"valid\", \"type\": \"identity\", \"properties\": null}")->slice(), options);
      trx.commit();
    }

    {
      std::map<std::string, std::pair<irs::string_ref, irs::string_ref>> expected = {
        { "identity", { "identity", irs::string_ref::NIL } },
        { arangodb::StaticStrings::SystemDatabase + "::valid", { "identity", irs::string_ref::NIL } },
      };
      arangodb::iresearch::IResearchAnalyzerFeature feature(s.server);

      feature.start(); // load persisted analyzers

      feature.visit([&expected](
        arangodb::iresearch::IResearchAnalyzerFeature::AnalyzerPool::ptr const& analyzer
      )->bool {
        if (analyzer->name() != "identity"
            && staticAnalyzers().find(analyzer->name()) != staticAnalyzers().end()) {
          return true; // skip static analyzers
        }

        auto itr = expected.find(analyzer->name());
        CHECK((itr != expected.end()));
        CHECK((itr->second.first == analyzer->type()));
        CHECK((itr->second.second == analyzer->properties()));
        expected.erase(itr);
        return true;
      });
      CHECK((expected.empty()));
      CHECK((true == feature.remove(arangodb::StaticStrings::SystemDatabase + "::valid").ok()));
      CHECK((false == feature.remove("identity").ok()));
    }

    {
      std::map<std::string, std::pair<irs::string_ref, irs::string_ref>> expected = {};
      arangodb::iresearch::IResearchAnalyzerFeature feature(s.server);

      feature.start(); // load persisted analyzers

      feature.visit([&expected](
        arangodb::iresearch::IResearchAnalyzerFeature::AnalyzerPool::ptr const& analyzer
      )->bool {
        if (staticAnalyzers().find(analyzer->name()) != staticAnalyzers().end()) {
          return true; // skip static analyzers
        }

        auto itr = expected.find(analyzer->name());
        CHECK((itr != expected.end()));
        CHECK((itr->second.first == analyzer->type()));
        CHECK((itr->second.second == analyzer->properties()));
        expected.erase(itr);
        return true;
      });
      CHECK((expected.empty()));
    }
  }

  // emplace on single-server (should persist)
  {
    arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;
    arangodb::iresearch::IResearchAnalyzerFeature feature(s.server);
    CHECK((true == feature.emplace(result, arangodb::StaticStrings::SystemDatabase + "::test_analyzerA", "TestAnalyzer", "abc", {irs::frequency::type()}).ok()));
    CHECK((false == !result.first));
    CHECK((false == !feature.get(arangodb::StaticStrings::SystemDatabase + "::test_analyzerA")));
    CHECK((false == !vocbase->lookupCollection(ANALYZER_COLLECTION_NAME)));
    arangodb::OperationOptions options;
    arangodb::SingleCollectionTransaction trx(
      arangodb::transaction::StandaloneContext::Create(*vocbase),
      ANALYZER_COLLECTION_NAME,
      arangodb::AccessMode::Type::WRITE
    );
    CHECK((trx.begin().ok()));
    auto queryResult = trx.all(ANALYZER_COLLECTION_NAME, 0, 2, options);
    CHECK((true == queryResult.ok()));
    auto slice = arangodb::velocypack::Slice(queryResult.buffer->data());
    CHECK((slice.isArray() && 1 == slice.length()));
    slice = slice.at(0);
    CHECK((slice.isObject()));
    CHECK((slice.hasKey("name") && slice.get("name").isString() && std::string("test_analyzerA") == slice.get("name").copyString()));
    CHECK((slice.hasKey("type") && slice.get("type").isString() && std::string("TestAnalyzer") == slice.get("type").copyString()));
    CHECK((slice.hasKey("properties") && slice.get("properties").isString() && std::string("abc") == slice.get("properties").copyString()));
    CHECK((slice.hasKey("features") && slice.get("features").isArray() && 1 == slice.get("features").length() && slice.get("features").at(0).isString() && std::string("frequency") == slice.get("features").at(0).copyString()));
    CHECK((trx.truncate(ANALYZER_COLLECTION_NAME, options).ok()));
    CHECK((trx.commit().ok()));
  }

  // emplace on coordinator (should persist)
  {
    auto before = arangodb::ServerState::instance()->getRole();
    arangodb::ServerState::instance()->setRole(arangodb::ServerState::ROLE_COORDINATOR);
    auto restore = irs::make_finally([&before]()->void { arangodb::ServerState::instance()->setRole(before); });

    // create a new instance of an ApplicationServer and fill it with the required features
    // cannot use the existing server since its features already have some state
    std::shared_ptr<arangodb::application_features::ApplicationServer> originalServer(
      arangodb::application_features::ApplicationServer::server,
      [](arangodb::application_features::ApplicationServer* ptr)->void {
        arangodb::application_features::ApplicationServer::server = ptr;
      }
    );
    arangodb::application_features::ApplicationServer::server = nullptr; // avoid "ApplicationServer initialized twice"
    arangodb::application_features::ApplicationServer server(nullptr, nullptr);
    arangodb::iresearch::IResearchAnalyzerFeature* feature;
    arangodb::DatabaseFeature* dbFeature;
    arangodb::SystemDatabaseFeature* sysDatabase;
    server.addFeature(new arangodb::ClusterFeature(server)); // required to create ClusterInfo instance
    server.addFeature(dbFeature = new arangodb::DatabaseFeature(server)); // required for IResearchAnalyzerFeature::emplace(...)
    server.addFeature(new arangodb::QueryRegistryFeature(server)); // required for constructing TRI_vocbase_t
    server.addFeature(new arangodb::ShardingFeature(server)); // required for Collections::create(...)
    server.addFeature(sysDatabase = new arangodb::SystemDatabaseFeature(server)); // required for IResearchAnalyzerFeature::start()
    server.addFeature(new arangodb::UpgradeFeature(server, nullptr, {})); // required for upgrade tasks
    server.addFeature(new arangodb::V8DealerFeature(server)); // required for DatabaseFeature::createDatabase(...)
    server.addFeature(new arangodb::application_features::CommunicationFeaturePhase(server)); // required for SimpleHttpClient::doRequest()
    server.addFeature(feature = new arangodb::iresearch::IResearchAnalyzerFeature(server)); // required for running upgrade task

    // create system vocbase (before feature start)
    {
      auto const databases = arangodb::velocypack::Parser::fromJson(std::string("[ { \"name\": \"") + arangodb::StaticStrings::SystemDatabase + "\" } ]");
      CHECK((TRI_ERROR_NO_ERROR == dbFeature->loadDatabases(databases->slice())));
      sysDatabase->start(); // get system database from DatabaseFeature
    }

    auto system = sysDatabase->use();

    server.getFeature<arangodb::ClusterFeature>("Cluster")->prepare(); // create ClusterInfo instance
    server.getFeature<arangodb::ShardingFeature>("Sharding")->prepare(); // required for Collections::create(...), register sharding types
    arangodb::AgencyCommManager::MANAGER->start(); // initialize agency

    ClusterCommMock clusterComm;
    auto scopedClusterComm = ClusterCommMock::setInstance(clusterComm);
    auto* ci = arangodb::ClusterInfo::instance();
    REQUIRE((nullptr != ci));

    // simulate heartbeat thread
    // (create dbserver in current) required by ClusterMethods::persistCollectionInAgency(...)
    // (create collection in current) required by ClusterMethods::persistCollectionInAgency(...)
    // (create dummy collection in plan to fill ClusterInfo::_shardServers) required by ClusterMethods::persistCollectionInAgency(...)
    {
      auto const srvPath = "/Current/DBServers";
      auto const srvValue = arangodb::velocypack::Parser::fromJson("{ \"dbserver-key-does-not-matter\": \"dbserver-value-does-not-matter\" }");
      CHECK(arangodb::AgencyComm().setValue(srvPath, srvValue->slice(), 0.0).successful());
      auto const colPath = "/Current/Collections/_system/2"; // '2' must match what ClusterInfo generates for LogicalCollection::id() or collection creation request will never get executed (use 'collectionID' from ClusterInfo::createCollectionCoordinator(...) in stack trace)
      auto const colValue = arangodb::velocypack::Parser::fromJson("{ \"same-as-dummy-shard-id\": { \"servers\": [ \"same-as-dummy-shard-server\" ] } }");
      CHECK(arangodb::AgencyComm().setValue(colPath, colValue->slice(), 0.0).successful());
      auto const dummyPath = "/Plan/Collections";
      auto const dummyValue = arangodb::velocypack::Parser::fromJson("{ \"_system\": { \"collection-id-does-not-matter\": { \"name\": \"dummy\", \"shards\": { \"same-as-dummy-shard-id\": [ \"same-as-dummy-shard-server\" ] } } } }");
      CHECK(arangodb::AgencyComm().setValue(dummyPath, dummyValue->slice(), 0.0).successful());
    }

    // insert response for expected extra analyzer
    {
      arangodb::ClusterCommResult response;
      response.operationID = 1; // sequential non-zero value
      response.status = arangodb::ClusterCommOpStatus::CL_COMM_RECEIVED;
      response.answer_code = arangodb::rest::ResponseCode::CREATED;
      response.answer = std::make_shared<GeneralRequestMock>(*vocbase);
      static_cast<GeneralRequestMock*>(response.answer.get())->_payload = *arangodb::velocypack::Parser::fromJson(std::string("{ \"_key\": \"") + std::to_string(response.operationID) + "\" }"); // unique arbitrary key
      clusterComm._responses.emplace_back(std::move(response));
    }

    arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;
    CHECK((true == feature->emplace(result, arangodb::StaticStrings::SystemDatabase + "::test_analyzerB", "TestAnalyzer", "abc").ok()));
    CHECK((nullptr != ci->getCollection(system->name(), ANALYZER_COLLECTION_NAME)));
    CHECK((1 == clusterComm._requests.size()));
    auto& entry = *(clusterComm._requests.begin());
    CHECK((entry._body));
    auto body = arangodb::velocypack::Parser::fromJson(*(entry._body));
    auto slice = body->slice();
    CHECK((slice.isObject()));
    CHECK((slice.get("name").isString()));
    CHECK((std::string("test_analyzerB") == slice.get("name").copyString()));
  }

  // emplace on db-server(should not persist)
  {
    auto before = arangodb::ServerState::instance()->getRole();
    arangodb::ServerState::instance()->setRole(arangodb::ServerState::ROLE_DBSERVER);
    auto restore = irs::make_finally([&before]()->void { arangodb::ServerState::instance()->setRole(before); });

    // create a new instance of an ApplicationServer and fill it with the required features
    // cannot use the existing server since its features already have some state
    std::shared_ptr<arangodb::application_features::ApplicationServer> originalServer(
      arangodb::application_features::ApplicationServer::server,
      [](arangodb::application_features::ApplicationServer* ptr)->void {
        arangodb::application_features::ApplicationServer::server = ptr;
      }
    );
    arangodb::application_features::ApplicationServer::server = nullptr; // avoid "ApplicationServer initialized twice"
    arangodb::application_features::ApplicationServer server(nullptr, nullptr);
    arangodb::iresearch::IResearchAnalyzerFeature* feature;
    arangodb::DatabaseFeature* dbFeature;
    arangodb::SystemDatabaseFeature* sysDatabase;
    server.addFeature(new arangodb::ClusterFeature(server)); // required to create ClusterInfo instance
    server.addFeature(dbFeature = new arangodb::DatabaseFeature(server)); // required for IResearchAnalyzerFeature::emplace(...)
    server.addFeature(new arangodb::QueryRegistryFeature(server)); // required for constructing TRI_vocbase_t
    server.addFeature(new arangodb::ShardingFeature(server)); // required for Collections::create(...)
    server.addFeature(sysDatabase = new arangodb::SystemDatabaseFeature(server)); // required for IResearchAnalyzerFeature::start()
    server.addFeature(new arangodb::UpgradeFeature(server, nullptr, {})); // required for upgrade tasks
    server.addFeature(new arangodb::V8DealerFeature(server)); // required for DatabaseFeature::createDatabase(...)
    server.addFeature(new arangodb::application_features::CommunicationFeaturePhase(server)); // required for SimpleHttpClient::doRequest()
    server.addFeature(feature = new arangodb::iresearch::IResearchAnalyzerFeature(server)); // required for running upgrade task

    // create system vocbase (before feature start)
    {
      auto const databases = arangodb::velocypack::Parser::fromJson(std::string("[ { \"name\": \"") + arangodb::StaticStrings::SystemDatabase + "\" } ]");
      CHECK((TRI_ERROR_NO_ERROR == dbFeature->loadDatabases(databases->slice())));
      sysDatabase->start(); // get system database from DatabaseFeature
    }

    auto system = sysDatabase->use();

    server.getFeature<arangodb::ClusterFeature>("Cluster")->prepare(); // create ClusterInfo instance
    server.getFeature<arangodb::ShardingFeature>("Sharding")->prepare(); // required for Collections::create(...), register sharding types
    arangodb::AgencyCommManager::MANAGER->start(); // initialize agency

    ClusterCommMock clusterComm;
    auto scopedClusterComm = ClusterCommMock::setInstance(clusterComm);
    auto* ci = arangodb::ClusterInfo::instance();
    REQUIRE((nullptr != ci));

    // simulate heartbeat thread
    // (create dbserver in current) required by ClusterMethods::persistCollectionInAgency(...)
    // (create collection in current) required by ClusterMethods::persistCollectionInAgency(...)
    // (create dummy collection in plan to fill ClusterInfo::_shardServers) required by ClusterMethods::persistCollectionInAgency(...)
    {
      auto const srvPath = "/Current/DBServers";
      auto const srvValue = arangodb::velocypack::Parser::fromJson("{ \"dbserver-key-does-not-matter\": \"dbserver-value-does-not-matter\" }");
      CHECK(arangodb::AgencyComm().setValue(srvPath, srvValue->slice(), 0.0).successful());
      auto const dummyPath = "/Plan/Collections";
      auto const dummyValue = arangodb::velocypack::Parser::fromJson("{ \"_system\": { \"collection-id-does-not-matter\": { \"name\": \"dummy\", \"shards\": { \"same-as-dummy-shard-id\": [ \"same-as-dummy-shard-server\" ] } } } }");
      CHECK(arangodb::AgencyComm().setValue(dummyPath, dummyValue->slice(), 0.0).successful());
    }

    arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;
    CHECK((true == feature->emplace(result, arangodb::StaticStrings::SystemDatabase + "::test_analyzerC", "TestAnalyzer", "abc").ok()));
    CHECK_THROWS((ci->getCollection(system->name(), ANALYZER_COLLECTION_NAME))); // throws on missing collection, not ClusterInfo persisted
    CHECK((true == !system->lookupCollection(ANALYZER_COLLECTION_NAME))); // not locally persisted
  }
}

SECTION("test_remove") {
  auto* dbFeature = arangodb::application_features::ApplicationServer::lookupFeature<
    arangodb::DatabaseFeature
  >("Database");
  REQUIRE((false == !dbFeature));
  arangodb::AqlFeature aqlFeature(s.server);
  aqlFeature.start(); // required for Query::Query(...), must not call ~AqlFeature() for the duration of the test

  // remove existing
  {
    arangodb::iresearch::IResearchAnalyzerFeature feature(s.server);

    // add analyzer
    {
      arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;
      REQUIRE((true == feature.emplace(result, arangodb::StaticStrings::SystemDatabase + "::test_analyzer0", "TestAnalyzer", "abc").ok()));
      REQUIRE((false == !feature.get(arangodb::StaticStrings::SystemDatabase + "::test_analyzer0")));
    }

    CHECK((true == feature.remove(arangodb::StaticStrings::SystemDatabase + "::test_analyzer0").ok()));
    CHECK((true == !feature.get(arangodb::StaticStrings::SystemDatabase + "::test_analyzer0")));
  }

  // remove existing (inRecovery) single-server
  {
    arangodb::iresearch::IResearchAnalyzerFeature feature(s.server);

    // add analyzer
    {
      arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;
      REQUIRE((true == feature.emplace(result, arangodb::StaticStrings::SystemDatabase + "::test_analyzer0", "TestAnalyzer", "abc").ok()));
      REQUIRE((false == !feature.get(arangodb::StaticStrings::SystemDatabase + "::test_analyzer0")));
    }

    auto before = StorageEngineMock::inRecoveryResult;
    StorageEngineMock::inRecoveryResult = true;
    auto restore = irs::make_finally([&before]()->void { StorageEngineMock::inRecoveryResult = before; });

    CHECK((false == feature.remove(arangodb::StaticStrings::SystemDatabase + "::test_analyzer0").ok()));
    CHECK((false == !feature.get(arangodb::StaticStrings::SystemDatabase + "::test_analyzer0")));
  }

  // remove existing (coordinator)
  {
    auto beforeRole = arangodb::ServerState::instance()->getRole();
    arangodb::ServerState::instance()->setRole(arangodb::ServerState::ROLE_COORDINATOR);
    auto restoreRole = irs::make_finally([&beforeRole]()->void { arangodb::ServerState::instance()->setRole(beforeRole); });

    // create a new instance of an ApplicationServer and fill it with the required features
    // cannot use the existing server since its features already have some state
    std::shared_ptr<arangodb::application_features::ApplicationServer> originalServer(
      arangodb::application_features::ApplicationServer::server,
      [](arangodb::application_features::ApplicationServer* ptr)->void {
        arangodb::application_features::ApplicationServer::server = ptr;
      }
    );
    arangodb::application_features::ApplicationServer::server = nullptr; // avoid "ApplicationServer initialized twice"
    arangodb::application_features::ApplicationServer server(nullptr, nullptr);
    arangodb::iresearch::IResearchAnalyzerFeature* feature;
    arangodb::DatabaseFeature* dbFeature;
    arangodb::SystemDatabaseFeature* sysDatabase;
    server.addFeature(new arangodb::ClusterFeature(server)); // required to create ClusterInfo instance
    server.addFeature(dbFeature = new arangodb::DatabaseFeature(server)); // required for IResearchAnalyzerFeature::emplace(...)
    server.addFeature(new arangodb::QueryRegistryFeature(server)); // required for constructing TRI_vocbase_t
    server.addFeature(new arangodb::ShardingFeature(server)); // required for Collections::create(...)
    server.addFeature(sysDatabase = new arangodb::SystemDatabaseFeature(server)); // required for IResearchAnalyzerFeature::start()
    server.addFeature(new arangodb::V8DealerFeature(server)); // required for DatabaseFeature::createDatabase(...)
    server.addFeature(new arangodb::application_features::CommunicationFeaturePhase(server)); // required for SimpleHttpClient::doRequest()
    server.addFeature(feature = new arangodb::iresearch::IResearchAnalyzerFeature(server)); // required for running upgrade task
    arangodb::aql::OptimizerRulesFeature(server).prepare(); // required for Query::preparePlan(...)
    auto clearOptimizerRules = irs::make_finally([&s]()->void { arangodb::aql::OptimizerRulesFeature(s.server).unprepare(); });

    // create system vocbase (before feature start)
    {
      auto const databases = arangodb::velocypack::Parser::fromJson(std::string("[ { \"name\": \"") + arangodb::StaticStrings::SystemDatabase + "\" } ]");
      CHECK((TRI_ERROR_NO_ERROR == dbFeature->loadDatabases(databases->slice())));
      sysDatabase->start(); // get system database from DatabaseFeature
    }

    server.getFeature<arangodb::ClusterFeature>("Cluster")->prepare(); //  create ClusterInfo instance, required or AgencyCallbackRegistry::registerCallback(...) will hang
    server.getFeature<arangodb::ShardingFeature>("Sharding")->prepare(); // required for Collections::create(...), register sharding types
    arangodb::AgencyCommManager::MANAGER->start(); // initialize agency or requests to agency will return invalid values (e.g. '_id' generation)

    ClusterCommMock clusterComm;
    auto scopedClusterComm = ClusterCommMock::setInstance(clusterComm); // or get SIGFPE in ClusterComm::communicator() while call to ClusterInfo::createDocumentOnCoordinator(...)

    // simulate heartbeat thread
    // (create dbserver in current) required by ClusterMethods::persistCollectionInAgency(...)
    // (create collection in current) required by ClusterMethods::persistCollectionInAgency(...)
    // (create dummy collection in plan to fill ClusterInfo::_shardServers) required by ClusterMethods::persistCollectionInAgency(...)
    {
      auto const srvPath = "/Current/DBServers";
      auto const srvValue = arangodb::velocypack::Parser::fromJson("{ \"dbserver-key-does-not-matter\": \"dbserver-value-does-not-matter\" }");
      CHECK(arangodb::AgencyComm().setValue(srvPath, srvValue->slice(), 0.0).successful());
      auto const failedPath = "/Target/FailedServers";
      auto const failedValue = arangodb::velocypack::Parser::fromJson("{ }"); // empty object or ClusterInfo::loadCurrentDBServers() will fail
      CHECK(arangodb::AgencyComm().setValue(failedPath, failedValue->slice(), 0.0).successful());
      auto const dbPath = "/Plan/Databases/_system";
      auto const dbValue = arangodb::velocypack::Parser::fromJson("null"); // value does not matter
      CHECK(arangodb::AgencyComm().setValue(dbPath, dbValue->slice(), 0.0).successful());
      auto const colPath = "/Current/Collections/_system/2"; // '2' must match what ClusterInfo generates for LogicalCollection::id() or collection creation request will never get executed (use 'collectionID' from ClusterInfo::createCollectionCoordinator(...) in stack trace)
      auto const colValue = arangodb::velocypack::Parser::fromJson("{ \"same-as-dummy-shard-id\": { \"servers\": [ \"same-as-dummy-shard-server\" ] } }");
      CHECK(arangodb::AgencyComm().setValue(colPath, colValue->slice(), 0.0).successful());
      auto const dummyPath = "/Plan/Collections";
      auto const dummyValue = arangodb::velocypack::Parser::fromJson("{ \"_system\": { \"collection-id-does-not-matter\": { \"name\": \"dummy\", \"shards\": { \"same-as-dummy-shard-id\": [ \"same-as-dummy-shard-server\" ] } } } }");
      CHECK(arangodb::AgencyComm().setValue(dummyPath, dummyValue->slice(), 0.0).successful());
    }

    // insert response for expected extra analyzer (insertion)
    {
      arangodb::ClusterCommResult response;
      response.operationID = 1; // sequential non-zero value
      response.status = arangodb::ClusterCommOpStatus::CL_COMM_RECEIVED;
      response.answer_code = arangodb::rest::ResponseCode::CREATED;
      response.answer = std::make_shared<GeneralRequestMock>(*(sysDatabase->use()));
      static_cast<GeneralRequestMock*>(response.answer.get())->_payload = *arangodb::velocypack::Parser::fromJson(std::string("{ \"_key\": \"") + std::to_string(response.operationID) + "\" }"); // unique arbitrary key
      clusterComm._responses.emplace_back(std::move(response));
    }

    // insert response for expected extra analyzer (removal)
    {
      arangodb::ClusterCommResult response;
      response.operationID = 2; // sequential non-zero value
      response.status = arangodb::ClusterCommOpStatus::CL_COMM_RECEIVED;
      response.answer_code = arangodb::rest::ResponseCode::OK;
      response.answer = std::make_shared<GeneralRequestMock>(*(sysDatabase->use()));
      clusterComm._responses.emplace_back(std::move(response));
    }

    // add analyzer
    {
      arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;
      REQUIRE((true == feature->emplace(result, arangodb::StaticStrings::SystemDatabase + "::test_analyzer1", "TestAnalyzer", "abc").ok()));
      REQUIRE((false == !feature->get(arangodb::StaticStrings::SystemDatabase + "::test_analyzer1")));
    }

    CHECK((true == feature->remove(arangodb::StaticStrings::SystemDatabase + "::test_analyzer1").ok()));
    CHECK((true == !feature->get(arangodb::StaticStrings::SystemDatabase + "::test_analyzer1")));
  }

  // remove existing (inRecovery) coordinator
  {
    auto beforeRole = arangodb::ServerState::instance()->getRole();
    arangodb::ServerState::instance()->setRole(arangodb::ServerState::ROLE_COORDINATOR);
    auto restoreRole = irs::make_finally([&beforeRole]()->void { arangodb::ServerState::instance()->setRole(beforeRole); });

    // create a new instance of an ApplicationServer and fill it with the required features
    // cannot use the existing server since its features already have some state
    std::shared_ptr<arangodb::application_features::ApplicationServer> originalServer(
      arangodb::application_features::ApplicationServer::server,
      [](arangodb::application_features::ApplicationServer* ptr)->void {
        arangodb::application_features::ApplicationServer::server = ptr;
      }
    );
    arangodb::application_features::ApplicationServer::server = nullptr; // avoid "ApplicationServer initialized twice"
    arangodb::application_features::ApplicationServer server(nullptr, nullptr);
    arangodb::iresearch::IResearchAnalyzerFeature* feature;
    arangodb::DatabaseFeature* dbFeature;
    arangodb::SystemDatabaseFeature* sysDatabase;
    server.addFeature(new arangodb::ClusterFeature(server)); // required to create ClusterInfo instance
    server.addFeature(dbFeature = new arangodb::DatabaseFeature(server)); // required for IResearchAnalyzerFeature::emplace(...)
    server.addFeature(new arangodb::QueryRegistryFeature(server)); // required for constructing TRI_vocbase_t
    server.addFeature(new arangodb::ShardingFeature(server)); // required for Collections::create(...)
    server.addFeature(sysDatabase = new arangodb::SystemDatabaseFeature(server)); // required for IResearchAnalyzerFeature::start()
    server.addFeature(new arangodb::V8DealerFeature(server)); // required for DatabaseFeature::createDatabase(...)
    server.addFeature(new arangodb::application_features::CommunicationFeaturePhase(server)); // required for SimpleHttpClient::doRequest()
    server.addFeature(feature = new arangodb::iresearch::IResearchAnalyzerFeature(server)); // required for running upgrade task
    arangodb::aql::OptimizerRulesFeature(server).prepare(); // required for Query::preparePlan(...)
    auto clearOptimizerRules = irs::make_finally([&s]()->void { arangodb::aql::OptimizerRulesFeature(s.server).unprepare(); });

    // create system vocbase (before feature start)
    {
      auto const databases = arangodb::velocypack::Parser::fromJson(std::string("[ { \"name\": \"") + arangodb::StaticStrings::SystemDatabase + "\" } ]");
      CHECK((TRI_ERROR_NO_ERROR == dbFeature->loadDatabases(databases->slice())));
      sysDatabase->start(); // get system database from DatabaseFeature
    }

    server.getFeature<arangodb::ClusterFeature>("Cluster")->prepare(); //  create ClusterInfo instance, required or AgencyCallbackRegistry::registerCallback(...) will hang
    server.getFeature<arangodb::ShardingFeature>("Sharding")->prepare(); // required for Collections::create(...), register sharding types
    arangodb::AgencyCommManager::MANAGER->start(); // initialize agency or requests to agency will return invalid values (e.g. '_id' generation)

    ClusterCommMock clusterComm;
    auto scopedClusterComm = ClusterCommMock::setInstance(clusterComm); // or get SIGFPE in ClusterComm::communicator() while call to ClusterInfo::createDocumentOnCoordinator(...)

    // simulate heartbeat thread
    // (create dbserver in current) required by ClusterMethods::persistCollectionInAgency(...)
    // (create collection in current) required by ClusterMethods::persistCollectionInAgency(...)
    // (create dummy collection in plan to fill ClusterInfo::_shardServers) required by ClusterMethods::persistCollectionInAgency(...)
    {
      auto const srvPath = "/Current/DBServers";
      auto const srvValue = arangodb::velocypack::Parser::fromJson("{ \"dbserver-key-does-not-matter\": \"dbserver-value-does-not-matter\" }");
      CHECK(arangodb::AgencyComm().setValue(srvPath, srvValue->slice(), 0.0).successful());
      auto const failedPath = "/Target/FailedServers";
      auto const failedValue = arangodb::velocypack::Parser::fromJson("{ }"); // empty object or ClusterInfo::loadCurrentDBServers() will fail
      CHECK(arangodb::AgencyComm().setValue(failedPath, failedValue->slice(), 0.0).successful());
      auto const dbPath = "/Plan/Databases/_system";
      auto const dbValue = arangodb::velocypack::Parser::fromJson("null"); // value does not matter
      CHECK(arangodb::AgencyComm().setValue(dbPath, dbValue->slice(), 0.0).successful());
      auto const colPath = "/Current/Collections/_system/1000002"; // '1000002' must match what ClusterInfo generates for LogicalCollection::id() or collection creation request will never get executed (use 'collectionID' from ClusterInfo::createCollectionCoordinator(...) in stack trace)
      auto const colValue = arangodb::velocypack::Parser::fromJson("{ \"s1000003\": { \"servers\": [ \"same-as-dummy-shard-server\" ] } }"); // 's1000003' mustmatch what ClusterInfo generates for LogicalCollection::getShardList(...) or EngineInfoContainerDBServer::createDBServerMapping(...) will not find shard
      CHECK(arangodb::AgencyComm().setValue(colPath, colValue->slice(), 0.0).successful());
      auto const dummyPath = "/Plan/Collections";
      auto const dummyValue = arangodb::velocypack::Parser::fromJson("{ \"_system\": { \"collection-id-does-not-matter\": { \"name\": \"dummy\", \"shards\": { \"s1000003\": [ \"same-as-dummy-shard-server\" ] } } } }"); // 's1000003' same as for collection above
      CHECK(arangodb::AgencyComm().setValue(dummyPath, dummyValue->slice(), 0.0).successful());
      auto const dbSrvPath = "/Current/ServersRegistered";
      auto const dbSrvValue = arangodb::velocypack::Parser::fromJson("{ \"same-as-dummy-shard-server\": { \"endpoint\": \"endpoint-does-not-matter\" } }");
      CHECK(arangodb::AgencyComm().setValue(dbSrvPath, dbSrvValue->slice(), 0.0).successful());
    }

    // insert response for expected extra analyzer
    {
      arangodb::ClusterCommResult response;
      response.operationID = 1; // sequential non-zero value
      response.status = arangodb::ClusterCommOpStatus::CL_COMM_RECEIVED;
      response.answer_code = arangodb::rest::ResponseCode::CREATED;
      response.answer = std::make_shared<GeneralRequestMock>(*(sysDatabase->use()));
      static_cast<GeneralRequestMock*>(response.answer.get())->_payload = *arangodb::velocypack::Parser::fromJson(std::string("{ \"_key\": \"") + std::to_string(response.operationID) + "\" }"); // unique arbitrary key
      clusterComm._responses.emplace_back(std::move(response));
    }

    // add analyzer
    {
      arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;
      REQUIRE((true == feature->emplace(result, arangodb::StaticStrings::SystemDatabase + "::test_analyzer1", "TestAnalyzer", "abc").ok()));
      REQUIRE((false == !feature->get(arangodb::StaticStrings::SystemDatabase + "::test_analyzer1")));
    }

    // insert response for expected analyzer lookup
    {
      arangodb::ClusterCommResult response;
      response.status = arangodb::ClusterCommOpStatus::CL_COMM_RECEIVED;
      response.result = std::make_shared<arangodb::httpclient::SimpleHttpResult>();
      response.result->getBody().appendText("{ \"result\": { \"snippets\": { \"6:shard-id-does-not-matter\": \"value-does-not-matter\" } } }").ensureNullTerminated(); // '6' must match GATHER Node id in ExecutionEngine::createBlocks(...)
      clusterComm._responses.emplace_back(std::move(response));
    }

    // insert response for expected analyzer reload from collection
    {
      arangodb::ClusterCommResult response;
      response.status = arangodb::ClusterCommOpStatus::CL_COMM_SENT;
      response.result = std::make_shared<arangodb::httpclient::SimpleHttpResult>();
      response.result->getBody().appendText("{ \"done\": true, \"nrItems\": 1, \"nrRegs\": 1, \"data\": [ 1 ], \"raw\": [ null, null, { \"_key\": \"key-does-not-matter\", \"name\": \"test_analyzer1\", \"type\": \"TestAnalyzer\", \"properties\": \"abc\" } ] }").ensureNullTerminated(); // 'data' value must be 1 as per AqlItemBlock::AqlItemBlock(...), first 2 'raw' values ignored, 'nrRegs' must be 1 or assertion failure in ExecutionBlockImpl<Executor>::requestWrappedBlock(...)
      clusterComm._responses.emplace_back(std::move(response));
    }

    auto before = StorageEngineMock::inRecoveryResult;
    StorageEngineMock::inRecoveryResult = true;
    auto restore = irs::make_finally([&before]()->void { StorageEngineMock::inRecoveryResult = before; });

    CHECK((false == feature->remove(arangodb::StaticStrings::SystemDatabase + "::test_analyzer1").ok()));
    CHECK((false == !feature->get(arangodb::StaticStrings::SystemDatabase + "::test_analyzer1")));
  }

  // remove existing (dbserver)
  {
    auto beforeRole = arangodb::ServerState::instance()->getRole();
    arangodb::ServerState::instance()->setRole(arangodb::ServerState::ROLE_DBSERVER);
    auto restoreRole = irs::make_finally([&beforeRole]()->void { arangodb::ServerState::instance()->setRole(beforeRole); });

    // create a new instance of an ApplicationServer and fill it with the required features
    // cannot use the existing server since its features already have some state
    std::shared_ptr<arangodb::application_features::ApplicationServer> originalServer(
      arangodb::application_features::ApplicationServer::server,
      [](arangodb::application_features::ApplicationServer* ptr)->void {
        arangodb::application_features::ApplicationServer::server = ptr;
      }
    );
    arangodb::application_features::ApplicationServer::server = nullptr; // avoid "ApplicationServer initialized twice"
    arangodb::application_features::ApplicationServer server(nullptr, nullptr);
    arangodb::iresearch::IResearchAnalyzerFeature* feature;
    arangodb::DatabaseFeature* dbFeature;
    arangodb::SystemDatabaseFeature* sysDatabase;
    server.addFeature(new arangodb::ClusterFeature(server)); // required to create ClusterInfo instance
    server.addFeature(dbFeature = new arangodb::DatabaseFeature(server)); // required for IResearchAnalyzerFeature::emplace(...)
    server.addFeature(new arangodb::QueryRegistryFeature(server)); // required for constructing TRI_vocbase_t
    server.addFeature(new arangodb::ShardingFeature(server)); // required for Collections::create(...)
    server.addFeature(sysDatabase = new arangodb::SystemDatabaseFeature(server)); // required for IResearchAnalyzerFeature::start()
    server.addFeature(new arangodb::V8DealerFeature(server)); // required for DatabaseFeature::createDatabase(...)
    server.addFeature(new arangodb::application_features::CommunicationFeaturePhase(server)); // required for SimpleHttpClient::doRequest()
    server.addFeature(feature = new arangodb::iresearch::IResearchAnalyzerFeature(server)); // required for running upgrade task
    arangodb::aql::OptimizerRulesFeature(server).prepare(); // required for Query::preparePlan(...)
    auto clearOptimizerRules = irs::make_finally([&s]()->void { arangodb::aql::OptimizerRulesFeature(s.server).unprepare(); });

    // create system vocbase (before feature start)
    {
      auto const databases = arangodb::velocypack::Parser::fromJson(std::string("[ { \"name\": \"") + arangodb::StaticStrings::SystemDatabase + "\" } ]");
      CHECK((TRI_ERROR_NO_ERROR == dbFeature->loadDatabases(databases->slice())));
      sysDatabase->start(); // get system database from DatabaseFeature
    }

    ClusterCommMock clusterComm;
    auto scopedClusterComm = ClusterCommMock::setInstance(clusterComm); // or get SIGFPE in ClusterComm::communicator() while call to ClusterInfo::createDocumentOnCoordinator(...)

    // insert response for expected empty initial analyzer list
    {
      arangodb::ClusterCommResult response;
      response.operationID = 1; // sequential non-zero value
      response.status = arangodb::ClusterCommOpStatus::CL_COMM_RECEIVED;
      response.answer_code = arangodb::rest::ResponseCode::CREATED;
      response.answer = std::make_shared<GeneralRequestMock>(*(sysDatabase->use()));
      static_cast<GeneralRequestMock*>(response.answer.get())->_payload = *arangodb::velocypack::Parser::fromJson("{ \"result\": [] }"); // empty initial result
      clusterComm._responses.emplace_back(std::move(response));
    }

    // add analyzer
    {
      arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;
      REQUIRE((true == !feature->get(arangodb::StaticStrings::SystemDatabase + "::test_analyzer2")));
      REQUIRE((true == feature->emplace(result, arangodb::StaticStrings::SystemDatabase + "::test_analyzer2", "TestAnalyzer", "abc").ok()));
      REQUIRE((false == !feature->get(arangodb::StaticStrings::SystemDatabase + "::test_analyzer2")));
    }

    CHECK((true == feature->remove(arangodb::StaticStrings::SystemDatabase + "::test_analyzer2").ok()));
    CHECK((true == !feature->get(arangodb::StaticStrings::SystemDatabase + "::test_analyzer2")));
  }

  // remove existing (inRecovery) dbserver
  {
    auto beforeRole = arangodb::ServerState::instance()->getRole();
    arangodb::ServerState::instance()->setRole(arangodb::ServerState::ROLE_DBSERVER);
    auto restoreRole = irs::make_finally([&beforeRole]()->void { arangodb::ServerState::instance()->setRole(beforeRole); });

    // create a new instance of an ApplicationServer and fill it with the required features
    // cannot use the existing server since its features already have some state
    std::shared_ptr<arangodb::application_features::ApplicationServer> originalServer(
      arangodb::application_features::ApplicationServer::server,
      [](arangodb::application_features::ApplicationServer* ptr)->void {
        arangodb::application_features::ApplicationServer::server = ptr;
      }
    );
    arangodb::application_features::ApplicationServer::server = nullptr; // avoid "ApplicationServer initialized twice"
    arangodb::application_features::ApplicationServer server(nullptr, nullptr);
    arangodb::iresearch::IResearchAnalyzerFeature* feature;
    arangodb::DatabaseFeature* dbFeature;
    arangodb::SystemDatabaseFeature* sysDatabase;
    server.addFeature(new arangodb::ClusterFeature(server)); // required to create ClusterInfo instance
    server.addFeature(dbFeature = new arangodb::DatabaseFeature(server)); // required for IResearchAnalyzerFeature::emplace(...)
    server.addFeature(new arangodb::QueryRegistryFeature(server)); // required for constructing TRI_vocbase_t
    server.addFeature(new arangodb::ShardingFeature(server)); // required for Collections::create(...)
    server.addFeature(sysDatabase = new arangodb::SystemDatabaseFeature(server)); // required for IResearchAnalyzerFeature::start()
    server.addFeature(new arangodb::V8DealerFeature(server)); // required for DatabaseFeature::createDatabase(...)
    server.addFeature(new arangodb::application_features::CommunicationFeaturePhase(server)); // required for SimpleHttpClient::doRequest()
    server.addFeature(feature = new arangodb::iresearch::IResearchAnalyzerFeature(server)); // required for running upgrade task
    arangodb::aql::OptimizerRulesFeature(server).prepare(); // required for Query::preparePlan(...)
    auto clearOptimizerRules = irs::make_finally([&s]()->void { arangodb::aql::OptimizerRulesFeature(s.server).unprepare(); });

    // create system vocbase (before feature start)
    {
      auto const databases = arangodb::velocypack::Parser::fromJson(std::string("[ { \"name\": \"") + arangodb::StaticStrings::SystemDatabase + "\" } ]");
      CHECK((TRI_ERROR_NO_ERROR == dbFeature->loadDatabases(databases->slice())));
      sysDatabase->start(); // get system database from DatabaseFeature
    }

    ClusterCommMock clusterComm;
    auto scopedClusterComm = ClusterCommMock::setInstance(clusterComm); // or get SIGFPE in ClusterComm::communicator() while call to ClusterInfo::createDocumentOnCoordinator(...)

    // insert response for expected empty initial analyzer list
    {
      arangodb::ClusterCommResult response;
      response.operationID = 1; // sequential non-zero value
      response.status = arangodb::ClusterCommOpStatus::CL_COMM_RECEIVED;
      response.answer_code = arangodb::rest::ResponseCode::CREATED;
      response.answer = std::make_shared<GeneralRequestMock>(*(sysDatabase->use()));
      static_cast<GeneralRequestMock*>(response.answer.get())->_payload = *arangodb::velocypack::Parser::fromJson("{ \"result\": [] }"); // empty initial result
      clusterComm._responses.emplace_back(std::move(response));
    }

    // add analyzer
    {
      arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;
      REQUIRE((true == !feature->get(arangodb::StaticStrings::SystemDatabase + "::test_analyzer2")));
      REQUIRE((true == feature->emplace(result, arangodb::StaticStrings::SystemDatabase + "::test_analyzer2", "TestAnalyzer", "abc").ok()));
      REQUIRE((false == !feature->get(arangodb::StaticStrings::SystemDatabase + "::test_analyzer2")));
    }

    auto before = StorageEngineMock::inRecoveryResult;
    StorageEngineMock::inRecoveryResult = true;
    auto restore = irs::make_finally([&before]()->void { StorageEngineMock::inRecoveryResult = before; });

    CHECK((true == feature->remove(arangodb::StaticStrings::SystemDatabase + "::test_analyzer2").ok()));
    CHECK((true == !feature->get(arangodb::StaticStrings::SystemDatabase + "::test_analyzer2")));
  }

  // remove existing (in-use)
  {
    arangodb::iresearch::IResearchAnalyzerFeature feature(s.server);
    arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result; // will keep reference
    REQUIRE((true == feature.emplace(result, arangodb::StaticStrings::SystemDatabase + "::test_analyzer3", "TestAnalyzer", "abc").ok()));
    REQUIRE((false == !feature.get(arangodb::StaticStrings::SystemDatabase + "::test_analyzer3")));

    CHECK((false == feature.remove(arangodb::StaticStrings::SystemDatabase + "::test_analyzer3", false).ok()));
    CHECK((false == !feature.get(arangodb::StaticStrings::SystemDatabase + "::test_analyzer3")));
    CHECK((true == feature.remove(arangodb::StaticStrings::SystemDatabase + "::test_analyzer3", true).ok()));
    CHECK((true == !feature.get(arangodb::StaticStrings::SystemDatabase + "::test_analyzer3")));
  }

  // remove missing (no vocbase)
  {
    arangodb::iresearch::IResearchAnalyzerFeature feature(s.server);
    REQUIRE((nullptr == dbFeature->lookupDatabase("testVocbase")));

    CHECK((true == !feature.get("testVocbase::test_analyzer")));
    CHECK((false == feature.remove("testVocbase::test_analyzer").ok()));
  }

  // remove missing (no collection)
  {
    arangodb::iresearch::IResearchAnalyzerFeature feature(s.server);
    TRI_vocbase_t* vocbase;
    REQUIRE((TRI_ERROR_NO_ERROR == dbFeature->createDatabase(1, "testVocbase", vocbase)));
    REQUIRE((nullptr != dbFeature->lookupDatabase("testVocbase")));

    CHECK((true == !feature.get("testVocbase::test_analyzer")));
    CHECK((false == feature.remove("testVocbase::test_analyzer").ok()));
  }

  // remove invalid
  {
    arangodb::iresearch::IResearchAnalyzerFeature feature(s.server);
    CHECK((true == !feature.get(arangodb::StaticStrings::SystemDatabase + "::test_analyzer")));
    CHECK((false == feature.remove(arangodb::StaticStrings::SystemDatabase + "::test_analyzer").ok()));
  }

  // remove static analyzer
  {
    arangodb::iresearch::IResearchAnalyzerFeature feature(s.server);
    CHECK((false == !feature.get("identity")));
    CHECK((false == feature.remove("identity").ok()));
    CHECK((false == !feature.get("identity")));
  }
}

SECTION("test_start") {
  auto* database = arangodb::application_features::ApplicationServer::lookupFeature<
    arangodb::SystemDatabaseFeature
  >();
  auto vocbase = database->use();

  // test feature start load configuration (inRecovery, no configuration collection)
  {
    // ensure no configuration collection
    {
      auto collection = vocbase->lookupCollection(ANALYZER_COLLECTION_NAME);

      if (collection) {
        vocbase->dropCollection(collection->id(), true, -1);
      }

      collection = vocbase->lookupCollection(ANALYZER_COLLECTION_NAME);
      CHECK((nullptr == collection));
    }

    auto before = StorageEngineMock::inRecoveryResult;
    StorageEngineMock::inRecoveryResult = true;
    auto restore = irs::make_finally([&before]()->void { StorageEngineMock::inRecoveryResult = before; });
    arangodb::iresearch::IResearchAnalyzerFeature feature(s.server);
    feature.start(); // load persisted analyzers
    CHECK((nullptr == vocbase->lookupCollection(ANALYZER_COLLECTION_NAME)));

    auto expected = staticAnalyzers();

    feature.visit([&expected, &feature](
      arangodb::iresearch::IResearchAnalyzerFeature::AnalyzerPool::ptr const& analyzer
    )->bool {
      auto itr = expected.find(analyzer->name());
      CHECK((itr != expected.end()));
      CHECK((itr->second.type == analyzer->type()));
      CHECK((itr->second.properties == analyzer->properties()));
      CHECK((itr->second.features.is_subset_of(feature.get(analyzer->name())->features())));
      expected.erase(itr);
      return true;
    });
    CHECK((expected.empty()));
  }

  // test feature start load configuration (inRecovery, with configuration collection)
  {
    // ensure there is an empty configuration collection
    {
      auto collection = vocbase->lookupCollection(ANALYZER_COLLECTION_NAME);

      if (collection) {
        vocbase->dropCollection(collection->id(), true, -1);
      }

      collection = vocbase->lookupCollection(ANALYZER_COLLECTION_NAME);
      CHECK((nullptr == collection));
      arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;
      arangodb::iresearch::IResearchAnalyzerFeature feature(s.server);

      CHECK((true == feature.emplace(result, arangodb::StaticStrings::SystemDatabase + "::test_analyzer", "identity", "abc").ok()));
      CHECK((false == !result.first));
      collection = vocbase->lookupCollection(ANALYZER_COLLECTION_NAME);
      CHECK((nullptr != collection));
    }

    auto before = StorageEngineMock::inRecoveryResult;
    StorageEngineMock::inRecoveryResult = true;
    auto restore = irs::make_finally([&before]()->void { StorageEngineMock::inRecoveryResult = before; });
    arangodb::iresearch::IResearchAnalyzerFeature feature(s.server);
    feature.start(); // load persisted analyzers
    CHECK((nullptr != vocbase->lookupCollection(ANALYZER_COLLECTION_NAME)));

    auto expected = staticAnalyzers();
    auto expectedAnalyzer = arangodb::StaticStrings::SystemDatabase + "::test_analyzer";

    expected.emplace(std::piecewise_construct, std::forward_as_tuple(expectedAnalyzer), std::forward_as_tuple("identity", "abc"));
    feature.visit([&expected, &feature](
      arangodb::iresearch::IResearchAnalyzerFeature::AnalyzerPool::ptr const& analyzer
    )->bool {
      auto itr = expected.find(analyzer->name());
      CHECK((itr != expected.end()));
      CHECK((itr->second.type == analyzer->type()));
      CHECK((itr->second.properties == analyzer->properties()));
      CHECK((itr->second.features.is_subset_of(feature.get(analyzer->name())->features())));
      expected.erase(itr);
      return true;
    });
    CHECK((expected.empty()));
  }

  // test feature start load configuration (no configuration collection)
  {
    // ensure no configuration collection
    {
      auto collection = vocbase->lookupCollection(ANALYZER_COLLECTION_NAME);

      if (collection) {
        vocbase->dropCollection(collection->id(), true, -1);
      }

      collection = vocbase->lookupCollection(ANALYZER_COLLECTION_NAME);
      CHECK((nullptr == collection));
    }

    arangodb::iresearch::IResearchAnalyzerFeature feature(s.server);
    feature.start(); // load persisted analyzers
    CHECK((nullptr == vocbase->lookupCollection(ANALYZER_COLLECTION_NAME)));

    auto expected = staticAnalyzers();

    feature.visit([&expected, &feature](
      arangodb::iresearch::IResearchAnalyzerFeature::AnalyzerPool::ptr const& analyzer
    )->bool {
      auto itr = expected.find(analyzer->name());
      CHECK((itr != expected.end()));
      CHECK((itr->second.type == analyzer->type()));
      CHECK((itr->second.properties == analyzer->properties()));
      CHECK((itr->second.features.is_subset_of(feature.get(analyzer->name())->features())));
      expected.erase(itr);
      return true;
    });
    CHECK((expected.empty()));
  }

  // test feature start load configuration (with configuration collection)
  {
    // ensure there is an empty configuration collection
    {
      auto collection = vocbase->lookupCollection(ANALYZER_COLLECTION_NAME);

      if (collection) {
        vocbase->dropCollection(collection->id(), true, -1);
      }

      collection = vocbase->lookupCollection(ANALYZER_COLLECTION_NAME);
      CHECK((nullptr == collection));
      arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;
      arangodb::iresearch::IResearchAnalyzerFeature feature(s.server);

      CHECK((true == feature.emplace(result, arangodb::StaticStrings::SystemDatabase + "::test_analyzer", "identity", "abc").ok()));
      CHECK((false == !result.first));
      collection = vocbase->lookupCollection(ANALYZER_COLLECTION_NAME);
      CHECK((nullptr != collection));
    }

    arangodb::iresearch::IResearchAnalyzerFeature feature(s.server);
    feature.start(); // load persisted analyzers
    CHECK((nullptr != vocbase->lookupCollection(ANALYZER_COLLECTION_NAME)));

    auto expected = staticAnalyzers();
    auto expectedAnalyzer = arangodb::StaticStrings::SystemDatabase + "::test_analyzer";

    expected.emplace(std::piecewise_construct, std::forward_as_tuple(expectedAnalyzer), std::forward_as_tuple("identity", "abc"));
    feature.visit([&expected, &feature](
      arangodb::iresearch::IResearchAnalyzerFeature::AnalyzerPool::ptr const& analyzer
    )->bool {
      auto itr = expected.find(analyzer->name());
      CHECK((itr != expected.end()));
      CHECK((itr->second.type == analyzer->type()));
      CHECK((itr->second.properties == analyzer->properties()));
      CHECK((itr->second.features.is_subset_of(feature.get(analyzer->name())->features())));
      expected.erase(itr);
      return true;
    });
    CHECK((expected.empty()));
  }
}

SECTION("test_tokens") {
  // create a new instance of an ApplicationServer and fill it with the required features
  // cannot use the existing server since its features already have some state
  std::shared_ptr<arangodb::application_features::ApplicationServer> originalServer(
    arangodb::application_features::ApplicationServer::server,
    [](arangodb::application_features::ApplicationServer* ptr)->void {
      arangodb::application_features::ApplicationServer::server = ptr;
    }
  );
  arangodb::application_features::ApplicationServer::server = nullptr; // avoid "ApplicationServer initialized twice"
  arangodb::application_features::ApplicationServer server(nullptr, nullptr);
  auto* analyzers = new arangodb::iresearch::IResearchAnalyzerFeature(server);
  auto* functions = new arangodb::aql::AqlFunctionFeature(server);
  auto* dbfeature = new arangodb::DatabaseFeature(server);
  auto* sharding = new arangodb::ShardingFeature(server);
  auto* systemdb = new arangodb::SystemDatabaseFeature(server);

  arangodb::application_features::ApplicationServer::server->addFeature(analyzers);
  arangodb::application_features::ApplicationServer::server->addFeature(dbfeature);
  arangodb::application_features::ApplicationServer::server->addFeature(functions);
  server.addFeature(new arangodb::QueryRegistryFeature(server)); // required for constructing TRI_vocbase_t
  arangodb::application_features::ApplicationServer::server->addFeature(sharding);
  arangodb::application_features::ApplicationServer::server->addFeature(systemdb);
  server.addFeature(new arangodb::V8DealerFeature(server)); // required for DatabaseFeature::createDatabase(...)

  sharding->prepare();

  // create system vocbase (before feature start)
  {
    auto const databases = arangodb::velocypack::Parser::fromJson(std::string("[ { \"name\": \"") + arangodb::StaticStrings::SystemDatabase + "\" } ]");
    CHECK((TRI_ERROR_NO_ERROR == dbfeature->loadDatabases(databases->slice())));
    systemdb->start(); // get system database from DatabaseFeature
  }

  auto vocbase = systemdb->use();

  // ensure there is no configuration collection
  {
    auto collection = vocbase->lookupCollection(ANALYZER_COLLECTION_NAME);

    if (collection) {
      vocbase->dropCollection(collection->id(), true, -1);
    }

    collection = vocbase->lookupCollection(ANALYZER_COLLECTION_NAME);
    CHECK((nullptr == collection));
  }

  // test function registration
  {
    arangodb::iresearch::IResearchAnalyzerFeature feature(s.server);

    // AqlFunctionFeature::byName(..) throws exception instead of returning a nullptr
    CHECK_THROWS((functions->byName("TOKENS")));

    feature.start(); // load AQL functions
    CHECK((nullptr != functions->byName("TOKENS")));
  }

  arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;
  analyzers->start(); // load AQL functions
  REQUIRE((true == analyzers->emplace(result, arangodb::StaticStrings::SystemDatabase + "::test_analyzer", "TestAnalyzer", "abc").ok()));
  REQUIRE((false == !result.first));

  // test tokenization
  {
    auto* function = functions->byName("TOKENS");
    CHECK((nullptr != functions->byName("TOKENS")));
    auto& impl = function->implementation;
    CHECK((false == !impl));

    std::string analyzer(arangodb::StaticStrings::SystemDatabase + "::test_analyzer");
    irs::string_ref data("abcdefghijklmnopqrstuvwxyz");
    VPackFunctionParametersWrapper args;
    args->emplace_back(data.c_str(), data.size());
    args->emplace_back(analyzer.c_str(), analyzer.size());
    AqlValueWrapper result(impl(nullptr, nullptr, *args));
    CHECK((result->isArray()));
    CHECK((26 == result->length()));

    for (int64_t i = 0; i < 26; ++i) {
      bool mustDestroy;
      auto entry = result->at(i, mustDestroy, false);
      CHECK((entry.isString()));
      auto value = arangodb::iresearch::getStringRef(entry.slice());
      CHECK((1 == value.size()));
      CHECK(('a' + i == value.c_str()[0]));
    }
  }

  // test invalid arg count
  {
    auto* function = functions->byName("TOKENS");
    CHECK((nullptr != functions->byName("TOKENS")));
    auto& impl = function->implementation;
    CHECK((false == !impl));

    arangodb::SmallVector<arangodb::aql::AqlValue>::allocator_type::arena_type arena;
    arangodb::aql::VPackFunctionParameters args{arena};
    AqlValueWrapper result(impl(nullptr, nullptr, args));
    CHECK((result->isNone()));
  }

  // test invalid data type
  {
    auto* function = functions->byName("TOKENS");
    CHECK((nullptr != functions->byName("TOKENS")));
    auto& impl = function->implementation;
    CHECK((false == !impl));

    irs::string_ref data("abcdefghijklmnopqrstuvwxyz");
    VPackFunctionParametersWrapper args;
    args->emplace_back(data.c_str(), data.size());
    args->emplace_back(arangodb::aql::AqlValueHintDouble(123.4));
    AqlValueWrapper result(impl(nullptr, nullptr, *args));
    CHECK((result->isNone()));
  }

  // test invalid analyzer type
  {
    auto* function = functions->byName("TOKENS");
    CHECK((nullptr != functions->byName("TOKENS")));
    auto& impl = function->implementation;
    CHECK((false == !impl));

    irs::string_ref analyzer("test_analyzer");
    VPackFunctionParametersWrapper args;
    args->emplace_back(arangodb::aql::AqlValueHintDouble(123.4));
    args->emplace_back(analyzer.c_str(), analyzer.size());
    AqlValueWrapper result(impl(nullptr, nullptr, *args));
    CHECK((result->isNone()));
  }

  // test invalid analyzer
  {
    auto* function = functions->byName("TOKENS");
    CHECK((nullptr != functions->byName("TOKENS")));
    auto& impl = function->implementation;
    CHECK((false == !impl));

    irs::string_ref analyzer("invalid");
    irs::string_ref data("abcdefghijklmnopqrstuvwxyz");
    VPackFunctionParametersWrapper args;
    args->emplace_back(data.c_str(), data.size());
    args->emplace_back(analyzer.c_str(), analyzer.size());
    AqlValueWrapper result(impl(nullptr, nullptr, *args));
    CHECK((result->isNone()));
  }
}

SECTION("test_upgrade_static_legacy") {
  static std::string const LEGACY_ANALYZER_COLLECTION_NAME("_iresearch_analyzers");
  static std::string const ANALYZER_COLLECTION_QUERY = std::string("FOR d IN ") + ANALYZER_COLLECTION_NAME + " RETURN d";
  static std::unordered_set<std::string> const EXPECTED_LEGACY_ANALYZERS = { "text_de", "text_en", "text_es", "text_fi", "text_fr", "text_it", "text_nl", "text_no", "text_pt", "text_ru", "text_sv", "text_zh", };
  auto createCollectionJson = arangodb::velocypack::Parser::fromJson(std::string("{ \"id\": 42, \"name\": \"") + ANALYZER_COLLECTION_NAME + "\", \"isSystem\": true, \"shards\": { \"same-as-dummy-shard-id\": [ \"shard-server-does-not-matter\" ] }, \"type\": 2 }"); // 'id' and 'shards' required for coordinator tests
  auto createLegacyCollectionJson = arangodb::velocypack::Parser::fromJson(std::string("{ \"id\": 43, \"name\": \"") + LEGACY_ANALYZER_COLLECTION_NAME + "\", \"isSystem\": true, \"shards\": { \"shard-id-does-not-matter\": [ \"shard-server-does-not-matter\" ] }, \"type\": 2 }"); // 'id' and 'shards' required for coordinator tests
  auto collectionId = std::to_string(42);
  auto legacyCollectionId = std::to_string(43);
  auto versionJson = arangodb::velocypack::Parser::fromJson("{ \"version\": 0, \"tasks\": {} }");
  arangodb::AqlFeature aqlFeature(s.server);
  aqlFeature.start(); // required for Query::Query(...), must not call ~AqlFeature() for the duration of the test

  // test no system, no analyzer collection (single-server)
  {
    TRI_vocbase_t system(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 0, TRI_VOC_SYSTEM_DATABASE); // create befor reseting srver

    // create a new instance of an ApplicationServer and fill it with the required features
    // cannot use the existing server since its features already have some state
    std::shared_ptr<arangodb::application_features::ApplicationServer> originalServer(
      arangodb::application_features::ApplicationServer::server,
      [](arangodb::application_features::ApplicationServer* ptr)->void {
        arangodb::application_features::ApplicationServer::server = ptr;
      }
    );
    arangodb::application_features::ApplicationServer::server = nullptr; // avoid "ApplicationServer initialized twice"
    arangodb::application_features::ApplicationServer server(nullptr, nullptr);
    arangodb::iresearch::IResearchAnalyzerFeature* feature;
    arangodb::DatabaseFeature* dbFeature;
    arangodb::SystemDatabaseFeature* sysDatabase;
    server.addFeature(dbFeature = new arangodb::DatabaseFeature(server)); // required for IResearchAnalyzerFeature::emplace(...)
    server.addFeature(new arangodb::QueryRegistryFeature(server)); // required for constructing TRI_vocbase_t
    server.addFeature(sysDatabase = new arangodb::SystemDatabaseFeature(server, &system)); // required for IResearchAnalyzerFeature::start()
    server.addFeature(new arangodb::UpgradeFeature(server, nullptr, {})); // required for upgrade tasks
    server.addFeature(new arangodb::V8DealerFeature(server)); // required for DatabaseFeature::createDatabase(...)
    server.addFeature(feature = new arangodb::iresearch::IResearchAnalyzerFeature(server)); // required for running upgrade task
    arangodb::aql::OptimizerRulesFeature(server).prepare(); // required for Query::preparePlan(...)
    auto clearOptimizerRules = irs::make_finally([&s]()->void { arangodb::aql::OptimizerRulesFeature(s.server).unprepare(); });

    feature->start(); // register upgrade tasks

    arangodb::DatabasePathFeature dbPathFeature(server);
    arangodb::tests::setDatabasePath(dbPathFeature); // ensure test data is stored in a unique directory
    auto versionFilename = StorageEngineMock::versionFilenameResult;
    auto versionFilenameRestore = irs::make_finally([&versionFilename]()->void { StorageEngineMock::versionFilenameResult = versionFilename; });
    StorageEngineMock::versionFilenameResult = (irs::utf8_path(dbPathFeature.directory()) /= "version").utf8();
    REQUIRE((irs::utf8_path(dbPathFeature.directory()).mkdir()));
    REQUIRE((arangodb::basics::VelocyPackHelper::velocyPackToFile(StorageEngineMock::versionFilenameResult, versionJson->slice(), false)));

    auto expected = EXPECTED_LEGACY_ANALYZERS;
    TRI_vocbase_t* vocbase;
    CHECK((TRI_ERROR_NO_ERROR == dbFeature->createDatabase(1, "testVocbase", vocbase)));
    sysDatabase->unprepare(); // unset system vocbase
    CHECK((arangodb::methods::Upgrade::startup(*vocbase, true, false).ok())); // run upgrade
    CHECK((false == !vocbase->lookupCollection(ANALYZER_COLLECTION_NAME)));
    auto result = arangodb::tests::executeQuery(*vocbase, ANALYZER_COLLECTION_QUERY);
    CHECK((result.result.ok()));
    auto slice = result.data->slice();
    CHECK(slice.isArray());

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto resolved = itr.value().resolveExternals();
      CHECK((resolved.isObject()));
      CHECK((resolved.get("name").isString()));
      CHECK((1 == expected.erase(resolved.get("name").copyString())));
    }

    CHECK((true == expected.empty()));
  }

  // test no system, with analyzer collection (single-server)
  {
    TRI_vocbase_t system(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 0, TRI_VOC_SYSTEM_DATABASE); // create befor reseting srver

    // create a new instance of an ApplicationServer and fill it with the required features
    // cannot use the existing server since its features already have some state
    std::shared_ptr<arangodb::application_features::ApplicationServer> originalServer(
      arangodb::application_features::ApplicationServer::server,
      [](arangodb::application_features::ApplicationServer* ptr)->void {
        arangodb::application_features::ApplicationServer::server = ptr;
      }
    );
    arangodb::application_features::ApplicationServer::server = nullptr; // avoid "ApplicationServer initialized twice"
    arangodb::application_features::ApplicationServer server(nullptr, nullptr);
    arangodb::iresearch::IResearchAnalyzerFeature* feature;
    arangodb::DatabaseFeature* dbFeature;
    arangodb::SystemDatabaseFeature* sysDatabase;
    server.addFeature(dbFeature = new arangodb::DatabaseFeature(server)); // required for IResearchAnalyzerFeature::emplace(...)
    server.addFeature(new arangodb::QueryRegistryFeature(server)); // required for constructing TRI_vocbase_t
    server.addFeature(sysDatabase = new arangodb::SystemDatabaseFeature(server, &system)); // required for IResearchAnalyzerFeature::start()
    server.addFeature(new arangodb::UpgradeFeature(server, nullptr, {})); // required for upgrade tasks
    server.addFeature(new arangodb::V8DealerFeature(server)); // required for DatabaseFeature::createDatabase(...)
    server.addFeature(feature = new arangodb::iresearch::IResearchAnalyzerFeature(server)); // required for running upgrade task
    arangodb::aql::OptimizerRulesFeature(server).prepare(); // required for Query::preparePlan(...)
    auto clearOptimizerRules = irs::make_finally([&s]()->void { arangodb::aql::OptimizerRulesFeature(s.server).unprepare(); });

    feature->start(); // register upgrade tasks

    arangodb::DatabasePathFeature dbPathFeature(server);
    arangodb::tests::setDatabasePath(dbPathFeature); // ensure test data is stored in a unique directory
    auto versionFilename = StorageEngineMock::versionFilenameResult;
    auto versionFilenameRestore = irs::make_finally([&versionFilename]()->void { StorageEngineMock::versionFilenameResult = versionFilename; });
    StorageEngineMock::versionFilenameResult = (irs::utf8_path(dbPathFeature.directory()) /= "version").utf8();
    REQUIRE((irs::utf8_path(dbPathFeature.directory()).mkdir()));
    REQUIRE((arangodb::basics::VelocyPackHelper::velocyPackToFile(StorageEngineMock::versionFilenameResult, versionJson->slice(), false)));

    auto expected = EXPECTED_LEGACY_ANALYZERS;
    expected.insert("abc");
    TRI_vocbase_t* vocbase;
    CHECK((TRI_ERROR_NO_ERROR == dbFeature->createDatabase(1, "testVocbase", vocbase)));
    CHECK((false == !vocbase->createCollection(createCollectionJson->slice())));

    // add document to collection
    {
      arangodb::OperationOptions options;
      arangodb::SingleCollectionTransaction trx(
        arangodb::transaction::StandaloneContext::Create(*vocbase),
        ANALYZER_COLLECTION_NAME,
        arangodb::AccessMode::Type::WRITE
      );
      CHECK((true == trx.begin().ok()));
      CHECK((true == trx.insert(ANALYZER_COLLECTION_NAME, arangodb::velocypack::Parser::fromJson("{\"name\": \"abc\"}")->slice(), options).ok()));
      CHECK((trx.commit().ok()));
    }

    sysDatabase->unprepare(); // unset system vocbase
    CHECK((arangodb::methods::Upgrade::startup(*vocbase, true, false).ok())); // run upgrade
    CHECK((false == !vocbase->lookupCollection(ANALYZER_COLLECTION_NAME)));
    auto result = arangodb::tests::executeQuery(*vocbase, ANALYZER_COLLECTION_QUERY);
    CHECK((result.result.ok()));
    auto slice = result.data->slice();
    CHECK(slice.isArray());

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto resolved = itr.value().resolveExternals();
      CHECK((resolved.isObject()));
      CHECK((resolved.get("name").isString()));
      CHECK((1 == expected.erase(resolved.get("name").copyString())));
    }

    CHECK((true == expected.empty()));
  }

  // test system, no legacy collection, no analyzer collection (single-server)
  {
    TRI_vocbase_t system(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 0, TRI_VOC_SYSTEM_DATABASE); // create befor reseting srver

    // create a new instance of an ApplicationServer and fill it with the required features
    // cannot use the existing server since its features already have some state
    std::shared_ptr<arangodb::application_features::ApplicationServer> originalServer(
      arangodb::application_features::ApplicationServer::server,
      [](arangodb::application_features::ApplicationServer* ptr)->void {
        arangodb::application_features::ApplicationServer::server = ptr;
      }
    );
    arangodb::application_features::ApplicationServer::server = nullptr; // avoid "ApplicationServer initialized twice"
    arangodb::application_features::ApplicationServer server(nullptr, nullptr);
    arangodb::iresearch::IResearchAnalyzerFeature* feature;
    arangodb::DatabaseFeature* dbFeature;
    arangodb::SystemDatabaseFeature* sysDatabase;
    server.addFeature(dbFeature = new arangodb::DatabaseFeature(server)); // required for IResearchAnalyzerFeature::emplace(...)
    server.addFeature(new arangodb::QueryRegistryFeature(server)); // required for constructing TRI_vocbase_t
    server.addFeature(sysDatabase = new arangodb::SystemDatabaseFeature(server, &system)); // required for IResearchAnalyzerFeature::start()
    server.addFeature(new arangodb::UpgradeFeature(server, nullptr, {})); // required for upgrade tasks
    server.addFeature(new arangodb::V8DealerFeature(server)); // required for DatabaseFeature::createDatabase(...)
    server.addFeature(feature = new arangodb::iresearch::IResearchAnalyzerFeature(server)); // required for running upgrade task
    arangodb::aql::OptimizerRulesFeature(server).prepare(); // required for Query::preparePlan(...)
    auto clearOptimizerRules = irs::make_finally([&s]()->void { arangodb::aql::OptimizerRulesFeature(s.server).unprepare(); });

    feature->start(); // register upgrade tasks

    // ensure no legacy collection after feature start
    {
      auto collection = system.lookupCollection(LEGACY_ANALYZER_COLLECTION_NAME);
      REQUIRE((true == !collection));
    }

    arangodb::DatabasePathFeature dbPathFeature(server);
    arangodb::tests::setDatabasePath(dbPathFeature); // ensure test data is stored in a unique directory
    auto versionFilename = StorageEngineMock::versionFilenameResult;
    auto versionFilenameRestore = irs::make_finally([&versionFilename]()->void { StorageEngineMock::versionFilenameResult = versionFilename; });
    StorageEngineMock::versionFilenameResult = (irs::utf8_path(dbPathFeature.directory()) /= "version").utf8();
    REQUIRE((irs::utf8_path(dbPathFeature.directory()).mkdir()));
    REQUIRE((arangodb::basics::VelocyPackHelper::velocyPackToFile(StorageEngineMock::versionFilenameResult, versionJson->slice(), false)));

    auto expected = EXPECTED_LEGACY_ANALYZERS;
    TRI_vocbase_t* vocbase;
    CHECK((TRI_ERROR_NO_ERROR == dbFeature->createDatabase(1, "testVocbase", vocbase)));
    CHECK((arangodb::methods::Upgrade::startup(*vocbase, true, false).ok())); // run upgrade
    CHECK((false == !vocbase->lookupCollection(ANALYZER_COLLECTION_NAME)));
    auto result = arangodb::tests::executeQuery(*vocbase, ANALYZER_COLLECTION_QUERY);
    CHECK((result.result.ok()));
    auto slice = result.data->slice();
    CHECK(slice.isArray());

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto resolved = itr.value().resolveExternals();
      CHECK((resolved.isObject()));
      CHECK((resolved.get("name").isString()));
      CHECK((1 == expected.erase(resolved.get("name").copyString())));
    }

    CHECK((true == expected.empty()));
  }

  // test system, no legacy collection, with analyzer collection (single-server)
  {
    TRI_vocbase_t system(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 0, TRI_VOC_SYSTEM_DATABASE); // create befor reseting srver

    // create a new instance of an ApplicationServer and fill it with the required features
    // cannot use the existing server since its features already have some state
    std::shared_ptr<arangodb::application_features::ApplicationServer> originalServer(
      arangodb::application_features::ApplicationServer::server,
      [](arangodb::application_features::ApplicationServer* ptr)->void {
        arangodb::application_features::ApplicationServer::server = ptr;
      }
    );
    arangodb::application_features::ApplicationServer::server = nullptr; // avoid "ApplicationServer initialized twice"
    arangodb::application_features::ApplicationServer server(nullptr, nullptr);
    arangodb::iresearch::IResearchAnalyzerFeature* feature;
    arangodb::DatabaseFeature* dbFeature;
    arangodb::SystemDatabaseFeature* sysDatabase;
    server.addFeature(dbFeature = new arangodb::DatabaseFeature(server)); // required for IResearchAnalyzerFeature::emplace(...)
    server.addFeature(new arangodb::QueryRegistryFeature(server)); // required for constructing TRI_vocbase_t
    server.addFeature(sysDatabase = new arangodb::SystemDatabaseFeature(server, &system)); // required for IResearchAnalyzerFeature::start()
    server.addFeature(new arangodb::UpgradeFeature(server, nullptr, {})); // required for upgrade tasks
    server.addFeature(new arangodb::V8DealerFeature(server)); // required for DatabaseFeature::createDatabase(...)
    server.addFeature(feature = new arangodb::iresearch::IResearchAnalyzerFeature(server)); // required for running upgrade task
    arangodb::aql::OptimizerRulesFeature(server).prepare(); // required for Query::preparePlan(...)
    auto clearOptimizerRules = irs::make_finally([&s]()->void { arangodb::aql::OptimizerRulesFeature(s.server).unprepare(); });

    feature->start(); // register upgrade tasks

    // ensure no legacy collection after feature start
    {
      auto collection = system.lookupCollection(LEGACY_ANALYZER_COLLECTION_NAME);
      REQUIRE((true == !collection));
    }

    arangodb::DatabasePathFeature dbPathFeature(server);
    arangodb::tests::setDatabasePath(dbPathFeature); // ensure test data is stored in a unique directory
    auto versionFilename = StorageEngineMock::versionFilenameResult;
    auto versionFilenameRestore = irs::make_finally([&versionFilename]()->void { StorageEngineMock::versionFilenameResult = versionFilename; });
    StorageEngineMock::versionFilenameResult = (irs::utf8_path(dbPathFeature.directory()) /= "version").utf8();
    REQUIRE((irs::utf8_path(dbPathFeature.directory()).mkdir()));
    REQUIRE((arangodb::basics::VelocyPackHelper::velocyPackToFile(StorageEngineMock::versionFilenameResult, versionJson->slice(), false)));

    auto expected = EXPECTED_LEGACY_ANALYZERS;
    expected.insert("abc");
    TRI_vocbase_t* vocbase;
    CHECK((TRI_ERROR_NO_ERROR == dbFeature->createDatabase(1, "testVocbase", vocbase)));
    CHECK((false == !vocbase->createCollection(createCollectionJson->slice())));

    // add document to collection
    {
      arangodb::OperationOptions options;
      arangodb::SingleCollectionTransaction trx(
        arangodb::transaction::StandaloneContext::Create(*vocbase),
        ANALYZER_COLLECTION_NAME,
        arangodb::AccessMode::Type::WRITE
      );
      CHECK((true == trx.begin().ok()));
      CHECK((true == trx.insert(ANALYZER_COLLECTION_NAME, arangodb::velocypack::Parser::fromJson("{\"name\": \"abc\"}")->slice(), options).ok()));
      CHECK((trx.commit().ok()));
    }

    CHECK((arangodb::methods::Upgrade::startup(*vocbase, true, false).ok())); // run upgrade
    CHECK((false == !vocbase->lookupCollection(ANALYZER_COLLECTION_NAME)));
    auto result = arangodb::tests::executeQuery(*vocbase, ANALYZER_COLLECTION_QUERY);
    CHECK((result.result.ok()));
    auto slice = result.data->slice();
    CHECK(slice.isArray());

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto resolved = itr.value().resolveExternals();
      CHECK((resolved.isObject()));
      CHECK((resolved.get("name").isString()));
      CHECK((1 == expected.erase(resolved.get("name").copyString())));
    }

    CHECK((true == expected.empty()));
  }

  // test system, with legacy collection, no analyzer collection (single-server)
  {
    TRI_vocbase_t system(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 0, TRI_VOC_SYSTEM_DATABASE); // create befor reseting srver

    // create a new instance of an ApplicationServer and fill it with the required features
    // cannot use the existing server since its features already have some state
    std::shared_ptr<arangodb::application_features::ApplicationServer> originalServer(
      arangodb::application_features::ApplicationServer::server,
      [](arangodb::application_features::ApplicationServer* ptr)->void {
        arangodb::application_features::ApplicationServer::server = ptr;
      }
    );
    arangodb::application_features::ApplicationServer::server = nullptr; // avoid "ApplicationServer initialized twice"
    arangodb::application_features::ApplicationServer server(nullptr, nullptr);
    arangodb::iresearch::IResearchAnalyzerFeature* feature;
    arangodb::DatabaseFeature* dbFeature;
    arangodb::SystemDatabaseFeature* sysDatabase;
    server.addFeature(dbFeature = new arangodb::DatabaseFeature(server)); // required for IResearchAnalyzerFeature::emplace(...)
    server.addFeature(new arangodb::QueryRegistryFeature(server)); // required for constructing TRI_vocbase_t
    server.addFeature(sysDatabase = new arangodb::SystemDatabaseFeature(server, &system)); // required for IResearchAnalyzerFeature::start()
    server.addFeature(new arangodb::UpgradeFeature(server, nullptr, {})); // required for upgrade tasks
    server.addFeature(new arangodb::V8DealerFeature(server)); // required for DatabaseFeature::createDatabase(...)
    server.addFeature(feature = new arangodb::iresearch::IResearchAnalyzerFeature(server)); // required for running upgrade task
    arangodb::aql::OptimizerRulesFeature(server).prepare(); // required for Query::preparePlan(...)
    auto clearOptimizerRules = irs::make_finally([&s]()->void { arangodb::aql::OptimizerRulesFeature(s.server).unprepare(); });

    feature->start(); // register upgrade tasks

    // ensure legacy collection after feature start
    {
      auto collection = system.lookupCollection(LEGACY_ANALYZER_COLLECTION_NAME);
      REQUIRE((true == !collection));
      REQUIRE((false == !system.createCollection(createLegacyCollectionJson->slice())));
    }

    // add document to legacy collection after feature start
    {
      arangodb::OperationOptions options;
      arangodb::SingleCollectionTransaction trx(
        arangodb::transaction::StandaloneContext::Create(system),
        LEGACY_ANALYZER_COLLECTION_NAME,
        arangodb::AccessMode::Type::WRITE
      );
      CHECK((true == trx.begin().ok()));
      CHECK((true == trx.insert(ANALYZER_COLLECTION_NAME, arangodb::velocypack::Parser::fromJson("{\"name\": \"legacy\"}")->slice(), options).ok()));
      CHECK((trx.commit().ok()));
    }

    arangodb::DatabasePathFeature dbPathFeature(server);
    arangodb::tests::setDatabasePath(dbPathFeature); // ensure test data is stored in a unique directory
    auto versionFilename = StorageEngineMock::versionFilenameResult;
    auto versionFilenameRestore = irs::make_finally([&versionFilename]()->void { StorageEngineMock::versionFilenameResult = versionFilename; });
    StorageEngineMock::versionFilenameResult = (irs::utf8_path(dbPathFeature.directory()) /= "version").utf8();
    REQUIRE((irs::utf8_path(dbPathFeature.directory()).mkdir()));
    REQUIRE((arangodb::basics::VelocyPackHelper::velocyPackToFile(StorageEngineMock::versionFilenameResult, versionJson->slice(), false)));

    auto expected = EXPECTED_LEGACY_ANALYZERS;
    TRI_vocbase_t* vocbase;
    CHECK((TRI_ERROR_NO_ERROR == dbFeature->createDatabase(1, "testVocbase", vocbase)));
    CHECK((arangodb::methods::Upgrade::startup(*vocbase, true, false).ok())); // run upgrade
    CHECK((false == !vocbase->lookupCollection(ANALYZER_COLLECTION_NAME)));
    auto result = arangodb::tests::executeQuery(*vocbase, ANALYZER_COLLECTION_QUERY);
    CHECK((result.result.ok()));
    auto slice = result.data->slice();
    CHECK(slice.isArray());

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto resolved = itr.value().resolveExternals();
      CHECK((resolved.isObject()));
      CHECK((resolved.get("name").isString()));
      CHECK((1 == expected.erase(resolved.get("name").copyString())));
    }

    CHECK((true == expected.empty()));
  }

  // test system, with legacy collection, with analyzer collection (single-server)
  {
    TRI_vocbase_t system(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 0, TRI_VOC_SYSTEM_DATABASE); // create befor reseting srver

    // create a new instance of an ApplicationServer and fill it with the required features
    // cannot use the existing server since its features already have some state
    std::shared_ptr<arangodb::application_features::ApplicationServer> originalServer(
      arangodb::application_features::ApplicationServer::server,
      [](arangodb::application_features::ApplicationServer* ptr)->void {
        arangodb::application_features::ApplicationServer::server = ptr;
      }
    );
    arangodb::application_features::ApplicationServer::server = nullptr; // avoid "ApplicationServer initialized twice"
    arangodb::application_features::ApplicationServer server(nullptr, nullptr);
    arangodb::iresearch::IResearchAnalyzerFeature* feature;
    arangodb::DatabaseFeature* dbFeature;
    arangodb::SystemDatabaseFeature* sysDatabase;
    server.addFeature(dbFeature = new arangodb::DatabaseFeature(server)); // required for IResearchAnalyzerFeature::emplace(...)
    server.addFeature(new arangodb::QueryRegistryFeature(server)); // required for constructing TRI_vocbase_t
    server.addFeature(sysDatabase = new arangodb::SystemDatabaseFeature(server, &system)); // required for IResearchAnalyzerFeature::start()
    server.addFeature(new arangodb::UpgradeFeature(server, nullptr, {})); // required for upgrade tasks
    server.addFeature(new arangodb::V8DealerFeature(server)); // required for DatabaseFeature::createDatabase(...)
    server.addFeature(feature = new arangodb::iresearch::IResearchAnalyzerFeature(server)); // required for running upgrade task
    arangodb::aql::OptimizerRulesFeature(server).prepare(); // required for Query::preparePlan(...)
    auto clearOptimizerRules = irs::make_finally([&s]()->void { arangodb::aql::OptimizerRulesFeature(s.server).unprepare(); });

    feature->start(); // register upgrade tasks

    // ensure legacy collection after feature start
    {
      auto collection = system.lookupCollection(LEGACY_ANALYZER_COLLECTION_NAME);
      REQUIRE((true == !collection));
      REQUIRE((false == !system.createCollection(createLegacyCollectionJson->slice())));
    }

    // add document to legacy collection after feature start
    {
      arangodb::OperationOptions options;
      arangodb::SingleCollectionTransaction trx(
        arangodb::transaction::StandaloneContext::Create(system),
        LEGACY_ANALYZER_COLLECTION_NAME,
        arangodb::AccessMode::Type::WRITE
      );
      CHECK((true == trx.begin().ok()));
      CHECK((true == trx.insert(ANALYZER_COLLECTION_NAME, arangodb::velocypack::Parser::fromJson("{\"name\": \"legacy\"}")->slice(), options).ok()));
      CHECK((trx.commit().ok()));
    }

    arangodb::DatabasePathFeature dbPathFeature(server);
    arangodb::tests::setDatabasePath(dbPathFeature); // ensure test data is stored in a unique directory
    auto versionFilename = StorageEngineMock::versionFilenameResult;
    auto versionFilenameRestore = irs::make_finally([&versionFilename]()->void { StorageEngineMock::versionFilenameResult = versionFilename; });
    StorageEngineMock::versionFilenameResult = (irs::utf8_path(dbPathFeature.directory()) /= "version").utf8();
    REQUIRE((irs::utf8_path(dbPathFeature.directory()).mkdir()));
    REQUIRE((arangodb::basics::VelocyPackHelper::velocyPackToFile(StorageEngineMock::versionFilenameResult, versionJson->slice(), false)));

    auto expected = EXPECTED_LEGACY_ANALYZERS;
    expected.insert("abc");
    TRI_vocbase_t* vocbase;
    CHECK((TRI_ERROR_NO_ERROR == dbFeature->createDatabase(1, "testVocbase", vocbase)));
    CHECK((false == !vocbase->createCollection(createCollectionJson->slice())));

    // add document to collection
    {
      arangodb::OperationOptions options;
      arangodb::SingleCollectionTransaction trx(
        arangodb::transaction::StandaloneContext::Create(*vocbase),
        ANALYZER_COLLECTION_NAME,
        arangodb::AccessMode::Type::WRITE
      );
      CHECK((true == trx.begin().ok()));
      CHECK((true == trx.insert(ANALYZER_COLLECTION_NAME, arangodb::velocypack::Parser::fromJson("{\"name\": \"abc\"}")->slice(), options).ok()));
      CHECK((trx.commit().ok()));
    }

    CHECK((arangodb::methods::Upgrade::startup(*vocbase, true, false).ok())); // run upgrade
    CHECK((false == !vocbase->lookupCollection(ANALYZER_COLLECTION_NAME)));
    auto result = arangodb::tests::executeQuery(*vocbase, ANALYZER_COLLECTION_QUERY);
    CHECK((result.result.ok()));
    auto slice = result.data->slice();
    CHECK(slice.isArray());

    for (arangodb::velocypack::ArrayIterator itr(slice); itr.valid(); ++itr) {
      auto resolved = itr.value().resolveExternals();
      CHECK((resolved.isObject()));
      CHECK((resolved.get("name").isString()));
      CHECK((1 == expected.erase(resolved.get("name").copyString())));
    }

    CHECK((true == expected.empty()));
  }

  // test no system, no analyzer collection (coordinator)
  {
    TRI_vocbase_t system(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 0, TRI_VOC_SYSTEM_DATABASE); // create befor reseting srver

    auto serverRoleBefore = arangodb::ServerState::instance()->getRole();
    arangodb::ServerState::instance()->setRole(arangodb::ServerState::ROLE_COORDINATOR);
    auto serverRoleRestore = irs::make_finally([&serverRoleBefore]()->void { arangodb::ServerState::instance()->setRole(serverRoleBefore); });

    // create a new instance of an ApplicationServer and fill it with the required features
    // cannot use the existing server since its features already have some state
    std::shared_ptr<arangodb::application_features::ApplicationServer> originalServer(
      arangodb::application_features::ApplicationServer::server,
      [](arangodb::application_features::ApplicationServer* ptr)->void {
        arangodb::application_features::ApplicationServer::server = ptr;
      }
    );
    arangodb::application_features::ApplicationServer::server = nullptr; // avoid "ApplicationServer initialized twice"
    arangodb::application_features::ApplicationServer server(nullptr, nullptr);
    arangodb::iresearch::IResearchAnalyzerFeature* feature;
    arangodb::DatabaseFeature* dbFeature;
    arangodb::SystemDatabaseFeature* sysDatabase;
    server.addFeature(new arangodb::ClusterFeature(server)); // required to create ClusterInfo instance
    server.addFeature(dbFeature = new arangodb::DatabaseFeature(server)); // required for IResearchAnalyzerFeature::emplace(...)
    server.addFeature(new arangodb::QueryRegistryFeature(server)); // required for constructing TRI_vocbase_t
    server.addFeature(new arangodb::ShardingFeature(server)); // required for Collections::create(...)
    server.addFeature(sysDatabase = new arangodb::SystemDatabaseFeature(server, &system)); // required for IResearchAnalyzerFeature::start()
    server.addFeature(new arangodb::UpgradeFeature(server, nullptr, {})); // required for upgrade tasks
    server.addFeature(new arangodb::V8DealerFeature(server)); // required for DatabaseFeature::createDatabase(...)
    server.addFeature(new arangodb::application_features::CommunicationFeaturePhase(server)); // required for SimpleHttpClient::doRequest()
    server.addFeature(feature = new arangodb::iresearch::IResearchAnalyzerFeature(server)); // required for running upgrade task
    arangodb::aql::OptimizerRulesFeature(server).prepare(); // required for Query::preparePlan(...)
    auto clearOptimizerRules = irs::make_finally([&s]()->void { arangodb::aql::OptimizerRulesFeature(s.server).unprepare(); });

    feature->start(); // register upgrade tasks
    server.getFeature<arangodb::ClusterFeature>("Cluster")->prepare(); // create ClusterInfo instance
    server.getFeature<arangodb::ShardingFeature>("Sharding")->prepare(); // required for Collections::create(...), register sharding types
    arangodb::AgencyCommManager::MANAGER->start(); // initialize agency

    ClusterCommMock clusterComm;
    auto scopedClusterComm = ClusterCommMock::setInstance(clusterComm);
    auto* ci = arangodb::ClusterInfo::instance();
    REQUIRE((nullptr != ci));

    // simulate heartbeat thread
    // (create dbserver in current) required by ClusterMethods::persistCollectionInAgency(...)
    // (create database in plan) required for ClusterInfo::createCollectionCoordinator(...)
    // (create collection in current) required by ClusterMethods::persistCollectionInAgency(...)
    // (create dummy collection in plan to fill ClusterInfo::_shardServers) required by ClusterMethods::persistCollectionInAgency(...)
    {
      auto const srvPath = "/Current/DBServers";
      auto const srvValue = arangodb::velocypack::Parser::fromJson("{ \"dbserver-key-does-not-matter\": \"dbserver-value-does-not-matter\" }");
      CHECK(arangodb::AgencyComm().setValue(srvPath, srvValue->slice(), 0.0).successful());
      auto const dbPath = "/Plan/Databases/testVocbase";
      auto const dbValue = arangodb::velocypack::Parser::fromJson("null"); // value does not matter
      CHECK(arangodb::AgencyComm().setValue(dbPath, dbValue->slice(), 0.0).successful());
      auto const colPath = "/Current/Collections/testVocbase/2"; // '2' must match what ClusterInfo generates for LogicalCollection::id() or collection creation request will never get executed (use 'collectionID' from ClusterInfo::createCollectionCoordinator(...) in stack trace)
      auto const colValue = arangodb::velocypack::Parser::fromJson("{ \"same-as-dummy-shard-id\": { \"servers\": [ \"same-as-dummy-shard-server\" ] } }");
      CHECK(arangodb::AgencyComm().setValue(colPath, colValue->slice(), 0.0).successful());
      auto const dummyPath = "/Plan/Collections";
      auto const dummyValue = arangodb::velocypack::Parser::fromJson("{ \"testVocbase\": { \"collection-id-does-not-matter\": { \"name\": \"dummy\", \"shards\": { \"same-as-dummy-shard-id\": [ \"same-as-dummy-shard-server\" ] } } } }");
      CHECK(arangodb::AgencyComm().setValue(dummyPath, dummyValue->slice(), 0.0).successful());
    }

    auto expected = EXPECTED_LEGACY_ANALYZERS;
    TRI_vocbase_t* vocbase;
    CHECK((TRI_ERROR_NO_ERROR == dbFeature->createDatabase(1, "testVocbase", vocbase)));

    // insert responses for the legacy static analyzers
    for (size_t i = 0, count = EXPECTED_LEGACY_ANALYZERS.size(); i < count; ++i) {
      arangodb::ClusterCommResult response;
      response.operationID = i + 1; // sequential non-zero value
      response.status = arangodb::ClusterCommOpStatus::CL_COMM_RECEIVED;
      response.answer_code = arangodb::rest::ResponseCode::CREATED;
      response.answer = std::make_shared<GeneralRequestMock>(*vocbase);
      static_cast<GeneralRequestMock*>(response.answer.get())->_payload = *arangodb::velocypack::Parser::fromJson(std::string("{ \"_key\": \"") + std::to_string(response.operationID) + "\" }"); // unique arbitrary key
      clusterComm._responses.emplace_back(std::move(response));
    }

    sysDatabase->unprepare(); // unset system vocbase
    CHECK_THROWS((ci->getCollection(vocbase->name(), ANALYZER_COLLECTION_NAME))); // throws on missing collection
    CHECK((arangodb::methods::Upgrade::clusterBootstrap(*vocbase).ok())); // run upgrade
    CHECK((false == !ci->getCollection(vocbase->name(), ANALYZER_COLLECTION_NAME)));
    CHECK((true == clusterComm._responses.empty()));

    for (auto& entry: clusterComm._requests) {
      CHECK((entry._body));
      auto body = arangodb::velocypack::Parser::fromJson(*(entry._body));
      auto slice = body->slice();
      CHECK((slice.isObject()));
      CHECK((slice.get("name").isString()));
      CHECK((1 == expected.erase(slice.get("name").copyString())));
    }

    CHECK((true == expected.empty()));
  }

  // test no system, with analyzer collection (coordinator)
  {
    TRI_vocbase_t system(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 0, TRI_VOC_SYSTEM_DATABASE); // create befor reseting srver

    auto serverRoleBefore = arangodb::ServerState::instance()->getRole();
    arangodb::ServerState::instance()->setRole(arangodb::ServerState::ROLE_COORDINATOR);
    auto serverRoleRestore = irs::make_finally([&serverRoleBefore]()->void { arangodb::ServerState::instance()->setRole(serverRoleBefore); });

    // create a new instance of an ApplicationServer and fill it with the required features
    // cannot use the existing server since its features already have some state
    std::shared_ptr<arangodb::application_features::ApplicationServer> originalServer(
      arangodb::application_features::ApplicationServer::server,
      [](arangodb::application_features::ApplicationServer* ptr)->void {
        arangodb::application_features::ApplicationServer::server = ptr;
      }
    );
    arangodb::application_features::ApplicationServer::server = nullptr; // avoid "ApplicationServer initialized twice"
    arangodb::application_features::ApplicationServer server(nullptr, nullptr);
    arangodb::iresearch::IResearchAnalyzerFeature* feature;
    arangodb::DatabaseFeature* dbFeature;
    arangodb::SystemDatabaseFeature* sysDatabase;
    server.addFeature(new arangodb::ClusterFeature(server)); // required to create ClusterInfo instance
    server.addFeature(dbFeature = new arangodb::DatabaseFeature(server)); // required for IResearchAnalyzerFeature::emplace(...)
    server.addFeature(new arangodb::QueryRegistryFeature(server)); // required for constructing TRI_vocbase_t
    server.addFeature(new arangodb::ShardingFeature(server)); // required for Collections::create(...)
    server.addFeature(sysDatabase = new arangodb::SystemDatabaseFeature(server, &system)); // required for IResearchAnalyzerFeature::start()
    server.addFeature(new arangodb::UpgradeFeature(server, nullptr, {})); // required for upgrade tasks
    server.addFeature(new arangodb::V8DealerFeature(server)); // required for DatabaseFeature::createDatabase(...)
    server.addFeature(new arangodb::application_features::CommunicationFeaturePhase(server)); // required for SimpleHttpClient::doRequest()
    server.addFeature(feature = new arangodb::iresearch::IResearchAnalyzerFeature(server)); // required for running upgrade task
    arangodb::aql::OptimizerRulesFeature(server).prepare(); // required for Query::preparePlan(...)
    auto clearOptimizerRules = irs::make_finally([&s]()->void { arangodb::aql::OptimizerRulesFeature(s.server).unprepare(); });

    feature->start(); // register upgrade tasks
    server.getFeature<arangodb::ClusterFeature>("Cluster")->prepare(); // create ClusterInfo instance
    server.getFeature<arangodb::ShardingFeature>("Sharding")->prepare(); // required for Collections::create(...), register sharding types
    arangodb::AgencyCommManager::MANAGER->start(); // initialize agency

    ClusterCommMock clusterComm;
    auto scopedClusterComm = ClusterCommMock::setInstance(clusterComm);
    auto* ci = arangodb::ClusterInfo::instance();
    REQUIRE((nullptr != ci));

    // simulate heartbeat thread
    // (create dbserver in current) required by ClusterMethods::persistCollectionInAgency(...)
    // (create database in plan) required for ClusterInfo::createCollectionCoordinator(...)
    // (create collection in current) required by ClusterMethods::persistCollectionInAgency(...)
    // (create dummy collection in plan to fill ClusterInfo::_shardServers) required by ClusterMethods::persistCollectionInAgency(...)
    {
      auto const srvPath = "/Current/DBServers";
      auto const srvValue = arangodb::velocypack::Parser::fromJson("{ \"dbserver-key-does-not-matter\": \"dbserver-value-does-not-matter\" }");
      CHECK(arangodb::AgencyComm().setValue(srvPath, srvValue->slice(), 0.0).successful());
      auto const dbPath = "/Plan/Databases/testVocbase";
      auto const dbValue = arangodb::velocypack::Parser::fromJson("null"); // value does not matter
      CHECK(arangodb::AgencyComm().setValue(dbPath, dbValue->slice(), 0.0).successful());
      auto const colPath = "/Current/Collections/testVocbase/2"; // '2' must match what ClusterInfo generates for LogicalCollection::id() or collection creation request will never get executed (use 'collectionID' from ClusterInfo::createCollectionCoordinator(...) in stack trace)
      auto const colValue = arangodb::velocypack::Parser::fromJson("{ \"same-as-dummy-shard-id\": { \"servers\": [ \"same-as-dummy-shard-server\" ] } }");
      CHECK(arangodb::AgencyComm().setValue(colPath, colValue->slice(), 0.0).successful());
      auto const dummyPath = "/Plan/Collections";
      auto const dummyValue = arangodb::velocypack::Parser::fromJson("{ \"testVocbase\": { \"collection-id-does-not-matter\": { \"name\": \"dummy\", \"shards\": { \"same-as-dummy-shard-id\": [ \"same-as-dummy-shard-server\" ] } } } }");
      CHECK(arangodb::AgencyComm().setValue(dummyPath, dummyValue->slice(), 0.0).successful());
    }

    auto expected = EXPECTED_LEGACY_ANALYZERS;
    TRI_vocbase_t* vocbase;
    CHECK((TRI_ERROR_NO_ERROR == dbFeature->createDatabase(1, "testVocbase", vocbase)));
    CHECK((ci->createCollectionCoordinator(vocbase->name(), collectionId, 0, 1, false, createCollectionJson->slice(), 0.0).ok()));
    CHECK((false == !ci->getCollection(vocbase->name(), collectionId)));

    // insert response for expected analyzer lookup
    {
      arangodb::ClusterCommResult response;
      response.status = arangodb::ClusterCommOpStatus::CL_COMM_RECEIVED;
      response.result = std::make_shared<arangodb::httpclient::SimpleHttpResult>();
      response.result->getBody().appendText("{ \"result\": { \"snippets\": { \"6:shard-id-does-not-matter\": \"value-does-not-matter\" } } }").ensureNullTerminated(); // '6' must match GATHER Node id in ExecutionEngine::createBlocks(...)
      clusterComm._responses.emplace_back(std::move(response));
    }

    // insert response for expected analyzer reload from collection
    {
      arangodb::ClusterCommResult response;
      response.status = arangodb::ClusterCommOpStatus::CL_COMM_SENT;
      response.result = std::make_shared<arangodb::httpclient::SimpleHttpResult>();
      response.result->getBody().appendText("{ \"done\": true, \"nrItems\": 1, \"nrRegs\": 1, \"data\": [ 1 ], \"raw\": [ null, null, { \"_key\": \"key-does-not-matter\", \"name\": \"abc\", \"type\": \"TestAnalyzer\", \"properties\": \"abc\" } ] }").ensureNullTerminated(); // 'data' value must be 1 as per AqlItemBlock::AqlItemBlock(...), first 2 'raw' values ignored, 'nrRegs' must be 1 or assertion failure in ExecutionBlockImpl<Executor>::requestWrappedBlock(...)
      clusterComm._responses.emplace_back(std::move(response));
    }

    // insert responses for the legacy static analyzers
    for (size_t i = 0, count = EXPECTED_LEGACY_ANALYZERS.size(); i < count; ++i) {
      arangodb::ClusterCommResult response;
      response.operationID = i + 1; // sequential non-zero value
      response.status = arangodb::ClusterCommOpStatus::CL_COMM_RECEIVED;
      response.answer_code = arangodb::rest::ResponseCode::CREATED;
      response.answer = std::make_shared<GeneralRequestMock>(*vocbase);
      static_cast<GeneralRequestMock*>(response.answer.get())->_payload = *arangodb::velocypack::Parser::fromJson(std::string("{ \"_key\": \"") + std::to_string(response.operationID) + "\" }"); // unique arbitrary key
      clusterComm._responses.emplace_back(std::move(response));
    }

    sysDatabase->unprepare(); // unset system vocbase
    CHECK((false == !ci->getCollection(vocbase->name(), ANALYZER_COLLECTION_NAME)));
    CHECK((arangodb::methods::Upgrade::clusterBootstrap(*vocbase).ok())); // run upgrade
    CHECK((false == !ci->getCollection(vocbase->name(), ANALYZER_COLLECTION_NAME)));
    CHECK((true == clusterComm._responses.empty()));

    for (size_t i = 2, count = clusterComm._requests.size(); i < count; ++i) { // +2 to skip requests from loadAnalyzers(...)
      auto& entry = clusterComm._requests[i];
      CHECK((entry._body));
      auto body = arangodb::velocypack::Parser::fromJson(*(entry._body));
      auto slice = body->slice();
      CHECK((slice.isObject()));
      CHECK((slice.get("name").isString()));
      CHECK((1 == expected.erase(slice.get("name").copyString())));
    }

    CHECK((true == expected.empty())); // expect only analyzers inserted by upgrade (since checking '_requests')
  }

  // test system, no legacy collection, no analyzer collection (coordinator)
  {
    auto serverRoleBefore = arangodb::ServerState::instance()->getRole();
    arangodb::ServerState::instance()->setRole(arangodb::ServerState::ROLE_COORDINATOR);
    auto serverRoleRestore = irs::make_finally([&serverRoleBefore]()->void { arangodb::ServerState::instance()->setRole(serverRoleBefore); });

    // create a new instance of an ApplicationServer and fill it with the required features
    // cannot use the existing server since its features already have some state
    std::shared_ptr<arangodb::application_features::ApplicationServer> originalServer(
      arangodb::application_features::ApplicationServer::server,
      [](arangodb::application_features::ApplicationServer* ptr)->void {
        arangodb::application_features::ApplicationServer::server = ptr;
      }
    );
    arangodb::application_features::ApplicationServer::server = nullptr; // avoid "ApplicationServer initialized twice"
    arangodb::application_features::ApplicationServer server(nullptr, nullptr);
    arangodb::iresearch::IResearchAnalyzerFeature* feature;
    arangodb::DatabaseFeature* dbFeature;
    arangodb::SystemDatabaseFeature* sysDatabase;
    server.addFeature(new arangodb::ClusterFeature(server)); // required to create ClusterInfo instance
    server.addFeature(dbFeature = new arangodb::DatabaseFeature(server)); // required for IResearchAnalyzerFeature::emplace(...)
    server.addFeature(new arangodb::QueryRegistryFeature(server)); // required for constructing TRI_vocbase_t
    server.addFeature(new arangodb::ShardingFeature(server)); // required for Collections::create(...)
    server.addFeature(sysDatabase = new arangodb::SystemDatabaseFeature(server)); // required for IResearchAnalyzerFeature::start()
    server.addFeature(new arangodb::UpgradeFeature(server, nullptr, {})); // required for upgrade tasks
    server.addFeature(new arangodb::V8DealerFeature(server)); // required for DatabaseFeature::createDatabase(...)
    server.addFeature(new arangodb::application_features::CommunicationFeaturePhase(server)); // required for SimpleHttpClient::doRequest()
    server.addFeature(feature = new arangodb::iresearch::IResearchAnalyzerFeature(server)); // required for running upgrade task
    arangodb::aql::OptimizerRulesFeature(server).prepare(); // required for Query::preparePlan(...)
    auto clearOptimizerRules = irs::make_finally([&s]()->void { arangodb::aql::OptimizerRulesFeature(s.server).unprepare(); });

    // create system vocbase (before feature start)
    {
      auto const databases = arangodb::velocypack::Parser::fromJson(std::string("[ { \"name\": \"") + arangodb::StaticStrings::SystemDatabase + "\" } ]");
      CHECK((TRI_ERROR_NO_ERROR == dbFeature->loadDatabases(databases->slice())));
      sysDatabase->start(); // get system database from DatabaseFeature
    }

    auto system = sysDatabase->use();

    feature->start(); // register upgrade tasks
    server.getFeature<arangodb::ClusterFeature>("Cluster")->prepare(); // create ClusterInfo instance
    server.getFeature<arangodb::ShardingFeature>("Sharding")->prepare(); // required for Collections::create(...), register sharding types
    arangodb::AgencyCommManager::MANAGER->start(); // initialize agency

    ClusterCommMock clusterComm;
    auto scopedClusterComm = ClusterCommMock::setInstance(clusterComm);
    auto* ci = arangodb::ClusterInfo::instance();
    REQUIRE((nullptr != ci));

    // ensure no legacy collection after feature start
    {
      CHECK_THROWS((ci->getCollection(system->name(), LEGACY_ANALYZER_COLLECTION_NAME))); // throws on missing collection
    }

    // simulate heartbeat thread
    // (create dbserver in current) required by ClusterMethods::persistCollectionInAgency(...)
    // (create database in plan) required for ClusterInfo::createCollectionCoordinator(...)
    // (create collection in current) required by ClusterMethods::persistCollectionInAgency(...)
    // (create dummy collection in plan to fill ClusterInfo::_shardServers) required by ClusterMethods::persistCollectionInAgency(...)
    {
      auto const srvPath = "/Current/DBServers";
      auto const srvValue = arangodb::velocypack::Parser::fromJson("{ \"dbserver-key-does-not-matter\": \"dbserver-value-does-not-matter\" }");
      CHECK(arangodb::AgencyComm().setValue(srvPath, srvValue->slice(), 0.0).successful());
      auto const dbPath = "/Plan/Databases/testVocbase";
      auto const dbValue = arangodb::velocypack::Parser::fromJson("null"); // value does not matter
      CHECK(arangodb::AgencyComm().setValue(dbPath, dbValue->slice(), 0.0).successful());
      auto const colPath0 = "/Current/Collections/_system/2000003"; // '2000003' must match what ClusterInfo generates for LogicalCollection::id() or collection creation request will never get executed (use 'collectionID' from ClusterInfo::createCollectionCoordinator(...) in stack trace)
      auto const colValue0 = arangodb::velocypack::Parser::fromJson("{ \"same-as-dummy-shard-id\": { \"servers\": [ \"same-as-dummy-shard-server\" ] } }");
      CHECK(arangodb::AgencyComm().setValue(colPath0, colValue0->slice(), 0.0).successful());
      auto const colPath1 = "/Current/Collections/testVocbase/2000019"; // '2000019 must match what ClusterInfo generates for LogicalCollection::id() or collection creation request will never get executed (use 'collectionID' from ClusterInfo::createCollectionCoordinator(...) in stack trace)
      auto const colValue1 = arangodb::velocypack::Parser::fromJson("{ \"same-as-dummy-shard-id\": { \"servers\": [ \"same-as-dummy-shard-server\" ] } }");
      CHECK(arangodb::AgencyComm().setValue(colPath1, colValue1->slice(), 0.0).successful());
      auto const dummyPath = "/Plan/Collections";
      auto const dummyValue = arangodb::velocypack::Parser::fromJson("{ \"_system\": { \"collection-id-does-not-matter\": { \"name\": \"dummy\", \"shards\": { \"same-as-dummy-shard-id\": [ \"same-as-dummy-shard-server\" ] } } }, \"testVocbase\": { \"collection-id-does-not-matter\": { \"name\": \"dummy\", \"shards\": { \"same-as-dummy-shard-id\": [ \"same-as-dummy-shard-server\" ] } } } }");
      CHECK(arangodb::AgencyComm().setValue(dummyPath, dummyValue->slice(), 0.0).successful());
      auto const versionPath = "/Plan/Version";
      auto const versionValue = arangodb::velocypack::Parser::fromJson(std::to_string(ci->getPlanVersion() + 1));
      CHECK((arangodb::AgencyComm().setValue(versionPath, versionValue->slice(), 0.0).successful())); // force loadPlan() update
      ci->invalidateCurrent(); // force reload of 'Current'
    }

    auto expected = EXPECTED_LEGACY_ANALYZERS;
    TRI_vocbase_t* vocbase;
    CHECK((TRI_ERROR_NO_ERROR == dbFeature->createDatabase(1, "testVocbase", vocbase)));

    // insert responses for the legacy static analyzers
    for (size_t i = 0, count = EXPECTED_LEGACY_ANALYZERS.size(); i < count; ++i) {
      arangodb::ClusterCommResult response;
      response.operationID = i + 1; // sequential non-zero value
      response.status = arangodb::ClusterCommOpStatus::CL_COMM_RECEIVED;
      response.answer_code = arangodb::rest::ResponseCode::CREATED;
      response.answer = std::make_shared<GeneralRequestMock>(*vocbase);
      static_cast<GeneralRequestMock*>(response.answer.get())->_payload = *arangodb::velocypack::Parser::fromJson(std::string("{ \"_key\": \"") + std::to_string(response.operationID) + "\" }"); // unique arbitrary key
      clusterComm._responses.emplace_back(std::move(response));
    }

    CHECK_THROWS((ci->getCollection(system->name(), LEGACY_ANALYZER_COLLECTION_NAME))); // throws on missing collection
    CHECK_THROWS((ci->getCollection(system->name(), ANALYZER_COLLECTION_NAME))); // throws on missing collection
    CHECK((arangodb::methods::Upgrade::clusterBootstrap(*system).ok())); // run system upgrade
    CHECK_THROWS((ci->getCollection(system->name(), LEGACY_ANALYZER_COLLECTION_NAME))); // throws on missing collection
    CHECK((false == !ci->getCollection(system->name(), ANALYZER_COLLECTION_NAME)));

    for (auto& entry: clusterComm._requests) {
      CHECK((entry._body));
      auto body = arangodb::velocypack::Parser::fromJson(*(entry._body));
      auto slice = body->slice();
      CHECK((slice.isObject()));
      CHECK((slice.get("name").isString()));
      CHECK((1 == expected.erase(slice.get("name").copyString())));
    }

    CHECK((true == expected.empty()));

    // insert responses for the legacy static analyzers
    for (size_t i = 0, count = EXPECTED_LEGACY_ANALYZERS.size(); i < count; ++i) {
      arangodb::ClusterCommResult response;
      response.operationID = i + 1; // sequential non-zero value
      response.status = arangodb::ClusterCommOpStatus::CL_COMM_RECEIVED;
      response.answer_code = arangodb::rest::ResponseCode::CREATED;
      response.answer = std::make_shared<GeneralRequestMock>(*vocbase);
      static_cast<GeneralRequestMock*>(response.answer.get())->_payload = *arangodb::velocypack::Parser::fromJson(std::string("{ \"_key\": \"") + std::to_string(response.operationID) + "\" }"); // unique arbitrary key
      clusterComm._responses.emplace_back(std::move(response));
    }

    clusterComm._requests.clear();
    expected = EXPECTED_LEGACY_ANALYZERS;
    CHECK_THROWS((ci->getCollection(vocbase->name(), ANALYZER_COLLECTION_NAME))); // throws on missing collection
    CHECK((arangodb::methods::Upgrade::clusterBootstrap(*vocbase).ok())); // run upgrade
    CHECK((false == !ci->getCollection(vocbase->name(), ANALYZER_COLLECTION_NAME)));
    CHECK((true == clusterComm._responses.empty()));

    for (auto& entry: clusterComm._requests) {
      CHECK((entry._body));
      auto body = arangodb::velocypack::Parser::fromJson(*(entry._body));
      auto slice = body->slice();
      CHECK((slice.isObject()));
      CHECK((slice.get("name").isString()));
      CHECK((1 == expected.erase(slice.get("name").copyString())));
    }

    CHECK((true == expected.empty()));
  }

  // test system, no legacy collection, with analyzer collection (coordinator)
  {
    auto serverRoleBefore = arangodb::ServerState::instance()->getRole();
    arangodb::ServerState::instance()->setRole(arangodb::ServerState::ROLE_COORDINATOR);
    auto serverRoleRestore = irs::make_finally([&serverRoleBefore]()->void { arangodb::ServerState::instance()->setRole(serverRoleBefore); });

    // create a new instance of an ApplicationServer and fill it with the required features
    // cannot use the existing server since its features already have some state
    std::shared_ptr<arangodb::application_features::ApplicationServer> originalServer(
      arangodb::application_features::ApplicationServer::server,
      [](arangodb::application_features::ApplicationServer* ptr)->void {
        arangodb::application_features::ApplicationServer::server = ptr;
      }
    );
    arangodb::application_features::ApplicationServer::server = nullptr; // avoid "ApplicationServer initialized twice"
    arangodb::application_features::ApplicationServer server(nullptr, nullptr);
    arangodb::iresearch::IResearchAnalyzerFeature* feature;
    arangodb::DatabaseFeature* dbFeature;
    arangodb::SystemDatabaseFeature* sysDatabase;
    server.addFeature(new arangodb::ClusterFeature(server)); // required to create ClusterInfo instance
    server.addFeature(dbFeature = new arangodb::DatabaseFeature(server)); // required for IResearchAnalyzerFeature::emplace(...)
    server.addFeature(new arangodb::QueryRegistryFeature(server)); // required for constructing TRI_vocbase_t
    server.addFeature(new arangodb::ShardingFeature(server)); // required for Collections::create(...)
    server.addFeature(sysDatabase = new arangodb::SystemDatabaseFeature(server)); // required for IResearchAnalyzerFeature::start()
    server.addFeature(new arangodb::UpgradeFeature(server, nullptr, {})); // required for upgrade tasks
    server.addFeature(new arangodb::V8DealerFeature(server)); // required for DatabaseFeature::createDatabase(...)
    server.addFeature(new arangodb::application_features::CommunicationFeaturePhase(server)); // required for SimpleHttpClient::doRequest()
    server.addFeature(feature = new arangodb::iresearch::IResearchAnalyzerFeature(server)); // required for running upgrade task
    arangodb::aql::OptimizerRulesFeature(server).prepare(); // required for Query::preparePlan(...)
    auto clearOptimizerRules = irs::make_finally([&s]()->void { arangodb::aql::OptimizerRulesFeature(s.server).unprepare(); });

    // create system vocbase (before feature start)
    {
      auto const databases = arangodb::velocypack::Parser::fromJson(std::string("[ { \"name\": \"") + arangodb::StaticStrings::SystemDatabase + "\" } ]");
      CHECK((TRI_ERROR_NO_ERROR == dbFeature->loadDatabases(databases->slice())));
      sysDatabase->start(); // get system database from DatabaseFeature
    }

    auto system = sysDatabase->use();

    feature->start(); // register upgrade tasks
    server.getFeature<arangodb::ClusterFeature>("Cluster")->prepare(); // create ClusterInfo instance
    server.getFeature<arangodb::ShardingFeature>("Sharding")->prepare(); // required for Collections::create(...), register sharding types
    arangodb::AgencyCommManager::MANAGER->start(); // initialize agency

    ClusterCommMock clusterComm;
    auto scopedClusterComm = ClusterCommMock::setInstance(clusterComm);
    auto* ci = arangodb::ClusterInfo::instance();
    REQUIRE((nullptr != ci));

    // ensure no legacy collection after feature start
    {
      CHECK_THROWS((ci->getCollection(system->name(), LEGACY_ANALYZER_COLLECTION_NAME))); // throws on missing collection
    }

    // simulate heartbeat thread
    // (create dbserver in current) required by ClusterMethods::persistCollectionInAgency(...)
    // (create database in plan) required for ClusterInfo::createCollectionCoordinator(...)
    // (create collection in current) required by ClusterMethods::persistCollectionInAgency(...)
    // (create dummy collection in plan to fill ClusterInfo::_shardServers) required by ClusterMethods::persistCollectionInAgency(...)
    {
      auto const srvPath = "/Current/DBServers";
      auto const srvValue = arangodb::velocypack::Parser::fromJson("{ \"dbserver-key-does-not-matter\": \"dbserver-value-does-not-matter\" }");
      CHECK(arangodb::AgencyComm().setValue(srvPath, srvValue->slice(), 0.0).successful());
      auto const dbPath = "/Plan/Databases/testVocbase";
      auto const dbValue = arangodb::velocypack::Parser::fromJson("null"); // value does not matter
      CHECK(arangodb::AgencyComm().setValue(dbPath, dbValue->slice(), 0.0).successful());
      auto const colPath0 = "/Current/Collections/_system/3000006"; // '3000006' must match what ClusterInfo generates for LogicalCollection::id() or collection creation request will never get executed (use 'collectionID' from ClusterInfo::createCollectionCoordinator(...) in stack trace)
      auto const colValue0 = arangodb::velocypack::Parser::fromJson("{ \"same-as-dummy-shard-id\": { \"servers\": [ \"same-as-dummy-shard-server\" ] } }");
      CHECK(arangodb::AgencyComm().setValue(colPath0, colValue0->slice(), 0.0).successful());
      auto const colPath = "/Current/Collections/testVocbase/3000008"; // '3000008' must match what ClusterInfo generates for LogicalCollection::id() or collection creation request will never get executed (use 'collectionID' from ClusterInfo::createCollectionCoordinator(...) in stack trace)
      auto const colValue = arangodb::velocypack::Parser::fromJson("{ \"same-as-dummy-shard-id\": { \"servers\": [ \"same-as-dummy-shard-server\" ] } }");
      CHECK(arangodb::AgencyComm().setValue(colPath, colValue->slice(), 0.0).successful());
      auto const dummyPath = "/Plan/Collections";
      auto const dummyValue = arangodb::velocypack::Parser::fromJson("{ \"_system\": { \"collection-id-does-not-matter\": { \"name\": \"dummy\", \"shards\": { \"same-as-dummy-shard-id\": [ \"same-as-dummy-shard-server\" ] } } }, \"testVocbase\": { \"collection-id-does-not-matter\": { \"name\": \"dummy\", \"shards\": { \"same-as-dummy-shard-id\": [ \"same-as-dummy-shard-server\" ] } } } }");
      CHECK(arangodb::AgencyComm().setValue(dummyPath, dummyValue->slice(), 0.0).successful());
      auto const versionPath = "/Plan/Version";
      auto const versionValue = arangodb::velocypack::Parser::fromJson(std::to_string(ci->getPlanVersion() + 1));
      CHECK((arangodb::AgencyComm().setValue(versionPath, versionValue->slice(), 0.0).successful())); // force loadPlan() update
    }

    auto expected = EXPECTED_LEGACY_ANALYZERS;
    TRI_vocbase_t* vocbase;
    CHECK((TRI_ERROR_NO_ERROR == dbFeature->createDatabase(1, "testVocbase", vocbase)));
    CHECK((ci->createCollectionCoordinator(vocbase->name(), collectionId, 0, 1, false, createCollectionJson->slice(), 0.0).ok()));
    CHECK((false == !ci->getCollection(vocbase->name(), collectionId)));

    // simulate heartbeat thread (create analyzer collection)
    {
      auto const path = "/Plan/Collections";
      auto const value = arangodb::velocypack::Parser::fromJson(std::string("{ \"") + vocbase->name() + "\": { \"3000008\": { \"name\": \"" + ANALYZER_COLLECTION_NAME + "\", \"isSystem\": true, \"shards\": { \"same-as-dummy-shard-id\": [ \"same-as-dummy-shard-server\" ] } } } }"); // must match what ClusterInfo generates for LogicalCollection::id() or shard list retrieval will fail (use 'collectionID' from ClusterInfo::getShardList(...) in stack trace)
      CHECK(arangodb::AgencyComm().setValue(path, value->slice(), 0.0).successful());
    }

    CHECK((false == !ci->getCollection(vocbase->name(), ANALYZER_COLLECTION_NAME)));

    // insert responses for the legacy static analyzers
    for (size_t i = 0, count = EXPECTED_LEGACY_ANALYZERS.size(); i < count; ++i) {
      arangodb::ClusterCommResult response;
      response.operationID = i + 1; // sequential non-zero value
      response.status = arangodb::ClusterCommOpStatus::CL_COMM_RECEIVED;
      response.answer_code = arangodb::rest::ResponseCode::CREATED;
      response.answer = std::make_shared<GeneralRequestMock>(*vocbase);
      static_cast<GeneralRequestMock*>(response.answer.get())->_payload = *arangodb::velocypack::Parser::fromJson(std::string("{ \"_key\": \"") + std::to_string(response.operationID) + "\" }"); // unique arbitrary key
      clusterComm._responses.emplace_back(std::move(response));
    }

    CHECK_THROWS((ci->getCollection(system->name(), LEGACY_ANALYZER_COLLECTION_NAME))); // throws on missing collection
    CHECK_THROWS((ci->getCollection(system->name(), ANALYZER_COLLECTION_NAME))); // throws on missing collection
    CHECK((arangodb::methods::Upgrade::clusterBootstrap(*system).ok())); // run system upgrade
    CHECK_THROWS((ci->getCollection(system->name(), LEGACY_ANALYZER_COLLECTION_NAME))); // throws on missing collection
    CHECK((false == !ci->getCollection(system->name(), ANALYZER_COLLECTION_NAME)));

    for (auto& entry: clusterComm._requests) {
      CHECK((entry._body));
      auto body = arangodb::velocypack::Parser::fromJson(*(entry._body));
      auto slice = body->slice();
      CHECK((slice.isObject()));
      CHECK((slice.get("name").isString()));
      CHECK((1 == expected.erase(slice.get("name").copyString())));
    }

    CHECK((true == expected.empty()));

    // insert response for expected analyzer lookup
    {
      arangodb::ClusterCommResult response;
      response.status = arangodb::ClusterCommOpStatus::CL_COMM_RECEIVED;
      response.result = std::make_shared<arangodb::httpclient::SimpleHttpResult>();
      response.result->getBody().appendText("{ \"result\": { \"snippets\": { \"6:shard-id-does-not-matter\": \"value-does-not-matter\" } } }").ensureNullTerminated(); // '6' must match GATHER Node id in ExecutionEngine::createBlocks(...)
      clusterComm._responses.emplace_back(std::move(response));
    }

    // insert response for expected analyzer reload from collection
    {
      arangodb::ClusterCommResult response;
      response.status = arangodb::ClusterCommOpStatus::CL_COMM_SENT;
      response.result = std::make_shared<arangodb::httpclient::SimpleHttpResult>();
      response.result->getBody().appendText("{ \"done\": true, \"nrItems\": 1, \"nrRegs\": 1, \"data\": [ 1 ], \"raw\": [ null, null, { \"_key\": \"key-does-not-matter\", \"name\": \"test_analyzer1\", \"type\": \"TestAnalyzer\", \"properties\": \"abc\" } ] }").ensureNullTerminated(); // 'data' value must be 1 as per AqlItemBlock::AqlItemBlock(...), first 2 'raw' values ignored, 'nrRegs' must be 1 or assertion failure in ExecutionBlockImpl<Executor>::requestWrappedBlock(...)
      clusterComm._responses.emplace_back(std::move(response));
    }

    // insert responses for the legacy static analyzers
    for (size_t i = 0, count = EXPECTED_LEGACY_ANALYZERS.size(); i < count; ++i) {
      arangodb::ClusterCommResult response;
      response.operationID = i + 1; // sequential non-zero value
      response.status = arangodb::ClusterCommOpStatus::CL_COMM_RECEIVED;
      response.answer_code = arangodb::rest::ResponseCode::CREATED;
      response.answer = std::make_shared<GeneralRequestMock>(*vocbase);
      static_cast<GeneralRequestMock*>(response.answer.get())->_payload = *arangodb::velocypack::Parser::fromJson(std::string("{ \"_key\": \"") + std::to_string(response.operationID) + "\" }"); // unique arbitrary key
      clusterComm._responses.emplace_back(std::move(response));
    }

    clusterComm._requests.clear();
    expected = EXPECTED_LEGACY_ANALYZERS;
    CHECK((false == !ci->getCollection(vocbase->name(), ANALYZER_COLLECTION_NAME)));
    CHECK((arangodb::methods::Upgrade::clusterBootstrap(*vocbase).ok())); // run upgrade
    CHECK((false == !ci->getCollection(vocbase->name(), ANALYZER_COLLECTION_NAME)));
    CHECK((true == clusterComm._responses.empty()));

    for (size_t i = 2, count = clusterComm._requests.size(); i < count; ++i) { // +2 to skip requests from loadAnalyzers(...)
      auto& entry = clusterComm._requests[i];
      CHECK((entry._body));
      auto body = arangodb::velocypack::Parser::fromJson(*(entry._body));
      auto slice = body->slice();
      CHECK((slice.isObject()));
      CHECK((slice.get("name").isString()));
      CHECK((1 == expected.erase(slice.get("name").copyString())));
    }

    CHECK((true == expected.empty())); // expect only analyzers inserted by upgrade (since checking '_requests')
  }

  // test system, with legacy collection, no analyzer collection (coordinator)
  {
    auto serverRoleBefore = arangodb::ServerState::instance()->getRole();
    arangodb::ServerState::instance()->setRole(arangodb::ServerState::ROLE_COORDINATOR);
    auto serverRoleRestore = irs::make_finally([&serverRoleBefore]()->void { arangodb::ServerState::instance()->setRole(serverRoleBefore); });

    // create a new instance of an ApplicationServer and fill it with the required features
    // cannot use the existing server since its features already have some state
    std::shared_ptr<arangodb::application_features::ApplicationServer> originalServer(
      arangodb::application_features::ApplicationServer::server,
      [](arangodb::application_features::ApplicationServer* ptr)->void {
        arangodb::application_features::ApplicationServer::server = ptr;
      }
    );
    arangodb::application_features::ApplicationServer::server = nullptr; // avoid "ApplicationServer initialized twice"
    arangodb::application_features::ApplicationServer server(nullptr, nullptr);
    arangodb::iresearch::IResearchAnalyzerFeature* feature;
    arangodb::DatabaseFeature* dbFeature;
    arangodb::SystemDatabaseFeature* sysDatabase;
    server.addFeature(new arangodb::ClusterFeature(server)); // required to create ClusterInfo instance
    server.addFeature(dbFeature = new arangodb::DatabaseFeature(server)); // required for IResearchAnalyzerFeature::emplace(...)
    server.addFeature(new arangodb::QueryRegistryFeature(server)); // required for constructing TRI_vocbase_t
    server.addFeature(new arangodb::ShardingFeature(server)); // required for Collections::create(...)
    server.addFeature(sysDatabase = new arangodb::SystemDatabaseFeature(server)); // required for IResearchAnalyzerFeature::start()
    server.addFeature(new arangodb::UpgradeFeature(server, nullptr, {})); // required for upgrade tasks
    server.addFeature(new arangodb::V8DealerFeature(server)); // required for DatabaseFeature::createDatabase(...)
    server.addFeature(new arangodb::application_features::CommunicationFeaturePhase(server)); // required for SimpleHttpClient::doRequest()
    server.addFeature(feature = new arangodb::iresearch::IResearchAnalyzerFeature(server)); // required for running upgrade task
    arangodb::aql::OptimizerRulesFeature(server).prepare(); // required for Query::preparePlan(...)
    auto clearOptimizerRules = irs::make_finally([&s]()->void { arangodb::aql::OptimizerRulesFeature(s.server).unprepare(); });

    // create system vocbase (before feature start)
    {
      auto const databases = arangodb::velocypack::Parser::fromJson(std::string("[ { \"name\": \"") + arangodb::StaticStrings::SystemDatabase + "\" } ]");
      CHECK((TRI_ERROR_NO_ERROR == dbFeature->loadDatabases(databases->slice())));
      sysDatabase->start(); // get system database from DatabaseFeature
    }

    auto system = sysDatabase->use();

    feature->start(); // register upgrade tasks
    server.getFeature<arangodb::ClusterFeature>("Cluster")->prepare(); // create ClusterInfo instance
    server.getFeature<arangodb::ShardingFeature>("Sharding")->prepare(); // required for Collections::create(...), register sharding types
    arangodb::AgencyCommManager::MANAGER->start(); // initialize agency

    ClusterCommMock clusterComm;
    auto scopedClusterComm = ClusterCommMock::setInstance(clusterComm);
    auto* ci = arangodb::ClusterInfo::instance();
    REQUIRE((nullptr != ci));

    // simulate heartbeat thread (create legacy analyzer collection after feature start)
    {
      auto const path = "/Plan/Collections";
      auto const value = arangodb::velocypack::Parser::fromJson(std::string("{ \"") + system->name() + "\": { \"" + LEGACY_ANALYZER_COLLECTION_NAME + "\": { \"name\": \"" + LEGACY_ANALYZER_COLLECTION_NAME + "\", \"isSystem\": true, \"shards\": { \"same-as-dummy-shard-id\": [ \"same-as-dummy-shard-server\" ] } } } }"); // collection ID must match id used in dropCollectionCoordinator(...)
      CHECK(arangodb::AgencyComm().setValue(path, value->slice(), 0.0).successful());
    }

    // ensure legacy collection after feature start
    {
      REQUIRE((false == !ci->getCollection(system->name(), LEGACY_ANALYZER_COLLECTION_NAME)));
    }

    // simulate heartbeat thread
    // (create dbserver in current) required by ClusterMethods::persistCollectionInAgency(...)
    // (create database in plan) required for ClusterInfo::createCollectionCoordinator(...)
    // (create collection in current) required by ClusterMethods::persistCollectionInAgency(...)
    // (create dummy collection in plan to fill ClusterInfo::_shardServers) required by ClusterMethods::persistCollectionInAgency(...)
    {
      auto const srvPath = "/Current/DBServers";
      auto const srvValue = arangodb::velocypack::Parser::fromJson("{ \"dbserver-key-does-not-matter\": \"dbserver-value-does-not-matter\" }");
      CHECK(arangodb::AgencyComm().setValue(srvPath, srvValue->slice(), 0.0).successful());
      auto const dbPath = "/Plan/Databases/testVocbase";
      auto const dbValue = arangodb::velocypack::Parser::fromJson("null"); // value does not matter
      CHECK(arangodb::AgencyComm().setValue(dbPath, dbValue->slice(), 0.0).successful());
      auto const colPath0 = "/Current/Collections/_system/4000004"; // '4000004' must match what ClusterInfo generates for LogicalCollection::id() or collection creation request will never get executed (use 'collectionID' from ClusterInfo::createCollectionCoordinator(...) in stack trace)
      auto const colValue0 = arangodb::velocypack::Parser::fromJson("{ \"same-as-dummy-shard-id\": { \"servers\": [ \"same-as-dummy-shard-server\" ] } }");
      CHECK(arangodb::AgencyComm().setValue(colPath0, colValue0->slice(), 0.0).successful());
      auto const colPath = "/Current/Collections/testVocbase/4000020"; // '4000020' must match what ClusterInfo generates for LogicalCollection::id() or collection creation request will never get executed (use 'collectionID' from ClusterInfo::createCollectionCoordinator(...) in stack trace)
      auto const colValue = arangodb::velocypack::Parser::fromJson("{ \"same-as-dummy-shard-id\": { \"servers\": [ \"same-as-dummy-shard-server\" ] } }");
      CHECK(arangodb::AgencyComm().setValue(colPath, colValue->slice(), 0.0).successful());
      auto const dummyPath = "/Plan/Collections";
      auto const dummyValue = arangodb::velocypack::Parser::fromJson("{ \"_system\": { \"collection-id-does-not-matter\": { \"name\": \"dummy\", \"shards\": { \"same-as-dummy-shard-id\": [ \"same-as-dummy-shard-server\" ] } } }, \"testVocbase\": { \"collection-id-does-not-matter\": { \"name\": \"dummy\", \"shards\": { \"same-as-dummy-shard-id\": [ \"same-as-dummy-shard-server\" ] } } } }");
      CHECK(arangodb::AgencyComm().setValue(dummyPath, dummyValue->slice(), 0.0).successful());
      auto const versionPath = "/Plan/Version";
      auto const versionValue = arangodb::velocypack::Parser::fromJson(std::to_string(ci->getPlanVersion() + 1));
      CHECK((arangodb::AgencyComm().setValue(versionPath, versionValue->slice(), 0.0).successful())); // force loadPlan() update
    }

    auto expected = EXPECTED_LEGACY_ANALYZERS;
    TRI_vocbase_t* vocbase;
    CHECK((TRI_ERROR_NO_ERROR == dbFeature->createDatabase(1, "testVocbase", vocbase)));

    // insert responses for the legacy static analyzers
    for (size_t i = 0, count = EXPECTED_LEGACY_ANALYZERS.size(); i < count; ++i) {
      arangodb::ClusterCommResult response;
      response.operationID = i + 1; // sequential non-zero value
      response.status = arangodb::ClusterCommOpStatus::CL_COMM_RECEIVED;
      response.answer_code = arangodb::rest::ResponseCode::CREATED;
      response.answer = std::make_shared<GeneralRequestMock>(*vocbase);
      static_cast<GeneralRequestMock*>(response.answer.get())->_payload = *arangodb::velocypack::Parser::fromJson(std::string("{ \"_key\": \"") + std::to_string(response.operationID) + "\" }"); // unique arbitrary key
      clusterComm._responses.emplace_back(std::move(response));
    }

    CHECK((false == !ci->getCollection(system->name(), LEGACY_ANALYZER_COLLECTION_NAME)));
    CHECK_THROWS((ci->getCollection(system->name(), ANALYZER_COLLECTION_NAME))); // throws on missing collection
    CHECK((arangodb::methods::Upgrade::clusterBootstrap(*system).ok())); // run system upgrade
    CHECK_THROWS((ci->getCollection(system->name(), LEGACY_ANALYZER_COLLECTION_NAME))); // throws on missing collection
    CHECK((false == !ci->getCollection(system->name(), ANALYZER_COLLECTION_NAME)));

    for (auto& entry: clusterComm._requests) {
      CHECK((entry._body));
      auto body = arangodb::velocypack::Parser::fromJson(*(entry._body));
      auto slice = body->slice();
      CHECK((slice.isObject()));
      CHECK((slice.get("name").isString()));
      CHECK((1 == expected.erase(slice.get("name").copyString())));
    }

    CHECK((true == expected.empty()));

    // insert responses for the legacy static analyzers
    for (size_t i = 0, count = EXPECTED_LEGACY_ANALYZERS.size(); i < count; ++i) {
      arangodb::ClusterCommResult response;
      response.operationID = i + 1; // sequential non-zero value
      response.status = arangodb::ClusterCommOpStatus::CL_COMM_RECEIVED;
      response.answer_code = arangodb::rest::ResponseCode::CREATED;
      response.answer = std::make_shared<GeneralRequestMock>(*vocbase);
      static_cast<GeneralRequestMock*>(response.answer.get())->_payload = *arangodb::velocypack::Parser::fromJson(std::string("{ \"_key\": \"") + std::to_string(response.operationID) + "\" }"); // unique arbitrary key
      clusterComm._responses.emplace_back(std::move(response));
    }

    clusterComm._requests.clear();
    expected = EXPECTED_LEGACY_ANALYZERS;
    CHECK_THROWS((ci->getCollection(vocbase->name(), ANALYZER_COLLECTION_NAME))); // throws on missing collection
    CHECK((arangodb::methods::Upgrade::clusterBootstrap(*vocbase).ok())); // run upgrade
    CHECK((false == !ci->getCollection(vocbase->name(), ANALYZER_COLLECTION_NAME)));
    CHECK((true == clusterComm._responses.empty()));

    for (auto& entry: clusterComm._requests) {
      CHECK((entry._body));
      auto body = arangodb::velocypack::Parser::fromJson(*(entry._body));
      auto slice = body->slice();
      CHECK((slice.isObject()));
      CHECK((slice.get("name").isString()));
      CHECK((1 == expected.erase(slice.get("name").copyString())));
    }

    CHECK((true == expected.empty()));
  }

  // test system, with legacy collection, with analyzer collection (coordinator)
  {
    auto serverRoleBefore = arangodb::ServerState::instance()->getRole();
    arangodb::ServerState::instance()->setRole(arangodb::ServerState::ROLE_COORDINATOR);
    auto serverRoleRestore = irs::make_finally([&serverRoleBefore]()->void { arangodb::ServerState::instance()->setRole(serverRoleBefore); });

    // create a new instance of an ApplicationServer and fill it with the required features
    // cannot use the existing server since its features already have some state
    std::shared_ptr<arangodb::application_features::ApplicationServer> originalServer(
      arangodb::application_features::ApplicationServer::server,
      [](arangodb::application_features::ApplicationServer* ptr)->void {
        arangodb::application_features::ApplicationServer::server = ptr;
      }
    );
    arangodb::application_features::ApplicationServer::server = nullptr; // avoid "ApplicationServer initialized twice"
    arangodb::application_features::ApplicationServer server(nullptr, nullptr);
    arangodb::iresearch::IResearchAnalyzerFeature* feature;
    arangodb::DatabaseFeature* dbFeature;
    arangodb::SystemDatabaseFeature* sysDatabase;
    server.addFeature(new arangodb::ClusterFeature(server)); // required to create ClusterInfo instance
    server.addFeature(dbFeature = new arangodb::DatabaseFeature(server)); // required for IResearchAnalyzerFeature::emplace(...)
    server.addFeature(new arangodb::QueryRegistryFeature(server)); // required for constructing TRI_vocbase_t
    server.addFeature(new arangodb::ShardingFeature(server)); // required for Collections::create(...)
    server.addFeature(sysDatabase = new arangodb::SystemDatabaseFeature(server)); // required for IResearchAnalyzerFeature::start()
    server.addFeature(new arangodb::UpgradeFeature(server, nullptr, {})); // required for upgrade tasks
    server.addFeature(new arangodb::V8DealerFeature(server)); // required for DatabaseFeature::createDatabase(...)
    server.addFeature(new arangodb::application_features::CommunicationFeaturePhase(server)); // required for SimpleHttpClient::doRequest()
    server.addFeature(feature = new arangodb::iresearch::IResearchAnalyzerFeature(server)); // required for running upgrade task
    arangodb::aql::OptimizerRulesFeature(server).prepare(); // required for Query::preparePlan(...)
    auto clearOptimizerRules = irs::make_finally([&s]()->void { arangodb::aql::OptimizerRulesFeature(s.server).unprepare(); });

    // create system vocbase (before feature start)
    {
      auto const databases = arangodb::velocypack::Parser::fromJson(std::string("[ { \"name\": \"") + arangodb::StaticStrings::SystemDatabase + "\" } ]");
      CHECK((TRI_ERROR_NO_ERROR == dbFeature->loadDatabases(databases->slice())));
      sysDatabase->start(); // get system database from DatabaseFeature
    }

    auto system = sysDatabase->use();

    feature->start(); // register upgrade tasks
    server.getFeature<arangodb::ClusterFeature>("Cluster")->prepare(); // create ClusterInfo instance
    server.getFeature<arangodb::ShardingFeature>("Sharding")->prepare(); // required for Collections::create(...), register sharding types
    arangodb::AgencyCommManager::MANAGER->start(); // initialize agency

    ClusterCommMock clusterComm;
    auto scopedClusterComm = ClusterCommMock::setInstance(clusterComm);
    auto* ci = arangodb::ClusterInfo::instance();
    REQUIRE((nullptr != ci));

    // simulate heartbeat thread (create legacy analyzer collection after feature start)
    {
      auto const path = "/Plan/Collections";
      auto const value = arangodb::velocypack::Parser::fromJson(std::string("{ \"") + system->name() + "\": { \"" + LEGACY_ANALYZER_COLLECTION_NAME + "\": { \"name\": \"" + LEGACY_ANALYZER_COLLECTION_NAME + "\", \"isSystem\": true, \"shards\": { \"same-as-dummy-shard-id\": [ \"same-as-dummy-shard-server\" ] } } } }"); // collection ID must match id used in dropCollectionCoordinator(...)
      CHECK(arangodb::AgencyComm().setValue(path, value->slice(), 0.0).successful());
    }

    // ensure legacy collection after feature start
    {
      REQUIRE((false == !ci->getCollection(system->name(), LEGACY_ANALYZER_COLLECTION_NAME)));
    }

    // simulate heartbeat thread
    // (create dbserver in current) required by ClusterMethods::persistCollectionInAgency(...)
    // (create database in plan) required for ClusterInfo::createCollectionCoordinator(...)
    // (create collection in current) required by ClusterMethods::persistCollectionInAgency(...)
    // (create dummy collection in plan to fill ClusterInfo::_shardServers) required by ClusterMethods::persistCollectionInAgency(...)
    {
      auto const srvPath = "/Current/DBServers";
      auto const srvValue = arangodb::velocypack::Parser::fromJson("{ \"dbserver-key-does-not-matter\": \"dbserver-value-does-not-matter\" }");
      CHECK(arangodb::AgencyComm().setValue(srvPath, srvValue->slice(), 0.0).successful());
      auto const dbPath = "/Plan/Databases/testVocbase";
      auto const dbValue = arangodb::velocypack::Parser::fromJson("null"); // value does not matter
      CHECK(arangodb::AgencyComm().setValue(dbPath, dbValue->slice(), 0.0).successful());
      auto const colPath0 = "/Current/Collections/_system/5000007"; // '5000007' must match what ClusterInfo generates for LogicalCollection::id() or collection creation request will never get executed (use 'collectionID' from ClusterInfo::createCollectionCoordinator(...) in stack trace)
      auto const colValue0 = arangodb::velocypack::Parser::fromJson("{ \"same-as-dummy-shard-id\": { \"servers\": [ \"same-as-dummy-shard-server\" ] } }");
      CHECK(arangodb::AgencyComm().setValue(colPath0, colValue0->slice(), 0.0).successful());
      auto const colPath = "/Current/Collections/testVocbase/5000009"; // '5000009' must match what ClusterInfo generates for LogicalCollection::id() or collection creation request will never get executed (use 'collectionID' from ClusterInfo::createCollectionCoordinator(...) in stack trace)
      auto const colValue = arangodb::velocypack::Parser::fromJson("{ \"same-as-dummy-shard-id\": { \"servers\": [ \"same-as-dummy-shard-server\" ] } }");
      CHECK(arangodb::AgencyComm().setValue(colPath, colValue->slice(), 0.0).successful());
      auto const dummyPath = "/Plan/Collections";
      auto const dummyValue = arangodb::velocypack::Parser::fromJson("{ \"testVocbase\": { \"collection-id-does-not-matter\": { \"name\": \"dummy\", \"shards\": { \"same-as-dummy-shard-id\": [ \"same-as-dummy-shard-server\" ] } } } }");
      CHECK(arangodb::AgencyComm().setValue(dummyPath, dummyValue->slice(), 0.0).successful());
      auto const versionPath = "/Plan/Version";
      auto const versionValue = arangodb::velocypack::Parser::fromJson(std::to_string(ci->getPlanVersion() + 1));
      CHECK((arangodb::AgencyComm().setValue(versionPath, versionValue->slice(), 0.0).successful())); // force loadPlan() update
    }

    auto expected = EXPECTED_LEGACY_ANALYZERS;
    TRI_vocbase_t* vocbase;
    CHECK((TRI_ERROR_NO_ERROR == dbFeature->createDatabase(1, "testVocbase", vocbase)));
    CHECK((ci->createCollectionCoordinator(vocbase->name(), collectionId, 0, 1, false, createCollectionJson->slice(), 0.0).ok()));
    CHECK((false == !ci->getCollection(vocbase->name(), collectionId)));

    // simulate heartbeat thread (create analyzer collection)
    // must also include legacy collection definition otherwise it'll be removed
    {
      auto const path = "/Plan/Collections";
      auto const value = arangodb::velocypack::Parser::fromJson(std::string("{ \"") + system->name() + "\": { \"5000004\": { \"name\": \"" + LEGACY_ANALYZER_COLLECTION_NAME + "\", \"isSystem\": true, \"shards\": { \"same-as-dummy-shard-id\": [ \"same-as-dummy-shard-server\" ] } } }, \"" + vocbase->name() + "\": { \"5000009\": { \"name\": \"" + ANALYZER_COLLECTION_NAME + "\", \"isSystem\": true, \"shards\": { \"same-as-dummy-shard-id\": [ \"same-as-dummy-shard-server\" ] } } } }"); // must match what ClusterInfo generates for LogicalCollection::id() or shard list retrieval will fail (use 'collectionID' from ClusterInfo::getShardList(...) in stack trace)
      CHECK(arangodb::AgencyComm().setValue(path, value->slice(), 0.0).successful());
      auto const versionPath = "/Plan/Version";
      auto const versionValue = arangodb::velocypack::Parser::fromJson(std::to_string(ci->getPlanVersion() + 1));
      CHECK((arangodb::AgencyComm().setValue(versionPath, versionValue->slice(), 0.0).successful())); // force loadPlan() update
    }

    CHECK((false == !ci->getCollection(vocbase->name(), ANALYZER_COLLECTION_NAME)));

    // insert responses for the legacy static analyzers
    for (size_t i = 0, count = EXPECTED_LEGACY_ANALYZERS.size(); i < count; ++i) {
      arangodb::ClusterCommResult response;
      response.operationID = i + 1; // sequential non-zero value
      response.status = arangodb::ClusterCommOpStatus::CL_COMM_RECEIVED;
      response.answer_code = arangodb::rest::ResponseCode::CREATED;
      response.answer = std::make_shared<GeneralRequestMock>(*vocbase);
      static_cast<GeneralRequestMock*>(response.answer.get())->_payload = *arangodb::velocypack::Parser::fromJson(std::string("{ \"_key\": \"") + std::to_string(response.operationID) + "\" }"); // unique arbitrary key
      clusterComm._responses.emplace_back(std::move(response));
    }

    CHECK((false == !ci->getCollection(system->name(), LEGACY_ANALYZER_COLLECTION_NAME)));
    CHECK_THROWS((ci->getCollection(system->name(), ANALYZER_COLLECTION_NAME))); // throws on missing collection
    CHECK((arangodb::methods::Upgrade::clusterBootstrap(*system).ok())); // run system upgrade
    CHECK_THROWS((ci->getCollection(system->name(), LEGACY_ANALYZER_COLLECTION_NAME))); // throws on missing collection
    CHECK((false == !ci->getCollection(system->name(), ANALYZER_COLLECTION_NAME)));

    for (auto& entry: clusterComm._requests) {
      CHECK((entry._body));
      auto body = arangodb::velocypack::Parser::fromJson(*(entry._body));
      auto slice = body->slice();
      CHECK((slice.isObject()));
      CHECK((slice.get("name").isString()));
      CHECK((1 == expected.erase(slice.get("name").copyString())));
    }

    CHECK((true == expected.empty()));

    // insert response for expected analyzer lookup
    {
      arangodb::ClusterCommResult response;
      response.status = arangodb::ClusterCommOpStatus::CL_COMM_RECEIVED;
      response.result = std::make_shared<arangodb::httpclient::SimpleHttpResult>();
      response.result->getBody().appendText("{ \"result\": { \"snippets\": { \"6:shard-id-does-not-matter\": \"value-does-not-matter\" } } }").ensureNullTerminated(); // '6' must match GATHER Node id in ExecutionEngine::createBlocks(...)
      clusterComm._responses.emplace_back(std::move(response));
    }

    // insert response for expected analyzer reload from collection
    {
      arangodb::ClusterCommResult response;
      response.status = arangodb::ClusterCommOpStatus::CL_COMM_SENT;
      response.result = std::make_shared<arangodb::httpclient::SimpleHttpResult>();
      response.result->getBody().appendText("{ \"done\": true, \"nrItems\": 1, \"nrRegs\": 1, \"data\": [ 1 ], \"raw\": [ null, null, { \"_key\": \"key-does-not-matter\", \"name\": \"abc\", \"type\": \"TestAnalyzer\", \"properties\": \"abc\" } ] }").ensureNullTerminated(); // 'data' value must be 1 as per AqlItemBlock::AqlItemBlock(...), first 2 'raw' values ignored, 'nrRegs' must be 1 or assertion failure in ExecutionBlockImpl<Executor>::requestWrappedBlock(...)
      clusterComm._responses.emplace_back(std::move(response));
    }

    // insert responses for the legacy static analyzers
    for (size_t i = 0, count = EXPECTED_LEGACY_ANALYZERS.size(); i < count; ++i) {
      arangodb::ClusterCommResult response;
      response.operationID = i + 1; // sequential non-zero value
      response.status = arangodb::ClusterCommOpStatus::CL_COMM_RECEIVED;
      response.answer_code = arangodb::rest::ResponseCode::CREATED;
      response.answer = std::make_shared<GeneralRequestMock>(*vocbase);
      static_cast<GeneralRequestMock*>(response.answer.get())->_payload = *arangodb::velocypack::Parser::fromJson(std::string("{ \"_key\": \"") + std::to_string(response.operationID) + "\" }"); // unique arbitrary key
      clusterComm._responses.emplace_back(std::move(response));
    }

    clusterComm._requests.clear();
    expected = EXPECTED_LEGACY_ANALYZERS;
    CHECK((false == !ci->getCollection(vocbase->name(), ANALYZER_COLLECTION_NAME)));
    CHECK((arangodb::methods::Upgrade::clusterBootstrap(*vocbase).ok())); // run upgrade
    CHECK((false == !ci->getCollection(vocbase->name(), ANALYZER_COLLECTION_NAME)));
    CHECK((true == clusterComm._responses.empty()));

    for (size_t i = 2, count = clusterComm._requests.size(); i < count; ++i) { // +2 to skip requests from loadAnalyzers(...)
      auto& entry = clusterComm._requests[i];
      CHECK((entry._body));
      auto body = arangodb::velocypack::Parser::fromJson(*(entry._body));
      auto slice = body->slice();
      CHECK((slice.isObject()));
      CHECK((slice.get("name").isString()));
      CHECK((1 == expected.erase(slice.get("name").copyString())));
    }

    CHECK((true == expected.empty())); // expect only analyzers inserted by upgrade (since checking '_requests')
  }
}

SECTION("test_visit") {
  struct ExpectedType {
    irs::flags _features;
    std::string _name;
    std::string _properties;
    ExpectedType(irs::string_ref const& name, irs::string_ref const& properties, irs::flags const& features)
      : _features(features), _name(name), _properties(properties) {
    }
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

      return false; // assume equal
    }
  };

  // create a new instance of an ApplicationServer and fill it with the required features
  // cannot use the existing server since its features already have some state
  std::shared_ptr<arangodb::application_features::ApplicationServer> originalServer(
    arangodb::application_features::ApplicationServer::server,
    [](arangodb::application_features::ApplicationServer* ptr)->void {
      arangodb::application_features::ApplicationServer::server = ptr;
    }
  );
  arangodb::application_features::ApplicationServer::server = nullptr; // avoid "ApplicationServer initialized twice"
  arangodb::application_features::ApplicationServer server(nullptr, nullptr);
  arangodb::iresearch::IResearchAnalyzerFeature feature(server);
  arangodb::DatabaseFeature* dbFeature;
  arangodb::SystemDatabaseFeature* sysDatabase;
  server.addFeature(new arangodb::QueryRegistryFeature(server)); // required for constructing TRI_vocbase_t
  server.addFeature(dbFeature = new arangodb::DatabaseFeature(server)); // required for IResearchAnalyzerFeature::emplace(...)
  server.addFeature(new arangodb::QueryRegistryFeature(server)); // required for constructing TRI_vocbase_t
  server.addFeature(sysDatabase = new arangodb::SystemDatabaseFeature(server)); // required for IResearchAnalyzerFeature::start()
  server.addFeature(new arangodb::V8DealerFeature(server)); // required for DatabaseFeature::createDatabase(...)

  // create system vocbase (before feature start)
  {
    auto const databases = arangodb::velocypack::Parser::fromJson(std::string("[ { \"name\": \"") + arangodb::StaticStrings::SystemDatabase + "\" } ]");
    CHECK((TRI_ERROR_NO_ERROR == dbFeature->loadDatabases(databases->slice())));
    sysDatabase->start(); // get system database from DatabaseFeature
  }

  arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;
  CHECK((true == feature.emplace(result, arangodb::StaticStrings::SystemDatabase + "::test_analyzer0", "TestAnalyzer", "abc0").ok()));
  CHECK((false == !result.first));
  CHECK((true == feature.emplace(result, arangodb::StaticStrings::SystemDatabase + "::test_analyzer1", "TestAnalyzer", "abc1").ok()));
  CHECK((false == !result.first));
  CHECK((true == feature.emplace(result, arangodb::StaticStrings::SystemDatabase + "::test_analyzer2", "TestAnalyzer", "abc2").ok()));
  CHECK((false == !result.first));

  // full visitation
  {
    std::set<ExpectedType> expected = {
      { arangodb::StaticStrings::SystemDatabase + "::test_analyzer0", "abc0", {} },
      { arangodb::StaticStrings::SystemDatabase + "::test_analyzer1", "abc1", {} },
      { arangodb::StaticStrings::SystemDatabase + "::test_analyzer2", "abc2", {} },
    };
    auto result = feature.visit([&expected](
      arangodb::iresearch::IResearchAnalyzerFeature::AnalyzerPool::ptr const& analyzer
    )->bool {
      if (staticAnalyzers().find(analyzer->name()) != staticAnalyzers().end()) {
        return true; // skip static analyzers
      }

      CHECK((analyzer->type() == "TestAnalyzer"));
      CHECK((1 == expected.erase(ExpectedType(analyzer->name(), analyzer->properties(), analyzer->features()))));
      return true;
    });
    CHECK((true == result));
    CHECK((expected.empty()));
  }

  // partial visitation
  {
    std::set<ExpectedType> expected = {
      { arangodb::StaticStrings::SystemDatabase + "::test_analyzer0", "abc0", {} },
      { arangodb::StaticStrings::SystemDatabase + "::test_analyzer1", "abc1", {} },
      { arangodb::StaticStrings::SystemDatabase + "::test_analyzer2", "abc2", {} },
    };
    auto result = feature.visit([&expected](
      arangodb::iresearch::IResearchAnalyzerFeature::AnalyzerPool::ptr const& analyzer
    )->bool {
      if (staticAnalyzers().find(analyzer->name()) != staticAnalyzers().end()) {
        return true; // skip static analyzers
      }

      CHECK((analyzer->type() == "TestAnalyzer"));
      CHECK((1 == expected.erase(ExpectedType(analyzer->name(), analyzer->properties(), analyzer->features()))));
      return false;
    });
    CHECK((false == result));
    CHECK((2 == expected.size()));
  }

  TRI_vocbase_t* vocbase0;
  TRI_vocbase_t* vocbase1;
  TRI_vocbase_t* vocbase2;
  CHECK((TRI_ERROR_NO_ERROR == dbFeature->createDatabase(1, "vocbase0", vocbase0)));
  CHECK((TRI_ERROR_NO_ERROR == dbFeature->createDatabase(1, "vocbase1", vocbase1)));
  CHECK((TRI_ERROR_NO_ERROR == dbFeature->createDatabase(1, "vocbase2", vocbase2)));

  // add database-prefixed analyzers
  {
    arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;
    CHECK((true == feature.emplace(result, "vocbase2::test_analyzer3", "TestAnalyzer", "abc3").ok()));
    CHECK((false == !result.first));
    CHECK((true == feature.emplace(result, "vocbase2::test_analyzer4", "TestAnalyzer", "abc4").ok()));
    CHECK((false == !result.first));
    CHECK((true == feature.emplace(result, "vocbase1::test_analyzer5", "TestAnalyzer", "abc5").ok()));
    CHECK((false == !result.first));
  }

  // full visitation limited to a vocbase (empty)
  {
    std::set<ExpectedType> expected = {};
    auto result = feature.visit(
      [&expected](
        arangodb::iresearch::IResearchAnalyzerFeature::AnalyzerPool::ptr const& analyzer
      )->bool {
        if (staticAnalyzers().find(analyzer->name()) != staticAnalyzers().end()) {
          return true; // skip static analyzers
        }

        CHECK((analyzer->type() == "TestAnalyzer"));
        CHECK((1 == expected.erase(ExpectedType(analyzer->name(), analyzer->properties(), analyzer->features()))));
        return true;
      },
      vocbase0
    );
    CHECK((true == result));
    CHECK((expected.empty()));
  }

  // full visitation limited to a vocbase (non-empty)
  {
    std::set<ExpectedType> expected = {
      { "vocbase2::test_analyzer3", "abc3", {} },
      { "vocbase2::test_analyzer4", "abc4", {} },
    };
    auto result = feature.visit(
      [&expected](
        arangodb::iresearch::IResearchAnalyzerFeature::AnalyzerPool::ptr const& analyzer
      )->bool {
        if (staticAnalyzers().find(analyzer->name()) != staticAnalyzers().end()) {
          return true; // skip static analyzers
        }

        CHECK((analyzer->type() == "TestAnalyzer"));
        CHECK((1 == expected.erase(ExpectedType(analyzer->name(), analyzer->properties(), analyzer->features()))));
        return true;
      },
      vocbase2
    );
    CHECK((true == result));
    CHECK((expected.empty()));
  }

}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate tests
////////////////////////////////////////////////////////////////////////////////

}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
