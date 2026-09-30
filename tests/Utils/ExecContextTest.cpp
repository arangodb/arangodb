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
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "Auth/AuthMode.h"
#include "Auth/Common.h"
#include "Auth/Rbac/Service.h"
#include "Mocks/ExecContextFactory.h"
#include "Utils/ExecContext.h"

#include <memory>
#include <span>
#include <string>

using namespace arangodb;
using namespace arangodb::tests::mocks;

namespace arangodb::tests {

// --- Construction ---

TEST(ExecContextTest, basic_construction) {
  auto cec = makeClassicExecContext("testuser", "testdb", auth::Level::RW,
                                    auth::Level::RW);
  auto& ctx = *cec.execContext;

  EXPECT_EQ(ctx.user(), "testuser");
  EXPECT_TRUE(ctx.canUseDatabase("_system", DatabaseAccessLevel::Write).ok());
  EXPECT_TRUE(ctx.canUseDatabase("testdb", DatabaseAccessLevel::Write).ok());
  EXPECT_TRUE(ctx.canUseAdminAction(arangodb::auth::perms::AdminBackup{}).ok());
  EXPECT_FALSE(ctx.isSuperuserOrDisabled());
}

// --- isSuperuser() predicate ---

TEST(ExecContextTest, superuser_requires_superuser_authmode) {
  // In the new API, isSuperuserOrDisabled() is true only for
  // AuthMode::Superuser (or AuthMode::Disabled). The old Type::Internal+RW/RW
  // maps to Superuser here.
  auto ctx = createSharedExecContext(AuthMode{AuthMode::Superuser{}}, false,
                                     VocbasePtr{nullptr});

  EXPECT_TRUE(ctx->isSuperuserOrDisabled());
  EXPECT_TRUE(ctx->isSuperuser());
}

TEST(ExecContextTest, disabled_is_not_superuser_authmode) {
  // In the new API, isSuperuserOrDisabled() is true only for
  // AuthMode::Superuser (or AuthMode::Disabled). The old Type::Internal+RW/RW
  // maps to Superuser here.
  auto ctx = createSharedExecContext(AuthMode{AuthMode::Disabled{"dummy"}},
                                     false, VocbasePtr{nullptr});

  EXPECT_TRUE(ctx->isSuperuserOrDisabled());
  EXPECT_FALSE(ctx->isSuperuser());
}

TEST(ExecContextTest, classic_rw_rw_is_not_superuser) {
  // "Normal" classic ExecContexts are not superuser or disabled:
  auto cec = makeClassicExecContext("", "db", auth::Level::RW, auth::Level::RW);

  EXPECT_FALSE(cec.execContext->isSuperuserOrDisabled());
  EXPECT_FALSE(cec.execContext->isSuperuser());
}

// --- canUseDatabase ---

TEST(ExecContextTest, canUseDatabase_superuser_grants_all_databases) {
  // AuthMode::Superuser grants access to any database at any level (the old
  // Type::Internal+WriteMeta/WriteMeta behaviour maps to this).
  auto ctx = createSharedExecContext(AuthMode{AuthMode::Superuser{}}, false,
                                     VocbasePtr{nullptr});

  EXPECT_TRUE(ctx->canUseDatabase("anydb", DatabaseAccessLevel::Write).ok());
  EXPECT_TRUE(ctx->canUseDatabase("anotherdb", DatabaseAccessLevel::Read).ok());
}

TEST(ExecContextTest, canUseDatabase_classic_ro_rejects_write) {
  // Classic context where the mock returns RO for "anydb": read is allowed,
  // write is not (the old Type::Internal+RO/RO restriction maps here).
  auto cec =
      makeClassicExecContext("", "anydb", auth::Level::NONE, auth::Level::RO);

  EXPECT_TRUE(
      cec.execContext->canUseDatabase("anydb", DatabaseAccessLevel::Read).ok());
  EXPECT_FALSE(
      cec.execContext->canUseDatabase("anydb", DatabaseAccessLevel::Write)
          .ok());
}

TEST(ExecContextTest, canUseDatabase_same_db_uses_dbAuthLevel) {
  // Classic context: "mydb" has RO access; _system has RW.
  auto cec =
      makeClassicExecContext("user", "mydb", auth::Level::RW, auth::Level::RO);

  EXPECT_TRUE(
      cec.execContext->canUseDatabase("mydb", DatabaseAccessLevel::Read).ok());
  EXPECT_FALSE(
      cec.execContext->canUseDatabase("mydb", DatabaseAccessLevel::Write).ok());
}

// --- canUseApiVersion ---

TEST(ExecContextTest, canUseApiVersion_classic_grants_every_version) {
  // Classic auth knows no per-identity API version restrictions: which
  // versions exist is decided by the handler factory, not by permissions. So
  // even an identity without any access grants may use any API version.
  auto cec = makeClassicExecContext("user", "mydb", auth::Level::NONE,
                                    auth::Level::NONE);

  EXPECT_TRUE(cec.execContext->canUseApiVersion(0).ok());
  EXPECT_TRUE(cec.execContext->canUseApiVersion(1).ok());
}

TEST(ExecContextTest, canUseApiVersion_superuser_grants_every_version) {
  auto ctx = createSharedExecContext(AuthMode{AuthMode::Superuser{}}, false,
                                     VocbasePtr{nullptr});

  EXPECT_TRUE(ctx->canUseApiVersion(1).ok());
}

TEST(ExecContextTest, canUseApiVersion_unauthenticated_is_denied) {
  auto ctx = createSharedExecContext(
      AuthMode{AuthMode::Unauthenticated{"dummy"}}, false, VocbasePtr{nullptr});

  auto const result = ctx->canUseApiVersion(1);
  EXPECT_EQ(result.errorNumber(), TRI_ERROR_FORBIDDEN);
  EXPECT_TRUE(result.errorMessage().find("API version '1'") !=
              std::string_view::npos)
      << result.errorMessage();
}

// --- Static superuser singleton ---

TEST(ExecContextTest, superuser_singleton) {
  auto const& su = ExecContext::superuser();

  EXPECT_TRUE(su.isSuperuserOrDisabled());
  EXPECT_TRUE(su.isSuperuser());
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

  EXPECT_TRUE(ExecContext::current().isSuperuserOrDisabled());
  EXPECT_TRUE(ExecContext::current().isSuperuser());
  EXPECT_EQ(ExecContext::currentAsShared(), nullptr);

  ExecContext::set(old);
}

TEST(ExecContextTest, set_swaps_and_returns_old_value) {
  auto old = ExecContext::set(nullptr);

  auto cec =
      makeClassicExecContext("u", "db", auth::Level::RO, auth::Level::RO);
  auto prev = ExecContext::set(cec.execContext);
  EXPECT_EQ(prev, nullptr);
  EXPECT_EQ(ExecContext::currentAsShared(), cec.execContext);
  EXPECT_EQ(ExecContext::current().user(), "u");

  auto prev2 = ExecContext::set(old);
  EXPECT_EQ(prev2, cec.execContext);
}

// --- ExecContextScope RAII ---

TEST(ExecContextTest, scope_sets_and_restores_current) {
  auto original = ExecContext::currentAsShared();

  auto cec =
      makeClassicExecContext("scoped", "db", auth::Level::RW, auth::Level::RW);
  {
    ExecContextScope scope(cec.execContext);
    EXPECT_EQ(ExecContext::current().user(), "scoped");
    EXPECT_EQ(ExecContext::currentAsShared(), cec.execContext);
  }

  EXPECT_EQ(ExecContext::currentAsShared(), original);
}

TEST(ExecContextTest, nested_scopes_restore_correctly) {
  auto original = ExecContext::currentAsShared();

  auto cec1 =
      makeClassicExecContext("outer", "db", auth::Level::RW, auth::Level::RW);
  auto cec2 =
      makeClassicExecContext("inner", "db", auth::Level::RO, auth::Level::RO);
  {
    ExecContextScope outer(cec1.execContext);
    EXPECT_EQ(ExecContext::current().user(), "outer");
    {
      ExecContextScope inner(cec2.execContext);
      EXPECT_EQ(ExecContext::current().user(), "inner");
    }
    EXPECT_EQ(ExecContext::current().user(), "outer");
  }

  EXPECT_EQ(ExecContext::currentAsShared(), original);
}

// --- ExecContextSuperuserScope RAII ---

TEST(ExecContextTest, superuser_scope_sets_and_restores) {
  auto original = ExecContext::currentAsShared();

  auto cec =
      makeClassicExecContext("regular", "db", auth::Level::RO, auth::Level::RO);
  {
    ExecContextScope setup(cec.execContext);
    EXPECT_EQ(ExecContext::current().user(), "regular");
    {
      ExecContextSuperuserScope su;
      EXPECT_TRUE(ExecContext::current().isSuperuserOrDisabled());
      EXPECT_TRUE(ExecContext::current().isSuperuser());
    }
    EXPECT_EQ(ExecContext::current().user(), "regular");
  }

  EXPECT_EQ(ExecContext::currentAsShared(), original);
}

TEST(ExecContextTest, superuser_scope_false_is_noop) {
  auto original = ExecContext::currentAsShared();

  auto cec =
      makeClassicExecContext("regular", "db", auth::Level::RO, auth::Level::RO);
  {
    ExecContextScope setup(cec.execContext);
    EXPECT_EQ(ExecContext::current().user(), "regular");
    {
      ExecContextSuperuserScope noop(false);
      EXPECT_EQ(ExecContext::current().user(), "regular");
      EXPECT_FALSE(ExecContext::current().isSuperuserOrDisabled());
      EXPECT_FALSE(ExecContext::current().isSuperuser());
    }
    EXPECT_EQ(ExecContext::current().user(), "regular");
  }

  EXPECT_EQ(ExecContext::currentAsShared(), original);
}

// --- canAccessQuery ---

/**
 * RBAC service that denies every question and counts how often it was asked
 */
struct DenyingRbacService final : rbac::Service {
  int checkCalls = 0;

