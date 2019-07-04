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

#include "../IResearch/common.h"
#include "gtest/gtest.h"

#include "../IResearch/common.h"
#include "../IResearch/RestHandlerMock.h"
#include "../Mocks/StorageEngineMock.h"
#include "Aql/QueryRegistry.h"

#if USE_ENTERPRISE
#include "Enterprise/Ldap/LdapFeature.h"
#endif

#include "Basics/VelocyPackHelper.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "IResearch/IResearchAnalyzerFeature.h"
#include "IResearch/IResearchCommon.h"
#include "RestHandler/RestAnalyzerHandler.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "RestServer/SystemDatabaseFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "V8Server/V8DealerFeature.h"
#include "VocBase/Methods/Collections.h"
#include "velocypack/Iterator.h"
#include "velocypack/Parser.h"

#include "analysis/analyzers.hpp"
#include "analysis/token_attributes.hpp"

namespace {

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
  static bool normalize(irs::string_ref const&, std::string& out) {
    out.resize(VPackSlice::emptyObjectSlice().byteSize());
    std::memcpy(&out[0], VPackSlice::emptyObjectSlice().begin(), out.size());
    return true;
  }
  virtual bool next() override { return false; }
  virtual bool reset(irs::string_ref const& data) override { return true; }

 private:
  irs::attribute_view _attrs;
  irs::frequency _attr;
};

DEFINE_ANALYZER_TYPE_NAMED(EmptyAnalyzer, "rest-analyzer-empty");
REGISTER_ANALYZER_VPACK(EmptyAnalyzer, EmptyAnalyzer::make, EmptyAnalyzer::normalize);

static const VPackBuilder systemDatabaseBuilder = dbArgsBuilder();
static const VPackSlice   systemDatabaseArgs = systemDatabaseBuilder.slice();
static const VPackBuilder testDatabaseBuilder = dbArgsBuilder("testVocbase");
static const VPackSlice   testDatabaseArgs = testDatabaseBuilder.slice();
static const VPackBuilder unknownDatabaseBuilder = dbArgsBuilder("unknownVocbase");
static const VPackSlice   unknownDatabaseArgs = unknownDatabaseBuilder.slice();
}

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

class RestAnalyzerHandlerTest : public ::testing::Test {
 protected:
  StorageEngineMock engine;
  arangodb::application_features::ApplicationServer server;
  std::vector<std::pair<arangodb::application_features::ApplicationFeature*, bool>> features;

  RestAnalyzerHandlerTest() : engine(server), server(nullptr, nullptr) {
    arangodb::EngineSelectorFeature::ENGINE = &engine;

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

  ~RestAnalyzerHandlerTest() {
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

TEST_F(RestAnalyzerHandlerTest, test_create) {
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

    auto databases = VPackBuilder();
    databases.openArray();
    databases.add(systemDatabaseArgs);
    databases.close();

    ASSERT_TRUE((TRI_ERROR_NO_ERROR == dbFeature->loadDatabases(databases.slice())));
    sysDatabase->start();  // get system database from DatabaseFeature
    arangodb::methods::Collections::createSystem(
        *sysDatabase->use(),
        arangodb::tests::AnalyzerCollectionName);
  }

  arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;

  {
    const auto name = arangodb::StaticStrings::SystemDatabase + "::testAnalyzer1";
    ASSERT_TRUE(analyzers->emplace(result, name, "identity",
                                  VPackParser::fromJson("{\"args\":\"abc\"}")->slice())
                           .ok());
  }

  {
    const auto name = arangodb::StaticStrings::SystemDatabase + "::emptyAnalyzer";
    ASSERT_TRUE(analyzers->emplace(result, name, "rest-analyzer-empty",
                                  VPackParser::fromJson("{\"args\":\"en\"}")->slice(),
                                  irs::flags{irs::frequency::type()})
                           .ok());
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

  // invalid params (non-object body)
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                          systemDatabaseArgs);
    auto requestPtr = std::make_unique<GeneralRequestMock>(vocbase);
    auto& request = *requestPtr;
    auto responcePtr = std::make_unique<GeneralResponseMock>();
    auto& responce = *responcePtr;
    arangodb::iresearch::RestAnalyzerHandler handler(requestPtr.release(),
                                                     responcePtr.release());
    request.setRequestType(arangodb::rest::RequestType::POST);
    request._payload.openArray();
    request._payload.close();

    arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
    auto& user =
        userMap
            .emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP))
            .first->second;
    user.grantDatabase(vocbase.name(), arangodb::auth::Level::RW);  // for system collections User::collectionAuthLevel(...) returns database auth::Level
    userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database

