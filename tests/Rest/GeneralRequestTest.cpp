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
#include <string>

#include "Basics/ErrorCode.h"
#include "Basics/Exceptions.h"
#include "Endpoint/ConnectionInfo.h"
#include "Rest/ApiVersion.h"
#include "Rest/HttpRequest.h"

using namespace arangodb;

// Helper: call detectAndStripApiVersion on a plain string path.
// On success, returns the remaining path (from the advanced start pointer to
// end) and sets apiVersionOut. On error (exception), returns std::nullopt and
// sets errorNumberOut / errorMessageOut.
struct DetectResult {
  bool ok;
  std::string remaining;  // path from advanced start to end
  uint32_t apiVersion;
  ErrorCode errorNumber;
  std::string errorMessage;
};

static DetectResult callDetect(HttpRequest& request, std::string const& path) {
  char const* start = path.data();
  char const* end = path.data() + path.size();
  try {
    request.detectAndStripApiVersion(start, end);
    return DetectResult{true,
                        std::string(start, end),
                        request.requestedApiVersion(),
                        TRI_ERROR_NO_ERROR,
                        {}};
  } catch (arangodb::basics::Exception const& e) {
    return DetectResult{false, path, request.requestedApiVersion(), e.code(),
                        e.message()};
  }
}

// Test fixture for API version detection tests
class ApiVersionDetectionTest : public ::testing::Test {
 protected:
  ConnectionInfo ci;
};

// Test: No API version prefix - should use default version
TEST_F(ApiVersionDetectionTest, NoApiVersionPrefix) {
  HttpRequest request(ci, 1);
  auto r = callDetect(request, "/_api/version");
  EXPECT_TRUE(r.ok);
  EXPECT_EQ(ApiVersion::defaultApiVersion, r.apiVersion);
  EXPECT_EQ("/_api/version", r.remaining);
}

// Test: Regular path without /_arango prefix
TEST_F(ApiVersionDetectionTest, RegularPath) {
  HttpRequest request(ci, 1);
  auto r = callDetect(request, "/some/random/path");
  EXPECT_TRUE(r.ok);
  EXPECT_EQ(ApiVersion::defaultApiVersion, r.apiVersion);
  EXPECT_EQ("/some/random/path", r.remaining);
}

TEST_F(ApiVersionDetectionTest, UnsupportedVersionNotStripped) {
  HttpRequest request(ci, 1);
  auto r = callDetect(request, "/_arango/v1/_api/version");
  EXPECT_TRUE(r.ok);
  EXPECT_EQ(ApiVersion::defaultApiVersion, r.apiVersion);
  EXPECT_EQ("/_arango/v1/_api/version", r.remaining);
}

// Test: Experimental API version
TEST_F(ApiVersionDetectionTest, ExperimentalApiVersion) {
  HttpRequest request(ci, 1);
  auto r = callDetect(request, "/_arango/experimental/_api/new-feature");
  EXPECT_TRUE(r.ok);
  EXPECT_EQ(ApiVersion::experimentalApiVersion, r.apiVersion);
  EXPECT_EQ("/_api/new-feature", r.remaining);
}

// Test: Path ending exactly at version number
TEST_F(ApiVersionDetectionTest, PathEndingAtVersion) {
  HttpRequest request(ci, 1);
  auto r = callDetect(request, "/_arango/v0");
  EXPECT_TRUE(r.ok);
  EXPECT_EQ(0u, r.apiVersion);
  EXPECT_EQ("", r.remaining);
}

// Test: Path ending exactly at experimental
TEST_F(ApiVersionDetectionTest, PathEndingAtExperimental) {
  HttpRequest request(ci, 1);
  auto r = callDetect(request, "/_arango/experimental");
  EXPECT_TRUE(r.ok);
  EXPECT_EQ(ApiVersion::experimentalApiVersion, r.apiVersion);
  EXPECT_EQ("", r.remaining);
}

// Test: Invalid - path is exactly /_arango/
TEST_F(ApiVersionDetectionTest, InvalidExactlyArango) {
  HttpRequest request(ci, 1);
  auto r = callDetect(request, "/_arango/");
  EXPECT_FALSE(r.ok);
  EXPECT_EQ(TRI_ERROR_HTTP_BAD_PARAMETER, r.errorNumber);
  EXPECT_EQ(ApiVersion::defaultApiVersion, r.apiVersion);  // unchanged on error
}

