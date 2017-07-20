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

#include "IResearch/IResearchAnalyzerFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"

NS_LOCAL

struct TestAttribute: public irs::attribute {
  DECLARE_ATTRIBUTE_TYPE();
  DECLARE_FACTORY_DEFAULT();
};

DEFINE_ATTRIBUTE_TYPE(TestAttribute);
DEFINE_FACTORY_DEFAULT(TestAttribute);

class TestAnalyzer: public irs::analysis::analyzer {
public:
  DECLARE_ANALYZER_TYPE();
  TestAnalyzer(): irs::analysis::analyzer(TestAnalyzer::type()) { _attrs.emplace<TestAttribute>(); }
  virtual irs::attribute_store const& attributes() const NOEXCEPT override { return _attrs; }
  static ptr make(irs::string_ref const& args) { if (args.null()) throw std::exception(); if (args.empty()) return nullptr; PTR_NAMED(TestAnalyzer, ptr); return ptr; }
  virtual bool next() override { return false; }
  virtual bool reset(irs::string_ref const& data) override { return true; }

private:
  irs::attribute_store _attrs;
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

  IResearchAnalyzerFeatureSetup(): server(nullptr, nullptr) {
    arangodb::EngineSelectorFeature::ENGINE = &engine;
    arangodb::application_features::ApplicationFeature* feature;

    arangodb::tests::init();
  }

  ~IResearchAnalyzerFeatureSetup() {
    arangodb::application_features::ApplicationServer::server = nullptr;
    arangodb::EngineSelectorFeature::ENGINE = nullptr;
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
    CHECK((false == !feature.emplace("test_analyzer", "TestAnalyzer", "abc")));
    auto pool = feature.get("test_analyzer");
    CHECK((false == !pool));
    CHECK((irs::flags({TestAttribute::type()}) == pool.features()));
  }

  // add duplicate valid (same name+type+properties)
  {
    arangodb::iresearch::IResearchAnalyzerFeature feature(nullptr);
    CHECK((false == !feature.emplace("test_analyzer", "TestAnalyzer", "abc")));
    auto pool = feature.get("test_analyzer");
    CHECK((false == !pool));
    CHECK((irs::flags({TestAttribute::type()}) == pool.features()));
    CHECK((false == !feature.emplace("test_analyzer", "TestAnalyzer", "abc")));
    CHECK((false == !feature.get("test_analyzer")));
  }

  // add duplicate invalid (same name+type different properties)
  {
    arangodb::iresearch::IResearchAnalyzerFeature feature(nullptr);
    CHECK((false == !feature.emplace("test_analyzer", "TestAnalyzer", "abc")));
    auto pool = feature.get("test_analyzer");
    CHECK((false == !pool));
    CHECK((irs::flags({TestAttribute::type()}) == pool.features()));
    CHECK((true == !feature.emplace("test_analyzer", "TestAnalyzer", "abcd")));
    CHECK((false == !feature.get("test_analyzer")));
  }
  
  // add duplicate invalid (same name+properties different type)
  {
    arangodb::iresearch::IResearchAnalyzerFeature feature(nullptr);
    CHECK((false == !feature.emplace("test_analyzer", "TestAnalyzer", "abc")));
    auto pool = feature.get("test_analyzer");
    CHECK((false == !pool));
    CHECK((irs::flags({TestAttribute::type()}) == pool.features()));
    CHECK((true == !feature.emplace("test_analyzer", "invalid", "abc")));
    CHECK((false == !feature.get("test_analyzer")));
  }

  // add invalid (instance creation failure)
  {
    arangodb::iresearch::IResearchAnalyzerFeature feature(nullptr);
    CHECK((true == !feature.emplace("test_analyzer", "TestAnalyzer", "")));
    CHECK((true == !feature.get("test_analyzer")));
  }

  // add invalid (instance creation exception)
  {
    arangodb::iresearch::IResearchAnalyzerFeature feature(nullptr);
    CHECK((true == !feature.emplace("test_analyzer", "TestAnalyzer", irs::string_ref::nil)));
    CHECK((true == !feature.get("test_analyzer")));
  }

  // add invalid (not registred)
  {
    arangodb::iresearch::IResearchAnalyzerFeature feature(nullptr);
    CHECK((true == !feature.emplace("test_analyzer", "invalid", irs::string_ref::nil)));
    CHECK((true == !feature.get("test_analyzer")));
  }
/* FIXME TODO implement persistence
  // test persisted config
  {
    {
      arangodb::iresearch::IResearchAnalyzerFeature feature(nullptr);
      CHECK((false == !feature.emplace("test_analyzer", "TestAnalyzer", "abc")));
    }

    {
      arangodb::iresearch::IResearchAnalyzerFeature feature(nullptr);
      auto pool = feature.get("test_analyzer");
      CHECK((false == !pool));
      CHECK((irs::flags({TestAttribute::type()}) == pool.features()));
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
    CHECK((irs::flags({TestAttribute::type()}) == pool.features()));
    auto analyzer = pool.get();
    CHECK((false == !analyzer));
  }

  // get invalid
  {
    CHECK((true == !feature.get("invalid")));
  }
}

SECTION("test_remove") {
  // remove existing
  {
    arangodb::iresearch::IResearchAnalyzerFeature feature(nullptr);
    CHECK((false == !feature.emplace("test_analyzer", "TestAnalyzer", "abc")));
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
/* FIXME TODO implement persistence
  // test persisted config
  {
    {
      arangodb::iresearch::IResearchAnalyzerFeature feature(nullptr);
      CHECK((false == !feature.emplace("test_analyzer", "TestAnalyzer", "abc")));
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