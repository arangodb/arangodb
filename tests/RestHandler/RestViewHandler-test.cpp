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

#include "catch.hpp"
#include "../IResearch/StorageEngineMock.h"
#include "../IResearch/RestHandlerMock.h"
#include "Aql/QueryRegistry.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "RestHandler/RestViewHandler.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "RestServer/ViewTypesFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "velocypack/Builder.h"
#include "velocypack/Parser.h"

namespace {

template <typename T, typename U>
std::shared_ptr<T> scopedPtr(T*& ptr, U* newValue) {
  auto* location = &ptr;
  auto* oldValue = ptr;
  ptr = newValue;
  return std::shared_ptr<T>(oldValue, [location](T* p)->void { *location = p; });
}

}

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

struct RestViewHandlerSetup {
  StorageEngineMock engine;
  arangodb::application_features::ApplicationServer server;
  std::vector<std::pair<arangodb::application_features::ApplicationFeature*, bool>> features;

  RestViewHandlerSetup(): server(nullptr, nullptr) {
    arangodb::EngineSelectorFeature::ENGINE = &engine;

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

  ~RestViewHandlerSetup() {
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

TEST_CASE("RestViewHandlerTest", "[rest]") {
  RestViewHandlerSetup s;
  (void)(s);

SECTION("test_auth") {
  // test create
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto requestPtr = std::make_unique<GeneralRequestMock>(vocbase);
    auto& request = *requestPtr;
    auto responcePtr = std::make_unique<GeneralResponseMock>();
    auto& responce = *responcePtr;
    arangodb::RestViewHandler handler(requestPtr.release(), responcePtr.release());

    request.setRequestType(arangodb::rest::RequestType::POST);
    request._payload.openObject();
    request._payload.add(arangodb::StaticStrings::DataSourceName, arangodb::velocypack::Value("testView"));
    request._payload.add(arangodb::StaticStrings::DataSourceType, arangodb::velocypack::Value("testViewType"));
    request._payload.close();

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

      auto status = handler.execute();
      CHECK((arangodb::RestStatus::DONE == status));
      CHECK((arangodb::rest::ResponseCode::FORBIDDEN == responce.responseCode()));
      auto slice = responce._payload.slice();
      CHECK((slice.isObject()));
      CHECK((slice.hasKey(arangodb::StaticStrings::Code) && slice.get(arangodb::StaticStrings::Code).isNumber<size_t>() && size_t(arangodb::rest::ResponseCode::FORBIDDEN) == slice.get(arangodb::StaticStrings::Code).getNumber<size_t>()));
      CHECK((slice.hasKey(arangodb::StaticStrings::Error) && slice.get(arangodb::StaticStrings::Error).isBoolean() && true == slice.get(arangodb::StaticStrings::Error).getBoolean()));
      CHECK((slice.hasKey(arangodb::StaticStrings::ErrorNum) && slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() && TRI_ERROR_FORBIDDEN == slice.get(arangodb::StaticStrings::ErrorNum).getNumber<int>()));
      CHECK((vocbase.views().empty()));
    }

    // not authorized (RO user)
    {
      arangodb::auth::UserMap userMap;
      auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
      user.grantDatabase(vocbase.name(), arangodb::auth::Level::RO);
      userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

      auto status = handler.execute();
      CHECK((arangodb::RestStatus::DONE == status));
      CHECK((arangodb::rest::ResponseCode::FORBIDDEN == responce.responseCode()));
      auto slice = responce._payload.slice();
      CHECK((slice.isObject()));
      CHECK((slice.hasKey(arangodb::StaticStrings::Code) && slice.get(arangodb::StaticStrings::Code).isNumber<size_t>() && size_t(arangodb::rest::ResponseCode::FORBIDDEN) == slice.get(arangodb::StaticStrings::Code).getNumber<size_t>()));
      CHECK((slice.hasKey(arangodb::StaticStrings::Error) && slice.get(arangodb::StaticStrings::Error).isBoolean() && true == slice.get(arangodb::StaticStrings::Error).getBoolean()));
      CHECK((slice.hasKey(arangodb::StaticStrings::ErrorNum) && slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() && TRI_ERROR_FORBIDDEN == slice.get(arangodb::StaticStrings::ErrorNum).getNumber<int>()));
      CHECK((vocbase.views().empty()));
    }

    // authorzed (RW user)
    {
      arangodb::auth::UserMap userMap;
      auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
      user.grantDatabase(vocbase.name(), arangodb::auth::Level::RW);
      userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

      auto status = handler.execute();
      CHECK((arangodb::RestStatus::DONE == status));
      CHECK((arangodb::rest::ResponseCode::CREATED == responce.responseCode()));
      auto slice = responce._payload.slice();
      CHECK((slice.isObject()));
      CHECK((slice.hasKey(arangodb::StaticStrings::DataSourceName) && slice.get(arangodb::StaticStrings::DataSourceName).isString() && std::string("testView") == slice.get(arangodb::StaticStrings::DataSourceName).copyString()));
      auto view = vocbase.lookupView("testView");
      CHECK((false == !view));
    }
  }

  // test drop
  {
    auto createViewJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"testViewType\" }");
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalView = vocbase.createView(createViewJson->slice());
    REQUIRE((false == !logicalView));
    auto requestPtr = std::make_unique<GeneralRequestMock>(vocbase);
    auto& request = *requestPtr;
    auto responcePtr = std::make_unique<GeneralResponseMock>();
    auto& responce = *responcePtr;
    arangodb::RestViewHandler handler(requestPtr.release(), responcePtr.release());

    request.addSuffix("testView");
    request.setRequestType(arangodb::rest::RequestType::DELETE_REQ);

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

      auto status = handler.execute();
      CHECK((arangodb::RestStatus::DONE == status));
      CHECK((arangodb::rest::ResponseCode::FORBIDDEN == responce.responseCode()));
      auto slice = responce._payload.slice();
      CHECK((slice.isObject()));
      CHECK((slice.hasKey(arangodb::StaticStrings::Code) && slice.get(arangodb::StaticStrings::Code).isNumber<size_t>() && size_t(arangodb::rest::ResponseCode::FORBIDDEN) == slice.get(arangodb::StaticStrings::Code).getNumber<size_t>()));
      CHECK((slice.hasKey(arangodb::StaticStrings::Error) && slice.get(arangodb::StaticStrings::Error).isBoolean() && true == slice.get(arangodb::StaticStrings::Error).getBoolean()));
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

      auto status = handler.execute();
      CHECK((arangodb::RestStatus::DONE == status));
      CHECK((arangodb::rest::ResponseCode::FORBIDDEN == responce.responseCode()));
      auto slice = responce._payload.slice();
      CHECK((slice.isObject()));
      CHECK((slice.hasKey(arangodb::StaticStrings::Code) && slice.get(arangodb::StaticStrings::Code).isNumber<size_t>() && size_t(arangodb::rest::ResponseCode::FORBIDDEN) == slice.get(arangodb::StaticStrings::Code).getNumber<size_t>()));
      CHECK((slice.hasKey(arangodb::StaticStrings::Error) && slice.get(arangodb::StaticStrings::Error).isBoolean() && true == slice.get(arangodb::StaticStrings::Error).getBoolean()));
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

      auto status = handler.execute();
      CHECK((arangodb::RestStatus::DONE == status));
      CHECK((arangodb::rest::ResponseCode::FORBIDDEN == responce.responseCode()));
      auto slice = responce._payload.slice();
      CHECK((slice.isObject()));
      CHECK((slice.hasKey(arangodb::StaticStrings::Code) && slice.get(arangodb::StaticStrings::Code).isNumber<size_t>() && size_t(arangodb::rest::ResponseCode::FORBIDDEN) == slice.get(arangodb::StaticStrings::Code).getNumber<size_t>()));
      CHECK((slice.hasKey(arangodb::StaticStrings::Error) && slice.get(arangodb::StaticStrings::Error).isBoolean() && true == slice.get(arangodb::StaticStrings::Error).getBoolean()));
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

      auto status = handler.execute();
      CHECK((arangodb::RestStatus::DONE == status));
      CHECK((arangodb::rest::ResponseCode::OK == responce.responseCode()));
      auto slice = responce._payload.slice();
      CHECK((slice.isObject()));
      CHECK((slice.hasKey("result") && slice.get("result").isBoolean() && true == slice.get("result").getBoolean()));
      CHECK((vocbase.views().empty()));
    }
  }

