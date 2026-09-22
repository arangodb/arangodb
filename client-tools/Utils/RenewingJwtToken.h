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

#pragma once

#include "Basics/ResultT.h"

#include <chrono>
#include <functional>
#include <mutex>
#include <optional>
#include <string>

namespace arangodb {
namespace httpclient {
class SimpleHttpResult;
}

/**
 * The full jwt token
 */
using JwtToken = std::string;

/**
 * Wall clock of JWT expiry: the "exp" claim is an absolute time since epoch
 */
using JwtClock = std::chrono::system_clock;

/**
 * Snapshot of a JWT that is renewed via POST /_open/auth or /_open/auth/renew
 */
struct JwtTokenState {
  JwtToken token;
  /// when the token was received
  JwtClock::time_point obtainedAt;
  /// "exp" claim of the token; nullopt means it never expires and is never
  /// renewed
  std::optional<JwtClock::time_point> expiresAt;
  /// earliest time for the next renewal request; set after a request that
  /// yielded no token
  JwtClock::time_point nextAttemptAt;
};

/**
 * Outcome of one POST to /_open/auth or /_open/auth/renew
 *
 * ok(token): the server issued a token. ok(nullopt): the server issued none,
 * because a renewal is not yet due or authentication is disabled.
 * error: the request failed.
 */
using TokenOutcome = ResultT<std::optional<JwtToken>>;

/// Time to wait after a renewal request that yielded no token
constexpr auto kRenewalRetryInterval = std::chrono::seconds{5};

/**
 * Whether a renewal request should be sent now
 *
 * Due once the token is within renewalThreshold of its expiry, but never
 * before half of its lifetime has passed (so a threshold above the lifetime
 * does not renew on every request) and never before nextAttemptAt.
 */
auto isRenewalDue(JwtTokenState const& state, JwtClock::time_point now,
                  JwtClock::duration renewalThreshold) -> bool;

/**
 * Folds one renewal outcome into the state
 *
 * A new token replaces the old one. Otherwise the token is kept and the
 * next attempt is scheduled kRenewalRetryInterval later.
 */
auto applyRenewal(JwtTokenState state, TokenOutcome const& outcome,
                  JwtClock::time_point now) -> JwtTokenState;

/**
 * Interprets the complete HTTP reply of a POST to /_open/auth or
 * /_open/auth/renew
 */
auto parseTokenResponse(httpclient::SimpleHttpResult const& response)
    -> TokenOutcome;

/**
 * Thread-safe holder of a user JWT that is renewed before it expires
 *
 * Example:
 *   auto token = std::make_shared<RenewingJwtToken>(
 *       initialToken,
 *       [](JwtToken const& current) { return postRenewWith(current); },
 *       std::chrono::seconds{300}, &JwtClock::now);
 *   params.setJwtProvider([token] { return token->current(); });
 *   auto const renewal = BackgroundJwtRenewal{token, std::chrono::seconds{1}};
 *
 * Renewal happens only in renewIfDue(), which is meant to be driven by one
 * background thread. current() never waits for a renewal request.
 */
class RenewingJwtToken {
 public:
  /// obtains a new token, via POST /_open/auth/renew with the current token or
  /// via POST /_open/auth with credentials
  using Renewer = std::function<TokenOutcome(JwtToken const& currentToken)>;
  /// tells the current time; injectable for tests
  using TimeSource = std::function<JwtClock::time_point()>;

  RenewingJwtToken(JwtToken initialToken, Renewer renewer,
                   JwtClock::duration renewalThreshold, TimeSource now);

  /**
   * Token to send now
   */
  auto current() -> JwtToken;

  /**
   * Renews the token if due
   *
   * The renewal request runs unlocked, so current() keeps answering with the
   * still valid token meanwhile. Meant to be called from a single thread.
   */
  void renewIfDue();

 private:
  std::mutex _mutex;
  JwtTokenState _state;
  Renewer _renewer;
  JwtClock::duration _renewalThreshold;
  TimeSource _now;
};

}  // namespace arangodb
