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

#include "src/objects/objects.h"  // must be included before src/api/api.h to avoid errors with MSVC

#include "src/api/api.h"  // must inclide V8 _before_ "catch.cpp' or CATCH() macro will be broken
// #include "src/objects-inl.h"  // (required to avoid compile warnings) must
// inclide V8 _before_ "catch.cpp' or CATCH() macro will be broken
#include "src/objects/scope-info.h"  // must inclide V8 _before_ "catch.cpp' or CATCH() macro will be broken

#include "Aql/OptimizerRulesFeature.h"
#include "gtest/gtest.h"

#include "analysis/analyzers.hpp"
#include "analysis/token_attributes.hpp"

#include <velocypack/Iterator.h>
#include <velocypack/Parser.h>

#include "IResearch/common.h"
#include "Mocks/LogLevels.h"

#include "V8/V8SecurityFeature.h"
#include "ApplicationFeatures/HttpEndpointProvider.h"
#include "Aql/QueryRegistry.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "IResearch/IResearchAnalyzerFeature.h"
#include "IResearch/IResearchCommon.h"
#include "IResearch/VelocyPackHelper.h"
#include "RestServer/AqlFeature.h"
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
#include "VocBase/Methods/Collections.h"

#if USE_ENTERPRISE
#include "Enterprise/Ldap/LdapFeature.h"
#include "Enterprise/Encryption/EncryptionFeature.h"
#endif

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

class EmptyAnalyzer final : public irs::analysis::TypedAnalyzer<EmptyAnalyzer> {
 public:
  static constexpr std::string_view type_name() noexcept {
    return "v8-analyzer-empty";
  }
  EmptyAnalyzer() = default;
  irs::attribute* get_mutable(irs::type_info::type_id type) noexcept final {
    if (type == irs::type<irs::frequency>::id()) {
      return &_attr;
    }

    return nullptr;
  }
  static ptr make(std::string_view) {
    return std::make_unique<EmptyAnalyzer>();
  }
  static bool normalize(std::string_view args, std::string& out) {
    auto slice = arangodb::iresearch::slice(args);
    if (slice.isNull()) throw std::exception();
    if (slice.isNone()) return false;
    arangodb::velocypack::Builder builder;
    if (slice.isString()) {
      VPackObjectBuilder scope(&builder);
      arangodb::iresearch::addStringRef(
          builder, "args", arangodb::iresearch::getStringRef(slice));
    } else if (slice.isObject() && slice.hasKey("args") &&
               slice.get("args").isString()) {
      VPackObjectBuilder scope(&builder);
      arangodb::iresearch::addStringRef(
          builder, "args",
          arangodb::iresearch::getStringRef(slice.get("args")));
    } else {
      return false;
    }
    out = builder.buffer()->toString();
    return true;
  }
  bool next() final { return false; }
  bool reset(std::string_view data) final { return true; }

 private:
  irs::frequency _attr;
};

REGISTER_ANALYZER_VPACK(EmptyAnalyzer, EmptyAnalyzer::make,
                        EmptyAnalyzer::normalize);

}  // namespace

class V8AnalyzerTest
    : public ::testing::Test,
      public arangodb::tests::LogSuppressor<arangodb::Logger::AUTHENTICATION,
                                            arangodb::LogLevel::ERR> {
 protected:
  arangodb::tests::mocks::MockAqlServer server;

  V8AnalyzerTest() {
    arangodb::tests::v8Init();  // one-time initialize V8
  }
};

v8::Local<v8::Object> getAnalyzerManagerInstance(TRI_v8_global_t* v8g,
                                                 v8::Isolate* isolate) {
  return v8::Local<v8::ObjectTemplate>::New(isolate,
                                            v8g->IResearchAnalyzerManagerTempl)
      ->NewInstance(TRI_IGETC)
      .FromMaybe(v8::Local<v8::Object>());
}

v8::Local<v8::Object> getAnalyzersInstance(TRI_v8_global_t* v8g,
                                           v8::Isolate* isolate) {
  return v8::Local<v8::ObjectTemplate>::New(isolate,
                                            v8g->IResearchAnalyzerInstanceTempl)
      ->NewInstance(TRI_IGETC)
      .FromMaybe(v8::Local<v8::Object>());
}

v8::Local<v8::Function> getAnalyzersMethodFunction(
    v8::Isolate* isolate, v8::Local<v8::Object>& analyzerObj,
    const char* name) {
  auto fn = analyzerObj->Get(TRI_IGETC, TRI_V8_ASCII_STRING(isolate, name))
                .FromMaybe(v8::Local<v8::Value>());
  bool isFunction = fn->IsFunction();
  EXPECT_TRUE(isFunction);
  return v8::Local<v8::Function>::Cast(fn);
}

TEST_F(V8AnalyzerTest, test_instance_accessors) {
  using namespace arangodb;
  using namespace arangodb::application_features;

  ASSERT_TRUE(server.server().hasFeature<CommunicationFeaturePhase>());
  ASSERT_TRUE(server.server().hasFeature<V8SecurityFeature>());
  ASSERT_TRUE(server.server().hasFeature<HttpEndpointProvider>());
#ifdef USE_ENTERPRISE
  ASSERT_TRUE(server.server().hasFeature<EncryptionFeature>());
#endif

  auto& analyzers =
      server.getFeature<arangodb::iresearch::IResearchAnalyzerFeature>();
  auto& dbFeature = server.getFeature<arangodb::DatabaseFeature>();

  {
    auto vocbase =
        dbFeature.useDatabase(arangodb::StaticStrings::SystemDatabase);
    std::shared_ptr<arangodb::LogicalCollection> ignored;
    arangodb::OperationOptions options(arangodb::ExecContext::current());
    arangodb::methods::Collections::createSystem(
        *vocbase, options, arangodb::tests::AnalyzerCollectionName, false,
        ignored);
  }

  arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;
  ASSERT_TRUE(
      analyzers
          .emplace(result,
                   arangodb::StaticStrings::SystemDatabase + "::testAnalyzer1",
                   "identity", VPackSlice::noneSlice(),
                   arangodb::transaction::Hints::TrxType::INTERNAL)
          .ok());
  auto analyzer =
      analyzers.get(arangodb::StaticStrings::SystemDatabase + "::testAnalyzer1",
                    arangodb::QueryAnalyzerRevisions::QUERY_LATEST,
                    arangodb::transaction::Hints::TrxType::INTERNAL);
  ASSERT_FALSE(!analyzer);

  struct ExecContext : public arangodb::ExecContext {
    ExecContext()
        : arangodb::ExecContext(arangodb::ExecContext::Type::Default, "", "",
                                arangodb::auth::Level::NONE,
                                arangodb::auth::Level::NONE, false) {}
  } execContext;
  arangodb::ExecContextScope execContextScope(&execContext);
  auto& authFeature = server.getFeature<arangodb::AuthenticationFeature>();
  auto* userManager = authFeature.userManager();

  arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
  TRI_vocbase_t vocbase(systemDBInfo(server.server()));
  v8::Isolate::CreateParams isolateParams;
  ArrayBufferAllocator arrayBufferAllocator;
  isolateParams.array_buffer_allocator = &arrayBufferAllocator;
  auto* isolate = v8::Isolate::New(isolateParams);
  ASSERT_NE(nullptr, isolate);
  irs::Finally cleanup = [isolate]() noexcept { isolate->Dispose(); };
  // otherwise v8::Isolate::Logger() will fail (called from
  // v8::Exception::Error)
  v8::Isolate::Scope isolateScope(isolate);
  // otherwise v8::Isolate::Logger() will fail (called from
  // v8::Exception::Error)
  v8::internal::Isolate::Current()->InitializeLoggingAndCounters();
  // required for v8::Context::New(...), v8::ObjectTemplate::New(...) and
  // TRI_AddMethodVocbase(...)
  v8::HandleScope handleScope(isolate);
  auto context = v8::Context::New(isolate);
  // required for TRI_AddMethodVocbase(...)
  v8::Context::Scope contextScope(context);
  // create and set inside 'isolate' for use with 'TRI_GET_GLOBALS()'
  std::unique_ptr<V8Global<arangodb::ArangodServer>> v8g(
      CreateV8Globals(server.server(), isolate, 0));
  // otherwise v8:-utils::CreateErrorObject(...) will fail
  v8g->ArangoErrorTempl.Reset(isolate, v8::ObjectTemplate::New(isolate));
  v8g->_vocbase = &vocbase;
  arangodb::iresearch::TRI_InitV8Analyzers(*v8g, isolate);

  auto v8Analyzer = getAnalyzersInstance(v8g.get(), isolate);
  auto fn_name = getAnalyzersMethodFunction(isolate, v8Analyzer, "name");
  auto fn_type = getAnalyzersMethodFunction(isolate, v8Analyzer, "type");
  auto fn_properties =
      getAnalyzersMethodFunction(isolate, v8Analyzer, "properties");
  auto fn_features =
      getAnalyzersMethodFunction(isolate, v8Analyzer, "features");

  v8Analyzer->SetInternalField(
      SLOT_CLASS_TYPE, v8::Integer::New(isolate, WRP_IRESEARCH_ANALYZER_TYPE));
  v8Analyzer->SetInternalField(SLOT_CLASS,
                               v8::External::New(isolate, analyzer.get()));
  // test name (authorised)
  {
    std::vector<v8::Local<v8::Value>> args = {};

    arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
    auto& user = userMap
                     .emplace("", arangodb::auth::User::newUser(
                                      "", "", arangodb::auth::Source::LDAP))
                     .first->second;
    user.grantDatabase(
        vocbase.name(),
        arangodb::auth::Level::RO);  // for system collections
                                     // User::collectionAuthLevel(...) returns
                                     // database auth::Level
    userManager->setAuthInfo(userMap);  // set user map to avoid loading
                                        // configuration from system database

    auto result = fn_name->CallAsFunction(
        context, v8Analyzer, static_cast<int>(args.size()), args.data());
    ASSERT_FALSE(result.IsEmpty());
    ASSERT_TRUE(result.ToLocalChecked()->IsString());
    EXPECT_TRUE((analyzer->name() ==
                 TRI_ObjectToString(isolate, result.ToLocalChecked())));
  }

  // test name (not authorised)
  {
    std::vector<v8::Local<v8::Value>> args = {};

    arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
    auto& user = userMap
                     .emplace("", arangodb::auth::User::newUser(
                                      "", "", arangodb::auth::Source::LDAP))
                     .first->second;
    user.grantDatabase(
        vocbase.name(),
        arangodb::auth::Level::NONE);  // for system collections
                                       // User::collectionAuthLevel(...) returns
                                       // database auth::Level
    userManager->setAuthInfo(userMap);  // set user map to avoid loading
                                        // configuration from system database

    arangodb::velocypack::Builder response;
    v8::TryCatch tryCatch(isolate);
    auto result = fn_name->CallAsFunction(
        context, v8Analyzer, static_cast<int>(args.size()), args.data());
    ASSERT_TRUE(result.IsEmpty());
    ASSERT_TRUE(tryCatch.HasCaught());
    TRI_V8ToVPack(isolate, response, tryCatch.Exception(), false);
    auto slice = response.slice();
    EXPECT_TRUE(slice.isObject());
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::ErrorNum) &&
                 slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() &&
                 TRI_ERROR_FORBIDDEN ==
                     ErrorCode{slice.get(arangodb::StaticStrings::ErrorNum)
                                   .getNumber<int>()}));
  }

  // test type (authorised)
  {
    std::vector<v8::Local<v8::Value>> args = {};

    arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
    auto& user = userMap
                     .emplace("", arangodb::auth::User::newUser(
                                      "", "", arangodb::auth::Source::LDAP))
                     .first->second;
    user.grantDatabase(
        vocbase.name(),
        arangodb::auth::Level::RO);  // for system collections
                                     // User::collectionAuthLevel(...) returns
                                     // database auth::Level
    userManager->setAuthInfo(userMap);  // set user map to avoid loading
                                        // configuration from system database

    auto result = fn_type->CallAsFunction(
        context, v8Analyzer, static_cast<int>(args.size()), args.data());
    ASSERT_FALSE(result.IsEmpty());
    ASSERT_TRUE(result.ToLocalChecked()->IsString());
    EXPECT_TRUE((analyzer->type() ==
                 TRI_ObjectToString(isolate, result.ToLocalChecked())));
  }

  // test type (not authorised)
  {
    std::vector<v8::Local<v8::Value>> args = {};

    arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
    auto& user = userMap
                     .emplace("", arangodb::auth::User::newUser(
                                      "", "", arangodb::auth::Source::LDAP))
                     .first->second;
    user.grantDatabase(
        vocbase.name(),
        arangodb::auth::Level::NONE);  // for system collections
                                       // User::collectionAuthLevel(...) returns
                                       // database auth::Level
    userManager->setAuthInfo(userMap);  // set user map to avoid loading
                                        // configuration from system database

    arangodb::velocypack::Builder response;
    v8::TryCatch tryCatch(isolate);
    auto result = fn_type->CallAsFunction(
        context, v8Analyzer, static_cast<int>(args.size()), args.data());
    ASSERT_TRUE(result.IsEmpty());
    ASSERT_TRUE(tryCatch.HasCaught());
    TRI_V8ToVPack(isolate, response, tryCatch.Exception(), false);
    auto slice = response.slice();
    EXPECT_TRUE(slice.isObject());
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::ErrorNum) &&
                 slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() &&
                 TRI_ERROR_FORBIDDEN ==
                     ErrorCode{slice.get(arangodb::StaticStrings::ErrorNum)
                                   .getNumber<int>()}));
  }

  // test properties (authorised)
  {
    std::vector<v8::Local<v8::Value>> args = {};

    arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
    auto& user = userMap
                     .emplace("", arangodb::auth::User::newUser(
                                      "", "", arangodb::auth::Source::LDAP))
                     .first->second;
    user.grantDatabase(
        vocbase.name(),
        arangodb::auth::Level::RO);  // for system collections
                                     // User::collectionAuthLevel(...) returns
                                     // database auth::Level
    userManager->setAuthInfo(userMap);  // set user map to avoid loading
                                        // configuration from system database

    auto result = fn_properties->CallAsFunction(
        context, v8Analyzer, static_cast<int>(args.size()), args.data());

    ASSERT_FALSE(result.IsEmpty());
    ASSERT_TRUE(result.ToLocalChecked()->IsObject());
    VPackBuilder resultVPack;
    TRI_V8ToVPack(isolate, resultVPack, result.ToLocalChecked(), false);
    EXPECT_EQUAL_SLICES(resultVPack.slice(), VPackSlice::emptyObjectSlice());
  }

  // test properties (not authorised)
  {
    std::vector<v8::Local<v8::Value>> args = {};

    arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
    auto& user = userMap
                     .emplace("", arangodb::auth::User::newUser(
                                      "", "", arangodb::auth::Source::LDAP))
                     .first->second;
    user.grantDatabase(
        vocbase.name(),
        arangodb::auth::Level::NONE);  // for system collections
                                       // User::collectionAuthLevel(...) returns
                                       // database auth::Level
    userManager->setAuthInfo(userMap);  // set user map to avoid loading
                                        // configuration from system database

    arangodb::velocypack::Builder response;
    v8::TryCatch tryCatch(isolate);
    auto result = fn_properties->CallAsFunction(
        context, v8Analyzer, static_cast<int>(args.size()), args.data());
    ASSERT_TRUE(result.IsEmpty());
    ASSERT_TRUE(tryCatch.HasCaught());
    TRI_V8ToVPack(isolate, response, tryCatch.Exception(), false);
    auto slice = response.slice();
    ASSERT_TRUE(slice.isObject());
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::ErrorNum) &&
                 slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() &&
                 TRI_ERROR_FORBIDDEN ==
                     ErrorCode{slice.get(arangodb::StaticStrings::ErrorNum)
                                   .getNumber<int>()}));
  }

  // test features (authorised)
  {
    std::vector<v8::Local<v8::Value>> args = {};

    arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
    auto& user = userMap
                     .emplace("", arangodb::auth::User::newUser(
                                      "", "", arangodb::auth::Source::LDAP))
                     .first->second;
    user.grantDatabase(
        vocbase.name(),
        arangodb::auth::Level::RO);  // for system collections
                                     // User::collectionAuthLevel(...) returns
                                     // database auth::Level
    userManager->setAuthInfo(userMap);  // set user map to avoid loading
                                        // configuration from system database

    auto result = fn_features->CallAsFunction(
        context, v8Analyzer, static_cast<int>(args.size()), args.data());
    ASSERT_FALSE(result.IsEmpty());
    ASSERT_TRUE(result.ToLocalChecked()->IsArray());
    auto v8Result = v8::Handle<v8::Array>::Cast(result.ToLocalChecked());

    size_t size = 0;
    analyzer->features().visit([&size](std::string_view) { ++size; });
    EXPECT_EQ(size, v8Result->Length());
  }

  // test features (not authorised)
  {
    std::vector<v8::Local<v8::Value>> args = {};

    arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
    auto& user = userMap
                     .emplace("", arangodb::auth::User::newUser(
                                      "", "", arangodb::auth::Source::LDAP))
                     .first->second;
    user.grantDatabase(
        vocbase.name(),
        arangodb::auth::Level::NONE);  // for system collections
                                       // User::collectionAuthLevel(...) returns
                                       // database auth::Level
    userManager->setAuthInfo(userMap);  // set user map to avoid loading
                                        // configuration from system database

    arangodb::velocypack::Builder response;
    v8::TryCatch tryCatch(isolate);
    auto result = fn_features->CallAsFunction(
        context, v8Analyzer, static_cast<int>(args.size()), args.data());
    ASSERT_TRUE(result.IsEmpty());
    ASSERT_TRUE(tryCatch.HasCaught());
    TRI_V8ToVPack(isolate, response, tryCatch.Exception(), false);
    auto slice = response.slice();
    EXPECT_TRUE(slice.isObject());
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::ErrorNum) &&
                 slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() &&
                 TRI_ERROR_FORBIDDEN ==
                     ErrorCode{slice.get(arangodb::StaticStrings::ErrorNum)
                                   .getNumber<int>()}));
  }
}

