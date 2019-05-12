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

#include "catch.hpp"
#include "../IResearch/RestHandlerMock.h"
#include "../Mocks/StorageEngineMock.h"
#include "Aql/QueryRegistry.h"

#if USE_ENTERPRISE
  #include "Enterprise/Ldap/LdapFeature.h"
#endif

#include "GeneralServer/AuthenticationFeature.h"
#include "IResearch/IResearchAnalyzerFeature.h"
#include "IResearch/IResearchCommon.h"
#include "RestHandler/RestAnalyzerHandler.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "RestServer/SystemDatabaseFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "V8Server/V8DealerFeature.h"
#include "velocypack/Iterator.h"
#include "velocypack/Parser.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

struct RestAnalyzerHandlerSetup {
  StorageEngineMock engine;
  arangodb::application_features::ApplicationServer server;
  std::vector<std::pair<arangodb::application_features::ApplicationFeature*, bool>> features;

  RestAnalyzerHandlerSetup(): engine(server), server(nullptr, nullptr) {
    arangodb::EngineSelectorFeature::ENGINE = &engine;

    // suppress INFO {authentication} Authentication is turned on (system only), authentication for unix sockets is turned on
    // suppress WARNING {authentication} --server.jwt-secret is insecure. Use --server.jwt-secret-keyfile instead
    arangodb::LogTopic::setLogLevel(arangodb::Logger::AUTHENTICATION.name(), arangodb::LogLevel::ERR);

    // suppress log messages since tests check error conditions
    arangodb::LogTopic::setLogLevel(arangodb::iresearch::TOPIC.name(), arangodb::LogLevel::FATAL);

    // setup required application features
    features.emplace_back(new arangodb::AuthenticationFeature(server), false); // required for VocbaseContext

    #if USE_ENTERPRISE
      features.emplace_back(new arangodb::LdapFeature(server), false); // required for AuthenticationFeature with USE_ENTERPRISE
    #endif

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
  }

  ~RestAnalyzerHandlerSetup() {
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

    arangodb::LogTopic::setLogLevel(arangodb::iresearch::TOPIC.name(), arangodb::LogLevel::DEFAULT);
    arangodb::LogTopic::setLogLevel(arangodb::Logger::AUTHENTICATION.name(), arangodb::LogLevel::DEFAULT);
    arangodb::EngineSelectorFeature::ENGINE = nullptr;
  }
};

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief setup
////////////////////////////////////////////////////////////////////////////////

