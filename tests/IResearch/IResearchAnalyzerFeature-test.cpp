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
#include "IResearch/IResearchAnalyzerFeature.h"
#include "IResearch/VelocyPackHelper.h"
#include "StorageEngine/EngineSelectorFeature.h"

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
  std::vector<std::pair<arangodb::application_features::ApplicationFeature*, bool>> features;

  IResearchAnalyzerFeatureSetup(): server(nullptr, nullptr) {
    arangodb::EngineSelectorFeature::ENGINE = &engine;

    arangodb::tests::init();

    // setup required application features
    features.emplace_back(new arangodb::aql::AqlFunctionFeature(&server), true);

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
    arangodb::LogTopic::setLogLevel(arangodb::Logger::FIXME.name(), arangodb::LogLevel::FATAL);
    irs::logger::output_le(iresearch::logger::IRL_FATAL, stderr);
  }

  ~IResearchAnalyzerFeatureSetup() {
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
    CHECK((false == !feature.emplace("test_analyzer", "TestAnalyzer", "abc").first));
    auto pool = feature.get("test_analyzer");
    CHECK((false == !pool));
    CHECK((irs::flags({TestAttribute::type(), irs::term_attribute::type()}) == pool->features()));
  }

  // add duplicate valid (same name+type+properties)
  {
    arangodb::iresearch::IResearchAnalyzerFeature feature(nullptr);
    CHECK((false == !feature.emplace("test_analyzer", "TestAnalyzer", "abc").first));
    auto pool = feature.get("test_analyzer");
    CHECK((false == !pool));
    CHECK((irs::flags({TestAttribute::type(), irs::term_attribute::type()}) == pool->features()));
    CHECK((false == !feature.emplace("test_analyzer", "TestAnalyzer", "abc").first));
    CHECK((false == !feature.get("test_analyzer")));
  }

  // add duplicate invalid (same name+type different properties)
  {
    arangodb::iresearch::IResearchAnalyzerFeature feature(nullptr);
    CHECK((false == !feature.emplace("test_analyzer", "TestAnalyzer", "abc").first));
    auto pool = feature.get("test_analyzer");
    CHECK((false == !pool));
    CHECK((irs::flags({TestAttribute::type(), irs::term_attribute::type()}) == pool->features()));
    CHECK((true == !feature.emplace("test_analyzer", "TestAnalyzer", "abcd").first));
    CHECK((false == !feature.get("test_analyzer")));
  }
  
  // add duplicate invalid (same name+properties different type)
  {
    arangodb::iresearch::IResearchAnalyzerFeature feature(nullptr);
    CHECK((false == !feature.emplace("test_analyzer", "TestAnalyzer", "abc").first));
    auto pool = feature.get("test_analyzer");
    CHECK((false == !pool));
    CHECK((irs::flags({TestAttribute::type(), irs::term_attribute::type()}) == pool->features()));
    CHECK((true == !feature.emplace("test_analyzer", "invalid", "abc").first));
    CHECK((false == !feature.get("test_analyzer")));
  }

  // add invalid (instance creation failure)
  {
    arangodb::iresearch::IResearchAnalyzerFeature feature(nullptr);
    CHECK((true == !feature.emplace("test_analyzer", "TestAnalyzer", "").first));
    CHECK((true == !feature.get("test_analyzer")));
  }

  // add invalid (instance creation exception)
  {
    arangodb::iresearch::IResearchAnalyzerFeature feature(nullptr);
    CHECK((true == !feature.emplace("test_analyzer", "TestAnalyzer", irs::string_ref::nil).first));
    CHECK((true == !feature.get("test_analyzer")));
  }

  // add invalid (not registred)
  {
    arangodb::iresearch::IResearchAnalyzerFeature feature(nullptr);
    CHECK((true == !feature.emplace("test_analyzer", "invalid", irs::string_ref::nil).first));
    CHECK((true == !feature.get("test_analyzer")));
  }
/* FIXME TODO implement persistence
  // test persisted config
  {
    {
      arangodb::iresearch::IResearchAnalyzerFeature feature(nullptr);
      CHECK((false == !feature.emplace("test_analyzer", "TestAnalyzer", "abc").first));
    }

    {
      arangodb::iresearch::IResearchAnalyzerFeature feature(nullptr);
      auto pool = feature.get("test_analyzer");
      CHECK((false == !pool));
      CHECK((irs::flags({TestAttribute::type(), irs::term_attribute::type()}) == pool->features()));
    }
  }
*/
}

SECTION("test_get") {
  arangodb::iresearch::IResearchAnalyzerFeature feature(nullptr);

  feature.emplace("test_analyzer", "TestAnalyzer", "abc");

  // get valid
  {
    auto pool = feature.get("test_analyzer");
    CHECK((false == !pool));
    CHECK((irs::flags({TestAttribute::type(), irs::term_attribute::type()}) == pool->features()));
    auto analyzer = pool.get();
    CHECK((false == !analyzer));
  }

  // get invalid
  {
    CHECK((true == !feature.get("invalid")));
  }
}

SECTION("test_remove") {
  // FIXME TODO test 'identity'
}

SECTION("test_remove") {
  // FIXME TODO test registration
}

SECTION("test_remove") {
  // remove existing
  {
    arangodb::iresearch::IResearchAnalyzerFeature feature(nullptr);
    CHECK((false == !feature.emplace("test_analyzer", "TestAnalyzer", "abc").first));
    CHECK((false == !feature.get("test_analyzer")));
    CHECK((1 == feature.remove("test_analyzer")));
    CHECK((true == !feature.get("test_analyzer")));
  }

  // remove invalid
  {
    arangodb::iresearch::IResearchAnalyzerFeature feature(nullptr);
    CHECK((true == !feature.get("test_analyzer")));
    CHECK((0 == feature.remove("test_analyzer")));
  }

  // FIXME TODO test force flag

/* FIXME TODO implement persistence
  // test persisted config
  {
    {
      arangodb::iresearch::IResearchAnalyzerFeature feature(nullptr);
      CHECK((false == !feature.emplace("test_analyzer", "TestAnalyzer", "abc").first));
      CHECK((false == !feature.get("test_analyzer")));
    }

    {
      arangodb::iresearch::IResearchAnalyzerFeature feature(nullptr);
      CHECK((false == !feature.get("test_analyzer")));
      CHECK((1 == feature.remove("test_analyzer")));
      CHECK((true == !feature.get("test_analyzer")));
    }

    {
      arangodb::iresearch::IResearchAnalyzerFeature feature(nullptr);
      CHECK((true == !feature.get("test_analyzer")));
    }
  }
*/
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
  arangodb::application_features::ApplicationServer server(nullptr, nullptr);
  auto* analyzers = new arangodb::iresearch::IResearchAnalyzerFeature(&server);
  auto* functions = new arangodb::aql::AqlFunctionFeature(&server);

  arangodb::application_features::ApplicationServer::server->addFeature(analyzers);
  arangodb::application_features::ApplicationServer::server->addFeature(functions);
  analyzers->emplace("test_analyzer", "TestAnalyzer", "abc");

  // test function registration
  {
    arangodb::iresearch::IResearchAnalyzerFeature feature(nullptr);

    // AqlFunctionFeature::byName(..) throws exception instead of returning a nullptr
    CHECK_THROWS((functions->byName("TOKENS")));

    feature.start();
    CHECK((nullptr != functions->byName("TOKENS")));
  }

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