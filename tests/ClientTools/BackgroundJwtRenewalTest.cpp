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

#include "Utils/BackgroundJwtRenewal.h"
#include "Utils/RenewingJwtToken.h"

#include <absl/strings/escaping.h>
#include <absl/strings/str_cat.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <thread>

using namespace arangodb;
using namespace std::chrono_literals;

namespace {

constexpr auto threshold = 300s;

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
 * Clock that tests advance while the background thread reads it
 */
struct FakeClock {
  std::atomic<std::int64_t> seconds{0};

  auto timeSource() -> RenewingJwtToken::TimeSource {
    return [this] {
      return JwtClock::time_point{std::chrono::seconds{seconds.load()}};
    };
  }
};

/**
 * Renewer that counts its calls and always hands out the same new token
 */
struct CountingRenewer {
  std::atomic<int> calls{0};

  auto asFunction() -> RenewingJwtToken::Renewer {
    return [this](JwtToken const&) {
      ++calls;
      return RenewalOutcome::success(std::optional{tokenExpiringAt(2000)});
    };
  }
};

/**
 * Polls until the condition holds; false if the timeout passes first
 */
auto waitFor(std::function<bool()> const& condition,
             std::chrono::milliseconds timeout) -> bool {
  auto const deadline = std::chrono::steady_clock::now() + timeout;
  while (!condition()) {
    if (std::chrono::steady_clock::now() >= deadline) {
      return false;
    }
    std::this_thread::sleep_for(1ms);
  }
  return true;
}

}  // namespace

TEST(BackgroundJwtRenewalTest, renewsWhenDueWithoutAnyRequest) {
  auto clock = FakeClock{};
  auto renewer = CountingRenewer{};
  auto const token = std::make_shared<RenewingJwtToken>(
      tokenExpiringAt(1000), renewer.asFunction(), threshold,
      clock.timeSource());
  auto const renewal = BackgroundJwtRenewal{token, 5ms};

  clock.seconds = 700;

  ASSERT_TRUE(waitFor([&renewer] { return renewer.calls.load() == 1; }, 2s));
  EXPECT_EQ(token->current(), tokenExpiringAt(2000));
}

TEST(BackgroundJwtRenewalTest, staysIdleWhileRenewalIsNotDue) {
  auto clock = FakeClock{};
  auto renewer = CountingRenewer{};
  auto const token = std::make_shared<RenewingJwtToken>(
      tokenExpiringAt(1000), renewer.asFunction(), threshold,
      clock.timeSource());
  auto const renewal = BackgroundJwtRenewal{token, 5ms};

  clock.seconds = 699;
  std::this_thread::sleep_for(50ms);

  EXPECT_EQ(renewer.calls.load(), 0);
}

TEST(BackgroundJwtRenewalTest, stopsPromptlyOnDestruction) {
  auto clock = FakeClock{};
  auto renewer = CountingRenewer{};
  auto const token = std::make_shared<RenewingJwtToken>(
      tokenExpiringAt(1000), renewer.asFunction(), threshold,
      clock.timeSource());

  auto const start = std::chrono::steady_clock::now();
  { auto const renewal = BackgroundJwtRenewal{token, 10s}; }

  EXPECT_LT(std::chrono::steady_clock::now() - start, 2s);
}