  // test rename
  {
    auto createViewJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"testViewType\" }");
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalView = vocbase.createView(createViewJson->slice());
    REQUIRE((false == !logicalView));
    auto requestPtr = std::make_unique<GeneralRequestMock>(vocbase);
    auto& request = *requestPtr;
    auto responcePtr = std::make_unique<GeneralResponseMock>();
    auto& responce = *responcePtr;
    arangodb::RestViewHandler handler(requestPtr.release(), responcePtr.release());

    request.addSuffix("testView");
    request.addSuffix("rename");
    request.setRequestType(arangodb::rest::RequestType::PUT);
    request._payload.openObject();
    request._payload.add("name", arangodb::velocypack::Value("testView1"));
    request._payload.close();

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

      auto status = handler.execute();
      CHECK((arangodb::RestStatus::DONE == status));
      CHECK((arangodb::rest::ResponseCode::FORBIDDEN == responce.responseCode()));
      auto slice = responce._payload.slice();
      CHECK((slice.isObject()));
      CHECK((slice.hasKey(arangodb::StaticStrings::Code) && slice.get(arangodb::StaticStrings::Code).isNumber<size_t>() && size_t(arangodb::rest::ResponseCode::FORBIDDEN) == slice.get(arangodb::StaticStrings::Code).getNumber<size_t>()));
      CHECK((slice.hasKey(arangodb::StaticStrings::Error) && slice.get(arangodb::StaticStrings::Error).isBoolean() && true == slice.get(arangodb::StaticStrings::Error).getBoolean()));
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

