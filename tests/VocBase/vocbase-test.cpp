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
#include "../Mocks/StorageEngineMock.h"
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
static const VPackBuilder systemDatabaseBuilder = dbArgsBuilder();
static const VPackSlice   systemDatabaseArgs = systemDatabaseBuilder.slice();
static const VPackBuilder testDatabaseBuilder = dbArgsBuilder("testVocbase");
static const VPackSlice   testDatabaseArgs = testDatabaseBuilder.slice();

struct TestView : public arangodb::LogicalView {
  TestView(TRI_vocbase_t& vocbase, arangodb::velocypack::Slice const& definition, uint64_t planVersion)
      : arangodb::LogicalView(vocbase, definition, planVersion) {}
  virtual arangodb::Result appendVelocyPackImpl(
      arangodb::velocypack::Builder&,
      std::underlying_type<arangodb::LogicalDataSource::Serialize>::type) const override {
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

  virtual arangodb::Result instantiate(arangodb::LogicalView::ptr& view,
                                       TRI_vocbase_t& vocbase,
                                       arangodb::velocypack::Slice const& definition,
                                       uint64_t planVersion) const override {
    view = std::make_shared<TestView>(vocbase, definition, planVersion);

    return arangodb::Result();
  }
};

}  // namespace

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

class VocbaseTest : public ::testing::Test {
 protected:
  StorageEngineMock engine;
  arangodb::application_features::ApplicationServer server;
  std::vector<std::pair<arangodb::application_features::ApplicationFeature*, bool>> features;
  ViewFactory viewFactory;

  VocbaseTest() : engine(server), server(nullptr, nullptr) {
    arangodb::EngineSelectorFeature::ENGINE = &engine;

    // setup required application features
    features.emplace_back(new arangodb::DatabaseFeature(server),
                          false);  // required for TRI_vocbase_t::dropCollection(...)
    features.emplace_back(new arangodb::QueryRegistryFeature(server), false);  // required for TRI_vocbase_t instantiation
    features.emplace_back(new arangodb::ViewTypesFeature(server),
                          false);  // required for TRI_vocbase_t::createView(...)
    features.emplace_back(new arangodb::ShardingFeature(server), false);

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

    // register view factory
    arangodb::application_features::ApplicationServer::lookupFeature<arangodb::ViewTypesFeature>()
        ->emplace(arangodb::LogicalDataSource::Type::emplace(
                      arangodb::velocypack::StringRef("testViewType")),
                  viewFactory);
  }

