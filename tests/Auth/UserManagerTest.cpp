////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for CuckooFilter based index selectivity estimator
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
  application_features::ApplicationServer server;
  TestQueryRegistry queryRegistry;
  ServerState* state;
  Mock<DatabaseFeature> databaseFeatureMock;
  DatabaseFeature& databaseFeature;
  auth::UserManager um;

  UserManagerTest()
      : server(nullptr, nullptr),
        state(ServerState::instance()),
        databaseFeature(databaseFeatureMock.get()),
        um(server) {
    state->setRole(ServerState::ROLE_SINGLE);
    DatabaseFeature::DATABASE = &databaseFeature;
  }

  ~UserManagerTest() {
    state->setServerMode(ServerState::Mode::DEFAULT);
    state->setReadOnly(false);
  }
};

TEST_F(UserManagerTest, unknown_user_will_have_no_access) {
  auth::UserMap userEntryMap;
  um.setAuthInfo(userEntryMap);
  auth::Level authLevel = um.databaseAuthLevel("test", "test");
  ASSERT_EQ(authLevel, auth::Level::NONE);
}

TEST_F(UserManagerTest, granting_rw_access_on_database_star_will_grant_to_all_databases) {
  auth::UserMap userEntryMap;
  auto testUser = auth::User::newUser("test", "test", auth::Source::Local);
  testUser.grantDatabase("*", auth::Level::RW);
  userEntryMap.emplace("test", testUser);

  um.setAuthInfo(userEntryMap);
  auth::Level authLevel = um.databaseAuthLevel("test", "test");
  ASSERT_EQ(authLevel, auth::Level::RW);
}

TEST_F(UserManagerTest, setting_serverstate_to_readonly_will_make_all_users_effectively_ro_users) {
  auth::UserMap userEntryMap;
  auto testUser = auth::User::newUser("test", "test", auth::Source::Local);
  testUser.grantDatabase("*", auth::Level::RW);
  userEntryMap.emplace("test", testUser);

  state->setReadOnly(true);

  um.setAuthInfo(userEntryMap);
  auth::Level authLevel = um.databaseAuthLevel("test", "test");
  ASSERT_EQ(authLevel, auth::Level::RO);
}

TEST_F(UserManagerTest, in_readonly_mode_the_configured_access_level_will_still_be_accessible) {
  auth::UserMap userEntryMap;
  auto testUser = auth::User::newUser("test", "test", auth::Source::Local);
  testUser.grantDatabase("*", auth::Level::RW);
  userEntryMap.emplace("test", testUser);

  state->setReadOnly(true);

  um.setAuthInfo(userEntryMap);
  auth::Level authLevel = um.databaseAuthLevel("test", "test", /*configured*/ true);
  ASSERT_EQ(authLevel, auth::Level::RW);
}

TEST_F(UserManagerTest, setting_serverstate_to_readonly_will_make_all_users_effective_ro_users_collection_level) {
  auth::UserMap userEntryMap;
  auto testUser = auth::User::newUser("test", "test", auth::Source::Local);
  testUser.grantDatabase("*", auth::Level::RW);
  testUser.grantCollection("test", "test", auth::Level::RW);
  userEntryMap.emplace("test", testUser);

  state->setReadOnly(true);

  um.setAuthInfo(userEntryMap);
  auth::Level authLevel = um.collectionAuthLevel("test", "test", "test");
  ASSERT_EQ(authLevel, auth::Level::RO);
}

TEST_F(UserManagerTest, in_readonly_mode_the_configured_access_level_will_still_be_accessible_collection_level) {
  auth::UserMap userEntryMap;
  auto testUser = auth::User::newUser("test", "test", auth::Source::Local);
  testUser.grantDatabase("*", auth::Level::RW);
  testUser.grantCollection("test", "test", auth::Level::RW);
  userEntryMap.emplace("test", testUser);

  state->setReadOnly(true);

  um.setAuthInfo(userEntryMap);
  auth::Level authLevel =
      um.collectionAuthLevel("test", "test", "test", /*configured*/ true);
  ASSERT_EQ(authLevel, auth::Level::RW);
}

}  // namespace auth_info_test
}  // namespace tests
}  // namespace arangodb