TEST_F(V8AnalyzerTest, test_manager_create) {
  auto& analyzers =
      server.getFeature<arangodb::iresearch::IResearchAnalyzerFeature>();
  auto& dbFeature = server.getFeature<arangodb::DatabaseFeature>();

  {
    auto vocbase =
        dbFeature.useDatabase(arangodb::StaticStrings::SystemDatabase);
    std::shared_ptr<arangodb::LogicalCollection> ignored;
    arangodb::OperationOptions options(arangodb::ExecContext::current());
    arangodb::methods::Collections::createSystem(
        *vocbase, options, arangodb::tests::AnalyzerCollectionName, false,
        ignored);
  }
  arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;

  {
    const auto name =
        arangodb::StaticStrings::SystemDatabase + "::testAnalyzer1";
    ASSERT_TRUE(analyzers
                    .emplace(result, name, "identity", VPackSlice::noneSlice(),
                             arangodb::transaction::Hints::TrxType::INTERNAL)
                    .ok());
  }

  {
    const auto name =
        arangodb::StaticStrings::SystemDatabase + "::emptyAnalyzer";
    ASSERT_TRUE(
        analyzers
            .emplace(result, name, "v8-analyzer-empty",
                     VPackParser::fromJson("{\"args\":\"12312\"}")->slice(),
                     arangodb::transaction::Hints::TrxType::INTERNAL,
                     arangodb::iresearch::Features(irs::IndexFeatures::FREQ))
            .ok());
  }

  struct ExecContext : public arangodb::ExecContext {
    ExecContext()
        : arangodb::ExecContext(arangodb::ExecContext::Type::Default, "", "",
                                arangodb::auth::Level::NONE,
                                arangodb::auth::Level::NONE, false) {}
  } execContext;
  arangodb::ExecContextScope execContextScope(&execContext);
  auto& authFeature = server.getFeature<arangodb::AuthenticationFeature>();
  auto* userManager = authFeature.userManager();

  TRI_vocbase_t vocbase(systemDBInfo(server.server()));
  v8::Isolate::CreateParams isolateParams;
  ArrayBufferAllocator arrayBufferAllocator;
  isolateParams.array_buffer_allocator = &arrayBufferAllocator;
  auto* isolate = v8::Isolate::New(isolateParams);
  ASSERT_NE(nullptr, isolate);
  irs::Finally cleanup = [isolate]() noexcept { isolate->Dispose(); };

  // otherwise v8::Isolate::Logger() will fail (called from
  // v8::Exception::Error)
  v8::Isolate::Scope isolateScope(isolate);

  // otherwise v8::Isolate::Logger() will fail (called from
  // v8::Exception::Error)
  v8::internal::Isolate::Current()->InitializeLoggingAndCounters();

  // required for v8::Context::New(...), v8::ObjectTemplate::New(...) and
  // TRI_AddMethodVocbase(...)
  v8::HandleScope handleScope(isolate);
  auto context = v8::Context::New(isolate);

  // required for TRI_AddMethodVocbase(...)
  v8::Context::Scope contextScope(context);
  std::unique_ptr<V8Global<arangodb::ArangodServer>> v8g(
      CreateV8Globals(server.server(), isolate, 0));

  // otherwise v8:-utils::CreateErrorObject(...) will fail
  v8g->ArangoErrorTempl.Reset(isolate, v8::ObjectTemplate::New(isolate));
  v8g->_vocbase = &vocbase;
  arangodb::iresearch::TRI_InitV8Analyzers(*v8g, isolate);

  auto v8AnalyzerManager = getAnalyzerManagerInstance(v8g.get(), isolate);
  auto fn_save = getAnalyzersMethodFunction(isolate, v8AnalyzerManager, "save");

  // invalid params (no args)
  {
    std::vector<v8::Local<v8::Value>> args = {};

    arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
    auto& user = userMap
                     .emplace("", arangodb::auth::User::newUser(
                                      "", "", arangodb::auth::Source::LDAP))
                     .first->second;

    // for system collections
    // User::collectionAuthLevel(...) returns
    // database auth::Level
    user.grantDatabase(vocbase.name(), arangodb::auth::Level::RW);

    // set user map to avoid loading
    // configuration from system database
    userManager->setAuthInfo(userMap);

    arangodb::velocypack::Builder response;
    v8::TryCatch tryCatch(isolate);
    auto result = fn_save->CallAsFunction(
        context, fn_save, static_cast<int>(args.size()), args.data());
    ASSERT_TRUE(result.IsEmpty());
    ASSERT_TRUE(tryCatch.HasCaught());
    TRI_V8ToVPack(isolate, response, tryCatch.Exception(), false);
    auto slice = response.slice();
    ASSERT_TRUE(slice.isObject());
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::ErrorNum) &&
                 slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() &&
                 TRI_ERROR_BAD_PARAMETER ==
                     ErrorCode{slice.get(arangodb::StaticStrings::ErrorNum)
                                   .getNumber<int>()}));
  }

  // invalid params (invalid type)
  {
    std::vector<v8::Local<v8::Value>> args = {
        TRI_V8_STD_STRING(isolate, arangodb::StaticStrings::SystemDatabase +
                                       "::testAnalyzer2"),
        v8::True(isolate),
    };

    arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
    auto& user = userMap
                     .emplace("", arangodb::auth::User::newUser(
                                      "", "", arangodb::auth::Source::LDAP))
                     .first->second;
    user.grantDatabase(
        vocbase.name(),
        arangodb::auth::Level::RW);  // for system collections
                                     // User::collectionAuthLevel(...) returns
                                     // database auth::Level
    userManager->setAuthInfo(userMap);  // set user map to avoid loading
                                        // configuration from system database

    arangodb::velocypack::Builder response;
    v8::TryCatch tryCatch(isolate);
    auto result = fn_save->CallAsFunction(
        context, fn_save, static_cast<int>(args.size()), args.data());
    ASSERT_TRUE(result.IsEmpty());
    ASSERT_TRUE(tryCatch.HasCaught());
    TRI_V8ToVPack(isolate, response, tryCatch.Exception(), false);
    auto slice = response.slice();
    ASSERT_TRUE(slice.isObject());
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::ErrorNum) &&
                 slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() &&
                 TRI_ERROR_BAD_PARAMETER ==
                     ErrorCode{slice.get(arangodb::StaticStrings::ErrorNum)
                                   .getNumber<int>()}));
  }

  // invalid params (invalid name)
  {
    std::vector<v8::Local<v8::Value>> args = {
        TRI_V8_STD_STRING(isolate, arangodb::StaticStrings::SystemDatabase +
                                       "::test:Analyzer2"),
        TRI_V8_ASCII_STRING(isolate, "identity"),
        v8::True(isolate),
    };

    arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
    auto& user = userMap
                     .emplace("", arangodb::auth::User::newUser(
                                      "", "", arangodb::auth::Source::LDAP))
                     .first->second;

    // for system collections User::collectionAuthLevel(...) returns
    // database auth::Level
    user.grantDatabase(vocbase.name(), arangodb::auth::Level::RW);

    // set user map to avoid loading
    // configuration from system database
    userManager->setAuthInfo(userMap);

    arangodb::velocypack::Builder response;
    v8::TryCatch tryCatch(isolate);
    auto result = fn_save->CallAsFunction(
        context, fn_save, static_cast<int>(args.size()), args.data());
    ASSERT_TRUE(result.IsEmpty());
    ASSERT_TRUE(tryCatch.HasCaught());
    TRI_V8ToVPack(isolate, response, tryCatch.Exception(), false);
    auto slice = response.slice();
    ASSERT_TRUE(slice.isObject());
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::ErrorNum) &&
                 slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() &&
                 TRI_ERROR_BAD_PARAMETER ==
                     ErrorCode{slice.get(arangodb::StaticStrings::ErrorNum)
                                   .getNumber<int>()}));
  }

  // invalid params (invalid name)
  {
    std::vector<v8::Local<v8::Value>> args = {
        TRI_V8_STD_STRING(isolate, "::test:Analyzer2"s),
        TRI_V8_ASCII_STRING(isolate, "identity"),
        v8::True(isolate),
    };

    arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
    auto& user = userMap
                     .emplace("", arangodb::auth::User::newUser(
                                      "", "", arangodb::auth::Source::LDAP))
                     .first->second;
    user.grantDatabase(
        vocbase.name(),
        arangodb::auth::Level::RW);  // for system collections
                                     // User::collectionAuthLevel(...) returns
                                     // database auth::Level
    userManager->setAuthInfo(userMap);  // set user map to avoid loading
                                        // configuration from system database

    arangodb::velocypack::Builder response;
    v8::TryCatch tryCatch(isolate);
    auto result = fn_save->CallAsFunction(
        context, fn_save, static_cast<int>(args.size()), args.data());
    ASSERT_TRUE(result.IsEmpty());
    ASSERT_TRUE(tryCatch.HasCaught());
    TRI_V8ToVPack(isolate, response, tryCatch.Exception(), false);
    auto slice = response.slice();
    ASSERT_TRUE(slice.isObject());
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::ErrorNum) &&
                 slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() &&
                 TRI_ERROR_BAD_PARAMETER ==
                     ErrorCode{slice.get(arangodb::StaticStrings::ErrorNum)
                                   .getNumber<int>()}));
  }

  // invalid params (invalid name)
  {
    std::vector<v8::Local<v8::Value>> args = {
        TRI_V8_ASCII_STRING(isolate, "unknownVocbase::testAnalyzer"),
        TRI_V8_ASCII_STRING(isolate, "identity"),
        v8::Null(isolate),
    };

    arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
    auto& user = userMap
                     .emplace("", arangodb::auth::User::newUser(
                                      "", "", arangodb::auth::Source::LDAP))
                     .first->second;
    user.grantDatabase(
        vocbase.name(),
        arangodb::auth::Level::RW);  // for system collections
                                     // User::collectionAuthLevel(...) returns
                                     // database auth::Level
    userManager->setAuthInfo(userMap);  // set user map to avoid loading
                                        // configuration from system database

    arangodb::velocypack::Builder response;
    v8::TryCatch tryCatch(isolate);
    auto result = fn_save->CallAsFunction(
        context, fn_save, static_cast<int>(args.size()), args.data());
    ASSERT_TRUE(result.IsEmpty());
    ASSERT_TRUE(tryCatch.HasCaught());
    TRI_V8ToVPack(isolate, response, tryCatch.Exception(), false);
    auto slice = response.slice();
    ASSERT_TRUE(slice.isObject());
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::ErrorNum) &&
                 slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() &&
                 TRI_ERROR_FORBIDDEN ==
                     ErrorCode{slice.get(arangodb::StaticStrings::ErrorNum)
                                   .getNumber<int>()}));
  }

  // name collision
  {
    std::vector<v8::Local<v8::Value>> args = {
        TRI_V8_STD_STRING(isolate, "emptyAnalyzer"s),
        TRI_V8_ASCII_STRING(isolate, "v8-analyzer-empty"),
        TRI_V8_ASCII_STRING(isolate, "{\"abc\":1}"),
    };

    arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
    auto& user = userMap
                     .emplace("", arangodb::auth::User::newUser(
                                      "", "", arangodb::auth::Source::LDAP))
                     .first->second;
    user.grantDatabase(
        vocbase.name(),
        arangodb::auth::Level::RW);  // for system collections
                                     // User::collectionAuthLevel(...) returns
                                     // database auth::Level
    userManager->setAuthInfo(userMap);  // set user map to avoid loading
                                        // configuration from system database

    arangodb::velocypack::Builder response;
    v8::TryCatch tryCatch(isolate);
    auto result = fn_save->CallAsFunction(
        context, fn_save, static_cast<int>(args.size()), args.data());
    ASSERT_TRUE(result.IsEmpty());
    ASSERT_TRUE(tryCatch.HasCaught());
    TRI_V8ToVPack(isolate, response, tryCatch.Exception(), false);
    auto slice = response.slice();
    ASSERT_TRUE(slice.isObject());
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::ErrorNum) &&
                 slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() &&
                 TRI_ERROR_BAD_PARAMETER ==
                     ErrorCode{slice.get(arangodb::StaticStrings::ErrorNum)
                                   .getNumber<int>()}));
  }

  // duplicate matching
  {
    std::vector<v8::Local<v8::Value>> args = {
        TRI_V8_STD_STRING(isolate, "testAnalyzer1"s),
        TRI_V8_ASCII_STRING(isolate, "identity"),
        v8::Null(isolate),
    };

    arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
    auto& user = userMap
                     .emplace("", arangodb::auth::User::newUser(
                                      "", "", arangodb::auth::Source::LDAP))
                     .first->second;
    user.grantDatabase(
        vocbase.name(),
        arangodb::auth::Level::RW);  // for system collections
                                     // User::collectionAuthLevel(...) returns
                                     // database auth::Level
    userManager->setAuthInfo(userMap);  // set user map to avoid loading
                                        // configuration from system database

    auto result = fn_save->CallAsFunction(
        context, fn_save, static_cast<int>(args.size()), args.data());
    ASSERT_FALSE(result.IsEmpty());
    ASSERT_TRUE(result.ToLocalChecked()->IsObject());
    auto* v8AnalyzerWeak = TRI_UnwrapClass<arangodb::iresearch::AnalyzerPool>(
        result.ToLocalChecked()->ToObject(TRI_IGETC).FromMaybe(
            v8::Local<v8::Object>()),
        WRP_IRESEARCH_ANALYZER_TYPE, TRI_IGETC);
    ASSERT_FALSE(!v8AnalyzerWeak);
    ASSERT_EQ(arangodb::StaticStrings::SystemDatabase + "::testAnalyzer1",
              v8AnalyzerWeak->name());
    ASSERT_EQ("identity", v8AnalyzerWeak->type());
    EXPECT_EQUAL_SLICES(VPackSlice::emptyObjectSlice(),
                        v8AnalyzerWeak->properties());
    ASSERT_EQ(v8AnalyzerWeak->features(), arangodb::iresearch::Features{});
    auto analyzer = analyzers.get(
        arangodb::StaticStrings::SystemDatabase + "::testAnalyzer1",
        arangodb::QueryAnalyzerRevisions::QUERY_LATEST,
        arangodb::transaction::Hints::TrxType::INTERNAL);
    EXPECT_FALSE(!analyzer);
  }

  // not authorised
  {
    std::vector<v8::Local<v8::Value>> args = {
        TRI_V8_STD_STRING(isolate, "testAnalyzer2"s),
        TRI_V8_ASCII_STRING(isolate, "identity"),
        TRI_V8_ASCII_STRING(isolate, "{\"abc\":1}"),
    };

    arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
    auto& user = userMap
                     .emplace("", arangodb::auth::User::newUser(
                                      "", "", arangodb::auth::Source::LDAP))
                     .first->second;
    user.grantDatabase(
        vocbase.name(),
        arangodb::auth::Level::RO);  // for system collections
                                     // User::collectionAuthLevel(...) returns
                                     // database auth::Level
    userManager->setAuthInfo(userMap);  // set user map to avoid loading
                                        // configuration from system database

    arangodb::velocypack::Builder response;
    v8::TryCatch tryCatch(isolate);
    auto result = fn_save->CallAsFunction(
        context, fn_save, static_cast<int>(args.size()), args.data());
    ASSERT_TRUE(result.IsEmpty());
    ASSERT_TRUE(tryCatch.HasCaught());
    TRI_V8ToVPack(isolate, response, tryCatch.Exception(), false);
    auto slice = response.slice();
    ASSERT_TRUE(slice.isObject());
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::ErrorNum) &&
                 slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() &&
                 TRI_ERROR_FORBIDDEN ==
                     ErrorCode{slice.get(arangodb::StaticStrings::ErrorNum)
                                   .getNumber<int>()}));
  }

  // successful creation
  {
    std::vector<v8::Local<v8::Value>> args = {
        TRI_V8_STD_STRING(isolate, "testAnalyzer2"s),
        TRI_V8_ASCII_STRING(isolate, "identity"),
        TRI_V8_ASCII_STRING(isolate, "{\"abc\":1}")};

    arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
    auto& user = userMap
                     .emplace("", arangodb::auth::User::newUser(
                                      "", "", arangodb::auth::Source::LDAP))
                     .first->second;
    user.grantDatabase(
        vocbase.name(),
        arangodb::auth::Level::RW);  // for system collections
                                     // User::collectionAuthLevel(...) returns
                                     // database auth::Level
    userManager->setAuthInfo(userMap);  // set user map to avoid loading
                                        // configuration from system database

    auto result = fn_save->CallAsFunction(
        context, fn_save, static_cast<int>(args.size()), args.data());
    ASSERT_FALSE(result.IsEmpty());
    ASSERT_TRUE(result.ToLocalChecked()->IsObject());
    auto* v8AnalyzerWeak = TRI_UnwrapClass<arangodb::iresearch::AnalyzerPool>(
        result.ToLocalChecked()->ToObject(TRI_IGETC).FromMaybe(
            v8::Local<v8::Object>()),
        WRP_IRESEARCH_ANALYZER_TYPE, TRI_IGETC);
    ASSERT_FALSE(!v8AnalyzerWeak);
    ASSERT_EQ(arangodb::StaticStrings::SystemDatabase + "::testAnalyzer2",
              v8AnalyzerWeak->name());
    ASSERT_EQ("identity", v8AnalyzerWeak->type());
    EXPECT_EQUAL_SLICES(VPackSlice::emptyObjectSlice(),
                        v8AnalyzerWeak->properties());
    ASSERT_EQ(v8AnalyzerWeak->features(), arangodb::iresearch::Features{});
    auto analyzer = analyzers.get(
        arangodb::StaticStrings::SystemDatabase + "::testAnalyzer2",
        arangodb::QueryAnalyzerRevisions::QUERY_LATEST,
        arangodb::transaction::Hints::TrxType::INTERNAL);
    EXPECT_FALSE(!analyzer);
  }
  // successful creation with DB name prefix
  {
    std::vector<v8::Local<v8::Value>> args = {
        TRI_V8_STD_STRING(isolate, vocbase.name() + "::testAnalyzer3"s),
        TRI_V8_ASCII_STRING(isolate, "identity"),
        TRI_V8_ASCII_STRING(isolate, "{\"abc\":1}")};

    arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
    auto& user = userMap
                     .emplace("", arangodb::auth::User::newUser(
                                      "", "", arangodb::auth::Source::LDAP))
                     .first->second;
    user.grantDatabase(
        vocbase.name(),
        arangodb::auth::Level::RW);  // for system collections
                                     // User::collectionAuthLevel(...) returns
                                     // database auth::Level
    userManager->setAuthInfo(userMap);  // set user map to avoid loading
                                        // configuration from system database

    auto result = fn_save->CallAsFunction(
        context, fn_save, static_cast<int>(args.size()), args.data());
    ASSERT_FALSE(result.IsEmpty());
    ASSERT_TRUE(result.ToLocalChecked()->IsObject());
    auto* v8AnalyzerWeak = TRI_UnwrapClass<arangodb::iresearch::AnalyzerPool>(
        result.ToLocalChecked()->ToObject(TRI_IGETC).FromMaybe(
            v8::Local<v8::Object>()),
        WRP_IRESEARCH_ANALYZER_TYPE, TRI_IGETC);
    ASSERT_FALSE(!v8AnalyzerWeak);
    ASSERT_EQ(vocbase.name() + "::testAnalyzer3", v8AnalyzerWeak->name());
    ASSERT_EQ("identity", v8AnalyzerWeak->type());
    EXPECT_EQUAL_SLICES(VPackSlice::emptyObjectSlice(),
                        v8AnalyzerWeak->properties());
    ASSERT_EQ(v8AnalyzerWeak->features(), arangodb::iresearch::Features{});
    auto analyzer =
        analyzers.get(vocbase.name() + "::testAnalyzer3",
                      arangodb::QueryAnalyzerRevisions::QUERY_LATEST,
                      arangodb::transaction::Hints::TrxType::INTERNAL);
    EXPECT_FALSE(!analyzer);
  }
  // successful creation in system db by :: prefix
  {
    std::vector<v8::Local<v8::Value>> args = {
        TRI_V8_STD_STRING(isolate, "::testAnalyzer4"s),
        TRI_V8_ASCII_STRING(isolate, "identity"),
        TRI_V8_ASCII_STRING(isolate, "{\"abc\":1}")};

    arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
    auto& user = userMap
                     .emplace("", arangodb::auth::User::newUser(
                                      "", "", arangodb::auth::Source::LDAP))
                     .first->second;
    user.grantDatabase(
        vocbase.name(),
        arangodb::auth::Level::RW);  // for system collections
                                     // User::collectionAuthLevel(...) returns
                                     // database auth::Level
    userManager->setAuthInfo(userMap);  // set user map to avoid loading
                                        // configuration from system database

    auto result = fn_save->CallAsFunction(
        context, fn_save, static_cast<int>(args.size()), args.data());
    ASSERT_FALSE(result.IsEmpty());
    ASSERT_TRUE(result.ToLocalChecked()->IsObject());
    auto* v8AnalyzerWeak = TRI_UnwrapClass<arangodb::iresearch::AnalyzerPool>(
        result.ToLocalChecked()->ToObject(TRI_IGETC).FromMaybe(
            v8::Local<v8::Object>()),
        WRP_IRESEARCH_ANALYZER_TYPE, TRI_IGETC);
    ASSERT_FALSE(!v8AnalyzerWeak);
    ASSERT_EQ(vocbase.name() + "::testAnalyzer4", v8AnalyzerWeak->name());
    ASSERT_EQ("identity", v8AnalyzerWeak->type());
    EXPECT_EQUAL_SLICES(VPackSlice::emptyObjectSlice(),
                        v8AnalyzerWeak->properties());
    ASSERT_EQ(v8AnalyzerWeak->features(), arangodb::iresearch::Features{});
    auto analyzer =
        analyzers.get(vocbase.name() + "::testAnalyzer4",
                      arangodb::QueryAnalyzerRevisions::QUERY_LATEST,
                      arangodb::transaction::Hints::TrxType::INTERNAL);
    EXPECT_NE(nullptr, analyzer);
  }
}

