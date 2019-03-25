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
#include "VocBase/KeyGenerator.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ManagedDocumentResult.h"

namespace {

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
    {"text_de", { "text", "{ \"locale\": \"de.UTF-8\", \"ignored_words\": [ ] }", { irs::frequency::type(), irs::norm::type(), irs::position::type() } } },
    {"text_en", { "text", "{ \"locale\": \"en.UTF-8\", \"ignored_words\": [ ] }", { irs::frequency::type(), irs::norm::type(), irs::position::type() } } },
    {"text_es", { "text", "{ \"locale\": \"es.UTF-8\", \"ignored_words\": [ ] }", { irs::frequency::type(), irs::norm::type(), irs::position::type() } } },
    {"text_fi", { "text", "{ \"locale\": \"fi.UTF-8\", \"ignored_words\": [ ] }", { irs::frequency::type(), irs::norm::type(), irs::position::type() } } },
    {"text_fr", { "text", "{ \"locale\": \"fr.UTF-8\", \"ignored_words\": [ ] }", { irs::frequency::type(), irs::norm::type(), irs::position::type() } } },
    {"text_it", { "text", "{ \"locale\": \"it.UTF-8\", \"ignored_words\": [ ] }", { irs::frequency::type(), irs::norm::type(), irs::position::type() } } },
    {"text_nl", { "text", "{ \"locale\": \"nl.UTF-8\", \"ignored_words\": [ ] }", { irs::frequency::type(), irs::norm::type(), irs::position::type() } } },
    {"text_no", { "text", "{ \"locale\": \"no.UTF-8\", \"ignored_words\": [ ] }", { irs::frequency::type(), irs::norm::type(), irs::position::type() } } },
    {"text_pt", { "text", "{ \"locale\": \"pt.UTF-8\", \"ignored_words\": [ ] }", { irs::frequency::type(), irs::norm::type(), irs::position::type() } } },
    {"text_ru", { "text", "{ \"locale\": \"ru.UTF-8\", \"ignored_words\": [ ] }", { irs::frequency::type(), irs::norm::type(), irs::position::type() } } },
    {"text_sv", { "text", "{ \"locale\": \"sv.UTF-8\", \"ignored_words\": [ ] }", { irs::frequency::type(), irs::norm::type(), irs::position::type() } } },
    {"text_zh", { "text", "{ \"locale\": \"zh.UTF-8\", \"ignored_words\": [ ] }", { irs::frequency::type(), irs::norm::type(), irs::position::type() } } },
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
  std::unique_ptr<Vocbase> system; // ensure destruction after 'server'
  arangodb::application_features::ApplicationServer server;
  std::vector<std::pair<arangodb::application_features::ApplicationFeature*, bool>> features;

  IResearchAnalyzerFeatureSetup(): engine(server), server(nullptr, nullptr) {
    auto* agencyCommManager = new AgencyCommManagerMock("arango");
    agency = agencyCommManager->addConnection<GeneralClientConnectionAgencyMock>(_agencyStore);
    agency = agencyCommManager->addConnection<GeneralClientConnectionAgencyMock>(_agencyStore); // need 2 connections or Agency callbacks will fail
    arangodb::AgencyCommManager::MANAGER.reset(agencyCommManager); // required for Coordinator tests

    arangodb::tests::init();

    // suppress INFO {authentication} Authentication is turned on (system only), authentication for unix sockets is turned on
    arangodb::LogTopic::setLogLevel(arangodb::Logger::AUTHENTICATION.name(), arangodb::LogLevel::WARN);

    // setup required application features
    features.emplace_back(new arangodb::AuthenticationFeature(server), true);
    features.emplace_back(new arangodb::DatabaseFeature(server), false);
    features.emplace_back(new arangodb::ShardingFeature(server), false);
    features.emplace_back(new arangodb::QueryRegistryFeature(server), false); // required for constructing TRI_vocbase_t
    arangodb::application_features::ApplicationServer::server->addFeature(features.back().first); // need QueryRegistryFeature feature to be added now in order to create the system database
    system = irs::memory::make_unique<Vocbase>(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 0, TRI_VOC_SYSTEM_DATABASE); // QueryRegistryFeature required for instantiation
    features.emplace_back(new arangodb::SystemDatabaseFeature(server, system.get()), false); // required for IResearchAnalyzerFeature
    features.emplace_back(new arangodb::aql::AqlFunctionFeature(server), true); // required for IResearchAnalyzerFeature

    #if USE_ENTERPRISE
      features.emplace_back(new arangodb::LdapFeature(server), false); // required for AuthenticationFeature with USE_ENTERPRISE
    #endif

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
    arangodb::iresearch::IResearchAnalyzerFeature feature(s.server);
    feature.start();
    CHECK((false == !feature.emplace("test_analyzer0", "TestAnalyzer", "abc").first));
    auto pool = feature.get("test_analyzer0");
    CHECK((false == !pool));
    CHECK((irs::flags() == pool->features()));
  }

  // add duplicate valid (same name+type+properties)
  {
    arangodb::iresearch::IResearchAnalyzerFeature feature(s.server);
    feature.start();
    CHECK((false == !feature.emplace("test_analyzer1", "TestAnalyzer", "abc", irs::flags{ TestAttribute::type() }).first));
    auto pool = feature.get("test_analyzer1");
    CHECK((false == !pool));
    CHECK((irs::flags({ TestAttribute::type() }) == pool->features()));
    CHECK((false == !feature.emplace("test_analyzer1", "TestAnalyzer", "abc").first));
    CHECK((false == !feature.get("test_analyzer1")));
  }

  // add duplicate invalid (same name+type different properties)
  {
    arangodb::iresearch::IResearchAnalyzerFeature feature(s.server);
    feature.start();
    CHECK((false == !feature.emplace("test_analyzer2", "TestAnalyzer", "abc").first));
    auto pool = feature.get("test_analyzer2");
    CHECK((false == !pool));
    CHECK((irs::flags() == pool->features()));
    CHECK((true == !feature.emplace("test_analyzer2", "TestAnalyzer", "abcd").first));
    CHECK((false == !feature.get("test_analyzer2")));
  }

  // add duplicate invalid (same name+properties different type)
  {
    arangodb::iresearch::IResearchAnalyzerFeature feature(s.server);
    feature.start();
    CHECK((false == !feature.emplace("test_analyzer3", "TestAnalyzer", "abc").first));
    auto pool = feature.get("test_analyzer3");
    CHECK((false == !pool));
    CHECK((irs::flags() == pool->features()));
    CHECK((true == !feature.emplace("test_analyzer3", "invalid", "abc").first));
    CHECK((false == !feature.get("test_analyzer3")));
  }

  // add invalid (instance creation failure)
  {
    arangodb::iresearch::IResearchAnalyzerFeature feature(s.server);
    feature.start();
    CHECK((true == !feature.emplace("test_analyzer4", "TestAnalyzer", "").first));
    CHECK((true == !feature.get("test_analyzer4")));
  }

  // add invalid (instance creation exception)
  {
    arangodb::iresearch::IResearchAnalyzerFeature feature(s.server);
    feature.start();
    CHECK((true == !feature.emplace("test_analyzer5", "TestAnalyzer", irs::string_ref::NIL).first));
    CHECK((true == !feature.get("test_analyzer5")));
  }

  // add invalid (not registred)
  {
    arangodb::iresearch::IResearchAnalyzerFeature feature(s.server);
    feature.start();
    CHECK((true == !feature.emplace("test_analyzer6", "invalid", irs::string_ref::NIL).first));
    CHECK((true == !feature.get("test_analyzer6")));
  }

  // add invalid (feature not started)
  {
    arangodb::iresearch::IResearchAnalyzerFeature feature(s.server);
    CHECK((true == !feature.emplace("test_analyzer7", "TestAnalyzer", "abc").first));
    auto pool = feature.ensure("test_analyzer");
    REQUIRE((false == !pool));
    CHECK((irs::flags::empty_instance() == pool->features()));
    auto analyzer = pool->get();
    CHECK((true == !analyzer));
    CHECK((true == !feature.get("test_analyzer7")));
  }

  // add static analyzer
  {
    arangodb::iresearch::IResearchAnalyzerFeature feature(s.server);
    feature.start();
    CHECK((false == !feature.emplace("identity", "identity", irs::string_ref::NIL).first));
    auto pool = feature.get("identity");
    CHECK((false == !pool));
    CHECK((irs::flags({irs::norm::type(), irs::frequency::type() }) == pool->features()));
    auto analyzer = pool->get();
    CHECK((false == !analyzer));
  }

