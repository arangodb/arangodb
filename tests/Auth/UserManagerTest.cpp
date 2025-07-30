////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Andreas Streichardt
/// @author Copyright 2017, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "fakeit.hpp"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Aql/QueryRegistry.h"
#include "Auth/Handler.h"
#include "Auth/User.h"
#include "Auth/UserManager.h"
#include "Cluster/ServerState.h"
#include "RestServer/DatabaseFeature.h"

using namespace fakeit;
using namespace arangodb;
using namespace arangodb::aql;

namespace arangodb {
namespace tests {
namespace auth_info_test {

class TestQueryRegistry : public QueryRegistry {
 public:
  TestQueryRegistry() : QueryRegistry(1.0){};
  virtual ~TestQueryRegistry() = default;
};

class UserManagerTest : public ::testing::Test {
 protected:
  arangodb::ArangodServer server;
  TestQueryRegistry queryRegistry;
  ServerState* state;
  auth::UserManager um;

  UserManagerTest()
      : server(nullptr, nullptr), state(ServerState::instance()), um(server) {
    state->setRole(ServerState::ROLE_SINGLE);

    server.addFeature<DatabaseFeature>();
  }

  ~UserManagerTest() {
    state->setServerMode(ServerState::Mode::DEFAULT);
    state->setReadOnly(ServerState::API_FALSE);
  }
};

TEST_F(UserManagerTest, unknown_user_will_have_no_access) {
  auth::UserMap userEntryMap;
  um.setAuthInfo(userEntryMap);
  auth::Level authLevel = um.databaseAuthLevel("test", "test");
  ASSERT_EQ(authLevel, auth::Level::NONE);
}

TEST_F(UserManagerTest,
       granting_rw_access_on_database_star_will_grant_to_all_databases) {
  auth::UserMap userEntryMap;
  auto testUser = auth::User::newUser("test", "test");
  testUser.grantDatabase("*", auth::Level::RW);
  userEntryMap.emplace("test", testUser);

  um.setAuthInfo(userEntryMap);
  auth::Level authLevel = um.databaseAuthLevel("test", "test");
  ASSERT_EQ(authLevel, auth::Level::RW);
}

TEST_F(
    UserManagerTest,
    setting_serverstate_to_readonly_will_make_all_users_effectively_ro_users) {
  auth::UserMap userEntryMap;
  auto testUser = auth::User::newUser("test", "test");
  testUser.grantDatabase("*", auth::Level::RW);
  userEntryMap.emplace("test", testUser);

  state->setReadOnly(ServerState::API_TRUE);

  um.setAuthInfo(userEntryMap);
  auth::Level authLevel = um.databaseAuthLevel("test", "test");
  ASSERT_EQ(authLevel, auth::Level::RO);
}

TEST_F(UserManagerTest,
       in_readonly_mode_the_configured_access_level_will_still_be_accessible) {
  auth::UserMap userEntryMap;
  auto testUser = auth::User::newUser("test", "test");
  testUser.grantDatabase("*", auth::Level::RW);
  userEntryMap.emplace("test", testUser);

  state->setReadOnly(ServerState::API_TRUE);

  um.setAuthInfo(userEntryMap);
  auth::Level authLevel =
      um.databaseAuthLevel("test", "test", /*configured*/ true);
  ASSERT_EQ(authLevel, auth::Level::RW);
}

TEST_F(
    UserManagerTest,
    setting_serverstate_to_readonly_will_make_all_users_effective_ro_users_collection_level) {
  auth::UserMap userEntryMap;
  auto testUser = auth::User::newUser("test", "test");
  testUser.grantDatabase("*", auth::Level::RW);
  testUser.grantCollection("test", "test", auth::Level::RW);
  userEntryMap.emplace("test", testUser);

  state->setReadOnly(ServerState::API_TRUE);

  um.setAuthInfo(userEntryMap);
  auth::Level authLevel = um.collectionAuthLevel("test", "test", "test");
  ASSERT_EQ(authLevel, auth::Level::RO);
}

TEST_F(
    UserManagerTest,
    in_readonly_mode_the_configured_access_level_will_still_be_accessible_collection_level) {
  auth::UserMap userEntryMap;
  auto testUser = auth::User::newUser("test", "test");
  testUser.grantDatabase("*", auth::Level::RW);
  testUser.grantCollection("test", "test", auth::Level::RW);
  userEntryMap.emplace("test", testUser);

  state->setReadOnly(ServerState::API_TRUE);

  um.setAuthInfo(userEntryMap);
  auth::Level authLevel =
      um.collectionAuthLevel("test", "test", "test", /*configured*/ true);
  ASSERT_EQ(authLevel, auth::Level::RW);
}

TEST_F(UserManagerTest, usermanager_should_throw_if_called_too_early) {
  // we never start the internal thread
  // so the internal version stays 0 and every call to the following functions
  // should lead to a `TRI_ERROR_STARTING_UP` exception
  auto const ShouldThrow = [](std::string_view const name,
                              std::function<void()> const& f) {
    try {
      f();
      FAIL() << name << " should have thrown";
    } catch (basics::Exception const& e) {
      ASSERT_EQ(e.code(), TRI_ERROR_STARTING_UP);
    }
  };
  ShouldThrow("storeUser",
              [&] { um.storeUser(true, "username", "password", true, {}); });
  ShouldThrow("enumerateUsers",
              [&] { um.enumerateUsers([](auto&) { return true; }, true); });
  ShouldThrow("updateUser", [&] {
    um.updateUser("username", [](auto&) { return Result(); });
  });
  ShouldThrow("accessUser", [&] {
    um.accessUser("username", [](auto&) { return Result(); });
  });
  ShouldThrow("userExists", [&] { um.userExists("username"); });
  ShouldThrow("serializeUser", [&] { um.serializeUser("username"); });
  ShouldThrow("removeUser", [&] { um.removeUser("username"); });
  ShouldThrow("removeAllUsers", [&] { um.removeAllUsers(); });
  ShouldThrow("databaseAuthLevel",
              [&] { um.databaseAuthLevel("username", "dbname", true); });
  ShouldThrow("collectionAuthLevel", [&] {
    um.collectionAuthLevel("username", "dbname", "collection", true);
  });

  ShouldThrow("checkCredentials", [&] {
    std::string un;
    um.checkCredentials("username", "dbname", un);
  });
}

}  // namespace auth_info_test
}  // namespace tests
}  // namespace arangodb