TEST_F(V8AnalyzerTest, test_manager_get) {
  auto& analyzers =
      server.getFeature<arangodb::iresearch::IResearchAnalyzerFeature>();
  auto& dbFeature = server.getFeature<arangodb::DatabaseFeature>();

  {
    auto const databases = arangodb::velocypack::Parser::fromJson(
        std::string("[ {\"name\" : \"testVocbase\"} ]"));
    ASSERT_EQ(TRI_ERROR_NO_ERROR, dbFeature.loadDatabases(databases->slice()));
  }
  arangodb::OperationOptions options(arangodb::ExecContext::current());
  {
    auto vocbase =
        dbFeature.useDatabase(arangodb::StaticStrings::SystemDatabase);
    std::shared_ptr<arangodb::LogicalCollection> ignored;
    arangodb::methods::Collections::createSystem(
        *vocbase, options, arangodb::tests::AnalyzerCollectionName, false,
        ignored);
  }
  {
    auto vocbase = dbFeature.useDatabase("testVocbase");
    std::shared_ptr<arangodb::LogicalCollection> ignored;
    arangodb::methods::Collections::createSystem(
        *vocbase, options, arangodb::tests::AnalyzerCollectionName, false,
        ignored);
  }
  arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;
  ASSERT_TRUE(
      (analyzers
           .emplace(result,
                    arangodb::StaticStrings::SystemDatabase + "::testAnalyzer1",
                    "identity", VPackSlice::noneSlice(),
                    arangodb::transaction::Hints::TrxType::INTERNAL)
           .ok()));
  ASSERT_TRUE((analyzers
                   .emplace(result, "testVocbase::testAnalyzer1", "identity",
                            VPackSlice::noneSlice(),
                            arangodb::transaction::Hints::TrxType::INTERNAL)
                   .ok()));
  struct ExecContext : public arangodb::ExecContext {
    ExecContext()
        : arangodb::ExecContext(arangodb::ExecContext::Type::Default, "", "",
                                arangodb::auth::Level::NONE,
                                arangodb::auth::Level::NONE, false) {}
  } execContext;
  arangodb::ExecContextScope execContextScope(&execContext);
  auto& authFeature = server.getFeature<arangodb::AuthenticationFeature>();
  auto* userManager = authFeature.userManager();

  TRI_vocbase_t vocbase(systemDBInfo(server.server()));
  v8::Isolate::CreateParams isolateParams;
  ArrayBufferAllocator arrayBufferAllocator;
  isolateParams.array_buffer_allocator = &arrayBufferAllocator;
  auto* isolate = v8::Isolate::New(isolateParams);
  ASSERT_NE(nullptr, isolate);
  irs::Finally cleanup = [isolate]() noexcept { isolate->Dispose(); };

  // otherwise v8::Isolate::Logger() will fail (called from
  // v8::Exception::Error)
  v8::Isolate::Scope isolateScope(isolate);

  // otherwise v8::Isolate::Logger() will fail (called from
  // v8::Exception::Error)
  v8::internal::Isolate::Current()->InitializeLoggingAndCounters();

  // required for v8::Context::New(...), v8::ObjectTemplate::New(...) and
  // TRI_AddMethodVocbase(...)
  v8::HandleScope handleScope(isolate);
  auto context = v8::Context::New(isolate);
  // required for TRI_AddMethodVocbase(...)
  v8::Context::Scope contextScope(context);

  // create and set inside 'isolate' for use with 'TRI_GET_GLOBALS()'
  std::unique_ptr<V8Global<arangodb::ArangodServer>> v8g(
      CreateV8Globals(server.server(), isolate, 0));
  v8g->ArangoErrorTempl.Reset(
      isolate,
      v8::ObjectTemplate::New(
          isolate));  // otherwise v8:-utils::CreateErrorObject(...) will
                      // fail
  v8g->_vocbase = &vocbase;
  arangodb::iresearch::TRI_InitV8Analyzers(*v8g, isolate);

  auto v8AnalyzerManager = getAnalyzerManagerInstance(v8g.get(), isolate);
  auto fn_analyzer =
      getAnalyzersMethodFunction(isolate, v8AnalyzerManager, "analyzer");

  // invalid params (no name)
  {
    std::vector<v8::Local<v8::Value>> args = {};

    arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
    auto& user = userMap
                     .emplace("", arangodb::auth::User::newUser(
                                      "", "", arangodb::auth::Source::LDAP))
                     .first->second;
    user.grantDatabase(
        vocbase.name(),
        arangodb::auth::Level::RO);  // for system collections
                                     // User::collectionAuthLevel(...) returns
                                     // database auth::Level
    userManager->setAuthInfo(userMap);  // set user map to avoid loading
                                        // configuration from system database

    arangodb::velocypack::Builder response;
    v8::TryCatch tryCatch(isolate);
    auto result = fn_analyzer->CallAsFunction(
        context, fn_analyzer, static_cast<int>(args.size()), args.data());
    ASSERT_TRUE(result.IsEmpty());
    ASSERT_TRUE(tryCatch.HasCaught());
    TRI_V8ToVPack(isolate, response, tryCatch.Exception(), false);
    auto slice = response.slice();
    ASSERT_TRUE(slice.isObject());
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::ErrorNum) &&
                 slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() &&
                 TRI_ERROR_BAD_PARAMETER ==
                     ErrorCode{slice.get(arangodb::StaticStrings::ErrorNum)
                                   .getNumber<int>()}));
  }

  // get static (known analyzer)
  {
    std::vector<v8::Local<v8::Value>> args = {
        TRI_V8_ASCII_STRING(isolate, "identity"),
    };

    arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
    auto& user = userMap
                     .emplace("", arangodb::auth::User::newUser(
                                      "", "", arangodb::auth::Source::LDAP))
                     .first->second;
    user.grantDatabase(
        vocbase.name(),
        arangodb::auth::Level::NONE);  // for system collections
                                       // User::collectionAuthLevel(...) returns
                                       // database auth::Level
    userManager->setAuthInfo(userMap);  // set user map to avoid loading
                                        // configuration from system database

    auto result = fn_analyzer->CallAsFunction(
        context, fn_analyzer, static_cast<int>(args.size()), args.data());
    ASSERT_FALSE(result.IsEmpty());
    ASSERT_TRUE(result.ToLocalChecked()->IsObject());
    auto* v8AnalyzerWeak = TRI_UnwrapClass<arangodb::iresearch::AnalyzerPool>(
        result.ToLocalChecked()->ToObject(TRI_IGETC).FromMaybe(
            v8::Local<v8::Object>()),
        WRP_IRESEARCH_ANALYZER_TYPE, TRI_IGETC);
    ASSERT_FALSE(!v8AnalyzerWeak);
    ASSERT_EQ(std::string("identity"), v8AnalyzerWeak->name());
    ASSERT_EQ(std::string("identity"), v8AnalyzerWeak->type());
    EXPECT_EQUAL_SLICES(VPackSlice::emptyObjectSlice(),
                        v8AnalyzerWeak->properties());
    size_t size = 0;
    v8AnalyzerWeak->features().visit([&size](std::string_view) { ++size; });
    EXPECT_EQ(2, size);
  }

  // get static (unknown analyzer)
  {
    std::vector<v8::Local<v8::Value>> args = {
        TRI_V8_ASCII_STRING(isolate, "unknown"),
    };

    arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
    auto& user = userMap
                     .emplace("", arangodb::auth::User::newUser(
                                      "", "", arangodb::auth::Source::LDAP))
                     .first->second;
    user.grantDatabase(
        vocbase.name(),
        arangodb::auth::Level::RO);  // for system collections
                                     // User::collectionAuthLevel(...) returns
                                     // database auth::Level
    userManager->setAuthInfo(userMap);  // set user map to avoid loading
                                        // configuration from system database

    auto result = fn_analyzer->CallAsFunction(
        context, fn_analyzer, static_cast<int>(args.size()), args.data());
    ASSERT_FALSE(result.IsEmpty());
    EXPECT_TRUE(result.FromMaybe(v8::Local<v8::Value>())->IsNull());
  }

  // get custom (known analyzer) authorized
  {
    std::vector<v8::Local<v8::Value>> args = {
        TRI_V8_STD_STRING(isolate, arangodb::StaticStrings::SystemDatabase +
                                       "::testAnalyzer1"),
    };

    arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
    auto& user = userMap
                     .emplace("", arangodb::auth::User::newUser(
                                      "", "", arangodb::auth::Source::LDAP))
                     .first->second;
    user.grantDatabase(
        vocbase.name(),
        arangodb::auth::Level::RO);  // for system collections
                                     // User::collectionAuthLevel(...) returns
                                     // database auth::Level
    userManager->setAuthInfo(userMap);  // set user map to avoid loading
                                        // configuration from system database

    auto result = fn_analyzer->CallAsFunction(
        context, fn_analyzer, static_cast<int>(args.size()), args.data());
    ASSERT_FALSE(result.IsEmpty());
    ASSERT_TRUE(result.ToLocalChecked()->IsObject());
    auto* v8AnalyzerWeak = TRI_UnwrapClass<arangodb::iresearch::AnalyzerPool>(
        result.ToLocalChecked()->ToObject(TRI_IGETC).FromMaybe(
            v8::Local<v8::Object>()),
        WRP_IRESEARCH_ANALYZER_TYPE, TRI_IGETC);
    ASSERT_FALSE(!v8AnalyzerWeak);
    ASSERT_TRUE((arangodb::StaticStrings::SystemDatabase + "::testAnalyzer1" ==
                 v8AnalyzerWeak->name()));
    ASSERT_EQ(std::string("identity"), v8AnalyzerWeak->type());
    EXPECT_EQUAL_SLICES(VPackSlice::emptyObjectSlice(),
                        v8AnalyzerWeak->properties());
    ASSERT_EQ(v8AnalyzerWeak->features(), arangodb::iresearch::Features{});
  }

  // get custom (known analyzer) authorized but wrong current db
  {
    std::vector<v8::Local<v8::Value>> args = {
        TRI_V8_ASCII_STRING(isolate, "testVocbase::testAnalyzer1"),
    };

    arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
    auto& user = userMap
                     .emplace("", arangodb::auth::User::newUser(
                                      "", "", arangodb::auth::Source::LDAP))
                     .first->second;
    user.grantDatabase(
        vocbase.name(),
        arangodb::auth::Level::RO);  // for system collections
                                     // User::collectionAuthLevel(...) returns
                                     // database auth::Level
    user.grantDatabase("testVocbase", arangodb::auth::Level::RO);
    userManager->setAuthInfo(userMap);  // set user map to avoid loading
                                        // configuration from system database
    v8::TryCatch tryCatch(isolate);
    arangodb::velocypack::Builder response;
    auto result = fn_analyzer->CallAsFunction(
        context, fn_analyzer, static_cast<int>(args.size()), args.data());
    ASSERT_TRUE(result.IsEmpty());
    ASSERT_TRUE(tryCatch.HasCaught());
    TRI_V8ToVPack(isolate, response, tryCatch.Exception(), false);
    auto slice = response.slice();
    ASSERT_TRUE(slice.isObject());
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::ErrorNum) &&
                 slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() &&
                 TRI_ERROR_FORBIDDEN ==
                     ErrorCode{slice.get(arangodb::StaticStrings::ErrorNum)
                                   .getNumber<int>()}));
  }
  // get custom (known analyzer) authorized from system with another current db
  {
    std::vector<v8::Local<v8::Value>> args = {
        TRI_V8_STD_STRING(isolate, arangodb::StaticStrings::SystemDatabase +
                                       "::testAnalyzer1"),
    };

    arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
    auto& user = userMap
                     .emplace("", arangodb::auth::User::newUser(
                                      "", "", arangodb::auth::Source::LDAP))
                     .first->second;
    user.grantDatabase(
        arangodb::StaticStrings::SystemDatabase,
        arangodb::auth::Level::RO);  // for system collections
                                     // User::collectionAuthLevel(...) returns
                                     // database auth::Level
    user.grantDatabase("testVocbase", arangodb::auth::Level::RO);
    userManager->setAuthInfo(userMap);  // set user map to avoid loading
                                        // configuration from system database

    auto result = fn_analyzer->CallAsFunction(
        context, fn_analyzer, static_cast<int>(args.size()), args.data());
    ASSERT_FALSE(result.IsEmpty());
    ASSERT_TRUE(result.ToLocalChecked()->IsObject());
    auto* v8AnalyzerWeak = TRI_UnwrapClass<arangodb::iresearch::AnalyzerPool>(
        result.ToLocalChecked()->ToObject(TRI_IGETC).FromMaybe(
            v8::Local<v8::Object>()),
        WRP_IRESEARCH_ANALYZER_TYPE, TRI_IGETC);
    ASSERT_FALSE(!v8AnalyzerWeak);
    ASSERT_TRUE((arangodb::StaticStrings::SystemDatabase + "::testAnalyzer1" ==
                 v8AnalyzerWeak->name()));
    ASSERT_EQ(std::string("identity"), v8AnalyzerWeak->type());
    EXPECT_EQUAL_SLICES(VPackSlice::emptyObjectSlice(),
                        v8AnalyzerWeak->properties());
    ASSERT_EQ(v8AnalyzerWeak->features(), arangodb::iresearch::Features{});
  }

  // get custom (known analyzer) not authorized
  {
    std::vector<v8::Local<v8::Value>> args = {
        TRI_V8_STD_STRING(isolate, arangodb::StaticStrings::SystemDatabase +
                                       "::testAnalyzer1"),
    };

    arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
    auto& user = userMap
                     .emplace("", arangodb::auth::User::newUser(
                                      "", "", arangodb::auth::Source::LDAP))
                     .first->second;
    user.grantDatabase(
        vocbase.name(),
        arangodb::auth::Level::NONE);  // for system collections
                                       // User::collectionAuthLevel(...) returns
                                       // database auth::Level
    userManager->setAuthInfo(userMap);  // set user map to avoid loading
                                        // configuration from system database

    arangodb::velocypack::Builder response;
    v8::TryCatch tryCatch(isolate);
    auto result = fn_analyzer->CallAsFunction(
        context, fn_analyzer, static_cast<int>(args.size()), args.data());
    ASSERT_TRUE(result.IsEmpty());
    ASSERT_TRUE(tryCatch.HasCaught());
    TRI_V8ToVPack(isolate, response, tryCatch.Exception(), false);
    auto slice = response.slice();
    ASSERT_TRUE(slice.isObject());
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::ErrorNum) &&
                 slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() &&
                 TRI_ERROR_FORBIDDEN ==
                     ErrorCode{slice.get(arangodb::StaticStrings::ErrorNum)
                                   .getNumber<int>()}));
  }

  // get custom (unknown analyzer) authorized
  {
    std::vector<v8::Local<v8::Value>> args = {
        TRI_V8_STD_STRING(
            isolate, arangodb::StaticStrings::SystemDatabase + "::unknown"),
    };

    arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
    auto& user = userMap
                     .emplace("", arangodb::auth::User::newUser(
                                      "", "", arangodb::auth::Source::LDAP))
                     .first->second;
    user.grantDatabase(
        vocbase.name(),
        arangodb::auth::Level::RO);  // for system collections
                                     // User::collectionAuthLevel(...) returns
                                     // database auth::Level
    userManager->setAuthInfo(userMap);  // set user map to avoid loading
                                        // configuration from system database

    auto result = fn_analyzer->CallAsFunction(
        context, fn_analyzer, static_cast<int>(args.size()), args.data());
    ASSERT_FALSE(result.IsEmpty());
    EXPECT_TRUE(result.FromMaybe(v8::Local<v8::Value>())->IsNull());
  }

  // get custom (unknown analyzer) not authorized
  {
    std::vector<v8::Local<v8::Value>> args = {
        TRI_V8_STD_STRING(
            isolate, arangodb::StaticStrings::SystemDatabase + "::unknown"),
    };

    arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
    auto& user = userMap
                     .emplace("", arangodb::auth::User::newUser(
                                      "", "", arangodb::auth::Source::LDAP))
                     .first->second;
    user.grantDatabase(
        vocbase.name(),
        arangodb::auth::Level::NONE);  // for system collections
                                       // User::collectionAuthLevel(...) returns
                                       // database auth::Level
    userManager->setAuthInfo(userMap);  // set user map to avoid loading
                                        // configuration from system database

    arangodb::velocypack::Builder response;
    v8::TryCatch tryCatch(isolate);
    auto result = fn_analyzer->CallAsFunction(
        context, fn_analyzer, static_cast<int>(args.size()), args.data());
    ASSERT_TRUE(result.IsEmpty());
    ASSERT_TRUE(tryCatch.HasCaught());
    TRI_V8ToVPack(isolate, response, tryCatch.Exception(), false);
    auto slice = response.slice();
    ASSERT_TRUE(slice.isObject());
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::ErrorNum) &&
                 slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() &&
                 TRI_ERROR_FORBIDDEN ==
                     ErrorCode{slice.get(arangodb::StaticStrings::ErrorNum)
                                   .getNumber<int>()}));
  }

  // get custom (unknown analyzer, unknown vocbase) authorized
  {
    std::vector<v8::Local<v8::Value>> args = {
        TRI_V8_ASCII_STRING(isolate, "unknownVocbase::unknown"),
    };

    arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
    v8::TryCatch tryCatch(isolate);
    arangodb::velocypack::Builder response;
    auto& user = userMap
                     .emplace("", arangodb::auth::User::newUser(
                                      "", "", arangodb::auth::Source::LDAP))
                     .first->second;
    user.grantDatabase(
        "unknownVocbase",
        arangodb::auth::Level::RO);  // for system collections
                                     // User::collectionAuthLevel(...) returns
                                     // database auth::Level
    userManager->setAuthInfo(userMap);  // set user map to avoid loading
                                        // configuration from system database

    auto result = fn_analyzer->CallAsFunction(
        context, fn_analyzer, static_cast<int>(args.size()), args.data());
    ASSERT_TRUE(tryCatch.HasCaught());
    TRI_V8ToVPack(isolate, response, tryCatch.Exception(), false);
    auto slice = response.slice();
    ASSERT_TRUE(slice.isObject());
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::ErrorNum) &&
                 slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() &&
                 TRI_ERROR_FORBIDDEN ==
                     ErrorCode{slice.get(arangodb::StaticStrings::ErrorNum)
                                   .getNumber<int>()}));
    ASSERT_TRUE(result.IsEmpty());
  }

  // get custom (unknown analyzer, unknown vocbase) not authorized
  {
    std::vector<v8::Local<v8::Value>> args = {
        TRI_V8_ASCII_STRING(isolate, "unknownVocbase::unknown"),
    };

    arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
    auto& user = userMap
                     .emplace("", arangodb::auth::User::newUser(
                                      "", "", arangodb::auth::Source::LDAP))
                     .first->second;
    user.grantDatabase(
        vocbase.name(),
        arangodb::auth::Level::NONE);  // for system collections
                                       // User::collectionAuthLevel(...) returns
                                       // database auth::Level
    userManager->setAuthInfo(userMap);  // set user map to avoid loading
                                        // configuration from system database

    arangodb::velocypack::Builder response;
    v8::TryCatch tryCatch(isolate);
    auto result = fn_analyzer->CallAsFunction(
        context, fn_analyzer, static_cast<int>(args.size()), args.data());
    ASSERT_TRUE(result.IsEmpty());
    ASSERT_TRUE(tryCatch.HasCaught());
    TRI_V8ToVPack(isolate, response, tryCatch.Exception(), false);
    auto slice = response.slice();
    ASSERT_TRUE(slice.isObject());
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::ErrorNum) &&
                 slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() &&
                 TRI_ERROR_FORBIDDEN ==
                     ErrorCode{slice.get(arangodb::StaticStrings::ErrorNum)
                                   .getNumber<int>()}));
  }
}

