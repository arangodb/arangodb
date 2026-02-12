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
/// @author Max Neunhoeffer
/// @author Copyright 2025, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include <limits>

#include "Basics/Result.h"
#include "Endpoint/ConnectionInfo.h"
#include "Rest/HttpRequest.h"

using namespace arangodb;

// Test fixture for API version detection tests
class ApiVersionDetectionTest : public ::testing::Test {
 protected:
  ConnectionInfo ci;
};

// Test: No API version prefix - should use default version
TEST_F(ApiVersionDetectionTest, NoApiVersionPrefix) {
  HttpRequest request(ci, 1);
  request.setRequestPath("/_api/version");

  Result res = request.detectAndStripApiVersion();

  EXPECT_TRUE(res.ok());
  EXPECT_EQ(GeneralRequest::defaultApiVersion, request.apiVersion());
  EXPECT_EQ("/_api/version", request.requestPath());
}

// Test: Regular path without /_arango prefix
TEST_F(ApiVersionDetectionTest, RegularPath) {
  HttpRequest request(ci, 1);
  request.setRequestPath("/some/random/path");

  Result res = request.detectAndStripApiVersion();

  EXPECT_TRUE(res.ok());
  EXPECT_EQ(GeneralRequest::defaultApiVersion, request.apiVersion());
  EXPECT_EQ("/some/random/path", request.requestPath());
}

// Test: Valid API version v1
TEST_F(ApiVersionDetectionTest, ValidApiVersionV1) {
  HttpRequest request(ci, 1);
  request.setRequestPath("/_arango/v1/_api/version");

  Result res = request.detectAndStripApiVersion();

  EXPECT_TRUE(res.ok());
  EXPECT_EQ(1u, request.apiVersion());
  EXPECT_EQ("/_api/version", request.requestPath());
}

// Test: Valid API version v2
TEST_F(ApiVersionDetectionTest, ValidApiVersionV2) {
  HttpRequest request(ci, 1);
  request.setRequestPath("/_arango/v2/_api/collection");

  Result res = request.detectAndStripApiVersion();

  EXPECT_TRUE(res.ok());
  EXPECT_EQ(2u, request.apiVersion());
  EXPECT_EQ("/_api/collection", request.requestPath());
}

// Test: Valid API version v42
TEST_F(ApiVersionDetectionTest, ValidApiVersionV42) {
  HttpRequest request(ci, 1);
  request.setRequestPath("/_arango/v42/_admin/status");

  Result res = request.detectAndStripApiVersion();

  EXPECT_TRUE(res.ok());
  EXPECT_EQ(42u, request.apiVersion());
  EXPECT_EQ("/_admin/status", request.requestPath());
}

// Test: Valid API version v1234567
TEST_F(ApiVersionDetectionTest, ValidApiVersionLargeNumber) {
  HttpRequest request(ci, 1);
  request.setRequestPath("/_arango/v1234567/path");

  Result res = request.detectAndStripApiVersion();

  EXPECT_TRUE(res.ok());
  EXPECT_EQ(1234567u, request.apiVersion());
  EXPECT_EQ("/path", request.requestPath());
}

// Test: Experimental API version
TEST_F(ApiVersionDetectionTest, ExperimentalApiVersion) {
  HttpRequest request(ci, 1);
  request.setRequestPath("/_arango/experimental/_api/new-feature");

  Result res = request.detectAndStripApiVersion();

  EXPECT_TRUE(res.ok());
  EXPECT_EQ(std::numeric_limits<uint32_t>::max(), request.apiVersion());
  EXPECT_EQ("/_api/new-feature", request.requestPath());
}

// Test: Path ending exactly at version number
TEST_F(ApiVersionDetectionTest, PathEndingAtVersion) {
  HttpRequest request(ci, 1);
  request.setRequestPath("/_arango/v5");

  Result res = request.detectAndStripApiVersion();

  EXPECT_TRUE(res.ok());
  EXPECT_EQ(5u, request.apiVersion());
  EXPECT_EQ("/", request.requestPath());
}

