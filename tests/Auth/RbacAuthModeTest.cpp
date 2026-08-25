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

// Unit tests for AuthMode::Rbac::check. They verify that each permission from
// the auth::perms vocabulary is translated into the expected (Action, Resource)
// query (or sequence of queries) against the RBAC Service. The Service itself
// is mocked: it records every question it is asked and returns a programmed
// answer, so these tests never talk to a real authorization backend.

#include "gtest/gtest.h"

#include <span>
#include <string>
#include <variant>
#include <vector>

#include "Auth/AuthMode.h"
#include "Auth/Permissions.h"
#include "Auth/Rbac/Service.h"
#include "Basics/overload.h"
#include "Basics/voc-errors.h"

using namespace arangodb;
namespace p = arangodb::auth::perms;

namespace {

// Render a Resource into a stable, human-readable string. Crucially this copies
// the underlying data out of the (view-based) resource immediately, since the
// resource's string_views point into the perms:: struct that only lives for the
// duration of the check() call.
std::string resourceStr(rbac::Resource const& resource) {
  return std::visit(overload{
                        [](rbac::resources::NoResource const&) {
                          return std::string{"<none>"};
                        },
                        [](rbac::resources::Database const& r) {
                          return std::string{"database:"} + std::string{r.name};
                        },
                        [](rbac::resources::Collection const& r) {
                          return std::string{"collection:"} +
                                 std::string{r.db} + ":" + std::string{r.name};
                        },
                        [](rbac::resources::View const& r) {
                          return std::string{"view:"} + std::string{r.db} +
                                 ":" + std::string{r.name};
                        },
                        [](rbac::resources::Analyzer const& r) {
                          return std::string{"analyzer:"} + std::string{r.db} +
                                 ":" + std::string{r.name};
                        },
                        [](rbac::resources::Graph const& r) {
                          return std::string{"graph:"} + std::string{r.db} +
                                 ":" + std::string{r.name};
                        },
                        [](rbac::resources::User const& r) {
                          return std::string{"user:"} + std::string{r.name};
                        },
                        [](rbac::resources::ApiVersion const& r) {
                          return std::string{"apiversion:"} +
                                 std::to_string(r.version);
                        },
                    },
                    resource);
}

// A Service that records every check() it receives and answers with a
// programmed Result (ok by default).
struct MockService : rbac::Service {
  struct Query {
    rbac::Action action;
    std::string resource;
  };

  std::vector<Query> queries;  // all pairs, flattened across all check() calls
  int checkCalls = 0;          // number of check() invocations
  Result answer{};             // returned from every check(); {} == ok
  rbac::Subject lastSubject;   // subject of the most recent check() call

  auto check(rbac::Subject const& subject,
             std::span<rbac::ActionResource const> qs) noexcept
      -> Result override {
    ++checkCalls;
    lastSubject = subject;
    for (auto const& q : qs) {
      queries.push_back({q.action, resourceStr(q.resource)});
    }

    return answer;
  }
};

// Fixture bundling a mock service and the Rbac auth mode under test.
struct RbacAuthModeTest : ::testing::Test {
  MockService svc;
  AuthMode::Rbac rbac{svc, "myuser", "mytoken"};

  // Discarding wrapper around rbac.check(). IAuth::check is [[nodiscard]], so
  // tests that only inspect the recorded queries would otherwise not compile
  // under -Werror; going through this helper avoids sprinkling (void) casts.
  Result check(auth::Permission perm) { return rbac.check(std::move(perm)); }

