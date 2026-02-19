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

struct TestExecContext : public ExecContext {
  TestExecContext(std::string const& user, std::string const& database,
                  auth::Level systemLevel, auth::Level dbLevel,
                  bool isAdminUser, std::vector<std::string> const& roles = {},
                  std::string const& jwtToken = "")
      : ExecContext(ConstructorToken{}, Type::Default, user, database,
                    systemLevel, dbLevel, isAdminUser, roles, jwtToken) {}

  TestExecContext(bool isInternal, std::string const& user,
                  std::string const& database, auth::Level systemLevel,
                  auth::Level dbLevel, bool isAdminUser)
      : ExecContext(ConstructorToken{},
                    isInternal ? Type::Internal : Type::Default, user, database,
                    systemLevel, dbLevel, isAdminUser) {}
};

// --- Construction ---

TEST(ExecContextTest, basic_construction) {
  TestExecContext ctx("testuser", "testdb", auth::Level::RW, auth::Level::RW,
                      true);

  EXPECT_EQ(ctx.user(), "testuser");
  EXPECT_EQ(ctx.database(), "testdb");
  EXPECT_EQ(ctx.systemAuthLevel(), auth::Level::RW);
  EXPECT_EQ(ctx.databaseAuthLevel(), auth::Level::RW);
  EXPECT_TRUE(ctx.isAdminUser());
  EXPECT_FALSE(ctx.isInternal());
  EXPECT_FALSE(ctx.isSuperuser());
  EXPECT_FALSE(ctx.isReadOnly());
  EXPECT_FALSE(ctx.hasJwtToken());
  EXPECT_TRUE(ctx.jwtToken().empty());
  EXPECT_TRUE(ctx.roles().empty());
}

TEST(ExecContextTest, construction_with_jwt_and_roles) {
  std::vector<std::string> roles = {"admin", "developer", "viewer"};
  std::string jwtToken = "eyJhbGciOiJIUzI1NiJ9.test.signature";

  TestExecContext ctx("jwtuser", "testdb", auth::Level::RW, auth::Level::RO,
                      true, roles, jwtToken);

  EXPECT_EQ(ctx.user(), "jwtuser");
  EXPECT_EQ(ctx.database(), "testdb");
  EXPECT_EQ(ctx.systemAuthLevel(), auth::Level::RW);
  EXPECT_EQ(ctx.databaseAuthLevel(), auth::Level::RO);
  EXPECT_TRUE(ctx.isAdminUser());
  EXPECT_TRUE(ctx.hasJwtToken());
  EXPECT_EQ(ctx.jwtToken(), jwtToken);

  auto const& ctxRoles = ctx.roles();
  ASSERT_EQ(ctxRoles.size(), 3u);
  EXPECT_EQ(ctxRoles[0], "admin");
  EXPECT_EQ(ctxRoles[1], "developer");
  EXPECT_EQ(ctxRoles[2], "viewer");
}

TEST(ExecContextTest, duplicate_roles_are_preserved) {
  std::vector<std::string> roles = {"admin", "dev", "admin"};
  TestExecContext ctx("user", "db", auth::Level::RW, auth::Level::RW, false,
                      roles, "tok");

  auto const& ctxRoles = ctx.roles();
  ASSERT_EQ(ctxRoles.size(), 3u);
  EXPECT_EQ(ctxRoles[0], "admin");
  EXPECT_EQ(ctxRoles[2], "admin");
}

// --- isSuperuser() predicate ---

TEST(ExecContextTest, superuser_requires_internal_and_rw_rw) {
  TestExecContext ctx(true, "", "", auth::Level::RW, auth::Level::RW, true);

  EXPECT_TRUE(ctx.isSuperuser());
  EXPECT_FALSE(ctx.isReadOnly());
}

TEST(ExecContextTest, internal_ro_ro_is_readonly_not_superuser) {
  TestExecContext ctx(true, "", "db", auth::Level::RO, auth::Level::RO, false);

  EXPECT_FALSE(ctx.isSuperuser());
  EXPECT_TRUE(ctx.isReadOnly());
}

TEST(ExecContextTest, internal_rw_ro_is_neither_superuser_nor_readonly) {
  TestExecContext ctx(true, "", "db", auth::Level::RW, auth::Level::RO, false);

  EXPECT_FALSE(ctx.isSuperuser());
  EXPECT_FALSE(ctx.isReadOnly());
}

TEST(ExecContextTest, internal_ro_rw_is_readonly_not_superuser) {
  TestExecContext ctx(true, "", "db", auth::Level::RO, auth::Level::RW, false);

  EXPECT_FALSE(ctx.isSuperuser());
  EXPECT_TRUE(ctx.isReadOnly());
}