  ~VocbaseTest() {
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

TEST_F(VocbaseTest, test_isAllowedName) {
  // direct (non-system)
  {
    EXPECT_TRUE((false == TRI_vocbase_t::IsAllowedName(
                              false, arangodb::velocypack::StringRef(nullptr, 0))));
    EXPECT_TRUE((false == TRI_vocbase_t::IsAllowedName(false, arangodb::velocypack::StringRef(""))));
    EXPECT_TRUE((true == TRI_vocbase_t::IsAllowedName(false, arangodb::velocypack::StringRef(
                                                                 "abc123"))));
    EXPECT_TRUE((false == TRI_vocbase_t::IsAllowedName(false, arangodb::velocypack::StringRef(
                                                                  "123abc"))));
    EXPECT_TRUE((false == TRI_vocbase_t::IsAllowedName(false, arangodb::velocypack::StringRef("123"))));
    EXPECT_TRUE((false == TRI_vocbase_t::IsAllowedName(false, arangodb::velocypack::StringRef(
                                                                  "_123"))));
    EXPECT_TRUE((false == TRI_vocbase_t::IsAllowedName(false, arangodb::velocypack::StringRef(
                                                                  "_abc"))));
    EXPECT_TRUE(
        (false == TRI_vocbase_t::IsAllowedName(false, arangodb::velocypack::StringRef("abcdefghijklmnopqrstuvwxyz0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"))));  // longer than TRI_COL_NAME_LENGTH
  }

  // direct (system)
  {
    EXPECT_TRUE((false == TRI_vocbase_t::IsAllowedName(
                              true, arangodb::velocypack::StringRef(nullptr, 0))));
    EXPECT_TRUE((false == TRI_vocbase_t::IsAllowedName(true, arangodb::velocypack::StringRef(""))));
    EXPECT_TRUE((true == TRI_vocbase_t::IsAllowedName(true, arangodb::velocypack::StringRef(
                                                                "abc123"))));
    EXPECT_TRUE((false == TRI_vocbase_t::IsAllowedName(true, arangodb::velocypack::StringRef(
                                                                 "123abc"))));
    EXPECT_TRUE((false == TRI_vocbase_t::IsAllowedName(true, arangodb::velocypack::StringRef("123"))));
    EXPECT_TRUE((true == TRI_vocbase_t::IsAllowedName(true, arangodb::velocypack::StringRef("_123"))));
    EXPECT_TRUE((true == TRI_vocbase_t::IsAllowedName(true, arangodb::velocypack::StringRef("_abc"))));
    EXPECT_TRUE(
        (false == TRI_vocbase_t::IsAllowedName(true, arangodb::velocypack::StringRef("abcdefghijklmnopqrstuvwxyz0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"))));  // longer than TRI_COL_NAME_LENGTH
  }

  // slice (default)
  {
    auto json0 = arangodb::velocypack::Parser::fromJson("{ }");
    EXPECT_TRUE((false == TRI_vocbase_t::IsAllowedName(json0->slice())));
    auto json1 = arangodb::velocypack::Parser::fromJson("{ \"name\": \"\" }");
    EXPECT_TRUE((false == TRI_vocbase_t::IsAllowedName(json1->slice())));
    auto json2 =
        arangodb::velocypack::Parser::fromJson("{ \"name\": \"abc123\" }");
    EXPECT_TRUE((true == TRI_vocbase_t::IsAllowedName(json2->slice())));
    auto json3 =
        arangodb::velocypack::Parser::fromJson("{ \"name\": \"123abc\" }");
    EXPECT_TRUE((false == TRI_vocbase_t::IsAllowedName(json3->slice())));
    auto json4 =
        arangodb::velocypack::Parser::fromJson("{ \"name\": \"123\" }");
    EXPECT_TRUE((false == TRI_vocbase_t::IsAllowedName(json4->slice())));
    auto json5 =
        arangodb::velocypack::Parser::fromJson("{ \"name\": \"_123\" }");
    EXPECT_TRUE((false == TRI_vocbase_t::IsAllowedName(json5->slice())));
    auto json6 =
        arangodb::velocypack::Parser::fromJson("{ \"name\": \"_abc\" }");
    EXPECT_TRUE((false == TRI_vocbase_t::IsAllowedName(json6->slice())));
    auto json7 = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": "
        "\"abcdefghijklmnopqrstuvwxyz0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ012345"
        "6789\" }");
    EXPECT_TRUE((false == TRI_vocbase_t::IsAllowedName(json7->slice())));  // longer than TRI_COL_NAME_LENGTH
  }

  // slice (non-system)
  {
    auto json0 =
        arangodb::velocypack::Parser::fromJson("{ \"isSystem\": false }");
    EXPECT_TRUE((false == TRI_vocbase_t::IsAllowedName(json0->slice())));
    auto json1 = arangodb::velocypack::Parser::fromJson(
        "{ \"isSystem\": false, \"name\": \"\" }");
    EXPECT_TRUE((false == TRI_vocbase_t::IsAllowedName(json1->slice())));
    auto json2 = arangodb::velocypack::Parser::fromJson(
        "{ \"isSystem\": false, \"name\": \"abc123\" }");
    EXPECT_TRUE((true == TRI_vocbase_t::IsAllowedName(json2->slice())));
    auto json3 = arangodb::velocypack::Parser::fromJson(
        "{ \"isSystem\": false, \"name\": \"123abc\" }");
    EXPECT_TRUE((false == TRI_vocbase_t::IsAllowedName(json3->slice())));
    auto json4 = arangodb::velocypack::Parser::fromJson(
        "{ \"isSystem\": false, \"name\": \"123\" }");
    EXPECT_TRUE((false == TRI_vocbase_t::IsAllowedName(json4->slice())));
    auto json5 = arangodb::velocypack::Parser::fromJson(
        "{\"isSystem\": false, \"name\": \"_123\" }");
    EXPECT_TRUE((false == TRI_vocbase_t::IsAllowedName(json5->slice())));
    auto json6 = arangodb::velocypack::Parser::fromJson(
        "{ \"isSystem\": false, \"name\": \"_abc\" }");
    EXPECT_TRUE((false == TRI_vocbase_t::IsAllowedName(json6->slice())));
    auto json7 = arangodb::velocypack::Parser::fromJson(
        "{ \"isSystem\": false, \"name\": 123 }");
    EXPECT_TRUE((false == TRI_vocbase_t::IsAllowedName(json7->slice())));
    auto json8 = arangodb::velocypack::Parser::fromJson(
        "{ \"isSystem\": 123, \"name\": \"abc\" }");
    EXPECT_TRUE((true == TRI_vocbase_t::IsAllowedName(json8->slice())));
    auto json9 = arangodb::velocypack::Parser::fromJson(
        "{ \"isSystem\": false, \"name\": "
        "\"abcdefghijklmnopqrstuvwxyz0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ012345"
        "6789\" }");
    EXPECT_TRUE((false == TRI_vocbase_t::IsAllowedName(json9->slice())));  // longer than TRI_COL_NAME_LENGTH
  }

  // slice (system)
  {
    auto json0 =
        arangodb::velocypack::Parser::fromJson("{ \"isSystem\": true }");
    EXPECT_TRUE((false == TRI_vocbase_t::IsAllowedName(json0->slice())));
    auto json1 = arangodb::velocypack::Parser::fromJson(
        "{ \"isSystem\": true, \"name\": \"\" }");
    EXPECT_TRUE((false == TRI_vocbase_t::IsAllowedName(json1->slice())));
    auto json2 = arangodb::velocypack::Parser::fromJson(
        "{ \"isSystem\": true, \"name\": \"abc123\" }");
    EXPECT_TRUE((true == TRI_vocbase_t::IsAllowedName(json2->slice())));
    auto json3 = arangodb::velocypack::Parser::fromJson(
        "{ \"isSystem\": true, \"name\": \"123abc\" }");
    EXPECT_TRUE((false == TRI_vocbase_t::IsAllowedName(json3->slice())));
    auto json4 = arangodb::velocypack::Parser::fromJson(
        "{ \"isSystem\": true, \"name\": \"123\" }");
    EXPECT_TRUE((false == TRI_vocbase_t::IsAllowedName(json4->slice())));
    auto json5 = arangodb::velocypack::Parser::fromJson(
        "{ \"isSystem\": true, \"name\": \"_123\" }");
    EXPECT_TRUE((true == TRI_vocbase_t::IsAllowedName(json5->slice())));
    auto json6 = arangodb::velocypack::Parser::fromJson(
        "{ \"isSystem\": true, \"name\": \"_abc\" }");
    EXPECT_TRUE((true == TRI_vocbase_t::IsAllowedName(json6->slice())));
    auto json7 = arangodb::velocypack::Parser::fromJson(
        "{ \"isSystem\": true, \"name\": 123 }");
    EXPECT_TRUE((false == TRI_vocbase_t::IsAllowedName(json7->slice())));
    auto json8 = arangodb::velocypack::Parser::fromJson(
        "{ \"isSystem\": 123, \"name\": \"_abc\" }");
    EXPECT_TRUE((false == TRI_vocbase_t::IsAllowedName(json8->slice())));
    auto json9 = arangodb::velocypack::Parser::fromJson(
        "{ \"isSystem\": true, \"name\": "
        "\"abcdefghijklmnopqrstuvwxyz0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ012345"
        "6789\" }");
    EXPECT_TRUE((false == TRI_vocbase_t::IsAllowedName(json9->slice())));  // longer than TRI_COL_NAME_LENGTH
  }
}

TEST_F(VocbaseTest, test_isSystemName) {
  EXPECT_TRUE((false == TRI_vocbase_t::IsSystemName("")));
  EXPECT_TRUE((true == TRI_vocbase_t::IsSystemName("_")));
  EXPECT_TRUE((true == TRI_vocbase_t::IsSystemName("_abc")));
  EXPECT_TRUE((false == TRI_vocbase_t::IsSystemName("abc")));
}

TEST_F(VocbaseTest, test_lookupDataSource) {
  auto collectionJson = arangodb::velocypack::Parser::fromJson(
      "{ \"globallyUniqueId\": \"testCollectionGUID\", \"id\": 100, \"name\": "
      "\"testCollection\" }");
  auto viewJson = arangodb::velocypack::Parser::fromJson(
      "{ \"id\": 200, \"name\": \"testView\", \"type\": \"testViewType\" }");  // any arbitrary view type
  Vocbase vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, testDatabaseArgs);

