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

#include "gtest/gtest.h"

#include "velocypack/Iterator.h"
#include "velocypack/Parser.h"

#include "analysis/analyzers.hpp"
#include "analysis/token_attributes.hpp"

#include "IResearch/RestHandlerMock.h"
#include "IResearch/common.h"
#include "Mocks/LogLevels.h"
#include "Mocks/Servers.h"
#include "Mocks/StorageEngineMock.h"

#include "Aql/QueryRegistry.h"
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
#include "VocBase/LogicalCollection.h"
#include "VocBase/Methods/Collections.h"

#if USE_ENTERPRISE
#include "Enterprise/Ldap/LdapFeature.h"
#endif

#define ASSERT_ARANGO_OK(x) \
  { ASSERT_TRUE(x.ok()) << x.errorMessage(); }

using namespace std::literals::string_literals;

namespace {

class EmptyAnalyzer : public irs::analysis::analyzer {
 public:
  static constexpr irs::string_ref type_name() noexcept {
    return "rest-analyzer-empty";
  }
  EmptyAnalyzer() : irs::analysis::analyzer(irs::type<EmptyAnalyzer>::get()) { }
  virtual irs::attribute* get_mutable(irs::type_info::type_id type) noexcept override {
    if (type == irs::type<irs::frequency>::id()) {
      return &_attr;
    }

    return nullptr;
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
  irs::frequency _attr;
};

REGISTER_ANALYZER_VPACK(EmptyAnalyzer, EmptyAnalyzer::make, EmptyAnalyzer::normalize);

}  // namespace

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

class RestAnalyzerHandlerTest
    : public ::testing::Test,
      public arangodb::tests::LogSuppressor<arangodb::Logger::AUTHENTICATION, arangodb::LogLevel::ERR> {
 protected:
  arangodb::tests::mocks::MockAqlServer server;
  TRI_vocbase_t& _system_vocbase;

  arangodb::iresearch::IResearchAnalyzerFeature& analyzers;
  arangodb::DatabaseFeature& dbFeature;
  arangodb::AuthenticationFeature& authFeature;
  arangodb::auth::UserManager* userManager;

  struct ExecContext : public arangodb::ExecContext {
    ExecContext()
        : arangodb::ExecContext(arangodb::ExecContext::Type::Default, "", "",
                                arangodb::auth::Level::NONE,
                                arangodb::auth::Level::NONE, false) {}
  } execContext;
  arangodb::ExecContextScope execContextScope;  // (&execContext);

  RestAnalyzerHandlerTest()
      : server(),
        _system_vocbase(server.getSystemDatabase()),
        analyzers(server.getFeature<arangodb::iresearch::IResearchAnalyzerFeature>()),
        dbFeature(server.getFeature<arangodb::DatabaseFeature>()),
        authFeature(server.getFeature<arangodb::AuthenticationFeature>()),
        userManager(authFeature.userManager()),
        execContext(),
        execContextScope(&execContext) {
    grantOnDb(arangodb::StaticStrings::SystemDatabase, arangodb::auth::Level::RW);

    // TODO: This should at the very least happen in the mock
    // create system vocbase
    {
      std::shared_ptr<arangodb::LogicalCollection> unused;
      arangodb::methods::Collections::createSystem(server.getSystemDatabase(),
                                                   arangodb::tests::AnalyzerCollectionName,
                                                   false,
                                                   unused);
    }
    createAnalyzers();

    grantOnDb(arangodb::StaticStrings::SystemDatabase, arangodb::auth::Level::NONE);
  }

  // Creates the analyzers that are used in all the tests
  void createAnalyzers() {
    arangodb::Result res;
    std::string name;
    arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;

    name = arangodb::StaticStrings::SystemDatabase + "::testAnalyzer1";

    ASSERT_ARANGO_OK(
        analyzers.emplace(result, name, "identity"s,
                           VPackParser::fromJson("{\"args\":\"abc\"}"s)->slice()));

    name = arangodb::StaticStrings::SystemDatabase + "::testAnalyzer2";
    ASSERT_ARANGO_OK(
        analyzers.emplace(result, name, "identity"s,
                           VPackParser::fromJson("{\"args\":\"abc\"}"s)->slice()));

    name = arangodb::StaticStrings::SystemDatabase + "::emptyAnalyzer"s;
    ASSERT_ARANGO_OK(
        analyzers.emplace(result, name, "rest-analyzer-empty"s,
                           VPackParser::fromJson("{\"args\":\"en\"}"s)->slice(),
                           irs::flags{irs::type<irs::frequency>::get()}));
  };

  // Creates a new database
  void createDatabase(std::string const& name) {
    arangodb::iresearch::IResearchAnalyzerFeature::EmplaceResult result;
    auto const databases = arangodb::velocypack::Parser::fromJson(
        std::string("[ { \"name\": \"" + name + "\" } ]"));
    ASSERT_EQ(TRI_ERROR_NO_ERROR, dbFeature.loadDatabases(databases->slice()));

    grantOnDb({{name, arangodb::auth::Level::RW},
               {arangodb::StaticStrings::SystemDatabase, arangodb::auth::Level::RW}});

    std::shared_ptr<arangodb::LogicalCollection> ignored;
    arangodb::Result res = arangodb::methods::Collections::createSystem(*dbFeature.useDatabase(name),
                                                     arangodb::tests::AnalyzerCollectionName,
                                                     false, ignored);

    ASSERT_TRUE(res.ok());

    ASSERT_ARANGO_OK(analyzers.emplace(result, name + "::testAnalyzer1",
                                        "identity"s, VPackSlice::noneSlice()));
    ASSERT_ARANGO_OK(analyzers.emplace(result, name + "::testAnalyzer2",
                                        "identity"s, VPackSlice::noneSlice()));
  }

  // Grant permissions on one DB
  // NOTE that permissions are always overwritten.
  void grantOnDb(std::string const& dbName, arangodb::auth::Level const& level) {
    arangodb::auth::UserMap userMap;  // empty map, no user -> no permissions
    auto& user =
        userMap
            .emplace("", arangodb::auth::User::newUser("", "", arangodb::auth::Source::LDAP))
            .first->second;

    // for system collections User::collectionAuthLevel(...) returns database auth::Level
    user.grantDatabase(dbName, level);
    // set user map to avoid loading configuration from system database
    userManager->setAuthInfo(userMap);
  }

  // Grant permissions on multiple DBs
  // NOTE that permissions are always overwritten.
  void grantOnDb(std::vector<std::pair<std::string const&, arangodb::auth::Level const&>> grants) {
    arangodb::auth::UserMap userMap;
    auto& user =
        userMap
            .emplace("", arangodb::auth::User::newUser(""s, ""s, arangodb::auth::Source::LDAP))
            .first->second;

    for (auto& g : grants) {
      user.grantDatabase(g.first, g.second);
    }
    userManager->setAuthInfo(userMap);
  }
};

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

// invalid params (non-object body)
TEST_F(RestAnalyzerHandlerTest, test_create_non_object_body) {
  grantOnDb(arangodb::StaticStrings::SystemDatabase, arangodb::auth::Level::RW);

  auto requestPtr = std::make_unique<GeneralRequestMock>(_system_vocbase);
  auto& request = *requestPtr;

  auto responcePtr = std::make_unique<GeneralResponseMock>();
  auto& responce = *responcePtr;

  arangodb::iresearch::RestAnalyzerHandler handler(server.server(),
                                                   requestPtr.release(),
                                                   responcePtr.release());
  request.setRequestType(arangodb::rest::RequestType::POST);
  request._payload.openArray();
  request._payload.close();

  auto status = handler.execute();
  EXPECT_EQ(arangodb::RestStatus::DONE, status);
  EXPECT_EQ(arangodb::rest::ResponseCode::BAD, responce.responseCode());
  auto slice = responce._payload.slice();
  EXPECT_TRUE(slice.isObject());
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
  grantOnDb(arangodb::StaticStrings::SystemDatabase, arangodb::auth::Level::RW);

  auto requestPtr = std::make_unique<GeneralRequestMock>(_system_vocbase);
  auto& request = *requestPtr;
  auto responcePtr = std::make_unique<GeneralResponseMock>();
  auto& responce = *responcePtr;
  arangodb::iresearch::RestAnalyzerHandler handler(server.server(), requestPtr.release(),
                                                   responcePtr.release());
  request.setRequestType(arangodb::rest::RequestType::POST);
  request._payload.openObject();
  request._payload.add("type", arangodb::velocypack::Value("identity"));
  request._payload.add("properties", arangodb::velocypack::Value(
                                         arangodb::velocypack::ValueType::Null));
  request._payload.close();

  auto status = handler.execute();
  EXPECT_EQ(arangodb::RestStatus::DONE, status);
  EXPECT_EQ(arangodb::rest::ResponseCode::BAD, responce.responseCode());
  auto slice = responce._payload.slice();
  EXPECT_TRUE(slice.isObject());
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
  grantOnDb(arangodb::StaticStrings::SystemDatabase, arangodb::auth::Level::RW);

  auto requestPtr = std::make_unique<GeneralRequestMock>(_system_vocbase);
  auto& request = *requestPtr;
  auto responcePtr = std::make_unique<GeneralResponseMock>();
  auto& responce = *responcePtr;
  arangodb::iresearch::RestAnalyzerHandler handler(server.server(), requestPtr.release(),
                                                   responcePtr.release());
  request.setRequestType(arangodb::rest::RequestType::POST);
  request._payload.openObject();
  request._payload.add("name", VPackValue("unknownVocbase::testAnalyzer"));
  request._payload.add("type", VPackValue("identity"));
  request._payload.add("properties", VPackValue(arangodb::velocypack::ValueType::Null));
  request._payload.close();

  auto status = handler.execute();
  EXPECT_EQ(arangodb::RestStatus::DONE, status);
  EXPECT_EQ(arangodb::rest::ResponseCode::FORBIDDEN, responce.responseCode());
  auto slice = responce._payload.slice();
  EXPECT_TRUE(slice.isObject());
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

TEST_F(RestAnalyzerHandlerTest, test_create_invalid_symbols) {
  grantOnDb(arangodb::StaticStrings::SystemDatabase, arangodb::auth::Level::RW);

  auto requestPtr = std::make_unique<GeneralRequestMock>(_system_vocbase);
  auto& request = *requestPtr;
  auto responcePtr = std::make_unique<GeneralResponseMock>();
  auto& responce = *responcePtr;
  arangodb::iresearch::RestAnalyzerHandler handler(server.server(), requestPtr.release(),
                                                   responcePtr.release());
  request.setRequestType(arangodb::rest::RequestType::POST);
  request._payload.openObject();
  request._payload.add("name", VPackValue(arangodb::StaticStrings::SystemDatabase + "::testAnalyzer"));
  request._payload.add("type", VPackValue("identity"));
  request._payload.add("properties", VPackValue(arangodb::velocypack::ValueType::Null));
  request._payload.close();

  auto status = handler.execute();
  EXPECT_EQ(arangodb::RestStatus::DONE, status);
  EXPECT_EQ(arangodb::rest::ResponseCode::BAD, responce.responseCode());
  auto slice = responce._payload.slice();
  EXPECT_TRUE(slice.isObject());
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

// TODO: is this the smae test as above?
TEST_F(RestAnalyzerHandlerTest, test_create_invalid_symbols_2) {
  grantOnDb(arangodb::StaticStrings::SystemDatabase, arangodb::auth::Level::RW);

  auto requestPtr = std::make_unique<GeneralRequestMock>(_system_vocbase);
  auto& request = *requestPtr;
  auto responcePtr = std::make_unique<GeneralResponseMock>();
  auto& responce = *responcePtr;
  arangodb::iresearch::RestAnalyzerHandler handler(server.server(), requestPtr.release(),
                                                   responcePtr.release());
  request.setRequestType(arangodb::rest::RequestType::POST);
  request._payload.openObject();
  request._payload.add("name", VPackValue("::testAnalyzer"));
  request._payload.add("type", VPackValue("identity"));
  request._payload.add("properties", VPackValue(arangodb::velocypack::ValueType::Null));
  request._payload.close();

  auto status = handler.execute();
  EXPECT_EQ(arangodb::RestStatus::DONE, status);
  EXPECT_EQ(arangodb::rest::ResponseCode::BAD, responce.responseCode());
  auto slice = responce._payload.slice();
  EXPECT_TRUE(slice.isObject());
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

TEST_F(RestAnalyzerHandlerTest, test_create_name_collision) {
  grantOnDb(arangodb::StaticStrings::SystemDatabase, arangodb::auth::Level::RW);

  auto requestPtr = std::make_unique<GeneralRequestMock>(_system_vocbase);
  auto& request = *requestPtr;
  auto responcePtr = std::make_unique<GeneralResponseMock>();
  auto& responce = *responcePtr;
  arangodb::iresearch::RestAnalyzerHandler handler(server.server(), requestPtr.release(),
                                                   responcePtr.release());
  request.setRequestType(arangodb::rest::RequestType::POST);
  request._payload.openObject();
  request._payload.add("name", arangodb::velocypack::Value("emptyAnalyzer"));
  request._payload.add("type",
                       arangodb::velocypack::Value("rest-analyzer-empty"));
  request._payload.add("properties",
                       arangodb::velocypack::Value("{\"args\":\"abc\"}"));
  request._payload.close();

  auto status = handler.execute();
  EXPECT_EQ(arangodb::RestStatus::DONE, status);
  EXPECT_EQ(arangodb::rest::ResponseCode::BAD, responce.responseCode());
  auto slice = responce._payload.slice();
  EXPECT_TRUE(slice.isObject());
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

TEST_F(RestAnalyzerHandlerTest, test_create_duplicate_matching) {
  grantOnDb(arangodb::StaticStrings::SystemDatabase, arangodb::auth::Level::RW);

  auto requestPtr = std::make_unique<GeneralRequestMock>(_system_vocbase);
  auto& request = *requestPtr;
  auto responcePtr = std::make_unique<GeneralResponseMock>();
  auto& responce = *responcePtr;
  arangodb::iresearch::RestAnalyzerHandler handler(server.server(), requestPtr.release(),
                                                   responcePtr.release());
  request.setRequestType(arangodb::rest::RequestType::POST);
  request._payload.openObject();
  request._payload.add("name", VPackValue("testAnalyzer1"));
  request._payload.add("type", VPackValue("identity"));
  request._payload.add("properties", VPackSlice::noneSlice());
  request._payload.close();

  auto status = handler.execute();
  EXPECT_EQ(arangodb::RestStatus::DONE, status);
  EXPECT_EQ(arangodb::rest::ResponseCode::OK, responce.responseCode());
  auto slice = responce._payload.slice();
  EXPECT_TRUE(slice.isObject());
  EXPECT_TRUE((slice.hasKey("name") && slice.get("name").isString() &&
               arangodb::StaticStrings::SystemDatabase + "::testAnalyzer1" ==
                   slice.get("name").copyString()));
  EXPECT_TRUE(slice.hasKey("type") && slice.get("type").isString());
  EXPECT_TRUE(slice.hasKey("properties") && slice.get("properties").isObject());
  EXPECT_TRUE(slice.hasKey("features") && slice.get("features").isArray());
  auto analyzer = analyzers.get(arangodb::StaticStrings::SystemDatabase +
                                 "::testAnalyzer1", arangodb::QueryAnalyzerRevisions::QUERY_LATEST);
  EXPECT_NE(nullptr, analyzer);
}

TEST_F(RestAnalyzerHandlerTest, test_create_not_authorized) {
  grantOnDb(arangodb::StaticStrings::SystemDatabase, arangodb::auth::Level::RO);

  auto requestPtr = std::make_unique<GeneralRequestMock>(_system_vocbase);
  auto& request = *requestPtr;
  auto responcePtr = std::make_unique<GeneralResponseMock>();
  auto& responce = *responcePtr;
  arangodb::iresearch::RestAnalyzerHandler handler(server.server(), requestPtr.release(),
                                                   responcePtr.release());
  request.setRequestType(arangodb::rest::RequestType::POST);
  request._payload.openObject();
  request._payload.add("name", VPackValue("testAnalyzer2"));
  request._payload.add("type", VPackValue("identity"));
  request._payload.add("properties",
                       arangodb::velocypack::Value("{\"args\":\"abc\"}"));
  request._payload.close();

  auto status = handler.execute();
  EXPECT_EQ(arangodb::RestStatus::DONE, status);
  EXPECT_EQ(arangodb::rest::ResponseCode::FORBIDDEN, responce.responseCode());
  auto slice = responce._payload.slice();
  EXPECT_TRUE(slice.isObject());
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

TEST_F(RestAnalyzerHandlerTest, test_create_success) {
  grantOnDb(arangodb::StaticStrings::SystemDatabase, arangodb::auth::Level::RW);

  auto requestPtr = std::make_unique<GeneralRequestMock>(_system_vocbase);
  auto& request = *requestPtr;
  auto responcePtr = std::make_unique<GeneralResponseMock>();
  auto& responce = *responcePtr;
  arangodb::iresearch::RestAnalyzerHandler handler(server.server(), requestPtr.release(),
                                                   responcePtr.release());
  request.setRequestType(arangodb::rest::RequestType::POST);
  request._payload.openObject();
  request._payload.add("name", arangodb::velocypack::Value("testAnalyzer3"));
  request._payload.add("type", arangodb::velocypack::Value("identity"));
  request._payload.add("properties",
                       arangodb::velocypack::Value("{\"args\":\"abc\"}"));
  request._payload.close();

  auto status = handler.execute();
  EXPECT_EQ(arangodb::RestStatus::DONE, status);
  EXPECT_EQ(arangodb::rest::ResponseCode::CREATED, responce.responseCode());
  auto slice = responce._payload.slice();
  EXPECT_TRUE(slice.isObject());
  EXPECT_TRUE((slice.hasKey("name") && slice.get("name").isString() &&
               arangodb::StaticStrings::SystemDatabase + "::testAnalyzer3" ==
                   slice.get("name").copyString()));
  EXPECT_TRUE(slice.hasKey("type") && slice.get("type").isString());
  EXPECT_TRUE(slice.hasKey("properties") && slice.get("properties").isObject());
  EXPECT_TRUE(slice.hasKey("features") && slice.get("features").isArray());
  auto analyzer = analyzers.get(arangodb::StaticStrings::SystemDatabase +
                                 "::testAnalyzer1", arangodb::QueryAnalyzerRevisions::QUERY_LATEST);
  EXPECT_NE(nullptr, analyzer);
}

TEST_F(RestAnalyzerHandlerTest, test_get_static_known) {
  grantOnDb(arangodb::StaticStrings::SystemDatabase, arangodb::auth::Level::RO);

  auto requestPtr = std::make_unique<GeneralRequestMock>(_system_vocbase);
  auto& request = *requestPtr;
  auto responcePtr = std::make_unique<GeneralResponseMock>();
  auto& responce = *responcePtr;
  arangodb::iresearch::RestAnalyzerHandler handler(server.server(), requestPtr.release(),
                                                   responcePtr.release());
  request.setRequestType(arangodb::rest::RequestType::GET);
  request.addSuffix("identity");

  auto status = handler.execute();
  EXPECT_EQ(arangodb::RestStatus::DONE, status);
  EXPECT_EQ(arangodb::rest::ResponseCode::OK, responce.responseCode());
  auto slice = responce._payload.slice();
  EXPECT_TRUE(slice.isObject());
  EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::Code) &&
               slice.get(arangodb::StaticStrings::Code).isNumber<size_t>() &&
               size_t(arangodb::rest::ResponseCode::OK) ==
                   slice.get(arangodb::StaticStrings::Code).getNumber<size_t>()));
  EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::Error) &&
               slice.get(arangodb::StaticStrings::Error).isBoolean() &&
               false == slice.get(arangodb::StaticStrings::Error).getBoolean()));
  EXPECT_TRUE((slice.hasKey("name") && slice.get("name").isString() &&
               std::string("identity") == slice.get("name").copyString()));
  EXPECT_TRUE(slice.hasKey("type") && slice.get("type").isString());
  EXPECT_TRUE(slice.hasKey("properties") && slice.get("properties").isObject());
  EXPECT_TRUE(slice.hasKey("features") && slice.get("features").isArray());
}