// Test: Path ending exactly at experimental
TEST_F(ApiVersionDetectionTest, PathEndingAtExperimental) {
  HttpRequest request(ci, 1);
  request.setRequestPath("/_arango/experimental");

  Result res = request.detectAndStripApiVersion();

  EXPECT_TRUE(res.ok());
  EXPECT_EQ(std::numeric_limits<uint32_t>::max(), request.apiVersion());
  EXPECT_EQ("/", request.requestPath());
}

// Test: Invalid - path is exactly /_arango/
TEST_F(ApiVersionDetectionTest, InvalidExactlyArango) {
  HttpRequest request(ci, 1);
  request.setRequestPath("/_arango/");

  Result res = request.detectAndStripApiVersion();

  EXPECT_TRUE(res.fail());
  EXPECT_EQ(TRI_ERROR_HTTP_BAD_PARAMETER, res.errorNumber());
  EXPECT_EQ(GeneralRequest::defaultApiVersion,
            request.apiVersion());                // Should remain at default
  EXPECT_EQ("/_arango/", request.requestPath());  // Path unchanged on error
}

// Test: Invalid - missing v prefix
TEST_F(ApiVersionDetectionTest, InvalidMissingVPrefix) {
  HttpRequest request(ci, 1);
  request.setRequestPath("/_arango/1/path");

  Result res = request.detectAndStripApiVersion();

  EXPECT_TRUE(res.fail());
  EXPECT_EQ(TRI_ERROR_HTTP_BAD_PARAMETER, res.errorNumber());
  EXPECT_EQ(GeneralRequest::defaultApiVersion, request.apiVersion());
  EXPECT_EQ("/_arango/1/path", request.requestPath());
}

// Test: Invalid - v without number
TEST_F(ApiVersionDetectionTest, InvalidVWithoutNumber) {
  HttpRequest request(ci, 1);
  request.setRequestPath("/_arango/v/path");

  Result res = request.detectAndStripApiVersion();

  EXPECT_TRUE(res.fail());
  EXPECT_EQ(TRI_ERROR_HTTP_BAD_PARAMETER, res.errorNumber());
  EXPECT_EQ(GeneralRequest::defaultApiVersion, request.apiVersion());
  EXPECT_EQ("/_arango/v/path", request.requestPath());
}

// Test: Invalid - v with non-numeric characters
TEST_F(ApiVersionDetectionTest, InvalidVWithAlpha) {
  HttpRequest request(ci, 1);
  request.setRequestPath("/_arango/vabc/path");

  Result res = request.detectAndStripApiVersion();

  EXPECT_TRUE(res.fail());
  EXPECT_EQ(TRI_ERROR_HTTP_BAD_PARAMETER, res.errorNumber());
  EXPECT_EQ(GeneralRequest::defaultApiVersion, request.apiVersion());
  EXPECT_EQ("/_arango/vabc/path", request.requestPath());
}

// Test: Invalid - random text after /_arango/
TEST_F(ApiVersionDetectionTest, InvalidRandomText) {
  HttpRequest request(ci, 1);
  request.setRequestPath("/_arango/invalid/path");

  Result res = request.detectAndStripApiVersion();

  EXPECT_TRUE(res.fail());
  EXPECT_EQ(TRI_ERROR_HTTP_BAD_PARAMETER, res.errorNumber());
  EXPECT_EQ(GeneralRequest::defaultApiVersion, request.apiVersion());
  EXPECT_EQ("/_arango/invalid/path", request.requestPath());
}

// Test: Invalid - experimental typo
TEST_F(ApiVersionDetectionTest, InvalidExperimentalTypo) {
  HttpRequest request(ci, 1);
  request.setRequestPath("/_arango/experimenta/path");

  Result res = request.detectAndStripApiVersion();

  EXPECT_TRUE(res.fail());
  EXPECT_EQ(TRI_ERROR_HTTP_BAD_PARAMETER, res.errorNumber());
  EXPECT_EQ(GeneralRequest::defaultApiVersion, request.apiVersion());
}

// Test: Edge case - v0 is valid (should match default if default is 0)
TEST_F(ApiVersionDetectionTest, ValidApiVersionV0) {
  HttpRequest request(ci, 1);
  request.setRequestPath("/_arango/v0/path");

  Result res = request.detectAndStripApiVersion();

  EXPECT_TRUE(res.ok());
  EXPECT_EQ(0u, request.apiVersion());
  EXPECT_EQ("/path", request.requestPath());
}

