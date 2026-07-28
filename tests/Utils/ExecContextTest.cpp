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
#include "Mocks/ExecContextFactory.h"
#include "Utils/ExecContext.h"

#include <memory>
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
  auto ctx = ExecContextAccessor::make(AuthMode{AuthMode::Superuser{}}, false,
                                       VocbasePtr{nullptr});

  EXPECT_TRUE(ctx->isSuperuserOrDisabled());
  EXPECT_TRUE(ctx->isSuperuser());
}

TEST(ExecContextTest, disabled_is_not_superuser_authmode) {
  // In the new API, isSuperuserOrDisabled() is true only for
  // AuthMode::Superuser (or AuthMode::Disabled). The old Type::Internal+RW/RW
  // maps to Superuser here.
  FakeGeneralRequest fakeRequest;
  auto ctx = ExecContextAccessor::make(
      AuthMode{AuthMode::Disabled{"dummy", fakeRequest}}, false,
      VocbasePtr{nullptr});

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
  auto ctx = ExecContextAccessor::make(AuthMode{AuthMode::Superuser{}}, false,
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

}  // namespace arangodb::tests