TEST_F(RestAnalyzerHandlerTest, test_get_static_unknown) {
  grantOnDb(arangodb::StaticStrings::SystemDatabase, arangodb::auth::Level::RO);

  auto requestPtr = std::make_unique<GeneralRequestMock>(_system_vocbase);
  TRI_ASSERT(requestPtr != nullptr);
  auto& request = *requestPtr;
  auto responcePtr = std::make_unique<GeneralResponseMock>();
  TRI_ASSERT(responcePtr != nullptr);
  auto& responce = *responcePtr;
  arangodb::iresearch::RestAnalyzerHandler handler(server.server(), requestPtr.release(),
                                                   responcePtr.release());
  request.setRequestType(arangodb::rest::RequestType::GET);
  request.addSuffix("unknown");

  auto status = handler.execute();
  EXPECT_EQ(arangodb::RestStatus::DONE, status);
  EXPECT_EQ(arangodb::rest::ResponseCode::NOT_FOUND, responce.responseCode());
  auto slice = responce._payload.slice();
  EXPECT_TRUE(slice.isObject());
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

TEST_F(RestAnalyzerHandlerTest, test_get_known) {
  grantOnDb(arangodb::StaticStrings::SystemDatabase, arangodb::auth::Level::RO);

  auto requestPtr = std::make_unique<GeneralRequestMock>(_system_vocbase);
  auto& request = *requestPtr;
  auto responcePtr = std::make_unique<GeneralResponseMock>();
  auto& responce = *responcePtr;
  arangodb::iresearch::RestAnalyzerHandler handler(server.server(), requestPtr.release(),
                                                   responcePtr.release());
  request.setRequestType(arangodb::rest::RequestType::GET);
  request.addSuffix(arangodb::StaticStrings::SystemDatabase +
                    "::testAnalyzer1");

  auto status = handler.execute();
  EXPECT_EQ(arangodb::RestStatus::DONE, status);
  EXPECT_EQ(arangodb::rest::ResponseCode::OK, responce.responseCode());
  auto slice = responce._payload.slice();
  EXPECT_TRUE(slice.isObject());
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
  EXPECT_TRUE(slice.hasKey("type") && slice.get("type").isString());
  EXPECT_TRUE(slice.hasKey("properties") && slice.get("properties").isObject());
  EXPECT_TRUE(slice.hasKey("features") && slice.get("features").isArray());
}

// TODO: This test needs some love (i.e. probably splitting)
TEST_F(RestAnalyzerHandlerTest, test_get_custom) {
  createDatabase("FooDb"s);
  createDatabase("FooDb2"s);

  grantOnDb({{"FooDb"s, arangodb::auth::Level::RW},
             {"FooDb2"s, arangodb::auth::Level::RW},
             {arangodb::StaticStrings::SystemDatabase, arangodb::auth::Level::RO}});

  {
    TRI_vocbase_t& vocbase = *dbFeature.useDatabase("FooDb2");
    auto requestPtr = std::make_unique<GeneralRequestMock>(vocbase);
    auto& request = *requestPtr;
    auto responcePtr = std::make_unique<GeneralResponseMock>();
    auto& responce = *responcePtr;
    arangodb::iresearch::RestAnalyzerHandler handler(server.server(), requestPtr.release(),
                                                     responcePtr.release());
    request.setRequestType(arangodb::rest::RequestType::GET);
    request.addSuffix("FooDb::testAnalyzer1");

    auto status = handler.execute();
    EXPECT_EQ(arangodb::RestStatus::DONE, status);
    // user has access but analyzer should not be visible
    EXPECT_EQ(arangodb::rest::ResponseCode::FORBIDDEN, responce.responseCode());
  }
  {
    TRI_vocbase_t& vocbase = *dbFeature.useDatabase("FooDb2");
    auto requestPtr = std::make_unique<GeneralRequestMock>(vocbase);
    auto& request = *requestPtr;
    auto responcePtr = std::make_unique<GeneralResponseMock>();
    auto& responce = *responcePtr;
    arangodb::iresearch::RestAnalyzerHandler handler(server.server(), requestPtr.release(),
                                                     responcePtr.release());
    request.setRequestType(arangodb::rest::RequestType::GET);
    request.addSuffix(arangodb::StaticStrings::SystemDatabase +
                      "::testAnalyzer1");
    auto status = handler.execute();
    EXPECT_EQ(arangodb::RestStatus::DONE, status);
    // system should be visible
    EXPECT_EQ(arangodb::rest::ResponseCode::OK, responce.responseCode());
  }
  {
    TRI_vocbase_t& vocbase = *dbFeature.useDatabase("FooDb2");
    auto requestPtr = std::make_unique<GeneralRequestMock>(vocbase);
    auto& request = *requestPtr;
    auto responcePtr = std::make_unique<GeneralResponseMock>();
    auto& responce = *responcePtr;
    arangodb::iresearch::RestAnalyzerHandler handler(server.server(), requestPtr.release(),
                                                     responcePtr.release());
    request.setRequestType(arangodb::rest::RequestType::GET);
    request.addSuffix("::testAnalyzer1");
    auto status = handler.execute();
    EXPECT_EQ(arangodb::RestStatus::DONE, status);
    // system should be visible
    EXPECT_EQ(arangodb::rest::ResponseCode::OK, responce.responseCode());
  }
}

TEST_F(RestAnalyzerHandlerTest, test_get_known_not_authorized) {
  grantOnDb(arangodb::StaticStrings::SystemDatabase, arangodb::auth::Level::NONE);

  auto requestPtr = std::make_unique<GeneralRequestMock>(_system_vocbase);
  auto& request = *requestPtr;
  auto responcePtr = std::make_unique<GeneralResponseMock>();
  auto& responce = *responcePtr;
  arangodb::iresearch::RestAnalyzerHandler handler(server.server(), requestPtr.release(),
                                                   responcePtr.release());
  request.setRequestType(arangodb::rest::RequestType::GET);
  request.addSuffix("testAnalyzer1");

  auto status = handler.execute();
  EXPECT_EQ(arangodb::RestStatus::DONE, status);
  EXPECT_EQ(arangodb::rest::ResponseCode::FORBIDDEN, responce.responseCode());
  auto slice = responce._payload.slice();
  EXPECT_TRUE(slice.isObject());
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

TEST_F(RestAnalyzerHandlerTest, test_get_unknown_authorized) {
  grantOnDb(arangodb::StaticStrings::SystemDatabase, arangodb::auth::Level::RO);

  auto requestPtr = std::make_unique<GeneralRequestMock>(_system_vocbase);
  auto& request = *requestPtr;
  auto responcePtr = std::make_unique<GeneralResponseMock>();
  auto& responce = *responcePtr;
  arangodb::iresearch::RestAnalyzerHandler handler(server.server(), requestPtr.release(),
                                                   responcePtr.release());
  request.setRequestType(arangodb::rest::RequestType::GET);
  request.addSuffix("unknown");

  auto status = handler.execute();
  EXPECT_EQ(arangodb::RestStatus::DONE, status);
  EXPECT_EQ(arangodb::rest::ResponseCode::NOT_FOUND, responce.responseCode());
  auto slice = responce._payload.slice();
  EXPECT_TRUE(slice.isObject());
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

TEST_F(RestAnalyzerHandlerTest, test_get_unknown_not_authorized) {
  grantOnDb(arangodb::StaticStrings::SystemDatabase, arangodb::auth::Level::NONE);
  auto requestPtr = std::make_unique<GeneralRequestMock>(_system_vocbase);
  auto& request = *requestPtr;
  auto responcePtr = std::make_unique<GeneralResponseMock>();
  auto& responce = *responcePtr;
  arangodb::iresearch::RestAnalyzerHandler handler(server.server(), requestPtr.release(),
                                                   responcePtr.release());
  request.setRequestType(arangodb::rest::RequestType::GET);
  request.addSuffix("unknown");

  auto status = handler.execute();
  EXPECT_EQ(arangodb::RestStatus::DONE, status);
  EXPECT_EQ(arangodb::rest::ResponseCode::FORBIDDEN, responce.responseCode());
  auto slice = responce._payload.slice();
  EXPECT_TRUE(slice.isObject());
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

TEST_F(RestAnalyzerHandlerTest, test_get_unknown_analyzer_unknown_vocbase_authorized) {
  grantOnDb(arangodb::StaticStrings::SystemDatabase, arangodb::auth::Level::RO);

  // TODO
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, unknownDBInfo(server.server()));
  auto requestPtr = std::make_unique<GeneralRequestMock>(vocbase);
  auto& request = *requestPtr;
  auto responcePtr = std::make_unique<GeneralResponseMock>();
  auto& responce = *responcePtr;
  arangodb::iresearch::RestAnalyzerHandler handler(server.server(), requestPtr.release(),
                                                   responcePtr.release());
  request.setRequestType(arangodb::rest::RequestType::GET);
  request.addSuffix("unknown");

  auto status = handler.execute();
  EXPECT_EQ(arangodb::RestStatus::DONE, status);
  EXPECT_EQ(arangodb::rest::ResponseCode::NOT_FOUND, responce.responseCode());
  auto slice = responce._payload.slice();
  EXPECT_TRUE(slice.isObject());
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

TEST_F(RestAnalyzerHandlerTest, test_get_unknown_analyzer_unknown_vocbase_not_authorized) {
  grantOnDb(arangodb::StaticStrings::SystemDatabase, arangodb::auth::Level::NONE);

  // TODO
  TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, unknownDBInfo(server.server()));
  auto requestPtr = std::make_unique<GeneralRequestMock>(vocbase);
  auto& request = *requestPtr;
  auto responcePtr = std::make_unique<GeneralResponseMock>();
  auto& responce = *responcePtr;
  arangodb::iresearch::RestAnalyzerHandler handler(server.server(), requestPtr.release(),
                                                   responcePtr.release());
  request.setRequestType(arangodb::rest::RequestType::GET);
  request.addSuffix("unknown");

  auto status = handler.execute();
  EXPECT_EQ(arangodb::RestStatus::DONE, status);
  EXPECT_EQ(arangodb::rest::ResponseCode::FORBIDDEN, responce.responseCode());
  auto slice = responce._payload.slice();
  EXPECT_TRUE(slice.isObject());
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

TEST_F(RestAnalyzerHandlerTest, test_list_system_database_authorized) {
  grantOnDb(arangodb::StaticStrings::SystemDatabase, arangodb::auth::Level::RO);

  auto requestPtr = std::make_unique<GeneralRequestMock>(_system_vocbase);
  auto& request = *requestPtr;
  auto responcePtr = std::make_unique<GeneralResponseMock>();
  auto& responce = *responcePtr;
  arangodb::iresearch::RestAnalyzerHandler handler(server.server(), requestPtr.release(),
                                                   responcePtr.release());
  request.setRequestType(arangodb::rest::RequestType::GET);

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
      arangodb::StaticStrings::SystemDatabase + "::testAnalyzer2",
      arangodb::StaticStrings::SystemDatabase + "::emptyAnalyzer",
  };
  auto status = handler.execute();
  EXPECT_EQ(arangodb::RestStatus::DONE, status);
  EXPECT_EQ(arangodb::rest::ResponseCode::OK, responce.responseCode());
  auto slice = responce._payload.slice();
  EXPECT_TRUE(slice.isObject());
  EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::Code) &&
               slice.get(arangodb::StaticStrings::Code).isNumber<size_t>() &&
               size_t(arangodb::rest::ResponseCode::OK) ==
                   slice.get(arangodb::StaticStrings::Code).getNumber<size_t>()));
  EXPECT_TRUE((slice.hasKey(arangodb::StaticStrings::Error) &&
               slice.get(arangodb::StaticStrings::Error).isBoolean() &&
               false == slice.get(arangodb::StaticStrings::Error).getBoolean()));

  for (arangodb::velocypack::ArrayIterator itr(slice.get("result")); itr.valid(); ++itr) {
    auto subSlice = *itr;
    EXPECT_TRUE(subSlice.isObject());
    EXPECT_TRUE(subSlice.hasKey("name") && subSlice.get("name").isString());
    EXPECT_TRUE(subSlice.hasKey("type") && subSlice.get("type").isString());
    EXPECT_TRUE(
        (subSlice.hasKey("properties") && (subSlice.get("properties").isObject() ||
                                           subSlice.get("properties").isNull())));
    EXPECT_TRUE(subSlice.hasKey("features") && subSlice.get("features").isArray());
    EXPECT_EQ(1, expected.erase(subSlice.get("name").copyString()));
  }

  EXPECT_TRUE(expected.empty());
}