// Test: Version number followed by non-slash character should be invalid
TEST_F(ApiVersionDetectionTest, InvalidVersionNotFollowedBySlash) {
  HttpRequest request(ci, 1);
  request.setRequestPath("/_arango/v1abc/path");

  Result res = request.detectAndStripApiVersion();

  EXPECT_TRUE(res.fail());
  EXPECT_EQ(TRI_ERROR_HTTP_BAD_PARAMETER, res.errorNumber());
}

// Test: Experimental followed by non-slash character should be invalid
TEST_F(ApiVersionDetectionTest, InvalidExperimentalNotFollowedBySlash) {
  HttpRequest request(ci, 1);
  request.setRequestPath("/_arango/experimentalabc/path");

  Result res = request.detectAndStripApiVersion();

  EXPECT_TRUE(res.fail());
  EXPECT_EQ(TRI_ERROR_HTTP_BAD_PARAMETER, res.errorNumber());
}

// Test: Multiple slashes after version
TEST_F(ApiVersionDetectionTest, MultipleSlashesAfterVersion) {
  HttpRequest request(ci, 1);
  request.setRequestPath("/_arango/v1///path");

  Result res = request.detectAndStripApiVersion();

  EXPECT_TRUE(res.ok());
  EXPECT_EQ(1, request.apiVersion());
  EXPECT_EQ("///path", request.requestPath());
}

// Test: Root path
TEST_F(ApiVersionDetectionTest, RootPath) {
  HttpRequest request(ci, 1);
  request.setRequestPath("/");

  Result res = request.detectAndStripApiVersion();

  EXPECT_TRUE(res.ok());
  EXPECT_EQ(GeneralRequest::defaultApiVersion, request.apiVersion());
  EXPECT_EQ("/", request.requestPath());
}

// Test: Deep nested path with version
TEST_F(ApiVersionDetectionTest, DeepNestedPath) {
  HttpRequest request(ci, 1);
  request.setRequestPath("/_arango/v3/_api/collection/test/document/123");

  Result res = request.detectAndStripApiVersion();

  EXPECT_TRUE(res.ok());
  EXPECT_EQ(3u, request.apiVersion());
  EXPECT_EQ("/_api/collection/test/document/123", request.requestPath());
}

// Test: Path with query parameters (should work on path only)
TEST_F(ApiVersionDetectionTest, PathWithQueryString) {
  HttpRequest request(ci, 1);
  request.setRequestPath("/_arango/v7/_api/cursor");

  Result res = request.detectAndStripApiVersion();

  EXPECT_TRUE(res.ok());
  EXPECT_EQ(7u, request.apiVersion());
  EXPECT_EQ("/_api/cursor", request.requestPath());
}

// Test: API version at max uint32_t - 1 should work
TEST_F(ApiVersionDetectionTest, MaxUint32Minus1) {
  HttpRequest request(ci, 1);
  std::string path = "/_arango/v" +
                     std::to_string(std::numeric_limits<uint32_t>::max() - 1) +
                     "/path";
  request.setRequestPath(path);

  Result res = request.detectAndStripApiVersion();

  EXPECT_TRUE(res.ok());
  EXPECT_EQ(std::numeric_limits<uint32_t>::max() - 1, request.apiVersion());
  EXPECT_EQ("/path", request.requestPath());
}

// Test: Check that default API version constant is accessible and is 0
TEST_F(ApiVersionDetectionTest, DefaultApiVersionConstant) {
  EXPECT_EQ(0u, GeneralRequest::defaultApiVersion);
}

// Test: Verify default version is applied to new requests
TEST_F(ApiVersionDetectionTest, NewRequestHasDefaultVersion) {
  HttpRequest request(ci, 1);
  // Before calling detectAndStripApiVersion, apiVersion should be
  // defaultApiVersion
  EXPECT_EQ(GeneralRequest::defaultApiVersion, request.apiVersion());
}

