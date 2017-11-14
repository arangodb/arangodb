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
#include "Cluster/ServerState.h"
#include "RestServer/DatabaseFeature.h"
#include "VocBase/AuthInfo.h"
#include "VocBase/AuthUserEntry.h"

using namespace fakeit;
using namespace arangodb;
using namespace arangodb::aql;

namespace arangodb {
namespace tests {
namespace auth_info_test {

class TestAuthenticationHandler: public AuthenticationHandler {
 public:
  TestAuthenticationHandler() {}
  AuthenticationResult authenticate(std::string const& username,
                                    std::string const& password) {
    
    std::unordered_map<std::string, AuthLevel> permissions {};
    std::unordered_set<std::string> roles {};
    AuthSource source = AuthSource::COLLECTION;
    AuthenticationResult result(permissions, roles, source);
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


TEST_CASE("🥑🔐 AuthInfo", "[authentication]") {
  auto authHandler = std::make_unique<TestAuthenticationHandler>();
  TestQueryRegistry queryRegistry;

  auto state = ServerState::instance();
  state->setRole(ServerState::ROLE_SINGLE);

  Mock<DatabaseFeature> databaseFeatureMock;
  DatabaseFeature &databaseFeature = databaseFeatureMock.get();
  DatabaseFeature::DATABASE = &databaseFeature;

  AuthInfo authInfo(std::move(authHandler));
  authInfo.setQueryRegistry(&queryRegistry);
  
  SECTION("An unknown user will have no access") {
    AuthUserEntryMap userEntryMap;
    authInfo.setAuthInfo(userEntryMap);
    AuthLevel authLevel = authInfo.canUseDatabase("test", "test");
    REQUIRE(authLevel == AuthLevel::NONE);
  }

  SECTION("Granting RW access on database * will grant access to all databases") {
    AuthUserEntryMap userEntryMap;
    auto testUser = AuthUserEntry::newUser("test", "test", AuthSource::COLLECTION);
    testUser.grantDatabase("*", AuthLevel::RW);
    userEntryMap.emplace("test", testUser);

    authInfo.setAuthInfo(userEntryMap);
    AuthLevel authLevel = authInfo.canUseDatabase("test", "test");
    REQUIRE(authLevel == AuthLevel::RW);
  }

  SECTION("Setting ServerState to readonly will make all users effective RO users") {
    AuthUserEntryMap userEntryMap;
    auto testUser = AuthUserEntry::newUser("test", "test", AuthSource::COLLECTION);
    testUser.grantDatabase("*", AuthLevel::RW);
    userEntryMap.emplace("test", testUser);

    state->setServerMode(ServerState::Mode::READ_ONLY);

    authInfo.setAuthInfo(userEntryMap);
    AuthLevel authLevel = authInfo.canUseDatabase("test", "test");
    REQUIRE(authLevel == AuthLevel::RO);
  }

  SECTION("In readonly mode the configured access level will still be accessible") {
    AuthUserEntryMap userEntryMap;
    auto testUser = AuthUserEntry::newUser("test", "test", AuthSource::COLLECTION);
    testUser.grantDatabase("*", AuthLevel::RW);
    userEntryMap.emplace("test", testUser);

    state->setServerMode(ServerState::Mode::READ_ONLY);

    authInfo.setAuthInfo(userEntryMap);
    AuthLevel authLevel = authInfo.configuredDatabaseAuthLevel("test", "test");
    REQUIRE(authLevel == AuthLevel::RW);
  }

  SECTION("Setting ServerState to readonly will make all users effective RO users (collection level)") {
    AuthUserEntryMap userEntryMap;
    auto testUser = AuthUserEntry::newUser("test", "test", AuthSource::COLLECTION);
    testUser.grantDatabase("*", AuthLevel::RW);
    testUser.grantCollection("test", "test", AuthLevel::RW);
    userEntryMap.emplace("test", testUser);

    state->setServerMode(ServerState::Mode::READ_ONLY);

    authInfo.setAuthInfo(userEntryMap);
    AuthLevel authLevel = authInfo.canUseCollection("test", "test", "test");
    REQUIRE(authLevel == AuthLevel::RO);
  }

  SECTION("In readonly mode the configured access level will still be accessible (collection level)") {
    AuthUserEntryMap userEntryMap;
    auto testUser = AuthUserEntry::newUser("test", "test", AuthSource::COLLECTION);
    testUser.grantDatabase("*", AuthLevel::RW);
    testUser.grantCollection("test", "test", AuthLevel::RW);
    userEntryMap.emplace("test", testUser);

    state->setServerMode(ServerState::Mode::READ_ONLY);

    authInfo.setAuthInfo(userEntryMap);
    AuthLevel authLevel = authInfo.configuredCollectionAuthLevel("test", "test", "test");
    REQUIRE(authLevel == AuthLevel::RW);
  }

  state->setServerMode(ServerState::Mode::DEFAULT);
}

}
}
}