TEST_F(RestAnalyzerHandlerTest, test_list_system_database_not_authorized) {
  grantOnDb(arangodb::StaticStrings::SystemDatabase, arangodb::auth::Level::NONE);

  auto requestPtr = std::make_unique<GeneralRequestMock>(_system_vocbase);
  auto& request = *requestPtr;
  auto responcePtr = std::make_unique<GeneralResponseMock>();
  auto& responce = *responcePtr;
  arangodb::iresearch::RestAnalyzerHandler handler(server.server(), requestPtr.release(),
                                                   responcePtr.release());
  request.setRequestType(arangodb::rest::RequestType::GET);

  std::set<std::string> expected = {
      "identity", "text_de", "text_en", "text_es", "text_fi",
      "text_fr",  "text_it", "text_nl", "text_no", "text_pt",
      "text_ru",  "text_sv", "text_zh",
  };
  auto status = handler.execute();
  EXPECT_EQ(arangodb::RestStatus::DONE, status);
  EXPECT_EQ(arangodb::rest::ResponseCode::OK, responce.responseCode());
  auto slice = responce._payload.slice();
  EXPECT_TRUE(slice.isObject());
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
    EXPECT_TRUE(subSlice.isObject());
    EXPECT_TRUE(subSlice.hasKey("name") && subSlice.get("name").isString());
    EXPECT_TRUE(subSlice.hasKey("type") && subSlice.get("type").isString());
    EXPECT_TRUE(
        (subSlice.hasKey("properties") && (subSlice.get("properties").isObject() ||
                                           subSlice.get("properties").isNull())));
    EXPECT_TRUE(subSlice.hasKey("features") && subSlice.get("features").isArray());
    EXPECT_EQ(1, expected.erase(subSlice.get("name").copyString()));
  }

  EXPECT_TRUE(expected.empty());
}

