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

#include <sstream>
#include <string>

#include "Basics/ApiVersion.h"
#include "Basics/ApiVersionFuerte.h"

using namespace arangodb;

// ApiVersion's underlying type is std::uint8_t, which streams as a *character*.
// These tests pin the promotion-safe conversions that callers must use instead
// of casting to the underlying type themselves.

TEST(ApiVersionTest, ToNumberYieldsTheVersionNumber) {
  EXPECT_EQ(0u, toNumber(ApiVersion::V0));
  EXPECT_EQ(1u, toNumber(ApiVersion::V1));
  EXPECT_EQ(2u, toNumber(ApiVersion::Experimental));
}

TEST(ApiVersionTest, StreamingYieldsDigitsNotControlCharacters) {
  auto streamed = [](ApiVersion v) {
    std::ostringstream os;
    os << v;
    return os.str();
  };

  EXPECT_EQ("0", streamed(ApiVersion::V0));
  EXPECT_EQ("1", streamed(ApiVersion::V1));
  EXPECT_EQ("2", streamed(ApiVersion::Experimental));
}

TEST(ApiVersionTest, ToStringYieldsDigitsAndCoexistsWithStdToString) {
  using std::to_string;
  EXPECT_EQ("0", to_string(ApiVersion::V0));
  EXPECT_EQ("1", to_string(ApiVersion::V1));
  EXPECT_EQ("2", to_string(ApiVersion::Experimental));
}

TEST(ApiVersionTest, StreamingEmitsNoUnprintableBytes) {
  std::ostringstream os;
  os << ApiVersion::V0;
  auto const s = os.str();
  ASSERT_EQ(1u, s.size());
  EXPECT_GE(static_cast<unsigned char>(s[0]), 0x20u)
      << "streamed a control byte instead of a digit";
}

TEST(ApiVersionTest, ToFuerteMapsEveryEnumerator) {
  EXPECT_EQ(fuerte::api_version::ApiVersion::V0, toFuerte(ApiVersion::V0));
  EXPECT_EQ(fuerte::api_version::ApiVersion::V1, toFuerte(ApiVersion::V1));
  EXPECT_EQ(fuerte::api_version::ApiVersion::Experimental,
            toFuerte(ApiVersion::Experimental));
}
