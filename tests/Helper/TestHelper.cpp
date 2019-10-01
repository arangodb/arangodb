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
/// @author Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "TestHelper.h"
#include <libplatform/libplatform.h>
#include <src/api.h>
#include <src/objects-inl.h>
#include <src/objects/scope-info.h>

#include "Mocks/Servers.h"

#include "Replication/ReplicationFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "RestServer/ViewTypesFeature.h"
#include "Sharding/ShardingFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "V8/v8-vpack.h"
#include "V8Server/V8DealerFeature.h"
#include "V8Server/v8-users.h"

arangodb::tests::TestHelper::TestHelper() : _v8Initialized(false) {}

arangodb::tests::TestHelper::~TestHelper() {
  v8Teardown();
}

// -----------------------------------------------------------------------------
// Mock Servers
// -----------------------------------------------------------------------------

arangodb::tests::mocks::MockAqlServer* arangodb::tests::TestHelper::mockAqlServerInit() {
  _mockAqlServer.reset(new arangodb::tests::mocks::MockAqlServer());
  _system = _mockAqlServer->getFeature<arangodb::SystemDatabaseFeature>().use();
  return _mockAqlServer.get();
}

// -----------------------------------------------------------------------------
// V8
// -----------------------------------------------------------------------------

v8::Isolate* arangodb::tests::TestHelper::v8Isolate() {
  return _v8Isolate;
}

v8::Handle<v8::Context> arangodb::tests::TestHelper::v8Context() {
  return v8::Local<v8::Context>::New(_v8Isolate, _v8Context);
}

TRI_v8_global_t* arangodb::tests::TestHelper::v8Globals() {
  return _v8Globals;
}

void arangodb::tests::TestHelper::v8Init() {
  struct init_t {
    std::shared_ptr<v8::Platform> platform;
    init_t() {
      platform = std::shared_ptr<v8::Platform>(v8::platform::CreateDefaultPlatform(),
                                               [](v8::Platform* p) -> void {
                                                 v8::V8::Dispose();
                                                 v8::V8::ShutdownPlatform();
                                                 delete p;
                                               });
      v8::V8::InitializePlatform(platform.get());  // avoid SIGSEGV duing 8::Isolate::New(...)
      v8::V8::Initialize();  // avoid error: "Check failed: thread_data_table_"
    }
  };
  static const init_t init;
  (void)(init);
}

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
}  // namespace

void arangodb::tests::TestHelper::v8Setup(TRI_vocbase_t* vocbase) {
  v8Init();

  if (!_v8Initialized) {
  v8::Isolate::CreateParams isolateParams;
  ArrayBufferAllocator arrayBufferAllocator;
  isolateParams.array_buffer_allocator = &arrayBufferAllocator;
  _v8Isolate = v8::Isolate::New(isolateParams);
  ASSERT_NE(nullptr, _v8Isolate);
  _v8Isolate->Enter();

  // otherwise v8::Isolate::Logger() will fail (called from v8::Exception::Error)
  v8::Isolate::Scope isolateScope(_v8Isolate);

  // otherwise v8::Isolate::Logger() will fail (called from v8::Exception::Error)
  v8::internal::Isolate::Current()->InitializeLoggingAndCounters();

  // required for v8::Context::New(...), v8::ObjectTemplate::New(...) and TRI_AddMethodVocbase(...)
  v8::HandleScope handleScope(_v8Isolate);

  v8::Handle<v8::ObjectTemplate> global = v8::ObjectTemplate::New(_v8Isolate);
  _v8Context.Reset(_v8Isolate, v8::Context::New(_v8Isolate, nullptr, global));
  v8::Handle<v8::Context> context = v8::Local<v8::Context>::New(_v8Isolate, _v8Context);
  
  // required for TRI_AddMethodVocbase(...) later
  v8::Context::Scope contextScope(context);

  // create and set inside 'isolate' for use with 'TRI_GET_GLOBALS()'
  _v8Globals = TRI_CreateV8Globals(_v8Isolate, 0);

  // otherwise v8:-utils::CreateErrorObject(...) will fail
  _v8Globals->ArangoErrorTempl.Reset(_v8Isolate,
                                     v8::ObjectTemplate::New(_v8Isolate));

  _v8Globals->_vocbase = vocbase;
  TRI_InitV8Users(context, vocbase, _v8Globals, _v8Isolate);

  _v8Initialized = true;
  }
}

