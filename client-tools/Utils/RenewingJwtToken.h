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
 * Snapshot of a JWT that is renewed via POST /_open/auth/renew
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
 * Outcome of one POST /_open/auth/renew
 *
 * ok(token): the server issued a new token. ok(nullopt): the server is not
 * yet willing to renew, because the token is still far from expiry.
 * error: the request failed.
 */
using RenewalOutcome = ResultT<std::optional<JwtToken>>;

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
auto applyRenewal(JwtTokenState state, RenewalOutcome const& outcome,
                  JwtClock::time_point now) -> JwtTokenState;

/**
 * Interprets the complete HTTP reply of POST /_open/auth/renew
 */
auto parseRenewalResponse(httpclient::SimpleHttpResult const& response)
    -> RenewalOutcome;

/**
 * Thread-safe holder of a user JWT that renews itself before it expires
 *
 * Example:
 *   auto token = std::make_shared<RenewingJwtToken>(
 *       initialToken,
 *       [](JwtToken const& current) { return postRenewWith(current); },
 *       std::chrono::seconds{300}, &JwtClock::now);
 *   params.setJwtProvider([token] { return token->current(); });
 *
 * The renewer runs while the holder is locked, so concurrent callers trigger a
 * single renewal request and afterwards all see the renewed token.
 */
class RenewingJwtToken {
 public:
  /// sends POST /_open/auth/renew authenticated with the current token
  using Renewer = std::function<RenewalOutcome(JwtToken const& currentToken)>;
  /// tells the current time; injectable for tests
  using TimeSource = std::function<JwtClock::time_point()>;

  RenewingJwtToken(JwtToken initialToken, Renewer renewer,
                   JwtClock::duration renewalThreshold, TimeSource now);

  /**
   * Token to send now; renews it first if due
   */
  auto current() -> JwtToken;

  /**
   * Renews the token if due, independent of any request being sent
   */
  void renewIfDue();

 private:
  /**
   * Renews the token if due; the caller holds _mutex
   */
  void renewIfDueLocked(JwtClock::time_point now);

  std::mutex _mutex;
  JwtTokenState _state;
  Renewer _renewer;
  JwtClock::duration _renewalThreshold;
  TimeSource _now;
};

}  // namespace arangodb
