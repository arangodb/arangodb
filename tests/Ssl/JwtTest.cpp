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

#include "Ssl/jwt.h"

#include <absl/strings/escaping.h>

#include <chrono>
#include <cmath>
#include <optional>
#include <string>

using namespace arangodb::rest::SslInterface::jwt;

namespace {

/**
 * Current time in seconds since epoch, the unit of the "exp" claim
 */
auto nowInSeconds() -> double {
  return std::chrono::duration<double>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

/**
 * Assembles a token around an already base64-encoded body
 *
 * Header and signature are never decoded by extractExpiration, so arbitrary
 * placeholders are sufficient.
 */
auto tokenWithEncodedBody(std::string const& encodedBody) -> std::string {
  return "header." + encodedBody + ".signature";
}

/**
 * Removes the trailing '=' padding, as JWTs must be encoded without it
 */
auto withoutPadding(std::string encoded) -> std::string {
  while (encoded.ends_with('=')) {
    encoded.pop_back();
  }
  return encoded;
}

// the '?' at index 17 (index 2 within its 3-byte group) encodes to the 6-bit
// value 63, which is '/' in the standard and '_' in the web-safe alphabet
constexpr auto bodyWithExpiry = R"({"exp":1000,"n":"?"})";

}  // namespace

TEST(JwtTest, extractExpirationReturnsExpClaimOfUserToken) {
  auto const before = nowInSeconds();
  auto const token =
      generateUserToken("secret", "root", std::chrono::seconds{3600});

  auto const expiration = extractExpiration(token);

  ASSERT_TRUE(expiration.has_value());
  EXPECT_GE(*expiration, std::floor(before) + 3600.0);
  EXPECT_LE(*expiration, nowInSeconds() + 3600.0);
}

TEST(JwtTest, extractExpirationIsEmptyForTokenWithoutExp) {
  EXPECT_EQ(extractExpiration(generateInternalToken("secret", "server-id")),
            std::nullopt);
  EXPECT_EQ(extractExpiration(generateUserToken("secret", "root")),
            std::nullopt);
}

TEST(JwtTest, extractExpirationIsEmptyForMalformedToken) {
  EXPECT_EQ(extractExpiration(""), std::nullopt);
  EXPECT_EQ(extractExpiration("header.body"), std::nullopt);
  EXPECT_EQ(extractExpiration("a.b.c.d"), std::nullopt);
  EXPECT_EQ(extractExpiration("header.!!not-base64!!.signature"), std::nullopt);
  EXPECT_EQ(extractExpiration(
                tokenWithEncodedBody(absl::WebSafeBase64Escape("not json"))),
            std::nullopt);
  EXPECT_EQ(extractExpiration(tokenWithEncodedBody(
                absl::WebSafeBase64Escape(R"(["exp",1000])"))),
            std::nullopt);
  EXPECT_EQ(extractExpiration(tokenWithEncodedBody(
                absl::WebSafeBase64Escape(R"({"exp":"soon"})"))),
            std::nullopt);
}

TEST(JwtTest, extractExpirationDecodesWebSafeBase64Body) {
  auto const encodedBody = absl::WebSafeBase64Escape(bodyWithExpiry);
  ASSERT_NE(encodedBody.find('_'), std::string::npos);

  EXPECT_EQ(extractExpiration(tokenWithEncodedBody(encodedBody)), 1000.0);
}

TEST(JwtTest, extractExpirationDecodesStandardBase64Body) {
  auto const encodedBody = withoutPadding(absl::Base64Escape(bodyWithExpiry));
  ASSERT_NE(encodedBody.find('/'), std::string::npos);

  EXPECT_EQ(extractExpiration(tokenWithEncodedBody(encodedBody)), 1000.0);
}

TEST(JwtTest, extractPreferredUsernameReturnsClaimOfUserToken) {
  auto const token =
      generateUserToken("secret", "alice", std::chrono::seconds{60});

  EXPECT_EQ(extractPreferredUsername(token), "alice");
}

TEST(JwtTest, extractPreferredUsernameIsEmptyWithoutStringClaim) {
  EXPECT_EQ(extractPreferredUsername(generateInternalToken("secret", "id")),
            std::nullopt);
  EXPECT_EQ(extractPreferredUsername(""), std::nullopt);
  EXPECT_EQ(extractPreferredUsername(tokenWithEncodedBody(
                absl::WebSafeBase64Escape(R"({"preferred_username":42})"))),
            std::nullopt);
}
