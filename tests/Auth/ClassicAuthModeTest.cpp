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
// system-collection rules) are exercised for real. Two consequences worth
// knowing when reading or extending these tests:
//
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

  void useApiVersion(uint32_t version) { req.setRequestedApiVersion(version); }

  static void expectError(Result const& r, ErrorCode expected) {
    EXPECT_EQ(r.errorNumber(), expected) << r.errorMessage();
  }
};

// ---------------------------------------------------------------------------
// Accessors
// ---------------------------------------------------------------------------

TEST_F(ClassicAuthModeTest, UsernameAndRequestAreExposed) {
  EXPECT_EQ(classic.username(), kUser);
  auto r = classic.request();
  ASSERT_TRUE(r.has_value());
  EXPECT_EQ(&r->get(), &req);
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

TEST_F(ClassicAuthModeTest, UseDatabaseWithoutAccessIsForbiddenUnderV0) {
  beUserWith(NONE);
  expectError(check(p::UseDatabase{.name = std::string{kDb},
                                   .level = DatabaseAccessLevel::Read}),
              TRI_ERROR_FORBIDDEN);
}

TEST_F(ClassicAuthModeTest, UseDatabaseWithoutAccessIsNotFoundUnderV1) {
  // With a versioned API the database's existence must not be revealed.
  beUserWith(NONE);
  useApiVersion(1);
  expectError(check(p::UseDatabase{.name = std::string{kDb},
                                   .level = DatabaseAccessLevel::Read}),
              TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
}

TEST_F(ClassicAuthModeTest, UseDatabaseWriteWithReadOnlyAccessStaysForbidden) {
  // The "hide existence" branch only applies when there is no access at all;
  // insufficient access remains FORBIDDEN even under a versioned API.
  beUserWith(RO);
  useApiVersion(1);
  expectError(check(p::UseDatabase{.name = std::string{kDb},
                                   .level = DatabaseAccessLevel::Write}),
              TRI_ERROR_FORBIDDEN);
}

TEST_F(ClassicAuthModeTest, SeeDatabaseNeedsReadAccess) {
  beUserWith(RO);
  EXPECT_TRUE(check(p::SeeDatabase{.name = std::string{kDb}}).ok());
  beUserWith(NONE);
  expectError(check(p::SeeDatabase{.name = std::string{kDb}}),
              TRI_ERROR_FORBIDDEN);
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

TEST_F(ClassicAuthModeTest, UseCollectionWithoutAccessIsForbiddenUnderV0) {
  beUserWith(NONE);
  expectError(check(p::UseCollection{.db = std::string{kDb},
                                     .name = "c",
                                     .level = CollectionAccessLevel::Read}),
              TRI_ERROR_FORBIDDEN);
}

TEST_F(ClassicAuthModeTest, UseCollectionWithoutAccessIsNotFoundUnderV1) {
  beUserWith(NONE);
  useApiVersion(1);
  expectError(check(p::UseCollection{.db = std::string{kDb},
                                     .name = "c",
                                     .level = CollectionAccessLevel::Read}),
              TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
}

TEST_F(ClassicAuthModeTest,
       UseCollectionWriteDataOnReadOnlyStaysReadOnlyUnderV1) {
  // Partial access is not hidden: the collection is known to exist.
  beUserWith(RO);
  useApiVersion(1);
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
  beUserWith(RW);
  EXPECT_TRUE(check(p::UseCollection{.db = std::string{kDb},
                                     .name = "c",
                                     .level = CollectionAccessLevel::WriteMeta})
                  .ok());
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

TEST_F(ClassicAuthModeTest, QueuesCollectionIsReadableWithoutAnyGrant) {
  setGrants({});
  EXPECT_TRUE(check(p::UseCollection{.db = std::string{kDb},
                                     .name = StaticStrings::QueuesCollection,
                                     .level = CollectionAccessLevel::Read})
                  .ok());
}

TEST_F(ClassicAuthModeTest, QueuesCollectionIsNotWritableEvenForAdmins) {
  beAdmin();
  expectError(
      check(p::UseCollection{.db = std::string{kDb},
                             .name = StaticStrings::QueuesCollection,
                             .level = CollectionAccessLevel::WriteData}),
      TRI_ERROR_FORBIDDEN);
}

TEST_F(ClassicAuthModeTest, FrontendCollectionIsFullyAccessibleWithoutGrants) {
  setGrants({});
  EXPECT_TRUE(check(p::UseCollection{.db = std::string{kDb},
                                     .name = StaticStrings::FrontendCollection,
                                     .level = CollectionAccessLevel::WriteMeta})
                  .ok());
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
              TRI_ERROR_FORBIDDEN);
}

TEST_F(ClassicAuthModeTest, SeeCollectionNeedsReadAccess) {
  beUserWith(RO);
  EXPECT_TRUE(
      check(p::SeeCollection{.db = std::string{kDb}, .name = "c"}).ok());
  beUserWith(NONE);
  expectError(check(p::SeeCollection{.db = std::string{kDb}, .name = "c"}),
              TRI_ERROR_FORBIDDEN);
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
  beUserWith(RW);
  EXPECT_TRUE(
      check(p::DropCollection{.db = std::string{kDb}, .name = "c"}).ok());
}

TEST_F(ClassicAuthModeTest, DropCollectionWithoutDatabaseWriteIsForbidden) {
  beUserWith(RO);
  expectError(check(p::DropCollection{.db = std::string{kDb}, .name = "c"}),
              TRI_ERROR_FORBIDDEN);
}

TEST_F(ClassicAuthModeTest,
       DropCollectionReadOnlyCollectionIsForbiddenUnderV0) {
  // The collection-level failure is TRI_ERROR_ARANGO_READ_ONLY, but V0 must
  // keep reporting FORBIDDEN for API compatibility.
  beUserWith(RW, {{std::string{kDb}, "c", RO}});
  expectError(check(p::DropCollection{.db = std::string{kDb}, .name = "c"}),
              TRI_ERROR_FORBIDDEN);
}

TEST_F(ClassicAuthModeTest, DropCollectionReadOnlyCollectionIsReadOnlyUnderV1) {
  beUserWith(RW, {{std::string{kDb}, "c", RO}});
  useApiVersion(1);
  expectError(check(p::DropCollection{.db = std::string{kDb}, .name = "c"}),
              TRI_ERROR_ARANGO_READ_ONLY);
}

// ---------------------------------------------------------------------------
// Views
// ---------------------------------------------------------------------------

TEST_F(ClassicAuthModeTest, UseViewFollowsTheDatabaseLevel) {
  beUserWith(RO);
  EXPECT_TRUE(check(p::UseView{.db = std::string{kDb},
                               .name = "v",
                               .level = ViewAccessLevel::Read})
                  .ok());
  expectError(check(p::UseView{.db = std::string{kDb},
                               .name = "v",
                               .level = ViewAccessLevel::Modify}),
              TRI_ERROR_FORBIDDEN);
  beUserWith(RW);
  EXPECT_TRUE(check(p::UseView{.db = std::string{kDb},
                               .name = "v",
                               .level = ViewAccessLevel::Modify})
                  .ok());
}

TEST_F(ClassicAuthModeTest, UseViewWithoutAccessIsForbiddenUnderV0) {
  beUserWith(NONE);
  expectError(
      check(p::UseView{
          .db = std::string{kDb}, .name = "v", .level = ViewAccessLevel::Read}),
      TRI_ERROR_FORBIDDEN);
}

TEST_F(ClassicAuthModeTest, UseViewWithoutAccessIsNotFoundUnderV1) {
  beUserWith(NONE);
  useApiVersion(1);
  expectError(
      check(p::UseView{
          .db = std::string{kDb}, .name = "v", .level = ViewAccessLevel::Read}),
      TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
}

TEST_F(ClassicAuthModeTest, SeeViewIsAlwaysGranted) {
  // Read access to the database is the only prerequisite and has already been
  // checked by the time this question is asked.
  setGrants({});
  EXPECT_TRUE(check(p::SeeView{.db = std::string{kDb}, .name = "v"}).ok());
}

TEST_F(ClassicAuthModeTest, CreateViewNeedsDatabaseWriteAndReadableLinks) {
  beUserWith(RW);
  EXPECT_TRUE(check(p::CreateView{.db = std::string{kDb},
                                  .name = "v",
                                  .linkedCollections = {"c1", "c2"}})
                  .ok());
}

TEST_F(ClassicAuthModeTest, CreateViewWithoutDatabaseWriteIsForbidden) {
  beUserWith(RO);
  expectError(
      check(p::CreateView{
          .db = std::string{kDb}, .name = "v", .linkedCollections = {}}),
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

TEST_F(ClassicAuthModeTest, CreateViewWrapsLinkFailuresAsForbiddenUnderV1) {
  // Unlike ModifyView below, CreateView normalises every linked-collection
  // failure to FORBIDDEN -- the underlying NOT_FOUND is not passed through.
  beUserWith(RW, {{std::string{kDb}, "secret", NONE}});
  useApiVersion(1);
  expectError(check(p::CreateView{.db = std::string{kDb},
                                  .name = "v",
                                  .linkedCollections = {"secret"}}),
              TRI_ERROR_FORBIDDEN);
}

TEST_F(ClassicAuthModeTest, ModifyViewNeedsDatabaseWriteAndReadableLinks) {
  beUserWith(RW);
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

TEST_F(ClassicAuthModeTest, ModifyViewPropagatesLinkFailuresUnverbatim) {
  beUserWith(RW, {{std::string{kDb}, "secret", NONE}});
  expectError(check(p::ModifyView{.db = std::string{kDb},
                                  .name = "v",
                                  .linkedCollections = {"secret"}}),
              TRI_ERROR_FORBIDDEN);
  // Under a versioned API the inner code reaches the caller unchanged.
  useApiVersion(1);
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
              TRI_ERROR_FORBIDDEN);
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
              TRI_ERROR_FORBIDDEN);
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
              TRI_ERROR_FORBIDDEN);
  beUserWith(RW);
  EXPECT_TRUE(check(p::UseGraph{.db = std::string{kDb},
                                .name = "g",
                                .level = GraphAccessLevel::Modify})
                  .ok());
}

TEST_F(ClassicAuthModeTest, CreateGraphWithChildCollections) {
  beUserWith(RW);
  std::vector<std::string> toCreate{"cc"};
  std::vector<std::string> toRead{"cr"};
  EXPECT_TRUE(check(p::CreateGraph{.db = std::string{kDb},
                                   .name = "g",
                                   .collectionNamesToCreate = toCreate,
                                   .collectionNamesToRead = toRead})
                  .ok());
}

TEST_F(ClassicAuthModeTest, CreateGraphWithoutDatabaseWriteIsReadOnlyUnderV0) {
  // With no child collections the final database check is what fails, and its
  // FORBIDDEN is reported as TRI_ERROR_ARANGO_READ_ONLY under V0.
  beUserWith(RO);
  std::vector<std::string> none;
  expectError(check(p::CreateGraph{.db = std::string{kDb},
                                   .name = "g",
                                   .collectionNamesToCreate = none,
                                   .collectionNamesToRead = none}),
              TRI_ERROR_ARANGO_READ_ONLY);
}

TEST_F(ClassicAuthModeTest, CreateGraphWithoutDatabaseWriteIsForbiddenUnderV1) {
  beUserWith(RO);
  useApiVersion(1);
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
              TRI_ERROR_FORBIDDEN);
}

TEST_F(ClassicAuthModeTest, DropGraphWithListedCollections) {
  beUserWith(RW);
  std::vector<std::string> colls{"c1", "c2"};
  EXPECT_TRUE(
      check(p::DropGraph{
                .db = std::string{kDb}, .name = "g", .collectionNames = colls})
          .ok());
}

TEST_F(ClassicAuthModeTest, DropGraphWithoutDatabaseWriteIsForbidden) {
  beUserWith(RO);
  std::vector<std::string> none;
  expectError(
      check(p::DropGraph{
          .db = std::string{kDb}, .name = "g", .collectionNames = none}),
      TRI_ERROR_FORBIDDEN);
}

TEST_F(ClassicAuthModeTest, DropGraphWithUndroppableCollectionIsForbidden) {
  beUserWith(RW, {{std::string{kDb}, "c1", RO}});
  std::vector<std::string> colls{"c1"};
  expectError(
      check(p::DropGraph{
          .db = std::string{kDb}, .name = "g", .collectionNames = colls}),
      TRI_ERROR_FORBIDDEN);
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
  expectError(check(p::ReadUser{.name = "alice"}), TRI_ERROR_FORBIDDEN);
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
// All of them collapse into the same question: RW on _system.
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
          EXPECT_EQ(r.errorNumber(), TRI_ERROR_FORBIDDEN) << permission;
          EXPECT_NE(r.errorMessage().find("Failed admin-permission check"),
                    std::string::npos)
              << r.errorMessage();
        };
        (expectDenied(admins), ...);
      },
      AllAdminPermissions{});
}

TEST_F(ClassicAuthModeTest, AdminReadUsersIsNotSpecialInClassic) {
  // Contrast with AuthMode::Rbac, where AdminReadUsers has no counterpart yet
  // and fails closed with TRI_ERROR_NOT_IMPLEMENTED.
  beAdmin();
  EXPECT_TRUE(check(p::AdminReadUsers{}).ok());
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
              TRI_ERROR_FORBIDDEN);
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
              TRI_ERROR_FORBIDDEN);
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

TEST_F(ClassicAuthModeTest, RestoreCreateIndexIsGrantedToAdmins) {
  beAdminWithoutDb();
  EXPECT_TRUE(
      check(p::RestoreCreateIndex{.db = std::string{kDb}, .collName = "c"})
          .ok());
}

TEST_F(ClassicAuthModeTest, RestoreCreateViewBehavesLikeCreateView) {
  beUserWith(RW);
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

}  // namespace