      auto status = handler.execute();
      CHECK((arangodb::RestStatus::DONE == status));
      CHECK((arangodb::rest::ResponseCode::FORBIDDEN == responce.responseCode()));
      auto slice = responce._payload.slice();
      CHECK((slice.isObject()));
      CHECK((slice.hasKey(arangodb::StaticStrings::Code) && slice.get(arangodb::StaticStrings::Code).isNumber<size_t>() && size_t(arangodb::rest::ResponseCode::FORBIDDEN) == slice.get(arangodb::StaticStrings::Code).getNumber<size_t>()));
      CHECK((slice.hasKey(arangodb::StaticStrings::Error) && slice.get(arangodb::StaticStrings::Error).isBoolean() && true == slice.get(arangodb::StaticStrings::Error).getBoolean()));
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

      auto status = handler.execute();
      CHECK((arangodb::RestStatus::DONE == status));
      CHECK((arangodb::rest::ResponseCode::FORBIDDEN == responce.responseCode()));
      auto slice = responce._payload.slice();
      CHECK((slice.isObject()));
      CHECK((slice.hasKey(arangodb::StaticStrings::Code) && slice.get(arangodb::StaticStrings::Code).isNumber<size_t>() && size_t(arangodb::rest::ResponseCode::FORBIDDEN) == slice.get(arangodb::StaticStrings::Code).getNumber<size_t>()));
      CHECK((slice.hasKey(arangodb::StaticStrings::Error) && slice.get(arangodb::StaticStrings::Error).isBoolean() && true == slice.get(arangodb::StaticStrings::Error).getBoolean()));
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

