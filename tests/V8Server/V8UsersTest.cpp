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
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#ifndef USE_V8
#error this file is not supposed to be used in builds with -DUSE_V8=Off
#endif

#include "Mocks/Servers.h"  // this must be first because windows

#include <v8.h>

#include "gtest/gtest.h"

#include "velocypack/Builder.h"

#include "IResearch/common.h"
#include "Mocks/LogLevels.h"

#include "ApplicationFeatures/HttpEndpointProvider.h"
#include "ApplicationFeatures/CommunicationFeaturePhase.h"
#include "Aql/QueryRegistry.h"
#include "Auth/UserManager.h"
#include "Basics/StaticStrings.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "Replication/ReplicationFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "RestServer/SystemDatabaseFeature.h"
#include "RestServer/ViewTypesFeature.h"
#include "Sharding/ShardingFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "Utils/ExecContext.h"
#include "V8/v8-vpack.h"
#include "V8Server/V8DealerFeature.h"
#include "V8/V8SecurityFeature.h"
#include "V8Server/v8-users.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/LogicalView.h"
#include "VocBase/vocbase.h"

#if USE_ENTERPRISE
#include "Enterprise/Encryption/EncryptionFeature.h"
#endif

namespace {
struct TestView : public arangodb::LogicalView {
  arangodb::Result _appendVelocyPackResult;
  arangodb::velocypack::Builder _properties;

  static constexpr auto typeInfo() noexcept {
    return std::pair{static_cast<arangodb::ViewType>(42),
                     std::string_view{"testViewType"}};
  }

