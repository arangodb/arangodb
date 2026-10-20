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

#include <algorithm>
#include <array>
#include <cstdint>
#include <iosfwd>
#include <string>

namespace arangodb {

// fuerte defines its own ApiVersion so that it does not have to include server
// headers. The two are kept in lockstep by `toFuerte` in ApiVersionFuerte.h,
// whose switch makes an unmapped enumerator a compile error.
enum class ApiVersion : std::uint8_t { V0 = 0, V1 = 1, Experimental = 2 };

/// @brief the version number, promoted so that it formats and streams as a
/// number. Casting to the underlying type instead would stream as a character.
constexpr unsigned toNumber(ApiVersion version) noexcept {
  return static_cast<unsigned>(version);
}

std::string to_string(ApiVersion version);

std::ostream& operator<<(std::ostream& os, ApiVersion version);

namespace api_version {
/// @brief Central configuration for API versioning

// The list of all supported API versions (both stable and deprecated)
inline constexpr std::array supportedApiVersions{ApiVersion::V0};

// The list of deprecated API versions (subset of supportedApiVersions)
inline constexpr std::array<ApiVersion, 0> deprecatedApiVersions{};

// The version API reports these newest-first by iterating in reverse, so the
// tables above must stay in ascending order.
static_assert(std::ranges::is_sorted(supportedApiVersions));
static_assert(std::ranges::is_sorted(deprecatedApiVersions));

// The API version used when no /_arango/vX prefix is specified.
inline constexpr ApiVersion defaultApiVersion = ApiVersion::V0;

// The experimental API version (accessed via /_arango/experimental).
inline constexpr ApiVersion experimentalApiVersion = ApiVersion::Experimental;

// Whether the given version appears in the list of supported versions. This is
// a pure lookup - see isApiVersionRequestable for the rule that governs
// incoming requests.
constexpr bool isApiVersionSupported(ApiVersion version) {
  return std::ranges::find(supportedApiVersions, version) !=
         supportedApiVersions.end();
}

// The highest version any handler may be registered for, and therefore the
// upper bound on a parsed /_arango/vX prefix. Experimental is included because
// it is routable even though it is not "supported".
constexpr ApiVersion maxApiVersion() {
  ApiVersion maxVersion = experimentalApiVersion;
  for (ApiVersion version : supportedApiVersions) {
    if (version > maxVersion) {
      maxVersion = version;
    }
  }
  return maxVersion;
}

// Whether an incoming request may ask for this version via /_arango/vX.
// Deliberately excludes the experimental version: that is reached via
// /_arango/experimental, never via its version number.
bool isApiVersionRequestable(ApiVersion version);

// Whether a handler may be registered for this version.
bool isApiVersionRegistrable(ApiVersion version);

}  // namespace api_version
}  // namespace arangodb