TEST_F(V8AnalyzerTest, test_manager_list) {
  auto& analyzers =
      server.getFeature<arangodb::iresearch::IResearchAnalyzerFeature>();
  auto& dbFeature = server.getFeature<arangodb::DatabaseFeature>();

  arangodb::OperationOptions options(arangodb::ExecContext::current());
  {
    auto vocbase =
        dbFeature.useDatabase(arangodb::StaticStrings::SystemDatabase);
    std::shared_ptr<arangodb::LogicalCollection> ignored;
    arangodb::methods::Collections::createSystem(
        *vocbase, options, arangodb::tests::AnalyzerCollectionName, false,
        ignored);
  }
  {
    TRI_vocbase_t* vocbase;
    arangodb::Result res =
        dbFeature.createDatabase(testDBInfo(server.server()), vocbase);
    ASSERT_TRUE(res.ok());
    std::shared_ptr<arangodb::LogicalCollection> ignored;
    arangodb::methods::Collections::createSystem(
        *vocbase, options, arangodb::tests::AnalyzerCollectionName, false,
        ignored);
  }

  arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;
  auto res = analyzers.emplace(
      result, arangodb::StaticStrings::SystemDatabase + "::testAnalyzer1",
      "identity", VPackSlice::noneSlice(),
      arangodb::transaction::Hints::TrxType::INTERNAL);
  ASSERT_TRUE(res.ok());
  res = analyzers.emplace(result, "testVocbase::testAnalyzer2", "identity",
                          VPackSlice::noneSlice(),
                          arangodb::transaction::Hints::TrxType::INTERNAL);
  ASSERT_TRUE(res.ok());

  struct ExecContext : public arangodb::ExecContext {
    ExecContext()
        : arangodb::ExecContext(arangodb::ExecContext::Type::Default, "", "",
                                arangodb::auth::Level::NONE,
                                arangodb::auth::Level::NONE, false) {}
  } execContext;
  arangodb::ExecContextScope execContextScope(&execContext);
  auto& authFeature = server.getFeature<arangodb::AuthenticationFeature>();
  auto* userManager = authFeature.userManager();

  TRI_vocbase_t systemDBVocbase(systemDBInfo(server.server()));
  TRI_vocbase_t testDBVocbase(testDBInfo(server.server()));
  v8::Isolate::CreateParams isolateParams;
  ArrayBufferAllocator arrayBufferAllocator;
  isolateParams.array_buffer_allocator = &arrayBufferAllocator;
  auto* isolate = v8::Isolate::New(isolateParams);
  ASSERT_NE(nullptr, isolate);
  irs::Finally cleanup = [isolate]() noexcept { isolate->Dispose(); };
  // otherwise v8::Isolate::Logger() will fail (called from
  // v8::Exception::Error)
  v8::Isolate::Scope isolateScope(isolate);
  // otherwise v8::Isolate::Logger() will fail (called from
  // v8::Exception::Error)
  v8::internal::Isolate::Current()->InitializeLoggingAndCounters();
  // required for v8::Context::New(...), v8::ObjectTemplate::New(...) and
  // TRI_AddMethodVocbase(...)
  v8::HandleScope handleScope(isolate);
  auto context = v8::Context::New(isolate);
  // required for TRI_AddMethodVocbase(...)
  v8::Context::Scope contextScope(context);
  // create and set inside 'isolate' for use with 'TRI_GET_GLOBALS()'
  std::unique_ptr<V8Global<arangodb::ArangodServer>> v8g(
      CreateV8Globals(server.server(), isolate, 0));
  // otherwise v8:-utils::CreateErrorObject(...) will fail
  v8g->ArangoErrorTempl.Reset(isolate, v8::ObjectTemplate::New(isolate));
  arangodb::iresearch::TRI_InitV8Analyzers(*v8g, isolate);

  auto v8AnalyzerManager = getAnalyzerManagerInstance(v8g.get(), isolate);
  auto fn_toArray =
      getAnalyzersMethodFunction(isolate, v8AnalyzerManager, "toArray");
  // system database (authorised)
  {
    v8g->_vocbase = &systemDBVocbase;
    std::vector<v8::Local<v8::Value>> args = {};

    arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
    auto& user = userMap
                     .emplace("", arangodb::auth::User::newUser(
                                      "", "", arangodb::auth::Source::LDAP))
                     .first->second;
    user.grantDatabase(
        systemDBVocbase.name(),
        arangodb::auth::Level::RO);  // for system collections
                                     // User::collectionAuthLevel(...) returns
                                     // database auth::Level
    userManager->setAuthInfo(userMap);  // set user map to avoid loading
                                        // configuration from system database

    std::set<std::string> expected = {
        "identity", "text_de",
        "text_en",  "text_es",
        "text_fi",  "text_fr",
        "text_it",  "text_nl",
        "text_no",  "text_pt",
        "text_ru",  "text_sv",
        "text_zh",  arangodb::StaticStrings::SystemDatabase + "::testAnalyzer1",
    };
    auto result = fn_toArray->CallAsFunction(
        context, fn_toArray, static_cast<int>(args.size()), args.data());
    ASSERT_FALSE(result.IsEmpty());
    ASSERT_TRUE(result.ToLocalChecked()->IsArray());
    auto v8Result = v8::Handle<v8::Array>::Cast(result.ToLocalChecked());

    for (uint32_t i = 0, count = v8Result->Length(); i < count; ++i) {
      auto v8Analyzer =
          v8Result->Get(context, i).FromMaybe(v8::Local<v8::Value>());
      ASSERT_FALSE(v8Analyzer.IsEmpty());
      ASSERT_TRUE(v8Analyzer->IsObject());
      auto* v8AnalyzerWeak = TRI_UnwrapClass<arangodb::iresearch::AnalyzerPool>(
          v8Analyzer->ToObject(TRI_IGETC).FromMaybe(v8::Local<v8::Object>()),
          WRP_IRESEARCH_ANALYZER_TYPE, TRI_IGETC);
      ASSERT_FALSE(!v8AnalyzerWeak);
      ASSERT_EQ(1, expected.erase(v8AnalyzerWeak->name()));
    }

    EXPECT_TRUE(expected.empty());
  }

  // system database (not authorised)
  {
    v8g->_vocbase = &systemDBVocbase;
    std::vector<v8::Local<v8::Value>> args = {};

    arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
    auto& user = userMap
                     .emplace("", arangodb::auth::User::newUser(
                                      "", "", arangodb::auth::Source::LDAP))
                     .first->second;
    user.grantDatabase(
        systemDBVocbase.name(),
        arangodb::auth::Level::NONE);  // for system collections
                                       // User::collectionAuthLevel(...) returns
                                       // database auth::Level
    userManager->setAuthInfo(userMap);  // set user map to avoid loading
                                        // configuration from system database

    std::set<std::string> expected = {
        "identity", "text_de", "text_en", "text_es", "text_fi",
        "text_fr",  "text_it", "text_nl", "text_no", "text_pt",
        "text_ru",  "text_sv", "text_zh",
    };

    auto result = fn_toArray->CallAsFunction(
        context, fn_toArray, static_cast<int>(args.size()), args.data());
    ASSERT_FALSE(result.IsEmpty());
    ASSERT_TRUE(result.ToLocalChecked()->IsArray());
    auto v8Result = v8::Handle<v8::Array>::Cast(result.ToLocalChecked());
    for (uint32_t i = 0, count = v8Result->Length(); i < count; ++i) {
      auto v8Analyzer =
          v8Result->Get(context, i).FromMaybe(v8::Local<v8::Value>());
      ASSERT_FALSE(v8Analyzer.IsEmpty());
      ASSERT_TRUE(v8Analyzer->IsObject());
      auto* v8AnalyzerWeak = TRI_UnwrapClass<arangodb::iresearch::AnalyzerPool>(
          v8Analyzer->ToObject(TRI_IGETC).FromMaybe(v8::Local<v8::Object>()),
          WRP_IRESEARCH_ANALYZER_TYPE, TRI_IGETC);
      ASSERT_FALSE(!v8AnalyzerWeak);
      ASSERT_EQ(1, expected.erase(v8AnalyzerWeak->name()));
    }

    EXPECT_TRUE(expected.empty());
  }

  // non-system database (authorised, system authorised)
  {
    v8g->_vocbase = &testDBVocbase;
    std::vector<v8::Local<v8::Value>> args = {};
    arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
    auto& user = userMap
                     .emplace("", arangodb::auth::User::newUser(
                                      "", "", arangodb::auth::Source::LDAP))
                     .first->second;
    user.grantDatabase(
        arangodb::StaticStrings::SystemDatabase,
        arangodb::auth::Level::RO);  // for system collections
                                     // User::collectionAuthLevel(...) returns
                                     // database auth::Level
    user.grantDatabase(
        testDBVocbase.name(),
        arangodb::auth::Level::RO);  // for system collections
                                     // User::collectionAuthLevel(...) returns
                                     // database auth::Level
    userManager->setAuthInfo(userMap);  // set user map to avoid loading
                                        // configuration from system database

    std::set<std::string> expected = {
        "identity",
        "text_de",
        "text_en",
        "text_es",
        "text_fi",
        "text_fr",
        "text_it",
        "text_nl",
        "text_no",
        "text_pt",
        "text_ru",
        "text_sv",
        "text_zh",
        arangodb::StaticStrings::SystemDatabase + "::testAnalyzer1",
        "testVocbase::testAnalyzer2",
    };
    auto result = fn_toArray->CallAsFunction(
        context, fn_toArray, static_cast<int>(args.size()), args.data());
    ASSERT_FALSE(result.IsEmpty());
    ASSERT_TRUE(result.ToLocalChecked()->IsArray());
    auto v8Result = v8::Handle<v8::Array>::Cast(result.ToLocalChecked());
    for (uint32_t i = 0, count = v8Result->Length(); i < count; ++i) {
      auto v8Analyzer =
          v8Result->Get(context, i).FromMaybe(v8::Local<v8::Value>());
      EXPECT_FALSE(v8Analyzer.IsEmpty());
      EXPECT_TRUE(v8Analyzer->IsObject());
      auto* v8AnalyzerWeak = TRI_UnwrapClass<arangodb::iresearch::AnalyzerPool>(
          v8Analyzer->ToObject(TRI_IGETC).FromMaybe(v8::Local<v8::Object>()),
          WRP_IRESEARCH_ANALYZER_TYPE, TRI_IGETC);
      EXPECT_FALSE(!v8AnalyzerWeak);
      EXPECT_EQ(1, expected.erase(v8AnalyzerWeak->name()));
    }

    EXPECT_TRUE(expected.empty());
  }

  // non-system database (not authorised, system authorised)
  {
    v8g->_vocbase = &testDBVocbase;

    std::vector<v8::Local<v8::Value>> args = {};

    arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
    auto& user = userMap
                     .emplace("", arangodb::auth::User::newUser(
                                      "", "", arangodb::auth::Source::LDAP))
                     .first->second;
    user.grantDatabase(
        arangodb::StaticStrings::SystemDatabase,
        arangodb::auth::Level::RO);  // for system collections
                                     // User::collectionAuthLevel(...) returns
                                     // database auth::Level
    user.grantDatabase(
        testDBVocbase.name(),
        arangodb::auth::Level::NONE);  // for system collections
                                       // User::collectionAuthLevel(...) returns
                                       // database auth::Level
    userManager->setAuthInfo(userMap);  // set user map to avoid loading
                                        // configuration from system database

    std::set<std::string> expected = {
        "identity", "text_de",
        "text_en",  "text_es",
        "text_fi",  "text_fr",
        "text_it",  "text_nl",
        "text_no",  "text_pt",
        "text_ru",  "text_sv",
        "text_zh",  arangodb::StaticStrings::SystemDatabase + "::testAnalyzer1",
    };
    auto result = fn_toArray->CallAsFunction(
        context, fn_toArray, static_cast<int>(args.size()), args.data());
    ASSERT_FALSE(result.IsEmpty());
    ASSERT_TRUE(result.ToLocalChecked()->IsArray());
    auto v8Result = v8::Handle<v8::Array>::Cast(result.ToLocalChecked());
    for (uint32_t i = 0, count = v8Result->Length(); i < count; ++i) {
      auto v8Analyzer =
          v8Result->Get(context, i).FromMaybe(v8::Local<v8::Value>());
      ASSERT_FALSE(v8Analyzer.IsEmpty());
      ASSERT_TRUE(v8Analyzer->IsObject());
      auto* v8AnalyzerWeak = TRI_UnwrapClass<arangodb::iresearch::AnalyzerPool>(
          v8Analyzer->ToObject(TRI_IGETC).FromMaybe(v8::Local<v8::Object>()),
          WRP_IRESEARCH_ANALYZER_TYPE, TRI_IGETC);
      EXPECT_FALSE(!v8AnalyzerWeak);
      EXPECT_EQ(1, expected.erase(v8AnalyzerWeak->name()));
    }

    EXPECT_TRUE(expected.empty());
  }

  // non-system database (authorised, system not authorised)
  {
    v8g->_vocbase = &testDBVocbase;

    std::vector<v8::Local<v8::Value>> args = {};

    arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
    auto& user = userMap
                     .emplace("", arangodb::auth::User::newUser(
                                      "", "", arangodb::auth::Source::LDAP))
                     .first->second;
    user.grantDatabase(
        arangodb::StaticStrings::SystemDatabase,
        arangodb::auth::Level::NONE);  // for system collections
                                       // User::collectionAuthLevel(...) returns
                                       // database auth::Level
    user.grantDatabase(
        testDBVocbase.name(),
        arangodb::auth::Level::RO);  // for system collections
                                     // User::collectionAuthLevel(...) returns
                                     // database auth::Level
    userManager->setAuthInfo(userMap);  // set user map to avoid loading
                                        // configuration from system database

    std::set<std::string> expected = {
        "identity", "text_de",
        "text_en",  "text_es",
        "text_fi",  "text_fr",
        "text_it",  "text_nl",
        "text_no",  "text_pt",
        "text_ru",  "text_sv",
        "text_zh",  "testVocbase::testAnalyzer2",
    };
    auto result = fn_toArray->CallAsFunction(
        context, fn_toArray, static_cast<int>(args.size()), args.data());
    ASSERT_FALSE(result.IsEmpty());
    ASSERT_TRUE(result.ToLocalChecked()->IsArray());
    auto v8Result = v8::Handle<v8::Array>::Cast(result.ToLocalChecked());
    for (uint32_t i = 0, count = v8Result->Length(); i < count; ++i) {
      auto v8Analyzer =
          v8Result->Get(context, i).FromMaybe(v8::Local<v8::Value>());
      ASSERT_FALSE(v8Analyzer.IsEmpty());
      ASSERT_TRUE(v8Analyzer->IsObject());
      auto* v8AnalyzerWeak = TRI_UnwrapClass<arangodb::iresearch::AnalyzerPool>(
          v8Analyzer->ToObject(TRI_IGETC).FromMaybe(v8::Local<v8::Object>()),
          WRP_IRESEARCH_ANALYZER_TYPE, TRI_IGETC);
      ASSERT_FALSE(!v8AnalyzerWeak);
      ASSERT_EQ(1, expected.erase(v8AnalyzerWeak->name()));
    }

    EXPECT_TRUE(expected.empty());
  }

  // non-system database (not authorised, system not authorised)
  {
    v8g->_vocbase = &testDBVocbase;

    std::vector<v8::Local<v8::Value>> args = {};

    arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
    auto& user = userMap
                     .emplace("", arangodb::auth::User::newUser(
                                      "", "", arangodb::auth::Source::LDAP))
                     .first->second;
    user.grantDatabase(
        arangodb::StaticStrings::SystemDatabase,
        arangodb::auth::Level::NONE);  // for system collections
                                       // User::collectionAuthLevel(...) returns
                                       // database auth::Level
    user.grantDatabase(
        testDBVocbase.name(),
        arangodb::auth::Level::NONE);  // for system collections
                                       // User::collectionAuthLevel(...) returns
                                       // database auth::Level
    userManager->setAuthInfo(userMap);  // set user map to avoid loading
                                        // configuration from system database

    std::set<std::string> expected = {
        "identity", "text_de", "text_en", "text_es", "text_fi",
        "text_fr",  "text_it", "text_nl", "text_no", "text_pt",
        "text_ru",  "text_sv", "text_zh",
    };
    auto result = fn_toArray->CallAsFunction(
        context, fn_toArray, static_cast<int>(args.size()), args.data());
    ASSERT_FALSE(result.IsEmpty());
    ASSERT_TRUE(result.ToLocalChecked()->IsArray());
    auto v8Result = v8::Handle<v8::Array>::Cast(result.ToLocalChecked());
    for (uint32_t i = 0, count = v8Result->Length(); i < count; ++i) {
      auto v8Analyzer =
          v8Result->Get(context, i).FromMaybe(v8::Local<v8::Value>());
      ASSERT_FALSE(v8Analyzer.IsEmpty());
      ASSERT_TRUE(v8Analyzer->IsObject());
      auto* v8AnalyzerWeak = TRI_UnwrapClass<arangodb::iresearch::AnalyzerPool>(
          v8Analyzer->ToObject(TRI_IGETC).FromMaybe(v8::Local<v8::Object>()),
          WRP_IRESEARCH_ANALYZER_TYPE, TRI_IGETC);
      ASSERT_FALSE(!v8AnalyzerWeak);
      ASSERT_EQ(1, expected.erase(v8AnalyzerWeak->name()));
    }

    EXPECT_TRUE(expected.empty());
  }
}

