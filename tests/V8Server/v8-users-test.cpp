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

std::string const SYSTEM_RESOURCE = "_system";
arangodb::auth::CollectionResource const COLLECTION_RESOURCE{DB_NAME, COLLECTION_NAME};

TRI_vocbase_t* vocbase = nullptr;

v8::Persistent<v8::Object> objUsers;

std::vector<std::string> argsGrant;
std::vector<std::string> argsGrantWildcard;
std::vector<std::string> argsRevoke;
std::vector<std::string> argsRevokeWildcard;

v8::Persistent<v8::Value> fnGrantCollection;
v8::Persistent<v8::Value> fnRevokeCollection;

constexpr arangodb::auth::Level RW = arangodb::auth::Level::RW;
constexpr arangodb::auth::Level RO = arangodb::auth::Level::RO;
}  // namespace

class V8UsersTest
    : public ::testing::Test,
      public arangodb::tests::LogSuppressor<arangodb::Logger::AUTHENTICATION, arangodb::LogLevel::ERR> {
 public:
  static arangodb::tests::TestHelper helper;

  // -----------------------------------------------------------------------------
  // --SECTION--                                                 setup / tear-down
  // -----------------------------------------------------------------------------

 protected:
  arangodb::ExecContext* exec;
  std::unique_ptr<arangodb::ExecContext::Scope> execContextScope;

  V8UsersTest() : exec(nullptr) {
    helper.usersSetup();

    exec = helper.createExecContext(arangodb::auth::AuthUser{USERNAME},
                                    arangodb::auth::DatabaseResource{DB_NAME});
    exec->setSystemDbAuthLevel(RW);
    execContextScope.reset(new arangodb::ExecContext::Scope(exec));
  }

  ~V8UsersTest() {
    helper.disposeExecContext();
    helper.usersTeardown();
  }

  // name changed between gtest versions
  static void SetUpTestCase() { SetUpTestSuite(); }

  static void SetUpTestSuite() {
    auto server = helper.mockAqlServerInit();
    helper.viewFactoryInit(server);

    vocbase = helper.createDatabase(DB_NAME);

    helper.v8Setup();

    auto v8isolate = helper.v8Isolate();
    v8::HandleScope handleScope(v8isolate);

    auto v8Context = helper.v8Context();
    v8::Context::Scope contextScope(v8Context);

    auto v8g = helper.v8Globals();
    auto arangoUsers =
        v8::Local<v8::ObjectTemplate>::New(v8isolate, v8g->UsersTempl)->NewInstance();
    objUsers.Reset(v8isolate, arangoUsers);

    auto grantCollection =
        arangoUsers->Get(TRI_V8_ASCII_STRING(v8isolate, "grantCollection"));
    ASSERT_TRUE(grantCollection->IsFunction());
    fnGrantCollection.Reset(v8isolate, grantCollection);

    argsGrant = {USERNAME, DB_NAME, COLLECTION_NAME,
                 arangodb::auth::convertFromAuthLevel(RW)};

    auto revokeCollection =
        arangoUsers->Get(TRI_V8_ASCII_STRING(v8isolate, "revokeCollection"));
    ASSERT_TRUE(revokeCollection->IsFunction());
    fnRevokeCollection.Reset(v8isolate, revokeCollection);

    argsRevoke = {USERNAME, DB_NAME, COLLECTION_NAME};

    argsGrantWildcard = {USERNAME, DB_NAME, WILDCARD,
                         arangodb::auth::convertFromAuthLevel(RW)};

    argsRevokeWildcard = {USERNAME, DB_NAME, WILDCARD};
  }

  // name changed between gtest versions
  static void TearDownTestCase() { TearDownTestSuite(); }

  static void TearDownTestSuite() { helper.v8Teardown(); }
};

arangodb::tests::TestHelper V8UsersTest::helper;

// -----------------------------------------------------------------------------
// test auth missing (grant)
// -----------------------------------------------------------------------------

TEST_F(V8UsersTest, v8_users_test_grant_collection_non_existing) {
  helper.createUser(USERNAME, [](arangodb::auth::User* user) {});

  ASSERT_FALSE(exec->hasAccess(COLLECTION_RESOURCE, RO));
  ASSERT_FALSE(exec->hasAccess(COLLECTION_RESOURCE, RW));

  helper.callFunctionThrow(objUsers, fnGrantCollection, argsGrant,
                           TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);

  ASSERT_FALSE(exec->hasAccess(COLLECTION_RESOURCE, RO));
  ASSERT_FALSE(exec->hasAccess(COLLECTION_RESOURCE, RW));
}

// -----------------------------------------------------------------------------
// test auth missing (revoke)
// -----------------------------------------------------------------------------

TEST_F(V8UsersTest, v8_users_test_revoke_collection_non_existing) {
  helper.createUser(USERNAME, [](arangodb::auth::User* user) {
    user->grantCollection(COLLECTION_RESOURCE, RO);
  });

  ASSERT_TRUE(exec->hasAccess(COLLECTION_RESOURCE, RO));
  ASSERT_FALSE(exec->hasAccess(COLLECTION_RESOURCE, RW));

  helper.callFunctionThrow(objUsers, fnRevokeCollection, argsRevoke,
                           TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);

  ASSERT_TRUE(exec->hasAccess(COLLECTION_RESOURCE, RO));
  ASSERT_FALSE(exec->hasAccess(COLLECTION_RESOURCE, RW));
}

