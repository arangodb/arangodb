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
#include "VocBase/LogicalCollection.h"

#include "velocypack/Iterator.h"
#include "velocypack/Parser.h"
#include "tests/Mocks/Servers.h"

#include "analysis/analyzers.hpp"
#include "analysis/token_attributes.hpp"

#include "Logger/LogMacros.h"

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

}  // namespace

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

class RestAnalyzerHandlerTest : public ::testing::Test {
 protected:
  arangodb::tests::mocks::MockAqlServer server;
  TRI_vocbase_t& _system_vocbase;


  std::vector<std::pair<arangodb::application_features::ApplicationFeature*, bool>> features;
  arangodb::iresearch::IResearchAnalyzerFeature* analyzers;
  arangodb::DatabaseFeature* dbFeature;
  arangodb::AuthenticationFeature* authFeature;
  arangodb::auth::UserManager* userManager;

  struct ExecContext : public arangodb::ExecContext {
    ExecContext()
      : arangodb::ExecContext(arangodb::ExecContext::Type::Default, "", "",
                              arangodb::auth::Level::NONE,
                              arangodb::auth::Level::NONE) {}
  } execContext;
  arangodb::ExecContextScope execContextScope; // (&execContext);
  arangodb::aql::QueryRegistry queryRegistry;  // required for UserManager::loadFromDB()

  RestAnalyzerHandlerTest()
      : server(),
        _system_vocbase(server.getSystemDatabase()),
        execContext(),
        execContextScope(&execContext),
        queryRegistry(0) {
    // suppress INFO {authentication} Authentication is turned on (system only), authentication for unix sockets is turned on
    // suppress WARNING {authentication} --server.jwt-secret is insecure. Use --server.jwt-secret-keyfile instead
    arangodb::LogTopic::setLogLevel(arangodb::Logger::AUTHENTICATION.name(),
                                    arangodb::LogLevel::ERR);

    // suppress log messages since tests check error conditions
    arangodb::LogTopic::setLogLevel(arangodb::iresearch::TOPIC.name(),
                                    arangodb::LogLevel::FATAL);

    authFeature = arangodb::AuthenticationFeature::instance();
    TRI_ASSERT(authFeature != nullptr);

    userManager = authFeature->userManager();
    TRI_ASSERT(userManager != nullptr);

    userManager->setQueryRegistry(&queryRegistry);

    arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
    auto& user =
      userMap
      .emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP))
      .first->second;

    // for system collections User::collectionAuthLevel(...) returns database auth::Level
    user.grantDatabase(arangodb::StaticStrings::SystemDatabase,
                       arangodb::auth::Level::RW);
    // set user map to avoid loading configuration from system database
    userManager->setAuthInfo(userMap);

    analyzers = arangodb::application_features::ApplicationServer::server
                    ->lookupFeature<arangodb::iresearch::IResearchAnalyzerFeature>(
                        "ArangoSearchAnalyzer");
    TRI_ASSERT(analyzers != nullptr);

    // TODO: This should at the very least happen in the mock
    // create system vocbase
    {
      arangodb::methods::Collections::createSystem(server.getSystemDatabase(),
                                                   arangodb::tests::AnalyzerCollectionName,
                                                   false);
    }
    dbFeature = arangodb::application_features::ApplicationServer::server
                    ->lookupFeature<arangodb::DatabaseFeature>("Database");
    TRI_ASSERT(dbFeature != nullptr);
  }
  ~RestAnalyzerHandlerTest() {
    arangodb::LogTopic::setLogLevel(arangodb::iresearch::TOPIC.name(),
                                    arangodb::LogLevel::DEFAULT);
    arangodb::LogTopic::setLogLevel(arangodb::Logger::AUTHENTICATION.name(),
                                    arangodb::LogLevel::DEFAULT);
  }

  // Creates the analyzers that are used in all the tests
  void createAnalyzers() {
    arangodb::Result res;
    std::string name;
    arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;

    name = arangodb::StaticStrings::SystemDatabase + "::testAnalyzer1";
    res = analyzers->emplace(result, name, "identity",
                             VPackParser::fromJson("{\"args\":\"abc\"}")->slice());
    ASSERT_TRUE(res.ok());

    name = arangodb::StaticStrings::SystemDatabase + "::emptyAnalyzer";
    res = analyzers->emplace(result, name, "rest-analyzer-empty",
                             VPackParser::fromJson("{\"args\":\"en\"}")->slice(),
                             irs::flags{irs::frequency::type()});
    ASSERT_TRUE(res.ok());
  };
};

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