      auto status = handler.execute();
      CHECK((arangodb::RestStatus::DONE == status));
      CHECK((arangodb::rest::ResponseCode::OK == responce.responseCode()));
      auto slice = responce._payload.slice();
      CHECK((slice.isObject()));
      CHECK((slice.hasKey(arangodb::StaticStrings::DataSourceName) && slice.get(arangodb::StaticStrings::DataSourceName).isString() && std::string("testView1") == slice.get(arangodb::StaticStrings::DataSourceName).copyString()));
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
    auto requestPtr = std::make_unique<GeneralRequestMock>(vocbase);
    auto& request = *requestPtr;
    auto responcePtr = std::make_unique<GeneralResponseMock>();
    auto& responce = *responcePtr;
    arangodb::RestViewHandler handler(requestPtr.release(), responcePtr.release());

    request.addSuffix("testView");
    request.addSuffix("properties");
    request.setRequestType(arangodb::rest::RequestType::PUT);
    request._payload.openObject();
    request._payload.add("key", arangodb::velocypack::Value("value"));
    request._payload.close();

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

      auto status = handler.execute();
      CHECK((arangodb::RestStatus::DONE == status));
      CHECK((arangodb::rest::ResponseCode::FORBIDDEN == responce.responseCode()));
      auto slice = responce._payload.slice();
      CHECK((slice.isObject()));
      CHECK((slice.hasKey(arangodb::StaticStrings::Code) && slice.get(arangodb::StaticStrings::Code).isNumber<size_t>() && size_t(arangodb::rest::ResponseCode::FORBIDDEN) == slice.get(arangodb::StaticStrings::Code).getNumber<size_t>()));
      CHECK((slice.hasKey(arangodb::StaticStrings::Error) && slice.get(arangodb::StaticStrings::Error).isBoolean() && true == slice.get(arangodb::StaticStrings::Error).getBoolean()));
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

      auto status = handler.execute();
      CHECK((arangodb::RestStatus::DONE == status));
      CHECK((arangodb::rest::ResponseCode::FORBIDDEN == responce.responseCode()));
      auto slice = responce._payload.slice();
      CHECK((slice.isObject()));
      CHECK((slice.hasKey(arangodb::StaticStrings::Code) && slice.get(arangodb::StaticStrings::Code).isNumber<size_t>() && size_t(arangodb::rest::ResponseCode::FORBIDDEN) == slice.get(arangodb::StaticStrings::Code).getNumber<size_t>()));
      CHECK((slice.hasKey(arangodb::StaticStrings::Error) && slice.get(arangodb::StaticStrings::Error).isBoolean() && true == slice.get(arangodb::StaticStrings::Error).getBoolean()));
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

      auto status = handler.execute();
      CHECK((arangodb::RestStatus::DONE == status));
      CHECK((arangodb::rest::ResponseCode::FORBIDDEN == responce.responseCode()));
      auto slice = responce._payload.slice();
      CHECK((slice.isObject()));
      CHECK((slice.hasKey(arangodb::StaticStrings::Code) && slice.get(arangodb::StaticStrings::Code).isNumber<size_t>() && size_t(arangodb::rest::ResponseCode::FORBIDDEN) == slice.get(arangodb::StaticStrings::Code).getNumber<size_t>()));
      CHECK((slice.hasKey(arangodb::StaticStrings::Error) && slice.get(arangodb::StaticStrings::Error).isBoolean() && true == slice.get(arangodb::StaticStrings::Error).getBoolean()));
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

      auto status = handler.execute();
      CHECK((arangodb::RestStatus::DONE == status));
      CHECK((arangodb::rest::ResponseCode::OK == responce.responseCode()));
      auto slice = responce._payload.slice();
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
    auto requestPtr = std::make_unique<GeneralRequestMock>(vocbase);
    auto& request = *requestPtr;
    auto responcePtr = std::make_unique<GeneralResponseMock>();
    auto& responce = *responcePtr;
    arangodb::RestViewHandler handler(requestPtr.release(), responcePtr.release());

