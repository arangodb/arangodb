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
#include "Utils/CollectionNameResolver.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/LogicalView.h"
#include "velocypack/Parser.h"

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

struct CollectionNameResolverSetup {
  StorageEngineMock engine;
  arangodb::application_features::ApplicationServer server;
  std::vector<std::pair<arangodb::application_features::ApplicationFeature*, bool>> features;
  ViewFactory viewFactory;

  CollectionNameResolverSetup(): engine(server), server(nullptr, nullptr) {
    arangodb::EngineSelectorFeature::ENGINE = &engine;

    // setup required application features
    features.emplace_back(new arangodb::DatabaseFeature(server), false); // required for TRI_vocbase_t::dropCollection(...)
    features.emplace_back(new arangodb::QueryRegistryFeature(server), false); // required for TRI_vocbase_t instantiation
    features.emplace_back(new arangodb::ShardingFeature(server), false);
    features.emplace_back(new arangodb::ViewTypesFeature(server), false); // required for TRI_vocbase_t::createView(...)

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

  ~CollectionNameResolverSetup() {
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

TEST_CASE("CollectionNameResolverTest", "[vocbase]") {
  CollectionNameResolverSetup s;
  (void)(s);

SECTION("test_getDataSource") {
  auto collectionJson = arangodb::velocypack::Parser::fromJson("{ \"globallyUniqueId\": \"testCollectionGUID\", \"id\": 100, \"name\": \"testCollection\" }");
  auto viewJson = arangodb::velocypack::Parser::fromJson("{ \"id\": 200, \"name\": \"testView\", \"type\": \"testViewType\" }"); // any arbitrary view type
  Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
  arangodb::CollectionNameResolver resolver(vocbase);

  // not present collection (no datasource)
  {
    CHECK((true == !resolver.getDataSource(100)));
    CHECK((true == !resolver.getDataSource("100")));
    CHECK((true == !resolver.getDataSource("testCollection")));
    CHECK((true == !resolver.getDataSource("testCollectionGUID")));
    CHECK((true == !resolver.getCollection(100)));
    CHECK((true == !resolver.getCollection("100")));
    CHECK((true == !resolver.getCollection("testCollection")));
    CHECK((true == !resolver.getCollection("testCollectionGUID")));
  }

  // not present view (no datasource)
  {
    CHECK((true == !resolver.getDataSource(200)));
    CHECK((true == !resolver.getDataSource("200")));
    CHECK((true == !resolver.getDataSource("testView")));
    CHECK((true == !resolver.getDataSource("testViewGUID")));
    CHECK((true == !resolver.getView(200)));
    CHECK((true == !resolver.getView("200")));
    CHECK((true == !resolver.getView("testView")));
    CHECK((true == !resolver.getView("testViewGUID")));
  }

  auto collection = vocbase.createCollection(collectionJson->slice());
  auto view = vocbase.createView(viewJson->slice());

  CHECK((false == collection->deleted()));
  CHECK((false == view->deleted()));

  // not present collection (is view)
  {
    CHECK((false == !resolver.getDataSource(200)));
    CHECK((false == !resolver.getDataSource("200")));
    CHECK((false == !resolver.getDataSource("testView")));
    CHECK((true == !resolver.getDataSource("testViewGUID")));
    CHECK((true == !resolver.getCollection(200)));
    CHECK((true == !resolver.getCollection("200")));
    CHECK((true == !resolver.getCollection("testView")));
    CHECK((true == !resolver.getCollection("testViewGUID")));
  }

  // not preset view (is collection)
  {
    CHECK((false == !resolver.getDataSource(100)));
    CHECK((false == !resolver.getDataSource("100")));
    CHECK((false == !resolver.getDataSource("testCollection")));
    CHECK((false == !resolver.getDataSource("testCollectionGUID")));
    CHECK((true == !resolver.getView(100)));
    CHECK((true == !resolver.getView("100")));
    CHECK((true == !resolver.getView("testCollection")));
    CHECK((true == !resolver.getView("testCollectionGUID")));
  }

  // present collection
  {
    CHECK((false == !resolver.getDataSource(100)));
    CHECK((false == !resolver.getDataSource("100")));
    CHECK((false == !resolver.getDataSource("testCollection")));
    CHECK((false == !resolver.getDataSource("testCollectionGUID")));
    CHECK((false == !resolver.getCollection(100)));
    CHECK((false == !resolver.getCollection("100")));
    CHECK((false == !resolver.getCollection("testCollection")));
    CHECK((false == !resolver.getCollection("testCollectionGUID")));
  }

  // present view
  {
    CHECK((false == !resolver.getDataSource(200)));
    CHECK((false == !resolver.getDataSource("200")));
    CHECK((false == !resolver.getDataSource("testView")));
    CHECK((true == !resolver.getDataSource("testViewGUID")));
    CHECK((false == !resolver.getView(200)));
    CHECK((false == !resolver.getView("200")));
    CHECK((false == !resolver.getView("testView")));
    CHECK((true == !resolver.getView("testViewGUID")));
  }

  CHECK((true == vocbase.dropCollection(collection->id(), true, 0).ok()));
  CHECK((true == view->drop().ok()));
  CHECK((true == collection->deleted()));
  CHECK((true == view->deleted()));

  // present collection (deleted, cached)
  {
    CHECK((false == !resolver.getDataSource(100)));
    CHECK((false == !resolver.getDataSource("100")));
    CHECK((false == !resolver.getDataSource("testCollection")));
    CHECK((false == !resolver.getDataSource("testCollectionGUID")));
    CHECK((false == !resolver.getCollection(100)));
    CHECK((false == !resolver.getCollection("100")));
    CHECK((false == !resolver.getCollection("testCollection")));
    CHECK((false == !resolver.getCollection("testCollectionGUID")));
    CHECK((true == resolver.getCollection(100)->deleted()));
  }

  // present view (deleted, cached)
  {
    CHECK((false == !resolver.getDataSource(200)));
    CHECK((false == !resolver.getDataSource("200")));
    CHECK((false == !resolver.getDataSource("testView")));
    CHECK((true == !resolver.getDataSource("testViewGUID")));
    CHECK((false == !resolver.getView(200)));
    CHECK((false == !resolver.getView("200")));
    CHECK((false == !resolver.getView("testView")));
    CHECK((true == !resolver.getView("testViewGUID")));
    CHECK((true == resolver.getView(200)->deleted()));
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate tests
////////////////////////////////////////////////////////////////////////////////

}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------