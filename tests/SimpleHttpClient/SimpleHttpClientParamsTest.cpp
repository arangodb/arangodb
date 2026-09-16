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

#include "SimpleHttpClient/SimpleHttpClient.h"

#include <string>

using namespace arangodb::httpclient;

namespace {

auto paramsWithoutCredentials() -> SimpleHttpClientParams {
  return SimpleHttpClientParams{/*requestTimeout*/ 10.0, /*warn*/ false};
}

}  // namespace

TEST(SimpleHttpClientParamsTest, currentJwtIsEmptyByDefault) {
  EXPECT_EQ(paramsWithoutCredentials().currentJwt(), "");
}

TEST(SimpleHttpClientParamsTest, currentJwtReturnsFixedTokenWithoutProvider) {
  auto params = paramsWithoutCredentials();
  params.setJwt("fixed-token");

  EXPECT_EQ(params.currentJwt(), "fixed-token");
}

TEST(SimpleHttpClientParamsTest, currentJwtPrefersProviderOverFixedToken) {
  auto params = paramsWithoutCredentials();
  params.setJwt("fixed-token");
  params.setJwtProvider([] { return std::string{"provided-token"}; });

  EXPECT_EQ(params.currentJwt(), "provided-token");
}

TEST(SimpleHttpClientParamsTest, currentJwtAsksProviderOnEveryCall) {
  auto params = paramsWithoutCredentials();
  auto calls = 0;
  params.setJwtProvider(
      [&calls] { return "token-" + std::to_string(++calls); });

  EXPECT_EQ(params.currentJwt(), "token-1");
  EXPECT_EQ(params.currentJwt(), "token-2");
}

TEST(SimpleHttpClientParamsTest, currentJwtIsEmptyWhenProviderReturnsEmpty) {
  auto params = paramsWithoutCredentials();
  params.setJwt("fixed-token");
  params.setJwtProvider([] { return std::string{}; });

  EXPECT_EQ(params.currentJwt(), "");
}

TEST(SimpleHttpClientParamsTest, providerSurvivesCopyingParams) {
  auto params = paramsWithoutCredentials();
  params.setJwtProvider([] { return std::string{"provided-token"}; });

  auto const copy = params;

  EXPECT_EQ(copy.currentJwt(), "provided-token");
}