// -----------------------------------------------------------------------------
// test auth collection (grant)
// -----------------------------------------------------------------------------

TEST_F(V8UsersTest, v8_users_test_grant_collection) {
  auto collection = helper.createCollection(vocbase, COLLECTION_RESOURCE);
  helper.createUser(USERNAME, [](arangodb::auth::User* user) {});

  ASSERT_FALSE(exec->hasAccess(COLLECTION_RESOURCE, RO));
  ASSERT_FALSE(exec->hasAccess(COLLECTION_RESOURCE, RW));

  helper.callFunction(objUsers, fnGrantCollection, argsGrant);

  ASSERT_TRUE(exec->hasAccess(COLLECTION_RESOURCE, RO));
  ASSERT_TRUE(exec->hasAccess(COLLECTION_RESOURCE, RW));
}

// -----------------------------------------------------------------------------
// test auth collection (revoke)
// -----------------------------------------------------------------------------

TEST_F(V8UsersTest, v8_users_test_revoke_collection) {
  auto collection = helper.createCollection(vocbase, COLLECTION_RESOURCE);
  helper.createUser(USERNAME, [](arangodb::auth::User* user) {
    user->grantCollection(COLLECTION_RESOURCE, RO);
  });

  ASSERT_TRUE(exec->hasAccess(COLLECTION_RESOURCE, RO));
  ASSERT_FALSE(exec->hasAccess(COLLECTION_RESOURCE, RW));

  helper.callFunction(objUsers, fnRevokeCollection, argsRevoke);

  ASSERT_FALSE(exec->hasAccess(COLLECTION_RESOURCE, RO));
  ASSERT_FALSE(exec->hasAccess(COLLECTION_RESOURCE, RW));
}

// ---------------------------------------------------------------------------
// test auth view (grant)
// ---------------------------------------------------------------------------

TEST_F(V8UsersTest, v8_users_test_grant_view) {
  helper.createUser(USERNAME, [](arangodb::auth::User* user) {});

  ASSERT_FALSE(exec->hasAccess(COLLECTION_RESOURCE, RO));
  ASSERT_FALSE(exec->hasAccess(COLLECTION_RESOURCE, RW));

  helper.callFunctionThrow(objUsers, fnGrantCollection, argsGrant,
                           TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);

  ASSERT_FALSE(exec->hasAccess(COLLECTION_RESOURCE, RO));
  ASSERT_FALSE(exec->hasAccess(COLLECTION_RESOURCE, RW));
}

// -----------------------------------------------------------------------------
// test auth view (revoke)
// -----------------------------------------------------------------------------

TEST_F(V8UsersTest, v8_users_test_revoke_view) {
  helper.createUser(USERNAME, [](arangodb::auth::User* user) {
    user->grantCollection(COLLECTION_RESOURCE, RO);
  });

  ASSERT_TRUE(exec->hasAccess(COLLECTION_RESOURCE, RO));
  ASSERT_FALSE(exec->hasAccess(COLLECTION_RESOURCE, RW));

  helper.callFunctionThrow(objUsers, fnRevokeCollection, argsRevoke,
                           TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);

  ASSERT_TRUE(exec->hasAccess(COLLECTION_RESOURCE, RO));
  ASSERT_FALSE(exec->hasAccess(COLLECTION_RESOURCE, RW));
}

// ---------------------------------------------------------------------------
// test auth wildcard (grant)
// ---------------------------------------------------------------------------

TEST_F(V8UsersTest, v8_users_test_grant_wildcard_collection) {
  auto collection = helper.createCollection(vocbase, COLLECTION_RESOURCE);
  helper.createUser(USERNAME, [](arangodb::auth::User*) {});

  ASSERT_FALSE(exec->hasAccess(COLLECTION_RESOURCE, RO));
  ASSERT_FALSE(exec->hasAccess(COLLECTION_RESOURCE, RW));

  helper.callFunction(objUsers, fnGrantCollection, argsGrantWildcard);

  ASSERT_TRUE(exec->hasAccess(COLLECTION_RESOURCE, RO));
  ASSERT_TRUE(exec->hasAccess(COLLECTION_RESOURCE, RW));
}

// -----------------------------------------------------------------------------
// test auth wildcard (revoke)
// ---------------------------------------------------------------------------

TEST_F(V8UsersTest, v8_users_test_revoke_wildcard_collection) {
  auto collection = helper.createCollection(vocbase, COLLECTION_RESOURCE);
  helper.createUser(USERNAME, [](arangodb::auth::User* user) {
    user->grantCollection(COLLECTION_RESOURCE, RO);
  });

  ASSERT_TRUE(exec->hasAccess(COLLECTION_RESOURCE, RO));
  ASSERT_FALSE(exec->hasAccess(COLLECTION_RESOURCE, RW));

  helper.callFunction(objUsers, fnRevokeCollection, argsRevokeWildcard);

  ASSERT_TRUE(exec->hasAccess(COLLECTION_RESOURCE, RO));
  ASSERT_FALSE(exec->hasAccess(COLLECTION_RESOURCE, RW));
}
