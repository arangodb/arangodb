////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
#include "VocBase/LogicalCollection.h"
#include "VocBase/LogicalView.h"
#include "velocypack/Parser.h"

#include <memory>

namespace {
struct TestView : public arangodb::LogicalView {
  static constexpr auto typeInfo() noexcept {
    return std::pair{static_cast<arangodb::ViewType>(42),
                     std::string_view{"testViewType"}};
  }

  TestView(TRI_vocbase_t& vocbase,
           arangodb::velocypack::Slice const& definition)
      : arangodb::LogicalView(*this, vocbase, definition) {}
  virtual arangodb::Result appendVelocyPackImpl(arangodb::velocypack::Builder&,
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
  virtual arangodb::Result properties(arangodb::velocypack::Slice, bool,
                                      bool) override {
    return arangodb::Result();
  }
  virtual bool visitCollections(CollectionVisitor const&) const override {
    return true;
  }
};

struct ViewFactory : public arangodb::ViewFactory {
  virtual arangodb::Result create(arangodb::LogicalView::ptr& view,
                                  TRI_vocbase_t& vocbase,
                                  arangodb::velocypack::Slice definition,
                                  bool) const override {
    view = vocbase.createView(definition);

    return arangodb::Result();
  }

  virtual arangodb::Result instantiate(
      arangodb::LogicalView::ptr& view, TRI_vocbase_t& vocbase,
      arangodb::velocypack::Slice definition) const override {
    view = std::make_shared<TestView>(vocbase, definition);

    return arangodb::Result();
  }
};

}  // namespace

class VocbaseTest : public ::testing::Test {
 protected:
  arangodb::tests::mocks::MockAqlServer server;
  ViewFactory viewFactory;

  VocbaseTest() {
    // register view factory
    server.getFeature<arangodb::ViewTypesFeature>().emplace(
        TestView::typeInfo().second, viewFactory);
  }
};

TEST_F(VocbaseTest, test_lookupDataSource) {
  auto collectionJson = arangodb::velocypack::Parser::fromJson(
      "{ \"globallyUniqueId\": \"testCollectionGUID\", \"id\": 100, \"name\": "
      "\"testCollection\" }");
  auto viewJson = arangodb::velocypack::Parser::fromJson(
      "{ \"id\": 200, \"name\": \"testView\", \"type\": \"testViewType\" "
      "}");  // any arbitrary view type
  Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL,
                  testDBInfo(server.server()));

  // not present collection (no datasource)
  {
    EXPECT_FALSE(vocbase.lookupDataSource(arangodb::DataSourceId{100}));
    EXPECT_FALSE(vocbase.lookupDataSource("100"));
    EXPECT_FALSE(vocbase.lookupDataSource("testCollection"));
    EXPECT_FALSE(vocbase.lookupDataSource("testCollectionGUID"));
    EXPECT_FALSE(vocbase.lookupCollection(arangodb::DataSourceId{100}));
    EXPECT_FALSE(vocbase.lookupCollection("100"));
    EXPECT_FALSE(vocbase.lookupCollection("testCollection"));
    EXPECT_FALSE(vocbase.lookupCollection("testCollectionGUID"));
  }

  // not present view (no datasource)
  {
    EXPECT_FALSE(vocbase.lookupDataSource(arangodb::DataSourceId{200}));
    EXPECT_FALSE(vocbase.lookupDataSource("200"));
    EXPECT_FALSE(vocbase.lookupDataSource("testView"));
    EXPECT_FALSE(vocbase.lookupDataSource("testViewGUID"));
    EXPECT_FALSE(vocbase.lookupView(arangodb::DataSourceId{200}));
    EXPECT_FALSE(vocbase.lookupView("200"));
    EXPECT_FALSE(vocbase.lookupView("testView"));
    EXPECT_FALSE(vocbase.lookupView("testViewGUID"));
  }

  auto collection = vocbase.createCollection(collectionJson->slice());
  auto view = vocbase.createView(viewJson->slice());

  EXPECT_FALSE(collection->deleted());
  EXPECT_FALSE(view->deleted());

  // not present collection (is view)
  {
    EXPECT_FALSE(!vocbase.lookupDataSource(arangodb::DataSourceId{200}));
    EXPECT_FALSE(!vocbase.lookupDataSource("200"));
    EXPECT_FALSE(!vocbase.lookupDataSource("testView"));
    EXPECT_FALSE(vocbase.lookupDataSource("testViewGUID"));
    EXPECT_FALSE(vocbase.lookupCollection(arangodb::DataSourceId{200}));
    EXPECT_FALSE(vocbase.lookupCollection("200"));
    EXPECT_FALSE(vocbase.lookupCollection("testView"));
    EXPECT_FALSE(vocbase.lookupCollection("testViewGUID"));
    EXPECT_FALSE(vocbase.lookupCollectionByUuid("testView"));
    EXPECT_FALSE(vocbase.lookupCollectionByUuid("testViewGUID"));
  }