// Test: API version exceeding uint32_t max should be rejected
TEST_F(ApiVersionDetectionTest, VersionExceedsUint32Max) {
  HttpRequest request(ci, 1);
  // Create a version number that's uint32_t::max() + 1
  uint64_t tooLarge =
      static_cast<uint64_t>(std::numeric_limits<uint32_t>::max()) + 1;
  std::string path = "/_arango/v" + std::to_string(tooLarge) + "/path";
  request.setRequestPath(path);

  Result res = request.detectAndStripApiVersion();

  EXPECT_TRUE(res.fail());
  EXPECT_EQ(TRI_ERROR_HTTP_BAD_PARAMETER, res.errorNumber());
  EXPECT_EQ(GeneralRequest::defaultApiVersion,
            request.apiVersion());         // Should remain at default
  EXPECT_EQ(path, request.requestPath());  // Path unchanged on error
  // Verify the error message mentions the version is too large
  EXPECT_NE(std::string::npos, res.errorMessage().find("too large"));
}

// Test: Version with leading zero should be rejected (v01)
TEST_F(ApiVersionDetectionTest, InvalidVersionLeadingZeroV01) {
  HttpRequest request(ci, 1);
  request.setRequestPath("/_arango/v01/path");

  Result res = request.detectAndStripApiVersion();

  EXPECT_TRUE(res.fail());
  EXPECT_EQ(TRI_ERROR_HTTP_BAD_PARAMETER, res.errorNumber());
  EXPECT_EQ(GeneralRequest::defaultApiVersion, request.apiVersion());
  EXPECT_EQ("/_arango/v01/path", request.requestPath());
  // Verify the error message mentions leading zeros
  EXPECT_NE(std::string::npos, res.errorMessage().find("leading zeros"));
}

// Test: Version with leading zero should be rejected (v007)
TEST_F(ApiVersionDetectionTest, InvalidVersionLeadingZeroV007) {
  HttpRequest request(ci, 1);
  request.setRequestPath("/_arango/v007/path");

  Result res = request.detectAndStripApiVersion();

  EXPECT_TRUE(res.fail());
  EXPECT_EQ(TRI_ERROR_HTTP_BAD_PARAMETER, res.errorNumber());
  EXPECT_EQ(GeneralRequest::defaultApiVersion, request.apiVersion());
  EXPECT_EQ("/_arango/v007/path", request.requestPath());
  EXPECT_NE(std::string::npos, res.errorMessage().find("leading zeros"));
}

// Test: Version with leading zero should be rejected (v0123)
TEST_F(ApiVersionDetectionTest, InvalidVersionLeadingZeroV0123) {
  HttpRequest request(ci, 1);
  request.setRequestPath("/_arango/v0123/_api/version");

  Result res = request.detectAndStripApiVersion();

  EXPECT_TRUE(res.fail());
  EXPECT_EQ(TRI_ERROR_HTTP_BAD_PARAMETER, res.errorNumber());
  EXPECT_EQ(GeneralRequest::defaultApiVersion, request.apiVersion());
  EXPECT_NE(std::string::npos, res.errorMessage().find("leading zeros"));
}

// Test: Version v00 should be rejected
TEST_F(ApiVersionDetectionTest, InvalidVersionV00) {
  HttpRequest request(ci, 1);
  request.setRequestPath("/_arango/v00/path");

  Result res = request.detectAndStripApiVersion();

  EXPECT_TRUE(res.fail());
  EXPECT_EQ(TRI_ERROR_HTTP_BAD_PARAMETER, res.errorNumber());
  EXPECT_EQ(GeneralRequest::defaultApiVersion, request.apiVersion());
  EXPECT_NE(std::string::npos, res.errorMessage().find("leading zeros"));
}

// Test: Version v000 should be rejected
TEST_F(ApiVersionDetectionTest, InvalidVersionV000) {
  HttpRequest request(ci, 1);
  request.setRequestPath("/_arango/v000");

  Result res = request.detectAndStripApiVersion();

  EXPECT_TRUE(res.fail());
  EXPECT_EQ(TRI_ERROR_HTTP_BAD_PARAMETER, res.errorNumber());
  EXPECT_EQ(GeneralRequest::defaultApiVersion, request.apiVersion());
  EXPECT_NE(std::string::npos, res.errorMessage().find("leading zeros"));
}

