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

#include "../IResearch/common.h"
#include "Helper/TestHelper.h"
#include "Mocks/Servers.h"

#include <velocypack/Builder.h>

#include "Auth/CollectionResource.h"
#include "Utils/ExecContext.h"
#include "V8/v8-globals.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

namespace {
std::string const COLLECTION_NAME = "testDataSource";
std::string const DB_NAME = "testDatabase";
std::string const USERNAME = "testUser";
std::string const WILDCARD = "*";

arangodb::auth::CollectionResource const COLLECTION_RESOURCE{DB_NAME, COLLECTION_NAME};

std::vector<v8::Local<v8::Value>> grantArgs;
std::vector<v8::Local<v8::Value>> grantWildcardArgs;
std::vector<v8::Local<v8::Value>> revokeArgs;
std::vector<v8::Local<v8::Value>> revokeWildcardArgs;

v8::Handle<v8::Value> fn_grantCollection;
v8::Handle<v8::Value> fn_revokeCollection;

TRI_vocbase_t* vocbase = nullptr;

constexpr arangodb::auth::Level RW = arangodb::auth::Level::RW;
constexpr arangodb::auth::Level RO = arangodb::auth::Level::RO;
}  // namespace

class V8UsersTest : public ::testing::Test {
 public:
  static arangodb::tests::TestHelper helper;

 protected:
  arangodb::ExecContext* exec;
  std::unique_ptr<arangodb::ExecContext::Scope> execContextScope;

  V8UsersTest() : exec(nullptr) {
    helper.usersSetup();

    exec = helper.createExecContext(arangodb::auth::AuthUser{USERNAME},
				    arangodb::auth::DatabaseResource{DB_NAME});
    execContextScope.reset(new arangodb::ExecContext::Scope(exec));
  }

  ~V8UsersTest() {
    helper.disposeExecContext();

    arangodb::LogTopic::setLogLevel(arangodb::Logger::AUTHENTICATION.name(),
                                    arangodb::LogLevel::DEFAULT);
  }

  // name changed between gtest versions
  static void SetUpTestCase() {
    SetUpTestSuite();
  }

  static void SetUpTestSuite() {
    // suppress INFO {authentication} Authentication is turned on (system only), authentication for unix sockets is turned on
    // suppress WARNING {authentication} --server.jwt-secret is insecure. Use --server.jwt-secret-keyfile instead
    arangodb::LogTopic::setLogLevel(arangodb::Logger::AUTHENTICATION.name(),
                                    arangodb::LogLevel::ERR);

    helper.viewFactoryInit(helper.mockAqlServerInit());

    vocbase = helper.createDatabase(DB_NAME);

    helper.v8Setup(vocbase);

    auto v8isolate = helper.v8Isolate();
    v8::HandleScope handleScope(v8isolate);
    auto v8Context = helper.v8Context();
    v8::Context::Scope contextScope(v8Context);

    auto v8g = helper.v8Globals();
    auto arangoUsers =
        v8::Local<v8::ObjectTemplate>::New(v8isolate, v8g->UsersTempl)->NewInstance();

    auto fn_grantCollection =
        arangoUsers->Get(TRI_V8_ASCII_STRING(v8isolate, "grantCollection"));
    EXPECT_TRUE((fn_grantCollection->IsFunction()));

    grantArgs = {
        TRI_V8_STD_STRING(v8isolate, USERNAME),
        TRI_V8_STD_STRING(v8isolate, DB_NAME),
        TRI_V8_STD_STRING(v8isolate, COLLECTION_NAME),
        TRI_V8_STD_STRING(v8isolate, arangodb::auth::convertFromAuthLevel(RW)),
    };

    fn_revokeCollection =
        arangoUsers->Get(TRI_V8_ASCII_STRING(v8isolate, "revokeCollection"));
    EXPECT_TRUE((fn_revokeCollection->IsFunction()));

    revokeArgs = {
        TRI_V8_STD_STRING(v8isolate, USERNAME),
        TRI_V8_STD_STRING(v8isolate, DB_NAME),
        TRI_V8_STD_STRING(v8isolate, COLLECTION_NAME),
    };

    grantWildcardArgs = {
        TRI_V8_STD_STRING(v8isolate, USERNAME),
        TRI_V8_STD_STRING(v8isolate, DB_NAME),
        TRI_V8_STD_STRING(v8isolate, WILDCARD),
        TRI_V8_STD_STRING(v8isolate, arangodb::auth::convertFromAuthLevel(RW)),
    };

    revokeWildcardArgs = {
        TRI_V8_STD_STRING(v8isolate, USERNAME),
        TRI_V8_STD_STRING(v8isolate, DB_NAME),
        TRI_V8_STD_STRING(v8isolate, WILDCARD),
    };
  }