// invalid params (non-object body)
TEST_F(RestAnalyzerHandlerTest, test_create_non_object_body) {

  createAnalyzers();

  auto requestPtr = std::make_unique<GeneralRequestMock>(_system_vocbase);
  auto& request = *requestPtr;
  auto responcePtr = std::make_unique<GeneralResponseMock>();
  auto& responce = *responcePtr;
  arangodb::iresearch::RestAnalyzerHandler handler(requestPtr.release(),
                                                     responcePtr.release());
  request.setRequestType(arangodb::rest::RequestType::POST);
  request._payload.openArray();
  request._payload.close();

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

TEST_F(RestAnalyzerHandlerTest, test_create_no_name) {
  createAnalyzers();

  auto requestPtr = std::make_unique<GeneralRequestMock>(_system_vocbase);
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

TEST_F(RestAnalyzerHandlerTest, test_create_no_permission) {

  createAnalyzers();

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


  // invalid params (no permission to access the analyzer given in name)
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                          arangodb::StaticStrings::SystemDatabase);
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

TEST_F(RestAnalyzerHandlerTest, test_create_invalid_symbols) {

  createAnalyzers();

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



  // invalid params (name contains invalid symbols)
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                          arangodb::StaticStrings::SystemDatabase);
    auto requestPtr = std::make_unique<GeneralRequestMock>(vocbase);
    auto& request = *requestPtr;
    auto responcePtr = std::make_unique<GeneralResponseMock>();
    auto& responce = *responcePtr;
    arangodb::iresearch::RestAnalyzerHandler handler(requestPtr.release(),
                                                     responcePtr.release());
    request.setRequestType(arangodb::rest::RequestType::POST);
    request._payload.openObject();
    request._payload.add("name", VPackValue(arangodb::StaticStrings::SystemDatabase +
                                            "::testAnalyzer"));
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
}

// TODO: is this the smae test as above?
TEST_F(RestAnalyzerHandlerTest, test_create_invalid_symbols_2) {
  createAnalyzers();

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


  // invalid params (name contains invalid symbols)
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                          arangodb::StaticStrings::SystemDatabase);
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
}

TEST_F(RestAnalyzerHandlerTest, test_create_name_collision) {
  createAnalyzers();

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


  // name collision
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                          arangodb::StaticStrings::SystemDatabase);
    auto requestPtr = std::make_unique<GeneralRequestMock>(vocbase);
    auto& request = *requestPtr;
    auto responcePtr = std::make_unique<GeneralResponseMock>();
    auto& responce = *responcePtr;
    arangodb::iresearch::RestAnalyzerHandler handler(requestPtr.release(),
                                                     responcePtr.release());
    request.setRequestType(arangodb::rest::RequestType::POST);
    request._payload.openObject();
    request._payload.add("name", arangodb::velocypack::Value("emptyAnalyzer"));
    request._payload.add("type",
                         arangodb::velocypack::Value("rest-analyzer-empty"));
    request._payload.add("properties",
                         arangodb::velocypack::Value("{\"args\":\"abc\"}"));
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
}

TEST_F(RestAnalyzerHandlerTest, test_create_duplicate_matching) {
  createAnalyzers();

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


  // duplicate matching
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                          arangodb::StaticStrings::SystemDatabase);
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
}

TEST_F(RestAnalyzerHandlerTest, test_create_not_authorized) {
  createAnalyzers();

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

  // not authorised
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                          arangodb::StaticStrings::SystemDatabase);
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
    request._payload.add("properties",
                         arangodb::velocypack::Value("{\"args\":\"abc\"}"));
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
}

TEST_F(RestAnalyzerHandlerTest, test_create_success) {
  createAnalyzers();

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

  // successful creation
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                          arangodb::StaticStrings::SystemDatabase);
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
    request._payload.add("properties",
                         arangodb::velocypack::Value("{\"args\":\"abc\"}"));
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

TEST_F(RestAnalyzerHandlerTest, test_get_static_known) {

  createAnalyzers();

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
  // get static (known analyzer)
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                          arangodb::StaticStrings::SystemDatabase);
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
}

