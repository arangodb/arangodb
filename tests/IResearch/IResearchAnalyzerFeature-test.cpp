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
#include "GeneralServer/AuthenticationFeature.h"
#include "IResearch/ApplicationServerHelper.h"
#include "IResearch/IResearchAnalyzerFeature.h"
#include "IResearch/SystemDatabaseFeature.h"
#include "IResearch/VelocyPackHelper.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/FeatureCacheFeature.h"
#include "RestServer/QueryRegistryFeature.h"
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
  DECLARE_FACTORY_DEFAULT();
};

DEFINE_ATTRIBUTE_TYPE(TestAttribute);
DEFINE_FACTORY_DEFAULT(TestAttribute);

struct TestTermAttribute: public irs::term_attribute {
 public:
  iresearch::bytes_ref value_;
  DECLARE_FACTORY_DEFAULT();
  virtual ~TestTermAttribute() {}
  void clear() override { value_ = irs::bytes_ref::nil; }
  virtual const irs::bytes_ref& value() const override { return value_; }
};

DEFINE_FACTORY_DEFAULT(TestTermAttribute);

class TestAnalyzer: public irs::analysis::analyzer {
public:
  DECLARE_ANALYZER_TYPE();
  TestAnalyzer(): irs::analysis::analyzer(TestAnalyzer::type()), _term(_attrs.emplace<TestTermAttribute>()) { _attrs.emplace<TestAttribute>(); }
  virtual irs::attribute_store const& attributes() const NOEXCEPT override { return _attrs; }
  static ptr make(irs::string_ref const& args) { if (args.null()) throw std::exception(); if (args.empty()) return nullptr; PTR_NAMED(TestAnalyzer, ptr); return ptr; }
  virtual bool next() override { if (_data.empty()) return false; _term->value_ = irs::bytes_ref(_data.c_str(), 1); _data = irs::bytes_ref(_data.c_str() + 1, _data.size() - 1); return true; }
  virtual bool reset(irs::string_ref const& data) override { _data = irs::ref_cast<irs::byte_type>(data); return true; }

private:
  irs::attribute_store _attrs;
  irs::bytes_ref _data;
  irs::attribute_store::ref<TestTermAttribute>& _term;
};

DEFINE_ANALYZER_TYPE_NAMED(TestAnalyzer, "TestAnalyzer");
REGISTER_ANALYZER(TestAnalyzer);

NS_END

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

struct IResearchAnalyzerFeatureSetup {
  StorageEngineMock engine;
  arangodb::application_features::ApplicationServer server;
  std::unique_ptr<TRI_vocbase_t> system;
  std::vector<std::pair<arangodb::application_features::ApplicationFeature*, bool>> features;

  IResearchAnalyzerFeatureSetup(): server(nullptr, nullptr) {
    arangodb::EngineSelectorFeature::ENGINE = &engine;

    arangodb::tests::init();

    // setup required application features
    features.emplace_back(new arangodb::AuthenticationFeature(&server), true); // required for FeatureCacheFeature
    features.emplace_back(new arangodb::DatabaseFeature(&server), false); // required for FeatureCacheFeature
    features.emplace_back(new arangodb::FeatureCacheFeature(&server), true); // required for IResearchAnalyzerFeature
    features.emplace_back(new arangodb::QueryRegistryFeature(&server), false); // required for constructing TRI_vocbase_t
    arangodb::application_features::ApplicationServer::server->addFeature(features.back().first);
    system = irs::memory::make_unique<TRI_vocbase_t>(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 0, TRI_VOC_SYSTEM_DATABASE);
    features.emplace_back(new arangodb::aql::AqlFunctionFeature(&server), true); // required for IResearchAnalyzerFeature
    features.emplace_back(new arangodb::iresearch::SystemDatabaseFeature(&server, system.get()), false); // required for IResearchAnalyzerFeature

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

    arangodb::KeyGenerator::Initialize(); // ensure document keys can be generated/validated correctly

    // suppress log messages since tests check error conditions
    arangodb::LogTopic::setLogLevel(arangodb::Logger::FIXME.name(), arangodb::LogLevel::FATAL);
    irs::logger::output_le(iresearch::logger::IRL_FATAL, stderr);
  }

