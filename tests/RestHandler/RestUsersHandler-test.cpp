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

#include "velocypack/Parser.h"

#include "IResearch/RestHandlerMock.h"
#include "IResearch/common.h"
#include "Mocks/LogLevels.h"
#include "Mocks/Servers.h"
#include "Mocks/StorageEngineMock.h"

#include "Aql/QueryRegistry.h"
#include "Basics/StaticStrings.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "RestHandler/RestUsersHandler.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "RestServer/SystemDatabaseFeature.h"
#include "RestServer/ViewTypesFeature.h"
#include "RestServer/VocbaseContext.h"
#include "Sharding/ShardingFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "Utils/ExecContext.h"
#include "V8Server/V8DealerFeature.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/LogicalView.h"
#include "VocBase/vocbase.h"

#if USE_ENTERPRISE
#include "Enterprise/Ldap/LdapFeature.h"
#endif

namespace {

struct TestView : public arangodb::LogicalView {
  arangodb::Result _appendVelocyPackResult;
  arangodb::velocypack::Builder _properties;

  TestView(TRI_vocbase_t& vocbase, arangodb::velocypack::Slice const& definition)
      : arangodb::LogicalView(vocbase, definition) {}
  virtual arangodb::Result appendVelocyPackImpl(
      arangodb::velocypack::Builder& builder,
      Serialization) const override {
    builder.add("properties", _properties.slice());
    return _appendVelocyPackResult;
  }
  virtual arangodb::Result dropImpl() override { return arangodb::Result(); }
  virtual void open() override {}
  virtual arangodb::Result renameImpl(std::string const&) override {
    return arangodb::Result();
  }
  virtual arangodb::Result properties(arangodb::velocypack::Slice const& properties,
                                      bool partialUpdate) override {
    _properties = arangodb::velocypack::Builder(properties);
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
                                       arangodb::velocypack::Slice const& definition) const override {
    view = std::make_shared<TestView>(vocbase, definition);

    return arangodb::Result();
  }
};

}  // namespace

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

class RestUsersHandlerTest
    : public ::testing::Test,
      public arangodb::tests::LogSuppressor<arangodb::Logger::AUTHENTICATION, arangodb::LogLevel::ERR> {
 protected:
  arangodb::tests::mocks::MockAqlServer server;
  arangodb::SystemDatabaseFeature::ptr system;
  ViewFactory viewFactory;

  RestUsersHandlerTest()
      : server(), system(server.getFeature<arangodb::SystemDatabaseFeature>().use()) {
    auto& viewTypesFeature = server.getFeature<arangodb::ViewTypesFeature>();
    viewTypesFeature.emplace(arangodb::LogicalDataSource::Type::emplace(arangodb::velocypack::StringRef(
                                 "testViewType")),
                             viewFactory);
  }
};

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

