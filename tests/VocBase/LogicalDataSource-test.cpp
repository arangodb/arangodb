////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
#include "../IResearch/StorageEngineMock.h"
#include "RestServer/QueryRegistryFeature.h"
#include "Sharding/ShardingFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "velocypack/Parser.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/LogicalView.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

struct LogicalDataSourceSetup {
  StorageEngineMock engine;
  arangodb::application_features::ApplicationServer server;
  std::vector<std::pair<arangodb::application_features::ApplicationFeature*, bool>> features;

  LogicalDataSourceSetup(): engine(server), server(nullptr, nullptr) {
    arangodb::EngineSelectorFeature::ENGINE = &engine;

    // setup required application features
    features.emplace_back(new arangodb::QueryRegistryFeature(server), false); // required for TRI_vocbase_t
    features.emplace_back(new arangodb::ShardingFeature(server), false);

    for (auto& f: features) {
      arangodb::application_features::ApplicationServer::server->addFeature(f.first);
    }

    for (auto& f: features) {
      f.first->prepare();
    }

    for (auto& f: features) {
      if (f.second) {
        f.first->start();
      }
    }
  }

  ~LogicalDataSourceSetup() {
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

TEST_CASE("LogicalDataSourceTest", "[vocbase]") {
  LogicalDataSourceSetup s;
  (void)(s);

SECTION("test_category") {
  // LogicalCollection
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
    arangodb::LogicalCollection instance(vocbase, json->slice(), true);

    CHECK((arangodb::LogicalCollection::category() == instance.category()));
  }

  // LogicalView
  {
    class LogicalViewImpl: public arangodb::LogicalView {
     public:
      LogicalViewImpl(TRI_vocbase_t& vocbase, arangodb::velocypack::Slice const& definition)
        : LogicalView(vocbase, definition, 0) {
      }
      virtual arangodb::Result drop() override { return arangodb::Result(); }
      virtual void open() override {}
      virtual arangodb::Result rename(std::string&& newName) override { return arangodb::Result(); }
      virtual arangodb::Result properties(arangodb::velocypack::Slice const& properties, bool partialUpdate) override { return arangodb::Result(); }
      virtual bool visitCollections(CollectionVisitor const& visitor) const override { return true; }
    };

    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\" }");
    LogicalViewImpl instance(vocbase, json->slice());

    CHECK((arangodb::LogicalView::category() == instance.category()));
  }
}

SECTION("test_construct") {
  // LogicalCollection
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto json = arangodb::velocypack::Parser::fromJson("{ \"id\": 1, \"planId\": 2, \"globallyUniqueId\": \"abc\", \"name\": \"testCollection\" }");
    arangodb::LogicalCollection instance(vocbase, json->slice(), true);

    CHECK((1 == instance.id()));
    CHECK((2 == instance.planId()));
    CHECK((std::string("abc") == instance.guid()));
  }

  // LogicalView
  {
    class LogicalViewImpl: public arangodb::LogicalView {
     public:
      LogicalViewImpl(TRI_vocbase_t& vocbase, arangodb::velocypack::Slice const& definition)
        : LogicalView(vocbase, definition, 0) {
      }
      virtual arangodb::Result drop() override { return arangodb::Result(); }
      virtual void open() override {}
      virtual arangodb::Result rename(std::string&& newName) override { return arangodb::Result(); }
      virtual arangodb::Result properties(arangodb::velocypack::Slice const& properties, bool partialUpdate) override { return arangodb::Result(); }
      virtual bool visitCollections(CollectionVisitor const& visitor) const override { return true; }
    };

    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto json = arangodb::velocypack::Parser::fromJson("{ \"id\": 1, \"planId\": 2, \"globallyUniqueId\": \"abc\", \"name\": \"testView\" }");
    LogicalViewImpl instance(vocbase, json->slice());

    CHECK((1 == instance.id()));
    CHECK((2 == instance.planId()));
    CHECK((std::string("abc") == instance.guid()));
  }
}

SECTION("test_defaults") {
  // LogicalCollection
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testCollection\" }");
    arangodb::LogicalCollection instance(vocbase, json->slice(), true);

    CHECK((0 != instance.id()));
    CHECK((0 != instance.planId()));
    CHECK((false == instance.guid().empty()));
  }

  // LogicalView
  {
    class LogicalViewImpl: public arangodb::LogicalView {
     public:
      LogicalViewImpl(TRI_vocbase_t& vocbase, arangodb::velocypack::Slice const& definition)
        : LogicalView(vocbase, definition, 0) {
      }
      virtual arangodb::Result drop() override { return arangodb::Result(); }
      virtual void open() override {}
      virtual arangodb::Result rename(std::string&& newName) override { return arangodb::Result(); }
      virtual arangodb::Result properties(arangodb::velocypack::Slice const& properties, bool partialUpdate) override { return arangodb::Result(); }
      virtual bool visitCollections(CollectionVisitor const& visitor) const override { return true; }
    };

    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\" }");
    LogicalViewImpl instance(vocbase, json->slice());

    CHECK((0 != instance.id()));
    CHECK((0 != instance.planId()));
    CHECK((instance.id() == instance.planId()));
    CHECK((false == instance.guid().empty()));
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate tests
////////////////////////////////////////////////////////////////////////////////

}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
