////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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

#include "src/api.h"  // must inclide V8 _before_ "catch.cpp' or CATCH() macro will be broken
#include "src/objects-inl.h"  // (required to avoid compile warnings) must inclide V8 _before_ "catch.cpp' or CATCH() macro will be broken
#include "src/objects/scope-info.h"  // must inclide V8 _before_ "catch.cpp' or CATCH() macro will be broken

#include "../IResearch/common.h"
#include "../Mocks/StorageEngineMock.h"
#include "Aql/QueryRegistry.h"
#include "gtest/gtest.h"

#if USE_ENTERPRISE
#include "Enterprise/Ldap/LdapFeature.h"
#endif

#include "GeneralServer/AuthenticationFeature.h"
#include "IResearch/IResearchAnalyzerFeature.h"
#include "IResearch/IResearchCommon.h"
#include "IResearch/VelocyPackHelper.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "RestServer/SystemDatabaseFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "Utils/ExecContext.h"
#include "V8/v8-conv.h"
#include "V8/v8-utils.h"
#include "V8/v8-vpack.h"
#include "V8Server/V8DealerFeature.h"
#include "V8Server/v8-analyzers.h"
#include "V8Server/v8-externals.h"
#include "velocypack/Iterator.h"
#include "velocypack/Parser.h"

#include "analysis/analyzers.hpp"
#include "analysis/token_attributes.hpp"

using namespace std::string_literals;

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

class EmptyAnalyzer : public irs::analysis::analyzer {
 public:
  DECLARE_ANALYZER_TYPE();
  EmptyAnalyzer() : irs::analysis::analyzer(EmptyAnalyzer::type()) {
    _attrs.emplace(_attr);
  }
  virtual irs::attribute_view const& attributes() const NOEXCEPT override {
    return _attrs;
  }
  static ptr make(irs::string_ref const&) {
    PTR_NAMED(EmptyAnalyzer, ptr);
    return ptr;
  }
  static bool normalize(irs::string_ref const& args, std::string& out) { 
    auto slice = arangodb::iresearch::slice(args);
    if (slice.isNull()) throw std::exception();
    if (slice.isNone()) return false;
    arangodb::velocypack::Builder builder;
    if (slice.isString()) {
      VPackObjectBuilder scope(&builder);
      arangodb::iresearch::addStringRef(builder, "args",
                                        arangodb::iresearch::getStringRef(slice));
    } else if (slice.isObject() && slice.hasKey("args") && slice.get("args").isString()) {
      VPackObjectBuilder scope(&builder);
      arangodb::iresearch::addStringRef(builder, "args",
                                        arangodb::iresearch::getStringRef(slice.get("args")));
    } else {
      return false;
    }
    out = builder.buffer()->toString();
    return true; 
  }
  virtual bool next() override { return false; }
  virtual bool reset(irs::string_ref const& data) override { return true; }

 private:
  irs::attribute_view _attrs;
  irs::frequency _attr;
};

DEFINE_ANALYZER_TYPE_NAMED(EmptyAnalyzer, "v8-analyzer-empty");
REGISTER_ANALYZER_VPACK(EmptyAnalyzer, EmptyAnalyzer::make, EmptyAnalyzer::normalize);

}  // namespace

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

class V8AnalyzersTest : public ::testing::Test {
 protected:
  StorageEngineMock engine;
  arangodb::application_features::ApplicationServer server;
  std::vector<std::pair<arangodb::application_features::ApplicationFeature*, bool>> features;

  V8AnalyzersTest() : engine(server), server(nullptr, nullptr) {
    arangodb::EngineSelectorFeature::ENGINE = &engine;

    arangodb::tests::v8Init();  // on-time initialize V8

    // suppress INFO {authentication} Authentication is turned on (system only), authentication for unix sockets is turned on
    // suppress WARNING {authentication} --server.jwt-secret is insecure. Use --server.jwt-secret-keyfile instead
    arangodb::LogTopic::setLogLevel(arangodb::Logger::AUTHENTICATION.name(),
                                    arangodb::LogLevel::ERR);

    // suppress log messages since tests check error conditions
    arangodb::LogTopic::setLogLevel(arangodb::iresearch::TOPIC.name(),
                                    arangodb::LogLevel::FATAL);

    // setup required application features
    features.emplace_back(new arangodb::AuthenticationFeature(server), false);  // required for VocbaseContext

#if USE_ENTERPRISE
    features.emplace_back(new arangodb::LdapFeature(server),
                          false);  // required for AuthenticationFeature with USE_ENTERPRISE
#endif

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
  }

  ~V8AnalyzersTest() {
    arangodb::application_features::ApplicationServer::server = nullptr;

    // destroy application features
    for (auto& f : features) {
      if (f.second) {
        f.first->stop();
      }
    }

    for (auto& f : features) {
      f.first->unprepare();
    }

    arangodb::LogTopic::setLogLevel(arangodb::iresearch::TOPIC.name(),
                                    arangodb::LogLevel::DEFAULT);
    arangodb::LogTopic::setLogLevel(arangodb::Logger::AUTHENTICATION.name(),
                                    arangodb::LogLevel::DEFAULT);
    arangodb::EngineSelectorFeature::ENGINE = nullptr;
  }
};

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

