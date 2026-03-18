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

using Cat = rbac::Category;
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

TEST(RbacServiceTest, ReadDatabase) {
  MockService svc;
  svc.maySync({}, Cat::ReadDatabase{.name = "mydb"});
  ASSERT_EQ(svc.lastQueries.size(), 1);
  EXPECT_EQ(svc.lastQueries[0].action, "db:ReadDatabase");
  EXPECT_EQ(svc.lastQueries[0].resource, "db:database:mydb");
}

TEST(RbacServiceTest, WriteDatabase) {
  MockService svc;
  svc.maySync({}, Cat::WriteDatabase{.name = "mydb"});
  ASSERT_EQ(svc.lastQueries.size(), 1);
  EXPECT_EQ(svc.lastQueries[0].action, "db:WriteDatabase");
  EXPECT_EQ(svc.lastQueries[0].resource, "db:database:mydb");
}

TEST(RbacServiceTest, ReadCollection) {
  MockService svc;
  svc.maySync({}, Cat::ReadCollection{.database = "mydb", .name = "vertices"});
  ASSERT_EQ(svc.lastQueries.size(), 1);
  EXPECT_EQ(svc.lastQueries[0].action, "db:ReadCollection");
  EXPECT_EQ(svc.lastQueries[0].resource, "db:collection:mydb:vertices");
}

TEST(RbacServiceTest, WriteCollectionData) {
  MockService svc;
  svc.maySync({},
              Cat::WriteCollectionData{.database = "mydb", .name = "vertices"});
  ASSERT_EQ(svc.lastQueries.size(), 1);
  EXPECT_EQ(svc.lastQueries[0].action, "db:WriteCollectionData");
  EXPECT_EQ(svc.lastQueries[0].resource, "db:collection:mydb:vertices");
}

TEST(RbacServiceTest, WriteCollectionMeta) {
  MockService svc;
  svc.maySync({},
              Cat::WriteCollectionMeta{.database = "mydb", .name = "vertices"});
  ASSERT_EQ(svc.lastQueries.size(), 1);
  EXPECT_EQ(svc.lastQueries[0].action, "db:WriteCollectionMeta");
  EXPECT_EQ(svc.lastQueries[0].resource, "db:collection:mydb:vertices");
}

TEST(RbacServiceTest, ReadView) {
  MockService svc;
  svc.maySync({}, Cat::ReadView{.database = "mydb", .name = "search"});
  ASSERT_EQ(svc.lastQueries.size(), 1);
  EXPECT_EQ(svc.lastQueries[0].action, "db:ReadView");
  EXPECT_EQ(svc.lastQueries[0].resource, "db:view:mydb:search");
}

TEST(RbacServiceTest, WriteView) {
  MockService svc;
  svc.maySync({}, Cat::WriteView{.database = "mydb", .name = "search"});
  ASSERT_EQ(svc.lastQueries.size(), 1);
  EXPECT_EQ(svc.lastQueries[0].action, "db:WriteView");
  EXPECT_EQ(svc.lastQueries[0].resource, "db:view:mydb:search");
}

TEST(RbacServiceTest, ReadAnalyzer) {
  MockService svc;
  svc.maySync({}, Cat::ReadAnalyzer{.database = "mydb", .name = "text_en"});
  ASSERT_EQ(svc.lastQueries.size(), 1);
  EXPECT_EQ(svc.lastQueries[0].action, "db:ReadAnalyzer");
  EXPECT_EQ(svc.lastQueries[0].resource, "db:analyzer:mydb:text_en");
}

TEST(RbacServiceTest, WriteAnalyzer) {
  MockService svc;
  svc.maySync({}, Cat::WriteAnalyzer{.database = "mydb", .name = "text_en"});
  ASSERT_EQ(svc.lastQueries.size(), 1);
  EXPECT_EQ(svc.lastQueries[0].action, "db:WriteAnalyzer");
  EXPECT_EQ(svc.lastQueries[0].resource, "db:analyzer:mydb:text_en");
}

TEST(RbacServiceTest, mayAllSync_combines_categories) {
  MockService svc;
  svc.mayAllSync({}, {Cat::ReadDatabase{.name = "db1"},
                      Cat::ReadCollection{.database = "db2", .name = "col"}});
  // One query per category
  ASSERT_EQ(svc.lastQueries.size(), 2);
  EXPECT_EQ(svc.lastQueries[0].action, "db:ReadDatabase");
  EXPECT_EQ(svc.lastQueries[0].resource, "db:database:db1");
  EXPECT_EQ(svc.lastQueries[1].action, "db:ReadCollection");
  EXPECT_EQ(svc.lastQueries[1].resource, "db:collection:db2:col");
}

TEST(RbacServiceTest, mayAllSync_empty_queries) {
  MockService svc;
  svc.mayAllSync({}, {});
  EXPECT_TRUE(svc.lastQueries.empty());
}

#pragma GCC diagnostic pop
