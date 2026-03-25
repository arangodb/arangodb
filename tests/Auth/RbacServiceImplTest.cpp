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

#include "Auth/Rbac/Backend.h"
#include "Auth/Rbac/ServiceImpl.h"

using namespace arangodb;

namespace {

struct MockBackend : rbac::Backend {
  rbac::Backend::Effect nextEffect = rbac::Backend::Effect::Allow;
  std::vector<rbac::Backend::RequestItem> lastItems;
  std::string lastJwtToken;

  auto evaluateTokenManyImpl(JwtToken const& token, RequestItems const& items,
                             transaction::MethodsApi)
      -> futures::Future<ResultT<EvaluateResponseMany>> override {
    lastJwtToken = token.jwtToken;
    lastItems = items.items;
    EvaluateResponseMany resp{};
    resp.effect = nextEffect;
    for ([[maybe_unused]] auto const& item : items.items) {
      resp.items.push_back(ResponseItem{.effect = nextEffect, .message = ""});
    }
    co_return resp;
  }

  auto evaluateManyImpl(PlainUser const&, RequestItems const& items,
                        transaction::MethodsApi)
      -> futures::Future<ResultT<EvaluateResponseMany>> override {
    lastItems = items.items;
    EvaluateResponseMany resp{};
    resp.effect = nextEffect;
    for ([[maybe_unused]] auto const& item : items.items) {
      resp.items.push_back(ResponseItem{.effect = nextEffect, .message = ""});
    }
    co_return resp;
  }
};

}  // namespace

auto constexpr testToken = "eyJhbGciOiJFUzI1NiIsInR5cCI6IkpXVCJ9.test";

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

TEST(RbacServiceImplTest, maySync_allow_returnsTrue) {
  auto mock = std::make_unique<MockBackend>();
  mock->nextEffect = rbac::Backend::Effect::Allow;
  rbac::ServiceImpl svc{std::move(mock)};

  auto result = svc.maySync(rbac::Service::User{.jwtToken = testToken},
                            rbac::Category::ReadDatabase{.name = "mydb"});

  ASSERT_TRUE(result.ok());
  EXPECT_TRUE(result.get());
}

TEST(RbacServiceImplTest, maySync_deny_returnsFalse) {
  auto mock = std::make_unique<MockBackend>();
  mock->nextEffect = rbac::Backend::Effect::Deny;
  rbac::ServiceImpl svc{std::move(mock)};

  auto result = svc.maySync(rbac::Service::User{.jwtToken = testToken},
                            rbac::Category::WriteCollectionData{
                                .database = "mydb", .name = "vertices"});

  ASSERT_TRUE(result.ok());
  EXPECT_FALSE(result.get());
}

TEST(RbacServiceImplTest, maySync_database_sends_single_item) {
  auto rawMock = std::make_unique<MockBackend>();
  auto* mockPtr = rawMock.get();
  rbac::ServiceImpl svc{std::move(rawMock)};

  svc.maySync(rbac::Service::User{.jwtToken = testToken},
              rbac::Category::ReadDatabase{.name = "testdb"});

  ASSERT_EQ(mockPtr->lastItems.size(), 1);
  EXPECT_EQ(mockPtr->lastItems[0].action, "db:ReadDatabase");
  EXPECT_EQ(mockPtr->lastItems[0].resource, "db:database:testdb");
}

TEST(RbacServiceImplTest, maySync_collection_sends_single_item) {
  auto rawMock = std::make_unique<MockBackend>();
  auto* mockPtr = rawMock.get();
  rbac::ServiceImpl svc{std::move(rawMock)};

  svc.maySync(
      rbac::Service::User{.jwtToken = testToken},
      rbac::Category::ReadCollection{.database = "mydb", .name = "edges"});

  ASSERT_EQ(mockPtr->lastItems.size(), 1);
  EXPECT_EQ(mockPtr->lastItems[0].action, "db:ReadCollection");
  EXPECT_EQ(mockPtr->lastItems[0].resource, "db:collection:mydb:edges");
}

TEST(RbacServiceImplTest, maySync_forwardsJwtTokenToBackend) {
  auto rawMock = std::make_unique<MockBackend>();
  auto* mockPtr = rawMock.get();
  rbac::ServiceImpl svc{std::move(rawMock)};

  svc.maySync(rbac::Service::User{.jwtToken = testToken},
              rbac::Category::ReadDatabase{.name = "mydb"});

  EXPECT_EQ(mockPtr->lastJwtToken, testToken);
}

TEST(RbacServiceImplTest, maySync_backendError_propagatesError) {
  struct ErrorBackend : rbac::Backend {
    auto evaluateTokenManyImpl(JwtToken const&, RequestItems const&,
                               transaction::MethodsApi)
        -> futures::Future<ResultT<EvaluateResponseMany>> override {
      co_return Result{TRI_ERROR_INTERNAL, "backend failure"};
    }
    auto evaluateManyImpl(PlainUser const&, RequestItems const&,
                          transaction::MethodsApi)
        -> futures::Future<ResultT<EvaluateResponseMany>> override {
      co_return Result{TRI_ERROR_INTERNAL, "backend failure"};
    }
  };

  rbac::ServiceImpl svc{std::make_unique<ErrorBackend>()};

  auto result = svc.maySync(rbac::Service::User{.jwtToken = testToken},
                            rbac::Category::ReadDatabase{.name = "mydb"});

  EXPECT_FALSE(result.ok());
}

TEST(RbacServiceImplTest, may_async_collection_sends_single_item) {
  auto rawMock = std::make_unique<MockBackend>();
  auto* mockPtr = rawMock.get();
  rbac::ServiceImpl svc{std::move(rawMock)};

  std::ignore = svc.may(
      rbac::Service::User{.jwtToken = testToken},
      rbac::Category::ReadCollection{.database = "mydb", .name = "edges"});

  ASSERT_EQ(mockPtr->lastItems.size(), 1);
  EXPECT_EQ(mockPtr->lastItems[0].action, "db:ReadCollection");
  EXPECT_EQ(mockPtr->lastItems[0].resource, "db:collection:mydb:edges");
}

#pragma GCC diagnostic pop