TEST_F(RestAnalyzerHandlerTest, test_list_non_system_database_authorized) {
  createDatabase("testVocbase"s);

  arangodb::auth::UserMap userMap;

  grantOnDb({{"testVocbase"s, arangodb::auth::Level::RW},
             {arangodb::StaticStrings::SystemDatabase, arangodb::auth::Level::RO}});

  TRI_vocbase_t& vocbase = *dbFeature.useDatabase("testVocbase");

  auto requestPtr = std::make_unique<GeneralRequestMock>(vocbase);
  auto& request = *requestPtr;
  auto responcePtr = std::make_unique<GeneralResponseMock>();
  auto& responce = *responcePtr;
  arangodb::iresearch::RestAnalyzerHandler handler(server.server(), requestPtr.release(),
                                                   responcePtr.release());
  request.setRequestType(arangodb::rest::RequestType::GET);
  grantOnDb({{"testVocbase"s, arangodb::auth::Level::RO},
             {arangodb::StaticStrings::SystemDatabase, arangodb::auth::Level::RO}});
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
      "testVocbase::testAnalyzer1",
      "testVocbase::testAnalyzer2",
      arangodb::StaticStrings::SystemDatabase + "::testAnalyzer1",
      arangodb::StaticStrings::SystemDatabase + "::testAnalyzer2",
      arangodb::StaticStrings::SystemDatabase + "::emptyAnalyzer",
  };
  auto status = handler.execute();
  EXPECT_EQ(arangodb::RestStatus::DONE, status);
  EXPECT_EQ(arangodb::rest::ResponseCode::OK, responce.responseCode());
  auto slice = responce._payload.slice();
  EXPECT_TRUE(slice.isObject());
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
    EXPECT_TRUE(subSlice.isObject());
    EXPECT_TRUE(subSlice.hasKey("name") && subSlice.get("name").isString());
    EXPECT_TRUE(subSlice.hasKey("type") && subSlice.get("type").isString());
    EXPECT_TRUE(
        (subSlice.hasKey("properties") && (subSlice.get("properties").isObject() ||
                                           subSlice.get("properties").isNull())));
    EXPECT_TRUE(subSlice.hasKey("features") && subSlice.get("features").isArray());
    EXPECT_EQ(1, expected.erase(subSlice.get("name").copyString()));
  }

  EXPECT_TRUE(expected.empty());
}