  // Convenience: single expected query.
  void expectSingle(rbac::Action action, std::string const& resource) {
    ASSERT_EQ(svc.queries.size(), 1u);
    EXPECT_EQ(svc.queries[0].action, action);
    EXPECT_EQ(svc.queries[0].resource, resource);
  }
};

// ---------------------------------------------------------------------------
// Databases
// ---------------------------------------------------------------------------

TEST_F(RbacAuthModeTest, SeeDatabase) {
  EXPECT_TRUE(check(p::SeeDatabase{.name = "mydb"}).ok());
  expectSingle(rbac::Action::Read, "database:mydb");
}

TEST_F(RbacAuthModeTest, CreateDatabase) {
  EXPECT_TRUE(check(p::CreateDatabase{.name = "mydb"}).ok());
  expectSingle(rbac::Action::Create, "database:mydb");
}

TEST_F(RbacAuthModeTest, DropDatabase) {
  EXPECT_TRUE(check(p::DropDatabase{.name = "mydb"}).ok());
  expectSingle(rbac::Action::Drop, "database:mydb");
}

TEST_F(RbacAuthModeTest, UseDatabaseRead) {
  check(p::UseDatabase{.name = "mydb", .level = DatabaseAccessLevel::Read});
  expectSingle(rbac::Action::Read, "database:mydb");
}

TEST_F(RbacAuthModeTest, UseDatabaseWrite) {
  check(p::UseDatabase{.name = "mydb", .level = DatabaseAccessLevel::Write});
  expectSingle(rbac::Action::WriteMeta, "database:mydb");
}

// ---------------------------------------------------------------------------
// Collections
// ---------------------------------------------------------------------------

TEST_F(RbacAuthModeTest, SeeCollection) {
  check(p::SeeCollection{.db = "mydb", .name = "c"});
  expectSingle(rbac::Action::Read, "collection:mydb:c");
}

TEST_F(RbacAuthModeTest, CreateCollection) {
  check(p::CreateCollection{.db = "mydb", .name = "c"});
  expectSingle(rbac::Action::Create, "collection:mydb:c");
}

TEST_F(RbacAuthModeTest, DropCollection) {
  check(p::DropCollection{.db = "mydb", .name = "c"});
  expectSingle(rbac::Action::Drop, "collection:mydb:c");
}

TEST_F(RbacAuthModeTest, UseCollectionRead) {
  check(p::UseCollection{
      .db = "mydb", .name = "c", .level = CollectionAccessLevel::Read});
  expectSingle(rbac::Action::Read, "collection:mydb:c");
}

TEST_F(RbacAuthModeTest, UseCollectionWriteData) {
  check(p::UseCollection{
      .db = "mydb", .name = "c", .level = CollectionAccessLevel::WriteData});
  expectSingle(rbac::Action::WriteData, "collection:mydb:c");
}

TEST_F(RbacAuthModeTest, UseCollectionWriteMeta) {
  check(p::UseCollection{
      .db = "mydb", .name = "c", .level = CollectionAccessLevel::WriteMeta});
  expectSingle(rbac::Action::WriteMeta, "collection:mydb:c");
}

// ---------------------------------------------------------------------------
// Views
// ---------------------------------------------------------------------------

TEST_F(RbacAuthModeTest, SeeView) {
  check(p::SeeView{.db = "mydb", .name = "v"});
  expectSingle(rbac::Action::Read, "view:mydb:v");
}

TEST_F(RbacAuthModeTest, DropView) {
  check(p::DropView{.db = "mydb", .name = "v"});
  expectSingle(rbac::Action::Drop, "view:mydb:v");
}

TEST_F(RbacAuthModeTest, DropViewChecksViewThenLinkedCollections) {
  std::vector<std::string> links{"c1", "c2"};
  EXPECT_TRUE(
      check(p::DropView{.db = "mydb", .name = "v", .linkedCollections = links})
          .ok());
  EXPECT_EQ(svc.checkCalls, 1);  // view + linked collections in one batch
  ASSERT_EQ(svc.queries.size(), 3u);
  EXPECT_EQ(svc.queries[0].action, rbac::Action::Drop);
  EXPECT_EQ(svc.queries[0].resource, "view:mydb:v");
  EXPECT_EQ(svc.queries[1].action, rbac::Action::Read);
  EXPECT_EQ(svc.queries[1].resource, "collection:mydb:c1");
  EXPECT_EQ(svc.queries[2].action, rbac::Action::Read);
  EXPECT_EQ(svc.queries[2].resource, "collection:mydb:c2");
}

TEST_F(RbacAuthModeTest, ReadView) {
  check(p::ReadView{.db = "mydb", .name = "v"});
  expectSingle(rbac::Action::Read, "view:mydb:v");
}

TEST_F(RbacAuthModeTest, CreateViewChecksViewThenLinkedCollections) {
  std::vector<std::string> links{"c1", "c2"};
  EXPECT_TRUE(check(p::CreateView{
                        .db = "mydb", .name = "v", .linkedCollections = links})
                  .ok());
  EXPECT_EQ(svc.checkCalls, 1);  // view + linked collections in one batch
  ASSERT_EQ(svc.queries.size(), 3u);
  EXPECT_EQ(svc.queries[0].action, rbac::Action::Create);
  EXPECT_EQ(svc.queries[0].resource, "view:mydb:v");
  EXPECT_EQ(svc.queries[1].action, rbac::Action::Read);
  EXPECT_EQ(svc.queries[1].resource, "collection:mydb:c1");
  EXPECT_EQ(svc.queries[2].action, rbac::Action::Read);
  EXPECT_EQ(svc.queries[2].resource, "collection:mydb:c2");
}

TEST_F(RbacAuthModeTest, ModifyViewChecksViewThenLinkedCollections) {
  std::vector<std::string> links{"c1"};
  check(p::ModifyView{.db = "mydb", .name = "v", .linkedCollections = links});
  EXPECT_EQ(svc.checkCalls, 1);
  ASSERT_EQ(svc.queries.size(), 2u);
  EXPECT_EQ(svc.queries[0].action, rbac::Action::WriteMeta);
  EXPECT_EQ(svc.queries[0].resource, "view:mydb:v");
  EXPECT_EQ(svc.queries[1].action, rbac::Action::Read);
  EXPECT_EQ(svc.queries[1].resource, "collection:mydb:c1");
}

TEST_F(RbacAuthModeTest, RenameViewChecksOldAndNewName) {
  check(p::RenameView{.db = "mydb", .oldName = "old", .newName = "new"});
  EXPECT_EQ(svc.checkCalls, 1);
  ASSERT_EQ(svc.queries.size(), 2u);
  EXPECT_EQ(svc.queries[0].action, rbac::Action::Drop);
  EXPECT_EQ(svc.queries[0].resource, "view:mydb:old");
  EXPECT_EQ(svc.queries[1].action, rbac::Action::Create);
  EXPECT_EQ(svc.queries[1].resource, "view:mydb:new");
}

TEST_F(RbacAuthModeTest, RenameViewChecksOldAndNewNameThenLinkedCollections) {
  std::vector<std::string> links{"c1", "c2"};
  EXPECT_TRUE(check(p::RenameView{.db = "mydb",
                                  .oldName = "old",
                                  .newName = "new",
                                  .linkedCollections = links})
                  .ok());
  EXPECT_EQ(svc.checkCalls, 1);  // old + new + linked collections in one batch
  ASSERT_EQ(svc.queries.size(), 4u);
  EXPECT_EQ(svc.queries[0].action, rbac::Action::Drop);
  EXPECT_EQ(svc.queries[0].resource, "view:mydb:old");
  EXPECT_EQ(svc.queries[1].action, rbac::Action::Create);
  EXPECT_EQ(svc.queries[1].resource, "view:mydb:new");
  EXPECT_EQ(svc.queries[2].action, rbac::Action::Read);
  EXPECT_EQ(svc.queries[2].resource, "collection:mydb:c1");
  EXPECT_EQ(svc.queries[3].action, rbac::Action::Read);
  EXPECT_EQ(svc.queries[3].resource, "collection:mydb:c2");
}

TEST_F(RbacAuthModeTest, RenameViewToSameNameIsBadParameterAndAsksNothing) {
  auto r = check(p::RenameView{.db = "mydb", .oldName = "v", .newName = "v"});
  EXPECT_EQ(r.errorNumber(), TRI_ERROR_BAD_PARAMETER);
  EXPECT_TRUE(svc.queries.empty());
}

// ---------------------------------------------------------------------------
// Analyzers
// ---------------------------------------------------------------------------

TEST_F(RbacAuthModeTest, SeeAnalyzer) {
  check(p::SeeAnalyzer{.db = "mydb", .name = "a"});
  expectSingle(rbac::Action::Read, "analyzer:mydb:a");
}

TEST_F(RbacAuthModeTest, CreateAnalyzer) {
  check(p::CreateAnalyzer{.db = "mydb", .name = "a"});
  expectSingle(rbac::Action::Create, "analyzer:mydb:a");
}

TEST_F(RbacAuthModeTest, DropAnalyzer) {
  check(p::DropAnalyzer{.db = "mydb", .name = "a"});
  expectSingle(rbac::Action::Drop, "analyzer:mydb:a");
}

TEST_F(RbacAuthModeTest, UseAnalyzerRead) {
  check(p::UseAnalyzer{
      .db = "mydb", .name = "a", .level = AnalyzerAccessLevel::Read});
  expectSingle(rbac::Action::Read, "analyzer:mydb:a");
}

TEST_F(RbacAuthModeTest, UseAnalyzerModify) {
  check(p::UseAnalyzer{
      .db = "mydb", .name = "a", .level = AnalyzerAccessLevel::Modify});
  expectSingle(rbac::Action::WriteMeta, "analyzer:mydb:a");
}

// ---------------------------------------------------------------------------
// Graphs
// ---------------------------------------------------------------------------

TEST_F(RbacAuthModeTest, SeeGraph) {
  check(p::SeeGraph{.db = "mydb", .name = "g"});
  expectSingle(rbac::Action::Read, "graph:mydb:g");
}

TEST_F(RbacAuthModeTest, UseGraphRead) {
  check(
      p::UseGraph{.db = "mydb", .name = "g", .level = GraphAccessLevel::Read});
  expectSingle(rbac::Action::Read, "graph:mydb:g");
}

TEST_F(RbacAuthModeTest, UseGraphModify) {
  check(p::UseGraph{
      .db = "mydb", .name = "g", .level = GraphAccessLevel::Modify});
  expectSingle(rbac::Action::WriteMeta, "graph:mydb:g");
}

TEST_F(RbacAuthModeTest, CreateGraphChecksGraphThenChildCollections) {
  std::vector<std::string> toCreate{"cc"};
  std::vector<std::string> toRead{"cr"};
  check(p::CreateGraph{.db = "mydb",
                       .name = "g",
                       .collectionNamesToCreate = toCreate,
                       .collectionNamesToRead = toRead});
  EXPECT_EQ(svc.checkCalls, 1);  // graph + child collections in one batch
  ASSERT_EQ(svc.queries.size(), 3u);
  EXPECT_EQ(svc.queries[0].action, rbac::Action::Create);
  EXPECT_EQ(svc.queries[0].resource, "graph:mydb:g");
  EXPECT_EQ(svc.queries[1].action, rbac::Action::Create);
  EXPECT_EQ(svc.queries[1].resource, "collection:mydb:cc");
  EXPECT_EQ(svc.queries[2].action, rbac::Action::Read);
  EXPECT_EQ(svc.queries[2].resource, "collection:mydb:cr");
}

TEST_F(RbacAuthModeTest, DropGraphChecksGraphThenListedCollections) {
  std::vector<std::string> colls{"c1", "c2"};
  check(p::DropGraph{.db = "mydb", .name = "g", .collectionNames = colls});
  EXPECT_EQ(svc.checkCalls, 1);  // graph + listed collections in one batch
  ASSERT_EQ(svc.queries.size(), 3u);
  EXPECT_EQ(svc.queries[0].action, rbac::Action::Drop);
  EXPECT_EQ(svc.queries[0].resource, "graph:mydb:g");
  EXPECT_EQ(svc.queries[1].action, rbac::Action::Drop);
  EXPECT_EQ(svc.queries[1].resource, "collection:mydb:c1");
  EXPECT_EQ(svc.queries[2].action, rbac::Action::Drop);
  EXPECT_EQ(svc.queries[2].resource, "collection:mydb:c2");
}

// ---------------------------------------------------------------------------
// Users
// ---------------------------------------------------------------------------

TEST_F(RbacAuthModeTest, ReadUser) {
  check(p::ReadUser{.name = "alice"});
  expectSingle(rbac::Action::Read, "user:alice");
}

TEST_F(RbacAuthModeTest, CreateUser) {
  check(p::CreateUser{.name = "alice"});
  expectSingle(rbac::Action::Create, "user:alice");
}

TEST_F(RbacAuthModeTest, DropUser) {
  check(p::DropUser{.name = "alice"});
  expectSingle(rbac::Action::Drop, "user:alice");
}

TEST_F(RbacAuthModeTest, ModifyUserProfile) {
  check(p::ModifyUserProfile{.name = "alice"});
  expectSingle(rbac::Action::WriteMeta, "user:alice");
}

TEST_F(RbacAuthModeTest, GrantUserPermissions) {
  check(p::GrantUserPermissions{.name = "alice"});
  ASSERT_TRUE(svc.queries.empty());
}

// ---------------------------------------------------------------------------
// Dump / Restore (delegate to the collection/view permissions)
// ---------------------------------------------------------------------------

TEST_F(RbacAuthModeTest, DumpCollection) {
  check(p::DumpCollection{.db = "mydb", .name = "c"});
  expectSingle(rbac::Action::Read, "collection:mydb:c");
}

TEST_F(RbacAuthModeTest, RestoreWriteData) {
  check(p::RestoreWriteData{.db = "mydb", .collName = "c"});
  expectSingle(rbac::Action::WriteData, "collection:mydb:c");
}

TEST_F(RbacAuthModeTest, RestoreCreateIndex) {
  check(p::RestoreCreateIndex{.db = "mydb", .collName = "c"});
  expectSingle(rbac::Action::WriteMeta, "collection:mydb:c");
}

TEST_F(RbacAuthModeTest, RestoreDropView) {
  check(p::RestoreDropView{.db = "mydb", .viewName = "v"});
  expectSingle(rbac::Action::Drop, "view:mydb:v");
}

TEST_F(RbacAuthModeTest, RestoreCollectionWithoutOverwriteWritesData) {
  check(p::RestoreCollection{.db = "mydb", .name = "c", .overwrite = false});
  expectSingle(rbac::Action::WriteData, "collection:mydb:c");
}

TEST_F(RbacAuthModeTest, RestoreCollectionWithOverwriteDropsThenCreates) {
  check(p::RestoreCollection{.db = "mydb", .name = "c", .overwrite = true});
  ASSERT_EQ(svc.queries.size(), 2u);
  EXPECT_EQ(svc.queries[0].action, rbac::Action::Drop);
  EXPECT_EQ(svc.queries[0].resource, "collection:mydb:c");
  EXPECT_EQ(svc.queries[1].action, rbac::Action::Create);
  EXPECT_EQ(svc.queries[1].resource, "collection:mydb:c");
}

TEST_F(RbacAuthModeTest, RestoreCreateViewChecksViewThenLinkedCollections) {
  check(p::RestoreCreateView{
      .db = "mydb", .viewName = "v", .linkedCollNames = {"c1"}});
  ASSERT_EQ(svc.queries.size(), 2u);
  EXPECT_EQ(svc.queries[0].action, rbac::Action::Create);
  EXPECT_EQ(svc.queries[0].resource, "view:mydb:v");
  EXPECT_EQ(svc.queries[1].action, rbac::Action::Read);
  EXPECT_EQ(svc.queries[1].resource, "collection:mydb:c1");
}

// ---------------------------------------------------------------------------
// Admin actions (map 1:1 to the identically-named action, no resource)
// ---------------------------------------------------------------------------

TEST_F(RbacAuthModeTest, AdminReadUsers) {
  check(p::AdminReadUsers{});
  expectSingle(rbac::Action::AdminReadUsers, "<none>");
}

TEST_F(RbacAuthModeTest, AdminMonitoring) {
  check(p::AdminMonitoring{});
  expectSingle(rbac::Action::AdminMonitoring, "<none>");
}

TEST_F(RbacAuthModeTest, AdminShutdown) {
  check(p::AdminShutdown{});
  expectSingle(rbac::Action::AdminShutdown, "<none>");
}

TEST_F(RbacAuthModeTest, AdminBackup) {
  check(p::AdminBackup{});
  expectSingle(rbac::Action::AdminBackup, "<none>");
}

TEST_F(RbacAuthModeTest, AdminQueryCache) {
  check(p::AdminQueryCache{});
  expectSingle(rbac::Action::AdminQueryCache, "<none>");
}

// ---------------------------------------------------------------------------
// API versions
// ---------------------------------------------------------------------------

TEST_F(RbacAuthModeTest, UseApiVersionAsksForTheVersionAsResource) {
  EXPECT_TRUE(check(p::UseApiVersion{.version = 1}).ok());
  expectSingle(rbac::Action::UseApiVersion, "apiversion:1");
}

TEST_F(RbacAuthModeTest, UseApiVersionDistinguishesVersions) {
  check(p::UseApiVersion{.version = 0});
  expectSingle(rbac::Action::UseApiVersion, "apiversion:0");
}

TEST_F(RbacAuthModeTest, UseApiVersionDenialIsPropagated) {
  svc.answer = {TRI_ERROR_FORBIDDEN, "nope"};
  EXPECT_EQ(check(p::UseApiVersion{.version = 1}).errorNumber(),
            TRI_ERROR_FORBIDDEN);
}

// ---------------------------------------------------------------------------
// Result propagation / short-circuiting
// ---------------------------------------------------------------------------

TEST_F(RbacAuthModeTest, DeniedResultIsPropagated) {
  svc.answer = {TRI_ERROR_FORBIDDEN, "nope"};
  auto r = check(p::SeeDatabase{.name = "mydb"});
  EXPECT_EQ(r.errorNumber(), TRI_ERROR_FORBIDDEN);
}

TEST_F(RbacAuthModeTest, CompositeCheckIsSentAsOneBatchAndDenialPropagates) {
  // A composite permission is evaluated as a single batch: all pairs are sent
  // together in one Service::check() call (one network round-trip), and the
  // service's denial is propagated. It is the service's job to decide the batch
  // as a whole; the auth mode does not pre-filter pairs.
  svc.answer = {TRI_ERROR_FORBIDDEN, "nope"};
  std::vector<std::string> toCreate{"cc"};
  std::vector<std::string> toRead{"cr"};
  auto r = check(p::CreateGraph{.db = "mydb",
                                .name = "g",
                                .collectionNamesToCreate = toCreate,
                                .collectionNamesToRead = toRead});
  EXPECT_EQ(r.errorNumber(), TRI_ERROR_FORBIDDEN);
  EXPECT_EQ(svc.checkCalls, 1);
  ASSERT_EQ(svc.queries.size(), 3u);
  EXPECT_EQ(svc.queries[0].resource, "graph:mydb:g");
  EXPECT_EQ(svc.queries[1].resource, "collection:mydb:cc");
  EXPECT_EQ(svc.queries[2].resource, "collection:mydb:cr");
}

// ---------------------------------------------------------------------------
// Which subject the authorization service is asked about (COR-907)
// ---------------------------------------------------------------------------

TEST_F(RbacAuthModeTest, JwtAuthenticatedRequestIsIdentifiedByToken) {
  // The fixture's mode has both a username and a token, as a Bearer-
  // authenticated request does. The token is the more precise identity.
  auto r = check(p::SeeDatabase{.name = "mydb"});
  ASSERT_TRUE(r.ok()) << r.errorMessage();
  ASSERT_TRUE(std::holds_alternative<rbac::JwtToken>(svc.lastSubject));
  EXPECT_EQ(std::get<rbac::JwtToken>(svc.lastSubject).jwtToken, "mytoken");
}

TEST(RbacAuthModeSubjectTest, BasicAuthenticatedRequestIsIdentifiedByUsername) {
  // HTTP Basic authentication (and personal access tokens) produce no JWT, so
  // the verified username has to identify the caller. Before COR-907 this sent
  // an empty token, which the authorization service always denied.
  MockService svc;
  AuthMode::Rbac rbac{svc, "myuser", ""};

  auto r = rbac.check(p::SeeDatabase{.name = "mydb"});
  ASSERT_TRUE(r.ok()) << r.errorMessage();
  EXPECT_EQ(svc.checkCalls, 1);
  ASSERT_TRUE(std::holds_alternative<rbac::Username>(svc.lastSubject));
  EXPECT_EQ(std::get<rbac::Username>(svc.lastSubject).name, "myuser");
}

TEST(RbacAuthModeSubjectTest, NoSubjectFailsClosedWithoutAskingTheService) {
  MockService svc;
  AuthMode::Rbac rbac{svc, "", ""};

  auto r = rbac.check(p::SeeDatabase{.name = "mydb"});
  EXPECT_EQ(r.errorNumber(), TRI_ERROR_FORBIDDEN);
  EXPECT_EQ(svc.checkCalls, 0);
}

}  // namespace
