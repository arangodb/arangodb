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

#include "gtest/gtest.h"

#include "../Helper/TestHelper.h"
#include "../IResearch/common.h"
#include "../Mocks/StorageEngineMock.h"

#include <velocypack/Builder.h>

#include "Auth/CollectionResource.h"
#include "Utils/ExecContext.h"
#include "V8/v8-globals.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

class V8UsersTest : public ::testing::Test {
 public:
  static std::string const COLLECTION_NAME;
  static std::string const DB_NAME;
  static std::string const USERNAME;
  static std::string const WILDCARD;

  static arangodb::auth:CollectionResource const COLLECTION_RESOURCE;

  static std::vector<v8::Local<v8::Value>> grantArgs;
  static std::vector<v8::Local<v8::Value>> grantWildcardArgs;
  static std::vector<v8::Local<v8::Value>> revokeArgs;
  static std::vector<v8::Local<v8::Value>> revokeWildcardArgs;

  static v8::Handle<v8::Value> fn_grantCollection;
  static v8::Handle<v8::Value> fn_revokeCollection;

  static TRI_vocbase_t* vocbase;

  static constexpr arangodb::auth::Level RW = arangodb::auth::Level::RW;
  static constexpr arangodb::auth::Level RO = arangodb::auth::Level::RO;

 protected:
  arangodb::TestHelper helper;

  V8UsersTest() : helper() {
    // suppress INFO {authentication} Authentication is turned on (system only), authentication for unix sockets is turned on
    // suppress WARNING {authentication} --server.jwt-secret is insecure. Use --server.jwt-secret-keyfile instead
    arangodb::LogTopic::setLogLevel(arangodb::Logger::AUTHENTICATION.name(),
                                    arangodb::LogLevel::ERR);
  }

  ~V8UsersTest() {
    arangodb::LogTopic::setLogLevel(arangodb::Logger::AUTHENTICATION.name(),
                                    arangodb::LogLevel::DEFAULT);
  }