TEST(ExecContextTest, internal_none_none_is_neither) {
  TestExecContext ctx(true, "", "db", auth::Level::NONE, auth::Level::NONE,
                      false);

  EXPECT_FALSE(ctx.isSuperuser());
  EXPECT_FALSE(ctx.isReadOnly());
}

TEST(ExecContextTest, default_rw_rw_is_not_superuser) {
  TestExecContext ctx("user", "db", auth::Level::RW, auth::Level::RW, true);

  EXPECT_FALSE(ctx.isSuperuser());
  EXPECT_FALSE(ctx.isReadOnly());
}

TEST(ExecContextTest, default_ro_ro_is_not_superuser_not_readonly) {
  // isReadOnly requires isInternal
  TestExecContext ctx("user", "db", auth::Level::RO, auth::Level::RO, false);

  EXPECT_FALSE(ctx.isSuperuser());
  EXPECT_FALSE(ctx.isReadOnly());
}

// --- canUseDatabase (single-arg: checks _databaseAuthLevel) ---

TEST(ExecContextTest, canUseDatabase_single_arg_level_check) {
  TestExecContext ctx("user", "testdb", auth::Level::RW, auth::Level::RO,
                      false);

  EXPECT_TRUE(ctx.canUseDatabase(auth::Level::NONE));
  EXPECT_TRUE(ctx.canUseDatabase(auth::Level::RO));
  EXPECT_FALSE(ctx.canUseDatabase(auth::Level::RW));
}

TEST(ExecContextTest, canUseDatabase_single_arg_rw_grants_all) {
  TestExecContext ctx("user", "testdb", auth::Level::RW, auth::Level::RW,
                      true);

  EXPECT_TRUE(ctx.canUseDatabase(auth::Level::NONE));
  EXPECT_TRUE(ctx.canUseDatabase(auth::Level::RO));
  EXPECT_TRUE(ctx.canUseDatabase(auth::Level::RW));
}

TEST(ExecContextTest, canUseDatabase_single_arg_none_grants_only_none) {
  TestExecContext ctx("user", "testdb", auth::Level::NONE, auth::Level::NONE,
                      false);

  EXPECT_TRUE(ctx.canUseDatabase(auth::Level::NONE));
  EXPECT_FALSE(ctx.canUseDatabase(auth::Level::RO));
  EXPECT_FALSE(ctx.canUseDatabase(auth::Level::RW));
}

// --- canUseDatabase (two-arg: internal and same-database paths) ---

TEST(ExecContextTest, canUseDatabase_internal_uses_dbAuthLevel) {
  TestExecContext ctx(true, "", "", auth::Level::RW, auth::Level::RW, true);

  EXPECT_TRUE(ctx.canUseDatabase("anydb", auth::Level::RW));
  EXPECT_TRUE(ctx.canUseDatabase("anotherdb", auth::Level::RO));
}

TEST(ExecContextTest, canUseDatabase_internal_ro_rejects_rw) {
  TestExecContext ctx(true, "", "", auth::Level::RO, auth::Level::RO, false);

  EXPECT_TRUE(ctx.canUseDatabase("anydb", auth::Level::RO));
  EXPECT_FALSE(ctx.canUseDatabase("anydb", auth::Level::RW));
}

TEST(ExecContextTest, canUseDatabase_same_db_uses_dbAuthLevel) {
  TestExecContext ctx("user", "mydb", auth::Level::RW, auth::Level::RO, false);

  EXPECT_TRUE(ctx.canUseDatabase("mydb", auth::Level::RO));
  EXPECT_FALSE(ctx.canUseDatabase("mydb", auth::Level::RW));
}

// --- collectionAuthLevel (internal path) ---

TEST(ExecContextTest, collectionAuthLevel_internal_returns_dbAuthLevel) {
  TestExecContext ctx(true, "", "", auth::Level::RW, auth::Level::RW, true);

  EXPECT_EQ(ctx.collectionAuthLevel("anydb", "anycoll"), auth::Level::RW);
}

TEST(ExecContextTest, collectionAuthLevel_internal_ro_returns_ro) {
  TestExecContext ctx(true, "", "", auth::Level::RO, auth::Level::RO, false);

  EXPECT_EQ(ctx.collectionAuthLevel("anydb", "anycoll"), auth::Level::RO);
}

// --- Static superuser singleton ---

