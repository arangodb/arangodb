////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2025 ArangoDB GmbH, Cologne, Germany
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

#include <limits>
#include <string>

#include "Basics/ApiVersion.h"
#include "Basics/ScopeGuard.h"
#include "Basics/debugging.h"
#include "Endpoint/ConnectionInfo.h"
#include "Rest/HttpRequest.h"

using namespace arangodb;

namespace {

// The path from the advanced start pointer to the end, i.e. what is left for
// routing once the version prefix has been consumed.
std::string detect(HttpRequest& request, std::string const& path) {
  char const* start = path.data();
  char const* end = path.data() + path.size();
  request.detectAndStripApiVersion(start, end);
  return std::string(start, end);
}

}  // namespace

TEST(GeneralRequestTest, ValidApiVersionPrefixesListsSupportedAndExperimental) {
  EXPECT_EQ("/_arango/v1, /_arango/experimental",
            GeneralRequest::validApiVersionPrefixes());
}

class ApiVersionDetectionTest : public ::testing::Test {
 protected:
  ConnectionInfo ci;
};

TEST_F(ApiVersionDetectionTest, NewRequestHasDefaultVersion) {
  HttpRequest request(ci, 1);
  ASSERT_TRUE(request.hasValidApiVersion());
  EXPECT_EQ(api_version::defaultApiVersion, request.requestedApiVersion());
}

TEST_F(ApiVersionDetectionTest, DefaultApiVersionIsV1) {
  EXPECT_EQ(ApiVersion::V1, api_version::defaultApiVersion);
}

TEST_F(ApiVersionDetectionTest, NoApiVersionPrefix) {
  HttpRequest request(ci, 1);
  EXPECT_EQ("/_api/version", detect(request, "/_api/version"));
  ASSERT_TRUE(request.hasValidApiVersion());
  EXPECT_EQ(api_version::defaultApiVersion, request.requestedApiVersion());
}

TEST_F(ApiVersionDetectionTest, RootPath) {
  HttpRequest request(ci, 1);
  EXPECT_EQ("/", detect(request, "/"));
  ASSERT_TRUE(request.hasValidApiVersion());
  EXPECT_EQ(api_version::defaultApiVersion, request.requestedApiVersion());
}

// Only "/_arango/" introduces a version prefix.
TEST_F(ApiVersionDetectionTest, ArangoWithoutTrailingSlashIsNoPrefix) {
  HttpRequest request(ci, 1);
  EXPECT_EQ("/_arango", detect(request, "/_arango"));
  ASSERT_TRUE(request.hasValidApiVersion());
  EXPECT_EQ(api_version::defaultApiVersion, request.requestedApiVersion());
}

TEST_F(ApiVersionDetectionTest, V1IsStripped) {
  HttpRequest request(ci, 1);
  EXPECT_EQ("/path", detect(request, "/_arango/v1/path"));
  ASSERT_TRUE(request.hasValidApiVersion());
  EXPECT_EQ(ApiVersion::V1, request.requestedApiVersion());
}

TEST_F(ApiVersionDetectionTest, PathEndingAtVersion) {
  HttpRequest request(ci, 1);
  EXPECT_EQ("", detect(request, "/_arango/v1"));
  ASSERT_TRUE(request.hasValidApiVersion());
  EXPECT_EQ(ApiVersion::V1, request.requestedApiVersion());
}

TEST_F(ApiVersionDetectionTest, ExperimentalIsStripped) {
  HttpRequest request(ci, 1);
  EXPECT_EQ("/_api/new-feature",
            detect(request, "/_arango/experimental/_api/new-feature"));
  ASSERT_TRUE(request.hasValidApiVersion());
  EXPECT_EQ(api_version::experimentalApiVersion, request.requestedApiVersion());
}

TEST_F(ApiVersionDetectionTest, PathEndingAtExperimental) {
  HttpRequest request(ci, 1);
  EXPECT_EQ("", detect(request, "/_arango/experimental"));
  ASSERT_TRUE(request.hasValidApiVersion());
  EXPECT_EQ(api_version::experimentalApiVersion, request.requestedApiVersion());
}

// Every path below carries a /_arango/ prefix that does not name a requestable
// version. Detection must not throw, must leave the request without a valid
// version (so it is rejected later with a 404), and must not strip anything.
class InvalidApiVersionTest : public ::testing::TestWithParam<std::string> {
 protected:
  ConnectionInfo ci;
};

TEST_P(InvalidApiVersionTest, HasNoValidVersionAndIsNotStripped) {
  HttpRequest request(ci, 1);
  std::string const& path = GetParam();
  EXPECT_EQ(path, detect(request, path));
  EXPECT_FALSE(request.hasValidApiVersion());
}

INSTANTIATE_TEST_SUITE_P(
    ApiVersionDetection, InvalidApiVersionTest,
    ::testing::Values(
        // malformed prefix
        "/_arango/", "/_arango/1/path", "/_arango/invalid/path",
        "/_arango/experimenta/path", "/_arango/experimentalabc/path",
        "/_arango/v", "/_arango/v/path", "/_arango/vabc/path",
        "/_arango/v1abc/path",
        // leading zeros
        "/_arango/v00/path", "/_arango/v000", "/_arango/v01/path",
        "/_arango/v007/path", "/_arango/v010", "/_arango/v0100/path",
        "/_arango/v0123/_api/version",
        // well-formed, but not requestable
        "/_arango/v2/path", "/_arango/v3/path", "/_arango/v7/_api/cursor",
        "/_arango/v42/_api/version",
        // 256 would truncate to V0 if cast to the underlying type unchecked
        "/_arango/v256/path",
        "/_arango/v" + std::to_string(std::numeric_limits<uint32_t>::max()) +
            "/path",
        "/_arango/v" + std::to_string(std::numeric_limits<uint64_t>::max()) +
            "/path",
        // does not fit into uint64_t
        "/_arango/v18446744073709551616/path",
        "/_arango/v" + std::string(100, '9') + "/path"));