// Test: Invalid - missing v prefix
TEST_F(ApiVersionDetectionTest, InvalidMissingVPrefix) {
  HttpRequest request(ci, 1);
  auto r = callDetect(request, "/_arango/1/path");
  EXPECT_FALSE(r.ok);
  EXPECT_EQ(TRI_ERROR_HTTP_BAD_PARAMETER, r.errorNumber);
  EXPECT_EQ(ApiVersion::defaultApiVersion, r.apiVersion);
}

// Test: v without number - unrecognized, not stripped
TEST_F(ApiVersionDetectionTest, VWithoutNumberNotStripped) {
  HttpRequest request(ci, 1);
  auto r = callDetect(request, "/_arango/v/path");
  EXPECT_TRUE(r.ok);
  EXPECT_EQ(ApiVersion::defaultApiVersion, r.apiVersion);
  EXPECT_EQ("/_arango/v/path", r.remaining);
}

// Test: v with non-numeric characters - unrecognized, not stripped
TEST_F(ApiVersionDetectionTest, VWithAlphaNotStripped) {
  HttpRequest request(ci, 1);
  auto r = callDetect(request, "/_arango/vabc/path");
  EXPECT_TRUE(r.ok);
  EXPECT_EQ(ApiVersion::defaultApiVersion, r.apiVersion);
  EXPECT_EQ("/_arango/vabc/path", r.remaining);
}

// Test: Invalid - random text after /_arango/
TEST_F(ApiVersionDetectionTest, InvalidRandomText) {
  HttpRequest request(ci, 1);
  auto r = callDetect(request, "/_arango/invalid/path");
  EXPECT_FALSE(r.ok);
  EXPECT_EQ(TRI_ERROR_HTTP_BAD_PARAMETER, r.errorNumber);
  EXPECT_EQ(ApiVersion::defaultApiVersion, r.apiVersion);
}

// Test: Invalid - experimental typo
TEST_F(ApiVersionDetectionTest, InvalidExperimentalTypo) {
  HttpRequest request(ci, 1);
  auto r = callDetect(request, "/_arango/experimenta/path");
  EXPECT_FALSE(r.ok);
  EXPECT_EQ(TRI_ERROR_HTTP_BAD_PARAMETER, r.errorNumber);
  EXPECT_EQ(ApiVersion::defaultApiVersion, r.apiVersion);
}

// Test: Edge case - v0 is valid
TEST_F(ApiVersionDetectionTest, ValidApiVersionV0) {
  HttpRequest request(ci, 1);
  auto r = callDetect(request, "/_arango/v0/path");
  EXPECT_TRUE(r.ok);
  EXPECT_EQ(0u, r.apiVersion);
  EXPECT_EQ("/path", r.remaining);
}

// Test: Version number followed by non-slash character - unrecognized, not
// stripped
TEST_F(ApiVersionDetectionTest, VersionFollowedByAlphaNotStripped) {
  HttpRequest request(ci, 1);
  auto r = callDetect(request, "/_arango/v1abc/path");
  EXPECT_TRUE(r.ok);
  EXPECT_EQ(ApiVersion::defaultApiVersion, r.apiVersion);
  EXPECT_EQ("/_arango/v1abc/path", r.remaining);
}

// Test: Experimental followed by non-slash character should be invalid
TEST_F(ApiVersionDetectionTest, InvalidExperimentalNotFollowedBySlash) {
  HttpRequest request(ci, 1);
  auto r = callDetect(request, "/_arango/experimentalabc/path");
  EXPECT_FALSE(r.ok);
  EXPECT_EQ(TRI_ERROR_HTTP_BAD_PARAMETER, r.errorNumber);
}

// Test: Unsupported version with multiple slashes - not stripped
TEST_F(ApiVersionDetectionTest, UnsupportedVersionWithMultipleSlashes) {
  HttpRequest request(ci, 1);
  auto r = callDetect(request, "/_arango/v1///path");
  EXPECT_TRUE(r.ok);
  EXPECT_EQ(ApiVersion::defaultApiVersion, r.apiVersion);
  EXPECT_EQ("/_arango/v1///path", r.remaining);
}

// Test: Root path
TEST_F(ApiVersionDetectionTest, RootPath) {
  HttpRequest request(ci, 1);
  auto r = callDetect(request, "/");
  EXPECT_TRUE(r.ok);
  EXPECT_EQ(ApiVersion::defaultApiVersion, r.apiVersion);
  EXPECT_EQ("/", r.remaining);
}

