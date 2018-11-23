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
#include "../IResearch/common.h"
#include "../IResearch/StorageEngineMock.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "RestServer/ViewTypesFeature.h"
#include "Sharding/ShardingFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/LogicalView.h"
#include "velocypack/Parser.h"

#include <memory>

namespace {

struct TestView: public arangodb::LogicalView {
  TestView(TRI_vocbase_t& vocbase, arangodb::velocypack::Slice const& definition, uint64_t planVersion)
    : arangodb::LogicalView(vocbase, definition, planVersion) {
  }
  virtual arangodb::Result appendVelocyPack(arangodb::velocypack::Builder&, bool , bool) const override { return arangodb::Result(); }
  virtual arangodb::Result drop() override { deleted(true); return vocbase().dropView(id(), true); }
  virtual void open() override {}
  virtual arangodb::Result rename(std::string&& newName) override { name(std::move(newName)); return arangodb::Result(); }
  virtual arangodb::Result properties(arangodb::velocypack::Slice const&, bool) override { return arangodb::Result(); }
  virtual bool visitCollections(CollectionVisitor const& visitor) const override { return true; }
};

struct ViewFactory: public arangodb::ViewFactory {
  virtual arangodb::Result create(
      arangodb::LogicalView::ptr& view,
      TRI_vocbase_t& vocbase,
      arangodb::velocypack::Slice const& definition
  ) const override {
    view = vocbase.createView(definition);

    return arangodb::Result();
  }

  virtual arangodb::Result instantiate(
      arangodb::LogicalView::ptr& view,
      TRI_vocbase_t& vocbase,
      arangodb::velocypack::Slice const& definition,
      uint64_t planVersion
  ) const override {
    view = std::make_shared<TestView>(vocbase, definition, planVersion);

    return arangodb::Result();
  }
};

}

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

struct VocbaseSetup {
  StorageEngineMock engine;
  arangodb::application_features::ApplicationServer server;
  std::vector<std::pair<arangodb::application_features::ApplicationFeature*, bool>> features;
  ViewFactory viewFactory;

  VocbaseSetup(): engine(server), server(nullptr, nullptr) {
    arangodb::EngineSelectorFeature::ENGINE = &engine;

    // setup required application features
    features.emplace_back(new arangodb::DatabaseFeature(server), false); // required for TRI_vocbase_t::dropCollection(...)
    features.emplace_back(new arangodb::QueryRegistryFeature(server), false); // required for TRI_vocbase_t instantiation
    features.emplace_back(new arangodb::ViewTypesFeature(server), false); // required for TRI_vocbase_t::createView(...)
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

    // register view factory
    arangodb::application_features::ApplicationServer::lookupFeature<
      arangodb::ViewTypesFeature
    >()->emplace(
      arangodb::LogicalDataSource::Type::emplace(
        arangodb::velocypack::StringRef("testViewType")
      ),
      viewFactory
    );
  }