TEST_F(RestAnalyzerHandlerTest, test_get_static_unknown) {

  createAnalyzers();

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

  // get static (unknown analyzer)
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                          arangodb::StaticStrings::SystemDatabase);
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
    user.grantDatabase(vocbase.name(), arangodb::auth::Level::RO);  // for system collections User::collectionAuthLevel(...) returns database auth::Level
    userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database

    auto status = handler.execute();
    EXPECT_TRUE((arangodb::RestStatus::DONE == status));
    EXPECT_EQ(arangodb::rest::ResponseCode::NOT_FOUND, responce.responseCode());
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
}

TEST_F(RestAnalyzerHandlerTest, test_get_known) {

  createAnalyzers();

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


  // get custom (known analyzer) authorized
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                          arangodb::StaticStrings::SystemDatabase);
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
}

TEST_F(RestAnalyzerHandlerTest, test_get_custom) {

  createAnalyzers();

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
  // get custom (known analyzer) authorized but from different db
  {
    auto const databases = arangodb::velocypack::Parser::fromJson(
      std::string("[ { \"name\": \"FooDb\" }, { \"name\": \"FooDb2\" } ]"));

    ASSERT_TRUE((TRI_ERROR_NO_ERROR == dbFeature->loadDatabases(databases->slice())));

    arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
    auto& user =
      userMap
      .emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP))
      .first->second;
    user.grantDatabase("FooDb", arangodb::auth::Level::RW);
    user.grantDatabase("FooDb2", arangodb::auth::Level::RW);
    user.grantDatabase(arangodb::StaticStrings::SystemDatabase, arangodb::auth::Level::RO);
    userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database

    arangodb::Result res;
    std::tie(res, std::ignore) = arangodb::methods::Collections::createSystem(
      *dbFeature->useDatabase("FooDb"), arangodb::tests::AnalyzerCollectionName, false);
    ASSERT_TRUE(res.ok());

    std::tie(res, std::ignore) = arangodb::methods::Collections::createSystem(
      *dbFeature->useDatabase("FooDb2"), arangodb::tests::AnalyzerCollectionName, false);
    ASSERT_TRUE(res.ok());

    arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;
    ASSERT_TRUE((analyzers->emplace(result, "FooDb::testAnalyzer1", "identity",
                                    VPackSlice::noneSlice()).ok()));  // Empty VPack for nullptr

    {
      TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                            "FooDb2");
      auto requestPtr = std::make_unique<GeneralRequestMock>(vocbase);
      auto& request = *requestPtr;
      auto responcePtr = std::make_unique<GeneralResponseMock>();
      auto& responce = *responcePtr;
      arangodb::iresearch::RestAnalyzerHandler handler(requestPtr.release(),
                                                       responcePtr.release());
      request.setRequestType(arangodb::rest::RequestType::GET);
      request.addSuffix("FooDb::testAnalyzer1");

      auto status = handler.execute();
      EXPECT_TRUE((arangodb::RestStatus::DONE == status));
      // user has access but analyzer should not be visible
      EXPECT_EQ(arangodb::rest::ResponseCode::FORBIDDEN, responce.responseCode());
    }
    {
      TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                            "FooDb2");
      auto requestPtr = std::make_unique<GeneralRequestMock>(vocbase);
      auto& request = *requestPtr;
      auto responcePtr = std::make_unique<GeneralResponseMock>();
      auto& responce = *responcePtr;
      arangodb::iresearch::RestAnalyzerHandler handler(requestPtr.release(),
                                                       responcePtr.release());
      request.setRequestType(arangodb::rest::RequestType::GET);
      request.addSuffix(arangodb::StaticStrings::SystemDatabase +
                        "::testAnalyzer1");
      auto status = handler.execute();
      EXPECT_TRUE((arangodb::RestStatus::DONE == status));
      // system should be visible
      EXPECT_EQ(arangodb::rest::ResponseCode::OK, responce.responseCode());
    }
    {
      TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                            "FooDb2");
      auto requestPtr = std::make_unique<GeneralRequestMock>(vocbase);
      auto& request = *requestPtr;
      auto responcePtr = std::make_unique<GeneralResponseMock>();
      auto& responce = *responcePtr;
      arangodb::iresearch::RestAnalyzerHandler handler(requestPtr.release(),
                                                       responcePtr.release());
      request.setRequestType(arangodb::rest::RequestType::GET);
      request.addSuffix("::testAnalyzer1");
      auto status = handler.execute();
      EXPECT_TRUE((arangodb::RestStatus::DONE == status));
      // system should be visible
      EXPECT_EQ(arangodb::rest::ResponseCode::OK, responce.responseCode());
    }
  }
}