    auto status = handler.execute();
    EXPECT_TRUE((arangodb::RestStatus::DONE == status));
    EXPECT_TRUE((arangodb::rest::ResponseCode::BAD == responce.responseCode()));
    auto slice = responce._payload.slice();
    EXPECT_TRUE((slice.isObject()));
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::Code) &&
                 slice.get(arangodb::StaticStrings::Code).isNumber<size_t>() &&
                 size_t(arangodb::rest::ResponseCode::BAD) ==
                     slice.get(arangodb::StaticStrings::Code).getNumber<size_t>()));
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::Error) &&
                 slice.get(arangodb::StaticStrings::Error).isBoolean() &&
                 true == slice.get(arangodb::StaticStrings::Error).getBoolean()));
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::ErrorNum) &&
                 slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() &&
                 TRI_ERROR_BAD_PARAMETER ==
                     slice.get(arangodb::StaticStrings::ErrorNum).getNumber<int>()));
  }

  // invalid params (no name)
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                          systemDatabaseArgs);
    auto requestPtr = std::make_unique<GeneralRequestMock>(vocbase);
    auto& request = *requestPtr;
    auto responcePtr = std::make_unique<GeneralResponseMock>();
    auto& responce = *responcePtr;
    arangodb::iresearch::RestAnalyzerHandler handler(requestPtr.release(),
                                                     responcePtr.release());
    request.setRequestType(arangodb::rest::RequestType::POST);
    request._payload.openObject();
    request._payload.add("type", arangodb::velocypack::Value("identity"));
    request._payload.add("properties", arangodb::velocypack::Value(
                                           arangodb::velocypack::ValueType::Null));
    request._payload.close();

    arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
    auto& user =
        userMap
            .emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP))
            .first->second;
    user.grantDatabase(vocbase.name(), arangodb::auth::Level::RW);  // for system collections User::collectionAuthLevel(...) returns database auth::Level
    userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database

    auto status = handler.execute();
    EXPECT_TRUE((arangodb::RestStatus::DONE == status));
    EXPECT_TRUE((arangodb::rest::ResponseCode::BAD == responce.responseCode()));
    auto slice = responce._payload.slice();
    EXPECT_TRUE((slice.isObject()));
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::Code) &&
                 slice.get(arangodb::StaticStrings::Code).isNumber<size_t>() &&
                 size_t(arangodb::rest::ResponseCode::BAD) ==
                     slice.get(arangodb::StaticStrings::Code).getNumber<size_t>()));
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::Error) &&
                 slice.get(arangodb::StaticStrings::Error).isBoolean() &&
                 true == slice.get(arangodb::StaticStrings::Error).getBoolean()));
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::ErrorNum) &&
                 slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() &&
                 TRI_ERROR_BAD_PARAMETER ==
                     slice.get(arangodb::StaticStrings::ErrorNum).getNumber<int>()));
  }

  // invalid params (invalid name)
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                          systemDatabaseArgs);
    auto requestPtr = std::make_unique<GeneralRequestMock>(vocbase);
    auto& request = *requestPtr;
    auto responcePtr = std::make_unique<GeneralResponseMock>();
    auto& responce = *responcePtr;
    arangodb::iresearch::RestAnalyzerHandler handler(requestPtr.release(),
                                                     responcePtr.release());
    request.setRequestType(arangodb::rest::RequestType::POST);
    request._payload.openObject();
    request._payload.add("name", VPackValue("unknownVocbase::testAnalyzer"));
    request._payload.add("type", VPackValue("identity"));
    request._payload.add("properties", VPackValue(arangodb::velocypack::ValueType::Null));
    request._payload.close();

    arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
    auto& user =
        userMap
            .emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP))
            .first->second;
    user.grantDatabase(vocbase.name(), arangodb::auth::Level::RW);  // for system collections User::collectionAuthLevel(...) returns database auth::Level
    userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database

    auto status = handler.execute();
    EXPECT_TRUE((arangodb::RestStatus::DONE == status));
    EXPECT_TRUE((arangodb::rest::ResponseCode::BAD == responce.responseCode()));
    auto slice = responce._payload.slice();
    EXPECT_TRUE((slice.isObject()));
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::Code) &&
                 slice.get(arangodb::StaticStrings::Code).isNumber<size_t>() &&
                 size_t(arangodb::rest::ResponseCode::BAD) ==
                     slice.get(arangodb::StaticStrings::Code).getNumber<size_t>()));
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::Error) &&
                 slice.get(arangodb::StaticStrings::Error).isBoolean() &&
                 true == slice.get(arangodb::StaticStrings::Error).getBoolean()));
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::ErrorNum) &&
                 slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() &&
                 TRI_ERROR_BAD_PARAMETER ==
                     slice.get(arangodb::StaticStrings::ErrorNum).getNumber<int>()));
  }

  // invalid params (name contains invalid symbols)
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                          systemDatabaseArgs);
    auto requestPtr = std::make_unique<GeneralRequestMock>(vocbase);
    auto& request = *requestPtr;
    auto responcePtr = std::make_unique<GeneralResponseMock>();
    auto& responce = *responcePtr;
    arangodb::iresearch::RestAnalyzerHandler handler(requestPtr.release(),
                                                     responcePtr.release());
    request.setRequestType(arangodb::rest::RequestType::POST);
    request._payload.openObject();
    request._payload.add("name", VPackValue(arangodb::StaticStrings::SystemDatabase + "::testAnalyzer"));
    request._payload.add("type", VPackValue("identity"));
    request._payload.add("properties", VPackValue(arangodb::velocypack::ValueType::Null));
    request._payload.close();

    arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
    auto& user =
        userMap
            .emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP))
            .first->second;
    user.grantDatabase(vocbase.name(), arangodb::auth::Level::RW);  // for system collections User::collectionAuthLevel(...) returns database auth::Level
    userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database

    auto status = handler.execute();
    EXPECT_TRUE((arangodb::RestStatus::DONE == status));
    EXPECT_TRUE((arangodb::rest::ResponseCode::BAD == responce.responseCode()));
    auto slice = responce._payload.slice();
    EXPECT_TRUE((slice.isObject()));
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::Code) &&
                 slice.get(arangodb::StaticStrings::Code).isNumber<size_t>() &&
                 size_t(arangodb::rest::ResponseCode::BAD) ==
                     slice.get(arangodb::StaticStrings::Code).getNumber<size_t>()));
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::Error) &&
                 slice.get(arangodb::StaticStrings::Error).isBoolean() &&
                 true == slice.get(arangodb::StaticStrings::Error).getBoolean()));
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::ErrorNum) &&
                 slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() &&
                 TRI_ERROR_BAD_PARAMETER ==
                     slice.get(arangodb::StaticStrings::ErrorNum).getNumber<int>()));
  }

  // invalid params (name contains invalid symbols)
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                          systemDatabaseArgs);
    auto requestPtr = std::make_unique<GeneralRequestMock>(vocbase);
    auto& request = *requestPtr;
    auto responcePtr = std::make_unique<GeneralResponseMock>();
    auto& responce = *responcePtr;
    arangodb::iresearch::RestAnalyzerHandler handler(requestPtr.release(),
                                                     responcePtr.release());
    request.setRequestType(arangodb::rest::RequestType::POST);
    request._payload.openObject();
    request._payload.add("name", VPackValue("::testAnalyzer"));
    request._payload.add("type", VPackValue("identity"));
    request._payload.add("properties", VPackValue(arangodb::velocypack::ValueType::Null));
    request._payload.close();

    arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
    auto& user =
        userMap
            .emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP))
            .first->second;
    user.grantDatabase(vocbase.name(), arangodb::auth::Level::RW);  // for system collections User::collectionAuthLevel(...) returns database auth::Level
    userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database

    auto status = handler.execute();
    EXPECT_TRUE((arangodb::RestStatus::DONE == status));
    EXPECT_TRUE((arangodb::rest::ResponseCode::BAD == responce.responseCode()));
    auto slice = responce._payload.slice();
    EXPECT_TRUE((slice.isObject()));
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::Code) &&
                 slice.get(arangodb::StaticStrings::Code).isNumber<size_t>() &&
                 size_t(arangodb::rest::ResponseCode::BAD) ==
                     slice.get(arangodb::StaticStrings::Code).getNumber<size_t>()));
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::Error) &&
                 slice.get(arangodb::StaticStrings::Error).isBoolean() &&
                 true == slice.get(arangodb::StaticStrings::Error).getBoolean()));
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::ErrorNum) &&
                 slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() &&
                 TRI_ERROR_BAD_PARAMETER ==
                     slice.get(arangodb::StaticStrings::ErrorNum).getNumber<int>()));
  }

  // name collision
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                          systemDatabaseArgs);
    auto requestPtr = std::make_unique<GeneralRequestMock>(vocbase);
    auto& request = *requestPtr;
    auto responcePtr = std::make_unique<GeneralResponseMock>();
    auto& responce = *responcePtr;
    arangodb::iresearch::RestAnalyzerHandler handler(requestPtr.release(),
                                                     responcePtr.release());
    request.setRequestType(arangodb::rest::RequestType::POST);
    request._payload.openObject();
    request._payload.add("name", arangodb::velocypack::Value("emptyAnalyzer"));
    request._payload.add("type", arangodb::velocypack::Value("rest-analyzer-empty"));
    request._payload.add("properties", arangodb::velocypack::Value("{\"args\":\"abc\"}"));
    request._payload.close();

    arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
    auto& user =
        userMap
            .emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP))
            .first->second;
    user.grantDatabase(vocbase.name(), arangodb::auth::Level::RW);  // for system collections User::collectionAuthLevel(...) returns database auth::Level
    userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database

    auto status = handler.execute();
    EXPECT_TRUE((arangodb::RestStatus::DONE == status));
    EXPECT_TRUE((arangodb::rest::ResponseCode::BAD == responce.responseCode()));
    auto slice = responce._payload.slice();
    EXPECT_TRUE((slice.isObject()));
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::Code) &&
                 slice.get(arangodb::StaticStrings::Code).isNumber<size_t>() &&
                 size_t(arangodb::rest::ResponseCode::BAD) ==
                     slice.get(arangodb::StaticStrings::Code).getNumber<size_t>()));
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::Error) &&
                 slice.get(arangodb::StaticStrings::Error).isBoolean() &&
                 true == slice.get(arangodb::StaticStrings::Error).getBoolean()));
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::ErrorNum) &&
                 slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() &&
                 TRI_ERROR_BAD_PARAMETER ==
                     slice.get(arangodb::StaticStrings::ErrorNum).getNumber<int>()));
  }

  // duplicate matching
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                          systemDatabaseArgs);
    auto requestPtr = std::make_unique<GeneralRequestMock>(vocbase);
    auto& request = *requestPtr;
    auto responcePtr = std::make_unique<GeneralResponseMock>();
    auto& responce = *responcePtr;
    arangodb::iresearch::RestAnalyzerHandler handler(requestPtr.release(),
                                                     responcePtr.release());
    request.setRequestType(arangodb::rest::RequestType::POST);
    request._payload.openObject();
    request._payload.add("name", VPackValue("testAnalyzer1"));
    request._payload.add("type", VPackValue("identity"));
    request._payload.add("properties", VPackSlice::noneSlice());
    request._payload.close();

    arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
    auto& user =
        userMap
            .emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP))
            .first->second;
    user.grantDatabase(vocbase.name(), arangodb::auth::Level::RW);  // for system collections User::collectionAuthLevel(...) returns database auth::Level
    userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database

    auto status = handler.execute();
    EXPECT_TRUE((arangodb::RestStatus::DONE == status));
    EXPECT_TRUE((arangodb::rest::ResponseCode::OK == responce.responseCode()));
    auto slice = responce._payload.slice();
    EXPECT_TRUE((slice.isObject()));
    EXPECT_TRUE((slice.hasKey("name") && slice.get("name").isString() &&
                 arangodb::StaticStrings::SystemDatabase + "::testAnalyzer1" ==
                     slice.get("name").copyString()));
    EXPECT_TRUE((slice.hasKey("type") && slice.get("type").isString()));
    EXPECT_TRUE(slice.hasKey("properties") && slice.get("properties").isObject());
    EXPECT_TRUE((slice.hasKey("features") && slice.get("features").isArray()));
    auto analyzer = analyzers->get(arangodb::StaticStrings::SystemDatabase +
                                   "::testAnalyzer1");
    EXPECT_TRUE((false == !analyzer));
  }

  // not authorised
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                          systemDatabaseArgs);
    auto requestPtr = std::make_unique<GeneralRequestMock>(vocbase);
    auto& request = *requestPtr;
    auto responcePtr = std::make_unique<GeneralResponseMock>();
    auto& responce = *responcePtr;
    arangodb::iresearch::RestAnalyzerHandler handler(requestPtr.release(),
                                                     responcePtr.release());
    request.setRequestType(arangodb::rest::RequestType::POST);
    request._payload.openObject();
    request._payload.add("name", VPackValue("testAnalyzer2"));
    request._payload.add("type", VPackValue("identity"));
    request._payload.add("properties", arangodb::velocypack::Value("{\"args\":\"abc\"}"));
    request._payload.close();

    arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
    auto& user =
        userMap
            .emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP))
            .first->second;
    user.grantDatabase(vocbase.name(), arangodb::auth::Level::RO);  // for system collections User::collectionAuthLevel(...) returns database auth::Level
    userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database

    auto status = handler.execute();
    EXPECT_TRUE((arangodb::RestStatus::DONE == status));
    EXPECT_TRUE((arangodb::rest::ResponseCode::FORBIDDEN == responce.responseCode()));
    auto slice = responce._payload.slice();
    EXPECT_TRUE((slice.isObject()));
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::Code) &&
                 slice.get(arangodb::StaticStrings::Code).isNumber<size_t>() &&
                 size_t(arangodb::rest::ResponseCode::FORBIDDEN) ==
                     slice.get(arangodb::StaticStrings::Code).getNumber<size_t>()));
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::Error) &&
                 slice.get(arangodb::StaticStrings::Error).isBoolean() &&
                 true == slice.get(arangodb::StaticStrings::Error).getBoolean()));
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::ErrorNum) &&
                 slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() &&
                 TRI_ERROR_FORBIDDEN ==
                     slice.get(arangodb::StaticStrings::ErrorNum).getNumber<int>()));
  }

  // successful creation
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                          systemDatabaseArgs);
    auto requestPtr = std::make_unique<GeneralRequestMock>(vocbase);
    auto& request = *requestPtr;
    auto responcePtr = std::make_unique<GeneralResponseMock>();
    auto& responce = *responcePtr;
    arangodb::iresearch::RestAnalyzerHandler handler(requestPtr.release(),
                                                     responcePtr.release());
    request.setRequestType(arangodb::rest::RequestType::POST);
    request._payload.openObject();
    request._payload.add("name", arangodb::velocypack::Value("testAnalyzer2"));
    request._payload.add("type", arangodb::velocypack::Value("identity"));
    request._payload.add("properties", arangodb::velocypack::Value("{\"args\":\"abc\"}"));
    request._payload.close();

    arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
    auto& user =
        userMap
            .emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP))
            .first->second;
    user.grantDatabase(vocbase.name(), arangodb::auth::Level::RW);  // for system collections User::collectionAuthLevel(...) returns database auth::Level
    userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database

    auto status = handler.execute();
    EXPECT_TRUE((arangodb::RestStatus::DONE == status));
    EXPECT_TRUE((arangodb::rest::ResponseCode::CREATED == responce.responseCode()));
    auto slice = responce._payload.slice();
    EXPECT_TRUE((slice.isObject()));
    EXPECT_TRUE((slice.hasKey("name") && slice.get("name").isString() &&
                 arangodb::StaticStrings::SystemDatabase + "::testAnalyzer2" ==
                     slice.get("name").copyString()));
    EXPECT_TRUE((slice.hasKey("type") && slice.get("type").isString()));
    EXPECT_TRUE(slice.hasKey("properties") && slice.get("properties").isObject());
    EXPECT_TRUE((slice.hasKey("features") && slice.get("features").isArray()));
    auto analyzer = analyzers->get(arangodb::StaticStrings::SystemDatabase +
                                   "::testAnalyzer1");
    EXPECT_TRUE((false == !analyzer));
  }
}

