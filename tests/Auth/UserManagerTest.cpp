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

#include "catch.hpp"
#include "fakeit.hpp"

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

class TestAuthenticationHandler: public auth::Handler {
 public:
  TestAuthenticationHandler() {}
  auth::HandlerResult authenticate(std::string const& username,
                                    std::string const& password) {
    
    std::unordered_map<std::string, auth::Level> permissions {};
    std::unordered_set<std::string> roles {};
    auth::Source source = auth::Source::COLLECTION;
    auth::HandlerResult result(permissions, roles, source);
    return result;
  }
  virtual ~TestAuthenticationHandler() {}
};

class TestQueryRegistry: public QueryRegistry {
 public:
  TestQueryRegistry() {}; 
  virtual ~TestQueryRegistry() {}
};

class TestDatabaseFeature: public DatabaseFeature {
 public:
  TestDatabaseFeature(application_features::ApplicationServer* server): DatabaseFeature(server) {}; 
};


TEST_CASE("🥑🔐 UserManager", "[authentication]") {
  auto authHandler = std::make_unique<TestAuthenticationHandler>();
  TestQueryRegistry queryRegistry;

  auto state = ServerState::instance();
  state->setRole(ServerState::ROLE_SINGLE);

  Mock<DatabaseFeature> databaseFeatureMock;
  DatabaseFeature &databaseFeature = databaseFeatureMock.get();
  DatabaseFeature::DATABASE = &databaseFeature;

  auth::UserManager um(std::move(authHandler));
  um.setQueryRegistry(&queryRegistry);
  
  SECTION("An unknown user will have no access") {
    auth::UserMap userEntryMap;
    um.setAuthInfo(userEntryMap);
    auth::Level authLevel = um.canUseDatabase("test", "test");
    REQUIRE(authLevel == auth::Level::NONE);
  }

  SECTION("Granting RW access on database * will grant access to all databases") {
    auth::UserMap userEntryMap;
    auto testUser = auth::User::newUser("test", "test", auth::Source::COLLECTION);
    testUser.grantDatabase("*", auth::Level::RW);
    userEntryMap.emplace("test", testUser);

    um.setAuthInfo(userEntryMap);
    auth::Level authLevel = um.canUseDatabase("test", "test");
    REQUIRE(authLevel == auth::Level::RW);
  }

  SECTION("Setting ServerState to readonly will make all users effective RO users") {
    auth::UserMap userEntryMap;
    auto testUser = auth::User::newUser("test", "test", auth::Source::COLLECTION);
    testUser.grantDatabase("*", auth::Level::RW);
    userEntryMap.emplace("test", testUser);

    state->setServerMode(ServerState::Mode::READ_ONLY);

    um.setAuthInfo(userEntryMap);
    auth::Level authLevel = um.canUseDatabase("test", "test");
    REQUIRE(authLevel == auth::Level::RO);
  }

  SECTION("In readonly mode the configured access level will still be accessible") {
    auth::UserMap userEntryMap;
    auto testUser = auth::User::newUser("test", "test", auth::Source::COLLECTION);
    testUser.grantDatabase("*", auth::Level::RW);
    userEntryMap.emplace("test", testUser);

    state->setServerMode(ServerState::Mode::READ_ONLY);

    um.setAuthInfo(userEntryMap);
    auth::Level authLevel = um.configuredDatabaseAuthLevel("test", "test");
    REQUIRE(authLevel == auth::Level::RW);
  }

  SECTION("Setting ServerState to readonly will make all users effective RO users (collection level)") {
    auth::UserMap userEntryMap;
    auto testUser = auth::User::newUser("test", "test", auth::Source::COLLECTION);
    testUser.grantDatabase("*", auth::Level::RW);
    testUser.grantCollection("test", "test", auth::Level::RW);
    userEntryMap.emplace("test", testUser);

    state->setServerMode(ServerState::Mode::READ_ONLY);

    um.setAuthInfo(userEntryMap);
    auth::Level authLevel = um.canUseCollection("test", "test", "test");
    REQUIRE(authLevel == auth::Level::RO);
  }

  SECTION("In readonly mode the configured access level will still be accessible (collection level)") {
    auth::UserMap userEntryMap;
    auto testUser = auth::User::newUser("test", "test", auth::Source::COLLECTION);
    testUser.grantDatabase("*", auth::Level::RW);
    testUser.grantCollection("test", "test", auth::Level::RW);
    userEntryMap.emplace("test", testUser);

    state->setServerMode(ServerState::Mode::READ_ONLY);

    um.setAuthInfo(userEntryMap);
    auth::Level authLevel = um.configuredCollectionAuthLevel("test", "test", "test");
    REQUIRE(authLevel == auth::Level::RW);
  }

  state->setServerMode(ServerState::Mode::DEFAULT);
}

}
}
}