  // not present collection (no datasource)
  {
    EXPECT_TRUE((true == !vocbase.lookupDataSource(100)));
    EXPECT_TRUE((true == !vocbase.lookupDataSource("100")));
    EXPECT_TRUE((true == !vocbase.lookupDataSource("testCollection")));
    EXPECT_TRUE((true == !vocbase.lookupDataSource("testCollectionGUID")));
    EXPECT_TRUE((true == !vocbase.lookupCollection(100)));
    EXPECT_TRUE((true == !vocbase.lookupCollection("100")));
    EXPECT_TRUE((true == !vocbase.lookupCollection("testCollection")));
    EXPECT_TRUE((true == !vocbase.lookupCollection("testCollectionGUID")));
  }

  // not present view (no datasource)
  {
    EXPECT_TRUE((true == !vocbase.lookupDataSource(200)));
    EXPECT_TRUE((true == !vocbase.lookupDataSource("200")));
    EXPECT_TRUE((true == !vocbase.lookupDataSource("testView")));
    EXPECT_TRUE((true == !vocbase.lookupDataSource("testViewGUID")));
    EXPECT_TRUE((true == !vocbase.lookupView(200)));
    EXPECT_TRUE((true == !vocbase.lookupView("200")));
    EXPECT_TRUE((true == !vocbase.lookupView("testView")));
    EXPECT_TRUE((true == !vocbase.lookupView("testViewGUID")));
  }