TEST_F(RestAnalyzerHandlerTest, test_get) {
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
    auto databases = VPackBuilder();
    databases.openArray();
    databases.add(systemDatabaseArgs);
    databases.close();

    ASSERT_TRUE((TRI_ERROR_NO_ERROR == dbFeature->loadDatabases(databases.slice())));
    arangodb::methods::Collections::createSystem(
        *dbFeature->useDatabase(arangodb::StaticStrings::SystemDatabase),
        arangodb::tests::AnalyzerCollectionName);
  }

  arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;
  ASSERT_TRUE((analyzers
                   ->emplace(result, arangodb::StaticStrings::SystemDatabase + "::testAnalyzer1",
                             "identity",
                             VPackSlice::noneSlice())  // Empty VPack for nullptr
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

  // get static (known analyzer)
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                          systemDatabaseArgs);
    auto requestPtr = std::make_unique<GeneralRequestMock>(vocbase);
    auto& request = *requestPtr;
    auto responcePtr = std::make_unique<GeneralResponseMock>();
    auto& responce = *responcePtr;
    arangodb::iresearch::RestAnalyzerHandler handler(requestPtr.release(),
                                                     responcePtr.release());
    request.setRequestType(arangodb::rest::RequestType::GET);
    request.addSuffix("identity");

    arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
    auto& user =
        userMap
            .emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP))
            .first->second;
    user.grantDatabase(vocbase.name(), arangodb::auth::Level::NONE);  // for system collections User::collectionAuthLevel(...) returns database auth::Level
    userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database

    auto status = handler.execute();
    EXPECT_TRUE((arangodb::RestStatus::DONE == status));
    EXPECT_TRUE((arangodb::rest::ResponseCode::OK == responce.responseCode()));
    auto slice = responce._payload.slice();
    EXPECT_TRUE((slice.isObject()));
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::Code) &&
                 slice.get(arangodb::StaticStrings::Code).isNumber<size_t>() &&
                 size_t(arangodb::rest::ResponseCode::OK) ==
                     slice.get(arangodb::StaticStrings::Code).getNumber<size_t>()));
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::Error) &&
                 slice.get(arangodb::StaticStrings::Error).isBoolean() &&
                 false == slice.get(arangodb::StaticStrings::Error).getBoolean()));
    EXPECT_TRUE((slice.hasKey("name") && slice.get("name").isString() &&
                 std::string("identity") == slice.get("name").copyString()));
    EXPECT_TRUE((slice.hasKey("type") && slice.get("type").isString()));
    EXPECT_TRUE(slice.hasKey("properties") && slice.get("properties").isObject());
    EXPECT_TRUE((slice.hasKey("features") && slice.get("features").isArray()));
  }

  // get static (unknown analyzer)
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                          systemDatabaseArgs);
    auto requestPtr = std::make_unique<GeneralRequestMock>(vocbase);
    auto& request = *requestPtr;
    auto responcePtr = std::make_unique<GeneralResponseMock>();
    auto& responce = *responcePtr;
    arangodb::iresearch::RestAnalyzerHandler handler(requestPtr.release(),
                                                     responcePtr.release());
    request.setRequestType(arangodb::rest::RequestType::GET);
    request.addSuffix("unknown");

    arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
    auto& user =
        userMap
            .emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP))
            .first->second;
    user.grantDatabase(vocbase.name(), arangodb::auth::Level::NONE);  // for system collections User::collectionAuthLevel(...) returns database auth::Level
    userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database

    auto status = handler.execute();
    EXPECT_TRUE((arangodb::RestStatus::DONE == status));
    EXPECT_TRUE((arangodb::rest::ResponseCode::NOT_FOUND == responce.responseCode()));
    auto slice = responce._payload.slice();
    EXPECT_TRUE((slice.isObject()));
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::Code) &&
                 slice.get(arangodb::StaticStrings::Code).isNumber<size_t>() &&
                 size_t(arangodb::rest::ResponseCode::NOT_FOUND) ==
                     slice.get(arangodb::StaticStrings::Code).getNumber<size_t>()));
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::Error) &&
                 slice.get(arangodb::StaticStrings::Error).isBoolean() &&
                 true == slice.get(arangodb::StaticStrings::Error).getBoolean()));
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::ErrorNum) &&
                 slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() &&
                 TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND ==
                     slice.get(arangodb::StaticStrings::ErrorNum).getNumber<int>()));
  }

  // get custom (known analyzer) authorized
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                          systemDatabaseArgs);
    auto requestPtr = std::make_unique<GeneralRequestMock>(vocbase);
    auto& request = *requestPtr;
    auto responcePtr = std::make_unique<GeneralResponseMock>();
    auto& responce = *responcePtr;
    arangodb::iresearch::RestAnalyzerHandler handler(requestPtr.release(),
                                                     responcePtr.release());
    request.setRequestType(arangodb::rest::RequestType::GET);
    request.addSuffix(arangodb::StaticStrings::SystemDatabase +
                      "::testAnalyzer1");

    arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
    auto& user =
        userMap
            .emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP))
            .first->second;
    user.grantDatabase(vocbase.name(), arangodb::auth::Level::RO);  // for system collections User::collectionAuthLevel(...) returns database auth::Level
    userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database

    auto status = handler.execute();
    EXPECT_TRUE((arangodb::RestStatus::DONE == status));
    EXPECT_TRUE((arangodb::rest::ResponseCode::OK == responce.responseCode()));
    auto slice = responce._payload.slice();
    EXPECT_TRUE((slice.isObject()));
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::Code) &&
                 slice.get(arangodb::StaticStrings::Code).isNumber<size_t>() &&
                 size_t(arangodb::rest::ResponseCode::OK) ==
                     slice.get(arangodb::StaticStrings::Code).getNumber<size_t>()));
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::Error) &&
                 slice.get(arangodb::StaticStrings::Error).isBoolean() &&
                 false == slice.get(arangodb::StaticStrings::Error).getBoolean()));
    EXPECT_TRUE((slice.hasKey("name") && slice.get("name").isString() &&
                 arangodb::StaticStrings::SystemDatabase + "::testAnalyzer1" ==
                     slice.get("name").copyString()));
    EXPECT_TRUE((slice.hasKey("type") && slice.get("type").isString()));
    EXPECT_TRUE(slice.hasKey("properties") && slice.get("properties").isObject());
    EXPECT_TRUE((slice.hasKey("features") && slice.get("features").isArray()));
  }

  // get custom (known analyzer) not authorized
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                          systemDatabaseArgs);
    auto requestPtr = std::make_unique<GeneralRequestMock>(vocbase);
    auto& request = *requestPtr;
    auto responcePtr = std::make_unique<GeneralResponseMock>();
    auto& responce = *responcePtr;
    arangodb::iresearch::RestAnalyzerHandler handler(requestPtr.release(),
                                                     responcePtr.release());
    request.setRequestType(arangodb::rest::RequestType::GET);
    request.addSuffix(arangodb::StaticStrings::SystemDatabase +
                      "::testAnalyzer1");

    arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
    auto& user =
        userMap
            .emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP))
            .first->second;
    user.grantDatabase(vocbase.name(), arangodb::auth::Level::NONE);  // for system collections User::collectionAuthLevel(...) returns database auth::Level
    userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database

    auto status = handler.execute();
    EXPECT_TRUE((arangodb::RestStatus::DONE == status));
    EXPECT_TRUE((arangodb::rest::ResponseCode::FORBIDDEN == responce.responseCode()));
    auto slice = responce._payload.slice();
    EXPECT_TRUE((slice.isObject()));
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::Code) &&
                 slice.get(arangodb::StaticStrings::Code).isNumber<size_t>() &&
                 size_t(arangodb::rest::ResponseCode::FORBIDDEN) ==
                     slice.get(arangodb::StaticStrings::Code).getNumber<size_t>()));
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::Error) &&
                 slice.get(arangodb::StaticStrings::Error).isBoolean() &&
                 true == slice.get(arangodb::StaticStrings::Error).getBoolean()));
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::ErrorNum) &&
                 slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() &&
                 TRI_ERROR_FORBIDDEN ==
                     slice.get(arangodb::StaticStrings::ErrorNum).getNumber<int>()));
  }

  // get custom (unknown analyzer) authorized
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                          systemDatabaseArgs);
    auto requestPtr = std::make_unique<GeneralRequestMock>(vocbase);
    auto& request = *requestPtr;
    auto responcePtr = std::make_unique<GeneralResponseMock>();
    auto& responce = *responcePtr;
    arangodb::iresearch::RestAnalyzerHandler handler(requestPtr.release(),
                                                     responcePtr.release());
    request.setRequestType(arangodb::rest::RequestType::GET);
    request.addSuffix(arangodb::StaticStrings::SystemDatabase + "::unknown");

    arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
    auto& user =
        userMap
            .emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP))
            .first->second;
    user.grantDatabase(vocbase.name(), arangodb::auth::Level::RO);  // for system collections User::collectionAuthLevel(...) returns database auth::Level
    userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database

    auto status = handler.execute();
    EXPECT_TRUE((arangodb::RestStatus::DONE == status));
    EXPECT_TRUE((arangodb::rest::ResponseCode::NOT_FOUND == responce.responseCode()));
    auto slice = responce._payload.slice();
    EXPECT_TRUE((slice.isObject()));
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::Code) &&
                 slice.get(arangodb::StaticStrings::Code).isNumber<size_t>() &&
                 size_t(arangodb::rest::ResponseCode::NOT_FOUND) ==
                     slice.get(arangodb::StaticStrings::Code).getNumber<size_t>()));
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::Error) &&
                 slice.get(arangodb::StaticStrings::Error).isBoolean() &&
                 true == slice.get(arangodb::StaticStrings::Error).getBoolean()));
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::ErrorNum) &&
                 slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() &&
                 TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND ==
                     slice.get(arangodb::StaticStrings::ErrorNum).getNumber<int>()));
  }

  // get custom (unknown analyzer) not authorized
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                          systemDatabaseArgs);
    auto requestPtr = std::make_unique<GeneralRequestMock>(vocbase);
    auto& request = *requestPtr;
    auto responcePtr = std::make_unique<GeneralResponseMock>();
    auto& responce = *responcePtr;
    arangodb::iresearch::RestAnalyzerHandler handler(requestPtr.release(),
                                                     responcePtr.release());
    request.setRequestType(arangodb::rest::RequestType::GET);
    request.addSuffix(arangodb::StaticStrings::SystemDatabase + "::unknown");

    arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
    auto& user =
        userMap
            .emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP))
            .first->second;
    user.grantDatabase(vocbase.name(), arangodb::auth::Level::NONE);  // for system collections User::collectionAuthLevel(...) returns database auth::Level
    userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database

    auto status = handler.execute();
    EXPECT_TRUE((arangodb::RestStatus::DONE == status));
    EXPECT_TRUE((arangodb::rest::ResponseCode::FORBIDDEN == responce.responseCode()));
    auto slice = responce._payload.slice();
    EXPECT_TRUE((slice.isObject()));
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::Code) &&
                 slice.get(arangodb::StaticStrings::Code).isNumber<size_t>() &&
                 size_t(arangodb::rest::ResponseCode::FORBIDDEN) ==
                     slice.get(arangodb::StaticStrings::Code).getNumber<size_t>()));
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::Error) &&
                 slice.get(arangodb::StaticStrings::Error).isBoolean() &&
                 true == slice.get(arangodb::StaticStrings::Error).getBoolean()));
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::ErrorNum) &&
                 slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() &&
                 TRI_ERROR_FORBIDDEN ==
                     slice.get(arangodb::StaticStrings::ErrorNum).getNumber<int>()));
  }

  // get custom (unknown analyzer, unknown vocbase) authorized
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                          unknownDatabaseArgs);
    auto requestPtr = std::make_unique<GeneralRequestMock>(vocbase);
    auto& request = *requestPtr;
    auto responcePtr = std::make_unique<GeneralResponseMock>();
    auto& responce = *responcePtr;
    arangodb::iresearch::RestAnalyzerHandler handler(requestPtr.release(),
                                                     responcePtr.release());
    request.setRequestType(arangodb::rest::RequestType::GET);
    request.addSuffix("unknownVocbase::unknown");

    arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
    auto& user =
        userMap
            .emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP))
            .first->second;
    user.grantDatabase("unknownVocbase", arangodb::auth::Level::RO);  // for system collections User::collectionAuthLevel(...) returns database auth::Level
    userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database

    auto status = handler.execute();
    EXPECT_TRUE((arangodb::RestStatus::DONE == status));
    EXPECT_TRUE((arangodb::rest::ResponseCode::NOT_FOUND == responce.responseCode()));
    auto slice = responce._payload.slice();
    EXPECT_TRUE((slice.isObject()));
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::Code) &&
                 slice.get(arangodb::StaticStrings::Code).isNumber<size_t>() &&
                 size_t(arangodb::rest::ResponseCode::NOT_FOUND) ==
                     slice.get(arangodb::StaticStrings::Code).getNumber<size_t>()));
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::Error) &&
                 slice.get(arangodb::StaticStrings::Error).isBoolean() &&
                 true == slice.get(arangodb::StaticStrings::Error).getBoolean()));
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::ErrorNum) &&
                 slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() &&
                 TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND ==
                     slice.get(arangodb::StaticStrings::ErrorNum).getNumber<int>()));
  }

  // get custom (unknown analyzer, unknown vocbase) not authorized
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                          unknownDatabaseArgs);
    auto requestPtr = std::make_unique<GeneralRequestMock>(vocbase);
    auto& request = *requestPtr;
    auto responcePtr = std::make_unique<GeneralResponseMock>();
    auto& responce = *responcePtr;
    arangodb::iresearch::RestAnalyzerHandler handler(requestPtr.release(),
                                                     responcePtr.release());
    request.setRequestType(arangodb::rest::RequestType::GET);
    request.addSuffix("unknownVocbase::unknown");

    arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
    auto& user =
        userMap
            .emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP))
            .first->second;
    user.grantDatabase(vocbase.name(), arangodb::auth::Level::NONE);  // for system collections User::collectionAuthLevel(...) returns database auth::Level
    userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database

    auto status = handler.execute();
    EXPECT_TRUE((arangodb::RestStatus::DONE == status));
    EXPECT_TRUE((arangodb::rest::ResponseCode::FORBIDDEN == responce.responseCode()));
    auto slice = responce._payload.slice();
    EXPECT_TRUE((slice.isObject()));
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::Code) &&
                 slice.get(arangodb::StaticStrings::Code).isNumber<size_t>() &&
                 size_t(arangodb::rest::ResponseCode::FORBIDDEN) ==
                     slice.get(arangodb::StaticStrings::Code).getNumber<size_t>()));
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::Error) &&
                 slice.get(arangodb::StaticStrings::Error).isBoolean() &&
                 true == slice.get(arangodb::StaticStrings::Error).getBoolean()));
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::ErrorNum) &&
                 slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() &&
                 TRI_ERROR_FORBIDDEN ==
                     slice.get(arangodb::StaticStrings::ErrorNum).getNumber<int>()));
  }
}

