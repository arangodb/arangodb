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

#include "Utils/RenewingJwtToken.h"

#include "Basics/voc-errors.h"
#include "SimpleHttpClient/SimpleHttpResult.h"

#include <absl/strings/escaping.h>
#include <absl/strings/str_cat.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <thread>

using namespace arangodb;
using namespace std::chrono_literals;

namespace {

constexpr auto threshold = 300s;

/**
 * Time point the given number of seconds after the epoch
 */
auto at(std::int64_t seconds) -> JwtClock::time_point {
  return JwtClock::time_point{std::chrono::seconds{seconds}};
}

/**
 * A JWT whose body carries only the given "exp" claim
 */
auto tokenExpiringAt(std::uint64_t expiresAt) -> JwtToken {
  return absl::StrCat(
      "header.",
      absl::WebSafeBase64Escape(absl::StrCat(R"({"exp":)", expiresAt, "}")),
      ".signature");
}

/**
 * A complete HTTP response with the given status and JSON body
 */
auto completeResponse(int httpStatus, std::string_view body)
    -> std::unique_ptr<httpclient::SimpleHttpResult> {
  auto response = std::make_unique<httpclient::SimpleHttpResult>();
  response->setResultType(httpclient::SimpleHttpResult::ResultType::COMPLETE);
  response->setHttpReturnCode(httpStatus);
  response->getBody().appendText(body);
  return response;
}

/**
 * Renewer that always answers with the same outcome and records its calls
 */
struct ScriptedRenewer {
  TokenOutcome outcome;
  std::atomic<int> calls{0};
  JwtToken lastToken;

  auto asFunction() -> RenewingJwtToken::Renewer {
    return [this](JwtToken const& currentToken) {
      lastToken = currentToken;
      ++calls;
      return outcome;
    };
  }
};

auto renewedToken() -> TokenOutcome {
  return TokenOutcome::success(std::optional{tokenExpiringAt(2000)});
}

}  // namespace

// tokenStateFor

TEST(RenewingJwtTokenTest, tokenStateForTakesExpiryFromToken) {
  auto const state = tokenStateFor(tokenExpiringAt(1000), at(100));

  EXPECT_EQ(state.token, tokenExpiringAt(1000));
  EXPECT_EQ(state.obtainedAt, at(100));
  EXPECT_EQ(state.expiresAt, at(1000));
  EXPECT_EQ(state.nextAttemptAt, JwtClock::time_point{});
}

TEST(RenewingJwtTokenTest, tokenStateForTokenWithoutExpiryNeverExpires) {
  auto const state = tokenStateFor("not-a-jwt", at(100));

  EXPECT_EQ(state.expiresAt, std::nullopt);
}

// isRenewalDue

