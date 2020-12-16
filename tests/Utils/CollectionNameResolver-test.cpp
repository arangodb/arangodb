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

#include "gtest/gtest.h"

#include "../IResearch/common.h"
#include "Mocks/Servers.h"

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
struct TestView : public arangodb::LogicalView {
  TestView(TRI_vocbase_t& vocbase, arangodb::velocypack::Slice const& definition)
      : arangodb::LogicalView(vocbase, definition, 0) {}
  virtual arangodb::Result appendVelocyPackImpl(
      arangodb::velocypack::Builder&,
      Serialization) const override {
    return arangodb::Result();
  }
  virtual arangodb::Result dropImpl() override {
    return arangodb::LogicalViewHelperStorageEngine::drop(*this);
  }
  virtual void open() override {}
  virtual arangodb::Result renameImpl(std::string const& oldName) override {
    return arangodb::LogicalViewHelperStorageEngine::rename(*this, oldName);
  }
  virtual arangodb::Result properties(arangodb::velocypack::Slice const&, bool) override {
    return arangodb::Result();
  }
  virtual bool visitCollections(CollectionVisitor const& visitor) const override {
    return true;
  }
};

struct ViewFactory : public arangodb::ViewFactory {
  virtual arangodb::Result create(arangodb::LogicalView::ptr& view, TRI_vocbase_t& vocbase,
                                  arangodb::velocypack::Slice const& definition) const override {
    view = vocbase.createView(definition);

    return arangodb::Result();
  }

  virtual arangodb::Result instantiate(
      arangodb::LogicalView::ptr& view,
      TRI_vocbase_t& vocbase,
      arangodb::velocypack::Slice const& definition) const override {
    view = std::make_shared<TestView>(vocbase, definition);

    return arangodb::Result();
  }
};

}  // namespace

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

class CollectionNameResolverTest : public ::testing::Test {
 protected:
  arangodb::tests::mocks::MockAqlServer server;
  ViewFactory viewFactory;

  CollectionNameResolverTest() {
    // register view factory
    server.getFeature<arangodb::ViewTypesFeature>().emplace(
        arangodb::LogicalDataSource::Type::emplace(arangodb::velocypack::StringRef("testViewType")),
        viewFactory);
  }
};

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

