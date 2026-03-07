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
  rbac::Backend::RequestItem lastItem;
  std::string lastJwtToken;

  auto evaluateTokenManyImpl(JwtToken const& token, RequestItems const& items,
                             transaction::MethodsApi)
      -> futures::Future<ResultT<EvaluateResponseMany>> override {
    lastJwtToken = token.jwtToken;
    lastItem = items.items.empty() ? RequestItem{} : items.items[0];
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
    lastItem = items.items.empty() ? RequestItem{} : items.items[0];
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

TEST(RbacServiceImplTest, maySync_allow_returnsTrue) {
  auto mock = std::make_unique<MockBackend>();
  mock->nextEffect = rbac::Backend::Effect::Allow;
  rbac::ServiceImpl svc{std::move(mock)};

  auto result =
      svc.maySync(rbac::Service::User{.jwtToken = testToken},
                  rbac::Service::Permission::Read,
                  rbac::Service::Category::Database{.name = "mydb"});

  ASSERT_TRUE(result.ok());
  EXPECT_TRUE(result.get());
}

TEST(RbacServiceImplTest, maySync_deny_returnsFalse) {
  auto mock = std::make_unique<MockBackend>();
  mock->nextEffect = rbac::Backend::Effect::Deny;
  rbac::ServiceImpl svc{std::move(mock)};

  auto result = svc.maySync(rbac::Service::User{.jwtToken = testToken},
                            rbac::Service::Permission::Write,
                            rbac::Service::Category::Collection{
                                .database = "mydb", .name = "vertices"});

  ASSERT_TRUE(result.ok());
  EXPECT_FALSE(result.get());
}

TEST(RbacServiceImplTest, maySync_forwardsCorrectActionAndResource) {
  auto rawMock = std::make_unique<MockBackend>();
  auto* mockPtr = rawMock.get();
  rbac::ServiceImpl svc{std::move(rawMock)};

  svc.maySync(rbac::Service::User{.jwtToken = testToken},
              rbac::Service::Permission::Read,
              rbac::Service::Category::Database{.name = "testdb"});

  EXPECT_EQ(mockPtr->lastItem.action, "db:ReadDatabase");
  EXPECT_EQ(mockPtr->lastItem.resource, "db:database:testdb");
}

TEST(RbacServiceImplTest, maySync_forwardsJwtTokenToBackend) {
  auto rawMock = std::make_unique<MockBackend>();
  auto* mockPtr = rawMock.get();
  rbac::ServiceImpl svc{std::move(rawMock)};

  svc.maySync(rbac::Service::User{.jwtToken = testToken},
              rbac::Service::Permission::Read,
              rbac::Service::Category::Database{.name = "mydb"});

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

  auto result =
      svc.maySync(rbac::Service::User{.jwtToken = testToken},
                  rbac::Service::Permission::Read,
                  rbac::Service::Category::Databases{});

  EXPECT_FALSE(result.ok());
}

TEST(RbacServiceImplTest, may_async_forwardsCorrectActionAndResource) {
  auto rawMock = std::make_unique<MockBackend>();
  auto* mockPtr = rawMock.get();
  rbac::ServiceImpl svc{std::move(rawMock)};

  std::ignore =
      svc.may(rbac::Service::User{.jwtToken = testToken},
              rbac::Service::Permission::Read,
              rbac::Service::Category::Collection{.database = "mydb",
                                                  .name = "edges"});

  EXPECT_EQ(mockPtr->lastItem.action, "db:ReadCollection");
  EXPECT_EQ(mockPtr->lastItem.resource, "db:collection:mydb:edges");
}