TEST_F(RestAnalyzerHandlerTest, test_get_known_not_authorized) {

  createAnalyzers();

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

  // get custom (known analyzer) not authorized
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                          arangodb::StaticStrings::SystemDatabase);
    auto requestPtr = std::make_unique<GeneralRequestMock>(vocbase);
    auto& request = *requestPtr;
    auto responcePtr = std::make_unique<GeneralResponseMock>();
    auto& responce = *responcePtr;
    arangodb::iresearch::RestAnalyzerHandler handler(requestPtr.release(),
                                                     responcePtr.release());
    request.setRequestType(arangodb::rest::RequestType::GET);
    request.addSuffix("testAnalyzer1");

    arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
    auto& user =
        userMap
            .emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP))
            .first->second;
    user.grantDatabase(vocbase.name(), arangodb::auth::Level::NONE);  // for system collections User::collectionAuthLevel(...) returns database auth::Level
    userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database

    auto status = handler.execute();
    EXPECT_TRUE((arangodb::RestStatus::DONE == status));
    EXPECT_EQ(arangodb::rest::ResponseCode::FORBIDDEN, responce.responseCode());
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

TEST_F(RestAnalyzerHandlerTest, test_get_unknown_authorized) {

  createAnalyzers();

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


  // get custom (unknown analyzer) authorized
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                          arangodb::StaticStrings::SystemDatabase);
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
}

TEST_F(RestAnalyzerHandlerTest, test_get_unknown_not_authorized) {

  createAnalyzers();

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



  // get custom (unknown analyzer) not authorized
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                          arangodb::StaticStrings::SystemDatabase);
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

TEST_F(RestAnalyzerHandlerTest, test_get_unknown_analyzer_unknown_vocbase_authorized) {

  createAnalyzers();

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
  // get custom (unknown analyzer, unknown vocbase) authorized
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                          "unknownVocbase");
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
}

TEST_F(RestAnalyzerHandlerTest, test_get_unknown_analyzer_unknown_vocbase_not_authorized) {

  createAnalyzers();

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

  // get custom (unknown analyzer, unknown vocbase) not authorized
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                          "unknownVocbase");
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

TEST_F(RestAnalyzerHandlerTest, test_list_system_database) {
  arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;

  auto* authFeature = arangodb::AuthenticationFeature::instance();
  auto* userManager = authFeature->userManager();
  arangodb::aql::QueryRegistry queryRegistry(0);  // required for UserManager::loadFromDB()
  userManager->setQueryRegistry(&queryRegistry);

  auto const databases = arangodb::velocypack::Parser::fromJson(
    std::string("[ { \"name\": \"testVocbase\" } ]"));

  ASSERT_TRUE((TRI_ERROR_NO_ERROR == dbFeature->loadDatabases(databases->slice())));

  arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
  auto& user =
    userMap
    .emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP))
    .first->second;
  user.grantDatabase("testVocbase", arangodb::auth::Level::RW);
  user.grantDatabase(arangodb::StaticStrings::SystemDatabase, arangodb::auth::Level::RO);
  userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database
  arangodb::Result res;
  std::tie(res, std::ignore) = arangodb::methods::Collections::createSystem(
    *dbFeature->useDatabase("testVocbase"), arangodb::tests::AnalyzerCollectionName, false);
  ASSERT_TRUE(res.ok());

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

  // system database (authorised)
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                          arangodb::StaticStrings::SystemDatabase);
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
        "identity", "text_de",
        "text_en",  "text_es",
        "text_fi",  "text_fr",
        "text_it",  "text_nl",
        "text_no",  "text_pt",
        "text_ru",  "text_sv",
        "text_zh",  arangodb::StaticStrings::SystemDatabase + "::testAnalyzer1",
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