  // name changed between gtest versions
  static void TearDownTestCase() {
    TearDownTestSuite();
  }

  static void TearDownTestSuite() {
    helper.v8Teardown();
  }
};

arangodb::tests::TestHelper V8UsersTest::helper;

// -----------------------------------------------------------------------------
// test auth missing (grant)
// -----------------------------------------------------------------------------

TEST_F(V8UsersTest, v8_users_test_grant_collection_non_existing) {
  helper.createUser(USERNAME, [](arangodb::auth::User* user) {});

  EXPECT_FALSE(exec->hasAccess(COLLECTION_RESOURCE, RO));
  EXPECT_FALSE(exec->hasAccess(COLLECTION_RESOURCE, RW));

  helper.callFunctionThrow(fn_grantCollection, grantArgs, TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);

  EXPECT_FALSE(exec->hasAccess(COLLECTION_RESOURCE, RO));
  EXPECT_FALSE(exec->hasAccess(COLLECTION_RESOURCE, RW));
}

// -----------------------------------------------------------------------------
// test auth missing (revoke)
// -----------------------------------------------------------------------------

TEST_F(V8UsersTest, v8_users_test_revoke_collection_non_existing) {
  helper.createUser(USERNAME, [](arangodb::auth::User* user) {
    user->grantCollection(COLLECTION_RESOURCE, RO);
  });

  EXPECT_TRUE(exec->hasAccess(COLLECTION_RESOURCE, RO));
  EXPECT_FALSE(exec->hasAccess(COLLECTION_RESOURCE, RW));

  helper.callFunctionThrow(fn_revokeCollection, revokeArgs,
                           TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);

  EXPECT_TRUE(exec->hasAccess(COLLECTION_RESOURCE, RO));
  EXPECT_FALSE(exec->hasAccess(COLLECTION_RESOURCE, RW));
}

// -----------------------------------------------------------------------------
// test auth collection (grant)
// -----------------------------------------------------------------------------

TEST_F(V8UsersTest, v8_users_test_grant_collection) {
  auto collection = helper.createCollection(vocbase, COLLECTION_RESOURCE);
  helper.createUser(USERNAME, [](arangodb::auth::User* user) {});

  EXPECT_FALSE(exec->hasAccess(COLLECTION_RESOURCE, RO));
  EXPECT_FALSE(exec->hasAccess(COLLECTION_RESOURCE, RW));

  helper.callFunction(fn_grantCollection, grantArgs);

  EXPECT_TRUE(exec->hasAccess(COLLECTION_RESOURCE, RO));
  EXPECT_TRUE(exec->hasAccess(COLLECTION_RESOURCE, RW));
}

// -----------------------------------------------------------------------------
// test auth collection (revoke)
// -----------------------------------------------------------------------------