TEST_F(RestAnalyzerHandlerTest, test_list_non_system_database_not_authorized) {
  grantOnDb(arangodb::StaticStrings::SystemDatabase, arangodb::auth::Level::RW);

  createDatabase("testVocbase"s);

  TRI_vocbase_t& vocbase = *dbFeature.useDatabase("testVocbase");
  auto requestPtr = std::make_unique<GeneralRequestMock>(vocbase);
  auto& request = *requestPtr;
  auto responcePtr = std::make_unique<GeneralResponseMock>();
  auto& responce = *responcePtr;
  arangodb::iresearch::RestAnalyzerHandler handler(server.server(), requestPtr.release(),
                                                   responcePtr.release());
  request.setRequestType(arangodb::rest::RequestType::GET);

  grantOnDb({{"testVocbase"s, arangodb::auth::Level::NONE},
             {arangodb::StaticStrings::SystemDatabase, arangodb::auth::Level::RO}});

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
      arangodb::StaticStrings::SystemDatabase + "::emptyAnalyzer",
      arangodb::StaticStrings::SystemDatabase + "::testAnalyzer1",
      arangodb::StaticStrings::SystemDatabase + "::testAnalyzer2",
  };
  auto status = handler.execute();
  EXPECT_EQ(arangodb::RestStatus::DONE, status);
  EXPECT_EQ(arangodb::rest::ResponseCode::OK, responce.responseCode());
  auto slice = responce._payload.slice();
  EXPECT_TRUE(slice.isObject());
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
    EXPECT_TRUE(subSlice.isObject());
    EXPECT_TRUE(subSlice.hasKey("name") && subSlice.get("name").isString());
    EXPECT_TRUE(subSlice.hasKey("type") && subSlice.get("type").isString());
    EXPECT_TRUE(
        (subSlice.hasKey("properties") && (subSlice.get("properties").isObject() ||
                                           subSlice.get("properties").isNull())));
    EXPECT_TRUE(subSlice.hasKey("features") && subSlice.get("features").isArray());
    EXPECT_EQ(1, expected.erase(subSlice.get("name").copyString()));
  }

  EXPECT_TRUE(expected.empty());
}

