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

#include <libplatform/libplatform.h>
#include <src/api.h>
#include <src/objects-inl.h>
#include <src/objects/scope-info.h>

#include "TestHelper.h"

#include "../IResearch/common.h"
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

arangodb::tests::TestHelper::TestHelper()
    : _mockServer(nullptr),
      _system(nullptr),
      _v8Initialized(false),
      _v8Isolate(nullptr),
      _v8Globals(nullptr) {}

arangodb::tests::TestHelper::~TestHelper() { v8Teardown(); }

// -----------------------------------------------------------------------------
// Mock Servers
// -----------------------------------------------------------------------------

arangodb::tests::mocks::MockServer* arangodb::tests::TestHelper::mockAqlServerInit() {
  _mockServer.reset(new arangodb::tests::mocks::MockAqlServer());
  _system = _mockServer->getFeature<arangodb::SystemDatabaseFeature>().use();
  return _mockServer.get();
}

// -----------------------------------------------------------------------------
// V8
// -----------------------------------------------------------------------------

v8::Isolate* arangodb::tests::TestHelper::v8Isolate() { return _v8Isolate; }

v8::Handle<v8::Context> arangodb::tests::TestHelper::v8Context() {
  return v8::Local<v8::Context>::New(_v8Isolate, _v8Context);
}

TRI_v8_global_t* arangodb::tests::TestHelper::v8Globals() { return _v8Globals; }

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

void arangodb::tests::TestHelper::v8Setup() {
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
    _v8Globals->ArangoErrorTempl.Reset(_v8Isolate, v8::ObjectTemplate::New(_v8Isolate));

    _v8Globals->_vocbase = _system.get();
    TRI_InitV8Users(context, _v8Globals->_vocbase, _v8Globals, _v8Isolate);

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

void arangodb::tests::TestHelper::callFunction(v8::Persistent<v8::Object>& obj,
                                               v8::Persistent<v8::Value>& func,
                                               std::vector<std::string> const& args) {
  ASSERT_NE(nullptr, _v8Isolate);

  v8::HandleScope handleScope(_v8Isolate);

  auto context = v8Context();
  v8::Context::Scope contextScope(context);

  auto f = v8::Local<v8::Value>::New(_v8Isolate, func);
  std::vector<v8::Local<v8::Value>> arguments;

  for (auto a : args) {
    arguments.push_back(TRI_V8_STD_STRING(_v8Isolate, a));
  }

  v8::TryCatch tryCatch(_v8Isolate);

  auto result =
      v8::Function::Cast(*f)->CallAsFunction(context,
                                             v8::Local<v8::Object>::New(_v8Isolate, obj),
                                             static_cast<int>(arguments.size()),
                                             arguments.data());

  ASSERT_FALSE(result.IsEmpty());
  ASSERT_TRUE(result.ToLocalChecked()->IsUndefined());
  ASSERT_FALSE(tryCatch.HasCaught());
}

void arangodb::tests::TestHelper::callFunctionThrow(v8::Persistent<v8::Object>& obj,
                                                    v8::Persistent<v8::Value>& func,
                                                    std::vector<std::string> const& args,
                                                    int errorCode) {
  ASSERT_NE(nullptr, _v8Isolate);

  v8::HandleScope handleScope(_v8Isolate);

  auto context = v8Context();
  v8::Context::Scope contextScope(context);

  auto f = v8::Local<v8::Value>::New(_v8Isolate, func);
  std::vector<v8::Local<v8::Value>> arguments;

  for (auto a : args) {
    arguments.push_back(TRI_V8_STD_STRING(_v8Isolate, a));
  }

  v8::TryCatch tryCatch(_v8Isolate);

  auto result =
      v8::Function::Cast(*f)->CallAsFunction(context,
                                             v8::Local<v8::Object>::New(_v8Isolate, obj),
                                             static_cast<int>(arguments.size()),
                                             arguments.data());
  ASSERT_TRUE(result.IsEmpty());
  ASSERT_TRUE(tryCatch.HasCaught());

  arangodb::velocypack::Builder response;
  auto res = TRI_V8ToVPack(_v8Isolate, response, tryCatch.Exception(), false);

  ASSERT_EQ(TRI_ERROR_NO_ERROR, res);

  auto slice = response.slice();

  ASSERT_TRUE(slice.isObject());
  ASSERT_TRUE(slice.hasKey(arangodb::StaticStrings::ErrorNum));
  ASSERT_TRUE(slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>());
  ASSERT_EQ(errorCode, slice.get(arangodb::StaticStrings::ErrorNum).getNumber<int>());
}

// -----------------------------------------------------------------------------
// Users and ExecContext
// -----------------------------------------------------------------------------

arangodb::ExecContext* arangodb::tests::TestHelper::createExecContext(
    auth::AuthUser const& user, auth::DatabaseResource const& database) {
  _exec = ExecContext::create(user, database);
  return _exec.get();
}

void arangodb::tests::TestHelper::disposeExecContext() { _exec.reset(); }

void arangodb::tests::TestHelper::usersSetup() {
  auto usersJson = arangodb::velocypack::Parser::fromJson(
      "{ \"name\": \"_users\", \"isSystem\": true }");

  _scopedUsers = std::shared_ptr<arangodb::LogicalCollection>(
      _system.get()->createCollection(usersJson->slice()).get(),
      [this](arangodb::LogicalCollection* ptr) -> void {
        _system.get()->dropCollection(ptr->id(), true, 0.0);
      });
}

void arangodb::tests::TestHelper::usersTeardown() { _scopedUsers.reset(); }

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

  ASSERT_TRUE(res.ok());
}

// -----------------------------------------------------------------------------
// Databases
// -----------------------------------------------------------------------------

TRI_vocbase_t* arangodb::tests::TestHelper::createDatabase(std::string const& dbName) {
  TRI_vocbase_t* vocbase;  // will be owned by DatabaseFeature
  auto& databaseFeature = _mockServer->getFeature<arangodb::DatabaseFeature>();
  auto res = databaseFeature.createDatabase(testDBInfo(_mockServer->server(), dbName), vocbase);
  EXPECT_TRUE(res.ok());

  return vocbase;
}

// -----------------------------------------------------------------------------
// Collections
// -----------------------------------------------------------------------------

std::shared_ptr<arangodb::LogicalCollection> arangodb::tests::TestHelper::createCollection(
    TRI_vocbase_t* vocbase, auth::CollectionResource const& collection) {
  auto collectionJson = arangodb::velocypack::Parser::fromJson(
      "{ \"name\": \"" + collection.collection() + "\" }");

  auto logicalCollection = std::shared_ptr<arangodb::LogicalCollection>(
      vocbase->createCollection(collectionJson->slice()).get(),
      [vocbase](arangodb::LogicalCollection* ptr) -> void {
        vocbase->dropCollection(ptr->id(), false, 0);
      });

  EXPECT_NE(nullptr, logicalCollection.get());

  return logicalCollection;
}

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
    TRI_vocbase_t* vocbase, auth::CollectionResource const& view) {
  auto viewJson = arangodb::velocypack::Parser::fromJson(
      "{ \"name\": \"" + view.collection() + "\", \"type\": \"testViewType\" }");

  auto logicalView = std::shared_ptr<arangodb::LogicalView>(
      vocbase->createView(viewJson->slice()).get(),
      [vocbase](arangodb::LogicalView* ptr) -> void {
        vocbase->dropView(ptr->id(), false);
      });

  EXPECT_NE(nullptr, logicalView.get());

  return logicalView;
}