TEST_F(V8AnalyzerTest, test_manager_remove) {
  auto& analyzers =
      server.getFeature<arangodb::iresearch::IResearchAnalyzerFeature>();
  auto& dbFeature = server.getFeature<arangodb::DatabaseFeature>();

  arangodb::OperationOptions options(arangodb::ExecContext::current());
  {
    auto vocbase =
        dbFeature.useDatabase(arangodb::StaticStrings::SystemDatabase);
    std::shared_ptr<arangodb::LogicalCollection> ignored;
    arangodb::methods::Collections::createSystem(
        *vocbase, options, arangodb::tests::AnalyzerCollectionName, false,
        ignored);
  }
  {
    TRI_vocbase_t* vocbase;
    arangodb::Result res =
        dbFeature.createDatabase(testDBInfo(server.server()), vocbase);
    ASSERT_TRUE(res.ok());
    std::shared_ptr<arangodb::LogicalCollection> ignored;
    arangodb::methods::Collections::createSystem(
        *vocbase, options, arangodb::tests::AnalyzerCollectionName, false,
        ignored);
  }
  {
    arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;
    ASSERT_TRUE((analyzers
                     .emplace(result,
                              arangodb::StaticStrings::SystemDatabase +
                                  "::testAnalyzer1",
                              "identity", VPackSlice::noneSlice(),
                              arangodb::transaction::Hints::TrxType::INTERNAL)
                     .ok()));
    ASSERT_TRUE((analyzers
                     .emplace(result,
                              arangodb::StaticStrings::SystemDatabase +
                                  "::testAnalyzer2",
                              "identity", VPackSlice::noneSlice(),
                              arangodb::transaction::Hints::TrxType::INTERNAL)
                     .ok()));
    ASSERT_TRUE((analyzers
                     .emplace(result,
                              arangodb::StaticStrings::SystemDatabase +
                                  "::testAnalyzer3",
                              "identity", VPackSlice::noneSlice(),
                              arangodb::transaction::Hints::TrxType::INTERNAL)
                     .ok()));
    ASSERT_TRUE((analyzers
                     .emplace(result, "testVocbase::testAnalyzer1", "identity",
                              VPackSlice::noneSlice(),
                              arangodb::transaction::Hints::TrxType::INTERNAL)
                     .ok()));
    ASSERT_TRUE((analyzers
                     .emplace(result, "testVocbase::testAnalyzer2", "identity",
                              VPackSlice::noneSlice(),
                              arangodb::transaction::Hints::TrxType::INTERNAL)
                     .ok()));
    ASSERT_TRUE((analyzers
                     .emplace(result, "testVocbase::testAnalyzer3", "identity",
                              VPackSlice::noneSlice(),
                              arangodb::transaction::Hints::TrxType::INTERNAL)
                     .ok()));
  }
  struct ExecContext : public arangodb::ExecContext {
    ExecContext()
        : arangodb::ExecContext(arangodb::ExecContext::Type::Default, "", "",
                                arangodb::auth::Level::NONE,
                                arangodb::auth::Level::NONE, false) {}
  } execContext;
  arangodb::ExecContextScope execContextScope(&execContext);
  auto& authFeature = server.getFeature<arangodb::AuthenticationFeature>();
  auto* userManager = authFeature.userManager();

  TRI_vocbase_t systemDBVocbase(systemDBInfo(server.server()));
  TRI_vocbase_t testDBVocbase(testDBInfo(server.server()));
  v8::Isolate::CreateParams isolateParams;
  ArrayBufferAllocator arrayBufferAllocator;
  isolateParams.array_buffer_allocator = &arrayBufferAllocator;
  auto* isolate = v8::Isolate::New(isolateParams);
  ASSERT_NE(nullptr, isolate);
  irs::Finally cleanup = [isolate]() noexcept { isolate->Dispose(); };
  // otherwise v8::Isolate::Logger() will fail (called from
  // v8::Exception::Error)
  v8::Isolate::Scope isolateScope(isolate);
  // otherwise v8::Isolate::Logger() will fail (called from
  // v8::Exception::Error)
  v8::internal::Isolate::Current()->InitializeLoggingAndCounters();
  // required for v8::Context::New(...), v8::ObjectTemplate::New(...) and
  // TRI_AddMethodVocbase(...)
  v8::HandleScope handleScope(isolate);
  auto context = v8::Context::New(isolate);
  // required for TRI_AddMethodVocbase(...)
  v8::Context::Scope contextScope(context);
  // create and set inside 'isolate' for use with 'TRI_GET_GLOBALS()'
  std::unique_ptr<V8Global<arangodb::ArangodServer>> v8g(
      CreateV8Globals(server.server(), isolate, 0));
  // otherwise v8:-utils::CreateErrorObject(...) will fail
  v8g->ArangoErrorTempl.Reset(isolate, v8::ObjectTemplate::New(isolate));
  arangodb::iresearch::TRI_InitV8Analyzers(*v8g, isolate);

  auto v8AnalyzerManager = getAnalyzerManagerInstance(v8g.get(), isolate);
  auto fn_remove =
      getAnalyzersMethodFunction(isolate, v8AnalyzerManager, "remove");

  // invalid params (no name)
  {
    v8g->_vocbase = &systemDBVocbase;
    std::vector<v8::Local<v8::Value>> args = {};

    arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
    auto& user = userMap
                     .emplace("", arangodb::auth::User::newUser(
                                      "", "", arangodb::auth::Source::LDAP))
                     .first->second;

    // for system collections User::collectionAuthLevel(...) returns database
    // auth::Level
    user.grantDatabase(systemDBVocbase.name(), arangodb::auth::Level::RW);

    // set user map to avoid loading configuration from system database
    userManager->setAuthInfo(userMap);

    arangodb::velocypack::Builder response;
    v8::TryCatch tryCatch(isolate);
    auto result =
        v8::Function::Cast(*fn_remove)
            ->CallAsFunction(context, fn_remove, static_cast<int>(args.size()),
                             args.data());
    ASSERT_TRUE(result.IsEmpty());
    ASSERT_TRUE(tryCatch.HasCaught());
    TRI_V8ToVPack(isolate, response, tryCatch.Exception(), false);
    auto slice = response.slice();
    ASSERT_TRUE(slice.isObject());
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::ErrorNum) &&
                 slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() &&
                 TRI_ERROR_BAD_PARAMETER ==
                     ErrorCode{slice.get(arangodb::StaticStrings::ErrorNum)
                                   .getNumber<int>()}));
  }

  // unknown analyzer
  {
    v8g->_vocbase = &systemDBVocbase;
    std::vector<v8::Local<v8::Value>> args = {
        TRI_V8_ASCII_STRING(isolate, "unknown"),
    };

    arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
    auto& user = userMap
                     .emplace("", arangodb::auth::User::newUser(
                                      "", "", arangodb::auth::Source::LDAP))
                     .first->second;
    user.grantDatabase(
        systemDBVocbase.name(),
        arangodb::auth::Level::RW);  // for system collections
                                     // User::collectionAuthLevel(...) returns
                                     // database auth::Level
    userManager->setAuthInfo(userMap);  // set user map to avoid loading
                                        // configuration from system database

    arangodb::velocypack::Builder response;
    v8::TryCatch tryCatch(isolate);
    auto result =
        v8::Function::Cast(*fn_remove)
            ->CallAsFunction(context, fn_remove, static_cast<int>(args.size()),
                             args.data());
    ASSERT_TRUE(result.IsEmpty());
    ASSERT_TRUE(tryCatch.HasCaught());
    TRI_V8ToVPack(isolate, response, tryCatch.Exception(), false);
    auto slice = response.slice();
    ASSERT_TRUE(slice.isObject());
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::ErrorNum) &&
                 slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() &&
                 TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND ==
                     ErrorCode{slice.get(arangodb::StaticStrings::ErrorNum)
                                   .getNumber<int>()}));
  }

  // not authorised
  {
    v8g->_vocbase = &systemDBVocbase;
    std::vector<v8::Local<v8::Value>> args = {
        TRI_V8_ASCII_STRING(isolate, "testAnalyzer1"),
    };

    arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
    auto& user = userMap
                     .emplace("", arangodb::auth::User::newUser(
                                      "", "", arangodb::auth::Source::LDAP))
                     .first->second;
    user.grantDatabase(
        systemDBVocbase.name(),
        arangodb::auth::Level::RO);  // for system collections
                                     // User::collectionAuthLevel(...) returns
                                     // database auth::Level
    userManager->setAuthInfo(userMap);  // set user map to avoid loading
                                        // configuration from system database

    arangodb::velocypack::Builder response;
    v8::TryCatch tryCatch(isolate);
    auto result =
        v8::Function::Cast(*fn_remove)
            ->CallAsFunction(context, fn_remove, static_cast<int>(args.size()),
                             args.data());
    ASSERT_TRUE(result.IsEmpty());
    ASSERT_TRUE(tryCatch.HasCaught());
    TRI_V8ToVPack(isolate, response, tryCatch.Exception(), false);
    auto slice = response.slice();
    ASSERT_TRUE(slice.isObject());
    ASSERT_TRUE((slice.hasKey(arangodb::StaticStrings::ErrorNum) &&
                 slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() &&
                 TRI_ERROR_FORBIDDEN ==
                     ErrorCode{slice.get(arangodb::StaticStrings::ErrorNum)
                                   .getNumber<int>()}));
    auto analyzer = analyzers.get(
        arangodb::StaticStrings::SystemDatabase + "::testAnalyzer1",
        arangodb::QueryAnalyzerRevisions::QUERY_LATEST,
        arangodb::transaction::Hints::TrxType::INTERNAL);
    EXPECT_FALSE(!analyzer);
  }

  // still in use (fail)
  {
    v8g->_vocbase = &systemDBVocbase;
    std::vector<v8::Local<v8::Value>> args = {
        TRI_V8_ASCII_STRING(isolate, "testAnalyzer2"),
        v8::False(isolate),
    };
    auto inUseAnalyzer = analyzers.get(
        arangodb::StaticStrings::SystemDatabase + "::testAnalyzer2",
        arangodb::QueryAnalyzerRevisions::QUERY_LATEST,
        arangodb::transaction::Hints::TrxType::INTERNAL);  // hold ref to mark
                                                           // in-use
    ASSERT_FALSE(!inUseAnalyzer);

    arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
    auto& user = userMap
                     .emplace("", arangodb::auth::User::newUser(
                                      "", "", arangodb::auth::Source::LDAP))
                     .first->second;
    user.grantDatabase(
        systemDBVocbase.name(),
        arangodb::auth::Level::RW);  // for system collections
                                     // User::collectionAuthLevel(...) returns
                                     // database auth::Level
    userManager->setAuthInfo(userMap);  // set user map to avoid loading
                                        // configuration from system database

    arangodb::velocypack::Builder response;
    v8::TryCatch tryCatch(isolate);
    auto result =
        v8::Function::Cast(*fn_remove)
            ->CallAsFunction(context, fn_remove, static_cast<int>(args.size()),
                             args.data());
    ASSERT_TRUE(result.IsEmpty());
    ASSERT_TRUE(tryCatch.HasCaught());
    TRI_V8ToVPack(isolate, response, tryCatch.Exception(), false);
    auto slice = response.slice();
    ASSERT_TRUE(slice.isObject());
    ASSERT_TRUE((slice.hasKey(arangodb::StaticStrings::ErrorNum) &&
                 slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() &&
                 TRI_ERROR_ARANGO_CONFLICT ==
                     ErrorCode{slice.get(arangodb::StaticStrings::ErrorNum)
                                   .getNumber<int>()}));
    auto analyzer = analyzers.get(
        arangodb::StaticStrings::SystemDatabase + "::testAnalyzer2",
        arangodb::QueryAnalyzerRevisions::QUERY_LATEST,
        arangodb::transaction::Hints::TrxType::INTERNAL);
    EXPECT_FALSE(!analyzer);
  }

  // still in use + force (success)
  {
    v8g->_vocbase = &systemDBVocbase;
    std::vector<v8::Local<v8::Value>> args = {
        TRI_V8_ASCII_STRING(isolate, "testAnalyzer2"),
        v8::True(isolate),
    };
    auto inUseAnalyzer = analyzers.get(
        arangodb::StaticStrings::SystemDatabase + "::testAnalyzer2",
        arangodb::QueryAnalyzerRevisions::QUERY_LATEST,
        arangodb::transaction::Hints::TrxType::INTERNAL);  // hold ref to mark
                                                           // in-use
    ASSERT_FALSE(!inUseAnalyzer);

    arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
    auto& user = userMap
                     .emplace("", arangodb::auth::User::newUser(
                                      "", "", arangodb::auth::Source::LDAP))
                     .first->second;
    user.grantDatabase(
        systemDBVocbase.name(),
        arangodb::auth::Level::RW);  // for system collections
                                     // User::collectionAuthLevel(...) returns
                                     // database auth::Level
    userManager->setAuthInfo(userMap);  // set user map to avoid loading
                                        // configuration from system database

    auto result =
        v8::Function::Cast(*fn_remove)
            ->CallAsFunction(context, fn_remove, static_cast<int>(args.size()),
                             args.data());
    ASSERT_FALSE(result.IsEmpty());
    ASSERT_TRUE(result.ToLocalChecked()->IsUndefined());
    auto analyzer = analyzers.get(
        arangodb::StaticStrings::SystemDatabase + "::testAnalyzer2",
        arangodb::QueryAnalyzerRevisions::QUERY_LATEST,
        arangodb::transaction::Hints::TrxType::INTERNAL);
    EXPECT_FALSE(analyzer);
  }

  // success removal
  {
    v8g->_vocbase = &systemDBVocbase;
    std::vector<v8::Local<v8::Value>> args = {
        TRI_V8_ASCII_STRING(isolate, "testAnalyzer1"),
    };

    arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
    auto& user = userMap
                     .emplace("", arangodb::auth::User::newUser(
                                      "", "", arangodb::auth::Source::LDAP))
                     .first->second;
    user.grantDatabase(
        systemDBVocbase.name(),
        arangodb::auth::Level::RW);  // for system collections
                                     // User::collectionAuthLevel(...) returns
                                     // database auth::Level
    userManager->setAuthInfo(userMap);  // set user map to avoid loading
                                        // configuration from system database

    auto result =
        v8::Function::Cast(*fn_remove)
            ->CallAsFunction(context, fn_remove, static_cast<int>(args.size()),
                             args.data());
    ASSERT_FALSE(result.IsEmpty());
    ASSERT_TRUE(result.ToLocalChecked()->IsUndefined());
    auto analyzer = analyzers.get(
        arangodb::StaticStrings::SystemDatabase + "::testAnalyzer1",
        arangodb::QueryAnalyzerRevisions::QUERY_LATEST,
        arangodb::transaction::Hints::TrxType::INTERNAL);
    EXPECT_FALSE(analyzer);
  }
  // removal by system db name with ::
  {
    v8g->_vocbase = &systemDBVocbase;
    std::vector<v8::Local<v8::Value>> args = {
        TRI_V8_ASCII_STRING(isolate, "::testAnalyzer3"),
        v8::False(isolate),
    };

    arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
    auto& user = userMap
                     .emplace("", arangodb::auth::User::newUser(
                                      "", "", arangodb::auth::Source::LDAP))
                     .first->second;
    user.grantDatabase(
        systemDBVocbase.name(),
        arangodb::auth::Level::RW);  // for system collections
                                     // User::collectionAuthLevel(...) returns
                                     // database auth::Level
    userManager->setAuthInfo(userMap);  // set user map to avoid loading
                                        // configuration from system database

    arangodb::velocypack::Builder response;
    v8::TryCatch tryCatch(isolate);
    auto result =
        v8::Function::Cast(*fn_remove)
            ->CallAsFunction(context, fn_remove, static_cast<int>(args.size()),
                             args.data());
    ASSERT_FALSE(result.IsEmpty());
    ASSERT_TRUE(result.ToLocalChecked()->IsUndefined());
    auto analyzer = analyzers.get(
        arangodb::StaticStrings::SystemDatabase + "::testAnalyzer3",
        arangodb::QueryAnalyzerRevisions::QUERY_LATEST,
        arangodb::transaction::Hints::TrxType::INTERNAL);
    EXPECT_EQ(nullptr, analyzer);
  }
  //  removal from wrong db
  {
    v8g->_vocbase = &systemDBVocbase;
    std::vector<v8::Local<v8::Value>> args = {
        TRI_V8_ASCII_STRING(isolate, "testVocbase::testAnalyzer1"),
        v8::False(isolate),
    };

    arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
    auto& user = userMap
                     .emplace("", arangodb::auth::User::newUser(
                                      "", "", arangodb::auth::Source::LDAP))
                     .first->second;
    user.grantDatabase(
        systemDBVocbase.name(),
        arangodb::auth::Level::RW);  // for system collections
                                     // User::collectionAuthLevel(...) returns
                                     // database auth::Level
    user.grantDatabase(
        "testVocbase",
        arangodb::auth::Level::RW);  // for system collections
                                     // User::collectionAuthLevel(...) returns
                                     // database auth::Level
    userManager->setAuthInfo(userMap);  // set user map to avoid loading
                                        // configuration from system database

    arangodb::velocypack::Builder response;
    v8::TryCatch tryCatch(isolate);
    auto result =
        v8::Function::Cast(*fn_remove)
            ->CallAsFunction(context, fn_remove, static_cast<int>(args.size()),
                             args.data());
    ASSERT_TRUE(result.IsEmpty());
    ASSERT_TRUE(tryCatch.HasCaught());
    TRI_V8ToVPack(isolate, response, tryCatch.Exception(), false);
    auto slice = response.slice();
    ASSERT_TRUE(slice.isObject());
    ASSERT_TRUE((slice.hasKey(arangodb::StaticStrings::ErrorNum) &&
                 slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() &&
                 TRI_ERROR_FORBIDDEN ==
                     ErrorCode{slice.get(arangodb::StaticStrings::ErrorNum)
                                   .getNumber<int>()}));
    auto analyzer =
        analyzers.get("testVocbase::testAnalyzer1",
                      arangodb::QueryAnalyzerRevisions::QUERY_LATEST,
                      arangodb::transaction::Hints::TrxType::INTERNAL);
    EXPECT_NE(nullptr, analyzer);
  }
  // success removal from non-system db
  {
    v8g->_vocbase = &testDBVocbase;
    std::vector<v8::Local<v8::Value>> args = {
        TRI_V8_ASCII_STRING(isolate, "testAnalyzer2"),
    };

    arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
    auto& user = userMap
                     .emplace("", arangodb::auth::User::newUser(
                                      "", "", arangodb::auth::Source::LDAP))
                     .first->second;
    user.grantDatabase(
        testDBVocbase.name(),
        arangodb::auth::Level::RW);  // for system collections
                                     // User::collectionAuthLevel(...) returns
                                     // database auth::Level
    userManager->setAuthInfo(userMap);  // set user map to avoid loading
                                        // configuration from system database

    auto result =
        v8::Function::Cast(*fn_remove)
            ->CallAsFunction(context, fn_remove, static_cast<int>(args.size()),
                             args.data());
    ASSERT_FALSE(result.IsEmpty());
    ASSERT_TRUE(result.ToLocalChecked()->IsUndefined());
    auto analyzer =
        analyzers.get("testVocbase::testAnalyzer2",
                      arangodb::QueryAnalyzerRevisions::QUERY_LATEST,
                      arangodb::transaction::Hints::TrxType::INTERNAL);
    EXPECT_EQ(nullptr, analyzer);
  }
  // success removal with db name prefix
  {
    v8g->_vocbase = &testDBVocbase;
    std::vector<v8::Local<v8::Value>> args = {
        TRI_V8_ASCII_STRING(isolate, "testVocbase::testAnalyzer3"),
    };

    arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
    auto& user = userMap
                     .emplace("", arangodb::auth::User::newUser(
                                      "", "", arangodb::auth::Source::LDAP))
                     .first->second;

    // for system collections User::collectionAuthLevel(...) returns
    // database auth::Level
    user.grantDatabase(testDBVocbase.name(), arangodb::auth::Level::RW);

    // set user map to avoid loading configuration from system database
    userManager->setAuthInfo(userMap);

    auto result =
        v8::Function::Cast(*fn_remove)
            ->CallAsFunction(context, fn_remove, static_cast<int>(args.size()),
                             args.data());
    ASSERT_FALSE(result.IsEmpty());
    ASSERT_TRUE(result.ToLocalChecked()->IsUndefined());
    auto analyzer =
        analyzers.get("testVocbase::testAnalyzer3",
                      arangodb::QueryAnalyzerRevisions::QUERY_LATEST,
                      arangodb::transaction::Hints::TrxType::INTERNAL);
    EXPECT_EQ(nullptr, analyzer);
  }
}