TEST_F(RestAnalyzerHandlerTest, test_list_non_system_database_system_not_authorized) {
  createDatabase("testVocbase"s);

  TRI_vocbase_t& vocbase = *dbFeature.useDatabase("testVocbase");
  auto requestPtr = std::make_unique<GeneralRequestMock>(vocbase);
  auto& request = *requestPtr;
  auto responcePtr = std::make_unique<GeneralResponseMock>();
  auto& responce = *responcePtr;
  arangodb::iresearch::RestAnalyzerHandler handler(server.server(), requestPtr.release(),
                                                   responcePtr.release());
  request.setRequestType(arangodb::rest::RequestType::GET);

  grantOnDb({{"testVocbase"s, arangodb::auth::Level::RO},
             {arangodb::StaticStrings::SystemDatabase, arangodb::auth::Level::NONE}});

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
      "testVocbase::testAnalyzer1",
      "testVocbase::testAnalyzer2",
  };
  auto status = handler.execute();
  EXPECT_EQ(arangodb::RestStatus::DONE, status);
  EXPECT_EQ(arangodb::rest::ResponseCode::OK, responce.responseCode());
  auto slice = responce._payload.slice();
  EXPECT_TRUE(slice.isObject());
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
    EXPECT_TRUE(subSlice.isObject());
    EXPECT_TRUE(subSlice.hasKey("name") && subSlice.get("name").isString());
    EXPECT_TRUE(subSlice.hasKey("type") && subSlice.get("type").isString());
    EXPECT_TRUE(
        (subSlice.hasKey("properties") && (subSlice.get("properties").isObject() ||
                                           subSlice.get("properties").isNull())));
    EXPECT_TRUE(subSlice.hasKey("features") && subSlice.get("features").isArray());
    EXPECT_EQ(1, expected.erase(subSlice.get("name").copyString()));
  }
  EXPECT_TRUE(expected.empty());
}

