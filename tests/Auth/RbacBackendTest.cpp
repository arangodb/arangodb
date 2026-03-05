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
#include "Basics/StaticStrings.h"
#include "Inspection/JsonPrintInspector.h"
#include "Network/Methods.h"

#include <fuerte/types.h>
#include <velocypack/Buffer.h>
#include <velocypack/Dumper.h>
#include <velocypack/Parser.h>

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

}  // namespace

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

TEST(RbacBackendTest, evaluateMany_sendsCorrectRequestAndParsesResponse) {
  auto responseJson = buildAllowResponseJson({rbac::Backend::Effect::Allow});

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
                    "roles": ["admin"],
                    "items": [{
                      "action": "db:ReadDatabase",
                      "resource": "db:database:mydata",
                      "context": {
                        "parameters": { "attribute": { "values": [] } }
                      }
                    }]
                  })"));
    return makeNetworkResponse(fuerte::StatusOK, responseJson);
  };

  auto testee = rbac::BackendImpl{sendRequestMock, "http://localhost:8080"};

  auto result =
      testee
          .evaluateMany(
              rbac::Backend::PlainUser{.username = "alice", .roles = {"admin"}},
              rbac::Backend::RequestItems{.items = {rbac::Backend::RequestItem{
                                              .action = "db:ReadDatabase",
                                              .resource = "db:database:mydata",
                                              .attributeValues = {}}}})
          .waitAndGet();

  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result.get().effect, rbac::Backend::Effect::Allow);
  ASSERT_EQ(result.get().items.size(), 1u);
  EXPECT_EQ(result.get().items[0].effect, rbac::Backend::Effect::Allow);
}

TEST(RbacBackendTest, evaluateMany_returnsErrorOnNonOkHttpStatus) {
  auto sendRequestMock =
      [](network::DestinationId const&, fuerte::RestVerb, std::string const&,
         velocypack::Buffer<uint8_t> const&, network::RequestOptions const&,
         network::Headers const&) -> network::FutureRes {
    return makeNetworkResponse(fuerte::StatusForbidden);
  };
  auto testee =
      rbac::BackendImpl{std::move(sendRequestMock), "http://localhost:8080"};

  auto result = testee
                    .evaluateMany(rbac::Backend::PlainUser{.username = "alice",
                                                           .roles = {}},
                                  rbac::Backend::RequestItems{})
                    .waitAndGet();

  EXPECT_FALSE(result.ok());
}

TEST(RbacBackendTest, evaluateTokenMany_sendsCorrectRequestAndParsesResponse) {
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
                    .evaluateTokenMany(
                        rbac::Backend::JwtToken{.jwtToken = "my.jwt.token"},
                        rbac::Backend::RequestItems{})
                    .waitAndGet();

  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result.get().effect, rbac::Backend::Effect::Allow);
  EXPECT_TRUE(result.get().items.empty());
}

TEST(RbacBackendTest, evaluateTokenMany_returnsErrorOnNonOkHttpStatus) {
  auto sendRequestMock =
      [](network::DestinationId const&, fuerte::RestVerb, std::string const&,
         velocypack::Buffer<uint8_t> const&, network::RequestOptions const&,
         network::Headers const&) -> network::FutureRes {
    return makeNetworkResponse(fuerte::StatusUnauthorized);
  };
  auto testee =
      rbac::BackendImpl{std::move(sendRequestMock), "http://localhost:8080"};

  auto result =
      testee
          .evaluateTokenMany(rbac::Backend::JwtToken{.jwtToken = "bad.token"},
                             rbac::Backend::RequestItems{})
          .waitAndGet();

  EXPECT_FALSE(result.ok());
}