  static void SetUpTestSuite() {
    helper.mockDatabase();

    vocbase = helper.createDatabase(V8UsersTest::DB_NAME);

    helper.setupV8();

    v8isolate = helper.v8Isolate();
    v8g = helper.v8Globals();

  v8::Context::Scope contextScope(helper.v8Context());
  auto exec =
      helper.createExecContext(arangodb::auth::AuthUser{V8UsersTest::USERNAME},
                               arangodb::auth::DatabaseResource{V8UsersTest::DB_NAME});
  arangodb::ExecContext::Scope execContextScope(exec.get());

  auto arangoUsers =
      v8::Local<v8::ObjectTemplate>::New(v8isolate, v8g->UsersTempl)->NewInstance();
  auto fn_grantCollection =
      arangoUsers->Get(TRI_V8_ASCII_STRING(v8isolate, "grantCollection"));
  EXPECT_TRUE((fn_grantCollection->IsFunction()));

  std::vector<v8::Local<v8::Value>> grantArgs = {
      TRI_V8_STD_STRING(v8isolate, V8UsersTest::USERNAME),
      TRI_V8_STD_STRING(v8isolate, V8UsersTest::DB_NAME),
      TRI_V8_STD_STRING(v8isolate, V8UsersTest::COLLECTION_NAME),
      TRI_V8_STD_STRING(v8isolate,
                        arangodb::auth::convertFromAuthLevel(arangodb::auth::Level::RW)),
  };

  auto fn_revokeCollection =
      arangoUsers->Get(TRI_V8_ASCII_STRING(v8isolate, "revokeCollection"));
  EXPECT_TRUE((fn_revokeCollection->IsFunction()));

  std::vector<v8::Local<v8::Value>> revokeArgs = {
      TRI_V8_STD_STRING(v8isolate, V8UsersTest::USERNAME),
      TRI_V8_STD_STRING(v8isolate, V8UsersTest::DB_NAME),
      TRI_V8_STD_STRING(v8isolate, V8UsersTest::COLLECTION_NAME),
  };

  std::vector<v8::Local<v8::Value>> grantWildcardArgs = {
      TRI_V8_STD_STRING(v8isolate, V8UsersTest::USERNAME),
      TRI_V8_STD_STRING(v8isolate, V8UsersTest::DB_NAME),
      TRI_V8_STD_STRING(v8isolate, V8UsersTest::WILDCARD),
      TRI_V8_STD_STRING(v8isolate,
                        arangodb::auth::convertFromAuthLevel(arangodb::auth::Level::RW)),
  };

  std::vector<v8::Local<v8::Value>> revokeWildcardArgs = {
      TRI_V8_STD_STRING(v8isolate, V8UsersTest::USERNAME),
      TRI_V8_STD_STRING(v8isolate, V8UsersTest::DB_NAME),
      TRI_V8_STD_STRING(v8isolate, V8UsersTest::WILDCARD),
  };

  void TearDownTestSuite() {
    helper.unmockDatabase();
  }
};

std::string const V8UsersTest::COLLECTION_NAME = "testDataSource";
std::string const V8UsersTest::DB_NAME = "testDatabase";
std::string const V8UsersTest::USERNAME = "testUser";
std::string const V8UsersTest::WILDCARD = "*";

arangodb::auth:CollectionResource const COLLECTION_RESOURCE{
  V8UsersTest::DB_NAME,
  V8UsersTest::COLLECTION_NAME};

TRI_vocbase_t* V8UsersTest::vocbase = nullptr;

// -----------------------------------------------------------------------------
// test auth missing (grant)
// -----------------------------------------------------------------------------

TEST_F(V8UsersTest, v8_users_test_grant_collection_non_existing) {
  helper.createUser(V8UsersTest::USERNAME, [](arangodb::auth::User* user) {});

  EXPECT_FALSE(exec->hasAccess(V8UsersTest::COLLECTION_RESOURCE, RO));
  EXPECT_FALSE(exec->hasAccess(V8UsersTest::COLLECTION_RESOURCE, RW));

  helper.callFunctionThrow(fn_grantCollection, grantArgs,
			   TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);

  EXPECT_FALSE(exec->hasAccess(V8UsersTest::COLLECTION_RESOURCE, RO));
  EXPECT_FALSE(exec->hasAccess(V8UsersTest::COLLECTION_RESOURCE, RW));
}

// -----------------------------------------------------------------------------
// test auth missing (revoke)
// -----------------------------------------------------------------------------

TEST_F(V8UsersTest, v8_users_test_revoke_collection_non_existing) {
  helper.createUser(V8UsersTest::USERNAME, [V8UsersTest::COLLECTION_RESOURCE](arangodb::auth::User* user) {
		      user->grantCollection(V8UsersTest::COLLECTION_RESOURCE, RO);
		    });

  EXPECT_TRUE(exec->hasAccess(V8UsersTest::COLLECTION_RESOURCE, RO));
  EXPECT_FALSE(exec->hasAccess(V8UsersTest::COLLECTION_RESOURCE, RW));

  helper.callFunctionThrow(fn_revokeCollection, revokeArgs,
			   TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);

  EXPECT_TRUE(exec->hasAccess(V8UsersTest::COLLECTION_RESOURCE, RO));
  EXPECT_FALSE(exec->hasAccess(V8UsersTest::COLLECTION_RESOURCE, RW));
}

// -----------------------------------------------------------------------------
// test auth collection (grant)
// -----------------------------------------------------------------------------

TEST_F(V8UsersTest, v8_users_test_grant_collection) {
  auto collection = helper.createCollection(vocbase, V8UsersTest::COLLECTION_RESOURCE);
  helper.createUser(V8UsersTest::USERNAME, [](arangodb::auth::User* user) {});

  EXPECT_FALSE(exec->hasAccess(V8UsersTest::COLLECTION_RESOURCE, RO));
  EXPECT_FALSE(exec->hasAccess(V8UsersTest::COLLECTION_RESOURCE, RW));

  helper.callFunction(fn_grantCollection, grantArgs);

  EXPECT_TRUE(exec->hasAccess(V8UsersTest::COLLECTION_RESOURCE, RO));
  EXPECT_TRUE(exec->hasAccess(V8UsersTest::COLLECTION_RESOURCE, RW));
}

// -----------------------------------------------------------------------------
// test auth collection (revoke)
// -----------------------------------------------------------------------------

TEST_F(V8UsersTest, v8_users_test_revoke_collection) {
  auto collection = helper.createCollection(vocbase, V8UsersTest::COLLECTION_RESOURCE);
  helper.createUser(V8UsersTest::USERNAME, [V8UsersTest::COLLECTION_RESOURCE](arangodb::auth::User* user) {
		      user->grantCollection(V8UsersTest::COLLECTION_RESOURCE, RO);
		    });

  EXPECT_TRUE(exec->hasAccess(V8UsersTest::COLLECTION_RESOURCE, RO));
  EXPECT_FALSE(exec->hasAccess(V8UsersTest::COLLECTION_RESOURCE, RW));

  helper.callFunction(fn_revokeCollection, revokeArgs);

  EXPECT_FALSE(exec->hasAccess(V8UsersTest::COLLECTION_RESOURCE, RO));
  EXPECT_FALSE(exec->hasAccess(V8UsersTest::COLLECTION_RESOURCE, RW));
}

// ---------------------------------------------------------------------------
// test auth view (grant)
// ---------------------------------------------------------------------------

TEST_F(V8UsersTest, v8_users_test_grant_view) {
  auto view = helper.createView(vocbase, V8UsersTest::COLLECTION_RESOURCE);
  helper.createUser(V8UsersTest::USERNAME, [V8UsersTest::COLLECTION_RESOURCE](arangodb::auth::User* user) {
		      user->grantCollection(V8UsersTest::COLLECTION_RESOURCE, RO);
		    });

  EXPECT_FALSE(exec->hasAccess(V8UsersTest::COLLECTION_RESOURCE, RO));
  EXPECT_FALSE(exec->hasAccess(V8UsersTest::COLLECTION_RESOURCE, RW));

  helper.callFunctionThrow(fn_grantCollection, grantArgs,
			   TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);

  EXPECT_FALSE(exec->hasAccess(V8UsersTest::COLLECTION_RESOURCE, RO));
  EXPECT_FALSE(exec->hasAccess(V8UsersTest::COLLECTION_RESOURCE, RW));
}

// -----------------------------------------------------------------------------
// test auth view (revoke)
// -----------------------------------------------------------------------------

TEST_F(V8UsersTest, v8_users_test_revoke_view) {
  auto view = helper.createView(vocbase, V8UsersTest::COLLECTION_RESOURCE);
  helper.createUser(V8UsersTest::USERNAME, [V8UsersTest::COLLECTION_RESOURCE](arangodb::auth::User* user) {
		      user->grantCollection(V8UsersTest::COLLECTION_RESOURCE, RO);
		    });

  EXPECT_TRUE(exec->hasAccess(V8UsersTest::COLLECTION_RESOURCE, RO));
  EXPECT_FALSE(exec->hasAccess(V8UsersTest::COLLECTION_RESOURCE, RW));

  helper.callFunctionThrow(fn_revokeCollection, revokeArgs,
			   TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);

  EXPECT_TRUE(exec->hasAccess(V8UsersTest::COLLECTION_RESOURCE, RO));
  EXPECT_FALSE(exec->hasAccess(V8UsersTest::COLLECTION_RESOURCE, RW));
}

// ---------------------------------------------------------------------------
// test auth wildcard (grant)
// ---------------------------------------------------------------------------

TEST_F(V8UsersTest, v8_users_test_grant_wildcard_collection) {
  auto collection = helper.createCollection(vocbase, V8UsersTest::COLLECTION_RESOURCE);
  helper.createUser(V8UsersTest::USERNAME, [](arangodb::auth::User*) {});

  EXPECT_FALSE(exec->hasAccess(V8UsersTest::COLLECTION_RESOURCE, RO));
  EXPECT_FALSE(exec->hasAccess(V8UsersTest::COLLECTION_RESOURCE, RW));

  helper.callFunction(fn_grantCollection, grantWildcardArgs);

  EXPECT_TRUE(exec->hasAccess(V8UsersTest::COLLECTION_RESOURCE, RO));
  EXPECT_TRUE(exec->hasAccess(V8UsersTest::COLLECTION_RESOURCE, RW));
}

// -----------------------------------------------------------------------------
// test auth wildcard (revoke)
// ---------------------------------------------------------------------------

TEST_F(V8UsersTest, v8_users_test_revoke_wildcard_collection) {
  auto collection = helper.createCollection(vocbase, V8UsersTest::COLLECTION_RESOURCE);
  helper.createUser(V8UsersTest::USERNAME, [V8UsersTest::COLLECTION_RESOURCE](arangodb::auth::User* user) {
		      user->grantCollection(V8UsersTest::COLLECTION_RESOURCE, RO);
		    });

  EXPECT_TRUE(exec->hasAccess(V8UsersTest::COLLECTION_RESOURCE, RO));
  EXPECT_FALSE(exec->hasAccess(V8UsersTest::COLLECTION_RESOURCE, RW));

  helper.callFunction(fn_revokeCollection, revokeWildcardArgs);

  EXPECT_TRUE(exec->hasAccess(V8UsersTest::COLLECTION_RESOURCE, RO));
  EXPECT_FALSE(exec->hasAccess(V8UsersTest::COLLECTION_RESOURCE, RW));
}
