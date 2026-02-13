////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
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
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "Auth/Common.h"
#include "Utils/ExecContext.h"

#include <memory>
#include <string>
#include <vector>

using namespace arangodb;

namespace arangodb::tests {

// Helper class for testing - provides public access to ExecContext constructor
struct TestExecContext : public ExecContext {
  // Constructor for default type contexts (most common case)
  TestExecContext(std::string const& user, std::string const& database,
                  auth::Level systemLevel, auth::Level dbLevel,
                  bool isAdminUser, std::vector<std::string> const& roles = {},
                  std::string const& jwtToken = "")
      : ExecContext(ConstructorToken{}, Type::Default, user, database,
                    systemLevel, dbLevel, isAdminUser, roles, jwtToken) {}

  // Constructor for internal type contexts - use isInternal=true for internal
  TestExecContext(bool isInternal, std::string const& user,
                  std::string const& database, auth::Level systemLevel,
                  auth::Level dbLevel, bool isAdminUser)
      : ExecContext(ConstructorToken{},
                    isInternal ? Type::Internal : Type::Default, user, database,
                    systemLevel, dbLevel, isAdminUser) {}
};

class ExecContextTest : public ::testing::Test {
 protected:
  ExecContextTest() = default;
  ~ExecContextTest() override = default;
};

// Test basic ExecContext construction with default parameters
TEST_F(ExecContextTest, test_basic_construction) {
  auto ctx = std::make_shared<TestExecContext>(
      "testuser", "testdb", auth::Level::RW, auth::Level::RW, true);

  EXPECT_EQ(ctx->user(), "testuser");
  EXPECT_EQ(ctx->database(), "testdb");
  EXPECT_EQ(ctx->systemAuthLevel(), auth::Level::RW);
  EXPECT_EQ(ctx->databaseAuthLevel(), auth::Level::RW);
  EXPECT_TRUE(ctx->isAdminUser());
  EXPECT_FALSE(ctx->isInternal());
  EXPECT_FALSE(ctx->isSuperuser());
  EXPECT_FALSE(ctx->isReadOnly());
}

// Test ExecContext construction with JWT token and roles
TEST_F(ExecContextTest, test_construction_with_jwt_and_roles) {
  std::vector<std::string> roles = {"admin", "developer", "viewer"};
  std::string jwtToken = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.test.signature";

  auto ctx =
      std::make_shared<TestExecContext>("jwtuser", "testdb", auth::Level::RW,
                                        auth::Level::RO, true, roles, jwtToken);

  EXPECT_EQ(ctx->user(), "jwtuser");
  EXPECT_EQ(ctx->database(), "testdb");
  EXPECT_EQ(ctx->systemAuthLevel(), auth::Level::RW);
  EXPECT_EQ(ctx->databaseAuthLevel(), auth::Level::RO);
  EXPECT_TRUE(ctx->isAdminUser());

  // Test JWT-specific functionality
  EXPECT_TRUE(ctx->hasJwtToken());
  EXPECT_EQ(ctx->jwtToken(), jwtToken);

  const auto& ctxRoles = ctx->roles();
  EXPECT_EQ(ctxRoles.size(), 3);
  EXPECT_EQ(ctxRoles[0], "admin");
  EXPECT_EQ(ctxRoles[1], "developer");
  EXPECT_EQ(ctxRoles[2], "viewer");
}

// Test ExecContext without JWT token
TEST_F(ExecContextTest, test_no_jwt_token) {
  auto ctx = std::make_shared<TestExecContext>(
      "basicuser", "testdb", auth::Level::RO, auth::Level::RO, false);

  EXPECT_FALSE(ctx->hasJwtToken());
  EXPECT_TRUE(ctx->jwtToken().empty());
  EXPECT_TRUE(ctx->roles().empty());
}

// Test ExecContext with empty roles but with JWT token
TEST_F(ExecContextTest, test_jwt_token_without_roles) {
  std::vector<std::string> emptyRoles;
  std::string jwtToken = "token.without.roles";

  auto ctx = std::make_shared<TestExecContext>("user", "testdb",
                                               auth::Level::RW, auth::Level::RW,
                                               true, emptyRoles, jwtToken);

  EXPECT_TRUE(ctx->hasJwtToken());
  EXPECT_EQ(ctx->jwtToken(), jwtToken);
  EXPECT_TRUE(ctx->roles().empty());
}

// Test ExecContext with roles but without JWT token (edge case)
TEST_F(ExecContextTest, test_roles_without_jwt_token) {
  std::vector<std::string> roles = {"role1", "role2"};

  auto ctx = std::make_shared<TestExecContext>(
      "user", "testdb", auth::Level::RW, auth::Level::RW, true, roles, "");

  EXPECT_FALSE(ctx->hasJwtToken());
  EXPECT_TRUE(ctx->jwtToken().empty());

  // Roles should still be accessible even without JWT token
  const auto& ctxRoles = ctx->roles();
  EXPECT_EQ(ctxRoles.size(), 2);
  EXPECT_EQ(ctxRoles[0], "role1");
  EXPECT_EQ(ctxRoles[1], "role2");
}

// Test internal superuser context
TEST_F(ExecContextTest, test_superuser_context) {
  auto ctx = std::make_shared<TestExecContext>(true, "", "", auth::Level::RW,
                                               auth::Level::RW, true);

  EXPECT_TRUE(ctx->user().empty());
  EXPECT_TRUE(ctx->database().empty());
  EXPECT_TRUE(ctx->isInternal());
  EXPECT_TRUE(ctx->isSuperuser());
  EXPECT_FALSE(ctx->isReadOnly());
  EXPECT_TRUE(ctx->isAdminUser());
  EXPECT_EQ(ctx->systemAuthLevel(), auth::Level::RW);
  EXPECT_EQ(ctx->databaseAuthLevel(), auth::Level::RW);
}

// Test read-only internal context
TEST_F(ExecContextTest, test_readonly_internal_context) {
  auto ctx = std::make_shared<TestExecContext>(
      true, "", "testdb", auth::Level::RO, auth::Level::RO, false);

  EXPECT_TRUE(ctx->isInternal());
  EXPECT_FALSE(ctx->isSuperuser());  // RO is not superuser
  EXPECT_TRUE(ctx->isReadOnly());
  EXPECT_FALSE(ctx->isAdminUser());
  EXPECT_EQ(ctx->systemAuthLevel(), auth::Level::RO);
  EXPECT_EQ(ctx->databaseAuthLevel(), auth::Level::RO);
}

// Test canUseDatabase method
TEST_F(ExecContextTest, test_can_use_database) {
  auto ctx = std::make_shared<TestExecContext>(
      "user", "testdb", auth::Level::RW, auth::Level::RO, false);

  // Should be able to use database with RO access
  EXPECT_TRUE(ctx->canUseDatabase(auth::Level::RO));
  EXPECT_TRUE(ctx->canUseDatabase(auth::Level::NONE));

  // Should NOT be able to use database with RW access (only has RO)
  EXPECT_FALSE(ctx->canUseDatabase(auth::Level::RW));
}

// Test superuser context
TEST_F(ExecContextTest, test_superuser_access) {
  auto& superuser = ExecContext::superuser();

  EXPECT_TRUE(superuser.isInternal());
  EXPECT_TRUE(superuser.isSuperuser());
  EXPECT_FALSE(superuser.isReadOnly());
  EXPECT_TRUE(superuser.isAdminUser());
  EXPECT_EQ(superuser.systemAuthLevel(), auth::Level::RW);
  EXPECT_EQ(superuser.databaseAuthLevel(), auth::Level::RW);
  EXPECT_TRUE(superuser.canUseDatabase(auth::Level::RW));
}

// Test superuser shared pointer
TEST_F(ExecContextTest, test_superuser_as_shared) {
  auto superuserPtr = ExecContext::superuserAsShared();

  ASSERT_NE(superuserPtr, nullptr);
  EXPECT_TRUE(superuserPtr->isSuperuser());
  EXPECT_TRUE(superuserPtr->isInternal());
}

// Test ExecContext with different auth levels
TEST_F(ExecContextTest, test_different_auth_levels) {
  auto ctxNone = std::make_shared<TestExecContext>(
      "user1", "testdb", auth::Level::NONE, auth::Level::NONE, false);

  auto ctxRO = std::make_shared<TestExecContext>(
      "user2", "testdb", auth::Level::RO, auth::Level::RO, false);

  auto ctxRW = std::make_shared<TestExecContext>(
      "user3", "testdb", auth::Level::RW, auth::Level::RW, true);

  EXPECT_FALSE(ctxNone->canUseDatabase(auth::Level::RO));
  EXPECT_TRUE(ctxNone->canUseDatabase(auth::Level::NONE));

  EXPECT_TRUE(ctxRO->canUseDatabase(auth::Level::RO));
  EXPECT_FALSE(ctxRO->canUseDatabase(auth::Level::RW));

  EXPECT_TRUE(ctxRW->canUseDatabase(auth::Level::RW));
  EXPECT_TRUE(ctxRW->canUseDatabase(auth::Level::RO));
  EXPECT_TRUE(ctxRW->canUseDatabase(auth::Level::NONE));
}

// Test JWT token with special characters
TEST_F(ExecContextTest, test_jwt_token_with_special_characters) {
  std::string complexToken =
      "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9."
      "eyJzdWIiOiIxMjM0NTY3ODkwIiwibmFtZSI6IkpvaG4gRG9lIiwiaWF0IjoxNTE2MjM5MDIy"
      "fQ."
      "SflKxwRJSMeKKF2QT4fwpMeJf36POk6yJV_adQssw5c";

  auto ctx = std::make_shared<TestExecContext>(
      "user", "testdb", auth::Level::RW, auth::Level::RW, true,
      std::vector<std::string>{}, complexToken);

  EXPECT_TRUE(ctx->hasJwtToken());
  EXPECT_EQ(ctx->jwtToken(), complexToken);
}

// Test roles with special characters
TEST_F(ExecContextTest, test_roles_with_special_characters) {
  std::vector<std::string> specialRoles = {
      "admin-role", "developer_role", "viewer.read-only", "role:with:colons",
      "role/with/slashes"};

  auto ctx = std::make_shared<TestExecContext>("user", "testdb",
                                               auth::Level::RW, auth::Level::RW,
                                               true, specialRoles, "token");

  const auto& ctxRoles = ctx->roles();
  EXPECT_EQ(ctxRoles.size(), specialRoles.size());
  for (size_t i = 0; i < specialRoles.size(); ++i) {
    EXPECT_EQ(ctxRoles[i], specialRoles[i]);
  }
}

// Test multiple roles with duplicates
TEST_F(ExecContextTest, test_multiple_roles_with_duplicates) {
  std::vector<std::string> rolesWithDuplicates = {"admin", "developer", "admin",
                                                  "viewer", "developer"};

  auto ctx = std::make_shared<TestExecContext>(
      "user", "testdb", auth::Level::RW, auth::Level::RW, true,
      rolesWithDuplicates, "token");

  // The ExecContext stores roles as-is, including duplicates
  const auto& ctxRoles = ctx->roles();
  EXPECT_EQ(ctxRoles.size(), rolesWithDuplicates.size());
  EXPECT_EQ(ctxRoles[0], "admin");
  EXPECT_EQ(ctxRoles[2], "admin");
}

// Test empty strings
TEST_F(ExecContextTest, test_empty_strings) {
  auto ctx = std::make_shared<TestExecContext>("", "", auth::Level::NONE,
                                               auth::Level::NONE, false);

  EXPECT_TRUE(ctx->user().empty());
  EXPECT_TRUE(ctx->database().empty());
  EXPECT_FALSE(ctx->hasJwtToken());
  EXPECT_TRUE(ctx->roles().empty());
}

// Test admin user flag combinations
TEST_F(ExecContextTest, test_admin_user_flag) {
  auto adminCtx = std::make_shared<TestExecContext>(
      "admin", "_system", auth::Level::RW, auth::Level::RW, true);

  auto nonAdminCtx = std::make_shared<TestExecContext>(
      "user", "userdb", auth::Level::RW, auth::Level::RW, false);

  EXPECT_TRUE(adminCtx->isAdminUser());
  EXPECT_FALSE(nonAdminCtx->isAdminUser());
}

// Test system database vs user database
TEST_F(ExecContextTest, test_system_vs_user_database) {
  auto sysCtx = std::make_shared<TestExecContext>(
      "admin", "_system", auth::Level::RW, auth::Level::RW, true);

  auto userCtx = std::make_shared<TestExecContext>(
      "user", "mydb", auth::Level::RO, auth::Level::RO, false);

  EXPECT_EQ(sysCtx->database(), "_system");
  EXPECT_EQ(sysCtx->systemAuthLevel(), auth::Level::RW);

  EXPECT_EQ(userCtx->database(), "mydb");
  EXPECT_EQ(userCtx->systemAuthLevel(), auth::Level::RO);
  EXPECT_EQ(userCtx->databaseAuthLevel(), auth::Level::RO);
}

// Test large number of roles
TEST_F(ExecContextTest, test_large_number_of_roles) {
  std::vector<std::string> manyRoles;
  for (int i = 0; i < 100; ++i) {
    manyRoles.push_back("role_" + std::to_string(i));
  }

  auto ctx = std::make_shared<TestExecContext>("user", "testdb",
                                               auth::Level::RW, auth::Level::RW,
                                               true, manyRoles, "token");

  const auto& ctxRoles = ctx->roles();
  EXPECT_EQ(ctxRoles.size(), 100);
  EXPECT_EQ(ctxRoles[0], "role_0");
  EXPECT_EQ(ctxRoles[99], "role_99");
}

// Test very long JWT token
TEST_F(ExecContextTest, test_long_jwt_token) {
  std::string longToken(10000, 'a');  // 10KB token
  longToken += ".";
  longToken += std::string(10000, 'b');
  longToken += ".";
  longToken += std::string(10000, 'c');

  auto ctx = std::make_shared<TestExecContext>(
      "user", "testdb", auth::Level::RW, auth::Level::RW, true,
      std::vector<std::string>{}, longToken);

  EXPECT_TRUE(ctx->hasJwtToken());
  EXPECT_EQ(ctx->jwtToken().length(), 30002);  // 3*10000 + 2 dots
}

}  // namespace arangodb::tests