TEST_CASE("RestAnalyzerHandlerTest", "[iresearch][rest]") {
  RestAnalyzerHandlerSetup s;
  (void)(s);

SECTION("test_create") {
  // create a new instance of an ApplicationServer and fill it with the required features
  // cannot use the existing server since its features already have some state
  std::shared_ptr<arangodb::application_features::ApplicationServer> originalServer(
    arangodb::application_features::ApplicationServer::server,
    [](arangodb::application_features::ApplicationServer* ptr)->void {
      arangodb::application_features::ApplicationServer::server = ptr;
    }
  );
  arangodb::application_features::ApplicationServer::server = nullptr; // avoid "ApplicationServer initialized twice"
  arangodb::application_features::ApplicationServer server(nullptr, nullptr);
  arangodb::iresearch::IResearchAnalyzerFeature* analyzers;
  arangodb::DatabaseFeature* dbFeature;
  server.addFeature(new arangodb::QueryRegistryFeature(server)); // required for constructing TRI_vocbase_t
  server.addFeature(new arangodb::V8DealerFeature(server)); // required for DatabaseFeature::createDatabase(...)
  server.addFeature(dbFeature = new arangodb::DatabaseFeature(server)); // required for IResearchAnalyzerFeature::emplace(...)
  server.addFeature(analyzers = new arangodb::iresearch::IResearchAnalyzerFeature(server)); // required for running upgrade task

  // create system vocbase
  {
    auto const databases = arangodb::velocypack::Parser::fromJson(std::string("[ { \"name\": \"") + arangodb::StaticStrings::SystemDatabase + "\" } ]");
    REQUIRE((TRI_ERROR_NO_ERROR == dbFeature->loadDatabases(databases->slice())));
  }

  arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;
  REQUIRE((analyzers->emplace(result, arangodb::StaticStrings::SystemDatabase + "::testAnalyzer1", "identity", nullptr).ok()));

  struct ExecContext: public arangodb::ExecContext {
    ExecContext(): arangodb::ExecContext(arangodb::ExecContext::Type::Default, "", "",
                                         arangodb::auth::Level::NONE, arangodb::auth::Level::NONE) {}
  } execContext;
  arangodb::ExecContextScope execContextScope(&execContext);
  auto* authFeature = arangodb::AuthenticationFeature::instance();
  auto* userManager = authFeature->userManager();
  arangodb::aql::QueryRegistry queryRegistry(0); // required for UserManager::loadFromDB()
  userManager->setQueryRegistry(&queryRegistry);
  auto resetUserManager = std::shared_ptr<arangodb::auth::UserManager>(userManager, [](arangodb::auth::UserManager* ptr)->void { ptr->removeAllUsers(); });

  // invalid params (non-object body)
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, arangodb::StaticStrings::SystemDatabase);
    auto requestPtr = std::make_unique<GeneralRequestMock>(vocbase);
    auto& request = *requestPtr;
    auto responcePtr = std::make_unique<GeneralResponseMock>();
    auto& responce = *responcePtr;
    arangodb::iresearch::RestAnalyzerHandler handler(requestPtr.release(), responcePtr.release());
    request.setRequestType(arangodb::rest::RequestType::POST);
    request._payload.openArray();
    request._payload.close();

    arangodb::auth::UserMap userMap; // empty map, no user -> no permissions
    auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
    user.grantDatabase(vocbase.name(), arangodb::auth::Level::RW); // for system collections User::collectionAuthLevel(...) returns database auth::Level
    userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

    auto status = handler.execute();
    CHECK((arangodb::RestStatus::DONE == status));
    CHECK((arangodb::rest::ResponseCode::BAD == responce.responseCode()));
    auto slice = responce._payload.slice();
    CHECK((slice.isObject()));
    CHECK((slice.hasKey(arangodb::StaticStrings::Code) && slice.get(arangodb::StaticStrings::Code).isNumber<size_t>() && size_t(arangodb::rest::ResponseCode::BAD) == slice.get(arangodb::StaticStrings::Code).getNumber<size_t>()));
    CHECK((slice.hasKey(arangodb::StaticStrings::Error) && slice.get(arangodb::StaticStrings::Error).isBoolean() && true == slice.get(arangodb::StaticStrings::Error).getBoolean()));
    CHECK((slice.hasKey(arangodb::StaticStrings::ErrorNum) && slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() && TRI_ERROR_BAD_PARAMETER == slice.get(arangodb::StaticStrings::ErrorNum).getNumber<int>()));
  }

  // invalid params (no name)
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, arangodb::StaticStrings::SystemDatabase);
    auto requestPtr = std::make_unique<GeneralRequestMock>(vocbase);
    auto& request = *requestPtr;
    auto responcePtr = std::make_unique<GeneralResponseMock>();
    auto& responce = *responcePtr;
    arangodb::iresearch::RestAnalyzerHandler handler(requestPtr.release(), responcePtr.release());
    request.setRequestType(arangodb::rest::RequestType::POST);
    request._payload.openObject();
    request._payload.add("type", arangodb::velocypack::Value("identity"));
    request._payload.add("properties", arangodb::velocypack::Value(arangodb::velocypack::ValueType::Null));
    request._payload.close();

    arangodb::auth::UserMap userMap; // empty map, no user -> no permissions
    auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
    user.grantDatabase(vocbase.name(), arangodb::auth::Level::RW); // for system collections User::collectionAuthLevel(...) returns database auth::Level
    userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

    auto status = handler.execute();
    CHECK((arangodb::RestStatus::DONE == status));
    CHECK((arangodb::rest::ResponseCode::BAD == responce.responseCode()));
    auto slice = responce._payload.slice();
    CHECK((slice.isObject()));
    CHECK((slice.hasKey(arangodb::StaticStrings::Code) && slice.get(arangodb::StaticStrings::Code).isNumber<size_t>() && size_t(arangodb::rest::ResponseCode::BAD) == slice.get(arangodb::StaticStrings::Code).getNumber<size_t>()));
    CHECK((slice.hasKey(arangodb::StaticStrings::Error) && slice.get(arangodb::StaticStrings::Error).isBoolean() && true == slice.get(arangodb::StaticStrings::Error).getBoolean()));
    CHECK((slice.hasKey(arangodb::StaticStrings::ErrorNum) && slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() && TRI_ERROR_BAD_PARAMETER == slice.get(arangodb::StaticStrings::ErrorNum).getNumber<int>()));
  }

  // invalid params (missing database)
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, arangodb::StaticStrings::SystemDatabase);
    auto requestPtr = std::make_unique<GeneralRequestMock>(vocbase);
    auto& request = *requestPtr;
    auto responcePtr = std::make_unique<GeneralResponseMock>();
    auto& responce = *responcePtr;
    arangodb::iresearch::RestAnalyzerHandler handler(requestPtr.release(), responcePtr.release());
    request.setRequestType(arangodb::rest::RequestType::POST);
    request._payload.openObject();
    request._payload.add("name", arangodb::velocypack::Value("unknownVocbase::testAnalyzer"));
    request._payload.add("type", arangodb::velocypack::Value("identity"));
    request._payload.add("properties", arangodb::velocypack::Value(arangodb::velocypack::ValueType::Null));
    request._payload.close();

    arangodb::auth::UserMap userMap; // empty map, no user -> no permissions
    auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
    user.grantDatabase(vocbase.name(), arangodb::auth::Level::RW); // for system collections User::collectionAuthLevel(...) returns database auth::Level
    userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

    auto status = handler.execute();
    CHECK((arangodb::RestStatus::DONE == status));
    CHECK((arangodb::rest::ResponseCode::NOT_FOUND == responce.responseCode()));
    auto slice = responce._payload.slice();
    CHECK((slice.isObject()));
    CHECK((slice.hasKey(arangodb::StaticStrings::Code) && slice.get(arangodb::StaticStrings::Code).isNumber<size_t>() && size_t(arangodb::rest::ResponseCode::NOT_FOUND) == slice.get(arangodb::StaticStrings::Code).getNumber<size_t>()));
    CHECK((slice.hasKey(arangodb::StaticStrings::Error) && slice.get(arangodb::StaticStrings::Error).isBoolean() && true == slice.get(arangodb::StaticStrings::Error).getBoolean()));
    CHECK((slice.hasKey(arangodb::StaticStrings::ErrorNum) && slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() && TRI_ERROR_ARANGO_DATABASE_NOT_FOUND == slice.get(arangodb::StaticStrings::ErrorNum).getNumber<int>()));
  }

  // name collision
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, arangodb::StaticStrings::SystemDatabase);
    auto requestPtr = std::make_unique<GeneralRequestMock>(vocbase);
    auto& request = *requestPtr;
    auto responcePtr = std::make_unique<GeneralResponseMock>();
    auto& responce = *responcePtr;
    arangodb::iresearch::RestAnalyzerHandler handler(requestPtr.release(), responcePtr.release());
    request.setRequestType(arangodb::rest::RequestType::POST);
    request._payload.openObject();
    request._payload.add("name", arangodb::velocypack::Value(arangodb::StaticStrings::SystemDatabase + "::testAnalyzer1"));
    request._payload.add("type", arangodb::velocypack::Value("identity"));
    request._payload.add("properties", arangodb::velocypack::Value("abc"));
    request._payload.close();

    arangodb::auth::UserMap userMap; // empty map, no user -> no permissions
    auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
    user.grantDatabase(vocbase.name(), arangodb::auth::Level::RW); // for system collections User::collectionAuthLevel(...) returns database auth::Level
    userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

    auto status = handler.execute();
    CHECK((arangodb::RestStatus::DONE == status));
    CHECK((arangodb::rest::ResponseCode::BAD == responce.responseCode()));
    auto slice = responce._payload.slice();
    CHECK((slice.isObject()));
    CHECK((slice.hasKey(arangodb::StaticStrings::Code) && slice.get(arangodb::StaticStrings::Code).isNumber<size_t>() && size_t(arangodb::rest::ResponseCode::BAD) == slice.get(arangodb::StaticStrings::Code).getNumber<size_t>()));
    CHECK((slice.hasKey(arangodb::StaticStrings::Error) && slice.get(arangodb::StaticStrings::Error).isBoolean() && true == slice.get(arangodb::StaticStrings::Error).getBoolean()));
    CHECK((slice.hasKey(arangodb::StaticStrings::ErrorNum) && slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() && TRI_ERROR_BAD_PARAMETER == slice.get(arangodb::StaticStrings::ErrorNum).getNumber<int>()));
  }

  // duplicate matching
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, arangodb::StaticStrings::SystemDatabase);
    auto requestPtr = std::make_unique<GeneralRequestMock>(vocbase);
    auto& request = *requestPtr;
    auto responcePtr = std::make_unique<GeneralResponseMock>();
    auto& responce = *responcePtr;
    arangodb::iresearch::RestAnalyzerHandler handler(requestPtr.release(), responcePtr.release());
    request.setRequestType(arangodb::rest::RequestType::POST);
    request._payload.openObject();
    request._payload.add("name", arangodb::velocypack::Value(arangodb::StaticStrings::SystemDatabase + "::testAnalyzer1"));
    request._payload.add("type", arangodb::velocypack::Value("identity"));
    request._payload.add("properties", arangodb::velocypack::Value(arangodb::velocypack::ValueType::Null));
    request._payload.close();

    arangodb::auth::UserMap userMap; // empty map, no user -> no permissions
    auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
    user.grantDatabase(vocbase.name(), arangodb::auth::Level::RW); // for system collections User::collectionAuthLevel(...) returns database auth::Level
    userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

    auto status = handler.execute();
    CHECK((arangodb::RestStatus::DONE == status));
    CHECK((arangodb::rest::ResponseCode::OK == responce.responseCode()));
    auto slice = responce._payload.slice();
    CHECK((slice.isObject()));
    CHECK((slice.hasKey("name") && slice.get("name").isString() && arangodb::StaticStrings::SystemDatabase + "::testAnalyzer1" == slice.get("name").copyString()));
    CHECK((slice.hasKey("type") && slice.get("type").isString()));
    CHECK((slice.hasKey("properties") && (slice.get("properties").isString() || slice.get("properties").isNull())));
    CHECK((slice.hasKey("features") && slice.get("features").isArray()));
    auto analyzer = analyzers->get(arangodb::StaticStrings::SystemDatabase + "::testAnalyzer1");
    CHECK((false == !analyzer));
  }

  // not authorised
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, arangodb::StaticStrings::SystemDatabase);
    auto requestPtr = std::make_unique<GeneralRequestMock>(vocbase);
    auto& request = *requestPtr;
    auto responcePtr = std::make_unique<GeneralResponseMock>();
    auto& responce = *responcePtr;
    arangodb::iresearch::RestAnalyzerHandler handler(requestPtr.release(), responcePtr.release());
    request.setRequestType(arangodb::rest::RequestType::POST);
    request._payload.openObject();
    request._payload.add("name", arangodb::velocypack::Value(arangodb::StaticStrings::SystemDatabase + "::testAnalyzer2"));
    request._payload.add("type", arangodb::velocypack::Value("identity"));
    request._payload.add("properties", arangodb::velocypack::Value("abc"));
    request._payload.close();

    arangodb::auth::UserMap userMap; // empty map, no user -> no permissions
    auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
    user.grantDatabase(vocbase.name(), arangodb::auth::Level::RO); // for system collections User::collectionAuthLevel(...) returns database auth::Level
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

  // successful creation
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, arangodb::StaticStrings::SystemDatabase);
    auto requestPtr = std::make_unique<GeneralRequestMock>(vocbase);
    auto& request = *requestPtr;
    auto responcePtr = std::make_unique<GeneralResponseMock>();
    auto& responce = *responcePtr;
    arangodb::iresearch::RestAnalyzerHandler handler(requestPtr.release(), responcePtr.release());
    request.setRequestType(arangodb::rest::RequestType::POST);
    request._payload.openObject();
    request._payload.add("name", arangodb::velocypack::Value(arangodb::StaticStrings::SystemDatabase + "::testAnalyzer2"));
    request._payload.add("type", arangodb::velocypack::Value("identity"));
    request._payload.add("properties", arangodb::velocypack::Value("abc"));
    request._payload.close();

    arangodb::auth::UserMap userMap; // empty map, no user -> no permissions
    auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
    user.grantDatabase(vocbase.name(), arangodb::auth::Level::RW); // for system collections User::collectionAuthLevel(...) returns database auth::Level
    userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

    auto status = handler.execute();
    CHECK((arangodb::RestStatus::DONE == status));
    CHECK((arangodb::rest::ResponseCode::CREATED == responce.responseCode()));
    auto slice = responce._payload.slice();
    CHECK((slice.isObject()));
    CHECK((slice.hasKey("name") && slice.get("name").isString() && arangodb::StaticStrings::SystemDatabase + "::testAnalyzer2" == slice.get("name").copyString()));
    CHECK((slice.hasKey("type") && slice.get("type").isString()));
    CHECK((slice.hasKey("properties") && (slice.get("properties").isString() || slice.get("properties").isNull())));
    CHECK((slice.hasKey("features") && slice.get("features").isArray()));
    auto analyzer = analyzers->get(arangodb::StaticStrings::SystemDatabase + "::testAnalyzer1");
    CHECK((false == !analyzer));
  }
}