  TestView(TRI_vocbase_t& vocbase,
           arangodb::velocypack::Slice const& definition)
      : arangodb::LogicalView(*this, vocbase, definition, false) {}
  arangodb::Result appendVPackImpl(arangodb::velocypack::Builder& build,
                                   Serialization, bool) const override {
    build.add("properties", _properties.slice());
    return _appendVelocyPackResult;
  }
  virtual arangodb::Result dropImpl() override { return arangodb::Result(); }
  virtual void open() override {}
  virtual arangodb::Result renameImpl(std::string const&) override {
    return arangodb::Result();
  }
  virtual arangodb::Result properties(arangodb::velocypack::Slice properties,
                                      bool isUserRequest,
                                      bool /*partialUpdate*/) override {
    EXPECT_TRUE(isUserRequest);
    _properties = arangodb::velocypack::Builder(properties);
    return arangodb::Result();
  }
  virtual bool visitCollections(
      CollectionVisitor const& /*visitor*/) const override {
    return true;
  }
};

struct ViewFactory : public arangodb::ViewFactory {
  virtual arangodb::Result create(arangodb::LogicalView::ptr& view,
                                  TRI_vocbase_t& vocbase,
                                  arangodb::velocypack::Slice definition,
                                  bool isUserRequest) const override {
    EXPECT_TRUE(isUserRequest);
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

class V8UsersTest
    : public ::testing::Test,
      public arangodb::tests::LogSuppressor<arangodb::Logger::AUTHENTICATION,
                                            arangodb::LogLevel::ERR> {
 protected:
  arangodb::tests::mocks::MockAqlServer server;
  ViewFactory viewFactory;
  arangodb::SystemDatabaseFeature::ptr system;

  V8UsersTest()
      : server(),
        system(server.getFeature<arangodb::SystemDatabaseFeature>().use()) {
    arangodb::tests::v8Init();  // on-time initialize V8

    auto& viewTypesFeature = server.getFeature<arangodb::ViewTypesFeature>();
    viewTypesFeature.emplace(TestView::typeInfo().second, viewFactory);
  }
};

TEST_F(V8UsersTest, test_collection_auth) {
  auto usersJson = arangodb::velocypack::Parser::fromJson(
      "{ \"name\": \"_users\", \"isSystem\": true }");
  static const std::string userName("testUser");
  auto& databaseFeature = server.getFeature<arangodb::DatabaseFeature>();
  TRI_vocbase_t* vocbase;  // will be owned by DatabaseFeature
  ASSERT_TRUE(
      databaseFeature.createDatabase(testDBInfo(server.server()), vocbase)
          .ok());
  v8::Isolate::CreateParams isolateParams;
  auto arrayBufferAllocator = std::unique_ptr<v8::ArrayBuffer::Allocator>(
      v8::ArrayBuffer::Allocator::NewDefaultAllocator());
  isolateParams.array_buffer_allocator = arrayBufferAllocator.get();
  auto isolate = std::shared_ptr<v8::Isolate>(
      v8::Isolate::New(isolateParams),
      [](v8::Isolate* p) -> void { p->Dispose(); });
  ASSERT_NE(nullptr, isolate);

  // otherwise v8::Isolate::Logger() will fail (called from
  // v8::Exception::Error)
  v8::Isolate::Scope isolateScope(isolate.get());
  // otherwise v8::Isolate::Logger() will fail (called from
  // v8::Exception::Error)
  // required for v8::Context::New(...), v8::ObjectTemplate::New(...) and
  // TRI_AddMethodVocbase(...)
  v8::HandleScope handleScope(isolate.get());
  auto context = v8::Context::New(isolate.get());
  // required for TRI_AddMethodVocbase(...)
  v8::Context::Scope contextScope(context);
  // create and set inside 'isolate' for use with 'TRI_GET_GLOBALS()'
  std::unique_ptr<V8Global<arangodb::ArangodServer>> v8g(
      CreateV8Globals(server.server(), isolate.get(), 0));
  // otherwise v8:-utils::CreateErrorObject(...) will fail
  v8g->ArangoErrorTempl.Reset(isolate.get(),
                              v8::ObjectTemplate::New(isolate.get()));
  v8g->_vocbase = vocbase;
  TRI_InitV8Users(context, vocbase, v8g.get(), isolate.get());
  auto arangoUsers =
      v8::Local<v8::ObjectTemplate>::New(isolate.get(), v8g->UsersTempl)
          ->NewInstance(TRI_IGETC)
          .FromMaybe(v8::Local<v8::Object>());
  auto fn_grantCollection =
      arangoUsers
          ->Get(context, TRI_V8_ASCII_STRING(isolate.get(), "grantCollection"))
          .FromMaybe(v8::Local<v8::Value>());
  EXPECT_TRUE(fn_grantCollection->IsFunction());
  auto fn_revokeCollection =
      arangoUsers
          ->Get(context, TRI_V8_ASCII_STRING(isolate.get(), "revokeCollection"))
          .FromMaybe(v8::Local<v8::Value>());
  EXPECT_TRUE(fn_revokeCollection->IsFunction());
  std::vector<v8::Local<v8::Value>> grantArgs = {
      TRI_V8_STD_STRING(isolate.get(), userName),
      TRI_V8_STD_STRING(isolate.get(), vocbase->name()),
      TRI_V8_ASCII_STRING(isolate.get(), "testDataSource"),
      TRI_V8_STD_STRING(isolate.get(), arangodb::auth::convertFromAuthLevel(
                                           arangodb::auth::Level::RW)),
  };
  std::vector<v8::Local<v8::Value>> grantWildcardArgs = {
      TRI_V8_STD_STRING(isolate.get(), userName),
      TRI_V8_STD_STRING(isolate.get(), vocbase->name()),
      TRI_V8_ASCII_STRING(isolate.get(), "*"),
      TRI_V8_STD_STRING(isolate.get(), arangodb::auth::convertFromAuthLevel(
                                           arangodb::auth::Level::RW)),
  };
  std::vector<v8::Local<v8::Value>> revokeArgs = {
      TRI_V8_STD_STRING(isolate.get(), userName),
      TRI_V8_STD_STRING(isolate.get(), vocbase->name()),
      TRI_V8_ASCII_STRING(isolate.get(), "testDataSource"),
  };
  std::vector<v8::Local<v8::Value>> revokeWildcardArgs = {
      TRI_V8_STD_STRING(isolate.get(), userName),
      TRI_V8_STD_STRING(isolate.get(), vocbase->name()),
      TRI_V8_ASCII_STRING(isolate.get(), "*"),
  };

  struct ExecContext : public arangodb::ExecContext {
    ExecContext()
        : arangodb::ExecContext(arangodb::ExecContext::ConstructorToken{},
                                arangodb::ExecContext::Type::Default, userName,
                                "", arangodb::auth::Level::RW,
                                arangodb::auth::Level::NONE, true) {
    }  // ExecContext::isAdminUser() == true
  };
  auto execContext = std::make_shared<ExecContext>();
  arangodb::ExecContextScope execContextScope(execContext);
  auto* authFeature = arangodb::AuthenticationFeature::instance();
  auto* userManager = authFeature->userManager();
  userManager->setGlobalVersion(0);  // required for UserManager::loadFromDB()

  // test auth missing (grant)
  {
    auto scopedUsers = std::shared_ptr<arangodb::LogicalCollection>(
        system->createCollection(usersJson->slice()).get(),
        [this](arangodb::LogicalCollection* ptr) -> void {
          system->dropCollection(ptr->id(), true);
        });
    arangodb::auth::UserMap userMap;
    arangodb::auth::User* userPtr = nullptr;
    userManager->setAuthInfo(userMap);  // insure an empty map is set before
                                        // UserManager::storeUser(...)
    userManager->storeUser(false, userName, arangodb::StaticStrings::Empty,
                           true, arangodb::velocypack::Slice());
    userManager->accessUser(
        userName,
        [&userPtr](arangodb::auth::User const& user) -> arangodb::Result {
          userPtr = const_cast<arangodb::auth::User*>(&user);
          return arangodb::Result();
        });
    ASSERT_NE(nullptr, userPtr);

    EXPECT_TRUE(
        (arangodb::auth::Level::NONE ==
         execContext->collectionAuthLevel(vocbase->name(), "testDataSource")));
    arangodb::velocypack::Builder response;
    v8::TryCatch tryCatch(isolate.get());
    auto result = v8::Function::Cast(*fn_grantCollection)
                      ->CallAsFunction(context, arangoUsers,
                                       static_cast<int>(grantArgs.size()),
                                       grantArgs.data());
    EXPECT_TRUE(result.IsEmpty());
    EXPECT_TRUE(tryCatch.HasCaught());
    TRI_V8ToVPack(isolate.get(), response, tryCatch.Exception(), false);
    auto slice = response.slice();
    EXPECT_TRUE(slice.isObject());
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::ErrorNum) &&
                 slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() &&
                 TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND ==
                     ErrorCode{slice.get(arangodb::StaticStrings::ErrorNum)
                                   .getNumber<int>()}));
    EXPECT_TRUE(
        (arangodb::auth::Level::NONE ==
         execContext->collectionAuthLevel(vocbase->name(), "testDataSource")));
  }

