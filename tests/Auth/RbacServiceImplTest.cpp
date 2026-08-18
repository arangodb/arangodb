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

#include "Auth/Rbac/Backend.h"
#include "Auth/Rbac/ServiceImpl.h"

#include <array>

using namespace arangodb;

namespace {

struct MockBackend : rbac::Backend {
  rbac::Backend::Effect nextEffect = rbac::Backend::Effect::Allow;
  std::string nextMessage;  // top-level message returned to the caller
  std::vector<rbac::Backend::RequestItem> lastItems;
  std::string lastJwtToken;
  int calls = 0;

  auto evaluateTokenManyImpl(rbac::JwtToken const& token,
                             RequestItems const& items, transaction::MethodsApi)
      -> futures::Future<ResultT<EvaluateResponseMany>> override {
    ++calls;
    lastJwtToken = token.jwtToken;
    lastItems = items.items;
    EvaluateResponseMany resp{};
    resp.effect = nextEffect;
    resp.message = nextMessage;
    for ([[maybe_unused]] auto const& item : items.items) {
      resp.items.push_back(ResponseItem{.effect = nextEffect, .message = ""});
    }
    co_return resp;
  }
};

auto constexpr testToken = "eyJhbGciOiJFUzI1NiIsInR5cCI6IkpXVCJ9.test";

// Helper: build a ServiceImpl over a fresh MockBackend, keeping a raw pointer
// to the mock for inspection after the move.
struct CheckFixture {
  MockBackend* mock;
  rbac::ServiceImpl svc;

  static CheckFixture make() {
    auto backend = std::make_unique<MockBackend>();
    auto* raw = backend.get();
    return CheckFixture{raw, rbac::ServiceImpl{std::move(backend)}};
  }
};

}  // namespace

// ---------------------------------------------------------------------------
// check(): translates ActionResource pairs into backend RequestItems using the
// new "db:<Action>" + typed-resource wire vocabulary, sends them as one batch,
// and maps the aggregate effect to a Result.
// ---------------------------------------------------------------------------

TEST(RbacServiceImplCheckTest, translatesDatabaseRead) {
  auto f = CheckFixture::make();
  std::array queries{rbac::ActionResource{
      rbac::Action::Read, rbac::resources::Database{.name = "mydb"}}};
  auto r = f.svc.check({testToken}, queries);
  EXPECT_TRUE(r.ok());
  ASSERT_EQ(f.mock->lastItems.size(), 1u);
  EXPECT_EQ(f.mock->lastItems[0].action, "db:Read");
  EXPECT_EQ(f.mock->lastItems[0].resource, "db:database:mydb");
  EXPECT_EQ(f.mock->lastJwtToken, testToken);
}

TEST(RbacServiceImplCheckTest, translatesCollectionCreate) {
  auto f = CheckFixture::make();
  std::array queries{rbac::ActionResource{
      rbac::Action::Create,
      rbac::resources::Collection{.db = "mydb", .name = "c"}}};
  f.svc.check({testToken}, queries);
  ASSERT_EQ(f.mock->lastItems.size(), 1u);
  EXPECT_EQ(f.mock->lastItems[0].action, "db:Create");
  EXPECT_EQ(f.mock->lastItems[0].resource, "db:collection:mydb:c");
}

TEST(RbacServiceImplCheckTest, translatesViewDrop) {
  auto f = CheckFixture::make();
  std::array queries{rbac::ActionResource{
      rbac::Action::Drop, rbac::resources::View{.db = "mydb", .name = "v"}}};
  f.svc.check({testToken}, queries);
  ASSERT_EQ(f.mock->lastItems.size(), 1u);
  EXPECT_EQ(f.mock->lastItems[0].action, "db:Drop");
  EXPECT_EQ(f.mock->lastItems[0].resource, "db:view:mydb:v");
}

TEST(RbacServiceImplCheckTest, translatesAnalyzerWriteMeta) {
  auto f = CheckFixture::make();
  std::array queries{rbac::ActionResource{
      rbac::Action::WriteMeta,
      rbac::resources::Analyzer{.db = "mydb", .name = "a"}}};
  f.svc.check({testToken}, queries);
  ASSERT_EQ(f.mock->lastItems.size(), 1u);
  EXPECT_EQ(f.mock->lastItems[0].action, "db:WriteMeta");
  EXPECT_EQ(f.mock->lastItems[0].resource, "db:analyzer:mydb:a");
}

TEST(RbacServiceImplCheckTest, translatesCollectionWriteData) {
  auto f = CheckFixture::make();
  std::array queries{rbac::ActionResource{
      rbac::Action::WriteData,
      rbac::resources::Collection{.db = "mydb", .name = "c"}}};
  f.svc.check({testToken}, queries);
  ASSERT_EQ(f.mock->lastItems.size(), 1u);
  EXPECT_EQ(f.mock->lastItems[0].action, "db:WriteData");
  EXPECT_EQ(f.mock->lastItems[0].resource, "db:collection:mydb:c");
}

TEST(RbacServiceImplCheckTest, translatesGraphRead) {
  auto f = CheckFixture::make();
  std::array queries{rbac::ActionResource{
      rbac::Action::Read, rbac::resources::Graph{.db = "mydb", .name = "g"}}};
  f.svc.check({testToken}, queries);
  ASSERT_EQ(f.mock->lastItems.size(), 1u);
  EXPECT_EQ(f.mock->lastItems[0].action, "db:Read");
  EXPECT_EQ(f.mock->lastItems[0].resource, "db:graph:mydb:g");
}