SECTION("test_get") {
  // create a new instance of an ApplicationServer and fill it with the required features
  // cannot use the existing server since its features already have some state
  std::shared_ptr<arangodb::application_features::ApplicationServer> originalServer(
    arangodb::application_features::ApplicationServer::server,
    [](arangodb::application_features::ApplicationServer* ptr)->void {
      arangodb::application_features::ApplicationServer::server = ptr;
    }
  );
  arangodb::application_features::ApplicationServer::server = nullptr; // avoid "ApplicationServer initialized twice"
  arangodb::application_features::ApplicationServer server(nullptr, nullptr);
  arangodb::iresearch::IResearchAnalyzerFeature* analyzers;
  arangodb::DatabaseFeature* dbFeature;
  server.addFeature(new arangodb::QueryRegistryFeature(server)); // required for constructing TRI_vocbase_t
  server.addFeature(new arangodb::V8DealerFeature(server)); // required for DatabaseFeature::createDatabase(...)
  server.addFeature(dbFeature = new arangodb::DatabaseFeature(server)); // required for IResearchAnalyzerFeature::emplace(...)
  server.addFeature(analyzers = new arangodb::iresearch::IResearchAnalyzerFeature(server)); // required for running upgrade task

  // create system vocbase
  {
    auto const databases = arangodb::velocypack::Parser::fromJson(std::string("[ { \"name\": \"") + arangodb::StaticStrings::SystemDatabase + "\" } ]");
    REQUIRE((TRI_ERROR_NO_ERROR == dbFeature->loadDatabases(databases->slice())));
  }

  arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;
  REQUIRE((analyzers->emplace(result, arangodb::StaticStrings::SystemDatabase + "::testAnalyzer1", "identity", nullptr).ok()));

  struct ExecContext: public arangodb::ExecContext {
    ExecContext(): arangodb::ExecContext(arangodb::ExecContext::Type::Default, "", "",
                                         arangodb::auth::Level::NONE, arangodb::auth::Level::NONE) {}
  } execContext;
  arangodb::ExecContextScope execContextScope(&execContext);
  auto* authFeature = arangodb::AuthenticationFeature::instance();
  auto* userManager = authFeature->userManager();
  arangodb::aql::QueryRegistry queryRegistry(0); // required for UserManager::loadFromDB()
  userManager->setQueryRegistry(&queryRegistry);
  auto resetUserManager = std::shared_ptr<arangodb::auth::UserManager>(userManager, [](arangodb::auth::UserManager* ptr)->void { ptr->removeAllUsers(); });

  // get static (known analyzer)
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, arangodb::StaticStrings::SystemDatabase);
    auto requestPtr = std::make_unique<GeneralRequestMock>(vocbase);
    auto& request = *requestPtr;
    auto responcePtr = std::make_unique<GeneralResponseMock>();
    auto& responce = *responcePtr;
    arangodb::iresearch::RestAnalyzerHandler handler(requestPtr.release(), responcePtr.release());
    request.setRequestType(arangodb::rest::RequestType::GET);
    request.addSuffix("identity");

    arangodb::auth::UserMap userMap; // empty map, no user -> no permissions
    auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
    user.grantDatabase(vocbase.name(), arangodb::auth::Level::NONE); // for system collections User::collectionAuthLevel(...) returns database auth::Level
    userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

    auto status = handler.execute();
    CHECK((arangodb::RestStatus::DONE == status));
    CHECK((arangodb::rest::ResponseCode::OK == responce.responseCode()));
    auto slice = responce._payload.slice();
    CHECK((slice.isObject()));
    CHECK((slice.hasKey(arangodb::StaticStrings::Code) && slice.get(arangodb::StaticStrings::Code).isNumber<size_t>() && size_t(arangodb::rest::ResponseCode::OK) == slice.get(arangodb::StaticStrings::Code).getNumber<size_t>()));
    CHECK((slice.hasKey(arangodb::StaticStrings::Error) && slice.get(arangodb::StaticStrings::Error).isBoolean() && false == slice.get(arangodb::StaticStrings::Error).getBoolean()));
    CHECK((slice.hasKey("name") && slice.get("name").isString() && std::string("identity") == slice.get("name").copyString()));
    CHECK((slice.hasKey("type") && slice.get("type").isString()));
    CHECK((slice.hasKey("properties") && (slice.get("properties").isString() || slice.get("properties").isNull())));
    CHECK((slice.hasKey("features") && slice.get("features").isArray()));
  }

  // get static (unknown analyzer)
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, arangodb::StaticStrings::SystemDatabase);
    auto requestPtr = std::make_unique<GeneralRequestMock>(vocbase);
    auto& request = *requestPtr;
    auto responcePtr = std::make_unique<GeneralResponseMock>();
    auto& responce = *responcePtr;
    arangodb::iresearch::RestAnalyzerHandler handler(requestPtr.release(), responcePtr.release());
    request.setRequestType(arangodb::rest::RequestType::GET);
    request.addSuffix("unknown");

    arangodb::auth::UserMap userMap; // empty map, no user -> no permissions
    auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
    user.grantDatabase(vocbase.name(), arangodb::auth::Level::NONE); // for system collections User::collectionAuthLevel(...) returns database auth::Level
    userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

    auto status = handler.execute();
    CHECK((arangodb::RestStatus::DONE == status));
    CHECK((arangodb::rest::ResponseCode::NOT_FOUND == responce.responseCode()));
    auto slice = responce._payload.slice();
    CHECK((slice.isObject()));
    CHECK((slice.hasKey(arangodb::StaticStrings::Code) && slice.get(arangodb::StaticStrings::Code).isNumber<size_t>() && size_t(arangodb::rest::ResponseCode::NOT_FOUND) == slice.get(arangodb::StaticStrings::Code).getNumber<size_t>()));
    CHECK((slice.hasKey(arangodb::StaticStrings::Error) && slice.get(arangodb::StaticStrings::Error).isBoolean() && true == slice.get(arangodb::StaticStrings::Error).getBoolean()));
    CHECK((slice.hasKey(arangodb::StaticStrings::ErrorNum) && slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() && TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND == slice.get(arangodb::StaticStrings::ErrorNum).getNumber<int>()));
  }

  // get custom (known analyzer) authorized
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, arangodb::StaticStrings::SystemDatabase);
    auto requestPtr = std::make_unique<GeneralRequestMock>(vocbase);
    auto& request = *requestPtr;
    auto responcePtr = std::make_unique<GeneralResponseMock>();
    auto& responce = *responcePtr;
    arangodb::iresearch::RestAnalyzerHandler handler(requestPtr.release(), responcePtr.release());
    request.setRequestType(arangodb::rest::RequestType::GET);
    request.addSuffix(arangodb::StaticStrings::SystemDatabase + "::testAnalyzer1");

    arangodb::auth::UserMap userMap; // empty map, no user -> no permissions
    auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
    user.grantDatabase(vocbase.name(), arangodb::auth::Level::RO); // for system collections User::collectionAuthLevel(...) returns database auth::Level
    userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

    auto status = handler.execute();
    CHECK((arangodb::RestStatus::DONE == status));
    CHECK((arangodb::rest::ResponseCode::OK == responce.responseCode()));
    auto slice = responce._payload.slice();
    CHECK((slice.isObject()));
    CHECK((slice.hasKey(arangodb::StaticStrings::Code) && slice.get(arangodb::StaticStrings::Code).isNumber<size_t>() && size_t(arangodb::rest::ResponseCode::OK) == slice.get(arangodb::StaticStrings::Code).getNumber<size_t>()));
    CHECK((slice.hasKey(arangodb::StaticStrings::Error) && slice.get(arangodb::StaticStrings::Error).isBoolean() && false == slice.get(arangodb::StaticStrings::Error).getBoolean()));
    CHECK((slice.hasKey("name") && slice.get("name").isString() && arangodb::StaticStrings::SystemDatabase + "::testAnalyzer1" == slice.get("name").copyString()));
    CHECK((slice.hasKey("type") && slice.get("type").isString()));
    CHECK((slice.hasKey("properties") && (slice.get("properties").isString() || slice.get("properties").isNull())));
    CHECK((slice.hasKey("features") && slice.get("features").isArray()));
  }

  // get custom (known analyzer) not authorized
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, arangodb::StaticStrings::SystemDatabase);
    auto requestPtr = std::make_unique<GeneralRequestMock>(vocbase);
    auto& request = *requestPtr;
    auto responcePtr = std::make_unique<GeneralResponseMock>();
    auto& responce = *responcePtr;
    arangodb::iresearch::RestAnalyzerHandler handler(requestPtr.release(), responcePtr.release());
    request.setRequestType(arangodb::rest::RequestType::GET);
    request.addSuffix(arangodb::StaticStrings::SystemDatabase + "::testAnalyzer1");

    arangodb::auth::UserMap userMap; // empty map, no user -> no permissions
    auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
    user.grantDatabase(vocbase.name(), arangodb::auth::Level::NONE); // for system collections User::collectionAuthLevel(...) returns database auth::Level
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

  // get custom (unknown analyzer) authorized
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, arangodb::StaticStrings::SystemDatabase);
    auto requestPtr = std::make_unique<GeneralRequestMock>(vocbase);
    auto& request = *requestPtr;
    auto responcePtr = std::make_unique<GeneralResponseMock>();
    auto& responce = *responcePtr;
    arangodb::iresearch::RestAnalyzerHandler handler(requestPtr.release(), responcePtr.release());
    request.setRequestType(arangodb::rest::RequestType::GET);
    request.addSuffix(arangodb::StaticStrings::SystemDatabase + "::unknown");

    arangodb::auth::UserMap userMap; // empty map, no user -> no permissions
    auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
    user.grantDatabase(vocbase.name(), arangodb::auth::Level::RO); // for system collections User::collectionAuthLevel(...) returns database auth::Level
    userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

    auto status = handler.execute();
    CHECK((arangodb::RestStatus::DONE == status));
    CHECK((arangodb::rest::ResponseCode::NOT_FOUND == responce.responseCode()));
    auto slice = responce._payload.slice();
    CHECK((slice.isObject()));
    CHECK((slice.hasKey(arangodb::StaticStrings::Code) && slice.get(arangodb::StaticStrings::Code).isNumber<size_t>() && size_t(arangodb::rest::ResponseCode::NOT_FOUND) == slice.get(arangodb::StaticStrings::Code).getNumber<size_t>()));
    CHECK((slice.hasKey(arangodb::StaticStrings::Error) && slice.get(arangodb::StaticStrings::Error).isBoolean() && true == slice.get(arangodb::StaticStrings::Error).getBoolean()));
    CHECK((slice.hasKey(arangodb::StaticStrings::ErrorNum) && slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() && TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND == slice.get(arangodb::StaticStrings::ErrorNum).getNumber<int>()));
  }

  // get custom (unknown analyzer) not authorized
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, arangodb::StaticStrings::SystemDatabase);
    auto requestPtr = std::make_unique<GeneralRequestMock>(vocbase);
    auto& request = *requestPtr;
    auto responcePtr = std::make_unique<GeneralResponseMock>();
    auto& responce = *responcePtr;
    arangodb::iresearch::RestAnalyzerHandler handler(requestPtr.release(), responcePtr.release());
    request.setRequestType(arangodb::rest::RequestType::GET);
    request.addSuffix(arangodb::StaticStrings::SystemDatabase + "::unknown");

    arangodb::auth::UserMap userMap; // empty map, no user -> no permissions
    auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
    user.grantDatabase(vocbase.name(), arangodb::auth::Level::NONE); // for system collections User::collectionAuthLevel(...) returns database auth::Level
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

  // get custom (unknown analyzer, unknown vocbase) authorized
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "unknownVocbase");
    auto requestPtr = std::make_unique<GeneralRequestMock>(vocbase);
    auto& request = *requestPtr;
    auto responcePtr = std::make_unique<GeneralResponseMock>();
    auto& responce = *responcePtr;
    arangodb::iresearch::RestAnalyzerHandler handler(requestPtr.release(), responcePtr.release());
    request.setRequestType(arangodb::rest::RequestType::GET);
    request.addSuffix("unknownVocbase::unknown");

    arangodb::auth::UserMap userMap; // empty map, no user -> no permissions
    auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
    user.grantDatabase("unknownVocbase", arangodb::auth::Level::RO); // for system collections User::collectionAuthLevel(...) returns database auth::Level
    userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

    auto status = handler.execute();
    CHECK((arangodb::RestStatus::DONE == status));
    CHECK((arangodb::rest::ResponseCode::NOT_FOUND == responce.responseCode()));
    auto slice = responce._payload.slice();
    CHECK((slice.isObject()));
    CHECK((slice.hasKey(arangodb::StaticStrings::Code) && slice.get(arangodb::StaticStrings::Code).isNumber<size_t>() && size_t(arangodb::rest::ResponseCode::NOT_FOUND) == slice.get(arangodb::StaticStrings::Code).getNumber<size_t>()));
    CHECK((slice.hasKey(arangodb::StaticStrings::Error) && slice.get(arangodb::StaticStrings::Error).isBoolean() && true == slice.get(arangodb::StaticStrings::Error).getBoolean()));
    CHECK((slice.hasKey(arangodb::StaticStrings::ErrorNum) && slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() && TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND == slice.get(arangodb::StaticStrings::ErrorNum).getNumber<int>()));
  }

  // get custom (unknown analyzer, unknown vocbase) not authorized
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "unknownVocbase");
    auto requestPtr = std::make_unique<GeneralRequestMock>(vocbase);
    auto& request = *requestPtr;
    auto responcePtr = std::make_unique<GeneralResponseMock>();
    auto& responce = *responcePtr;
    arangodb::iresearch::RestAnalyzerHandler handler(requestPtr.release(), responcePtr.release());
    request.setRequestType(arangodb::rest::RequestType::GET);
    request.addSuffix("unknownVocbase::unknown");

    arangodb::auth::UserMap userMap; // empty map, no user -> no permissions
    auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
    user.grantDatabase(vocbase.name(), arangodb::auth::Level::NONE); // for system collections User::collectionAuthLevel(...) returns database auth::Level
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
}