  auto collection = vocbase.createCollection(collectionJson->slice());
  auto view = vocbase.createView(viewJson->slice());

  EXPECT_TRUE((false == collection->deleted()));
  EXPECT_TRUE((false == view->deleted()));

  // not present collection (is view)
  {
    EXPECT_TRUE((false == !vocbase.lookupDataSource(200)));
    EXPECT_TRUE((false == !vocbase.lookupDataSource("200")));
    EXPECT_TRUE((false == !vocbase.lookupDataSource("testView")));
    EXPECT_TRUE((true == !vocbase.lookupDataSource("testViewGUID")));
    EXPECT_TRUE((true == !vocbase.lookupCollection(200)));
    EXPECT_TRUE((true == !vocbase.lookupCollection("200")));
    EXPECT_TRUE((true == !vocbase.lookupCollection("testView")));
    EXPECT_TRUE((true == !vocbase.lookupCollection("testViewGUID")));
    EXPECT_TRUE((true == !vocbase.lookupCollectionByUuid("testView")));
    EXPECT_TRUE((true == !vocbase.lookupCollectionByUuid("testViewGUID")));
  }

  // not preset view (is collection)
  {
    EXPECT_TRUE((false == !vocbase.lookupDataSource(100)));
    EXPECT_TRUE((false == !vocbase.lookupDataSource("100")));
    EXPECT_TRUE((false == !vocbase.lookupDataSource("testCollection")));
    EXPECT_TRUE((false == !vocbase.lookupDataSource("testCollectionGUID")));
    EXPECT_TRUE((true == !vocbase.lookupView(100)));
    EXPECT_TRUE((true == !vocbase.lookupView("100")));
    EXPECT_TRUE((true == !vocbase.lookupView("testCollection")));
    EXPECT_TRUE((true == !vocbase.lookupView("testCollectionGUID")));
  }