TEST_F(V8UsersTest, v8_users_test_revoke_collection) {
  auto collection = helper.createCollection(vocbase, COLLECTION_RESOURCE);
  helper.createUser(USERNAME, [](arangodb::auth::User* user) {
    user->grantCollection(COLLECTION_RESOURCE, RO);
  });

  EXPECT_TRUE(exec->hasAccess(COLLECTION_RESOURCE, RO));
  EXPECT_FALSE(exec->hasAccess(COLLECTION_RESOURCE, RW));

  helper.callFunction(fn_revokeCollection, revokeArgs);

  EXPECT_FALSE(exec->hasAccess(COLLECTION_RESOURCE, RO));
  EXPECT_FALSE(exec->hasAccess(COLLECTION_RESOURCE, RW));
}

// ---------------------------------------------------------------------------
// test auth view (grant)
// ---------------------------------------------------------------------------

TEST_F(V8UsersTest, v8_users_test_grant_view) {
  auto view = helper.createView(vocbase, COLLECTION_RESOURCE);
  helper.createUser(USERNAME, [](arangodb::auth::User* user) {
    user->grantCollection(COLLECTION_RESOURCE, RO);
  });

  EXPECT_FALSE(exec->hasAccess(COLLECTION_RESOURCE, RO));
  EXPECT_FALSE(exec->hasAccess(COLLECTION_RESOURCE, RW));

  helper.callFunctionThrow(fn_grantCollection, grantArgs, TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);

  EXPECT_FALSE(exec->hasAccess(COLLECTION_RESOURCE, RO));
  EXPECT_FALSE(exec->hasAccess(COLLECTION_RESOURCE, RW));
}

// -----------------------------------------------------------------------------
// test auth view (revoke)
// -----------------------------------------------------------------------------

TEST_F(V8UsersTest, v8_users_test_revoke_view) {
  auto view = helper.createView(vocbase, COLLECTION_RESOURCE);
  helper.createUser(USERNAME, [](arangodb::auth::User* user) {
    user->grantCollection(COLLECTION_RESOURCE, RO);
  });

  EXPECT_TRUE(exec->hasAccess(COLLECTION_RESOURCE, RO));
  EXPECT_FALSE(exec->hasAccess(COLLECTION_RESOURCE, RW));

  helper.callFunctionThrow(fn_revokeCollection, revokeArgs,
                           TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);

  EXPECT_TRUE(exec->hasAccess(COLLECTION_RESOURCE, RO));
  EXPECT_FALSE(exec->hasAccess(COLLECTION_RESOURCE, RW));
}

// ---------------------------------------------------------------------------
// test auth wildcard (grant)
// ---------------------------------------------------------------------------

TEST_F(V8UsersTest, v8_users_test_grant_wildcard_collection) {
  auto collection = helper.createCollection(vocbase, COLLECTION_RESOURCE);
  helper.createUser(USERNAME, [](arangodb::auth::User*) {});

  EXPECT_FALSE(exec->hasAccess(COLLECTION_RESOURCE, RO));
  EXPECT_FALSE(exec->hasAccess(COLLECTION_RESOURCE, RW));

  helper.callFunction(fn_grantCollection, grantWildcardArgs);

  EXPECT_TRUE(exec->hasAccess(COLLECTION_RESOURCE, RO));
  EXPECT_TRUE(exec->hasAccess(COLLECTION_RESOURCE, RW));
}

// -----------------------------------------------------------------------------
// test auth wildcard (revoke)
// ---------------------------------------------------------------------------

TEST_F(V8UsersTest, v8_users_test_revoke_wildcard_collection) {
  auto collection = helper.createCollection(vocbase, COLLECTION_RESOURCE);
  helper.createUser(USERNAME, [](arangodb::auth::User* user) {
    user->grantCollection(COLLECTION_RESOURCE, RO);
  });

  EXPECT_TRUE(exec->hasAccess(COLLECTION_RESOURCE, RO));
  EXPECT_FALSE(exec->hasAccess(COLLECTION_RESOURCE, RW));

  helper.callFunction(fn_revokeCollection, revokeWildcardArgs);

  EXPECT_TRUE(exec->hasAccess(COLLECTION_RESOURCE, RO));
  EXPECT_FALSE(exec->hasAccess(COLLECTION_RESOURCE, RW));
}
