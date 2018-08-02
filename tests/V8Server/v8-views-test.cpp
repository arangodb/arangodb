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
#include <iostream>
#include "src/api.h" // must inclide V8 _before_ "catch.cpp' or CATCH() macro will be broken
#include "src/objects-inl.h" // (required to avoid compile warnings) must inclide V8 _before_ "catch.cpp' or CATCH() macro will be broken
#include "src/objects/scope-info.h" // must inclide V8 _before_ "catch.cpp' or CATCH() macro will be broken

#include "catch.hpp"
#include "../IResearch/StorageEngineMock.h"
#include "Aql/QueryRegistry.h"
#include "Basics/StaticStrings.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "Logger/Logger.h"
#include "Logger/LogTopic.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "RestServer/ViewTypesFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "Utils/ExecContext.h"
#include "V8/v8-utils.h"
#include "V8/v8-vpack.h"
#include "V8Server/v8-externals.h"
#include "V8Server/v8-views.h"
#include "velocypack/Parser.h"
#include "VocBase/vocbase.h"

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
  virtual void Free(void* data, size_t) override {
    free(data);
  }
};

template <typename T, typename U>
std::shared_ptr<T> scopedPtr(T*& ptr, U* newValue) {
  auto* location = &ptr;
  auto* oldValue = ptr;
  ptr = newValue;
  return std::shared_ptr<T>(oldValue, [location](T* p)->void { *location = p; });
}

// @Note: once V8 is initialized all 'CATCH' errors will result in SIGILL
void v8Init() {
  struct init_t {
    std::shared_ptr<v8::Platform> platform;
    init_t() {
      platform = std::shared_ptr<v8::Platform>(
        v8::platform::CreateDefaultPlatform(),
        [](v8::Platform* p)->void {
          v8::V8::Dispose();
          v8::V8::ShutdownPlatform();
          delete p;
        }
      );
      v8::V8::InitializePlatform(platform.get()); // avoid SIGSEGV duing 8::Isolate::New(...)
      v8::V8::Initialize(); // avoid error: "Check failed: thread_data_table_"
    }
  };
  static const init_t init;
  (void)(init);
}

}

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

struct V8ViewsSetup {
  StorageEngineMock engine;
  arangodb::application_features::ApplicationServer server;
  std::vector<std::pair<arangodb::application_features::ApplicationFeature*, bool>> features;

  V8ViewsSetup(): server(nullptr, nullptr) {
    arangodb::EngineSelectorFeature::ENGINE = &engine;

    v8Init(); // on-time initialize V8

    // suppress INFO {authentication} Authentication is turned on (system only), authentication for unix sockets is turned on
    arangodb::LogTopic::setLogLevel(arangodb::Logger::AUTHENTICATION.name(), arangodb::LogLevel::WARN);

    // setup required application features
    features.emplace_back(new arangodb::AuthenticationFeature(&server), false); // required for VocbaseContext
    features.emplace_back(new arangodb::DatabaseFeature(&server), false); // required for TRI_vocbase_t::renameView(...)
    features.emplace_back(new arangodb::QueryRegistryFeature(&server), false); // required for TRI_vocbase_t
    features.emplace_back(new arangodb::ViewTypesFeature(&server), false); // required for LogicalView::create(...)

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

    struct TestView: public arangodb::LogicalView {
      arangodb::velocypack::Builder _properties;

      TestView(TRI_vocbase_t& vocbase, arangodb::velocypack::Slice const& definition, uint64_t planVersion)
        : arangodb::LogicalView(vocbase, definition, planVersion) {
      }
      virtual arangodb::Result appendVelocyPack(arangodb::velocypack::Builder& builder, bool /*detailed*/, bool /*forPersistence*/) const { builder.add("properties", _properties.slice()); return arangodb::Result(); }
      virtual arangodb::Result drop() override { return arangodb::Result(); }
      static std::shared_ptr<LogicalView> make(
            TRI_vocbase_t& vocbase,
            arangodb::velocypack::Slice const& definition,
            bool isNew,
            uint64_t planVersion,
            arangodb::LogicalView::PreCommitCallback const& preCommit
      ) {
        auto view = std::make_shared<TestView>(vocbase, definition, planVersion);
        return preCommit(view) ? view : nullptr;
      }
      virtual void open() override {}
      virtual arangodb::Result rename(std::string&& newName, bool doSync) override { name(std::move(newName)); return arangodb::Result(); }
      virtual arangodb::Result updateProperties(arangodb::velocypack::Slice const& properties, bool partialUpdate, bool doSync) override { _properties = arangodb::velocypack::Builder(properties); return arangodb::Result(); }
      virtual bool visitCollections(CollectionVisitor const& visitor) const override { return true; }
    };
   auto* viewTypesFeature =
     arangodb::application_features::ApplicationServer::lookupFeature<arangodb::ViewTypesFeature>();

    viewTypesFeature->emplace(
      arangodb::LogicalDataSource::Type::emplace(arangodb::velocypack::StringRef("testViewType")),
      TestView::make
    );
  }