  // add static analyzer (feature not started)
  {
    arangodb::iresearch::IResearchAnalyzerFeature feature(s.server);
    CHECK((false == !feature.emplace("identity", "identity", irs::string_ref::NIL).first));
    auto pool = feature.get("identity");
    CHECK((false == !pool));
    CHECK((irs::flags({irs::norm::type(), irs::frequency::type() }) == pool->features()));
    auto analyzer = pool->get();
    CHECK((false == !analyzer));
  }
}

SECTION("test_ensure") {
  {
    arangodb::iresearch::IResearchAnalyzerFeature feature(s.server);

    // ensure (not started)
    {
      auto pool = feature.ensure("test_analyzer");
      REQUIRE((false == !pool));
      CHECK((irs::flags::empty_instance() == pool->features()));
      auto analyzer = pool->get();
      CHECK((true == !analyzer));
    }

    // ensure static analyzer (not started)
    {
      auto pool = feature.ensure("identity");
      REQUIRE((false == !pool));
      CHECK((irs::flags({irs::norm::type(), irs::frequency::type()}) == pool->features()));
      auto analyzer = pool->get();
      CHECK((false == !analyzer));
    }
  }

  {
    arangodb::iresearch::IResearchAnalyzerFeature feature(s.server);

    feature.start();
    feature.emplace("test_analyzer", "TestAnalyzer", "abc");

    // ensure valid (started)
    {
      auto pool = feature.get("test_analyzer");
      REQUIRE((false == !pool));
      CHECK((irs::flags() == pool->features()));
      auto analyzer = pool.get();
      CHECK((false == !analyzer));
    }

    // ensure invalid (started)
    {
      CHECK((true == !feature.get("invalid")));
    }

    // ensure static analyzer (started)
    {
      auto pool = feature.ensure("identity");
      REQUIRE((false == !pool));
      CHECK((irs::flags({irs::norm::type(), irs::frequency::type() }) == pool->features()));
      auto analyzer = pool->get();
      CHECK((false == !analyzer));
    }
  }
}