TEST(ExecContextTest, superuser_singleton) {
  auto const& su = ExecContext::superuser();

  EXPECT_TRUE(su.isInternal());
  EXPECT_TRUE(su.isSuperuser());
  EXPECT_FALSE(su.isReadOnly());
  EXPECT_TRUE(su.isAdminUser());
  EXPECT_EQ(su.systemAuthLevel(), auth::Level::RW);
  EXPECT_EQ(su.databaseAuthLevel(), auth::Level::RW);
  EXPECT_TRUE(su.canUseDatabase(auth::Level::RW));
}

TEST(ExecContextTest, superuser_as_shared_returns_same_object) {
  auto ptr = ExecContext::superuserAsShared();

  ASSERT_NE(ptr, nullptr);
  EXPECT_EQ(ptr.get(), &ExecContext::superuser());
}

// --- current() / currentAsShared() / set() ---

TEST(ExecContextTest, current_returns_superuser_when_no_context_set) {
  // CURRENT is thread_local and starts as nullptr in a fresh thread.
  // current() should fall back to superuser.
  auto old = ExecContext::set(nullptr);

  EXPECT_TRUE(ExecContext::current().isSuperuser());
  EXPECT_EQ(ExecContext::currentAsShared(), nullptr);

  ExecContext::set(old);
}

TEST(ExecContextTest, set_swaps_and_returns_old_value) {
  auto old = ExecContext::set(nullptr);

  auto ctx =
      std::make_shared<TestExecContext>("u", "db", auth::Level::RO,
                                        auth::Level::RO, false);
  auto prev = ExecContext::set(ctx);
  EXPECT_EQ(prev, nullptr);
  EXPECT_EQ(ExecContext::currentAsShared(), ctx);
  EXPECT_EQ(ExecContext::current().user(), "u");

  auto prev2 = ExecContext::set(old);
  EXPECT_EQ(prev2, ctx);
}

// --- ExecContextScope RAII ---

TEST(ExecContextTest, scope_sets_and_restores_current) {
  auto original = ExecContext::currentAsShared();

  auto ctx =
      std::make_shared<TestExecContext>("scoped", "db", auth::Level::RW,
                                        auth::Level::RW, false);
  {
    ExecContextScope scope(ctx);
    EXPECT_EQ(ExecContext::current().user(), "scoped");
    EXPECT_EQ(ExecContext::currentAsShared(), ctx);
  }

  EXPECT_EQ(ExecContext::currentAsShared(), original);
}

TEST(ExecContextTest, nested_scopes_restore_correctly) {
  auto original = ExecContext::currentAsShared();

  auto ctx1 =
      std::make_shared<TestExecContext>("outer", "db", auth::Level::RW,
                                        auth::Level::RW, false);
  auto ctx2 =
      std::make_shared<TestExecContext>("inner", "db", auth::Level::RO,
                                        auth::Level::RO, false);
  {
    ExecContextScope outer(ctx1);
    EXPECT_EQ(ExecContext::current().user(), "outer");
    {
      ExecContextScope inner(ctx2);
      EXPECT_EQ(ExecContext::current().user(), "inner");
    }
    EXPECT_EQ(ExecContext::current().user(), "outer");
  }

  EXPECT_EQ(ExecContext::currentAsShared(), original);
}

// --- ExecContextSuperuserScope RAII ---

TEST(ExecContextTest, superuser_scope_sets_and_restores) {
  auto original = ExecContext::currentAsShared();

  auto ctx =
      std::make_shared<TestExecContext>("regular", "db", auth::Level::RO,
                                        auth::Level::RO, false);
  {
    ExecContextScope setup(ctx);
    EXPECT_EQ(ExecContext::current().user(), "regular");
    {
      ExecContextSuperuserScope su;
      EXPECT_TRUE(ExecContext::current().isSuperuser());
    }
    EXPECT_EQ(ExecContext::current().user(), "regular");
  }

  EXPECT_EQ(ExecContext::currentAsShared(), original);
}

TEST(ExecContextTest, superuser_scope_false_is_noop) {
  auto original = ExecContext::currentAsShared();

  auto ctx =
      std::make_shared<TestExecContext>("regular", "db", auth::Level::RO,
                                        auth::Level::RO, false);
  {
    ExecContextScope setup(ctx);
    EXPECT_EQ(ExecContext::current().user(), "regular");
    {
      ExecContextSuperuserScope noop(false);
      EXPECT_EQ(ExecContext::current().user(), "regular");
      EXPECT_FALSE(ExecContext::current().isSuperuser());
    }
    EXPECT_EQ(ExecContext::current().user(), "regular");
  }

  EXPECT_EQ(ExecContext::currentAsShared(), original);
}

}  // namespace arangodb::tests