  ~V8ViewsSetup() {
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

    arangodb::LogTopic::setLogLevel(arangodb::Logger::AUTHENTICATION.name(), arangodb::LogLevel::DEFAULT);
  }
};

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief setup
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("V8ViewsTest", "[v8]") {
  V8ViewsSetup s;
  (void)(s);

SECTION("test_auth") {
  // test create
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    v8::Isolate::CreateParams isolateParams;
    ArrayBufferAllocator arrayBufferAllocator;
    isolateParams.array_buffer_allocator = &arrayBufferAllocator;
    auto isolate = std::shared_ptr<v8::Isolate>(v8::Isolate::New(isolateParams), [](v8::Isolate* p)->void { p->Dispose(); });
    REQUIRE((nullptr != isolate));
    v8::Isolate::Scope isolateScope(isolate.get()); // otherwise v8::Isolate::Logger() will fail (called from v8::Exception::Error)
    v8::internal::Isolate::Current()->InitializeLoggingAndCounters(); // otherwise v8::Isolate::Logger() will fail (called from v8::Exception::Error)
    v8::HandleScope handleScope(isolate.get()); // required for v8::Context::New(...), v8::ObjectTemplate::New(...) and TRI_AddMethodVocbase(...)
    auto context = v8::Context::New(isolate.get());
    v8::Context::Scope contextScope(context); // required for TRI_AddMethodVocbase(...)
    auto v8g = TRI_CreateV8Globals(isolate.get()); // create and set inside 'isolate' for use with 'TRI_GET_GLOBALS()'
    v8g->ArangoErrorTempl.Reset(isolate.get(), v8::ObjectTemplate::New(isolate.get())); // otherwise v8:-utils::CreateErrorObject(...) will fail
    v8g->_vocbase = &vocbase;
    auto arangoDBNS = v8::ObjectTemplate::New(isolate.get());
    TRI_InitV8Views(context, &vocbase, v8g, isolate.get(), arangoDBNS);

    auto fn_createView = arangoDBNS->NewInstance()->Get(TRI_V8_ASCII_STRING(isolate.get(), "_createView"));
    CHECK((fn_createView->IsFunction()));

    std::vector<v8::Local<v8::Value>> args = {
      TRI_V8_ASCII_STRING(isolate.get(), "testView"),
      TRI_V8_ASCII_STRING(isolate.get(), "testViewType"),
      TRI_VPackToV8(isolate.get(), arangodb::velocypack::Parser::fromJson("{}")->slice()),
    };

    CHECK((vocbase.views().empty()));

    struct ExecContext: public arangodb::ExecContext {
      ExecContext(): arangodb::ExecContext(0, "", "", arangodb::auth::Level::NONE, arangodb::auth::Level::NONE) {}
    } execContext;
    auto resetExecContext = scopedPtr(ExecContext::CURRENT, &execContext);
    auto* authFeature = arangodb::AuthenticationFeature::instance();
    auto* userManager = authFeature->userManager();
    arangodb::aql::QueryRegistry queryRegistry(0); // required for UserManager::loadFromDB()
    userManager->setQueryRegistry(&queryRegistry);
    auto resetUserManager = std::shared_ptr<arangodb::auth::UserManager>(userManager, [](arangodb::auth::UserManager* ptr)->void { ptr->removeAllUsers(); });

    // not authorized (missing user)
    {
      arangodb::auth::UserMap userMap; // empty map, no user -> no permissions
      userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

      arangodb::velocypack::Builder responce;
      v8::TryCatch tryCatch(isolate.get());
      auto result = v8::Function::Cast(*fn_createView)->CallAsFunction(context, fn_createView, args.size(), args.data());
      CHECK((result.IsEmpty()));
      CHECK((tryCatch.HasCaught()));
      CHECK((TRI_ERROR_NO_ERROR == TRI_V8ToVPack(isolate.get(), responce, tryCatch.Exception(), false)));
      auto slice = responce.slice();
      CHECK((slice.isObject()));
      CHECK((slice.hasKey(arangodb::StaticStrings::ErrorNum) && slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() && TRI_ERROR_FORBIDDEN == slice.get(arangodb::StaticStrings::ErrorNum).getNumber<int>()));
      CHECK((vocbase.views().empty()));
    }

    // not authorized (RO user)
    {
      arangodb::auth::UserMap userMap;
      auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
      user.grantDatabase(vocbase.name(), arangodb::auth::Level::RO);
      userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

      arangodb::velocypack::Builder responce;
      v8::TryCatch tryCatch(isolate.get());
      auto result = v8::Function::Cast(*fn_createView)->CallAsFunction(context, fn_createView, args.size(), args.data());
      CHECK((result.IsEmpty()));
      CHECK((tryCatch.HasCaught()));
      CHECK((TRI_ERROR_NO_ERROR == TRI_V8ToVPack(isolate.get(), responce, tryCatch.Exception(), false)));
      auto slice = responce.slice();
      CHECK((slice.isObject()));
      CHECK((slice.hasKey(arangodb::StaticStrings::ErrorNum) && slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() && TRI_ERROR_FORBIDDEN == slice.get(arangodb::StaticStrings::ErrorNum).getNumber<int>()));
      CHECK((vocbase.views().empty()));
    }

    // authorzed (RW user)
    {
      arangodb::auth::UserMap userMap;
      auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
      user.grantDatabase(vocbase.name(), arangodb::auth::Level::RW);
      userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

      auto result = v8::Function::Cast(*fn_createView)->CallAsFunction(context, fn_createView, args.size(), args.data());
      CHECK((!result.IsEmpty()));
      CHECK((result.ToLocalChecked()->IsObject()));
      auto v8View = *TRI_UnwrapClass<std::shared_ptr<arangodb::LogicalView>>(result.ToLocalChecked()->ToObject(), WRP_VOCBASE_VIEW_TYPE);
      CHECK((false == !v8View));
      CHECK((std::string("testView") == v8View->name()));
      CHECK((std::string("testViewType") == v8View->type().name()));
      auto view = vocbase.lookupView("testView");
      CHECK((false == !view));
    }
  }

  // test drop (static)
  {
    auto createViewJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"testViewType\" }");
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalView = vocbase.createView(createViewJson->slice());
    REQUIRE((false == !logicalView));

    v8::Isolate::CreateParams isolateParams;
    ArrayBufferAllocator arrayBufferAllocator;
    isolateParams.array_buffer_allocator = &arrayBufferAllocator;
    auto isolate = std::shared_ptr<v8::Isolate>(v8::Isolate::New(isolateParams), [](v8::Isolate* p)->void { p->Dispose(); });
    REQUIRE((nullptr != isolate));
    v8::Isolate::Scope isolateScope(isolate.get()); // otherwise v8::Isolate::Logger() will fail (called from v8::Exception::Error)
    v8::internal::Isolate::Current()->InitializeLoggingAndCounters(); // otherwise v8::Isolate::Logger() will fail (called from v8::Exception::Error)
    v8::HandleScope handleScope(isolate.get()); // required for v8::Context::New(...), v8::ObjectTemplate::New(...) and TRI_AddMethodVocbase(...)
    auto context = v8::Context::New(isolate.get());
    v8::Context::Scope contextScope(context); // required for TRI_AddMethodVocbase(...)
    auto v8g = TRI_CreateV8Globals(isolate.get()); // create and set inside 'isolate' for use with 'TRI_GET_GLOBALS()'
    v8g->ArangoErrorTempl.Reset(isolate.get(), v8::ObjectTemplate::New(isolate.get())); // otherwise v8:-utils::CreateErrorObject(...) will fail
    v8g->_vocbase = &vocbase;
    auto arangoDBNS = v8::ObjectTemplate::New(isolate.get());
    TRI_InitV8Views(context, &vocbase, v8g, isolate.get(), arangoDBNS);

    auto fn_dropView = arangoDBNS->NewInstance()->Get(TRI_V8_ASCII_STRING(isolate.get(), "_dropView"));
    CHECK((fn_dropView->IsFunction()));

    std::vector<v8::Local<v8::Value>> args = {
      TRI_V8_ASCII_STRING(isolate.get(), "testView"),
    };

    struct ExecContext: public arangodb::ExecContext {
      ExecContext(): arangodb::ExecContext(0, "", "", arangodb::auth::Level::NONE, arangodb::auth::Level::NONE) {}
    } execContext;
    auto resetExecContext = scopedPtr(ExecContext::CURRENT, &execContext);
    auto* authFeature = arangodb::AuthenticationFeature::instance();
    auto* userManager = authFeature->userManager();
    arangodb::aql::QueryRegistry queryRegistry(0); // required for UserManager::loadFromDB()
    userManager->setQueryRegistry(&queryRegistry);
    auto resetUserManager = std::shared_ptr<arangodb::auth::UserManager>(userManager, [](arangodb::auth::UserManager* ptr)->void { ptr->removeAllUsers(); });

    // not authorized (missing user)
    {
      arangodb::auth::UserMap userMap; // empty map, no user -> no permissions
      userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

      arangodb::velocypack::Builder responce;
      v8::TryCatch tryCatch(isolate.get());
      auto result = v8::Function::Cast(*fn_dropView)->CallAsFunction(context, fn_dropView, args.size(), args.data());
      CHECK((result.IsEmpty()));
      CHECK((tryCatch.HasCaught()));
      CHECK((TRI_ERROR_NO_ERROR == TRI_V8ToVPack(isolate.get(), responce, tryCatch.Exception(), false)));
      auto slice = responce.slice();
      CHECK((slice.isObject()));
      CHECK((slice.hasKey(arangodb::StaticStrings::ErrorNum) && slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() && TRI_ERROR_FORBIDDEN == slice.get(arangodb::StaticStrings::ErrorNum).getNumber<int>()));
      auto view = vocbase.lookupView("testView");
      CHECK((false == !view));
    }

    // not authorized (RO user database)
    {
      arangodb::auth::UserMap userMap;
      auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
      user.grantDatabase(vocbase.name(), arangodb::auth::Level::RO);
      userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

      arangodb::velocypack::Builder responce;
      v8::TryCatch tryCatch(isolate.get());
      auto result = v8::Function::Cast(*fn_dropView)->CallAsFunction(context, fn_dropView, args.size(), args.data());
      CHECK((result.IsEmpty()));
      CHECK((tryCatch.HasCaught()));
      CHECK((TRI_ERROR_NO_ERROR == TRI_V8ToVPack(isolate.get(), responce, tryCatch.Exception(), false)));
      auto slice = responce.slice();
      CHECK((slice.isObject()));
      CHECK((slice.hasKey(arangodb::StaticStrings::ErrorNum) && slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() && TRI_ERROR_FORBIDDEN == slice.get(arangodb::StaticStrings::ErrorNum).getNumber<int>()));
      auto view = vocbase.lookupView("testView");
      CHECK((false == !view));
    }

    // not authorized (RO user view)
    {
      arangodb::auth::UserMap userMap;
      auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
      user.grantDatabase(vocbase.name(), arangodb::auth::Level::RW);
      user.grantCollection(vocbase.name(), "testView", arangodb::auth::Level::RO);
      userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

      arangodb::velocypack::Builder responce;
      v8::TryCatch tryCatch(isolate.get());
      auto result = v8::Function::Cast(*fn_dropView)->CallAsFunction(context, fn_dropView, args.size(), args.data());
      CHECK((result.IsEmpty()));
      CHECK((tryCatch.HasCaught()));
      CHECK((TRI_ERROR_NO_ERROR == TRI_V8ToVPack(isolate.get(), responce, tryCatch.Exception(), false)));
      auto slice = responce.slice();
      CHECK((slice.isObject()));
      CHECK((slice.hasKey(arangodb::StaticStrings::ErrorNum) && slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() && TRI_ERROR_FORBIDDEN == slice.get(arangodb::StaticStrings::ErrorNum).getNumber<int>()));
      auto view = vocbase.lookupView("testView");
      CHECK((false == !view));
    }

    // authorzed (RW user)
    {
      arangodb::auth::UserMap userMap;
      auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
      user.grantDatabase(vocbase.name(), arangodb::auth::Level::RW);
      user.grantCollection(vocbase.name(), "testView", arangodb::auth::Level::RW);
      userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

      auto result = v8::Function::Cast(*fn_dropView)->CallAsFunction(context, fn_dropView, args.size(), args.data());
      CHECK((!result.IsEmpty()));
      CHECK((result.ToLocalChecked()->IsUndefined()));
      CHECK((vocbase.views().empty()));
    }
  }

  // test drop (instance)
  {
    auto createViewJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"testViewType\" }");
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalView = vocbase.createView(createViewJson->slice());
    REQUIRE((false == !logicalView));

    v8::Isolate::CreateParams isolateParams;
    ArrayBufferAllocator arrayBufferAllocator;
    isolateParams.array_buffer_allocator = &arrayBufferAllocator;
    auto isolate = std::shared_ptr<v8::Isolate>(v8::Isolate::New(isolateParams), [](v8::Isolate* p)->void { p->Dispose(); });
    REQUIRE((nullptr != isolate));
    v8::Isolate::Scope isolateScope(isolate.get()); // otherwise v8::Isolate::Logger() will fail (called from v8::Exception::Error)
    v8::internal::Isolate::Current()->InitializeLoggingAndCounters(); // otherwise v8::Isolate::Logger() will fail (called from v8::Exception::Error)
    v8::HandleScope handleScope(isolate.get()); // required for v8::Context::New(...), v8::ObjectTemplate::New(...) and TRI_AddMethodVocbase(...)
    auto context = v8::Context::New(isolate.get());
    v8::Context::Scope contextScope(context); // required for TRI_AddMethodVocbase(...)
    auto v8g = TRI_CreateV8Globals(isolate.get()); // create and set inside 'isolate' for use with 'TRI_GET_GLOBALS()'
    v8g->ArangoErrorTempl.Reset(isolate.get(), v8::ObjectTemplate::New(isolate.get())); // otherwise v8:-utils::CreateErrorObject(...) will fail
    v8g->_vocbase = &vocbase;
    auto arangoDBNS = v8::ObjectTemplate::New(isolate.get());
    TRI_InitV8Views(context, &vocbase, v8g, isolate.get(), arangoDBNS);

    auto arangoView = v8::Local<v8::ObjectTemplate>::New(isolate.get(), v8g->VocbaseViewTempl)->NewInstance();
    auto fn_drop = arangoView->Get(TRI_V8_ASCII_STRING(isolate.get(), "drop"));
    CHECK((fn_drop->IsFunction()));

    arangoView->SetInternalField(SLOT_CLASS_TYPE, v8::Integer::New(isolate.get(), WRP_VOCBASE_VIEW_TYPE));
    arangoView->SetInternalField(SLOT_CLASS, v8::External::New(isolate.get(), &logicalView));
    arangoView->SetInternalField(SLOT_EXTERNAL, v8::External::New(isolate.get(), &logicalView));
    std::vector<v8::Local<v8::Value>> args = {
    };

    struct ExecContext: public arangodb::ExecContext {
      ExecContext(): arangodb::ExecContext(0, "", "", arangodb::auth::Level::NONE, arangodb::auth::Level::NONE) {}
    } execContext;
    auto resetExecContext = scopedPtr(ExecContext::CURRENT, &execContext);
    auto* authFeature = arangodb::AuthenticationFeature::instance();
    auto* userManager = authFeature->userManager();
    arangodb::aql::QueryRegistry queryRegistry(0); // required for UserManager::loadFromDB()
    userManager->setQueryRegistry(&queryRegistry);
    auto resetUserManager = std::shared_ptr<arangodb::auth::UserManager>(userManager, [](arangodb::auth::UserManager* ptr)->void { ptr->removeAllUsers(); });

    // not authorized (missing user)
    {
      arangodb::auth::UserMap userMap; // empty map, no user -> no permissions
      userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

      arangodb::velocypack::Builder responce;
      v8::TryCatch tryCatch(isolate.get());
      auto result = v8::Function::Cast(*fn_drop)->CallAsFunction(context, arangoView, args.size(), args.data());
      CHECK((result.IsEmpty()));
      CHECK((tryCatch.HasCaught()));
      CHECK((TRI_ERROR_NO_ERROR == TRI_V8ToVPack(isolate.get(), responce, tryCatch.Exception(), false)));
      auto slice = responce.slice();
      CHECK((slice.isObject()));
      CHECK((slice.hasKey(arangodb::StaticStrings::ErrorNum) && slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() && TRI_ERROR_FORBIDDEN == slice.get(arangodb::StaticStrings::ErrorNum).getNumber<int>()));
      auto view = vocbase.lookupView("testView");
      CHECK((false == !view));
    }

    // not authorized (RO user database)
    {
      arangodb::auth::UserMap userMap;
      auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
      user.grantDatabase(vocbase.name(), arangodb::auth::Level::RO);
      userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

      arangodb::velocypack::Builder responce;
      v8::TryCatch tryCatch(isolate.get());
      auto result = v8::Function::Cast(*fn_drop)->CallAsFunction(context, arangoView, args.size(), args.data());
      CHECK((result.IsEmpty()));
      CHECK((tryCatch.HasCaught()));
      CHECK((TRI_ERROR_NO_ERROR == TRI_V8ToVPack(isolate.get(), responce, tryCatch.Exception(), false)));
      auto slice = responce.slice();
      CHECK((slice.isObject()));
      CHECK((slice.hasKey(arangodb::StaticStrings::ErrorNum) && slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() && TRI_ERROR_FORBIDDEN == slice.get(arangodb::StaticStrings::ErrorNum).getNumber<int>()));
      auto view = vocbase.lookupView("testView");
      CHECK((false == !view));
    }

    // not authorized (RO user view)
    {
      arangodb::auth::UserMap userMap;
      auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
      user.grantDatabase(vocbase.name(), arangodb::auth::Level::RW);
      user.grantCollection(vocbase.name(), "testView", arangodb::auth::Level::RO);
      userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

      arangodb::velocypack::Builder responce;
      v8::TryCatch tryCatch(isolate.get());
      auto result = v8::Function::Cast(*fn_drop)->CallAsFunction(context, arangoView, args.size(), args.data());
      CHECK((result.IsEmpty()));
      CHECK((tryCatch.HasCaught()));
      CHECK((TRI_ERROR_NO_ERROR == TRI_V8ToVPack(isolate.get(), responce, tryCatch.Exception(), false)));
      auto slice = responce.slice();
      CHECK((slice.isObject()));
      CHECK((slice.hasKey(arangodb::StaticStrings::ErrorNum) && slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() && TRI_ERROR_FORBIDDEN == slice.get(arangodb::StaticStrings::ErrorNum).getNumber<int>()));
      auto view = vocbase.lookupView("testView");
      CHECK((false == !view));
    }

    // authorzed (RW user)
    {
      arangodb::auth::UserMap userMap;
      auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
      user.grantDatabase(vocbase.name(), arangodb::auth::Level::RW);
      user.grantCollection(vocbase.name(), "testView", arangodb::auth::Level::RW);
      userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

      auto result = v8::Function::Cast(*fn_drop)->CallAsFunction(context, arangoView, args.size(), args.data());
      CHECK((!result.IsEmpty()));
      CHECK((result.ToLocalChecked()->IsUndefined()));
      CHECK((vocbase.views().empty()));
    }
  }

  // test rename
  {
    auto createViewJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"testViewType\" }");
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalView = vocbase.createView(createViewJson->slice());
    REQUIRE((false == !logicalView));

    v8::Isolate::CreateParams isolateParams;
    ArrayBufferAllocator arrayBufferAllocator;
    isolateParams.array_buffer_allocator = &arrayBufferAllocator;
    auto isolate = std::shared_ptr<v8::Isolate>(v8::Isolate::New(isolateParams), [](v8::Isolate* p)->void { p->Dispose(); });
    REQUIRE((nullptr != isolate));
    v8::Isolate::Scope isolateScope(isolate.get()); // otherwise v8::Isolate::Logger() will fail (called from v8::Exception::Error)
    v8::internal::Isolate::Current()->InitializeLoggingAndCounters(); // otherwise v8::Isolate::Logger() will fail (called from v8::Exception::Error)
    v8::HandleScope handleScope(isolate.get()); // required for v8::Context::New(...), v8::ObjectTemplate::New(...) and TRI_AddMethodVocbase(...)
    auto context = v8::Context::New(isolate.get());
    v8::Context::Scope contextScope(context); // required for TRI_AddMethodVocbase(...)
    auto v8g = TRI_CreateV8Globals(isolate.get()); // create and set inside 'isolate' for use with 'TRI_GET_GLOBALS()'
    v8g->ArangoErrorTempl.Reset(isolate.get(), v8::ObjectTemplate::New(isolate.get())); // otherwise v8:-utils::CreateErrorObject(...) will fail
    v8g->_vocbase = &vocbase;
    auto arangoDBNS = v8::ObjectTemplate::New(isolate.get());
    TRI_InitV8Views(context, &vocbase, v8g, isolate.get(), arangoDBNS);

    auto arangoView = v8::Local<v8::ObjectTemplate>::New(isolate.get(), v8g->VocbaseViewTempl)->NewInstance();
    auto fn_rename = arangoView->Get(TRI_V8_ASCII_STRING(isolate.get(), "rename"));
    CHECK((fn_rename->IsFunction()));

    arangoView->SetInternalField(SLOT_CLASS_TYPE, v8::Integer::New(isolate.get(), WRP_VOCBASE_VIEW_TYPE));
    arangoView->SetInternalField(SLOT_CLASS, v8::External::New(isolate.get(), &logicalView));
    arangoView->SetInternalField(SLOT_EXTERNAL, v8::External::New(isolate.get(), &logicalView));
    std::vector<v8::Local<v8::Value>> args = {
      TRI_V8_ASCII_STRING(isolate.get(), "testView1"),
    };

    struct ExecContext: public arangodb::ExecContext {
      ExecContext(): arangodb::ExecContext(0, "", "", arangodb::auth::Level::NONE, arangodb::auth::Level::NONE) {}
    } execContext;
    auto resetExecContext = scopedPtr(ExecContext::CURRENT, &execContext);
    auto* authFeature = arangodb::AuthenticationFeature::instance();
    auto* userManager = authFeature->userManager();
    arangodb::aql::QueryRegistry queryRegistry(0); // required for UserManager::loadFromDB()
    userManager->setQueryRegistry(&queryRegistry);
    auto resetUserManager = std::shared_ptr<arangodb::auth::UserManager>(userManager, [](arangodb::auth::UserManager* ptr)->void { ptr->removeAllUsers(); });

    // not authorized (missing user)
    {
      arangodb::auth::UserMap userMap; // empty map, no user -> no permissions
      userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

      arangodb::velocypack::Builder responce;
      v8::TryCatch tryCatch(isolate.get());
      auto result = v8::Function::Cast(*fn_rename)->CallAsFunction(context, arangoView, args.size(), args.data());
      CHECK((result.IsEmpty()));
      CHECK((tryCatch.HasCaught()));
      CHECK((TRI_ERROR_NO_ERROR == TRI_V8ToVPack(isolate.get(), responce, tryCatch.Exception(), false)));
      auto slice = responce.slice();
      CHECK((slice.isObject()));
      CHECK((slice.hasKey(arangodb::StaticStrings::ErrorNum) && slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() && TRI_ERROR_FORBIDDEN == slice.get(arangodb::StaticStrings::ErrorNum).getNumber<int>()));
      auto view = vocbase.lookupView("testView");
      CHECK((false == !view));
      auto view1 = vocbase.lookupView("testView1");
      CHECK((true == !view1));
    }

    // not authorized (RO user database)
    {
      arangodb::auth::UserMap userMap;
      auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
      user.grantDatabase(vocbase.name(), arangodb::auth::Level::RO);
      userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

      arangodb::velocypack::Builder responce;
      v8::TryCatch tryCatch(isolate.get());
      auto result = v8::Function::Cast(*fn_rename)->CallAsFunction(context, arangoView, args.size(), args.data());
      CHECK((result.IsEmpty()));
      CHECK((tryCatch.HasCaught()));
      CHECK((TRI_ERROR_NO_ERROR == TRI_V8ToVPack(isolate.get(), responce, tryCatch.Exception(), false)));
      auto slice = responce.slice();
      CHECK((slice.isObject()));
      CHECK((slice.hasKey(arangodb::StaticStrings::ErrorNum) && slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() && TRI_ERROR_FORBIDDEN == slice.get(arangodb::StaticStrings::ErrorNum).getNumber<int>()));
      auto view = vocbase.lookupView("testView");
      CHECK((false == !view));
      auto view1 = vocbase.lookupView("testView1");
      CHECK((true == !view1));
    }

    // not authorized (RO user view)
    {
      arangodb::auth::UserMap userMap;
      auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
      user.grantDatabase(vocbase.name(), arangodb::auth::Level::RW);
      user.grantCollection(vocbase.name(), "testView", arangodb::auth::Level::RO);
      userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

      arangodb::velocypack::Builder responce;
      v8::TryCatch tryCatch(isolate.get());
      auto result = v8::Function::Cast(*fn_rename)->CallAsFunction(context, arangoView, args.size(), args.data());
      CHECK((result.IsEmpty()));
      CHECK((tryCatch.HasCaught()));
      CHECK((TRI_ERROR_NO_ERROR == TRI_V8ToVPack(isolate.get(), responce, tryCatch.Exception(), false)));
      auto slice = responce.slice();
      CHECK((slice.isObject()));
      CHECK((slice.hasKey(arangodb::StaticStrings::ErrorNum) && slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() && TRI_ERROR_FORBIDDEN == slice.get(arangodb::StaticStrings::ErrorNum).getNumber<int>()));
      auto view = vocbase.lookupView("testView");
      CHECK((false == !view));
      auto view1 = vocbase.lookupView("testView1");
      CHECK((true == !view1));
    }

    // authorzed (RW user)
    {
      arangodb::auth::UserMap userMap;
      auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
      user.grantDatabase(vocbase.name(), arangodb::auth::Level::RW);
      user.grantCollection(vocbase.name(), "testView", arangodb::auth::Level::RW);
      userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

      auto result = v8::Function::Cast(*fn_rename)->CallAsFunction(context, arangoView, args.size(), args.data());
      CHECK((!result.IsEmpty()));
      CHECK((result.ToLocalChecked()->IsUndefined()));
      auto view = vocbase.lookupView("testView");
      CHECK((true == !view));
      auto view1 = vocbase.lookupView("testView1");
      CHECK((false == !view1));
    }
  }

  // test modify
  {
    auto createViewJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"testViewType\" }");
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalView = vocbase.createView(createViewJson->slice());
    REQUIRE((false == !logicalView));

    v8::Isolate::CreateParams isolateParams;
    ArrayBufferAllocator arrayBufferAllocator;
    isolateParams.array_buffer_allocator = &arrayBufferAllocator;
    auto isolate = std::shared_ptr<v8::Isolate>(v8::Isolate::New(isolateParams), [](v8::Isolate* p)->void { p->Dispose(); });
    REQUIRE((nullptr != isolate));
    char isolateData[64]; // 64 > sizeof(arangodb::V8PlatformFeature::IsolateData)
    std::memset(isolateData, 0, 64); // otherwise arangodb::V8PlatformFeature::isOutOfMemory(isolate) returns true
    isolate->SetData(arangodb::V8PlatformFeature::V8_INFO, isolateData); // required for TRI_VPackToV8(...) with nn-empty jSON
    v8::Isolate::Scope isolateScope(isolate.get()); // otherwise v8::Isolate::Logger() will fail (called from v8::Exception::Error)
    v8::internal::Isolate::Current()->InitializeLoggingAndCounters(); // otherwise v8::Isolate::Logger() will fail (called from v8::Exception::Error)
    v8::HandleScope handleScope(isolate.get()); // required for v8::Context::New(...), v8::ObjectTemplate::New(...) and TRI_AddMethodVocbase(...)
    auto context = v8::Context::New(isolate.get());
    v8::Context::Scope contextScope(context); // required for TRI_AddMethodVocbase(...)
    auto v8g = TRI_CreateV8Globals(isolate.get()); // create and set inside 'isolate' for use with 'TRI_GET_GLOBALS()'
    v8g->ArangoErrorTempl.Reset(isolate.get(), v8::ObjectTemplate::New(isolate.get())); // otherwise v8:-utils::CreateErrorObject(...) will fail
    v8g->_vocbase = &vocbase;
    auto arangoDBNS = v8::ObjectTemplate::New(isolate.get());
    TRI_InitV8Views(context, &vocbase, v8g, isolate.get(), arangoDBNS);

    auto arangoView = v8::Local<v8::ObjectTemplate>::New(isolate.get(), v8g->VocbaseViewTempl)->NewInstance();
    auto fn_properties = arangoView->Get(TRI_V8_ASCII_STRING(isolate.get(), "properties"));
    CHECK((fn_properties->IsFunction()));

    arangoView->SetInternalField(SLOT_CLASS_TYPE, v8::Integer::New(isolate.get(), WRP_VOCBASE_VIEW_TYPE));
    arangoView->SetInternalField(SLOT_CLASS, v8::External::New(isolate.get(), &logicalView));
    arangoView->SetInternalField(SLOT_EXTERNAL, v8::External::New(isolate.get(), &logicalView));
    std::vector<v8::Local<v8::Value>> args = {
      TRI_VPackToV8(isolate.get(), arangodb::velocypack::Parser::fromJson("{ \"key\": \"value\" }")->slice()),
    };

    struct ExecContext: public arangodb::ExecContext {
      ExecContext(): arangodb::ExecContext(0, "", "", arangodb::auth::Level::NONE, arangodb::auth::Level::NONE) {}
    } execContext;
    auto resetExecContext = scopedPtr(ExecContext::CURRENT, &execContext);
    auto* authFeature = arangodb::AuthenticationFeature::instance();
    auto* userManager = authFeature->userManager();
    arangodb::aql::QueryRegistry queryRegistry(0); // required for UserManager::loadFromDB()
    userManager->setQueryRegistry(&queryRegistry);
    auto resetUserManager = std::shared_ptr<arangodb::auth::UserManager>(userManager, [](arangodb::auth::UserManager* ptr)->void { ptr->removeAllUsers(); });

    // not authorized (missing user)
    {
      arangodb::auth::UserMap userMap; // empty map, no user -> no permissions
      userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

      arangodb::velocypack::Builder responce;
      v8::TryCatch tryCatch(isolate.get());
      auto result = v8::Function::Cast(*fn_properties)->CallAsFunction(context, arangoView, args.size(), args.data());
      CHECK((result.IsEmpty()));
      CHECK((tryCatch.HasCaught()));
      CHECK((TRI_ERROR_NO_ERROR == TRI_V8ToVPack(isolate.get(), responce, tryCatch.Exception(), false)));
      auto slice = responce.slice();
      CHECK((slice.isObject()));
      CHECK((slice.hasKey(arangodb::StaticStrings::ErrorNum) && slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() && TRI_ERROR_FORBIDDEN == slice.get(arangodb::StaticStrings::ErrorNum).getNumber<int>()));
      auto view = vocbase.lookupView("testView");
      CHECK((false == !view));
    }

    // not authorized (RO user database)
    {
      arangodb::auth::UserMap userMap;
      auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
      user.grantDatabase(vocbase.name(), arangodb::auth::Level::RO);
      userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

      arangodb::velocypack::Builder responce;
      v8::TryCatch tryCatch(isolate.get());
      auto result = v8::Function::Cast(*fn_properties)->CallAsFunction(context, arangoView, args.size(), args.data());
      CHECK((result.IsEmpty()));
      CHECK((tryCatch.HasCaught()));
      CHECK((TRI_ERROR_NO_ERROR == TRI_V8ToVPack(isolate.get(), responce, tryCatch.Exception(), false)));
      auto slice = responce.slice();
      CHECK((slice.isObject()));
      CHECK((slice.hasKey(arangodb::StaticStrings::ErrorNum) && slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() && TRI_ERROR_FORBIDDEN == slice.get(arangodb::StaticStrings::ErrorNum).getNumber<int>()));
      auto view = vocbase.lookupView("testView");
      CHECK((false == !view));
    }

    // not authorized (RO user view)
    {
      arangodb::auth::UserMap userMap;
      auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
      user.grantDatabase(vocbase.name(), arangodb::auth::Level::RW);
      user.grantCollection(vocbase.name(), "testView", arangodb::auth::Level::RO);
      userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

      arangodb::velocypack::Builder responce;
      v8::TryCatch tryCatch(isolate.get());
      auto result = v8::Function::Cast(*fn_properties)->CallAsFunction(context, arangoView, args.size(), args.data());
      CHECK((result.IsEmpty()));
      CHECK((tryCatch.HasCaught()));
      CHECK((TRI_ERROR_NO_ERROR == TRI_V8ToVPack(isolate.get(), responce, tryCatch.Exception(), false)));
      auto slice = responce.slice();
      CHECK((slice.isObject()));
      CHECK((slice.hasKey(arangodb::StaticStrings::ErrorNum) && slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() && TRI_ERROR_FORBIDDEN == slice.get(arangodb::StaticStrings::ErrorNum).getNumber<int>()));
      auto view = vocbase.lookupView("testView");
      CHECK((false == !view));
    }

    // authorzed (RW user)
    {
      arangodb::auth::UserMap userMap;
      auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
      user.grantDatabase(vocbase.name(), arangodb::auth::Level::RW);
      user.grantCollection(vocbase.name(), "testView", arangodb::auth::Level::RW);
      userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

      arangodb::velocypack::Builder responce;
      auto result = v8::Function::Cast(*fn_properties)->CallAsFunction(context, arangoView, args.size(), args.data());
      CHECK((!result.IsEmpty()));
      CHECK((result.ToLocalChecked()->IsObject()));
      CHECK((TRI_ERROR_NO_ERROR == TRI_V8ToVPack(isolate.get(), responce, result.ToLocalChecked(), false)));
      auto slice = responce.slice();
      CHECK((slice.isObject()));
      CHECK((slice.hasKey(arangodb::StaticStrings::DataSourceName) && slice.get(arangodb::StaticStrings::DataSourceName).isString() && std::string("testView") == slice.get(arangodb::StaticStrings::DataSourceName).copyString()));
      CHECK((slice.hasKey("properties") && slice.get("properties").isObject() && slice.get("properties").hasKey("key") && slice.get("properties").get("key").isString() && std::string("value") == slice.get("properties").get("key").copyString()));
      auto view = vocbase.lookupView("testView");
      CHECK((false == !view));
    }
  }

  // test get view (basic)
  {
    auto createViewJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"testViewType\" }");
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalView = vocbase.createView(createViewJson->slice());
    REQUIRE((false == !logicalView));

    v8::Isolate::CreateParams isolateParams;
    ArrayBufferAllocator arrayBufferAllocator;
    isolateParams.array_buffer_allocator = &arrayBufferAllocator;
    auto isolate = std::shared_ptr<v8::Isolate>(v8::Isolate::New(isolateParams), [](v8::Isolate* p)->void { p->Dispose(); });
    REQUIRE((nullptr != isolate));
    v8::Isolate::Scope isolateScope(isolate.get()); // otherwise v8::Isolate::Logger() will fail (called from v8::Exception::Error)
    v8::internal::Isolate::Current()->InitializeLoggingAndCounters(); // otherwise v8::Isolate::Logger() will fail (called from v8::Exception::Error)
    v8::HandleScope handleScope(isolate.get()); // required for v8::Context::New(...), v8::ObjectTemplate::New(...) and TRI_AddMethodVocbase(...)
    auto context = v8::Context::New(isolate.get());
    v8::Context::Scope contextScope(context); // required for TRI_AddMethodVocbase(...)
    auto v8g = TRI_CreateV8Globals(isolate.get()); // create and set inside 'isolate' for use with 'TRI_GET_GLOBALS()'
    v8g->ArangoErrorTempl.Reset(isolate.get(), v8::ObjectTemplate::New(isolate.get())); // otherwise v8:-utils::CreateErrorObject(...) will fail
    v8g->_vocbase = &vocbase;
    auto arangoDBNS = v8::ObjectTemplate::New(isolate.get());
    TRI_InitV8Views(context, &vocbase, v8g, isolate.get(), arangoDBNS);

    auto fn_view = arangoDBNS->NewInstance()->Get(TRI_V8_ASCII_STRING(isolate.get(), "_view"));
    CHECK((fn_view->IsFunction()));

    std::vector<v8::Local<v8::Value>> args = {
      TRI_V8_ASCII_STRING(isolate.get(), "testView"),
    };

    struct ExecContext: public arangodb::ExecContext {
      ExecContext(): arangodb::ExecContext(0, "", "", arangodb::auth::Level::NONE, arangodb::auth::Level::NONE) {}
    } execContext;
    auto resetExecContext = scopedPtr(ExecContext::CURRENT, &execContext);
    auto* authFeature = arangodb::AuthenticationFeature::instance();
    auto* userManager = authFeature->userManager();
    arangodb::aql::QueryRegistry queryRegistry(0); // required for UserManager::loadFromDB()
    userManager->setQueryRegistry(&queryRegistry);
    auto resetUserManager = std::shared_ptr<arangodb::auth::UserManager>(userManager, [](arangodb::auth::UserManager* ptr)->void { ptr->removeAllUsers(); });

    // not authorized (missing user)
    {
      arangodb::auth::UserMap userMap; // empty map, no user -> no permissions
      userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

      arangodb::velocypack::Builder responce;
      v8::TryCatch tryCatch(isolate.get());
      auto result = v8::Function::Cast(*fn_view)->CallAsFunction(context, fn_view, args.size(), args.data());
      CHECK((result.IsEmpty()));
      CHECK((tryCatch.HasCaught()));
      CHECK((TRI_ERROR_NO_ERROR == TRI_V8ToVPack(isolate.get(), responce, tryCatch.Exception(), false)));
      auto slice = responce.slice();
      CHECK((slice.isObject()));
      CHECK((slice.hasKey(arangodb::StaticStrings::ErrorNum) && slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() && TRI_ERROR_FORBIDDEN == slice.get(arangodb::StaticStrings::ErrorNum).getNumber<int>()));
      auto view = vocbase.lookupView("testView");
      CHECK((false == !view));
    }

    // not authorized (NONE view)
    {
      arangodb::auth::UserMap userMap;
      auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
      user.grantDatabase(vocbase.name(), arangodb::auth::Level::RO);
      user.grantCollection(vocbase.name(), "testView", arangodb::auth::Level::NONE); // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

      arangodb::velocypack::Builder responce;
      v8::TryCatch tryCatch(isolate.get());
      auto result = v8::Function::Cast(*fn_view)->CallAsFunction(context, fn_view, args.size(), args.data());
      CHECK((result.IsEmpty()));
      CHECK((tryCatch.HasCaught()));
      CHECK((TRI_ERROR_NO_ERROR == TRI_V8ToVPack(isolate.get(), responce, tryCatch.Exception(), false)));
      auto slice = responce.slice();
      CHECK((slice.isObject()));
      CHECK((slice.hasKey(arangodb::StaticStrings::ErrorNum) && slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() && TRI_ERROR_FORBIDDEN == slice.get(arangodb::StaticStrings::ErrorNum).getNumber<int>()));
      auto view = vocbase.lookupView("testView");
      CHECK((false == !view));
    }

    // authorzed (RO view)
    {
      arangodb::auth::UserMap userMap;
      auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
      user.grantDatabase(vocbase.name(), arangodb::auth::Level::RO);
      user.grantCollection(vocbase.name(), "testView", arangodb::auth::Level::RO);
      userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

      auto result = v8::Function::Cast(*fn_view)->CallAsFunction(context, fn_view, args.size(), args.data());
      CHECK((!result.IsEmpty()));
      CHECK((result.ToLocalChecked()->IsObject()));
      auto v8View = *TRI_UnwrapClass<std::shared_ptr<arangodb::LogicalView>>(result.ToLocalChecked()->ToObject(), WRP_VOCBASE_VIEW_TYPE);
      CHECK((false == !v8View));
      CHECK((std::string("testView") == v8View->name()));
      CHECK((std::string("testViewType") == v8View->type().name()));
      auto view = vocbase.lookupView("testView");
      CHECK((false == !view));
    }
  }

  // test get view (detailed)
  {
    auto createViewJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"testViewType\" }");
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalView = vocbase.createView(createViewJson->slice());
    REQUIRE((false == !logicalView));

    v8::Isolate::CreateParams isolateParams;
    ArrayBufferAllocator arrayBufferAllocator;
    isolateParams.array_buffer_allocator = &arrayBufferAllocator;
    auto isolate = std::shared_ptr<v8::Isolate>(v8::Isolate::New(isolateParams), [](v8::Isolate* p)->void { p->Dispose(); });
    REQUIRE((nullptr != isolate));
    char isolateData[64]; // 64 > sizeof(arangodb::V8PlatformFeature::IsolateData)
    std::memset(isolateData, 0, 64); // otherwise arangodb::V8PlatformFeature::isOutOfMemory(isolate) returns true
    isolate->SetData(arangodb::V8PlatformFeature::V8_INFO, isolateData); // required for TRI_VPackToV8(...) with nn-empty jSON
    v8::Isolate::Scope isolateScope(isolate.get()); // otherwise v8::Isolate::Logger() will fail (called from v8::Exception::Error)
    v8::internal::Isolate::Current()->InitializeLoggingAndCounters(); // otherwise v8::Isolate::Logger() will fail (called from v8::Exception::Error)
    v8::HandleScope handleScope(isolate.get()); // required for v8::Context::New(...), v8::ObjectTemplate::New(...) and TRI_AddMethodVocbase(...)
    auto context = v8::Context::New(isolate.get());
    v8::Context::Scope contextScope(context); // required for TRI_AddMethodVocbase(...)
    auto v8g = TRI_CreateV8Globals(isolate.get()); // create and set inside 'isolate' for use with 'TRI_GET_GLOBALS()'
    v8g->ArangoErrorTempl.Reset(isolate.get(), v8::ObjectTemplate::New(isolate.get())); // otherwise v8:-utils::CreateErrorObject(...) will fail
    v8g->_vocbase = &vocbase;
    auto arangoDBNS = v8::ObjectTemplate::New(isolate.get());
    TRI_InitV8Views(context, &vocbase, v8g, isolate.get(), arangoDBNS);

    auto arangoView = v8::Local<v8::ObjectTemplate>::New(isolate.get(), v8g->VocbaseViewTempl)->NewInstance();
    auto fn_properties = arangoView->Get(TRI_V8_ASCII_STRING(isolate.get(), "properties"));
    CHECK((fn_properties->IsFunction()));

    arangoView->SetInternalField(SLOT_CLASS_TYPE, v8::Integer::New(isolate.get(), WRP_VOCBASE_VIEW_TYPE));
    arangoView->SetInternalField(SLOT_CLASS, v8::External::New(isolate.get(), &logicalView));
    arangoView->SetInternalField(SLOT_EXTERNAL, v8::External::New(isolate.get(), &logicalView));
    std::vector<v8::Local<v8::Value>> args = {
    };

    struct ExecContext: public arangodb::ExecContext {
      ExecContext(): arangodb::ExecContext(0, "", "", arangodb::auth::Level::NONE, arangodb::auth::Level::NONE) {}
    } execContext;
    auto resetExecContext = scopedPtr(ExecContext::CURRENT, &execContext);
    auto* authFeature = arangodb::AuthenticationFeature::instance();
    auto* userManager = authFeature->userManager();
    arangodb::aql::QueryRegistry queryRegistry(0); // required for UserManager::loadFromDB()
    userManager->setQueryRegistry(&queryRegistry);
    auto resetUserManager = std::shared_ptr<arangodb::auth::UserManager>(userManager, [](arangodb::auth::UserManager* ptr)->void { ptr->removeAllUsers(); });

    // not authorized (missing user)
    {
      arangodb::auth::UserMap userMap; // empty map, no user -> no permissions
      userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

      arangodb::velocypack::Builder responce;
      v8::TryCatch tryCatch(isolate.get());
      auto result = v8::Function::Cast(*fn_properties)->CallAsFunction(context, arangoView, args.size(), args.data());
      CHECK((result.IsEmpty()));
      CHECK((tryCatch.HasCaught()));
      CHECK((TRI_ERROR_NO_ERROR == TRI_V8ToVPack(isolate.get(), responce, tryCatch.Exception(), false)));
      auto slice = responce.slice();
      CHECK((slice.isObject()));
      CHECK((slice.hasKey(arangodb::StaticStrings::ErrorNum) && slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() && TRI_ERROR_FORBIDDEN == slice.get(arangodb::StaticStrings::ErrorNum).getNumber<int>()));
      auto view = vocbase.lookupView("testView");
      CHECK((false == !view));
    }

    // not authorized (NONE view)
    {
      arangodb::auth::UserMap userMap;
      auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
      user.grantDatabase(vocbase.name(), arangodb::auth::Level::RO);
      user.grantCollection(vocbase.name(), "testView", arangodb::auth::Level::NONE); // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

      arangodb::velocypack::Builder responce;
      v8::TryCatch tryCatch(isolate.get());
      auto result = v8::Function::Cast(*fn_properties)->CallAsFunction(context, arangoView, args.size(), args.data());
      CHECK((result.IsEmpty()));
      CHECK((tryCatch.HasCaught()));
      CHECK((TRI_ERROR_NO_ERROR == TRI_V8ToVPack(isolate.get(), responce, tryCatch.Exception(), false)));
      auto slice = responce.slice();
      CHECK((slice.isObject()));
      CHECK((slice.hasKey(arangodb::StaticStrings::ErrorNum) && slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() && TRI_ERROR_FORBIDDEN == slice.get(arangodb::StaticStrings::ErrorNum).getNumber<int>()));
      auto view = vocbase.lookupView("testView");
      CHECK((false == !view));
    }

    // authorzed (RO view)
    {
      arangodb::auth::UserMap userMap;
      auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
      user.grantDatabase(vocbase.name(), arangodb::auth::Level::RO);
      user.grantCollection(vocbase.name(), "testView", arangodb::auth::Level::RO);
      userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

      arangodb::velocypack::Builder responce;
      auto result = v8::Function::Cast(*fn_properties)->CallAsFunction(context, arangoView, args.size(), args.data());
      CHECK((!result.IsEmpty()));
      CHECK((result.ToLocalChecked()->IsObject()));
      CHECK((TRI_ERROR_NO_ERROR == TRI_V8ToVPack(isolate.get(), responce, result.ToLocalChecked(), false)));
      auto slice = responce.slice();
      CHECK((slice.isObject()));
      CHECK((slice.hasKey(arangodb::StaticStrings::DataSourceName) && slice.get(arangodb::StaticStrings::DataSourceName).isString() && std::string("testView") == slice.get(arangodb::StaticStrings::DataSourceName).copyString()));
      auto view = vocbase.lookupView("testView");
      CHECK((false == !view));
    }
  }

  // test get all views
  {
    auto createView1Json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView1\", \"type\": \"testViewType\" }");
    auto createView2Json = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView2\", \"type\": \"testViewType\" }");
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalView1 = vocbase.createView(createView1Json->slice());
    REQUIRE((false == !logicalView1));
    auto logicalView2 = vocbase.createView(createView2Json->slice());
    REQUIRE((false == !logicalView2));

    v8::Isolate::CreateParams isolateParams;
    ArrayBufferAllocator arrayBufferAllocator;
    isolateParams.array_buffer_allocator = &arrayBufferAllocator;
    auto isolate = std::shared_ptr<v8::Isolate>(v8::Isolate::New(isolateParams), [](v8::Isolate* p)->void { p->Dispose(); });
    REQUIRE((nullptr != isolate));
    v8::Isolate::Scope isolateScope(isolate.get()); // otherwise v8::Isolate::Logger() will fail (called from v8::Exception::Error)
    v8::internal::Isolate::Current()->InitializeLoggingAndCounters(); // otherwise v8::Isolate::Logger() will fail (called from v8::Exception::Error)
    v8::HandleScope handleScope(isolate.get()); // required for v8::Context::New(...), v8::ObjectTemplate::New(...) and TRI_AddMethodVocbase(...)
    auto context = v8::Context::New(isolate.get());
    v8::Context::Scope contextScope(context); // required for TRI_AddMethodVocbase(...)
    auto v8g = TRI_CreateV8Globals(isolate.get()); // create and set inside 'isolate' for use with 'TRI_GET_GLOBALS()'
    v8g->ArangoErrorTempl.Reset(isolate.get(), v8::ObjectTemplate::New(isolate.get())); // otherwise v8:-utils::CreateErrorObject(...) will fail
    v8g->_vocbase = &vocbase;
    auto arangoDBNS = v8::ObjectTemplate::New(isolate.get());
    TRI_InitV8Views(context, &vocbase, v8g, isolate.get(), arangoDBNS);

    auto fn_views = arangoDBNS->NewInstance()->Get(TRI_V8_ASCII_STRING(isolate.get(), "_views"));
    CHECK((fn_views->IsFunction()));

    std::vector<v8::Local<v8::Value>> args = {
    };

    struct ExecContext: public arangodb::ExecContext {
      ExecContext(): arangodb::ExecContext(0, "", "", arangodb::auth::Level::NONE, arangodb::auth::Level::NONE) {}
    } execContext;
    auto resetExecContext = scopedPtr(ExecContext::CURRENT, &execContext);
    auto* authFeature = arangodb::AuthenticationFeature::instance();
    auto* userManager = authFeature->userManager();
    arangodb::aql::QueryRegistry queryRegistry(0); // required for UserManager::loadFromDB()
    userManager->setQueryRegistry(&queryRegistry);
    auto resetUserManager = std::shared_ptr<arangodb::auth::UserManager>(userManager, [](arangodb::auth::UserManager* ptr)->void { ptr->removeAllUsers(); });

    // not authorized (missing user)
    {
      arangodb::auth::UserMap userMap; // empty map, no user -> no permissions
      userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

      arangodb::velocypack::Builder responce;
      v8::TryCatch tryCatch(isolate.get());
      auto result = v8::Function::Cast(*fn_views)->CallAsFunction(context, fn_views, args.size(), args.data());
      CHECK((result.IsEmpty()));
      CHECK((tryCatch.HasCaught()));
      CHECK((TRI_ERROR_NO_ERROR == TRI_V8ToVPack(isolate.get(), responce, tryCatch.Exception(), false)));
      auto slice = responce.slice();
      CHECK((slice.isObject()));
      CHECK((slice.hasKey(arangodb::StaticStrings::ErrorNum) && slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() && TRI_ERROR_FORBIDDEN == slice.get(arangodb::StaticStrings::ErrorNum).getNumber<int>()));
      auto view1 = vocbase.lookupView("testView1");
      CHECK((false == !view1));
      auto view2 = vocbase.lookupView("testView2");
      CHECK((false == !view2));
    }

    // not authorized (NONE view)
    {
      arangodb::auth::UserMap userMap;
      auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
      user.grantDatabase(vocbase.name(), arangodb::auth::Level::RO);
      user.grantCollection(vocbase.name(), "testView1", arangodb::auth::Level::NONE);
      user.grantCollection(vocbase.name(), "testView2", arangodb::auth::Level::NONE); // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

      auto result = v8::Function::Cast(*fn_views)->CallAsFunction(context, fn_views, args.size(), args.data());
      CHECK((!result.IsEmpty()));
      CHECK((result.ToLocalChecked()->IsArray()));
      auto* resultArray = v8::Array::Cast(*result.ToLocalChecked());
      CHECK((0U == resultArray->Length()));
      auto view1 = vocbase.lookupView("testView1");
      CHECK((false == !view1));
      auto view2 = vocbase.lookupView("testView2");
      CHECK((false == !view2));
    }

    // authorzed (RO view)
    {
      arangodb::auth::UserMap userMap;
      auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
      user.grantDatabase(vocbase.name(), arangodb::auth::Level::RO);
      user.grantCollection(vocbase.name(), "testView1", arangodb::auth::Level::RO);
      user.grantCollection(vocbase.name(), "testView2", arangodb::auth::Level::NONE); // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

      auto result = v8::Function::Cast(*fn_views)->CallAsFunction(context, fn_views, args.size(), args.data());
      CHECK((!result.IsEmpty()));
      CHECK((result.ToLocalChecked()->IsArray()));
      auto* resultArray = v8::Array::Cast(*result.ToLocalChecked());
      CHECK((1U == resultArray->Length()));
      auto v8View = *TRI_UnwrapClass<std::shared_ptr<arangodb::LogicalView>>(resultArray->Get(0).As<v8::Object>(), WRP_VOCBASE_VIEW_TYPE);
      CHECK((false == !v8View));
      CHECK((std::string("testView1") == v8View->name()));
      CHECK((std::string("testViewType") == v8View->type().name()));
      auto view1 = vocbase.lookupView("testView1");
      CHECK((false == !view1));
      auto view2 = vocbase.lookupView("testView2");
      CHECK((false == !view2));
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate tests
////////////////////////////////////////////////////////////////////////////////

}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------