TEST(RbacServiceImplCheckTest, translatesUserRead) {
  auto f = CheckFixture::make();
  std::array queries{rbac::ActionResource{
      rbac::Action::Read, rbac::resources::User{.name = "alice"}}};
  f.svc.check({testToken}, queries);
  ASSERT_EQ(f.mock->lastItems.size(), 1u);
  EXPECT_EQ(f.mock->lastItems[0].resource, "db:user:alice");
}

TEST(RbacServiceImplCheckTest, translatesUseApiVersion) {
  auto f = CheckFixture::make();
  std::array queries{rbac::ActionResource{
      rbac::Action::UseApiVersion, rbac::resources::ApiVersion{.version = 1}}};
  f.svc.check({testToken}, queries);
  ASSERT_EQ(f.mock->lastItems.size(), 1u);
  EXPECT_EQ(f.mock->lastItems[0].action, "db:UseApiVersion");
  EXPECT_EQ(f.mock->lastItems[0].resource, "db:apiversion:v1");
}

TEST(RbacServiceImplCheckTest, translatesUseApiVersionZero) {
  auto f = CheckFixture::make();
  std::array queries{rbac::ActionResource{
      rbac::Action::UseApiVersion, rbac::resources::ApiVersion{.version = 0}}};
  f.svc.check({testToken}, queries);
  ASSERT_EQ(f.mock->lastItems.size(), 1u);
  EXPECT_EQ(f.mock->lastItems[0].action, "db:UseApiVersion");
  EXPECT_EQ(f.mock->lastItems[0].resource, "db:apiversion:v0");
}

TEST(RbacServiceImplCheckTest, adminActionHasNoResource) {
  auto f = CheckFixture::make();
  std::array queries{rbac::ActionResource{rbac::Action::AdminQueryCache,
                                          rbac::resources::NoResource{}}};
  f.svc.check({testToken}, queries);
  ASSERT_EQ(f.mock->lastItems.size(), 1u);
  EXPECT_EQ(f.mock->lastItems[0].action, "db:AdminQueryCache");
  EXPECT_EQ(f.mock->lastItems[0].resource, "");
}

TEST(RbacServiceImplCheckTest, sendsWholeBatchInOrder) {
  auto f = CheckFixture::make();
  std::array queries{
      rbac::ActionResource{rbac::Action::Create,
                           rbac::resources::Graph{.db = "mydb", .name = "g"}},
      rbac::ActionResource{
          rbac::Action::Create,
          rbac::resources::Collection{.db = "mydb", .name = "c1"}},
      rbac::ActionResource{
          rbac::Action::Read,
          rbac::resources::Collection{.db = "mydb", .name = "c2"}}};
  f.svc.check({testToken}, queries);
  EXPECT_EQ(f.mock->calls, 1);  // one batch, one round-trip
  ASSERT_EQ(f.mock->lastItems.size(), 3u);
  EXPECT_EQ(f.mock->lastItems[0].resource, "db:graph:mydb:g");
  EXPECT_EQ(f.mock->lastItems[1].resource, "db:collection:mydb:c1");
  EXPECT_EQ(f.mock->lastItems[2].resource, "db:collection:mydb:c2");
}

TEST(RbacServiceImplCheckTest, allowReturnsOk) {
  auto f = CheckFixture::make();
  f.mock->nextEffect = rbac::Backend::Effect::Allow;
  std::array queries{rbac::ActionResource{
      rbac::Action::Read, rbac::resources::Database{.name = "mydb"}}};
  EXPECT_TRUE(f.svc.check({testToken}, queries).ok());
}

TEST(RbacServiceImplCheckTest, denyReturnsForbiddenWithMessage) {
  auto f = CheckFixture::make();
  f.mock->nextEffect = rbac::Backend::Effect::Deny;
  f.mock->nextMessage = "role lacks db:Read";
  std::array queries{rbac::ActionResource{
      rbac::Action::Read, rbac::resources::Database{.name = "mydb"}}};
  auto r = f.svc.check({testToken}, queries);
  EXPECT_EQ(r.errorNumber(), TRI_ERROR_FORBIDDEN);
  EXPECT_EQ(r.errorMessage(), "role lacks db:Read");
}

TEST(RbacServiceImplCheckTest, backendErrorIsPropagated) {
  struct ErrorBackend : rbac::Backend {
    auto evaluateTokenManyImpl(rbac::JwtToken const&, RequestItems const&,
                               transaction::MethodsApi)
        -> futures::Future<ResultT<EvaluateResponseMany>> override {
      co_return Result{TRI_ERROR_INTERNAL, "backend failure"};
    }
  };
  rbac::ServiceImpl svc{std::make_unique<ErrorBackend>()};
  std::array queries{rbac::ActionResource{
      rbac::Action::Read, rbac::resources::Database{.name = "mydb"}}};
  auto r = svc.check({testToken}, queries);
  EXPECT_EQ(r.errorNumber(), TRI_ERROR_INTERNAL);
}

TEST(RbacServiceImplCheckTest, emptyBatchIsOkWithoutBackendCall) {
  auto f = CheckFixture::make();
  auto r = f.svc.check({testToken}, {});
  EXPECT_TRUE(r.ok());
  EXPECT_EQ(f.mock->calls, 0);  // no round-trip for an empty batch
}
