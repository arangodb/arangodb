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


#include "Mocks/Servers.h"  // this must be first because windows

#include "gtest/gtest.h"

#include "velocypack/Builder.h"

#include "IResearch/common.h"
#include "Mocks/LogLevels.h"

#include "Aql/QueryRegistry.h"
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
#include "V8Server/v8-users.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/LogicalView.h"
#include "VocBase/vocbase.h"

#if USE_ENTERPRISE
#include "Enterprise/Ldap/LdapFeature.h"
#endif

// The following v8 headers must be included late, or MSVC fails to compile
// (error in mswsockdef.h), because V8's win32-headers.h #undef some macros like
// CONST and VOID. If, e.g., "Cluster/ClusterInfo.h" (which is in turn included
// here by "Sharding/ShardingFeature.h") is included after these, compilation
// fails. Another option than to include the following headers late is to
// include ClusterInfo.h before them.
// I have not dug into which header included by ClusterInfo.h will finally
// include mwsockdef.h. Nor did I check whether all of the following headers
// will include V8's "src/base/win32-headers.h".
#include "src/api/api.h"
// #include "src/objects-inl.h"
#include "src/objects/scope-info.h"

namespace {
class ArrayBufferAllocator : public v8::ArrayBuffer::Allocator {
 public:
  virtual void* Allocate(size_t length) override {
    void* data = AllocateUninitialized(length);
    return data == nullptr ? data : memset(data, 0, length);
  }
  virtual void* AllocateUninitialized(size_t length) override {
    return malloc(length);
  }
  virtual void Free(void* data, size_t) override { free(data); }
};

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

class V8UsersTest
    : public ::testing::Test,
      public arangodb::tests::LogSuppressor<arangodb::Logger::AUTHENTICATION, arangodb::LogLevel::ERR> {
 protected:
  arangodb::tests::mocks::MockAqlServer server;
  ViewFactory viewFactory;
  arangodb::SystemDatabaseFeature::ptr system;

  V8UsersTest()
      : server(), system(server.getFeature<arangodb::SystemDatabaseFeature>().use()) {
    arangodb::tests::v8Init();  // on-time initialize V8

    auto& viewTypesFeature = server.getFeature<arangodb::ViewTypesFeature>();
    viewTypesFeature.emplace(arangodb::LogicalDataSource::Type::emplace(arangodb::velocypack::StringRef(
                                 "testViewType")),
                             viewFactory);
  }
};

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

TEST_F(V8UsersTest, test_collection_auth) {
  auto usersJson = arangodb::velocypack::Parser::fromJson(
      "{ \"name\": \"_users\", \"isSystem\": true }");
  static const std::string userName("testUser");
  auto& databaseFeature = server.getFeature<arangodb::DatabaseFeature>();
  TRI_vocbase_t* vocbase;  // will be owned by DatabaseFeature
  ASSERT_TRUE(databaseFeature.createDatabase(testDBInfo(server.server()), vocbase).ok());
  v8::Isolate::CreateParams isolateParams;
  ArrayBufferAllocator arrayBufferAllocator;
  isolateParams.array_buffer_allocator = &arrayBufferAllocator;
  auto isolate =
      std::shared_ptr<v8::Isolate>(v8::Isolate::New(isolateParams),
                                   [](v8::Isolate* p) -> void { p->Dispose(); });
  ASSERT_NE(nullptr, isolate);
  v8::Isolate::Scope isolateScope(isolate.get());  // otherwise v8::Isolate::Logger() will fail (called from v8::Exception::Error)
  v8::internal::Isolate::Current()->InitializeLoggingAndCounters();  // otherwise v8::Isolate::Logger() will fail (called from v8::Exception::Error)
  v8::HandleScope handleScope(isolate.get());  // required for v8::Context::New(...), v8::ObjectTemplate::New(...) and TRI_AddMethodVocbase(...)
  auto context = v8::Context::New(isolate.get());
  v8::Context::Scope contextScope(context);  // required for TRI_AddMethodVocbase(...)
  std::unique_ptr<TRI_v8_global_t> v8g(TRI_CreateV8Globals(server.server(),
                                                           isolate.get(), 0));  // create and set inside 'isolate' for use with 'TRI_GET_GLOBALS()'
  v8g->ArangoErrorTempl.Reset(isolate.get(), v8::ObjectTemplate::New(isolate.get()));  // otherwise v8:-utils::CreateErrorObject(...) will fail
  v8g->_vocbase = vocbase;
  TRI_InitV8Users(context, vocbase, v8g.get(), isolate.get());
  auto arangoUsers =
      v8::Local<v8::ObjectTemplate>::New(isolate.get(), v8g->UsersTempl)->NewInstance(TRI_IGETC).FromMaybe(v8::Local<v8::Object>());
  auto fn_grantCollection =
    arangoUsers->Get(context, TRI_V8_ASCII_STRING(isolate.get(), "grantCollection")).FromMaybe(v8::Local<v8::Value>());
  EXPECT_TRUE(fn_grantCollection->IsFunction());
  auto fn_revokeCollection =
    arangoUsers->Get(context, TRI_V8_ASCII_STRING(isolate.get(), "revokeCollection")).FromMaybe(v8::Local<v8::Value>());
  EXPECT_TRUE(fn_revokeCollection->IsFunction());
  std::vector<v8::Local<v8::Value>> grantArgs = {
      TRI_V8_STD_STRING(isolate.get(), userName),
      TRI_V8_STD_STRING(isolate.get(), vocbase->name()),
      TRI_V8_ASCII_STRING(isolate.get(), "testDataSource"),
      TRI_V8_STD_STRING(isolate.get(),
                        arangodb::auth::convertFromAuthLevel(arangodb::auth::Level::RW)),
  };
  std::vector<v8::Local<v8::Value>> grantWildcardArgs = {
      TRI_V8_STD_STRING(isolate.get(), userName),
      TRI_V8_STD_STRING(isolate.get(), vocbase->name()),
      TRI_V8_ASCII_STRING(isolate.get(), "*"),
      TRI_V8_STD_STRING(isolate.get(),
                        arangodb::auth::convertFromAuthLevel(arangodb::auth::Level::RW)),
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
    arangodb::velocypack::Builder response;
    v8::TryCatch tryCatch(isolate.get());
    auto result = v8::Function::Cast(*fn_grantCollection)
                      ->CallAsFunction(context, arangoUsers,
                                       static_cast<int>(grantWildcardArgs.size()),
                                       grantWildcardArgs.data());
    EXPECT_FALSE(result.IsEmpty());
    EXPECT_TRUE(result.ToLocalChecked()->IsUndefined());
    EXPECT_FALSE(tryCatch.HasCaught());
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
    arangodb::velocypack::Builder response;
    v8::TryCatch tryCatch(isolate.get());
    auto result = v8::Function::Cast(*fn_revokeCollection)
                      ->CallAsFunction(context, arangoUsers,
                                       static_cast<int>(revokeWildcardArgs.size()),
                                       revokeWildcardArgs.data());
    EXPECT_FALSE(result.IsEmpty());
    EXPECT_TRUE(result.ToLocalChecked()->IsUndefined());
    EXPECT_FALSE(tryCatch.HasCaught());
    EXPECT_TRUE(
        (arangodb::auth::Level::RO ==
         execContext.collectionAuthLevel(vocbase->name(), "testDataSource")));  // unchanged since revocation is only for exactly matching collection names
  }
}
