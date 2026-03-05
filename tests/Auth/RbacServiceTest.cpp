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

#include "Auth/Rbac/Service.h"

using namespace arangodb;

using Cat = rbac::Service::Category;
using Perm = rbac::Service::Permission;

namespace {

struct MockService : rbac::Service {
  std::string lastAction;
  std::string lastResource;

  auto mayImpl(User, std::string action, std::string resource) noexcept
      -> ResultT<bool> override {
    lastAction = std::move(action);
    lastResource = std::move(resource);
    return {true};
  }
};

}  // namespace

TEST(RbacServiceTest, Databases) {
  MockService svc;
  svc.may({}, Perm::Read, Cat::Databases{});
  EXPECT_EQ(svc.lastAction, "db:ReadDatabases");
  EXPECT_EQ(svc.lastResource, "");
}

TEST(RbacServiceTest, Database) {
  MockService svc;
  svc.may({}, Perm::Write, Cat::Database{.name = "mydb"});
  EXPECT_EQ(svc.lastAction, "db:WriteDatabase");
  EXPECT_EQ(svc.lastResource, "db:database:mydb");
}

TEST(RbacServiceTest, Collection) {
  MockService svc;
  svc.may({}, Perm::Read,
          Cat::Collection{.database = "mydb", .name = "vertices"});
  EXPECT_EQ(svc.lastAction, "db:ReadCollection");
  EXPECT_EQ(svc.lastResource, "db:collection:mydb:vertices");
}

TEST(RbacServiceTest, View) {
  MockService svc;
  svc.may({}, Perm::Write, Cat::View{.database = "mydb", .name = "search"});
  EXPECT_EQ(svc.lastAction, "db:WriteView");
  EXPECT_EQ(svc.lastResource, "db:view:mydb:search");
}

TEST(RbacServiceTest, Analyzer) {
  MockService svc;
  svc.may({}, Perm::Read,
          Cat::Analyzer{.database = "mydb", .name = "text_en"});
  EXPECT_EQ(svc.lastAction, "db:ReadAnalyzer");
  EXPECT_EQ(svc.lastResource, "db:analyzer:mydb:text_en");
}

TEST(RbacServiceTest, Documents) {
  MockService svc;
  svc.may({}, Perm::Write,
          Cat::Documents{.database = "mydb", .collection = "edges"});
  EXPECT_EQ(svc.lastAction, "db:WriteDocuments");
  EXPECT_EQ(svc.lastResource, "db:collection:mydb:edges");
}
