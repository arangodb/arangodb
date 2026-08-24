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
#include "Basics/StaticStrings.h"
#include "Inspection/JsonPrintInspector.h"
#include "Metrics/Histogram.h"
#include "Metrics/LogScale.h"
#include "Network/Methods.h"

#include <fuerte/types.h>
#include <velocypack/Buffer.h>
#include <velocypack/Dumper.h>
#include <velocypack/Parser.h>

#include <numeric>
#include <sstream>

using namespace arangodb;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

namespace {

template<typename T>
std::string toJson(T& value) {
  std::ostringstream stream;
  inspection::JsonPrintInspector<> inspector{
      stream, inspection::JsonPrintFormat::kMinimal};
  auto res = inspector.apply(value);
  EXPECT_TRUE(res.ok()) << (res.ok() ? "" : res.error());
  return stream.str();
}

std::string normalizeJson(std::string_view json) {
  auto builder = velocypack::Parser::fromJson(json);
  return velocypack::Dumper::toString(builder->slice());
}

std::string payloadToString(velocypack::Buffer<uint8_t> const& buf) {
  return {reinterpret_cast<char const*>(buf.data()), buf.size()};
}

std::string buildAllowResponseJson(
    std::vector<rbac::Backend::Effect> itemEffects = {}) {
  rbac::Backend::EvaluateResponseMany resp{};
  resp.effect = rbac::Backend::Effect::Allow;
  resp.message = "";
  for (auto eff : itemEffects) {
    resp.items.push_back(
        rbac::Backend::ResponseItem{.effect = eff, .message = ""});
  }
  return toJson(resp);
}

network::Response makeNetworkResponse(fuerte::StatusCode statusCode,
                                      std::string_view jsonBody = {}) {
  auto response = std::make_unique<fuerte::Response>();
  response->header.responseCode = statusCode;
  response->header.contentType(fuerte::ContentType::Json);
  if (!jsonBody.empty()) {
    velocypack::Buffer<uint8_t> buf;
    buf.append(reinterpret_cast<uint8_t const*>(jsonBody.data()),
               jsonBody.size());
    response->setPayload(std::move(buf), 0);
  }
  // create a stub request, because we have to pass a non-nullptr request.
  // currently, there's no need to make it a "correct" request.
  auto request = std::make_unique<fuerte::Request>();
  return network::Response{std::string{}, fuerte::Error::NoError,
                           std::move(request), std::move(response)};
}

auto makeRequestDurationMetric() -> rbac::BackendImpl::RequestDurationMetric {
  // the exact scale doesn't matter here, only the number of recorded
  // observations
  using scale_t = metrics::LogScale<std::uint64_t>;
  return {scale_t{scale_t::kSupplySmallestBucket, 2, 0, 10, 22},
          "arangodb_rbac_request_duration", "", ""};
}

auto totalObservations(rbac::BackendImpl::RequestDurationMetric const& metric)
    -> std::uint64_t {
  auto buckets = metric.load();
  return std::accumulate(buckets.begin(), buckets.end(), std::uint64_t{0});
}

}  // namespace

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

TEST(RbacBackendTest,
     evaluateMany_withToken_sendsCorrectRequestAndParsesResponse) {
  auto responseJson = buildAllowResponseJson();

  auto sendRequestMock = [&](network::DestinationId const& dest,
                             fuerte::RestVerb verb, std::string const& path,
                             velocypack::Buffer<uint8_t> const& payload,
                             network::RequestOptions const& opts,
                             network::Headers const&) -> network::FutureRes {
    EXPECT_EQ(dest, "http://localhost:8080");
    EXPECT_EQ(verb, fuerte::RestVerb::Post);
    EXPECT_EQ(path, "/_integration/authorization/v1/evaluate-token-many");
    EXPECT_EQ(opts.contentType, "application/json; charset=utf-8");
    EXPECT_EQ(opts.acceptType, "application/json; charset=utf-8");
    EXPECT_EQ(normalizeJson(payloadToString(payload)), normalizeJson(R"({
                    "token": "my.jwt.token",
                    "items": []
                  })"));
    return makeNetworkResponse(fuerte::StatusOK, responseJson);
  };
  auto testee = rbac::BackendImpl{sendRequestMock, "http://localhost:8080"};

  auto result = testee
                    .evaluateMany(rbac::JwtToken{.jwtToken = "my.jwt.token"},
                                  rbac::Backend::RequestItems{})
                    .waitAndGet();

  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result.get().effect, rbac::Backend::Effect::Allow);
  EXPECT_TRUE(result.get().items.empty());
}

