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
#include <string_view>

namespace arangodb {

// fuerte defines its own ApiVersion so that it does not have to include server
// headers. The two are kept in lockstep by `toFuerte` in ApiVersionFuerte.h,
// whose switch makes an unmapped enumerator a compile error.
enum class ApiVersion : std::uint8_t { V1 = 1, Experimental = 2 };

constexpr std::string_view to_string(ApiVersion version) noexcept {
  switch (version) {
    case ApiVersion::V1:
      return "v1";
    case ApiVersion::Experimental:
      return "experimental";
  }
  std::abort();  // necessary for cppcheck
}

std::ostream& operator<<(std::ostream& os, ApiVersion version);

// Serialized by name, like to_string.
template<class Inspector>
auto inspect(Inspector& f, ApiVersion& version) {
  return f.enumeration(version).values(
      ApiVersion::V1, to_string(ApiVersion::V1),  //
      ApiVersion::Experimental, to_string(ApiVersion::Experimental));
}

namespace api_version {
/// @brief Central configuration for API versioning

// Every enumerator, whether or not it is currently supported.
inline constexpr std::array allApiVersions{ApiVersion::V1,
                                           ApiVersion::Experimental};

static_assert(
    [] {
      auto const isGap = [](ApiVersion a, ApiVersion b) {
        return static_cast<unsigned>(b) != static_cast<unsigned>(a) + 1;
      };
      return allApiVersions.front() == ApiVersion::V1 &&
             allApiVersions.back() == ApiVersion::Experimental &&
             std::ranges::adjacent_find(allApiVersions, isGap) ==
                 allApiVersions.end();
    }(),
    "allApiVersions must list every enumerator, in order and without gaps");

// The list of all supported API versions (both stable and deprecated)
inline constexpr std::array supportedApiVersions{ApiVersion::V1};

// The list of deprecated API versions (subset of supportedApiVersions)
inline constexpr std::array<ApiVersion, 0> deprecatedApiVersions{};

// The version API reports these newest-first by iterating in reverse, so the
// tables above must stay in ascending order.
static_assert(std::ranges::is_sorted(supportedApiVersions));
static_assert(std::ranges::is_sorted(deprecatedApiVersions));

// The API version used when no /_arango/vX prefix is specified.
inline constexpr ApiVersion defaultApiVersion = ApiVersion::V1;

// The experimental API version (accessed via /_arango/experimental).
inline constexpr ApiVersion experimentalApiVersion = ApiVersion::Experimental;

// Whether the given version appears in the list of supported versions. This is
// a pure lookup - see isApiVersionRequestable for the rule that governs
// incoming requests.
constexpr bool isApiVersionSupported(ApiVersion version) {
  return std::ranges::find(supportedApiVersions, version) !=
         supportedApiVersions.end();
}

// Whether an incoming request may ask for this version via /_arango/vX.
// Deliberately excludes the experimental version: that is reached via
// /_arango/experimental, never via its version number.
bool isApiVersionRequestable(ApiVersion version);

}  // namespace api_version
}  // namespace arangodb
