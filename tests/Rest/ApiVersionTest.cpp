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
#include <string_view>

#include "Basics/ApiVersion.h"
#include "Basics/ApiVersionFuerte.h"
#include "Basics/voc-errors.h"
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

// The branch tag is the major product version, so these expectations must be
// updated together with ARANGODB_VERSION_MAJOR.

TEST(ApiVersionTest, ServedVersionIsTheFullIdentifier) {
  EXPECT_EQ("v1.0-4", api_version::formatServedApiVersion(ApiVersion::V1));
  EXPECT_EQ("experimental",
            api_version::formatServedApiVersion(ApiVersion::Experimental));
}

TEST(ApiVersionTest, RequestedVersionAcceptsWhatTheServerServes) {
  auto const header = api_version::formatServedApiVersion(ApiVersion::V1);
  EXPECT_TRUE(
      api_version::checkRequestedApiVersion(ApiVersion::V1, header).ok());
}

// A client written against 3.12 keeps working after the upgrade to 4.
TEST(ApiVersionTest, RequestedVersionAcceptsALowerBranch) {
  EXPECT_TRUE(
      api_version::checkRequestedApiVersion(ApiVersion::V1, "v1.0-3").ok());
}

TEST(ApiVersionTest, RequestedVersionRejectsIncompatibleVersions) {
  for (std::string_view header : {
           "v1.1-4",  // higher minor
           "v1.0-5",  // higher branch
           "v0.0-3",  // major differs from the URL
           "v2.0-4",  // major differs from the URL
       }) {
    EXPECT_EQ(TRI_ERROR_HTTP_API_VERSION_MISMATCH,
              api_version::checkRequestedApiVersion(ApiVersion::V1, header)
                  .errorNumber())
        << header;
  }
}

TEST(ApiVersionTest, RequestedVersionRejectsMalformedHeaders) {
  for (std::string_view header :
       {"", "v1", "v1.0", "1.0-4", "V1.0-4", "v1.0-", "v.0-4", "v1.-4",
        "v1.0-4x", " v1.0-4", "v1.0-4 ", "v01.0-4", "v1.00-4", "v1.0-04",
        "v+1.0-4", "v-1.0-4", "v1.0-99999999999999999999", "experimental"}) {
    EXPECT_EQ(TRI_ERROR_BAD_PARAMETER,
              api_version::checkRequestedApiVersion(ApiVersion::V1, header)
                  .errorNumber())
        << "'" << header << "'";
  }
}

TEST(ApiVersionTest, RequestedVersionIsNotAllowedForExperimental) {
  for (std::string_view header : {"v1.0-4", "experimental"}) {
    EXPECT_EQ(TRI_ERROR_BAD_PARAMETER, api_version::checkRequestedApiVersion(
                                           ApiVersion::Experimental, header)
                                           .errorNumber())
        << header;
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