// A caller that authenticated without a JWT (HTTP Basic, or a personal access
// token) is identified by name instead, which goes to the other batch endpoint
// of the AuthorizationV1 API. Only the subject field of the envelope differs.
TEST(RbacBackendTest, evaluateMany_withUsername_sendsToEvaluateManyPath) {
  auto responseJson = buildAllowResponseJson();

  auto sendRequestMock = [&](network::DestinationId const& dest,
                             fuerte::RestVerb verb, std::string const& path,
                             velocypack::Buffer<uint8_t> const& payload,
                             network::RequestOptions const& opts,
                             network::Headers const&) -> network::FutureRes {
    EXPECT_EQ(dest, "http://localhost:8080");
    EXPECT_EQ(verb, fuerte::RestVerb::Post);
    EXPECT_EQ(path, "/_integration/authorization/v1/evaluate-many");
    EXPECT_EQ(opts.contentType, "application/json; charset=utf-8");
    EXPECT_EQ(opts.acceptType, "application/json; charset=utf-8");
    EXPECT_EQ(normalizeJson(payloadToString(payload)), normalizeJson(R"({
                    "user": "alice",
                    "items": []
                  })"));
    return makeNetworkResponse(fuerte::StatusOK, responseJson);
  };
  auto testee = rbac::BackendImpl{sendRequestMock, "http://localhost:8080"};

  auto result = testee
                    .evaluateMany(rbac::Username{.name = "alice"},
                                  rbac::Backend::RequestItems{})
                    .waitAndGet();

  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result.get().effect, rbac::Backend::Effect::Allow);
}

// The items are the same message type on both endpoints, so they must be
// encoded identically no matter how the subject is given.
TEST(RbacBackendTest, evaluateMany_encodesItemsIdenticallyForBothSubjects) {
  auto responseJson = buildAllowResponseJson({rbac::Backend::Effect::Allow});
  auto items = rbac::Backend::RequestItems{
      .items = {rbac::Backend::RequestItem{.action = "db:Read",
                                           .resource = "db:database:mydb",
                                           .attributeValues = {"a", "b"}}}};

  auto capture = [&](std::string& path, std::string& body) {
    return [&](network::DestinationId const&, fuerte::RestVerb,
               std::string const& p, velocypack::Buffer<uint8_t> const& payload,
               network::RequestOptions const&,
               network::Headers const&) -> network::FutureRes {
      path = p;
      body = payloadToString(payload);
      return makeNetworkResponse(fuerte::StatusOK, responseJson);
    };
  };

  std::string tokenPath, tokenBody;
  auto tokenSender = capture(tokenPath, tokenBody);
  auto withToken = rbac::BackendImpl{tokenSender, "http://localhost:8080"};
  ASSERT_TRUE(
      withToken.evaluateManySync(rbac::JwtToken{.jwtToken = "t"}, items).ok());

  std::string userPath, userBody;
  auto userSender = capture(userPath, userBody);
  auto withUser = rbac::BackendImpl{userSender, "http://localhost:8080"};
  ASSERT_TRUE(
      withUser.evaluateManySync(rbac::Username{.name = "alice"}, items).ok());

  EXPECT_EQ(tokenPath, "/_integration/authorization/v1/evaluate-token-many");
  EXPECT_EQ(userPath, "/_integration/authorization/v1/evaluate-many");

  auto expectedItems = normalizeJson(R"([{
    "action": "db:Read",
    "resource": "db:database:mydb",
    "context": {"parameters": {"attribute": {"values": ["a", "b"]}}}
  }])");
  auto itemsOf = [](std::string const& body) {
    auto builder = velocypack::Parser::fromJson(body);
    return velocypack::Dumper::toString(builder->slice().get("items"));
  };
  EXPECT_EQ(itemsOf(tokenBody), expectedItems);
  EXPECT_EQ(itemsOf(userBody), expectedItems);
}

