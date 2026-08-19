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

#include "gtest/gtest.h"

#include "velocypack/Parser.h"

#include "IResearch/RestHandlerMock.h"
#include "IResearch/common.h"
#include "Mocks/ExecContextFactory.h"
#include "Mocks/LogLevels.h"
#include "Mocks/Servers.h"
#include "Mocks/StorageEngineMock.h"

#include "Aql/QueryRegistry.h"
#include "Mocks/Auth/UserManagerTester.h"
#include "Basics/StaticStrings.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "RestHandler/RestUsersHandler.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/SystemDatabaseFeature.h"
#include "RestServer/ViewTypesFeature.h"
#include "Utils/ExecContext.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/LogicalView.h"
#include "VocBase/vocbase.h"

namespace {

struct TestView : arangodb::LogicalView {
  static constexpr auto typeInfo() noexcept {
    return std::pair{static_cast<arangodb::ViewType>(42),
                     std::string_view{"testViewType"}};
  }

  arangodb::Result _appendVelocyPackResult;
  arangodb::velocypack::Builder _properties;

  TestView(TRI_vocbase_t& vocbase,
           arangodb::velocypack::Slice const& definition)
      : arangodb::LogicalView(*this, vocbase, definition, false) {}
  arangodb::Result appendVPackImpl(arangodb::velocypack::Builder& build,
                                   Serialization, bool) const override {
    build.add("properties", _properties.slice());
    return _appendVelocyPackResult;
  }
  arangodb::Result dropImpl() override { return arangodb::Result(); }
  void open() override {}
  arangodb::Result renameImpl(std::string const&) override {
    return arangodb::Result();
  }
  arangodb::Result properties(arangodb::velocypack::Slice properties,
                              bool isUserRequest,
                              bool /*partialUpdate*/) override {
    EXPECT_TRUE(isUserRequest);
    _properties = arangodb::velocypack::Builder(properties);
    return arangodb::Result();
  }
  bool visitCollections(CollectionVisitor const&) const override {
    return true;
  }
};

struct ViewFactory : arangodb::ViewFactory {
  arangodb::Result create(arangodb::LogicalView::ptr& view,
                          TRI_vocbase_t& vocbase,
                          arangodb::velocypack::Slice definition,
                          bool isUserRequest) const override {
    EXPECT_TRUE(isUserRequest);
    view = vocbase.createView(definition, isUserRequest);

    return arangodb::Result();
  }

  arangodb::Result instantiate(arangodb::LogicalView::ptr& view,
                               TRI_vocbase_t& vocbase,
                               arangodb::velocypack::Slice definition,
                               bool /*isUserRequest*/) const override {
    view = std::make_shared<TestView>(vocbase, definition);

    return arangodb::Result();
  }
};

}  // namespace

class RestUsersHandlerTest
    : public ::testing::Test,
      public arangodb::tests::LogSuppressor<arangodb::Logger::AUTHENTICATION,
                                            arangodb::LogLevel::ERR> {
 protected:
  arangodb::tests::mocks::MockAqlServer server;
  arangodb::SystemDatabaseFeature::ptr system;
  ViewFactory viewFactory;

  RestUsersHandlerTest()
      : server(),
        system(server.getFeature<arangodb::SystemDatabaseFeature>().use()) {
    auto& viewTypesFeature = server.getFeature<arangodb::ViewTypesFeature>();
    viewTypesFeature.emplace(TestView::typeInfo().second, viewFactory);
  }
};