TEST_F(V8AnalyzersTest, test_accessors) {
  // create a new instance of an ApplicationServer and fill it with the required features
  // cannot use the existing server since its features already have some state
  std::shared_ptr<arangodb::application_features::ApplicationServer> originalServer(
      arangodb::application_features::ApplicationServer::server,
      [](arangodb::application_features::ApplicationServer* ptr) -> void {
        arangodb::application_features::ApplicationServer::server = ptr;
      });
  arangodb::application_features::ApplicationServer::server =
      nullptr;  // avoid "ApplicationServer initialized twice"
  arangodb::application_features::ApplicationServer server(nullptr, nullptr);
  arangodb::iresearch::IResearchAnalyzerFeature* analyzers;
  arangodb::DatabaseFeature* dbFeature;
  server.addFeature(new arangodb::QueryRegistryFeature(server));  // required for constructing TRI_vocbase_t
  server.addFeature(new arangodb::V8DealerFeature(server));  // required for DatabaseFeature::createDatabase(...)
  server.addFeature(dbFeature = new arangodb::DatabaseFeature(server));  // required for IResearchAnalyzerFeature::emplace(...)
  server.addFeature(analyzers = new arangodb::iresearch::IResearchAnalyzerFeature(server));  // required for running upgrade task

  // create system vocbase
  {
    auto const databases = arangodb::velocypack::Parser::fromJson(
        std::string("[ { \"name\": \"") +
        arangodb::StaticStrings::SystemDatabase + "\" } ]");
    ASSERT_TRUE((TRI_ERROR_NO_ERROR == dbFeature->loadDatabases(databases->slice())));
  }

  arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;
  ASSERT_TRUE((analyzers
                   ->emplace(result, arangodb::StaticStrings::SystemDatabase + "::testAnalyzer1",
                             "identity", VPackSlice::noneSlice())
                   .ok()));
  auto analyzer = analyzers->get(arangodb::StaticStrings::SystemDatabase +
                                 "::testAnalyzer1");
  ASSERT_TRUE((false == !analyzer));

  struct ExecContext : public arangodb::ExecContext {
    ExecContext()
        : arangodb::ExecContext(arangodb::ExecContext::Type::Default, "", "",
                                arangodb::auth::Level::NONE,
                                arangodb::auth::Level::NONE) {}
  } execContext;
  arangodb::ExecContextScope execContextScope(&execContext);
  auto* authFeature = arangodb::AuthenticationFeature::instance();
  auto* userManager = authFeature->userManager();
  arangodb::aql::QueryRegistry queryRegistry(0);  // required for UserManager::loadFromDB()
  userManager->setQueryRegistry(&queryRegistry);
  auto resetUserManager = std::shared_ptr<arangodb::auth::UserManager>(
      userManager,
      [](arangodb::auth::UserManager* ptr) -> void { ptr->removeAllUsers(); });

  // test name (authorised)
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                          arangodb::StaticStrings::SystemDatabase);
    v8::Isolate::CreateParams isolateParams;
    ArrayBufferAllocator arrayBufferAllocator;
    isolateParams.array_buffer_allocator = &arrayBufferAllocator;
    auto isolate =
        std::shared_ptr<v8::Isolate>(v8::Isolate::New(isolateParams),
                                     [](v8::Isolate* p) -> void { p->Dispose(); });
    ASSERT_TRUE((nullptr != isolate));
    v8::Isolate::Scope isolateScope(isolate.get());  // otherwise v8::Isolate::Logger() will fail (called from v8::Exception::Error)
    v8::internal::Isolate::Current()->InitializeLoggingAndCounters();  // otherwise v8::Isolate::Logger() will fail (called from v8::Exception::Error)
    v8::HandleScope handleScope(isolate.get());  // required for v8::Context::New(...), v8::ObjectTemplate::New(...) and TRI_AddMethodVocbase(...)
    auto context = v8::Context::New(isolate.get());
    v8::Context::Scope contextScope(context);  // required for TRI_AddMethodVocbase(...)
    std::unique_ptr<TRI_v8_global_t> v8g(TRI_CreateV8Globals(isolate.get()));  // create and set inside 'isolate' for use with 'TRI_GET_GLOBALS()'
    v8g->ArangoErrorTempl.Reset(isolate.get(), v8::ObjectTemplate::New(isolate.get()));  // otherwise v8:-utils::CreateErrorObject(...) will fail
    v8g->_vocbase = &vocbase;
    arangodb::iresearch::TRI_InitV8Analyzers(*v8g, isolate.get());

    auto v8Analyzer = v8::Local<v8::ObjectTemplate>::New(isolate.get(), v8g->IResearchAnalyzerTempl)
                          ->NewInstance();
    auto fn_name = v8Analyzer->Get(TRI_V8_ASCII_STRING(isolate.get(), "name"));
    EXPECT_TRUE((fn_name->IsFunction()));

    v8Analyzer->SetInternalField(SLOT_CLASS_TYPE,
                                 v8::Integer::New(isolate.get(), WRP_IRESEARCH_ANALYZER_TYPE));
    v8Analyzer->SetInternalField(SLOT_CLASS,
                                 v8::External::New(isolate.get(), analyzer.get()));
    std::vector<v8::Local<v8::Value>> args = {};

    arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
    auto& user =
        userMap
            .emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP))
            .first->second;
    user.grantDatabase(vocbase.name(), arangodb::auth::Level::RO);  // for system collections User::collectionAuthLevel(...) returns database auth::Level
    userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database

    auto result =
        v8::Function::Cast(*fn_name)->CallAsFunction(context, v8Analyzer,
                                                     static_cast<int>(args.size()),
                                                     args.data());
    EXPECT_TRUE((!result.IsEmpty()));
    EXPECT_TRUE((result.ToLocalChecked()->IsString()));
    EXPECT_TRUE((analyzer->name() ==
                 TRI_ObjectToString(isolate.get(), result.ToLocalChecked())));
  }

  // test name (not authorised)
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                          arangodb::StaticStrings::SystemDatabase);
    v8::Isolate::CreateParams isolateParams;
    ArrayBufferAllocator arrayBufferAllocator;
    isolateParams.array_buffer_allocator = &arrayBufferAllocator;
    auto isolate =
        std::shared_ptr<v8::Isolate>(v8::Isolate::New(isolateParams),
                                     [](v8::Isolate* p) -> void { p->Dispose(); });
    ASSERT_TRUE((nullptr != isolate));
    v8::Isolate::Scope isolateScope(isolate.get());  // otherwise v8::Isolate::Logger() will fail (called from v8::Exception::Error)
    v8::internal::Isolate::Current()->InitializeLoggingAndCounters();  // otherwise v8::Isolate::Logger() will fail (called from v8::Exception::Error)
    v8::HandleScope handleScope(isolate.get());  // required for v8::Context::New(...), v8::ObjectTemplate::New(...) and TRI_AddMethodVocbase(...)
    auto context = v8::Context::New(isolate.get());
    v8::Context::Scope contextScope(context);  // required for TRI_AddMethodVocbase(...)
    std::unique_ptr<TRI_v8_global_t> v8g(TRI_CreateV8Globals(isolate.get()));  // create and set inside 'isolate' for use with 'TRI_GET_GLOBALS()'
    v8g->ArangoErrorTempl.Reset(isolate.get(), v8::ObjectTemplate::New(isolate.get()));  // otherwise v8:-utils::CreateErrorObject(...) will fail
    v8g->_vocbase = &vocbase;
    arangodb::iresearch::TRI_InitV8Analyzers(*v8g, isolate.get());

    auto v8Analyzer = v8::Local<v8::ObjectTemplate>::New(isolate.get(), v8g->IResearchAnalyzerTempl)
                          ->NewInstance();
    auto fn_name = v8Analyzer->Get(TRI_V8_ASCII_STRING(isolate.get(), "name"));
    EXPECT_TRUE((fn_name->IsFunction()));

    v8Analyzer->SetInternalField(SLOT_CLASS_TYPE,
                                 v8::Integer::New(isolate.get(), WRP_IRESEARCH_ANALYZER_TYPE));
    v8Analyzer->SetInternalField(SLOT_CLASS,
                                 v8::External::New(isolate.get(), analyzer.get()));
    std::vector<v8::Local<v8::Value>> args = {};

    arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
    auto& user =
        userMap
            .emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP))
            .first->second;
    user.grantDatabase(vocbase.name(), arangodb::auth::Level::NONE);  // for system collections User::collectionAuthLevel(...) returns database auth::Level
    userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database

    arangodb::velocypack::Builder responce;
    v8::TryCatch tryCatch(isolate.get());
    auto result =
        v8::Function::Cast(*fn_name)->CallAsFunction(context, v8Analyzer,
                                                     static_cast<int>(args.size()),
                                                     args.data());
    EXPECT_TRUE((result.IsEmpty()));
    EXPECT_TRUE((tryCatch.HasCaught()));
    EXPECT_TRUE((TRI_ERROR_NO_ERROR == TRI_V8ToVPack(isolate.get(), responce,
                                                     tryCatch.Exception(), false)));
    auto slice = responce.slice();
    EXPECT_TRUE((slice.isObject()));
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::ErrorNum) &&
                 slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() &&
                 TRI_ERROR_FORBIDDEN ==
                     slice.get(arangodb::StaticStrings::ErrorNum).getNumber<int>()));
  }

  // test type (authorised)
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                          arangodb::StaticStrings::SystemDatabase);
    v8::Isolate::CreateParams isolateParams;
    ArrayBufferAllocator arrayBufferAllocator;
    isolateParams.array_buffer_allocator = &arrayBufferAllocator;
    auto isolate =
        std::shared_ptr<v8::Isolate>(v8::Isolate::New(isolateParams),
                                     [](v8::Isolate* p) -> void { p->Dispose(); });
    ASSERT_TRUE((nullptr != isolate));
    v8::Isolate::Scope isolateScope(isolate.get());  // otherwise v8::Isolate::Logger() will fail (called from v8::Exception::Error)
    v8::internal::Isolate::Current()->InitializeLoggingAndCounters();  // otherwise v8::Isolate::Logger() will fail (called from v8::Exception::Error)
    v8::HandleScope handleScope(isolate.get());  // required for v8::Context::New(...), v8::ObjectTemplate::New(...) and TRI_AddMethodVocbase(...)
    auto context = v8::Context::New(isolate.get());
    v8::Context::Scope contextScope(context);  // required for TRI_AddMethodVocbase(...)
    std::unique_ptr<TRI_v8_global_t> v8g(TRI_CreateV8Globals(isolate.get()));  // create and set inside 'isolate' for use with 'TRI_GET_GLOBALS()'
    v8g->ArangoErrorTempl.Reset(isolate.get(), v8::ObjectTemplate::New(isolate.get()));  // otherwise v8:-utils::CreateErrorObject(...) will fail
    v8g->_vocbase = &vocbase;
    arangodb::iresearch::TRI_InitV8Analyzers(*v8g, isolate.get());

    auto v8Analyzer = v8::Local<v8::ObjectTemplate>::New(isolate.get(), v8g->IResearchAnalyzerTempl)
                          ->NewInstance();
    auto fn_type = v8Analyzer->Get(TRI_V8_ASCII_STRING(isolate.get(), "type"));
    EXPECT_TRUE((fn_type->IsFunction()));

    v8Analyzer->SetInternalField(SLOT_CLASS_TYPE,
                                 v8::Integer::New(isolate.get(), WRP_IRESEARCH_ANALYZER_TYPE));
    v8Analyzer->SetInternalField(SLOT_CLASS,
                                 v8::External::New(isolate.get(), analyzer.get()));
    std::vector<v8::Local<v8::Value>> args = {};

    arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
    auto& user =
        userMap
            .emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP))
            .first->second;
    user.grantDatabase(vocbase.name(), arangodb::auth::Level::RO);  // for system collections User::collectionAuthLevel(...) returns database auth::Level
    userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database

    auto result =
        v8::Function::Cast(*fn_type)->CallAsFunction(context, v8Analyzer,
                                                     static_cast<int>(args.size()),
                                                     args.data());
    EXPECT_TRUE((!result.IsEmpty()));
    EXPECT_TRUE((result.ToLocalChecked()->IsString()));
    EXPECT_TRUE((analyzer->type() ==
                 TRI_ObjectToString(isolate.get(), result.ToLocalChecked())));
  }

  // test type (not authorised)
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                          arangodb::StaticStrings::SystemDatabase);
    v8::Isolate::CreateParams isolateParams;
    ArrayBufferAllocator arrayBufferAllocator;
    isolateParams.array_buffer_allocator = &arrayBufferAllocator;
    auto isolate =
        std::shared_ptr<v8::Isolate>(v8::Isolate::New(isolateParams),
                                     [](v8::Isolate* p) -> void { p->Dispose(); });
    ASSERT_TRUE((nullptr != isolate));
    v8::Isolate::Scope isolateScope(isolate.get());  // otherwise v8::Isolate::Logger() will fail (called from v8::Exception::Error)
    v8::internal::Isolate::Current()->InitializeLoggingAndCounters();  // otherwise v8::Isolate::Logger() will fail (called from v8::Exception::Error)
    v8::HandleScope handleScope(isolate.get());  // required for v8::Context::New(...), v8::ObjectTemplate::New(...) and TRI_AddMethodVocbase(...)
    auto context = v8::Context::New(isolate.get());
    v8::Context::Scope contextScope(context);  // required for TRI_AddMethodVocbase(...)
    std::unique_ptr<TRI_v8_global_t> v8g(TRI_CreateV8Globals(isolate.get()));  // create and set inside 'isolate' for use with 'TRI_GET_GLOBALS()'
    v8g->ArangoErrorTempl.Reset(isolate.get(), v8::ObjectTemplate::New(isolate.get()));  // otherwise v8:-utils::CreateErrorObject(...) will fail
    v8g->_vocbase = &vocbase;
    arangodb::iresearch::TRI_InitV8Analyzers(*v8g, isolate.get());

    auto v8Analyzer = v8::Local<v8::ObjectTemplate>::New(isolate.get(), v8g->IResearchAnalyzerTempl)
                          ->NewInstance();
    auto fn_type = v8Analyzer->Get(TRI_V8_ASCII_STRING(isolate.get(), "type"));
    EXPECT_TRUE((fn_type->IsFunction()));

    v8Analyzer->SetInternalField(SLOT_CLASS_TYPE,
                                 v8::Integer::New(isolate.get(), WRP_IRESEARCH_ANALYZER_TYPE));
    v8Analyzer->SetInternalField(SLOT_CLASS,
                                 v8::External::New(isolate.get(), analyzer.get()));
    std::vector<v8::Local<v8::Value>> args = {};

    arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
    auto& user =
        userMap
            .emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP))
            .first->second;
    user.grantDatabase(vocbase.name(), arangodb::auth::Level::NONE);  // for system collections User::collectionAuthLevel(...) returns database auth::Level
    userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database

    arangodb::velocypack::Builder responce;
    v8::TryCatch tryCatch(isolate.get());
    auto result =
        v8::Function::Cast(*fn_type)->CallAsFunction(context, v8Analyzer,
                                                     static_cast<int>(args.size()),
                                                     args.data());
    EXPECT_TRUE((result.IsEmpty()));
    EXPECT_TRUE((tryCatch.HasCaught()));
    EXPECT_TRUE((TRI_ERROR_NO_ERROR == TRI_V8ToVPack(isolate.get(), responce,
                                                     tryCatch.Exception(), false)));
    auto slice = responce.slice();
    EXPECT_TRUE((slice.isObject()));
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::ErrorNum) &&
                 slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() &&
                 TRI_ERROR_FORBIDDEN ==
                     slice.get(arangodb::StaticStrings::ErrorNum).getNumber<int>()));
  }

  // test properties (authorised)
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                          arangodb::StaticStrings::SystemDatabase);
    v8::Isolate::CreateParams isolateParams;
    ArrayBufferAllocator arrayBufferAllocator;
    isolateParams.array_buffer_allocator = &arrayBufferAllocator;
    auto isolate =
        std::shared_ptr<v8::Isolate>(v8::Isolate::New(isolateParams),
                                     [](v8::Isolate* p) -> void { p->Dispose(); });
    ASSERT_TRUE((nullptr != isolate));
    v8::Isolate::Scope isolateScope(isolate.get());  // otherwise v8::Isolate::Logger() will fail (called from v8::Exception::Error)
    v8::internal::Isolate::Current()->InitializeLoggingAndCounters();  // otherwise v8::Isolate::Logger() will fail (called from v8::Exception::Error)
    v8::HandleScope handleScope(isolate.get());  // required for v8::Context::New(...), v8::ObjectTemplate::New(...) and TRI_AddMethodVocbase(...)
    auto context = v8::Context::New(isolate.get());
    v8::Context::Scope contextScope(context);  // required for TRI_AddMethodVocbase(...)
    std::unique_ptr<TRI_v8_global_t> v8g(TRI_CreateV8Globals(isolate.get()));  // create and set inside 'isolate' for use with 'TRI_GET_GLOBALS()'
    v8g->ArangoErrorTempl.Reset(isolate.get(), v8::ObjectTemplate::New(isolate.get()));  // otherwise v8:-utils::CreateErrorObject(...) will fail
    v8g->_vocbase = &vocbase;
    arangodb::iresearch::TRI_InitV8Analyzers(*v8g, isolate.get());

    auto v8Analyzer = v8::Local<v8::ObjectTemplate>::New(isolate.get(), v8g->IResearchAnalyzerTempl)
                          ->NewInstance();
    auto fn_properties =
        v8Analyzer->Get(TRI_V8_ASCII_STRING(isolate.get(), "properties"));
    EXPECT_TRUE((fn_properties->IsFunction()));

    v8Analyzer->SetInternalField(SLOT_CLASS_TYPE,
                                 v8::Integer::New(isolate.get(), WRP_IRESEARCH_ANALYZER_TYPE));
    v8Analyzer->SetInternalField(SLOT_CLASS,
                                 v8::External::New(isolate.get(), analyzer.get()));
    std::vector<v8::Local<v8::Value>> args = {};

    arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
    auto& user =
        userMap
            .emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP))
            .first->second;
    user.grantDatabase(vocbase.name(), arangodb::auth::Level::RO);  // for system collections User::collectionAuthLevel(...) returns database auth::Level
    userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database

    auto result = v8::Function::Cast(*fn_properties)
                      ->CallAsFunction(context, v8Analyzer,
                                       static_cast<int>(args.size()), args.data());

    EXPECT_TRUE((!result.IsEmpty()));
    EXPECT_TRUE((result.ToLocalChecked()->IsObject()));
    VPackBuilder resultVPack;
    EXPECT_EQ(TRI_ERROR_NO_ERROR, TRI_V8ToVPack(isolate.get(), resultVPack, result.ToLocalChecked(), false));
    EXPECT_TRUE(resultVPack.slice() == VPackSlice::emptyObjectSlice());
  }

  // test properties (not authorised)
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                          arangodb::StaticStrings::SystemDatabase);
    v8::Isolate::CreateParams isolateParams;
    ArrayBufferAllocator arrayBufferAllocator;
    isolateParams.array_buffer_allocator = &arrayBufferAllocator;
    auto isolate =
        std::shared_ptr<v8::Isolate>(v8::Isolate::New(isolateParams),
                                     [](v8::Isolate* p) -> void { p->Dispose(); });
    ASSERT_TRUE((nullptr != isolate));
    v8::Isolate::Scope isolateScope(isolate.get());  // otherwise v8::Isolate::Logger() will fail (called from v8::Exception::Error)
    v8::internal::Isolate::Current()->InitializeLoggingAndCounters();  // otherwise v8::Isolate::Logger() will fail (called from v8::Exception::Error)
    v8::HandleScope handleScope(isolate.get());  // required for v8::Context::New(...), v8::ObjectTemplate::New(...) and TRI_AddMethodVocbase(...)
    auto context = v8::Context::New(isolate.get());
    v8::Context::Scope contextScope(context);  // required for TRI_AddMethodVocbase(...)
    std::unique_ptr<TRI_v8_global_t> v8g(TRI_CreateV8Globals(isolate.get()));  // create and set inside 'isolate' for use with 'TRI_GET_GLOBALS()'
    v8g->ArangoErrorTempl.Reset(isolate.get(), v8::ObjectTemplate::New(isolate.get()));  // otherwise v8:-utils::CreateErrorObject(...) will fail
    v8g->_vocbase = &vocbase;
    arangodb::iresearch::TRI_InitV8Analyzers(*v8g, isolate.get());

    auto v8Analyzer = v8::Local<v8::ObjectTemplate>::New(isolate.get(), v8g->IResearchAnalyzerTempl)
                          ->NewInstance();
    auto fn_properties =
        v8Analyzer->Get(TRI_V8_ASCII_STRING(isolate.get(), "properties"));
    EXPECT_TRUE((fn_properties->IsFunction()));

    v8Analyzer->SetInternalField(SLOT_CLASS_TYPE,
                                 v8::Integer::New(isolate.get(), WRP_IRESEARCH_ANALYZER_TYPE));
    v8Analyzer->SetInternalField(SLOT_CLASS,
                                 v8::External::New(isolate.get(), analyzer.get()));
    std::vector<v8::Local<v8::Value>> args = {};

    arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
    auto& user =
        userMap
            .emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP))
            .first->second;
    user.grantDatabase(vocbase.name(), arangodb::auth::Level::NONE);  // for system collections User::collectionAuthLevel(...) returns database auth::Level
    userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database

    arangodb::velocypack::Builder responce;
    v8::TryCatch tryCatch(isolate.get());
    auto result = v8::Function::Cast(*fn_properties)
                      ->CallAsFunction(context, v8Analyzer,
                                       static_cast<int>(args.size()), args.data());
    EXPECT_TRUE((result.IsEmpty()));
    EXPECT_TRUE((tryCatch.HasCaught()));
    EXPECT_TRUE((TRI_ERROR_NO_ERROR == TRI_V8ToVPack(isolate.get(), responce,
                                                     tryCatch.Exception(), false)));
    auto slice = responce.slice();
    EXPECT_TRUE((slice.isObject()));
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::ErrorNum) &&
                 slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() &&
                 TRI_ERROR_FORBIDDEN ==
                     slice.get(arangodb::StaticStrings::ErrorNum).getNumber<int>()));
  }

  // test features (authorised)
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                          arangodb::StaticStrings::SystemDatabase);
    v8::Isolate::CreateParams isolateParams;
    ArrayBufferAllocator arrayBufferAllocator;
    isolateParams.array_buffer_allocator = &arrayBufferAllocator;
    auto isolate =
        std::shared_ptr<v8::Isolate>(v8::Isolate::New(isolateParams),
                                     [](v8::Isolate* p) -> void { p->Dispose(); });
    ASSERT_TRUE((nullptr != isolate));
    v8::Isolate::Scope isolateScope(isolate.get());  // otherwise v8::Isolate::Logger() will fail (called from v8::Exception::Error)
    v8::internal::Isolate::Current()->InitializeLoggingAndCounters();  // otherwise v8::Isolate::Logger() will fail (called from v8::Exception::Error)
    v8::HandleScope handleScope(isolate.get());  // required for v8::Context::New(...), v8::ObjectTemplate::New(...) and TRI_AddMethodVocbase(...)
    auto context = v8::Context::New(isolate.get());
    v8::Context::Scope contextScope(context);  // required for TRI_AddMethodVocbase(...)
    std::unique_ptr<TRI_v8_global_t> v8g(TRI_CreateV8Globals(isolate.get()));  // create and set inside 'isolate' for use with 'TRI_GET_GLOBALS()'
    v8g->ArangoErrorTempl.Reset(isolate.get(), v8::ObjectTemplate::New(isolate.get()));  // otherwise v8:-utils::CreateErrorObject(...) will fail
    v8g->_vocbase = &vocbase;
    arangodb::iresearch::TRI_InitV8Analyzers(*v8g, isolate.get());

    auto v8Analyzer = v8::Local<v8::ObjectTemplate>::New(isolate.get(), v8g->IResearchAnalyzerTempl)
                          ->NewInstance();
    auto fn_features =
        v8Analyzer->Get(TRI_V8_ASCII_STRING(isolate.get(), "features"));
    EXPECT_TRUE((fn_features->IsFunction()));

    v8Analyzer->SetInternalField(SLOT_CLASS_TYPE,
                                 v8::Integer::New(isolate.get(), WRP_IRESEARCH_ANALYZER_TYPE));
    v8Analyzer->SetInternalField(SLOT_CLASS,
                                 v8::External::New(isolate.get(), analyzer.get()));
    std::vector<v8::Local<v8::Value>> args = {};

    arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
    auto& user =
        userMap
            .emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP))
            .first->second;
    user.grantDatabase(vocbase.name(), arangodb::auth::Level::RO);  // for system collections User::collectionAuthLevel(...) returns database auth::Level
    userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database

    auto result = v8::Function::Cast(*fn_features)
                      ->CallAsFunction(context, v8Analyzer,
                                       static_cast<int>(args.size()), args.data());
    EXPECT_TRUE((!result.IsEmpty()));
    EXPECT_TRUE((result.ToLocalChecked()->IsArray()));
    auto v8Result = v8::Handle<v8::Array>::Cast(result.ToLocalChecked());
    EXPECT_TRUE((analyzer->features().size() == v8Result->Length()));
  }

  // test features (not authorised)
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                          arangodb::StaticStrings::SystemDatabase);
    v8::Isolate::CreateParams isolateParams;
    ArrayBufferAllocator arrayBufferAllocator;
    isolateParams.array_buffer_allocator = &arrayBufferAllocator;
    auto isolate =
        std::shared_ptr<v8::Isolate>(v8::Isolate::New(isolateParams),
                                     [](v8::Isolate* p) -> void { p->Dispose(); });
    ASSERT_TRUE((nullptr != isolate));
    v8::Isolate::Scope isolateScope(isolate.get());  // otherwise v8::Isolate::Logger() will fail (called from v8::Exception::Error)
    v8::internal::Isolate::Current()->InitializeLoggingAndCounters();  // otherwise v8::Isolate::Logger() will fail (called from v8::Exception::Error)
    v8::HandleScope handleScope(isolate.get());  // required for v8::Context::New(...), v8::ObjectTemplate::New(...) and TRI_AddMethodVocbase(...)
    auto context = v8::Context::New(isolate.get());
    v8::Context::Scope contextScope(context);  // required for TRI_AddMethodVocbase(...)
    std::unique_ptr<TRI_v8_global_t> v8g(TRI_CreateV8Globals(isolate.get()));  // create and set inside 'isolate' for use with 'TRI_GET_GLOBALS()'
    v8g->ArangoErrorTempl.Reset(isolate.get(), v8::ObjectTemplate::New(isolate.get()));  // otherwise v8:-utils::CreateErrorObject(...) will fail
    v8g->_vocbase = &vocbase;
    arangodb::iresearch::TRI_InitV8Analyzers(*v8g, isolate.get());

    auto v8Analyzer = v8::Local<v8::ObjectTemplate>::New(isolate.get(), v8g->IResearchAnalyzerTempl)
                          ->NewInstance();
    auto fn_features =
        v8Analyzer->Get(TRI_V8_ASCII_STRING(isolate.get(), "features"));
    EXPECT_TRUE((fn_features->IsFunction()));

    v8Analyzer->SetInternalField(SLOT_CLASS_TYPE,
                                 v8::Integer::New(isolate.get(), WRP_IRESEARCH_ANALYZER_TYPE));
    v8Analyzer->SetInternalField(SLOT_CLASS,
                                 v8::External::New(isolate.get(), analyzer.get()));
    std::vector<v8::Local<v8::Value>> args = {};

    arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
    auto& user =
        userMap
            .emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP))
            .first->second;
    user.grantDatabase(vocbase.name(), arangodb::auth::Level::NONE);  // for system collections User::collectionAuthLevel(...) returns database auth::Level
    userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database

    arangodb::velocypack::Builder responce;
    v8::TryCatch tryCatch(isolate.get());
    auto result = v8::Function::Cast(*fn_features)
                      ->CallAsFunction(context, v8Analyzer,
                                       static_cast<int>(args.size()), args.data());
    EXPECT_TRUE((result.IsEmpty()));
    EXPECT_TRUE((tryCatch.HasCaught()));
    EXPECT_TRUE((TRI_ERROR_NO_ERROR == TRI_V8ToVPack(isolate.get(), responce,
                                                     tryCatch.Exception(), false)));
    auto slice = responce.slice();
    EXPECT_TRUE((slice.isObject()));
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::ErrorNum) &&
                 slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() &&
                 TRI_ERROR_FORBIDDEN ==
                     slice.get(arangodb::StaticStrings::ErrorNum).getNumber<int>()));
  }
}