// Test: Version v010 should be rejected
TEST_F(ApiVersionDetectionTest, InvalidVersionV010) {
  HttpRequest request(ci, 1);
  request.setRequestPath("/_arango/v010");

  Result res = request.detectAndStripApiVersion();

  EXPECT_TRUE(res.fail());
  EXPECT_EQ(TRI_ERROR_HTTP_BAD_PARAMETER, res.errorNumber());
  EXPECT_NE(std::string::npos, res.errorMessage().find("leading zeros"));
}

// Test: Version v0100 should be rejected
TEST_F(ApiVersionDetectionTest, InvalidVersionV0100) {
  HttpRequest request(ci, 1);
  request.setRequestPath("/_arango/v0100/path");

  Result res = request.detectAndStripApiVersion();

  EXPECT_TRUE(res.fail());
  EXPECT_EQ(TRI_ERROR_HTTP_BAD_PARAMETER, res.errorNumber());
  EXPECT_NE(std::string::npos, res.errorMessage().find("leading zeros"));
}

// Test: Version number that exceeds uint64_t max should trigger stoull
// exception
TEST_F(ApiVersionDetectionTest, VersionExceedsUint64Max) {
  HttpRequest request(ci, 1);
  // Create a version number that's way too large for uint64_t
  // uint64_t max is 18446744073709551615 (20 digits)
  // Let's use a number with many more digits
  std::string path = "/_arango/v99999999999999999999999999999999/path";
  request.setRequestPath(path);

  Result res = request.detectAndStripApiVersion();

  EXPECT_TRUE(res.fail());
  EXPECT_EQ(TRI_ERROR_HTTP_BAD_PARAMETER, res.errorNumber());
  EXPECT_EQ(GeneralRequest::defaultApiVersion, request.apiVersion());
  EXPECT_EQ(path, request.requestPath());
  // Verify the error message mentions the parsing failure
  EXPECT_NE(std::string::npos, res.errorMessage().find("failed to parse"));
}

// Test: Version number at uint64_t max boundary
TEST_F(ApiVersionDetectionTest, VersionAtUint64Max) {
  HttpRequest request(ci, 1);
  // uint64_t max is 18446744073709551615
  std::string path = "/_arango/v18446744073709551615/path";
  request.setRequestPath(path);

  Result res = request.detectAndStripApiVersion();

  // This should succeed in parsing but fail because it exceeds uint32_t max
  EXPECT_TRUE(res.fail());
  EXPECT_EQ(TRI_ERROR_HTTP_BAD_PARAMETER, res.errorNumber());
  EXPECT_EQ(GeneralRequest::defaultApiVersion, request.apiVersion());
  EXPECT_EQ(path, request.requestPath());
  EXPECT_NE(std::string::npos, res.errorMessage().find("too large"));
}

// Test: Version number slightly above uint64_t max
TEST_F(ApiVersionDetectionTest, VersionSlightlyAboveUint64Max) {
  HttpRequest request(ci, 1);
  // uint64_t max is 18446744073709551615, let's use 18446744073709551616
  std::string path = "/_arango/v18446744073709551616/path";
  request.setRequestPath(path);

  Result res = request.detectAndStripApiVersion();

  EXPECT_TRUE(res.fail());
  EXPECT_EQ(TRI_ERROR_HTTP_BAD_PARAMETER, res.errorNumber());
  EXPECT_EQ(GeneralRequest::defaultApiVersion, request.apiVersion());
  EXPECT_EQ(path, request.requestPath());
  // Should trigger stoull exception for out_of_range
  EXPECT_NE(std::string::npos, res.errorMessage().find("failed to parse"));
}

// Test: Very long string of digits that would cause stoull to throw
TEST_F(ApiVersionDetectionTest, VersionExcessivelyLongNumber) {
  HttpRequest request(ci, 1);
  // Create a version with 100 digits
  std::string hugeNumber(100, '9');
  std::string path = "/_arango/v" + hugeNumber + "/path";
  request.setRequestPath(path);

  Result res = request.detectAndStripApiVersion();

  EXPECT_TRUE(res.fail());
  EXPECT_EQ(TRI_ERROR_HTTP_BAD_PARAMETER, res.errorNumber());
  EXPECT_EQ(GeneralRequest::defaultApiVersion, request.apiVersion());
  EXPECT_EQ(path, request.requestPath());
  EXPECT_NE(std::string::npos, res.errorMessage().find("failed to parse"));
}