    request.addSuffix("testView");
    request.setRequestType(arangodb::rest::RequestType::GET);

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

      auto status = handler.execute();
      CHECK((arangodb::RestStatus::DONE == status));
      CHECK((arangodb::rest::ResponseCode::FORBIDDEN == responce.responseCode()));
      auto slice = responce._payload.slice();
      CHECK((slice.isObject()));
      CHECK((slice.hasKey(arangodb::StaticStrings::Code) && slice.get(arangodb::StaticStrings::Code).isNumber<size_t>() && size_t(arangodb::rest::ResponseCode::FORBIDDEN) == slice.get(arangodb::StaticStrings::Code).getNumber<size_t>()));
      CHECK((slice.hasKey(arangodb::StaticStrings::Error) && slice.get(arangodb::StaticStrings::Error).isBoolean() && true == slice.get(arangodb::StaticStrings::Error).getBoolean()));
      CHECK((slice.hasKey(arangodb::StaticStrings::ErrorNum) && slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() && TRI_ERROR_FORBIDDEN == slice.get(arangodb::StaticStrings::ErrorNum).getNumber<int>()));
    }

    // not authorized (NONE view)
    {
      arangodb::auth::UserMap userMap;
      auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
      user.grantDatabase(vocbase.name(), arangodb::auth::Level::RO);
      user.grantCollection(vocbase.name(), "testView", arangodb::auth::Level::NONE); // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

      auto status = handler.execute();
      CHECK((arangodb::RestStatus::DONE == status));
      CHECK((arangodb::rest::ResponseCode::FORBIDDEN == responce.responseCode()));
      auto slice = responce._payload.slice();
      CHECK((slice.isObject()));
      CHECK((slice.hasKey(arangodb::StaticStrings::Code) && slice.get(arangodb::StaticStrings::Code).isNumber<size_t>() && size_t(arangodb::rest::ResponseCode::FORBIDDEN) == slice.get(arangodb::StaticStrings::Code).getNumber<size_t>()));
      CHECK((slice.hasKey(arangodb::StaticStrings::Error) && slice.get(arangodb::StaticStrings::Error).isBoolean() && true == slice.get(arangodb::StaticStrings::Error).getBoolean()));
      CHECK((slice.hasKey(arangodb::StaticStrings::ErrorNum) && slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() && TRI_ERROR_FORBIDDEN == slice.get(arangodb::StaticStrings::ErrorNum).getNumber<int>()));
    }

    // authorzed (RO view)
    {
      arangodb::auth::UserMap userMap;
      auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
      user.grantDatabase(vocbase.name(), arangodb::auth::Level::RO);
      user.grantCollection(vocbase.name(), "testView", arangodb::auth::Level::RO);
      userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

      auto status = handler.execute();
      CHECK((arangodb::RestStatus::DONE == status));
      CHECK((arangodb::rest::ResponseCode::OK == responce.responseCode()));
      auto slice = responce._payload.slice();
      CHECK((slice.isObject()));
      CHECK((slice.hasKey(arangodb::StaticStrings::DataSourceName) && slice.get(arangodb::StaticStrings::DataSourceName).isString() && std::string("testView") == slice.get(arangodb::StaticStrings::DataSourceName).copyString()));
    }
  }

  // test get view (detailed)
  {
    auto createViewJson = arangodb::velocypack::Parser::fromJson("{ \"name\": \"testView\", \"type\": \"testViewType\" }");
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto logicalView = vocbase.createView(createViewJson->slice());
    REQUIRE((false == !logicalView));
    auto requestPtr = std::make_unique<GeneralRequestMock>(vocbase);
    auto& request = *requestPtr;
    auto responcePtr = std::make_unique<GeneralResponseMock>();
    auto& responce = *responcePtr;
    arangodb::RestViewHandler handler(requestPtr.release(), responcePtr.release());

    request.addSuffix("testView");
    request.addSuffix("properties");
    request.setRequestType(arangodb::rest::RequestType::GET);

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

      auto status = handler.execute();
      CHECK((arangodb::RestStatus::DONE == status));
      CHECK((arangodb::rest::ResponseCode::FORBIDDEN == responce.responseCode()));
      auto slice = responce._payload.slice();
      CHECK((slice.isObject()));
      CHECK((slice.hasKey(arangodb::StaticStrings::Code) && slice.get(arangodb::StaticStrings::Code).isNumber<size_t>() && size_t(arangodb::rest::ResponseCode::FORBIDDEN) == slice.get(arangodb::StaticStrings::Code).getNumber<size_t>()));
      CHECK((slice.hasKey(arangodb::StaticStrings::Error) && slice.get(arangodb::StaticStrings::Error).isBoolean() && true == slice.get(arangodb::StaticStrings::Error).getBoolean()));
      CHECK((slice.hasKey(arangodb::StaticStrings::ErrorNum) && slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() && TRI_ERROR_FORBIDDEN == slice.get(arangodb::StaticStrings::ErrorNum).getNumber<int>()));
    }

    // not authorized (NONE view)
    {
      arangodb::auth::UserMap userMap;
      auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
      user.grantDatabase(vocbase.name(), arangodb::auth::Level::RO);
      user.grantCollection(vocbase.name(), "testView", arangodb::auth::Level::NONE); // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

      auto status = handler.execute();
      CHECK((arangodb::RestStatus::DONE == status));
      CHECK((arangodb::rest::ResponseCode::FORBIDDEN == responce.responseCode()));
      auto slice = responce._payload.slice();
      CHECK((slice.isObject()));
      CHECK((slice.hasKey(arangodb::StaticStrings::Code) && slice.get(arangodb::StaticStrings::Code).isNumber<size_t>() && size_t(arangodb::rest::ResponseCode::FORBIDDEN) == slice.get(arangodb::StaticStrings::Code).getNumber<size_t>()));
      CHECK((slice.hasKey(arangodb::StaticStrings::Error) && slice.get(arangodb::StaticStrings::Error).isBoolean() && true == slice.get(arangodb::StaticStrings::Error).getBoolean()));
      CHECK((slice.hasKey(arangodb::StaticStrings::ErrorNum) && slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() && TRI_ERROR_FORBIDDEN == slice.get(arangodb::StaticStrings::ErrorNum).getNumber<int>()));
    }

    // authorzed (RO view)
    {
      arangodb::auth::UserMap userMap;
      auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
      user.grantDatabase(vocbase.name(), arangodb::auth::Level::RO);
      user.grantCollection(vocbase.name(), "testView", arangodb::auth::Level::RO);
      userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

      auto status = handler.execute();
      CHECK((arangodb::RestStatus::DONE == status));
      CHECK((arangodb::rest::ResponseCode::OK == responce.responseCode()));
      auto slice = responce._payload.slice();
      CHECK((slice.isObject()));
      CHECK((slice.hasKey(arangodb::StaticStrings::DataSourceName) && slice.get(arangodb::StaticStrings::DataSourceName).isString() && std::string("testView") == slice.get(arangodb::StaticStrings::DataSourceName).copyString()));
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
    auto requestPtr = std::make_unique<GeneralRequestMock>(vocbase);
    auto& request = *requestPtr;
    auto responcePtr = std::make_unique<GeneralResponseMock>();
    auto& responce = *responcePtr;
    arangodb::RestViewHandler handler(requestPtr.release(), responcePtr.release());

    request.setRequestType(arangodb::rest::RequestType::GET);

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

      auto status = handler.execute();
      CHECK((arangodb::RestStatus::DONE == status));
      CHECK((arangodb::rest::ResponseCode::FORBIDDEN == responce.responseCode()));
      auto slice = responce._payload.slice();
      CHECK((slice.isObject()));
      CHECK((slice.hasKey(arangodb::StaticStrings::Code) && slice.get(arangodb::StaticStrings::Code).isNumber<size_t>() && size_t(arangodb::rest::ResponseCode::FORBIDDEN) == slice.get(arangodb::StaticStrings::Code).getNumber<size_t>()));
      CHECK((slice.hasKey(arangodb::StaticStrings::Error) && slice.get(arangodb::StaticStrings::Error).isBoolean() && true == slice.get(arangodb::StaticStrings::Error).getBoolean()));
      CHECK((slice.hasKey(arangodb::StaticStrings::ErrorNum) && slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() && TRI_ERROR_FORBIDDEN == slice.get(arangodb::StaticStrings::ErrorNum).getNumber<int>()));
    }

    // not authorized (NONE view)
    {
      arangodb::auth::UserMap userMap;
      auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
      user.grantDatabase(vocbase.name(), arangodb::auth::Level::RO);
      user.grantCollection(vocbase.name(), "testView1", arangodb::auth::Level::NONE);
      user.grantCollection(vocbase.name(), "testView2", arangodb::auth::Level::NONE); // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

      auto status = handler.execute();
      CHECK((arangodb::RestStatus::DONE == status));
      CHECK((arangodb::rest::ResponseCode::OK == responce.responseCode()));
      auto slice = responce._payload.slice();
      CHECK((slice.isObject()));
      CHECK((slice.hasKey(arangodb::StaticStrings::Code) && slice.get(arangodb::StaticStrings::Code).isNumber<size_t>() && size_t(arangodb::rest::ResponseCode::OK) == slice.get(arangodb::StaticStrings::Code).getNumber<size_t>()));
      CHECK((slice.hasKey(arangodb::StaticStrings::Error) && slice.get(arangodb::StaticStrings::Error).isBoolean() && false == slice.get(arangodb::StaticStrings::Error).getBoolean()));
      CHECK((slice.hasKey("result")));
      slice = slice.get("result");
      CHECK((slice.isArray()));
      CHECK((0U == slice.length()));
    }

    // authorzed (RO view)
    {
      arangodb::auth::UserMap userMap;
      auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
      user.grantDatabase(vocbase.name(), arangodb::auth::Level::RO);
      user.grantCollection(vocbase.name(), "testView1", arangodb::auth::Level::RO);
      user.grantCollection(vocbase.name(), "testView2", arangodb::auth::Level::NONE); // for missing collections User::collectionAuthLevel(...) returns database auth::Level
      userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

      auto status = handler.execute();
      CHECK((arangodb::RestStatus::DONE == status));
      CHECK((arangodb::rest::ResponseCode::OK == responce.responseCode()));
      auto slice = responce._payload.slice();
      CHECK((slice.isObject()));
      CHECK((slice.hasKey(arangodb::StaticStrings::Code) && slice.get(arangodb::StaticStrings::Code).isNumber<size_t>() && size_t(arangodb::rest::ResponseCode::OK) == slice.get(arangodb::StaticStrings::Code).getNumber<size_t>()));
      CHECK((slice.hasKey(arangodb::StaticStrings::Error) && slice.get(arangodb::StaticStrings::Error).isBoolean() && false == slice.get(arangodb::StaticStrings::Error).getBoolean()));
      CHECK((slice.hasKey("result")));
      slice = slice.get("result");
      CHECK((slice.isArray()));
      CHECK((1U == slice.length()));
      slice = slice.at(0);
      CHECK((slice.isObject()));
      CHECK((slice.hasKey(arangodb::StaticStrings::DataSourceName) && slice.get(arangodb::StaticStrings::DataSourceName).isString() && std::string("testView1") == slice.get(arangodb::StaticStrings::DataSourceName).copyString()));
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