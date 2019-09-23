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

 protected:
  arangodb::TestHelper helper;

  V8UsersTest() : helper() {
    // suppress INFO {authentication} Authentication is turned on (system only), authentication for unix sockets is turned on
    // suppress WARNING {authentication} --server.jwt-secret is insecure. Use --server.jwt-secret-keyfile instead
    arangodb::LogTopic::setLogLevel(arangodb::Logger::AUTHENTICATION.name(),
                                    arangodb::LogLevel::ERR);

    helper.mockDatabase();
  }

  ~V8UsersTest() {
    helper.unmockDatabase();

    arangodb::LogTopic::setLogLevel(arangodb::Logger::AUTHENTICATION.name(),
                                    arangodb::LogLevel::DEFAULT);
  }
};

std::string const V8UsersTest::COLLECTION_NAME = "testDataSource";
std::string const V8UsersTest::DB_NAME = "testDatabase";
std::string const V8UsersTest::USERNAME = "testUser";
std::string const V8UsersTest::WILDCARD = "*";

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

TEST_F(V8UsersTest, test_collection_auth) {
  TRI_vocbase_t* vocbase = helper.createDatabase(V8UsersTest::DB_NAME);

  helper.setupV8();
  auto v8isolate = helper.v8Isolate();
  auto v8g = helper.v8Globals();
  v8::Context::Scope contextScope(helper.v8Context());

  auto exec =
      helper.createExecContext(arangodb::auth::AuthUser{V8UsersTest::USERNAME},
                               arangodb::auth::DatabaseResource{V8UsersTest::DB_NAME});
  arangodb::ExecContext::Scope execContextScope(exec.get());

  arangodb::auth::CollectionResource const collectionResource{V8UsersTest::DB_NAME,
                                                              V8UsersTest::COLLECTION_NAME};
  auto const RW = arangodb::auth::Level::RW;
  auto const RO = arangodb::auth::Level::RO;

  // ---------------------------------------------------------------------------
  // test auth missing (grant)
  // ---------------------------------------------------------------------------

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

  {
    helper.createUser(V8UsersTest::USERNAME, [](arangodb::auth::User* user) {});

    EXPECT_FALSE(exec->hasAccess(collectionResource, RO));
    EXPECT_FALSE(exec->hasAccess(collectionResource, RW));

    helper.callFunctionThrow(fn_grantCollection, grantArgs,
                             TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);

    EXPECT_FALSE(exec->hasAccess(collectionResource, RO));
    EXPECT_FALSE(exec->hasAccess(collectionResource, RW));
  }

  // ---------------------------------------------------------------------------
  // test auth missing (revoke)
  // ---------------------------------------------------------------------------

  auto fn_revokeCollection =
      arangoUsers->Get(TRI_V8_ASCII_STRING(v8isolate, "revokeCollection"));
  EXPECT_TRUE((fn_revokeCollection->IsFunction()));

  std::vector<v8::Local<v8::Value>> revokeArgs = {
      TRI_V8_STD_STRING(v8isolate, V8UsersTest::USERNAME),
      TRI_V8_STD_STRING(v8isolate, V8UsersTest::DB_NAME),
      TRI_V8_STD_STRING(v8isolate, V8UsersTest::COLLECTION_NAME),
  };

  {
    helper.createUser(V8UsersTest::USERNAME, [collectionResource](arangodb::auth::User* user) {
      user->grantCollection(collectionResource, RO);
    });

    EXPECT_TRUE(exec->hasAccess(collectionResource, RO));
    EXPECT_FALSE(exec->hasAccess(collectionResource, RW));

    helper.callFunctionThrow(fn_revokeCollection, revokeArgs,
                             TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);

    EXPECT_TRUE(exec->hasAccess(collectionResource, RO));
    EXPECT_FALSE(exec->hasAccess(collectionResource, RW));
  }

  // ---------------------------------------------------------------------------
  // test auth collection (grant)
  // ---------------------------------------------------------------------------

  {
    auto collection = helper.createCollection(vocbase, collectionResource);
    helper.createUser(V8UsersTest::USERNAME, [](arangodb::auth::User* user) {});

    EXPECT_FALSE(exec->hasAccess(collectionResource, RO));
    EXPECT_FALSE(exec->hasAccess(collectionResource, RW));

    helper.callFunction(fn_grantCollection, grantArgs);

    EXPECT_TRUE(exec->hasAccess(collectionResource, RO));
    EXPECT_TRUE(exec->hasAccess(collectionResource, RW));
  }

  // ---------------------------------------------------------------------------
  // test auth collection (revoke)
  // ---------------------------------------------------------------------------

  {
    auto collection = helper.createCollection(vocbase, collectionResource);
    helper.createUser(V8UsersTest::USERNAME, [collectionResource](arangodb::auth::User* user) {
      user->grantCollection(collectionResource, RO);
    });

    EXPECT_TRUE(exec->hasAccess(collectionResource, RO));
    EXPECT_FALSE(exec->hasAccess(collectionResource, RW));

    helper.callFunction(fn_revokeCollection, revokeArgs);

    EXPECT_FALSE(exec->hasAccess(collectionResource, RO));
    EXPECT_FALSE(exec->hasAccess(collectionResource, RW));
  }

  // ---------------------------------------------------------------------------
  // test auth view (grant)
  // ---------------------------------------------------------------------------

  {
    auto view = helper.createView(vocbase, collectionResource);
    helper.createUser(V8UsersTest::USERNAME, [collectionResource](arangodb::auth::User* user) {
      user->grantCollection(collectionResource, RO);
    });

    EXPECT_FALSE(exec->hasAccess(collectionResource, RO));
    EXPECT_FALSE(exec->hasAccess(collectionResource, RW));

    helper.callFunctionThrow(fn_grantCollection, grantArgs,
                             TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);

    EXPECT_FALSE(exec->hasAccess(collectionResource, RO));
    EXPECT_FALSE(exec->hasAccess(collectionResource, RW));
  }

  // ---------------------------------------------------------------------------
  // test auth view (revoke)
  // ---------------------------------------------------------------------------

  {
    auto view = helper.createView(vocbase, collectionResource);
    helper.createUser(V8UsersTest::USERNAME, [collectionResource](arangodb::auth::User* user) {
      user->grantCollection(collectionResource, RO);
    });

    EXPECT_TRUE(exec->hasAccess(collectionResource, RO));
    EXPECT_FALSE(exec->hasAccess(collectionResource, RW));

    helper.callFunctionThrow(fn_revokeCollection, revokeArgs,
                             TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);

    EXPECT_TRUE(exec->hasAccess(collectionResource, RO));
    EXPECT_FALSE(exec->hasAccess(collectionResource, RW));
  }

  // ---------------------------------------------------------------------------
  // test auth wildcard (grant)
  // ---------------------------------------------------------------------------

  std::vector<v8::Local<v8::Value>> grantWildcardArgs = {
      TRI_V8_STD_STRING(v8isolate, V8UsersTest::USERNAME),
      TRI_V8_STD_STRING(v8isolate, V8UsersTest::DB_NAME),
      TRI_V8_STD_STRING(v8isolate, V8UsersTest::WILDCARD),
      TRI_V8_STD_STRING(v8isolate,
                        arangodb::auth::convertFromAuthLevel(arangodb::auth::Level::RW)),
  };

  {
    auto collection = helper.createCollection(vocbase, collectionResource);
    helper.createUser(V8UsersTest::USERNAME, [](arangodb::auth::User*) {});

    EXPECT_FALSE(exec->hasAccess(collectionResource, RO));
    EXPECT_FALSE(exec->hasAccess(collectionResource, RW));

    helper.callFunction(fn_grantCollection, grantWildcardArgs);

    EXPECT_TRUE(exec->hasAccess(collectionResource, RO));
    EXPECT_TRUE(exec->hasAccess(collectionResource, RW));
  }

  // ---------------------------------------------------------------------------
  // test auth wildcard (revoke)
  // ---------------------------------------------------------------------------

  std::vector<v8::Local<v8::Value>> revokeWildcardArgs = {
      TRI_V8_STD_STRING(v8isolate, V8UsersTest::USERNAME),
      TRI_V8_STD_STRING(v8isolate, V8UsersTest::DB_NAME),
      TRI_V8_STD_STRING(v8isolate, V8UsersTest::WILDCARD),
  };

  {
    auto collection = helper.createCollection(vocbase, collectionResource);
    helper.createUser(V8UsersTest::USERNAME, [collectionResource](arangodb::auth::User* user) {
      user->grantCollection(collectionResource, RO);
    });

    EXPECT_TRUE(exec->hasAccess(collectionResource, RO));
    EXPECT_FALSE(exec->hasAccess(collectionResource, RW));

    helper.callFunction(fn_revokeCollection, revokeWildcardArgs);

    EXPECT_TRUE(exec->hasAccess(collectionResource, RO));
    EXPECT_FALSE(exec->hasAccess(collectionResource, RW));
  }
}
