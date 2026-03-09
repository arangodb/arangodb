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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "Async/async.h"
#include "Auth/Rbac/Service.h"

using namespace arangodb;

using Cat = rbac::Service::Category;
using Perm = rbac::Service::Permission;
using AQ = rbac::Service::AuthorizationQuery;

namespace {

struct MockService : rbac::Service {
  std::vector<AuthorizationQuery> lastQueries;

  auto mayImpl(User, std::vector<AuthorizationQuery> queries) noexcept
      -> async<ResultT<bool>> override {
    lastQueries = std::move(queries);
    co_return ResultT<bool>{true};
  }

  auto maySyncImpl(User, std::vector<AuthorizationQuery> queries) noexcept
      -> ResultT<bool> override {
    lastQueries = std::move(queries);
    return {true};
  }
};

}  // namespace

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

TEST(RbacServiceTest, Databases) {
  MockService svc;
  svc.maySync({}, Perm::Read, Cat::Databases{});
  ASSERT_EQ(svc.lastQueries.size(), 1);
  EXPECT_EQ(svc.lastQueries[0].action, "db:ReadDatabases");
  EXPECT_EQ(svc.lastQueries[0].resource, "");
}

TEST(RbacServiceTest, Database) {
  MockService svc;
  svc.maySync({}, Perm::Write, Cat::Database{.name = "mydb"});
  ASSERT_EQ(svc.lastQueries.size(), 1);
  EXPECT_EQ(svc.lastQueries[0].action, "db:WriteDatabase");
  EXPECT_EQ(svc.lastQueries[0].resource, "db:database:mydb");
}

TEST(RbacServiceTest, Collection_requires_ReadDatabase) {
  MockService svc;
  svc.maySync({}, Perm::Read,
              Cat::Collection{.database = "mydb", .name = "vertices"});
  ASSERT_EQ(svc.lastQueries.size(), 2);
  EXPECT_EQ(svc.lastQueries[0].action, "db:ReadCollection");
  EXPECT_EQ(svc.lastQueries[0].resource, "db:collection:mydb:vertices");
  EXPECT_EQ(svc.lastQueries[1].action, "db:ReadDatabase");
  EXPECT_EQ(svc.lastQueries[1].resource, "db:database:mydb");
}

TEST(RbacServiceTest, Collection_write_requires_WriteDatabase) {
  MockService svc;
  svc.maySync({}, Perm::Write,
              Cat::Collection{.database = "mydb", .name = "vertices"});
  ASSERT_EQ(svc.lastQueries.size(), 2);
  EXPECT_EQ(svc.lastQueries[0].action, "db:WriteCollection");
  EXPECT_EQ(svc.lastQueries[1].action, "db:WriteDatabase");
}

TEST(RbacServiceTest, View_requires_ReadDatabase) {
  MockService svc;
  svc.maySync({}, Perm::Write, Cat::View{.database = "mydb", .name = "search"});
  ASSERT_EQ(svc.lastQueries.size(), 2);
  EXPECT_EQ(svc.lastQueries[0].action, "db:WriteView");
  EXPECT_EQ(svc.lastQueries[0].resource, "db:view:mydb:search");
  EXPECT_EQ(svc.lastQueries[1].action, "db:WriteDatabase");
  EXPECT_EQ(svc.lastQueries[1].resource, "db:database:mydb");
}

TEST(RbacServiceTest, Analyzer_requires_ReadDatabase) {
  MockService svc;
  svc.maySync({}, Perm::Read,
              Cat::Analyzer{.database = "mydb", .name = "text_en"});
  ASSERT_EQ(svc.lastQueries.size(), 2);
  EXPECT_EQ(svc.lastQueries[0].action, "db:ReadAnalyzer");
  EXPECT_EQ(svc.lastQueries[0].resource, "db:analyzer:mydb:text_en");
  EXPECT_EQ(svc.lastQueries[1].action, "db:ReadDatabase");
  EXPECT_EQ(svc.lastQueries[1].resource, "db:database:mydb");
}

TEST(RbacServiceTest, Documents_requires_ReadCollection_and_ReadDatabase) {
  MockService svc;
  svc.maySync({}, Perm::Write,
              Cat::Documents{.database = "mydb", .collection = "edges"});
  ASSERT_EQ(svc.lastQueries.size(), 3);
  EXPECT_EQ(svc.lastQueries[0].action, "db:WriteDocuments");
  EXPECT_EQ(svc.lastQueries[0].resource, "db:collection:mydb:edges");
  EXPECT_EQ(svc.lastQueries[1].action, "db:WriteCollection");
  EXPECT_EQ(svc.lastQueries[1].resource, "db:collection:mydb:edges");
  EXPECT_EQ(svc.lastQueries[2].action, "db:WriteDatabase");
  EXPECT_EQ(svc.lastQueries[2].resource, "db:database:mydb");
}

TEST(RbacServiceTest, mayAllSync_combines_hierarchies) {
  MockService svc;
  svc.mayAllSync(
      {}, {{Perm::Read, Cat::Database{.name = "db1"}},
           {Perm::Read, Cat::Collection{.database = "db2", .name = "col"}}});
  // Database: 1 query, Collection: 2 queries
  ASSERT_EQ(svc.lastQueries.size(), 3);
  EXPECT_EQ(svc.lastQueries[0].action, "db:ReadDatabase");
  EXPECT_EQ(svc.lastQueries[0].resource, "db:database:db1");
  EXPECT_EQ(svc.lastQueries[1].action, "db:ReadCollection");
  EXPECT_EQ(svc.lastQueries[1].resource, "db:collection:db2:col");
  EXPECT_EQ(svc.lastQueries[2].action, "db:ReadDatabase");
  EXPECT_EQ(svc.lastQueries[2].resource, "db:database:db2");
}

TEST(RbacServiceTest, mayAllSync_empty_queries) {
  MockService svc;
  svc.mayAllSync({}, {});
  EXPECT_TRUE(svc.lastQueries.empty());
}

#pragma GCC diagnostic pop

TEST(RbacServiceTest, may_async) {
  MockService svc;
  auto result = svc.may({}, Perm::Read, Cat::Database{.name = "mydb"});
  ASSERT_EQ(svc.lastQueries.size(), 1);
  EXPECT_EQ(svc.lastQueries[0].action, "db:ReadDatabase");
  EXPECT_EQ(svc.lastQueries[0].resource, "db:database:mydb");
}

TEST(RbacServiceTest, mayAll_async) {
  MockService svc;
  auto result = svc.mayAll(
      {}, {{Perm::Read, Cat::Documents{.database = "db", .collection = "c"}}});
  ASSERT_EQ(svc.lastQueries.size(), 3);
  EXPECT_EQ(svc.lastQueries[0].action, "db:ReadDocuments");
  EXPECT_EQ(svc.lastQueries[1].action, "db:ReadCollection");
  EXPECT_EQ(svc.lastQueries[2].action, "db:ReadDatabase");
}