TEST(RenewingJwtTokenTest, renewalIsNeverDueWithoutExpiry) {
  auto const state = JwtTokenState{
      .token = "t", .obtainedAt = at(0), .expiresAt = std::nullopt};

  EXPECT_FALSE(isRenewalDue(state, at(1'000'000'000), threshold));
}

TEST(RenewingJwtTokenTest, renewalIsDueOnceWithinThresholdOfExpiry) {
  auto const state =
      JwtTokenState{.token = "t", .obtainedAt = at(0), .expiresAt = at(1000)};

  EXPECT_FALSE(isRenewalDue(state, at(699), threshold));
  EXPECT_TRUE(isRenewalDue(state, at(700), threshold));
  EXPECT_TRUE(isRenewalDue(state, at(1001), threshold));
}

TEST(RenewingJwtTokenTest,
     renewalWaitsForHalfLifetimeWhenThresholdExceedsLifetime) {
  auto const state =
      JwtTokenState{.token = "t", .obtainedAt = at(990), .expiresAt = at(1000)};

  EXPECT_FALSE(isRenewalDue(state, at(994), threshold));
  EXPECT_TRUE(isRenewalDue(state, at(995), threshold));
}

TEST(RenewingJwtTokenTest, renewalIsNotDueBeforeNextAttempt) {
  auto const state = JwtTokenState{.token = "t",
                                   .obtainedAt = at(0),
                                   .expiresAt = at(1000),
                                   .nextAttemptAt = at(710)};

  EXPECT_FALSE(isRenewalDue(state, at(705), threshold));
  EXPECT_TRUE(isRenewalDue(state, at(710), threshold));
}

// applyRenewal

TEST(RenewingJwtTokenTest, newTokenReplacesTokenAndExpiry) {
  auto const before = JwtTokenState{.token = tokenExpiringAt(1000),
                                    .obtainedAt = at(0),
                                    .expiresAt = at(1000),
                                    .nextAttemptAt = at(705)};

  auto const after = applyRenewal(before, renewedToken(), at(700));

  EXPECT_EQ(after.token, tokenExpiringAt(2000));
  EXPECT_EQ(after.obtainedAt, at(700));
  EXPECT_EQ(after.expiresAt, at(2000));
  EXPECT_EQ(after.nextAttemptAt, JwtClock::time_point{});
}

TEST(RenewingJwtTokenTest, unwillingServerKeepsTokenAndSchedulesRetry) {
  auto const before =
      JwtTokenState{.token = "t", .obtainedAt = at(0), .expiresAt = at(1000)};

  auto const after =
      applyRenewal(before, TokenOutcome::success(std::nullopt), at(700));

  EXPECT_EQ(after.token, "t");
  EXPECT_EQ(after.obtainedAt, at(0));
  EXPECT_EQ(after.expiresAt, at(1000));
  EXPECT_EQ(after.nextAttemptAt, at(705));
}

TEST(RenewingJwtTokenTest, failedRenewalKeepsTokenAndSchedulesRetry) {
  auto const before =
      JwtTokenState{.token = "t", .obtainedAt = at(0), .expiresAt = at(1000)};

  auto const after = applyRenewal(
      before,
      TokenOutcome::error(TRI_ERROR_FORBIDDEN, "User not authenticated"),
      at(700));

  EXPECT_EQ(after.token, "t");
  EXPECT_EQ(after.obtainedAt, at(0));
  EXPECT_EQ(after.expiresAt, at(1000));
  EXPECT_EQ(after.nextAttemptAt, at(705));
}

// renewOrLogin

namespace {

/**
 * Token request that always answers with the same outcome and counts its
 * calls
 */
struct ScriptedRequest {
  TokenOutcome outcome;
  int calls = 0;

  auto asFunction() -> std::function<TokenOutcome()> {
    return [this] {
      ++calls;
      return outcome;
    };
  }
};

auto validUntil1000() -> JwtTokenState {
  return JwtTokenState{
      .token = "t", .obtainedAt = at(0), .expiresAt = at(1000)};
}

auto loggedInToken() -> TokenOutcome {
  return TokenOutcome::success(std::optional{tokenExpiringAt(3000)});
}

}  // namespace

TEST(RenewingJwtTokenTest, renewOrLoginRenewsAValidTokenWithoutLoggingIn) {
  auto renew = ScriptedRequest{.outcome = renewedToken()};
  auto login = ScriptedRequest{.outcome = loggedInToken()};

  auto const outcome = renewOrLogin(validUntil1000(), at(700),
                                    renew.asFunction(), login.asFunction());

  ASSERT_TRUE(outcome.ok());
  EXPECT_EQ(outcome.get(), renewedToken().get());
  EXPECT_EQ(renew.calls, 1);
  EXPECT_EQ(login.calls, 0);
}

TEST(RenewingJwtTokenTest, renewOrLoginAcceptsThatNoTokenWasIssuedYet) {
  auto renew = ScriptedRequest{.outcome = TokenOutcome::success(std::nullopt)};
  auto login = ScriptedRequest{.outcome = loggedInToken()};

  auto const outcome = renewOrLogin(validUntil1000(), at(700),
                                    renew.asFunction(), login.asFunction());

  ASSERT_TRUE(outcome.ok());
  EXPECT_EQ(outcome.get(), std::nullopt);
  EXPECT_EQ(login.calls, 0);
}

TEST(RenewingJwtTokenTest, renewOrLoginLogsInWhenTheRenewalIsRefused) {
  auto renew = ScriptedRequest{
      .outcome = TokenOutcome::error(TRI_ERROR_HTTP_UNAUTHORIZED, "refused")};
  auto login = ScriptedRequest{.outcome = loggedInToken()};

  auto const outcome = renewOrLogin(validUntil1000(), at(700),
                                    renew.asFunction(), login.asFunction());

  ASSERT_TRUE(outcome.ok());
  EXPECT_EQ(outcome.get(), loggedInToken().get());
  EXPECT_EQ(renew.calls, 1);
  EXPECT_EQ(login.calls, 1);
}

TEST(RenewingJwtTokenTest, renewOrLoginKeepsTransientRenewalErrors) {
  // the token is still valid, so sending the password would not help
  for (auto const error : {TRI_ERROR_SIMPLE_CLIENT_COULD_NOT_CONNECT,
                           TRI_ERROR_FAILED, ErrorCode{503}}) {
    auto renew =
        ScriptedRequest{.outcome = TokenOutcome::error(error, "unavailable")};
    auto login = ScriptedRequest{.outcome = loggedInToken()};

    auto const outcome = renewOrLogin(validUntil1000(), at(700),
                                      renew.asFunction(), login.asFunction());

    ASSERT_TRUE(outcome.fail());
    EXPECT_EQ(outcome.errorNumber(), error);
    EXPECT_EQ(login.calls, 0);
  }
}

TEST(RenewingJwtTokenTest, renewOrLoginLogsInWithoutRenewingAnExpiredToken) {
  for (auto const now : {at(1000), at(1500)}) {
    auto renew = ScriptedRequest{.outcome = renewedToken()};
    auto login = ScriptedRequest{.outcome = loggedInToken()};

    auto const outcome = renewOrLogin(validUntil1000(), now, renew.asFunction(),
                                      login.asFunction());

    ASSERT_TRUE(outcome.ok());
    EXPECT_EQ(outcome.get(), loggedInToken().get());
    EXPECT_EQ(renew.calls, 0);
    EXPECT_EQ(login.calls, 1);
  }
}

// parseTokenResponse

TEST(RenewingJwtTokenTest, parsesRenewedTokenFromResponse) {
  auto const outcome = parseTokenResponse(
      TokenRequest::kRenew, *completeResponse(200, R"({"jwt":"renewed"})"));

  ASSERT_TRUE(outcome.ok());
  EXPECT_EQ(outcome.get(), std::optional<JwtToken>{"renewed"});
}

TEST(RenewingJwtTokenTest, parsesEmptyResponseAsNoTokenIssued) {
  auto const outcome =
      parseTokenResponse(TokenRequest::kRenew, *completeResponse(200, "{}"));

  ASSERT_TRUE(outcome.ok());
  EXPECT_EQ(outcome.get(), std::nullopt);
}

TEST(RenewingJwtTokenTest, parsesInvalidTokenAsNoTokenIssued) {
  // the server answers with the token "invalid" when authentication is off
  auto const outcome = parseTokenResponse(
      TokenRequest::kRenew, *completeResponse(200, R"({"jwt":"invalid"})"));

  ASSERT_TRUE(outcome.ok());
  EXPECT_EQ(outcome.get(), std::nullopt);
}

TEST(RenewingJwtTokenTest, parsesHttpErrorIntoErrorNumberAndMessage) {
  auto const outcome = parseTokenResponse(
      TokenRequest::kLogin,
      *completeResponse(
          401,
          R"({"error":true,"errorNum":11,"errorMessage":"User not authenticated","code":401})"));

  ASSERT_TRUE(outcome.fail());
  EXPECT_EQ(outcome.errorNumber(), TRI_ERROR_FORBIDDEN);
  EXPECT_NE(outcome.errorMessage().find("User not authenticated"),
            std::string::npos);
}

TEST(RenewingJwtTokenTest, rejectsUnexpectedResponseBodies) {
  EXPECT_TRUE(
      parseTokenResponse(TokenRequest::kRenew, *completeResponse(200, "[]"))
          .fail());
  EXPECT_TRUE(parseTokenResponse(TokenRequest::kRenew,
                                 *completeResponse(200, R"({"jwt":1})"))
                  .fail());
  EXPECT_TRUE(parseTokenResponse(TokenRequest::kRenew,
                                 *completeResponse(200, "not json"))
                  .fail());
}

TEST(RenewingJwtTokenTest, parsesUnimplementedLoginEndpointAsNoTokenIssued) {
  // servers without /_open/auth answer with 404 or 501
  for (auto const status : {404, 501}) {
    auto const outcome = parseTokenResponse(
        TokenRequest::kLogin,
        *completeResponse(status, R"({"error":true,"errorNum":1,"code":404})"));

    ASSERT_TRUE(outcome.ok()) << status;
    EXPECT_EQ(outcome.get(), std::nullopt) << status;
  }
}

TEST(RenewingJwtTokenTest, parsesRefusedRenewalAsUnauthorized) {
  // /_open/auth/renew answers 404 to a token it does not accept, such as an
  // expired one; 401 and 403 refuse it as well
  for (auto const status : {401, 403, 404}) {
    auto const outcome = parseTokenResponse(
        TokenRequest::kRenew,
        *completeResponse(
            status,
            R"({"error":true,"errorNum":1703,"errorMessage":"user not found"})"));

    ASSERT_TRUE(outcome.fail()) << status;
    EXPECT_EQ(outcome.errorNumber(), TRI_ERROR_HTTP_UNAUTHORIZED) << status;
    EXPECT_NE(outcome.errorMessage().find("user not found"), std::string::npos)
        << status;
  }
}

TEST(RenewingJwtTokenTest, parsesHttpErrorWithoutErrorBodyIntoStatus) {
  auto const outcome =
      parseTokenResponse(TokenRequest::kRenew, *completeResponse(503, "busy"));

  ASSERT_TRUE(outcome.fail());
  EXPECT_EQ(outcome.errorNumber(), ErrorCode{503});
  EXPECT_NE(outcome.errorMessage().find("HTTP 503"), std::string::npos);
}

TEST(RenewingJwtTokenTest, parseTokenReplyRejectsMissingBody) {
  EXPECT_TRUE(
      parseTokenReply(TokenRequest::kRenew, 200, velocypack::Slice{}).fail());
}

TEST(RenewingJwtTokenTest, parsesEmptyTokenAsNotTokenIssued) {
  // the server answers with the token "invalid" when authentication is off
  auto const outcome = parseTokenResponse(
      TokenRequest::kRenew, *completeResponse(200, R"({"jwt":""})"));

  ASSERT_TRUE(outcome.ok());
  EXPECT_EQ(outcome.get(), std::nullopt);
}

// RenewingJwtToken

TEST(RenewingJwtTokenTest, currentReturnsTheTokenWithoutRenewingIt) {
  auto now = at(0);
  auto renewer = ScriptedRenewer{.outcome = renewedToken()};
  auto token = RenewingJwtToken{tokenExpiringAt(1000), renewer.asFunction(),
                                threshold, [&now] { return now; }};

  now = at(700);

  EXPECT_EQ(token.current(), tokenExpiringAt(1000));
  EXPECT_EQ(renewer.calls.load(), 0);
}

TEST(RenewingJwtTokenTest, renewIfDueDoesNothingBeforeTheRenewalPoint) {
  auto now = at(0);
  auto renewer = ScriptedRenewer{.outcome = renewedToken()};
  auto token = RenewingJwtToken{tokenExpiringAt(1000), renewer.asFunction(),
                                threshold, [&now] { return now; }};

  now = at(699);
  token.renewIfDue();

  EXPECT_EQ(renewer.calls.load(), 0);
  EXPECT_EQ(token.current(), tokenExpiringAt(1000));
}

TEST(RenewingJwtTokenTest, renewIfDueRenewsWhenDue) {
  auto now = at(0);
  auto renewer = ScriptedRenewer{.outcome = renewedToken()};
  auto token = RenewingJwtToken{tokenExpiringAt(1000), renewer.asFunction(),
                                threshold, [&now] { return now; }};

  now = at(700);
  token.renewIfDue();
  EXPECT_EQ(renewer.calls.load(), 1);
  EXPECT_EQ(renewer.lastToken, tokenExpiringAt(1000));
  EXPECT_EQ(token.current(), tokenExpiringAt(2000));

  // the renewed token expires at 2000, so its renewal point is 1700
  now = at(1699);
  token.renewIfDue();
  EXPECT_EQ(renewer.calls.load(), 1);

  now = at(1700);
  token.renewIfDue();
  EXPECT_EQ(renewer.calls.load(), 2);
}

TEST(RenewingJwtTokenTest, renewIfDueRetriesLaterWhenNoTokenWasIssued) {
  auto now = at(0);
  auto renewer =
      ScriptedRenewer{.outcome = TokenOutcome::success(std::nullopt)};
  auto token = RenewingJwtToken{tokenExpiringAt(1000), renewer.asFunction(),
                                threshold, [&now] { return now; }};

  now = at(700);
  token.renewIfDue();
  EXPECT_EQ(renewer.calls.load(), 1);
  EXPECT_EQ(token.current(), tokenExpiringAt(1000));

  now = at(704);
  token.renewIfDue();
  EXPECT_EQ(renewer.calls.load(), 1);

  now = at(705);
  token.renewIfDue();
  EXPECT_EQ(renewer.calls.load(), 2);
  EXPECT_EQ(token.current(), tokenExpiringAt(1000));
}

TEST(RenewingJwtTokenTest, renewIfDueRetriesAfterTheFailedAttemptCompleted) {
  // a renewal that fails only after its retry interval passed must not be
  // repeated right away
  auto now = at(0);
  auto calls = 0;
  auto token = RenewingJwtToken{tokenExpiringAt(1000),
                                [&](JwtToken const&) {
                                  ++calls;
                                  now += 10s;
                                  return TokenOutcome::error(
                                      TRI_ERROR_SIMPLE_CLIENT_COULD_NOT_CONNECT,
                                      "timeout");
                                },
                                threshold, [&now] { return now; }};

  now = at(700);
  token.renewIfDue();
  EXPECT_EQ(calls, 1);

  now = at(714);
  token.renewIfDue();
  EXPECT_EQ(calls, 1);

  now = at(715);
  token.renewIfDue();
  EXPECT_EQ(calls, 2);
}

TEST(RenewingJwtTokenTest, currentDoesNotWaitForARenewalInProgress) {
  auto now = at(0);
  std::atomic<bool> renewalStarted{false};
  std::atomic<bool> renewalMayFinish{false};
  auto token =
      RenewingJwtToken{tokenExpiringAt(1000),
                       [&renewalStarted, &renewalMayFinish](JwtToken const&) {
                         renewalStarted = true;
                         while (!renewalMayFinish) {
                           std::this_thread::sleep_for(1ms);
                         }
                         return renewedToken();
                       },
                       threshold, [&now] { return now; }};

  now = at(700);
  auto renewal = std::jthread{[&token] { token.renewIfDue(); }};
  while (!renewalStarted) {
    std::this_thread::sleep_for(1ms);
  }
  auto releaser = std::jthread{[&renewalMayFinish] {
    std::this_thread::sleep_for(200ms);
    renewalMayFinish = true;
  }};

  // had current() waited for the renewal, it would return the renewed token
  EXPECT_EQ(token.current(), tokenExpiringAt(1000));

  renewal.join();
  EXPECT_EQ(token.current(), tokenExpiringAt(2000));
}

TEST(RenewingJwtTokenTest, changedThresholdAppliesToTheNextRenewalCheck) {
  auto now = at(0);
  auto renewer = ScriptedRenewer{.outcome = renewedToken()};
  auto token = RenewingJwtToken{tokenExpiringAt(1000), renewer.asFunction(),
                                threshold, [&now] { return now; }};

  // with a threshold of 300 the renewal point is 700
  now = at(600);
  token.renewIfDue();
  EXPECT_EQ(renewer.calls.load(), 0);

  token.setRenewalThreshold(400s);
  token.renewIfDue();
  EXPECT_EQ(renewer.calls.load(), 1);
  EXPECT_EQ(token.current(), tokenExpiringAt(2000));
}