void arangodb::tests::TestHelper::v8Teardown() {
  if (_v8Initialized) {
  delete _v8Globals;
  _v8Globals = nullptr;

  _v8Context.Reset();

  _v8Isolate->Exit();
  _v8Isolate->Dispose();
  _v8Isolate = nullptr;

  _v8Initialized = false;
  }
}

// -----------------------------------------------------------------------------
// Users and ExecContext
// -----------------------------------------------------------------------------

arangodb::ExecContext* arangodb::tests::TestHelper::createExecContext(
    auth::AuthUser const& user, auth::DatabaseResource const& database) {
  _exec = ExecContext::create(user, database);
  return _exec.get();
}

void arangodb::tests::TestHelper::disposeExecContext() {
  _exec.reset();
}
								      

void arangodb::tests::TestHelper::usersSetup() {
  auto usersJson = arangodb::velocypack::Parser::fromJson(
      "{ \"name\": \"_users\", \"isSystem\": true }");

  _scopedUsers = std::shared_ptr<arangodb::LogicalCollection>(
        _system.get()->createCollection(usersJson->slice()).get(),
        [this](arangodb::LogicalCollection* ptr) -> void {
          _system.get()->dropCollection(ptr->id(), true, 0.0);
        });
}

void arangodb::tests::TestHelper::createUser(std::string const& username,
                                             std::function<void(auth::User*)> callback) {
  auto* authFeature = arangodb::AuthenticationFeature::instance();
  ASSERT_NE(nullptr, authFeature);

  auto* userManager = authFeature->userManager();
  ASSERT_NE(nullptr, userManager);

  arangodb::auth::User user =
      arangodb::auth::User::newUser(username, "", arangodb::auth::Source::Test);
  callback(&user);

  userManager->setAuthInfo(arangodb::auth::UserMap{});
  auto res = userManager->storeUser(user);

  ASSERT_EQ(true, res.ok());
}

// -----------------------------------------------------------------------------
// Databases
// -----------------------------------------------------------------------------

TRI_vocbase_t* arangodb::tests::TestHelper::createDatabase(std::string const& dbName) {}

// -----------------------------------------------------------------------------
// Collections
// -----------------------------------------------------------------------------

std::shared_ptr<arangodb::LogicalCollection> arangodb::tests::TestHelper::createCollection(
    TRI_vocbase_t*, auth::CollectionResource const&) {}

// -----------------------------------------------------------------------------
// Views
// -----------------------------------------------------------------------------

namespace {
struct TestView : public arangodb::LogicalView {
  arangodb::Result _appendVelocyPackResult;
  arangodb::velocypack::Builder _properties;

