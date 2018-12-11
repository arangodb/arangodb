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
#include "common.h"
#include "StorageEngineMock.h"

#include "analysis/analyzers.hpp"
#include "analysis/token_attributes.hpp"

#include "Aql/AqlFunctionFeature.h"

#if USE_ENTERPRISE
  #include "Enterprise/Ldap/LdapFeature.h"
#endif

#include "GeneralServer/AuthenticationFeature.h"
#include "IResearch/IResearchAnalyzerFeature.h"
#include "IResearch/IResearchCommon.h"
#include "IResearch/VelocyPackHelper.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "RestServer/SystemDatabaseFeature.h"
#include "Sharding/ShardingFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/OperationOptions.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/KeyGenerator.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ManagedDocumentResult.h"

NS_LOCAL

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

NS_END

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

struct IResearchAnalyzerFeatureSetup {
  StorageEngineWrapper engine; // can only nullify 'ENGINE' after all TRI_vocbase_t and ApplicationServer have been destroyed
  std::unique_ptr<Vocbase> system; // ensure destruction after 'server'
  arangodb::application_features::ApplicationServer server;
  std::vector<std::pair<arangodb::application_features::ApplicationFeature*, bool>> features;

  IResearchAnalyzerFeatureSetup(): engine(server), server(nullptr, nullptr) {
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
    arangodb::LogTopic::setLogLevel(arangodb::iresearch::TOPIC.name(), arangodb::LogLevel::FATAL);
    irs::logger::output_le(iresearch::logger::IRL_FATAL, stderr);
  }

  ~IResearchAnalyzerFeatureSetup() {
    arangodb::LogTopic::setLogLevel(arangodb::iresearch::TOPIC.name(), arangodb::LogLevel::DEFAULT);
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

    arangodb::LogTopic::setLogLevel(arangodb::Logger::AUTHENTICATION.name(), arangodb::LogLevel::DEFAULT);
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
  auto* sharding = new arangodb::ShardingFeature(server);
  auto* systemdb = new arangodb::SystemDatabaseFeature(server, s.system.get());

  arangodb::application_features::ApplicationServer::server->addFeature(analyzers);
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
      auto entry = result->at(nullptr, i, mustDestroy, false);
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

SECTION("test_visit") {
  arangodb::iresearch::IResearchAnalyzerFeature feature(s.server);
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
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate tests
////////////////////////////////////////////////////////////////////////////////

}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------