TEST_F(RestUsersHandlerTest, test_collection_auth) {
  auto usersJson = arangodb::velocypack::Parser::fromJson(
      "{ \"name\": \"_users\", \"isSystem\": true }");
  static const std::string userName("testUser");
  auto& databaseFeature = server.getFeature<arangodb::DatabaseFeature>();
  TRI_vocbase_t* vocbase;  // will be owned by DatabaseFeature
  ASSERT_TRUE(databaseFeature.createDatabase(testDBInfo(server.server()), vocbase).ok());
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
  auto revokeWildcardRequestPtr = std::make_unique<GeneralRequestMock>(*vocbase);
  auto& revokeWildcardRequest = *revokeWildcardRequestPtr;
  auto revokeWildcardResponcePtr = std::make_unique<GeneralResponseMock>();
  auto& revokeWildcardResponce = *revokeWildcardResponcePtr;
  arangodb::RestUsersHandler grantHandler(server.server(), grantRequestPtr.release(),
                                          grantResponcePtr.release());
  arangodb::RestUsersHandler grantWildcardHandler(server.server(),
                                                  grantWildcardRequestPtr.release(),
                                                  grantWildcardResponcePtr.release());
  arangodb::RestUsersHandler revokeHandler(server.server(), revokeRequestPtr.release(),
                                           revokeResponcePtr.release());
  arangodb::RestUsersHandler revokeWildcardHandler(server.server(),
                                                   revokeWildcardRequestPtr.release(),
                                                   revokeWildcardResponcePtr.release());

  grantRequest.addSuffix("testUser");
  grantRequest.addSuffix("database");
  grantRequest.addSuffix(vocbase->name());
  grantRequest.addSuffix("testDataSource");
  grantRequest.setRequestType(arangodb::rest::RequestType::PUT);
  grantRequest._payload.openObject();
  grantRequest._payload.add("grant", arangodb::velocypack::Value(arangodb::auth::convertFromAuthLevel(
                                         arangodb::auth::Level::RW)));
  grantRequest._payload.close();

  grantWildcardRequest.addSuffix("testUser");
  grantWildcardRequest.addSuffix("database");
  grantWildcardRequest.addSuffix(vocbase->name());
  grantWildcardRequest.addSuffix("*");
  grantWildcardRequest.setRequestType(arangodb::rest::RequestType::PUT);
  grantWildcardRequest._payload.openObject();
  grantWildcardRequest._payload.add("grant", arangodb::velocypack::Value(arangodb::auth::convertFromAuthLevel(
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

  struct ExecContext : public arangodb::ExecContext {
    ExecContext()
        : arangodb::ExecContext(arangodb::ExecContext::Type::Default, userName,
                                "", arangodb::auth::Level::RW,
                                arangodb::auth::Level::NONE, true) {
    }  // ExecContext::isAdminUser() == true
  } execContext;
  arangodb::ExecContextScope execContextScope(&execContext);
  auto* authFeature = arangodb::AuthenticationFeature::instance();
  auto* userManager = authFeature->userManager();
  userManager->setGlobalVersion(0);  // required for UserManager::loadFromDB()

  // test auth missing (grant)
  {
    auto scopedUsers = std::shared_ptr<arangodb::LogicalCollection>(
        system->createCollection(usersJson->slice()).get(),
        [this](arangodb::LogicalCollection* ptr) -> void {
          system->dropCollection(ptr->id(), true, 0.0);
        });
    arangodb::auth::UserMap userMap;
    arangodb::auth::User* userPtr = nullptr;
    userManager->setAuthInfo(userMap);  // insure an empty map is set before UserManager::storeUser(...)
    userManager->storeUser(false, userName, arangodb::StaticStrings::Empty,
                           true, arangodb::velocypack::Slice());
    userManager->accessUser(userName, [&userPtr](arangodb::auth::User const& user) -> arangodb::Result {
      userPtr = const_cast<arangodb::auth::User*>(&user);
      return arangodb::Result();
    });
    ASSERT_NE(nullptr, userPtr);

    EXPECT_TRUE(
        (arangodb::auth::Level::NONE ==
         execContext.collectionAuthLevel(vocbase->name(), "testDataSource")));
    auto status = grantHandler.execute();
    EXPECT_EQ(arangodb::RestStatus::DONE, status);
    EXPECT_EQ(arangodb::rest::ResponseCode::NOT_FOUND, grantResponce.responseCode());
    auto slice = grantResponce._payload.slice();
    EXPECT_TRUE(slice.isObject());
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::Code) &&
                 slice.get(arangodb::StaticStrings::Code).isNumber<size_t>() &&
                 size_t(arangodb::rest::ResponseCode::NOT_FOUND) ==
                     slice.get(arangodb::StaticStrings::Code).getNumber<size_t>()));
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::Error) &&
                 slice.get(arangodb::StaticStrings::Error).isBoolean() &&
                 true == slice.get(arangodb::StaticStrings::Error).getBoolean()));
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::ErrorNum) &&
                 slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() &&
                 TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND ==
                     ErrorCode{slice.get(arangodb::StaticStrings::ErrorNum).getNumber<int>()}));
    EXPECT_TRUE(
        (arangodb::auth::Level::NONE ==
         execContext.collectionAuthLevel(vocbase->name(), "testDataSource")));
  }

  // test auth missing (revoke)
  {
    auto scopedUsers = std::shared_ptr<arangodb::LogicalCollection>(
        system->createCollection(usersJson->slice()).get(),
        [this](arangodb::LogicalCollection* ptr) -> void {
          system->dropCollection(ptr->id(), true, 0.0);
        });
    arangodb::auth::UserMap userMap;
    arangodb::auth::User* userPtr = nullptr;
    userManager->setAuthInfo(userMap);  // insure an empty map is set before UserManager::storeUser(...)
    userManager->storeUser(false, userName, arangodb::StaticStrings::Empty,
                           true, arangodb::velocypack::Slice());
    userManager->accessUser(userName, [&userPtr](arangodb::auth::User const& user) -> arangodb::Result {
      userPtr = const_cast<arangodb::auth::User*>(&user);
      return arangodb::Result();
    });
    ASSERT_NE(nullptr, userPtr);
    userPtr->grantCollection(vocbase->name(),
                             "testDataSource", arangodb::auth::Level::RO);  // for missing collections User::collectionAuthLevel(...) returns database auth::Level

    EXPECT_TRUE(
        (arangodb::auth::Level::RO ==
         execContext.collectionAuthLevel(vocbase->name(), "testDataSource")));
    auto status = revokeHandler.execute();
    EXPECT_EQ(arangodb::RestStatus::DONE, status);
    EXPECT_EQ(arangodb::rest::ResponseCode::NOT_FOUND, revokeResponce.responseCode());
    auto slice = revokeResponce._payload.slice();
    EXPECT_TRUE(slice.isObject());
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::Code) &&
                 slice.get(arangodb::StaticStrings::Code).isNumber<size_t>() &&
                 size_t(arangodb::rest::ResponseCode::NOT_FOUND) ==
                     slice.get(arangodb::StaticStrings::Code).getNumber<size_t>()));
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::Error) &&
                 slice.get(arangodb::StaticStrings::Error).isBoolean() &&
                 true == slice.get(arangodb::StaticStrings::Error).getBoolean()));
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::ErrorNum) &&
         slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() &&
         TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND ==
             ErrorCode{slice.get(arangodb::StaticStrings::ErrorNum).getNumber<int>()}));
    EXPECT_TRUE(
        (arangodb::auth::Level::RO ==
         execContext.collectionAuthLevel(vocbase->name(), "testDataSource")));  // not modified from above
  }

  // test auth collection (grant)
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testDataSource\" }");
    auto scopedUsers = std::shared_ptr<arangodb::LogicalCollection>(
        system->createCollection(usersJson->slice()).get(),
        [this](arangodb::LogicalCollection* ptr) -> void {
          system->dropCollection(ptr->id(), true, 0.0);
        });
    arangodb::auth::UserMap userMap;
    arangodb::auth::User* userPtr = nullptr;
    userManager->setAuthInfo(userMap);  // insure an empty map is set before UserManager::storeUser(...)
    userManager->storeUser(false, userName, arangodb::StaticStrings::Empty,
                           true, arangodb::velocypack::Slice());
    userManager->accessUser(userName, [&userPtr](arangodb::auth::User const& user) -> arangodb::Result {
      userPtr = const_cast<arangodb::auth::User*>(&user);
      return arangodb::Result();
    });
    ASSERT_NE(nullptr, userPtr);
    auto logicalCollection = std::shared_ptr<arangodb::LogicalCollection>(
        vocbase->createCollection(collectionJson->slice()).get(),
        [vocbase](arangodb::LogicalCollection* ptr) -> void {
          vocbase->dropCollection(ptr->id(), false, 0);
        });
    ASSERT_FALSE(!logicalCollection);

    EXPECT_TRUE(
        (arangodb::auth::Level::NONE ==
         execContext.collectionAuthLevel(vocbase->name(), "testDataSource")));
    auto status = grantHandler.execute();
    EXPECT_EQ(arangodb::RestStatus::DONE, status);
    EXPECT_EQ(arangodb::rest::ResponseCode::OK, grantResponce.responseCode());
    auto slice = grantResponce._payload.slice();
    EXPECT_TRUE(slice.isObject());
    EXPECT_TRUE((slice.hasKey(vocbase->name() + "/testDataSource") &&
                 slice.get(vocbase->name() + "/testDataSource").isString() &&
                 arangodb::auth::convertFromAuthLevel(arangodb::auth::Level::RW) ==
                     slice.get(vocbase->name() + "/testDataSource").copyString()));
    EXPECT_TRUE(
        (arangodb::auth::Level::RW ==
         execContext.collectionAuthLevel(vocbase->name(), "testDataSource")));
  }

  // test auth collection (revoke)
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testDataSource\" }");
    auto scopedUsers = std::shared_ptr<arangodb::LogicalCollection>(
        system->createCollection(usersJson->slice()).get(),
        [this](arangodb::LogicalCollection* ptr) -> void {
          system->dropCollection(ptr->id(), true, 0.0);
        });
    arangodb::auth::UserMap userMap;
    arangodb::auth::User* userPtr = nullptr;
    userManager->setAuthInfo(userMap);  // insure an empty map is set before UserManager::storeUser(...)
    userManager->storeUser(false, userName, arangodb::StaticStrings::Empty,
                           true, arangodb::velocypack::Slice());
    userManager->accessUser(userName, [&userPtr](arangodb::auth::User const& user) -> arangodb::Result {
      userPtr = const_cast<arangodb::auth::User*>(&user);
      return arangodb::Result();
    });
    ASSERT_NE(nullptr, userPtr);
    userPtr->grantCollection(vocbase->name(),
                             "testDataSource", arangodb::auth::Level::RO);  // for missing collections User::collectionAuthLevel(...) returns database auth::Level
    auto logicalCollection = std::shared_ptr<arangodb::LogicalCollection>(
        vocbase->createCollection(collectionJson->slice()).get(),
        [vocbase](arangodb::LogicalCollection* ptr) -> void {
          vocbase->dropCollection(ptr->id(), false, 0);
        });
    ASSERT_FALSE(!logicalCollection);

    EXPECT_TRUE(
        (arangodb::auth::Level::RO ==
         execContext.collectionAuthLevel(vocbase->name(), "testDataSource")));
    auto status = revokeHandler.execute();
    EXPECT_EQ(arangodb::RestStatus::DONE, status);
    EXPECT_EQ(arangodb::rest::ResponseCode::ACCEPTED, revokeResponce.responseCode());
    auto slice = revokeResponce._payload.slice();
    EXPECT_TRUE(slice.isObject());
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::Code) &&
                 slice.get(arangodb::StaticStrings::Code).isNumber<size_t>() &&
                 size_t(arangodb::rest::ResponseCode::ACCEPTED) ==
                     slice.get(arangodb::StaticStrings::Code).getNumber<size_t>()));
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::Error) &&
                 slice.get(arangodb::StaticStrings::Error).isBoolean() &&
                 false == slice.get(arangodb::StaticStrings::Error).getBoolean()));
    EXPECT_TRUE(
        (arangodb::auth::Level::NONE ==
         execContext.collectionAuthLevel(vocbase->name(), "testDataSource")));
  }

  // test auth view (grant)
  {
    auto viewJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testDataSource\", \"type\": \"testViewType\" }");
    auto scopedUsers = std::shared_ptr<arangodb::LogicalCollection>(
        system->createCollection(usersJson->slice()).get(),
        [this](arangodb::LogicalCollection* ptr) -> void {
          system->dropCollection(ptr->id(), true, 0.0);
        });
    arangodb::auth::UserMap userMap;
    arangodb::auth::User* userPtr = nullptr;
    userManager->setAuthInfo(userMap);  // insure an empty map is set before UserManager::storeUser(...)
    userManager->storeUser(false, userName, arangodb::StaticStrings::Empty,
                           true, arangodb::velocypack::Slice());
    userManager->accessUser(userName, [&userPtr](arangodb::auth::User const& user) -> arangodb::Result {
      userPtr = const_cast<arangodb::auth::User*>(&user);
      return arangodb::Result();
    });
    ASSERT_NE(nullptr, userPtr);
    auto logicalView = std::shared_ptr<arangodb::LogicalView>(
        vocbase->createView(viewJson->slice()).get(),
        [vocbase](arangodb::LogicalView* ptr) -> void {
          vocbase->dropView(ptr->id(), false);
        });
    ASSERT_FALSE(!logicalView);

    EXPECT_TRUE(
        (arangodb::auth::Level::NONE ==
         execContext.collectionAuthLevel(vocbase->name(), "testDataSource")));
    auto status = grantHandler.execute();
    EXPECT_EQ(arangodb::RestStatus::DONE, status);
    EXPECT_EQ(arangodb::rest::ResponseCode::NOT_FOUND, grantResponce.responseCode());
    auto slice = grantResponce._payload.slice();
    EXPECT_TRUE(slice.isObject());
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::Code) &&
                 slice.get(arangodb::StaticStrings::Code).isNumber<size_t>() &&
                 size_t(arangodb::rest::ResponseCode::NOT_FOUND) ==
                     slice.get(arangodb::StaticStrings::Code).getNumber<size_t>()));
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::Error) &&
                 slice.get(arangodb::StaticStrings::Error).isBoolean() &&
                 true == slice.get(arangodb::StaticStrings::Error).getBoolean()));
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::ErrorNum) &&
         slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() &&
         TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND ==
             ErrorCode{slice.get(arangodb::StaticStrings::ErrorNum).getNumber<int>()}));
    EXPECT_TRUE(
        (arangodb::auth::Level::NONE ==
         execContext.collectionAuthLevel(vocbase->name(), "testDataSource")));
  }

  // test auth view (revoke)
  {
    auto viewJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testDataSource\", \"type\": \"testViewType\" }");
    auto scopedUsers = std::shared_ptr<arangodb::LogicalCollection>(
        system->createCollection(usersJson->slice()).get(),
        [this](arangodb::LogicalCollection* ptr) -> void {
          system->dropCollection(ptr->id(), true, 0.0);
        });
    arangodb::auth::UserMap userMap;
    arangodb::auth::User* userPtr = nullptr;
    userManager->setAuthInfo(userMap);  // insure an empty map is set before UserManager::storeUser(...)
    userManager->storeUser(false, userName, arangodb::StaticStrings::Empty,
                           true, arangodb::velocypack::Slice());
    userManager->accessUser(userName, [&userPtr](arangodb::auth::User const& user) -> arangodb::Result {
      userPtr = const_cast<arangodb::auth::User*>(&user);
      return arangodb::Result();
    });
    ASSERT_NE(nullptr, userPtr);
    userPtr->grantCollection(vocbase->name(),
                             "testDataSource", arangodb::auth::Level::RO);  // for missing collections User::collectionAuthLevel(...) returns database auth::Level
    auto logicalView = std::shared_ptr<arangodb::LogicalView>(
        vocbase->createView(viewJson->slice()).get(),
        [vocbase](arangodb::LogicalView* ptr) -> void {
          vocbase->dropView(ptr->id(), false);
        });
    ASSERT_FALSE(!logicalView);

    EXPECT_TRUE(
        (arangodb::auth::Level::RO ==
         execContext.collectionAuthLevel(vocbase->name(), "testDataSource")));
    auto status = revokeHandler.execute();
    EXPECT_EQ(arangodb::RestStatus::DONE, status);
    EXPECT_EQ(arangodb::rest::ResponseCode::NOT_FOUND, revokeResponce.responseCode());
    auto slice = revokeResponce._payload.slice();
    EXPECT_TRUE(slice.isObject());
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::Code) &&
                 slice.get(arangodb::StaticStrings::Code).isNumber<size_t>() &&
                 size_t(arangodb::rest::ResponseCode::NOT_FOUND) ==
                     slice.get(arangodb::StaticStrings::Code).getNumber<size_t>()));
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::Error) &&
                 slice.get(arangodb::StaticStrings::Error).isBoolean() &&
                 true == slice.get(arangodb::StaticStrings::Error).getBoolean()));
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::ErrorNum) &&
                 slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() &&
                 TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND ==
                     ErrorCode{slice.get(arangodb::StaticStrings::ErrorNum).getNumber<int>()}));
    EXPECT_TRUE(
        (arangodb::auth::Level::RO ==
         execContext.collectionAuthLevel(vocbase->name(), "testDataSource")));  // not modified from above
  }

  // test auth wildcard (grant)
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testDataSource\" }");
    auto scopedUsers = std::shared_ptr<arangodb::LogicalCollection>(
        system->createCollection(usersJson->slice()).get(),
        [this](arangodb::LogicalCollection* ptr) -> void {
          system->dropCollection(ptr->id(), true, 0.0);
        });
    arangodb::auth::UserMap userMap;
    arangodb::auth::User* userPtr = nullptr;
    userManager->setAuthInfo(userMap);  // insure an empty map is set before UserManager::storeUser(...)
    userManager->storeUser(false, userName, arangodb::StaticStrings::Empty,
                           true, arangodb::velocypack::Slice());
    userManager->accessUser(userName, [&userPtr](arangodb::auth::User const& user) -> arangodb::Result {
      userPtr = const_cast<arangodb::auth::User*>(&user);
      return arangodb::Result();
    });
    ASSERT_NE(nullptr, userPtr);
    auto logicalCollection = std::shared_ptr<arangodb::LogicalCollection>(
        vocbase->createCollection(collectionJson->slice()).get(),
        [vocbase](arangodb::LogicalCollection* ptr) -> void {
          vocbase->dropCollection(ptr->id(), false, 0);
        });
    ASSERT_FALSE(!logicalCollection);

    EXPECT_TRUE(
        (arangodb::auth::Level::NONE ==
         execContext.collectionAuthLevel(vocbase->name(), "testDataSource")));
    auto status = grantWildcardHandler.execute();
    EXPECT_EQ(arangodb::RestStatus::DONE, status);
    EXPECT_EQ(arangodb::rest::ResponseCode::OK, grantWildcardResponce.responseCode());
    auto slice = grantWildcardResponce._payload.slice();
    EXPECT_TRUE(slice.isObject());
    EXPECT_TRUE((slice.hasKey(vocbase->name() + "/*") &&
                 slice.get(vocbase->name() + "/*").isString() &&
                 arangodb::auth::convertFromAuthLevel(arangodb::auth::Level::RW) ==
                     slice.get(vocbase->name() + "/*").copyString()));
    EXPECT_TRUE(
        (arangodb::auth::Level::RW ==
         execContext.collectionAuthLevel(vocbase->name(), "testDataSource")));
  }

  // test auth wildcard (revoke)
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testDataSource\" }");
    auto scopedUsers = std::shared_ptr<arangodb::LogicalCollection>(
        system->createCollection(usersJson->slice()).get(),
        [this](arangodb::LogicalCollection* ptr) -> void {
          system->dropCollection(ptr->id(), true, 0.0);
        });
    arangodb::auth::UserMap userMap;
    arangodb::auth::User* userPtr = nullptr;
    userManager->setAuthInfo(userMap);  // insure an empty map is set before UserManager::storeUser(...)
    userManager->storeUser(false, userName, arangodb::StaticStrings::Empty,
                           true, arangodb::velocypack::Slice());
    userManager->accessUser(userName, [&userPtr](arangodb::auth::User const& user) -> arangodb::Result {
      userPtr = const_cast<arangodb::auth::User*>(&user);
      return arangodb::Result();
    });
    ASSERT_NE(nullptr, userPtr);
    userPtr->grantCollection(vocbase->name(),
                             "testDataSource", arangodb::auth::Level::RO);  // for missing collections User::collectionAuthLevel(...) returns database auth::Level
    auto logicalCollection = std::shared_ptr<arangodb::LogicalCollection>(
        vocbase->createCollection(collectionJson->slice()).get(),
        [vocbase](arangodb::LogicalCollection* ptr) -> void {
          vocbase->dropCollection(ptr->id(), false, 0);
        });
    ASSERT_FALSE(!logicalCollection);

    EXPECT_TRUE(
        (arangodb::auth::Level::RO ==
         execContext.collectionAuthLevel(vocbase->name(), "testDataSource")));
    auto status = revokeWildcardHandler.execute();
    EXPECT_EQ(arangodb::RestStatus::DONE, status);
    EXPECT_TRUE((arangodb::rest::ResponseCode::ACCEPTED ==
                 revokeWildcardResponce.responseCode()));
    auto slice = revokeWildcardResponce._payload.slice();
    EXPECT_TRUE(slice.isObject());
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::Code) &&
                 slice.get(arangodb::StaticStrings::Code).isNumber<size_t>() &&
                 size_t(arangodb::rest::ResponseCode::ACCEPTED) ==
                     slice.get(arangodb::StaticStrings::Code).getNumber<size_t>()));
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::Error) &&
                 slice.get(arangodb::StaticStrings::Error).isBoolean() &&
                 false == slice.get(arangodb::StaticStrings::Error).getBoolean()));
    EXPECT_TRUE(
        (arangodb::auth::Level::RO ==
         execContext.collectionAuthLevel(vocbase->name(), "testDataSource")));  // unchanged since revocation is only for exactly matching collection names
  }
}