  // present collection
  {
    EXPECT_TRUE((false == !vocbase.lookupDataSource(100)));
    EXPECT_TRUE((false == !vocbase.lookupDataSource("100")));
    EXPECT_TRUE((false == !vocbase.lookupDataSource("testCollection")));
    EXPECT_TRUE((false == !vocbase.lookupDataSource("testCollectionGUID")));
    EXPECT_TRUE((false == !vocbase.lookupCollection(100)));
    EXPECT_TRUE((false == !vocbase.lookupCollection("100")));
    EXPECT_TRUE((false == !vocbase.lookupCollection("testCollection")));
    EXPECT_TRUE((false == !vocbase.lookupCollection("testCollectionGUID")));
    EXPECT_TRUE((true == !vocbase.lookupCollectionByUuid("testCollection")));
    EXPECT_TRUE(
        (false == !vocbase.lookupCollectionByUuid("testCollectionGUID")));
  }

  // present view
  {
    EXPECT_TRUE((false == !vocbase.lookupDataSource(200)));
    EXPECT_TRUE((false == !vocbase.lookupDataSource("200")));
    EXPECT_TRUE((false == !vocbase.lookupDataSource("testView")));
    EXPECT_TRUE((true == !vocbase.lookupDataSource("testViewGUID")));
    EXPECT_TRUE((false == !vocbase.lookupView(200)));
    EXPECT_TRUE((false == !vocbase.lookupView("200")));
    EXPECT_TRUE((false == !vocbase.lookupView("testView")));
    EXPECT_TRUE((true == !vocbase.lookupView("testViewGUID")));
  }

  EXPECT_TRUE((true == vocbase.dropCollection(collection->id(), true, 0).ok()));
  EXPECT_TRUE((true == view->drop().ok()));
  EXPECT_TRUE((true == collection->deleted()));
  EXPECT_TRUE((true == view->deleted()));

  // not present collection (deleted)
  {
    EXPECT_TRUE((true == !vocbase.lookupDataSource(100)));
    EXPECT_TRUE((true == !vocbase.lookupDataSource("100")));
    EXPECT_TRUE((true == !vocbase.lookupDataSource("testCollection")));
    EXPECT_TRUE((true == !vocbase.lookupDataSource("testCollectionGUID")));
    EXPECT_TRUE((true == !vocbase.lookupCollection(100)));
    EXPECT_TRUE((true == !vocbase.lookupCollection("100")));
    EXPECT_TRUE((true == !vocbase.lookupCollection("testCollection")));
    EXPECT_TRUE((true == !vocbase.lookupCollection("testCollectionGUID")));
    EXPECT_TRUE((true == !vocbase.lookupCollectionByUuid("testCollection")));
    EXPECT_TRUE(
        (true == !vocbase.lookupCollectionByUuid("testCollectionGUID")));
  }

  // not present view (deleted)
  {
    EXPECT_TRUE((true == !vocbase.lookupDataSource(200)));
    EXPECT_TRUE((true == !vocbase.lookupDataSource("200")));
    EXPECT_TRUE((true == !vocbase.lookupDataSource("testView")));
    EXPECT_TRUE((true == !vocbase.lookupDataSource("testViewGUID")));
    EXPECT_TRUE((true == !vocbase.lookupView(200)));
    EXPECT_TRUE((true == !vocbase.lookupView("200")));
    EXPECT_TRUE((true == !vocbase.lookupView("testView")));
    EXPECT_TRUE((true == !vocbase.lookupView("testViewGUID")));
    EXPECT_TRUE((true == !vocbase.lookupCollectionByUuid("testCollection")));
    EXPECT_TRUE(
        (true == !vocbase.lookupCollectionByUuid("testCollectionGUID")));
  }
}
