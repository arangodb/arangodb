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

#include "Auth/Rbac/BackendImpl.h"
#include "Auth/Rbac/ServiceImpl.h"
#include "Auth/SmockerClient.h"
#include "Network/ConnectionPool.h"

#include <fuerte/connection.h>
#include <fuerte/requests.h>
#include <fuerte/types.h>
#include <velocypack/Dumper.h>
#include <velocypack/Parser.h>

#include <cstdlib>
#include <format>
#include <vector>

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

struct SmockerConfig {
  std::string host;
  bool manageDocker;
};

auto getSmockerConfig() -> SmockerConfig {
  if (auto const* env = std::getenv("SMOCKER_HOST")) {
    return {env, false};
  }
  return {"localhost", true};
}

constexpr auto kEvaluateTokenManyPath =
    "/_integration/authorization/v1/evaluate-token-many";
constexpr auto kEvaluateManyPath =
    "/_integration/authorization/v1/evaluate-many";

auto buildAllowResponse(std::size_t numItems) -> std::string {
  std::string items;
  for (std::size_t i = 0; i < numItems; ++i) {
    if (i > 0) items += ",";
    items += R"({"effect":"Allow","message":""})";
  }
  return std::format(R"({{"effect":"Allow","message":"","items":[{}]}})",
                     items);
}

auto buildDenyResponse(std::size_t numItems) -> std::string {
  std::string items;
  for (std::size_t i = 0; i < numItems; ++i) {
    if (i > 0) items += ",";
    items += R"({"effect":"Deny","message":"access denied"})";
  }
  return std::format(
      R"({{"effect":"Deny","message":"access denied","items":[{}]}})", items);
}

}  // namespace

struct RbacIntegrationTest : ::testing::Test {
  static inline std::unique_ptr<test::SmockerClient> _smocker;
  static inline std::string _smockerMockUrl;

  static void SetUpTestSuite() {
    auto [host, manageDocker] = getSmockerConfig();
    _smockerMockUrl = "http://" + host + ":8080";
    auto adminUrl = "http://" + host + ":8081";
    _smocker = std::make_unique<test::SmockerClient>(
        kContainerName, _smockerMockUrl, adminUrl, manageDocker);
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
    ASSERT_TRUE(!_smocker->startError())
        << *_smocker->startError() << "\n\n"
        << "To run these tests, either:\n"
        << "  - Install Docker and ensure it is accessible, or\n"
        << "  - Set SMOCKER_HOST to point to a running Smocker instance.\n"
        << "To skip, use: --gtest_filter=-RbacIntegrationTest.*";
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
                                               _smockerMockUrl);
  }

  auto makeService() -> std::unique_ptr<rbac::ServiceImpl> {
    return std::make_unique<rbac::ServiceImpl>(makeBackend());
  }

  std::unique_ptr<network::ConnectionPool> _pool;
};

// =============================================================================
// check() — the ActionResource path. Verifies the new "db:<Action>" +
// typed-resource wire vocabulary is what actually goes over the wire, and that
// the aggregate effect maps to a Result.
// =============================================================================

TEST_F(RbacIntegrationTest, ServiceCheck_Allow) {
  _smocker->addMock(kEvaluateTokenManyPath, 200, buildAllowResponse(1));
  auto service = makeService();

  std::vector<rbac::ActionResource> queries{
      {rbac::Action::Read, rbac::resources::Database{.name = "mydb"}}};
  auto result = service->check(rbac::JwtToken{"test.jwt.token"}, queries);

  ASSERT_TRUE(result.ok()) << result.errorMessage();

  auto history = _smocker->getHistory();
  ASSERT_EQ(history.size(), 1u);
  EXPECT_EQ(history[0].method, "POST");
  EXPECT_EQ(history[0].path, kEvaluateTokenManyPath);
  EXPECT_EQ(normalizeJson(history[0].body), normalizeJson(R"({
    "token": "test.jwt.token",
    "items": [{
      "action": "db:Read",
      "resource": "db:database:mydb",
      "context": {"parameters": {"attribute": {"values": []}}}
    }]
  })"));
}