  auto check(rbac::JwtToken const& /*token*/,
             std::span<rbac::ActionResource const> /*queries*/)
      -> Result override {
    ++checkCalls;
    return {TRI_ERROR_FORBIDDEN, "denied by test service"};
  }
};

TEST(ExecContextTest, canAccessQuery_own_query_is_allowed) {
  auto const cec =
      makeClassicExecContext("alice", "db", auth::Level::NONE, auth::Level::RO);

  EXPECT_TRUE(cec.execContext->canAccessQuery("alice").ok());
}

TEST(ExecContextTest, canAccessQuery_foreign_query_requires_admin) {
  auto const cec =
      makeClassicExecContext("alice", "db", auth::Level::NONE, auth::Level::RO);

  auto const result = cec.execContext->canAccessQuery("bob");
  // FakeGeneralRequest is API v0, so classic admin checks report HTTP_FORBIDDEN
  EXPECT_EQ(result.errorNumber(), TRI_ERROR_HTTP_FORBIDDEN);
}

TEST(ExecContextTest, canAccessQuery_system_read_only_is_not_admin) {
  auto const cec =
      makeClassicExecContext("alice", "db", auth::Level::RO, auth::Level::RO);

  EXPECT_FALSE(cec.execContext->canAccessQuery("bob").ok());
}

TEST(ExecContextTest, canAccessQuery_admin_may_access_foreign_query) {
  // classic admin = read-write access to the _system database
  auto const cec =
      makeClassicExecContext("root", "db", auth::Level::RW, auth::Level::RO);

  EXPECT_TRUE(cec.execContext->canAccessQuery("bob").ok());
  // queries without a user (internal ones)
  EXPECT_TRUE(cec.execContext->canAccessQuery("").ok());
}

TEST(ExecContextTest, canAccessQuery_superuser_may_access_any_query) {
  auto const ctx = createSharedExecContext(AuthMode{AuthMode::Superuser{}},
                                           false, VocbasePtr{nullptr});

  EXPECT_TRUE(ctx->canAccessQuery("bob").ok());
  EXPECT_TRUE(ctx->canAccessQuery("").ok());
}

TEST(ExecContextTest, canAccessQuery_disabled_auth_may_access_any_query) {
  auto const ctx = createSharedExecContext(
      AuthMode{AuthMode::Disabled{"dummy"}}, false, VocbasePtr{nullptr});

  EXPECT_TRUE(ctx->canAccessQuery("bob").ok());
}

TEST(ExecContextTest,
     canAccessQuery_unauthenticated_is_denied_even_for_own_name) {
  auto const ctx = createSharedExecContext(
      AuthMode{AuthMode::Unauthenticated{"dummy"}}, false, VocbasePtr{nullptr});

  EXPECT_FALSE(ctx->canAccessQuery("dummy").ok());
}

TEST(ExecContextTest, canAccessQuery_rbac_is_not_scoped_yet) {
  // RBAC mode is deliberately left unscoped (follow-up of COR-1023): every
  // query is accessible and the external service is not even asked.
  DenyingRbacService service;
  auto const ctx = createSharedExecContext(
      AuthMode{AuthMode::Rbac{service, "alice", "token", 0}}, false,
      VocbasePtr{nullptr});

  EXPECT_TRUE(ctx->canAccessQuery("bob").ok());
  EXPECT_EQ(service.checkCalls, 0);
}

}  // namespace arangodb::tests