SECTION("test_list") {
  // create a new instance of an ApplicationServer and fill it with the required features
  // cannot use the existing server since its features already have some state
  std::shared_ptr<arangodb::application_features::ApplicationServer> originalServer(
    arangodb::application_features::ApplicationServer::server,
    [](arangodb::application_features::ApplicationServer* ptr)->void {
      arangodb::application_features::ApplicationServer::server = ptr;
    }
  );
  arangodb::application_features::ApplicationServer::server = nullptr; // avoid "ApplicationServer initialized twice"
  arangodb::application_features::ApplicationServer server(nullptr, nullptr);
  arangodb::iresearch::IResearchAnalyzerFeature* analyzers;
  arangodb::DatabaseFeature* dbFeature;
  arangodb::SystemDatabaseFeature* sysDatabase;
  server.addFeature(new arangodb::QueryRegistryFeature(server)); // required for constructing TRI_vocbase_t
  server.addFeature(new arangodb::V8DealerFeature(server)); // required for DatabaseFeature::createDatabase(...)
  server.addFeature(dbFeature = new arangodb::DatabaseFeature(server)); // required for IResearchAnalyzerFeature::emplace(...)
  server.addFeature(sysDatabase = new arangodb::SystemDatabaseFeature(server)); // required for IResearchAnalyzerFeature::start()
  server.addFeature(analyzers = new arangodb::iresearch::IResearchAnalyzerFeature(server)); // required for running upgrade task

  // create system vocbase
  {
    auto const databases = arangodb::velocypack::Parser::fromJson(std::string("[ { \"name\": \"") + arangodb::StaticStrings::SystemDatabase + "\" } ]");
    REQUIRE((TRI_ERROR_NO_ERROR == dbFeature->loadDatabases(databases->slice())));
    TRI_vocbase_t* vocbase;
    REQUIRE((TRI_ERROR_NO_ERROR == dbFeature->createDatabase(1, "testVocbase", vocbase)));
    sysDatabase->start(); // get system database from DatabaseFeature
  }

  arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;
  REQUIRE((analyzers->emplace(result, arangodb::StaticStrings::SystemDatabase + "::testAnalyzer1", "identity", nullptr).ok()));
  REQUIRE((analyzers->emplace(result, "testVocbase::testAnalyzer2", "identity", nullptr).ok()));

  struct ExecContext: public arangodb::ExecContext {
    ExecContext(): arangodb::ExecContext(arangodb::ExecContext::Type::Default, "", "",
                                         arangodb::auth::Level::NONE, arangodb::auth::Level::NONE) {}
  } execContext;
  arangodb::ExecContextScope execContextScope(&execContext);
  auto* authFeature = arangodb::AuthenticationFeature::instance();
  auto* userManager = authFeature->userManager();
  arangodb::aql::QueryRegistry queryRegistry(0); // required for UserManager::loadFromDB()
  userManager->setQueryRegistry(&queryRegistry);
  auto resetUserManager = std::shared_ptr<arangodb::auth::UserManager>(userManager, [](arangodb::auth::UserManager* ptr)->void { ptr->removeAllUsers(); });

  // system database (authorised)
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, arangodb::StaticStrings::SystemDatabase);
    auto requestPtr = std::make_unique<GeneralRequestMock>(vocbase);
    auto& request = *requestPtr;
    auto responcePtr = std::make_unique<GeneralResponseMock>();
    auto& responce = *responcePtr;
    arangodb::iresearch::RestAnalyzerHandler handler(requestPtr.release(), responcePtr.release());
    request.setRequestType(arangodb::rest::RequestType::GET);

    arangodb::auth::UserMap userMap; // empty map, no user -> no permissions
    auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
    user.grantDatabase(vocbase.name(), arangodb::auth::Level::RO); // for system collections User::collectionAuthLevel(...) returns database auth::Level
    userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

    std::set<std::string> expected = {
      "identity",
      arangodb::StaticStrings::SystemDatabase + "::testAnalyzer1",
    };
    auto status = handler.execute();
    CHECK((arangodb::RestStatus::DONE == status));
    CHECK((arangodb::rest::ResponseCode::OK == responce.responseCode()));
    auto slice = responce._payload.slice();
    CHECK((slice.isObject()));
    CHECK((slice.hasKey(arangodb::StaticStrings::Code) && slice.get(arangodb::StaticStrings::Code).isNumber<size_t>() && size_t(arangodb::rest::ResponseCode::OK) == slice.get(arangodb::StaticStrings::Code).getNumber<size_t>()));
    CHECK((slice.hasKey(arangodb::StaticStrings::Error) && slice.get(arangodb::StaticStrings::Error).isBoolean() && false == slice.get(arangodb::StaticStrings::Error).getBoolean()));
    CHECK((slice.hasKey("result") && slice.get("result").isArray() && expected.size() == slice.get("result").length()));

    for (arangodb::velocypack::ArrayIterator itr(slice.get("result")); itr.valid(); ++itr) {
      auto subSlice = *itr;
      CHECK((subSlice.isObject()));
      CHECK((subSlice.hasKey("name") && subSlice.get("name").isString()));
      CHECK((subSlice.hasKey("type") && subSlice.get("type").isString()));
      CHECK((subSlice.hasKey("properties") && (subSlice.get("properties").isString() || subSlice.get("properties").isNull())));
      CHECK((subSlice.hasKey("features") && subSlice.get("features").isArray()));
      CHECK((1 == expected.erase(subSlice.get("name").copyString())));
    }

    CHECK((true == expected.empty()));
  }

  // system database (not authorised)
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, arangodb::StaticStrings::SystemDatabase);
    auto requestPtr = std::make_unique<GeneralRequestMock>(vocbase);
    auto& request = *requestPtr;
    auto responcePtr = std::make_unique<GeneralResponseMock>();
    auto& responce = *responcePtr;
    arangodb::iresearch::RestAnalyzerHandler handler(requestPtr.release(), responcePtr.release());
    request.setRequestType(arangodb::rest::RequestType::GET);

    arangodb::auth::UserMap userMap; // empty map, no user -> no permissions
    auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
    user.grantDatabase(vocbase.name(), arangodb::auth::Level::NONE); // for system collections User::collectionAuthLevel(...) returns database auth::Level
    userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

    std::set<std::string> expected = {
      "identity",
    };
    auto status = handler.execute();
    CHECK((arangodb::RestStatus::DONE == status));
    CHECK((arangodb::rest::ResponseCode::OK == responce.responseCode()));
    auto slice = responce._payload.slice();
    CHECK((slice.isObject()));
    CHECK((slice.hasKey(arangodb::StaticStrings::Code) && slice.get(arangodb::StaticStrings::Code).isNumber<size_t>() && size_t(arangodb::rest::ResponseCode::OK) == slice.get(arangodb::StaticStrings::Code).getNumber<size_t>()));
    CHECK((slice.hasKey(arangodb::StaticStrings::Error) && slice.get(arangodb::StaticStrings::Error).isBoolean() && false == slice.get(arangodb::StaticStrings::Error).getBoolean()));
    CHECK((slice.hasKey("result") && slice.get("result").isArray() && expected.size() == slice.get("result").length()));

    for (arangodb::velocypack::ArrayIterator itr(slice.get("result")); itr.valid(); ++itr) {
      auto subSlice = *itr;
      CHECK((subSlice.isObject()));
      CHECK((subSlice.hasKey("name") && subSlice.get("name").isString()));
      CHECK((subSlice.hasKey("type") && subSlice.get("type").isString()));
      CHECK((subSlice.hasKey("properties") && (subSlice.get("properties").isString() || subSlice.get("properties").isNull())));
      CHECK((subSlice.hasKey("features") && subSlice.get("features").isArray()));
      CHECK((1 == expected.erase(subSlice.get("name").copyString())));
    }

    CHECK((true == expected.empty()));
  }

  // non-system database (authorised, system authorised)
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto requestPtr = std::make_unique<GeneralRequestMock>(vocbase);
    auto& request = *requestPtr;
    auto responcePtr = std::make_unique<GeneralResponseMock>();
    auto& responce = *responcePtr;
    arangodb::iresearch::RestAnalyzerHandler handler(requestPtr.release(), responcePtr.release());
    request.setRequestType(arangodb::rest::RequestType::GET);

    arangodb::auth::UserMap userMap; // empty map, no user -> no permissions
    auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
    user.grantDatabase(arangodb::StaticStrings::SystemDatabase, arangodb::auth::Level::RO); // for system collections User::collectionAuthLevel(...) returns database auth::Level
    user.grantDatabase(vocbase.name(), arangodb::auth::Level::RO); // for system collections User::collectionAuthLevel(...) returns database auth::Level
    userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

    std::set<std::string> expected = {
      "identity",
      arangodb::StaticStrings::SystemDatabase + "::testAnalyzer1",
      "testVocbase::testAnalyzer2",
    };
    auto status = handler.execute();
    CHECK((arangodb::RestStatus::DONE == status));
    CHECK((arangodb::rest::ResponseCode::OK == responce.responseCode()));
    auto slice = responce._payload.slice();
    CHECK((slice.isObject()));
    CHECK((slice.hasKey(arangodb::StaticStrings::Code) && slice.get(arangodb::StaticStrings::Code).isNumber<size_t>() && size_t(arangodb::rest::ResponseCode::OK) == slice.get(arangodb::StaticStrings::Code).getNumber<size_t>()));
    CHECK((slice.hasKey(arangodb::StaticStrings::Error) && slice.get(arangodb::StaticStrings::Error).isBoolean() && false == slice.get(arangodb::StaticStrings::Error).getBoolean()));
    CHECK((slice.hasKey("result") && slice.get("result").isArray() && expected.size() == slice.get("result").length()));

    for (arangodb::velocypack::ArrayIterator itr(slice.get("result")); itr.valid(); ++itr) {
      auto subSlice = *itr;
      CHECK((subSlice.isObject()));
      CHECK((subSlice.hasKey("name") && subSlice.get("name").isString()));
      CHECK((subSlice.hasKey("type") && subSlice.get("type").isString()));
      CHECK((subSlice.hasKey("properties") && (subSlice.get("properties").isString() || subSlice.get("properties").isNull())));
      CHECK((subSlice.hasKey("features") && subSlice.get("features").isArray()));
      CHECK((1 == expected.erase(subSlice.get("name").copyString())));
    }

    CHECK((true == expected.empty()));
  }

  // non-system database (not authorised, system authorised)
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto requestPtr = std::make_unique<GeneralRequestMock>(vocbase);
    auto& request = *requestPtr;
    auto responcePtr = std::make_unique<GeneralResponseMock>();
    auto& responce = *responcePtr;
    arangodb::iresearch::RestAnalyzerHandler handler(requestPtr.release(), responcePtr.release());
    request.setRequestType(arangodb::rest::RequestType::GET);

    arangodb::auth::UserMap userMap; // empty map, no user -> no permissions
    auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
    user.grantDatabase(arangodb::StaticStrings::SystemDatabase, arangodb::auth::Level::RO); // for system collections User::collectionAuthLevel(...) returns database auth::Level
    user.grantDatabase(vocbase.name(), arangodb::auth::Level::NONE); // for system collections User::collectionAuthLevel(...) returns database auth::Level
    userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

    std::set<std::string> expected = {
      "identity",
      arangodb::StaticStrings::SystemDatabase + "::testAnalyzer1",
    };
    auto status = handler.execute();
    CHECK((arangodb::RestStatus::DONE == status));
    CHECK((arangodb::rest::ResponseCode::OK == responce.responseCode()));
    auto slice = responce._payload.slice();
    CHECK((slice.isObject()));
    CHECK((slice.hasKey(arangodb::StaticStrings::Code) && slice.get(arangodb::StaticStrings::Code).isNumber<size_t>() && size_t(arangodb::rest::ResponseCode::OK) == slice.get(arangodb::StaticStrings::Code).getNumber<size_t>()));
    CHECK((slice.hasKey(arangodb::StaticStrings::Error) && slice.get(arangodb::StaticStrings::Error).isBoolean() && false == slice.get(arangodb::StaticStrings::Error).getBoolean()));
    CHECK((slice.hasKey("result") && slice.get("result").isArray() && expected.size() == slice.get("result").length()));

    for (arangodb::velocypack::ArrayIterator itr(slice.get("result")); itr.valid(); ++itr) {
      auto subSlice = *itr;
      CHECK((subSlice.isObject()));
      CHECK((subSlice.hasKey("name") && subSlice.get("name").isString()));
      CHECK((subSlice.hasKey("type") && subSlice.get("type").isString()));
      CHECK((subSlice.hasKey("properties") && (subSlice.get("properties").isString() || subSlice.get("properties").isNull())));
      CHECK((subSlice.hasKey("features") && subSlice.get("features").isArray()));
      CHECK((1 == expected.erase(subSlice.get("name").copyString())));
    }

    CHECK((true == expected.empty()));
  }

  // non-system database (authorised, system not authorised)
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto requestPtr = std::make_unique<GeneralRequestMock>(vocbase);
    auto& request = *requestPtr;
    auto responcePtr = std::make_unique<GeneralResponseMock>();
    auto& responce = *responcePtr;
    arangodb::iresearch::RestAnalyzerHandler handler(requestPtr.release(), responcePtr.release());
    request.setRequestType(arangodb::rest::RequestType::GET);

    arangodb::auth::UserMap userMap; // empty map, no user -> no permissions
    auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
    user.grantDatabase(arangodb::StaticStrings::SystemDatabase, arangodb::auth::Level::NONE); // for system collections User::collectionAuthLevel(...) returns database auth::Level
    user.grantDatabase(vocbase.name(), arangodb::auth::Level::RO); // for system collections User::collectionAuthLevel(...) returns database auth::Level
    userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

    std::set<std::string> expected = {
      "identity",
      "testVocbase::testAnalyzer2",
    };
    auto status = handler.execute();
    CHECK((arangodb::RestStatus::DONE == status));
    CHECK((arangodb::rest::ResponseCode::OK == responce.responseCode()));
    auto slice = responce._payload.slice();
    CHECK((slice.isObject()));
    CHECK((slice.hasKey(arangodb::StaticStrings::Code) && slice.get(arangodb::StaticStrings::Code).isNumber<size_t>() && size_t(arangodb::rest::ResponseCode::OK) == slice.get(arangodb::StaticStrings::Code).getNumber<size_t>()));
    CHECK((slice.hasKey(arangodb::StaticStrings::Error) && slice.get(arangodb::StaticStrings::Error).isBoolean() && false == slice.get(arangodb::StaticStrings::Error).getBoolean()));
    CHECK((slice.hasKey("result") && slice.get("result").isArray() && expected.size() == slice.get("result").length()));

    for (arangodb::velocypack::ArrayIterator itr(slice.get("result")); itr.valid(); ++itr) {
      auto subSlice = *itr;
      CHECK((subSlice.isObject()));
      CHECK((subSlice.hasKey("name") && subSlice.get("name").isString()));
      CHECK((subSlice.hasKey("type") && subSlice.get("type").isString()));
      CHECK((subSlice.hasKey("properties") && (subSlice.get("properties").isString() || subSlice.get("properties").isNull())));
      CHECK((subSlice.hasKey("features") && subSlice.get("features").isArray()));
      CHECK((1 == expected.erase(subSlice.get("name").copyString())));
    }

    CHECK((true == expected.empty()));
  }

  // non-system database (not authorised, system not authorised)
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");
    auto requestPtr = std::make_unique<GeneralRequestMock>(vocbase);
    auto& request = *requestPtr;
    auto responcePtr = std::make_unique<GeneralResponseMock>();
    auto& responce = *responcePtr;
    arangodb::iresearch::RestAnalyzerHandler handler(requestPtr.release(), responcePtr.release());
    request.setRequestType(arangodb::rest::RequestType::GET);

    arangodb::auth::UserMap userMap; // empty map, no user -> no permissions
    auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
    user.grantDatabase(arangodb::StaticStrings::SystemDatabase, arangodb::auth::Level::NONE); // for system collections User::collectionAuthLevel(...) returns database auth::Level
    user.grantDatabase(vocbase.name(), arangodb::auth::Level::NONE); // for system collections User::collectionAuthLevel(...) returns database auth::Level
    userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

    std::set<std::string> expected = {
      "identity",
    };
    auto status = handler.execute();
    CHECK((arangodb::RestStatus::DONE == status));
    CHECK((arangodb::rest::ResponseCode::OK == responce.responseCode()));
    auto slice = responce._payload.slice();
    CHECK((slice.isObject()));
    CHECK((slice.hasKey(arangodb::StaticStrings::Code) && slice.get(arangodb::StaticStrings::Code).isNumber<size_t>() && size_t(arangodb::rest::ResponseCode::OK) == slice.get(arangodb::StaticStrings::Code).getNumber<size_t>()));
    CHECK((slice.hasKey(arangodb::StaticStrings::Error) && slice.get(arangodb::StaticStrings::Error).isBoolean() && false == slice.get(arangodb::StaticStrings::Error).getBoolean()));
    CHECK((slice.hasKey("result") && slice.get("result").isArray() && expected.size() == slice.get("result").length()));

    for (arangodb::velocypack::ArrayIterator itr(slice.get("result")); itr.valid(); ++itr) {
      auto subSlice = *itr;
      CHECK((subSlice.isObject()));
      CHECK((subSlice.hasKey("name") && subSlice.get("name").isString()));
      CHECK((subSlice.hasKey("type") && subSlice.get("type").isString()));
      CHECK((subSlice.hasKey("properties") && (subSlice.get("properties").isString() || subSlice.get("properties").isNull())));
      CHECK((subSlice.hasKey("features") && subSlice.get("features").isArray()));
      CHECK((1 == expected.erase(subSlice.get("name").copyString())));
    }

    CHECK((true == expected.empty()));
  }
}

