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

#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "SimpleHttpClient/HttpResponseChecker.h"
#include "SimpleHttpClient/SimpleHttpResult.h"
#include "Ssl/jwt.h"

#include <absl/strings/str_cat.h>
#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

#include <algorithm>
#include <chrono>
#include <exception>
#include <utility>

namespace arangodb {

namespace {

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
    return JwtTokenState{.token = *outcome.get(),
                         .obtainedAt = now,
                         .expiresAt = expiryOf(*outcome.get())};
  }
  state.nextAttemptAt = now + kRenewalRetryInterval;
  return state;
}

auto parseTokenResponse(httpclient::SimpleHttpResult const& response)
    -> TokenOutcome {
  if (auto const check = HttpResponseChecker::check("", &response);
      check.fail()) {
    return check;
  }
  try {
    auto const body = response.getBodyVelocyPack()->slice();
    if (!body.isObject()) {
      return TokenOutcome::error(
          TRI_ERROR_INTERNAL,
          "unexpected reply from /_open/auth: not an object");
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
    if (jwt.isEqualString("invalid")) {
      // the server issues no tokens while authentication is disabled
      return TokenOutcome::success(std::nullopt);
    }
    return TokenOutcome::success(jwt.copyString());
  } catch (std::exception const& ex) {
    return TokenOutcome::error(
        TRI_ERROR_INTERNAL,
        absl::StrCat("cannot parse reply from /_open/auth: ", ex.what()));
  }
}

RenewingJwtToken::RenewingJwtToken(JwtToken initialToken, Renewer renewer,
                                   JwtClock::duration renewalThreshold,
                                   TimeSource now)
    : _state{.token = initialToken,
             .obtainedAt = now(),
             .expiresAt = expiryOf(initialToken)},
      _renewer(std::move(renewer)),
      _renewalThreshold(renewalThreshold),
      _now(std::move(now)) {}

auto RenewingJwtToken::current() -> JwtToken {
  auto const guard = std::lock_guard{_mutex};
  return _state.token;
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