  TestView(TRI_vocbase_t& vocbase, arangodb::velocypack::Slice const& definition, uint64_t planVersion)
      : arangodb::LogicalView(vocbase, definition, planVersion) {}
  virtual arangodb::Result appendVelocyPackImpl(
      arangodb::velocypack::Builder& builder,
      std::underlying_type<arangodb::LogicalDataSource::Serialize>::type) const override {
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

struct ViewFactoryTest : public arangodb::ViewFactory {
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

void arangodb::tests::TestHelper::viewFactoryInit(mocks::MockServer* server) {
  _viewFactory.reset(new ViewFactoryTest());
  auto& viewTypesFeature = server->getFeature<arangodb::ViewTypesFeature>();
  viewTypesFeature.emplace(arangodb::LogicalDataSource::Type::emplace(
                               arangodb::velocypack::StringRef("testViewType")),
                           *_viewFactory);
}

std::shared_ptr<arangodb::LogicalView> arangodb::tests::TestHelper::createView(
    TRI_vocbase_t*, auth::CollectionResource const&) {}

void arangodb::tests::TestHelper::callFunction(v8::Handle<v8::Value>,
                                               std::vector<v8::Local<v8::Value>>&) {}

void arangodb::tests::TestHelper::callFunctionThrow(v8::Handle<v8::Value>,
                                                    std::vector<v8::Local<v8::Value>>&,
                                                    int errorCode) {}

#if 0

#include "Aql/QueryRegistry.h"
#include "Basics/StaticStrings.h"
#include "GeneralServer/AuthenticationFeature.h"
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

// #include "src/api.h"
// #include "src/objects-inl.h"
// #include "src/objects/scope-info.h"






std::shared_ptr<arangodb::LogicalCollection> arangodb::tests::TestHelper::createCollection(TRI_vocbase_t* vocbase, CollectionResource const& collection) {
  auto collectionJson = arangodb::velocypack::Parser::fromJson(
      "{ \"name\": \"" + collection.collectionName() "\" }");

  auto logicalCollection = std::shared_ptr<arangodb::LogicalCollection>(
      vocbase->createCollection(collectionJson->slice()).get(),
      [vocbase](arangodb::LogicalCollection* ptr) -> void {
	vocbase->dropCollection(ptr->id(), false, 0);
      });

  ASSERT_NE(nullptr, logicalCollection.get());

  return logicalCollection;
}

std::shared_ptr<arangodb::LogicalView> arangodb::tests::TestHelper::createView(std::string const& collectionName) {
    auto viewJson = arangodb::velocypack::Parser::fromJson(
      "{ \"name\": \"" + collection.collectionName() "\", \"type\": \"testViewType\" }");

    auto logicalView = std::shared_ptr<arangodb::LogicalView>(
        vocbase->createView(viewJson->slice()).get(),
        [vocbase](arangodb::LogicalView* ptr) -> void {
          vocbase->dropView(ptr->id(), false);
        });
  ASSERT_NE(nullptr, logicalCollection.get());

  return logicalView;
}

void callFunction() {
  v8::TryCatch tryCatch(isolate.get());
  auto result = v8::Function::Cast(*fn_revokeCollection)
                    ->CallAsFunction(context, arangoUsers,
                                     static_cast<int>(revokeWildcardArgs.size()),
                                     revokeWildcardArgs.data());
  EXPECT_TRUE((!result.IsEmpty()));
  EXPECT_TRUE((result.ToLocalChecked()->IsUndefined()));
  EXPECT_TRUE((!tryCatch.HasCaught()));
}

void callFunctionThrow() {
  v8::TryCatch tryCatch(isolate.get());

  auto result = v8::Function::Cast(*fn_revokeCollection)
		    ->CallAsFunction(context, arangoUsers,
				     static_cast<int>(revokeArgs.size()),
				     revokeArgs.data());
  EXPECT_TRUE((result.IsEmpty()));
  EXPECT_TRUE((tryCatch.HasCaught()));

  arangodb::velocypack::Builder response;

  EXPECT_TRUE((TRI_ERROR_NO_ERROR == TRI_V8ToVPack(isolate.get(), response,
						   tryCatch.Exception(), false)));
  auto slice = response.slice();
  EXPECT_TRUE((slice.isObject()));
  EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::ErrorNum) &&
	       slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() &&
	       TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND ==
		   slice.get(arangodb::StaticStrings::ErrorNum).getNumber<int>()));
}

StorageEngineMock engine;
  ViewFactory viewFactory;
  std::vector<std::pair<arangodb::application_features::ApplicationFeature*, bool>> features;
  std::unique_ptr<TRI_vocbase_t> system;


    arangodb::EngineSelectorFeature::ENGINE = &engine;

void mockDatabase() {

}

unmockDatabase  ~V8UsersTest() {
    system.reset();  // destroy before reseting the 'ENGINE'
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

    arangodb::EngineSelectorFeature::ENGINE =
        nullptr;  // nullify only after DatabaseFeature::unprepare()



    createDatabase() {
  auto* databaseFeature =
      arangodb::application_features::ApplicationServer::getFeature<arangodb::DatabaseFeature>(
          "Database");
  TRI_vocbase_t* vocbase;  // will be owned by DatabaseFeature
  ASSERT_TRUE(databaseFeature->createDatabase(1, "testDatabase", vocbase).ok());
    }



    /// ??? 
  auto* authFeature = arangodb::AuthenticationFeature::instance();
  auto* userManager = authFeature->userManager();
  arangodb::aql::QueryRegistry queryRegistry(0);  // required for UserManager::loadFromDB()
  userManager->setGlobalVersion(0);  // required for UserManager::loadFromDB()
  userManager->setQueryRegistry(&queryRegistry);


  // exec context
  std::unique_ptr<arangodb::ExecContext> userScope(
      arangodb::ExecContext::create(arangodb::auth::AuthUser{V8UsersTest::USERNAME},
                                    arangodb::auth::DatabaseResource{vocbase->name()})
          .release());

#if 0

struct TestView : public arangodb::LogicalView {
  arangodb::Result _appendVelocyPackResult;
  arangodb::velocypack::Builder _properties;

  TestView(TRI_vocbase_t& vocbase, arangodb::velocypack::Slice const& definition, uint64_t planVersion)
      : arangodb::LogicalView(vocbase, definition, planVersion) {}
  virtual arangodb::Result appendVelocyPackImpl(
      arangodb::velocypack::Builder& builder,
      std::underlying_type<arangodb::LogicalDataSource::Serialize>::type) const override {
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



#endif

#endif