TEST_F(RestAnalyzerHandlerTest, test_list) {
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
    auto databases = VPackBuilder();
    databases.openArray();
    databases.add(systemDatabaseArgs);
    databases.close();

    ASSERT_TRUE((TRI_ERROR_NO_ERROR == dbFeature->loadDatabases(databases.slice())));
    TRI_vocbase_t* vocbase;
    ASSERT_TRUE((TRI_ERROR_NO_ERROR ==
                 dbFeature->createDatabase(1, "testVocbase", arangodb::velocypack::Slice::emptyObjectSlice(), vocbase)));
    sysDatabase->start();  // get system database from DatabaseFeature
    arangodb::methods::Collections::createSystem(
        *vocbase,
        arangodb::tests::AnalyzerCollectionName);
    arangodb::methods::Collections::createSystem(
        *sysDatabase->use(),
        arangodb::tests::AnalyzerCollectionName);
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
                          systemDatabaseArgs);
    auto requestPtr = std::make_unique<GeneralRequestMock>(vocbase);
    auto& request = *requestPtr;
    auto responcePtr = std::make_unique<GeneralResponseMock>();
    auto& responce = *responcePtr;
    arangodb::iresearch::RestAnalyzerHandler handler(requestPtr.release(),
                                                     responcePtr.release());
    request.setRequestType(arangodb::rest::RequestType::GET);

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
    auto status = handler.execute();
    EXPECT_TRUE((arangodb::RestStatus::DONE == status));
    EXPECT_TRUE((arangodb::rest::ResponseCode::OK == responce.responseCode()));
    auto slice = responce._payload.slice();
    EXPECT_TRUE((slice.isObject()));
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::Code) &&
                 slice.get(arangodb::StaticStrings::Code).isNumber<size_t>() &&
                 size_t(arangodb::rest::ResponseCode::OK) ==
                     slice.get(arangodb::StaticStrings::Code).getNumber<size_t>()));
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::Error) &&
                 slice.get(arangodb::StaticStrings::Error).isBoolean() &&
                 false == slice.get(arangodb::StaticStrings::Error).getBoolean()));
    EXPECT_TRUE((slice.hasKey("result") && slice.get("result").isArray() &&
                 expected.size() == slice.get("result").length()));

    for (arangodb::velocypack::ArrayIterator itr(slice.get("result")); itr.valid(); ++itr) {
      auto subSlice = *itr;
      EXPECT_TRUE((subSlice.isObject()));
      EXPECT_TRUE((subSlice.hasKey("name") && subSlice.get("name").isString()));
      EXPECT_TRUE((subSlice.hasKey("type") && subSlice.get("type").isString()));
      EXPECT_TRUE((subSlice.hasKey("properties") &&
                   (subSlice.get("properties").isObject() ||
                    subSlice.get("properties").isNull())));
      EXPECT_TRUE((subSlice.hasKey("features") && subSlice.get("features").isArray()));
      EXPECT_TRUE((1 == expected.erase(subSlice.get("name").copyString())));
    }

    EXPECT_TRUE((true == expected.empty()));
  }

  // system database (not authorised)
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                          systemDatabaseArgs);
    auto requestPtr = std::make_unique<GeneralRequestMock>(vocbase);
    auto& request = *requestPtr;
    auto responcePtr = std::make_unique<GeneralResponseMock>();
    auto& responce = *responcePtr;
    arangodb::iresearch::RestAnalyzerHandler handler(requestPtr.release(),
                                                     responcePtr.release());
    request.setRequestType(arangodb::rest::RequestType::GET);

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
    auto status = handler.execute();
    EXPECT_TRUE((arangodb::RestStatus::DONE == status));
    EXPECT_TRUE((arangodb::rest::ResponseCode::OK == responce.responseCode()));
    auto slice = responce._payload.slice();
    EXPECT_TRUE((slice.isObject()));
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::Code) &&
                 slice.get(arangodb::StaticStrings::Code).isNumber<size_t>() &&
                 size_t(arangodb::rest::ResponseCode::OK) ==
                     slice.get(arangodb::StaticStrings::Code).getNumber<size_t>()));
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::Error) &&
                 slice.get(arangodb::StaticStrings::Error).isBoolean() &&
                 false == slice.get(arangodb::StaticStrings::Error).getBoolean()));
    EXPECT_TRUE((slice.hasKey("result") && slice.get("result").isArray() &&
                 expected.size() == slice.get("result").length()));

    for (arangodb::velocypack::ArrayIterator itr(slice.get("result")); itr.valid(); ++itr) {
      auto subSlice = *itr;
      EXPECT_TRUE((subSlice.isObject()));
      EXPECT_TRUE((subSlice.hasKey("name") && subSlice.get("name").isString()));
      EXPECT_TRUE((subSlice.hasKey("type") && subSlice.get("type").isString()));
      EXPECT_TRUE((subSlice.hasKey("properties") &&
                   (subSlice.get("properties").isObject() ||
                    subSlice.get("properties").isNull())));
      EXPECT_TRUE((subSlice.hasKey("features") && subSlice.get("features").isArray()));
      EXPECT_TRUE((1 == expected.erase(subSlice.get("name").copyString())));
    }

    EXPECT_TRUE((true == expected.empty()));
  }

  // non-system database (authorised, system authorised)
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                          testDatabaseArgs);
    auto requestPtr = std::make_unique<GeneralRequestMock>(vocbase);
    auto& request = *requestPtr;
    auto responcePtr = std::make_unique<GeneralResponseMock>();
    auto& responce = *responcePtr;
    arangodb::iresearch::RestAnalyzerHandler handler(requestPtr.release(),
                                                     responcePtr.release());
    request.setRequestType(arangodb::rest::RequestType::GET);

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
    auto status = handler.execute();
    EXPECT_TRUE((arangodb::RestStatus::DONE == status));
    EXPECT_TRUE((arangodb::rest::ResponseCode::OK == responce.responseCode()));
    auto slice = responce._payload.slice();
    EXPECT_TRUE((slice.isObject()));
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::Code) &&
                 slice.get(arangodb::StaticStrings::Code).isNumber<size_t>() &&
                 size_t(arangodb::rest::ResponseCode::OK) ==
                     slice.get(arangodb::StaticStrings::Code).getNumber<size_t>()));
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::Error) &&
                 slice.get(arangodb::StaticStrings::Error).isBoolean() &&
                 false == slice.get(arangodb::StaticStrings::Error).getBoolean()));
    EXPECT_TRUE((slice.hasKey("result") && slice.get("result").isArray() &&
                 expected.size() == slice.get("result").length()));

    for (arangodb::velocypack::ArrayIterator itr(slice.get("result")); itr.valid(); ++itr) {
      auto subSlice = *itr;
      EXPECT_TRUE((subSlice.isObject()));
      EXPECT_TRUE((subSlice.hasKey("name") && subSlice.get("name").isString()));
      EXPECT_TRUE((subSlice.hasKey("type") && subSlice.get("type").isString()));
      EXPECT_TRUE((subSlice.hasKey("properties") &&
                   (subSlice.get("properties").isObject() ||
                    subSlice.get("properties").isNull())));
      EXPECT_TRUE((subSlice.hasKey("features") && subSlice.get("features").isArray()));
      EXPECT_TRUE((1 == expected.erase(subSlice.get("name").copyString())));
    }

    EXPECT_TRUE((true == expected.empty()));
  }

  // non-system database (not authorised, system authorised)
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                          testDatabaseArgs);
    auto requestPtr = std::make_unique<GeneralRequestMock>(vocbase);
    auto& request = *requestPtr;
    auto responcePtr = std::make_unique<GeneralResponseMock>();
    auto& responce = *responcePtr;
    arangodb::iresearch::RestAnalyzerHandler handler(requestPtr.release(),
                                                     responcePtr.release());
    request.setRequestType(arangodb::rest::RequestType::GET);

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
    auto status = handler.execute();
    EXPECT_TRUE((arangodb::RestStatus::DONE == status));
    EXPECT_TRUE((arangodb::rest::ResponseCode::OK == responce.responseCode()));
    auto slice = responce._payload.slice();
    EXPECT_TRUE((slice.isObject()));
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::Code) &&
                 slice.get(arangodb::StaticStrings::Code).isNumber<size_t>() &&
                 size_t(arangodb::rest::ResponseCode::OK) ==
                     slice.get(arangodb::StaticStrings::Code).getNumber<size_t>()));
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::Error) &&
                 slice.get(arangodb::StaticStrings::Error).isBoolean() &&
                 false == slice.get(arangodb::StaticStrings::Error).getBoolean()));
    EXPECT_TRUE((slice.hasKey("result") && slice.get("result").isArray() &&
                 expected.size() == slice.get("result").length()));

    for (arangodb::velocypack::ArrayIterator itr(slice.get("result")); itr.valid(); ++itr) {
      auto subSlice = *itr;
      EXPECT_TRUE((subSlice.isObject()));
      EXPECT_TRUE((subSlice.hasKey("name") && subSlice.get("name").isString()));
      EXPECT_TRUE((subSlice.hasKey("type") && subSlice.get("type").isString()));
      EXPECT_TRUE((subSlice.hasKey("properties") &&
                   (subSlice.get("properties").isObject() ||
                    subSlice.get("properties").isNull())));
      EXPECT_TRUE((subSlice.hasKey("features") && subSlice.get("features").isArray()));
      EXPECT_TRUE((1 == expected.erase(subSlice.get("name").copyString())));
    }

    EXPECT_TRUE((true == expected.empty()));
  }

  // non-system database (authorised, system not authorised)
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                          testDatabaseArgs);
    auto requestPtr = std::make_unique<GeneralRequestMock>(vocbase);
    auto& request = *requestPtr;
    auto responcePtr = std::make_unique<GeneralResponseMock>();
    auto& responce = *responcePtr;
    arangodb::iresearch::RestAnalyzerHandler handler(requestPtr.release(),
                                                     responcePtr.release());
    request.setRequestType(arangodb::rest::RequestType::GET);

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
    auto status = handler.execute();
    EXPECT_TRUE((arangodb::RestStatus::DONE == status));
    EXPECT_TRUE((arangodb::rest::ResponseCode::OK == responce.responseCode()));
    auto slice = responce._payload.slice();
    EXPECT_TRUE((slice.isObject()));
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::Code) &&
                 slice.get(arangodb::StaticStrings::Code).isNumber<size_t>() &&
                 size_t(arangodb::rest::ResponseCode::OK) ==
                     slice.get(arangodb::StaticStrings::Code).getNumber<size_t>()));
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::Error) &&
                 slice.get(arangodb::StaticStrings::Error).isBoolean() &&
                 false == slice.get(arangodb::StaticStrings::Error).getBoolean()));
    EXPECT_TRUE((slice.hasKey("result") && slice.get("result").isArray() &&
                 expected.size() == slice.get("result").length()));

    for (arangodb::velocypack::ArrayIterator itr(slice.get("result")); itr.valid(); ++itr) {
      auto subSlice = *itr;
      EXPECT_TRUE((subSlice.isObject()));
      EXPECT_TRUE((subSlice.hasKey("name") && subSlice.get("name").isString()));
      EXPECT_TRUE((subSlice.hasKey("type") && subSlice.get("type").isString()));
      EXPECT_TRUE((subSlice.hasKey("properties") &&
                   (subSlice.get("properties").isObject() ||
                    subSlice.get("properties").isNull())));
      EXPECT_TRUE((subSlice.hasKey("features") && subSlice.get("features").isArray()));
      EXPECT_TRUE((1 == expected.erase(subSlice.get("name").copyString())));
    }

    EXPECT_TRUE((true == expected.empty()));
  }

  // non-system database (not authorised, system not authorised)
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                          testDatabaseArgs);
    auto requestPtr = std::make_unique<GeneralRequestMock>(vocbase);
    auto& request = *requestPtr;
    auto responcePtr = std::make_unique<GeneralResponseMock>();
    auto& responce = *responcePtr;
    arangodb::iresearch::RestAnalyzerHandler handler(requestPtr.release(),
                                                     responcePtr.release());
    request.setRequestType(arangodb::rest::RequestType::GET);

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
    auto status = handler.execute();
    EXPECT_TRUE((arangodb::RestStatus::DONE == status));
    EXPECT_TRUE((arangodb::rest::ResponseCode::OK == responce.responseCode()));
    auto slice = responce._payload.slice();
    EXPECT_TRUE((slice.isObject()));
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::Code) &&
                 slice.get(arangodb::StaticStrings::Code).isNumber<size_t>() &&
                 size_t(arangodb::rest::ResponseCode::OK) ==
                     slice.get(arangodb::StaticStrings::Code).getNumber<size_t>()));
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::Error) &&
                 slice.get(arangodb::StaticStrings::Error).isBoolean() &&
                 false == slice.get(arangodb::StaticStrings::Error).getBoolean()));
    EXPECT_TRUE((slice.hasKey("result") && slice.get("result").isArray() &&
                 expected.size() == slice.get("result").length()));

    for (arangodb::velocypack::ArrayIterator itr(slice.get("result")); itr.valid(); ++itr) {
      auto subSlice = *itr;
      EXPECT_TRUE((subSlice.isObject()));
      EXPECT_TRUE((subSlice.hasKey("name") && subSlice.get("name").isString()));
      EXPECT_TRUE((subSlice.hasKey("type") && subSlice.get("type").isString()));
      EXPECT_TRUE((subSlice.hasKey("properties") &&
                   (subSlice.get("properties").isObject() ||
                    subSlice.get("properties").isNull())));
      EXPECT_TRUE((subSlice.hasKey("features") && subSlice.get("features").isArray()));
      EXPECT_TRUE((1 == expected.erase(subSlice.get("name").copyString())));
    }

    EXPECT_TRUE((true == expected.empty()));
  }
}