TEST_F(RestAnalyzerHandlerTest, test_list_system_database_not_authorized) {
  arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;

  auto* authFeature = arangodb::AuthenticationFeature::instance();
  auto* userManager = authFeature->userManager();
  arangodb::aql::QueryRegistry queryRegistry(0);  // required for UserManager::loadFromDB()
  userManager->setQueryRegistry(&queryRegistry);

  auto const databases = arangodb::velocypack::Parser::fromJson(
    std::string("[ { \"name\": \"testVocbase\" } ]"));

  ASSERT_TRUE((TRI_ERROR_NO_ERROR == dbFeature->loadDatabases(databases->slice())));

  arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
  auto& user =
    userMap
    .emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP))
    .first->second;
  user.grantDatabase("testVocbase", arangodb::auth::Level::RW);
  user.grantDatabase(arangodb::StaticStrings::SystemDatabase, arangodb::auth::Level::RO);
  userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database
  arangodb::Result res;
  std::tie(res, std::ignore) = arangodb::methods::Collections::createSystem(
    *dbFeature->useDatabase("testVocbase"), arangodb::tests::AnalyzerCollectionName, false);
  ASSERT_TRUE(res.ok());

  ASSERT_TRUE((analyzers
                   ->emplace(result, arangodb::StaticStrings::SystemDatabase + "::testAnalyzer1",
                             "identity", VPackSlice::noneSlice())
                   .ok()));
  ASSERT_TRUE((analyzers
                   ->emplace(result, "testVocbase::testAnalyzer2", "identity",
                             VPackSlice::noneSlice())
                   .ok()));


  // system database (not authorised)
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                          arangodb::StaticStrings::SystemDatabase);
    auto requestPtr = std::make_unique<GeneralRequestMock>(vocbase);
    auto& request = *requestPtr;
    auto responcePtr = std::make_unique<GeneralResponseMock>();
    auto& responce = *responcePtr;
    arangodb::iresearch::RestAnalyzerHandler handler(requestPtr.release(),
                                                     responcePtr.release());
    request.setRequestType(arangodb::rest::RequestType::GET);

    struct ExecContext : public arangodb::ExecContext {
      ExecContext()
        : arangodb::ExecContext(arangodb::ExecContext::Type::Default, "", "",
                                arangodb::auth::Level::NONE,
                                arangodb::auth::Level::NONE) {}
    } execContext;
    arangodb::ExecContextScope execContextScope(&execContext);


    arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
    auto& user =
        userMap
            .emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP))
            .first->second;
    user.grantDatabase(vocbase.name(), arangodb::auth::Level::NONE);  // for system collections User::collectionAuthLevel(...) returns database auth::Level
    userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database

    std::set<std::string> expected = {
        "identity", "text_de", "text_en", "text_es", "text_fi",
        "text_fr",  "text_it", "text_nl", "text_no", "text_pt",
        "text_ru",  "text_sv", "text_zh",
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

TEST_F(RestAnalyzerHandlerTest, test_list_non_system_database_authorized) {
  arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;

  auto* authFeature = arangodb::AuthenticationFeature::instance();
  auto* userManager = authFeature->userManager();
  arangodb::aql::QueryRegistry queryRegistry(0);  // required for UserManager::loadFromDB()
  userManager->setQueryRegistry(&queryRegistry);

  auto const databases = arangodb::velocypack::Parser::fromJson(
    std::string("[ { \"name\": \"testVocbase\" } ]"));

  ASSERT_TRUE((TRI_ERROR_NO_ERROR == dbFeature->loadDatabases(databases->slice())));

  arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
  auto& user =
    userMap
    .emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP))
    .first->second;
  user.grantDatabase("testVocbase", arangodb::auth::Level::RW);
  user.grantDatabase(arangodb::StaticStrings::SystemDatabase, arangodb::auth::Level::RO);
  userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database
  arangodb::Result res;
  std::tie(res, std::ignore) = arangodb::methods::Collections::createSystem(
    *dbFeature->useDatabase("testVocbase"), arangodb::tests::AnalyzerCollectionName, false);
  ASSERT_TRUE(res.ok());

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

  // non-system database (authorised, system authorised)
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                          "testVocbase");
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

