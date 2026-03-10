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

#include "Auth/Rbac/BackendImpl.h"
#include "Auth/Rbac/ServiceImpl.h"
#include "Auth/SmockerClient.h"
#include "Network/ConnectionPool.h"

#include <fuerte/connection.h>
#include <fuerte/requests.h>
#include <fuerte/types.h>
#include <velocypack/Dumper.h>
#include <velocypack/Parser.h>

#include <format>

using namespace arangodb;

namespace {

auto normalizeJsonOption = []() {
  auto options = velocypack::Options{};
  options.prettyPrint = true;
  return options;
}();
auto normalizeJson(std::string_view json) -> std::string {
  auto builder = velocypack::Parser::fromJson(json);
  return velocypack::Dumper::toString(builder->slice(), &normalizeJsonOption);
}
auto normalizeJson(velocypack::Slice slice) -> std::string {
  return velocypack::Dumper::toString(slice, &normalizeJsonOption);
}

constexpr auto kContainerName = "rbac-integration-smocker";
constexpr auto kSmockerMockUrl = "http://localhost:8080";
constexpr auto kSmockerAdminUrl = "http://localhost:8081";

constexpr auto kEvaluateTokenManyPath =
    "/_integration/authorization/v1/evaluate-token-many";

auto buildAllowResponse(std::size_t numItems) -> std::string {
  std::string items;
  for (std::size_t i = 0; i < numItems; ++i) {
    if (i > 0) items += ",";
    items += R"({"effect":"Allow","message":""})";
  }
  return std::format(R"({{"effect":"Allow","message":"","items":[{}]}})",
                     items);
}

auto buildDenyResponse(std::size_t numItems)
    -> std::string {
  std::string items;
  for (std::size_t i = 0; i < numItems; ++i) {
    if (i > 0) items += ",";
    items += R"({"effect":"Deny","message":"access denied"})";
  }
  return std::format(R"({{"effect":"Deny","message":"access denied","items":[{}]}})",
                     items);
}

auto buildMixedResponse(std::initializer_list<std::string_view> effects)
    -> std::string {
  std::string items;
  bool anyDeny = false;
  bool first = true;
  for (auto effect : effects) {
    if (!first) items += ",";
    first = false;
    items += std::format(R"({{"effect":"{}","message":""}})", effect);
    if (effect == "Deny") anyDeny = true;
  }
  auto overallEffect = anyDeny ? "Deny" : "Allow";
  return std::format(R"({{"effect":"{}","message":"","items":[{}]}})",
                     overallEffect, items);
}

}  // namespace

struct RbacIntegrationTest : ::testing::Test {
  static inline std::unique_ptr<test::SmockerClient> _smocker;

  static void SetUpTestSuite() {
    _smocker = std::make_unique<test::SmockerClient>(
        kContainerName, kSmockerMockUrl, kSmockerAdminUrl);
    _smocker->start();
  }

  static void TearDownTestSuite() {
    if (_smocker) {
      _smocker->stop();
    }
    _smocker.reset();
  }

  void SetUp() override {
    // Note that a failure / an exception on SetUpTestSuite() will cause all
    // tests to be *skipped*; so we need to check for errors and fail here
    // instead.
    ASSERT_TRUE(!_smocker->startError()) << *_smocker->startError();
    _smocker->resetMocks();

    auto config = network::ConnectionPool::Config();
    config.metrics = network::ConnectionPool::Metrics::createStub(
        "RbacIntegrationTest test");
    _pool = std::make_unique<network::ConnectionPool>(config);
  }

  // -- factory helpers --------------------------------------------------------

  static auto makeSender(network::ConnectionPool& pool) -> network::Sender {
    return [&pool](network::DestinationId const& dest, fuerte::RestVerb verb,
                   std::string const& path, velocypack::Buffer<uint8_t> payload,
                   network::RequestOptions const&,
                   network::Headers) -> network::FutureRes {
      bool isFromPool = false;
      auto conn = pool.leaseConnection(dest, isFromPool);
      auto req = fuerte::createRequest(verb, path);
      req->header.contentType(fuerte::ContentType::Json);
      req->payloadForModification() = std::move(payload);
      auto fuerteRes = conn->sendRequest(std::move(req));
      auto stubReq = std::make_unique<fuerte::Request>();
      return network::Response{std::string{dest}, fuerte::Error::NoError,
                               std::move(stubReq), std::move(fuerteRes)};
    };
  }

  auto makeBackend() -> std::unique_ptr<rbac::BackendImpl> {
    return std::make_unique<rbac::BackendImpl>(makeSender(*_pool),
                                               kSmockerMockUrl);
  }

  auto makeService() -> std::unique_ptr<rbac::ServiceImpl> {
    return std::make_unique<rbac::ServiceImpl>(makeBackend());
  }

  std::unique_ptr<network::ConnectionPool> _pool;
};

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

// =============================================================================
// maySync
// =============================================================================

TEST_F(RbacIntegrationTest, ServiceMaySync_Allow) {
  _smocker->addMock(kEvaluateTokenManyPath, 200, buildAllowResponse(1));
  auto service = makeService();

  auto result =
      service->maySync(rbac::Service::User{.jwtToken = "test.jwt.token"},
                       rbac::Service::Permission::Read,
                       rbac::Service::Category::Database{.name = "mydb"});

  ASSERT_TRUE(result.ok()) << result.result().errorMessage();
  EXPECT_TRUE(result.get());

  auto history = _smocker->getHistory();
  ASSERT_EQ(history.size(), 1u);
  EXPECT_EQ(history[0].method, "POST");
  EXPECT_EQ(history[0].path, kEvaluateTokenManyPath);
  EXPECT_EQ(normalizeJson(history[0].body), normalizeJson(R"({
    "token": "test.jwt.token",
    "items": [{
      "action": "db:ReadDatabase",
      "resource": "db:database:mydb",
      "context": {"parameters": {"attribute": {"values": []}}}
    }]
  })"));
}