SECTION("test_get") {
  {
    arangodb::iresearch::IResearchAnalyzerFeature feature(s.server);

    feature.ensure("test_analyzer");

    // get valid (not started)
    {
      auto pool = feature.get("test_analyzer");
      REQUIRE((false == !pool));
      CHECK((irs::flags::empty_instance() == pool->features()));
      auto analyzer = pool->get();
      CHECK((true == !analyzer));
    }

    // get invalid (not started)
    {
      CHECK((true == !feature.get("invalid")));
    }

    // get static analyzer (not started)
    {
      auto pool = feature.get("identity");
      REQUIRE((false == !pool));
      CHECK((irs::flags({irs::norm::type(), irs::frequency::type()}) == pool->features()));
      auto analyzer = pool->get();
      CHECK((false == !analyzer));
    }
  }

  {
    arangodb::iresearch::IResearchAnalyzerFeature feature(s.server);

    feature.start();
    feature.emplace("test_analyzer", "TestAnalyzer", "abc");

    // get valid (started)
    {
      auto pool = feature.get("test_analyzer");
      REQUIRE((false == !pool));
      CHECK((irs::flags() == pool->features()));
      auto analyzer = pool.get();
      CHECK((false == !analyzer));
    }

    // get invalid (started)
    {
      CHECK((true == !feature.get("invalid")));
    }

    // get static analyzer (started)
    {
      auto pool = feature.get("identity");
      REQUIRE((false == !pool));
      CHECK((irs::flags({irs::norm::type(), irs::frequency::type()}) == pool->features()));
      auto analyzer = pool->get();
      CHECK((false == !analyzer));
    }
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
    auto pool = feature.ensure("identity");
    CHECK((false == !pool));
    feature.start();
    pool = feature.get("identity");
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

SECTION("test_text_features") {
  // test registered 'identity'
  {
    arangodb::iresearch::IResearchAnalyzerFeature feature(s.server);
    for (auto& analyzerEntry : staticAnalyzers()) {
      CHECK((false == !feature.get(analyzerEntry.first)));
      auto pool = feature.ensure(analyzerEntry.first);
      CHECK((false == !pool));
      feature.start();
      pool = feature.get(analyzerEntry.first);
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
    auto collection = vocbase->lookupCollection("_iresearch_analyzers");

    if (collection) {
      vocbase->dropCollection(collection->id(), true, -1);
    }

    collection = vocbase->lookupCollection("_iresearch_analyzers");
    CHECK((nullptr == collection));
    arangodb::iresearch::IResearchAnalyzerFeature feature(s.server);
    feature.start();
    collection = vocbase->lookupCollection("_iresearch_analyzers");
    CHECK((nullptr != collection));
  }

  // read invalid configuration (missing attributes)
  {
    {
      std::string collection("_iresearch_analyzers");
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

    std::map<irs::string_ref, std::pair<irs::string_ref, irs::string_ref>> expected = {};
    arangodb::iresearch::IResearchAnalyzerFeature feature(s.server);

    feature.start();

    feature.visit([&expected](irs::string_ref const& name, irs::string_ref const& type, irs::string_ref const& properties)->bool {
      if (staticAnalyzers().find(name) != staticAnalyzers().end()) {
        return true; // skip static analyzers
      }

      auto itr = expected.find(name);
      CHECK((itr != expected.end()));
      CHECK((itr->second.first == type));
      CHECK((itr->second.second == properties));
      expected.erase(itr);
      return true;
    });
    CHECK((expected.empty()));
  }

  // read invalid configuration (duplicate non-identical records)
  {
    {
      std::string collection("_iresearch_analyzers");
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

  // read invalid configuration (static analyzers present)
  {
    {
      std::string collection("_iresearch_analyzers");
      arangodb::OperationOptions options;
      arangodb::SingleCollectionTransaction trx(
        arangodb::transaction::StandaloneContext::Create(*vocbase),
        collection,
        arangodb::AccessMode::Type::WRITE
      );
      trx.begin();
      trx.truncate(collection, options);
      trx.insert(collection, arangodb::velocypack::Parser::fromJson("{\"name\": \"identity\", \"type\": \"identity\", \"properties\": \"abc\"}")->slice(), options);
      trx.commit();
    }

    arangodb::iresearch::IResearchAnalyzerFeature feature(s.server);
    CHECK_THROWS((feature.start()));
  }

  // read valid configuration (different parameter options)
  {
    {
      std::string collection("_iresearch_analyzers");
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

    std::map<irs::string_ref, std::pair<irs::string_ref, irs::string_ref>> expected = {
      { "valid0", { "identity", irs::string_ref::NIL } },
      { "valid2", { "identity", "abc" } },
      { "valid4", { "identity", "[1,\"abc\"]" } },
      { "valid5", { "identity", "{\"a\":7,\"b\":\"c\"}" } },
    };
    arangodb::iresearch::IResearchAnalyzerFeature feature(s.server);

    feature.start();

    feature.visit([&expected](irs::string_ref const& name, irs::string_ref const& type, irs::string_ref const& properties)->bool {
      if (staticAnalyzers().find(name) != staticAnalyzers().end()) {
        return true; // skip static analyzers
      }

      auto itr = expected.find(name);
      CHECK((itr != expected.end()));
      CHECK((itr->second.first == type));
      CHECK((itr->second.second == properties));
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
      auto collection = vocbase->lookupCollection("_iresearch_analyzers");
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
      arangodb::iresearch::IResearchAnalyzerFeature feature(s.server);
      feature.start();
      auto result = feature.emplace("valid", "identity", "abc");
      CHECK((result.first));
      CHECK((result.second));
    }

    {
      std::map<irs::string_ref, std::pair<irs::string_ref, irs::string_ref>> expected = {
        { "valid", { "identity", "abc" } },
      };
      arangodb::iresearch::IResearchAnalyzerFeature feature(s.server);

      feature.start();

      feature.visit([&expected](irs::string_ref const& name, irs::string_ref const& type, irs::string_ref const& properties)->bool {
        if (staticAnalyzers().find(name) != staticAnalyzers().end()) {
          return true; // skip static analyzers
        }

        auto itr = expected.find(name);
        CHECK((itr != expected.end()));
        CHECK((itr->second.first == type));
        CHECK((itr->second.second == properties));
        expected.erase(itr);
        return true;
      });
      CHECK((expected.empty()));
    }
  }

  // remove existing records
  {
    {
      std::string collection("_iresearch_analyzers");
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
      std::map<irs::string_ref, std::pair<irs::string_ref, irs::string_ref>> expected = {
        { "identity", { "identity", irs::string_ref::NIL } },
        { "valid", { "identity", irs::string_ref::NIL } },
      };
      arangodb::iresearch::IResearchAnalyzerFeature feature(s.server);

      feature.start();

      feature.visit([&expected](irs::string_ref const& name, irs::string_ref const& type, irs::string_ref const& properties)->bool {
        if (name != "identity" &&
            staticAnalyzers().find(name) != staticAnalyzers().end()) {
          return true; // skip static analyzers
        }

        auto itr = expected.find(name);
        CHECK((itr != expected.end()));
        CHECK((itr->second.first == type));
        CHECK((itr->second.second == properties));
        expected.erase(itr);
        return true;
      });
      CHECK((expected.empty()));
      CHECK((1 == feature.erase("valid")));
      CHECK((0 == feature.erase("identity")));
    }

    {
      std::map<irs::string_ref, std::pair<irs::string_ref, irs::string_ref>> expected = {};
      arangodb::iresearch::IResearchAnalyzerFeature feature(s.server);

      feature.start();

      feature.visit([&expected](irs::string_ref const& name, irs::string_ref const& type, irs::string_ref const& properties)->bool {
        if (staticAnalyzers().find(name) != staticAnalyzers().end()) {
          return true; // skip static analyzers
        }

        auto itr = expected.find(name);
        CHECK((itr != expected.end()));
        CHECK((itr->second.first == type));
        CHECK((itr->second.second == properties));
        expected.erase(itr);
        return true;
      });
      CHECK((expected.empty()));
    }
  }
}

SECTION("test_remove") {
  // remove (not started)
  {
    arangodb::iresearch::IResearchAnalyzerFeature feature(s.server);
    CHECK((0 == feature.erase("test_analyzer")));
    CHECK((true == !feature.get("test_analyzer")));
    CHECK((false == !feature.ensure("test_analyzer")));
    CHECK((1 == feature.erase("test_analyzer")));
  }

  // remove static analyzer (not started)
  {
    arangodb::iresearch::IResearchAnalyzerFeature feature(s.server);
    CHECK((0 == feature.erase("identity")));
    CHECK((false == !feature.get("identity")));
    CHECK((0 == feature.erase("identity")));
  }

  // remove existing (started)
  {
    arangodb::iresearch::IResearchAnalyzerFeature feature(s.server);
    feature.start();
    CHECK((false == !feature.emplace("test_analyzer", "TestAnalyzer", "abc").first));
    CHECK((false == !feature.get("test_analyzer")));
    CHECK((1 == feature.erase("test_analyzer")));
    CHECK((true == !feature.get("test_analyzer")));
  }

  // remove invalid (started)
  {
    arangodb::iresearch::IResearchAnalyzerFeature feature(s.server);
    feature.start();
    CHECK((true == !feature.get("test_analyzer")));
    CHECK((0 == feature.erase("test_analyzer")));
  }

  // remove static analyzer (started)
  {
    arangodb::iresearch::IResearchAnalyzerFeature feature(s.server);
    feature.start();
    CHECK((false == !feature.get("identity")));
    CHECK((0 == feature.erase("identity")));
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
      auto collection = vocbase->lookupCollection("_iresearch_analyzers");

      if (collection) {
        vocbase->dropCollection(collection->id(), true, -1);
      }

      collection = vocbase->lookupCollection("_iresearch_analyzers");
      CHECK((nullptr == collection));
    }

    auto before = StorageEngineMock::inRecoveryResult;
    StorageEngineMock::inRecoveryResult = true;
    auto restore = irs::make_finally([&before]()->void { StorageEngineMock::inRecoveryResult = before; });
    arangodb::iresearch::IResearchAnalyzerFeature feature(s.server);
    feature.start();
    CHECK((nullptr == vocbase->lookupCollection("_iresearch_analyzers")));

    auto expected = staticAnalyzers();

    feature.visit([&expected, &feature](irs::string_ref const& name, irs::string_ref const& type, irs::string_ref const& properties)->bool {
      auto itr = expected.find(name);
      CHECK((itr != expected.end()));
      CHECK((itr->second.type == type));
      CHECK((itr->second.properties == properties));
      CHECK((itr->second.features.is_subset_of(feature.get(name)->features())));
      expected.erase(itr);
      return true;
    });
    CHECK((expected.empty()));
  }

  // test feature start load configuration (inRecovery, no configuration collection, uninitialized analyzers)
  {
    // ensure no configuration collection
    {
      auto collection = vocbase->lookupCollection("_iresearch_analyzers");

      if (collection) {
        vocbase->dropCollection(collection->id(), true, -1);
      }

      collection = vocbase->lookupCollection("_iresearch_analyzers");
      CHECK((nullptr == collection));
    }

    auto before = StorageEngineMock::inRecoveryResult;
    StorageEngineMock::inRecoveryResult = true;
    auto restore = irs::make_finally([&before]()->void { StorageEngineMock::inRecoveryResult = before; });
    arangodb::iresearch::IResearchAnalyzerFeature feature(s.server);
    CHECK((true == !feature.get("test_analyzer")));
    CHECK((false == !feature.ensure("test_analyzer")));
    feature.start();
    CHECK((nullptr == vocbase->lookupCollection("_iresearch_analyzers")));

    auto expected = staticAnalyzers();

    expected.emplace(std::piecewise_construct, std::forward_as_tuple("test_analyzer"), std::forward_as_tuple(irs::string_ref::NIL, irs::string_ref::NIL));
    feature.visit([&expected, &feature](irs::string_ref const& name, irs::string_ref const& type, irs::string_ref const& properties)->bool {
      auto itr = expected.find(name);
      CHECK((itr != expected.end()));
      CHECK((itr->second.type == type));
      CHECK((itr->second.properties == properties));
      CHECK((itr->second.features.is_subset_of(feature.get(name)->features())));
      expected.erase(itr);
      return true;
    });
    CHECK((expected.empty()));
  }

  // test feature start load configuration (inRecovery, with configuration collection)
  {
    // ensure there is an empty configuration collection
    {
      auto collection = vocbase->lookupCollection("_iresearch_analyzers");

      if (collection) {
        vocbase->dropCollection(collection->id(), true, -1);
      }

      collection = vocbase->lookupCollection("_iresearch_analyzers");
      CHECK((nullptr == collection));
      arangodb::iresearch::IResearchAnalyzerFeature feature(s.server);
      feature.start();
      CHECK((false == !feature.emplace("test_analyzer", "identity", "abc").first));
      collection = vocbase->lookupCollection("_iresearch_analyzers");
      CHECK((nullptr != collection));
    }

    auto before = StorageEngineMock::inRecoveryResult;
    StorageEngineMock::inRecoveryResult = true;
    auto restore = irs::make_finally([&before]()->void { StorageEngineMock::inRecoveryResult = before; });
    arangodb::iresearch::IResearchAnalyzerFeature feature(s.server);
    feature.start();
    CHECK((nullptr != vocbase->lookupCollection("_iresearch_analyzers")));

    auto expected = staticAnalyzers();

    expected.emplace(std::piecewise_construct, std::forward_as_tuple("test_analyzer"), std::forward_as_tuple("identity", "abc"));
    feature.visit([&expected, &feature](irs::string_ref const& name, irs::string_ref const& type, irs::string_ref const& properties)->bool {
      auto itr = expected.find(name);
      CHECK((itr != expected.end()));
      CHECK((itr->second.type == type));
      CHECK((itr->second.properties == properties));
      CHECK((itr->second.features.is_subset_of(feature.get(name)->features())));
      expected.erase(itr);
      return true;
    });
    CHECK((expected.empty()));
  }

  // test feature start load configuration (inRecovery, with configuration collection, uninitialized analyzers)
  {
    // ensure there is an empty configuration collection
    {
      auto collection = vocbase->lookupCollection("_iresearch_analyzers");

      if (collection) {
        vocbase->dropCollection(collection->id(), true, -1);
      }

      collection = vocbase->lookupCollection("_iresearch_analyzers");
      CHECK((nullptr == collection));
      arangodb::iresearch::IResearchAnalyzerFeature feature(s.server);
      feature.start();
      CHECK((false == !feature.emplace("test_analyzer", "identity", "abc").first));
      collection = vocbase->lookupCollection("_iresearch_analyzers");
      CHECK((nullptr != collection));
    }

    auto before = StorageEngineMock::inRecoveryResult;
    StorageEngineMock::inRecoveryResult = true;
    auto restore = irs::make_finally([&before]()->void { StorageEngineMock::inRecoveryResult = before; });
    arangodb::iresearch::IResearchAnalyzerFeature feature(s.server);
    CHECK((true == !feature.get("test_analyzer")));
    CHECK((false == !feature.ensure("test_analyzer")));
    feature.start();
    CHECK((nullptr != vocbase->lookupCollection("_iresearch_analyzers")));

    auto expected = staticAnalyzers();

    expected.emplace(std::piecewise_construct, std::forward_as_tuple("test_analyzer"), std::forward_as_tuple("identity", "abc"));
    feature.visit([&expected, &feature](irs::string_ref const& name, irs::string_ref const& type, irs::string_ref const& properties)->bool {
      auto itr = expected.find(name);
      CHECK((itr != expected.end()));
      CHECK((itr->second.type == type));
      CHECK((itr->second.properties == properties));
      CHECK((itr->second.features.is_subset_of(feature.get(name)->features())));
      expected.erase(itr);
      return true;
    });
    CHECK((expected.empty()));
  }

  // test feature start load configuration (no configuration collection)
  {
    // ensure no configuration collection
    {
      auto collection = vocbase->lookupCollection("_iresearch_analyzers");

      if (collection) {
        vocbase->dropCollection(collection->id(), true, -1);
      }

      collection = vocbase->lookupCollection("_iresearch_analyzers");
      CHECK((nullptr == collection));
    }

    arangodb::iresearch::IResearchAnalyzerFeature feature(s.server);
    feature.start();
    CHECK((nullptr != vocbase->lookupCollection("_iresearch_analyzers")));

    auto expected = staticAnalyzers();

    feature.visit([&expected, &feature](irs::string_ref const& name, irs::string_ref const& type, irs::string_ref const& properties)->bool {
      auto itr = expected.find(name);
      CHECK((itr != expected.end()));
      CHECK((itr->second.type == type));
      CHECK((itr->second.properties == properties));
      CHECK((itr->second.features.is_subset_of(feature.get(name)->features())));
      expected.erase(itr);
      return true;
    });
    CHECK((expected.empty()));
  }

  // test feature start load configuration (no configuration collection, static analyzers)
  {
    // ensure no configuration collection
    {
      auto collection = vocbase->lookupCollection("_iresearch_analyzers");

      if (collection) {
        vocbase->dropCollection(collection->id(), true, -1);
      }

      collection = vocbase->lookupCollection("_iresearch_analyzers");
      CHECK((nullptr == collection));
    }

    arangodb::iresearch::IResearchAnalyzerFeature feature(s.server);
    CHECK((false == !feature.get("identity")));
    feature.start();
    CHECK((nullptr != vocbase->lookupCollection("_iresearch_analyzers")));

    auto expected = staticAnalyzers();

    feature.visit([&expected, &feature](irs::string_ref const& name, irs::string_ref const& type, irs::string_ref const& properties)->bool {
      auto itr = expected.find(name);
      CHECK((itr != expected.end()));
      CHECK((itr->second.type == type));
      CHECK((itr->second.properties == properties));
      CHECK((itr->second.features.is_subset_of(feature.get(name)->features())));
      expected.erase(itr);
      return true;
    });
    CHECK((expected.empty()));
  }

  // test feature start load configuration (with configuration collection)
  {

    // ensure there is an empty configuration collection
    {
      auto collection = vocbase->lookupCollection("_iresearch_analyzers");

      if (collection) {
        vocbase->dropCollection(collection->id(), true, -1);
      }

      collection = vocbase->lookupCollection("_iresearch_analyzers");
      CHECK((nullptr == collection));
      arangodb::iresearch::IResearchAnalyzerFeature feature(s.server);
      feature.start();
      CHECK((false == !feature.emplace("test_analyzer", "identity", "abc").first));
      collection = vocbase->lookupCollection("_iresearch_analyzers");
      CHECK((nullptr != collection));
    }

    arangodb::iresearch::IResearchAnalyzerFeature feature(s.server);
    feature.start();
    CHECK((nullptr != vocbase->lookupCollection("_iresearch_analyzers")));

    auto expected = staticAnalyzers();

    expected.emplace(std::piecewise_construct, std::forward_as_tuple("test_analyzer"), std::forward_as_tuple("identity", "abc"));
    feature.visit([&expected, &feature](irs::string_ref const& name, irs::string_ref const& type, irs::string_ref const& properties)->bool {
      auto itr = expected.find(name);
      CHECK((itr != expected.end()));
      CHECK((itr->second.type == type));
      CHECK((itr->second.properties == properties));
      CHECK((itr->second.features.is_subset_of(feature.get(name)->features())));
      expected.erase(itr);
      return true;
    });
    CHECK((expected.empty()));
  }

  // test feature start load configuration (with configuration collection, uninitialized analyzers)
  {
    // ensure there is an empty configuration collection
    {
      auto collection = vocbase->lookupCollection("_iresearch_analyzers");

      if (collection) {
        vocbase->dropCollection(collection->id(), true, -1);
      }

      collection = vocbase->lookupCollection("_iresearch_analyzers");
      CHECK((nullptr == collection));
      arangodb::iresearch::IResearchAnalyzerFeature feature(s.server);
      feature.start();
      CHECK((false == !feature.emplace("test_analyzer", "identity", "abc").first));
      collection = vocbase->lookupCollection("_iresearch_analyzers");
      CHECK((nullptr != collection));
    }

    arangodb::iresearch::IResearchAnalyzerFeature feature(s.server);
    CHECK((true == !feature.get("test_analyzer")));
    CHECK((false == !feature.ensure("test_analyzer")));
    feature.start();
    CHECK((nullptr != vocbase->lookupCollection("_iresearch_analyzers")));

    auto expected = staticAnalyzers();

    expected.emplace(std::piecewise_construct, std::forward_as_tuple("test_analyzer"), std::forward_as_tuple("identity", "abc"));
    feature.visit([&expected, &feature](irs::string_ref const& name, irs::string_ref const& type, irs::string_ref const& properties)->bool {
      auto itr = expected.find(name);
      CHECK((itr != expected.end()));
      CHECK((itr->second.type == type));
      CHECK((itr->second.properties == properties));
      CHECK((itr->second.features.is_subset_of(feature.get(name)->features())));
      expected.erase(itr);
      return true;
    });
    CHECK((expected.empty()));
  }
}

SECTION("test_tokens") {
  auto* database = arangodb::application_features::ApplicationServer::lookupFeature<
    arangodb::SystemDatabaseFeature
  >();
  auto vocbase = database->use();

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
  auto* systemdb = new arangodb::SystemDatabaseFeature(server, vocbase.get());

  arangodb::application_features::ApplicationServer::server->addFeature(analyzers);
  arangodb::application_features::ApplicationServer::server->addFeature(dbfeature);
  arangodb::application_features::ApplicationServer::server->addFeature(functions);
  arangodb::application_features::ApplicationServer::server->addFeature(sharding);
  arangodb::application_features::ApplicationServer::server->addFeature(systemdb);

  sharding->prepare();

  // ensure there is no configuration collection
  {
    auto collection = vocbase->lookupCollection("_iresearch_analyzers");

    if (collection) {
      vocbase->dropCollection(collection->id(), true, -1);
    }

    collection = vocbase->lookupCollection("_iresearch_analyzers");
    CHECK((nullptr == collection));
  }

  // test function registration
  {
    arangodb::iresearch::IResearchAnalyzerFeature feature(s.server);

    // AqlFunctionFeature::byName(..) throws exception instead of returning a nullptr
    CHECK_THROWS((functions->byName("TOKENS")));

    feature.start();
    CHECK((nullptr != functions->byName("TOKENS")));
  }

  analyzers->start();
  REQUIRE((false == !analyzers->emplace("test_analyzer", "TestAnalyzer", "abc").first));

  // test tokenization
  {
    auto* function = functions->byName("TOKENS");
    CHECK((nullptr != functions->byName("TOKENS")));
    auto& impl = function->implementation;
    CHECK((false == !impl));

    irs::string_ref analyzer("test_analyzer");
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
  static std::string const ANALYZER_COLLECTION_NAME("_iresearch_analyzers");
  static std::string const LEGACY_ANALYZER_COLLECTION_NAME("_iresearch_analyzers");
  static std::string const ANALYZER_COLLECTION_QUERY = std::string("FOR d IN ") + ANALYZER_COLLECTION_NAME + " RETURN d";
  static std::unordered_set<std::string> const EXPECTED_LEGACY_ANALYZERS = { "text_de", "text_en", "text_es", "text_fi", "text_fr", "text_it", "text_nl", "text_no", "text_pt", "text_ru", "text_sv", "text_zh", };
  auto createCollectionJson = arangodb::velocypack::Parser::fromJson(std::string("{ \"id\": 42, \"name\": \"") + ANALYZER_COLLECTION_NAME + "\", \"isSystem\": true, \"shards\": { \"shard-id-does-not-matter\": [ \"shard-server-does-not-matter\" ] }, \"type\": 2 }"); // 'id' and 'shards' required for coordinator tests
  auto collectionId = std::to_string(42);
  auto versionJson = arangodb::velocypack::Parser::fromJson("{ \"version\": 0, \"tasks\": {} }");
  arangodb::AqlFeature aqlFeature(s.server);
  aqlFeature.start(); // required for Query::Query(...), must not call ~AqlFeature() for the duration of the test
  arangodb::aql::OptimizerRulesFeature(s.server).prepare(); // required for Query::preparePlan(...)
  auto clearOptimizerRules = irs::make_finally([&s]()->void { arangodb::aql::OptimizerRulesFeature(s.server).unprepare(); });

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
    CHECK((TRI_ERROR_NO_ERROR == result.code));
    auto slice = result.result->slice();
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
    CHECK((TRI_ERROR_NO_ERROR == result.code));
    auto slice = result.result->slice();
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

    feature->start(); // register upgrade tasks

    // remove legacy collection after feature start
    {
      auto collection = system.lookupCollection(LEGACY_ANALYZER_COLLECTION_NAME);
      REQUIRE((false == !collection));
      REQUIRE((system.dropCollection(collection->id(), true, -1.0).ok()));
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
    CHECK((TRI_ERROR_NO_ERROR == result.code));
    auto slice = result.result->slice();
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

    feature->start(); // register upgrade tasks

    // remove legacy collection after feature start
    {
      auto collection = system.lookupCollection(LEGACY_ANALYZER_COLLECTION_NAME);
      REQUIRE((false == !collection));
      REQUIRE((system.dropCollection(collection->id(), true, -1.0).ok()));
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
    CHECK((TRI_ERROR_NO_ERROR == result.code));
    auto slice = result.result->slice();
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

    feature->start(); // register upgrade tasks

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
    CHECK((TRI_ERROR_NO_ERROR == result.code));
    auto slice = result.result->slice();
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

    feature->start(); // register upgrade tasks

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
    CHECK((TRI_ERROR_NO_ERROR == result.code));
    auto slice = result.result->slice();
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
      response.operationID = clusterComm.nextOperationId() + i; // sequential value
      response.status = arangodb::ClusterCommOpStatus::CL_COMM_RECEIVED;
      response.answer_code = arangodb::rest::ResponseCode::CREATED;
      response.answer = std::make_shared<GeneralRequestMock>(*vocbase);
      static_cast<GeneralRequestMock*>(response.answer.get())->_payload = *arangodb::velocypack::Parser::fromJson(std::string("{ \"_key\": \"") + std::to_string(response.operationID) + "\" }"); // unique arbitrary key
      clusterComm._responses[0].emplace_back(std::move(response));
    }

    sysDatabase->unprepare(); // unset system vocbase
    CHECK_THROWS((!ci->getCollection(vocbase->name(), ANALYZER_COLLECTION_NAME))); // throws on missing collection
    CHECK((arangodb::methods::Upgrade::clusterBootstrap(*vocbase).ok())); // run upgrade
    CHECK((false == !ci->getCollection(vocbase->name(), ANALYZER_COLLECTION_NAME)));
    CHECK((true == clusterComm._responses.empty()));

    for (auto& entry: clusterComm._requests) {
      CHECK((entry.second._body));
      auto body = arangodb::velocypack::Parser::fromJson(*(entry.second._body));
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
    expected.insert("abc");
    TRI_vocbase_t* vocbase;
    CHECK((TRI_ERROR_NO_ERROR == dbFeature->createDatabase(1, "testVocbase", vocbase)));
    CHECK((ci->createCollectionCoordinator(vocbase->name(), collectionId, 0, 1, false, createCollectionJson->slice(), 0.0).ok()));
    CHECK((false == !ci->getCollection(vocbase->name(), collectionId)));

    // insert response for expected extra analyzer
    {
      arangodb::ClusterCommResult response;
      response.operationID = clusterComm.nextOperationId();
      response.status = arangodb::ClusterCommOpStatus::CL_COMM_RECEIVED;
      response.answer_code = arangodb::rest::ResponseCode::CREATED;
      response.answer = std::make_shared<GeneralRequestMock>(*vocbase);
      static_cast<GeneralRequestMock*>(response.answer.get())->_payload = *arangodb::velocypack::Parser::fromJson(std::string("{ \"_key\": \"") + std::to_string(response.operationID) + "\" }"); // unique arbitrary key
      clusterComm._responses[0].emplace_back(std::move(response));
    }

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
      CHECK((true == clusterComm._responses.empty()));
    }

    // insert responses for the legacy static analyzers
    for (size_t i = 0, count = EXPECTED_LEGACY_ANALYZERS.size(); i < count; ++i) {
      arangodb::ClusterCommResult response;
      response.operationID = clusterComm.nextOperationId() + i; // sequential value
      response.status = arangodb::ClusterCommOpStatus::CL_COMM_RECEIVED;
      response.answer_code = arangodb::rest::ResponseCode::CREATED;
      response.answer = std::make_shared<GeneralRequestMock>(*vocbase);
      static_cast<GeneralRequestMock*>(response.answer.get())->_payload = *arangodb::velocypack::Parser::fromJson(std::string("{ \"_key\": \"") + std::to_string(response.operationID) + "\" }"); // unique arbitrary key
      clusterComm._responses[0].emplace_back(std::move(response));
    }

    sysDatabase->unprepare(); // unset system vocbase
    CHECK((false == !ci->getCollection(vocbase->name(), ANALYZER_COLLECTION_NAME)));
    CHECK((arangodb::methods::Upgrade::clusterBootstrap(*vocbase).ok())); // run upgrade
    CHECK((false == !ci->getCollection(vocbase->name(), ANALYZER_COLLECTION_NAME)));
    CHECK((true == clusterComm._responses.empty()));

    for (auto& entry: clusterComm._requests) {
      CHECK((entry.second._body));
      auto body = arangodb::velocypack::Parser::fromJson(*(entry.second._body));
      auto slice = body->slice();
      CHECK((slice.isObject()));
      CHECK((slice.get("name").isString()));
      CHECK((1 == expected.erase(slice.get("name").copyString())));
    }

    CHECK((true == expected.empty()));
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

    // create system vocbase (before feature start)
    {
      auto const databases = arangodb::velocypack::Parser::fromJson(std::string("[ { \"name\": \"") + arangodb::StaticStrings::SystemDatabase + "\" } ]");
      CHECK((TRI_ERROR_NO_ERROR == dbFeature->loadDatabases(databases->slice())));
      sysDatabase->start(); // get system database from DatabaseFeature
    }

    auto system = sysDatabase->use();

    // simulate heartbeat thread (create legacy analyzer collection before feature start)
    {
      auto const colPath = "/Plan/Collections";
      auto const colValue = arangodb::velocypack::Parser::fromJson(std::string("{ \"") + system->name() + "\": { \"" + LEGACY_ANALYZER_COLLECTION_NAME + "\": { \"name\": \"" + LEGACY_ANALYZER_COLLECTION_NAME + "\", \"isSystem\": true, \"shards\": { } } } }"); // collection ID must match id used in dropCollectionCoordinator(...)
      CHECK((arangodb::AgencyComm().setValue(colPath, colValue->slice(), 0.0).successful()));
    }

    feature->start(); // register upgrade tasks
    server.getFeature<arangodb::ClusterFeature>("Cluster")->prepare(); // create ClusterInfo instance
    server.getFeature<arangodb::ShardingFeature>("Sharding")->prepare(); // required for Collections::create(...), register sharding types
    arangodb::AgencyCommManager::MANAGER->start(); // initialize agency

    ClusterCommMock clusterComm;
    auto scopedClusterComm = ClusterCommMock::setInstance(clusterComm);
    auto* ci = arangodb::ClusterInfo::instance();
    REQUIRE((nullptr != ci));

    // remove legacy collection after feature start
    {
      auto collection = ci->getCollection(system->name(), LEGACY_ANALYZER_COLLECTION_NAME);
      REQUIRE((false == !collection));
      REQUIRE((ci->dropCollectionCoordinator(system->name(), LEGACY_ANALYZER_COLLECTION_NAME, 0.0).ok()));
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
      auto const colPath0 = "/Current/Collections/_system/2000004"; // '2000004' must match what ClusterInfo generates for LogicalCollection::id() or collection creation request will never get executed (use 'collectionID' from ClusterInfo::createCollectionCoordinator(...) in stack trace)
      auto const colValue0 = arangodb::velocypack::Parser::fromJson("{ \"same-as-dummy-shard-id\": { \"servers\": [ \"same-as-dummy-shard-server\" ] } }");
      CHECK(arangodb::AgencyComm().setValue(colPath0, colValue0->slice(), 0.0).successful());
      auto const colPath1 = "/Current/Collections/testVocbase/2000020"; // '2000020 must match what ClusterInfo generates for LogicalCollection::id() or collection creation request will never get executed (use 'collectionID' from ClusterInfo::createCollectionCoordinator(...) in stack trace)
      auto const colValue1 = arangodb::velocypack::Parser::fromJson("{ \"same-as-dummy-shard-id\": { \"servers\": [ \"same-as-dummy-shard-server\" ] } }");
      CHECK(arangodb::AgencyComm().setValue(colPath1, colValue1->slice(), 0.0).successful());
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
      response.operationID = clusterComm.nextOperationId() + i; // sequential value
      response.status = arangodb::ClusterCommOpStatus::CL_COMM_RECEIVED;
      response.answer_code = arangodb::rest::ResponseCode::CREATED;
      response.answer = std::make_shared<GeneralRequestMock>(*vocbase);
      static_cast<GeneralRequestMock*>(response.answer.get())->_payload = *arangodb::velocypack::Parser::fromJson(std::string("{ \"_key\": \"") + std::to_string(response.operationID) + "\" }"); // unique arbitrary key
      clusterComm._responses[0].emplace_back(std::move(response));
    }

    CHECK_THROWS((ci->getCollection(system->name(), LEGACY_ANALYZER_COLLECTION_NAME))); // throws on missing collection
    CHECK_THROWS((ci->getCollection(system->name(), ANALYZER_COLLECTION_NAME))); // throws on missing collection
    CHECK((arangodb::methods::Upgrade::clusterBootstrap(*system).ok())); // run system upgrade
    // FIXME TODO uncomment CHECK_THROWS((ci->getCollection(system->name(), LEGACY_ANALYZER_COLLECTION_NAME))); // throws on missing collection
    CHECK((false == !ci->getCollection(system->name(), ANALYZER_COLLECTION_NAME)));

    for (auto& entry: clusterComm._requests) {
      CHECK((entry.second._body));
      auto body = arangodb::velocypack::Parser::fromJson(*(entry.second._body));
      auto slice = body->slice();
      CHECK((slice.isObject()));
      CHECK((slice.get("name").isString()));
      CHECK((1 == expected.erase(slice.get("name").copyString())));
    }

    CHECK((true == expected.empty()));

    // insert responses for the legacy static analyzers
    for (size_t i = 0, count = EXPECTED_LEGACY_ANALYZERS.size(); i < count; ++i) {
      arangodb::ClusterCommResult response;
      response.operationID = clusterComm.nextOperationId() + i; // sequential value
      response.status = arangodb::ClusterCommOpStatus::CL_COMM_RECEIVED;
      response.answer_code = arangodb::rest::ResponseCode::CREATED;
      response.answer = std::make_shared<GeneralRequestMock>(*vocbase);
      static_cast<GeneralRequestMock*>(response.answer.get())->_payload = *arangodb::velocypack::Parser::fromJson(std::string("{ \"_key\": \"") + std::to_string(response.operationID) + "\" }"); // unique arbitrary key
      clusterComm._responses[0].emplace_back(std::move(response));
    }

    clusterComm._requests.clear();
    expected = EXPECTED_LEGACY_ANALYZERS;
    CHECK_THROWS((ci->getCollection(vocbase->name(), ANALYZER_COLLECTION_NAME))); // throws on missing collection
    CHECK((arangodb::methods::Upgrade::clusterBootstrap(*vocbase).ok())); // run upgrade
    CHECK((false == !ci->getCollection(vocbase->name(), ANALYZER_COLLECTION_NAME)));
    CHECK((true == clusterComm._responses.empty()));

    for (auto& entry: clusterComm._requests) {
      CHECK((entry.second._body));
      auto body = arangodb::velocypack::Parser::fromJson(*(entry.second._body));
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

    // create system vocbase (before feature start)
    {
      auto const databases = arangodb::velocypack::Parser::fromJson(std::string("[ { \"name\": \"") + arangodb::StaticStrings::SystemDatabase + "\" } ]");
      CHECK((TRI_ERROR_NO_ERROR == dbFeature->loadDatabases(databases->slice())));
      sysDatabase->start(); // get system database from DatabaseFeature
    }

    auto system = sysDatabase->use();

    // simulate heartbeat thread (create legacy analyzer collection before feature start)
    {
      auto const colPath = "/Plan/Collections";
      auto const colValue = arangodb::velocypack::Parser::fromJson(std::string("{ \"") + system->name() + "\": { \"" + LEGACY_ANALYZER_COLLECTION_NAME + "\": { \"name\": \"" + LEGACY_ANALYZER_COLLECTION_NAME + "\", \"isSystem\": true, \"shards\": { } } } }"); // collection ID must match id used in dropCollectionCoordinator(...)
      CHECK((arangodb::AgencyComm().setValue(colPath, colValue->slice(), 0.0).successful()));
    }

    feature->start(); // register upgrade tasks
    server.getFeature<arangodb::ClusterFeature>("Cluster")->prepare(); // create ClusterInfo instance
    server.getFeature<arangodb::ShardingFeature>("Sharding")->prepare(); // required for Collections::create(...), register sharding types
    arangodb::AgencyCommManager::MANAGER->start(); // initialize agency

    ClusterCommMock clusterComm;
    auto scopedClusterComm = ClusterCommMock::setInstance(clusterComm);
    auto* ci = arangodb::ClusterInfo::instance();
    REQUIRE((nullptr != ci));

    // remove legacy collection after feature start
    {
      auto collection = ci->getCollection(system->name(), LEGACY_ANALYZER_COLLECTION_NAME);
      REQUIRE((false == !collection));
      REQUIRE((ci->dropCollectionCoordinator(system->name(), LEGACY_ANALYZER_COLLECTION_NAME, 0.0).ok()));
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
      response.operationID = clusterComm.nextOperationId() + i; // sequential value
      response.status = arangodb::ClusterCommOpStatus::CL_COMM_RECEIVED;
      response.answer_code = arangodb::rest::ResponseCode::CREATED;
      response.answer = std::make_shared<GeneralRequestMock>(*vocbase);
      static_cast<GeneralRequestMock*>(response.answer.get())->_payload = *arangodb::velocypack::Parser::fromJson(std::string("{ \"_key\": \"") + std::to_string(response.operationID) + "\" }"); // unique arbitrary key
      clusterComm._responses[0].emplace_back(std::move(response));
    }

    CHECK_THROWS((ci->getCollection(system->name(), LEGACY_ANALYZER_COLLECTION_NAME))); // throws on missing collection
    CHECK_THROWS((ci->getCollection(system->name(), ANALYZER_COLLECTION_NAME))); // throws on missing collection
    CHECK((arangodb::methods::Upgrade::clusterBootstrap(*system).ok())); // run system upgrade
    // FIXME TODO uncomment CHECK_THROWS((ci->getCollection(system->name(), LEGACY_ANALYZER_COLLECTION_NAME))); // throws on missing collection
    CHECK((false == !ci->getCollection(system->name(), ANALYZER_COLLECTION_NAME)));

    for (auto& entry: clusterComm._requests) {
      CHECK((entry.second._body));
      auto body = arangodb::velocypack::Parser::fromJson(*(entry.second._body));
      auto slice = body->slice();
      CHECK((slice.isObject()));
      CHECK((slice.get("name").isString()));
      CHECK((1 == expected.erase(slice.get("name").copyString())));
    }

    CHECK((true == expected.empty()));

    // insert responses for the legacy static analyzers
    for (size_t i = 0, count = EXPECTED_LEGACY_ANALYZERS.size(); i < count; ++i) {
      arangodb::ClusterCommResult response;
      response.operationID = clusterComm.nextOperationId() + i; // sequential value
      response.status = arangodb::ClusterCommOpStatus::CL_COMM_RECEIVED;
      response.answer_code = arangodb::rest::ResponseCode::CREATED;
      response.answer = std::make_shared<GeneralRequestMock>(*vocbase);
      static_cast<GeneralRequestMock*>(response.answer.get())->_payload = *arangodb::velocypack::Parser::fromJson(std::string("{ \"_key\": \"") + std::to_string(response.operationID) + "\" }"); // unique arbitrary key
      clusterComm._responses[0].emplace_back(std::move(response));
    }

    clusterComm._requests.clear();
    expected = EXPECTED_LEGACY_ANALYZERS;
    CHECK((false == !ci->getCollection(vocbase->name(), ANALYZER_COLLECTION_NAME)));
    CHECK((arangodb::methods::Upgrade::clusterBootstrap(*vocbase).ok())); // run upgrade
    CHECK((false == !ci->getCollection(vocbase->name(), ANALYZER_COLLECTION_NAME)));
    CHECK((true == clusterComm._responses.empty()));

    for (auto& entry: clusterComm._requests) {
      CHECK((entry.second._body));
      auto body = arangodb::velocypack::Parser::fromJson(*(entry.second._body));
      auto slice = body->slice();
      CHECK((slice.isObject()));
      CHECK((slice.get("name").isString()));
      CHECK((1 == expected.erase(slice.get("name").copyString())));
    }

    CHECK((true == expected.empty()));
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

    // create system vocbase (before feature start)
    {
      auto const databases = arangodb::velocypack::Parser::fromJson(std::string("[ { \"name\": \"") + arangodb::StaticStrings::SystemDatabase + "\" } ]");
      CHECK((TRI_ERROR_NO_ERROR == dbFeature->loadDatabases(databases->slice())));
      sysDatabase->start(); // get system database from DatabaseFeature
    }

    auto system = sysDatabase->use();

    // simulate heartbeat thread (create legacy analyzer collection before feature start)
    {
      auto const path = "/Plan/Collections";
      auto const value = arangodb::velocypack::Parser::fromJson(std::string("{ \"") + system->name() + "\": { \"" + LEGACY_ANALYZER_COLLECTION_NAME + "\": { \"name\": \"" + LEGACY_ANALYZER_COLLECTION_NAME + "\", \"isSystem\": true, \"shards\": { \"same-as-dummy-shard-id\": [ \"same-as-dummy-shard-server\" ] } } } }"); // collection ID must match id used in dropCollectionCoordinator(...)
      CHECK(arangodb::AgencyComm().setValue(path, value->slice(), 0.0).successful());
    }

    feature->start(); // register upgrade tasks
    server.getFeature<arangodb::ClusterFeature>("Cluster")->prepare(); // create ClusterInfo instance
    server.getFeature<arangodb::ShardingFeature>("Sharding")->prepare(); // required for Collections::create(...), register sharding types
    arangodb::AgencyCommManager::MANAGER->start(); // initialize agency

    ClusterCommMock clusterComm;
    auto scopedClusterComm = ClusterCommMock::setInstance(clusterComm);
    auto* ci = arangodb::ClusterInfo::instance();
    REQUIRE((nullptr != ci));

    CHECK((false == !ci->getCollection(system->name(), LEGACY_ANALYZER_COLLECTION_NAME)));

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
      response.operationID = clusterComm.nextOperationId() + i; // sequential value
      response.status = arangodb::ClusterCommOpStatus::CL_COMM_RECEIVED;
      response.answer_code = arangodb::rest::ResponseCode::CREATED;
      response.answer = std::make_shared<GeneralRequestMock>(*vocbase);
      static_cast<GeneralRequestMock*>(response.answer.get())->_payload = *arangodb::velocypack::Parser::fromJson(std::string("{ \"_key\": \"") + std::to_string(response.operationID) + "\" }"); // unique arbitrary key
      clusterComm._responses[0].emplace_back(std::move(response));
    }

    CHECK((false == !ci->getCollection(system->name(), LEGACY_ANALYZER_COLLECTION_NAME)));
    // FIXME TODO uncomment CHECK_THROWS((ci->getCollection(system->name(), ANALYZER_COLLECTION_NAME))); // throws on missing collection
    CHECK((arangodb::methods::Upgrade::clusterBootstrap(*system).ok())); // run system upgrade
    // FIXME TODO uncomment CHECK_THROWS((ci->getCollection(system->name(), LEGACY_ANALYZER_COLLECTION_NAME))); // throws on missing collection
    CHECK((false == !ci->getCollection(system->name(), ANALYZER_COLLECTION_NAME)));

    for (auto& entry: clusterComm._requests) {
      CHECK((entry.second._body));
      auto body = arangodb::velocypack::Parser::fromJson(*(entry.second._body));
      auto slice = body->slice();
      CHECK((slice.isObject()));
      CHECK((slice.get("name").isString()));
      CHECK((1 == expected.erase(slice.get("name").copyString())));
    }

    CHECK((true == expected.empty()));

    // insert responses for the legacy static analyzers
    for (size_t i = 0, count = EXPECTED_LEGACY_ANALYZERS.size(); i < count; ++i) {
      arangodb::ClusterCommResult response;
      response.operationID = clusterComm.nextOperationId() + i; // sequential value
      response.status = arangodb::ClusterCommOpStatus::CL_COMM_RECEIVED;
      response.answer_code = arangodb::rest::ResponseCode::CREATED;
      response.answer = std::make_shared<GeneralRequestMock>(*vocbase);
      static_cast<GeneralRequestMock*>(response.answer.get())->_payload = *arangodb::velocypack::Parser::fromJson(std::string("{ \"_key\": \"") + std::to_string(response.operationID) + "\" }"); // unique arbitrary key
      clusterComm._responses[0].emplace_back(std::move(response));
    }

    clusterComm._requests.clear();
    expected = EXPECTED_LEGACY_ANALYZERS;
    CHECK_THROWS((ci->getCollection(vocbase->name(), ANALYZER_COLLECTION_NAME))); // throws on missing collection
    CHECK((arangodb::methods::Upgrade::clusterBootstrap(*vocbase).ok())); // run upgrade
    CHECK((false == !ci->getCollection(vocbase->name(), ANALYZER_COLLECTION_NAME)));
    CHECK((true == clusterComm._responses.empty()));

    for (auto& entry: clusterComm._requests) {
      CHECK((entry.second._body));
      auto body = arangodb::velocypack::Parser::fromJson(*(entry.second._body));
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

    // create system vocbase (before feature start)
    {
      auto const databases = arangodb::velocypack::Parser::fromJson(std::string("[ { \"name\": \"") + arangodb::StaticStrings::SystemDatabase + "\" } ]");
      CHECK((TRI_ERROR_NO_ERROR == dbFeature->loadDatabases(databases->slice())));
      sysDatabase->start(); // get system database from DatabaseFeature
    }

    auto system = sysDatabase->use();

    // simulate heartbeat thread (create legacy analyzer collection before feature start)
    {
      auto const path = "/Plan/Collections";
      auto const value = arangodb::velocypack::Parser::fromJson(std::string("{ \"") + system->name() + "\": { \"" + LEGACY_ANALYZER_COLLECTION_NAME + "\": { \"name\": \"" + LEGACY_ANALYZER_COLLECTION_NAME + "\", \"isSystem\": true, \"shards\": { \"same-as-dummy-shard-id\": [ \"same-as-dummy-shard-server\" ] } } } }"); // collection ID must match id used in dropCollectionCoordinator(...)
      CHECK(arangodb::AgencyComm().setValue(path, value->slice(), 0.0).successful());
    }

    feature->start(); // register upgrade tasks
    server.getFeature<arangodb::ClusterFeature>("Cluster")->prepare(); // create ClusterInfo instance
    server.getFeature<arangodb::ShardingFeature>("Sharding")->prepare(); // required for Collections::create(...), register sharding types
    arangodb::AgencyCommManager::MANAGER->start(); // initialize agency

    ClusterCommMock clusterComm;
    auto scopedClusterComm = ClusterCommMock::setInstance(clusterComm);
    auto* ci = arangodb::ClusterInfo::instance();
    REQUIRE((nullptr != ci));

    CHECK((false == !ci->getCollection(system->name(), LEGACY_ANALYZER_COLLECTION_NAME)));

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
      auto const colPath0 = "/Current/Collections/_system/5000004"; // '5000004' must match what ClusterInfo generates for LogicalCollection::id() or collection creation request will never get executed (use 'collectionID' from ClusterInfo::createCollectionCoordinator(...) in stack trace)
      auto const colValue0 = arangodb::velocypack::Parser::fromJson("{ \"same-as-dummy-shard-id\": { \"servers\": [ \"same-as-dummy-shard-server\" ] } }");
      CHECK(arangodb::AgencyComm().setValue(colPath0, colValue0->slice(), 0.0).successful());
      auto const colPath = "/Current/Collections/testVocbase/5000006"; // '5000006' must match what ClusterInfo generates for LogicalCollection::id() or collection creation request will never get executed (use 'collectionID' from ClusterInfo::createCollectionCoordinator(...) in stack trace)
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
    {
      auto const path = "/Plan/Collections";
      auto const value = arangodb::velocypack::Parser::fromJson(std::string("{ \"") + vocbase->name() + "\": { \"5000006\": { \"name\": \"" + ANALYZER_COLLECTION_NAME + "\", \"isSystem\": true, \"shards\": { \"same-as-dummy-shard-id\": [ \"same-as-dummy-shard-server\" ] } } } }"); // must match what ClusterInfo generates for LogicalCollection::id() or shard list retrieval will fail (use 'collectionID' from ClusterInfo::getShardList(...) in stack trace)
      CHECK(arangodb::AgencyComm().setValue(path, value->slice(), 0.0).successful());
    }

    CHECK((false == !ci->getCollection(vocbase->name(), ANALYZER_COLLECTION_NAME)));

    // insert responses for the legacy static analyzers
    for (size_t i = 0, count = EXPECTED_LEGACY_ANALYZERS.size(); i < count; ++i) {
      arangodb::ClusterCommResult response;
      response.operationID = clusterComm.nextOperationId() + i; // sequential value
      response.status = arangodb::ClusterCommOpStatus::CL_COMM_RECEIVED;
      response.answer_code = arangodb::rest::ResponseCode::CREATED;
      response.answer = std::make_shared<GeneralRequestMock>(*vocbase);
      static_cast<GeneralRequestMock*>(response.answer.get())->_payload = *arangodb::velocypack::Parser::fromJson(std::string("{ \"_key\": \"") + std::to_string(response.operationID) + "\" }"); // unique arbitrary key
      clusterComm._responses[0].emplace_back(std::move(response));
    }

    // FIXME TODO uncomment CHECK((false == !ci->getCollection(system->name(), LEGACY_ANALYZER_COLLECTION_NAME)));
    CHECK_THROWS((ci->getCollection(system->name(), ANALYZER_COLLECTION_NAME))); // throws on missing collection
    CHECK((arangodb::methods::Upgrade::clusterBootstrap(*system).ok())); // run system upgrade
    // FIXME TODO uncomment CHECK_THROWS((ci->getCollection(system->name(), LEGACY_ANALYZER_COLLECTION_NAME))); // throws on missing collection
    CHECK((false == !ci->getCollection(system->name(), ANALYZER_COLLECTION_NAME)));

    for (auto& entry: clusterComm._requests) {
      CHECK((entry.second._body));
      auto body = arangodb::velocypack::Parser::fromJson(*(entry.second._body));
      auto slice = body->slice();
      CHECK((slice.isObject()));
      CHECK((slice.get("name").isString()));
      CHECK((1 == expected.erase(slice.get("name").copyString())));
    }

    CHECK((true == expected.empty()));

    // insert responses for the legacy static analyzers
    for (size_t i = 0, count = EXPECTED_LEGACY_ANALYZERS.size(); i < count; ++i) {
      arangodb::ClusterCommResult response;
      response.operationID = clusterComm.nextOperationId() + i; // sequential value
      response.status = arangodb::ClusterCommOpStatus::CL_COMM_RECEIVED;
      response.answer_code = arangodb::rest::ResponseCode::CREATED;
      response.answer = std::make_shared<GeneralRequestMock>(*vocbase);
      static_cast<GeneralRequestMock*>(response.answer.get())->_payload = *arangodb::velocypack::Parser::fromJson(std::string("{ \"_key\": \"") + std::to_string(response.operationID) + "\" }"); // unique arbitrary key
      clusterComm._responses[0].emplace_back(std::move(response));
    }

    clusterComm._requests.clear();
    expected = EXPECTED_LEGACY_ANALYZERS;
    CHECK((false == !ci->getCollection(vocbase->name(), ANALYZER_COLLECTION_NAME)));
    CHECK((arangodb::methods::Upgrade::clusterBootstrap(*vocbase).ok())); // run upgrade
    CHECK((false == !ci->getCollection(vocbase->name(), ANALYZER_COLLECTION_NAME)));
    CHECK((true == clusterComm._responses.empty()));

    for (auto& entry: clusterComm._requests) {
      CHECK((entry.second._body));
      auto body = arangodb::velocypack::Parser::fromJson(*(entry.second._body));
      auto slice = body->slice();
      CHECK((slice.isObject()));
      CHECK((slice.get("name").isString()));
      CHECK((1 == expected.erase(slice.get("name").copyString())));
    }

    CHECK((true == expected.empty()));
  }
}

SECTION("test_visit") {
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
  arangodb::iresearch::IResearchAnalyzerFeature feature(server);
  arangodb::DatabaseFeature* dbFeature;
  server.addFeature(new arangodb::QueryRegistryFeature(server)); // required for constructing TRI_vocbase_t
  server.addFeature(dbFeature = new arangodb::DatabaseFeature(server)); // required for IResearchAnalyzerFeature::emplace(...)
  server.addFeature(new arangodb::SystemDatabaseFeature(server, &system)); // required for IResearchAnalyzerFeature::start()
  server.addFeature(new arangodb::V8DealerFeature(server)); // required for DatabaseFeature::createDatabase(...)

  feature.start();

  CHECK((false == !feature.emplace("test_analyzer0", "TestAnalyzer", "abc0").first));
  CHECK((false == !feature.emplace("test_analyzer1", "TestAnalyzer", "abc1").first));
  CHECK((false == !feature.emplace("test_analyzer2", "TestAnalyzer", "abc2").first));

  // full visitation
  {
    std::set<std::pair<irs::string_ref, irs::string_ref>> expected = {
      std::make_pair<irs::string_ref, irs::string_ref>("test_analyzer0", "abc0"),
      std::make_pair<irs::string_ref, irs::string_ref>("test_analyzer1", "abc1"),
      std::make_pair<irs::string_ref, irs::string_ref>("test_analyzer2", "abc2"),
    };
    auto result = feature.visit([&expected](
      irs::string_ref const& name,
      irs::string_ref const& type,
      irs::string_ref const& properties
    )->bool {
      if (staticAnalyzers().find(name) != staticAnalyzers().end()) {
        return true; // skip static analyzers
      }

      CHECK((type == "TestAnalyzer"));
      CHECK((1 == expected.erase(std::make_pair<irs::string_ref, irs::string_ref>(irs::string_ref(name), irs::string_ref(properties)))));
      return true;
    });
    CHECK((true == result));
    CHECK((expected.empty()));
  }

  // partial visitation
  {
    std::set<std::pair<irs::string_ref, irs::string_ref>> expected = {
      std::make_pair<irs::string_ref, irs::string_ref>("test_analyzer0", "abc0"),
      std::make_pair<irs::string_ref, irs::string_ref>("test_analyzer1", "abc1"),
      std::make_pair<irs::string_ref, irs::string_ref>("test_analyzer2", "abc2"),
    };
    auto result = feature.visit([&expected](
      irs::string_ref const& name,
      irs::string_ref const& type,
      irs::string_ref const& properties
    )->bool {
      if (staticAnalyzers().find(name) != staticAnalyzers().end()) {
        return true; // skip static analyzers
      }

      CHECK((type == "TestAnalyzer"));
      CHECK((1 == expected.erase(std::make_pair<irs::string_ref, irs::string_ref>(irs::string_ref(name), irs::string_ref(properties)))));
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
    CHECK((false == !feature.emplace("vocbase2::test_analyzer3", "TestAnalyzer", "abc3").first));
    CHECK((false == !feature.emplace("vocbase2::test_analyzer4", "TestAnalyzer", "abc4").first));
    CHECK((false == !feature.emplace("vocbase1::test_analyzer5", "TestAnalyzer", "abc5").first));
  }

  // full visitation limited to a vocbase (empty)
  {
    std::set<std::pair<irs::string_ref, irs::string_ref>> expected = {};
    auto result = feature.visit(
      [&expected](
        irs::string_ref const& name,
        irs::string_ref const& type,
        irs::string_ref const& properties
      )->bool {
        if (staticAnalyzers().find(name) != staticAnalyzers().end()) {
          return true; // skip static analyzers
        }

        CHECK((type == "TestAnalyzer"));
        CHECK((1 == expected.erase(std::make_pair<irs::string_ref, irs::string_ref>(irs::string_ref(name), irs::string_ref(properties)))));
        return true;
      },
      vocbase0
    );
    CHECK((true == result));
    CHECK((expected.empty()));
  }

  // full visitation limited to a vocbase (non-empty)
  {
    std::set<std::pair<irs::string_ref, irs::string_ref>> expected = {
      std::make_pair<irs::string_ref, irs::string_ref>("vocbase2::test_analyzer3", "abc3"),
      std::make_pair<irs::string_ref, irs::string_ref>("vocbase2::test_analyzer4", "abc4"),
    };
    auto result = feature.visit(
      [&expected](
        irs::string_ref const& name,
        irs::string_ref const& type,
        irs::string_ref const& properties
      )->bool {
        if (staticAnalyzers().find(name) != staticAnalyzers().end()) {
          return true; // skip static analyzers
        }

        CHECK((type == "TestAnalyzer"));
        CHECK((1 == expected.erase(std::make_pair<irs::string_ref, irs::string_ref>(irs::string_ref(name), irs::string_ref(properties)))));
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