TEST_F(V8AnalyzersTest, test_create) {
  // create a new instance of an ApplicationServer and fill it with the required features
  // cannot use the existing server since its features already have some state
  std::shared_ptr<arangodb::application_features::ApplicationServer> originalServer(
      arangodb::application_features::ApplicationServer::server,
      [](arangodb::application_features::ApplicationServer* ptr) -> void {
        arangodb::application_features::ApplicationServer::server = ptr;
      });
  arangodb::application_features::ApplicationServer::server =
      nullptr;  // avoid "ApplicationServer initialized twice"
  arangodb::application_features::ApplicationServer server(nullptr, nullptr);
  arangodb::iresearch::IResearchAnalyzerFeature* analyzers;
  arangodb::DatabaseFeature* dbFeature;
  arangodb::SystemDatabaseFeature* sysDatabase;
  server.addFeature(new arangodb::QueryRegistryFeature(server));  // required for constructing TRI_vocbase_t
  server.addFeature(new arangodb::V8DealerFeature(server));  // required for DatabaseFeature::createDatabase(...)
  server.addFeature(dbFeature = new arangodb::DatabaseFeature(server));  // required for IResearchAnalyzerFeature::emplace(...)
  server.addFeature(analyzers = new arangodb::iresearch::IResearchAnalyzerFeature(server));  // required for running upgrade task
  server.addFeature(sysDatabase = new arangodb::SystemDatabaseFeature(server));  // required for IResearchAnalyzerFeature::start()

  // create system vocbase
  {
    auto const databases = arangodb::velocypack::Parser::fromJson(
        std::string("[ { \"name\": \"") +
        arangodb::StaticStrings::SystemDatabase + "\" } ]");
    ASSERT_TRUE((TRI_ERROR_NO_ERROR == dbFeature->loadDatabases(databases->slice())));
    sysDatabase->start();  // get system database from DatabaseFeature
  }

  arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;

  {
    const auto name = arangodb::StaticStrings::SystemDatabase + "::testAnalyzer1";
    ASSERT_TRUE(analyzers->emplace(result, name, "identity", 
                                   VPackSlice::noneSlice())
                           .ok());
  }

  {
    const auto name = arangodb::StaticStrings::SystemDatabase + "::emptyAnalyzer";
    ASSERT_TRUE(analyzers->emplace(result, name, "v8-analyzer-empty",
                                   VPackParser::fromJson("\"en\"")->slice(), 
                                   irs::flags{irs::frequency::type()}).ok());
  }

  struct ExecContext : public arangodb::ExecContext {
    ExecContext()
        : arangodb::ExecContext(arangodb::ExecContext::Type::Default, "", "",
                                arangodb::auth::Level::NONE,
                                arangodb::auth::Level::NONE) {}
  } execContext;
  arangodb::ExecContextScope execContextScope(&execContext);
  auto* authFeature = arangodb::AuthenticationFeature::instance();
  auto* userManager = authFeature->userManager();
  arangodb::aql::QueryRegistry queryRegistry(0);  // required for UserManager::loadFromDB()
  userManager->setQueryRegistry(&queryRegistry);
  auto resetUserManager = std::shared_ptr<arangodb::auth::UserManager>(
      userManager,
      [](arangodb::auth::UserManager* ptr) -> void { ptr->removeAllUsers(); });

  // invalid params (no args)
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                          arangodb::StaticStrings::SystemDatabase);
    v8::Isolate::CreateParams isolateParams;
    ArrayBufferAllocator arrayBufferAllocator;
    isolateParams.array_buffer_allocator = &arrayBufferAllocator;
    auto isolate =
        std::shared_ptr<v8::Isolate>(v8::Isolate::New(isolateParams),
                                     [](v8::Isolate* p) -> void { p->Dispose(); });
    ASSERT_TRUE((nullptr != isolate));
    v8::Isolate::Scope isolateScope(isolate.get());  // otherwise v8::Isolate::Logger() will fail (called from v8::Exception::Error)
    v8::internal::Isolate::Current()->InitializeLoggingAndCounters();  // otherwise v8::Isolate::Logger() will fail (called from v8::Exception::Error)
    v8::HandleScope handleScope(isolate.get());  // required for v8::Context::New(...), v8::ObjectTemplate::New(...) and TRI_AddMethodVocbase(...)
    auto context = v8::Context::New(isolate.get());
    v8::Context::Scope contextScope(context);  // required for TRI_AddMethodVocbase(...)
    std::unique_ptr<TRI_v8_global_t> v8g(TRI_CreateV8Globals(isolate.get()));  // create and set inside 'isolate' for use with 'TRI_GET_GLOBALS()'
    v8g->ArangoErrorTempl.Reset(isolate.get(), v8::ObjectTemplate::New(isolate.get()));  // otherwise v8:-utils::CreateErrorObject(...) will fail
    v8g->_vocbase = &vocbase;
    arangodb::iresearch::TRI_InitV8Analyzers(*v8g, isolate.get());

    auto v8Analyzers = v8::Local<v8::ObjectTemplate>::New(isolate.get(), v8g->IResearchAnalyzersTempl)
                           ->NewInstance();
    auto fn_create =
        v8Analyzers->Get(TRI_V8_ASCII_STRING(isolate.get(), "save"));
    EXPECT_TRUE((fn_create->IsFunction()));

    std::vector<v8::Local<v8::Value>> args = {};

    arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
    auto& user =
        userMap
            .emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP))
            .first->second;
    user.grantDatabase(vocbase.name(), arangodb::auth::Level::RW);  // for system collections User::collectionAuthLevel(...) returns database auth::Level
    userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database

    arangodb::velocypack::Builder responce;
    v8::TryCatch tryCatch(isolate.get());
    auto result = v8::Function::Cast(*fn_create)
                      ->CallAsFunction(context, fn_create,
                                       static_cast<int>(args.size()), args.data());
    EXPECT_TRUE((result.IsEmpty()));
    EXPECT_TRUE((tryCatch.HasCaught()));
    EXPECT_TRUE((TRI_ERROR_NO_ERROR == TRI_V8ToVPack(isolate.get(), responce,
                                                     tryCatch.Exception(), false)));
    auto slice = responce.slice();
    EXPECT_TRUE((slice.isObject()));
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::ErrorNum) &&
                 slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() &&
                 TRI_ERROR_BAD_PARAMETER ==
                     slice.get(arangodb::StaticStrings::ErrorNum).getNumber<int>()));
  }

  // invalid params (invalid type)
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                          arangodb::StaticStrings::SystemDatabase);
    v8::Isolate::CreateParams isolateParams;
    ArrayBufferAllocator arrayBufferAllocator;
    isolateParams.array_buffer_allocator = &arrayBufferAllocator;
    auto isolate =
        std::shared_ptr<v8::Isolate>(v8::Isolate::New(isolateParams),
                                     [](v8::Isolate* p) -> void { p->Dispose(); });
    ASSERT_TRUE((nullptr != isolate));
    v8::Isolate::Scope isolateScope(isolate.get());  // otherwise v8::Isolate::Logger() will fail (called from v8::Exception::Error)
    v8::internal::Isolate::Current()->InitializeLoggingAndCounters();  // otherwise v8::Isolate::Logger() will fail (called from v8::Exception::Error)
    v8::HandleScope handleScope(isolate.get());  // required for v8::Context::New(...), v8::ObjectTemplate::New(...) and TRI_AddMethodVocbase(...)
    auto context = v8::Context::New(isolate.get());
    v8::Context::Scope contextScope(context);  // required for TRI_AddMethodVocbase(...)
    std::unique_ptr<TRI_v8_global_t> v8g(TRI_CreateV8Globals(isolate.get()));  // create and set inside 'isolate' for use with 'TRI_GET_GLOBALS()'
    v8g->ArangoErrorTempl.Reset(isolate.get(), v8::ObjectTemplate::New(isolate.get()));  // otherwise v8:-utils::CreateErrorObject(...) will fail
    v8g->_vocbase = &vocbase;
    arangodb::iresearch::TRI_InitV8Analyzers(*v8g, isolate.get());

    auto v8Analyzers = v8::Local<v8::ObjectTemplate>::New(isolate.get(), v8g->IResearchAnalyzersTempl)
                           ->NewInstance();
    auto fn_create =
        v8Analyzers->Get(TRI_V8_ASCII_STRING(isolate.get(), "save"));
    EXPECT_TRUE((fn_create->IsFunction()));

    std::vector<v8::Local<v8::Value>> args = {
        TRI_V8_STD_STRING(isolate.get(), arangodb::StaticStrings::SystemDatabase + "::testAnalyzer2"),
        v8::True(isolate.get()),
    };

    arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
    auto& user =
        userMap
            .emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP))
            .first->second;
    user.grantDatabase(vocbase.name(), arangodb::auth::Level::RW);  // for system collections User::collectionAuthLevel(...) returns database auth::Level
    userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database

    arangodb::velocypack::Builder responce;
    v8::TryCatch tryCatch(isolate.get());
    auto result = v8::Function::Cast(*fn_create)
                      ->CallAsFunction(context, fn_create,
                                       static_cast<int>(args.size()), args.data());
    EXPECT_TRUE((result.IsEmpty()));
    EXPECT_TRUE((tryCatch.HasCaught()));
    EXPECT_TRUE((TRI_ERROR_NO_ERROR == TRI_V8ToVPack(isolate.get(), responce,
                                                     tryCatch.Exception(), false)));
    auto slice = responce.slice();
    EXPECT_TRUE((slice.isObject()));
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::ErrorNum) &&
                 slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() &&
                 TRI_ERROR_BAD_PARAMETER ==
                     slice.get(arangodb::StaticStrings::ErrorNum).getNumber<int>()));
  }

  // invalid params (invalid name)
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                          arangodb::StaticStrings::SystemDatabase);
    v8::Isolate::CreateParams isolateParams;
    ArrayBufferAllocator arrayBufferAllocator;
    isolateParams.array_buffer_allocator = &arrayBufferAllocator;
    auto isolate =
        std::shared_ptr<v8::Isolate>(v8::Isolate::New(isolateParams),
                                     [](v8::Isolate* p) -> void { p->Dispose(); });
    ASSERT_TRUE((nullptr != isolate));
    v8::Isolate::Scope isolateScope(isolate.get());  // otherwise v8::Isolate::Logger() will fail (called from v8::Exception::Error)
    v8::internal::Isolate::Current()->InitializeLoggingAndCounters();  // otherwise v8::Isolate::Logger() will fail (called from v8::Exception::Error)
    v8::HandleScope handleScope(isolate.get());  // required for v8::Context::New(...), v8::ObjectTemplate::New(...) and TRI_AddMethodVocbase(...)
    auto context = v8::Context::New(isolate.get());
    v8::Context::Scope contextScope(context);  // required for TRI_AddMethodVocbase(...)
    std::unique_ptr<TRI_v8_global_t> v8g(TRI_CreateV8Globals(isolate.get()));  // create and set inside 'isolate' for use with 'TRI_GET_GLOBALS()'
    v8g->ArangoErrorTempl.Reset(isolate.get(), v8::ObjectTemplate::New(isolate.get()));  // otherwise v8:-utils::CreateErrorObject(...) will fail
    v8g->_vocbase = &vocbase;
    arangodb::iresearch::TRI_InitV8Analyzers(*v8g, isolate.get());

    auto v8Analyzers = v8::Local<v8::ObjectTemplate>::New(isolate.get(), v8g->IResearchAnalyzersTempl)
                           ->NewInstance();
    auto fn_create =
        v8Analyzers->Get(TRI_V8_ASCII_STRING(isolate.get(), "save"));
    EXPECT_TRUE((fn_create->IsFunction()));

    std::vector<v8::Local<v8::Value>> args = {
        TRI_V8_STD_STRING(isolate.get(), arangodb::StaticStrings::SystemDatabase + "::testAnalyzer2"),
        TRI_V8_ASCII_STRING(isolate.get(), "identity"),
        v8::True(isolate.get()),
    };

    arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
    auto& user =
        userMap
            .emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP))
            .first->second;
    user.grantDatabase(vocbase.name(), arangodb::auth::Level::RW);  // for system collections User::collectionAuthLevel(...) returns database auth::Level
    userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database

    arangodb::velocypack::Builder responce;
    v8::TryCatch tryCatch(isolate.get());
    auto result = v8::Function::Cast(*fn_create)
                      ->CallAsFunction(context, fn_create,
                                       static_cast<int>(args.size()), args.data());
    EXPECT_TRUE((result.IsEmpty()));
    EXPECT_TRUE((tryCatch.HasCaught()));
    EXPECT_TRUE((TRI_ERROR_NO_ERROR == TRI_V8ToVPack(isolate.get(), responce,
                                                     tryCatch.Exception(), false)));
    auto slice = responce.slice();
    EXPECT_TRUE((slice.isObject()));
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::ErrorNum) &&
                 slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() &&
                 TRI_ERROR_BAD_PARAMETER ==
                     slice.get(arangodb::StaticStrings::ErrorNum).getNumber<int>()));
  }

  // invalid params (invalid name)
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                          arangodb::StaticStrings::SystemDatabase);
    v8::Isolate::CreateParams isolateParams;
    ArrayBufferAllocator arrayBufferAllocator;
    isolateParams.array_buffer_allocator = &arrayBufferAllocator;
    auto isolate =
        std::shared_ptr<v8::Isolate>(v8::Isolate::New(isolateParams),
                                     [](v8::Isolate* p) -> void { p->Dispose(); });
    ASSERT_TRUE((nullptr != isolate));
    v8::Isolate::Scope isolateScope(isolate.get());  // otherwise v8::Isolate::Logger() will fail (called from v8::Exception::Error)
    v8::internal::Isolate::Current()->InitializeLoggingAndCounters();  // otherwise v8::Isolate::Logger() will fail (called from v8::Exception::Error)
    v8::HandleScope handleScope(isolate.get());  // required for v8::Context::New(...), v8::ObjectTemplate::New(...) and TRI_AddMethodVocbase(...)
    auto context = v8::Context::New(isolate.get());
    v8::Context::Scope contextScope(context);  // required for TRI_AddMethodVocbase(...)
    std::unique_ptr<TRI_v8_global_t> v8g(TRI_CreateV8Globals(isolate.get()));  // create and set inside 'isolate' for use with 'TRI_GET_GLOBALS()'
    v8g->ArangoErrorTempl.Reset(isolate.get(), v8::ObjectTemplate::New(isolate.get()));  // otherwise v8:-utils::CreateErrorObject(...) will fail
    v8g->_vocbase = &vocbase;
    arangodb::iresearch::TRI_InitV8Analyzers(*v8g, isolate.get());

    auto v8Analyzers = v8::Local<v8::ObjectTemplate>::New(isolate.get(), v8g->IResearchAnalyzersTempl)
                           ->NewInstance();
    auto fn_create =
        v8Analyzers->Get(TRI_V8_ASCII_STRING(isolate.get(), "save"));
    EXPECT_TRUE((fn_create->IsFunction()));

    std::vector<v8::Local<v8::Value>> args = {
        TRI_V8_STD_STRING(isolate.get(), "::testAnalyzer2"s),
        TRI_V8_ASCII_STRING(isolate.get(), "identity"),
        v8::True(isolate.get()),
    };

    arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
    auto& user =
        userMap
            .emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP))
            .first->second;
    user.grantDatabase(vocbase.name(), arangodb::auth::Level::RW);  // for system collections User::collectionAuthLevel(...) returns database auth::Level
    userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database

    arangodb::velocypack::Builder responce;
    v8::TryCatch tryCatch(isolate.get());
    auto result = v8::Function::Cast(*fn_create)
                      ->CallAsFunction(context, fn_create,
                                       static_cast<int>(args.size()), args.data());
    EXPECT_TRUE((result.IsEmpty()));
    EXPECT_TRUE((tryCatch.HasCaught()));
    EXPECT_TRUE((TRI_ERROR_NO_ERROR == TRI_V8ToVPack(isolate.get(), responce,
                                                     tryCatch.Exception(), false)));
    auto slice = responce.slice();
    EXPECT_TRUE((slice.isObject()));
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::ErrorNum) &&
                 slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() &&
                 TRI_ERROR_BAD_PARAMETER ==
                     slice.get(arangodb::StaticStrings::ErrorNum).getNumber<int>()));
  }

  // invalid params (invalid name)
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                          arangodb::StaticStrings::SystemDatabase);
    v8::Isolate::CreateParams isolateParams;
    ArrayBufferAllocator arrayBufferAllocator;
    isolateParams.array_buffer_allocator = &arrayBufferAllocator;
    auto isolate =
        std::shared_ptr<v8::Isolate>(v8::Isolate::New(isolateParams),
                                     [](v8::Isolate* p) -> void { p->Dispose(); });
    ASSERT_TRUE((nullptr != isolate));
    v8::Isolate::Scope isolateScope(isolate.get());  // otherwise v8::Isolate::Logger() will fail (called from v8::Exception::Error)
    v8::internal::Isolate::Current()->InitializeLoggingAndCounters();  // otherwise v8::Isolate::Logger() will fail (called from v8::Exception::Error)
    v8::HandleScope handleScope(isolate.get());  // required for v8::Context::New(...), v8::ObjectTemplate::New(...) and TRI_AddMethodVocbase(...)
    auto context = v8::Context::New(isolate.get());
    v8::Context::Scope contextScope(context);  // required for TRI_AddMethodVocbase(...)
    std::unique_ptr<TRI_v8_global_t> v8g(TRI_CreateV8Globals(isolate.get()));  // create and set inside 'isolate' for use with 'TRI_GET_GLOBALS()'
    v8g->ArangoErrorTempl.Reset(isolate.get(), v8::ObjectTemplate::New(isolate.get()));  // otherwise v8:-utils::CreateErrorObject(...) will fail
    v8g->_vocbase = &vocbase;
    arangodb::iresearch::TRI_InitV8Analyzers(*v8g, isolate.get());

    auto v8Analyzers = v8::Local<v8::ObjectTemplate>::New(isolate.get(), v8g->IResearchAnalyzersTempl)
                           ->NewInstance();
    auto fn_create =
        v8Analyzers->Get(TRI_V8_ASCII_STRING(isolate.get(), "save"));
    EXPECT_TRUE((fn_create->IsFunction()));

    std::vector<v8::Local<v8::Value>> args = {
        TRI_V8_ASCII_STRING(isolate.get(), "unknownVocbase::testAnalyzer"),
        TRI_V8_ASCII_STRING(isolate.get(), "identity"),
        v8::Null(isolate.get()),
    };

    arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
    auto& user =
        userMap
            .emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP))
            .first->second;
    user.grantDatabase(vocbase.name(), arangodb::auth::Level::RW);  // for system collections User::collectionAuthLevel(...) returns database auth::Level
    userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database

    arangodb::velocypack::Builder responce;
    v8::TryCatch tryCatch(isolate.get());
    auto result = v8::Function::Cast(*fn_create)
                      ->CallAsFunction(context, fn_create,
                                       static_cast<int>(args.size()), args.data());
    EXPECT_TRUE((result.IsEmpty()));
    EXPECT_TRUE((tryCatch.HasCaught()));
    EXPECT_TRUE((TRI_ERROR_NO_ERROR == TRI_V8ToVPack(isolate.get(), responce,
                                                     tryCatch.Exception(), false)));
    auto slice = responce.slice();
    EXPECT_TRUE((slice.isObject()));
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::ErrorNum) &&
                 slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() &&
                 TRI_ERROR_BAD_PARAMETER == slice.get(arangodb::StaticStrings::ErrorNum).getNumber<int>()));
  }

  // name collision
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                          arangodb::StaticStrings::SystemDatabase);
    v8::Isolate::CreateParams isolateParams;
    ArrayBufferAllocator arrayBufferAllocator;
    isolateParams.array_buffer_allocator = &arrayBufferAllocator;
    auto isolate =
        std::shared_ptr<v8::Isolate>(v8::Isolate::New(isolateParams),
                                     [](v8::Isolate* p) -> void { p->Dispose(); });
    ASSERT_TRUE((nullptr != isolate));
    v8::Isolate::Scope isolateScope(isolate.get());  // otherwise v8::Isolate::Logger() will fail (called from v8::Exception::Error)
    v8::internal::Isolate::Current()->InitializeLoggingAndCounters();  // otherwise v8::Isolate::Logger() will fail (called from v8::Exception::Error)
    v8::HandleScope handleScope(isolate.get());  // required for v8::Context::New(...), v8::ObjectTemplate::New(...) and TRI_AddMethodVocbase(...)
    auto context = v8::Context::New(isolate.get());
    v8::Context::Scope contextScope(context);  // required for TRI_AddMethodVocbase(...)
    std::unique_ptr<TRI_v8_global_t> v8g(TRI_CreateV8Globals(isolate.get()));  // create and set inside 'isolate' for use with 'TRI_GET_GLOBALS()'
    v8g->ArangoErrorTempl.Reset(isolate.get(), v8::ObjectTemplate::New(isolate.get()));  // otherwise v8:-utils::CreateErrorObject(...) will fail
    v8g->_vocbase = &vocbase;
    arangodb::iresearch::TRI_InitV8Analyzers(*v8g, isolate.get());

    auto v8Analyzers = v8::Local<v8::ObjectTemplate>::New(isolate.get(), v8g->IResearchAnalyzersTempl)
                           ->NewInstance();
    auto fn_create =
        v8Analyzers->Get(TRI_V8_ASCII_STRING(isolate.get(), "save"));
    EXPECT_TRUE((fn_create->IsFunction()));

    std::vector<v8::Local<v8::Value>> args = {
        TRI_V8_STD_STRING(isolate.get(), "emptyAnalyzer"s),
        TRI_V8_ASCII_STRING(isolate.get(), "v8-analyzer-empty"),
        TRI_V8_ASCII_STRING(isolate.get(), "\"abc\""),
    };

    arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
    auto& user =
        userMap
            .emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP))
            .first->second;
    user.grantDatabase(vocbase.name(), arangodb::auth::Level::RW);  // for system collections User::collectionAuthLevel(...) returns database auth::Level
    userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database

    arangodb::velocypack::Builder responce;
    v8::TryCatch tryCatch(isolate.get());
    auto result = v8::Function::Cast(*fn_create)
                      ->CallAsFunction(context, fn_create,
                                       static_cast<int>(args.size()), args.data());
    EXPECT_TRUE((result.IsEmpty()));
    EXPECT_TRUE((tryCatch.HasCaught()));
    EXPECT_TRUE((TRI_ERROR_NO_ERROR == TRI_V8ToVPack(isolate.get(), responce,
                                                     tryCatch.Exception(), false)));
    auto slice = responce.slice();
    EXPECT_TRUE((slice.isObject()));
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::ErrorNum) &&
                 slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() &&
                 TRI_ERROR_BAD_PARAMETER ==
                     slice.get(arangodb::StaticStrings::ErrorNum).getNumber<int>()));
  }

  // duplicate matching
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                          arangodb::StaticStrings::SystemDatabase);
    v8::Isolate::CreateParams isolateParams;
    ArrayBufferAllocator arrayBufferAllocator;
    isolateParams.array_buffer_allocator = &arrayBufferAllocator;
    auto isolate =
        std::shared_ptr<v8::Isolate>(v8::Isolate::New(isolateParams),
                                     [](v8::Isolate* p) -> void { p->Dispose(); });
    ASSERT_TRUE((nullptr != isolate));
    v8::Isolate::Scope isolateScope(isolate.get());  // otherwise v8::Isolate::Logger() will fail (called from v8::Exception::Error)
    v8::internal::Isolate::Current()->InitializeLoggingAndCounters();  // otherwise v8::Isolate::Logger() will fail (called from v8::Exception::Error)
    v8::HandleScope handleScope(isolate.get());  // required for v8::Context::New(...), v8::ObjectTemplate::New(...) and TRI_AddMethodVocbase(...)
    auto context = v8::Context::New(isolate.get());
    v8::Context::Scope contextScope(context);  // required for TRI_AddMethodVocbase(...)
    std::unique_ptr<TRI_v8_global_t> v8g(TRI_CreateV8Globals(isolate.get()));  // create and set inside 'isolate' for use with 'TRI_GET_GLOBALS()'
    v8g->ArangoErrorTempl.Reset(isolate.get(), v8::ObjectTemplate::New(isolate.get()));  // otherwise v8:-utils::CreateErrorObject(...) will fail
    v8g->_vocbase = &vocbase;
    arangodb::iresearch::TRI_InitV8Analyzers(*v8g, isolate.get());

    auto v8Analyzers = v8::Local<v8::ObjectTemplate>::New(isolate.get(), v8g->IResearchAnalyzersTempl)
                           ->NewInstance();
    auto fn_create =
        v8Analyzers->Get(TRI_V8_ASCII_STRING(isolate.get(), "save"));
    EXPECT_TRUE((fn_create->IsFunction()));

    std::vector<v8::Local<v8::Value>> args = {
        TRI_V8_STD_STRING(isolate.get(), "testAnalyzer1"s),
        TRI_V8_ASCII_STRING(isolate.get(), "identity"),
        v8::Null(isolate.get()),
    };

    arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
    auto& user =
        userMap
            .emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP))
            .first->second;
    user.grantDatabase(vocbase.name(), arangodb::auth::Level::RW);  // for system collections User::collectionAuthLevel(...) returns database auth::Level
    userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database

    auto result = v8::Function::Cast(*fn_create)
                      ->CallAsFunction(context, fn_create,
                                       static_cast<int>(args.size()), args.data());
    EXPECT_TRUE((!result.IsEmpty()));
    EXPECT_TRUE((result.ToLocalChecked()->IsObject()));
    auto* v8Analyzer = TRI_UnwrapClass<arangodb::iresearch::IResearchAnalyzerFeature::AnalyzerPool>(
        result.ToLocalChecked()->ToObject(TRI_IGETC).FromMaybe(v8::Local<v8::Object>()),
        WRP_IRESEARCH_ANALYZER_TYPE, TRI_IGETC);
    EXPECT_TRUE((false == !v8Analyzer));
    EXPECT_TRUE((arangodb::StaticStrings::SystemDatabase + "::testAnalyzer1" == v8Analyzer->name()));
    EXPECT_EQ("identity", v8Analyzer->type());
    EXPECT_EQ(VPackSlice::emptyObjectSlice(), v8Analyzer->properties());
    EXPECT_TRUE(v8Analyzer->features().empty());
    auto analyzer = analyzers->get(arangodb::StaticStrings::SystemDatabase + "::testAnalyzer1");
    EXPECT_TRUE((false == !analyzer));
  }

  // not authorised
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                          arangodb::StaticStrings::SystemDatabase);
    v8::Isolate::CreateParams isolateParams;
    ArrayBufferAllocator arrayBufferAllocator;
    isolateParams.array_buffer_allocator = &arrayBufferAllocator;
    auto isolate =
        std::shared_ptr<v8::Isolate>(v8::Isolate::New(isolateParams),
                                     [](v8::Isolate* p) -> void { p->Dispose(); });
    ASSERT_TRUE((nullptr != isolate));
    v8::Isolate::Scope isolateScope(isolate.get());  // otherwise v8::Isolate::Logger() will fail (called from v8::Exception::Error)
    v8::internal::Isolate::Current()->InitializeLoggingAndCounters();  // otherwise v8::Isolate::Logger() will fail (called from v8::Exception::Error)
    v8::HandleScope handleScope(isolate.get());  // required for v8::Context::New(...), v8::ObjectTemplate::New(...) and TRI_AddMethodVocbase(...)
    auto context = v8::Context::New(isolate.get());
    v8::Context::Scope contextScope(context);  // required for TRI_AddMethodVocbase(...)
    std::unique_ptr<TRI_v8_global_t> v8g(TRI_CreateV8Globals(isolate.get()));  // create and set inside 'isolate' for use with 'TRI_GET_GLOBALS()'
    v8g->ArangoErrorTempl.Reset(isolate.get(), v8::ObjectTemplate::New(isolate.get()));  // otherwise v8:-utils::CreateErrorObject(...) will fail
    v8g->_vocbase = &vocbase;
    arangodb::iresearch::TRI_InitV8Analyzers(*v8g, isolate.get());

    auto v8Analyzers = v8::Local<v8::ObjectTemplate>::New(isolate.get(), v8g->IResearchAnalyzersTempl)
                           ->NewInstance();
    auto fn_create =
        v8Analyzers->Get(TRI_V8_ASCII_STRING(isolate.get(), "save"));
    EXPECT_TRUE((fn_create->IsFunction()));

    std::vector<v8::Local<v8::Value>> args = {
        TRI_V8_STD_STRING(isolate.get(), "testAnalyzer2"s),
        TRI_V8_ASCII_STRING(isolate.get(), "identity"),
        TRI_V8_ASCII_STRING(isolate.get(), "\"abc\""),
    };

    arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
    auto& user =
        userMap
            .emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP))
            .first->second;
    user.grantDatabase(vocbase.name(), arangodb::auth::Level::RO);  // for system collections User::collectionAuthLevel(...) returns database auth::Level
    userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database

    arangodb::velocypack::Builder responce;
    v8::TryCatch tryCatch(isolate.get());
    auto result = v8::Function::Cast(*fn_create)
                      ->CallAsFunction(context, fn_create,
                                       static_cast<int>(args.size()), args.data());
    EXPECT_TRUE((result.IsEmpty()));
    EXPECT_TRUE((tryCatch.HasCaught()));
    EXPECT_TRUE((TRI_ERROR_NO_ERROR == TRI_V8ToVPack(isolate.get(), responce,
                                                     tryCatch.Exception(), false)));
    auto slice = responce.slice();
    EXPECT_TRUE((slice.isObject()));
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::ErrorNum) &&
                 slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() &&
                 TRI_ERROR_FORBIDDEN ==
                     slice.get(arangodb::StaticStrings::ErrorNum).getNumber<int>()));
  }

  // successful creation
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                          arangodb::StaticStrings::SystemDatabase);
    v8::Isolate::CreateParams isolateParams;
    ArrayBufferAllocator arrayBufferAllocator;
    isolateParams.array_buffer_allocator = &arrayBufferAllocator;
    auto isolate =
        std::shared_ptr<v8::Isolate>(v8::Isolate::New(isolateParams),
                                     [](v8::Isolate* p) -> void { p->Dispose(); });
    ASSERT_TRUE((nullptr != isolate));
    v8::Isolate::Scope isolateScope(isolate.get());  // otherwise v8::Isolate::Logger() will fail (called from v8::Exception::Error)
    v8::internal::Isolate::Current()->InitializeLoggingAndCounters();  // otherwise v8::Isolate::Logger() will fail (called from v8::Exception::Error)
    v8::HandleScope handleScope(isolate.get());  // required for v8::Context::New(...), v8::ObjectTemplate::New(...) and TRI_AddMethodVocbase(...)
    auto context = v8::Context::New(isolate.get());
    v8::Context::Scope contextScope(context);  // required for TRI_AddMethodVocbase(...)
    std::unique_ptr<TRI_v8_global_t> v8g(TRI_CreateV8Globals(isolate.get()));  // create and set inside 'isolate' for use with 'TRI_GET_GLOBALS()'
    v8g->ArangoErrorTempl.Reset(isolate.get(), v8::ObjectTemplate::New(isolate.get()));  // otherwise v8:-utils::CreateErrorObject(...) will fail
    v8g->_vocbase = &vocbase;
    arangodb::iresearch::TRI_InitV8Analyzers(*v8g, isolate.get());

    auto v8Analyzers = v8::Local<v8::ObjectTemplate>::New(isolate.get(), v8g->IResearchAnalyzersTempl)
                           ->NewInstance();
    auto fn_create =
        v8Analyzers->Get(TRI_V8_ASCII_STRING(isolate.get(), "save"));
    EXPECT_TRUE((fn_create->IsFunction()));

    std::vector<v8::Local<v8::Value>> args = {
        TRI_V8_STD_STRING(isolate.get(), "testAnalyzer2"s),
        TRI_V8_ASCII_STRING(isolate.get(), "identity"),
        TRI_V8_ASCII_STRING(isolate.get(), "\"abc\"")
    };

    arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
    auto& user =
        userMap
            .emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP))
            .first->second;
    user.grantDatabase(vocbase.name(), arangodb::auth::Level::RW);  // for system collections User::collectionAuthLevel(...) returns database auth::Level
    userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database

    auto result = v8::Function::Cast(*fn_create)
                      ->CallAsFunction(context, fn_create,
                                       static_cast<int>(args.size()), args.data());
    EXPECT_TRUE((!result.IsEmpty()));
    EXPECT_TRUE((result.ToLocalChecked()->IsObject()));
    auto* v8Analyzer = TRI_UnwrapClass<arangodb::iresearch::IResearchAnalyzerFeature::AnalyzerPool>(
        result.ToLocalChecked()->ToObject(TRI_IGETC).FromMaybe(v8::Local<v8::Object>()),
        WRP_IRESEARCH_ANALYZER_TYPE, TRI_IGETC);
    EXPECT_TRUE((false == !v8Analyzer));
    EXPECT_EQ(arangodb::StaticStrings::SystemDatabase + "::testAnalyzer2", v8Analyzer->name());
    EXPECT_EQ("identity", v8Analyzer->type());
    EXPECT_EQ(VPackSlice::emptyObjectSlice(), v8Analyzer->properties());
    EXPECT_TRUE(v8Analyzer->features().empty());
    auto analyzer = analyzers->get(arangodb::StaticStrings::SystemDatabase + "::testAnalyzer2");
    EXPECT_TRUE((false == !analyzer));
  }
}

