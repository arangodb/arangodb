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

#include "RenewingJwtToken.h"

#include "Basics/StaticStrings.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Rest/CommonDefines.h"
#include "SimpleHttpClient/HttpResponseChecker.h"
#include "SimpleHttpClient/SimpleHttpResult.h"
#include "Ssl/jwt.h"

#include <absl/strings/str_cat.h>
#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

#include <algorithm>
#include <chrono>
#include <exception>
#include <memory>
#include <string_view>
#include <utility>

namespace arangodb {

namespace {

/// the token /_open/auth answers with while authentication is disabled
constexpr std::string_view kAuthDisabledToken = "invalid";

/**
 * Expiry of a token as a time point, or nullopt if it never expires
 */
auto expiryOf(JwtToken const& token) -> std::optional<JwtClock::time_point> {
  auto const secondsSinceEpoch =
      rest::SslInterface::jwt::extractExpiration(token);
  if (!secondsSinceEpoch.has_value()) {
    return std::nullopt;
  }
  return JwtClock::time_point{std::chrono::duration_cast<JwtClock::duration>(
      std::chrono::duration<double>{*secondsSinceEpoch})};
}

/**
 * Reports the outcome of a renewal attempt
 */
void logRenewal(TokenOutcome const& outcome, JwtTokenState const& state,
                JwtClock::time_point now) {
  if (outcome.fail()) {
    LOG_TOPIC("c4d18", WARN, Logger::AUTHENTICATION)
        << "renewing the JWT token failed: " << outcome.errorMessage()
        << "; keeping the current token";
    return;
  }
  if (!outcome.get().has_value()) {
    LOG_TOPIC("b3f90", DEBUG, Logger::AUTHENTICATION)
        << "server did not renew the JWT token yet, retry at least after "
        << kRenewalRetryInterval.count() << " seconds";
    return;
  }
  if (state.expiresAt.has_value()) {
    LOG_TOPIC("a7e21", INFO, Logger::AUTHENTICATION)
        << "renewed the JWT token, the new token expires in "
        << std::chrono::duration_cast<std::chrono::seconds>(*state.expiresAt -
                                                            now)
               .count()
        << " seconds";
  } else {
    LOG_TOPIC("d5a2f", INFO, Logger::AUTHENTICATION)
        << "renewed the JWT token, the new token does not expire";
  }
}

}  // namespace

auto tokenStateFor(JwtToken token, JwtClock::time_point now) -> JwtTokenState {
  auto expiresAt = expiryOf(token);
  return JwtTokenState{
      .token = std::move(token), .obtainedAt = now, .expiresAt = expiresAt};
}

auto isRenewalDue(JwtTokenState const& state, JwtClock::time_point now,
                  JwtClock::duration renewalThreshold) -> bool {
  if (!state.expiresAt.has_value()) {
    return false;
  }
  if (now < state.nextAttemptAt) {
    return false;
  }
  auto const expiresAt = *state.expiresAt;
  auto const halfLifetime =
      state.obtainedAt + (expiresAt - state.obtainedAt) / 2;
  return now >= std::max(expiresAt - renewalThreshold, halfLifetime);
}

auto applyRenewal(JwtTokenState state, TokenOutcome const& outcome,
                  JwtClock::time_point now) -> JwtTokenState {
  if (outcome.ok() && outcome.get().has_value()) {
    return tokenStateFor(*outcome.get(), now);
  }
  state.nextAttemptAt = now + kRenewalRetryInterval;
  return state;
}

auto parseTokenReply(TokenRequest request, int httpStatus,
                     velocypack::Slice body) -> TokenOutcome {
  if (request == TokenRequest::kLogin &&
      (httpStatus == static_cast<int>(rest::ResponseCode::NOT_FOUND) ||
       httpStatus == static_cast<int>(rest::ResponseCode::NOT_IMPLEMENTED))) {
    // the server does not implement /_open/auth, so it issues no tokens
    return TokenOutcome::success(std::nullopt);
  }
  if (httpStatus != static_cast<int>(rest::ResponseCode::OK)) {
    auto errorNum = ErrorCode{httpStatus};
    std::string errorMessage = absl::StrCat("HTTP ", httpStatus);
    if (body.isObject()) {
      if (auto const num = body.get(StaticStrings::ErrorNum); num.isNumber()) {
        errorNum = ErrorCode{num.getNumber<int>()};
      }
      if (auto const msg = body.get(StaticStrings::ErrorMessage);
          msg.isString()) {
        errorMessage = msg.copyString();
      }
    }
    return TokenOutcome::error(
        errorNum, absl::StrCat("got invalid response from server: HTTP ",
                               httpStatus, ": '", errorMessage, "'"));
  }
  if (!body.isObject()) {
    return TokenOutcome::error(
        TRI_ERROR_INTERNAL, "unexpected reply from /_open/auth: not an object");
  }
  auto const jwt = body.get("jwt");
  if (jwt.isNone()) {
    return TokenOutcome::success(std::nullopt);
  }
  if (!jwt.isString()) {
    return TokenOutcome::error(
        TRI_ERROR_INTERNAL,
        "unexpected reply from /_open/auth: jwt is not a string");
  }
  if (jwt.isEqualString("") || jwt.isEqualString(kAuthDisabledToken)) {
    return TokenOutcome::success(std::nullopt);
  }
  return TokenOutcome::success(jwt.copyString());
}

auto parseTokenResponse(TokenRequest request,
                        httpclient::SimpleHttpResult const& response)
    -> TokenOutcome {
  if (!response.isComplete()) {
    // no reply at all, e.g. the connection failed
    return HttpResponseChecker::check("", &response);
  }
  std::shared_ptr<velocypack::Builder> parsed;
  velocypack::Slice body;
  try {
    parsed = response.getBodyVelocyPack();
    body = parsed->slice();
  } catch (std::exception const&) {
    // not JSON; body stays None
  }
  return parseTokenReply(request, response.getHttpReturnCode(), body);
}

RenewingJwtToken::RenewingJwtToken(JwtToken initialToken, Renewer renewer,
                                   JwtClock::duration renewalThreshold,
                                   TimeSource now)
    : _state{tokenStateFor(std::move(initialToken), now())},
      _renewer(std::move(renewer)),
      _renewalThreshold(renewalThreshold),
      _now(std::move(now)) {}

auto RenewingJwtToken::current() -> JwtToken {
  auto const guard = std::lock_guard{_mutex};
  return _state.token;
}

void RenewingJwtToken::setRenewalThreshold(
    JwtClock::duration renewalThreshold) {
  auto const guard = std::lock_guard{_mutex};
  _renewalThreshold = renewalThreshold;
}

void RenewingJwtToken::renewIfDue() {
  auto const now = _now();
  auto const currentToken = [&] {
    auto const guard = std::lock_guard{_mutex};
    return isRenewalDue(_state, now, _renewalThreshold)
               ? std::optional{_state.token}
               : std::nullopt;
  }();
  if (!currentToken.has_value()) {
    return;
  }

  auto const outcome = _renewer(*currentToken);

  auto const guard = std::lock_guard{_mutex};
  _state = applyRenewal(std::move(_state), outcome, now);
  logRenewal(outcome, _state, now);
}

}  // namespace arangodb