TEST_F(RestUsersHandlerTest, test_collection_auth) {
  auto* authFeature = arangodb::AuthenticationFeature::instance();
  auto* userManager = static_cast<arangodb::auth::UserManagerTester*>(
      authFeature->userManager());

  static const std::string userName("testUser");
  auto& databaseFeature = server.getFeature<arangodb::DatabaseFeature>();
  TRI_vocbase_t* vocbase;  // will be owned by DatabaseFeature
  ASSERT_TRUE(
      databaseFeature.createDatabase(testDBInfo(server.server()), vocbase)
          .ok());
  auto grantRequestPtr = std::make_unique<GeneralRequestMock>(*vocbase);
  auto& grantRequest = *grantRequestPtr;
  auto grantResponcePtr = std::make_unique<GeneralResponseMock>();
  auto& grantResponce = *grantResponcePtr;
  auto grantWildcardRequestPtr = std::make_unique<GeneralRequestMock>(*vocbase);
  auto& grantWildcardRequest = *grantWildcardRequestPtr;
  auto grantWildcardResponcePtr = std::make_unique<GeneralResponseMock>();
  auto& grantWildcardResponce = *grantWildcardResponcePtr;
  auto revokeRequestPtr = std::make_unique<GeneralRequestMock>(*vocbase);
  auto& revokeRequest = *revokeRequestPtr;
  auto revokeResponcePtr = std::make_unique<GeneralResponseMock>();
  auto& revokeResponce = *revokeResponcePtr;
  auto revokeWildcardRequestPtr =
      std::make_unique<GeneralRequestMock>(*vocbase);
  auto& revokeWildcardRequest = *revokeWildcardRequestPtr;
  auto revokeWildcardResponcePtr = std::make_unique<GeneralResponseMock>();
  auto& revokeWildcardResponce = *revokeWildcardResponcePtr;
  arangodb::RestUsersHandler grantHandler(
      server.server(), grantRequestPtr.release(), grantResponcePtr.release());
  arangodb::RestUsersHandler grantWildcardHandler(
      server.server(), grantWildcardRequestPtr.release(),
      grantWildcardResponcePtr.release());
  arangodb::RestUsersHandler revokeHandler(
      server.server(), revokeRequestPtr.release(), revokeResponcePtr.release());
  arangodb::RestUsersHandler revokeWildcardHandler(
      server.server(), revokeWildcardRequestPtr.release(),
      revokeWildcardResponcePtr.release());

  grantRequest.addSuffix("testUser");
  grantRequest.addSuffix("database");
  grantRequest.addSuffix(vocbase->name());
  grantRequest.addSuffix("testDataSource");
  grantRequest.setRequestType(arangodb::rest::RequestType::PUT);
  grantRequest._payload.openObject();
  grantRequest._payload.add(
      "grant", arangodb::velocypack::Value(arangodb::auth::convertFromAuthLevel(
                   arangodb::auth::Level::RW)));
  grantRequest._payload.close();

  grantWildcardRequest.addSuffix("testUser");
  grantWildcardRequest.addSuffix("database");
  grantWildcardRequest.addSuffix(vocbase->name());
  grantWildcardRequest.addSuffix("*");
  grantWildcardRequest.setRequestType(arangodb::rest::RequestType::PUT);
  grantWildcardRequest._payload.openObject();
  grantWildcardRequest._payload.add(
      "grant", arangodb::velocypack::Value(arangodb::auth::convertFromAuthLevel(
                   arangodb::auth::Level::RW)));
  grantWildcardRequest._payload.close();

  revokeRequest.addSuffix("testUser");
  revokeRequest.addSuffix("database");
  revokeRequest.addSuffix(vocbase->name());
  revokeRequest.addSuffix("testDataSource");
  revokeRequest.setRequestType(arangodb::rest::RequestType::DELETE_REQ);

  revokeWildcardRequest.addSuffix("testUser");
  revokeWildcardRequest.addSuffix("database");
  revokeWildcardRequest.addSuffix(vocbase->name());
  revokeWildcardRequest.addSuffix("*");
  revokeWildcardRequest.setRequestType(arangodb::rest::RequestType::DELETE_REQ);

  auto testUserExists = [&]() { return userManager->userExists(userName); };
  auto giveUserAdmin = [&]() {
    userManager->updateUser(
        userName,
        [](arangodb::auth::User& user) -> arangodb::Result {
          user.grantDatabase("_system", arangodb::auth::Level::RW);
          return {};
        },
        arangodb::auth::UserManager::RetryOnConflict::No);
  };
  auto takeUserAdmin = [&]() {
    userManager->updateUser(
        userName,
        [](arangodb::auth::User& user) -> arangodb::Result {
          user.removeDatabase("_system");
          return {};
        },
        arangodb::auth::UserManager::RetryOnConflict::No);
  };

  auto execCtxBundle = arangodb::tests::mocks::makeClassicExecContextFrom(
      *userManager, userName);
  auto execContext = execCtxBundle.execContext;
  arangodb::ExecContextScope execContextScope(execContext);

  // test auth missing (grant)
  {
    userManager->removeAllUsers();
    userManager->storeUser(false, userName, arangodb::StaticStrings::Empty,
                           true, arangodb::velocypack::Slice());
    ASSERT_TRUE(testUserExists());

    EXPECT_TRUE(execContext
                    ->canUseCollection(vocbase->name(), "testDataSource",
                                       arangodb::CollectionAccessLevel::Read)
                    .fail());
    giveUserAdmin();
    auto status = grantHandler.execute();
    takeUserAdmin();
    EXPECT_EQ(arangodb::RestStatus::DONE, status);
    EXPECT_EQ(arangodb::rest::ResponseCode::NOT_FOUND,
              grantResponce.responseCode());
    auto slice = grantResponce._payload.slice();
    EXPECT_TRUE(slice.isObject());
    EXPECT_TRUE(
        (slice.hasKey(arangodb::StaticStrings::Code) &&
         slice.get(arangodb::StaticStrings::Code).isNumber<size_t>() &&
         size_t(arangodb::rest::ResponseCode::NOT_FOUND) ==
             slice.get(arangodb::StaticStrings::Code).getNumber<size_t>()));
    EXPECT_TRUE(
        (slice.hasKey(arangodb::StaticStrings::Error) &&
         slice.get(arangodb::StaticStrings::Error).isBoolean() &&
         true == slice.get(arangodb::StaticStrings::Error).getBoolean()));
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::ErrorNum) &&
                 slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() &&
                 TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND ==
                     ErrorCode{slice.get(arangodb::StaticStrings::ErrorNum)
                                   .getNumber<int>()}));
    EXPECT_TRUE(execContext
                    ->canUseCollection(vocbase->name(), "testDataSource",
                                       arangodb::CollectionAccessLevel::Read)
                    .fail());
  }

  // test auth missing (revoke)
  {
    userManager->removeAllUsers();
    userManager->storeUser(false, userName, arangodb::StaticStrings::Empty,
                           true, arangodb::velocypack::Slice());
    auto res = userManager->updateUser(
        userName,
        [&vocbase](arangodb::auth::User& user) -> arangodb::Result {
          user.grantCollection(vocbase->name(), "testDataSource",
                               arangodb::auth::Level::RO);
          // for missing collections
          // User::collectionAuthLevel(...)
          // returns database auth::Level
          return {};
        },
        arangodb::auth::UserManager::RetryOnConflict::No);
    ASSERT_TRUE(res.ok()) << res.errorMessage();

    EXPECT_TRUE(execContext
                    ->canUseCollection(vocbase->name(), "testDataSource",
                                       arangodb::CollectionAccessLevel::Read)
                    .ok());
    EXPECT_TRUE(
        execContext
            ->canUseCollection(vocbase->name(), "testDataSource",
                               arangodb::CollectionAccessLevel::WriteData)
            .fail());
    giveUserAdmin();
    auto status = revokeHandler.execute();
    takeUserAdmin();
    EXPECT_EQ(arangodb::RestStatus::DONE, status);
    EXPECT_EQ(arangodb::rest::ResponseCode::NOT_FOUND,
              revokeResponce.responseCode());
    auto slice = revokeResponce._payload.slice();
    EXPECT_TRUE(slice.isObject());
    EXPECT_TRUE(
        (slice.hasKey(arangodb::StaticStrings::Code) &&
         slice.get(arangodb::StaticStrings::Code).isNumber<size_t>() &&
         size_t(arangodb::rest::ResponseCode::NOT_FOUND) ==
             slice.get(arangodb::StaticStrings::Code).getNumber<size_t>()));
    EXPECT_TRUE(
        (slice.hasKey(arangodb::StaticStrings::Error) &&
         slice.get(arangodb::StaticStrings::Error).isBoolean() &&
         true == slice.get(arangodb::StaticStrings::Error).getBoolean()));
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::ErrorNum) &&
                 slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() &&
                 TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND ==
                     ErrorCode{slice.get(arangodb::StaticStrings::ErrorNum)
                                   .getNumber<int>()}));
    EXPECT_TRUE(execContext
                    ->canUseCollection(vocbase->name(), "testDataSource",
                                       arangodb::CollectionAccessLevel::Read)
                    .ok());  // not modified from above
    EXPECT_TRUE(
        execContext
            ->canUseCollection(vocbase->name(), "testDataSource",
                               arangodb::CollectionAccessLevel::WriteData)
            .fail());  // not modified from above
  }

  // test auth collection (grant)
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testDataSource\" }");
    userManager->removeAllUsers();
    userManager->storeUser(false, userName, arangodb::StaticStrings::Empty,
                           true, arangodb::velocypack::Slice());
    EXPECT_TRUE(testUserExists());

    auto logicalCollection = std::shared_ptr<arangodb::LogicalCollection>(
        vocbase->createCollection(collectionJson->slice()).get(),
        [vocbase](arangodb::LogicalCollection* ptr) -> void {
          vocbase->dropCollection(ptr->id(), false);
        });
    ASSERT_FALSE(!logicalCollection);

    EXPECT_TRUE(execContext
                    ->canUseCollection(vocbase->name(), "testDataSource",
                                       arangodb::CollectionAccessLevel::Read)
                    .fail());
    giveUserAdmin();
    auto status = grantHandler.execute();
    takeUserAdmin();
    EXPECT_EQ(arangodb::RestStatus::DONE, status);
    EXPECT_EQ(arangodb::rest::ResponseCode::OK, grantResponce.responseCode());
    auto slice = grantResponce._payload.slice();
    EXPECT_TRUE(slice.isObject());
    EXPECT_TRUE(
        (slice.hasKey(vocbase->name() + "/testDataSource") &&
         slice.get(vocbase->name() + "/testDataSource").isString() &&
         arangodb::auth::convertFromAuthLevel(arangodb::auth::Level::RW) ==
             slice.get(vocbase->name() + "/testDataSource").copyString()));
    EXPECT_TRUE(
        execContext
            ->canUseCollection(vocbase->name(), "testDataSource",
                               arangodb::CollectionAccessLevel::WriteData)
            .ok());
  }

  // test auth collection (revoke)
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testDataSource\" }");
    userManager->removeAllUsers();
    userManager->storeUser(false, userName, arangodb::StaticStrings::Empty,
                           true, arangodb::velocypack::Slice());
    auto res = userManager->updateUser(
        userName,
        [&vocbase](arangodb::auth::User& user) -> arangodb::Result {
          user.grantCollection(
              vocbase->name(), "testDataSource",
              arangodb::auth::Level::RO);  // for missing collections
                                           // User::collectionAuthLevel(...)
                                           // returns database auth::Level
          return {};
        },
        arangodb::auth::UserManager::RetryOnConflict::No);
    ASSERT_TRUE(res.ok()) << res.errorMessage();

    auto logicalCollection = std::shared_ptr<arangodb::LogicalCollection>(
        vocbase->createCollection(collectionJson->slice()).get(),
        [vocbase](arangodb::LogicalCollection* ptr) -> void {
          vocbase->dropCollection(ptr->id(), false);
        });
    ASSERT_FALSE(!logicalCollection);

    EXPECT_TRUE(execContext
                    ->canUseCollection(vocbase->name(), "testDataSource",
                                       arangodb::CollectionAccessLevel::Read)
                    .ok());
    EXPECT_TRUE(
        execContext
            ->canUseCollection(vocbase->name(), "testDataSource",
                               arangodb::CollectionAccessLevel::WriteData)
            .fail());
    giveUserAdmin();
    auto status = revokeHandler.execute();
    takeUserAdmin();
    EXPECT_EQ(arangodb::RestStatus::DONE, status);
    EXPECT_EQ(arangodb::rest::ResponseCode::ACCEPTED,
              revokeResponce.responseCode());
    auto slice = revokeResponce._payload.slice();
    EXPECT_TRUE(slice.isObject());
    EXPECT_TRUE(
        (slice.hasKey(arangodb::StaticStrings::Code) &&
         slice.get(arangodb::StaticStrings::Code).isNumber<size_t>() &&
         size_t(arangodb::rest::ResponseCode::ACCEPTED) ==
             slice.get(arangodb::StaticStrings::Code).getNumber<size_t>()));
    EXPECT_TRUE(
        (slice.hasKey(arangodb::StaticStrings::Error) &&
         slice.get(arangodb::StaticStrings::Error).isBoolean() &&
         false == slice.get(arangodb::StaticStrings::Error).getBoolean()));
    // Granting collection-level access sets the Database level to UNDEFINED
    // (see User::grantCollection) after the collection-level access is revoked,
    // the DB level stays UNDEFINED.
    EXPECT_TRUE(execContext
                    ->canUseCollection(vocbase->name(), "testDataSource",
                                       arangodb::CollectionAccessLevel::Read)
                    .fail());
  }

  // test auth view (grant)
  {
    auto viewJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testDataSource\", \"type\": \"testViewType\" }");
    userManager->removeAllUsers();
    userManager->storeUser(false, userName, arangodb::StaticStrings::Empty,
                           true, arangodb::velocypack::Slice());
    EXPECT_TRUE(testUserExists());

    auto logicalView = std::shared_ptr<arangodb::LogicalView>(
        vocbase->createView(viewJson->slice(), false).get(),
        [vocbase](arangodb::LogicalView* ptr) -> void {
          vocbase->dropView(ptr->id(), false);
        });
    ASSERT_FALSE(!logicalView);

    EXPECT_TRUE(execContext
                    ->canUseView(vocbase->name(), "testDataSource",
                                 arangodb::ViewAccessLevel::Read)
                    .fail());
    giveUserAdmin();
    auto status = grantHandler.execute();
    takeUserAdmin();
    EXPECT_EQ(arangodb::RestStatus::DONE, status);
    EXPECT_EQ(arangodb::rest::ResponseCode::NOT_FOUND,
              grantResponce.responseCode());
    auto slice = grantResponce._payload.slice();
    EXPECT_TRUE(slice.isObject());
    EXPECT_TRUE(
        (slice.hasKey(arangodb::StaticStrings::Code) &&
         slice.get(arangodb::StaticStrings::Code).isNumber<size_t>() &&
         size_t(arangodb::rest::ResponseCode::NOT_FOUND) ==
             slice.get(arangodb::StaticStrings::Code).getNumber<size_t>()));
    EXPECT_TRUE(
        (slice.hasKey(arangodb::StaticStrings::Error) &&
         slice.get(arangodb::StaticStrings::Error).isBoolean() &&
         true == slice.get(arangodb::StaticStrings::Error).getBoolean()));
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::ErrorNum) &&
                 slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() &&
                 TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND ==
                     ErrorCode{slice.get(arangodb::StaticStrings::ErrorNum)
                                   .getNumber<int>()}));
    EXPECT_TRUE(execContext
                    ->canUseView(vocbase->name(), "testDataSource",
                                 arangodb::ViewAccessLevel::Read)
                    .fail());
  }

  // test auth view (revoke)
  {
    auto viewJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testDataSource\", \"type\": \"testViewType\" }");
    userManager->removeAllUsers();
    userManager->storeUser(false, userName, arangodb::StaticStrings::Empty,
                           true, arangodb::velocypack::Slice());
    auto res = userManager->updateUser(
        userName,
        [&vocbase](arangodb::auth::User& user) -> arangodb::Result {
          user.grantCollection(
              vocbase->name(), "testDataSource",
              arangodb::auth::Level::RO);  // for missing collections
                                           // User::collectionAuthLevel(...)
                                           // returns database auth::Level
          return {};
        },
        arangodb::auth::UserManager::RetryOnConflict::No);
    ASSERT_TRUE(res.ok()) << res.errorMessage();

    auto logicalView = std::shared_ptr<arangodb::LogicalView>(
        vocbase->createView(viewJson->slice(), false).get(),
        [vocbase](arangodb::LogicalView* ptr) -> void {
          vocbase->dropView(ptr->id(), false);
        });
    ASSERT_FALSE(!logicalView);

    EXPECT_TRUE(execContext
                    ->canUseView(vocbase->name(), "testDataSource",
                                 arangodb::ViewAccessLevel::Read)
                    .fail());
    giveUserAdmin();
    auto status = revokeHandler.execute();
    takeUserAdmin();
    EXPECT_EQ(arangodb::RestStatus::DONE, status);
    EXPECT_EQ(arangodb::rest::ResponseCode::NOT_FOUND,
              revokeResponce.responseCode());
    auto slice = revokeResponce._payload.slice();
    EXPECT_TRUE(slice.isObject());
    EXPECT_TRUE(
        (slice.hasKey(arangodb::StaticStrings::Code) &&
         slice.get(arangodb::StaticStrings::Code).isNumber<size_t>() &&
         size_t(arangodb::rest::ResponseCode::NOT_FOUND) ==
             slice.get(arangodb::StaticStrings::Code).getNumber<size_t>()));
    EXPECT_TRUE(
        (slice.hasKey(arangodb::StaticStrings::Error) &&
         slice.get(arangodb::StaticStrings::Error).isBoolean() &&
         true == slice.get(arangodb::StaticStrings::Error).getBoolean()));
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::ErrorNum) &&
                 slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() &&
                 TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND ==
                     ErrorCode{slice.get(arangodb::StaticStrings::ErrorNum)
                                   .getNumber<int>()}));
    EXPECT_TRUE(execContext
                    ->canUseView(vocbase->name(), "testDataSource",
                                 arangodb::ViewAccessLevel::Read)
                    .fail());  // not modified from above
  }

  // test auth wildcard (grant)
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testDataSource\" }");
    userManager->removeAllUsers();
    userManager->storeUser(false, userName, arangodb::StaticStrings::Empty,
                           true, arangodb::velocypack::Slice());
    EXPECT_TRUE(testUserExists());

    auto logicalCollection = std::shared_ptr<arangodb::LogicalCollection>(
        vocbase->createCollection(collectionJson->slice()).get(),
        [vocbase](arangodb::LogicalCollection* ptr) -> void {
          vocbase->dropCollection(ptr->id(), false);
        });
    ASSERT_FALSE(!logicalCollection);

    EXPECT_TRUE(execContext
                    ->canUseCollection(vocbase->name(), "testDataSource",
                                       arangodb::CollectionAccessLevel::Read)
                    .fail());
    giveUserAdmin();
    auto status = grantWildcardHandler.execute();
    takeUserAdmin();
    EXPECT_EQ(arangodb::RestStatus::DONE, status);
    EXPECT_EQ(arangodb::rest::ResponseCode::OK,
              grantWildcardResponce.responseCode());
    auto slice = grantWildcardResponce._payload.slice();
    EXPECT_TRUE(slice.isObject());
    EXPECT_TRUE(
        (slice.hasKey(vocbase->name() + "/*") &&
         slice.get(vocbase->name() + "/*").isString() &&
         arangodb::auth::convertFromAuthLevel(arangodb::auth::Level::RW) ==
             slice.get(vocbase->name() + "/*").copyString()));
    EXPECT_TRUE(
        execContext
            ->canUseCollection(vocbase->name(), "testDataSource",
                               arangodb::CollectionAccessLevel::WriteData)
            .ok());
  }

  // test auth wildcard (revoke)
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testDataSource\" }");
    userManager->removeAllUsers();
    userManager->storeUser(false, userName, arangodb::StaticStrings::Empty,
                           true, arangodb::velocypack::Slice());
    auto res = userManager->updateUser(
        userName,
        [&vocbase](arangodb::auth::User& user) -> arangodb::Result {
          user.grantCollection(
              vocbase->name(), "testDataSource",
              arangodb::auth::Level::RO);  // for missing collections
                                           // User::collectionAuthLevel(...)
                                           // returns database auth::Level
          return {};
        },
        arangodb::auth::UserManager::RetryOnConflict::No);
    ASSERT_TRUE(res.ok()) << res.errorMessage();

    auto logicalCollection = std::shared_ptr<arangodb::LogicalCollection>(
        vocbase->createCollection(collectionJson->slice()).get(),
        [vocbase](arangodb::LogicalCollection* ptr) -> void {
          vocbase->dropCollection(ptr->id(), false);
        });
    ASSERT_FALSE(!logicalCollection);

    EXPECT_TRUE(execContext
                    ->canUseCollection(vocbase->name(), "testDataSource",
                                       arangodb::CollectionAccessLevel::Read)
                    .ok());
    EXPECT_TRUE(
        execContext
            ->canUseCollection(vocbase->name(), "testDataSource",
                               arangodb::CollectionAccessLevel::WriteData)
            .fail());
    giveUserAdmin();
    auto status = revokeWildcardHandler.execute();
    takeUserAdmin();
    EXPECT_EQ(arangodb::RestStatus::DONE, status);
    EXPECT_TRUE((arangodb::rest::ResponseCode::ACCEPTED ==
                 revokeWildcardResponce.responseCode()));
    auto slice = revokeWildcardResponce._payload.slice();
    EXPECT_TRUE(slice.isObject());
    EXPECT_TRUE(
        (slice.hasKey(arangodb::StaticStrings::Code) &&
         slice.get(arangodb::StaticStrings::Code).isNumber<size_t>() &&
         size_t(arangodb::rest::ResponseCode::ACCEPTED) ==
             slice.get(arangodb::StaticStrings::Code).getNumber<size_t>()));
    EXPECT_TRUE(
        (slice.hasKey(arangodb::StaticStrings::Error) &&
         slice.get(arangodb::StaticStrings::Error).isBoolean() &&
         false == slice.get(arangodb::StaticStrings::Error).getBoolean()));
    EXPECT_TRUE(execContext
                    ->canUseCollection(vocbase->name(), "testDataSource",
                                       arangodb::CollectionAccessLevel::Read)
                    .ok());  // unchanged since revocation is only for
                             // exactly matching collection names
    EXPECT_TRUE(
        execContext
            ->canUseCollection(vocbase->name(), "testDataSource",
                               arangodb::CollectionAccessLevel::WriteData)
            .fail());  // unchanged since revocation is only for
                       // exactly matching collection names
  }
}