TEST_F(RestAnalyzerHandlerTest, test_list_non_system_database_not_authorized) {
  arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;

  auto* authFeature = arangodb::AuthenticationFeature::instance();
  auto* userManager = authFeature->userManager();
  arangodb::aql::QueryRegistry queryRegistry(0);  // required for UserManager::loadFromDB()
  userManager->setQueryRegistry(&queryRegistry);

  auto const databases = arangodb::velocypack::Parser::fromJson(
    std::string("[ { \"name\": \"testVocbase\" } ]"));

  ASSERT_TRUE((TRI_ERROR_NO_ERROR == dbFeature->loadDatabases(databases->slice())));

  arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
  auto& user =
    userMap
    .emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP))
    .first->second;
  user.grantDatabase("testVocbase", arangodb::auth::Level::RW);
  user.grantDatabase(arangodb::StaticStrings::SystemDatabase, arangodb::auth::Level::RO);
  userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database
  arangodb::Result res;
  std::tie(res, std::ignore) = arangodb::methods::Collections::createSystem(
    *dbFeature->useDatabase("testVocbase"), arangodb::tests::AnalyzerCollectionName, false);
  ASSERT_TRUE(res.ok());

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


  // non-system database (not authorised, system authorised)
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                          "testVocbase");
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
        "identity", "text_de",
        "text_en",  "text_es",
        "text_fi",  "text_fr",
        "text_it",  "text_nl",
        "text_no",  "text_pt",
        "text_ru",  "text_sv",
        "text_zh",  arangodb::StaticStrings::SystemDatabase + "::testAnalyzer1",
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

TEST_F(RestAnalyzerHandlerTest, test_list_non_system_database_system_not_authorized) {
  arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;

  auto* authFeature = arangodb::AuthenticationFeature::instance();
  auto* userManager = authFeature->userManager();
  arangodb::aql::QueryRegistry queryRegistry(0);  // required for UserManager::loadFromDB()
  userManager->setQueryRegistry(&queryRegistry);

  auto const databases = arangodb::velocypack::Parser::fromJson(
    std::string("[ { \"name\": \"testVocbase\" } ]"));

  ASSERT_TRUE((TRI_ERROR_NO_ERROR == dbFeature->loadDatabases(databases->slice())));

  arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
  auto& user =
    userMap
    .emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP))
    .first->second;
  user.grantDatabase("testVocbase", arangodb::auth::Level::RW);
  user.grantDatabase(arangodb::StaticStrings::SystemDatabase, arangodb::auth::Level::RO);
  userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database
  arangodb::Result res;
  std::tie(res, std::ignore) = arangodb::methods::Collections::createSystem(
    *dbFeature->useDatabase("testVocbase"), arangodb::tests::AnalyzerCollectionName, false);
  ASSERT_TRUE(res.ok());

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

  // non-system database (authorised, system not authorised)
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                          "testVocbase");
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
        "identity", "text_de",
        "text_en",  "text_es",
        "text_fi",  "text_fr",
        "text_it",  "text_nl",
        "text_no",  "text_pt",
        "text_ru",  "text_sv",
        "text_zh",  "testVocbase::testAnalyzer2",
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

TEST_F(RestAnalyzerHandlerTest, test_list_non_system_database_system_not_authorized_not_authorized) {
  arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;

  auto* authFeature = arangodb::AuthenticationFeature::instance();
  auto* userManager = authFeature->userManager();
  arangodb::aql::QueryRegistry queryRegistry(0);  // required for UserManager::loadFromDB()
  userManager->setQueryRegistry(&queryRegistry);

  auto const databases = arangodb::velocypack::Parser::fromJson(
    std::string("[ { \"name\": \"testVocbase\" } ]"));

  ASSERT_TRUE((TRI_ERROR_NO_ERROR == dbFeature->loadDatabases(databases->slice())));

  arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
  auto& user =
    userMap
    .emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP))
    .first->second;
  user.grantDatabase("testVocbase", arangodb::auth::Level::RW);
  user.grantDatabase(arangodb::StaticStrings::SystemDatabase, arangodb::auth::Level::RO);
  userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database
  arangodb::Result res;
  std::tie(res, std::ignore) = arangodb::methods::Collections::createSystem(
    *dbFeature->useDatabase("testVocbase"), arangodb::tests::AnalyzerCollectionName, false);
  ASSERT_TRUE(res.ok());

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


  // non-system database (not authorised, system not authorised)
  {
    TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1,
                          "testVocbase");
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
        "identity", "text_de", "text_en", "text_es", "text_fi",
        "text_fr",  "text_it", "text_nl", "text_no", "text_pt",
        "text_ru",  "text_sv", "text_zh",
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