TEST_F(RestAnalyzerHandlerTest, test_remove) {
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
    auto databases = VPackBuilder();
    databases.openArray();
    databases.add(systemDatabaseArgs);
    databases.close();

    ASSERT_TRUE((TRI_ERROR_NO_ERROR == dbFeature->loadDatabases(databases.slice())));
    arangodb::methods::Collections::createSystem(
        *dbFeature->useDatabase(arangodb::StaticStrings::SystemDatabase),
        arangodb::tests::AnalyzerCollectionName);
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
                          systemDatabaseArgs);
    auto requestPtr = std::make_unique<GeneralRequestMock>(vocbase);
    auto& request = *requestPtr;
    auto responcePtr = std::make_unique<GeneralResponseMock>();
    auto& responce = *responcePtr;
    arangodb::iresearch::RestAnalyzerHandler handler(requestPtr.release(),
                                                     responcePtr.release());
    request.setRequestType(arangodb::rest::RequestType::DELETE_REQ);

    arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
    auto& user =
        userMap
            .emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP))
            .first->second;
    user.grantDatabase(vocbase.name(), arangodb::auth::Level::RW);  // for system collections User::collectionAuthLevel(...) returns database auth::Level
    userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database

    auto status = handler.execute();
    EXPECT_TRUE((arangodb::RestStatus::DONE == status));
    EXPECT_TRUE((arangodb::rest::ResponseCode::BAD == responce.responseCode()));
    auto slice = responce._payload.slice();
    EXPECT_TRUE((slice.isObject()));
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::Code) &&
                 slice.get(arangodb::StaticStrings::Code).isNumber<size_t>() &&
                 size_t(arangodb::rest::ResponseCode::BAD) ==
                     slice.get(arangodb::StaticStrings::Code).getNumber<size_t>()));
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::Error) &&
                 slice.get(arangodb::StaticStrings::Error).isBoolean() &&
                 true == slice.get(arangodb::StaticStrings::Error).getBoolean()));
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::ErrorNum) &&
                 slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() &&
                 TRI_ERROR_BAD_PARAMETER ==
                     slice.get(arangodb::StaticStrings::ErrorNum).getNumber<int>()));
  }

  // unknown analyzer
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                          systemDatabaseArgs);
    auto requestPtr = std::make_unique<GeneralRequestMock>(vocbase);
    auto& request = *requestPtr;
    auto responcePtr = std::make_unique<GeneralResponseMock>();
    auto& responce = *responcePtr;
    arangodb::iresearch::RestAnalyzerHandler handler(requestPtr.release(),
                                                     responcePtr.release());
    request.setRequestType(arangodb::rest::RequestType::DELETE_REQ);
    request.addSuffix(arangodb::StaticStrings::SystemDatabase + "::unknown");

    arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
    auto& user =
        userMap
            .emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP))
            .first->second;
    user.grantDatabase(vocbase.name(), arangodb::auth::Level::RW);  // for system collections User::collectionAuthLevel(...) returns database auth::Level
    userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database

    auto status = handler.execute();
    EXPECT_TRUE((arangodb::RestStatus::DONE == status));
    EXPECT_TRUE((arangodb::rest::ResponseCode::NOT_FOUND == responce.responseCode()));
    auto slice = responce._payload.slice();
    EXPECT_TRUE((slice.isObject()));
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::Code) &&
                 slice.get(arangodb::StaticStrings::Code).isNumber<size_t>() &&
                 size_t(arangodb::rest::ResponseCode::NOT_FOUND) ==
                     slice.get(arangodb::StaticStrings::Code).getNumber<size_t>()));
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::Error) &&
                 slice.get(arangodb::StaticStrings::Error).isBoolean() &&
                 true == slice.get(arangodb::StaticStrings::Error).getBoolean()));
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::ErrorNum) &&
                 slice.get(arangodb::StaticStrings::ErrorNum).isNumber<int>() &&
                 TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND ==
                     slice.get(arangodb::StaticStrings::ErrorNum).getNumber<int>()));
  }

  // not authorised
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                          systemDatabaseArgs);
    auto requestPtr = std::make_unique<GeneralRequestMock>(vocbase);
    auto& request = *requestPtr;
    auto responcePtr = std::make_unique<GeneralResponseMock>();
    auto& responce = *responcePtr;
    arangodb::iresearch::RestAnalyzerHandler handler(requestPtr.release(),
                                                     responcePtr.release());
    request.setRequestType(arangodb::rest::RequestType::DELETE_REQ);
    request.addSuffix(arangodb::StaticStrings::SystemDatabase +
                      "::testAnalyzer1");

    arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
    auto& user =
        userMap
            .emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP))
            .first->second;
    user.grantDatabase(vocbase.name(), arangodb::auth::Level::RO);  // for system collections User::collectionAuthLevel(...) returns database auth::Level
    userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database

    auto status = handler.execute();
    EXPECT_TRUE((arangodb::RestStatus::DONE == status));
    EXPECT_TRUE((arangodb::rest::ResponseCode::FORBIDDEN == responce.responseCode()));
    auto slice = responce._payload.slice();
    EXPECT_TRUE((slice.isObject()));
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::Code) &&
                 slice.get(arangodb::StaticStrings::Code).isNumber<size_t>() &&
                 size_t(arangodb::rest::ResponseCode::FORBIDDEN) ==
                     slice.get(arangodb::StaticStrings::Code).getNumber<size_t>()));
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::Error) &&
                 slice.get(arangodb::StaticStrings::Error).isBoolean() &&
                 true == slice.get(arangodb::StaticStrings::Error).getBoolean()));
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
                          systemDatabaseArgs);
    auto requestPtr = std::make_unique<GeneralRequestMock>(vocbase);
    auto& request = *requestPtr;
    auto responcePtr = std::make_unique<GeneralResponseMock>();
    auto& responce = *responcePtr;
    arangodb::iresearch::RestAnalyzerHandler handler(requestPtr.release(),
                                                     responcePtr.release());
    request.setRequestType(arangodb::rest::RequestType::DELETE_REQ);
    request.addSuffix(arangodb::StaticStrings::SystemDatabase +
                      "::testAnalyzer2");
    request.values().emplace("force", "false");
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

    auto status = handler.execute();
    EXPECT_TRUE((arangodb::RestStatus::DONE == status));
    EXPECT_TRUE((arangodb::rest::ResponseCode::CONFLICT == responce.responseCode()));
    auto slice = responce._payload.slice();
    EXPECT_TRUE((slice.isObject()));
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::Code) &&
                 slice.get(arangodb::StaticStrings::Code).isNumber<size_t>() &&
                 size_t(arangodb::rest::ResponseCode::CONFLICT) ==
                     slice.get(arangodb::StaticStrings::Code).getNumber<size_t>()));
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::Error) &&
                 slice.get(arangodb::StaticStrings::Error).isBoolean() &&
                 true == slice.get(arangodb::StaticStrings::Error).getBoolean()));
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
                          systemDatabaseArgs);
    auto requestPtr = std::make_unique<GeneralRequestMock>(vocbase);
    auto& request = *requestPtr;
    auto responcePtr = std::make_unique<GeneralResponseMock>();
    auto& responce = *responcePtr;
    arangodb::iresearch::RestAnalyzerHandler handler(requestPtr.release(),
                                                     responcePtr.release());
    request.setRequestType(arangodb::rest::RequestType::DELETE_REQ);
    request.addSuffix(arangodb::StaticStrings::SystemDatabase +
                      "::testAnalyzer2");
    request.values().emplace("force", "true");
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

    auto status = handler.execute();
    EXPECT_TRUE((arangodb::RestStatus::DONE == status));
    EXPECT_TRUE((arangodb::rest::ResponseCode::OK == responce.responseCode()));
    auto slice = responce._payload.slice();
    EXPECT_TRUE((slice.isObject()));
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::Code) &&
                 slice.get(arangodb::StaticStrings::Code).isNumber<size_t>() &&
                 size_t(arangodb::rest::ResponseCode::OK) ==
                     slice.get(arangodb::StaticStrings::Code).getNumber<size_t>()));
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::Error) &&
                 slice.get(arangodb::StaticStrings::Error).isBoolean() &&
                 false == slice.get(arangodb::StaticStrings::Error).getBoolean()));
    EXPECT_TRUE((slice.hasKey("name") && slice.get("name").isString() &&
                 arangodb::StaticStrings::SystemDatabase + "::testAnalyzer2" ==
                     slice.get("name").copyString()));
    auto analyzer = analyzers->get(arangodb::StaticStrings::SystemDatabase +
                                   "::testAnalyzer2");
    EXPECT_TRUE((true == !analyzer));
  }

  // success removal
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                          systemDatabaseArgs);
    auto requestPtr = std::make_unique<GeneralRequestMock>(vocbase);
    auto& request = *requestPtr;
    auto responcePtr = std::make_unique<GeneralResponseMock>();
    auto& responce = *responcePtr;
    arangodb::iresearch::RestAnalyzerHandler handler(requestPtr.release(),
                                                     responcePtr.release());
    request.setRequestType(arangodb::rest::RequestType::DELETE_REQ);
    request.addSuffix(arangodb::StaticStrings::SystemDatabase +
                      "::testAnalyzer1");

    arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
    auto& user =
        userMap
            .emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP))
            .first->second;
    user.grantDatabase(vocbase.name(), arangodb::auth::Level::RW);  // for system collections User::collectionAuthLevel(...) returns database auth::Level
    userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database

    auto status = handler.execute();
    EXPECT_TRUE((arangodb::RestStatus::DONE == status));
    EXPECT_TRUE((arangodb::rest::ResponseCode::OK == responce.responseCode()));
    auto slice = responce._payload.slice();
    EXPECT_TRUE((slice.isObject()));
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::Code) &&
                 slice.get(arangodb::StaticStrings::Code).isNumber<size_t>() &&
                 size_t(arangodb::rest::ResponseCode::OK) ==
                     slice.get(arangodb::StaticStrings::Code).getNumber<size_t>()));
    EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::Error) &&
                 slice.get(arangodb::StaticStrings::Error).isBoolean() &&
                 false == slice.get(arangodb::StaticStrings::Error).getBoolean()));
    EXPECT_TRUE((slice.hasKey("name") && slice.get("name").isString() &&
                 arangodb::StaticStrings::SystemDatabase + "::testAnalyzer1" ==
                     slice.get("name").copyString()));
    auto analyzer = analyzers->get(arangodb::StaticStrings::SystemDatabase +
                                   "::testAnalyzer1");
    EXPECT_TRUE((true == !analyzer));
  }
}