SECTION("test_remove") {
  // create a new instance of an ApplicationServer and fill it with the required features
  // cannot use the existing server since its features already have some state
  std::shared_ptr<arangodb::application_features::ApplicationServer> originalServer(
    arangodb::application_features::ApplicationServer::server,
    [](arangodb::application_features::ApplicationServer* ptr)->void {
      arangodb::application_features::ApplicationServer::server = ptr;
    }
  );
  arangodb::application_features::ApplicationServer::server = nullptr; // avoid "ApplicationServer initialized twice"
  arangodb::application_features::ApplicationServer server(nullptr, nullptr);
  arangodb::iresearch::IResearchAnalyzerFeature* analyzers;
  arangodb::DatabaseFeature* dbFeature;
  server.addFeature(new arangodb::QueryRegistryFeature(server)); // required for constructing TRI_vocbase_t
  server.addFeature(new arangodb::V8DealerFeature(server)); // required for DatabaseFeature::createDatabase(...)
  server.addFeature(dbFeature = new arangodb::DatabaseFeature(server)); // required for IResearchAnalyzerFeature::emplace(...)
  server.addFeature(analyzers = new arangodb::iresearch::IResearchAnalyzerFeature(server)); // required for running upgrade task

  // create system vocbase
  {
    auto const databases = arangodb::velocypack::Parser::fromJson(std::string("[ { \"name\": \"") + arangodb::StaticStrings::SystemDatabase + "\" } ]");
    REQUIRE((TRI_ERROR_NO_ERROR == dbFeature->loadDatabases(databases->slice())));
  }

  arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;
  REQUIRE((analyzers->emplace(result, arangodb::StaticStrings::SystemDatabase + "::testAnalyzer1", "identity", nullptr).ok()));
  REQUIRE((analyzers->emplace(result, arangodb::StaticStrings::SystemDatabase + "::testAnalyzer2", "identity", nullptr).ok()));

  struct ExecContext: public arangodb::ExecContext {
    ExecContext(): arangodb::ExecContext(arangodb::ExecContext::Type::Default, "", "",
                                         arangodb::auth::Level::NONE, arangodb::auth::Level::NONE) {}
  } execContext;
  arangodb::ExecContextScope execContextScope(&execContext);
  auto* authFeature = arangodb::AuthenticationFeature::instance();
  auto* userManager = authFeature->userManager();
  arangodb::aql::QueryRegistry queryRegistry(0); // required for UserManager::loadFromDB()
  userManager->setQueryRegistry(&queryRegistry);
  auto resetUserManager = std::shared_ptr<arangodb::auth::UserManager>(userManager, [](arangodb::auth::UserManager* ptr)->void { ptr->removeAllUsers(); });

  // invalid params (no name)
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, arangodb::StaticStrings::SystemDatabase);
    auto requestPtr = std::make_unique<GeneralRequestMock>(vocbase);
    auto& request = *requestPtr;
    auto responcePtr = std::make_unique<GeneralResponseMock>();
    auto& responce = *responcePtr;
    arangodb::iresearch::RestAnalyzerHandler handler(requestPtr.release(), responcePtr.release());
    request.setRequestType(arangodb::rest::RequestType::DELETE_REQ);

    arangodb::auth::UserMap userMap; // empty map, no user -> no permissions
    auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
    user.grantDatabase(vocbase.name(), arangodb::auth::Level::RW); // for system collections User::collectionAuthLevel(...) returns database auth::Level
    userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

    auto status = handler.execute();
    CHECK((arangodb::RestStatus::DONE == status));
    CHECK((arangodb::rest::ResponseCode::BAD == responce.responseCode()));
    auto slice = responce._payload.slice();
    CHECK((slice.isObject()));
    CHECK((slice.hasKey(arangodb::StaticStrings::Code) && slice.get(arangodb::StaticStrings::Code).isNumber<size_t>() && size_t(arangodb::rest::ResponseCode::BAD) == slice.get(arangodb::StaticStrings::Code).getNumber<size_t>()));
    CHECK((slice.hasKey(arangodb::StaticStrings::Error) && slice.get(arangodb::StaticStrings::Error).isBoolean() && true == slice.get(arangodb::StaticStrings::Error).getBoolean()));
    CHECK((slice.hasKey(arangodb::StaticStrings::ErrorNum) && slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() && TRI_ERROR_BAD_PARAMETER == slice.get(arangodb::StaticStrings::ErrorNum).getNumber<int>()));
  }

  // unknown analyzer
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, arangodb::StaticStrings::SystemDatabase);
    auto requestPtr = std::make_unique<GeneralRequestMock>(vocbase);
    auto& request = *requestPtr;
    auto responcePtr = std::make_unique<GeneralResponseMock>();
    auto& responce = *responcePtr;
    arangodb::iresearch::RestAnalyzerHandler handler(requestPtr.release(), responcePtr.release());
    request.setRequestType(arangodb::rest::RequestType::DELETE_REQ);
    request.addSuffix(arangodb::StaticStrings::SystemDatabase + "::unknown");

    arangodb::auth::UserMap userMap; // empty map, no user -> no permissions
    auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
    user.grantDatabase(vocbase.name(), arangodb::auth::Level::RW); // for system collections User::collectionAuthLevel(...) returns database auth::Level
    userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

    auto status = handler.execute();
    CHECK((arangodb::RestStatus::DONE == status));
    CHECK((arangodb::rest::ResponseCode::NOT_FOUND == responce.responseCode()));
    auto slice = responce._payload.slice();
    CHECK((slice.isObject()));
    CHECK((slice.hasKey(arangodb::StaticStrings::Code) && slice.get(arangodb::StaticStrings::Code).isNumber<size_t>() && size_t(arangodb::rest::ResponseCode::NOT_FOUND) == slice.get(arangodb::StaticStrings::Code).getNumber<size_t>()));
    CHECK((slice.hasKey(arangodb::StaticStrings::Error) && slice.get(arangodb::StaticStrings::Error).isBoolean() && true == slice.get(arangodb::StaticStrings::Error).getBoolean()));
    CHECK((slice.hasKey(arangodb::StaticStrings::ErrorNum) && slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() && TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND == slice.get(arangodb::StaticStrings::ErrorNum).getNumber<int>()));
  }

  // not authorised
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, arangodb::StaticStrings::SystemDatabase);
    auto requestPtr = std::make_unique<GeneralRequestMock>(vocbase);
    auto& request = *requestPtr;
    auto responcePtr = std::make_unique<GeneralResponseMock>();
    auto& responce = *responcePtr;
    arangodb::iresearch::RestAnalyzerHandler handler(requestPtr.release(), responcePtr.release());
    request.setRequestType(arangodb::rest::RequestType::DELETE_REQ);
    request.addSuffix(arangodb::StaticStrings::SystemDatabase + "::testAnalyzer1");

    arangodb::auth::UserMap userMap; // empty map, no user -> no permissions
    auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
    user.grantDatabase(vocbase.name(), arangodb::auth::Level::RO); // for system collections User::collectionAuthLevel(...) returns database auth::Level
    userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

    auto status = handler.execute();
    CHECK((arangodb::RestStatus::DONE == status));
    CHECK((arangodb::rest::ResponseCode::FORBIDDEN == responce.responseCode()));
    auto slice = responce._payload.slice();
    CHECK((slice.isObject()));
    CHECK((slice.hasKey(arangodb::StaticStrings::Code) && slice.get(arangodb::StaticStrings::Code).isNumber<size_t>() && size_t(arangodb::rest::ResponseCode::FORBIDDEN) == slice.get(arangodb::StaticStrings::Code).getNumber<size_t>()));
    CHECK((slice.hasKey(arangodb::StaticStrings::Error) && slice.get(arangodb::StaticStrings::Error).isBoolean() && true == slice.get(arangodb::StaticStrings::Error).getBoolean()));
    CHECK((slice.hasKey(arangodb::StaticStrings::ErrorNum) && slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() && TRI_ERROR_FORBIDDEN == slice.get(arangodb::StaticStrings::ErrorNum).getNumber<int>()));
    auto analyzer = analyzers->get(arangodb::StaticStrings::SystemDatabase + "::testAnalyzer1");
    CHECK((false == !analyzer));
  }

  // still in use (fail)
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, arangodb::StaticStrings::SystemDatabase);
    auto requestPtr = std::make_unique<GeneralRequestMock>(vocbase);
    auto& request = *requestPtr;
    auto responcePtr = std::make_unique<GeneralResponseMock>();
    auto& responce = *responcePtr;
    arangodb::iresearch::RestAnalyzerHandler handler(requestPtr.release(), responcePtr.release());
    request.setRequestType(arangodb::rest::RequestType::DELETE_REQ);
    request.addSuffix(arangodb::StaticStrings::SystemDatabase + "::testAnalyzer2");
    request.values().emplace("force", "false");
    auto inUseAnalyzer = analyzers->get(arangodb::StaticStrings::SystemDatabase + "::testAnalyzer2"); // hold ref to mark in-use
    REQUIRE((false == !inUseAnalyzer));

    arangodb::auth::UserMap userMap; // empty map, no user -> no permissions
    auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
    user.grantDatabase(vocbase.name(), arangodb::auth::Level::RW); // for system collections User::collectionAuthLevel(...) returns database auth::Level
    userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

    auto status = handler.execute();
    CHECK((arangodb::RestStatus::DONE == status));
    CHECK((arangodb::rest::ResponseCode::CONFLICT == responce.responseCode()));
    auto slice = responce._payload.slice();
    CHECK((slice.isObject()));
    CHECK((slice.hasKey(arangodb::StaticStrings::Code) && slice.get(arangodb::StaticStrings::Code).isNumber<size_t>() && size_t(arangodb::rest::ResponseCode::CONFLICT) == slice.get(arangodb::StaticStrings::Code).getNumber<size_t>()));
    CHECK((slice.hasKey(arangodb::StaticStrings::Error) && slice.get(arangodb::StaticStrings::Error).isBoolean() && true == slice.get(arangodb::StaticStrings::Error).getBoolean()));
    CHECK((slice.hasKey(arangodb::StaticStrings::ErrorNum) && slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() && TRI_ERROR_ARANGO_CONFLICT == slice.get(arangodb::StaticStrings::ErrorNum).getNumber<int>()));
    auto analyzer = analyzers->get(arangodb::StaticStrings::SystemDatabase + "::testAnalyzer2");
    CHECK((false == !analyzer));
  }

  // still in use + force (success)
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, arangodb::StaticStrings::SystemDatabase);
    auto requestPtr = std::make_unique<GeneralRequestMock>(vocbase);
    auto& request = *requestPtr;
    auto responcePtr = std::make_unique<GeneralResponseMock>();
    auto& responce = *responcePtr;
    arangodb::iresearch::RestAnalyzerHandler handler(requestPtr.release(), responcePtr.release());
    request.setRequestType(arangodb::rest::RequestType::DELETE_REQ);
    request.addSuffix(arangodb::StaticStrings::SystemDatabase + "::testAnalyzer2");
    request.values().emplace("force", "true");
    auto inUseAnalyzer = analyzers->get(arangodb::StaticStrings::SystemDatabase + "::testAnalyzer2"); // hold ref to mark in-use
    REQUIRE((false == !inUseAnalyzer));

    arangodb::auth::UserMap userMap; // empty map, no user -> no permissions
    auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
    user.grantDatabase(vocbase.name(), arangodb::auth::Level::RW); // for system collections User::collectionAuthLevel(...) returns database auth::Level
    userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

    auto status = handler.execute();
    CHECK((arangodb::RestStatus::DONE == status));
    CHECK((arangodb::rest::ResponseCode::OK == responce.responseCode()));
    auto slice = responce._payload.slice();
    CHECK((slice.isObject()));
    CHECK((slice.hasKey(arangodb::StaticStrings::Code) && slice.get(arangodb::StaticStrings::Code).isNumber<size_t>() && size_t(arangodb::rest::ResponseCode::OK) == slice.get(arangodb::StaticStrings::Code).getNumber<size_t>()));
    CHECK((slice.hasKey(arangodb::StaticStrings::Error) && slice.get(arangodb::StaticStrings::Error).isBoolean() && false == slice.get(arangodb::StaticStrings::Error).getBoolean()));
    CHECK((slice.hasKey("name") && slice.get("name").isString() && arangodb::StaticStrings::SystemDatabase + "::testAnalyzer2" == slice.get("name").copyString()));
    auto analyzer = analyzers->get(arangodb::StaticStrings::SystemDatabase + "::testAnalyzer2");
    CHECK((true == !analyzer));
  }

  // success removal
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, arangodb::StaticStrings::SystemDatabase);
    auto requestPtr = std::make_unique<GeneralRequestMock>(vocbase);
    auto& request = *requestPtr;
    auto responcePtr = std::make_unique<GeneralResponseMock>();
    auto& responce = *responcePtr;
    arangodb::iresearch::RestAnalyzerHandler handler(requestPtr.release(), responcePtr.release());
    request.setRequestType(arangodb::rest::RequestType::DELETE_REQ);
    request.addSuffix(arangodb::StaticStrings::SystemDatabase + "::testAnalyzer1");

    arangodb::auth::UserMap userMap; // empty map, no user -> no permissions
    auto& user = userMap.emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP)).first->second;
    user.grantDatabase(vocbase.name(), arangodb::auth::Level::RW); // for system collections User::collectionAuthLevel(...) returns database auth::Level
    userManager->setAuthInfo(userMap); // set user map to avoid loading configuration from system database

    auto status = handler.execute();
    CHECK((arangodb::RestStatus::DONE == status));
    CHECK((arangodb::rest::ResponseCode::OK == responce.responseCode()));
    auto slice = responce._payload.slice();
    CHECK((slice.isObject()));
    CHECK((slice.hasKey(arangodb::StaticStrings::Code) && slice.get(arangodb::StaticStrings::Code).isNumber<size_t>() && size_t(arangodb::rest::ResponseCode::OK) == slice.get(arangodb::StaticStrings::Code).getNumber<size_t>()));
    CHECK((slice.hasKey(arangodb::StaticStrings::Error) && slice.get(arangodb::StaticStrings::Error).isBoolean() && false == slice.get(arangodb::StaticStrings::Error).getBoolean()));
    CHECK((slice.hasKey("name") && slice.get("name").isString() && arangodb::StaticStrings::SystemDatabase + "::testAnalyzer1" == slice.get("name").copyString()));
    auto analyzer = analyzers->get(arangodb::StaticStrings::SystemDatabase + "::testAnalyzer1");
    CHECK((true == !analyzer));
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate tests
////////////////////////////////////////////////////////////////////////////////

}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------