TEST_F(V8AnalyzersTest, test_get) {
  // create a new instance of an ApplicationServer and fill it with the required features
  // cannot use the existing server since its features already have some state
  std::shared_ptr<arangodb::application_features::ApplicationServer> originalServer(
      arangodb::application_features::ApplicationServer::server,
      [](arangodb::application_features::ApplicationServer* ptr) -> void {
        arangodb::application_features::ApplicationServer::server = ptr;
      });
  arangodb::application_features::ApplicationServer::server =
      nullptr;  // avoid "ApplicationServer initialized twice"
  arangodb::application_features::ApplicationServer server(nullptr, nullptr);
  arangodb::iresearch::IResearchAnalyzerFeature* analyzers;
  arangodb::DatabaseFeature* dbFeature;
  server.addFeature(new arangodb::QueryRegistryFeature(server));  // required for constructing TRI_vocbase_t
  server.addFeature(new arangodb::V8DealerFeature(server));  // required for DatabaseFeature::createDatabase(...)
  server.addFeature(dbFeature = new arangodb::DatabaseFeature(server));  // required for IResearchAnalyzerFeature::emplace(...)
  server.addFeature(analyzers = new arangodb::iresearch::IResearchAnalyzerFeature(server));  // required for running upgrade task
  analyzers->prepare();  // add static analyzers

  // create system vocbase
  {
    auto const databases = arangodb::velocypack::Parser::fromJson(
        std::string("[ { \"name\": \"") +
        arangodb::StaticStrings::SystemDatabase + "\" } ]");
    ASSERT_TRUE((TRI_ERROR_NO_ERROR == dbFeature->loadDatabases(databases->slice())));
  }

  arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;
  ASSERT_TRUE((analyzers
                   ->emplace(result, arangodb::StaticStrings::SystemDatabase + "::testAnalyzer1",
                             "identity", VPackSlice::noneSlice())
                   .ok()));

  struct ExecContext : public arangodb::ExecContext {
    ExecContext()
        : arangodb::ExecContext(arangodb::ExecContext::Type::Default, "", "",
                                arangodb::auth::Level::NONE,
                                arangodb::auth::Level::NONE) {}
  } execContext;
  arangodb::ExecContextScope execContextScope(&execContext);
  auto* authFeature = arangodb::AuthenticationFeature::instance();
  auto* userManager = authFeature->userManager();
  arangodb::aql::QueryRegistry queryRegistry(0);  // required for UserManager::loadFromDB()
  userManager->setQueryRegistry(&queryRegistry);
  auto resetUserManager = std::shared_ptr<arangodb::auth::UserManager>(
      userManager,
      [](arangodb::auth::UserManager* ptr) -> void { ptr->removeAllUsers(); });

  // invalid params (no name)
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                          arangodb::StaticStrings::SystemDatabase);
    v8::Isolate::CreateParams isolateParams;
    ArrayBufferAllocator arrayBufferAllocator;
    isolateParams.array_buffer_allocator = &arrayBufferAllocator;
    auto isolate =
        std::shared_ptr<v8::Isolate>(v8::Isolate::New(isolateParams),
                                     [](v8::Isolate* p) -> void { p->Dispose(); });
    ASSERT_TRUE((nullptr != isolate));
    v8::Isolate::Scope isolateScope(isolate.get());  // otherwise v8::Isolate::Logger() will fail (called from v8::Exception::Error)
    v8::internal::Isolate::Current()->InitializeLoggingAndCounters();  // otherwise v8::Isolate::Logger() will fail (called from v8::Exception::Error)
    v8::HandleScope handleScope(isolate.get());  // required for v8::Context::New(...), v8::ObjectTemplate::New(...) and TRI_AddMethodVocbase(...)
    auto context = v8::Context::New(isolate.get());
    v8::Context::Scope contextScope(context);  // required for TRI_AddMethodVocbase(...)
    std::unique_ptr<TRI_v8_global_t> v8g(TRI_CreateV8Globals(isolate.get()));  // create and set inside 'isolate' for use with 'TRI_GET_GLOBALS()'
    v8g->ArangoErrorTempl.Reset(isolate.get(), v8::ObjectTemplate::New(isolate.get()));  // otherwise v8:-utils::CreateErrorObject(...) will fail
    v8g->_vocbase = &vocbase;
    arangodb::iresearch::TRI_InitV8Analyzers(*v8g, isolate.get());

    auto v8Analyzers = v8::Local<v8::ObjectTemplate>::New(isolate.get(), v8g->IResearchAnalyzersTempl)
                           ->NewInstance();
    auto fn_get =
        v8Analyzers->Get(TRI_V8_ASCII_STRING(isolate.get(), "analyzer"));
    EXPECT_TRUE((fn_get->IsFunction()));

    std::vector<v8::Local<v8::Value>> args = {};

    arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
    auto& user =
        userMap
            .emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP))
            .first->second;
    user.grantDatabase(vocbase.name(), arangodb::auth::Level::RO);  // for system collections User::collectionAuthLevel(...) returns database auth::Level
    userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database

    arangodb::velocypack::Builder responce;
    v8::TryCatch tryCatch(isolate.get());
    auto result =
        v8::Function::Cast(*fn_get)->CallAsFunction(context, fn_get,
                                                    static_cast<int>(args.size()),
                                                    args.data());
    EXPECT_TRUE((result.IsEmpty()));
    EXPECT_TRUE((tryCatch.HasCaught()));
    EXPECT_TRUE((TRI_ERROR_NO_ERROR == TRI_V8ToVPack(isolate.get(), responce,
                                                     tryCatch.Exception(), false)));
    auto slice = responce.slice();
    EXPECT_TRUE((slice.isObject()));
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::ErrorNum) &&
                 slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() &&
                 TRI_ERROR_BAD_PARAMETER ==
                     slice.get(arangodb::StaticStrings::ErrorNum).getNumber<int>()));
  }

  // get static (known analyzer)
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                          arangodb::StaticStrings::SystemDatabase);
    v8::Isolate::CreateParams isolateParams;
    ArrayBufferAllocator arrayBufferAllocator;
    isolateParams.array_buffer_allocator = &arrayBufferAllocator;
    auto isolate =
        std::shared_ptr<v8::Isolate>(v8::Isolate::New(isolateParams),
                                     [](v8::Isolate* p) -> void { p->Dispose(); });
    ASSERT_TRUE((nullptr != isolate));
    v8::Isolate::Scope isolateScope(isolate.get());  // otherwise v8::Isolate::Logger() will fail (called from v8::Exception::Error)
    v8::internal::Isolate::Current()->InitializeLoggingAndCounters();  // otherwise v8::Isolate::Logger() will fail (called from v8::Exception::Error)
    v8::HandleScope handleScope(isolate.get());  // required for v8::Context::New(...), v8::ObjectTemplate::New(...) and TRI_AddMethodVocbase(...)
    auto context = v8::Context::New(isolate.get());
    v8::Context::Scope contextScope(context);  // required for TRI_AddMethodVocbase(...)
    std::unique_ptr<TRI_v8_global_t> v8g(TRI_CreateV8Globals(isolate.get()));  // create and set inside 'isolate' for use with 'TRI_GET_GLOBALS()'
    v8g->ArangoErrorTempl.Reset(isolate.get(), v8::ObjectTemplate::New(isolate.get()));  // otherwise v8:-utils::CreateErrorObject(...) will fail
    v8g->_vocbase = &vocbase;
    arangodb::iresearch::TRI_InitV8Analyzers(*v8g, isolate.get());

    auto v8Analyzers = v8::Local<v8::ObjectTemplate>::New(isolate.get(), v8g->IResearchAnalyzersTempl)
                           ->NewInstance();
    auto fn_get =
        v8Analyzers->Get(TRI_V8_ASCII_STRING(isolate.get(), "analyzer"));
    EXPECT_TRUE((fn_get->IsFunction()));

    std::vector<v8::Local<v8::Value>> args = {
        TRI_V8_ASCII_STRING(isolate.get(), "identity"),
    };

    arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
    auto& user =
        userMap
            .emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP))
            .first->second;
    user.grantDatabase(vocbase.name(), arangodb::auth::Level::NONE);  // for system collections User::collectionAuthLevel(...) returns database auth::Level
    userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database

    auto result =
        v8::Function::Cast(*fn_get)->CallAsFunction(context, fn_get,
                                                    static_cast<int>(args.size()),
                                                    args.data());
    EXPECT_TRUE((!result.IsEmpty()));
    EXPECT_TRUE((result.ToLocalChecked()->IsObject()));
    auto* v8Analyzer = TRI_UnwrapClass<arangodb::iresearch::IResearchAnalyzerFeature::AnalyzerPool>(
        result.ToLocalChecked()->ToObject(TRI_IGETC).FromMaybe(v8::Local<v8::Object>()),
        WRP_IRESEARCH_ANALYZER_TYPE, TRI_IGETC);
    EXPECT_TRUE((false == !v8Analyzer));
    EXPECT_TRUE((std::string("identity") == v8Analyzer->name()));
    EXPECT_TRUE((std::string("identity") == v8Analyzer->type()));
    EXPECT_EQ(VPackSlice::emptyObjectSlice(), v8Analyzer->properties());
    EXPECT_TRUE((2 == v8Analyzer->features().size()));
  }

  // get static (unknown analyzer)
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                          arangodb::StaticStrings::SystemDatabase);
    v8::Isolate::CreateParams isolateParams;
    ArrayBufferAllocator arrayBufferAllocator;
    isolateParams.array_buffer_allocator = &arrayBufferAllocator;
    auto isolate =
        std::shared_ptr<v8::Isolate>(v8::Isolate::New(isolateParams),
                                     [](v8::Isolate* p) -> void { p->Dispose(); });
    ASSERT_TRUE((nullptr != isolate));
    v8::Isolate::Scope isolateScope(isolate.get());  // otherwise v8::Isolate::Logger() will fail (called from v8::Exception::Error)
    v8::internal::Isolate::Current()->InitializeLoggingAndCounters();  // otherwise v8::Isolate::Logger() will fail (called from v8::Exception::Error)
    v8::HandleScope handleScope(isolate.get());  // required for v8::Context::New(...), v8::ObjectTemplate::New(...) and TRI_AddMethodVocbase(...)
    auto context = v8::Context::New(isolate.get());
    v8::Context::Scope contextScope(context);  // required for TRI_AddMethodVocbase(...)
    std::unique_ptr<TRI_v8_global_t> v8g(TRI_CreateV8Globals(isolate.get()));  // create and set inside 'isolate' for use with 'TRI_GET_GLOBALS()'
    v8g->ArangoErrorTempl.Reset(isolate.get(), v8::ObjectTemplate::New(isolate.get()));  // otherwise v8:-utils::CreateErrorObject(...) will fail
    v8g->_vocbase = &vocbase;
    arangodb::iresearch::TRI_InitV8Analyzers(*v8g, isolate.get());

    auto v8Analyzers = v8::Local<v8::ObjectTemplate>::New(isolate.get(), v8g->IResearchAnalyzersTempl)
                           ->NewInstance();
    auto fn_get =
        v8Analyzers->Get(TRI_V8_ASCII_STRING(isolate.get(), "analyzer"));
    EXPECT_TRUE((fn_get->IsFunction()));

    std::vector<v8::Local<v8::Value>> args = {
        TRI_V8_ASCII_STRING(isolate.get(), "unknown"),
    };

    arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
    auto& user =
        userMap
            .emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP))
            .first->second;
    user.grantDatabase(vocbase.name(), arangodb::auth::Level::NONE);  // for system collections User::collectionAuthLevel(...) returns database auth::Level
    userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database

    auto result =
        v8::Function::Cast(*fn_get)->CallAsFunction(context, fn_get,
                                                    static_cast<int>(args.size()),
                                                    args.data());
    EXPECT_TRUE((!result.IsEmpty()));
    EXPECT_TRUE((result.ToLocalChecked()->IsNull()));
  }

  // get custom (known analyzer) authorized
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                          arangodb::StaticStrings::SystemDatabase);
    v8::Isolate::CreateParams isolateParams;
    ArrayBufferAllocator arrayBufferAllocator;
    isolateParams.array_buffer_allocator = &arrayBufferAllocator;
    auto isolate =
        std::shared_ptr<v8::Isolate>(v8::Isolate::New(isolateParams),
                                     [](v8::Isolate* p) -> void { p->Dispose(); });
    ASSERT_TRUE((nullptr != isolate));
    v8::Isolate::Scope isolateScope(isolate.get());  // otherwise v8::Isolate::Logger() will fail (called from v8::Exception::Error)
    v8::internal::Isolate::Current()->InitializeLoggingAndCounters();  // otherwise v8::Isolate::Logger() will fail (called from v8::Exception::Error)
    v8::HandleScope handleScope(isolate.get());  // required for v8::Context::New(...), v8::ObjectTemplate::New(...) and TRI_AddMethodVocbase(...)
    auto context = v8::Context::New(isolate.get());
    v8::Context::Scope contextScope(context);  // required for TRI_AddMethodVocbase(...)
    std::unique_ptr<TRI_v8_global_t> v8g(TRI_CreateV8Globals(isolate.get()));  // create and set inside 'isolate' for use with 'TRI_GET_GLOBALS()'
    v8g->ArangoErrorTempl.Reset(isolate.get(), v8::ObjectTemplate::New(isolate.get()));  // otherwise v8:-utils::CreateErrorObject(...) will fail
    v8g->_vocbase = &vocbase;
    arangodb::iresearch::TRI_InitV8Analyzers(*v8g, isolate.get());

    auto v8Analyzers = v8::Local<v8::ObjectTemplate>::New(isolate.get(), v8g->IResearchAnalyzersTempl)
                           ->NewInstance();
    auto fn_get =
        v8Analyzers->Get(TRI_V8_ASCII_STRING(isolate.get(), "analyzer"));
    EXPECT_TRUE((fn_get->IsFunction()));

    std::vector<v8::Local<v8::Value>> args = {
        TRI_V8_STD_STRING(isolate.get(), arangodb::StaticStrings::SystemDatabase + "::testAnalyzer1"),
    };

    arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
    auto& user =
        userMap
            .emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP))
            .first->second;
    user.grantDatabase(vocbase.name(), arangodb::auth::Level::RO);  // for system collections User::collectionAuthLevel(...) returns database auth::Level
    userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database

    auto result =
        v8::Function::Cast(*fn_get)->CallAsFunction(context, fn_get,
                                                    static_cast<int>(args.size()),
                                                    args.data());
    EXPECT_TRUE((!result.IsEmpty()));
    EXPECT_TRUE((result.ToLocalChecked()->IsObject()));
    auto* v8Analyzer = TRI_UnwrapClass<arangodb::iresearch::IResearchAnalyzerFeature::AnalyzerPool>(
        result.ToLocalChecked()->ToObject(TRI_IGETC).FromMaybe(v8::Local<v8::Object>()),
        WRP_IRESEARCH_ANALYZER_TYPE, TRI_IGETC);
    EXPECT_TRUE((false == !v8Analyzer));
    EXPECT_TRUE((arangodb::StaticStrings::SystemDatabase + "::testAnalyzer1" ==
                 v8Analyzer->name()));
    EXPECT_TRUE((std::string("identity") == v8Analyzer->type()));
    EXPECT_EQ(VPackSlice::emptyObjectSlice(), v8Analyzer->properties());
    EXPECT_TRUE((true == v8Analyzer->features().empty()));
  }

  // get custom (known analyzer) not authorized
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                          arangodb::StaticStrings::SystemDatabase);
    v8::Isolate::CreateParams isolateParams;
    ArrayBufferAllocator arrayBufferAllocator;
    isolateParams.array_buffer_allocator = &arrayBufferAllocator;
    auto isolate =
        std::shared_ptr<v8::Isolate>(v8::Isolate::New(isolateParams),
                                     [](v8::Isolate* p) -> void { p->Dispose(); });
    ASSERT_TRUE((nullptr != isolate));
    v8::Isolate::Scope isolateScope(isolate.get());  // otherwise v8::Isolate::Logger() will fail (called from v8::Exception::Error)
    v8::internal::Isolate::Current()->InitializeLoggingAndCounters();  // otherwise v8::Isolate::Logger() will fail (called from v8::Exception::Error)
    v8::HandleScope handleScope(isolate.get());  // required for v8::Context::New(...), v8::ObjectTemplate::New(...) and TRI_AddMethodVocbase(...)
    auto context = v8::Context::New(isolate.get());
    v8::Context::Scope contextScope(context);  // required for TRI_AddMethodVocbase(...)
    std::unique_ptr<TRI_v8_global_t> v8g(TRI_CreateV8Globals(isolate.get()));  // create and set inside 'isolate' for use with 'TRI_GET_GLOBALS()'
    v8g->ArangoErrorTempl.Reset(isolate.get(), v8::ObjectTemplate::New(isolate.get()));  // otherwise v8:-utils::CreateErrorObject(...) will fail
    v8g->_vocbase = &vocbase;
    arangodb::iresearch::TRI_InitV8Analyzers(*v8g, isolate.get());

    auto v8Analyzers = v8::Local<v8::ObjectTemplate>::New(isolate.get(), v8g->IResearchAnalyzersTempl)
                           ->NewInstance();
    auto fn_get =
        v8Analyzers->Get(TRI_V8_ASCII_STRING(isolate.get(), "analyzer"));
    EXPECT_TRUE((fn_get->IsFunction()));

    std::vector<v8::Local<v8::Value>> args = {
        TRI_V8_STD_STRING(isolate.get(), arangodb::StaticStrings::SystemDatabase + "::testAnalyzer1"),
    };

    arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
    auto& user =
        userMap
            .emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP))
            .first->second;
    user.grantDatabase(vocbase.name(), arangodb::auth::Level::NONE);  // for system collections User::collectionAuthLevel(...) returns database auth::Level
    userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database

    arangodb::velocypack::Builder responce;
    v8::TryCatch tryCatch(isolate.get());
    auto result =
        v8::Function::Cast(*fn_get)->CallAsFunction(context, fn_get,
                                                    static_cast<int>(args.size()),
                                                    args.data());
    EXPECT_TRUE((result.IsEmpty()));
    EXPECT_TRUE((tryCatch.HasCaught()));
    EXPECT_TRUE((TRI_ERROR_NO_ERROR == TRI_V8ToVPack(isolate.get(), responce,
                                                     tryCatch.Exception(), false)));
    auto slice = responce.slice();
    EXPECT_TRUE((slice.isObject()));
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::ErrorNum) &&
                 slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() &&
                 TRI_ERROR_FORBIDDEN ==
                     slice.get(arangodb::StaticStrings::ErrorNum).getNumber<int>()));
  }

  // get custom (unknown analyzer) authorized
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                          arangodb::StaticStrings::SystemDatabase);
    v8::Isolate::CreateParams isolateParams;
    ArrayBufferAllocator arrayBufferAllocator;
    isolateParams.array_buffer_allocator = &arrayBufferAllocator;
    auto isolate =
        std::shared_ptr<v8::Isolate>(v8::Isolate::New(isolateParams),
                                     [](v8::Isolate* p) -> void { p->Dispose(); });
    ASSERT_TRUE((nullptr != isolate));
    v8::Isolate::Scope isolateScope(isolate.get());  // otherwise v8::Isolate::Logger() will fail (called from v8::Exception::Error)
    v8::internal::Isolate::Current()->InitializeLoggingAndCounters();  // otherwise v8::Isolate::Logger() will fail (called from v8::Exception::Error)
    v8::HandleScope handleScope(isolate.get());  // required for v8::Context::New(...), v8::ObjectTemplate::New(...) and TRI_AddMethodVocbase(...)
    auto context = v8::Context::New(isolate.get());
    v8::Context::Scope contextScope(context);  // required for TRI_AddMethodVocbase(...)
    std::unique_ptr<TRI_v8_global_t> v8g(TRI_CreateV8Globals(isolate.get()));  // create and set inside 'isolate' for use with 'TRI_GET_GLOBALS()'
    v8g->ArangoErrorTempl.Reset(isolate.get(), v8::ObjectTemplate::New(isolate.get()));  // otherwise v8:-utils::CreateErrorObject(...) will fail
    v8g->_vocbase = &vocbase;
    arangodb::iresearch::TRI_InitV8Analyzers(*v8g, isolate.get());

    auto v8Analyzers = v8::Local<v8::ObjectTemplate>::New(isolate.get(), v8g->IResearchAnalyzersTempl)
                           ->NewInstance();
    auto fn_get =
        v8Analyzers->Get(TRI_V8_ASCII_STRING(isolate.get(), "analyzer"));
    EXPECT_TRUE((fn_get->IsFunction()));

    std::vector<v8::Local<v8::Value>> args = {
        TRI_V8_STD_STRING(isolate.get(), arangodb::StaticStrings::SystemDatabase + "::unknown"),
    };

    arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
    auto& user =
        userMap
            .emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP))
            .first->second;
    user.grantDatabase(vocbase.name(), arangodb::auth::Level::RO);  // for system collections User::collectionAuthLevel(...) returns database auth::Level
    userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database

    auto result =
        v8::Function::Cast(*fn_get)->CallAsFunction(context, fn_get,
                                                    static_cast<int>(args.size()),
                                                    args.data());
    EXPECT_TRUE((!result.IsEmpty()));
    EXPECT_TRUE((result.ToLocalChecked()->IsNull()));
  }

  // get custom (unknown analyzer) not authorized
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                          arangodb::StaticStrings::SystemDatabase);
    v8::Isolate::CreateParams isolateParams;
    ArrayBufferAllocator arrayBufferAllocator;
    isolateParams.array_buffer_allocator = &arrayBufferAllocator;
    auto isolate =
        std::shared_ptr<v8::Isolate>(v8::Isolate::New(isolateParams),
                                     [](v8::Isolate* p) -> void { p->Dispose(); });
    ASSERT_TRUE((nullptr != isolate));
    v8::Isolate::Scope isolateScope(isolate.get());  // otherwise v8::Isolate::Logger() will fail (called from v8::Exception::Error)
    v8::internal::Isolate::Current()->InitializeLoggingAndCounters();  // otherwise v8::Isolate::Logger() will fail (called from v8::Exception::Error)
    v8::HandleScope handleScope(isolate.get());  // required for v8::Context::New(...), v8::ObjectTemplate::New(...) and TRI_AddMethodVocbase(...)
    auto context = v8::Context::New(isolate.get());
    v8::Context::Scope contextScope(context);  // required for TRI_AddMethodVocbase(...)
    std::unique_ptr<TRI_v8_global_t> v8g(TRI_CreateV8Globals(isolate.get()));  // create and set inside 'isolate' for use with 'TRI_GET_GLOBALS()'
    v8g->ArangoErrorTempl.Reset(isolate.get(), v8::ObjectTemplate::New(isolate.get()));  // otherwise v8:-utils::CreateErrorObject(...) will fail
    v8g->_vocbase = &vocbase;
    arangodb::iresearch::TRI_InitV8Analyzers(*v8g, isolate.get());

    auto v8Analyzers = v8::Local<v8::ObjectTemplate>::New(isolate.get(), v8g->IResearchAnalyzersTempl)
                           ->NewInstance();
    auto fn_get =
        v8Analyzers->Get(TRI_V8_ASCII_STRING(isolate.get(), "analyzer"));
    EXPECT_TRUE((fn_get->IsFunction()));

    std::vector<v8::Local<v8::Value>> args = {
        TRI_V8_STD_STRING(isolate.get(), arangodb::StaticStrings::SystemDatabase + "::unknown"),
    };

    arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
    auto& user =
        userMap
            .emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP))
            .first->second;
    user.grantDatabase(vocbase.name(), arangodb::auth::Level::NONE);  // for system collections User::collectionAuthLevel(...) returns database auth::Level
    userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database

    arangodb::velocypack::Builder responce;
    v8::TryCatch tryCatch(isolate.get());
    auto result =
        v8::Function::Cast(*fn_get)->CallAsFunction(context, fn_get,
                                                    static_cast<int>(args.size()),
                                                    args.data());
    EXPECT_TRUE((result.IsEmpty()));
    EXPECT_TRUE((tryCatch.HasCaught()));
    EXPECT_TRUE((TRI_ERROR_NO_ERROR == TRI_V8ToVPack(isolate.get(), responce,
                                                     tryCatch.Exception(), false)));
    auto slice = responce.slice();
    EXPECT_TRUE((slice.isObject()));
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::ErrorNum) &&
                 slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() &&
                 TRI_ERROR_FORBIDDEN ==
                     slice.get(arangodb::StaticStrings::ErrorNum).getNumber<int>()));
  }

  // get custom (unknown analyzer, unknown vocbase) authorized
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                          "unknownVocbase");
    v8::Isolate::CreateParams isolateParams;
    ArrayBufferAllocator arrayBufferAllocator;
    isolateParams.array_buffer_allocator = &arrayBufferAllocator;
    auto isolate =
        std::shared_ptr<v8::Isolate>(v8::Isolate::New(isolateParams),
                                     [](v8::Isolate* p) -> void { p->Dispose(); });
    ASSERT_TRUE((nullptr != isolate));
    v8::Isolate::Scope isolateScope(isolate.get());  // otherwise v8::Isolate::Logger() will fail (called from v8::Exception::Error)
    v8::internal::Isolate::Current()->InitializeLoggingAndCounters();  // otherwise v8::Isolate::Logger() will fail (called from v8::Exception::Error)
    v8::HandleScope handleScope(isolate.get());  // required for v8::Context::New(...), v8::ObjectTemplate::New(...) and TRI_AddMethodVocbase(...)
    auto context = v8::Context::New(isolate.get());
    v8::Context::Scope contextScope(context);  // required for TRI_AddMethodVocbase(...)
    std::unique_ptr<TRI_v8_global_t> v8g(TRI_CreateV8Globals(isolate.get()));  // create and set inside 'isolate' for use with 'TRI_GET_GLOBALS()'
    v8g->ArangoErrorTempl.Reset(isolate.get(), v8::ObjectTemplate::New(isolate.get()));  // otherwise v8:-utils::CreateErrorObject(...) will fail
    v8g->_vocbase = &vocbase;
    arangodb::iresearch::TRI_InitV8Analyzers(*v8g, isolate.get());

    auto v8Analyzers = v8::Local<v8::ObjectTemplate>::New(isolate.get(), v8g->IResearchAnalyzersTempl)
                           ->NewInstance();
    auto fn_get =
        v8Analyzers->Get(TRI_V8_ASCII_STRING(isolate.get(), "analyzer"));
    EXPECT_TRUE((fn_get->IsFunction()));

    std::vector<v8::Local<v8::Value>> args = {
        TRI_V8_ASCII_STRING(isolate.get(), "unknownVocbase::unknown"),
    };

    arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
    auto& user =
        userMap
            .emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP))
            .first->second;
    user.grantDatabase("unknownVocbase", arangodb::auth::Level::RO);  // for system collections User::collectionAuthLevel(...) returns database auth::Level
    userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database

    auto result =
        v8::Function::Cast(*fn_get)->CallAsFunction(context, fn_get,
                                                    static_cast<int>(args.size()),
                                                    args.data());
    EXPECT_TRUE((!result.IsEmpty()));
    EXPECT_TRUE((result.ToLocalChecked()->IsNull()));
  }

  // get custom (unknown analyzer, unknown vocbase) not authorized
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                          "unknownVocbase");
    v8::Isolate::CreateParams isolateParams;
    ArrayBufferAllocator arrayBufferAllocator;
    isolateParams.array_buffer_allocator = &arrayBufferAllocator;
    auto isolate =
        std::shared_ptr<v8::Isolate>(v8::Isolate::New(isolateParams),
                                     [](v8::Isolate* p) -> void { p->Dispose(); });
    ASSERT_TRUE((nullptr != isolate));
    v8::Isolate::Scope isolateScope(isolate.get());  // otherwise v8::Isolate::Logger() will fail (called from v8::Exception::Error)
    v8::internal::Isolate::Current()->InitializeLoggingAndCounters();  // otherwise v8::Isolate::Logger() will fail (called from v8::Exception::Error)
    v8::HandleScope handleScope(isolate.get());  // required for v8::Context::New(...), v8::ObjectTemplate::New(...) and TRI_AddMethodVocbase(...)
    auto context = v8::Context::New(isolate.get());
    v8::Context::Scope contextScope(context);  // required for TRI_AddMethodVocbase(...)
    std::unique_ptr<TRI_v8_global_t> v8g(TRI_CreateV8Globals(isolate.get()));  // create and set inside 'isolate' for use with 'TRI_GET_GLOBALS()'
    v8g->ArangoErrorTempl.Reset(isolate.get(), v8::ObjectTemplate::New(isolate.get()));  // otherwise v8:-utils::CreateErrorObject(...) will fail
    v8g->_vocbase = &vocbase;
    arangodb::iresearch::TRI_InitV8Analyzers(*v8g, isolate.get());

    auto v8Analyzers = v8::Local<v8::ObjectTemplate>::New(isolate.get(), v8g->IResearchAnalyzersTempl)
                           ->NewInstance();
    auto fn_get =
        v8Analyzers->Get(TRI_V8_ASCII_STRING(isolate.get(), "analyzer"));
    EXPECT_TRUE((fn_get->IsFunction()));

    std::vector<v8::Local<v8::Value>> args = {
        TRI_V8_ASCII_STRING(isolate.get(), "unknownVocbase::unknown"),
    };

    arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
    auto& user =
        userMap
            .emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP))
            .first->second;
    user.grantDatabase(vocbase.name(), arangodb::auth::Level::NONE);  // for system collections User::collectionAuthLevel(...) returns database auth::Level
    userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database

    arangodb::velocypack::Builder responce;
    v8::TryCatch tryCatch(isolate.get());
    auto result =
        v8::Function::Cast(*fn_get)->CallAsFunction(context, fn_get,
                                                    static_cast<int>(args.size()),
                                                    args.data());
    EXPECT_TRUE((result.IsEmpty()));
    EXPECT_TRUE((tryCatch.HasCaught()));
    EXPECT_TRUE((TRI_ERROR_NO_ERROR == TRI_V8ToVPack(isolate.get(), responce,
                                                     tryCatch.Exception(), false)));
    auto slice = responce.slice();
    EXPECT_TRUE((slice.isObject()));
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::ErrorNum) &&
                 slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() &&
                 TRI_ERROR_FORBIDDEN ==
                     slice.get(arangodb::StaticStrings::ErrorNum).getNumber<int>()));
  }
}

