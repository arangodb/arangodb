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

#include "TestHelper.h"

void arangodb::TestHelper::createUser(std::string const& username,
				    std::function<bool(User* user)> permissions) {
  auto* userManager = authFeature->userManager();
  ASSERT_EQ(nullptr, userManager);

  arangodb::auth::User* user = User::newUser(username, "", arangodb::auth::Source::TEST);
  permissions(user);
  userManager->setAuthInfo(arangodb::auth::UserMap{});
  userManager->storeUser(user);
}

std::shared_ptr<arangodb::LogicalCollection> arangodb::TestHelper::createCollection(TRI_vocbase_t* vocbase, CollectionResource const& collection) {
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

std::shared_ptr<arangodb::LogicalView> arangodb::TestHelper::createView(std::string const& collectionName) {
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

createSystemDatabase() {
    system = std::make_unique<TRI_vocbase_t>(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL,
                                             0, TRI_VOC_SYSTEM_DATABASE);
}

StorageEngineMock engine;
  ViewFactory viewFactory;
  arangodb::application_features::ApplicationServer server;
  std::vector<std::pair<arangodb::application_features::ApplicationFeature*, bool>> features;
  std::unique_ptr<TRI_vocbase_t> system;

 server(nullptr, nullptr)

    arangodb::EngineSelectorFeature::ENGINE = &engine;

void initV8 () {
    arangodb::tests::v8Init();  // on-time initialize V8
}

void mockDatabase() {

    features.emplace_back(new arangodb::AuthenticationFeature(server), false);  // required for VocbaseContext
    features.emplace_back(new arangodb::DatabaseFeature(server),
                          false);  // required for UserManager::updateUser(...)
    features.emplace_back(new arangodb::QueryRegistryFeature(server), false);  // required for TRI_vocbase_t
    arangodb::application_features::ApplicationServer::server->addFeature(
        features.back().first);  // need QueryRegistryFeature feature to be added now in order to create the system database
    features.emplace_back(new arangodb::ReplicationFeature(server), false);  // required for DatabaseFeature::createDatabase(...)
    features.emplace_back(new arangodb::ShardingFeature(server),
                          false);  // required for LogicalCollection::LogicalCollection(...)
    features.emplace_back(new arangodb::ViewTypesFeature(server),
                          false);  // required for LogicalView::create(...)

    system = helper.createSystemDatabase();
    features.emplace_back(new arangodb::SystemDatabaseFeature(server, system.get()),
                          false);  // required for IResearchAnalyzerFeature

#if USE_ENTERPRISE
    features.emplace_back(new arangodb::LdapFeature(server),
                          false);  // required for AuthenticationFeature with USE_ENTERPRISE
#endif

    arangodb::application_features::ApplicationServer::server->addFeature(
        new arangodb::V8DealerFeature(server));  // add without calling prepare(), required for DatabaseFeature::createDatabase(...)

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

    auto* viewTypesFeature =
        arangodb::application_features::ApplicationServer::lookupFeature<arangodb::ViewTypesFeature>();

    viewTypesFeature->emplace(arangodb::LogicalDataSource::Type::emplace(arangodb::velocypack::StringRef(
                                  "testViewType")),
                              viewFactory);
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


    setupV8 {
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
  std::unique_ptr<TRI_v8_global_t> v8g(TRI_CreateV8Globals(isolate.get(), 0));  // create and set inside 'isolate' for use with 'TRI_GET_GLOBALS()'
  v8g->ArangoErrorTempl.Reset(isolate.get(), v8::ObjectTemplate::New(isolate.get()));  // otherwise v8:-utils::CreateErrorObject(...) will fail
  v8g->_vocbase = vocbase;
  TRI_InitV8Users(context, vocbase, v8g.get(), isolate.get());
  return context
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



struct ViewFactory : public arangodb::ViewFactory {
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



#endif