TEST_F(RbacIntegrationTest, ServiceMaySync_Deny) {
  _smocker->addMock(kEvaluateTokenManyPath, 200, buildDenyResponse(1));
  auto service = makeService();

  auto result =
      service->maySync(rbac::Service::User{.jwtToken = "test.jwt.token"},
                       rbac::Service::Permission::Read,
                       rbac::Service::Category::Database{.name = "mydb"});

  ASSERT_TRUE(result.ok()) << result.result().errorMessage();
  EXPECT_FALSE(result.get());
}

TEST_F(RbacIntegrationTest, ServiceMaySync_HttpServerError) {
  _smocker->addMock(kEvaluateTokenManyPath, 500, R"({"error":"internal"})");
  auto service = makeService();

  auto result =
      service->maySync(rbac::Service::User{.jwtToken = "test.jwt.token"},
                       rbac::Service::Permission::Read,
                       rbac::Service::Category::Database{.name = "mydb"});

  EXPECT_FALSE(result.ok());
}

TEST_F(RbacIntegrationTest, ServiceMaySync_MalformedResponse) {
  _smocker->addMock(kEvaluateTokenManyPath, 200, "invalid json {{{");
  auto service = makeService();

  auto result =
      service->maySync(rbac::Service::User{.jwtToken = "test.jwt.token"},
                       rbac::Service::Permission::Read,
                       rbac::Service::Category::Database{.name = "mydb"});

  EXPECT_FALSE(result.ok());
}

// =============================================================================
// mayAllSync
// =============================================================================

TEST_F(RbacIntegrationTest, ServiceMayAllSync_AllAllow) {
  _smocker->addMock(kEvaluateTokenManyPath, 200, buildAllowResponse(3));
  auto service = makeService();

  auto result = service->mayAllSync(
      rbac::Service::User{.jwtToken = "test.jwt.token"},
      {
          {rbac::Service::Permission::Read,
           rbac::Service::Category::Database{.name = "db1"}},
          {rbac::Service::Permission::Write,
           rbac::Service::Category::Database{.name = "db2"}},
          {rbac::Service::Permission::Read,
           rbac::Service::Category::Database{.name = "db3"}},
      });

  ASSERT_TRUE(result.ok()) << result.result().errorMessage();
  EXPECT_TRUE(result.get());

  auto history = _smocker->getHistory();
  ASSERT_EQ(history.size(), 1u);
  EXPECT_EQ(history[0].method, "POST");
  EXPECT_EQ(history[0].path, kEvaluateTokenManyPath);
  EXPECT_EQ(normalizeJson(history[0].body), normalizeJson(R"({
    "token": "test.jwt.token",
    "items": [
      {"action": "db:ReadDatabase", "resource": "db:database:db1",
       "context": {"parameters": {"attribute": {"values": []}}}},
      {"action": "db:WriteDatabase", "resource": "db:database:db2",
       "context": {"parameters": {"attribute": {"values": []}}}},
      {"action": "db:ReadDatabase", "resource": "db:database:db3",
       "context": {"parameters": {"attribute": {"values": []}}}}
    ]
  })"));
}

TEST_F(RbacIntegrationTest, ServiceMayAllSync_AllDeny) {
  _smocker->addMock(kEvaluateTokenManyPath, 200, buildDenyResponse(3));
  auto service = makeService();

  auto result = service->mayAllSync(
      rbac::Service::User{.jwtToken = "test.jwt.token"},
      {
          {rbac::Service::Permission::Read,
           rbac::Service::Category::Database{.name = "db1"}},
          {rbac::Service::Permission::Write,
           rbac::Service::Category::Database{.name = "db2"}},
          {rbac::Service::Permission::Read,
           rbac::Service::Category::Database{.name = "db3"}},
      });

  ASSERT_TRUE(result.ok()) << result.result().errorMessage();
  EXPECT_FALSE(result.get());
}

TEST_F(RbacIntegrationTest, ServiceMayAllSync_OneDenied) {
  _smocker->addMock(kEvaluateTokenManyPath, 200,
                    buildMixedResponse({"Allow", "Deny", "Allow"}));
  auto service = makeService();

  auto result = service->mayAllSync(
      rbac::Service::User{.jwtToken = "test.jwt.token"},
      {
          {rbac::Service::Permission::Read,
           rbac::Service::Category::Database{.name = "db1"}},
          {rbac::Service::Permission::Write,
           rbac::Service::Category::Database{.name = "db2"}},
          {rbac::Service::Permission::Read,
           rbac::Service::Category::Database{.name = "db3"}},
      });

  ASSERT_TRUE(result.ok()) << result.result().errorMessage();
  EXPECT_FALSE(result.get());
}

TEST_F(RbacIntegrationTest, ServiceMayAllSync_HttpServerError) {
  _smocker->addMock(kEvaluateTokenManyPath, 500, R"({"error":"internal"})");
  auto service = makeService();

  auto result = service->mayAllSync(
      rbac::Service::User{.jwtToken = "test.jwt.token"},
      {
          {rbac::Service::Permission::Read,
           rbac::Service::Category::Database{.name = "db1"}},
          {rbac::Service::Permission::Write,
           rbac::Service::Category::Database{.name = "db2"}},
          {rbac::Service::Permission::Read,
           rbac::Service::Category::Database{.name = "db3"}},
      });

  EXPECT_FALSE(result.ok());
}

#pragma GCC diagnostic pop