TEST_F(CollectionNameResolverTest, test_getDataSource) {
  auto collectionJson = arangodb::velocypack::Parser::fromJson(
      "{ \"globallyUniqueId\": \"testCollectionGUID\", \"id\": 100, \"name\": "
      "\"testCollection\" }");
  auto viewJson = arangodb::velocypack::Parser::fromJson(
      "{ \"id\": 200, \"name\": \"testView\", \"type\": \"testViewType\" }");  // any arbitrary view type
  Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
  arangodb::CollectionNameResolver resolver(vocbase);

  // not present collection (no datasource)
  {
    EXPECT_FALSE(resolver.getDataSource(100));
    EXPECT_FALSE(resolver.getDataSource("100"));
    EXPECT_FALSE(resolver.getDataSource("testCollection"));
    EXPECT_FALSE(resolver.getDataSource("testCollectionGUID"));
    EXPECT_FALSE(resolver.getCollection(100));
    EXPECT_FALSE(resolver.getCollection("100"));
    EXPECT_FALSE(resolver.getCollection("testCollection"));
    EXPECT_FALSE(resolver.getCollection("testCollectionGUID"));
  }

  // not present view (no datasource)
  {
    EXPECT_FALSE(resolver.getDataSource(200));
    EXPECT_FALSE(resolver.getDataSource("200"));
    EXPECT_FALSE(resolver.getDataSource("testView"));
    EXPECT_FALSE(resolver.getDataSource("testViewGUID"));
    EXPECT_FALSE(resolver.getView(200));
    EXPECT_FALSE(resolver.getView("200"));
    EXPECT_FALSE(resolver.getView("testView"));
    EXPECT_FALSE(resolver.getView("testViewGUID"));
  }

  auto collection = vocbase.createCollection(collectionJson->slice());
  auto view = vocbase.createView(viewJson->slice());

  EXPECT_FALSE(collection->deleted());
  EXPECT_FALSE(view->deleted());

  // not present collection (is view)
  {
    EXPECT_FALSE(!resolver.getDataSource(200));
    EXPECT_FALSE(!resolver.getDataSource("200"));
    EXPECT_FALSE(!resolver.getDataSource("testView"));
    EXPECT_FALSE(resolver.getDataSource("testViewGUID"));
    EXPECT_FALSE(resolver.getCollection(200));
    EXPECT_FALSE(resolver.getCollection("200"));
    EXPECT_FALSE(resolver.getCollection("testView"));
    EXPECT_FALSE(resolver.getCollection("testViewGUID"));
  }

  // not preset view (is collection)
  {
    EXPECT_FALSE(!resolver.getDataSource(100));
    EXPECT_FALSE(!resolver.getDataSource("100"));
    EXPECT_FALSE(!resolver.getDataSource("testCollection"));
    EXPECT_FALSE(!resolver.getDataSource("testCollectionGUID"));
    EXPECT_FALSE(resolver.getView(100));
    EXPECT_FALSE(resolver.getView("100"));
    EXPECT_FALSE(resolver.getView("testCollection"));
    EXPECT_FALSE(resolver.getView("testCollectionGUID"));
  }

  // present collection
  {
    EXPECT_FALSE(!resolver.getDataSource(100));
    EXPECT_FALSE(!resolver.getDataSource("100"));
    EXPECT_FALSE(!resolver.getDataSource("testCollection"));
    EXPECT_FALSE(!resolver.getDataSource("testCollectionGUID"));
    EXPECT_FALSE(!resolver.getCollection(100));
    EXPECT_FALSE(!resolver.getCollection("100"));
    EXPECT_FALSE(!resolver.getCollection("testCollection"));
    EXPECT_FALSE(!resolver.getCollection("testCollectionGUID"));
  }

  // present view
  {
    EXPECT_FALSE(!resolver.getDataSource(200));
    EXPECT_FALSE(!resolver.getDataSource("200"));
    EXPECT_FALSE(!resolver.getDataSource("testView"));
    EXPECT_FALSE(resolver.getDataSource("testViewGUID"));
    EXPECT_FALSE(!resolver.getView(200));
    EXPECT_FALSE(!resolver.getView("200"));
    EXPECT_FALSE(!resolver.getView("testView"));
    EXPECT_FALSE(resolver.getView("testViewGUID"));
  }

  EXPECT_TRUE(vocbase.dropCollection(collection->id(), true, 0).ok());
  EXPECT_TRUE(view->drop().ok());
  EXPECT_TRUE(collection->deleted());
  EXPECT_TRUE(view->deleted());

  // present collection (deleted, cached)
  {
    EXPECT_FALSE(!resolver.getDataSource(100));
    EXPECT_FALSE(!resolver.getDataSource("100"));
    EXPECT_FALSE(!resolver.getDataSource("testCollection"));
    EXPECT_FALSE(!resolver.getDataSource("testCollectionGUID"));
    EXPECT_FALSE(!resolver.getCollection(100));
    EXPECT_FALSE(!resolver.getCollection("100"));
    EXPECT_FALSE(!resolver.getCollection("testCollection"));
    EXPECT_FALSE(!resolver.getCollection("testCollectionGUID"));
    EXPECT_TRUE(resolver.getCollection(100)->deleted());
  }

  // present view (deleted, cached)
  {
    EXPECT_FALSE(!resolver.getDataSource(200));
    EXPECT_FALSE(!resolver.getDataSource("200"));
    EXPECT_FALSE(!resolver.getDataSource("testView"));
    EXPECT_FALSE(resolver.getDataSource("testViewGUID"));
    EXPECT_FALSE(!resolver.getView(200));
    EXPECT_FALSE(!resolver.getView("200"));
    EXPECT_FALSE(!resolver.getView("testView"));
    EXPECT_FALSE(resolver.getView("testViewGUID"));
    EXPECT_TRUE(resolver.getView(200)->deleted());
  }
}