  // test auth missing (revoke)
  {
    auto scopedUsers = std::shared_ptr<arangodb::LogicalCollection>(
        system->createCollection(usersJson->slice()).get(),
        [this](arangodb::LogicalCollection* ptr) -> void {
          system->dropCollection(ptr->id(), true);
        });
    arangodb::auth::UserMap userMap;
    arangodb::auth::User* userPtr = nullptr;
    userManager->setAuthInfo(userMap);  // insure an empty map is set before
                                        // UserManager::storeUser(...)
    userManager->storeUser(false, userName, arangodb::StaticStrings::Empty,
                           true, arangodb::velocypack::Slice());
    userManager->accessUser(
        userName,
        [&userPtr](arangodb::auth::User const& user) -> arangodb::Result {
          userPtr = const_cast<arangodb::auth::User*>(&user);
          return arangodb::Result();
        });
    ASSERT_NE(nullptr, userPtr);
    userPtr->grantCollection(
        vocbase->name(), "testDataSource",
        arangodb::auth::Level::RO);  // for missing collections
                                     // User::collectionAuthLevel(...) returns
                                     // database auth::Level

    EXPECT_TRUE(
        (arangodb::auth::Level::RO ==
         execContext->collectionAuthLevel(vocbase->name(), "testDataSource")));
    arangodb::velocypack::Builder response;
    v8::TryCatch tryCatch(isolate.get());
    auto result = v8::Function::Cast(*fn_revokeCollection)
                      ->CallAsFunction(context, arangoUsers,
                                       static_cast<int>(revokeArgs.size()),
                                       revokeArgs.data());
    EXPECT_TRUE(result.IsEmpty());
    EXPECT_TRUE(tryCatch.HasCaught());
    TRI_V8ToVPack(isolate.get(), response, tryCatch.Exception(), false);
    auto slice = response.slice();
    EXPECT_TRUE(slice.isObject());
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::ErrorNum) &&
                 slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() &&
                 TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND ==
                     ErrorCode{slice.get(arangodb::StaticStrings::ErrorNum)
                                   .getNumber<int>()}));
    EXPECT_TRUE(
        (arangodb::auth::Level::RO ==
         execContext->collectionAuthLevel(
             vocbase->name(), "testDataSource")));  // not modified from above
  }

  // test auth collection (grant)
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testDataSource\" }");
    auto scopedUsers = std::shared_ptr<arangodb::LogicalCollection>(
        system->createCollection(usersJson->slice()).get(),
        [this](arangodb::LogicalCollection* ptr) -> void {
          system->dropCollection(ptr->id(), true);
        });
    arangodb::auth::UserMap userMap;
    arangodb::auth::User* userPtr = nullptr;
    userManager->setAuthInfo(userMap);  // insure an empty map is set before
                                        // UserManager::storeUser(...)
    userManager->storeUser(false, userName, arangodb::StaticStrings::Empty,
                           true, arangodb::velocypack::Slice());
    userManager->accessUser(
        userName,
        [&userPtr](arangodb::auth::User const& user) -> arangodb::Result {
          userPtr = const_cast<arangodb::auth::User*>(&user);
          return arangodb::Result();
        });
    ASSERT_NE(nullptr, userPtr);
    auto logicalCollection = std::shared_ptr<arangodb::LogicalCollection>(
        vocbase->createCollection(collectionJson->slice()).get(),
        [vocbase](arangodb::LogicalCollection* ptr) -> void {
          vocbase->dropCollection(ptr->id(), false);
        });
    ASSERT_FALSE(!logicalCollection);

    EXPECT_TRUE(
        (arangodb::auth::Level::NONE ==
         execContext->collectionAuthLevel(vocbase->name(), "testDataSource")));
    arangodb::velocypack::Builder response;
    v8::TryCatch tryCatch(isolate.get());
    auto result = v8::Function::Cast(*fn_grantCollection)
                      ->CallAsFunction(context, arangoUsers,
                                       static_cast<int>(grantArgs.size()),
                                       grantArgs.data());
    ASSERT_FALSE(result.IsEmpty());
    EXPECT_TRUE(result.ToLocalChecked()->IsUndefined());
    EXPECT_FALSE(tryCatch.HasCaught());
    EXPECT_TRUE(
        (arangodb::auth::Level::RW ==
         execContext->collectionAuthLevel(vocbase->name(), "testDataSource")));
  }

  // test auth collection (revoke)
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testDataSource\" }");
    auto scopedUsers = std::shared_ptr<arangodb::LogicalCollection>(
        system->createCollection(usersJson->slice()).get(),
        [this](arangodb::LogicalCollection* ptr) -> void {
          system->dropCollection(ptr->id(), true);
        });
    arangodb::auth::UserMap userMap;
    arangodb::auth::User* userPtr = nullptr;
    userManager->setAuthInfo(userMap);  // insure an empty map is set before
                                        // UserManager::storeUser(...)
    userManager->storeUser(false, userName, arangodb::StaticStrings::Empty,
                           true, arangodb::velocypack::Slice());
    userManager->accessUser(
        userName,
        [&userPtr](arangodb::auth::User const& user) -> arangodb::Result {
          userPtr = const_cast<arangodb::auth::User*>(&user);
          return arangodb::Result();
        });
    ASSERT_NE(nullptr, userPtr);
    userPtr->grantCollection(
        vocbase->name(), "testDataSource",
        arangodb::auth::Level::RO);  // for missing collections
                                     // User::collectionAuthLevel(...) returns
                                     // database auth::Level
    auto logicalCollection = std::shared_ptr<arangodb::LogicalCollection>(
        vocbase->createCollection(collectionJson->slice()).get(),
        [vocbase](arangodb::LogicalCollection* ptr) -> void {
          vocbase->dropCollection(ptr->id(), false);
        });
    ASSERT_FALSE(!logicalCollection);

    EXPECT_TRUE(
        (arangodb::auth::Level::RO ==
         execContext->collectionAuthLevel(vocbase->name(), "testDataSource")));
    arangodb::velocypack::Builder response;
    v8::TryCatch tryCatch(isolate.get());
    auto result = v8::Function::Cast(*fn_revokeCollection)
                      ->CallAsFunction(context, arangoUsers,
                                       static_cast<int>(revokeArgs.size()),
                                       revokeArgs.data());
    EXPECT_FALSE(result.IsEmpty());
    EXPECT_TRUE(result.ToLocalChecked()->IsUndefined());
    EXPECT_FALSE(tryCatch.HasCaught());
    EXPECT_TRUE(
        (arangodb::auth::Level::NONE ==
         execContext->collectionAuthLevel(vocbase->name(), "testDataSource")));
  }

  // test auth view (grant)
  {
    auto viewJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testDataSource\", \"type\": \"testViewType\" }");
    auto scopedUsers = std::shared_ptr<arangodb::LogicalCollection>(
        system->createCollection(usersJson->slice()).get(),
        [this](arangodb::LogicalCollection* ptr) -> void {
          system->dropCollection(ptr->id(), true);
        });
    arangodb::auth::UserMap userMap;
    arangodb::auth::User* userPtr = nullptr;
    userManager->setAuthInfo(userMap);  // insure an empty map is set before
                                        // UserManager::storeUser(...)
    userManager->storeUser(false, userName, arangodb::StaticStrings::Empty,
                           true, arangodb::velocypack::Slice());
    userManager->accessUser(
        userName,
        [&userPtr](arangodb::auth::User const& user) -> arangodb::Result {
          userPtr = const_cast<arangodb::auth::User*>(&user);
          return arangodb::Result();
        });
    ASSERT_NE(nullptr, userPtr);
    auto logicalView = std::shared_ptr<arangodb::LogicalView>(
        vocbase->createView(viewJson->slice(), false).get(),
        [vocbase](arangodb::LogicalView* ptr) -> void {
          vocbase->dropView(ptr->id(), false);
        });
    ASSERT_FALSE(!logicalView);

    EXPECT_TRUE(
        (arangodb::auth::Level::NONE ==
         execContext->collectionAuthLevel(vocbase->name(), "testDataSource")));
    arangodb::velocypack::Builder response;
    v8::TryCatch tryCatch(isolate.get());
    auto result = v8::Function::Cast(*fn_grantCollection)
                      ->CallAsFunction(context, arangoUsers,
                                       static_cast<int>(grantArgs.size()),
                                       grantArgs.data());
    EXPECT_TRUE(result.IsEmpty());
    EXPECT_TRUE(tryCatch.HasCaught());
    TRI_V8ToVPack(isolate.get(), response, tryCatch.Exception(), false);
    auto slice = response.slice();
    EXPECT_TRUE(slice.isObject());
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::ErrorNum) &&
                 slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() &&
                 TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND ==
                     ErrorCode{slice.get(arangodb::StaticStrings::ErrorNum)
                                   .getNumber<int>()}));
    EXPECT_TRUE(
        (arangodb::auth::Level::NONE ==
         execContext->collectionAuthLevel(vocbase->name(), "testDataSource")));
  }

  // test auth view (revoke)
  {
    auto viewJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testDataSource\", \"type\": \"testViewType\" }");
    auto scopedUsers = std::shared_ptr<arangodb::LogicalCollection>(
        system->createCollection(usersJson->slice()).get(),
        [this](arangodb::LogicalCollection* ptr) -> void {
          system->dropCollection(ptr->id(), true);
        });
    arangodb::auth::UserMap userMap;
    arangodb::auth::User* userPtr = nullptr;
    userManager->setAuthInfo(userMap);  // insure an empty map is set before
                                        // UserManager::storeUser(...)
    userManager->storeUser(false, userName, arangodb::StaticStrings::Empty,
                           true, arangodb::velocypack::Slice());
    userManager->accessUser(
        userName,
        [&userPtr](arangodb::auth::User const& user) -> arangodb::Result {
          userPtr = const_cast<arangodb::auth::User*>(&user);
          return arangodb::Result();
        });
    ASSERT_NE(nullptr, userPtr);
    userPtr->grantCollection(
        vocbase->name(), "testDataSource",
        arangodb::auth::Level::RO);  // for missing collections
                                     // User::collectionAuthLevel(...) returns
                                     // database auth::Level
    auto logicalView = std::shared_ptr<arangodb::LogicalView>(
        vocbase->createView(viewJson->slice(), false).get(),
        [vocbase](arangodb::LogicalView* ptr) -> void {
          vocbase->dropView(ptr->id(), false);
        });
    ASSERT_FALSE(!logicalView);

    EXPECT_TRUE(
        (arangodb::auth::Level::RO ==
         execContext->collectionAuthLevel(vocbase->name(), "testDataSource")));
    arangodb::velocypack::Builder response;
    v8::TryCatch tryCatch(isolate.get());
    auto result = v8::Function::Cast(*fn_revokeCollection)
                      ->CallAsFunction(context, arangoUsers,
                                       static_cast<int>(revokeArgs.size()),
                                       revokeArgs.data());
    EXPECT_TRUE(result.IsEmpty());
    EXPECT_TRUE(tryCatch.HasCaught());
    TRI_V8ToVPack(isolate.get(), response, tryCatch.Exception(), false);
    auto slice = response.slice();
    EXPECT_TRUE(slice.isObject());
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::ErrorNum) &&
                 slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() &&
                 TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND ==
                     ErrorCode{slice.get(arangodb::StaticStrings::ErrorNum)
                                   .getNumber<int>()}));
    EXPECT_TRUE(
        (arangodb::auth::Level::RO ==
         execContext->collectionAuthLevel(
             vocbase->name(), "testDataSource")));  // not modified from above
  }

  // test auth wildcard (grant)
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testDataSource\" }");
    auto scopedUsers = std::shared_ptr<arangodb::LogicalCollection>(
        system->createCollection(usersJson->slice()).get(),
        [this](arangodb::LogicalCollection* ptr) -> void {
          system->dropCollection(ptr->id(), true);
        });
    arangodb::auth::UserMap userMap;
    arangodb::auth::User* userPtr = nullptr;
    userManager->setAuthInfo(userMap);  // insure an empty map is set before
                                        // UserManager::storeUser(...)
    userManager->storeUser(false, userName, arangodb::StaticStrings::Empty,
                           true, arangodb::velocypack::Slice());
    userManager->accessUser(
        userName,
        [&userPtr](arangodb::auth::User const& user) -> arangodb::Result {
          userPtr = const_cast<arangodb::auth::User*>(&user);
          return arangodb::Result();
        });
    ASSERT_NE(nullptr, userPtr);
    auto logicalCollection = std::shared_ptr<arangodb::LogicalCollection>(
        vocbase->createCollection(collectionJson->slice()).get(),
        [vocbase](arangodb::LogicalCollection* ptr) -> void {
          vocbase->dropCollection(ptr->id(), false);
        });
    ASSERT_FALSE(!logicalCollection);

    EXPECT_TRUE(
        (arangodb::auth::Level::NONE ==
         execContext->collectionAuthLevel(vocbase->name(), "testDataSource")));
    arangodb::velocypack::Builder response;
    v8::TryCatch tryCatch(isolate.get());
    auto result =
        v8::Function::Cast(*fn_grantCollection)
            ->CallAsFunction(context, arangoUsers,
                             static_cast<int>(grantWildcardArgs.size()),
                             grantWildcardArgs.data());
    EXPECT_FALSE(result.IsEmpty());
    EXPECT_TRUE(result.ToLocalChecked()->IsUndefined());
    EXPECT_FALSE(tryCatch.HasCaught());
    EXPECT_TRUE(
        (arangodb::auth::Level::RW ==
         execContext->collectionAuthLevel(vocbase->name(), "testDataSource")));
  }

  // test auth wildcard (revoke)
  {
    auto collectionJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testDataSource\" }");
    auto scopedUsers = std::shared_ptr<arangodb::LogicalCollection>(
        system->createCollection(usersJson->slice()).get(),
        [this](arangodb::LogicalCollection* ptr) -> void {
          system->dropCollection(ptr->id(), true);
        });
    arangodb::auth::UserMap userMap;
    arangodb::auth::User* userPtr = nullptr;
    userManager->setAuthInfo(userMap);  // insure an empty map is set before
                                        // UserManager::storeUser(...)
    userManager->storeUser(false, userName, arangodb::StaticStrings::Empty,
                           true, arangodb::velocypack::Slice());
    userManager->accessUser(
        userName,
        [&userPtr](arangodb::auth::User const& user) -> arangodb::Result {
          userPtr = const_cast<arangodb::auth::User*>(&user);
          return arangodb::Result();
        });
    ASSERT_NE(nullptr, userPtr);
    userPtr->grantCollection(
        vocbase->name(), "testDataSource",
        arangodb::auth::Level::RO);  // for missing collections
                                     // User::collectionAuthLevel(...) returns
                                     // database auth::Level
    auto logicalCollection = std::shared_ptr<arangodb::LogicalCollection>(
        vocbase->createCollection(collectionJson->slice()).get(),
        [vocbase](arangodb::LogicalCollection* ptr) -> void {
          vocbase->dropCollection(ptr->id(), false);
        });
    ASSERT_FALSE(!logicalCollection);

    EXPECT_TRUE(
        (arangodb::auth::Level::RO ==
         execContext->collectionAuthLevel(vocbase->name(), "testDataSource")));
    arangodb::velocypack::Builder response;
    v8::TryCatch tryCatch(isolate.get());
    auto result =
        v8::Function::Cast(*fn_revokeCollection)
            ->CallAsFunction(context, arangoUsers,
                             static_cast<int>(revokeWildcardArgs.size()),
                             revokeWildcardArgs.data());
    EXPECT_FALSE(result.IsEmpty());
    EXPECT_TRUE(result.ToLocalChecked()->IsUndefined());
    EXPECT_FALSE(tryCatch.HasCaught());
    EXPECT_TRUE(
        (arangodb::auth::Level::RO ==
         execContext->collectionAuthLevel(
             vocbase->name(),
             "testDataSource")));  // unchanged since revocation is only for
                                   // exactly matching collection names
  }
}