TEST_F(V8AnalyzersTest, test_list) {
  // create a new instance of an ApplicationServer and fill it with the required features
  // cannot use the existing server since its features already have some state
  std::shared_ptr<arangodb::application_features::ApplicationServer> originalServer(
      arangodb::application_features::ApplicationServer::server,
      [](arangodb::application_features::ApplicationServer* ptr) -> void {
        arangodb::application_features::ApplicationServer::server = ptr;
      });
  arangodb::application_features::ApplicationServer::server =
      nullptr;  // avoid "ApplicationServer initialized twice"
  arangodb::application_features::ApplicationServer server(nullptr, nullptr);
  arangodb::iresearch::IResearchAnalyzerFeature* analyzers;
  arangodb::DatabaseFeature* dbFeature;
  arangodb::SystemDatabaseFeature* sysDatabase;
  server.addFeature(new arangodb::QueryRegistryFeature(server));  // required for constructing TRI_vocbase_t
  server.addFeature(new arangodb::V8DealerFeature(server));  // required for DatabaseFeature::createDatabase(...)
  server.addFeature(dbFeature = new arangodb::DatabaseFeature(server));  // required for IResearchAnalyzerFeature::emplace(...)
  server.addFeature(sysDatabase = new arangodb::SystemDatabaseFeature(server));  // required for IResearchAnalyzerFeature::start()
  server.addFeature(analyzers = new arangodb::iresearch::IResearchAnalyzerFeature(server));  // required for running upgrade task

  // create system vocbase
  {
    auto const databases = arangodb::velocypack::Parser::fromJson(
        std::string("[ { \"name\": \"") +
        arangodb::StaticStrings::SystemDatabase + "\" } ]");
    ASSERT_TRUE((TRI_ERROR_NO_ERROR == dbFeature->loadDatabases(databases->slice())));
    TRI_vocbase_t* vocbase;
    ASSERT_TRUE((TRI_ERROR_NO_ERROR ==
                 dbFeature->createDatabase(1, "testVocbase", vocbase)));
    sysDatabase->start();  // get system database from DatabaseFeature
  }

  arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;
  ASSERT_TRUE((analyzers
                   ->emplace(result, arangodb::StaticStrings::SystemDatabase + "::testAnalyzer1",
                             "identity", VPackSlice::noneSlice())
                   .ok()));
  ASSERT_TRUE((analyzers
                   ->emplace(result, "testVocbase::testAnalyzer2", "identity",
                             VPackSlice::noneSlice())
                   .ok()));

  struct ExecContext : public arangodb::ExecContext {
    ExecContext()
        : arangodb::ExecContext(arangodb::ExecContext::Type::Default, "", "",
                                arangodb::auth::Level::NONE,
                                arangodb::auth::Level::NONE) {}
  } execContext;
  arangodb::ExecContextScope execContextScope(&execContext);
  auto* authFeature = arangodb::AuthenticationFeature::instance();
  auto* userManager = authFeature->userManager();
  arangodb::aql::QueryRegistry queryRegistry(0);  // required for UserManager::loadFromDB()
  userManager->setQueryRegistry(&queryRegistry);
  auto resetUserManager = std::shared_ptr<arangodb::auth::UserManager>(
      userManager,
      [](arangodb::auth::UserManager* ptr) -> void { ptr->removeAllUsers(); });

  // system database (authorised)
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                          arangodb::StaticStrings::SystemDatabase);
    v8::Isolate::CreateParams isolateParams;
    ArrayBufferAllocator arrayBufferAllocator;
    isolateParams.array_buffer_allocator = &arrayBufferAllocator;
    auto isolate =
        std::shared_ptr<v8::Isolate>(v8::Isolate::New(isolateParams),
                                     [](v8::Isolate* p) -> void { p->Dispose(); });
    ASSERT_TRUE((nullptr != isolate));
    v8::Isolate::Scope isolateScope(isolate.get());  // otherwise v8::Isolate::Logger() will fail (called from v8::Exception::Error)
    v8::internal::Isolate::Current()->InitializeLoggingAndCounters();  // otherwise v8::Isolate::Logger() will fail (called from v8::Exception::Error)
    v8::HandleScope handleScope(isolate.get());  // required for v8::Context::New(...), v8::ObjectTemplate::New(...) and TRI_AddMethodVocbase(...)
    auto context = v8::Context::New(isolate.get());
    v8::Context::Scope contextScope(context);  // required for TRI_AddMethodVocbase(...)
    std::unique_ptr<TRI_v8_global_t> v8g(TRI_CreateV8Globals(isolate.get()));  // create and set inside 'isolate' for use with 'TRI_GET_GLOBALS()'
    v8g->ArangoErrorTempl.Reset(isolate.get(), v8::ObjectTemplate::New(isolate.get()));  // otherwise v8:-utils::CreateErrorObject(...) will fail
    v8g->_vocbase = &vocbase;
    arangodb::iresearch::TRI_InitV8Analyzers(*v8g, isolate.get());

    auto v8Analyzers = v8::Local<v8::ObjectTemplate>::New(isolate.get(), v8g->IResearchAnalyzersTempl)
                           ->NewInstance();
    auto fn_list =
        v8Analyzers->Get(TRI_V8_ASCII_STRING(isolate.get(), "toArray"));
    EXPECT_TRUE((fn_list->IsFunction()));

    std::vector<v8::Local<v8::Value>> args = {};

    arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
    auto& user =
        userMap
            .emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP))
            .first->second;
    user.grantDatabase(vocbase.name(), arangodb::auth::Level::RO);  // for system collections User::collectionAuthLevel(...) returns database auth::Level
    userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database

    std::set<std::string> expected = {
      "identity", "text_de",  "text_en",  "text_es",  "text_fi",
      "text_fr",  "text_it",  "text_nl",  "text_no",  "text_pt",
      "text_ru",  "text_sv",  "text_zh",
      arangodb::StaticStrings::SystemDatabase + "::testAnalyzer1",
    };
    auto result =
        v8::Function::Cast(*fn_list)->CallAsFunction(context, fn_list,
                                                     static_cast<int>(args.size()),
                                                     args.data());
    EXPECT_TRUE((!result.IsEmpty()));
    EXPECT_TRUE((result.ToLocalChecked()->IsArray()));
    auto v8Result = v8::Handle<v8::Array>::Cast(result.ToLocalChecked());

    for (uint32_t i = 0, count = v8Result->Length(); i < count; ++i) {
      auto v8Analyzer = v8Result->Get(i);
      EXPECT_TRUE((!v8Analyzer.IsEmpty()));
      EXPECT_TRUE((v8Analyzer->IsObject()));
      auto* analyzer =
          TRI_UnwrapClass<arangodb::iresearch::IResearchAnalyzerFeature::AnalyzerPool>(
              v8Analyzer->ToObject(TRI_IGETC).FromMaybe(v8::Local<v8::Object>()),
              WRP_IRESEARCH_ANALYZER_TYPE, TRI_IGETC);
      EXPECT_TRUE((false == !analyzer));
      EXPECT_TRUE((1 == expected.erase(analyzer->name())));
    }

    EXPECT_TRUE((true == expected.empty()));
  }

  // system database (not authorised)
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                          arangodb::StaticStrings::SystemDatabase);
    v8::Isolate::CreateParams isolateParams;
    ArrayBufferAllocator arrayBufferAllocator;
    isolateParams.array_buffer_allocator = &arrayBufferAllocator;
    auto isolate =
        std::shared_ptr<v8::Isolate>(v8::Isolate::New(isolateParams),
                                     [](v8::Isolate* p) -> void { p->Dispose(); });
    ASSERT_TRUE((nullptr != isolate));
    v8::Isolate::Scope isolateScope(isolate.get());  // otherwise v8::Isolate::Logger() will fail (called from v8::Exception::Error)
    v8::internal::Isolate::Current()->InitializeLoggingAndCounters();  // otherwise v8::Isolate::Logger() will fail (called from v8::Exception::Error)
    v8::HandleScope handleScope(isolate.get());  // required for v8::Context::New(...), v8::ObjectTemplate::New(...) and TRI_AddMethodVocbase(...)
    auto context = v8::Context::New(isolate.get());
    v8::Context::Scope contextScope(context);  // required for TRI_AddMethodVocbase(...)
    std::unique_ptr<TRI_v8_global_t> v8g(TRI_CreateV8Globals(isolate.get()));  // create and set inside 'isolate' for use with 'TRI_GET_GLOBALS()'
    v8g->ArangoErrorTempl.Reset(isolate.get(), v8::ObjectTemplate::New(isolate.get()));  // otherwise v8:-utils::CreateErrorObject(...) will fail
    v8g->_vocbase = &vocbase;
    arangodb::iresearch::TRI_InitV8Analyzers(*v8g, isolate.get());

    auto v8Analyzers = v8::Local<v8::ObjectTemplate>::New(isolate.get(), v8g->IResearchAnalyzersTempl)
                           ->NewInstance();
    auto fn_list =
        v8Analyzers->Get(TRI_V8_ASCII_STRING(isolate.get(), "toArray"));
    EXPECT_TRUE((fn_list->IsFunction()));

    std::vector<v8::Local<v8::Value>> args = {};

    arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
    auto& user =
        userMap
            .emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP))
            .first->second;
    user.grantDatabase(vocbase.name(), arangodb::auth::Level::NONE);  // for system collections User::collectionAuthLevel(...) returns database auth::Level
    userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database

    std::set<std::string> expected = {
      "identity", "text_de",  "text_en",  "text_es",  "text_fi",
      "text_fr",  "text_it",  "text_nl",  "text_no",  "text_pt",
      "text_ru",  "text_sv",  "text_zh",
    };

    auto result =
        v8::Function::Cast(*fn_list)->CallAsFunction(context, fn_list,
                                                     static_cast<int>(args.size()),
                                                     args.data());
    EXPECT_TRUE((!result.IsEmpty()));
    EXPECT_TRUE((result.ToLocalChecked()->IsArray()));
    auto v8Result = v8::Handle<v8::Array>::Cast(result.ToLocalChecked());

    for (uint32_t i = 0, count = v8Result->Length(); i < count; ++i) {
      auto v8Analyzer = v8Result->Get(i);
      EXPECT_TRUE((!v8Analyzer.IsEmpty()));
      EXPECT_TRUE((v8Analyzer->IsObject()));
      auto* analyzer =
          TRI_UnwrapClass<arangodb::iresearch::IResearchAnalyzerFeature::AnalyzerPool>(
              v8Analyzer->ToObject(TRI_IGETC).FromMaybe(v8::Local<v8::Object>()),
              WRP_IRESEARCH_ANALYZER_TYPE, TRI_IGETC);
      EXPECT_TRUE((false == !analyzer));
      EXPECT_TRUE((1 == expected.erase(analyzer->name())));
    }

    EXPECT_TRUE((true == expected.empty()));
  }

  // non-system database (authorised, system authorised)
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                          "testVocbase");
    v8::Isolate::CreateParams isolateParams;
    ArrayBufferAllocator arrayBufferAllocator;
    isolateParams.array_buffer_allocator = &arrayBufferAllocator;
    auto isolate =
        std::shared_ptr<v8::Isolate>(v8::Isolate::New(isolateParams),
                                     [](v8::Isolate* p) -> void { p->Dispose(); });
    ASSERT_TRUE((nullptr != isolate));
    v8::Isolate::Scope isolateScope(isolate.get());  // otherwise v8::Isolate::Logger() will fail (called from v8::Exception::Error)
    v8::internal::Isolate::Current()->InitializeLoggingAndCounters();  // otherwise v8::Isolate::Logger() will fail (called from v8::Exception::Error)
    v8::HandleScope handleScope(isolate.get());  // required for v8::Context::New(...), v8::ObjectTemplate::New(...) and TRI_AddMethodVocbase(...)
    auto context = v8::Context::New(isolate.get());
    v8::Context::Scope contextScope(context);  // required for TRI_AddMethodVocbase(...)
    std::unique_ptr<TRI_v8_global_t> v8g(TRI_CreateV8Globals(isolate.get()));  // create and set inside 'isolate' for use with 'TRI_GET_GLOBALS()'
    v8g->ArangoErrorTempl.Reset(isolate.get(), v8::ObjectTemplate::New(isolate.get()));  // otherwise v8:-utils::CreateErrorObject(...) will fail
    v8g->_vocbase = &vocbase;
    arangodb::iresearch::TRI_InitV8Analyzers(*v8g, isolate.get());

    auto v8Analyzers = v8::Local<v8::ObjectTemplate>::New(isolate.get(), v8g->IResearchAnalyzersTempl)
                           ->NewInstance();
    auto fn_list =
        v8Analyzers->Get(TRI_V8_ASCII_STRING(isolate.get(), "toArray"));
    EXPECT_TRUE((fn_list->IsFunction()));

    std::vector<v8::Local<v8::Value>> args = {};

    arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
    auto& user =
        userMap
            .emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP))
            .first->second;
    user.grantDatabase(arangodb::StaticStrings::SystemDatabase, arangodb::auth::Level::RO);  // for system collections User::collectionAuthLevel(...) returns database auth::Level
    user.grantDatabase(vocbase.name(), arangodb::auth::Level::RO);  // for system collections User::collectionAuthLevel(...) returns database auth::Level
    userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database

    std::set<std::string> expected = {
      "identity", "text_de",  "text_en",  "text_es",  "text_fi",
      "text_fr",  "text_it",  "text_nl",  "text_no",  "text_pt",
      "text_ru",  "text_sv",  "text_zh",
      arangodb::StaticStrings::SystemDatabase + "::testAnalyzer1",
      "testVocbase::testAnalyzer2",
    };
    auto result =
        v8::Function::Cast(*fn_list)->CallAsFunction(context, fn_list,
                                                     static_cast<int>(args.size()),
                                                     args.data());
    EXPECT_TRUE((!result.IsEmpty()));
    EXPECT_TRUE((result.ToLocalChecked()->IsArray()));
    auto v8Result = v8::Handle<v8::Array>::Cast(result.ToLocalChecked());

    for (uint32_t i = 0, count = v8Result->Length(); i < count; ++i) {
      auto v8Analyzer = v8Result->Get(i);
      EXPECT_TRUE((!v8Analyzer.IsEmpty()));
      EXPECT_TRUE((v8Analyzer->IsObject()));
      auto* analyzer =
          TRI_UnwrapClass<arangodb::iresearch::IResearchAnalyzerFeature::AnalyzerPool>(
              v8Analyzer->ToObject(TRI_IGETC).FromMaybe(v8::Local<v8::Object>()),
              WRP_IRESEARCH_ANALYZER_TYPE, TRI_IGETC);
      EXPECT_TRUE((false == !analyzer));
      EXPECT_TRUE((1 == expected.erase(analyzer->name())));
    }

    EXPECT_TRUE((true == expected.empty()));
  }

  // non-system database (not authorised, system authorised)
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                          "testVocbase");
    v8::Isolate::CreateParams isolateParams;
    ArrayBufferAllocator arrayBufferAllocator;
    isolateParams.array_buffer_allocator = &arrayBufferAllocator;
    auto isolate =
        std::shared_ptr<v8::Isolate>(v8::Isolate::New(isolateParams),
                                     [](v8::Isolate* p) -> void { p->Dispose(); });
    ASSERT_TRUE((nullptr != isolate));
    v8::Isolate::Scope isolateScope(isolate.get());  // otherwise v8::Isolate::Logger() will fail (called from v8::Exception::Error)
    v8::internal::Isolate::Current()->InitializeLoggingAndCounters();  // otherwise v8::Isolate::Logger() will fail (called from v8::Exception::Error)
    v8::HandleScope handleScope(isolate.get());  // required for v8::Context::New(...), v8::ObjectTemplate::New(...) and TRI_AddMethodVocbase(...)
    auto context = v8::Context::New(isolate.get());
    v8::Context::Scope contextScope(context);  // required for TRI_AddMethodVocbase(...)
    std::unique_ptr<TRI_v8_global_t> v8g(TRI_CreateV8Globals(isolate.get()));  // create and set inside 'isolate' for use with 'TRI_GET_GLOBALS()'
    v8g->ArangoErrorTempl.Reset(isolate.get(), v8::ObjectTemplate::New(isolate.get()));  // otherwise v8:-utils::CreateErrorObject(...) will fail
    v8g->_vocbase = &vocbase;
    arangodb::iresearch::TRI_InitV8Analyzers(*v8g, isolate.get());

    auto v8Analyzers = v8::Local<v8::ObjectTemplate>::New(isolate.get(), v8g->IResearchAnalyzersTempl)
                           ->NewInstance();
    auto fn_list =
        v8Analyzers->Get(TRI_V8_ASCII_STRING(isolate.get(), "toArray"));
    EXPECT_TRUE((fn_list->IsFunction()));

    std::vector<v8::Local<v8::Value>> args = {};

    arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
    auto& user =
        userMap
            .emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP))
            .first->second;
    user.grantDatabase(arangodb::StaticStrings::SystemDatabase, arangodb::auth::Level::RO);  // for system collections User::collectionAuthLevel(...) returns database auth::Level
    user.grantDatabase(vocbase.name(), arangodb::auth::Level::NONE);  // for system collections User::collectionAuthLevel(...) returns database auth::Level
    userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database

    std::set<std::string> expected = {
      "identity", "text_de",  "text_en",  "text_es",  "text_fi",
      "text_fr",  "text_it",  "text_nl",  "text_no",  "text_pt",
      "text_ru",  "text_sv",  "text_zh",
      arangodb::StaticStrings::SystemDatabase + "::testAnalyzer1",
    };
    auto result =
        v8::Function::Cast(*fn_list)->CallAsFunction(context, fn_list,
                                                     static_cast<int>(args.size()),
                                                     args.data());
    EXPECT_TRUE((!result.IsEmpty()));
    EXPECT_TRUE((result.ToLocalChecked()->IsArray()));
    auto v8Result = v8::Handle<v8::Array>::Cast(result.ToLocalChecked());

    for (uint32_t i = 0, count = v8Result->Length(); i < count; ++i) {
      auto v8Analyzer = v8Result->Get(i);
      EXPECT_TRUE((!v8Analyzer.IsEmpty()));
      EXPECT_TRUE((v8Analyzer->IsObject()));
      auto* analyzer =
          TRI_UnwrapClass<arangodb::iresearch::IResearchAnalyzerFeature::AnalyzerPool>(
              v8Analyzer->ToObject(TRI_IGETC).FromMaybe(v8::Local<v8::Object>()),
              WRP_IRESEARCH_ANALYZER_TYPE, TRI_IGETC);
      EXPECT_TRUE((false == !analyzer));
      EXPECT_TRUE((1 == expected.erase(analyzer->name())));
    }

    EXPECT_TRUE((true == expected.empty()));
  }

  // non-system database (authorised, system not authorised)
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                          "testVocbase");
    v8::Isolate::CreateParams isolateParams;
    ArrayBufferAllocator arrayBufferAllocator;
    isolateParams.array_buffer_allocator = &arrayBufferAllocator;
    auto isolate =
        std::shared_ptr<v8::Isolate>(v8::Isolate::New(isolateParams),
                                     [](v8::Isolate* p) -> void { p->Dispose(); });
    ASSERT_TRUE((nullptr != isolate));
    v8::Isolate::Scope isolateScope(isolate.get());  // otherwise v8::Isolate::Logger() will fail (called from v8::Exception::Error)
    v8::internal::Isolate::Current()->InitializeLoggingAndCounters();  // otherwise v8::Isolate::Logger() will fail (called from v8::Exception::Error)
    v8::HandleScope handleScope(isolate.get());  // required for v8::Context::New(...), v8::ObjectTemplate::New(...) and TRI_AddMethodVocbase(...)
    auto context = v8::Context::New(isolate.get());
    v8::Context::Scope contextScope(context);  // required for TRI_AddMethodVocbase(...)
    std::unique_ptr<TRI_v8_global_t> v8g(TRI_CreateV8Globals(isolate.get()));  // create and set inside 'isolate' for use with 'TRI_GET_GLOBALS()'
    v8g->ArangoErrorTempl.Reset(isolate.get(), v8::ObjectTemplate::New(isolate.get()));  // otherwise v8:-utils::CreateErrorObject(...) will fail
    v8g->_vocbase = &vocbase;
    arangodb::iresearch::TRI_InitV8Analyzers(*v8g, isolate.get());

    auto v8Analyzers = v8::Local<v8::ObjectTemplate>::New(isolate.get(), v8g->IResearchAnalyzersTempl)
                           ->NewInstance();
    auto fn_list =
        v8Analyzers->Get(TRI_V8_ASCII_STRING(isolate.get(), "toArray"));
    EXPECT_TRUE((fn_list->IsFunction()));

    std::vector<v8::Local<v8::Value>> args = {};

    arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
    auto& user =
        userMap
            .emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP))
            .first->second;
    user.grantDatabase(arangodb::StaticStrings::SystemDatabase, arangodb::auth::Level::NONE);  // for system collections User::collectionAuthLevel(...) returns database auth::Level
    user.grantDatabase(vocbase.name(), arangodb::auth::Level::RO);  // for system collections User::collectionAuthLevel(...) returns database auth::Level
    userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database

    std::set<std::string> expected = {
      "identity", "text_de",  "text_en",  "text_es",  "text_fi",
      "text_fr",  "text_it",  "text_nl",  "text_no",  "text_pt",
      "text_ru",  "text_sv",  "text_zh",
      "testVocbase::testAnalyzer2",
    };
    auto result =
        v8::Function::Cast(*fn_list)->CallAsFunction(context, fn_list,
                                                     static_cast<int>(args.size()),
                                                     args.data());
    EXPECT_TRUE((!result.IsEmpty()));
    EXPECT_TRUE((result.ToLocalChecked()->IsArray()));
    auto v8Result = v8::Handle<v8::Array>::Cast(result.ToLocalChecked());

    for (uint32_t i = 0, count = v8Result->Length(); i < count; ++i) {
      auto v8Analyzer = v8Result->Get(i);
      EXPECT_TRUE((!v8Analyzer.IsEmpty()));
      EXPECT_TRUE((v8Analyzer->IsObject()));
      auto* analyzer =
          TRI_UnwrapClass<arangodb::iresearch::IResearchAnalyzerFeature::AnalyzerPool>(
              v8Analyzer->ToObject(TRI_IGETC).FromMaybe(v8::Local<v8::Object>()),
              WRP_IRESEARCH_ANALYZER_TYPE, TRI_IGETC);
      EXPECT_TRUE((false == !analyzer));
      EXPECT_TRUE((1 == expected.erase(analyzer->name())));
    }

    EXPECT_TRUE((true == expected.empty()));
  }

  // non-system database (not authorised, system not authorised)
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                          "testVocbase");
    v8::Isolate::CreateParams isolateParams;
    ArrayBufferAllocator arrayBufferAllocator;
    isolateParams.array_buffer_allocator = &arrayBufferAllocator;
    auto isolate =
        std::shared_ptr<v8::Isolate>(v8::Isolate::New(isolateParams),
                                     [](v8::Isolate* p) -> void { p->Dispose(); });
    ASSERT_TRUE((nullptr != isolate));
    v8::Isolate::Scope isolateScope(isolate.get());  // otherwise v8::Isolate::Logger() will fail (called from v8::Exception::Error)
    v8::internal::Isolate::Current()->InitializeLoggingAndCounters();  // otherwise v8::Isolate::Logger() will fail (called from v8::Exception::Error)
    v8::HandleScope handleScope(isolate.get());  // required for v8::Context::New(...), v8::ObjectTemplate::New(...) and TRI_AddMethodVocbase(...)
    auto context = v8::Context::New(isolate.get());
    v8::Context::Scope contextScope(context);  // required for TRI_AddMethodVocbase(...)
    std::unique_ptr<TRI_v8_global_t> v8g(TRI_CreateV8Globals(isolate.get()));  // create and set inside 'isolate' for use with 'TRI_GET_GLOBALS()'
    v8g->ArangoErrorTempl.Reset(isolate.get(), v8::ObjectTemplate::New(isolate.get()));  // otherwise v8:-utils::CreateErrorObject(...) will fail
    v8g->_vocbase = &vocbase;
    arangodb::iresearch::TRI_InitV8Analyzers(*v8g, isolate.get());

    auto v8Analyzers = v8::Local<v8::ObjectTemplate>::New(isolate.get(), v8g->IResearchAnalyzersTempl)
                           ->NewInstance();
    auto fn_list =
        v8Analyzers->Get(TRI_V8_ASCII_STRING(isolate.get(), "toArray"));
    EXPECT_TRUE((fn_list->IsFunction()));

    std::vector<v8::Local<v8::Value>> args = {};

    arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
    auto& user =
        userMap
            .emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP))
            .first->second;
    user.grantDatabase(arangodb::StaticStrings::SystemDatabase, arangodb::auth::Level::NONE);  // for system collections User::collectionAuthLevel(...) returns database auth::Level
    user.grantDatabase(vocbase.name(), arangodb::auth::Level::NONE);  // for system collections User::collectionAuthLevel(...) returns database auth::Level
    userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database

    std::set<std::string> expected = {
      "identity", "text_de",  "text_en",  "text_es",  "text_fi",
      "text_fr",  "text_it",  "text_nl",  "text_no",  "text_pt",
      "text_ru",  "text_sv",  "text_zh",
    };
    auto result =
        v8::Function::Cast(*fn_list)->CallAsFunction(context, fn_list,
                                                     static_cast<int>(args.size()),
                                                     args.data());
    EXPECT_TRUE((!result.IsEmpty()));
    EXPECT_TRUE((result.ToLocalChecked()->IsArray()));
    auto v8Result = v8::Handle<v8::Array>::Cast(result.ToLocalChecked());

    for (uint32_t i = 0, count = v8Result->Length(); i < count; ++i) {
      auto v8Analyzer = v8Result->Get(i);
      EXPECT_TRUE((!v8Analyzer.IsEmpty()));
      EXPECT_TRUE((v8Analyzer->IsObject()));
      auto* analyzer =
          TRI_UnwrapClass<arangodb::iresearch::IResearchAnalyzerFeature::AnalyzerPool>(
              v8Analyzer->ToObject(TRI_IGETC).FromMaybe(v8::Local<v8::Object>()),
              WRP_IRESEARCH_ANALYZER_TYPE, TRI_IGETC);
      EXPECT_TRUE((false == !analyzer));
      EXPECT_TRUE((1 == expected.erase(analyzer->name())));
    }

    EXPECT_TRUE((true == expected.empty()));
  }
}

