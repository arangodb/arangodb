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

#include "src/api/api.h"  // must inclide V8 _before_ "catch.cpp' or CATCH() macro will be broken
// #include "src/objects-inl.h"  // (required to avoid compile warnings) must inclide V8 _before_ "catch.cpp' or CATCH() macro will be broken
#include "src/objects/scope-info.h"  // must inclide V8 _before_ "catch.cpp' or CATCH() macro will be broken

#include "gtest/gtest.h"

#include "velocypack/Parser.h"

#include "IResearch/common.h"
#include "Mocks/LogLevels.h"

#include "Aql/QueryRegistry.h"
#include "Basics/StaticStrings.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "Logger/LogTopic.h"
#include "Logger/Logger.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "RestServer/ViewTypesFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "Utils/ExecContext.h"
#include "V8/v8-utils.h"
#include "V8/v8-vpack.h"
#include "V8Server/v8-externals.h"
#include "V8Server/v8-views.h"
#include "VocBase/vocbase.h"

#if USE_ENTERPRISE
#include "Enterprise/Ldap/LdapFeature.h"
#endif

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
  virtual arangodb::Result dropImpl() override {
    return arangodb::LogicalViewHelperStorageEngine::drop(*this);
  }
  virtual void open() override {}
  virtual arangodb::Result renameImpl(std::string const& oldName) override {
    return arangodb::LogicalViewHelperStorageEngine::rename(*this, oldName);
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

v8::Local<v8::Object> getDbInstance(TRI_v8_global_t *v8g,
                                    v8::Isolate *isolate) {
  auto views = v8::ObjectTemplate::New(isolate);
  v8g->VocbaseViewTempl.Reset(isolate, views);
  auto db = v8::ObjectTemplate::New(isolate);
  v8g->VocbaseTempl.Reset(isolate, db);
  TRI_InitV8Views(*v8g, isolate);
  return v8::Local<v8::ObjectTemplate>::New(isolate, v8g->VocbaseTempl)->NewInstance(TRI_IGETC).FromMaybe(v8::Local<v8::Object>());
}

v8::Local<v8::Object> getViewInstance(TRI_v8_global_t *v8g,
                                      v8::Isolate *isolate) {
  auto views = v8::ObjectTemplate::New(isolate);
  v8g->VocbaseViewTempl.Reset(isolate, views);
  auto db = v8::ObjectTemplate::New(isolate);
  v8g->VocbaseTempl.Reset(isolate, db);
  TRI_InitV8Views(*v8g, isolate);
  return v8::Local<v8::ObjectTemplate>::New(isolate, v8g->VocbaseViewTempl)->NewInstance(TRI_IGETC).FromMaybe(v8::Local<v8::Object>());
}


v8::Local<v8::Function> getViewDBMemberFunction(TRI_v8_global_t *v8g,
                                                v8::Isolate *isolate,
                                                v8::Local<v8::Object> db,
                                                const char* name) {
  auto fn = db->Get(TRI_IGETC,
                    TRI_V8_ASCII_STRING(isolate, name)).FromMaybe(v8::Local<v8::Value>());
  EXPECT_TRUE(fn->IsFunction());
  return v8::Local<v8::Function>::Cast(fn);
}


v8::Local<v8::Function> getViewMethodFunction(TRI_v8_global_t *v8g,
                                              v8::Isolate *isolate,
                                              v8::Local<v8::Object>& arangoViewObj,
                                              const char* name) {

    auto fn = arangoViewObj->Get
      (TRI_IGETC,
       TRI_V8_ASCII_STRING(isolate, name)).FromMaybe(v8::Local<v8::Value>());
    EXPECT_TRUE(fn->IsFunction());
    return v8::Local<v8::Function>::Cast(fn);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

class V8ViewsTest
    : public ::testing::Test,
      public arangodb::tests::LogSuppressor<arangodb::Logger::AUTHENTICATION, arangodb::LogLevel::ERR> {
 protected:
  arangodb::tests::mocks::MockAqlServer server;
  ViewFactory viewFactory;

  V8ViewsTest() {
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

TEST_F(V8ViewsTest, test_auth) {
  // test create
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
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
    v8g->_vocbase = &vocbase;
    auto db = getDbInstance(v8g.get(), isolate.get());
    auto fn_createView = getViewDBMemberFunction(v8g.get(), isolate.get(), db, "_createView");

    std::vector<v8::Local<v8::Value>> args = {
        TRI_V8_ASCII_STRING(isolate.get(), "testView"),
        TRI_V8_ASCII_STRING(isolate.get(), "testViewType"),
        TRI_VPackToV8(isolate.get(),
                      arangodb::velocypack::Parser::fromJson("{}")->slice()),
    };

    EXPECT_TRUE(vocbase.views().empty());

    struct ExecContext : public arangodb::ExecContext {
      ExecContext()
          : arangodb::ExecContext(arangodb::ExecContext::Type::Default, "", "",
                                  arangodb::auth::Level::NONE,
                                  arangodb::auth::Level::NONE, false) {}
    } execContext;
    arangodb::ExecContextScope execContextScope(&execContext);
    auto* authFeature = arangodb::AuthenticationFeature::instance();
    auto* userManager = authFeature->userManager();
    
    auto resetUserManager = std::shared_ptr<arangodb::auth::UserManager>(
        userManager,
        [](arangodb::auth::UserManager* ptr) -> void { ptr->removeAllUsers(); });

    // not authorized (missing user)
    {
      arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
      userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database

      arangodb::velocypack::Builder response;
      v8::TryCatch tryCatch(isolate.get());
      auto result = v8::Function::Cast(*fn_createView)
                        ->CallAsFunction(context, fn_createView,
                                         static_cast<int>(args.size()), args.data());
      EXPECT_TRUE(result.IsEmpty());
      EXPECT_TRUE(tryCatch.HasCaught());
      TRI_V8ToVPack(isolate.get(), response, tryCatch.Exception(), false);
      auto slice = response.slice();
      EXPECT_TRUE(slice.isObject());
      EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::ErrorNum) &&
                   slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() &&
                   TRI_ERROR_FORBIDDEN ==
                       ErrorCode{slice.get(arangodb::StaticStrings::ErrorNum).getNumber<int>()}));
      EXPECT_TRUE(vocbase.views().empty());
    }

    // not authorized (RO user)
    {
      arangodb::auth::UserMap userMap;
      auto& user =
          userMap
              .emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP))
              .first->second;
      user.grantDatabase(vocbase.name(), arangodb::auth::Level::RO);
      userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database

      arangodb::velocypack::Builder response;
      v8::TryCatch tryCatch(isolate.get());
      auto result = v8::Function::Cast(*fn_createView)
                        ->CallAsFunction(context, fn_createView,
                                         static_cast<int>(args.size()), args.data());
      EXPECT_TRUE(result.IsEmpty());
      EXPECT_TRUE(tryCatch.HasCaught());
      TRI_V8ToVPack(isolate.get(), response, tryCatch.Exception(), false);
      auto slice = response.slice();
      EXPECT_TRUE(slice.isObject());
      EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::ErrorNum) &&
                   slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() &&
                   TRI_ERROR_FORBIDDEN ==
                       ErrorCode{slice.get(arangodb::StaticStrings::ErrorNum).getNumber<int>()}));
      EXPECT_TRUE(vocbase.views().empty());
    }

    // authorzed (RW user)
    {
      arangodb::auth::UserMap userMap;
      auto& user =
          userMap
              .emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP))
              .first->second;
      user.grantDatabase(vocbase.name(), arangodb::auth::Level::RW);
      userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database

      auto result = v8::Function::Cast(*fn_createView)
                        ->CallAsFunction(context, fn_createView,
                                         static_cast<int>(args.size()), args.data());
      EXPECT_FALSE(result.IsEmpty());
      EXPECT_TRUE(result.ToLocalChecked()->IsObject());
      auto* v8View = TRI_UnwrapClass<arangodb::LogicalView>(
          result.ToLocalChecked()->ToObject(TRI_IGETC).FromMaybe(v8::Local<v8::Object>()),
          WRP_VOCBASE_VIEW_TYPE, TRI_IGETC);
      EXPECT_FALSE(!v8View);
      EXPECT_EQ(std::string("testView"), v8View->name());
      EXPECT_EQ(std::string("testViewType"), v8View->type().name());
      auto view = vocbase.lookupView("testView");
      EXPECT_FALSE(!view);
    }
  }

  // test drop (static)
  {
    auto createViewJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testView\", \"type\": \"testViewType\" }");
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
    auto logicalView = vocbase.createView(createViewJson->slice());
    ASSERT_FALSE(!logicalView);

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
    v8g->_vocbase = &vocbase;
    auto db = getDbInstance(v8g.get(), isolate.get());
    auto fn_dropView = getViewDBMemberFunction(v8g.get(), isolate.get(), db, "_dropView");

    std::vector<v8::Local<v8::Value>> args = {
        TRI_V8_ASCII_STRING(isolate.get(), "testView"),
    };

    struct ExecContext : public arangodb::ExecContext {
      ExecContext()
          : arangodb::ExecContext(arangodb::ExecContext::Type::Default, "", "",
                                  arangodb::auth::Level::NONE,
                                  arangodb::auth::Level::NONE, false) {}
    } execContext;
    arangodb::ExecContextScope execContextScope(&execContext);
    auto* authFeature = arangodb::AuthenticationFeature::instance();
    auto* userManager = authFeature->userManager();
    
    auto resetUserManager = std::shared_ptr<arangodb::auth::UserManager>(
        userManager,
        [](arangodb::auth::UserManager* ptr) -> void { ptr->removeAllUsers(); });

    // not authorized (missing user)
    {
      arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
      userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database

      arangodb::velocypack::Builder response;
      v8::TryCatch tryCatch(isolate.get());
      auto result = v8::Function::Cast(*fn_dropView)
                        ->CallAsFunction(context, fn_dropView,
                                         static_cast<int>(args.size()), args.data());
      EXPECT_TRUE(result.IsEmpty());
      EXPECT_TRUE(tryCatch.HasCaught());
      TRI_V8ToVPack(isolate.get(), response, tryCatch.Exception(), false);
      auto slice = response.slice();
      EXPECT_TRUE(slice.isObject());
      EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::ErrorNum) &&
                   slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() &&
                   TRI_ERROR_FORBIDDEN ==
                       ErrorCode{slice.get(arangodb::StaticStrings::ErrorNum).getNumber<int>()}));
      auto view = vocbase.lookupView("testView");
      EXPECT_FALSE(!view);
    }

    // not authorized (RO user database)
    {
      arangodb::auth::UserMap userMap;
      auto& user =
          userMap
              .emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP))
              .first->second;
      user.grantDatabase(vocbase.name(), arangodb::auth::Level::RO);
      userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database

      arangodb::velocypack::Builder response;
      v8::TryCatch tryCatch(isolate.get());
      auto result = v8::Function::Cast(*fn_dropView)
                        ->CallAsFunction(context, fn_dropView,
                                         static_cast<int>(args.size()), args.data());
      EXPECT_TRUE(result.IsEmpty());
      EXPECT_TRUE(tryCatch.HasCaught());
      TRI_V8ToVPack(isolate.get(), response, tryCatch.Exception(), false);
      auto slice = response.slice();
      EXPECT_TRUE(slice.isObject());
      EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::ErrorNum) &&
                   slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() &&
                   TRI_ERROR_FORBIDDEN ==
                       ErrorCode{slice.get(arangodb::StaticStrings::ErrorNum).getNumber<int>()}));
      auto view = vocbase.lookupView("testView");
      EXPECT_FALSE(!view);
    }

    // authorized (NONE user view) as per https://github.com/arangodb/backlog/issues/459
    {
      arangodb::auth::UserMap userMap;
      auto& user =
          userMap
              .emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP))
              .first->second;
      user.grantDatabase(vocbase.name(), arangodb::auth::Level::RW);
      user.grantCollection(vocbase.name(), "testView", arangodb::auth::Level::NONE);
      userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database

      auto result = v8::Function::Cast(*fn_dropView)
                        ->CallAsFunction(context, fn_dropView,
                                         static_cast<int>(args.size()), args.data());
      EXPECT_FALSE(result.IsEmpty());
      EXPECT_TRUE(result.ToLocalChecked()->IsUndefined());
      EXPECT_TRUE(vocbase.views().empty());
    }
  }

  // test drop (instance)
  {
    auto createViewJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testView\", \"type\": \"testViewType\" }");
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
    auto logicalView = vocbase.createView(createViewJson->slice());
    ASSERT_FALSE(!logicalView);

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
    v8g->_vocbase = &vocbase;

    auto arangoView = getViewInstance(v8g.get(), isolate.get());
    auto fn_drop = getViewMethodFunction(v8g.get(), isolate.get(), arangoView, "drop");

    arangoView->SetInternalField(SLOT_CLASS_TYPE,
                                 v8::Integer::New(isolate.get(), WRP_VOCBASE_VIEW_TYPE));
    arangoView->SetInternalField(SLOT_CLASS,
                                 v8::External::New(isolate.get(), logicalView.get()));
    std::vector<v8::Local<v8::Value>> args = {};

    struct ExecContext : public arangodb::ExecContext {
      ExecContext()
          : arangodb::ExecContext(arangodb::ExecContext::Type::Default, "", "",
                                  arangodb::auth::Level::NONE,
                                  arangodb::auth::Level::NONE, false) {}
    } execContext;
    arangodb::ExecContextScope execContextScope(&execContext);
    auto* authFeature = arangodb::AuthenticationFeature::instance();
    auto* userManager = authFeature->userManager();
    
    auto resetUserManager = std::shared_ptr<arangodb::auth::UserManager>(
        userManager,
        [](arangodb::auth::UserManager* ptr) -> void { ptr->removeAllUsers(); });

    // not authorized (missing user)
    {
      arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
      userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database

      arangodb::velocypack::Builder response;
      v8::TryCatch tryCatch(isolate.get());
      auto result =
          v8::Function::Cast(*fn_drop)->CallAsFunction(context, arangoView,
                                                       static_cast<int>(args.size()),
                                                       args.data());
      EXPECT_TRUE(result.IsEmpty());
      EXPECT_TRUE(tryCatch.HasCaught());
      TRI_V8ToVPack(isolate.get(), response, tryCatch.Exception(), false);
      auto slice = response.slice();
      EXPECT_TRUE(slice.isObject());
      EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::ErrorNum) &&
                   slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() &&
                   TRI_ERROR_FORBIDDEN ==
                       ErrorCode{slice.get(arangodb::StaticStrings::ErrorNum).getNumber<int>()}));
      auto view = vocbase.lookupView("testView");
      EXPECT_FALSE(!view);
    }

    // not authorized (RO user database)
    {
      arangodb::auth::UserMap userMap;
      auto& user =
          userMap
              .emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP))
              .first->second;
      user.grantDatabase(vocbase.name(), arangodb::auth::Level::RO);
      userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database

      arangodb::velocypack::Builder response;
      v8::TryCatch tryCatch(isolate.get());
      auto result =
          v8::Function::Cast(*fn_drop)->CallAsFunction(context, arangoView,
                                                       static_cast<int>(args.size()),
                                                       args.data());
      EXPECT_TRUE(result.IsEmpty());
      EXPECT_TRUE(tryCatch.HasCaught());
      TRI_V8ToVPack(isolate.get(), response, tryCatch.Exception(), false);
      auto slice = response.slice();
      EXPECT_TRUE(slice.isObject());
      EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::ErrorNum) &&
                   slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() &&
                   TRI_ERROR_FORBIDDEN ==
                       ErrorCode{slice.get(arangodb::StaticStrings::ErrorNum).getNumber<int>()}));
      auto view = vocbase.lookupView("testView");
      EXPECT_FALSE(!view);
    }

    // authorized (NONE user view) as per https://github.com/arangodb/backlog/issues/459
    {
      arangodb::auth::UserMap userMap;
      auto& user =
          userMap
              .emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP))
              .first->second;
      user.grantDatabase(vocbase.name(), arangodb::auth::Level::RW);
      user.grantCollection(vocbase.name(), "testView", arangodb::auth::Level::NONE);
      userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database

      auto result =
          v8::Function::Cast(*fn_drop)->CallAsFunction(context, arangoView,
                                                       static_cast<int>(args.size()),
                                                       args.data());
      EXPECT_FALSE(result.IsEmpty());
      EXPECT_TRUE(result.ToLocalChecked()->IsUndefined());
      EXPECT_TRUE(vocbase.views().empty());
    }
  }

  // test rename
  {
    auto createViewJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testView\", \"type\": \"testViewType\" }");
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
    auto logicalView = vocbase.createView(createViewJson->slice());
    ASSERT_FALSE(!logicalView);

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
    v8g->_vocbase = &vocbase;
    auto arangoView = getViewInstance(v8g.get(), isolate.get());
    auto fn_rename = getViewMethodFunction(v8g.get(), isolate.get(), arangoView, "rename");

    arangoView->SetInternalField(SLOT_CLASS_TYPE,
                                 v8::Integer::New(isolate.get(), WRP_VOCBASE_VIEW_TYPE));
    arangoView->SetInternalField(SLOT_CLASS,
                                 v8::External::New(isolate.get(), logicalView.get()));
    std::vector<v8::Local<v8::Value>> args = {
        TRI_V8_ASCII_STRING(isolate.get(), "testView1"),
    };

    struct ExecContext : public arangodb::ExecContext {
      ExecContext()
          : arangodb::ExecContext(arangodb::ExecContext::Type::Default, "", "",
                                  arangodb::auth::Level::NONE,
                                  arangodb::auth::Level::NONE, false) {}
    } execContext;
    arangodb::ExecContextScope execContextScope(&execContext);
    auto* authFeature = arangodb::AuthenticationFeature::instance();
    auto* userManager = authFeature->userManager();
    
    auto resetUserManager = std::shared_ptr<arangodb::auth::UserManager>(
        userManager,
        [](arangodb::auth::UserManager* ptr) -> void { ptr->removeAllUsers(); });

    // not authorized (missing user)
    {
      arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
      userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database

      arangodb::velocypack::Builder response;
      v8::TryCatch tryCatch(isolate.get());
      auto result = v8::Function::Cast(*fn_rename)
                        ->CallAsFunction(context, arangoView,
                                         static_cast<int>(args.size()), args.data());
      EXPECT_TRUE(result.IsEmpty());
      EXPECT_TRUE(tryCatch.HasCaught());
      TRI_V8ToVPack(isolate.get(), response, tryCatch.Exception(), false);
      auto slice = response.slice();
      EXPECT_TRUE(slice.isObject());
      EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::ErrorNum) &&
                   slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() &&
                   TRI_ERROR_FORBIDDEN ==
                       ErrorCode{slice.get(arangodb::StaticStrings::ErrorNum).getNumber<int>()}));
      auto view = vocbase.lookupView("testView");
      EXPECT_FALSE(!view);
      auto view1 = vocbase.lookupView("testView1");
      EXPECT_FALSE(view1);
    }

    // not authorized (RO user database)
    {
      arangodb::auth::UserMap userMap;
      auto& user =
          userMap
              .emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP))
              .first->second;
      user.grantDatabase(vocbase.name(), arangodb::auth::Level::RO);
      userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database

      arangodb::velocypack::Builder response;
      v8::TryCatch tryCatch(isolate.get());
      auto result = v8::Function::Cast(*fn_rename)
                        ->CallAsFunction(context, arangoView,
                                         static_cast<int>(args.size()), args.data());
      EXPECT_TRUE(result.IsEmpty());
      EXPECT_TRUE(tryCatch.HasCaught());
      TRI_V8ToVPack(isolate.get(), response, tryCatch.Exception(), false);
      auto slice = response.slice();
      EXPECT_TRUE(slice.isObject());
      EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::ErrorNum) &&
                   slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() &&
                   TRI_ERROR_FORBIDDEN ==
                       ErrorCode{slice.get(arangodb::StaticStrings::ErrorNum).getNumber<int>()}));
      auto view = vocbase.lookupView("testView");
      EXPECT_FALSE(!view);
      auto view1 = vocbase.lookupView("testView1");
      EXPECT_FALSE(view1);
    }

    // not authorized (NONE user view with failing toVelocyPack()) as per https://github.com/arangodb/backlog/issues/459
    {
      arangodb::auth::UserMap userMap;
      auto& user =
          userMap
              .emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP))
              .first->second;
      user.grantDatabase(vocbase.name(), arangodb::auth::Level::RW);
      user.grantCollection(vocbase.name(), "testView", arangodb::auth::Level::NONE);
      userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database
      auto* testView = arangodb::LogicalView::cast<TestView>(logicalView.get());
      testView->_appendVelocyPackResult = arangodb::Result(TRI_ERROR_FORBIDDEN);
      auto resetAppendVelocyPackResult =
          std::shared_ptr<TestView>(testView, [](TestView* p) -> void {
            p->_appendVelocyPackResult = arangodb::Result();
          });

      arangodb::velocypack::Builder response;
      v8::TryCatch tryCatch(isolate.get());
      auto result = v8::Function::Cast(*fn_rename)
                        ->CallAsFunction(context, arangoView,
                                         static_cast<int>(args.size()), args.data());
      EXPECT_TRUE(result.IsEmpty());
      EXPECT_TRUE(tryCatch.HasCaught());
      TRI_V8ToVPack(isolate.get(), response, tryCatch.Exception(), false);
      auto slice = response.slice();
      EXPECT_TRUE(slice.isObject());
      EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::ErrorNum) &&
                   slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() &&
                   TRI_ERROR_FORBIDDEN ==
                       ErrorCode{slice.get(arangodb::StaticStrings::ErrorNum).getNumber<int>()}));
      auto view = vocbase.lookupView("testView");
      EXPECT_FALSE(!view);
      auto view1 = vocbase.lookupView("testView1");
      EXPECT_FALSE(view1);
    }

    // authorized (NONE user view) as per https://github.com/arangodb/backlog/issues/459
    {
      arangodb::auth::UserMap userMap;
      auto& user =
          userMap
              .emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP))
              .first->second;
      user.grantDatabase(vocbase.name(), arangodb::auth::Level::RW);
      user.grantCollection(vocbase.name(), "testView", arangodb::auth::Level::NONE);
      userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database

      auto result = v8::Function::Cast(*fn_rename)
                        ->CallAsFunction(context, arangoView,
                                         static_cast<int>(args.size()), args.data());
      EXPECT_FALSE(result.IsEmpty());
      EXPECT_TRUE(result.ToLocalChecked()->IsUndefined());
      auto view = vocbase.lookupView("testView");
      EXPECT_FALSE(view);
      auto view1 = vocbase.lookupView("testView1");
      EXPECT_FALSE(!view1);
    }
  }

  // test modify
  {
    auto createViewJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testView\", \"type\": \"testViewType\" }");
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
    auto logicalView = vocbase.createView(createViewJson->slice());
    ASSERT_FALSE(!logicalView);

    v8::Isolate::CreateParams isolateParams;
    ArrayBufferAllocator arrayBufferAllocator;
    isolateParams.array_buffer_allocator = &arrayBufferAllocator;
    auto isolate =
        std::shared_ptr<v8::Isolate>(v8::Isolate::New(isolateParams),
                                     [](v8::Isolate* p) -> void { p->Dispose(); });
    ASSERT_NE(nullptr, isolate);
    char isolateData[64];  // 64 > sizeof(arangodb::V8PlatformFeature::IsolateData)
    std::memset(isolateData, 0, 64);  // otherwise arangodb::V8PlatformFeature::isOutOfMemory(isolate) returns true
    isolate->SetData(arangodb::V8PlatformFeature::V8_INFO,
                     isolateData);  // required for TRI_VPackToV8(...) with nn-empty jSON
    v8::Isolate::Scope isolateScope(isolate.get());  // otherwise v8::Isolate::Logger() will fail (called from v8::Exception::Error)
    v8::internal::Isolate::Current()->InitializeLoggingAndCounters();  // otherwise v8::Isolate::Logger() will fail (called from v8::Exception::Error)
    v8::HandleScope handleScope(isolate.get());  // required for v8::Context::New(...), v8::ObjectTemplate::New(...) and TRI_AddMethodVocbase(...)
    auto context = v8::Context::New(isolate.get());
    v8::Context::Scope contextScope(context);  // required for TRI_AddMethodVocbase(...)
    std::unique_ptr<TRI_v8_global_t> v8g(TRI_CreateV8Globals(server.server(),
                                                             isolate.get(), 0));  // create and set inside 'isolate' for use with 'TRI_GET_GLOBALS()'
    v8g->ArangoErrorTempl.Reset(isolate.get(), v8::ObjectTemplate::New(isolate.get()));  // otherwise v8:-utils::CreateErrorObject(...) will fail
    v8g->_vocbase = &vocbase;
    auto arangoView = getViewInstance(v8g.get(), isolate.get());
    auto fn_properties = getViewMethodFunction(v8g.get(), isolate.get(), arangoView, "properties");

    arangoView->SetInternalField(SLOT_CLASS_TYPE,
                                 v8::Integer::New(isolate.get(), WRP_VOCBASE_VIEW_TYPE));
    arangoView->SetInternalField(SLOT_CLASS,
                                 v8::External::New(isolate.get(), logicalView.get()));
    std::vector<v8::Local<v8::Value>> args = {
        TRI_VPackToV8(
            isolate.get(),
            arangodb::velocypack::Parser::fromJson("{ \"key\": \"value\" }")->slice()),
    };

    struct ExecContext : public arangodb::ExecContext {
      ExecContext()
          : arangodb::ExecContext(arangodb::ExecContext::Type::Default, "", "",
                                  arangodb::auth::Level::NONE,
                                  arangodb::auth::Level::NONE, false) {}
    } execContext;
    arangodb::ExecContextScope execContextScope(&execContext);
    auto* authFeature = arangodb::AuthenticationFeature::instance();
    auto* userManager = authFeature->userManager();
    
    auto resetUserManager = std::shared_ptr<arangodb::auth::UserManager>(
        userManager,
        [](arangodb::auth::UserManager* ptr) -> void { ptr->removeAllUsers(); });

    // not authorized (missing user)
    {
      arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
      userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database

      arangodb::velocypack::Builder response;
      v8::TryCatch tryCatch(isolate.get());
      auto result = v8::Function::Cast(*fn_properties)
                        ->CallAsFunction(context, arangoView,
                                         static_cast<int>(args.size()), args.data());
      EXPECT_TRUE(result.IsEmpty());
      EXPECT_TRUE(tryCatch.HasCaught());
      TRI_V8ToVPack(isolate.get(), response, tryCatch.Exception(), false);
      auto slice = response.slice();
      EXPECT_TRUE(slice.isObject());
      EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::ErrorNum) &&
                   slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() &&
                   TRI_ERROR_FORBIDDEN ==
                       ErrorCode{slice.get(arangodb::StaticStrings::ErrorNum).getNumber<int>()}));
      auto view = vocbase.lookupView("testView");
      EXPECT_FALSE(!view);
    }

    // not authorized (RO user database)
    {
      arangodb::auth::UserMap userMap;
      auto& user =
          userMap
              .emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP))
              .first->second;
      user.grantDatabase(vocbase.name(), arangodb::auth::Level::RO);
      userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database

      arangodb::velocypack::Builder response;
      v8::TryCatch tryCatch(isolate.get());
      auto result = v8::Function::Cast(*fn_properties)
                        ->CallAsFunction(context, arangoView,
                                         static_cast<int>(args.size()), args.data());
      EXPECT_TRUE(result.IsEmpty());
      EXPECT_TRUE(tryCatch.HasCaught());
      TRI_V8ToVPack(isolate.get(), response, tryCatch.Exception(), false);
      auto slice = response.slice();
      EXPECT_TRUE(slice.isObject());
      EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::ErrorNum) &&
                   slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() &&
                   TRI_ERROR_FORBIDDEN ==
                       ErrorCode{slice.get(arangodb::StaticStrings::ErrorNum).getNumber<int>()}));
      auto view = vocbase.lookupView("testView");
      EXPECT_FALSE(!view);
    }

    // not authorized (NONE user view with failing toVelocyPack()) as per https://github.com/arangodb/backlog/issues/459
    {
      arangodb::auth::UserMap userMap;
      auto& user =
          userMap
              .emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP))
              .first->second;
      user.grantDatabase(vocbase.name(), arangodb::auth::Level::RW);
      user.grantCollection(vocbase.name(), "testView", arangodb::auth::Level::NONE);
      userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database
      auto* testView = arangodb::LogicalView::cast<TestView>(logicalView.get());
      testView->_appendVelocyPackResult = arangodb::Result(TRI_ERROR_INTERNAL);
      auto resetAppendVelocyPackResult =
          std::shared_ptr<TestView>(testView, [](TestView* p) -> void {
            p->_appendVelocyPackResult = arangodb::Result();
          });

      arangodb::velocypack::Builder response;
      v8::TryCatch tryCatch(isolate.get());
      auto result = v8::Function::Cast(*fn_properties)
                        ->CallAsFunction(context, arangoView,
                                         static_cast<int>(args.size()), args.data());
      EXPECT_TRUE(result.IsEmpty());
      EXPECT_TRUE(tryCatch.HasCaught());
      TRI_V8ToVPack(isolate.get(), response, tryCatch.Exception(), false);
      auto slice = response.slice();
      EXPECT_TRUE(slice.isObject());
      EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::ErrorNum) &&
                   slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() &&
                   TRI_ERROR_INTERNAL ==
                       ErrorCode{slice.get(arangodb::StaticStrings::ErrorNum).getNumber<int>()}));
      auto view = vocbase.lookupView("testView");
      EXPECT_FALSE(!view);
      slice = arangodb::LogicalView::cast<TestView>(*view)._properties.slice();
      EXPECT_FALSE(slice.isObject());
    }

    // authorized (NONE user view) as per https://github.com/arangodb/backlog/issues/459
    {
      arangodb::auth::UserMap userMap;
      auto& user =
          userMap
              .emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP))
              .first->second;
      user.grantDatabase(vocbase.name(), arangodb::auth::Level::RW);
      user.grantCollection(vocbase.name(), "testView", arangodb::auth::Level::NONE);
      userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database

      arangodb::velocypack::Builder response;
      auto result = v8::Function::Cast(*fn_properties)
                        ->CallAsFunction(context, arangoView,
                                         static_cast<int>(args.size()), args.data());
      EXPECT_FALSE(result.IsEmpty());
      EXPECT_TRUE(result.ToLocalChecked()->IsObject());
      TRI_V8ToVPack(isolate.get(), response, result.ToLocalChecked(), false);
      auto slice = response.slice();
      EXPECT_TRUE(slice.isObject());
      EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::DataSourceName) &&
                   slice.get(arangodb::StaticStrings::DataSourceName).isString() &&
                   std::string("testView") ==
                       slice.get(arangodb::StaticStrings::DataSourceName).copyString()));
      EXPECT_TRUE((slice.hasKey("properties") && slice.get("properties").isObject() &&
                   slice.get("properties").hasKey("key") &&
                   slice.get("properties").get("key").isString() &&
                   std::string("value") == slice.get("properties").get("key").copyString()));
      auto view = vocbase.lookupView("testView");
      EXPECT_FALSE(!view);
      slice = arangodb::LogicalView::cast<TestView>(*view)._properties.slice();
      EXPECT_TRUE(slice.isObject());
      EXPECT_TRUE((slice.hasKey("key") && slice.get("key").isString() &&
                   std::string("value") == slice.get("key").copyString()));
    }
  }

  // test get view (basic)
  {
    auto createViewJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testView\", \"type\": \"testViewType\" }");
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
    auto logicalView = vocbase.createView(createViewJson->slice());
    ASSERT_FALSE(!logicalView);

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
    v8g->_vocbase = &vocbase;
    auto db = getDbInstance(v8g.get(), isolate.get());
    auto fn_view = getViewDBMemberFunction(v8g.get(), isolate.get(), db, "_view");

    std::vector<v8::Local<v8::Value>> args = {
        TRI_V8_ASCII_STRING(isolate.get(), "testView"),
    };

    struct ExecContext : public arangodb::ExecContext {
      ExecContext()
          : arangodb::ExecContext(arangodb::ExecContext::Type::Default, "", "",
                                  arangodb::auth::Level::NONE,
                                  arangodb::auth::Level::NONE, false) {}
    } execContext;
    arangodb::ExecContextScope execContextScope(&execContext);
    auto* authFeature = arangodb::AuthenticationFeature::instance();
    auto* userManager = authFeature->userManager();
    
    auto resetUserManager = std::shared_ptr<arangodb::auth::UserManager>(
        userManager,
        [](arangodb::auth::UserManager* ptr) -> void { ptr->removeAllUsers(); });

    // not authorized (missing user)
    {
      arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
      userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database

      arangodb::velocypack::Builder response;
      v8::TryCatch tryCatch(isolate.get());
      auto result =
          v8::Function::Cast(*fn_view)->CallAsFunction(context, fn_view,
                                                       static_cast<int>(args.size()),
                                                       args.data());
      EXPECT_TRUE(result.IsEmpty());
      EXPECT_TRUE(tryCatch.HasCaught());
      TRI_V8ToVPack(isolate.get(), response, tryCatch.Exception(), false);
      auto slice = response.slice();
      EXPECT_TRUE(slice.isObject());
      EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::ErrorNum) &&
                   slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() &&
                   TRI_ERROR_FORBIDDEN ==
                       ErrorCode{slice.get(arangodb::StaticStrings::ErrorNum).getNumber<int>()}));
      auto view = vocbase.lookupView("testView");
      EXPECT_FALSE(!view);
    }

    // not authorized (failed detailed toVelocyPack(...)) as per https://github.com/arangodb/backlog/issues/459
    {
      arangodb::auth::UserMap userMap;
      auto& user =
          userMap
              .emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP))
              .first->second;
      user.grantDatabase(vocbase.name(), arangodb::auth::Level::RO);
      user.grantCollection(vocbase.name(), "testView", arangodb::auth::Level::NONE);  // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database
      auto* testView = arangodb::LogicalView::cast<TestView>(logicalView.get());
      testView->_appendVelocyPackResult = arangodb::Result(TRI_ERROR_FORBIDDEN);
      auto resetAppendVelocyPackResult =
          std::shared_ptr<TestView>(testView, [](TestView* p) -> void {
            p->_appendVelocyPackResult = arangodb::Result();
          });

      arangodb::velocypack::Builder response;
      v8::TryCatch tryCatch(isolate.get());
      auto result =
          v8::Function::Cast(*fn_view)->CallAsFunction(context, fn_view,
                                                       static_cast<int>(args.size()),
                                                       args.data());
      EXPECT_TRUE(result.IsEmpty());
      EXPECT_TRUE(tryCatch.HasCaught());
      TRI_V8ToVPack(isolate.get(), response, tryCatch.Exception(), false);
      auto slice = response.slice();
      EXPECT_TRUE(slice.isObject());
      EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::ErrorNum) &&
                   slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() &&
                   TRI_ERROR_FORBIDDEN ==
                       ErrorCode{slice.get(arangodb::StaticStrings::ErrorNum).getNumber<int>()}));
      auto view = vocbase.lookupView("testView");
      EXPECT_FALSE(!view);
    }

    // authorized (NONE view) as per https://github.com/arangodb/backlog/issues/459
    {
      arangodb::auth::UserMap userMap;
      auto& user =
          userMap
              .emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP))
              .first->second;
      user.grantDatabase(vocbase.name(), arangodb::auth::Level::RO);
      user.grantCollection(vocbase.name(), "testView", arangodb::auth::Level::NONE);  // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database

      auto result =
          v8::Function::Cast(*fn_view)->CallAsFunction(context, fn_view,
                                                       static_cast<int>(args.size()),
                                                       args.data());
      EXPECT_FALSE(result.IsEmpty());
      EXPECT_TRUE(result.ToLocalChecked()->IsObject());
      auto* v8View = TRI_UnwrapClass<arangodb::LogicalView>(
          result.ToLocalChecked()->ToObject(TRI_IGETC).FromMaybe(v8::Local<v8::Object>()),
          WRP_VOCBASE_VIEW_TYPE, TRI_IGETC);
      EXPECT_FALSE(!v8View);
      EXPECT_EQ(std::string("testView"), v8View->name());
      EXPECT_EQ(std::string("testViewType"), v8View->type().name());
      auto view = vocbase.lookupView("testView");
      EXPECT_FALSE(!view);
    }
  }

  // test get view (detailed)
  {
    auto createViewJson = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testView\", \"type\": \"testViewType\" }");
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
    auto logicalView = vocbase.createView(createViewJson->slice());
    ASSERT_FALSE(!logicalView);

    v8::Isolate::CreateParams isolateParams;
    ArrayBufferAllocator arrayBufferAllocator;
    isolateParams.array_buffer_allocator = &arrayBufferAllocator;
    auto isolate =
        std::shared_ptr<v8::Isolate>(v8::Isolate::New(isolateParams),
                                     [](v8::Isolate* p) -> void { p->Dispose(); });
    ASSERT_NE(nullptr, isolate);
    char isolateData[64];  // 64 > sizeof(arangodb::V8PlatformFeature::IsolateData)
    std::memset(isolateData, 0, 64);  // otherwise arangodb::V8PlatformFeature::isOutOfMemory(isolate) returns true
    isolate->SetData(arangodb::V8PlatformFeature::V8_INFO,
                     isolateData);  // required for TRI_VPackToV8(...) with nn-empty jSON
    v8::Isolate::Scope isolateScope(isolate.get());  // otherwise v8::Isolate::Logger() will fail (called from v8::Exception::Error)
    v8::internal::Isolate::Current()->InitializeLoggingAndCounters();  // otherwise v8::Isolate::Logger() will fail (called from v8::Exception::Error)
    v8::HandleScope handleScope(isolate.get());  // required for v8::Context::New(...), v8::ObjectTemplate::New(...) and TRI_AddMethodVocbase(...)
    auto context = v8::Context::New(isolate.get());
    v8::Context::Scope contextScope(context);  // required for TRI_AddMethodVocbase(...)
    std::unique_ptr<TRI_v8_global_t> v8g(TRI_CreateV8Globals(server.server(),
                                                             isolate.get(), 0));  // create and set inside 'isolate' for use with 'TRI_GET_GLOBALS()'
    v8g->ArangoErrorTempl.Reset(isolate.get(), v8::ObjectTemplate::New(isolate.get()));  // otherwise v8:-utils::CreateErrorObject(...) will fail
    v8g->_vocbase = &vocbase;

    auto arangoView = getViewInstance(v8g.get(), isolate.get());
    auto fn_properties = getViewMethodFunction(v8g.get(), isolate.get(), arangoView, "properties");

    arangoView->SetInternalField(SLOT_CLASS_TYPE,
                                 v8::Integer::New(isolate.get(), WRP_VOCBASE_VIEW_TYPE));
    arangoView->SetInternalField(SLOT_CLASS,
                                 v8::External::New(isolate.get(), logicalView.get()));
    std::vector<v8::Local<v8::Value>> args = {};

    struct ExecContext : public arangodb::ExecContext {
      ExecContext()
          : arangodb::ExecContext(arangodb::ExecContext::Type::Default, "", "",
                                  arangodb::auth::Level::NONE,
                                  arangodb::auth::Level::NONE, false) {}
    } execContext;
    arangodb::ExecContextScope execContextScope(&execContext);
    auto* authFeature = arangodb::AuthenticationFeature::instance();
    auto* userManager = authFeature->userManager();
    
    auto resetUserManager = std::shared_ptr<arangodb::auth::UserManager>(
        userManager,
        [](arangodb::auth::UserManager* ptr) -> void { ptr->removeAllUsers(); });

    // not authorized (missing user)
    {
      arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
      userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database

      arangodb::velocypack::Builder response;
      v8::TryCatch tryCatch(isolate.get());
      auto result = v8::Function::Cast(*fn_properties)
                        ->CallAsFunction(context, arangoView,
                                         static_cast<int>(args.size()), args.data());
      EXPECT_TRUE(result.IsEmpty());
      EXPECT_TRUE(tryCatch.HasCaught());
      TRI_V8ToVPack(isolate.get(), response, tryCatch.Exception(), false);
      auto slice = response.slice();
      EXPECT_TRUE(slice.isObject());
      EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::ErrorNum) &&
                   slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() &&
                   TRI_ERROR_FORBIDDEN ==
                       ErrorCode{slice.get(arangodb::StaticStrings::ErrorNum).getNumber<int>()}));
      auto view = vocbase.lookupView("testView");
      EXPECT_FALSE(!view);
    }

    // not authorized (failed detailed toVelocyPack(...))
    {
      arangodb::auth::UserMap userMap;
      auto& user =
          userMap
              .emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP))
              .first->second;
      user.grantDatabase(vocbase.name(), arangodb::auth::Level::RO);
      user.grantCollection(vocbase.name(), "testView", arangodb::auth::Level::NONE);  // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database
      auto* testView = arangodb::LogicalView::cast<TestView>(logicalView.get());
      testView->_appendVelocyPackResult = arangodb::Result(TRI_ERROR_FORBIDDEN);
      auto resetAppendVelocyPackResult =
          std::shared_ptr<TestView>(testView, [](TestView* p) -> void {
            p->_appendVelocyPackResult = arangodb::Result();
          });

      arangodb::velocypack::Builder response;
      v8::TryCatch tryCatch(isolate.get());
      auto result = v8::Function::Cast(*fn_properties)
                        ->CallAsFunction(context, arangoView,
                                         static_cast<int>(args.size()), args.data());
      EXPECT_TRUE(result.IsEmpty());
      EXPECT_TRUE(tryCatch.HasCaught());
      TRI_V8ToVPack(isolate.get(), response, tryCatch.Exception(), false);
      auto slice = response.slice();
      EXPECT_TRUE(slice.isObject());
      EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::ErrorNum) &&
                   slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() &&
                   TRI_ERROR_FORBIDDEN ==
                       ErrorCode{slice.get(arangodb::StaticStrings::ErrorNum).getNumber<int>()}));
      auto view = vocbase.lookupView("testView");
      EXPECT_FALSE(!view);
    }

    // authorized (NONE view) as per https://github.com/arangodb/backlog/issues/459
    {
      arangodb::auth::UserMap userMap;
      auto& user =
          userMap
              .emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP))
              .first->second;
      user.grantDatabase(vocbase.name(), arangodb::auth::Level::RO);
      user.grantCollection(vocbase.name(), "testView", arangodb::auth::Level::NONE);  // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database

      arangodb::velocypack::Builder response;
      auto result = v8::Function::Cast(*fn_properties)
                        ->CallAsFunction(context, arangoView,
                                         static_cast<int>(args.size()), args.data());
      EXPECT_FALSE(result.IsEmpty());
      EXPECT_TRUE(result.ToLocalChecked()->IsObject());
      TRI_V8ToVPack(isolate.get(), response, result.ToLocalChecked(), false);
      auto slice = response.slice();
      EXPECT_TRUE(slice.isObject());
      EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::DataSourceName) &&
                   slice.get(arangodb::StaticStrings::DataSourceName).isString() &&
                   std::string("testView") ==
                       slice.get(arangodb::StaticStrings::DataSourceName).copyString()));
      auto view = vocbase.lookupView("testView");
      EXPECT_FALSE(!view);
    }
  }

  // test get all views
  {
    auto createView1Json = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testView1\", \"type\": \"testViewType\" }");
    auto createView2Json = arangodb::velocypack::Parser::fromJson(
        "{ \"name\": \"testView2\", \"type\": \"testViewType\" }");
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, testDBInfo(server.server()));
    auto logicalView1 = vocbase.createView(createView1Json->slice());
    ASSERT_FALSE(!logicalView1);
    auto logicalView2 = vocbase.createView(createView2Json->slice());
    ASSERT_FALSE(!logicalView2);

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
    v8g->_vocbase = &vocbase;
    auto db = getDbInstance(v8g.get(), isolate.get());
    auto fn_views = getViewDBMemberFunction(v8g.get(), isolate.get(), db, "_views");

    std::vector<v8::Local<v8::Value>> args = {};

    struct ExecContext : public arangodb::ExecContext {
      ExecContext()
          : arangodb::ExecContext(arangodb::ExecContext::Type::Default, "", "",
                                  arangodb::auth::Level::NONE,
                                  arangodb::auth::Level::NONE, false) {}
    } execContext;
    arangodb::ExecContextScope execContextScope(&execContext);
    auto* authFeature = arangodb::AuthenticationFeature::instance();
    auto* userManager = authFeature->userManager();
    
    auto resetUserManager = std::shared_ptr<arangodb::auth::UserManager>(
        userManager,
        [](arangodb::auth::UserManager* ptr) -> void { ptr->removeAllUsers(); });

    // not authorized (missing user)
    {
      arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
      userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database

      arangodb::velocypack::Builder response;
      v8::TryCatch tryCatch(isolate.get());
      auto result =
          v8::Function::Cast(*fn_views)->CallAsFunction(context, fn_views,
                                                        static_cast<int>(args.size()),
                                                        args.data());
      EXPECT_TRUE(result.IsEmpty());
      EXPECT_TRUE(tryCatch.HasCaught());
      TRI_V8ToVPack(isolate.get(), response, tryCatch.Exception(), false);
      auto slice = response.slice();
      EXPECT_TRUE(slice.isObject());
      EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::ErrorNum) &&
                   slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() &&
                   TRI_ERROR_FORBIDDEN ==
                       ErrorCode{slice.get(arangodb::StaticStrings::ErrorNum).getNumber<int>()}));
      auto view1 = vocbase.lookupView("testView1");
      EXPECT_FALSE(!view1);
      auto view2 = vocbase.lookupView("testView2");
      EXPECT_FALSE(!view2);
    }

    // not authorized (failed detailed toVelocyPack(...)) as per https://github.com/arangodb/backlog/issues/459
    {
      arangodb::auth::UserMap userMap;
      auto& user =
          userMap
              .emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP))
              .first->second;
      user.grantDatabase(vocbase.name(), arangodb::auth::Level::RO);
      user.grantCollection(vocbase.name(), "testView1", arangodb::auth::Level::NONE);  // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      user.grantCollection(vocbase.name(), "testView2", arangodb::auth::Level::NONE);  // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database
      auto* testView = arangodb::LogicalView::cast<TestView>(logicalView2.get());
      testView->_appendVelocyPackResult = arangodb::Result(TRI_ERROR_FORBIDDEN);
      auto resetAppendVelocyPackResult =
          std::shared_ptr<TestView>(testView, [](TestView* p) -> void {
            p->_appendVelocyPackResult = arangodb::Result();
          });

      auto result =
          v8::Function::Cast(*fn_views)->CallAsFunction(context, fn_views,
                                                        static_cast<int>(args.size()),
                                                        args.data());
      EXPECT_FALSE(result.IsEmpty());
      EXPECT_TRUE(result.ToLocalChecked()->IsArray());
      auto* resultArray = v8::Array::Cast(*result.ToLocalChecked());
      EXPECT_EQ(1U, resultArray->Length());
      auto context = TRI_IGETC;
      auto* v8View =
        TRI_UnwrapClass<arangodb::LogicalView>(resultArray->Get(context, 0)
                                               .FromMaybe(v8::Local<v8::Value>())
                                               .As<v8::Object>(),
                                               WRP_VOCBASE_VIEW_TYPE, TRI_IGETC);
      EXPECT_FALSE(!v8View);
      EXPECT_EQ(std::string("testView1"), v8View->name());
      EXPECT_EQ(std::string("testViewType"), v8View->type().name());
      auto view1 = vocbase.lookupView("testView1");
      EXPECT_FALSE(!view1);
    }

    // authorized (NONE view) as per https://github.com/arangodb/backlog/issues/459
    {
      arangodb::auth::UserMap userMap;
      auto& user =
          userMap
              .emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP))
              .first->second;
      user.grantDatabase(vocbase.name(), arangodb::auth::Level::RO);
      user.grantCollection(vocbase.name(), "testView1", arangodb::auth::Level::NONE);  // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database

      vocbase.dropView(logicalView2->id(), true);  // remove second view to make test result deterministic
      auto result =
          v8::Function::Cast(*fn_views)->CallAsFunction(context, fn_views,
                                                        static_cast<int>(args.size()),
                                                        args.data());
      EXPECT_FALSE(result.IsEmpty());
      EXPECT_TRUE(result.ToLocalChecked()->IsArray());
      auto* resultArray = v8::Array::Cast(*result.ToLocalChecked());
      EXPECT_EQ(1U, resultArray->Length());
      auto context = TRI_IGETC;
      auto* v8View =
        TRI_UnwrapClass<arangodb::LogicalView>(resultArray->Get(context, 0)
                                               .FromMaybe(v8::Local<v8::Value>())
                                               .As<v8::Object>(),
                                               WRP_VOCBASE_VIEW_TYPE, TRI_IGETC);
      EXPECT_FALSE(!v8View);
      EXPECT_EQ(std::string("testView1"), v8View->name());
      EXPECT_EQ(std::string("testViewType"), v8View->type().name());
      auto view1 = vocbase.lookupView("testView1");
      EXPECT_FALSE(!view1);
    }
  }
}