  ~IResearchAnalyzerFeatureSetup() {
    system.reset(); // destroy before reseting the 'ENGINE'
    arangodb::LogTopic::setLogLevel(arangodb::Logger::FIXME.name(), arangodb::LogLevel::DEFAULT);
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

TEST_CASE("IResearchAnalyzerFeatureTest", "[iresearch][iresearch-feature]") {
  IResearchAnalyzerFeatureSetup s;
  UNUSED(s);

SECTION("test_emplace") {
  // add valid
  {
    arangodb::iresearch::IResearchAnalyzerFeature feature(nullptr);
    feature.start();
    CHECK((false == !feature.emplace("test_analyzer0", "TestAnalyzer", "abc").first));
    auto pool = feature.get("test_analyzer0");
    CHECK((false == !pool));
    CHECK((irs::flags({TestAttribute::type(), irs::term_attribute::type()}) == pool->features()));
  }

  // add duplicate valid (same name+type+properties)
  {
    arangodb::iresearch::IResearchAnalyzerFeature feature(nullptr);
    feature.start();
    CHECK((false == !feature.emplace("test_analyzer1", "TestAnalyzer", "abc").first));
    auto pool = feature.get("test_analyzer1");
    CHECK((false == !pool));
    CHECK((irs::flags({TestAttribute::type(), irs::term_attribute::type()}) == pool->features()));
    CHECK((false == !feature.emplace("test_analyzer1", "TestAnalyzer", "abc").first));
    CHECK((false == !feature.get("test_analyzer1")));
  }

  // add duplicate invalid (same name+type different properties)
  {
    arangodb::iresearch::IResearchAnalyzerFeature feature(nullptr);
    feature.start();
    CHECK((false == !feature.emplace("test_analyzer2", "TestAnalyzer", "abc").first));
    auto pool = feature.get("test_analyzer2");
    CHECK((false == !pool));
    CHECK((irs::flags({TestAttribute::type(), irs::term_attribute::type()}) == pool->features()));
    CHECK((true == !feature.emplace("test_analyzer2", "TestAnalyzer", "abcd").first));
    CHECK((false == !feature.get("test_analyzer2")));
  }

  // add duplicate invalid (same name+properties different type)
  {
    arangodb::iresearch::IResearchAnalyzerFeature feature(nullptr);
    feature.start();
    CHECK((false == !feature.emplace("test_analyzer3", "TestAnalyzer", "abc").first));
    auto pool = feature.get("test_analyzer3");
    CHECK((false == !pool));
    CHECK((irs::flags({TestAttribute::type(), irs::term_attribute::type()}) == pool->features()));
    CHECK((true == !feature.emplace("test_analyzer3", "invalid", "abc").first));
    CHECK((false == !feature.get("test_analyzer3")));
  }

  // add invalid (instance creation failure)
  {
    arangodb::iresearch::IResearchAnalyzerFeature feature(nullptr);
    feature.start();
    CHECK((true == !feature.emplace("test_analyzer4", "TestAnalyzer", "").first));
    CHECK((true == !feature.get("test_analyzer4")));
  }

  // add invalid (instance creation exception)
  {
    arangodb::iresearch::IResearchAnalyzerFeature feature(nullptr);
    feature.start();
    CHECK((true == !feature.emplace("test_analyzer5", "TestAnalyzer", irs::string_ref::nil).first));
    CHECK((true == !feature.get("test_analyzer5")));
  }

  // add invalid (not registred)
  {
    arangodb::iresearch::IResearchAnalyzerFeature feature(nullptr);
    feature.start();
    CHECK((true == !feature.emplace("test_analyzer6", "invalid", irs::string_ref::nil).first));
    CHECK((true == !feature.get("test_analyzer6")));
  }

  // add invalalid (feature not started)
  {
    arangodb::iresearch::IResearchAnalyzerFeature feature(nullptr);
    CHECK((true == !feature.emplace("test_analyzer7", "TestAnalyzer", irs::string_ref::nil).first));
    auto pool = feature.get("test_analyzer");
    REQUIRE((false == !pool));
    CHECK((false == !feature.get("test_analyzer7")));
    CHECK((irs::flags::empty_instance() == pool->features()));
    auto analyzer = pool->get();
    CHECK((true == !analyzer));
  }
}

SECTION("test_get") {
  {
    arangodb::iresearch::IResearchAnalyzerFeature feature(nullptr);

    feature.emplace("test_analyzer", "TestAnalyzer", "abc");

    // get valid (not started)
    {
      auto pool = feature.get("test_analyzer");
      REQUIRE((false == !pool));
      CHECK((irs::flags::empty_instance() == pool->features()));
      auto analyzer = pool->get();
      CHECK((true == !analyzer));
    }

    // get invalid (started)
    {
      CHECK((false == !feature.get("invalid")));
    }
  }

  {
    arangodb::iresearch::IResearchAnalyzerFeature feature(nullptr);

    feature.start();
    feature.emplace("test_analyzer", "TestAnalyzer", "abc");

    // get valid (started)
    {
      auto pool = feature.get("test_analyzer");
      REQUIRE((false == !pool));
      CHECK((irs::flags({TestAttribute::type(), irs::term_attribute::type()}) == pool->features()));
      auto analyzer = pool.get();
      CHECK((false == !analyzer));
    }

    // get invalid (started)
    {
      CHECK((true == !feature.get("invalid")));
    }
  }
}

SECTION("test_identity") {
  // test static 'identity'
  {
    auto pool = arangodb::iresearch::IResearchAnalyzerFeature::identity();
    CHECK((false == !pool));
    CHECK((irs::flags({irs::increment::type(), irs::term_attribute::type()}) == pool->features()));
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
    arangodb::iresearch::IResearchAnalyzerFeature feature(nullptr);
    auto pool = feature.get("identity");
    CHECK((false == !pool));
    feature.start();
    pool = feature.get("identity");
    REQUIRE((false == !pool));
    CHECK((irs::flags({irs::increment::type(), irs::term_attribute::type()}) == pool->features()));
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

SECTION("test_persistence") {
  auto* database = arangodb::iresearch::getFeature<arangodb::iresearch::SystemDatabaseFeature>();
  auto vocbase = database->use();

  // ensure there is an empty configuration collection
  {
    auto* collection = vocbase->lookupCollection("_iresearch_analyzers");

    if (collection) {
      vocbase->dropCollection(collection, true, -1);
    }

    collection = vocbase->lookupCollection("_iresearch_analyzers");
    CHECK((nullptr == collection));
    arangodb::iresearch::IResearchAnalyzerFeature feature(nullptr);
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
        arangodb::transaction::StandaloneContext::Create(vocbase.get()),
        collection,
        arangodb::AccessMode::Type::WRITE
      );
      trx.begin();
      trx.truncate(collection, options);
      trx.insert(collection, arangodb::velocypack::Parser::fromJson("{}")->slice(), options);
      trx.insert(collection, arangodb::velocypack::Parser::fromJson("{                        \"type\": \"identity\", \"properties\": null, \"ref_count\": 42}")->slice(), options);
      trx.insert(collection, arangodb::velocypack::Parser::fromJson("{\"name\": 12345,        \"type\": \"identity\", \"properties\": null, \"ref_count\": 42}")->slice(), options);
      trx.insert(collection, arangodb::velocypack::Parser::fromJson("{\"name\": \"invalid1\",                         \"properties\": null, \"ref_count\": 42}")->slice(), options);
      trx.insert(collection, arangodb::velocypack::Parser::fromJson("{\"name\": \"invalid2\", \"type\": 12345,        \"properties\": null, \"ref_count\": 42}")->slice(), options);
      trx.insert(collection, arangodb::velocypack::Parser::fromJson("{\"name\": \"invalid3\", \"type\": \"identity\", \"properties\": null                   }")->slice(), options);
      trx.insert(collection, arangodb::velocypack::Parser::fromJson("{\"name\": \"invalid4\", \"type\": \"identity\", \"properties\": null, \"ref_count\": -1}")->slice(), options);
      trx.commit();
    }

    std::map<irs::string_ref, std::pair<irs::string_ref, irs::string_ref>> expected = {
      { "identity", { "identity", irs::string_ref::nil } },
    };
    arangodb::iresearch::IResearchAnalyzerFeature feature(nullptr);
    feature.start();
    feature.visit([&expected](irs::string_ref const& name, irs::string_ref const& type, irs::string_ref const& properties)->bool {
      auto itr = expected.find(name);
      CHECK((itr != expected.end()));
      CHECK((itr->second.first == type));
      CHECK((itr->second.second == properties));
      expected.erase(itr);
      return true;
    });
    CHECK((expected.empty()));
  }

  // read invalid configuration (duplicate records)
  {
    {
      std::string collection("_iresearch_analyzers");
      arangodb::OperationOptions options;
      arangodb::SingleCollectionTransaction trx(
        arangodb::transaction::StandaloneContext::Create(vocbase.get()),
        collection,
        arangodb::AccessMode::Type::WRITE
      );
      trx.begin();
      trx.truncate(collection, options);
      trx.insert(collection, arangodb::velocypack::Parser::fromJson("{\"name\": \"valid\", \"type\": \"identity\", \"properties\": null, \"ref_count\": 42}")->slice(), options);
      trx.insert(collection, arangodb::velocypack::Parser::fromJson("{\"name\": \"valid\", \"type\": \"identity\", \"properties\": null, \"ref_count\": 52}")->slice(), options);
      trx.commit();
    }

    arangodb::iresearch::IResearchAnalyzerFeature feature(nullptr);
    CHECK_THROWS((feature.start()));
  }

  // read valid configuration (different parameter options)
  {
    {
      std::string collection("_iresearch_analyzers");
      arangodb::OperationOptions options;
      arangodb::SingleCollectionTransaction trx(
        arangodb::transaction::StandaloneContext::Create(vocbase.get()),
        collection,
        arangodb::AccessMode::Type::WRITE
      );
      trx.begin();
      trx.truncate(collection, options);
      trx.insert(collection, arangodb::velocypack::Parser::fromJson("{\"name\": \"valid0\", \"type\": \"identity\", \"properties\": null,                       \"ref_count\": 42}")->slice(), options);
      trx.insert(collection, arangodb::velocypack::Parser::fromJson("{\"name\": \"valid1\", \"type\": \"identity\", \"properties\": true,                       \"ref_count\": 42}")->slice(), options);
      trx.insert(collection, arangodb::velocypack::Parser::fromJson("{\"name\": \"valid2\", \"type\": \"identity\", \"properties\": \"abc\",                    \"ref_count\": 42}")->slice(), options);
      trx.insert(collection, arangodb::velocypack::Parser::fromJson("{\"name\": \"valid3\", \"type\": \"identity\", \"properties\": 3.14,                       \"ref_count\": 42}")->slice(), options);
      trx.insert(collection, arangodb::velocypack::Parser::fromJson("{\"name\": \"valid4\", \"type\": \"identity\", \"properties\": [ 1, \"abc\" ],             \"ref_count\": 42}")->slice(), options);
      trx.insert(collection, arangodb::velocypack::Parser::fromJson("{\"name\": \"valid5\", \"type\": \"identity\", \"properties\": { \"a\": 7, \"b\": \"c\" }, \"ref_count\": 42}")->slice(), options);
      trx.commit();
    }

    std::map<irs::string_ref, std::pair<irs::string_ref, irs::string_ref>> expected = {
      { "identity", { "identity", irs::string_ref::nil } },
      { "valid0", { "identity", "null" } },
      { "valid1", { "identity", "true" } },
      { "valid2", { "identity", "\"abc\"" } },
      { "valid3", { "identity", "3.14" } },
      { "valid4", { "identity", "[1,\"abc\"]" } },
      { "valid5", { "identity", "{\"a\":7,\"b\":\"c\"}" } },
    };
    arangodb::iresearch::IResearchAnalyzerFeature feature(nullptr);
    feature.start();
    feature.visit([&expected](irs::string_ref const& name, irs::string_ref const& type, irs::string_ref const& properties)->bool {
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
      TRI_voc_tick_t resultMarkerTick;
      auto* collection = vocbase->lookupCollection("_iresearch_analyzers");
      collection->truncate(nullptr, options);
    }

    {
      arangodb::iresearch::IResearchAnalyzerFeature feature(nullptr);
      feature.start();
      auto result = feature.emplace("valid", "identity", "abc");
      CHECK((result.first));
      CHECK((result.second));
    }

    {
      std::map<irs::string_ref, std::pair<irs::string_ref, irs::string_ref>> expected = {
        { "identity", { "identity", irs::string_ref::nil } },
        { "valid", { "identity", "\"abc\"" } },
      };
      arangodb::iresearch::IResearchAnalyzerFeature feature(nullptr);
      feature.start();
      feature.visit([&expected](irs::string_ref const& name, irs::string_ref const& type, irs::string_ref const& properties)->bool {
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
        arangodb::transaction::StandaloneContext::Create(vocbase.get()),
        collection,
        arangodb::AccessMode::Type::WRITE
      );
      trx.begin();
      trx.truncate(collection, options);
      trx.insert(collection, arangodb::velocypack::Parser::fromJson("{\"name\": \"valid0\", \"type\": \"identity\", \"properties\": null, \"ref_count\": 0}")->slice(), options);
      trx.insert(collection, arangodb::velocypack::Parser::fromJson("{\"name\": \"valid1\", \"type\": \"identity\", \"properties\": null, \"ref_count\": 1}")->slice(), options);
      trx.commit();
    }

    {
      std::map<irs::string_ref, std::pair<irs::string_ref, irs::string_ref>> expected = {
        { "identity", { "identity", irs::string_ref::nil } },
        { "valid0", { "identity", "null" } },
        { "valid1", { "identity", "null" } },
      };
      arangodb::iresearch::IResearchAnalyzerFeature feature(nullptr);
      feature.start();
      feature.visit([&expected](irs::string_ref const& name, irs::string_ref const& type, irs::string_ref const& properties)->bool {
        auto itr = expected.find(name);
        CHECK((itr != expected.end()));
        CHECK((itr->second.first == type));
        CHECK((itr->second.second == properties));
        expected.erase(itr);
        return true;
      });
      CHECK((expected.empty()));
      CHECK((1 == feature.erase("valid0")));
      CHECK((0 == feature.erase("valid1")));
      CHECK((1 == feature.erase("valid1", true))); // force removal
    }

    {
      std::map<irs::string_ref, std::pair<irs::string_ref, irs::string_ref>> expected = {
        { "identity", { "identity", irs::string_ref::nil } },
      };
      arangodb::iresearch::IResearchAnalyzerFeature feature(nullptr);
      feature.start();
      feature.visit([&expected](irs::string_ref const& name, irs::string_ref const& type, irs::string_ref const& properties)->bool {
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

  // update records (reserve/remove pool)
  {
    {
      std::string collection("_iresearch_analyzers");
      arangodb::OperationOptions options;
      arangodb::SingleCollectionTransaction trx(
        arangodb::transaction::StandaloneContext::Create(vocbase.get()),
        collection,
        arangodb::AccessMode::Type::WRITE
      );
      trx.begin();
      trx.truncate(collection, options);
      trx.insert(collection, arangodb::velocypack::Parser::fromJson("{\"name\": \"valid0\", \"type\": \"identity\", \"properties\": null, \"ref_count\": 0}")->slice(), options);
      trx.insert(collection, arangodb::velocypack::Parser::fromJson("{\"name\": \"valid1\", \"type\": \"identity\", \"properties\": null, \"ref_count\": 1}")->slice(), options);
      trx.insert(collection, arangodb::velocypack::Parser::fromJson("{\"name\": \"valid2\", \"type\": \"identity\", \"properties\": null, \"ref_count\": 2}")->slice(), options);
      trx.commit();
    }

    {
      arangodb::iresearch::IResearchAnalyzerFeature feature(nullptr);
      feature.start();
      auto pool = feature.get("valid0");
      CHECK((nullptr != pool));
      CHECK((feature.reserve(pool)));
      pool = feature.get("valid1");
      CHECK((nullptr != pool));
      CHECK((feature.release(pool)));
      CHECK((!feature.release(pool))); // no more references
    }

    // test reserve (remove failure)
    {
      arangodb::iresearch::IResearchAnalyzerFeature feature(nullptr);
      feature.start();
      CHECK((0 == feature.erase("valid0")));
      CHECK((1 == feature.erase("valid1")));
      auto pool = feature.get("valid0");
      CHECK((nullptr != pool));
      CHECK((feature.release(pool)));
    }

    // test remove (remove allowed)
    {
      arangodb::iresearch::IResearchAnalyzerFeature feature(nullptr);
      feature.start();
      CHECK((1 == feature.erase("valid0")));
    }
  }

  // reserve to overflow (on startup)
  {
    {
      auto refCount = std::to_string(std::numeric_limits<uint64_t>::max());
      std::string collection("_iresearch_analyzers");
      arangodb::OperationOptions options;
      arangodb::SingleCollectionTransaction trx(
        arangodb::transaction::StandaloneContext::Create(vocbase.get()),
        collection,
        arangodb::AccessMode::Type::WRITE
      );
      trx.begin();
      trx.truncate(collection, options);
      trx.insert(collection, arangodb::velocypack::Parser::fromJson(std::string("{\"name\": \"valid\", \"type\": \"identity\", \"properties\": null, \"ref_count\": ") + refCount + "}")->slice(), options);
      trx.commit();
    }

    {
      arangodb::iresearch::IResearchAnalyzerFeature feature(nullptr);
      auto pool = feature.get("valid");
      CHECK((nullptr != pool));
      CHECK((feature.reserve(pool)));
      CHECK_THROWS((feature.start()));
    }
  }

  // store to overflow (on reserve, reserve valid for 2^32 more reservations)
  {
    {
      auto refCount = std::to_string(std::numeric_limits<int64_t>::max());
      std::string collection("_iresearch_analyzers");
      arangodb::OperationOptions options;
      arangodb::SingleCollectionTransaction trx(
        arangodb::transaction::StandaloneContext::Create(vocbase.get()),
        collection,
        arangodb::AccessMode::Type::WRITE
      );
      trx.begin();
      trx.truncate(collection, options);
      trx.insert(collection, arangodb::velocypack::Parser::fromJson(std::string("{\"name\": \"valid\", \"type\": \"identity\", \"properties\": null, \"ref_count\": ") + refCount + "}")->slice(), options);
      trx.commit();
    }

    {
      arangodb::iresearch::IResearchAnalyzerFeature feature(nullptr);
      feature.start();
      auto pool = feature.get("valid");
      CHECK((nullptr != pool));
      CHECK((feature.reserve(pool)));
    }

    {
      arangodb::iresearch::IResearchAnalyzerFeature feature(nullptr);
      CHECK_THROWS((feature.start()));
    }
  }
}

SECTION("test_registration") {
  auto* database = arangodb::iresearch::getFeature<arangodb::iresearch::SystemDatabaseFeature>();
  auto vocbase = database->use();

  // reserve pool (not started)
  {
    arangodb::iresearch::IResearchAnalyzerFeature feature(nullptr);
    auto pool = feature.get("valid");
    CHECK((false == !pool));
    CHECK((feature.reserve(pool)));
    CHECK((0 == feature.erase("valid"))); // remove not allowed
  }

  // release pool (not started)
  {
    arangodb::iresearch::IResearchAnalyzerFeature feature(nullptr);
    auto pool = feature.get("valid");
    CHECK((false == !pool));
    CHECK((feature.reserve(pool)));
    CHECK((feature.reserve(pool))); // 2nd time
    CHECK((feature.release(pool)));
    CHECK((0 == feature.erase("valid"))); // remove not allowed
    CHECK((feature.release(pool)));
    CHECK((1 == feature.erase("valid")));
  }

  // reserve pool (persistance failure)
  {
    arangodb::iresearch::IResearchAnalyzerFeature feature(nullptr);
    feature.start(); // create configuration collection
    auto entry = feature.emplace("valid", "identity", nullptr);
    CHECK((false == !entry.first));

    {
      // simulate temporary failure
      auto before = PhysicalCollectionMock::before;
      auto restore = irs::make_finally([before]()->void { PhysicalCollectionMock::before =before; });
      PhysicalCollectionMock::before = []()->void { throw "exception"; };

      CHECK((!feature.reserve(entry.first)));
    }

    CHECK((1 == feature.erase("valid"))); // remove allowed
  }

  // release pool (persistence failure)
  {
    // ensure no persisted configuration present
    {
      auto* collection = vocbase->lookupCollection("_iresearch_analyzers");
      vocbase->dropCollection(collection, true, -1); // drop configuration collection
    }

    arangodb::iresearch::IResearchAnalyzerFeature feature(nullptr);
    feature.start(); // create configuration collection
    auto entry = feature.emplace("valid", "identity", nullptr);
    CHECK((false == !entry.first));
    CHECK((entry.second));
    CHECK((feature.reserve(entry.first)));

    {
      // simulate temporary failure
      auto before = PhysicalCollectionMock::before;
      auto restore = irs::make_finally([before]()->void { PhysicalCollectionMock::before =before; });
      PhysicalCollectionMock::before = []()->void { throw "exception"; };

      CHECK((!feature.release(entry.first)));
    }

    CHECK((0 == feature.erase("valid"))); // remove not allowed
  }

  // reserve/release pool (persisted OK)
  {
    // ensure no persisted configuration present
    {
      auto* collection = vocbase->lookupCollection("_iresearch_analyzers");
      vocbase->dropCollection(collection, true, -1); // drop configuration collection
    }

    {
      arangodb::iresearch::IResearchAnalyzerFeature feature(nullptr);
      feature.start();
      auto entry = feature.emplace("valid", "identity", nullptr);
      CHECK((nullptr != entry.first));
      CHECK((entry.second));
      CHECK((!feature.release(entry.first)));
      CHECK((feature.reserve(entry.first)));
      CHECK((0 == feature.erase("valid"))); // remove denied
    }

    {
      arangodb::iresearch::IResearchAnalyzerFeature feature(nullptr);
      feature.start();
      auto pool = feature.get("valid");
      CHECK((nullptr != pool));
      CHECK((feature.release(pool)));
    }

    {
      arangodb::iresearch::IResearchAnalyzerFeature feature(nullptr);
      feature.start();
      auto pool = feature.get("valid");
      CHECK((nullptr != pool));
      CHECK((!feature.release(pool)));
      CHECK((1 == feature.erase("valid"))); // remove allowed
    }
  }

  // reserve not started + started
  {
    // ensure no persisted configuration present
    {
      auto* collection = vocbase->lookupCollection("_iresearch_analyzers");
      vocbase->dropCollection(collection, true, -1); // drop configuration collection
    }

    {
      arangodb::iresearch::IResearchAnalyzerFeature feature(nullptr);
      feature.start();
      auto pool = feature.emplace("valid", "identity", nullptr).first;
      CHECK((false == !pool));
      CHECK((feature.reserve(pool)));
    }

    {
      arangodb::iresearch::IResearchAnalyzerFeature feature(nullptr);
      auto pool = feature.get("valid");
      CHECK((false == !pool));
      CHECK((feature.reserve(pool)));
      feature.start();
      CHECK((0 == feature.erase("valid"))); // remove denied
      CHECK((feature.release(pool)));
      CHECK((0 == feature.erase("valid"))); // remove denied
      CHECK((feature.release(pool)));
      CHECK((1 == feature.erase("valid"))); // remove allowed
    }
  }
/* FIXME TODO store actual value as null or json string, load as json string
  // reserve identity not started + started
  {
    // ensure no persisted configuration present
    {
      auto* collection = vocbase->lookupCollection("_iresearch_analyzers");
      vocbase->dropCollection(collection, true, -1); // drop configuration collection
    }

    {
      arangodb::iresearch::IResearchAnalyzerFeature feature(nullptr);
      feature.start();
      auto pool = arangodb::iresearch::IResearchAnalyzerFeature::identity();
      CHECK((false == !pool));
      CHECK((feature.reserve(pool)));
    }

    {
      arangodb::iresearch::IResearchAnalyzerFeature feature(nullptr);
      auto pool = arangodb::iresearch::IResearchAnalyzerFeature::identity();
      CHECK((false == !pool));
      CHECK((feature.reserve(pool)));
      feature.start();
      CHECK((0 == feature.erase(pool->name()))); // remove denied
      CHECK((feature.release(pool)));
      CHECK((0 == feature.erase(pool->name()))); // remove denied
      CHECK((feature.release(pool)));
      CHECK((1 == feature.erase(pool->name()))); // remove allowed
    }
  }
*/
}

SECTION("test_remove") {
  // remove (not started)
  {
    arangodb::iresearch::IResearchAnalyzerFeature feature(nullptr);
    CHECK((0 == feature.erase("test_analyzer")));
    CHECK((false == !feature.get("test_analyzer")));
    CHECK((1 == feature.erase("test_analyzer")));
  }

  // remove existing (started)
  {
    arangodb::iresearch::IResearchAnalyzerFeature feature(nullptr);
    feature.start();
    CHECK((false == !feature.emplace("test_analyzer", "TestAnalyzer", "abc").first));
    CHECK((false == !feature.get("test_analyzer")));
    CHECK((1 == feature.erase("test_analyzer")));
    CHECK((true == !feature.get("test_analyzer")));
  }

  // remove invalid (started)
  {
    arangodb::iresearch::IResearchAnalyzerFeature feature(nullptr);
    feature.start();
    CHECK((true == !feature.get("test_analyzer")));
    CHECK((0 == feature.erase("test_analyzer")));
  }
}

SECTION("test_tokens") {
  auto* database = arangodb::iresearch::getFeature<arangodb::iresearch::SystemDatabaseFeature>();
  auto vocbase = database->use();

  // create a new instance of an ApplicationServer and fill it with the required features
  // cannot use the existing server since its features already have some state
  std::shared_ptr<arangodb::application_features::ApplicationServer> originalServer(
    arangodb::application_features::ApplicationServer::server,
    [](arangodb::application_features::ApplicationServer* ptr)->void {
      arangodb::application_features::ApplicationServer::server = ptr;
    }
  );
  arangodb::application_features::ApplicationServer server(nullptr, nullptr);
  auto* analyzers = new arangodb::iresearch::IResearchAnalyzerFeature(&server);
  auto* functions = new arangodb::aql::AqlFunctionFeature(&server);
  auto* systemdb = new arangodb::iresearch::SystemDatabaseFeature(&server, s.system.get());

  arangodb::application_features::ApplicationServer::server->addFeature(analyzers);
  arangodb::application_features::ApplicationServer::server->addFeature(functions);
  arangodb::application_features::ApplicationServer::server->addFeature(systemdb);

  // ensure there is no configuration collection
  {
    auto* collection = vocbase->lookupCollection("_iresearch_analyzers");

    if (collection) {
      vocbase->dropCollection(collection, true, -1);
    }

    collection = vocbase->lookupCollection("_iresearch_analyzers");
    CHECK((nullptr == collection));
  }

  // test function registration
  {
    arangodb::iresearch::IResearchAnalyzerFeature feature(nullptr);

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
    arangodb::SmallVector<arangodb::aql::AqlValue>::allocator_type::arena_type arena;
    arangodb::aql::VPackFunctionParameters args{arena};
    args.emplace_back(data.c_str(), data.size());
    args.emplace_back(analyzer.c_str(), analyzer.size());
    auto result = impl(nullptr, nullptr, args);
    CHECK((result.isArray()));
    CHECK((26 == result.length()));

    for (int64_t i = 0; i < 26; ++i) {
      bool mustDestroy;
      auto entry = result.at(nullptr, i, mustDestroy, false);
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
    auto result = impl(nullptr, nullptr, args);
    CHECK((result.isNone()));
  }

  // test invalid data type
  {
    auto* function = functions->byName("TOKENS");
    CHECK((nullptr != functions->byName("TOKENS")));
    auto& impl = function->implementation;
    CHECK((false == !impl));

    irs::string_ref data("abcdefghijklmnopqrstuvwxyz");
    arangodb::SmallVector<arangodb::aql::AqlValue>::allocator_type::arena_type arena;
    arangodb::aql::VPackFunctionParameters args{arena};
    args.emplace_back(data.c_str(), data.size());
    args.emplace_back(123.4);
    auto result = impl(nullptr, nullptr, args);
    CHECK((result.isNone()));
  }

  // test invalid analyzer type
  {
    auto* function = functions->byName("TOKENS");
    CHECK((nullptr != functions->byName("TOKENS")));
    auto& impl = function->implementation;
    CHECK((false == !impl));

    irs::string_ref analyzer("test_analyzer");
    arangodb::SmallVector<arangodb::aql::AqlValue>::allocator_type::arena_type arena;
    arangodb::aql::VPackFunctionParameters args{arena};
    args.emplace_back(123.4);
    args.emplace_back(analyzer.c_str(), analyzer.size());
    auto result = impl(nullptr, nullptr, args);
    CHECK((result.isNone()));
  }

  // test invalid analyzer
  {
    auto* function = functions->byName("TOKENS");
    CHECK((nullptr != functions->byName("TOKENS")));
    auto& impl = function->implementation;
    CHECK((false == !impl));

    irs::string_ref analyzer("invalid");
    irs::string_ref data("abcdefghijklmnopqrstuvwxyz");
    arangodb::SmallVector<arangodb::aql::AqlValue>::allocator_type::arena_type arena;
    arangodb::aql::VPackFunctionParameters args{arena};
    args.emplace_back(data.c_str(), data.size());
    args.emplace_back(analyzer.c_str(), analyzer.size());
    auto result = impl(nullptr, nullptr, args);
    CHECK((result.isNone()));
  }
}

SECTION("test_visit") {
  arangodb::iresearch::IResearchAnalyzerFeature feature(nullptr);
  feature.start();
  feature.erase("identity"); // remove to simplify test implementation
  feature.emplace("test_analyzer0", "TestAnalyzer", "abc0");
  feature.emplace("test_analyzer1", "TestAnalyzer", "abc1");
  feature.emplace("test_analyzer2", "TestAnalyzer", "abc2");

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