TEST(RbacBackendTest, evaluateMany_returnsErrorOnNonOkHttpStatus) {
  auto sendRequestMock =
      [](network::DestinationId const&, fuerte::RestVerb, std::string const&,
         velocypack::Buffer<uint8_t> const&, network::RequestOptions const&,
         network::Headers const&) -> network::FutureRes {
    return makeNetworkResponse(fuerte::StatusUnauthorized);
  };
  auto testee =
      rbac::BackendImpl{std::move(sendRequestMock), "http://localhost:8080"};

  auto result = testee
                    .evaluateMany(rbac::JwtToken{.jwtToken = "bad.token"},
                                  rbac::Backend::RequestItems{})
                    .waitAndGet();

  EXPECT_FALSE(result.ok());
}

TEST(RbacBackendTest, evaluateManySync_setsSkipSchedulerAndReturnsResult) {
  auto responseJson = buildAllowResponseJson();

  auto sendRequestMock = [&](network::DestinationId const&, fuerte::RestVerb,
                             std::string const&,
                             velocypack::Buffer<uint8_t> const&,
                             network::RequestOptions const& opts,
                             network::Headers const&) -> network::FutureRes {
    EXPECT_TRUE(opts.skipScheduler);
    return makeNetworkResponse(fuerte::StatusOK, responseJson);
  };
  auto testee = rbac::BackendImpl{sendRequestMock, "http://localhost:8080"};

  auto result =
      testee.evaluateManySync(rbac::JwtToken{.jwtToken = "my.jwt.token"},
                              rbac::Backend::RequestItems{});

  EXPECT_TRUE(result.ok());
}

TEST(RbacBackendTest, evaluateMany_measuresRequestDuration) {
  auto responseJson = buildAllowResponseJson();
  auto requestDuration = makeRequestDurationMetric();

  auto sendRequestMock =
      [&](network::DestinationId const&, fuerte::RestVerb, std::string const&,
          velocypack::Buffer<uint8_t> const&, network::RequestOptions const&,
          network::Headers const&) -> network::FutureRes {
    return makeNetworkResponse(fuerte::StatusOK, responseJson);
  };
  auto testee = rbac::BackendImpl{sendRequestMock, "http://localhost:8080",
                                  &requestDuration};

  ASSERT_EQ(totalObservations(requestDuration), 0);

  auto result =
      testee.evaluateManySync(rbac::JwtToken{.jwtToken = "my.jwt.token"},
                              rbac::Backend::RequestItems{});

  ASSERT_TRUE(result.ok());
  EXPECT_EQ(totalObservations(requestDuration), 1);

  auto asyncResult =
      testee
          .evaluateMany(rbac::JwtToken{.jwtToken = "my.jwt.token"},
                        rbac::Backend::RequestItems{})
          .waitAndGet();

  ASSERT_TRUE(asyncResult.ok());
  EXPECT_EQ(totalObservations(requestDuration), 2);
}

TEST(RbacBackendTest, evaluateMany_measuresRequestDurationOnError) {
  auto requestDuration = makeRequestDurationMetric();

  auto sendRequestMock =
      [](network::DestinationId const&, fuerte::RestVerb, std::string const&,
         velocypack::Buffer<uint8_t> const&, network::RequestOptions const&,
         network::Headers const&) -> network::FutureRes {
    return makeNetworkResponse(fuerte::StatusUnauthorized);
  };
  auto testee = rbac::BackendImpl{sendRequestMock, "http://localhost:8080",
                                  &requestDuration};

  auto result = testee.evaluateManySync(rbac::JwtToken{.jwtToken = "bad.token"},
                                        rbac::Backend::RequestItems{});

  ASSERT_FALSE(result.ok());
  EXPECT_EQ(totalObservations(requestDuration), 1);
}