TEST_F(RestAnalyzerHandlerTest,
       test_list_non_system_database_system_not_authorized_not_authorized) {
  createDatabase("testVocbase"s);

  TRI_vocbase_t& vocbase = *dbFeature.useDatabase("testVocbase");

  auto requestPtr = std::make_unique<GeneralRequestMock>(vocbase);
  auto& request = *requestPtr;
  auto responcePtr = std::make_unique<GeneralResponseMock>();
  auto& responce = *responcePtr;
  arangodb::iresearch::RestAnalyzerHandler handler(server.server(), requestPtr.release(),
                                                   responcePtr.release());
  request.setRequestType(arangodb::rest::RequestType::GET);

  grantOnDb({{arangodb::StaticStrings::SystemDatabase, arangodb::auth::Level::NONE},
             {"testVocbase"s, arangodb::auth::Level::NONE}});

  std::set<std::string> expected = {
      "identity", "text_de", "text_en", "text_es", "text_fi",
      "text_fr",  "text_it", "text_nl", "text_no", "text_pt",
      "text_ru",  "text_sv", "text_zh",
  };
  auto status = handler.execute();
  EXPECT_EQ(arangodb::RestStatus::DONE, status);
  EXPECT_EQ(arangodb::rest::ResponseCode::OK, responce.responseCode());
  auto slice = responce._payload.slice();
  EXPECT_TRUE(slice.isObject());
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
    EXPECT_TRUE(subSlice.isObject());
    EXPECT_TRUE(subSlice.hasKey("name") && subSlice.get("name").isString());
    EXPECT_TRUE(subSlice.hasKey("type") && subSlice.get("type").isString());
    EXPECT_TRUE(
        (subSlice.hasKey("properties") && (subSlice.get("properties").isObject() ||
                                           subSlice.get("properties").isNull())));
    EXPECT_TRUE(subSlice.hasKey("features") && subSlice.get("features").isArray());
    EXPECT_EQ(1, expected.erase(subSlice.get("name").copyString()));
  }

  EXPECT_TRUE(expected.empty());
}