  ~VocbaseSetup() {
    arangodb::application_features::ApplicationServer::server = nullptr;
    arangodb::EngineSelectorFeature::ENGINE = nullptr;

    // destroy application features
    for (auto& f: features) {
      if (f.second) {
        f.first->stop();
      }
    }

    for (auto& f: features) {
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

TEST_CASE("VocbaseTest", "[vocbase]") {
  VocbaseSetup s;
  (void)(s);

SECTION("test_isAllowedName") {
  // direct (non-system)
  {
    CHECK((false == TRI_vocbase_t::IsAllowedName(false, arangodb::velocypack::StringRef(nullptr, 0))));
    CHECK((false == TRI_vocbase_t::IsAllowedName(false, arangodb::velocypack::StringRef(""))));
    CHECK((true == TRI_vocbase_t::IsAllowedName(false, arangodb::velocypack::StringRef("abc123"))));
    CHECK((false == TRI_vocbase_t::IsAllowedName(false, arangodb::velocypack::StringRef("123abc"))));
    CHECK((false == TRI_vocbase_t::IsAllowedName(false, arangodb::velocypack::StringRef("123"))));
    CHECK((false == TRI_vocbase_t::IsAllowedName(false, arangodb::velocypack::StringRef("_123"))));
    CHECK((false == TRI_vocbase_t::IsAllowedName(false, arangodb::velocypack::StringRef("_abc"))));
    CHECK((false == TRI_vocbase_t::IsAllowedName(false, arangodb::velocypack::StringRef("abcdefghijklmnopqrstuvwxyz0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789")))); // longer than TRI_COL_NAME_LENGTH
  }

  // direct (system)
  {
    CHECK((false == TRI_vocbase_t::IsAllowedName(true, arangodb::velocypack::StringRef(nullptr, 0))));
    CHECK((false == TRI_vocbase_t::IsAllowedName(true, arangodb::velocypack::StringRef(""))));
    CHECK((true == TRI_vocbase_t::IsAllowedName(true, arangodb::velocypack::StringRef("abc123"))));
    CHECK((false == TRI_vocbase_t::IsAllowedName(true, arangodb::velocypack::StringRef("123abc"))));
    CHECK((false == TRI_vocbase_t::IsAllowedName(true, arangodb::velocypack::StringRef("123"))));
    CHECK((true == TRI_vocbase_t::IsAllowedName(true, arangodb::velocypack::StringRef("_123"))));
    CHECK((true == TRI_vocbase_t::IsAllowedName(true, arangodb::velocypack::StringRef("_abc"))));
    CHECK((false == TRI_vocbase_t::IsAllowedName(true, arangodb::velocypack::StringRef("abcdefghijklmnopqrstuvwxyz0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789")))); // longer than TRI_COL_NAME_LENGTH
  }

  // slice (default)
  {
    auto json0 = arangodb::velocypack::Parser::fromJson("{ }");
    CHECK((false == TRI_vocbase_t::IsAllowedName(json0->slice())));
    auto json1 = arangodb::velocypack::Parser::fromJson("{ \"name\": \"\" }");
    CHECK((false == TRI_vocbase_t::IsAllowedName(json1->slice())));
    auto json2 = arangodb::velocypack::Parser::fromJson("{ \"name\": \"abc123\" }");
    CHECK((true == TRI_vocbase_t::IsAllowedName(json2->slice())));
    auto json3 = arangodb::velocypack::Parser::fromJson("{ \"name\": \"123abc\" }");
    CHECK((false == TRI_vocbase_t::IsAllowedName(json3->slice())));
    auto json4 = arangodb::velocypack::Parser::fromJson("{ \"name\": \"123\" }");
    CHECK((false == TRI_vocbase_t::IsAllowedName(json4->slice())));
    auto json5 = arangodb::velocypack::Parser::fromJson("{ \"name\": \"_123\" }");
    CHECK((false == TRI_vocbase_t::IsAllowedName(json5->slice())));
    auto json6 = arangodb::velocypack::Parser::fromJson("{ \"name\": \"_abc\" }");
    CHECK((false == TRI_vocbase_t::IsAllowedName(json6->slice())));
    auto json7 = arangodb::velocypack::Parser::fromJson("{ \"name\": \"abcdefghijklmnopqrstuvwxyz0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789\" }");
    CHECK((false == TRI_vocbase_t::IsAllowedName(json7->slice()))); // longer than TRI_COL_NAME_LENGTH
  }

  // slice (non-system)
  {
    auto json0 = arangodb::velocypack::Parser::fromJson("{ \"isSystem\": false }");
    CHECK((false == TRI_vocbase_t::IsAllowedName(json0->slice())));
    auto json1 = arangodb::velocypack::Parser::fromJson("{ \"isSystem\": false, \"name\": \"\" }");
    CHECK((false == TRI_vocbase_t::IsAllowedName(json1->slice())));
    auto json2 = arangodb::velocypack::Parser::fromJson("{ \"isSystem\": false, \"name\": \"abc123\" }");
    CHECK((true == TRI_vocbase_t::IsAllowedName(json2->slice())));
    auto json3 = arangodb::velocypack::Parser::fromJson("{ \"isSystem\": false, \"name\": \"123abc\" }");
    CHECK((false == TRI_vocbase_t::IsAllowedName(json3->slice())));
    auto json4 = arangodb::velocypack::Parser::fromJson("{ \"isSystem\": false, \"name\": \"123\" }");
    CHECK((false == TRI_vocbase_t::IsAllowedName(json4->slice())));
    auto json5 = arangodb::velocypack::Parser::fromJson("{\"isSystem\": false, \"name\": \"_123\" }");
    CHECK((false == TRI_vocbase_t::IsAllowedName(json5->slice())));
    auto json6 = arangodb::velocypack::Parser::fromJson("{ \"isSystem\": false, \"name\": \"_abc\" }");
    CHECK((false == TRI_vocbase_t::IsAllowedName(json6->slice())));
    auto json7 = arangodb::velocypack::Parser::fromJson("{ \"isSystem\": false, \"name\": 123 }");
    CHECK((false == TRI_vocbase_t::IsAllowedName(json7->slice())));
    auto json8 = arangodb::velocypack::Parser::fromJson("{ \"isSystem\": 123, \"name\": \"abc\" }");
    CHECK((true == TRI_vocbase_t::IsAllowedName(json8->slice())));
    auto json9 = arangodb::velocypack::Parser::fromJson("{ \"isSystem\": false, \"name\": \"abcdefghijklmnopqrstuvwxyz0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789\" }");
    CHECK((false == TRI_vocbase_t::IsAllowedName(json9->slice()))); // longer than TRI_COL_NAME_LENGTH
  }

  // slice (system)
  {
    auto json0 = arangodb::velocypack::Parser::fromJson("{ \"isSystem\": true }");
    CHECK((false == TRI_vocbase_t::IsAllowedName(json0->slice())));
    auto json1 = arangodb::velocypack::Parser::fromJson("{ \"isSystem\": true, \"name\": \"\" }");
    CHECK((false == TRI_vocbase_t::IsAllowedName(json1->slice())));
    auto json2 = arangodb::velocypack::Parser::fromJson("{ \"isSystem\": true, \"name\": \"abc123\" }");
    CHECK((true == TRI_vocbase_t::IsAllowedName(json2->slice())));
    auto json3 = arangodb::velocypack::Parser::fromJson("{ \"isSystem\": true, \"name\": \"123abc\" }");
    CHECK((false == TRI_vocbase_t::IsAllowedName(json3->slice())));
    auto json4 = arangodb::velocypack::Parser::fromJson("{ \"isSystem\": true, \"name\": \"123\" }");
    CHECK((false == TRI_vocbase_t::IsAllowedName(json4->slice())));
    auto json5 = arangodb::velocypack::Parser::fromJson("{ \"isSystem\": true, \"name\": \"_123\" }");
    CHECK((true == TRI_vocbase_t::IsAllowedName(json5->slice())));
    auto json6 = arangodb::velocypack::Parser::fromJson("{ \"isSystem\": true, \"name\": \"_abc\" }");
    CHECK((true == TRI_vocbase_t::IsAllowedName(json6->slice())));
    auto json7 = arangodb::velocypack::Parser::fromJson("{ \"isSystem\": true, \"name\": 123 }");
    CHECK((false == TRI_vocbase_t::IsAllowedName(json7->slice())));
    auto json8 = arangodb::velocypack::Parser::fromJson("{ \"isSystem\": 123, \"name\": \"_abc\" }");
    CHECK((false == TRI_vocbase_t::IsAllowedName(json8->slice())));
    auto json9 = arangodb::velocypack::Parser::fromJson("{ \"isSystem\": true, \"name\": \"abcdefghijklmnopqrstuvwxyz0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789\" }");
    CHECK((false == TRI_vocbase_t::IsAllowedName(json9->slice()))); // longer than TRI_COL_NAME_LENGTH
  }
}

SECTION("test_isSystemName") {
  CHECK((false == TRI_vocbase_t::IsSystemName("")));
  CHECK((true == TRI_vocbase_t::IsSystemName("_")));
  CHECK((true == TRI_vocbase_t::IsSystemName("_abc")));
  CHECK((false == TRI_vocbase_t::IsSystemName("abc")));
}

SECTION("test_lookupDataSource") {
  auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"globallyUniqueId\": \"testCollectionGUID\", \"id\": 100, \"name\": \"testCollection\" }");
  auto viewJson = arangodb::velocypack::Parser::fromJson("{ \"id\": 200, \"name\": \"testView\", \"type\": \"testViewType\" }"); // any arbitrary view type
  Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");

  // not present collection (no datasource)
  {
    CHECK((true == !vocbase.lookupDataSource(100)));
    CHECK((true == !vocbase.lookupDataSource("100")));
    CHECK((true == !vocbase.lookupDataSource("testCollection")));
    CHECK((true == !vocbase.lookupDataSource("testCollectionGUID")));
    CHECK((true == !vocbase.lookupCollection(100)));
    CHECK((true == !vocbase.lookupCollection("100")));
    CHECK((true == !vocbase.lookupCollection("testCollection")));
    CHECK((true == !vocbase.lookupCollection("testCollectionGUID")));
  }

  // not present view (no datasource)
  {
    CHECK((true == !vocbase.lookupDataSource(200)));
    CHECK((true == !vocbase.lookupDataSource("200")));
    CHECK((true == !vocbase.lookupDataSource("testView")));
    CHECK((true == !vocbase.lookupDataSource("testViewGUID")));
    CHECK((true == !vocbase.lookupView(200)));
    CHECK((true == !vocbase.lookupView("200")));
    CHECK((true == !vocbase.lookupView("testView")));
    CHECK((true == !vocbase.lookupView("testViewGUID")));
  }

  auto collection = vocbase.createCollection(collectionJson->slice());
  auto view = vocbase.createView(viewJson->slice());

  CHECK((false == collection->deleted()));
  CHECK((false == view->deleted()));

  // not present collection (is view)
  {
    CHECK((false == !vocbase.lookupDataSource(200)));
    CHECK((false == !vocbase.lookupDataSource("200")));
    CHECK((false == !vocbase.lookupDataSource("testView")));
    CHECK((true == !vocbase.lookupDataSource("testViewGUID")));
    CHECK((true == !vocbase.lookupCollection(200)));
    CHECK((true == !vocbase.lookupCollection("200")));
    CHECK((true == !vocbase.lookupCollection("testView")));
    CHECK((true == !vocbase.lookupCollection("testViewGUID")));
    CHECK((true == !vocbase.lookupCollectionByUuid("testView")));
    CHECK((true == !vocbase.lookupCollectionByUuid("testViewGUID")));
  }

  // not preset view (is collection)
  {
    CHECK((false == !vocbase.lookupDataSource(100)));
    CHECK((false == !vocbase.lookupDataSource("100")));
    CHECK((false == !vocbase.lookupDataSource("testCollection")));
    CHECK((false == !vocbase.lookupDataSource("testCollectionGUID")));
    CHECK((true == !vocbase.lookupView(100)));
    CHECK((true == !vocbase.lookupView("100")));
    CHECK((true == !vocbase.lookupView("testCollection")));
    CHECK((true == !vocbase.lookupView("testCollectionGUID")));
  }

  // present collection
  {
    CHECK((false == !vocbase.lookupDataSource(100)));
    CHECK((false == !vocbase.lookupDataSource("100")));
    CHECK((false == !vocbase.lookupDataSource("testCollection")));
    CHECK((false == !vocbase.lookupDataSource("testCollectionGUID")));
    CHECK((false == !vocbase.lookupCollection(100)));
    CHECK((false == !vocbase.lookupCollection("100")));
    CHECK((false == !vocbase.lookupCollection("testCollection")));
    CHECK((false == !vocbase.lookupCollection("testCollectionGUID")));
    CHECK((true == !vocbase.lookupCollectionByUuid("testCollection")));
    CHECK((false == !vocbase.lookupCollectionByUuid("testCollectionGUID")));
  }

  // present view
  {
    CHECK((false == !vocbase.lookupDataSource(200)));
    CHECK((false == !vocbase.lookupDataSource("200")));
    CHECK((false == !vocbase.lookupDataSource("testView")));
    CHECK((true == !vocbase.lookupDataSource("testViewGUID")));
    CHECK((false == !vocbase.lookupView(200)));
    CHECK((false == !vocbase.lookupView("200")));
    CHECK((false == !vocbase.lookupView("testView")));
    CHECK((true == !vocbase.lookupView("testViewGUID")));
  }

  CHECK((true == vocbase.dropCollection(collection->id(), true, 0).ok()));
  CHECK((true == view->drop().ok()));
  CHECK((true == collection->deleted()));
  CHECK((true == view->deleted()));

  // not present collection (deleted)
  {
    CHECK((true == !vocbase.lookupDataSource(100)));
    CHECK((true == !vocbase.lookupDataSource("100")));
    CHECK((true == !vocbase.lookupDataSource("testCollection")));
    CHECK((true == !vocbase.lookupDataSource("testCollectionGUID")));
    CHECK((true == !vocbase.lookupCollection(100)));
    CHECK((true == !vocbase.lookupCollection("100")));
    CHECK((true == !vocbase.lookupCollection("testCollection")));
    CHECK((true == !vocbase.lookupCollection("testCollectionGUID")));
    CHECK((true == !vocbase.lookupCollectionByUuid("testCollection")));
    CHECK((true == !vocbase.lookupCollectionByUuid("testCollectionGUID")));
  }

  // not present view (deleted)
  {
    CHECK((true == !vocbase.lookupDataSource(200)));
    CHECK((true == !vocbase.lookupDataSource("200")));
    CHECK((true == !vocbase.lookupDataSource("testView")));
    CHECK((true == !vocbase.lookupDataSource("testViewGUID")));
    CHECK((true == !vocbase.lookupView(200)));
    CHECK((true == !vocbase.lookupView("200")));
    CHECK((true == !vocbase.lookupView("testView")));
    CHECK((true == !vocbase.lookupView("testViewGUID")));
    CHECK((true == !vocbase.lookupCollectionByUuid("testCollection")));
    CHECK((true == !vocbase.lookupCollectionByUuid("testCollectionGUID")));
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate tests
////////////////////////////////////////////////////////////////////////////////

}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------