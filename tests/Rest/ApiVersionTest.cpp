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
#include "Inspection/VPack.h"

#include <velocypack/Builder.h>
#include <velocypack/Parser.h>

using namespace arangodb;

// A version is presented by its name, the same one that selects it in a
// /_arango/<name> prefix - never by its numeric value.

TEST(ApiVersionTest, ToStringYieldsTheNameAndCoexistsWithStdToString) {
  using std::to_string;
  EXPECT_EQ("v1", to_string(ApiVersion::V1));
  EXPECT_EQ("experimental", to_string(ApiVersion::Experimental));
}

TEST(ApiVersionTest, StreamingYieldsTheName) {
  auto streamed = [](ApiVersion v) {
    std::ostringstream os;
    os << v;
    return os.str();
  };

  EXPECT_EQ("v1", streamed(ApiVersion::V1));
  EXPECT_EQ("experimental", streamed(ApiVersion::Experimental));
}

TEST(ApiVersionTest, ToFuerteMapsEveryEnumerator) {
  EXPECT_EQ(fuerte::api_version::ApiVersion::V1, toFuerte(ApiVersion::V1));
  EXPECT_EQ(fuerte::api_version::ApiVersion::Experimental,
            toFuerte(ApiVersion::Experimental));
}

TEST(ApiVersionTest, InspectSerializesTheName) {
  for (auto version : {ApiVersion::V1, ApiVersion::Experimental}) {
    velocypack::Builder builder;
    velocypack::serialize(builder, version);
    ASSERT_TRUE(builder.slice().isString());
    EXPECT_EQ(to_string(version), builder.slice().stringView());
  }
}

TEST(ApiVersionTest, InspectDeserializesTheName) {
  for (auto version : {ApiVersion::V1, ApiVersion::Experimental}) {
    auto const json = velocypack::Parser::fromJson(
        "\"" + std::string{to_string(version)} + "\"");
    ApiVersion result{};
    auto const status =
        velocypack::deserializeWithStatus(json->slice(), result);
    ASSERT_TRUE(status.ok()) << status.error();
    EXPECT_EQ(version, result);
  }
}

TEST(ApiVersionTest, InspectRejectsUnknownNamesAndNumbers) {
  for (auto const* json : {R"("v2")", R"("V0")", R"("")", "0", "2"}) {
    ApiVersion result{};
    EXPECT_FALSE(velocypack::deserializeWithStatus(
                     velocypack::Parser::fromJson(json)->slice(), result)
                     .ok())
        << json;
  }
}
