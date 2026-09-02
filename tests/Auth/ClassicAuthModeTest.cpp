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
////////////////////////////////////////////////////////////////////////////////

// Unit tests for AuthMode::Classic::check(). The counterpart to
// RbacAuthModeTest.cpp: where the RBAC tests assert which (Action, Resource)
// questions are asked of an external service, Classic answers every question
// from the access levels stored in `_users`, so these tests instead program a
// set of grants and assert the resulting Result.
//
// The grants are installed into a real auth::UserManagerTester, i.e. the
// resolution rules of auth::User (wildcards, the _system fallback, the fixed
// system-collection rules) are exercised for real. Three consequences worth
// knowing when reading or extending these tests:
//
//   * A collection has no access level of its own unless one was granted
//     explicitly: User::collectionAuthLevel() maxes in the *containing
//     database's* level, so `grantDatabase(kDb, RW)` alone yields RW on every
//     collection name in kDb -- including names that appear nowhere in the
//     grants. Tests whose intent involves a collection being readable or
//     writable therefore spell the collection grant out even where the
//     database grant would already cover it; see
//     CollectionLevelIsInheritedFromTheDatabaseLevel, which pins the rule.
//     An explicit grant short-circuits the fallback and is the only way to get
//     a collection *below* its database's level.
//   * RW on _system leaks into every *collection*, because
//     User::collectionAuthLevel() unconditionally maxes in the _system
//     database level as a fallback. A specific grant on the target database
//     shadows it at the *database* level only. That is why "admin" cases below
//     pin the target database to NONE: it is the only way to tell the
//     isAdmin() short-circuits apart from ordinary access.
//   * Classic asks the UserManager with `configured = true`, so the
//     read-only-server downgrade never applies here.
//
// Unless stated otherwise, tests run with requestedApiVersion() == 0 (V0).
// Several rules deliberately return a different error code under V0 than under
// later versions -- both are asserted where that is the case.

#include "ExecContextFactory.h"

#include "gtest/gtest.h"

#include <algorithm>
#include <array>
#include <format>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include "Auth/AuthMode.h"
#include "Auth/Permissions.h"
#include "Auth/User.h"
#include "Basics/StaticStrings.h"
#include "Basics/voc-errors.h"
#include "Mocks/Auth/UserManagerTester.h"
#include "Rest/GeneralRequest.h"

using namespace arangodb;
namespace p = arangodb::auth::perms;

namespace {

constexpr auto RO = auth::Level::RO;
constexpr auto RW = auth::Level::RW;
constexpr auto NONE = auth::Level::NONE;

// A database-level grant, i.e. `grantDatabase(db, level)`.
struct DbGrant {
  std::string db;
  auth::Level level;
};

// A collection-level grant, i.e. `grantCollection(db, coll, level)`.
struct CollGrant {
  std::string db;
  std::string coll;
  auth::Level level;
};

struct ClassicAuthModeTest : ::testing::Test {
  static constexpr std::string_view kUser = "myuser";
  static constexpr std::string_view kDb = "mydb";
  // The collection the grant-space sweep at the bottom of this file asks
  // about. Deliberately not underscore-prefixed: the fixed system-collection
  // rules would otherwise ignore the grants entirely.
  static constexpr std::string_view kColl = "c";

  auth::UserManagerTester um;
  tests::mocks::FakeGeneralRequest req;
  AuthMode::Classic classic{um, std::string{kUser}, req};

  // Replace kUser's grants. Database grants are applied first: a
  // grantCollection() for a database without a grant of its own creates an
  // entry whose database level is UNDEFINED, which changes how the wildcard /
  // _system fallbacks resolve.
  void setGrants(std::vector<DbGrant> const& dbs,
                 std::vector<CollGrant> const& colls = {}) {
    auth::UserMap userMap;
    auto& user =
        userMap.emplace(kUser, auth::User::newUser(std::string{kUser}, ""))
            .first->second;
    user.setActive(true);
    for (auto const& g : dbs) {
      user.grantDatabase(g.db, g.level);
    }
    for (auto const& g : colls) {
      user.grantCollection(g.db, g.coll, g.level);
    }
    um.setAuthInfo(userMap);
  }

  // The two recurring shapes: an ordinary user with `level` on kDb and no
  // access to _system, and an "admin" (RW on _system) whose access to kDb is
  // explicitly revoked.
  void beUserWith(auth::Level level, std::vector<CollGrant> const& colls = {}) {
    setGrants(
        {{StaticStrings::SystemDatabase, NONE}, {std::string{kDb}, level}},
        colls);
  }
  void beAdmin() { setGrants({{StaticStrings::SystemDatabase, RW}}); }
  void beAdminWithoutDb() {
    setGrants({{StaticStrings::SystemDatabase, RW}, {std::string{kDb}, NONE}});
  }

  // Discarding-free wrapper around classic.check(); IAuth::check is
  // [[nodiscard]].
  Result check(auth::Permission perm) { return classic.check(std::move(perm)); }

  void useApiVersion(uint32_t version) {
    classic.setRequestedApiVersion(version);
  }