TEST_F(RbacIntegrationTest, ServiceCheck_Deny) {
  _smocker->addMock(kEvaluateTokenManyPath, 200, buildDenyResponse(1));
  auto service = makeService();

  std::vector<rbac::ActionResource> queries{
      {rbac::Action::Read, rbac::resources::Database{.name = "mydb"}}};
  auto result = service->check(rbac::JwtToken{"test.jwt.token"}, queries);

  EXPECT_EQ(result.errorNumber(), TRI_ERROR_FORBIDDEN);
}

// A Basic-authenticated caller has no JWT, so it is identified by name and the
// request goes to the other batch endpoint. The items are unchanged; only the
// envelope's subject field differs (COR-907).
TEST_F(RbacIntegrationTest, ServiceCheck_UsernameSubjectUsesEvaluateMany) {
  _smocker->addMock(kEvaluateManyPath, 200, buildAllowResponse(1));
  auto service = makeService();

  std::vector<rbac::ActionResource> queries{
      {rbac::Action::Read, rbac::resources::Database{.name = "mydb"}}};
  auto result = service->check(rbac::Username{"alice"}, queries);

  ASSERT_TRUE(result.ok()) << result.errorMessage();

  auto history = _smocker->getHistory();
  ASSERT_EQ(history.size(), 1u);
  EXPECT_EQ(history[0].method, "POST");
  EXPECT_EQ(history[0].path, kEvaluateManyPath);
  EXPECT_EQ(normalizeJson(history[0].body), normalizeJson(R"({
    "user": "alice",
    "items": [{
      "action": "db:Read",
      "resource": "db:database:mydb",
      "context": {"parameters": {"attribute": {"values": []}}}
    }]
  })"));
}

TEST_F(RbacIntegrationTest, ServiceCheck_UsernameSubjectDeny) {
  _smocker->addMock(kEvaluateManyPath, 200, buildDenyResponse(1));
  auto service = makeService();

  std::vector<rbac::ActionResource> queries{
      {rbac::Action::Read, rbac::resources::Database{.name = "mydb"}}};
  auto result = service->check(rbac::Username{"alice"}, queries);

  EXPECT_EQ(result.errorNumber(), TRI_ERROR_FORBIDDEN);
}

TEST_F(RbacIntegrationTest, ServiceCheck_BatchUsesNewVocabulary) {
  _smocker->addMock(kEvaluateTokenManyPath, 200, buildAllowResponse(3));
  auto service = makeService();

  // A composite permission (create graph + child collections) sent as one
  // batch, exercising the graph/collection resource strings and Create/Read
  // actions of the new vocabulary.
  std::vector<rbac::ActionResource> queries{
      {rbac::Action::Create, rbac::resources::Graph{.db = "mydb", .name = "g"}},
      {rbac::Action::Create,
       rbac::resources::Collection{.db = "mydb", .name = "c1"}},
      {rbac::Action::Read,
       rbac::resources::Collection{.db = "mydb", .name = "c2"}}};
  auto result = service->check(rbac::JwtToken{"test.jwt.token"}, queries);

  ASSERT_TRUE(result.ok()) << result.errorMessage();

  auto history = _smocker->getHistory();
  ASSERT_EQ(history.size(), 1u);
  EXPECT_EQ(normalizeJson(history[0].body), normalizeJson(R"({
    "token": "test.jwt.token",
    "items": [
      {"action": "db:Create", "resource": "db:graph:mydb:g",
       "context": {"parameters": {"attribute": {"values": []}}}},
      {"action": "db:Create", "resource": "db:collection:mydb:c1",
       "context": {"parameters": {"attribute": {"values": []}}}},
      {"action": "db:Read", "resource": "db:collection:mydb:c2",
       "context": {"parameters": {"attribute": {"values": []}}}}
    ]
  })"));
}