// Test: Unsupported version with deep nested path - not stripped
TEST_F(ApiVersionDetectionTest, UnsupportedVersionWithNestedPath) {
  HttpRequest request(ci, 1);
  auto r = callDetect(request, "/_arango/v3/_api/collection/test/document/123");
  EXPECT_TRUE(r.ok);
  EXPECT_EQ(ApiVersion::defaultApiVersion, r.apiVersion);
  EXPECT_EQ("/_arango/v3/_api/collection/test/document/123", r.remaining);
}

// Test: Another unsupported version - not stripped
TEST_F(ApiVersionDetectionTest, UnsupportedVersionV7NotStripped) {
  HttpRequest request(ci, 1);
  auto r = callDetect(request, "/_arango/v7/_api/cursor");
  EXPECT_TRUE(r.ok);
  EXPECT_EQ(ApiVersion::defaultApiVersion, r.apiVersion);
  EXPECT_EQ("/_arango/v7/_api/cursor", r.remaining);
}

// Test: Large unsupported version (uint32_t max - 1) - not stripped
TEST_F(ApiVersionDetectionTest, UnsupportedLargeVersionNotStripped) {
  HttpRequest request(ci, 1);
  std::string path = "/_arango/v" +
                     std::to_string(std::numeric_limits<uint32_t>::max() - 1) +
                     "/path";
  auto r = callDetect(request, path);
  EXPECT_TRUE(r.ok);
  EXPECT_EQ(ApiVersion::defaultApiVersion, r.apiVersion);
  EXPECT_EQ(path, r.remaining);
}

// Test: Check that default API version constant is accessible and is 0
TEST_F(ApiVersionDetectionTest, DefaultApiVersionConstant) {
  EXPECT_EQ(1u, ApiVersion::defaultApiVersion);
}

// Test: Verify default version is applied to new requests
TEST_F(ApiVersionDetectionTest, NewRequestHasDefaultVersion) {
  HttpRequest request(ci, 1);
  EXPECT_EQ(ApiVersion::defaultApiVersion, request.requestedApiVersion());
}

// Test: Version exceeding uint32_t max - unsupported, not stripped
TEST_F(ApiVersionDetectionTest, UnsupportedVersionExceedsUint32Max) {
  HttpRequest request(ci, 1);
  uint64_t tooLarge =
      static_cast<uint64_t>(std::numeric_limits<uint32_t>::max()) + 1;
  std::string path = "/_arango/v" + std::to_string(tooLarge) + "/path";
  auto r = callDetect(request, path);
  EXPECT_TRUE(r.ok);
  EXPECT_EQ(ApiVersion::defaultApiVersion, r.apiVersion);
  EXPECT_EQ(path, r.remaining);
}

// Test: Version with leading zero should be rejected (v01)
TEST_F(ApiVersionDetectionTest, InvalidVersionLeadingZeroV01) {
  HttpRequest request(ci, 1);
  auto r = callDetect(request, "/_arango/v01/path");
  EXPECT_FALSE(r.ok);
  EXPECT_EQ(TRI_ERROR_HTTP_BAD_PARAMETER, r.errorNumber);
  EXPECT_EQ(ApiVersion::defaultApiVersion, r.apiVersion);
  EXPECT_NE(std::string::npos, r.errorMessage.find("leading zeros"));
}

// Test: Version with leading zero should be rejected (v007)
TEST_F(ApiVersionDetectionTest, InvalidVersionLeadingZeroV007) {
  HttpRequest request(ci, 1);
  auto r = callDetect(request, "/_arango/v007/path");
  EXPECT_FALSE(r.ok);
  EXPECT_EQ(TRI_ERROR_HTTP_BAD_PARAMETER, r.errorNumber);
  EXPECT_EQ(ApiVersion::defaultApiVersion, r.apiVersion);
  EXPECT_NE(std::string::npos, r.errorMessage.find("leading zeros"));
}

// Test: Version with leading zero should be rejected (v0123)
TEST_F(ApiVersionDetectionTest, InvalidVersionLeadingZeroV0123) {
  HttpRequest request(ci, 1);
  auto r = callDetect(request, "/_arango/v0123/_api/version");
  EXPECT_FALSE(r.ok);
  EXPECT_EQ(TRI_ERROR_HTTP_BAD_PARAMETER, r.errorNumber);
  EXPECT_EQ(ApiVersion::defaultApiVersion, r.apiVersion);
  EXPECT_NE(std::string::npos, r.errorMessage.find("leading zeros"));
}