  static void expectError(Result const& r, ErrorCode expected) {
    EXPECT_EQ(r.errorNumber(), expected) << r.errorMessage();
  }
};

// ---------------------------------------------------------------------------
// Accessors
// ---------------------------------------------------------------------------

TEST_F(ClassicAuthModeTest, UsernameIsExposed) {
  EXPECT_EQ(classic.username(), kUser);
}

// ---------------------------------------------------------------------------
// Databases
// ---------------------------------------------------------------------------

TEST_F(ClassicAuthModeTest, UseDatabaseReadWithReadOnlyAccess) {
  beUserWith(RO);
  EXPECT_TRUE(check(p::UseDatabase{.name = std::string{kDb},
                                   .level = DatabaseAccessLevel::Read})
                  .ok());
}

TEST_F(ClassicAuthModeTest, UseDatabaseWriteWithReadOnlyAccessIsForbidden) {
  beUserWith(RO);
  expectError(check(p::UseDatabase{.name = std::string{kDb},
                                   .level = DatabaseAccessLevel::Write}),
              TRI_ERROR_FORBIDDEN);
}

TEST_F(ClassicAuthModeTest, UseDatabaseWriteWithReadWriteAccess) {
  beUserWith(RW);
  EXPECT_TRUE(check(p::UseDatabase{.name = std::string{kDb},
                                   .level = DatabaseAccessLevel::Write})
                  .ok());
}

TEST_F(ClassicAuthModeTest, UseDatabaseWithoutAccessIsNotFound) {
  // With a versioned API the database's existence must not be revealed.
  beUserWith(NONE);
  expectError(check(p::UseDatabase{.name = std::string{kDb},
                                   .level = DatabaseAccessLevel::Read}),
              TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
}

TEST_F(ClassicAuthModeTest, UseDatabaseWriteWithReadOnlyAccessStaysForbidden) {
  // The "hide existence" branch only applies when there is no access at all;
  // insufficient access remains FORBIDDEN even under a versioned API.
  beUserWith(RO);
  expectError(check(p::UseDatabase{.name = std::string{kDb},
                                   .level = DatabaseAccessLevel::Write}),
              TRI_ERROR_FORBIDDEN);
}

TEST_F(ClassicAuthModeTest, SeeDatabaseNeedsReadAccess) {
  beUserWith(RO);
  EXPECT_TRUE(check(p::SeeDatabase{.name = std::string{kDb}}).ok());
  beUserWith(NONE);
  expectError(check(p::SeeDatabase{.name = std::string{kDb}}),
              TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
}

TEST_F(ClassicAuthModeTest, CreateDatabaseNeedsSystemReadWrite) {
  // The name of the database to be created is irrelevant; only RW on _system
  // counts.
  beAdmin();
  EXPECT_TRUE(check(p::CreateDatabase{.name = "brandnew"}).ok());
}

TEST_F(ClassicAuthModeTest, CreateDatabaseWithoutSystemReadWriteIsForbidden) {
  setGrants({{StaticStrings::SystemDatabase, RO}});
  expectError(check(p::CreateDatabase{.name = "brandnew"}),
              TRI_ERROR_FORBIDDEN);
}

TEST_F(ClassicAuthModeTest, DropDatabaseNeedsSystemReadWrite) {
  beAdmin();
  EXPECT_TRUE(check(p::DropDatabase{.name = std::string{kDb}}).ok());
  setGrants({{StaticStrings::SystemDatabase, RO}});
  expectError(check(p::DropDatabase{.name = std::string{kDb}}),
              TRI_ERROR_FORBIDDEN);
}

// ---------------------------------------------------------------------------
// Collections
// ---------------------------------------------------------------------------

TEST_F(ClassicAuthModeTest, UseCollectionRead) {
  beUserWith(RO);
  EXPECT_TRUE(check(p::UseCollection{.db = std::string{kDb},
                                     .name = "c",
                                     .level = CollectionAccessLevel::Read})
                  .ok());
}

TEST_F(ClassicAuthModeTest, CollectionLevelIsInheritedFromTheDatabaseLevel) {
  // The rule the rest of this file leans on: User::collectionAuthLevel() maxes
  // in the containing database's own level for every non-system collection, so
  // a database grant alone covers *every* collection name -- the name need not
  // appear in the grants at all.
  beUserWith(RW);
  EXPECT_TRUE(check(p::UseCollection{.db = std::string{kDb},
                                     .name = "never-granted-anywhere",
                                     .level = CollectionAccessLevel::WriteData})
                  .ok());

  // An explicit collection grant short-circuits that fallback, and is the only
  // way to put a collection *below* its database's level.
  beUserWith(RW, {{std::string{kDb}, "c", RO}});
  expectError(
      check(p::UseCollection{.db = std::string{kDb},
                             .name = "c",
                             .level = CollectionAccessLevel::WriteData}),
      TRI_ERROR_ARANGO_READ_ONLY);
  beUserWith(RW, {{std::string{kDb}, "c", NONE}});
  expectError(check(p::UseCollection{.db = std::string{kDb},
                                     .name = "c",
                                     .level = CollectionAccessLevel::Read}),
              TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);

  // A wildcard collection grant raises every collection in that database above
  // the database's own level, again for arbitrary names.
  beUserWith(RO, {{std::string{kDb}, "*", RW}});
  EXPECT_TRUE(check(p::UseCollection{.db = std::string{kDb},
                                     .name = "anything",
                                     .level = CollectionAccessLevel::WriteData})
                  .ok());
}

TEST_F(ClassicAuthModeTest, CollectionGrantLeavesTheDatabaseLevelUndefined) {
  // A collection grant is not inert with respect to the *database* level:
  // grantCollection() for a database that has no grant of its own creates an
  // entry whose database level is UNDEFINED, and databaseAuthLevel() applies
  // the wildcard / _system fallbacks precisely when it resolves to UNDEFINED.
  // So here kDb still picks up RO from the wildcard database grant.
  setGrants({{"*", RO}}, {{std::string{kDb}, "c", RW}});
  EXPECT_TRUE(check(p::UseDatabase{.name = std::string{kDb},
                                   .level = DatabaseAccessLevel::Read})
                  .ok());

  // An explicit grant of NONE is *not* UNDEFINED, so it blocks the fallback.
  // This is why the beUserWith() helpers always pin kDb and _system: it is the
  // only way to keep an added collection grant from moving the database level.
  setGrants({{"*", RO}, {std::string{kDb}, NONE}},
            {{std::string{kDb}, "c", RW}});
  expectError(check(p::UseDatabase{.name = std::string{kDb},
                                   .level = DatabaseAccessLevel::Read}),
              TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
}

TEST_F(ClassicAuthModeTest, UseCollectionWriteDataOnReadOnlyIsReadOnlyError) {
  beUserWith(RO);
  expectError(
      check(p::UseCollection{.db = std::string{kDb},
                             .name = "c",
                             .level = CollectionAccessLevel::WriteData}),
      TRI_ERROR_ARANGO_READ_ONLY);
}

TEST_F(ClassicAuthModeTest, UseCollectionWriteDataWithReadWriteAccess) {
  beUserWith(RW);
  EXPECT_TRUE(check(p::UseCollection{.db = std::string{kDb},
                                     .name = "c",
                                     .level = CollectionAccessLevel::WriteData})
                  .ok());
}

TEST_F(ClassicAuthModeTest, UseCollectionWithoutAccessIsNotFound) {
  beUserWith(NONE);
  expectError(check(p::UseCollection{.db = std::string{kDb},
                                     .name = "c",
                                     .level = CollectionAccessLevel::Read}),
              TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
}

TEST_F(ClassicAuthModeTest, UseCollectionWriteDataOnReadOnlyStaysReadOnly) {
  // Partial access is not hidden: the collection is known to exist.
  beUserWith(RO);
  expectError(
      check(p::UseCollection{.db = std::string{kDb},
                             .name = "c",
                             .level = CollectionAccessLevel::WriteData}),
      TRI_ERROR_ARANGO_READ_ONLY);
}

TEST_F(ClassicAuthModeTest, UseCollectionWriteMetaAlsoNeedsDatabaseReadWrite) {
  // Container principle: changing a collection's meta-data requires write
  // access to the containing database, even when the collection itself is RW.
  beUserWith(RO, {{std::string{kDb}, "c", RW}});
  expectError(
      check(p::UseCollection{.db = std::string{kDb},
                             .name = "c",
                             .level = CollectionAccessLevel::WriteMeta}),
      TRI_ERROR_FORBIDDEN);
}

TEST_F(ClassicAuthModeTest, UseCollectionWriteMetaWithDatabaseReadWrite) {
  beUserWith(RW, {{std::string{kDb}, "c", RW}});
  EXPECT_TRUE(check(p::UseCollection{.db = std::string{kDb},
                                     .name = "c",
                                     .level = CollectionAccessLevel::WriteMeta})
                  .ok());
}

TEST_F(ClassicAuthModeTest,
       UseCollectionWriteMetaWithReadOnlyCollectionIsReadOnly) {
  // The other half of the container principle: write access to the database
  // does not compensate for a collection that was explicitly pinned to RO.
  beUserWith(RW, {{std::string{kDb}, "c", RO}});
  expectError(
      check(p::UseCollection{.db = std::string{kDb},
                             .name = "c",
                             .level = CollectionAccessLevel::WriteMeta}),
      TRI_ERROR_ARANGO_READ_ONLY);
}

TEST_F(ClassicAuthModeTest, SystemUsersCollectionIsForbiddenEvenForAdmins) {
  // _system/_users may never be touched through the normal APIs, no matter
  // what the grants say.
  beAdmin();
  for (auto level :
       {CollectionAccessLevel::Read, CollectionAccessLevel::WriteData,
        CollectionAccessLevel::WriteMeta}) {
    expectError(check(p::UseCollection{.db = StaticStrings::SystemDatabase,
                                       .name = StaticStrings::UsersCollection,
                                       .level = level}),
                TRI_ERROR_FORBIDDEN);
  }
}

TEST_F(ClassicAuthModeTest, OtherSystemCollectionsFollowTheDatabaseLevel) {
  // A system collection without a fixed rule (here _graphs) inherits the
  // database's access level.
  beUserWith(RO);
  EXPECT_TRUE(check(p::UseCollection{.db = std::string{kDb},
                                     .name = StaticStrings::GraphsCollection,
                                     .level = CollectionAccessLevel::Read})
                  .ok());
  beUserWith(NONE);
  expectError(check(p::UseCollection{.db = std::string{kDb},
                                     .name = StaticStrings::GraphsCollection,
                                     .level = CollectionAccessLevel::Read}),
              TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
}

TEST_F(ClassicAuthModeTest, SeeCollectionNeedsReadAccess) {
  beUserWith(RO);
  EXPECT_TRUE(
      check(p::SeeCollection{.db = std::string{kDb}, .name = "c"}).ok());
  beUserWith(NONE);
  expectError(check(p::SeeCollection{.db = std::string{kDb}, .name = "c"}),
              TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
}

TEST_F(ClassicAuthModeTest, SeeCollectionIsGrantedToAdmins) {
  // An admin must be able to run arangodump, so it may see every collection
  // even without access to the containing database.
  beAdminWithoutDb();
  EXPECT_TRUE(
      check(p::SeeCollection{.db = std::string{kDb}, .name = "c"}).ok());
}

TEST_F(ClassicAuthModeTest, CreateCollectionNeedsDatabaseReadWrite) {
  beUserWith(RW);
  EXPECT_TRUE(
      check(p::CreateCollection{.db = std::string{kDb}, .name = "c"}).ok());
  beUserWith(RO);
  expectError(check(p::CreateCollection{.db = std::string{kDb}, .name = "c"}),
              TRI_ERROR_FORBIDDEN);
}

TEST_F(ClassicAuthModeTest, DropCollectionNeedsDatabaseAndCollectionAccess) {
  beUserWith(RW, {{std::string{kDb}, "c", RW}});
  EXPECT_TRUE(
      check(p::DropCollection{.db = std::string{kDb}, .name = "c"}).ok());
}

TEST_F(ClassicAuthModeTest, DropCollectionWithoutDatabaseWriteIsForbidden) {
  beUserWith(RO);
  expectError(check(p::DropCollection{.db = std::string{kDb}, .name = "c"}),
              TRI_ERROR_FORBIDDEN);
}

TEST_F(ClassicAuthModeTest, DropCollectionReadOnlyCollectionIsReadOnly) {
  // The collection-level failure is TRI_ERROR_ARANGO_READ_ONLY, but V0 must
  // keep reporting FORBIDDEN for API compatibility.
  beUserWith(RW, {{std::string{kDb}, "c", RO}});
  expectError(check(p::DropCollection{.db = std::string{kDb}, .name = "c"}),
              TRI_ERROR_ARANGO_READ_ONLY);
}

TEST_F(ClassicAuthModeTest, DropCollectionReadOnlyCollectionIsReadOnly) {
  beUserWith(RW, {{std::string{kDb}, "c", RO}});
  expectError(check(p::DropCollection{.db = std::string{kDb}, .name = "c"}),
              TRI_ERROR_ARANGO_READ_ONLY);
}

// ---------------------------------------------------------------------------
// Views
// ---------------------------------------------------------------------------

TEST_F(ClassicAuthModeTest, ReadViewRequiresSystemReadAccess) {
  beUserWith(RO);
  EXPECT_TRUE(check(p::ReadView{.db = std::string{kDb}, .name = "v"}).ok());
  beUserWith(RW);
  EXPECT_TRUE(check(p::ReadView{.db = std::string{kDb}, .name = "v"}).ok());
}

TEST_F(ClassicAuthModeTest, ReadViewWithoutAccessIsNotFound) {
  beUserWith(NONE);
  expectError(check(p::ReadView{.db = std::string{kDb}, .name = "v"}),
              TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
}

TEST_F(ClassicAuthModeTest, SeeViewIsAlwaysGranted) {
  // Read access to the database is the only prerequisite and has already been
  // checked by the time this question is asked.
  setGrants({});
  EXPECT_TRUE(check(p::SeeView{.db = std::string{kDb}, .name = "v"}).ok());
}

TEST_F(ClassicAuthModeTest, CreateViewNeedsDatabaseWriteAndReadableLinks) {
  // The link grants are spelled out although the database grant would already
  // imply them -- otherwise this would pass for any collection name at all.
  beUserWith(RW, {{std::string{kDb}, "c1", RO}, {std::string{kDb}, "c2", RO}});
  EXPECT_TRUE(check(p::CreateView{.db = std::string{kDb},
                                  .name = "v",
                                  .linkedCollections = {"c1", "c2"}})
                  .ok());
}

TEST_F(ClassicAuthModeTest, CreateViewWithoutDatabaseWriteIsForbidden) {
  // The degenerate case: with no links at all, the database check is the only
  // one left. See CreateViewWithReadableLinksButNoDatabaseWriteIsForbidden for
  // the case that actually isolates the missing database grant.
  beUserWith(RO);
  expectError(
      check(p::CreateView{
          .db = std::string{kDb}, .name = "v", .linkedCollections = {}}),
      TRI_ERROR_FORBIDDEN);
}

TEST_F(ClassicAuthModeTest,
       CreateViewWithReadableLinksButNoDatabaseWriteIsForbidden) {
  // The database grant is checked first and is not substitutable: even RW on
  // every linked collection does not make up for a read-only database.
  beUserWith(RO, {{std::string{kDb}, "c1", RW}});
  expectError(
      check(p::CreateView{
          .db = std::string{kDb}, .name = "v", .linkedCollections = {"c1"}}),
      TRI_ERROR_FORBIDDEN);
}

TEST_F(ClassicAuthModeTest, CreateViewWithUnreadableLinkIsForbidden) {
  beUserWith(RW, {{std::string{kDb}, "secret", NONE}});
  auto r = check(p::CreateView{
      .db = std::string{kDb}, .name = "v", .linkedCollections = {"secret"}});
  expectError(r, TRI_ERROR_FORBIDDEN);
  EXPECT_NE(r.errorMessage().find("Insufficient access to linked collection"),
            std::string::npos)
      << r.errorMessage();
}

TEST_F(ClassicAuthModeTest, CreateViewWrapsLinkFailuresAsForbidden) {
  // Unlike ModifyView below, CreateView normalises every linked-collection
  // failure to FORBIDDEN -- the underlying NOT_FOUND is not passed through.
  beUserWith(RW, {{std::string{kDb}, "secret", NONE}});
  expectError(check(p::CreateView{.db = std::string{kDb},
                                  .name = "v",
                                  .linkedCollections = {"secret"}}),
              TRI_ERROR_FORBIDDEN);
}

TEST_F(ClassicAuthModeTest, ModifyViewNeedsDatabaseWriteAndReadableLinks) {
  beUserWith(RW, {{std::string{kDb}, "c1", RO}});
  EXPECT_TRUE(check(p::ModifyView{.db = std::string{kDb},
                                  .name = "v",
                                  .linkedCollections = {"c1"}})
                  .ok());
  beUserWith(RO);
  expectError(
      check(p::ModifyView{
          .db = std::string{kDb}, .name = "v", .linkedCollections = {}}),
      TRI_ERROR_FORBIDDEN);
}

TEST_F(ClassicAuthModeTest,
       ModifyViewWithReadableLinksButNoDatabaseWriteIsForbidden) {
  beUserWith(RO, {{std::string{kDb}, "c1", RW}});
  expectError(
      check(p::ModifyView{
          .db = std::string{kDb}, .name = "v", .linkedCollections = {"c1"}}),
      TRI_ERROR_FORBIDDEN);
}

TEST_F(ClassicAuthModeTest, ModifyViewPropagatesLinkFailuresUnverbatim) {
  beUserWith(RW, {{std::string{kDb}, "secret", NONE}});
  expectError(check(p::ModifyView{.db = std::string{kDb},
                                  .name = "v",
                                  .linkedCollections = {"secret"}}),
              TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
}

TEST_F(ClassicAuthModeTest, RenameViewToSameNameIsBadParameter) {
  beAdmin();
  expectError(check(p::RenameView{
                  .db = std::string{kDb}, .oldName = "v", .newName = "v"}),
              TRI_ERROR_BAD_PARAMETER);
}

TEST_F(ClassicAuthModeTest, RenameViewNeedsDatabaseWrite) {
  beUserWith(RW);
  EXPECT_TRUE(
      check(p::RenameView{
                .db = std::string{kDb}, .oldName = "old", .newName = "new"})
          .ok());
  beUserWith(RO);
  expectError(check(p::RenameView{
                  .db = std::string{kDb}, .oldName = "old", .newName = "new"}),
              TRI_ERROR_FORBIDDEN);
}

TEST_F(ClassicAuthModeTest, DropViewNeedsDatabaseWrite) {
  beUserWith(RW);
  EXPECT_TRUE(check(p::DropView{.db = std::string{kDb}, .name = "v"}).ok());
  beUserWith(RO);
  expectError(check(p::DropView{.db = std::string{kDb}, .name = "v"}),
              TRI_ERROR_FORBIDDEN);
}

TEST_F(ClassicAuthModeTest, DropViewNeedsDatabaseWriteAndReadableLinks) {
  // The link grants are spelled out although the database grant would already
  // imply them -- otherwise this would pass for any collection name at all.
  beUserWith(RW, {{std::string{kDb}, "c1", RO}, {std::string{kDb}, "c2", RO}});
  EXPECT_TRUE(check(p::DropView{.db = std::string{kDb},
                                .name = "v",
                                .linkedCollections = {"c1", "c2"}})
                  .ok());
}

TEST_F(ClassicAuthModeTest,
       DropViewWithReadableLinksButNoDatabaseWriteIsForbidden) {
  // The database grant is checked first and is not substitutable: even RW on
  // every linked collection does not make up for a read-only database.
  beUserWith(RO, {{std::string{kDb}, "c1", RW}});
  expectError(
      check(p::DropView{
          .db = std::string{kDb}, .name = "v", .linkedCollections = {"c1"}}),
      TRI_ERROR_FORBIDDEN);
}

TEST_F(ClassicAuthModeTest, DropViewWithUnreadableLinkIsForbidden) {
  beUserWith(RW, {{std::string{kDb}, "secret", NONE}});
  auto r = check(p::DropView{
      .db = std::string{kDb}, .name = "v", .linkedCollections = {"secret"}});
  expectError(r, TRI_ERROR_FORBIDDEN);
  EXPECT_NE(r.errorMessage().find("Insufficient access to linked collection"),
            std::string::npos)
      << r.errorMessage();
}

// ---------------------------------------------------------------------------
// Analyzers
//
// Analyzers follow the database level, with one backwards-compatibility
// exception: RW on _system grants every analyzer permission.
// ---------------------------------------------------------------------------

TEST_F(ClassicAuthModeTest, SeeAnalyzerNeedsDatabaseRead) {
  beUserWith(RO);
  EXPECT_TRUE(check(p::SeeAnalyzer{.db = std::string{kDb}, .name = "a"}).ok());
  beUserWith(NONE);
  expectError(check(p::SeeAnalyzer{.db = std::string{kDb}, .name = "a"}),
              TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
}

TEST_F(ClassicAuthModeTest, SeeAnalyzerIsGrantedToAdmins) {
  beAdminWithoutDb();
  EXPECT_TRUE(check(p::SeeAnalyzer{.db = std::string{kDb}, .name = "a"}).ok());
}

TEST_F(ClassicAuthModeTest, CreateAnalyzerNeedsDatabaseWrite) {
  beUserWith(RW);
  EXPECT_TRUE(
      check(p::CreateAnalyzer{.db = std::string{kDb}, .name = "a"}).ok());
  beUserWith(RO);
  expectError(check(p::CreateAnalyzer{.db = std::string{kDb}, .name = "a"}),
              TRI_ERROR_FORBIDDEN);
}

TEST_F(ClassicAuthModeTest, CreateAnalyzerIsGrantedToAdmins) {
  beAdminWithoutDb();
  EXPECT_TRUE(
      check(p::CreateAnalyzer{.db = std::string{kDb}, .name = "a"}).ok());
}

TEST_F(ClassicAuthModeTest, DropAnalyzerNeedsDatabaseWrite) {
  beUserWith(RW);
  EXPECT_TRUE(check(p::DropAnalyzer{.db = std::string{kDb}, .name = "a"}).ok());
  beUserWith(RO);
  expectError(check(p::DropAnalyzer{.db = std::string{kDb}, .name = "a"}),
              TRI_ERROR_FORBIDDEN);
}

TEST_F(ClassicAuthModeTest, DropAnalyzerIsGrantedToAdmins) {
  beAdminWithoutDb();
  EXPECT_TRUE(check(p::DropAnalyzer{.db = std::string{kDb}, .name = "a"}).ok());
}

TEST_F(ClassicAuthModeTest, UseAnalyzerFollowsTheDatabaseLevel) {
  beUserWith(RO);
  EXPECT_TRUE(check(p::UseAnalyzer{.db = std::string{kDb},
                                   .name = "a",
                                   .level = AnalyzerAccessLevel::Read})
                  .ok());
  expectError(check(p::UseAnalyzer{.db = std::string{kDb},
                                   .name = "a",
                                   .level = AnalyzerAccessLevel::Modify}),
              TRI_ERROR_FORBIDDEN);
  beUserWith(RW);
  EXPECT_TRUE(check(p::UseAnalyzer{.db = std::string{kDb},
                                   .name = "a",
                                   .level = AnalyzerAccessLevel::Modify})
                  .ok());
}

TEST_F(ClassicAuthModeTest, UseAnalyzerModifyIsGrantedToAdmins) {
  beAdminWithoutDb();
  EXPECT_TRUE(check(p::UseAnalyzer{.db = std::string{kDb},
                                   .name = "a",
                                   .level = AnalyzerAccessLevel::Modify})
                  .ok());
}

// ---------------------------------------------------------------------------
// Graphs
// ---------------------------------------------------------------------------

TEST_F(ClassicAuthModeTest, SeeGraphNeedsDatabaseRead) {
  beUserWith(RO);
  EXPECT_TRUE(check(p::SeeGraph{.db = std::string{kDb}, .name = "g"}).ok());
  beUserWith(NONE);
  expectError(check(p::SeeGraph{.db = std::string{kDb}, .name = "g"}),
              TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
}

TEST_F(ClassicAuthModeTest, UseGraphFollowsTheDatabaseLevel) {
  beUserWith(RO);
  EXPECT_TRUE(check(p::UseGraph{.db = std::string{kDb},
                                .name = "g",
                                .level = GraphAccessLevel::Read})
                  .ok());
  expectError(check(p::UseGraph{.db = std::string{kDb},
                                .name = "g",
                                .level = GraphAccessLevel::Modify}),
              TRI_ERROR_ARANGO_READ_ONLY);
  beUserWith(RW);
  EXPECT_TRUE(check(p::UseGraph{.db = std::string{kDb},
                                .name = "g",
                                .level = GraphAccessLevel::Modify})
                  .ok());
}

TEST_F(ClassicAuthModeTest, CreateGraphWithChildCollections) {
  // "cr" gets an explicit read grant because that is what the permission
  // requires of it. "cc" deliberately gets none: CreateCollection is a pure
  // database-RW check, so a collection that does not exist yet cannot and need
  // not carry a grant.
  beUserWith(RW, {{std::string{kDb}, "cr", RO}});
  std::vector<std::string> toCreate{"cc"};
  std::vector<std::string> toRead{"cr"};
  EXPECT_TRUE(check(p::CreateGraph{.db = std::string{kDb},
                                   .name = "g",
                                   .collectionNamesToCreate = toCreate,
                                   .collectionNamesToRead = toRead})
                  .ok());
}

TEST_F(ClassicAuthModeTest, CreateGraphWithoutDatabaseWriteIsForbidden) {
  beUserWith(RO);
  std::vector<std::string> none;
  expectError(check(p::CreateGraph{.db = std::string{kDb},
                                   .name = "g",
                                   .collectionNamesToCreate = none,
                                   .collectionNamesToRead = none}),
              TRI_ERROR_FORBIDDEN);
}

TEST_F(ClassicAuthModeTest, CreateGraphChecksChildCollectionsFirst) {
  // The child collections are checked before the database, so a creatable
  // collection reports the plain FORBIDDEN of CreateCollection rather than the
  // READ_ONLY of the trailing database check.
  beUserWith(RO);
  std::vector<std::string> toCreate{"cc"};
  std::vector<std::string> none;
  expectError(check(p::CreateGraph{.db = std::string{kDb},
                                   .name = "g",
                                   .collectionNamesToCreate = toCreate,
                                   .collectionNamesToRead = none}),
              TRI_ERROR_FORBIDDEN);
}

TEST_F(ClassicAuthModeTest, CreateGraphWithUnreadableChildCollection) {
  beUserWith(RW, {{std::string{kDb}, "secret", NONE}});
  std::vector<std::string> none;
  std::vector<std::string> toRead{"secret"};
  expectError(check(p::CreateGraph{.db = std::string{kDb},
                                   .name = "g",
                                   .collectionNamesToCreate = none,
                                   .collectionNamesToRead = toRead}),
              TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
}

TEST_F(ClassicAuthModeTest, DropGraphWithListedCollections) {
  beUserWith(RW, {{std::string{kDb}, "c1", RW}, {std::string{kDb}, "c2", RW}});
  std::vector<std::string> colls{"c1", "c2"};
  EXPECT_TRUE(
      check(p::DropGraph{
                .db = std::string{kDb}, .name = "g", .collectionNames = colls})
          .ok());
}

TEST_F(ClassicAuthModeTest,
       CreateGraphWithReadableChildrenButNoDatabaseWriteIsReadOnly) {
  // The child collections are checked first and pass here, so the failure comes
  // from the trailing database check -- which reports READ_ONLY under V0.
  // RW on the child collection does not substitute for the database grant.
  beUserWith(RO, {{std::string{kDb}, "cr", RW}});
  std::vector<std::string> none;
  std::vector<std::string> toRead{"cr"};
  expectError(check(p::CreateGraph{.db = std::string{kDb},
                                   .name = "g",
                                   .collectionNamesToCreate = none,
                                   .collectionNamesToRead = toRead}),
              TRI_ERROR_FORBIDDEN);
}

TEST_F(ClassicAuthModeTest, DropGraphWithoutDatabaseWriteIsForbidden) {
  beUserWith(RO);
  std::vector<std::string> none;
  expectError(
      check(p::DropGraph{
          .db = std::string{kDb}, .name = "g", .collectionNames = none}),
      TRI_ERROR_ARANGO_READ_ONLY);
}

TEST_F(ClassicAuthModeTest,
       DropGraphWithDroppableCollectionsButNoDatabaseWriteIsForbidden) {
  // RW on every listed collection does not substitute for the database grant.
  beUserWith(RO, {{std::string{kDb}, "c1", RW}});
  std::vector<std::string> colls{"c1"};
  expectError(
      check(p::DropGraph{
          .db = std::string{kDb}, .name = "g", .collectionNames = colls}),
      TRI_ERROR_FORBIDDEN);
}

TEST_F(ClassicAuthModeTest, DropGraphWithUndroppableCollectionIsForbidden) {
  beUserWith(RW, {{std::string{kDb}, "c1", RO}});
  std::vector<std::string> colls{"c1"};
  expectError(
      check(p::DropGraph{
          .db = std::string{kDb}, .name = "g", .collectionNames = colls}),
      TRI_ERROR_ARANGO_READ_ONLY);
}

// ---------------------------------------------------------------------------
// Users
//
// Every user operation reduces to RW on _system. (The self-exception for
// ModifyUserProfile is handled by ExecContext, above this layer.)
// ---------------------------------------------------------------------------

TEST_F(ClassicAuthModeTest, UserOperationsNeedSystemReadWrite) {
  beAdmin();
  EXPECT_TRUE(check(p::ReadUser{.name = "alice"}).ok());
  EXPECT_TRUE(check(p::CreateUser{.name = "alice"}).ok());
  EXPECT_TRUE(check(p::DropUser{.name = "alice"}).ok());
  EXPECT_TRUE(check(p::ModifyUserProfile{.name = "alice"}).ok());
  EXPECT_TRUE(check(p::GrantUserPermissions{.name = "alice"}).ok());
}

TEST_F(ClassicAuthModeTest, UserOperationsAreForbiddenWithoutSystemReadWrite) {
  setGrants({{StaticStrings::SystemDatabase, RO}});
  expectError(check(p::ReadUser{.name = "alice"}), TRI_ERROR_HTTP_FORBIDDEN);
  expectError(check(p::CreateUser{.name = "alice"}), TRI_ERROR_FORBIDDEN);
  expectError(check(p::DropUser{.name = "alice"}), TRI_ERROR_FORBIDDEN);
  expectError(check(p::ModifyUserProfile{.name = "alice"}),
              TRI_ERROR_FORBIDDEN);
  expectError(check(p::GrantUserPermissions{.name = "alice"}),
              TRI_ERROR_FORBIDDEN);
}

// ---------------------------------------------------------------------------
// Admin actions
//
// All of them collapse into the same question: RW on _system. The one
// exception is AdminQueryCache, which only needs RO on _system; see
// AdminQueryCacheIsGrantedBySystemReadOnly below.
// ---------------------------------------------------------------------------

// Every alternative of auth::perms::detail::AdminList. Kept explicit so that
// adding an admin permission without extending this list is visible here
// rather than silently untested.
using AllAdminPermissions =
    std::tuple<p::AdminReadUsers, p::AdminMoveShards, p::AdminMonitoring,
               p::AdminMonitoringInternal, p::AdminAuthReload,
               p::AdminCrashHandler, p::AdminApiCalls, p::AdminAqlQueries,
               p::AdminShutdown, p::AdminReadLogs, p::AdminSetLogLevel,
               p::AdminOptions, p::AdminSupervisionState, p::AdminRemoveServer,
               p::AdminClusterInfo, p::AdminMaintenance, p::AdminRebalance,
               p::AdminLicense, p::AdminBackup, p::AdminReadReplicatedLog,
               p::AdminWriteReplicatedLog, p::AdminDump, p::AdminRestore,
               p::AdminWalAccess, p::AdminReadAgency, p::AdminQueryCache>;

static_assert(std::tuple_size_v<AllAdminPermissions> == 26);

// Same as AllAdminPermissions, minus AdminQueryCache: used by the tests below
// that pin "forbidden without RW", which no longer holds for AdminQueryCache.
using AllAdminPermissionsExceptQueryCache =
    std::tuple<p::AdminReadUsers, p::AdminMoveShards, p::AdminMonitoring,
               p::AdminMonitoringInternal, p::AdminAuthReload,
               p::AdminCrashHandler, p::AdminApiCalls, p::AdminAqlQueries,
               p::AdminShutdown, p::AdminReadLogs, p::AdminSetLogLevel,
               p::AdminOptions, p::AdminSupervisionState, p::AdminRemoveServer,
               p::AdminClusterInfo, p::AdminMaintenance, p::AdminRebalance,
               p::AdminLicense, p::AdminBackup, p::AdminReadReplicatedLog,
               p::AdminWriteReplicatedLog, p::AdminDump, p::AdminRestore,
               p::AdminWalAccess, p::AdminReadAgency>;

static_assert(std::tuple_size_v<AllAdminPermissionsExceptQueryCache> == 25);

TEST_F(ClassicAuthModeTest, EveryAdminActionIsGrantedBySystemReadWrite) {
  beAdmin();
  std::apply(
      [&](auto... admins) {
        auto expectGranted = [&](auto const& admin) {
          auto permission = auth::Permission{admin};
          EXPECT_TRUE(check(permission).ok()) << permission;
        };
        (expectGranted(admins), ...);
      },
      AllAdminPermissions{});
}

TEST_F(ClassicAuthModeTest, EveryAdminActionIsForbiddenWithoutSystemReadWrite) {
  setGrants({{StaticStrings::SystemDatabase, RO}});
  std::apply(
      [&](auto... admins) {
        auto expectDenied = [&](auto const& admin) {
          auto permission = auth::Permission{admin};
          auto r = check(permission);
          EXPECT_EQ(r.errorNumber(), TRI_ERROR_HTTP_FORBIDDEN) << permission;
          EXPECT_NE(r.errorMessage().find("Failed admin-permission check"),
                    std::string::npos)
              << r.errorMessage();
        };
        (expectDenied(admins), ...);
      },
      AllAdminPermissionsExceptQueryCache{});
}

TEST_F(ClassicAuthModeTest, AdminReadUsersIsNotSpecialInClassic) {
  // Contrast with AuthMode::Rbac, where AdminReadUsers has no counterpart yet
  // and fails closed with TRI_ERROR_NOT_IMPLEMENTED.
  beAdmin();
  EXPECT_TRUE(check(p::AdminReadUsers{}).ok());
}

TEST_F(ClassicAuthModeTest, AdminQueryCacheIsGrantedBySystemReadOnly) {
  // AdminQueryCache needs RW on _system.
  setGrants({{StaticStrings::SystemDatabase, RO}});
  EXPECT_FALSE(check(p::AdminQueryCache{}).ok());
}

TEST_F(ClassicAuthModeTest, AdminQueryCacheIsForbiddenWithoutSystemAccess) {
  setGrants({{StaticStrings::SystemDatabase, NONE}});
  expectError(check(p::AdminQueryCache{}), TRI_ERROR_HTTP_FORBIDDEN);
}

// ---------------------------------------------------------------------------
// Dump / Restore
//
// These delegate to the collection/view permissions, but are additionally
// granted to admins (mirroring canUseAdminAction(AdminDump/AdminRestore)).
// ---------------------------------------------------------------------------

TEST_F(ClassicAuthModeTest, DumpCollectionBehavesLikeReadAccess) {
  beUserWith(RO);
  EXPECT_TRUE(
      check(p::DumpCollection{.db = std::string{kDb}, .name = "c"}).ok());
  beUserWith(NONE);
  expectError(check(p::DumpCollection{.db = std::string{kDb}, .name = "c"}),
              TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
}

TEST_F(ClassicAuthModeTest, DumpCollectionIsGrantedToAdmins) {
  // Note: RW on _system also reaches this collection through the ordinary
  // path, so this pins the outcome rather than the isAdmin() short-circuit.
  beAdminWithoutDb();
  EXPECT_TRUE(
      check(p::DumpCollection{.db = std::string{kDb}, .name = "c"}).ok());
}

TEST_F(ClassicAuthModeTest, RestoreCollectionBehavesLikeWriteData) {
  beUserWith(RW);
  EXPECT_TRUE(
      check(p::RestoreCollection{
                .db = std::string{kDb}, .name = "c", .overwrite = false})
          .ok());
  beUserWith(RO);
  expectError(check(p::RestoreCollection{
                  .db = std::string{kDb}, .name = "c", .overwrite = false}),
              TRI_ERROR_ARANGO_READ_ONLY);
}

TEST_F(ClassicAuthModeTest, RestoreCollectionIsGrantedToAdmins) {
  beAdminWithoutDb();
  EXPECT_TRUE(
      check(p::RestoreCollection{
                .db = std::string{kDb}, .name = "c", .overwrite = false})
          .ok());
}

TEST_F(ClassicAuthModeTest, RestoreCollectionWithOverwriteDropsThenCreates) {
  beUserWith(RW);
  EXPECT_TRUE(check(p::RestoreCollection{
                        .db = std::string{kDb}, .name = "c", .overwrite = true})
                  .ok());
}

TEST_F(ClassicAuthModeTest, RestoreCollectionWithOverwriteNeedsToDropFirst) {
  // The drop step fails, and DropCollection maps its READ_ONLY to FORBIDDEN
  // under V0.
  beUserWith(RW, {{std::string{kDb}, "c", RO}});
  expectError(check(p::RestoreCollection{
                  .db = std::string{kDb}, .name = "c", .overwrite = true}),
              TRI_ERROR_ARANGO_READ_ONLY);
}

TEST_F(ClassicAuthModeTest, RestoreCreateIndexBehavesLikeWriteMeta) {
  // Read-write on the collection is not enough; the container principle
  // additionally requires write access to the database.
  beUserWith(RO, {{std::string{kDb}, "c", RW}});
  expectError(
      check(p::RestoreCreateIndex{.db = std::string{kDb}, .collName = "c"}),
      TRI_ERROR_FORBIDDEN);
  beUserWith(RW);
  EXPECT_TRUE(
      check(p::RestoreCreateIndex{.db = std::string{kDb}, .collName = "c"})
          .ok());
}

TEST_F(ClassicAuthModeTest,
       RestoreCreateIndexWithReadOnlyCollectionIsReadOnly) {
  // The other half of RestoreCreateIndex's WriteMeta: database RW alone is not
  // enough when the collection itself is pinned to RO.
  beUserWith(RW, {{std::string{kDb}, "c", RO}});
  expectError(
      check(p::RestoreCreateIndex{.db = std::string{kDb}, .collName = "c"}),
      TRI_ERROR_ARANGO_READ_ONLY);
}

TEST_F(ClassicAuthModeTest, RestoreCreateIndexIsGrantedToAdmins) {
  beAdminWithoutDb();
  EXPECT_TRUE(
      check(p::RestoreCreateIndex{.db = std::string{kDb}, .collName = "c"})
          .ok());
}

TEST_F(ClassicAuthModeTest, RestoreCreateViewBehavesLikeCreateView) {
  beUserWith(RW, {{std::string{kDb}, "c1", RO}});
  EXPECT_TRUE(check(p::RestoreCreateView{.db = std::string{kDb},
                                         .viewName = "v",
                                         .linkedCollNames = {"c1"}})
                  .ok());
  beUserWith(RO);
  expectError(
      check(p::RestoreCreateView{
          .db = std::string{kDb}, .viewName = "v", .linkedCollNames = {}}),
      TRI_ERROR_FORBIDDEN);
}

TEST_F(ClassicAuthModeTest,
       RestoreCreateViewWithReadableLinksButNoDatabaseWriteIsForbidden) {
  beUserWith(RO, {{std::string{kDb}, "c1", RW}});
  expectError(
      check(p::RestoreCreateView{
          .db = std::string{kDb}, .viewName = "v", .linkedCollNames = {"c1"}}),
      TRI_ERROR_FORBIDDEN);
}

TEST_F(ClassicAuthModeTest, RestoreCreateViewIsGrantedToAdmins) {
  beAdminWithoutDb();
  EXPECT_TRUE(
      check(p::RestoreCreateView{
                .db = std::string{kDb}, .viewName = "v", .linkedCollNames = {}})
          .ok());
}

TEST_F(ClassicAuthModeTest, RestoreDropViewBehavesLikeDropView) {
  beUserWith(RW);
  EXPECT_TRUE(
      check(p::RestoreDropView{.db = std::string{kDb}, .viewName = "v"}).ok());
  beUserWith(RO);
  expectError(
      check(p::RestoreDropView{.db = std::string{kDb}, .viewName = "v"}),
      TRI_ERROR_FORBIDDEN);
}

TEST_F(ClassicAuthModeTest, RestoreDropViewIsGrantedToAdmins) {
  beAdminWithoutDb();
  EXPECT_TRUE(
      check(p::RestoreDropView{.db = std::string{kDb}, .viewName = "v"}).ok());
}

TEST_F(ClassicAuthModeTest, RestoreWriteDataBehavesLikeWriteData) {
  beUserWith(RW);
  EXPECT_TRUE(
      check(p::RestoreWriteData{.db = std::string{kDb}, .collName = "c"}).ok());
  beUserWith(RO);
  expectError(
      check(p::RestoreWriteData{.db = std::string{kDb}, .collName = "c"}),
      TRI_ERROR_ARANGO_READ_ONLY);
}

TEST_F(ClassicAuthModeTest, RestoreWriteDataIsGrantedToAdmins) {
  beAdminWithoutDb();
  EXPECT_TRUE(
      check(p::RestoreWriteData{.db = std::string{kDb}, .collName = "c"}).ok());
}

// ---------------------------------------------------------------------------
// Grant-space properties
//
// The cases above each pick one interesting grant configuration by hand. The
// two suites below instead sweep the grants that could possibly influence a
// permission on kDb / kDb.kColl and assert two properties across all of them:
//
//   * necessity -- when no grant anywhere can supply a level the permission
//     requires, the check must fail, whatever else is granted. This is the
//     exhaustive form of the "one required grant alone is not enough" cases
//     above: it also covers every *combination* of the remaining grants.
//   * no-leak -- adding a grant the permission does not name never changes the
//     answer, not even the reported error code.
//
// Both properties are deliberately one-sided: they never predict *which*
// fallback wins, only that auth::User::databaseAuthLevel() and
// collectionAuthLevel() are maxima over a fixed set of sources. So neither
// suite re-implements the resolution rules, and neither needs updating when a
// fallback changes -- only when a new *source* of access is added.
//
// Collections with fixed rules (_users, _queues, _frontend) are out of scope
// here by construction: their level does not depend on the grants at all, and
// grantCollection() rejects underscore-prefixed names anyway. They are covered
// by the hand-written cases above. So is SeeView, which never fails.
// ---------------------------------------------------------------------------

// One point in the grant space. std::nullopt means "not granted at all", which
// is not the same as an explicit NONE: NONE is a level and blocks the wildcard
// and _system fallbacks, while an absent entry lets them through (see
// CollectionGrantLeavesTheDatabaseLevelUndefined).
struct GrantSpace {
  std::optional<auth::Level> system;        // grantDatabase("_system", *)
  std::optional<auth::Level> db;            // grantDatabase(kDb, *)
  std::optional<auth::Level> dbStar;        // grantDatabase("*", *)
  std::optional<auth::Level> coll;          // grantCollection(kDb, kColl, *)
  std::optional<auth::Level> collStar;      // grantCollection(kDb, "*", *)
  std::optional<auth::Level> collStarStar;  // grantCollection("*", "*", *)
};

// Which level a permission requires of what.
enum class Scope { SystemDb, Db, Coll };

struct Need {
  Scope scope;
  auth::Level level;
};

// A permission's requirements as a disjunction of conjunctions: it can succeed
// only if *some* alternative has all of its needs met. Several Classic
// permissions are "isAdmin() OR <ordinary check>", which is exactly a second
// alternative -- isAdmin() being RW on _system.
using Alternatives = std::vector<std::vector<Need>>;

struct PermCase {
  std::string_view name;  // for the gtest instance name
  auth::Permission perm;
  Alternatives alternatives;
};

// Without this gtest prints a failing parameter as a raw byte dump.
void PrintTo(PermCase const& permCase, std::ostream* os) {
  *os << permCase.perm;
}

// The largest level the resolution rules could possibly produce for `scope`,
// derived only from the fact that both level lookups are maxima over a fixed
// set of sources. This is an upper bound, never a prediction: it may say RW
// where the real level is NONE (an explicit grant of NONE shadows a wildcard).
// That is why the property below only ever asserts *failure*.
auth::Level upperBound(GrantSpace const& gs, Scope scope) {
  auto best = [](std::initializer_list<std::optional<auth::Level>> levels) {
    auto max = NONE;
    for (auto const& level : levels) {
      if (level.has_value()) {
        max = std::max(max, *level);
      }
    }
    return max;
  };
  switch (scope) {
    case Scope::SystemDb:
      // databaseAuthLevel() skips the _system fallback when asked about
      // _system itself, so only an explicit grant or the wildcard database
      // grant can raise it.
      return best({gs.system, gs.dbStar});
    case Scope::Db:
      return best({gs.db, gs.dbStar, gs.system});
    case Scope::Coll:
      // Every source collectionAuthLevel() consults for a non-system
      // collection. Note that _system's *collection* grants are not among
      // them; only its database level leaks.
      return best(
          {gs.coll, gs.collStar, gs.collStarStar, gs.db, gs.dbStar, gs.system});
  }
  ADD_FAILURE() << "unhandled scope";
  return NONE;
}

// True when no grant in `gs` can supply what any alternative asks for, i.e.
// when the permission cannot possibly be granted.
bool cannotBeGranted(GrantSpace const& gs, Alternatives const& alternatives) {
  return std::ranges::all_of(alternatives, [&](auto const& alternative) {
    return std::ranges::any_of(alternative, [&](Need const& need) {
      return need.level > upperBound(gs, need.scope);
    });
  });
}

std::vector<GrantSpace> allGrantSpaces() {
  // 4^6 = 4096 points; a point costs one setAuthInfo() plus one check().
  constexpr std::array kLevels{std::optional<auth::Level>{},
                               std::optional{NONE}, std::optional{RO},
                               std::optional{RW}};
  std::vector<GrantSpace> result;
  result.reserve(4096);
  for (auto const& system : kLevels) {
    for (auto const& db : kLevels) {
      for (auto const& dbStar : kLevels) {
        for (auto const& coll : kLevels) {
          for (auto const& collStar : kLevels) {
            for (auto const& collStarStar : kLevels) {
              result.push_back(GrantSpace{.system = system,
                                          .db = db,
                                          .dbStar = dbStar,
                                          .coll = coll,
                                          .collStar = collStar,
                                          .collStarStar = collStarStar});
            }
          }
        }
      }
    }
  }
  return result;
}

std::string describe(GrantSpace const& gs) {
  auto one = [](std::string_view label, std::optional<auth::Level> level) {
    return std::format("{}={} ", label,
                       level.has_value() ? auth::convertFromAuthLevel(*level)
                                         : std::string_view{"<ungranted>"});
  };
  auto const db = std::string{ClassicAuthModeTest::kDb};
  return one(StaticStrings::SystemDatabase, gs.system) + one(db, gs.db) +
         one("*", gs.dbStar) +
         one(db + "/" + std::string{ClassicAuthModeTest::kColl}, gs.coll) +
         one(db + "/*", gs.collStar) + one("*/*", gs.collStarStar);
}

// The permission set to sweep, with what Classic requires of each. Kept
// explicit rather than derived from AuthMode.cpp so that a change in the
// production rules has to be reflected here deliberately.
std::vector<PermCase> permissionCases() {
  auto const db = std::string{ClassicAuthModeTest::kDb};
  auto const coll = std::string{ClassicAuthModeTest::kColl};

  // CreateGraph/DropGraph hold std::span, i.e. they do not own their collection
  // lists -- so the backing storage has to outlive the cases returned here.
  static std::vector<std::string> links{coll};
  static std::vector<std::string> nothing;

  Alternatives const adminOnly{{{Scope::SystemDb, RW}}};
  auto orAdmin = [&](std::vector<Need> ordinary) -> Alternatives {
    return {{{Scope::SystemDb, RW}}, std::move(ordinary)};
  };

  return {
      // Databases.
      {"SeeDatabase", p::SeeDatabase{.name = db}, {{{Scope::Db, RO}}}},
      {"UseDatabaseRead",
       p::UseDatabase{.name = db, .level = DatabaseAccessLevel::Read},
       {{{Scope::Db, RO}}}},
      {"UseDatabaseWrite",
       p::UseDatabase{.name = db, .level = DatabaseAccessLevel::Write},
       {{{Scope::Db, RW}}}},
      {"CreateDatabase", p::CreateDatabase{.name = "brandnew"}, adminOnly},
      {"DropDatabase", p::DropDatabase{.name = db}, adminOnly},

      // Collections.
      {"UseCollectionRead",
       p::UseCollection{
           .db = db, .name = coll, .level = CollectionAccessLevel::Read},
       {{{Scope::Coll, RO}}}},
      {"UseCollectionWriteData",
       p::UseCollection{
           .db = db, .name = coll, .level = CollectionAccessLevel::WriteData},
       {{{Scope::Coll, RW}}}},
      {"UseCollectionWriteMeta",
       p::UseCollection{
           .db = db, .name = coll, .level = CollectionAccessLevel::WriteMeta},
       {{{Scope::Coll, RW}, {Scope::Db, RW}}}},
      {"CreateCollection",
       p::CreateCollection{.db = db, .name = coll},
       {{{Scope::Db, RW}}}},
      {"DropCollection",
       p::DropCollection{.db = db, .name = coll},
       {{{Scope::Db, RW}, {Scope::Coll, RW}}}},
      {"SeeCollection", p::SeeCollection{.db = db, .name = coll},
       orAdmin({{Scope::Coll, RO}})},
      {"DumpCollection", p::DumpCollection{.db = db, .name = coll},
       orAdmin({{Scope::Coll, RO}})},

      // Views.
      {"CreateView",
       p::CreateView{.db = db, .name = "v", .linkedCollections = links},
       {{{Scope::Db, RW}, {Scope::Coll, RO}}}},
      {"ModifyView",
       p::ModifyView{.db = db, .name = "v", .linkedCollections = links},
       {{{Scope::Db, RW}, {Scope::Coll, RO}}}},
      {"RenameView",
       p::RenameView{.db = db, .oldName = "v", .newName = "w"},
       {{{Scope::Db, RW}}}},
      {"DropView",
       p::DropView{.db = db, .name = "v", .linkedCollections = links},
       {{{Scope::Db, RW}, {Scope::Coll, RO}}}},
      {"ReadView", p::ReadView{.db = db, .name = "v"}, {{{Scope::Db, RO}}}},

      // Analyzers.
      {"SeeAnalyzer", p::SeeAnalyzer{.db = db, .name = "a"},
       orAdmin({{Scope::Db, RO}})},
      {"CreateAnalyzer", p::CreateAnalyzer{.db = db, .name = "a"},
       orAdmin({{Scope::Db, RW}})},
      {"DropAnalyzer", p::DropAnalyzer{.db = db, .name = "a"},
       orAdmin({{Scope::Db, RW}})},
      {"UseAnalyzerModify",
       p::UseAnalyzer{
           .db = db, .name = "a", .level = AnalyzerAccessLevel::Modify},
       orAdmin({{Scope::Db, RW}})},

      // Graphs.
      {"SeeGraph", p::SeeGraph{.db = db, .name = "g"}, {{{Scope::Db, RO}}}},
      {"UseGraphModify",
       p::UseGraph{.db = db, .name = "g", .level = GraphAccessLevel::Modify},
       {{{Scope::Db, RW}}}},
      {"CreateGraph",
       p::CreateGraph{.db = db,
                      .name = "g",
                      .collectionNamesToCreate = nothing,
                      .collectionNamesToRead = links},
       {{{Scope::Db, RW}, {Scope::Coll, RO}}}},
      {"DropGraph",
       p::DropGraph{.db = db, .name = "g", .collectionNames = links},
       {{{Scope::Db, RW}, {Scope::Coll, RW}}}},

      // Users and admin actions.
      {"ReadUser", p::ReadUser{.name = "someone"}, adminOnly},
      {"CreateUser", p::CreateUser{.name = "someone"}, adminOnly},
      {"DropUser", p::DropUser{.name = "someone"}, adminOnly},
      // The "everybody may modify their own profile" exception lives in
      // ExecContext, above this layer, so here it is a plain admin check.
      {"ModifyUserProfile", p::ModifyUserProfile{.name = "someone"}, adminOnly},
      {"GrantUserPermissions", p::GrantUserPermissions{.name = "someone"},
       adminOnly},
      {"AdminBackup", p::AdminBackup{}, adminOnly},

      // Dump / restore.
      {"RestoreCollection",
       p::RestoreCollection{.db = db, .name = coll, .overwrite = false},
       orAdmin({{Scope::Coll, RW}})},
      {"RestoreCollectionOverwrite",
       p::RestoreCollection{.db = db, .name = coll, .overwrite = true},
       orAdmin({{Scope::Db, RW}, {Scope::Coll, RW}})},
      {"RestoreCreateIndex", p::RestoreCreateIndex{.db = db, .collName = coll},
       orAdmin({{Scope::Coll, RW}, {Scope::Db, RW}})},
      {"RestoreCreateView",
       p::RestoreCreateView{
           .db = db, .viewName = "v", .linkedCollNames = links},
       orAdmin({{Scope::Db, RW}, {Scope::Coll, RO}})},
      {"RestoreDropView", p::RestoreDropView{.db = db, .viewName = "v"},
       orAdmin({{Scope::Db, RW}})},
      {"RestoreWriteData", p::RestoreWriteData{.db = db, .collName = coll},
       orAdmin({{Scope::Coll, RW}})},
  };
}

struct ClassicGrantSpaceTest : ClassicAuthModeTest,
                               ::testing::WithParamInterface<PermCase> {
  void installGrants(GrantSpace const& gs) {
    std::vector<DbGrant> dbs;
    std::vector<CollGrant> colls;
    auto addDb = [&](std::string db, std::optional<auth::Level> level) {
      if (level.has_value()) {
        dbs.push_back({std::move(db), *level});
      }
    };
    auto addColl = [&](std::string db, std::string coll,
                       std::optional<auth::Level> level) {
      if (level.has_value()) {
        colls.push_back({std::move(db), std::move(coll), *level});
      }
    };
    addDb(StaticStrings::SystemDatabase, gs.system);
    addDb(std::string{kDb}, gs.db);
    addDb("*", gs.dbStar);
    addColl(std::string{kDb}, std::string{kColl}, gs.coll);
    addColl(std::string{kDb}, "*", gs.collStar);
    addColl("*", "*", gs.collStarStar);
    setGrants(dbs, colls);
  }
};

// Property 1: if nothing in the grant space can supply a required level, the
// check fails -- no matter which other grants are present.
TEST_P(ClassicGrantSpaceTest, RequiredGrantsAreNecessary) {
  auto const& param = GetParam();
  size_t denied = 0;
  size_t granted = 0;
  for (auto const& gs : allGrantSpaces()) {
    installGrants(gs);
    auto const result = check(param.perm);
    if (cannotBeGranted(gs, param.alternatives)) {
      EXPECT_FALSE(result.ok())
          << param.name << " was granted although no grant could supply what "
          << "it requires; grants: " << describe(gs);
      ++denied;
    } else if (result.ok()) {
      ++granted;
    }
  }
  // Non-vacuity, in both directions: an upper bound so loose that no grant
  // space is ever unsatisfiable would make the property assert nothing, and a
  // permission that denies everything would satisfy it trivially.
  EXPECT_GT(denied, 0u) << param.name << ": no unsatisfiable grant space found";
  EXPECT_GT(granted, 0u) << param.name
                         << ": not granted in any grant space at all";
}

// Property 2: a grant the permission does not name changes nothing -- not the
// verdict and not the reported reason.
TEST_P(ClassicGrantSpaceTest, UnrelatedGrantsChangeNothing) {
  auto const& param = GetParam();

  // Every baseline pins both kDb and _system explicitly, so that adding a
  // collection grant cannot move a database level via the UNDEFINED path (see
  // CollectionGrantLeavesTheDatabaseLevelUndefined).
  std::vector<std::vector<DbGrant>> const baselines{
      {{StaticStrings::SystemDatabase, NONE}, {std::string{kDb}, NONE}},
      {{StaticStrings::SystemDatabase, NONE}, {std::string{kDb}, RO}},
      {{StaticStrings::SystemDatabase, NONE}, {std::string{kDb}, RW}},
      {{StaticStrings::SystemDatabase, RW}, {std::string{kDb}, NONE}},
  };
  // None of these is named by any permission under test: another database, a
  // collection in another database, a sibling collection in kDb, and a
  // collection grant on _system (of which only the *database* level leaks).
  std::vector<DbGrant> const dbDecoys{{"otherdb", RW}};
  std::vector<CollGrant> const collDecoys{
      {"otherdb", "*", RW},
      {std::string{kDb}, "unrelated", RW},
      {StaticStrings::SystemDatabase, "unrelated", RW}};

  for (auto const& baseline : baselines) {
    setGrants(baseline);
    auto const expected = check(param.perm).errorNumber();

    for (auto const& decoy : dbDecoys) {
      auto grants = baseline;
      grants.push_back(decoy);
      setGrants(grants);
      EXPECT_EQ(check(param.perm).errorNumber(), expected)
          << param.name << ": granting "
          << auth::convertFromAuthLevel(decoy.level) << " on database '"
          << decoy.db << "' changed the answer";
    }
    for (auto const& decoy : collDecoys) {
      setGrants(baseline, {decoy});
      EXPECT_EQ(check(param.perm).errorNumber(), expected)
          << param.name << ": granting "
          << auth::convertFromAuthLevel(decoy.level) << " on '" << decoy.db
          << "/" << decoy.coll << "' changed the answer";
    }
  }
}

INSTANTIATE_TEST_SUITE_P(Permissions, ClassicGrantSpaceTest,
                         ::testing::ValuesIn(permissionCases()),
                         [](::testing::TestParamInfo<PermCase> const& info) {
                           return std::string{info.param.name};
                         });

}  // namespace
