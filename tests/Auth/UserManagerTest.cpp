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

#include "gmock/gmock-matchers.h"
#include "gtest/gtest.h"

#include "fakeit.hpp"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Aql/QueryRegistry.h"
#include "Auth/Handler.h"
#include "Auth/User.h"
#include "Auth/UserManagerImpl.h"
#include "Cluster/ServerState.h"
#include "RestServer/DatabaseFeature.h"

using namespace fakeit;
using namespace arangodb;
using namespace arangodb::aql;

namespace arangodb::tests::auth_info_test {

class TestQueryRegistry : public QueryRegistry {
 public:
  TestQueryRegistry() : QueryRegistry(1.0){};
  ~TestQueryRegistry() override = default;
};

class UserManagerTest : public ::testing::Test {
 protected:
  ArangodServer server;
  TestQueryRegistry queryRegistry;
  ServerState* state;
  auth::UserManagerImpl um;

  UserManagerTest()
      : server(nullptr, nullptr), state(ServerState::instance()), um(server) {
    state->setRole(ServerState::ROLE_SINGLE);

    server.addFeature<DatabaseFeature>();
  }

  ~UserManagerTest() override {
    state->setServerMode(ServerState::Mode::DEFAULT);
    state->setReadOnly(ServerState::API_FALSE);
  }
};

TEST_F(UserManagerTest, unknown_user_will_have_no_access) {
  auth::UserMap userEntryMap;
  um.setAuthInfo(userEntryMap);
  auth::Level authLevel = um.databaseAuthLevel("test", "test", false);
  ASSERT_EQ(authLevel, auth::Level::NONE);
}

TEST_F(UserManagerTest,
       granting_rw_access_on_database_star_will_grant_to_all_databases) {
  auth::UserMap userEntryMap;
  auto testUser = auth::User::newUser("test", "test");
  testUser.grantDatabase("*", auth::Level::RW);
  userEntryMap.emplace("test", testUser);

  um.setAuthInfo(userEntryMap);
  auth::Level authLevel = um.databaseAuthLevel("test", "test", false);
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
  auth::Level authLevel = um.databaseAuthLevel("test", "test", false);
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
  auth::Level authLevel = um.collectionAuthLevel("test", "test", "test", false);
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

  using namespace ::testing;

  EXPECT_THAT([&] { um.storeUser(true, "username", "password", true, {}); },
              Throws<basics::Exception>(
                  Property(&basics::Exception::code, TRI_ERROR_STARTING_UP)));
  EXPECT_THAT(
      [&] {
        um.enumerateUsers([](auto&) { return true; },
                          auth::UserManager::RetryOnConflict::Yes);
      },
      Throws<basics::Exception>(
          Property(&basics::Exception::code, TRI_ERROR_STARTING_UP)));
  EXPECT_THAT(
      [&] {
        um.updateUser(
            "username", [](auto&) { return Result(); },
            auth::UserManager::RetryOnConflict::No);
      },
      Throws<basics::Exception>(
          Property(&basics::Exception::code, TRI_ERROR_STARTING_UP)));
  EXPECT_THAT(
      [&] { um.accessUser("username", [](auto&) { return Result(); }); },
      Throws<basics::Exception>(
          Property(&basics::Exception::code, TRI_ERROR_STARTING_UP)));
  EXPECT_THAT([&] { um.userExists("username"); },
              Throws<basics::Exception>(
                  Property(&basics::Exception::code, TRI_ERROR_STARTING_UP)));
  EXPECT_THAT([&] { um.serializeUser("username"); },
              Throws<basics::Exception>(
                  Property(&basics::Exception::code, TRI_ERROR_STARTING_UP)));
  EXPECT_THAT([&] { um.removeUser("username"); },
              Throws<basics::Exception>(
                  Property(&basics::Exception::code, TRI_ERROR_STARTING_UP)));
  EXPECT_THAT([&] { um.removeAllUsers(); },
              Throws<basics::Exception>(
                  Property(&basics::Exception::code, TRI_ERROR_STARTING_UP)));
  EXPECT_THAT([&] { um.databaseAuthLevel("username", "dbname", true); },
              Throws<basics::Exception>(
                  Property(&basics::Exception::code, TRI_ERROR_STARTING_UP)));
  EXPECT_THAT(
      [&] { um.collectionAuthLevel("username", "dbname", "collection", true); },
      Throws<basics::Exception>(
          Property(&basics::Exception::code, TRI_ERROR_STARTING_UP)));
}

}  // namespace arangodb::tests::auth_info_test