// invalid params (no name)
TEST_F(RestAnalyzerHandlerTest, test_remove_invalid_params) {
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

  TRI_vocbase_t& vocbase = server.getSystemDatabase();

  {
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
}

// unknown analyzer
TEST_F(RestAnalyzerHandlerTest, test_remove_unknown_analyzer) {
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
  TRI_vocbase_t& vocbase = server.getSystemDatabase();

  {
    auto requestPtr = std::make_unique<GeneralRequestMock>(vocbase);
    auto& request = *requestPtr;
    auto responcePtr = std::make_unique<GeneralResponseMock>();
    auto& responce = *responcePtr;
    arangodb::iresearch::RestAnalyzerHandler handler(requestPtr.release(),
                                                     responcePtr.release());
    request.setRequestType(arangodb::rest::RequestType::DELETE_REQ);
    request.addSuffix("unknown");

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
}

// not authorised
TEST_F(RestAnalyzerHandlerTest, test_remove_not_authorized) {
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
  TRI_vocbase_t& vocbase = server.getSystemDatabase();



  {
    auto requestPtr = std::make_unique<GeneralRequestMock>(vocbase);
    auto& request = *requestPtr;
    auto responcePtr = std::make_unique<GeneralResponseMock>();
    auto& responce = *responcePtr;
    arangodb::iresearch::RestAnalyzerHandler handler(requestPtr.release(),
                                                     responcePtr.release());
    request.setRequestType(arangodb::rest::RequestType::DELETE_REQ);
    request.addSuffix("testAnalyzer1");

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
}

// still in use (fail)
TEST_F(RestAnalyzerHandlerTest, test_remove_still_in_use) {
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
  TRI_vocbase_t& vocbase = server.getSystemDatabase();


  {
    auto requestPtr = std::make_unique<GeneralRequestMock>(vocbase);
    auto& request = *requestPtr;
    auto responcePtr = std::make_unique<GeneralResponseMock>();
    auto& responce = *responcePtr;
    arangodb::iresearch::RestAnalyzerHandler handler(requestPtr.release(),
                                                     responcePtr.release());
    request.setRequestType(arangodb::rest::RequestType::DELETE_REQ);
    request.addSuffix("testAnalyzer2");
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
}

// still in use + force (success)
TEST_F(RestAnalyzerHandlerTest, test_remove_still_in_use_force) {
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
  TRI_vocbase_t& vocbase = server.getSystemDatabase();

  {
    auto requestPtr = std::make_unique<GeneralRequestMock>(vocbase);
    auto& request = *requestPtr;
    auto responcePtr = std::make_unique<GeneralResponseMock>();
    auto& responce = *responcePtr;
    arangodb::iresearch::RestAnalyzerHandler handler(requestPtr.release(),
                                                     responcePtr.release());
    request.setRequestType(arangodb::rest::RequestType::DELETE_REQ);
    request.addSuffix("testAnalyzer2");
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
    EXPECT_EQ(analyzer, nullptr);
  }
}

// removal with db name in analyzer name
TEST_F(RestAnalyzerHandlerTest, test_remove_invalid_name) {
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
  TRI_vocbase_t& vocbase = server.getSystemDatabase();

  {
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
    EXPECT_EQ(arangodb::rest::ResponseCode::OK, responce.responseCode());
    auto analyzer = analyzers->get(arangodb::StaticStrings::SystemDatabase +
                                   "::testAnalyzer1");
    EXPECT_EQ(nullptr, analyzer);
  }
}

TEST_F(RestAnalyzerHandlerTest, test_remove_success) {
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
  TRI_vocbase_t& vocbase = server.getSystemDatabase();


  // success removal
  {
    auto requestPtr = std::make_unique<GeneralRequestMock>(vocbase);
    auto& request = *requestPtr;
    auto responcePtr = std::make_unique<GeneralResponseMock>();
    auto& responce = *responcePtr;
    arangodb::iresearch::RestAnalyzerHandler handler(requestPtr.release(),
                                                     responcePtr.release());
    request.setRequestType(arangodb::rest::RequestType::DELETE_REQ);
    request.addSuffix("testAnalyzer1");

    arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
    auto& user =
        userMap
            .emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP))
            .first->second;
    user.grantDatabase(vocbase.name(), arangodb::auth::Level::RW);  // for system collections User::collectionAuthLevel(...) returns database auth::Level
    userManager->setAuthInfo(userMap);  // set user map to avoid loading configuration from system database

    auto status = handler.execute();
    EXPECT_TRUE((arangodb::RestStatus::DONE == status));
    EXPECT_EQ(arangodb::rest::ResponseCode::OK, responce.responseCode());
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