TEST_F(V8AnalyzersTest, test_remove) {
  // create a new instance of an ApplicationServer and fill it with the required features
  // cannot use the existing server since its features already have some state
  std::shared_ptr<arangodb::application_features::ApplicationServer> originalServer(
      arangodb::application_features::ApplicationServer::server,
      [](arangodb::application_features::ApplicationServer* ptr) -> void {
        arangodb::application_features::ApplicationServer::server = ptr;
      });
  arangodb::application_features::ApplicationServer::server =
      nullptr;  // avoid "ApplicationServer initialized twice"
  arangodb::application_features::ApplicationServer server(nullptr, nullptr);
  arangodb::iresearch::IResearchAnalyzerFeature* analyzers;
  arangodb::DatabaseFeature* dbFeature;
  server.addFeature(new arangodb::QueryRegistryFeature(server));  // required for constructing TRI_vocbase_t
  server.addFeature(new arangodb::V8DealerFeature(server));  // required for DatabaseFeature::createDatabase(...)
  server.addFeature(dbFeature = new arangodb::DatabaseFeature(server));  // required for IResearchAnalyzerFeature::emplace(...)
  server.addFeature(analyzers = new arangodb::iresearch::IResearchAnalyzerFeature(server));  // required for running upgrade task

  // create system vocbase
  {
    auto const databases = arangodb::velocypack::Parser::fromJson(
        std::string("[ { \"name\": \"") +
        arangodb::StaticStrings::SystemDatabase + "\" } ]");
    ASSERT_TRUE((TRI_ERROR_NO_ERROR == dbFeature->loadDatabases(databases->slice())));
  }

  arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;
  ASSERT_TRUE((analyzers
                   ->emplace(result, arangodb::StaticStrings::SystemDatabase + "::testAnalyzer1",
                             "identity", VPackSlice::noneSlice())
                   .ok()));
  ASSERT_TRUE((analyzers
                   ->emplace(result, arangodb::StaticStrings::SystemDatabase + "::testAnalyzer2",
                             "identity", VPackSlice::noneSlice())
                   .ok()));

  struct ExecContext : public arangodb::ExecContext {
    ExecContext()
        : arangodb::ExecContext(arangodb::ExecContext::Type::Default, "", "",
                                arangodb::auth::Level::NONE,
                                arangodb::auth::Level::NONE) {}
  } execContext;
  arangodb::ExecContextScope execContextScope(&execContext);
  auto* authFeature = arangodb::AuthenticationFeature::instance();
  auto* userManager = authFeature->userManager();
  arangodb::aql::QueryRegistry queryRegistry(0);  // required for UserManager::loadFromDB()
  userManager->setQueryRegistry(&queryRegistry);
  auto resetUserManager = std::shared_ptr<arangodb::auth::UserManager>(
      userManager,
      [](arangodb::auth::UserManager* ptr) -> void { ptr->removeAllUsers(); });

  // invalid params (no name)
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                          arangodb::StaticStrings::SystemDatabase);
    v8::Isolate::CreateParams isolateParams;
    ArrayBufferAllocator arrayBufferAllocator;
    isolateParams.array_buffer_allocator = &arrayBufferAllocator;
    auto isolate =
        std::shared_ptr<v8::Isolate>(v8::Isolate::New(isolateParams),
                                     [](v8::Isolate* p) -> void { p->Dispose(); });
    ASSERT_TRUE((nullptr != isolate));
    v8::Isolate::Scope isolateScope(isolate.get());  // otherwise v8::Isolate::Logger() will fail (called from v8::Exception::Error)
    v8::internal::Isolate::Current()->InitializeLoggingAndCounters();  // otherwise v8::Isolate::Logger() will fail (called from v8::Exception::Error)
    v8::HandleScope handleScope(isolate.get());  // required for v8::Context::New(...), v8::ObjectTemplate::New(...) and TRI_AddMethodVocbase(...)
    auto context = v8::Context::New(isolate.get());
    v8::Context::Scope contextScope(context);  // required for TRI_AddMethodVocbase(...)
    std::unique_ptr<TRI_v8_global_t> v8g(TRI_CreateV8Globals(isolate.get()));  // create and set inside 'isolate' for use with 'TRI_GET_GLOBALS()'
    v8g->ArangoErrorTempl.Reset(isolate.get(), v8::ObjectTemplate::New(isolate.get()));  // otherwise v8:-utils::CreateErrorObject(...) will fail
    v8g->_vocbase = &vocbase;
    arangodb::iresearch::TRI_InitV8Analyzers(*v8g, isolate.get());

    auto v8Analyzers = v8::Local<v8::ObjectTemplate>::New(isolate.get(), v8g->IResearchAnalyzersTempl)
                           ->NewInstance();
    auto fn_remove =
        v8Analyzers->Get(TRI_V8_ASCII_STRING(isolate.get(), "remove"));
    EXPECT_TRUE((fn_remove->IsFunction()));

    std::vector<v8::Local<v8::Value>> args = {};

    arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
    auto& user =
        userMap
            .emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP))
            .first->second;
    user.grantDatabase(vocbase.name(), arangodb::auth::Level::RW);  // for system collections User::collectionAuthLevel(...) returns database auth::Level
    userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database

    arangodb::velocypack::Builder responce;
    v8::TryCatch tryCatch(isolate.get());
    auto result = v8::Function::Cast(*fn_remove)
                      ->CallAsFunction(context, fn_remove,
                                       static_cast<int>(args.size()), args.data());
    EXPECT_TRUE((result.IsEmpty()));
    EXPECT_TRUE((tryCatch.HasCaught()));
    EXPECT_TRUE((TRI_ERROR_NO_ERROR == TRI_V8ToVPack(isolate.get(), responce,
                                                     tryCatch.Exception(), false)));
    auto slice = responce.slice();
    EXPECT_TRUE((slice.isObject()));
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::ErrorNum) &&
                 slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() &&
                 TRI_ERROR_BAD_PARAMETER ==
                     slice.get(arangodb::StaticStrings::ErrorNum).getNumber<int>()));
  }

  // unknown analyzer
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                          arangodb::StaticStrings::SystemDatabase);
    v8::Isolate::CreateParams isolateParams;
    ArrayBufferAllocator arrayBufferAllocator;
    isolateParams.array_buffer_allocator = &arrayBufferAllocator;
    auto isolate =
        std::shared_ptr<v8::Isolate>(v8::Isolate::New(isolateParams),
                                     [](v8::Isolate* p) -> void { p->Dispose(); });
    ASSERT_TRUE((nullptr != isolate));
    v8::Isolate::Scope isolateScope(isolate.get());  // otherwise v8::Isolate::Logger() will fail (called from v8::Exception::Error)
    v8::internal::Isolate::Current()->InitializeLoggingAndCounters();  // otherwise v8::Isolate::Logger() will fail (called from v8::Exception::Error)
    v8::HandleScope handleScope(isolate.get());  // required for v8::Context::New(...), v8::ObjectTemplate::New(...) and TRI_AddMethodVocbase(...)
    auto context = v8::Context::New(isolate.get());
    v8::Context::Scope contextScope(context);  // required for TRI_AddMethodVocbase(...)
    std::unique_ptr<TRI_v8_global_t> v8g(TRI_CreateV8Globals(isolate.get()));  // create and set inside 'isolate' for use with 'TRI_GET_GLOBALS()'
    v8g->ArangoErrorTempl.Reset(isolate.get(), v8::ObjectTemplate::New(isolate.get()));  // otherwise v8:-utils::CreateErrorObject(...) will fail
    v8g->_vocbase = &vocbase;
    arangodb::iresearch::TRI_InitV8Analyzers(*v8g, isolate.get());

    auto v8Analyzers = v8::Local<v8::ObjectTemplate>::New(isolate.get(), v8g->IResearchAnalyzersTempl)
                           ->NewInstance();
    auto fn_remove =
        v8Analyzers->Get(TRI_V8_ASCII_STRING(isolate.get(), "remove"));
    EXPECT_TRUE((fn_remove->IsFunction()));

    std::vector<v8::Local<v8::Value>> args = {
        TRI_V8_STD_STRING(isolate.get(), arangodb::StaticStrings::SystemDatabase + "::unknown"),
    };

    arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
    auto& user =
        userMap
            .emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP))
            .first->second;
    user.grantDatabase(vocbase.name(), arangodb::auth::Level::RW);  // for system collections User::collectionAuthLevel(...) returns database auth::Level
    userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database

    arangodb::velocypack::Builder responce;
    v8::TryCatch tryCatch(isolate.get());
    auto result = v8::Function::Cast(*fn_remove)
                      ->CallAsFunction(context, fn_remove,
                                       static_cast<int>(args.size()), args.data());
    EXPECT_TRUE((result.IsEmpty()));
    EXPECT_TRUE((tryCatch.HasCaught()));
    EXPECT_TRUE((TRI_ERROR_NO_ERROR == TRI_V8ToVPack(isolate.get(), responce,
                                                     tryCatch.Exception(), false)));
    auto slice = responce.slice();
    EXPECT_TRUE((slice.isObject()));
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::ErrorNum) &&
                 slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() &&
                 TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND ==
                     slice.get(arangodb::StaticStrings::ErrorNum).getNumber<int>()));
  }

  // not authorised
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                          arangodb::StaticStrings::SystemDatabase);
    v8::Isolate::CreateParams isolateParams;
    ArrayBufferAllocator arrayBufferAllocator;
    isolateParams.array_buffer_allocator = &arrayBufferAllocator;
    auto isolate =
        std::shared_ptr<v8::Isolate>(v8::Isolate::New(isolateParams),
                                     [](v8::Isolate* p) -> void { p->Dispose(); });
    ASSERT_TRUE((nullptr != isolate));
    v8::Isolate::Scope isolateScope(isolate.get());  // otherwise v8::Isolate::Logger() will fail (called from v8::Exception::Error)
    v8::internal::Isolate::Current()->InitializeLoggingAndCounters();  // otherwise v8::Isolate::Logger() will fail (called from v8::Exception::Error)
    v8::HandleScope handleScope(isolate.get());  // required for v8::Context::New(...), v8::ObjectTemplate::New(...) and TRI_AddMethodVocbase(...)
    auto context = v8::Context::New(isolate.get());
    v8::Context::Scope contextScope(context);  // required for TRI_AddMethodVocbase(...)
    std::unique_ptr<TRI_v8_global_t> v8g(TRI_CreateV8Globals(isolate.get()));  // create and set inside 'isolate' for use with 'TRI_GET_GLOBALS()'
    v8g->ArangoErrorTempl.Reset(isolate.get(), v8::ObjectTemplate::New(isolate.get()));  // otherwise v8:-utils::CreateErrorObject(...) will fail
    v8g->_vocbase = &vocbase;
    arangodb::iresearch::TRI_InitV8Analyzers(*v8g, isolate.get());

    auto v8Analyzers = v8::Local<v8::ObjectTemplate>::New(isolate.get(), v8g->IResearchAnalyzersTempl)
                           ->NewInstance();
    auto fn_remove =
        v8Analyzers->Get(TRI_V8_ASCII_STRING(isolate.get(), "remove"));
    EXPECT_TRUE((fn_remove->IsFunction()));

    std::vector<v8::Local<v8::Value>> args = {
        TRI_V8_STD_STRING(isolate.get(), arangodb::StaticStrings::SystemDatabase + "::testAnalyzer1"),
    };

    arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
    auto& user =
        userMap
            .emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP))
            .first->second;
    user.grantDatabase(vocbase.name(), arangodb::auth::Level::RO);  // for system collections User::collectionAuthLevel(...) returns database auth::Level
    userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database

    arangodb::velocypack::Builder responce;
    v8::TryCatch tryCatch(isolate.get());
    auto result = v8::Function::Cast(*fn_remove)
                      ->CallAsFunction(context, fn_remove,
                                       static_cast<int>(args.size()), args.data());
    EXPECT_TRUE((result.IsEmpty()));
    EXPECT_TRUE((tryCatch.HasCaught()));
    EXPECT_TRUE((TRI_ERROR_NO_ERROR == TRI_V8ToVPack(isolate.get(), responce,
                                                     tryCatch.Exception(), false)));
    auto slice = responce.slice();
    EXPECT_TRUE((slice.isObject()));
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::ErrorNum) &&
                 slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() &&
                 TRI_ERROR_FORBIDDEN ==
                     slice.get(arangodb::StaticStrings::ErrorNum).getNumber<int>()));
    auto analyzer = analyzers->get(arangodb::StaticStrings::SystemDatabase +
                                   "::testAnalyzer1");
    EXPECT_TRUE((false == !analyzer));
  }

  // still in use (fail)
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                          arangodb::StaticStrings::SystemDatabase);
    v8::Isolate::CreateParams isolateParams;
    ArrayBufferAllocator arrayBufferAllocator;
    isolateParams.array_buffer_allocator = &arrayBufferAllocator;
    auto isolate =
        std::shared_ptr<v8::Isolate>(v8::Isolate::New(isolateParams),
                                     [](v8::Isolate* p) -> void { p->Dispose(); });
    ASSERT_TRUE((nullptr != isolate));
    v8::Isolate::Scope isolateScope(isolate.get());  // otherwise v8::Isolate::Logger() will fail (called from v8::Exception::Error)
    v8::internal::Isolate::Current()->InitializeLoggingAndCounters();  // otherwise v8::Isolate::Logger() will fail (called from v8::Exception::Error)
    v8::HandleScope handleScope(isolate.get());  // required for v8::Context::New(...), v8::ObjectTemplate::New(...) and TRI_AddMethodVocbase(...)
    auto context = v8::Context::New(isolate.get());
    v8::Context::Scope contextScope(context);  // required for TRI_AddMethodVocbase(...)
    std::unique_ptr<TRI_v8_global_t> v8g(TRI_CreateV8Globals(isolate.get()));  // create and set inside 'isolate' for use with 'TRI_GET_GLOBALS()'
    v8g->ArangoErrorTempl.Reset(isolate.get(), v8::ObjectTemplate::New(isolate.get()));  // otherwise v8:-utils::CreateErrorObject(...) will fail
    v8g->_vocbase = &vocbase;
    arangodb::iresearch::TRI_InitV8Analyzers(*v8g, isolate.get());

    auto v8Analyzers = v8::Local<v8::ObjectTemplate>::New(isolate.get(), v8g->IResearchAnalyzersTempl)
                           ->NewInstance();
    auto fn_remove =
        v8Analyzers->Get(TRI_V8_ASCII_STRING(isolate.get(), "remove"));
    EXPECT_TRUE((fn_remove->IsFunction()));

    std::vector<v8::Local<v8::Value>> args = {
        TRI_V8_STD_STRING(isolate.get(), arangodb::StaticStrings::SystemDatabase + "::testAnalyzer2"),
        v8::False(isolate.get()),
    };
    auto inUseAnalyzer = analyzers->get(arangodb::StaticStrings::SystemDatabase +
                                        "::testAnalyzer2");  // hold ref to mark in-use
    ASSERT_TRUE((false == !inUseAnalyzer));

    arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
    auto& user =
        userMap
            .emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP))
            .first->second;
    user.grantDatabase(vocbase.name(), arangodb::auth::Level::RW);  // for system collections User::collectionAuthLevel(...) returns database auth::Level
    userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database

    arangodb::velocypack::Builder responce;
    v8::TryCatch tryCatch(isolate.get());
    auto result = v8::Function::Cast(*fn_remove)
                      ->CallAsFunction(context, fn_remove,
                                       static_cast<int>(args.size()), args.data());
    EXPECT_TRUE((result.IsEmpty()));
    EXPECT_TRUE((tryCatch.HasCaught()));
    EXPECT_TRUE((TRI_ERROR_NO_ERROR == TRI_V8ToVPack(isolate.get(), responce,
                                                     tryCatch.Exception(), false)));
    auto slice = responce.slice();
    EXPECT_TRUE((slice.isObject()));
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::ErrorNum) &&
                 slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() &&
                 TRI_ERROR_ARANGO_CONFLICT ==
                     slice.get(arangodb::StaticStrings::ErrorNum).getNumber<int>()));
    auto analyzer = analyzers->get(arangodb::StaticStrings::SystemDatabase +
                                   "::testAnalyzer2");
    EXPECT_TRUE((false == !analyzer));
  }

  // still in use + force (success)
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                          arangodb::StaticStrings::SystemDatabase);
    v8::Isolate::CreateParams isolateParams;
    ArrayBufferAllocator arrayBufferAllocator;
    isolateParams.array_buffer_allocator = &arrayBufferAllocator;
    auto isolate =
        std::shared_ptr<v8::Isolate>(v8::Isolate::New(isolateParams),
                                     [](v8::Isolate* p) -> void { p->Dispose(); });
    ASSERT_TRUE((nullptr != isolate));
    v8::Isolate::Scope isolateScope(isolate.get());  // otherwise v8::Isolate::Logger() will fail (called from v8::Exception::Error)
    v8::internal::Isolate::Current()->InitializeLoggingAndCounters();  // otherwise v8::Isolate::Logger() will fail (called from v8::Exception::Error)
    v8::HandleScope handleScope(isolate.get());  // required for v8::Context::New(...), v8::ObjectTemplate::New(...) and TRI_AddMethodVocbase(...)
    auto context = v8::Context::New(isolate.get());
    v8::Context::Scope contextScope(context);  // required for TRI_AddMethodVocbase(...)
    std::unique_ptr<TRI_v8_global_t> v8g(TRI_CreateV8Globals(isolate.get()));  // create and set inside 'isolate' for use with 'TRI_GET_GLOBALS()'
    v8g->ArangoErrorTempl.Reset(isolate.get(), v8::ObjectTemplate::New(isolate.get()));  // otherwise v8:-utils::CreateErrorObject(...) will fail
    v8g->_vocbase = &vocbase;
    arangodb::iresearch::TRI_InitV8Analyzers(*v8g, isolate.get());

    auto v8Analyzers = v8::Local<v8::ObjectTemplate>::New(isolate.get(), v8g->IResearchAnalyzersTempl)
                           ->NewInstance();
    auto fn_remove =
        v8Analyzers->Get(TRI_V8_ASCII_STRING(isolate.get(), "remove"));
    EXPECT_TRUE((fn_remove->IsFunction()));

    std::vector<v8::Local<v8::Value>> args = {
        TRI_V8_STD_STRING(isolate.get(), arangodb::StaticStrings::SystemDatabase + "::testAnalyzer2"),
        v8::True(isolate.get()),
    };
    auto inUseAnalyzer = analyzers->get(arangodb::StaticStrings::SystemDatabase +
                                        "::testAnalyzer2");  // hold ref to mark in-use
    ASSERT_TRUE((false == !inUseAnalyzer));

    arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
    auto& user =
        userMap
            .emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP))
            .first->second;
    user.grantDatabase(vocbase.name(), arangodb::auth::Level::RW);  // for system collections User::collectionAuthLevel(...) returns database auth::Level
    userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database

    auto result = v8::Function::Cast(*fn_remove)
                      ->CallAsFunction(context, fn_remove,
                                       static_cast<int>(args.size()), args.data());
    EXPECT_TRUE((!result.IsEmpty()));
    EXPECT_TRUE((result.ToLocalChecked()->IsUndefined()));
    auto analyzer = analyzers->get(arangodb::StaticStrings::SystemDatabase +
                                   "::testAnalyzer2");
    EXPECT_TRUE((true == !analyzer));
  }

  // success removal
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                          arangodb::StaticStrings::SystemDatabase);
    v8::Isolate::CreateParams isolateParams;
    ArrayBufferAllocator arrayBufferAllocator;
    isolateParams.array_buffer_allocator = &arrayBufferAllocator;
    auto isolate =
        std::shared_ptr<v8::Isolate>(v8::Isolate::New(isolateParams),
                                     [](v8::Isolate* p) -> void { p->Dispose(); });
    ASSERT_TRUE((nullptr != isolate));
    v8::Isolate::Scope isolateScope(isolate.get());  // otherwise v8::Isolate::Logger() will fail (called from v8::Exception::Error)
    v8::internal::Isolate::Current()->InitializeLoggingAndCounters();  // otherwise v8::Isolate::Logger() will fail (called from v8::Exception::Error)
    v8::HandleScope handleScope(isolate.get());  // required for v8::Context::New(...), v8::ObjectTemplate::New(...) and TRI_AddMethodVocbase(...)
    auto context = v8::Context::New(isolate.get());
    v8::Context::Scope contextScope(context);  // required for TRI_AddMethodVocbase(...)
    std::unique_ptr<TRI_v8_global_t> v8g(TRI_CreateV8Globals(isolate.get()));  // create and set inside 'isolate' for use with 'TRI_GET_GLOBALS()'
    v8g->ArangoErrorTempl.Reset(isolate.get(), v8::ObjectTemplate::New(isolate.get()));  // otherwise v8:-utils::CreateErrorObject(...) will fail
    v8g->_vocbase = &vocbase;
    arangodb::iresearch::TRI_InitV8Analyzers(*v8g, isolate.get());

    auto v8Analyzers = v8::Local<v8::ObjectTemplate>::New(isolate.get(), v8g->IResearchAnalyzersTempl)
                           ->NewInstance();
    auto fn_remove =
        v8Analyzers->Get(TRI_V8_ASCII_STRING(isolate.get(), "remove"));
    EXPECT_TRUE((fn_remove->IsFunction()));

    std::vector<v8::Local<v8::Value>> args = {
        TRI_V8_STD_STRING(isolate.get(), arangodb::StaticStrings::SystemDatabase + "::testAnalyzer1"),
    };

    arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
    auto& user =
        userMap
            .emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP))
            .first->second;
    user.grantDatabase(vocbase.name(), arangodb::auth::Level::RW);  // for system collections User::collectionAuthLevel(...) returns database auth::Level
    userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database

    auto result = v8::Function::Cast(*fn_remove)
                      ->CallAsFunction(context, fn_remove,
                                       static_cast<int>(args.size()), args.data());
    EXPECT_TRUE((!result.IsEmpty()));
    EXPECT_TRUE((result.ToLocalChecked()->IsUndefined()));
    auto analyzer = analyzers->get(arangodb::StaticStrings::SystemDatabase +
                                   "::testAnalyzer1");
    EXPECT_TRUE((true == !analyzer));
  }
}
