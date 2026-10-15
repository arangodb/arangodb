////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
////////////////////////////////////////////////////////////////////////////////

#include "VocBase/Identifiers/DataSourceId.h"
#include "gtest/gtest.h"

#include "../IResearch/common.h"
#include "Mocks/ExecContextFactory.h"
#include "Mocks/Servers.h"

#include "Auth/Common.h"
#include "Basics/voc-errors.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "RestServer/ViewTypesFeature.h"
#include "Sharding/ShardingFeature.h"
#include "Utils/ExecContext.h"
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
      : arangodb::LogicalView(*this, vocbase, definition, false) {}
  arangodb::Result appendVPackImpl(arangodb::velocypack::Builder&,
                                   Serialization, bool) const override {
    return {};
  }
  virtual arangodb::Result dropImpl() override {
    return arangodb::storage_helper::drop(*this);
  }
  virtual void open() override {}
  virtual arangodb::Result renameImpl(std::string const& oldName) override {
    return arangodb::storage_helper::rename(*this, oldName);
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
                                  bool isUserRequest) const override {
    view = vocbase.createView(definition, isUserRequest);

    return arangodb::Result();
  }

  virtual arangodb::Result instantiate(arangodb::LogicalView::ptr& view,
                                       TRI_vocbase_t& vocbase,
                                       arangodb::velocypack::Slice definition,
                                       bool /*isUserRequest*/) const override {
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
  Vocbase vocbase(testDBInfo(server.server()), server.engine());

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
  auto view = vocbase.createView(viewJson->slice(), false);

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

  EXPECT_TRUE(vocbase.dropCollection(collection->id(), true).ok());
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

TEST_F(VocbaseTest, test_lookupCollection_requiresSeePermission) {
  auto collectionJson = arangodb::velocypack::Parser::fromJson(
      "{ \"globallyUniqueId\": \"testCollectionGUID\", \"id\": 100, \"name\": "
      "\"testCollection\" }");
  Vocbase vocbase(testDBInfo(server.server()), server.engine());

  // The collection itself is created as the (default) superuser context.
  auto collection = vocbase.createCollection(collectionJson->slice());
  ASSERT_NE(collection, nullptr);
  auto const cid = collection->id();

  // API version 2: a user who may not see the collection must NOT be able to
  // look it up by id, name or UUID.
  {
    auto ctx = arangodb::tests::mocks::makeClassicExecContext(
        "restricted", arangodb::auth::Level::NONE,
        {"testVocbase", arangodb::auth::Level::NONE},
        {{"testCollection", arangodb::auth::Level::NONE}},
        /*isRestApiHardened*/ false,
        /*apiVersion*/ 2);
    arangodb::ExecContextScope scope(ctx.execContext);

    EXPECT_EQ(vocbase.lookupCollection(cid).errorNumber(), TRI_ERROR_FORBIDDEN);
    EXPECT_EQ(vocbase.lookupCollection("100").errorNumber(),
              TRI_ERROR_FORBIDDEN);
    EXPECT_EQ(vocbase.lookupCollection("testCollection").errorNumber(),
              TRI_ERROR_FORBIDDEN);
    EXPECT_EQ(vocbase.lookupCollection("testCollectionGUID").errorNumber(),
              TRI_ERROR_FORBIDDEN);
    EXPECT_EQ(
        vocbase.lookupCollectionByUuid("testCollectionGUID").errorNumber(),
        TRI_ERROR_FORBIDDEN);

    // Existence must not leak: a hidden collection and a missing one give an
    // identical FORBIDDEN (code and message), by identifier as well as by name.
    auto const hidden = vocbase.lookupCollection(cid);
    auto const missingById =
        vocbase.lookupCollection(arangodb::DataSourceId{1});
    auto const missingByName = vocbase.lookupCollection("anotherCollection");
    EXPECT_EQ(hidden.errorNumber(), missingById.errorNumber());
    EXPECT_EQ(hidden.errorMessage(), missingById.errorMessage());
    EXPECT_EQ(hidden.errorNumber(), missingByName.errorNumber());
    EXPECT_EQ(hidden.errorMessage(), missingByName.errorMessage());

    // The check belongs in lookupCollection, not in the shared lookupDataSource
    // (which also backs views/analyzers and is out of scope for the
    // collection-only change): the raw data-source lookup still resolves.
    EXPECT_NE(vocbase.lookupDataSource("testCollection"), nullptr);
  }

  // API version 2: a user with read access to exactly one collection, and no
  // other grant at all (note: _system access would act as a fallback grant
  // for every database), may see that collection and nothing else.
  {
    auto ctx = arangodb::tests::mocks::makeClassicExecContext(
        "reader", arangodb::auth::Level::NONE,
        {"testVocbase", arangodb::auth::Level::NONE},
        {{"testCollection", arangodb::auth::Level::RO}},
        /*isRestApiHardened*/ false,
        /*apiVersion*/ 2);
    arangodb::ExecContextScope scope(ctx.execContext);

    EXPECT_TRUE(vocbase.lookupCollection(cid).ok());
    EXPECT_TRUE(vocbase.lookupCollection("100").ok());
    EXPECT_TRUE(vocbase.lookupCollection("testCollection").ok());
    EXPECT_TRUE(vocbase.lookupCollection("testCollectionGUID").ok());
    EXPECT_TRUE(vocbase.lookupCollectionByUuid("testCollectionGUID").ok());

    // Other collections: this user is granted for no other name, and a missing
    // collection requested by identifier is never granted - so every lookup is
    // FORBIDDEN, whether or not such a collection exists.
    EXPECT_EQ(vocbase.lookupCollection(arangodb::DataSourceId{1}).errorNumber(),
              TRI_ERROR_FORBIDDEN);
    EXPECT_EQ(vocbase.lookupCollection("324").errorNumber(),
              TRI_ERROR_FORBIDDEN);
    EXPECT_EQ(vocbase.lookupCollection("anotherCollection").errorNumber(),
              TRI_ERROR_FORBIDDEN);
    EXPECT_EQ(vocbase.lookupCollection("anotherCollectionGUID").errorNumber(),
              TRI_ERROR_FORBIDDEN);
    EXPECT_EQ(
        vocbase.lookupCollectionByUuid("anotherCollectionGUID").errorNumber(),
        TRI_ERROR_FORBIDDEN);
  }

  // API version 2: a user with read access to the whole database is granted
  // for any name, so a missing collection requested by NAME is NOT_FOUND. By
  // id or uuid there is no name to check, so it stays FORBIDDEN: existence
  // cannot be probed by identifier.
  {
    auto ctx = arangodb::tests::mocks::makeClassicExecContext(
        "dbReader", "testVocbase", arangodb::auth::Level::NONE,
        arangodb::auth::Level::RO, /*isRestApiHardened*/ false,
        /*apiVersion*/ 2);
    arangodb::ExecContextScope scope(ctx.execContext);

    EXPECT_TRUE(vocbase.lookupCollection("testCollection").ok());
    EXPECT_EQ(vocbase.lookupCollection("anotherCollection").errorNumber(),
              TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
    EXPECT_EQ(vocbase.lookupCollection(arangodb::DataSourceId{1}).errorNumber(),
              TRI_ERROR_FORBIDDEN);
    EXPECT_EQ(vocbase.lookupCollection("324").errorNumber(),
              TRI_ERROR_FORBIDDEN);
    EXPECT_EQ(
        vocbase.lookupCollectionByUuid("anotherCollectionGUID").errorNumber(),
        TRI_ERROR_FORBIDDEN);
  }

  // API versions 0 and 1 keep the legacy behaviour: the visibility check is
  // not enforced, so even a user without access resolves the collection.
  for (uint32_t const apiVersion : {0u, 1u}) {
    auto ctx = arangodb::tests::mocks::makeClassicExecContext(
        "reader", arangodb::auth::Level::RO,
        {"testVocbase", arangodb::auth::Level::NONE},
        {{"testCollection", arangodb::auth::Level::NONE}},
        /*isRestApiHardened*/ false, apiVersion);
    arangodb::ExecContextScope scope(ctx.execContext);

    EXPECT_TRUE(vocbase.lookupCollection(cid).ok());
    EXPECT_TRUE(vocbase.lookupCollection("testCollection").ok());
    EXPECT_TRUE(vocbase.lookupCollectionByUuid("testCollectionGUID").ok());
  }

  EXPECT_TRUE(vocbase.dropCollection(cid, true).ok());
}

TEST_F(VocbaseTest, test_lookupCollection_systemCollection) {
  auto systemCollectionJson = arangodb::velocypack::Parser::fromJson(
      "{ \"globallyUniqueId\": \"testSystemGUID\", \"id\": 101, \"name\": "
      "\"_testSystem\", \"isSystem\": true }");
  Vocbase vocbase(testDBInfo(server.server()), server.engine());

  auto collection = vocbase.createCollection(systemCollectionJson->slice());
  ASSERT_NE(collection, nullptr);
  auto const cid = collection->id();

  // Internal / superuser context: the system collection must always resolve,
  // otherwise internal operations that touch it would break.
  {
    arangodb::ExecContextSuperuserScope superuserScope;

    EXPECT_TRUE(vocbase.lookupCollection(cid).ok());
    EXPECT_TRUE(vocbase.lookupCollection("_testSystem").ok());
    EXPECT_TRUE(vocbase.lookupCollection("testSystemGUID").ok());
    EXPECT_TRUE(vocbase.lookupCollectionByUuid("testSystemGUID").ok());
  }

  // API version 2: a regular user without any access must not have the system
  // collection leaked either, so every lookup returns nullptr.
  {
    auto ctx = arangodb::tests::mocks::makeClassicExecContext(
        "restricted", arangodb::auth::Level::NONE,
        {"testVocbase", arangodb::auth::Level::NONE}, {},
        /*isRestApiHardened*/ false,
        /*apiVersion*/ 2);
    arangodb::ExecContextScope scope(ctx.execContext);

    EXPECT_EQ(vocbase.lookupCollection(cid).errorNumber(), TRI_ERROR_FORBIDDEN);
    EXPECT_EQ(vocbase.lookupCollection("_testSystem").errorNumber(),
              TRI_ERROR_FORBIDDEN);
    EXPECT_EQ(vocbase.lookupCollection("testSystemGUID").errorNumber(),
              TRI_ERROR_FORBIDDEN);
    EXPECT_EQ(vocbase.lookupCollectionByUuid("testSystemGUID").errorNumber(),
              TRI_ERROR_FORBIDDEN);
  }

  EXPECT_TRUE(vocbase.dropCollection(cid, true).ok());
}