  // not preset view (is collection)
  {
    EXPECT_FALSE(!vocbase.lookupDataSource(arangodb::DataSourceId{100}));
    EXPECT_FALSE(!vocbase.lookupDataSource("100"));
    EXPECT_FALSE(!vocbase.lookupDataSource("testCollection"));
    EXPECT_FALSE(!vocbase.lookupDataSource("testCollectionGUID"));
    EXPECT_FALSE(vocbase.lookupView(arangodb::DataSourceId{100}));
    EXPECT_FALSE(vocbase.lookupView("100"));
    EXPECT_FALSE(vocbase.lookupView("testCollection"));
    EXPECT_FALSE(vocbase.lookupView("testCollectionGUID"));
  }

  // present collection
  {
    EXPECT_FALSE(!vocbase.lookupDataSource(arangodb::DataSourceId{100}));
    EXPECT_FALSE(!vocbase.lookupDataSource("100"));
    EXPECT_FALSE(!vocbase.lookupDataSource("testCollection"));
    EXPECT_FALSE(!vocbase.lookupDataSource("testCollectionGUID"));
    EXPECT_FALSE(!vocbase.lookupCollection(arangodb::DataSourceId{100}));
    EXPECT_FALSE(!vocbase.lookupCollection("100"));
    EXPECT_FALSE(!vocbase.lookupCollection("testCollection"));
    EXPECT_FALSE(!vocbase.lookupCollection("testCollectionGUID"));
    EXPECT_FALSE(vocbase.lookupCollectionByUuid("testCollection"));
    EXPECT_TRUE(
        (false == !vocbase.lookupCollectionByUuid("testCollectionGUID")));
  }

  // present view
  {
    EXPECT_FALSE(!vocbase.lookupDataSource(arangodb::DataSourceId{200}));
    EXPECT_FALSE(!vocbase.lookupDataSource("200"));
    EXPECT_FALSE(!vocbase.lookupDataSource("testView"));
    EXPECT_FALSE(vocbase.lookupDataSource("testViewGUID"));
    EXPECT_FALSE(!vocbase.lookupView(arangodb::DataSourceId{200}));
    EXPECT_FALSE(!vocbase.lookupView("200"));
    EXPECT_FALSE(!vocbase.lookupView("testView"));
    EXPECT_FALSE(vocbase.lookupView("testViewGUID"));
  }

  EXPECT_TRUE(vocbase.dropCollection(collection->id(), true, 0).ok());
  EXPECT_TRUE(view->drop().ok());
  EXPECT_TRUE(collection->deleted());
  EXPECT_TRUE(view->deleted());

  // not present collection (deleted)
  {
    EXPECT_FALSE(vocbase.lookupDataSource(arangodb::DataSourceId{100}));
    EXPECT_FALSE(vocbase.lookupDataSource("100"));
    EXPECT_FALSE(vocbase.lookupDataSource("testCollection"));
    EXPECT_FALSE(vocbase.lookupDataSource("testCollectionGUID"));
    EXPECT_FALSE(vocbase.lookupCollection(arangodb::DataSourceId{100}));
    EXPECT_FALSE(vocbase.lookupCollection("100"));
    EXPECT_FALSE(vocbase.lookupCollection("testCollection"));
    EXPECT_FALSE(vocbase.lookupCollection("testCollectionGUID"));
    EXPECT_FALSE(vocbase.lookupCollectionByUuid("testCollection"));
    EXPECT_TRUE(
        (true == !vocbase.lookupCollectionByUuid("testCollectionGUID")));
  }

  // not present view (deleted)
  {
    EXPECT_FALSE(vocbase.lookupDataSource(arangodb::DataSourceId{200}));
    EXPECT_FALSE(vocbase.lookupDataSource("200"));
    EXPECT_FALSE(vocbase.lookupDataSource("testView"));
    EXPECT_FALSE(vocbase.lookupDataSource("testViewGUID"));
    EXPECT_FALSE(vocbase.lookupView(arangodb::DataSourceId{200}));
    EXPECT_FALSE(vocbase.lookupView("200"));
    EXPECT_FALSE(vocbase.lookupView("testView"));
    EXPECT_FALSE(vocbase.lookupView("testViewGUID"));
    EXPECT_FALSE(vocbase.lookupCollectionByUuid("testCollection"));
    EXPECT_TRUE(
        (true == !vocbase.lookupCollectionByUuid("testCollectionGUID")));
  }
}