// invalid params (no name)
TEST_F(RestAnalyzerHandlerTest, test_remove_invalid_params) {
  grantOnDb(arangodb::StaticStrings::SystemDatabase, arangodb::auth::Level::RW);

  auto requestPtr = std::make_unique<GeneralRequestMock>(_system_vocbase);
  auto& request = *requestPtr;
  auto responcePtr = std::make_unique<GeneralResponseMock>();
  auto& responce = *responcePtr;
  arangodb::iresearch::RestAnalyzerHandler handler(server.server(), requestPtr.release(),
                                                   responcePtr.release());
  request.setRequestType(arangodb::rest::RequestType::DELETE_REQ);

  auto status = handler.execute();
  EXPECT_EQ(arangodb::RestStatus::DONE, status);
  EXPECT_EQ(arangodb::rest::ResponseCode::BAD, responce.responseCode());
  auto slice = responce._payload.slice();
  EXPECT_TRUE(slice.isObject());
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
TEST_F(RestAnalyzerHandlerTest, test_remove_unknown_analyzer) {
  grantOnDb(arangodb::StaticStrings::SystemDatabase, arangodb::auth::Level::RW);

  auto requestPtr = std::make_unique<GeneralRequestMock>(_system_vocbase);
  auto& request = *requestPtr;
  auto responcePtr = std::make_unique<GeneralResponseMock>();
  auto& responce = *responcePtr;
  arangodb::iresearch::RestAnalyzerHandler handler(server.server(), requestPtr.release(),
                                                   responcePtr.release());
  request.setRequestType(arangodb::rest::RequestType::DELETE_REQ);
  request.addSuffix("unknown");

  auto status = handler.execute();
  EXPECT_EQ(arangodb::RestStatus::DONE, status);
  EXPECT_EQ(arangodb::rest::ResponseCode::NOT_FOUND, responce.responseCode());
  auto slice = responce._payload.slice();
  EXPECT_TRUE(slice.isObject());
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
TEST_F(RestAnalyzerHandlerTest, test_remove_not_authorized) {
  grantOnDb(arangodb::StaticStrings::SystemDatabase, arangodb::auth::Level::RO);

  auto requestPtr = std::make_unique<GeneralRequestMock>(_system_vocbase);
  auto& request = *requestPtr;
  auto responcePtr = std::make_unique<GeneralResponseMock>();
  auto& responce = *responcePtr;
  arangodb::iresearch::RestAnalyzerHandler handler(server.server(), requestPtr.release(),
                                                   responcePtr.release());
  request.setRequestType(arangodb::rest::RequestType::DELETE_REQ);
  request.addSuffix("testAnalyzer1");

  auto status = handler.execute();
  EXPECT_EQ(arangodb::RestStatus::DONE, status);
  EXPECT_EQ(arangodb::rest::ResponseCode::FORBIDDEN, responce.responseCode());
  auto slice = responce._payload.slice();
  EXPECT_TRUE(slice.isObject());
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

  // Check it's not gone
  auto analyzer = analyzers.get(arangodb::StaticStrings::SystemDatabase +
                                 "::testAnalyzer1", arangodb::QueryAnalyzerRevisions::QUERY_LATEST);
  ASSERT_NE(analyzer, nullptr);
}

// still in use (fail)
TEST_F(RestAnalyzerHandlerTest, test_remove_still_in_use) {
  grantOnDb(arangodb::StaticStrings::SystemDatabase, arangodb::auth::Level::RW);

  auto requestPtr = std::make_unique<GeneralRequestMock>(_system_vocbase);
  auto& request = *requestPtr;
  auto responcePtr = std::make_unique<GeneralResponseMock>();
  auto& responce = *responcePtr;
  arangodb::iresearch::RestAnalyzerHandler handler(server.server(), requestPtr.release(),
                                                   responcePtr.release());
  request.setRequestType(arangodb::rest::RequestType::DELETE_REQ);
  request.addSuffix("testAnalyzer2");
  request.values().emplace("force", "false");

  // Hold a reference to mark analyzer in use
  auto inUseAnalyzer = analyzers.get(arangodb::StaticStrings::SystemDatabase +
                                      "::testAnalyzer2", arangodb::QueryAnalyzerRevisions::QUERY_LATEST);
  ASSERT_NE(nullptr, inUseAnalyzer);

  auto status = handler.execute();
  EXPECT_EQ(arangodb::RestStatus::DONE, status);
  EXPECT_EQ(arangodb::rest::ResponseCode::CONFLICT, responce.responseCode());
  auto slice = responce._payload.slice();
  EXPECT_TRUE(slice.isObject());
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
  auto analyzer = analyzers.get(arangodb::StaticStrings::SystemDatabase +
                                 "::testAnalyzer2", arangodb::QueryAnalyzerRevisions::QUERY_LATEST);
  // Check it's not gone
  ASSERT_NE(analyzer, nullptr);
}

// still in use + force (success)
TEST_F(RestAnalyzerHandlerTest, test_remove_still_in_use_force) {
  grantOnDb(arangodb::StaticStrings::SystemDatabase, arangodb::auth::Level::RW);

  auto requestPtr = std::make_unique<GeneralRequestMock>(_system_vocbase);
  auto& request = *requestPtr;
  auto responcePtr = std::make_unique<GeneralResponseMock>();
  auto& responce = *responcePtr;
  arangodb::iresearch::RestAnalyzerHandler handler(server.server(), requestPtr.release(),
                                                   responcePtr.release());
  request.setRequestType(arangodb::rest::RequestType::DELETE_REQ);
  request.addSuffix("testAnalyzer2");
  request.values().emplace("force", "true");

  // hold ref to mark in-use
  auto inUseAnalyzer = analyzers.get(arangodb::StaticStrings::SystemDatabase +
                                      "::testAnalyzer2", arangodb::QueryAnalyzerRevisions::QUERY_LATEST);
  ASSERT_NE(nullptr, inUseAnalyzer);

  auto status = handler.execute();
  EXPECT_EQ(arangodb::RestStatus::DONE, status);
  EXPECT_EQ(arangodb::rest::ResponseCode::OK, responce.responseCode());
  auto slice = responce._payload.slice();
  EXPECT_TRUE(slice.isObject());
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
  auto analyzer = analyzers.get(arangodb::StaticStrings::SystemDatabase +
                                 "::testAnalyzer2", arangodb::QueryAnalyzerRevisions::QUERY_LATEST);
  ASSERT_EQ(nullptr, analyzer);
}

//  removal with  db name in analyzer name
TEST_F(RestAnalyzerHandlerTest, test_remove_with_db_name) {
  grantOnDb(arangodb::StaticStrings::SystemDatabase, arangodb::auth::Level::RW);

  auto requestPtr = std::make_unique<GeneralRequestMock>(_system_vocbase);
  auto& request = *requestPtr;
  auto responcePtr = std::make_unique<GeneralResponseMock>();
  auto& responce = *responcePtr;
  arangodb::iresearch::RestAnalyzerHandler handler(server.server(), requestPtr.release(),
                                                   responcePtr.release());
  request.setRequestType(arangodb::rest::RequestType::DELETE_REQ);
  request.addSuffix(arangodb::StaticStrings::SystemDatabase +
                    "::testAnalyzer1");

  auto status = handler.execute();
  EXPECT_EQ(arangodb::RestStatus::DONE, status);
  EXPECT_EQ(arangodb::rest::ResponseCode::OK, responce.responseCode());
  auto analyzer = analyzers.get(arangodb::StaticStrings::SystemDatabase +
                                 "::testAnalyzer1", arangodb::QueryAnalyzerRevisions::QUERY_LATEST);
  EXPECT_EQ(nullptr, analyzer);
}

TEST_F(RestAnalyzerHandlerTest, test_remove_success) {
  grantOnDb(arangodb::StaticStrings::SystemDatabase, arangodb::auth::Level::RW);

  auto requestPtr = std::make_unique<GeneralRequestMock>(_system_vocbase);
  auto& request = *requestPtr;
  auto responcePtr = std::make_unique<GeneralResponseMock>();
  auto& responce = *responcePtr;
  arangodb::iresearch::RestAnalyzerHandler handler(server.server(), requestPtr.release(),
                                                   responcePtr.release());
  request.setRequestType(arangodb::rest::RequestType::DELETE_REQ);
  request.addSuffix("testAnalyzer1");

  auto status = handler.execute();
  EXPECT_EQ(arangodb::RestStatus::DONE, status);
  EXPECT_EQ(arangodb::rest::ResponseCode::OK, responce.responseCode());
  auto slice = responce._payload.slice();
  EXPECT_TRUE(slice.isObject());
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
  auto analyzer = analyzers.get(arangodb::StaticStrings::SystemDatabase +
                                 "::testAnalyzer1", arangodb::QueryAnalyzerRevisions::QUERY_LATEST);
  ASSERT_EQ(nullptr, analyzer);
}