// Test: Version v00 should be rejected
TEST_F(ApiVersionDetectionTest, InvalidVersionV00) {
  HttpRequest request(ci, 1);
  auto r = callDetect(request, "/_arango/v00/path");
  EXPECT_FALSE(r.ok);
  EXPECT_EQ(TRI_ERROR_HTTP_BAD_PARAMETER, r.errorNumber);
  EXPECT_EQ(ApiVersion::defaultApiVersion, r.apiVersion);
  EXPECT_NE(std::string::npos, r.errorMessage.find("leading zeros"));
}

// Test: Version v000 should be rejected
TEST_F(ApiVersionDetectionTest, InvalidVersionV000) {
  HttpRequest request(ci, 1);
  auto r = callDetect(request, "/_arango/v000");
  EXPECT_FALSE(r.ok);
  EXPECT_EQ(TRI_ERROR_HTTP_BAD_PARAMETER, r.errorNumber);
  EXPECT_EQ(ApiVersion::defaultApiVersion, r.apiVersion);
  EXPECT_NE(std::string::npos, r.errorMessage.find("leading zeros"));
}

// Test: Version v010 should be rejected
TEST_F(ApiVersionDetectionTest, InvalidVersionV010) {
  HttpRequest request(ci, 1);
  auto r = callDetect(request, "/_arango/v010");
  EXPECT_FALSE(r.ok);
  EXPECT_EQ(TRI_ERROR_HTTP_BAD_PARAMETER, r.errorNumber);
  EXPECT_NE(std::string::npos, r.errorMessage.find("leading zeros"));
}

// Test: Version v0100 should be rejected
TEST_F(ApiVersionDetectionTest, InvalidVersionV0100) {
  HttpRequest request(ci, 1);
  auto r = callDetect(request, "/_arango/v0100/path");
  EXPECT_FALSE(r.ok);
  EXPECT_EQ(TRI_ERROR_HTTP_BAD_PARAMETER, r.errorNumber);
  EXPECT_NE(std::string::npos, r.errorMessage.find("leading zeros"));
}

// Test: Version number that exceeds uint64_t max should trigger stoull
// exception
TEST_F(ApiVersionDetectionTest, VersionExceedsUint64Max) {
  HttpRequest request(ci, 1);
  std::string path = "/_arango/v99999999999999999999999999999999/path";
  auto r = callDetect(request, path);
  EXPECT_FALSE(r.ok);
  EXPECT_EQ(TRI_ERROR_HTTP_BAD_PARAMETER, r.errorNumber);
  EXPECT_EQ(ApiVersion::defaultApiVersion, r.apiVersion);
  EXPECT_NE(std::string::npos, r.errorMessage.find("failed to parse"));
}

// Test: uint64_t max - parses fine but unsupported, not stripped
TEST_F(ApiVersionDetectionTest, UnsupportedVersionAtUint64Max) {
  HttpRequest request(ci, 1);
  // uint64_t max is 18446744073709551615 - exceeds uint32_t max
  std::string path = "/_arango/v18446744073709551615/path";
  auto r = callDetect(request, path);
  EXPECT_TRUE(r.ok);
  EXPECT_EQ(ApiVersion::defaultApiVersion, r.apiVersion);
  EXPECT_EQ(path, r.remaining);
}

// Test: Version number slightly above uint64_t max
TEST_F(ApiVersionDetectionTest, VersionSlightlyAboveUint64Max) {
  HttpRequest request(ci, 1);
  std::string path = "/_arango/v18446744073709551616/path";
  auto r = callDetect(request, path);
  EXPECT_FALSE(r.ok);
  EXPECT_EQ(TRI_ERROR_HTTP_BAD_PARAMETER, r.errorNumber);
  EXPECT_EQ(ApiVersion::defaultApiVersion, r.apiVersion);
  EXPECT_NE(std::string::npos, r.errorMessage.find("failed to parse"));
}

// Test: Very long string of digits that would cause stoull to throw
TEST_F(ApiVersionDetectionTest, VersionExcessivelyLongNumber) {
  HttpRequest request(ci, 1);
  std::string hugeNumber(100, '9');
  std::string path = "/_arango/v" + hugeNumber + "/path";
  auto r = callDetect(request, path);
  EXPECT_FALSE(r.ok);
  EXPECT_EQ(TRI_ERROR_HTTP_BAD_PARAMETER, r.errorNumber);
  EXPECT_EQ(ApiVersion::defaultApiVersion, r.apiVersion);
  EXPECT_NE(std::string::npos, r.errorMessage.find("failed to parse"));
}
